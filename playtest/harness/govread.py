#!/usr/bin/env python3
"""Reads the government wars (probe24 output): every directive with what stood against it and how it ended.
usage: govread.py DIR [directives|totals|trust]"""
import re, sys, glob, os, collections
d = sys.argv[1]; what = sys.argv[2] if len(sys.argv) > 2 else 'directives'
MON = ['January','February','March','April','May','June','July','August','September','October','November','December']
def turn(s):
    m, y = s.split(); return (int(y) - 1939) * 12 + MON.index(m) - 8 + 1
rows = []
for f in sorted(glob.glob(os.path.join(d, '*.txt'))):
    name = os.path.basename(f)[:-4]
    if name in ('totals', 'errors'): continue
    cur = None
    for line in open(f):
        m = re.match(r'^(\w+ \d{4})  DIRECTIVE (\w+) from (.+?): (.+?); (\d+) months; reward (.+?); price if missed ([\d.]+)(?: \[(.*)\])?', line)
        if m:
            cur = dict(file=name, at=m.group(1), t=turn(m.group(1)), kind=m.group(2), by=m.group(3), title=m.group(4), months=int(m.group(5)), reward=m.group(6), price=float(m.group(7)), extra=m.group(8) or '', end='open', took=None)
            x = cur['extra']
            g = re.match(r'(\w+)(?: fort (\d))?( CAPITAL)? of (\w+); my provinces beside it (\d+), my corps beside it (\d+), land hops for my nearest corps (-?\d+); defenders (\d+) divs; my strength beside it (\d+) divs', x)
            if g: cur.update(size=g.group(1), fort=int(g.group(2) or 0), cap=bool(g.group(3)), holder=g.group(4), nbprov=int(g.group(5)), nbcorps=int(g.group(6)), hops=int(g.group(7)), foes=int(g.group(8)), mine=int(g.group(9)))
            h = re.match(r'(\w+)(?: fort (\d))?; (under siege|threatened); my troops there (\d+) divs', x)
            if h: cur.update(size=h.group(1), fort=int(h.group(2) or 0), state=h.group(3), there=int(h.group(4)))
            a = re.match(r'army now (\d+) divs, target (\d+); treasury (-?\d+), net (-?\d+)', x)
            if a: cur.update(army=int(a.group(1)), target=int(a.group(2)), treasury=int(a.group(3)), net=int(a.group(4)))
            rows.append(cur); continue
        m = re.match(r'^\s+(\w+ \d{4})  => (.+?)(?: \(\?\))?$', line)
        if m and cur: cur['end'] = m.group(2); cur['took'] = turn(m.group(1)) - cur['t']; cur = None
if what == 'directives':
    for r in rows:
        if r['kind'] in ('take', 'relief') and 'holder' in r:
            print(f"{r['file']:22} {r['at']:15} {r['kind']:6} {r['title'][:34]:34} {r['size']:8}{' F'+str(r['fort']) if r['fort'] else '   '}{' CAP' if r['cap'] else '    '} of {r['holder']:4} nb-prov {r['nbprov']} nb-corps {r['nbcorps']} hops {r['hops']:2} foes {r['foes']:3} mine {r['mine']:3} | {r['months']}mo | {r['end']}{' in '+str(r['took']) if r['took'] is not None else ''}")
        elif r['kind'] == 'hold':
            print(f"{r['file']:22} {r['at']:15} hold   {r['title'][:34]:34} {r.get('size','?'):8}{' F'+str(r['fort']) if r.get('fort') else '   '} {r.get('state','?')}, my troops there {r.get('there','?')} | {r['months']}mo | {r['end']}{' in '+str(r['took']) if r['took'] is not None else ''}")
        else:
            print(f"{r['file']:22} {r['at']:15} raise  army {r.get('army','?')} to {r.get('target','?')}, treasury {r.get('treasury','?')}, net {r.get('net','?')} | {r['months']}mo | {r['end']}{' in '+str(r['took']) if r['took'] is not None else ''}")
elif what == 'sum':
    def grp(sel, label):
        c = collections.Counter(); n = 0
        for r in rows:
            if not sel(r): continue
            n += 1; c[r['end']] += 1
        print(f"{label:58} {n:4}  " + ', '.join(f"{k} {v}" for k, v in c.most_common()))
    for part, pre in (('scripted player, no cabinet', 'play-'), ('idle player', 'idle-'), ('player with a cabinet', 'cab-'), ('sets every one aside', 'aside-')):
        P = lambda r, pre=pre: r['file'].startswith(pre)
        print('==', part)
        for k in ('take', 'relief', 'hold', 'raise'): grp(lambda r, k=k: P(r) and r['kind'] == k, '  ' + k)
        tk = lambda r: P(r) and r['kind'] in ('take', 'relief') and 'holder' in r
        grp(lambda r: tk(r) and r['hops'] < 0, '  take/relief: no way by land for any of my corps')
        grp(lambda r: tk(r) and r['hops'] > 2, '  take/relief: nearest corps 3 or more provinces away')
        grp(lambda r: tk(r) and r['nbcorps'] == 0, '  take/relief: none of my corps beside it')
        grp(lambda r: tk(r) and r['mine'] > 0 and r['foes'] > r['mine'], '  take/relief: more defenders than my troops beside it')
        grp(lambda r: tk(r) and r['mine'] > 0 and r['foes'] > 1.5 * r['mine'], '  take/relief: defenders over 1.5 times my troops beside it')
        grp(lambda r: tk(r) and r['foes'] == 0, '  take/relief: no defenders at all')
        grp(lambda r: tk(r) and r['fort'] >= 2, '  take/relief: a fortress of level 2 or more')
        grp(lambda r: tk(r) and r['cap'], '  take/relief: a capital')
        grp(lambda r: P(r) and r['kind'] == 'hold' and r.get('state') == 'threatened', '  hold: threatened only')
        grp(lambda r: P(r) and r['kind'] == 'hold' and r.get('state') == 'under siege', '  hold: under siege')
        done = [r['took'] for r in rows if P(r) and r['end'] == 'carried out' and r['kind'] in ('take', 'relief')]
        if done: print(f"  take/relief carried out: months taken {sorted(done)}")
