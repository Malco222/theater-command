#!/usr/bin/env python3
# Version 73: the landing card's forecast of the sea behind the beach against what happened (HELD lines of probe56).
# usage: holdtab.py DIR PREFIX... [--rows]
import sys, re, glob, os, collections
args = [a for a in sys.argv[1:] if not a.startswith('--')]; rows = '--rows' in sys.argv
d, prefs = args[0], args[1:]
pat = re.compile(r'^HELD (\w+) seed (\d+): (\w+ \d{4}) (.+?) with (\d+) corps: the card said the (.+?) (stays open|would be closed) \(enemy (\d+), own (\d+), need (\d+)(, no harbour)?\); a month later it was (CLOSED|open|\?), the force (gone|fed|unfed[^;]*|\?); closed in any of the three months after: (yes|no); corps on that ground after 1, 2, 3 months: (.+)$')
for p in prefs:
    by = collections.defaultdict(list)
    for f in sorted(glob.glob(os.path.join(d, p + '-*.txt'))):
        if not re.match(re.escape(p) + r'-[A-Z]+-\d+\.txt$', os.path.basename(f)): continue
        for l in open(f):
            m = pat.match(l.strip())
            if m: by[m.group(1)].append(m)
    print('== ' + p)
    for seat, ms in by.items():
        for said in ('stays open', 'would be closed'):
            g = [m for m in ms if m.group(7) == said]
            if not g: continue
            closed1 = sum(1 for m in g if m.group(12) == 'CLOSED'); closed3 = sum(1 for m in g if m.group(14) == 'yes')
            gone1 = sum(1 for m in g if m.group(13) == 'gone'); unfed1 = sum(1 for m in g if m.group(13).startswith('unfed')); fed1 = sum(1 for m in g if m.group(13) == 'fed')
            there3 = sum(1 for m in g if m.group(15).split(', ')[2] not in ('0', '?'))
            print(f'   {seat}: the card said "{said}" before {len(g)} landings: a month later the sea was closed in {closed1}, closed within three months in {closed3}; the force a month later: fed {fed1}, unfed {unfed1}, gone {gone1}; still on that ground after three months in {there3}')
            if rows:
                for m in g: print(f'      seed {m.group(2)} {m.group(3)} {m.group(4)} ({m.group(5)} corps; {m.group(6)}; enemy {m.group(8)}, own {m.group(9)}, need {m.group(10)}{m.group(11) or ""}): a month later {m.group(12)}, {m.group(13)}; within three months closed {m.group(14)}; corps after 1, 2, 3 months {m.group(15)}')
