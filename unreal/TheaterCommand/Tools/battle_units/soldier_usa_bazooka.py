"""The American bazooka team of the Normandy battles (summer 1944): the gunner with the 2.36-inch rocket launcher, and
his loader with the M1 carbine and a bag of rockets on his back. Made beside soldier_usa.py and soldier.py (both stay
as they are).

    python soldier_usa_bazooka.py --man gunner            build, rig, animate and export the gunner
    python soldier_usa_bazooka.py --man loader            ... the loader (each man in his own process: see below)
    python soldier_usa_bazooka.py --only picture          the sheet of the two together
    (--keep: stage only, nothing copied to the shared folder; --fast: a small test picture)

Run it in a checkout of claude/project-thread-8egity's Tools/battle_units (soldier.py, soldier_usa.py, human.py,
mocap.py, kit.py and MakeHuman's base files beside it), with the bpy module (Blender 5.0.1), as soldier_usa.py is run.

Both men are soldier_usa.py's rifleman in body, dress and rig: the same 32 bones, 5 sockets and 19 clip names. What
is their own is laid on by wrapping soldier.py's and soldier_usa.py's makers, and by swapping, per man, the tables
soldier.py reads for "the rifle" (WEAPONS, BUTT_AT, the sights' measures, the mocap carries, the clips): so each man
is made in his own process (`become` sets one man's tables; the picture sets them in turn as it poses each).

- The gunner (soldier_USA_bazooka): the rocket launcher of 1944 (our own drawing: a 1.37 m tube of 60 mm bore with a
  flared rear, a wooden shoulder stock holding the batteries, a trigger grip and a front grip, the sights on a
  bracket on the left, the contact spring at the rear), on the right shoulder. A pistol belt (no rifle's pockets: he
  has no rifle) with the pistol in its holster on the right hip and its magazine pocket in front. His own fire_stand,
  fire_kneel and fire_prone, one rocket every 9 s; mocap carries with the tube on his shoulder; his own go_prone,
  cower and die_kneel; lmg_burst and throw_grenade kept harmless (no shots, no grenade).
- The loader (soldier_USA_loader): the M1 carbine (our own drawing, on the rifle's frame), semi-automatic like the
  Garand; carbine magazine pockets on his pistol belt; the rocket bag on his back; his own load_bazooka, timed to the
  gunner's fire_kneel (the "pair" in both files says where he kneels). The rocket he loads rides his bolt bone, and
  lies hidden in the bag in every other clip.
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def _arg_man():
    a = sys.argv
    if "--man" in a:
        return a[a.index("--man") + 1]
    return os.environ.get("BAZOOKA_MAN", "gunner")


import soldier_usa as U  # noqa: E402  (soldier.py and the American's wrappers)
S = U.S
import bpy  # noqa: E402
import numpy as np  # noqa: E402
import kit  # noqa: E402
import mocap  # noqa: E402
from sculpt import Ellipsoid, Layer, RoundBox, RoundCone, frame_from, on_surface, rot_euler  # noqa: E402

NATION = "USA"
S.SCRATCH = "/home/user/art/bazooka/"
S.STAGE = os.path.join(S.SCRATCH, "out")
MEN = {"gunner": dict(base="soldier_USA_bazooka", seed=118, who="bazooka gunner"),
       "loader": dict(base="soldier_USA_loader", seed=146, who="bazooka loader")}

# ---- new paints (soldier.py's are 0-14; 15-16 are the medic's, 17 the squad leader's): the rocket's own olive drab,
# so the game can tell a round from the helmet's paint
S.FIGURE_PAINTS["rocket"] = 18
S.LOOK[NATION]["rocket"] = "5D5F3B"
NEW_PAINTS = {"rocket": 18}

R_axis, M4, frame2, _n = S.R_axis, S.M4, S.frame2, S._n
ramp, bump, osc, add3 = S.ramp, S.bump, S.osc, S.add3


# ================================================================================================ the launcher
# Made in soldier.py's weapon frame (x to the muzzle, y left, z up) with its origin on the sights' line (y = 0, where
# the solver puts the right eye) level with the tube's axis, over the trigger grip: the tube's axis runs TY to the
# right of the sights' line. The right hand's grip is the trigger grip, the left hand's the front grip; the shoulder
# bears under the stock's rear (BUTT_AT_BZ: the point that goes to the shoulder's pocket).
TY, TR, BORE = -0.08, 0.0335, 0.030
X_REAR, X_MUZZLE = -0.76, 0.61          # 1.37 m
SIGHT_Z_BZ = 0.024
FRONT_X = 0.32                          # the front grip, ahead of the trigger grip
GRIP_Z = -0.088                         # the fist's middle on either grip, under the tube's axis
RAKE = 8.0                              # the grips' rake, back from upright (deg)
REAR_SIGHT_X, FRONT_SIGHT_X = 0.02, 0.30
CONTACT_X = X_REAR + 0.105              # the contact spring and the latch, on top at the rear
SHOULDER_REST = (-0.235, TY, -0.093)    # the stock's underside where it lies on the shoulder
BUTT_AT_BZ = (-0.165, TY + 0.05, -0.128)  # the shoulder's pocket from there (forward 7.5, in 5, down 3.5 cm)
GRIP_LINE = (-math.sin(math.radians(RAKE)), 0.0, -math.cos(math.radians(RAKE)))   # down each grip


def _ring(x, r, n=16, y=TY, z=0.0):
    return [(x, y + r * math.cos(2 * math.pi * (i + 0.5) / n), z + r * math.sin(2 * math.pi * (i + 0.5) / n)) for i in range(n)]


def build_launcher():
    """The 2.36-inch rocket launcher of 1944, our own drawing of it: the steel tube (1.37 m, a 60 mm bore) with a
    reinforced muzzle and a flared rear, the wooden shoulder stock under it (the batteries in a box in its left side),
    the trigger grip and the front grip, the rear sight and the framed front sight on brackets out to the left, bands
    at the grips and the stock, and the wire from the stock back to the contact spring. Returns (launcher, latch): the
    latch (the contact spring's housing on top at the rear) rides the bolt bone and never moves."""
    st, od = "steel", "helmet"                  # (the tube in the olive drab the helmet wears)
    g = S.Geo()
    # ---- the tube as one closed skin: the bore's dark hollow at the muzzle, the reinforced lip, the tube, the flare
    # at the rear and its hollow; drawn from the muzzle's hollow round to the rear's
    rings = [[(X_MUZZLE - 0.07, TY, 0.0)], _ring(X_MUZZLE - 0.07, BORE), _ring(X_MUZZLE, BORE), _ring(X_MUZZLE, TR + 0.002),
             _ring(X_MUZZLE - 0.028, TR + 0.002), _ring(X_MUZZLE - 0.03, TR), _ring(X_REAR + 0.055, TR),
             _ring(X_REAR + 0.02, 0.043), _ring(X_REAR, 0.051), _ring(X_REAR, 0.047), _ring(X_REAR + 0.03, BORE + 0.002),
             _ring(X_REAR + 0.09, BORE), [(X_REAR + 0.09, TY, 0.0)]]
    g.shell(rings, od)
    # bands round the tube: at the stock's two ends, the trigger grip and the front grip
    for x, w in ((-0.415, 0.014), (-0.05, 0.012), (0.0, 0.03), (FRONT_X, 0.026)):
        g.tube((x - w / 2, TY, 0.0), (x + w / 2, TY, 0.0), TR + 0.0028, TR + 0.0028, od, 16)
    # ---- the shoulder stock: wood under the rear of the tube, deep at its rear where it lies on the shoulder (its
    # underside hollowed to fit it), shallow forward; the batteries' box let into its left side
    wood = Layer("bz_stock", "wood")
    wood.add(RoundBox((-0.275, TY, -0.06), (0.13, 0.0235, 0.03), 0.012))
    wood.add(RoundBox((-0.1, TY, -0.045), (0.06, 0.02, 0.016), 0.01), 0.03)
    wood.sub(Ellipsoid((-0.235, TY, -0.152), (0.09, 0.07, 0.062)), 0.01)
    wood.add(RoundBox((-0.25, TY, -0.0255), (0.165, 0.016, 0.008), 0.006), 0.012)     # (its seat under the tube)
    # ---- the grips: the trigger grip under the tube's middle, raked back; the front grip ahead
    grip = Layer("bz_grip", "wood")
    grip.add(RoundBox((-0.003, TY, GRIP_Z - 0.006), (0.019, 0.0145, 0.05), 0.011, rot_euler(0, RAKE, 0)))
    grip.add(RoundBox((FRONT_X - 0.002, TY, GRIP_Z - 0.002), (0.017, 0.0145, 0.044), 0.011, rot_euler(0, RAKE - 4.0, 0)))
    stock = S.mesh_from_layers("bz_stock", [wood, grip], 0.0025, 420)
    # the grips' brackets, the trigger and its guard, the batteries' box
    g.box((0.004, TY, -TR - 0.009), (0.03, 0.012, 0.009), st)
    g.box((FRONT_X + 0.002, TY, -TR - 0.008), (0.022, 0.011, 0.008), st)
    g.box((0.034, TY, -0.06), (0.003, 0.0022, 0.011), st, rot_euler(0, 10, 0))               # trigger
    loop = [(0.026 + 0.03 * math.cos(t), TY, -0.05 - 0.028 * math.sin(t)) for t in np.linspace(0.0, math.pi - 0.5, 6)]
    for a, b in zip(loop[:-1], loop[1:]):
        g.tube(a, b, 0.003, 0.003, st, 4, caps=False)                                           # its guard
    g.box((-0.29, TY + 0.0245, -0.062), (0.055, 0.0018, 0.019), st)                             # batteries' box lid
    g.box((-0.29, TY + 0.0262, -0.062), (0.006, 0.0012, 0.008), st)                             # its catch
    # ---- the sights out on the left: the rear sight's aperture leaf, and the front sight's frame with its range bars
    for x, h in ((REAR_SIGHT_X, 0.011), (FRONT_SIGHT_X, 0.012)):
        y0 = TY + TR - 0.004
        g.box((x, (y0 + 0.004) / 2, SIGHT_Z_BZ - 0.022), (0.0065, (0.004 - y0) / 2, 0.0035), st)   # bracket arm
        g.box((x, TY + TR + 0.002, -0.004), (0.009, 0.003, 0.02), st)                            # its foot on the tube
    g.box((REAR_SIGHT_X, 0.0, SIGHT_Z_BZ - 0.009), (0.0018, 0.0075, 0.0095), st)                # rear leaf
    g.box((REAR_SIGHT_X, 0.0, SIGHT_Z_BZ + 0.0015), (0.0022, 0.0035, 0.0012), st)               # (its aperture's rim)
    fx = FRONT_SIGHT_X
    for dy in (-0.016, 0.016):
        g.box((fx, dy, SIGHT_Z_BZ), (0.0016, 0.0016, 0.022), st)                                # the frame's sides
    for dz in (-0.022, 0.022):
        g.box((fx, 0.0, SIGHT_Z_BZ + dz), (0.0016, 0.0176, 0.0016), st)                         # its top and foot
    for dz in (-0.012, -0.004, 0.004):
        g.box((fx, 0.0, SIGHT_Z_BZ + dz), (0.0008, 0.014, 0.0008), st)                          # range bars
    g.box((fx, 0.0, SIGHT_Z_BZ), (0.0008, 0.0008, 0.02), st)                                    # the middle post
    # ---- the wire from the stock's rear back along the top right of the tube to the contact spring
    wire = [(-0.405, TY - 0.016, -0.03), (-0.44, TY - 0.03, -0.012), (-0.52, TY - 0.031, 0.008),
            (CONTACT_X + 0.03, TY - 0.024, 0.024), (CONTACT_X + 0.01, TY - 0.014, TR + 0.004)]
    for a, b in zip(wire[:-1], wire[1:]):
        g.tube(a, b, 0.0024, 0.0024, "leather", 5)
    g.absorb(stock)
    launcher = g.build("rifle", sharp=50.0)
    # ---- the latch and the contact spring on top at the rear (on the bolt bone: still)
    b = S.Geo()
    b.box((CONTACT_X, TY, TR + 0.006), (0.022, 0.011, 0.007), st)
    b.box((CONTACT_X - 0.03, TY, TR + 0.004), (0.012, 0.006, 0.004), st)
    spring = [(CONTACT_X - 0.012, TY - 0.006, TR + 0.013), (CONTACT_X - 0.035, TY - 0.006, TR + 0.018),
              (CONTACT_X - 0.06, TY - 0.004, TR + 0.012), (CONTACT_X - 0.07, TY, TR + 0.004)]
    for a_, b_ in zip(spring[:-1], spring[1:]):
        b.tube(a_, b_, 0.0022, 0.0022, st, 5)
    latch = b.build("rifle_bolt")
    return launcher, latch


LAUNCHER = {"grip_r": (0.0, TY, GRIP_Z), "grip_l": (FRONT_X, TY, GRIP_Z), "front": (FRONT_X, TY, GRIP_Z),
            "butt": SHOULDER_REST, "rear": (X_REAR, TY, 0.0), "sight_z": SIGHT_Z_BZ, "bolt": (CONTACT_X, TY, TR),
            "carry": (-0.06, TY, 0.016), "tube_mid": (-0.06, TY, 0.0)}
MUZZLE_BZ = (X_MUZZLE, TY, 0.0)
BACKBLAST = [round(X_REAR - X_MUZZLE, 3), 0.0, 0.0]      # the rear's offset from the muzzle socket, along the weapon


# ================================================================================================ the rocket
ROCKET_LONG = 0.54
ROCKET_R = 0.0295


def build_rocket(name="rocket"):
    """The 2.36-inch rocket, our own drawing of it: a rounded ogive head on a short body (its widest, 59 mm, sliding in
    the 60 mm bore), tapering to the slim motor tube, and the fin assembly at the tail inside its ring, with the
    igniter wire's lead. 0.54 m. In its own frame: its axis x, the nose +x, its middle at the origin."""
    n = 14
    ring = lambda x, r: [(x, r * math.cos(2 * math.pi * (i + 0.5) / n), r * math.sin(2 * math.pi * (i + 0.5) / n)) for i in range(n)]
    nose, body0, body1, mot0, tail = 0.27, 0.13, 0.075, 0.035, -0.205
    prof = [(nose - 0.004, 0.004), (nose - 0.02, 0.0125), (nose - 0.045, 0.0205), (nose - 0.08, 0.0265), (body0, ROCKET_R),
            (body1, ROCKET_R), (body1 - 0.012, 0.026), (mot0, 0.0185), (mot0 - 0.01, 0.0175), (tail, 0.0175), (tail - 0.012, 0.0155),
            (-0.262, 0.0155), (-0.268, 0.009)]
    g = S.Geo()
    rings = [[(nose, 0.0, 0.0)]] + [ring(x, r) for x, r in prof] + [[(-0.268, 0.0, 0.0)]]
    g.shell(rings, "rocket")
    for k in range(4):                                     # the fins, in their ring
        a = math.pi / 4 + k * math.pi / 2
        d = np.array((0.0, math.cos(a), math.sin(a)))
        c = np.array((-0.236, 0.0, 0.0)) + d * 0.0225
        g.box(c, (0.026, 0.0008, 0.0055), "steel", frame_from((1.0, 0.0, 0.0), np.cross((1.0, 0.0, 0.0), d)))
    g.tube((-0.258, 0.0, 0.0), (-0.226, 0.0, 0.0), 0.0285, 0.0285, "steel", n, caps=False)
    g.tube((-0.256, 0.0, 0.0), (-0.228, 0.0, 0.0), 0.0272, 0.0272, "steel", n, caps=False)
    lead = [(-0.268, 0.004, 0.0), (-0.29, 0.012, 0.004), (-0.3, 0.02, 0.0)]
    for a_, b_ in zip(lead[:-1], lead[1:]):
        g.tube(a_, b_, 0.0018, 0.0018, "leather", 4)
    return g.build(name, sharp=50.0)


# ================================================================================================ the carbine
# In soldier.py's rifle frame (origin where the right hand grips the wrist, x to the muzzle, y left, z up, the bore 4.2
# cm over the grip, the sights' line at 7 cm), shorter than the rifle: the butt plate at -0.29, the muzzle at 0.585.
CARBINE_BUTT = -0.291
CARBINE_MUZZLE = (0.585, 0.0, 0.042)


def build_carbine():
    """The M1 carbine, our own drawing of it: a slim one-piece walnut stock with a semi-pistol grip, a short handguard
    over the barrel, the barrel band with its swivel, the round receiver with the operating slide on its right and the
    flip aperture rear sight on its heel, the fifteen-round magazine under it ahead of the trigger guard, the front
    sight between its ears, and the sling down the stock's left side. 0.904 m. Returns (carbine, rocket): the rocket
    he loads rides the bolt bone (see clip_state)."""
    wood = Layer("stock", "wood")
    wood.add(RoundBox((-0.205, 0.0, -0.022), (0.086, 0.019, 0.051), 0.015, rot_euler(0, -4, 0)))      # butt
    wood.add(RoundBox((-0.11, 0.0, -0.008), (0.05, 0.0175, 0.03), 0.013), 0.03)                        # into the wrist
    wood.add(RoundCone((-0.085, 0.0, -0.001), (0.03, 0.0, 0.012), 0.0172, 0.0165), 0.018)             # the wrist
    wood.add(Ellipsoid((-0.019, 0.0, -0.028), (0.026, 0.0158, 0.03)), 0.02)                            # semi-pistol grip
    wood.add(RoundBox((0.105, 0.0, 0.016), (0.1, 0.0172, 0.026), 0.012), 0.02)                          # under the receiver
    wood.add(RoundBox((0.27, 0.0, 0.024), (0.085, 0.0162, 0.018), 0.012), 0.02)                        # fore-end
    wood.add(RoundCone((0.18, 0.0, 0.05), (0.33, 0.0, 0.0495), 0.0118, 0.0112), 0.004)               # handguard
    carbine = S.mesh_from_layers("rifle_stock", [wood], 0.0025, 300)
    g = S.Geo()
    st = "steel"
    g.box((CARBINE_BUTT, 0.0, -0.022), (0.0025, 0.0195, 0.052), st, rot_euler(0, -4, 0))              # butt plate
    g.tube((0.02, 0.0, 0.045), (0.178, 0.0, 0.045), 0.0152, 0.0148, st, 14)                            # receiver
    g.box((0.034, 0.0, 0.0625), (0.009, 0.0085, 0.0045), st)                                           # rear sight base
    g.box((0.033, 0.0, 0.0705), (0.0022, 0.0062, 0.0055), st)                                          # its flip leaf
    g.tube((0.178, 0.0, 0.042), (0.585, 0.0, 0.042), 0.0098, 0.0088, st, 12)                           # barrel
    g.tube((0.334, 0.0, 0.036), (0.358, 0.0, 0.036), 0.0222, 0.0222, st, 14, squash=1.12)              # barrel band
    g.box((0.346, 0.0, 0.006), (0.006, 0.004, 0.007), st)                                              # its swivel
    g.box((0.57, 0.0, 0.0625), (0.0035, 0.0016, 0.0075), st)                                           # front sight
    for side in (1.0, -1.0):
        g.box((0.57, 0.0075 * side, 0.06), (0.0055, 0.0016, 0.0085), st)                               # its ears
    g.box((0.12, -0.0175, 0.043), (0.05, 0.0028, 0.0055), st)                                          # operating slide
    g.box((0.164, -0.023, 0.044), (0.008, 0.005, 0.0055), st)                                          # its handle
    g.box((0.088, 0.0, -0.046), (0.017, 0.0118, 0.041), st, rot_euler(0, -5, 0))                       # magazine
    g.box((0.084, 0.0, -0.0875), (0.019, 0.013, 0.0025), st, rot_euler(0, -5, 0))                      # its floor plate
    g.box((0.035, 0.0, -0.0175), (0.04, 0.0128, 0.0035), st)                                           # guard plate
    loop = [(0.0 + 0.025 * math.cos(t), 0.0, -0.018 - 0.018 * math.sin(t)) for t in np.linspace(0.15, math.pi - 0.15, 6)]
    for a, b in zip(loop[:-1], loop[1:]):
        g.tube(a, b, 0.003, 0.003, st, 4, caps=False)                                                  # trigger guard
    g.box((0.011, 0.0, -0.021), (0.003, 0.002, 0.009), st)                                             # trigger
    sl = S.spline([(0.346, 0.018, 0.004), (0.16, 0.0205, -0.022), (-0.04, 0.022, -0.04), (-0.17, 0.0215, -0.02)], 10)
    g.band(sl, [(0.0, 1.0, 0.0)] * len(sl), 0.024, 0.0028, "straps")                                  # sling, on the left
    g.absorb(carbine)
    carbine = g.build("rifle")
    return carbine, build_rocket("rocket")


CARBINE = {"grip_r": (0.0, 0.0, 0.0), "grip_l": (0.29, 0.0, 0.006), "butt": (CARBINE_BUTT, 0.0, -0.024), "sight_z": 0.07,
           "bolt": (-0.064, -0.058, 0.02), "carry": (0.13, 0.0, -0.004)}
BUTT_AT_CARBINE = (CARBINE_BUTT - 0.002, 0.0, -0.058)
# the rocket in the loader's hands, as a "weapon" of its own while he loads (its grips, in its own frame)
ROCKET_GRIPS = {"grip_r": (-0.2, 0.0, 0.0), "tail": (-0.215, 0.0, 0.0), "body": (0.0, 0.0, 0.0), "head": (0.1, 0.0, 0.0),
                "sight_z": 0.0, "butt": (-0.27, 0.0, 0.0), "bolt": (0.0, 0.0, 0.0), "carry": (0.0, 0.0, 0.0)}
S.WEAPONS["rocket"] = dict(ROCKET_GRIPS)
S.REAR_NOTCH["rocket"], S.COMB_Z["rocket"] = 0.0, 0.0


# ================================================================================================ their belts and bags
# Neither man carries the M1 rifle, so neither wears its cartridge belt's ten pockets: the pistol belt (the same width
# of webbing) carries the gunner's pistol and its magazines, or the loader's carbine magazines. The canteen and the
# first-aid pouch stay. The loader's rocket bag takes the haversack's place on his back.
_usa_build_kit = S.build_kit
NOW = {"man": None}


def _rings(J, body_ls, Z):
    tunic = body_ls[0]
    zb = Z(1.0)
    cx = J["spine_01"][0] + 0.03 if "crown" in J else 0.0
    rings, _ = U._taut({side: U._waist_ring(tunic, cx, zb, side) for side in (1.0, -1.0)}, cx)
    return rings, zb, cx


def _on_belt(rings, side, u, half, drop, out_by=0.011):
    """A pocket's centre and frame hung on the belt `u` metres round the waist from the buckle on a side: its top
    `drop` under the belt's top, standing `out_by` off the webbing."""
    P, N, s = rings[side]
    p0, n0 = U._at_length(P, N, s, u)
    R = frame_from(n0, (0.0, 0.0, 1.0))
    return p0, n0, R


def _gap(c, R, half, bc, bh):
    """The least distance from a box (centre, frame, half sizes) to an upright one (centre, half sizes), over a grid
    of the first's points."""
    g = np.linspace(-1.0, 1.0, 5)
    pts = np.array([c + R @ (np.array((i, j, k)) * half) for i in g for j in g for k in g])
    return float(np.min(np.linalg.norm(np.maximum(np.abs(pts - bc) - bh, 0.0), axis=1)))


def build_kit(J, spec, body_ls, Z=None):
    out = _usa_build_kit(J, spec, body_ls, Z)
    man = NOW["man"]
    if not U._ours(spec) or man is None:
        return out
    Z = Z or (lambda z: z)
    tunic = body_ls[0]
    for k in ("pouches", "belt_pockets"):
        if k in out:
            bpy.data.objects.remove(out.pop(k)[0])
    rings, zb, cx = _rings(J, body_ls, Z)
    top = zb + U.BELT_W / 2 + 0.004
    front = []
    hang = {}
    if man == "gunner":
        # the pistol's two magazines in their pocket on the front left of the belt (on the pouch bone, which still
        # hangs at the right too: every man has the same 32 bones)
        pockets = [(1.0, 0.085, (0.0125, 0.034, 0.055))]
    else:
        # a pocket of two carbine magazines either side of the buckle
        pockets = [(1.0, 0.085, (0.0165, 0.04, 0.06)), (-1.0, 0.085, (0.0165, 0.04, 0.06))]
    for side, u, half in pockets:
        p0, n0, R = _on_belt(rings, side, u, half, 0.0)
        c = np.array((p0[0], p0[1], top - half[2] + 0.004)) + n0 * (0.011 + half[0])
        L = Layer(f"pocket_{side:+.0f}", "straps")
        L.add(RoundBox(c, half, 0.004, R), 0.0)
        L.add(RoundBox(c + R @ np.array((0.003, 0.0, half[2] - 0.016)), (half[0], half[1] + 0.0016, 0.0165), 0.003, R), 0.0015)
        if man == "gunner":                           # (two magazines: two flaps, each with its snap)
            L.sub(RoundBox(c + R @ np.array((half[0] + 0.003, 0.0, half[2] - 0.03)), (0.0035, 0.0016, 0.016), 0.001, R), 0.0)
        front.append(L)
    for side in (1.0, -1.0):
        P, N, s = rings[side]
        p0, n0 = U._at_length(P, N, s, 0.085)
        hang[side] = np.array((p0[0], p0[1], top - 0.012)) + n0 * 0.012
        J["pouch_pivot_" + ("l" if side > 0 else "r")] = tuple(float(v) for v in hang[side])
    out["pouches"] = (S.mesh_from_layers("pouches", front, 0.002, 160 * len(front)), None)
    if man == "gunner":
        # ---- the pistol in its holster on the right hip, a little behind the side seam: the leather holster with
        # its flap over the butt, hung from the belt by its hook; the tie-down thong round the thigh is left off
        # (it stands off the waist by the hip's swell below the belt, and takes the body's weights where it lies, the
        # hip's at its mouth and the thigh's at its toe, as the tie-down would hold it: fit_check found a holster on the
        # pelvis alone 4 to 9 cm inside the thigh whenever the leg came forward)
        p0, n0, R = _on_belt(rings, -1.0, 0.24, None, 0.0)
        c = np.array((p0[0], p0[1], zb - 0.07)) + n0 * 0.051
        L = Layer("holster", "leather")
        L.add(RoundBox(c + R @ np.array((0.0, 0.0, 0.02)), (0.0165, 0.052, 0.07), 0.012, R @ rot_euler(-6, 0, 0)))
        L.add(RoundBox(c + R @ np.array((0.0, -0.016, -0.075)), (0.014, 0.027, 0.05), 0.01, R @ rot_euler(-6, 0, 0)), 0.012)
        flap = Layer("holster_flap", "leather")
        flap.add(RoundBox(c + R @ np.array((0.004, 0.0, 0.07)), (0.017, 0.056, 0.03), 0.012, R), 0.0)
        hook = Layer("holster_hook", "steel")
        hook.add(RoundBox(c + R @ np.array((-0.026, 0.0, 0.1)), (0.012, 0.012, 0.02), 0.003, R))    # (out from the belt)
        stud = Layer("holster_stud", "steel")
        stud.add(RoundBox(c + R @ np.array((0.022, 0.0, 0.052)), (0.003, 0.006, 0.006), 0.002, R))
        out["holster"] = (S.mesh_from_layers("holster", [L, flap, hook, stud], 0.0025, 230), None)
    else:
        # ---- the rocket bag in the haversack's place: tall canvas, from the belt up to the shoulder blades' top, a
        # flap over its mouth with two straps; the rocket he loads lies in it nose up (BAG_ROCKET, in the chest's
        # frame), and he draws it out over his right shoulder. The canteen and the first-aid pouch go round behind the
        # hips, clear of its foot.
        for k in ("pack", "shovel", "bottle", "aid_pouch"):
            if k in out:
                bpy.data.objects.remove(out.pop(k)[0])
        lo, hi = Z(0.955), Z(1.515)
        backs = []
        for z in np.linspace(lo, hi, 9):
            for y in (-0.06, 0.0, 0.06):
                p, _ = on_surface(tunic, (-0.4, y, z), (1.0, 0.0, 0.0))
                backs.append(p[0])
        belt_back = float(rings[1.0][0][-1][0]) - 0.012           # (the belt's webbing, straight behind)
        bx = min(min(backs) - 0.014, belt_back - 0.008)
        hx, hy, hz = 0.047, 0.078, (hi - lo) / 2
        c = np.array((bx - hx, 0.0, (lo + hi) / 2))
        L = Layer("rocket_bag", "pack")
        L.add(RoundBox(c, (hx, hy, hz), 0.026))
        flap = Layer("rocket_bag_flap", "pack")
        flap.add(RoundBox(c + np.array((-0.003, 0.0, hz - 0.05)), (hx + 0.004, hy + 0.004, 0.058), 0.02))
        band = Layer("rocket_bag_bands", "straps")
        for dy in (-0.045, 0.045):
            band.add(RoundBox(c + np.array((-hx - 0.002, dy, hz - 0.075)), (0.004, 0.011, 0.07), 0.003))
        band.add(RoundBox(c + np.array((0.0, 0.0, -0.12)), (hx + 0.003, hy + 0.003, 0.012), 0.011))
        out["rocket_bag"] = (S.mesh_from_layers("rocket_bag", [L, flap, band], 0.004, 330), "chest")
        J["rocket_bag"] = {"c": [float(v) for v in c], "half": [hx, hy, hz]}
        # the canteen behind the right hip, the first-aid pouch behind the left: each as far back as the rifleman's
        # allow, but 3.5 cm clear of the bag's foot (it hangs below the belt between them)
        for side, key in ((-1.0, "bottle"), (1.0, "aid_pouch")):
            P, N, s = rings[side]
            drop, off, half = ((0.082, 0.042, (0.036, 0.066, 0.096)) if key == "bottle" else (0.035, 0.024, (0.02, 0.054, 0.05)))
            u = s[-1] - (0.066 if key == "bottle" else 0.07)
            while u > 0.2:
                p0, n0 = U._at_length(P, N, s, u)
                R = frame_from(n0, (0.0, 0.0, 1.0))
                c = np.array((p0[0], p0[1], zb - U.BELT_W / 2 - drop)) + n0 * off
                if _gap(c, R, np.array(half), np.array(J["rocket_bag"]["c"]), np.array(J["rocket_bag"]["half"])) >= 0.035:
                    break
                u -= 0.005
            print(f"    {key}: {u:.3f} m round the waist from the buckle (the back's middle at {s[-1]:.3f})")
            # (there the buttock swells under it: each hangs a little off the hip and swung out at its foot about its
            # hook, as fit_check asked: 8 and 6 degrees)
            top, tilt, push = (0.1, 8.0, 0.008) if key == "bottle" else (0.048, 6.0, 0.01)
            ct, st_ = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))
            Rt = R @ np.array(((ct, 0.0, -st_), (0.0, 1.0, 0.0), (st_, 0.0, ct)))
            c = c + R @ np.array((0.0, 0.0, top)) - Rt @ np.array((0.0, 0.0, top)) + n0 * push
            R = Rt
            if key == "bottle":
                Lc = Layer("canteen", "pack")
                fl = Layer("canteen_flap", "pack")
                hk = Layer("canteen_hook", "steel")
                Lc.add(RoundBox(c, (0.033, 0.064, 0.084), 0.028, R))
                Lc.sub(Ellipsoid(c - R[:, 0] * 0.085, (0.06, 0.1, 0.14), R), 0.012)
                fl.add(RoundBox(c + R @ np.array((0.003, 0.0, 0.07)), (0.034, 0.066, 0.026), 0.012, R))
                hk.add(RoundBox(c + R @ np.array((-0.03, 0.0, 0.1)), (0.012, 0.012, 0.02), 0.003, R))
                out["bottle"] = (S.mesh_from_layers("bottle", [Lc, fl, hk], 0.0025, 200), "pelvis")
            else:
                La = Layer("aid_pouch", "pack")
                La.add(RoundBox(c, (0.017, 0.052, 0.045), 0.01, R))
                La.add(RoundBox(c + R @ np.array((0.004, 0.0, 0.028)), (0.016, 0.054, 0.02), 0.006, R), 0.003)
                out["aid_pouch"] = (S.mesh_from_layers("aid_pouch", [La], 0.0025, 110), "pelvis")
    return out


S.build_kit = build_kit


# ================================================================================================ the gunner's holds
# The right hand round the trigger grip, the left round the front grip: power grips, the palm on the grip's outer side
# (facing in), the hand's axis forward across the grip and a little in, each free to roll round its grip's line; the
# fists sit a little high on the grips (the hand's axis level or a touch up), which keeps the wrists straight with the
# forearms rising to the grips (rom.py's ulnar deviation).
def _hold(a, n):
    F = frame2(a, n)
    return tuple(float(v) for v in F[:, 0]), tuple(float(v) for v in F[:, 1])


A_R, N_R = _hold((0.9, 0.3, 0.1), (-0.3, 1.0, 0.0))
A_L, N_L = _hold((0.9, -0.3, 0.0), (-0.3, -1.0, 0.0))
HOLD_BZ = {
    "r": {"grip": "grip_r", "a": A_R, "n": N_R, "pole_world": (-0.3, -1.0, -0.75), "roll": 25.0, "roll_axis": GRIP_LINE},
    "l": {"grip": "grip_l", "a": A_L, "n": N_L, "pole_world": (0.25, 0.5, -1.0), "roll": 25.0, "roll_axis": GRIP_LINE},
}
# the same grips in mocap's rifle frame (x right, y barrel, z up) for the captured clips' carries
_to_mo = lambda v: (-float(v[1]), float(v[0]), float(v[2]))
GRIP_R_BZ = (_to_mo(A_R), _to_mo(N_R))
GRIP_L_BZ = (_to_mo(A_L), _to_mo(N_L))


def bz_hands(P, **over_l):
    P["hand_r"] = dict(HOLD_BZ["r"])
    P["hand_l"] = dict(HOLD_BZ["l"], **over_l)
    P.update(fingers_l=35.0, fingers_r=35.0, thumb_l=30.0, thumb_r=35.0)
    return P


CARRY_LIFT = 0.035           # (at ease the tube rides on top of the shoulder, not in its pocket: fit_check found the
#                              stock 4 to 5 cm into the right upper arm with it in the pocket)


def on_shoulder(pitch=0.0, yaw=0.0, cant=0.0, lift=0.0):
    """The launcher on the right shoulder (its stock's rear on it, soldier.py's aim with the launcher's BUTT_AT),
    pitched up `pitch` degrees; `lift` metres up off the pocket, square to the tube."""
    place = S.aim(yaw=yaw, pitch=pitch, cant=cant)
    if not lift:
        return place

    def lifted(rig, st):
        W = np.array(place(rig, st), float)
        W[:3, 3] += W[:3, :3] @ np.array((0.0, 0.0, lift))
        return W
    return lifted


ORIG_FN = {}                 # (soldier.py's own pose makers, kept before the gunner's replace them)


def base_stand_bz(t=0.0):
    """At ease: the launcher on the right shoulder, sloped up 15 degrees, both hands on its grips, the head upright
    and a little to the left of the tube."""
    P = ORIG_FN["base_stand"](t)
    P = bz_hands(P)
    P.update(weapon=on_shoulder(15.0, lift=CARRY_LIFT), sight=0.0, clavicle_r=(6.0, 4.0), clavicle_l=(2.0, 8.0), neck=(-2.0, 2.0, 0.5),
             head=(-1.0, 3.0, 0.5))
    return P


def aim_stand_bz(t=0.0):
    P = ORIG_FN["aim_stand"](t)
    P = bz_hands(P)
    P.update(clavicle_r=(12.0, 6.0), clavicle_l=(0.0, 14.0))
    return P


def kneel_bz(t=0.0, aimed=True):
    P = ORIG_FN["kneel_base"](t, aimed=True)
    P = bz_hands(P, pole_world=(0.3, 0.3, -1.0))
    P.update(clavicle_r=(12.0, 6.0))
    if not aimed:
        P.update(weapon=on_shoulder(15.0, lift=CARRY_LIFT), sight=0.0, neck=(-2.0, 2.0, 6.0), head=(-2.0, 3.0, 8.0), clavicle_r=(6.0, 4.0),
                 clavicle_l=(0.0, 6.0), chest=(8.0, 0.0, 4.0), spine_02=(5.0, 0.0, 4.0))
    return P


PRONE_TURN = -16.0           # (degrees more to the right than the rifleman lies: his legs out of the backblast)


def _turn_feet(P, deg, about):
    for s in ("l", "r"):
        f = dict(P["foot_" + s])
        at = np.array(f["at"], float)
        d = R_axis((0, 0, 1), deg) @ (at - np.array((about[0], about[1], 0.0)))
        f["at"] = tuple(float(v) for v in d + np.array((about[0], about[1], 0.0)))
        f["yaw"] = f.get("yaw", 0.0) + deg
        if "pole" in f:
            f["pole"] = tuple(float(v) for v in R_axis((0, 0, 1), deg) @ np.array(f["pole"], float))
        P["foot_" + s] = f
    return P


def prone_bz(t=0.0, arch=0.5):
    """Lying to fire, turned well to the right of the line of fire so his legs lie out of the blast behind the tube."""
    P = ORIG_FN["prone_base"](t, arch=arch)
    P = bz_hands(P, pole_world=(0.0, 0.3, -1.0))
    pv = P["pelvis"]
    P["pelvis"] = (pv[0], pv[1], pv[2] + PRONE_TURN)
    P = _turn_feet(P, PRONE_TURN, P["pelvis_at"][:2])
    P["chest"] = add3(P["chest"], (0.0, 0.0, -0.5 * PRONE_TURN))
    P["spine_02"] = add3(P["spine_02"], (0.0, 0.0, -0.25 * PRONE_TURN))
    P["clavicle_r"] = (18.0, -6.0)
    return P


# ================================================================================================ the gunner's clips
FIRE_LONG_BZ = 9.0           # one rocket every 9 s: the launch, then the hold while his loader loads
LAUNCH = 0.4
TAP = 7.0                    # (his loader's tap on the helmet: loaded, clear)
REAIM = (7.25, 8.1)


def jolt(P, t, shot=LAUNCH, body=1.0):
    """The launch: no recoil to speak of (the rocket's motor burns out in the tube), but a jolt and the blast's slap:
    the tube nudged back and its muzzle up a little, the head flinching, then steady again."""
    if t < shot:
        return P
    u = t - shot
    k = (1 - math.exp(-u / 0.01)) * math.exp(-u / 0.09)
    P["weapon"] = S._kicked(P["weapon"], 0.012 * k, 2.2 * k)
    P["chest"] = add3(P.get("chest", (0, 0, 0)), (-1.4 * k * body, 0.0, -1.0 * k * body))
    P["clavicle_r"] = add3(P.get("clavicle_r", (0, 0)), (1.0 * k * body, -3.0 * k * body))[:2]
    P["head"] = add3(P.get("head", (0, 0, 0)), (-2.5 * k, 0.0, 0.0))
    return P


def hold_cycle(P, t):
    """After the launch: the eye eased off the sight while the loader works behind him (the head up a little, the
    tube kept on the mark), the loader's tap on the helmet, and back onto the sight for the next."""
    off = ramp(t, 1.0, 1.8) * (1.0 - ramp(t, REAIM[0], REAIM[1]))
    P["sight"] = 1.0 - 0.55 * off
    P["neck"] = add3(P.get("neck", (0, 0, 0)), (-3.0 * off, 0.0, -1.5 * off))
    P["head"] = add3(P.get("head", (0, 0, 0)), (-4.0 * off + 2.5 * bump(t, TAP, TAP + 0.05, TAP + 0.22), 0.0, 0.0))
    return P


BZ_EVENTS = [S.ev(LAUNCH, "rocket_launch", "socket_muzzle", "bazooka_fire"),
             S.ev(LAUNCH, "backblast", "socket_muzzle", None, offset=BACKBLAST, along="weapon")]


def c_fire_stand_bz(t):
    """Standing: the launcher on the shoulder, the eye at the sight; the launch, the jolt; the hold while it is
    loaded; one rocket every 9 s."""
    P = aim_stand_bz(t)
    P = S.breathe(P, t, 4.5, 0.4)
    return hold_cycle(jolt(P, t), t)


def c_fire_kneel_bz(t):
    P = kneel_bz(t)
    P = S.breathe(P, t, 4.5, 0.4)
    return hold_cycle(jolt(P, t), t)


def c_fire_prone_bz(t):
    P = prone_bz(t)
    P = S.breathe(P, t, 4.5, 0.3)
    return hold_cycle(jolt(P, t, body=0.6), t)


def c_lmg_burst_bz(t):
    """(No machine gun: lying on the aim, the launcher held on the mark. No shots.)"""
    return S.breathe(prone_bz(t), t, 1.2, 0.3)


def c_throw_bz(t):
    """(No grenade in his hands: he keeps the launcher on his shoulder and looks left and right. No events.)"""
    look = bump(t, 0.1, 0.45, 0.8) - bump(t, 0.75, 1.1, 1.5)
    P = base_stand_bz()
    P["neck"] = add3(P["neck"], (0.0, 0.0, 14.0 * look))
    P["head"] = add3(P["head"], (-1.0 * abs(look), 0.0, 16.0 * look))
    P["chest"] = add3(P.get("chest", (0, 0, 0)), (0.0, 0.0, 4.0 * look))
    return S.breathe(P, t, 1.6, 0.6)


HIT_HEAD = ((-12.0, 8.0, 8.0), (-10.0, 8.0, 10.0))       # (his neck and head as he lands: up and turned left, off the tube)
HIT_YAW = 0.0


def c_go_prone_bz(t):
    """Down on the right knee, then forward onto the elbows, the launcher kept on the shoulder and its muzzle up out
    of the dirt, both hands on it; then turned onto the aim."""
    knee = kneel_bz(aimed=False)
    knee.update({"pelvis_at": (-0.12, 0.0, -0.47), "spine_01": (10.0, 0, 0), "spine_02": (12.0, 0, 0), "chest": (12.0, 0, 0),
                 "foot_l": {"at": (0.32, 0.16, S.ANKLE), "yaw": -5.0, "pole": (1.0, 0.25, 0.3)},
                 "foot_r": {"at": (-0.35, -0.18, 0.105), "yaw": -20.0, "pitch": 70.0, "toe": 55.0, "pole": (1.0, -0.2, -0.6)},
                 "weapon": on_shoulder(20.0, lift=CARRY_LIFT)})
    fall = {"pelvis_at": (0.0, 0.02, -0.6), "pelvis": (55.0, 0.0, -10.0), "spine_01": (4.0, 0, 0), "spine_02": (4.0, 0, 0),
            "chest": (2.0, 0, 0), "neck": (-20.0, 0, 0), "head": (-15.0, 3.0, 0), "clavicle_l": (4.0, 14.0), "clavicle_r": (8.0, 6.0),
            "foot_l": {"at": (-0.3, 0.2, 0.2), "yaw": 5.0, "pitch": 60.0, "toe": 30.0, "pole": (0.3, 0.3, -1.0)},
            "foot_r": {"at": (-0.5, -0.2, 0.12), "yaw": -15.0, "pitch": 75.0, "toe": 50.0, "pole": (0.3, -0.3, -1.0)},
            "weapon": on_shoulder(10.0, lift=0.025)}
    fall = bz_hands(fall)
    hit = prone_bz(arch=0.0)
    hit.update({"pelvis_at": (0.0, 0.03, S.PRONE_Z - 0.01), "neck": HIT_HEAD[0], "head": HIT_HEAD[1],
                "weapon": on_shoulder(2.0, yaw=HIT_YAW, lift=0.03), "sight": 0.0})
    return S.keyed([(0.0, base_stand_bz()), (0.32, knee), (0.58, fall), (0.75, hit), (1.0, prone_bz())])(t)


def c_cower_bz(t):
    """Wavering: down on his heels, head pulled in, the launcher held across his knees by both grips, flinching at
    each near miss and peering up between them."""
    P = ORIG_FN["cower"](t)
    flinch = math.exp(-((t - 0.7) % 3.0) / 0.12) * ramp((t - 0.7) % 3.0, 0.0, 0.03) + \
        0.7 * math.exp(-((t - 2.05) % 3.0) / 0.12) * ramp((t - 2.05) % 3.0, 0.0, 0.03)
    R = frame_from(_n((0.12, 1.0, -0.06)), (0.0, 0.0, 1.0)) @ R_axis((1, 0, 0), 6.0 * flinch)
    grip = np.array((0.36 - 0.02 * flinch, -0.12, 0.47 - 0.015 * flinch))
    P["weapon"] = M4(R, grip - R @ np.array(LAUNCHER["grip_r"]))
    P = bz_hands(P)
    P["hand_r"] = dict(P["hand_r"], pole_world=(0.0, -1.0, -0.4), roll=45.0)
    P["hand_l"] = dict(P["hand_l"], pole_world=(0.0, 1.0, -0.4), roll=45.0)
    P.update(fingers_l=55.0, fingers_r=55.0)
    return P


def c_die_kneel_bz(t):
    """soldier.py's die_kneel with the launcher on his shoulder at the start, both hands on it (it goes from his
    hands as he is hit, falls and lies on its side: KEYED_DROP)."""
    stand = base_stand_bz()
    hunch = dict(base_stand_bz(), chest=(18.0, 0.0, 0.0), spine_02=(12.0, 0.0, 0.0), spine_01=(8.0, 0.0, 0.0), neck=(10.0, 0, 0),
                 head=(12.0, 0, 0), pelvis_at=(0.0, 0.0, -0.06), weapon=S.drop_rifle((0.4, -0.25, 0.5), 20.0, 40.0),
                 hand_l={"arm": (24.0, 60.0, 42.0, 105.0, 30.0, 25.0, 0.0)}, hand_r={"arm": (24.0, 60.0, 42.0, 105.0, 30.0, 25.0, 0.0)},
                 fingers_l=40.0, fingers_r=40.0)
    knees = {"pelvis_at": (-0.12, 0.0, -0.46), "pelvis": (10.0, 0.0, 0.0), "spine_01": (10.0, 0, 0), "spine_02": (14.0, 0, 0),
             "chest": (16.0, 0.0, 0.0), "neck": (14.0, 0, 0), "head": (14.0, 0, 0), "clavicle_l": (-4.0, 10.0), "clavicle_r": (-4.0, 10.0),
             "foot_l": {"at": (-0.5, 0.15, 0.105), "yaw": 10.0, "pitch": 72.0, "toe": 60.0, "pole": (1.0, 0.1, -0.5)},
             "foot_r": {"at": (-0.5, -0.15, 0.105), "yaw": -10.0, "pitch": 72.0, "toe": 60.0, "pole": (1.0, -0.1, -0.5)},
             "weapon": S.drop_rifle((0.5, -0.3, 0.03), 25.0),
             "hand_l": hunch["hand_l"], "hand_r": hunch["hand_r"], "fingers_l": 40.0, "fingers_r": 40.0}
    sway = dict(knees, pelvis=(14.0, -4.0, 4.0), chest=(20.0, -4.0, 0.0), head=(20.0, -6.0, 0.0))
    down = {"pelvis_at": (0.05, 0.08, -0.8), "pelvis": (84.0, -10.0, 14.0), "spine_01": (2.0, 0, 0), "spine_02": (2.0, -2.0, 0),
            "chest": (2.0, -4.0, 0.0), "neck": (-4.0, 0, -30.0), "head": (0.0, -10.0, -40.0), "clavicle_l": (4.0, 8.0), "clavicle_r": (4.0, 8.0),
            "foot_l": {"at": (-0.78, 0.27, 0.09), "yaw": 30.0, "pitch": 95.0, "toe": 10.0, "pole": (0.0, 0.3, -1.0)},
            "foot_r": {"at": (-0.74, -0.14, 0.09), "yaw": -10.0, "pitch": 95.0, "toe": 10.0, "pole": (0.0, -0.3, -1.0)},
            "weapon": S.drop_rifle((0.5, -0.3, 0.03), 25.0),
            "hand_l": {"arm": (115.0, -4.0, -35.0, 25.0, 50.0, 10.0, 0.0)},
            "hand_r": {"arm": (35.0, -6.0, 20.0, 30.0, 50.0, 10.0, 0.0)}, "fingers_l": 30.0, "fingers_r": 35.0}
    bounce = dict(down, pelvis_at=(0.05, 0.08, -0.77), chest=(-3.0, -4.0, 0.0))
    return S.keyed([(0.0, stand), (0.22, hunch), (0.45, knees), (0.75, sway), (1.0, down), (1.12, bounce), (1.5, down)])(t)


def _gun_falls_bz(Dg, Pg, held, t, t0, fps, throw, body, lie_yaw=None):
    """soldier.py's _gun_falls for the launcher: it comes to lie on its right side (the grips and the stock flat
    along the ground, the sights up), the tube's skin on the ground."""
    i0 = int(round(t0 * fps))
    v = (Pg[i0 + 1] - Pg[i0 - 1]) * fps / 2 + np.array(throw)
    p0, R0 = Pg[i0].copy(), Dg[i0].copy()
    g = 9.81
    lie = TR + 0.002            # (the trigger grip's middle is level with the tube's axis, lying so)
    T = max((v[2] + math.sqrt(max(v[2] ** 2 + 2 * g * (p0[2] - lie), 0.0))) / g, 0.25)
    yaw = math.degrees(math.atan2(R0[1, 0], R0[0, 0])) + 25.0 if lie_yaw is None else lie_yaw
    Rg = R_axis((0, 0, 1), yaw) @ R_axis((1, 0, 0), 90.0)
    land = S._clear_of(body, p0[:2] + v[:2] * T * 0.7, Rg, clear=0.3)
    for f in range(i0 + 1, len(t)):
        tau = t[f] - t[i0]
        if tau < T:
            u = tau / T
            Pg[f, :2] = p0[:2] + (land - p0[:2]) * u
            Pg[f, 2] = max(p0[2] + v[2] * tau - 0.5 * g * tau * tau, lie)
            Dg[f] = mocap.slerp_mats(R0, Rg, S.sm(u))
        else:
            Pg[f] = (land[0], land[1], lie)
            Dg[f] = Rg
    held["r"] = held["r"] * (1.0 - S._sm_arr((t - t0) / 0.1))
    return [S.ev(round(t0 + T, 3), "rifle_drop", cue="kit_rattle")]


def carry_bz(pitch, lift=0.0, **more):
    """A mocap carry (mocap.RIFLE_POSES' terms) with the launcher on the right shoulder, sloped up `pitch` degrees;
    its grip's place is worked out on each man from his shoulder's pocket (see mo_prepare)."""
    c = dict(twist=0.0, lean=0.0, steady=0.5, follow=1.0, grip_l=GRIP_L_BZ, left=FRONT_X, pole_r=(-0.35, -0.75, -0.6),
             pole_l=(0.15, 0.45, -1.0), clav_r=(6.0, 4.0), clav_l=(2.0, 8.0), bz_pitch=pitch, bz_lift=lift)
    c.update(more)
    return c


CARRIES = {"idle": carry_bz(15.0, CARRY_LIFT), "walk": carry_bz(15.0, CARRY_LIFT), "run": carry_bz(18.0, CARRY_LIFT, steady=0.6),
           "advance": carry_bz(8.0, steady=0.65, follow=0.6), "creep": carry_bz(8.0, steady=0.65, follow=0.5),
           "die_forward": carry_bz(15.0), "die_back": carry_bz(15.0, steady=0.0)}

_mo_prepare = S.mo_prepare


def mo_prepare(rig, name):
    spec = S.MOCAP.get(name, {})
    c = spec.get("carry")
    if isinstance(c, dict) and "bz_pitch" in c and name not in rig.mo:
        R = R_axis((0, 0, 1), 0.0) @ R_axis((0, 1, 0), -c["bz_pitch"])
        at = np.array(rig.pocket0) + R @ (np.array(LAUNCHER["grip_r"]) - np.array(BUTT_AT_BZ) + np.array((0.0, 0.0, c.get("bz_lift", 0.0))))
        c["at"] = tuple(float(v) for v in at)
        c["barrel"] = tuple(float(v) for v in R[:, 0])
        c["up"] = tuple(float(v) for v in R[:, 2])
    return _mo_prepare(rig, name)


S.mo_prepare = mo_prepare


# ================================================================================================ the loader's clips
# (load_bazooka and the pair: see below, after the gunner's track)
def c_lmg_burst_ld(t):
    """(No machine gun: lying on the aim with the carbine, no shots.)"""
    return S.breathe(ORIG_FN["prone_base"](t), t, 1.2, 0.3)


# ================================================================================================ one man at a time
BASE = {}


def _snapshot():
    """soldier.py's tables as soldier_usa.py leaves them (the rifleman's), kept before either man changes them."""
    BASE["clips"] = [(k, dict(v)) for k, v in S.CLIPS.items()]
    BASE["mocap"] = {k: dict(v) for k, v in S.MOCAP.items()}
    BASE["rifle"] = dict(S.WEAPONS["rifle"])
    BASE["tables"] = dict(BUTT_AT=S.BUTT_AT["rifle"], REAR_NOTCH=S.REAR_NOTCH["rifle"], COMB_Z=S.COMB_Z["rifle"])
    BASE["sight"] = dict(SIGHT_CANT=S.SIGHT_CANT, SIGHT_PITCH=S.SIGHT_PITCH, SIGHT_RELIEF=S.SIGHT_RELIEF)
    BASE["grip_r"] = mocap.GRIP_R
    for k in ("base_stand", "kneel_base", "prone_base", "aim_stand", "_gun_falls"):
        ORIG_FN[k] = getattr(S, k)
    ORIG_FN["cower"] = S.CLIPS["cower"]["fn"]
    ORIG_FN["die_kneel"] = S.CLIPS["die_kneel"]["fn"]
    BASE["build_garand"] = U.build_garand


def _entry(fn, long, loop=False, events=(), weapon="rifle", stride=0.0):
    return {"long": long, "loop": loop, "stride": stride, "events": list(events), "fn": fn, "weapon": weapon}


def become(man):
    """Set soldier.py's tables (and soldier_usa.py's rifle maker) for one man of the team: "gunner" or "loader"."""
    NOW["man"] = man
    S.CLIPS.clear()
    S.CLIPS.update({k: dict(v) for k, v in BASE["clips"]})
    S.MOCAP.clear()
    S.MOCAP.update({k: dict(v) for k, v in BASE["mocap"].items()})
    for k in ("base_stand", "kneel_base", "prone_base", "aim_stand", "_gun_falls"):
        setattr(S, k, ORIG_FN[k])
    for k, v in BASE["sight"].items():
        setattr(S, k, v)
    mocap.GRIP_R = BASE["grip_r"]
    if man == "gunner":
        S.WEAPONS["rifle"] = dict(LAUNCHER)
        S.BUTT_AT["rifle"], S.REAR_NOTCH["rifle"], S.COMB_Z["rifle"] = BUTT_AT_BZ, REAR_SIGHT_X, -0.03
        # (his head on the sight leans further onto the tube than a rifleman's onto his comb)
        S.SIGHT_CANT, S.SIGHT_PITCH = (2.0, 24.0), (6.0, 22.0)
        mocap.GRIP_R = GRIP_R_BZ
        U.build_garand = build_launcher
        S.base_stand, S.kneel_base, S._gun_falls = base_stand_bz, kneel_bz, _gun_falls_bz
        for name, c in CARRIES.items():
            S.MOCAP[name] = dict(S.MOCAP[name], carry=dict(c))
            S.CLIPS[name]["mocap"] = S.MOCAP[name]
        del S.MOCAP["go_prone"]
        S.CLIPS["go_prone"] = _entry(c_go_prone_bz, 1.0, events=[S.ev(0.32, "knee_down", bone="calf_r"),
                                                                 S.ev(0.75, "body_fall", bone="spine_02")])
        for name, fn in (("fire_stand", c_fire_stand_bz), ("fire_kneel", c_fire_kneel_bz), ("fire_prone", c_fire_prone_bz)):
            S.CLIPS[name] = _entry(fn, FIRE_LONG_BZ, loop=True, events=BZ_EVENTS)
        S.CLIPS["lmg_burst"] = _entry(c_lmg_burst_bz, 1.2, loop=True)
        S.CLIPS["throw_grenade"] = _entry(c_throw_bz, 1.6)
        S.CLIPS["cower"] = _entry(c_cower_bz, 3.0, loop=True)
        S.CLIPS["die_kneel"] = _entry(c_die_kneel_bz, 1.5, events=[S.ev(0.45, "knee_down", bone="calf_l"),
                                                                   S.ev(1.0, "body_fall", bone="spine_02")])
    else:
        S.WEAPONS["rifle"] = dict(CARBINE)
        S.BUTT_AT["rifle"], S.REAR_NOTCH["rifle"], S.COMB_Z["rifle"] = BUTT_AT_CARBINE, 0.033, 0.035
        U.build_garand = build_carbine
        S.CLIPS["lmg_burst"] = _entry(c_lmg_burst_ld, 1.2, loop=True)
        S.CLIPS["load_bazooka"] = _entry(c_load_bazooka, FIRE_LONG_BZ, loop=True, events=LOAD_EVENTS)
    return man


def assemble_man(man):
    """One man of the team, dressed, armed and rigged (soldier.py's assemble through soldier_usa.py's), his muzzle
    socket moved to his own weapon's muzzle."""
    become(man)
    m = S.assemble(NATION, "rifle", seed=MEN[man]["seed"])
    m["rig"].bz_man = man
    old = m["sockets"][0]
    g0 = np.array(m["arm"].data.bones["weapon"].head_local)
    at = MUZZLE_BZ if man == "gunner" else CARBINE_MUZZLE
    new = S.add_socket(m["arm"], "muzzle", "weapon", tuple(g0 + np.array(at)))
    bpy.data.objects.remove(old)
    new.name = "socket_muzzle"
    m["sockets"][0] = new
    m["man"] = man
    return m


# ================================================================================================ where the rocket is
def _hand_frame(rig, st, s="r"):
    D = st["D"]["hand_" + s]
    a0, t0, n0 = rig.hand0[s]
    g = st["H"]["hand_" + s] + D @ (S.grip_point(rig.J, s) - rig.H0["hand_" + s])
    return g, D @ np.asarray(a0), D @ np.asarray(n0)


def _set_weapon(rig, st, W):
    st["D"]["weapon"], st["H"]["weapon"], st["W"] = W[:3, :3].copy(), W[:3, 3].copy(), W
    for b in ("bolt", "bipod"):
        st["D"][b] = W[:3, :3].copy()
        st["H"][b] = W[:3, 3] + W[:3, :3] @ (rig.H0[b] - rig.H0["weapon"])


def _set_rocket(rig, st, M):
    """The bolt bone put where the rocket's own frame (its axis x, the nose +x, its middle at the origin) is M."""
    R, o = M[:3, :3], M[:3, 3]
    st["D"]["bolt"] = R.copy()
    st["H"]["bolt"] = o + R @ (rig.H0["bolt"] - rig.H0["weapon"])


BAG_ROCKET = (0.008, -0.04, 0.0)     # the rocket he loads, in the bag: its middle from the bag's (to his right side of it)


def bag_of(rig):
    """The rocket bag's middle and half sizes (his rest frame): build_kit's, or for a rig without his kit
    (arms_check's light rig) the same measured off the chest's joint."""
    bag = rig.J.get("rocket_bag")
    if bag:
        return np.array(bag["c"]), np.array(bag["half"])
    return np.array(rig.H0["chest"]) + np.array((-0.137, 0.0, -0.0272)), np.array((0.047, 0.078, 0.2733))


def bag_place(rig, st):
    """The rocket in the bag on his back, nose up, in the chest's frame (hidden in the bag)."""
    c, _ = bag_of(rig)
    R0 = frame_from((0.0, 0.0, 1.0), (-1.0, 0.0, 0.0))
    D, H = st["D"]["chest"], st["H"]["chest"]
    return M4(D @ R0, H + D @ (c + np.array(BAG_ROCKET) - rig.H0["chest"]))


def drawn_place(rig, st):
    """The rocket as his right fist draws it out of the bag by its nose: along the bag while it is deep in, then
    turning about the bag's mouth as his hand swings forward over his shoulder, until its tail is out."""
    c, half = bag_of(rig)
    D, H = st["D"]["chest"], st["H"]["chest"]
    g, _, _ = _hand_frame(rig, st, "r")
    gl = D.T @ (g - H) + rig.H0["chest"]                    # (the fist in his rest frame)
    axis = c + np.array(BAG_ROCKET)
    q = np.array((axis[0], axis[1], min(gl[2] - 0.2, c[2] + half[2] - 0.03)))
    d = D @ S._n(gl - q)
    R = frame_from(d, D @ np.array((-1.0, 0.0, 0.0)))
    return M4(R, g - R @ np.array(S.WEAPONS["rocket"]["nose"]))


CRAWL_GRIP_X = X_REAR + 0.07       # (crawling, his right fist on the tube's rear end, pushing it ahead of him)


_clip_state = S.clip_state


def clip_state(rig, name, t, kind=None):
    man = getattr(rig, "bz_man", None) or NOW["man"]
    if man == "loader" and name == "load_bazooka":
        out_place(rig)
        st = _clip_state(rig, name, t, "rocket")
        W = st["W"]
        tl = t % FIRE_LONG_BZ
        if T_REACH <= tl < T_OUT:                           # (drawn by his fist: where his hand took it)
            u = S.sm(min((tl - T_REACH) / 0.25, 1.0))
            W = S.blend_places([(1.0 - u, bag_place), (u, drawn_place)])(rig, st)
        elif T_OUT <= tl < T_TAKE and getattr(rig, "bz_hold", None) is not None:   # (out: held in his fist)
            W = _hand_M(st, "r") @ rig.bz_hold
        _set_weapon(rig, st, CARBINE_DOWN)
        _set_rocket(rig, st, W)
        return st
    st = _clip_state(rig, name, t, kind)
    if man == "gunner" and name == "crawl":
        # crawling he pushes the launcher ahead of him on the ground at his right front, muzzle forward, lying on its
        # right side (the grips toward him, the sights up), his right fist round its rear end: it rides that fist,
        # still while the fist is planted and he pulls himself up to it, slid on as the fist reaches forward again
        g, a_w, n_w = _hand_frame(rig, st, "r")
        R = np.array(((1.0, 0.0, 0.0), (0.0, 0.0, -1.0), (0.0, 1.0, 0.0)))
        axis = np.array((g[0], g[1], max(TR + 0.016, g[2] - 0.03)))
        _set_weapon(rig, st, M4(R, axis - R @ np.array((CRAWL_GRIP_X, TY, 0.0))))
    if man == "loader":
        _set_rocket(rig, st, bag_place(rig, st))
    return st


S.clip_state = clip_state


# ================================================================================================ the pair and the load
# The loader kneels at the gunner's right rear, beside the launcher's rear end, facing it: his root PAIR_AT (m) from
# the gunner's root in the gunner's frame (x forward, y left), turned PAIR_YAW degrees to the gunner's left. The
# gunner's fire_kneel and the loader's load_bazooka are one 9 s loop on the same clock: the launch at 0.4 s; the loader
# draws a rocket from his bag over his right shoulder, brings it down across his chest into both hands, offers it at
# the rear end and slides it in nose first, makes the
# wire's contact on the spring, taps the gunner's helmet (TAP, 7.0 s) and leans away, his head out of the backblast,
# before the next launch. In every other clip the rocket lies in his bag.
PAIR_AT = (-0.68, -0.64)
PAIR_YAW = 72.0
LOADED_X = X_REAR + 0.282           # the rocket's middle along the tube when it is in (its tail just inside the rear)
START_X = X_REAR - 0.285            # ... and as he offers it up, its nose at the rear end
TRACK_FPS = 60
TRACK = {}


def _track_key():
    """(The gunner's side of this file, all above the pair: what his track depends on.)"""
    import hashlib
    text = open(os.path.abspath(__file__), encoding="utf-8").read()
    return hashlib.sha1(text.split("# " + "=" * 96 + " the pair and the load")[0].encode()).hexdigest()[:12]


def gunner_track():
    """The gunner's launcher (its frame) and head (turn and place) through fire_kneel, 60 times a second, in the
    gunner's frame: worked out on a rig of the gunner's own (his seed), once, and kept beside the art."""
    if TRACK:
        return TRACK
    path = os.path.join(S.SCRATCH, f"gunner_track_{_track_key()}.npz")
    if os.path.exists(path):
        d = np.load(path)
        TRACK.update({k: d[k] for k in d.files})
        return TRACK
    was = NOW["man"]
    become("gunner")
    keep = set(bpy.data.objects)
    man = S.mh_body(NATION, MEN["gunner"]["seed"])
    J = S.joints_of(man)
    S.fit_body(J)
    S.landmarks(J)
    arm = S.build_armature(J)
    rig = S.Rig(arm, J)
    rig.bz_man = "gunner"
    n = int(round(FIRE_LONG_BZ * TRACK_FPS)) + 1
    W = np.zeros((n, 4, 4))
    HD = np.zeros((n, 3, 3))
    HH = np.zeros((n, 3))
    for k in range(n):
        st = S.clip_state(rig, "fire_kneel", k / TRACK_FPS, "rifle")
        W[k], HD[k], HH[k] = st["W"], st["D"]["head"], st["H"]["head"]
    head0 = np.array(rig.H0["head"])
    for ob in [o for o in bpy.data.objects if o not in keep]:
        bpy.data.objects.remove(ob)
    become(was)
    TRACK.update(W=W, HD=HD, HH=HH, head0=head0)
    os.makedirs(S.SCRATCH, exist_ok=True)
    np.savez(path, **TRACK)
    return TRACK


def to_loader():
    """The gunner's frame into the loader's (4x4)."""
    R = R_axis((0, 0, 1), -PAIR_YAW)
    return M4(R, -R @ np.array((PAIR_AT[0], PAIR_AT[1], 0.0)))


def tube_at(t, local):
    """A point or frame on the gunner's launcher at time t (its own frame: `local` a 3-vector or 4x4), in the
    loader's frame."""
    T = gunner_track()
    u = (t % FIRE_LONG_BZ) * TRACK_FPS
    i = min(int(u), len(T["W"]) - 2)
    a = u - i
    W0, W1 = T["W"][i], T["W"][i + 1]
    W = M4(mocap.slerp_mats(W0[:3, :3], W1[:3, :3], a), W0[:3, 3] + (W1[:3, 3] - W0[:3, 3]) * a)
    M = to_loader() @ W
    if np.ndim(local) == 1:
        return M[:3, :3] @ np.asarray(local, float) + M[:3, 3]
    return M @ local


def helmet_at(t, rest_point):
    """A point on the gunner's helmet (riding his head; `rest_point` from his head's joint at rest), loader's frame."""
    T = gunner_track()
    u = (t % FIRE_LONG_BZ) * TRACK_FPS
    i = min(int(u), len(T["W"]) - 2)
    a = u - i
    D = T["HD"][i] + (T["HD"][i + 1] - T["HD"][i]) * a
    H = T["HH"][i] + (T["HH"][i + 1] - T["HH"][i]) * a
    p = H + D @ np.asarray(rest_point, float)
    L = to_loader()
    return L[:3, :3] @ p + L[:3, 3]


# ---- the rocket's way through the loop (its own frame: axis x, nose +x)
START_TURN = 32.0           # (offered up: its nose at the rear end, its tail swung this far out toward him)


def _rk_tube(t, nose_x, turn=0.0):
    """The rocket on the tube's line with its nose at `nose_x` along the tube, its tail swung `turn` degrees out to
    the tube's right (toward the loader) about the nose."""
    R = R_axis((0, 0, 1), turn) @ R_axis((0, 1, 0), -0.25 * turn)
    nose = np.array((nose_x, TY, 0.0))
    return lambda rig, st: tube_at(t, M4(R, nose - R @ np.array((0.27, 0.0, 0.0))))


def _rk_loaded(t):
    return _rk_tube(t, LOADED_X + 0.27)


# the load's moments (s, on the gunner's clock): his hand at the bag; the rocket's tail out of the bag; his left hand
# on it; it is offered at the rear end; it starts in; it is home
T_REACH, T_OUT, T_LOW, T_TAKE, T_TAKE2, T_START, T_SLIDE, T_IN = 1.6, 2.6, 2.95, 3.15, 3.25, 3.6, 3.8, 4.6
OUT_HOLD = {"a": (0.0, 0.0, -1.0), "n": (0.0, 1.0, 0.0)}   # (his right fist on the nose as it comes out: out_place)


def _hand_M(st, s):
    return M4(st["D"]["hand_" + s], st["H"]["hand_" + s])


def out_place(rig):
    """How his fist holds the rocket once its tail is out of the bag (T_OUT, from the draw itself), and where the
    rocket is when he has brought it down across his chest (T_LOW), in his chest's frame: worked out once per rig, so
    the left hand that takes it there, and the right that keeps it, have it."""
    if getattr(rig, "bz_out", None) is not None or getattr(rig, "bz_busy", False):
        return
    rig.bz_busy = True
    try:
        st = _clip_state(rig, "load_bazooka", T_OUT, "rocket")
        M = drawn_place(rig, st)
        rig.bz_hold = np.linalg.inv(_hand_M(st, "r")) @ M
        st = _clip_state(rig, "load_bazooka", T_LOW, "rocket")
        M = _hand_M(st, "r") @ rig.bz_hold
        rig.bz_out = np.linalg.inv(M4(st["D"]["chest"], st["H"]["chest"])) @ M
        _, a_w, n_w = _hand_frame(rig, st, "r")
        R = M[:3, :3]
        A_, K, T_ = st["H"]["upperarm_r"], st["H"]["lowerarm_r"], st["H"]["hand_r"]
        w = S._n(T_ - A_)
        OUT_HOLD.update(a=tuple(float(v) for v in R.T @ a_w), n=tuple(float(v) for v in R.T @ n_w),
                        pole_world=tuple(float(v) for v in S._n((K - A_) - w * ((K - A_) @ w))))
    finally:
        rig.bz_busy = False


def _out(rig, st):
    if getattr(rig, "bz_out", None) is None:
        return bag_place(rig, st)
    return M4(st["D"]["chest"], st["H"]["chest"]) @ rig.bz_out


def rocket_way(t):
    """The rocket's place at t (a placement: rig, state -> 4x4), and its phase. (While his fist draws it, T_REACH to
    T_OUT, clip_state puts it in his hand after the pose is solved; the bag's place stands in until then.)"""
    t = t % FIRE_LONG_BZ
    if t < LAUNCH:
        return _rk_loaded(t), "loaded"
    if t < T_REACH:
        return bag_place, "bag"
    if t < T_OUT:
        return bag_place, "drawn"
    if t < T_TAKE2:
        return _out, "out"
    if t < T_START:
        u = S.sm((t - T_TAKE2) / (T_START - T_TAKE2))
        return S.blend_places([(1.0 - u, _out), (u, _rk_tube(t, X_REAR - 0.004, START_TURN))]), "offered"
    if t < T_SLIDE:
        return _rk_tube(t, X_REAR - 0.004, START_TURN), "start"
    if t < T_IN:
        u = S.sm((t - T_SLIDE) / (T_IN - T_SLIDE))
        turn = START_TURN * (1.0 - S.sm(u / 0.22))          # (it comes square to the tube as its nose goes in)
        return _rk_tube(t, X_REAR - 0.004 + (LOADED_X + 0.274 - X_REAR) * u, turn), "sliding"
    return _rk_loaded(t), "loaded"


# ---- his body through the load (his own frame), and his hands: on the rocket by its grips, or on the gunner's launcher
# and helmet where the track says they are
def load_kneel():
    """Kneeling on his right knee facing the launcher's rear end, upright, the left foot forward."""
    P = dict(ORIG_FN["kneel_base"](0.0, aimed=False))
    P.update({"pelvis_at": (-0.19, 0.0, -0.475), "pelvis": (8.0, 0.0, -6.0), "spine_01": (4.0, 0.0, 2.0), "spine_02": (5.0, 0.0, 2.0),
              "chest": (6.0, 0.0, 2.0), "neck": (4.0, 0.0, 0.0), "head": (6.0, 0.0, 0.0), "clavicle_l": (0.0, 6.0), "clavicle_r": (0.0, 6.0),
              "foot_l": {"at": (0.3, 0.19, S.ANKLE), "yaw": 4.0, "pole": (1.0, 0.25, 0.3)},
              "foot_r": {"at": (-0.48, -0.17, 0.105), "yaw": -20.0, "pitch": 72.0, "toe": 62.0, "pole": (1.0, -0.2, -0.6)},
              "fingers_l": 35.0, "fingers_r": 35.0, "thumb_l": 30.0, "thumb_r": 30.0, "sight": 0.0,
              "comfort": False})         # (every hold of the load carries its fitted elbow: the solver keeps to it)
    return P


def _on_thigh(s):
    """A hand resting on its thigh (by the arm's own angles)."""
    return {"arm": (22.0, 62.0, 30.0, 70.0, 10.0, 15.0, 0.0)} if s == "l" else {"arm": (20.0, 55.0, 25.0, 65.0, 5.0, 15.0, 0.0)}


def _rod(grip, a=(0.0, 0.0, -1.0), n=(0.0, -1.0, 0.0), roll=180.0, **more):
    """A fist round the rocket at one of its grips (the rod across the palm), free to roll round it."""
    h = {"grip": grip, "a": a, "n": n, "roll": roll, "roll_axis": (1.0, 0.0, 0.0), "flip": True}
    h.update(more)
    return h


# his arms through the load, fitted on his rig to where the bag, the rocket, the launcher and the gunner's helmet are
# (scipy's simplex over rom.py's measures, kept inside its limits): by their own angles where the hand only has to be
# somewhere (rom's terms: elevation, plane, rotation, elbow, pronation, wrist flexion, radial deviation), as a hold
# (grip, the hand's axis and palm in the rocket's frame, the elbow's pole) where a hand moves with the rocket.
REACH_ARM = (127.5, 115.8, 65.2, 140.4, 0.0, 0.0, 0.0)      # at the bag's mouth behind his neck
LIFT_ARM = (133.2, 116.9, 62.4, 131.1, 0.0, 0.0, 0.0)        # the nose coming up out of the bag behind his neck
UP_ARM = (138.9, 61.5, -38.5, 120.2, 0.0, 0.0, 0.0)          # the fist lifting the nose up behind his head
OUT_ARM = (100.3, 54.2, -41.8, 96.3, 0.0, 0.0, 0.0)          # forward over his right shoulder, the tail just out
MID_ARM = (74.0, 30.9, -41.4, 136.6, -23.0, 0.0, 0.0)        # (on the way down: the fist turned so the tail clears his helmet)
MID2_ARM = (22.3, 75.0, -45.4, 80.1, 20.7, 0.0, 0.0)        # (coming round: the fist down by his side, the tail swung
#                                                              out past his right hip, clear of his back)
LOW_ARM = (13.9, 80.3, -4.5, 100.7, 75.1, 0.0, 0.0)          # brought down: the rocket across his chest
REACH_HEAD = ((10.0, 16.0, 0.0), (8.0, 18.0, 2.0))           # (his neck and head as he reaches back: ducked to the left,
LIFT_HEAD = ((10.0, 18.0, 0.0), (8.0, 20.0, 2.0))            # clear of his right arm going up past his helmet)
UP_HEAD = ((8.0, 14.0, 4.0), (6.0, 16.0, 6.0))
OFFER_HEAD = ((6.0, 10.0, 10.0), (2.0, 10.0, 12.0))          # (his neck and head as he offers it: clear of its tail)
STEADY_ARM = (141.3, 56.8, 17.4, 85.3, 0.0, 0.0, 0.0)        # the right hand on the tube's rear end, steadying it
WIRE_ARM = (139.4, 34.5, -21.2, 89.5, 0.0, 0.0, 0.0)         # the right hand at the contact spring
REAR_L_ARM = (86.8, 125.1, 35.0, 96.1, 0.0, 0.0, 0.0)        # the left hand under the tube's rear end
TAP_ARM = (89.8, 75.0, 15.9, 94.9, 0.0, 0.0, 0.0)            # the right hand over the gunner's helmet ...
TAPPED_ARM = (83.0, 75.0, 16.1, 97.5, 0.0, 0.0, 0.0)         # ... and on it
RELEASE_R = (81.9, 126.4, 69.1, 85.2, 0.0, 0.0, 0.0)         # (the right hand as it lets go of the rocket's head, by angles)
RELEASE_L = (93.3, 99.2, 19.1, 94.5, 0.0, 0.0, 0.0)          # (the left as it comes off the rocket's tail, pushed home)
TAKE_L = {"grip": "body", "a": (0.706, 0.324, 0.629), "n": (0.598, -0.748, -0.286), "pole_world": (-0.737, 0.295, -0.608)}
SWING_L = {"grip": "body", "a": (0.56, 0.304, 0.771), "n": (0.745, -0.591, -0.309), "pole_world": (-0.653, 0.147, -0.743)}
OFFER_L = {"grip": "body", "a": (-0.366, 0.514, 0.776), "n": (0.639, -0.468, 0.611), "pole_world": (0.328, -0.632, -0.702)}
OFFER_R = {"grip": "head", "a": (-0.034, 0.575, 0.818), "n": (-0.99, -0.13, 0.05), "pole_world": (0.168, 0.102, -0.98)}
PUSH_L = {"grip": "push", "a": (0.085, 0.4, 0.913), "n": (0.993, -0.109, -0.045), "pole_world": (0.211, 0.089, -0.973)}
PUSH_L2 = {"grip": "push", "a": (0.277, 0.403, 0.872), "n": (0.932, -0.332, -0.142), "pole_world": (0.233, 0.132, -0.964)}
TAP_POINT = (0.0, -0.132, 0.148)        # (on the gunner's helmet, from his head's joint at rest: its right side)
LEAN = {"pelvis_at": (0.06, -0.12, -0.30), "pelvis": (10.0, -4.0, -12.0), "spine_01": (4.0, -2.0, -4.0),     # (up on his
        "spine_02": (5.0, -2.0, -4.0), "chest": (6.0, -3.0, -6.0), "neck": (0.0, 0.0, -8.0), "head": (-4.0, 0.0, -8.0),  # knee)
        "clavicle_r": (10.0, 14.0)}


def _held(h, roll=20.0):
    """A hold on the rocket (see above), free to roll a little round the rocket's line."""
    return dict(h, roll=roll, roll_axis=(1.0, 0.0, 0.0))


def _arm(a):
    return {"arm": tuple(a)}


def c_load_bazooka(t):
    """Loading the gunner's launcher, on the gunner's clock (see the pair)."""
    t = t % FIRE_LONG_BZ
    base = load_kneel()
    clear = dict(base, pelvis=(2.0, -6.0, -14.0), spine_01=(-2.0, -5.0, -6.0), spine_02=(-2.0, -6.0, -8.0),
                 chest=(4.0, -8.0, -10.0), neck=(18.0, -6.0, -20.0), head=(16.0, -6.0, -24.0), pelvis_at=(-0.24, -0.06, -0.49),
                 hand_l=_on_thigh("l"), hand_r=_on_thigh("r"), clavicle_l=(4.0, 10.0), clavicle_r=(4.0, 10.0))
    look = dict(base, neck=(6.0, 0.0, -10.0), head=(10.0, 0.0, -14.0), hand_l=_on_thigh("l"), hand_r=_on_thigh("r"))
    reach = dict(look, neck=REACH_HEAD[0], head=REACH_HEAD[1], chest=(4.0, 4.0, 0.0), clavicle_r=(22.0, -8.0),
                 hand_r=_arm(REACH_ARM), fingers_r=55.0)
    duck = dict(look, neck=REACH_HEAD[0], head=REACH_HEAD[1])
    lift = dict(reach, neck=LIFT_HEAD[0], head=LIFT_HEAD[1], clavicle_r=(24.0, -6.0), hand_r=_arm(LIFT_ARM))
    up = dict(reach, clavicle_r=(26.0, -4.0), neck=UP_HEAD[0], head=UP_HEAD[1], hand_r=_arm(UP_ARM))
    out = dict(up, neck=(8.0, 6.0, 6.0), head=(6.0, 8.0, 8.0), clavicle_r=(26.0, -4.0), chest=(6.0, 2.0, 2.0),
               hand_r=_arm(OUT_ARM))
    mid = dict(out, neck=(10.0, 4.0, 0.0), head=(8.0, 4.0, 0.0), clavicle_r=(14.0, 4.0), hand_r=_arm(MID_ARM))
    mid2 = dict(mid, neck=(12.0, 2.0, 0.0), head=(11.0, 2.0, 0.0), clavicle_r=(10.0, 5.0), hand_r=_arm(MID2_ARM))
    low = dict(out, neck=(14.0, 0.0, 0.0), head=(14.0, 0.0, 0.0), chest=(8.0, 0.0, 2.0), clavicle_r=(6.0, 6.0),
               hand_r=_arm(LOW_ARM))
    take = dict(low, hand_l=_held(TAKE_L), fingers_l=55.0,
                hand_r=dict(_rod("nose", a=OUT_HOLD["a"], n=OUT_HOLD["n"], roll=0.0), flip=False,
                            pole_world=OUT_HOLD.get("pole_world", (0.46, -0.435, -0.774))))
    offer = dict(take, chest=(14.0, 0.0, 8.0), spine_02=(8.0, 0.0, 4.0), neck=OFFER_HEAD[0], head=OFFER_HEAD[1],
                 clavicle_r=(6.0, 8.0), hand_l=_held(OFFER_L), hand_r=_held(OFFER_R))
    swing = dict(offer, hand_l=_held(SWING_L, 0.0))       # (on the way: the left elbow down by his side)
    release = dict(offer, hand_r=_arm(RELEASE_R))
    steady = dict(offer, hand_r=_arm(STEADY_ARM), fingers_r=30.0)
    push = dict(steady, hand_l=_held(PUSH_L), fingers_l=20.0, chest=(16.0, 0.0, 12.0))
    push2 = dict(push, hand_l=_held(PUSH_L2))
    pushed = dict(push2, hand_l=_arm(RELEASE_L))
    wire = dict(push, hand_r=_arm(WIRE_ARM), fingers_r=45.0, thumb_r=45.0, hand_l=_arm(REAR_L_ARM), fingers_l=40.0,
                neck=(8.0, 10.0, 10.0), head=(6.0, 10.0, 12.0))
    wire2 = dict(wire, hand_r=_arm(WIRE_ARM[:4] + (WIRE_ARM[4] + 8.0, WIRE_ARM[5] + 10.0, WIRE_ARM[6])), thumb_r=55.0)
    tap = dict(base, **LEAN, hand_r=_arm(TAP_ARM), hand_l=_on_thigh("l"), fingers_r=20.0, thumb_r=20.0)
    tapped = dict(tap, hand_r=_arm(TAPPED_ARM))
    # (where a hand goes from a hold on the rocket to its own angles, or back, the key is doubled either side, so the
    # spline mixes only those two there and the arm is not pulled through a third)
    keys = [(0.0, clear), (LAUNCH + 0.15, clear), (1.1, look), (1.3, duck), (T_REACH, reach), (T_REACH + 0.05, reach), (1.85, lift), (2.1, up), (T_OUT, out), (2.78, mid),
            (2.875, mid2), (T_LOW, low), (T_LOW + 0.1, low), (T_TAKE, take), (T_TAKE2, take), (3.43, swing), (T_START, offer), (T_START + 0.05, offer),
            (T_SLIDE, release), (T_SLIDE + 0.05, release), (4.05, steady), (4.35, push), (T_IN + 0.05, push2),
            (T_IN + 0.1, push2), (4.85, pushed), (4.9, pushed), (5.1, wire), (5.45, wire2), (5.8, wire), (6.15, wire2),
            (6.6, tap), (TAP, tapped), (TAP + 0.18, tap), (7.9, clear), (FIRE_LONG_BZ, clear)]
    P = S.keyed(keys)(t)
    P["weapon"], phase = rocket_way(t)
    P["weapon_kind"] = "rocket"
    return P


LOAD_EVENTS = [S.ev(1.95, "kit_rattle", bone="chest"),
               S.ev(4.6, "rocket_loaded", socket=None, cue=None, bone="hand_l"),
               S.ev(5.6, "wire_contact", socket=None, cue=None, bone="hand_r"),
               S.ev(TAP, "helmet_tap", socket=None, cue=None, bone="hand_r", partner="head")]
S.WEAPONS["rocket"].update(push=(-0.305, 0.0, 0.0), nose=(0.235, 0.0, 0.0))
# his carbine laid down at his left knee while he loads (on its left side, as soldier.py lays a dropped rifle)
CARBINE_DOWN = M4(R_axis((0, 0, 1), 35.0) @ R_axis((1, 0, 0), -90.0), (0.14, 0.33, S.GUN_LIE))


def pair_info():
    """The pair as both files carry it, in the game's axes (x forward, y right, z up)."""
    return {"gunner": "soldier_USA_bazooka", "loader": "soldier_USA_loader",
            "clips": {"soldier_USA_bazooka": "fire_kneel", "soldier_USA_loader": "load_bazooka"},
            "loader_at": [round(PAIR_AT[0], 3), round(-PAIR_AT[1], 3), 0.0],
            "loader_yaw": round(-PAIR_YAW, 1),
            "about": "The loader's root in the gunner's frame (metres; x forward, y right, z up: the game's axes), and his"
                     " turn from the gunner's facing (degrees about z, Unreal's yaw: + turns to the right; "
                     f"{-PAIR_YAW:g} is {PAIR_YAW:g} degrees to the gunner's left, so he faces the launcher's rear end). Play the two clips on one"
                     f" clock (both {FIRE_LONG_BZ:g} s loops): the launch at {LAUNCH:g} s, the helmet tap at {TAP:g} s.",
            "long": FIRE_LONG_BZ, "launch_t": LAUNCH, "tap_t": TAP}


# ================================================================================================ the files
ABOUT = {
    "gunner": (" He is the bazooka gunner: soldier_usa.py's rifleman (dress, helmet, canteen, first-aid pouch, haversack)"
               " with the 2.36-inch rocket launcher on the weapon bone (1.37 m; the right hand on the trigger grip, the left on"
               " the front grip, the stock's rear on the right shoulder, the right eye at the sights on the left of the tube),"
               " a pistol belt with the pistol in its holster on the right hip and its magazine pocket in front (no rifle, so"
               " no rifle's pockets). His own: fire_stand, fire_kneel, fire_prone (9 s loops: the launch at 0.4 s, event"
               " rocket_launch at socket_muzzle with cue bazooka_fire, and backblast at the same moment, its `offset` the"
               " rear's place from the muzzle socket along the weapon's own axes; a jolt; then he holds on the mark while his"
               " loader loads, and the loader's tap on his helmet at 7.0 s); the captured clips carry the launcher on his"
               " shoulder (crawling, he pushes it ahead of him on the ground at his right front, his right fist on its rear"
               " end); go_prone, cower and die_kneel are his own; lmg_burst (prone on the aim) and throw_grenade (a look"
               " round) fire nothing. The bolt bone carries the latch and contact spring (still). See `pair` for his loader."),
    "loader": (" He is the bazooka loader: soldier_usa.py's rifleman (dress, helmet) with the M1 carbine on the weapon bone"
               " (0.904 m; semi-automatic: his fire_stand, fire_kneel and fire_prone are soldier_usa.py's, one shot a"
               " second, no bolt worked), a pistol belt with two carbine magazine pockets, the canteen and first-aid pouch"
               " behind the hips, either side of the bag's foot, and the rocket bag on his back. His own: load_bazooka (9 s loop, on the gunner's fire_kneel"
               " clock: clear of the backblast at the launch, 0.4 s; the rocket drawn by its nose from the bag over his"
               " right shoulder, 1.6-2.6 s, brought down across his chest and taken in both hands, 3.15 s; offered at the"
               " rear of the tube and slid in nose first, 3.6-4.6 s; the wire's contact made on the spring, 5.6 s; up on"
               " his knee to tap the gunner's helmet, 7.0 s; back down and leaning away, his head out of the blast). The"
               " rocket he loads rides the bolt bone: hidden in the bag in every other clip, in his hands and then in the gunner's tube in load_bazooka (it is in the tube until the"
               " launch, then back in the bag: the game's rocket flies from the gunner's muzzle). While he loads, his carbine"
               " lies at his left knee. lmg_burst (prone on the aim) fires nothing. See `pair`."),
}


def _json_about_bz(data, man):
    data["name"] = f"Battle {MEN[man]['who']}, USA, Normandy 1944 (our own design)"
    data["about"] = data["about"].replace("Made by Tools/battle_units/soldier.py.",
                                          "Made by Tools/battle_units/soldier_usa_bazooka.py on soldier_usa.py and soldier.py.")
    data["about"] += ABOUT[man]
    data["colours"] = dict(S.LOOK[NATION])
    data["new_paints"] = dict(NEW_PAINTS)
    data["pair"] = pair_info()
    return data


def _export_man(info, m, base, man):
    t0 = time.time()
    socks = m["sockets"]
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    bpy.context.scene.frame_set(0)
    n = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    rec = info.setdefault(base, {})
    rec["tris"] = n
    rec["parts"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    rec["short"] = {k: float(v) for k, v in m["short"].items()}
    S.export_glb(S.stage_path(f"{base}.glb"), [m["arm"]] + lods[0] + socks)
    for lod in (1, 2):
        S.export_glb(S.stage_path(f"{base}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
    data = S.write_game_json(S.stage_path(f"{base}.json"), m, lods)
    data = _json_about_bz(data, man)
    with open(S.stage_path(f"{base}.json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    rec["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
    rec["high"] = data["high"]
    rec["sockets"] = data["sockets"]
    rec["bones"] = [b["name"] for b in data["bones"]]
    rec["clips"] = {k: [v["long"], v["loop"], [(e["t"], e["fx"]) for e in v["events"]]] for k, v in data["clips"].items()}
    for lod in (1, 2):
        for o in lods[lod]:
            bpy.data.objects.remove(o)
    print(f"  {base} written in {time.time() - t0:.0f} s: {n}")
    return [f"{base}.glb", f"{base}_LOD1.glb", f"{base}_LOD2.glb", f"{base}.json"]


def export_man(man, info, deliver=True):
    t0 = time.time()
    kit.fresh(S.FPS)
    if man == "loader":
        gunner_track()                      # (worked out first, on a rig of the gunner's own)
    m = assemble_man(man)
    S.bake_clips(m)
    files = _export_man(info, m, MEN[man]["base"], man)
    if deliver:
        for n_ in files:
            S.deliver(n_)
    info[f"export_{man}_s"] = round(time.time() - t0)
    return m


# ================================================================================================ the picture
def _show(m, on):
    for ob in [m["arm"]] + S.man_objects(m) + m["sockets"]:
        ob.hide_render = not on


def _pose(m, name, t):
    become(m["man"])
    st = S.clip_state(m["rig"], name, t, "rifle")
    S.pose_once(m["arm"], m["rig"], st, 0)
    return st


def picture(info, fast=False, deliver=True, shots=None):
    """pictures/soldier_USA_bazooka.png: the gunner at ease, the launcher on his shoulder; the team kneeling as the
    rocket goes (the loader leaning clear of the blast); the loader sliding the next rocket into the tube."""
    size = (640, 760, 12) if fast else (1000, 1180, 36)
    kit.fresh(S.FPS)
    gunner_track()
    g = assemble_man("gunner")
    ld = assemble_man("loader")
    for m in (g, ld):
        S.studio_smooth(m)
    ld["arm"].location = (PAIR_AT[0], PAIR_AT[1], 0.0)
    ld["arm"].rotation_euler = (0.0, 0.0, math.radians(PAIR_YAW))
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(*size)
    shots = shots or [("ease", [(g, "idle", 0.5)], (0.0, 0.0, 1.0), 3.4, 6.0, 28.0),
                      ("launch", [(g, "fire_kneel", LAUNCH + 0.02), (ld, "load_bazooka", LAUNCH + 0.02)], (-0.35, -0.3, 0.75), 3.6, 9.0, 318.0),
                      ("load", [(g, "fire_kneel", 3.7), (ld, "load_bazooka", 3.7)], (-0.6, -0.35, 0.85), 2.7, 22.0, 230.0)]
    out_ims = []
    for key, poses, aim, dist, pitch, yaw in shots:
        _show(g, any(m is g for m, _, _ in poses))
        _show(ld, any(m is ld for m, _, _ in poses))
        for m, name, t in poses:
            _pose(m, name, t)
        kit.orbit(cam, aim, dist, pitch, yaw)
        out_ims.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"bz_{key}.png")))
    out = S.stage_path("pictures", "soldier_USA_bazooka.png")
    kit.sheet(out_ims, out, cols=len(out_ims),
              labels=["Bazooka gunner at ease: the launcher on his shoulder",
                      "The rocket goes: the loader leans clear of the blast",
                      "Loading: the next rocket slid nose first into the tube's rear"][:len(out_ims)],
              title="American bazooka team, Normandy 1944: soldier_usa_bazooka.py (our own design)")
    if deliver and not fast:
        S.deliver("pictures/soldier_USA_bazooka.png")
    info["picture"] = out
    return out


def main():
    a = kit.args()
    fast = "--fast" in a
    keep = "--keep" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export"]
    man = NOW["man"]
    p = S.stage_path(f"export_info_bazooka_{man if 'export' in steps else 'picture'}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "export":
            export_man(man, info, deliver=not keep)
        elif step == "picture":
            picture(info, fast, deliver=not keep)
        with open(p, "w") as f:
            json.dump(info, f, indent=1)
        print(f"step {step}: {time.time() - t0:.0f} s")
    if not keep:
        ok = S.check_delivered()
        print("delivered" if ok else "SOME FILES DID NOT LAND")


_snapshot()
become(_arg_man())

if __name__ == "__main__":
    main()
