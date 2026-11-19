"""The what-if aircraft of the sandbox: our own designs that never flew (Brian, 2026-11-19: "i want this to be a
sandbox to play and make your own history"; the roster is the tech-tree thread's, sandbox/ARMS-ROSTER.md).

Each is an original machine in the game's numbers (see game/plane_numbers.py: stations of the fuselage as (x, z of
its middle, half its width, half its height), sections of a flying surface as (how far out, leading edge, trailing
edge, height, thickness); x ahead, y to the right, z up, in metres), built to the B-17 benchmark as the others are:
framed glazing over an open cockpit with her crew inside at their controls (fighters.py's `canopy`), gunners at
their guns, rivets and panel lines (fitout.py). No real prototype's lines or name, and none of ruling 230's late-war
prototypes. Any army that has researched one flies her in its own paint (`nation_paint`) with its own mark (`*`).

    WhatIf_EscortFighter   long-range escort fighter: twin booms, an engine pulling at the front of her pod and one
                           pushing at its back; a bubble hood; four heavy guns and a cannon in her nose
    WhatIf_TankBuster      twin-engined, two seats, an armoured cockpit; a 75 mm gun under her, its muzzle well ahead of her nose
    WhatIf_GunshipBomber   a four-engined heavy that carries guns, not bombs: eight gun positions, twelve men
    WhatIf_NightFighter    twin-engined, three seats, radar antlers on her nose, cannon under her and firing up behind her cockpit; matt black
    WhatIf_SeaHunter       four-engined flying boat: a deep hull with two steps, floats that fold up to be her wingtips,
                           a radar dome under her bow, torpedoes under her wing
    WhatIf_AssaultGlider   heavy glider: a box of a fuselage whose whole nose swings open to the side for the glider tank

What the plane kit gained for them (planes.py reads these keys of a machine's numbers):

    pushers   [(x, z), ...]: an airscrew on her middle line facing aft, pushing (its blades turned the other way)
    glider    True: no engine and no airscrew anywhere
    assign    f(kind, c, mean, ob) -> a part or None: puts a piece of her in a part of her own choosing (her booms in
              her tail, a door's skin in its door)
    doors     [{"name", "parent", "pivot" (her numbers' axes), "note", "kw" (the rest of the part: axis, limits, kind,
              slide, slide_limits, in the file's axes, y to port)}]: parts of her own that swing or slide (a door, a
              gun's barrel), cut clean from her skin (not torn); `assign` puts her pieces in them
    hooks     {"fit": [f(m, f, s, G, hull)], "clips": [...]}: called after her fit-out and after her clips
"""
import math

from mathutils import Vector

import fighters
import fitout

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))

# The game's sculptor, patched (after planes.JETS): a pusher's airscrew, and a glider's nose with no spinner.
SOURCE = [
    ('    boss_back = 1.0e9 if (s.get("engines") and not s.get("nose_engine")) or s.get("jets") else sorted(',
     '    boss_back = 1.0e9 if (s.get("engines") and not s.get("nose_engine")) or s.get("jets") or s.get("glider") else sorted('),
    ('    if (not s.get("engines") or s.get("nose_engine")) and not s.get("jets"):\n        airscrew(parts, (hub_x, 0.0, hub_z)',
     '    for push_x, push_z in s.get("pushers", []):\n'
     '        for k in range(s.get("blades", 3)):\n'
     '            blade(parts, (push_x, 0.0, push_z), s.get("screw", 1.5), math.radians(40.0) + 2.0 * math.pi * k / s.get("blades", 3), -1.0)\n'
     '    if (not s.get("engines") or s.get("nose_engine")) and not s.get("jets") and not s.get("glider"):\n        airscrew(parts, (hub_x, 0.0, hub_z)'),
]

# A plain coat for a machine any army may fly: the game paints her in her owner's colours (`nation_paint`).
_PLAIN = {"upper": "4B5139", "upper2": "5E6146", "under": "8C9196", "nose": "4B5139", "boss": "2A2B2D"}

PLANES, CREW, ROLE = {}, {}, {}
TAIL_CUT = {}                                                # (where a machine's tail comes away: planes.TAIL_CUT)


def port(v):
    return Vector((v[0], -v[1], v[2]))


def add_socket(m, name, at, facing, part, note):
    """A socket in her numbers' axes (y to the right): the file's are y to port."""
    m.socket(name, port(at), port(facing), part=part, note=note)


# The research each comes from (the tech tree's keys, data/campaign/classes.json `tech`; the tech-tree thread,
# 2026-11-19). The glider is no class of her own: she comes with the glider tank.
TECH = {"escortftr": "escortfighters", "tankbuster": "tankbusters", "gunship": "gunships", "nightftr": "nightfighters",
        "seahunter": "seahunters", "glidertank": "assaultgliders"}


def set_whatif(m, kind, like, note):
    """What the file says of a what-if machine (as the ships' and the tanks' what-ifs say it)."""
    m.info["file"].update({"stands_for": None, "whatif": {"kind": kind, "like": like, "from": TECH.get(kind),
                                                          "note": "an original design that never flew: any air force that has the research flies her, in its own paint. " + note}})


# ---- glazing -------------------------------------------------------------------------------------------------
def bubble_canopy(screen, rows):
    """A bubble hood on rails over an open cockpit, behind a windscreen with one flat front pane between two posts
    and a quarter pane each side (the Mustang's way: fighters.py). screen: (x of its foot, half its front pane's
    width there, and at its head); rows: (x, the height of the sill where the glass begins, half its width there, its top)."""
    foot_x, foot_half, head_half = screen

    def make(hull):
        x0, sill, hw, top = rows[0]
        head = fighters.headed(fighters.bubble(hull, x0, sill, hw, top), head_half)
        post = len(head) - 2
        foot = [fighters.deck(hull, foot_x, foot_half)] * (len(head) - 1) + [fighters.deck(hull, foot_x, 0.0)]
        front = {"sections": [(foot_x, foot), (x0, head)], "skirt": fighters.SKIRT, "hoops": [-1], "rails": [post, fighters.SKIRT], "bar": 0.022}
        hood = [(x, fighters.bubble(hull, x, sl, w, t)) for x, sl, w, t in rows[1:]]
        first = fighters.resample(head, len(hood[0][1]) - 1)
        hood = [(x0, first)] + fighters.tween(x0, first, hood[0][0], hood[0][1], 3) + hood
        slide = {"sections": hood, "skirt": fighters.SKIRT, "hoops": [0], "rails": [fighters.SKIRT], "bar": 0.018, "cap": True, "bare": (len(hood) - 1,)}
        return [front, slide]
    return make


def bubble_hoods(screen, rows, deck_z):
    """The `hoods` entry the rest of the kit reads, for a bubble canopy."""
    return fighters.hood_of([(screen[0], deck_z, screen[1], 0.01)] + [(x, sl, w, t - sl) for x, sl, w, t in rows])


# ---- 1. the long-range escort fighter (class `escortftr`) ------------------------------------------------------
# A single seat in a short pod between two slim booms: an engine in the pod's nose pulling, a second at its back
# pushing, so that she has two engines' power and range in one small fuselage; her tail is carried on the booms
# and joined by a high tailplane, clear of the pushing airscrew's wash. A bubble hood amidships over the pilot; four
# heavy machine guns round her spinner and a cannon firing through it; a drop tank under each wing inboard of the
# booms. Span 15 m, 11.5 m long over her booms.
_ESC_SCREEN = (2.10, 0.20, 0.11)
_ESC_BUBBLE = [(1.72, 0.58, 0.33, 0.98), (1.50, 0.585, 0.35, 1.03), (1.22, 0.59, 0.36, 1.07), (0.92, 0.595, 0.36, 1.085), (0.62, 0.60, 0.355, 1.075),
               (0.32, 0.61, 0.34, 1.04), (0.06, 0.62, 0.31, 0.98), (-0.18, 0.64, 0.25, 0.90), (-0.36, 0.66, 0.17, 0.82), (-0.48, 0.68, 0.09, 0.77),
               (-0.54, 0.69, 0.03, 0.745)]
_ESC_BOOM = 2.30


def _esc_assign(kind, c, mean, ob):
    if kind == "extra" and abs(mean.y) > 1.6 and mean.x < 0.5:
        return "tail"                                      # (her booms go with her tail: they break away together behind the wing)
    if kind == "extra" and abs(mean.y) < 0.3 and mean.x < -3.0:
        return "plane"                                     # (the pushing airscrew's spinner is her pod's)
    return None


def _esc_fit(m, f, s, G, hull):
    k = 0
    for y, z, x in ((0.13, 0.30, 4.12), (-0.13, 0.30, 4.12), (0.28, 0.20, 4.05), (-0.28, 0.20, 4.05)):
        k += 1
        add_socket(m, f"gun.{k}", (x, y, z), X, "plane", "a heavy machine gun's muzzle in her nose")
    add_socket(m, "gun.5", (4.78, 0.0, 0.0), X, "plane", "the cannon firing through her spinner")
    add_socket(m, "exhaust.rear", (-2.5, 0.0, 0.55), Vector((-1, 0, 0.2)), "plane", "the pushing engine's exhaust, on her back ahead of its airscrew")
    set_whatif(m, "escortftr", ["Mustang", "Fighter"], "Two engines in one pod, one pulling and one pushing: the range to go all the way with the bombers.")


PLANES["WhatIf_EscortFighter"] = {
    "title": "Long-range escort fighter, a what-if (our own design)", "nation_paint": True,
    # (her pod: the spinner, the long cowling of the engine in her nose, the cockpit amidships, and the engine behind
    #  it tapering to the pushing airscrew's spinner)
    "hull": [(4.60, 0.0, 0.03, 0.03), (4.45, 0.0, 0.15, 0.15), (4.22, 0.0, 0.26, 0.26), (4.02, 0.0, 0.32, 0.32), (3.96, -0.03, 0.36, 0.40),
             (3.60, -0.06, 0.42, 0.52), (3.00, -0.08, 0.48, 0.62), (2.20, -0.06, 0.52, 0.68), (1.40, -0.02, 0.54, 0.72), (0.60, 0.0, 0.55, 0.73),
             (-0.40, 0.0, 0.55, 0.72), (-1.30, 0.0, 0.52, 0.66), (-2.10, 0.0, 0.44, 0.54), (-2.70, 0.0, 0.34, 0.40), (-3.05, 0.0, 0.26, 0.28), (-3.10, 0.0, 0.22, 0.22)],
    "square": 2.2, "screw": 1.62, "blades": 3, "nose_back": 2.6, "spinner": 4, "pushers": [(-3.22, 0.0)],
    "hoods": bubble_hoods(_ESC_SCREEN, _ESC_BUBBLE, 0.60),
    "canopy": bubble_canopy(_ESC_SCREEN, _ESC_BUBBLE),
    "opening": (1.72, -0.12, 52.0),
    "glare": (3.7, 2.2, 0.16), "stubs": (3.55, 2.75, 15.0),
    # (a straight-tapered wing through her pod, a little dihedral; the booms at 2.3 m out)
    "wing": [(0.0, 1.35, -1.55, -0.25, 0.46), (_ESC_BOOM, 1.22, -1.48, -0.20, 0.42), (5.5, 0.85, -0.95, -0.02, 0.26), (7.2, 0.62, -0.62, 0.08, 0.14),
             (7.5, 0.35, -0.38, 0.10, 0.03)],
    "aileron": (0.62, 0.95, 0.74),
    # (the high tailplane between the tops of the two fins, standing out a little beyond each)
    "tail": [(0.0, -6.42, -7.36, 1.30, 0.13), (_ESC_BOOM, -6.52, -7.36, 1.30, 0.11), (2.62, -6.66, -7.28, 1.30, 0.04)],
    "fins": [([(0.0, -5.70, -7.48, -0.10, 0.12), (0.0, -6.02, -7.52, 0.45, 0.11), (0.0, -6.32, -7.48, 1.05, 0.085), (0.0, -6.58, -7.36, 1.48, 0.05),
               (0.0, -6.84, -7.14, 1.62, 0.01)], _ESC_BOOM)],
    "patch": 2.2, "colours": dict(_PLAIN), "pitot": True,
    "scoops": [
        # (each boom's radiator under its fore end, its mouth under the wing's leading edge)
        ([(1.90, -0.47, 0.14, 0.08), (1.80, -0.475, 0.17, 0.11), (1.00, -0.46, 0.18, 0.12), (0.20, -0.40, 0.12, 0.06), (-0.20, -0.37, 0.04, 0.02)], _ESC_BOOM, 2.4)],
    "extras": [
        # (the booms: slim, from ahead of the wing back to the tail, each with its fin)
        ("body", ([(2.25, -0.20, 0.04, 0.04), (2.05, -0.20, 0.17, 0.19), (1.60, -0.20, 0.24, 0.27), (0.50, -0.18, 0.25, 0.30), (-1.50, -0.12, 0.24, 0.28),
                   (-3.50, 0.0, 0.18, 0.22), (-5.50, 0.08, 0.14, 0.17), (-6.90, 0.12, 0.12, 0.15), (-7.40, 0.13, 0.06, 0.08), (-7.60, 0.13, 0.02, 0.02)], _ESC_BOOM, 2.0, "coat")),
        # (the pushing airscrew's spinner, behind her pod)
        ("body", ([(-3.08, 0.0, 0.22, 0.22), (-3.30, 0.0, 0.20, 0.20), (-3.50, 0.0, 0.12, 0.12), (-3.62, 0.0, 0.03, 0.03)], 0.0, 2.0, "boss")),
        # (a drop tank under each wing, between pod and boom)
        ("body", ([(1.45, -0.72, 0.03, 0.03), (1.20, -0.72, 0.17, 0.17), (0.60, -0.72, 0.22, 0.22), (-0.60, -0.72, 0.20, 0.20), (-1.20, -0.72, 0.08, 0.08),
                   (-1.35, -0.72, 0.02, 0.02)], 1.35, 2.0, "under")),
        ("rod", ((0.30, 1.35, -0.50), (0.30, 1.35, -0.33), 0.03, 0.03, "under")), ("rod", ((-0.40, 1.35, -0.50), (-0.40, 1.35, -0.36), 0.03, 0.03, "under")),
        # (four heavy machine guns in the cowling round her spinner, and the cannon through it)
        ("rod", ((3.80, 0.13, 0.30), (4.12, 0.13, 0.30), 0.022, 0.016, "frame")), ("rod", ((3.75, 0.28, 0.20), (4.05, 0.28, 0.20), 0.022, 0.016, "frame")),
        ("rod", ((4.50, 0.0, 0.0), (4.78, 0.0, 0.0), 0.032, 0.026, "frame")),
        # (the pushing engine's air scoop on her back behind the hood, and its exhausts each side ahead of the airscrew)
        ("body", ([(-1.00, 0.70, 0.04, 0.03), (-1.20, 0.72, 0.16, 0.10), (-1.80, 0.66, 0.17, 0.10), (-2.30, 0.52, 0.10, 0.05), (-2.50, 0.46, 0.03, 0.02)], 0.0, 2.2, "coat")),
        ("rod", ((-2.30, 0.38, 0.22), (-2.62, 0.30, 0.20), 0.045, 0.04, "soot"))],
    "tailwheel": None,
    "side_marks": [(-1.55, 0.18, "*")], "wing_marks": [(5.9, 0.0, 0.5, "*", "*", True, True)], "code": (-0.95, 0.30),
    "assign": _esc_assign, "hooks": {"fit": [_esc_fit]},
}
CREW["WhatIf_EscortFighter"], ROLE["WhatIf_EscortFighter"] = 1, "fighter"


def green_section(hull, x, sill, ys, shoulder, top, post=None, lean=0.886):
    """Half a section of a greenhouse (the Val's way: planes.py): the metal rail it stands on (its foot on her skin);
    a flat side pane leaning in from the sill to the frame at its shoulder; a roof, nearly flat, over a frame 0.19 out
    to her middle line. `post` adds a point where a windscreen's post meets it."""
    yb = ys + 0.004
    zb = min(sill - 0.05, fitout.body_top(hull, x, yb) - 0.006)
    if shoulder is None:
        shoulder = sill + 0.78 * (top - sill)
    ysh, yr = lean * ys, min(0.19, 0.55 * ys)
    zr = shoulder + 0.8 * (top - shoulder)
    pts = [(max(yb - 0.03, 0.0), zb - 0.06), (yb, zb), (ys, sill), (ysh, shoulder), (yr, zr)]
    if post is not None:
        pts.append((post, zr + (top - zr) * (yr - post) / yr))
    return pts + [(0.0, top)]


def green_canopy(screen, pieces, screen_bar=0.02):
    """A framed greenhouse over an open cockpit: a windscreen with a flat front pane between two posts and a side pane
    each side, then its pieces. screen: (x of its foot, half its front pane there, x of its head, the top there, half
    the front pane at its head); each piece: {"rows": [(x, sill, out, shoulder, top), ...], "hoops", "rails", "bars",
    "bar", "cap"} (rails 2 is the sill, 3 the shoulder, 4 the roof's frame)."""
    foot_x, foot_half, head_x, head_top, post_y = screen

    def make(hull):
        skirt = fighters.SKIRT
        x0, sill, ys, shoulder, top = pieces[0]["rows"][0]
        head = fighters.headed(green_section(hull, head_x, sill + 0.02, ys - 0.012, None if shoulder is None else shoulder - 0.012, head_top), post_y)
        post = len(head) - 2
        foot = [fighters.deck(hull, foot_x, foot_half)] * (len(head) - 1) + [fighters.deck(hull, foot_x, 0.0)]
        out = [{"sections": [(foot_x, foot), (head_x, head), (x0, green_section(hull, x0, sill, ys, shoulder, top, post_y))], "skirt": skirt,
                "hoops": [1, 2], "rails": [post, skirt], "bar": screen_bar}]
        for pc in pieces:
            secs = [(x, green_section(hull, x, sl, w, sh, t)) for x, sl, w, sh, t in pc["rows"]]
            out.append({"sections": secs, "skirt": skirt, "hoops": pc.get("hoops", [0, len(secs) - 1]), "rails": pc.get("rails", [skirt, 3, 4]),
                        "bars": pc.get("bars", []), "bar": pc.get("bar", 0.018), "cap": pc.get("cap", False)})
        return out
    return make


def green_hoods(screen, pieces, deck_z):
    """The `hoods` entry the rest of the kit reads (where her crew sit): her greenhouse's own lines."""
    rows = [r for pc in pieces for r in pc["rows"]]
    return fighters.hood_of([(screen[0], deck_z, screen[1], 0.01), (screen[2], rows[0][1], rows[0][2], screen[3] - rows[0][1])]
                            + [(x, sl, w, t - sl) for x, sl, w, sh, t in rows[1:]])


# ---- 2. the tank-buster (class `tankbuster`) ---------------------------------------------------------------------
# Twin-engined, two men back to back in an armoured box of a cockpit (the plates of her fore fuselage are her
# armour: a bath round them both), under a small greenhouse with a thick armoured windscreen. Slung under her
# middle in a long fairing is a 75 mm anti-tank gun whose muzzle stands two metres ahead of her nose: one heavy shot
# at a time, from low and slow, through a tank's thin roof. Two machine guns in her wings, a gun for the man facing
# aft. Span 17 m, 12.5 m long.
_TB_SCREEN = (3.70, 0.17, 3.30, 1.14, 0.15)
_TB_PIECES = [
    {"rows": [(3.28, 0.75, 0.40, 1.04, 1.17), (2.92, 0.76, 0.405, 1.05, 1.18), (2.55, 0.77, 0.405, 1.05, 1.17)], "hoops": [0, 1, 2], "bar": 0.026},
    {"rows": [(2.57, 0.77, 0.40, 1.01, 1.12), (2.15, 0.78, 0.40, 1.00, 1.10), (1.78, 0.79, 0.39, 0.98, 1.07)], "hoops": [0, 1, 2], "bar": 0.024},
    {"rows": [(1.80, 0.79, 0.385, None, 1.05), (1.58, 0.80, 0.36, None, 0.99), (1.40, 0.81, 0.32, None, 0.93)], "hoops": [0], "rails": [2, 3], "bar": 0.02, "cap": True}]
_TB_CANNON = (7.55, 0.0, -0.86)                             # (the 75 mm's muzzle)


def _tb_assign(kind, c, mean, ob):
    if kind == "extra" and abs(mean.y) < 0.05 and mean.x > 4.2 and mean.z < -0.6:
        return "cannon"                                    # (the 75 mm's barrel and its muzzle brake: they run back when she fires)
    return None


def _tb_fit(m, f, s, G, hull):
    m.use("plane")
    if m.fine:
        # (her armoured glass: a thick block behind the front pane of her windscreen, green at its edges)
        m.box((3.46, 0.0, 1.0), (0.07, 0.30, 0.20), "glass", pitch=-40.0)
    add_socket(m, "gun.3", _TB_CANNON, X, "cannon", "the 75 mm's muzzle: one heavy shot (clip fire.cannon)")
    set_whatif(m, "tankbuster", ["Sturmovik", "Bomber"], "A 75 mm anti-tank gun under her: she kills tanks from the air, one shot at a time.")


def _tb_clips(m, f, s, G, hull):
    C = m.clips
    # (her fixed guns: the clip `fire` flickers every gun. The 75 mm is its own: one shot, the barrel back and home,
    #  a big flash and smoke at its muzzle, and she checks in the air with it)
    if "fire" in C:
        C["fire"]["events"] = [e for e in C["fire"]["events"] if e["socket"] != "gun.3"]
    C["fire.cannon"] = {"long": 1.2, "loop": False, "after": "hold", "about": "Her 75 mm fires once: the barrel runs back and home, a big flash and a puff of smoke at its muzzle, and she checks in the air.",
                        "tracks": [{"part": "cannon", "slide": [[0.0, 0.0], [0.05, 0.55], [0.5, 0.55], [1.1, 0.0]]},
                                   {"part": "plane", "move": [[0.0, 0, 0, 0], [0.06, -0.12, 0, 0.03], [0.4, -0.04, 0, 0.0], [1.2, 0, 0, 0]]}],
                        "events": [{"t": 0.0, "fx": "muzzle_flash_big", "socket": "gun.3", "cue": "tank_gun_01"}, {"t": 0.05, "fx": "gun_smoke", "socket": "gun.3", "cue": ""}]}


PLANES["WhatIf_TankBuster"] = {
    "title": "Tank-buster, a what-if (our own design)", "nation_paint": True,
    # (a solid nose; the slab-sided armoured box of her cockpit; a slim tail)
    "hull": [(5.40, -0.10, 0.04, 0.04), (5.30, -0.10, 0.30, 0.30), (5.05, -0.08, 0.48, 0.50), (4.60, -0.05, 0.60, 0.66), (3.80, 0.0, 0.68, 0.78),
             (2.80, 0.0, 0.72, 0.84), (1.50, 0.0, 0.72, 0.86), (0.0, 0.02, 0.70, 0.84), (-1.50, 0.06, 0.62, 0.74), (-3.0, 0.12, 0.50, 0.60),
             (-4.5, 0.20, 0.36, 0.44), (-5.8, 0.26, 0.22, 0.30), (-6.8, 0.30, 0.10, 0.16), (-7.1, 0.30, 0.03, 0.04)],
    "square": 2.6, "screw": 1.65, "blades": 3, "hull_lines": [4.1, 1.15, -1.6, -4.3],
    "hoods": green_hoods(_TB_SCREEN, _TB_PIECES, 0.78),
    "canopy": green_canopy(_TB_SCREEN, _TB_PIECES, screen_bar=0.04),
    "opening": (3.30, 1.85, 56.0), "frames_paint": "upper",
    "wing": [(0.0, 2.35, -0.90, -0.45, 0.72), (2.6, 2.20, -0.85, -0.40, 0.62), (6.0, 1.55, -0.55, -0.15, 0.36), (8.2, 1.05, -0.20, 0.02, 0.18), (8.5, 0.70, 0.05, 0.05, 0.04)],
    "tail": [(0.0, -5.40, -6.90, 0.35, 0.16), (2.6, -5.75, -6.75, 0.35, 0.10), (3.1, -6.05, -6.55, 0.35, 0.02)],
    "fin": [(0, -4.60, -7.05, 0.30, 0.15), (0, -5.50, -7.10, 1.10, 0.12), (0, -6.05, -7.05, 1.85, 0.08), (0, -6.40, -6.95, 2.25, 0.04), (0, -6.60, -6.85, 2.35, 0.01)],
    "patch": 2.6, "colours": dict(_PLAIN), "pitot": True,
    "engines": [(2.6, 2.0, -0.12, True, 3.2)],
    "guns": [(3.6, 0.05, 0.35, 0.022)],
    "tailwheel": (-6.2, -0.80),
    "extras": [
        # (the 75 mm's fairing under her middle, its cradle where the barrel leaves it, the barrel and its muzzle brake)
        ("body", ([(3.60, -0.86, 0.05, 0.05), (3.40, -0.86, 0.20, 0.17), (2.80, -0.86, 0.28, 0.24), (0.0, -0.84, 0.30, 0.26), (-1.80, -0.80, 0.24, 0.20),
                   (-2.80, -0.74, 0.10, 0.08), (-3.10, -0.72, 0.02, 0.02)], 0.0, 2.4, "coat")),
        ("rod", ((3.30, 0.0, -0.86), (3.95, 0.0, -0.86), 0.12, 0.10, "under")),
        ("rod", ((3.90, 0.0, -0.86), (7.20, 0.0, -0.86), 0.068, 0.056, "frame")),
        ("rod", ((7.14, 0.0, -0.86), (7.55, 0.0, -0.86), 0.10, 0.10, "frame")),
        # (the rear gunner's gun on its free mounting)
        ("rod", ((1.42, 0.0, 0.98), (0.70, 0.0, 1.10), 0.022, 0.018, "steel"))],
    "side_marks": [(-3.3, 0.18, "*")], "wing_marks": [(6.2, 0.1, 0.62, "*", "*", True, True)], "code": (-4.4, 0.24),
    "assign": _tb_assign, "hooks": {"fit": [_tb_fit], "clips": [_tb_clips]},
    "doors": [{"name": "cannon", "parent": "plane", "pivot": (5.5, 0.0, -0.86), "note": "the 75 mm's barrel: runs back along its length when it fires (metres)",
               "kw": {"kind": "slide", "slide": (-1, 0, 0), "slide_limits": [0.0, 0.6]}}],
}
CREW["WhatIf_TankBuster"], ROLE["WhatIf_TankBuster"] = 2, "attack"
fitout.SEATS["WhatIf_TankBuster"] = [(0, 2.92, 0.0, 0.0), (0, 1.98, 0.0, 180.0)]


# ---- 3. the gunship escort bomber (class `gunship`) -----------------------------------------------------------
# A four-engined heavy of the B-17's size that carries no bombs: her bay holds ammunition, and she has eight gun
# positions (a turret in her nose and one under her chin, two on her back, a steel ball under her, a blister each
# side at her waist, a turret in her tail), with armour round her gunners. She flies in the bomber stream and the
# fighters that come at it meet a wall of her guns. Twin square fins at the tips of her tailplane leave her tail
# gunner a clear field. Twelve men. Span 32 m, 23.5 m long.
_GS_SCREEN = (9.15, 0.30, 8.75, 1.90, 0.17)
_GS_PIECES = [
    {"rows": [(8.72, 1.18, 0.78, 1.70, 1.92), (8.30, 1.20, 0.80, 1.72, 1.94), (7.88, 1.22, 0.80, 1.72, 1.94), (7.46, 1.24, 0.80, 1.70, 1.92)],
     "hoops": [0, 1, 2, 3], "bar": 0.03},
    {"rows": [(7.48, 1.24, 0.78, None, 1.90), (7.15, 1.26, 0.66, None, 1.76), (6.90, 1.28, 0.50, None, 1.58)], "hoops": [0], "rails": [2, 3], "bar": 0.026, "cap": True}]


def _gs_fit(m, f, s, G, hull):
    set_whatif(m, "gunship", ["B17", "Bomber"], "Guns, not bombs: eight gun positions and twelve men, flying in the bomber stream to meet its fighters.")


def _gs_clips(m, f, s, G, hull):
    m.clips.pop("bombs_away", None)                        # (her bay holds ammunition: she drops nothing)


PLANES["WhatIf_GunshipBomber"] = {
    "title": "Gunship escort bomber, a what-if (our own design)", "nation_paint": True,
    # (a round face to her nose with the turret in it; her cabin's greenhouse on the deck behind; a deep straight
    #  fuselage; her tail tapering up to the tail turret)
    "hull": [(11.05, 0.0, 0.10, 0.10), (11.00, 0.0, 0.46, 0.48), (10.88, 0.0, 0.68, 0.72), (10.60, -0.02, 0.84, 0.90), (10.10, -0.05, 0.96, 1.04),
             (9.40, -0.05, 1.06, 1.18), (8.40, 0.0, 1.16, 1.30), (5.00, 0.0, 1.20, 1.36), (0.0, 0.0, 1.20, 1.36), (-4.0, 0.06, 1.10, 1.26),
             (-7.0, 0.18, 0.90, 1.02), (-9.5, 0.32, 0.66, 0.76), (-11.0, 0.40, 0.50, 0.58), (-11.40, 0.42, 0.46, 0.52), (-11.55, 0.42, 0.36, 0.40),
             (-11.62, 0.42, 0.12, 0.14)],
    "square": 2.5, "screw": 1.70, "blades": 3,
    "hoods": green_hoods(_GS_SCREEN, _GS_PIECES, 1.17),
    "canopy": green_canopy(_GS_SCREEN, _GS_PIECES, screen_bar=0.04),
    "opening": (8.78, 7.75, 52.0), "opening_cabin": True, "pilot_wheel": True, "frames_paint": "upper",
    # (a long straight-tapered wing set at her middle)
    "wing": [(0.0, 4.0, -1.40, -0.30, 1.05), (3.6, 3.70, -1.25, -0.12, 0.86), (7.2, 3.30, -1.02, 0.06, 0.66), (11.0, 2.75, -0.70, 0.26, 0.44),
             (14.8, 2.10, -0.25, 0.46, 0.22), (15.6, 1.80, 0.05, 0.50, 0.12), (16.0, 1.20, 0.35, 0.52, 0.05)],
    "aileron": (0.62, 0.95, 0.76), "flap": (0.08, 0.60, 0.80),
    "tail": [(0.0, -8.4, -11.4, 0.45, 0.40), (5.15, -9.3, -11.15, 0.45, 0.16), (5.35, -9.45, -11.1, 0.45, 0.04)],
    # (a square fin at each tip of her tailplane)
    "fins": [([(0.0, -9.10, -11.45, -0.62, 0.02), (0.0, -9.00, -11.50, -0.50, 0.13), (0.0, -9.05, -11.55, 0.45, 0.16), (0.0, -9.15, -11.55, 1.60, 0.14),
               (0.0, -9.30, -11.50, 2.30, 0.10), (0.0, -9.55, -11.35, 2.52, 0.05), (0.0, -9.90, -11.10, 2.60, 0.01)], 5.15)],
    "patch": 3.0, "colours": dict(_PLAIN), "pitot": True,
    "engines": [(3.6, 3.55, -0.25, True, 3.8), (7.2, 3.05, -0.05, True, 3.2)],
    "tailwheel": (-8.9, -1.25),
    "extras": [
        # (the nose turret in the face of her nose, and its pair of guns)
        ("dome", ((11.05, 0.0, 0.0), (0.62, 0.58, 0.62))), ("rod", ((11.35, 0.16, 0.02), (12.35, 0.16, 0.0), 0.026, 0.02, "steel")),
        # (the chin turret, its fairing aft, and its pair)
        ("ball", ((10.05, 0.0, -1.30), (0.44, 0.44, 0.36), "under")),
        ("body", ([(9.95, -1.28, 0.42, 0.34), (9.55, -1.24, 0.40, 0.28), (9.10, -1.18, 0.32, 0.20), (8.70, -1.14, 0.22, 0.12), (8.40, -1.12, 0.10, 0.05)], 0.0, 2.0, "coat")),
        ("rod", ((10.30, 0.14, -1.38), (11.20, 0.14, -1.50), 0.026, 0.02, "steel")),
        # (the two turrets on her back: the fore one behind her cabin, its guns forward; the after one, its guns aft)
        ("dome", ((6.20, 0.0, 1.46), (0.56, 0.56, 0.50))), ("rod", ((6.40, 0.17, 1.70), (7.30, 0.17, 2.06), 0.026, 0.02, "steel")),
        ("dome", ((-3.40, 0.0, 1.38), (0.56, 0.56, 0.50))), ("rod", ((-3.60, 0.17, 1.62), (-4.75, 0.17, 1.80), 0.026, 0.02, "steel")),
        # (the steel ball under her)
        ("dome", ((-0.60, 0.0, -1.50), (0.58, 0.58, 0.58))), ("rod", ((-0.85, 0.2, -1.68), (-1.80, 0.2, -1.98), 0.026, 0.02, "steel")),
        # (a blister each side at her waist, a man and a gun in each)
        ("dome", ((-6.0, 0.92, 0.30), (0.50, 0.30, 0.42))), ("dome", ((-6.0, -0.92, 0.30), (0.50, 0.30, 0.42))),
        ("rod", ((-6.05, 1.12, 0.36), (-6.70, 1.95, 0.30), 0.024, 0.019, "steel")),
        # (the tail turret, and its pair)
        ("dome", ((-11.75, 0.0, 0.42), (0.50, 0.48, 0.56))), ("rod", ((-11.95, 0.14, 0.40), (-12.95, 0.14, 0.36), 0.026, 0.02, "steel")),
        # (her main wheels go up into the inner nacelles, half of each tyre out in the wind)
        ("rod", ((3.0, 3.42, -0.85), (3.0, 3.78, -0.85), 0.62, 0.62, "tyre")), ("rod", ((3.0, 3.40, -0.85), (3.0, 3.80, -0.85), 0.28, 0.28, "leg"))],
    "side_marks": [(-4.6, 0.55, "*")], "wing_marks": [(12.0, 0.75, 1.0, "*", "*", True, True)], "code": (-7.6, 0.5),
    "hooks": {"fit": [_gs_fit], "clips": [_gs_clips]},
}
CREW["WhatIf_GunshipBomber"], ROLE["WhatIf_GunshipBomber"] = 12, "bomber"
fitout.SEATS["WhatIf_GunshipBomber"] = [(0, 8.25, 0.40, 0.0), (0, 8.25, -0.40, 0.0)]
fitout.SEAT_DROP["WhatIf_GunshipBomber"] = 0.12
fitout.BALL.add("WhatIf_GunshipBomber")
fitout.BLISTER_CREW.add("WhatIf_GunshipBomber")
TAIL_CUT["WhatIf_GunshipBomber"] = -7.3                     # (aft of her waist blisters)


# ---- 4. the radar night fighter (class `nightftr`) ------------------------------------------------------------
# Twin-engined, three men in a row under a long greenhouse: her pilot, her radar operator at his scopes, and a
# gunner facing aft. The radar's aerials stand on her nose like a stag's antlers; four cannon fire from a tray under
# her, and a pair behind her cabin fire upward and forward, so that she can creep up under a bomber in the dark and
# fire into its belly from where its gunners cannot see her. Shrouds over her exhausts hide their flames; matt black
# all over. Span 18 m, 13.9 m long.
_NF_SCREEN = (4.62, 0.17, 4.22, 1.13, 0.15)
_NF_PIECES = [
    {"rows": [(4.20, 0.62, 0.43, 1.02, 1.15), (3.75, 0.63, 0.44, 1.03, 1.16), (3.30, 0.64, 0.44, 1.03, 1.15)], "hoops": [0, 1, 2], "bar": 0.024},
    {"rows": [(3.32, 0.64, 0.44, 1.02, 1.13), (2.85, 0.65, 0.44, 1.01, 1.11), (2.40, 0.66, 0.43, 1.00, 1.09)], "hoops": [0, 1, 2], "bar": 0.022},
    {"rows": [(2.42, 0.66, 0.43, 0.99, 1.08), (1.95, 0.67, 0.42, 0.98, 1.06), (1.55, 0.68, 0.41, 0.96, 1.03)], "hoops": [0, 1, 2], "bar": 0.022},
    {"rows": [(1.57, 0.68, 0.40, None, 1.01), (1.30, 0.70, 0.36, None, 0.94), (1.10, 0.71, 0.30, None, 0.86)], "hoops": [0], "rails": [2, 3], "bar": 0.02, "cap": True}]
_NF_BLACK = {"upper": "1A1C1E", "upper2": "202224", "under": "151618", "nose": "1A1C1E", "boss": "111111"}


def _nf_antlers():
    """Her radar's aerials: four masts raking forward from her nose, each with a cross of dipoles at its tip."""
    out = []
    for zs in (1.0, -1.0):
        a, b = (5.90, 0.22, 0.21 * zs), (6.85, 0.30, 0.29 * zs)
        out.append(("rod", (a, b, 0.016, 0.012, "leg")))
        out.append(("rod", ((b[0], b[1], b[2] - 0.30), (b[0], b[1], b[2] + 0.30), 0.007, 0.007, "leg")))
        out.append(("rod", ((b[0], b[1] - 0.26, b[2]), (b[0], b[1] + 0.26, b[2]), 0.007, 0.007, "leg")))
    return out


def _nf_fit(m, f, s, G, hull):
    k = 0
    for y in (0.10, -0.10, 0.26, -0.26):
        k += 1
        add_socket(m, f"gun.{k}", (4.00 if abs(y) < 0.2 else 3.85, y, -0.86), X, "plane", "a cannon's muzzle in the tray under her")
    for y in (0.12, -0.12):
        k += 1
        add_socket(m, f"gun.{k}", (0.53, y, 0.98), Vector((0.36, 0.0, 0.93)), "plane", "an upward-firing cannon behind her cabin (clip fire.upward)")
    add_socket(m, "radar", (6.85, 0.0, 0.0), X, "plane", "her radar's aerials: where its beam looks out")
    set_whatif(m, "nightftr", ["Fighter"], "Radar on her nose and cannon that fire upward: she finds bombers in the dark and strikes from under them.")


def _nf_clips(m, f, s, G, hull):
    C = m.clips
    up = [g for g in ("gun.5", "gun.6")]
    if "fire" in C:
        C["fire"]["events"] = [e for e in C["fire"]["events"] if e["socket"] not in up]
    C["fire.upward"] = {"long": 0.6, "loop": True, "about": "Her two upward-firing cannon, from under a bomber's belly: a short flash at each muzzle, no tracer.",
                        "tracks": [], "events": [{"t": round(0.15 * i, 2), "fx": "muzzle_flash_small", "socket": g, "cue": "plane_guns" if i == 0 else ""}
                                                 for i, g in enumerate(up + up)]}


PLANES["WhatIf_NightFighter"] = {
    "title": "Radar night fighter, a what-if (our own design)", "nation_paint": False,
    "hull": [(6.20, -0.05, 0.05, 0.05), (6.10, -0.05, 0.28, 0.28), (5.85, -0.04, 0.44, 0.46), (5.40, -0.02, 0.56, 0.60), (4.70, 0.0, 0.64, 0.72),
             (3.80, 0.0, 0.68, 0.78), (2.60, 0.0, 0.70, 0.80), (1.20, 0.02, 0.68, 0.78), (0.0, 0.04, 0.64, 0.72), (-1.5, 0.08, 0.56, 0.62),
             (-3.0, 0.14, 0.46, 0.50), (-4.6, 0.20, 0.34, 0.38), (-6.0, 0.25, 0.22, 0.26), (-7.2, 0.28, 0.10, 0.13), (-7.7, 0.28, 0.02, 0.03)],
    "square": 2.2, "screw": 1.6, "blades": 3, "hull_lines": [4.9, 1.0, -1.8, -4.4],
    "hoods": green_hoods(_NF_SCREEN, _NF_PIECES, 0.73),
    "canopy": green_canopy(_NF_SCREEN, _NF_PIECES, screen_bar=0.03),
    "opening": (4.18, 1.62, 51.0), "frames_paint": "upper",
    "wing": [(0.0, 2.15, -0.95, -0.35, 0.62), (2.7, 1.95, -0.85, -0.28, 0.52), (6.0, 1.35, -0.55, -0.05, 0.32), (8.6, 0.85, -0.22, 0.12, 0.16),
             (9.0, 0.50, 0.05, 0.14, 0.04)],
    "aileron": (0.60, 0.94, 0.76),
    "tail": [(0.0, -5.60, -7.10, 0.40, 0.15), (2.8, -5.95, -7.00, 0.40, 0.09), (3.3, -6.25, -6.80, 0.40, 0.02)],
    "fin": [(0, -4.90, -7.60, 0.25, 0.14), (0, -5.70, -7.65, 1.00, 0.12), (0, -6.20, -7.60, 1.75, 0.08), (0, -6.55, -7.45, 2.15, 0.04), (0, -6.75, -7.30, 2.22, 0.01)],
    "patch": 0.0, "colours": dict(_NF_BLACK), "pitot": True,
    "engines": [(2.7, 1.4, -0.15, False, 2.6)],
    "tailwheel": (-6.3, -0.62),
    "extras": _nf_antlers() + [
        # (the tray of four cannon under her, and their barrels)
        ("body", ([(3.40, -0.84, 0.05, 0.04), (3.20, -0.84, 0.32, 0.11), (2.60, -0.84, 0.38, 0.14), (0.40, -0.82, 0.36, 0.12), (-0.60, -0.78, 0.20, 0.06),
                   (-0.90, -0.76, 0.03, 0.02)], 0.0, 2.4, "coat")),
        ("rod", ((3.10, 0.10, -0.86), (4.00, 0.10, -0.86), 0.03, 0.024, "frame")), ("rod", ((3.00, 0.26, -0.86), (3.85, 0.26, -0.86), 0.03, 0.024, "frame")),
        # (the pair firing upward behind her cabin, their muzzles through her roof)
        ("rod", ((0.40, 0.12, 0.62), (0.53, 0.12, 0.98), 0.03, 0.024, "frame")),
        # (the shrouds over her exhausts, each side of each nacelle, that hide their flames from the bomber's gunners)
        ("body", ([(3.36, -0.10, 0.03, 0.03), (3.22, -0.10, 0.09, 0.10), (1.10, -0.13, 0.09, 0.10), (0.80, -0.14, 0.03, 0.03)], 3.17, 2.0, "soot")),
        ("body", ([(3.36, -0.10, 0.03, 0.03), (3.22, -0.10, 0.09, 0.10), (1.10, -0.13, 0.09, 0.10), (0.80, -0.14, 0.03, 0.03)], 2.23, 2.0, "soot")),
        # (the gunner's gun on its free mounting, facing aft)
        ("rod", ((1.40, 0.0, 0.92), (0.62, 0.0, 1.04), 0.022, 0.018, "steel"))],
    "side_marks": [(-3.4, 0.2, "*")], "wing_marks": [(6.6, 0.1, 0.6, "*", "*", True, True)], "code": (-4.6, 0.25),
    "hooks": {"fit": [_nf_fit], "clips": [_nf_clips]},
}
CREW["WhatIf_NightFighter"], ROLE["WhatIf_NightFighter"] = 3, "fighter"
fitout.SEATS["WhatIf_NightFighter"] = [(0, 3.75, 0.0, 0.0), (0, 2.85, 0.0, 0.0), (0, 1.95, 0.0, 180.0)]


# ---- 5. the sea hunter flying boat (class `seahunter`) --------------------------------------------------------
# Four engines on a high wing over a deep boat of a hull with two steps in her bottom, to come unstuck from the sea;
# her bow rises to a turret, and under it hangs the dome of a search radar that finds a surfaced submarine at night.
# A float hangs under each wing out near its tip to steady her on the water; in the air each swings out and up on its
# struts and becomes the wing's tip. A turret in her tail, a blister each side at her waist; torpedoes (or depth
# charges) under her inner wings. Ten men. Span 38 m with her floats up, 27.4 m long.
_SH_SCREEN = (10.55, 0.30, 10.15, 2.46, 0.17)
_SH_PIECES = [
    {"rows": [(10.12, 1.60, 0.78, 2.22, 2.44), (9.70, 1.62, 0.80, 2.24, 2.46), (9.28, 1.64, 0.80, 2.24, 2.46), (8.86, 1.66, 0.80, 2.22, 2.44)],
     "hoops": [0, 1, 2, 3], "bar": 0.03},
    {"rows": [(8.88, 1.66, 0.78, None, 2.42), (8.55, 1.68, 0.66, None, 2.28), (8.30, 1.70, 0.50, None, 2.08)], "hoops": [0], "rails": [2, 3], "bar": 0.026, "cap": True}]
_SH_FLOAT = (15.15, 2.52, 2.55)                             # (how far out each float's hinge is, its height, how far below it the float hangs)
_SH_TORPEDO = (1.8, 3.2, 1.50)                              # (the middle of each torpedo: along her, out, up)


def _sh_torpedo():
    x, y, z = _SH_TORPEDO
    a, b, r = x + 2.65, x - 2.65, 0.225
    return [("body", ([(a, z, 0.03, 0.03), (a - 0.22, z, 0.17, 0.17), (a - 0.55, z, r, r), (b + 1.0, z, r, r), (b + 0.35, z, 0.1, 0.1), (b, z, 0.03, 0.03)], y, 2.0, "torpedo")),
            ("rod", ((b + 0.02, y, z), (b + 0.42, y, z), 0.24, 0.24, "torpedo")),
            ("rod", ((x + 0.8, y, z + 0.2), (x + 0.8, y, z + 0.36), 0.05, 0.05, "under")), ("rod", ((x - 0.8, y, z + 0.2), (x - 0.8, y, z + 0.36), 0.05, 0.05, "under"))]


def _sh_fit(m, f, s, G, hull):
    out, hz, drop = _SH_FLOAT
    for nm, hand, parent in (("float.port", 1.0, "wing.port"), ("float.starboard", -1.0, "wing.starboard")):
        m.part(nm, parent, (1.5, hand * out, hz), axis=(hand, 0, 0), kind="turn", limits=[0, 90],
               note="a wing-tip float on its struts; plus swings it out and up until it is the wing's tip (clip floats_up)")
        m.use(nm)
        y, cz = hand * out, hz - drop
        m.lathe((3.40, y, cz), -X, [(0.0, 0.02), (0.25, 0.20), (0.6, 0.35), (1.2, 0.42), (2.5, 0.40), (3.3, 0.22), (3.8, 0.03)], "under", 16 if m.fine else 8)
        m.box((1.5, y, cz - 0.36), (2.6, 0.05, 0.10), "under")                    # (its keel)
        for x in (2.35, 0.85):                                                     # (its struts up to the wing, and a brace between them)
            m.rod((x, y, cz + 0.36), (x, y, hz - 0.18), 0.055, "under", rb=0.045, sides=10 if m.fine else 6)
        m.rod((2.35, y, cz + 0.55), (0.85, y, hz - 0.30), 0.03, "under", sides=8 if m.fine else 5)
    m.use("plane")
    x, y, z = _SH_TORPEDO
    for k in m.sockets:
        if k["name"] == "torpedo":
            k["at"] = Vector((x, -y, z - 0.25))
            k["note"] = "where her starboard torpedo leaves (torpedo.2: her port one)"
            m.sockets.append({**k, "name": "torpedo.2", "at": Vector((x, y, z - 0.25)), "note": "where her port torpedo leaves"})
            break
    add_socket(m, "radar", (12.30, 0.0, -1.0), X, "plane", "her search radar's dome under her bow")
    set_whatif(m, "seahunter", ["Kate", "TorpedoBomber"], "A flying boat with radar: she finds submarines on the surface at night, and her floats fold up to make her wingtips.")


def _sh_clips(m, f, s, G, hull):
    m.clips["floats_up"] = {"long": 6.0, "loop": False, "after": "hold", "about": "After she has come unstuck from the sea: her floats swing out and up on their struts until each is her wing's tip. Played backwards, they come down for a landing.",
                            "tracks": [{"part": nm, "turn": [[0.0, 0.0], [1.0, 8.0], [5.0, 84.0], [6.0, 90.0]]} for nm in ("float.port", "float.starboard")],
                            "events": []}


PLANES["WhatIf_SeaHunter"] = {
    "title": "Sea hunter flying boat, a what-if (our own design)", "nation_paint": True,
    # (her bow rising to the turret; her deep boat of a hull, its bottom stepped at her middle and again 6 m aft, the
    #  after body sweeping up to her tail turret)
    "hull": [(13.20, 0.70, 0.12, 0.20), (13.05, 0.525, 0.45, 0.475), (12.80, 0.375, 0.72, 0.775), (12.30, 0.235, 0.95, 1.085), (11.50, 0.15, 1.15, 1.35),
             (10.50, 0.105, 1.26, 1.555), (9.50, 0.07, 1.30, 1.67), (7.00, 0.03, 1.32, 1.73), (0.02, 0.03, 1.32, 1.73), (-0.02, 0.14, 1.32, 1.62),
             (-3.00, 0.18, 1.28, 1.58), (-5.98, 0.22, 1.18, 1.52), (-6.02, 0.345, 1.17, 1.395), (-8.50, 0.585, 0.92, 1.135), (-11.00, 0.885, 0.62, 0.835),
             (-12.60, 1.10, 0.42, 0.65), (-13.30, 1.225, 0.26, 0.475), (-13.50, 1.30, 0.12, 0.25)],
    "square": 2.6, "screw": 1.70, "blades": 3,
    "hoods": green_hoods(_SH_SCREEN, _SH_PIECES, 1.66),
    "canopy": green_canopy(_SH_SCREEN, _SH_PIECES, screen_bar=0.04),
    "opening": (10.12, 8.95, 55.0), "opening_cabin": True, "pilot_wheel": True, "frames_paint": "upper",
    # (her high wing on her back; its tip is blunt, for the float folds up against it)
    "wing": [(0.0, 5.0, -1.6, 2.25, 1.10), (5.0, 4.8, -1.45, 2.30, 0.95), (10.0, 4.4, -1.15, 2.40, 0.72), (14.0, 3.9, -0.80, 2.50, 0.50),
             (16.8, 3.5, -0.55, 2.56, 0.36), (17.3, 3.4, -0.48, 2.57, 0.34)],
    "aileron": (0.62, 0.97, 0.76),
    "tail": [(0.0, -9.3, -12.5, 1.95, 0.40), (5.6, -10.4, -12.3, 1.95, 0.16), (6.2, -10.8, -12.1, 1.95, 0.04)],
    "fin": [(0, -8.4, -12.9, 1.70, 0.34), (0, -9.4, -12.9, 2.8, 0.30), (0, -10.2, -12.8, 4.2, 0.22), (0, -10.8, -12.6, 5.4, 0.14), (0, -11.3, -12.3, 5.9, 0.06),
            (0, -11.7, -12.0, 6.0, 0.01)],
    "patch": 3.0, "colours": dict(_PLAIN), "pitot": True,
    "engines": [(5.0, 4.6, 2.20, True, 3.4), (10.0, 4.25, 2.36, True, 3.0)],
    "tailwheel": None,
    "extras": _sh_torpedo() + [
        # (the fairing that blends her wing's root into her back)
        ("body", ([(5.6, 1.80, 0.05, 0.05), (5.2, 1.86, 0.70, 0.30), (4.0, 1.92, 0.95, 0.42), (0.0, 1.92, 0.95, 0.42), (-1.6, 1.88, 0.75, 0.32),
                   (-2.4, 1.82, 0.10, 0.08)], 0.0, 2.4, "coat")),
        # (a chine strake each side of her forebody, where her sides meet her planing bottom, that throws the spray clear)
        ("rod", ((12.6, 0.53, -0.45), (9.5, 1.01, -1.20), 0.045, 0.045, "under")), ("rod", ((9.5, 1.01, -1.20), (0.05, 1.05, -1.25), 0.045, 0.045, "under")),
        # (the turret over her bow, and its pair)
        ("dome", ((12.40, 0.0, 1.30), (0.55, 0.52, 0.55))), ("rod", ((12.70, 0.15, 1.35), (13.75, 0.15, 1.30), 0.026, 0.02, "steel")),
        # (the search radar's dome under her bow)
        ("body", ([(12.75, -0.62, 0.04, 0.04), (12.60, -0.74, 0.30, 0.22), (12.30, -0.98, 0.42, 0.30), (11.95, -1.12, 0.36, 0.24), (11.70, -1.18, 0.05, 0.04)], 0.0, 2.0, "under")),
        # (a blister each side at her waist, a man and a gun in each)
        ("dome", ((-4.2, 1.20, 0.80), (0.62, 0.36, 0.55))), ("dome", ((-4.2, -1.20, 0.80), (0.62, 0.36, 0.55))),
        ("rod", ((-4.25, 1.45, 0.85), (-5.05, 2.25, 0.75), 0.024, 0.019, "steel")),
        # (the tail turret, and its pair)
        ("dome", ((-13.55, 0.0, 1.28), (0.48, 0.46, 0.50))), ("rod", ((-13.80, 0.13, 1.28), (-14.80, 0.13, 1.24), 0.026, 0.02, "steel"))],
    "side_marks": [(-2.4, 0.9, "*")], "wing_marks": [(13.0, 0.75, 1.1, "*", "*", True, True)], "code": (-8.0, 0.9),
    "hooks": {"fit": [_sh_fit], "clips": [_sh_clips]},
}
CREW["WhatIf_SeaHunter"], ROLE["WhatIf_SeaHunter"] = 10, "torpedo bomber"
fitout.SEATS["WhatIf_SeaHunter"] = [(0, 9.62, 0.40, 0.0), (0, 9.62, -0.40, 0.0)]
fitout.SEAT_DROP["WhatIf_SeaHunter"] = 0.12
fitout.BLISTER_CREW.add("WhatIf_SeaHunter")
TAIL_CUT["WhatIf_SeaHunter"] = -6.6                         # (aft of her second step and her waist blisters)


# ---- 6. the heavy assault glider (flies the glider tank, class `glidertank`) --------------------------------------
# No engine: a tug tows her to the landing zone and lets her go. A box of a fuselage round a hold that takes the
# glider tank (land/whatif_tanks.GLIDER_HOLD: 4.8 m long, 2.4 wide, 1.95 high); her whole nose is a door, hinged on
# her starboard side, that swings open, and two ramps unfold from the sill of her hold to the ground for the tank
# to drive out on. Her two pilots sit side by side in a glazed cockpit on the roof of her hold. A high wing, fixed
# main wheels, two skids under her belly, a tailwheel. Span 34 m, 20 m long.
_GL_SEAM = 6.40                                             # (where her nose door meets her fuselage)
_GL_SCREEN = (6.30, 0.40, 5.90, 2.48, 0.17)
_GL_PIECES = [
    {"rows": [(5.88, 1.52, 0.86, 2.26, 2.54), (5.40, 1.52, 0.88, 2.28, 2.56), (4.92, 1.52, 0.88, 2.28, 2.56)], "hoops": [0, 1, 2], "bar": 0.03},
    {"rows": [(4.94, 1.52, 0.86, None, 2.52), (4.60, 1.52, 0.72, None, 2.28), (4.35, 1.52, 0.55, None, 1.98)], "hoops": [0], "rails": [2, 3], "bar": 0.026, "cap": True}]
_GL_FLOOR = -0.93                                           # (the top of her hold's floor)
_GL_RAMP = (0.82, 2.0, 0.56)                                # (each ramp: how far out its middle is, how long, how wide)


def _gl_assign(kind, c, mean, ob):
    if kind in ("hull", "lines", "frames", "glints") and c.x > _GL_SEAM:
        return "nose"                                      # (her nose door: her skin ahead of the seam, and the lines on it)
    return None


def _gl_fit(m, f, s, G, hull):
    # (her hold: her skin again from inside, dark; the bulkhead at its after end; its floor, with the tank's lashing
    #  rings along each side)
    m.use("nose")
    fitout.liner(m, f, hull, hull.front - 0.25, _GL_SEAM, lambda x, high: False)
    m.use("plane")
    fitout.liner(m, f, hull, _GL_SEAM, 1.30, lambda x, high: False)
    fitout.wall(m, hull, 1.30, X)
    m.box((3.85, 0.0, _GL_FLOOR - 0.03), (5.10, 2.0, 0.06), "inside")
    if m.fine:
        for x in (5.6, 4.4, 3.2, 2.0):
            for y in (0.92, -0.92):
                m.rod((x - 0.06, y, _GL_FLOOR + 0.02), (x + 0.06, y, _GL_FLOOR + 0.02), 0.018, "gunmetal", sides=6)
    # (the two ramps, lying on her floor, each hinged at the sill to unfold forward and down to the ground)
    y0, long, wide = _GL_RAMP
    for nm, hand in (("ramp.port", 1.0), ("ramp.starboard", -1.0)):
        m.part(nm, "plane", (_GL_SEAM - 0.04, hand * y0, _GL_FLOOR + 0.03), axis=(0, -1, 0), kind="turn", limits=[0, 160],
               note="a ramp, lying on her floor; plus unfolds it forward over the sill and down to the ground (clip unload)")
        m.use(nm)
        m.box((_GL_SEAM - 0.04 - 0.5 * long, hand * y0, _GL_FLOOR + 0.055), (long, wide, 0.05), "gunmetal")
        if m.fine:
            for k in range(9):                              # (cleats across it, for the tracks to grip)
                x = _GL_SEAM - 0.24 - k * (long - 0.4) / 8.0
                m.box((x, hand * y0, _GL_FLOOR + 0.09), (0.03, wide - 0.04, 0.025), "gunmetal")
    m.use("plane")
    add_socket(m, "engine.1", (2.0, 0.0, 0.6), X, "plane", "no engine: where the wind's sound on her plays (clip fly)")
    add_socket(m, "hold", (3.85, 0.0, _GL_FLOOR), Z, "plane", "the middle of her hold's floor: where the glider tank is lashed down, facing forward")
    add_socket(m, "tow", (hull.front + 0.05, 0.0, hull.z(-hull.front)), X, "nose", "where the tug's tow rope is made fast")
    f.engines = 1
    set_whatif(m, "glidertank", ["Bomber"], "No engine: towed to the landing zone, she glides in with a light tank in her hold, and her whole nose opens to let it out.")
    m.info["file"]["engines"] = 0
    m.info["file"]["carries"] = {"model": "WhatIf_GliderTank", "socket": "hold", "hold": {"long": 4.80, "wide": 2.40, "high": 1.95}}


def _gl_clips(m, f, s, G, hull):
    C = m.clips
    if "fly" in C:
        C["fly"]["tracks"] = [t for t in C["fly"]["tracks"] if not t["part"].startswith("prop")]
        C["fly"]["about"] = "Under way on the wind: no engine, so no airscrew; she rides the air a little. The game flies her; this only lives on top."
        for e in C["fly"]["events"]:
            if e.get("fx") == "engine_loop":
                e["fx"], e["cue"] = "", "glider_wind_loop"
    C.pop("idle", None)
    door, ramp = [[0.0, 0.0], [0.6, 6.0], [3.4, 104.0], [4.0, 110.0]], [[0.0, 0.0], [4.0, 0.0], [4.6, 12.0], [6.6, 150.0], [7.2, 157.0]]
    C["unload"] = {"long": 7.2, "loop": False, "after": "hold", "about": "On the ground: her nose swings open to starboard, then the two ramps unfold over the sill to the ground. The glider tank then drives out (the game moves it from her socket `hold`).",
                   "tracks": [{"part": "nose", "turn": door}, {"part": "ramp.port", "turn": ramp}, {"part": "ramp.starboard", "turn": ramp}],
                   "events": [{"t": 0.0, "fx": "", "socket": "tow", "cue": "bomb_doors"}, {"t": 4.0, "fx": "", "socket": "hold", "cue": "bomb_doors"}]}


PLANES["WhatIf_AssaultGlider"] = {
    "title": "Heavy assault glider, a what-if (our own design)", "nation_paint": True, "glider": True,
    # (a blunt nose that is all door; a box of a fuselage round her hold; a long tail tapering up to her fin)
    "hull": [(8.70, 0.10, 0.30, 0.30), (8.55, 0.12, 0.85, 0.80), (8.20, 0.15, 1.20, 1.12), (7.60, 0.18, 1.36, 1.26), (_GL_SEAM + 0.02, 0.20, 1.40, 1.30),
             (_GL_SEAM - 0.02, 0.20, 1.40, 1.30), (2.0, 0.20, 1.40, 1.30), (-1.6, 0.22, 1.36, 1.26), (-4.0, 0.30, 1.10, 1.06), (-7.0, 0.42, 0.72, 0.78),
             (-9.6, 0.52, 0.42, 0.50), (-11.0, 0.58, 0.22, 0.28), (-11.40, 0.60, 0.05, 0.06)],
    "square": 3.4, "hull_lines": [_GL_SEAM, 1.3, -2.6, -6.0],
    "hoods": green_hoods(_GL_SCREEN, _GL_PIECES, 1.50),
    "canopy": green_canopy(_GL_SCREEN, _GL_PIECES, screen_bar=0.04), "pilot_wheel": True, "frames_paint": "upper",
    # (a high wing on her roof, its root clear of her hold)
    "wing": [(0.0, 4.0, -0.9, 1.95, 1.05), (4.0, 3.8, -0.8, 2.0, 0.90), (10.0, 3.3, -0.4, 2.1, 0.60), (15.0, 2.7, 0.1, 2.2, 0.32), (16.6, 2.3, 0.45, 2.24, 0.16),
             (17.0, 1.8, 0.75, 2.25, 0.06)],
    "aileron": (0.60, 0.97, 0.74), "flap": (0.06, 0.55, 0.78),
    "tail": [(0.0, -8.6, -11.0, 1.05, 0.28), (4.6, -9.3, -10.9, 1.05, 0.12), (5.0, -9.6, -10.7, 1.05, 0.03)],
    "fin": [(0, -8.2, -11.3, 0.95, 0.24), (0, -9.0, -11.3, 2.0, 0.20), (0, -9.6, -11.2, 3.0, 0.14), (0, -10.1, -11.0, 3.6, 0.07), (0, -10.4, -10.8, 3.75, 0.01)],
    "patch": 3.0, "colours": dict(_PLAIN), "pitot": True,
    "tailwheel": (-9.0, -0.70),
    "extras": [
        # (her main wheels, fixed, each on a leg from her side and a brace)
        ("rod", ((3.4, 1.08, -1.40), (3.4, 1.40, -1.40), 0.42, 0.42, "tyre")), ("rod", ((3.4, 1.05, -1.40), (3.4, 1.43, -1.40), 0.16, 0.16, "leg")),
        ("rod", ((3.4, 1.10, -0.70), (3.4, 1.22, -1.40), 0.07, 0.06, "leg")), ("rod", ((2.3, 0.95, -1.00), (3.4, 1.22, -1.40), 0.05, 0.05, "leg")),
        # (a skid under each side of her belly, its fore end turned up)
        ("rod", ((6.30, 0.55, -1.12), (6.05, 0.55, -1.24), 0.05, 0.05, "leg")), ("rod", ((6.05, 0.55, -1.24), (2.2, 0.55, -1.30), 0.05, 0.05, "leg")),
        ("rod", ((5.4, 0.55, -1.10), (5.4, 0.55, -1.25), 0.035, 0.035, "leg")), ("rod", ((3.0, 0.55, -1.10), (3.0, 0.55, -1.28), 0.035, 0.035, "leg"))],
    "side_marks": [(-3.0, 0.4, "*")], "wing_marks": [(12.0, 0.7, 1.0, "*", "*", True, True)], "code": (-5.4, 0.45),
    "assign": _gl_assign, "hooks": {"fit": [_gl_fit], "clips": [_gl_clips]},
    "doors": [{"name": "nose", "parent": "plane", "pivot": (_GL_SEAM, 1.47, -0.15), "note": "her nose, all of it a door hinged on her starboard side; plus swings it open (clip unload)",
               "kw": {"kind": "turn", "axis": (0, 0, -1), "limits": [0, 110]}}],
}
CREW["WhatIf_AssaultGlider"], ROLE["WhatIf_AssaultGlider"] = 2, "glider"
fitout.SEATS["WhatIf_AssaultGlider"] = [(0, 5.45, 0.42, 0.0), (0, 5.45, -0.42, 0.0)]
fitout.SEAT_DROP["WhatIf_AssaultGlider"] = 0.0
fitout.CABIN_FLOOR["WhatIf_AssaultGlider"] = 1.48
