"""
The score's player: plays notes written in Python on real recorded orchestral instruments.

Every sound comes from a recording of a real player (the VS Chamber Orchestra 2: Community Edition
library, CC0, by Versilian Studios / Sam Gossner and Ivy Audio / Simon Dalzell). Nothing here makes a
tone from an oscillator. This file only chooses which recording to play for a note, shifts it to the
pitch asked for (never more than a few semitones from the recorded note), shapes its start and end, and
places it in a concert hall.
"""
import os, re, random, functools
import numpy as np
import soundfile as sf
import soxr
from scipy import signal

SR = 44100
LIB = os.environ.get('VSCO', '/home/user/lib/vsco')
SFZ = os.environ.get('VSCO_SFZ', '/home/user/lib/sfz')

# ---------------------------------------------------------------- the library on disk

@functools.lru_cache(maxsize=1)
def _pathmap():
    m = {}
    for root, _, files in os.walk(LIB):
        if '/.git' in root:
            continue
        for f in files:
            full = os.path.join(root, f)
            m[os.path.relpath(full, LIB).lower()] = full
    return m


def resolve(rel):
    p = _pathmap().get(rel.replace('\\', '/').lower())
    if p is None:
        raise FileNotFoundError(rel)
    return p


_LOUD = None
_LOUD_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sample_loudness.json')


def loudness_of(path, x, onset):
    """How loud a recording is as heard (LUFS over its first second and a half), measured once and kept."""
    global _LOUD
    import json
    if _LOUD is None:
        _LOUD = json.load(open(_LOUD_PATH)) if os.path.exists(_LOUD_PATH) else {}
    key = os.path.relpath(path, LIB)
    if key not in _LOUD:
        import pyloudnorm as pyln
        # the loudest second and a half: the attack of a struck drum, the top of a swell, the body of a held note
        w = int(1.5 * SR)
        e = (x[onset:, 0] ** 2 + x[onset:, 1] ** 2)
        if len(e) > w:
            c = np.concatenate([[0.0], np.cumsum(e, dtype=np.float64)])
            sums = c[w:] - c[:-w]
            best = int(np.argmax(sums[::256])) * 256
        else:
            best = 0
        seg = x[onset + best:onset + best + w]
        if len(seg) < int(0.6 * SR):
            seg = np.concatenate([seg, np.zeros((int(0.6 * SR) - len(seg), 2), np.float32)])
        try:
            v = pyln.Meter(SR).integrated_loudness(seg.astype(np.float64))
        except Exception:
            v = -70.0
        _LOUD[key] = float(v if np.isfinite(v) else -70.0)
        load.dirty = True
    return _LOUD[key]


def save_loudness():
    import json
    if getattr(load, 'dirty', False):
        json.dump(_LOUD, open(_LOUD_PATH, 'w'), indent=0, sort_keys=True)


@functools.lru_cache(maxsize=4096)
def load(path):
    """A recording as float32 stereo, and where its sound starts (the silence before it is skipped)."""
    x, sr = sf.read(path, dtype='float32', always_2d=True)
    if sr != SR:
        x = soxr.resample(x, sr, SR)
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    x = x[:, :2]
    a = np.abs(x).max(axis=1)
    peak = a.max() + 1e-12
    idx = np.argmax(a > peak * 10 ** (-32 / 20))
    onset = max(0, idx - int(0.004 * SR))
    return x, onset, loudness_of(path, x, onset)


# ---------------------------------------------------------------- SFZ maps

class Region:
    __slots__ = ('path', 'lokey', 'hikey', 'kc', 'lovel', 'hivel', 'vol', 'tune', 'seqlen', 'seqpos',
                 'lorand', 'hirand', 'release', 'attack')


def parse_sfz(fname):
    lines = open(os.path.join(SFZ, fname), encoding='latin-1').read().splitlines()
    default_path = ''
    level = None
    glob, grp, reg = {}, {}, None
    regions = []

    def flush():
        if reg is not None:
            d = {**glob, **grp, **reg}
            r = Region()
            r.path = resolve(default_path + d['sample'])
            r.lokey = int(d.get('lokey', 0)); r.hikey = int(d.get('hikey', 127))
            r.kc = int(d.get('pitch_keycenter', d.get('key', 60)))
            r.lovel = int(d.get('lovel', 0)); r.hivel = int(d.get('hivel', 127))
            r.vol = float(d.get('volume', 0)); r.tune = float(d.get('tune', 0))
            r.seqlen = int(d.get('seq_length', 1)); r.seqpos = int(d.get('seq_position', 1))
            r.lorand = float(d.get('lorand', 0)); r.hirand = float(d.get('hirand', 1))
            r.release = float(d.get('ampeg_release', 0.5)); r.attack = float(d.get('ampeg_attack', 0.001))
            regions.append(r)

    for raw in lines:
        line = raw.split('//')[0].strip()
        if not line:
            continue
        for tag in re.findall(r'<(\w+)>', line):
            if tag == 'region':
                flush(); reg = {}
            elif tag == 'group':
                flush(); reg = None; grp = {}
            elif tag == 'global':
                flush(); reg = None; glob = {}
            elif tag == 'control':
                flush(); reg = None
            level = tag
        line = re.sub(r'<\w+>', '', line).strip()
        pairs = [(line.split('=', 1)[0], line.split('=', 1)[1])] if line.startswith(('sample=', 'default_path=')) \
            else re.findall(r'(\w+)=(.*?)(?=\s+\w+=|$)', line)
        for k, v in pairs:
            k, v = k.strip(), v.strip()
            if k == 'default_path':
                default_path = v
            elif level == 'region':
                reg[k] = v
            elif level == 'group':
                grp[k] = v
            elif level == 'global':
                glob[k] = v
    flush()
    return regions


class Articulation:
    """One way of playing an instrument (held, short, tremolo...), from one or more SFZ maps."""

    def __init__(self, name, sfz, release=None, attack=None, lead=0.0, gain_db=0.0):
        self.name = name
        self.regions = parse_sfz(sfz)
        self.release = release
        self.attack = attack
        self.lead = lead          # seconds the sound needs to speak: the note is started this much early
        self.gain_db = gain_db
        self.target = -23.0
        self.counter = {}
        lo = min(r.lokey for r in self.regions); hi = max(r.hikey for r in self.regions)
        self.range = (lo, hi)

    def pick(self, key, vel, rng):
        cands = [r for r in self.regions if r.lokey <= key <= r.hikey and r.lovel <= vel <= r.hivel]
        if not cands:
            # outside the velocity map: take the nearest layer at this key
            at_key = [r for r in self.regions if r.lokey <= key <= r.hikey]
            if not at_key:
                raise ValueError(f'{self.name}: no sample for key {key} (range {self.range})')
            cands = sorted(at_key, key=lambda r: min(abs(vel - r.lovel), abs(vel - r.hivel)))[:1]
        seqlen = max(r.seqlen for r in cands)
        if seqlen > 1:
            k = (key, cands[0].lovel)
            n = self.counter.get(k, rng.randrange(seqlen)) % seqlen
            self.counter[k] = n + 1
            c2 = [r for r in cands if r.seqpos == n + 1] or cands
            cands = c2
        if len(cands) > 1 and any(r.hirand < 1 or r.lorand > 0 for r in cands):
            x = rng.random()
            c2 = [r for r in cands if r.lorand <= x < r.hirand] or cands
            cands = c2
        return rng.choice(cands)


# ---------------------------------------------------------------- one note

def _extend(y, n):
    """Holds a sustained recording longer than it was recorded, by crossfading its middle onto itself."""
    if len(y) >= n:
        return y[:n]
    a, b = int(len(y) * 0.30), int(len(y) * 0.80)
    if b - a < SR // 2:
        out = np.zeros((n, 2), np.float32); out[:len(y)] = y
        return out
    xf = min(int(0.35 * SR), (b - a) // 3)
    out = y[:b].copy()
    seg = y[a:b]
    fade_in = np.sin(np.linspace(0, np.pi / 2, xf))[:, None].astype(np.float32)
    fade_out = np.cos(np.linspace(0, np.pi / 2, xf))[:, None].astype(np.float32)
    while len(out) < n:
        tail = out[-xf:] * fade_out + seg[:xf] * fade_in
        out = np.concatenate([out[:-xf], tail, seg[xf:]])
    return out[:n]


def render_note(art, key, vel, dur, rng, detune=0.0, attack=None, release=None, curve=None, skip=0.0):
    r = art.pick(key, vel, rng)
    x, onset, loud = load(r.path)
    ratio = 2 ** ((key - r.kc + r.tune / 100 + detune / 100) / 12)
    rel = release if release is not None else (art.release if art.release is not None else r.release)
    rel = min(rel, 6.0)
    n_out = int((dur + rel) * SR) + 16
    start = onset + int(skip * SR)
    need = int(n_out * ratio) + 64
    seg = x[start:start + need]
    if abs(ratio - 1) > 1e-4:
        y = soxr.resample(seg, SR, SR / ratio, quality='HQ').astype(np.float32)
    else:
        y = seg.copy()
    y = _extend(y, n_out)
    # the envelope: an optional slower start, the held part, then a release
    env = np.ones(n_out, np.float32)
    att = attack if attack is not None else (art.attack if art.attack is not None else 0.0)
    att = max(att or 0.0, 0.003)   # never start a recording with a step: at least 3 ms to open
    na = min(int(att * SR), n_out)
    env[:na] = np.sin(np.linspace(0, np.pi / 2, na)) ** 2
    nd = int(dur * SR)
    nr = n_out - nd
    if nr > 0:
        env[nd:] *= np.cos(np.linspace(0, np.pi / 2, nr)) ** 2
    if curve:
        # curve: list of (fraction of the held part, dB); a swell or a fade inside one note
        ts = [int(f * nd) for f, _ in curve] + [n_out]
        ds = [d for _, d in curve] + [curve[-1][1]]
        g = np.interp(np.arange(n_out), ts, ds)
        env *= (10 ** (g / 20)).astype(np.float32)
    # loudness is set by the marking, the same for every recording: the strength the player was recorded at
    # only chooses the tone (a soft horn is round, a loud one brassy)
    gain = 10 ** ((art.target - loud + art.gain_db) / 20) * (max(vel, 1) / 78) ** 1.8
    return y * (env * gain)[:, None]


# ---------------------------------------------------------------- time

class Clock:
    """Beats to seconds, with tempo changes: tempo is a list of (beat, bpm); between two points the tempo
    glides when glide=True at the later point."""

    def __init__(self, bpm, changes=None):
        self.points = [(0.0, float(bpm), False)] + [(float(b), float(t), bool(g)) for b, t, g in (changes or [])]
        self.points.sort()

    def sec(self, beat):
        t = 0.0
        pts = self.points
        for i, (b0, bpm0, _) in enumerate(pts):
            b1 = pts[i + 1][0] if i + 1 < len(pts) else float('inf')
            if beat <= b0:
                break
            end = min(beat, b1)
            nxt = pts[i + 1] if i + 1 < len(pts) else None
            if nxt and nxt[2]:
                # tempo glides from bpm0 to nxt bpm across [b0, b1]: integrate 60/bpm(b)
                bpm1 = nxt[1]
                steps = 64
                bs = np.linspace(b0, end, steps + 1)
                frac = (bs - b0) / (b1 - b0)
                bp = bpm0 + (bpm1 - bpm0) * frac
                t += np.trapezoid(60.0 / bp, bs)
            else:
                t += (end - b0) * 60.0 / bpm0
            if beat <= b1:
                break
        return t


# ---------------------------------------------------------------- a section of players

class Track:
    """A desk of the orchestra (first violins, horns...). It holds notes and renders them as one stereo stem."""

    def __init__(self, name, arts, pan=0.0, width=0.7, send=0.25, depth=0.0, gain_db=0.0, players=1,
                 spread_cents=6.0, spread_ms=14.0, hp=None, lp=None):
        self.name = name
        self.arts = arts             # dict name -> Articulation
        self.pan, self.width, self.send, self.depth = pan, width, send, depth
        self.gain_db = gain_db
        self.players = players       # solo recordings doubled into a section: each player a different take
        self.spread_cents, self.spread_ms = spread_cents, spread_ms
        self.hp, self.lp = hp, lp
        self.notes = []

    def add(self, beat, dur_beats, key, vel, art='sus', **kw):
        self.notes.append((beat, dur_beats, key, vel, art, kw))


def note_number(name):
    """'Bb4' -> 70. Middle C is C4 = 60."""
    if isinstance(name, (int, np.integer)):
        return int(name)
    m = re.match(r'^([A-Ga-g])(#|b|##|bb)?(-?\d)$', name.strip())
    if not m:
        raise ValueError(name)
    step = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}[m.group(1).upper()]
    acc = {'#': 1, 'b': -1, '##': 2, 'bb': -2, None: 0}[m.group(2)]
    return 12 * (int(m.group(3)) + 1) + step + acc


def render_track(tr, clock, total_s, seed=0, humanize_ms=7.0, humanize_vel=4, wrap=None):
    """wrap: for a loop, its length in samples. A note that starts a hair before the first beat (a player
    early, or a sample's lead-in) has its first moments laid at the very end of the loop, where they belong
    when the loop comes round, instead of being cut off (a cut there is a click at the join)."""
    rng = random.Random(hash((tr.name, seed)) & 0xffffffff)
    n = int(total_s * SR)
    out = np.zeros((n, 2), np.float32)
    for (beat, dur_b, key, vel, artname, kw) in tr.notes:
        art = tr.arts[artname]
        key = note_number(key)
        t0 = clock.sec(beat)
        t1 = clock.sec(beat + dur_b)
        dur = max(0.03, t1 - t0 - kw.get('gap', 0.0))
        players = kw.get('players', tr.players)
        for p in range(players):
            v = int(np.clip(vel + rng.randint(-humanize_vel, humanize_vel), 1, 127))
            dt = rng.gauss(0, humanize_ms / 1000)
            det = kw.get('detune', 0.0)
            if players > 1:
                dt += (p - (players - 1) / 2) * tr.spread_ms / 1000 * rng.uniform(0.5, 1.0)
                det += rng.uniform(-tr.spread_cents, tr.spread_cents)
            start = t0 - art.lead + dt + kw.get('nudge', 0.0)
            y = render_note(art, key, v, dur + art.lead, rng, detune=det, attack=kw.get('attack'),
                            release=kw.get('release'), curve=kw.get('curve'), skip=kw.get('skip', 0.0))
            if players > 1:
                y = y / np.sqrt(players)
                # each player sits a little apart
                pp = (p - (players - 1) / 2) / max(1, players - 1) * 0.5
                y = _pan(y, pp, 0.8)
            i0 = int(start * SR)
            if i0 < 0:
                if wrap and -i0 < wrap:
                    out[wrap + i0:wrap] += y[:-i0]
                y = y[-i0:]; i0 = 0
            i1 = min(n, i0 + len(y))
            if i1 > i0:
                out[i0:i1] += y[:i1 - i0]
    return out


# ---------------------------------------------------------------- the hall

def _pan(x, pan, width):
    """Narrows a stereo recording, then places it: pan -1 left, +1 right (equal power)."""
    m = (x[:, 0] + x[:, 1]) * 0.5
    s = (x[:, 0] - x[:, 1]) * 0.5 * width
    l, r = m + s, m - s
    a = (pan + 1) * np.pi / 4
    gl, gr = np.cos(a) * np.sqrt(2), np.sin(a) * np.sqrt(2)
    return np.stack([l * gl, r * gr], axis=1).astype(np.float32)


def make_hall(rt60=2.5, predelay=0.024, seed=7, size=1.0):
    """An impulse response for a large hall, built the way a hall builds one: a scatter of early
    reflections, then a dense tail that dies away faster in the treble than the bass."""
    rng = np.random.default_rng(seed)
    n = int(SR * rt60 * 1.35)
    t = np.arange(n) / SR
    bands = [(0, 180, 1.25), (180, 700, 1.1), (700, 2500, 0.95), (2500, 6000, 0.7), (6000, 22050, 0.45)]
    ir = np.zeros((n, 2))
    freqs = np.fft.rfftfreq(n, 1 / SR)
    for ch in range(2):
        noise = rng.standard_normal(n)
        spec = np.fft.rfft(noise)
        acc = np.zeros(n)
        for lo, hi, k in bands:
            mask = (0.5 * (1 + np.tanh((freqs - lo) / (0.15 * lo + 20)))) * (0.5 * (1 - np.tanh((freqs - hi) / (0.15 * hi + 20))))
            band = np.fft.irfft(spec * mask, n)
            acc += band * np.exp(-6.91 * t / (rt60 * k))
        build = np.clip(t / (0.06 * size), 0, 1) ** 2
        acc *= build
        # early reflections: walls, floor, the stage
        er = np.zeros(n)
        for _ in range(18):
            d = rng.uniform(0.006, 0.085) * size
            g = 0.9 * np.exp(-d / 0.05) * rng.choice([-1, 1]) * rng.uniform(0.5, 1)
            er[int(d * SR)] += g
        er = signal.lfilter([0.5, 0.5], [1], er)
        acc = acc / np.sqrt(np.sum(acc ** 2)) + er * 0.35
        ir[:, ch] = acc
    pre = np.zeros((int(predelay * SR), 2))
    ir = np.concatenate([pre, ir])
    return (ir / np.sqrt(np.sum(ir ** 2) / 2)).astype(np.float32)


def shelf(x, f, db, kind='low'):
    """A gentle shelf, as on any mixing desk."""
    import pedalboard as pb
    board = pb.Pedalboard([pb.LowShelfFilter(cutoff_frequency_hz=f, gain_db=db) if kind == 'low'
                           else pb.HighShelfFilter(cutoff_frequency_hz=f, gain_db=db)])
    return board(x.T, SR).T.astype(np.float32)


def filt(x, hp=None, lp=None):
    import pedalboard as pb
    fx = []
    if hp: fx.append(pb.HighpassFilter(cutoff_frequency_hz=hp))
    if lp: fx.append(pb.LowpassFilter(cutoff_frequency_hz=lp))
    if not fx:
        return x
    return pb.Pedalboard(fx)(x.T, SR).T.astype(np.float32)


def mix(stems, tracks, hall, wet_db=-9.0):
    """Places each desk on the stage and in the hall. depth 0 is the front of the stage, 1 the back:
    the back desks are a little duller and send more of themselves to the hall."""
    n = max(len(s) for s in stems.values())
    dry = np.zeros((n, 2), np.float32)
    send = np.zeros((n, 2), np.float32)
    for tr in tracks:
        x = stems[tr.name]
        if tr.hp or tr.lp:
            x = filt(x, tr.hp, tr.lp)
        x = _pan(x, tr.pan, tr.width) * (10 ** (tr.gain_db / 20))
        if tr.depth > 0:
            x = shelf(x, 3500, -4.0 * tr.depth, 'high')
        dry[:len(x)] += x * (1.0 - 0.35 * tr.depth)
        send[:len(x)] += x * (tr.send + 0.25 * tr.depth)
    wet = np.stack([signal.oaconvolve(send[:, 0], hall[:, 0])[:n], signal.oaconvolve(send[:, 1], hall[:, 1])[:n]], axis=1)
    wet = filt(wet.astype(np.float32), hp=120)
    return dry + wet * (10 ** (wet_db / 20)), dry, wet


def limit(x, ceiling_db=-1.0, hold_ms=12.0):
    """A look-ahead peak limiter: turns the music down just enough, just before a peak, and lets it back up
    smoothly. Returns the limited audio and the most it ever turned the music down, in dB."""
    from scipy.ndimage import maximum_filter1d, uniform_filter1d
    c = 10 ** (ceiling_db / 20)
    a = np.abs(x).max(axis=1)
    need = np.clip(1.0 - c / np.maximum(a, 1e-9), 0, 1)
    L = int(hold_ms * SR / 1000)
    held = maximum_filter1d(need, size=2 * L + 1)
    smooth = uniform_filter1d(held, size=L + 1)
    smooth = uniform_filter1d(smooth, size=L + 1)
    g = 1.0 - smooth
    worst = float(-20 * np.log10(max(g.min(), 1e-6)))
    return (x * g[:, None]).astype(np.float32), worst


def master(x, target_lufs=-16.0, ceiling_db=-1.0, loop=False):
    """The finishing desk: a slow, gentle glue on the whole orchestra, then the level set by loudness the
    way broadcasters measure it, with the peaks kept under the ceiling.
    For a loop, the desk first hears the loop's own end before its start (and its start after its end), so
    the filter, the glue and the limiter are already settled at the join and nothing jumps there."""
    import pedalboard as pb
    import pyloudnorm as pyln
    board = pb.Pedalboard([
        pb.HighpassFilter(cutoff_frequency_hz=28),
        pb.Compressor(threshold_db=-22, ratio=1.6, attack_ms=60, release_ms=400),
    ])
    meter = pyln.Meter(SR)
    pre = post = 0
    if loop:
        pre, post = min(len(x), 10 * SR), min(len(x), int(0.5 * SR))
        x = np.concatenate([x[-pre:], x, x[:post]])
    y = board(x.T.astype(np.float32), SR).T
    y = y * 10 ** ((target_lufs - meter.integrated_loudness(y)) / 20)
    worst = 0.0
    for _ in range(3):
        y, w = limit(y, ceiling_db - 0.3)
        worst = max(worst, w)
        up = soxr.resample(y, SR, SR * 4)
        tp = 20 * np.log10(np.abs(up).max() + 1e-12)
        if tp <= ceiling_db:
            break
        y = y * 10 ** ((ceiling_db - tp - 0.05) / 20)
    master.last_reduction = worst
    if loop:
        y = y[pre:len(y) - post]
    return y.astype(np.float32), meter.integrated_loudness(y), tp


def fold_loop(x, n_loop):
    """The tail that rings on past the last bar is laid over the first bars, so the piece loops without a seam."""
    out = x[:n_loop].copy()
    tail = x[n_loop:]
    k = min(len(tail), n_loop)
    out[:k] += tail[:k]
    return out
