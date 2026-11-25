"""The sailors of the sea battles, for the men who go over the side when their ship goes down (Brian, 22 November
2026: "sailors should jump in the water if the boat is going down", "have them use the models we have for the soldiers
for the sailors"): the American sailor (soldier_USA_sailor), the Japanese sailor (soldier_JAP_sailor), the British
sailor (soldier_UK_sailor, `--man uk`), the German sailor (soldier_GER_sailor, `--man ger`), the Italian sailor
(soldier_ITA_sailor, `--man ita`) and the Soviet sailor (soldier_SOV_sailor, `--man sov`). Made beside soldier.py and
soldier_usa.py (both stay as they are).

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
  root on the thwart's or the raft's top);
- on deck at a gun and giving up (release 2, the sea scene), keyed on his rig: hands_up, gun_layer, gun_trainer,
  gun_loader (a round about every 5 s), the passers ammo_passer, ammo_passer_50 and ammo_passer_70 to _110 every
  0.05 m (root `hatch`) with ammo_passer_wait, and the carrier ammo_carrier_take, _walk, _wait and _give, with their
  wheel, sight, round and pair blocks. `--only gun` draws them (stills); `--only graft` adds just these (GUN_CLIPS)
  to the delivered file (backed up first) and rewrites the gun crew's lines of its about and water block, leaving
  every other clip and key in it as it is, as run_styles.py added the runs.
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
assert WHO in ("usa", "jap", "uk", "ger", "ita", "sov"), WHO
NATION = {"usa": "USA", "jap": "JAP", "uk": "UK", "ger": "GER", "ita": "ITA", "sov": "SOV"}[WHO]
BASE = {"usa": "soldier_USA_sailor", "jap": "soldier_JAP_sailor", "uk": "soldier_UK_sailor", "ger": "soldier_GER_sailor",
        "ita": "soldier_ITA_sailor", "sov": "soldier_SOV_sailor"}[WHO]

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


SEED = {"usa": 233, "jap": 57, "uk": 149, "ger": 191, "ita": 167, "sov": 211}[WHO]      # (his face: a mix no other man's file uses)
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
        "sweater": {"shirt_own": "AEA285",     # (oatmeal, the art director's 22 November: cream read as summer white)
                    "about": "the oatmeal (undyed wool) roll-neck sea jersey of the North Atlantic escorts, Coastal "
                             "Forces and the submarines"},
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

elif WHO == "ita":
    # the Italian sailor (a rating of the Navy at his action station, no badge or insignia): the faded blue-grey cotton
    # working dress (jumper and trousers), black leather shoes, the dark blue round cap, and a life jacket of cork
    # blocks sewn into cream canvas (the kapok jacket's make, its cork in blocks: grooves across as well as down),
    # tied over the jumper
    S.NATION_SEED[NATION] = SEED
    S.LOOK[NATION] = dict(S.LOOK["GER"])
    S.NATION_KIT[NATION] = dict(S.NATION_KIT["UK"])
    SPEC = S.NATION_KIT[NATION]
    SPEC.update({"helmet": "roundcap", "pack": False, "y_straps": False, "pouches": {"front": 0, "rear": False},
                 "cross_straps": False, "chest_pouches": False})
    COLOURS = {
        "tunic": "6B7C92",     # the working jumper, faded blue-grey cotton
        "trousers": "4A566A",  # its trousers, darker
        "lower": "4A566A",     # (the legs below the knee, the same cloth)
        "boots": "1E1A18",     # black leather shoes
        "helmet": "1C2232",    # the round cap, dark blue
        "straps": "8A8068",    # the jacket's ties, a tape a shade darker than its canvas
        "kapok": "B9AE92",     # the cork jacket's cream canvas
        "leather": "1E1A18",
    }
    S.LOOK[NATION].update(COLOURS)
    OWN_COLOURS = {"kapok": COLOURS["kapok"], "shirt_own": COLOURS["tunic"], "trousers_own": COLOURS["trousers"],
                   "hat": COLOURS["helmet"], "ties": COLOURS["straps"]}
    VARIANTS = {
        "white_summer": {"shirt_own": "E0DDD3", "trousers_own": "D8D5CA",
                         "about": "the white summer working dress of the Mediterranean: jumper and trousers (the cap "
                                  "stays dark)"},
        "dark_blue": {"shirt_own": "222A3C", "trousers_own": "1E2536",
                      "about": "the dark blue cloth dress of the winter"},
        "gun_crew": {"about": "a gun crew's man: the same colours, with a steel helmet (a helmet finish, in grey-green "
                              "5C6052) drawn in place of the cap (helmet_paints)"},
    }

elif WHO == "sov":
    # the Soviet sailor (a rating of the Navy at his action station, no badge or insignia): the dark blue flannel
    # jumper and black trousers, black leather shoes (not the soldier's tall boots), the peakless round cap with its two
    # black ribbons hanging behind, and a kapok bib: a pad over his chest and one over his back, joined over the
    # shoulders and round the neck, open at his sides
    S.NATION_SEED[NATION] = SEED
    S.LOOK[NATION] = dict(S.LOOK.get(NATION, S.LOOK["GER"]))
    SPEC = S.NATION_KIT[NATION]
    SPEC.update({"helmet": "bescap", "tall_boots": False, "pack": False, "roll": False, "canister": False,
                 "y_straps": False, "pouches": {"front": 0, "rear": False}, "cross_straps": False, "chest_pouches": False})
    SPEC.pop("puttees", None)
    COLOURS = {
        "tunic": "202638",     # the dark blue flannel jumper
        "trousers": "17181D",  # the black trousers
        "lower": "17181D",     # (the legs below the knee, the same cloth)
        "boots": "1B1917",     # black leather shoes
        "helmet": "15161B",    # the peakless cap and its ribbons, black
        "straps": "6E6650",    # the bib's ties, a tape a shade darker than its canvas
        "kapok": "8F8466",     # the kapok bib's khaki canvas
        "leather": "1B1917",
    }
    S.LOOK[NATION].update(COLOURS)
    OWN_COLOURS = {"kapok": COLOURS["kapok"], "shirt_own": COLOURS["tunic"], "trousers_own": COLOURS["trousers"],
                   "hat": COLOURS["helmet"], "ties": COLOURS["straps"]}
    VARIANTS = {
        "white_summer": {"shirt_own": "E2E0D8",
                         "about": "the white flannel jumper of summer (the trousers and the cap stay dark)"},
        "pea_coat": {"shirt_own": "16171B",
                     "about": "the black pea coat of the cold seas, in place of the jumper's colour"},
        "gun_crew": {"about": "a gun crew's man: the same colours, with a steel helmet (a helmet finish, in dark "
                              "grey-green 4E5446) drawn in place of the cap (helmet_paints)"},
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
CORK_ROWS = {"ita": (0.11, 0.22, 0.33)}     # (m over the hem: the grooves across a cork jacket, between its blocks)
BIB = {"sov": (0.115, 0.13)}   # (m: a bib's pads, front and back, end this far out from his middle, from this far under
#                                his shoulders down: open at his sides)
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

    def __init__(self, z0, z1, neck_c, neck_ax, hole, bib=None):
        self.box = Box((-0.32, -0.34, z0), (0.32, 0.34, z1))
        self.lo, self.hi, self.c, self.R = self.box.lo, self.box.hi, self.box.c, None
        self.nc, self.na, self.hole = np.asarray(neck_c, float), _n(neck_ax), hole
        self.bib = bib                                  # (a bib's sides open: past |y| = bib[0], under z = bib[1])

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
        out = np.maximum(out, (1.0 - e) * ry)
        if self.bib is not None:
            ys, zs = self.bib
            out = np.maximum(out, np.minimum(np.abs(by) - ys, zs - bz))     # (kept unless both out past ys and under zs)
        return out


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
    bib = (BIB[WHO][0], float(LM["shoulder"]) - BIB[WHO][1]) if WHO in BIB else None
    vest.add(Over(was, 0.0, VestRegion(z0, z1, nk, LM["neck_axis"], hole, bib), VEST_ROUND), 0.0)
    # ---- open down the front: the two halves meet over the breastbone with a groove between them
    zt = LM["collar"] - 0.03
    xf = float(on_surface(vest, (0.45, 0.0, (z0 + zt) / 2), (-1.0, 0.0, 0.0))[0][0])
    vest.sub(RoundBox((xf + 0.03 - VEST_GAP, 0.0, (z0 + zt) / 2), (0.03, 0.006, (zt - z0) / 2 + 0.02), 0.004), 0.004)
    # ---- the kapok in channels: a groove down the vest between its pads, front and back
    # (a bib's channels keep clear of its open sides: a groove by the edge would chew the edge ragged)
    grooves = [y for y in GROOVES if WHO not in BIB or y < BIB[WHO][0] - 0.03]
    for side in (1.0, -1.0):
        for y in grooves:
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
    # ---- a cork jacket's blocks: grooves across it as well, front and back
    for zz in CORK_ROWS.get(WHO, ()):
        for fb in (1.0, -1.0):
            pts = []
            for y in np.linspace(-0.16, 0.16, 9):
                try:
                    p, nrm = on_surface(vest, (0.45 * fb, y, z0 + zz), (-fb, 0.0, 0.0))
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
    # shorter shirt's hem would find no cloth to sit on). Not the American's: he was made before this, and his kit (built
    # and then taken off) is laid on those shapes
    before = len(shirt.ops)
    if WHO != "usa":
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
    if spec is S.NATION_KIT[NATION] and WHO in ("uk", "ger", "ita", "sov"):
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
S.HELMETS["roundcap"] = dict(S.HELMETS["dixie"])
S.HELMETS["bescap"] = dict(S.HELMETS["dixie"])
CAP = dict(clear=0.01, rim=0.03, top=0.012, back=0.008, foot=0.016, dome=0.131)
# the German rating's plate cap: the same make with a wider, flatter crown, and two black ribbons hanging behind from
# its band (each RIBBONS: how long, how wide, how thick; they hang out from his neck as they go down)
CAP_TELLER = dict(CAP, clear=0.008, rim=0.045, top=0.01, back=0.004)
RIBBONS = (0.15, 0.026, 0.0015)
CAP_ITA = dict(CAP, rim=0.034, top=0.011)      # (the Italian rating's round cap: a little wider in the crown)
CAP_SOV = dict(CAP, rim=0.036, top=0.011)      # (the Soviet rating's peakless cap)
RIBBONS_SOV = (0.15, 0.024, 0.0015)            # (its ribbons narrower: longer ones run into the jacket's neck pad)
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
    if kind == "roundcap" and "crown" in J:
        return navy_cap(J, sides or 36, CAP_ITA, "roundcap")
    if kind == "bescap" and "crown" in J:
        return navy_cap(J, sides or 36, CAP_SOV, "bescap", RIBBONS_SOV)
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


# ------------------------------------------------------------------------------------- hands up, and a deck gun's crew
# (the sea scene, 24 November: a U-boat's crew surrendering on its casing, and a surfaced boat's deck gun served)
def eased(keys, t, long):
    """Values keyed [(t, tuple)] round a loop, eased in and out at each key (a man stops at each: a grip, a lay, a ram)."""
    ts = [k[0] for k in keys]
    n = len(keys)
    t = t % long
    i = max(j for j in range(n) if ts[j] <= t)
    t1 = ts[i + 1] if i + 1 < n else long
    u = sm((t - ts[i]) / (t1 - ts[i]))
    a, b = np.asarray(keys[i][1], float), np.asarray(keys[(i + 1) % n][1], float)
    return a + (b - a) * u


def planted(P, l, r, l_yaw=8.0, r_yaw=-8.0, pole=None):
    """Both feet flat on the deck at (x, y) l and r (the root's frame), the knees bending toward `pole`."""
    for s, (x, y), yaw in (("l", l, l_yaw), ("r", r, r_yaw)):
        f = {"at": (x, y, MAN["ankle"]), "yaw": yaw}
        if pole is not None:
            side = 1.0 if s == "l" else -1.0
            f["pole"] = (pole[0], pole[1] * side, pole[2])
        P["foot_" + s] = f
    P["shin_feet"] = 0.0
    return P


UP_LONG = 4.0


@water_clip("hands_up", UP_LONG, loop=True, root="deck",
            about="Surrendering, standing (on a deck or a surfaced boat's casing; the root under his feet): both hands "
                  "raised open above his head, the palms forward; a slow small sway from foot to foot, the head turning a "
                  "little as he watches. For the crew of a boat that gives up.")
def c_hands_up(t):
    a = 2 * math.pi * t / UP_LONG
    w = math.sin(a)
    look = math.sin(0.5 * a + 0.6)
    P = body(MAN["pz"] - 0.014 - 0.005 * abs(w), y=0.012 * w, roll=-1.5 * w, yaw=1.5 * w, bend=(-2.5, 1.0 * w, 0.0),
             neck=(-3.0, 0.0, 6.0 * look), head=(-3.0, -1.0 * w, 8.0 * look))
    planted(P, (0.03, 0.13), (-0.02, -0.13))
    P["clavicle_l"] = P["clavicle_r"] = (0.0, 16.0)
    for s, side in (("l", 1.0), ("r", -1.0)):
        at = (0.07 + 0.008 * math.cos(a), side * 0.245 + 0.008 * w, MAN["shoulder"] + 0.52 + 0.006 * math.cos(a + side))
        P["hand_" + s] = world_hand(at, (0.1, side * 0.12, 1.0), (1.0, 0.0, -0.08), (0.3, side * 1.0, -0.15))
        P["fingers_" + s] = 6.0
        P["thumb_" + s] = 8.0
    return S.breathe(P, t, UP_LONG / 2, 1.3)


# ---- the gun: each man's own frame (the root on the deck under his feet, facing his clip's forward, the gun's way)
WHEEL_HUB = (0.40, -0.06, 1.00)   # (m: the handwheel's hub for the layer, the gun on his right; the trainer's mirrored)
WHEEL_R = 0.10                    # (m: its rim's radius; the wheel stands upright across his front, its axis along x)
LAY_LONG = 4.0
TRAIN_LONG = 4.8


def lay_turn(t):
    a = 2 * math.pi * t / LAY_LONG
    return 26.0 * math.sin(a) + 7.0 * math.sin(2 * a + 0.7)


def train_turn(t):
    a = 2 * math.pi * t / TRAIN_LONG
    return 34.0 * math.sin(a + 1.1) + 5.0 * math.sin(3 * a)


def wheel_hub(s):
    return np.array((WHEEL_HUB[0], s * WHEEL_HUB[1], WHEEL_HUB[2]))


def wheel_man(t, long, s, turn):
    """At a deck gun's sight, braced and crouched a little, leaning in, both hands on the handwheel ahead of him, which
    he turns to and fro as the boat rolls (s = 1: the layer, the gun on his right; s = -1: the trainer, on its other
    side, mirrored)."""
    a = 2 * math.pi * t / long
    th = math.radians(turn(t))
    sway = math.sin(a + 0.4)
    P = body(MAN["pz"] - 0.075, x=-0.045, y=-s * 0.012 + 0.006 * sway, pitch=9.0 + 1.0 * sway, roll=-s * 1.5,
             yaw=-s * 5.0, bend=(15.0 + 1.5 * math.sin(a), -s * 1.5, -s * 3.0 + 0.04 * math.degrees(th) * 0.3),
             neck=(-9.0, 0.0, -s * 3.0), head=(-9.0, -s * 1.5, -s * 2.0))
    if s > 0:
        planted(P, (0.17, 0.16), (-0.12, -0.15), 6.0, -16.0, pole=(1.0, 0.3, 0.2))
    else:
        planted(P, (-0.12, 0.15), (0.17, -0.16), 16.0, -6.0, pole=(1.0, 0.3, 0.2))
    hub = wheel_hub(s)
    u, v, fx = np.array((0.0, -1.0, 0.0)), np.array((0.0, 0.0, 1.0)), np.array((1.0, 0.0, 0.0))
    for hs, ph in (("r", 0.0), ("l", math.pi)):
        phi = ph + th
        rad = math.cos(phi) * u + math.sin(phi) * v
        tan = -math.sin(phi) * u + math.cos(phi) * v
        if tan[2] < 0.0:
            tan = -tan
        side = 1.0 if hs == "l" else -1.0
        P["hand_" + hs] = world_hand(hub + WHEEL_R * rad, _n(tan + 0.35 * fx), _n(0.75 * fx - 0.65 * rad),
                                     (-0.2, side * 1.0, -0.8))
        P["fingers_" + hs] = 68.0
        P["thumb_" + hs] = 45.0
    return S.breathe(P, t, long / 2, 0.8)


@water_clip("gun_layer", LAY_LONG, loop=True, root="deck",
            about="A deck gun's layer, at the gun's left side (the gun on his right): braced and crouched a little, "
                  "his eye at the sight, both hands on the elevating handwheel ahead of him, turning it to and fro as "
                  "the boat rolls. The `wheel` block gives the wheel's hub, rim and turn, and `sight` where his eye is, "
                  "in the root's frame, so the gun's wheel and eyepiece can be put there.")
def c_gun_layer(t):
    return wheel_man(t, LAY_LONG, 1.0, lay_turn)


@water_clip("gun_trainer", TRAIN_LONG, loop=True, root="deck",
            about="A deck gun's trainer, at the gun's right side (the gun on his left): the layer's stance mirrored, "
                  "his eye at his own sight, both hands on the training handwheel ahead of him, turning it in longer "
                  "sweeps. `wheel` and `sight` as gun_layer's.")
def c_gun_trainer(t):
    return wheel_man(t, TRAIN_LONG, -1.0, train_turn)


# ---- the loader: takes a round from the man behind him on his left, lays it in the tray and rams it home
ROUND_LONG = 0.72                  # (m: a fixed round, the shell and its case, nose to base)
ROUND_R = 0.05                     # (m: its case's radius, for the pictures)
BREECH = (1.02, -0.08, 1.02)       # (m: the chamber's mouth in the loader's frame: the boats' guns sit 1.00-1.05 m
                                   #  over the casing, the sea scene measured, 25 November; was 1.12)
BORE = (1.0, 0.0, 0.05)            # (the bore's line at the gun's resting elevation)
LOAD_LONG = 5.0
TAKE_T = 0.75                      # (s: he has it; the passer lets go)
TAKE_AT = (0.0, 0.25, 0.95)        # (m: the round's base when he takes it, at his left front)
TAKE_WAY = (0.95, -0.3, 0.05)      # (its nose's way then)


def loader_keys():
    """The round's keys: (t, its base's point, its nose's way); shown from the take until it is home."""
    d_b = _n(BORE)
    tray = np.array(BREECH) - ROUND_LONG * d_b
    pushed = np.array(BREECH) - 0.25 * d_b
    home = np.array(BREECH)
    tk, d0 = np.array(TAKE_AT), _n(TAKE_WAY)
    return [(0.0, tk, d0), (TAKE_T, tk, d0), (1.65, np.array((0.08, 0.03, 1.1)), _n((1.0, -0.06, 0.05))),
            (2.45, tray, d_b), (2.75, tray, d_b), (3.02, pushed, d_b), (3.18, home, d_b), (3.5, home, d_b)]


def loader_round(t):
    """The round's base point, its nose's way, and whether it is shown (in his hands, or sliding home)."""
    K = loader_keys()
    t = t % LOAD_LONG
    v = eased([(k[0], tuple(k[1]) + tuple(k[2])) for k in K], t, LOAD_LONG)
    return v[:3], _n(v[3:]), (1.0 if TAKE_T <= t < 3.18 else 0.0)


def round_hands(base, d, under=0.45):
    """The right hand cupping the round's base, the left under its case (palm up)."""
    z = np.array((0.0, 0.0, 1.0))
    ar = _n(z - (z @ d) * d)
    al = _n(np.cross(d, z))
    r = world_hand(base - 0.03 * d + 0.01 * ar, ar, d, (-0.2, -1.0, -0.9))
    l = world_hand(base + under * ROUND_LONG * d - 0.045 * z, al, z, (-0.1, 1.0, -0.9))
    return l, r


def _mix(a, b, u):
    if u <= 1e-4:
        return a
    if u >= 1.0 - 1e-4:
        return b
    return {"blend": [(1.0 - u, a), (u, b)]}


LOAD_TRUNK = [(0.0, (46.0, 24.0, 0.0, 0.08)), (TAKE_T, (46.0, 24.0, 0.0, 0.08)), (1.65, (4.0, 10.0, 0.0, 0.05)),
              (2.45, (-2.0, 30.0, 0.03, 0.08)), (2.75, (-2.0, 30.0, 0.03, 0.08)), (3.05, (-3.0, 33.0, 0.12, 0.08)),
              (3.5, (-2.0, 22.0, 0.08, 0.05)), (4.4, (42.0, 24.0, 0.0, 0.08))]
# (his trunk through the loop: turn to his left, lean forward (degrees), lunge toward the gun and crouch (m); bent
#  at the knees and the hips enough that both hands reach the round where it is taken and laid: a loader lifting
#  a 15-25 kg round from waist height and laying it at the breech, 1.02 m over the deck)


@water_clip("gun_loader", LOAD_LONG, loop=True, root="deck",
            events=[sev(TAKE_T, "round_taken", "round_handoff"), sev(3.0, "ram", "shell_ram"),
                    sev(3.3, "breech_close", "breech_close")],
            about="A deck gun's loader, behind the breech: turned to his left he takes a round from the passer "
                  "(ammo_passer or another passer clip) or the carrier (ammo_carrier_give), swings it round to his chest, lays it in the tray nose first and rams it home "
                  "with his right fist, lunging after it; the breech closes and he turns back for the next, one round "
                  "about every 5 s. The `round` block gives the round's base, its nose's way and when it is shown (in "
                  "his hands, then sliding into the chamber), and the chamber's mouth, in the root's frame.")
def c_gun_loader(t):
    tt = t % LOAD_LONG
    # his trunk: turned to the left to take it, square to the gun to lay it, lunging in on the ram
    turn, lean, lunge, crouch = eased(LOAD_TRUNK, tt, LOAD_LONG)
    P = body(MAN["pz"] - 0.05 - 0.25 * lunge - crouch, x=-0.03 + lunge, pitch=0.35 * lean, yaw=0.4 * turn,
             bend=(0.65 * lean, 0.0, 0.6 * turn), neck=(-0.3 * lean, 0.0, 0.25 * turn), head=(-0.25 * lean, 0.0, 0.15 * turn))
    planted(P, (0.2, 0.17), (-0.2, -0.14), 14.0, -20.0, pole=(1.0, 0.3, 0.15))
    base, d, _ = loader_round(min(tt, 2.75))
    l_path, r_path = round_hands(base, d)
    l_take, r_take = round_hands(np.array(TAKE_AT), _n(TAKE_WAY))
    rest = world_hand((0.55, 0.24, 1.08), (1.0, 0.0, -0.2), (0.0, -1.0, 0.0), (-0.2, 1.0, -0.9))
    b2, d2, _ = loader_round(min(max(tt, 2.75), 3.02))
    dn = np.array((0.0, 0.0, -1.0))
    fist = world_hand(b2 - 0.06 * d2, d2, _n(dn - (dn @ d2) * d2), (-0.2, -1.0, -0.9))
    if tt < 2.6:
        l, r, k = l_path, r_path, 0.0
    elif tt < 3.4:
        l = _mix(l_path, rest, ramp(tt, 2.6, 2.75))
        k = ramp(tt, 2.6, 2.8)
        r = _mix(r_path, fist, k)
    else:
        u = ramp(tt, 3.45, 4.4)
        l = _mix(rest, l_take, u)
        r = _mix(fist, r_take, ramp(tt, 3.4, 4.3))
        k = 1.0 - ramp(tt, 3.4, 3.8)
    P["fingers_r"] = 40.0 + 50.0 * k
    P["thumb_r"] = 30.0 + 30.0 * k
    P["fingers_l"], P["thumb_l"] = 40.0, 30.0
    P["hand_l"], P["hand_r"] = l, r
    return S.breathe(P, t, LOAD_LONG / 2, 1.2)


# ---- the passer: up through the hatch behind the gun, handing the rounds up from below; and where the hatch is too
# far from the loader for one man's reach, the carrier, who takes them at the hatch and walks them along the casing
# (the sea scene, 25 November: hatches 0.75 to 4.6 m from the loader's take, as the boats' guns train)
PASS_LONG = LOAD_LONG
HATCH_DOWN = 0.90                  # (m: the rung he takes the rounds on, under the deck)
HATCH_HALF = 0.30                  # (m: half the hatch's opening, fore and aft and across: under the deck his legs, his
                                   #  hands and the round stay inside it)
RUNG = 0.25                        # (m: from rung to rung of its ladder)
PASSER_AT = (-0.32, 0.30, 0.0)     # (m: his root from the loader's, so the round passes from hand to hand)
PASS_HOLD = (0.05, 0.22)           # (along the round from its base: his right hand, his left, under it)
SHELL_HOLD = (0.60, 0.82)          # (the far reaching passers' and the carrier's: his left hand, his right, under the
                                   #  round's shell, clear of the loader's, who takes it by its base and its case)
Z_UP = np.array((0.0, 0.0, 1.0))


def held(keys, t):
    """Values keyed [(t, tuple)], eased in and out at each key and held after the last (a clip that does not loop)."""
    ts = [k[0] for k in keys]
    if t <= ts[0]:
        return np.asarray(keys[0][1], float)
    if t >= ts[-1]:
        return np.asarray(keys[-1][1], float)
    i = max(j for j in range(len(keys)) if ts[j] <= t)
    u = sm((t - ts[i]) / (ts[i + 1] - ts[i]))
    a, b = np.asarray(keys[i][1], float), np.asarray(keys[i + 1][1], float)
    return a + (b - a) * u


def grip(base, d, f, side, fingers="fwd", off=(0.0, 0.0, 0.0)):
    """A hand under the round, f of its length along from its base, palm up against it (moved by `off`). `fingers`
    fwd: across the round, pointing on from his forearm (away from his shoulder on that side, so the wrist is never
    turned back on itself); first: the first passer's (across it, to his right)."""
    d = _n(d)
    if fingers == "first":
        n, a = Z_UP, _n(np.cross(d, Z_UP))
        at = base + f * ROUND_LONG * d - 0.045 * Z_UP
    else:
        n = _n(Z_UP - (Z_UP @ d) * d)
        a = _n(np.cross(n, d))
        at = base + f * ROUND_LONG * d - 0.045 * n
        reach = np.array((at[0], at[1] - side * 0.185))          # (from his shoulder, level)
        if a[0] * reach[0] + a[1] * reach[1] < 0.0:
            a = -a
    return world_hand(np.asarray(at) + np.asarray(off, float), a, n, (-0.2, side * 1.0, -0.9))


def across(alpha, c, z=0.95):
    """A round held across in front of him by its shell (SHELL_HOLD), its nose `alpha` degrees round from pointing
    back at him toward his right, his hands' middle c ahead of him: its base's point and its nose's way."""
    a = math.radians(alpha)
    d = _n((-math.cos(a), -math.sin(a), 0.05))
    mid = 0.5 * (SHELL_HOLD[0] + SHELL_HOLD[1]) * ROUND_LONG
    base = np.array((c, 0.0, 0.0)) - mid * d
    base[2] = z
    return base, d


def taker(base, d):
    """Where the loader stands for this round to be his take (TAKE_AT, its nose TAKE_WAY):
    his root and his turn (degrees to the left) in the giver's frame."""
    tw = _n(TAKE_WAY)
    turn = math.degrees(math.atan2(d[1], d[0]) - math.atan2(tw[1], tw[0]))
    turn = (turn + 180.0) % 360.0 - 180.0
    c, s = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    ta = np.array(TAKE_AT)
    root = np.array((base[0] - (c * ta[0] - s * ta[1]), base[1] - (s * ta[0] + c * ta[1]), 0.0))
    return root, turn


def rung_steps(h):
    """His feet's steps from the rung he gives on (h under the deck) down to HATCH_DOWN after he lets go, and back up
    with the next round: [(foot, from t, to t, the rung's depth)]."""
    n = int(round((HATCH_DOWN - h) / RUNG))
    if n <= 0:
        return []
    if n == 1:
        return [("r", 1.05, 1.35, HATCH_DOWN), ("l", 1.35, 1.65, HATCH_DOWN), ("r", 2.65, 2.95, h), ("l", 2.95, 3.25, h)]
    mid = h + RUNG
    return [("r", 1.0, 1.3, mid), ("l", 1.3, 1.6, HATCH_DOWN), ("r", 1.6, 1.9, HATCH_DOWN),
            ("r", 2.62, 2.9, mid), ("l", 2.9, 3.2, h), ("r", 3.2, 3.5, h)]


def passer(reach, give, way, hold, fingers, rung, trunk_give, trunk_low, low, path=(), trunk_rise=None):
    """A passer: where he gives the round and how it lies, his grip, the rung he gives on, his trunk (lean forward,
    the pelvis's x, crouch, yaw to his left) giving and taking it from below (and, if given, as it rises at 3.3 s),
    the round low (base, way), and its keys (t, base, way) from there up to the give, clear of the loader (see
    PASS_PATHS)."""
    give = np.asarray(give, float)
    steps = rung_steps(rung)
    root, turn = taker(give, way)
    back = (8.0, -0.05, 0.0, 0.0)       # (climbing, his hips back from the ladder, so a raised knee keeps off its rungs)
    trunk = [(0.0, trunk_give), (TAKE_T, trunk_give)] + ([(1.2, back)] if steps else []) + \
        [(2.3, trunk_low), (2.6, trunk_low)] + ([(3.3, trunk_rise)] if trunk_rise else []) + [(3.9, trunk_give)]
    return {"reach": float(np.hypot(give[0], give[1])), "nominal": reach, "give": give, "way": _n(way), "hold": hold,
            "fingers": fingers, "rung": rung, "steps": steps,
            "low": (np.asarray(low[0], float), _n(low[1])), "trunk": trunk, "loader_root": root, "loader_turn": turn,
            "path": [(k[0], np.asarray(k[1], float), _n(k[2])) for k in path]}


UP_T = 2.45                         # (s: round_up: the next round shows in his hands, low in front of him, as if handed up
                                    #  to him past his legs (a 0.72 m round has no room beside them in the 0.6 m opening))


def _turned(d, deg):
    """The way d turned deg degrees to his left, about the upright."""
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return _n((c * d[0] - s * d[1], s * d[0] + c * d[1], d[2]))


SHELL_MID = 0.5 * (SHELL_HOLD[0] + SHELL_HOLD[1]) * ROUND_LONG     # (m: from the base to his hands' middle, by the shell)
# PASS_PATHS: how the round comes up from below to the give, clear of the loader. In the loader's own frame the give is
# always his take, so his loop (laying it in the tray, the ram, turning back) sweeps the same space round it for every
# passer: his left thigh lies under the round there, and his arms work the tray beside it until about 3.4 s. So the
# round never rises under the give: the first passers (holding it by its base, behind the loader on his left) take it
# from below with its nose turned 60 degrees to their left, lift it so on their left (clear of their own heads), and
# swing the nose round onto the loader's take from 3.3 s; the far passers (across in front of them by the shell) take it
# low on their right (hands 0.48 m ahead, 0.22 m right, 0.30 m up, the same for every reach), its base turned 40 degrees
# ahead, lift it there (hands 0.3 m to the right, 0.7 m up, at most 0.5 m ahead) and bring it across onto the give from
# 3.15 s. Laid out by a search over these keys against the loader's loop
# (his bones every 1/30 s, his round, the breech), then checked on the rig. The low takes are 0.27 to 0.30 m over the
# deck, so a passer stood up to 0.125 m lower (a hatch higher than the loader's deck) keeps his arms off the deck plate.
PASS_PATHS = {
    "first": {"low": ((0.25, 0.22, 0.27), 60.0), "rise": (3.3, (-0.02, 0.25, 0.88), 60.0), "swing": (3.6, (-0.01, 0.11, 0.94), 30.0)},
    "fwd": {"low": ((0.48, -0.22, 0.30), -40.0), "rise": (3.15, (-0.05, -0.30, 0.70), -30.0, 0.50), "swing": (3.55, -0.10)},
}


def first_path(give, way, rise=None):
    """The first passers' round from below (base, way) and its keys up to the give (PASS_PATHS first: the rise's base
    at (give x + dx, y, z), or `rise`'s, the swing's at (give x + dx, give y + dy, z))."""
    P = PASS_PATHS["first"]
    lb = np.asarray(P["low"][0], float)
    ld = _turned(way, P["low"][1])
    rt, (rx, ry, rz), ra = rise or P["rise"]
    st_, (sx, sy, sz), sa = P["swing"]
    return (lb, ld), [(rt, np.array((give[0] + rx, ry, rz)), _turned(way, ra)),
                      (st_, np.array((give[0] + sx, give[1] + sy, sz)), _turned(way, sa))]


def across_path(give, way):
    """The far passers' round from below and its keys up to the give, by his hands' middle (PASS_PATHS fwd: low at
    the same place for every reach, the rise's at (give x + dx, y, z) but at most `most` ahead, the swing's beside
    the give)."""
    P = PASS_PATHS["fwd"]
    gm = give + SHELL_MID * way
    ml, al = P["low"]
    ld = _turned(way, al)
    low = (np.asarray(ml, float) - SHELL_MID * ld, ld)
    rt, (dxr, yr, zr), ar, most = P["rise"]
    rd = _turned(way, ar)
    rise = (rt, np.array((min(gm[0] + dxr, most), yr, zr)) - SHELL_MID * rd, rd)
    st_, ys = P["swing"]
    swing = (st_, gm + np.array((0.0, ys, 0.0)) - SHELL_MID * way, way)
    return low, [rise, swing]


PASSERS = {}
# The first passers' trunk (lean, the pelvis's x, crouch, yaw to his left): giving, taking it from below, and turned to
# his left as it rises there, so his right hand reaches the base across him.
FIRST_GIVE, FIRST_LOW, FIRST_RISE = (16.0, 0.12, 0.0, 0.0), (24.0, 0.0, 0.04, 8.0), (12.0, 0.06, 0.0, 25.0)
_g0 = np.array(TAKE_AT) - np.array(PASSER_AT)
_lo, _pa = first_path(_g0, _n(TAKE_WAY))
PASSERS["ammo_passer"] = passer(float(np.hypot(_g0[0], _g0[1])), _g0, TAKE_WAY, (PASS_HOLD[1], PASS_HOLD[0]), "first",
                                HATCH_DOWN, FIRST_GIVE, FIRST_LOW, _lo, _pa, FIRST_RISE)
# (his hips 0.14 m nearer the deck's forward edge as he gives it, leaning 16 degrees, so both hands reach it (his
#  right was 33 mm short); he takes the next turned a little to his left, the round's nose that way, clear of the
#  loader on his right)
# The farther passers. Each is laid out so that, as its pair says, the loader stands clear of the hatch (his feet
# 0.12 m or more outside its opening): at 0.5 m as the first passer, holding the round by its base, a rung up; from
# 0.70 m to 1.10 m every 0.05 m (the sea scene picks the nearest, so its 4 cm test holds within 2.5 cm), across in front
# of him by its shell (the loader's hands at its base and case), the nose `alpha` degrees round from pointing back at
# him (75 at 0.70 m to 60 at 1.10 m): to 0.80 m on the low rung (0.90 m under the deck: a rung up, his head was in the
# round), from 0.85 m a rung up (0.65 m) to give and down to the low rung to take.
PASS_REACHES = [round(0.70 + 0.05 * k, 2) for k in range(9)]


def pass_alpha(r):
    return 75.0 - 37.5 * (r - 0.70)


def pass_rung(r):
    return HATCH_DOWN if r <= 0.80 + 1e-6 else HATCH_DOWN - RUNG


def pass_trunk(r):
    """His trunk (lean, the pelvis's x, crouch, yaw to his left) giving at reach r, and taking it from below."""
    lean = 12.0 + 40.0 * (r - 0.70) if pass_rung(r) >= HATCH_DOWN else 12.0 + 20.0 * (r - 0.70)
    return (lean, 0.14, 0.0, 0.0), (26.0, -0.02, 0.04, -12.0)


_g5 = 0.5 * np.array((math.cos(math.atan2(_g0[1], _g0[0])), math.sin(math.atan2(_g0[1], _g0[0])), 0.0)) + (0.0, 0.0, TAKE_AT[2])
# (a rung up, he lifts it lower and nearer him, turned less, so his right hand reaches its base and his knee, coming up
#  the ladder, keeps inside the hatch)
_lo, _pa = first_path(_g5, _n(TAKE_WAY), rise=(3.3, (-0.12, 0.15, 0.82), 60.0))
PASSERS["ammo_passer_50"] = passer(0.5, _g5, TAKE_WAY, (PASS_HOLD[1], PASS_HOLD[0]), "first", HATCH_DOWN - RUNG,
                                   FIRST_GIVE, FIRST_LOW, _lo, _pa, (16.0, 0.03, 0.0, 14.0))
for _r in PASS_REACHES:
    _al = pass_alpha(_r)
    _a = math.radians(_al)
    _c = math.sqrt(_r * _r - (0.71 * ROUND_LONG * math.sin(_a)) ** 2) - 0.71 * ROUND_LONG * math.cos(_a)
    _b, _d = across(_al, _c)
    _lo, _pa = across_path(_b, _d)
    _tg, _tl = pass_trunk(_r)
    PASSERS["ammo_passer_%d" % round(_r * 100)] = passer(_r, _b, _d, SHELL_HOLD, "fwd", pass_rung(_r), _tg, _tl, _lo, _pa)


STEP_MID = (0.32, -0.02, 0.50)     # (m: his hands' middle, the round hidden, as he climbs down for the next)


def passer_round(t, name="ammo_passer"):
    """The round he holds up (its base, its nose's way, shown), in his own frame: at the give, where the loader's take
    is in the pair; low, he takes the next from a man below; then up by its path to the give."""
    V = PASSERS[name]
    G, d = V["give"], V["way"]
    lb, ld = V["low"]
    if V["steps"]:                     # (hidden: his hands come in near him as he climbs down)
        dm = _n(d + ld)
        mid = (1.25, np.array(STEP_MID) - 0.5 * (V["hold"][0] + V["hold"][1]) * ROUND_LONG * dm, dm)
    else:
        mid = (1.1, G + (0.0, 0.0, -0.05), d)
    keys = [(0.0, G, d), (TAKE_T, G, d), mid, (2.3, lb, ld), (2.6, lb, ld)] + list(V["path"]) + [(3.9, G, d)]
    tt = t % PASS_LONG
    v = eased([(k[0], tuple(k[1]) + tuple(k[2])) for k in keys], tt, PASS_LONG)
    return v[:3], _n(v[3:]), (1.0 if (tt < TAKE_T or tt >= UP_T) else 0.0)


def rung_at(V, s, t):
    """How deep foot s stands under the deck at t, and how far through a step it is (0 standing, 1 mid-step)."""
    h = V["rung"]
    for f, t0, t1, to in V["steps"]:
        if f != s:
            continue
        if t >= t1:
            h = to
        elif t > t0:
            u = (t - t0) / (t1 - t0)
            return h + (to - h) * sm(u), math.sin(math.pi * u)
        else:
            break
    return h, 0.0


FREE_OFF = (0.04, 0.0, -0.10)       # (m: his hands from the round while it is hidden, going down for the next: ahead, out, down)


def passer_pose(name, t):
    V = PASSERS[name]
    tt = t % PASS_LONG
    base, d, sh = passer_round(tt, name)
    lean, px, crouch, yaw = eased(V["trunk"], tt, PASS_LONG)
    deep = {}
    for s in ("l", "r"):
        deep[s] = rung_at(V, s, tt)
    P = body(MAN["pz"] - max(deep["l"][0], deep["r"][0]) - 0.02 - crouch, x=px, pitch=0.3 * lean, yaw=yaw,
             bend=(0.7 * lean, 0.0, 0.714 * yaw), neck=(-0.4 * lean - 4.0, 0.0, -0.286 * yaw),
             head=(-0.3 * lean - 4.0, 0.0, -0.286 * yaw))
    for s, side in (("l", 1.0), ("r", -1.0)):
        h, u = deep[s]
        P["foot_" + s] = {"at": (0.02 - 0.05 * u, side * 0.12, -h + MAN["ankle"] + 0.04 * u), "yaw": side * 6.0,
                          "pole": (1.0, side * 0.2, 0.1)}
    P["shin_feet"] = 0.0
    let_go = ramp(tt, TAKE_T, TAKE_T + 0.3) * (1.0 - ramp(tt, 1.9, 2.3))
    for s, side, f in (("l", 1.0, V["hold"][0]), ("r", -1.0, V["hold"][1])):
        hold = grip(base, d, f, side, V["fingers"])
        free = grip(base, d, f, side, V["fingers"], off=(FREE_OFF[0], side * FREE_OFF[1], FREE_OFF[2]))
        P["hand_" + s] = _mix(hold, free, let_go)
        P["fingers_" + s] = 45.0
        P["thumb_" + s] = 30.0
    return S.breathe(P, t, PASS_LONG / 2, 1.2)


def _pass_events(name):
    ev = [sev(UP_T, "round_up", "round_lift")]
    for f, t0, t1, to in PASSERS[name]["steps"]:
        ev.append(sev(t1, "footstep", "footstep", bone="foot_" + f))
    return sorted(ev, key=lambda e: e["t"])


@water_clip("ammo_passer", PASS_LONG, loop=True, root="hatch", events=_pass_events("ammo_passer"),
            about="Handing the rounds up through a hatch behind a deck gun: stood on a rung 0.9 m under the deck (the "
                  "root at the hatch's middle on the deck, facing the gun's way), the deck at his waist, he holds a round "
                  "up for the loader, lets it go as the loader takes it, stoops for the next (it shows in his hands at "
                  "round_up, low in front of him, its nose turned to his left, as if handed up past him), lifts it so "
                  "on his left and swings it round onto the loader's take as the loader turns back, clear of the "
                  "loader's legs and body, one about every 5 s. Played beside gun_loader from the same moment: "
                  "`pair` gives the loader's root from his, so the round passes from hand to hand; `round` as the "
                  "loader's. While a carrier is away from the hatch, ammo_passer_wait holds the next one ready.")
def c_ammo_passer(t):
    return passer_pose("ammo_passer", t)


def _reach_clip(name):
    V = PASSERS[name]
    climb = ("climbing a rung (%.2f m under the deck) to give it and back down to take the next" % V["rung"]) if V["steps"] \
        else "on the rung 0.9 m under the deck"
    hold = ("holds the round up by its base, as ammo_passer does, the loader ahead of him on his right facing his "
            "way" if V["fingers"] == "first" else "holds the round out across in front of him by its shell, its base "
            "toward the loader")
    low = ("stoops for the next (it shows in his hands at round_up, low in front of him, its nose turned to his left, "
           "as if handed up past him), lifts it so on his left and swings it round onto the loader's take as the "
           "loader turns back" if V["fingers"] == "first" else "stoops for the next (it shows in his hands at round_up, "
           "low on his right, as if handed up past him), lifts it there and brings it across onto the loader's take as "
           "the loader turns back")
    about = ("The passer for a hatch farther from the loader: his root at the hatch's middle, the loader's take %.2f m "
             "from it (`pair`). Up through the hatch, %s, he %s, lets it go as the loader takes it, and %s, clear of "
             "the loader's legs and body (his hands come onto it as at his take), one about every 5 s. Played with "
             "gun_loader from the same moment; turn him on his root so the round's base meets the loader's take, within "
             "the pair's turn_within degrees of its hatch_bearing."
             % (V["reach"], climb, hold, low))

    @water_clip(name, PASS_LONG, loop=True, root="hatch", events=_pass_events(name), about=about)
    def c(t):
        return passer_pose(name, t)
    return c


for _k in sorted(PASSERS):
    if _k != "ammo_passer":
        _reach_clip(_k)
PASS_CLIPS = tuple(sorted(PASSERS, key=lambda k: PASSERS[k]["reach"]))


PW_LONG = 5.0                      # (s: the loader's loop, so a wait of whole loops ends on ammo_passer's first frame)


def passer_wait_round(t):
    """The round ammo_passer holds up ready while the carrier is away: at his give at the loop's ends (so it runs on
    into ammo_passer from its start, and back), let down a little in the middle to rest his arms."""
    V = PASSERS["ammo_passer"]
    u = math.sin(math.pi * (t % PW_LONG) / PW_LONG) ** 2
    return V["give"] + np.array((-0.05 * u, 0.02 * u, -0.08 * u)), V["way"], 1.0


@water_clip("ammo_passer_wait", PW_LONG, loop=True, root="hatch",
            about="ammo_passer holding the next round up ready at his give while the carrier is away from the hatch: "
                  "letting it down a little and lifting it again, his weight easing on the rung. Its ends are "
                  "ammo_passer's first frame, so it runs on into ammo_passer (from its start, as the carrier comes to "
                  "take) and back out of it at its end. `round` as ammo_passer's.")
def c_ammo_passer_wait(t):
    V = PASSERS["ammo_passer"]
    u = math.sin(math.pi * (t % PW_LONG) / PW_LONG) ** 2
    lean, px, crouch, yaw = np.array(V["trunk"][0][1], float) + np.array((-4.0, -0.03, 0.03, 2.0)) * u
    h = V["rung"]
    P = body(MAN["pz"] - h - 0.02 - crouch, x=px, pitch=0.3 * lean, yaw=yaw,
             bend=(0.7 * lean, 0.0, 0.714 * yaw), neck=(-0.4 * lean - 4.0, 0.0, -0.286 * yaw),
             head=(-0.3 * lean - 4.0, 0.0, -0.286 * yaw))
    for s, side in (("l", 1.0), ("r", -1.0)):
        P["foot_" + s] = {"at": (0.02, side * 0.12, -h + MAN["ankle"]), "yaw": side * 6.0, "pole": (1.0, side * 0.2, 0.1)}
    P["shin_feet"] = 0.0
    base, d, _ = passer_wait_round(t)
    for s, side, f in (("l", 1.0, V["hold"][0]), ("r", -1.0, V["hold"][1])):
        P["hand_" + s] = grip(base, d, f, side, V["fingers"])
        P["fingers_" + s] = 45.0
        P["thumb_" + s] = 30.0
    return S.breathe(P, t, PW_LONG / 2, 1.2)


# ---- the carrier: takes the round from ammo_passer, carries it along the casing (the game moves his root), waits if he
# must, and gives it to the loader, by its shell. He stands on the loader's left, turned to face across him: the gun's
# tray and breech run ahead of the loader on his right, and a man holding the round's shell from there would stand in
# the tray (the first build put him there). So he holds the round across his front with its nose to his left and its
# base to his right, where the loader takes it; and at the hatch he takes it from ammo_passer the same way, standing
# ahead of the passer, beside the round's middle on its left, turned to face across it and the passer's front.
CARRY_MID = (0.28, 0.0, 1.10)      # (m: his hands' middle carrying it, the round across his front, its nose to his left)
CARRY_WAY = (0.0, 1.0, 0.05)
CARRY_HOLD = (0.65, 0.86)          # (along the round from its base: his right hand, his left, under its shell, clear of
                                   #  the loader's left hand and forearm as he takes it by the base and case)
CT_LONG = 2.2
GIVE_LEAD = 10.0 / 3.0             # (s: the loader's time in his loop as the give starts, the loop before his take:
                                   #  after his breech_close (3.3 s), on the keys' 1/15 s grid)
GIVE_T = LOAD_LONG - GIVE_LEAD + TAKE_T         # (s: the take, in the give: 2.5)
CG_LONG = GIVE_T + 1.4
CARRIER_WAY = _turned(_n(TAKE_WAY), 90.0)       # (the round's way as he gives it, and takes it: forward and to his left)
CARRIER_GIVE = (np.array((0.42, 0.0, 0.0)) - SHELL_MID * CARRIER_WAY, CARRIER_WAY)
CARRIER_GIVE[0][2] = TAKE_AT[2]                 # (its base, at the loader's take; his hands' middle 0.42 m ahead of him)
CARRIER_TAKE_YAW = -90.0                        # (his turn from ammo_passer's facing as he takes, degrees to the left)
CARRIER_TAKE_AT = PASSERS["ammo_passer"]["give"] - np.array((CARRIER_GIVE[0][1], -CARRIER_GIVE[0][0], 0.0)) - \
    np.array((0.0, 0.0, PASSERS["ammo_passer"]["give"][2]))
# (his root in ammo_passer's frame as he takes: so ammo_passer's give is CARRIER_GIVE in his own frame)
CARRY_GAIT = S.Gait(1.2, 1.1, 0.6, 0.06, center=-0.085, width=0.11, heel=-12.0, push=44.0, flat=(0.12, 0.45),
                    base=-0.02, bob=0.01)
CARRY_Z0 = float(np.mean(CARRY_GAIT.zs))


def _low_across(base, d, mid=(0.30, 0.0, 0.10)):
    """The round lying across in front of him on its way `d`, the shell's middle (SHELL_HOLD) at `mid`, lifted by
    the hand's thickness under it."""
    m = 0.5 * (SHELL_HOLD[0] + SHELL_HOLD[1]) * ROUND_LONG
    b = np.asarray(mid, float) - m * d + 0.045 * _n(Z_UP - (Z_UP @ d) * d)
    return b, d


def carry_round(dz=0.0, dy=0.0):
    b, d = _low_across(np.zeros(3), _n(CARRY_WAY), mid=np.array(CARRY_MID) + (0.0, dy, dz))
    return b, d


def carrier_body(turn, lean, crouch, x=0.0):
    return body(MAN["pz"] - 0.02 - crouch, x=x, pitch=0.3 * lean, yaw=0.4 * turn,
                bend=(0.65 * lean, 0.0, 0.6 * turn), neck=(-0.3 * lean, 0.0, 0.25 * turn), head=(-0.25 * lean, 0.0, 0.15 * turn))


def carrier_hands(P, base, d, free=0.0, rest=0.0, rest_x=0.0):
    for s, side, f in (("l", 1.0, CARRY_HOLD[1]), ("r", -1.0, CARRY_HOLD[0])):     # (the nose to his left)
        h = grip(base, d, f, side)
        if free > 0.0:
            h = _mix(h, grip(base, d, f, side, off=(-0.08, 0.0, -0.04)), free)
        if rest > 0.0:
            h = _mix(h, world_hand((0.04 + rest_x, side * 0.22, 0.86), (0.0, 0.0, -1.0), (0.0, -side, 0.0),
                                   (-1.0, side * 0.3, 0.0)), rest)
        P["hand_" + s] = h
        P["fingers_" + s] = 45.0 * (1.0 - rest) + 20.0 * rest
        P["thumb_" + s] = 30.0 * (1.0 - rest) + 15.0 * rest
    return P


def carrier_take_round(t):
    """The round in the carrier's hands as he takes it (his frame): at ammo_passer's give, then across his front."""
    g, tw = CARRIER_GIVE
    cb, cd = carry_round()
    v = held([(0.0, tuple(g) + tuple(tw)), (TAKE_T, tuple(g) + tuple(tw)), (1.75, tuple(cb) + tuple(cd))], t)
    return v[:3], _n(v[3:]), (1.0 if t >= TAKE_T else 0.0)


TAKE_TRUNK = [(0.0, (0.0, 4.0, 0.0)), (0.55, (0.0, 20.0, 0.06)), (TAKE_T, (0.0, 20.0, 0.06)), (1.75, (0.0, 4.0, 0.0))]


@water_clip("ammo_carrier_take", CT_LONG, root="deck", events=[sev(TAKE_T, "round_taken", "round_handoff")],
            about="The carrier takes a round from ammo_passer at the hatch: coming in empty handed (from walk or "
                  "idle) he bends to it and puts both hands under its shell while the passer holds its base (standing "
                  "ahead of the passer, beside the round's middle on its left, facing across it), takes it at 0.75 s "
                  "and draws it in across his front to carry it, its nose to his left; it ends standing, carrying. "
                  "Played from the same moment as ammo_passer's loop, his root and yaw where `pair` puts them from "
                  "the passer's, both on the hatch's deck.")
def c_carrier_take(t):
    turn, lean, crouch = held(TAKE_TRUNK, t)
    P = carrier_body(turn, lean, crouch)
    planted(P, (0.08, 0.13), (-0.04, -0.13), 12.0, -8.0, pole=(1.0, 0.3, 0.15))
    b, d, _ = carrier_take_round(t)
    carrier_hands(P, b, d, rest=1.0 - ramp(t, 0.0, 0.55))       # (his hands come in to it from his sides)
    return S.breathe(P, t, CT_LONG, 1.0)


def carrier_walk_round(t):
    g = CARRY_GAIT
    ph = 2 * math.pi * t / g.long
    return carry_round(g.pelvis_z(t) - CARRY_Z0, 0.7 * 0.014 * math.cos(ph))


@water_clip("ammo_carrier_walk", CARRY_GAIT.long, loop=True, stride=CARRY_GAIT.stride, root="deck",
            events=[sev(0.0, "footstep", "footstep", bone="foot_l"), sev(CARRY_GAIT.long / 2, "footstep", "footstep", bone="foot_r")],
            about="The carrier walking along the casing with a round across his front, both hands under its shell, a "
                  "shorter step for its weight: move the root on at stride / long metres a second.")
def c_carrier_walk(t):
    P = body(MAN["pz"])
    S.gait_pose(CARRY_GAIT, t, 4.0, 3.0, 0.014, P)
    P["shin_feet"] = 0.0
    ph = 2 * math.pi * t / CARRY_GAIT.long
    P["neck"] = (-4.0, 0.0, 1.5 * math.cos(ph + 0.4))
    P["head"] = (-4.0 + 1.0 * math.cos(2 * ph), 0.0, 1.0 * math.cos(ph + 0.4))
    b, d = carrier_walk_round(t)
    carrier_hands(P, b, d)
    return S.breathe(P, t, CARRY_GAIT.long * 2, 0.8)


WAIT_LONG = 4.0


def carrier_wait_round(t):
    w = math.sin(2 * math.pi * t / WAIT_LONG)
    return carry_round(-0.004 * abs(w), 0.012 * w)


@water_clip("ammo_carrier_wait", WAIT_LONG, loop=True, root="deck",
            about="The carrier standing with a round across his front, waiting for the loader (or, empty handed, use "
                  "idle for the passer): his weight shifting a little from foot to foot.")
def c_carrier_wait(t):
    w = math.sin(2 * math.pi * t / WAIT_LONG)
    P = carrier_body(1.5 * w, 3.0, 0.01)
    P["pelvis_at"] = (0.0, 0.012 * w, -0.03 - 0.004 * abs(w))
    planted(P, (0.04, 0.13), (-0.02, -0.13), 8.0, -8.0)
    b, d = carrier_wait_round(t)
    carrier_hands(P, b, d)
    return S.breathe(P, t, WAIT_LONG, 1.0)


STEP_IN = 0.45                     # (m: he waits this far back from where he gives, steps in to give and back out, clear
                                   #  of the loader's loop the rest of the time)
# He steps in as the loader turns back from the breech, so the round is where the loader takes it (by 0.65 s, the
# loader's 3.9 s, as the passers' are) before the loader's hands come round to it, and holds it there for him.
GIVE_STEPS = [("r", 0.05, 0.30, 1.0), ("l", 0.30, 0.55, 1.0), ("l", GIVE_T + 0.30, GIVE_T + 0.60, 0.0),
              ("r", GIVE_T + 0.60, GIVE_T + 0.90, 0.0)]
# (each foot's step: from t to t, ending stepped in (1) or back (0))


def give_step(t):
    """How far in he is (his pelvis: 0 where he waits, 1 where he gives), and each foot's (how far in, its lift)."""
    feet = {}
    for s in ("l", "r"):
        at, lift = 0.0, 0.0
        for f, t0, t1, to in GIVE_STEPS:
            if f != s:
                continue
            if t >= t1:
                at = to
            elif t > t0:
                u = (t - t0) / (t1 - t0)
                at, lift = at + (to - at) * sm(u), 0.05 * math.sin(math.pi * u)
                break
            else:
                break
        feet[s] = (at, lift)
    k = ramp(t, 0.05, 0.55) * (1.0 - ramp(t, GIVE_T + 0.30, GIVE_T + 0.90))
    return k, feet


GIVE_DIP = (0.3, -0.08)            # (the round's way in: how far along, and how low under the give, at 0.325 s)


def carrier_give_round(t):
    cb, cd = carry_round()
    gb, gd = CARRIER_GIVE
    gb = gb + np.array((STEP_IN, 0.0, 0.0))
    m = cb + GIVE_DIP[0] * (gb - cb)
    m[2] = gb[2] + GIVE_DIP[1]         # (in under the loader's left arm as it comes back from the breech)
    v = held([(0.0, tuple(cb) + tuple(cd)), (0.325, tuple(m) + tuple(_n(cd + gd))), (0.65, tuple(gb) + tuple(gd)),
              (GIVE_T, tuple(gb) + tuple(gd)), (GIVE_T + 0.35, tuple(gb + (0.0, 0.0, -0.05)) + tuple(gd))], t)
    return v[:3], _n(v[3:]), (1.0 if t < GIVE_T else 0.0)


GIVE_TRUNK = [(0.0, (0.0, 4.0, 0.0)), (0.325, (0.0, 22.0, 0.08)), (0.65, (0.0, 20.0, 0.06)), (GIVE_T, (0.0, 20.0, 0.06)),
              (GIVE_T + 0.85, (0.0, 6.0, 0.02)), (GIVE_T + 1.35, (0.0, 2.0, 0.0))]


@water_clip("ammo_carrier_give", CG_LONG, root="deck",
            events=[sev(GIVE_T, "round_given", None)] +
                   [sev(t1, "footstep", "footstep", bone="foot_" + f) for f, t0, t1, to in GIVE_STEPS],
            about="The carrier gives the round to the loader: from where he waits (ammo_carrier_wait, his root) he steps "
                  "0.45 m in as the loader turns back from the breech, holds the round out across in front of him by "
                  "its shell where the loader takes it, lets it go as the loader takes it, steps back out and lets his "
                  "arms down; it ends where it began, standing empty handed (walk back, or idle). Started when "
                  "gun_loader's loop is at 3.33 s (10/3, after his breech_close), the loop before the one he takes "
                  "it in (so the loader takes it at this clip's 2.42 s, his own 0.75 s); his root where `pair` puts it from the loader's. Its "
                  "round_given event has no sound of its own (the loader's round_taken plays it).")
def c_carrier_give(t):
    turn, lean, crouch = held(GIVE_TRUNK, t)
    k, feet = give_step(t)
    P = carrier_body(turn, lean, crouch, x=STEP_IN * k)
    planted(P, (0.04, 0.13), (-0.02, -0.13), 8.0, -8.0)
    for s in ("l", "r"):
        at, lift = feet[s]
        x, y, z = P["foot_" + s]["at"]
        P["foot_" + s]["at"] = (x + STEP_IN * at, y, z + lift)
    b, d, _ = carrier_give_round(t)
    carrier_hands(P, b, d, free=ramp(t, GIVE_T, GIVE_T + 0.25), rest=ramp(t, GIVE_T + 0.2, GIVE_T + 0.95),
                  rest_x=STEP_IN * k)
    return S.breathe(P, t, CG_LONG, 1.0)


CARRY_CLIPS = ("ammo_carrier_take", "ammo_carrier_walk", "ammo_carrier_wait", "ammo_carrier_give")


GUN_CLIPS = ("hands_up", "gun_layer", "gun_trainer", "gun_loader") + PASS_CLIPS + ("ammo_passer_wait",) + CARRY_CLIPS


def gun_json(data, rig):
    """The gun crew's blocks: the wheel and sight of gun_layer and gun_trainer, the round of gun_loader, the passers
    and the carrier, and their pairs (in the root's frame, the game's axes: y to his right)."""
    g = lambda p: [round(float(p[0]), 4) + 0.0, round(float(-p[1]), 4) + 0.0, round(float(p[2]), 4) + 0.0]  # noqa: E731
    for name, s, long, turn in (("gun_layer", 1.0, LAY_LONG, lay_turn), ("gun_trainer", -1.0, TRAIN_LONG, train_turn)):
        d = data["clips"].get(name)
        if d is None:
            continue
        eyes = []
        for k in range(int(round(long * 15)) + 1):
            st = clip_state(rig, name, k / 15.0, "rifle")
            eyes.append((rig.eye(st, "r") + rig.eye(st, "l")) / 2.0)
        eyes = np.array(eyes)
        d["wheel"] = {"hub": g(wheel_hub(s)), "radius": WHEEL_R, "axis": [1.0, 0.0, 0.0],
                      "turn_fields": ["t", "degrees"],
                      "turn": [[round(k / 15.0, 4), round(turn(k / 15.0), 2)] for k in range(int(round(long * 15)) + 1)],
                      "about": "the handwheel he turns: its hub here, upright across his front (its axis along x, toward "
                               "him), his hands on its rim either side; turn it by `turn` (degrees, + his right hand up)"}
        d["sight"] = {"eye": g(eyes.mean(axis=0)), "moves": round(float(np.linalg.norm(eyes - eyes.mean(axis=0), axis=1).max()), 3),
                      "about": "where his eyes are (the mean between them; they move this little): put the sight's "
                               "eyepiece just ahead of it, along x"}
    ld = data["clips"].get("gun_loader")
    if ld is not None:
        tr = []
        for k in range(int(round(LOAD_LONG * 15)) + 1):
            t = k / 15.0
            b, dd, sh = loader_round(t)
            tr.append([round(t, 4), sh] + g(b) + g(dd))
        ld["round"] = {"length": ROUND_LONG, "radius": ROUND_R, "breech": g(BREECH), "bore": g(_n(BORE)),
                       "track_fields": ["t", "shown", "x", "y", "z", "dx", "dy", "dz"], "track": tr,
                       "about": "the round he loads: its base's middle (x, y, z) and its nose's way (dx, dy, dz) through "
                                "the loop, drawn while shown (from the take until it is home in the chamber); "
                                "`breech` is the chamber's mouth and `bore` the barrel's line, in the root's frame: put "
                                "the gun's breech there"}
    def track(fn, long):
        tr = []
        for k in range(int(round(long * 15)) + 1):
            t = k / 15.0
            r = fn(t)
            b, dd = r[0], r[1]
            sh = r[2] if len(r) > 2 else 1.0
            tr.append([round(t, 4), sh] + g(b) + g(dd))
        return tr
    yaw = lambda turn: round(-float(turn), 1) + 0.0  # noqa: E731  (a turn to his left here; the game's yaw, + to the right)
    for name in PASS_CLIPS:
        ps = data["clips"].get(name)
        if ps is None:
            continue
        V = PASSERS[name]
        c_, s_ = math.cos(math.radians(V["loader_turn"])), math.sin(math.radians(V["loader_turn"]))
        q = -np.asarray(V["loader_root"], float)
        hat = np.array((c_ * q[0] + s_ * q[1], -s_ * q[0] + c_ * q[1], 0.0))      # (the hatch's middle, the loader's frame)
        fromtake = hat - np.array((TAKE_AT[0], TAKE_AT[1], 0.0))
        bearing = round(-math.degrees(math.atan2(fromtake[1], fromtake[0])), 1) + 0.0     # (the game's: + to his right)
        within = 15 if V["reach"] < 0.825 else 30
        ps["round"] = {"length": ROUND_LONG, "radius": ROUND_R, "track_fields": ["t", "shown", "x", "y", "z", "dx", "dy", "dz"],
                       "track": track(lambda t, name=name: passer_round(t, name), PASS_LONG),
                       "about": "the round he passes up, as gun_loader's (shown from when he lifts it from below until "
                                "the loader takes it)"}
        ps["pair"] = {"loader_root": g(V["loader_root"]), "loader_yaw": yaw(V["loader_turn"]),
                      "same_facing": abs(V["loader_turn"]) < 0.5, "same_time": True, "reach": round(V["reach"], 3),
                      "rung": V["rung"], "take_t": TAKE_T, "give_z": round(float(V["give"][2]), 3),
                      "hatch_at": g(hat), "hatch_bearing": bearing, "turn_within": within,
                      "about": "the loader's root from his and the loader's yaw from his (the game's axes and yaw, + to "
                               "the right), both clips played from the same moment: the round then passes from his hands "
                               "to the loader's at t = %.2f s, its base give_z over his root as over the loader's. `reach` "
                               "is from his root to the loader's take (the round's base then), level; `hatch_at` is the "
                               "hatch's middle from the loader's root (the loader's frame, the game's axes) and "
                               "`hatch_bearing` its bearing from the loader's take, degrees from the way he faces, + to his "
                               "right. Pick the clip whose reach is nearest the hatch's (they are 0.05 m apart from 0.70 "
                               "to 1.10 m, so within 0.025 m) and turn him on his root by the difference in bearing, so "
                               "the round's base meets the loader's take. Checked on the rig at this bearing and turned "
                               "15 and 30 degrees either way: up to `turn_within` degrees the men keep apart and the "
                               "loader's feet off the opening, though the loader's forearms meet the round up to 3 cm "
                               "deeper as his hands come round to it (and the nearer passers' arm can touch the loader's "
                               "thigh by up to 4 cm as they let go); farther round, his feet come over the opening or "
                               "the men meet: use the carrier. Turned, the round's way in his hands is not the loader's "
                               "at the take: ease it over to the loader's track's way through the 0.2 s after the take. "
                               "Where the hatch's deck stands higher than the loader's (dz), put his root dz lower (he "
                               "stands that much lower on the ladder; checked clear of the hatch's sides to 0.125 m): "
                               "with the loader only, not with the carrier" % TAKE_T}
    pw = data["clips"].get("ammo_passer_wait")
    if pw is not None:
        pw["round"] = {"length": ROUND_LONG, "radius": ROUND_R, "track_fields": ["t", "shown", "x", "y", "z", "dx", "dy", "dz"],
                       "track": track(passer_wait_round, PW_LONG),
                       "about": "the round he holds up ready, as ammo_passer's at its start"}
    cr = data["clips"].get("ammo_carrier_take")
    if cr is not None:
        cr["round"] = {"length": ROUND_LONG, "radius": ROUND_R, "track_fields": ["t", "shown", "x", "y", "z", "dx", "dy", "dz"],
                       "track": track(carrier_take_round, CT_LONG),
                       "about": "the round he takes: shown from the take (the passer holds it before)"}
        cr["pair"] = {"with": "ammo_passer", "carrier_root": g(CARRIER_TAKE_AT), "carrier_yaw": yaw(CARRIER_TAKE_YAW),
                      "same_time": True, "take_t": TAKE_T,
                      "about": "his root and yaw from ammo_passer's (the passer's frame, the game's axes and yaw, + to the "
                               "right: he faces across the passer's front, at its left), both roots on the hatch's deck "
                               "(do not lower the passer by the hatch's height over the loader's deck here: that is for "
                               "his pair with the loader): play this clip from the moment ammo_passer's loop starts and "
                               "he takes the round from the passer's hands at t = %.2f s" % TAKE_T}
    for name, fn, long in (("ammo_carrier_walk", carrier_walk_round, CARRY_GAIT.long), ("ammo_carrier_wait", carrier_wait_round, WAIT_LONG)):
        cw = data["clips"].get(name)
        if cw is not None:
            cw["round"] = {"length": ROUND_LONG, "radius": ROUND_R, "track_fields": ["t", "shown", "x", "y", "z", "dx", "dy", "dz"],
                           "track": track(fn, long), "about": "the round he carries, across his front"}
    cg = data["clips"].get("ammo_carrier_give")
    if cg is not None:
        gb, gd = CARRIER_GIVE
        gb = gb + np.array((STEP_IN, 0.0, 0.0))
        root, turn = taker(gb, gd)                 # (the loader's root and turn from his clip's root, where he waits)
        c, s_ = math.cos(math.radians(-turn)), math.sin(math.radians(-turn))
        back = np.array((-(c * root[0] - s_ * root[1]), -(s_ * root[0] + c * root[1]), 0.0))
        cg["round"] = {"length": ROUND_LONG, "radius": ROUND_R, "track_fields": ["t", "shown", "x", "y", "z", "dx", "dy", "dz"],
                       "track": track(carrier_give_round, CG_LONG),
                       "about": "the round he gives: shown until the loader takes it"}
        cg["pair"] = {"with": "gun_loader", "carrier_root": g(back), "carrier_yaw": yaw(-turn), "loader_root": g(root),
                      "loader_yaw": yaw(turn), "same_time": False, "start_at_loader_t": GIVE_LEAD, "take_t": GIVE_T,
                      "loader_take_t": TAKE_T, "step_in": STEP_IN, "reach": round(float(np.hypot(gb[0], gb[1])), 3),
                      "about": "`carrier_root` and `carrier_yaw`: his root and yaw from the loader's (the loader's frame, "
                               "the game's axes and yaw); `loader_root` and `loader_yaw` the other way. His root is where "
                               "he waits (ammo_carrier_wait there, clear of the loader's loop) and where the clip ends: he "
                               "steps step_in metres forward to give and back out. Start this clip when gun_loader's loop "
                               "is at start_at_loader_t (%.2f s), the loop before the take: the round is where the loader "
                               "takes it before his hands come round to it, and passes to him at this clip's take_t (%.2f "
                               "s), the loader's loader_take_t (%.2f s) in his next loop. For the gun's first round, start "
                               "the loader at his start_at_loader_t with it (empty handed, turning back from the breech)"
                               % (GIVE_LEAD, GIVE_T, TAKE_T)}
    return data


# ================================================================================================ the files
NAME = {"usa": "American sailor (soldier_USA_sailor)", "jap": "Japanese sailor (soldier_JAP_sailor)",
        "uk": "British sailor (soldier_UK_sailor)", "ger": "German sailor (soldier_GER_sailor)",
        "ita": "Italian sailor (soldier_ITA_sailor)", "sov": "Soviet sailor (soldier_SOV_sailor)"}[WHO]
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
           "cover of the summer and the Mediterranean, the oatmeal sea jersey of the Atlantic escorts, and the gun "
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
    "ita": (" The Italian sailor: soldier.py's man (the same 32 bones) as a rating of the Navy at his action station, "
            "with no badge or insignia anywhere: the faded blue-grey cotton working dress, jumper and trousers, black "
            "leather shoes, the dark blue round cap, and a life jacket of cork blocks sewn into cream canvas, tied over "
            "the jumper. He carries no weapon (every clip's weapon is none). His colours are the file's own paints "
            "(new_paints, colours: kapok = the cork jacket, shirt_own = the jumper, trousers_own, hat = the cap, ties = "
            "the jacket's ties), not an army's; colour_variants gives the white summer working dress, the dark blue "
            "winter dress, and the gun crew's steel helmet. Clips on deck from the motion capture; over the side and in "
            "the water keyed on his rig, the root at the sea's surface (water block)."),
    "sov": (" The Soviet sailor: soldier.py's man (the same 32 bones) as a rating of the Navy at his action station, "
            "with no badge or insignia anywhere: the dark blue flannel jumper and black trousers, black leather shoes, "
            "the peakless round cap with its two black ribbons hanging behind, and a kapok bib in khaki canvas (a pad "
            "over his chest and one over his back, joined over the shoulders and round the neck, open at his sides). "
            "He carries no weapon (every clip's weapon is none). His colours are the file's own paints (new_paints, "
            "colours: kapok = the bib, shirt_own = the jumper, trousers_own, hat = the cap and its ribbons, ties = the "
            "bib's ties), not an army's; colour_variants gives the white summer jumper, the black pea coat of the cold "
            "seas, and the gun crew's steel helmet. Clips on deck from the motion capture; over the side and in the "
            "water keyed on his rig, the root at the sea's surface (water block)."),
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
    "hatch": "the root on the deck at the hatch's middle (he stands on a rung under it), facing the clip's forward",
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
    gun_json(data, rig)
    data["water"] = {
        "sea_level": "z = 0 in each water clip's root frame is the sea's surface (root `surface`)",
        "roots": ROOTS,
        "order": "a man going over the side: run (or walk) to the rail, jump_feet_first (the 2 m, 4 m or 8 m one "
                 "nearest the deck's height over the water; for any other height, fall_loop while the game drops "
                 "him, then enter_water at the surface), then tread (or swim_breast, wave, cling); a body: "
                 "float_dead. Picked up: climb_net up the side, or into a boat (row, sit_raft)",
        "events": "splash (splash_body: his feet into the water; height is the drop), surface (surface_gasp: his "
                  "head out again), water_stroke and water_kick (swim_breast), net_grab (climb_net, the hand), "
                  "oar_catch (row: the blade in), round_taken, ram and breech_close (gun_loader), round_taken "
                  "(ammo_carrier_take, from the passer), round_given (ammo_carrier_give: no sound of its own, the "
                  "loader's round_taken plays it), round_up (the ammo_passer clips), footstep (a passer's feet on the "
                  "hatch's rungs, the carrier's on deck); the deck clips' footsteps as the soldiers'",
        "gun_crew": GUN_CREW,
        "no_weapon": "he has no weapon: the file has no weapon triangles; the weapon bone rides 2 m under his root "
                     "in the water clips",
    }
    return data


GUN_CREW = ("on deck, keyed on his rig (the root on the deck under his feet): hands_up (surrendering, hands raised open "
            "above the head, a slow sway); a deck gun's crew: gun_layer (the gun's left, the elevating handwheel and the "
            "sight), gun_trainer (its right, the training handwheel, mirrored), gun_loader (takes a round from the "
            "passer or the carrier on his left, lays it in the tray and rams it, about one every 5 s), the passers "
            "ammo_passer (behind him on his left, 0.32 m from the loader's take, on the rung 0.9 m down, holding the "
            "round by its base) and ammo_passer_50 (0.5 m, the same grip, a rung up to give and down to take the "
            "next), ammo_passer_70, _75 ... _110 (0.70 to 1.10 m every 0.05 m, holding the round out across in front "
            "by its shell: to 0.80 m on the rung 0.9 m down, from 0.85 m a rung up to give and down to take the next), "
            "each in a hatch, the root at its middle: they hand the rounds up in step with the loader, `pair` giving "
            "the loader's root and yaw from theirs, their `reach` and where the hatch lies from the loader "
            "(`hatch_at`, `hatch_bearing`): pick the nearest reach and turn him on his root to meet the take (the "
            "round's way at the take turns with him); where the hatch's deck stands dz higher than the loader's, put "
            "his root dz lower (with the loader only). The next round shows in his hands at round_up, low in front "
            "of him, as if handed up past him. ammo_passer_wait (ammo_passer holding one ready while the carrier is "
            "away, a 5 s loop); and for a hatch farther off, the carrier, who holds the round across his front, its "
            "nose to his left: ammo_carrier_take (at the hatch, from ammo_passer, from that loop's start, reaching in "
            "empty handed, standing ahead of the passer beside the round's middle, facing across it, both on the "
            "hatch's deck), ammo_carrier_walk (along the casing, the game moving his root, routed clear of the loader "
            "and the gun), ammo_carrier_wait (on the loader's left, where he waits, clear of the loader's loop), "
            "ammo_carrier_give (started at the loader's 3.33 s, after his breech_close, the loop before his take: "
            "steps in 0.45 m, holds it where the loader takes it at his 0.75 s, steps back; 3.82 s long, ending at "
            "the loader's 2.15 s), each with its `round` and `pair`. A carrier's round trip (give, walk back, take, "
            "walk to wait) is 6.0 + D / 1.25 + D / 0.92 s at the least, D the walk (m) from where he takes to where "
            "he waits: with ammo_passer's loop started at the loader's (2.15 + D / 1.25) s, N = ceil(round trip / 5) "
            "carriers in turn keep the loader's 5 s (D to 2.1 m: N = 2; to 4.7 m: 3). With two or more, the next "
            "waits with his round about 0.8 m back along his way in, clear of the one giving, and walks in when that "
            "one's give ends (the loader's 2.15 s), by the loader's 3.33 s. Fewer carriers give a round every 5N / "
            "carriers s, the loader waiting at his 3.33 s (the breech shut) until one stands ready. Each clip's "
            "wheel, sight and round blocks say where the gun's parts go in its root's frame")


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
GUN_SHOTS = [
    ("hands_up", "hands_up", 1.0, (0.0, 0.0, 1.05), 4.2, 6.0, 25.0, ("gun",)),
    ("layer", "gun_layer", 1.0, (0.2, 0.0, 1.0), 3.0, 12.0, 55.0, ("gun",)),
    ("trainer", "gun_trainer", 1.2, (0.2, 0.0, 1.0), 3.0, 12.0, 305.0, ("gun",)),
    ("loader_take", "gun_loader", 0.8, (0.3, 0.0, 1.0), 3.6, 12.0, 60.0, ("gun",)),
    ("loader_lay", "gun_loader", 2.6, (0.4, 0.0, 1.0), 3.6, 12.0, 300.0, ("gun",)),
    ("loader_ram", "gun_loader", 3.02, (0.4, 0.0, 1.0), 3.6, 12.0, 300.0, ("gun",)),
    ("passer_give", "ammo_passer", 0.4, (0.15, 0.0, 0.4), 3.2, 14.0, 40.0, ("gun",)),
    ("passer_low", "ammo_passer", 2.45, (0.15, 0.0, 0.1), 3.2, 20.0, 40.0, ("gun",)),
    ("hands_up_back", "hands_up", 3.0, (0.0, 0.0, 1.05), 4.2, 6.0, 200.0, ("gun",)),
    ("passer_50", "ammo_passer_50", 0.4, (0.2, 0.0, 0.45), 3.4, 14.0, 300.0, ("gun",)),
    ("passer_70", "ammo_passer_70", 0.4, (0.3, 0.0, 0.45), 3.4, 14.0, 300.0, ("gun",)),
    ("passer_90", "ammo_passer_90", 0.4, (0.35, 0.0, 0.55), 3.6, 14.0, 300.0, ("gun",)),
    ("passer_110", "ammo_passer_110", 0.4, (0.4, 0.0, 0.55), 3.8, 14.0, 300.0, ("gun",)),
    ("passer_110_climb", "ammo_passer_110", 2.95, (0.15, 0.0, 0.35), 3.4, 18.0, 40.0, ("gun",)),
    ("passer_110_low", "ammo_passer_110", 2.45, (0.15, 0.0, 0.25), 3.2, 20.0, 40.0, ("gun",)),
    ("passer_90_rise", "ammo_passer_90", 3.3, (0.3, -0.1, 0.55), 3.6, 16.0, 40.0, ("gun",)),
    ("passer_wait", "ammo_passer_wait", 2.0, (0.15, 0.0, 0.4), 3.2, 14.0, 40.0, ("gun",)),
    ("carrier_take", "ammo_carrier_take", 0.8, (0.0, 0.1, 0.9), 3.6, 12.0, 60.0, ("gun",)),
    ("carrier_carry", "ammo_carrier_take", 2.2, (0.15, 0.0, 1.0), 3.4, 10.0, 40.0, ("gun",)),
    ("carrier_walk", "ammo_carrier_walk", 0.3, (0.15, 0.0, 1.0), 3.6, 8.0, 60.0, ("gun",)),
    ("carrier_wait", "ammo_carrier_wait", 1.0, (0.15, 0.0, 1.0), 3.4, 8.0, 320.0, ("gun",)),
    ("carrier_give", "ammo_carrier_give", 0.7, (0.6, 0.1, 1.0), 3.6, 12.0, 320.0, ("gun",)),
    ("carrier_done", "ammo_carrier_give", 3.8, (0.15, 0.0, 1.0), 3.6, 8.0, 320.0, ("gun",)),
]
GUN_LABELS = {"hands_up": "hands_up: surrendering, a slow sway", "layer": "gun_layer: eye at the sight, elevating wheel",
              "trainer": "gun_trainer: the other side, training wheel", "loader_take": "gun_loader: takes the round",
              "loader_lay": "lays it in the tray", "loader_ram": "rams it home (event ram)",
              "passer_give": "ammo_passer: holds it up from the hatch", "passer_low": "stoops for the next",
              "hands_up_back": "hands_up from behind",
              "passer_50": "ammo_passer_50: holds it out, 0.5 m reach", "passer_70": "ammo_passer_70: 0.7 m reach",
              "passer_90": "ammo_passer_90: a rung up, 0.9 m reach", "passer_110": "ammo_passer_110: a rung up, 1.1 m",
              "passer_110_climb": "climbing back up with the next", "passer_110_low": "takes the next low on his right",
              "passer_90_rise": "ammo_passer_90: brings it across",
              "passer_wait": "ammo_passer_wait: one ready, the carrier away",
              "carrier_take": "ammo_carrier_take: at the hatch, by the shell", "carrier_carry": "swung across to carry",
              "carrier_walk": "ammo_carrier_walk: along the casing", "carrier_wait": "ammo_carrier_wait",
              "carrier_give": "ammo_carrier_give: steps in, holds it out", "carrier_done": "stepped back out, arms down"}
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
    "ita": ["Italian sailor: blue-grey working dress, cork life jacket, round cap",
            "From behind: the jacket's cork blocks",
            "Close: the round cap, the jacket's collar"],
    "sov": ["Soviet sailor: dark blue jumper, black trousers, kapok bib, peakless cap",
            "From behind: the bib's back pad, the cap's ribbons",
            "Close: the peakless cap, the bib's collar"],
}[WHO]


def rod(a, b, r, mat, name="rod"):
    """A cylinder from a to b (the pictures' props)."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = b - a
    L = float(np.linalg.norm(d))
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=L, vertices=20, location=tuple((a + b) / 2.0))
    o = bpy.context.object
    o.name = name
    o.rotation_mode = "QUATERNION"
    o.rotation_quaternion = Vector((0.0, 0.0, 1.0)).rotation_difference(Vector(tuple(d / L)))
    o.data.materials.append(mat)
    return o


def slab(c, size, mat, name="slab"):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=tuple(c))
    o = bpy.context.object
    o.name = name
    o.scale = tuple(size)
    o.data.materials.append(mat)
    return o


def gun_props(name, t):
    """The deck, and the gun's parts where the clip's blocks put them (each man in his own root's frame)."""
    steel = kit.material("gunsteel", kit.hex_rgb("5E6469"), rough=0.5)
    deck = kit.material("hull", kit.hex_rgb("6F757A"), rough=0.6)
    brass = kit.material("brass", kit.hex_rgb("B08A3E"), rough=0.35)
    made = []
    hatch, turned = None, False
    if name in PASSERS or name == "ammo_passer_wait":
        hatch = np.zeros(3)
    elif name == "ammo_carrier_take":       # (the passer's hatch in his frame: he is turned 90 degrees from the passer)
        c_, s_ = math.cos(math.radians(-CARRIER_TAKE_YAW)), math.sin(math.radians(-CARRIER_TAKE_YAW))
        q = -np.asarray(CARRIER_TAKE_AT, float)
        hatch, turned = np.array((c_ * q[0] - s_ * q[1], s_ * q[0] + c_ * q[1], 0.0)), True
    if hatch is not None:
        for c, sz in (((0.0, 0.95, -0.03), (2.6, 1.3, 0.06)), ((0.0, -0.95, -0.03), (2.6, 1.3, 0.06)),
                      ((0.95, 0.0, -0.03), (1.3, 0.6, 0.06)), ((-0.95, 0.0, -0.03), (1.3, 0.6, 0.06))):
            if turned:
                c, sz = (-c[1], c[0], c[2]), (sz[1], sz[0], sz[2])
            made.append(slab(hatch + np.array(c), sz, deck, "deck"))
        if name in PASSERS:
            b, d, sh = passer_round(t, name)
        elif name == "ammo_passer_wait":
            b, d, sh = passer_wait_round(t)
        else:
            b, d, sh = carrier_take_round(t)
    elif name in CARRY_CLIPS:
        made.append(slab((0.3, 0.0, -0.03), (3.0, 2.4, 0.06), deck, "deck"))
        r = {"ammo_carrier_walk": carrier_walk_round, "ammo_carrier_wait": carrier_wait_round,
             "ammo_carrier_give": carrier_give_round}[name](t)
        b, d, sh = r[0], r[1], (r[2] if len(r) > 2 else 1.0)
    else:
        made.append(slab((0.3, 0.0, -0.03), (3.0, 2.4, 0.06), deck, "deck"))
        sh = 0.0
    if name in ("gun_layer", "gun_trainer"):
        sd = 1.0 if name == "gun_layer" else -1.0
        hub = wheel_hub(sd)
        bpy.ops.mesh.primitive_torus_add(major_radius=WHEEL_R, minor_radius=0.009, location=tuple(hub), rotation=(0.0, math.radians(90), 0.0))
        w = bpy.context.object
        w.data.materials.append(steel)
        made.append(w)
        made.append(rod(hub, hub + (0.06, 0.0, 0.0), 0.012, steel, "shaft"))
        made.append(slab((0.55, -sd * 0.38, 1.05), (1.2, 0.36, 0.5), steel, "gun"))
        made.append(rod(hub + (0.06, 0.0, 0.0), (hub[0] + 0.06, -sd * 0.2, hub[2]), 0.012, steel, "shaft"))
        made.append(rod((0.3, -sd * 0.38, 1.36), (2.6, -sd * 0.38, 1.36), 0.055, steel, "barrel"))
    if name == "gun_loader":
        d_b = _n(BORE)
        B = np.array(BREECH)
        made.append(slab(B + 0.2 * d_b, (0.4, 0.34, 0.34), steel, "breech"))
        made.append(rod(B + 0.4 * d_b, B + 2.8 * d_b, 0.06, steel, "barrel"))
        tray = B - ROUND_LONG * d_b
        made.append(rod(tray - (0.0, 0.0, 0.06), B - (0.0, 0.0, 0.06), 0.03, steel, "tray"))
        b, d, sh = loader_round(t)
    if sh > 0.5:
        made.append(rod(b, b + (ROUND_LONG - 0.28) * d, ROUND_R, brass, "case"))
        made.append(rod(b + (ROUND_LONG - 0.28) * d, b + ROUND_LONG * d, ROUND_R * 0.82, steel, "shell"))
    return made


def _sets(kind, clip=None, t=0.0):
    made = []
    if not kind:
        return made
    if kind[0] == "gun":
        return gun_props(clip, t)
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
        made = _sets(kind, name, t)
        bd.location.z = -6.0 if kind and kind[0] in ("sea", "spar", "deck") else (-0.4 if kind and kind[0] == "seat" else
                                                                               (-1.6 if name in PASSERS or name == "ammo_passer_wait" else
                                                                                (-0.1 if kind and kind[0] == "gun" else 0.0)))
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


SHARED = "/mnt/project-files/battle-models/land/soldier/"
BACKUP = "/home/user/art/sailors/backup/"
GUN_ABOUT_WAS = (" Also keyed on his rig, on deck: hands_up (surrendering) and a deck gun's crew, gun_layer, gun_trainer, "
                 "gun_loader and ammo_passer (water: gun_crew).")
GUN_ABOUT_OLD = (GUN_ABOUT_WAS, " Also keyed on his rig, on deck: hands_up (surrendering) and a deck gun's crew, gun_layer, "
                 "gun_trainer, gun_loader, the passers (ammo_passer and ammo_passer_50 to _110) and the carrier "
                 "(ammo_carrier_take, _walk, _wait and _give) (water: gun_crew).")
GUN_ABOUT = (" Also keyed on his rig, on deck: hands_up (surrendering) and a deck gun's crew, gun_layer, gun_trainer, "
             "gun_loader, the passers (ammo_passer, ammo_passer_50, ammo_passer_70 to _110 every 0.05 m, "
             "ammo_passer_wait) and the carrier (ammo_carrier_take, _walk, _wait and _give) (water: gun_crew).")


def graft(info, folder=SHARED):
    """The new deck clips (GUN_CLIPS) made on his rig and added to the delivered file, nothing else in it touched but
    those clips, the gun crew's lines of its about (and the British jersey's word there) and of its water block (its
    hatch root, events and gun_crew), read fresh, backed up, written whole."""
    import shutil
    t0 = time.time()
    kit.fresh(S.FPS)
    m = assemble()
    rig = m["rig"]
    keep = dict(S.CLIPS)
    S.CLIPS.clear()
    S.CLIPS.update({k: keep[k] for k in GUN_CLIPS})
    try:
        new = S.clips_json(rig)
    finally:
        S.CLIPS.clear()
        S.CLIPS.update(keep)
    mini = water_json({"clips": new}, rig)
    path = os.path.join(folder, f"{BASE}.json")
    D = json.load(open(path))
    worst = 0.0
    for b in D["bones"]:
        h = rig.H0.get(b["name"])
        if h is not None:
            worst = max(worst, float(np.linalg.norm(np.array(b["head"]) - np.array((h[0], -h[1], h[2])))))
    if worst > 0.002:
        raise SystemExit(f"{BASE}: his rig is not the file's (a bone {worst * 1000:.1f} mm off): export him whole instead")
    os.makedirs(BACKUP, exist_ok=True)
    shutil.copy2(path, os.path.join(BACKUP, f"{BASE}.json.{time.strftime('%m%d_%H%M%S')}"))
    before = set(D["clips"])
    for k in GUN_CLIPS:
        D["clips"][k] = mini["clips"][k]
    D["water"]["roots"]["hatch"] = ROOTS["hatch"]
    D["water"]["events"] = mini["water"]["events"]
    D["water"]["gun_crew"] = GUN_CREW
    for was in GUN_ABOUT_OLD:
        D["about"] = D["about"].replace(was, "")
    D["about"] = D["about"].replace("the cream sea jersey of the Atlantic escorts", "the oatmeal sea jersey of the Atlantic escorts")
    if GUN_ABOUT not in D["about"]:
        D["about"] += GUN_ABOUT
    text = json.dumps(D, separators=(",", ":"), allow_nan=False)      # (a NaN anywhere stops it here, the file untouched)
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        f.write(text)
    os.replace(tmp, path)
    time.sleep(10)
    D2 = json.load(open(path))
    lost = sorted(before - set(D2["clips"]))
    missing = [k for k in GUN_CLIPS if k not in D2["clips"]]
    if lost or missing:
        raise SystemExit(f"{BASE}: clips lost {lost}, not written {missing}")
    info["graft"] = {"file": path, "rig_off_mm": round(worst * 1000, 2), "clips": sorted(D2["clips"]),
                     "new": {k: [new[k]["long"], len(new[k]["tracks"])] for k in GUN_CLIPS}, "s": round(time.time() - t0)}
    print(f"  {BASE}: {len(GUN_CLIPS)} clips added ({len(D2['clips'])} in all; the rig {worst * 1000:.2f} mm off the file's)", flush=True)


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
        elif step == "gun":
            want = os.environ.get("GUN_SHOTS")
            shots = [x for x in GUN_SHOTS if not want or x[0] in want.split(",")]
            picture(info, fast, shots=shots, water=True, labels=[GUN_LABELS[x[0]] for x in shots],
                    title=f"{NAME}: hands up, and a deck gun's crew (the gun drawn where the clips' blocks put it)",
                    out_name=(os.path.join(S.SCRATCH, "look", "gun.png") if fast else S.stage_path("pictures", f"{BASE}_gun.png")))
        elif step == "graft":
            graft(info)
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
