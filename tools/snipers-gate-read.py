#!/usr/bin/env python3
"""The sniper gate's marks, from the gate's JSONL files in a directory (arms off, on, p0, carve): the design director's
ruling of 25 Nov 16:30Z with the bars as re-read at 17:48Z (design-director/snipers-gate-1125.md).

  python3 tools/snipers-gate-read.py <dir>     (dir: the gate's JSONL files, named <set>-<arm>.jsonl)

Not a test and not shipped: the reader of tools/snipers-gate.ts. G5's foot corps come from footcorps.json beside the runs.
"""
import json, os, sys, glob, statistics as st
from collections import defaultdict

G = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
D = {}
for f in glob.glob(os.path.join(G, '*-*.jsonl')):
    name = os.path.basename(f)[:-6]
    s, a = name.split('-', 1)
    rows = [json.loads(l) for l in open(f) if l.strip()]
    D[(s, a)] = {r['key']: r for r in rows}

def rows(s, a): return D.get((s, a), {})
def win(rs): return sum(1 for r in rs if r['w'] == 'att') / len(rs) if rs else float('nan')
def paired(s, a, b='off'):
    A, B = rows(s, a), rows(s, b)
    ks = sorted(set(A) & set(B))
    return [A[k] for k in ks], [B[k] for k in ks]
def fmt(x, n=3): return 'nan' if x != x else f'{x:+.{n}f}'
out = []
def say(*a): out.append(' '.join(str(x) for x in a))
fails = []
def mark(name, ok, text):
    say(('PASS ' if ok else 'FAIL ') + name + ': ' + text)
    if not ok: fails.append(name)


SETS = ['F', 'T', 'Y', 'P', 'B']
def cells_of(rs, s): 
    by = defaultdict(list)
    for r in rs: by[r['cell'].split(':')[0] if s == 'F' else r['cell']].append(r)
    return by
def shift_by_cell(s, a):
    on, off = paired(s, a)
    A, B = cells_of(on, s), cells_of(off, s)
    return {c: win(A[c]) - win(B[c]) for c in A}

# ---- the pairing check: the dice placebo flips at most 3 battles in 100 on each set (17:48Z, 2)
for s in SETS:
    on, off = paired(s, 'p0')
    if not on: continue
    fl = sum(1 for x, y in zip(on, off) if x['w'] != y['w']) / len(on)
    mark(f'pairing {s} (p0 flips)', fl <= 0.03, f'{fl:.3f} of {len(on)} (0.03)')

# ---- G2 who wins: pooled bars against OFF with no floor (carve-only must pass them too); F per ground floored by carve-only
LIM = {'F': 0.02, 'T': 0.05, 'Y': 0.05, 'P': 0.05}
for s in SETS:
    for a in ['on', 'carve']:
        on, off = paired(s, a)
        if not on: continue
        d = win(on) - win(off)
        flips = sum(1 for x, y in zip(on, off) if x['w'] != y['w'])
        per = shift_by_cell(s, a)
        line = f'{s} {a}: n={len(on)} att wins off {win(off):.3f} {a} {win(on):.3f} diff {fmt(d)} flips {flips}'
        if s in ('F', 'B'): line += ' | ' + ' '.join(f'{c} {fmt(v, 2)}' for c, v in sorted(per.items()))
        say(line)
        tag = '' if a == 'on' else ' (carve-only)'
        if s in LIM: mark(f'G2 {s} pooled{tag}', abs(d) <= LIM[s], f'{fmt(d)} (limit ±{LIM[s]})')
        if s == 'B' and a == 'on': mark('G2 B per pairing', all(abs(v) <= 0.06 for v in per.values()), f'max |diff| {max(abs(v) for v in per.values()):.3f} (0.06, no floor)')
    if s == 'F' and rows('F', 'on'):
        per = shift_by_cell('F', 'on'); fl = shift_by_cell('F', 'carve') if rows('F', 'carve') else {}
        floor = max([abs(v) for v in fl.values()] or [0.0]); bar = max(0.05, floor)
        mark('G2 F per ground', max(abs(v) for v in per.values()) <= bar + 1e-9, f'max |diff| {max(abs(v) for v in per.values()):.3f} (bar {bar:.3f}: 0.05 or carve-only worst {floor:.3f})')

# ---- G1 gap to the dice on F, against OFF: each ground OFF + 0.06 (or carve-only's worst rise), all grounds OFF + 0.02
def gaps(a):
    R = list(rows('F', a).values())
    if not R: return None
    by = defaultdict(list)
    for r in R: by[r['cell']].append(r)
    g = defaultdict(list)
    for c, rs in by.items(): g[c.split(':')[0]].append(abs(win(rs) - rs[0]['chance']))
    return {k: sum(v) / len(v) for k, v in g.items()}
G1 = {a: gaps(a) for a in ['off', 'on', 'carve', 'p0']}
for a, v in G1.items():
    if v: say(f'G1 {a}: ' + ' '.join(f'{k} {x:.3f}' for k, x in sorted(v.items())) + f' | all {sum(v.values()) / len(v):.3f}')
if G1['on'] and G1['off']:
    o, n = G1['off'], G1['on']; allo = sum(o.values()) / len(o)
    rise = {k: n[k] - o[k] for k in n}
    floor = max([G1['carve'][k] - o[k] for k in o] or [0.0]) if G1['carve'] else 0.0
    bar = max(0.06, floor); worst = max(rise, key=rise.get)
    mark('G1 each ground', rise[worst] <= bar + 1e-9, f'worst rise {rise[worst]:+.3f} {worst} (bar {bar:.3f}: 0.06 or carve-only worst {floor:+.3f})')
    for a in ['on', 'carve']:
        if G1[a]: mark('G1 all grounds' + ('' if a == 'on' else ' (carve-only)'), sum(G1[a].values()) / len(G1[a]) - allo <= 0.02 + 1e-9, f'{sum(G1[a].values()) / len(G1[a]) - allo:+.3f} (OFF + 0.02)')

# ---- G3 the clock: F pooled OFF + 0.02, P pooled OFF + 0.03 (carve-only too); P each cell at most 3 battles (or carve-only's worst) over OFF
clock = lambda rs: sum(1 for r in rs if r['how'] == 'time')
for s, lim in (('F', 0.02), ('P', 0.03)):
    for a in ['on', 'carve']:
        on, off = paired(s, a)
        if not on: continue
        d = (clock(on) - clock(off)) / len(on)
        mark(f'G3 {s} pooled' + ('' if a == 'on' else ' (carve-only)'), d <= lim + 1e-9, f'{clock(on) / len(on):.3f} against OFF {clock(off) / len(off):.3f}: {d:+.3f} (OFF + {lim})')
if rows('P', 'on'):
    def excess(a):
        on, off = paired('P', a)
        A, B = cells_of(on, 'P'), cells_of(off, 'P')
        return {c: clock(A[c]) - clock(B[c]) for c in A}
    ex = excess('on'); fl = excess('carve') if rows('P', 'carve') else {}
    floor = max(list(fl.values()) or [0]); bar = max(3, floor)
    say('G3 P battles on the clock over OFF: ' + ' '.join(f'{c.split(":")[-1]} {v:+d}' for c, v in sorted(ex.items())))
    mark('G3 P each cell', max(ex.values()) <= bar, f'worst {max(ex.values()):+d} battles (bar {bar}: 3 or carve-only worst {floor:+d})')

# ---- G4 the cost test (b356) with snipers on
def bills(R):
    cl = lambda x, a, b: max(a, min(b, x))
    return cl(0.14 / R, 0.02, 0.2), cl(0.07 + 0.1 * R, 0.06, 0.45) * (1 if R >= 1.2 else 0.6)
for a in ['off', 'on']:
    R = list(rows('C', a).values())
    if not R: continue
    cl = lambda x, lo, hi: max(lo, min(hi, x))
    cells = {'att': [0, 0, 0], 'def': [0, 0, 0]}; inA = inD = 0
    for r in R:
        bA, bD = bills(r['R'])
        c = cells['att' if r['w'] == 'att' else 'def']; c[0] += r['la'] / bA; c[1] += r['ld'] / bD; c[2] += 1
        lo, hi = r['odds'] * r['dice'][0], r['odds'] * r['dice'][1]
        ds = [bills(lo)[1], bills(hi)[1], bills(cl(1.2, lo, hi))[1]]
        if bills(hi)[0] * 0.75 <= r['la'] <= bills(lo)[0] * 1.25: inA += 1
        if min(ds) * 0.75 <= r['ld'] <= max(ds) * 1.25: inD += 1
    ok = all(c[2] > 20 and 0.8 < c[0] / c[2] < 1.25 and 0.8 < c[1] / c[2] < 1.25 for c in cells.values()) and inA / len(R) >= 0.7 and inD / len(R) >= 0.7
    txt = ' '.join(f'{k} won n{c[2]} a{c[0] / max(1, c[2]):.2f} d{c[1] / max(1, c[2]):.2f}' for k, c in cells.items()) + f' inA {inA / len(R):.2f} inD {inD / len(R):.2f}'
    if a == 'on': mark('G4 cost (b356) on', ok, txt)
    else: say(f'G4 off: {"holds" if ok else "FAILS"} {txt}')

# ---- G5 the card's toll, pooled over its nine cells: each side's foot share ON minus OFF within ±0.02; a foot corps' split weight traced past +0.05
FOOTC = json.load(open('/home/user/runs/snp-gate/footcorps.json')) if os.path.exists('/home/user/runs/snp-gate/footcorps.json') else {}
on, off = paired('F', 'on')
if on:
    pick = lambda r: r['cell'].split(':')[0] in ('forest', 'urban', 'plains') and float(r['cell'].split(':')[1]) in (1.0, 1.2, 1.4)
    P = [(x, y) for x, y in zip(on, off) if pick(x)]
    fa = st.mean(x['fa'] for x, _ in P) - st.mean(y['fa'] for _, y in P)
    fd = st.mean(x['fd'] for x, _ in P) - st.mean(y['fd'] for _, y in P)
    mark('G5 foot share (pooled)', max(abs(fa), abs(fd)) <= 0.02, f'att {fmt(fa)} def {fmt(fd)} over {len(P)} battles (±0.02)')
    rise = defaultdict(list); allc = defaultdict(list)
    for x, y in P:
        fc = FOOTC.get(x['key'], {})
        for side, sk in (('att', 'sa'), ('def', 'sd')):
            for c in y[sk]:
                d = x[sk].get(c, 1) - y[sk].get(c, 1)
                allc[(side, c)].append(d)
                if int(c) in fc.get(side, []): rise[(side, c)].append(d)
    say('G5 split weights ON-OFF by corps: ' + ' '.join(f'{s}{c}{"*" if (s, c) in rise else ""} {st.mean(v):+.3f}' for (s, c), v in sorted(allc.items())) + ' (* foot)')
    if FOOTC:
        worst = max((st.mean(v) for v in rise.values()), default=0.0)
        mark('G5 foot corps split (traced past +0.05)', worst <= 0.05, f'most a foot corps rose {worst:+.3f} (+0.05)')
    else: say('G5 foot corps: footcorps.json missing, not read')

# ---- G6 verdicts outside the dice's run: |ON-OFF| within 0.01, or carve-only's own |carve-OFF| on that set
for s in SETS:
    vals = {}
    for a in ['off', 'on', 'carve', 'p0']:
        R = list(rows(s, a).values())
        if R: vals[a] = sum(r['out'] for r in R) / len(R)
    if 'on' in vals and 'off' in vals:
        floor = abs(vals['carve'] - vals['off']) if 'carve' in vals else 0.0
        d = abs(vals['on'] - vals['off'])
        say(f'G6 {s}: ' + ' '.join(f'{a} {v:.3f}' for a, v in vals.items()))
        mark(f'G6 {s} outside the run', d <= max(floor, 0.01) + 1e-9, f'|on-off| {d:.3f} (0.01 or carve-only {floor:.3f})')

# ---- G7 the plain attack (mark 4)
for a in ['off', 'on']:
    R = list(rows('M', a).values())
    if not R: continue
    by = defaultdict(list)
    for r in R: by[r['cell']].append(r)
    k = defaultdict(list)
    for c, rs in by.items(): k[c.split(':')[0]].append(win(rs) - rs[0]['chance'])
    res = {e: (sum(abs(x) for x in v) / len(v), sum(v) / len(v)) for e, v in k.items()}
    ok = all(-0.1 <= b <= 0.1 and g <= 0.15 for g, b in res.values()) and sum(g for g, _ in res.values()) / len(res) <= 0.1 and abs(sum(b for _, b in res.values()) / len(res)) <= 0.04
    txt = ' '.join(f'{e} g{g:.2f} b{b:+.2f}' for e, (g, b) in sorted(res.items()))
    if a == 'on': mark('G7 plain attack on', ok, txt)
    else: say(f'G7 off: {"holds" if ok else "FAILS"} {txt}')

# ---- W: the pin's price at bite 0.6, reported only (pin 20 is ON; W on80 at pin 80)
w20 = [r for r in rows('F', 'on').values() if r['cell'].split(':')[0] in ('plains', 'forest', 'urban')]
o = [r for r in rows('F', 'off').values() if r['cell'].split(':')[0] in ('plains', 'forest', 'urban')]
w80 = list(rows('W', 'on80').values())
if w20 and o:
    say(f'W att wins: off {win(o):.3f} pin20 {win(w20):.3f} pin80 {win(w80) if w80 else float("nan"):.3f}' + (f' slope {100 * (win(w80) - win(w20)) / 60:+.3f} points a pin-second' if w80 else ''))

# ---- reported meters (on)
R = list(rows('F', 'on').values()); R0 = list(rows('F', 'off').values())
if R:
    m = lambda rs, k: st.mean(r[k] for r in rs if r.get(k) is not None) if any(r.get(k) is not None for r in rs) else float('nan')
    say(f'meters F off/on: contact {m(R0, "contact"):.0f}/{m(R, "contact"):.0f} s, start line {m(R0, "arrived"):.0f}/{m(R, "arrived"):.0f}, went {m(R0, "went"):.0f}/{m(R, "went"):.0f}, decision {m(R0, "sec"):.0f}/{m(R, "sec"):.0f}, blocks routed {m(R0, "routed"):.2f}/{m(R, "routed"):.2f}')
    T = [t for r in R for t in r.get('snp', [])]
    if T:
        say(f'teams: {len(T)}; seconds hidden {st.mean(t["hid"] for t in T) / 10:.0f} vague {st.mean(t["vague"] for t in T) / 10:.0f} found {st.mean(t["found"] for t in T) / 10:.0f} silenced {st.mean(t["quiet"] for t in T) / 10:.0f} slipping {st.mean(t["slip"] for t in T) / 10:.0f} firing {st.mean(t["fired"] for t in T) / 10:.0f}; slips {st.mean(t["slips"] for t in T):.2f}; withdrew {st.mean(t["out"] for t in T):.2f}; worn share {st.mean(t["worn"] for t in T):.4f}')
        for side in ('att', 'def'):
            S = [t for t in T if t['side'] == side]
            say(f'  {side}: start hid {sum(1 for t in S if t["start"] == "hid") / len(S):.2f} cover {sum(1 for t in S if t["start"] == "cover") / len(S):.2f}; ever fired {sum(1 for t in S if t["fired"] > 0) / len(S):.2f}; firing {st.mean(t["fired"] for t in S) / 10:.0f} s')
    pins = [r['pin'] for r in R if r.get('pin')]
    if pins: say(f'pin: blocks pinned a battle {st.mean(p["n"] for p in pins):.1f}, mean seconds {st.mean(p["mean"] for p in pins):.1f}, most {max(p["most"] for p in pins)}; closed tank seconds a battle {st.mean(r.get("closed", 0) for r in R):.0f}')
say('FAILED: ' + (', '.join(fails) if fails else 'none'))
print('\n'.join(out))
