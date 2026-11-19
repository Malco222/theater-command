"""Aircraft ranged on a carrier's deck: a model of its own for each kind of machine.

A carrier's file says which range she wears, by the month (`deck_park`: the plain navies and Britain the generic
carrier fighter, America that one until 1943 and then the Hellcat, Japan the Zero), and the range stands at the pivot
of her part `deckpark`. So one carrier serves every year of the war, and her deck is empty when her air group is up.

Each machine in a range is that aircraft's own plain model (the far mesh of her file: the same shape, paints,
camouflage and markings as the one that flies), without the blur of her airscrew and with nobody in her, standing on
her undercarriage, tail down, chocked, her airscrew stopped at her own angle. Each is a part of its own (`plane.1`, `plane.2`, ...), so the game can take
them off the deck one at a time. The file carries the aircraft's own `schemes` and `mark_months`, so the range is
painted by the month exactly as the aircraft is.
"""
import math

from mathutils import Matrix, Vector

import planes
import fitout
from rigid import Model, X, Z, mirror, ring_cut

MACHINES = ("CarrierFighter", "Hellcat", "Zero")
CREW = {"skin", "suit", "vest", "goggles", "boot"}       # (nobody sits in a machine that is chocked)
BLUR = {"blur", "blur_tip"}                              # (her airscrew is stopped)
ROWS = (2, 1, 2, 1, 2, 1, 2)                              # (two abreast, then one between them: eleven machines)
CHOCK = "7A5A34"                                         # (wooden chocks)
_STOOD = {}


PITCH = {"Hellcat": 13.0, "Zero": 11.5}                 # (how far her nose is up as she stands, degrees; 12 where not said)
NO_TANK = {"Hellcat"}                                    # (ranged without the long-range tank under her middle)
TAIL_R = 0.13                                            # (her tailwheel's radius)
TIP = 0.06                                               # (the painted tip of a blade: this much of its length)
TIP_YELLOW = "D9B224"
TIPPED = ("USA", "UK")                                   # (the navies whose airscrews wear yellow tips; Japan's wore two red bands and no yellow)


def plain(kind):
    """Her plain model (the far mesh of her file), without the tank she flies with where she is ranged without it."""
    spec = planes.EXTRA_PLANES.get(kind)
    held = None
    if kind in NO_TANK and spec and spec.get("extras"):
        held, spec["extras"] = spec["extras"], []
    try:
        return planes.BUILDERS[kind](False)
    finally:
        if held is not None:
            spec["extras"] = held


def stance(kind, pm, pf, body):
    """How she sits on a deck: how far her nose is up (radians), how long her legs are below the wing, where they
    are, her wheels' radius, and the height of the deck in her own tipped axes. Her nose is up as her kind stands;
    the tips of her airscrew clear the deck by a foot and more, and nothing else of her comes nearer it than a
    hand's breadth and a half. Her legs and her tailwheel's strut are as long as that asks."""
    half = pf.half
    u = min(max(0.27 * half, 1.5), 2.6)
    top = pf.wing.at(u, 0.32, False)                     # (under her wing, a third of the way back)
    rw = 0.34 if half < 7.0 else 0.42
    hub = max(pf.hubs, key=lambda h: h.x)
    th = math.radians(PITCH.get(kind, 12.0))
    s, c = math.sin(th), math.cos(th)
    g = min(min(v.z * c + v.x * s for v in body) - 0.25, hub.z * c + hub.x * s - pf.screw - 0.35)
    leg = top.z - (g + rw - top.x * s) / c
    return th, leg, u, top, rw, g


def standing(kind):
    """One machine as she stands, the deck at z = 0 under her: points, faces and what paints them."""
    if kind in _STOOD:
        return _STOOD[kind]
    pm, pf = plain(kind)
    names = list(pm.paints)
    # her own skin, without her crew, the blur of her airscrew, and her own small tailwheel (she gets one on a strut)
    blur_parts = {i for i, p in enumerate(pm.parts) if p.name.endswith(".blur")}
    aft = pf.hull.back + 1.6
    keep = [k for k in range(len(pm.faces)) if pm.fpart[k] not in blur_parts and names[pm.fpaint[k]] not in CREW | BLUR
            and not (names[pm.fpaint[k]] in ("tyre", "leg") and all(pm.verts[i].x < aft for i in pm.faces[k]))]
    used = sorted({i for k in keep for i in pm.faces[k]})
    th, leg, u, top, rw, g = stance(kind, pm, pf, [pm.verts[i] for i in used])
    tip = Matrix.Rotation(-th, 4, "Y")                   # (nose up)
    lift = Vector((0.0, 0.0, -g))

    # ---- her undercarriage, in her own axes: a leg, its door, an axle and a wheel under each wing
    gear = Model("gear", "", "plane", dict(pm.paints), True)
    gear.part("gear", None, (0, 0, 0), kind="root")
    wheels = []
    for hand in (1.0, -1.0):
        yw = hand * u
        yl = hand * (u - 0.17)
        zc = top.z - leg
        gear.rod((top.x, yl, top.z + 0.06), (top.x, yl, zc + 0.02), 0.06, "leg", rb=0.045, sides=10)
        gear.rod((top.x, yl, zc), (top.x, yw + hand * 0.02, zc), 0.035, "leg", sides=8)
        gear.rod((top.x, yw - 0.1, zc), (top.x, yw + 0.1, zc), rw, "tyre", sides=22)
        gear.rod((top.x, yw - 0.108, zc), (top.x, yw + 0.108, zc), rw * 0.45, "leg", sides=14)
        gear.box((top.x + 0.02, yl - hand * 0.08, top.z - leg * 0.42), (0.34, 0.025, leg * 0.8), "under")
        wheels.append(Vector((top.x, yw, zc)))

    # her tailwheel on its strut, as she stands (the wheel's axle is level whichever way she tips)
    tw = pf.spec.get("tailwheel") if isinstance(pf.spec, dict) else None
    xt = tw[0] if tw else pf.hull.back + 0.6
    foot = tip @ Vector((xt + 0.1, 0.0, pf.hull.at(xt + 0.1, -math.pi / 2.0).z + 0.05)) + lift
    tail = Model("tailwheel", "", "plane", dict(pm.paints), True)
    tail.part("tailwheel", None, (0, 0, 0), kind="root")
    axle = Vector((foot.x - 0.1, 0.0, TAIL_R))
    tail.rod(foot, axle + Vector((0.0, 0.0, 0.02)), 0.04, "leg", rb=0.03, sides=8)
    tail.rod(axle - Vector((0, 0.05, 0)), axle + Vector((0, 0.05, 0)), TAIL_R, "tyre", sides=16)

    verts, faces, paints, by_point, smooth, coat, blend, soot = [], [], [], [], [], [], [], []
    at = {i: n for n, i in enumerate(used)}
    for i in used:
        verts.append(tip @ pm.verts[i] + lift)
        c = pm.vcoat[i] if i < len(pm.vcoat) else -1
        coat.append(names[c] if c >= 0 else None)
        blend.append(pm.vblend[i] if i < len(pm.vblend) else 0.0)
        soot.append(pm.vsoot[i] if i < len(pm.vsoot) else 0.0)
    for k in keep:
        faces.append(tuple(at[i] for i in pm.faces[k]))
        paints.append(names[pm.fpaint[k]])
        by_point.append(bool(pm.fcoat[k]) if k < len(pm.fcoat) else False)
        smooth.append(bool(pm.fsmooth[k]))
    for piece, tipped in ((gear, True), (tail, False)):
        base = len(verts)
        gnames = list(piece.paints)
        for v in piece.verts:
            verts.append(tip @ v + lift if tipped else v.copy())
            coat.append(None); blend.append(0.0); soot.append(0.0)
        for k, f in enumerate(piece.faces):
            faces.append(tuple(base + i for i in f))
            paints.append(gnames[piece.fpaint[k]])
            by_point.append(False)
            smooth.append(bool(piece.fsmooth[k]))
    # chocks, level on the deck, before and behind each wheel
    for w in wheels:
        c = tip @ w + lift
        for way in (1.0, -1.0):
            x0 = c.x + way * (rw * 0.62)
            x1 = x0 + way * 0.26
            b = len(verts)
            for y in (c.y - 0.17, c.y + 0.17):
                verts += [Vector((x0, y, 0.0)), Vector((x1, y, 0.0)), Vector((x0, y, 0.17))]
            coat += [None] * 6; blend += [0.0] * 6; soot += [0.0] * 6
            quads = [(0, 1, 4, 3), (1, 2, 5, 4), (2, 0, 3, 5)]
            tris = [(0, 2, 1), (3, 4, 5)]
            mid = Vector(((x0 + x1) / 2.0, c.y, 0.06))
            for f in quads + tris:
                pts = [verts[b + i] for i in f]
                n = (pts[1] - pts[0]).cross(pts[2] - pts[0])
                centre = sum(pts, Vector()) / len(pts)
                if n.dot(centre - mid) < 0.0:
                    f = tuple(reversed(f))
                faces.append(tuple(b + i for i in f))
                paints.append("chock"); by_point.append(False); smooth.append(False)

    # her airscrews: the tip of each blade is a paint of its own where her nation's airscrews wear yellow tips, and
    # what turns with each is kept, so that every machine of a range can be stopped with her blades at her own angle
    owner = fitout.nation(pf.spec)
    rot = tip.to_3x3()
    screws = []
    for pi, part in enumerate(pm.parts):
        if not part.name.startswith("prop.") or part.name.endswith(".blur"):
            continue
        hub, axis = tip @ part.pivot + lift, (rot @ Vector(part.axis)).normalized()
        mine = [n for n, k in enumerate(keep) if pm.fpart[k] == pi]
        if owner in TIPPED or owner == "*":                  # (a plain machine's tips are cut too: her owner may wear them)
            mine += cut_tips(verts, faces, paints, by_point, smooth, (coat, blend, soot), [n for n in mine if paints[n] == "blade"], hub, axis, pf.screw * (1.0 - TIP))
        screws.append({"hub": hub, "axis": axis, "points": sorted({i for n in mine for i in faces[n]}), "blades": int(pf.spec.get("blades", 3)) if isinstance(pf.spec, dict) else 3})

    # her markings, as her own file gives them, on what shows from above and abeam
    marks = []
    for mk in pm.marks or []:
        if mk["tag"] in ("#", "*FIN"):                       # (her number; and a small nation's rudder flag: small nations have no carriers)
            continue
        for hand in ((1.0, -1.0) if mk.get("both") else (1.0,)):
            p = Vector((mk["at"][0], -mk["at"][1] * hand, mk["at"][2]))
            n = Vector((mk["facing"][0], -mk["facing"][1] * hand, mk["facing"][2])).normalized()
            if n.z < -0.3:
                continue                                  # (under her wing: nobody sees it on a deck)
            up = X if abs(n.z) > 0.7 else Z
            marks.append({"tag": mk["tag"], "at": tip @ p + lift, "facing": rot @ n, "up": rot @ up, "radius": mk["radius"], "curve": mk.get("curve"), "place": mk.get("place")})
    out = dict(pm=pm, pf=pf, verts=verts, faces=faces, paints=paints, by_point=by_point, smooth=smooth, coat=coat, blend=blend, soot=soot,
               marks=marks, pitch=th, leg=leg, wheels=[tip @ w + lift for w in wheels], rw=rw, screws=screws,
               tips=owner in TIPPED, plain=owner == "*")
    _STOOD[kind] = out
    return out


def cut_tips(verts, faces, paints, by_point, smooth, per_point, which, hub, axis, r_cut):
    """Cuts the faces numbered in `which` where they cross a radius from an airscrew's axis, and paints what lies
    outside it `blade_tip`. Returns the numbers of the faces it added."""
    n = len(verts)
    _, outside, born = ring_cut(verts, faces, which, hub, axis, r_cut)
    for row, empty in zip(per_point, (None, 0.0, 0.0)):
        row.extend([empty] * (len(verts) - n))
    for _, like in born:
        paints.append(paints[like]); by_point.append(by_point[like]); smooth.append(smooth[like])
    for k in outside:
        paints[k] = "blade_tip"
    return [k for k, _ in born]


def stopped(st, i):
    """The points of machine `i` of a range, her airscrew stopped at her own angle (no two alike)."""
    vs = list(st["verts"])
    for sc in st["screws"]:
        turn = Matrix.Rotation(2.0 * math.pi / sc["blades"] * ((i + 1) * 0.6180339887 % 1.0), 3, sc["axis"])
        for j in sc["points"]:
            vs[j] = turn @ (st["verts"][j] - sc["hub"]) + sc["hub"]
    return vs


def places(pf):
    """Where each machine of a range stands, from the foremost aft: (x, y) of her own middle, facing the bow.
    Two abreast with a wingtip's room between them, then one on the middle line whose airscrew is clear of their
    wings and whose wings are clear of their tails."""
    yp = pf.half + 1.1
    step = max(pf.hull.front + 0.3 - pf.wing.trail(pf.wing.root) + 0.6, 0.72 * pf.long)
    out = []
    for r, n in enumerate(ROWS):
        for y in ((yp, -yp) if n == 2 else (0.0,)):
            out.append((-r * step, y))
    return out


def stand_in(m, o, span, long):
    """A machine a long way off: a body, wings, a tail, a hood, two wheels. Enough to read as an aircraft."""
    nose, tail = o + X * (long * 0.45) + Z * 2.0, o - X * (long * 0.5) + Z * 0.75
    ax = tail - nose
    u = ax.normalized()
    m.lathe(nose, ax, [(0.0, 0.2), (0.3, 0.6), (long * 0.3, 0.66), (long * 0.62, 0.4), (ax.length, 0.07)], "upper", 6)
    root = nose + u * (long * 0.3) - Z * 0.4
    m.box(root, (long * 0.25, span, 0.15), "upper")
    m.box(tail - u * 0.55 + Z * 0.12, (long * 0.12, span * 0.34, 0.08), "upper")
    m.box(tail - u * 0.5 + Z * 0.7, (long * 0.14, 0.08, 1.15), "upper")
    m.ball(nose + u * (long * 0.34) + Z * 0.5, (1.0, 0.36, 0.34), "glass", 8, 4, top_only=True)
    for hand in (1, -1):
        w = root + Vector((0.25, hand * 1.6, 0.0))
        m.rod(w, w - Z * 1.25, 0.07, "leg", sides=4)
        m.box(w - Z * 1.3, (0.6, 0.16, 0.6), "tyre")


def park(kind, fine=True):
    """The range of one kind of machine as a model: a root (`park`, standing at the pivot of a carrier's `deckpark`)
    and a part for each machine."""
    st = standing(kind)
    pm, pf = st["pm"], st["pf"]
    paints = dict(pm.paints)
    paints["chock"] = CHOCK
    if st["tips"] or st["plain"]:
        paints["blade_tip"] = TIP_YELLOW if st["tips"] else paints["blade"]  # (yellow where her nation's airscrews wear yellow tips)
    title = pm.title.replace(" (our own model)", "")
    m = Model("DeckPark_" + kind, f"{title}s ranged on a carrier's deck (our own model)", "deck park", paints, fine)
    m.part("park", None, (0, 0, 0), kind="root", note="the whole range: it stands at the pivot of the carrier's part `deckpark` and rides with it")
    spots = places(pf)
    for i, (x, y) in enumerate(spots):
        name = f"plane.{i + 1}"
        m.part(name, "park", (x, y, 0.0), kind="fixed", note="one machine on her wheels, chocked: scale her away when she has flown off")
        o = Vector((x, y, 0.0))
        if not fine:
            m.use(name)
            stand_in(m, o, pf.half * 2.0, pf.long)
            continue
        n0 = len(m.faces)
        m.raw([v + o for v in stopped(st, i)], st["faces"], st["paints"], name, coat=st["coat"], blend=st["blend"], soot=st["soot"], by_point=st["by_point"])
        m.fsmooth[n0:] = st["smooth"]
        for mk in st["marks"]:
            m.marks.append({"tag": mk["tag"], "at": mirror(mk["at"] + o), "radius": mk["radius"], "facing": mirror(mk["facing"]), "up": mirror(mk["up"]),
                            "curve": mk["curve"], "both": False, "part": name, "place": mk.get("place")})
    src = pm.info.get("file", {})
    m.info["file"] = {"machine": kind, "pitch": round(math.degrees(st["pitch"]), 2),
                      "places": [[round(x, 2), round(-y, 2), 0.0] for x, y in spots],
                      "about_park": "A range of aircraft for a carrier's deck. Stand it at the pivot of her part `deckpark` (x forward, z up; its own "
                                    "origin is the deck under the foremost row) and let it ride with that part. `places` is where each machine "
                                    "stands, `pitch` how far her nose is up in degrees. Paint and markings by the month are the aircraft's own: "
                                    "`schemes`, `mark_months`, `nation_paint`, exactly as in her own file. The paint `blade_tip` is the tip of "
                                    "each stopped blade, yellow where her nation's airscrews wear yellow tips (Japan's do not: no such paint). On a machine painted for her owner "
                                    "(`nation_paint`) it is the blade's own black, and `blade_tip_for` says which owners' machines wear it "
                                    "yellow."}
    if src.get("nation_paint"):
        m.info["file"]["blade_tip_for"] = {"colour": TIP_YELLOW, "owners": list(TIPPED)}
    for k in ("nation_paint", "number_ink", "clear", "schemes", "mark_months", "year", "finish"):
        if k in src:
            m.info["file"][k] = src[k]
    return m, None


def in_month(m, month):
    """Paints a range as it is in a month (YYYY-MM): the coat and the markings of that month. Returns whether the
    coat is the one gloss one."""
    gloss = False
    if month:
        coats = [c for c in m.info["file"].get("schemes", []) if c["from"] <= month]
        if coats:
            m.paints.update(coats[-1]["colours"])
            gloss = coats[-1].get("finish") == "gloss"
        for mk in m.marks or []:
            mk["tag"] = planes.mark_in(mk["tag"], month, mk.get("place"))
    return gloss


def of_carrier(ship, month=None):
    """Which range a carrier wears in a month (with no month: her navy's own machine, the last on her list)."""
    plan = ship.info.get("file", {}).get("deck_park") or []
    if not plan:
        return None
    if month:
        fit = [e for e in plan if "from" not in e or e["from"] <= month]
        return (fit[-1] if fit else plan[0])["model"]
    return plan[-1]["model"]


def own_for(ship, month=None):
    """For the pictures: the marking a plain machine wears on this carrier's deck where her file says the owner's
    own. America's star of the month on the American carrier; Britain's roundels on the plain one, which is hers."""
    if ship.name == "Carrier_USA":
        return planes.mark_in("USA", month or "1942-06")
    return lambda n: "UK-B" if abs(n.z) > 0.7 else "UK"


def range_on(ship, ship_arm, month=None, own=None):
    """For the pictures only: stands a carrier's range on her deck, riding her part `deckpark`. `own` is the marking
    a plain machine wears where her file says the owner's own (a function of the facing of the mark, or a tag).
    Returns the objects made."""
    import bpy

    import airshow
    import marks
    import rigid
    name = of_carrier(ship, month)
    if not name or "deckpark" not in ship.part_at:
        return []
    m, _ = park(name[len("DeckPark_"):], True)
    gloss = in_month(m, month)
    if ship.name == "Carrier_USA" and m.info["file"]["machine"] == "CarrierFighter":
        m.paints.update(planes._BLUE_GREY)               # (the game paints a plain machine for her owner: America's blue-grey of 1941 and 1942)
    if m.info["file"].get("blade_tip_for") and ship.name in ("Carrier_USA", "Carrier"):
        m.paints["blade_tip"] = TIP_YELLOW               # (America's and Britain's airscrews wear yellow tips)
    ob, arm = rigid.to_blender(m, (lambda *a: airshow.plane_look(*a, gloss=gloss)), crease=50.0)
    made = [ob, arm] + marks.put(m, ob, arm, own=own or own_for(ship, month))
    bone = ship_arm.data.bones["deckpark"]
    arm.parent = ship_arm
    arm.parent_type = "BONE"
    arm.parent_bone = "deckpark"
    arm.location = (0.0, -bone.length, 0.0)              # (a bone's children hang from its far end: back to its pivot)
    bpy.context.view_layer.update()
    return made


BUILDERS = {"DeckPark_" + k: (lambda fine=True, k=k: park(k, fine)) for k in MACHINES}
