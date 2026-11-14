#!/usr/bin/env python3
"""Version 67: main efforts by the nation that made them, from probe28's EFFORT lines (files DIR/NAME-GER-SEED.txt).
usage: efftab.py DIR NAME... [--seeds A-B]
For each nation: efforts named where the player stood, how many came to blows, when the first blow fell against the
month the effort was named (the month named is meant to be massing only), and when the player first had word of it
against the first blow."""
import re, sys, glob, os, collections
args = sys.argv[1:]
lo, hi = 1, 9999
if '--seeds' in args:
    i = args.index('--seeds'); lo, hi = map(int, args[i + 1].split('-')); del args[i:i + 2]
d, names = args[0], args[1:]
MON = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']
def turn(s):
    m = re.match(r'(\w+) (\d{4})', s); return (int(m.group(2)) - 1939) * 12 + MON.index(m.group(1)) + 1 - 8 if m else None
for name in names:
    by = collections.defaultdict(lambda: collections.Counter())
    for f in glob.glob(os.path.join(d, f'{name}-GER-*.txt')):
        seed = int(re.search(r'-(\d+)\.txt$', f).group(1))
        if not lo <= seed <= hi: continue
        for l in open(f):
            if not l.startswith('EFFORT'): continue
            m = re.match(r'EFFORT (\w+) named (\w+ \d{4}) at (.+?): .*battles (\d+) .*first word (.+?), first blow (.+)$', l.strip())
            if not m: by['?']['unread'] += 1; continue
            nat, named, at, battles, word, blow = m.groups()
            c = by[nat]; c['named'] += 1
            tn, tw, tb = turn(named), turn(word), turn(blow)
            if tb is None: c['never fought'] += 1; continue
            c['fought'] += 1
            c['blow in the month named' if tb == tn else 'blow one month after' if tb == tn + 1 else 'blow two or three months after' if tb <= tn + 3 else 'blow later still'] += 1
            c['no word' if tw is None else 'word a month or more ahead' if tw < tb else 'word with the blow' if tw == tb else 'word after the blow'] += 1
    print(f'== {name}')
    keys = ['named', 'fought', 'never fought', 'blow in the month named', 'blow one month after', 'blow two or three months after', 'blow later still', 'word a month or more ahead', 'word with the blow', 'word after the blow', 'no word']
    for nat in sorted(by, key=lambda n: -by[n]['named']):
        print(f"   {nat:4} " + '; '.join(f'{k} {by[nat][k]}' for k in keys if by[nat][k]))
