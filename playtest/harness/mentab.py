#!/usr/bin/env python3
"""Version 62: call-up habits on twelve seeds, from probe32's TOTAL lines. usage: mentab.py DIR [-v]"""
import sys, re, glob, os, statistics as st
from collections import defaultdict
D = sys.argv[1]; V = '-v' in sys.argv
MON = {m: i for i, m in enumerate('January February March April May June July August September October November December'.split())}
def t(s):
    if not s or s == 'never': return None
    m, y = s.split(); return (int(y) - 1939) * 12 + MON[m]
def lab(n):
    if n is None: return 'never'
    return f"{list(MON)[n % 12][:3]} {1939 + n // 12}"
rows = defaultdict(dict)
for f in sorted(glob.glob(os.path.join(D, 'callup-*.txt'))):
    name = os.path.basename(f)[:-4]; m = re.match(r'callup-(\w+?)_(\w+)-(\d+)$', name); who, habit, seed = m.group(1), m.group(2), int(m.group(3))
    for ln in open(f, errors='replace'):
        if not ln.startswith('TOTAL'): continue
        fell = re.search(r'played to (\w+ \d{4}) \(fell', ln); end = re.search(r'played to (\w+ \d{4})', ln).group(1)
        r = dict(fell=t(fell.group(1)) if fell else None, end=t(end),
                 half=t(re.search(r'reserve under half (never|\w+ \d{4})', ln).group(1)), quarter=t(re.search(r'under a quarter (never|\w+ \d{4})', ln).group(1)), spent=t(re.search(r'spent (never|\w+ \d{4})', ln).group(1)),
                 early=int(re.search(r'classes called early (\d+)', ln).group(1)), held=int(re.search(r'call-up held (\d+) months', ln).group(1)),
                 peak=int(re.search(r'strongest (\d+) divisions', ln).group(1)), peakat=t(re.search(r'strongest \d+ divisions in (\w+ \d{4})', ln).group(1)),
                 berlin=re.search(r'Berlin in December 1944: (\S+?);', ln).group(1), prov=int(re.search(r'provinces at the end (\d+)', ln).group(1)),
                 jan={int(y): (int(c), int(a), int(b), int(p)) for y, c, a, b, p in re.findall(r'(\d{4}): (\d+) corps, (\d+) of (\d+) divisions, (\d+) provinces', ln)})
        rows[(who, habit)][seed] = r
def med(xs):
    xs = [x for x in xs if x is not None]; return st.median(xs) if xs else None
for who in ('SOV', 'SOVF', 'GER'):
    hs = [h for (w, h) in rows if w == who]
    if not hs: continue
    print(f'== {who}')
    base = rows[(who, 'asis')]
    for h in sorted(hs, key=lambda x: (x != 'asis', x)):
        R = rows[(who, h)]; n = len(R); fell = [r['fell'] for r in R.values() if r['fell'] is not None]
        jan = lambda y, i: med([r['jan'][y][i] for r in R.values() if y in r['jan']])
        later = sum(1 for s, r in R.items() if s in base and (r['fell'] or 999) > (base[s]['fell'] or 999)); sooner = sum(1 for s, r in R.items() if s in base and (r['fell'] or 999) < (base[s]['fell'] or 999))
        print(f"  {h:<13}{n:>3} wars | fell {len(fell):>2} (median {lab(int(med(fell))) if fell else '-'}) | against as it stands: lasts longer {later}, falls sooner {sooner} | "
              f"own capital... Berlin Dec 1944: {dict((k, sum(1 for r in R.values() if r['berlin'] == k)) for k in sorted(set(r['berlin'] for r in R.values())))} | provinces at the end median {med([r['prov'] for r in R.values()]):.0f} | "
              f"reserve spent: {sum(1 for r in R.values() if r['spent'] is not None)} wars, median {lab(int(med([r['spent'] for r in R.values()]))) if med([r['spent'] for r in R.values()]) is not None else '-'} | "
              f"strongest {med([r['peak'] for r in R.values()]):.0f} divs | early classes {med([r['early'] for r in R.values()]):.0f}, held {med([r['held'] for r in R.values()]):.0f} months")
        print('      Januaries (median corps, divisions filled of full, provinces): ' + ' | '.join(f"{y}: {jan(y, 0):.0f}, {jan(y, 1):.0f} of {jan(y, 2):.0f}, {jan(y, 3):.0f}" for y in (1942, 1943, 1944, 1945) if any(y in r['jan'] for r in R.values())))
        if V:
            print('      by seed: ' + ' '.join(f"{s}:{lab(r['fell']) if r['fell'] is not None else 'stands ' + str(r['prov'])}" for s, r in sorted(R.items())))
