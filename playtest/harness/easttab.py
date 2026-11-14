#!/usr/bin/env python3
"""Version 67: the war in the east, from probe28's EAST and TOTAL lines (files DIR/NAME-GER-SEED.txt).
usage: easttab.py DIR NAME... [--seeds A-B]
For each set: how many wars the Soviet Union fell in and when, Berlin, the silence while the eastern war lasted
(six months or more, a year or more) and the silence after it was won, so that a front that sleeps is not
mistaken for a war already won, nor the other way round."""
import re, sys, glob, os, statistics as st
args = sys.argv[1:]
lo, hi = 1, 9999
if '--seeds' in args:
    i = args.index('--seeds'); lo, hi = map(int, args[i + 1].split('-')); del args[i:i + 2]
d, names = args[0], args[1:]
MON = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']
def turn(month, year): return (int(year) - 1939) * 12 + MON.index(month) + 1 - 8
def date(t): t += 7; return f'{MON[t % 12]} {1939 + t // 12}'
for name in names:
    rows = []
    for f in glob.glob(os.path.join(d, f'{name}-GER-*.txt')):
        seed = int(re.search(r'-(\d+)\.txt$', f).group(1))
        if not lo <= seed <= hi: continue
        txt = open(f).read().splitlines()
        e = [l for l in txt if l.startswith('EAST')]
        t = [l for l in txt if l.startswith('TOTAL')]
        if not e or not t: continue
        e, t = e[-1], t[-1]
        r = {'seed': seed, 'berlin': 'Berlin in December 1944: GER' in t, 'stands': '(fell ' not in t}
        m = re.search(r'over (\w+) (\d{4}) \((it fell|peace)\)', e)
        r['over'] = turn(m.group(1), m.group(2)) if m else 0
        r['how'] = m.group(3) if m else ''
        m = re.search(r'while it lasted (\d+) months', e); r['during'] = int(m.group(1)) if m else 0
        m = re.search(r'after it: (\d+) months still at war with someone, a battle of the player\'s in (\d+), longest silence (\d+)', e)
        r['after'], r['afterFought'], r['afterQuiet'] = map(int, m.groups()) if m else (0, 0, 0)
        rows.append(r)
    n = len(rows)
    if not n: print(f'== {name}: no wars with an EAST line'); continue
    fell = [r for r in rows if r['how'] == 'it fell']
    peace = [r for r in rows if r['how'] == 'peace']
    print(f"== {name}: {n} wars; Berlin held to the end of 1944 in {sum(r['berlin'] for r in rows)} ({round(100 * sum(r['berlin'] for r in rows) / n)}%); Germany standing at the end in {sum(r['stands'] for r in rows)}")
    print(f"   the Soviet Union fell in {len(fell)} ({round(100 * len(fell) / n)}%)" + (f", the middle one in {date(sorted(r['over'] for r in fell)[len(fell) // 2])}, the first in {date(min(r['over'] for r in fell))}; by year " + ', '.join(f"{y} {sum(1 for r in fell if 1939 + (r['over'] + 7) // 12 == y)}" for y in range(1941, 1946) if any(1939 + (r['over'] + 7) // 12 == y for r in fell)) if fell else '') + (f"; peace without a fall in {len(peace)}" if peace else ''))
    print(f"   silence while the eastern war lasted (from January 1942): six months or more in {sum(r['during'] >= 6 for r in rows)}, a year or more in {sum(r['during'] >= 12 for r in rows)}, middle {st.median(r['during'] for r in rows):g}, longest {max(r['during'] for r in rows)}")
    if fell:
        print(f"   after the Soviet Union fell: still at war for a middle {st.median(r['after'] for r in fell):g} months, with a battle of the player's in a middle {st.median(r['afterFought'] for r in fell):g} of them; longest silence then: middle {st.median(r['afterQuiet'] for r in fell):g}, a year or more in {sum(r['afterQuiet'] >= 12 for r in fell)}")
    alive = [r for r in rows if not r['how']]
    if alive:
        print(f"   wars where the eastern war was still on at the end, or Germany fell first: {len(alive)}; Berlin held in {sum(r['berlin'] for r in alive)}; silence of six months or more in {sum(r['during'] >= 6 for r in alive)}")
