#!/usr/bin/env python3
"""Soundscapes: new battle beds and longer scenes, mixed from Brian's approved battle pack only.

    python3 -I sound/tools/soundscape.py <out-folder> [name ...]

Asked for on 19 November 2026 (the coordinator's list, item 4, Brian: "all 4"): "a second pass on battle sound"
with "richer battle soundscapes: distant artillery rumble, aircraft flybys, ship guns". Nothing here is made up:
every sound is a recording from /mnt/project-files/sound/battle-pack/ (Brian approved them 18 Nov). The mixer only
moves them, as the battle mix's rules do (mix/battle-mix.md, mix/mix.json):
- level by distance, a low-pass that closes with distance (air takes the top off far sounds);
- sound's travel: a far gun is heard after its flash, and a moving engine changes pitch as it comes and goes
  (Doppler, the same physics the 3D engine applies; here at 60 % strength so a recording is never pushed far
  from its own pitch);
- a side, and an echo for the place (the plain decaying reverb mix_preview.py uses, the one step that is not a
  recording).

What it writes (each with a .json listing every sound placed and the marks for the listening page):
- beds that loop without a join, for the game to play under a battle, like battle_far_loop:
  front_rumble_loop  a front's guns 3 to 14 km off, rolling like thunder
  naval_guns_far_loop  a naval action 6 to 18 km off across water: salvos and their echoes
- scenes, for Brian's ear (the game makes them itself from the rules; these show how it should sound):
  scene_horizon  "The guns on the horizon": waiting behind the line, a battery firing, a column passing
  scene_bombers  "Bombers overhead": a raid passes over a gun site, escorts, flak, one bomber falls
  scene_sea      "Gun duel at sea": on a cruiser's deck, her salvos, the enemy's falling closer, a hit
  scene_strike   "Air support": two fighter-bombers strafe and bomb a line during a fight
"""
import json, math, os, random, subprocess, sys
import numpy as np

SR = 48000
PACK = '/mnt/project-files/sound/battle-pack/'
MIX = json.load(open('/mnt/project-files/sound/mix/mix.json'))
LIST = {x['name']: x for x in json.load(open(PACK + 'sound-list.json'))}
C = 343.0
DOPPLER = 0.6

# Far beyond the battlefield: the game's families stop at 4 to 15 km. A bed of the horizon needs its own curve
# (offered for mix.json as distance.horizon): level falls gently, the top goes early, the echo grows.
FAMILIES = dict(MIX['distance'])
FAMILIES['horizon'] = {'full': 800, 'silent': 40000, 'lpf': [[800, 12000], [3000, 2600], [8000, 1100], [15000, 650], [30000, 400]],
                       'reverb': [[0, 0.15], [15000, 0.75]]}
FAMILIES['air_far'] = {'full': 300, 'silent': 14000, 'lpf': [[300, 20000], [2500, 6000], [6000, 2400], [12000, 1100]],
                       'reverb': [[0, 0.03], [12000, 0.35]]}

_cache = {}
def load(name):
    p = PACK + 'wav/' + name + '.wav'
    if p not in _cache:
        raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', p, '-f', 'f32le', '-ac', '2', '-ar', str(SR), '-'],
                             capture_output=True, check=True).stdout
        _cache[p] = np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).astype(np.float64)
    return _cache[p]

def db(x): return 10 ** (x / 20)

def lowpass(x, fc, order=2):
    if fc >= 19000 or len(x) == 0:
        return x
    n = len(x); M = 1 << int(math.ceil(math.log2(n + SR // 20)))
    X = np.fft.rfft(x, M, axis=0); f = np.fft.rfftfreq(M, 1 / SR)
    H = 1 / np.sqrt(1 + (f / fc) ** (2 * order))
    return np.fft.irfft(X * H[:, None] if X.ndim == 2 else X * H, M, axis=0)[:n]

def interp_log(points, d):
    return math.exp(float(np.interp(d, [p[0] for p in points], [math.log(p[1]) for p in points])))

def interp(points, d):
    return float(np.interp(d, [p[0] for p in points], [p[1] for p in points]))

def gain_db(d, fam):
    F = FAMILIES[fam]; full, silent = F['full'], F['silent']
    g = 0.0 if d <= full else -MIX['distance']['rolloff'] * 20 * math.log10(d / full)
    t = (d - 0.6 * silent) / (0.4 * silent)
    if t > 0:
        g += 20 * math.log10(max(1e-5, 1 - min(1.0, t) ** 2))
    return g

def ir(decay, pre_ms, damping, lpf=None, slap=None, seed=7):
    n = int(decay * 1.2 * SR)
    r = np.random.default_rng(seed)
    h = r.standard_normal((n, 2)) * np.exp(-6.9 * np.arange(n) / SR / decay)[:, None]
    h = lowpass(h, lpf or (12000 * (1 - damping) + 2500))
    h = np.vstack([np.zeros((int(pre_ms / 1000 * SR), 2)), h])
    if slap:
        for ms, gl in ((slap[0], 0.5), (slap[1], 0.3)):
            j = int(ms / 1000 * SR)
            if j < len(h):
                h[j] += [gl, gl * 0.8]
    return h / np.sqrt((h ** 2).sum(axis=0).mean())

def convolve(x, h):
    n = len(x) + len(h) - 1; M = 1 << int(math.ceil(math.log2(n)))
    return np.fft.irfft(np.fft.rfft(x, M, axis=0) * np.fft.rfft(h, M, axis=0), M, axis=0)[:len(x)]

PLACES = {k: v for k, v in MIX['places'].items() if not k.startswith('_')}
PLACES['sea_far'] = {'decaySeconds': 4.0, 'preDelayMs': 140, 'damping': 0.55, 'note': 'a gun far off over open water: a long roll'}
PLACES['horizon'] = {'decaySeconds': 3.2, 'preDelayMs': 120, 'damping': 0.6, 'note': 'guns beyond the hills: the roll of thunder'}


class Scene:
    """A listener at (0, 0, height); x to the right, y ahead, z up, in metres."""

    def __init__(self, name, seconds, seed, height=1.7, loop=False, title='', about=''):
        self.name, self.T, self.loop = name, float(seconds), loop
        self.pad = 12.0 if loop else 0.0
        self.N = int((self.T + self.pad) * SR)
        self.lis = (0.0, 0.0, height)
        self.rng = random.Random(seed)
        self.dry = np.zeros((self.N, 2))
        self.send = {k: np.zeros((self.N, 2)) for k in PLACES}
        self.events, self.marks = [], []
        self.title, self.about = title, about
        self.last = {}

    def dist(self, p):
        x, y, z = (p + (0.0,))[:3] if len(p) == 2 else p
        return math.sqrt((x - self.lis[0]) ** 2 + (y - self.lis[1]) ** 2 + (z - self.lis[2]) ** 2)

    def mark(self, t, label):
        self.marks.append({'t': round(t, 1), 'label': label})

    def take(self, names):
        key = tuple(names)
        pool = [n for n in names if n != self.last.get(key)] or list(names)
        n = self.rng.choice(pool); self.last[key] = n
        return n

    def _put(self, buf, sig, t0):
        i = int(round(t0 * SR))
        if self.loop:
            i %= int(self.T * SR)
        if i >= self.N or i + len(sig) <= 0:
            return
        a, b = max(0, -i), min(len(sig), self.N - i)
        buf[i + a:i + b] += sig[a:b]
        if self.loop and i + len(sig) > self.N:          # a long tail past the pad comes round again
            rest = sig[b:]
            buf[:min(len(rest), self.N)] += rest[:self.N]

    @staticmethod
    def _pan(mono, az_deg, width=0.0):
        az = max(-90.0, min(90.0, az_deg))
        p = (az + 90) / 180 * math.pi / 2
        st = np.stack([mono * math.cos(p), mono * math.sin(p)], axis=1) * math.sqrt(2) * 0.7071
        return st

    def az(self, p):
        x, y = p[0], p[1]
        a = math.degrees(math.atan2(x - self.lis[0], y - self.lis[1]))
        return a if abs(a) <= 90 else math.copysign(180 - abs(a), a)

    def one(self, name, t, p, fam, place='open', gain=0.0, start=0.0, length=None, stereo=False, fade_out=0.05,
            pitch=None, delay=True, label=None):
        """One recording at point p (x, y[, z]): level, air, echo, travel time, side by distance."""
        d = self.dist(p)
        F = FAMILIES[fam]
        g = gain_db(d, fam) + gain
        if g < -70:
            return None
        src = load(name)
        a = int(start * SR); b = len(src) if length is None else a + int(length * SR)
        sig = src[a:b].copy()
        if fade_out and length is not None:
            k = min(len(sig), int(fade_out * SR)); sig[-k:] *= np.linspace(1, 0, k)[:, None]
        pv = MIX['variety']['pitchPercent'].get(fam, 0) if pitch is None else pitch
        if pv:
            r = 1 + self.rng.uniform(-pv, pv) / 100
            idx = np.arange(0, len(sig) - 1, r)
            sig = np.stack([np.interp(idx, np.arange(len(sig)), sig[:, c]) for c in (0, 1)], axis=1)
        g += self.rng.uniform(-MIX['variety']['levelDb'], MIX['variety']['levelDb'])
        fc = interp_log(F['lpf'], d)
        if stereo:
            st = lowpass(sig, fc) * db(g)
        else:
            st = self._pan(lowpass(sig.mean(axis=1)[:, None], fc)[:, 0] * db(g), self.az(p))
        lag = d / C if delay else 0.0
        t1 = t + lag
        self._put(self.dry, st, t1)
        self._put(self.send[place], st * interp(F['reverb'], d), t1)
        self.events.append({'t': round(t1, 3), 'fired': round(t, 3), 'name': name, 'fam': fam, 'm': round(d), 'gainDb': round(g, 1),
                            'lpfHz': int(fc), 'place': place, 'label': label})
        return t1

    def bed(self, name, t0, t1, gain=0.0, fade=2.0, curve=None, offset=None, place=None, send=0.0):
        """A loop that surrounds the listener (not placed), from t0 to t1; curve = [(t, dB), ...] on top."""
        src = load(name)
        L = LIST[name]
        n0, n1 = int(t0 * SR), min(self.N, int(t1 * SR))
        k = n1 - n0
        off = self.rng.randrange(len(src)) if offset is None else int(offset * SR)
        sig = np.resize(np.roll(src, -off, axis=0), (k, 2))
        env = np.ones(k)
        fl = min(k, int(fade * SR))
        if fl and not self.loop:
            env[:fl] = np.minimum(env[:fl], np.linspace(0, 1, fl))
            env[-fl:] = np.minimum(env[-fl:], np.linspace(1, 0, fl))
        if curve:
            ts = np.arange(n0, n1) / SR
            env *= db(np.interp(ts, [c[0] for c in curve], [c[1] for c in curve]))
        sig = sig * env[:, None] * db(gain)
        self.dry[n0:n1] += sig
        if place and send:
            self.send[place][n0:n1] += sig * send
        self.events.append({'t': round(t0, 3), 'end': round(t1, 3), 'name': name, 'fam': 'bed', 'gainDb': gain})

    def moving(self, name, t0, t1, path, fam, place='open', gain=0.0, doppler=DOPPLER, fade=1.5, offset=None, label=None, curve=None):
        """A loop carried along path [(t, (x, y, z)), ...] from t0 to t1: level, air, side and pitch follow it
        every sample (sound leaves the source at t - distance/343: that delay, changing, is the Doppler)."""
        src = load(name).mean(axis=1)
        n0, n1 = int(t0 * SR), min(self.N, int(t1 * SR))
        k = n1 - n0
        if k <= 0:
            return
        ts = np.arange(n0, n1) / SR
        P = np.array([[np.interp(ts, [q[0] for q in path], [q[1][i] for q in path]) for i in range(3)]])[0]
        dx, dy, dz = P[0] - self.lis[0], P[1] - self.lis[1], P[2] - self.lis[2]
        d = np.sqrt(dx ** 2 + dy ** 2 + dz ** 2)
        te = ts - doppler * d / C                               # when the sound heard now left the source
        off = self.rng.randrange(len(src)) if offset is None else int(offset * SR)
        pos = ((te - t0) * SR + off) % len(src)
        mono = np.interp(pos, np.arange(len(src)), src)
        # the air: a bank of low-passes, crossfaded by distance
        F = FAMILIES[fam]
        fcs = [20000, 10000, 6000, 3500, 2000, 1200, 700, 400, 250]
        bank = [lowpass(mono, f) for f in fcs]
        want = np.array([interp_log(F['lpf'], float(v)) for v in d[::480]])
        want = np.interp(np.arange(k), np.arange(0, k, 480)[:len(want)], want)
        lf = np.log(np.clip(want, fcs[-1], fcs[0]))
        lfs = np.log(fcs)
        out = np.zeros(k)
        for i in range(len(fcs) - 1):
            hi, lo = lfs[i], lfs[i + 1]
            m = (lf <= hi) & (lf >= lo)
            w = (lf[m] - lo) / (hi - lo)
            out[m] += bank[i][m] * w + bank[i + 1][m] * (1 - w)
        gs = np.array([db(gain_db(float(v), fam) + gain) for v in d[::480]])
        G = np.interp(np.arange(k), np.arange(0, k, 480)[:len(gs)], gs)
        env = np.ones(k); fl = min(k, int(fade * SR))
        env[:fl] = np.linspace(0, 1, fl); env[-fl:] = np.minimum(env[-fl:], np.linspace(1, 0, fl))
        if curve:          # the mix's own hand on top of distance: [(t, dB), ...]
            env *= db(np.interp(ts, [c[0] for c in curve], [c[1] for c in curve]))
        out *= G * env
        az = np.degrees(np.arctan2(dx, dy))
        az = np.where(np.abs(az) <= 90, az, np.sign(az) * (180 - np.abs(az)))
        pp = (np.clip(az, -90, 90) + 90) / 180 * math.pi / 2
        st = np.stack([out * np.cos(pp), out * np.sin(pp)], axis=1)
        self.dry[n0:n1] += st
        rv = np.array([interp(F['reverb'], float(v)) for v in d[::480]])
        self.send[place][n0:n1] += st * np.interp(np.arange(k), np.arange(0, k, 480)[:len(rv)], rv)[:, None]
        closest = int(np.argmin(d))
        self.events.append({'t': round(t0, 3), 'end': round(t1, 3), 'name': name, 'fam': fam, 'moving': True,
                            'closestM': round(float(d[closest])), 'closestAt': round(float(ts[closest]), 2),
                            'pitchMax': round(float(np.max(np.gradient(te) * SR)), 3), 'pitchMin': round(float(np.min(np.gradient(te) * SR)), 3),
                            'label': label})

    def render(self, out, lufs, limit_db=-1.0, wet=1.0):
        wetbus = np.zeros_like(self.dry)
        for k, P in PLACES.items():
            if np.abs(self.send[k]).max() == 0:
                continue
            h = ir(P['decaySeconds'], P['preDelayMs'], P['damping'], P.get('lpfHz'), P.get('slapbackMs'))
            if self.loop:          # the echo of the loop's end rings into its start
                L = int(self.T * SR)
                folded = self.send[k][:L].copy(); folded[:self.N - L] += self.send[k][L:]
                x = np.vstack([folded] * 2)
                y = convolve(x, h)[int(self.T * SR):]
                y = np.vstack([y, np.zeros((self.N - len(y), 2))])
                wetbus += y
            else:
                wetbus += convolve(self.send[k], h)
        mix = self.dry + wetbus * wet
        if self.loop:
            L = int(self.T * SR)
            mix[:self.N - L] += mix[L:]
            mix = mix[:L]
        # loudness to target, then the limiter at limit_db
        import pyloudnorm as pyln
        L0 = pyln.Meter(SR).integrated_loudness(mix)
        mix *= db(lufs - L0)
        wav = os.path.join(out, self.name + '.wav')
        lim = db(limit_db)
        if self.loop:
            # a loop never goes through the limiter: a limiter starts cold at the file's start and its look-ahead
            # shifts the sound, either of which leaves a step at the join. The beds peak well under the limit.
            if np.abs(mix).max() > lim:
                mix *= lim / np.abs(mix).max()
            import soundfile as sf
            sf.write(wav, mix, SR, subtype='PCM_24')
        else:
            raw = os.path.join(out, self.name + '.f32')
            mix.astype(np.float32).tofile(raw)
            subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'f32le', '-ar', str(SR), '-ac', '2', '-i', raw,
                            '-af', f'alimiter=limit={lim:.4f}:attack=3:release=120:level=disabled:latency=1',
                            '-c:a', 'pcm_s24le', wav], check=True)
            os.remove(raw)
        info = measure(wav, self.loop)
        info.update({'name': self.name, 'title': self.title, 'about': self.about, 'loop': self.loop,
                     'seconds': round(self.T, 2), 'marks': sorted(self.marks, key=lambda m: m['t']),
                     'sounds': sorted({e['name'] for e in self.events}), 'events': self.events,
                     'echoUnderDryDb': round(rms(wetbus * wet) - rms(self.dry), 1)})
        json.dump(info, open(os.path.join(out, self.name + '.json'), 'w'), indent=1)
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', wav, '-af', 'alimiter=limit=0.891:level=disabled',
                        '-codec:a', 'libmp3lame', '-b:a', '192k', os.path.join(out, self.name + '.mp3')], check=True)
        return info


def rms(x):
    return 20 * math.log10(math.sqrt(float((x ** 2).mean())) + 1e-12)


def measure(path, loop):
    """What can be measured without ears: loudness, its range, the peak, every second's level, and for a loop
    the join (the last 50 ms against the first, and the sharpest jump across it against the jumps around it)."""
    import soundfile as sf, pyloudnorm as pyln
    from scipy import signal
    x, sr = sf.read(path, dtype='float64')
    meter = pyln.Meter(sr)
    m = x.mean(axis=1)
    sec = [round(rms(x[i:i + sr]), 1) for i in range(0, len(x) - sr + 1, sr)]
    out = {'lufs': round(meter.integrated_loudness(x), 1), 'peakDb': round(20 * math.log10(np.abs(x).max()), 1),
           'perSecondDb': sec}
    # loudness range: the spread of 3 s windows (EBU style: 10th to 95th percentile of gated short-term)
    st = []
    for i in range(0, len(x) - 3 * sr, sr):
        v = meter.integrated_loudness(x[i:i + 3 * sr]) if np.abs(x[i:i + 3 * sr]).max() > 1e-4 else -70
        if v > -70:
            st.append(v)
    st = np.array(st); st = st[st > st.max() - 20] if len(st) else st
    out['rangeLU'] = round(float(np.percentile(st, 95) - np.percentile(st, 10)), 1) if len(st) else 0
    # the low end: how much of the energy is under 200 Hz (a rumble lives there; a laptop speaker loses it)
    f, P = signal.welch(m, sr, nperseg=8192)
    out['under200HzPercent'] = round(100 * float(P[f < 200].sum() / P.sum()), 1)
    out['under100HzPercent'] = round(100 * float(P[f < 100].sum() / P.sum()), 1)
    # what a laptop's speakers lose: loudness after a 200 Hz high-pass (4th order), against the whole
    hp200 = signal.sosfilt(signal.butter(4, 200, 'hp', fs=sr, output='sos'), x, axis=0)
    out['laptopLossDb'] = round(meter.integrated_loudness(x) - meter.integrated_loudness(hp200), 1)
    if loop:
        j = np.concatenate([x[-2 * sr:], x[:2 * sr]]).mean(axis=1)
        hp = signal.sosfilt(signal.butter(4, 9000, 'hp', fs=sr, output='sos'), np.concatenate([x[-4 * sr:-2 * sr].mean(axis=1), j]))[2 * sr:]
        env = np.sqrt(np.maximum(signal.convolve(hp ** 2, np.ones(2205) / 2205, mode='same'), 0)) + 1e-6
        ratio = np.abs(hp) / env
        near = slice(2 * sr - int(0.05 * sr), 2 * sr + int(0.05 * sr))
        w = int(0.05 * sr)
        out['join'] = {'sharpestJump': round(float(ratio[near].max()), 1), 'elsewhere': round(float(np.percentile(ratio, 99.99)), 1),
                       'endDb': round(rms(x[-w:]), 1), 'startDb': round(rms(x[:w]), 1),
                       'step': round(float(np.abs(x[0] - x[-1]).max()), 4)}
    return out


# ================================================================ the beds

def front_rumble(out):
    """A front's guns 3 to 14 km off: batteries firing and shells landing, many at once, so it rolls like thunder
    and never stops. 32 s, loops. Played under a battle before contact and in its lulls, and under the map at war
    if the 3D game wants it there."""
    S = Scene('front_rumble_loop', 32.0, seed=1916, loop=True, title='The front, far off',
              about='Batteries firing and shells landing 3 to 14 km away, rolling like thunder. Loops (32 s).')
    guns = ['far_guns_01', 'far_guns_02', 'far_guns_03', 'artillery_fire_01', 'field_gun_fire_02']
    blasts = ['explosion_far_01', 'explosion_far_02', 'shell_impact_far', 'shells_landing']
    t = 0.0
    while t < S.T:
        # guns: a battery's salvo is two to four guns 0.1 to 0.6 s apart, then a pause
        bx = S.rng.uniform(-9000, 9000); by = S.rng.uniform(3000, 14000)
        for _ in range(S.rng.randint(2, 4)):
            S.one(S.take(guns), t, (bx + S.rng.uniform(-150, 150), by + S.rng.uniform(-150, 150), 0), 'horizon',
                  place='horizon', delay=False, pitch=4)
            t += S.rng.uniform(0.1, 0.6)
        t += S.rng.uniform(0.4, 1.8)
    t = 0.7
    while t < S.T:
        x = S.rng.uniform(-8000, 8000); y = S.rng.uniform(4000, 12000)
        S.one(S.take(blasts), t, (x, y, 0), 'horizon', place='horizon', delay=False, gain=1.5, pitch=4)
        t += S.rng.uniform(0.5, 2.2)
    return S.render(out, lufs=-27.0)


def naval_far(out):
    """A naval action 6 to 18 km off across water: heavy salvos (several guns in one roll), cruisers' guns
    between, the rolling echo off the sea. 32 s, loops. For a land battle on a coast with ships firing, and for
    a sea battle beyond the one in view (under sea_battle_loop)."""
    S = Scene('naval_guns_far_loop', 32.0, seed=1942, loop=True, title='A sea fight, far off',
              about='Battleship and cruiser salvos 6 to 18 km off across water, with the sea\'s long echo. Loops (32 s).')
    t = 0.3
    while t < S.T:
        heavy = S.rng.random() < 0.45
        x = S.rng.uniform(-12000, 12000); y = S.rng.uniform(6000, 18000)
        names = ['ship_gun_heavy'] if heavy else ['ship_gun_medium', 'ship_guns']
        for _ in range(S.rng.randint(2, 4) if heavy else S.rng.randint(1, 3)):
            S.one(S.take(names), t, (x + S.rng.uniform(-200, 200), y, 0), 'horizon', place='sea_far', delay=False,
                  gain=2.0 if heavy else -1.0, pitch=3)
            t += S.rng.uniform(0.05, 0.35)
        t += S.rng.uniform(1.2, 3.6)
    return S.render(out, lufs=-26.0)


# ================================================================ the scenes

def scene_horizon(out, rumble_path):
    S = Scene('scene_horizon', 60.0, seed=1944, title='The guns on the horizon',
              about='Behind the line before a battle: the wind, the front rumbling far off (the new bed), a battery '
                    'firing 400 m to the left, its shells landing 4 km ahead, a column marching past and a lorry.')
    S.bed('wind_loop', 0, 60, gain=-4, fade=3)
    rumble = load_wav(rumble_path)
    S.dry[:len(S.dry)] += np.resize(rumble, S.dry.shape) * db(-3) * np.clip(np.minimum(np.arange(S.N) / (3 * SR), (S.N - np.arange(S.N)) / (3 * SR)), 0, 1)[:, None]
    S.mark(0.5, 'Wind; the front rumbling far off')
    # a battery of four field guns 400 m to the left firing every 9 to 14 s (close takes: inside farTakesFrom, 500 m);
    # their shells land 4 km ahead about 11 s later
    bat = (-370, 140, 0)
    t = 7.0
    first = True
    while t < 56:
        for g in range(4):
            S.one(S.take(['artillery_fire_01', 'field_gun_fire', 'field_gun_fire_02']), t + g * S.rng.uniform(0.25, 0.7),
                  (bat[0] + g * 20, bat[1], 0), 'big_guns', place='open', gain=8, label='battery' if first else None)
        if first:
            S.mark(t + 400 / C, 'A battery fires, 400 m to the left')
        for g in range(4):
            S.one(S.take(['explosion_far_01', 'explosion_far_02', 'shell_impact_far']), t + 11.5 + g * S.rng.uniform(0.2, 0.9),
                  (S.rng.uniform(-300, 600), S.rng.uniform(3800, 4400), 0), 'horizon', place='horizon', gain=8)
        if first:
            S.mark(t + 11.5 + 4000 / C if t + 11.5 + 4000 / C < 60 else 59, 'Its shells land 4 km ahead')
            first = False
        t += S.rng.uniform(9, 14)
    # a column marching past from behind-left to right, close (8 m), kit rattling; then a lorry
    S.moving('marching_loop', 14, 44, [(14, (-38, -12, 1)), (44, (34, -6, 1))], 'engines', gain=-3, doppler=0, label='column')
    S.moving('marching_loop_02', 16, 46, [(16, (-40, -14, 1)), (46, (32, -8, 1))], 'engines', gain=-6, doppler=0)
    S.mark(14, 'A column marches past')
    t = 20.0
    while t < 40:
        S.one(S.take(['kit_rattle_01', 'kit_rattle_02', 'kit_rattle_03']), t, (-36 + (t - 14) * 2.4, -9, 0), 'body', gain=4)
        t += S.rng.uniform(2.5, 6)
    S.one('order_english_05', 27.0, (-6, -10, 1.7), 'voices', gain=MIX['buses']['voices']['calmGainDb'])
    S.moving('lorry_drive_loop', 42, 58, [(42, (-140, -20, 1)), (51, (0, -14, 1)), (58, (120, -24, 1))], 'lorries', gain=-2, label='lorry')
    S.mark(48, 'A lorry passes behind')
    return S.render(out, lufs=-21.0)


def load_wav(p):
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', p, '-f', 'f32le', '-ac', '2', '-ar', str(SR), '-'], capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).astype(np.float64)


def scene_bombers(out, rumble_path):
    S = Scene('scene_bombers', 72.0, seed=1943, title='Bombers overhead',
              about='At a flak site under a daylight raid: the siren, a formation of B-17s 3 km up coming from ahead '
                    'and passing over, P-51 escorts crossing low, the flak guns firing and their bursts high above, '
                    'one bomber hit and falling away.')
    S.bed('wind_loop', 0, 72, gain=-8, fade=3)
    S.one('air_raid_siren', 0.0, (-900, -400, 20), 'horizon', place='horizon', delay=False, gain=0, pitch=0, length=26, fade_out=5)
    S.mark(0.0, 'The raid warning, from the town')
    # the formation: three groups of B-17s at 3,000 m, 80 m/s, heading from far ahead to far behind, a little right
    alt = 2200.0
    # the formation comes over the hills: the mix holds it down at first and lets it swell until it is overhead,
    # as a film does (in the game the formation would simply come from farther off)
    swell = [(0, -14), (20, -8), (36, -2), (50, 0), (64, -1), (72, -4)]
    for i, (dx, g) in enumerate(((150, 0), (-250, -2), (500, -3))):
        t_over = 44.0 + 6.0 * i                     # each group overhead 6 s after the one before
        path = [(0.0, (dx, 80.0 * t_over, alt)), (72.0, (dx, 80.0 * (t_over - 72.0), alt))]
        S.moving('b17_drone_loop', 1.0, 72.0, path, 'air_far', gain=g + 9, fade=6, label=f'B-17 group {i + 1}', place='horizon',
                 curve=swell)
        S.moving('bomber_heavy_loop', 1.0, 72.0, [(tt, (x + 60, y + 120, z)) for tt, (x, y, z) in path], 'air_far', gain=g + 5,
                 fade=6, place='horizon', curve=swell)
    S.mark(6, 'The formation, still 4 km off ahead')
    t_over = 44.0
    # escorts: P-51s crossing low from right to left, then back (their passes are recorded passes)
    S.one('p51_pass', 21.0 - 3.75, (300, 600, 700), 'aircraft', place='open', gain=4)        # its loudest moment at 3.75 s
    S.mark(21.0, 'P-51 escorts cross')
    S.one('p51_pass', 24.0 - 3.75, (-250, 800, 800), 'aircraft', place='open', gain=1)
    S.one('spitfire_pass', 52.0 - 8.0, (-200, 400, 600), 'aircraft', place='open', gain=2)     # loudest at 8.0 s
    S.mark(52.0, 'A fighter low overhead')
    # the flak site: four heavy guns 60 to 180 m around the listener, opening up at 25 s; bursts 3 km up, 7 to 10 s later
    guns = [(-60, 40, 0), (80, 70, 0), (-150, -90, 0), (170, -60, 0)]
    t = 25.0
    S.mark(25.0, 'The flak guns open up')
    while t < 62:
        g = S.rng.choice(guns)
        S.one('flak_heavy_fire_01', t, g, 'big_guns', place='open')
        bt = t + S.rng.uniform(7.0, 10.0)
        bx, by = S.rng.uniform(-900, 900), S.rng.uniform(-1500, 2500)
        S.one('flak_burst_01', bt, (bx, by, alt + S.rng.uniform(-200, 150)), 'air_far', place='horizon', gain=6, delay=True)
        t += S.rng.uniform(0.9, 2.4)
    for t in (30.0, 37.5, 44.0, 51.0):
        S.one('flak_light_fire', t, (40, -120, 0), 'big_guns', place='open', gain=-2)
    # the bomber hit: an explosion high up, then a plane going down far to the right
    S.one('plane_explode', 45.5, (900, 1200, alt), 'air_far', place='horizon', gain=8)
    S.one('plane_down', 46.5, (1600, 2600, 2000), 'air_far', place='horizon', gain=10)
    S.mark(45.5 + 3300 / C, 'A bomber is hit and falls away')
    S.mark(t_over, 'The formation overhead')
    S.one('parachute_open', 58.0, (1500, 2200, 1400), 'air_far', place='horizon', gain=6)
    return S.render(out, lufs=-19.0)


def scene_sea(out, naval_path):
    S = Scene('scene_sea', 70.0, seed=1942, height=12.0, title='Gun duel at sea',
              about='On a cruiser\'s deck in a night or day action: her engines and wash, her forward turrets firing salvos, '
                    'the enemy\'s guns flashing far off and their shells landing short, then over, then closer, a hit '
                    'astern and a fire, the bell, and a bigger fight beyond.')
    S.bed('sea_day_loop', 0, 70, gain=-12, fade=3)
    S.bed('ship_engine_loop', 0, 70, gain=-17, fade=3)
    S.bed('ship_wash_loop', 0, 70, gain=-14, fade=3)
    nav = load_wav(naval_path)
    ramp = np.clip((np.arange(S.N) / SR - 20) / 10, 0, 1)
    S.dry += np.resize(nav, S.dry.shape) * db(-5) * ramp[:, None]
    S.mark(0.5, 'Under way: engines, the wash, the sea')
    S.one('ship_bells_two', 3.0, (6, -20, 10), 'voices', place='water', gain=-2)
    S.mark(3.0, 'Action stations')
    # her own forward turrets: 20 to 30 m ahead of the listener on the bridge; a salvo every 11 to 14 s
    turrets = [(-3, 28, 8), (3, 22, 9)]
    t = 8.0
    first = True
    while t < 66:
        for i, tp in enumerate(turrets):
            S.one(S.take(['ship_gun_heavy', 'ship_gun_medium']), t + i * S.rng.uniform(0.08, 0.3), tp, 'ships', place='water',
                  gain=-2)
        if first:
            S.mark(t, 'Her salvo')
            first = False
        t += S.rng.uniform(11, 14)
    # the enemy: flashes 14 km off; their guns heard only as a far thud, their shells land short, over, then close
    falls = [(17.0, (-300, 900)), (29.0, (250, -700)), (41.0, (-120, 260)), (52.5, (60, -90))]
    for i, (tf, (x, y)) in enumerate(falls):
        S.one('ship_gun_heavy', tf - 3.0, (2000, 14000, 0), 'horizon', place='sea_far', delay=False, gain=4, pitch=3)
        for k in range(3):
            S.one('shell_splash', tf + k * S.rng.uniform(0.15, 0.5), (x + S.rng.uniform(-60, 60), y + S.rng.uniform(-60, 60), 0),
                  'ships', place='water', gain=4)
    for (tf, (x, y)), label in zip(falls, ('Enemy shells fall short', 'Over', 'Closer: straddled')):
        S.mark(tf + math.hypot(x, y) / C, label)
    # the hit at 52.5 s: a shell bursts aft; a fire takes
    S.one('explosion_debris', 52.9, (8, -45, 6), 'explosions', place='water', gain=2, pitch=0)
    S.one('shell_impact_near_03', 53.0, (6, -50, 6), 'explosions', place='water', gain=-2, pitch=0)
    S.mark(52.9, 'Hit aft')
    S.bed('fire_loop', 54.0, 70.0, gain=-18, fade=3)
    S.one('ship_bell', 57.0, (6, -20, 10), 'voices', place='water', gain=-4)
    S.mark(30.0, 'Another fight beyond (the new far bed)')
    return S.render(out, lufs=-19.0)


def scene_strike(out):
    S = Scene('scene_strike', 48.0, seed=1945, height=6.0, title='Air support',
              about='Brian\'s ground support ask, heard from behind our line: a fight going on ahead, two P-47s called '
                    'in, the first diving and strafing the enemy line 250 m ahead and dropping its bombs, the second '
                    'following, both climbing away; the fight picking up after.')
    S.bed('battle_far_loop', 0, 48, gain=-6, fade=2)
    S.bed('rifles_far_loop', 0, 48, gain=-8, fade=2)
    S.bed('wind_loop', 0, 48, gain=-12, fade=2)
    S.mark(0.5, 'A fight ahead')
    t = 1.0
    while t < 46:
        S.one(S.take(['mg42_burst_01', 'mg42_burst_02']), t, (S.rng.uniform(-120, 120), S.rng.uniform(240, 300), 0), 'mg', place='open')
        t += S.rng.uniform(2.5, 6.0)
    t = 1.5
    while t < 46:
        S.one(S.take(['rifle_shot_01', 'rifle_shot_02', 'rifle_shot_03', 'rifle_shot_04']), t, (S.rng.uniform(-100, 100), S.rng.uniform(200, 280), 0), 'small_arms', place='open')
        t += S.rng.uniform(0.6, 2.0)
    # the fighters: from behind the listener, low, over our heads, into a shallow dive on the line ahead
    for k, (t0, side) in enumerate(((9.0, -40), (16.5, 60))):
        # overhead at t0 + 1.0 (200 m/s, 250 m up), diving on the line 300 m ahead, bombs at t0 + 3.5, away by t0 + 12
        S.one('p47_pass', t0 + 1.0 - 4.25, (side, 0, 120), 'aircraft', place='open', gain=5 - 2 * k)   # loudest at 4.25 s
        S.moving('fighter_engine_loop', t0 - 5, t0 + 12, [(t0 - 5, (side, -1200, 700)), (t0 + 1.0, (side, 0, 120)),
                                                         (t0 + 2.2, (side, 180, 90)), (t0 + 3.5, (side, 330, 50)),
                                                         (t0 + 12, (side * 2, 1900, 900))],
                 'aircraft', gain=-6 - 2 * k, doppler=0.35, label='P-47 %d' % (k + 1))
        # its guns as it dives: three bursts walking along the line
        for b in range(3):
            S.one('plane_guns', t0 + 2.0 + b * 0.5, (side, 180 + b * 50, 90 - b * 15), 'air_guns', place='open', gain=5)
        S.one('bomb_release', t0 + 3.5, (side, 330, 50), 'aircraft', place='open', gain=2)
        for b in range(2):
            S.one(S.take(['shell_impact_near', 'shell_impact_near_03', 'explosion_debris']), t0 + 4.4 + b * 0.35,
                  (side + S.rng.uniform(-25, 25), 360 + b * 20, 0), 'explosions', place='open', gain=9, pitch=0)   # a bomb: 4 to 5 dB over a shell (mix.json impacts), +4 for the scene
        S.mark(t0 - 2, 'P-47 %s comes in over us' % ('one' if k == 0 else 'two'))
        S.mark(t0 + 2.0 + 200 / C, 'Strafing')
        S.mark(t0 + 4.4 + 360 / C, 'Bombs on the line')
    S.one('victory_cry', 30.0, (-15, 40, 1.7), 'voices', place='open', gain=2)
    S.mark(30.0, 'A cheer from our line')
    S.one('whistle_long', 34.0, (-10, 30, 1.7), 'voices', place='open')
    S.one('order_charge', 35.0, (-12, 34, 1.7), 'voices', place='open')
    S.mark(34.0, 'Up and forward')
    return S.render(out, lufs=-19.0)


if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'soundscapes'
    os.makedirs(out, exist_ok=True)
    want = set(sys.argv[2:])
    run = lambda n: not want or n in want
    res = {}
    if run('front_rumble_loop'):
        res['front_rumble_loop'] = front_rumble(out)
    if run('naval_guns_far_loop'):
        res['naval_guns_far_loop'] = naval_far(out)
    rumble = os.path.join(out, 'front_rumble_loop.wav'); naval = os.path.join(out, 'naval_guns_far_loop.wav')
    if run('scene_horizon'):
        res['scene_horizon'] = scene_horizon(out, rumble)
    if run('scene_bombers'):
        res['scene_bombers'] = scene_bombers(out, rumble)
    if run('scene_sea'):
        res['scene_sea'] = scene_sea(out, naval)
    if run('scene_strike'):
        res['scene_strike'] = scene_strike(out)
    for k, v in res.items():
        print(k, {a: v[a] for a in ('lufs', 'peakDb', 'rangeLU', 'under200HzPercent', 'echoUnderDryDb') if a in v}, v.get('join', ''))
