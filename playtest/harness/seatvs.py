#!/usr/bin/env python3
"""Version 68: does the player in a seat change when Germany falls? Sets probe54's SEAT lines beside the builder's
computer-only wars on the same seeds (/mnt/project-files/theater-command/results/v68/fin68L-*.txt).
usage: seatvs.py DIR PREFIX... [--seeds A-B]
For each prefix and seat: on how many seeds Germany fell both ways, the middle difference in months (minus: sooner
with the player in the seat), how many wars fell in the very same month, and the spread."""
import re, sys, glob, os, statistics as st
args = sys.argv[1:]
lo, hi = 1, 9999
if '--seeds' in args:
    i = args.index('--seeds'); lo, hi = map(int, args[i + 1].split('-')); del args[i:i + 2]
d, names = args[0], args[1:]
MON = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']
def turn(s):
    m = re.match(r'(\w+) (\d{4})', s); return (int(m.group(2)) - 1939) * 12 + MON.index(m.group(1)) + 1 - 8
base = {}
for f in glob.glob('/mnt/project-files/theater-command/results/v68/fin68L-*.txt'):
    for l in open(f):
        m = re.match(r'seed (\d+): GER (fell (\w+ \d{4})|stands)', l)
        if m: base[int(m.group(1))] = turn(m.group(3)) if m.group(3) else 0
print(f'computer-only wars read: {len(base)} (Germany fell in {sum(1 for v in base.values() if v)})')
for name in names:
    by = {}
    for f in glob.glob(os.path.join(d, f'{name}-*-*.txt')):
        m = re.search(rf'{re.escape(name)}-([A-Z]+)-(\d+)\.txt$', os.path.basename(f))
        if not m: continue
        nat, seed = m.group(1), int(m.group(2))
        if not lo <= seed <= hi or seed not in base: continue
        s = [l for l in open(f).read().splitlines() if l.startswith('SEAT')]
        if not s: continue
        m2 = re.search(r'great powers fallen: ([^;]*);', s[-1]).group(1)
        g = re.search(r'GER (\w+ \d{4})(?! \(back\))', m2)
        by.setdefault(nat, []).append((seed, turn(g.group(1)) if g else 0))
    print(f'== {name}')
    for nat in sorted(by):
        rows = by[nat]
        both = [(t - base[s]) for s, t in rows if t and base[s]]
        only_player = sum(1 for s, t in rows if t and not base[s]); only_comp = sum(1 for s, t in rows if not t and base[s])
        if not both: print(f'   {nat}: {len(rows)} seeds, none where Germany fell both ways (with the player only {only_player}, computer only {only_comp})'); continue
        print(f"   {nat}: {len(rows)} seeds; Germany fell both ways in {len(both)}; with the player in the seat it fell a middle {st.median(both):+g} months against the computer-only war (same month in {sum(1 for x in both if x == 0)}, sooner in {sum(1 for x in both if x < 0)}, later in {sum(1 for x in both if x > 0)}; from {min(both):+d} to {max(both):+d}); fell only with the player {only_player}, only without {only_comp}")
