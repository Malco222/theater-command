#!/usr/bin/env python3
"""Version 76: the hemisphere set from a player's seat (probe74's files DIR/latin-MODE-NAT-SEED.txt).
usage: latintab.py DIR [--rows]
For each country and each way of playing it: the mean income a month over the war and the income in the last reading
(of it trade), the months at war, the months in which the player's country was attacked or attacked somebody, the
provinces won and lost, the treasury at the end, and for the merchant the trading houses and the sides traded with."""
import re, sys, glob, os, statistics as st
d = sys.argv[1]; rowsToo = '--rows' in sys.argv
NAT = ['BRA', 'ARG', 'CHL', 'PER', 'COL', 'VEN', 'MEX']
MODE = ['idle', 'merchant', 'league', 'war', 'warearly', 'axis']
WORD = {'idle': 'no orders', 'merchant': 'at peace, building and trading', 'league': 'the League, by hand', 'war': 'Allies, war on Germany August 1942', 'warearly': 'Allies, war on Germany October 1939', 'axis': 'Axis, war on America and Britain August 1942'}
R = {}
for f in glob.glob(os.path.join(d, 'latin-*-*-*.txt')):
    m = re.search(r'latin-(\w+)-([A-Z]+)-(\d+)\.txt$', f)
    if not m: continue
    t = open(f).read(); row = {'seed': int(m[3]), 'err': 'ERROR' in t}
    s = re.search(r'SUM .*?income a month, the mean ([\d.]+) and the most ([\d.]+); of it trade, the mean ([\d.]+); at war in (\d+) months; trading with both sides at once in (\d+) months; agreements signed by the player (\d+)(?:, market halls ordered (\d+), trading houses at the end (\d+))?.*?; (stands|FALLEN)', t)
    a = re.search(r'ACT .*?at war from ([\w ]+); the player attacked in (\d+) months, was attacked in (\d+); provinces won (\d+), lost (\d+); bought (\d+) things; corps at the end (\d+)', t)
    hs = re.findall(r'^H .*? \| (\w+) (\w+) \| income ([\d.]+) of it trade ([\d.]+) \| partners ([^|]*) \| treasury (\d+) \| field corps (\d+), away (\d+)', t, re.M)
    if not s or not a or not hs: row['bad'] = True; R.setdefault((m[2], m[1]), []).append(row); continue
    row.update(mean=float(s[1]), most=float(s[2]), trade=float(s[3]), war=int(s[4]), both=int(s[5]), signed=int(s[6]), halls=int(s[7] or 0), houses=int(s[8] or 0), stands=s[9] == 'stands',
               warFrom=a[1], attacked=int(a[2]), hit=int(a[3]), won=int(a[4]), lost=int(a[5]), bought=int(a[6]), corps=int(a[7]),
               first=float(hs[0][2]), last=float(hs[-1][2]), lastTrade=float(hs[-1][3]), treasury=int(hs[-1][5]), away=max(int(h[7]) for h in hs), sides=sorted({x for h in hs for x in re.findall(r'\((\w+)\)', h[4])}))
    R.setdefault((m[2], m[1]), []).append(row)
mean = lambda xs: st.mean(xs) if xs else 0
for nat in NAT:
    if not any((nat, mo) in R for mo in MODE): continue
    print(f'== {nat}')
    for mo in MODE:
        rs = [r for r in R.get((nat, mo), []) if not r.get('bad')]; bad = len(R.get((nat, mo), [])) - len(rs)
        if not rs and not bad: continue
        if not rs: print(f'   {WORD[mo]}: {bad} wars with no reading'); continue
        print(f"   {WORD[mo]:44s} {len(rs)} wars | income a month: first {mean([r['first'] for r in rs]):4.1f}, mean {mean([r['mean'] for r in rs]):4.1f}, most {max(r['most'] for r in rs):4.1f}, of it trade {mean([r['trade'] for r in rs]):3.1f} | at war {mean([r['war'] for r in rs]):4.1f} months, attacked in {mean([r['hit'] for r in rs]):3.1f}, attacking in {mean([r['attacked'] for r in rs]):3.1f} | provinces won {sum(r['won'] for r in rs)}, lost {sum(r['lost'] for r in rs)} | corps away at most {max(r['away'] for r in rs)} | treasury at the end {mean([r['treasury'] for r in rs]):.0f} | fallen {sum(not r['stands'] for r in rs)}{', errors ' + str(sum(r['err'] for r in rs)) if any(r['err'] for r in rs) else ''}" + (f" | halls {mean([r['halls'] for r in rs]):.0f}, houses {mean([r['houses'] for r in rs]):.1f}, both sides in {mean([r['both'] for r in rs]):.0f} months, sides {sorted({x for r in rs for x in r['sides']})}" if mo == 'merchant' else ''))
        if rowsToo:
            for r in sorted(rs, key=lambda r: r['seed']): print(f"      seed {r['seed']}: mean {r['mean']}, at war from {r['warFrom']} ({r['war']} months), attacked in {r['hit']}, attacking in {r['attacked']}, won {r['won']}, lost {r['lost']}, away {r['away']}, treasury {r['treasury']}")
