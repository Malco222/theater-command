#!/usr/bin/env python3
# Version 61: one line per defence war from probe28's TOTAL line. usage: v61tab.py DIR [DIR...]
import sys, glob, os, re
for d in sys.argv[1:]:
    print('==', d)
    for f in sorted(glob.glob(os.path.join(d, 'def-*.txt'))):
        t = [l for l in open(f) if l.startswith('TOTAL')]
        if not t: print(f"{os.path.basename(f):34s} (no total yet)"); continue
        t = t[-1]
        g = lambda p, n=1: (re.search(p, t).group(n) if re.search(p, t) else '-')
        print(f"{os.path.basename(f)[4:-4]:26s} to {g(r'played to ([A-Za-z]+ \d+)'):15s} {'FELL' if 'fell' in t else '    '} Berlin Dec44 {g(r'Berlin in December 1944: (\S+);'):4s} quiet {g(r'no battle of the player.s: (\d+) months'):>2s} | back-but-wore mine {g(r'my attacks (\d+ \([\d.]+ months\))'):18s} on me {g(r'attacks on me (\d+ \([\d.]+ months\))'):18s} under-bar {g(r'wear a line (\d+)'):>3s} | {g(r'by year ([^;]+);')} | prov {g(r'provinces at the end (\d+)')}")
