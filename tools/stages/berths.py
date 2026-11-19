import json, math, os
from shapely.geometry import shape, Point, Polygon
from shapely.ops import unary_union
import numpy as np
WORK = os.environ.get('STAGE_WORK', '/tmp/stage-work')
lat0, lon0 = 21.3638, -157.9520
kx = 111320.0 * math.cos(math.radians(lat0)); ky = 110574.0 + 1110.0 * math.sin(math.radians(lat0)) ** 2
def L(lat, lon): return ((lon - lon0) * kx, (lat - lat0) * ky)
s = json.load(open(os.path.join(WORK, 'out', 'oahu.json')))
land = unary_union([Polygon(r['outer'], r['holes']) for r in s['land']])
row = math.radians(231); u = (math.sin(row), math.cos(row)); v = (math.sin(row - math.pi / 2), math.cos(row - math.pi / 2))  # v: bearing 141 (to the channel)
def mid(a, b): return ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
def off(p, out, along=0.0): return (p[0] + v[0] * out + u[0] * along, p[1] + v[1] * out + u[1] * along)
q = {k: L(*ll) for k, ll in {
  'nev': (21.36642, -157.94849), 'ari': (21.36521, -157.95010), 'ves': (21.36478, -157.95067), 'ten': (21.36400, -157.95171), 'wva': (21.36357, -157.95229),
  'f5a': (21.36278, -157.95330), 'f5b': (21.36230, -157.95395)}.items()}
F8 = q['nev']; F7 = mid(q['ari'], q['ves']); F6 = mid(q['ten'], q['wva']); F5 = mid(q['f5a'], q['f5b'])
step = math.dist(F6, F5)
F4 = (F5[0] + u[0] * step, F5[1] + u[1] * step); F3 = (F4[0] + u[0] * step, F4[1] + u[1] * step)
IN, OUT = 22.0, 52.0
B = []  # id, cls, record name, x, y, heading deg, exposed (0 sheltered .. 1 open to torpedo runs), date of the record
LB = {}
def snap(p, clear):
    if not LB.setdefault(clear, land.buffer(clear)).contains(Point(p)): return p
    for r in range(10, 600, 10):
        best = None
        for k in range(32):
            a = 2 * math.pi * k / 32; q = (p[0] + r * math.sin(a), p[1] + r * math.cos(a))
            if not LB[clear].contains(Point(q)): best = q; break
        if best: return best
    return p
def add(i, c, n, p, h, e, note='', dock=False):
    if not dock: p = snap(p, {'bb': 22, 'cv': 22, 'ca': 15, 'dd': 10, 'ss': 8}.get(c, 12))
    B.append({'id': i, 'cls': c, 'name': n, 'at': [round(p[0], 1), round(p[1], 1)], 'face': h, 'open': e, **({'note': note} if note else {}), **({'dock': True} if dock else {})})
add('F-8', 'bb', 'Nevada', off(F8, IN), 51, 1.0)
add('F-7', 'bb', 'Arizona', off(F7, IN), 51, 0.6)
add('F-7o', 'ar', None, off(F7, OUT), 51, 1.0, 'Vestal, a repair ship: no campaign class')
add('F-6', 'bb', 'Tennessee', off(F6, IN), 51, 0.3)
add('F-6o', 'bb', 'West Virginia', off(F6, OUT), 51, 1.0)
add('F-5', 'bb', 'Maryland', off(F5, IN), 51, 0.3)
add('F-5o', 'bb', 'Oklahoma', off(F5, OUT), 51, 1.0)
add('F-4', 'ao', None, off(F4, IN), 51, 1.0, 'Neosho, an oiler: no campaign class')
add('F-3', 'bb', 'California', off(F3, IN), 51, 1.0)
# The Navy Yard (the historian places these; the docks and piers are today's).
mouth = (-650.0, -1215.0); ax = math.radians(165); A = (math.sin(ax), math.cos(ax)); S = (math.sin(ax + math.pi / 2), math.cos(ax + math.pi / 2))
add('DD1', 'bb', 'Pennsylvania', (mouth[0] + A[0] * 110, mouth[1] + A[1] * 110), 165, 0.0, 'Dry Dock No. 1', dock=True)
add('DD1a', 'dd', 'Cassin', (mouth[0] + A[0] * 260 + S[0] * 7, mouth[1] + A[1] * 260 + S[1] * 7), 165, 0.0, 'Dry Dock No. 1, ahead of Pennsylvania', dock=True)
add('DD1b', 'dd', 'Downes', (mouth[0] + A[0] * 260 - S[0] * 7, mouth[1] + A[1] * 260 - S[1] * 7), 165, 0.0, 'Dry Dock No. 1, ahead of Pennsylvania', dock=True)
add('1010', 'ca', 'Helena', (-395.0, -990.0), 30, 0.8, '1010 Dock')
add('1010o', 'cm', None, (-395.0 - 19 * math.sin(math.radians(60)), -990.0 + 19 * math.cos(math.radians(60))), 30, 1.0, 'Oglala, a minelayer, outboard of Helena: no campaign class')
add('B-21', 'ca', 'Honolulu', L(21.3540, -157.9509), 51, 0.2)
add('B-22', 'ca', 'St. Louis', L(21.3542, -157.9497), 51, 0.3)
add('B-19', 'ca', 'San Francisco', L(21.3524, -157.9524), 51, 0.1)
add('B-20', 'ca', 'New Orleans', L(21.3528, -157.9512), 51, 0.1)
add('YFD-2', 'dd', 'Shaw', L(21.3518, -157.9465), 141, 0.0, 'floating dry dock YFD-2')
add('C-6', 'ca', 'Phoenix', L(21.3712, -157.9440), 20, 0.7, 'anchored north-east of Ford Island')
# Ford Island's north-west side, the carrier berths.
nw = math.radians(52.7); w = (math.sin(nw), math.cos(nw)); nwo = (math.sin(nw - math.pi / 2), math.cos(nw - math.pi / 2))
F9q, F10q, F11q = L(21.36582, -157.96622), L(21.36728, -157.96419), L(21.36882, -157.96206)
stepw = math.dist(F10q, F11q)
F12q = (F11q[0] + w[0] * stepw, F11q[1] + w[1] * stepw); F13q = (F12q[0] + w[0] * stepw, F12q[1] + w[1] * stepw)
def offw(p, o): return (p[0] + nwo[0] * o, p[1] + nwo[1] * o)
add('F-9', 'cv', None, offw(F9q, 30), 232, 1.0, 'a carrier berth, empty that day')
add('F-10', 'cv', None, offw(F10q, 30), 232, 1.0, 'Tangier, a seaplane tender, that day: a carrier berth')
add('F-11', 'ag', None, offw(F11q, 25), 232, 1.0, 'Utah, a target ship: no campaign class')
add('F-12', 'ca', 'Raleigh', offw(F12q, 25), 232, 1.0)
add('F-13', 'ca', 'Detroit', offw(F13q, 25), 232, 1.0)
# The submarine base.
add('S-10', 'ss', 'Tautog', L(21.3562, -157.9425), 160, 0.0)
add('S-11', 'ss', 'Narwhal', L(21.3570, -157.9418), 160, 0.0)
add('S-20', 'ss', 'Dolphin', L(21.3577, -157.9440), 160, 0.0)
# Unnamed moorings for the rest, in water clear of every berth: destroyer nests in East Loch, then the lochs.
def water(p, clear=70):
    if land.buffer(clear).contains(Point(p)): return False
    return all(math.dist(p, b['at']) > 90 for b in B)
spots = []
for (la, lo) in [(21.3725, -157.9465), (21.3742, -157.9440), (21.3760, -157.9415), (21.3778, -157.9390), (21.3712, -157.9490), (21.3795, -157.9365)]:
    p = L(la, lo)
    for k in range(4):
        x = (p[0] + k * 14 * math.sin(math.radians(140)), p[1] + k * 14 * math.cos(math.radians(140)))
        x = snap(x, 12) if k == 0 else (spots[-1]['at'][0] + 14 * math.sin(math.radians(140)), spots[-1]['at'][1] + 14 * math.cos(math.radians(140)))
        spots.append({'at': [round(x[0], 1), round(x[1], 1)], 'face': 50, 'nest': True})
# A grid over the harbour for anything else.
for gy in np.arange(-3500, 4500, 260):
    for gx in np.arange(-6500, 3000, 260):
        p = (float(gx), float(gy))
        if water(p, 90) and Polygon([(-7000, -2200), (3500, -2200), (3500, 5000), (-7000, 5000)]).contains(Point(p)):
            spots.append({'at': [round(p[0], 1), round(p[1], 1)], 'face': 50})
bad = [b['id'] for b in B if land.contains(Point(b['at']))]
print('berths', len(B), 'on land:', bad, 'spots', len(spots), 'nests', sum(1 for x in spots if x.get('nest')), 'nests on land', sum(1 for x in spots if x.get('nest') and land.contains(Point(x['at']))))
json.dump({'berths': B, 'spots': spots}, open(os.path.join(WORK, 'out', 'oahu-berths.json'), 'w'))
