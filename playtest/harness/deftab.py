#!/usr/bin/env python3
# Adds up probe28's BATTLES lines over a folder of wars: deftab.py DIR [NAT ...]
import sys, re, glob, os, collections
d = sys.argv[1]; nats = sys.argv[2:]
tab = collections.defaultdict(lambda: [0, 0, 0.0, 0.0, 0.0])
for f in sorted(glob.glob(os.path.join(d, 'def-*.txt'))):
    nat = os.path.basename(f).split('-')[1]
    if nats and nat not in nats: continue
    for line in open(f):
        m = re.match(r'BATTLES (.+?)\s+(\d+) fought \| attacker won\s+(\d+) .*mean odds ([\d.]+) \| on the day [\d.]+ \| lost a battle: attacker ([\d.]+), defender ([\d.]+)', line)
        if not m: continue
        k = m.group(1).strip(); n = int(m.group(2)); t = tab[k]
        t[0] += n; t[1] += int(m.group(3)); t[2] += float(m.group(4)) * n; t[3] += float(m.group(5)) * n; t[4] += float(m.group(6)) * n
for k in sorted(tab):
    n, w, o, a, dd = tab[k]
    print(f"{k:58s} {n:5d} fought, attacker won {w:5d} ({100*w/n:5.1f}%), thrown back {n-w:4d}, mean odds {o/n:5.2f}, men lost a battle: attacker {a/n:8.0f}, defender {dd/n:8.0f}, ratio {a/max(dd,1):.2f}")
