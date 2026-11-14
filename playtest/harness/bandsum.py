#!/usr/bin/env python3
"""Sum the BATTLES bands of probe28 outputs. usage: bandsum.py FILE..."""
import sys, re
from collections import defaultdict
t = defaultdict(lambda: [0, 0])
for f in sys.argv[1:]:
    for ln in open(f, errors='replace'):
        m = re.match(r'BATTLES (.+?)\s+(\d+) fought \| attacker won\s+(\d+)', ln)
        if m: k = re.sub(r'\s+', ' ', m.group(1)); t[k][0] += int(m.group(2)); t[k][1] += int(m.group(3))
for k in sorted(t): print(f'{k:<62}{t[k][0]:>6} fought, attacker won {t[k][1]:>6} ({100 * t[k][1] / max(1, t[k][0]):.0f}%)')
