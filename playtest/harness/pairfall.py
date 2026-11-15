#!/usr/bin/env python3
"""Pairs two sets of seat wars by seed on the month Germany fell (from the SEAT lines) and on par.
usage: pairfall.py DIR1 PREFIX1 DIR2 PREFIX2 [--seeds A-B]   (PREFIX like seatstage-USA; a war where Germany stands is left out of the mean and counted)"""
import re, sys, glob, statistics
M=['January','February','March','April','May','June','July','August','September','October','November','December']
def turn(l):
    m=re.match(r'(\w+) (\d{4})',l); return (int(m.group(2))-1939)*12+M.index(m.group(1))-8+1
def label(t):
    k=int(round(t))+8-1; return f'{M[k%12]} {1939+k//12}'
args=[a for a in sys.argv[1:] if not a.startswith('--')]
lo,hi=1,999
if '--seeds' in sys.argv: lo,hi=map(int,sys.argv[sys.argv.index('--seeds')+1].split('-'))
def rd(d,p):
    out={}
    for f in glob.glob(f'{d}/{p}-*.txt'):
        s=int(re.search(r'-(\d+)\.txt$',f).group(1))
        if not lo<=s<=hi: continue
        for line in open(f):
            if not line.startswith('SEAT '): continue
            m=re.search(r'great powers fallen: ([^;]*);',line); g=re.search(r'GER (\w+ \d{4})',m.group(1)) if m else None
            par=re.search(r'; par (BEATEN|missed) \(',line)
            out[s]=(turn(g.group(1)) if g else None, par.group(1)=='BEATEN' if par else None)
    return out
a=rd(args[0],args[1]); b=rd(args[2],args[3])
both=sorted(set(a)&set(b))
d=[b[s][0]-a[s][0] for s in both if a[s][0] and b[s][0]]
fa=sorted(a[s][0] for s in both if a[s][0]); fb=sorted(b[s][0] for s in both if b[s][0])
print(f"{args[0]}/{args[1]} then {args[2]}/{args[3]}, {len(both)} wars paired by seed (seeds {both[0]} to {both[-1]}):")
print(f"   Germany fell in {len(fa)} then {len(fb)}; the middle war {label(statistics.median(fa)) if fa else '-'} then {label(statistics.median(fb)) if fb else '-'}")
if d: print(f"   paired: Germany falls {statistics.mean(d):+.1f} months on the second (middle {statistics.median(d):+.1f}); earlier in {sum(x<0 for x in d)}, later in {sum(x>0 for x in d)}, the same month in {sum(x==0 for x in d)} of {len(d)}")
pa=[a[s][1] for s in both]; pb=[b[s][1] for s in both]
if all(x is not None for x in pa+pb): print(f"   par beaten in {sum(pa)} then {sum(pb)} of {len(both)}; gained {sum((not x) and y for x,y in zip(pa,pb))}, lost {sum(x and (not y) for x,y in zip(pa,pb))}")
