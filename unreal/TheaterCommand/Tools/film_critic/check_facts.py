#!/usr/bin/env python3
"""
Film critic: checks over a scene's facts file (critic-facts.jsonl, written by the game in a -FilmCritic run).

Each check is named the way rules.yaml names it in a rule's `facts:` field, and returns
  {status: pass | fail | not-measured, measured: {...}, worst: [...], words: "one plain sentence"}
A check whose kind of line is missing from the file is not-measured, never a pass.

Checks:
  camera-smooth          `cam` lines: per-frame turn and travel (declared cuts left out), jumps beyond the
                         frames around, zoom pops, stutter, hitches; worst moments with real time and nearest still
  fleet-bow-on-course    `fleet` lines: the bow points along the course; wakes that all point one way on screen
  opening-shows-both-sides `planes` lines of the opening shot: share with both sides in frame
  marks-on-every-model   `marks` lines: any model with no markings
  men-gait-variety       `men` lines: running speeds and gait clips vary while 20+ men move
  vehicles-no-rocking    `vehicle` lines: a vehicle that swings back and forth while standing
  battle-end-not-abrupt  `end` line: the card did not come within ~10 s of the break with no offer to go on
  look-matches-place     `look` line: which look was used (and, given the scene's theater and date, whether it fits)

Usage:
  check_facts.py critic-facts.jsonl [--checks camera-smooth,marks-on-every-model] [--context '{"year":1940}']
                 [--out result.json]
Only the standard library is needed. Run with python3 -I on downloaded files.
"""
import argparse, json, math, os, sys
from statistics import median

# ---- limits (see CHECKER.md for why) -------------------------------------------------------------------------
JUMP_PX = 20.0          # a frame's screen move beyond the straight-on guess from the frame before, px on 1920
ZOOM_POP = 0.10         # the lens (fov) or the distance to the subject changes over 10% in one frame
STUTTER_X = 0.5         # same limits as measure_clip's stutter: off the expected move by half the usual move...
STUTTER_FLOOR = 4.0     # ... and by over 4 px on 1920
MOVE_FLOOR = 1.0        # the view counts as moving when it moves 1 px on 1920 a frame on average
WINDOW = 5              # frames each side for the expected move
HITCH_MS = (50.0, 100.0)
EYE_LURCH_M = 0.5       # with no subject to measure against: an eye step this many metres beyond the guess ...
EYE_LURCH_X = 3.0       # ... and over 3 times the usual step of the frames around
BOW_DOT = math.cos(math.radians(25))  # art director 26 Nov: within 25 degrees of the back of her track passes
BOW_BAD_SHARE = 0.25    # a fleet fails when over a quarter of its sailing samples point off course (turns are brief)
OPENING_SHARE = 0.5
GAIT_SPREAD = 1.25
GAIT_RATES = 3
GAIT_MEN = 20
ROCK_RATE = 2.0         # deg/s
ROCK_SPEED = 0.5        # m/s
ROCK_TIMES = 3          # more than this many swings ...
ROCK_WINDOW = 10.0      # ... in this many seconds
END_GAP = 10.0          # real seconds from the break to the card


def load_facts(path):
    lines, bad = [], 0
    with open(path, encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            raw = raw.strip()
            if not raw:
                continue
            try:
                d = json.loads(raw)
            except ValueError:
                bad += 1
                continue
            if isinstance(d, dict) and "k" in d:
                lines.append(d)
            else:
                bad += 1
    lines.sort(key=lambda d: num(d.get("t"), 0.0))
    return lines, bad


def num(v, default=None):
    try:
        f = float(v)
        return f if math.isfinite(f) else default
    except (TypeError, ValueError):
        return default


def kind(lines, k):
    return [d for d in lines if d.get("k") == k]


def result(status, measured=None, worst=None, words=""):
    return {"status": status, "measured": measured or {}, "worst": worst or [], "words": words}


def not_measured(k):
    return result("not-measured", {"lines": 0}, [], f"The game wrote no '{k}' facts in this run, so this was not checked.")


class Nearest:
    """Finds the still and the strip frame nearest a moment, from the `still` and `strip` lines."""

    def __init__(self, lines):
        self.stills = [(num(d.get("t"), 0.0), d.get("file") or d.get("name")) for d in kind(lines, "still")]
        self.strips = [(num(d.get("t"), 0.0), d.get("file"), d.get("of")) for d in kind(lines, "strip")]

    def near(self, t):
        out = {}
        if self.stills:
            ts, f = min(self.stills, key=lambda s: abs(s[0] - t))
            out.update(still=f, still_off_s=round(ts - t, 2))
        if self.strips:
            ts, f, of = min(self.strips, key=lambda s: abs(s[0] - t))
            if abs(ts - t) < 2.0:
                out.update(strip=f, strip_of=of, strip_off_s=round(ts - t, 2))
        return out


def moment(d, near, **kw):
    m = {"t": round(num(d.get("t"), 0.0), 3)}
    if d.get("bt") is not None:
        m["bt"] = round(num(d.get("bt"), 0.0), 2)
    if d.get("why"):
        m["shot"] = d.get("why")
    m.update(kw)
    m.update(near.near(m["t"]))
    return m


def wrap180(a):
    return (a + 180.0) % 360.0 - 180.0


def vec3(v):
    if isinstance(v, (list, tuple)) and len(v) >= 3:
        xs = [num(x) for x in v[:3]]
        if None not in xs:
            return xs
    return None


def subject_pos(d):
    s = d.get("subject")
    if isinstance(s, dict):
        for key in ("pos", "at", "where", "xyz"):
            if vec3(s.get(key)):
                return vec3(s.get(key))
    return vec3(s)


def subject_name(d):
    s = d.get("subject")
    if isinstance(s, str):
        return s
    if isinstance(s, dict) and s.get("name"):
        return str(s["name"])
    return "its subject"


def forward(pitch, yaw):
    p, y = math.radians(pitch), math.radians(yaw)
    return (math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p))


def screen_pos(f):
    """Where the subject sits on screen, px on 1920 from the centre (x right, y down), by the Unreal convention:
    X forward, Y right, Z up; yaw turns about Z, pitch raises the nose; roll is left out."""
    p, y = math.radians(f["pitch"]), math.radians(f["yaw"])
    fw = (math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p))
    rt = (-math.sin(y), math.cos(y), 0.0)
    up = (-math.sin(p) * math.cos(y), -math.sin(p) * math.sin(y), math.cos(p))
    v = [f["subj"][i] - f["eye"][i] for i in range(3)]
    along = sum(v[i] * fw[i] for i in range(3))
    if along <= 1e-3:
        return None  # behind the camera
    kpx = 1920.0 / max(1.0, f["fov"])
    return (math.degrees(math.atan2(sum(v[i] * rt[i] for i in range(3)), along)) * kpx,
            -math.degrees(math.atan2(sum(v[i] * up[i] for i in range(3)), along)) * kpx)


def quad_expect(ts, vs, t0):
    """The value a smooth curve (quadratic, refit once without the worst points) through (ts, vs) gives at t0.
    vs: list of tuples. Falls back to the median with few points."""
    m = len(ts)
    dims = len(vs[0])
    if m < 6:
        return tuple(median(v[i] for v in vs) for i in range(dims))
    keep = list(range(m))
    coef = None
    for _ in range(2):
        coef = [lsq_quad([ts[k] - t0 for k in keep], [vs[k][i] for k in keep]) for i in range(dims)]
        errs = [math.sqrt(sum((vs[k][i] - poly(coef[i], ts[k] - t0)) ** 2 for i in range(dims))) for k in range(m)]
        mags = [math.sqrt(sum(x * x for x in v)) for v in vs]
        lim = max(2.5 * median([errs[k] for k in keep]), 0.25 * median(mags) + 1e-6)
        nk = [k for k in range(m) if errs[k] <= lim]
        if len(nk) < 5 or nk == keep:
            break
        keep = nk
    return tuple(coef[i][0] for i in range(dims))


def poly(c, x):
    return c[0] + c[1] * x + c[2] * x * x


def lsq_quad(xs, ys):
    """Least-squares c0 + c1 x + c2 x^2 by the normal equations (3x3), no numpy needed."""
    s = [sum(x ** p for x in xs) for p in range(5)]
    t = [sum(y * x ** p for x, y in zip(xs, ys)) for p in range(3)]
    A = [[s[0], s[1], s[2]], [s[1], s[2], s[3]], [s[2], s[3], s[4]]]
    try:
        return solve3(A, t)
    except ZeroDivisionError:
        my = sum(ys) / len(ys)
        return [my, 0.0, 0.0]


def solve3(A, b):
    M = [row[:] + [bb] for row, bb in zip(A, b)]
    for c in range(3):
        p = max(range(c, 3), key=lambda r: abs(M[r][c]))
        if abs(M[p][c]) < 1e-12:
            raise ZeroDivisionError
        M[c], M[p] = M[p], M[c]
        for r in range(3):
            if r != c:
                f = M[r][c] / M[c][c]
                for k in range(c, 4):
                    M[r][k] -= f * M[c][k]
    return [M[i][3] / M[i][i] for i in range(3)]


# ---- camera-smooth ---------------------------------------------------------------------------------------------
def check_camera_smooth(lines, ctx, near):
    cams = kind(lines, "cam")
    if not cams:
        return not_measured("cam")
    lim = ctx.get("limits", {})
    jump_px = lim.get("jump_px", JUMP_PX)
    zoom_pop = lim.get("zoom_pop", ZOOM_POP)
    sx, sfloor = lim.get("stutter_x", STUTTER_X), lim.get("stutter_floor", STUTTER_FLOOR)
    # one record a frame: screen step (px on 1920) of the turn, eye step (m), lens
    frames = []
    for d in cams:
        rot = d.get("rot") or []
        eye = vec3(d.get("eye"))
        if len(rot) < 2 or eye is None:
            continue
        frames.append({"d": d, "t": num(d.get("t"), 0.0), "dt": num(d.get("dt")), "pitch": num(rot[0], 0.0),
                       "yaw": num(rot[1], 0.0), "roll": num(rot[2], 0.0) if len(rot) > 2 else 0.0,
                       "fov": num(d.get("fov"), 90.0) or 90.0, "eye": eye, "subj": subject_pos(d),
                       "cut": bool(d.get("cut")), "scene": d.get("scene")})
    if len(frames) < 3:
        return result("not-measured", {"lines": len(cams)}, [], "Too few camera facts to judge the camera.")
    dts = [f["dt"] for f in frames if f["dt"] is not None and f["dt"] > 0]
    # steps: frame k against frame k-1, unless k is a declared cut, the scene changed, or the gap is not one frame
    steps = [None] * len(frames)
    cuts = 0
    for k in range(1, len(frames)):
        a, b = frames[k - 1], frames[k]
        if b["cut"]:
            cuts += 1
            continue
        if a["scene"] != b["scene"]:
            continue
        gap = b["t"] - a["t"]
        dt_s = (b["dt"] / 1000.0) if b["dt"] else gap
        if gap <= 0 or gap > max(3.0 * dt_s, 0.25):
            continue  # a rest (the map logs once a second when still) or a missing stretch: not one frame
        kpx = 1920.0 / max(1.0, b["fov"])
        dyaw = wrap180(b["yaw"] - a["yaw"]) * math.cos(math.radians(b["pitch"]))
        dpitch = b["pitch"] - a["pitch"]
        droll = math.radians(wrap180(b["roll"] - a["roll"])) * 960.0
        s = [dyaw * kpx, -dpitch * kpx, droll]
        eye_step = [b["eye"][i] - a["eye"][i] for i in range(3)]
        dist_change = q = None
        if b["subj"] and a["subj"]:
            # the subject's own move on screen (the team's jump: the subject's screen move beyond the straight-on guess)
            pa, pb = screen_pos(a), screen_pos(b)
            if pa and pb:
                q = [pb[0] - pa[0], pb[1] - pa[1]]
            dist, dist0 = math.dist(b["eye"], b["subj"]), math.dist(a["eye"], a["subj"])
            if dist > 1e-3 and dist0 > 1e-3:
                dist_change = (dist - dist0) / dist0
        steps[k] = {"s": s, "q": q, "dt": dt_s, "eye": eye_step, "eye_m": math.sqrt(sum(x * x for x in eye_step)),
                    "fov_change": (b["fov"] - a["fov"]) / max(1e-6, a["fov"]), "dist_change": dist_change}
    n = len(frames)
    jumps, pops, stutters, lurches, worst = [], [], [], [], []

    def run_around(k, w):
        out = []
        for step in (-1, 1):
            j = k + step
            while 0 < j < n and abs(j - k) <= w and steps[j] is not None:
                out.append(j)
                j += step
        return sorted(out)

    moving_frames = 0
    last_jump = None
    for k in range(1, n):
        st = steps[k]
        if st is None:
            continue
        d = frames[k]["d"]
        mag = math.sqrt(sum(x * x for x in st["s"]))
        # zoom pop
        zc = st["fov_change"]
        if abs(zc) > zoom_pop:
            pops.append(moment(d, near, kind="zoom pop", size=round(zc * 100, 1),
                               words=f"the lens {'widens' if zc > 0 else 'narrows'} by {abs(zc) * 100:.0f}% in one frame"))
        elif st["dist_change"] is not None and abs(st["dist_change"]) > zoom_pop:
            dc = st["dist_change"]
            pops.append(moment(d, near, kind="zoom pop", size=round(dc * 100, 1),
                               words=f"the camera {'pulls back' if dc > 0 else 'closes in'} by {abs(dc) * 100:.0f}% of its distance in one frame"))
        # jump: the screen move beyond the straight-on guess from the frame before
        prev = steps[k - 1] if k - 1 >= 1 else None
        if prev is not None and last_jump == k - 1:
            # the frame after a jump is guessed from the frame before the jump, so one lurch counts once
            prev = steps[k - 2] if k - 2 >= 1 else None
        if prev is not None:
            f = st["dt"] / prev["dt"] if prev["dt"] else 1.0
            guess = [x * f for x in prev["s"]]
            beyond = math.sqrt(sum((st["s"][i] - guess[i]) ** 2 for i in range(3)))
            sb = 0.0
            if st["q"] is not None and prev["q"] is not None:
                sb = math.hypot(st["q"][0] - prev["q"][0] * f, st["q"][1] - prev["q"][1] * f)
            if beyond > jump_px or sb > jump_px:
                what = "the view" if beyond >= sb else f"what the shot follows ({subject_name(frames[k]['d'])})"
                size = max(beyond, sb)
                jumps.append(moment(d, near, kind="jump", size_px=round(size, 1),
                                    words=f"{what} jumps {size:.0f} px (on a 1920 screen) beyond where it was heading"))
                last_jump = k
                continue
            # the eye lurches (no subject distance to turn it into pixels): far beyond the steps around it
            ge = [x * f for x in prev["eye"]]
            eb = math.sqrt(sum((st["eye"][i] - ge[i]) ** 2 for i in range(3)))
            around = [steps[j]["eye_m"] for j in run_around(k, WINDOW)]
            if around and eb > max(EYE_LURCH_M, EYE_LURCH_X * median(around)):
                lurches.append(moment(d, near, kind="eye lurch", size_m=round(eb, 2),
                                      words=f"the camera's eye lurches {eb:.1f} m off its path in one frame"))
        # stutter: velocity against the smooth curve of the frames around it
        idx = run_around(k, WINDOW)
        if len(idx) < 4:
            continue
        vel = lambda j: [x / max(1e-4, steps[j]["dt"]) for x in steps[j]["s"]]
        vs = [tuple(vel(j)) for j in idx]
        ts = [frames[j]["t"] for j in idx]
        exp = quad_expect(ts, vs, frames[k]["t"])
        vk = vel(k)
        dt_usual = median([steps[j]["dt"] for j in idx])
        usual_px = max(math.sqrt(sum(x * x for x in exp)),
                       sum(math.sqrt(sum(x * x for x in v)) for v in vs + [tuple(vk)]) / (len(vs) + 1)) * dt_usual
        if usual_px < MOVE_FLOOR:
            continue
        moving_frames += 1
        dev_px = math.sqrt(sum((vk[i] - exp[i]) ** 2 for i in range(3))) * st["dt"]
        if dev_px > sx * usual_px and dev_px > sfloor:
            stutters.append(moment(d, near, kind="stutter", size_px=round(dev_px, 1), usual_px=round(usual_px, 1),
                                   words=f"the view moves {dev_px:.0f} px off its even pace in one frame (it usually moves {usual_px:.0f} px a frame)"))
    hitch50 = [f for f in frames if f["dt"] and f["dt"] > HITCH_MS[0]]
    hitch100 = [f for f in frames if f["dt"] and f["dt"] > HITCH_MS[1]]
    secs = frames[-1]["t"] - frames[0]["t"]
    per_min = (lambda c: round(c * 60.0 / secs, 1)) if secs > 0 else (lambda c: None)
    sdt = sorted(dts)
    measured = {
        "frames": len(frames), "seconds": round(secs, 1), "declared_cuts": cuts, "moving_frames": moving_frames,
        "jumps": len(jumps), "zoom_pops": len(pops), "stutters": len(stutters), "eye_lurches": len(lurches),
        "jumps_per_minute": per_min(len(jumps)), "stutters_per_minute": per_min(len(stutters)),
        "hitches_over_50ms": len(hitch50), "hitches_over_100ms": len(hitch100),
        "frame_ms_median": round(median(sdt), 1) if sdt else None,
        "frame_ms_p99": round(sdt[min(len(sdt) - 1, int(0.99 * len(sdt)))], 1) if sdt else None,
        "frame_ms_worst": round(sdt[-1], 1) if sdt else None,
        "limits": {"jump_px_on_1920": jump_px, "zoom_pop": zoom_pop, "stutter_x": sx, "stutter_floor_px": sfloor},
    }
    for f in sorted(hitch100, key=lambda f: -f["dt"])[:3]:
        worst.append(moment(f["d"], near, kind="hitch", size_ms=round(f["dt"], 1),
                            words=f"one frame takes {f['dt']:.0f} ms (the picture stands still that long)"))
    ranked = (sorted(jumps, key=lambda m: -m["size_px"]) + sorted(pops, key=lambda m: -abs(m["size"])) +
              sorted(stutters, key=lambda m: -m["size_px"]) + sorted(lurches, key=lambda m: -m["size_m"]))
    worst = ranked[:8] + worst
    bad = len(jumps) + len(pops) + len(stutters) + len(lurches)
    lim_max = ctx.get("max", {})
    over = (len(jumps) > lim_max.get("jump", 0) or len(pops) > lim_max.get("zoom", 0) or
            len(stutters) > lim_max.get("stutter", 0) or len(lurches) > lim_max.get("lurch", 0))
    hitch_words = (f"; {len(hitch100)} frame{'s' if len(hitch100) != 1 else ''} took over 100 ms" if hitch100 else "")
    if over:
        parts = []
        if jumps:
            j0 = max(jumps, key=lambda m: m["size_px"])
            parts.append(f"jumped {len(jumps)} time{'s' if len(jumps) != 1 else ''} (worst {j0['size_px']:.0f} px at {j0['t']:.1f} s)")
        if pops:
            parts.append(f"zoomed in a single frame {len(pops)} time{'s' if len(pops) != 1 else ''}")
        if stutters:
            parts.append(f"moved unevenly {len(stutters)} time{'s' if len(stutters) != 1 else ''} while panning")
        if lurches:
            parts.append(f"lurched off its path {len(lurches)} time{'s' if len(lurches) != 1 else ''}")
        words = "The camera " + join_words(parts) + f" in {secs:.0f} s{hitch_words}."
        return result("fail", measured, worst, words)
    words = f"The camera moved smoothly over {len(frames)} frames ({secs:.0f} s): no jumps, zoom pops or uneven moves{hitch_words}."
    return result("pass", measured, worst, words)


def fmt_n(x):
    if x is None:
        return "?"
    return str(int(x)) if float(x).is_integer() else f"{x:.1f}"


def join_words(parts):
    if len(parts) <= 1:
        return "".join(parts)
    return ", ".join(parts[:-1]) + " and " + parts[-1]


# ---- fleet-bow-on-course ---------------------------------------------------------------------------------------
def unit2(v):
    if not isinstance(v, (list, tuple)) or len(v) < 2:
        return None
    x, y = num(v[0]), num(v[1])
    if x is None or y is None:
        return None
    m = math.hypot(x, y)
    return (x / m, y / m) if m > 1e-9 else None


def circ_spread(angles):
    """Mean resultant length of angles (radians): 1 = all the same way, 0 = all ways."""
    if not angles:
        return None
    c = sum(math.cos(a) for a in angles) / len(angles)
    s = sum(math.sin(a) for a in angles) / len(angles)
    return math.hypot(c, s), math.atan2(s, c)


def screen_dir_words(a):
    """a: screen angle, radians, x right, y down."""
    deg = math.degrees(a) % 360
    names = ["right", "down and right", "down", "down and left", "left", "up and left", "up", "up and right"]
    return names[int(((deg + 22.5) % 360) // 45)]


def check_fleet(lines, ctx, near):
    fl = kind(lines, "fleet")
    if not fl:
        return not_measured("fleet")
    by = {}
    for d in fl:
        by.setdefault(str(d.get("id", d.get("name", "?"))), []).append(d)
    fleets, worst = [], []
    wake_dirs, heading_dirs = {}, {}
    seen_sailing = set()
    for fid, ds in by.items():
        samples = bad = 0
        lowest = None
        last_course = None
        for d in ds:
            c, b = unit2(d.get("course")), unit2(d.get("bow"))
            sailing = d.get("sailing") in (True, 1, None)
            if c and sailing:
                last_course = c
                seen_sailing.add(fid)
            # A fleet seen to arrive keeps the heading of its last leg at rest (headings are not saved in a save, so
            # a fleet only ever seen at rest is not judged; harbour fleets face the table on purpose, with no wake).
            if not sailing and last_course and (num(d.get("wake"), 0) or 0) > 0:
                c = last_course
            elif not sailing:
                c = None
            if c and b:
                samples += 1
                dot = c[0] * b[0] + c[1] * b[1]
                if dot < BOW_DOT:
                    bad += 1
                if lowest is None or dot < lowest[0]:
                    lowest = (dot, d)
            wake = num(d.get("wake"), 0) or 0
            bp, tp = d.get("bowpx"), d.get("tailpx")
            if wake > 0 and isinstance(bp, list) and isinstance(tp, list) and len(bp) >= 2 and len(tp) >= 2:
                dx, dy = num(tp[0], 0) - num(bp[0], 0), num(tp[1], 0) - num(bp[1], 0)
                if math.hypot(dx, dy) > 2:
                    wake_dirs.setdefault(fid, []).append(math.atan2(dy, dx))
                    hd = c or b
                    if hd:
                        heading_dirs.setdefault(fid, []).append(math.atan2(hd[1], hd[0]) - math.radians(num(d.get("camyaw"), 0) or 0))
        name = ds[0].get("name") or fid
        entry = {"id": fid, "name": name, "nation": ds[0].get("nation"), "samples": samples, "off_course": bad}
        if lowest:
            entry["worst_dot"] = round(lowest[0], 3)
        failing = samples > 0 and bad / samples > BOW_BAD_SHARE
        entry["fails"] = failing
        fleets.append(entry)
        if failing and lowest:
            ang = math.degrees(math.acos(max(-1.0, min(1.0, lowest[0]))))
            worst.append(moment(lowest[1], near, kind="bow off course", fleet=name, angle_deg=round(ang),
                                words=f"{name} points {ang:.0f} degrees away from the way it sails"))
    measured_any = any(f["samples"] for f in fleets)
    # the habit: every wake points the same way on screen while the fleets sail different ways
    habit = None
    # (only fleets seen sailing tonight count for the habit: a fleet only ever seen at rest has no known heading)
    wake_dirs = {k: v for k, v in wake_dirs.items() if k in seen_sailing}
    heading_dirs = {k: v for k, v in heading_dirs.items() if k in seen_sailing}
    if len(wake_dirs) >= 3:
        per_fleet_wake = [circ_spread(v)[1] for v in wake_dirs.values()]
        per_fleet_head = [circ_spread(v)[1] for k, v in heading_dirs.items()]
        rw = circ_spread(per_fleet_wake)
        rh = circ_spread(per_fleet_head) if len(per_fleet_head) >= 3 else None
        same = sum(1 for a in per_fleet_wake if math.cos(a - rw[1]) > math.cos(math.radians(30)))
        habit = {"fleets_with_wakes": len(wake_dirs), "wakes_same_way": same, "wake_spread": round(rw[0], 3),
                 "heading_spread": round(rh[0], 3) if rh else None, "wake_way": screen_dir_words(rw[1])}
        habit["fails"] = bool(rw[0] > 0.9 and same >= 3 and rh is not None and rh[0] < 0.7)
        if habit["fails"]:
            d0 = by[next(iter(wake_dirs))][0]
            worst.insert(0, moment(d0, near, kind="wakes one way", count=same,
                                   words=f"{same} fleets' wakes all point {habit['wake_way']} on screen though they sail different ways"))
    if not measured_any and habit is None:
        return result("not-measured", {"fleets": len(fleets)}, [], "No fleet was seen sailing with a known course or a wake.")
    off = [f for f in fleets if f["fails"]]
    measured = {"fleets": len(fleets), "fleets_off_course": len(off), "fleets": fleets, "wake_habit": habit,
                "limits": {"bow_dot": BOW_DOT, "bad_share": BOW_BAD_SHARE}}
    measured["fleet_count"] = len(fleets)
    if off or (habit and habit["fails"]):
        parts = []
        if off:
            parts.append(f"{len(off)} fleet{'s' if len(off) != 1 else ''} ({', '.join(str(f['name']) for f in off[:3])}) "
                         f"{'point' if len(off) != 1 else 'points'} away from the way {'they sail' if len(off) != 1 else 'it sails'}")
        if habit and habit["fails"]:
            parts.append(f"{habit['wakes_same_way']} fleets' wakes all point {habit['wake_way']} on screen whatever way they sail")
        return result("fail", measured, worst, "On the map, " + join_words(parts) + ".")
    return result("pass", measured, worst, f"All {len(fleets)} fleets point the way they sail, and their wakes follow them.")


# ---- opening-shows-both-sides ----------------------------------------------------------------------------------
def check_opening(lines, ctx, near):
    pl = kind(lines, "planes")
    if not pl:
        return not_measured("planes")
    shots = [str(d.get("shot") or "") for d in pl]
    named = [s for s in dict.fromkeys(shots) if any(w in s.lower() for w in ("flyover", "opening", "00-"))]
    if named:
        opening = [d for d in pl if str(d.get("shot") or "") in named]
    else:
        first = shots[0]
        opening = []
        for d in pl:
            if str(d.get("shot") or "") != first:
                break
            opening.append(d)
    if len(opening) < 2:
        return result("not-measured", {"lines": len(pl)}, [], "Too few aircraft facts in the opening shot to judge it.")
    both = att = dfd = none = 0
    gap_start = None
    gaps = []
    for d in opening:
        f = d.get("inframe") or {}
        a, b = (num(f.get("attack"), 0) or 0) > 0, (num(f.get("defend"), 0) or 0) > 0
        both += a and b
        att += a and not b
        dfd += b and not a
        none += not a and not b
        t = num(d.get("t"), 0.0)
        if not (a and b):
            if gap_start is None:
                gap_start = d
        elif gap_start is not None:
            gaps.append((gap_start, d))
            gap_start = None
    if gap_start is not None:
        gaps.append((gap_start, opening[-1]))
    n = len(opening)
    share = both / n
    t0, t1 = num(opening[0].get("t"), 0.0), num(opening[-1].get("t"), 0.0)
    measured = {"shot": named or [shots[0]], "samples": n, "seconds": round(t1 - t0, 1),
                "share_both_sides": round(share, 3), "share_attackers_only": round(att / n, 3),
                "share_defenders_only": round(dfd / n, 3), "share_none": round(none / n, 3), "limit": OPENING_SHARE}
    worst = []
    for g0, g1 in sorted(gaps, key=lambda g: -(num(g[1].get("t"), 0) - num(g[0].get("t"), 0)))[:3]:
        a, b = num(g0.get("t"), 0.0), num(g1.get("t"), 0.0)
        worst.append(moment(g0, near, kind="one side missing", seconds=round(b - a, 1),
                            words=f"for {b - a:.1f} s the opening shows only {'the attackers' if att >= dfd else 'the defenders'}" if (att or dfd) else f"for {b - a:.1f} s the opening shows no aircraft"))
    if share < OPENING_SHARE:
        missing = "the defenders" if att >= dfd else "the attackers"
        return result("fail", measured, worst,
                      f"The opening shows both sides together only {share * 100:.0f}% of the time; {missing} are missing for most of it.")
    return result("pass", measured, worst, f"The opening shows both sides together {share * 100:.0f}% of the time.")


# ---- marks-on-every-model --------------------------------------------------------------------------------------
def check_marks(lines, ctx, near):
    mk = kind(lines, "marks")
    if not mk:
        return not_measured("marks")
    bare = {}
    for d in mk:
        if (num(d.get("marks"), 1) or 0) <= 0:
            key = (d.get("kind"), d.get("type"), d.get("nation"), d.get("year"), d.get("month"))
            bare.setdefault(key, []).append(d)
    measured = {"models": len(mk), "bare_models": sum(len(v) for v in bare.values()),
                "bare": [{"kind": k[0], "type": k[1], "nation": k[2], "year": k[3], "month": k[4], "count": len(v)}
                         for k, v in bare.items()]}
    if bare:
        worst = [moment(v[0], near, kind="bare model", model=k[1], nation=k[2],
                        words=f"the {k[2] or ''} {k[1] or k[0]} ({k[3]}-{k[4]}) carries no markings".replace("  ", " "))
                 for k, v in list(bare.items())[:8]]
        names = ", ".join(f"{k[2] or ''} {k[1] or k[0]}".strip() for k in list(bare)[:3])
        return result("fail", measured, worst,
                      f"{measured['bare_models']} of {len(mk)} models drawn carry no markings ({names}).")
    return result("pass", measured, [], f"All {len(mk)} models drawn carry their markings.")


# ---- men-gait-variety ------------------------------------------------------------------------------------------
def check_men(lines, ctx, near):
    mn = kind(lines, "men")
    if not mn:
        return not_measured("men")
    busy, failing, spreads, rates = [], [], [], []
    for d in mn:
        if (num(d.get("moving"), 0) or 0) < GAIT_MEN:
            continue
        busy.append(d)
        p10, p90 = num(d.get("run_p10")), num(d.get("run_p90"))
        sp = (p90 / p10) if (p10 and p90 and p10 > 0) else None
        r = num(d.get("rates"))
        if sp is not None:
            spreads.append(sp)
        if r is not None:
            rates.append(r)
        if (sp is not None and sp < GAIT_SPREAD) or (r is not None and r < GAIT_RATES):
            failing.append((d, sp, r))
    if not busy:
        return result("not-measured", {"seconds": len(mn), "seconds_with_20_moving": 0}, [],
                      "Never 20 or more men moving at once, so the variety of their running was not judged.")
    msp = median(spreads) if spreads else None
    mr = median(rates) if rates else None
    measured = {"seconds_with_20_moving": len(busy), "seconds_too_alike": len(failing),
                "speed_spread_median": round(msp, 3) if msp else None, "rates_median": mr,
                "capped_seconds": sum(1 for d in busy if d.get("capped")),
                "limits": {"spread": GAIT_SPREAD, "rates": GAIT_RATES, "men": GAIT_MEN}}
    worst = [moment(d, near, kind="men alike", spread=round(sp, 2) if sp else None, rates=r,
                    words=f"{int(num(d.get('moving'), 0))} men run with speeds only {((sp or 1) - 1) * 100:.0f}% apart and {r if r is not None else '?'} gait clips")
             for d, sp, r in sorted(failing, key=lambda x: (x[1] or 9))[:5]]
    bad = (msp is not None and msp < GAIT_SPREAD) or (mr is not None and mr < GAIT_RATES)
    if bad:
        return result("fail", measured, worst,
                      f"When the men run they look alike: speeds only {((msp or 1) - 1) * 100:.0f}% apart and {fmt_n(mr)} kind{'s' if mr != 1 else ''} of gait most of the time.")
    return result("pass", measured, worst, f"Running men vary: speeds {((msp or 1) - 1) * 100:.0f}% apart, {fmt_n(mr)} kinds of gait.")


# ---- vehicles-no-rocking ---------------------------------------------------------------------------------------
def check_vehicles(lines, ctx, near):
    vh = kind(lines, "vehicle")
    if not vh:
        return not_measured("vehicle")
    by = {}
    for d in vh:
        by.setdefault(str(d.get("id")), []).append(d)
    rockers, worst = [], []
    for vid, ds in by.items():
        ds.sort(key=lambda d: num(d.get("t"), 0.0))
        rates = []
        for a, b in zip(ds, ds[1:]):
            dt = num(b.get("t"), 0.0) - num(a.get("t"), 0.0)
            if dt <= 0 or dt > 2.0:
                rates.append(None)
                continue
            rate = wrap180(num(b.get("heading"), 0.0) - num(a.get("heading"), 0.0)) / dt
            slow = max(abs(num(a.get("speed"), 0.0)), abs(num(b.get("speed"), 0.0))) < ROCK_SPEED
            rates.append((num(b.get("t"), 0.0), rate, slow, b))
        swings = []
        for r0, r1 in zip(rates, rates[1:]):
            if r0 and r1 and r0[2] and r1[2] and abs(r0[1]) > ROCK_RATE and abs(r1[1]) > ROCK_RATE and r0[1] * r1[1] < 0:
                swings.append(r1)
        best, j = 0, 0
        for i in range(len(swings)):
            while swings[i][0] - swings[j][0] > ROCK_WINDOW:
                j += 1
            if i - j + 1 > best:
                best = i - j + 1
                at = swings[i]
        if best > ROCK_TIMES:
            d = at[3]
            rockers.append({"id": vid, "type": d.get("type"), "nation": d.get("nation"), "swings_in_10s": best})
            worst.append(moment(d, near, kind="rocking", vehicle=d.get("type"), swings=best,
                                words=f"a standing {d.get('nation') or ''} {d.get('type') or 'vehicle'} swings its nose back and forth {best} times in 10 s".replace("  ", " ")))
    measured = {"vehicles": len(by), "rocking": len(rockers), "rockers": rockers,
                "limits": {"yaw_rate_deg_s": ROCK_RATE, "speed_m_s": ROCK_SPEED, "times": ROCK_TIMES, "window_s": ROCK_WINDOW}}
    if rockers:
        return result("fail", measured, worst,
                      f"{len(rockers)} of {len(by)} vehicles rock back and forth while standing still.")
    return result("pass", measured, [], f"None of the {len(by)} vehicles rocks while standing still.")


# ---- battle-end-not-abrupt -------------------------------------------------------------------------------------
def real_time_of(lines, bt):
    """Real seconds at battle second bt, from lines that carry both."""
    pts = sorted((num(d.get("bt")), num(d.get("t"))) for d in lines if num(d.get("bt")) is not None and num(d.get("t")) is not None)
    if not pts:
        return None
    if bt <= pts[0][0]:
        return pts[0][1] - (pts[0][0] - bt)
    for (b0, t0), (b1, t1) in zip(pts, pts[1:]):
        if b0 <= bt <= b1:
            return t0 + (t1 - t0) * ((bt - b0) / (b1 - b0) if b1 > b0 else 0.0)
    return pts[-1][1] + (bt - pts[-1][0])


def check_end(lines, ctx, near):
    ends = kind(lines, "end")
    if not ends:
        return not_measured("end")
    d = ends[-1]
    broke = num(d.get("broke_at"))
    card = num(d.get("card_at"))
    offered = d.get("offered_continue")
    measured = {"how": d.get("how"), "broke_at_bt": broke, "ended_at": d.get("ended_at"), "card_at": card,
                "offered_continue": offered, "limit_s": END_GAP}
    if broke is None:
        return result("pass", measured, [], f"The battle ended by {d.get('how') or 'its limit'}, not by a side breaking, so the end was not sudden.")
    t_break = real_time_of(lines, broke)
    if t_break is None or card is None:
        return result("not-measured", measured, [], "The break or the card's time is missing, so the end was not judged.")
    gap = card - t_break
    measured["break_to_card_s"] = round(gap, 1)
    if gap < END_GAP and offered is not True:
        w = moment(d, near, kind="abrupt end", seconds=round(gap, 1),
                   words=f"the result card comes {gap:.0f} s after the enemy breaks, with no offer to keep fighting")
        return result("fail", measured, [w],
                      f"The battle stops {gap:.0f} s after the losing side breaks and the card appears with no offer to go on.")
    return result("pass", measured, [], f"After the break the battle runs on {gap:.0f} s before the card" +
                  (", and offers to go on." if offered else "."))


# ---- look-matches-place ----------------------------------------------------------------------------------------
def check_look(lines, ctx, near):
    lk = kind(lines, "look")
    if not lk:
        return not_measured("look")
    d = lk[-1]
    measured = {k: d.get(k) for k in ("look", "month", "year", "lat", "lon", "snow", "leafless", "hour", "weather")}
    year, month = num(ctx.get("year")), num(ctx.get("month"))
    issues = []
    if year is not None and num(d.get("year")) is not None and int(year) != int(num(d.get("year"))):
        issues.append(f"the look is for {int(num(d.get('year')))} but the scene is set in {int(year)}")
    m = num(d.get("month"))
    lat = num(d.get("lat"))
    if m is not None and lat is not None:
        summer = (m in (6, 7, 8)) if lat >= 0 else (m in (12, 1, 2))
        if summer and abs(lat) < 60 and d.get("snow"):
            issues.append("snow lies on the ground in high summer")
        if summer and d.get("leafless"):
            issues.append("the trees are bare in high summer")
    if month is not None and m is not None and int(month) != int(m):
        issues.append(f"the look is for month {int(m)} but the scene is set in month {int(month)}")
    place = f"{d.get('look')} ({d.get('month')}/{d.get('year')}, {d.get('weather') or 'weather unknown'})"
    measured["theater"] = ctx.get("theater")
    if issues:
        return result("fail", measured, [moment(d, near, kind="look", words=issues[0])],
                      f"The {place} look does not fit: " + "; ".join(issues) + ".")
    if year is None and ctx.get("theater") is None:
        return result("not-measured", measured, [], f"The {place} look was used; nothing says where or when the scene is, to compare.")
    return result("pass", measured, [], f"The {place} look was used and fits the scene's place and date.")


CHECKS = {
    "camera-smooth": check_camera_smooth,
    "fleet-bow-on-course": check_fleet,
    "opening-shows-both-sides": check_opening,
    "marks-on-every-model": check_marks,
    "men-gait-variety": check_men,
    "vehicles-no-rocking": check_vehicles,
    "battle-end-not-abrupt": check_end,
    "look-matches-place": check_look,
}


def run_checks(path, names=None, ctx=None):
    """Run the named checks (all when None) over one facts file. Returns {name: result} plus '_file' info."""
    ctx = ctx or {}
    names = list(names or CHECKS)
    out = {}
    if not path or not os.path.exists(path):
        for n in names:
            out[n] = result("not-measured", {}, [], "The game wrote no facts file in this run, so this was not checked.")
        out["_file"] = {"path": path, "lines": 0, "bad_lines": 0, "kinds": {}}
        return out
    lines, bad = load_facts(path)
    near = Nearest(lines)
    kinds = {}
    for d in lines:
        kinds[d["k"]] = kinds.get(d["k"], 0) + 1
    run = kind(lines, "run")
    for n in names:
        fn = CHECKS.get(n)
        if fn is None:
            out[n] = result("not-measured", {}, [], f"No check named '{n}' exists yet in check_facts.py.")
            continue
        try:
            c = dict(ctx)
            c.update((ctx.get("per_check") or {}).get(n, {}))
            out[n] = fn(lines, c, near)
        except Exception as e:  # a broken line must not stop the night's other checks
            out[n] = result("not-measured", {"error": f"{type(e).__name__}: {e}"}, [],
                            f"The check could not read the facts ({type(e).__name__}).")
    out["_file"] = {"path": path, "lines": len(lines), "bad_lines": bad, "kinds": kinds,
                    "run": run[0] if run else None}
    return out


def main():
    ap = argparse.ArgumentParser(description="Run named checks over a critic-facts.jsonl file.")
    ap.add_argument("facts")
    ap.add_argument("--checks", help="comma-separated names (default: all)")
    ap.add_argument("--context", help="JSON: theater, year, month of the scene; limits; max per measure")
    ap.add_argument("--out", help="write the full result here (JSON)")
    ap.add_argument("--list", action="store_true", help="list the checks and leave")
    a = ap.parse_args()
    if a.list:
        for n, f in CHECKS.items():
            print(n)
        return
    ctx = json.loads(a.context) if a.context else {}
    res = run_checks(a.facts, a.checks.split(",") if a.checks else None, ctx)
    if a.out:
        with open(a.out, "w") as fh:
            json.dump(res, fh, indent=1)
    for n, r in res.items():
        if n.startswith("_"):
            continue
        print(f"{r['status']:>13}  {n}: {r['words']}")


if __name__ == "__main__":
    main()
