#!/usr/bin/env python3
"""Version 76: a year in a small country's seat (probe75's files DIR/year-MODE-NAT-SEED.txt).
usage: yeartab.py DIR [DIR...]
One line a war: the paths open in September 1939, what the player did (path taken, war declared, League invitations),
when the country was at war and with whom, its income at peace and at war, the months it was attacked, the months with
an enemy next to one of its corps, the months with no order at all, and the corps that left home."""
import re, sys, glob, os
NAT = ['BRA', 'ARG', 'CHL', 'PER', 'COL', 'VEN', 'MEX']
rows = []
for d in sys.argv[1:]:
    for f in sorted(glob.glob(os.path.join(d, 'year-*-*-*.txt'))):
        m = re.search(r'year-(\w+)-([A-Z]+)-(\d+)\.txt$', f)
        if not m: continue
        t = open(f).read()
        ys = re.findall(r'^Y \w+ \w+ seed \d+ (.*?) \| (.*?) \| income ([\d.]+) trade ([\d.]+) treasury (\d+) capital ([\d.-]+) \| paths: (.*?) \| League founders willing: (.*?) \| may declare on: (.*?) \| trade: .*? \| field corps (\d+), away (\d+), sealift (\d+) \| provinces (\d+)(?: \| the United States at war with (.*?); League members (.*))?$', t, re.M)
        if not ys: print(f'{os.path.basename(f)}: no reading'); continue
        first = ys[0]
        paths = '; '.join(p.split(' (')[0] for p in first[6].split('; '))
        s = re.search(r'^SUM .*?declared (.*?); a battle of the player\'s own in (\d+) months, attacked in (\d+), no order at all in (\d+); (stands|FALLEN)', t, re.M)
        w = re.search(r'^W \w+ is at war from (\w+ \d+) with (.*)$', t, re.M)
        inv = re.findall(r'^I \w+ invited (\w+) into the League in (\w+ \d+)', t, re.M)
        opt = sum(int(x) for x in re.findall(r'had an enemy next to a corps in (\d+)', t))
        peace = [float(y[2]) for y in ys if y[1].startswith('peace')]; war = [float(y[2]) for y in ys if y[1].startswith('WAR')]
        notes = re.findall(r'(landing at [^:;]+?) from', t)
        rows.append((NAT.index(m[2]) if m[2] in NAT else 9, m[2], m[1], int(m[3]),
            f"{m[2]} {m[1]:6s} seed {m[3]}: doors in September 1939: {paths} | {s[1] if s else '?'}{'; invited ' + ' '.join(f'{a} ({b})' for a, b in inv) if inv else ''} | at war {('from ' + w[1] + ' with ' + w[2]) if w else 'never'} | income at peace {max(peace) if peace else 0:.1f}, at war {max(war) if war else 0:.1f} | attacked in {s[3] if s else '?'} months, an enemy beside a corps in {opt}, no order at all in {s[4] if s else '?'} of 76 | corps away at most {max(int(y[10]) for y in ys)}, corps at the end {ys[-1][9]} | provinces {first[12]} then {ys[-1][12]} | {s[5] if s else '?'}{' | ' + '; '.join(sorted(set(notes))[:3]) if notes else ''}{' | America at war with: ' + ys[-2][13] if len(ys) > 1 and ys[-2][13] else ''}"))
for r in sorted(rows): print(r[4])
