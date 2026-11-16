#!/usr/bin/env python3
"""Writes the Version 78 queue (ruling 58a and b: America early, not never; a League of neighbours).
usage: mk78.py   (writes v78a.tsv)"""
import json
def j(d): return "'"+json.dumps(d,separators=(',',':'))+"'"
GERD=[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]]
ITAD=[[10,"FRA"],[10,"UK"],[14,"GRE"]]
HAND={'GER':{"minOdds":1.4,"declare":GERD,"land":True},'ITA':{"minOdds":1.4,"declare":ITAD,"land":True}}
a=[]
# the Playtester's own case first: a small Axis player declares on the United States in September 1940
for me,s,t in (('BRA',25,13),('BRA',26,13),('PER',25,13),('BRA',25,0),('BRA',27,13),('BRA',28,13),('ARG',25,13),('MEX',25,13),('COL',25,13),('BRA',25,2)):
    a.append((f"us-{me}-{s}-T{t}",f"node out78/probe76.mjs {me} {s} 76 {t}"))
# a great Axis power by hand at war with the United States from September 1939 (the page never offers it: forced by the
# rules' own call), and the same war without it
for s in range(25,29):
    for t in (1,0): a.append((f"early-GER-{s}-T{t}",f"node out78/probe79.mjs GER {s} 76 {t} {j({**HAND['GER'],'force':True})}"))
for s in (25,26):
    for t in (1,0): a.append((f"early-ITA-{s}-T{t}",f"node out78/probe79.mjs ITA {s} 76 {t} {j({**HAND['ITA'],'force':True})}"))
# nobody playing: Germany declares in September 1939 (the director's case) beside the same wars without it, seeds 17 to 40
for mode in ('ger39','base'):
    for k in range(8): a.append((f"np-{mode}-{17+3*k}",f"node out78/probe80.mjs {17+3*k},{18+3*k},{19+3*k} 80 {mode}"))
for mode in ('ita39','bra40'):
    for k in range(2): a.append((f"np-{mode}-{17+3*k}",f"node out78/probe80.mjs {17+3*k},{18+3*k},{19+3*k} 80 {mode}"))
# a League asks only its own part of the world: Brazil and Argentina found one, and a year in the seat
for me in ('BRA','ARG'):
    for mode in ('league','axis','allies'): a.append((f"year-{mode}-{me}-25",f"node out78/probe75.mjs {me} 25 76 {mode}"))
open('v78a.tsv','w').write(''.join(f"{n}\t{c}\n" for n,c in a)); print(len(a),'jobs')
