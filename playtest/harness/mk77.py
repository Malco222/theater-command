#!/usr/bin/env python3
"""Writes the Version 77 queues (the staffs cross where history planned it), each short enough for the two-hour limit.
usage: mk77.py   (writes v77a.tsv to v77e.tsv)"""
import json
B56='node out77/probe56.mjs'; B58='node out77/probe58.mjs'; B59='node out77/probe59.mjs'
L74='node out77/probe74.mjs'; L75='node out77/probe75.mjs'; B58b='node out77/probe58.mjs'
def j(d): return "'"+json.dumps(d,separators=(',',':'))+"'"
BUY=["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]
GERD=[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]]
ITAD=[[10,"FRA"],[10,"UK"],[14,"GRE"]]
HAND={'GER':{"minOdds":1.4,"declare":GERD,"land":True},'ITA':{"minOdds":1.4,"declare":ITAD,"land":True},
      'UK':{"minOdds":1.5,"land":True,"buy":BUY},'FRA':{"minOdds":1.6,"homeOnly":True},'SOV':{"minOdds":1.5},
      'USA':{"minOdds":1.5,"land":True,"stage":True,"buy":BUY},'CAN':{"minOdds":1.5,"land":True,"stage":True,"buy":BUY}}
FIELD={k:{**{x:v[x] for x in v if x not in('buy','stage')},"depth":"field"} for k,v in HAND.items()}
IDLE={'idle':True}
LATIN=('BRA','ARG','CHL','PER','COL','VEN')
def write(name, rows):
    open(name,'w').write(''.join(f"{a}\t{b}\n" for a,b in rows)); print(name, len(rows))
# W. First: the computer's American army (the two repeats), the war with nobody playing, then Britain's and Canada's
# first landings (ruling 43), three wars a file.
w=[]
for s in range(17,41,3): w.append((f"army-USA-{s}",f"node out77/probe68.mjs {s},{s+1},{s+2} 80"))
for s in range(17,41,3): w.append((f"watch49s3-{s}",f"{B59} 3 {s},{s+1},{s+2} 124"))
for s in range(17,41,3): w.append((f"army-UK-{s}",f"node out77/probe68.mjs {s},{s+1},{s+2} 80 UK"))
for s in range(17,41,3): w.append((f"army-CAN-{s}",f"node out77/probe68.mjs {s},{s+1},{s+2} 80 CAN"))
write('v77w.tsv', w)
# A. The first number, the builder's hemisphere set on my seeds, the American and British seats.
a=[]
for s in (18,22,26,30,34,38): a.append((f"hemi-{s}",f"node out77/hemi76.mjs {s} 76"))
for s in range(25,37):
    a.append((f"seatstage-USA-{s}",f"{B58} USA 1 {s} 76 {j(HAND['USA'])}"))
    a.append((f"seatfield-USA-{s}",f"{B58} USA 1 {s} 76 {j(FIELD['USA'])}"))
for s in range(25,49):
    a.append((f"seat-UK-{s}",f"{B58} UK 1 {s} 76 {j(HAND['UK'])}"))
    a.append((f"seatfield-UK-{s}",f"{B58} UK 1 {s} 76 {j(FIELD['UK'])}"))
for s in range(37,49):
    a.append((f"seatstage-USA-{s}",f"{B58} USA 1 {s} 76 {j(HAND['USA'])}"))
    a.append((f"seatfield-USA-{s}",f"{B58} USA 1 {s} 76 {j(FIELD['USA'])}"))
UKH=j(HAND['UK'])
for s in range(25,37): a.append((f"armyuk-USA-{s}",f"node out77/probe69.mjs UK 1 {s} 76 {UKH} USA"))
for s in range(25,37): a.append((f"armyukidle-USA-{s}",f"node out77/probe69.mjs UK 1 {s} 76 {j(IDLE)} USA"))
write('v77a.tsv', a)
# B. Par from the other seats, seeds 25 to 48, by hand and at Field command.
b=[]
for s in range(25,49):
    for me in ('GER','SOV','FRA','ITA'):
        b.append((f"seat-{me}-{s}",f"{B58} {me} 1 {s} 76 {j(HAND[me])}"))
        b.append((f"seatfield-{me}-{s}",f"{B58} {me} 1 {s} 76 {j(FIELD[me])}"))
write('v77b.tsv', b)
# C. World domination and every seat's landings followed (seeds 1 to 8, to December 1949).
CROSS={"cross":True,"landRisk":0.4}
DOM={'GER':{**HAND['GER'],**CROSS,"crossFoe":"UK","buy":["ship:tp","air:ftr","corps:inf","corps:arm","work:factory"]},
     'UK':{"minOdds":1.5,"land":True,**CROSS,"buy":["corps:arm","corps:inf","ship:tp","air:ftr","work:factory"]},
     'ITA':{**HAND['ITA'],**CROSS,"crossFoe":"UK","buy":["ship:tp","air:ftr","corps:inf"]},
     'FRA':{"minOdds":1.6,"land":True,**CROSS,"buy":["corps:arm","corps:inf","ship:tp","air:ftr","work:factory"]},
     'USA':{"minOdds":1.5,"land":True,"stage":True,**CROSS,"buy":["corps:arm","corps:inf","ship:tp","air:ftr","work:factory"]}}
DOMF={'GER':{**HAND['GER'],**CROSS,"crossFoe":"UK","depth":"field"},'SOV':{"minOdds":1.5,"land":True,**CROSS,"crossFoe":"UK","depth":"field"},
      'UK':{"minOdds":1.5,"land":True,**CROSS,"depth":"field"},'ITA':{**HAND['ITA'],**CROSS,"crossFoe":"UK","depth":"field"},
      'FRA':{"minOdds":1.6,"land":True,**CROSS,"depth":"field"},'USA':{"minOdds":1.5,"land":True,**CROSS,"depth":"field"}}
SOVFE={"minOdds":1.5,"land":True,**CROSS,"crossFoe":"UK","buy":["corps:arm","corps:inf","ship:tp","air:ftr","work:factory"],"crossFirst":True,"engs":6}
c=[]
for s in range(1,9):
    for me in ('GER','UK','ITA','FRA','USA'): c.append((f"dom-{me}-{s}",f"{B56} {me} 1 {s} 124 {j(DOM[me])}"))
    c.append((f"domfirsteng-SOV-{s}",f"{B56} SOV 1 {s} 124 {j(SOVFE)}"))
    for me in ('GER','SOV','UK','ITA','FRA','USA'): c.append((f"domfield-{me}-{s}",f"{B56} {me} 1 {s} 124 {j(DOMF[me])}"))
write('v77c.tsv', c)
# D. The idle player from every seat; Canada and Mexico.
d=[]
for s in range(1,13):
    for me in ('GER','UK','SOV','FRA','ITA','USA'): d.append((f"seatidle-{me}-{s}",f"{B58} {me} 1 {s} 76 {j(IDLE)}"))
for s in range(25,37):
    d.append((f"seatstage-CAN-{s}",f"{B58} CAN 1 {s} 76 {j(HAND['CAN'])}"))
write('v77d.tsv', d)
# L. The Latin seats again, small: a year in Brazil's and Argentina's seat on each path, seed 25.
l=[]
for me in ('BRA','ARG'):
    for mode in ('look','allies','axis','league'):
        l.append((f"year-{mode}-{me}-25",f"{L75} {me} 25 76 {mode}"))
write('v77l.tsv', l)
