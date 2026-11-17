"""How each desk's loudness answers the marking: pp to ff should climb steadily."""
import sys, random, json, numpy as np
sys.path.insert(0, '/home/user/score')
import pyloudnorm as pyln
from engine import render_note, SR
from orchestra import build
T = build(); rng = random.Random(9); meter = pyln.Meter(SR)
desks = {'vln1': ('sus', 67), 'vla': ('sus', 60), 'vc': ('sus', 48), 'cb': ('sus', 36), 'hn': ('sus', 60), 'tpt': ('sus', 67),
         'tbn': ('sus', 50), 'tba': ('sus', 36), 'fl': ('sus', 77), 'ob': ('sus', 72), 'cl': ('sus', 62), 'bsn': ('sus', 46),
         'timp': ('hit', 46), 'snare': ('hit', 60), 'bd': ('hit', 60), 'vln1s': None}
for d, spec in desks.items():
    if not spec: continue
    for a in T[d].arts:
        k = spec[1]
        if not (T[d].arts[a].range[0] <= k <= T[d].arts[a].range[1]): k = (T[d].arts[a].range[0] + T[d].arts[a].range[1]) // 2
        row = []
        for v in (22, 34, 48, 62, 78, 96, 112, 124):
            ls = []
            for _ in range(2):
                y = render_note(T[d].arts[a], k, v, 1.2 if a not in ('spic', 'stac', 'hit', 'pizz', 'pluck') else 0.3, rng)
                y = np.concatenate([y, np.zeros((SR // 2, 2), np.float32)])
                try: ls.append(meter.integrated_loudness(y))
                except Exception: pass
            row.append(np.mean(ls))
        print(f'{d}.{a:6s}', ' '.join(f'{x:6.1f}' for x in row))
from engine import save_loudness; save_loudness()
