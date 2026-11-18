"""The soldier's and the crews' arms against a real body's range of motion: every joint of every frame of every clip,
measured with rom.py (the mocap animator's check, the coordinator's limits of 18 November 2026), not looked at.

    python arms_check.py [--soldier] [--hero] [--crew] [--only a,b] [--out report.json] [--md table.md]
                                                                    (the bpy module; nothing is rendered)
    python arms_check.py --sheet arms_check.png [--only a,b]       (the soldier's clips drawn: see sheet)

soldier   every clip of soldier.CLIPS as clip_state plays it at 60 fps (each with its own weapon)
hero      every man of the hero clip (soldier.hero_plan) through his whole Performance, at 60 fps
crew      every clip of crew.CREW on the crew's rig, at 60 fps

The report per clip is rom.check's: the worst of every measure with its frame, the faults (over by more than rom.TOL),
and the joints that turn faster than 900 degrees a second between two 60ths. ARM lists the arm measures; the rest
(hips, knees, spine, neck: limits marked "mine" in rom.py are common clinical ranges) are reported beside them.

AIM (every frame with the rifle fully at the shoulder, soldier.Rig.sight_measures): the right eye's distance off the
sights' line and how far behind the rear sight's notch it sits, the head's forward tilt and its cant to the right
(the coordinator's review: 10-20 degrees forward, canted a little right), the right cheekbone over the comb, the right
elbow's drop below the shoulder, and how far the butt was eased in the pocket.
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import rom  # noqa: E402

ARM = ("sh_", "el_", "pron", "sup", "wr_", "cl_")
ARM_JOINTS = ("clavicle", "upperarm", "lowerarm", "hand")


def is_arm(key):
    return key.startswith(ARM)


def angles(rig, st):
    """One state's measures (rom.measure) as plain floats: {key_side: value}."""
    m = rom.measure(rom.Body.of_rig(rig), rom.Poses.of_states([st]))
    return {k: float(v[0]) for k, v in m.items()}


def check(rig, states, name, fps=60.0, kind="rifle"):
    rep = rom.check_states(states, rig, fps=fps, name=name)
    rep["arm_faults"] = [r for r in rep["faults"] if is_arm(r["key"])]
    rep["arm_fast"] = [(j, d) for j, d in rep["fast"] if j.startswith(ARM_JOINTS)]
    a = aim_report(rig, states, kind)
    if a:
        rep["aim"] = a
    return rep


def aim_report(rig, states, kind="rifle"):
    """The aimed frames' sight measures (Rig.sight_measures), each as [least, most]; None if he never aims."""
    rows = [rig.sight_measures(st, kind) for st in states if st.get("sight_miss") is not None and "W" in st]
    if not rows:
        return None
    out = {"frames": len(rows), "weapon": kind}
    for k in rows[0]:
        vals = [r[k] for r in rows]
        out[k] = [round(min(vals), 1), round(max(vals), 1)]
    return out


def aim_line(a):
    return (f"aim ({a['frames']} frames, {a['weapon']}): eye off the sights' line up to {a['eye_line_cm'][1]:g} cm "
            f"({a['eye_behind_rear_cm'][0]:g}-{a['eye_behind_rear_cm'][1]:g} cm behind the rear sight); head "
            f"{a['head_pitch'][0]:g}-{a['head_pitch'][1]:g} deg forward, canted {a['head_cant'][0]:g}-{a['head_cant'][1]:g} "
            f"right; cheekbone {a['cheek_over_comb_cm'][0]:g}-{a['cheek_over_comb_cm'][1]:g} cm over the comb; right elbow "
            f"{a['elbow_r_drop'][0]:g}-{a['elbow_r_drop'][1]:g} deg below the shoulder; butt eased up to {a['butt_slide_cm'][1]:g} cm")


def light_rig(nation="GER", seed=None):
    """The soldier's rig alone (no mesh): his joints, the armature and human.py's rest, in about a second."""
    import kit
    import mocap
    import soldier as S
    kit.fresh(S.FPS)
    man = S.mh_body(nation, seed)
    J = S.joints_of(man)
    S.fit_body(J)
    mrest = mocap.Rest.of(man["armature"])
    arm = S.build_armature(J)
    rig = S.Rig(arm, J)
    rig.mrest = mrest
    return rig


def clip_states(rig, name, fps=60.0):
    import soldier as S
    c = S.CLIPS[name]
    kind = "lmg" if c["weapon"] == "lmg" else "rifle"
    rig.weapon_kind = kind
    n = int(round(c["long"] * fps)) + 1
    sts = [S.clip_state(rig, name, k / fps, kind) for k in range(n)]
    rig.weapon_kind = "rifle"
    return sts


def clip_kind(name):
    import soldier as S
    return "lmg" if S.CLIPS[name]["weapon"] == "lmg" else "rifle"


def soldier_clips(rig, names=None):
    import soldier as S
    S.fill_mocap_events(rig)
    out = {}
    for name, c in S.CLIPS.items():
        if names and name not in names:
            continue
        out[name] = check(rig, clip_states(rig, name), name, kind=clip_kind(name))
        out[name]["mocap"] = bool(c.get("mocap"))
    return out


def hero_states(rig, man, fps=60.0):
    import soldier as S
    rig.weapon_kind = man["weapon"]
    perf = S.Performance(rig, [dict(sg) for sg in man["segs"]], kind=man["weapon"], until=S.HERO_SECONDS + 0.5)
    sts = [perf.state(k / fps) for k in range(int(S.HERO_SECONDS * fps) + 1)]
    rig.weapon_kind = "rifle"
    return sts


def hero(rig, names=None):
    import soldier as S
    S.fill_mocap_events(rig)
    out = {}
    for man in S.hero_plan():
        name = "man %d (%s)" % (man["i"], man["role"])
        if names and name not in names and str(man["i"]) not in names:
            continue
        out[name] = check(rig, hero_states(rig, man), name, kind=man["weapon"])
    return out


def crew_clips(names=None):
    import crew as C
    m = C.setup()
    rig = m["rig"]
    out = {}
    for name, c in C.CREW.items():
        if names and name not in names:
            continue
        n = int(round(c["long"] * 60)) + 1
        kind = c.get("weapon") if c.get("weapon") in ("rifle", "lmg") else "rifle"
        out[name] = check(rig, [rig.solve(c["fn"](k / 60.0)) for k in range(n)], name, kind=kind)
    return out


def strip(reps):
    return {k: rom._strip(v) for k, v in reps.items()}


def line(rep, n=4):
    f = rep["arm_faults"]
    txt = "; ".join(f"{r['label']} {rom.side_name(r['side'])} {r['value']:g} (+{r['over']:g}) @{r['frame']}" for r in f[:n])
    fast = ", ".join(f"{rom.joint_name(j)} {d['deg_s']}/s @{d['frame']}" for j, d in rep["arm_fast"])
    body = [r for r in rep["faults"] if not is_arm(r["key"])]
    btxt = "; ".join(f"{r['label']} {rom.side_name(r['side'])} {r['value']:g} (+{r['over']:g}) @{r['frame']}" for r in body[:2])
    return (txt or "arms ok") + (" | FAST " + fast if fast else "") + (" | body: " + btxt if btxt else "")


# ------------------------------------------------------------------------------------------------ the sheet
SHEET_TILE = 168          # px, each view
SHEET_LABEL = 300         # px, the clip's column of numbers


def key_frames(rep, n_frames, k=4):
    """Three frames spread through the clip and the one where the arms come nearest their limits."""
    near = max((r for r in rep["rows"] if is_arm(r["key"])), key=lambda r: r["over"] / max(abs(r["limit"]), 10.0))
    fs = {0, (n_frames - 1) // 3, 2 * (n_frames - 1) // 3, int(near["frame"])}
    fs = sorted(fs)
    while len(fs) < k:
        fs.append(n_frames - 1)
        fs = sorted(set(fs))
        if len(fs) < k:
            fs.append((fs[-1] + fs[-2]) // 2)
            fs = sorted(set(fs))
    return fs[:k], near


def nearest(rep, n=3):
    """The arm measures nearest their limits (value, limit, frame), worst first, and the fastest arm joint."""
    rows = [r for r in rep["rows"] if is_arm(r["key"]) and r["key"] not in ("el_torso", "el_back", "sh_abd90")]
    rows.sort(key=lambda r: -(r["over"] / max(abs(r["limit"]), 10.0)))
    sp = max(((j, d) for j, d in rep["speed"].items() if j.startswith(ARM_JOINTS)), key=lambda x: x[1]["deg_s"])
    return rows[:n], sp


SHEET_REUSE = bool(os.environ.get("ARMS_REUSE"))     # (lay the sheet out again from the tiles already rendered)
SHORT = (("horizontal", "horiz."), ("flexion/abduction", "elevation"), ("extension / horiz. abduction", "extension/horiz. abd."),
         ("internal rotation", "int. rotation"), ("external rotation", "ext. rotation"))


def short_label(label):
    """A rom label short enough for the sheet's column: no parenthesis, the long words cut."""
    t = label.split(" (")[0].strip()
    for a, b in SHORT:
        t = t.replace(a, b)
    return t + (" (cm)" if label.endswith("(cm)") else "")


def fit_text(d, txt, font, width):
    """txt, cut with an ellipsis where it would run past `width` pixels."""
    if d.textlength(txt, font=font) <= width:
        return txt
    while txt and d.textlength(txt + "...", font=font) > width:
        txt = txt[:-1]
    return txt + "..."


def sheet(out, names=None, fps=60.0):
    """arms_check.png: every soldier clip at four frames (three through it and its nearest-the-limit one), front
    and side, cropped to the upper body, with the worst numbers beside it (small Cycles tiles, 16 samples)."""
    import bpy
    import kit
    import soldier as S
    from mathutils import Vector
    from PIL import Image, ImageDraw
    kit.fresh(S.FPS)
    men = {"rifle": S.assemble("GER"), "lmg": S.assemble("GER", weapon="lmg")}
    for kind, m in men.items():
        S.fill_mocap_events(m["rig"])
    sc = bpy.context.scene
    kit.studio(sun_elev=42.0, sun_turn=-30.0)
    S.shared_render(SHEET_TILE, SHEET_TILE, 16)            # (Cycles, 16 samples, two threads: the cores are shared)
    sc.cycles.transparent_max_bounces = 32
    cam_data = bpy.data.cameras.new("sheet_cam")
    cam_data.type = "ORTHO"
    cam = bpy.data.objects.new("sheet_cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    work = os.path.join(os.environ.get("ARMS_WORK", "/home/user/art/soldier/arms/"), "sheet") + "/"
    os.makedirs(work, exist_ok=True)

    def all_objs(m):
        obs = [m["arm"], m["body"], m["helmet"], m["weapon"], m["weapon_part"]] + list(m["kit"])
        return [o for o in obs if o is not None]
    rows = []
    for name, c in S.CLIPS.items():
        if names and name not in names:
            continue
        kind = "lmg" if c["weapon"] == "lmg" else "rifle"
        m = men[kind]
        for k2, m2 in men.items():
            for o in all_objs(m2):
                o.hide_render = k2 != kind
        rig = m["rig"]
        rig.weapon_kind = kind
        n = int(round(c["long"] * fps)) + 1
        sts = [S.clip_state(rig, name, i / fps, kind) for i in range(n)]
        rep = check(rig, sts, name, fps, kind=kind)
        frames, near = key_frames(rep, n)
        tiles = []
        for f in frames:
            st = sts[f]
            S.pose_once(m["arm"], rig, st, 0)
            sc.frame_set(0)
            # (look at the upper body: the chest, the head and both hands; front from where his hips face, and from
            # his right side)
            pts = np.array([st["H"][b] for b in ("chest", "head", "hand_l", "hand_r", "upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r")])
            ctr = 0.5 * (pts.min(0) + pts.max(0))
            span = float(np.max(pts.max(0) - pts.min(0)))
            Dp = st["D"]["pelvis"]
            fwd = Dp[:, 0].copy()
            fwd[2] = 0.0
            if np.linalg.norm(fwd) < 0.3:              # (lying: his hips face the ground; his head's way is "ahead")
                fwd = Dp[:, 2].copy()
                fwd[2] = 0.0
            fwd /= np.linalg.norm(fwd)
            right = np.array((fwd[1], -fwd[0], 0.0))
            for view, d in (("front", fwd), ("side", right)):
                loc = ctr + d * 3.0 + np.array((0.0, 0.0, 0.25))
                cam.location = Vector(loc.tolist())
                cam.rotation_euler = (Vector((ctr - loc).tolist())).to_track_quat("-Z", "Y").to_euler()
                cam_data.ortho_scale = max(0.75, span + 0.25)
                path = f"{work}{name}_{f}_{view}.png"
                if not (SHEET_REUSE and os.path.exists(path)):
                    sc.render.filepath = path
                    bpy.ops.render.render(write_still=True)
                tiles.append((f, view, path))
        rows.append((name, rep, frames, tiles, bool(c.get("mocap"))))
        rig.weapon_kind = "rifle"
    # the sheet
    T, L = SHEET_TILE, SHEET_LABEL
    head = 64
    W = L + 8 * T
    H = head + len(rows) * (T + 22)
    im = Image.new("RGB", (W, H), (236, 233, 226))
    d = ImageDraw.Draw(im)
    font = kit._font(15)
    small = kit._font(12)
    big = kit._font(22)
    d.text((10, 8), "Arms against a real body's range (rom.py, every frame at 60 fps): each clip at four frames, front | side",
           fill=(20, 20, 20), font=big)
    d.text((10, 38), "numbers: the arm measures nearest their limits (value / limit @frame; red = over), and the fastest arm "
           "joint (limit 900 deg/s)", fill=(60, 60, 60), font=font)
    for i, (name, rep, frames, tiles, mocap) in enumerate(rows):
        y = head + i * (T + 22)
        d.line([(0, y - 1), (W, y - 1)], fill=(180, 175, 165))
        d.text((8, y + 4), name + ("  (capture)" if mocap else ""), fill=(10, 10, 10), font=big)
        near_rows, (j, spd) = nearest(rep)
        yy = y + 34
        for r in near_rows:
            over = r["over"] > rom.TOL
            txt = f"{short_label(r['label'])} {rom.side_name(r['side'])[:1]}: {r['value']:g}/{r['limit']:g} @{r['frame']}"
            d.text((8, yy), fit_text(d, txt, small, L - 12), fill=(190, 20, 20) if over else (30, 30, 30), font=small)
            yy += 17
        d.text((8, yy), fit_text(d, f"fastest: {rom.joint_name(j)} {spd['deg_s']} deg/s @{spd['frame']}", small, L - 12),
               fill=(190, 20, 20) if spd["over"] else (30, 30, 30), font=small)
        yy += 17
        nf = len(rep["arm_faults"]) + len(rep["arm_fast"])
        d.text((8, yy), "arms within every limit" if nf == 0 else f"{nf} arm faults",
               fill=(20, 120, 40) if nf == 0 else (190, 20, 20), font=font)
        a = rep.get("aim")
        if a:
            yy += 19
            off = a["eye_line_cm"][1] > 1.0 or a["head_pitch"][1] > 20.5 or a["head_pitch"][0] < 9.5
            d.text((8, yy), fit_text(d, f"eye off sight line <={a['eye_line_cm'][1]:g} cm, behind rear sight "
                                        f"{a['eye_behind_rear_cm'][0]:.0f}-{a['eye_behind_rear_cm'][1]:.0f} cm", small, L - 12),
                   fill=(190, 20, 20) if off else (30, 30, 30), font=small)
            yy += 15
            d.text((8, yy), fit_text(d, f"head {a['head_pitch'][0]:.0f}-{a['head_pitch'][1]:.0f} deg fwd, cant "
                                        f"{a['head_cant'][0]:.0f}-{a['head_cant'][1]:.0f} R, elbow r "
                                        f"{a['elbow_r_drop'][0]:.0f}-{a['elbow_r_drop'][1]:.0f} down", small, L - 12),
                   fill=(190, 20, 20) if off else (30, 30, 30), font=small)
        for k, (f, view, path) in enumerate(tiles):
            x = L + k * T
            im.paste(Image.open(path).convert("RGB"), (x, y))
            d.text((x + 4, y + T + 3), f"{view} @{f}", fill=(40, 40, 40), font=small)
    im.save(out)
    return out


if __name__ == "__main__":
    args = sys.argv[1:]
    outp = args[args.index("--out") + 1] if "--out" in args else os.path.join(os.environ.get("ARMS_WORK", "/home/user/art/soldier/arms/"), "arms_report.json")
    md = args[args.index("--md") + 1] if "--md" in args else None
    names = args[args.index("--only") + 1].split(",") if "--only" in args else None
    res = {}
    rig = None
    if "--soldier" in args or not any(a in args for a in ("--hero", "--crew")):
        rig = light_rig()
        res["soldier"] = soldier_clips(rig, names)
    if "--hero" in args:
        rig = rig or light_rig()
        res["hero"] = hero(rig, names)
    if "--sheet" in args:
        out = args[args.index("--sheet") + 1]
        print("sheet", sheet(out, names))
        sys.exit(0)
    if "--crew" in args:
        res["crew"] = crew_clips(names)
    json.dump({p: strip(r) for p, r in res.items()}, open(outp, "w"), indent=1)
    for part, reps in res.items():
        for name, rep in reps.items():
            print(f"{part:7s} {name:22s} {line(rep)}", flush=True)
            if rep.get("aim"):
                print(f"{'':7s} {'':22s} {aim_line(rep['aim'])}", flush=True)
    if md:
        with open(md, "w") as f:
            for part, reps in res.items():
                f.write(f"### {part}\n\n" + rom.table(reps) + "\n\n" + rom.table(reps, rom.COLS_BODY) + "\n\n"
                        + rom.speed_table(reps) + "\n\n")
    print("written", outp)
