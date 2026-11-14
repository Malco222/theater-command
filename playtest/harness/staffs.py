#!/usr/bin/env python3
# staffs.py DIR HABIT... : Version 65, what the enemy staffs did to the player, from probe28's STAFFS, EFFORT, BATTLES and TOTAL lines
# (files DIR/HABIT-GER-SEED.txt). For each habit: main efforts where the player stood (a war, by year), how many came to blows and
# took ground, the battles in them (lost by the player, card odds, men), how much word the player had, the enemy's probes and raids on
# his works, attacks on the player's prepared line thrown back, and Berlin and the longest silence split by the Soviet staff's character.
import re, sys, glob, statistics, collections
d = sys.argv[1]
def num(p, s, cast=float, default=0):
    m = re.search(p, s); return cast(m.group(1)) if m else default
for h in sys.argv[2:]:
    S = []; E = []; B = collections.defaultdict(lambda: [0, 0, 0.0, 0.0, 0.0]); chars = collections.defaultdict(list)
    for f in sorted(glob.glob(f"{d}/{h}-GER-*.txt"), key=lambda s: int(re.search(r"-(\d+)\.txt$", s).group(1))):
        seed = int(re.search(r"-(\d+)\.txt$", f).group(1))
        L = open(f).read().split('\n')
        st = [l for l in L if l.startswith('STAFFS')]; tot = [l for l in L if l.startswith('TOTAL')]
        if not tot: continue
        s, t = (st[-1] if st else ''), tot[-1]
        held = 'Berlin in December 1944: GER' in t
        sil = num(r"longest silence from January 1942: (\d+)", t, int)
        sov = (re.search(r"characters .*?SOV (\w+)", s) or [None, '?'])[1]
        chars[sov].append((held, sil))
        S.append(dict(seed=seed, held=held, sil=sil, sov=sov,
            eff=num(r"main efforts where the player stood (\d+)", s, int), ally=num(r"at allies only (\d+)", s, int),
            effM=num(r"months under one (\d+)", s, int), genM=num(r"months under a general offensive (\d+)", s, int),
            blows=num(r"efforts that came to blows (\d+)", s, int), took=num(r"that took ground (\d+)", s, int), prov=num(r"provinces taken (\d+)", s, int),
            bat=num(r"battles in them (\d+)", s, int), lost=num(r"\(the player lost (\d+)", s, int), prep=num(r"on a prepared line (\d+)\)", s, int),
            aMen=num(r"men lost by the attackers (\d+)", s, int), dMen=num(r"by the player (\d+); word", s, int),
            ahead=num(r"word a month or more before the first blow (\d+)", s, int), same=num(r"in the same month (\d+)", s, int), none=num(r"none before it (\d+)", s, int),
            probes=num(r"enemy probes of the player's line (\d+)", s, int), pwear=num(r"\(months off ([\d.]+)\), raids", s), raids=num(r"raids on his works (\d+)", s, int),
            named=num(r"effort named (\d+)", s, int), overTook=num(r"over and took ground (\d+)", s, int), overHeld=num(r"over and the line held (\d+)", s, int),
            years=dict((int(y), int(n)) for y, n in re.findall(r"(\d{4}) (\d+)", (re.search(r"\(by year (.*?)\)", s) or [None, ''])[1]))))
        for l in L:
            if l.startswith('EFFORT'):
                E.append(dict(seed=seed, nat=l.split()[1], bat=num(r"battles (\d+)", l, int), lost=num(r"the player lost (\d+)", l, int), odds=num(r"card odds ([\d.]+)", l), won=num(r"provinces taken (\d+)", l, int), yr=num(r"named \w+ (\d{4})", l, int)))
            m = re.match(r"BATTLES (.*?)\s+(\d+) fought \| attacker won\s+(\d+) .*?mean odds ([\d.]+) .*?attacker ([\d.]+), defender ([\d.]+)", l)
            if m and m.group(1).strip().endswith('all odds'):
                k = m.group(1).strip(); n = int(m.group(2)); b = B[k]; b[0] += n; b[1] += int(m.group(3)); b[2] += float(m.group(4)) * n; b[3] += float(m.group(5)) * n; b[4] += float(m.group(6)) * n
    if not S: print(h, 'no STAFFS lines'); continue
    n = len(S); tot = lambda k: sum(x[k] for x in S)
    yrs = collections.Counter()
    for x in S: yrs.update(x['years'])
    print(f"== {h}: {n} wars; Berlin held {sum(x['held'] for x in S)} ({100*sum(x['held'] for x in S)/n:.0f}%); longest silence median {statistics.median([x['sil'] for x in S])}, six or more in {sum(x['sil']>=6 for x in S)}, twelve or more in {sum(x['sil']>=12 for x in S)}")
    print(f"   main efforts where the player stood: {tot('eff')} ({tot('eff')/n:.1f} a war; wars with none {sum(x['eff']==0 for x in S)}); by year {dict(sorted(yrs.items()))}; at allies only {tot('ally')}")
    print(f"   months under a main effort {tot('effM')/n:.1f} a war, under a general offensive {tot('genM')/n:.1f}")
    print(f"   came to blows {tot('blows')} of {tot('eff')}; took ground {tot('took')} ({tot('prov')} provinces); battles in them {tot('bat')}, the player lost {tot('lost')} ({100*tot('lost')/max(1,tot('bat')):.0f}%), on a prepared line {tot('prep')}; men lost: attackers {tot('aMen')/max(1,tot('bat')):.0f} a battle, the player {tot('dMen')/max(1,tot('bat')):.0f}")
    print(f"   word before the first blow: a month or more ahead {tot('ahead')}, the same month {tot('same')}, none {tot('none')}; headlines: effort named {tot('named')}, over and took ground {tot('overTook')}, over and the line held {tot('overHeld')}")
    print(f"   enemy probes of the player's line {tot('probes')} ({tot('probes')/n:.1f} a war, {tot('pwear'):.0f} months of works off), raids on his works {tot('raids')} ({tot('raids')/n:.1f} a war)")
    eo = [e['odds'] for e in E if e['bat']]
    if eo: print(f"   efforts that fought: {len(eo)}; card odds of their battles: median {statistics.median(eo):.2f}, lowest {min(eo):.2f}; by nation {dict(collections.Counter(e['nat'] for e in E))}")
    for k in sorted(B):
        b = B[k]
        if b[0]: print(f"   {k:58s} {b[0]:5d} fought, attacker won {b[1]:5d} ({100*b[1]/b[0]:.1f}%), thrown back {b[0]-b[1]:4d} ({100*(b[0]-b[1])/b[0]:.1f}%), mean odds {b[2]/b[0]:.2f}, men lost a battle: attacker {b[3]/b[0]:.0f}, defender {b[4]/b[0]:.0f}")
    for c, v in sorted(chars.items()):
        print(f"   Soviet staff {c:12s} {len(v):2d} wars: Berlin held {sum(a for a, _ in v)} ({100*sum(a for a, _ in v)/len(v):.0f}%), longest silence median {statistics.median([b for _, b in v])}, six or more in {sum(b>=6 for _, b in v)}")
