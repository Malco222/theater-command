#!/usr/bin/env python3
"""History's dates in the wars with nobody playing, for the staff report card (ruling 56): the year France falls,
the Soviet Union is attacked, America goes to war, Italy is out, France is freed, the Americans are ashore in the
west to stay, and Germany falls. Reads the WATCH lines of watch49s3-*.txt and the ARMY lines of army-USA-*.txt.
usage: datestab.py DIR..."""
import re, sys, glob, collections
def card(d):
    W = [l for f in sorted(glob.glob(f'{d}/watch49s3-*.txt')) for l in open(f) if l.startswith('WATCH')]
    A = [l for f in sorted(glob.glob(f'{d}/army-USA-*.txt')) for l in open(f) if l.startswith('ARMY')]
    yr = lambda pat, l: (m := re.search(pat, l)) and int(m.group(1))
    fell = lambda l: l.split('fell:')[1].split('|')[0]
    rows = [('France falls', collections.Counter(yr(r'FRA \w+ (\d{4})', fell(l)) or 'never' for l in W)),
            ('the Soviet Union attacked', collections.Counter(yr(r'east \w+ (\d{4})', l) or 'never' for l in W)),
            ('America at war', collections.Counter(yr(r'at war \w+ (\d{4})', l) or 'never' for l in A)),
            ('Italy out', collections.Counter(yr(r'ITA \w+ (\d{4})', fell(l)) or 'never' for l in W)),
            ('France freed', collections.Counter(yr(r'back \w+ (\d{4})', l) or 'never' for l in W)),
            ('Americans ashore in the west to stay', collections.Counter(yr(r'ashore in the west to stay from \w+ (\d{4})', l.split('| Germany ')[0]) or 'never' for l in A)),
            ('Germany falls', collections.Counter(yr(r'GER \w+ (\d{4})', fell(l)) or 'never' for l in W))]
    print(f'{d}: {len(W)} wars, {len(A)} army lines')
    for k, v in rows: print(f'   {k}: ' + ', '.join(f'{y} in {n}' for y, n in sorted(v.items(), key=lambda x: str(x[0]))))
for d in sys.argv[1:]: card(d)
