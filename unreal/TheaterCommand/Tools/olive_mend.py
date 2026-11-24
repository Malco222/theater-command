#!/usr/bin/env python3
"""The olive tree's mend (Content/Models/Med/OliveTree.json; Brian, 24 November, the Algiers battle: "the tree trunk
looks weird"). The model's maker (tools/battle-models/med/scenery.py) needs Blender, so this mends its file in place:

- the trunk: a fat flat-topped stump (0.34 m across its top) with the limbs, half as thick, pushed into its lid, so it
  read as a flowerpot; now it flares into roots at the ground and narrows up into its limbs (0.21 m at its top, its lid
  drawn up into a low cone between them);
- the crown's clumps: smooth eggs; now each a fifth smaller and lumpy, so the leaves round them make the crown's edge.

Its near and middle levels both. Run once: the file says it was mended (its "about") and a second run does nothing.
Usage: Tools/olive_mend.py [OliveTree.json]
"""
import json, math, os, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "Content", "Models", "Med", "OliveTree.json")
MARK = " Mended by Tools/olive_mend.py (24 November): the trunk flares at its foot and narrows into its limbs; the crown's clumps smaller and lumpy."

# the trunk's radius by height, as made (scenery.py's lathe) and as it should be
WAS = [(-0.1, 0.42), (0.15, 0.34), (0.8, 0.30), (1.1, 0.34)]
NOW = [(-0.1, 0.50), (0.15, 0.34), (0.8, 0.25), (1.1, 0.21)]
LID = 0.12      # the lid's middle drawn up this much
CLUMP = 0.78    # each clump this much of its size
LUMP = 0.32     # and lumpy by this much


def at(table, z):
    if z <= table[0][0]:
        return table[0][1]
    for (z0, r0), (z1, r1) in zip(table, table[1:]):
        if z <= z1:
            return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    return table[-1][1]


def parts(m):
    P, T = m["points"], m["triangles"]
    n = len(P) // 3
    key, wid = {}, [0] * n
    for v in range(n):
        wid[v] = key.setdefault((round(P[3 * v], 4), round(P[3 * v + 1], 4), round(P[3 * v + 2], 4)), len(key))
    par = list(range(len(key)))

    def f(a):
        while par[a] != a:
            par[a] = par[par[a]]
            a = par[a]
        return a
    for i in range(0, len(T), 3):
        a, b, c = f(wid[T[i]]), f(wid[T[i + 1]]), f(wid[T[i + 2]])
        par[b] = a
        par[f(c)] = a
    out = collections.defaultdict(list)
    for v in range(n):
        out[f(wid[v])].append(v)
    return list(out.values()), wid


def renormal(m, verts, wid):
    """Smooth normals over these points again, from their triangles, each kept to its old side."""
    P, T, N = m["points"], m["triangles"], m["facings"]
    inside = set(verts)
    acc = collections.defaultdict(lambda: [0.0, 0.0, 0.0])
    for i in range(0, len(T), 3):
        a, b, c = T[i], T[i + 1], T[i + 2]
        if a not in inside:
            continue
        ax, ay, az = P[3 * a:3 * a + 3]
        bx, by, bz = P[3 * b:3 * b + 3]
        cx, cy, cz = P[3 * c:3 * c + 3]
        ux, uy, uz, vx, vy, vz = bx - ax, by - ay, bz - az, cx - ax, cy - ay, cz - az
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        for v in (a, b, c):
            s = acc[wid[v]]
            s[0] += nx
            s[1] += ny
            s[2] += nz
    for v in verts:
        s = acc[wid[v]]
        L = math.sqrt(s[0] ** 2 + s[1] ** 2 + s[2] ** 2)
        if L < 1e-12:
            continue
        nx, ny, nz = s[0] / L, s[1] / L, s[2] / L
        if nx * N[3 * v] + ny * N[3 * v + 1] + nz * N[3 * v + 2] < 0.0:
            nx, ny, nz = -nx, -ny, -nz
        N[3 * v:3 * v + 3] = [round(nx, 4), round(ny, 4), round(nz, 4)]


def mend(m):
    P, Pa = m["points"], m["paints"]
    cs, wid = parts(m)
    trunk = min(cs, key=lambda c: min(P[3 * v + 2] for v in c))
    for v in trunk:
        x, y, z = P[3 * v:3 * v + 3]
        r = math.hypot(x, y)
        if r < 0.02:
            if z > 1.0:
                P[3 * v + 2] = round(z + LID, 4)
            continue
        k = at(NOW, z) / at(WAS, z)
        P[3 * v] = round(x * k, 4)
        P[3 * v + 1] = round(y * k, 4)
    renormal(m, trunk, wid)
    clumps = [c for c in cs if Pa[c[0]] != 0 and len(c) > 4]
    for j, c in enumerate(clumps):
        mx = sum(P[3 * v] for v in c) / len(c)
        my = sum(P[3 * v + 1] for v in c) / len(c)
        mz = sum(P[3 * v + 2] for v in c) / len(c)
        a, b, e = 1.7 * j, 2.9 * j + 1.0, 4.3 * j + 2.0
        for v in c:
            dx, dy, dz = P[3 * v] - mx, P[3 * v + 1] - my, P[3 * v + 2] - mz
            L = math.sqrt(dx * dx + dy * dy + dz * dz) or 1.0
            ux, uy, uz = dx / L, dy / L, dz / L
            n = 0.5 * math.sin(5.0 * ux + a) + 0.35 * math.sin(6.0 * uy + b) + 0.4 * math.sin(4.5 * uz + e)
            k = CLUMP * (1.0 + LUMP * max(-1.0, min(1.0, n)))
            P[3 * v:3 * v + 3] = [round(mx + dx * k, 4), round(my + dy * k, 4), round(mz + dz * k, 4)]
        renormal(m, c, wid)
    return len(trunk), len(clumps)


def main():
    with open(PATH) as fh:
        d = json.load(fh)
    if MARK.strip() in d.get("about", ""):
        print("olive tree: mended already")
        return
    for lod in ("near", "mid"):
        t, c = mend(d[lod])
        print(f"olive tree {lod}: trunk {t} points, {c} clumps")
    d["about"] = d.get("about", "") + MARK
    with open(PATH, "w") as fh:
        json.dump(d, fh, separators=(",", ":"))


if __name__ == "__main__":
    main()
