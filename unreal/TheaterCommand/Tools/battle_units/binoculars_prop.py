"""The tank commander's field glasses as a battle prop the game can draw (release 2).

commander_hatch, die_hatch_slump and die_hatch_drop of the tankers' files carry a `props` track named `binoculars`
(30 a second, in the clip's root frame, the game's axes: t, x, y, z, qw, qx, qy, qz, shown), but the game had no model
to draw there, so he raised empty fists to his eyes. This writes Content/Models/Battle/Binoculars.json from the studio's
crew/props/binoculars.glb in the props' own format (FieldPhone.json's): metres, x forward (the barrels, eyepieces at
the origin), y right, z up; one part; triangles wound with their facings, as the vehicles' and props' are.

    python3 binoculars_prop.py            writes the shared folder's copy and Content/Models/Battle/Binoculars.json
"""
import json
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = "/mnt/project-files/battle-models/land/soldier/crew/props/binoculars.glb"
SHARED = "/mnt/project-files/battle-models/land/soldier/crew/props/Binoculars.json"
CONTENT = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "Models", "Battle"))
LIKE = os.path.join(CONTENT, "FieldPhone.json")
BLACK, GLASS = 10, 9


def read_glb(path):
    b = open(path, "rb").read()
    jl = struct.unpack("<I", b[12:16])[0]
    j = json.loads(b[20:20 + jl])
    bin0 = 20 + jl + 8
    def acc(i):
        a = j["accessors"][i]
        v = j["bufferViews"][a["bufferView"]]
        off = bin0 + v.get("byteOffset", 0) + a.get("byteOffset", 0)
        n = {"SCALAR": 1, "VEC3": 3}[a["type"]]
        dt = {5126: "<f4", 5123: "<u2", 5125: "<u4"}[a["componentType"]]
        return np.frombuffer(b, dtype=dt, count=a["count"] * n, offset=off).reshape(-1, n).astype(float)
    pr = j["meshes"][0]["primitives"][0]
    return acc(pr["attributes"]["POSITION"]), acc(pr["attributes"]["NORMAL"]), acc(pr["indices"]).astype(int).reshape(-1, 3)


def box(lo, hi):
    lo, hi = np.array(lo, float), np.array(hi, float)
    P, F, T = [], [], []
    for ax in range(3):
        for sg in (-1.0, 1.0):
            u, v = [(1, 2), (2, 0), (0, 1)][ax]
            c = [lo, hi][sg > 0][ax]
            quad = []
            for du, dv in ((0, 0), (1, 0), (1, 1), (0, 1)):
                p = np.zeros(3)
                p[ax] = c
                p[u] = [lo, hi][du][u]
                p[v] = [lo, hi][dv][v]
                quad.append(p)
            n = np.zeros(3)
            n[ax] = sg
            k = len(P)
            P += quad
            F += [n] * 4
            tri = [(k, k + 1, k + 2), (k, k + 2, k + 3)]
            for a, b_, c_ in tri:
                if np.cross(quad[(b_ - k)] - quad[(a - k)], quad[(c_ - k)] - quad[(a - k)]) @ n < 0:
                    b_, c_ = c_, b_
                T.append((a, b_, c_))
    return np.array(P), np.array(F), np.array(T)


def level(P, F, T, paints):
    n = len(P)
    return {"points": [round(float(v), 4) for v in P.reshape(-1)], "facings": [round(float(v), 4) for v in F.reshape(-1)],
            "paints": [int(v) for v in paints], "triangles": [int(v) for v in T.reshape(-1)],
            "blend": [0] * n, "soot": [22] * n, "wear": [0] * n, "part": [0] * n}


def main():
    Pg, Ng, T = read_glb(SRC)
    # (glTF: x, y up, z; the game's: x forward, y right, z up. The glasses are the same either side, so y = glTF's z.)
    P = np.stack([Pg[:, 0], Pg[:, 2], Pg[:, 1]], axis=1)
    F = np.stack([Ng[:, 0], Ng[:, 2], Ng[:, 1]], axis=1)
    F /= np.maximum(np.linalg.norm(F, axis=1), 1e-9)[:, None]
    # (wound with the facings, as the props are)
    n = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    flip = np.einsum("ij,ij->i", n, F[T].mean(1)) < 0
    T[flip] = T[flip][:, [0, 2, 1]]
    # (the object lenses: the faces at the barrels' far ends, looking forward)
    paints = np.full(len(P), BLACK)
    paints[(P[:, 0] > P[:, 0].max() - 0.004) & (F[:, 0] > 0.9)] = GLASS
    like = json.load(open(LIKE))
    lo, hi = P.min(0), P.max(0)
    mid_y = 0.5 * (lo[1] + hi[1])
    b1 = box((lo[0], lo[1], lo[2]), (hi[0], mid_y - 0.008, hi[2]))
    b2 = box((lo[0], mid_y + 0.008, lo[2]), (hi[0], hi[1], hi[2]))
    Pf = np.vstack([b1[0], b2[0]])
    Ff = np.vstack([b1[1], b2[1]])
    Tf = np.vstack([b1[2], b2[2] + len(b1[0])])
    D = {"name": "field glasses",
         "about": like["about"].split(" Paints:")[0] + " Paints: " + ", ".join(f"{i} {nm}" for i, nm in enumerate(like["paint_names"])) +
                  ". The tank commander's field glasses (binoculars_prop.py, from the studio's crew/props/binoculars.glb): the origin "
                  "between the eyepieces, the barrels along +x. Drawn where a tankers' clip's `props` track `binoculars` "
                  "puts them (t, x, y, z, qw, qx, qy, qz, shown; the clip's root frame, 30 a second), while `shown` is 1.",
         "kind": "prop", "colours": like["colours"], "paint_names": like["paint_names"],
         "near": level(P, F, T, paints), "far": level(Pf, Ff, Tf, [BLACK] * len(Pf)),
         "parts": [{"name": "glasses", "parent": -1, "pivot": [0.0, 0.0, 0.0], "kind": "root",
                    "note": "the origin between the eyepieces; the game places it by the clip's props track"}],
         "sockets": [], "clips": {}, "controls": {}, "nation": "any", "role": "the tank commander's field glasses",
         "units": "the tank commanders (soldier_USA_tanker, soldier_GER_panzer): commander_hatch and the hatch deaths",
         "long": round(float(hi[0] - lo[0]), 3), "wide": round(float(hi[1] - lo[1]), 3), "high": round(float(hi[2] - lo[2]), 3)}
    for dest in (SHARED, os.path.join(CONTENT, "Binoculars.json")):
        json.dump(D, open(dest + ".tmp", "w"), separators=(",", ":"))
        os.replace(dest + ".tmp", dest)
        print("wrote", dest)
    Tn = np.array(D["near"]["triangles"]).reshape(-1, 3)
    nn = np.cross(P[Tn[:, 1]] - P[Tn[:, 0]], P[Tn[:, 2]] - P[Tn[:, 0]])
    print(f"near {len(P)} points, {len(T)} triangles (wound against their facings: {int((np.einsum('ij,ij->i', nn, F[Tn].mean(1)) < 0).sum())}); "
          f"far {len(Pf)} points, {len(Tf)} triangles; {D['long']} x {D['wide']} x {D['high']} m; glass points {int((paints == GLASS).sum())}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
