#!/usr/bin/env python3
"""endtab.py DIR SET...: the end of the enemy power from probe41 TOTAL lines (files DIR/SET-SEED.txt): wars in which it
gave in and the median month, wars that came down to ten provinces without an end, the months from ten provinces to the
end, the longest run of months with no battle after ten, and the pocket surrenders."""
import re, sys, glob, statistics as st
MON = ['January','February','March','April','May','June','July','August','September','October','November','December']
def idx(s):
    m = re.match(r'(\w+) (\d{4})', s); return (int(m.group(2)) - 1939) * 12 + MON.index(m.group(1)) if m else None
def name(i): return f"{MON[i % 12]} {1939 + i // 12}"
d = sys.argv[1]
for h in sys.argv[2:]:
    rows = []
    for f in sorted(glob.glob(f"{d}/{h}-*.txt")):
        if not re.search(rf"{re.escape(h)}-\d+\.txt$", f): continue
        txt = open(f).read().splitlines()
        t = [l for l in txt if l.startswith('TOTAL')]
        if not t: print('NO TOTAL', f); continue
        l = t[-1]
        seed = int(re.search(r'seed (\d+)', l).group(1))
        gave = re.search(r'gave in (\w+ \d{4})', l)
        ten = re.search(r'down to ten provinces (\w+ \d{4}|never)', l).group(1)
        gap = re.search(r'months from ten provinces to the end (\S+);', l).group(1)
        idle = int(re.search(r'no battle against it after that (\d+)', l).group(1))
        played = re.search(r'played to (\w+ \d{4})', l).group(1)
        laid = sum(1 for x in txt if 'lays down its arms' in x or 'lay down their arms' in x)
        rows.append(dict(seed=seed, gave=gave.group(1) if gave else None, ten=ten, gap=gap, idle=idle, played=played, laid=laid, war='at war with me: true' in l))
    if not rows: print(h, 'no rows'); continue
    g = [idx(r['gave']) for r in rows if r['gave']]
    stuck = [r for r in rows if not r['gave'] and r['ten'] != 'never']
    lost = [r for r in rows if not r['gave'] and r['played'] != 'December 1945']
    gaps = [int(r['gap']) for r in rows if r['gap'].lstrip('-').isdigit()]
    print(f"{h:20s} wars {len(rows)}; the enemy gave in {len(g)}" + (f", median {name(int(st.median(g)))}, by end 1944 in {sum(x <= idx('December 1944') for x in g)}" if g else '') + f"; down to ten provinces and never finished {len(stuck)}" + (f" (seeds {','.join(str(r['seed']) for r in stuck)})" if stuck else '') + f"; player gone before the end {len(lost)}; months from ten provinces to the end: " + (f"median {st.median(gaps)}, most {max(gaps)}" if gaps else 'none') + f"; longest run with no battle after ten: most {max(r['idle'] for r in rows)}; surrenders in pockets a war {sum(r['laid'] for r in rows) / len(rows):.1f}")
    print('    ' + '; '.join(f"{r['seed']}: {r['gave'] or ('still at war' if r['war'] else 'no end')} (ten {r['ten']}, gap {r['gap']}, idle {r['idle']})" for r in sorted(rows, key=lambda r: r['seed'])))
