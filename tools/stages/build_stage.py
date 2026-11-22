# Builds the Pearl Harbor (Oahu) and Midway stage geometry for the 3D game from open map data:
# coast, reef and harbour from Overture Maps (OpenStreetMap-derived, ODbL), heights from the Terrarium
# elevation tiles (Mapzen / AWS open data). Output: local metres east and north of each stage's origin.
import json, math, os, sys
import numpy as np
WORK = os.environ.get('STAGE_WORK', '/tmp/stage-work')
from shapely.geometry import shape, Polygon, MultiPolygon, mapping, box
from shapely.ops import unary_union, transform
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dem import mosaic

def local(lat0, lon0):
    kx = 111320.0 * math.cos(math.radians(lat0)); ky = 110574.0 + 1110.0 * math.sin(math.radians(lat0)) ** 2
    return (lambda lon, lat: ((lon - lon0) * kx, (lat - lat0) * ky)), kx, ky

def rings(geom, f, tol, least):
    out = []
    if geom.geom_type not in ('Polygon', 'MultiPolygon'): return []
    gs = [geom] if geom.geom_type == 'Polygon' else list(geom.geoms)
    for p in gs:
        q = transform(lambda x, y, z=None: f(x, y), p).simplify(tol, preserve_topology=True)
        if q.is_empty or q.area < least: continue
        qs = [q] if q.geom_type == 'Polygon' else list(q.geoms)
        for r in qs:
            out.append({'outer': [[round(x, 1), round(y, 1)] for x, y in list(r.exterior.coords)[:-1]],
                        'holes': [[[round(x, 1), round(y, 1)] for x, y in list(h.coords)[:-1]] for h in r.interiors if Polygon(h).area >= least]})
    return out

ATTR = 'Coast, reef and harbour: Overture Maps Foundation / OpenStreetMap contributors (ODbL). Heights: Terrarium elevation tiles (Mapzen, AWS Open Data; SRTM and others). Shapes are today\'s: the historian marks what differed in the record\'s year.'

def oahu():
    lat0, lon0 = 21.3638, -157.9520  # Battleship Row, F-6
    f, kx, ky = local(lat0, lon0)
    d = json.load(open(os.path.join(WORK, 'oahu-land.json')))
    land = [shape(x['geometry']) for x in d['features'] if x['properties']['subtype'] == 'land' and x['properties']['class'] in ('island', 'islet')]
    land = [g for g in land if g.geom_type in ('Polygon', 'MultiPolygon') and g.bounds[1] > 21.2 and g.bounds[3] < 21.8 and g.bounds[0] > -158.4]
    reef = [shape(x['geometry']) for x in d['features'] if x['properties']['subtype'] == 'reef']
    L = unary_union(land)
    # Heights: Terrarium zoom 12 (about 38 m a pixel here), resampled to 150 m cells over the island.
    la0, la1, lo0, lo1 = 21.24, 21.73, -158.29, -157.64
    m = mosaic(12, la0, la1, lo0, lo1)
    H, W = m.shape
    cell = 150.0
    e0, n0 = f(lo0, la0); e1, n1 = f(lo1, la1)
    cols = int((e1 - e0) // cell); rows = int((n1 - n0) // cell)
    data = []
    from shapely.prepared import prep
    PL = prep(transform(lambda x, y, z=None: f(x, y), L))
    from shapely.geometry import Point
    for r in range(rows):
        n = n0 + (r + 0.5) * cell
        lat = lat0 + n / ky
        # Mercator row of the mosaic for this latitude.
        def ty(lat):
            return (1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2
        fy = (ty(lat) - ty(la1)) / (ty(la0) - ty(la1)) * (H - 1)
        row = []
        for c in range(cols):
            e = e0 + (c + 0.5) * cell
            lon = lon0 + e / kx
            fx = (lon - lo0) / (lo1 - lo0) * (W - 1)
            x0, y0 = int(fx), int(fy)
            x1, y1 = min(W - 1, x0 + 1), min(H - 1, y0 + 1)
            ax, ay = fx - x0, fy - y0
            v = (m[y0, x0] * (1 - ax) + m[y0, x1] * ax) * (1 - ay) + (m[y1, x0] * (1 - ax) + m[y1, x1] * ax) * ay
            on = PL.contains(Point(e, n))
            row.append(int(round(max(1.0, v))) if on else int(round(min(-2.0, v))) )
        data.append(row)
    # 1941 airfields (the historian checks): where the defending fighters rise from, and the bombers' fields struck.
    fields = [
        {'name': 'Wheeler Field', 'lat': 21.4810, 'lon': -158.0375, 'kind': 'fighters', 'runway': 70, 'len': 1500},
        {'name': 'Haleiwa', 'lat': 21.5970, 'lon': -158.1000, 'kind': 'fighters', 'runway': 10, 'len': 900},
        {'name': 'Bellows Field', 'lat': 21.3600, 'lon': -157.7130, 'kind': 'fighters', 'runway': 90, 'len': 1200},
        {'name': 'Hickam Field', 'lat': 21.3380, 'lon': -157.9500, 'kind': 'bombers', 'runway': 80, 'len': 2000},
        {'name': 'Ford Island', 'lat': 21.3640, 'lon': -157.9610, 'kind': 'navy', 'runway': 50, 'len': 1200},
        {'name': 'Kaneohe Bay', 'lat': 21.4500, 'lon': -157.7680, 'kind': 'navy', 'runway': 40, 'len': 1200},
        {'name': 'Ewa', 'lat': 21.3300, 'lon': -158.0500, 'kind': 'navy', 'runway': 110, 'len': 1000},
    ]
    for x in fields: x['at'] = [round(v, 1) for v in f(x['lon'], x['lat'])]
    infra = json.load(open(os.path.join(WORK, 'pearl-infra.json')))
    piers = [x for x in infra['features'] if x['properties']['class'] == 'pier' and x['geometry']['type'] in ('Polygon', 'LineString', 'MultiPolygon')]
    pierOut = []
    for x in piers:
        g = transform(lambda a, b, z=None: f(a, b), shape(x['geometry']))
        if g.geom_type == 'LineString': g = g.buffer(6, cap_style=2)
        g = g.simplify(1.5)
        if g.is_empty or g.area < 40: continue
        for p in ([g] if g.geom_type == 'Polygon' else list(g.geoms)):
            pierOut.append([[round(a, 1), round(b, 1)] for a, b in list(p.exterior.coords)[:-1]])
    tanks = [x for x in infra['features'] if x['properties']['class'] == 'storage_tank']
    tankOut = []
    for x in tanks:
        g = shape(x['geometry']); c = g.centroid
        r = math.sqrt(max(1.0, transform(lambda a, b, z=None: f(a, b), g).area) / math.pi) if g.geom_type != 'Point' else 12
        tankOut.append({'at': [round(v, 1) for v in f(c.x, c.y)], 'r': round(min(40, r), 1)})
    out = {
        'key': 'oahu', 'name': 'Oahu', 'origin': [lat0, lon0], 'units': 'metres east and north of the origin',
        'land': rings(L, f, 12.0, 400.0), 'reef': [r for g in reef for r in rings(g, f, 25.0, 2000.0)],
        'heights': {'e0': round(e0, 1), 'n0': round(n0, 1), 'cell': cell, 'cols': cols, 'rows': rows, 'data': data},
        'fields': fields, 'piers': pierOut, 'tanks': tankOut, 'attribution': ATTR,
    }
    return out

def midway():
    lat0, lon0 = 28.2080, -177.3600  # between Sand Island and Eastern Island
    f, kx, ky = local(lat0, lon0)
    d = json.load(open(os.path.join(WORK, 'midway-land.json')))
    land = [shape(x['geometry']) for x in d['features'] if x['properties']['subtype'] == 'land' and x['properties']['class'] in ('island', 'islet')]
    land = [g for g in land if g.geom_type in ('Polygon', 'MultiPolygon')]
    reef = [shape(x['geometry']) for x in d['features'] if x['properties']['subtype'] == 'reef']
    L = unary_union(land)
    fields = [{'name': 'Eastern Island', 'lat': 28.2125, 'lon': -177.3370, 'kind': 'fighters', 'runway': 60, 'len': 1500},
              {'name': 'Sand Island seaplane base', 'lat': 28.2060, 'lon': -177.3760, 'kind': 'navy', 'runway': 0, 'len': 0}]
    for x in fields: x['at'] = [round(v, 1) for v in f(x['lon'], x['lat'])]
    return {'key': 'midway', 'name': 'Midway', 'origin': [lat0, lon0], 'units': 'metres east and north of the origin',
            'land': rings(L, f, 4.0, 50.0), 'reef': [r for g in reef for r in rings(g, f, 8.0, 500.0)],
            'heights': {'e0': 0, 'n0': 0, 'cell': 0, 'cols': 0, 'rows': 0, 'data': [], 'flat': 3},
            'fields': fields, 'piers': [], 'tanks': [], 'attribution': ATTR}

if __name__ == '__main__':
    for s in (oahu(), midway()):
        p = os.path.join(WORK, 'out', f"{s['key']}.json")
        json.dump(s, open(p, 'w'), separators=(',', ':'))
        pts = sum(len(r['outer']) + sum(len(h) for h in r['holes']) for r in s['land'])
        print(s['key'], 'land rings', len(s['land']), 'points', pts, 'reef', len(s['reef']), 'piers', len(s['piers']), 'tanks', len(s['tanks']), 'heights', s['heights']['cols'], 'x', s['heights']['rows'])
