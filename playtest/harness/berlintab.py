#!/usr/bin/env python3
"""Berlin at the end of 1944, with the cabinet and with it off, seed by seed. usage: berlintab.py DIR... (files berlin-GER-N.txt, berlinoff-GER-N.txt)"""
import sys, re, glob, os, statistics as st
MON = {m: i for i, m in enumerate('January February March April May June July August September October November December'.split())}
def t(s): m, y = s.split(); return (int(y) - 1939) * 12 + MON[m]
def lab(n): return f"{list(MON)[int(n) % 12][:3]} {1939 + int(n) // 12}"
on = {}; off = {}
for D in sys.argv[1:]:
    for f in glob.glob(os.path.join(D, 'berlin*-GER-*.txt')):
        m = re.match(r'(berlin|berlinoff)-GER-(\d+)\.txt', os.path.basename(f)); seed = int(m.group(2))
        for ln in open(f, errors='replace'):
            if ln.startswith('TOTAL'):
                fell = re.search(r'played to (\w+ \d{4}) \(fell', ln)
                r = dict(held='Berlin in December 1944: GER' in ln, fell=t(fell.group(1)) if fell else 84)
                (on if m.group(1) == 'berlin' else off)[seed] = r
def line(label, d, seeds):
    xs = [d[s] for s in seeds if s in d]
    if not xs: return
    print(f"{label:<44}{sum(1 for r in xs if r['held']):>3} of {len(xs):>3} hold Berlin to the end of 1944; median fall {lab(st.median([r['fell'] for r in xs])) if st.median([r['fell'] for r in xs]) < 84 else 'after 1945'}; still standing December 1945: {sum(1 for r in xs if r['fell'] == 84)}")
for a, b in ((1, 24), (25, 60), (1, 60)):
    s = range(a, b + 1); line(f'with the cabinet, seeds {a} to {b}', on, s); line(f'cabinet off, seeds {a} to {b}', off, s)
both = [s for s in on if s in off]
print(f"seed by seed ({len(both)} pairs): held with and without {sum(1 for s in both if on[s]['held'] and off[s]['held'])}, only with the cabinet {sum(1 for s in both if on[s]['held'] and not off[s]['held'])}, only without {sum(1 for s in both if not on[s]['held'] and off[s]['held'])}, neither {sum(1 for s in both if not on[s]['held'] and not off[s]['held'])}")
d = [on[s]['fell'] - off[s]['fell'] for s in both]
if d: print(f"months the war lasts longer with the cabinet: median {st.median(d)}, longer on {sum(1 for x in d if x > 0)}, shorter on {sum(1 for x in d if x < 0)}, the same on {sum(1 for x in d if x == 0)}")
