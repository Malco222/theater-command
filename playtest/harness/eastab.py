#!/usr/bin/env python3
"""The eastern front table from probe89's lines. usage: eastab.py 'GLOB' [--months] [--label TEXT]
Sets history beside the middle war: the opening, who struck, the deepest advance, the towns, the way back, Berlin.
--months prints one row a calendar month (the middle value over the wars) for a chart."""
import sys, glob, re, statistics as st
MON = ['January','February','March','April','May','June','July','August','September','October','November','December']
def turn(s):
    m = re.match(r'(\w+) (\d{4})', s.strip());
    return (int(m.group(2)) - 1939) * 12 + MON.index(m.group(1)) + 1 - 8 if m else None
def date(t):
    t = int(round(t)); y = 1939 + (t + 7) // 12; m = (t + 7) % 12
    return f"{MON[m][:3]} {y}"
def med(a): return st.median(a) if a else None
def dm(a, n=None): return f"{date(med(a))}" + (f" ({len(a)} of {n})" if n is not None else '') if a else 'never'
HIST_FALL = {'Minsk':'June 1941','Kiev':'September 1941','Smolensk':'July 1941','Kharkov':'October 1941','Rostov':'July 1942','Crimea':'July 1942','Odessa':'October 1941','Latvia':'July 1941'}
HIST_BACK = {'Minsk':'July 1944','Kiev':'November 1943','Smolensk':'September 1943','Kharkov':'August 1943','Rostov':'February 1943','Crimea':'May 1944','Odessa':'April 1944','Latvia':'October 1944'}
HIST_SOV = {'Bucharest':'August 1944','Warsaw':'January 1945','Budapest':'February 1945','Konigsberg':'April 1945','Vienna':'April 1945','Berlin':'May 1945'}
args = [a for a in sys.argv[1:] if not a.startswith('--')]; months = '--months' in sys.argv
label = sys.argv[sys.argv.index('--label') + 1] if '--label' in sys.argv else args[0]
if '--label' in sys.argv: args = [a for a in args if a != label]
east, city, years, F, army, rings, seat = {}, {}, {}, {}, {}, {}, {}
for f in sorted(glob.glob(args[0])):
    for ln in open(f, errors='replace'):
        m = re.match(r'(\w+) seed (\d+)[: ]\s*(.*)', ln)
        if not m: continue
        k, s, rest = m.group(1), int(m.group(2)), m.group(3)
        if k == 'EAST': east[s] = rest
        elif k == 'CITY': city[s] = rest
        elif k == 'YEARS': years[s] = rest
        elif k == 'F': F.setdefault(s, []).append(rest)
        elif k == 'ARMY': army.setdefault(s, []).append(rest)
        elif k == 'RING': rings.setdefault(s, []).append(rest)
        elif k == 'SEAT': seat[s] = rest
n = len(east)
print(f"THE EASTERN FRONT: {label} ({n} wars, seeds {min(east)} to {max(east)})" if n else 'no wars')
if not n: sys.exit()
errs = [s for s in east if 'ERROR' in east[s]]
def field(rest, key):
    m = re.search(key + r' ([^|]*)', rest); return m.group(1).strip() if m else ''
war = {s: turn(field(east[s], 'war from')) for s in east if 'war from' in east[s]}
struck = {}
for s in war: struck[field(east[s], 'war from').split('struck by ')[-1]] = struck.get(field(east[s], 'war from').split('struck by ')[-1], 0) + 1
print(f"The war opens (history June 1941): middle {dm(list(war.values()))}; in 1941 in {sum(1 for t in war.values() if 17 <= t <= 28)} of {n}; earliest {date(min(war.values()))}, latest {date(max(war.values()))}; no war in {n - len(war)}")
print(f"Who struck first: {', '.join(f'{k} {v}' for k, v in sorted(struck.items(), key=lambda x: -x[1]))}   (history: Germany)")
deep_t, deep_n, deep_home, deep_city = [], [], [], {}
for s in war:
    d = field(east[s], 'deepest'); m = re.match(r'(\w+ \d{4}): (\d+) provinces of the line lost \((\d+) of (\d+) home\), east to (\d+) \((\w+)\)', d)
    if m: deep_t.append(turn(m.group(1))); deep_n.append(int(m.group(2))); deep_home.append(int(m.group(3))); deep_city[m.group(6)] = deep_city.get(m.group(6), 0) + 1
print(f"The enemy's deepest advance (history November 1942, about 35 of these provinces, to Stalingrad): middle {dm(deep_t)}; home provinces lost then: middle {med(deep_home)}, least {min(deep_home)}, most {max(deep_home)}; reached the longitude of: {', '.join(f'{k} {v}' for k, v in sorted(deep_city.items(), key=lambda x: -x[1]))}")
def when(key, hist):
    a = [turn(field(east[s], key)) for s in war if turn(field(east[s], key)) is not None]
    print(f"{key} (history {hist}): middle {dm(a)}; in {len(a)} of {len(war)} wars")
when('home ground whole again', 'about August 1944'); when('on German home ground', 'October 1944')
bw = {}
bt = []
for s in east:
    b = field(east[s], 'Berlin')
    if b.startswith('never'): bw['nobody'] = bw.get('nobody', 0) + 1
    else: bw[b.split()[0]] = bw.get(b.split()[0], 0) + 1; bt.append(turn(' '.join(b.split()[1:3])))
print(f"Berlin taken by (history: the Soviet Union, May 1945): {', '.join(f'{k} {v}' for k, v in sorted(bw.items(), key=lambda x: -x[1]))}; middle {dm(bt)}")
gf = [turn(field(east[s], 'Germany').replace('fell ', '')) for s in east if 'Germany fell' in east[s]]
print(f"Germany fell: {len(gf)} of {n}; middle {dm(gf)}; earliest {date(min(gf)) if gf else '-'}, latest {date(max(gf)) if gf else '-'}")
for c in ['Moscow', 'Leningrad', 'Stalingrad']:
    lost = [s for s in east if field(east[s], c) and 'never' not in field(east[s], c)]
    print(f"{c} lost in {len(lost)} of {n} wars (history: never){': seeds ' + ', '.join(map(str, lost)) if lost and len(lost) <= 12 else ''}")
if seat:
    stands = sum(1 for s in seat if 'stands' in seat[s]); print(f"The seat by hand: stands in {stands} of {len(seat)}")
print()
print('TOWNS: fell to the enemy / back in Soviet hands, the middle war (wars it happened in), history beside it')
cl = {}
for s in city:
    for part in city[s].split(' | '):
        nm, _, log = part.partition(': '); cl.setdefault(nm, {})[s] = [] if 'never' in log else [(x.split()[0], turn(' '.join(x.split()[1:3]))) for x in log.split(', ')]
for nm in HIST_FALL:
    fell = [next(t for w, t in cl[nm][s] if w not in ('SOV',)) for s in cl.get(nm, {}) if any(w != 'SOV' for w, t in cl[nm][s])]
    back = []
    for s in cl.get(nm, {}):
        seq = cl[nm][s]; lost = False
        for w, t in seq:
            if w != 'SOV': lost = True
            elif lost: back.append(t); break
    print(f"  {nm:10s} fell {dm(fell, n):24s} history {HIST_FALL[nm]:15s} | back {dm(back, n):24s} history {HIST_BACK[nm]}")
print('TOWNS the Red Army took (or freed), the middle war, history beside it')
for nm in HIST_SOV:
    a = [next(t for w, t in cl[nm][s] if w in ('SOV', 'POL') and t > 20) for s in cl.get(nm, {}) if any(w in ('SOV', 'POL') and t > 20 for w, t in cl[nm][s])]
    print(f"  {nm:10s} {dm(a, n):24s} history {HIST_SOV[nm]}")
print()
print('BY YEAR, all wars together: attacks ordered and won, corps dead')
tot = {}
for s in years:
    for part in years[s].split(' | '):
        m = re.match(r'(\d{4}): Soviet attacks (\d+) won (\d+), enemy attacks (\d+) won (\d+), dead Soviet (\d+) enemy (\d+)', part)
        if m: y = m.group(1); v = list(map(int, m.groups()[1:])); tot[y] = [a + b for a, b in zip(tot.get(y, [0] * 6), v)]
for y in sorted(tot):
    v = tot[y]; print(f"  {y}: Red Army attacks {v[0]:5d}, won {100 * v[1] // max(1, v[0]):3d}% | enemy attacks {v[2]:5d}, won {100 * v[3] // max(1, v[2]):3d}% | corps dead: Soviet {v[4]:4d}, enemy in the east {v[5]:4d}")
# odds
so, ao = [], []
rows = {}
for s in F:
    for ln in F[s]:
        m = re.match(r't(\d+) [^:]+: Soviet (\d+) corps (\d+) divs \(front (\d+)\), enemy front (\d+), German corps (\d+), home lost (\d+), line lost (\d+), beyond (\d+), enemy east to (\d+), Soviet west to (\d+), front (\d+) provinces \| Soviet attacks (\d+) won (\d+) odds ([\d.]+) \| enemy attacks (\d+) won (\d+) odds ([\d.]+) \| dead Soviet (\d+) enemy (\d+)', ln)
        if not m: continue
        v = m.groups(); t = int(v[0]); rows.setdefault(t, []).append(v)
        if int(v[12]): so.append((float(v[14]), int(v[12])))
        if int(v[15]): ao.append((float(v[17]), int(v[15])))
w = lambda a: sum(x * k for x, k in a) / max(1, sum(k for x, k in a))
print(f"Mean odds of an attack: the Red Army's {w(so):.2f}, its enemy's {w(ao):.2f}")
print()
print('THE ARMIES each February, the middle war: Soviet corps (divisions) against German corps (divisions)')
ar = {}
for s in army:
    for ln in army[s]:
        m = re.match(r'(\w+ \d{4}): SOV (\d+) corps \(([^)]*)\), (\d+) divisions, strength (\d+)%, experience (\d+); GER (?:(\d+) corps \(([^)]*)\), (\d+) divisions, strength (\d+)%, experience (\d+)|none)', ln)
        if m: ar.setdefault(m.group(1), []).append(m.groups())
for d in sorted(ar, key=turn):
    a = ar[d]; g = [x for x in a if x[6]]
    art = lambda k: med([int(re.search(r'art (\d+)', x[k]).group(1)) if re.search(r'art (\d+)', x[k]) else 0 for x in (a if k == 2 else g)]) if (a if k == 2 else g) else 0
    arm = lambda k: med([int(re.search(r'arm (\d+)', x[k]).group(1)) if re.search(r'arm (\d+)', x[k]) else 0 for x in (a if k == 2 else g)]) if (a if k == 2 else g) else 0
    print(f"  {d:14s} Soviet {med([int(x[1]) for x in a]):5.0f} corps ({med([int(x[3]) for x in a]):.0f} divisions; armoured {arm(2):.0f}, artillery {art(2):.0f}; strength {med([int(x[4]) for x in a]):.0f}%, experience {med([int(x[5]) for x in a]):.0f}) | German {(med([int(x[6]) for x in g]) if g else 0):5.0f} corps ({(med([int(x[8]) for x in g]) if g else 0):.0f} divisions; armoured {arm(7):.0f}; strength {(med([int(x[9]) for x in g]) if g else 0):.0f}%, experience {(med([int(x[10]) for x in g]) if g else 0):.0f}) in {len(g)} of {len(a)} wars")
big = sorted(((int(re.search(r'Soviet corps dead (\d+)', r).group(1)), s, r) for s in rings for r in rings[s]), reverse=True)
tr = sum(len(rings[s]) for s in rings)
print(); print(f"MONTHS a side lost three corps or more in the east: {tr} in {n} wars; the Red Army's worst months:")
for d, s, r in big[:5]: print(f"  seed {s}: {r[:260]}")
if errs: print(f"ERRORS in seeds {errs}")
if months:
    print(); print('MONTH BY MONTH, the middle war: turn, month, wars at war, Soviet corps, Soviet at the front, enemy at the front, German corps, home provinces lost, provinces beyond the line, Red Army attacks, enemy attacks')
    for t in sorted(rows):
        a = rows[t]; c = lambda i: med([int(x[i]) for x in a])
        print(f"  {t}\t{date(t)}\t{len(a)}\t{c(1):.0f}\t{c(3):.0f}\t{c(4):.0f}\t{c(5):.0f}\t{c(6):.1f}\t{c(8):.1f}\t{c(12):.1f}\t{c(15):.1f}")
