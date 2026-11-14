#!/usr/bin/env python3
"""Does a kept promise lift the level the cabinet's trust settles at (the count of ignored directives) with no directive carried out?
Version 60: yes, 10 times of 13. Version 62: only a fair or bolder promise does. usage: settletab.py DIR [PREFIX]"""
import re, glob, os, sys
D = sys.argv[1]; P = sys.argv[2] if len(sys.argv) > 2 else 'gov-vow'
tot = dict(kept=0, stood=0, cleared=0); cases = []
for f in sorted(glob.glob(os.path.join(D, P + '*.txt'))):
    lines = open(f, errors='replace').read().split('\n')
    vals = [int(m.group(1)) for ln in lines for m in [re.match(r'\w+ \d{4}: trust .*?settles at (\d+)', ln)] if m]
    if not vals: continue
    base = max(vals); last = None; kept = None; done = False
    for ln in lines:
        m = re.match(r'(\w+ \d{4}): trust .*?settles at (\d+)', ln)
        if m:
            s = int(m.group(2))
            if kept and last is not None and last < base and not done:
                tot['stood'] += 1
                if s > last: tot['cleared'] += 1; cases.append((os.path.basename(f), kept, last, s))
            last = s; kept = None; done = False; continue
        if 'Promise kept:' in ln: tot['kept'] += 1; kept = ln.strip()[:70]
        if '=> carried out' in ln: done = True
print(f"promises kept {tot['kept']}; kept while a count of ignored directives stood {tot['stood']}; of those the count was cleared {tot['cleared']}")
for c in cases: print('   ', *c)
