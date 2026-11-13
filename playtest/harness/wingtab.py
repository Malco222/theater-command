#!/usr/bin/env python3
"""Version 60: what a hawk or a dove table asks for, month by month, and what came of it (probe24 outputs with 'when asked').
usage: wingtab.py DIR PREFIX [-v]"""
import sys, re, glob, os
from collections import Counter, defaultdict
D, pre = sys.argv[1], sys.argv[2]; V = '-v' in sys.argv
MON = {m: i for i, m in enumerate('January February March April May June July August September October November December'.split())}
def t(s): m, y = s.split(); return (int(y) - 1939) * 12 + MON[m]
agg = defaultdict(Counter); rows = []
for f in sorted(glob.glob(os.path.join(D, pre + '*.txt'))):
    name = os.path.basename(f)[:-4]; wing = name.split('-')[1]; nat = name.split('-')[2]
    prov = {}; cur = None; mine = []
    for ln in open(f, errors='replace'):
        m = re.match(r'(\w+ \d{4}): trust ([\d.]+) .*?provinces (\d+), divs (\d+)', ln)
        if m: prov[t(m.group(1))] = (int(m.group(3)), int(m.group(4)), float(m.group(2))); continue
        m = re.match(r'(\w+ \d{4})\s+DIRECTIVE (\w+) from (.+?): (.+?); (\d+) months; reward (.+?); price if missed ([\d.]+)(.*)', ln)
        if m:
            rest = m.group(8); o = re.search(r"the cabinet's odds (over 99|[\d.]+)", rest); dv = re.search(r'when asked: held by (\w+), ([\d.]+) divs', rest)
            cur = dict(war=name, wing=wing, nat=nat, when=m.group(1), tt=t(m.group(1)), kind=m.group(2), title=m.group(4), months=int(m.group(5)), price=float(m.group(7)), odds=(99.0 if o and o.group(1) == 'over 99' else float(o.group(1)) if o else None), divs=float(dv.group(2)) if dv else None, hard='strongly held' in rest, again='asked before' in rest, end='open', endt=None)
            mine.append(cur); continue
        m = re.match(r'\s+(\w+ \d{4})\s+=> (.+)', ln)
        if m and cur and cur['end'] == 'open': cur['end'] = m.group(2).strip(); cur['endt'] = t(m.group(1)); continue
        if 'THE CABINET FELL' in ln: agg[(wing, nat)]['fell'] += 1
    # was the war going badly when it asked? provinces lower than a year before (from the six-month lines)
    ks = sorted(prov)
    def trend(tt):
        before = [k for k in ks if k <= tt]; 
        if len(before) < 3: return 0
        return prov[before[-1]][0] - prov[before[-3]][0]
    for d in mine:
        d['trend'] = trend(d['tt']); rows.append(d)
        agg[(wing, nat)][d['kind']] += 1; agg[(wing, nat)][d['kind'] + ' ' + d['end']] += 1
print(f"{'table':<8}{'nation':<7}{'asked':>6}  what came of it")
for k in sorted(agg):
    a = agg[k]; kinds = [x for x in a if ' ' not in x and x != 'fell']
    print(f"{k[0]:<8}{k[1]:<7}{sum(a[x] for x in kinds):>6}  " + '; '.join(f"{x} {a[x]} (done {a[x + ' carried out']}, missed {a[x + ' not carried out']}, withdrawn {a[x + ' withdrawn']})" for x in sorted(kinds)) + (f"; cabinet fell {a['fell']}" if a['fell'] else ''))
print()
takes = [d for d in rows if d['kind'] in ('take', 'relief') and d['odds'] is not None]
def line(label, xs):
    if not xs: print(f"  {label:<46} none"); return
    done = sum(1 for d in xs if d['end'] == 'carried out'); same = sum(1 for d in xs if d['end'] == 'carried out' and d['endt'] == d['tt'])
    print(f"  {label:<46} {len(xs):>3} asked, {done:>3} done ({100 * done / len(xs):.0f}%), {same} in the month asked; strongly held {sum(1 for d in xs if d['hard'])}; asked before {sum(1 for d in xs if d['again'])}; median odds {sorted(d['odds'] for d in xs)[len(xs) // 2]:.2f}")
for w in ('hawks', 'doves'):
    ws = [d for d in takes if d['wing'] == w]
    print(f"TOWNS ASKED FOR, {w} table ({len(ws)})")
    line('war going well or level (provinces not falling)', [d for d in ws if d['trend'] >= 0])
    line('war going badly (provinces down on a year before)', [d for d in ws if d['trend'] < 0])
    line('  of those, down 10 provinces or more', [d for d in ws if d['trend'] <= -10])
    for lo, hi in ((0, 1), (1, 1.5), (1.5, 3), (3, 1e9)):
        line(f"cabinet's odds {lo} to {hi if hi < 1e8 else 'up'}", [d for d in ws if lo <= d['odds'] < hi])
    line('no troops in the town when asked', [d for d in ws if d['divs'] == 0])
if V:
    for d in rows: print(f"   {d['war']:<20}{d['when']:<16}{d['kind']:<7}{d['title'][:34]:<36}{'odds ' + (f'{d['odds']:.2f}' if d['odds'] is not None else '-'):<12}{'divs ' + str(d['divs']) if d['divs'] is not None else '':<11}{'HARD ' if d['hard'] else ''}{'AGAIN ' if d['again'] else ''}trend {d['trend']:+d}  -> {d['end']}")
