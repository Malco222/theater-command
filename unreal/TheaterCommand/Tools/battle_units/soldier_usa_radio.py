"""The American radio man of the Normandy battles (summer 1944): the company's radio operator with the SCR-300 set on
his back (its long whip antenna up over him) and its handset clipped to his left shoulder strap, armed with the M1
carbine. His own clip, radio_call, is the call for support: he takes the handset to his ear and calls, and the battle
decides whether artillery or an airstrike comes. Made beside soldier_usa.py and soldier.py (both stay as they are).

    python soldier_usa_radio.py --only export,picture     build, rig, animate and export him; his picture sheet
    (--keep: stage only, nothing copied to the shared folder; --fast: a small test picture)

Run it in a checkout of claude/project-thread-8egity's Tools/battle_units (soldier.py, soldier_usa.py, human.py,
mocap.py, kit.py and MakeHuman's base files beside it), with the bpy module (Blender 5.0.1), as soldier_usa.py is run.

He is soldier_usa.py's rifleman in body, dress and rig: the same 32 bones, 5 sockets and 19 clip names, plus his own
radio_call. The M1 carbine (the bazooka loader's drawing, copied here from soldier_usa_bazooka.py so neither file
leans on the other) is on the weapon bone; the handset rides the bolt bone (as the loader's rocket rides his): on the
strap in every clip, in his left hand and at his ear in radio_call.
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
import numpy as np  # noqa: E402
import kit  # noqa: E402
from sculpt import Ellipsoid, Layer, RoundBox, RoundCone, frame_from, on_surface, rot_euler  # noqa: E402

NATION = "USA"
S.SCRATCH = "/home/user/art/radio/"
S.STAGE = os.path.join(S.SCRATCH, "out")
BASE = "soldier_USA_radio"
SEED = 163                 # (his face: MakeHuman's mix 163; the rifleman is 85, the squad leader 192, the medic 140,
#                            the bazooka gunner 118 and his loader 146)

# ---- a new paint (soldier.py's are 0-14; 15-16 the medic's, 17 the squad leader's, 18 the bazooka's rocket): the set's
# own olive drab, so the game can tell the radio from the helmet's paint
S.FIGURE_PAINTS["radio"] = 19
S.LOOK[NATION]["radio"] = "4E5039"
NEW_PAINTS = {"radio": 19}

R_axis, M4, _n = S.R_axis, S.M4, S._n
ramp, bump, osc, add3, sm = S.ramp, S.bump, S.osc, S.add3, S.sm


# ================================================================================================ the carbine
# (soldier_usa_bazooka.py's drawing, the same numbers.) In soldier.py's rifle frame (origin where the right hand grips
# the wrist, x to the muzzle, y left, z up, the bore 4.2 cm over the grip, the sights' line at 7 cm): the butt plate at
# -0.29, the muzzle at 0.585.
CARBINE_BUTT = -0.291
CARBINE_MUZZLE = (0.585, 0.0, 0.042)
CARBINE = {"grip_r": (0.0, 0.0, 0.0), "grip_l": (0.29, 0.0, 0.006), "butt": (CARBINE_BUTT, 0.0, -0.024), "sight_z": 0.07,
           "bolt": (-0.064, -0.058, 0.02), "carry": (0.13, 0.0, -0.004)}
BUTT_AT_CARBINE = (CARBINE_BUTT - 0.002, 0.0, -0.058)


def build_carbine():
    """The M1 carbine (soldier_usa_bazooka.py's drawing): a slim one-piece walnut stock with a semi-pistol grip, a short
    handguard, the barrel band with its swivel, the round receiver with the operating slide on its right and the flip
    rear sight, the fifteen-round magazine, the front sight between its ears, the sling down the stock's left side.
    0.904 m. Returns (carbine, handset): the handset rides the bolt bone (see clip_state)."""
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
    return carbine, build_handset()


# ================================================================================================ the handset
# The TS-15 handset, our own drawing: a black handle with the earphone's cup at one end and the mouthpiece's at the
# other (angled toward the mouth), the press-to-talk bar on the handle's inner side, a spring clip on its back, and a
# stub of its cord out of the mouthpiece's end. In its own frame: x from the mouthpiece to the earphone, z out of its
# back (the cups stand on its -z side, the side that goes to the face), the origin at the handle's middle.
EAR_CUP = (0.088, 0.0, -0.021)        # the earphone cup's face (its middle), in the handset's frame
MOUTH_CUP = (-0.09, 0.0, -0.024)      # the mouthpiece's face
HANDSET_LONG = 0.228


def build_handset():
    h = Layer("handset", "steel")
    h.add(RoundBox((0.0, 0.0, 0.0), (0.06, 0.0145, 0.0105), 0.0085))                                      # handle
    h.add(RoundCone((0.062, 0.0, -0.002), (0.082, 0.0, -0.008), 0.0125, 0.017), 0.008)                   # its neck up
    h.add(RoundCone((EAR_CUP[0], 0.0, 0.002), (EAR_CUP[0], 0.0, EAR_CUP[2] + 0.002), 0.0285, 0.0305), 0.006)   # earphone
    h.sub(Ellipsoid((EAR_CUP[0], 0.0, EAR_CUP[2] - 0.004), (0.02, 0.02, 0.006)), 0.003)                   # (its dished face)
    h.add(RoundCone((-0.062, 0.0, -0.002), (-0.08, 0.0, -0.01), 0.0125, 0.016), 0.008)                   # neck down
    h.add(RoundCone((MOUTH_CUP[0] + 0.004, 0.0, 0.0), (MOUTH_CUP[0], 0.0, MOUTH_CUP[2] + 0.002), 0.0235, 0.025), 0.006)
    h.sub(Ellipsoid((MOUTH_CUP[0], 0.0, MOUTH_CUP[2] - 0.004), (0.015, 0.015, 0.006)), 0.003)
    bar = Layer("handset_bar", "steel")
    bar.add(RoundBox((0.0, 0.0, -0.0115), (0.026, 0.0075, 0.003), 0.0025), 0.0015)                    # press-to-talk
    clip = Layer("handset_clip", "steel")
    clip.add(RoundBox((0.02, 0.0, 0.0125), (0.018, 0.009, 0.002), 0.0015), 0.001)                     # spring clip
    me = S.mesh_from_layers("handset", [h, bar, clip], 0.0018, 420)
    g = S.Geo()
    g.absorb(me)
    a = np.array((MOUTH_CUP[0] - 0.012, 0.0, 0.004))
    g.tube(a, a + np.array((-0.04, 0.0, 0.012)), 0.0042, 0.0038, "steel", 6)                       # the cord's stub
    return g.build("rifle_bolt")


# ================================================================================================ his tables
S.WEAPONS["rifle"] = dict(CARBINE)
S.BUTT_AT["rifle"], S.REAR_NOTCH["rifle"], S.COMB_Z["rifle"] = BUTT_AT_CARBINE, 0.033, 0.035
U.build_garand = build_carbine
# the handset in his left hand, as a "weapon" of its own while he calls (its grips, in its own frame)
S.WEAPONS["handset"] = {"grip_l": (-0.014, 0.0, 0.0), "grip_r": (-0.014, 0.0, 0.0), "sight_z": 0.0, "butt": (-0.1, 0.0, 0.0),
                        "bolt": (0.0, 0.0, 0.0), "carry": (0.0, 0.0, 0.0)}
S.REAR_NOTCH["handset"], S.COMB_Z["handset"] = 0.0, 0.0


# ================================================================================================ his belt and the radio
# The pistol belt (no rifle, so none of the rifle belt's ten pockets) with a pocket of two carbine magazines in front
# on the right (clear of the left thigh as he kneels on his right knee to call); the canteen and the first-aid pouch as the rifleman's. On his back, in the haversack's place, the
# SCR-300: the set over its battery case on a canvas pad, hung from the suspenders by its harness's two top straps, the
# long whip antenna up from its top left. The handset hangs clipped to the left suspender strap, high on his chest.
_usa_build_kit = S.build_kit
RADIO_HALF = (0.085, 0.165, 0.215)    # the set and its battery case together (half: deep, wide, tall): 17 x 33 x 43 cm
RADIO_SPLIT = 0.2                     # the battery case's height, at the bottom
ANTENNA_LONG = 3.25                   # the long whip (10 ft 8 in), in sections
ANTENNA_TILT = (14.0, 10.0)           # (degrees back from his spine's line, and out to his left: clear of his helmet,
#                                       lying down as well as standing)
ANTENNA_R = (0.0062, 0.0024)          # the whip's radius at its foot and at its tip
ANTENNA_JOINTS = 7                    # (ferrules where its sections join)
CLIP_Z = 1.40                         # (the handset's middle on the strap: this high on the reference man, its lower end
#                                       over the forearm that holds the carbine across him)
CLIP_OUT_BY = 0.02                    # (its middle this far out of the strap's face: its back and the clip between)


def _strap_front(straps, J, z, side=1.0):
    """The suspender strap's front face over his chest at height z on a side: its middle and the way out of it."""
    me = straps.data
    V = np.array([v.co[:] for v in me.vertices])
    front = V[(V[:, 1] * side > 0.02) & (V[:, 0] > 0.03) & (np.abs(V[:, 2] - z) < 0.06)]
    A = np.stack([front[:, 2] - z, np.ones(len(front))], axis=1)        # (the strap's line through its points: x, y on z)
    (kx, x), _, _, _ = np.linalg.lstsq(A, front[:, 0], rcond=None)
    (ky, y), _, _, _ = np.linalg.lstsq(A, front[:, 1], rcond=None)
    return np.array((float(x) + 0.002, float(y), z))                     # (its outer face: half the webbing's thickness)


def antenna_line(J):
    """The whip's foot and its way up, in his rest frame."""
    c, half = np.array(J["radio"]["c"]), np.array(J["radio"]["half"])
    foot = c + np.array((-0.02, half[1] - 0.035, half[2] + 0.045))
    back, out = (math.radians(a) for a in ANTENNA_TILT)
    way = _n(np.array((-math.sin(back), math.sin(out), math.cos(back) * math.cos(out))))
    return foot, way


def build_antenna(foot, way, sides=6, joints=ANTENNA_JOINTS, r=ANTENNA_R, name="antenna"):
    """The whip: a thin steel rod tapering to its tip, the ferrules where its sections join (none at the far levels)."""
    g = S.Geo()
    up = (1.0, 0.0, 0.0)
    n = joints + 1
    for k in range(n):
        a, b = foot + way * ANTENNA_LONG * k / n, foot + way * ANTENNA_LONG * (k + 1) / n
        ra = r[0] + (r[1] - r[0]) * k / n
        rb = r[0] + (r[1] - r[0]) * (k + 1) / n
        g.tube(a, b, ra, rb, "steel", sides, caps=(k == n - 1), up=up)
        if joints and k < n - 1:
            g.tube(b - way * 0.012, b + way * 0.012, rb + 0.0016, rb + 0.0016, "steel", sides, caps=True, up=up)
    return g.build(name)


def build_kit(J, spec, body_ls, Z=None):
    out = _usa_build_kit(J, spec, body_ls, Z)
    if not U._ours(spec):
        return out
    Z = Z or (lambda z: z)
    tunic = body_ls[0]
    for k in ("pouches", "belt_pockets", "pack", "shovel"):
        if k in out:
            bpy.data.objects.remove(out.pop(k)[0])
    zb = Z(1.0)
    cx = J["spine_01"][0] + 0.03 if "crown" in J else 0.0
    rings, _ = U._taut({side: U._waist_ring(tunic, cx, zb, side) for side in (1.0, -1.0)}, cx)
    top = zb + U.BELT_W / 2 + 0.004
    # ---- a pocket of two carbine magazines on the front right of the belt (on the pouch bone, which hangs at the left
    # too: every man has the same 32 bones)
    half = (0.0165, 0.04, 0.06)
    P, N, s = rings[-1.0]
    p0, n0 = U._at_length(P, N, s, 0.085)
    R = frame_from(n0, (0.0, 0.0, 1.0))
    c = np.array((p0[0], p0[1], top - half[2] + 0.004)) + n0 * (0.011 + half[0])
    L = Layer("pocket", "straps")
    L.add(RoundBox(c, half, 0.004, R), 0.0)
    L.add(RoundBox(c + R @ np.array((0.003, 0.0, half[2] - 0.016)), (half[0], half[1] + 0.0016, 0.0165), 0.003, R), 0.0015)
    for side in (1.0, -1.0):
        P, N, s = rings[side]
        p0, n0 = U._at_length(P, N, s, 0.085)
        J["pouch_pivot_" + ("l" if side > 0 else "r")] = tuple(float(v) for v in np.array((p0[0], p0[1], top - 0.012)) + n0 * 0.012)
    out["pouches"] = (S.mesh_from_layers("pouches", [L], 0.002, 160), None)
    # ---- the SCR-300 on his back: its front face (the canvas pad) laid against the back's highest points over its
    # height, so the box stands off the shoulder blades and the small of the back hangs free under it
    hx, hy, hz = RADIO_HALF
    hi = Z(1.485)
    lo = hi - 2.0 * hz
    backs = []
    for z in np.arange(lo, hi + 1e-6, 0.015):
        for y in np.arange(-0.13, 0.131, 0.026):
            p, _ = on_surface(tunic, (-0.4, y, z), (1.0, 0.0, 0.0))
            if abs(p[0]) < 0.5:                                    # (over his collar the ray finds nothing)
                backs.append(p[0])
    pad = 0.014
    bx = min(backs) - 0.006 - pad
    c = np.array((bx - hx, 0.0, (lo + hi) / 2))
    J["radio"] = {"c": [float(v) for v in c], "half": [hx, hy, hz]}
    st, od = "steel", "radio"
    zs = c[2] - hz + RADIO_SPLIT                                   # (the seam between the battery case and the set)
    box = Layer("radio_set", od)
    box.add(RoundBox(c + np.array((0.0, 0.0, (zs + c[2] + hz) / 2 - c[2])), (hx, hy, (c[2] + hz - zs) / 2), 0.012))
    case = Layer("radio_case", od)
    case.add(RoundBox(c + np.array((0.0, 0.0, (c[2] - hz + zs) / 2 - c[2])), (hx - 0.004, hy - 0.004, (zs - c[2] + hz) / 2), 0.012))
    padL = Layer("radio_pad", "pack")
    padL.add(RoundBox(c + np.array((hx + pad / 2, 0.0, 0.01)), (pad / 2, hy + 0.012, hz + 0.012), 0.006))
    me = S.mesh_from_layers("radio", [box, case, padL], 0.003, 520)
    g = S.Geo()
    g.absorb(me)
    for dz in (0.0,):                                                                 # (the seam's two clamps a side)
        for side in (1.0, -1.0):
            for dx in (-0.04, 0.04):
                g.box(c + np.array((dx, side * (hy + 0.001), zs - c[2] + dz)), (0.009, 0.004, 0.022), st)
    tz = c[2] + hz
    g.box((c[0] - 0.01, c[1] - 0.03, tz + 0.002), (0.045, 0.06, 0.0025), od)                       # the panel on top
    g.box((c[0] - 0.01, c[1] - 0.045, tz + 0.0055), (0.016, 0.03, 0.0018), "leather")             # the dial's window
    for dy in (0.015, -0.1):                                                                       # two knobs
        g.tube((c[0] - 0.01, c[1] + dy, tz + 0.003), (c[0] - 0.01, c[1] + dy, tz + 0.018), 0.012, 0.011, st, 10)
    foot, way = antenna_line(J)
    g.tube(foot - way * 0.05, foot + way * 0.03, 0.013, 0.01, st, 10)                              # the antenna's base
    g.tube((c[0] + 0.03, c[1] - hy + 0.04, tz), (c[0] + 0.03, c[1] - hy + 0.04, tz + 0.02), 0.009, 0.008, st, 8)  # the jack
    g.tube((c[0] + 0.03, c[1] - hy + 0.04, tz + 0.02), (c[0] + 0.05, c[1] - hy + 0.04, tz + 0.045), 0.0042, 0.0038, st, 6)
    # the harness's two top straps, from the pad's top corners forward to the suspenders on his shoulder blades
    for side in (1.0, -1.0):
        a = np.array((bx + 0.004, side * (hy - 0.03), hi + 0.006))
        b, nrm = on_surface(tunic, (-0.4, side * 0.1, hi - 0.07), (1.0, 0.0, 0.0))
        assert np.all(np.abs(b) < 2.0), "the harness strap's ray missed his back"
        b = b + nrm * 0.006
        u = _n(b - a)                                             # (its frame: across its thickness, its width, along it)
        w = _n(np.array((0.0, 1.0, 0.0)) - u * u[1])
        g.box((a + b) / 2, (0.003, 0.016, float(np.linalg.norm(b - a)) / 2 + 0.01), "straps",
              np.stack([np.cross(w, u), w, u], axis=1))
    out["radio"] = (g.build("radio"), "chest")
    out["antenna"] = (build_antenna(foot, way), "chest")
    # ---- where the handset hangs: clipped high on the left suspender strap, its back to the strap, the earphone up
    # (its long way laid along the strap's own line there, so its ends keep off the chest as the strap does)
    straps = out.get("straps")
    if straps is not None:
        p = _strap_front(straps[0], J, Z(CLIP_Z))
        up = _n(_strap_front(straps[0], J, Z(CLIP_Z + 0.04)) - _strap_front(straps[0], J, Z(CLIP_Z - 0.06)))
    else:
        p, _ = on_surface(tunic, (0.4, 0.1, Z(CLIP_Z)), (-1.0, 0.0, 0.0))
        up = np.array(CLIP_UP)
    q, nrm = on_surface(tunic, (0.4, p[1], p[2]), (-1.0, 0.0, 0.0))
    J["handset_clip"] = {"at": [float(v) for v in p + nrm * CLIP_OUT_BY], "out": [float(v) for v in nrm],
                         "up": [float(v) for v in up]}
    print(f"  radio: the set's middle {np.round(c, 3)}, its front {bx:.3f} (the back's highest {min(backs):.3f}); "
          f"the whip's foot {np.round(foot, 3)}; the handset's clip {np.round(np.array(J['handset_clip']['at']), 3)} "
          f"out {np.round(nrm, 3)} up {np.round(up, 3)}")
    return out


S.build_kit = build_kit


# ================================================================================================ the levels of detail
# The whip is his silhouette: it is never thinned (a thinned tube collapses to a line). Each level draws its own:
# six sides with the ferrules near, four sides plain at the middle level, three at the far one, a little thicker there
# so it still shows as a line at battle range.
_make_lods = S.make_lods


def make_lods(m, lod):
    ant = next((o for o in m["kit"] if o.name.split(".")[0] == "antenna"), None)
    if ant is None:
        return _make_lods(m, lod)
    kit0 = m["kit"]
    m["kit"] = [o for o in kit0 if o is not ant]
    try:
        out = _make_lods(m, lod)
    finally:
        m["kit"] = kit0
    foot, way = antenna_line(m["J"])
    c = build_antenna(foot, way, sides=4 if lod == 1 else 3, joints=0,
                      r=(0.0075, 0.0035) if lod == 1 else (0.0095, 0.005), name=f"antenna_LOD{lod}")
    S.skin_rigid(c, "chest")
    S.attach(c, m["arm"])
    out.append(c)
    return out


S.make_lods = make_lods


# ================================================================================================ where the handset is
def _set_weapon(rig, st, W):
    st["D"]["weapon"], st["H"]["weapon"], st["W"] = W[:3, :3].copy(), W[:3, 3].copy(), W
    for b in ("bolt", "bipod"):
        st["D"][b] = W[:3, :3].copy()
        st["H"][b] = W[:3, 3] + W[:3, :3] @ (rig.H0[b] - rig.H0["weapon"])


def _set_handset(rig, st, M):
    """The bolt bone put where the handset's own frame (see build_handset) is M."""
    R, o = M[:3, :3], M[:3, 3]
    st["D"]["bolt"] = R.copy()
    st["H"]["bolt"] = o + R @ (rig.H0["bolt"] - rig.H0["weapon"])


def clip_of(rig):
    """The handset's clip on the strap (his rest frame): build_kit's, or for a rig without his kit (arms_check's light
    rig) the same measured off the chest's joint."""
    c = rig.J.get("handset_clip")
    if c:
        return np.array(c["at"]), np.array(c["out"]), np.array(c["up"])
    return np.array(rig.H0["chest"]) + np.array(CLIP_OFF), _n(np.array(CLIP_OUT)), _n(np.array(CLIP_UP))


CLIP_OFF = (0.184, 0.117, 0.108)      # (measured on the man made: the clip from the chest's joint, and the way out)
CLIP_OUT = (0.858, 0.103, 0.503)
CLIP_UP = (-0.534, 0.041, 0.844)


def strap_place(rig, st):
    """The handset hung on the strap: the earphone up, its back to the strap, the cups out."""
    p, out, up = clip_of(rig)
    x = _n(up - out * (out @ up))
    R0 = np.stack([x, np.cross(-out, x), -out], axis=1)
    D, H = st["D"]["chest"], st["H"]["chest"]
    return M4(D @ R0, H + D @ (p - rig.H0["chest"]))


def ear_points(rig):
    """His left ear (where the earphone's face goes) and his mouth (where the mouthpiece points), in his rest frame."""
    J = rig.J
    e = np.array(J["eye_l"]) if "eye_l" in J else np.array(rig.eye0["l"])
    chin = np.array(J["chin"]) if "chin" in J else e + np.array((0.0, -0.033, -0.11))
    ear = np.array((e[0] - 0.088, 0.079 + EAR_OUT, e[2] - 0.03))
    mouth = np.array((chin[0] - 0.004, 0.0, chin[2] + 0.045))
    return ear, mouth


EAR_OUT = 0.011                       # (the earphone's face this far out of the ear's canal: the ear itself between)
MOUTH_OFF = (0.03, 0.03, -0.012)      # (the mouthpiece's face this far from the mouth: before it, out to the left, below)


def ear_place(rig, st, nod=0.0):
    """The handset at his left ear: the earphone's face on the ear, the mouthpiece before his mouth."""
    ear, mouth = ear_points(rig)
    m = mouth + np.array(MOUTH_OFF)
    x = _n(ear - m)
    zl = _n(np.array((0.0, 1.0, 0.0)) - x * x[1])                 # (its back out to his left)
    R0 = np.stack([x, np.cross(zl, x), zl], axis=1)
    o = ear - R0 @ np.array(EAR_CUP)
    D, H = st["D"]["head"], st["H"]["head"]
    return M4(D @ R0, H + D @ (o - rig.H0["head"]))


WAY_OUT = (0.07, 0.1, 0.0)            # (on its way between the strap and his ear the hand swings this far out, in the
#                                       chest's frame: forward and to his left, clear of his chin and his shoulder)


# ================================================================================================ his call
# radio_call (8 s, a loop, kneeling): his carbine laid by his left knee, he takes the handset off the strap with his
# left hand and puts it to his ear (radio_squelch as it comes there), calls (radio_talk: the press-to-talk bar under
# his fingers, small moves of his head as he speaks), turns his head to the target and points to it with his right
# hand, and the call goes through (call_support: the battle decides whether artillery or an airstrike comes); he hears
# the answer, nods, and hangs the handset back on the strap.
CALL_LONG = 8.0
T_REACH, T_GRAB, T_EAR = 0.3, 0.85, 1.5
T_TALK, T_CALL = 1.95, 5.3
T_DOWN, T_HUNG, T_FREE = 6.55, 7.25, 7.5
CALL_EVENTS = [S.ev(T_EAR, "radio_squelch", None, "radio_squelch", bone="head"),
               S.ev(T_TALK, "radio_talk", None, "radio_talk", bone="head"),
               S.ev(T_CALL, "call_support", None, None)]
# (his carbine laid on the ground by his left knee, on its left side, as the bazooka loader lays his)
CARBINE_DOWN = M4(R_axis((0, 0, 1), 35.0) @ R_axis((1, 0, 0), -90.0), (0.14, 0.33, S.GUN_LIE))
ON_THIGH_L = {"arm": (22.0, 62.0, 30.0, 70.0, 10.0, 15.0, 0.0)}
ON_THIGH_R = {"arm": (20.0, 55.0, 25.0, 65.0, 5.0, 15.0, 0.0)}
POINT_R = {"arm": (102.0, 60.0, 5.0, 10.0, -30.0, 0.0, 0.0)}     # (the right arm out level to the target, the hand open)
# (the left hand on the handset: at the strap by its left edge, the fingers up along it and round its back, the wrist
# under it and the elbow down by his side; at the ear round the handle from behind, the palm on its back, the fingers
# forward round its edge and the wrist out below and behind. Each found by scanning holds against rom.py's limits: at
# the strap reaching and hanging up, at the ear talking, pointing and listening)
HOLD_STRAP = {"grip": "grip_l", "a": (0.866, 0.0, 0.5), "n": (0.0, 1.0, 0.0), "roll": 40.0, "roll_axis": (1.0, 0.0, 0.0)}
HOLD_EAR = {"grip": "grip_l", "a": (0.174, 0.853, -0.492), "n": (0.0, -0.5, -0.866), "roll": 60.0, "roll_axis": (1.0, 0.0, 0.0)}


def call_kneel():
    """Kneeling on his right knee, upright, the left foot forward (the bazooka loader's kneel)."""
    P = dict(S.kneel_base(0.0, aimed=False))
    P.update({"pelvis_at": (-0.19, 0.0, -0.475), "pelvis": (8.0, 0.0, -6.0), "spine_01": (4.0, 0.0, 2.0), "spine_02": (5.0, 0.0, 2.0),
              "chest": (6.0, 0.0, 2.0), "neck": (4.0, 0.0, 0.0), "head": (6.0, 0.0, 0.0), "clavicle_l": (0.0, 6.0), "clavicle_r": (0.0, 6.0),
              "foot_l": {"at": (0.3, 0.19, S.ANKLE), "yaw": 4.0, "pole": (1.0, 0.25, 0.3)},
              "foot_r": {"at": (-0.48, -0.17, 0.105), "yaw": -20.0, "pitch": 72.0, "toe": 62.0, "pole": (1.0, -0.2, -0.6)},
              "fingers_l": 35.0, "fingers_r": 35.0, "thumb_l": 30.0, "thumb_r": 30.0, "sight": 0.0,
              "hand_l": dict(ON_THIGH_L), "hand_r": dict(ON_THIGH_R)})
    return P


# On the strap and at his ear the hand takes the handset where it is (its grip, the hand free to roll round the
# handle); between, the hand carries it: the hand's way is laid in the chest's frame from where it was on the strap to
# where it will be at the ear (or back), swung out on the way, its turn mixed from one end's to the other's, and the
# handset rides the hand, its place in the hand mixed the same way. (A handset moved on its own way, the hand only
# following its grip, swings the elbow from one side to the other halfway.)
TA = T_GRAB + 0.08                    # (off the strap)
LEGS = ((TA, T_EAR), (T_DOWN, T_HUNG))
POLE_HOLD = 0.05                      # (cost of a degree squared of the elbow's swing off its way, carrying)
_ENDS = {}
_NOW = {}


def leg_of(t):
    """(leg, u) while the hand carries the handset, else None."""
    for k, (t0, t1) in enumerate(LEGS):
        if t0 < t < t1:
            return k, sm((t - t0) / (t1 - t0))
    return None


def handset_way(t):
    """Where the handset is while the hand only holds it (carried, the solve takes this and clip_state puts it in the
    hand)."""
    return ear_place if T_EAR <= t <= T_DOWN else strap_place


def hand_grip(rig, st, s="l"):
    """The hand's grip point and grip frame (frame2 of the hand's way and its palm's) as solved."""
    D, H = st["D"]["hand_" + s], st["H"]["hand_" + s]
    a0, t0, n0 = (np.asarray(v, float) for v in rig.hand0[s])
    return H + D @ (S.grip_point(rig.J, s) - rig.H0["hand_" + s]), D @ S.frame2(a0, n0)


def call_ends(rig):
    """At each end of each leg: the hand's grip (point, frame) and the elbow's way in the chest's frame, and the
    handset's place in the hand's grip frame."""
    key = id(rig)
    if key not in _ENDS:
        out = {}
        for t in sorted({x for leg in LEGS for x in leg}):
            st = clip_state(rig, "radio_call", t)
            g, F = hand_grip(rig, st)
            Dc, Hc = st["D"]["chest"], st["H"]["chest"]
            A, K = st["H"]["upperarm_l"], st["H"]["lowerarm_l"]
            w = _n(g - A)
            pole = Dc.T @ _n((K - A) - w * ((K - A) @ w))
            out[t] = {"g": Dc.T @ (g - Hc), "F": Dc.T @ F, "pole": pole, "rel": np.linalg.inv(M4(F, g)) @ st["Wh"]}
        _ENDS[key] = out
    return _ENDS[key]


def carry_hand(t):
    """The left hand's spec while it carries the handset (in the chest's frame), from call_ends."""
    k, u = leg_of(t)
    e0, e1 = (_NOW["ends"][x] for x in LEGS[k])
    g = e0["g"] + (e1["g"] - e0["g"]) * u + math.sin(math.pi * u) * np.array(WAY_OUT)
    F = S.mocap.slerp_mats(e0["F"], e1["F"], u)
    pole = _n(e0["pole"] + (e1["pole"] - e0["pole"]) * u)
    return {"frame": "chest", "at": tuple(g), "a": tuple(F[:, 0]), "n": tuple(F[:, 1]), "roll": 0.0, "pole": tuple(pole),
            "pole_weight": POLE_HOLD}


def c_radio_call(t):
    t = t % CALL_LONG
    base = call_kneel()
    look = dict(base, neck=(6.0, 0.0, -4.0), head=(8.0, 0.0, -6.0))
    down = dict(base, neck=(16.0, 4.0, 10.0), head=(18.0, 4.0, 14.0))        # (looking down at the strap, to his left)
    near = dict(down, hand_l=dict(HOLD_STRAP, slide=(0.0, 0.0, 0.05)), fingers_l=10.0, thumb_l=20.0)
    took = dict(down, hand_l=dict(HOLD_STRAP), fingers_l=60.0, thumb_l=40.0)
    lift = dict(took, neck=(6.0, 6.0, 4.0), head=(6.0, 8.0, 4.0), hand_l=dict(HOLD_EAR))
    ear = dict(took, neck=(4.0, 8.0, 2.0), head=(2.0, 10.0, 0.0), clavicle_l=(8.0, 6.0), hand_l=dict(HOLD_EAR))
    talk = dict(ear, fingers_l=72.0)
    turn = dict(talk, chest=(6.0, 0.0, -6.0), neck=(2.0, 6.0, -12.0), head=(0.0, 8.0, -14.0))
    point = dict(turn, hand_r=dict(POINT_R), fingers_r=8.0, thumb_r=10.0, clavicle_r=(6.0, 10.0))
    listen = dict(ear, neck=(6.0, 8.0, -2.0), head=(6.0, 10.0, -4.0), fingers_l=60.0)
    hang = dict(near, neck=(14.0, 4.0, 8.0), head=(16.0, 4.0, 12.0))
    keys = [(0.0, look), (T_REACH, look), (0.62, near), (T_GRAB, took), (T_GRAB + 0.08, took), (1.2, lift), (T_EAR, ear),
            (T_TALK - 0.15, ear), (T_TALK, talk), (2.7, talk), (3.15, turn), (3.5, point), (4.25, point), (4.75, turn),
            (5.15, talk), (T_CALL, talk), (5.55, listen), (6.35, listen), (T_DOWN, ear), (7.0, lift), (T_HUNG, took),
            (T_HUNG + 0.06, took), (T_FREE, hang), (7.8, look), (CALL_LONG, look)]
    P = S.keyed(keys)(t)
    # his head as he speaks: small nods and turns while the bar is pressed (radio_talk to the call's going through,
    # and again as he answers)
    talking = ramp(t, T_TALK, T_TALK + 0.2) * (1.0 - ramp(t, T_CALL + 0.1, T_CALL + 0.3)) + bump(t, 5.9, 6.05, 6.3)
    if talking > 1e-3:
        P["head"] = add3(P["head"], (2.2 * talking * math.sin(2 * math.pi * 2.3 * t), 0.0, 1.5 * talking * math.sin(2 * math.pi * 1.1 * t)))
    P = S.breathe(P, t, 4.0, 0.3)
    P["weapon"] = handset_way(t)
    P["weapon_kind"] = "handset"
    if leg_of(t) and _NOW.get("ends"):
        P["hand_l"] = carry_hand(t)
    return P


def c_lmg_burst_radio(t):
    """(No machine gun: lying on the aim with the carbine, no shots.)"""
    return S.breathe(S.prone_base(t), t, 1.2, 0.3)


S.CLIPS["radio_call"] = {"long": CALL_LONG, "loop": True, "stride": 0.0, "events": list(CALL_EVENTS), "fn": c_radio_call,
                         "weapon": "rifle"}
S.CLIPS["lmg_burst"] = {"long": 1.2, "loop": True, "stride": 0.0, "events": [], "fn": c_lmg_burst_radio, "weapon": "rifle"}

_clip_state = S.clip_state


def clip_state(rig, name, t, kind=None):
    if name == "radio_call":
        leg = leg_of(t % CALL_LONG)
        if leg:
            _NOW["ends"] = call_ends(rig)
        try:
            st = _clip_state(rig, name, t, "handset")
        finally:
            _NOW.pop("ends", None)
        W = st["W"]
        if leg:                                   # (carried: the handset where the hand has it)
            k, u = leg
            e0, e1 = (call_ends(rig)[x] for x in LEGS[k])
            R = S.mocap.slerp_mats(e0["rel"][:3, :3], e1["rel"][:3, :3], u)
            rel = M4(R, e0["rel"][:3, 3] + (e1["rel"][:3, 3] - e0["rel"][:3, 3]) * u)
            g, F = hand_grip(rig, st)
            W = M4(F, g) @ rel
        _set_weapon(rig, st, CARBINE_DOWN)
        _set_handset(rig, st, W)
        st["Wh"] = W
        return st
    st = _clip_state(rig, name, t, kind)
    _set_handset(rig, st, strap_place(rig, st))
    return st


S.clip_state = clip_state


# ================================================================================================ him
def assemble():
    """The radio man, dressed, armed and rigged (soldier.py's assemble through soldier_usa.py's), his muzzle socket
    moved to the carbine's muzzle."""
    m = S.assemble(NATION, "rifle", seed=SEED)
    old = m["sockets"][0]
    g0 = np.array(m["arm"].data.bones["weapon"].head_local)
    new = S.add_socket(m["arm"], "muzzle", "weapon", tuple(g0 + np.array(CARBINE_MUZZLE)))
    bpy.data.objects.remove(old)
    new.name = "socket_muzzle"
    m["sockets"][0] = new
    return m


ABOUT = (" He is the company's radio man: soldier_usa.py's rifleman (dress, helmet, canteen, first-aid pouch) with the M1"
         " carbine on the weapon bone (0.904 m; semi-automatic: his fire_stand, fire_kneel and fire_prone are soldier_usa.py's,"
         " one shot a second, cue rifle_shot), a pistol belt with a pocket of two carbine magazines, and on his back in the"
         " haversack's place the SCR-300 set over its battery case (paint 19, radio) with its long whip antenna (3.25 m, up"
         " from the set's top left, leaning a little back and out to his left; rigid on the chest bone). The handset rides the"
         " bolt bone: clipped high on his left suspender strap in every clip, in his left hand and at his left ear in"
         f" radio_call. His own: radio_call ({CALL_LONG:g} s loop, kneeling, his carbine laid by his left knee): the handset"
         f" taken off the strap ({T_GRAB:g} s) and put to his ear, radio_squelch at {T_EAR:g} s (cue radio_squelch, on the head);"
         f" he calls, radio_talk at {T_TALK:g} s (cue radio_talk, on the head), turns his head to the target and points to it"
         f" with his right hand; call_support at {T_CALL:g} s (no socket, no cue): the call goes through and the battle"
         " decides what comes, artillery or an airstrike; he hears the answer and nods, and hangs the handset back on the"
         f" strap ({T_HUNG:g} s). For an airstrike (in 1944 the target was marked with coloured smoke) the game can mark it"
         " with his throw_grenade clip (its grenade_release event) as a coloured smoke grenade. lmg_burst (prone on the aim)"
         " fires nothing.")


def about(data):
    data["name"] = "Battle radio man, USA, Normandy 1944 (our own design)"
    data["about"] = data["about"].replace("Made by Tools/battle_units/soldier.py.",
                                          "Made by Tools/battle_units/soldier_usa_radio.py on soldier_usa.py and soldier.py.")
    data["about"] += ABOUT
    # (the roof mark is his helmet's top, not the whip's tip: `high` stays the whole height, whip and all)
    P = np.array(data["near"]["points"]).reshape(-1, 3)
    chest = [b["name"] for b in data["bones"]].index("chest")
    whip = (np.array(data["near"]["part"]) == chest) & (P[:, 2] > 1.6)
    data["marks"] = {"roof": round(float(P[~whip, 2].max() - P[:, 2].min()), 3)}
    data["colours"] = dict(S.LOOK[NATION])
    data["new_paints"] = dict(NEW_PAINTS)
    return data


def export_man(info, m):
    t0 = time.time()
    socks = m["sockets"]
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    bpy.context.scene.frame_set(0)
    rec = info.setdefault(BASE, {})
    rec["tris"] = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    rec["parts"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    rec["parts_far"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[2]}
    rec["short"] = {k: float(v) for k, v in m["short"].items()}
    S.export_glb(S.stage_path(f"{BASE}.glb"), [m["arm"]] + lods[0] + socks)
    for lod in (1, 2):
        S.export_glb(S.stage_path(f"{BASE}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
    data = S.write_game_json(S.stage_path(f"{BASE}.json"), m, lods)
    data = about(data)
    with open(S.stage_path(f"{BASE}.json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    rec["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
    rec["high"] = data["high"]
    rec["sockets"] = data["sockets"]
    rec["bones"] = [b["name"] for b in data["bones"]]
    rec["clips"] = {k: [v["long"], v["loop"], [(e["t"], e["fx"], e["cue"]) for e in v["events"]]] for k, v in data["clips"].items()}
    for lod in (1, 2):
        for o in lods[lod]:
            bpy.data.objects.remove(o)
    print(f"  {BASE} written in {time.time() - t0:.0f} s: {rec['tris']}")
    return [f"{BASE}.glb", f"{BASE}_LOD1.glb", f"{BASE}_LOD2.glb", f"{BASE}.json"]


def export_radio(info, deliver=True):
    t0 = time.time()
    kit.fresh(S.FPS)
    m = assemble()
    S.bake_clips(m)
    files = export_man(info, m)
    if deliver:
        for n_ in files:
            S.deliver(n_)
    info["export_s"] = round(time.time() - t0)
    return m


# ================================================================================================ the picture
def picture(info, fast=False, deliver=True, shots=None):
    """pictures/soldier_USA_radio.png: the radio man at ease from the front quarter, from behind (the set and its
    whip), and calling with the handset at his ear."""
    size = (640, 760, 12) if fast else (1000, 1180, 36)
    kit.fresh(S.FPS)
    m = assemble()
    S.studio_smooth(m)
    rig = m["rig"]
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(*size)
    shots = shots or [("front", "idle", 0.5, (0.0, 0.0, 1.25), 4.6, 6.0, 28.0),
                      ("back", "idle", 0.5, (0.0, 0.0, 2.1), 7.2, 8.0, 212.0),
                      ("call", "radio_call", 3.9, (0.25, -0.05, 0.8), 3.4, 8.0, 40.0)]
    ims = []
    for key, name, t, aim, dist, pitch, yaw in shots:
        st = S.clip_state(rig, name, t, "rifle")
        S.pose_once(m["arm"], rig, st, 0)
        kit.orbit(cam, aim, dist, pitch, yaw)
        ims.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"radio_{key}.png")))
    out = S.stage_path("pictures", f"{BASE}.png")
    if len(ims) != 3:                     # (test shots: the stills only)
        return ims
    kit.sheet(ims, out, cols=len(ims),
              labels=["American radio man at ease: the carbine, the handset on his strap",
                      "From behind: the SCR-300 on his back, its long whip antenna",
                      "Calling for support: the handset at his ear, pointing out the target"],
              title="American radio man, Normandy 1944: soldier_usa_radio.py (our own design)")
    if deliver and not fast:
        S.deliver(f"pictures/{BASE}.png")
    info["picture"] = out
    return out


def main():
    a = kit.args()
    fast = "--fast" in a
    keep = "--keep" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export", "picture"]
    p = S.stage_path(f"export_info_{BASE}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "export":
            export_radio(info, deliver=not keep)
        elif step == "picture":
            picture(info, fast, deliver=not keep)
        with open(p, "w") as f:
            json.dump(info, f, indent=1, default=str)
        print(f"step {step}: {time.time() - t0:.0f} s")
    if not keep:
        ok = S.check_delivered()
        print("delivered" if ok else "SOME FILES DID NOT LAND")


if __name__ == "__main__":
    main()
