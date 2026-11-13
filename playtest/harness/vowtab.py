#!/usr/bin/env python3
"""Version 60: promises and heads asked for, read from probe24 outputs.
usage: vowtab.py DIR [PREFIX]   (files DIR/gov-*.txt; PREFIX narrows, for example gov-vow)"""
import sys, re, glob, os, statistics as st
from collections import defaultdict, Counter
D = sys.argv[1]; pre = sys.argv[2] if len(sys.argv) > 2 else 'gov-'
MON = {m: i for i, m in enumerate('January February March April May June July August September October November December'.split())}
def t(s):
    m, y = s.split(); return (int(y) - 1939) * 12 + MON[m]
vows = defaultdict(lambda: Counter()); took = defaultdict(list); heads = []; tot = {}
for f in sorted(glob.glob(os.path.join(D, pre + '*.txt'))):
    name = os.path.basename(f)[:-4]; parts = name.split('-'); kind = parts[1]; habit = parts[4] if len(parts) > 4 else '-'
    key = (kind, habit) if kind == 'vow' else (kind, '')
    made = {}; asked = {}
    for ln in open(f, errors='replace'):
        m = re.match(r'\s+(\w+ \d{4})\s+Your promise: (.+) by the end of (\w+ \d{4})\.', ln)
        if m: made[m.group(2)] = (t(m.group(1)), t(m.group(3))); vows[key]['made'] += 1; continue
        m = re.match(r'\s+(\w+ \d{4})\s+Promise (kept|broken|taken back|released): (.+) by the end of', ln)
        if m:
            vows[key][m.group(2)] += 1
            if m.group(2) == 'kept' and m.group(3) in made:
                d = t(m.group(1)) - made[m.group(3)][0]; took[key].append(d); 
                if d == 0: vows[key]['kept the month made'] += 1
            continue
        m = re.match(r'(\w+ \d{4})\s+HEAD asked: (\S+(?: \S+)*?) ([A-Z][\w\-\.]+(?: [\w\-\.]+)*) \((.+?)\); (.+?) record (\d+) won of (\d+), level (\d+)(, popular)?; .*?his last battles: (.*); the player\'s answer: (\w+)', ln)
        if m:
            fights = [x.strip() for x in m.group(10).split(' | ') if x.strip() and x.strip() != 'none']
            lost = []
            for x in fights:
                mm = re.match(r'(attacked|defended) (.+) at ([\d.]+) to 1 \((\d+) corps on (\d+)\), attacker (won|thrown back)', x)
                if not mm: continue
                beaten = (mm.group(1) == 'attacked') == (mm.group(6) == 'thrown back')
                if beaten: lost.append((mm.group(1), float(mm.group(3)), mm.group(2)))
            # the name is the last words before the bracket; rank comes first
            full = (m.group(2) + ' ' + m.group(3))
            h = dict(file=name, when=m.group(1), full=full, why=m.group(5), won=int(m.group(6)), battles=int(m.group(7)), level=int(m.group(8)), popular=bool(m.group(9)), lost=lost[:2], answer=m.group(11), stale='READ AFTER' in ln, end='?')
            heads.append(h); asked[full] = h; continue
        m = re.match(r'\s+\w+ \d{4}\s+(.+) (vindicated|lost again)\.', ln)
        if m:
            for full, h in asked.items():
                if full.endswith(m.group(1)): h['end'] = m.group(2)
            continue
        m = re.match(r'\s+\w+ \d{4}\s+(?:You stood by (.+)\.|(.+) dismissed, as the cabinet asked\.)', ln)
        if m:
            nm = m.group(1) or m.group(2)
            for full, h in asked.items():
                if full.endswith(nm) and h['end'] == '?': h['end'] = 'stood by, no verdict yet' if m.group(1) else 'dismissed'
            continue
        if ln.startswith('TOTAL'):
            mm = re.search(r'cabinet fell (\d+) times.*?trust mean (\d+), lowest ([\d.\-]+), months under 35: (\d+); months the cabinet doubted the player: (\d+); heads asked (\d+)', ln)
            if mm: tot[name] = tuple(float(x) for x in mm.groups())
print('PROMISES by the player\'s habit')
print(f"{'habit':<14}{'wars':>5}{'made':>6}{'kept':>6}{'same month':>11}{'broken':>8}{'back':>6}{'released':>9}{'kept %':>8}{'months to keep':>16}{'lowest trust':>14}{'fell':>6}")
for key in sorted(vows):
    v = vows[key]; names = [n for n in tot if n.split('-')[1] == key[0] and (key[1] == '' or n.endswith('-' + key[1]))]
    done = v['kept'] + v['broken']
    low = min((tot[n][2] for n in names), default=float('nan')); fell = sum(tot[n][0] for n in names)
    print(f"{(key[0] + ' ' + key[1]).strip():<14}{len(names):>5}{v['made']:>6}{v['kept']:>6}{v['kept the month made']:>11}{v['broken']:>8}{v['taken back']:>6}{v['released']:>9}{(100 * v['kept'] / done if done else 0):>7.0f}%{(st.mean(took[key]) if took[key] else 0):>16.1f}{low:>14.0f}{fell:>6.0f}")
print()
print(f'HEADS asked for: {len(heads)} in {len(tot)} wars ({sum(1 for h in heads if h["stale"])} read after the month)')
good = [h for h in heads if not h['stale']]
def share(xs, f): return f"{sum(1 for x in xs if f(x))} of {len(xs)}"
print('  both defeats were defences:        ', share(good, lambda h: len(h['lost']) == 2 and all(x[0] == 'defended' for x in h['lost'])))
print('  both defeats were attacks:         ', share(good, lambda h: len(h['lost']) == 2 and all(x[0] == 'attacked' for x in h['lost'])))
print('  one of each:                       ', share(good, lambda h: len(h['lost']) == 2 and len({x[0] for x in h['lost']}) == 2))
print('  fewer than two defeats found:      ', share(good, lambda h: len(h['lost']) < 2))
dd = [x[1] for h in good for x in h['lost'] if x[0] == 'defended']; aa = [x[1] for h in good for x in h['lost'] if x[0] == 'attacked']
if dd: print(f'  defences lost: {len(dd)}, odds against him median {st.median(dd):.2f}, at 2 to 1 or worse {sum(1 for x in dd if x >= 2)}, at 3 to 1 or worse {sum(1 for x in dd if x >= 3)}')
if aa: print(f'  attacks thrown back: {len(aa)}, odds median {st.median(aa):.2f}, under 1.5 to 1: {sum(1 for x in aa if x < 1.5)}')
print('  general had won half his battles or more:', share(heads, lambda h: h['battles'] and h['won'] / h['battles'] >= 0.5), '; three in four or more:', share(heads, lambda h: h['battles'] and h['won'] / h['battles'] >= 0.75), '; popular:', share(heads, lambda h: h['popular']))
print('  level: ', dict(sorted(Counter(h['level'] for h in heads).items())))
print('  answers:', dict(Counter(h['answer'] for h in heads)), ' ends:', dict(Counter((h['answer'], h['end']) for h in heads)))
if '-v' in sys.argv:
    for h in heads: print(f"   {h['file']:<26}{h['when']:<16}{h['full'][:44]:<46}{h['won']}/{h['battles']} L{h['level']}{' pop' if h['popular'] else ''}  lost: {h['lost']}  {h['answer']} -> {h['end']}")
