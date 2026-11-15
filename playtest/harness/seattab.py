#!/usr/bin/env python3
"""Versions 67 to 69: a fair shot from every seat, from the SEAT lines of probe53, probe54 or probe57 (files DIR/PREFIX-NAT-SEED.txt).
usage: seattab.py DIR PREFIX... [--seeds A-B] [--rows]
For each prefix (say `seat` by hand and `seatfield` at Field command) and each country: how many wars the player's
country stood to the end of, how the war went for its side (won: the chief enemy fell while the player stands; lost:
the player's country fell, or for a junior partner its senior partner fell; undecided), when the falls came, and how
much of the war the player had a battle to fight in. With probe54's lines, also par: in how many wars the country
beat its own history, and how near it came (home provinces in enemy hands at the worst, months with an enemy on home
soil). --rows prints every war."""
import re, sys, glob, os, statistics as st
args = sys.argv[1:]
lo, hi = 1, 9999
rowsToo = '--rows' in args
if rowsToo: args.remove('--rows')
if '--seeds' in args:
    i = args.index('--seeds'); lo, hi = map(int, args[i + 1].split('-')); del args[i:i + 2]
d, names = args[0], args[1:]
MON = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']
def turn(s):
    m = re.match(r'(\w+) (\d{4})', s); return (int(m.group(2)) - 1939) * 12 + MON.index(m.group(1)) + 1 - 8
def date(t): t += 7; return f'{MON[t % 12]} {1939 + t // 12}'
def mid(xs): xs = sorted(xs); return xs[len(xs) // 2] if xs else 0
SENIOR = {'ITA': 'GER'}  # a junior partner's war is lost with its senior partner
ORDER = ['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA', 'JAP']
NAME = {'GER': 'Germany', 'ITA': 'Italy', 'UK': 'Britain', 'FRA': 'France', 'SOV': 'Soviet Union', 'USA': 'United States', 'JAP': 'Japan'}
for name in names:
    by = {}
    for f in glob.glob(os.path.join(d, f'{name}-*-*.txt')):
        m = re.search(rf'{re.escape(name)}-([A-Z]+)-(\d+)\.txt$', os.path.basename(f))
        if not m: continue
        nat, seed = m.group(1), int(m.group(2))
        if not lo <= seed <= hi: continue
        s = [l for l in open(f).read().splitlines() if l.startswith('SEAT')]
        if not s: continue
        s = s[-1]
        r = {'seed': seed, 'stands': '; stands;' in s}
        m = re.search(r'; fell (\w+ \d{4});', s); r['fell'] = turn(m.group(1)) if m else 0
        r['result'] = re.search(r'result (\w+)', s).group(1)
        m = re.search(r'result won \((\w+) fell (\w+ \d{4})\)', s); r['chiefFell'] = turn(m.group(2)) if m else 0
        fallen = re.search(r'great powers fallen: ([^;]*);', s).group(1)
        r['fallen'] = {x.split()[0]: (turn(' '.join(x.split()[1:3])), '(back)' in x) for x in fallen.split(', ') if x != 'none'}
        sen = SENIOR.get(nat)
        r['senFell'] = r['fallen'][sen][0] if sen and sen in r['fallen'] and not r['fallen'][sen][1] else 0
        r['side'] = 'lost' if not r['stands'] or r['senFell'] else r['result']
        m = re.search(r'capital (.+?) held by (\w+)', s); r['capital'] = m.group(2)
        m = re.search(r"at war (\d+) months(?: from (\w+ \d{4}))?, a battle of the player's in (\d+), longest silence (\d+)", s)
        r['war'], r['fought'], r['quiet'] = int(m.group(1)), int(m.group(3)), int(m.group(4))
        m = re.search(r'provinces at the start (\d+), most (\d+), at the end (\d+)', s); r['began'], r['most'], r['end'] = map(int, m.groups())
        m = re.search(r'; par (BEATEN|missed) \((.*?)\); home provinces in enemy hands at the worst (\d+), an enemy on home soil in (\d+) months', s)
        r['par'] = None if not m else m.group(1) == 'BEATEN'
        r['parWhy'] = m.group(2) if m else ''
        r['homeLost'], r['homeMonths'] = (int(m.group(3)), int(m.group(4))) if m else (0, 0)
        m = re.search(r'Soviet levies (\d+)', s); r['levies'] = int(m.group(1)) if m else 0
        m = re.search(r'; old line (beaten|missed)', s); r['old'] = None if not m else m.group(1) == 'beaten'
        m = re.search(r'; the east opened (\w+ \d{4})', s); r['east'] = turn(m.group(1)) if m else 0
        m = re.search(r'ashore to stay from (\w+ \d{4})', s); r['ashore'] = turn(m.group(1)) if m else 0
        r['gerFell'] = r['fallen']['GER'][0] if 'GER' in r['fallen'] and not r['fallen']['GER'][1] else 0
        r['line'] = s
        by.setdefault(nat, []).append(r)
    print(f'== {name}' + (f' (seeds {lo} to {hi})' if hi < 9999 else ''))
    if not by: print('   no wars with a SEAT line'); continue
    print(f"   {'seat':13} {'wars':>4} {'stands':>6} {'won':>4} {'lost':>4} {'open':>4}  {'own fall (middle)':18} {'chief enemy fell (middle)':26} {'battle in':>9} {'silence':>7} {'a year+':>7} {'land: start, most, end':22}")
    for nat in sorted(by, key=lambda x: ORDER.index(x) if x in ORDER else 99):
        rows = sorted(by[nat], key=lambda r: r['seed']); n = len(rows)
        won = [r for r in rows if r['side'] == 'won']; lost = [r for r in rows if r['side'] == 'lost']
        fell = [r['fell'] for r in rows if r['fell']]
        cf = [r['chiefFell'] for r in rows if r['chiefFell']]
        share = st.median(100 * r['fought'] / r['war'] for r in rows if r['war']) if any(r['war'] for r in rows) else 0
        print(f"   {NAME.get(nat, nat):13} {n:4} {sum(r['stands'] for r in rows):6} {len(won):4} {len(lost):4} {n - len(won) - len(lost):4}  "
              f"{(date(mid(fell)) + f' ({len(fell)})') if fell else '-':18} {(date(mid(cf)) + f' ({len(cf)})') if cf else '-':26} {share:8.0f}% {st.median(r['quiet'] for r in rows):7g} {sum(r['quiet'] >= 12 for r in rows):7} "
              f"{st.median(r['began'] for r in rows):g}, {st.median(r['most'] for r in rows):g}, {st.median(r['end'] for r in rows):g}")
        if any(r['par'] is not None for r in rows):
            pb = sum(1 for r in rows if r['par'])
            extra = ''
            if nat == 'UK':
                a = [r['ashore'] for r in rows if r['ashore']]
                extra = f"; ashore in the west to stay in {len(a)}" + (f", the middle one from {date(mid(a))}, before June 1944 in {sum(1 for x in a if x < 58)}" if a else '') + f"; invaded in {sum(1 for r in rows if r['homeMonths'])}"
            if nat != 'GER':
                gf = [r['gerFell'] for r in rows if r['gerFell']]
                extra += f"; Germany fell in {len(gf)}" + (f", the middle one in {date(mid(gf))}" if gf else '')
            lv = sum(1 for r in rows if r['levies'])
            if any(r.get('old') is not None for r in rows): extra += f"; by the lines as first told (Version 68): {sum(1 for r in rows if r.get('old'))}"
            ea = [r['east'] for r in rows if r.get('east')]
            if ea and nat in ('SOV', 'GER'): extra += f"; the east opens in 1941 in {sum(1 for x in ea if x <= 28)}, in 1942 in {sum(1 for x in ea if 28 < x <= 40)}, later in {sum(1 for x in ea if x > 40)}, never in {len(rows) - len(ea)}"
            print(f"   {'':13} PAR BEATEN in {pb} of {n} ({round(100 * pb / n)}%); an enemy on home soil in {sum(1 for r in rows if r['homeMonths'])} wars (middle {st.median(r['homeMonths'] for r in rows):g} months), home provinces lost at the worst: middle {st.median(r['homeLost'] for r in rows):g}; a Soviet levy in {lv}{extra}")
        sen = SENIOR.get(nat)
        if sen:
            sf = [r['senFell'] for r in rows if r['senFell']]
            print(f"   {'':13} its senior partner {NAME[sen]} fell in {len(sf)} of {n}" + (f", the middle one in {date(mid(sf))}" if sf else '') + f"; the player's own country stood in {sum(r['stands'] for r in rows)}")
        if rowsToo:
            for r in rows: print(f"      seed {r['seed']:2}: {'par' if r['par'] else '   '} {r['side']:9} {'stands' if r['stands'] else 'fell ' + date(r['fell']):20} battle in {r['fought']} of {r['war']}, silence {r['quiet']}, land {r['began']} / {r['most']} / {r['end']}")
