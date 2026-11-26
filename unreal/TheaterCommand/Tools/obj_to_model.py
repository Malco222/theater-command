"""obj_to_model.py in.obj in.mtl out_folder : turns the art director's sculpt of the agent piece (her own design, made
from numbers by her script) into the game's own model files (Content/Models/<Name>.json): points, which way each
faces, its paint, and the triangles.

Four files: Agent (the figure on his base with his lantern's frame), AgentLamp (the lantern's panes alone, which the
game lights or leaves dark), AgentMark (the caught mark: the base alone with the lantern set down on its middle) and
AgentMarkLamp (that lantern's panes).

The sculpt is Z up, one unit from the base's top to the hat's crown, its front toward -Y and its lantern at +X. The
game's files have x forward and y to the right. Edges sharper than about forty degrees stay sharp: a point shared by
faces that lean apart by more is kept once for each.
"""
import json
import math
import sys

obj_path, mtl_path, out_folder = sys.argv[1:4]
CREASE = math.cos(math.radians(40.0))

colours = {}
name = None
for line in open(mtl_path):
    bits = line.split()
    if not bits:
        continue
    if bits[0] == 'newmtl':
        name = bits[1]
    elif bits[0] == 'Kd' and name:
        colours[name] = tuple(max(0, min(255, round(float(v) * 255))) for v in bits[1:4])
paint_names = sorted(colours)  # (m00 to m15, in the sculptor's own order)

verts = []
parts = []  # (name, paint index, [faces as vertex index triples])
for line in open(obj_path):
    bits = line.split()
    if not bits or bits[0].startswith('#'):
        continue
    if bits[0] == 'v':
        verts.append(tuple(float(v) for v in bits[1:4]))
    elif bits[0] == 'o':
        parts.append([bits[1], 0, []])
    elif bits[0] == 'usemtl':
        parts[-1][1] = paint_names.index(bits[1])
    elif bits[0] == 'f':
        idx = [int(b.split('/')[0]) - 1 for b in bits[1:]]
        for k in range(1, len(idx) - 1):
            parts[-1][2].append((idx[0], idx[k], idx[k + 1]))


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit(a):
    n = math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])
    return (a[0] / n, a[1] / n, a[2] / n) if n > 1e-12 else (0.0, 0.0, 1.0)


def turned(p):
    # (The sculpt's front is -Y and its lantern is at +X, in a right-handed frame: seen from the front the lantern is
    # on the right, in his left hand. The game's files have x forward and y to the figure's own right.)
    return (-p[1], -p[0], p[2])


def build(name, about, keep, shift=None):
    points, facings, paints, triangles = [], [], [], []
    for part_name, paint, faces in parts:
        if not keep(part_name):
            continue
        move = shift(part_name) if shift else (0.0, 0.0, 0.0)
        normals = [unit(cross(sub(verts[b], verts[a]), sub(verts[c], verts[a]))) for a, b, c in faces]
        around = {}
        for f, tri in enumerate(faces):
            for v in tri:
                around.setdefault(v, []).append(f)
        welded = {}
        for f, tri in enumerate(faces):
            out = []
            for v in tri:
                n = [0.0, 0.0, 0.0]
                for g in around[v]:
                    if sum(normals[f][k] * normals[g][k] for k in range(3)) >= CREASE:
                        for k in range(3):
                            n[k] += normals[g][k]
                n = unit(n)
                key = (v, round(n[0], 3), round(n[1], 3), round(n[2], 3))
                if key not in welded:
                    welded[key] = len(paints)
                    p = turned(tuple(verts[v][k] + move[k] for k in range(3)))
                    q = turned(n)
                    points.extend(round(x, 5) for x in p)
                    facings.extend(round(x, 4) for x in q)
                    paints.append(paint)
                out.append(welded[key])
            triangles.extend(out)
    body = {'points': points, 'facings': facings, 'paints': paints, 'triangles': triangles}
    model = {
        'name': name, 'about': about,
        'long': round(max(points[0::3]) - min(points[0::3]), 4), 'high': round(max(points[2::3]), 4),
        # (a piece: its paints are the file's own, it takes no dust of the road, no markings and no wear)
        'kind': 'piece',
        'colours': ['%02X%02X%02X' % colours[n] for n in paint_names],
        'near': body,
    }
    json.dump(model, open('%s/%s.json' % (out_folder, name), 'w'), separators=(',', ':'))
    print('%s: %d points, %d triangles; height %.3f, lowest %.3f' % (name, len(paints), len(triangles) // 3, max(points[2::3]), min(points[2::3])))


SCULPT = "The art director's own sculpt of the agent piece (agent-piece.obj, from her script spy3d.py), turned into the game's model file by Tools/obj_to_model.py. One unit is the figure, from the base's top to the hat's crown."
glass = lambda n: n == 'lantern_glass'
lantern = lambda n: n.startswith('lantern')
base = lambda n: n.startswith('base')
# (the lantern's foot stands at 0.230 over (0.264, -0.123): on the caught mark it is set down on the middle of the base)
down = lambda n: (-0.264, 0.123, -0.230) if lantern(n) else (0.0, 0.0, 0.0)
build('Agent', SCULPT + ' A faceless figure in a raincoat and hat with a case and a lantern, on a brass-rimmed base; the lantern\'s panes are a file of their own.', lambda n: not glass(n))
build('AgentLamp', SCULPT + ' The panes of his lantern alone: the game lights them or leaves them dark.', glass)
build('AgentMark', SCULPT + ' The mark left for a month where an agent was caught: the base alone, with the lantern set down on it.', lambda n: base(n) or (lantern(n) and not glass(n)), down)
build('AgentMarkLamp', SCULPT + ' The panes of the lantern on the caught mark.', glass, down)
print('paints:', ', '.join('%d %s' % (i, n) for i, n in enumerate(paint_names)))
