#!/usr/bin/env python3
"""Writes the Version 75 queues (America ships its army overseas), each short enough for the two-hour limit.
usage: mk75.py   (writes v75a.tsv, v75b.tsv, v75c.tsv, v75d.tsv)"""
import json
B56='node out75/probe56.mjs'; B58='node out75/probe58.mjs'; B59='node out75/probe59.mjs'
def j(d): return "'"+json.dumps(d,separators=(',',':'))+"'"
BUY=["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]
GERD=[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]]
ITAD=[[10,"FRA"],[10,"UK"],[14,"GRE"]]
HAND={'GER':{"minOdds":1.4,"declare":GERD,"land":True},'ITA':{"minOdds":1.4,"declare":ITAD,"land":True},
      'UK':{"minOdds":1.5,"land":True,"buy":BUY},'FRA':{"minOdds":1.6,"homeOnly":True},'SOV':{"minOdds":1.5},
      'USA':{"minOdds":1.5,"land":True,"stage":True,"buy":BUY},'CAN':{"minOdds":1.5,"land":True,"stage":True,"buy":BUY}}
FIELD={k:{**{x:v[x] for x in v if x not in('buy','stage')},"depth":"field"} for k,v in HAND.items()}
IDLE={'idle':True}
def write(name, rows):
    open(name,'w').write(''.join(f"{a}\t{b}\n" for a,b in rows)); print(name, len(rows))
# A. The first number, the American seat, Britain by hand.
a=[]
for s in range(17,41,3): a.append((f"watch49s3-{s}",f"{B59} 3 {s},{s+1},{s+2} 124"))
for s in range(25,37):
    a.append((f"seatstage-USA-{s}",f"{B58} USA 1 {s} 76 {j(HAND['USA'])}"))
    a.append((f"seatfield-USA-{s}",f"{B58} USA 1 {s} 76 {j(FIELD['USA'])}"))
    a.append((f"seatidle-USA-{s}",f"{B58} USA 1 {s} 76 {j(IDLE)}"))
    a.append((f"seatearly-USA-{s}",f"{B58} USA 1 {s} 76 {j({**HAND['USA'],'declare':[[13,'GER']]})}"))
for s in range(25,49):
    a.append((f"seat-UK-{s}",f"{B58} UK 1 {s} 76 {j(HAND['UK'])}"))
    a.append((f"seatfield-UK-{s}",f"{B58} UK 1 {s} 76 {j(FIELD['UK'])}"))
for s in range(37,49): a.append((f"seatstage-USA-{s}",f"{B58} USA 1 {s} 76 {j(HAND['USA'])}"))
for s in range(25,31): a.append((f"seatfirst-USA-{s}",f"{B58} USA 1 {s} 76 {j({**HAND['USA'],'declare':[[1,'GER']]})}"))
write('v75a.tsv', a)
# B. Par from the other seats, seeds 25 to 48, by hand and at Field command.
b=[]
for s in range(25,49):
    for me in ('GER','SOV','FRA','ITA'):
        b.append((f"seat-{me}-{s}",f"{B58} {me} 1 {s} 76 {j(HAND[me])}"))
        b.append((f"seatfield-{me}-{s}",f"{B58} {me} 1 {s} 76 {j(FIELD[me])}"))
write('v75b.tsv', b)
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
write('v75c.tsv', c)
# D. The idle player from every seat; Canada and Mexico for errors.
d=[]
for s in range(1,13):
    for me in ('GER','UK','SOV','FRA','ITA'): d.append((f"seatidle-{me}-{s}",f"{B58} {me} 1 {s} 76 {j(IDLE)}"))
for s in range(25,31):
    d.append((f"seatstage-CAN-{s}",f"{B58} CAN 1 {s} 76 {j(HAND['CAN'])}"))
for s in range(25,28):
    d.append((f"seatpath-MEX-{s}",f"{B58} MEX 1 {s} 76 {j({'minOdds':1.5,'land':True,'path':'allies','declare':[[33,'GER']]})}"))
    d.append((f"seatpathaxis-MEX-{s}",f"{B58} MEX 1 {s} 76 {j({'minOdds':1.5,'path':'axis','declare':[[13,'USA']]})}"))
for s in range(1,9):
    for me in ('GER','UK','SOV','FRA','ITA','USA'): d.append((f"domidle-{me}-{s}",f"{B56} {me} 1 {s} 124 {j(IDLE)}"))
write('v75d.tsv', d)
