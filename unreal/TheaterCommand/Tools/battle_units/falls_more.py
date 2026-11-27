"""Seven more deaths for men shot lying down or in cover, and where every death was struck (`hit`), for the effects
team's puff of dust and cloth. Numpy only (PIL for the pictures); everything is done on the soldier JSON files.

Why (the effects team's measure of 25 November, battle-fx/hits/falls-today.md): 52 to 69 percent of bullet deaths are
men lying or in cover; lying has four falls and cover five; die_cover_a and _b open alike for 0.33 s; the lying falls
barely move in their first 0.3 s ("he just drops"). So seven more, each opening its own way and each REACTING in its
first 0.3 s, then ending flat and still as the others do:

    die_prone_d   struck in the left side: he rolls half onto his right side, then sinks back
    die_prone_e   he kicks and arches (the legs kick, the back arches off the ground), then goes limp
    die_prone_f   a hand goes to the wound under him (he half-lifts to get it there), then he is still
    die_prone_g   he pushes up slowly on his arms, fails, and sinks down onto his face
    die_cover_d   he slides down where he knelt: onto his heel, a shoulder dragging down, then tips over
    die_cover_e   thrown back off his cover: a hard blow to the chest from in front, over backwards, knees bent
    die_cover_f   slumps forward over his cover: folds forward and down (the chest's forward travel kept short:
                  the clip cannot know the wall, bank or sandbags in front of him)

They are ragdoll falls made as falls.py makes the other 39 (the same Ragdoll and Fall of ragdoll.py: blows, tone, the
ground, the rifle let go and landing where it lands), so they match them in weight and look. Where he DOES something
(kicks, arches, reaches for the wound, pushes up) the same joint drive that holds his pose (the `tone`) is steered
toward keyed joint poses with a wounded man's strength, as falls.py's crawl_death drives him toward the crawl
(DRIVE below), and then let go. What is added to falls.py's way, here only:
  - the first key IS the named start pose, every bone (the ragdoll starts inside its joints' ranges and without the
    collar bones' turns, a little off the pose; that offset is eased out over the first EASE seconds), so the game's
    0.12 s blend into the clip has no arm snapping to the ragdoll's start (the radio man's whip too: falls.py drapes
    it after the physics, and ease_chain eases that drape out of his start pose's own whip the same way);
  - events only for what really comes down (a knee already on the ground at the hit is not a knee_down);
  - the fall runs until he is still (no cut at a fixed length while he is still moving); once his act is over and
    his trunk lies slow, the parts lying on the ground get a stronger soil-and-cloth damping (see Touch), so an arm
    pressed under him does not go on pushing him along for seconds, and a second later the whole of him; and in the
    last REST_END of the longest time a fall may run, whatever still rocks (a leg folded under him, caught between
    his weight and the ground) is calmed and each joint held as it then is, so every clip ends still;
  - a blow can roll a lying man (a recipe's `roll`: see roll_him);
  - the reaction test does not count the ragdoll's own lift of the man clear of the ground at its start;
  - a man the recipe leaves outside the checks is made again with its tries (nudged blows; die_cover_f also leans
    him the other way), and the first try inside them is kept, else the nearest, its faults reported;
  - die_prone_d and die_cover_f must end on his front (`face_down`: "sinks back", "slumps forward"), a check and a
    retry if not; where every clip ends (front down, roll) is in the checks of all seven (die_prone_f may end half on
    his side, curled over the drawn-up knee: said in its checks, not required).
Speed, not behaviour: np.cross on two 3-vectors is replaced, for the ragdoll's run, by the same three products and
differences without numpy's axis shuffling (the same bits: checked on a whole fall, every frame equal), and --build
keeps one pool over every (file, fall) so no worker waits at a file's end.

falls.py and ragdoll.py live on branch claude/project-thread-o6cg2k (Tools/battle_units/) and are NOT changed: they
are imported from a folder (env FALLS_DIR, default /home/user/art/hits/falls_src/). rom.py (the range-of-motion
checker, with mocap.py and mocap_cmu/rest_ref.json beside it, as on branch claude/project-thread-8egity) is imported
from env ROM_DIR (default /home/user/art/hits/rom_src/, else FALLS_DIR); without it the range-of-motion check is
reported as not run.

THE `hit` FIELD, on every die_* clip of a file (the 39 of falls.py, the three older takes, the riders' seat slumps and
the seven here): {"bone": the bone struck, "side": "front" | "back" | "left" | "right" (the face of his body struck, in
his own axes at the clip's first key), "at": [x, y, z] (a point of that bone's own surface on that side, in the file's
rest-pose axes, so it rides on `bone` by the file's skinning rule), "push": [x, y, z] (unit, his own axes at the first
key: the way the blow drove him), "from": the kind word}; a thrown death also "blast": true, a seat slump "seat": true.
Taken from each death's own recipe in falls.py (the first blow's part and change of speed; a mirrored death has y
turned; die_leg_*: the leg taken from under him; thrown: the blast on his chest; crawl: its fall's first blow); for
the seven here from the recipe's `hit`, the round's own blow, level or from above (the heave or jerk up that goes
with it in `push` is the man's own: a round never comes from under the ground). The
struck face is opposite the push (push toward -x: front; +x: back; +y, his right: left; -y: right; the larger of x and
y decides; a blow straight up counts as front). `at`: the bone's own near-mesh points (rest pose), the cap of them
within HIT_CAP of the farthest along minus push (turned into the bone's rest frame by its turn at the first key), and
of that cap the point nearest the line through the bone's middle: the surface the round met, not a kit's far corner.

    python3 falls_more.py --build   [--files GER,USA,...] [--src DIR] [--stage DIR] [--workers N] [--only names]
        one bundle a file, DIR/soldier_<N>.falls_more.json = {"clips": {the seven}, "deaths_add": {stance: {kind:
        [names]}}, "hit": {clip: block, for every die_* clip}, "about": ..., "checks": {...}}
    python3 falls_more.py --apply   --src DIR --out DIR2 [--files ...] [--stage DIR]
        whole soldier files with the bundle merged in (for checking and pictures)
    python3 falls_more.py --check   [--files ...] [--src DIR] [--stage DIR]
        the checks again on what is staged, read back from the baked keys; the opening tables
    python3 falls_more.py --pictures [--files GER,USA,JAP] [--src DIR] [--stage DIR] [--pics DIR]
    python3 falls_more.py --deliver [--content] [--files ...] [--stage DIR]
        (NOT RUN in this pass) merges the staged bundles into the shared soldier files and, with --content, into the
        game's Content copies where they exist: each target read again just before its write, backed up first,
        written through a temporary file and os.replace, read back.

Defaults: src /home/user/art/hits/src/, stage /home/user/art/hits/stage/, pictures /home/user/art/hits/pics/falls/.
The output is the same bytes every run (the physics has no dice; the arm solver's draws are seeded by the clip's
name); a file that already has the seven has them replaced. Metres; x forward, y right, z up.
"""

import argparse
import json
import math
import os
import shutil
import sys
import time
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
FALLS_DIR = os.environ.get("FALLS_DIR", "/home/user/art/hits/falls_src/")
ROM_DIR = os.environ.get("ROM_DIR", "/home/user/art/hits/rom_src/")
for _d in (FALLS_DIR, ROM_DIR):
    if os.path.isdir(_d) and _d not in sys.path:
        sys.path.insert(0, _d)
import ragdoll as RG  # noqa: E402
import falls as F  # noqa: E402

_np_cross = np.cross


def _cross3(a, b, *args, **kw):
    """np.cross for two float 3-vectors without its axis shuffling (the ragdoll calls it about 150 times a step, and it
    was 40% of the run). The same three products and differences in the same order as numpy's, so the same bits;
    anything else goes to numpy's own."""
    if not args and not kw:
        a_, b_ = np.asarray(a), np.asarray(b)
        if a_.shape == (3,) and b_.shape == (3,) and a_.dtype == np.float64 and b_.dtype == np.float64:
            a0, a1, a2 = a_.tolist()
            b0, b1, b2 = b_.tolist()
            return np.array((a1 * b2 - a2 * b1, a2 * b0 - a0 * b2, a0 * b1 - a1 * b0))
    return _np_cross(a, b, *args, **kw)


np.cross = _cross3

NAMES = ["CHN", "FRA", "GER", "GER_detail", "ITA", "ITA_desert", "JAP", "POL", "SOV", "SOV_snow", "SOV_winter", "UK",
         "USA", "USA_bar", "USA_bazooka", "USA_loader", "USA_medic", "USA_nco", "USA_para", "USA_radio"]
SRC = "/home/user/art/hits/src/"
STAGE = "/home/user/art/hits/stage/"
PICS = "/home/user/art/hits/pics/falls/"
BACKUP = "/home/user/art/hits/backup/"
SHARED = "/mnt/project-files/battle-models/land/soldier/"
CONTENT = "/home/user/theater-command/unreal/TheaterCommand/Content/Models/Soldiers/"
SET = "falls_more"
TOOL = "falls_more.py"

SEVEN = ["die_prone_d", "die_prone_e", "die_prone_f", "die_prone_g", "die_cover_d", "die_cover_e", "die_cover_f"]
# deaths[stance][kind] gains these, after the lists the file has (the game's picker takes them with no code change)
DEATHS_ADD = {
    "prone": {"back": ["die_prone_d", "die_prone_e"], "forward": ["die_prone_g"], "side": ["die_prone_d"],
              "knees": ["die_prone_f", "die_prone_g"], "spin": ["die_prone_d", "die_prone_e"],
              "thrown": ["die_prone_e"], "gut": ["die_prone_f"], "leg": ["die_prone_e"]},
    "cover": {"back": ["die_cover_e"], "forward": ["die_cover_f"], "side": ["die_cover_d"],
              "knees": ["die_cover_d", "die_cover_f"], "spin": ["die_cover_d"], "thrown": ["die_cover_e"],
              "gut": ["die_cover_d"], "leg": ["die_cover_d"], "head": ["die_cover_f"]},
}

EASE = 0.2                          # s: the ragdoll's start eased out of the named start pose over this
LIE_MAX = F.LIE_MAX                 # m: his chest bone's head under this at the end, he lies (falls.py's)
HEAD_MAX = 0.36                     # m: and his head bone's head under this (the existing falls' ends: 0.33 at most)
STILL_CM, STILL_DEG = 1.0, 3.0      # the last 0.2 s of a clip: no bone's head moves more, no bone turns more
REACT_DEG, REACT_CM = 15.0, 4.0     # the first 0.3 s: head or chest turned this much, or the pelvis moved this far
APART_DEG, APART_CM = 8.0, 5.0      # at 0.3 s: from every other fall of its row by this much of chest or head, or pelvis
OPEN_T = (0.1, 0.2, 0.3)
TRAVEL_MAX = 0.35                   # m: die_cover_f's chest bone, forward of where it started, at most
SETTLE = 0.5                        # s: all but still this long (ragdoll.py's measure), and the fall is over
REST_SLOW, REST_AFTER, REST_DAMP = 0.12, 0.35, 45.0   # m/s, s, 1/s: see Touch (the last of a fall's motion)
REST_LAST = 1.0                     # s: after REST_AFTER, every part damped so (see Touch: a rocking limb)
REST_END = 0.8                      # s: and in the last this of a fall's longest time (see Touch)
CALM_TONE = 0.6                     # (then the joints held as they are, this strong: a wounded man's tone is 0.3-1)
REST_GUN_Z = 0.25                   # m: the rifle's centre under this, once he is at rest, it is held by it too
HIT_CAP = 0.03                      # m: the cap of surface points taken as "farthest along minus push"


# ------------------------------------------------------------------------------------------------ joints, by angle
def joint_turn(j, c):
    """A joint's turn (child against parent) from its own angles c = (flex, side, twist) in degrees, as ragdoll.py's
    clamp_joint builds it (the wrist twists first)."""
    f, s, t = (math.radians(float(v)) for v in c)
    if j.get("twist_first"):
        return RG.axis_angle(j["t"], t) @ RG.from_rotvec(f * j["a1"] + s * j["a2"])
    return RG.from_rotvec(f * j["a1"] + s * j["a2"]) @ RG.axis_angle(j["t"], t)


def joint_angles(j, Rrel):
    """The other way: (flex, side, twist) in degrees of a joint's turn, as clamp_joint measures it."""
    if j.get("twist_first"):
        w, tw = RG.swing_twist(Rrel.T, j["t"])
        w, tw = -w, -tw
    else:
        w, tw = RG.swing_twist(Rrel, j["t"])
    return np.degrees(np.array((float(w @ j["a1"]), float(w @ j["a2"]), tw)))


def in_range(j, c, inside=1.0):
    """Angles put inside the joint's range (a degree inside, so the drive does not fight the clamp)."""
    r = np.degrees(j["rng"])
    return np.array([min(max(float(c[i]), r[i][0] + inside), r[i][1] - inside) for i in range(3)])


def sm(u):
    u = min(1.0, max(0.0, u))
    return u * u * (3.0 - 2.0 * u)


class Ctx:
    """What a recipe's plan can ask of the man as he is hit: each joint's angles in the pose the ragdoll holds
    (`hold`), an arm's angles that put its palm on a point (`reach`), the parts' places."""

    def __init__(self, man, rd, f, name):
        self.man, self.rd, self.f, self.name = man, rd, f, name
        self.j = {j["name"]: j for j in f.joints}
        self.c0 = {n: joint_angles(j, f.hold[j["child"]]) for n, j in self.j.items()}
        self.R0, self.x0 = f.R.copy(), f.x.copy()
        self.H0 = f.heads()

    def hold(self, n):
        return self.c0[n].copy()

    def A(self, n, flex=None, side=None, twist=None):
        """Angles for joint n: the given ones absolute (degrees, the joint's own), the others as he holds them."""
        c = self.hold(n)
        for i, v in enumerate((flex, side, twist)):
            if v is not None:
                c[i] = v
        return in_range(self.j[n], c)

    def D(self, n, flex=0.0, side=0.0, twist=0.0):
        """Angles for joint n: his own, changed by these."""
        return in_range(self.j[n], self.hold(n) + np.array((flex, side, twist), float))

    def way(self, n, i, axis, toward, step=8.0):
        """+1 or -1: the way joint n's angle i (0 flex, 1 side, 2 twist) has to go to turn its part's own `axis` toward
        the world direction `toward`, as he is now (so a plan need not know a joint's signs)."""
        j = self.j[n]
        c = self.hold(n)
        got = []
        for sg in (1.0, -1.0):
            c2 = c.copy()
            c2[i] += sg * step
            R = self.R0[j["parent"]] @ joint_turn(j, c2)
            got.append(float((R @ np.asarray(axis, float)) @ np.asarray(toward, float)))
        return 1.0 if got[0] >= got[1] else -1.0

    def part_point(self, part, p_rest):
        """A rest-pose point riding a part, where it is now."""
        k = self.rd.pix[part]
        return self.x0[k] + self.R0[k] @ (np.asarray(p_rest, float) - self.rd.c0[k])

    def frame(self, part):
        k = self.rd.pix[part]
        return self.R0[k]

    def arm(self, side, cu, cl, ch=None):
        """An arm's parts from its joints' angles on the chest as it is now: (palm point, palm normal, forearm's line,
        elbow, wrist)."""
        rd, j = self.rd, self.j
        kc = rd.pix["chest"]
        ju, jl, jh = j["upperarm_" + side], j["lowerarm_" + side], j["hand_" + side]
        Ru = self.R0[kc] @ joint_turn(ju, cu)
        pu = self.x0[kc] + self.R0[kc] @ ju["rp"]
        xu = pu - Ru @ ju["rc"]
        Rl = Ru @ joint_turn(jl, (cl, 0.0, 0.0))
        pl = xu + Ru @ jl["rp"]
        xl = pl - Rl @ jl["rc"]
        Rh = Rl @ joint_turn(jh, ch if ch is not None else (0.0, 0.0, 0.0))
        ph = xl + Rl @ jh["rp"]
        ax, nn = RG._unit(RG.HAND[side][0]), RG._unit(RG.HAND[side][1])
        return ph + Rh @ (0.075 * ax), Rh @ nn, RG._unit(ph - pl), pl, ph

    def reach(self, side, target, normal=None, fore=None, seed=0, elbow_up=None, w_n=0.02, w_f=0.02, point="palm",
              keep=0.0):
        """The arm's joint angles (upper arm's three, the elbow's bend) inside their ranges that bring the palm (or,
        point="elbow", the elbow) nearest `target` (a point where the man is now; the chest is taken as it is), the
        palm facing `normal` and the forearm along `fore` where given; `keep` holds the angles near the ones he has
        (a weight). A seeded search, so every run finds the same."""
        pt = 3 if point == "elbow" else 0
        c_now = np.concatenate([self.c0["upperarm_" + side], self.c0["lowerarm_" + side][:1]])
        ju, jl = self.j["upperarm_" + side], self.j["lowerarm_" + side]
        ru, rl = np.degrees(ju["rng"]), np.degrees(jl["rng"])
        lo = np.array((ru[0][0], ru[1][0], ru[2][0], rl[0][0])) + 1.5
        hi = np.array((ru[0][1], ru[1][1], ru[2][1], rl[0][1])) - 1.5
        target = np.asarray(target, float)

        def cost(v):
            got = self.arm(side, v[:3], v[3])
            palm, nn, fa, el, wr = got
            c = float(np.sum((got[pt] - target) ** 2))
            if keep:
                c += keep * float(np.sum(np.radians(v - c_now) ** 2))
            if normal is not None:
                c += w_n * (1.0 - float(nn @ normal))
            if fore is not None:
                c += w_f * (1.0 - float(fa @ fore))
            if elbow_up is not None:
                c += 0.02 * max(0.0, elbow_up - float(el[2])) ** 2 * 25.0
            return c

        rs = np.random.RandomState((zlib.crc32((self.name + side).encode()) + seed) % (2 ** 31))
        V = lo + (hi - lo) * rs.rand(1500, 4)
        V[0] = np.clip(c_now, lo, hi)
        C = np.array([cost(v) for v in V])
        best = None
        for i in np.argsort(C)[:4]:
            v, c = V[i].copy(), C[i]
            sg = (hi - lo) * 0.12
            for it in range(160):
                w = np.clip(v + sg * rs.randn(4), lo, hi)
                cw = cost(w)
                if cw < c:
                    v, c = w, cw
                elif it % 8 == 7:
                    sg = sg * 0.8
            if best is None or c < best[1]:
                best = (v, c)
        v = best[0]
        got = self.arm(side, v[:3], v[3])[pt]
        return np.round(v[:3], 2), float(np.round(v[3], 2)), float(np.linalg.norm(got - target))


class Drive:
    """Joint drives: per joint a list of keys (time, angles or None for the pose he was hit in, strength or None for
    the recipe's own fading tone at that time). Before a joint's first key its recipe tone holds his pose, as in every
    fall; between keys the target turns and the strength go smoothly from one to the next; after the last the last
    stays (a plan ends its joints at strength 0: limp)."""

    def __init__(self, f, plan, base):
        self.f = f
        self.tr = {}
        jn = {j["name"]: j for j in f.joints}
        for n, keys in plan.items():
            j = jn[n]
            k = j["child"]
            ks = []
            for (t, c, s) in sorted(keys, key=lambda e: e[0]):
                R = f.hold[k] if c is None else joint_turn(j, in_range(j, c, 0.5))
                ks.append((float(t), R, base[k](float(t)) if s is None else float(s)))
            self.tr[k] = ks
            f.tone[k] = (lambda t, k=k, b=base[k]: self.strength(k, t, b))
        self.n = 0

    def _seg(self, k, t):
        ks = self.tr[k]
        if t <= ks[0][0]:
            return None
        for a, b in zip(ks[:-1], ks[1:]):
            if t <= b[0]:
                return a, b, sm((t - a[0]) / max(b[0] - a[0], 1e-6))
        return ks[-1], ks[-1], 1.0

    def strength(self, k, t, base):
        s = self._seg(k, t)
        if s is None:
            return base(t)
        a, b, u = s
        return a[2] + (b[2] - a[2]) * u

    def step(self, f):
        self.n += 1
        if self.n % 3 != 1:                           # (the targets at 240 a second: they move slowly)
            return
        for k in self.tr:
            s = self._seg(k, f.t)
            if s is None:
                continue
            a, b, u = s
            f.target[k] = a[1] if u >= 1.0 or a is b else a[1] @ RG.frac(a[1].T @ b[1], u)


# ------------------------------------------------------------------------------------------------ the seven
# A recipe as falls.py's (name, stance, kind, start, push, tone, tone_part, release, throw, mu), and:
#   plan     a function of Ctx giving the joint drives (see Drive); none: a plain fall
#   t_min    not ended before this (his act is over by then); t_max: the longest it runs
#   tries    what is changed, in turn, for a body the recipe leaves outside the checks (TRIES unless the recipe has
#            its own; each: {"push": scale} and/or {"later": more blows}, {"later_own": more shoves of his own},
#            {"drive": strength scale}, {"lean": his lean, die_cover_f's})
#   face_down  he must end on his front (the chest's front at least 45 degrees down): a check, and a retry if not
#   hold_r   die_prone_d: his right arm held this strong (in the pose he was hit in) while he rolls onto it
#   sit      die_cover_f, a try's: False leaves out the drive that sits him back onto his heel
#   hit      (part, change of speed) the round itself, for the `hit` block: the first blow's part and its way, level
#            or from above (a lying man's heave up off the ground, or a man's jerk up from a blow, is his own and goes
#            in `push` beside it; the round never comes from under the ground). die_cover_d's first blow is it.
# The kinds (how he FALLS, falls.py's words): back = struck in the front and goes back; forward = struck from behind
# or leaning in and goes onto his face.

def plan_prone_d(c):
    """Struck in the left side: the blow rolls his chest up off it (the roll, the upper body most); his left hand is
    drawn in and planted under that shoulder and pushes hard (to near a straight arm), the back twists that side up
    and the left knee is drawn up and out beside him, so he lies half on his right side over his right elbow a
    moment (the chest turned 40-60 degrees onto that side, about 0.6-1 s); then the arm gives, the back untwists and
    he sinks back onto his front. (The first take of this, a weaker push and twist and the knee barely drawn up,
    turned his chest only 12-14 degrees and fell back at once: it did not read as a roll.)"""
    p = {}
    kc = c.rd.pix["chest"]
    down = c.frame("chest") @ np.array((1.0, 0.0, 0.0))          # (the chest's front: toward the ground, lying)
    out_r = c.frame("chest") @ np.array((0.0, 1.0, 0.0))
    sh = c.x0[kc] + c.R0[kc] @ c.j["upperarm_l"]["rp"]
    far = c.rc.get("push_len", 0.34)
    k = c.rc.get("arm_k", 1.0)                       # (the push's and the twist's strength, a scale)
    cu0, ce0, m0 = c.reach("l", sh + 0.20 * down - 0.06 * out_r, normal=down, fore=down, seed=1)
    cu1, ce1, m1 = c.reach("l", sh + far * down - 0.04 * out_r, normal=down, fore=down, seed=2)
    c.note["plant_miss_cm_l"] = round(m0 * 100.0, 1)
    c.note["push_miss_cm_l"] = round(m1 * 100.0, 1)
    # (the arm does not just go dead under him, a strut: it gives, the hand sliding out forward again as he sinks)
    p["upperarm_l"] = [(0.04, None, None), (0.25, cu0, 2.4 * k), (0.55, cu1, 3.4 * k), (0.85, cu1, 3.0 * k),
                       (1.2, None, 0.8), (1.55, None, 0.0)]
    p["lowerarm_l"] = [(0.04, None, None), (0.25, (ce0, 0.0, 0.0), 2.4 * k), (0.55, (ce1, 0.0, 0.0), 3.4 * k),
                       (0.85, (ce1, 0.0, 0.0), 3.0 * k), (1.2, None, 0.8), (1.55, None, 0.0)]
    p["hand_l"] = [(0.04, None, None), (0.25, (-40.0, 0.0, 0.0), 2.0), (0.85, (-50.0, 0.0, 0.0), 2.0),
                   (1.2, (-20.0, 0.0, 0.0), 0.5), (1.55, (-20.0, 0.0, 0.0), 0.0)]
    tw = c.rc.get("twist", 10.0)
    for n, a in (("spine_01", 1.1 * tw), ("spine_02", tw), ("chest", tw)):
        sg = c.way(n, 2, (0.0, -1.0, 0.0), (0.0, 0.0, 1.0))       # (the twist that lifts his left side)
        p[n] = [(0.02, None, 0.8), (0.3, c.A(n, twist=sg * a), 1.3 * k), (0.85, c.A(n, twist=sg * a), 1.2 * k),
                (1.2, None, 1.4), (1.9, None, 0.6), (2.5, None, 0.0)]
    # (the left knee is drawn up and out beside him as he rolls, propping him on his side, and lets go as he sinks;
    # he ends with it drawn up a little, as it falls)
    kf, ks, kc_, kst = c.rc.get("knee", (25.0, 6.0, 45.0, 0.4))
    p["thigh_l"] = [(0.05, None, None), (0.45, c.D("thigh_l", flex=kf, side=ks), kst),
                    (0.9, c.D("thigh_l", flex=kf, side=ks), 0.75 * kst), (1.25, None, 0.6), (1.9, None, 0.4),
                    (2.4, None, 0.0)]
    p["calf_l"] = [(0.05, None, None), (0.45, c.A("calf_l", flex=kc_), 0.3), (0.9, c.A("calf_l", flex=kc_), 0.2),
                   (1.25, None, 0.3), (1.9, None, 0.2), (2.4, None, 0.0)]
    # (the right arm goes limp under him as he rolls onto it; as he sinks back it is drawn out forward again, so
    # it does not stay folded under his chest and roll him back onto his side)
    # (held a little, in the pose he was hit in, the while his weight is on it: limp, it is wrung round under him
    # past what a shoulder turns, 90 degrees and more on some men)
    hr = c.rc.get("hold_r", 0.0)
    for n in ("upperarm_r", "lowerarm_r"):
        p[n] = ([(0.0, None, hr), (0.6, None, 0.8 * hr), (0.95, None, 0.7 * hr)] if hr else [(0.9, None, 0.0)]) + \
            [(1.25, None, 0.6), (1.9, None, 0.4), (2.4, None, 0.0)]
    return p


def plan_prone_e(c):
    """He kicks and arches: his elbows still prop him, the back and neck straighten hard and the head wrenches round,
    the thighs lift a little off the ground and one knee then the other kicks; all of it gone by 1 s."""
    p = {}
    for s in "lr":
        # (his elbows on the ground: the spasm straightens them and his shoulders come up off his forearms)
        p["upperarm_" + s] = [(0.0, None, 2.0), (0.55, None, 1.6), (0.9, None, 0.0)]
        p["lowerarm_" + s] = [(0.0, None, 2.0), (0.2, c.A("lowerarm_" + s, flex=15.0), 2.2),
                              (0.55, c.A("lowerarm_" + s, flex=15.0), 1.8), (0.9, None, 0.0)]
    for n in ("spine_01", "spine_02", "chest"):
        p[n] = [(0.02, None, 1.2), (0.22, c.A(n, flex=-7.0), 1.7), (0.6, c.A(n, flex=-7.0), 1.3),
                (0.95, c.A(n, flex=0.0), 0.0)]
    for n in ("neck", "head"):
        sg = c.way(n, 2, (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))        # (the turn of his face to his right)
        tw = float(c.hold(n)[2]) + sg * 22.0
        p[n] = [(0.02, None, None), (0.25, c.A(n, flex=-24.0, twist=tw), 1.0), (0.6, c.A(n, flex=-24.0, twist=tw), 0.8),
                (0.95, c.A(n, flex=0.0), 0.0)]
    for s in "lr":
        n = "thigh_" + s
        p[n] = [(0.02, None, 0.8), (0.3, c.D(n, flex=-8.0), 0.55), (0.55, c.D(n, flex=-3.0), 0.4), (0.8, None, 0.0)]
    p["calf_r"] = [(0.03, None, None), (0.26, c.A("calf_r", flex=90.0), 0.3), (0.5, c.A("calf_r", flex=30.0), 0.3),
                   (0.72, c.A("calf_r", flex=70.0), 0.25), (1.0, c.A("calf_r", flex=25.0), 0.0)]
    p["calf_l"] = [(0.03, None, None), (0.28, c.A("calf_l", flex=30.0), 0.3), (0.52, c.A("calf_l", flex=95.0), 0.3),
                   (0.78, c.A("calf_l", flex=30.0), 0.25), (1.0, c.A("calf_l", flex=25.0), 0.0)]
    return p


def plan_prone_f(c):
    """A hand to the wound: the blow and his own heave roll him half onto his left side (the left arm folding limp
    under him, the right knee drawn up), so his right hand can go in under him to where he was struck; he curls over
    it, and the strength goes out of him."""
    p = {}
    wound = c.wound if c.wound is not None else c.H0[c.rd.pix["spine_02"]]
    kc = c.rd.pix["chest"]
    cu, ce, miss = c.reach("r", wound, normal=RG._unit(c.H0[kc] - wound + np.array((0.0, 0.0, 0.15))), w_n=0.004)
    c.note["reach_miss_cm"] = round(miss * 100.0, 1)
    p["upperarm_r"] = [(0.08, None, None), (0.45, cu, 1.4), (1.2, cu, 1.0), (1.7, cu, 0.0)]
    p["lowerarm_r"] = [(0.08, None, None), (0.45, (ce, 0.0, 0.0), 1.4), (1.2, (ce, 0.0, 0.0), 1.0),
                       (1.7, (ce, 0.0, 0.0), 0.0)]
    p["hand_r"] = [(0.08, None, None), (0.45, (15.0, 0.0, 0.0), 1.0), (1.7, (15.0, 0.0, 0.0), 0.0)]
    for n, a in (("spine_01", 9.0), ("spine_02", 9.0), ("chest", 9.0)):
        sg = c.way(n, 2, (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))        # (the twist that lifts his right side)
        p[n] = [(0.0, None, 1.0), (0.35, c.D(n, flex=4.0, twist=sg * a), 1.2), (1.1, c.D(n, flex=9.0, twist=sg * a), 0.9),
                (1.6, c.D(n, flex=9.0), 0.0)]
    for n in ("neck", "head"):
        p[n] = [(0.04, None, None), (0.3, c.A(n, flex=-5.0), 0.8), (1.0, c.A(n, flex=12.0), 0.6),
                (1.4, c.A(n, flex=12.0), 0.0)]
    p["thigh_r"] = [(0.05, None, None), (0.45, c.A("thigh_r", flex=60.0, side=32.0), 0.6),
                    (1.2, c.A("thigh_r", flex=60.0, side=32.0), 0.45), (1.7, c.A("thigh_r", flex=45.0), 0.0)]
    p["calf_r"] = [(0.05, None, None), (0.45, c.A("calf_r", flex=80.0), 0.6), (1.2, c.A("calf_r", flex=80.0), 0.45),
                   (1.7, c.A("calf_r", flex=65.0), 0.0)]
    for n in ("thigh_l", "calf_l"):
        p[n] = [(0.0, None, 0.6), (1.2, None, 0.5), (1.6, None, 0.0)]
    return p


def plan_prone_g(c):
    """He pushes up on his arms, slowly, fails and sinks: the hands drawn in and planted under his shoulders, the
    arms straightening against his weight for more than a second, the head lifting; then they go."""
    p = {}
    kc = c.rd.pix["chest"]
    down = c.frame("chest") @ np.array((1.0, 0.0, 0.0))          # (the chest's front: toward the ground, lying)
    out_r = c.frame("chest") @ np.array((0.0, 1.0, 0.0))
    for s, sg in (("l", -1.0), ("r", 1.0)):
        sh = c.x0[kc] + c.R0[kc] @ c.j["upperarm_" + s]["rp"]
        plant = sh + 0.20 * down + 0.07 * sg * out_r
        push = sh + 0.50 * down + 0.05 * sg * out_r
        cu0, ce0, m0 = c.reach(s, plant, normal=down, fore=down, seed=1)
        cu1, ce1, m1 = c.reach(s, push, normal=down, fore=down, seed=2)
        c.note["plant_miss_cm_" + s] = round(m0 * 100.0, 1)
        c.note["push_miss_cm_" + s] = round(m1 * 100.0, 1)
        p["upperarm_" + s] = [(0.3, None, 0.0), (0.62, cu0, 1.5), (0.85, cu0, 3.2), (2.0, cu1, 3.6), (2.2, cu1, 3.2),
                              (2.45, cu1, 0.0)]
        p["lowerarm_" + s] = [(0.3, None, 0.0), (0.62, (ce0, 0.0, 0.0), 1.5), (0.85, (ce0, 0.0, 0.0), 3.2),
                              (2.0, (ce1, 0.0, 0.0), 3.6), (2.2, (ce1, 0.0, 0.0), 3.2), (2.45, (ce1, 0.0, 0.0), 0.0)]
        p["hand_" + s] = [(0.3, None, 0.0), (0.62, (-40.0, 0.0, 0.0), 2.0), (2.2, (-55.0, 0.0, 0.0), 2.0),
                          (2.45, (-40.0, 0.0, 0.0), 0.0)]
    for n in ("spine_01", "spine_02", "chest"):
        p[n] = [(0.0, None, 0.7), (2.2, c.A(n, flex=-5.0), 0.9), (2.5, c.A(n, flex=0.0), 0.0)]
    for n in ("neck", "head"):
        p[n] = [(0.25, c.A(n, flex=10.0), 0.3), (0.85, c.A(n, flex=0.0), 0.6), (1.9, c.A(n, flex=-20.0), 0.9),
                (2.2, c.A(n, flex=-10.0), 0.6), (2.45, c.A(n, flex=5.0), 0.0)]
    return p


def plan_cover_d(c):
    """He slides down where he knelt: knocked back, his legs give (the recipe's tone) and the knee under him folds as
    he sinks back onto his heel; his trunk is held up over his heels a moment but sags to his right, that shoulder
    dragging down, the head dropping, the struck arm hanging, his right hand going down to the ground; then the
    strength goes and he tips over onto his front. (The forward leg is left to itself: driven, it turns him round on
    his knee.)"""
    p = {}
    p["calf_r"] = [(0.1, None, None), (0.6, c.A("calf_r", flex=137.0), 0.4), (1.3, c.A("calf_r", flex=137.0), 0.3),
                   (1.7, c.A("calf_r", flex=130.0), 0.0)]
    for n in ("spine_01", "spine_02", "chest"):
        sg = c.way(n, 1, (0.0, 0.0, 1.0), (0.0, 1.0, 0.0))        # (the bend that leans him to his right)
        p[n] = [(0.06, None, 1.0), (0.45, c.A(n, flex=-3.0, side=sg * 3.0), 1.0),
                (1.25, c.A(n, flex=2.0, side=sg * 6.0), 0.7), (1.7, c.A(n, flex=4.0, side=sg * 7.0), 0.0)]
    for n in ("neck", "head"):
        sg = c.way(n, 1, (0.0, 0.0, 1.0), (0.0, 1.0, 0.0))
        p[n] = [(0.2, None, None), (0.75, c.A(n, flex=14.0, side=sg * 9.0), 0.5),
                (1.5, c.A(n, flex=18.0, side=sg * 9.0), 0.0)]
    return p


def plan_cover_f(c):
    """He folds forward and down where he is: the head knocked back, then the back and the hips give forward while
    the knee under him gives too (he sinks back as he folds, so his chest goes down, not on), leaning a little to his
    right, the way he comes off his own knee."""
    p = {}
    lean = c.rc.get("lean", 1.0)                  # (to his right: 1; a try may lean him the other way)
    for n in ("spine_01", "spine_02", "chest"):
        sg = c.way(n, 1, (0.0, 0.0, 1.0), (0.0, 1.0, 0.0))
        p[n] = [(0.1, None, None), (0.5, c.A(n, flex=20.0, side=sg * 4.0 * lean), 0.5),
                (0.8, c.A(n, flex=22.0, side=sg * 5.0 * lean), 0.3), (1.1, c.A(n, flex=22.0), 0.0)]
    if c.rc.get("sit", True):
        # (a try may leave these out: a man with a heavy pack who sits back onto his heel can go over backwards)
        p["calf_r"] = [(0.08, None, None), (0.6, c.A("calf_r", flex=137.0), 0.8), (0.9, c.A("calf_r", flex=137.0), 0.4),
                       (1.3, c.A("calf_r", flex=125.0), 0.0)]
        p["thigh_r"] = [(0.08, None, None), (0.6, c.D("thigh_r", flex=45.0), 0.6), (0.9, c.D("thigh_r", flex=50.0), 0.3),
                        (1.3, c.D("thigh_r", flex=40.0), 0.0)]
    p["thigh_l"] = [(0.08, None, None), (0.6, c.A("thigh_l", flex=108.0), 0.3), (1.0, c.A("thigh_l", flex=108.0), 0.0)]
    return p


TRIES = [dict(), dict(push=1.25), dict(push=0.85, drive=1.12)]
TRIES_D = TRIES + [dict(push=0.7)]

MORE = [
    # ---- lying. Struck in the left side from his left front: the blow and his own heave roll him (the upper body
    # most, the hips with it) and he pushes off the ground hard with his left hand, the left knee drawn up and out,
    # half onto his right side (that arm, under him, held a little: limp, it was wrung 90 degrees and more past a
    # shoulder's turn); then the arm gives, the back untwists and he sinks back onto his front (a last shove of his
    # own weight, 1.15 s, back toward it)
    dict(name="die_prone_d", stance="prone", kind="side", start=("fire_prone", 0.5),
         push=[(0.0, "chest", (-0.3, 0.7, 0.6)), (0.0, "head", (-0.2, 0.4, 0.2)),
               (1.15, "chest", (0.0, -0.45, -0.25)), (1.15, "upperarm_l", (0.0, -0.3, -0.35))],
         roll=dict(w=6.0, to="right", share={"pelvis": 0.6, "spine_01": 0.8, "spine_02": 0.85, "thigh": 0.15,
                                             "calf": 0.08, "foot": 0.05}), push_len=0.55, arm_k=2.0, twist=16.0,
         knee=(60.0, 20.0, 80.0, 0.7),
         # (the radio man, the set on his back, stays on his side after the three usual tries: a softer blow, a
         # smaller roll, lets him sink back)
         tries=TRIES_D,
         hit=("chest", (-0.3, 0.7, 0.0)), hold_r=1.0,
         tone={"legs": (0.3, 0.4), "trunk": (0.6, 0.5), "neck": (0.4, 0.4), "arms": (0.0, 0.1)},
         plan=plan_prone_d, release=0.04, throw=(0.4, -0.5, 0.3), t_min=2.8, t_max=6.0, face_down=True),
    # ---- lying. Struck in the chest from in front (through the shoulders, into the spine): a spasm (0.07 s, his
    # own muscles: the chest and head thrown up) and he kicks and arches, then goes limp
    dict(name="die_prone_e", stance="prone", kind="back", start=("fire_prone", 1.15),
         push=[(0.0, "chest", (-0.5, 0.25, 0.2)), (0.0, "spine_01", (-0.2, 0.15, 0.2)),
               (0.07, "chest", (0.0, 0.0, 1.0)), (0.07, "spine_02", (0.0, 0.0, 0.6)), (0.07, "head", (0.0, 0.0, 0.6))],
         tone={"legs": (0.0, 0.15), "trunk": (0.1, 0.2), "neck": (0.1, 0.2), "arms": (0.3, 0.3)},
         hit=("chest", (-0.5, 0.25, -0.1)),
         plan=plan_prone_e, release=0.04, throw=(0.1, 0.2, 0.0), throw_spin=(0.0, 0.5, 0.3), t_min=1.5, t_max=5.0),
    # ---- lying. Struck in the body under him from his right front: he heaves half onto his left side (that arm
    # folding under him: held a moment, as limp it is wrung past what a shoulder turns), his right hand goes in under
    # him to the wound, the right knee drawn up; he curls over it and is still, on his front or, on some men's
    # bodies, left half on his side over the drawn-up knee (not required: the checks say which, `front`)
    dict(name="die_prone_f", stance="prone", kind="gut", start=("fire_prone", 0.15),
         push=[(0.0, "spine_02", (-0.6, -0.5, 0.3)), (0.0, "head", (-0.2, -0.2, 0.0))],
         roll=dict(w=4.0, to="left", share={"pelvis": 0.5, "spine_01": 0.7, "spine_02": 0.9, "thigh": 0.2,
                                            "calf": 0.1, "foot": 0.05}),
         tone={"legs": (0.3, 0.6), "trunk": (0.2, 0.4), "neck": (0.2, 0.4), "arms": (0.1, 0.2)},
         tone_part={"upperarm_l": (0.45, 0.45), "lowerarm_l": (0.45, 0.45)},
         hit=("spine_02", (-0.6, -0.5, 0.0)),
         plan=plan_prone_f, wound=True, release=0.03, throw=(0.1, 0.1, 0.0), throw_spin=(0.0, 0.5, 0.3),
         t_min=2.1, t_max=6.0),
    # ---- lying. Struck from behind in the right shoulder: that arm is knocked from under him and his face goes
    # down (half way: he is not dead yet, his neck holds a moment, so it opens unlike the falls whose head drops
    # limp, die_prone_a and die_crawl_c); he gathers his hands under him and pushes up, slowly, fails and sinks onto
    # his face
    dict(name="die_prone_g", stance="prone", kind="forward", start=("fire_prone", 1.3),
         push=[(0.0, "upperarm_r", (1.4, -1.0, 0.3)), (0.0, "chest", (0.5, -0.2, 0.0)), (0.0, "head", (0.7, 0.0, -0.3))],
         tone={"legs": (1.6, 0.6), "trunk": (0.5, 0.5), "neck": (0.2, 0.3), "arms": (0.0, 0.1)},
         hit=("upperarm_r", (1.4, -1.0, -0.3)),
         plan=plan_prone_g, release=0.04, throw=(0.6, 0.5, 0.2), t_min=3.0, t_max=6.0),
    # ---- in cover, kneeling at it. Struck in the left shoulder from in front: it is knocked back and he with it, his
    # legs give and he slides down where he knelt, onto his heel, his trunk held up a moment and sagging to his
    # right, the shoulder dragging down; then (a shove of his own weight, 1.35 s, to his right as he then faces) he
    # tips over
    dict(name="die_cover_d", stance="cover", kind="side", start=("fire_kneel", 0.15),
         push=[(0.0, "upperarm_l", (-1.0, 0.5, -0.1)), (0.0, "chest", (-0.4, 0.2, 0.0)), (0.0, "head", (-0.4, 0.3, 0.0)),
               (0.0, "pelvis", (-0.6, 0.1, 0.0))],
         tone={"legs": (0.15, 0.3), "trunk": (0.3, 0.6), "neck": (0.1, 0.4), "arms": (0.05, 0.3)},
         tone_part={"upperarm_l": (0.0, 0.05), "lowerarm_l": (0.0, 0.05), "hand_l": (0.0, 0.05)},
         plan=plan_cover_d, later_own=[(1.35, "chest", (-0.1, 0.5, 0.0))], release=0.15, throw=(0.2, 0.3, 0.0),
         t_min=2.0, t_max=5.5),
    # ---- in cover. A hard blow to the chest from in front throws him back off it: over backwards, the knees bent
    dict(name="die_cover_e", stance="cover", kind="back", start=("fire_kneel", 1.05),
         push=[(0.0, "chest", (-3.2, 0.2, 0.6)), (0.0, "head", (-1.6, 0.0, 0.3)), (0.0, "spine_02", (-1.2, 0.0, 0.2))],
         tone={"legs": (0.0, 0.12), "trunk": (0.0, 0.15), "neck": (0.0, 0.1), "arms": (0.0, 0.08)},
         hit=("chest", (-3.2, 0.2, 0.0)),
         release=0.03, throw=(0.8, 0.4, 1.4), t_min=1.2, t_max=5.0),
    # ---- in cover, up at it. Struck in the head from in front: it snaps back, and he folds forward and down (his own
    # weight going forward as the head snaps back, 0.32 s: without it some men's bodies, JAP's, went over backwards
    # off the knee and the radio man's onto his side)
    dict(name="die_cover_f", stance="cover", kind="forward", start=("fire_kneel", 1.3),
         push=[(0.0, "head", (-1.5, 0.15, 0.3))],
         later_own=[(0.32, "chest", (0.8, 0.0, -0.1)), (0.32, "spine_02", (0.4, 0.0, 0.0))],
         tone={"legs": (0.2, 0.5), "trunk": (0.04, 0.2), "neck": (0.0, 0.04), "arms": (0.0, 0.1)},
         hit=("head", (-1.5, 0.15, 0.0)),
         plan=plan_cover_f, release=0.1, throw=(0.3, 0.2, 0.0), t_min=1.6, t_max=5.5, travel=True, face_down=True,
         # (a man who sits back onto his heel as he folds can keel over onto his right side, off his raised left knee
         # (USA, JAP); leaning him the other way, and a softer blow that knocks him back less, put him on his face)
         tries=[dict(), dict(lean=-1.2), dict(push=0.5, lean=-1.2)]),
]


def recipes(only=None):
    return [r for r in MORE if not only or r["name"] in only]


# ------------------------------------------------------------------------------------------------ one fall
class Touch:
    """What comes down when: each part's landings (a touch after 0.1 s clear of the ground), with the speed it came
    down at, for the events; and the drive's step."""

    def __init__(self, f, drive=None, after=1.0, own=(), t_end=None):
        self.f, self.drive, self.after, self.t_end = f, drive, after, t_end
        self.own = sorted(own, key=lambda e: e[0])  # (t, part index, dv): a later shove in his own axes AT THAT TIME
        self.slow = 0.0
        self.trunk = [f.rd.pix[n] for n in ("pelvis", "spine_01", "spine_02", "chest", "head")]
        n = len(f.rd.parts)
        self.last = np.full(n, -1.0)                # (the last time each part touched)
        self.land = {}                               # part -> [(t, speed down)]
        self.vz = np.zeros(n)
        self.was = set()
        self.calm = False

    def step(self, f):
        if self.t_end is not None and f.t > self.t_end - REST_END and not self.calm:
            # (see the end of this step: from here on every joint is held, gently, as it now is, and the drives
            # are done)
            self.calm = True
            self.drive = None
            t0 = f.t
            for j in f.joints:
                kc = j["child"]
                f.target[kc] = j["R0"].T @ f.R[j["parent"]].T @ f.R[kc]
                f.tone[kc] = (lambda t, t0=t0: CALM_TONE * sm((t - t0) / 0.3))
        if self.drive is not None:
            self.drive.step(f)
        while self.own and self.own[0][0] <= f.t:
            # (his heading then: the pelvis's forward, level; a shove "to his right" goes to his right as he is now
            # turned, not as he was hit)
            _, k, dv = self.own.pop(0)
            fw = f.R[f.rd.pix["pelvis"]] @ np.array((1.0, 0.0, 0.0))
            fw[2] = 0.0
            fw = fw / max(float(np.linalg.norm(fw)), 1e-9)
            rt = np.array((-fw[1], fw[0], 0.0))
            f.v[k] += dv[0] * fw + dv[1] * rt + np.array((0.0, 0.0, dv[2]))
        # (f.touch is what touched in the step before this one)
        for k in f.touch:
            if k not in self.was and f.t - self.last[k] > 0.1:
                self.land.setdefault(k, []).append((round(f.t, 4), round(float(-self.vz[k]), 3)))
            self.last[k] = f.t
        self.was = set(f.touch)
        self.vz = f.v[:, 2].copy()
        # (the last of it: ragdoll.py's cloth-and-soil damping holds a man who is down and slow, but a limb pressed
        # between him and the ground can go on pushing him along a few cm a second for seconds. Once his act is over
        # and his trunk has been slow for REST_AFTER, the parts that LIE ON THE GROUND get the same damping,
        # REST_DAMP strong; a part still in the air (a shin standing on its knee) is left to fall)
        h = 1.0 / f.hz
        lin = float(np.max(np.linalg.norm(f.v[self.trunk], axis=1)))
        self.slow = self.slow + h if (f.t > self.after and lin < REST_SLOW and (not f.held or f.rd.unarmed)) else 0.0
        if self.slow > REST_AFTER and f.touch:
            k = math.exp(-REST_DAMP * h * min(1.0, (self.slow - REST_AFTER) / 0.3))
            on = set(f.touch)
            # (the rifle, let go and lying low, on the ground or across his arm: it does not rock on for seconds
            # after he is still, teetering on a sleeve where the ground's touch does not reach it)
            wk = f.rd.pix["weapon"]
            if not f.held and not f.rd.unarmed and f.x[wk][2] < REST_GUN_Z:
                on.add(wk)
            on = sorted(on)
            f.v[on] *= k
            f.w[on] *= k
            # (a hand or forearm lying low across him, off the ground, trembles on its joint: its turning, not its
            # fall, is damped the same way)
            low = [i for i in range(len(f.rd.parts)) if i not in on and f.x[i][2] < REST_GUN_Z]
            f.w[low] *= k
        if self.slow > REST_AFTER + REST_LAST:
            # (and a limb that still will not lie still a second after his trunk has: a knee standing up off his
            # back, its foot rocking on its sole for seconds, the joints and the ground's touch feeding each other.
            # Whatever moves then is that rocking, not a fall: the whole of him is damped the same way)
            k = math.exp(-REST_DAMP * h * min(1.0, (self.slow - REST_AFTER - REST_LAST) / 0.3))
            f.v *= k
            f.w *= k
        elif self.t_end is not None and f.t > self.t_end - REST_END:
            # (the last resort: a leg folded under him, caught between his weight and the ground, can rock him
            # for as long as the fall is let run and his trunk is never slow for long. In the last REST_END of the
            # longest time a fall may run, whatever still moves is calmed the same way, and each joint held as it
            # is then (CALM_TONE, see the top of this step: the joint limit and the ground's touch otherwise go on
            # turning a foot on its ankle), so the clip ends still)
            k = math.exp(-REST_DAMP * h * min(1.0, (f.t - self.t_end + REST_END) / 0.3))
            f.v *= k
            f.w *= k


def down_at_start(rd, f, lift):
    """The parts that are on the ground in the pose he was hit in (within 3 cm, before the ragdoll's lift clear)."""
    P = f._spheres()
    low = {}
    for i, k in enumerate(rd.sp_part):
        low[k] = min(low.get(k, 9.0), float(P[i, 2] - rd.sp_r[i] - lift))
    return {k for k, z in low.items() if z < 0.03}


def events_of(f, rd, tch, down0):
    """knee_down, body_fall, rifle_drop, elbow_down at the moment they touch, as the other deaths': one of each (an
    elbow a side). A part on the ground when he was hit: its first set-down (the ragdoll's lift) is not a landing."""
    ev = []
    lands = {}
    for k, ls in tch.land.items():
        ls = [x for x in ls if x[0] > 0.02]
        if k in down0 and ls and ls[0][0] < 0.3:
            ls = ls[1:]
        lands[rd.parts[k]] = ls
    knees = [(ls[0][0], n) for n, ls in lands.items() if n in ("calf_l", "calf_r") and ls]
    if knees:
        t, n = min(knees)
        ev.append({"t": round(t, 3), "fx": "knee_down", "socket": None, "cue": "knee_down", "bone": n})
    body = [x for n in ("pelvis", "spine_02", "chest") for x in lands.get(n, [])]
    if body:
        hard = max(body, key=lambda x: x[1])         # (the thump: where the trunk comes down hardest)
        first = min(body)
        t = hard[0] if hard[1] > 0.5 else first[0]
        ev.append({"t": round(t, 3), "fx": "body_fall", "socket": None, "cue": "body_fall", "bone": "spine_02"})
    if not rd.unarmed and f.weapon_land is not None:
        ev.append({"t": round(float(f.weapon_land[0]), 3), "fx": "rifle_drop", "socket": None, "cue": "kit_rattle"})
    for n in ("lowerarm_l", "lowerarm_r"):
        ls = [x for x in lands.get(n, []) if x[1] > 0.6]
        if ls:
            t = max(ls, key=lambda x: x[1])[0]
            ev.append({"t": round(t, 3), "fx": "elbow_down", "socket": None, "cue": "footstep", "bone": n})
    return sorted(ev, key=lambda e: e["t"])


def changed(rc, tr):
    """A recipe with one of its tries' changes."""
    r = dict(rc)
    k = tr.get("push", 1.0)
    r["push"] = ([(t, p, tuple(v * k for v in dv)) for (t, p, dv) in rc.get("push", [])] + list(rc.get("later", []))
                 + list(tr.get("later", [])))
    r["later_own"] = list(rc.get("later_own", [])) + list(tr.get("later_own", []))
    r["drive_scale"] = tr.get("drive", 1.0)
    r["push_scale"] = k
    for k in ("lean", "sit"):
        if k in tr:
            r[k] = tr[k]
    return r


def ease_in(man, frames, start_loc):
    """The first key made the named start pose itself: each bone's turn (and move) off the pose at the ragdoll's first
    frame is eased out over EASE seconds (see the module's note). Frames [(t, {bone: (q, move)})], changed in place."""
    t0, loc0 = frames[0]
    one = np.array((1.0, 0.0, 0.0, 0.0))
    off = {}
    for n in set(loc0) | set(start_loc):
        if n == "root":
            continue
        q0, m0 = loc0.get(n, (one, None))
        qs, ms = start_loc.get(n, (one, None))
        dq = RG.qmat(q0).T @ RG.qmat(qs)
        dm = (np.zeros(3) if ms is None else np.asarray(ms, float)) - (np.zeros(3) if m0 is None else np.asarray(m0, float))
        if np.linalg.norm(RG.rotvec(dq)) > 1e-5 or np.linalg.norm(dm) > 1e-5:
            off[n] = (dq, dm)
    for i, (t, loc) in enumerate(frames):
        w = 1.0 - sm(t / EASE)
        if w <= 0.0:
            break
        for n, (dq, dm) in off.items():
            q, mv = loc.get(n, (one, None))
            q2 = RG.matq(RG.qmat(q) @ RG.frac(dq, w))
            if np.linalg.norm(dm) > 1e-5 or mv is not None:
                mv = (np.zeros(3) if mv is None else np.asarray(mv, float)) + dm * w
            loc[n] = (q2, mv)
    # (a bone eased from a turn to none, or a move to none, needs its keys to end: clip_from keeps a track only for
    # bones that turn, and these now do; nothing more to do)
    return frames


def ease_chain(man, c, start_loc):
    """The whip chain's keys (falls.chain_drape's, laid from the pose after the physics) eased from the named start
    pose's own whip over the first EASE seconds, as ease_in does for the body: the radio man's start keys his whip
    his own way, the drape its own (11 degrees apart at the tip, lying). Keys at the drape's rate there."""
    ch = getattr(man, "chain", None)
    if not ch or not all(b in man.ix for b in ch):
        return c
    have = {tr["part_name"]: tr for tr in c["tracks"]}
    one = np.array((1.0, 0.0, 0.0, 0.0))
    ts = [k / F.BAG_FPS for k in range(int(round(EASE * F.BAG_FPS)) + 1)]
    for b in ch:
        tr = have.get(b)
        if tr is None or "rot" not in tr:
            continue
        qs = RG.qmat(start_loc.get(b, (one, None))[0])
        new = []
        for t in ts:
            qd = RG.qmat(man.local_at(c, t)[b][0])
            u = sm(t / EASE)
            q = RG.matq(qs @ RG.frac(qs.T @ qd, u))
            if new and float(np.dot(q, new[-1][1:])) < 0.0:
                q = -q
            new.append([round(t, 4)] + [round(float(v), 5) for v in q])
        rest = [k for k in tr["rot"] if k[0] > EASE + 1e-9]
        if rest and float(np.dot(np.array(rest[0][1:]), new[-1][1:])) < 0.0:
            rest = [[k[0]] + [-v for v in k[1:]] for k in rest]
        tr["rot"] = F._thin(new + rest)
    return c


ROLL_SHARE = {"pelvis": 1.0, "spine_01": 1.0, "spine_02": 1.0, "chest": 1.0, "neck": 1.0, "head": 1.0,
              "upperarm": 0.6, "lowerarm": 0.4, "hand": 0.3, "thigh": 0.6, "calf": 0.25, "foot": 0.15}


def roll_him(f, rd, roll, k=1.0):
    """A blow that rolls a lying man (a recipe's `roll`: w, rad/s; to: "right" or "left", the side he rolls onto): every
    part set turning about the line along his body on the ground at that side, the trunk whole, the limbs by
    ROLL_SHARE (they trail), as falls.py gives a blast's turn to the whole man."""
    H = f.heads()
    ax = H[rd.pix["head"]] - H[rd.pix["pelvis"]]
    ax[2] = 0.0
    ax = ax / np.linalg.norm(ax)
    right = np.cross((0.0, 0.0, 1.0), ax)
    sg = 1.0 if roll.get("to", "right") == "right" else -1.0
    piv = H[rd.pix["spine_02"]] + sg * roll.get("half", 0.17) * right
    piv[2] = 0.0
    w = -sg * float(roll["w"]) * k * ax
    share = dict(ROLL_SHARE, **roll.get("share", {}))
    for i, n in enumerate(rd.parts):
        if n == "weapon":
            continue
        fam = n.rsplit("_", 1)[0] if n.endswith(("_l", "_r")) else n
        s = share.get(fam, 1.0)
        r = f.x[i] - piv
        r = r - ax * float(r @ ax)
        f.v[i] += s * np.cross(w, r)
        f.w[i] += s * w


def simulate(man, rd, rc):
    """One fall of a recipe: (clip, fall, info). As falls.simulate, with the drives, the landings, the eased start."""
    clip, t0 = rc["start"]
    dt = 1.0 / 30.0
    start_loc = man.local_at(clip, t0)
    f = RG.Fall(rd, start_loc, man.local_at(clip, t0 - dt), dt, travel=np.zeros(3),
                mu_d=rc.get("mu", 0.7), mu_s=rc.get("mu", 0.7) + 0.2)
    Gs, Xs = man.fk(start_loc)
    lift = float(f.heads()[rd.pix["pelvis"]][2] - Xs[man.i("pelvis")][2])      # (the lift clear of the ground)
    down0 = down_at_start(rd, f, lift)
    base = {}
    for j in f.joints:
        g = next(k for k, v in F.GROUPS.items() if j["fam"] in v)
        h, fd = rc["tone"][g]
        h, fd = rc.get("tone_part", {}).get(rd.parts[j["child"]], (h, fd))
        base[j["child"]] = F.tone_fn(h, fd)
        f.tone[j["child"]] = base[j["child"]]
    ctx = Ctx(man, rd, f, rc["name"])
    ctx.note = {}
    ctx.rc = rc
    ctx.wound = None
    hit = hit_of_recipe(rc)
    if rc.get("wound"):
        # (where he was struck, as the `hit` block has it, where it is on him now)
        at = hit_point(man, hit["bone"], np.array(hit["push"]), Gs[man.i(hit["bone"])])
        ctx.wound = Xs[man.i(hit["bone"])] + Gs[man.i(hit["bone"])] @ (at - man.head[man.i(hit["bone"])])
        ctx.wound = ctx.wound + np.array((0.0, 0.0, lift))
    drive = None
    if rc.get("plan"):
        plan = rc["plan"](ctx)
        k = rc.get("drive_scale", 1.0)
        if k != 1.0:
            plan = {n: [(t, c, None if s is None else s * k) for (t, c, s) in ks] for n, ks in plan.items()}
        drive = Drive(f, plan, base)
    f.push = [(t, rd.pix[p], np.array(dv, float)) for (t, p, dv) in rc.get("push", [])]
    if rc.get("roll"):
        roll_him(f, rd, rc["roll"], rc.get("push_scale", 1.0))
    f.release_at = rc.get("release", 0.1)
    f.throw = np.array(rc.get("throw", (0.0, 0.0, 0.0)), float)
    f.throw_w = np.array(rc.get("throw_spin", (0.0, 6.0, 2.0)), float)
    own = [(t, rd.pix[p], np.array(dv, float) * rc.get("push_scale", 1.0)) for (t, p, dv) in rc.get("later_own", [])]
    tch = Touch(f, drive, after=rc.get("t_min", 1.0) - 0.2, own=own, t_end=rc.get("t_max", 4.0))
    f.run(t_max=rc.get("t_max", 4.0), t_min=rc.get("t_min", 1.0), settle=SETTLE, on_step=tch.step)
    rest = bool(f.still >= SETTLE)
    F._trim(f, rc.get("t_min", 1.0))
    note = ("ragdoll fall (%s): from %s at %.2f s, %s; %s; physics baked, the first key is the start pose, the last "
            "frame lies still" % (TOOL, clip, t0, rc["kind"], ABOUT_EACH[rc["name"]]))
    frames = [(t, F.frame_local(rd, R, H, t, None)) for (t, R, H) in f.rec]
    ease_in(man, frames, start_loc)
    c = F.clip_from(man, frames, note, events_of(f, rd, tch, down0))
    ease_chain(man, c, start_loc)
    c["start"] = [clip, t0]
    c["weapon"] = man.clips[clip].get("weapon", "rifle")
    kv, kw = int(np.argmax(np.linalg.norm(f.v, axis=1))), int(np.argmax(np.linalg.norm(f.w, axis=1)))
    ctx.note["end_fastest"] = [rd.parts[kv], round(float(np.linalg.norm(f.v[kv])), 3), rd.parts[kw],
                               round(float(np.linalg.norm(f.w[kw])), 2)]
    info = dict(rest=rest, lift=round(lift, 3), note=ctx.note,
                rifle_land=None if f.weapon_land is None else [round(float(v), 3) for v in f.weapon_land[1]])
    return c, f, info


ABOUT_EACH = {
    "die_prone_d": "struck in the left side, he rolls half onto his right side (the blow, and joints driven: his left "
                   "hand planted and pushing, the back turning that side up, the left knee drawn up), then the arm "
                   "gives and he sinks back onto his front",
    "die_prone_e": "he kicks and arches (a spasm; joints driven: the elbows straightened under him, back and neck "
                   "straightened, the head wrenched round, the thighs lifted, the knees kicking in turn), then goes "
                   "limp",
    "die_prone_f": "his right hand goes to the wound under him (the blow and joints driven: he heaves half onto his "
                   "left side, the arm to the struck point, the right knee drawn up, the back curling), then he is "
                   "still",
    "die_prone_g": "he pushes up slowly on his arms (joints driven: hands planted, arms straightening), fails and "
                   "sinks onto his face",
    "die_cover_d": "struck in the left shoulder, he slides down onto his heel (his legs going; joints driven: the "
                   "knee under him folding, the trunk held up and sagging to his right, that shoulder dragging down), "
                   "then tips over",
    "die_cover_e": "a hard blow to the chest from in front throws him over backwards, knees bent",
    "die_cover_f": "struck in the head, it snaps back and he folds forward and down where he is (his weight going "
                   "forward; joints driven: back and hips folding as the knee under him gives)",
}


# ------------------------------------------------------------------------------------------------ the checks
def pose_at(man, c, t):
    Gw, X = man.fk(man.local_at(c, t))
    return Gw, X


def ang(Ra, Rb):
    return math.degrees(float(np.linalg.norm(RG.rotvec(Ra @ Rb.T))))


def opening(man, c):
    """A clip's first 0.3 s: the pelvis's place, the chest's and the head's turns at 0 and at OPEN_T; and how far he
    moves from his first key (the most within 0.3 s, and at each time)."""
    ip, ic, ih = man.i("pelvis"), man.i("chest"), man.i("head")
    G0, X0 = pose_at(man, c, 0.0)
    at = {}
    for t in OPEN_T:
        G, X = pose_at(man, c, t)
        at[t] = (X[ip].copy(), G[ic].copy(), G[ih].copy())
    most = [0.0, 0.0, 0.0]
    for k in range(1, 19):
        G, X = pose_at(man, c, 0.3 * k / 18)
        most[0] = max(most[0], 100.0 * float(np.linalg.norm(X[ip] - X0[ip])))
        most[1] = max(most[1], ang(G[ic], G0[ic]))
        most[2] = max(most[2], ang(G[ih], G0[ih]))
    return dict(first=(X0[ip].copy(), G0[ic].copy(), G0[ih].copy()), at=at, most=most)


def own_move(o):
    """[pelvis cm, chest deg, head deg] from the first key, at each of OPEN_T, and the most within 0.3 s."""
    p0, c0, h0 = o["first"]
    out = {}
    for t in OPEN_T:
        p, c, h = o["at"][t]
        out["%.1f" % t] = [round(100.0 * float(np.linalg.norm(p - p0)), 1), round(ang(c, c0), 1), round(ang(h, h0), 1)]
    out["most"] = [round(v, 1) for v in o["most"]]
    return out


def pair_diff(oa, ob, t):
    """Two falls at time t: the difference as drawn (pelvis place cm, chest and head turn degrees), and the
    difference of their MOVES from their own first keys (so two falls from different poses that do the same thing
    read as alike)."""
    pa, ca, ha = oa["at"][t]
    pb, cb, hb = ob["at"][t]
    drawn = [100.0 * float(np.linalg.norm(pa - pb)), ang(ca, cb), ang(ha, hb)]
    p0a, c0a, h0a = oa["first"]
    p0b, c0b, h0b = ob["first"]
    moved = [100.0 * float(np.linalg.norm((pa - p0a) - (pb - p0b))), ang(ca @ c0a.T, cb @ c0b.T),
             ang(ha @ h0a.T, hb @ h0b.T)]
    return [round(v, 1) for v in drawn], [round(v, 1) for v in moved]


def apart(d):
    return d[0] >= APART_CM or d[1] >= APART_DEG or d[2] >= APART_DEG


def reacting(most, lift_cm=0.0):
    """Whether a fall's first 0.3 s is a reaction: the chest or the head turned REACT_DEG, or the pelvis moved
    REACT_CM, NOT counting the ragdoll's own lift of a lying man clear of the ground (lift_cm: every lying fall has
    it, eased in here over EASE; it is no reaction)."""
    return bool(most[1] >= REACT_DEG or most[2] >= REACT_DEG or most[0] - lift_cm >= REACT_CM)


def opening_table(man, clips, rows, lifts=None):
    """The opening table of a file: rows {stance: [clip names]}; every pair's difference at OPEN_T, and each new
    clip's verdict. `clips`: name -> clip (the file's and the seven); `lifts`: new clip -> the ragdoll's lift, cm."""
    lifts = lifts or {}
    out = {}
    for stance, names in rows.items():
        names = [n for n in names if n in clips]
        op = {n: opening(man, clips[n]) for n in names}
        pairs = {}
        for i, a in enumerate(names):
            for b in names[i + 1:]:
                e = {}
                for t in OPEN_T:
                    dr, mv = pair_diff(op[a], op[b], t)
                    e["%.1f" % t] = {"drawn": dr, "moved": mv}
                e["apart"] = bool(apart(e["0.3"]["drawn"]) and apart(e["0.3"]["moved"]))
                pairs[a + " | " + b] = e
        new = {}
        for n in names:
            if n not in SEVEN:
                continue
            mv = own_move(op[n])
            react = reacting(mv["most"], lifts.get(n, 0.0))
            near = [k for k, e in pairs.items() if n in k.split(" | ") and not e["apart"]]
            new[n] = {"moves": mv, "lift_cm": lifts.get(n, 0.0), "reacts": bool(react), "too_like": near,
                      "pass": bool(react and not near)}
        out[stance] = {"clips": names, "own": {n: own_move(op[n]) for n in names}, "pairs": pairs, "new": new}
    return out


def rows_of(deaths):
    """The lying row and the cover row: every clip named under the stance, in the table's order, once."""
    out = {}
    for st in ("prone", "cover"):
        seen = []
        for kind, names in deaths.get(st, {}).items():
            for n in names:
                if n not in seen:
                    seen.append(n)
        out[st] = seen
    return out


def table_with(deaths):
    """The file's deaths table with DEATHS_ADD's names after its own (none twice)."""
    out = {s: {k: list(v) for k, v in kinds.items()} for s, kinds in deaths.items()}
    for s, kinds in DEATHS_ADD.items():
        for k, names in kinds.items():
            row = out.setdefault(s, {}).setdefault(k, [])
            for n in names:
                if n not in row:
                    row.append(n)
    return out


_ROM = {}


def rom_faults(man, c):
    """rom.py's faults of a clip (falls.rom_check), and which of them the start pose itself has (the first EASE
    seconds are the file's own start pose easing out). None if rom.py is not there."""
    try:
        import rom  # noqa: F401
    except Exception as e:                           # (not beside the tools: said, not hidden)
        return None
    rep = F.rom_check(man, c)
    out = []
    for r in rep["faults"]:
        out.append({"key": r["key"] + r["side"], "value": round(float(r["value"]), 1), "limit": r["limit"],
                    "t": r["t"], "in_start_pose": bool(r["t"] <= EASE + 1e-6)})
    fast = [(n, d["deg_s"], d["t"]) for n, d in rep["fast"]]
    return {"faults": out, "fast": fast}


def first_key_check(man, c):
    """The first key against the named start pose: the largest turn off it of any bone (degrees, each bone's own
    local turn), and the pelvis's and the weapon's place off it (cm)."""
    s = c["start"]
    A, B = man.local_at(c, 0.0), man.local_at(s[0], s[1])
    one = np.array((1.0, 0.0, 0.0, 0.0))
    worst, who = 0.0, ""
    for n in man.names:
        qa, qb = A.get(n, (one, None))[0], B.get(n, (one, None))[0]
        a = ang(RG.qmat(qa), RG.qmat(qb))
        if a > worst:
            worst, who = a, n
    GA, XA = man.fk(A)
    GB, XB = man.fk(B)
    far = max(float(np.linalg.norm(XA[b] - XB[b])) for b in range(len(man.names)))
    return {"deg": round(worst, 2), "bone": who, "heads_cm": round(100.0 * far, 2), "ok": bool(worst <= 1.0)}


def end_check(man, rd, c):
    """The last frame: flat (chest and head low; falls.py's ground check), still (the last 0.2 s), where the rifle
    ends."""
    g = F.ground_check(man, c)
    L = c["long"]
    G1, X1 = pose_at(man, c, L)
    G0, X0 = pose_at(man, c, L - 0.2)
    body = [b for b, n in enumerate(man.names) if n in rd.bone_part]
    mv = max(float(np.linalg.norm(X1[b] - X0[b])) for b in body)
    tn = max(ang(G1[b], G0[b]) for b in body)
    g["chest_z"] = round(float(X1[man.i("chest")][2]), 3)
    g["head_z"] = round(float(X1[man.i("head")][2]), 3)
    g["flat"] = bool(g["chest_z"] < LIE_MAX and g["head_z"] < HEAD_MAX)
    g["last_0.2s_cm"] = round(100.0 * mv, 2)
    g["last_0.2s_deg"] = round(tn, 2)
    g["still"] = bool(100.0 * mv <= STILL_CM and tn <= STILL_DEG)
    wp = X1[man.i("weapon")]
    g["rifle"] = None if rd.unarmed else {
        "at": [round(float(v), 3) for v in wp], "from_pelvis_m": round(float(np.linalg.norm((wp - X1[man.i("pelvis")])[:2])), 2),
        "z": round(float(wp[2]), 3), "on_ground": bool(wp[2] < 0.12)}
    return g


def travel_check(man, c):
    """How far forward of its first place the chest bone's head goes (the most over the clip, and at the end), and the
    head's the same: what a wall in front of him would have to leave room for."""
    ic, ih = man.i("chest"), man.i("head")
    G0, X0 = pose_at(man, c, 0.0)
    n = int(round(c["long"] * 30)) + 1
    mc = mh = 0.0
    for k in range(n + 1):
        G, X = pose_at(man, c, min(c["long"], k / 30.0))
        mc = max(mc, float(X[ic][0] - X0[ic][0]))
        mh = max(mh, float(X[ih][0] - X0[ih][0]))
    return {"chest_forward_max_m": round(mc, 3), "chest_forward_end_m": round(float(X[ic][0] - X0[ic][0]), 3),
            "head_forward_max_m": round(mh, 3), "ok": bool(mc < TRAVEL_MAX)}


def checks_of(man, rd, rc, c, info):
    """Every check of one new clip (but the opening table, which needs its row)."""
    out = {"long": c["long"], "start": c["start"], "end_offset": c["end_offset"], "events": [(e["fx"], e["t"]) for e in c["events"]],
           "came_to_rest": info["rest"], "ground": end_check(man, rd, c), "first_key": first_key_check(man, c),
           "rom": rom_faults(man, c), "note": info["note"], "blows": info.get("blows", 1.0), "try": info.get("try", 0)}
    op = opening(man, c)
    out["opening"] = own_move(op)
    mv = out["opening"]["most"]
    out["lift_cm"] = round(100.0 * float(info.get("lift", 0.0)), 1)
    out["reacts"] = reacting(mv, out["lift_cm"])
    if rc.get("travel"):
        out["travel"] = travel_check(man, c)
    out["front"] = front_check(man, c)
    out["front"]["required"] = bool(rc.get("face_down"))
    return out


def front_ok(ch):
    f = ch.get("front")
    return bool(f is None or not f.get("required") or f["ok"])


def front_check(man, c):
    """Whether he ends on his front: the chest bone's front (its x) pointing down at least 45 degrees, and his roll
    (to his right, +) and how far that front points down at the end."""
    G, X = pose_at(man, c, c["long"])
    R = G[man.i("chest")]
    fz = float((R @ np.array((1.0, 0.0, 0.0)))[2])
    roll = math.degrees(math.asin(max(-1.0, min(1.0, -float((R @ np.array((0.0, 1.0, 0.0)))[2])))))
    return {"chest_front_z": round(fz, 3), "roll_deg": round(roll, 1), "ok": bool(fz <= -math.sqrt(0.5))}


def passes(ch):
    """Whether a try is inside the checks a retry can mend (flat, still, reacting, the first key, the travel, the
    rifle on the ground, opening apart from the file's own falls of its row). Still is read from the baked keys (the
    last 0.2 s); `came_to_rest` (ragdoll.py's own measure held for SETTLE before the longest time allowed) is kept
    beside it as a note: a hand pressed under him can tremble on unseen."""
    g = ch["ground"]
    return bool(g["flat"] and g["still"] and ch["reacts"] and ch["first_key"]["ok"]
                and g["under_1cm"] <= 60 and ch.get("travel", {"ok": True})["ok"] and front_ok(ch)
                and (g["rifle"] is None or g["rifle"]["on_ground"]) and not ch.get("too_like_old"))


def score(ch):
    g = ch["ground"]
    return ((0 if g["flat"] else 4) + (0 if g["still"] else 2) + (0 if ch["reacts"] else 2)
            + (0 if ch.get("travel", {"ok": True})["ok"] else 3) + (0 if ch["first_key"]["ok"] else 1)
            + (0 if front_ok(ch) else 3)
            + (0 if g["under_1cm"] <= 60 else 1)
            + (0 if g["rifle"] is None or g["rifle"]["on_ground"] else 1)
            + (1 if ch.get("too_like_old") else 0))


_OLD_OPEN = {}


def too_like_old(man, rc, c):
    """The file's own falls of the new clip's row (not the seven: they are made beside it) that it opens too like,
    by the opening table's rule at 0.3 s: so a try can mend it, as it mends the others (the full table, the seven
    against each other too, is made once all seven are in)."""
    key = (id(man), rc["stance"])
    if key not in _OLD_OPEN:
        row = rows_of(man.data.get("deaths", {})).get(rc["stance"], [])
        _OLD_OPEN[key] = {n: opening(man, man.clips[n]) for n in row if n in man.clips and n not in SEVEN}
    o = opening(man, c)
    out = []
    for n, oo in _OLD_OPEN[key].items():
        dr, mv = pair_diff(o, oo, 0.3)
        if not (apart(dr) and apart(mv)):
            out.append(n)
    return out


# ------------------------------------------------------------------------------------------------ where he was struck
def side_of(push):
    """The face struck, opposite the push: the larger of x and y decides; a blow with neither counts as front."""
    x, y = float(push[0]), float(push[1])
    if abs(x) < 1e-9 and abs(y) < 1e-9:
        return "front"
    if abs(x) >= abs(y):
        return "front" if x < 0 else "back"
    return "left" if y > 0 else "right"


def hit_of_recipe(rc):
    """A recipe's hit: {bone, push (unit), from, side[, blast]} (no `at` yet: that is each man's own)."""
    kind = rc["kind"]
    r = rc["fall"] if kind == "crawl" else rc
    if rc.get("hit"):
        bone, dv, blast = rc["hit"][0], rc["hit"][1], False      # (the seven: the round's own blow, see MORE)
    elif "blast" in r and not r.get("push"):
        bone, dv, blast = "chest", r["blast"], True
    else:
        t, bone, dv = min(r["push"], key=lambda e: e[0])      # (the first blow; of several at once, the first named)
        blast = "blast" in r
    if rc["name"].startswith("die_leg_") and rc.get("tone_part") and not rc.get("hit"):
        bone = next(iter(rc["tone_part"]))            # (the leg taken from under him: its thigh, as the README has it)
    v = np.array(dv, float)
    u = v / max(float(np.linalg.norm(v)), 1e-9)
    h = {"bone": bone, "side": side_of(u), "push": [round(float(x), 3) for x in u], "from": kind}
    if blast or kind == "thrown":
        h["blast"] = True
    return h


# The deaths that have no recipe: the three older takes (the README's words: die_back "hit in front, the feet go from
# under him and he lands flat on his back"; die_forward "hit on the move, a jolt, the knees buckle and he falls on his
# face"; die_kneel "doubles over, sinks to his knees, falls to the side"), and the riders' seat slumps, each of which
# its own `about` begins "struck back" (the round from in front).
OLD_TAKES = {
    "die_back": {"bone": "chest", "side": "front", "push": [-1.0, 0.0, 0.0], "from": "back"},
    "die_forward": {"bone": "spine_02", "side": "back", "push": [1.0, 0.0, 0.0], "from": "forward"},
    "die_kneel": {"bone": "spine_01", "side": "front", "push": [-1.0, 0.0, 0.0], "from": "gut"},
}
SEAT = {"bone": "chest", "side": "front", "push": [-1.0, 0.0, 0.0], "from": "back", "seat": True}


def hit_point(man, bone, push, G0):
    """A point of the bone's own surface on the struck side, in the rest pose's axes: see the module's note."""
    b = man.i(bone)
    pts = man.P[man.part == b]
    if not len(pts):
        return man.head[b].copy()
    d = G0.T @ (-np.asarray(push, float))             # (toward the gun, in the bone's rest frame)
    d = d / max(float(np.linalg.norm(d)), 1e-9)
    mid = pts.mean(0)
    s = (pts - mid) @ d
    cap = pts[s >= s.max() - HIT_CAP]
    off = (cap - mid) - np.outer((cap - mid) @ d, d)
    return cap[int(np.argmin(np.linalg.norm(off, axis=1)))].copy()


def hits_of(man, clips, new_recipes=()):
    """The `hit` block of every die_* clip of a file (`clips`: the file's and the new ones)."""
    by = {r["name"]: r for r in F.all_recipes()}
    by.update({r["name"]: r for r in F.CRAWLS})
    by.update({r["name"]: r for r in new_recipes})
    out, left = {}, []
    for n in sorted(clips):
        if not n.startswith("die_"):
            continue
        if n in by:
            h = hit_of_recipe(by[n])
        elif n in OLD_TAKES:
            h = dict(OLD_TAKES[n])
        elif n.endswith("_slump") or "_slump_" in n:
            h = dict(SEAT)
        else:
            left.append(n)
            continue
        if h["bone"] not in man.ix:
            left.append(n)
            continue
        G0, X0 = pose_at(man, clips[n], 0.0)
        at = hit_point(man, h["bone"], np.array(h["push"]), G0[man.i(h["bone"])])
        blk = {"bone": h["bone"], "side": h["side"], "at": [round(float(v), 4) for v in at], "push": h["push"],
               "from": h["from"]}
        for k in ("blast", "seat"):
            if h.get(k):
                blk[k] = True
        out[n] = blk
    return out, left


ABOUT = (
    "More deaths and where each was struck (Tools/battle_units/falls_more.py on the ragdoll of falls.py and "
    "ragdoll.py; for the effects team, 25 November 2026). Seven more physics falls for men shot lying or in cover, "
    "each opening its own way and reacting in its first 0.3 s, in the `deaths` table after the ones it had: "
    "die_prone_d (struck in the left side, rolls half onto his right side, sinks back), die_prone_e (kicks and "
    "arches, goes limp), die_prone_f (his right hand goes to the wound under him, then still), die_prone_g (pushes up "
    "slowly on his arms, fails, sinks onto his face), die_cover_d (slides down onto his heel, a shoulder "
    "dragging down, tips over), die_cover_e (thrown back off his cover, over backwards with the knees bent), "
    "die_cover_f (struck in the head, folds forward and down; its chest bone goes at most 0.35 m forward of where it "
    "started: the clip cannot know the cover in front of him). Their first key is the `start` pose itself (every "
    "bone). Events as the other deaths': knee_down, body_fall, rifle_drop, elbow_down, at the moment they touch. "
    "Every die_* clip carries `hit`: bone (the bone struck), side (front, back, left or right: the face of his body "
    "struck, in his own axes at the clip's first key), at ([x, y, z], a point of that bone's surface on that side in "
    "the rest pose's axes: skin it with `bone` alone and it rides on him), push ([x, y, z] unit, his own axes at the "
    "first key: the way the blow drove him; the round came from minus push), from (the fall's kind word); blast true "
    "on the thrown deaths (a burst, not a round), seat true on the riders' slumps (struck back in his seat).")


# ------------------------------------------------------------------------------------------------ build
_W = {}


def _init(path):
    F._BODY.clear()                                  # (falls.rom_check keys its body by id(man): a new man may reuse it)
    _OLD_OPEN.clear()                                # (and too_like_old the same)
    F._init(path)
    _W["m"], _W["rd"], _W["path"] = F._W["m"], F._W["rd"], path


def _job(rc):
    """One recipe on the worker's man: tried as it is, then with its tries' changes while it is outside the checks;
    the first inside them is kept, else the nearest."""
    m, rd = _W["m"], _W["rd"]
    st = rc["start"]
    m.bag_rel = F.bag_rel(m, st[0], st[1], m.bag) if m.bag else None
    best = None
    t0 = time.time()
    tries = rc.get("tries", TRIES)
    for i, tr in enumerate(tries[:rc.get("n_tries", len(tries))]):
        c, f, info = simulate(m, rd, changed(rc, tr))
        n_run = i + 1
        info["blows"], info["try"] = tr.get("push", 1.0), i
        ch = checks_of(m, rd, rc, c, info)
        ch["too_like_old"] = too_like_old(m, rc, c)
        ok = passes(ch)
        if ok or best is None or score(ch) < best[3]:
            best = (c, ch, i, score(ch))
        if ok:
            break
    c, ch, i, _ = best
    if i:
        c["from"] += " (try %d for this man's body: %s)" % (i + 1, json.dumps(tries[i], sort_keys=True))
    ch["tries"] = i + 1
    print("   %s %s: try %d kept (%d run), %.0f s" % (os.path.basename(_W["path"]), rc["name"], i + 1, n_run,
                                                     time.time() - t0), flush=True)
    return rc["name"], c, ch


def _job_at(job):
    """One (file, recipe) job: the worker loads that file's man when it is not the one it holds."""
    path, rc = job
    if _W.get("path") != path:
        _init(path)
    return _job(rc)


def build_files(names, src, stage, workers=2, only=None):
    """The bundles of several files, one pool over every (file, recipe) so no worker waits at a file's end; each
    file's bundle is staged as soon as its seven are in (in the order given). Yields (name, bundle)."""
    jobs = recipes(only)
    todo = [(os.path.join(src, "soldier_%s.json" % n), rc) for n in names for rc in jobs]
    t0 = time.time()
    if workers > 1 and len(todo) > 1:
        from multiprocessing import Pool
        p = Pool(min(workers, len(todo)))
        it = p.imap(_job_at, todo, chunksize=1)
    else:
        p, it = None, map(_job_at, todo)
    try:
        for name in names:
            out = [next(it) for _ in jobs]
            yield name, build_file(name, src, stage, out=out, only=only, secs=time.time() - t0)
            t0 = time.time()
    finally:
        if p is not None:
            p.close()
            p.join()


def build_file(name, src, stage, workers=2, only=None, out=None, secs=None):
    """The bundle of one file: its seven clips, the table's additions, every die_* clip's hit, the checks. With
    `only` (some of the seven's names) and a bundle already staged from the same source file, only those are made
    again and the others kept. `out`: the seven already made (build_files)."""
    path = os.path.join(src, "soldier_%s.json" % name)
    out_path = os.path.join(stage, "soldier_%s.%s.json" % (name, SET))
    source = {"file": os.path.basename(path), "bytes": os.path.getsize(path)}
    t0 = time.time() - (secs or 0.0)
    if out is None:
        jobs = recipes(only)
        if workers > 1 and len(jobs) > 1:
            from multiprocessing import Pool
            with Pool(min(workers, len(jobs)), initializer=_init, initargs=(path,)) as p:
                out = p.map(_job, jobs, chunksize=1)
        else:
            _init(path)
            out = [_job(rc) for rc in jobs]
    data = json.load(open(path))
    man = RG.Man(data)
    clips = {n: c for n, c, ch in out}
    checks = {n: ch for n, c, ch in out}
    if only and os.path.exists(out_path):
        old = json.load(open(out_path))
        if old["checks"]["source"] == source:
            for n in SEVEN:
                if n not in clips and n in old["clips"]:
                    clips[n], checks[n] = old["clips"][n], old["checks"]["clips"][n]
                    if "front" not in checks[n]:          # (a bundle staged before the check: from its keys)
                        checks[n]["front"] = front_check(man, clips[n])
                        checks[n]["front"]["required"] = bool(next(r for r in MORE if r["name"] == n).get("face_down"))
    allc = dict(data["clips"])
    allc.update(clips)
    hit, left = hits_of(man, allc, MORE)
    for n, c in clips.items():
        c["hit"] = hit[n]
    deaths = table_with(data.get("deaths", {}))
    table = opening_table(man, allc, rows_of(deaths), {n: ch["lift_cm"] for n, ch in checks.items()})
    for st in table:
        for n, v in table[st]["new"].items():
            if n in checks:
                checks[n]["opening_pass"] = v["pass"]
                checks[n]["too_like"] = v["too_like"]
    bundle = {"clips": {n: clips[n] for n in SEVEN if n in clips}, "deaths_add": DEATHS_ADD, "hit": hit, "about": ABOUT,
              "checks": {"clips": {n: checks[n] for n in SEVEN if n in checks}, "opening": table,
                         "hit_left_out": left, "source": source, "bones": [b["name"] for b in data["bones"]],
                         "mesh": mesh_sign(data), "starts": starts_sign(data)}}
    os.makedirs(stage, exist_ok=True)
    tmp = out_path + ".tmp"
    with open(tmp, "w") as fh:
        json.dump(bundle, fh, separators=(",", ":"))
    os.replace(tmp, out_path)
    print(report_line(name, bundle, time.time() - t0), flush=True)
    return bundle


def mesh_sign(data):
    """What the `hit` points were measured on: the near mesh's point count and a checksum of the bone heads and the
    near points (rounded to 0.1 mm), so a delivery onto a re-exported man is refused rather than wrong."""
    m = data["near"]
    heads = np.round(np.array([b["head"] for b in data["bones"]], float) * 1e4).astype(np.int64)
    pts = np.round(np.array(m["points"], float) * 1e4).astype(np.int64)
    return {"near_points": int(len(pts) // 3 if pts.ndim == 1 else len(pts)),
            "crc": int(zlib.crc32(heads.tobytes() + pts.tobytes()))}


def starts_sign(data):
    """The poses the falls start from, as they were: a checksum of each start clip's keys (sorted JSON)."""
    names = sorted({rc["start"][0] for rc in MORE})
    return {n: int(zlib.crc32(json.dumps(data["clips"][n], sort_keys=True).encode())) if n in data["clips"] else None
            for n in names}


def faults_of(name, bundle):
    """What of a file's bundle is outside the checks: a list of sentences (none: all inside)."""
    out = []
    ck = bundle["checks"]["clips"]
    for n in SEVEN:
        if n not in ck:
            out.append("%s %s: not made" % (name, n))
            continue
        ch = ck[n]
        g = ch["ground"]
        if not g["flat"]:
            out.append("%s %s: not flat at the end (chest %.2f m, head %.2f m)" % (name, n, g["chest_z"], g["head_z"]))
        if not g["still"]:
            out.append("%s %s: not still at the end (%.1f cm, %.1f deg in the last 0.2 s)" %
                       (name, n, g["last_0.2s_cm"], g["last_0.2s_deg"]))
        if g["under_1cm"] > 60:
            out.append("%s %s: %d points more than 1 cm under the ground (lowest %.3f m)" % (name, n, g["under_1cm"], g["min_z"]))
        if g["rifle"] is not None and not g["rifle"]["on_ground"]:
            out.append("%s %s: the rifle ends %.2f m up" % (name, n, g["rifle"]["z"]))
        if not ch["first_key"]["ok"]:
            out.append("%s %s: first key %.1f deg off the start pose (%s)" % (name, n, ch["first_key"]["deg"], ch["first_key"]["bone"]))
        if not ch["reacts"]:
            out.append("%s %s: no reaction in the first 0.3 s (pelvis %.1f cm, chest %.1f, head %.1f deg)" %
                       ((name, n) + tuple(ch["opening"]["most"])))
        if ch.get("too_like"):
            out.append("%s %s: opens too like %s" % (name, n, ", ".join(ch["too_like"])))
        if "travel" in ch and not ch["travel"]["ok"]:
            out.append("%s %s: chest goes %.2f m forward (%.2f at most)" % (name, n, ch["travel"]["chest_forward_max_m"], TRAVEL_MAX))
        if not front_ok(ch):
            out.append("%s %s: ends on his %s, not his front (chest front %.2f down, roll %.0f deg)" % (
                name, n, "back" if ch["front"]["chest_front_z"] > 0.3 else "side", -ch["front"]["chest_front_z"],
                ch["front"]["roll_deg"]))
        if ch["rom"] is None:
            out.append("%s %s: range-of-motion check not run (rom.py not found)" % (name, n))
        else:
            late = [r for r in ch["rom"]["faults"] if not r["in_start_pose"]]
            if late:
                out.append("%s %s: range of motion: %s" % (name, n, ", ".join("%s %.0f (limit %.0f) at %.2f s" % (
                    r["key"], r["value"], r["limit"], r["t"]) for r in late)))
            # (rom.py's `fast` list, a joint over 900 deg/s, is kept in the checks and not counted a fault: falls.py
            # does not count it either, and every one of the existing falls has such joints, a limp hand's flop)
    if bundle["checks"]["hit_left_out"]:
        out.append("%s: no hit written for %s" % (name, bundle["checks"]["hit_left_out"]))
    return out


def report_line(name, bundle, secs=None):
    ck = bundle["checks"]["clips"]
    parts = []
    for n in SEVEN:
        if n not in ck:
            continue
        ch = ck[n]
        g = ch["ground"]
        parts.append("%s %.2fs try%d chest %.2f head %.2f under %d key0 %.2f open %s%s" % (
            n[4:], ch["long"], ch["tries"], g["chest_z"], g["head_z"], g["under_1cm"], ch["first_key"]["deg"],
            "/".join("%.0f" % v for v in ch["opening"]["most"]),
            ((" fwd %.2f" % ch["travel"]["chest_forward_max_m"]) if "travel" in ch else "")
            + ((" ends front %.2f roll %.0f" % (-ch["front"]["chest_front_z"], ch["front"]["roll_deg"])) if "front" in ch else "")))
    fl = faults_of(name, bundle)
    return "%s%s  %s  hits %d  %s" % (name, (" %.0f s" % secs) if secs is not None else "", " | ".join(parts),
                                      len(bundle["hit"]), ("FAULTS: " + "; ".join(fl)) if fl else "all inside the checks")


def print_table(name, table):
    """The opening table of a file, as text: each row's pairs at 0.1, 0.2 and 0.3 s (as drawn; the moves from their
    own first keys at 0.3 s), and each new clip's verdict."""
    for st, T in table.items():
        print("%s  %s row: %s" % (name, st, ", ".join(T["clips"])))
        print("   %-28s %-22s %-22s %-22s %-22s %s" % ("pair", "0.1 s drawn", "0.2 s drawn", "0.3 s drawn",
                                                       "0.3 s moved", "apart"))
        for k, e in T["pairs"].items():
            f3 = lambda v: "%5.1f cm %5.1f %5.1f" % tuple(v)
            print("   %-28s %-22s %-22s %-22s %-22s %s" % (k.replace("die_", ""), f3(e["0.1"]["drawn"]), f3(e["0.2"]["drawn"]),
                                                           f3(e["0.3"]["drawn"]), f3(e["0.3"]["moved"]),
                                                           "yes" if e["apart"] else "NO"))
        for n, v in T["new"].items():
            print("   %s: moves in its first 0.3 s pelvis %.1f cm (%.1f of it the ragdoll's lift), chest %.1f deg, "
                  "head %.1f deg (%s); %s -> %s" % (
                n, v["moves"]["most"][0], v.get("lift_cm", 0.0), v["moves"]["most"][1], v["moves"]["most"][2],
                "reacts" if v["reacts"] else "NO REACTION",
                "apart from every other" if not v["too_like"] else "TOO LIKE " + ", ".join(v["too_like"]),
                "pass" if v["pass"] else "FAIL"))


# ------------------------------------------------------------------------------------------------ apply, check, deliver
def merged(data, bundle):
    """A soldier file's data with a bundle merged in: the seven clips (replacing ones of those names), the table's
    additions after its own lists, `hit` on every die_* clip, `about_falls_more`. Nothing else changes."""
    for n, c in bundle["clips"].items():
        data["clips"].pop(n, None)
    for n, c in bundle["clips"].items():
        data["clips"][n] = c
    deaths = data.get("deaths", {})
    for s, kinds in bundle["deaths_add"].items():
        for k, names in kinds.items():
            row = deaths.setdefault(s, {}).setdefault(k, [])
            for n in names:
                if n not in row and n in data["clips"]:
                    row.append(n)
    data["deaths"] = deaths
    for n, h in bundle["hit"].items():
        if n in data["clips"]:
            data["clips"][n]["hit"] = h
    data["about_falls_more"] = bundle["about"]
    return data


def write_whole(path, data):
    tmp = path + ".tmp"
    with open(tmp, "w") as fh:
        json.dump(data, fh, separators=(",", ":"))
    json.load(open(tmp))                              # (it reads back)
    os.replace(tmp, path)


def apply_files(names, src, out, stage):
    os.makedirs(out, exist_ok=True)
    for nm in names:
        bp = os.path.join(stage, "soldier_%s.%s.json" % (nm, SET))
        if not os.path.exists(bp):
            print(nm, "not staged", flush=True)
            continue
        data = merged(json.load(open(os.path.join(src, "soldier_%s.json" % nm))), json.load(open(bp)))
        write_whole(os.path.join(out, "soldier_%s.json" % nm), data)
        print(nm, "->", os.path.join(out, "soldier_%s.json" % nm), flush=True)


def only_added(before, after, bundle):
    """Whether `after` is `before` with nothing changed but what a bundle adds: a list of what else differs."""
    bad = []
    for k in set(before) | set(after):
        if k in ("clips", "deaths", "about_falls_more"):
            continue
        if before.get(k) != after.get(k):
            bad.append("key " + k)
    for n, c in before["clips"].items():
        if n in bundle["clips"]:
            continue
        a = after["clips"].get(n)
        if a is None:
            bad.append("clip gone " + n)
            continue
        a2 = {k: v for k, v in a.items() if k != "hit"}
        c2 = {k: v for k, v in c.items() if k != "hit"}
        if a2 != c2:
            bad.append("clip changed " + n)
    for s, kinds in before.get("deaths", {}).items():
        for k, names in kinds.items():
            got = after["deaths"].get(s, {}).get(k, [])
            if got[:len(names)] != names:
                bad.append("deaths %s %s reordered" % (s, k))
    return bad


def check_staged(names, src, stage, tables=("GER", "USA", "JAP")):
    """The checks again from the staged keys (read back, not from the physics): each clip's end, first key, opening,
    range of motion; the tables; the merge is additive."""
    all_faults = []
    for nm in names:
        bp = os.path.join(stage, "soldier_%s.%s.json" % (nm, SET))
        if not os.path.exists(bp):
            print(nm, "not staged", flush=True)
            all_faults.append("%s: not staged" % nm)
            continue
        bundle = json.load(open(bp))
        path = os.path.join(src, "soldier_%s.json" % nm)
        F._init(path)
        man, rd = F._W["m"], F._W["rd"]
        before = json.load(open(path))
        after = merged(json.load(open(path)), bundle)
        bad = only_added(before, after, bundle)
        ck = bundle["checks"]["clips"]
        for rc in MORE:
            n = rc["name"]
            if n not in bundle["clips"]:
                continue
            c = bundle["clips"][n]
            info = {"rest": ck[n]["came_to_rest"], "note": ck[n]["note"], "blows": ck[n]["blows"], "try": ck[n]["try"],
                    "lift": ck[n]["lift_cm"] / 100.0}
            st = rc["start"]
            again = checks_of(man, rd, rc, c, info)
            for k in ("ground", "first_key", "opening", "reacts", "travel", "front"):
                if again.get(k) != ck[n].get(k):
                    bad.append("%s %s: staged check differs from the keys" % (n, k))
        man2 = RG.Man(after)
        table = opening_table(man2, after["clips"], rows_of(after["deaths"]), {n: v["lift_cm"] for n, v in ck.items()})
        if json.dumps(table, sort_keys=True) != json.dumps(bundle["checks"]["opening"], sort_keys=True):
            bad.append("opening table differs from the keys")
        hit, left = hits_of(man2, after["clips"], MORE)
        if hit != bundle["hit"]:
            bad.append("hit blocks differ from the keys")
        n_die = sum(1 for n in after["clips"] if n.startswith("die_"))
        n_hit = sum(1 for n, c in after["clips"].items() if n.startswith("die_") and "hit" in c)
        fl = faults_of(nm, bundle) + ["%s: %s" % (nm, b) for b in bad]
        all_faults += fl
        print(report_line(nm, bundle), "| die_ clips %d, with hit %d | table rows prone %d cover %d" % (
            n_die, n_hit, len(table["prone"]["clips"]), len(table["cover"]["clips"])),
            ("| MERGE: " + "; ".join(bad)) if bad else "| merge additive", flush=True)
        if nm in tables:
            print_table(nm, table)
    print("faults: %d" % len(all_faults))
    for x in all_faults:
        print("  ", x)
    return all_faults


def deliver(names, stage, content=False):
    """NOT RUN in this pass. Each target (the shared soldier file; with `content`, the game's Content copy where it
    exists) is read just before its write, the bundle merged in, and if that changes it: backed up as it is (a new
    backup beside any earlier one that differs), written whole through a temporary file and os.replace, and read
    back. A target that already holds exactly this bundle is left alone (the same bytes)."""
    os.makedirs(BACKUP, exist_ok=True)
    for nm in names:
        bp = os.path.join(stage, "soldier_%s.%s.json" % (nm, SET))
        if not os.path.exists(bp):
            print(nm, "not staged: skipped", flush=True)
            continue
        bundle = json.load(open(bp))
        targets = [(SHARED, "shared")] + ([(CONTENT, "content")] if content else [])
        for folder, tag in targets:
            path = os.path.join(folder, "soldier_%s.json" % nm)
            if not os.path.exists(path):
                print(nm, tag, "no file there: skipped", flush=True)
                continue
            raw = open(path).read()                   # (read again just before the write)
            before = json.loads(raw)
            if [b["name"] for b in before["bones"]] != bundle["checks"]["bones"] or \
                    bundle["checks"].get("mesh") != mesh_sign(before) or \
                    bundle["checks"].get("starts") != starts_sign(before):
                print(nm, tag, "SKIPPED: its skeleton, mesh or start clips are not what was staged (the falls and the "
                      "hit points were made on another man); build again from this file", flush=True)
                continue
            after = merged(json.loads(raw), bundle)
            bad = only_added(before, after, bundle)
            if bad:
                print(nm, tag, "SKIPPED: the merge would change", bad, flush=True)
                continue
            text = json.dumps(after, separators=(",", ":"))
            if text == raw:
                print(nm, tag, "already holds this bundle: left as it is", flush=True)
                continue
            base = os.path.join(BACKUP, "soldier_%s.%s.before_%s" % (nm, tag, SET))
            bk, k = base + ".json", 1
            while os.path.exists(bk) and open(bk).read() != raw:
                k += 1
                bk = "%s.%d.json" % (base, k)
            if not os.path.exists(bk):
                with open(bk + ".tmp", "w") as fh:
                    fh.write(raw)
                os.replace(bk + ".tmp", bk)
            if open(path).read() != raw:
                print(nm, tag, "SKIPPED: the file changed while it was being merged; run again", flush=True)
                continue
            write_whole(path, after)
            ok = open(path).read() == text
            print(nm, tag, "delivered" if ok else "READ BACK MISMATCH", "(backup %s)" % bk, flush=True)


# ------------------------------------------------------------------------------------------------ pictures
PAINTS = F.PAINT_ORDER


def draw(man, P, size=(300, 220), az=0.0, down=45.0, dist=3.6, target=(0.3, 0.0, 0.3), focal=1.6, look=None,
         marks=()):
    """A quick picture of the man's near mesh at points P: flat-shaded triangles in painter's order, his shadow
    straight under him (so a float or a sink shows), any triangle more than 2 cm under the ground in red. az 0: from
    straight behind him, + round to his right (90: from his right side); down: degrees the camera looks down. The
    game's axes are left-handed (y right) and are drawn true: from behind, his right is on the right."""
    from PIL import Image, ImageDraw
    lk = dict(F.LOOK)
    lk.update(look or {})
    W, H = size
    img = Image.new("RGB", size, (196, 190, 170))
    dr = ImageDraw.Draw(img)
    tgt = np.asarray(target, float)
    a, p = math.radians(az), math.radians(down)
    eye = tgt + dist * np.array((-math.cos(p) * math.cos(a), math.cos(p) * math.sin(a), math.sin(p)))
    fw = (tgt - eye) / np.linalg.norm(tgt - eye)
    rt = np.cross((0.0, 0.0, 1.0), fw)
    rt /= np.linalg.norm(rt)
    up = np.cross(fw, rt)
    foc = focal * W / 2

    def proj(X):
        d = X - eye
        z = d @ fw
        return np.stack([W / 2 + foc * (d @ rt) / z, H / 2 - foc * (d @ up) / z], -1), z
    for g in np.arange(-3.0, 3.01, 0.5):
        for A, B in (((g, -3.0, 0.0), (g, 3.0, 0.0)), ((-3.0, g, 0.0), (3.0, g, 0.0))):
            s, z = proj(np.array((A, B), float))
            if np.all(z > 0.1):
                dr.line([tuple(s[0]), tuple(s[1])], fill=(170, 164, 146), width=1)
    T = np.array(man.data["near"]["triangles"], int).reshape(-1, 3)
    pnt = np.array(man.data["near"]["paints"], int)
    s, z = proj(P)
    v0, v1, v2 = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    nrm = np.cross(v1 - v0, v2 - v0)
    nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-12)
    L = np.array((0.4, -0.5, 0.75))
    L /= np.linalg.norm(L)
    sh = np.clip(np.abs(nrm @ L), 0.0, 1.0) * 0.65 + 0.35
    sg, zg = proj(P * np.array((1.0, 1.0, 0.0)))
    order = np.argsort(-z[T].mean(1))
    for t in order[::3]:
        dr.polygon([tuple(sg[T[t, 0]]), tuple(sg[T[t, 1]]), tuple(sg[T[t, 2]])], fill=(150, 144, 128))
    cols = [F._rgb(lk.get(PAINTS[min(q, len(PAINTS) - 1)], "777777")) for q in range(len(PAINTS))]
    for t in order:
        c = cols[pnt[T[t, 0]] if pnt[T[t, 0]] < len(cols) else 0]
        k = sh[t]
        if max(P[T[t, 0], 2], P[T[t, 1], 2], P[T[t, 2], 2]) < -0.02:
            c, k = (205, 40, 40), 1.0
        dr.polygon([tuple(s[T[t, 0]]), tuple(s[T[t, 1]]), tuple(s[T[t, 2]])],
                   fill=(int(c[0] * k), int(c[1] * k), int(c[2] * k)))
    for (pt, col) in marks:
        q, zz = proj(np.asarray(pt, float)[None, :])
        x, y = q[0]
        dr.ellipse([x - 4, y - 4, x + 4, y + 4], outline=col, width=2)
    return img


def strip_times(c):
    """Eight frames, denser at the start: 0, 0.1, 0.2, 0.3, 0.5, 0.9, 1.5, the end."""
    return [0.0, 0.1, 0.2, 0.3, 0.5, 0.9, min(1.5, c["long"]), c["long"]]


def strip(man, c, label, az, down, size=(300, 220), dist=3.6, look=None, times=None):
    """A film strip of a clip from one camera, labelled; the camera follows his middle; where he was struck is ringed
    (red) while it shows."""
    from PIL import Image, ImageDraw
    times = times or strip_times(c)
    W, H = size
    out = Image.new("RGB", (W * len(times), H + 18), (236, 232, 222))
    d = ImageDraw.Draw(out)
    hit = c.get("hit")
    for i, t in enumerate(times):
        Gw, X = man.fk(man.local_at(c, t))
        P = man.skin(Gw, X)
        tgt = 0.5 * (X[man.i("pelvis")] + X[man.i("chest")])
        tgt[2] = min(0.45, max(0.2, float(tgt[2])))
        marks = ()
        if hit and t <= 0.5:
            b = man.i(hit["bone"])
            marks = ((X[b] + Gw[b] @ (np.array(hit["at"]) - man.head[b]), (220, 30, 30)),)
        out.paste(draw(man, P, size, az, down, dist, tgt, look=look, marks=marks), (i * W, 18))
        d.text((i * W + 6, 3), ("%s   " % label if i == 0 else "") + "%.2f s" % t, fill=(20, 20, 20))
    return out


def pictures(names, src, stage, pics, all_names=None):
    """For each of `names`: a sheet of film strips, each new fall from the side and from the game's high rear camera
    (45 degrees down). And one sheet of how each of the men (all_names) lies at the end of each new fall."""
    from PIL import Image, ImageDraw
    os.makedirs(pics, exist_ok=True)
    made = []
    for nm in names:
        bp = os.path.join(stage, "soldier_%s.%s.json" % (nm, SET))
        if not os.path.exists(bp):
            print(nm, "not staged", flush=True)
            continue
        bundle = json.load(open(bp))
        data = json.load(open(os.path.join(src, "soldier_%s.json" % nm)))
        man = RG.Man(data)
        look = data.get("colours", {})
        rows = []
        for n in SEVEN:
            if n not in bundle["clips"]:
                continue
            c = bundle["clips"][n]
            rows.append(strip(man, c, "%s  %s  from his right side" % (nm, n), 90.0, 8.0, size=(300, 220), dist=2.3,
                              look=look))
            rows.append(strip(man, c, "%s  %s  high rear camera, 45 down" % (nm, n), 0.0, 45.0, size=(300, 220),
                              dist=2.9, look=look))
        for part, sel in (("prone", [r for r in range(len(rows)) if r < 8]), ("cover", [r for r in range(len(rows)) if r >= 8])):
            if not sel:
                continue
            W = max(rows[r].size[0] for r in sel)
            Hh = sum(rows[r].size[1] for r in sel)
            sheet = Image.new("RGB", (W, Hh), (255, 255, 255))
            y = 0
            for r in sel:
                sheet.paste(rows[r], (0, y))
                y += rows[r].size[1]
            p = os.path.join(pics, "strips_%s_%s.jpg" % (nm, part))
            sheet.save(p, quality=86)
            made.append(p)
            print("picture", p, flush=True)
    if all_names:
        W, H = 250, 180
        have = []
        for nm in all_names:
            bp = os.path.join(stage, "soldier_%s.%s.json" % (nm, SET))
            if os.path.exists(bp):
                have.append(nm)
        sheet = Image.new("RGB", (70 + W * len(SEVEN), 16 + H * len(have)), (236, 232, 222))
        d = ImageDraw.Draw(sheet)
        for j, n in enumerate(SEVEN):
            d.text((74 + j * W, 3), n, fill=(20, 20, 20))
        for i, nm in enumerate(have):
            bundle = json.load(open(os.path.join(stage, "soldier_%s.%s.json" % (nm, SET))))
            data = json.load(open(os.path.join(src, "soldier_%s.json" % nm)))
            man = RG.Man(data)
            look = data.get("colours", {})
            d.text((3, 16 + i * H + H // 2), nm.replace("_", "\n"), fill=(20, 20, 20))
            for j, n in enumerate(SEVEN):
                if n not in bundle["clips"]:
                    continue
                c = bundle["clips"][n]
                Gw, X = man.fk(man.local_at(c, c["long"] + 1.0))
                P = man.skin(Gw, X)
                tgt = P.mean(0)
                tgt[2] = 0.15
                far = float(np.max(np.linalg.norm((P - tgt)[:, :2], axis=1)))
                sheet.paste(draw(man, P, (W, H), 40.0, 40.0, max(3.0, 2.0 * far), tgt, look=look), (70 + j * W, 16 + i * H))
        p = os.path.join(pics, "ends_all.jpg")
        sheet.save(p, quality=86)
        made.append(p)
        print("picture", p, flush=True)
    return made


# ------------------------------------------------------------------------------------------------ main
def main(argv):
    ap = argparse.ArgumentParser(description="Seven more deaths (lying, in cover) and every death's hit block.")
    ap.add_argument("--build", action="store_true")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--pictures", action="store_true")
    ap.add_argument("--deliver", action="store_true")
    ap.add_argument("--content", action="store_true", help="with --deliver: the game's Content copies too")
    ap.add_argument("--files", default=",".join(NAMES), help="names, e.g. GER,USA (default: all twenty)")
    ap.add_argument("--src", default=SRC)
    ap.add_argument("--stage", default=STAGE)
    ap.add_argument("--out", default="")
    ap.add_argument("--pics", default=PICS)
    ap.add_argument("--workers", type=int, default=2)
    ap.add_argument("--only", default="", help="with --build: only these of the seven made again (the others kept "
                                               "from the staged bundle)")
    a = ap.parse_args(argv)
    names = [n for n in a.files.split(",") if n]
    if a.build:
        only = set(a.only.split(",")) - {""}
        allf = []
        for nm, b in build_files(names, a.src, a.stage, a.workers, only):
            allf += faults_of(nm, b)
            if nm in ("GER", "USA", "JAP") and not only:
                print_table(nm, b["checks"]["opening"])
        print("faults: %d" % len(allf))
        for x in allf:
            print("  ", x)
    if a.apply:
        if not a.out:
            ap.error("--apply needs --out DIR2")
        apply_files(names, a.src, a.out, a.stage)
    if a.check:
        check_staged(names, a.src, a.stage)
    if a.pictures:
        pictures([n for n in names if n in ("GER", "USA", "JAP")] if a.files == ",".join(NAMES) else names,
                 a.src, a.stage, a.pics, all_names=NAMES)
    if a.deliver:
        deliver(names, a.stage, a.content)
    if not (a.build or a.apply or a.check or a.pictures or a.deliver):
        print(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
