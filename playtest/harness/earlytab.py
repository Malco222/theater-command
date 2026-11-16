#!/usr/bin/env python3
"""The table of probe80's EARLY lines (Version 78, ruling 58a): by mode, how many wars have America among the Allies,
at war with Germany and with Italy by December 1941, the December 1941 event fired, the small Allies in, first corps
across, ashore in the west to stay, and when Germany falls; paired by seed against the base mode.
usage: earlytab.py DIR   (reads DIR/np-MODE-*.txt)"""
import sys,re,glob,statistics
MON='January February March April May June July August September October November December'.split()
def turn(s):
    m=re.match(r'(\w+) (\d{4})',s or '')
    return (int(m.group(2))-1939)*12+MON.index(m.group(1))-8+1 if m else 0
def date(t): t=int(round(t)); y=1939+(t+7)//12; return f"{MON[(t+7)%12]} {y}"
d=sys.argv[1]; wars={}
for f in sorted(glob.glob(f'{d}/np-*.txt')):
    for l in open(f):
        m=re.match(r'EARLY (\w+) seed (\d+): (.*)',l)
        if not m: continue
        mode,seed,rest=m.group(1),int(m.group(2)),m.group(3)
        g=lambda pat: (re.search(pat,rest) or [None,''])[1]
        w=dict(ally=turn(g(r'America among the Allies ([\w ]+?);')),ger=turn(g(r'at war with Germany ([\w ]+?),')),ita=turn(g(r'with Italy ([\w ]+?);')),
               pearl=turn(g(r'December 1941 event ([\w ]+?);')),full=turn(g(r'all on war work ([\w ]+?);')),across=turn(g(r'first corps across the ocean ([\w ]+?);')),
               first=g(r'first ground on the continent (\w+)'),firstT=turn(g(r'first ground on the continent \w+ ([\w ]+?);')),west=turn(g(r'ashore in the west to stay ([\w ]+?);')),
               cub=turn(g(r'Cuba at war with Germany ([\w ]+?),')),mex=turn(g(r'Mexico ([\w ]+?),')),bra=turn(g(r'Brazil ([\w ]+?);')),brafell='Brazil fell' in rest,
               fell=turn(g(r'Germany fell ([\w ]+?);')),err='ERROR' in rest)
        wars.setdefault(mode,{})[seed]=w
def mid(xs): xs=[x for x in xs if x]; return date(statistics.median(xs)) if xs else 'never'
for mode,ws in wars.items():
    n=len(ws); v=list(ws.values()); c=lambda f: sum(1 for w in v if f(w))
    print(f"{mode}: {n} wars (seeds {min(ws)} to {max(ws)}), errors {c(lambda w:w['err'])}")
    print(f"   America among the Allies: {c(lambda w:w['ally'])} (middle {mid([w['ally'] for w in v])}); at war with Germany by December 1941: {c(lambda w:0<w['ger']<=28)}; with Italy by December 1941: {c(lambda w:0<w['ita']<=28)}; the December 1941 event fired: {c(lambda w:w['pearl'])}")
    print(f"   all on war work: middle {mid([w['full'] for w in v])}; first corps across the ocean: {c(lambda w:w['across'])} (middle {mid([w['across'] for w in v])}); first ground on the continent: "+', '.join(f"{k} {sum(1 for w in v if w['first']==k)}" for k in sorted({w['first'] for w in v}))+f" (middle {mid([w['firstT'] for w in v])})")
    print(f"   ashore in the west to stay: {c(lambda w:w['west'])} (middle {mid([w['west'] for w in v])}; before June 1944 in {c(lambda w:0<w['west']<58)}); Cuba at war with Germany: {c(lambda w:w['cub'])} (middle {mid([w['cub'] for w in v])}), Mexico {c(lambda w:w['mex'])} ({mid([w['mex'] for w in v])}), Brazil {c(lambda w:w['bra'])} ({mid([w['bra'] for w in v])}); Brazil fell in {c(lambda w:w['brafell'])}")
    f=[w['fell'] for w in v if w['fell']]
    print(f"   Germany fell in {len(f)} of {n}: middle {mid(f)}, earliest {date(min(f)) if f else '-'}, latest {date(max(f)) if f else '-'}; February to August 1945 in {sum(1 for x in f if 66<=x<=72)}, earlier in {sum(1 for x in f if x<66)}, later in {sum(1 for x in f if x>72)}")
    if mode!='base' and 'base' in wars:
        pr=[(ws[s]['fell']-wars['base'][s]['fell']) for s in ws if s in wars['base'] and ws[s]['fell'] and wars['base'][s]['fell']]
        if pr: print(f"   against the same {len(pr)} wars without it: Germany falls {statistics.mean(pr):+.2f} months (middle {statistics.median(pr):+.1f}); earlier in {sum(1 for x in pr if x<0)}, later in {sum(1 for x in pr if x>0)}, the same in {sum(1 for x in pr if x==0)}; biggest moves {min(pr):+d} and {max(pr):+d}")
