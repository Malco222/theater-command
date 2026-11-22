"""The American paratrooper of the Normandy battles (June 1944): a rifleman of the airborne divisions in the M1942
parachute jumper's coat and trousers, jump boots, the M2 helmet under a net, the flag on his right sleeve, armed with
the M1 rifle. Made beside soldier_usa.py and soldier.py (both stay as they are).

    python soldier_usa_para.py --only export,picture     build, rig, animate and export him; his picture sheet
    (--keep: stage only, nothing copied to the shared folder; --fast: a small test picture; --only look: test stills)

Run it in a checkout of claude/project-thread-8egity's Tools/battle_units (soldier.py, soldier_usa.py, human.py,
mocap.py, kit.py and MakeHuman's base files beside it), with the bpy module (Blender 5.0.1), as soldier_usa.py is run.

He is soldier_usa.py's rifleman in rig and clips: the same 32 bones, 5 sockets, the 19 clip names and the Garand's
reload_stand, reload_kneel and reload_prone (with_reload), the cartridge belt's ten pockets the reload reaches into.
What is his own is his dress and kit:

- the M1942 jumper's coat and trousers in their own light shade (paints tunic and trousers, recoloured): the coat's
  two large slanted bellows pockets on the chest and two on the skirt, each with a two-snap flap, its own cloth belt at
  the waist, and the trousers' bellows cargo pockets on the thighs with their tie-downs; the riggers' darker canvas
  over the elbows and the knees (paint 20, reinforce);
- russet jump boots laced high (about 25 cm), the trousers bloused over them; no leggings;
- the M2 helmet: the M1's shell with its fixed D-bales, the liner's A-yoke chin strap and the leather chin cup on his
  chin, under a net (paint 21, net) with a few strips of burlap scrim;
- the flag on his right sleeve, below the shoulder (12.7 x 7.6 cm): seven red and six white stripes (paints 16 and 15,
  the medic's red and white) and the blue canton (paint 22, flag_blue); no division patch;
- the cartridge belt and suspenders, canteen and first-aid pouch (the rifleman's); the M1936 musette bag high on his
  back; a cloth bandoleer of six pockets over his left shoulder across his chest; his grenades in the coat's
  pockets; the M3 trench knife in its leather scabbard strapped to his lower right leg; the gas-mask bag strapped
  to his left thigh.
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import soldier_usa as U  # noqa: E402  (soldier.py and the American's wrappers)
S = U.S
import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402
import kit  # noqa: E402
from sculpt import Ellipsoid, HalfSpace, Layer, RoundBox, RoundCone, Sphere, Torus, Tube, frame_from, on_surface, rot_euler  # noqa: E402,F401

NATION = "USA"
S.SCRATCH = "/home/user/art/para/"
S.STAGE = os.path.join(S.SCRATCH, "out")
BASE = "soldier_USA_para"
SEED = 171                 # (his face: MakeHuman's mix 171; the rifleman is 85, the squad leader 192, the medic 140,
#                            the bazooka gunner 118 and his loader 146, the radio man 163, and 127 is taken)
S.NATION_SEED[NATION] = SEED

# ---- his colours: the jump suit's own light shade (paints 0 and 1, recoloured for his file), russet boots; and three
# new paints (soldier.py's 0-14, the medic's 15-16, the squad leader's 17, the bazooka's 18, the radio's 19)
S.LOOK[NATION].update({
    "tunic": "948C6A",      # the M1942 jump suit: a light khaki-drab poplin, paler and yellower than the wool jacket
    "trousers": "908867",   # (the same cloth; the trousers a shade darker with wear)
    "boots": "6E4429",      # russet brown jump boots
})
NEW_PAINTS = {"reinforce": 20, "net": 21, "flag_blue": 22}
S.FIGURE_PAINTS.update(NEW_PAINTS)
S.FIGURE_PAINTS.update({"medic_white": 15, "medic_red": 16})   # (the flag's white and red: the medic's own two paints)
S.LOOK[NATION].update({"reinforce": "565638",   # the riggers' darker olive drab canvas over the elbows and knees
                       "net": "4A4632",         # the helmet net's dark brown-green cord
                       "flag_blue": "283460",   # the flag's canton
                       "medic_white": "E4E1D3", "medic_red": "A3221C"})
FLAG_PAINTS = ("medic_white", "medic_red", "flag_blue")

# ---- his kit's switches: jump boots (made as soldier.py's tall boots, cut low: see uniform_layers), no leggings
S.NATION_KIT[NATION]["tall_boots"] = True

R_axis, M4, _n = S.R_axis, S.M4, S._n
_walk = on_surface


def on_surface(L, start, way, steps=60):
    """sculpt.py's on_surface, by a marched ray (hit, below) where it meets the cloth: the cloth's field is not a true
    distance everywhere, and a walk by its values can overshoot into the cloth (a snap 2 cm under it)."""
    p, nrm = hit(L, start, way, reach=0.6)
    return (p, nrm) if p is not None else _walk(L, start, way, steps)


# ================================================================================================ shapes on a cloth
class Over:
    """A cloth's own surface stood `off` metres out of it, kept to where `region` is (its inside): a patch sewn on, a
    tape round a leg, a belt on a coat, following the cloth exactly. `src` is a snapshot of the cloth's layer."""

    def __init__(self, src, off, region):
        self.src, self.off, self.region = src, off, region
        self.lo, self.hi = region.lo, region.hi
        self.c, self.R = region.c, None

    def d(self, px, py, pz):
        bx, by, bz = np.broadcast_arrays(np.asarray(px, float), np.asarray(py, float), np.asarray(pz, float))
        shape = bx.shape
        r = self.region.d(bx, by, bz)
        out = np.array(r, dtype=float, copy=True)
        near = r < 0.02                                    # (the cloth asked only where the region is near)
        if near.any():
            P = np.stack([bx[near], by[near], bz[near]], axis=1)
            out[near] = np.maximum(self.src.at_points(P) - self.off, r[near])
        return out.reshape(shape)


def snapshot(L):
    c = Layer(L.name + "_was", L.paint)
    c.ops = list(L.ops)
    return c


class Box:
    """An axis box as a region (negative inside)."""

    def __init__(self, lo, hi):
        self.lo, self.hi = np.asarray(lo, float), np.asarray(hi, float)
        self.c, self.R = (self.lo + self.hi) / 2, None

    def d(self, px, py, pz):
        c, h = self.c, (self.hi - self.lo) / 2
        qx, qy, qz = np.abs(px - c[0]) - h[0], np.abs(py - c[1]) - h[1], np.abs(pz - c[2]) - h[2]
        o = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2 + np.maximum(qz, 0) ** 2)
        return o + np.minimum(np.maximum(np.maximum(qx, qy), qz), 0.0)


# ================================================================================================ the jump suit
JUMP_BOOT = 0.165          # the boot's top above the ankle joint (m): about 25 cm off the ground, as the 10-inch boot
BLOUSE = (0.018, 0.02)     # (the trousers' fullness rolled over the boot's top: its radius out, and its height)
CHEST_POCKET = (0.011, 0.07, 0.088)       # (half: deep, wide, tall) the coat's chest pockets, bellows
SKIRT_POCKET = (0.009, 0.072, 0.07)       # the coat's lower pockets on the skirt
POCKET_SLANT = 14.0        # (degrees: the pockets set slanted, their outer side higher, the mouths toward his middle)
CARGO = (0.016, 0.082, 0.105)             # the trousers' cargo pockets on the thighs (half: deep, wide, tall)
PATCH = 0.0017             # (m: how far the riggers' canvas stands off the cloth)
_usa_uniform = S.uniform_layers


def uniform_layers(man, J, spec, LM):
    knee = LM["knee"]
    LM["knee"] = LM["ankle"] + JUMP_BOOT + 0.075          # (soldier.py's tall boot's top is the knee less 7.5 cm)
    try:
        layers = _usa_uniform(man, J, spec, LM)
    finally:
        LM["knee"] = knee
    if not U._ours(spec):
        return layers
    tunic, trousers, boots = layers[0], layers[1], layers[2]
    top = LM["boot_top"]
    # ---- the trousers bloused over the boots: a full roll of cloth hanging over the boot's top
    for s, side in (("l", 1.0), ("r", -1.0)):
        c = S.leg_point(J, s, top + 0.03)
        Rs = S.ring_frame(np.array(J["calf_" + s]) - np.array(J["foot_" + s]))
        rx, ry = S.ring_axes(trousers, c, Rs)
        rr = None
        if side < 0:                   # (the leg's own cloth a little above the boot, under where the knife will lie)
            rr = min(float(np.linalg.norm((hit(trousers, S.leg_point(J, s, z) - KNIFE_WAY * 0.25, KNIFE_WAY)[0]
                                           - S.leg_point(J, s, z))[:2])) for z in (top + 0.05, top + 0.065, top + 0.08))
        c = S.leg_point(J, s, top + 0.004)
        trousers.add(Torus(c, rx + 0.007, BLOUSE[0], Rs @ rot_euler(0, -5, 0), major_y=ry + 0.007), 0.022)
        if rr is not None:             # (the roll pressed flat under the trench knife's scabbard, as its strap holds it)
            q = c + (-KNIFE_WAY) * (rr + 0.002 + 0.03) + np.array((0.0, 0.0, 0.012))
            trousers.sub(RoundBox(q, (0.03, 0.036, 0.055), 0.006, frame_from(-KNIFE_WAY, (0.0, 0.0, 1.0))), 0.006)
            if os.environ.get("PARA_DEBUG"):
                for z in (top - 0.01, top + 0.005, top + 0.02, top + 0.035, top + 0.05):
                    ax_ = S.leg_point(J, s, z)
                    h_, _ = hit(trousers, ax_ - KNIFE_WAY * 0.25, KNIFE_WAY)
                    print(f"     notch: z {z:.3f} rr {rr:.4f} cloth at {float(np.linalg.norm((h_ - ax_)[:2])):.4f}")
    # ---- the boots laced high up the front: the lacing a raised strip from the instep to the top
    lace = Layer("laces", "leather")
    for s, side in (("l", 1.0), ("r", -1.0)):
        pts = []
        for z in np.linspace(LM["ankle"] + 0.01, top - 0.006, 6):
            c = S.leg_point(J, s, z)
            p, nrm = on_surface(boots, c + np.array((0.2, 0.0, 0.0)), (-1.0, 0.0, 0.0))
            pts.append((p, nrm))
        for (a, na), (b, nb) in zip(pts[:-1], pts[1:]):
            lace.add(RoundCone(a - na * 0.001, b - nb * 0.001, 0.0062, 0.0062), 0.003)
        foot = np.array(J["foot_" + s]) + np.array((0.045, 0.0, -0.012))         # (and down over the instep)
        p, nrm = on_surface(boots, foot + np.array((0.0, 0.0, 0.2)), (0.0, 0.0, -1.0))
        lace.add(RoundCone(pts[0][0] - pts[0][1] * 0.001, p - nrm * 0.001, 0.0062, 0.0058), 0.003)
    layers.append(lace)
    # ---- the coat: the chest pockets and the skirt's, slanted (outer side up), each with its flap: the coat's own
    # surface stood out over the pocket's outline (so a pocket lies round the chest as cloth does), its flap a little
    # further out over the top
    zb = LM["Z"](1.0)
    was = snapshot(tunic)
    for side in (1.0, -1.0):
        for aim_, half, way in (((0.4, 0.088 * side, LM["Z"](1.272)), CHEST_POCKET, (-1.0, -0.22 * side, 0.0)),
                                ((0.4, 0.112 * side, LM["Z"](0.85)), SKIRT_POCKET, (-1.0, -0.3 * side, 0.0))):
            p, nrm = on_surface(tunic, np.array(aim_), way)
            R = frame_from(nrm, (0.0, 0.0, 1.0)) @ rot_euler(POCKET_SLANT * side, 0, 0)
            tunic.add(Over(was, half[0], RoundBox(p, (0.06, half[1], half[2]), 0.012, R)), 0.004)
            flap = p + R @ np.array((0.0, 0.0, half[2] - 0.019))
            tunic.add(Over(was, half[0] + 0.0035, RoundBox(flap, (0.06, half[1] + 0.004, 0.022), 0.006, R)), 0.002)
            LM.setdefault("para_pockets", []).append((p, R, half))
    # ---- the coat's own cloth belt, cinched at the waist above the cartridge belt (kept off the arms)
    was = snapshot(tunic)
    zc = zb + U.BELT_W / 2 + 0.024
    tunic.add(Over(was, 0.0026, Box((-0.4, -0.235, zc - 0.017), (0.4, 0.235, zc + 0.017))), 0.0)
    LM["coat_belt"] = zc
    # ---- the trousers' cargo pockets on the outside of the thighs (a little forward), each with a two-snap flap, and
    # the tie-down tape round the thigh under it
    tw = snapshot(trousers)
    for s, side in (("l", 1.0), ("r", -1.0)):
        hip, knee_ = np.array(J["thigh_" + s]), np.array(J["calf_" + s])
        zc = knee_[2] + (hip[2] - knee_[2]) * 0.40
        c = hip + (knee_ - hip) * ((hip[2] - zc) / (hip[2] - knee_[2]))
        way = _n(np.array((-0.45, -side, 0.0)))
        p, nrm = on_surface(trousers, c - way * 0.25, way)
        nrm = _n(nrm * np.array((1.0, 1.0, 0.25)))
        R = frame_from(nrm, (0.0, 0.0, 1.0))
        trousers.add(Over(tw, CARGO[0], RoundBox(p, (0.07, CARGO[1], CARGO[2]), 0.016, R)), 0.008)
        flap = p + R @ np.array((0.0, 0.0, CARGO[2] - 0.028))
        trousers.add(Over(tw, CARGO[0] + 0.004, RoundBox(flap, (0.07, CARGO[1] + 0.005, 0.03), 0.008, R)), 0.003)
        LM.setdefault("cargo", {})[s] = (p, R)
        zt = zc - CARGO[2] - 0.016
        LM.setdefault("tie_z", {})[s] = zt                  # (the tie-down tape round the thigh: a patch, below)
    print(f"  para: jump boots to {top:.3f} m; coat belt at {LM['coat_belt']:.3f}; cargo pockets "
          + ", ".join(f"{s} {np.round(LM['cargo'][s][0], 3)}" for s in ("l", "r")))
    return layers


S.uniform_layers = uniform_layers
S.BUDGET["uniform"] = 2450 + 50            # (the pockets, the belt, the blousing and the laces need their share of faces)


# ================================================================================================ the kit
MUSETTE = (0.062, 0.152, 0.136)            # the M1936 musette bag (half: deep, wide, tall): about 12 x 30 x 27 cm, packed
GRENADE = (0.029, 0.034)                   # the fragmentation grenade's body (radius, half height)
GRENADE_AT = ((0.05, 1.13), (0.115, 1.19))  # (y to his left, height on the standing man: the two grenades' middles)
_usa_kit = S.build_kit


def across(Z, side, lo):
    """soldier.py's way from the top of one shoulder (side 1: the left) down across the chest to the other hip, and
    round the back up to the shoulder again."""
    pts = ((0.02, 0.12, 1.468), (0.1, 0.1, 1.4), (0.14, 0.04, 1.29), (0.15, -0.035, 1.17), (0.135, -0.105, 1.06),
           (0.08, -0.16, lo + 0.02), (-0.02, -0.185, lo), (-0.12, -0.135, lo + 0.06), (-0.15, -0.04, 1.17),
           (-0.14, 0.055, 1.31), (-0.08, 0.11, 1.42), (0.0, 0.124, 1.468))
    return [(x, y * side, Z(z)) for x, y, z in pts]


def y_strap_ctrl(Z, side):
    """soldier.py's Y-strap (the suspenders) on one side: from the belt in front over the shoulder to the back."""
    return [(x, y * side, Z(z)) for x, y, z in ((0.13, 0.082, 1.03), (0.13, 0.09, 1.16), (0.125, 0.1, 1.28), (0.09, 0.112, 1.4),
            (0.025, 0.12, 1.468), (-0.05, 0.112, 1.45), (-0.12, 0.085, 1.37), (-0.142, 0.045, 1.29), (-0.145, 0.006, 1.235))]


def build_kit(J, spec, body_ls, Z=None):
    out = _usa_kit(J, spec, body_ls, Z)
    if not U._ours(spec):
        return out
    Z = Z or (lambda z: z)
    tunic, trousers = body_ls[0], body_ls[1]
    for k in ("pack", "shovel", "details"):
        if k in out:
            bpy.data.objects.remove(out.pop(k)[0])
    # ---- what lies over the coat: the cloth and the suspenders (the bandoleer lies over both)
    over = Layer("over", "tunic")
    over.ops = list(tunic.ops)
    for side in (1.0, -1.0):
        P, N = S.project(tunic, S.spline(y_strap_ctrl(Z, side), 22))
        for a, b, na, nb in zip(P[:-1], P[1:], N[:-1], N[1:]):
            over.add(RoundCone(a + na * 0.0025, b + nb * 0.0025, 0.0045, 0.0045), 0.002)
    # ---- the small things: the snaps on the coat's pocket flaps and the cargo pockets', the coat belt's buckle, the
    # zip's pull at the throat
    g = S.Geo()
    for p, R, half in J.get("para_pockets", []):
        for dy in (-0.55, 0.55):
            q = p + R @ np.array((0.0, dy * half[1], half[2] - 0.022))
            q, nrm = on_surface(tunic, q + R[:, 0] * 0.06, -R[:, 0])
            g.tube(q - nrm * 0.001, q + nrm * 0.003, 0.0055, 0.005, "steel", 6)
    for s in ("l", "r"):
        cc, R = J["para_cargo"][s]
        for dy in (-0.5, 0.5):
            q = cc + R @ np.array((0.0, dy * CARGO[1], CARGO[2] - 0.026))
            q, nrm = on_surface(trousers, q + R[:, 0] * 0.08, -R[:, 0])
            g.tube(q - nrm * 0.001, q + nrm * 0.003, 0.0058, 0.0052, "steel", 6)
    p, nrm = on_surface(tunic, (0.3, 0.0, J["para_coat_belt"]), (-1.0, 0.0, 0.0))
    g.box(p + nrm * 0.002, (0.003, 0.02, 0.019), "steel", frame_from(nrm, (0.0, 0.0, 1.0)))
    p, nrm = on_surface(tunic, (0.3, 0.0, Z(1.39)), (-1.0, 0.0, 0.0))
    g.box(p + nrm * 0.003, (0.0025, 0.005, 0.011), "steel", frame_from(nrm, (0.0, 0.0, 1.0)))
    collar = body_ls[3]
    for chain in J.get("usa_shoulder_straps_kept", []):
        q, nrm = chain[1]
        g.tube(q + nrm * 0.004, q + nrm * 0.0085, 0.0068, 0.006, "leather", 6)
    out["details"] = (g.build("details"), None)
    # ---- the bandoleer: six pockets of Garand clips over his left shoulder, across his chest to his right side above
    # the belt, and round his back
    path = S.spline(across(Z, 1.0, 1.075), 30)
    g = S.Geo()
    S.strap_on(g, over, path, 0.056, 0.004, "straps", gap=0.003)
    out["bandoleer"] = (g.build("bandoleer"), None)
    P, N = S.project(over, path)
    for a, b, na, nb in zip(P[:-1], P[1:], N[:-1], N[1:]):            # (the bag on his back lies over it too)
        over.add(RoundCone(a + na * 0.004, b + nb * 0.004, 0.005, 0.005), 0.002)
    L = Layer("bandoleer_pockets", "straps")
    front = [i for i in range(len(P)) if P[i][0] > 0.05 and Z(1.08) < P[i][2] < Z(1.41)]
    for i in np.linspace(front[0], front[-1], 6).astype(int):
        T_ = P[min(i + 1, len(P) - 1)] - P[max(i - 1, 0)]
        R = frame_from(N[i], T_)
        L.add(RoundBox(P[i] + N[i] * 0.016, (0.013, 0.04, 0.031), 0.008, R), 0.0)      # (z along the band: 6 x 8 cm)
    out["bandoleer_pockets"] = (S.mesh_from_layers("bandoleer_pockets", [L], 0.0025, 160), None)
    # ---- the musette bag high on his back, hung from the suspenders: its back against the back's highest points
    hx, hy, hz = MUSETTE
    hi = Z(1.41)
    lo = hi - 2.0 * hz
    backs = []
    for z in np.arange(lo, hi + 1e-6, 0.015):
        for y in np.arange(-0.13, 0.131, 0.026):
            p, _ = on_surface(over, (-0.4, y, z), (1.0, 0.0, 0.0))
            if abs(p[0]) < 0.5:
                backs.append(p[0])
    bx = min(backs) - 0.012
    c = np.array((bx - hx, 0.0, (lo + hi) / 2))
    bag = Layer("musette", "pack")
    flap = Layer("musette_flap", "pack")
    pk = Layer("musette_pockets", "pack")
    straps = Layer("musette_straps", "straps")
    bag.add(RoundBox(c, (hx, hy, hz), 0.035))
    flap.add(RoundBox(c + np.array((-0.004, 0.0, hz * 0.42)), (hx + 0.005, hy + 0.005, hz * 0.6), 0.03), 0.004)
    for dy, w in ((0.0, 0.05), (0.1, 0.034), (-0.1, 0.034)):                   # (its three outer pockets, low)
        pk.add(RoundBox(c + np.array((-hx - 0.006, dy, -hz * 0.55)), (0.014, w, 0.052), 0.01), 0.006)
    for dy in (-0.075, 0.075):                                                  # (the flap's two straps and buckles)
        straps.add(RoundBox(c + np.array((-hx - 0.006, dy, -hz * 0.05)), (0.0035, 0.012, hz * 0.5), 0.002), 0.0)
    me = S.mesh_from_layers("musette", [bag, flap, pk, straps], 0.004, 230)
    g = S.Geo()
    g.absorb(me)
    for dy in (-0.075, 0.075):
        g.box(c + np.array((-hx - 0.011, dy, -hz * 0.42)), (0.0025, 0.014, 0.011), "steel")
    for side in (1.0, -1.0):                                                    # (its hooks up to the suspenders)
        a = np.array((bx - 0.004, side * 0.075, hi + 0.004))
        b, nrm = on_surface(over, (-0.4, side * 0.07, hi + 0.035), (1.0, 0.0, 0.0))
        b = b + nrm * 0.004
        u = _n(b - a)
        w = _n(np.array((0.0, 1.0, 0.0)) - u * u[1])
        g.box((a + b) / 2, (0.0025, 0.012, float(np.linalg.norm(b - a)) / 2 + 0.008), "straps", np.stack([np.cross(w, u), w, u], axis=1))
    out["musette"] = (g.build("musette"), "chest")
    J["musette"] = {"c": [float(v) for v in c], "half": [hx, hy, hz]}
    # ---- no grenades hung outside: wherever they hook on his front, the left forearm (at port, the advance, the
    # reload) or the rifle passes 3-4 cm through them (fit check, 11-22). He carries them in the coat's pockets.
    # ---- the trench knife in its leather scabbard, strapped on the outside of the lower right leg, a little forward
    out["knife"] = (build_knife(J, [trousers, body_ls[2]], Z), "calf_r")
    # ---- the gas-mask bag strapped to the outside of the left thigh, upside down, over the cargo pocket
    out["gasmask"] = (build_gasmask(J, [trousers, body_ls[2]]), "thigh_l")
    # ---- the flag on the right sleeve, and the riggers' canvas over the knees and the elbows
    SHAPES.clear()
    for name, spec in PATCHES.items():
        spec["name"] = name
        SHAPES[name] = limb_shape(tunic if spec["a"].startswith("upperarm") else trousers, J, spec)
        out[name] = (patch_mesh(name, 0), None)
    return out


def grenade(g, c, R, r, hh, sides=8):
    """A fragmentation grenade: the oval body (a lathe, its serrations left to the paint), the fuse, the lever down its
    side and the ring. R: x up the grenade, z out of the chest."""
    rings = [[c + R[:, 0] * (-hh)]]
    for a in (-60, -25, 10, 45):
        t = math.radians(a)
        rr, zz = r * math.cos(t), hh * math.sin(t)
        rings.append([c + R[:, 0] * zz + (R[:, 1] * math.cos(2 * math.pi * k / sides) + R[:, 2] * math.sin(2 * math.pi * k / sides)) * rr
                      for k in range(sides)])
    rings.append([c + R[:, 0] * (hh * 0.93)])
    g.shell(rings, "helmet")
    top = c + R[:, 0] * hh
    g.tube(top - R[:, 0] * 0.004, top + R[:, 0] * 0.018, 0.009, 0.0085, "steel", 8)                     # the fuse
    g.box(c + R[:, 0] * (hh * 0.45) + R[:, 1] * (r * 0.93), (0.03, 0.002, 0.0065), "steel", R)        # the lever
    g.tube(top + R[:, 0] * 0.01 + R[:, 1] * 0.012, top + R[:, 0] * 0.01 + R[:, 1] * 0.03, 0.0095, 0.0095, "steel", 6, caps=False)  # ring


def hit(L, start, way, reach=0.4, step=0.002):
    """Where the ray from `start` along `way` first meets the layer's surface (marched in steps of 2 mm, then halved
    down: the cloth's field is not a true distance everywhere, and a walk by its values can overshoot), and the
    surface's outward normal there; (None, None) when it meets none within `reach` or starts inside."""
    w = _n(np.asarray(way, float))
    p0 = np.asarray(start, float)
    ts = np.arange(0.0, reach, step)
    d = L.at_points(p0[None, :] + ts[:, None] * w[None, :])
    idx = np.where(d <= 0.0)[0]
    if not len(idx) or idx[0] == 0:
        return None, None
    lo, hi = ts[idx[0] - 1], ts[idx[0]]
    for _ in range(20):
        mid = (lo + hi) / 2
        if float(L.at_points((p0 + w * mid)[None, :])[0]) > 0.0:
            lo = mid
        else:
            hi = mid
    p = p0 + w * (lo + hi) / 2
    e = 2e-4
    g = np.array([float(L.at_points((p + np.eye(3)[i] * e)[None, :])[0] - L.at_points((p - np.eye(3)[i] * e)[None, :])[0])
                  for i in range(3)])
    n = g / np.linalg.norm(g) if np.linalg.norm(g) > 1e-9 else -w
    return p, n


def height_over(layers, q, nrm, reach=0.1):
    """How far the cloth (the highest of the layers) stands above the point q along nrm (negative: below it)."""
    best = -reach
    for L in layers:
        p, _ = hit(L, q + nrm * reach, -nrm)
        if p is not None:
            best = max(best, float((p - q) @ nrm))
    return best


def lift_line(need):
    """The least straight line l0 + (l1 - l0) f over the needed heights (f, h) at every sample."""
    fs = np.array([f for f, _ in need])
    hs = np.array([h for _, h in need])
    best = None
    for l0 in np.linspace(hs.min(), 2.0 * hs.max(), 121):
        with np.errstate(divide="ignore", invalid="ignore"):
            l1 = np.max(np.where(fs > 1e-6, (hs - l0 * (1 - fs)) / np.maximum(fs, 1e-6), -1.0))
        if np.all(l0 * (1 - fs) + l1 * fs >= hs - 1e-6) and (best is None or l0 + l1 < best[0] + best[1]):
            best = (l0, l1)
    return best


KNIFE = (0.19, 0.132)       # the M6 scabbard's length; the guard, grip and pommel above it (32 cm in all)
KNIFE_WAY = _n(np.array((-0.25, 1.0, 0.0)))   # (looking at it: from outside the right leg, a little forward, inward)


def build_knife(J, layers, Z):
    """The M3 trench knife in the M6 leather scabbard on the outside of the right calf, a little forward: the scabbard's
    tip down over the boot's top, the grip of stacked leather washers up to a hand below the knee, the steel guard and
    pommel; two straps round the leg. The scabbard lies on the leg as the leg lies (its ends lifted each its own way,
    so it tilts over the blousing and the calf rather than standing off them)."""
    knee, ank = np.array(J["calf_r"]), np.array(J["foot_r"])
    u = _n(knee - ank)
    total = KNIFE[0] + KNIFE[1]
    s_top = (knee[2] - 0.05 - ank[2]) / u[2]
    way = KNIFE_WAY
    ends = []
    for s in (s_top - total, s_top):
        c = ank + u * s
        hits = [h for h in (hit(L, c - way * 0.2, way) for L in layers) if h[0] is not None]
        ends.append(max(hits, key=lambda h: float((c - h[0]) @ way)))                    # (the outermost)
    (a, na), (b, nb) = ends
    nrm = _n(na + nb)
    ax = _n(b - a)
    nrm = _n(nrm - ax * (nrm @ ax))
    side = _n(np.cross(ax, nrm))
    a = b - ax * total
    need = []
    for f in np.linspace(0.0, 1.0, 17):
        r = 0.0085 if f * total < KNIFE[0] else 0.0128
        for w in (-0.018, 0.0, 0.018):
            q = a + ax * (total * f) + side * (w * (0.55 if r > 0.01 else 1.0))
            need.append((f, height_over(layers, q, nrm) + r + 0.0025))
    l0, l1 = lift_line(need)
    prof = {}
    for f, h in need:
        prof[round(f, 2)] = max(prof.get(round(f, 2), -1.0), h)
    print("  para: the knife's needed lift along it (cm):", " ".join(f"{100 * v:.1f}" for v in prof.values()))
    if os.environ.get("PARA_DEBUG"):
        for f in np.linspace(0.0, 1.0, 17):
            q = a + ax * (total * f)
            print(f"     f {f:.2f} z {q[2]:.3f}: " + " ".join(f"{L.name} {100 * height_over([L], q, nrm):.1f}" for L in layers))
    base = a + nrm * l0
    top = b + nrm * l1
    ax = _n(top - base)
    side = _n(np.cross(ax, nrm))
    nrm = np.cross(side, ax)
    R = np.stack([ax, side, nrm], axis=1)
    mouth = base + ax * KNIFE[0]
    sc = Layer("scabbard", "leather")
    sc.add(RoundBox((base + mouth) / 2, (KNIFE[0] / 2, 0.019, 0.0075), 0.006, R))
    sc.add(RoundCone(base - ax * 0.006, base + ax * 0.035, 0.0085, 0.013), 0.008)            # (the tip's steel cap)
    me = S.mesh_from_layers("scabbard", [sc], 0.0022, 110)
    g = S.Geo()
    g.absorb(me)
    st = "steel"
    g.tube(mouth - ax * 0.004, mouth + ax * 0.003, 0.0105, 0.0105, st, 6, up=side)               # the scabbard's throat
    g.box(mouth + ax * 0.007, (0.0035, 0.03, 0.0062), st, R)                                       # the guard
    g.tube(mouth + ax * 0.01, mouth + ax * 0.117, 0.0118, 0.0124, "leather", 6, up=side)          # the grip
    g.tube(mouth + ax * 0.117, mouth + ax * 0.132, 0.0124, 0.009, st, 6, up=side)                 # the pommel
    for z in (0.05, 0.15):                                                                          # two straps round it
        q = base + ax * z
        g.box(q + nrm * 0.001, (0.011, 0.024, 0.0095), "straps", R)
    tip = mouth + ax * KNIFE[1]
    print(f"  para: the trench knife from {base[2]:.3f} to {tip[2]:.3f} m on the right leg (its ends "
          f"{100 * l0:.1f} and {100 * l1:.1f} cm off the cloth)")
    return g.build("knife")


GASMASK = (0.036, 0.085, 0.11)              # the gas-mask bag (half: deep, wide, tall)


def build_gasmask(J, layers):
    """The gas-mask bag (the paratroopers' lightweight bag) strapped upside down to the outside of the left thigh, a
    little forward, over the cargo pocket's lower half: its flap and snaps at the bottom, two straps round the thigh.
    Its back is set on the highest of the cloth under it (only rays that truly meet the cloth count)."""
    hip, knee = np.array(J["thigh_l"]), np.array(J["calf_l"])
    u = _n(knee - hip)
    zc = hip[2] + (knee[2] - hip[2]) * 0.64
    c = hip + u * ((zc - hip[2]) / u[2])
    way = _n(np.array((-0.25, -1.0, 0.0)))
    p0, n0 = hit(layers[0], c - way * 0.2, way)
    n0 = _n(np.array((n0[0], n0[1], 0.0)))
    R = frame_from(n0, -u)
    up = 0.0
    for dz in np.linspace(-GASMASK[2] + 0.02, GASMASK[2] - 0.02, 7):
        for dy in np.linspace(-GASMASK[1] + 0.025, GASMASK[1] - 0.025, 7):
            q = p0 + R @ np.array((0.0, dy, dz))
            up = max(up, height_over(layers, q, n0, 0.12))
    cc = p0 + n0 * (up + GASMASK[0] + 0.003)
    bag = Layer("gasmask", "helmet")
    bag.add(RoundBox(cc, GASMASK, 0.022, R))
    flap = Layer("gasmask_flap", "helmet")
    flap.add(RoundBox(cc + R @ np.array((0.004, 0.0, -GASMASK[2] + 0.03)), (GASMASK[0] + 0.004, GASMASK[1] + 0.004, 0.032), 0.012, R), 0.004)
    st = Layer("gasmask_straps", "straps")
    for z in (-0.05, 0.065):
        st.add(RoundBox(cc + R @ np.array((0.0, 0.0, z)), (GASMASK[0] + 0.004, GASMASK[1] + 0.012, 0.013), 0.008, R), 0.0)
    me = S.mesh_from_layers("gasmask", [bag, flap, st], 0.003, 170)
    g = S.Geo()
    g.absorb(me)
    for dy in (-0.045, 0.045):
        q = cc + R @ np.array((GASMASK[0] + 0.006, dy, -GASMASK[2] + 0.02))
        g.tube(q - R[:, 0] * 0.003, q + R[:, 0] * 0.002, 0.005, 0.0045, "steel", 8)
    print(f"  para: the gas-mask bag at {np.round(cc, 3)} on the left thigh ({100 * up:.1f} cm over the cloth's middle)")
    return g.build("gasmask")


S.build_kit = build_kit


# ================================================================================================ patches on the limbs
# The flag on his right sleeve and the riggers' darker canvas over his knees and elbows are thin pieces laid on the
# cloth as soldier_usa_medic.py lays its brassard: the cloth's radius round the limb's axis sampled under each, a
# lattice of quads 1.5 to 2 mm off the highest of it (walls down to the cloth at the near level), and each skinned with
# the limb's own weights as its lattice lies (never the coat's side by his armpit, nor the other leg).
#
# The flag (the D-Day pattern: about 3 x 5 inches, sewn or pinned on the right sleeve below the shoulder): the fly round
# the arm, the hoist along it; seven red and six white stripes, the blue canton at the top toward his back (a flag as
# printed, seen on his right side). Stars are left to the paint.
FLAG_W, FLAG_H = 0.127, 0.076             # the fly (round the arm) and the hoist (along it)
PATCHES = {
    # name: (limb's bones from-to, its middle as a share of the way, centre's bearing, width round, height along, shape)
    "flag": dict(a="upperarm_r", b="lowerarm_r", at=0.33, way="up", turn=28.0, W=FLAG_W, H=FLAG_H, round=False,
                 bones=("upperarm_r", "lowerarm_r"), lods={0: (5, 13, True, 0.0021), 1: (4, 5, False, 0.003)}),
    "knee_l": dict(a="thigh_l", b="foot_l", at=None, joint="calf_l", way="front", turn=0.0, W=0.13, H=0.17, round=True,
                   bones=("thigh_l", "calf_l"), lods={0: (4, 5, True, 0.004), 1: (2, 3, False, 0.005)}),
    "knee_r": dict(a="thigh_r", b="foot_r", at=None, joint="calf_r", way="front", turn=0.0, W=0.13, H=0.17, round=True,
                   bones=("thigh_r", "calf_r"), lods={0: (4, 5, True, 0.004), 1: (2, 3, False, 0.005)}),
    # (the elbows: on the upper arm's line, as the forearm bends forward from the elbow at rest; the patch from 8 cm
    # above the elbow to 3 cm past it, over the point of the elbow)
    "elbow_l": dict(a="upperarm_l", b="lowerarm_l", at=0.9, way="back", turn=0.0, W=0.10, H=0.11, round=True,
                    bones=("upperarm_l", "lowerarm_l"), lods={0: (3, 4, True, 0.004), 1: (2, 3, False, 0.005)}),
    "elbow_r": dict(a="upperarm_r", b="lowerarm_r", at=0.9, way="back", turn=0.0, W=0.10, H=0.11, round=True,
                    bones=("upperarm_r", "lowerarm_r"), lods={0: (3, 4, True, 0.004), 1: (2, 3, False, 0.005)}),
    # (the cargo pockets' tie-down tapes round the outside and back of each thigh, below the pocket; near only)
    "tie_l": dict(a="thigh_l", b="calf_l", at=None, z_key="para_tie_z", side="l", dir=(-0.7, 1.0, 0.0), turn=0.0, W=0.16,
                  H=0.013, round=False, paint="straps", bones=("thigh_l", "calf_l"), lods={0: (6, 1, True, 0.0035)}),
    "tie_r": dict(a="thigh_r", b="calf_r", at=None, z_key="para_tie_z", side="r", dir=(-0.7, -1.0, 0.0), turn=0.0, W=0.16,
                  H=0.013, round=False, paint="straps", bones=("thigh_r", "calf_r"), lods={0: (6, 1, True, 0.0035)}),
}
SHAPES = {}                               # (each patch's sampled cloth, kept for the levels of detail and the skinning)


def limb_shape(layer, J, spec):
    """The cloth's radius round a limb's axis (a to b) under a patch: (s along, phi round) sampled; phi 0 is `o`."""
    a, b = np.array(J[spec["a"]], float), np.array(J[spec["b"]], float)
    u = _n(b - a)
    L = float(np.linalg.norm(b - a))
    if spec.get("z_key"):
        z = J[spec["z_key"]][spec["side"]]
        c = a + u * ((z - a[2]) / u[2])
    elif spec["at"] is not None:
        c = a + u * L * spec["at"]
    else:
        j = np.array(J[spec["joint"]], float)
        c = a + u * float((j - a) @ u)
    ref = np.array((0.0, 0.0, 1.0)) if abs(u[2]) < 0.85 else np.array((1.0, 0.0, 0.0))
    o = _n(ref - u * (u @ ref))
    w = np.cross(u, o)
    H = spec["H"]
    Ss = np.linspace(-H / 2 - 0.01, H / 2 + 0.01, 13)
    PH = np.linspace(0.0, 2 * math.pi, 120, endpoint=False)
    rho = np.arange(0.012, 0.16, 0.0015)
    D = np.cos(PH)[:, None] * o + np.sin(PH)[:, None] * w
    pts = c[None, None, None, :] + Ss[:, None, None, None] * u + D[None, :, None, :] * rho[None, None, :, None]
    F = layer.at_points(pts.reshape(-1, 3)).reshape(len(Ss), len(PH), len(rho))
    k = np.argmax(F > 0, axis=2)
    found = (F > 0).any(axis=2)
    k = np.maximum(k, 1)
    lo, hi = rho[k - 1], rho[k]
    for _ in range(12):
        mid = (lo + hi) / 2
        p = c[None, None, :] + Ss[:, None, None] * u + D[None, :, :] * mid[:, :, None]
        inside = layer.at_points(p.reshape(-1, 3)).reshape(mid.shape) < 0
        lo, hi = np.where(inside, mid, lo), np.where(inside, hi, mid)
    r = (lo + hi) / 2
    med = np.nanmedian(np.where(found, r, np.nan), axis=1, keepdims=True)
    good = found & (r < 1.35 * med)
    for i in range(len(Ss)):
        idx = np.where(good[i])[0]
        if not len(idx):
            raise RuntimeError(f"the {spec['name']}'s row {i} meets no cloth round the limb")
        x = np.concatenate([idx - len(PH), idx, idx + len(PH)])
        r[i] = np.interp(np.arange(len(PH)), x, np.tile(r[i, idx], 3))
    r = np.maximum(r, (np.roll(r, 1, 1) + 2 * r + np.roll(r, -1, 1)) / 4)
    if "dir" in spec:
        d = np.asarray(spec["dir"], float)
        f0 = math.atan2(float(d @ w), float(d @ o))
    elif spec["way"] == "up":                                    # (the flag: from the arm's upper side toward his back)
        back = np.array((-1.0, 0.0, 0.0))
        f0 = math.copysign(math.radians(spec["turn"]), float(back @ w))
    else:
        d = np.array((1.0, 0.0, 0.0)) * (1.0 if spec["way"] == "front" else -1.0)
        f0 = math.atan2(float(d @ w), float(d @ o)) + math.radians(spec["turn"])
    out = dict(c=c, u=u, o=o, w=w, S=Ss, PH=PH, r=r, L=L, f0=f0, toward_back=float(np.array((-1.0, 0.0, 0.0)) @ w))
    print(f"  para: under the {spec['name']} the cloth is {100 * r.min():.1f}-{100 * r.max():.1f} cm round the limb's axis "
          f"({int((~good).sum())} ways carried across)")
    return out


def shape_r(B, s, phi):
    Ss, r = B["S"], B["r"]
    n = len(B["PH"])
    x = (phi % (2 * math.pi)) / (2 * math.pi) * n
    j0 = int(math.floor(x)) % n
    fj = x - math.floor(x)
    i = int(np.clip(np.searchsorted(Ss, s) - 1, 0, len(Ss) - 2))
    fi = float(np.clip((s - Ss[i]) / (Ss[i + 1] - Ss[i]), 0.0, 1.0))
    row = lambda ii: r[ii, j0] * (1 - fj) + r[ii, (j0 + 1) % n] * fj
    return float(row(i) * (1 - fi) + row(i + 1) * fi)


def patch_grid(name, lod):
    """A patch's lattice: phi of each column line (from his front toward his back where that has a sense), s of each
    row line (from the top down the limb), and each node's radius (the highest of the cloth near it, plus the lift)."""
    B, spec = SHAPES[name], PATCHES[name]
    cols, rows, walls, lift = spec["lods"][lod]
    r0 = shape_r(B, 0.0, B["f0"])
    half = spec["W"] / 2 / (r0 + lift)
    phis = [B["f0"] - half + 2 * half * i / cols for i in range(cols + 1)]
    if B["toward_back"] < 0:
        phis = phis[::-1]
    ss = [-spec["H"] / 2 + spec["H"] * j / rows for j in range(rows + 1)]
    if B["u"][2] > 0:                                             # (top first: s runs down the limb)
        ss = ss[::-1]
    dphi, ds = 2 * half / cols, spec["H"] / rows
    node = np.array([[max(shape_r(B, sj + e1, ph + e2) for e1 in np.linspace(-ds / 2, ds / 2, 3)
                          for e2 in np.linspace(-dphi / 2, dphi / 2, 3)) for ph in phis] for sj in ss]) + lift
    return phis, ss, node


def cell_paint(name, i, j, cols, rows):
    """Column i (0 at his front), row j (0 at the top): the flag's canton at the top toward his back, else its
    stripes; the patches all one paint (their corners left off, so they are rounded)."""
    if name != "flag":
        x, y = (i + 0.5) / cols * 2 - 1, (j + 0.5) / rows * 2 - 1
        if not PATCHES[name]["round"]:
            return PATCHES[name].get("paint", "reinforce")
        return "reinforce" if (cols < 3 or abs(x) ** 3 + abs(y) ** 3 <= 1.15) else None
    canton_cols = max(1, int(round(cols * 0.4)))
    canton_rows = int(round(rows * 7 / 13))
    if i >= cols - canton_cols and j < canton_rows:
        return "flag_blue"
    return "medic_red" if j % 2 == 0 else "medic_white"


def patch_mesh(name, lod):
    B, spec = SHAPES[name], PATCHES[name]
    cols, rows, walls, lift = spec["lods"][lod]
    c, u, o, w = B["c"], B["u"], B["o"], B["w"]
    phis, ss, node = patch_grid(name, lod)
    at = lambda ph, sj, rad: c + u * sj + (math.cos(ph) * o + math.sin(ph) * w) * rad
    V = [[at(phis[i], ss[j], node[j, i]) for i in range(cols + 1)] for j in range(rows + 1)]
    Vb = [[at(phis[i], ss[j], node[j, i] - lift - 0.001) for i in range(cols + 1)] for j in range(rows + 1)]
    g = S.Geo()
    on = {}
    for j in range(rows):
        for i in range(cols):
            pt = cell_paint(name, i, j, cols, rows)
            if pt is None:
                continue
            on[(i, j)] = pt
            quad = [V[j][i], V[j + 1][i], V[j + 1][i + 1], V[j][i + 1]]
            g._add(quad, [(0, 1, 2, 3)], pt)
    if walls:
        edge_paint = "medic_white" if name == "flag" else PATCHES[name].get("paint", "reinforce")
        for (i, j) in on:
            for (di, dj), (A, Bc) in (((-1, 0), ((i, j), (i, j + 1))), ((1, 0), ((i + 1, j + 1), (i + 1, j))),
                                      ((0, -1), ((i + 1, j), (i, j))), ((0, 1), ((i, j + 1), (i + 1, j + 1)))):
                if (i + di, j + dj) in on:
                    continue
                (ia, ja), (ib, jb) = A, Bc
                g._add([Vb[ja][ia], Vb[jb][ib], V[jb][ib], V[ja][ia]], [(0, 1, 2, 3)], edge_paint)
    return g.build(name if lod == 0 else f"{name}_LOD{lod}", sharp=55.0, recalc=True)


_skin_like = S.skin_like


def skin_like(piece, body, k=4):
    """A patch takes the weights of the limb's own points under it (the body's points carried mostly by its two
    bones) as its lattice lies; every other piece as soldier.py skins it."""
    name = piece.name.split(".")[0].split("_LOD")[0]
    if name not in PATCHES:
        return _skin_like(piece, body, k)
    from mathutils import kdtree
    spec, B = PATCHES[name], SHAPES[name]
    W, names = S.weights_of(body)
    limb = [names.index(b) for b in spec["bones"] if b in names]
    own = [i for i in range(len(W)) if int(np.argmax(W[i])) in limb]
    kd = kdtree.KDTree(len(own))
    for j, i in enumerate(own):
        kd.insert(body.matrix_world @ body.data.vertices[i].co, j)
    kd.balance()
    lod = int(piece.name.split("_LOD")[1][0]) if "_LOD" in piece.name else 0
    cols, rows, walls, lift = spec["lods"][lod]
    phis, ss, node = patch_grid(name, lod)
    Wn = np.zeros((rows + 1, cols + 1, len(names)))
    for j in range(rows + 1):
        for i in range(cols + 1):
            p = B["c"] + B["u"] * ss[j] + (math.cos(phis[i]) * B["o"] + math.sin(phis[i]) * B["w"]) * (node[j, i] - lift)
            tot = 0.0
            for co, jj, d in kd.find_n(Vector(p), k):
                wt = 1.0 / max(d, 1e-3)
                Wn[j, i] += W[own[jj]] * wt
                tot += wt
            Wn[j, i] /= tot
    other = "_r" if spec["bones"][0].endswith("_l") else "_l"           # (never a bone of the other side)
    tot_w = Wn.sum(axis=(0, 1))
    tot_w[[i for i, b in enumerate(names) if b.endswith(other)]] = -1.0
    main = np.argsort(-tot_w)[:4]
    keep = np.zeros(len(names), bool)
    keep[main] = True
    Wn[..., ~keep] = 0.0
    Wn[Wn < 0.03] = 0.0
    Wn /= Wn.sum(axis=2, keepdims=True)
    Wp = np.zeros((len(piece.data.vertices), len(names)))
    for v in piece.data.vertices:
        d = np.array(piece.matrix_world @ v.co) - B["c"]
        sv = float(d @ B["u"])
        ph = math.atan2(float(d @ B["w"]), float(d @ B["o"]))
        ph = phis[0] + ((ph - phis[0] + math.pi) % (2 * math.pi) - math.pi)
        x = float(np.clip((ph - phis[0]) / (phis[-1] - phis[0]) * cols, 0.0, cols - 1e-9))
        y = float(np.clip((sv - ss[0]) / (ss[-1] - ss[0]) * rows, 0.0, rows - 1e-9))
        i0, j0 = int(x), int(y)
        fx, fy = x - i0, y - j0
        Wp[v.index] = (Wn[j0, i0] * (1 - fx) * (1 - fy) + Wn[j0, i0 + 1] * fx * (1 - fy) + Wn[j0 + 1, i0] * (1 - fx) * fy
                       + Wn[j0 + 1, i0 + 1] * fx * fy)
    S.set_weights(piece, Wp, names, 4, 0.0)
    print(f"  para: {piece.name} skinned on {[names[b] for b in main]} as its lattice lies")


S.skin_like = skin_like


# ================================================================================================ the M2 helmet
# The M1's shell (soldier.py's "pot") as the paratroopers' M2: its own chin strap off, the fixed D-shaped bales at the
# sides under the rim, the liner's A-yoke (two straps a side, before and behind the ear, joined under it) to the
# leather chin cup on his chin; and over the shell a net of dark cord in diamonds, with a few strips of burlap scrim
# through it. The net and the scrim are drawn again for each level of detail (a thinned net falls to pieces); the yoke
# and the cup are laid on his own face once he is made (assemble).
NET_STEP = 0.57            # (radians between the net's cords, in the dome's flat map: about 7 cm over the crown)
NET_W = 0.0042             # (m: a cord's width)
NET_OFF = 0.0022           # (m: off the shell)
SCRIM = [(30.0, 0.55, 150.0, 0.085, "blanket"), (95.0, 0.9, 110.0, 0.075, "reinforce"), (160.0, 0.6, 200.0, 0.09, "blanket"),
         (215.0, 1.05, 250.0, 0.08, "reinforce"), (280.0, 0.75, 300.0, 0.085, "blanket"), (335.0, 1.2, 5.0, 0.07, "reinforce"),
         (125.0, 1.45, 70.0, 0.07, "blanket")]   # (bearing, polar angle, way it runs, length, paint)
HELMET_KIT = ("net",) + ("blanket", "reinforce")
HELMET_SIDES = 28          # (the shell drawn round in 28, not 36: under the net its facets do not show)
SHELL = {}
_build_helmet = S.build_helmet


class Shell:
    """The M1's outside as build_helmet made it ("pot"): its middle, its half-axes, a BVH of its mesh, the rim."""

    def __init__(self, ob, J):
        k = S.HELMETS["pot"]
        self.k = k
        self.C = np.array((J["skull_x"] - 0.004 + k["dx"], 0.0, J["crown"] - 0.108))
        self.az = k["az"] + 0.013
        self.ay = k["ay"] * (J["skull_half"][1] + 0.025) / 0.108
        self.ax = k["ax"] * (J["skull_half"][0] + 0.023) / 0.122
        me = ob.data
        Mw = ob.matrix_world
        self.V = [tuple(Mw @ v.co) for v in me.vertices]
        self.F = [tuple(p.vertices) for p in me.polygons]
        self.bvh = BVHTree.FromPolygons(self.V, self.F)

    def rho(self, f):
        return 1.0 / math.sqrt((math.cos(f) / self.ax) ** 2 + (math.sin(f) / self.ay) ** 2)

    def rim(self, f):
        """The rim's outer edge at bearing f: (radius out of the middle, height)."""
        k = self.k
        zE = k["edge"](f)
        z = zE if zE < 0 else zE - 1e-4
        return self.rho(f) + k["flare"](f), self.C[2] + z - k["drop"](f)

    def point(self, f, th, off):
        """The shell's outside on the ray from its middle at bearing f and polar angle th, `off` out of it."""
        d = np.array((math.sin(th) * math.cos(f), math.sin(th) * math.sin(f), math.cos(th)))
        hit, nrm, _, _ = self.bvh.ray_cast(Vector(self.C + d * 0.4), Vector(-d), 0.5)
        if hit is None:
            return None, None
        h, n = np.array(hit), _n(np.array(nrm))
        if n @ d < 0:
            n = -n
        return h + n * off, n

    def th_rim(self, f):
        r, z = self.rim(f)
        return math.atan2(r, z - self.C[2])


def net_mesh(lod):
    """The net at a level of detail: two families of cords crossing in diamonds (lines u + v and u - v constant in the
    dome's flat map, the polar angle as the radius), each a flat ribbon on the shell running straight from knot to knot,
    ended at the rim; and the scrim."""
    sh = SHELL["shell"]
    step = {0: NET_STEP, 1: NET_STEP * 1.7}[lod]
    width = {0: NET_W, 1: NET_W * 1.7}[lod]
    off = {0: NET_OFF, 1: NET_OFF * 1.8}[lod]
    margin = 0.03
    g = S.Geo()
    tmax = 2.1
    n = int(math.ceil(2 * tmax / step)) + 1

    def at(c0, fam, t):
        uu = c0 / 2 + t / 2
        vv = (c0 - uu) * fam
        return math.hypot(uu, vv), math.atan2(vv, uu)

    def inside(c0, fam, t):
        th, f = at(c0, fam, t)
        return th < sh.th_rim(f) - margin
    cords = 0
    for fam in (1.0, -1.0):
        for k in range(-n, n + 1):
            c0 = (k + 0.5) * step                                     # (u + fam * v = c0: one cord)
            ts = [(m + 0.5) * step for m in range(-n, n + 1)]         # (its knots, where the other cords cross it)
            runs, run = [], []
            for t0, t1 in zip(ts[:-1], ts[1:]):
                i0, i1 = inside(c0, fam, t0), inside(c0, fam, t1)
                if i0 and not run:
                    run.append(t0)
                if i0 != i1:                                          # (to the rim's edge, by halves)
                    lo, hi = (t0, t1) if i0 else (t1, t0)
                    for _ in range(12):
                        mid = (lo + hi) / 2
                        lo, hi = (mid, hi) if inside(c0, fam, mid) else (lo, mid)
                    if i0:
                        run.append(lo)
                        runs.append(run)
                        run = []
                    else:
                        run = [lo]
                if i1:
                    run.append(t1)
            if len(run) > 1:
                runs.append(run)
            for run in runs:
                if len(run) < 2:
                    continue
                pts = [sh.point(at(c0, fam, t)[1], at(c0, fam, t)[0], off) for t in run]
                pts = [(p, q) for p, q in pts if p is not None]
                if len(pts) < 2:
                    continue
                P = np.array([p for p, _ in pts])
                N = np.array([q for _, q in pts])
                verts, faces = [], []
                for i in range(len(P)):
                    T = P[min(i + 1, len(P) - 1)] - P[max(i - 1, 0)]
                    B = _n(np.cross(N[i], T))
                    verts += [P[i] + B * width / 2, P[i] - B * width / 2]
                for i in range(len(P) - 1):
                    faces.append((2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2))
                g._add(verts, faces, "net")
                cords += 1
    # the scrim: strips of burlap tucked through the net, lying on it, their tails lifting a little
    for bearing, th0, way, length, paint in SCRIM:
        if lod > 0 and paint == "reinforce":
            continue
        f0 = math.radians(bearing)
        wy = math.radians(way)
        n_s = 3 if lod == 0 else 2
        P, N = [], []
        for i in range(n_s + 1):
            a = i / n_s
            # (along the shell: a step of `length` in the map's direction `way`, from (f0, th0))
            uu = th0 * math.cos(f0) + a * length / 0.13 * math.cos(wy)
            vv = th0 * math.sin(f0) + a * length / 0.13 * math.sin(wy)
            th, f = math.hypot(uu, vv), math.atan2(vv, uu)
            th = min(th, sh.th_rim(f) - 0.02)
            p, n = sh.point(f, th, off + 0.0025 + 0.006 * a * a)
            if p is None:
                break
            P.append(p)
            N.append(n)
        if len(P) > 1:
            g.band(np.array(P), np.array(N), 0.022 if lod == 0 else 0.026, 0.0016, paint)
    return g.build("net" if lod == 0 else f"net_LOD{lod}", sharp=60.0, recalc=False)


def bales(sh):
    """The fixed D-shaped bales, one a side, just under the rim inside it."""
    g = S.Geo()
    for f in (math.pi / 2, -math.pi / 2):
        r, z = sh.rim(f)
        d = np.array((math.cos(f), math.sin(f), 0.0))
        c = sh.C + d * (r - 0.007) + np.array((0.0, 0.0, z - sh.C[2] - 0.006))
        R = frame_from(np.array((1.0, 0.0, 0.0)), d)
        g.box(c, (0.011, 0.0016, 0.0075), "helmet", np.stack([R[:, 0], R[:, 1], R[:, 2]], axis=1))     # (painted with it)
    return g.build("bales")


def build_helmet(kind, J, sides=None):
    ob = _build_helmet(kind, J, sides or (HELMET_SIDES if kind == "pot" and "crown" in J else None))
    if not (kind == "pot" and "crown" in J):
        return ob
    names = [mt.name.split(".")[0] for mt in ob.data.materials]
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    gone = [f for f in bm.faces if names[f.material_index] == "straps"]          # (the M1's own chin strap)
    bmesh.ops.delete(bm, geom=gone, context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(ob.data)
    bm.free()
    sh = Shell(ob, J)
    SHELL.clear()
    SHELL.update(shell=sh)
    net = net_mesh(0)
    print(f"  para: the M2's chin strap off ({len(gone)} faces); the net {S.tris_of(net)} triangles")
    return S.join_meshes([ob, bales(sh), net], ob.name.split(".")[0])


S.build_helmet = build_helmet


# ---- the liner's A-yoke and the chin cup, laid on his face (after he is made)
YOKE_W = 0.0125
YOKE_OFF = 0.0032


def ribbon(g, P, N, width, paint):
    """A flat strap (one face, no edges: a few millimetres off the skin, its thinness does not show) along P."""
    P, N = np.asarray(P, float), np.asarray(N, float)
    verts, faces = [], []
    for i in range(len(P)):
        T = _n(P[min(i + 1, len(P) - 1)] - P[max(i - 1, 0)])
        B = _n(np.cross(T, N[i]))
        verts += [P[i] + B * width / 2, P[i] - B * width / 2]
    for i in range(len(P) - 1):
        faces.append((2 * i, 2 * i + 2, 2 * i + 3, 2 * i + 1))
    g._add(verts, faces, paint)


def yoke_mesh(m, lean=False):
    """The A-yoke a side (a strap from in front of the ear and one from behind it, out from under the rim, joined at a
    buckle under the ear, then one strap on to the chin cup) and the leather cup over the point of his chin."""
    J = m["J"]
    body = m["body"]
    me = body.data
    names = [mt.name.split(".")[0] for mt in me.materials]
    V = [tuple(body.matrix_world @ v.co) for v in me.vertices]
    F = [tuple(p.vertices) for p in me.polygons if names[p.material_index] == "skin"]
    bvh = BVHTree.FromPolygons(V, F)
    sh = SHELL["shell"]
    e = np.array(J["eye_l"])
    chin = np.array(J["chin"])
    centre = np.array((J["skull_x"], 0.0, chin[2] + 0.07))

    def on_skin(q, off=YOKE_OFF):
        q = np.asarray(q, float)
        d = _n(q - centre)
        hit, nrm, _, _ = bvh.ray_cast(Vector(centre + d * 0.25), Vector(-d), 0.3)
        if hit is None:
            raise RuntimeError("the yoke missed his face")
        n = _n(np.array(nrm))
        if n @ d < 0:
            n = -n
        return np.array(hit) + n * off, n
    g = S.Geo()
    ear_x, ear_z = e[0] - 0.088, e[2] - 0.03
    cup_c = chin + np.array((0.008, 0.0, 0.012))
    for side in (1.0, -1.0):
        f_side = side * math.pi / 2
        r_rim, z_rim = sh.rim(f_side)
        knot = np.array((ear_x + 0.012, side * 0.07, ear_z - 0.058))
        for dx in (0.038, -0.046):                                    # (before and behind the ear)
            top = np.array((ear_x + dx, side * 0.07, z_rim + 0.012))
            path = [top + (knot - top) * a for a in np.linspace(0.0, 1.0, 3 if lean else 4)]
            P, N = zip(*(on_skin(q) for q in path))
            ribbon(g, P, N, YOKE_W, "straps")
        cup_side = cup_c + np.array((-0.012, side * 0.024, 0.004))
        path = [knot + (cup_side - knot) * a for a in np.linspace(0.0, 1.0, 3 if lean else 5)]
        P, N = zip(*(on_skin(q) for q in path))
        ribbon(g, P, N, YOKE_W, "straps")
        if lean:
            continue
        k0, kn = on_skin(knot, YOKE_OFF + 0.002)
        g.box(k0, (0.0028, 0.011, 0.009), "steel", frame_from(kn, (0.0, 0.0, 1.0)))       # (the buckle)
    # the cup: a lattice over the chin's point, 2.5 mm off the skin, with its edge turned down to the skin
    cu, cv = (3, 2) if lean else (4, 3)
    grid = []
    for j in range(cv + 1):
        row = []
        for i in range(cu + 1):
            a = -0.028 + 0.056 * i / cu                             # (across the chin)
            b = -0.02 + 0.034 * j / cv                              # (from under it up over its front)
            q = cup_c + np.array((0.03 * (b + 0.02) / 0.034 - 0.004, a, -0.016 + 0.03 * j / cv))
            p, n = on_skin(q, 0.0)
            row.append((p, n))
        grid.append(row)
    top = [[p + n * 0.0028 for p, n in row] for row in grid]
    bot = [[p + n * 0.0008 for p, n in row] for row in grid]
    V2, F2 = [], []
    idx = lambda j, i, t: (j * (cu + 1) + i) * 2 + t
    for j in range(cv + 1):
        for i in range(cu + 1):
            V2 += [top[j][i], bot[j][i]]
    for j in range(cv):
        for i in range(cu):
            F2.append((idx(j, i, 0), idx(j, i + 1, 0), idx(j + 1, i + 1, 0), idx(j + 1, i, 0)))
    edge = [(0, i) for i in range(cu + 1)] + [(j, cu) for j in range(1, cv + 1)] + \
           [(cv, i) for i in range(cu - 1, -1, -1)] + [(j, 0) for j in range(cv - 1, 0, -1)]
    for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
        F2.append((idx(j0, i0, 1), idx(j1, i1, 1), idx(j1, i1, 0), idx(j0, i0, 0)))
    g._add(V2, F2, "boots")                                      # (russet leather, as the boots)
    return g.build("yoke_LOD1" if lean else "yoke", sharp=50.0, recalc=True)


# ================================================================================================ the uniform's records
_uniform_layers_inner = S.uniform_layers


def _keep_records(man, J, spec, LM):
    layers = _uniform_layers_inner(man, J, spec, LM)
    if U._ours(spec):
        J["para_pockets"] = LM.get("para_pockets", [])
        J["para_cargo"] = LM["cargo"]
        J["para_coat_belt"] = LM["coat_belt"]
        J["para_tie_z"] = LM["tie_z"]
        J["usa_shoulder_straps_kept"] = LM.get("shoulder_straps", [])
    return layers


S.uniform_layers = _keep_records


# ================================================================================================ him
def assemble():
    """The paratrooper, dressed, armed and rigged (soldier.py's assemble through soldier_usa.py's), and the helmet's
    A-yoke and chin cup laid on his face: a piece of its own (`yoke`, rigid on the head), not the helmet's, so that a
    helmet knocked off (variety.py, by the helmet's paints) leaves no strap on it and takes none of his face."""
    U.with_reload(True)
    m = S.assemble(NATION, "rifle", seed=SEED)
    yk = yoke_mesh(m)
    S.skin_rigid(yk, "head")
    S.attach(yk, m["arm"])
    S.mark_rest(yk)
    m["kit"].append(yk)
    return m


# ================================================================================================ levels of detail
# soldier.py's thinning for everything but what would fall to pieces under it: the helmet is thinned without its net
# and scrim, and the net drawn again for LOD1 (its cords farther apart and wider; none at LOD2); the flag and the
# riggers' patches are drawn again as coarser lattices at LOD1 and left off at LOD2 (the tie-downs are near only); the
# yoke is left off at LOD1 and LOD2 (a chin strap is not seen from there). Their triangles come off soldier.py's target, so each level stays at its budget.
_make_lods = S.make_lods
FAR_TARGET = 760           # (LOD2: a little over soldier.py's 600, as the budget allows: the far man keeps his shape)
REDRAWN = tuple(PATCHES) + ("yoke",)


def make_lods(m, lod):
    redrawn = [o for o in m["kit"] if o.name.split(".")[0] in REDRAWN]
    if not redrawn:
        return _make_lods(m, lod)
    helmet = m["helmet"]
    plain = helmet.copy()
    plain.data = helmet.data.copy()
    bpy.context.scene.collection.objects.link(plain)
    names = [mt.name.split(".")[0] for mt in plain.data.materials]
    bm = bmesh.new()
    bm.from_mesh(plain.data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if names[f.material_index] in HELMET_KIT], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(plain.data)
    bm.free()
    extras = []
    if lod == 1:
        net = net_mesh(1)
        S.skin_rigid(net, "head")
        extras = [net] + [patch_mesh(name, 1) for name in PATCHES if 1 in PATCHES[name]["lods"]]
    extra = sum(S.tris_of(o) for o in extras)
    old = S.LOD_TARGET[lod]
    S.LOD_TARGET[lod] = FAR_TARGET - extra if lod == 2 else old - extra
    try:
        out = _make_lods(dict(m, helmet=plain, kit=[k for k in m["kit"] if k not in redrawn]), lod)
    finally:
        S.LOD_TARGET[lod] = old
        bpy.data.objects.remove(plain)
    hl = next(o for o in out if o.name.startswith("helmet"))
    for o in extras:
        if o.name.startswith("net"):
            for q in bpy.context.selected_objects:
                q.select_set(False)
            o.select_set(True)
            hl.select_set(True)
            bpy.context.view_layer.objects.active = hl
            with bpy.context.temp_override(active_object=hl, selected_editable_objects=[hl, o], selected_objects=[hl, o]):
                bpy.ops.object.join()
            continue
        if not o.name.startswith("yoke"):
            S.skin_like(o, m["body"])
        S.clean_weights(o)
        S.attach(o, m["arm"])
        out.append(o)
    print(f"  para: LOD{lod}: {sum(S.tris_of(o) for o in out)} triangles ({extra} of them drawn again for it)")
    return out


S.make_lods = make_lods


# ================================================================================================ the game's file
# The rifle is in his hands in every clip, so the weapon bone's track (and the operating rod's, on the bolt bone, which
# rides it) is keyed 60 times a second in the json, as the squad leader's magazine is: keyed 15 times a second its
# straight-line steps between keys would part it from the hands in the quick moves (the reload, the throw, the recoil).
_clips_json = S.clips_json
HELD = ("weapon", "bolt")


def clips_json(rig, rate=15):
    out = _clips_json(rig, rate)
    for name, c in out.items():
        L = c["long"]
        n = int(round(L * 60))
        times = [k / 60.0 for k in range(n + 1)]
        if times[-1] < L - 1e-6:
            times.append(L)
        for b in HELD:
            tr = next((x for x in c["tracks"] if x["part_name"] == b), None)
            if tr is None:
                continue
            p = rig.parent[b]
            rots, moves = [], []
            for tc in times:
                st = S.clip_state(rig, name, tc, c["weapon"] if c["weapon"] in ("rifle", "lmg") else "rifle")
                q = S._flip_q(Matrix((st["D"][p].T @ st["D"][b]).tolist()).to_quaternion())
                if rots and sum(x * y for x, y in zip(rots[-1][1:], q)) < 0.0:
                    q = -q
                d = st["D"][p].T @ (st["H"][b] - st["H"][p]) - (rig.H0[b] - rig.H0[p])
                rots.append([round(tc, 4)] + [round(float(v), 5) for v in q])
                moves.append([round(tc, 4), round(float(d[0]), 4), round(float(-d[1]), 4), round(float(d[2]), 4)])
            if "rot" in tr:
                tr["rot"] = rots
            if "move" in tr:
                tr["move"] = moves
    return out


S.clips_json = clips_json


ABOUT = (" He is the American paratrooper of the Normandy battles (June 1944): soldier_usa.py's rifleman in body, skeleton"
         " (the same 32 bones in the same order), the 5 sockets, the 19 clip names and the Garand's reload_stand,"
         " reload_kneel and reload_prone, with his own face (MakeHuman's mix 171) and his own dress and kit. The M1942"
         " parachute jumper's coat and trousers in their own light khaki shade (paints tunic and trousers): two large"
         " slanted bellows pockets on the chest and two on the skirt, each flap with two snaps; the coat's own cloth belt;"
         " the trousers' bellows cargo pockets on the thighs with their tie-downs; the riggers' darker canvas over the"
         " knees and the elbows (paint 20, reinforce). Russet jump boots laced high, the trousers bloused over them, no"
         " leggings. The M2 helmet: the M1's shell with its fixed D-bales, under a net of dark cord (paint 21, net) with"
         " strips of burlap scrim (paints 13 blanket and 20 reinforce); the liner's A-yoke and the leather chin cup are"
         " their own piece (`yoke`, rigid on the head, paints straps, leather and steel), so the helmet's triangles are"
         " those of `helmet_paints` (5, 21, 13, 20) on the head bone. The flag on his right sleeve (12.7 x 7.6 cm, seven"
         " red and six white stripes in paints 16 and 15, the blue canton in paint 22 flag_blue, at the top toward his"
         " back), skinned with the sleeve; no division patch. Kit: the rifleman's cartridge belt (the ten clip pockets the"
         " reload reaches into, three a side in front on pouch_l and pouch_r), suspenders, canteen and first-aid pouch;"
         " the M1936 musette bag high on his back (rigid on chest); a cloth bandoleer of six pockets over his left"
         " shoulder across his chest to his right side; his grenades out of sight in the coat's pockets; the M3"
         " trench knife in its leather scabbard strapped to his lower right leg (rigid on calf_r); the"
         " gas-mask bag strapped upside down to his left thigh (rigid on thigh_l). The weapon bone's and the bolt bone's"
         " tracks are keyed 60 times a second (the rifle is in his hands); every other track 15 times a second.")


def about(data):
    data["name"] = "Battle paratrooper (airborne rifleman), USA, Normandy 1944 (our own design)"
    data["about"] = data["about"].replace("Made by Tools/battle_units/soldier.py.",
                                          "Made by Tools/battle_units/soldier_usa_para.py on soldier_usa.py and soldier.py.")
    data["about"] += ABOUT
    data["colours"] = dict(S.LOOK[NATION])
    data["new_paints"] = dict(NEW_PAINTS)
    data["helmet_paints"] = [S.FIGURE_PAINTS[k] for k in ("helmet", "net", "blanket", "reinforce")]
    return data


def helmet_share(data):
    """The helmet's triangles as variety.py would take them (helmet_paints, every corner on the head bone, the middle
    over 1.5 m), and any triangle of those paints on the head bone left out by that rule."""
    hb = [b["name"] for b in data["bones"]].index("head")
    hp = data["helmet_paints"]
    out = {}
    for lod in ("near", "mid", "far"):
        m = data[lod]
        P = np.array(m["points"]).reshape(-1, 3)
        pa = np.array(m["paints"])
        part = np.array(m["part"])
        T = np.array(m["triangles"]).reshape(-1, 3)
        head = (part[T] == hb).all(1)
        hpt = np.isin(pa[T], hp).all(1)
        high = P[T].mean(1)[:, 2] > 1.5
        out[lod] = {"helmet": int((hpt & head & high).sum()), "helmet_paints_on_head_low": int((hpt & head & ~high).sum()),
                    "head_other_paints_high": int((~hpt & head & high & ~np.isin(pa[T], [S.FIGURE_PAINTS["skin"]]).all(1)).sum())}
    return out


def export_man(info, m):
    t0 = time.time()
    socks = m["sockets"]
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    bpy.context.scene.frame_set(0)
    rec = info.setdefault(BASE, {})
    rec["tris"] = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    for lod in lods:
        rec[f"parts_{lod}"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[lod]}
    rec["short"] = {k: float(v) for k, v in m["short"].items()}
    S.export_glb(S.stage_path(f"{BASE}.glb"), [m["arm"]] + lods[0] + socks)
    for lod in (1, 2):
        S.export_glb(S.stage_path(f"{BASE}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
    data = S.write_game_json(S.stage_path(f"{BASE}.json"), m, lods)
    data = about(data)
    with open(S.stage_path(f"{BASE}.json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    rec["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
    rec["share"] = helmet_share(data)
    rec["high"] = data["high"]
    rec["sockets"] = data["sockets"]
    rec["bones"] = [b["name"] for b in data["bones"]]
    rec["clips"] = {k: [v["long"], v["loop"], [(e["t"], e["fx"], e.get("socket"), e["cue"]) for e in v["events"]]]
                    for k, v in data["clips"].items()}
    for lod in (1, 2):
        for o in lods[lod]:
            bpy.data.objects.remove(o)
    print(f"  {BASE} written in {time.time() - t0:.0f} s: {rec['tris']}; json {rec['json']}; helmet {json.dumps(rec['share'])}")
    return [f"{BASE}.glb", f"{BASE}_LOD1.glb", f"{BASE}_LOD2.glb", f"{BASE}.json"]


def export_para(info):
    t0 = time.time()
    kit.fresh(S.FPS)
    m = assemble()
    S.bake_clips(m)
    files = export_man(info, m)
    info["export_s"] = round(time.time() - t0)
    info["files"] = files
    return m


# ================================================================================================ the picture
SHOTS = [("front", "idle", 0.5, (0.0, 0.0, 0.95), 4.3, 6.0, 28.0),
         ("back", "idle", 0.5, (0.0, 0.0, 0.95), 4.3, 8.0, 205.0),
         ("kneel", "fire_kneel", 0.5, (0.15, 0.0, 0.7), 3.6, 6.0, 55.0),
         ("reload", "reload_stand", 0.85, (0.05, 0.0, 1.1), 3.0, 5.0, 330.0),
         ("legs", "idle", 0.5, (0.0, 0.0, 0.42), 1.9, 8.0, 300.0)]
LABELS = ["American paratrooper at ease: jump coat, M2 helmet under its net, the flag on his right sleeve",
          "From behind: the musette bag high on his back, the bandoleer, the knife and the gas-mask bag on his legs",
          "Firing kneeling (fire_kneel)",
          "Reloading standing (reload_stand): a clip from the belt's pocket into the open action",
          "Close: jump boots laced high, trousers bloused; the trench knife and the gas-mask bag strapped on"]


def picture(info, fast=False, shots=None):
    """pictures/soldier_USA_para.png: at ease from the front quarter, from behind, firing kneeling, reloading, and
    close on his legs' kit."""
    size = (520, 640, 12) if fast else (900, 1080, 36)
    kit.fresh(S.FPS)
    m = assemble()
    S.studio_smooth(m)
    rig = m["rig"]
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(*size)
    ims = []
    for key, name, t, aim, dist, pitch, yaw in (shots or SHOTS):
        st = S.clip_state(rig, name, t, "rifle")
        S.pose_once(m["arm"], rig, st, 0)
        kit.orbit(cam, aim, dist, pitch, yaw)
        ims.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"para_{key}.png")))
    if shots:
        return ims
    out = S.stage_path("pictures", f"{BASE}.png")
    kit.sheet(ims, out, cols=len(ims) if len(ims) <= 3 else 3, labels=LABELS,
              title="American paratrooper, Normandy 1944: soldier_usa_para.py (our own design)")
    info["picture"] = out
    return out


def look(fast=True):
    """Test stills: at ease from the front quarter, the side, behind, and close on the legs and helmet."""
    size = (520, 640, 10)
    kit.fresh(S.FPS)
    m = assemble()
    S.studio_smooth(m)
    rig = m["rig"]
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(*size)
    out = []
    P = S.base_stand()
    S.pose_once(m["arm"], rig, P, 0)
    for key, aim, dist, pitch, yaw in (("front", (0.0, 0.0, 0.95), 4.4, 6.0, 25.0), ("side", (0.0, 0.0, 0.95), 4.4, 6.0, 270.0),
                                       ("back", (0.0, 0.0, 0.95), 4.4, 8.0, 200.0), ("legs", (0.0, 0.0, 0.45), 2.0, 8.0, 300.0),
                                       ("head", (0.05, 0.0, 1.55), 1.1, 4.0, 330.0), ("chest", (0.05, 0.0, 1.2), 1.6, 4.0, 15.0)):
        kit.orbit(cam, aim, dist, pitch, yaw)
        out.append(kit.still(os.path.join(S.SCRATCH, "look", f"{key}.png")))
    for o in (m["weapon"], m["weapon_part"]):                     # (the chest and the knees without the rifle before them)
        o.hide_render = True
    for key, aim, dist, pitch, yaw in (("bare", (0.0, 0.0, 1.1), 2.6, 4.0, 340.0), ("knee", (0.05, -0.05, 0.4), 1.2, 6.0, 30.0),
                                       ("helm", (0.0, 0.0, 1.62), 0.75, 18.0, 140.0)):
        kit.orbit(cam, aim, dist, pitch, yaw)
        out.append(kit.still(os.path.join(S.SCRATCH, "look", f"{key}.png")))
    kit.sheet(out, os.path.join(S.SCRATCH, "look", "sheet.png"), cols=3, labels=[os.path.basename(p) for p in out])
    return m


def main():
    a = kit.args()
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export", "picture"]
    p = S.stage_path(f"export_info_{BASE}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "look":
            look(fast)
        elif step == "export":
            export_para(info)
        elif step == "picture":
            picture(info, fast)
        with open(p, "w") as f:
            json.dump(info, f, indent=1, default=str)
        print(f"step {step}: {time.time() - t0:.0f} s", flush=True)


if __name__ == "__main__":
    main()
