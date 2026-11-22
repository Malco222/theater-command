"""The sailors of the sea battles, for the men who go over the side when their ship goes down (Brian, 22 November
2026: "sailors should jump in the water if the boat is going down", "have them use the models we have for the soldiers
for the sailors"): the American sailor (soldier_USA_sailor), the Japanese sailor (soldier_JAP_sailor), the British
sailor (soldier_UK_sailor, `--man uk`) and the German sailor (soldier_GER_sailor, `--man ger`). Made beside soldier.py
and soldier_usa.py (both stay as they are).

    python soldier_sailors.py --man usa --only export,picture      build, rig, animate and export him; his pictures
    python soldier_sailors.py --man usa --only look                 test stills of the dress (no clips baked)
    python soldier_sailors.py --man usa --only water                stills of the water clips (--fast: small)

Run it in a checkout of Tools/battle_units (soldier.py, soldier_usa.py, human.py, mocap.py, kit.py, sculpt.py and
MakeHuman's base files beside it), with the bpy module (Blender 5.0.1), as soldier_usa.py is run.

Each is soldier.py's man in every bone (the same 32, in the same order). He carries no weapon: the file has no
weapon triangles, and every clip's `weapon` is "none" (the weapon bone rides 2 m under his root). His clips:

- on deck, from the motion capture with his arms free: idle, walk, run, cower (hunched, hands up by his face, as
  aircraft come in), die_back and die_forward;
- over the side and in the water, keyed on his rig, the root at the sea's surface (z = 0) unless said:
  jump_feet_first (a 4 m drop), jump_feet_first_2m and jump_feet_first_8m: from the deck edge (his feet on it, h over
  the water), arms crossed over the vest, a step off, the fall, the splash (event `splash` when his feet reach the
  water), under and up again into tread's first pose; fall_loop (the fall alone, in place, for any other height) and
  enter_water (from the splash on: the root at the surface); tread, wave, swim_breast, cling, float_dead;
  climb_net (up a scrambling net, the net 0.3 m ahead of his root; `climb` metres a loop); row and sit_raft (the
  root on the thwart's or the raft's top).
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def _arg(name, default):
    a = sys.argv
    return a[a.index(name) + 1] if name in a else default


WHO = _arg("--man", "usa")
assert WHO in ("usa", "jap", "uk", "ger"), WHO
NATION = {"usa": "USA", "jap": "JAP", "uk": "UK", "ger": "GER"}[WHO]
BASE = {"usa": "soldier_USA_sailor", "jap": "soldier_JAP_sailor", "uk": "soldier_UK_sailor", "ger": "soldier_GER_sailor"}[WHO]

if WHO == "usa":
    import soldier_usa as U  # noqa: E402  (soldier.py with the American's wrappers)
    S = U.S
else:
    import soldier as S  # noqa: E402
    U = None
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402,F401
import kit  # noqa: E402
import mocap  # noqa: E402
from sculpt import smax, Layer, RoundBox, RoundCone, Sphere, Torus, Tube, frame_from, rot_euler  # noqa: E402,F401

S.SCRATCH = f"/home/user/art/sailors/{WHO}/"
S.STAGE = os.path.join(S.SCRATCH, "out")
M4, E, R_axis = S.M4, S.E, S.R_axis


def _n(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def on_surface(L, start, way, steps=240, most=0.008):
    """sculpt.py's walk to a layer's surface, in steps of at most `most` (a thick cloth's field read far off)."""
    p = np.array(start, dtype=float)
    w = _n(way)
    for _ in range(steps):
        d = float(L.at_points(p[None, :])[0])
        if abs(d) < 1e-5:
            break
        p = p + w * float(np.clip(d, -most, most))
    if abs(float(L.at_points(p[None, :])[0])) > 0.002:
        raise RuntimeError(f"no surface met from {np.round(start, 3)} along {np.round(way, 3)}")
    e = 1e-4
    g = np.array([(L.at_points((p + np.eye(3)[i] * e)[None, :])[0] - L.at_points((p - np.eye(3)[i] * e)[None, :])[0])
                  for i in range(3)])
    return p, _n(g)


SEED = {"usa": 233, "jap": 57, "uk": 149, "ger": 191}[WHO]      # (his face: a mix no other man's file uses)
# His own paints (past the army's fifteen, so the battle colours them from this file, not from an army's table): the
# file is written with these in place of the army's tunic, trousers, gaiters and helmet (OWN_OF below)
NEW_PAINTS = {"kapok": 41, "shirt_own": 42, "trousers_own": 43, "hat": 44, "ties": 45}
OWN_OF = {0: 42, 1: 43, 3: 43, 4: 45, 5: 44}    # army paint -> his own, in the json (tunic, trousers, lower, straps, helmet)
S.FIGURE_PAINTS.update(NEW_PAINTS)

if WHO == "usa":
    S.NATION_SEED[NATION] = SEED
    SPEC = S.NATION_KIT[NATION]
    SPEC["y_straps"] = False
    SPEC["helmet"] = "dixie"
    SPEC["pouches"] = {"front": 0, "rear": False}
    U.LEGGING_FLARE = (0.0, 0.04)              # (the dungarees' legs: no legging's edge standing off at the foot)
    U.LEGGING_RIM = (0.0, 0.025)               # (nor at its top)
    COLOURS = {
        "tunic": "8CA3BE",     # the chambray work shirt, light blue
        "trousers": "2E3A52",  # the dungarees, dark blue denim
        "lower": "2E3A52",     # (the legs below the knee, the same denim)
        "boots": "4A3A2E",     # the brown work shoes ("boondockers")
        "helmet": "E6E3DA",    # the white hat
        "straps": "6F6A5A",    # the vest's ties and tapes: a cotton tape, darker than the kapok cloth
        "kapok": "8E8A7A",     # the kapok life jacket's grey cloth
        "leather": "2B231D",
    }
    S.LOOK[NATION].update(COLOURS)
    OWN_COLOURS = {"kapok": COLOURS["kapok"], "shirt_own": COLOURS["tunic"], "trousers_own": COLOURS["trousers"],
                   "hat": COLOURS["helmet"], "ties": COLOURS["straps"]}
    VARIANTS = {
        "officer": {"shirt_own": "A8996F", "trousers_own": "9A8C66", "hat": "A49670",
                    "about": "an officer or chief: khaki shirt and trousers (and, if the battle has it, a khaki cap's "
                             "colour on the hat)"},
        "gun_crew": {"about": "a gun crew's man: the same colours, with a steel helmet (a helmet finish, the M1 'pot' "
                              "in deck grey 5D6670) drawn in place of the hat (helmet_paints)"},
    }

elif WHO == "uk":
    # the British sailor (a rating of the Royal Navy at his action station, 1940-43): a navy blue woollen jersey and
    # bell-bottomed blue trousers, black leather shoes, the round flat cap (the crown over a band, no peak), and the
    # inflatable rubber lifebelt in its cloth cover, blown up round his chest under the arms, its mouth tube up on
    # the left
    S.NATION_SEED[NATION] = SEED
    S.LOOK[NATION] = dict(S.LOOK["GER"])
    SPEC = S.NATION_KIT[NATION]
    SPEC.update({"helmet": "navycap", "pack": False, "y_straps": False, "pouches": {"front": 0, "rear": False},
                 "cross_straps": False, "chest_pouches": False})
    COLOURS = {
        "tunic": "262D40",     # the navy blue jersey
        "trousers": "232A3B",  # the blue serge trousers
        "lower": "232A3B",     # (the legs below the knee, the same cloth)
        "boots": "1E1A18",     # black leather shoes
        "helmet": "1D2231",    # the round cap, navy blue
        "straps": "17161A",    # the lifebelt's mouth tube and valve, black rubber
        "kapok": "7B7C6C",     # the lifebelt's cloth cover, grey-khaki
        "leather": "1E1A18",
    }
    S.LOOK[NATION].update(COLOURS)
    OWN_COLOURS = {"kapok": COLOURS["kapok"], "shirt_own": COLOURS["tunic"], "trousers_own": COLOURS["trousers"],
                   "hat": COLOURS["helmet"], "ties": COLOURS["straps"]}
    VARIANTS = {
        "white_cap": {"hat": "E4E1D8",
                      "about": "the summer and the Mediterranean: the cap's white cover over the crown"},
        "sweater": {"shirt_own": "D3CAB2",
                    "about": "the cream roll-neck sea jersey of the North Atlantic escorts and the submarines"},
        "gun_crew": {"about": "a gun crew's man: the same colours, with a steel helmet (a helmet finish, the flat "
                              "broad-brimmed helmet in dark grey 4E5550) drawn in place of the cap (helmet_paints)"},
    }

elif WHO == "ger":
    # the German sailor (a rating of the Navy at his action station, no badge or insignia anywhere): a dark blue
    # woollen pullover with its stand collar and dark blue trousers, black leather shoes (not the soldier's tall
    # boots), the round flat "plate" cap with its two black ribbons hanging behind (no tally on its band), and a
    # grey-green canvas kapok life jacket tied over the pullover
    S.NATION_SEED[NATION] = SEED
    SPEC = S.NATION_KIT[NATION]
    SPEC.update({"helmet": "tellercap", "tall_boots": False, "pack": False, "canister": False, "y_straps": False,
                 "pouches": {"front": 0, "rear": False}, "cross_straps": False, "chest_pouches": False})
    COLOURS = {
        "tunic": "1F2535",     # the dark blue pullover
        "trousers": "1D2230",  # the dark blue trousers
        "lower": "1D2230",     # (the legs below the knee, the same cloth)
        "boots": "1C1917",     # black leather shoes
        "helmet": "1A1F2C",    # the round cap and its ribbons, near black
        "straps": "4E5143",    # the life jacket's ties, a tape a shade darker than its canvas
        "kapok": "6F725E",     # the life jacket's grey-green canvas
        "leather": "1C1917",
    }
    S.LOOK[NATION].update(COLOURS)
    OWN_COLOURS = {"kapok": COLOURS["kapok"], "shirt_own": COLOURS["tunic"], "trousers_own": COLOURS["trousers"],
                   "hat": COLOURS["helmet"], "ties": COLOURS["straps"]}
    VARIANTS = {
        "white_drill": {"shirt_own": "DCD8CB", "trousers_own": "D3CFC1",
                        "about": "the white drill working dress of summer and the Mediterranean: jumper and trousers "
                                 "(the cap stays dark)"},
        "grey_dress": {"shirt_own": "5E6157", "trousers_own": "585B52",
                       "about": "the grey-green working dress worn later in the war"},
        "gun_crew": {"about": "a gun crew's man: the same colours, with a steel helmet (a helmet finish, the flared "
                              "helmet in dark grey 525A5E) drawn in place of the cap (helmet_paints)"},
    }

else:
    # the Japanese sailor: the Navy's white cotton working dress (jumper and trousers), black leather shoes, the soft
    # working cap with its short peak in white, and a tan canvas kapok life jacket over the jumper
    S.NATION_SEED[NATION] = SEED
    SPEC = S.NATION_KIT[NATION]
    # (the soft cap a little fuller than the Chinese soldier's, so it sits over his brow rather than in it)
    S.HELMETS["navcap"] = dict(S.HELMETS["fieldcap"], ax=0.116, ay=0.096, dx=-0.001)
    SPEC.update({"helmet": "navcap", "pack": False, "y_straps": False, "pouches": {"front": 0, "rear": False},
                 "cross_straps": False})
    SPEC.pop("puttees", None)
    COLOURS = {
        "tunic": "D6D2C4",     # the white working jumper, a little worn
        "trousers": "CFCBBC",  # its trousers
        "lower": "CFCBBC",     # (the legs below the knee, the same cloth)
        "boots": "2B2420",     # black leather shoes
        "helmet": "DAD6C9",    # the white working cap
        "straps": "6A6046",    # the vest's ties
        "kapok": "A3916C",     # the life jacket's tan canvas
        "leather": "2A2420",
    }
    S.LOOK[NATION].update(COLOURS)
    OWN_COLOURS = {"kapok": COLOURS["kapok"], "shirt_own": COLOURS["tunic"], "trousers_own": COLOURS["trousers"],
                   "hat": COLOURS["helmet"], "ties": COLOURS["straps"]}
    VARIANTS = {
        "green": {"shirt_own": "77785E", "trousers_own": "707259", "hat": "77785E",
                  "about": "the later war's grey-green working dress (or a landing party's): jumper, trousers and cap"},
        "gun_crew": {"about": "a gun crew's man: the same colours, with a steel helmet (a helmet finish, the dome in "
                              "grey-green 6B6A55) drawn in place of the cap (helmet_paints)"},
    }

# ================================================================================================ the shirt, the dungarees
VEST_OFF = 0.034           # (m: how thick the kapok stands over the shirt)
VEST_DOWN = 0.075          # (m: the vest's hem under the belt's middle)
VEST_NECK = 0.074          # (m: the vest's neck opening round the neck's axis)
ARMHOLE = (0.05, -0.027, 0.06, 0.16)   # (m: the armhole, an upright oval through him front to back: its middle this far
#                                         out and down from the shoulder's joint, and its half width and half height)
SEAM = 0.02                # (m: the shirt is taken as his trunk to this far out past the shoulder's joint: the arm, hung
#                            close beside him, is no part of what the vest is stood over)
GROOVES = (0.065, 0.13)    # (the kapok's channels, this far either side of the middle, front and back)
TIES = (0.075, 0.18, 0.285)    # (the three ties across the front, this far over the belt)
VEST_ROUND = 0.0           # (m: the vest's edges rounded over, at the hem, the armholes and the neck)
VEST_GAP = 0.02            # (m: how deep the groove where its two halves meet goes)
if WHO not in ("usa", "jap"):
    # (from the German sailor on, 22 November: the edges rounded, a wider armhole, so the edge is the vest's own and
    #  runs clean, and a shallower groove that never shows the shirt through it; the American's and the Japanese
    #  sailor's files were made with the values above, and keep them until they are made again)
    ARMHOLE = (0.03, -0.027, 0.065, 0.16)
    VEST_ROUND = 0.014
    VEST_GAP = 0.012


class Box:
    def __init__(self, lo, hi):
        self.lo, self.hi = np.asarray(lo, float), np.asarray(hi, float)
        self.c, self.R = (self.lo + self.hi) / 2, None

    def d(self, px, py, pz):
        p = np.stack(np.broadcast_arrays(np.asarray(px, float), np.asarray(py, float), np.asarray(pz, float)), axis=-1)
        q = np.abs(p - self.c) - (self.hi - self.lo) / 2
        return np.linalg.norm(np.maximum(q, 0.0), axis=-1) + np.minimum(q.max(-1), 0.0)


def _seg_dist(P, a, b):
    ab = b - a
    t = np.clip(((P - a) @ ab) / (ab @ ab), 0.0, 1.0)
    return np.linalg.norm(P - (a + t[..., None] * ab), axis=-1)


class VestRegion:
    """Where the vest is (negative inside): round the trunk from its hem to over the shoulders, less the neck (a
    cylinder along the neck's axis, above the neck's foot) and the armholes (an upright oval each side, through him
    from front to back: the vest's edge meets its own surface square on, so it is a clean edge, not a fringe)."""

    def __init__(self, z0, z1, neck_c, neck_ax, hole):
        self.box = Box((-0.32, -0.34, z0), (0.32, 0.34, z1))
        self.lo, self.hi, self.c, self.R = self.box.lo, self.box.hi, self.box.c, None
        self.nc, self.na, self.hole = np.asarray(neck_c, float), _n(neck_ax), hole

    def d(self, px, py, pz):
        bx, by, bz = np.broadcast_arrays(np.asarray(px, float), np.asarray(py, float), np.asarray(pz, float))
        P = np.stack([bx, by, bz], axis=-1)
        out = self.box.d(bx, by, bz)
        v = P - self.nc
        along = v @ self.na
        rad = np.linalg.norm(v - along[..., None] * self.na, axis=-1)
        neck = np.where(along > -0.025, VEST_NECK - rad, -1.0)
        out = np.maximum(out, neck)
        yc, zc, ry, rz = self.hole
        e = np.sqrt(((np.abs(by) - yc) / ry) ** 2 + ((bz - zc) / rz) ** 2)
        return np.maximum(out, (1.0 - e) * ry)


class Trunk:
    """A cloth's field (a shape's d) with the arms left off: cut square at |y| = y_cut (the arm hangs out past it)."""

    def __init__(self, src, y_cut):
        self.src, self.y_cut = src, y_cut

    def at_points(self, P):
        P = np.asarray(P, float)
        return np.maximum(self.src.d(P[:, 0], P[:, 1], P[:, 2]), np.abs(P[:, 1]) - self.y_cut)


# how loose soldier.py's tunic stands off the body, bone by bone: the vest stands VEST_OFF over that
TUNIC_LOOSE = {"pelvis": 0.017, "spine_01": 0.017, "spine_02": 0.015, "chest": 0.012, "clavicle_l": 0.01, "clavicle_r": 0.01,
               "neck": 0.007}
VEST_BAND = 0.075          # (m: the body's shell read exactly this far out, so the vest's surface is in it)


class Over:
    """A cloth's own surface stood `off` metres out of it, kept to `region` (soldier_tankers.py's); `round` rounds its
    edges where the region ends it (a blend that many metres), so a padded edge rolls over to meet the cloth under it
    steeply, and the line where their paints meet runs clean rather than stepping from point to point of the mesh."""

    def __init__(self, src, off, region, round=0.0):
        self.src, self.off, self.region, self.round = src, off, region, round
        self.lo, self.hi = region.lo, region.hi
        self.c, self.R = region.c, None

    def d(self, px, py, pz):
        bx, by, bz = np.broadcast_arrays(np.asarray(px, float), np.asarray(py, float), np.asarray(pz, float))
        shape = bx.shape
        r = self.region.d(bx, by, bz)
        out = np.array(r, dtype=float, copy=True)
        near = r < 0.03
        if near.any():
            P = np.stack([bx[near], by[near], bz[near]], axis=1)
            out[near] = smax(self.src.at_points(P) - self.off, r[near], self.round)
        return out.reshape(shape)


def snapshot(L):
    c = Layer(L.name + "_was", L.paint)
    c.ops = list(L.ops)
    return c


def _is_box(p, half, tol=1e-6):
    return isinstance(p, RoundBox) and np.allclose(p.half, half, atol=tol)


def kapok_vest(man, J, LM, shirt):
    """The kapok life jacket: the shirt's own surface stood VEST_OFF out over the trunk (sleeveless, open at the neck),
    its kapok in vertical channels, a thick pad round the back of the neck, open down the front and tied across it."""
    sh = np.array(J["upperarm_l"])
    # the vest is the body's own shell stood out (as the tunic is, VEST_OFF further), read exactly that far out: not
    # the shirt's field, whose pockets, folds and placket would swell into lumps, and which is only exact near it
    t0 = time.time()
    shell = S.ShellField(man["tights"], band=VEST_BAND)
    base = shell.cloth({b: v + VEST_OFF for b, v in TUNIC_LOOSE.items()}, default=-0.06)
    print(f"  sailor: the body's shell read {VEST_BAND * 100:.1f} cm out in {time.time() - t0:.0f} s")
    was = Trunk(base, abs(float(sh[1])) + SEAM + VEST_OFF)
    hole = (abs(float(sh[1])) + ARMHOLE[0], float(sh[2]) + ARMHOLE[1], ARMHOLE[2], ARMHOLE[3])
    nk = np.array(J["neck"])
    z0 = LM["belt"] - VEST_DOWN
    z1 = LM["shoulder"] + 0.16
    vest = Layer("kapok", "kapok")
    vest.add(Over(was, 0.0, VestRegion(z0, z1, nk, LM["neck_axis"], hole), VEST_ROUND), 0.0)
    # ---- open down the front: the two halves meet over the breastbone with a groove between them
    zt = LM["collar"] - 0.03
    xf = float(on_surface(vest, (0.45, 0.0, (z0 + zt) / 2), (-1.0, 0.0, 0.0))[0][0])
    vest.sub(RoundBox((xf + 0.03 - VEST_GAP, 0.0, (z0 + zt) / 2), (0.03, 0.006, (zt - z0) / 2 + 0.02), 0.004), 0.004)
    # ---- the kapok in channels: a groove down the vest between its pads, front and back
    for side in (1.0, -1.0):
        for y in GROOVES:
            for fb in (1.0, -1.0):
                pts = []
                for z in np.linspace(z0 + 0.03, LM["shoulder"] - 0.03 - 0.05 * (y > 0.15), 7):
                    try:
                        p, nrm = on_surface(vest, (0.45 * fb, y * side, z), (-fb, 0.0, 0.0))
                    except RuntimeError:
                        continue
                    pts.append(p + nrm * 0.003)
                for a, b in zip(pts[:-1], pts[1:]):
                    vest.sub(RoundCone(a, b, 0.0075, 0.0075), 0.004)
    # ---- the pad round the back of the neck (it holds the head up in the water)
    nmid, nfwd, nrx, nry = LM["neck_ring"]
    ax = LM["neck_axis"]
    side_ax = np.cross(ax, nfwd)
    pts = []
    for b in np.linspace(math.radians(70.0), math.radians(290.0), 9):
        pts.append(nmid + ax * 0.004 + nfwd * math.cos(b) * (nrx + 0.05) + side_ax * math.sin(b) * (nry + 0.05))
    for a, b in zip(pts[:-1], pts[1:]):
        vest.add(RoundCone(a, b, 0.034, 0.034), 0.03)
    # ---- the ties: three tapes across the opening, a bow at each
    ties = Layer("vest_ties", "straps")
    for h in TIES:
        z = LM["belt"] + h
        p, nrm = on_surface(vest, (0.45, 0.0, z), (-1.0, 0.0, 0.0))
        ties.add(RoundBox(p + nrm * 0.001, (0.004, 0.05, 0.007), 0.002), 0.0)
        ties.add(Sphere(p + nrm * 0.006, 0.011), 0.004)
    LM["vest"] = (z0, z1)
    print(f"  sailor: the kapok vest from {z0:.3f} to the shoulders, {VEST_OFF * 100:.1f} cm thick")
    return [vest, ties]


BELT_OFF = 0.044           # (m: how far the blown-up lifebelt stands over the jersey at its fattest)
BELT_UP = (0.205, 0.052)   # (m: its middle this far over the belt's middle, and its half height)
TUBE = 0.0065              # (m: the mouth tube's radius)
# (round in section, the section a circle closed on itself in the shell's own measure, so it needs no planes to end it:
#  a band cut by planes left a flat ledge on his back, where the jersey lies further in, and the paint line along it
#  stepped from point to point of the mesh)


class Puffed:
    """A blown-up tube round him, from z0 to z1, standing `off` out of a cloth's own surface at its fattest: its section a
    circle (radius half its height) whose middle lies just under that surface, measured by the cloth's own distance, so
    it follows his shape round and closes on itself top and bottom wherever the jersey under it lies. Kept to `region`
    (a box a little bigger than the tube, so it never cuts it)."""

    def __init__(self, src, off, z0, z1, region):
        self.src, self.zc, self.region = src, (z0 + z1) / 2, region
        self.R = (z1 - z0) / 2
        self.under = self.R - off                     # (the circle's middle this far under the cloth's surface)
        self.lo, self.hi = region.lo, region.hi
        self.c = region.c

    def d(self, px, py, pz):
        bx, by, bz = np.broadcast_arrays(np.asarray(px, float), np.asarray(py, float), np.asarray(pz, float))
        shape = bx.shape
        r = self.region.d(bx, by, bz)
        out = np.array(r, dtype=float, copy=True)
        near = r < 0.03
        if near.any():
            P = np.stack([bx[near], by[near], bz[near]], axis=1)
            rho = self.src.at_points(P) + self.under
            out[near] = np.maximum(np.hypot(rho, bz[near] - self.zc) - self.R, r[near])
        return out.reshape(shape)


def lifebelt(man, J, LM, shirt):
    """The Royal Navy's inflatable lifebelt, blown up: a rubber tube in its cloth cover round his chest under the arms
    (the jersey's own surface stood out, round in section), its mouth tube and valve up on the left of his chest."""
    sh = np.array(J["upperarm_l"])
    t0 = time.time()
    shell = S.ShellField(man["tights"], band=VEST_BAND)
    base = shell.cloth(dict(TUNIC_LOOSE), default=-0.06)
    print(f"  sailor: the body's shell read {VEST_BAND * 100:.1f} cm out in {time.time() - t0:.0f} s")
    was = Trunk(base, abs(float(sh[1])) + SEAM + BELT_OFF)
    zc = LM["belt"] + BELT_UP[0]
    z0, z1 = zc - BELT_UP[1], zc + BELT_UP[1]
    belt = Layer("kapok", "kapok")
    belt.add(Puffed(was, BELT_OFF, z0, z1, Box((-0.32, -0.34, z0 - 0.02), (0.32, 0.34, z1 + 0.02))), 0.0)
    # ---- the mouth tube: from the belt's top on his left front up his chest to the valve
    tube = Layer("vest_ties", "straps")
    p0, n0 = on_surface(belt, (0.45, 0.085, zc + BELT_UP[1] * 0.55), (-1.0, 0.0, 0.0))
    p1, n1 = on_surface(was, (0.45, 0.095, zc + 0.1), (-1.0, 0.0, 0.0))
    a = p0 - n0 * 0.008
    b = p1 + n1 * 0.016
    tube.add(RoundCone(a, b, TUBE, TUBE), 0.002)
    tube.add(Sphere(b + n1 * 0.002, TUBE * 1.6), 0.003)
    LM["vest"] = (z0, z1)
    print(f"  sailor: the lifebelt from {z0:.3f} to {z1:.3f}, {BELT_OFF * 100:.1f} cm proud at its middle")
    return [belt, tube]


def _placed(p):
    """A shape that has a place (no NaN in its box; an endless box, a half space's, is a place; so is no box)."""
    if not isinstance(p, (RoundBox, RoundCone, Sphere, Torus)):
        return True
    try:
        lo, hi = np.asarray(p.lo, float), np.asarray(p.hi, float)
    except (AttributeError, TypeError, ValueError):
        return True
    return not bool(np.any(np.isnan(lo)) or np.any(np.isnan(hi)))


_uniform_prev = S.uniform_layers


def uniform_layers(man, J, spec, LM):
    if spec is not S.NATION_KIT[NATION]:
        return _uniform_prev(man, J, spec, LM)
    LM["hem"] = LM["belt"] - 0.05                  # (the shirt tucked in: it ends at the waist, under the vest)
    layers = _uniform_prev(man, J, spec, LM)
    shirt, trousers = layers[0], layers[1]
    # ---- off: soldier_usa.py's shoulder straps (a work shirt has none)
    before = len(shirt.ops)
    shirt.ops = [(how, p, k) for how, p, k in shirt.ops
                 if not (how == "add" and isinstance(p, RoundBox) and abs(p.half[1] - 0.021) < 1e-6 and abs(p.half[2] - 0.0042) < 1e-6)]
    J["usa_shoulder_straps"] = []
    LM["shoulder_straps"] = []
    print(f"  sailor: {before - len(shirt.ops)} pieces of the field jacket's shoulder straps taken off")
    # ---- no skirt: the shirt tucked into the dungarees (the jacket's skirt flared out from under the vest's hem)
    before = len(shirt.ops)
    shirt.ops = [(how, p, k) for how, p, k in shirt.ops if not (how == "add" and isinstance(p, S.Blend))]
    print(f"  sailor: {before - len(shirt.ops)} pieces of the jacket's skirt taken off")
    # ---- nor its pockets, buttons, placket, folds and yoke: all under the vest (and the buttons that were below the
    # shorter shirt's hem would find no cloth to sit on)
    before = len(shirt.ops)
    shirt.ops = [(how, p, k) for how, p, k in shirt.ops if not (how == "add" and isinstance(p, (RoundBox, RoundCone, Sphere, Torus)))]
    print(f"  sailor: {before - len(shirt.ops)} of the jacket's pockets, buttons and folds taken off (under the vest)")
    for L in layers:
        bad = [o for o in L.ops if not _placed(o[1])]
        if bad:
            L.ops = [o for o in L.ops if o not in bad]
            print(f"  sailor: {len(bad)} shapes with no place taken off {L.name}")
    # ---- the dungarees' legs straight to the shoes: no blousing over a legging's top, no lacing up its side
    before = len(trousers.ops)
    trousers.ops = [(how, p, k) for how, p, k in trousers.ops
                    if not (how == "add" and isinstance(p, Torus) and float(p.c[2]) < LM["knee"])]
    print(f"  sailor: {before - len(trousers.ops)} blousings taken off the trousers' legs")
    for L in layers:
        if L.name == "gaiter_straps":
            L.ops = []
    layers = [L for L in layers if L.ops]
    layers += (lifebelt if WHO == "uk" else kapok_vest)(man, J, LM, shirt)
    return layers


S.uniform_layers = uniform_layers
S.BUDGET["uniform"] = S.BUDGET["uniform"] + 3000

_kit_prev = S.build_kit


def build_kit(J, spec, body_ls, Z=None):
    if spec is S.NATION_KIT[NATION] and WHO in ("uk", "ger"):
        return {}                                 # (nothing to build: the jersey is shorter than a tunic's hip)
    out = _kit_prev(J, spec, body_ls, Z)
    if spec is not S.NATION_KIT[NATION]:
        return out
    for k in list(out):                           # (no belt, pouches, pack or canteen: the vest over his shirt)
        bpy.data.objects.remove(out.pop(k)[0])
    return out


S.build_kit = build_kit

# ---- the white hat: a soft crown standing over the head, its brim turned up all round
S.HELMETS["dixie"] = dict(ax=0.104, ay=0.094, az=0.118, dx=-0.006, n=3.0, edge=S._cos3(0.036, 0.024, 0.016),
                          flare=S._cos3(0.003, 0.003, 0.003), drop=S._cos3(0.002, 0.002, 0.002), cloth=True)
BRIM = (0.05, 0.007, 14.0)        # (the turned-up brim: its height, thickness, and how far it leans out, degrees)
_helmet_prev = S.build_helmet


def dixie_hat(J, sides=36):
    ob = _helmet_prev("dixie", J, sides)
    k = dict(S.HELMETS["dixie"])
    C = np.array((J["skull_x"] - 0.004 + k["dx"], 0.0, J["crown"] - 0.108))
    ay = k["ay"] * (J["skull_half"][1] + 0.025) / 0.108
    ax = k["ax"] * (J["skull_half"][0] + 0.023) / 0.122
    g = S.Geo()
    lean = math.radians(BRIM[2])
    pts, nrm = [], []
    for i in range(sides):
        f = 2 * math.pi * i / sides
        rho = 1.0 / math.sqrt((math.cos(f) / ax) ** 2 + (math.sin(f) / ay) ** 2)
        zE = k["edge"](f)
        out = np.array((math.cos(f), math.sin(f), 0.0))
        up = np.array((0.0, 0.0, 1.0))
        rise = up * math.cos(lean) + out * math.sin(lean)              # (the brim's way up its face, leaning out)
        foot = C + out * (rho + 0.003) + up * (zE - 0.006)
        pts.append(foot + rise * BRIM[0] * 0.5 + out * BRIM[1])
        nrm.append(_n(out * math.cos(lean) - up * math.sin(lean)))
    g.band(pts, nrm, BRIM[0], BRIM[1], "helmet", closed=True)
    brim = g.build("hat_brim", sharp=60.0)
    print(f"  sailor: the white hat {S.tris_of(ob)} triangles, its brim {S.tris_of(brim)}")
    return S.join_meshes([ob, brim], ob.name.split(".")[0])


# ---- the Royal Navy rating's round cap: a band round the head and a flat crown over it, wider all round, no peak
S.HELMETS["navycap"] = dict(S.HELMETS["dixie"])
S.HELMETS["tellercap"] = dict(S.HELMETS["dixie"])
CAP = dict(clear=0.01, rim=0.03, top=0.012, back=0.008, foot=0.016, dome=0.131)
# the German rating's plate cap: the same make with a wider, flatter crown, and two black ribbons hanging behind from
# its band (each RIBBONS: how long, how wide, how thick; they hang out from his neck as they go down)
CAP_TELLER = dict(CAP, clear=0.008, rim=0.045, top=0.01, back=0.004)
RIBBONS = (0.15, 0.026, 0.0015)
# (m: the crown's top this far over the skull's crown; how far the crown stands out past the band; its thickness; how
#  much lower it sits behind; the band's foot this far over the white hat's edge, up at his hairline; the height of
#  the white hat's dome, which holds his skull, so the band is drawn in to where the skull is that high)


def navy_cap(J, sides=36, CAP=CAP, kind="navycap", ribbons=None):
    k = dict(S.HELMETS[kind])
    C = np.array((J["skull_x"] - 0.004 + k["dx"], 0.0, J["crown"] - 0.108))
    ay = k["ay"] * (J["skull_half"][1] + 0.025) / 0.108
    ax = k["ax"] * (J["skull_half"][0] + 0.023) / 0.122
    zE = [k["edge"](2 * math.pi * i / sides) - 0.006 + CAP["foot"] for i in range(sides)]
    zr0 = 0.108 + CAP["clear"] - CAP["top"]                     # (the crown's rim: its top clears the skull's crown)
    outer, inner = [], []
    for i in range(sides):
        f = 2 * math.pi * i / sides
        rho = 1.0 / math.sqrt((math.cos(f) / ax) ** 2 + (math.sin(f) / ay) ** 2)
        zr = max(zr0 + CAP["back"] * 0.5 * (math.cos(f) - 1.0), zE[i] + 0.052)
        rb = rho * (1.0 - min(0.99, max(0.0, zE[i] / CAP["dome"])) ** 3) ** (1.0 / 3.0)     # (the band, round his skull)
        outer.append([(rho * 0.55 + 0.03, zr + CAP["top"]),                    # the flat top
                      (rho + CAP["rim"] - 0.006, zr + CAP["top"] * 0.85),      # the crown's edge, rounded
                      (rho + CAP["rim"], zr + CAP["top"] * 0.4),
                      (rho + CAP["rim"] - 0.004, zr - 0.001),
                      (rho + 0.014, zr - 0.012),                               # the crown flaring out from the band
                      (rb + 0.007, zr - 0.021),
                      (rb + 0.005, zE[i])])                                    # the band's foot, on his head
        inner.append([(rb - 0.001, zE[i] + 0.004), (rb - 0.002, zr - 0.024)])
    def ring(lst, j):
        out = []
        for i in range(sides):
            f = 2 * math.pi * i / sides
            r, z = lst[i][j]
            out.append(C + np.array((math.cos(f) * r, math.sin(f) * r, z)))
        return out
    top = zr0 + CAP["top"] + 0.001
    rings = [[C + np.array((0.0, 0.0, top))]]
    rings += [ring(outer, j) for j in range(len(outer[0]))]
    rings += [ring(inner, j) for j in range(len(inner[0]))]
    rings += [[C + np.array((0.0, 0.0, top - 0.02))]]
    g = S.Geo()
    g.shell(rings, "helmet")
    if ribbons:
        ln, wd, th = ribbons
        i0 = sides // 2                                         # (straight behind him)
        rb, zf = outer[i0][-1]
        for sgn in (-1.0, 1.0):
            pts, nrm = [], []
            for t in np.linspace(0.0, 1.0, 8):
                pts.append(C + np.array((-(rb + 0.004 + th + 0.03 * t * t), sgn * (0.011 + 0.014 * t), zf + 0.016 - ln * t)))
                nrm.append(_n((-1.0, sgn * 0.2, 0.06 * t)))
            g.band(pts, nrm, wd, th, "helmet")
    ob = g.build(f"helmet_{kind}", sharp=50.0)
    print(f"  sailor: the {'plate' if ribbons else 'round'} cap {S.tris_of(ob)} triangles")
    return ob


def build_helmet(kind, J, sides=None):
    if kind == "dixie" and "crown" in J:
        return dixie_hat(J, sides or 36)
    if kind == "navycap" and "crown" in J:
        return navy_cap(J, sides or 36)
    if kind == "tellercap" and "crown" in J:
        return navy_cap(J, sides or 36, CAP_TELLER, "tellercap", RIBBONS)
    return _helmet_prev(kind, J, sides)


S.build_helmet = build_helmet

# ---- no weapon in his files: the rifle soldier.py makes for every man is left out of what is exported
_man_objects_prev = S.man_objects


def man_objects(m):
    if m.get("sailor"):
        return [m["body"]] + m["kit"] + [m["helmet"]]
    return _man_objects_prev(m)


S.man_objects = man_objects


# ================================================================================================ the clips
# On deck: the motion capture with his arms free (no rifle carried over it). The rest keyed here on his rig.
DECK = {"idle": "idle", "walk": "walk", "run": "run", "cower": "cower", "die_back": "fall_back", "die_forward": "fall_forward"}
S.MOCAP.clear()
S.MOCAP.update({name: {"src": src} for name, src in DECK.items()})
S.KEYED_DROP.clear()
S.CLIPS.clear()
for _name, _spec in S.MOCAP.items():
    _c = S.MO_DATA[_spec["src"]]
    S.CLIPS[_name] = {"long": (_c["frames"] - 1) / _c["fps"], "loop": bool(_c["loop"]), "stride": float(_c["stride"]),
                      "events": [], "fn": None, "weapon": "rifle", "mocap": _spec,
                      "end": list(_c.get("end_offset", (0.0, 0.0, 0.0)))}

G = 9.81
GUN_AWAY = M4(np.eye(3), (0.2, 0.0, -2.0))      # (the weapon bone 2 m under his root: he carries none)
MAN = {}                                        # his measures (use_man)
WATER = {}                                      # the clips keyed here: {name: dict(root=..., pelvis_z=..., ...)}


def use_man(m):
    """The clips are set out on the man they are played on: his pelvis's rest height, hips, legs, chest."""
    rig = m["rig"]
    H0 = rig.H0
    MAN["pz"] = float(H0["pelvis"][2])
    MAN["hip"] = {s: np.array(H0["thigh_" + s] - H0["pelvis"]) for s in ("l", "r")}
    MAN["thigh"] = float(np.linalg.norm(H0["calf_l"] - H0["thigh_l"]))
    MAN["shin"] = float(np.linalg.norm(H0["foot_l"] - H0["calf_l"]))
    MAN["leg"] = MAN["thigh"] + MAN["shin"]
    MAN["ankle"] = float(H0["foot_l"][2])
    MAN["chest"] = np.array(H0["chest"])
    MAN["shoulder"] = float(H0["upperarm_l"][2])
    m["sailor"] = True


def water_clip(name, long, loop=False, stride=0.0, events=(), root="surface", climb=0.0, about="", end=None):
    def deco(fn):
        S.CLIPS[name] = {"long": long, "loop": loop, "stride": stride, "events": list(events), "fn": fn, "weapon": "rifle"}
        WATER[name] = {"root": root, "climb": climb, "about": about, "end": end}
        return fn
    return deco


def sev(t, fx, cue, **more):
    e = {"t": round(float(t), 4), "fx": fx, "socket": None, "cue": cue}
    e.update(more)
    return e


# ---- the solver for these clips: the root may be under the water (no knee is kept off the ground: P free_knees),
# the low poses not scaled to the man (rig.fit, as the crew's), and a foot may follow its shin (P flex_l / flex_r:
# the ankle's bend from the shin's own line, toes down +, so a foot hangs or points as the leg lies)
_solve_prev = S.Rig.solve


def _solve(self, P, lift_knees=4):
    if P.get("free_knees", 0.0) > 0.5:
        lift_knees = 0
    st = _solve_prev(self, P, lift_knees)
    if P.get("shin_feet", 0.0) > 0.5:
        for s in ("l", "r"):
            D = st["D"]["calf_" + s] @ R_axis((0, 1, 0), float(P.get("flex_" + s, 0.0)))
            st["D"]["foot_" + s] = D
            st["D"]["ball_" + s] = D @ R_axis((0, 1, 0), -float(P.get("toe_" + s, 0.0)))
            st["H"]["ball_" + s] = st["H"]["foot_" + s] + D @ (self.H0["ball_" + s] - self.H0["foot_" + s])
    return st


S.Rig.solve = _solve
_clip_state_prev = S.clip_state


def clip_state(rig, name, t, kind=None):
    if name not in WATER:
        return _clip_state_prev(rig, name, t, kind)
    keep = rig.fit
    rig.fit = (1.0, 1.0)
    try:
        return _clip_state_prev(rig, name, t, kind)
    finally:
        rig.fit = keep


S.clip_state = clip_state


def looped(keys, long):
    """Key poses [(t, P)] passed through smoothly round a loop (Catmull-Rom, the first key again at t = long)."""
    times = [k[0] for k in keys]
    poses = [k[1] for k in keys]
    n = len(keys)

    def at(t):
        t = t % long
        i = max(j for j in range(n) if times[j] <= t)
        t1 = times[i + 1] if i + 1 < n else long
        u = (t - times[i]) / (t1 - times[i])
        p0, p1, p2, p3 = poses[(i - 1) % n], poses[i], poses[(i + 1) % n], poses[(i + 2) % n]
        w = ((-u ** 3 + 2 * u * u - u) * 0.5, (3 * u ** 3 - 5 * u * u + 2) * 0.5, (-3 * u ** 3 + 4 * u * u + u) * 0.5, (u ** 3 - u * u) * 0.5)
        Ps, ws = [], []
        for P, wi in zip((p0, p1, p2, p3), w):
            if abs(wi) > 1e-9:
                Ps.append(P)
                ws.append(wi)
        return S.mix(Ps, ws)
    return at


def body(pz, x=0.0, y=0.0, pitch=0.0, roll=0.0, yaw=0.0, bend=(0.0, 0.0, 0.0), neck=(0.0, 0.0, 0.0), head=(0.0, 0.0, 0.0)):
    """A pose's trunk: the pelvis's head at height pz (the root's frame) and (x, y), turned (pitch forward, roll to his
    left, yaw to his left); the spine bent by `bend` (forward, left, turn), spread up its three bones."""
    b = np.asarray(bend, float)
    return {"pelvis_at": (x, y, pz - MAN["pz"]), "pelvis": (pitch, roll, yaw),
            "spine_01": tuple(0.35 * b), "spine_02": tuple(0.35 * b), "chest": tuple(0.3 * b),
            "neck": tuple(neck), "head": tuple(head), "clavicle_l": (0.0, 3.0), "clavicle_r": (0.0, 3.0),
            "weapon": GUN_AWAY, "free_knees": 1.0, "shin_feet": 1.0, "flex_l": 30.0, "flex_r": 30.0,
            "fingers_l": 25.0, "fingers_r": 25.0, "thumb_l": 20.0, "thumb_r": 20.0}


def pelvis_R(P):
    return E(*P["pelvis"])


def hip_at(P, s):
    pa = np.asarray(P["pelvis_at"], float)
    return np.array((0.0, 0.0, MAN["pz"])) + pa + pelvis_R(P) @ MAN["hip"][s]


def leg(P, s, off, pole=(1.0, 0.0, 0.0), flex=30.0, frame="pelvis"):
    """foot_s placed `off` from his hip (in the pelvis's frame: x ahead of him, y to his left, z up his trunk; or in
    the root's frame), the knee bending toward `pole` (the same frame), the foot `flex` off the shin's line."""
    side = 1.0 if s == "l" else -1.0
    o = np.array((off[0], off[1] * side, off[2]))
    pl = np.array((pole[0], pole[1] * side, pole[2]))
    R = pelvis_R(P) if frame == "pelvis" else np.eye(3)
    h = hip_at(P, s)
    at = h + R @ o
    d = at - h
    reach = 0.985 * MAN["leg"]
    if np.linalg.norm(d) > reach:
        at = h + d / np.linalg.norm(d) * reach
    P["foot_" + s] = {"at": tuple(float(v) for v in at), "pole": tuple(float(v) for v in R @ pl)}
    P["flex_" + s] = float(flex)
    return P


def arms(P, l=None, r=None):
    """Hands by the arm's own angles (elevation, plane, rotation, elbow, pronation[, wrist flex, deviation]; see
    soldier.Rig.arm_place), or a hand spec as it is."""
    for s, a in (("l", l), ("r", r)):
        if a is None:
            continue
        P["hand_" + s] = a if isinstance(a, dict) else {"arm": tuple(float(v) for v in a)}
    return P


def chest_hand(at, a, n, pole=None):
    h = {"frame": "chest", "at": tuple(at), "a": tuple(_n(a)), "n": tuple(_n(n))}
    if pole is not None:
        h["pole"] = tuple(pole)
    return h


def world_hand(at, a, n, pole):
    return {"at": tuple(float(v) for v in at), "a": tuple(_n(a)), "n": tuple(_n(n)), "pole_world": tuple(float(v) for v in pole)}


def crossed_arms(P):
    """Arms crossed over the vest, each hand flat on the other side's front, the right over the left: how a man goes
    over the side in a life jacket (it holds the jacket down, so it does not ride up into his chin)."""
    P["hand_l"] = chest_hand((0.205, -0.085, 0.075), (0.15, -0.75, 0.6), (-1.0, 0.0, 0.1), pole=(-0.2, 0.7, -1.0))
    P["hand_r"] = chest_hand((0.23, 0.075, 0.025), (0.15, 0.75, 0.6), (-1.0, 0.0, 0.1), pole=(-0.2, -0.7, -1.0))
    P["fingers_l"] = P["fingers_r"] = 12.0
    P["thumb_l"] = P["thumb_r"] = 10.0
    return P


def shift(P, d):
    """The whole pose moved by d (the root's frame): the pelvis and the feet (and any hand placed in the root's
    frame), so a pose made standing can be put anywhere, the legs as they were under him."""
    d = np.asarray(d, float)
    P = dict(P)
    P["pelvis_at"] = tuple(np.asarray(P["pelvis_at"], float) + d)
    for s in ("l", "r"):
        f = P.get("foot_" + s)
        if isinstance(f, dict) and "at" in f:
            f = dict(f)
            f["at"] = tuple(np.asarray(f["at"], float) + d)
            P["foot_" + s] = f
        h = P.get("hand_" + s)
        if isinstance(h, dict) and "at" in h and "frame" not in h:
            h = dict(h)
            h["at"] = tuple(np.asarray(h["at"], float) + d)
            P["hand_" + s] = h
    return P


def put(P, x, y, z):
    """The pose moved so its pelvis's head is at (x, y, z) (the root's frame)."""
    pa = np.asarray(P["pelvis_at"], float) + np.array((0.0, 0.0, MAN["pz"]))
    return shift(P, np.array((x, y, z)) - pa)


def sm(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def ramp(t, t0, t1):
    return sm((t - t0) / (t1 - t0)) if t1 > t0 else float(t >= t0)


# ---------------------------------------------------------------------------------------------------- in the water
TREAD_Z = -0.35          # (the pelvis's head under the surface, treading: the vest's top and his shoulders out)
TREAD_LONG = 2.0


def tread_legs(P, t, long, kicks=2, depth=0.0):
    """An eggbeater of the legs under him: each foot round a circle in front of its hip, the two half a turn apart."""
    for s, ph in (("l", 0.0), ("r", math.pi)):
        a = 2 * math.pi * kicks * t / long + ph
        leg(P, s, (0.2 + 0.14 * math.cos(a), 0.04, -0.63 + 0.12 * math.sin(a) - depth), pole=(1.0, 0.35, 0.25),
            flex=35.0 + 15.0 * math.sin(a))
    return P


def scull(t, long, k=2, phase=0.0, side=1.0):
    """An arm sculling at the surface: the hand swept out and in, palm down, tilted the way it goes."""
    a = 2 * math.pi * k * t / long + phase
    return (50.0 + 6.0 * math.sin(a), 38.0 + 26.0 * math.sin(a), 12.0, 72.0 - 10.0 * math.sin(a), 62.0 + 18.0 * math.cos(a), 0.0, 0.0)


def tread_pose(t, long=TREAD_LONG):
    a = 2 * math.pi * t / long
    P = body(TREAD_Z + 0.022 * math.sin(2 * a), pitch=9.0 + 2.0 * math.sin(2 * a + 0.6), yaw=3.0 * math.sin(a),
             bend=(5.0, 0.0, 0.0), neck=(-8.0, 0.0, 10.0 * math.sin(a + 0.4)), head=(-5.0, 0.0, 12.0 * math.sin(a + 0.4)))
    tread_legs(P, t, long)
    arms(P, scull(t, long), scull(t, long, phase=0.25))
    P["fingers_l"] = P["fingers_r"] = 10.0
    return S.breathe(P, t, long, 0.8)


@water_clip("tread", TREAD_LONG, loop=True, about="Treading water upright in his vest, head and shoulders out: the legs "
            "in an eggbeater under him, the hands sculling at the surface, looking about.")
def c_tread(t):
    return tread_pose(t)


WAVE_LONG = 1.6


@water_clip("wave", WAVE_LONG, loop=True, about="In the water, waving for rescue: kicking harder to rise (the pelvis 5 cm "
            "higher than treading), the left hand sculling, the right arm up over his head waving side to side twice a "
            "loop, the hand open, looking up and ahead.")
def c_wave(t):
    a = 2 * math.pi * t / WAVE_LONG
    P = body(TREAD_Z + 0.05 + 0.03 * math.sin(2 * a), pitch=6.0, roll=-5.0, bend=(2.0, -4.0, 0.0),
             neck=(-14.0, 0.0, 4.0 * math.sin(a)), head=(-10.0, 0.0, 6.0 * math.sin(a)))
    tread_legs(P, t, WAVE_LONG, kicks=2, depth=0.03)
    w = math.sin(2 * a)
    arms(P, scull(t, WAVE_LONG, k=2), (142.0 + 22.0 * w, 12.0, -10.0, 18.0 + 6.0 * w, -70.0, -10.0 * w, 0.0))
    P["fingers_l"], P["fingers_r"], P["thumb_r"] = 10.0, 4.0, 6.0
    return P


SWIM_LONG = 1.4
SWIM_STRIDE = 0.95


def _swim_key(pitch, pz, arm, legs, neck=(-36.0, 0.0, 0.0), head=(-24.0, 0.0, 0.0)):
    P = body(pz, pitch=pitch, bend=(4.0, 0.0, 0.0), neck=neck, head=head)
    arms(P, arm, arm)
    for s in ("l", "r"):
        off, pole, flex = legs
        leg(P, s, off, pole=pole, flex=flex)
    P["fingers_l"] = P["fingers_r"] = 8.0
    P["thumb_l"] = P["thumb_r"] = 8.0
    return P


# the stroke: the glide (arms out ahead, legs together behind), the pull (hands out, down and in under the chin, the
# head and shoulders rising to breathe), the recovery (hands shot forward, heels drawn up), the kick (the feet out and
# round and together), the glide again
SWIM_KEYS = [
    (0.00, dict(pitch=74.0, pz=-0.17, arm=(168.0, 88.0, 0.0, 8.0, 0.0), legs=((-0.02, 0.03, -0.83), (1.0, 0.3, 0.0), 60.0))),
    (0.30, dict(pitch=68.0, pz=-0.14, arm=(128.0, 42.0, 10.0, 38.0, 20.0), legs=((-0.03, 0.04, -0.82), (1.0, 0.3, 0.0), 58.0))),
    (0.52, dict(pitch=60.0, pz=-0.11, arm=(70.0, 86.0, 20.0, 132.0, 40.0), legs=((-0.16, 0.08, -0.62), (1.0, 0.5, 0.0), 30.0),
                neck=(-30.0, 0.0, 0.0), head=(-22.0, 0.0, 0.0))),
    (0.74, dict(pitch=66.0, pz=-0.13, arm=(140.0, 88.0, 0.0, 50.0, 0.0), legs=((-0.27, 0.13, -0.42), (1.0, 0.55, 0.0), 0.0))),
    (0.92, dict(pitch=72.0, pz=-0.16, arm=(168.0, 88.0, 0.0, 8.0, 0.0), legs=((-0.08, 0.3, -0.66), (1.0, 0.6, 0.0), -15.0))),
    (1.10, dict(pitch=75.0, pz=-0.18, arm=(170.0, 88.0, 0.0, 6.0, 0.0), legs=((-0.02, 0.05, -0.83), (1.0, 0.3, 0.0), 62.0))),
]
_swim = None


@water_clip("swim_breast", SWIM_LONG, loop=True, stride=SWIM_STRIDE,
            events=[sev(0.22, "water_stroke", "swim_stroke"), sev(0.98, "water_kick", "swim_kick")],
            about="Breaststroke, head up: the body near level just under the surface (the vest's back awash), a stroke "
                  "of the arms (out, down, in under the chin, shot forward), the frog kick, the glide.")
def c_swim(t):
    global _swim
    if _swim is None:
        _swim = looped([(k, _swim_key(**d)) for k, d in SWIM_KEYS], SWIM_LONG)
    return _swim(t)


FLOAT_LONG = 4.0


@water_clip("float_dead", FLOAT_LONG, loop=True, about="Dead, floating face down, limp: the back and the vest awash, the "
            "head and arms hanging down in the water, the legs a little lower, rocked by the swell.")
def c_float_dead(t):
    a = 2 * math.pi * t / FLOAT_LONG
    P = body(-0.12 + 0.02 * math.sin(a), pitch=96.0 + 2.5 * math.sin(a + 0.8), roll=4.0 * math.sin(a + 1.6),
             yaw=2.0 * math.sin(a), bend=(8.0, 0.0, 0.0), neck=(24.0, 0.0, 6.0), head=(16.0, 0.0, 8.0))
    sw = math.sin(a + 0.4)
    arms(P, (80.0 + 6.0 * sw, 82.0, 8.0, 28.0 + 6.0 * sw, 10.0), (76.0 - 5.0 * sw, 88.0, 4.0, 34.0 - 4.0 * sw, 6.0))
    for s, k in (("l", 1.0), ("r", -1.0)):
        leg(P, s, (0.22 + 0.03 * k * sw, 0.06, -0.8), pole=(1.0, 0.15, 0.0), flex=42.0)
    P["fingers_l"] = P["fingers_r"] = 30.0
    P["thumb_l"] = P["thumb_r"] = 20.0
    return P


CLING_LONG = 3.0
RAIL = ((0.24, 0.0, 0.06), 0.08)     # (what he clings to: a spar along y, its middle and radius, in the root's frame)


@water_clip("cling", CLING_LONG, loop=True, about="Clinging to wreckage: both forearms over a floating spar ahead of him "
            "(`rail`: a round spar 0.16 m thick along his left-right, its middle 0.24 m ahead of the root and 0.06 m "
            "over the surface), hands hanging over its far side, the head down on his arms now and then, the body and "
            "legs hanging under it, a slow kick.")
def c_cling(t):
    a = 2 * math.pi * t / CLING_LONG
    c, r = np.array(RAIL[0]), RAIL[1]
    rest = sm(0.5 + 0.5 * math.sin(a))                     # (the head lowered onto his arms, and up again)
    P = body(-0.40 + 0.015 * math.sin(a), x=-0.04, pitch=14.0 + 3.0 * rest, bend=(8.0 + 4.0 * rest, 0.0, 0.0),
             neck=(6.0 + 11.0 * rest, 0.0, 5.0 * math.sin(a)), head=(4.0 + 6.0 * rest, 0.0, 4.0 * math.sin(a)))
    top = c + np.array((0.0, 0.0, r))
    for s, side in (("l", 1.0), ("r", -1.0)):
        at = top + np.array((0.095, side * 0.16, -0.03))
        P["hand_" + s] = world_hand(at, (0.35, 0.0, -1.0), (-1.0, 0.0, -0.2), (-0.2, side * 0.8, -0.6))
    for s, ph in (("l", 0.0), ("r", math.pi)):
        k = math.sin(a + ph)
        leg(P, s, (0.06 + 0.07 * k, 0.03, -0.8), pole=(1.0, 0.2, 0.0), flex=40.0 + 10.0 * k)
    P["fingers_l"] = P["fingers_r"] = 35.0
    return P


# ---------------------------------------------------------------------------------------------------- over the side
NET_LONG = 1.6
NET_CLIMB = 0.45
NET_X = 0.30             # (the net's ropes hang this far ahead of his root, upright; he faces it)


def _climb_limb(u, start, swing=0.35):
    """Where a planted hand or foot is through the loop, as the net goes down past him (in place: he climbs `NET_CLIMB`
    a loop): down the loop's climb while planted, then lifted back up to its next hold. Returns (drop, lifted 0-1)."""
    v = (u - start) % 1.0
    if v < swing:
        s = sm(v / swing)
        return NET_CLIMB * (1.0 - swing) * (1.0 - s), math.sin(math.pi * v / swing)      # (lifted to its next hold)
    w = (v - swing) / (1.0 - swing)
    return NET_CLIMB * (1.0 - swing) * w, 0.0


@water_clip("climb_net", NET_LONG, loop=True, root="net", climb=NET_CLIMB,
            events=[sev(0.0, "net_grab", "net_rope", hand="r"), sev(0.8, "net_grab", "net_rope", hand="l")],
            about="Climbing a scrambling net up the ship's side: in place, the net upright 0.30 m ahead of his root, "
                  "the root where his feet would stand; each loop he climbs `climb` metres (the game moves him up the "
                  "net at climb / long metres a second). Right hand and left foot, then left hand and right foot.")
def c_climb(t):
    u = t / NET_LONG
    lift = 0.5 + 0.5 * math.sin(2 * math.pi * (2 * u - 0.1))
    P = body(MAN["pz"] - 0.16 + 0.03 * lift, x=-0.05, pitch=6.0, bend=(4.0, 0.0, 2.0 * math.sin(2 * math.pi * u)),
             neck=(-14.0, 0.0, 0.0), head=(-10.0, 0.0, 0.0))
    top_h = MAN["shoulder"] + 0.42
    for s, side, start in (("r", -1.0, 0.0), ("l", 1.0, 0.5)):
        drop, up = _climb_limb(u, start)
        at = np.array((NET_X - 0.035 - 0.05 * up, side * 0.2, top_h - drop))
        P["hand_" + s] = world_hand(at, (0.15, 0.0, 1.0), (1.0, 0.0, -0.1), (-0.3, side * 1.0, -0.6))
        P["fingers_" + s] = 70.0 - 55.0 * up
        P["thumb_" + s] = 45.0 - 30.0 * up
    foot_top = MAN["ankle"] + 0.5
    for s, side, start in (("l", 1.0, 0.1), ("r", -1.0, 0.6)):
        drop, up = _climb_limb(u, start)
        at = np.array((NET_X - 0.16 - 0.06 * up, side * 0.13, foot_top - drop))
        leg(P, s, at - hip_at(P, s), pole=(1.0, 0.35, 0.1), flex=-5.0 + 15.0 * up, frame="root")
    return P


FALL_LONG = 0.8


def fall_pose(t, long=FALL_LONG):
    """Falling feet first: upright, leaning back a little, the legs straight and together (ankles crossed), the toes
    pointed down, the arms crossed over the vest, the chin down; the legs and the trunk stir a little."""
    a = 2 * math.pi * t / long
    P = body(MAN["pz"] + 0.02, pitch=-5.0 + 1.5 * math.sin(a), roll=1.0 * math.sin(a), bend=(2.0, 0.0, 0.0),
             neck=(12.0, 0.0, 0.0), head=(8.0, 0.0, 0.0))
    crossed_arms(P)
    for s, k in (("l", 1.0), ("r", -1.0)):
        leg(P, s, (0.02 + 0.02 * k * math.sin(a), -0.045, -MAN["leg"] + 0.02), pole=(1.0, 0.0, 0.0), flex=50.0)
    return P


@water_clip("fall_loop", FALL_LONG, loop=True, root="feet", about="Falling feet first through the air, in place (the "
            "game moves him on his fall): the root under his feet as if he stood; arms crossed over the vest, legs "
            "together, toes pointed, chin down. For a drop the jump clips do not cover.")
def c_fall(t):
    return fall_pose(t)


def stand_edge(t):
    """Stood at the deck's edge (the root's frame: his feet on the deck at z = 0), looking down at the water."""
    P = body(MAN["pz"] - 0.012, pitch=4.0, bend=(4.0, 0.0, 0.0), neck=(14.0, 0.0, 0.0), head=(12.0, 0.0, 0.0))
    for s, side, x in (("l", 1.0, 0.04), ("r", -1.0, -0.04)):
        P["foot_" + s] = {"at": (x, side * 0.12, MAN["ankle"]), "yaw": side * 6.0}
    P["shin_feet"] = 0.0
    arms(P, (8.0, 10.0, 15.0, 15.0, 0.0), (8.0, 10.0, 15.0, 15.0, 0.0))
    return P


def under_pose(t, x, z):
    """Gone under, the arms still crossed, legs together: (x, z) the pelvis's head."""
    P = put(fall_pose(0.0), x, 0.0, z)
    P["neck"], P["head"] = (6.0, 0.0, 0.0), (4.0, 0.0, 0.0)
    return P


def jump_spec(h):
    """The times of a jump from a deck h metres over the water: the step (0 to T0), the fall to the splash (TE), down
    and back up to tread (T1); the drift ahead, and how deep he goes."""
    T0 = 0.62
    tau = math.sqrt(2.0 * max(h - 0.05, 0.1) / G)
    TE = T0 + tau
    v = G * tau
    deep = {2.0: 2.0, 4.0: 2.6, 8.0: 3.4}.get(h, 2.0 + 0.2 * h)        # (the feet's deepest under the surface)
    T1 = TE + 0.55 + 0.12 * deep + 0.6
    T1 = math.ceil(T1 * 60.0) / 60.0
    return dict(h=h, T0=T0, TE=TE, T1=T1, v=v, deep=deep, drift=0.9)


def jump_at(t, J):
    h, T0, TE, T1 = J["h"], J["T0"], J["TE"], J["T1"]
    standing_z = MAN["pz"] - 0.012
    if t < T0:
        # stood at the edge (his feet h over the water), the arms coming to cross, a dip, the right foot out over the
        # edge and the left pushing off
        P = stand_edge(t)
        w = ramp(t, 0.0, 0.3)
        Q = crossed_arms(dict(P))
        P = S.mix([P, Q], [1.0 - w, w])
        dip = 0.05 * (ramp(t, 0.2, 0.38) - ramp(t, 0.38, 0.55))
        fwd = 0.3 * ramp(t, 0.3, T0)
        P["pelvis_at"] = (fwd, 0.0, standing_z - dip - MAN["pz"] + h - 0.05 * ramp(t, 0.5, T0))
        P["free_knees"] = 1.0
        fr = ramp(t, 0.28, 0.5)
        P["foot_r"] = {"at": (-0.04 + 0.4 * fr, -0.12, h + MAN["ankle"] + 0.07 * math.sin(math.pi * fr) - 0.08 * ramp(t, 0.5, T0)),
                       "yaw": -6.0, "pitch": 20.0 * fr}
        P["foot_l"] = {"at": (0.04 + 0.12 * ramp(t, 0.45, T0), 0.12, h + MAN["ankle"]), "yaw": 6.0, "pitch": 35.0 * ramp(t, 0.45, T0)}
        return P
    if t < TE:
        s = t - T0
        z = standing_z - 0.05 + h - 0.5 * G * s * s
        P = put(fall_pose(s), 0.3 + J["drift"] * s, 0.0, z)
        w = ramp(s, 0.0, 0.18)                                   # (from the step into the fall)
        if w < 1.0:
            Q = jump_at(T0 - 1e-6, J)
            P = S.mix([Q, P], [1.0 - w, w])
        return P
    # under: driven down by the speed of the fall (spent quickly in the water), then the vest takes him up to tread
    s = t - TE
    ze = standing_z - 0.05 + h - 0.5 * G * (TE - T0) ** 2
    d = J["deep"] - (MAN["pz"] - MAN["ankle"]) * 0.0
    k = J["v"] / d
    z_down = ze - d * (1.0 - math.exp(-k * s))
    x = 0.3 + J["drift"] * (TE - T0) + 0.25 * (1.0 - math.exp(-3.0 * s))
    up = ramp(t, TE + 0.45, T1)
    z = z_down + (TREAD_Z - z_down) * up
    P = under_pose(t, x, z)
    tr = tread_pose(0.0)
    tr = shift(tr, (x, 0.0, z - TREAD_Z))         # (tread's first pose, where he is: it is his pose at the end, at the end)
    w = ramp(t, TE + 0.35, T1)
    return S.mix([P, tr], [1.0 - w, w])


def jump_end(J):
    return 0.3 + J["drift"] * (J["TE"] - J["T0"]) + 0.25 * (1.0 - math.exp(-3.0 * (J["T1"] - J["TE"])))


def _jump(name, h):
    J = jump_spec(h)
    head_up = J["T1"] - 0.45
    water_clip(name, J["T1"], root="surface", end=(jump_end(J), 0.0, 0.0),
               events=[sev(J["TE"], "splash", "splash_body", bone="foot_l", height=h),
                       sev(head_up, "surface", "surface_gasp", bone="head")],
               about=f"Over the side from a deck {h:g} m over the water (the root at the sea's surface under the deck's "
                     f"edge; at the start his feet are on the deck, {h:g} m up): arms cross over the vest, a step off "
                     f"the edge, the fall ({J['TE'] - J['T0']:.2f} s), the splash as his feet reach the water "
                     f"({J['TE']:.2f} s, event `splash`), down to his feet about {J['deep']:g} m under, and up again "
                     f"into tread's first pose; he ends `end` ahead of the root (then play tread there).")(lambda t: jump_at(t, J))
    return J


JUMPS = {"jump_feet_first": _jump("jump_feet_first", 4.0), "jump_feet_first_2m": _jump("jump_feet_first_2m", 2.0),
         "jump_feet_first_8m": _jump("jump_feet_first_8m", 8.0)}


def _enter():
    J = dict(JUMPS["jump_feet_first"])
    start = J["TE"]
    long = math.ceil((J["T1"] - start) * 60.0) / 60.0
    x0 = 0.3 + J["drift"] * (J["TE"] - J["T0"])

    def fn(t):
        return shift(jump_at(start + t, J), (-x0, 0.0, 0.0))
    water_clip("enter_water", long, root="surface", end=(jump_end(J) - x0, 0.0, 0.0),
               events=[sev(0.0, "splash", "splash_body", bone="foot_l"), sev(long - 0.45, "surface", "surface_gasp", bone="head")],
               about="From the splash on (after fall_loop): his feet at the surface coming down at the speed of a 4 m "
                     "drop, under, and up into tread's first pose; the root at the surface where his feet went in.")(fn)


_enter()

# ---------------------------------------------------------------------------------------------------- in the boats
ROW_LONG = 2.4
OARLOCK = (0.02, -0.6, 0.2)      # (his oar's rowlock: beside him on his right, over the seat; the root on the thwart)


def sit(P_lean=0.0, slouch=0.0, turn=0.0, hip_x=-0.07, hip_z=0.115):
    P = body(hip_z, x=hip_x, pitch=-12.0 + 0.8 * P_lean,
             bend=(15.0 + 13.0 * slouch + 0.3 * P_lean, 0.0, turn), neck=(-3.0 - 4.0 * slouch, 0.0, 0.1 * turn),
             head=(-2.0 - 3.0 * slouch, 0.0, 0.1 * turn))
    P["pelvis_at"] = (hip_x, 0.0, hip_z - MAN["pz"])
    return P


def oar_hands(P, hx, hz):
    """Both hands on the oar's loom (overhand, a hand's breadth apart), its handle at (hx, 0, hz)."""
    for s, y in (("l", 0.075), ("r", -0.04)):
        at = np.array((hx, y, hz))
        P["hand_" + s] = world_hand(at, (1.0, 0.0, 0.15), (0.0, 0.0, -1.0), (-0.3, (1.0 if s == "l" else -1.0), -0.7))
        P["fingers_" + s] = 70.0
        P["thumb_" + s] = 50.0
    return P


ROW_KEYS = [(0.0, 26.0, 0.6, 0.33), (0.45, 0.0, 0.42, 0.335), (0.95, -22.0, 0.17, 0.32), (1.25, -18.0, 0.2, 0.27),
            (1.85, 6.0, 0.45, 0.28)]      # (t, lean forward, the handle's x and z)
_row = None


def row_handle(t):
    ts = [k[0] for k in ROW_KEYS]
    n = len(ROW_KEYS)
    t = t % ROW_LONG
    i = max(j for j in range(n) if ts[j] <= t)
    t1 = ts[i + 1] if i + 1 < n else ROW_LONG
    u = sm((t - ts[i]) / (t1 - ts[i]))
    a, b = ROW_KEYS[i], ROW_KEYS[(i + 1) % n]
    return tuple(a[k] + (b[k] - a[k]) * u for k in (1, 2, 3))


@water_clip("row", ROW_LONG, loop=True, root="seat", events=[sev(0.0, "oar_catch", "oar_splash")],
            about="Pulling an oar in a boat, sat on the thwart (the root on its top, facing the stern as rowers do; "
                  "his feet on the boards 0.35 m lower): the catch leaning forward with his arms out, the drive leaning "
                  "back and the hands to the chest, the recovery. Both hands on one oar's loom; the `oar` track gives "
                  "its handle and its rowlock (on his right) so the boat's oar can be drawn through them.")
def c_row(t):
    lean, hx, hz = row_handle(t)
    P = sit(P_lean=lean, slouch=0.2)
    oar_hands(P, hx, hz)
    for s, side in (("l", 1.0), ("r", -1.0)):
        P["foot_" + s] = {"at": (0.5, side * 0.13, -0.35 + MAN["ankle"]), "yaw": side * 8.0, "pole": (1.0, side * 0.25, 0.55)}
    P["shin_feet"] = 0.0
    return P


RAFT_LONG = 6.0


@water_clip("sit_raft", RAFT_LONG, loop=True, root="seat",
            about="Sat on a raft's deck or a boat's bottom boards (the root on it), knees up, arms round his shins, "
                  "hunched; he lifts his head and looks about, and lowers it again.")
def c_sit_raft(t):
    a = 2 * math.pi * t / RAFT_LONG
    look = sm(0.5 + 0.5 * math.sin(a))
    P = body(0.135, x=-0.11, pitch=-18.0 + 4.0 * look, bend=(30.0 - 8.0 * look, 0.0, 6.0 * math.sin(a)),
             neck=(18.0 - 22.0 * look, 0.0, 22.0 * math.sin(a + 0.5) * look),
             head=(10.0 - 12.0 * look, 0.0, 18.0 * math.sin(a + 0.5) * look))
    for s, side in (("l", 1.0), ("r", -1.0)):
        P["foot_" + s] = {"at": (0.36, side * 0.13, MAN["ankle"]), "yaw": side * 10.0, "pole": (1.0, side * 0.2, 0.8)}
    P["shin_feet"] = 0.0
    for s, side, x, z in (("l", 1.0, 0.33, 0.36), ("r", -1.0, 0.31, 0.34)):
        P["hand_" + s] = world_hand((x, side * 0.02, z), (0.0, -side, -0.25), (-1.0, 0.0, 0.0), (0.0, side * 1.0, -0.4))
        P["fingers_" + s] = 55.0
    return S.breathe(P, t, RAFT_LONG / 2, 1.2)


# ================================================================================================ the files
NAME = {"usa": "American sailor (soldier_USA_sailor)", "jap": "Japanese sailor (soldier_JAP_sailor)",
        "uk": "British sailor (soldier_UK_sailor)", "ger": "German sailor (soldier_GER_sailor)"}[WHO]
ABOUT = {
    "usa": (" The American sailor: soldier.py's man (the same 32 bones) in the Navy's working dress, a light blue "
            "chambray shirt, dark blue dungarees, brown work shoes, the white hat, and a grey kapok life jacket tied "
            "over the shirt. He carries no weapon (every clip's weapon is none). His colours are the file's own paints "
            "(new_paints, colours: kapok, shirt_own, trousers_own, hat, ties), not an army's; colour_variants gives "
            "an officer's khaki and the gun crew's steel helmet. Clips on deck from the motion capture; over the "
            "side and in the water keyed on his rig, the root at the sea's surface (water block)."),
    "jap": (" The Japanese sailor: soldier.py's man (the same 32 bones) in the Navy's white cotton working dress, "
            "jumper and trousers, black leather shoes, the soft white working cap with its short peak, and a tan "
            "canvas kapok life jacket tied over the jumper. He carries no weapon (every clip's weapon is none). His "
            "colours are the file's own paints (new_paints, colours: kapok, shirt_own, trousers_own, hat, ties), not "
            "an army's; colour_variants gives the later war's grey-green working dress and the gun crew's steel "
            "helmet. Clips on deck from the motion capture; over the side and in the water keyed on his rig, the "
            "root at the sea's surface (water block)."),
    "uk": (" The British sailor: soldier.py's man (the same 32 bones) as a rating of the Royal Navy at his action "
           "station: a navy blue jersey and blue serge trousers, black leather shoes, the round flat cap (a crown over "
           "a band, no peak), and the inflatable rubber lifebelt in its grey-khaki cover, blown up round his chest "
           "under the arms, its mouth tube up on the left. He carries no weapon (every clip's weapon is none). His "
           "colours are the file's own paints (new_paints, colours: kapok = the lifebelt's cover, shirt_own = the "
           "jersey, trousers_own, hat, ties = the mouth tube), not an army's; colour_variants gives the white cap "
           "cover of the summer and the Mediterranean, the cream sea jersey of the Atlantic escorts, and the gun "
           "crew's steel helmet. Clips on deck from the motion capture; over the side and in the water keyed on his "
           "rig, the root at the sea's surface (water block)."),
    "ger": (" The German sailor: soldier.py's man (the same 32 bones) as a rating of the Navy at his action station, "
            "with no badge or insignia anywhere: a dark blue pullover with its stand collar and dark blue trousers, "
            "black leather shoes, the round flat cap with its two black ribbons hanging behind (no tally on its band), "
            "and a grey-green canvas kapok life jacket tied over the pullover. He carries no weapon (every clip's "
            "weapon is none). His colours are the file's own paints (new_paints, colours: kapok = the life jacket, "
            "shirt_own = the pullover, trousers_own, hat = the cap and its ribbons, ties = the jacket's ties), not an "
            "army's; colour_variants gives the white drill working dress of summer and the Mediterranean, the "
            "grey-green working dress, and the gun crew's steel helmet. Clips on deck from the motion capture; over "
            "the side and in the water keyed on his rig, the root at the sea's surface (water block)."),
}[WHO]
DECK_ABOUT = {
    "idle": "On deck, at ease, arms free (motion capture).",
    "walk": "On deck, walking (motion capture), arms free.",
    "run": "On deck, running (motion capture), arms free: to his station, or for the side.",
    "cower": "On deck, hunched, hands up by his head, as aircraft come in or the ship is hit (motion capture).",
    "die_back": "Killed on deck, falling back (motion capture); he stays where he falls.",
    "die_forward": "Killed on deck, falling forward (motion capture); he stays where he falls.",
}
ROOTS = {
    "deck": "the root on the deck under his feet, as the soldiers' clips",
    "surface": "the root at the sea's surface (z = 0 is the water's top): place it on the water where he is; the "
               "pelvis_z below says how deep his pelvis rides",
    "feet": "the root under his feet as if he stood (the fall: the game moves the root down his fall)",
    "net": "the root where his feet would stand at the net's foot, the net hanging upright 0.30 m ahead of him; move "
           "the root up the net at climb / long metres a second",
    "seat": "the root on the top of the thwart or the raft's deck he sits on, facing the clip's forward",
}


def water_json(data, rig):
    """The water block and each clip's notes: where its root goes, how deep the pelvis and the head ride under the
    sea's surface, what the events are for, the spar of cling and the oar of row."""
    for name, d in data["clips"].items():
        d["weapon"] = "none"
        w = WATER.get(name)
        if w is None:
            d["root"] = "deck"
            d["about"] = DECK_ABOUT.get(name, "")
            continue
        d["root"] = w["root"]
        d["about"] = w["about"]
        if w["climb"]:
            d["climb"] = w["climb"]
        if w["end"] is not None:
            e = w["end"]
            d["end_offset"] = [round(e[0], 3), round(-e[1], 3), round(e[2], 3)]
        if name in JUMPS:
            J = JUMPS[name]
            d["deck_height"] = J["h"]
            d["splash_t"] = round(J["TE"], 3)
        # how deep he rides (the root's frame: at the surface roots, under the water is below 0)
        c = S.CLIPS[name]
        zs = {"pelvis": [], "head": [], "foot_l": []}
        n = int(round(c["long"] * 15))
        for k in range(n + 1):
            st = clip_state(rig, name, k / 15.0, c["weapon"])
            for b in zs:
                zs[b].append(float(st["H"][b][2]))
        d["heights"] = {b: {"low": round(min(v), 3), "mean": round(sum(v) / len(v), 3), "high": round(max(v), 3)}
                        for b, v in zs.items()}
        if name in JUMPS:                       # (how deep his feet went, as it came out)
            J = JUMPS[name]
            d["about"] = d["about"].replace(f"about {J['deep']:g} m under", f"about {-d['heights']['foot_l']['low']:.1f} m under")
        if w["root"] == "surface" and name not in JUMPS and name != "enter_water":
            d["pelvis_z"] = d["heights"]["pelvis"]["mean"]
    cl = data["clips"].get("cling")
    if cl is not None:
        c, r = RAIL
        cl["rail"] = {"middle": [round(c[0], 3), round(-c[1], 3), round(c[2], 3)], "radius": r, "along": "y",
                      "about": "the floating spar or wreck rail his arms lie over: a round bar along his left-right "
                               "(the game's y), its middle here in the root's frame; draw any wreckage there"}
    rw = data["clips"].get("row")
    if rw is not None:
        tr = []
        for k in range(int(round(ROW_LONG * 15)) + 1):
            t = k / 15.0
            lean, hx, hz = row_handle(t)
            tr.append([round(t, 4), round(hx, 4), 0.0, round(hz, 4)])
        rw["oar"] = {"rowlock": [OARLOCK[0], -OARLOCK[1], OARLOCK[2]], "track_fields": ["t", "x", "y", "z"], "track": tr,
                     "about": "the oar's handle (between his hands) through the stroke, and its rowlock (fixed, on "
                              "his right), in the root's frame, the game's axes: draw the oar from the handle through "
                              "the rowlock and on, its blade in the water at the catch (event oar_catch)"}
    data["water"] = {
        "sea_level": "z = 0 in each water clip's root frame is the sea's surface (root `surface`)",
        "roots": ROOTS,
        "order": "a man going over the side: run (or walk) to the rail, jump_feet_first (the 2 m, 4 m or 8 m one "
                 "nearest the deck's height over the water; for any other height, fall_loop while the game drops "
                 "him, then enter_water at the surface), then tread (or swim_breast, wave, cling); a body: "
                 "float_dead. Picked up: climb_net up the side, or into a boat (row, sit_raft)",
        "events": "splash (splash_body: his feet into the water; height is the drop), surface (surface_gasp: his "
                  "head out again), water_stroke and water_kick (swim_breast), net_grab (climb_net, the hand), "
                  "oar_catch (row: the blade in); the deck clips' footsteps as the soldiers'",
        "no_weapon": "he has no weapon: the file has no weapon triangles; the weapon bone rides 2 m under his root "
                     "in the water clips",
    }
    return data


def paints_json(data):
    """His own paints in place of the army's (the battle colours them from this file): the shirt, the dungarees, the
    vest's ties and the hat; and his colours, with the officer's and the gun crew's variants."""
    for lod in ("near", "mid", "far"):
        pa = data[lod]["paints"]
        data[lod]["paints"] = [OWN_OF.get(p, p) for p in pa]
    data["new_paints"] = dict(NEW_PAINTS)
    cols = dict(S.LOOK[NATION])
    cols.update(OWN_COLOURS)
    data["colours"] = cols
    data["colour_variants"] = VARIANTS
    data["helmet_paints"] = [NEW_PAINTS["hat"]]
    return data


def about(data):
    data["name"] = NAME
    data["about"] = data["about"].replace("Made by Tools/battle_units/soldier.py.",
                                          "Made by Tools/battle_units/soldier_sailors.py on soldier.py"
                                          + (" and soldier_usa.py." if WHO == "usa" else "."))
    data["about"] += ABOUT
    data["weapon"] = "none"
    return data


def paint_count(data):
    out = {}
    for lod in ("near", "mid", "far"):
        T = np.array(data[lod]["triangles"]).reshape(-1, 3)
        pa = np.array(data[lod]["paints"])
        first = pa[T[:, 0]]
        u, c = np.unique(first, return_counts=True)
        out[lod] = {int(a): int(b) for a, b in zip(u, c)}
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
    data = paints_json(data)
    data = water_json(data, m["rig"])
    with open(S.stage_path(f"{BASE}.json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    rec["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
    rec["paints"] = paint_count(data)
    rec["high"] = data["high"]
    rec["clips"] = {k: [v["long"], v["loop"], v["weapon"], v.get("pelvis_z")] for k, v in data["clips"].items()}
    rec["heights"] = {k: v.get("heights") for k, v in data["clips"].items() if v.get("heights")}
    for lod in (1, 2):
        for o in lods[lod]:
            bpy.data.objects.remove(o)
    print(f"  {BASE} written in {time.time() - t0:.0f} s: {rec['tris']}; json {rec['json']}; paints {rec['paints']['near']}")
    return [f"{BASE}.glb", f"{BASE}_LOD1.glb", f"{BASE}_LOD2.glb", f"{BASE}.json"]


def assemble():
    if WHO == "usa":
        U.with_reload(False)
    m = S.assemble(NATION, "rifle", seed=SEED)
    use_man(m)
    return m


def export(info):
    t0 = time.time()
    kit.fresh(S.FPS)
    m = assemble()
    S.bake_clips(m)
    info["files"] = export_man(info, m)
    info["export_s"] = round(time.time() - t0)
    return m


# ================================================================================================ pictures
def hide_weapon(m):
    for o in (m.get("weapon"), m.get("weapon_part")):
        if o is not None:
            o.hide_render = True
    for o in m["kit"]:
        if o.name.startswith("rifle_"):
            o.hide_render = True


def water_plane(size=40.0):
    """The sea's surface at z = 0 for the pictures: a grey-green, half see-through, so what is under shows dimly."""
    bpy.ops.mesh.primitive_plane_add(size=size, location=(0, 0, 0))
    w = bpy.context.object
    w.name = "sea"
    mt = bpy.data.materials.new("sea")
    mt.use_nodes = True
    b = mt.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*kit.hex_rgb("2F4F57"), 1.0)
    b.inputs["Roughness"].default_value = 0.12
    b.inputs["Alpha"].default_value = 0.55
    w.data.materials.append(mt)
    return w


def deck_block(h, name="deck"):
    """A ship's side for the jump pictures: its deck h over the water, its edge at x = 0.12 ahead of the root."""
    bpy.ops.mesh.primitive_cube_add(size=1.0)
    d = bpy.context.object
    d.name = name
    d.scale = (3.0, 4.0, h + 1.0)
    d.location = (0.12 - 1.5, 0.0, (h - 1.0) / 2.0)
    d.data.materials.append(kit.material("hull", kit.hex_rgb("6F757A"), rough=0.6))
    return d


def raft_block(name="thwart"):
    bpy.ops.mesh.primitive_cube_add(size=1.0)
    d = bpy.context.object
    d.name = name
    d.scale = (0.25, 1.4, 0.05)
    d.location = (-0.06, 0.0, -0.025)
    d.data.materials.append(kit.material("thwart", kit.hex_rgb("8A6A48"), rough=0.7))
    return d


def spar(name="spar"):
    c, r = RAIL
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=2.4, location=c, rotation=(math.radians(90), 0, 0))
    d = bpy.context.object
    d.name = name
    d.data.materials.append(kit.material("spar", kit.hex_rgb("5C4A38"), rough=0.8))
    return d


SHOTS = [("front", "idle", 0.5, (0.0, 0.0, 0.95), 4.3, 6.0, 28.0),
         ("back", "idle", 0.5, (0.0, 0.0, 0.95), 4.3, 8.0, 205.0),
         ("head", "idle", 0.5, (0.02, 0.0, 1.45), 1.25, 6.0, 35.0)]
LOOK_SHOTS = SHOTS + [("side", "idle", 0.5, (0.0, 0.0, 1.0), 3.4, 6.0, 270.0),
                      ("headback", "idle", 0.5, (0.0, 0.0, 1.45), 1.2, 10.0, 200.0),
                      ("headside", "idle", 0.5, (0.0, 0.0, 1.45), 1.2, 4.0, 90.0)]
# (key, clip, t, aim, dist, pitch, yaw, set: none / sea / deck h / net / seat / spar)
WATER_SHOTS = [
    ("jump_step", "jump_feet_first", 0.45, (0.2, 0.0, 4.9), 4.6, 4.0, 300.0, ("deck", 4.0)),
    ("jump_fall", "jump_feet_first", 1.15, (0.6, 0.0, 2.2), 6.5, 2.0, 290.0, ("deck", 4.0)),
    ("jump_under", "jump_feet_first", None, (0.9, 0.0, -0.9), 4.6, -4.0, 300.0, ("deck", 4.0)),
    ("tread", "tread", 0.6, (0.0, 0.0, 0.0), 3.4, 14.0, 30.0, ("sea",)),
    ("wave", "wave", 0.2, (0.0, 0.0, 0.3), 3.6, 10.0, 25.0, ("sea",)),
    ("swim", "swim_breast", 0.5, (0.3, 0.0, -0.1), 3.6, 16.0, 300.0, ("sea",)),
    ("float_dead", "float_dead", 1.0, (0.3, 0.0, -0.1), 3.6, 28.0, 300.0, ("sea",)),
    ("cling", "cling", 0.8, (0.1, 0.0, -0.05), 3.0, 14.0, 330.0, ("spar",)),
    ("climb_net", "climb_net", 0.3, (0.0, 0.0, 1.1), 3.6, 6.0, 210.0, ("net",)),
    ("row", "row", 0.5, (0.1, 0.0, 0.4), 3.0, 12.0, 60.0, ("seat",)),
    ("sit_raft", "sit_raft", 1.0, (0.1, 0.0, 0.35), 2.8, 14.0, 40.0, ("seat",)),
    ("cower", "cower", 0.8, (0.0, 0.0, 0.8), 3.6, 8.0, 30.0, ()),
]
WATER_LABELS = {"jump_step": "jump_feet_first (4 m): the step off, arms crossed", "jump_fall": "the fall (0.9 s), legs together",
                "jump_under": "under, feet about 2 m down, then up", "tread": "tread: head and shoulders out",
                "wave": "wave: an arm up for rescue", "swim": "swim_breast: head up", "float_dead": "float_dead: face down, limp",
                "cling": "cling: forearms over a spar", "climb_net": "climb_net: up the ship's side",
                "row": "row: pulling an oar on a thwart", "sit_raft": "sit_raft: knees up, looking about",
                "cower": "cower (on deck): aircraft coming in"}
LABELS = {
    "usa": ["American sailor: chambray shirt, dungarees, kapok vest, white hat",
            "From behind: the vest's back, its channels",
            "Close: the white hat, the vest's collar"],
    "jap": ["Japanese sailor: white working dress, kapok vest, working cap",
            "From behind: the vest's back, its channels",
            "Close: the working cap, the vest's collar"],
    "uk": ["British sailor: navy jersey, blue trousers, lifebelt blown up, round cap",
           "From behind: the lifebelt round his chest",
           "Close: the round cap, the mouth tube"],
    "ger": ["German sailor: navy pullover, kapok life jacket, round cap with its ribbons",
            "From behind: the jacket's back, the cap's ribbons",
            "Close: the round cap, the jacket's collar"],
}[WHO]


def _sets(kind):
    made = []
    if not kind:
        return made
    if kind[0] in ("sea", "spar", "deck"):
        made.append(water_plane())
    if kind[0] == "deck":
        made.append(deck_block(kind[1]))
    if kind[0] == "spar":
        made.append(spar())
    if kind[0] == "net":
        bpy.ops.mesh.primitive_plane_add(size=1.0, location=(NET_X + 0.01, 0.0, 1.2), rotation=(0.0, math.radians(90), 0.0))
        n = bpy.context.object
        n.scale = (2.6, 1.6, 1.0)
        n.data.materials.append(kit.material("hull", kit.hex_rgb("6F757A"), rough=0.6))
        made.append(n)
    if kind[0] == "seat":
        made.append(raft_block())
    return made


def picture(info, fast=False, shots=None, out_name=None, labels=None, title=None, water=False):
    size = (520, 640, 12) if fast else (900, 1080, 36)
    kit.fresh(S.FPS)
    m = assemble()
    S.studio_smooth(m)
    hide_weapon(m)
    rig = m["rig"]
    kit.studio()
    bd = kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(*size)
    ims = []
    made = []
    for shot in (shots or SHOTS):
        key, name, t, aim, dist, pitch, yaw = shot[:7]
        kind = shot[7] if len(shot) > 7 else ()
        for o in made:
            bpy.data.objects.remove(o)
        made = _sets(kind)
        bd.location.z = -6.0 if kind and kind[0] in ("sea", "spar", "deck") else (-0.4 if kind and kind[0] == "seat" else 0.0)
        if t is None:
            J = JUMPS[name]
            t = J["TE"] + 0.35
        st = S.clip_state(rig, name, t, "rifle")
        S.pose_once(m["arm"], rig, st, 0)
        kit.orbit(cam, aim, dist, pitch, yaw)
        ims.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"{WHO}_{key}.png")))
    out = out_name or S.stage_path("pictures", f"{BASE}.png")
    kit.sheet(ims, out, cols=3, labels=labels or [os.path.basename(p) for p in ims], title=title or f"{NAME}: soldier_sailors.py")
    info["picture" if not water else "water_picture"] = out
    return out


def main():
    a = sys.argv
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export", "picture", "water"]
    os.makedirs(S.STAGE, exist_ok=True)
    p = S.stage_path(f"export_info_{BASE}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "look":
            picture(info, fast=True, shots=LOOK_SHOTS, out_name=os.path.join(S.SCRATCH, "look", "sheet.png"))
        elif step == "export":
            export(info)
        elif step == "picture":
            picture(info, fast, labels=LABELS)
        elif step == "water":
            want = os.environ.get("WATER_SHOTS")
            shots = [s for s in WATER_SHOTS if not want or s[0] in want.split(",")]
            picture(info, fast, shots=shots, water=True, labels=[WATER_LABELS[x[0]] for x in shots],
                    title=f"{NAME}: over the side and in the water (the sea's surface drawn half see-through)",
                    out_name=(os.path.join(S.SCRATCH, "look", "water.png") if fast else S.stage_path("pictures", f"{BASE}_water.png")))
        with open(p, "w") as f:
            json.dump(info, f, indent=1, default=str)
        print(f"step {step}: {time.time() - t0:.0f} s", flush=True)


if __name__ == "__main__":
    main()
