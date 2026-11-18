"""soundtrack.py <clip's sound log .json> <sound pack's wav folder> <mix.json> <out.wav> [<video.mp4> <out.mp4>]

Mixes an air clip's sound track, and (given a video) lays it under the picture.

airshow.py writes the log beside a clip's frames: where the camera was on every frame, and every thing heard with its
name in the sound pack, when, and where it was on every frame. This reads that, the approved pack's recordings where
they lie, and the sound director's mix numbers (mix.json, spec battle-mix.md), and mixes as that spec says:

- the camera is the film's microphone: the listener stands between the camera and what it looks at, facing as it faces,
  so what is on the left of the picture is heard on the left;
- every sound falls away with distance by its family's law (full up to `full`, then 0.75 of the inverse-distance law,
  tapering to nothing over the last 40% before `silent`) and loses its top with distance;
- a blast farther than 150 m is heard after it is seen, by distance / 343 s;
- machines' own engine loops are bent by their speed toward or away (Doppler); a fighter going by within 300 m plays
  its own fly-past recording, set so its loudest moment falls when it is nearest;
- machines on the same loop within 80 m of each other share one, a little louder for each;
- guns are their burst recording laid end to end while they fire, no two bursts alike (pitch and level), the six
  loudest at once;
- a big moment (a machine going down, breaking up) dips everything else 4 dB for a moment;
- a limiter keeps the whole under -1 dB.

Nothing is made up here: every sound is a recording from the pack, and a name the pack does not hold is silent.
The tools and the repository carry names and numbers only, never a sound file.
"""
import json
import math
import os
import random
import subprocess
import sys
import wave

import numpy as np

SR = 48000
SOUND = 343.0

FAMILY = {"b17_drone_loop": "aircraft", "b24_drone_loop": "aircraft", "fighter_engine_loop": "aircraft", "plane_guns": "aircraft", "gunner_burst": "aircraft",
          "flak_burst_01": "explosions", "plane_explode": "explosions", "aircraft_break_up": "explosions", "plane_down": "aircraft",
          "fire_small_loop": "fire", "bullets_incoming_01": "small_arms", "bullets_incoming_02": "small_arms", "parachute_open": "voices"}
GUNS = ("plane_guns", "gunner_burst")


def family(name):
    return FAMILY.get(name, "aircraft" if name.endswith("_pass") or name.endswith("_loop") else "explosions")


def read(folder, name, cache={}):
    """A recording as mono floats at 48 kHz, or None if the pack does not hold it."""
    if name not in cache:
        path = os.path.join(folder, name + ".wav")
        if not os.path.exists(path):
            cache[name] = None
        else:
            with wave.open(path) as w:
                n, ch, width, rate = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
                raw = w.readframes(n)
            if width != 2:
                raise SystemExit("%s: %d-byte samples, only 16-bit read here" % (path, width))
            x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
            x = x.reshape(-1, ch).mean(axis=1)
            if rate != SR:
                x = np.interp(np.arange(0, len(x), rate / SR), np.arange(len(x)), x)
            cache[name] = x
    return cache[name]


def low_pass(x, hz):
    """The top taken off, gently (12 dB an octave above `hz`)."""
    if hz >= 18000.0 or len(x) < 16:
        return x
    spec = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1.0 / SR)
    return np.fft.irfft(spec / np.sqrt(1.0 + (f / hz) ** 4), len(x))


def pitched(x, factor):
    return x if abs(factor - 1.0) < 1e-4 else np.interp(np.arange(0.0, len(x) - 1.0, factor), np.arange(len(x)), x)


def db(v):
    return 10.0 ** (v / 20.0)


def mean_over(x, n):
    """The mean of x over n samples round each sample (by running sums: a plain convolution of this length is minutes)."""
    c = np.cumsum(np.concatenate(([0.0], x)))
    i = np.arange(len(x))
    a, b = np.maximum(i - n // 2, 0), np.minimum(i + n // 2, len(x))
    return (c[b] - c[a]) / np.maximum(b - a, 1)


def edges(x, a=0.004, b=0.03):
    """A short fade at both ends, so nothing starts or stops with a click."""
    x = x.copy()
    na, nb = min(int(a * SR), len(x) // 2), min(int(b * SR), len(x) // 2)
    if na:
        x[:na] *= np.linspace(0.0, 1.0, na)
    if nb:
        x[-nb:] *= np.linspace(1.0, 0.0, nb)
    return x


class Mix:
    def __init__(self, log, folder, rules):
        self.log, self.folder, self.rules = log, folder, rules
        self.T = float(log["seconds"])
        self.N = int(round(self.T * SR))
        self.t = np.arange(self.N) / SR
        cam = np.array(log["camera"], dtype=np.float64)
        self.ct = cam[:, 0]
        eye, look = cam[:, 1:4], cam[:, 4:7]
        to = look - eye
        far = np.linalg.norm(to, axis=1, keepdims=True)
        far[far < 1e-6] = 1e-6
        ahead = to / far
        # the microphone: six tenths of the way from the camera to what it looks at, never more than 120 m from it
        self.ears = eye + ahead * np.minimum(0.6 * far, 120.0)
        right = np.cross(ahead, np.array([0.0, 0.0, 1.0]))
        norm = np.linalg.norm(right, axis=1, keepdims=True)
        norm[norm < 1e-6] = 1.0
        self.right = right / norm
        self.buses = {k: np.zeros((self.N, 2)) for k in ("loops", "close", "big")}
        self.big_at, self.report = [], []
        self.dice = random.Random(1944)

    # -- where a sound is, to the microphone
    def heard(self, path):
        """Distance, side (-1 left .. +1 right) and speed away, at every sample."""
        p = np.array(path, dtype=np.float64)
        pt = p[:, 0]
        pos = np.stack([np.interp(self.ct, pt, p[:, k]) for k in (1, 2, 3)], axis=1)
        rel = pos - self.ears
        d = np.linalg.norm(rel, axis=1)
        side = np.einsum("ij,ij->i", rel, self.right) / np.maximum(d, 1.0)
        # (outside the moments its place was logged, it holds its last distance: no speed)
        away = np.gradient(d, self.ct) if len(self.ct) > 2 else np.zeros_like(d)
        away[(self.ct < pt[0]) | (self.ct > pt[-1])] = 0.0
        if len(away) > 4:
            away = np.convolve(np.pad(away, 1, mode="edge"), np.ones(3) / 3.0, mode="valid")
        return np.interp(self.t, self.ct, d), np.interp(self.t, self.ct, side), np.interp(self.t, self.ct, away)

    def level(self, fam, d):
        """The family's law of distance, as a gain."""
        law = self.rules["distance"][fam]
        full, silent, roll = law["full"], law["silent"], self.rules["distance"]["rolloff"]
        g = np.where(d > full, (np.maximum(d, full) / full) ** (-roll), 1.0)
        return g * np.clip((silent - d) / (0.4 * silent), 0.0, 1.0)

    def top(self, fam, d):
        pts = self.rules["distance"][fam]["lpf"]
        return float(np.exp(np.interp(d, [p[0] for p in pts], [math.log(p[1]) for p in pts])))

    def lay(self, bus, x, start, gain, side):
        """A mono sound into a bus from sample `start`, at a gain and a side that may each change sample by sample."""
        a, b = max(start, 0), min(start + len(x), self.N)
        if b <= a:
            return
        x = x[a - start:b - start]
        g = gain[a:b] if isinstance(gain, np.ndarray) else gain
        s = np.clip((side[a:b] if isinstance(side, np.ndarray) else side) * 0.8, -1.0, 1.0)
        ang = (s + 1.0) * (math.pi / 4.0)
        self.buses[bus][a:b, 0] += x * g * np.cos(ang)
        self.buses[bus][a:b, 1] += x * g * np.sin(ang)

    # -- the kinds of sound
    def loop(self, name, paths, t0, t1, gain_db, note=""):
        x = read(self.folder, name)
        if x is None:
            return self.report.append(dict(name=name, silent="not in the pack"))
        fam = family(name)
        ts = [p[0] for p in paths[0]]
        arrs = [np.array(p, dtype=np.float64) for p in paths]
        mid = np.stack([np.array(ts)] + [np.mean([np.interp(ts, a[:, 0], a[:, k]) for a in arrs], axis=0) for k in (1, 2, 3)], axis=1)     # (the middle of them)
        d, side, away = self.heard(mid)
        g = self.level(fam, d) * db(gain_db + min(1.5 * (len(paths) - 1), 4.5))
        x = low_pass(x, self.top(fam, float(np.median(d))))
        rate = SOUND / (SOUND + np.clip(away, -200.0, 200.0)) if self.rules["distance"][fam].get("doppler") else np.ones(self.N)
        phase = (self.dice.uniform(0, len(x)) + np.cumsum(rate)) % (len(x) - 1)
        i = phase.astype(np.int64)
        y = x[i] + (x[i + 1] - x[i]) * (phase - i)
        on = np.clip((self.t - t0) / 0.25, 0.0, 1.0) * np.clip(((t1 if t1 is not None else self.T + 1.0) - self.t) / 0.4, 0.0, 1.0)
        self.lay("loops", y, 0, g * on, side)
        self.report.append(dict(name=name, kind="loop", machines=len(paths), metres=[round(float(d.min())), round(float(d.max()))], db=round(20 * math.log10(max(float(g.max()), 1e-6)), 1), note=note))

    def shot(self, s, big=False):
        name = s["name"]
        x = read(self.folder, name)
        if x is None:
            return self.report.append(dict(name=name, silent="not in the pack"))
        fam = family(name)
        d, side, _ = self.heard(s["path"])
        i0 = min(max(int(s["t0"] * SR), 0), self.N - 1)
        d0 = float(d[i0])
        late = min(d0 / SOUND, self.rules["flashToBang"]["maxSeconds"]) if fam in self.rules["flashToBang"]["families"] and d0 > self.rules["flashToBang"]["from"] else 0.0
        var = self.rules["variety"]
        x = pitched(x, 1.0 + self.dice.uniform(-1, 1) * var["pitchPercent"].get(fam, 3) / 100.0)
        x = edges(low_pass(x, self.top(fam, d0)))
        g = float(self.level(fam, np.array([d0]))[0])
        if big:
            g = max(g, db(-12.0))                 # (a big moment always plays)
        g *= db(s.get("gain", 0.0) + self.dice.uniform(-1, 1) * var["levelDb"])
        start = int((s["t0"] + late) * SR)
        self.lay("big" if big else "close", x, start, g, float(side[min(max(start, 0), self.N - 1)]))
        if big:
            self.big_at.append(s["t0"] + late)
        self.report.append(dict(name=name, kind="big" if big else "shot", at=round(s["t0"] + late, 2), late=round(late, 2), metres=round(d0), db=round(20 * math.log10(max(g, 1e-6)), 1)))

    def fly_past(self, s):
        name = s["name"]
        x = read(self.folder, name)
        if x is None:
            return self.report.append(dict(name=name, silent="not in the pack"))
        d, side, _ = self.heard(s["path"])
        pt = [p[0] for p in s["path"]]
        a, b = max(int(pt[0] * SR), 0), min(int(pt[-1] * SR), self.N - 1)
        near = a + int(np.argmin(d[a:b + 1]))
        if d[near] > 300.0:
            return self.report.append(dict(name=name, silent="never within 300 m (%d m)" % d[near]))
        x = pitched(x, 1.0 + self.dice.uniform(-1, 1) * 0.02)
        loud = mean_over(x * x, int(0.25 * SR))
        start = near - int(np.argmax(loud))
        g = float(self.level("aircraft", np.array([d[near]]))[0]) * db(s.get("gain", 0.0) + self.dice.uniform(-1, 1) * 1.5)
        self.lay("close", edges(x, 0.05, 0.3), start, g, side)
        self.report.append(dict(name=name, kind="pass", nearest_at=round(near / SR, 2), metres=round(float(d[near])), db=round(20 * math.log10(max(g, 1e-6)), 1)))

    def bursts(self, guns):
        """Guns: each one's burst recording laid end to end while it fires. At most six are heard at once (the mix's
        limit for machine guns), the loudest, and of those at most four of one kind: so the bombers' many gunners do
        not shut the fighters' guns out."""
        take = []
        for s in guns:
            d, side, _ = self.heard(s["path"])
            a, b = max(int(s["t0"] * SR), 0), min(int(s["t1"] * SR), self.N)
            if b - a < int(0.05 * SR):
                continue
            g = self.level(family(s["name"]), d)
            take.append((float(g[a:b].max()), s, a, b, g, side, d))
        take.sort(key=lambda k: -k[0])
        busy = np.zeros(self.N, dtype=np.int32)
        kinds = {}
        limit = self.rules["limits"]["mg"]
        var = self.rules["variety"]
        for peak, s, a, b, g, side, d in take:
            own = kinds.setdefault(s["name"], np.zeros(self.N, dtype=np.int32))
            if busy[a:b].max() >= limit or own[a:b].max() >= 4:
                self.report.append(dict(name=s["name"], silent="more than %d guns at once" % limit, at=round(a / SR, 2)))
                continue
            busy[a:b] += 1
            own[a:b] += 1
            x0 = read(self.folder, s["name"])
            if x0 is None:
                self.report.append(dict(name=s["name"], silent="not in the pack"))
                continue
            x0 = low_pass(x0, self.top(family(s["name"]), float(np.median(d[a:b]))))
            at = a
            while at < b:
                x = pitched(x0, 1.0 + self.dice.uniform(-1, 1) * var["pitchPercent"]["mg"] / 100.0)
                x = edges(x[:b - at], 0.003, 0.04)
                self.lay("close", x, at, g * db(s.get("gain", 0.0) + self.dice.uniform(-1, 1) * var["levelDb"]), side)
                at += len(x)
            self.report.append(dict(name=s["name"], kind="burst", at=[round(a / SR, 2), round(b / SR, 2)], metres=[round(float(d[a:b].min())), round(float(d[a:b].max()))], db=round(20 * math.log10(max(peak, 1e-6)), 1)))

    def run(self):
        sounds = self.log["sounds"]
        loops = [s for s in sounds if s["kind"] == "loop"]
        # machines on the same loop within 80 m of each other share one
        groups = []
        for s in loops:
            mid = np.mean(np.array(s["path"])[:, 1:4], axis=0)
            for g in groups:
                if g["name"] == s["name"] and any(np.linalg.norm(mid - m) < 80.0 for m in g["mids"]) and abs(g["t0"] - s["t0"]) < 0.5:
                    g["paths"].append(s["path"])
                    g["mids"].append(mid)
                    break
            else:
                groups.append(dict(name=s["name"], paths=[s["path"]], mids=[mid], t0=s["t0"], t1=s["t1"], gain=s.get("gain", 0.0)))
        for g in groups:
            n = min(len(p) for p in g["paths"])
            self.loop(g["name"], [p[:n] for p in g["paths"]], g["t0"], g["t1"], g["gain"])
        self.bursts([s for s in sounds if s["kind"] == "burst"])
        bigs = set(self.rules["big"]["names"])
        for s in sounds:
            if s["kind"] == "shot":
                self.shot(s, big=s["name"] in bigs)
            elif s["kind"] == "pass":
                self.fly_past(s)
        # a big moment dips the rest
        duck = np.ones(self.N)
        r = self.rules["big"]
        for at in self.big_at:
            a, h, rel = r["attackMs"] / 1000.0, r["holdMs"] / 1000.0, r["releaseMs"] / 1000.0
            x = self.t - at
            dip = np.where(x < 0, 0.0, np.where(x < a, x / a, np.where(x < a + h, 1.0, np.clip(1.0 - (x - a - h) / rel, 0.0, 1.0))))
            duck = np.minimum(duck, 10.0 ** (r["duckDb"]["close"] * dip / 20.0))
        out = (self.buses["loops"] + self.buses["close"]) * duck[:, None] + self.buses["big"]
        return self.master(out)

    def master(self, out):
        # brought to a film's level: the loudest half second of it at about -14 dB (never lifted more than 12 dB), then the limiter
        power = mean_over((out ** 2).mean(axis=1), int(0.5 * SR))
        loud = math.sqrt(max(float(power.max()), 1e-12))
        trim = min(db(-14.0) / loud, db(12.0))
        out = out * trim
        ceil = db(self.rules["buses"]["master"]["limiterCeilingDb"])
        peak = np.abs(out).max(axis=1)
        need = np.minimum(1.0, ceil / np.maximum(peak, 1e-9))
        look = int(0.005 * SR)                                   # (it sees 5 ms ahead, and lets go over the release time)
        ahead = np.minimum.reduce([np.roll(need, -k) for k in range(0, look, 16)])
        rel = math.exp(-1.0 / (self.rules["buses"]["master"]["limiterReleaseMs"] / 1000.0 * SR))
        g = np.empty(self.N)
        cur = 1.0
        for i in range(self.N):
            cur = ahead[i] if ahead[i] < cur else 1.0 - (1.0 - cur) * rel
            g[i] = cur
        out = out * g[:, None]
        fade = np.clip(self.t / 0.05, 0.0, 1.0) * np.clip((self.T - self.t) / 0.25, 0.0, 1.0)
        out = np.clip(out * fade[:, None], -ceil, ceil)
        self.trim_db = 20 * math.log10(trim)
        self.peak_db = 20 * math.log10(max(float(np.abs(out).max()), 1e-9))
        self.rms_db = 10 * math.log10(max(float((out ** 2).mean()), 1e-12))
        return out


def main():
    if len(sys.argv) < 5:
        raise SystemExit(__doc__)
    log_path, folder, rules_path, out_wav = sys.argv[1:5]
    with open(log_path) as f:
        log = json.load(f)
    with open(rules_path) as f:
        rules = json.load(f)
    m = Mix(log, folder, rules)
    out = m.run()
    with wave.open(out_wav, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((out * 32767.0).astype("<i2").tobytes())
    with open(os.path.splitext(out_wav)[0] + "-placed.json", "w") as f:
        json.dump(dict(seconds=m.T, trim_db=round(m.trim_db, 1), peak_db=round(m.peak_db, 1), rms_db=round(m.rms_db, 1), sounds=m.report), f, indent=1)
    heard = [r for r in m.report if "silent" not in r]
    print("mixed %s: %.1f s, %d sounds heard, %d silent, trim %+.1f dB, peak %.1f dB, rms %.1f dB" % (out_wav, m.T, len(heard), len(m.report) - len(heard), m.trim_db, m.peak_db, m.rms_db))
    if len(sys.argv) >= 7:
        video, out_mp4 = sys.argv[5:7]
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", video, "-i", out_wav, "-c:v", "copy", "-c:a", "aac", "-b:a", "192k", "-shortest", out_mp4], check=True)
        print("laid under", out_mp4)


if __name__ == "__main__":
    main()
