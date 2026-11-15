#!/usr/bin/env python3
"""The computer's American army, from the ARMY lines of probe68 (nobody playing) and probe69 (beside a player).
usage: armytab.py GLOB..."""
import re, sys, glob, statistics, collections
M=['January','February','March','April','May','June','July','August','September','October','November','December']
def turn(l):
    m=re.match(r'(\w+) (\d{4})',l); return (int(m.group(2))-1939)*12+M.index(m.group(1))-8+1
def label(t):
    k=int(t)+8-1; return f'{M[k%12]} {1939+k//12}'
rows=[]
for pat in sys.argv[1:]:
    for f in sorted(glob.glob(pat)):
        for line in open(f):
            if line.startswith('ARMY '): rows.append(line.rstrip())
            if line.startswith('ERROR'): print('ERROR in', f)
n=len(rows)
if not n: sys.exit('no ARMY lines')
def field(r,key):
    m=re.search(key,r); return m
par=sum('par BEATEN' in r for r in rows)
first=[turn(m.group(1)) for r in rows if (m:=re.search(r'first on the continent (\w+ \d{4})',r))]
where=collections.Counter(m.group(1) for r in rows if (m:=re.search(r'first on the continent \w+ \d{4} at [^(]*\((\w+)\)',r)))
stay=[turn(m.group(1)) for r in rows if (m:=re.search(r'ashore in the west to stay from (\w+ \d{4})',r))]
never=sum('ashore in the west to stay never' in r for r in rows)
brit=[int(m.group(1)) for r in rows if (m:=re.search(r'in Britain at the most (\d+) corps',r))]
most=[int(m.group(1)) for r in rows if (m:=re.search(r'most across (\d+) corps',r))]
raised=[int(m.group(1)) for r in rows if (m:=re.search(r'corps raised (\d+)',r))]
fell=[turn(m.group(1)) for r in rows if (m:=re.search(r'Germany fell (\w+ \d{4})',r))]
uk=[];cont=[];home=[];tot=[]
for r in rows:
    m=re.search(r'when Germany fell its corps stood: ([^|]*)',r)
    if not m or m.group(1).strip()=='-': continue
    d={k:int(v) for k,v in re.findall(r'(\w+(?: overseas)?) (\d+)',m.group(1))}
    uk.append(d.get('UK',0)); home.append(d.get('home',0)); cont.append(sum(v for k,v in d.items() if k not in('UK','home'))); tot.append(sum(d.values()))
med=lambda x: statistics.median(x) if x else 0
print(f"{n} wars")
print(f"   Germany fell in {len(fell)}; the American seat's own par (ashore in the west to stay before June 1944, Germany beaten before May 1945) BEATEN by the computer's America in {par} of {n}")
print(f"   first corps on the continent: in {len(first)} wars, the middle one {label(med(first)) if first else '-'}; on whose ground first: {', '.join(f'{k} {v}' for k,v in where.most_common())}")
print(f"   ashore in the west to stay: in {len(stay)} wars (the middle one from {label(med(stay)) if stay else '-'}; before June 1944 in {sum(t<58 for t in stay)}); never in {never}")
print(f"   corps raised: middle {med(raised)}; most across the ocean at once: middle {med(most)} (from {min(most)} to {max(most)}); in Britain at the most: middle {med(brit)} (from {min(brit)} to {max(brit)})")
if tot: print(f"   the month Germany fell (middle war): {med(tot)} corps in all, {med(home)} at home, {med(uk)} in Britain, {med(cont)} anywhere else; wars with more corps in Britain than on the continent: {sum(a>b for a,b in zip(uk,cont))} of {len(tot)}; wars with none on the continent: {sum(b==0 for b in cont)}")
j=[(int(m.group(1)),int(m.group(2))) for r in rows if (m:=re.search(r'as June 1944 opens (-?\d+) in Britain and (-?\d+) in the west',r))]
j=[x for x in j if x[0]>=0]
if j: print(f"   as June 1944 opens (wars still going: {len(j)}): middle {med([a for a,b in j])} corps in Britain, {med([b for a,b in j])} in the west; wars with none in the west {sum(b==0 for a,b in j)}")
p=[turn(m.group(1)) for r in rows if (m:=re.search(r"the player's own corps ashore in the west to stay from (\w+ \d{4})",r))]
if any('the player' in r for r in rows): print(f"   the player's own corps ashore in the west to stay: in {len(p)} wars, before June 1944 in {sum(t<58 for t in p)}")
