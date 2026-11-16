#!/usr/bin/env python3
"""Every staff landing in the wars with nobody playing, from the LAND and FIRST lines of probe78.
For each nation: its first landing (in how many wars, the middle month, on whose ground), its first on the western
mainland, and what all its landings cost within six months. For the United States also the landings in France by year.
usage: landtab.py GLOB... [--nats UK,USA,CAN,BRA]"""
import re, sys, glob, statistics, collections
M=['January','February','March','April','May','June','July','August','September','October','November','December']
def turn(mon, yr): return (int(yr)-1939)*12+M.index(mon)-8+1
def label(t): k=int(t)+8-1; return f'{M[k%12]} {1939+k//12}'
args=[a for a in sys.argv[1:] if not a.startswith('--')]
nats=next((a.split('=')[1].split(',') for a in sys.argv[1:] if a.startswith('--nats=')), ['UK','USA','CAN','BRA','ITA','GER','SOV'])
lands=[]; firsts=[]; wars=0
for pat in args:
    for f in sorted(glob.glob(pat)):
        for l in open(f):
            if 'ERROR' in l: print('ERROR in', f)
            m=re.match(r'LAND seed (\d+): (\w+) (\w+) (\d{4}) at (.+?) \((\w+)\)( WEST)? \| corps (\d+) \| (first|later) wave(, warned)?(, thrown back)? \| six months on: (?:dead (\d+) of \d+, beach held by (\w+)|.*)', l)
            if m: lands.append(dict(seed=int(m[1]), nat=m[2], t=turn(m[3],m[4]), yr=int(m[4]), name=m[5], owner=m[6], west=bool(m[7]), n=int(m[8]), first=m[9]=='first', warned=bool(m[10]), back=bool(m[11]), dead=None if m[12] is None else int(m[12]), held=m[13]))
            if l.startswith('FIRST '): wars+=1
print(f'{wars} wars, {len(lands)} landings told by the staffs ({sum(1 for l in lands if l["first"])} first waves)')
AXIS={'GER','ITA','FIN','HUN','ROM','BUL','JAP'}
for n in nats:
    mine=[l for l in lands if l['nat']==n and l['first']]
    if not mine: print(f'   {n}: no landing of its own in any war'); continue
    by=collections.defaultdict(list)
    for l in mine: by[l['seed']].append(l)
    f=[sorted(v,key=lambda x:x['t'])[0] for v in by.values()]
    w=[sorted([x for x in v if x['west']],key=lambda x:x['t'])[0] for v in by.values() if any(x['west'] for x in v)]
    c=collections.Counter(x['owner'] for x in f).most_common(); cw=collections.Counter(x['owner'] for x in w).most_common()
    places=collections.Counter(x['name'] for x in f).most_common(4)
    print(f'   {n}: a landing of its own in {len(f)} of {wars} wars; the first in the middle war {label(statistics.median(x["t"] for x in f))} (from {label(min(x["t"] for x in f))} to {label(max(x["t"] for x in f))}); on whose ground: {", ".join(f"{k} {v}" for k,v in c)}; places: {", ".join(f"{k} {v}" for k,v in places)}')
    if w: print(f'      first on the western mainland: in {len(w)} wars, the middle one {label(statistics.median(x["t"] for x in w))}; {", ".join(f"{k} {v}" for k,v in cw)}; by year: {", ".join(f"{k} in {v}" for k,v in sorted(collections.Counter(x["yr"] for x in w).items()))}')
    else: print('      never on the western mainland')
    allm=[l for l in lands if l['nat']==n and l['dead'] is not None]
    if allm: print(f'      all its landings followed six months ({len(allm)}, later waves too): corps dead {sum(l["dead"] for l in allm)} of {sum(l["n"] for l in allm)}; the beach in Axis hands again after {sum(1 for l in allm if l["held"] in AXIS)}')
    if n=='USA':
        for yr in sorted({l['yr'] for l in mine if l['owner']=='FRA'}):
            fr=[l for l in mine if l['owner']=='FRA' and l['yr']==yr and l['dead'] is not None]
            if fr: print(f'      first waves in France in {yr}: {len(fr)} in {len({l["seed"] for l in fr})} wars; corps dead in six months {sum(l["dead"] for l in fr)} of {sum(l["n"] for l in fr)}; beaches not in Axis hands six months on {sum(1 for l in fr if l["held"] not in AXIS)}; worst: ' + '; '.join(f'seed {l["seed"]} {label(l["t"])} {l["name"]} {l["dead"]} of {l["n"]}' for l in sorted(fr,key=lambda x:-x["dead"])[:3]))
