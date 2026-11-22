"""A fighter pilot kept inside her skin (2026-11-22).

On narrow fuselages the pilot's shoulders came out through her skin under the canopy (Hurricane, Fw 190, Warhawk, the
plain fighter, the plain carrier fighter), and his left elbow, reaching for the throttle on the cockpit wall, came out
through her port side (Bf 109, Mustang, C.202, Yak): right where the named aces' name line and victory marks are
painted (the air rules thread, for Brian's named aces). He now sits low enough for his shoulders to be inside her
fuselage where it is wide enough (lower by MOST at most, and never so low that his chin is under her skin's top: the
plain carrier fighter's 13.7 cm is the most any is lowered), and anything of him or his controls that would still stand
out of her side is drawn in to just inside it."""
import math

SHOULDER = 0.27              # (how far out from his middle his shoulders and the tops of his arms reach: 0.2 + the sleeve's 0.066)
MOST = 0.18                  # (the most he is lowered)
CHIN = 0.17                  # (his chin and the foot of his neck, under the middle of his head: never lower than her skin's top, so that his head is seen over her rail)
SPARE = 0.015                # (how far inside her skin anything of him stays; an open cockpit's lining stands 3.5 cm in)


FIGHTERS = {"Spitfire", "Hurricane", "Bf109", "Fw190", "Zero", "Yak", "Warhawk", "Fighter", "Mustang", "Hellcat", "CarrierFighter",
            "C202", "Me262", "WhatIf_EscortFighter", "WhatIf_NightFighter", "Wildcat"}


def wanted(f):
    """Her pilot is one this keeps inside her: a fighter's (other machines' crews are as they were)."""
    return getattr(f, "name", None) in FIGHTERS


def half_wide(hull, x, z):
    """How far her skin stands out to one side at height z at station x (0 above her top or under her keel)."""
    lo, hi = math.pi / 2.0, 1.5 * math.pi            # (her top, round her side, to her keel)
    if not hull.at(x, hi).z < z < hull.at(x, lo).z:
        return 0.0
    for _ in range(40):
        mid = 0.5 * (lo + hi)
        if hull.at(x, mid).z > z:
            lo = mid
        else:
            hi = mid
    return abs(hull.at(x, 0.5 * (lo + hi)).y)


def skin_top(f, hull):
    """How high her skin comes up her side at x: her top, or where an open cockpit is cut out of her (fighters.opening:
    above that his shoulders and head are under her canopy's glass, and she has no skin to keep him inside)."""
    cut = getattr(f, "cockpit", None)

    def top(x):
        if cut and cut[1] - 0.05 < x < cut[0] + 0.05:
            return hull.at(x, math.radians(180.0 - cut[2])).z
        return hull.at(x, math.pi / 2.0).z
    return top


def drop(hull, x, z, top, hood=None):
    """How much lower he sits (his head's middle at z) for his shoulders to have room: inside her where she is wide
    enough, or, where they are above her skin's top (an open cockpit, or her hood), inside her hood's glass."""
    def room(zq):
        if zq < top(x) - 0.004 or hood is None:
            return half_wide(hull, x, zq)
        return half_wide(hood, x, zq) or half_wide(hull, x, zq)     # (under her hood's foot: her skin, as if not cut)
    most = min(MOST, max(0.0, z - CHIN - top(x)))
    d = 0.0
    while d < most - 1e-9 and room(z - d - 0.25) < SHOULDER + SPARE:
        d = min(d + 0.005, most)                                    # (never past `most`: his chin stays over her skin's top)
    return d


def squash(m, hull, n0, top, spare=SPARE, hood=None):
    """Whatever of him (and his controls) was drawn from point n0 on and stands out of her side, drawn in to just
    inside her skin; above her skin's top, to just inside her canopy's glass (`hood`, her hood's body), where his
    shoulders are wider than her narrow hood. Returns the most moved."""
    most = 0.0
    for i in range(n0, len(m.verts)):
        v = m.verts[i]
        if v.z < top(v.x) - 0.004:
            w = half_wide(hull, v.x, v.z) - spare
        elif hood is not None and hood.back < v.x < hood.front:
            w = (half_wide(hood, v.x, v.z) or half_wide(hull, v.x, v.z)) - SPARE     # (under her hood's foot: her skin)
        else:
            continue
        if w > 0.0 and abs(v.y) > w:
            most = max(most, abs(v.y) - w)
            v.y = math.copysign(w, v.y)
    return most
