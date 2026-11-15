#!/usr/bin/env python3
# Writes v71.tsv: the Version 71 queue, most wanted first (see README, "Version 71").
import json
GER = {"minOdds":1.4,"declare":[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":True}
GERBOOK = {"minOdds":1.4,"declare":[[8,"DEN"],[8,"NOR"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":True}
UK = {"minOdds":1.5,"land":True,"buy":["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]}
SOV = {"minOdds":1.5}
ITA = {"minOdds":1.4,"declare":[[10,"FRA"],[10,"UK"],[14,"GRE"]],"land":True}
FRA = {"minOdds":1.6,"homeOnly":True}
SEAT = {"GER":GER,"UK":UK,"SOV":SOV,"ITA":ITA,"FRA":FRA}
DOM = {"GER":GER,"UK":{"minOdds":1.5,"land":True},"SOV":{"minOdds":1.5,"land":True},"ITA":ITA,"FRA":{"minOdds":1.6,"land":True}}
out = []
def job(name, me, frm, seed, until, plan, stop=0, field=False, prog='probe58'):
    p = dict(plan)
    if field:
        p.pop("buy", None); p["depth"] = "field"
    if stop: p["stop"] = stop
    out.append(f"{name}-{me}-{seed}\tnode out71/{prog}.mjs {me} {frm} {seed} {until} '{json.dumps(p, separators=(',',':'))}'")
B = range(25, 49); S8 = range(1, 9)
# 1. Par at the middle stop on the builder's seeds: the seats a stronger computer Germany touches, then Germany.
for seats in (('SOV', 'UK', 'FRA', 'ITA'), ('GER',)):
    for s in B:
        for me in seats:
            job('seat', me, 1, s, 76, SEAT[me]); job('seatfield', me, 1, s, 76, SEAT[me], 0, True)
# 2. The world domination column by the rule of Version 71, a small set: by hand, at Field command, and the idle player.
for s in S8:
    for me in ('SOV', 'UK', 'GER', 'ITA', 'FRA'):
        job('dom', me, 1, s, 124, DOM[me], prog='probe56'); job('domfield', me, 1, s, 124, DOM[me], 0, True, prog='probe56')
        job('domidle', me, 1, s, 124, {"idle":True}, prog='probe56')
# 3. Par at Close to history (stop 2) on the builder's seeds.
for s in B:
    for me in ('SOV', 'UK', 'FRA', 'ITA', 'GER'):
        job('seat2', me, 1, s, 76, SEAT[me], 2); job('seat2field', me, 1, s, 76, SEAT[me], 2, True)
# 4. Start years 1940 to 1942 at stops 1, 2, 4 and 5: errors, wars that never end.
for frm, y in ((5, 40), (17, 41), (29, 42)):
    for st in (1, 2, 4, 5):
        for me in ('GER', 'UK', 'SOV', 'ITA') + (('FRA',) if frm == 5 else ()):
            plan = GERBOOK if me == 'GER' else SEAT[me]
            for s in (1, 2): job(f'y{y}s{st}', me, frm, s, 88, plan, st)
            job(f'y{y}s{st}field', me, frm, 3, 88, plan, st, True)
# 5. Stops 4 and 5 from every seat, and wars nobody plays to December 1949.
for st in (4, 5):
    for me in ('ITA', 'GER', 'UK', 'SOV', 'FRA'):
        for s in S8: job(f'd{st}', me, 1, s, 76, SEAT[me], st)
        for s in range(1, 5): job(f'd{st}field', me, 1, s, 76, SEAT[me], st, True)
for st in (1, 2, 3, 4, 5):
    for a in (17, 20, 23, 26):
        out.append(f"watch49s{st}-{a}\tnode out71/probe59.mjs {st} {a},{a+1},{a+2} 124")
# 6. Par at the middle stop on my own seeds, the French player who digs in, and the player who never gives an order.
for s in range(1, 25):
    for me in ('SOV', 'UK', 'FRA', 'ITA', 'GER'):
        job('seat', me, 1, s, 76, SEAT[me]); job('seatfield', me, 1, s, 76, SEAT[me], 0, True)
for s in range(1, 13):
    for me in ('SOV', 'UK', 'FRA', 'ITA', 'GER'): job('seatidle', me, 1, s, 76, {"idle":True})
for s in range(1, 25):
    job('seatdig', 'FRA', 1, s, 76, dict(FRA, dig=2)); job('seatfielddig', 'FRA', 1, s, 76, dict(FRA, dig=2), 0, True)
open('v71.tsv', 'w').write('\n'.join(out) + '\n')
print(len(out), 'jobs')
