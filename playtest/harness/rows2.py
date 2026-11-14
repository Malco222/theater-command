#!/usr/bin/env python3
# Version 61: like-for-like battle rows of two folders (same file names only). usage: rows2.py OLD NEW [NAME-FILTER]
import sys, glob, os, collections
a, b = sys.argv[1], sys.argv[2]; flt = sys.argv[3] if len(sys.argv) > 3 else ''
names = sorted(set(os.path.basename(f) for f in glob.glob(os.path.join(a, 'def-*.txt'))) & set(os.path.basename(f) for f in glob.glob(os.path.join(b, 'def-*.txt'))))
names = [n for n in names if flt in n]
def load(d):
    R = []
    for n in names:
        for l in open(os.path.join(d, n)):
            if not l.startswith('ROW '): continue
            p = l.split()
            R.append(dict(turn=int(p[1]), side=p[2], att=p[3], dfn=p[4], prep=float(p[5]), card=float(p[6]), day=float(p[7]), won=int(p[8]), al=int(p[9]), dl=int(p[10]), na=int(p[11]), nd=int(p[12]), art=int(p[13]), reg=p[14], f=n))
    return R
print(len(names), 'wars in both:', ' '.join(n[4:-4] for n in names))
for d in (a, b):
    R = load(d); prep = [r for r in R if r['prep'] > 0]
    print(f"== {d}: {len(R)} field battles, {len(prep)} on a prepared line ({100*len(prep)/max(1,len(R)):.0f}%), thrown back {sum(1 for r in R if not r['won'])} of all, {sum(1 for r in prep if not r['won'])} of prepared ({100*sum(1 for r in prep if not r['won'])/max(1,len(prep)):.1f}%)")
    full = [r for r in prep if r['prep'] >= 0.95]; half = [r for r in prep if 0.5 <= r['prep'] < 0.95]; low = [r for r in prep if r['prep'] < 0.5]
    for nm, s in (('fully ready', full), ('half to full', half), ('under half', low)):
        print(f"   {nm:13s}: {len(s):4d} attacks, thrown back {sum(1 for r in s if not r['won']):3d} ({100*sum(1 for r in s if not r['won'])/max(1,len(s)):.0f}%)")
    for lo, hi in ((0, 1.2), (1.2, 1.4), (1.4, 1.6), (1.6, 2.2), (2.2, 100)):
        s = [r for r in prep if lo <= r['card'] < hi]
        if s: print(f"   prepared, card odds {lo} to {hi}: {len(s):4d}, thrown back {sum(1 for r in s if not r['won']):3d} ({100*sum(1 for r in s if not r['won'])/len(s):.0f}%); men lost attacker {sum(r['al'] for r in s)/len(s):6.0f} defender {sum(r['dl'] for r in s)/len(s):6.0f}")
    g = [r for r in prep if r['art'] > 0]
    print(f"   attacks on prepared lines with artillery corps: {len(g)} of {len(prep)} ({100*len(g)/max(1,len(prep)):.1f}%); by the computer only: {sum(1 for r in g if r['side']=='B')} of {sum(1 for r in prep if r['side']=='B')}; all field battles with guns {sum(1 for r in R if r['art']>0)} of {len(R)}")
    print(f"   guns by attacker: {dict(collections.Counter(r['att'] for r in R if r['art']>0).most_common(8))}")
    for y in range(1939, 1946):
        s = [r for r in R if 1939 + (r['turn'] + 7) // 12 == y]
        if s: print(f"   {y}: {len(s):4d} battles, {sum(1 for r in s if r['prep']>0):4d} on prepared lines, thrown back {sum(1 for r in s if not r['won']):3d}")
