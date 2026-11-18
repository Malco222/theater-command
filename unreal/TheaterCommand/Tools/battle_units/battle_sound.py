"""Soundtracks for the soldier's preview movies, from the approved battle sound pack (Brian, 18 November 2026 12:02Z).

Two steps, so that the mix re-runs without Blender:

    python battle_sound.py events      the movies' sound events, rebuilt from soldier.py's own plans (hero_plan, MOVIES)
                                       and the clips' events (the ones that drive the flashes and the dust), with the
                                       bpy module but nothing rendered: each event's time, cue, man and place, and the
                                       camera's path -> battle_sound_events.json beside this script
    python battle_sound.py mix [--only squad_advance,walk] [--seed 7] [--no-deliver]
                                       numpy and ffmpeg only: a take of each event's sound placed at its moment, turned
                                       up or down and panned by its distance and bearing from the camera at that moment,
                                       late by sound's speed over that distance (and duller far off); a quiet far
                                       battle under it and, in the squad's clip, a crack or two overhead. Each movie's
                                       soundtrack muxed in (AAC 48 kHz stereo, the video stream copied):
                                       pictures/squad_advance_sound.mp4 beside squad_advance.mp4, and so on.

The pack's rules (its README): where a sound has several takes, a random one each time and never the same take twice
running; a loop is played from loop.from for loop.length (the WAV masters are the loop itself: from 0); the sounds go
in at the level they are mastered at (their LUFS), so a footstep sits under a shot as it does in the game.
"""

import json
import math
import os
import subprocess
import sys
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
PACK = "/mnt/project-files/sound/battle-pack/"
WAVS = PACK + "wav/"
PICS = "/mnt/project-files/battle-models/land/soldier/pictures/"
EVENTS = os.path.join(HERE, "battle_sound_events.json")
WORK = os.environ.get("SOUND_WORK", "/home/user/art/soldier/sound/")
RATE = 48000
SPEED = 343.0           # (m/s, sound in air)
REF = 4.0               # (m: nearer than this a sound is at its mastered level; farther, down 6 dB a doubling)

# Which takes play for an event (the README's table "Soldier"): event fx (and cue) -> the takes
TAKES = {
    "kar98": ["kar98_shot"],                                  # German rifles, near (the pack's one Mauser take: its level and
    "rifle_far": ["rifle_shot_%02d" % k for k in range(1, 5)],  # pitch moved a little each shot); farther, rifle_shot_01-04
    "mg42": ["mg42_burst_01", "mg42_burst_02"],              # the German LMG's burst
    "bolt": ["bolt_k98"],
    "footstep": ["footstep_%02d" % k for k in range(1, 9)],
    "elbow": ["footstep_%02d" % k for k in range(1, 9)],
    "knee_down": ["knee_down"],
    "body_fall": ["body_fall_%02d" % k for k in range(1, 6)],
    "body_fall_cry": ["body_fall_cry_%02d" % k for k in range(1, 6)],   # the hit man, the camera close
    "kit_rattle": ["kit_rattle_%02d" % k for k in range(1, 4)],        # getting up; the dropped rifle landing
    "crack": ["bullet_crack"],
    "bed": ["battle_far_loop"],
}
# (german_drop_weapon is a voice, a shouted order to a surrendering man in the README's "Voices and calls": not used for
# the rifle a hit man drops, which is kit_rattle's job)
TRIM_DB = {"footstep": 3.0, "elbow": -1.0, "knee_down": 3.0, "body_fall": 2.0, "body_fall_cry": 2.0, "kit_rattle": 2.0, "bolt": 0.0,
           "kar98": 0.0, "rifle_far": 0.0, "mg42": 0.0, "crack": -4.0, "bed": -13.0}
SQUAD_STEPS_DB = -6.0    # (the squad's twelve men stepping 13-28 m off, 155 steps in 8 s: under the shots, not level with them)
NEAR_RIFLE = 30.0        # (m: a rifle nearer the camera than this is kar98_shot, the README's "close, by army" (the
                         # whole squad, 13-28 m off); farther, the rifle_shot takes, mastered 5 dB down as the far rifles)


# ================================================================================================ the events (bpy)
def _cam_orbit(target, dist, pitch, yaw):
    p, y = math.radians(pitch), math.radians(yaw)
    t = np.asarray(target, float)
    return t + np.array((math.cos(y) * math.cos(p), math.sin(y) * math.cos(p), math.sin(p))) * dist


def _event_rows(S, perf, end, fps, place, role, weapon, man):
    """Each clip event of a man's Performance before `end`: on the frame it is seen on (as the flashes and the dust),
    where it happens (the muzzle for a shot, else its bone; world axes by place(t, point))."""
    rows = []
    for T, e, sg in perf.events(end):
        tf = math.ceil(T * fps - 1e-6) / fps
        if tf >= end:
            continue
        st = perf.state(tf)
        if e["fx"] == "muzzle_flash":
            W = np.asarray(st["W"], float)
            p = (W @ np.array(tuple(S.MUZZLE) + (1.0,)))[:3]
        else:
            bone = e.get("bone") or ("weapon" if e["fx"] == "rifle_drop" else "spine_02")
            p = np.asarray(st["H"][bone], float)
        q = place(tf, p)
        row = {"t": round(tf, 4), "fx": e["fx"], "cue": e.get("cue"), "clip": sg["clip"], "man": man, "role": role,
               "weapon": weapon, "at": [round(float(x), 3) for x in q]}
        if e.get("until") is not None:                     # (the bolt worked from t to until)
            row["until"] = round(tf + (e["until"] - e["t"]) / sg.get("rate", 1.0), 4)
        rows.append(row)
    return rows


def export_events(path=EVENTS):
    """The hero clip's and the clip movies' events and cameras, as the render path makes them (bpy needed, nothing
    rendered)."""
    sys.path.insert(0, HERE)
    import kit
    import soldier as S
    import battle_set as BS
    out = {"about": "sound events of the soldier's preview movies (battle_sound.py events): t (s, on the frame it is "
                    "seen on), fx and cue (the clip's event), man, role, weapon, at (world, m, Blender's axes: z up); "
                    "camera: eye and target per frame (path) or fixed",
           "movies": {}}
    # the squad's eight seconds (pictures_hero without the stage, the dressing or the render)
    kit.fresh(S.HERO_FPS)
    fps = S.HERO_FPS
    rows = []
    for man in S.hero_plan():
        m = S.assemble("GER", weapon=man["weapon"], seed=S.HERO_SEEDS[man["i"]])
        S.fit_body(m["J"])
        rig = m["rig"]
        S.fill_mocap_events(rig)
        perf = S.Performance(rig, [dict(sg) for sg in man["segs"]], kind=man["weapon"], until=S.HERO_SECONDS + 0.5)
        yaw = math.radians(man["turn"])
        R = np.array(((math.cos(yaw), -math.sin(yaw), 0.0), (math.sin(yaw), math.cos(yaw), 0.0), (0.0, 0.0, 1.0)))
        fwd = R[:, 0]

        def place(t, p, perf=perf, man=man, R=R, fwd=fwd):
            d = perf.travel(t)
            x, y = man["x"] + fwd[0] * d, man["y"] + fwd[1] * d
            return np.array((x, y, BS._ground_height(x, y, BS.ROAD))) + R @ p
        rows += _event_rows(S, perf, S.HERO_SECONDS, fps, place, man["role"], man["weapon"], man["i"])
        print(f"  man {man['i']} ({man['role']}): {sum(1 for r in rows if r['man'] == man['i'])} events", flush=True)
    frames = int(S.HERO_SECONDS * fps)
    eyes, tgts = [], []
    for f in range(frames):
        e, g = S.hero_camera(f / fps)
        eyes.append([round(v, 4) for v in e])
        tgts.append([round(v, 4) for v in g])
    out["movies"]["squad_advance"] = {"seconds": S.HERO_SECONDS, "fps": fps, "frames": frames, "close": False,
                                      "camera": {"kind": "path", "eye": eyes, "target": tgts},
                                      "events": sorted(rows, key=lambda r: r["t"])}
    # the clip movies: one man at the middle of the field (the ground slides under him), the camera still
    kit.fresh(S.FPS)
    m = S.assemble("GER")
    S.fill_mocap_events(m["rig"])
    for key, (parts, target, dist, pitch, yaw) in S.MOVIES.items():
        perf, long = S.movie_performance(m["rig"], parts)
        n = int(round(long * 30)) + 1
        rows = _event_rows(S, perf, n / 30.0, 30, lambda t, p: p, "hit" if key.startswith("die") else "alone", "rifle", 0)
        out["movies"][key] = {"seconds": n / 30.0, "fps": 30, "frames": n, "close": True,
                              "camera": {"kind": "fixed", "eye": [round(float(v), 4) for v in _cam_orbit(target, dist, pitch, yaw)],
                                         "target": list(target)},
                              "events": rows}
        print(f"  movie {key}: {len(rows)} events, {n} frames", flush=True)
    with open(path, "w") as f:
        json.dump(out, f, indent=1)
    print("events written:", path)
    return out


# ================================================================================================ the mix (numpy, ffmpeg)
_CACHE = {}


def load_wav(name):
    """A pack master as float32 (n, channels) at 48 kHz."""
    if name not in _CACHE:
        w = wave.open(WAVS + name + ".wav")
        assert w.getframerate() == RATE and w.getsampwidth() == 2, name
        a = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float32) / 32768.0
        _CACHE[name] = a.reshape(-1, w.getnchannels())
        w.close()
    return _CACHE[name]


def loop_span(name):
    """(from, length) s of a loop to play round (mp3-list.json's loop; the WAV master is the loop itself when it is
    exactly loop.length long, so from 0)."""
    meta = {x["name"]: x for x in json.load(open(PACK + "mp3-list.json"))}.get(name, {})
    lp = meta.get("loop")
    a = load_wav(name)
    secs = len(a) / RATE
    if not lp:
        return 0.0, secs
    start = 0.0 if abs(secs - lp["length"]) < 0.02 else lp["from"]
    return start, float(lp["length"])


class Picker:
    """A random take each time, never the same one twice running (per family)."""

    def __init__(self, rng):
        self.rng, self.last = rng, {}

    def take(self, family):
        names = TAKES[family]
        pool = [n for n in names if n != self.last.get(family)] or names
        n = pool[int(self.rng.integers(len(pool)))]
        self.last[family] = n
        return n


def camera_at(cam, t, fps):
    if cam["kind"] == "fixed":
        return np.asarray(cam["eye"], float), np.asarray(cam["target"], float)
    eyes, tgts = np.asarray(cam["eye"], float), np.asarray(cam["target"], float)
    x = min(max(t * fps, 0.0), len(eyes) - 1.0)
    i = int(math.floor(x))
    j = min(i + 1, len(eyes) - 1)
    w = x - i
    return eyes[i] * (1 - w) + eyes[j] * w, tgts[i] * (1 - w) + tgts[j] * w


def heard(p, eye, tgt):
    """(distance m, pan -1 left .. +1 right) of a point from the camera (z up)."""
    v = np.asarray(p, float) - eye
    d = float(np.linalg.norm(v))
    f = tgt - eye
    f = f / np.linalg.norm(f)
    r = np.cross(f, (0.0, 0.0, 1.0))
    r = r / np.linalg.norm(r)
    az = math.atan2(float(v @ r), float(v @ f))
    return d, max(-1.0, min(1.0, math.sin(az)))


def dull(x, d):
    """Air takes the top off a far sound: a gentle low-pass, 16 kHz near to about 4 kHz at 60 m (by FFT)."""
    if d < 8.0 or len(x) < 64:
        return x
    fc = max(4000.0, 16000.0 * (8.0 / d) ** 0.7)
    n = len(x)
    X = np.fft.rfft(x, axis=0)
    fr = np.fft.rfftfreq(n, 1.0 / RATE)
    H = 1.0 / np.sqrt(1.0 + (fr / fc) ** 4)
    return np.fft.irfft(X * H[:, None], n=n, axis=0).astype(np.float32)


def resample(x, k):
    """Pitch (and length) by k: >1 higher and shorter."""
    if abs(k - 1.0) < 1e-4:
        return x
    n = int(len(x) / k)
    src = np.arange(n) * k
    return np.stack([np.interp(src, np.arange(len(x)), x[:, c]) for c in range(x.shape[1])], axis=1).astype(np.float32)


def add(buf, x, t, gain, pan):
    """Mix a mono or stereo sound into the stereo buffer at t (s), constant-power panned."""
    i0 = int(round(t * RATE))
    if i0 >= len(buf):
        return
    if x.shape[1] == 1:
        a = (pan + 1.0) * math.pi / 4.0
        lr = x[:, 0:1] * np.array((math.cos(a), math.sin(a)), np.float32) * math.sqrt(2.0)
    else:
        lr = x.copy()
        if pan:
            lr[:, 0] *= min(1.0, 1.0 - pan)
            lr[:, 1] *= min(1.0, 1.0 + pan)
    lr = lr * gain
    if i0 < 0:
        lr, i0 = lr[-i0:], 0
    n = min(len(lr), len(buf) - i0)
    buf[i0:i0 + n] += lr[:n]


def fade_out(x, secs):
    n = min(len(x), int(secs * RATE))
    if n > 0:
        x = x.copy()
        x[-n:] *= np.linspace(1.0, 0.0, n, dtype=np.float32)[:, None]
    return x


def family_of(e, d, close):
    fx, cue = e["fx"], e.get("cue")
    if fx == "muzzle_flash":
        if e["weapon"] == "lmg" or cue == "lmg_burst":
            return "mg42" if cue == "lmg_burst" else None          # (one burst take for the eight rounds)
        return "kar98" if d < NEAR_RIFLE else "rifle_far"
    if fx == "bolt":
        return "bolt"
    if fx == "footstep":
        return "footstep"
    if fx == "knee_down":
        return "knee_down"
    if fx == "body_fall":
        return "body_fall_cry" if (e["role"] == "hit" and (close or d < 20.0)) else "body_fall"
    if fx in ("rifle_drop", "kit_rattle"):
        return "kit_rattle"
    if fx == "elbow_down":                                         # (crawling: his elbow and the rifle's butt on the
        return "elbow"                                             # ground, the clip's cue a footstep, softer)
    return cue if cue in TAKES else None


def mix_movie(name, mv, rng, out_wav):
    """One movie's soundtrack as a stereo 48 kHz WAV: returns what went in."""
    n = int(round(mv["seconds"] * RATE))
    buf = np.zeros((n, 2), np.float32)
    pick = Picker(rng)
    fps = mv["fps"]
    used, skipped = {}, {}
    evs = mv["events"]
    bursts = [r["t"] for r in evs if r["fx"] == "muzzle_flash" and (r.get("cue") == "lmg_burst")]
    for e in evs:
        eye, tgt = camera_at(mv["camera"], e["t"], fps)
        d, pan = heard(e["at"], eye, tgt)
        fam = family_of(e, d, mv.get("close"))
        if fam is None:
            skipped[e["fx"]] = skipped.get(e["fx"], 0) + 1
            continue
        take = pick.take(fam)
        x = load_wav(take)
        gdb = TRIM_DB.get(fam, 0.0)
        if fam in ("footstep", "elbow") and not mv.get("close"):
            gdb += SQUAD_STEPS_DB
        if fam in ("kar98", "bolt", "knee_down"):                   # (one take: the game's small changes carry it)
            x = resample(x, float(rng.uniform(0.96, 1.04)))
            gdb += float(rng.uniform(-1.5, 1.5))
        if fam == "mg42":
            # (the clip's burst is eight rounds in 0.8 s, then a pause: the take cut there, its echo let die)
            nxt = [b for b in bursts if b > e["t"] + 1e-3]
            span = min(0.85, (nxt[0] - e["t"]) if nxt else 0.85)
            x = fade_out(x[:int((span + 0.3) * RATE)], 0.3)
        if fam == "bolt" and e.get("until"):
            x = fade_out(x[:int((e["until"] - e["t"] + 0.35) * RATE)], 0.2)
        gain = 10 ** (gdb / 20.0) * (REF / max(d, REF))
        x = dull(x, d)
        add(buf, x, e["t"] + d / SPEED, gain, pan * (0.8 if mv.get("close") else 1.0))
        used[take] = used.get(take, 0) + 1
    # the far battle under it all: the loop from a random place in it, round as needed
    bed = load_wav("battle_far_loop")
    s0, L = loop_span("battle_far_loop")
    loop = bed[int(s0 * RATE):int((s0 + L) * RATE)]
    off = int(rng.integers(len(loop)))
    reps = np.concatenate([loop[off:]] + [loop] * (1 + n // len(loop)))[:n]
    reps = reps.copy()
    k = int(0.4 * RATE)
    reps[:k] *= np.linspace(0.0, 1.0, k, dtype=np.float32)[:, None]
    reps = fade_out(reps, 0.25)
    add(buf, reps, 0.0, 10 ** ((TRIM_DB["bed"] - (4.0 if mv.get("close") else 0.0)) / 20.0), 0.0)
    used["battle_far_loop"] = 1
    # a crack or two overhead (the squad is under fire)
    if not mv.get("close"):
        for t in sorted(rng.uniform(1.5, mv["seconds"] - 1.0, size=2)):
            add(buf, load_wav("bullet_crack"), float(t), 10 ** (TRIM_DB["crack"] / 20.0), float(rng.uniform(-0.6, 0.6)))
            used["bullet_crack"] = used.get("bullet_crack", 0) + 1
    # level: about -20 dBFS RMS, the peaks kept under -1 dBFS
    rms = float(np.sqrt(np.mean(buf ** 2))) + 1e-9
    peak = float(np.abs(buf).max()) + 1e-9
    g = min(10 ** (-20 / 20) / rms, 10 ** (-1 / 20) / peak)
    buf *= g
    pcm = (np.clip(buf, -1.0, 1.0) * 32767.0).astype("<i2")
    with wave.open(out_wav, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    return {"takes": used, "skipped_fx": skipped, "gain_db": round(20 * math.log10(g), 1),
            "peak_dbfs": round(20 * math.log10(peak * g), 1)}


def mux(video, wav, out):
    """The movie with its soundtrack: the video stream copied, AAC 48 kHz stereo."""
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", video, "-i", wav, "-map", "0:v:0", "-map", "1:a:0", "-c:v", "copy",
                    "-c:a", "aac", "-b:a", "192k", "-ar", str(RATE), "-ac", "2", "-shortest", "-movflags", "+faststart", out],
                   check=True)


def probe(path):
    r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "stream=codec_type,codec_name,sample_rate,channels,nb_frames",
                        "-show_entries", "format=duration", "-of", "json", path], capture_output=True, text=True, check=True)
    return json.loads(r.stdout)


def deliver(local, dest):
    """Copy one file to the shared folder once, then read it back about ten seconds later and compare."""
    import hashlib
    import shutil
    import time
    shutil.copyfile(local, dest)
    time.sleep(10)
    a = hashlib.md5(open(local, "rb").read()).hexdigest()
    b = hashlib.md5(open(dest, "rb").read()).hexdigest()
    if a != b:      # (the shared folder adds a few kB of its own to a file: compare the streams, not the bytes)
        st = lambda p: subprocess.run(["ffmpeg", "-v", "error", "-i", p, "-map", "0", "-c", "copy", "-f", "md5", "-"],
                                      capture_output=True, text=True).stdout.strip()
        return bool(st(local)) and st(local) == st(dest)
    return True


def mix_all(only=None, seed=7, send=True):
    data = json.load(open(EVENTS))
    os.makedirs(WORK, exist_ok=True)
    rng = np.random.default_rng(seed)
    report = {}
    for name, mv in data["movies"].items():
        if only and name not in only:
            continue
        video = PICS + name + ".mp4"
        if not os.path.exists(video):
            print("no movie:", video)
            continue
        pv = probe(video)
        vframes = next(int(s["nb_frames"]) for s in pv["streams"] if s["codec_type"] == "video")
        if vframes != mv["frames"]:
            print(f"  {name}: the movie has {vframes} frames, the plan {mv['frames']}: the mix follows the plan's clock")
        wav = os.path.join(WORK, name + "_sound.wav")
        info = mix_movie(name, mv, rng, wav)
        out = os.path.join(WORK, name + "_sound.mp4")
        mux(video, wav, out)
        po = probe(out)
        a = next(s for s in po["streams"] if s["codec_type"] == "audio")
        info.update(audio=f"{a['codec_name']} {a['sample_rate']} Hz {a['channels']} ch", seconds=float(po["format"]["duration"]),
                    events=len(mv["events"]))
        if send:
            info["delivered"] = deliver(out, PICS + name + "_sound.mp4")
        report[name] = info
        print(f"  {name}: {info}", flush=True)
    return report


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "events":
        export_events()
    elif args and args[0] == "mix":
        only = set(args[args.index("--only") + 1].split(",")) if "--only" in args else None
        seed = int(args[args.index("--seed") + 1]) if "--seed" in args else 7
        rep = mix_all(only, seed, send="--no-deliver" not in args)
        json.dump(rep, open(os.path.join(WORK, "mix_report.json"), "w"), indent=1)
    else:
        print(__doc__)
