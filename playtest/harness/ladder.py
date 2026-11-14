#!/usr/bin/env python3
"""Berlin by habit: one line a habit from probe28 TOTAL lines, and the habits paired seed by seed.
usage: ladder.py DIR HABIT... [--seeds A-B]   (files DIR/HABIT-GER-SEED.txt)"""
import re, sys, glob, os, statistics as st
args = sys.argv[1:]
lo, hi = 1, 9999
if '--seeds' in args:
    i = args.index('--seeds'); lo, hi = map(int, args[i+1].split('-')); del args[i:i+2]
d, habits = args[0], args[1:]
def read(h):
    out = {}
    for f in glob.glob(os.path.join(d, f'{h}-GER-*.txt')):
        seed = int(re.search(r'-(\d+)\.txt$', f).group(1))
        if not lo <= seed <= hi: continue
        t = [l for l in open(f) if l.startswith('TOTAL')]
        if not t: print('NO TOTAL', f); continue
        t = t[0]
        m = re.search(r'\(fell (\d+)\)', t)
        r = {'fell': int(m.group(1)) if m else 77, 'berlin': 'Berlin in December 1944: GER' in t}
        m = re.search(r'defences (\d+), lost (\d+), on a prepared line (\d+) \(lost (\d+)\), met tired (\d+), against guns (\d+)', t)
        if m: r.update(dict(zip(['defs', 'lost', 'prep', 'prepLost', 'tired', 'guns'], map(int, m.groups()))))
        m = re.search(r'rings closed on the player (\d+) \((\d+) men\), surrenders (\d+) \((\d+) men\)', t)
        if m: r.update(dict(zip(['rings', 'ringMen', 'gave', 'gaveMen'], map(int, m.groups()))))
        m = re.search(r'longest stretch at war with no battle of the player\'s: (\d+) months', t)
        if m: r['quiet'] = int(m.group(1))
        m = re.search(r'line corps in works: (.*)$', t)
        if m:
            r['line'] = {k.strip(): tuple(map(int, v.split('/'))) for k, v in (x.rsplit(' ', 1) for x in m.group(1).split(', ') if ' ' in x)}
        m = re.search(r'my field attacks by year (.*?); provinces', t)
        if m: r['att'] = {int(a): int(b) for a, b in (x.split() for x in m.group(1).split(', '))}
        out[seed] = r
    return out
data = {h: read(h) for h in habits}
mon = lambda t: 'stands' if t >= 77 else f"{['Sep','Oct','Nov','Dec','Jan','Feb','Mar','Apr','May','Jun','Jul','Aug'][(t-1)%12]} {1939+(t+7)//12}"
for h in habits:
    r = data[h]; n = len(r)
    if not n: continue
    fell = sorted(x['fell'] for x in r.values())
    held = sum(x['berlin'] for x in r.values())
    s = f"{h:10s} wars {n:3d} | Berlin held to end 1944: {held:2d} ({100*held/n:.0f}%) | falls: median {mon(int(st.median(fell)))}, quarter by {mon(fell[n//4])}, three quarters by {mon(fell[(3*n)//4])}, stands Dec 1945 {sum(f>=77 for f in fell)}"
    if all('defs' in x for x in r.values()):
        D = sum(x['defs'] for x in r.values()); L = sum(x['lost'] for x in r.values()); P = sum(x['prep'] for x in r.values()); PL = sum(x['prepLost'] for x in r.values())
        s += f" | defences {D/n:.0f} a war, lost {100*L/D:.0f}%, on a prepared line {100*P/D:.0f}% (lost {100*PL/max(1,P):.0f}%), tired {100*sum(x['tired'] for x in r.values())/D:.0f}%, guns {100*sum(x['guns'] for x in r.values())/D:.0f}%"
        s += f" | surrendered {sum(x['gaveMen'] for x in r.values())/n/1000:.0f}k men a war in {sum(x['gave'] for x in r.values())/n:.1f}; rings {sum(x['rings'] for x in r.values())/n:.1f}"
    if all('quiet' in x for x in r.values()):
        q = sorted(x['quiet'] for x in r.values()); s += f" | longest silence median {st.median(q):.0f}, 7 months or more in {sum(v>=7 for v in q)}"
    print(s)
    if all('line' in x for x in r.values()):
        keys = ['Jan 1942', 'Jul 1942', 'Jan 1943', 'Jul 1943', 'Jan 1944', 'Jul 1944']
        parts = []
        for k in keys:
            xs = [x['line'][k] for x in r.values() if k in x['line'] and x['line'][k][1] > 0]
            if xs: parts.append(f"{k}: {100*sum(a for a,b in xs)/sum(b for a,b in xs):.0f}% of {sum(b for a,b in xs)/len(xs):.0f} corps ({len(xs)} wars)")
        print('           line in works  ' + '; '.join(parts))
    if all('att' in x for x in r.values()):
        print('           attacks a war  ' + ', '.join(f"{y} {sum(x['att'].get(y,0) for x in r.values())/n:.0f}" for y in range(1939, 1946)))
print()
for i, a in enumerate(habits):
    for b in habits[i+1:]:
        common = sorted(set(data[a]) & set(data[b]))
        if not common: continue
        both = sum(data[a][s]['berlin'] and data[b][s]['berlin'] for s in common); oa = sum(data[a][s]['berlin'] and not data[b][s]['berlin'] for s in common); ob = sum(data[b][s]['berlin'] and not data[a][s]['berlin'] for s in common)
        later = sum(data[b][s]['fell'] > data[a][s]['fell'] for s in common); sooner = sum(data[b][s]['fell'] < data[a][s]['fell'] for s in common)
        diff = st.median(data[b][s]['fell'] - data[a][s]['fell'] for s in common)
        print(f"{a} against {b} on {len(common)} seeds: Berlin held by both {both}, only {a} {oa}, only {b} {ob}, neither {len(common)-both-oa-ob}; {b} lasts longer on {later}, shorter on {sooner}, median {diff:+.1f} months")
