# Version 66: adds up the DEPTH and LINES lines of probe28 for each set of wars: Berlin, how the war ended, the
# treasury and the men each January, how long the Staff tab said there were no men to spare, and the staff's lines.
# usage: depthtab.py DIR NAME...     (files DIR/NAME-GER-SEED.txt)
import re, sys, glob, statistics as st
d = sys.argv[1]
med = lambda xs: (st.median(xs) if xs else float('nan'))
MON = 'January February March April May June July August September October November December'.split()
turn = lambda m, y: (int(y) - 1939) * 12 + MON.index(m) - 8 + 1
for name in sys.argv[2:]:
    wars = held = standing = 0; fell = []; peak = []; nomen = []; first = []; spent = []; jan = {}; men = {}
    lines = []; same = []; desk = {}; top = {}
    for f in sorted(glob.glob(f'{d}/{name}-GER-*.txt')):
        t = open(f).read().split('\n')
        tot = [l for l in t if l.startswith('TOTAL')]
        if not tot: continue
        wars += 1; T = tot[-1]
        held += 'Berlin in December 1944: GER' in T
        m = re.search(r'\(fell (\d+)\)', T)
        if m: fell.append(int(m.group(1)))
        else: standing += 1
        for l in t:
            if l.startswith('DEPTH'):
                m = re.search(r'treasury each January ([^;]*);', l)
                for y, v in re.findall(r'(\d{4}) (-?\d+)', m.group(1)): jan.setdefault(y, []).append(int(v))
                m = re.search(r'men in the pool each January ([^;]*);', l)
                for y, v in re.findall(r'(\d{4}) (-?\d+)%', m.group(1)): men.setdefault(y, []).append(int(v))
                m = re.search(r'peak treasury (-?\d+)', l); peak.append(int(m.group(1)))
                m = re.search(r'the staff spent (-?\d+) on building', l); spent.append(int(m.group(1)))
                m = re.search(r'no men to spare (\d+)(?:, first (\w+) (\d{4}))?', l); nomen.append(int(m.group(1)))
                if m.group(2): first.append(turn(m.group(2), m.group(3)))
            if l.startswith('LINES'):
                m = re.search(r'staff lines (\d+) \(([^)]*)\)', l); lines.append(int(m.group(1)))
                for k, v in re.findall(r'(\w+) (\d+)', m.group(2)): desk.setdefault(k, []).append(int(v))
                m = re.search(r'the month before (\d+)', l); same.append(int(m.group(1)))
                for n, w in re.findall(r'(\d+) times "([^"]*)"', l): top[w] = top.get(w, 0) + int(n)
    if not wars: print(f'== {name}: no wars'); continue
    date = lambda t: f'{MON[(int(t) + 7) % 12]} {1939 + (int(t) + 7) // 12}'
    print(f'== {name}: {wars} wars; Berlin held to the end of 1944 in {held} ({100*held/wars:.0f}%); Germany still standing at the end of 1945 in {standing}; of those that fell, the middle one fell {date(med(fell)) if fell else "-"}')
    if peak: print('   treasury each January (middle war): ' + ', '.join(f'{y} {med(v):.0f}' for y, v in sorted(jan.items()) if len(v) >= wars / 2))
    if peak: print('   men left in the pool each January (middle war): ' + ', '.join(f'{y} {med(v):.0f}%' for y, v in sorted(men.items()) if len(v) >= wars / 2))
    if not peak: continue
    print(f'   highest treasury: middle war {med(peak):.0f}, lowest {min(peak)}, highest {max(peak)}; wars that passed 50,000: {sum(p >= 50000 for p in peak)}, 100,000: {sum(p >= 100000 for p in peak)}')
    if any(spent): print(f'   the staff spent on building, middle war: {med(spent):.0f}; months the Staff tab said there were no men to spare: middle war {med(nomen):.0f}, wars with any {sum(n > 0 for n in nomen)}; first said in {date(med(first)) if first else "-"} (middle war), earliest {date(min(first)) if first else "-"}')
    if any(lines):
        print(f'   staff lines a war: middle {med(lines):.0f}; by desk ' + ', '.join(f'{k} {med(v):.0f}' for k, v in desk.items()) + f'; word for word the same as that desk\'s line the month before: {sum(same)} of {sum(lines)} ({100*sum(same)/max(1,sum(lines)):.0f}%)')
        print('   most repeated across the set: ' + ' | '.join(f'{n} "{w}"' for w, n in sorted(top.items(), key=lambda x: -x[1])[:5]))
