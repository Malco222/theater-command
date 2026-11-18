"""The sound for the art director's explosion test clip: a 105 mm salvo of three shells landing near a Panzer IV and a
rifle squad, one far (about 250 m), one in the middle (about 80 m) and one close (about 25 m from the camera's focus),
held 4 s after the last hit (art-director/explosions/explosions-look.md, "The test clip I need back").

Mixed from the approved battle pack by the rules in /mnt/project-files/sound/mix/mix.json (impacts, distance,
flashToBang, big, places), the same way sound/tools/mix_preview.py mixes the 84-second battle, so the clip's
picture can be laid to it: the flash times are fixed here and written to the .json beside the track.

    python3 -I sound/tools/salvo_track.py <out-dir>
"""
import json, math, os, random, subprocess, sys
import numpy as np

SR = 48000
PACK = '/mnt/project-files/sound/battle-pack/'
MIX = json.load(open('/mnt/project-files/sound/mix/mix.json'))
OUT = sys.argv[1] if len(sys.argv) > 1 else 'salvo'
os.makedirs(OUT, exist_ok=True)
LIST = {x['name']: x for x in json.load(open(PACK + 'sound-list.json'))}

T = 10.0
N = int(T * SR)
FPS = 30
CAM_H = 12.0                                   # the art director's reference camera: 12 m up, 15 degrees down, 28 mm
LIS = (0.0, 0.0, MIX['listener']['heightFactor'] * CAM_H)
ROLL = MIX['distance']['rolloff']
MASTER_DB = 4.0                                # about -21 LUFS: one close blast sets the peak; the limiter at -1 dB takes about 1 dB off its first crack
rng = random.Random(105)

def load(name):
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', PACK + 'wav/' + name + '.wav', '-f', 'f32le', '-ac', '2',
                          '-ar', str(SR), '-'], capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).astype(np.float64)

def db(x): return 10 ** (x / 20)

def lowpass(x, fc, order=2):
    if fc >= 19000 or len(x) == 0:
        return x
    n = len(x); M = 1 << int(math.ceil(math.log2(n + SR // 20)))
    X = np.fft.rfft(x, M, axis=0); f = np.fft.rfftfreq(M, 1 / SR)
    return np.fft.irfft(X * (1 / np.sqrt(1 + (f / fc) ** (2 * order)))[:, None], M, axis=0)[:n]

def interp_log(points, d):
    return math.exp(float(np.interp(d, [p[0] for p in points], [math.log(p[1]) for p in points])))

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
    az = math.degrees(math.atan2(x - LIS[0], y - LIS[1]))       # 0 ahead, + right
    az = max(-90.0, min(90.0, az if abs(az) <= 90 else math.copysign(180 - abs(az), az)))   # behind folds to the front
    p = (az + 90) / 180 * math.pi / 2
    return np.stack([mono * math.cos(p), mono * math.sin(p)], axis=1)

BUS = {k: np.zeros((N, 2)) for k in ('bed', 'loops', 'close', 'voices', 'big')}
SEND = np.zeros((N, 2))
LOG = []

def put(bus, sig, t0):
    i = int(t0 * SR)
    a, b = max(0, -i), min(len(sig), N - i)
    if b > a:
        bus[i + a:i + b] += sig[a:b]

FAR_TAKES = {'shell_impact_far', 'explosion_far_01', 'explosion_far_02'}

def place(name, t, x, y, fam, bus='close', extra_db=0.0, pitch=True, src=None, what='', part=None, fade_in=0.0, fade_out=0.0):
    d = dist(x, y)
    F = MIX['distance'][fam]
    g = gain_db(d, fam) + extra_db
    if name in FAR_TAKES:                      # a far take starts at the close takes' level at the switch
        g += -16 - LIST[name]['lufs']
    delay = 0.0
    fb = MIX['flashToBang']
    if fam in fb['families'] and d > fb['from']:
        delay = min(d / fb['metresPerSecond'], fb['maxSeconds'])
    mono = (load(name) if src is None else src).mean(axis=1)
    if part:                                   # only a stretch of the take, from part[0] to part[1] s
        mono = mono[int(part[0] * SR):int(part[1] * SR)].copy()
        if fade_in:
            k = int(fade_in * SR); mono[:k] *= np.linspace(0, 1, k)
        if fade_out:
            k = int(fade_out * SR); mono[-k:] *= np.linspace(1, 0, k)
    pv = MIX['variety']['pitchPercent'].get(fam, 0) if pitch else 0
    if pv:
        r = 1 + rng.uniform(-pv, pv) / 100
        mono = np.interp(np.arange(0, len(mono) - 1, r), np.arange(len(mono)), mono)
    fc = interp_log(F['lpf'], d)
    mono = lowpass(mono[:, None], fc)[:, 0] * db(g)
    st = pan(mono, x, y)
    put(BUS[bus], st, t + delay)
    put(SEND, st * interp(F['reverb'], d), t + delay)
    LOG.append({'what': what, 'sound': name + ('@%g-%g' % tuple(part) if part else ''), 'flash': round(t, 3), 'heard': round(t + delay, 3), 'metres': round(d, 1),
                'db': round(g, 1), 'lowPassHz': int(fc), 'bus': bus})
    return t + delay

def loop(name, x, y, fam, bus, extra_db=0.0, fade=0.3):
    src = load(name).mean(axis=1)
    off = rng.randrange(len(src))
    mono = np.resize(np.roll(src, -off), N)
    d = dist(x, y)
    extra_db += MIX['buses'][bus]['gainDb']
    mono = lowpass(mono[:, None], interp_log(MIX['distance'][fam]['lpf'], d))[:, 0] * db(gain_db(d, fam) + extra_db)
    env = np.ones(N); fl = int(fade * SR); env[:fl] = np.linspace(0, 1, fl)
    st = pan(mono * env, x, y)
    BUS[bus] += st
    SEND[:] += st * interp(MIX['distance'][fam]['reverb'], d)
    LOG.append({'what': 'loop', 'sound': name, 'metres': round(d, 1), 'db': round(gain_db(d, fam) + extra_db, 1), 'bus': bus})

def bed(name, gain):
    src = load(name)
    off = rng.randrange(len(src))
    BUS['bed'] += np.resize(np.roll(src, -off, axis=0), (N, 2)) * db(gain)
    LOG.append({'what': 'bed', 'sound': name, 'db': gain})

# ---------------------------------------------------------------- the scene
# The camera looks at a Panzer IV with a German rifle squad beside it. The air: a fight off in the distance.
I = MIX['impacts']
size = I['bySize']['howitzer_105']
bed('rifles_far_loop', MIX['beds']['list'][0]['gainDb'])
loop('tank_idle_loop', 0, 1, 'engines', 'loops')                     # the Panzer IV ticking over at the focus

# Where the fortifications thread's picture puts the shells (x right of the view line, y beyond the focus, metres).
SHELLS = [  # (flash at, x, y, band)
    (1.50, 45, 242, 'far'),
    (3.40, 8, 81, 'middle'),
    (5.00, 8, -23, 'near'),      # on the camera's side of the focus: right foreground
]
# The close hit is shell_impact_near_02, the punchiest close take (0.1 s after it starts it is 4 dB over the others),
# with its earth brought forward to land when the picture's does (impacts.debris): the blast plays to 2.3 s, and its
# own falling part (2.3 to 4.9 s, earth from 2.5 s) starts 0.7 s after the flash, so the clods land 0.9 to 3.0 s
# after it, thickest 1.4 to 2.9 s; the picture's land 0.9 to 3.1 s, thickest 1.5 to 2.5 s. The middle hit is
# shell_impact_near, the far one shell_impact_far.
TAKE = {'far': 'shell_impact_far', 'middle': 'shell_impact_near', 'near': 'shell_impact_near_02'}
DEB = I['debris']
heard = {}
for t, x, y, band in SHELLS:
    near = band == 'near' and dist(x, y) <= size['bigWithin']
    name = TAKE[band]
    cut = DEB['cutAt'].get(name) if near else None
    heard[band] = place(name, t, x, y, 'explosions', bus='big' if near else 'close', extra_db=size['db'],
                        pitch=not near, what='105 mm shell, ' + band,
                        part=(0, cut) if cut else None, fade_out=DEB['cutFade'] if cut else 0.0)
    if near and name in DEB['for']:
        place(DEB['take'], t + 0.7, x, y, 'explosions', bus='big', extra_db=size['db'] + DEB['db'], pitch=False,
              part=(DEB['from'], DEB['to']), fade_in=DEB['fadeIn'], what='earth coming down')

# The men start to drop as the far shell's bang reaches them (2.22 s) and are down by 3.32 s; the squad leader shouts.
# The fight is calm here (the meter well under 0.3), so the shout is not lifted (buses.voices.calmGainDb).
place('german_schnell', 2.45, -3, 4, 'voices', bus='voices', extra_db=MIX['buses']['voices']['calmGainDb'], what='squad leader')
for t, x, y in ((2.95, -5, 4), (3.05, 2, 6), (3.18, -1, 3), (3.30, 4, 5)):
    place(rng.choice(['kit_rattle_01', 'kit_rattle_02', 'kit_rattle_03']), t, x, y, 'body', what='a man drops')
# One of the three crouching by the tank is knocked flat by the close shell, down at about 5.6 s: part of the moment.
place('body_fall_04', 5.58, 3, -1, 'body', bus='big', extra_db=4, what='a man knocked flat')
place('kit_rattle_02', 5.62, 3, -1, 'body', bus='big', extra_db=2, what='his kit')

# ---------------------------------------------------------------- the big moment ducks the rest
B = MIX['big']
sl = np.arange(N) / SR
t0 = heard['near']
a, h, r = B['attackMs'] / 1000, B['holdMs'] / 1000, B['releaseMs'] / 1000
u = np.clip((sl - t0) / a, 0, 1) * np.where(sl < t0 + a + h, 1.0, np.clip(1 - (sl - t0 - a - h) / r, 0, 1))
for k in ('bed', 'loops', 'close'):
    BUS[k] *= db(B['duckDb'][k] * u)[:, None]

# ---------------------------------------------------------------- the open field's echo
P = MIX['places']['open']
n = int(P['decaySeconds'] * 1.2 * SR)
noise = np.random.default_rng(7).standard_normal((n, 2)) * np.exp(-6.9 * np.arange(n) / SR / P['decaySeconds'])[:, None]
hir = lowpass(noise, 12000 * (1 - P['damping']) + 2500)
hir = np.vstack([np.zeros((int(P['preDelayMs'] / 1000 * SR), 2)), hir]); hir /= np.sqrt((hir ** 2).sum(axis=0).mean())
M = 1 << int(math.ceil(math.log2(N + len(hir))))
WET = np.fft.irfft(np.fft.rfft(SEND, M, axis=0) * np.fft.rfft(hir, M, axis=0), M, axis=0)[:N]

mix = BUS['bed'] + BUS['loops'] + BUS['close'] + BUS['voices'] + BUS['big'] + WET
fo = int(0.6 * SR); mix[-fo:] *= np.linspace(1, 0, fo)[:, None]
raw = os.path.join(OUT, 'raw.f32'); mix.astype(np.float32).tofile(raw)
wav = os.path.join(OUT, 'salvo-105-test.wav')
subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'f32le', '-ar', str(SR), '-ac', '2', '-i', raw, '-af',
                'volume=%sdB,alimiter=limit=0.891:attack=3:release=120:level=disabled' % MASTER_DB, '-c:a', 'pcm_s24le', wav],
               check=True)
subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', wav, '-c:a', 'libmp3lame', '-b:a', '192k',
                os.path.join(OUT, 'salvo-105-test.mp3')], check=True)
os.remove(raw)
info = {
    'what': 'Sound for the 105 mm salvo test clip (art-director/explosions/explosions-look.md). Lay the picture to it: '
            'each shell\'s flash at its frame below (30 fps). Same sound for the dusk version.',
    'seconds': T, 'fps': FPS, 'cameraHeightM': CAM_H, 'listener': 'the camera focus, raised %.0f m' % LIS[2],
    'shells': [{'band': b, 'flashSeconds': t, 'flashFrame': round(t * FPS), 'x': x, 'y': y,
                'metresFromListener': round(dist(x, y), 1), 'bangSeconds': round(heard[b], 3), 'bangFrame': round(heard[b] * FPS)}
               for t, x, y, b in SHELLS],
    'squad': {'shout': 2.45, 'menDown': [2.95, 3.3], 'knockedFlat': 5.58, 'note': 'the men start down as the far bang reaches them (2.22 s) and are down by 3.32 s; the squad leader shouts "Schnell!"; one man by the tank is knocked flat by the close shell'},
    'earthComingDown': 'from 0.9 s after the close flash (5.9 s), thickest about 6.4 to 7.9 s, done by 8.0 s',
    'incoming': 'none: no real recording of a shell coming in yet (mix.json impacts.incoming)',
    'sounds': LOG,
}
json.dump(info, open(os.path.join(OUT, 'salvo-105-test.json'), 'w'), indent=1)
peak = 20 * np.log10(np.abs(mix).max())
print('peak before limiter %.1f dB' % peak)
for s in info['shells']:
    print(s)
