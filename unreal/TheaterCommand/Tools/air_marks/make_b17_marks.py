"""The finer B-17's bomber markings (the Battle rules card's "Bomber markings", row flyBomberMarks): copies the marks the
approved picture wore out of the livery file (B17Livery.json, the model thread's, made on her newer shape) into the
game's small data file, Content/Data/AirMarks/b17-fine-marks.json, each part at her two levels (near and far), each a
small vertex-painted mesh in her own file frame (metres x forward, y right, z up; written in millimetres) that the air
scene draws over her as an instanced copy with her own place (Battle/AirLetters, Battle/AirBattleLetters.cpp).

    python3 -I Tools/air_marks/make_b17_marks.py --livery /path/to/B17Livery.json [--bundle 1|2]
        [--model Content/Models/B17Fine.json] [--out Content/Data/AirMarks/b17-fine-marks.json]
        [--wing-at 0.9,11.6] [--wing-size 1.5]

Bundle 1: the group's white triangle and its black letter on her fin (the livery's group.T and group.T.<L>) and the
same on her upper starboard wing (made here: the fin's triangle and letter laid flat, apex forward, the letter reading
the right way round seen from above with her nose up, draped 12 and 16 mm over her wing). Bundle 2 adds her plane's
letter on the fin (yellow) and the side, and the squadron codes on both sides (the default: the game's file). The yellow
serial is not made yet. A re-run writes the same bytes from the same files; bundle 1's parts are the same in bundle 2.

Inks are the livery's own paints (group DCD9CC, group_ink 17171A, serial C99A35, code B9BAB4): matt on PlaneInk. No
date, month or year is read or written: the paint never depends on them.

It refuses unless her model is the one the marks were laid on (sha256 5e1f39002423...), the livery is for the B17 and
its pivots are hers; and a wing point that finds no wing under it. Any glyph point closer than 5 mm to its own level's
skin along its facing is moved out to 10 mm (it prints how many).
"""
import argparse
import hashlib
import json
import sys

import numpy as np

L24 = "ABCDEFGHJKLMNPQRSTUVWXYZ"
FINE_SHA = "5e1f39002423"
INK_OF_PAINT = {"group": "white", "group_ink": "black", "serial": "yellow", "code": "code"}
PIVOTS = ("tail", "rudder", "wing.starboard", "turret.1", "prop.1")
# (the 303rd Bomb Group's triangle C and its four squadrons' codes, 358th to 427th: history, for the historian to confirm)
KNOWN = {"303rd Bomb Group": {"letter": "C", "codes": ["VK", "BN", "PU", "GN"]}}
LIFT_BELOW = 0.005
LIFT_TO = 0.010
WING_LIFT = 0.012
WING_LETTER_LIFT = 0.016
EDGE_MOST = {"near": 0.08, "far": 0.20}  # (the far skin is coarser, and a far copy is small on the screen)


def read(path):
    raw = open(path, "rb").read()
    return json.loads(raw), hashlib.sha256(raw).hexdigest()


def level_of(m):
    P = np.array(m["points"], float).reshape(-1, 3)
    N = np.array(m["facings"], float).reshape(-1, 3)
    T = np.array(m["triangles"], int).reshape(-1, 3)
    return P, N, T


def part_mesh(lv, k):
    """A livery part at one level: its triangles (all three corners in the part), points renumbered in file order."""
    P, N, T = level_of(lv)
    part = np.array(lv["part"])
    paints = np.array(lv["paints"])
    keep = T[(part[T[:, 0]] == k) & (part[T[:, 1]] == k) & (part[T[:, 2]] == k)]
    used = np.unique(keep)
    renum = -np.ones(len(P), int)
    renum[used] = np.arange(len(used))
    return P[used].copy(), N[used].copy(), renum[keep], paints[used]


def skin_of(model, lvl):
    P, N, T = level_of(model[lvl])
    A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    F = -np.cross(B - C, A - C)  # (the files' own way: Cross(B - C, A - C) points into her)
    F /= np.maximum(np.linalg.norm(F, axis=1, keepdims=True), 1e-12)
    return {"P": P, "N": N, "T": T, "A": A, "B": B, "C": C, "F": F,
            "lo": np.minimum(np.minimum(A, B), C), "hi": np.maximum(np.maximum(A, B), C)}


def near_tris(skin, lo, hi, pad):
    return np.where((skin["lo"] <= hi + pad).all(1) & (skin["hi"] >= lo - pad).all(1))[0]


def ray(o, d, A, B, C):
    """Möller-Trumbore against many triangles: (t, u, v, hit)."""
    e1 = B - A
    e2 = C - A
    p = np.cross(d, e2)
    det = (e1 * p).sum(1)
    ok = np.abs(det) > 1e-12
    inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
    s = o - A
    u = (s * p).sum(1) * inv
    q = np.cross(s, e1)
    v = (q * d).sum(1) * inv
    t = (e2 * q).sum(1) * inv
    return t, u, v, ok & (u >= -1e-9) & (v >= -1e-9) & (u + v <= 1 + 1e-9)


def gaps(P, N, skin):
    """Each point's gap to the skin along its facing (metres, + outside), nan where no skin within 20 cm."""
    idx = near_tris(skin, P.min(0), P.max(0), 0.25)
    A, B, C = skin["A"][idx], skin["B"][idx], skin["C"][idx]
    out = np.full(len(P), np.nan)
    for i in range(len(P)):
        t, _, _, hit = ray(P[i], N[i], A, B, C)
        t = t[hit & (np.abs(t) < 0.2)]
        if len(t):
            out[i] = -t[np.argmin(np.abs(t))]
    return out


def closest(p, A, B, C):
    """The nearest point of each triangle to p (Ericson, Real-Time Collision Detection 5.1.5), vectorised."""
    ab, ac, ap = B - A, C - A, p - A
    d1, d2 = (ab * ap).sum(1), (ac * ap).sum(1)
    bp = p - B
    d3, d4 = (ab * bp).sum(1), (ac * bp).sum(1)
    cp = p - C
    d5, d6 = (ab * cp).sum(1), (ac * cp).sum(1)
    va, vb, vc = d3 * d6 - d5 * d4, d5 * d2 - d1 * d6, d1 * d4 - d3 * d2
    den = va + vb + vc
    den = np.where(np.abs(den) < 1e-18, 1e-18, den)
    out = A + ab * (vb / den)[:, None] + ac * (vc / den)[:, None]
    safe = lambda n, d: n / np.where(np.abs(d) < 1e-18, 1e-18, d)
    bc = (va <= 0) & (d4 - d3 >= 0) & (d5 - d6 >= 0)
    out = np.where(bc[:, None], B + (C - B) * safe(d4 - d3, (d4 - d3) + (d5 - d6))[:, None], out)
    acr = (vb <= 0) & (d2 >= 0) & (d6 <= 0)
    out = np.where(acr[:, None], A + ac * safe(d2, d2 - d6)[:, None], out)
    out = np.where(((d6 >= 0) & (d5 <= d6))[:, None], C, out)
    abr = (vc <= 0) & (d1 >= 0) & (d3 <= 0)
    out = np.where(abr[:, None], A + ab * safe(d1, d1 - d3)[:, None], out)
    out = np.where(((d3 >= 0) & (d4 <= d3))[:, None], B, out)
    out = np.where(((d1 <= 0) & (d2 <= 0))[:, None], A, out)
    return out


def nearest_gaps(P, skin):
    """Each point's distance to her nearest face, less than nothing behind it (as the cloud test measures)."""
    idx = near_tris(skin, P.min(0), P.max(0), 0.25)
    A, B, C, F = skin["A"][idx], skin["B"][idx], skin["C"][idx], skin["F"][idx]
    out = np.full(len(P), np.nan)
    way = np.zeros((len(P), 3))
    for i in range(len(P)):
        q = closest(P[i], A, B, C)
        d = ((P[i] - q) ** 2).sum(1)
        j = int(np.argmin(d))
        out[i] = np.sqrt(d[j]) * (1.0 if ((P[i] - q[j]) * F[j]).sum() >= 0 else -1.0)
        way[i] = F[j]
    return out, way


def lift(P, N, skin):
    # (the nearer of its gap along its facing and its distance to her nearest face: a glyph's edge, facing off her
    # skin, can lie deeper than its facing says)
    along = gaps(P, N, skin)
    near, way = nearest_gaps(P, skin)
    P = P.copy()
    # (along its facing where that is the nearer; else straight out of her nearest face, till it stands 10 mm off it)
    first = ~np.isnan(along) & (along < LIFT_BELOW) & ~(near < along)
    P[first] += (LIFT_TO - along[first])[:, None] * N[first]
    second = ~first & (near < LIFT_BELOW)
    P[second] += (LIFT_TO - near[second])[:, None] * way[second]
    g = np.fmin(along, near)
    return P, int(first.sum() + second.sum()), int(np.isnan(along).sum()), g


def wound(P, N, T):
    """The triangles turned, where needed, to the files' own way: Cross(B - C, A - C) against the facing."""
    T = T.copy()
    A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    d = (np.cross(B - C, A - C) * (N[T[:, 0]] + N[T[:, 1]] + N[T[:, 2]])).sum(1)
    flip = d > 0
    T[flip] = T[flip][:, [0, 2, 1]]
    return T


def split(P2, T, most):
    """Triangles in a plane each cut into a grid of n by n smaller ones, n from its longest edge, so that no edge is longer
    than most. Its outline is kept exactly (no point leaves it); where two neighbours take different n their shared
    edge's points do not meet, which draped over her gentle wing leaves a seam far under a millimetre."""
    pts, tris = [], []
    for t in T:
        grid, gt = tri_grid([P2[t[0]], P2[t[1]], P2[t[2]]], most)
        tris += (gt + len(pts)).tolist()
        pts += grid.tolist()
    return np.array(pts), np.array(tris, int)


def hits_down(XY, skin, where):
    """Her upper wing's height under each point: cast down from 3 m onto the wing's upper faces (facing up more than
    0.5); five rays 2 mm apart, the highest hit taken (a ray that slips through a seam of her skin would find something
    inside the wing)."""
    lo = np.array([XY[:, 0].min(), XY[:, 1].min(), -2.0])
    hi = np.array([XY[:, 0].max(), XY[:, 1].max(), 3.0])
    idx = near_tris(skin, lo, hi, 0.05)
    idx = idx[skin["F"][idx, 2] > 0.5]
    A, B, C = skin["A"][idx], skin["B"][idx], skin["C"][idx]
    Z = np.zeros(len(XY))
    for i, (x, y) in enumerate(XY):
        best = None
        for dx, dy in ((0, 0), (0.002, 0), (-0.002, 0), (0, 0.002), (0, -0.002)):
            t, _, _, hit = ray(np.array([x + dx, y + dy, 3.0]), np.array([0.0, 0.0, -1.0]), A, B, C)
            hit &= t > 0
            if hit.any():
                best = t[hit].min() if best is None else min(best, t[hit].min())
        if best is None:
            sys.exit(f"refused: the {where} at ({x:.3f}, {y:.3f}) finds no upper wing under it")
        Z[i] = 3.0 - best
    return Z


def terms(x, y):
    return np.stack([np.ones_like(x), x, y, x * x, x * y, y * y, x ** 3, x * x * y], 1)


def wing_surface(corners2, skin, lift_m, where):
    """A smooth surface through her upper wing under the mark (a small polynomial in x and y, fitted to her skin's
    height on a 2 cm grid, her panel lines' grooves left out), so that the triangle and its letter lie on it parallel,
    over her panel lines and rivets alike. Refuses where her skin stands within 4 mm of the triangle's height."""
    G, _ = tri_grid(corners2, 0.02)
    Z = hits_down(G, skin, where)
    keep = np.ones(len(G), bool)
    for _ in range(4):
        fit, *_ = np.linalg.lstsq(terms(G[keep, 0], G[keep, 1]), Z[keep], rcond=None)
        r = Z - terms(G[:, 0], G[:, 1]) @ fit
        keep = r > -0.002
    up = r.max()
    if up > lift_m - 0.004:
        sys.exit(f"refused: her wing under the {where} stands {up * 1000:.1f} mm over its smooth surface")
    return fit, float(up), float(-r.min())


def drape(XY, fit, lift_m):
    """Points laid lift_m over the wing's smooth surface, facing out of it."""
    x, y = XY[:, 0], XY[:, 1]
    z = terms(x, y) @ fit
    h = 1e-4
    dzdx = (terms(x + h, y) @ fit - terms(x - h, y) @ fit) / (2 * h)
    dzdy = (terms(x, y + h) @ fit - terms(x, y - h) @ fit) / (2 * h)
    N = np.stack([-dzdx, -dzdy, np.ones_like(x)], 1)
    N /= np.linalg.norm(N, axis=1, keepdims=True)
    return np.stack([x, y, z + lift_m], 1), N


def tri_grid(corners2, most):
    """A flat triangle as a grid of small ones, no edge over most."""
    a, b, c = [np.array(p, float) for p in corners2]
    longest = max(np.linalg.norm(a - b), np.linalg.norm(b - c), np.linalg.norm(c - a))
    n = max(1, int(np.ceil(longest / most)))
    pts, at = [], {}
    for i in range(n + 1):
        for j in range(n + 1 - i):
            at[(i, j)] = len(pts)
            pts.append(a + (b - a) * i / n + (c - a) * j / n)
    tris = []
    for i in range(n):
        for j in range(n - i):
            tris.append((at[(i, j)], at[(i + 1, j)], at[(i, j + 1)]))
            if i + j + 1 < n:
                tris.append((at[(i + 1, j)], at[(i + 1, j + 1)], at[(i, j + 1)]))
    return np.array(pts), np.array(tris, int)


def leans(P2, T):
    """A glyph's ink, its area's middle against its box's middle, in its own (right, up): a letter F has more of it
    left and up."""
    A, B, C = P2[T[:, 0]], P2[T[:, 1]], P2[T[:, 2]]
    area = np.abs((B[:, 0] - A[:, 0]) * (C[:, 1] - A[:, 1]) - (C[:, 0] - A[:, 0]) * (B[:, 1] - A[:, 1])) / 2
    cen = ((A + B + C) / 3 * area[:, None]).sum(0) / area.sum()
    box = (P2.min(0) + P2.max(0)) / 2
    return cen - box


def level_out(P, N, T, ink):
    return {"ink": ink,
            "points": [int(round(v)) for v in (P * 1000.0).ravel()],
            "facings": [int(round(v)) for v in (N * 100.0).ravel()],
            "triangles": [int(v) for v in T.ravel()]}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--livery", required=True)
    ap.add_argument("--model", default="Content/Models/B17Fine.json")
    ap.add_argument("--out", default="Content/Data/AirMarks/b17-fine-marks.json")
    ap.add_argument("--bundle", type=int, default=2, choices=(1, 2))
    ap.add_argument("--wing-at", default="0.9,11.6", help="the wing triangle's box middle, x,y metres (y right)")
    ap.add_argument("--wing-size", type=float, default=1.5, help="the wing triangle's width across, metres")
    a = ap.parse_args()

    model, model_sha = read(a.model)
    liv, liv_sha = read(a.livery)
    if not model_sha.startswith(FINE_SHA):
        sys.exit(f"refused: {a.model} is not the shape the marks were laid on (sha256 {model_sha[:12]}, wanted {FINE_SHA})")
    if liv.get("for") != "B17":
        sys.exit("refused: the livery is not for the B17")
    piv_l = {p["name"]: p["pivot"] for p in liv["parts"]}
    piv_m = {p["name"]: p["pivot"] for p in model["parts"]}
    for k in PIVOTS:
        if piv_l.get(k) != piv_m.get(k):
            sys.exit(f"refused: the livery's pivot of {k} is not hers")

    names = [p["name"] for p in liv["parts"]]
    paint_names = liv["paint_names"]
    inks = {INK_OF_PAINT[n]: liv["colours"][paint_names.index(n)] for n in INK_OF_PAINT}
    skins = {lvl: skin_of(model, lvl) for lvl in ("near", "far")}

    wanted = ["group.T"] + [f"group.T.{c}" for c in L24]
    if a.bundle >= 2:
        for c in L24:
            wanted += [f"letter.fin.{c}", f"letter.side.{c}", f"code.port.1.{c}", f"code.port.2.{c}",
                       f"code.starboard.1.{c}", f"code.starboard.2.{c}"]

    parts = {}
    moved_total = {"near": 0, "far": 0}
    for name in wanted:
        k = names.index(name)
        out_name = "fin." + name[len("group."):] if name.startswith("group.") else name
        entry = {}
        for lvl in ("near", "far"):
            if name.startswith("serial.") and lvl == "far":
                continue
            P, N, T, paints = part_mesh(liv[lvl], k)
            if len(T) == 0:
                sys.exit(f"refused: the livery's {name} has no triangles at its {lvl} level")
            ps = set(paints.tolist())
            if len(ps) != 1:
                sys.exit(f"refused: the livery's {name} wears more than one paint")
            ink = INK_OF_PAINT[paint_names[ps.pop()]]
            if name.startswith("letter.side.") and name != "letter.side.A":
                # (a side letter whose tail reaches down past the others' foot, the J's, reaches onto her wing's root
                # there: raised till its foot stands with the A's, and laid again 12 mm off her nearest face)
                AP, _, _, _ = part_mesh(liv[lvl], names.index("letter.side.A"))
                foot = AP[:, 2].min()
                if P[:, 2].min() < foot - 0.03:
                    P = P.copy()
                    P[:, 2] += foot - P[:, 2].min()
                    g, way = nearest_gaps(P, skins[lvl])
                    P += (0.012 - g)[:, None] * way
                    N = way.copy()
                    print(f"  {name} {lvl}: raised to the A's foot and laid again on her side")
            # (again where a point moved out of one face came near another, as in the hollow under her side: at most 4 times)
            moved = 0
            for _ in range(4):
                P, more, missed, _ = lift(P, N, skins[lvl])
                moved += more
                if not more:
                    break
            moved_total[lvl] += moved
            # (a corner still within 3 mm of her skin, or under it, after that: the J's tail where it reaches down onto her
            # wing's root; its triangles left out, at most 5 in a hundred of a part's)
            near_now, _ = nearest_gaps(P, skins[lvl])
            sunk = near_now < 0.003
            if sunk.any():
                keep = ~sunk[T].any(1)
                if (~keep).sum() * 100 > 5 * len(T):
                    sys.exit(f"refused: the livery's {name} at its {lvl} level lies in her skin ({(~keep).sum()} of {len(T)} triangles)")
                print(f"  {name} {lvl}: {(~keep).sum()} of {len(T)} triangles left out, in her skin")
                T = T[keep]
            if missed:
                print(f"  {name} {lvl}: {missed} points found no skin within 20 cm along their facing (left as laid)")
            entry[lvl] = level_out(P, N, T, ink)
        parts[out_name] = entry
    print(f"glyph points moved out to {LIFT_TO * 1000:.0f} mm: near {moved_total['near']}, far {moved_total['far']}")

    # The wing: the fin's triangle and letter laid flat on her upper starboard wing, apex forward. On the fin's
    # starboard face a letter's right is forward (+x) and its up is up (+z); seen from above with her nose up the
    # picture, the starboard wing's right is +y and its up is forward (+x). So wing y = y0 + s (fin x - fin x0) and
    # wing x = x0 + s (fin z - fin z0): the letter reads the right way round from above.
    wx, wy = (float(v) for v in a.wing_at.split(","))
    for lvl in ("near", "far"):
        P, N, T, _ = part_mesh(liv[lvl], names.index("group.T"))
        side = P[:, 1] > 0
        fx, fz = P[side, 0], P[side, 2]
        apex = np.argmax(fz)
        base_z = fz.min()
        low = np.abs(fz - base_z) < 0.03
        corners_fin = [(fx[apex], fz[apex]), (fx[low].min(), base_z), (fx[low].max(), base_z)]
        bx0, bx1 = min(c[0] for c in corners_fin), max(c[0] for c in corners_fin)
        bz0, bz1 = base_z, fz[apex]
        mx, mz = (bx0 + bx1) / 2, (bz0 + bz1) / 2
        s = a.wing_size / (bx1 - bx0)

        def to_wing(x, z):
            return np.stack([wx + s * (np.asarray(z) - mz), wy + s * (np.asarray(x) - mx)], axis=-1)

        cw = [to_wing(x, z) for x, z in corners_fin]
        G, GT = tri_grid(cw, EDGE_MOST[lvl])
        fit, up, groove = wing_surface(cw, skins[lvl], WING_LIFT, f"wing triangle ({lvl})")
        print(f"  wing ({lvl}): her skin within {up * 1000:.1f} mm over the mark's surface and {groove * 1000:.1f} mm under it")
        WP, WN = drape(G, fit, WING_LIFT)
        GT = wound(WP, WN, GT)
        parts.setdefault("wing.T", {})[lvl] = level_out(WP, WN, GT, "white")
        for c in L24:
            # (the letter's far-level glyph at both levels: its near one has a few stray corners sunk under the white at
            # its strokes' ends, hidden on the fin, that would stand out laid flat; the far one is clean, and on the wing,
            # draped and split as finely as the level's triangle, it reads as well)
            LP, LN, LT, _ = part_mesh(liv["far"], names.index(f"group.T.{c}"))
            # (its starboard face alone: the corners facing out of her right side, not its edges' walls)
            # (stray corners the livery left sunk under its triangle's white, hidden on her fin, would stand out of a
            # letter laid flat: counted, and the tool refuses any)
            face = LN[:, 1] > 0.7
            depth = gaps(LP[face], LN[face], skins["far"])
            if np.nanmin(depth) < np.nanmedian(depth) - 0.003:
                sys.exit(f"refused: the livery's far {c} has corners sunk under its face")
            keep = LT[face[LT].all(1)]
            used = np.unique(keep)
            renum = -np.ones(len(LP), int)
            renum[used] = np.arange(len(used))
            flat = to_wing(LP[used, 0], LP[used, 2])
            flat, ST = split(flat, renum[keep], EDGE_MOST[lvl])
            DP, DN = drape(flat, fit, WING_LETTER_LIFT)
            ST = wound(DP, DN, ST)
            parts.setdefault(f"wing.T.{c}", {})[lvl] = level_out(DP, DN, ST, "black")
        if lvl == "near":
            print(f"wing triangle: {a.wing_size:.2f} m across, {s * (bz1 - bz0):.2f} m long, apex forward, its box's middle at x {wx}, y {wy}; "
                  f"the fin's is {bx1 - bx0:.2f} by {bz1 - bz0:.2f} m")

    # Self-check: the letter F reads the right way round on both faces of her fin and on her wing seen from above.
    for lvl in ("near", "far"):
        P, N, T, _ = part_mesh(liv[lvl], names.index("group.T.F"))
        stb = T[(P[T, 1] > 0).all(1)]
        prt = T[(P[T, 1] < 0).all(1)]
        # (starboard face seen from her right: right is +x; port face seen from her left: right is -x)
        ls = leans(np.stack([P[:, 0], P[:, 2]], 1), stb)
        lp = leans(np.stack([-P[:, 0], P[:, 2]], 1), prt)
        W = parts["wing.T.F"][lvl]
        WP = np.array(W["points"], float).reshape(-1, 3) / 1000.0
        WT = np.array(W["triangles"]).reshape(-1, 3)
        # (seen from above, nose up: right is +y, up is +x)
        lw = leans(np.stack([WP[:, 1], WP[:, 0]], 1), WT)
        for what, lean in (("fin, starboard", ls), ("fin, port", lp), ("wing, from above", lw)):
            if not (lean[0] < 0 and lean[1] > 0):
                sys.exit(f"refused: the letter F on her {what} ({lvl}) does not read the right way round (leans {lean})")
    print("the letter F reads the right way round on both faces of her fin and on her wing from above, near and far")

    out = {
        "about": "The finer B-17's bomber markings (the Battle rules card's \"Bomber markings\", flyBomberMarks): each part a "
                 "small mesh in her model file's frame (millimetres, x forward, y right, z up; facings in hundredths), at her "
                 "near and far levels, drawn over her as an instanced copy. Made by Tools/air_marks/make_b17_marks.py from "
                 "the model thread's B17Livery.json; never by date.",
        "model": "B17Fine",
        "model_sha256": model_sha,
        "livery_sha256": liv_sha,
        "bundle": a.bundle,
        "inks": inks,
        "known": KNOWN,
        "wing": {"at": [wx, wy], "size_m": a.wing_size, "apex": "forward", "lift_mm": [WING_LIFT * 1000, WING_LETTER_LIFT * 1000]},
        "parts": parts,
    }
    text = json.dumps(out, sort_keys=True, separators=(",", ":"))
    with open(a.out, "w") as f:
        f.write(text + "\n")
    tris = {lvl: sum(len(p[lvl]["triangles"]) // 3 for p in parts.values() if lvl in p) for lvl in ("near", "far")}
    print(f"wrote {a.out}: bundle {a.bundle}, {len(parts)} parts, {len(text) / 1e6:.2f} MB, triangles near {tris['near']}, far {tris['far']}")


if __name__ == "__main__":
    main()
