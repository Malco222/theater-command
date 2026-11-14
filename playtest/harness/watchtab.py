#!/usr/bin/env python3
# watchtab.py DIR...: how the computer-only wars of watch.mjs end, a folder a line: who falls, when, and which wars are
# still undecided in the last month played (neither Germany nor the Soviet Union has fallen).
import re, sys, glob
for d in sys.argv[1:]:
    rows = {}
    for f in glob.glob(f'{d}/watch-*.txt'):
        for l in open(f):
            if not l.startswith('seed'): continue
            s = int(re.match(r'seed\s+(\d+)', l).group(1))
            fell = dict(re.findall(r'(FRA|SOV|GER|ITA|UK) (-|[A-Z][a-z]{2} \d\d)', l.split('|')[0]))
            by = re.search(r'battles/yr ([\d ]+)\|', l).group(1).split()
            rows[s] = (fell, by)
    und = sorted(s for s, (f, b) in rows.items() if f['SOV'] == '-' and f['GER'] == '-')
    gw = sorted((s, f['SOV']) for s, (f, b) in rows.items() if f['SOV'] != '-')
    sw = sorted((s, f['GER']) for s, (f, b) in rows.items() if f['GER'] != '-')
    yr = lambda x: 1900 + int(x.split()[1])
    print(f"{d}: {len(rows)} wars; Germany falls in {len(sw)} (by end 1945 in {sum(yr(g) <= 1945 for s, g in sw)}, in 1946 or 1947 in {[(s, g) for s, g in sw if yr(g) >= 1946]}); the Soviet Union falls in {len(gw)} {gw}; neither has fallen in the last month {und}")
    for s in und: print(f"   seed {s}: battles by year {' '.join(rows[s][1])}")
    quiet = [(s, [1939 + i for i, n in enumerate(b) if int(n) < 12 and 1 <= i <= 6]) for s, (f, b) in sorted(rows.items())]
    print('   years with fewer than 12 battles (1940 to 1945):', [(s, y) for s, y in quiet if y])
