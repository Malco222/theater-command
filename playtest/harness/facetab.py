"""Version 84: the attack card beside its result, by seat, from the FACE lines of probe94 (files DIR/face-NAT-SEED.txt).
usage: python3 facetab.py DIR"""
import re, sys, glob, collections
by = collections.defaultdict(lambda: collections.Counter())
years = collections.defaultdict(lambda: collections.Counter())
for f in sorted(glob.glob(sys.argv[1] + '/face-*.txt')):
    for l in open(f):
        if not l.startswith('FACE'): continue
        nat = l.split()[1]; c = by[nat]; c['wars'] += 1
        g = lambda rx: int(re.search(rx, l).group(1)) if re.search(rx, l) else 0
        c['attacks'] += g(r'attacks (\d+)'); c['block'] += g(r'cards with the block (\d+)'); c['unscouted'] += g(r'not scouted (\d+)')
        c['tanks'] += g(r'tanks in (\d+)'); c['alone'] += g(r'sent alone (\d+)'); c['aloneCut'] += g(r'cut a quarter or more (\d+)')
        m = re.search(r'won alone (\d+) of (\d+), won with infantry (\d+) of (\d+)', l)
        if m: c['wonAlone'] += int(m.group(1)); c['nAlone'] += int(m.group(2)); c['wonWith'] += int(m.group(3)); c['nWith'] += int(m.group(4))
        c['named'] += g(r'a cause named (\d+)'); c['even'] += g(r'even (\d+)'); c['noline'] += g(r'no line (\d+)')
        c['TOLD'] += g(r'TOLD (\d+)'); c['UNKNOWN'] += g(r'UNKNOWN (\d+)'); c['SURPRISE'] += g(r'SURPRISE (\d+)')
        m = re.search(r'shock words on both (\d+), the same (\d+)', l)
        if m: c['shock'] += int(m.group(1)); c['shockSame'] += int(m.group(2))
        m = re.search(r'by year ([^|]*)\|', l)
        if m:
            for y, a, b in re.findall(r'(\d{4}): (\d+) of (\d+)', m.group(1)): years[y]['un'] += int(a); years[y]['n'] += int(b)
print('seat wars attacks  block  not-scouted   tanks alone (cut 1/4+) won-alone won-with | cause named: told unknown SURPRISE | shock same')
tot = collections.Counter()
for nat, c in by.items():
    tot.update(c)
    print(f"{nat:4} {c['wars']:4} {c['attacks']:7} {c['block']:6} {c['unscouted']:5} ({100*c['unscouted']//max(1,c['attacks'])}%) {c['tanks']:7} {c['alone']:5} ({c['aloneCut']}) {c['wonAlone']}/{c['nAlone']} {c['wonWith']}/{c['nWith']} | {c['named']}: {c['TOLD']} {c['UNKNOWN']} {c['SURPRISE']} | {c['shockSame']}/{c['shock']}")
c = tot
print(f"ALL  {c['wars']:4} {c['attacks']:7} {c['block']:6} {c['unscouted']:5} ({100*c['unscouted']//max(1,c['attacks'])}%) {c['tanks']:7} {c['alone']:5} ({c['aloneCut']}) {c['wonAlone']}/{c['nAlone']} {c['wonWith']}/{c['nWith']} | {c['named']}: {c['TOLD']} {c['UNKNOWN']} {c['SURPRISE']} | {c['shockSame']}/{c['shock']}")
print('not scouted by year: ' + ', '.join(f"{y}: {v['un']} of {v['n']} ({100*v['un']//max(1,v['n'])}%)" for y, v in sorted(years.items())))
