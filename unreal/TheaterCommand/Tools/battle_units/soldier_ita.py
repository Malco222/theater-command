"""The Italian rifleman of the Mediterranean battles, made beside soldier.py (which stays as it is), as soldier_sov.py
makes the Soviet man.

    python soldier_ita.py                     build, rig, animate and export the Italian man, then his picture
    python soldier_ita.py --only export       (steps: export, picture; --fast for a small test picture)
    python soldier_ita.py --dress summer,desert

He is soldier.py's man in every bone and clip: the same MakeHuman body, skeleton, mocap and weapon work, so the game
plays him with the German's clips. What is his own is laid on in this file, by wrapping four of soldier.py's makers
before `assemble` calls them (the art director's calls, 2026-11-19: the army's own marks only):

- the grey-green tunic of 1937 worn open at the neck: lapels over a shirt and tie (the shirt's collar is soldier.py's
  collar band), a short row of four buttons from the lapels' foot to the belt, its four pockets as soldier.py makes
  them; on each lapel's point a collar patch with the army's five-pointed star, pale (never black: black two-pointed
  flames were the Blackshirts' mark). The game paints a man from one fixed table of paints, so the patch takes the
  helmet's paint and the star the skin's (the only pale one); the infantry's red patch and a white-metal star wait
  for the game to read a soldier file's own "colours" (after release 1);
- breeches full in the thigh and bloused just under the knee over puttees, the wool band wound up the shin from the
  ankle boots to the knee in a spiral, each turn's lower edge lapped over the one below like a shingle (its edge a
  rolled fold in shade, not a gap);
- his kit: belt, bread bag, water bottle and entrenching tool as soldier.py makes them; no Y-straps; two leather
  cartridge pouches on the belt, one either side of the buckle;
- the steel helmet of 1933: a deep round dome with a short peak, low over the ears, with the boss on its crown.

And the desert dress on the same man, skeleton and clips (`--dress desert`): the sahariana in khaki, its breast
pockets' flaps running up over the shoulders as a yoke, worn open without a tie; khaki breeches and puttees; the
helmet painted sand. Its file's json carries its colours under "colours", as the Soviet winter dress does.

No party marks anywhere: no fasces, no MVSN. Everything is our own shape. Files go to battle-models/land/soldier/ as
soldier_ITA*.glb and soldier_ITA.json (soldier_ITA_desert* for the desert dress).
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import soldier as S  # noqa: E402  (imports bpy first)
import bpy  # noqa: E402
import numpy as np  # noqa: E402
import kit  # noqa: E402
from sculpt import HalfSpace, Layer, RoundBox, Torus, frame_from, on_surface  # noqa: E402

NATION = "ITA"

# ---- his colours in the pictures (studio only: the game colours him by paint number)
S.LOOK[NATION] = {
    "tunic": "73806C",      # grey-green (the art director's)
    "trousers": "6C7865",   # the breeches and the shirt, a shade darker
    "boots": "3B2B20",      # brown ankle boots
    "lower": "68715E",      # the puttees' wool
    "straps": "5A553F",     # grey-green leather: belt and pouches
    "helmet": "5C6553",     # the helmet's grey-green, and the collar patches
    "pack": "767460",       # canvas bread bag and shovel carrier
    "skin": "BDA08E",
    "wood": "6B4429",
    "steel": "34363A",
    "eye": "2A2420",
    "leather": "3A2E25",
    "blanket": "665F4C",    # the water bottle's felt
}
S.NATION_SEED[NATION] = 23          # (his face: one of MakeHuman's mixes no other army's file uses)
import human  # noqa: E402
human.NATION_MIX[NATION] = (0.86, 0.08, 0.06)          # (a Mediterranean man's mix of MakeHuman's three bases)
S.NATION_KIT[NATION] = {"helmet": "m33", "tall_boots": False, "pack": False, "roll": False, "canister": False, "chest_pouches": False}

# ---- the desert dress: the same man (face, body, skeleton) in the sahariana. dress: (kit tag, file suffix)
DRESSES = {"summer": (NATION, ""), "desert": ("ITAD", "_desert")}
S.NATION_KIT["ITAD"] = dict(S.NATION_KIT[NATION], dress="desert")
S.NATION_SEED["ITAD"] = S.NATION_SEED[NATION]
S.LOOK["ITAD"] = dict(S.LOOK[NATION], tunic="B8A57A", trousers="AE9A70", lower="A6936B", straps="6E5A3E", helmet="B39E73", pack="A69777",
                      boots="4A3626", blanket="8A7A5C")

# ---- the helmet of 1933: deep, round, a short peak ahead, low over the ears and a little flared behind
S.HELMETS["m33"] = dict(ax=0.121, ay=0.111, az=0.114, dx=-0.004, edge=S._cos3(0.026, -0.026, -0.034), flare=S._cos3(0.03, 0.011, 0.022),
                        drop=S._cos3(-0.003, 0.011, 0.013))

BIG, OFF = 3.0, -0.06
LM_NOW = {}                         # (the landmarks of the man being dressed: the kit lays the puttees' turns by them)
PUTTEE = (0.075, 0.0045)             # (each turn's rise up the shin, and how far its lapped lower edge stands off the one below)
MARKS = ("helmet", "skin")           # (the paints his collar patch and star take: see the top of this file)


def _ours(spec):
    """Whether a kit is this man's (either dress)."""
    return spec is S.NATION_KIT[NATION] or spec.get("dress") == "desert"


_mh_body = S.mh_body


def mh_body(nation, seed=None, name=None):
    return _mh_body(NATION if nation == "ITAD" else nation, seed, name)                     # (the same man in either dress)


S.mh_body = mh_body

S.SCRATCH = "/home/user/art/soldier_ita/"
S.STAGE = os.path.join(S.SCRATCH, "out")


# ================================================================================================ the uniform
_uniform_layers = S.uniform_layers
UNIFORM_BUDGET = S.BUDGET["uniform"]


def uniform_layers(man, J, spec, LM):
    layers = _uniform_layers(man, J, spec, LM)
    if not _ours(spec):
        return layers
    tunic, trousers = layers[0], layers[1]
    lower = next(L for L in layers if L.name == "gaiters")
    layers.remove(next(L for L in layers if L.name == "gaiter_straps"))
    lower.name = "puttees"
    Z = LM["Z"]
    # ---- off: the button strip from the collar to the hem (his tunic is open at the neck; its four buttons below)
    keep = []
    for how, p, k in tunic.ops:
        if how == "add" and isinstance(p, RoundBox) and abs(p.half[0] - 0.006) < 1e-6 and abs(p.half[1] - 0.014) < 1e-6 and p.half[2] > 0.08:
            continue
        keep.append((how, p, k))
    tunic.ops = keep
    v_foot = Z(1.29)
    p, nrm = on_surface(tunic, (0.4, 0.0, (v_foot + LM["hem"]) / 2), (-1.0, 0.0, 0.0))
    half = (v_foot - LM["hem"]) / 2
    tunic.add(RoundBox(p - nrm * 0.004, (0.006, 0.014, half), 0.005, frame_from(nrm, (0.0, 0.0, 1.0))), 0.008)
    LM["v_foot"] = v_foot
    # ---- the breeches: full in the thigh and over the knee, drawn in just under it, bloused over the puttees' tops
    tights = S.ShellField(man["tights"])
    tuck = LM["knee"] - 0.075
    trousers.ops = []
    trousers.add(tights.cloth({"pelvis": 0.012, "spine_01": 0.012, "thigh_l": 0.021, "thigh_r": 0.021, "calf_l": 0.016, "calf_r": 0.016},
                              lo=(-BIG, -BIG, tuck - 0.03), hi=(BIG, BIG, LM["belt"] + 0.04), default=OFF,
                              more=lambda P: -0.016 * np.clip((tuck + 0.012 - P[:, 2]) / 0.03, 0.0, 1.0)))
    for s, side in (("l", 1.0), ("r", -1.0)):
        knee, ank = np.array(J["calf_" + s]), np.array(J["foot_" + s])
        c = S.leg_point(J, s, tuck + 0.02)
        Rs = S.ring_frame(knee - ank)
        rx, ry = S.ring_axes(trousers, c, Rs)
        trousers.add(Torus(c, rx - 0.002, 0.019, Rs @ S.rot_euler(0, -5, 0), major_y=ry - 0.002), 0.02)
    # ---- the puttees: the wound band itself is the kit's (_wind); here the wool under it, 3 mm below the band's
    # face so no turn sinks into it, from just above the boot's top (the band covers its ragged foot and the boot's
    # top by 1.5 cm, standing 4 mm clear of the boot) to under the breeches' blousing
    lower.ops = []
    lower.add(tights.cloth({"calf_l": 0.013, "calf_r": 0.013, "foot_l": 0.013, "foot_r": 0.013}, lo=(-BIG, -BIG, LM["ankle"] + 0.04),
                           hi=(BIG, BIG, tuck + 0.03), default=OFF))
    lower.cut(HalfSpace((0.0, 0.0, tuck + 0.012), (0.0, 0.0, 1.0)), 0.005)
    lower.cut(HalfSpace((0.0, 0.0, LM["ankle"] + 0.05), (0.0, 0.0, -1.0)), 0.006)
    face = Layer("puttee_face", "lower")                       # (the band's face, uncut: the turns are laid on it)
    face.add(tights.cloth({"calf_l": 0.016, "calf_r": 0.016, "foot_l": 0.016, "foot_r": 0.016}, lo=(-BIG, -BIG, LM["ankle"] - 0.02),
                          hi=(BIG, BIG, tuck + 0.05), default=OFF))
    LM["puttee_face"] = face
    LM["tuck"] = tuck
    LM_NOW.clear()
    LM_NOW.update(LM)
    print(f"  ITA: open neck, breeches bloused at {tuck:.2f} m over puttees ({PUTTEE[0] * 100:.1f} cm turns)")
    return layers


S.uniform_layers = uniform_layers


# ================================================================================================ the kit
_build_kit = S.build_kit


def _plate(g, L, corners, lift, thick, paint, n=6):
    """A thin plate lying on the layer's front: a triangle (or a four-cornered piece, as two) given by its corners
    (y, z), each point of it found on the cloth by a ray from in front and lifted along the cloth's normal."""
    if len(corners) == 4:
        _plate(g, L, corners[:3], lift, thick, paint, n)
        _plate(g, L, [corners[0], corners[2], corners[3]], lift, thick, paint, n)
        return
    A, B, C = (np.array(c, float) for c in corners)
    idx, P, N = {}, [], []
    for i in range(n + 1):
        for j in range(n + 1 - i):
            y, z = A + (B - A) * i / n + (C - A) * j / n
            p, nrm = on_surface(L, (0.4, y, z), (-1.0, 0.0, 0.0))
            idx[(i, j)] = len(P)
            P.append(p)
            N.append(nrm)
    P, N = np.array(P), np.array(N)
    k = len(P)
    verts = list(P + N * (lift + thick)) + list(P + N * lift)
    out = N.mean(axis=0)
    faces = []
    for (i, j), a in idx.items():
        tris = []
        if (i + 1, j) in idx and (i, j + 1) in idx:
            tris.append((a, idx[(i + 1, j)], idx[(i, j + 1)]))
        if (i + 1, j) in idx and (i + 1, j + 1) in idx and (i, j + 1) in idx:
            tris.append((idx[(i + 1, j)], idx[(i + 1, j + 1)], idx[(i, j + 1)]))
        for t in tris:
            v0, v1, v2 = (np.array(verts[q]) for q in t)
            if np.cross(v1 - v0, v2 - v0) @ out < 0:
                t = (t[0], t[2], t[1])
            faces.append(t)
            faces.append((t[0] + k, t[2] + k, t[1] + k))
    rim = [idx[(i, 0)] for i in range(n + 1)] + [idx[(n - j, j)] for j in range(1, n + 1)] + [idx[(0, j)] for j in range(n - 1, 0, -1)]
    mid = P.mean(axis=0)
    for a, b in zip(rim, rim[1:] + rim[:1]):
        f = (a, b, b + k, a + k)
        v0, v1, v3 = np.array(verts[a]), np.array(verts[b]), np.array(verts[a + k])
        if np.cross(v1 - v0, v3 - v0) @ ((v0 + v1) / 2 - mid) < 0:
            f = (a, a + k, b + k, b)
        faces.append(f)
    g._add([tuple(v) for v in verts], faces, paint)


# the wound band's section from its lower edge (0) to its upper (1): how far it stands off the cloth, as a share of
# PUTTEE[1]. Its lower edge rolls down onto the turn below (the fold, in shade); its upper part lies flat under the next
# turn, which stands clear of it all along the lap.
WIND = ((-0.03, 0.0), (0.0, 0.5), (0.08, 1.0), (1.0, 0.05))
LAP = 0.2                           # (the share of each turn's width the next turn covers)


def _wind(g, L, J, s_, side, zb, zt, per_turn=14):
    """One leg's puttee: a band wound from zb (its lowest turn's edge flat at the boot's top) to zt in a spiral, each
    turn's lower edge lapped over the turn below. The band's points are found on the puttee layer and stood off it."""
    rise, thick = PUTTEE
    w = rise / (1.0 - LAP)
    a_end = 2.0 * math.pi * (zt - zb + rise) / rise
    A = np.arange(0.0, a_end + 1e-9, 2.0 * math.pi / per_turn)
    rows = []
    for a_ in A:
        z0 = zb - rise + rise * a_ / (2.0 * math.pi)       # (this turn's lower edge here, before it is held to zb..zt)
        way = np.array((math.cos(a_), side * math.sin(a_), 0.0))
        row = []
        for s, lift in WIND:
            z = min(max(z0 + s * w, zb), zt)
            row.append((S.leg_point(J, s_, z) + way * 0.075, lift))
        rows.append(row)
    flat = [p for row in rows for p, _ in row]
    P, N = S.project(L, flat)
    lift = np.array([lf for row in rows for _, lf in row]) * thick + 0.0004
    V = P + N * lift[:, None]
    k = len(WIND)
    faces = []
    for i in range(len(rows) - 1):
        for j in range(k - 1):
            a, b, c, d = i * k + j, (i + 1) * k + j, (i + 1) * k + j + 1, i * k + j + 1
            faces.append((a, b, c, d) if side > 0 else (a, d, c, b))
    # (each quad faced outward: checked against the normal at its first corner)
    out = []
    for f in faces:
        v0, v1, v2, v3 = (V[q] for q in f)
        if np.linalg.norm(np.cross(v2 - v0, v3 - v1)) < 1e-9:         # (none where the band is held flat at its ends)
            continue
        if np.cross(v1 - v0, v3 - v0) @ N[f[0]] < 0:
            f = (f[0], f[3], f[2], f[1])
        out.append(f)
    g._add([tuple(v) for v in V], out, "lower")


def _star(g, p, nrm, up, r, paint="steel"):
    """A small five-pointed star lying on the cloth at p: five rays from its middle."""
    R = frame_from(nrm, up)
    for k in range(5):
        a = math.radians(90.0 + 72.0 * k)
        ray = R @ np.array((0.0, math.cos(a), math.sin(a)))
        g.box(p + nrm * 0.0012 + ray * r * 0.5, (0.0009, r * 0.17, r * 0.5), paint, frame_from(nrm, ray))


def build_kit(J, spec, body_ls, Z=None):
    if not _ours(spec):
        return _build_kit(J, spec, body_ls, Z)
    out = _build_kit(J, spec, body_ls, Z)
    Z = Z or (lambda z: z)
    tunic = body_ls[0]
    desert = spec.get("dress") == "desert"
    for name in ("straps", "details", "pouches"):           # (no Y-straps; his own buttons, collar and pouches below)
        ob = out.pop(name, (None, None))[0]
        if ob is not None:
            bpy.data.objects.remove(ob)
    g = S.Geo()
    # ---- the open neck: the shirt in the V, the tie (not in the desert), and the lapels over them, each a thin plate
    # laid on the tunic's front (its points found by rays from in front of him)
    top, foot = Z(1.43), Z(1.29)
    _plate(g, tunic, [(-0.028, top + 0.004), (0.028, top + 0.004), (0.0, foot - 0.004)], 0.0012, 0.0012, "trousers", 4)
    if not desert:
        _plate(g, tunic, [(-0.0065, top - 0.01), (0.0065, top - 0.01), (0.0085, foot + 0.022), (-0.0085, foot + 0.022)], 0.0026, 0.0014, "lower", 3)
        p, nrm = on_surface(tunic, (0.4, 0.0, top - 0.009), (-1.0, 0.0, 0.0))
        g.box(p + nrm * 0.0055, (0.003, 0.0085, 0.0065), "lower", frame_from(nrm, (0.0, 0.0, 1.0)))     # (the knot)
    for side in (1.0, -1.0):
        _plate(g, tunic, [(0.026 * side, top + 0.005), (0.074 * side, top - 0.006), (0.0, foot - 0.006)], 0.0032, 0.003, "tunic", 4)
        # the collar patch along the lapel's outer edge, the star at its lower end
        _plate(g, tunic, [(0.042 * side, top - 0.002), (0.07 * side, top - 0.008), (0.058 * side, top - 0.05), (0.037 * side, top - 0.04)],
               0.0064, 0.0012, MARKS[0], 2)
        p, nrm = on_surface(tunic, (0.4, 0.05 * side, top - 0.035), (-1.0, 0.0, 0.0))
        _star(g, p + nrm * 0.0066, nrm, (0.0, 0.0, 1.0), 0.0075, MARKS[1])
    # ---- the four buttons from the lapels' foot to the belt, and one on each pocket's flap
    spots = [((0.3, 0.0, z), (-1.0, 0.0, 0.0)) for z in np.linspace(foot - 0.012, Z(1.045), 4)]
    for side in (1.0, -1.0):
        spots += [((0.3, 0.078 * side, Z(1.336)), (-1.0, -0.25 * side, 0.0)), ((0.3, 0.105 * side, Z(0.918)), (-1.0, -0.25 * side, 0.0))]
    for start, way in spots:
        p, nrm = on_surface(tunic, start, way)
        g.tube(p - nrm * 0.001, p + nrm * 0.0035, 0.0082, 0.007, "steel", 6)
    if desert:
        # the sahariana's yoke: each breast pocket's flap carried on up over the shoulder and down the back a hand's width
        for side in (1.0, -1.0):
            ctrl = [(0.14, 0.08 * side, Z(1.36)), (0.12, 0.095 * side, Z(1.415)), (0.06, 0.11 * side, Z(1.462)), (-0.03, 0.112 * side, Z(1.462)),
                    (-0.1, 0.1 * side, Z(1.415)), (-0.13, 0.09 * side, Z(1.36))]
            first = len(g.V)
            S.strap_on(g, tunic, S.spline(ctrl, 18), 0.07, 0.003, paint="tunic", gap=0.0026)
            # (over the shoulder by the collar the tunic moves with the neck: there the yoke stands 6 mm clearer)
            P = np.array(g.V[first:])
            _, Nn = S.project(tunic, P)
            P += Nn * (0.006 * np.clip((P[:, 2] - Z(1.4)) / 0.04, 0.0, 1.0))[:, None]
            g.V[first:] = [tuple(v) for v in P]
    out["details"] = (g.build("details"), None)
    if LM_NOW.get("puttee_face") is not None:
        gw = S.Geo()
        for s_, side in (("l", 1.0), ("r", -1.0)):
            _wind(gw, LM_NOW["puttee_face"], J, s_, side, LM_NOW["ankle"] + 0.03, LM_NOW["tuck"] + 0.006)
        out["puttee_wraps"] = (gw.build("puttee_wraps"), None)
    # ---- the two cartridge pouches on the belt, one either side of the buckle, each with its rounded lid
    from sculpt import Layer
    L = Layer("pouches", "straps")
    zb = Z(1.0)
    cx = J["spine_01"][0] + 0.03
    for side in (1.0, -1.0):
        a = math.atan2(0.1 * side, 0.12)
        p0, n0 = S.project(tunic, [(cx + math.cos(a) * 0.2, math.sin(a) * 0.2, zb - 0.008)])
        p0, n0 = p0[0], n0[0]
        R = frame_from(n0, (0.0, 0.0, 1.0))
        c = p0 + n0 * 0.03
        L.add(RoundBox(c, (0.024, 0.05, 0.042), 0.01, R), 0.0)
        L.add(RoundBox(c + R @ np.array((0.005, 0.0, 0.03)), (0.022, 0.052, 0.018), 0.009, R), 0.003)
    out["pouches"] = (S.mesh_from_layers("pouches", [L], 0.0025, 200), None)
    return out


S.build_kit = build_kit
S.LOD2_LEFT_OFF = S.LOD2_LEFT_OFF + ("puttee_wraps",)
_skin_like = S.skin_like


def skin_like(piece, body, k=4):
    """The wound band takes the weights of the wool it lies on (the uniform's puttee layer, which takes the shin's),
    never those of the boot below it, so its lowest turns ride the shin and not the foot."""
    if not piece.name.startswith("puttee_wraps"):
        return _skin_like(piece, body, k)
    from mathutils import kdtree
    W, names = S.weights_of(body)
    mats = {i for i, m_ in enumerate(body.data.materials) if m_ is not None and m_.name.split(".")[0] == "lower"}
    on = sorted({v for p_ in body.data.polygons if p_.material_index in mats for v in p_.vertices})
    kd = kdtree.KDTree(len(on))
    for v in on:
        kd.insert(body.data.vertices[v].co, v)
    kd.balance()
    Wp = np.zeros((len(piece.data.vertices), len(names)))
    Mi = piece.matrix_world
    for v in piece.data.vertices:
        tot = 0.0
        for co, j, d in kd.find_n(Mi @ v.co, k):
            w = 1.0 / max(d, 1e-3)
            Wp[v.index] += W[j] * w
            tot += w
        Wp[v.index] /= tot
    S.set_weights(piece, Wp, names, 4, 0.03)


S.skin_like = skin_like


# ================================================================================================ the helmet
_build_helmet = S.build_helmet


def build_helmet(kind, J, sides=None):
    ob = _build_helmet(kind, J, sides)
    if kind != "m33" or "crown" not in J:
        return ob
    # the boss on the crown over its vent
    k = S.HELMETS[kind]
    top = np.array((J["skull_x"] - 0.004 + k["dx"], 0.0, J["crown"] - 0.108 + k["az"] + 0.013))
    g = S.Geo()
    g.tube(top - np.array((0.0, 0.0, 0.004)), top + np.array((0.0, 0.0, 0.0028)), 0.012, 0.0095, "helmet", 12)
    boss = g.build("helmet_boss")
    return S.join_meshes([ob, boss], "helmet_" + kind)


S.build_helmet = build_helmet


# ================================================================================================ the files
def export_ita(info, dress="summer"):
    """soldier_ITA.glb (LOD0, every clip), soldier_ITA_LOD1.glb and _LOD2.glb (same skeleton, no clips) and
    soldier_ITA.json (the game's own form); for the desert dress the same with its suffix. No .fbx."""
    t0 = time.time()
    tag, suffix = DRESSES[dress]
    base = f"soldier_{NATION}{suffix}"
    sc = kit.fresh(S.FPS)
    m = S.assemble(tag)
    m["arm"].name = "soldier_" + NATION                     # (either dress the same rig, by the same names)
    m["body"].name = "body_" + NATION
    S.bake_clips(m)
    socks = m["sockets"]
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    sc.frame_set(0)
    n = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    rec = info.setdefault(dress, {})
    rec["tris"] = n
    rec["parts"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    rec["short"] = {k: float(v) for k, v in m["short"].items()}
    S.export_glb(S.stage_path(f"{base}.glb"), [m["arm"]] + lods[0] + socks)
    for lod in (1, 2):
        S.export_glb(S.stage_path(f"{base}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
    data = S.write_game_json(S.stage_path(f"{base}.json"), m, lods)
    data["about"] = data["about"].replace("soldier.py.", "soldier_ita.py on soldier.py.")
    data["about"] += f" His collar patches take the {MARKS[0]} paint and their stars the {MARKS[1]} paint (pale, never black)."
    if dress != "summer":
        data["about"] += f" Dress: {dress}; paint by its colours below (they differ from the grey-green man's)."
        data["colours"] = dict(S.LOOK[tag])
    with open(S.stage_path(f"{base}.json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    rec["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
    rec["high"] = data["high"]
    for n_ in (f"{base}.glb", f"{base}_LOD1.glb", f"{base}_LOD2.glb", f"{base}.json"):
        S.deliver(n_)
    rec["export_s"] = round(time.time() - t0)
    print(f"  {base} exported in {time.time() - t0:.0f} s: {n}")
    return info


# ================================================================================================ his picture
def picture(info, fast=False, deliver=True):
    """The Italian man in grey-green and in the desert dress beside the German, at ease, from the front quarter, from
    behind and close: one sheet, pictures/soldier_ITA.png."""
    kit.fresh(S.FPS)
    cast = [("ITA", -1.1), ("ITAD", 0.0), ("GER", 1.1)]
    for nation, y in cast:
        m = S.assemble(nation)
        S.studio_smooth(m)
        for mt in bpy.data.materials:
            if not mt.name.startswith(("ITA_", "ITAD_", "GER_", "backdrop", "ground")):
                mt.name = nation + "_" + mt.name
        m["arm"].location = (0.0, y, 0.0)
        P = S.base_stand()
        P["foot_l"] = {"at": (0.12, 0.12, S.ANKLE), "yaw": 6.0}
        P["foot_r"] = {"at": (-0.06, -0.12, S.ANKLE), "yaw": -8.0}
        S.pose_once(m["arm"], m["rig"], P, 0)
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(640 if fast else 1100, 760 if fast else 1300, 12 if fast else 48)
    shots = []
    for key, yaw, pitch, dist, aim in (("front", 25.0, 6.0, 5.6, (0.0, 0.0, 1.0)), ("back", 200.0, 8.0, 5.6, (0.0, 0.0, 1.0)),
                                       ("near", 15.0, 4.0, 2.1, (0.0, -0.6, 1.3)), ("legs", 20.0, 2.0, 1.6, (0.0, -0.9, 0.42))):
        kit.orbit(cam, aim, dist, pitch, yaw)
        path = os.path.join(S.SCRATCH, "pictures", f"ita_{key}.png")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        kit.still(path)
        shots.append(path)
    name = f"soldier_{NATION}.png"
    out = S.stage_path("pictures", name)
    kit.sheet(shots, out, cols=2, labels=["Italian rifleman in grey-green (left), in the desert dress, beside the German",
                                          "From behind: the sahariana's yoke over the shoulders",
                                          "Close: open neck, collar patch and star, the helmet of 1933",
                                          "Breeches bloused over the puttees, ankle boots"],
              title="Italian rifleman: soldier_ita.py on soldier.py (our own design)")
    if deliver and not fast:
        S.deliver(f"pictures/{name}")
    info["picture"] = out
    return out


def main():
    a = kit.args()
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export", "picture"]
    dresses = a[a.index("--dress") + 1].split(",") if "--dress" in a else ["summer", "desert"]
    p = S.stage_path(f"export_info_{NATION}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "export":
            for d in dresses:
                export_ita(info, d)
        elif step == "picture":
            picture(info, fast, deliver="--keep" not in a)
        with open(p, "w") as f:
            json.dump(info, f, indent=1)
        print(f"step {step}: {time.time() - t0:.0f} s")
    ok = S.check_delivered()
    print("delivered" if ok else "SOME FILES DID NOT LAND")


if __name__ == "__main__":
    main()
