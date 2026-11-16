#!/usr/bin/env python3
"""probe78's LAND lines by flag, ground and year: landings, wars, corps dead within six months, beaches Axis again;
and each landing in France before May 1944 with who stood on Germany's home ground that month (when the file has it).
usage: landyear.py GLOB [--nats=UK,USA] [--grounds=FRA,ITA,GRE] [--list]"""
import re,glob,sys,collections
args=[a for a in sys.argv[1:] if not a.startswith('--')]; opt={a.split('=')[0]:(a.split('=')[1] if '=' in a else '1') for a in sys.argv[1:] if a.startswith('--')}
nats=opt.get('--nats','UK,USA').split(','); grounds=opt.get('--grounds','FRA,ITA,GRE').split(',')
MON='January February March April May June July August September October November December'.split()
rows=[]
for f in sorted(glob.glob(args[0])):
    for l in open(f):
        m=re.match(r'LAND seed (\d+): (\w+) (\w+) (\d{4}) at (.+?) \((\w+)\)( WEST)? \| corps (\d+) \| (first wave|later wave)(.*?) \| six months on: dead (\d+) of (\d+), beach held by (\w+)(?: \| Germany\'s home ground entered that month: (.*))?',l)
        if m: rows.append(m.groups())
AX={'GER','ITA'}
print(f"{len({r[0] for r in rows})} wars, {len(rows)} landings")
agg=collections.defaultdict(lambda:[0,set(),0,0,0])
for r in rows:
    if r[1] in nats and r[5] in grounds:
        a=agg[(r[1],r[5],r[3])]; a[0]+=1; a[1].add(r[0]); a[2]+=int(r[10]); a[3]+=int(r[11]); a[4]+= r[12] in AX
for k in sorted(agg): a=agg[k]; print(f"   {k[0]} on {k[1]} ground in {k[2]}: {a[0]} landings in {len(a[1])} wars, dead {a[2]} of {a[3]} corps within six months, {a[4]} beaches Axis again")
tot=collections.defaultdict(lambda:[0,0,0])
for r in rows:
    if r[1] in nats: t=tot[r[1]]; t[0]+=1; t[1]+=int(r[10]); t[2]+=int(r[11])
for k,t in tot.items(): print(f"   {k}, all its landings: {t[0]}, dead {t[1]} of {t[2]}")
early=[r for r in rows if r[1] in nats and r[5]=='FRA' and (int(r[3]),MON.index(r[2]))<(1944,4)]
if early:
    has=[r for r in early if r[13] is not None]; ent=[r for r in has if r[13] and r[13]!='no']
    print(f"   landings in France before May 1944 by {' and '.join(nats)}: {len(early)} in {len({r[0] for r in early})} wars; dead {sum(int(r[10]) for r in early)} of {sum(int(r[11]) for r in early)}" + (f"; Germany's home ground entered that month in {len(ent)} of {len(has)}; NOT entered: {[(r[0],r[1],r[2],r[3],r[4]) for r in has if r not in ent]}" if has else ''))
    if '--list' in opt:
        for r in early: print(f"      seed {r[0]} {r[1]:<3} {r[2]} {r[3]} {r[4]}: dead {r[10]} of {r[11]}, beach held by {r[12]}" + (f", Germany's home ground entered by: {r[13] or 'nobody'}" if r[13] is not None else ''))
