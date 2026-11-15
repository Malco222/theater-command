#!/usr/bin/env python3
# usage: westtab.py DIR   (WEST lines of probe61: w70-SEED.txt and w71-SEED.txt)
import re,os,sys,statistics as st
d=sys.argv[1]
for v in ['70','71']:
    rows=[]
    for s in range(1,200):
        f=f'{d}/w{v}-{s}.txt'
        if not os.path.exists(f) or not os.path.getsize(f): continue
        t=open(f).read()
        m=re.findall(r'(January 1942|January 1943|June 1943|December 1943|May 1944): German corps there (\d+) of (\d+), held by ([^,]*), British corps (\d+) \(at home (\d+), in a home port (\d+)\), Germany (\w+)',t)
        if len(m)<5: print('short',f); continue
        land=re.search(r'landings from 1942: (.*?) \| landing log',t).group(1)
        rows.append((s,m,land))
    if not rows: continue
    print(f'Version {v}: {len(rows)} wars')
    for k,name in enumerate(['January 1942','January 1943','June 1943','December 1943','May 1944']):
        alive=[r for r in rows if r[1][k][7]=='stands']
        allc=[int(r[1][k][2]) for r in alive]; west=[int(r[1][k][1]) for r in alive]
        print(f'   {name}: Germany stands in {len(alive)}; German corps in all, middle {st.median(allc) if allc else "-"}; in France and the Low Countries, middle {st.median(west) if west else "-"}, none at all in {sum(1 for w in west if w==0)}')
    yrs={}
    for s,m,land in rows:
        ws=[x for x in land.split('; ') if re.search(r'landing at',x)]
        first=next((x for x in ws if not re.search(r'Rhodes|Sardinia|Sirte|Sfax|Sicily|Crete|Tobruk|Benghazi|Derna|Bardia|Tripoli|Corsica|Calabria|Bergen|Oslo|Narvik|Trondheim',x)),None)
        y=first.split(' landing')[0] if first else 'none by May 1944'
        yrs[y]=yrs.get(y,0)+1
    print('   first landing in the west from 1942 on: '+', '.join(f'{k} {n}' for k,n in sorted(yrs.items(), key=lambda kv:(kv[0][-4:],kv[0]))))
    if '--rows' in sys.argv:
        for s,m,land in rows: print('   ',s,' | '.join(f"{a[1]}/{a[2]}{'' if a[7]=='stands' else ' fallen'}" for a in m),'|',land[:160])
