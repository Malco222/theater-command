"""Measures every desk playing the same thing at the same marking (mf), so the desks can be set level with
one another before the music asks for any balance of its own."""
import sys, random, json, numpy as np
sys.path.insert(0, '/home/user/score')
import pyloudnorm as pyln
from engine import render_note, SR
from orchestra import build
T = build(); rng = random.Random(5); meter = pyln.Meter(SR)
plan = {'vln1': ('sus', [62, 67, 72, 76, 79]), 'vla': ('sus', [55, 60, 64, 67]), 'vc': ('sus', [43, 48, 53, 57, 60]),
        'cb': ('sus', [31, 36, 41, 43]), 'hn': ('sus', [53, 58, 62, 65, 70]), 'tpt': ('sus', [62, 65, 70, 74]),
        'tbn': ('sus', [46, 50, 53, 58]), 'tba': ('sus', [31, 34, 38, 41]), 'fl': ('sus', [74, 77, 82, 86]),
        'ob': ('sus', [67, 70, 74, 77]), 'cl': ('sus', [58, 62, 65, 70]), 'bsn': ('sus', [41, 46, 50, 53]),
        'harp': ('pluck', [55, 62, 67, 74]), 'timp': ('hit', [41, 43, 46, 48]), 'pno': ('hit', [55, 60, 67, 72]),
        'snare': ('hit', [60]), 'bd': ('hit', [60]), 'vln1_spic': None}
res = {}
for desk, spec in plan.items():
    if not spec: continue
    art, keys = spec
    for a in T[desk].arts:
        T[desk].arts[a].gain_db = 0.0
        vals = []
        for k in keys:
            if not (T[desk].arts[a].range[0] <= k <= T[desk].arts[a].range[1]): continue
            y = render_note(T[desk].arts[a], k, 78, 1.6 if a not in ('spic', 'stac') else 0.3, rng)
            y = np.concatenate([y, np.zeros((SR // 2, 2), np.float32)])
            try:
                vals.append(meter.integrated_loudness(y))
            except Exception:
                pass
        res[f'{desk}.{a}'] = round(float(np.mean(vals)), 1)
        print(f'{desk}.{a:6s} {res[f"{desk}.{a}"]:6.1f} LUFS')
json.dump(res, open('/home/user/score/calibration.json', 'w'), indent=1)
