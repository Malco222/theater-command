#!/usr/bin/env python3
"""Version 72: adds up the CROSS lines of probe56 (the sea crossing in the player's hands) beside the DOM lines.
usage: crosstab.py DIR PREFIX... [--seeds A-B] [--rows]
One row for each prefix and seat: wars, won, how many months the way was open at a danger the player bears, how many
months a force stood ready on the quay, both at once (middle of the wars), landings ordered and how they went."""
import re, sys, glob, os, statistics
args = [a for a in sys.argv[1:] if not a.startswith('--')]
lo, hi = 1, 10**9
if '--seeds' in sys.argv:
    a, b = sys.argv[sys.argv.index('--seeds') + 1].split('-'); lo, hi = int(a), int(b)
    args.remove(f'{a}-{b}')
rows = '--rows' in sys.argv
d, prefixes = args[0], args[1:]
NAME = {'GER': 'Germany', 'ITA': 'Italy', 'UK': 'Britain', 'FRA': 'France', 'SOV': 'Soviet Union'}
def mid(xs): return statistics.median(xs) if xs else 0
def num(pat, s, default=0):
    m = re.search(pat, s); return float(m.group(1)) if m else default
for p in prefixes:
    print(f'== {p}')
    print(f"   {'seat':13} wars  won  prepared  open  ready  both  quay changes  wars landing  landings (ashore)  wars with men ashore  months ashore (most)  most corps  rival beaten  lift")
    for seat in ['GER', 'ITA', 'UK', 'FRA', 'SOV']:
        wars = won = prep = 0; openM = []; readyM = []; bothM = []; sw = []; landings = ashoreL = warsLanding = warsAshore = 0; monthsAshore = []; most = []; fell = 0; lift = []; errs = 0
        for f in sorted(glob.glob(os.path.join(d, f'{p}-{seat}-*.txt')), key=lambda x: int(re.findall(r'-(\d+)\.txt$', x)[0])):
            seed = int(re.findall(r'-(\d+)\.txt$', f)[0])
            if not lo <= seed <= hi: continue
            t = open(f).read()
            dom = [l for l in t.splitlines() if l.startswith('DOM ')]
            if not dom: continue
            wars += 1
            if '; WON ' in dom[0]: won += 1
            if 'ERROR' in t: errs += 1
            cr = [l for l in t.splitlines() if l.startswith('CROSS ')]
            if not cr: continue
            c = cr[0]
            if 'crossing prepared' in c: prep += 1
            o, r, b = num(r'bears in (\d+) months', c), num(r'ready on the quay in (\d+)', c), num(r'both in (\d+)', c)
            openM.append(o); readyM.append(r); bothM.append(b); sw.append(num(r'changed quay (\d+) times', c))
            L, A = int(num(r'landings ordered (\d+)', c)), int(num(r'landings ordered \d+, ashore (\d+)', c))
            landings += L; ashoreL += A; warsLanding += 1 if L else 0
            ma = int(num(r'months ashore (\d+)', c)); monthsAshore.append(ma); warsAshore += 1 if ma else 0
            most.append(int(num(r'most corps there (\d+)', c)))
            if re.search(r'; \w+ fell', c): fell += 1
            lift.append(int(num(r'at the most (\d+)', c)))
            if rows: print(f'      seed {seed}: ' + c.split(': ', 1)[1][:330])
        if not wars: continue
        print(f"   {NAME[seat]:13} {wars:4} {won:4} {prep:9} {mid(openM):5g} {mid(readyM):6g} {mid(bothM):5g} {mid(sw):13g} {warsLanding:13} {landings:9} ({ashoreL})        {warsAshore:21} {mid(monthsAshore):13g} ({max(monthsAshore) if monthsAshore else 0})     {max(most) if most else 0:10} {fell:13} {mid(lift):5g}" + (f'   ERRORS in {errs}' if errs else ''))
