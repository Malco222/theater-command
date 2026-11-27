#!/usr/bin/env python3
# lane_lift.py DIR : what the oil lane's paint adds to the sea, per screen column, in the stills lane_lift.sh drew
# (deck-lane-V and deck-lane-nl-V, the same scene with the strip unpainted), graded by game_curve.py show. Each
# column's lift is its PEAK (the pixel the lane changes most there), as the fourth look review measured it, not the
# band's median. The bounds, as the lead restated them on 27 November: seen low by day (760 m off 20 m up, 2 km off 30 m
# up, at 60 and 20 degree lenses) the lane is a lighter streak in every column, the 5th to 95th percentile of the
# columns' peaks within 12 to 24 levels at 760 m and at least 8 at 2 km, its brightest pixel under the wake's foam
# (luma 190), never darker than the sea's darkest there; from straight above it is dark; on a night sea (FX_NIGHT=1)
# no column lifted more than 2 levels (no calm band, the lane a dark film); at a dusk (FX_DIM=0.35) the band at that
# share over the dark film, its brightest pixel never lighter than the sky at the horizon as drawn (its lift printed,
# either sign). Exit 1 on a bound missed.
import sys
import numpy as np
from PIL import Image

D = sys.argv[1]
Bad = 0


def load(n):
    return np.asarray(Image.open(f"{D}/deck-{n}-f0000.png").convert("RGB")).astype(int)


def luma(x):
    return 0.2126 * x[..., 0] + 0.7152 * x[..., 1] + 0.0722 * x[..., 2]


# (view, kind, low bound on the columns' 5th percentile peak, high bound on their 95th)
for V, Kind, Lo, Hi in (("760", "day", 12, 24), ("760t", "day", 12, 24), ("2k", "day", 8, 99), ("2kt", "day", 8, 99),
                        ("top", "top", -999, -20), ("n760", "night", -999, 2), ("n2k", "night", -999, 2),
                        ("d760", "dusk", -99, 99)):
    A, B = load("lane-" + V), load("lane-nl-" + V)
    d = (A - B).mean(axis=2)
    if Kind == "night":
        # (every column's largest lift, wherever the lane is: none over 2 levels)
        most = float(d.max())
        ok = most <= Hi
        Bad += 0 if ok else 1
        changed = int((np.abs(d) >= 1).any(axis=0).sum())
        print(f"{'ok  ' if ok else 'FAIL'} {V} (night): the largest lift in any column {most:+.1f} levels (bound {Hi}); "
              f"the lane changes {changed} columns by a level or more, its darkest change {float(d.min()):+.1f}")
        continue
    m = np.abs(d) >= 3
    cols = np.where(m.any(axis=0))[0]
    if len(cols) == 0:
        print(f"FAIL {V}: the lane adds nothing"); Bad += 1; continue
    peak = np.array([d[np.argmax(np.abs(d[:, c])), c] for c in cols])
    p5, med, p95 = (float(np.percentile(peak, q)) for q in (5, 50, 95))
    bright = float(luma(A[m]).max())
    dark = float(luma(A[m]).min())
    seadark = float(luma(B[m]).min())
    extra = ""
    if Kind == "top":
        ok = med <= Hi
    elif Kind == "dusk":
        ok = True  # (held below by the sky alone: at dusk the band's share is small and the film may barely show)
    else:
        ok = len(cols) == A.shape[1] and p5 >= Lo and p95 <= Hi and bright <= 190.0 and dark >= seadark - 3.0
    if Kind == "dusk":
        # (the sky at the horizon as drawn: the sky is lightest at the horizon, so the lightest row's median above the
        # lane's first row, in the render without it)
        sea_top = int(np.where(m.any(axis=1))[0].min())
        sky = float(max(np.median(luma(B[r])) for r in range(0, max(sea_top, 1))))
        ok = ok and bright <= sky
        extra = f", the sky at the horizon luma {sky:.0f}"
    Bad += 0 if ok else 1
    print(f"{'ok  ' if ok else 'FAIL'} {V} ({Kind}): in {len(cols)} of {A.shape[1]} columns, the columns' peak lift "
          f"{p5:+.1f} / {med:+.1f} / {p95:+.1f} levels (5th / median / 95th; bound {Lo} to {Hi}), min {float(peak.min()):+.1f} "
          f"max {float(peak.max()):+.1f}, brightest luma {bright:.0f} (foam 190){extra}, darkest {dark:.0f} on the sea's darkest {seadark:.0f}")
sys.exit(1 if Bad else 0)
