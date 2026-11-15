#!/usr/bin/env python3
"""World Domination from every seat, from probe56's DOM lines (files DIR/PREFIX-NAT-SEED.txt).
usage: domtab.py DIR PREFIX... [--seeds A-B] [--rows]
For each prefix (say `dom` by hand and `domfield` at Field command) and each country: in how many wars the war between
the alliances was decided, and for the player's side; in how many the last act began; in how many the player WON the
world, fell, or was left short with other nations standing; the share of the map's worth held at the end and at the
most (how near the player came); who was still standing in the wars left short; and, where the war was never decided,
which great powers were still at war. --rows prints every war."""
import re, sys, glob, os, statistics as st, collections
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
ORDER = ['GER', 'ITA', 'JAP', 'UK', 'FRA', 'SOV', 'USA']
NAME = {'GER': 'Germany', 'ITA': 'Italy', 'UK': 'Britain', 'FRA': 'France', 'SOV': 'Soviet Union', 'USA': 'United States', 'JAP': 'Japan'}
for name in names:
    by = {}
    for f in glob.glob(os.path.join(d, f'{name}-*-*.txt')):
        m = re.search(rf'{re.escape(name)}-([A-Z]+)-(\d+)\.txt$', os.path.basename(f))
        if not m: continue
        nat, seed = m.group(1), int(m.group(2))
        if not lo <= seed <= hi: continue
        s = [l for l in open(f).read().splitlines() if l.startswith('DOM ')]
        if not s: continue
        s = s[-1]; r = {'seed': seed, 'line': s}
        m = re.search(r"war decided (\w+ \d{4}) for (\w+) .*?; the player's side (won|did not win)", s)
        r['decided'] = turn(m.group(1)) if m else 0; r['mine'] = bool(m and m.group(3) == 'won'); r['victor'] = m.group(2) if m else ''
        m = re.search(r'still at war: ([^)]*)\)', s); r['pairs'] = [] if not m or m.group(1) == 'no two' else m.group(1).split(', ')
        m = re.search(r'last act begun (\w+ \d{4}) with (\d+) nations, the player holding (\d+)%', s)
        r['dom'] = turn(m.group(1)) if m else 0; r['domN'] = int(m.group(2)) if m else 0; r['domShare'] = int(m.group(3)) if m else 0
        m = re.search(r'; WON (\w+ \d{4});', s); r['won'] = turn(m.group(1)) if m else 0
        m = re.search(r'; the player fell (\w+ \d{4}) to (\w+|\?)', s); r['fell'] = turn(m.group(1)) if m else 0; r['fellTo'] = m.group(2) if m else ''
        m = re.search(r'; played to (\w+ \d{4})( \(given up: undecided\))?: (\d+) nations left', s); r['left'] = int(m.group(3)) if m else (1 if r['won'] else 0); r['gaveUp'] = bool(m and m.group(2))
        m = re.search(r"at the end (\d+)% \(rank (\S+) of (\d+)\), at the most (\d+)%", s); r['end'], r['most'] = int(m.group(1)), int(m.group(4))
        m = re.search(r'strongest rival (\w+) (\d+)% and (\d+) divisions', s); r['rival'] = m.group(1) if m else ''
        m = re.search(r'fell after the last act began (\d+)', s); r['beat'] = int(m.group(1))
        m = re.search(r'left at the end: ([^;]*);', s); r['who'] = [x.split()[0] for x in m.group(1).split(', ') if x] if m else []
        by.setdefault(nat, []).append(r)
    print(f'== {name}' + (f' (seeds {lo} to {hi})' if hi < 9999 else ''))
    if not by: print('   no wars with a DOM line'); continue
    print(f"   {'seat':13} {'wars':>4} {'decided':>7} {'for us':>6} {'last act':>8} {'WON':>4} {'fell':>4} {'short':>5}  {'share at the end, at the most (middle)':38} {'decided (middle)':17} {'won (middle)':14}")
    for nat in sorted(by, key=lambda x: ORDER.index(x) if x in ORDER else 99):
        rows = sorted(by[nat], key=lambda r: r['seed']); n = len(rows)
        dec = [r for r in rows if r['decided']]; ours = [r for r in dec if r['mine']]; act = [r for r in rows if r['dom']]
        won = [r for r in rows if r['won']]; fell = [r for r in rows if r['fell']]; short = [r for r in act if not r['won'] and not r['fell']]
        print(f"   {NAME.get(nat, nat):13} {n:4} {len(dec):7} {len(ours):6} {len(act):8} {len(won):4} {len(fell):4} {len(short):5}  "
              f"{f'{st.median(r['end'] for r in rows):g}%, {st.median(r['most'] for r in rows):g}%':38} {date(mid([r['decided'] for r in dec])) if dec else '-':17} {date(mid([r['won'] for r in won])) if won else '-':14}")
        print(f"   {'':13} WORLD WON in {len(won)} of {n} ({round(100 * len(won) / n)}%); half the map's worth or more at the most in {sum(1 for r in rows if r['most'] >= 50)}, three quarters or more in {sum(1 for r in rows if r['most'] >= 75)}, nine tenths or more in {sum(1 for r in rows if r['most'] >= 90)}")
        if short:
            who = collections.Counter(x for r in short for x in r['who'] if x != nat)
            print(f"   {'':13} left short in {len(short)}: middle {st.median(r['left'] for r in short):g} nations standing, the player holding a middle {st.median(r['end'] for r in short):g}%, beat a middle {st.median(r['beat'] for r in short):g} nations after the last act began; still standing: {', '.join(f'{k} {v}' for k, v in who.most_common(8))}")
        if fell:
            to = collections.Counter(r['fellTo'] for r in fell)
            print(f"   {'':13} the player fell in {len(fell)} (middle {date(mid([r['fell'] for r in fell]))}; to {', '.join(f'{k} {v}' for k, v in to.most_common(4))}); of them after the last act began: {sum(1 for r in fell if r['dom'])}")
        und = [r for r in rows if not r['decided'] and not r['fell']]
        if und:
            pairs = collections.Counter(p for r in und for p in r['pairs'])
            print(f"   {'':13} never decided with the player standing in {len(und)}: great powers still at war: {', '.join(f'{k} {v}' for k, v in pairs.most_common(6)) or 'none'}")
        if rowsToo:
            for r in rows: print(f"      seed {r['seed']:2}: {'WON ' + date(r['won']) if r['won'] else 'fell ' + date(r['fell']) if r['fell'] else str(r['left']) + ' left':22} decided {date(r['decided']) if r['decided'] else 'never':15} share {r['end']:3}% most {r['most']:3}% beat {r['beat']:2}  left: {' '.join(r['who'][:8])}")
