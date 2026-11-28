#!/usr/bin/env python3
"""A stretch of a land battle's sound, put together off the Mac from the land_play test's list of takes.

The land_play test (Tools/land_play) plays a recorded battle through the game's own sound core (Battle/LandSound) and,
with LAND_PLAY_TAKES_DUMP=<file>, writes what the core asked the scene to start: each frame's ear (E), each take (T)
and each bed (B). This reads that list and the recordings themselves (Sounds/Real) and lays them on a stereo track as
the scene's players would: each take from its place, at its level less its family's loss over the distance (the
scene's law, ALandBattle::FamilyOf: none to `full`, then falling evenly in decibels to -60 at `silent`), its top taken
off with distance (the family's lpf points) and by an echo's own filters, panned by where it stands against the
camera's facing, cut and faded as the take says; the beds under them; the master limiter over all.

It is a sketch for a person's ear, for hearing one change against another on the same battle, not the game's mixer:
no engines (the scene starts those), no shell shock, no ducking, no height or occlusion, a plain left-right pan. Nobody
at this desk can hear it: what it is for is Brian's ear, before a Mac build is spent.

  render_clip.py <takes dump> <mix.json> <Sounds/Real> <from s> <to s> <out.wav> [--no-beds] [--only-echoes]
                 [--mark <file>]   (--mark writes the clip's own list, a take a line: when it starts in the clip, its
                                    name, what it is (sound, crack, tail, echo), metres from the ear, dB at the ear,
                                    seconds it plays, its family)
"""
import argparse
import bisect
import json
import math
import os
import sys
import warnings

import numpy as np
from scipy.io import wavfile
from scipy.ndimage import minimum_filter1d
from scipy.signal import butter, sosfilt

RATE = 48000
_cache = {}


def recording(folder, name):
    """A recording as floats, (samples, channels); None where the pack has none of that name."""
    if name not in _cache:
        path = os.path.join(folder, name + ".wav")
        if not os.path.exists(path):
            _cache[name] = None
        else:
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                rate, data = wavfile.read(path)
            if rate != RATE:
                raise SystemExit(f"{path}: {rate} Hz, not {RATE}")
            data = data.astype(np.float32) / (32768.0 if data.dtype == np.int16 else 1.0)
            _cache[name] = data.reshape(len(data), -1)
    return _cache[name]


def loss_db(family, far):
    if not family:
        return 0.0
    full, silent = family.get("full", 25.0), family.get("silent", 700.0)
    if far <= full:
        return 0.0
    if far >= silent:
        return -120.0
    return -60.0 * (far - full) / max(1.0, silent - full)


def top_hz(family, far):
    points = (family or {}).get("lpf")
    if not points or len(points) < 2:
        return 20000.0
    (near, near_hz), (far_at, far_hz) = points[0], points[-1]
    share = min(1.0, max(0.0, (far - near) / max(1.0, far_at - near)))
    return math.exp(math.log(near_hz) + (math.log(far_hz) - math.log(near_hz)) * share)


def limit(track, ceiling_db=-1.5, release_ms=500.0, ahead_ms=1.0):
    """The master limiter (mix.json buses.master): never past the ceiling, back up over the release."""
    ceiling = 10.0 ** (ceiling_db / 20.0)
    peak = np.max(np.abs(track), axis=1)
    want = np.minimum(1.0, ceiling / np.maximum(peak, 1e-9))
    if want.min() >= 1.0:
        return track, 0.0
    ahead = max(1, int(RATE * ahead_ms / 1000.0))
    want = minimum_filter1d(want, size=2 * ahead + 1, mode="nearest")
    step = 1.0 - math.exp(-1.0 / (RATE * release_ms / 1000.0))
    gain = np.empty_like(want)
    now = 1.0
    for k, w in enumerate(want.tolist()):
        now = w if w < now else now + (1.0 - now) * step
        gain[k] = now
    return track * gain[:, None], 20.0 * math.log10(gain.min())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dump")
    ap.add_argument("mix")
    ap.add_argument("sounds")
    ap.add_argument("start", type=float)
    ap.add_argument("end", type=float)
    ap.add_argument("out")
    ap.add_argument("--no-beds", action="store_true")
    ap.add_argument("--only-echoes", action="store_true")
    ap.add_argument("--mark")
    args = ap.parse_args()

    mix = json.load(open(args.mix, encoding="utf-8"))
    families = {k: v for k, v in mix["distance"].items() if isinstance(v, dict)}
    buses = {k: (v.get("gainDb") or 0.0) for k, v in mix["buses"].items() if isinstance(v, dict)}

    frames, ears, takes, beds = [], [], [], {}
    for line in open(args.dump, encoding="utf-8"):
        p = line.split()
        if not p:
            continue
        if p[0] == "E":
            frames.append(float(p[1]))
            eye, look, ear = [float(v) for v in p[3:6]], [float(v) for v in p[6:9]], [float(v) for v in p[9:12]]
            ears.append((ear, (look[0] - eye[0], look[1] - eye[1])))
        elif p[0] == "T":
            takes.append(dict(at=float(p[1]), name=p[2], pos=[float(v) for v in p[3:6]], family=p[6], bus=p[7], db=float(p[8]), pitch=float(p[9]),
                              frm=float(p[10]), cut=float(p[11]), cutfade=float(p[12]), fadein=float(p[13]), spatial=p[14] == "1",
                              lpf=float(p[15]), hpf=float(p[16]), echo=p[17] == "1"))
        elif p[0] == "B":
            beds.setdefault(p[2], []).append((float(p[1]), float(p[3])))

    length = int((args.end - args.start) * RATE)
    track = np.zeros((length, 2), dtype=np.float64)
    marks = []
    missing = set()
    for t in takes:
        if t["at"] >= args.end or t["at"] < args.start - 12.0:
            continue
        if args.only_echoes and not t["echo"]:
            continue
        data = recording(args.sounds, t["name"])
        if data is None:
            missing.add(t["name"])
            continue
        piece = data[int(t["frm"] * RATE):]
        if len(piece) < 8:
            continue
        if abs(t["pitch"] - 1.0) > 1e-4:
            # (the player's pitch: the recording run faster or slower)
            where = np.arange(0.0, len(piece) - 1, t["pitch"])
            piece = np.stack([np.interp(where, np.arange(len(piece)), piece[:, c]) for c in range(piece.shape[1])], axis=1)
        if t["cut"] > 0.0:
            ends = int((t["cut"] + t["cutfade"]) * RATE)
            piece = piece[:ends].copy()
            fade = min(len(piece), int(t["cutfade"] * RATE))
            if fade > 0:
                piece[-fade:] *= np.linspace(1.0, 0.0, fade)[:, None]
        else:
            piece = piece.copy()
        rise = min(len(piece), int(t["fadein"] * RATE))
        if rise > 1:
            piece[:rise] *= np.linspace(0.0, 1.0, rise)[:, None]
        k = max(0, bisect.bisect_right(frames, t["at"]) - 1)
        ear, facing = ears[k]
        db = t["db"] + buses.get(t["bus"], 0.0)
        left = right = math.sqrt(0.5)
        far = 0.0
        if t["spatial"]:
            dx, dy, dz = t["pos"][0] - ear[0], t["pos"][1] - ear[1], t["pos"][2] - ear[2]
            far = math.sqrt(dx * dx + dy * dy + dz * dz)
            family = families.get(t["family"])
            db += loss_db(family, far)
            if db < -70.0:
                continue
            hz = top_hz(family, far)
            if t["lpf"] > 0.0:
                hz = min(hz, t["lpf"])
            mono = piece.mean(axis=1)
            if hz < 18000.0:
                mono = sosfilt(butter(2, hz, "lowpass", fs=RATE, output="sos"), mono)
            if t["hpf"] > 0.0:
                mono = sosfilt(butter(2, t["hpf"], "highpass", fs=RATE, output="sos"), mono)
            # (left on the screen is heard on the left: the camera's facing, the field's X east and Y north)
            flat = math.hypot(dx, dy)
            face = math.hypot(facing[0], facing[1])
            side = 0.0
            if flat > 2.0 and face > 1e-6:
                side = (dx * facing[1] - dy * facing[0]) / (flat * face)
            angle = (side * 0.85 + 1.0) * math.pi / 4.0
            left, right = math.cos(angle), math.sin(angle)
            piece = np.stack([mono * left, mono * right], axis=1)
        else:
            piece = piece if piece.shape[1] == 2 else np.repeat(piece, 2, axis=1) * math.sqrt(0.5)
        gain = 10.0 ** (db / 20.0)
        at = int(round((t["at"] - args.start) * RATE))
        a, b = max(0, at), min(length, at + len(piece))
        if b > a:
            track[a:b] += piece[a - at:b - at] * gain
            if t["at"] >= args.start:
                kind = "echo" if t["echo"] else "tail" if t["frm"] > 0.2 and t["cut"] == 0.0 and t["family"] == "mg" else "crack" if "crack" in t["name"] else "sound"
                marks.append((t["at"] - args.start, t["name"], kind, round(far), round(db, 1), round(len(piece) / RATE, 2), t["family"]))

    if not args.no_beds and not args.only_echoes:
        times = np.arange(length) / RATE + args.start
        for name, rows in beds.items():
            data = recording(args.sounds, name)
            if data is None:
                missing.add(name)
                continue
            at = np.array([r[0] for r in rows])
            level = np.array([10.0 ** ((r[1] + buses.get("bed", 0.0)) / 20.0) for r in rows])
            # (silent before its first frame and after its last, as the scene's bed players fade)
            gain = np.interp(times, at, level, left=0.0, right=0.0)
            if gain.max() <= 0.0:
                continue
            index = (np.arange(length) + int(args.start * RATE)) % len(data)
            loop = data[index]
            if loop.shape[1] == 1:
                loop = np.repeat(loop, 2, axis=1) * math.sqrt(0.5)
            track += loop * gain[:, None]

    track, most = limit(track)
    edge = min(length // 2, int(0.25 * RATE))
    track[:edge] *= np.linspace(0.0, 1.0, edge)[:, None]
    track[-edge:] *= np.linspace(1.0, 0.0, edge)[:, None]
    wavfile.write(args.out, RATE, (np.clip(track, -1.0, 1.0) * 32767.0).astype(np.int16))
    rms = 20.0 * math.log10(max(1e-9, float(np.sqrt(np.mean(track ** 2)))))
    print(f"{args.out}: {args.end - args.start:.1f} s, {len(marks)} takes started in it ({sum(1 for m in marks if m[2] == 'echo')} echoes, {sum(1 for m in marks if m[2] == 'tail')} tails), "
          f"level {rms:.1f} dB RMS, the limiter took off {abs(most):.1f} dB at the most" + (f"; no recording for: {', '.join(sorted(missing))}" if missing else ""))
    if args.mark:
        with open(args.mark, "w", encoding="utf-8") as out:
            for m in sorted(marks):
                out.write(f"{m[0]:.3f}\t{m[1]}\t{m[2]}\t{m[3]}\t{m[4]}\t{m[5]}\t{m[6]}\n")


if __name__ == "__main__":
    sys.exit(main())
