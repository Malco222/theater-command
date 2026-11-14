#!/usr/bin/env python3
# silence.py DIR HABIT...: the longest run of months without a battle on the player's eastern front, counted from January 1942,
# for each habit in probe28 TOTAL lines (files DIR/HABIT-GER-SEED.txt): median, how many wars had a silence of 6 and of 12 months
# or more, and the year the silence ended in.
import re, sys, glob, statistics, collections
d = sys.argv[1]
for h in sys.argv[2:]:
    rows = []
    for f in sorted(glob.glob(f"{d}/{h}-GER-*.txt"), key=lambda s: int(re.search(r"-(\d+)\.txt$", s).group(1))):
        t = [l for l in open(f) if l.startswith("TOTAL")]
        if not t: continue
        m = re.search(r"longest silence from January 1942: (\d+) months?(?:, ending (\w+) (\d{4}))?", t[-1])
        if not m: continue
        seed = int(re.search(r"-(\d+)\.txt$", f).group(1))
        rows.append((seed, int(m.group(1)), m.group(3)))
    if not rows: print(h, "no rows"); continue
    n = [r[1] for r in rows]
    yrs = collections.Counter(r[2] for r in rows if r[1] >= 6)
    print(f"{h:14s} wars {len(rows):2d}; longest silence: median {statistics.median(n):.1f} months, mean {statistics.mean(n):.1f}, most {max(n)}; six months or more in {sum(x>=6 for x in n)}, twelve or more in {sum(x>=12 for x in n)}; by year it ended (six or more): {dict(sorted(yrs.items()))}")
    print("   by seed:", " ".join(f"{s}:{x}" for s, x, _ in rows))
