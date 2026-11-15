#!/usr/bin/env python3
# Writes v70x.tsv: the Version 70 history dial sets (see README, "Version 70 and the history dial").
import json
GER = {"minOdds":1.4,"declare":[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":True}
GERBOOK = {"minOdds":1.4,"declare":[[8,"DEN"],[8,"NOR"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":True}
GERSTILL = {"minOdds":1.4,"land":True}
UK = {"minOdds":1.5,"land":True,"buy":["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]}
UKF = {"minOdds":1.5,"land":True}
SOV = {"minOdds":1.5}
ITA = {"minOdds":1.4,"declare":[[10,"FRA"],[10,"UK"],[14,"GRE"]],"land":True}
ITAOUT = {"minOdds":1.4,"land":True}
FRA = {"minOdds":1.6,"homeOnly":True}
SEAT = {"GER":GER,"UK":UK,"SOV":SOV,"ITA":ITA,"FRA":FRA}
out = []
def job(name, me, frm, seed, until, plan, stop, field=False, prog='probe58'):
    p = dict(plan)
    if field:
        p.pop("buy", None); p["depth"] = "field"
    if stop: p["stop"] = stop
    out.append(f"{name}-{me}-{seed}\tnode out70/{prog}.mjs {me} {frm} {seed} {until} '{json.dumps(p, separators=(',',':'))}'")
S8 = range(1, 9)
# A. Stop 1. Germany by the book, Germany by my usual plan (no Norway), a Germany that never strikes in the west, an idle Germany; then the other seats.
for s in S8:
    job('d1book', 'GER', 1, s, 76, GERBOOK, 1); job('d1bookfield', 'GER', 1, s, 76, GERBOOK, 1, True)
    job('d1mine', 'GER', 1, s, 76, GER, 1)
    job('d1still', 'GER', 1, s, 76, GERSTILL, 1); job('d1stillfield', 'GER', 1, s, 76, GERSTILL, 1, True)
    job('d1idle', 'GER', 1, s, 76, {"idle":True}, 1)
for s in S8:
    for me in ('SOV', 'UK', 'FRA', 'ITA'):
        job('d1', me, 1, s, 76, SEAT[me], 1); job('d1field', me, 1, s, 76, SEAT[me], 1, True)
for s in range(1, 5): job('d1out', 'ITA', 1, s, 76, ITAOUT, 1)
# B. Stops 4 and 5 as Italy.
for s in S8:
    for st in (4, 5):
        job(f'd{st}', 'ITA', 1, s, 76, ITA, st); job(f'd{st}field', 'ITA', 1, s, 76, ITA, st, True)
# C. Par at stop 2, Germany and the Soviet Union, on the builder's seeds.
for s in range(25, 49):
    for me in ('GER', 'SOV'):
        job('seat2', me, 1, s, 76, SEAT[me], 2); job('seat2field', me, 1, s, 76, SEAT[me], 2, True)
# D. Start years 1940 to 1942 at stops 1, 2, 4 and 5.
for frm, y in ((5, 40), (17, 41), (29, 42)):
    for st in (1, 2, 4, 5):
        for me in ('GER', 'UK', 'SOV', 'ITA') + (('FRA',) if frm == 5 else ()):
            plan = GERBOOK if me == 'GER' else SEAT[me]
            for s in (1, 2): job(f'y{y}s{st}', me, frm, s, 88, plan, st)
            job(f'y{y}s{st}field', me, frm, 3, 88, plan, st, True)
# E. Stops 4 and 5 from the other seats.
for st in (4, 5):
    for me in ('GER', 'UK', 'SOV', 'FRA'):
        for s in S8: job(f'd{st}', me, 1, s, 76, SEAT[me], st)
        for s in range(1, 5): job(f'd{st}field', me, 1, s, 76, SEAT[me], st, True)
# F. Nobody at the controls, to December 1949, on seeds the builder has not played.
for st in (1, 2, 4, 5):
    for a in (17, 20, 23, 26):
        out.append(f"watch49s{st}-{a}\tnode out70/probe59.mjs {st} {a},{a+1},{a+2} 124")
# G. Par at stop 2 from the other three seats.
for s in range(25, 49):
    for me in ('UK', 'FRA', 'ITA'):
        job('seat2', me, 1, s, 76, SEAT[me], 2); job('seat2field', me, 1, s, 76, SEAT[me], 2, True)
open('v70x.tsv', 'w').write('\n'.join(out) + '\n')
print(len(out), 'jobs')
