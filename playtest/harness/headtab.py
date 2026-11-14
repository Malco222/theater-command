#!/usr/bin/env python3
"""Version 62: heads the cabinet asked for, read from the cabinet's own words. usage: headtab.py NEWDIR OLDDIR"""
import sys, re, glob, os, statistics as st
from collections import Counter, defaultdict
N, O = sys.argv[1], sys.argv[2]
def heads(f):
    return [ln for ln in open(f, errors='replace') if re.search(r'\bHEAD asked:', ln)]
per = defaultdict(lambda: [0, 0, 0]); ev = []; names = Counter(); odd = []; kinds = Counter(); rec = []
for f in sorted(glob.glob(os.path.join(N, 'gov-*.txt'))):
    name = os.path.basename(f); kind = name.split('-')[1]; hs = heads(f); per[kind][0] += 1; per[kind][1] += len(hs)
    o = os.path.join(O, name)
    if os.path.exists(o): per[kind][2] += len(heads(o))
    seen = Counter()
    for h in hs:
        who = re.search(r'HEAD asked: (.+?) \(', h).group(1); seen[who] += 1
        text = h.split('record')[0]
        d = [float(x) for x in re.findall(r'lost the defence of [^.]*? to an attack at ([\d.]+) to 1', text)]
        d += [float(x) for x in re.findall(r'and lost the defence of [^.]*? to an attack at ([\d.]+) to 1', text) if False]
        a = [float(x) for x in re.findall(r'(?:thrown back|failed|repulsed)[^.]*? at ([\d.]+) to 1', text)]
        a2 = [float(x) for x in re.findall(r'attack(?:ed)? [^.]*? at ([\d.]+) to 1[^.]*?(?:thrown back|failed|was beaten)', text)]
        kinds[(len(d), len(a) + len(a2), 'directive' if 'directive was not carried out' in text else '')] += 1
        ev.append((name, who, d, a + a2, text.strip()[:260]))
        m = re.search(r'record (\d+) won of (\d+)', h)
        if m: rec.append((int(m.group(1)), int(m.group(2))))
    for w, c in seen.items():
        if c > 1: names[(name, w)] = c
print('heads asked, by set (wars, on this version, on the old one in the same wars):')
for k, (w, n, o) in sorted(per.items()): print(f'  {k:<8}{w:>4} wars {n:>4} now {o:>4} before')
print('total', sum(v[1] for v in per.values()), 'now,', sum(v[2] for v in per.values()), 'before, in', sum(v[0] for v in per.values()), 'wars')
dd = [x for e in ev for x in e[2]]; aa = [x for e in ev for x in e[3]]
print(f'defences the cabinet holds against him: {len(dd)}; odds median {st.median(dd) if dd else 0}; 1.5 or over: {sum(1 for x in dd if x >= 1.5)}; highest {max(dd) if dd else 0}')
print(f'attacks thrown back it holds against him: {len(aa)}; odds median {st.median(aa) if aa else 0}; under 1.5: {sum(1 for x in aa if x < 1.5)}')
print('evidence shapes (defences, attacks, with a missed directive):', dict(kinds))
print('the same general asked twice or more in one war:', dict(names))
won = [w / max(1, n) for w, n in rec]
print(f'his record when asked: won half or more {sum(1 for x in won if x >= 0.5)} of {len(won)}; three in four or more {sum(1 for x in won if x >= 0.75)}')
if '-v' in sys.argv:
    for e in ev: print('  ', e[0], '|', e[4])
