# The 3D game's stage files for the set-piece air battles (ruling 238): the island, its shallows and reef, the
# piers, oil tanks, airfields and palms of Oahu (Pearl Harbor) and Midway, as ready-made triangles, so that the
# game only copies them into a mesh. Reads the stage outlines build_stage.py makes (from Overture Maps and the
# Terrarium heights); writes <out>/<key>-3d.json.
#
# Units: metres, x east and y north of the stage's origin, z up from the sea. The game turns and places the stage
# as the battle's ticket turns the map (stages.ts onBoard). Heights are kept in decimetres (whole numbers).
import json, math, os, sys, random
from yard import yard, DRAWN_FIELDS, CUT
import numpy as np
import shapely
from shapely.geometry import Polygon, MultiPolygon, Point, box, LineString
from shapely.ops import unary_union
from shapely.prepared import prep

WORK = os.environ.get('STAGE_WORK', '/tmp/stage-work')
OUT = os.environ.get('STAGE_OUT', os.path.join(WORK, 'out'))

# The colours, by name; each point of the ground carries one. The art director tunes them here.
PALETTE = [
    ('sand', '#D9CBA3'),       # 0 the beach and the coast's edge
    ('cane', '#6E8B3D'),       # 1 the lowland: sugar cane and grass
    ('pineapple', '#7A6A45'),  # 2 the red earth of the central plain's pineapple fields
    ('forest', '#3D5A2E'),     # 3 the mountains' forest
    ('rock', '#6A6155'),       # 4 bare cliffs
    ('town', '#9A938A'),       # 5 Honolulu and the yards
    ('shallows', '#4E8F8A'),   # 6 the shallows round the coast
    ('reef', '#8FC1B0'),       # 7 the reef just awash
    ('concrete', '#A7A39A'),   # 8 piers, quays, aprons
    ('runway', '#5A5A57'),     # 9 the runways
    ('scrub', '#8A9A5B'),      # 10 Midway's scrub and ironwood
    ('lagoon', '#5FB3B0'),     # 11 Midway's lagoon floor seen through clear water
]
IDX = {n: i for i, (n, _) in enumerate(PALETTE)}
# Oahu's own paint (the art director on build 1734: Pearl Harbor's land read as three plain colours, the shallows as a
# mint band): the greens darker, the earth redder, the sand a duller band, the shallows a darker teal-green. Midway
# keeps the palette above. The 3D game varies these point by point (Battle/AirGround.cpp).
OAHU_PAINT = {'sand': '#CDBD95', 'cane': '#5D7934', 'pineapple': '#7D5A3C', 'forest': '#35512A', 'shallows': '#3A6E68', 'reef': '#6C9E8E',
              'concrete': '#8F8B83'}  # (the concrete about 140 luma, worn in the game: her second note of 17:45Z)


def dm(v):
    return int(round(v * 10))


class Mesh:
    def __init__(self, name):
        self.name, self.v, self.c, self.t, self.key = name, [], [], [], {}

    def add(self, x, y, z, col):
        k = (round(x, 1), round(y, 1), round(z, 1), col)
        if k in self.key:
            return self.key[k]
        self.key[k] = len(self.c)
        self.v += [dm(x), dm(y), dm(z)]
        self.c.append(col)
        return self.key[k]

    def tri(self, a, b, c):
        if a != b and b != c and a != c:
            self.t += [a, b, c]

    def out(self):
        return {'name': self.name, 'v': self.v, 'c': self.c, 't': self.t}


def polys(g):
    if g.is_empty:
        return []
    if g.geom_type == 'Polygon':
        return [g]
    if g.geom_type in ('MultiPolygon', 'GeometryCollection'):
        return [p for x in g.geoms for p in polys(x)]
    return []


def triangles(poly):
    """The polygon cut into triangles along its own edges (shapely's constrained Delaunay)."""
    t = shapely.constrained_delaunay_triangles(poly)
    return [list(x.exterior.coords)[:3] for x in t.geoms]


def flat(mesh, geom, z, col):
    for p in polys(geom):
        for tr in triangles(p):
            mesh.tri(*[mesh.add(x, y, z, col) for x, y in tr])


def ground(stage, L, colour_of, cell=None):
    """The land with its heights: whole cells inside the coast as two triangles, cells the coast cuts as the piece
    of the cell on land, cut into triangles; heights from the grid's corners, the coast itself at 0.4 m."""
    H = stage['heights']
    m = Mesh('land')
    PL = prep(L)
    edge = L.boundary
    if not H['cols']:
        # Midway: flat coral islands a few metres high, on a fine grid of their own; the coast ramps them down.
        minx, miny, maxx, maxy = L.bounds
        cs = 40.0
        e0, n0 = minx - cs, miny - cs
        cols, rows = int((maxx - minx) // cs) + 3, int((maxy - miny) // cs) + 3
        flatz = H.get('flat', 3)
        H = {'e0': e0, 'n0': n0, 'cell': cs, 'cols': cols, 'rows': rows,
             'data': [[flatz if PL.contains(Point(e0 + (c + .5) * cs, n0 + (r + .5) * cs)) else -2 for c in range(cols)] for r in range(rows)]}
    e0, n0, cs, cols, rows = H['e0'], H['n0'], H['cell'], H['cols'], H['rows']
    g = np.array(H['data'], dtype=float)
    # The heights at the cells' corners: the mean of the land cells round each corner.
    land = np.where(g > 0, g, np.nan)
    pad = np.pad(land, 1, constant_values=np.nan)
    stack = np.stack([pad[:-1, :-1], pad[:-1, 1:], pad[1:, :-1], pad[1:, 1:]])
    with np.errstate(all='ignore'):
        corner = np.nanmean(stack, axis=0)
    corner = np.where(np.isnan(corner), 0.5, corner)  # (rows+1) x (cols+1)
    gy, gx = np.gradient(corner, cs)
    slope = np.hypot(gx, gy)

    def zat(x, y):
        fx = min(max((x - e0) / cs, 0), cols - 1e-6)
        fy = min(max((y - n0) / cs, 0), rows - 1e-6)
        i, j = int(fy), int(fx)
        ax, ay = fx - j, fy - i
        z = (corner[i, j] * (1 - ax) + corner[i, j + 1] * ax) * (1 - ay) + (corner[i + 1, j] * (1 - ax) + corner[i + 1, j + 1] * ax) * ay
        s = slope[min(i + (ay > .5), rows), min(j + (ax > .5), cols)]
        return z, s

    # Which cells the coast cuts, and so which corners lie near it: their height ramps down to the water.
    cut = np.zeros((rows, cols), dtype=bool)
    inside = np.zeros((rows, cols), dtype=bool)
    for r in range(rows):
        for c in range(cols):
            x0, y0 = e0 + c * cs, n0 + r * cs
            sq = box(x0, y0, x0 + cs, y0 + cs)
            if not PL.intersects(sq):
                continue
            if PL.contains(sq):
                inside[r, c] = True
            else:
                cut[r, c] = True
    near = np.zeros((rows + 1, cols + 1), dtype=bool)
    for dr in (0, 1):
        for dc in (0, 1):
            near[dr:rows + dr, dc:cols + dc] |= cut
    ztab = {}

    def height(x, y, i=None, j=None):
        """One height for one place, whichever cell asks: no cracks between a whole cell and a cut one."""
        k = (round(x, 1), round(y, 1))
        if k in ztab:
            return ztab[k]
        if i is not None:
            z, s = corner[i, j], slope[i, j]
            d = edge.distance(Point(x, y)) if near[i, j] else 1e9
        else:
            z, s = zat(x, y)
            d = edge.distance(Point(x, y))
        z = 0.4 if d < 0.5 else max(1.0, min(z, 0.4 + d * 0.25))
        ztab[k] = (z, s, d)
        return ztab[k]

    for r in range(rows):
        for c in range(cols):
            if inside[r, c]:
                corners = [(i, j, e0 + j * cs, n0 + i * cs) for (i, j) in ((r, c), (r, c + 1), (r + 1, c + 1), (r + 1, c))]
                hs = [height(x, y, i, j) for i, j, x, y in corners]
                # A field of the plain is one colour to its edges, so the fields read as fields.
                xc, yc = e0 + (c + .5) * cs, n0 + (r + .5) * cs
                field = colour_of(xc, yc, sum(h[0] for h in hs) / 4, max(h[1] for h in hs), min(h[2] for h in hs), True)
                ids = [m.add(x, y, h[0], field if field is not None else colour_of(x, y, h[0], h[1], h[2])) for (i, j, x, y), h in zip(corners, hs)]
                m.tri(ids[0], ids[1], ids[2])
                m.tri(ids[0], ids[2], ids[3])
            elif cut[r, c]:
                x0, y0 = e0 + c * cs, n0 + r * cs
                for p in polys(L.intersection(box(x0, y0, x0 + cs, y0 + cs))):
                    for tr in triangles(p):
                        ids = []
                        for x, y in tr:
                            z, s, d = height(x, y)
                            ids.append(m.add(x, y, z, colour_of(x, y, z, s, d)))
                        m.tri(*ids)
    return m


def oahu_colour(x, y, z, s, d, cell=False):
    if cell:
        # The central plain between the ranges, Wahiawa to the Leilehua plateau: blocks of pineapple fields on red
        # earth among the cane, a fixed patchwork (the same every time).
        if 150 < z < 330 and s < 0.12 and -16000 < x < -3000 and 6000 < y < 22000 and d > 300:
            bx, by = int((x + 40000) // 600), int((y + 40000) // 450)
            return IDX['pineapple'] if (bx * 73856093 ^ by * 19349663) % 7 < 2 else None
        return None
    if d < 25 and z < 4:
        return IDX['sand']
    if s > 0.9:
        return IDX['rock']
    # Honolulu in 1941: the town between the harbour and Diamond Head, low ground only.
    if 4500 < x < 16000 and -9000 < y < -2500 and z < 70:
        return IDX['town']
    if z >= 330 or s > 0.3:
        return IDX['forest']
    # The central plain between the ranges, Wahiawa to the Leilehua plateau: the pineapple fields' red earth,
    # broken by cane where a field lies fallow (a fixed patchwork, so the ground is the same every time).
    return IDX['cane']


def midway_colour(x, y, z, s, d, cell=False):
    if cell:
        return None
    return IDX['sand'] if d < 35 else IDX['scrub']


def piers(stage, L, mesh, zat, cut=()):
    """The piers and quays as slabs two metres above the water, with their sides down to it (not today's piers that
    stand where 1941 ships lay: yard.py's CUT)."""
    for ring in stage['piers']:
        p = Polygon(ring)
        if not p.is_valid:
            p = p.buffer(0)
        for q in polys(p):
            if any(q.centroid.distance(Point(c)) < 20.0 for c in cut):
                continue
            top = 2.0
            flat(mesh, q, top, IDX['concrete'])
            pts = list(q.exterior.coords)
            for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
                a0, b0 = mesh.add(ax, ay, -0.5, IDX['concrete']), mesh.add(bx, by, -0.5, IDX['concrete'])
                a1, b1 = mesh.add(ax, ay, top, IDX['concrete']), mesh.add(bx, by, top, IDX['concrete'])
                mesh.tri(a0, b0, b1)
                mesh.tri(a0, b1, a1)


def runways(fields, mesh, zat, extra, L):
    """The runways as dark strips on the ground, cut to the land (a strip never runs out over the water)."""
    out = []
    for f in fields + extra:
        if not f.get('len'):
            continue
        x, y = f['at']
        h = math.radians(f['runway'])
        u = (math.sin(h), math.cos(h))
        v = (u[1], -u[0])
        half, w = f['len'] / 2, f.get('width', 60) / 2
        z = zat(x, y) + 0.3
        strip = Polygon([(x + u[0] * a + v[0] * b, y + u[1] * a + v[1] * b) for a, b in ((-half, -w), (half, -w), (half, w), (-half, w))]).intersection(L.buffer(10))
        flat(mesh, strip, z, IDX['runway'])
        out.append({'name': f['name'], 'at': [round(x, 1), round(y, 1)], 'heading': f['runway'], 'len': f['len'], 'z': round(z, 1)})
    return out


def hangars(fields, zat, drawn=()):
    """A row of hangars beside each field's runway: big at the bomber fields, fewer at the fighter strips (not where
    the yard draws the field's own, yard.py)."""
    out = []
    for f in fields:
        if f['name'] in drawn:
            continue
        n = {'bombers': 6, 'navy': 3, 'fighters': 4}.get(f['kind'], 2)
        x, y = f['at']
        h = math.radians(f['runway'] or 90)
        u = (math.sin(h), math.cos(h))
        v = (u[1], -u[0])
        side = 170 if f['runway'] else 60
        size = (70, 55, 16) if f['kind'] == 'bombers' else (55, 45, 13)
        for k in range(n):
            a = (k - (n - 1) / 2) * (size[0] + 30)
            hx, hy = x + u[0] * a + v[0] * side, y + u[1] * a + v[1] * side
            out.append([round(hx, 1), round(hy, 1), round(zat(hx, hy), 1), size[0], size[1], size[2], f['runway'] or 90])
    return out


def palms(L, zat, n, seed, near, within=None):
    rnd = random.Random(seed)
    band = L.intersection(L.boundary.buffer(near)) if near else L
    if within is not None:
        band = band.intersection(within)
    P = prep(band)
    minx, miny, maxx, maxy = band.bounds
    out, tries = [], 0
    while len(out) < n and tries < n * 400:
        tries += 1
        x, y = rnd.uniform(minx, maxx), rnd.uniform(miny, maxy)
        if not P.contains(Point(x, y)):
            continue
        z = zat(x, y)
        if z > 30:
            continue
        out.append([round(x, 1), round(y, 1), round(z, 1), round(rnd.uniform(12, 25), 1)])
    return out


def ground_grid(st, L, zat):
    """The height of the ground in whole metres on a coarse grid (0 on the water), for whatever stands on it or
    lands on it in the 3D game: bombs, wrecks, the camera."""
    H = st['heights']
    if H['cols']:
        e0, n0, cell, cols, rows = H['e0'], H['n0'], H['cell'], H['cols'], H['rows']
    else:
        minx, miny, maxx, maxy = L.bounds
        cell = 50.0
        e0, n0 = minx - 200, miny - 200
        cols, rows = int((maxx - minx + 400) // cell) + 1, int((maxy - miny + 400) // cell) + 1
    P = prep(L)
    z = []
    for r in range(rows):
        for c in range(cols):
            x, y = e0 + (c + .5) * cell, n0 + (r + .5) * cell
            z.append(int(round(zat(x, y))) if P.contains(Point(x, y)) else 0)
    return {'e0': e0, 'n0': n0, 'cell': cell, 'cols': cols, 'rows': rows, 'z': z}


def test_ships(key):
    """The record's ships for the 3D game's stage test (-TheaterStage): Pearl Harbor's berths, and the Kido Butai
    in its box off Midway. The battle itself lays its own ships from the campaign's fleets."""
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(here, '..', '..', 'data', 'battle', 'air-stages.json')
    if not os.path.exists(path):
        return None
    S = json.load(open(path))['stages'][key]
    if key == 'oahu':
        ships = [{'cls': b['cls'], 'nat': 'USA', 'name': b['name'] or '', 'at': b['at'], 'face': b['face'], **({'dock': True} if b.get('dock') else {})} for b in S['berths'] if b['cls'] in ('bb', 'ca', 'dd', 'ss', 'ag')]
        return {'aim': S['aim']['harbour'], 'ships': ships}
    ax, ay = S['aim']['fleet']
    J = S['fleets']['JAP']
    ships = []
    for k, n in enumerate(J['cv']):
        ships.append({'cls': 'cv', 'nat': 'JAP', 'name': n, 'at': [ax + (k % 2 - .5) * 2500, ay + (k // 2 - .5) * 2500], 'face': 135})
    for k, n in enumerate(J['bb'] + J['ca']):
        a = 2 * math.pi * k / 5
        ships.append({'cls': 'bb' if n in J['bb'] else 'ca', 'nat': 'JAP', 'name': n, 'at': [ax + 4000 * math.sin(a), ay + 4000 * math.cos(a)], 'face': 135})
    for k in range(8):
        a = 2 * math.pi * (k + .5) / 8
        ships.append({'cls': 'dd', 'nat': 'JAP', 'name': '', 'at': [ax + 6500 * math.sin(a), ay + 6500 * math.cos(a)], 'face': 135})
    return {'aim': [ax, ay], 'ships': ships}


def build(key):
    st = json.load(open(os.path.join(OUT, f'{key}.json')))
    L = unary_union([Polygon(r['outer'], r['holes']).buffer(0) for r in st['land']])
    colour = oahu_colour if key == 'oahu' else midway_colour
    land = ground(st, L, colour)
    # A height on land for anything that stands on it: the nearest point of the land mesh is close enough.
    V = np.array(land.v, dtype=float).reshape(-1, 3) / 10.0

    def zat(x, y):
        if not len(V):
            return 1.0
        i = int(np.argmin((V[:, 0] - x) ** 2 + (V[:, 1] - y) ** 2))
        return max(0.4, V[i, 2])

    # The water painted on the sea a little above its plane: the shallows round the coast, the reef just awash, and
    # (Midway) the lagoon inside the barrier reef. The three never overlap, so none fights another for the eye.
    near = 160 if key == 'oahu' else 260
    R = unary_union([Polygon(r['outer'], r['holes']).buffer(0) for r in st['reef']]).difference(L) if st['reef'] else Polygon()
    band = L.buffer(near, join_style=2).difference(L)
    if key == 'oahu':
        lochs = box(-9000, -2600, 2600, 6500)
        # No reef awash inside the lochs in 1941 (today's mapped shoals there read as mint patches by Ford Island and
        # along Battleship Row: the art director, 19 November 17:45Z): the shallows' band runs on there instead.
        R = R.difference(lochs)
        band = band.difference(lochs).union(L.buffer(35, join_style=2).difference(L).intersection(lochs))
    band = band.difference(R)
    sea = Mesh('shallows')
    flat(sea, band.simplify(8), 0.3, IDX['shallows'])
    reef = Mesh('reef')
    flat(reef, R.simplify(6), 0.3, IDX['reef'])
    if key == 'midway' and not R.is_empty:
        lagoon = unary_union([R, L]).convex_hull.difference(R).difference(L).difference(band)
        flat(reef, lagoon.simplify(6), 0.3, IDX['lagoon'])
    works = Mesh('works')
    piers(st, L, works, zat, CUT.get(key, ()))
    extra = []
    if key == 'midway':
        # Eastern Island's three runways of 1942: the long one down the island's length, two across it in a
        # triangle (the historian checks).
        x, y = st['fields'][0]['at']
        east = min(polys(L), key=lambda p: p.centroid.distance(Point(x, y)))
        rect = list(east.minimum_rotated_rectangle.exterior.coords)
        sides = [(rect[k], rect[k + 1]) for k in range(4)]
        (a, b) = max(sides, key=lambda e: math.dist(*e))
        along = math.degrees(math.atan2(b[0] - a[0], b[1] - a[1])) % 180
        c = east.centroid
        st['fields'][0]['at'] = [round(c.x, 1), round(c.y, 1)]
        st['fields'][0]['runway'] = round(along)
        extra = [{'name': 'Eastern Island, the second', 'at': [c.x - 150, c.y + 40], 'runway': round(along + 50) % 180, 'len': 1300, 'width': 50},
                 {'name': 'Eastern Island, the third', 'at': [c.x + 150, c.y - 40], 'runway': round(along - 50) % 180, 'len': 1300, 'width': 50}]
    strips = runways(st['fields'], works, zat, extra, L)
    out = {
        'key': key, 'name': st['name'], 'origin': st['origin'],
        'units': 'decimetres in v (x east, y north, z up from the sea, from the origin); metres everywhere else',
        'palette': [{'name': n, 'hex': (OAHU_PAINT.get(n, h) if key == 'oahu' else h)} for n, h in PALETTE],
        'meshes': [land.out(), sea.out(), reef.out(), works.out()],
        'tanks': [[t['at'][0], t['at'][1], round(zat(*t['at']), 1), t['r'], round(min(14.0, max(6.0, t['r'] * 0.8)), 1)] for t in st['tanks'] if t['r'] >= 6],
        'hangars': hangars(st['fields'], zat, DRAWN_FIELDS.get(key, set())),
        'yard': yard(key, zat),
        'runways': strips,
        'fields': [{'name': f['name'], 'kind': f['kind'], 'at': f['at']} for f in st['fields']],
        'palms': palms(L, zat, 420, 7, 900) if key == 'oahu' else palms(L, zat, 60, 11, 0),
        'attribution': st['attribution'],
    }
    if key == 'midway':
        # Sand Island's oil tanks (set burning on 4 June): a row of tanks by the seaplane base.
        x, y = st['fields'][1]['at']
        out['tanks'] = [[round(x + 260 + 34 * k, 1), round(y + 210, 1), round(zat(x + 260 + 34 * k, y + 210), 1), 12.0, 9.0] for k in range(6)]
    out['ground'] = ground_grid(st, L, zat)
    # The sun at the hour of the record's attack (azimuth from north, clockwise; elevation): the historian checks.
    out['sun'] = {'azimuth': 125, 'elevation': 18, 'when': '7 December 1941, 07:55'} if key == 'oahu' else {'azimuth': 85, 'elevation': 45, 'when': '4 June 1942, 10:20'}
    out['test'] = test_ships(key)
    path = os.path.join(OUT, f'{key}-3d.json')
    json.dump(out, open(path, 'w'), separators=(',', ':'))
    tri = sum(len(m['t']) // 3 for m in out['meshes'])
    print(key, 'triangles', tri, {m['name']: len(m['t']) // 3 for m in out['meshes']}, 'tanks', len(out['tanks']), 'hangars', len(out['hangars']), 'yard', len(out['yard']), 'palms', len(out['palms']), 'bytes', os.path.getsize(path))


if __name__ == '__main__':
    for k in sys.argv[1:] or ['oahu', 'midway']:
        build(k)
