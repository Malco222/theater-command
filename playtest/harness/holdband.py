#!/usr/bin/env python3
"""The forecast "Once ashore" against what happened, split by how the two fleets compared on the card (Version 74).
usage: holdband.py GLOB...   (the HELD lines of probe56; a landing whose month after could not be seen is left out)
The card says "stays open" when the lander's fleets count at least half of the enemy fleets that can come.
"""
import glob, re, sys
pat = re.compile(r"^HELD (\w+) seed (\d+): (\w+ \d+) (.+?) with (\d+) corps: the card said the (.+?) (stays open|would be closed) \(enemy (\d+), own (\d+), need (\d+)(.*?)\); a month later it was (\w+|\?), the force (.+?); closed in any of the three months after: (\w+)")
rows = []
for a in sys.argv[1:]:
    for f in sorted(glob.glob(a)):
        for l in open(f):
            m = pat.match(l)
            if m: rows.append(m.groups())
def band(e, o):
    if e == 0: return '1 no enemy fleet can come'
    r = o / e
    return '2 own fleets at least the enemy\'s' if r >= 1 else '3 own fleets 50 to 99% of the enemy\'s' if r >= .5 else '4 own fleets under half'
print(f'{len(rows)} landings read')
for grp, seats in (('Germany, Italy, France, the Soviet Union (the weaker navies)', ('GER', 'ITA', 'FRA', 'SOV')), ('Britain', ('UK',)), ('the United States', ('USA',))):
    t = {}
    for nat, seed, when, place, n, sea, said, e, o, need, extra, later, force, three in rows:
        if nat not in seats or later == '?': continue
        a = t.setdefault((said, band(int(e), int(o))), [0, 0, 0]); a[0] += 1; a[1] += later == 'CLOSED'; a[2] += three == 'yes'
    if not t: continue
    print(f'\n   {grp}:')
    for k in sorted(t):
        n, c1, c3 = t[k]
        print(f'      the card said "{"can be closed" if k[0] == "would be closed" else k[0]}", {k[1][2:]:38} {n:4} landings: closed a month later in {c1:3} ({round(100 * c1 / n):2}%), within three months in {c3:3} ({round(100 * c3 / n):2}%)')
