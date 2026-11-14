# Version 66: my wars against the rules builder's on the same seeds. For each seed both played, when Germany fell
# and whether Berlin was held to the end of 1944.   usage: bycmp.py MY-DIR MY-NAME THEIR-SET [FROM TO]
import re, sys, glob
mydir, myname, theirs = sys.argv[1:4]
lo, hi = (int(sys.argv[4]), int(sys.argv[5])) if len(sys.argv) > 5 else (25, 72)
MON = 'January February March April May June July August September October November December'.split()
turn = lambda m, y: (int(y) - 1939) * 12 + MON.index(m) - 8 + 1
their = {}
for f in glob.glob(f'/mnt/project-files/theater-command/results/v66/{theirs}-*.txt'):
    for l in open(f):
        m = re.match(r'GER seed (\d+): (FELL (\w+) (\d{4})|standing)', l)
        if m: their[int(m.group(1))] = turn(m.group(3), m.group(4)) if m.group(3) else None
mine = {}
for s in range(lo, hi + 1):
    try: t = [l for l in open(f'{mydir}/{myname}-GER-{s}.txt') if l.startswith('TOTAL')][-1]
    except Exception: continue
    m = re.search(r'\(fell (\d+)\)', t)
    mine[s] = int(m.group(1)) if m else None
both = sorted(set(mine) & set(their))
held = lambda t: t is None or t > 64
a = sum(held(mine[s]) for s in both); b = sum(held(their[s]) for s in both)
print(f'{myname} against {theirs}: seeds both played {len(both)} ({both[0] if both else "-"} to {both[-1] if both else "-"}); Berlin held to the end of 1944: mine {a}, the builder\'s {b}; both {sum(held(mine[s]) and held(their[s]) for s in both)}, mine only {sum(held(mine[s]) and not held(their[s]) for s in both)}, the builder\'s only {sum(held(their[s]) and not held(mine[s]) for s in both)}, neither {sum(not held(mine[s]) and not held(their[s]) for s in both)}')
print(f'   Germany still standing at the end of 1945: mine {sum(mine[s] is None for s in both)}, the builder\'s {sum(their[s] is None for s in both)}')
d = [((mine[s] or 77) - (their[s] or 77)) for s in both]
if d: print(f'   months my Germany lasts longer than the builder\'s, by seed: {" ".join(f"{s}:{x:+d}" for s, x in zip(both, d))}; mean {sum(d)/len(d):+.1f}')
