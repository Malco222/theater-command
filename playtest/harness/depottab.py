#!/usr/bin/env python3
# depottab: one row a war from probe92's DEPOTS lines.   python3 depottab.py 'out/v831h/*.txt' 'out/v831i/*.txt'
# The variant is the file's first word (before, minded, synth, foot, lean); the seat and seed are read from the line.
import sys, glob, re, os
rows = []
for pat in sys.argv[1:]:
    for f in sorted(glob.glob(pat)):
        base = os.path.basename(f)[:-4]
        parts = base.split('-')
        var = parts[-1] if parts[0] == 'depots' else parts[0]
        for line in open(f):
            m = re.match(r'DEPOTS (\w+) seed (\d+)[^:]*: (FELL|STANDS to) ([A-Z][a-z]+ \d+) \| supplies first short ([^,]+), short in (\d+) months of (\d+).*?\| oil first short ([^,]+), short in (\d+) months, worst (\d+)% met \| privation took a tenth of the will ([^,]+), reached its cap ([^,]+), months at the cap (\d+), peak ([\d.]+) points \([^)]*\), on the last card ([\d.]+) points = (\d+)% \| farms: (\d+) orders for (\d+) in all.*?plants ([\d.]+) oil a month at most \| divisions at most (\d+)', line)
            if not m: continue
            nat, seed, end, when, sup1, supN, months, oil1, oilN, oilWorst, tenth, cap, capN, peak, lastPts, lastPc, farmN, farmCost, plants, divs = m.groups()
            rows.append((nat, int(seed), var, 'fell' if end == 'FELL' else 'stands', when, int(supN), oil1, int(oilN), int(oilWorst), cap, int(capN), float(lastPts), int(lastPc), float(plants), int(divs), int(farmN)))
order = {'before': 0, 'synth': 1, 'minded': 2, 'foot': 3, 'lean': 4}
rows.sort(key=lambda r: (r[0], order.get(r[2], 9), r[1]))
print('%-4s %-7s %4s  %-7s %-15s %9s  %-15s %8s %6s  %-15s %6s  %-12s %6s %5s' % ('seat', 'variant', 'seed', 'end', 'when', 'sup short', 'oil first short', 'months', 'worst', 'at its cap from', 'months', 'last card', 'plants', 'divs'))
for r in rows:
    print('%-4s %-7s %4d  %-7s %-15s %9d  %-15s %8d %5d%%  %-15s %6d  %5.2f = %3d%% %6.1f %5d' % (r[0], r[2], r[1], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11], r[12], r[13], r[14]))
