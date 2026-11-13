#!/usr/bin/env python3
# One line per war of a guns folder: gunstab.py DIR [DIR ...]
import re, glob, os, sys
for d in sys.argv[1:]:
    print('==', d)
    for f in sorted(glob.glob(d + '/def-*.txt')):
        m = re.match(r'def-(\w+?)_(\w+)-(\d+)\.txt', os.path.basename(f))
        if not m: continue
        who, habit, seed = m.groups(); t = open(f).read()
        tot = re.search(r'TOTAL .*?: played to ([A-Za-z]+ \d+)(?: \(fell (\d+)\))?; Berlin in December 1944: (\S+); in the last month: (\S+); longest .*?: (\d+) months', t)
        if not tot: print(f"   {who:4s} {seed:>2s} {habit:7s} (running)"); continue
        ja = dict((y, p) for y, p in re.findall(r'January (\d+): .*?provinces (\d+)', t))
        rows = [l.split() for l in t.splitlines() if l.startswith('ROW ')]
        mine = [r for r in rows if r[2] == 'A']; prep = [r for r in mine if float(r[5]) > 0]; g = [r for r in prep if int(r[13]) > 0]
        won = lambda rs: sum(int(r[8]) for r in rs)
        print(f"   {who:4s} {seed:>2s} {habit:7s} to {tot.group(1):14s} Berlin Dec44 {tot.group(3):4s} last {tot.group(4):4s} quiet {tot.group(5):>2s} | provinces Jan 43/44/45 {ja.get('1943','-'):>3s}/{ja.get('1944','-'):>3s}/{ja.get('1945','-'):>3s} | my attacks {len(mine):3d}, on prepared lines {len(prep):3d} (won {won(prep):3d}), with guns {len(g):3d} (won {won(g):3d})")
