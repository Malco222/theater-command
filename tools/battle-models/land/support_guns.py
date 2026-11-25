"""The support teams' weapons (Brian, 25 Nov 2026: "There should be machine gun teams that lay down suppressing fire,
such as the German MG 42 and the US machine gun"): each army's machine gun on its tripod or wheeled mount, and its
mortar on baseplate and bipod. Made on the vehicles' kit (Works), so they come out in the same file shape as a tank's
parts: `parts`, `sockets` (where the crew's hands go), `controls` (what the game turns), near and far meshes.

    python export_support.py <out folder> [name,name,...|all] [json glb fbx]

Every weapon faces +x and stands on the ground at z = 0. Its root part is the mount (the tripod, the wheeled carriage,
the baseplate and bipod), with its origin on the ground under the pintle (a machine gun) or under the tube's foot (a
mortar). What the game turns is a second part: a machine gun's `gun` (its cradle, the gun, its sight and the first of
its belt: swung left and right and laid up and down about the pintle, and what the gunner carries on his shoulder
when the team moves), a mortar's `tube` (carried the same way). Seen at 10 to 60 m, so plain true shapes, about one
to two thousand triangles near, a third of that far. No national mark on anything.

The sockets every machine gun has: `muzzle` (the tracer and the flash), `feed` (where the number two's hands guide the
belt or the strip in, and which side that is), `grip.gunner.l` and `grip.gunner.r` (the gunner's hands), `seat` (on the
ground: where the gunner's own root goes, and so where the rules' key man stands; `controls.gunner.posture` says
whether he lies or sits there), `seat.loader` (the number two) and `box` (the ammunition on the ground). A mortar has
`muzzle` (the bomb goes in here, and the flash), `sight` (the gunner's eye comes to it), `grip.elevate` and
`grip.traverse` (his hands on the two cranks), `seat` (the gunner, kneeling on the left), `seat.loader` (the number
two, kneeling on the right by the muzzle) and `rounds` (the bombs laid ready).
"""
import math

from mathutils import Vector

from motorpool import X, Y, Z
from works import Works

# Paints (the weapons' own; a mount's paint is its army's, given by each builder).
STEEL = "3C3E42"          # blued or parkerised gun steel (above the art director's floor for small dark parts)
WOOD = "6E4E33"           # grips and a butt: oiled walnut or beech
LEATHER = "5C4430"        # the Lafette's two pads
CLOTH = "A29A80"          # a webbing belt of cartridges (Browning, Vickers, Maxim)
LINK = "7A6C48"           # a steel link belt with its brass showing
BRASS = "8E7B45"          # a feed strip of cartridges, a filler cap
RUBBER = "2E2C2A"         # the condenser's hose
GLASS = "26282B"          # a sight's lens


def _weapon(name, title, fine, mount, box=None):
    v = Works(name, title, "prop", "_", fine)
    for k, hx in (("steel", STEEL), ("wood", WOOD), ("leather", LEATHER), ("cloth", CLOTH), ("link", LINK), ("brass", BRASS),
                  ("rubber", RUBBER), ("glass", GLASS), ("mount", mount), ("box", box or mount)):
        v.m.paints[k] = hx
        v.m.paint_at[k] = list(v.m.paints).index(k)
    return v


def _cyl(v, a, b, r, paint, sides=10, r2=None):
    v.rod(a, b, r, paint, sides, r2=r2)


def _foot(v, at, kind, paint="mount"):
    """What a leg stands on: a round shoe, or a spade plate dug in behind it."""
    x, y, _ = at
    if kind == "shoe":
        v.lathe((x, y, 0.0), Z, [(0.014, 0.0), (0.014, 0.034), (0.0, 0.04), (0.0, 0.0)], paint, v.n(10, 6))
    elif kind == "spade":
        v.slab((x - 0.008, y - 0.04, -0.01), (x + 0.008, y + 0.04, 0.06), paint)
        v.slab((x - 0.05, y - 0.035, 0.0), (x + 0.03, y + 0.035, 0.012), paint)
    elif kind == "claw":
        v.slab((x - 0.03, y - 0.03, 0.0), (x + 0.03, y + 0.03, 0.014), paint)


def _belt(v, pts, wide, paint, thick=0.014):
    """Cartridges in their belt: a strip `wide` across (along the gun), hanging through these points."""
    a = X * (wide / 2.0)
    for p, q in zip(pts, pts[1:]):
        p, q = Vector(p), Vector(q)
        v.plate([p - a, p + a, q + a, q - a], thick, paint)


def _ammo_box(v, at, size, paint="box", lid=None, handle=True):
    """An ammunition box on the ground, its long side along x; `lid`: +1 or -1, its lid standing open on that side."""
    x, y = at[0], at[1]
    L, W, H = size
    v.box((L, W, H), (x, y, H / 2.0), paint, bevel=0.005)
    if lid:
        v.slab((x - L / 2, y + lid * (W / 2), H - 0.004), (x + L / 2, y + lid * (W / 2 + 0.006), H + W * 0.9), paint)
    elif handle and v.fine:
        v.tube([(x - 0.05, y, H), (x - 0.04, y, H + 0.025), (x + 0.04, y, H + 0.025), (x + 0.05, y, H)], 0.005, "steel", 4)


def _spade_grips(v, x, z, half=0.052, high=0.115, reach=0.05):
    """A Maxim's or a Vickers's twin handles behind the receiver: two wooden grips upright, between two steel bars,
    the thumb trigger between them."""
    for zz in (z + high / 2, z - high / 2):
        v.rod((x, -half - 0.008, zz), (x, half + 0.008, zz), 0.008, "steel", 6)
        v.rod((x + reach, 0.0, zz), (x, 0.0, zz), 0.009, "steel", 6)
    for s in (1, -1):
        v.rod((x, s * half, z - high / 2), (x, s * half, z + high / 2), 0.017, "wood", 8)
    if v.fine:
        v.slab((x - 0.004, -0.018, z - 0.02), (x + 0.012, 0.018, z + 0.02), "steel")


def _handwheel(v, at, axis, r=0.04, paint="steel"):
    v.lathe(at, axis, [(0.012, 0.0), (0.012, r), (0.0, r), (0.0, 0.0)], paint, v.n(12, 6))


def _controls(v, posture, feed_side, left=25.0, right=25.0, low=-8.0, high=12.0, burst="long bursts of 6 to 10, a pause, again", extra=None):
    c = {
        "traverse": {"part": "cradle", "what": "the game swings the gun left and right about the pintle (+ to the right)", "limits": [-left, right]},
        "elevate": {"part": "gun", "what": "the game lays the muzzle up and down (+ up)", "limits": [low, high]},
        "recoil": {"part": "gun", "what": "a shake of 3 to 5 mm back along the barrel on each round of a burst (the game's own; no clip)", "throw": 0.004},
        "gunner": {"posture": posture, "what": "how the gunner serves her: prone (lying behind, his elbows down) or sit (sitting on the ground behind, his knees up either side)"},
        "feed": {"side": feed_side, "what": "the side the belt or the strip comes in from, and so where the number two is (socket seat.loader)"},
        "carry": {"what": "when the team moves: the game shows the `gun` part alone on the gunner's shoulder (cradle and gun together), and puts the mount away until the number two has a clip that carries it", "fire": burst},
    }
    c.update(extra or {})
    v.m.info.setdefault("file", {})["controls"] = c


# ---------------------------------------------------------------------------------------------------------------------
# Germany: the MG 42 (or, before the summer of 1942, the MG 34) on the Lafette, set low.
# ---------------------------------------------------------------------------------------------------------------------
def lafette(fine=True, gun="mg42"):
    """The MG 42 on the Lafette 42, low, as a heavy machine gun (sMG): the gun in a sprung cradle that tips about its
    front, an optical sight on the cradle's left, the trigger handle under its back end, a front leg of two tubes with
    the two leather pads it is carried by on a man's back, and two rear legs splayed either side of the gunner, who
    lies behind with the butt at his shoulder. The belt comes in from the left out of a steel box. With gun="mg34",
    the MG 34 in the same mount (the Lafette 34): a round perforated barrel jacket and a slimmer butt."""
    is42 = gun == "mg42"
    v = _weapon("HmgMG42" if is42 else "HmgMG34", ("MG 42" if is42 else "MG 34") + " on the Lafette, set low", fine, "484A46", "50524A")
    pz, gz = 0.30, 0.42                       # the pivot's height (the cradle's front), the barrel's
    v.part("mount", None, (0, 0, 0), kind="root", note="on the ground under the cradle's front pivot; the game places it")
    # The lower frame: the head under the pivot, a beam back to the traverse arc, the arc itself.
    v.lathe((0.0, 0.0, 0.22), Z, [(0.07, 0.0), (0.07, 0.034), (0.0, 0.034), (0.0, 0.0)], "mount", v.n(10, 6))
    v.slab((-0.62, -0.022, 0.185), (0.0, 0.022, 0.225), "mount", bevel=0.004)
    v.slab((-0.655, -0.14, 0.175), (-0.595, 0.14, 0.205), "mount", bevel=0.004)
    # The front leg: two tubes to a clawed foot, the two pads lying on them.
    top, foot = Vector((0.03, 0.0, 0.225)), Vector((0.47, 0.0, 0.018))
    for s in (1, -1):
        v.rod(top + Y * (s * 0.042), foot + Y * (s * 0.042), 0.013, "mount", 6)
    v.rod(foot - Y * 0.075, foot + Y * 0.075, 0.014, "mount", 6)
    v.slab((foot.x - 0.01, -0.07, 0.0), (foot.x + 0.012, 0.07, 0.03), "mount")
    along = (foot - top).normalized()
    up = Y.cross(along).normalized()
    if up.z < 0:
        up = -up
    for t in (0.34, 0.72):
        c = top + (foot - top) * t + up * (0.013 + 0.036)
        v.plate([c - along * 0.075 - Y * 0.075, c + along * 0.075 - Y * 0.075, c + along * 0.075 + Y * 0.075, c - along * 0.075 + Y * 0.075][::-1],
                0.036, "leather", bevel=0.008)
    # The rear legs, splayed, on spade feet.
    for s in (1, -1):
        v.rod((-0.60, s * 0.11, 0.19), (-0.73, s * 0.41, 0.012), 0.015, "mount", 6)
        _foot(v, (-0.73, s * 0.41, 0.0), "spade")
    # The cradle's carrier: what swings left and right on the arc (the search-fire gear and its handwheel).
    v.part("cradle", "mount", (0.0, 0.0, pz), axis=(0, 0, -1), limits=[-25.0, 25.0], kind="turn",
           note="swings about the cradle's front pivot along the arc: + the muzzle to the right")
    v.lathe((0.0, 0.0, pz - 0.02), Z, [(0.05, 0.0), (0.05, 0.026), (0.0, 0.026), (0.0, 0.0)], "mount", v.n(10, 6))
    v.slab((-0.66, -0.035, 0.205), (-0.58, 0.035, 0.30), "mount", bevel=0.004)
    v.rod((-0.62, 0.035, 0.25), (-0.62, 0.062, 0.25), 0.008, "steel", 6)
    _handwheel(v, (-0.62, 0.062, 0.25), Y, 0.042)
    v.socket("grip.gunner.l", (-0.62, 0.085, 0.25), Y, part="cradle", note="the gunner's left hand on the elevating wheel (or steadying the butt)")
    # The gun in its cradle: tips about the cradle's front.
    v.part("gun", "cradle", (0.0, 0.0, pz + 0.03), axis=(0, -1, 0), limits=[-8.0, 12.0], kind="turn",
           note="the cradle and the gun tip about the cradle's front: + the muzzle up; and what is carried on a shoulder")
    for s in (1, -1):                         # the cradle: two rails, the buffer springs' housings on them
        v.rod((-0.68, s * 0.034, 0.345), (0.02, s * 0.034, 0.345), 0.011, "mount", 6)
        v.rod((-0.36, s * 0.034, 0.345), (-0.10, s * 0.034, 0.345), 0.021, "mount", 8)
    v.slab((-0.03, -0.045, 0.325), (0.03, 0.045, gz - 0.02), "mount", bevel=0.004)          # the front clamp
    v.slab((-0.70, -0.04, 0.325), (-0.63, 0.04, gz - 0.035), "mount", bevel=0.004)          # the rear clamp and trigger gear
    v.rod((-0.71, 0.0, 0.335), (-0.775, 0.0, 0.255), 0.016, "wood", 8)                       # the trigger handle
    v.socket("grip.gunner.r", (-0.745, 0.0, 0.29), -X, part="gun", note="the gunner's right hand on the cradle's trigger handle")
    # The sight on the cradle's left: a bracket, the body, the eyepiece back to his eye.
    v.rod((-0.56, 0.045, 0.35), (-0.56, 0.11, 0.50), 0.009, "mount", 5)
    v.box((0.10, 0.05, 0.065), (-0.56, 0.115, 0.535), "mount", bevel=0.006)
    v.rod((-0.61, 0.115, 0.535), (-0.685, 0.115, 0.535), 0.015, "mount", 8)
    v.rod((-0.51, 0.115, 0.55), (-0.455, 0.115, 0.55), 0.017, "mount", 8)
    v.socket("sight", (-0.69, 0.115, 0.535), -X, part="gun", note="the optical sight's eyepiece: the gunner's eye for long fire")
    # The gun.
    if is42:
        v.box((0.62, 0.046, 0.058), (-0.11, 0.0, gz), "steel", bevel=0.008)                  # the square barrel casing
        v.lathe((0.20, 0.0, gz), X, [(0.10, 0.0), (0.10, 0.019), (0.07, 0.024), (0.0, 0.018), (0.0, 0.0)], "steel", v.n(10, 6))   # the muzzle booster
        v.box((0.22, 0.05, 0.075), (-0.53, 0.0, gz - 0.004), "steel", bevel=0.006)           # the receiver
        v.slab((-0.62, -0.027, gz + 0.03), (-0.40, 0.027, gz + 0.046), "steel", bevel=0.004)  # the feed cover
        v.extrude([(-0.64, gz + 0.022), (-0.92, gz + 0.03), (-0.92, gz - 0.085), (-0.80, gz - 0.04), (-0.64, gz - 0.022)], -0.018, 0.018, "wood", bevel=0.004)
    else:
        v.rod((-0.42, 0.0, gz), (0.20, 0.0, gz), 0.023, "steel", 10)                         # the round jacket
        v.lathe((0.20, 0.0, gz), X, [(0.10, 0.0), (0.10, 0.015), (0.06, 0.02), (0.0, 0.016), (0.0, 0.0)], "steel", v.n(10, 6))
        v.rod((-0.64, 0.0, gz), (-0.42, 0.0, gz), 0.027, "steel", 10)                        # the receiver, round
        v.slab((-0.62, -0.024, gz + 0.022), (-0.44, 0.024, gz + 0.04), "steel", bevel=0.004)
        v.extrude([(-0.64, gz + 0.016), (-0.92, gz + 0.012), (-0.92, gz - 0.07), (-0.82, gz - 0.03), (-0.64, gz - 0.02)], -0.016, 0.016, "wood", bevel=0.004)
    v.slab((-0.575, -0.014, gz - 0.11), (-0.545, 0.014, gz - 0.03), "wood")                 # the gun's own grip
    v.slab((-0.55, 0.024, gz + 0.012), (-0.45, 0.07, gz + 0.022), "steel")                  # the feed tray, out to the left
    _belt(v, [(-0.50, 0.03, gz + 0.03), (-0.50, 0.10, gz + 0.02), (-0.50, 0.19, gz - 0.14), (-0.50, 0.235, gz - 0.255)], 0.075, "link")
    v.socket("muzzle", (0.30, 0.0, gz), X, part="gun", note="the muzzle: the flash and the tracer")
    v.socket("feed", (-0.50, 0.07, gz + 0.025), Y, part="gun", note="the belt comes in here from the left: the number two's hands")
    v.socket("eject", (-0.50, 0.0, gz - 0.045), -Z, part="gun", note="the empties fall out underneath")
    v.socket("shoulder", (-0.92, 0.0, gz - 0.02), -X, part="gun", note="the butt: the gunner's right shoulder")
    v.use("mount")
    _ammo_box(v, (-0.50, 0.275, 0.0), (0.36, 0.09, 0.17))
    v.socket("box", (-0.50, 0.275, 0.17), Z, part="mount", note="the belt box on the ground at the gun's left")
    v.socket("seat", (-1.39, 0.20, 0.0), X, part="mount", note="the gunner's root, lying behind her a little to the left: his right shoulder at the butt 0.47 m ahead, his eye by the sight")
    v.socket("seat.loader", (-0.78, 0.62, 0.0), X, part="mount", note="the number two, lying at her left by the belt box")
    _controls(v, "prone", "left", burst="long bursts of 8 to 12 at 1,200 a minute (the MG 42; the MG 34 at 850), a pause, again")
    return v


# ---------------------------------------------------------------------------------------------------------------------
# The United States: the Browning M1919A4 on the M2 tripod (and, water-cooled, the M1917A1 pattern on its own).
# ---------------------------------------------------------------------------------------------------------------------
def browning(fine=True, kind="m1919"):
    """The Browning M1919A4 on the M2 tripod: a slab-sided receiver, a perforated barrel jacket, a pistol grip at the
    back, on a low tripod whose two rear legs carry a traversing bar; the belt comes from the left out of an M1 box
    with its lid up. kind="m1917": the water-cooled pattern (a fat smooth jacket, a higher tripod): the M1917A1, and
    the Polish wz. 30 made from it."""
    air = kind == "m1919"
    v = _weapon("HmgM1919" if air else "HmgM1917", "Browning M1919A4 on the M2 tripod" if air else "water-cooled Browning on its tripod", fine,
                "4B4A32" if air else "4F5238", "4B4A32" if air else "4F5238")
    pz = 0.26 if air else 0.40
    gz = pz + 0.07
    spread = 1.0 if air else 1.15
    v.part("mount", None, (0, 0, 0), kind="root", note="on the ground under the pintle; the game places it")
    v.lathe((0.0, 0.0, pz - 0.07), Z, [(0.07, 0.0), (0.07, 0.03), (0.0, 0.034), (0.0, 0.0)], "mount", v.n(10, 6))
    hz = pz - 0.04
    fx = 0.44 * spread
    v.rod((0.03, 0.0, hz), (fx, 0.0, 0.015), 0.016, "mount", 6)
    _foot(v, (fx, 0.0, 0.0), "claw")
    rear = (-0.56 * spread, 0.37 * spread)
    for s in (1, -1):
        v.rod((-0.03, s * 0.03, hz), (rear[0], s * rear[1], 0.015), 0.016, "mount", 6)
        _foot(v, (rear[0], s * rear[1], 0.0), "shoe")
    # The traversing bar between the rear legs.
    t = 0.64
    bx, by, bz = -0.03 + (rear[0] + 0.03) * t, 0.03 + (rear[1] - 0.03) * t, hz + (0.015 - hz) * t
    v.slab((bx - 0.012, -by, bz - 0.01), (bx + 0.012, by, bz + 0.01), "mount")
    v.part("cradle", "mount", (0.0, 0.0, pz), axis=(0, 0, -1), limits=[-22.0, 22.0], kind="turn", note="swings on the pintle: + the muzzle to the right")
    v.lathe((0.0, 0.0, pz - 0.01), Z, [(0.04, 0.0), (0.04, 0.02), (0.0, 0.02), (0.0, 0.0)], "steel", v.n(8, 5))
    # The traversing and elevating gear: from under the receiver's back down to the bar, its handwheel on it.
    v.rod((-0.27, 0.0, gz - 0.05), (bx, 0.0, bz + 0.012), 0.012, "steel", 6)
    _handwheel(v, (-0.27 + (bx + 0.27) * 0.5, 0.0, (gz - 0.05 + bz) * 0.5 - 0.006), Z, 0.032)
    v.socket("grip.gunner.l", (-0.27 + (bx + 0.27) * 0.5, 0.03, (gz - 0.05 + bz) * 0.5), Y, part="cradle", note="the gunner's left hand on the elevating wheel")
    v.part("gun", "cradle", (0.0, 0.0, pz + 0.02), axis=(0, -1, 0), limits=[-10.0, 14.0], kind="turn", note="tips on the pintle: + the muzzle up; and what is carried on a shoulder")
    v.box((0.36, 0.05, 0.10), (-0.16, 0.0, gz), "steel", bevel=0.006)                         # the receiver
    v.slab((-0.17, -0.027, gz + 0.05), (0.02, 0.027, gz + 0.064), "steel", bevel=0.003)       # the top cover
    v.slab((-0.31, -0.012, gz + 0.05), (-0.29, 0.012, gz + 0.10), "steel")                    # the rear sight's leaf
    if air:
        v.rod((0.02, 0.0, gz + 0.005), (0.55, 0.0, gz + 0.005), 0.023, "steel", 10)          # the perforated jacket
        v.rod((0.55, 0.0, gz + 0.005), (0.60, 0.0, gz + 0.005), 0.016, "steel", 8)
        mx = 0.60
    else:
        v.rod((0.02, 0.0, gz + 0.005), (0.62, 0.0, gz + 0.005), 0.047, "mount", 12)          # the water jacket
        v.rod((0.62, 0.0, gz + 0.005), (0.67, 0.0, gz + 0.005), 0.018, "steel", 8)
        v.rod((0.50, 0.0, gz + 0.05), (0.50, 0.0, gz + 0.075), 0.014, "brass", 6)            # the filler
        mx = 0.67
    v.slab((0.50, -0.004, gz + 0.025), (0.52, 0.004, gz + (0.06 if air else 0.085)), "steel")  # the front sight
    v.rod((-0.345, 0.0, gz - 0.035), (-0.385, 0.0, gz - 0.125), 0.017, "wood", 8)            # the pistol grip
    v.socket("grip.gunner.r", (-0.365, 0.0, gz - 0.08), -X, part="gun", note="the gunner's right hand on the pistol grip")
    _belt(v, [(-0.05, 0.028, gz + 0.03), (-0.05, 0.10, gz + 0.02), (-0.05, 0.19, gz - 0.11), (-0.05, 0.225, 0.20)], 0.075, "cloth")
    v.socket("muzzle", (mx, 0.0, gz + 0.005), X, part="gun", note="the muzzle: the flash and the tracer")
    v.socket("feed", (-0.05, 0.06, gz + 0.03), Y, part="gun", note="the belt comes in here from the left: the number two's hands")
    v.socket("eject", (-0.05, 0.0, gz - 0.05), -Z, part="gun", note="the empties fall out underneath")
    v.use("mount")
    _ammo_box(v, (-0.05, 0.24, 0.0), (0.28, 0.095, 0.185), lid=1)
    v.socket("box", (-0.05, 0.24, 0.185), Z, part="mount", note="the belt box on the ground at the gun's left, its lid up")
    if not air:                                   # the water can and its hose, at the right
        v.box((0.20, 0.13, 0.26), (0.30, -0.36, 0.13), "mount", bevel=0.01)
        v.tube([(0.56, -0.02, gz - 0.035), (0.52, -0.16, gz - 0.16), (0.36, -0.33, 0.30), (0.32, -0.36, 0.26)], 0.011, "rubber", 5)
    posture = "sit" if not air else "prone"
    v.socket("seat", (-1.13, 0.20, 0.0), X, part="mount",
             note="the gunner's root: lying behind her a little to the left, his right hand 0.77 m ahead on the pistol grip")
    v.socket("seat.loader", (-0.35, 0.62, 0.0), X, part="mount", note="the number two, lying at her left by the belt box")
    _controls(v, posture, "left", left=22.0, right=22.0, low=-10.0, high=14.0, burst="bursts of 6 to 9 at 450 a minute, a pause, again")
    return v


# ---------------------------------------------------------------------------------------------------------------------
# Britain: the Vickers on its tripod.
# ---------------------------------------------------------------------------------------------------------------------
def vickers(fine=True):
    """The Vickers Mk I on the Mk IV tripod: a fat water jacket, a slab receiver with two spade grips, on a crosshead
    over two short front legs and a long rear one; the gunner sits behind astride the rear leg. The belt comes from
    the RIGHT out of a wooden-lined box, and a hose runs from the jacket's nose to a condenser can at the left."""
    v = _weapon("HmgVickers", "Vickers Mk I on the Mk IV tripod", fine, "4A4D3A", "5A5540")
    pz, gz = 0.50, 0.60
    v.part("mount", None, (0, 0, 0), kind="root", note="on the ground under the crosshead; the game places it")
    v.lathe((0.0, 0.0, pz - 0.14), Z, [(0.12, 0.0), (0.12, 0.034), (0.0, 0.04), (0.0, 0.0)], "mount", v.n(10, 6))
    for s in (1, -1):
        v.rod((0.03, s * 0.04, pz - 0.10), (0.44, s * 0.39, 0.015), 0.019, "mount", 6)
        _foot(v, (0.44, s * 0.39, 0.0), "shoe")
    v.rod((-0.03, 0.0, pz - 0.10), (-0.82, 0.0, 0.02), 0.021, "mount", 6)
    _foot(v, (-0.82, 0.0, 0.0), "spade")
    v.part("cradle", "mount", (0.0, 0.0, pz), axis=(0, 0, -1), limits=[-30.0, 30.0], kind="turn", note="the crosshead turns in its socket: + the muzzle to the right")
    v.slab((-0.035, -0.045, pz - 0.03), (0.035, 0.045, pz + 0.03), "mount", bevel=0.004)
    v.rod((-0.24, 0.0, gz - 0.075), (-0.10, 0.0, pz - 0.04), 0.013, "steel", 6)              # the elevating screw
    _handwheel(v, (-0.17, 0.0, pz - 0.012), Z, 0.05)
    v.part("gun", "cradle", (0.0, 0.0, pz + 0.03), axis=(0, -1, 0), limits=[-10.0, 14.0], kind="turn", note="tips on the crosshead's pin: + the muzzle up; and what is carried on a shoulder")
    v.box((0.38, 0.068, 0.135), (-0.19, 0.0, gz), "steel", bevel=0.006)                       # the breech casing
    v.rod((0.0, 0.0, gz + 0.005), (0.64, 0.0, gz + 0.005), 0.05, "mount", 12)                # the water jacket
    v.rod((0.64, 0.0, gz + 0.005), (0.70, 0.0, gz + 0.005), 0.02, "steel", 8)
    v.lathe((0.70, 0.0, gz + 0.005), X, [(0.04, 0.0), (0.04, 0.03), (0.0, 0.034), (0.0, 0.0)], "steel", v.n(10, 6))   # the muzzle cup
    v.rod((0.08, 0.0, gz + 0.05), (0.08, 0.0, gz + 0.075), 0.014, "brass", 6)                # the filler plug
    v.slab((-0.11, -0.05, gz + 0.065), (0.0, 0.05, gz + 0.105), "steel", bevel=0.004)         # the feed block
    v.slab((-0.345, -0.014, gz + 0.065), (-0.325, 0.014, gz + 0.165), "steel")                # the rear sight's leaf, up
    v.slab((0.60, -0.004, gz + 0.05), (0.62, 0.004, gz + 0.085), "steel")
    _spade_grips(v, -0.43, gz)
    v.socket("grip.gunner.l", (-0.43, 0.052, gz), -X, part="gun", note="the gunner's left hand on its spade grip")
    v.socket("grip.gunner.r", (-0.43, -0.052, gz), -X, part="gun", note="the gunner's right hand on its spade grip")
    _belt(v, [(-0.055, -0.05, gz + 0.085), (-0.055, -0.12, gz + 0.07), (-0.055, -0.22, gz - 0.20), (-0.055, -0.25, 0.20)], 0.075, "cloth")
    v.socket("muzzle", (0.74, 0.0, gz + 0.005), X, part="gun", note="the muzzle: the flash and the tracer")
    v.socket("feed", (-0.055, -0.085, gz + 0.08), -Y, part="gun", note="the belt comes in here from the RIGHT: the number two's hands")
    v.socket("eject", (-0.02, 0.0, gz - 0.07), -Z, part="gun", note="the empties fall out underneath")
    v.use("mount")
    _ammo_box(v, (-0.055, -0.275, 0.0), (0.44, 0.10, 0.19), lid=-1)
    v.socket("box", (-0.055, -0.275, 0.19), Z, part="mount", note="the belt box on the ground at the gun's right, its lid up")
    v.box((0.24, 0.15, 0.32), (0.42, 0.40, 0.16), "mount", bevel=0.012)                       # the condenser can
    v.rod((0.42, 0.40, 0.32), (0.42, 0.40, 0.345), 0.022, "steel", 8)
    v.tube([(0.60, 0.02, gz - 0.04), (0.60, 0.16, gz - 0.17), (0.50, 0.33, 0.40), (0.44, 0.40, 0.345)], 0.012, "rubber", 5)
    v.socket("seat", (-0.78, 0.20, 0.0), X, part="mount", note="the gunner's root: sitting (today kneeling) behind her by the rear leg, his hands 0.35 m ahead on the grips")
    v.socket("seat.loader", (-0.30, -0.66, 0.0), X, part="mount", note="the number two, lying at her right by the belt box")
    _controls(v, "sit", "right", left=30.0, right=30.0, low=-10.0, high=14.0, burst="long steady bursts of 10 to 20 at 500 a minute, tapped across a front")
    return v


# ---------------------------------------------------------------------------------------------------------------------
# The Soviet Union: the Maxim of 1910 on Sokolov's wheeled mount, with its shield.
# ---------------------------------------------------------------------------------------------------------------------
def maxim(fine=True):
    """The Maxim M1910 on the Sokolov mount: a fat smooth water jacket with the wide filler cap of 1941, spade grips,
    a steel shield, two small spoked steel wheels and a U-shaped trail the crew pull her by. The belt comes from the
    RIGHT out of a green steel box. The gunner sits (or lies) behind the trail's end."""
    v = _weapon("HmgMaxim", "Maxim M1910 on the Sokolov mount", fine, "4E5637", "4E5637")
    pz, gz, wr, wy = 0.37, 0.50, 0.205, 0.29
    v.part("mount", None, (0, 0, 0), kind="root", note="on the ground under the turntable, between her wheels; the game places it")
    v.rod((0.0, -wy - 0.03, wr), (0.0, wy + 0.03, wr), 0.018, "mount", 8)                     # the axle
    for s in (1, -1):
        y = s * wy
        ax = Y if s > 0 else -Y
        v.lathe((0.0, y - s * 0.02, wr), ax, [(0.0, wr - 0.03), (0.04, wr - 0.03), (0.04, wr), (0.0, wr), (0.0, wr - 0.03)], "mount", v.n(18, 9))   # the rim
        v.lathe((0.0, y - s * 0.025, wr), ax, [(0.05, 0.0), (0.05, 0.035), (0.0, 0.035), (0.0, 0.0)], "mount", v.n(8, 5))                           # the hub
        for k in range(8 if fine else 4):
            a = 2 * math.pi * k / (8 if fine else 4)
            v.rod((0.035 * math.cos(a), y, wr + 0.035 * math.sin(a)), ((wr - 0.03) * math.cos(a), y, wr + (wr - 0.03) * math.sin(a)), 0.007, "mount", 4)
    v.slab((-0.13, -0.11, wr + 0.015), (0.13, 0.11, wr + 0.05), "mount", bevel=0.005)          # the table
    v.lathe((0.0, 0.0, wr + 0.05), Z, [(pz - wr - 0.05, 0.0), (pz - wr - 0.05, 0.045), (0.0, 0.055), (0.0, 0.0)], "mount", v.n(10, 6))
    v.tube([(-0.06, 0.17, wr + 0.01), (-0.72, 0.13, 0.05), (-0.86, 0.0, 0.035), (-0.72, -0.13, 0.05), (-0.06, -0.17, wr + 0.01)], 0.017, "mount", 6)   # the trail
    v.rod((-0.86, -0.07, 0.035), (-0.86, 0.07, 0.035), 0.024, "wood", 8)                      # its handle
    v.socket("pull", (-0.86, 0.0, 0.06), -X, part="mount", note="the trail's handle: where a man takes her to pull her along on her wheels")
    v.part("cradle", "mount", (0.0, 0.0, pz), axis=(0, 0, -1), limits=[-35.0, 35.0], kind="turn", note="turns on the table: + the muzzle to the right (the shield turns with her)")
    v.slab((-0.05, -0.05, pz - 0.01), (0.05, 0.05, pz + 0.05), "mount", bevel=0.004)
    v.rod((-0.25, 0.0, gz - 0.07), (-0.17, 0.0, wr + 0.06), 0.013, "steel", 6)               # the elevating screw
    _handwheel(v, (-0.21, 0.0, (gz - 0.07 + wr + 0.06) / 2), Z, 0.045)
    # The shield: four plates round the jacket's hole.
    sx, top, bot, half, hole = 0.03, 0.74, 0.30, 0.27, 0.07
    v.slab((sx, hole, bot), (sx + 0.008, half, top), "mount")
    v.slab((sx, -half, bot), (sx + 0.008, -hole, top), "mount")
    v.slab((sx, -hole, gz + 0.075), (sx + 0.008, hole, top), "mount")
    v.slab((sx, -hole, bot), (sx + 0.008, hole, gz - 0.065), "mount")
    v.part("gun", "cradle", (0.0, 0.0, pz + 0.05), axis=(0, -1, 0), limits=[-8.0, 12.0], kind="turn", note="tips on her trunnions: + the muzzle up")
    v.box((0.34, 0.07, 0.13), (-0.17, 0.0, gz), "steel", bevel=0.006)                         # the receiver
    v.rod((0.0, 0.0, gz + 0.005), (0.66, 0.0, gz + 0.005), 0.055, "mount", 12)               # the water jacket
    v.rod((0.66, 0.0, gz + 0.005), (0.74, 0.0, gz + 0.005), 0.024, "steel", 8)
    v.lathe((0.36, 0.0, gz + 0.052), Z, [(0.025, 0.0), (0.025, 0.045), (0.0, 0.045), (0.0, 0.0)], "mount", v.n(10, 6))   # the wide filler cap
    v.slab((-0.10, -0.05, gz + 0.065), (0.0, 0.05, gz + 0.10), "steel", bevel=0.004)          # the feed block
    v.slab((-0.30, -0.014, gz + 0.065), (-0.28, 0.014, gz + 0.15), "steel")                   # the rear sight
    _spade_grips(v, -0.39, gz)
    v.socket("grip.gunner.l", (-0.39, 0.052, gz), -X, part="gun", note="the gunner's left hand on its spade grip")
    v.socket("grip.gunner.r", (-0.39, -0.052, gz), -X, part="gun", note="the gunner's right hand on its spade grip")
    _belt(v, [(-0.05, -0.05, gz + 0.08), (-0.05, -0.12, gz + 0.065), (-0.05, -0.20, gz - 0.14), (-0.05, -0.24, 0.215)], 0.075, "cloth")
    v.socket("muzzle", (0.74, 0.0, gz + 0.005), X, part="gun", note="the muzzle: the flash and the tracer")
    v.socket("feed", (-0.05, -0.085, gz + 0.075), -Y, part="gun", note="the belt comes in here from the RIGHT: the number two's hands")
    v.socket("eject", (0.0, 0.0, gz - 0.07), -Z, part="gun", note="the empties fall out forward, under the jacket")
    v.use("mount")
    _ammo_box(v, (-0.05, -0.50, 0.0), (0.46, 0.09, 0.21), lid=-1)
    v.socket("box", (-0.05, -0.50, 0.21), Z, part="mount", note="the belt box on the ground at her right, outside the wheel")
    v.socket("seat", (-0.74, 0.20, 0.0), X, part="mount", note="the gunner's root: sitting (today kneeling) inside the trail's loop, his hands 0.35 m ahead on the grips, the shield before him")
    v.socket("seat.loader", (-0.30, -0.80, 0.0), X, part="mount", note="the number two, lying at her right by the belt box")
    _controls(v, "sit", "right", left=35.0, right=35.0, burst="long steady bursts of 10 to 20 at 550 a minute",
              extra={"carry": {"what": "when the team moves she is not lifted: the game keeps her whole and rolls her behind the gunner on her wheels, her trail's handle (socket pull) in his hand; her wheels are 0.41 m across", "wheels": 0.41}})
    return v


# ---------------------------------------------------------------------------------------------------------------------
# Japan's Type 92, France's Hotchkiss of 1914 and Italy's Breda 37: finned barrels fed by strips, on tripods.
# ---------------------------------------------------------------------------------------------------------------------
def strip_gun(fine=True, kind="type92"):
    """An air-cooled heavy machine gun fed by rigid strips of cartridges pushed in from the left: a slab receiver, a
    barrel with deep cooling fins, on a tripod of two front legs and one rear. type92: Japan's Type 92 (the Hotchkiss
    pattern made again: two grips that fold down, sockets on the legs for the carrying poles, a flash hider);
    hotchkiss: France's Mle 1914 (a pistol grip, five great fins by the receiver, a seat on the rear leg);
    breda37: Italy's Breda Mod. 37 (spade grips, a heavier finned barrel)."""
    name, title, mount, box = {
        "type92": ("HmgType92", "Type 92 heavy machine gun on its tripod", "4A4538", "6E5B3C"),
        "hotchkiss": ("HmgHotchkiss", "Hotchkiss Mle 1914 on its tripod", "4F5440", "5A5540"),
        "breda37": ("HmgBreda37", "Breda Mod. 37 on its tripod", "5B5F4A", "5B5F4A"),
    }[kind]
    v = _weapon(name, title, fine, mount, box)
    pz = {"type92": 0.34, "hotchkiss": 0.42, "breda37": 0.36}[kind]
    gz = pz + 0.10
    v.part("mount", None, (0, 0, 0), kind="root", note="on the ground under the pintle; the game places it")
    v.lathe((0.0, 0.0, pz - 0.10), Z, [(0.10, 0.0), (0.10, 0.03), (0.03, 0.06), (0.0, 0.06), (0.0, 0.0)], "mount", v.n(10, 6))
    hz = pz - 0.07
    for s in (1, -1):
        v.rod((0.04, s * 0.04, hz), (0.46, s * 0.37, 0.015), 0.019, "mount", 6)
        _foot(v, (0.46, s * 0.37, 0.0), "shoe")
        if kind == "type92":                                  # the sockets the carrying poles go through
            v.rod((0.40, s * 0.37, 0.07), (0.52, s * 0.37, 0.07), 0.024, "mount", 8)
    v.rod((-0.04, 0.0, hz), (-0.74, 0.0, 0.02), 0.02, "mount", 6)
    _foot(v, (-0.74, 0.0, 0.0), "spade")
    if kind == "type92":
        v.rod((-0.80, 0.0, 0.07), (-0.68, 0.0, 0.07), 0.024, "mount", 8)
    if kind == "hotchkiss":                                   # the gunner's seat on the rear leg
        v.lathe((-0.66, 0.0, 0.12), Z, [(0.02, 0.0), (0.02, 0.11), (0.0, 0.10), (0.0, 0.0)], "mount", v.n(10, 6))
    v.part("cradle", "mount", (0.0, 0.0, pz), axis=(0, 0, -1), limits=[-30.0, 30.0], kind="turn", note="turns on the pintle: + the muzzle to the right")
    v.slab((-0.04, -0.045, pz - 0.02), (0.04, 0.045, pz + 0.04), "mount", bevel=0.004)
    v.rod((-0.26, 0.0, gz - 0.06), (-0.12, 0.0, pz - 0.05), 0.012, "steel", 6)               # the elevating screw
    _handwheel(v, (-0.19, 0.0, pz - 0.015), Z, 0.045)
    v.part("gun", "cradle", (0.0, 0.0, pz + 0.04), axis=(0, -1, 0), limits=[-10.0, 14.0], kind="turn", note="tips on the pintle: + the muzzle up; and what is carried on a shoulder")
    rl = {"type92": 0.40, "hotchkiss": 0.44, "breda37": 0.42}[kind]
    v.box((rl, 0.06, 0.115), (-rl / 2, 0.0, gz), "steel", bevel=0.006)                        # the receiver
    # The barrel: fins by the receiver, then plain to the muzzle.
    fins, f0, f1, hi, lo = {"type92": (9, 0.0, 0.40, 0.046, 0.022), "hotchkiss": (5, 0.0, 0.13, 0.05, 0.024), "breda37": (10, 0.02, 0.50, 0.036, 0.022)}[kind]
    bz = gz + 0.012
    if fine:
        pitch = (f1 - f0) / fins
        prof = [(f1, 0.0)]
        for k in range(fins, 0, -1):
            d = f0 + pitch * k
            prof += [(d, lo), (d - pitch * 0.25, lo), (d - pitch * 0.25, hi), (d - pitch * 0.7, hi), (d - pitch * 0.7, lo)]
        prof += [(f0, lo), (f0, 0.0)]
        v.lathe((0.0, 0.0, bz), X, prof, "steel", 8)
    else:
        v.rod((f0, 0.0, bz), (f1, 0.0, bz), (hi + lo) / 2, "steel", 8)
    ml = {"type92": 0.70, "hotchkiss": 0.80, "breda37": 0.68}[kind]
    v.rod((f1, 0.0, bz), (ml, 0.0, bz), 0.016 if kind != "hotchkiss" else 0.019, "steel", 8)
    if kind == "type92":
        v.lathe((ml, 0.0, bz), X, [(0.056, 0.0), (0.056, 0.025), (0.0, 0.015), (0.0, 0.0)], "steel", v.n(8, 5))   # the flash hider
        ml += 0.056
        for s in (1, -1):                                     # the two grips, folded down and out
            v.rod((-rl, s * 0.03, gz - 0.02), (-rl - 0.06, s * 0.06, gz - 0.11), 0.016, "wood", 8)
        gl, gr = (-rl - 0.04, 0.05, gz - 0.08), (-rl - 0.04, -0.05, gz - 0.08)
        v.box((0.07, 0.05, 0.035), (-rl * 0.45, 0.0, gz + 0.075), "steel", bevel=0.004)      # the oil tank on the top
    elif kind == "hotchkiss":
        v.rod((-rl + 0.01, 0.0, gz - 0.05), (-rl - 0.03, 0.0, gz - 0.15), 0.018, "wood", 8)  # the pistol grip
        v.rod((-rl, 0.0, gz + 0.01), (-rl - 0.07, 0.0, gz + 0.01), 0.014, "steel", 6)        # the back handle
        gl, gr = (-rl - 0.07, 0.0, gz + 0.01), (-rl - 0.01, 0.0, gz - 0.10)
    else:
        _spade_grips(v, -rl - 0.05, gz)
        gl, gr = (-rl - 0.05, 0.052, gz), (-rl - 0.05, -0.052, gz)
    v.socket("grip.gunner.l", gl, -X, part="gun", note="the gunner's left hand")
    v.socket("grip.gunner.r", gr, -X, part="gun", note="the gunner's right hand, on the trigger")
    v.slab((-rl + 0.05, -0.012, gz + 0.058), (-rl + 0.07, 0.012, gz + 0.12), "steel")        # the rear sight
    # The strip: in from the left, the fired end out at the right.
    fx = -0.10
    v.slab((fx - 0.04, 0.03, gz + 0.02), (fx + 0.04, 0.33, gz + 0.034), "brass")
    v.slab((fx - 0.04, -0.13, gz + 0.02), (fx + 0.04, -0.03, gz + 0.03), "steel")
    v.socket("muzzle", (ml, 0.0, bz), X, part="gun", note="the muzzle: the flash and the tracer")
    v.socket("feed", (fx, 0.33, gz + 0.027), Y, part="gun", note="the strip is pushed in here from the left: the number two's hands, a new strip every 30 rounds")
    v.socket("eject", (fx, -0.13, gz + 0.02), -Y, part="gun", note="the spent strip comes out at the right")
    v.use("mount")
    _ammo_box(v, (-0.10, 0.52, 0.0), (0.42, 0.15, 0.12))
    v.socket("box", (-0.10, 0.52, 0.12), Z, part="mount", note="the box of strips on the ground at her left")
    v.socket("seat", (-rl - 0.82, 0.18, 0.0), X, part="mount", note="the gunner's root: sitting low (today lying) behind her a little to the left, his hands 0.78 m ahead on the grips")
    v.socket("seat.loader", (-0.25, 0.75, 0.0), X, part="mount", note="the number two, kneeling at her left with the next strip")
    _controls(v, "sit", "left", left=30.0, right=30.0, low=-10.0, high=14.0,
              burst="slow heavy bursts of 5 to 8 at 450 a minute; a strip is 30 rounds (a Breda's 20), then a beat while the next goes in")
    return v


# ---------------------------------------------------------------------------------------------------------------------
# The mortars: the Brandt pattern every army made.
# ---------------------------------------------------------------------------------------------------------------------
MORTARS = {
    # tag: (name, title, tube's length, bore's radius, baseplate (shape, long, wide), paint, the bombs' paint)
    "GER": ("MortarGER", "8 cm Granatwerfer 34", 1.14, 0.047, ("rect", 0.54, 0.38), "484A46", "4F2B25"),
    "USA": ("MortarUSA", "81 mm Mortar M1", 1.26, 0.047, ("rect", 0.50, 0.36), "4B4A32", "4B4A32"),
    "UK": ("MortarUK", "Ordnance ML 3-inch mortar", 1.19, 0.045, ("rect", 0.52, 0.40), "4A4D3A", "6A5A32"),
    "SOV": ("MortarSOV", "82 mm battalion mortar of 1937", 1.22, 0.048, ("round", 0.50, 0.50), "4E5637", "4E5637"),
    "JAP": ("MortarJAP", "Type 97 81 mm infantry mortar", 1.26, 0.047, ("rect", 0.50, 0.36), "5A5238", "2E2C2A"),
    "ITA": ("MortarITA", "Mortaio da 81 Mod. 35", 1.15, 0.047, ("rect", 0.50, 0.36), "5B5F4A", "5B5F4A"),
    "FRA": ("MortarFRA", "Brandt Mle 27/31, 81 mm", 1.26, 0.047, ("rect", 0.50, 0.36), "4F5440", "4F5440"),
}


def _bomb(v, at, heading, paint, r=0.04):
    """A mortar bomb lying on the ground: a teardrop body and its tail with fins."""
    c, s = math.cos(heading), math.sin(heading)
    a = Vector((at[0], at[1], r))
    ax = Vector((c, s, 0.0))
    v.lathe(a, ax, [(0.20, 0.0), (0.17, r * 0.55), (0.11, r), (0.04, r * 0.8), (0.0, r * 0.35), (0.0, 0.0)], paint, v.n(8, 5))
    v.rod(a - ax * 0.11, a, r * 0.3, "steel", 5)
    v.lathe(a - ax * 0.12, ax, [(0.05, 0.0), (0.05, r * 0.9), (0.0, r * 0.9), (0.0, 0.0)], "steel", v.n(6, 4))


def mortar(fine=True, tag="GER"):
    """A medium mortar of the Brandt pattern laid at about 62 degrees: the smooth tube with its ball foot in the
    baseplate, a collar two thirds up it on the bipod's elevating screw, the traversing screw and the sight on the
    collar's left, two legs with a chain between them, three bombs laid ready at the right. The gunner kneels at the
    left with his eye to the sight; the number two kneels at the right and drops the bombs in at the muzzle."""
    name, title, L, bore, (shape, pl, pw), paint, bomb = MORTARS[tag]
    v = _weapon(name, title, fine, paint, bomb)
    E = math.radians(62.0)
    d = Vector((math.cos(E), 0.0, math.sin(E)))
    foot = Vector((0.0, 0.0, 0.075))
    tip = foot + d * L
    col = foot + d * (L * 0.62)
    v.part("mortar", None, (0, 0, 0), kind="root", note="on the ground under the tube's foot (the baseplate's socket); the game places it")
    # The baseplate: leaning a little to meet the tube, its back edge dug in.
    lean = math.radians(14.0)
    n = Vector((math.sin(lean), 0.0, math.cos(lean)))
    u = Vector((math.cos(lean), 0.0, -math.sin(lean)))
    c = Vector((-0.02, 0.0, 0.04))
    if shape == "rect":
        v.plate([c - u * pl / 2 - Y * pw / 2, c + u * pl / 2 - Y * pw / 2, c + u * pl / 2 + Y * pw / 2, c - u * pl / 2 + Y * pw / 2], 0.035, "mount", bevel=0.008)
        if fine:
            for s in (1, -1):                                 # two ribs and the carrying handle
                a = c + n * 0.004 + Y * (s * pw * 0.3)
                v.plate([a - u * pl * 0.42 - Y * 0.012, a + u * pl * 0.42 - Y * 0.012, a + u * pl * 0.42 + Y * 0.012, a - u * pl * 0.42 + Y * 0.012][::1], -0.014, "mount")
            h = c - u * (pl / 2 - 0.03)
            v.tube([h - Y * 0.07, h - Y * 0.06 + n * 0.035, h + Y * 0.06 + n * 0.035, h + Y * 0.07], 0.006, "steel", 4)
    else:
        v.lathe(c - n * 0.035, n, [(0.035, 0.0), (0.035, pl / 2 - 0.02), (0.0, pl / 2), (0.0, 0.0)], "mount", v.n(18, 9))
    v.ball(foot - d * 0.02, 0.05, "mount", 8, 5, dome=True)                                  # the socket's cup
    # The bipod: a yoke under the collar, the elevating screw up to it, two legs, the chain between them.
    yoke = col + Vector((0.10, 0.0, -0.16))
    feet = [Vector((yoke.x + 0.22, s * 0.36, 0.0)) for s in (1, -1)]
    for f in feet:
        v.rod(yoke, f + Z * 0.012, 0.016, "mount", 6)
        _foot(v, (f.x, f.y, 0.0), "shoe")
    v.rod(yoke - Z * 0.04, col - d * 0.0, 0.02, "mount", 8)                                   # the elevating screw's housing
    v.rod(yoke - Z * 0.02 - Y * 0.0, yoke - Z * 0.02 - Y * 0.10, 0.008, "steel", 5)           # its crank, out to the right
    v.rod(yoke - Z * 0.02 - Y * 0.10, yoke - Z * 0.07 - Y * 0.10, 0.008, "steel", 5)
    m0, m1 = yoke + (feet[0] - yoke) * 0.5, yoke + (feet[1] - yoke) * 0.5
    v.rod(m0, m1, 0.006, "steel", 4)                                                          # the chain
    # The collar and the traversing screw across it; the sight on its left end.
    side = Y
    v.rod(col - side * 0.11, col + side * 0.11, 0.014, "steel", 6)
    _handwheel(v, col - side * 0.125, -Y, 0.035)
    if fine:
        v.rod(col + side * 0.10, col + side * 0.10 + Z * 0.07, 0.008, "steel", 5)
        v.box((0.05, 0.04, 0.05), tuple(col + side * 0.115 + Z * 0.095), "steel", bevel=0.004)
    v.socket("sight", tuple(col + side * 0.115 + Z * 0.10 - X * 0.03), -X, part="mortar", note="the sight on the collar's left: the gunner's eye comes to it from behind")
    v.socket("grip.traverse", tuple(col - side * 0.14), -Y, part="mortar", note="the traversing handwheel at the collar's right end")
    v.socket("grip.elevate", tuple(yoke - Z * 0.07 - Y * 0.10), -Y, part="mortar", note="the elevating crank under the yoke, at the right")
    # The bombs laid ready and their case, at the right.
    for k in range(3):
        _bomb(v, (-0.10 + 0.02 * k, -0.62 - 0.11 * k, 0.0), math.radians(8.0 - 6.0 * k), "box")
    _ammo_box(v, (-0.45, -0.72, 0.0), (0.42, 0.26, 0.20), "mount")
    v.socket("rounds", (0.0, -0.72, 0.05), Z, part="mortar", note="the bombs laid ready: the number two's hand takes the next from here")
    v.socket("seat", (0.05, 0.45, 0.0), X, part="mortar", note="the gunner's root: kneeling at her left, his eye over the sight, his right hand to the collar")
    v.socket("seat.loader", (tip.x * 0.55, -0.50, 0.0), Vector((0.2, 1.0, 0.0)), part="mortar", note="the number two's root: kneeling at her right, facing the muzzle, a bomb in his hands")
    # The tube: its own part, so it can be laid, and carried on a shoulder.
    v.part("tube", "mortar", tuple(foot), axis=(0, -1, 0), limits=[-17.0, 23.0], kind="turn", note="the tube on its ball foot (rest: 62 degrees up): + steeper; and what is carried on a shoulder")
    r = bore + 0.008
    v.lathe(foot, d, [(L, bore), (L, r), (0.10, r), (0.06, r + 0.012), (0.0, r * 0.7), (0.0, 0.0), (0.02, 0.0), (0.02, bore * 0.6), (L - 0.02, bore), (L, bore)],
            "mount", v.n(12, 7))
    v.ball(foot - d * 0.03, 0.035, "steel", 8, 5)
    v.lathe(col - d * 0.035, d, [(0.07, r), (0.07, r + 0.012), (0.0, r + 0.012), (0.0, r), (0.07, r)], "steel", v.n(12, 7))   # the collar's band
    v.socket("muzzle", tuple(tip), tuple(d), part="tube", note="the muzzle: the bomb is dropped in here, and the flash and the smoke come out")
    v.m.info.setdefault("file", {})["controls"] = {
        "elevate": {"part": "tube", "what": "the tube's lay (rest 62 degrees up; + steeper). The bipod is drawn for the rest lay: a change of a few degrees reads true, more wants the bipod moved", "limits": [-17.0, 23.0]},
        "gunner": {"posture": "kneel", "what": "the gunner kneels at her left, his eye to the sight (socket sight); the number two kneels at her right (seat.loader) and drops each bomb in at the muzzle, then ducks away with his hands to his ears"},
        "fire": {"what": "a bomb every 3 to 4 seconds when she fires for effect; one to range first. A thump, a ring of dust at the baseplate, a puff of grey smoke at the muzzle (no long flash by day)"},
        "carry": {"what": "when the team moves: the game shows the `tube` part alone on the gunner's shoulder and puts the baseplate and bipod away until the number two and the bearers have clips that carry them"},
    }
    return v


BUILDERS = {
    "HmgMG42": lambda fine=True: lafette(fine, "mg42"),
    "HmgMG34": lambda fine=True: lafette(fine, "mg34"),
    "HmgM1919": lambda fine=True: browning(fine, "m1919"),
    "HmgM1917": lambda fine=True: browning(fine, "m1917"),
    "HmgVickers": vickers,
    "HmgMaxim": maxim,
    "HmgType92": lambda fine=True: strip_gun(fine, "type92"),
    "HmgHotchkiss": lambda fine=True: strip_gun(fine, "hotchkiss"),
    "HmgBreda37": lambda fine=True: strip_gun(fine, "breda37"),
}
for _tag, _row in MORTARS.items():
    BUILDERS[_row[0]] = (lambda t: (lambda fine=True: mortar(fine, t)))(_tag)

ABOUT = {
    "HmgMG42": {"role": "heavy machine gun", "nation": "GER", "units": "German machine gun teams from the summer of 1942", "from": "1942-06"},
    "HmgMG34": {"role": "heavy machine gun", "nation": "GER", "units": "German machine gun teams until the MG 42 comes (and Romania's and Hungary's German-armed teams)", "to": "1942-05"},
    "HmgM1919": {"role": "heavy machine gun", "nation": "USA", "units": "American machine gun teams"},
    "HmgM1917": {"role": "heavy machine gun", "nation": "POL", "units": "Polish teams (the wz. 30), and any army's water-cooled Browning"},
    "HmgVickers": {"role": "heavy machine gun", "nation": "UK", "units": "British and Commonwealth machine gun teams"},
    "HmgMaxim": {"role": "heavy machine gun", "nation": "SOV", "units": "Soviet machine gun teams (and Finland's, on the same mount)"},
    "HmgType92": {"role": "heavy machine gun", "nation": "JAP", "units": "Japanese machine gun teams"},
    "HmgHotchkiss": {"role": "heavy machine gun", "nation": "FRA", "units": "French machine gun teams (and China's)"},
    "HmgBreda37": {"role": "heavy machine gun", "nation": "ITA", "units": "Italian machine gun teams"},
}
for _tag, _row in MORTARS.items():
    ABOUT[_row[0]] = {"role": "medium mortar", "nation": _tag, "units": _row[1]}
