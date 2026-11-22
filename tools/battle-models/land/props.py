"""The ground battle's small props (Brian's picks of 22 Nov 17:33Z: field telephones, stretcher bearers): the
signaller's field telephone in its leather case, the lineman's cable reel on its carrying frame, and the canvas
stretcher on two wooden poles. Made on the vehicles' kit (Works), so they come out in the same file shape as a tank's
parts: `parts`, `sockets` (where hands hold them), `controls` (what the game turns), near and far meshes.

    python export_props.py <out folder> [name,name,...|all] [json glb fbx]

The art director's brief (art-director/battles/ground-r2-phones-bearers-patrol-brief.md): no red cross, no red
crescent and no emblem on anything; the reel's drum about 0.35 to 0.4 m across; the stretcher's poles about 2.3 m,
olive canvas. Seen at 10 to 60 m, so plain shapes, a few hundred to two thousand triangles near.

Every prop faces +x (its front, or for the stretcher the head end), stands on the ground at z = 0, and has one root
part the game places.
"""
import math

from motorpool import X, Y, Z
from works import Works

# Paints (the props' own; nothing here takes a nation's coat).
LEATHER = "5C4430"        # oiled brown leather, a shade lighter than the tank crews' (it must read on a dark hole's floor)
WEBBING = "6E6A4C"        # olive webbing strap
BAKELITE = "2C2B2A"       # the handset (dark, but above the art director's floor for small dark parts)
CANVAS = "6B6847"         # olive canvas, the stretcher's bed and the reel's straps
WOOD = "8C6B47"           # the poles, oiled ash
WIRE = "34302A"           # field wire wound on the drum: dark, L about 25 to 30 as the brief's wire
STEEL = "4A4C4A"          # painted steel frames and fittings
ZINC = "7E8078"           # the drum's flanges, galvanised


def _prop(name, title, fine):
    v = Works(name, title, "prop", "_", fine)
    for k, hx in (("leather", LEATHER), ("webbing", WEBBING), ("bakelite", BAKELITE), ("canvas", CANVAS), ("wood", WOOD),
                  ("wire", WIRE), ("steel", STEEL), ("zinc", ZINC)):
        v.m.paints[k] = hx
        v.m.paint_at[k] = list(v.m.paints).index(k)
    return v


def field_phone(fine=True):
    """A field telephone in its leather case, as a signaller sets it down in a hole or behind a bank: a box about
    0.24 m long, 0.11 m deep and 0.25 m high with a domed lid and a buckled flap, a webbing strap, the magneto's crank
    on its right side and the handset lying in the clip on its top. The handset is its own part, so the game can lift
    it to the signaller's ear; the crank turns."""
    v = _prop("FieldPhone", "field telephone", fine)
    L, D, H = 0.24, 0.11, 0.25
    v.part("phone", None, (0, 0, 0), kind="root", note="on the ground under its middle; the game places it")
    # The case: a box with its edges rounded, the lid a little proud of it, the flap down its front.
    v.box((L, D, H - 0.03), (0.0, 0.0, (H - 0.03) / 2), "leather", bevel=0.012, segs=2)
    v.box((L + 0.006, D + 0.006, 0.035), (0.0, 0.0, H - 0.0175), "leather", bevel=0.01, segs=2)
    v.slab((-0.05, -D / 2 - 0.006, H - 0.12), (0.05, -D / 2, H), "leather", bevel=0.004)
    v.box((0.03, 0.008, 0.02), (0.0, -D / 2 - 0.008, H - 0.11), "steel", bevel=0.002)             # the flap's buckle
    # The strap: up from each end over the top, standing off it a little.
    v.tube([(-L / 2 - 0.004, 0.0, 0.06), (-L / 2 - 0.006, 0.0, H + 0.04), (-0.06, 0.0, H + 0.11), (0.06, 0.0, H + 0.11),
            (L / 2 + 0.006, 0.0, H + 0.04), (L / 2 + 0.004, 0.0, 0.06)], 0.008, "webbing", 4)
    # The magneto's crank on the right side (y up to the left in Blender: the game's right is -y here, mirrored on export).
    ax = (0.03, -D / 2, 0.11)
    v.part("crank", "phone", ax, axis=(0, -1, 0), kind="turn", note="the magneto's crank: + turns it as the signaller rings")
    v.rod(ax, (ax[0], ax[1] - 0.02, ax[2]), 0.008, "steel", 8)
    v.rod((ax[0], ax[1] - 0.02, ax[2]), (ax[0] + 0.05, ax[1] - 0.02, ax[2] + 0.02), 0.006, "steel", 6)
    v.rod((ax[0] + 0.05, ax[1] - 0.02, ax[2] + 0.02), (ax[0] + 0.05, ax[1] - 0.05, ax[2] + 0.02), 0.009, "wood", 8)
    v.socket("grip.crank", (ax[0] + 0.05, ax[1] - 0.04, ax[2] + 0.02), -Y, part="crank", note="the signaller's right hand on the crank's knob")
    # The handset: a bar with an ear cup and a mouth cup, lying along the lid in its clip.
    v.use("phone")
    hz = H + 0.03
    v.part("handset", "phone", (0.0, 0.0, hz), kind="free", note="the handset: lifted off the case by the game to the signaller's ear")
    v.rod((-0.09, 0.0, hz), (0.09, 0.0, hz), 0.012, "bakelite", 8)
    v.lathe((-0.10, 0.0, hz - 0.012), Z, [(0.022, 0.0), (0.022, 0.024), (0.0, 0.026), (0.0, 0.0)], "bakelite", v.n(12, 8))   # the ear cup
    v.lathe((0.10, 0.0, hz - 0.012), Z, [(0.02, 0.0), (0.02, 0.02), (0.0, 0.022), (0.0, 0.0)], "bakelite", v.n(12, 8))       # the mouth cup
    v.socket("grip.handset", (0.0, 0.0, hz), Z, part="handset", note="the hand round the handset's bar")
    v.socket("ear", (-0.10, 0.0, hz + 0.01), Z, part="handset", note="the ear cup: to the signaller's ear")
    v.use("phone")
    v.socket("grip.strap", (0.0, 0.0, H + 0.11), Z, part="phone", note="where a hand or a shoulder takes the strap")
    v.socket("wire", (-L / 2, 0.02, 0.08), -X, part="phone", note="where the field wire comes in (its two posts on the back end)")
    v.rod((-L / 2, 0.015, 0.08), (-L / 2 - 0.02, 0.015, 0.08), 0.006, "steel", 6)
    v.rod((-L / 2, 0.035, 0.08), (-L / 2 - 0.02, 0.035, 0.08), 0.006, "steel", 6)
    v.m.info.setdefault("file", {})["controls"] = {
        "crank": {"part": "crank", "what": "turned by the game while the signaller rings (two or three turns, quick)"},
        "handset": {"part": "handset", "what": "the game lifts it to the signaller's ear (socket ear), and lays it back"},
    }
    return v


def cable_reel(fine=True):
    """The lineman's cable reel on its carrying frame: a drum 0.38 m across and 0.2 m wide wound with dark field wire,
    turning on an axle in a tubular steel frame that has two handles on top and webbing straps behind, so he carries
    it on his chest or back, or by the handles. The drum turns as wire pays out; a crank on its left flange winds in."""
    v = _prop("CableReel", "cable reel on its carrying frame", fine)
    R, W, zc = 0.19, 0.20, 0.30
    F, T = R + 0.04, R + 0.07              # the frame's half length (clear of the drum) and its top above the axle
    v.part("reel", None, (0, 0, 0), kind="root", note="on the ground under the drum's axle, the frame's feet down")
    hw = W / 2 + 0.035
    # The frame: two side hoops (feet to the handles), joined by bars.
    for s in (1, -1):
        y = s * hw
        v.tube([(-F, y, 0.0), (-F, y, zc + 0.12), (-0.12, y, zc + T), (0.12, y, zc + T), (F, y, zc + 0.12), (F, y, 0.0)], 0.012, "steel", 6)
        v.rod((-F, y, 0.0), (F, y, 0.0), 0.01, "steel", 6)
        v.rod((0.0, y, zc), (0.0, y, zc + T), 0.01, "steel", 6)              # the strut up from the axle's bearing
        v.lathe((0.0, y, zc), Y if s > 0 else -Y, [(0.02, 0.0), (0.02, 0.03), (0.0, 0.03), (0.0, 0.0)], "steel", v.n(10, 6))   # the bearing
    v.rod((-0.08, -hw, zc + T), (-0.08, hw, zc + T), 0.014, "steel", 8)       # the two handles
    v.rod((0.08, -hw, zc + T), (0.08, hw, zc + T), 0.014, "steel", 8)
    v.rod((-F, -hw, 0.02), (-F, hw, 0.02), 0.01, "steel", 6)
    v.rod((F, -hw, 0.02), (F, hw, 0.02), 0.01, "steel", 6)
    # The straps behind (-x), olive webbing from the top bar to the feet.
    for s in (1, -1):
        v.slab((-F - 0.03, s * 0.06 - 0.02, 0.04), (-F - 0.015, s * 0.06 + 0.02, zc + 0.14), "canvas", bevel=0.003)
    v.socket("grip.left", (0.0, hw - 0.02, zc + T), Z, part="reel", note="his left hand on a handle")
    v.socket("grip.right", (0.0, -hw + 0.02, zc + T), Z, part="reel", note="his right hand on a handle")
    v.socket("back", (-F - 0.03, 0.0, zc + 0.05), -X, part="reel", note="what rests on his back or chest when he wears it")
    # The drum: zinc flanges, the wound wire between them, the axle through.
    v.part("drum", "reel", (0.0, 0.0, zc), axis=(0, 1, 0), kind="turn", note="turns as wire pays out (+: the wire goes out forward, off the bottom)")
    for s in (1, -1):
        y = s * W / 2
        v.lathe((0.0, y - s * 0.004, zc), Y if s > 0 else -Y, [(0.008, 0.0), (0.008, R), (0.0, R), (0.0, 0.0)], "zinc", v.n(28, 12))
    v.lathe((0.0, -W / 2 + 0.004, zc), Y, [(0.0, R - 0.035), (W - 0.008, R - 0.035), (W - 0.008, R - 0.012), (0.0, R - 0.012), (0.0, R - 0.035)], "wire", v.n(28, 12))
    v.rod((0.0, -hw - 0.02, zc), (0.0, hw + 0.02, zc), 0.012, "steel", 8)
    # The crank on the left flange.
    v.rod((0.0, hw + 0.02, zc), (0.0, hw + 0.02, zc - 0.11), 0.008, "steel", 6)
    v.rod((0.0, hw + 0.02, zc - 0.11), (0.0, hw + 0.07, zc - 0.11), 0.011, "wood", 8)
    v.socket("grip.crank", (0.0, hw + 0.06, zc - 0.11), Y, part="drum", note="his hand on the crank when he winds in")
    v.socket("wire", (0.10, 0.0, zc - R + 0.03), X, part="reel", note="where the wire leaves the drum: the game draws the line on from here")
    v.m.info.setdefault("file", {})["controls"] = {
        "drum": {"part": "drum", "what": "turned by the game by the wire paid out: degrees = metres / (2 pi 0.17 m) x 360, + paying out", "radius": 0.17},
    }
    return v


def stretcher(fine=True):
    """A canvas stretcher: two wooden poles 2.3 m long, an olive canvas bed 1.85 m by 0.56 m laced between them
    with a little sag, steel spreader bars across each end and four small stirrup feet. No cross or emblem anywhere.
    Its head end is +x; it lies on the ground on its feet, and two bearers carry it by the poles' ends."""
    v = _prop("Stretcher", "stretcher", fine)
    Lp, Lb, half, zp, rp = 2.30, 1.85, 0.29, 0.14, 0.022
    v.part("stretcher", None, (0, 0, 0), kind="root", note="on the ground under its middle, on its feet; the game carries it by the grips")
    for s in (1, -1):
        y = s * half
        v.rod((-Lb / 2 - 0.12, y, zp), (Lb / 2 + 0.12, y, zp), rp, "wood", 10)
        for e in (1, -1):                                            # the handles: the poles' ends turned a little thinner
            v.rod((e * (Lb / 2 + 0.12), y, zp), (e * Lp / 2, y, zp), rp - 0.005, "wood", 10)
        for x in (-Lb / 2 + 0.06, Lb / 2 - 0.06):                    # the stirrup feet, bent steel strap under each pole
            v.tube([(x - 0.05, y, zp - 0.01), (x - 0.035, y, 0.012), (x + 0.035, y, 0.012), (x + 0.05, y, zp - 0.01)], 0.008, "steel", 5)
    for x in (-Lb / 2 - 0.03, Lb / 2 + 0.03):                        # the spreader bars
        v.rod((x, -half, zp), (x, half, zp), 0.009, "steel", 6)
    # The bed: an olive canvas sheet between the poles, sagging 4 cm in the middle, its edges wrapped round the poles.
    n = 10 if fine else 4
    rows = []
    for i in range(n + 1):
        y = -half + 2 * half * i / n
        sag = 0.04 * (1.0 - (y / half) ** 2)
        rows.append((y, zp - 0.004 - sag))
    verts, faces = [], []
    for x in (-Lb / 2, Lb / 2):
        for y, z in rows:
            verts.append((x, y, z))
    for i in range(n):                                               # (the upper face, looking up)
        a, b = i, i + 1
        c, d = (n + 1) + i + 1, (n + 1) + i
        faces.append((d, c, b, a))
    # (a thin sheet with both faces, so it reads from below as the bearers lift it)
    top = len(verts)
    for x in (-Lb / 2, Lb / 2):
        for y, z in rows:
            verts.append((x, y, z - 0.006))
    for i in range(n):                                               # (the under face, looking down)
        a, b = top + i, top + i + 1
        c, d = top + (n + 1) + i + 1, top + (n + 1) + i
        faces.append((a, b, c, d))
    v.m.add(verts, faces, "canvas", True, None, "stretcher")
    for s in (1, -1):
        v.rod((-Lb / 2, s * (half - 0.004), zp), (Lb / 2, s * (half - 0.004), zp), rp + 0.004, "canvas", 10)
    # The grips: where each bearer's hands hold the poles, on the handles 5 cm in from the ends.
    for end, x in (("front", Lp / 2 - 0.05), ("back", -Lp / 2 + 0.05)):
        for side, s in (("left", 1), ("right", -1)):
            v.socket(f"grip.{end}.{side}", (x, s * half, zp), Z, part="stretcher", note=f"a {end} bearer's {side} hand")
    v.socket("lie", (0.0, 0.0, zp - 0.03), Z, part="stretcher", note="where the carried man lies, on his back, head to +x (the head end)")
    v.m.info.setdefault("file", {})["controls"] = {
        "carry": {"part": "stretcher", "what": "carried at the bearers' hip height by the grips (about 0.8 m), its head end first; low and quick under fire"},
    }
    return v


BUILDERS = {"FieldPhone": field_phone, "CableReel": cable_reel, "Stretcher": stretcher}
ABOUT = {
    "FieldPhone": {"role": "the signaller's field telephone", "units": "every army's signallers (one shape; leather case, no marks)"},
    "CableReel": {"role": "the lineman's cable reel", "units": "every army's linemen (one shape, no marks)"},
    "Stretcher": {"role": "the stretcher bearers' stretcher", "units": "every army's bearers (one shape; no cross or emblem, by Brian's markings rule)"},
}
