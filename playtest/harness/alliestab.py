#!/usr/bin/env python3
"""alliestab.py DIR [PREFIX=allies]: one row a war from probe87's output: when Germany fell, whether the Soviet Union
was ever at war with Italy, how Italy ended, and where Britain's, America's and the Soviet Union's field corps stood
the month after Germany fell and twelve months later (total / at home / on Italy's home ground / beside it)."""
import sys, re, glob, os
d = sys.argv[1]; pre = sys.argv[2] if len(sys.argv) > 2 else 'allies'
def cut(line):
    m = re.search(r'(\d+) field corps: (.*)$', line)
    if not m: return (0, 0, 0, 0)
    tot = int(m.group(1)); home = on = by = 0
    for part in m.group(2).split(', '):
        mm = re.match(r'(.*) (\d+)$', part)
        if not mm: continue
        k, v = mm.group(1), int(mm.group(2))
        if k == 'at home': home += v
        elif k.startswith("ON ITALY"): on += v
        elif k.startswith('BESIDE ITALY'): by += v
    return (tot, home, on, by)
rows = []; n = sovnever = stands = 0; noContact = 0; vichy = 0; later_on = []
print(f"{'seed':>4} {'Germany fell':<15} {'Soviet war on Italy':<20} {'Italy':<22} | month after: UK tot/home/on/by  USA  SOV tot/by | Vichy | Europe contact | a year on: UK+USA on Italy, home held")
for f in sorted(glob.glob(os.path.join(d, pre + '-*.txt')), key=lambda p: int(re.findall(r'-(\d+)\.txt', p)[0])):
    L = open(f).read().split('\n')
    if not L or not L[0].startswith('ALLIES'): continue
    seed = re.search(r'seed (\d+)', L[0]).group(1)
    gf = re.search(r'Germany (fell [A-Za-z]+ \d+|stands)', L[0]).group(1)
    sov = re.search(r'SOV (never|[A-Za-z]+ \d+ to [A-Za-z]+ \d+)', L[0]).group(1)
    it = re.search(r'; Italy (stands|out [A-Za-z]+ \d+ \(\w+\))', L[0]).group(1)
    blocks = {}; cur = None
    for l in L[1:]:
        if l.startswith('STAND '): cur = re.match(r'STAND ([^,]+),', l).group(1); blocks[cur] = [l]
        elif cur: blocks[cur].append(l)
    def get(b, key):
        for l in blocks.get(b, []):
            if l.strip().startswith(key): return l
        return ''
    b1 = 'the month after Germany falls'; b2 = 'twelve months later'
    n += 1; sovnever += sov == 'never'; stands += it == 'stands'
    if b1 not in blocks:
        print(f"{seed:>4} {gf:<15} {sov:<20} {it:<22} | Germany did not fall before Italy or the end"); continue
    uk = cut(get(b1, 'Britain:')); us = cut(get(b1, 'America:')); so = cut(get(b1, 'Soviet Union:'))
    v = get(b1, 'Vichy:'); vm = re.search(r'Vichy: (stands|is out).*?holds (\d+) provinces', v); vs = f"{vm.group(1)} {vm.group(2)}" if vm else '-'
    c = get(b1, 'where an enemy'); places = [] if 'NOWHERE' in c else c.split(': ', 1)[1].split('; ')
    home = get(b1, "the ground beside Italy's home"); homeNames = set(re.findall(r'([A-Z][A-Za-z\' ]+) \(', home))
    lostNames = set(re.findall(r'([A-Z][A-Za-z\' ]+) \(', get(b1, "Italy's home provinces lost")))
    eur = [p for p in places if p.split(' (')[0] in homeNames or p.split(' (')[0] in lostNames]
    if b2 in blocks:
        uk2 = cut(get(b2, 'Britain:')); us2 = cut(get(b2, 'America:')); ot2 = cut(get(b2, 'other Allies:'))
        hm = re.search(r'(\d+) of (\d+) home provinces', blocks[b2][0]); y = f"{uk2[2] + us2[2] + ot2[2]} corps, {hm.group(1)} of {hm.group(2)}" if hm else 'Italy out'
    else: y = '(not reached)'
    print(f"{seed:>4} {gf:<15} {sov:<20} {it:<22} | {uk[0]:>3}/{uk[1]:>2}/{uk[2]}/{uk[3]}  {us[0]:>3}/{us[1]:>2}/{us[2]}/{us[3]}  {so[0]:>3}/{so[3]:>2} | {vs:<9} | {len(eur)} at Italy's home, {len(places)} in all | {y}")
print(f"\n{n} wars: the Soviet Union never at war with Italy in {sovnever}; Italy stands at the end in {stands}")
