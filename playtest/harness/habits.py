#!/usr/bin/env python3
"""Version 64's orders by habit, from probe28 TOTAL lines (files DIR/HABIT-GER-SEED.txt).
usage: habits.py DIR[:LABEL] HABIT... [--seeds A-B]   A habit may be given as OTHERDIR/HABIT to read it from another folder.
For each habit: Berlin, the big towns and when they fell, attacks by year (fought, won, on a prepared line, men lost and
inflicted), and the Version 64 counters (folds, reliefs, probes, line raids, softened lines)."""
import re, sys, glob, os, statistics as st
args = sys.argv[1:]
lo, hi = 1, 9999
if '--seeds' in args:
    i = args.index('--seeds'); lo, hi = map(int, args[i+1].split('-')); del args[i:i+2]
d0, habits = args[0], args[1:]
MON = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']
def turn(month, year): return (int(year) - 1939) * 12 + MON.index(month) + 1 - 8
def read(spec):
    d, h = (os.path.split(spec) if '/' in spec else (d0, spec))
    out = {}
    for f in glob.glob(os.path.join(d, f'{h}-GER-*.txt')):
        seed = int(re.search(r'-(\d+)\.txt$', f).group(1))
        if not lo <= seed <= hi: continue
        t = [l for l in open(f) if l.startswith('TOTAL')]
        if not t: print('NO TOTAL', f); continue
        t = t[-1]
        m = re.search(r'\(fell (\d+)\)', t)
        r = {'fell': int(m.group(1)) if m else 77, 'berlin': 'Berlin in December 1944: GER' in t}
        m = re.search(r'provinces at the end (\d+)', t); r['prov'] = int(m.group(1)) if m else 0
        m = re.search(r'longest silence from January 1942: (\d+)', t); r['sil'] = int(m.group(1)) if m else None
        m = re.search(r'rings closed on the player (\d+) \((\d+) men\), surrenders (\d+) \((\d+) men\)', t)
        r['rings'], r['ringMen'], r['gave'], r['gaveMen'] = map(int, m.groups()) if m else (0, 0, 0, 0)
        m = re.search(r'; took (.*?); attacks by year', t)
        r['took'] = {}
        if m:
            for town, mo, yr in re.findall(r'(Moscow|Leningrad|Kiev|Stalingrad) (\w+) (\d{4})', m.group(1)): r['took'][town] = turn(mo, yr)
        m = re.search(r'attacks by year \(fought, won, on a prepared line won of fought, my men, theirs\): (.*?); Version 64', t)
        r['yr'] = {}
        if m:
            for part in m.group(1).split(' | '):
                x = re.match(r'(\d{4}) (\d+), (\d+), (\d+) of (\d+), (\d+), (\d+)', part.strip())
                if x: y, *v = map(int, x.groups()); r['yr'][y] = v
        m = re.search(r'folds (\d+) \(in the line (\d+), men back (\d+)\), cadres raised (\d+); reliefs (\d+) \(works found by (\d+), months handed over ([\d.]+) of ([\d.]+); no relief near (\d+), no road (\d+), half done (\d+)\); probes (\d+) \(men (\d+), months off ([\d.]+), corps named (\d+)\); line raids (\d+) \(groups (\d+), months off ([\d.]+), refused (\d+)\); softened (\d+) lines, odds ([\d.]+) to ([\d.]+), lifted over the bar (\d+)', t)
        keys = ['folds', 'foldLine', 'foldBack', 'cadres', 'reliefs', 'found', 'kept', 'before', 'noRelief', 'noRoad', 'half', 'probes', 'probeMen', 'probeOff', 'named', 'raids', 'groups', 'raidOff', 'refused', 'soft', 'oddsA', 'oddsB', 'lifted']
        r.update(dict(zip(keys, map(float, m.groups()))) if m else {k: 0.0 for k in keys})
        out[seed] = r
    return out
mon = lambda t: 'never' if t is None else f"{MON[(t + 7) % 12][:3]} {1939 + (t + 7) // 12}"
data = {h: read(h) for h in habits}
for h in habits:
    r = data[h]; n = len(r)
    if not n: print(h, 'no wars'); continue
    v = list(r.values())
    fell = sorted(x['fell'] for x in v)
    print(f"== {h}: {n} wars | Berlin held to end 1944 {sum(x['berlin'] for x in v)} ({100*sum(x['berlin'] for x in v)/n:.0f}%), falls median {mon(int(st.median(fell)))}, stands Dec 1945 {sum(f >= 77 for f in fell)}; provinces at the end median {st.median(x['prov'] for x in v):.0f}")
    sil = [x['sil'] for x in v if x['sil'] is not None]
    if sil: print(f"   longest silence from January 1942: median {st.median(sil):.0f}, six or more in {sum(s >= 6 for s in sil)}, most {max(sil)}; rings on the player {sum(x['rings'] for x in v)/n:.1f} a war ({sum(x['ringMen'] for x in v)/n/1000:.0f}k men), surrenders {sum(x['gaveMen'] for x in v)/n/1000:.0f}k men a war")
    parts = []
    for town in ['Kiev', 'Leningrad', 'Moscow', 'Stalingrad']:
        ts = sorted(x['took'][town] for x in v if town in x['took'])
        parts.append(f"{town} {len(ts)} of {n}" + (f" (median {mon(int(st.median(ts)))}, in 1941 {sum(t <= 28 for t in ts)})" if ts else ''))
    print('   took: ' + '; '.join(parts))
    for y in range(1939, 1946):
        rows = [x['yr'][y] for x in v if y in x['yr']]
        if not rows: continue
        F, W, PW, PF, M, T = (sum(c[i] for c in rows) for i in range(6))
        if F == 0: continue
        print(f"   {y}: attacks a war {F/n:5.1f}, won {100*W/F:3.0f}%; on a prepared line {PF/n:5.1f} a war, won {100*PW/max(1,PF):3.0f}%; men lost a war {M/n/1000:5.0f}k, theirs {T/n/1000:5.0f}k (ratio {T/max(1,M):.2f})")
    s = lambda k: sum(x[k] for x in v)
    if s('folds') or s('reliefs') or s('probes') or s('raids'):
        print(f"   folds {s('folds')/n:.1f} a war (in the line {s('foldLine')/n:.1f}, men back {s('foldBack')/n/1000:.0f}k), cadres raised {s('cadres')/n:.1f}")
        print(f"   reliefs {s('reliefs')/n:.1f} a war; the relieving corps found works in {100*s('found')/max(1,s('reliefs')):.0f}%; months handed over {s('kept'):.0f} of {s('before'):.0f} ({100*s('kept')/max(1,s('before')):.0f}%); no relief near {s('noRelief')/n:.1f}, no road {s('noRoad')/n:.1f}, half done {s('half')/n:.1f}")
        print(f"   probes {s('probes')/n:.1f} a war, men {s('probeMen')/max(1,s('probes')):.0f} a probe ({s('probeMen')/n/1000:.1f}k a war), months off {s('probeOff')/max(1,s('probes')):.2f} a probe, corps named {s('named')/max(1,s('probes')):.1f} a probe")
        print(f"   line raids {s('raids')/n:.1f} a war, groups {s('groups')/max(1,s('raids')):.1f} a raid, months off {s('raidOff')/max(1,s('raids')):.2f} a raid, refused {s('refused')/n:.1f} a war")
        print(f"   softened {s('soft')/n:.1f} lines a war; odds {sum(x['oddsA']*x['soft'] for x in v)/max(1,s('soft')):.2f} before, {sum(x['oddsB']*x['soft'] for x in v)/max(1,s('soft')):.2f} after; lifted over the bar {s('lifted')/n:.1f} a war ({100*s('lifted')/max(1,s('soft')):.0f}%)")
print()
for i, a in enumerate(habits):
    for b in habits[i+1:]:
        common = sorted(set(data[a]) & set(data[b]))
        if not common: continue
        A, B = data[a], data[b]
        both = sum(A[s]['berlin'] and B[s]['berlin'] for s in common); oa = sum(A[s]['berlin'] and not B[s]['berlin'] for s in common); ob = sum(B[s]['berlin'] and not A[s]['berlin'] for s in common)
        diff = st.median(B[s]['fell'] - A[s]['fell'] for s in common)
        pd = st.median(B[s]['prov'] - A[s]['prov'] for s in common)
        print(f"{a} against {b} on {len(common)} seeds: Berlin held by both {both}, only {a} {oa}, only {b} {ob}; {b} lasts a median {diff:+.1f} months, ends with a median {pd:+.0f} provinces")
