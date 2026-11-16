#!/usr/bin/env python3
"""Version 81's measure (probe83 FOE lines): a computer-led enemy in the Americas, nobody playing.
usage: foetab.py GLOB [--base GLOB_OF_none]   e.g. foetab.py 'out/v81/foe-*.txt'"""
import sys, re, glob, statistics
M=['January','February','March','April','May','June','July','August','September','October','November','December']
def t(s):
    m=re.match(r'(\w+) (\d{4})',s or '')
    return (int(m.group(2))-1939)*12+M.index(m.group(1))-8+1 if m else None
def name(n):
    n=int(round(n)); k=n-1+8; return f"{M[k%12]} {1939+k//12}"
def yr(n): return 1939+(n+7)//12
rows={}
for f in sorted(f for a in sys.argv[1:] if not a.startswith('--') for f in glob.glob(a)):
    for l in open(f):
        m=re.match(r'FOE (\w+) seed (\d+): declared ([^;]*); (?:first American corps on its soil ([^;]*); \w+ (fell (\w+ \d{4}) \((-?\d+) months after declaring\)|STANDS); )?America at war with Germany ([^;]*); first corps across the ocean ([^;]*); first ground on the continent ([^;]*); ashore in the west to stay ([^;]*); Germany (fell (\w+ \d{4})|STANDS); American field corps the month Germany fell (\d+), at home (\d+); at the end \(([^)]*)\) (\d+), at home (\d+), on \w+\'s soil (\d+)(.*)',l)
        if not m: continue
        g=m.groups()
        rows.setdefault(g[0],{})[int(g[1])]=dict(on=t(g[3]),fell=t(g[5]),gap=int(g[6]) if g[6] else None,across=t(g[8]),first=g[9].strip(),west=t(g[10]),ger=t(g[12]),nF=int(g[13]),homeF=int(g[14]),end=g[15],n=int(g[16]),home=int(g[17]),onfoe=int(g[18]),err='ERROR' in g[19])
base=rows.get('none',{})
for foe in sorted(rows):
    R=rows[foe]; seeds=sorted(R)
    print(f"== {foe}: {len(R)} wars (seeds {seeds})" + (f"; ERRORS {sum(r['err'] for r in R.values())}" if any(r['err'] for r in R.values()) else ''))
    if foe!='none':
        fell=[r for r in R.values() if r['fell']]
        gaps=sorted(r['gap'] for r in fell)
        print(f"   {foe} falls in {len(fell)} of {len(R)}" + (f": months after declaring {gaps}; first American corps on its soil (the month that opens with them there): {sorted(set(name(r['on']) for r in R.values() if r['on']))}" if fell else f"; American corps ever on its soil in {sum(1 for r in R.values() if r['on'])}"))
    ws=[r['west'] for r in R.values()]
    by={}
    for w in ws: by['never' if not w else str(yr(w))]=by.get('never' if not w else str(yr(w)),0)+1
    print(f"   Americans ashore in the west to stay: " + ', '.join(f"{k} in {v}" for k,v in sorted(by.items())) + f"; by the end of 1944 in {sum(1 for w in ws if w and yr(w)<=1944)} of {len(R)}")
    gs=sorted(r['ger'] for r in R.values() if r['ger'])
    med=lambda x: (x[len(x)//2-1]+x[len(x)//2])/2 if len(x)%2==0 else x[len(x)//2]
    print(f"   Germany falls in {len(gs)} of {len(R)}; middle {name(med(gs)) if gs else '-'}" + (f" (from {name(gs[0])} to {name(gs[-1])})" if gs else ''))
    if foe!='none' and base:
        d=[R[s]['ger']-base[s]['ger'] for s in seeds if s in base and R[s]['ger'] and base[s]['ger']]
        if d: print(f"   against the same seeds with no such enemy: Germany falls {statistics.mean(d):+.2f} months (later in {sum(x>0 for x in d)}, earlier in {sum(x<0 for x in d)}, same in {sum(x==0 for x in d)}); " + ' '.join(f"{s}:{R[s]['ger']-base[s]['ger']:+d}" for s in seeds if s in base and R[s]['ger'] and base[s]['ger']))
        dw=[(s,R[s]['west'],base[s]['west']) for s in seeds if s in base]
        print("   ashore to stay, with / without: " + '; '.join(f"{s}: {name(a) if a else 'never'} / {name(b) if b else 'never'}" for s,a,b in dw))
    print(f"   American field corps the month Germany fell: middle {statistics.median([r['nF'] for r in R.values()])}, at home {statistics.median([r['homeF'] for r in R.values()])}; first corps across the ocean: {sorted(set(name(r['across']) for r in R.values() if r['across']))}")
