#!/usr/bin/env python3
"""Directive kinds issued and how they ended, summed from the TOTAL lines of probe24 outputs, new beside old. usage: kindtab.py NEW OLD [PREFIX]"""
import sys, re, glob, os, json
from collections import Counter, defaultdict
N, O = sys.argv[1], sys.argv[2]; P = sys.argv[3] if len(sys.argv) > 3 else 'gov-'
def read(D):
    iss = Counter(); end = Counter(); fell = 0; wars = 0; low = 0; mean = []
    per = {}
    for f in sorted(glob.glob(os.path.join(D, P + '*.txt'))):
        for ln in open(f, errors='replace'):
            if ln.startswith('TOTAL'):
                m = re.search(r'directives issued (\{.*?\}); ended (\{.*?\}); cabinet fell (\d+) times', ln)
                if not m: continue
                wars += 1; i = json.loads(m.group(1)); e = json.loads(m.group(2)); iss.update(i); end.update(e); fell += int(m.group(3))
                t = re.search(r'trust mean (\d+), lowest (\d+), months under 35: (\d+)', ln); mean.append(int(t.group(1))); low += int(t.group(3))
                per[os.path.basename(f)[:-4]] = (i, int(m.group(3)), int(t.group(1)))
    return wars, iss, end, fell, low, mean, per
a = read(N); b = read(O)
for lab, (wars, iss, end, fell, low, mean, per) in (('NEW', a), ('OLD', b)):
    tot = sum(iss.values())
    print(f'{lab}: {wars} wars, {tot} directives ({tot / max(1, wars):.1f} a war); cabinet fell {fell} times; months under 35: {low}; mean trust {sum(mean) / max(1, len(mean)):.0f}')
    for k, n in iss.most_common():
        d = end.get(k + ' carried out', 0); x = end.get(k + ' not carried out', 0); w = end.get(k + ' withdrawn', 0); s = end.get(k + ' set aside', 0)
        print(f'   {k:<8}{n:>4} ({100 * n / tot:.0f}%)  carried out {d}, missed {x}, withdrawn {w}, set aside {s}')
if '-v' in sys.argv:
    for k in sorted(a[6]):
        if k in b[6]: print(f'  {k:<26} fell {a[6][k][1]} (was {b[6][k][1]}), trust {a[6][k][2]} (was {b[6][k][2]}), jobs {a[6][k][0].get("build", 0) + a[6][k][0].get("agent", 0)} of {sum(a[6][k][0].values())}')
