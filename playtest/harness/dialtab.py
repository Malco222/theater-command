#!/usr/bin/env python3
# Adds up the DIAL lines (probe58) by set: when the great attacks fell, where the book was closed, the marches and how
# long after their warnings they came, claims on the player, and whether the war was decided.
# usage: dialtab.py DIR PREFIX... [--rows]     (files DIR/PREFIX-SEAT-SEED.txt; PREFIX may hold a seat: d1book-GER)
import sys, re, glob, os, collections
args = [a for a in sys.argv[1:] if not a.startswith('--')]; rows = '--rows' in sys.argv
d, prefixes = args[0], args[1:]
MON = 'January February March April May June July August September October November December'.split()
def key(m):
    if m == 'never': return (9999, 0)
    a, y = m.split(); return (int(y), MON.index(a))
def spread(c):
    return ', '.join(f"{k} {v}" for k, v in sorted(c.items(), key=lambda kv: key(kv[0])))
for p in prefixes:
    files = sorted(glob.glob(os.path.join(d, p + '-*.txt')), key=lambda f: (re.sub(r'-\d+\.txt$', '', f), int(re.search(r'-(\d+)\.txt$', f).group(1))))
    by = collections.defaultdict(list)
    for f in files:
        m = re.match(r'(.+)-([A-Z]+)-(\d+)\.txt$', os.path.basename(f))
        if not m: continue
        txt = open(f, errors='replace').read()
        dl = [l for l in txt.split('\n') if l.startswith('DIAL ')]
        sl = [l for l in txt.split('\n') if l.startswith('SEAT ')]
        err = [l for l in txt.split('\n') if l.startswith('ERROR') or 'Error:' in l or 'TypeError' in l]
        by[(m.group(1), m.group(2))].append((int(m.group(3)), dl[0] if dl else '', sl[0] if sl else '', err))
    for (set_, seat), wars in sorted(by.items()):
        n = len(wars); bad = [s for s, dl, sl, e in wars if e or not dl]
        print(f"== {set_} {seat}: {n} wars" + (f"; ERRORS or no line on seeds {bad}" if bad else '; no errors'))
        first = {k: collections.Counter() for k in ('west', 'nor', 'ita', 'gre', 'yug', 'east')}
        closed = collections.Counter(); nowhere = 0; decided = collections.Counter(); undec = collections.Counter(); und = 0
        marches = ahead = less = none = 0; wars_marched = 0; onme = collections.Counter(); onme_m = collections.Counter(); unread = 0
        fate = collections.Counter(); par = 0; parn = 0
        for seed, dl, sl, e in wars:
            if not dl: continue
            parts = [x.strip() for x in dl.split(' | ')]
            for k in first:
                mm = re.search(rf'\b{k} ((?:\w+ \d{{4}})|never)', dl)
                if mm: first[k][mm.group(1)] += 1
            mm = re.search(r'war decided (\w+ \d{4})', dl)
            if mm: decided[mm.group(1).split()[1]] += 1
            else:
                und += 1; mm = re.search(r'still at war: ([^)]*)\)', dl)
                for pr in (mm.group(1).split() if mm else []): undec[pr] += 1
            mm = re.search(r'book closed: (.*?) \| marches', dl)
            if mm:
                if mm.group(1) == 'nowhere': nowhere += 1
                else:
                    for c in mm.group(1).split('; '): closed[c] += 1
            mm = re.search(r'marches (\d+): warned two months or more ahead (\d+), less (\d+), not at all (\d+)', dl)
            if mm:
                a, b, c, dd = map(int, mm.groups()); marches += a; ahead += b; less += c; none += dd; wars_marched += a > 0
            mm = re.search(r'claims on the player: (.*?) \| flags', dl)
            if mm and mm.group(1) != 'none':
                for c in mm.group(1).split('; '):
                    nm = c.split(':')[0]; onme[nm] += 1
                    if 'marched never' not in c: onme_m[nm] += 1
                    if 'readable never' in c: unread += 1
            if sl:
                parn += 1; par += 'par BEATEN' in sl
                mm = re.search(r'; (stands|fell \w+ \d{4});', sl)
                if mm: fate['stands' if mm.group(1) == 'stands' else 'fell ' + mm.group(1).split()[2]] += 1
            if rows: print('   ', dl[:400])
        for k, nm in (('west', 'the west'), ('nor', 'Norway'), ('ita', "Italy's war"), ('gre', 'Greece'), ('yug', 'Yugoslavia'), ('east', 'the east')):
            print(f"   {nm:12} {spread(first[k])}")
        print(f"   book closed nowhere in {nowhere}" + (''.join(f"; {c} in {v}" for c, v in closed.most_common()) if closed else ''))
        print(f"   marches {marches} in {wars_marched} wars: warned two months or more ahead {ahead}, less {less}, not at all {none}")
        if onme: print("   claims on the player: " + ', '.join(f"{k} in {v} (marched for in {onme_m[k]})" for k, v in onme.most_common()) + (f"; NOT readable before pressed: {unread}" if unread else '; every one readable before it was pressed'))
        print(f"   war decided in {n - und - len(bad)}: " + (', '.join(f"{k} {v}" for k, v in sorted(decided.items())) or '-') + (f"; not decided in {und}: " + ', '.join(f"{k} {v}" for k, v in undec.most_common()) if und else ''))
        print(f"   the seat: par beaten in {par} of {parn}; " + ', '.join(f"{k} {v}" for k, v in sorted(fate.items())))
