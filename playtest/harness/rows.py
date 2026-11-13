#!/usr/bin/env python3
# Reads probe28's ROW lines (ROWS=1) from a folder: how often a line holds, by the card's odds and by how ready the line is.
# usage: rows.py DIR [NAME-FILTER]
import sys, glob, os, collections
d = sys.argv[1]; flt = sys.argv[2] if len(sys.argv) > 2 else ''
R = []
for f in sorted(glob.glob(os.path.join(d, 'def-*.txt'))):
    if flt and flt not in os.path.basename(f): continue
    for l in open(f):
        if not l.startswith('ROW '): continue
        p = l.split()
        R.append(dict(turn=int(p[1]), side=p[2], att=p[3], dfn=p[4], prep=float(p[5]), card=float(p[6]), day=float(p[7]), won=int(p[8]), al=int(p[9]), dl=int(p[10]), na=int(p[11]), nd=int(p[12]), art=int(p[13]), reg=p[14], f=os.path.basename(f)))
print(f"{len(R)} field battles in {d} {flt}")
def tab(name, rows, bands):
    print(name)
    for lo, hi in bands:
        s = [r for r in rows if lo <= r['card'] < hi]
        if not s: continue
        held = sum(1 for r in s if not r['won'])
        print(f"   odds {lo:4.2f} to {hi:5.2f}: {len(s):4d} attacks, line held {held:3d} ({100*held/len(s):5.1f}%), mean odds {sum(r['card'] for r in s)/len(s):.2f}, men lost attacker {sum(r['al'] for r in s)/len(s):7.0f} defender {sum(r['dl'] for r in s)/len(s):7.0f}")
B = [(0, 1), (1, 1.2), (1.2, 1.4), (1.4, 1.6), (1.6, 1.8), (1.8, 2.2), (2.2, 3), (3, 100)]
prep = [r for r in R if r['prep'] > 0]; op = [r for r in R if r['prep'] == 0]
tab('OPEN ground (not prepared)', op, B)
tab('PREPARED line, any share', prep, B)
tab('PREPARED line, fully ready (share 0.95 or more)', [r for r in prep if r['prep'] >= 0.95], B)
tab('PREPARED line, partly ready (share under 0.95)', [r for r in prep if r['prep'] < 0.95], B)
sh = collections.Counter('full (0.95+)' if r['prep'] >= 0.95 else 'half to 0.95' if r['prep'] >= 0.5 else 'under half' for r in prep)
print('how ready the prepared lines were:', dict(sh), 'of', len(prep), '| share of all field battles on a prepared line:', f"{100*len(prep)/max(1,len(R)):.0f}%")
print('who attacked prepared lines:', dict(collections.Counter(r['att'] for r in prep).most_common(6)))
g = [r for r in prep if r['art'] > 0]
print(f"attacks on prepared lines with artillery corps in them: {len(g)} of {len(prep)}; of all field battles: {sum(1 for r in R if r['art']>0)} of {len(R)}")
if g: tab('PREPARED line, attack with guns', g, B)
# by year
for y in range(1939, 1946):
    s = [r for r in R if 1939 + (r['turn'] + 7) // 12 == y]
    if s: print(f"   {y}: {len(s):4d} battles, {sum(1 for r in s if r['prep']>0):4d} on prepared lines, thrown back {sum(1 for r in s if not r['won']):3d}")
