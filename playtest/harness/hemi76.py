# usage: python3 hemi76.py [GLOB...]   (the rules builder's table; the Playtester's copy takes the files to read, default res76/hemi-*.txt: what the countries of the Americas earn at peace and at war)
import glob, re, statistics, sys
rows = []; ends = []
for f in sorted(x for g in (sys.argv[1:] or ['res76/hemi-*.txt']) for x in glob.glob(g)):
    for ln in open(f):
        m = re.match(r'H seed (\d+) (\w+ \d+) \| (\w+) (WAR|peace|fallen) (\w+) \| income ([\d.]+) trade ([\d.]+) \| partners (\S+)( BOTH-SIDES)? \| treasury (\d+) \| corps (\d+) away (\d+) \| provinces (\d+)', ln)
        if m: rows.append(dict(seed=int(m[1]), date=m[2], nat=m[3], state=m[4], align=m[5], income=float(m[6]), trade=float(m[7]), partners=m[8], both=bool(m[9]), treasury=int(m[10]), corps=int(m[11]), away=int(m[12]), prov=int(m[13])))
        elif ln.startswith('E '): ends.append(ln.strip())
AM = ['USA', 'CAN', 'MEX', 'CUB', 'BRA', 'ARG', 'CHL', 'PER', 'COL', 'VEN', 'GUA', 'ELS', 'HON', 'NIC', 'COS', 'PAN', 'HAI', 'DOM', 'ECU', 'BOL', 'PAR', 'URU']
seeds = sorted({r['seed'] for r in rows})
print(f'{len(seeds)} wars (seeds {seeds}), read each January and at the end')
print('country   at peace: income (of it trade), treasury at most | at war: income (trade), treasury at most | most corps, most away from home')
for nat in AM:
    p = [r for r in rows if r['nat'] == nat and r['state'] == 'peace' and r['date'] < 'Z' and not (r['align'] == 'allies' and nat not in ('CAN',))]
    w = [r for r in rows if r['nat'] == nat and r['state'] == 'WAR']
    mine = [r for r in rows if r['nat'] == nat]
    f = lambda rs: f"{statistics.mean(r['income'] for r in rs):5.1f} ({statistics.mean(r['trade'] for r in rs):.1f}), {max(r['treasury'] for r in rs):3d}" if rs else '   never'
    print(f"{nat:5s}     {f(p):28s} | {f(w):24s} | {max(r['corps'] for r in mine)}, {max(r['away'] for r in mine)}")
neutral = [r for r in rows if r['nat'] in AM and r['state'] == 'peace' and r['nat'] != 'USA']
war = [r for r in rows if r['state'] == 'WAR']
top = max(neutral, key=lambda r: r['income'])
print(f"the best-earning neutral of the Americas in any reading: {top['nat']} {top['income']} a month ({top['date']}, seed {top['seed']}), of it trade {top['trade']}")
print('neutrals trading with both sides:', sorted({r['nat'] for r in rows if r['both'] and r['state'] == 'peace'}) or 'none')
# The same country at peace and at war, in the same war.
for nat in ['BRA', 'MEX', 'CUB', 'PAN', 'GUA']:
    ratios = []
    for s in seeds:
        p = [r['income'] for r in rows if r['seed'] == s and r['nat'] == nat and r['state'] == 'peace' and r['align'] == 'independent']
        w = [r['income'] for r in rows if r['seed'] == s and r['nat'] == nat and r['state'] == 'WAR']
        if p and w: ratios.append(statistics.mean(w) / statistics.mean(p))
    if ratios: print(f"{nat}: at war it earns {statistics.mean(ratios):.2f} times what it earned at peace ({len(ratios)} wars)")
for e in ends: print(e)
