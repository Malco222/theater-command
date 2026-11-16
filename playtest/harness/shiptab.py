#!/usr/bin/env python3
"""Ruling 92 and ruling 94 (probe84): corps that came to the continent by ship and what became of them six months on,
and the Italian road into Germany. usage: shiptab.py GLOB [--list N]   (--list N: every group that lost N corps or more)"""
import sys, re, glob, statistics
M=['January','February','March','April','May','June','July','August','September','October','November','December']
def t(m,y): return (int(y)-1939)*12+M.index(m)-8+1
def name(n):
    n=int(n); k=n-1+8; return f"{M[k%12]} {1939+k//12}"
args=[a for a in sys.argv[1:] if not a.startswith('--')]
listn=int(sys.argv[sys.argv.index('--list')+1]) if '--list' in sys.argv else 0
if listn: args=[a for a in args if a!=str(listn)]
files=sorted(f for a in args for f in glob.glob(a))
ship=[]; italy={}; errs=0
for f in files:
    for l in open(f):
        if 'ERROR' in l: errs+=1
        m=re.match(r"SHIP seed (\d+): (\w+) (\w+) (\d{4}) at (.+?) \((\w+)\) \| (landing|ferry\+?) \| corps (\d+) \| six months on: dead (\d+) of (\d+)(.*)",l)
        if m:
            seed,nat,mo,yr,prov,own,kind,n,dead,done,rest=m.groups()
            w=[0]*6
            mm=re.search(r"in place: cut off (\d+), in supply (\d+); behind: cut off (\d+), in supply (\d+); marched out: cut off (\d+), in supply (\d+)",rest)
            if mm: w=[int(x) for x in mm.groups()]
            ship.append(dict(seed=int(seed),nat=nat,t=t(mo,yr),yr=int(yr),prov=prov,own=own,kind=kind,n=int(n),dead=int(dead),done=int(done),w=w,line=l.strip()))
        m=re.match(r"ITALY seed (\d+): Italy signed (\w+ \d{4}|never)(.*?) \| first western corps on Germany's home ground (never|(\w+) (\d{4}): (.*?)) \| months from Italy's signing to that: (\S+) \| first Soviet corps there (\w+ \d{4}|never) \| months before May 1944 with western corps there: (\d+) \| Germany (fell (\w+) (\d{4})|stands)",l)
        if m:
            g=m.groups(); seed=int(g[0])
            frm=re.search(r"the ground they stood on the month before: (.*?); they had come",g[6] or '')
            prov=re.search(r"of Italy's mainland provinces that month (\d+) are the Allies' \((\d+) of them touching Germany's home ground\), (\d+) Germany's",g[2] or '')
            italy[seed]=dict(signed=None if g[1]=='never' else t(*g[1].split()), west=None if g[3]=='never' else t(g[4],g[5]), detail=g[6] or '', frm=frm.group(1) if frm else '', gap=g[7], sov=None if g[8]=='never' else t(*g[8].split()), before=int(g[9]), fell=t(g[11],g[12]) if g[11] else None, prov=tuple(int(x) for x in prov.groups()) if prov else None)
seeds=sorted({s['seed'] for s in ship}|set(italy))
print(f"{len(files)} files, {len(seeds)} wars (seeds {min(seeds)} to {max(seeds)}), {len(ship)} groups that came by ship; error lines {errs}")
print("\nRULING 92: corps of Britain, America and Canada that came to the continent by ship, six months on")
print("   (dead split: IN PLACE the province of arrival / BEHIND other ground its side held at arrival / OUT ground the enemy held at arrival; each as cut off + in supply)")
def row(label,rs):
    n=sum(r['done'] for r in rs); d=sum(r['dead'] for r in rs); w=[sum(r['w'][i] for r in rs) for i in range(6)]
    wars=len({r['seed'] for r in rs}); lw=len({r['seed'] for r in rs if r['dead']})
    print(f"   {label}: {len(rs)} groups in {wars} wars, corps {n}, dead {d} (in {lw} wars)" + (f"; in place {w[0]}+{w[1]}, behind {w[2]}+{w[3]}, marched out {w[4]}+{w[5]}" if d else ''))
for kind in ('landing','ferry','ferry+'):
    rs=[r for r in ship if r['kind']==kind]
    if not rs: continue
    print(f" {kind}:")
    row('all flags, all years',rs)
    for nat in ('UK','USA','CAN'):
        a=[r for r in rs if r['nat']==nat]
        if a: row(f'{nat}',a)
    for lab,lo,hi in (('1939 to 1940',1939,1940),('1941 to 1942',1941,1942),('1943',1943,1943),('1944',1944,1944),('1945 on',1945,1999)):
        a=[r for r in rs if lo<=r['yr']<=hi]
        if a: row(f'arrived {lab}',a)
    late=[r for r in rs if r['yr']>=1941]
    owns={}
    for r in late: owns.setdefault(r['own'],[]).append(r)
    for own,a in sorted(owns.items(),key=lambda kv:-sum(r['done'] for r in kv[1])): row(f'from 1941, on {own} ground',a)
if listn:
    print(f"\n groups that lost {listn} corps or more:")
    for r in sorted(ship,key=lambda r:(-r['dead'],r['seed'],r['t'])):
        if r['dead']>=listn: print('   '+r['line'][:330])
if italy:
    print("\nRULING 94: the Italian road")
    road=[]; other=[]
    for s in sorted(italy):
        i=italy[s]
        fromita = i['west'] is not None and re.match(r"ITA \d+",i['frm'] or '') is not None and all(p.strip().startswith('ITA') for p in i['frm'].split(',')[:1])
        mixed = i['west'] is not None and 'ITA' in (i['frm'] or '')
        (road if mixed else other).append(s)
        print(f"   seed {s}: Italy signed {name(i['signed']) if i['signed'] else 'never'}" + (f" (mainland: Allies {i['prov'][0]}, of them touching Germany {i['prov'][1]}, Germany's side {i['prov'][2]})" if i['prov'] else '') + f"; first western corps on German home ground {name(i['west']) if i['west'] else 'never'}" + (f", the month before on: {i['frm']}; gap {i['gap']} months" if i['west'] else '') + f"; first Soviet {name(i['sov']) if i['sov'] else 'never'}; Germany fell {name(i['fell']) if i['fell'] else 'never'}; western months there before May 1944: {i['before']}")
    def mid(xs): return name(statistics.median_low(sorted(xs))) if xs else 'none'
    print(f"\n   1. western corps first enter Germany's home ground FROM ITALIAN GROUND in {len(road)} of {len(italy)} wars: seeds {road}")
    ms={}
    for s in road: ms.setdefault(name(italy[s]['west']),[]).append(s)
    print("      months: " + '; '.join(f"{k} (seeds {v})" for k,v in sorted(ms.items(),key=lambda kv:t(*kv[0].split()))))
    yrs={}
    for s in road: yrs[name(italy[s]['west']).split()[1]]=yrs.get(name(italy[s]['west']).split()[1],0)+1
    print("      by year: "+', '.join(f"{y} in {n}" for y,n in sorted(yrs.items())))
    gaps=sorted(int(italy[s]['gap']) for s in road if italy[s]['gap'].lstrip('-').isdigit())
    if gaps: print(f"   2. months from Italy's signing to the first western corps on Germany's home ground, in those wars: {gaps}; middle {statistics.median_low(gaps)}")
    fr=[italy[s]['fell'] for s in road if italy[s]['fell']]; fo=[italy[s]['fell'] for s in other if italy[s]['fell']]
    if fr and fo:
        print(f"   3. Germany falls, the middle war: by the Italian road {mid(fr)} ({len(fr)} wars); the others {mid(fo)} ({len(fo)} wars); difference {statistics.median_low(sorted(fo))-statistics.median_low(sorted(fr))} months (means {statistics.mean(fr):.1f} against {statistics.mean(fo):.1f}: {statistics.mean(fo)-statistics.mean(fr):+.1f})")
    print("      the others' first western corps on German home ground: " + '; '.join(f"seed {s}: {name(italy[s]['west']) if italy[s]['west'] else 'never'} from {italy[s]['frm'] or '-'}" for s in other))
    bef=[s for s in italy if italy[s]['before']>0]
    print(f"   western corps on Germany's home ground before May 1944 in {len(bef)} wars: seeds {bef}")
