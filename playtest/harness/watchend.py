#!/usr/bin/env python3
"""The war with nobody playing: when Germany fell, by seed (WATCH lines of probe59). watchend.py DIR [DIR...] [--seeds A-B]"""
import sys,re,glob,statistics
M=['January','February','March','April','May','June','July','August','September','October','November','December']
def t(m,y): return (int(y)-1939)*12+M.index(m)-8+1   # turn 1 = September 1939
def name(n):
    n=int(n); k=n-1+8; return f"{M[k%12]} {1939+k//12}"
args=[a for a in sys.argv[1:] if not a.startswith('--')]
lo,hi=1,999
if '--seeds' in sys.argv:
    lo,hi=map(int,sys.argv[sys.argv.index('--seeds')+1].split('-')); args=[a for a in args if '-' not in a or '/' in a]
for d in args:
    wars={}
    for f in glob.glob(d+'/watch49s3-*.txt'):
        for l in open(f):
            m=re.match(r'WATCH seed (\d+) stop 3',l)
            if not m: continue
            s=int(m.group(1))
            if not lo<=s<=hi: continue
            g=re.search(r'GER (\w+) (\d{4})',l.split('fell:')[1].split('|')[0]) if 'fell:' in l else None
            dec=re.search(r'\| decided (\w+) (\d{4})',l)
            wars[s]=(t(*g.groups()) if g else None, t(*dec.groups()) if dec else None, 'NOT DECIDED' in l, l)
    falls=sorted(v[0] for v in wars.values() if v[0])
    print(f"== {d}: {len(wars)} wars, seeds {min(wars)} to {max(wars)}")
    print(f"   Germany fell in {len(falls)} of {len(wars)}; not decided by December 1949 in {sum(1 for v in wars.values() if v[2])}")
    if falls:
        allf=sorted([v[0] if v[0] else 999 for v in wars.values()])
        med=statistics.median(allf)
        print(f"   middle war (a war where Germany never fell counted last): {'never' if med>=999 else name(round(med))}; among the wars where it fell: {name(round(statistics.median(falls)))}; earliest {name(falls[0])}, latest {name(falls[-1])}")
        may45=t('May','1945')
        print(f"   within three months of May 1945 (February to August 1945): {sum(1 for x in falls if abs(x-may45)<=3)}; earlier than February 1945: {sum(1 for x in falls if x<may45-3)}; later than August 1945 or never: {sum(1 for x in allf if x>may45+3)}")
    for s in sorted(wars):
        v=wars[s]; print(f"   seed {s:2d}: Germany {'fell '+name(v[0]) if v[0] else 'did not fall'}{'' if not v[2] else ' (war not decided)'}")
