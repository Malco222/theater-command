#!/usr/bin/env python3
# lane_lift.py DIR : what the oil lane's paint adds to the sea, per screen column, in the stills lane_lift.sh drew
# (deck-lane-V and deck-lane-nl-V, the same scene with the strip unpainted), graded by game_curve.py show; holds the
# bounds of the look reviews of 27 November: seen low (760 m off 20 m up, 2 km off 30 m up, at 60 and 20 degree lenses)
# the lane is a lighter streak in every column, its median lift 12 to 16 levels at 760 m and at least 12 at 2 km, its
# brightest pixel never over the wake's foam (luma 190), never darker than the sea's darkest there; from straight
# above it is dark. Exit 1 on a bound missed.
import sys
import numpy as np
from PIL import Image

D = sys.argv[1]
Bad = 0


def load(n):
    return np.asarray(Image.open(f"{D}/deck-{n}-f0000.png").convert("RGB")).astype(int)


def luma(x):
    return 0.2126 * x[..., 0] + 0.7152 * x[..., 1] + 0.0722 * x[..., 2]


for V, Lo, Hi in (("760", 12, 16), ("760t", 12, 16), ("2k", 12, 99), ("2kt", 12, 99), ("top", -999, -20)):
    A, B = load("lane-" + V), load("lane-nl-" + V)
    d = (A - B).mean(axis=2)
    m = np.abs(d) >= 3
    cols = np.where(m.any(axis=0))[0]
    if len(cols) == 0:
        print(f"FAIL {V}: the lane adds nothing"); Bad += 1; continue
    peak = np.array([d[np.argmax(np.abs(d[:, c])), c] for c in cols])
    med = float(np.median(peak))
    bright = float(luma(A[m]).max())
    dark = float(luma(A[m]).min())
    seadark = float(luma(B[m]).min())
    ok = Lo <= med <= Hi and (V == "top" or (len(cols) == A.shape[1] and bright <= 190.0 and dark >= seadark - 3.0))
    Bad += 0 if ok else 1
    print(f"{'ok  ' if ok else 'FAIL'} {V}: in {len(cols)} of {A.shape[1]} columns, median lift {med:+.1f} levels (bound {Lo} to {Hi}), "
          f"brightest luma {bright:.0f} (foam 190), darkest {dark:.0f} on the sea's darkest {seadark:.0f}")
sys.exit(1 if Bad else 0)
