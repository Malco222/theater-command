#!/usr/bin/env python3
"""Writes the Version 74 queue (the wider map with North America), most wanted first. usage: mk74.py > v74.tsv"""
import json
B56='node out74/probe56.mjs'; B58='node out74/probe58.mjs'; B59='node out74/probe59.mjs'
def j(d): return "'"+json.dumps(d,separators=(',',':'))+"'"
BUY=["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]
GERD=[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]]
ITAD=[[10,"FRA"],[10,"UK"],[14,"GRE"]]
HAND={'GER':{"minOdds":1.4,"declare":GERD,"land":True},'ITA':{"minOdds":1.4,"declare":ITAD,"land":True},
      'UK':{"minOdds":1.5,"land":True,"buy":BUY},'FRA':{"minOdds":1.6,"homeOnly":True},'SOV':{"minOdds":1.5},
      'USA':{"minOdds":1.5,"land":True,"buy":BUY},'CAN':{"minOdds":1.5,"land":True,"buy":BUY},'MEX':{"minOdds":1.5,"land":True}}
FIELD={k:{**{x:v[x] for x in v if x!='buy'},"depth":"field"} for k,v in HAND.items()}
out=[]
# 1. The war with nobody playing, to December 1949, seeds 17 to 40.
for s in range(17,41,3): out.append((f"watch49s3-{s}",f"{B59} 3 {s},{s+1},{s+2} 124"))
# 2. The yardstick: every seat's landings followed for three months (the domination set by hand and at Field command).
CROSS={"cross":True,"landRisk":0.4}
DOM={'GER':{**HAND['GER'],**CROSS,"crossFoe":"UK","buy":["ship:tp","air:ftr","corps:inf","corps:arm","work:factory"]},
     'UK':{"minOdds":1.5,"land":True,**CROSS,"buy":["corps:arm","corps:inf","ship:tp","air:ftr","work:factory"]},
     'ITA':{**HAND['ITA'],**CROSS,"crossFoe":"UK","buy":["ship:tp","air:ftr","corps:inf"]},
     'FRA':{"minOdds":1.6,"land":True,**CROSS,"buy":["corps:arm","corps:inf","ship:tp","air:ftr","work:factory"]},
     'USA':{"minOdds":1.5,"land":True,**CROSS,"buy":["corps:arm","corps:inf","ship:tp","air:ftr","work:factory"]}}
DOMF={'GER':{**HAND['GER'],**CROSS,"crossFoe":"UK","depth":"field"},'SOV':{"minOdds":1.5,"land":True,**CROSS,"crossFoe":"UK","depth":"field"},
      'UK':{"minOdds":1.5,"land":True,**CROSS,"depth":"field"},'ITA':{**HAND['ITA'],**CROSS,"crossFoe":"UK","depth":"field"},
      'FRA':{"minOdds":1.6,"land":True,**CROSS,"depth":"field"},'USA':{"minOdds":1.5,"land":True,**CROSS,"depth":"field"}}
SOVFE={"minOdds":1.5,"land":True,**CROSS,"crossFoe":"UK","buy":["corps:arm","corps:inf","ship:tp","air:ftr","work:factory"],"crossFirst":True,"engs":6}
for s in range(1,9):
    for me in ('GER','UK','ITA','FRA','USA'): out.append((f"dom-{me}-{s}",f"{B56} {me} 1 {s} 124 {j(DOM[me])}"))
    out.append((f"domfirsteng-SOV-{s}",f"{B56} SOV 1 {s} 124 {j(SOVFE)}"))
    for me in ('GER','SOV','UK','ITA','FRA','USA'): out.append((f"domfield-{me}-{s}",f"{B56} {me} 1 {s} 124 {j(DOMF[me])}"))
# 3. The new seats, to December 1945: the United States, Canada, Mexico.
for s in range(25,37):
    out.append((f"seat-USA-{s}",f"{B58} USA 1 {s} 76 {j(HAND['USA'])}"))
    out.append((f"seatfield-USA-{s}",f"{B58} USA 1 {s} 76 {j(FIELD['USA'])}"))
    out.append((f"seatidle-USA-{s}",f"{B58} USA 1 {s} 76 {j({'idle':True})}"))
    out.append((f"seat-CAN-{s}",f"{B58} CAN 1 {s} 76 {j(HAND['CAN'])}"))
    out.append((f"seatfield-CAN-{s}",f"{B58} CAN 1 {s} 76 {j(FIELD['CAN'])}"))
    out.append((f"seatidle-CAN-{s}",f"{B58} CAN 1 {s} 76 {j({'idle':True})}"))
    out.append((f"seatally-MEX-{s}",f"{B58} MEX 1 {s} 76 {j({**HAND['MEX'],'declare':[[33,'GER']]})}"))
for s in range(25,31):
    out.append((f"seatidle-MEX-{s}",f"{B58} MEX 1 {s} 76 {j({'idle':True})}"))
    out.append((f"seataxis-MEX-{s}",f"{B58} MEX 1 {s} 76 {j({'minOdds':1.5,'declare':[[13,'USA']]})}"))
    out.append((f"seatearly-USA-{s}",f"{B58} USA 1 {s} 76 {j({**HAND['USA'],'declare':[[13,'GER']]})}"))
# 4. Par on the wider map, the middle stop, seeds 25 to 48, by hand and at Field command.
for s in range(25,49):
    for me in ('GER','UK','SOV','FRA','ITA'):
        out.append((f"seat-{me}-{s}",f"{B58} {me} 1 {s} 76 {j(HAND[me])}"))
        out.append((f"seatfield-{me}-{s}",f"{B58} {me} 1 {s} 76 {j(FIELD[me])}"))
for s in range(37,49):
    out.append((f"seat-USA-{s}",f"{B58} USA 1 {s} 76 {j(HAND['USA'])}"))
    out.append((f"seatfield-USA-{s}",f"{B58} USA 1 {s} 76 {j(FIELD['USA'])}"))
# 5. The idle player: never wins the world, never beats par.
for s in range(1,9):
    for me in ('GER','SOV','UK','ITA','FRA','USA'): out.append((f"domidle-{me}-{s}",f"{B56} {me} 1 {s} 124 {j({'idle':True})}"))
for s in range(1,13):
    for me in ('GER','SOV','UK','ITA','FRA'): out.append((f"seatidle-{me}-{s}",f"{B58} {me} 1 {s} 76 {j({'idle':True})}"))
for n,c in out: print(f"{n}\t{c}")
