#!/usr/bin/env python3
"""How a town falls: the table from probe90's FELL and SIEGE lines. usage: lentab.py 'GLOB' [--label TEXT]"""
import sys, glob, re, statistics as st
from collections import Counter
args = sys.argv[1:]; label = ''
if '--label' in args: i = args.index('--label'); label = args[i + 1]; del args[i:i + 2]
files = sorted(f for a in args for f in glob.glob(a))
fell, siege, fieldb, peak, reached = {}, {}, {}, {}, {}
for f in files:
    for ln in open(f):
        m = re.match(r'FELL seed (\d+): (\S+) falls (\w+ \d+) to (\w+): (\w+)(?: \(([^)]*)\))?(?: at ([\d.]+) by (\d+) corps on (\d+))? \| the month before: (not besieged|besieged), garrison (\d+) corps (\d+) divs str ([\d.]+) org ([\d.]+), guard ([\d.]+), field (\d+), outside (\d+) corps (\d+) divs, shut (\d+) of (\d+), by sea (\w+), ring (\w+), breach ([\d.]+), roads cut (\d+) of (\d+), next door ours (\d+) foes (\d+) \| so far: besieged (\d+) months, storms (\d+), sorties (\d+), reliefs (\d+)', ln)
        if m:
            s = int(m[1])
            if s in fell: continue  # the first fall of each war
            fell[s] = dict(when=m[3], to=m[4], how=m[5], odds=float(m[7]) if m[7] else None, by=int(m[8]) if m[8] else None, bes=m[10] == 'besieged', garr=int(m[11]), gdivs=int(m[12]), guard=float(m[15]), field=int(m[16]),
                           out=int(m[17]), odivs=int(m[18]), shut=int(m[19]), stores=int(m[20]), sea=m[21], ring=m[22], breach=float(m[23]), cut=int(m[24]), roads=int(m[25]), ours=int(m[26]), foes=int(m[27]), months=int(m[28]), storms=int(m[29]), sorties=int(m[30]), reliefs=int(m[31]))
        m = re.match(r'L seed (\d+) (\w+ \d+): held by SOV.*?\| (\w+)>SOV ([\d.]+) field won, town holds.*?\((\d+)v(\d+)\)', ln)
        if m and int(m[1]) not in fieldb: fieldb[int(m[1])] = dict(when=m[2], odds=float(m[4]), att=int(m[5]), dfn=int(m[6]))
        m = re.match(r'L seed (\d+) (\w+ \d+): held by SOV.*?field (\d+) \|.*?roads cut (\d+)', ln)
        if m and int(m[4]) > 0: peak[int(m[1])] = max(peak.get(int(m[1]), 0), int(m[3])); reached.setdefault(int(m[1]), m[2])
        m = re.match(r'SIEGE seed (\d+): (\S+) \(([^)]*)\) besieged (\d+) months in (\d+) sieges(?: from (\w+ \d+))?, storms (\d+) \(won (\d+)\), sorties (\d+), reliefs (\d+) \| fell (.*?) \| back (.*)', ln)
        if m: siege[int(m[1])] = dict(town=m[2], kind=m[3], months=int(m[4]), sieges=int(m[5]), first=m[6], storms=int(m[7]), won=int(m[8]), sorties=int(m[9]), reliefs=int(m[10]), fell=m[11], back=m[12].strip())
n = len(siege)
if not n: sys.exit('no SIEGE lines')
town = next(iter(siege.values()))['town']; kind = next(iter(siege.values()))['kind']
print(f'HOW {town.upper()} FALLS{": " + label if label else ""} ({n} wars)'); print(f'  the place: {kind}')
bes = [s for s, v in siege.items() if v['months'] > 0]
print(f'  the enemy closes round it in {len(bes)} wars of {n}; it falls in {len(fell)}; holds through a siege in {len([s for s in bes if s not in fell])}; never reached in {n - len(bes) - len([s for s in fell if s not in bes])}')
hows = Counter(v['how'] for v in fell.values())
print('  how it falls: ' + ', '.join(f'{k.lower()} {c}' for k, c in hows.most_common()))
if fell:
    med = lambda xs: st.median(xs) if xs else 0
    v = list(fell.values())
    print(f'  the month before it falls: a garrison inside in {sum(1 for x in v if x["garr"] > 0)} of {len(v)} (corps inside: {dict(Counter(x["garr"] for x in v))}); the town guard {med([x["guard"] for x in v]):.1f} divisions; Soviet corps in the field of the province {dict(Counter(x["field"] for x in v))}; Soviet corps in the provinces next door, middle {med([x["ours"] for x in v])} (least {min(x["ours"] for x in v)}, most {max(x["ours"] for x in v)})')
    print(f'  under siege before the fall: middle {med([x["months"] for x in v])} months (most {max(x["months"] for x in v)}); stores for {med([x["stores"] for x in v])} months, shut for {med([x["shut"] for x in v])}; fed by sea in {sum(1 for x in v if x["sea"] == "fed")}; relief attacks before the fall {sum(x["reliefs"] for x in v)}, sorties {sum(x["sorties"] for x in v)}')
    fb = [fieldb[s] for s in fell if s in fieldb]
    if fb: print(f'  the fight for the field outside the walls, before the siege ({len(fb)} wars): the defenders in the field {dict(Counter(x["dfn"] for x in fb))} corps, attacked by middle {med([x["att"] for x in fb])} corps at odds middle {med([x["odds"] for x in fb]):.2f}')
    pk = [peak.get(s, 0) for s in fell]
    print(f'  the most Soviet corps ever standing in the province while the enemy was next door, in the wars it fell: {dict(sorted(Counter(pk).items()))}')
    so = [x for x in v if x['odds']]
    if so: print(f'  the storm: odds middle {med([x["odds"] for x in so]):.2f} (least {min(x["odds"] for x in so):.2f}, most {max(x["odds"] for x in so):.2f}), by {med([x["by"] for x in so])} corps; the besiegers the month before: {med([x["out"] for x in so])} corps, {med([x["odivs"] for x in so])} divisions')
    print(f'  taken by: {dict(Counter(x["to"] for x in v))}')
print('  storms on the town in all wars: %d, won %d' % (sum(v['storms'] for v in siege.values()), sum(v['won'] for v in siege.values())))
print()
print('  seed  besieged  first siege      fell (how)                          the month before                                   back')
for s in sorted(siege):
    v = siege[s]; x = fell.get(s)
    before = f'garrison {x["garr"]}, guard {x["guard"]:.1f}, field {x["field"]}, next door {x["ours"]} v {x["foes"]}, outside {x["out"]} corps' + (f', storm {x["odds"]:.2f}' if x['odds'] else '') if x else ''
    print(f'  {s:>4}  {v["months"]:>3} mo    {v["first"] or "-":<15}  {v["fell"]:<34}  {before:<50} {v["back"]}')
