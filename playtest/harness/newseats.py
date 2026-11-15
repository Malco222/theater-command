#!/usr/bin/env python3
"""Version 74: the new seats (the United States, Canada, Mexico), one row a set of wars, from the SEAT lines of probe58.
usage: newseats.py DIR PREFIX-SEAT... [--seeds A-B] [--rows]
For each set: wars, how the player ended, when Germany fell (the middle war), months at war, months with a battle of the
player's own, the longest run of months at war with no battle, provinces at the start, at the most and at the end.
"""
import glob, os, re, statistics, sys
MON = ['January','February','March','April','May','June','July','August','September','October','November','December']
def turn(s):
    m, y = s.split(); return (int(y) - 1939) * 12 + MON.index(m) - 7
def date(t):
    t = int(round(t)); return f'{MON[(t + 7) % 12]} {1939 + (t + 7) // 12}'
args = [a for a in sys.argv[1:] if not a.startswith('--')]; d, sets = args[0], args[1:]
lo, hi = 0, 9999
if '--seeds' in sys.argv: lo, hi = map(int, sys.argv[sys.argv.index('--seeds') + 1].split('-')); sets = [s for s in sets if not re.fullmatch(r'\d+-\d+', s)]
rows = '--rows' in sys.argv
mid = lambda xs: statistics.median(xs) if xs else 0
print(f"   {'set':22} wars  stands  Germany fell (middle; never)   at war  battle months  longest silence  provinces start, most, end   errors")
for s in sets:
    R = []
    for f in sorted(glob.glob(os.path.join(d, s + '-*.txt')), key=lambda p: int(re.search(r'-(\d+)\.txt$', p).group(1))):
        seed = int(re.search(r'-(\d+)\.txt$', f).group(1))
        if not lo <= seed <= hi: continue
        if os.path.getsize(f) == 0: continue
        t = open(f).read(); m = re.search(r'^SEAT (\w+) seed (\d+): (.*)$', t, re.M)
        if not m: R.append({'seed': seed, 'err': True}); continue
        x = m.group(3); r = {'seed': seed, 'err': ' ERROR' in t or '\nERROR' in t}
        r['stands'] = '; stands;' in ('; ' + x)
        g = re.search(r'GER fell (\w+ \d{4})', x); r['ger'] = turn(g.group(1)) if g else 0
        w = re.search(r'at war (\d+) months(?: from (\w+ \d{4}))?, a battle of the player\'s in (\d+), longest silence (\d+) months', x)
        r['war'], r['from'], r['bat'], r['sil'] = (int(w.group(1)), w.group(2) or '', int(w.group(3)), int(w.group(4))) if w else (0, '', 0, 0)
        if not w:
            w0 = re.search(r'at war (\d+) months', x); r['war'] = int(w0.group(1)) if w0 else 0
        p = re.search(r'provinces at the start (\d+), most (\d+), at the end (\d+)', x); r['p'] = tuple(map(int, p.groups())) if p else (0, 0, 0)
        e = re.search(r'result (\w+)', x); r['res'] = e.group(1) if e else '?'
        R.append(r)
    ok = [r for r in R if 'war' in r]
    if not ok: print(f'   {s:22} no wars'); continue
    fell = [r['ger'] for r in ok if r['ger']]
    gm = date(sorted([r['ger'] or 999 for r in ok])[(len(ok) - 1) // 2]) if len(fell) > len(ok) / 2 else 'never'
    print(f"   {s:22} {len(ok):4}  {sum(r['stands'] for r in ok):6}  {gm:16} {len(ok) - len(fell):3} never    {mid([r['war'] for r in ok]):6.0f}  {mid([r['bat'] for r in ok]):13.0f}  {mid([r['sil'] for r in ok]):15.0f}  {mid([r['p'][0] for r in ok]):.0f}, {mid([r['p'][1] for r in ok]):.0f}, {mid([r['p'][2] for r in ok]):.0f}{'':14} {sum(r['err'] for r in R)}")
    if rows:
        for r in ok: print(f"      seed {r['seed']}: {r['res']}; Germany fell {date(r['ger']) if r['ger'] else 'never'}; at war {r['war']} months{' from ' + r['from'] if r['from'] else ''}, battles in {r['bat']}, longest silence {r['sil']}; provinces {r['p'][0]}, {r['p'][1]}, {r['p'][2]}")
