#!/usr/bin/env python3
"""Regenerates the 1944 Italian campaign data from the pre-1944 backups, so a tuning run is one command.
Writes data/campaign/theatres.json (the italy block), setup-1939.json (expedition rows) and air.json (plan)."""
import json, re, sys
SP = '/tmp/claude-0/-home-user/955ccfcd-1fde-5188-ad46-d1826f6d9dba/scratchpad'
ROOT = '/home/user/theater-command/data/campaign'

# ---- names -------------------------------------------------------------------------------------
XIII43 = 'Eighth Army: XIII Corps (5th, 50th and 78th Divisions)'
XXX43 = 'Eighth Army: XXX Corps (51st, 1st Canadian and 1st Airborne Divisions)'
V43 = 'Eighth Army: V Corps (1st Airborne and 8th Indian Divisions)'
II43 = 'Seventh Army: II Corps (1st, 9th and 45th Infantry Divisions)'
PROV43 = 'Seventh Army: Provisional Corps (3rd Infantry, 2nd Armored and 82nd Airborne Divisions)'
VI43 = 'Fifth Army: VI Corps (36th and 45th Infantry Divisions)'
X43 = 'Fifth Army: X Corps (British 46th, 56th and 7th Armoured Divisions)'

VI_A = 'Fifth Army: VI Corps (3rd and 45th Infantry and British 1st Divisions)'
VI_B = 'Fifth Army: VI Corps (3rd and 45th Infantry, 1st Armored and British 1st and 56th Divisions)'
II_A = 'Fifth Army: II Corps (34th and 36th Infantry Divisions)'
II_B = 'Fifth Army: II Corps (34th, 36th, 85th and 88th Infantry Divisions)'
FEC_A = 'Fifth Army: French Expeditionary Corps (2nd Moroccan and 3rd Algerian Divisions)'
FEC_B = 'Fifth Army: French Expeditionary Corps (2nd Moroccan, 3rd Algerian, 4th Moroccan Mountain and 1st Free French Divisions)'
XIII = 'Eighth Army: XIII Corps (4th, 78th and 6th Armoured Divisions)'
CAN = 'Eighth Army: I Canadian Corps (1st Canadian Infantry and 5th Canadian Armoured Divisions)'
V = 'Eighth Army: V Corps (8th and 10th Indian Divisions)'
NZ = 'New Zealand Corps (2nd New Zealand and 4th Indian Divisions)'
POL = 'Eighth Army: Polish II Corps (3rd Carpathian and 5th Kresowa Divisions)'

SOUTH = ['Sicily', 'Calabria', 'Apulia', 'Naples']
E = ['ITA', 'GER']

# ---- knobs -------------------------------------------------------------------------------------
K = dict(
    gustav={'Cassino': 3, 'Abruzzo': 2, 'Rome': 2},
    anzioWorks=2, cassinoWorks=2, abruzzoWorks=2,
    firstOdds=0.15, diademUK=0.25, diademUSA=0.5,
    fisch=0.6,
)
for a in sys.argv[1:]:
    k, v = a.split('=')
    K[k] = json.loads(v)

# ---- the theatre -------------------------------------------------------------------------------
old = json.load(open(f'{SP}/theatres-pre44.json'))['italy']
plans = old['plans']
uk43 = [p for p in plans['UK']]
us43 = [p for p in plans['USA']]

ger = [
    {'name': 'Back to the Gustav Line', 'from': 49, 'to': 50, 'ifLost': ['Sicily'], 'posture': 'defend', 'quiet': True, 'abandon': SOUTH, 'staging': K['gustav']},
    {'name': 'The Gustav Line', 'from': 51, 'to': 99, 'ifLost': ['Sicily'], 'posture': 'defend', 'quiet': True, 'abandon': SOUTH, 'staging': K['gustav']},
    {'name': 'The Strait of Messina', 'from': 47, 'to': 99, 'posture': 'defend', 'quiet': True},
]
uk = uk43 + [
]
usa = us43 + [
    {'name': 'Operation Shingle', 'from': 53, 'to': 53, 'posture': 'attack', 'enemies': E, 'objectives': ['Cassino'], 'targets': ['Cassino'], 'minOdds': K['firstOdds'], 'renew': False,
     'staging': {'Anzio': 1}, 'landings': [{'corps': VI_A, 'to': ['Anzio'], 'minOdds': 0.9}]},
]

italy = {
    'name': 'Italy',
    'bases': old['bases'],
    '_note': old['_note'],
    'regions': ['Malta', 'Sicily', 'Calabria', 'Apulia', 'Naples', 'Cassino', 'Anzio', 'Abruzzo', 'Rome', 'Marche', 'Tuscany', 'Emilia', 'Liguria', 'Sardinia', 'Corsica'],
    'works': [
        [50, 'Abruzzo', K['abruzzoWorks'], ['GER'], 'German engineers are fortifying the mountains across Italy south of Rome, from the Garigliano to the Sangro: the Gustav Line.'],
        [50, 'Cassino', K['cassinoWorks'], ['GER'], 'The western end of the Gustav Line is anchored on Monte Cassino, above the one good road to Rome.'],
        [54, 'Anzio', K['anzioWorks'], ['USA', 'UK'], 'The beachhead at Anzio is dug in. Every yard of it is under German guns.'],
    ],
    'plans': {'UK': uk, 'USA': usa, 'GER': ger},
}

def block(t):
    out = ['  "italy": {']
    out.append(f'    "name": {json.dumps(t["name"])},')
    out.append(f'    "bases": {json.dumps(t["bases"])},')
    out.append(f'    "_note": {json.dumps(t["_note"])},')
    out.append(f'    "regions": {json.dumps(t["regions"])},')
    out.append('    "works": [')
    out.append(',\n'.join('      ' + json.dumps(w) for w in t['works']))
    out.append('    ],')
    out.append('    "plans": {')
    nats = list(t['plans'].keys())
    for i, nat in enumerate(nats):
        out.append(f'      "{nat}": [')
        out.append(',\n'.join('        ' + json.dumps(p) for p in t['plans'][nat]))
        out.append('      ]' + (',' if i < len(nats) - 1 else ''))
    out.append('    }')
    out.append('  }')
    out.append('}')
    return '\n'.join(out) + '\n'

src = open(f'{SP}/theatres-pre44.json').read()
k = src.index('  "italy": {')
open(f'{ROOT}/theatres.json', 'w').write(src[:k] + block(italy))
json.load(open(f'{ROOT}/theatres.json'))

# ---- the formations ----------------------------------------------------------------------------
W = {'war': ['USA', 'GER']}
rows = [
    [53, 'USA', VI_A, 'inf', 1, 'Naples', {'joins': VI43}],
    [53, 'USA', II_A, 'inf', 2, 'Naples', {'gen': 'Geoffrey Keyes', 'join': 1, 'xp': 0.3, 'from': [II43, PROV43], **W,
        'news': 'Eisenhower, Montgomery, Bradley and Patton have left the Mediterranean for England, and seven veteran divisions have gone with them for the invasion of France. Alexander fights on in Italy with what is left.'}],
    [53, 'USA', FEC_A, 'mtn', 2, 'Naples', {'gen': 'Alphonse Juin', 'trait': 'attacker', 'join': 1, 'xp': 0.35, **W}],
    [53, 'UK', XIII, 'inf', 3, 'Naples', {'gen': 'Oliver Leese', 'join': 1, 'xp': 0.4, 'from': [XIII43], 'war': ['UK', 'GER']}],
    [53, 'UK', CAN, 'mot', 2, 'Apulia', {'gen': 'E. L. M. Burns', 'join': 1, 'xp': 0.35, 'from': [XXX43], 'war': ['UK', 'GER']}],
    [53, 'UK', V, 'inf', 2, 'Apulia', {'gen': 'Charles Allfrey', 'join': 1, 'xp': 0.3, 'from': [V43], 'war': ['UK', 'GER']}],
    [53, 'GER', 'I Parachute Corps (4th Parachute and 715th Divisions)', 'mot', 2, 'Rome', {'gen': 'Alfred Schlemm', 'join': 1, 'xp': 0.25, **W}],
    [54, 'GER', '14th Army: 114th Jager and 362nd Divisions', 'inf', 2, 'Rome', {'gen': 'Eberhard von Mackensen', 'join': 1, 'xp': 0.2, **W}],
    [54, 'USA', VI_B, 'mot', 2, 'Anzio', {'joins': VI_A}],
    [54, 'UK', NZ, 'inf', 2, 'Naples', {'gen': 'Bernard Freyberg', 'join': 1, 'xp': 0.45, 'war': ['UK', 'GER']}],
]
src = open(f'{SP}/setup-pre44.json').read()
last = f'    [49, "USA", "{X43}"'
k = src.index(last)
e = src.index('\n', k)
add = ''.join(',\n    ' + json.dumps(r) for r in rows)
open(f'{ROOT}/setup-1939.json', 'w').write(src[:e] + add + src[e:])
json.load(open(f'{ROOT}/setup-1939.json'))
print('written', K)
