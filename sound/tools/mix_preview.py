#!/usr/bin/env python3
"""A one-minute battle mixed by the rules in /mnt/project-files/sound/mix/mix.json, from the approved pack only.

    python3 -I sound/tools/mix_preview.py <out-folder>

Writes battle-preview.wav (48 kHz stereo, limited at -1 dB), layers.json (each layer's level every 0.1 s, the
intensity meter, the music's level and the marked moments, for the picture) and events.json (every sound placed).

Nothing is made up: every sound is a pack recording. The mix only moves them: level by distance, a low-pass for air,
an echo for the place, a delay for sound's travel, panning, and ducking. The echo is a plain reverb (a decaying
impulse), the one processing step that is not a recording.

The scene: a British attack (the camera behind it, 20 m up) on a German line at a village edge and a wood about 300 m
off, with a Sherman on the left, a German tank and German guns far off. Contact at 16 s, the Germans break at 54 s,
the end at 60 s, the victory call after.
"""
import json, math, os, random, subprocess, sys
import numpy as np

SR = 48000
PACK = '/mnt/project-files/sound/battle-pack/'
MIX = json.load(open('/mnt/project-files/sound/mix/mix.json'))
MUSIC = '/mnt/project-files/music/game-wav/trench_line.wav'
OUT = sys.argv[1] if len(sys.argv) > 1 else 'mixprev'
os.makedirs(OUT, exist_ok=True)
T = 84.0                      # seconds of preview
MASTER_DB = 2.5               # brings the whole to about -18 LUFS for a laptop; the limiter at -1 dB takes the tops
N = int(T * SR)
CAM_H = 20.0                  # camera height above its focus point, metres
LIS = (0.0, 0.0, MIX['listener']['heightFactor'] * CAM_H)
ROLL = MIX['distance']['rolloff']   # 0.75 of the inverse-distance law: 4.5 dB a doubling (a game's gentler fall-off)
rng = random.Random(1944)
LIST = {x['name']: x for x in json.load(open(PACK + 'sound-list.json'))}

# ---------------------------------------------------------------- audio helpers
_cache = {}
def load(name, path=None):
    p = path or PACK + 'wav/' + name + '.wav'
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
    return np.fft.irfft(X * H[:, None], M, axis=0)[:n]

def interp_log(points, d):
    ds = [p[0] for p in points]; vs = [math.log(p[1]) for p in points]
    return math.exp(float(np.interp(d, ds, vs)))

def interp(points, d):
    return float(np.interp(d, [p[0] for p in points], [p[1] for p in points]))

def dist(x, y):
    return math.sqrt((x - LIS[0]) ** 2 + (y - LIS[1]) ** 2 + LIS[2] ** 2)

def gain_db(d, fam):
    F = MIX['distance'][fam]; full, silent = F['full'], F['silent']
    g = 0.0 if d <= full else -ROLL * 20 * math.log10(d / full)
    t = (d - 0.6 * silent) / (0.4 * silent)
    if t > 0:
        g += 20 * math.log10(max(1e-5, 1 - min(1.0, t) ** 2))
    return g

def pan(mono, x, y):
    az = math.degrees(math.atan2(x - LIS[0], y - LIS[1]))      # 0 ahead, + right
    az = max(-90.0, min(90.0, az if abs(az) <= 90 else math.copysign(180 - abs(az), az)))
    p = (az + 90) / 180 * math.pi / 2
    return np.stack([mono * math.cos(p), mono * math.sin(p)], axis=1) * math.sqrt(2) * 0.7071

# ---------------------------------------------------------------- the buses
BUS = {k: np.zeros((N, 2)) for k in ('bed', 'loops', 'close', 'voices', 'big')}
SEND = {k: np.zeros((N, 2)) for k in ('open', 'woods', 'buildings', 'water')}
EVENTS = []          # every one-shot placed, for the meter, the limits and the log
PLAYING = []         # (start, end, limit key, distance)

def put(bus, sig, t0):
    i = int(t0 * SR)
    if i >= N or i + len(sig) <= 0:
        return
    a, b = max(0, -i), min(len(sig), N - i)
    bus[i + a:i + b] += sig[a:b]

FAR_FROM = {k: MIX['distance'][k]['farTakesFrom'] for k in ('small_arms', 'mg', 'big_guns', 'explosions')}

def place(name, t, x, y, fam, bus='close', place_='open', extra_db=0.0, weight=None, limit=None, big=False):
    """One recording at a point: level, air, echo, travel time and side, by its distance from the listener."""
    d = dist(x, y)
    F = MIX['distance'][fam]
    g = gain_db(d, fam) + extra_db
    if g < -60:
        return None
    rec = LIST[name]
    # a far take keeps the close take's level at the switch, then falls off: the pack's far takes are already quieter
    if fam in FAR_FROM and name in FAR_TAKES:
        g += CLOSE_LUFS.get(fam, -16) - rec['lufs']
    if limit:
        live = [p for p in PLAYING if p[2] == limit and p[0] <= t < p[1]]
        if len(live) >= MIX['limits'][limit]:
            far = max(live, key=lambda p: p[3])
            if far[3] <= d:
                return None
            PLAYING.remove(far)          # the farthest gives way (its tail rings on; good enough for a preview)
    delay = 0.0
    fb = MIX['flashToBang']
    if fam in fb['families'] and d > fb['from']:
        delay = min(d / fb['metresPerSecond'], fb['maxSeconds'])
    src = load(name)
    mono = src.mean(axis=1)
    pv = MIX['variety']['pitchPercent'].get(fam, 0)
    if pv and not big:
        r = 1 + rng.uniform(-pv, pv) / 100            # resample for pitch (length changes with it, as tape does)
        idx = np.arange(0, len(mono) - 1, r)
        mono = np.interp(idx, np.arange(len(mono)), mono)
    g += rng.uniform(-MIX['variety']['levelDb'], MIX['variety']['levelDb']) if not big else 0
    fc = interp_log(F['lpf'], d)
    mono = lowpass(mono[:, None], fc)[:, 0] * db(g)
    st = pan(mono, x, y)
    put(BUS[bus], st, t + delay)
    send = interp(F['reverb'], d)
    put(SEND[place_], st * send, t + delay)
    end = t + delay + len(mono) / SR
    if limit:
        PLAYING.append((t + delay, end, limit, d))
    EVENTS.append({'t': round(t + delay, 3), 'name': name, 'fam': fam, 'd': round(d, 1), 'gainDb': round(g, 1),
                   'lpfHz': int(fc), 'bus': bus, 'weight': weight, 'big': big, 'end': round(end, 3)})
    return t + delay

def loop(name, t0, t1, path, fam, bus='loops', place_='open', extra_db=0.0, fade=1.5):
    """A loop from t0 to t1 that may move: path is [(t, (x, y)), ...]; level and side follow it every 50 ms."""
    src = load(name).mean(axis=1)
    n0, n1 = int(t0 * SR), min(N, int(t1 * SR))
    if n1 <= n0:
        return
    k = n1 - n0
    off = rng.randrange(len(src))
    mono = np.resize(np.roll(src, -off), k)
    ts = np.arange(n0, n1, SR // 20) / SR
    def at(tt):
        pts = [p for p in path]
        xs = np.interp(tt, [p[0] for p in pts], [p[1][0] for p in pts]); ys = np.interp(tt, [p[0] for p in pts], [p[1][1] for p in pts])
        return xs, ys
    xs, ys = at(ts)
    ds = [dist(a, b) for a, b in zip(xs, ys)]
    gs = np.array([db(gain_db(dd, fam) + extra_db) for dd in ds])
    azs = np.array([max(-90, min(90, math.degrees(math.atan2(a - LIS[0], b - LIS[1])))) for a, b in zip(xs, ys)])
    ps = (azs + 90) / 180 * math.pi / 2
    sl = np.arange(k) / SR + t0
    G = np.interp(sl, ts, gs); P = np.interp(sl, ts, ps)
    fc = interp_log(MIX['distance'][fam]['lpf'], float(np.mean(ds)))
    mono = lowpass(mono[:, None], fc)[:, 0]
    env = np.ones(k); fl = int(fade * SR)
    env[:fl] = np.linspace(0, 1, min(fl, k))[:min(fl, k)]
    env[-fl:] = np.minimum(env[-fl:], np.linspace(1, 0, min(fl, k))[:min(fl, k)])
    mono = mono * G * env
    st = np.stack([mono * np.cos(P), mono * np.sin(P)], axis=1)
    BUS[bus][n0:n1] += st
    send = interp(MIX['distance'][fam]['reverb'], float(np.mean(ds)))
    SEND[place_][n0:n1] += st * send

# ---------------------------------------------------------------- the scene
FAR_TAKES = {'rifle_shot_01', 'rifle_shot_02', 'rifle_shot_03', 'rifle_shot_04', 'bren_far_01', 'bren_far_02', 'bren_far_03',
             'far_guns_01', 'far_guns_02', 'far_guns_03', 'shell_impact_far', 'explosion_far_01', 'explosion_far_02'}
CLOSE_LUFS = {'small_arms': -16, 'mg': -16, 'big_guns': -16, 'explosions': -16}

def path_at(path, t):
    return (float(np.interp(t, [p[0] for p in path], [p[1][0] for p in path])),
            float(np.interp(t, [p[0] for p in path], [p[1][1] for p in path])))

BLOCKS = [
    dict(id='B1', nat='UK', kind='inf', w=60, dp=12, cover='open', fight=(16, 60), path=[(0, (-15, -70)), (16, (-15, 18))]),
    dict(id='B2', nat='UK', kind='inf', w=60, dp=12, cover='open', fight=(18, 60), path=[(0, (50, -60)), (18, (45, 38))]),
    dict(id='T1', nat='UK', kind='arm', w=8, dp=6, cover='open', fight=(22, 58), path=[(0, (-95, -90)), (22, (-80, 25))]),
    dict(id='G1', nat='GER', kind='inf', w=70, dp=15, cover='buildings', fight=(15.5, 54), waver=(46, 54), path=[(0, (-30, 320)), (54, (-30, 320)), (66, (-40, 520))]),
    dict(id='G2', nat='GER', kind='inf', w=70, dp=15, cover='woods', fight=(17, 55), waver=(48, 55), path=[(0, (90, 300)), (55, (90, 300)), (66, (100, 500))]),
    dict(id='G3', nat='GER', kind='arm', w=8, dp=6, cover='open', fight=(22, 47), path=[(0, (170, 345))]),
]
FIRE = MIX['fireModel']; NAT = MIX['byNation']

def take(lst, last={}):
    key = tuple(lst)
    pool = [x for x in lst if x != last.get(key)] or lst
    t = rng.choice(pool); last[key] = t
    return t

def nat_list(table, nat):
    v = NAT[table].get(nat, NAT[table]['*'])
    return v

def point_in(b, t):
    x, y = path_at(b['path'], t)
    return x + rng.uniform(-b['w'] / 2, b['w'] / 2), y + rng.uniform(-b['dp'] / 2, b['dp'] / 2)

def rate_mult(b, t):
    w = b.get('waver')
    return 0.6 if w and w[0] <= t < w[1] else 1.0

def weapons(b):
    if b['kind'] == 'inf':
        yield 'rifle', FIRE['inf']['rifle'], 'small_arms', nat_list('rifle', b['nat']), FIRE['inf']['rifle']['far']
        yield 'mg', FIRE['inf']['mg'], 'mg', nat_list('mg', b['nat']), FIRE['inf']['mg']['far']
        yield 'smg', FIRE['inf']['smg'], 'small_arms', nat_list('smg', b['nat']), []
    if b['kind'] == 'arm':
        yield 'gun', FIRE['arm']['gun'], 'big_guns', FIRE['arm']['gun']['close'], FIRE['arm']['gun']['far']
        yield 'mg', FIRE['arm']['mg'], 'mg', FIRE['arm']['mg']['close'], FIRE['arm']['mg']['far']

ENEMY = {'B1': 'G1', 'B2': 'G2', 'T1': 'G3', 'G1': 'B1', 'G2': 'B2', 'G3': 'T1'}
LIMIT_OF = {'small_arms': 'small_arms_close', 'mg': 'mg', 'big_guns': 'big_guns', 'explosions': 'explosions'}
for b in BLOCKS:
    f0, f1 = b['fight']
    for wkey, spec, fam, close, far in weapons(b):
        lo, hi = spec['every']
        t = f0 + rng.uniform(0, hi)
        while t < f1:
            x, y = point_in(b, t)
            d = dist(x, y)
            e = next(bb for bb in BLOCKS if bb['id'] == ENEMY[b['id']])
            ex, ey = path_at(e['path'], t)
            near_enemy = math.hypot(ex - x, ey - y)
            ok = not ('onlyWhenEnemyWithin' in spec and near_enemy > spec['onlyWhenEnemyWithin'])
            if ok:
                use_far = d > FAR_FROM[fam] and far
                name = take(far if use_far else close)
                lim = 'small_arms_far' if (fam == 'small_arms' and use_far) else LIMIT_OF[fam]
                tt = place(name, t, x, y, fam, place_=b['cover'], weight=MIX['intensity']['weights'][fam], limit=lim)
                if tt is not None and wkey == 'gun' and 'then' in spec and d < spec['then']['onlyCloserThan']:
                    place(take(spec['then']['takes']), t + rng.uniform(*spec['then']['after']), x, y, 'body', place_=b['cover'])
                # a tank's round lands on the tank it fights
                if wkey == 'gun':
                    ix, iy = ex + rng.uniform(-15, 15), ey + rng.uniform(-10, 10)
                    di = dist(ix, iy)
                    imp = take(['shell_impact_far', 'explosion_far_01', 'explosion_far_02']) if di > FAR_FROM['explosions'] else take(['shell_impact_near', 'shell_impact_near_02', 'shell_impact_near_03'])
                    place(imp, t + 0.4, ix, iy, 'explosions', place_=e['cover'], weight=MIX['intensity']['weights']['explosions'], limit='explosions')
            t += rng.uniform(lo, hi) / rate_mult(b, t)

# The Germans' rounds crack past the British block nearest the camera (it is within 40 m of the listener).
B1 = BLOCKS[0]
t = 17.0
while t < 54:
    x, y = point_in(B1, t)
    if dist(x, y) <= FIRE['incoming']['within'] + 10:
        place(take(FIRE['incoming']['takes']), t, x + rng.uniform(-6, 6), y - rng.uniform(4, 12), 'small_arms',
              weight=1, limit='small_arms_close')
    t += rng.uniform(*FIRE['incoming']['every']) / (0.6 if t > 46 else 1)

# German guns 2.6 km off fire on the British line; their shells land 2 to 5 s later. One lands 12 m off at 36 s.
GUN = (300, 2600)
for t in (23.0, 29.5, 33.0, 40.5, 44.0):
    place(take(FIRE['art']['gun']['far']), t, *GUN, 'big_guns', weight=4, limit='big_guns')
    ix, iy = rng.uniform(-90, 90), rng.uniform(40, 160)
    di = dist(ix, iy)
    near = di <= FAR_FROM['explosions']
    name = take(FIRE['art']['gun']['impact']['close'] if near else FIRE['art']['gun']['impact']['far'])
    place(name, t + rng.uniform(2, 5), ix, iy, 'explosions', weight=6, limit='explosions', big=near and di < MIX['big']['alsoWhenCloserThan'].get(name, 0))
SHOCK_T = 36.0
place('shell_impact_near_02', SHOCK_T, 8, 9, 'explosions', bus='big', weight=10, big=True)

# The German tank brews up at 47 s (a big moment, about 400 m off).
place('tank_knocked_out_02', 47.2, 170, 345, 'explosions', bus='big', weight=10, big=True)

# A grenade fight on the right at 41 s; a man falls near the camera at 31 s, another cries out at 43 s.
place('grenade_throw', 40.6, 30, 30, 'body', place_='open')
place('grenade_blast_02', 42.6, 58, 70, 'explosions', weight=6, limit='explosions')
place('body_fall_02', 31.0, -6, 10, 'body', extra_db=4)
place('body_fall_cry_02', 43.4, -2, 8, 'body', extra_db=4)

# Engines and boots (layer 2). The two marching blocks are within 80 m: one shared loop at their middle, +1.5 dB.
mid = [(0, (17, -65)), (17, (15, 28))]
loop('marching_loop', 0.0, 18.5, mid, 'engines', extra_db=1.5)   # a column's boots carry like an engine, not like one man's step
loop('tank_sherman_drive_loop', 0.0, 22.5, BLOCKS[2]['path'], 'engines')
loop('tank_idle_sherman_loop', 22.0, 64.0, [(0, (-80, 25))], 'engines', fade=2.0)
t = 6.0
while t < 17:
    x, y = point_in(B1, t)
    place(take(['kit_rattle_01', 'kit_rattle_02', 'kit_rattle_03']), t, x, y, 'body')
    t += rng.uniform(3, 8)

# Voices (layer 4): one British line at a time; the dialogue pulls the close layer down 3 dB while it speaks.
VOICE = []
def say(name, t, x, y):
    tt = place(name, t, x, y, 'voices', bus='voices', extra_db=MIX['buses']['voices']['gainDb'])
    if tt is not None:
        VOICE.append((tt, tt + LIST[name]['seconds']))
say('order_move', 9.5, -12, 4)
say('whistle_long', 16.0, -14, 14)
say('order_charge', 18.2, -10, 16)
for tv, nm in ((25.5, 'order_english_03'), (34.0, 'order_english_07'), (45.5, 'order_english_11')):
    say(nm, tv, rng.uniform(-20, 10), rng.uniform(10, 20))
say('victory_cry', 56.0, 0, 22)

# ---------------------------------------------------------------- the meter, the beds, the music
IM = MIX['intensity']
STEP = 0.25
times = np.arange(0, T, STEP)
heat_target = np.zeros(len(times))
for i, tt in enumerate(times):
    h = 0.0
    for e in EVENTS:
        if e['weight'] and tt - IM['window'] <= e['t'] <= tt:
            h += e['weight'] * max(0.0, 1 - e['d'] / IM['radius'])
    for b in BLOCKS:
        if b['fight'][0] <= tt < b['fight'][1] and not (b['id'] == 'G3' and tt >= 47):
            x, y = path_at(b['path'], tt)
            if dist(x, y) <= IM['blockRadius']:
                h += IM['perBlockFighting']
    heat_target[i] = min(1.0, h / IM['fullAt'])
meter = np.zeros(len(times))
for i in range(1, len(times)):
    tc = IM['rise'] if heat_target[i] > meter[i - 1] else IM['fall']
    meter[i] = meter[i - 1] + (heat_target[i] - meter[i - 1]) * (1 - math.exp(-STEP / tc))

CONTACT, END = 15.5, 60.0
def bed_gain(bd, m):
    if m <= bd['in'] or m >= bd['out']:
        return 0.0
    if m < bd['full']:
        return math.sin((m - bd['in']) / (bd['full'] - bd['in']) * math.pi / 2)
    if m <= bd['hold']:
        return 1.0
    return math.cos((m - bd['hold']) / (bd['out'] - bd['hold']) * math.pi / 2)

sl = np.arange(N) / SR
bed_curves = {}
for bd in MIX['beds']['list']:
    g = np.array([bed_gain(bd, m) for m in meter]) * db(bd['gainDb'])
    if bd['name'] == 'rifles_far_loop':      # before contact: only the far crackle, at -14 dB, after the first seconds
        g = np.where((times >= 4) & (times < CONTACT), db(-14), g)
    g = np.where(times >= END, 0, g)
    # never faster than 2 s: smooth the curve
    k = int(2.0 / STEP); g = np.convolve(np.pad(g, (k, k), mode='edge'), np.ones(k) / k, mode='same')[k:-k]
    G = np.interp(sl, times, g)
    src = load(bd['name'])
    off = rng.randrange(len(src))
    st = np.resize(np.roll(src, -off, axis=0), (N, 2))
    BUS['bed'] += st * G[:, None]
    SEND['open'] += st * G[:, None] * 0.05
    bed_curves[bd['name']] = g

# A far salvo off the field before contact, 2 to 2.5 km away: the war somewhere else (beforeContact).
place('far_guns_02', 3.0, -900, 2300, 'big_guns', weight=None)
place('far_guns_03', 11.5, 1400, 2000, 'big_guns', weight=None)

# Music: Brian's "Trench Line At Twilight", steered by the battle (mix.json "music").
MU = MIX['music']
mus = load(None, MUSIC)
mus = mus[:N] if len(mus) >= N else np.vstack([mus, np.zeros((N - len(mus), 2))])
MUSIC_FULL_DB = -20 - (-15.0)      # the song measures -15 LUFS; in battle it sits at -20 LUFS when full
STINGER_AT = END + MU['end']['battleSoundsFade'] + MU['end']['stingerAfter']
STINGER_LEN = LIST['victory']['seconds']
music_db = np.zeros(len(times))
level, i_thick, lull_from = 0.0, False, None
plan = []  # (time, target dB, fade) in order
plan.append((CONTACT, MU['onContact']['db'], MU['onContact']['fade']))
thick_at = next((tt for tt, m in zip(times, meter) if tt > CONTACT and m > MU['thickOfIt']['whenIntensityOver']), None)
if thick_at:
    plan.append((thick_at, MU['thickOfIt']['db'], MU['thickOfIt']['fade']))
plan.append((50.0, MU['nearBreak']['db'], MU['nearBreak']['fade']))
plan.append((54.0, MU['enemyRouting']['db'], MU['enemyRouting']['fade']))
plan.append((STINGER_AT, MU['end']['stingerDucksAllDb'], 0.3))
plan.append((STINGER_AT + STINGER_LEN + MU['end']['musicBackAfterStinger'], 0.0, 2.0))
plan.append((T - 5, -60, 5.0))
plan.sort(key=lambda p: p[0])
for i, tt in enumerate(times):
    act = [p for p in plan if p[0] <= tt]
    if act:
        p = act[-1]; prev = act[-2][1] if len(act) > 1 else 0.0
        frac = min(1.0, (tt - p[0]) / max(p[2], 1e-3))
        music_db[i] = prev + (p[1] - prev) * frac
    else:
        music_db[i] = 0.0
# big moments push the music, beds and the rest down for a moment
B = MIX['big']
duck = {k: np.zeros(len(times)) for k in ('bed', 'loops', 'close', 'music')}
big_marks = [e for e in EVENTS if e['big']]
for e in big_marks:
    for i, tt in enumerate(times):
        a, h, r = B['attackMs'] / 1000, B['holdMs'] / 1000, B['releaseMs'] / 1000
        if e['t'] <= tt < e['t'] + a + h + r:
            u = 1.0 if tt < e['t'] + a + h else 1 - (tt - e['t'] - a - h) / r
            for k in duck:
                duck[k][i] = min(duck[k][i], B['duckDb'][k] * u)
for i, tt in enumerate(times):                     # dialogue ducks the close layer
    if any(a <= tt < b for a, b in VOICE):
        duck['close'][i] = min(duck['close'][i], MIX['buses']['voices']['duckCloseDb'])
for k in ('bed', 'loops', 'close'):
    BUS[k] *= db(np.interp(sl, times, duck[k]))[:, None]
music_curve = music_db + duck['music']
MUSIC_BUS = mus * db(MUSIC_FULL_DB + np.interp(sl, times, music_curve))[:, None]

# ---------------------------------------------------------------- the places' echoes
def ir(decay, pre_ms, damping, lpf=None, slap=None, seconds=None):
    n = int((seconds or decay * 1.2) * SR)
    r = np.random.default_rng(7)
    noise = r.standard_normal((n, 2))
    env = np.exp(-6.9 * np.arange(n) / SR / decay)[:, None]
    h = noise * env
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
    X = np.fft.rfft(x, M, axis=0); H = np.fft.rfft(h, M, axis=0)
    return np.fft.irfft(X * H, M, axis=0)[:len(x)]

WET = np.zeros((N, 2))
for k, P in ((k, P) for k, P in MIX['places'].items() if not k.startswith('_')):
    if np.abs(SEND[k]).max() == 0:
        continue
    WET += convolve(SEND[k], ir(P['decaySeconds'], P['preDelayMs'], P['damping'], P.get('lpfHz'), P.get('slapbackMs')))

# ---------------------------------------------------------------- the end, the dull ears, the master
world = BUS['bed'] + BUS['loops'] + BUS['close'] + BUS['voices'] + BUS['big'] + WET * 1.0
fade_end = np.clip(1 - (sl - END) / MU['end']['battleSoundsFade'], 0, 1)
world *= fade_end[:, None]
S = MIX['shellShock']
shock_curve = np.zeros(N)
t0 = SHOCK_T
down, hold, back = S['downMs'] / 1000, S['holdMs'] / 1000, S['backMs'] / 1000
m = (sl >= t0) & (sl < t0 + down); shock_curve[m] = (sl[m] - t0) / down
m = (sl >= t0 + down) & (sl < t0 + down + hold); shock_curve[m] = 1
m = (sl >= t0 + down + hold) & (sl < t0 + down + hold + back); shock_curve[m] = 1 - (sl[m] - t0 - down - hold) / back
dull = lowpass(world, S['lpfHz'], order=3) * db(S['worldDb'])
world = world * (1 - shock_curve[:, None]) + dull * shock_curve[:, None]
stinger = load('victory')
stinger_bus = np.zeros((N, 2)); put(stinger_bus, stinger * db(4), STINGER_AT)
mix = world + MUSIC_BUS + stinger_bus
peak = np.abs(mix).max()
a, b_ = int(20 * SR), int(55 * SR)
dry = BUS['bed'] + BUS['loops'] + BUS['close'] + BUS['voices'] + BUS['big']
rms = lambda x: 20 * np.log10(np.sqrt((x ** 2).mean()) + 1e-12)
print('fight 20-55 s: dry', round(rms(dry[a:b_]), 1), 'echo', round(rms(WET[a:b_]), 1), 'music', round(rms(MUSIC_BUS[a:b_]), 1))
over = np.abs(mix * db(MASTER_DB)) > db(-1)
print('samples the limiter takes:', round(100 * over.any(axis=1).mean(), 3), '% of the time')
for (va, vb) in VOICE:
    i0, i1 = int(va * SR), int(vb * SR)
    print('voice %.1f s: voice %.1f dB, close+big %.1f dB, bed %.1f dB' % (va, rms(BUS['voices'][i0:i1]), rms(BUS['close'][i0:i1] + BUS['big'][i0:i1]), rms(BUS['bed'][i0:i1])))
raw = os.path.join(OUT, 'raw.f32')
mix.astype(np.float32).tofile(raw)
out = os.path.join(OUT, 'battle-preview.wav')
subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'f32le', '-ar', str(SR), '-ac', '2', '-i', raw,
                '-af', 'volume=' + str(MASTER_DB) + 'dB,alimiter=limit=0.891:attack=3:release=120:level=disabled', '-c:a', 'pcm_s24le', out], check=True)
os.remove(raw)

# ---------------------------------------------------------------- what the picture needs
def level(x):
    w = int(0.1 * SR)
    k = len(x) // w
    r = np.sqrt((x[:k * w] ** 2).mean(axis=1).reshape(k, w).mean(axis=1) + 1e-12)
    return [round(float(20 * np.log10(v)), 1) for v in r]
layers = {
    'step': 0.1,
    'bed': level(BUS['bed'] * fade_end[:, None]), 'loops': level(BUS['loops'] * fade_end[:, None]),
    'close': level((BUS['close'] + BUS['big']) * fade_end[:, None]), 'voices': level(BUS['voices']),
    'music': level(MUSIC_BUS + stinger_bus), 'all': level(mix),
    'meter': [round(float(v), 3) for v in np.interp(np.arange(0, T, 0.1), times, meter)],
    'musicDb': [round(float(v), 1) for v in np.interp(np.arange(0, T, 0.1), times, music_curve)],
    'marks': [
        {'t': 0, 'label': 'Moving up'}, {'t': 9.5, 'label': 'An order'}, {'t': CONTACT, 'label': 'Contact'},
        {'t': float(thick_at) if thick_at else None, 'label': 'The thick of it'}, {'t': SHOCK_T, 'label': 'A shell lands close'},
        {'t': 47.2, 'label': 'Their tank brews up'}, {'t': 50.0, 'label': 'They waver'}, {'t': 54.0, 'label': 'They break'},
        {'t': END, 'label': 'Battle over'}, {'t': STINGER_AT, 'label': 'Victory call'}],
    'peakBeforeLimiter': round(float(20 * np.log10(peak)), 1),
}
json.dump(layers, open(os.path.join(OUT, 'layers.json'), 'w'))
json.dump(EVENTS, open(os.path.join(OUT, 'events.json'), 'w'), indent=0)
fams = {}
for e in EVENTS:
    fams[e['fam']] = fams.get(e['fam'], 0) + 1
print('events', len(EVENTS), fams, 'thick at', thick_at, 'meter max', round(float(meter.max()), 2), 'peak dB', layers['peakBeforeLimiter'])
