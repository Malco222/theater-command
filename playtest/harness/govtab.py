#!/usr/bin/env python3
"""Tables from directives.txt (written by govread.py DIR directives)."""
import re, sys, collections
rows = []
for line in open(sys.argv[1]):
    f = line[:22].strip(); at = line[23:38].strip(); kind = line[39:45].strip()
    end = line.rsplit('|', 1)[1].strip()
    how = re.sub(r' in \d+$', '', end); took = int(end.rsplit(' in ', 1)[1]) if ' in ' in end else None
    r = dict(file=f, at=at, kind=kind, how=how, took=took, line=line)
    m = re.search(r'foes\s+(\d+) mine\s+(\d+)', line)
    if m: r['foes'] = int(m.group(1)); r['mine'] = int(m.group(2))
    m = re.search(r' F(\d) ', line); r['fort'] = int(m.group(1)) if m else 0
    r['cap'] = ' CAP ' in line
    m = re.search(r'nb-corps (\d+) hops\s+(-?\d+)', line)
    if m: r['nbc'] = int(m.group(1)); r['hops'] = int(m.group(2))
    r['group'] = f.split('-')[0]; r['nat'] = f.split('-')[1]
    rows.append(r)
def tally(sel):
    c = collections.Counter(r['how'] for r in rows if sel(r)); n = sum(c.values()); ended = n - c['open']
    return n, c['carried out'], c['not carried out'] + c['set aside'], c['withdrawn'], c['open'], (100 * c['carried out'] / ended if ended else 0)
def show(label, sel):
    n, done, miss, wd, op, pct = tally(sel)
    print(f"{label:52} {n:4} given | carried out {done:3} | not {miss:3} | withdrawn {wd} | still open {op:2} | {pct:3.0f}% of those ended")
active = lambda r: r['group'] in ('play', 'cab')
print('ALL', len(rows)); 
for gname in ('play', 'cab', 'idle', 'aside'): show(f'group {gname}: all', lambda r, g=gname: r['group'] == g)
print('\n-- the player who fights (scripted, with or without a cabinet): by kind')
for k in ('take', 'relief', 'hold', 'raise'): show('  ' + k, lambda r, k=k: active(r) and r['kind'] == k)
show('  all', active)
tk = lambda r: active(r) and r['kind'] in ('take', 'relief') and 'foes' in r
print('\n-- take and relief, by what stood against it when it was given')
show('  nothing in the town', lambda r: tk(r) and r['foes'] == 0)
show('  my troops beside it 3 times the defenders or more', lambda r: tk(r) and r['foes'] > 0 and r['mine'] >= 3 * r['foes'])
show('  1.5 to 3 times', lambda r: tk(r) and r['foes'] > 0 and 1.5 * r['foes'] <= r['mine'] < 3 * r['foes'])
show('  1 to 1.5 times', lambda r: tk(r) and r['foes'] > 0 and r['foes'] <= r['mine'] < 1.5 * r['foes'])
show('  fewer than the defenders', lambda r: tk(r) and r['foes'] > 0 and r['mine'] < r['foes'])
print('-- by walls')
for lv in (0, 1, 2, 3): show(f'  fortress level {lv}', lambda r, lv=lv: tk(r) and r['fort'] == lv)
show('  a capital', lambda r: tk(r) and r['cap'])
show('  a fortress 2+ or fewer troops than the defenders', lambda r: tk(r) and (r['fort'] >= 2 or (r['foes'] > 0 and r['mine'] < r['foes'])))
show('  neither', lambda r: tk(r) and not (r['fort'] >= 2 or (r['foes'] > 0 and r['mine'] < r['foes'])))
show('  across water: none of my corps beside it, no land way', lambda r: r['kind'] in ('take','relief') and r.get('nbc') == 0 and r.get('hops') == -1)
print('\n-- carried out in the month it was given (take and relief)')
d = [r for r in rows if tk(r) and r['how'] == 'carried out']; print(f"  {sum(1 for r in d if r['took'] == 0)} of {len(d)} in the same month, {sum(1 for r in d if r['took'] <= 1)} within a month")
print('\n-- the first directive of each war from 1939 (by nation, the player who fights)')
first = {}
for r in rows:
    if r['group'] in ('play', 'cab', 'idle', 'aside') and r['file'] not in first: first[r['file']] = r
c = collections.Counter()
for f, r in sorted(first.items()):
    c[(r['nat'], r['kind'], r['how'])] += 1
for (nat, kind, how), n in sorted(c.items()): print(f"  {nat:4} {kind:6} {how:16} {n}")
print('\n-- by nation (the player who fights)')
for nat in sorted(set(r['nat'] for r in rows)): show('  ' + nat, lambda r, nat=nat: active(r) and r['nat'] == nat)
print('\n-- raise, by nation, all groups')
for nat in sorted(set(r['nat'] for r in rows)): show('  ' + nat, lambda r, nat=nat: r['kind'] == 'raise' and r['nat'] == nat)
print('\n-- hold')
show('  threatened only (the player who fights)', lambda r: active(r) and r['kind'] == 'hold' and 'threatened' in r['line'])
show('  under siege (the player who fights)', lambda r: active(r) and r['kind'] == 'hold' and 'under siege' in r['line'])
show('  threatened only (idle)', lambda r: r['group'] == 'idle' and r['kind'] == 'hold' and 'threatened' in r['line'])
show('  under siege (idle)', lambda r: r['group'] == 'idle' and r['kind'] == 'hold' and 'under siege' in r['line'])
