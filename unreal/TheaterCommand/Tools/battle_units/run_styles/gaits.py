"""The gaits, fitted to every soldier file (25 November 2026, the owner: "can we have different running animations for
the soldiers? it looks like they are all jogging at the same speed").

Today a man moving at 4.5 or 6 m/s plays his 3 m/s run faster. This gives each file a ladder of gaits by speed, so the
game can pick the clip nearest a man's speed, and a "pace" on every gait loop: the ground speeds it can be played at.

    sprint, sprint_b, sprint_c   flat out (three runners; "style_of": "sprint" on _b and _c)      about 5.8, 5.3, 6.3 m/s
    dash, dash_b                 the hunched bound between cover ("style_of": "dash" on _b)        about 4.2, 4.9
    jog_loaded                   a man under a load: short heavy steps, leaning into it            about 2.2
    run_tired                    labouring, the head hung, the arms low                            about 2.5
    run_stumble                  NOT a loop: out of his run at 0.0 s, a caught foot, two quick steps, back into
                                 his run at "rejoin": ["run", t]
    limp_l, limp_r               the walking wounded, favouring the left / the right leg           about 1.0

The motion capture is gaits_clips.json (gaits_mocap.py: CMU takes cut into loops on the reference man). Here each cut
is keyed onto each file's own skeleton as run_styles.py keys the run styles (its plan, plant, feet and ball_slide are
used as they are): the legs, the pelvis and the trunk are the capture's, the pelvis's travel scaled to his legs, the
feet planted on his own legs and the stride fitted to them. Where the speed on him comes out a few per cent off the
speed asked (his legs are not the reference man's), the cut is played that much faster ("quick", 8 per cent at
most). What he does with his arms is the CARRY:

    own     his own clip's arms and hold on his weapon, played in step (the weapon rides the bone it rides there):
            his run's for the loaded jog, the stumble, the tired run of a man whose weapon or load needs both hands,
            and for the sprint of such a man (the machine-gunners, the BAR, the bazooka, its loader, the radio; the
            medic's free arms are the capture's); his ADVANCE's for the dash. Where the new body leans further
            forward than his own clip's, both arms are turned up about his shoulders by that much, so the weapon
            keeps the pitch his own clip gives it at the same moment of his step (and up to 10 degrees more or less,
            the pitch within 5 of his own, where the weapon would be deeper in him than in his own clip at that
            moment, or a forearm nearer his head than 5.5 cm; where that is not enough his trunk is stood up a
            little, his head kept looking where it looked, and last his head turned). Then the hold is carried LOW
            ("low", rehold: turned muzzle-down about his right fist and lowered, both arms set to it by IK, as far
            as clears him and as the arms reach, the drop given up first, per key and eased): the dash's rifle low
            across his body at the hip (4 degrees over level, 7 cm down, turned across him to 38 degrees left of
            ahead: "heading"), dash_b's low and further across (6 degrees, 5 cm, 60 degrees left of ahead: the
            pair differ from above, the one rifle hidden by his body, the other a line across it); the loaded jog's
            low across his belt (8 degrees, 10 cm, held steady at 58 degrees left of ahead, the most across of his
            run's port and no more than 5 past it, not swung by the trunk's roll and twist; a two-handed man's 24
            degrees and 5 cm, kept at his run's heading, "steady"); a two-handed man's tired run 20 degrees and 8 cm;
    hang    the tired run of a rifle file: the rifle hanging in the right hand at the trail, low and almost still
            (TRAIL["tired"]), the left arm hanging and swinging low: a spent man, not a runner;
    trail   the rifle in the RIGHT hand at the trail: the right arm hangs by IK and swings a little with the capture's,
            the fist closed on the rifle at its balance, the weapon bone keyed to ride that fist, muzzle forward
            and a little down, held out from his thigh just as far as clears it; the LEFT arm is the capture's own
            (it pumps free); where it would pass into his kit or chest its elbow is bent more first (to 120 degrees
            of bend at most: a sprinter's elbow is about square), the forearm carried in across his chest or out
            past it (the upper arm turned about its length), its swing drawn toward its middle (a man in webbing
            pumps shorter, to the whole swing where nothing less clears his chest kit), and only then the upper
            arm opened out from his side (OPEN_MOST, 15 degrees at most),
            the least of these, eased between keys, the hand kept within 25 cm of his midline where any way that
            clears does; only where none of these clears his kit (a chest rig, a greatcoat's pouches) is it opened
            to 30 degrees and the forearm carried out to 30 (OPEN_LAST); it may lie a centimetre into the soft
            kit's profile. The rifle files' sprints;
    limp    the rifle at the trail in the right hand, lower and almost still; limp_l: the left hand laid flat on the
            outside of the left thigh (as far down it as the arm reaches; the elbow's side chosen per key and eased
            between keys; where his kit at the hip would put the forearm into it, the hand set out from the thigh
            up to 8 cm (a winter coat's skirt), and where even that does not let the forearm by, the arm hangs
            instead), limp_r: the left arm hanging, swinging low. limp_r is limp_l's capture mirrored, the arms and
            hold made again. Two-handed files keep their own hold.

A child bone of the weapon that his run moves far from it (the loader's rocket in the bag on his back, the radio
man's handset on his strap) is not part of the weapon: it is kept where his own clip has it on his chest, wherever
the weapon goes, and left out of the weapon's clearance.

The sprints of the men in TWO_HANDS (own carry) are never under SPRINT_LEAST (4.6 m/s): neither the clip's speed nor
the low end of its pace.

Keys are 15 a second, as every clip's. Footstep events are at the boots' contacts (contact_starts: the bottom outline
of each boot within 1.5 cm of the ground, at least 0.03 s, not travelling), as late after them as his run's are after
its own (about 0.02 s), one at each. Each clip carries "pace": [lo, hi] m/s (PACE_HOW) and "about" (the carry, the
speed on this man, the pace); the eleven gait loops already in the file get a "pace" too ("pace_add" in the bundle).

Mended 26 November 2026 after two reviews: the bazooka man's arms are lifted only to keep his own weapon line at the
same moment of his step (lift_needed), 10 degrees past it at most and the tube within 5 degrees of his own pitch (they
went up 45 degrees and the tube see-sawed); where his left forearm, across his face to the tube on his shoulder,
would come nearer his head than 5.5 cm, his trunk is stood up a little (a man with a tube on his shoulder runs more
upright) and last his head turned; where no lift within those 5 degrees keeps a weapon held across a man out of his
hips and legs (the BAR's butt over a limping man's thigh, an MG 42's at his hip), it is held up to 12 degrees more
upright (PITCH_LEGS), and each eased key is made sure of; footstep events at the boots' contacts (they missed by a sixth of a step on the
fast gaits); every clip at 15 keys a second (the sprints, dashes and stumble were 30): the sprints and dashes
planted on keys at 30 a second and every other key kept ("dense"), the loop begun where the kept keys cut the boots
least through the ground and pop least (key_phase: the joints turning straight from key to key cut a boot through
the ground between them, and a knee can do its whole snap between two keys), each loop as long as takes a whole
number of keys at 15 a second (keyable: a 0.576 s dash takes neither 8 nor 9); the trail's free arm cleared by
bending its elbow and carrying the forearm out first and opened 30 degrees at most (it was thrown out 54 to 78
degrees); the family's outer pace widened toward the game's speeds (widen_paces; a jog as far as 0.80 to 1.30 of its
time, the tired run at 2.5 m/s so its pace holds 2.0 to 3.2); a planted boot of the stumble that floated set down by
leg IK (set_caught_down); a walk's boots kept out of the ground at its keys by leg IK (boots_up: the limp's good leg
went into it and slid) and a planted boot held still through its stance (boots_still); the limps
played at 1.057 m/s so their pace holds 0.6 to 1.4.

Mended again 26 November 2026 after the pictures' review (gaits_mocap.py made the bodies; here the arms): the tired
run hangs its rifle at the trail ("hang") and the loaded jog carries its rifle low across the belt at his run's
heading ("steady"), so neither reads as his run; the dash's rifle low across the body at the hip, dash_b's higher;
the sprint's free arm opened 15 degrees at most (30 only where nothing less clears his kit), its elbow bent to a
square first and its swing calmed, the hand kept near his midline;
the limp's hand laid on a winter coat too (set out up to 8 cm). Floating is judged on the lower of the sole and
the boot's outline (a boot on its toe cap is on the ground), and key_phase also counts a boot left over the ground;
the limps quickened to the exact speed asked (their pace holds 0.6 to 1.4 to the centimetre a second); the loaded
jog played at 2.2 m/s (2.4 steps a second, slower than his run's 2.5) and its pace widened to 0.77 to 1.40 of its
time, the tired run's to 0.78 to 1.30.

Mended a third time 27 November 2026 (the pictures still showed the advance's upright rifle by dash_b's head and the
loaded jog's muzzle over his left shoulder from the game's camera, and the trail sprint's free hand 35 to 42 cm out
from his midline on CHN and JAP): dash_b's rifle low too (6 degrees, 5 cm) and turned across him to 60 degrees left of
ahead, so the pair still differ (the dash's rifle hidden by his body, dash_b's a line across it); the loaded jog's
held steady at 58 degrees left of ahead; the free arm's swing may be drawn in to the whole of it (CALM_STEPS) before
the arm is opened past 15 degrees.

Mended a fourth time 27 November 2026 (USA's dash had his rifle 3.2 cm into his right thigh between two keys, where
the joints turning straight from one kept key to the next carry the swinging thigh up into the low-held rifle): in
the gaits planted at 30 keys a second and kept at 15 ("dense"), the held-low weapon is kept LOW_MARGIN (1 cm) clearer
of him at the keys than his own clip is; where no hold tried clears as asked, the clearest of them is taken (it was
his own hold, untried); and the search gives up the drop before the turn, as tilt_needed always said it did.

    python3 gaits.py --stage [DIR] [names...]          key and check; DIR/soldier_<N>.gaits.json = {"clips",
                                                       "about_gaits", "pace_add", "checks"} (DIR: STAGE below)
    python3 gaits.py --apply --out DIR2 [names...]     whole soldier files with the bundles merged in (only the new
                                                       clips, "about_gaits" and the old gait loops' "pace" change)
    python3 gaits.py --check [names...]                the staged bundles again, merged on their sources in memory
    python3 gaits.py --deliver [--content] [names...]  merge the bundles into the shared soldier files (and, with
                                                       --content, the game's copies where a file exists): each
                                                       backed up first, written to a temp file and replaced, the
                                                       target re-read just before the write; a target whose skeleton
                                                       or run differs from the one the bundle was keyed on is refused
    options: --src DIR (key from soldier files in another folder), --stage-dir DIR (where --apply, --check and
    --deliver read the bundles), names as GER or soldier_GER.

Run twice, the same bytes. After run_styles.py --deliver (which removes every clip that has a "style_of", so
sprint_b, sprint_c and dash_b with them) or any re-export of a file's run or advance: stage and deliver again.
"""
import hashlib
import json
import math
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import run_styles as R  # noqa: E402

CUTS = os.path.join(HERE, "gaits_clips.json")
RUNS = os.path.join(HERE, "run_styles_clips.json")
SRC_INFANTRY = "/home/user/art/hits/src/"
CONTENT = "/home/user/theater-command/unreal/TheaterCommand/Content/Models/Soldiers/"
SHARED = "/mnt/project-files/battle-models/land/soldier/"
STAGE = "/home/user/art/gaits/stage/"
BACKUP = "/home/user/art/gaits/backup/"
FPS = 30
INFANTRY = ["CHN", "FRA", "GER", "GER_detail", "ITA", "ITA_desert", "JAP", "POL", "SOV", "SOV_snow", "SOV_winter", "UK",
            "USA", "USA_bar", "USA_bazooka", "USA_loader", "USA_medic", "USA_nco", "USA_para", "USA_radio"]
GUNNERS = ["GER_lmg", "GER_lmg34"]          # (their source is the game's copy)
# files whose weapon or load needs both hands: they keep their own hold in the sprint and the limp
TWO_HANDS = {"USA_bar": "the BAR", "USA_bazooka": "the bazooka", "USA_loader": "the rocket bag and carbine",
             "USA_medic": "no weapon (the capture's free arms)", "USA_radio": "the radio and carbine",
             "GER_lmg": "the machine gun", "GER_lmg34": "the machine gun"}
SPRINT_LEAST = 4.6           # (m/s: the sprints of the men in TWO_HANDS are never played slower; their pace starts here)
LOW_TWO = ("USA_bar", "GER_lmg", "GER_lmg34")   # (two-handed men whose weapon is carried across them: held low too
#                                                  where a gait has low_two; the bazooka on the shoulder, the loader's
#                                                  and the radio man's loads are left as their run has them)

# name, the cut in gaits_clips.json, keys a second, the speeds the game will send (the contract), the speed asked,
# the clip whose arms and hold an "own" carry takes, the family's carry on a rifle file
GAITS = [
    dict(name="sprint", cut="sprint_a", rate=15, band=(5.0, 6.5), asked=5.8, hold="run", rifle="trail", kind="sprint",
         quicken=True, dense=2),
    dict(name="sprint_b", cut="sprint_b", rate=15, band=(5.0, 6.5), asked=5.3, hold="run", rifle="trail", kind="sprint",
         quicken=True, style_of="sprint", dense=2),
    dict(name="sprint_c", cut="sprint_c", rate=15, band=(5.0, 6.5), asked=6.3, hold="run", rifle="trail", kind="sprint",
         quicken=True, style_of="sprint", dense=2),
    dict(name="dash", cut="dash_a", rate=15, band=(3.8, 5.5), asked=4.2, hold="advance", rifle="own", kind="dash",
         quicken=True, low=(4.0, 0.07), heading=-38.0, dense=2),
    dict(name="dash_b", cut="dash_b", rate=15, band=(3.8, 5.5), asked=4.9, hold="advance", rifle="own", kind="dash",
         quicken=True, low=(6.0, 0.05), heading=-60.0, style_of="dash", dense=2),
    dict(name="jog_loaded", cut="jog_loaded", rate=15, band=(1.8, 3.0), asked=2.2, hold="run", rifle="own", kind="jog",
         quicken=True, low=(8.0, 0.10), low_two=(24.0, 0.05), heading=-58.0, steady=True, widen=(0.77, 1.40)),
    dict(name="run_tired", cut="run_tired", rate=15, band=(2.0, 3.2), asked=2.5, hold="run", rifle="hang", kind="jog",
         quicken=True, low=(10.0, 0.14), low_two=(20.0, 0.08), trail="tired", left_hangs=True, widen=(0.78, 1.30)),
    dict(name="run_stumble", cut="stumble", rate=15, band=None, asked=None, hold="run", rifle="own", kind="stumble"),
    dict(name="limp_l", cut="limp_l", rate=15, band=(0.6, 1.4), asked=1.057, hold="run", rifle="limp", kind="walk",
         quicken=True, exact=True),
    dict(name="limp_r", cut="limp_l", rate=15, band=(0.6, 1.4), asked=1.057, hold="run", rifle="limp", kind="walk",
         quicken=True, mirror=True, exact=True),
]
# (the limps' 1.057 m/s, "about 1.0": the one speed at which a walk's time limits, 0.565 to 1.33 of its own time,
#  hold the whole of the 0.6 to 1.4 m/s the game will send them)
# (quicken: played a few per cent faster or slower than the cut so the clip comes to the speed asked on this man;
#  low: (degrees, metres): his own two-handed hold turned until the muzzle is no more than that far above level and
#  lowered that much (rehold), as far as clearing him allows: the dash's weapon low at the hip, the loaded man's low
#  across his belt; low_two: (degrees, metres) as low, for the men in LOW_TWO (a weapon carried across him in both
#  hands: less, the gun is long and heavy); the drop is given up 2 cm at a time where the arms do not reach it;
#  heading: the hold turned (yaw) to this many degrees from ahead (left -), across his body: a rifleman's only;
#  steady: the hold kept at his run's heading across him (yaw), not swung by the trunk's roll and twist;
#  widen: how far the clip may be played from its own time to reach the family's band (widen_paces);
#  hang (the tired run's carry on a rifle file): the rifle hanging in the right hand at the trail (TRAIL[trail]), the
#  left arm hanging and swinging low)
NEW = [g["name"] for g in GAITS]
# the gait loops already in every file, and how each is stepped (pace_of's kinds)
OLD = {"walk": "walk", "creep": "walk", "run": "jog", "run_b": "jog", "run_c": "jog", "run_d": "jog",
       "advance": "jog", "advance_b": "jog", "advance_c": "jog", "advance_d": "jog", "flee": "fast"}
FAMILY = {"sprint": ("sprint", "sprint_b", "sprint_c"), "dash": ("dash", "dash_b"), "jog_loaded": ("jog_loaded",),
          "run_tired": ("run_tired",), "limp": ("limp_l", "limp_r")}
FAMILY_BAND = {"sprint": (5.0, 6.5), "dash": (3.8, 5.5), "jog_loaded": (1.8, 3.0), "run_tired": (2.0, 3.2),
               "limp": (0.6, 1.4)}

TRUNK = R.TRUNK
LEGS = R.LEGS
ARMS = R.ARMS
GAME_RATE_MOST = 1.6         # (LandMen: a gait plays at his speed over the clip's own, no faster than 1.6 times)
SLIDE_TIMES = 1.5            # (a pace holds where a planted foot slides under this many times his own run's)
GROUND_UNDER = 0.010         # (no sole more than this under the ground)
GROUND_OVER = 0.020          # (no foot's lowest more than this over it)
FIST_TOL = 0.015             # (a holding fist within this of its place on the weapon)
PLANT_TOLS = (0.035, 0.025, 0.045, 0.02, 0.015)
QUICK_MOST = 0.08            # (a cut played at most this much faster or slower to come to the speed asked)

# ---- the carry at the trail (the right arm by IK; angles in degrees, lengths in metres)
TRAIL = {
    "sprint": dict(back=-6.0, swing=11.0, reach=0.94, pitch=8.0, rock=3.0, yaw=4.0, out0=0.05, clear=0.025),
    "limp": dict(back=-2.0, swing=3.5, reach=0.975, pitch=11.0, rock=1.0, yaw=3.0, out0=0.04, clear=0.02),
    "tired": dict(back=-3.0, swing=9.0, reach=0.975, pitch=14.0, rock=3.0, yaw=3.0, out0=0.04, clear=0.02,
                  left=dict(back=4.0, swing=14.0, reach=0.97)),
}
GRIP_ALONG = 0.072           # (the fist's middle down the hand from the wrist)
GRIP_CURL, GRIP_THUMB = 80.0, 34.0
FREE_CURL, FREE_THUMB = 68.0, 28.0     # (the free hand a loose fist)
FREE_ARM_CLEAR = -0.012      # (the free arm no deeper than this into his kit's profile: the profile is good to about
#                              a centimetre and its widest is soft kit, a tunic's skirt, a bread bag; the second
#                              review asked to take a centimetre of soft kit rather than fling the arm out)
THIGH_DOWN = 0.15            # (the hand on the hurt thigh: this far down it from the hip, if the arm reaches)
LAID_HAND = 0.02             # (the hand laid on the thigh may lie this far inside the thigh's profile: the profile
#                              is good to about a centimetre and takes a tunic's hem at its widest)


# ================================================================================================ small maths
def unit(v):
    v = np.asarray(v, float)
    return v / max(float(np.linalg.norm(v)), 1e-12)


def r_axis(k, deg):
    k = unit(k)
    th = math.radians(deg)
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + math.sin(th) * K + (1 - math.cos(th)) * K @ K


def frame_map(a0, b0, a1, b1):
    """The turn taking a0 onto a1 and b0's part across a0 onto b1's across a1 (field_clips.frame_map)."""
    def fr(a, b):
        a = unit(a)
        b = unit(np.asarray(b, float) - a * (np.asarray(b, float) @ a))
        return np.stack([a, b, np.cross(a, b)], 1)
    return fr(a1, b1) @ fr(a0, b0).T


def sm(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def smooth_loop(v, times=2, loop=True):
    v = np.asarray(v, float).copy()
    for _ in range(times):
        if loop:
            v = 0.25 * np.roll(v, 1) + 0.5 * v + 0.25 * np.roll(v, -1)
        else:
            w = v.copy()
            w[1:-1] = 0.25 * v[:-2] + 0.5 * v[1:-1] + 0.25 * v[2:]
            v = w
    return v


def swap_side(n):
    return n[:-2] + ("_r" if n.endswith("_l") else "_l") if n.endswith(("_l", "_r")) else n


# ================================================================================================ the cut loops
class Cut:
    """A clip of gaits_clips.json as run_styles.Capture reads it (each bone's world turn from its rest, game axes),
    sampled as a loop or, the stumble, once from end to end; `mirror` changes left and right over."""

    def __init__(self, c, rest, mirror=False):
        cap = R.Capture(c, rest)
        self.c = c
        self.F = cap.F
        self.long = cap.long
        self.stride = cap.stride
        self.parent = cap.parent
        self.loop = bool(c.get("loop", True))
        self.mirror = mirror
        if mirror:
            self.T = {n: R.MIRROR @ cap.T[swap_side(n)] @ R.MIRROR for n in cap.T}
            self.pelvis = cap.pelvis * np.array((1.0, -1.0, 1.0))
        else:
            self.T = cap.T
            self.pelvis = cap.pelvis

    def _at(self, f):
        n = self.F - 1
        f = f % n if self.loop else min(max(f, 0.0), float(n))
        i = min(int(f), n - 1)
        return i, f - i

    def local(self, bone, parent, f):
        i, u = self._at(f)
        Tp = (lambda k: self.T[parent][k]) if parent in self.T else (lambda k: np.eye(3))
        a = R.m2q(Tp(i).T @ self.T[bone][i])
        b = R.m2q(Tp(i + 1).T @ self.T[bone][i + 1])
        return R.q2m(R.nlerp(a, b, u))

    def pelvis_at(self, f):
        i, u = self._at(f)
        return self.pelvis[i] * (1 - u) + self.pelvis[i + 1] * u


# ================================================================================================ his hands
class Hands:
    """The arm's lengths and the hand's own directions at rest (field_clips.Man's): d0 down the hand from the wrist,
    t0 across it toward the thumb, n0 out of the palm."""

    def __init__(self, sk):
        self.sk = sk
        H = lambda b: sk.H0[sk.ix[b]]
        self.side = {}
        for s, med in (("l", 1.0), ("r", -1.0)):
            up, lo = H("lowerarm_" + s) - H("upperarm_" + s), H("hand_" + s) - H("lowerarm_" + s)
            d0 = unit(H("fingers_" + s) - H("hand_" + s))
            th = H("thumb_" + s) - H("hand_" + s)
            t0 = unit(th - d0 * (th @ d0))
            n0 = unit(np.cross(d0, t0))
            if n0 @ np.array((0.0, med, -0.3)) < 0:      # (the palm: the side toward his thigh at rest)
                n0 = -n0
            self.side[s] = {"u0": up, "l0": lo, "l1": float(np.linalg.norm(up)), "l2": float(np.linalg.norm(lo)),
                            "bend0": unit(np.cross(up, lo)), "d0": d0, "t0": t0, "n0": n0, "thumb0": unit(th)}

    def grip(self, G, X, s, palm, along=GRIP_ALONG):
        """Where what he holds runs through his fist."""
        A = self.side[s]
        h = self.sk.ix["hand_" + s]
        return X[h] + G[h] @ (A["d0"] * along + A["n0"] * palm)

    def reach(self, s):
        return self.side[s]["l1"] + self.side[s]["l2"]

    def arm(self, q, mv, s, T, pole, Gh, share=0.5):
        """The arm's turns for the wrist at T, the elbow toward `pole`, the hand's world turn Gh (field_clips.Man.arm)."""
        sk, A = self.sk, self.side[s]
        G, X = sk.fk(q, mv)
        ua = sk.ix["upperarm_" + s]
        S = X[ua]
        l1, l2 = A["l1"], A["l2"]
        dv = np.asarray(T, float) - S
        dist = float(np.linalg.norm(dv))
        dc = min(max(dist, abs(l1 - l2) + 1e-4), (l1 + l2) * 0.999)
        u = dv / max(dist, 1e-9)
        v = unit(np.asarray(pole, float) - u * (np.asarray(pole, float) @ u))
        ca = (l1 * l1 + dc * dc - l2 * l2) / (2 * l1 * dc)
        Kn = S + l1 * (u * ca + v * math.sqrt(max(0.0, 1 - ca * ca)))
        Tc = S + u * dc
        up, lo = Kn - S, Tc - Kn
        bend = np.cross(up, lo)
        if np.linalg.norm(bend) < 1e-6:
            bend = np.cross(up, np.asarray(pole, float))
        Gu = frame_map(A["u0"], A["bend0"], up, bend)
        Gl = frame_map(A["l0"], A["bend0"], lo, bend)
        # (the forearm takes `share` of the hand's turn about it, so the wrist does not wring)
        la_ = unit(lo)
        tf, th_ = Gl @ A["t0"], Gh @ A["t0"]
        tf, th_ = unit(tf - la_ * (tf @ la_)), unit(th_ - la_ * (th_ @ la_))
        phi = math.degrees(math.atan2(np.cross(tf, th_) @ la_, tf @ th_))
        Gl = r_axis(la_, share * phi) @ Gl
        Gp = G[sk.parent[ua]]
        q["upperarm_" + s] = Gp.T @ Gu
        q["lowerarm_" + s] = Gu.T @ Gl
        q["hand_" + s] = Gl.T @ Gh
        return Tc

    def fingers(self, q, s, curl, thumb):
        A = self.side[s]
        q["fingers_" + s] = r_axis(np.cross(A["d0"], A["n0"]), curl)
        q["thumb_" + s] = r_axis(np.cross(A["thumb0"], A["n0"]), thumb)


# ================================================================================================ his body, for clearance
class Body:
    """His body and kit as the near mesh's points, a bone at a time: each bone's points (the ones it carries most)
    as a profile about an axis through them (the bone's own for a leg, upright for the trunk and its kit): the
    farthest point in each of 16 directions round the axis, in slices along it. clear() is how far a point stands
    outside the nearest profile (under zero: inside it). Each bone's points go with that bone alone, so near a joint
    it is good to about a centimetre."""
    LIMB = {"thigh_l": "calf_l", "calf_l": "foot_l", "foot_l": "ball_l", "thigh_r": "calf_r", "calf_r": "foot_r",
            "foot_r": "ball_r"}
    BONES = ("pelvis", "spine_01", "spine_02", "chest", "neck", "head", "pouch_l", "pouch_r", "thigh_l", "calf_l",
             "foot_l", "thigh_r", "calf_r", "foot_r")
    NA = 16

    def __init__(self, D, sk):
        self.sk = sk
        M = D["near"]
        P = np.array(M["points"], float).reshape(-1, 3)
        a = np.array(M["part"])
        self.P, self.part = P, a
        self.shapes = {}
        for bone in self.BONES:
            if bone not in sk.ix:
                continue
            pts = P[a == sk.ix[bone]]
            if len(pts) < 16:
                continue
            h = sk.H0[sk.ix[bone]]
            if bone in self.LIMB:
                axis = unit(sk.H0[sk.ix[self.LIMB[bone]]] - h)
                c = h.copy()
            else:
                axis = np.array((0.0, 0.0, 1.0))
                c = pts.mean(0)
            e1 = np.array((1.0, 0.0, 0.0)) if abs(axis[0]) < 0.9 else np.array((0.0, 1.0, 0.0))
            e1 = unit(e1 - axis * (e1 @ axis))
            e2 = np.cross(axis, e1)
            rel = pts - c
            s = rel @ axis
            rho = rel - np.outer(s, axis)
            r = np.linalg.norm(rho, axis=1)
            phi = np.arctan2(rho @ e2, rho @ e1)
            s0, s1 = float(s.min()), float(s.max())
            ns = int(min(12, max(3, round((s1 - s0) / 0.04))))
            ds = (s1 - s0) / ns
            si = np.clip(((s - s0) / ds).astype(int), 0, ns - 1)
            ai = ((phi + math.pi) / (2 * math.pi / self.NA)).astype(int) % self.NA
            tab = np.zeros((ns, self.NA))
            for da in (-1, 0, 1):               # (a point counts for its own direction and the two beside it)
                np.maximum.at(tab, (si, (ai + da) % self.NA), r)
            for i in range(ns):                 # (an empty direction: the slice's smallest filled one)
                row = tab[i]
                if (row > 0).any():
                    row[row == 0] = row[row > 0].min()
            for i in range(ns):
                if not (tab[i] > 0).any():
                    tab[i] = tab[max(i - 1, 0)] if (tab[max(i - 1, 0)] > 0).any() else tab[min(i + 1, ns - 1)]
            self.shapes[bone] = dict(ix=sk.ix[bone], c=c, axis=axis, e1=e1, e2=e2, s0=s0, s1=s1, ds=ds, ns=ns, tab=tab)

    def clear(self, Q, G, X, bones):
        """The least clearance of the world points Q from the profiles of `bones` at the pose (G, X), and which."""
        best, which = 9.0, None
        for bone in bones:
            sh = self.shapes.get(bone)
            if sh is None:
                continue
            i = sh["ix"]
            L = (Q - X[i]) @ G[i] + self.sk.H0[i] - sh["c"]
            s = L @ sh["axis"]
            rho = L - np.outer(s, sh["axis"])
            r = np.linalg.norm(rho, axis=1)
            phi = np.arctan2(rho @ sh["e2"], rho @ sh["e1"])
            si = np.clip(((s - sh["s0"]) / sh["ds"]).astype(int), 0, sh["ns"] - 1)
            ai = ((phi + math.pi) / (2 * math.pi / self.NA)).astype(int) % self.NA
            cr = r - sh["tab"][si, ai]
            ex = np.maximum(np.maximum(sh["s0"] - s, s - sh["s1"]), 0.0)
            d = np.where(ex > 0, np.sqrt(ex * ex + np.maximum(cr, 0.0) ** 2), cr)
            k = int(np.argmin(d))
            if d[k] < best:
                best, which = float(d[k]), bone
        return best, which

    def radius(self, bone, s, direction):
        """The profile's reach at `s` along the bone's axis, toward `direction` (rest axes)."""
        sh = self.shapes[bone]
        rho = np.asarray(direction, float) - sh["axis"] * (np.asarray(direction, float) @ sh["axis"])
        phi = math.atan2(rho @ sh["e2"], rho @ sh["e1"])
        si = int(min(max((s - sh["s0"]) / sh["ds"], 0), sh["ns"] - 1))
        ai = int((phi + math.pi) / (2 * math.pi / self.NA)) % self.NA
        return float(sh["tab"][si, ai])

    def points_of(self, bones, most=160):
        """The mesh's rest points carried by `bones` (thinned to `most`), with the bone each rides."""
        ids = [self.sk.ix[b] for b in bones if b in self.sk.ix]
        sel = np.where(np.isin(self.part, ids))[0]
        if len(sel) > most:
            order = np.argsort(self.P[sel, 0], kind="stable")
            sel = sel[order][np.linspace(0, len(sel) - 1, most).astype(int)]
        return self.P[sel], self.part[sel]

    @staticmethod
    def world(pts, part, G, X, H0):
        return X[part] + np.einsum("nij,nj->ni", G[part], pts - H0[part])


TRUNK_SHAPES = ("pelvis", "spine_01", "spine_02", "chest", "pouch_l", "pouch_r")
LEG_SHAPES = ("thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r", "foot_r")
HIPS_LEGS = ("pelvis", "pouch_l", "pouch_r") + LEG_SHAPES      # (lift_needed's PITCH_LEGS: his hips, hip kit, legs)


# ================================================================================================ one file
def held_across(fit, g):
    """Whether gait g's low hold and its steadiness (low, steady) are for this man: a rifleman's own two-handed hold,
    or one of LOW_TWO's where the gait has low_two. A heading of its own is the riflemen's only (heading_for)."""
    if fit.name not in TWO_HANDS:
        return True
    return fit.name in LOW_TWO and bool(g.get("low_two"))


def heading_for(fit, g):
    """The heading gait g turns his weapon to (degrees from ahead, left -), or None: the riflemen's only (a two-handed
    man's long gun or load is kept at his own clip's heading, steady where the gait says so)."""
    return g.get("heading") if fit.name not in TWO_HANDS else None


def carry_of(name, D, g):
    """own, trail, limp or hang: what this file does with his arms in gait g."""
    if g["rifle"] == "own":
        return "own"
    C = D["clips"]["run"]
    if name in TWO_HANDS or C.get("weapon") in (None, "none"):
        return "own"
    return g["rifle"]


class Fit:
    """One soldier file: his skeleton, his run and advance as run_styles.plan reads them, his hands, his weapon."""

    def __init__(self, name, D, cuts, runs, log=print):
        self.name, self.D = name, D
        self.sk = sk = R.Skel(D["bones"])
        self.cuts, self.rest = cuts, cuts["_rest"]
        quiet = lambda *_: None
        self.plans = {"run": R.plan(D, "run", R.Capture(runs["run_a"], runs["_rest"]), log=quiet),
                      "advance": R.plan(D, "advance", R.Capture(runs["advance_a"], runs["_rest"]), log=quiet)}
        self.k = self.plans["run"]["k"]
        self.hands = Hands(sk)
        self.body = Body(D, sk)
        self.run = D["clips"]["run"]
        self.run_speed = float(self.run["stride"]) / float(self.run["long"])
        self.own = R.ball_slide(sk, self.run)
        self.own_per = self.own["slide"] / self.own["speed"]
        self.own_ahead = self.own["ahead"] / self.own["speed"]
        ft = self.plans["run"]["feet"]
        self.ground = 0.5 * (ft["l"]["ground"] + ft["r"]["ground"])
        self._weapon()
        self._holds = {}
        p = self.plans["run"]
        log(f"   his run: {self.run['long']} s, stride {self.run['stride']} m ({self.run_speed:.2f} m/s), legs x{self.k:.4f} "
            f"of the reference man's, arms {'free' if p['free_arms'] else 'carried'}, the weapon rides {p['anchor']}; "
            f"a planted ball slides {self.own_per:.4f} per m/s; weapon '{self.run.get('weapon')}'"
            + (f", {self.w_long:.2f} m long, its balance {self.w_bal_share:.2f} of the way from the butt" if self.w_pts is not None else ""))

    def _weapon(self):
        sk, body = self.sk, self.body
        # (a child bone of the weapon that his run moves far from it carries something that is not on the weapon:
        #  the loader's rocket in the bag on his back, the radio man's handset on his strap: a rider, kept on his
        #  body by keep_riders and left out of the weapon's points)
        run_tracks = {t["part_name"]: t for t in self.D["clips"]["run"]["tracks"]}
        self.riders = []
        for b in ("bolt", "bipod"):
            t = run_tracks.get(b)
            if b in sk.ix and t is not None:
                far = max([float(np.linalg.norm(k_[1:])) for k_ in (t.get("move") or [])] + [0.0])
                turn = max([math.degrees(2 * math.acos(min(1.0, abs(k_[1])))) for k_ in (t.get("rot") or [])] + [0.0])
                if far > 0.05 or turn > 20.0:
                    self.riders.append(b)
        ids = [sk.ix[b] for b in ("weapon", "bolt", "bipod") if b in sk.ix and b not in self.riders]
        sel = np.isin(body.part, ids)
        self.w_pts = None
        if "weapon" not in sk.ix or sel.sum() < 12:
            return
        P = body.P[sel]
        T = np.array(self.D["near"]["triangles"]).reshape(-1, 3)
        tw = T[sel[T].all(1)]
        v0, v1, v2 = body.P[tw[:, 0]], body.P[tw[:, 1]], body.P[tw[:, 2]]
        ar = 0.5 * np.linalg.norm(np.cross(v1 - v0, v2 - v0), axis=1)
        cen = ((v0 + v1 + v2) / 3 * ar[:, None]).sum(0) / max(float(ar.sum()), 1e-9)
        x0, x1 = float(P[:, 0].min()), float(P[:, 0].max())
        L = x1 - x0
        # the balance: a little ahead of the middle of its surface (the barrel is steel, the butt is wood), and
        # there or just beside it where the fist can close round it (the slimmest 5 cm of it)
        bal = min(max(cen[0] + 0.045 * L, x0 + 0.44 * L), x0 + 0.50 * L)
        best = None
        for dx in np.arange(-0.04, 0.1201, 0.01):
            s = P[np.abs(P[:, 0] - (bal + dx)) < 0.025]
            if len(s) < 6:
                continue
            z0, z1 = np.percentile(s[:, 2], 4), np.percentile(s[:, 2], 96)
            cost = (z1 - z0) + 0.35 * abs(dx)
            if best is None or cost < best[0]:
                best = (cost, bal + dx, 0.5 * (z0 + z1), 0.5 * (s[:, 1].min() + s[:, 1].max()),
                        0.5 * (np.percentile(s[:, 1], 96) - np.percentile(s[:, 1], 4)), z1 - z0)
        self.w_pts, self.w_long = P, L
        self.w_part = body.part[sel]
        self.w_bal = np.array((best[1], best[3], best[2])) if best else np.array((bal, cen[1], cen[2]))
        self.w_half = float(best[4]) if best else 0.024
        self.w_tall = float(best[5]) if best else 0.07
        self.w_bal_share = (self.w_bal[0] - x0) / L
        order = np.argsort(P[:, 0], kind="stable")
        pick = order[np.linspace(0, len(P) - 1, min(len(P), 200)).astype(int)]
        self.w_test = (P[pick], self.w_part[pick])

    def cut(self, g):
        key = (g["cut"], bool(g.get("mirror")))
        if key not in self._holds:
            self._holds[key] = Cut(self.cuts[g["cut"]], self.rest, mirror=bool(g.get("mirror")))
        return self._holds[key]

    def weapon_clear(self, G, X):
        """The weapon's least clearance from his trunk, kit and legs, and from the ground."""
        if self.w_pts is None:
            return 9.0, None, 9.0
        W = Body.world(self.w_test[0], self.w_test[1], G, X, self.sk.H0)
        c, which = self.body.clear(W, G, X, TRUNK_SHAPES + LEG_SHAPES)
        return c, which, float(W[:, 2].min())

    def fists_on_weapon(self, clip, ts):
        """Each fist's place in the weapon's own frame at the times ts: {side: (len(ts), 3)}."""
        sk = self.sk
        w = sk.ix["weapon"]
        out = {s: [] for s in "lr"}
        for t in ts:
            G, X = sk.fk(*R.clip_pose(clip, t))
            for s in "lr":
                out[s].append(G[w].T @ (self.hands.grip(G, X, s, 0.022, 0.075) - X[w]))
        return {s: np.array(v) for s, v in out.items()}


def track_names(fit, C, extra=()):
    """The new clip's tracks: his own clip's, with any of `extra` he has no track for, in the bones' order."""
    sk = fit.sk
    have = [tr["part_name"] for tr in C["tracks"]]
    names = sorted(set(have) | {n for n in extra if n in sk.ix}, key=lambda n: sk.ix[n])
    proto = {tr["part_name"]: tr for tr in C["tracks"]}
    return names, proto


def swing_signal(fit, cut, ts, s, loop=True):
    """How far the capture's own arm `s` is swung at each key: its angle ahead of straight down, as -1 to 1 about its
    mean (so what is hung by IK swings in step with the runner's own arm)."""
    sk = fit.sk
    chain = ("pelvis", "spine_01", "spine_02", "chest", "clavicle_" + s, "upperarm_" + s, "lowerarm_" + s, "hand_" + s)
    phi = []
    for t in ts:
        q = {n: cut.local(n, sk.pname[sk.ix[n]], t * FPS) for n in chain}
        G, X = sk.fk(q, {})
        v = X[sk.ix["hand_" + s]] - X[sk.ix["upperarm_" + s]]
        phi.append(math.atan2(v[0], -v[2]))
    phi = np.unwrap(np.array(phi))
    if loop:
        phi = phi[:-1]
    d = phi - phi.mean()
    d = d / max(float(np.abs(d).max()), 1e-6)
    return np.append(d, d[0]) if loop else d


def hang_arm(fit, q, mv, s, sig, par, out, grip_palm=None):
    """Arm `s` hung by IK from its shoulder: the wrist `reach` of the arm's length away, `back` degrees ahead of
    straight down (swung `swing` more by sig) and `out` to the side. With grip_palm, the hand is turned to carry the
    rifle at the trail (the fist's hollow along the rifle: muzzle forward, `pitch` down, `yaw` inward) and the place
    the rifle runs through the fist is returned with the rifle's line; without, the hand hangs open along the forearm."""
    sk, hands = fit.sk, fit.hands
    A = hands.side[s]
    sgn = 1.0 if s == "r" else -1.0                       # (outward: +y for the right arm)
    G, X = sk.fk(q, mv)
    S = X[sk.ix["upperarm_" + s]]
    th = math.radians(par["back"] + par["swing"] * sig)
    L = par["reach"] * hands.reach(s)
    hv = unit((math.sin(th), 0.0, -math.cos(th)))
    hv = unit(hv * math.sqrt(max(L * L - out * out, 1e-6)) + np.array((0.0, sgn * out, 0.0)))
    Wt = S + hv * L
    pole = np.array((-1.0, sgn * 0.35, 0.1))
    if grip_palm is None:
        d = hv
        fwd = unit(np.array((math.cos(th), -sgn * 0.25, math.sin(th))))
        for _ in range(2):
            Gh = frame_map(A["d0"], A["t0"], d, fwd)
            Tc = hands.arm(q, mv, s, Wt, pole, Gh)
            G, X = sk.fk(q, mv)
            d = unit(X[sk.ix["hand_" + s]] - X[sk.ix["lowerarm_" + s]])
        return None, None
    p = math.radians(par["pitch"] + par["rock"] * sig)
    y = math.radians(par["yaw"])
    a = unit((math.cos(p) * math.cos(y), -sgn * math.sin(y) * math.cos(p), -math.sin(p)))
    d = unit(hv - a * (hv @ a))
    for _ in range(3):
        Gh = frame_map(A["d0"], A["t0"], d, a)
        hands.arm(q, mv, s, Wt, pole, Gh)
        G, X = sk.fk(q, mv)
        fore = unit(X[sk.ix["hand_" + s]] - X[sk.ix["lowerarm_" + s]])
        d = unit(fore - a * (fore @ a))
    return hands.grip(G, X, s, grip_palm), a


def place_weapon(fit, q, mv, at, a, roll=0.0):
    """The weapon bone (a child of the root) keyed so that its balance is at `at` and its line along `a`, upright."""
    sk = fit.sk
    Gw = frame_map(np.array((1.0, 0.0, 0.0)), np.array((0.0, 0.0, 1.0)), a, np.array((0.0, 0.0, 1.0)))
    if roll:
        Gw = r_axis(a, roll) @ Gw
    w, r = sk.ix["weapon"], sk.ix["root"]
    Xw = np.asarray(at, float) - Gw @ (fit.w_bal - sk.H0[w])
    q["weapon"] = Gw
    mv["weapon"] = Xw - sk.H0[r] - (sk.H0[w] - sk.H0[r])


POLE_ANGLES = np.arange(20.0, 100.1, 10.0)   # (the elbow's side for the hand on the thigh: from behind to out)
POLE_STEP_MOST = 20.0                         # (degrees the elbow's side may move between two keys)


def pole_vec(a, sgn):
    a = math.radians(a)
    return np.array((-math.cos(a), sgn * math.sin(a), 0.12))


def pole_path(cl, tol, loop=True):
    """The elbow's side per key (indices into POLE_ANGLES) from the forearm's clearance cl[key, angle]: no key
    deeper into his kit than `tol` where any side lets it by, then as little moving between keys as can be, and
    among equals the clearer (a shortest path round the loop)."""
    K, A = cl.shape
    ang = POLE_ANGLES
    cost = np.where(cl < -tol, 1000.0 * (-tol - cl) + 10.0, 0.0) - 2.0 * np.minimum(cl, 0.03)
    d = np.abs(np.subtract.outer(ang, ang))
    step = np.where(d > POLE_STEP_MOST + 1e-6, np.inf, 0.04 * d)
    best = None
    for a0 in range(A):
        Dv = np.full(A, np.inf)
        Dv[a0] = cost[0, a0]
        back = []
        for j in range(1, K):
            M = Dv[:, None] + step
            back.append(M.argmin(0))
            Dv = M.min(0) + cost[j]
        tot = Dv + (step[:, a0] if loop else 0.0)
        e = int(np.argmin(tot))
        if not np.isfinite(tot[e]):
            continue
        if best is None or tot[e] < best[0] - 1e-9:
            path = [e]
            for arg in reversed(back):
                path.append(int(arg[path[-1]]))
            best = (float(tot[e]), path[::-1])
    return best[1]


SET_OUT_MOST = 0.08          # (hand_on_thigh: the laid hand set out from the thigh at most this far, to pass his kit
#                              at the hip: a winter coat's skirt and the pouches over it take 7 or 8 cm; the second
#                              review wanted the hand on the hurt thigh there too, not hanging)


def hand_on_thigh(fit, q, mv, s, down, pole=None):
    """Hand `s` laid flat on the outside of its own thigh, a little to the front, `down` metres from the hip (less
    where the arm does not reach); then, wherever a part of the hand or forearm would still lie inside his thigh,
    hip or kit (a pocket, a pouch), the hand set that much further out. Returns how far down it lies and how far
    the palm is from where it was laid (0 when it rests there). `pole`: the side the elbow points to, degrees
    round from straight behind him to out at his side (90) and a little before it (key_gait chooses it per key,
    eased between keys, so the elbow does not jump); without, the best of POLE_ANGLES."""
    sk, hands, body = fit.sk, fit.hands, fit.body
    A = hands.side[s]
    sgn = 1.0 if s == "r" else -1.0
    th = "thigh_" + s
    i = sk.ix[th]
    axis = body.shapes[th]["axis"]
    dr = unit(np.array((0.15, sgn * 0.99, 0.0)))        # (the outside of the thigh, a little to the front: the
    dr = unit(dr - axis * (dr @ axis))                    #  thumb then lies forward along it, not under a tunic)
    G, X = sk.fk(q, mv)
    S = X[sk.ix["upperarm_" + s]]
    reach = 0.985 * hands.reach(s)
    poles = [pole_vec(a_, sgn) for a_ in (POLE_ANGLES if pole is None else [pole])]
    if not hasattr(fit, "_hand_pts"):
        fit._hand_pts = {x: fit.body.points_of([f"{b}_{x}" for b in ("hand", "fingers", "thumb")], 90) for x in "lr"}
        fit._fore_pts = {x: fit.body.points_of(["lowerarm_" + x], 60) for x in "lr"}

    def clear_of(pts, q_):
        G_, X_ = sk.fk(q_, mv)
        W = Body.world(pts[0], pts[1], G_, X_, sk.H0)
        return body.clear(W, G_, X_, TRUNK_SHAPES + ("thigh_l", "thigh_r"))[0]
    got = None
    for dn in np.arange(down, 0.0199, -0.01):
        p = X[i] + G[i] @ (axis * dn + dr * (body.radius(th, dn, dr) + 0.022))
        Gh = frame_map(A["d0"], A["n0"], unit(G[i] @ (axis + 0.14 * dr) + 0.0), -(G[i] @ dr))
        Wt = p - Gh @ (A["d0"] * 0.07 + A["n0"] * 0.012)
        got = dn
        if np.linalg.norm(Wt - S) <= reach:
            break
    dn = got
    extra = 0.0
    q0 = dict(q)
    for _ in range(7):
        q.update(q0)
        G, X = sk.fk(q, mv)
        p = X[i] + G[i] @ (axis * dn + dr * (body.radius(th, dn, dr) + 0.022 + extra))
        n = -(G[i] @ dr)
        d = G[i] @ unit(axis + 0.14 * dr)                # (the fingers down the thigh's outside, along it)
        d = unit(d - n * (d @ n))
        Gh = frame_map(A["d0"], A["n0"], d, n)
        Wt = p - Gh @ (A["d0"] * 0.07 + A["n0"] * 0.012)
        best = None
        for pl in poles:                                  # (the elbow where the forearm passes his hip's kit)
            hands.arm(q, mv, s, Wt, pl, Gh, share=0.6)
            cf = clear_of(fit._fore_pts[s], q)
            if best is None or cf > best[0] + 0.002:
                best = (cf, pl)
        hands.arm(q, mv, s, Wt, best[1], Gh, share=0.6)
        hands.fingers(q, s, -40.0, -18.0)                 # (the rest hand is curled: laid flat on the thigh)
        c = clear_of(fit._hand_pts[s], q)
        fore_short = -(LAID_HAND - 0.004) - best[0]       # (the forearm still in his hip's kit: the hand set out
        #                                                    from the thigh till it passes; and the whole forearm and
        #                                                    hand as check_clip measures them)
        all_short = (-(LAID_HAND - 0.004) - free_arm_clear(fit, q, mv, s)[0]) if hasattr(fit, "_free_pts") else 0.0
        if (c >= -0.012 and fore_short <= 0.0 and all_short <= 0.0) or extra >= SET_OUT_MOST:
            break
        extra = min(SET_OUT_MOST, extra + max(-c - 0.008, fore_short + 0.004, all_short + 0.004, 0.004))
    G, X = sk.fk(q, mv)
    palm = X[sk.ix["hand_" + s]] + G[sk.ix["hand_" + s]] @ (A["d0"] * 0.07 + A["n0"] * 0.012)
    arm_in = free_arm_clear(fit, q, mv, s)[0] if hasattr(fit, "_free_pts") else c   # (as check_clip measures it)
    return dn, float(np.linalg.norm(palm - p)), round(extra, 3), float(best[0]), float(arm_in)


OPEN_MOST = 15.0             # (the free arm opened out from his side at most this far, degrees: the second review
#                              saw 25 to 36 degrees as a wing flung out, not a pumping arm)
FLEX_MOST = 60.0             # (and its elbow bent at most this much more, to 120 degrees of bend at most: a sprinter's
ELBOW_MOST = 120.0           #  elbow is about square)
FLEX_COST = 0.25             # (a degree of elbow bend counts as this much of a degree opened out: bend first)
TWIST_MOST = 30.0            # (and the upper arm turned about its own length at most this far, the forearm and hand
TWIST_COST = 0.6             #  carried in across his chest (or out past it, 10 at most) with the elbow where it was;
TWIST_RANGE = (-30.0, 10.0)  #  a degree of it counts as this much of a degree opened out)
HAND_MID_MOST = 0.25         # (the free hand kept within this of his midline, sideways, where any way that clears does)
OPEN_LAST = 30.0             # (only where nothing within OPEN_MOST and TWIST_RANGE clears his kit (a chest rig, a
TWIST_LAST = 30.0            #  greatcoat's pouches): opened out to this at most and the forearm carried out to this,
LAST_COST = 200.0            #  each such way counted this much dearer than any within them; check_clip reports it)
CALM_STEPS = (0.0, 0.2, 0.4, 0.6, 0.8, 1.0)   # (and the upper arm's swing drawn toward its middle by these shares,
CALM_COST = 25.0             #  fore and back, where his kit at the hip or on his chest stops it: a man in webbing
#                              pumps shorter; a share of it counts as this many degrees opened out: before opening
#                              wide; up to the whole swing at a key where nothing less clears his chest kit: the third
#                              mending, the hand had gone 35 to 42 cm out from his midline on CHN and JAP)


def open_out(fit, q, mv, s, deg):
    """The upper arm `s` opened out from the body by `deg` (OPEN_LAST at most) about his forward line."""
    if not deg:
        return
    sk = fit.sk
    sgn = 1.0 if s == "r" else -1.0
    G, X = sk.fk(q, mv)
    ua = sk.ix["upperarm_" + s]
    Gu = r_axis((1.0, 0.0, 0.0), sgn * min(deg, OPEN_LAST)) @ G[ua]
    q["upperarm_" + s] = G[sk.parent[ua]].T @ Gu


def flex_elbow(fit, q, mv, s, deg):
    """The elbow `s` bent `deg` more (the forearm and the hand turned about the elbow's hinge toward the upper arm),
    to ELBOW_MOST at most: a pumping arm carried higher and tighter, so it swings past his hip kit and his chest
    without being thrown out from his side."""
    if not deg:
        return 0.0
    sk = fit.sk
    G, X = sk.fk(q, mv)
    ua, la, ha = (sk.ix[n + s] for n in ("upperarm_", "lowerarm_", "hand_"))
    up, lo = X[la] - X[ua], X[ha] - X[la]
    bend = math.degrees(math.acos(max(-1.0, min(1.0, float(unit(up) @ unit(lo))))))
    deg = min(deg, max(0.0, ELBOW_MOST - bend))
    if deg <= 0:
        return 0.0
    ax = np.cross(up, lo)
    if np.linalg.norm(ax) < 1e-6:
        ax = G[ua] @ fit.hands.side[s]["bend0"]
    q["lowerarm_" + s] = G[ua].T @ (r_axis(ax, deg) @ G[la])
    return deg


def twist_out(fit, q, mv, s, deg):
    """The upper arm `s` turned about its own length by `deg` (TWIST_MOST at most either way) so that the forearm and
    the hand go out from his chest (+: the shoulder's outward turn) or in across it (-): the elbow stays where it is,
    the forearm swings past or across his chest instead of the whole arm being thrown out from his side."""
    if not deg:
        return
    sk = fit.sk
    G, X = sk.fk(q, mv)
    ua, la = sk.ix["upperarm_" + s], sk.ix["lowerarm_" + s]
    ax = X[la] - X[ua]
    sgn = 1.0 if s == "l" else -1.0          # (about shoulder-to-elbow: + takes a left forearm to his left, -y)
    Gu = r_axis(ax, sgn * max(min(deg, TWIST_MOST), -TWIST_MOST)) @ G[ua]
    q["upperarm_" + s] = G[sk.parent[ua]].T @ Gu


def lift_arms(q, deg):
    """Both arms (each clavicle and all below it, and so what rides a hand) turned forward and up by `deg` about his
    chest's own side-to-side line: the two shoulders' heads lie on that line, so the hands keep their hold."""
    if not deg:
        return
    Ry = r_axis((0.0, -1.0, 0.0), deg)
    for s in "lr":
        n = "clavicle_" + s
        q[n] = Ry @ q.get(n, np.eye(3))


def pitch_of(G):
    """How far a bone's up leans forward of upright, degrees."""
    u = G[:, 2]
    return math.degrees(math.atan2(u[0], u[2]))


LIFT_EXTRA_MOST = 10.0       # (degrees the arms may be lifted past what keeps his own clip's weapon line, or lowered)
PITCH_KEEP = 5.0             # (the weapon's pitch kept within this of his own clip's at the same moment, degrees)
PITCH_LEGS = 12.0            # (but where no lift within PITCH_KEEP keeps it out of his hips and legs, as far as
#                              this: a weapon held across him at the port, the BAR's butt over a limping man's thigh,
#                              an MG 42's at his hip, held a little more upright rather than through him)
HEAD_GAP = 0.055             # (a hand or forearm kept this far from his head, or as far as his own clip keeps it)
NOD_MOST = 21.0              # (where the arms cannot, his head turned up or down at most this far to keep it so)
UP_STEPS = (0.0, 4.0, 8.0, 12.0, 16.0, 20.0, 24.0, 28.0, 32.0)   # (and before that his trunk stood up so far,
#                                                                  degrees, the least that does)


def upright(q, deg):
    """His trunk stood up by `deg` (spine_01, spine_02 and chest a third each, leaning back about his side-to-side
    line), his head turned forward as much at the neck and the head, so it looks where it looked."""
    if not deg:
        return
    Ry = r_axis((0.0, -1.0, 0.0), deg / 3.0)
    for n in ("spine_01", "spine_02", "chest"):
        if n in q:
            q[n] = Ry @ q[n]
    Rn = r_axis((0.0, -1.0, 0.0), -deg / 2.0)
    for n in ("neck", "head"):
        if n in q:
            q[n] = Rn @ q[n]


def nod_head(q, deg):
    """His head turned chin-up (+) or chin-down about his chest's side-to-side line, half at the neck, half at the
    head: the face kept off a forearm that crosses before it (the bazooka's left arm to the tube on his right
    shoulder)."""
    if not deg:
        return
    Ry = r_axis((0.0, -1.0, 0.0), 0.5 * deg)
    for n in ("neck", "head"):
        if n in q:
            q[n] = Ry @ q[n]


def head_gap(fit, G, X, near=0.08):
    """The least distance from either forearm or hand to his head (its mesh, the helmet with it) in one pose: every
    point of both, the head's taken only where it lies within `near` of an arm's box (so past `near`, `near`)."""
    if not hasattr(fit, "_hh_pts"):
        fit._hh_pts = ({s_: fit.body.points_of([f"{b}_{s_}" for b in ("lowerarm", "hand", "fingers", "thumb")], 10 ** 6)
                        for s_ in "lr"}, fit.body.points_of(["head"], 10 ** 6))
    arms, (hp, hpart) = fit._hh_pts
    if not len(hp):
        return 9.0
    Hh = Body.world(hp, hpart, G, X, fit.sk.H0)
    best = near
    for s_ in "lr":
        ap, apart = arms[s_]
        if not len(ap):
            continue
        A = Body.world(ap, apart, G, X, fit.sk.H0)
        lo, hi = A.min(0) - best, A.max(0) + best
        H_ = Hh[np.all((Hh >= lo) & (Hh <= hi), axis=1)]
        if len(H_):
            best = min(best, float(np.sqrt(((A[:, None, :] - H_[None, :, :]) ** 2).sum(-1)).min()))
    return best


def lift_needed(fit, pose_at, nk, hold, loop=True, drop=0.0):
    """The arms' lift per key for a carry of his own on a body that leans more than his own clip's. First the lift
    that keeps the weapon at his own clip's pitch at the same moment of his step (so the arms and what they hold keep
    his clip's line to the ground: about the chest's lean past his own clip's); then, where the weapon would be
    deeper in his trunk, kit or legs than in his own clip AT THAT MOMENT (or nearer the ground than 5 cm), or a hand
    or forearm nearer his head than HEAD_GAP (or than his own clip at that moment), the least change that clears
    them: the arms lifted further or let down, a degree at a time, LIFT_EXTRA_MOST at most either way and never past
    PITCH_KEEP of his own clip's pitch (where none within it keeps the weapon out of his hips and legs, up to
    PITCH_LEGS: the weapon held a little more upright). (Measured against his whole clip's worst, a man whose own hold already rests
    the weapon on him, the bazooka on his shoulder, had his arms thrown up 45 degrees and the tube see-sawing;
    against his own clip at the same moment, what he does anyway is not counted.) Where that is not enough, his trunk
    stood up a little (UP_STEPS: so much less lean, so much less lift, his head kept looking where it looked: a man
    with a tube on his shoulder runs more upright), the least that does, eased between keys, and the lift found again
    on it; then, last, his head turned (NOD_MOST at most). Eased between keys, each key made sure of (where easing
    would put the weapon deeper than the key's own lift did, the key's own lift kept). `drop`: the arms carried that much
    lower than that first. `pose_at(j, lift, nod=0, up=0)` -> (q, mv, (Go, Xo)): the pose (his head turned by
    nod_head, his trunk stood up by upright), and his own clip's world turns and heads at that moment.
    lift_needed.nod and lift_needed.up per key."""
    sk = fit.sk
    ch = sk.ix["chest"]
    info = {"extra_deg": 0.0, "let_down_deg": 0.0, "pitch_off_deg": 0.0, "head_gap_cm": 9.0}
    legs_up = {}                     # ((key, up) -> how far past PITCH_KEEP's the weapon was held up off his legs)
    steps = sorted(np.arange(-LIFT_EXTRA_MOST, LIFT_EXTRA_MOST + 0.01, 1.0), key=lambda e: (abs(e), -e))
    own = []
    for j in range(nk):
        Go, Xo = pose_at(j, 0.0)[2]
        own_c, _, own_z = fit.weapon_clear(Go, Xo)
        own.append((Go, weapon_pitch(fit, Go), min(0.0, own_c) + 0.004, min(0.05, own_z),
                    min(HEAD_GAP, head_gap(fit, Go, Xo))))

    def at_up(j, up):
        """(lift, score, head gap, lift that keeps his pitch) at key j with his trunk stood up `up`."""
        Go, own_p, want, zwant, hwant = own[j]
        q, mv, _ = pose_at(j, 0.0, 0.0, up)
        G, X = sk.fk(q, mv)
        th0 = pitch_of(G[ch]) - pitch_of(Go[ch])
        lo = th0 - 12.0 if up else max(0.0, th0 - 12.0)       # (stood up, his chest can lean less than his own
        th0 = th0 if up else max(0.0, th0)                    #  clip's: then the arms come down to keep his pitch)
        best = None                    # (the lift that keeps his own pitch: about the chest's lean, a degree at a time)
        for th in np.arange(lo, max(lo, th0) + 12.01, 1.0):
            q, mv, _ = pose_at(j, float(th), 0.0, up)
            p = weapon_pitch(fit, sk.fk(q, mv)[0])
            if best is None or abs(p - own_p) < best[1] - 1e-9:
                best = (float(th), abs(p - own_p))
        th_keep = best[0] - drop
        got = None
        for extra in steps:
            th = th_keep + extra
            q, mv, _ = pose_at(j, float(th), 0.0, up)
            G, X = sk.fk(q, mv)
            if extra and abs(weapon_pitch(fit, G) - own_p) > PITCH_KEEP + 1e-6:
                continue
            c, which, zmin = fit.weapon_clear(G, X)
            hg = head_gap(fit, G, X)
            score = min(c - want, zmin - zwant, hg - hwant)
            if got is None or score > got[1] + 1e-4 or score >= 0.0:
                got = (th, score, hg)
                legs = score < 0.0 and abs(score - (c - want)) < 1e-9 and which in HIPS_LEGS
            if score >= 0.0:
                break
        legs_up.pop((j, up), None)
        if got[1] < 0.0 and legs:         # (none within PITCH_KEEP keeps it out of his legs: up to PITCH_LEGS)
            for extra in steps:
                th = th_keep + extra
                q, mv, _ = pose_at(j, float(th), 0.0, up)
                G, X = sk.fk(q, mv)
                dp = abs(weapon_pitch(fit, G) - own_p)
                if dp <= PITCH_KEEP + 1e-6 or dp > PITCH_LEGS + 1e-6:
                    continue
                c, which, zmin = fit.weapon_clear(G, X)
                hg = head_gap(fit, G, X)
                score = min(c - want, zmin - zwant, hg - hwant)
                if score > got[1] + 1e-4:
                    got = (th, score, hg)
                    legs_up[(j, up)] = round(dp, 1)
                if score >= 0.0:
                    break
        return got + (th_keep,)

    def weapon_score(j, th, up):
        """the weapon's own terms of the score at key j (the clearance from him and the ground)"""
        Go, own_p, want, zwant, hwant = own[j]
        q, mv, _ = pose_at(j, float(th), 0.0, up)
        G, X = sk.fk(q, mv)
        c, which, zmin = fit.weapon_clear(G, X)
        return min(c - want, zmin - zwant)

    def ease(v):
        if loop:
            return smooth_loop(v, 1)
        e = v.copy()
        e[1:-1] = 0.25 * v[:-2] + 0.5 * v[1:-1] + 0.25 * v[2:]
        return e

    def held(v):                                     # (held over its neighbours, then eased)
        if not np.any(v):
            return v
        b = np.where(np.abs(v) >= np.abs(np.roll(v, 1)), v, np.roll(v, 1))
        b = np.where(np.abs(b) >= np.abs(np.roll(v, -1)), b, np.roll(v, -1))
        if not loop:
            b[0], b[-1] = v[0], v[-1]
        return ease(b)
    # (how far his trunk is stood up at each key: the least of UP_STEPS that clears, held and eased)
    ups = np.zeros(nk)
    first = {}
    for j in range(nk):
        top = None
        for up in UP_STEPS:
            got = at_up(j, up)
            if top is None or got[1] > top[1][1] + 1e-4:
                top = (up, got)
            if got[1] >= 0.0:
                break
        ups[j] = top[0]
        first[j] = top
    ups = held(ups)
    ups = np.round(ups, 1)
    # (on that, the lift; then his head where it is still too near)
    need = np.zeros(nk)
    nod = np.zeros(nk)
    for j in range(nk):
        up = float(ups[j])
        got = first[j][1] if abs(first[j][0] - up) < 1e-6 else at_up(j, up)
        th, score, hg, th_keep = got
        need[j] = th
        hwant = own[j][4]
        if hg < hwant - 1e-4:
            g2 = (0.0, hg)
            for nd in sorted(np.arange(-NOD_MOST, NOD_MOST + 0.01, 3.0), key=lambda e: (abs(e), -e)):
                if not nd:
                    continue
                q, mv, _ = pose_at(j, float(th), float(nd), up)
                h_ = head_gap(fit, *sk.fk(q, mv))
                if h_ > g2[1] + 1e-4:
                    g2 = (float(nd), h_)
                if h_ >= hwant:
                    break
            nod[j], hg = g2
        info["extra_deg"] = max(info["extra_deg"], round(float(th - th_keep), 1))
        info["let_down_deg"] = max(info["let_down_deg"], round(float(th_keep - th), 1))
        info["head_gap_cm"] = min(info["head_gap_cm"], round(100 * hg, 1))
        if (j, up) in legs_up:
            info["pitch_off_deg"] = max(info["pitch_off_deg"], legs_up[(j, up)])
    nod = held(nod)
    info["nod_deg"] = [round(float(nod.min()), 1), round(float(nod.max()), 1)]
    info["up_deg"] = [round(float(ups.min()), 1), round(float(ups.max()), 1)]
    out = ease(need)
    kept = 0
    for j in range(nk):                   # (each key made sure of: eased is not always as clear)
        if abs(out[j] - need[j]) < 0.05:
            continue
        s_need = weapon_score(j, need[j], float(ups[j]))
        if weapon_score(j, out[j], float(ups[j])) < min(0.0, s_need) - 1e-4:
            out[j] = need[j]
            kept += 1
    info["keys_kept_uneased"] = kept
    lift_needed.info = info
    lift_needed.nod = nod
    lift_needed.up = ups
    return out


def weapon_pitch(fit, G):
    """How far the weapon's muzzle points above level, degrees."""
    ax = G[fit.sk.ix["weapon"]][:, 0]
    return math.degrees(math.asin(max(-1.0, min(1.0, ax[2]))))


def heading_off(fit, Go, Gn):
    """Degrees the weapon's heading over the ground (its line seen from above) must turn about the upright to be his
    own clip's (Go) from what it is now (Gn): + turns it from +x toward +y."""
    w = fit.sk.ix["weapon"]
    a, b = Go[w][:, 0], Gn[w][:, 0]
    if math.hypot(a[0], a[1]) < 0.2 or math.hypot(b[0], b[1]) < 0.2:
        return 0.0
    d = math.degrees(math.atan2(a[1], a[0]) - math.atan2(b[1], b[0]))
    return (d + 180.0) % 360.0 - 180.0


def rehold(fit, q, mv, tilt, lower, yaw=0.0):
    """His own two-handed hold turned and lowered as one: the weapon and both hands turned muzzle-down by `tilt`
    degrees about his right fist (in the upright plane along the weapon: it keeps its heading across him), turned
    `yaw` degrees about the upright through that fist (steady: back to his own clip's heading), and lowered `lower`
    metres; each arm set by IK to its turned hand (its elbow kept on its side), so each fist keeps its place on the
    weapon where the arm reaches."""
    if not tilt and not lower and not yaw:
        return
    sk, hands = fit.sk, fit.hands
    G, X = sk.fk(q, mv)
    w, r = sk.ix["weapon"], sk.ix["root"]
    p = hands.grip(G, X, "r", 0.022, 0.075)
    Rt = r_axis(np.cross((0.0, 0.0, 1.0), G[w][:, 0]), tilt)
    if yaw:
        Rt = r_axis((0.0, 0.0, 1.0), yaw) @ Rt
    d = np.array((0.0, 0.0, -lower))
    miss = 0.0
    for s in "lr":
        h = sk.ix["hand_" + s]
        pole = X[sk.ix["lowerarm_" + s]] - X[sk.ix["upperarm_" + s]]
        T = p + Rt @ (X[h] - p) + d
        Tc = hands.arm(q, mv, s, T, pole, Rt @ G[h])
        miss = max(miss, float(np.linalg.norm(np.asarray(Tc) - T)))
    fit._reach_miss = miss                    # (tilt_needed: a hold the arms cannot reach is not taken)
    Gw = Rt @ G[w]
    Xw = p + Rt @ (X[w] - p) + d
    q["weapon"] = Gw
    mv["weapon"] = Xw - sk.H0[r] - (sk.H0[w] - sk.H0[r])


REACH_MISS = 0.006           # (rehold: a turned hold whose wrist the arm misses by more than this is not taken)
LOW_MARGIN = 0.01            # (rehold: in a gait planted at 30 keys a second and kept at 15 ("dense"), a held-low
#                              weapon kept this much clearer of him at the keys than his own clip is, as the joints
#                              turning straight between the kept keys carry a swinging thigh up into it: the fourth
#                              mending, USA's dash had it 3.2 cm into his right thigh between two keys)


def tilt_needed(fit, pose_at, nk, hold, target, lower=0.0, loop=True, margin=0.0):
    """rehold's turn and drop per key: the weapon brought down to `target` degrees above level (none where it is
    already lower) and lowered `lower` metres, less of either wherever that would put it into him (as clear of his
    trunk, kit and legs as in his own clip, 5 cm over the ground) or out of his arms' reach (the drop given up 2 cm
    at a time first, then the turn), eased between keys. `pose_at(j, tilt, lower)` -> (q, mv). Returns (tilts,
    lowers)."""
    sk = fit.sk
    base = fit.base_clear(hold)
    want = min(0.0, base) + 0.004 + margin
    allowed = np.zeros(nk)
    dropped = np.zeros(nk)
    drops = [lower] if lower <= 0.0 else list(np.arange(lower, -0.001, -0.02))
    for j in range(nk):
        q, mv = pose_at(j, 0.0, 0.0)
        G, X = sk.fk(q, mv)
        t0 = max(0.0, weapon_pitch(fit, G) - target)
        got, best = None, None
        for tl in np.arange(t0, -0.01, -3.0):     # (the drop given up first, then the turn)
            for lw in drops:
                fit._reach_miss = 0.0
                q, mv = pose_at(j, tl, lw)
                G, X = sk.fk(q, mv)
                c, which, zmin = fit.weapon_clear(G, X)
                if zmin >= 0.05 and fit._reach_miss <= REACH_MISS:
                    if c >= want:
                        got = (tl, lw)
                        break
                    if best is None or c > best[0]:
                        best = (c, tl, lw)
            if got is not None:
                break
        if got is None:                       # (nothing clears as asked: the clearest of the holds tried)
            got = (best[1], best[2]) if best is not None else (0.0, 0.0)
        allowed[j] = max(got[0], 0.0)
        dropped[j] = max(got[1], 0.0)

    def ease(v):
        if loop:
            a = np.minimum(v, np.minimum(np.roll(v, 1), np.roll(v, -1)))
            return np.minimum(smooth_loop(a, 2), v)
        a = v.copy()
        a[1:-1] = np.minimum(v[1:-1], np.minimum(v[:-2], v[2:]))
        return np.minimum(smooth_loop(a, 2, loop=False), v)
    return ease(allowed), ease(dropped)


def keep_riders(fit, q, mv, tq, tm, on="chest"):
    """The weapon's riders (Fit._weapon) kept where his own clip (tq, tm) has them on his body, wherever the weapon
    has gone: the same place and turn in his chest's frame."""
    if not fit.riders:
        return
    sk = fit.sk
    Gn, Xn = sk.fk(q, mv)
    Go, Xo = sk.fk(tq, tm)
    c = sk.ix[on]
    for b in fit.riders:
        i = sk.ix[b]
        p = sk.parent[i]
        Gw = Gn[c] @ Go[c].T @ Go[i]
        Xw = Xn[c] + Gn[c] @ Go[c].T @ (Xo[i] - Xo[c])
        q[b] = Gn[p].T @ Gw
        mv[b] = Gn[p].T @ (Xw - Xn[p]) - (sk.H0[i] - sk.H0[p])


def hand_mid(fit, q, mv, s):
    """How far the hand `s` is out from his midline, sideways (the clip is in place, facing +x), metres."""
    sk = fit.sk
    G, X = sk.fk(q, mv)
    return abs(float(X[sk.ix["hand_" + s]][1] - X[sk.ix["pelvis"]][1]))


def free_arm_clear(fit, q, mv, s):
    sk = fit.sk
    pts, part = fit._free_pts[s]
    G, X = sk.fk(q, mv)
    W = Body.world(pts, part, G, X, sk.H0)
    return fit.body.clear(W, G, X, TRUNK_SHAPES + ("thigh_l", "thigh_r"))


def key_gait(fit, g, log=print, quick=1.0, legs_only=False, phase=0.0):
    """Gait g keyed on this file: the clip (planted, set down, with its events) and what was done. `quick`: the cut
    played that many times faster than its own time (bundle_for: to bring the clip to the speed asked of it on this
    man, a few per cent at most). `legs_only`: the arms not fitted (no searches): for the stride alone, which the
    arms do not change. `phase`: the loop begun this far (s) into the cut, so its keys fall where they are wanted
    (key_phase)."""
    sk, hands = fit.sk, fit.hands
    cut = fit.cut(g)
    carry = carry_of(fit.name, fit.D, g)
    P = fit.plans[g["hold"]]
    C = P["C"]
    k = fit.k
    loop = cut.loop
    long = round(cut.long / quick, 4)
    fr = (cut.F - 1) / long                  # (the cut's frames a second of this clip)
    dk = int(g.get("dense", 1))              # (keyed this many times as often, planted, then every dk-th key kept)
    K = max(2, int(round(long * g["rate"]))) * dk
    ts = [long * j / K for j in range(K + 1)]
    free = P["free_arms"]
    own_trunk = carry == "own" and not free
    delta = P["delta"] if own_trunk else {}
    arm_tracks = [n for n in ARMS if n in sk.ix]
    hand_tracks = [f"{b}_{s}" for s in "lr" for b in ("fingers", "thumb") if f"{b}_{s}" in sk.ix]
    extra = list(TRUNK + LEGS)
    if carry in ("trail", "limp", "hang"):
        extra += arm_tracks + hand_tracks
    elif not free:
        extra += ["clavicle_l", "clavicle_r"]
    names, proto = track_names(fit, C, extra)
    body = [n for n in TRUNK + LEGS if n in sk.ix] + ([n for n in arm_tracks if n in names] if free else [])
    info = {"carry": carry, "rate": g["rate"], "keys": K // dk + 1, "keys_a_second": round(K // dk / long, 2),
            "quick": round(quick, 4)}

    def body_pose(t):
        f = t * fr
        q = {}
        for n in body:
            L = cut.local(n, sk.pname[sk.ix[n]], f)
            if n in delta:
                L = L @ delta[n]
            q[n] = L
        return q, {"pelvis": k * cut.pelvis_at(f) + P["pelvis_c"]}

    # where his feet fall (the body alone decides it), so his own arms and kit can be played in step
    dense = [j / fr for j in range(cut.F - 1)]
    ft = R.feet(sk, [sk.fk(*body_pose(t)) for t in dense], long)
    lf = R.left_foot(ft, long)
    shift = P["lf"] - lf

    def old_t(t):
        return ((t / long + shift) % 1.0) * P["long"]

    sig = {}
    if carry in ("trail", "limp", "hang"):
        sig["r"] = swing_signal(fit, cut, [(t + phase) * fr / FPS for t in ts], "r")
        sig["l"] = swing_signal(fit, cut, [(t + phase) * fr / FPS for t in ts], "l")
        par = TRAIL[g.get("trail") or ("sprint" if carry == "trail" else "limp")]
        palm = fit.w_half + 0.008
        if not hasattr(fit, "_free_pts"):
            fit._free_pts = {s: fit.body.points_of([f"{b}_{s}" for b in ("lowerarm", "hand", "fingers", "thumb")], 120)
                             for s in "lr"}

    mid_ua = None
    if carry == "trail":                      # (the free upper arm's middle over the cut: calm_swing draws to it)
        pn = sk.pname[sk.ix["upperarm_l"]]
        qs = [R.m2q(cut.local("upperarm_l", pn, f)) for f in range(cut.F - 1)]
        q0_ = qs[0]
        qs = [x if x @ q0_ >= 0 else -x for x in qs]
        mid_ua = R.q2m(np.mean(qs, axis=0))

    left_hangs = [bool(g.get("mirror") or g.get("left_hangs"))]   # (limp: the left arm hangs, or its hand is on
    #                                                                 the thigh; hang: it hangs)
    thigh_at = {}
    thigh_pole = {}
    yaw_add = [0.0]

    def pose(j, out=None, opened=0.0, lift=0.0, tilt=0.0, lower=0.0, flex=0.0, twist=0.0, nod=0.0, up=0.0, calm=0.0):
        """Key j whole. `out`: the trail hand's distance out from the shoulder; `opened`, `flex`: the free arm opened
        out and its elbow bent more; `calm`: its upper arm's swing drawn that share toward its middle;
        `lift`: his own carry's arms turned up (lift_needed); `tilt`, `lower`: his own hold turned muzzle-down and
        lowered (rehold)."""
        t = ts[j] + phase
        q, mv = body_pose(t)
        tq, tm = R.clip_pose(C, old_t(t))
        note = {}
        if carry == "own":
            for n in names:
                if n not in q and n in tq and sk.pname[sk.ix[n]] != "root":
                    q[n] = tq[n]
                if n != "pelvis" and n in tm and sk.pname[sk.ix[n]] != "root":
                    mv[n] = tm[n]
            upright(q, up)
            lift_arms(q, lift)
            nod_head(q, nod)
            Gn, Xn = sk.fk(q, mv)
            Go, Xo = sk.fk(tq, tm)
            note["Go"], note["Xo"] = Go, Xo
            for n, a in P["anchor"].items():
                i, jx = sk.ix[n], sk.ix[a]
                Gw = Gn[jx] @ Go[jx].T @ Go[i]
                Xw = Xn[jx] + Gn[jx] @ Go[jx].T @ (Xo[i] - Xo[jx])
                r = sk.ix["root"]
                q[n] = Gw
                mv[n] = Xw - sk.H0[r] - (sk.H0[i] - sk.H0[r])
            yaw_c = 0.0
            hd_ = heading_for(fit, g)
            if (g.get("steady") or hd_ is not None) and "weapon" in q \
                    and fit.run.get("weapon") not in (None, "none") and held_across(fit, g):
                Gs, _ = sk.fk(q, mv)
                if hd_ is not None:                       # (a heading of its own: across him)
                    a_ = Gs[sk.ix["weapon"]][:, 0]
                    yaw_c = (hd_ - math.degrees(math.atan2(a_[1], a_[0])) + 180.0) % 360.0 - 180.0
                else:                                     # (his own clip's heading at this moment)
                    yaw_c = heading_off(fit, Go, Gs)
                note["yaw_c"] = yaw_c
            rehold(fit, q, mv, tilt, lower, yaw_c)
            keep_riders(fit, q, mv, tq, tm)
            return q, mv, note
        # his kit (the pouches) from his own clip in step; the arms made here
        for n in names:
            if n in q or sk.pname[sk.ix[n]] == "root" or n in arm_tracks or n in hand_tracks:
                continue
            if n in tq:
                q[n] = tq[n]
            if n in tm:
                mv[n] = tm[n]
        for n in ("clavicle_l", "clavicle_r"):
            q[n] = cut.local(n, sk.pname[sk.ix[n]], t * fr)
        # the right arm: the rifle at the trail
        at, a = hang_arm(fit, q, mv, "r", sig["r"][j], dict(par, yaw=par["yaw"] + yaw_add[0]), out, grip_palm=palm)
        hands.fingers(q, "r", GRIP_CURL, GRIP_THUMB)
        place_weapon(fit, q, mv, at, a)
        keep_riders(fit, q, mv, tq, tm)
        # the left arm
        if carry == "trail":
            for n in ("upperarm_l", "lowerarm_l", "hand_l"):
                q[n] = cut.local(n, sk.pname[sk.ix[n]], t * fr)
            if calm and mid_ua is not None:
                q["upperarm_l"] = R.q2m(R.nlerp(R.m2q(q["upperarm_l"]), R.m2q(mid_ua), calm))
            flex_elbow(fit, q, mv, "l", flex)
            twist_out(fit, q, mv, "l", twist)
            open_out(fit, q, mv, "l", opened)
            hands.fingers(q, "l", FREE_CURL, FREE_THUMB)
        elif left_hangs[0]:
            hang_arm(fit, q, mv, "l", sig["l"][j], par.get("left", dict(back=3.0, swing=9.0, reach=0.965)), opened)
            hands.fingers(q, "l", 30.0, 12.0)
        else:                                             # (the same at a key whatever the right arm does)
            if j not in thigh_at:
                q2 = dict(q)
                th_ = hand_on_thigh(fit, q2, mv, "l", THIGH_DOWN, pole=thigh_pole.get(j, thigh_pole.get("all")))
                thigh_at[j] = ({n: q2[n] for n in ("upperarm_l", "lowerarm_l", "hand_l", "fingers_l", "thumb_l")
                                if n in q2}, th_)
            q.update(thigh_at[j][0])
            note["thigh"] = thigh_at[j][1]
        return q, mv, note

    # ---- his own carry on a body that leans further than his own clip: the arms lifted to keep the weapon clear
    lift = np.zeros(K + 1)
    nodded = np.zeros(K + 1)
    stood = np.zeros(K + 1)
    if carry == "own" and not free and fit.w_pts is not None and fit.run.get("weapon") not in (None, "none") \
            and not legs_only:
        def pose_at(j, th, nd=0.0, up=0.0):
            q, mv, note = pose(j, lift=th, nod=nd, up=up)
            return q, mv, (note["Go"], note["Xo"])
        lv = lift_needed(fit, pose_at, K, g["hold"], loop=True, drop=g.get("drop", 0.0))
        lift[:K] = lv
        lift[K] = lv[0]
        nodded[:K] = lift_needed.nod
        nodded[K] = lift_needed.nod[0]
        stood[:K] = lift_needed.up
        stood[K] = lift_needed.up[0]
        if np.any(nodded):
            info["head_turned_deg"] = lift_needed.info["nod_deg"]
        if np.any(stood):
            info["trunk_stood_up_deg"] = lift_needed.info["up_deg"]
        info["arms_lifted_deg"] = [round(float(lv.min()), 1), round(float(lv.max()), 1)]
        info["arms_lifted_past_his_line_deg"] = lift_needed.info["extra_deg"]
        info["arms_let_down_from_his_line_deg"] = lift_needed.info["let_down_deg"]
        info["hands_head_at_keys_cm"] = lift_needed.info["head_gap_cm"]
        if lift_needed.info["pitch_off_deg"]:
            info["weapon_up_off_his_legs_deg"] = lift_needed.info["pitch_off_deg"]
    tilt = np.zeros(K + 1)
    lower = np.zeros(K + 1)
    if "low" in g and carry == "own" and not free and fit.w_pts is not None \
            and fit.run.get("weapon") not in (None, "none") and not legs_only and held_across(fit, g):
        target, drop = g["low"] if fit.name not in TWO_HANDS else g["low_two"]
        tv, lv_ = tilt_needed(fit, lambda j, tl, lw: pose(j, lift=float(lift[j]), tilt=tl, lower=lw,
                                                          nod=float(nodded[j]), up=float(stood[j]))[:2], K,
                              g["hold"], target, lower=drop, loop=True,
                              margin=LOW_MARGIN if int(g.get("dense", 1)) > 1 else 0.0)
        tilt[:K] = tv
        tilt[K] = tv[0]
        lower[:K] = lv_
        lower[K] = lv_[0]
        info["held_low"] = {"tilt_deg": [round(float(tv.min()), 1), round(float(tv.max()), 1)],
                            "lowered_m": [round(float(lv_.min()), 3), round(float(lv_.max()), 3)],
                            "asked": [target, drop], "muzzle_up_to_deg": target}
        if heading_for(fit, g) is not None:
            info["held_low"]["heading_deg"] = heading_for(fit, g)

    # ---- the limp's hand on the thigh: the elbow's side at each key where the forearm passes his hip kit, eased
    #      between keys (pole_path)
    if carry == "limp" and not left_hangs[0] and not legs_only:
        cl = np.zeros((K, len(POLE_ANGLES)))
        for a_i, a_ in enumerate(POLE_ANGLES):
            thigh_pole.clear()
            thigh_pole["all"] = float(a_)
            thigh_at.clear()
            for j in range(K):
                cl[j, a_i] = pose(j, par["out0"])[2]["thigh"][3]
        path = pole_path(cl, LAID_HAND)
        thigh_pole.clear()
        thigh_pole.update({j: float(POLE_ANGLES[path[j]]) for j in range(K)})
        thigh_at.clear()
        info["thigh_elbow_side_deg"] = [float(POLE_ANGLES[min(path)]), float(POLE_ANGLES[max(path)])]

    # ---- the trail: out from the thigh just as far as clears it, the free arm opened where it would pass through
    out = None
    opened = np.zeros(K + 1)
    flexed = np.zeros(K + 1)
    twisted = np.zeros(K + 1)
    calmed = np.zeros(K + 1)
    if carry in ("trail", "limp", "hang") and legs_only:
        out = par["out0"]
    elif carry in ("trail", "limp", "hang"):
        # (out from his thigh as little as clears it; where his kit at the hip still takes the butt, the muzzle
        #  turned a little in, so the butt swings out past it)
        best = None
        for ya in (0.0, 5.0, 10.0):
            yaw_add[0] = ya
            for out_try in np.arange(par["out0"], 0.2401, 0.01):
                worst, low = 9.0, 9.0
                for j in range(K):
                    q, mv, _ = pose(j, out_try)
                    G, X = sk.fk(q, mv)
                    c, which, zmin = fit.weapon_clear(G, X)
                    worst, low = min(worst, c), min(low, zmin)
                if best is None or worst > best[1]:
                    best = (out_try, worst, low, ya)
                if worst >= par["clear"]:
                    break
            if best[1] >= par["clear"]:
                break
        out = float(best[0])
        yaw_add[0] = best[3]
        info["trail_out"] = round(out, 3)
        info["trail_yaw_in_deg"] = par["yaw"] + best[3]
        if carry == "limp" and not left_hangs[0]:
            # (the hand to the hurt thigh where his kit at the hip lets the forearm by; else it hangs, as limp_r's)
            fore = min(pose(j, out)[2]["thigh"][3] for j in range(K))
            palm_in = min(pose(j, out)[2]["thigh"][4] for j in range(K))
            if fore < -LAID_HAND:
                left_hangs[0] = True
                info["left_hand"] = (f"hangs: his kit at the left hip would have the forearm {-fore * 100:.0f} cm "
                                     f"into it on the way to the thigh")
            elif palm_in < -LAID_HAND:
                left_hangs[0] = True
                info["left_hand"] = (f"hangs: his kit at the left hip would hold the hand or forearm "
                                     f"{-palm_in * 100:.0f} cm into it, the hand set out as far as it may be")
        if carry == "trail":
            # (the free arm where the capture's would pass into his kit or trunk: its elbow bent more first, the
            #  forearm carried out past his chest, then the upper arm opened out from his side, the least of these
            #  that clears him, eased between keys)
            def cost(c_):
                last = c_[0] > OPEN_MOST + 1e-6 or c_[2] > TWIST_RANGE[1] + 1e-6
                return (LAST_COST if last else 0.0) + c_[0] + FLEX_COST * c_[1] + TWIST_COST * abs(c_[2]) \
                    + CALM_COST * c_[3]
            combos = sorted(((float(o_), float(f_), float(w_), float(k_))
                             for o_ in np.arange(0.0, OPEN_LAST + 0.01, 5.0)
                             for f_ in np.arange(0.0, FLEX_MOST + 0.01, 10.0)
                             for w_ in np.arange(TWIST_RANGE[0], TWIST_LAST + 0.01, 10.0)
                             for k_ in CALM_STEPS),
                            key=lambda c_: (cost(c_), c_[0], c_[3], abs(c_[2])))

            def clear_at(j, o_, f_, w_, k_, mid=False):
                q, mv, _ = pose(j, out, o_, flex=f_, twist=w_, calm=k_)
                c_ = free_arm_clear(fit, q, mv, "l")[0]
                return (c_, hand_mid(fit, q, mv, "l")) if mid else c_

            def first_clear(j):
                best_, wide_ = None, None
                for o_, f_, w_, k_ in combos:
                    if wide_ is not None and cost((o_, f_, w_, k_)) >= LAST_COST:
                        return wide_                 # (a way within OPEN_MOST that clears, the hand wide, before
                    c_, hm = clear_at(j, o_, f_, w_, k_, mid=True)   # any opened past it)
                    if c_ >= FREE_ARM_CLEAR:
                        if hm <= HAND_MID_MOST:
                            return o_, f_, w_, k_, c_
                        if wide_ is None:
                            wide_ = (o_, f_, w_, k_, c_)
                    if best_ is None or c_ > best_[4] + 1e-4:
                        best_ = (o_, f_, w_, k_, c_)
                return wide_ or best_
            chosen = [first_clear(j) for j in range(K)]
            o_k = np.array([c_[0] for c_ in chosen])
            f_k = np.array([c_[1] for c_ in chosen])
            w_k = np.array([c_[2] for c_ in chosen])
            k_k = np.array([c_[3] for c_ in chosen])

            def ease(v):
                n_ = np.maximum(v, np.maximum(np.roll(v, 1), np.roll(v, -1)))
                return np.maximum(smooth_loop(n_, 2), v)
            o_s, f_s, w_s, k_s = ease(o_k), ease(f_k), ease(w_k), ease(k_k)
            worst = 9.0
            for j in range(K):                       # (each key made sure of: eased is not always clearer)
                c_ = clear_at(j, float(o_s[j]), float(f_s[j]), float(w_s[j]), float(k_s[j]))
                if c_ < min(FREE_ARM_CLEAR, chosen[j][4]) - 1e-9:
                    o_s[j], f_s[j], w_s[j], k_s[j], c_ = chosen[j]
                worst = min(worst, c_)
            opened[:K], opened[K] = o_s, o_s[0]
            flexed[:K], flexed[K] = f_s, f_s[0]
            twisted[:K], twisted[K] = w_s, w_s[0]
            calmed[:K], calmed[K] = k_s, k_s[0]
            info["free_arm_swing_calmed"] = round(float(k_s.max()), 2)
            info["free_arm_opened_deg"] = round(float(o_s.max()), 1)
            info["free_arm_elbow_bent_deg"] = round(float(f_s.max()), 1)
            info["free_arm_forearm_out_deg"] = round(float(w_s.max()), 1)
            info["free_arm_forearm_in_deg"] = round(float(-w_s.min()), 1)
            info["free_arm_clear_cm"] = round(100 * worst, 1)
        elif left_hangs[0]:
            tries = np.arange(0.03, 0.2801, 0.01)

            def first_clear(j, lo=-1.0):
                best_ = None
                for v in tries:
                    if v < lo - 1e-9:
                        continue
                    q, mv, _ = pose(j, out, v)
                    c_ = free_arm_clear(fit, q, mv, "l")[0]
                    if c_ >= FREE_ARM_CLEAR:
                        return v
                    if best_ is None or c_ > best_[1] + 1e-4:
                        best_ = (v, c_)
                return best_[0]
            need = np.zeros(K + 1)
            for j in range(K):
                need[j] = first_clear(j)
            opened[:] = float(need[:K].max())
            info["free_arm_out"] = round(float(opened[0]), 3)

    # ---- the keys
    keys = {n: {"rot": [], "move": []} for n in names}
    has_move = {n: bool(proto[n].get("move")) if n in proto else False for n in names}
    thigh = []
    for j in range(K + 1):
        jj = j if j < K or not loop else 0
        q, mv, note = pose(jj, out, float(opened[jj]), float(lift[jj]), float(tilt[jj]), float(lower[jj]), float(flexed[jj]),
                           float(twisted[jj]), float(nodded[jj]), float(stood[jj]), float(calmed[jj]))
        if "thigh" in note:
            thigh.append(note["thigh"])
        t = ts[j]
        for n in names:
            v = R.m2q(q.get(n, np.eye(3)))
            prev = keys[n]["rot"][-1][1:] if keys[n]["rot"] else None
            if prev is not None and np.dot(prev, v) < 0:
                v = -v
            keys[n]["rot"].append([round(t, 4)] + [round(float(x), 5) for x in v])
            if has_move[n]:
                keys[n]["move"].append([round(t, 4)] + [round(float(x), 4) for x in mv.get(n, np.zeros(3))])
    info["left_hangs"] = left_hangs[0] if carry in ("limp", "hang") else None
    if (g.get("steady") or heading_for(fit, g) is not None) and carry == "own" and fit.w_pts is not None \
            and fit.run.get("weapon") not in (None, "none") and not legs_only:
        yc = [pose(j, out, float(opened[j]), float(lift[j]), float(tilt[j]), float(lower[j]), float(flexed[j]), float(twisted[j]),
                   float(nodded[j]), float(stood[j]))[2].get("yaw_c", 0.0) for j in range(K)]
        info["held_steady_yaw_deg"] = [round(float(min(yc)), 1), round(float(max(yc)), 1)]
    if thigh and not left_hangs[0]:
        info["hand_on_thigh"] = {"down_m": [round(float(min(x[0] for x in thigh)), 3),
                                            round(float(max(x[0] for x in thigh)), 3)],
                                 "off_m": round(max(x[1] for x in thigh), 3),
                                 "set_out_m": round(max(x[2] for x in thigh), 3)}
    for n in names:                                    # (the loop closes exactly)
        for kind in ("rot", "move"):
            ks = keys[n][kind]
            if ks:
                v = ks[0][1:]
                if kind == "rot" and np.dot(ks[-1][1:], v) < 0:
                    v = [-x for x in v]
                ks[-1] = [ks[-1][0]] + list(v)
    tracks = []
    for n in names:
        o = {kk: vv for kk, vv in proto[n].items() if kk not in ("rot", "move")} if n in proto else \
            {"part": sk.ix[n], "part_name": n}
        o["rot"] = keys[n]["rot"]
        if has_move[n]:
            o["move"] = keys[n]["move"]
        tracks.append(o)
    clip = {"long": long, "loop": True, "stride": round(k * cut.stride, 3), "weapon": fit.run.get("weapon"),
            "tracks": tracks, "events": []}
    info["ground_mm"] = set_down(fit, clip)
    before = R.ball_slide(sk, clip)
    done = None
    for tol in PLANT_TOLS:
        trial = json.loads(json.dumps(clip))
        done = R.plant(sk, trial, tol)
        if done:
            clip = trial
            break
    if not done:
        raise RuntimeError(f"{fit.name} {g['name']}: the feet could not be planted")
    if dk > 1:
        # (planted on keys twice as close: a sprinting foot is down 0.10 to 0.17 s, one or two keys at 15 a second,
        #  too few to fit its stride to; then every other key kept, 15 a second as every clip's)
        for tr in clip["tracks"]:
            for kind_ in ("rot", "move"):
                if tr.get(kind_):
                    tr[kind_] = tr[kind_][::dk]
        info["planted_on_keys_a_second"] = g["rate"] * dk
        info["ground_mm"] = round(info["ground_mm"] + set_down(fit, clip), 1)     # (on the keys kept)
    if g["kind"] == "walk":
        info["boots_raised_mm"] = boots_up(fit, clip)
        info["boots_lowered_mm"], info["swing_raised_mm"] = boots_settle(fit, clip)
        info["boots_held_mm"] = boots_still(fit, clip)
        if carry == "limp" and not left_hangs[0] and not legs_only:
            # (the legs were bent again at their keys: the hand laid on the thigh where the thigh is now)
            info["hand_relaid_into_cm"] = relay_hand(fit, clip, thigh_pole)
    after = R.ball_slide(sk, clip)
    if not legs_only:
        clip["events"], cs = footstep_events(fit, clip, loop=True,
                                             fallback={s_: [round((x - phase) % long, 3) for x in ft[s_]["on"]]
                                                       for s_ in "lr"})
        info["contacts_s"] = {s_: [[round(a_, 3), round(b_, 3)] for a_, b_ in v] for s_, v in cs.items()}
    info["plant"] = {"tol_cm": round(done["tol"] * 100, 1), "body_slid_cm": done["slide_cm"],
                     "stride_scaled": done["stride"][0], "stride": done["stride"][1],
                     "slide_before": round(before["slide"] / before["speed"], 4),
                     "slide_after": round(after["slide"] / after["speed"], 4)}
    info["left_foot"] = round((lf - phase / long) % 1.0, 3)
    if phase:
        info["phase_s"] = round(phase, 4)
    log(f"   {g['name']:11s} {carry:5s} {long:.4f} s, {K // dk + 1} keys, stride {clip['stride']} m (scaled {done['stride'][0]}): "
        f"{clip['stride'] / long:.2f} m/s; planted within {done['tol'] * 100:.1f} cm, the body slid {done['slide_cm']} cm at "
        f"most, a planted ball {before['slide'] / before['speed']:.4f} -> {after['slide'] / after['speed']:.4f} per m/s"
        + (f"; rifle {out * 100:.0f} cm out from the shoulder" if out is not None else "")
        + (f", free arm opened {info['free_arm_opened_deg']} deg, elbow bent {info['free_arm_elbow_bent_deg']} deg more,"
           f" forearm out {info['free_arm_forearm_out_deg']} deg, swing calmed {info['free_arm_swing_calmed']}"
           f" ({info['free_arm_clear_cm']} cm clear)" if "free_arm_opened_deg" in info else "")
        + (f", hand on the thigh {info['hand_on_thigh']}" if "hand_on_thigh" in info else "")
        + (f"; arms lifted {info['arms_lifted_deg']} deg" if "arms_lifted_deg" in info else ""))
    return clip, info


KEY_RATE_TOL = 0.4           # (keys a second: a loop is as long as takes a whole number of evenly spaced keys within
#                              this of its rate)
PHASES = 12                  # (a dense gait's loop begun at this many points between two keys, the best kept)


def keyable(cut_long, quick, rate):
    """`quick` changed as little as may be so that the loop's length takes a whole number of keys at `rate` a second,
    evenly spaced, to within KEY_RATE_TOL (a short loop can fall between: 0.576 s takes 8 keys, 13.9 a second, or 9,
    15.6 a second)."""
    long = round(cut_long / quick, 4)
    K = max(2, int(round(long * rate)))
    if abs(K / long - rate) <= KEY_RATE_TOL:
        return quick
    best = None
    for K_ in (K - 1, K, K + 1):
        for r_ in (rate - KEY_RATE_TOL + 0.02, rate + KEY_RATE_TOL - 0.02):
            L_ = K_ / r_
            if best is None or abs(L_ - long) < abs(best - long):
                best = L_
    return round(cut_long / best, 4)


def leg_pops(clip, most=30.0):
    """The legs' pops between keys: a bone's turn from one key to the next over `most` degrees and over three times
    its turn over either neighbouring step (a loop's steps wrap): [(bone, degrees), ...]."""
    out = []
    for tr in clip["tracks"]:
        if tr["part_name"] not in LEGS or not tr.get("rot"):
            continue
        Q = np.array(tr["rot"], float)[:, 1:]
        ang = np.degrees(2 * np.arccos(np.abs((Q[1:] * Q[:-1]).sum(1)).clip(0, 1)))
        for i in range(len(ang)):
            nb = max(ang[(i - 1) % len(ang)], ang[(i + 1) % len(ang)])
            if ang[i] > most and ang[i] > 3 * nb:
                out.append((tr["part_name"], round(float(ang[i]), 1)))
    return out


def boots_lowest(fit, clip, per_s=120, each=False):
    """The lowest point of either boot's bottom outline over the loop (boot_points: the near mesh's own skin); with
    `each`, {side: its boot's lowest}."""
    L = float(clip["long"])
    n = int(round(L * per_s))
    low = {"l": 9.0, "r": 9.0}
    for j in range(n):
        G, X = fit.sk.fk(*R.clip_pose(clip, L * j / n))
        for s_ in "lr":
            low[s_] = min(low[s_], float(boot_points(fit, G, X, s_)[:, 2].min()))
    return low if each else min(low.values())


def key_phase(fit, g, quick):
    """Where in the cut a dense gait's loop begins (key_gait's phase): of PHASES starts between two keys, the one whose
    kept keys (every dk-th of those it was planted on) cut the boots least through the ground between keys (the
    lowest point of their bottom outline under 10 mm) and pop least (leg_pops: a knee or a foot that does its whole
    turn between two keys and hardly any either side; each such turn counts half its degrees over 20). A sprinting
    foot is down 0.10 to 0.17 s, two or three keys at 15 a second: a key near the middle of its stance keeps the
    knee's give there, where keys either side of it would cut through the ground; a knee that snaps through 60
    degrees in a fifteenth of a second is better split across two keys. Returns (phase s, cost, lowest m, pops)."""
    best = None
    step = 1.0 / (g["rate"] * PHASES)
    for i in range(PHASES):
        ph = i * step
        c, _ = key_gait(fit, g, log=lambda *_: None, quick=quick, legs_only=True, phase=ph)
        lows = boots_lowest(fit, c, each=True)
        low = min(lows.values())
        pp = leg_pops(c)
        # (and a boot that never comes down to the ground between its kept keys: a short stance the keys straddle)
        cost = max(0.0, -0.010 - low) * 1000 + sum(0.5 * (a_ - 20.0) for _, a_ in pp) \
            + sum(max(0.0, v - 0.012) * 2000 for v in lows.values())
        if best is None or cost < best[1] - 1e-6:
            best = (ph, cost, low, pp)
    return best


def soles_of(fit, clip, per_s=60, loop=True):
    """Each foot's sole over the clip (soldier.mo_events' sole, run_styles.sole_z), sampled per_s a second."""
    sk = fit.sk
    long = float(clip["long"])
    n = int(round(long * per_s))
    tt = [long * j / n for j in range(n if loop else n + 1)]
    poses = [sk.fk(*R.clip_pose(clip, t)) for t in tt]
    return {s: np.array([R.sole_z(sk, G, X, s) for G, X in poses]) for s in "lr"}, tt


def set_down(fit, clip, loop=True):
    """The clip set on his own ground as his run stands on it (the soles' tenth percentile, as run_styles does), then,
    if a sole would go more than 8 mm under the ground, lifted to that. The pelvis and what rides the root together."""
    sk = fit.sk
    z, _ = soles_of(fit, clip, loop=loop)
    dz = fit.ground - 0.5 * (np.percentile(z["l"], 10) + np.percentile(z["r"], 10))
    low = min(z["l"].min(), z["r"].min()) + dz
    if low < -0.008:
        dz += -0.008 - low
    for tr in clip["tracks"]:
        if tr.get("move") and (tr["part_name"] == "pelvis" or sk.pname[sk.ix[tr["part_name"]]] == "root"):
            for k_ in tr["move"]:
                k_[3] = round(k_[3] + dz, 4)
    return round(1000 * float(dz), 1)


# ================================================================================================ the footfalls
EV_DOWN = 0.015              # (a boot is down when the lowest point of its bottom outline is within this of the ground,
EV_LEAST = 0.03              #  for at least this long, s, moving over the ground at under half the clip's speed)
EV_PER_S = 240               # (samples a second)


def boot_soles(fit):
    """Each boot's bottom outline: the near mesh's points carried by the foot and the ball that lie within 8 mm of the
    boot's lowest at rest, with both bones of their skin and the weights (the near mesh's own two-bone skin)."""
    if not hasattr(fit, "_boots"):
        M = fit.D["near"]
        P, a = fit.body.P, fit.body.part
        b = np.array(M.get("part2", a))
        w = np.array(M.get("w2", np.zeros(len(a))), float)
        fit._boots = {}
        for s_ in "lr":
            ids = [fit.sk.ix[n] for n in ("foot_" + s_, "ball_" + s_) if n in fit.sk.ix]
            idx = np.where(np.isin(a, ids))[0]
            z = P[idx, 2]
            idx = idx[z <= z.min() + 0.008]
            fit._boots[s_] = (P[idx], a[idx], b[idx], w[idx][:, None])
    return fit._boots


def boot_points(fit, G, X, s_):
    P, a, b, w = boot_soles(fit)[s_]
    H0 = fit.sk.H0
    A = X[a] + np.einsum("nij,nj->ni", G[a], P - H0[a])
    B = X[b] + np.einsum("nij,nj->ni", G[b], P - H0[b])
    return A * (1 - w) + B * w


def contact_starts(fit, clip, loop=True):
    """Where each boot comes down in the clip played in place at its own speed: {side: [(start s, end s), ...]}. A
    contact is a run of samples with the boot's lowest point within EV_DOWN of the ground, at least EV_LEAST long,
    while the boot moves over the ground at under half the clip's speed (a heel's tap, a toe skimming past, is not
    one)."""
    sk = fit.sk
    L = float(clip["long"])
    n = max(96, int(math.ceil(L * EV_PER_S)))
    ts = np.arange(n) * (L / n) if loop else np.linspace(0.0, L, n + 1)
    dt = L / n
    own = float(clip["stride"]) / L
    poses = [sk.fk(*R.clip_pose(clip, t)) for t in ts]
    out = {}
    for s_ in "lr":
        W = np.array([boot_points(fit, G, X, s_) for G, X in poses])
        low = W[:, :, 2].min(1)
        dW = (np.roll(W, -1, 0) - W) if loop else np.diff(W, axis=0, append=W[-1:])
        vw = np.hypot(dW[:, :, 0].mean(1) / dt + own, dW[:, :, 1].mean(1) / dt)
        down = low <= EV_DOWN
        if loop:
            spans = R._runs(down) if down.any() else []
        else:
            spans, cur = [], []
            for i, d_ in enumerate(down):
                if d_:
                    cur.append(i)
                elif cur:
                    spans.append(cur)
                    cur = []
            if cur:
                spans.append(cur)
        got = []
        for r_ in spans:
            if len(r_) * dt < EV_LEAST or float(np.median(vw[r_])) > 0.5 * own:
                continue
            got.append((float(ts[r_[0]]), float(ts[r_[-1]])))
        out[s_] = sorted(got)
    return out


def event_offset(fit):
    """His run's footstep events against its own contact starts (contact_starts): the mean delay, s."""
    if not hasattr(fit, "_ev_off"):
        run = fit.run
        L = float(run["long"])
        cs = contact_starts(fit, run, loop=True)
        d = []
        for e in run.get("events", []):
            if e.get("fx") != "footstep" or not cs.get(str(e.get("bone"))[-1]):
                continue
            starts = [a for a, b in cs[str(e["bone"])[-1]]]
            d.append(min(((e["t"] - a + L / 2) % L - L / 2 for a in starts), key=abs))
        fit._ev_off = float(np.mean(d)) if d else 0.02
    return fit._ev_off


def footstep_events(fit, clip, loop=True, fallback=None):
    """A footstep event at every contact of either boot (contact_starts), as late after it as his run's events are
    after its own contacts (event_offset: about 0.02 s); a foot that never comes down by that measure takes its
    footfalls from `fallback` (run_styles.feet's). His run's other events (kit_rattle) as his run has them."""
    L = float(clip["long"])
    off = event_offset(fit)
    cs = contact_starts(fit, clip, loop=loop)
    evs = [dict(e) for e in fit.run.get("events", []) if e.get("fx") != "footstep"]
    for s_ in "lr":
        ts = [((a + off) % L) if loop else min(a + off, L) for a, b in cs[s_]]
        if not ts and fallback:
            ts = list(fallback.get(s_, []))
        for t in ts:
            evs.append({"t": round(float(t), 3), "fx": "footstep", "socket": None, "cue": "footstep", "bone": "foot_" + s_})
    evs.sort(key=lambda e: e["t"])
    return evs, cs


# ================================================================================================ the stumble
def onsets_once(z, tol=0.012, up=0.035):
    """Footfalls on a clip played once: the samples where a sole comes down (within tol of its lowest) after it was
    `up` clear."""
    base = z.min()
    on, out = bool(z[0] < base + tol), []
    for i in range(1, len(z)):
        if not on and z[i] < base + tol:
            on = True
            out.append(i)
        elif on and z[i] > base + up:
            on = False
    return out


def plant_once(sk, clip, tol, speed):
    """plant() for a clip played once at a given ground speed (the stumble, at his run's): over each run of keys where
    a ball is within `tol` of its lowest, the body slides so that the ball goes back at exactly `speed` and keeps its
    line; between steps the slide eases; the first and the last key do not move (they are his run's)."""
    long = float(clip["long"])
    trk = {tr["part_name"]: tr for tr in clip["tracks"]}
    ts = np.array([k_[0] for k_ in trk["pelvis"]["move"]])
    K = len(ts)
    low = R.balls(sk, clip, np.linspace(0.0, long, 121))[:, :, 2].min(1)
    B = R.balls(sk, clip, ts)
    E = np.zeros((K, 2))
    cnt = np.zeros(K)
    steps = 0
    for f in range(2):
        down = B[f, :, 2] < low[f] + tol
        j = 0
        while j < K:
            if not down[j]:
                j += 1
                continue
            e = j
            while e + 1 < K and down[e + 1]:
                e += 1
            s = list(range(j, e + 1))
            j = e + 1
            if len(s) < 2:
                continue
            w = B[f, s, 0] + speed * ts[s]
            # (a step that holds an end key is held where that key has it: the ends are his run's)
            ref_x = w[0] if s[0] == 0 else (w[-1] if s[-1] == K - 1 else w.mean())
            ref_y = B[f, s[0], 1] if s[0] == 0 else (B[f, s[-1], 1] if s[-1] == K - 1 else B[f, s, 1].mean())
            E[s, 0] += -(w - ref_x)
            E[s, 1] += -(B[f, s, 1] - ref_y)
            cnt[s] += 1
            steps += 1
    held = cnt > 0
    E[held] /= cnt[held][:, None]
    held[0] = held[-1] = True
    E[0] = E[-1] = 0.0
    S = E.copy()
    free = np.where(~held)[0]
    if len(free):
        D2 = np.zeros((K - 2, K))
        for j in range(1, K - 1):
            D2[j - 1, j - 1], D2[j - 1, j], D2[j - 1, j + 1] = 1.0, -2.0, 1.0
        for c in range(2):
            S[free, c] = np.linalg.lstsq(D2[:, free], -D2 @ E[:, c], rcond=None)[0]
    for n, tr in trk.items():
        if tr.get("move") and (n == "pelvis" or sk.pname[sk.ix[n]] == "root"):
            for j, k_ in enumerate(tr["move"]):
                k_[1] = round(k_[1] + float(S[j, 0]), 4)
                k_[2] = round(k_[2] + float(S[j, 1]), 4)
    return {"tol": tol, "slide_cm": round(100 * float(np.abs(S).max()), 1), "steps": steps}


def slide_once(sk, clip, speed, n=150):
    """ball_slide for a clip played once at ground speed `speed`."""
    long = float(clip["long"])
    ts = np.linspace(0.0, long, n + 1)
    B = R.balls(sk, clip, ts)
    B[:, :, 0] += speed * ts
    dt = long / n
    s = a = 0.0
    c = 0
    for f in range(2):
        lo = B[f, :, 2].min() + 0.02
        for i in range(1, n + 1):
            if B[f, i - 1, 2] < lo and B[f, i, 2] < lo:
                d = B[f, i] - B[f, i - 1]
                s += math.hypot(d[0], d[1]) / dt
                a += abs(d[0]) / dt
                c += 1
    return {"slide": s / max(c, 1), "ahead": a / max(c, 1), "speed": speed}


def swing_to(a, b):
    """The least turn taking direction a onto direction b."""
    a, b = unit(a), unit(b)
    ax = np.cross(a, b)
    sn = float(np.linalg.norm(ax))
    if sn < 1e-9:
        return np.eye(3)
    return r_axis(ax, math.degrees(math.atan2(sn, float(a @ b))))


def leg_ik(fit, q, mv, s_, T):
    """The leg `s_` bent by two-bone IK so its ankle reaches T: the knee kept in the plane it bends in now (hip, knee,
    ankle), the thigh and the calf each swung the least way, the foot keeping its world turn."""
    sk = fit.sk
    th, ca, ft = (sk.ix[n + s_] for n in ("thigh_", "calf_", "foot_"))
    G, X = sk.fk(q, mv)
    hip, knee, ank = X[th], X[ca], X[ft]
    L1 = float(np.linalg.norm(sk.H0[ca] - sk.H0[th]))
    L2 = float(np.linalg.norm(sk.H0[ft] - sk.H0[ca]))
    d = np.asarray(T, float) - hip
    dist = min(max(float(np.linalg.norm(d)), abs(L1 - L2) + 1e-3), (L1 + L2) * 0.999)
    dh = unit(d)
    pole = knee - hip
    pole = pole - dh * (pole @ dh)
    ph = unit(pole)
    ca_ = (L1 * L1 + dist * dist - L2 * L2) / (2 * L1 * dist)
    knee_n = hip + L1 * (dh * ca_ + ph * math.sqrt(max(0.0, 1 - ca_ * ca_)))
    ank_n = hip + dh * dist
    St = swing_to(knee - hip, knee_n - hip)
    Gt = St @ G[th]
    Sc = swing_to(St @ (ank - knee), ank_n - knee_n)
    Gc = Sc @ St @ G[ca]
    q["thigh_" + s_] = G[sk.parent[th]].T @ Gt
    q["calf_" + s_] = Gt.T @ Gc
    q["foot_" + s_] = Gc.T @ G[ft]
    return float(np.linalg.norm(ank_n - np.asarray(T, float)))


def relay_hand(fit, clip, poles, s="l"):
    """The hand on the hurt thigh laid again at each key of a clip whose legs were moved after it was laid (a walk's
    boots set on the ground by leg IK turn the thigh under the hand): hand_on_thigh on the keyed pose, the elbow's
    side as chosen for that key. The loop's last key is its first. Returns how far the arm then lies into his body at
    worst, cm (free_arm_clear's measure)."""
    trk = {tr["part_name"]: tr for tr in clip["tracks"]}
    ts = [k_[0] for k_ in trk["pelvis"]["rot"]]
    K = len(ts) - 1
    names = [n + s for n in ("upperarm_", "lowerarm_", "hand_", "fingers_", "thumb_") if n + s in trk]
    worst = 9.0
    for j in range(K):
        q, mv = R.clip_pose(clip, ts[j])
        q, mv = dict(q), dict(mv)
        res = hand_on_thigh(fit, q, mv, s, THIGH_DOWN, pole=poles.get(j, poles.get("all")))
        worst = min(worst, res[4])
        for n in names:
            for jj in ((j, K) if j == 0 else (j,)):
                k_ = trk[n]["rot"][jj]
                v4 = R.m2q(q[n])
                if np.dot(v4, k_[1:]) < 0:
                    v4 = -v4
                trk[n]["rot"][jj] = [k_[0]] + [round(float(x), 5) for x in v4]
    return round(100 * worst, 1)


def set_caught_down(fit, clip, cut, log=print, target=0.004, most=0.06):
    """A planted boot of the stumble set down on the ground (the caught toe, which on some men's legs the plant left
    floating 2 to 3.6 cm over it): at each key but the two at each end (his run's), where a boot stands still on the
    ground (going back at under a quarter of the run's speed between the keys either side) with its lowest point more
    than 6 mm up, the leg is bent by IK until that point is `target` over the ground (`most` at most). Returns what
    was lowered, {key time: (side, mm)}."""
    sk = fit.sk
    trk = {tr["part_name"]: tr for tr in clip["tracks"]}
    ts = [k_[0] for k_ in trk["pelvis"]["rot"]]
    K = len(ts) - 1
    own = float(clip["stride"]) / float(clip["long"])
    poses = [sk.fk(*R.clip_pose(clip, t)) for t in ts]
    out = {}
    for s_ in "lr":
        Wb = [boot_points(fit, G, X, s_) for G, X in poses]
        for j in range(2, K - 1):
            v = (Wb[j + 1].mean(0) - Wb[j - 1].mean(0)) / (ts[j + 1] - ts[j - 1])
            if math.hypot(v[0] + own, v[1]) > 0.25 * own:
                continue
            low = float(Wb[j][:, 2].min())
            if low <= target + 0.002:
                continue
            q, mv = R.clip_pose(clip, ts[j])
            G, X = sk.fk(q, mv)
            leg_ik(fit, q, mv, s_, X[sk.ix["foot_" + s_]] - np.array((0.0, 0.0, min(low - target, most))))
            for n in ("thigh_", "calf_", "foot_"):
                k_ = trk[n + s_]["rot"][j]
                v4 = R.m2q(q[n + s_])
                if np.dot(v4, k_[1:]) < 0:
                    v4 = -v4
                trk[n + s_]["rot"][j] = [k_[0]] + [round(float(x), 5) for x in v4]
            out[round(ts[j], 4)] = (s_, round(1000 * min(low - target, most), 1))
    if out:
        log(f"   run_stumble: a planted boot set down on the ground at {out} (s: side, mm)")
    return out


def boots_up(fit, clip, under=-0.002, target=0.0, most=0.02):
    """A walk's boots kept out of the ground at its keys: where the lowest point of a boot's bottom outline
    (boot_points) is more than `under` below the ground at a key, the leg is bent by IK until that point is at
    `target` (`most` at most), the foot keeping its turn and its place over the ground (so the plant stands). The
    long, bent-kneed stance of a limp's good leg went 6 to 13 mm into the ground on some men's legs, its sole rolling
    through it: what was under the ground slid with it. The loop's last key is its first. Returns the most raised,
    mm, per side."""
    sk = fit.sk
    trk = {tr["part_name"]: tr for tr in clip["tracks"]}
    ts = [k_[0] for k_ in trk["pelvis"]["rot"]]
    K = len(ts) - 1
    out = {"l": 0.0, "r": 0.0}
    for j in range(K):
        q, mv = R.clip_pose(clip, ts[j])
        G, X = sk.fk(q, mv)
        changed = False
        for s_ in "lr":
            low = float(boot_points(fit, G, X, s_)[:, 2].min())
            if low >= under:
                continue
            dz = min(target - low, most)
            leg_ik(fit, q, mv, s_, X[sk.ix["foot_" + s_]] + np.array((0.0, 0.0, dz)))
            out[s_] = max(out[s_], round(1000 * dz, 1))
            changed = True
        if not changed:
            continue
        for s_ in "lr":
            for n in ("thigh_", "calf_", "foot_"):
                for jj in ((j, K) if j == 0 else (j,)):
                    k_ = trk[n + s_]["rot"][jj]
                    v4 = R.m2q(q[n + s_])
                    if np.dot(v4, k_[1:]) < 0:
                        v4 = -v4
                    trk[n + s_]["rot"][jj] = [k_[0]] + [round(float(x), 5) for x in v4]
    return out


def boots_settle(fit, clip, over=0.003, slow=0.25, swing=0.03, fast=0.5, most=0.03):
    """A walk's boots on the ground where they stand and off it where they swing, at its keys, by leg IK (the foot
    keeping its turn): a boot that stands (its outline going over the ground at under `slow` of the clip's speed at
    this key) with its lowest point more than `over` up is let down onto the ground; a boot that swings (faster than
    `fast` of it) with its lowest point under `swing` is lifted to `swing` (`most` either way). The limp's good foot
    came to rest 1 to 3 cm up after a heel-first landing and settled through half its stance; its toe, rolled off the
    ground, swung back through it at 1.3 m/s (a scuff). The loop's last key is its first. Returns the most let down
    and the most lifted, mm, per side."""
    sk = fit.sk
    trk = {tr["part_name"]: tr for tr in clip["tracks"]}
    ts = [k_[0] for k_ in trk["pelvis"]["rot"]]
    K = len(ts) - 1
    L = float(clip["long"])
    own = float(clip["stride"]) / L
    dt = L / K
    poses = [R.clip_pose(clip, ts[j]) for j in range(K)]
    W = {s_: [] for s_ in "lr"}
    for j in range(K):
        G, X = sk.fk(*poses[j])
        for s_ in "lr":
            W[s_].append(boot_points(fit, G, X, s_))
    down = {"l": 0.0, "r": 0.0}
    up = {"l": 0.0, "r": 0.0}
    for j in range(K):
        q, mv = dict(poses[j][0]), dict(poses[j][1])
        changed = False
        for s_ in "lr":
            a_, b_ = W[s_][(j - 1) % K], W[s_][(j + 1) % K]
            d = (b_[:, :2] - a_[:, :2]).mean(0)
            d[0] += own * 2 * dt
            speed = float(np.hypot(d[0], d[1])) / (2 * dt)
            low = float(W[s_][j][:, 2].min())
            dz = 0.0
            if speed < slow * own and low > over:
                # (not so far that the sole, as check_clip measures it, goes more than 6 mm under: on some boots
                #  it lies lower than the outline's lowest point)
                Gp, Xp = sk.fk(*poses[j])
                room = max(0.0, float(R.sole_z(sk, Gp, Xp, s_)) + 0.006)
                dz = -min(low, most, room)
            elif speed > fast * own and low < swing:
                dz = min(swing - low, most)
            if abs(dz) < 0.001:
                continue
            G, X = sk.fk(q, mv)
            leg_ik(fit, q, mv, s_, X[sk.ix["foot_" + s_]] + np.array((0.0, 0.0, dz)))
            if dz < 0:
                down[s_] = max(down[s_], round(-1000 * dz, 1))
            else:
                up[s_] = max(up[s_], round(1000 * dz, 1))
            changed = True
        if not changed:
            continue
        for s_ in "lr":
            for n in ("thigh_", "calf_", "foot_"):
                for jj in ((j, K) if j == 0 else (j,)):
                    k_ = trk[n + s_]["rot"][jj]
                    v4 = R.m2q(q[n + s_])
                    if np.dot(v4, k_[1:]) < 0:
                        v4 = -v4
                    trk[n + s_]["rot"][jj] = [k_[0]] + [round(float(x), 5) for x in v4]
    return down, up


def boots_still(fit, clip, down=0.008, most=0.05):
    """A walk's standing boots held still on the ground at its keys: through each stance (keys where a boot has
    points within `down` of the ground at this key and the next), the points that are down at both keys should not
    move over the ground (the clip in place, the ground going back at its speed); what they move, on the mean, is
    added up back from the stance's end (where the ball is down and the plant has it going back at the ground's
    speed) and each key's ankle set by leg IK where its points will be at the next (`most` at most; the foot keeps
    its turn, so it still rolls heel to toe), eased in over the key before the stance. The limp's good leg, a long
    bent-kneed stance, came down heel first with its heel going back 6 to 7 mm a sixtieth of a second, 4 to 7 cm a
    step on some men's legs: the plant (the body slid so the balls keep the ground's speed) holds the ball, which is
    still 5 to 15 cm up then. Returns the most moved, mm, per side."""
    sk = fit.sk
    trk = {tr["part_name"]: tr for tr in clip["tracks"]}
    ts = [k_[0] for k_ in trk["pelvis"]["rot"]]
    K = len(ts) - 1
    L = float(clip["long"])
    own = float(clip["stride"]) / L
    poses = [R.clip_pose(clip, ts[j]) for j in range(K)]
    out = {}
    corr = {s_: np.zeros((K, 2)) for s_ in "lr"}
    for s_ in "lr":
        W = [boot_points(fit, *sk.fk(*poses[j]), s_) for j in range(K)]
        on = [w[:, 2] <= down for w in W]
        # (the slide between each key and the next of the points down at both)
        step = np.zeros((K, 2))
        held = np.zeros(K, bool)
        for j in range(K):
            j2 = (j + 1) % K
            m = on[j] & on[j2]
            if m.sum() < 3:
                continue
            d = W[j2][m, :2] - W[j][m, :2]
            d[:, 0] += own * (L / K)
            step[j] = d.mean(0)
            held[j] = True
        if held.all() or not held.any():
            out[s_] = 0.0
            continue
        # (each stance held where it ends, on the ball, as the plant left it: from its last held step back to its
        #  first, each key set where the points will be at the next; eased in over the key before the stance)
        ends = [j for j in range(K) if held[j] and not held[(j + 1) % K]]
        for j1 in ends:
            cum = np.zeros(2)
            j = j1
            while held[j]:
                cum = cum + step[j]
                c_ = cum.copy()
                n_ = float(np.linalg.norm(c_))
                if n_ > most:
                    c_ = c_ * (most / n_)
                corr[s_][j] = c_
                j = (j - 1) % K
            corr[s_][j] = 0.5 * corr[s_][(j + 1) % K]
        out[s_] = round(1000 * float(np.linalg.norm(corr[s_], axis=1).max()), 1)
    for j in range(K):
        if not (np.any(corr["l"][j]) or np.any(corr["r"][j])):
            continue
        q, mv = poses[j]
        q, mv = dict(q), dict(mv)
        for s_ in "lr":
            c_ = corr[s_][j]
            if not np.any(c_):
                continue
            G, X = sk.fk(q, mv)
            leg_ik(fit, q, mv, s_, X[sk.ix["foot_" + s_]] + np.array((c_[0], c_[1], 0.0)))
        for s_ in "lr":
            for n in ("thigh_", "calf_", "foot_"):
                for jj in ((j, K) if j == 0 else (j,)):
                    k_ = trk[n + s_]["rot"][jj]
                    v4 = R.m2q(q[n + s_])
                    if np.dot(v4, k_[1:]) < 0:
                        v4 = -v4
                    trk[n + s_]["rot"][jj] = [k_[0]] + [round(float(x), 5) for x in v4]
    return out


def key_stumble(fit, g, log=print):
    """The stumble, played once: the cut's body on his skeleton, his run's own arms, kit and hold at the time of his
    run that the cut has bent (build.run_time); the first key his run's at 0.0 and the last his run's at `rejoin`,
    exactly (the keys copied; the few keys beside them eased onto them)."""
    sk = fit.sk
    cut = fit.cut(g)
    c = cut.c
    P = fit.plans["run"]
    C = P["C"]
    k = fit.k
    long = cut.long
    K = max(2, int(round(long * g["rate"])))
    ts = [long * j / K for j in range(K + 1)]
    t_r = float(c["rejoin"][1])
    run_time = np.array(c["build"]["run_time"], float)
    free = P["free_arms"]
    delta = {} if free else P["delta"]
    arm_tracks = [n for n in ARMS if n in sk.ix]
    names, proto = track_names(fit, C, TRUNK + LEGS + (() if free else ("clavicle_l", "clavicle_r")))
    body = [n for n in TRUNK + LEGS if n in sk.ix] + ([n for n in arm_tracks if n in names] if free else [])
    has_move = {n: bool(proto[n].get("move")) if n in proto else False for n in names}

    def run_t(t):
        f = min(max(t * FPS, 0.0), len(run_time) - 1.0)
        i = min(int(f), len(run_time) - 2)
        return (run_time[i] + (run_time[i + 1] - run_time[i]) * (f - i)) % P["long"]

    ease = max(2, int(round(0.1 * g["rate"])))                  # (keys at each end eased onto his run's: 0.1 s)

    def pose(j, lift=0.0, nod=0.0, up=0.0):
        t = ts[j]
        f = t * FPS
        q, mv = {}, {}
        for n in body:
            L = cut.local(n, sk.pname[sk.ix[n]], f)
            if n in delta:
                L = L @ delta[n]
            q[n] = L
        mv["pelvis"] = k * cut.pelvis_at(f) + P["pelvis_c"]
        tr_t = 0.0 if j == 0 else (t_r if j == K else run_t(t))
        tq, tm = R.clip_pose(C, tr_t)
        # (at the ends his run's own keys; beside them the cut's body eased onto his run's)
        w = min(sm(j / ease), sm((K - j) / ease))
        if w < 1.0:
            for n in body:
                own = tq.get(n, np.eye(3))
                q[n] = R.q2m(R.nlerp(R.m2q(own), R.m2q(q[n]), w))
            mv["pelvis"] = tm["pelvis"] * (1 - w) + mv["pelvis"] * w
        for n in names:
            if n not in q and n in tq and sk.pname[sk.ix[n]] != "root":
                q[n] = tq[n]
            if n != "pelvis" and n in tm and sk.pname[sk.ix[n]] != "root":
                mv[n] = tm[n]
        if not free:
            upright(q, up)
            lift_arms(q, lift)
            nod_head(q, nod)
        Gn, Xn = sk.fk(q, mv)
        Go, Xo = sk.fk(tq, tm)
        for n, a in P["anchor"].items():
            i, jx = sk.ix[n], sk.ix[a]
            Gw = Gn[jx] @ Go[jx].T @ Go[i]
            Xw = Xn[jx] + Gn[jx] @ Go[jx].T @ (Xo[i] - Xo[jx])
            r = sk.ix["root"]
            q[n] = Gw
            mv[n] = Xw - sk.H0[r] - (sk.H0[i] - sk.H0[r])
        keep_riders(fit, q, mv, tq, tm)
        return q, mv, (Go, Xo)

    # (his arms lifted where the pitch forward would put his weapon into him; none at the ends, his run's keys)
    lift = np.zeros(K + 1)
    nodded = np.zeros(K + 1)
    stood = np.zeros(K + 1)
    lift_info = {}
    if not free and fit.w_pts is not None and fit.run.get("weapon") not in (None, "none"):
        lift = lift_needed(fit, pose, K + 1, "run", loop=False)
        nodded = lift_needed.nod.copy()
        stood = lift_needed.up.copy()
        lift_info = {"arms_lifted_past_his_line_deg": lift_needed.info["extra_deg"],
                     "arms_let_down_from_his_line_deg": lift_needed.info["let_down_deg"],
                     "hands_head_at_keys_cm": lift_needed.info["head_gap_cm"]}
        if lift_needed.info["pitch_off_deg"]:
            lift_info["weapon_up_off_his_legs_deg"] = lift_needed.info["pitch_off_deg"]
        for v_ in (lift, nodded, stood):     # (the end keys his run's own; the keys beside them, eased onto his
            v_[0] = v_[K] = 0.0              #  run's body by pose, were measured as they are)
        if np.any(nodded):
            lift_info["head_turned_deg"] = [round(float(nodded.min()), 1), round(float(nodded.max()), 1)]
        if np.any(stood):
            lift_info["trunk_stood_up_deg"] = [round(float(stood.min()), 1), round(float(stood.max()), 1)]
    keys = {n: {"rot": [], "move": []} for n in names}
    for j, t in enumerate(ts):
        q, mv, _ = pose(j, float(lift[j]), float(nodded[j]), float(stood[j]))
        for n in names:
            v = R.m2q(q.get(n, np.eye(3)))
            prev = keys[n]["rot"][-1][1:] if keys[n]["rot"] else None
            if prev is not None and np.dot(prev, v) < 0:
                v = -v
            keys[n]["rot"].append([round(t, 4)] + [round(float(x), 5) for x in v])
            if has_move[n]:
                keys[n]["move"].append([round(t, 4)] + [round(float(x), 4) for x in mv.get(n, np.zeros(3))])
    # the ends: his run's own keys, copied
    for n in names:
        tr = proto.get(n)
        for kind, rot in (("rot", True), ("move", False)):
            ks = keys[n][kind]
            if not ks:
                continue
            for jj, tt in ((0, 0.0), (K, t_r)):
                if tr is not None and tr.get(kind):
                    v = [float(x) for x in R.key_at(tr[kind], tt, rot)]
                    hit = [k_ for k_ in tr[kind] if abs(k_[0] - tt) < 1e-6]
                    if hit:
                        v = [float(x) for x in hit[0][1:]]
                else:
                    v = [1.0, 0.0, 0.0, 0.0] if rot else [0.0, 0.0, 0.0]
                if rot:
                    near = ks[1][1:] if jj == 0 else ks[-2][1:]
                    if np.dot(near, v) < 0 and jj == K:
                        v = [-x for x in v]
                ks[jj] = [ks[jj][0]] + v
            if rot:                                           # (the signs follow on from the first key)
                for jj in range(1, len(ks)):
                    if np.dot(ks[jj - 1][1:], ks[jj][1:]) < 0:
                        ks[jj] = [ks[jj][0]] + [-x for x in ks[jj][1:]]
    tracks = []
    for n in names:
        o = {kk: vv for kk, vv in proto[n].items() if kk not in ("rot", "move")} if n in proto else \
            {"part": sk.ix[n], "part_name": n}
        o["rot"] = keys[n]["rot"]
        if has_move[n]:
            o["move"] = keys[n]["move"]
        tracks.append(o)
    clip = {"long": long, "loop": False, "stride": round(fit.run_speed * long, 3), "weapon": fit.run.get("weapon"),
            "tracks": tracks, "events": [], "rejoin": ["run", round(t_r, 4)]}
    before = slide_once(sk, clip, fit.run_speed)
    done = None
    for tol in PLANT_TOLS[:2]:
        trial = json.loads(json.dumps(clip))
        d = plant_once(sk, trial, tol, fit.run_speed)
        if d["slide_cm"] <= 6.0:
            clip, done = trial, d
            break
    after = slide_once(sk, clip, fit.run_speed)
    caught = set_caught_down(fit, clip, cut, log=log)
    z, tt = soles_of(fit, clip, loop=False)
    clip["events"], cs = footstep_events(fit, clip, loop=False,
                                         fallback={s_: [tt[i] for i in onsets_once(z[s_])] for s_ in "lr"})
    info = {"carry": "own", "rate": g["rate"], "keys": K + 1,
            "arms_lifted_deg": [round(float(lift.min()), 1), round(float(lift.max()), 1)], **lift_info,
            "boot_set_down_mm": caught, "contacts_s": {s_: [[round(a_, 3), round(b_, 3)] for a_, b_ in v]
                                                       for s_, v in cs.items()},
            "plant": {"tol_cm": round(done["tol"] * 100, 1) if done else None,
                      "body_slid_cm": done["slide_cm"] if done else 0.0,
                      "slide_before": round(before["slide"] / before["speed"], 4),
                      "slide_after": round(after["slide"] / after["speed"], 4)}}
    log(f"   {g['name']:11s} own   {long:.4f} s played once, {K + 1} keys, out of his run at 0.0 and back into it at "
        f"{t_r:.4f} s; covers {clip['stride']} m at his run's {fit.run_speed:.2f} m/s; "
        + (f"planted within {done['tol'] * 100:.1f} cm, the body slid {done['slide_cm']} cm at most, " if done else "NOT planted, ")
        + f"a planted ball {before['slide'] / before['speed']:.4f} -> {after['slide'] / after['speed']:.4f} per m/s")
    return clip, info


# ================================================================================================ pace
PACE_HOW = ("pace [lo, hi]: the ground speeds in m/s this clip can be played at. The game plays a gait at his speed "
            "over the clip's own (stride / long), so the planted feet keep the ground's speed at any of them and what "
            "limits a clip is time: film slowed too far floats and stops reading as the gait, film hurried too far "
            "jerks. A capture may be played from 0.80 to 1.30 of its own time, a gait with both feet in the air no "
            "slower than 0.85 of the clip's time, and the steps a second must stay a man's for the gait (sprint and "
            "dash 3.0 to 4.7, jog 2.1 to 3.3, a fast run 2.1 to 4.7, walk 1.0 to 2.7; a walk, a foot always down, "
            "from 0.565 to 1.33 of its time). Where a family's clips leave the speeds the game will send it "
            "uncovered at either end, the clip that reaches furthest that way is widened toward the edge, as far as "
            "0.85 to 1.25 of its own time (a jog 0.80 to 1.30, a walk 0.565 to 1.33), past its capture's limits "
            "(widen_paces). Then tested "
            "on this man at lo and hi as the game would play it (no faster than 1.6 times): a planted foot's slide "
            "per m/s of ground must stay under 1.5 times his own run's, or the end is pulled in. What is still "
            "uncovered is listed in the checks' pace_gaps: there the game plays the nearest clip.")


def pace_rule(kind, long, steps=2):
    """The clip's own time's limits (lo, hi), as shares of its own speed, for a clip played as it was captured."""
    sps = steps / long
    if kind == "walk":
        lo, hi = 0.565, 1.33
        band = (1.0, 2.7)
    else:
        lo, hi = 0.85, 1.30
        band = {"jog": (2.1, 3.3), "sprint": (3.0, 4.7), "dash": (3.0, 4.7), "fast": (2.1, 4.7)}[kind]
    return max(lo, band[0] / sps), min(hi, band[1] / sps)


def slide_at(sk, clip, v, loop=True, n=120):
    """A planted ball's slide at ground speed v as the game plays the clip (at v over its own speed, no faster than
    1.6 times): per m/s of ground, all ways (run_styles.ball_slide's measure, at another speed)."""
    long = float(clip["long"])
    own = float(clip["stride"]) / long
    rate = min(v / own, GAME_RATE_MOST)
    ts = np.linspace(0.0, long, n + 1)
    B = R.balls(sk, clip, ts)
    B[:, :, 0] += (v / rate) * ts
    dt = long / n / rate
    s = 0.0
    c = 0
    for f in range(2):
        lo = B[f, :, 2].min() + 0.02
        for i in range(1, n + 1):
            if B[f, i - 1, 2] < lo and B[f, i, 2] < lo:
                d = B[f, i] - B[f, i - 1]
                s += math.hypot(d[0], d[1]) / dt
                c += 1
    return (s / max(c, 1)) / v


def pace_test(fit, clip, lo, hi):
    """lo and hi pulled in to where a planted foot's slide stays under SLIDE_TIMES his run's; and the slides there."""
    sk = fit.sk
    lim = SLIDE_TIMES * fit.own_per
    own = float(clip["stride"]) / float(clip["long"])
    s_lo, s_hi = slide_at(sk, clip, lo), slide_at(sk, clip, hi)
    n = 0
    while s_hi > lim and hi > own and n < 200:
        hi = round(hi - 0.01, 2)
        s_hi = slide_at(sk, clip, hi)
        n += 1
    return lo, hi, s_lo, s_hi, lim


def pace_new(fit, g, clip):
    """A new gait's pace on this man: the cut's own pace (set on the reference man by pace_rule's limits and the
    capture's time) carried to his fitted stride, then tested."""
    c = fit.cuts[g["cut"]]
    if c.get("loop", True):
        k = float(clip["stride"]) / float(c["stride"])          # (time's limits are the cut's: the speeds go with
    else:                                                       #  the stride on this man)
        k = (float(clip["stride"]) / float(clip["long"])) / float(c["speed"])
    lo, hi = math.ceil(c["pace"][0] * k * 100 - 1e-6) / 100, math.floor(c["pace"][1] * k * 100 + 1e-6) / 100
    if g["kind"] == "sprint" and fit.name in TWO_HANDS:
        lo = max(lo, SPRINT_LEAST)
    return pace_test(fit, clip, lo, hi)


WIDEN = {"walk": (0.565, 1.33), "jog": (0.80, 1.30)}   # (how far a family's outer clip may be played from its own
WIDEN_FLIGHT = (0.85, 1.25)       # time to reach the edge of the speeds the game will send that family: a walk; a jog, as
#                                   far as a capture may be played; a faster gait with both feet in the air)


def widen_paces(fit, clips, infos):
    """Where a family's paces leave the speeds the game will send it uncovered at either end, the clip that reaches
    furthest that way is widened toward the edge: as far as WIDEN (a walk) or WIDEN_FLIGHT of its own time, past the
    capture's own time limits, its planted foot's slide tested there. What cannot be reached stays a gap
    (family_gaps): the game plays the nearest clip."""
    kind = {g["name"]: g["kind"] for g in GAITS}
    for fam, names in FAMILY.items():
        lo_b, hi_b = FAMILY_BAND[fam]
        have = [n for n in names if n in clips]
        if not have:
            continue

        def v(n):
            return float(clips[n]["stride"]) / float(clips[n]["long"])

        def sh(n):
            gw = next((g_ for g_ in GAITS if g_["name"] == n), {}).get("widen")
            return tuple(gw) if gw else WIDEN.get(kind[n], WIDEN_FLIGHT)
        if max(clips[n]["pace"][1] for n in have) < hi_b - 1e-9:
            n = max(have, key=lambda n_: v(n_) * sh(n_)[1])
            hi = min(hi_b, math.floor(v(n) * sh(n)[1] * 100 + 1e-6) / 100)
            if hi > clips[n]["pace"][1]:
                was = list(clips[n]["pace"])
                lo_, hi2, _, _, _ = pace_test(fit, clips[n], clips[n]["pace"][0], hi)
                clips[n]["pace"] = [lo_, hi2]
                infos[n]["pace_widened"] = {"from": was, "to": [lo_, hi2],
                                            "own_time": [round(lo_ / v(n), 3), round(hi2 / v(n), 3)]}
        if min(clips[n]["pace"][0] for n in have) > lo_b + 1e-9:
            n = min(have, key=lambda n_: v(n_) * sh(n_)[0])
            lo = max(lo_b, math.ceil(v(n) * sh(n)[0] * 100 - 1e-6) / 100)
            if g_sprint_two(fit, kind[n]):
                lo = max(lo, SPRINT_LEAST)
            if lo < clips[n]["pace"][0] and slide_at(fit.sk, clips[n], lo) <= SLIDE_TIMES * fit.own_per:
                was = infos[n].get("pace_widened", {}).get("from", list(clips[n]["pace"]))
                clips[n]["pace"] = [lo, clips[n]["pace"][1]]
                infos[n]["pace_widened"] = {"from": was, "to": list(clips[n]["pace"]),
                                            "own_time": [round(clips[n]["pace"][0] / v(n), 3),
                                                         round(clips[n]["pace"][1] / v(n), 3)]}


def g_sprint_two(fit, kind):
    return kind == "sprint" and fit.name in TWO_HANDS


def pace_old(fit, name):
    clip = fit.D["clips"][name]
    own = float(clip["stride"]) / float(clip["long"])
    steps = max(2, sum(1 for e in clip.get("events", []) if e.get("fx") == "footstep"))
    a, b = pace_rule(OLD[name], float(clip["long"]), steps)
    lo, hi = math.ceil(a * own * 100 - 1e-6) / 100, math.floor(b * own * 100 + 1e-6) / 100
    return pace_test(fit, clip, lo, hi)


# ================================================================================================ checks
def turn_rate(clip):
    """The fastest turn of any bone between two keys, degrees a second, and the bone."""
    worst, which = 0.0, None
    for tr in clip["tracks"]:
        ks = tr.get("rot")
        if not ks:
            continue
        Q = np.array([k_[1:] for k_ in ks])
        T = np.array([k_[0] for k_ in ks])
        d = np.abs(np.sum(Q[1:] * Q[:-1], axis=1)).clip(0, 1)
        ang = np.degrees(2 * np.arccos(d)) / np.maximum(np.diff(T), 1e-6)
        if ang.max() > worst:
            worst, which = float(ang.max()), tr["part_name"]
    return round(worst, 0), which


def ends_apart(a, b):
    """Two keys of one track apart: degrees for a turn (each key made whole first: a key is written to 5 places)."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = abs(float(np.dot(a / np.linalg.norm(a), b / np.linalg.norm(b))))
    return math.degrees(2 * math.acos(min(1.0, d)))


def arm_out(fit, q, mv, s="l"):
    """How far the upper arm stands out from his side, degrees: its direction in his chest's frame, out of the
    chest's upright fore-and-aft plane."""
    sk = fit.sk
    G, X = sk.fk(q, mv)
    u = unit(G[sk.ix["chest"]].T @ (X[sk.ix["lowerarm_" + s]] - X[sk.ix["upperarm_" + s]]))
    return math.degrees(math.asin(max(-1.0, min(1.0, (-1.0 if s == "l" else 1.0) * float(u[1])))))


def hands_head(fit, clip, ts):
    """The least distance from either forearm or hand to his head (its mesh, the helmet with it) at the times ts."""
    return min(head_gap(fit, *fit.sk.fk(*R.clip_pose(clip, t))) for t in ts)


def check_clip(fit, g, clip, log=print, made=None):
    """One new clip on this file, measured: returns (measures, faults)."""
    sk = fit.sk
    name = g["name"]
    D = fit.D
    carry = carry_of(fit.name, D, g)
    loop = bool(clip.get("loop", True))
    long = float(clip["long"])
    m, bad = {"carry": carry}, []
    trk = {tr["part_name"]: tr for tr in clip["tracks"]}
    # the form
    for tr in clip["tracks"]:
        if sk.ix.get(tr["part_name"]) != tr["part"]:
            bad.append(f"{tr['part_name']}: part {tr['part']}")
        for kind in ("rot", "move"):
            ks = tr.get(kind)
            if not ks:
                continue
            tt = [k_[0] for k_ in ks]
            if tt[0] != 0.0 or abs(tt[-1] - long) > 1e-3 or any(b_ <= a_ for a_, b_ in zip(tt, tt[1:])):
                bad.append(f"{tr['part_name']} {kind}: times")
            if kind == "rot" and any(abs(np.linalg.norm(k_[1:]) - 1.0) > 2e-4 for k_ in ks):
                bad.append(f"{tr['part_name']}: a turn not whole")
    for f_ in ("long", "loop", "stride", "weapon", "tracks", "events", "from", "about", "pace"):
        if f_ not in clip:
            bad.append(f"no '{f_}'")
    if clip.get("weapon") != fit.run.get("weapon"):
        bad.append(f"weapon '{clip.get('weapon')}', his run's '{fit.run.get('weapon')}'")
    if g.get("style_of") != clip.get("style_of"):
        bad.append(f"style_of {clip.get('style_of')}")
    # the loop closes / the stumble's ends
    if loop:
        worst = 0.0
        for tr in clip["tracks"]:
            worst = max(worst, ends_apart(tr["rot"][0][1:], tr["rot"][-1][1:]))
            if tr.get("move") and np.abs(np.array(tr["move"][0][1:]) - np.array(tr["move"][-1][1:])).max() > 1e-4:
                bad.append(f"{tr['part_name']}: the loop's moves open")
        m["loop_open_deg"] = round(worst, 3)
        if worst > 0.5:
            bad.append(f"the loop is open by {worst:.2f} degrees")
    else:
        rj = clip.get("rejoin")
        if not rj or rj[0] != "run":
            bad.append("no rejoin")
        else:
            run = D["clips"]["run"]
            rt = {tr["part_name"]: tr for tr in run["tracks"]}
            worst = [0.0, 0.0]
            for n, tr in trk.items():
                for e, (kk, tt) in enumerate(((0, 0.0), (-1, float(rj[1])))):
                    own = R.key_at(rt[n]["rot"], tt, True) if n in rt and rt[n].get("rot") else np.array((1.0, 0, 0, 0))
                    worst[e] = max(worst[e], ends_apart(tr["rot"][kk][1:], own))
                    if tr.get("move") and n in rt and rt[n].get("move"):
                        dm = float(np.abs(np.array(tr["move"][kk][1:]) - R.key_at(rt[n]["move"], tt, False)).max())
                        if dm > 0.002:
                            bad.append(f"{n}: {dm * 1000:.1f} mm off his run's at {'the start' if e == 0 else 'the rejoin'}")
            for n in rt:
                if n not in trk:
                    bad.append(f"his run's track {n} is not in the stumble")
            m["first_key_off_run_deg"], m["last_key_off_run_deg"] = round(worst[0], 3), round(worst[1], 3)
            m["rejoin"] = rj
            if max(worst) > 1.0:
                bad.append(f"the ends are {worst[0]:.2f} / {worst[1]:.2f} degrees off his run's")
            want = round(fit.run_speed * long, 3)
            if abs(clip["stride"] - want) > 0.002:
                bad.append(f"stride {clip['stride']}, his run's speed over the clip {want}")
    # the feet
    own_speed = float(clip["stride"]) / long
    if loop:
        s = R.ball_slide(sk, clip)
        base_per, base_ahead = fit.own_per, fit.own_ahead
    else:
        s = slide_once(sk, clip, fit.run_speed)
        base_per, base_ahead = fit.own_per, fit.own_ahead
    per, ahead = s["slide"] / s["speed"], s["ahead"] / s["speed"]
    m["speed_mps"] = round(own_speed, 3)
    m["slide_per_mps"] = round(per, 4)
    m["slide_ahead_per_mps"] = round(ahead, 4)
    m["slide_of_his_runs"] = round(per / base_per, 2)
    if per > base_per + 1e-4:
        bad.append(f"a planted foot slides {per:.4f} per m/s, his run's {base_per:.4f}")
    z, _ = soles_of(fit, clip, loop=loop)
    lows = {s_: float(z[s_].min()) for s_ in "lr"}
    m["sole_lowest_mm"] = {s_: round(1000 * v, 1) for s_, v in lows.items()}
    # (a foot up on its toes, the dash's, has its sole's heel and ball points over the ground while the boot stands
    #  on its toe cap: floating is judged on the lower of the sole and the boot's own outline)
    boots = boots_lowest(fit, clip, each=True)
    m["boot_lowest_mm"] = {s_: round(1000 * v, 1) for s_, v in boots.items()}
    for s_, v in lows.items():
        if v < -GROUND_UNDER - 5e-4:
            bad.append(f"foot_{s_} {-1000 * v:.1f} mm under the ground")
        if min(v, boots[s_]) > GROUND_OVER + 5e-4:
            bad.append(f"foot_{s_} floats {1000 * min(v, boots[s_]):.1f} mm at its lowest")
    feet_on = [e["bone"] for e in clip["events"] if e["fx"] == "footstep"]
    m["footfalls"] = {b: [e["t"] for e in clip["events"] if e["fx"] == "footstep" and e["bone"] == b] for b in ("foot_l", "foot_r")}
    if set(feet_on) != {"foot_l", "foot_r"}:
        bad.append(f"footfalls {sorted(set(feet_on))}")
    rattle = [e for e in fit.run.get("events", []) if e.get("fx") != "footstep"]
    if [e for e in clip["events"] if e["fx"] != "footstep"] != rattle:
        bad.append("kit_rattle not as his run has it")
    # the weapon in his hands, and clear of him
    ts = [k_[0] for k_ in trk["pelvis"]["rot"]]
    has_weapon = fit.w_pts is not None and clip.get("weapon") not in (None, "none")
    if has_weapon:
        if carry == "own":
            hold = D["clips"][g["hold"]]
            ref = fit.fists_on_weapon(hold, np.linspace(0.0, hold["long"], 17)[:-1])
            now = fit.fists_on_weapon(clip, ts)
            off = {}
            for s_ in "lr":
                spread = float(np.linalg.norm(ref[s_] - ref[s_].mean(0), axis=1).max())
                if spread > 0.012:                    # (a hand that does not hold in his own clip)
                    continue
                off[s_] = float(np.linalg.norm(now[s_] - ref[s_].mean(0), axis=1).max())
            m["fist_off_mm"] = {s_: round(1000 * v, 1) for s_, v in off.items()}
            m["holds"] = "".join(sorted(off))
        else:
            w = sk.ix["weapon"]
            worst = 0.0
            pitch, lowest = [], 9.0
            for t in ts:
                G, X = sk.fk(*R.clip_pose(clip, t))
                at = X[w] + G[w] @ (fit.w_bal - sk.H0[w])
                worst = max(worst, float(np.linalg.norm(fit.hands.grip(G, X, "r", fit.w_half + 0.008) - at)))
                ax = G[w][:, 0]
                pitch.append(math.degrees(math.asin(max(-1.0, min(1.0, -ax[2])))))
                if ax[0] < 0.8:
                    bad.append(f"the muzzle is not forward at {t:.3f} s")
                    break
            off = {"r": worst}
            m["fist_off_mm"] = {"r": round(1000 * worst, 1)}
            m["holds"] = "r at the balance"
            m["muzzle_down_deg"] = [round(min(pitch), 1), round(max(pitch), 1)]
        for s_, v in off.items():
            if v > FIST_TOL:
                bad.append(f"fist_{s_} {1000 * v:.1f} mm off its place on the weapon")
        cl, which, zmin = 9.0, None, 9.0
        for t in np.linspace(0.0, long, 2 * (len(ts) - 1) + 1):
            G, X = sk.fk(*R.clip_pose(clip, t))
            c_, w_, z_ = fit.weapon_clear(G, X)
            if c_ < cl:
                cl, which = c_, w_
            zmin = min(zmin, z_)
        base = fit.base_clear(g["hold"])
        if carry == "own":
            hold = D["clips"][g["hold"]]
            hh_own = hands_head(fit, hold, np.linspace(0.0, hold["long"], 17)[:-1])
            hh = hands_head(fit, clip, np.linspace(0.0, long, 2 * (len(ts) - 1) + 1))
            m["hands_head_cm"] = [round(100 * hh, 1), round(100 * hh_own, 1)]
            if hh < min(hh_own, 0.05) - 0.005:
                bad.append(f"a hand or forearm {100 * hh:.1f} cm from his head (his own {g['hold']}: {100 * hh_own:.1f})")
            m["weapon_pitch_deg"] = weapon_pitch_range(fit, clip, ts)
            m["weapon_pitch_his_own_deg"] = weapon_pitch_range(fit, hold, np.linspace(0.0, hold["long"], 17)[:-1])
            m["weapon_heading_deg"] = weapon_heading_range(fit, clip, ts)
            m["weapon_heading_his_own_deg"] = weapon_heading_range(fit, hold, np.linspace(0.0, hold["long"], 17)[:-1])
        m["weapon_clear_cm"] = round(100 * cl, 1)
        m["weapon_nearest"] = which
        m["weapon_clear_in_his_own_cm"] = round(100 * base, 1)
        m["weapon_over_ground_cm"] = round(100 * zmin, 1)
        if cl < min(0.0, base) - 0.01:
            bad.append(f"the weapon {-100 * cl:.1f} cm into his {which} (his own {g['hold']}: {100 * base:.1f} cm clear)")
        if zmin < 0.03:
            bad.append(f"the weapon {100 * zmin:.1f} cm from the ground")
    if carry in ("trail", "limp", "hang") and hasattr(fit, "_free_pts"):
        cl, which = 9.0, None
        for t in ts:
            q, mv = R.clip_pose(clip, t)
            c_, w_ = free_arm_clear(fit, q, mv, "l")
            if c_ < cl:
                cl, which = c_, w_
        m["free_arm_clear_cm"] = round(100 * cl, 1)
        m["free_arm_nearest"] = which
        laid = carry == "limp" and not (made or {}).get("left_hangs", bool(g.get("mirror")))
        if cl < (-LAID_HAND if laid else FREE_ARM_CLEAR - 0.004):
            bad.append(f"the left arm {-100 * cl:.1f} cm into his {which}")
        # (how far the free upper arm stands out from his side, against his run's: the chest's frame)
        m["upperarm_l_out_deg"] = [round(max(arm_out(fit, *R.clip_pose(clip, t)) for t in ts), 1),
                                   round(max(arm_out(fit, *R.clip_pose(D["clips"]["run"], t))
                                             for t in np.linspace(0.0, D["clips"]["run"]["long"], 13)[:-1]), 1)]
        m["hand_l_from_mid_cm"] = [round(100 * max(hand_mid(fit, *R.clip_pose(clip, t), "l") for t in ts), 1),
                                   round(100 * max(hand_mid(fit, *R.clip_pose(D["clips"]["run"], t), "l")
                                                   for t in np.linspace(0.0, D["clips"]["run"]["long"], 13)[:-1]), 1)]
    # speed, pace, pops
    if g["band"]:
        if not (g["band"][0] <= own_speed <= g["band"][1]):
            bad.append(f"{own_speed:.2f} m/s, outside the contract's {g['band'][0]} to {g['band'][1]}")
    if g["kind"] == "sprint" and fit.name in TWO_HANDS:
        if own_speed < SPRINT_LEAST or (clip.get("pace") and clip["pace"][0] < SPRINT_LEAST - 1e-9):
            bad.append(f"his sprint may be played under {SPRINT_LEAST} m/s ({own_speed:.2f} m/s, pace {clip.get('pace')})")
    pc = clip.get("pace")
    if pc:
        lim = SLIDE_TIMES * fit.own_per
        if loop:
            s_lo, s_hi = slide_at(sk, clip, pc[0]), slide_at(sk, clip, pc[1])
        else:
            s_lo = s_hi = per
        m["pace"] = pc
        m["pace_slide_per_mps"] = [round(s_lo, 4), round(s_hi, 4)]
        m["pace_slide_limit"] = round(lim, 4)
        if not (pc[0] < pc[1]) or s_lo > lim or s_hi > lim:
            bad.append(f"pace {pc}: a planted foot slides {s_lo:.4f} / {s_hi:.4f} per m/s at its ends, the limit {lim:.4f}")
        if loop and not (pc[0] <= own_speed <= pc[1]):
            bad.append(f"pace {pc} does not hold the clip's own {own_speed:.2f} m/s")
    rate, which = turn_rate(clip)
    m["fastest_turn_deg_s"] = [rate, which]
    return m, bad


def weapon_heading_range(fit, clip, ts):
    """The weapon's heading over the ground (its line seen from above, from +x toward +y), degrees, least and most."""
    w = fit.sk.ix["weapon"]
    v = []
    for t in ts:
        a = fit.sk.fk(*R.clip_pose(clip, t))[0][w][:, 0]
        v.append(math.degrees(math.atan2(a[1], a[0])))
    return [round(float(min(v)), 1), round(float(max(v)), 1)]


def weapon_pitch_range(fit, clip, ts):
    v = [weapon_pitch(fit, fit.sk.fk(*R.clip_pose(clip, t))[0]) for t in ts]
    return [round(float(min(v)), 1), round(float(max(v)), 1)]


def base_clear(fit, hold):
    if not hasattr(fit, "_base_clear"):
        fit._base_clear = {}
    if hold not in fit._base_clear:
        C = fit.D["clips"][hold]
        cl = 9.0
        for t in np.linspace(0.0, C["long"], 25)[:-1]:
            G, X = fit.sk.fk(*R.clip_pose(C, t))
            cl = min(cl, fit.weapon_clear(G, X)[0])
        fit._base_clear[hold] = cl
    return fit._base_clear[hold]


Fit.base_clear = base_clear


# ================================================================================================ words
def from_of(fit, g, info):
    c = fit.cuts[g["cut"]]
    b = c["build"]
    src = f"CMU {b['take']} frames {b['a']}-{b['b']}"
    carry = info["carry"]
    if g["kind"] == "stumble":
        return (f"gaits.py: gaits_mocap.py's stumble, keyed by code on his run's capture ({src}; no trip inside a run "
                f"in the CMU library: {c.get('note', '').rstrip('.')}), with this file's own arms and hold from his "
                f"run{hold_changes(info)}; the first and last keys are his run's own, the feet planted at his run's "
                f"speed")
    what = {"own": f"this file's own {'free arms' if fit.plans[g['hold']]['free_arms'] else 'hold on his weapon'} from his {g['hold']}"
            + hold_changes(info),
            "trail": "the rifle keyed into his right hand at the trail, the left arm the capture's",
            "limp": "the rifle keyed into his right hand at the trail, the left " +
                    ("arm hung low" if info.get("left_hangs") else "hand laid on his left thigh"),
            "hang": "the rifle keyed into his right hand, hanging low at the trail, the left arm hanging limp"}[carry]
    quick = info.get("quick", 1.0)
    return (f"motion capture: {src} (gaits_mocap.py's {g['cut']}: {c.get('note', '').rstrip('.')})"
            + (", mirrored left for right" if g.get("mirror") else "") + f"; keyed on this file by gaits.py with "
            f"{what}, the feet planted on his legs and the stride fitted to them"
            + (f", played {abs(quick - 1) * 100:.1f} per cent {'faster' if quick > 1 else 'slower'} than the cut to "
               f"come to the speed asked on him" if abs(quick - 1) > 1e-4 else ""))


def hold_changes(info):
    """What was done to his own hold, in words."""
    out = []
    up = info.get("trunk_stood_up_deg")
    if up and up[1] >= 0.5:
        out.append(f"his trunk stood up {up[0]:.0f} to {up[1]:.0f} degrees from the capture's lean, his head kept "
                   f"looking where it looked (so the weapon's line needs less of his arms and his forearm stays off "
                   f"his face)")
    lf = info.get("arms_lifted_deg")
    if lf and max(abs(lf[0]), abs(lf[1])) >= 0.5:
        out.append(f"both arms turned {lf[0]:.0f} to {lf[1]:.0f} degrees about his shoulders (up +: his chest leans "
                   f"further than in his own clip; the weapon kept at his own clip's pitch, within 5 degrees, and clear "
                   f"of him)")
    nd = info.get("head_turned_deg")
    if nd and max(abs(nd[0]), abs(nd[1])) >= 0.5:
        out.append(f"his head turned {nd[0]:.0f} to {nd[1]:.0f} degrees (chin up +) where a forearm would come "
                   f"nearer his face than in his own clip")
    hl = info.get("held_low")
    if hl:
        out.append(f"the weapon held low: turned {hl['tilt_deg'][0]:.0f} to {hl['tilt_deg'][1]:.0f} degrees "
                   f"muzzle-down about his right fist"
                   + (f" and lowered {hl['lowered_m'][0] * 100:.0f} to {hl['lowered_m'][1] * 100:.0f} cm"
                      if hl["lowered_m"][1] else "")
                   + (f", turned across him to {-hl['heading_deg']:.0f} degrees left of ahead"
                      if hl.get("heading_deg") is not None else "") + ", both arms set to it by IK")
    return (", " + "; ".join(out)) if out else ""


def about_of(fit, g, clip, info):
    c = fit.cuts[g["cut"]]
    v = clip["stride"] / clip["long"]
    carry = info["carry"]
    said = {
        "sprint": "flat out: a long driving stride, the trunk well forward",
        "dash": "the short bound between cover: hunched low, quick short steps, the weapon low in both hands",
        "jog_loaded": "a man under a heavy load: short heavy steps, little bounce, leaning into it",
        "run_tired": "a tired runner: labouring, the head hung and rolling, the feet scuffing",
        "run_stumble": "a stumble inside his run: a caught foot, a pitch forward, two quick recovering steps",
        "limp_l": "a wounded man walking, favouring his LEFT leg: it is stiff and short on the ground, the trunk dips over it",
        "limp_r": "a wounded man walking, favouring his RIGHT leg (limp_l mirrored): it is stiff and short on the ground",
    }[g["name"] if g["name"] in ("jog_loaded", "run_tired", "run_stumble", "limp_l", "limp_r") else g["kind"]]
    if carry == "own":
        if fit.plans[g["hold"]]["free_arms"]:
            how = "carry: own (he carries no weapon: the capture's free arms)"
        else:
            why = f" ({TWO_HANDS[fit.name]} needs both hands)" if fit.name in TWO_HANDS and g["rifle"] != "own" else ""
            how = f"carry: own (his {g['hold']}'s arms and hold on his weapon, played in step{hold_changes(info)}){why}"
    elif carry == "trail":
        how = (f"carry: trail (the rifle in the right hand at its balance, muzzle forward and "
               f"{clip['_m']['muzzle'][0]:.0f} to {clip['_m']['muzzle'][1]:.0f} degrees down, held "
               f"{info['trail_out'] * 100:.0f} cm out from the shoulder's line to clear his thigh; the left arm pumps "
               f"free" + open_words(info) + ")")
    elif carry == "hang":
        how = ("carry: hang (a tired man's: the rifle hanging in the right hand at the trail, muzzle forward and "
               f"{clip['_m']['muzzle'][0]:.0f} to {clip['_m']['muzzle'][1]:.0f} degrees down, held "
               f"{info['trail_out'] * 100:.0f} cm out from the shoulder's line to clear his thigh; the left arm hanging "
               "limp and swinging low)")
    else:
        if info.get("left_hangs"):
            how = ("carry: limp (the rifle in the right hand at the trail, the left arm hanging, swinging low"
                   + (f"; the hand is not on the hurt thigh: it {info['left_hand']}" if info.get("left_hand") else "")
                   + ")")
        else:
            th = info.get("hand_on_thigh", {})
            out_ = th.get("set_out_m", 0.0)
            how = ("carry: limp (the rifle in the right hand at the trail, the left hand laid on his left thigh "
                   f"{th.get('down_m', [0, 0])[0] * 100:.0f} to {th.get('down_m', [0, 0])[1] * 100:.0f} cm below the hip, as far as the arm reaches"
                   + (f"; set out from it up to {out_ * 100:.0f} cm where his kit at the hip would take the forearm"
                      if out_ >= 0.015 else "") + ")")
    if g["kind"] == "stumble":
        return (f"{said}. NOT a loop: play it once in place of his run from the run's 0.0 s, then go on in the run from "
                f"{clip['rejoin'][1]} s (rejoin). {clip['long']} s; stride is the {clip['stride']} m his run covers in that "
                f"time ({v:.2f} m/s). {how}. Keyed by code on the run's capture, not captured. pace {clip['pace']}: the "
                f"run's speeds it can be dropped into. {info['rate']} keys a second.")
    driven = ""
    if c.get("build", {}).get("speed_as_captured") and g["kind"] in ("sprint",):
        driven = (f" The capture is a {c['build']['speed_as_captured']:.2f} m/s run driven faster by the cutter (the CMU "
                  f"library has no sprint).")
    wd = info.get("pace_widened")
    widened = (f"; widened from {wd['from']} to reach the speeds the game will send it: {wd['own_time'][0]:.2f} to "
               f"{wd['own_time'][1]:.2f} of its own time" if wd else "")
    return (f"{said}. {v:.2f} m/s on this man ({clip['stride']} m in {clip['long']:.4f} s), the stride fitted to his "
            f"planted feet. {how}. pace {clip['pace']}: the ground speeds in m/s it can be played at (the capture's time "
            f"limits carried to his stride{widened}; a planted foot's slide tested at both ends).{driven} "
            f"{info['rate']} keys a second.")


def open_words(info):
    o, f = info.get("free_arm_opened_deg", 0.0), info.get("free_arm_elbow_bent_deg", 0.0)
    t = info.get("free_arm_forearm_out_deg", 0.0)
    if not o and not f and not t and not info.get("free_arm_swing_calmed"):
        return ""
    w = []
    if f:
        w.append(f"its elbow bent up to {f:.0f} degrees more")
    if t:
        w.append(f"the forearm carried up to {t:.0f} degrees out (the upper arm turned about its length)")
    k = info.get("free_arm_swing_calmed", 0.0)
    if k:
        w.append(f"its swing drawn up to {k * 100:.0f} per cent toward its middle")
    if o:
        w.append(f"the upper arm opened out up to {o:.0f} degrees")
    return ", " + " and ".join(w) + " where the capture's arm would pass into his kit or chest"


def about_gaits(fit, clips, pace_add):
    def row(n):
        c = clips[n]
        return f"{n} {c['stride'] / c['long']:.2f} m/s (pace {c['pace'][0]} to {c['pace'][1]})"
    old = ", ".join(f"{n} {v[0]} to {v[1]}" for n, v in pace_add.items())
    carries = sorted({(n, clips[n]["_m"]["carry"]) for n in clips})
    trail = [n for n, c in carries if c == "trail"]
    limp = [n for n, c in carries if c in ("limp", "hang")]
    st = clips["run_stumble"]
    return (
        "gaits.py (25 November 2026): a ladder of gaits by speed, each another runner's motion capture (CMU, cut by "
        "gaits_mocap.py) keyed on this file's own skeleton, the feet planted on his legs and the stride fitted to "
        "them. Sprints: " + ", ".join(row(n) for n in FAMILY["sprint"]) + ". Dashes between cover (hunched low): "
        + ", ".join(row(n) for n in FAMILY["dash"]) + ". " + row("jog_loaded") + ", a man under a load. "
        + row("run_tired") + ", a man who has run long. Walking wounded: " + ", ".join(row(n) for n in FAMILY["limp"])
        + f" (the left / the right leg the hurt one). run_stumble: {st['long']} s, played once out of his run at 0.0 s and "
        f"back into it at {st['rejoin'][1]} s (rejoin), for runs of {st['pace'][0]} to {st['pace'][1]} m/s. HOW TO PICK: every "
        "gait loop has a pace [lo, hi], the ground speeds in m/s it can be played at; take the family by his posture "
        "and speed (sprint for a run or a flight at 5 m/s or more, dash for an advance at 3.8 or more, jog_loaded for "
        "men under a load, run_tired for men who have run long, limp_l or limp_r for walking wounded, else as before) "
        "and in it the clip whose pace holds his speed (where two do, either; where none does, the nearest). A clip "
        "with style_of is another runner of that family (sprint_b, sprint_c, dash_b): style_of names the family only, "
        "and a clip chosen by its pace is played as it is, not picked again among its family by style (each has its "
        "own pace). Play run_stumble once, now and then, in a tired man's run, then the run from rejoin. Until "
        "cycles are baked for the new gaits, a far man's pose bank may stand them in as the old ones: sprint, "
        "sprint_b, sprint_c as flee; dash, dash_b as advance; jog_loaded, run_tired, run_stumble as run; limp_l, "
        "limp_r as reel_walk (or walk). The carry: "
        + ("the rifle in the right hand at the trail in " + ", ".join(trail) + (" and " + ", ".join(limp) if limp else "") + "; "
           if trail else "")
        + "his own hold on his weapon in the rest (the dashes take his advance's hold, the weapon low in both hands; "
        "the loaded jog his run's, held low across his belt). "
        "The gait loops that were here have a pace now too: " + old + ". Every clip is keyed 15 a second; footstep "
        "events are at the boots' contacts. If run_styles.py is delivered again it removes every clip that has a "
        "style_of: deliver the gaits again after it.")


# ================================================================================================ a file's bundle
def fingerprint(D):
    """What a bundle was keyed on: the skeleton and the clips it reads."""
    h = hashlib.sha1()
    h.update(json.dumps(D["bones"], separators=(",", ":"), sort_keys=True).encode())
    for n in ("run", "advance"):
        h.update(json.dumps(D["clips"][n], separators=(",", ":"), sort_keys=True).encode())
    for n in OLD:
        c = {k: v for k, v in D["clips"][n].items() if k != "pace"}
        h.update(json.dumps(c, separators=(",", ":"), sort_keys=True).encode())
    return h.hexdigest()


def bundle_for(name, D, cuts, runs, log=print):
    fit = Fit(name, D, cuts, runs, log=log)
    clips, infos, checks = {}, {}, {}
    for g in GAITS:
        quick = 1.0
        if g.get("quicken"):                       # (the stride on him first: the legs alone decide it)
            c0, _ = key_gait(fit, g, log=lambda *_: None, legs_only=True)
            v = c0["stride"] / c0["long"]
            quick = min(max(g["asked"] / v, 1.0 - QUICK_MOST), 1.0 + QUICK_MOST)
            quick = round(quick, 4) if abs(quick - 1.0) > (0.002 if g.get("exact") else 0.008) else 1.0
        if g["kind"] == "stumble":
            clip, info = key_stumble(fit, g, log=log)
        else:
            quick = keyable(fit.cut(g).long, quick, g["rate"])
            phase = 0.0
            kp = None
            if int(g.get("dense", 1)) > 1:
                kp = key_phase(fit, g, quick)
                phase = kp[0]
            clip, info = key_gait(fit, g, log=log, quick=quick, phase=phase)
            if kp:
                info["phase_chosen"] = {"boots_lowest_mm": round(1000 * kp[2], 1), "leg_pops": kp[3]}
        if g.get("style_of"):
            clip["style_of"] = g["style_of"]
        lo, hi, s_lo, s_hi, lim = pace_new(fit, g, clip)
        clip["pace"] = [lo, hi]
        clips[g["name"]], infos[g["name"]] = clip, info
    widen_paces(fit, clips, infos)
    pace_add = {}
    old_checks = {}
    for n in OLD:
        if n not in D["clips"]:
            continue
        lo, hi, s_lo, s_hi, lim = pace_old(fit, n)
        c = D["clips"][n]
        own = R.ball_slide(fit.sk, c)
        pace_add[n] = [lo, hi]
        old_checks[n] = {"speed_mps": round(c["stride"] / c["long"], 3), "pace": [lo, hi],
                         "slide_per_mps": round(own["slide"] / own["speed"], 4),
                         "pace_slide_per_mps": [round(s_lo, 4), round(s_hi, 4)], "pace_slide_limit": round(lim, 4),
                         "ok": bool(s_lo <= lim and s_hi <= lim)}
    faults = {}
    for g in GAITS:
        n = g["name"]
        clip, info = clips[n], infos[n]
        # (what the words need of the measures)
        m0, _ = check_clip(fit, g, dict(clip, **{"from": "", "about": ""}), log=log, made=info)
        clip["_m"] = {"carry": info["carry"], "muzzle": m0.get("muzzle_down_deg", [0, 0])}
        clip["from"] = from_of(fit, g, info)
        clip["about"] = about_of(fit, g, clip, info)
    text = about_gaits(fit, clips, pace_add)
    for g in GAITS:
        n = g["name"]
        clips[n].pop("_m")
        clips[n] = order_clip(clips[n])
        m, bad = check_clip(fit, g, clips[n], log=log, made=infos[n])
        m["made"] = infos[n]
        m["faults"] = bad
        checks[n] = m
        if bad:
            faults[n] = bad
        line = (f"   {n:11s} {m['carry']:5s} {m['speed_mps']:.2f} m/s pace {clips[n]['pace']}  slide {m['slide_per_mps']:.4f} per m/s "
                f"({m['slide_of_his_runs']:.2f} of his run's)  soles {m['sole_lowest_mm']} mm")
        if "fist_off_mm" in m:
            line += f"  fists {m['fist_off_mm']} mm  weapon clear {m['weapon_clear_cm']} cm of his {m['weapon_nearest']} (his own {m['weapon_clear_in_his_own_cm']})"
        if "free_arm_clear_cm" in m:
            line += f"  left arm clear {m['free_arm_clear_cm']} cm of his {m['free_arm_nearest']}"
        line += f"  fastest turn {m['fastest_turn_deg_s'][0]:.0f} deg/s {m['fastest_turn_deg_s'][1]}"
        if "loop_open_deg" in m:
            line += f"  loop {m['loop_open_deg']} deg"
        else:
            line += f"  ends {m['first_key_off_run_deg']} / {m['last_key_off_run_deg']} deg off his run's"
        log(line + ("  FAULT: " + "; ".join(bad) if bad else "  ok"))
    gaps = family_gaps({n: clips[n]["pace"] for n in clips})
    for n, v in old_checks.items():
        if not v["ok"]:
            faults.setdefault(n, []).append(f"pace {v['pace']}: slide {v['pace_slide_per_mps']} over the limit {v['pace_slide_limit']}")
    log("   pace of the loops that were here: " + ", ".join(f"{n} {v}" for n, v in pace_add.items()))
    log("   gaps inside the speeds the game will send: " + (json.dumps(gaps) if gaps else "none"))
    checks_all = {"source": {"fingerprint": fingerprint(D), "name": D.get("name")},
                  "his_run": {"speed_mps": round(fit.run_speed, 3), "slide_per_mps": round(fit.own_per, 4),
                              "slide_ahead_per_mps": round(fit.own_ahead, 4), "legs_of_reference": round(fit.k, 4)},
                  "clips": checks, "old_loops": old_checks, "pace_gaps": gaps, "pace_how": PACE_HOW,
                  "faults": faults, "pass": not faults}
    return {"clips": clips, "about_gaits": text, "pace_add": pace_add, "checks": checks_all}


def order_clip(c):
    first = ("long", "loop", "stride", "weapon", "tracks", "events", "style_of", "rejoin", "from", "about", "pace")
    return {k: c[k] for k in first if k in c} | {k: v for k, v in c.items() if k not in first}


def family_gaps(paces):
    """Where a family's paces leave a gap inside the speeds the game will send it: {family: [[from, to], ...]}."""
    out = {}
    for fam, names in FAMILY.items():
        lo, hi = FAMILY_BAND[fam]
        spans = sorted(tuple(paces[n]) for n in names if n in paces)
        gaps, at = [], lo
        for a, b in spans:
            if a > at + 1e-9:
                gaps.append([round(at, 2), round(min(a, hi), 2)])
            at = max(at, b)
            if at >= hi:
                break
        if at < hi - 1e-9:
            gaps.append([round(at, 2), round(hi, 2)])
        gaps = [g_ for g_ in gaps if g_[1] - g_[0] > 1e-9]
        if gaps:
            out[fam] = gaps
    return out


def merge(D, B):
    """The bundle merged into a soldier file's data, in place: the new clips (replacing its own from an earlier
    run), about_gaits, and "pace" on the gait loops that were there. Nothing else."""
    for n, c in B["clips"].items():
        D["clips"][n] = c
    for n, p in B["pace_add"].items():
        if n in D["clips"]:
            D["clips"][n]["pace"] = p
    D["about_gaits"] = B["about_gaits"]
    return D


def only_gaits_changed(before, after, B):
    """Faults if anything but the new clips, about_gaits and the old loops' pace differs between two files' data."""
    bad = []
    for k in set(before) | set(after):
        if k in ("clips", "about_gaits"):
            continue
        if before.get(k) != after.get(k):
            bad.append(f"'{k}' changed")
    for n in set(before["clips"]) | set(after["clips"]):
        if n in B["clips"]:
            continue
        a, b = before["clips"].get(n), after["clips"].get(n)
        if a is None or b is None:
            bad.append(f"clip {n} {'added' if a is None else 'gone'}")
            continue
        a2 = {k: v for k, v in a.items() if k != "pace"}
        b2 = {k: v for k, v in b.items() if k != "pace"}
        if a2 != b2:
            bad.append(f"clip {n} changed")
        if n not in B["pace_add"] and a.get("pace") != b.get("pace"):
            bad.append(f"clip {n}: pace changed")
    return bad


# ================================================================================================ the commands
def norm(n):
    return n[len("soldier_"):] if n.startswith("soldier_") else n


def source_of(n, src=None):
    if src:
        return os.path.join(src, f"soldier_{n}.json")
    return os.path.join(CONTENT if n in GUNNERS else SRC_INFANTRY, f"soldier_{n}.json")


def wanted(names):
    names = [norm(n) for n in names]
    every = INFANTRY + GUNNERS
    for n in names:
        if n not in every:
            raise SystemExit(f"{n}: not one of {every}")
    return [n for n in every if not names or n in names]


def dump(obj, path):
    tmp = path + ".part"
    with open(tmp, "w") as f:
        json.dump(obj, f, separators=(",", ":"))
    os.replace(tmp, path)


def stage(dir_, names, src=None, log=print):
    cuts, runs = json.load(open(CUTS)), json.load(open(RUNS))
    os.makedirs(dir_, exist_ok=True)
    report = {}
    for n in wanted(names):
        path = source_of(n, src)
        D = json.load(open(path))
        log(f"soldier_{n}  ({path})")
        B = bundle_for(n, D, cuts, runs, log=log)
        after = merge(json.loads(json.dumps(D)), B)
        bad = only_gaits_changed(D, after, B)
        if bad:
            B["checks"]["faults"]["merge"] = bad
            B["checks"]["pass"] = False
        dump(B, os.path.join(dir_, f"soldier_{n}.gaits.json"))
        report[n] = B["checks"]["faults"]
        log(f"   {'PASS' if not report[n] else 'FAIL ' + json.dumps(report[n])}")
    return report


def load_bundle(stage_dir, n):
    return json.load(open(os.path.join(stage_dir, f"soldier_{n}.gaits.json")))


def recheck(n, D, B, cuts, runs, log=print):
    """A bundle's clips checked again on a file's data (the bundle merged in memory)."""
    if fingerprint(D) != B["checks"]["source"]["fingerprint"]:
        return {"source": ["the file's skeleton, run or gait loops are not the ones this bundle was keyed on: stage again"]}
    fit = Fit(n, D, cuts, runs, log=lambda *_: None)
    fit._free_pts = {s: fit.body.points_of([f"{b}_{s}" for b in ("lowerarm", "hand", "fingers", "thumb")], 120) for s in "lr"}
    faults = {}
    for g in GAITS:
        c = B["clips"].get(g["name"])
        if c is None:
            faults[g["name"]] = ["missing"]
            continue
        m, bad = check_clip(fit, g, c, log=log, made=B["checks"]["clips"].get(g["name"], {}).get("made"))
        if bad:
            faults[g["name"]] = bad
    for nme, p in B["pace_add"].items():
        lo, hi, s_lo, s_hi, lim = pace_old(fit, nme)
        if [lo, hi] != p:
            faults.setdefault(nme, []).append(f"pace {p}, measured again {[lo, hi]}")
    after = merge(json.loads(json.dumps(D)), B)
    bad = only_gaits_changed(D, after, B)
    if bad:
        faults["merge"] = bad
    return faults


def apply(out, names, stage_dir=STAGE, src=None, log=print):
    os.makedirs(out, exist_ok=True)
    done = {}
    for n in wanted(names):
        D = json.load(open(source_of(n, src)))
        B = load_bundle(stage_dir, n)
        if fingerprint(D) != B["checks"]["source"]["fingerprint"]:
            log(f"soldier_{n}: REFUSED, the source is not the file this bundle was keyed on")
            done[n] = ["refused"]
            continue
        before = json.loads(json.dumps(D))
        merge(D, B)
        bad = only_gaits_changed(before, D, B)
        dump(D, os.path.join(out, f"soldier_{n}.json"))
        done[n] = bad
        log(f"soldier_{n}: written to {out} ({len(B['clips'])} clips, pace on {len(B['pace_add'])} loops)"
            + (" FAULT " + "; ".join(bad) if bad else ""))
    return done


def deliver(names, content=False, stage_dir=STAGE, log=print):
    """Merge the staged bundles into the shared soldier files (and the game's copies): see the top of this file."""
    cuts, runs = json.load(open(CUTS)), json.load(open(RUNS))
    stamp = time.strftime("%Y%m%dT%H%M%S")
    folders = [("shared", SHARED)] + ([("content", CONTENT)] if content else [])
    done, refused = [], []
    for n in wanted(names):
        B = load_bundle(stage_dir, n)
        if not B["checks"]["pass"]:
            refused.append((n, "the bundle has faults"))
            log(f"soldier_{n}: REFUSED, the staged bundle has faults {json.dumps(B['checks']['faults'])}")
            continue
        for where, folder in folders:
            path = os.path.join(folder, f"soldier_{n}.json")
            if not os.path.exists(path):
                log(f"soldier_{n}: no file in {folder}")
                continue
            for attempt in range(3):
                raw = open(path, "rb").read()
                D = json.loads(raw)
                faults = recheck(n, D, B, cuts, runs, log=lambda *_: None)
                if faults:
                    refused.append((n, where))
                    log(f"soldier_{n} ({where}): REFUSED {json.dumps(faults)}")
                    break
                os.makedirs(os.path.join(BACKUP, where), exist_ok=True)
                bk = os.path.join(BACKUP, where, f"soldier_{n}.{stamp}.json")
                if not os.path.exists(bk):
                    shutil.copy2(path, bk)
                merge(D, B)
                tmp = path + ".gaits.part"
                with open(tmp, "w") as f:
                    json.dump(D, f, separators=(",", ":"))
                if open(path, "rb").read() != raw:          # (the target changed while this ran: read it again)
                    os.remove(tmp)
                    log(f"soldier_{n} ({where}): the file changed under the merge, reading it again")
                    continue
                os.replace(tmp, path)
                done.append((n, where, path))
                log(f"soldier_{n} ({where}): delivered, backup {bk}")
                break
            else:
                refused.append((n, where))
                log(f"soldier_{n} ({where}): REFUSED, the file kept changing")
    if any(w == "shared" for _, w, _ in done):
        time.sleep(10)          # (the shared folder is a network mount: read back after it settles)
    for n, where, path in done:
        D = json.load(open(path))
        B = load_bundle(stage_dir, n)
        ok = all(D["clips"].get(k) == v for k, v in B["clips"].items()) and D.get("about_gaits") == B["about_gaits"] \
            and all(D["clips"][k].get("pace") == v for k, v in B["pace_add"].items())
        log(f"soldier_{n} ({where}): re-read {'PASS' if ok else 'FAIL'}")
    return done, refused


def table(stage_dir=STAGE, names=()):
    rows = []
    for n in wanted(names):
        p = os.path.join(stage_dir, f"soldier_{n}.gaits.json")
        if not os.path.exists(p):
            continue
        B = json.load(open(p))
        cells = []
        for g in GAITS:
            c = B["clips"][g["name"]]
            cells.append(f"{c['stride'] / c['long']:.2f} [{c['pace'][0]}-{c['pace'][1]}]")
        rows.append(f"{n:12s} " + "  ".join(cells) + ("" if B["checks"]["pass"] else "   FAULTS"))
    return "name         " + "  ".join(f"{g['name']:>16s}" for g in GAITS) + "\n" + "\n".join(rows)


def main(a):
    def opt(flag, default=None):
        if flag in a:
            i = a.index(flag)
            v = a[i + 1]
            del a[i:i + 2]
            return v
        return default
    src = opt("--src")
    stage_dir = opt("--stage-dir", STAGE)
    out = opt("--out")
    content = "--content" in a
    if content:
        a.remove("--content")
    if a and a[0] == "--stage":
        rest = a[1:]
        dir_ = STAGE
        if rest and (os.sep in rest[0] or os.path.isdir(rest[0])):
            dir_, rest = rest[0], rest[1:]
        rep = stage(dir_, rest, src=src)
        print("PASS" if not any(rep.values()) else "FAIL", {k: v for k, v in rep.items() if v})
        print(table(dir_, rest))
    elif a and a[0] == "--apply":
        if not out:
            raise SystemExit("--apply needs --out DIR")
        rep = apply(out, a[1:], stage_dir=stage_dir, src=src)
        print("PASS" if not any(rep.values()) else "FAIL", {k: v for k, v in rep.items() if v})
    elif a and a[0] == "--check":
        cuts, runs = json.load(open(CUTS)), json.load(open(RUNS))
        rep = {}
        for n in wanted(a[1:]):
            D = json.load(open(source_of(n, src)))
            B = load_bundle(stage_dir, n)
            rep[n] = recheck(n, D, B, cuts, runs, log=lambda *_: None)
            staged = B["checks"]["faults"]
            if staged:
                rep[n] = dict(rep[n], staged=staged)
            print(f"soldier_{n}: {'PASS' if not rep[n] else 'FAIL ' + json.dumps(rep[n])}")
        print("PASS" if not any(rep.values()) else "FAIL", {k: v for k, v in rep.items() if v})
    elif a and a[0] == "--deliver":
        done, refused = deliver(a[1:], content=content, stage_dir=stage_dir)
        print(f"delivered {len(done)}, refused {len(refused)}: {refused}")
    elif a and a[0] == "--table":
        print(table(stage_dir, a[1:]))
    else:
        print(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
