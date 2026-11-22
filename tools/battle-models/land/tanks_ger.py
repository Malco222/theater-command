"""Germany's tanks: Panzer IV (the line tank), Panther (from 1944), Tiger (the heavy unit).

Each is modelled to the real type's measurements and layout from the published dimensions; every shape is our own
mesh, made from numbers here. Markings are the plain beam cross and turret numbers, nothing else.
"""

import math

from mathutils import Vector

from motorpool import LEFT, RIGHT, SIDES, X, Y, Z, bm_box, inset_plan, livery, outline, rot, round_plan, sym_plan
from works import Works, dirt_for


def coat(mid, n):
    """The coat, and its shadowed underside."""
    return "shade" if n.z < -0.75 else "upper"


def plane_of(p0, p1, p2, toward):
    """The outward facing of the plane through three points (`toward` any point on its outer side)."""
    p0, p1, p2 = Vector(p0), Vector(p1), Vector(p2)
    n = (p1 - p0).cross(p2 - p0).normalized()
    return n if n.dot(Vector(toward) - p0) > 0 else -n


# The coats a German tank wore, and when (the art director's ruling: dark grey until early 1943, dark yellow after).
GER_COATS = {"grey": ("4E5459", "1939 to early 1943", "dark grey"), "yellow": ("8E7E58", "1943 on", "dark yellow"),
             "camo": ("8E7E58", "1943 on", "dark yellow with olive green and red-brown sprayed over it in the field")}
GER_GREEN = "4E5638"          # the olive green and the red-brown pastes the units thinned and sprayed over the yellow (the art director's
GER_BROWN = "5E3E2C"          # starting points of 22 Nov against her dark yellow: the two within 10 of each other in lightness)
# From February 1943 the yellow was the base and the units sprayed green (and brown) over it as they could: by the
# summer of 1943 most had, by 1944 nearly all (our own shares, low on the numbers).
YELLOW_EARLY = {"mix": {"yellow": 0.6, "camo": 0.4}, "one": "yellow"}
YELLOW_LATE = {"mix": {"camo": 0.8, "yellow": 0.2}, "one": "camo"}


def sprayed(rows):
    """A by_month table's rows for a vehicle that carries the sprayed blotches: where a row says yellow, the share that was sprayed."""
    out = []
    for row in rows:
        row = dict(row)
        if row.get("paint") == "yellow":
            row["paint"] = YELLOW_EARLY if row.get("to", "9999") <= "1943-05" else YELLOW_LATE
        out.append(row)
    return out


def spray(v, paint, **kw):
    """The second and third colours' blotches on a German vehicle (camo.py), broad and soft-edged as a spray gun in
    the field leaves them: patches half a metre to a metre and a half across, their edges blurring over ten
    centimetres, the yellow keeping about half of her. The file carries them whatever coat she was built in: the
    `camo` coat fills them with the green and the red-brown, and the one-colour coats fill `upper2` and `upper3`
    with `upper` so that they do not show. The last thing a builder adds."""
    v.own_paint("upper3", v.m.paints["upper"])
    kw = {"cell": 1.0, "cover": (0.40, 0.36), "three": True, **({"soft": 0.05, "grain": 0.3} if v.fine else {"grain": 0.9}), **kw}
    done = v.camouflage(**kw)
    v.m.paints["upper2"], v.m.paints["upper3"] = (GER_GREEN, GER_BROWN) if paint == "camo" else (v.m.paints["upper"],) * 2
    return done


def coats(v, default="grey", only=None):
    """The paint swap as the file tells it: for each coat, the three colours to put in place of upper, upper2, shade.
    `only`: the coats she ever wore (a Panther came after the grey and never wore it)."""
    out = {}
    for key, (hx, years, words) in GER_COATS.items():
        if only and key not in only:
            continue
        p = livery("GER", hx)
        names = ("upper", "upper2", "shade")
        if getattr(v.m, "camo", None):                  # (she carries the blotches: the green and the brown fill them, or her own coat hides them)
            p["upper2"], p["upper3"] = (GER_GREEN, GER_BROWN) if key == "camo" else (p["upper"], p["upper"])
            names = ("upper", "upper2", "upper3", "shade")
        elif key == "camo":
            continue
        out[key] = {"what": words, "years": years, "paints": {k: p[k] for k in names}}
        if "dust_colour" in v.m.info.get("file", {}):   # (the dirt that shows on this coat: works.dirt_for)
            out[key]["dust_colour"], out[key]["haze_colour"] = dirt_for(p["upper"])
    return {"default": default, "choices": out}


# How a file's `swaps.by_month` is read (the Panzer IV's and the StuG III's alike).
BY_MONTH_HOW = ("by_month: the face she wore in each month of a campaign, one row per run of months (from and to are both counted in), naming "
                "the choice of each swap. Where a row gives a `mix`, both served side by side: each vehicle of a unit draws its own choice by "
                "those shares, and a game that shows one face a unit takes `one`.")

# Zimmerit: the factories put the ribbed paste on new tanks from September 1943 to September 1944, and a tank wore
# hers until she was lost, so through the winter a unit had both and by the summer of 1944 nearly all wore it (our
# own shares, low on the numbers; the months are the historian's of 18 Nov).
ZIMMERIT_MONTHS = [{"from": "1943-09", "to": "1943-12", "zimmerit": {"mix": {"off": 0.7, "on": 0.3}, "one": "off"}},
                   {"from": "1944-01", "to": "1944-05", "zimmerit": {"mix": {"on": 0.6, "off": 0.4}, "one": "on"}},
                   {"from": "1944-06", "to": "1944-12", "zimmerit": {"mix": {"on": 0.8, "off": 0.2}, "one": "on"}},
                   {"from": "1945-01", "zimmerit": {"mix": {"on": 0.5, "off": 0.5}, "one": "on"}}]
ZIMMERIT_HOW = ("zimmerit: the ribbed paste on her upright plates, as real ribs (no texture is needed): show the choice's parts, "
                "or scale them to nothing for a tank without it. Near mesh only: the far mesh has none.")


def zimmerit_swap(parts):
    return {"default": "off", "choices": {
        "off": {"what": "bare plate", "years": "to August 1943, and on tanks built after September 1944", "parts": []},
        "on": {"what": "ribbed paste on her upright plates", "years": "tanks built 1943-09 to 1944-09", "parts": parts}}}


# The Panzer IV's faces by the month (the historian, 18 Nov: the two guns served side by side through 1942, about a
# third of them long by the middle of the year and most by its end; medium-low on the shares).
PZ4_MONTHS = [{"to": "1942-03", "gun": "short", "paint": "grey", "skirts": "off", "zimmerit": "off"},
              {"from": "1942-04", "to": "1942-08", "gun": {"mix": {"short": 0.67, "long": 0.33}, "one": "long"}, "paint": "grey", "skirts": "off", "zimmerit": "off"},
              {"from": "1942-09", "to": "1943-01", "gun": {"mix": {"long": 0.67, "short": 0.33}, "one": "long"}, "paint": "grey", "skirts": "off", "zimmerit": "off"},
              {"from": "1943-02", "to": "1943-04", "gun": "long", "paint": "yellow", "skirts": "off", "zimmerit": "off"},
              {"from": "1943-05", "to": "1943-08", "gun": "long", "paint": "yellow", "skirts": "on", "zimmerit": "off"}] + [
                  {**row, "gun": "long", "paint": "yellow", "skirts": "on"} for row in ZIMMERIT_MONTHS]

# The Tiger's: grey to early 1943, dark yellow after, the paste as on the others.
TIGER_MONTHS = [{"to": "1943-01", "paint": "grey", "zimmerit": "off"}, {"from": "1943-02", "to": "1943-08", "paint": "yellow", "zimmerit": "off"}] + [
    {**row, "paint": "yellow"} for row in ZIMMERIT_MONTHS]


def hull_skirts(v, lost, xa, xb, n, ys, zr, zbot, guard, side_y=None, straps=None, mark_z=1.22, under=False, bent=None):
    """Skirts along a hull: n thin plates a side hung on a rail that stands off her side, over the upper run of the
    tracks. Every plate is a part of its own, since they were lost: `lost` names the ones this vehicle has lost
    ("l2", "r4"). xa, xb: where the rail runs; ys, zr: how far out it stands and how high; zbot: the plates' feet;
    guard: (y, z) of the track guard's edge its stays stand on; side_y: her side, where a strap reaches the rail
    from it (a number, or a function of x that gives None where no side is in reach: there a brace stands on the
    guard instead); straps: where along her they are; mark_z: how high her cross sits on the middle plate (None:
    the plates carry none, her own shows over them); under: the plates hang under the rail by straps hooked over it,
    so the rail shows along their tops (else their tops stand proud of it and hide it); bent: plates with a lower
    corner bent out, {"r3": ("fore", 0.11)}: which end, and how far out the corner's tip stands (metres).
    The pictures splash their lower third with the ground's mud (works.land_look reads the model's `splash`).
    Returns the names of all the skirts' parts, and of the plates among them."""
    fine = v.fine
    parts, plates = [], []
    step = (xb - xa) / n
    straps = straps or [xa + 0.04] + [xa + step * k for k in range(1, n)] + [xb - 0.04]
    for letter, s in SIDES:
        rail = f"skirt_rail_{letter}"
        parts.append(rail)
        v.part(rail, "hull", (0.0, s * ys, zr), kind="fixed", note="the rail her hull skirts hang on, on its brackets (shown with the skirts)")
        v.slab((xa - 0.02, s * (ys - 0.04), zr - 0.006), (xb + 0.02, s * ys, zr), coat, 0.002)                 # an angle iron: its flat,
        v.slab((xa - 0.02, s * (ys - 0.006), zr - 0.05), (xb + 0.02, s * ys, zr), coat, 0.002)                 # and its web
        for x in straps:
            sy = side_y(x) if callable(side_y) else side_y
            if sy is not None:
                v.slab((x - 0.022, s * sy, zr - 0.034), (x + 0.022, s * (ys - 0.03), zr - 0.006), coat, 0.002)     # a strap out from her side
            v.rod((x, s * (ys - 0.02), zr - 0.03), (x, s * guard[0], guard[1]), 0.012, coat, 6)                    # and a stay down to the track guard
            if sy is None:
                v.rod((x, s * (ys - 0.02), zr - 0.03), (x, s * (guard[0] - 0.14), guard[1]), 0.012, coat, 6)       # (a brace beside it where no side is in reach)
            elif fine:
                v.bolt((x, s * (sy + 0.03), zr - 0.006), Z, 0.009, 0.006)
        for k in range(n):
            name = f"skirt_{letter}{k + 1}"
            parts.append(name)
            plates.append(name)
            if f"{letter}{k + 1}" in lost:
                continue
            x1, x2 = xa + step * k + 0.008, xa + step * (k + 1) - 0.008
            ztop = zr - 0.045 if under else zr + 0.025
            v.part(name, "hull", ((x1 + x2) / 2, s * ys, zr), kind="fixed", note="a skirt plate hung on the rail: they get lost (the game hides none, one or two a vehicle)")
            # hung by hand and knocked about: each plate a degree or two off true, one end lower, its foot swung out or in
            tilt, swing = math.radians(1.7) * v.rng.uniform(-1.0, 1.0), math.radians(2.0) * v.rng.uniform(-1.0, 1.0)
            yo, long, high = s * (ys + 0.014), x2 - x1, ztop - zbot
            drop = long / 2 * math.tan(tilt)

            def pt(u, h):                                  # a point of the plate: u along it from its after end, h up from its foot
                return Vector((x1 + u * long, yo + high * math.tan(swing) * (1.0 - h / high), zbot + h + drop * (2.0 * u - 1.0)))
            cs = [pt(0, 0), pt(0, high), pt(1, high), pt(1, 0)]
            if k == n - 1:                                 # the foremost plate's lower corner is cut away
                cs = [pt(0, 0), pt(0, high), pt(1, high), pt(1, 0.42), pt(1 - 0.3 / long, 0)]
            if k == 0:                                     # and the aftermost's
                cs = [pt(0.26 / long, 0), pt(0, 0.36), pt(0, high), pt(1, high), pt(1, 0)]
            fold = (bent or {}).get(f"{letter}{k + 1}") if 0 < k < n - 1 else None
            if fold:                                       # a lower corner caught on something and bent out: the plate folds along a line across the corner
                cu, ch = 0.30 / long, 0.27
                if fold[0] == "fore":
                    a, b, tip = pt(1 - cu, 0), pt(1, ch), pt(1, 0)
                    cs = [pt(0, 0), pt(0, high), pt(1, high), b, a]
                else:
                    a, b, tip = pt(0, ch), pt(cu, 0), pt(0, 0)
                    cs = [b, a, pt(0, high), pt(1, high), pt(1, 0)]
                foot = a + (b - a) * ((tip - a).dot(b - a) / (b - a).length_squared)
                d = (tip - foot).length
                tip = foot + (tip - foot) / d * math.sqrt(d * d - fold[1] ** 2) + Vector((0.0, s * fold[1], 0.0))
                v.plate([tuple(c) for c in (b, tip, a)][::int(s)], 0.006, coat, 0.002)
            v.plate([tuple(c) for c in cs][::int(s)], 0.006, coat, 0.002)
            for x in (x1 + 0.16, x2 - 0.16):               # the two hooks it hangs by
                if under:                                  # (a strap riveted to the plate's face, up past its top and hooked over the rail: in her own paint)
                    v.slab((x - 0.026, s * (ys - 0.002), ztop - 0.075), (x + 0.026, s * (ys + 0.024), zr + 0.004), coat, 0.003)
                    v.slab((x - 0.026, s * (ys - 0.018), zr), (x + 0.026, s * (ys + 0.024), zr + 0.009), coat, 0.002)
                    if fine:
                        for dz in (0.02, 0.055):
                            v.bolt((x, s * (ys + 0.024), ztop - dz), Y * s, 0.006, 0.004)
                    continue
                v.slab((x - 0.03, s * (ys - 0.012), zr - 0.07), (x + 0.03, s * (ys + 0.022), zr + 0.012), "steel", 0.003)
            if k == n // 2 and mark_z is not None:         # her cross, on the plate's own face
                face = (pt(0, high) - pt(0, 0)).cross(pt(1, 0) - pt(0, 0)).normalized()
                face = face if face.y * s > 0 else -face
                v.mark("GER", tuple(pt(0.5, mark_z - zbot) + face * 0.0015), face, 0.2, part=name)
    v.m.splash = list(getattr(v.m, "splash", [])) + [(ys - 0.03, zbot, zbot + 0.45 * (zr - zbot))]
    return parts, plates


PZ4_SHIELD = [(0.80, 1.02), (0.34, 1.17), (-0.52, 1.15), (-1.20, 0.98), (-1.68, 0.55)]     # the turret shield's plan, a side (from the turret's middle)


def pz4_skirts(v, x0, zt, lost=()):
    """The Panzer IV's skirts of May 1943 on: five thin plates a side hung on a rail a hand's width off her hull,
    over the upper run of the tracks, and a shield round her turret's sides and back with a door each side. Every
    hull plate is a part of its own, since tanks lost them: `lost` names the ones this tank has lost ("l2", "r4").
    Returns the names of all the skirts' parts, and of the hull plates among them."""
    fine = v.fine
    xa, xb, n = -2.56, 1.94, 5
    step = (xb - xa) / n
    parts, plates = hull_skirts(v, lost, xa, xb, n, 1.60, 1.60, 0.52, (1.43, 1.105), side_y=1.31,
                                straps=(xa + 0.04, xa + step, xa + 2 * step, xa + 3 * step, xa + 4 * step, 1.66))
    # ---- the turret's shield: a horseshoe of plate round her sides and back, standing off on stays ---------------
    z0, z1 = zt + 0.08, zt + 0.58
    side = PZ4_SHIELD
    v.part("skirt_turret", "turret", (x0 - 0.4, 0.0, z0), kind="fixed", note="the shield round her turret's sides and back (shown with the skirts; it always stays)")
    parts.append("skirt_turret")

    def quad(p, q, s, cut=0.0):
        a, b = (x0 + p[0], s * p[1]), (x0 + q[0], s * q[1])
        cs = [(a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1 - cut)]
        return cs[::int(s)]
    for letter, s in SIDES:
        v.use("skirt_turret")
        v.plate(quad(side[0], side[1], s, cut=0.18), 0.008, coat, 0.002)
        v.plate(quad(side[2], side[3], s), 0.008, coat, 0.002)
        v.plate(quad(side[3], side[4], s), 0.008, coat, 0.002)
        for a, b in (((0.60, 0.56, 2.22), (0.56, 1.09, 2.2)), ((-0.66, 0.62, 2.28), (-0.80, 1.07, 2.26)), ((-1.0, 0.36, 2.285), (-1.70, 0.36, 2.26)),
                     ((0.5, 0.80, 1.86), (0.55, 1.09, 1.86)), ((-0.74, 0.85, 1.86), (-0.80, 1.07, 1.86))):
            v.rod((x0 + a[0], s * a[1], a[2]), (x0 + b[0], s * b[1], b[2]), 0.013, coat, 6)
        # the door: across from her own side doors, hung on its fore edge, opening outward
        name = f"skirt_door_{letter}"
        parts.append(name)
        hinge = Vector((x0 + side[1][0], s * side[1][1], (z0 + z1) / 2))
        v.part(name, "skirt_turret", hinge, axis=(0, 0, -s), limits=[0.0, 100.0], kind="turn", note="the door in her turret's shield: + opens it outward (thrown open when her men bail out)")
        v.plate(quad((side[1][0] - 0.006, side[1][1]), (side[2][0] + 0.006, side[2][1]), s), 0.008, coat, 0.002)
        xm, ym = x0 + (side[1][0] + side[2][0]) / 2, s * ((side[1][1] + side[2][1]) / 2 + 0.001)
        v.slab((xm - 0.004, ym - 0.002, z0 + 0.01), (xm + 0.004, ym + 0.003, z1 - 0.01), "shade")                   # it is in two leaves
        if fine:
            for z in (z0 + 0.1, z1 - 0.1):
                v.hinge((hinge.x, hinge.y + s * 0.006, z - 0.04), (hinge.x, hinge.y + s * 0.006, z + 0.04), 0.011)
            v.handle((xm - 0.16, ym + s * 0.001, z0 + 0.25), (xm - 0.06, ym + s * 0.001, z0 + 0.25), Y * s, 0.007, 0.03, "steel")
    v.use("skirt_turret")
    p, q = (x0 + side[4][0], side[4][1]), (x0 + side[4][0], -side[4][1])
    v.plate([(p[0], p[1], z0), (q[0], q[1], z0), (q[0], q[1], z1), (p[0], p[1], z1)], 0.008, coat, 0.002)      # across her back, behind the bin
    return parts, plates


def pz4_gear(v, xs=None, idler=(-2.52, 0.64), rollers=None, dust_x=-2.75):
    """The Panzer IV's running gear: eight small road wheels a side in four leaf-sprung pairs, four return rollers,
    the drive at the front. xs: where the wheels stand (a longer hull spreads the pairs); idler: (x, z); rollers:
    (x, z) each."""
    fine = v.fine
    ty = 1.225
    xs = xs or [-1.76 + 0.503 * i for i in range(8)]
    r_w, r_s, r_i, r_r = 0.235, 0.335, 0.31, 0.125
    spr, idl = (2.50, 0.70), idler
    rollers = rollers or [(-1.50, 0.905), (-0.50, 0.905), (0.50, 0.905), (1.50, 0.905)]
    for letter, s in SIDES:
        v.use("vehicle")
        for i, x in enumerate(xs):
            v.road_wheel(f"wheel_{letter}{i + 1}", "vehicle", x, s * ty, r_w, r_w, s, w=0.085, tyre=0.032, gap=0.075, bolts=6, sides=30)
        v.sprocket(f"sprocket_{letter}", "vehicle", spr[0], s * ty, spr[1], r_s, s, teeth=18, wide=0.24, tooth=0.03, holes=6, sides=30)
        v.idler(f"idler_{letter}", "vehicle", idl[0], s * ty, idl[1], r_i, s, wide=0.24, kind="spoke", spokes=7)
        for i, (x, z) in enumerate(rollers):
            v.roller(f"roller_{letter}{i + 1}", "vehicle", x, s * ty, z, r_r, s, wide=0.17, tyre=0.02, reach=0.26)
        wheels = [(x, r_w, r_w) for x in xs] + [(spr[0], spr[1], r_s)] + [(x, z, r_r) for x, z in reversed(rollers)] + [(idl[0], idl[1], r_i)]
        v.track(letter, s, s * ty, wheels, pitch=0.12, wide=0.40, thick=0.028, style={"kind": "plate", "horn": "centre", "horn_high": 0.075, "grouser": 0.014})
        # the bogies: a leaf spring under each pair of wheels, on the root so they stay with the wheels
        v.use("vehicle")
        for k in range(4):
            xa, xb = xs[2 * k], xs[2 * k + 1]
            xm = (xa + xb) / 2
            yi = s * (ty - 0.30)
            v.slab((xm - 0.11, s * 0.93, 0.36), (xm + 0.11, s * (ty - 0.2), 0.58), "shade", 0.01)
            if fine:
                for j in range(4):                       # the leaves
                    half = 0.27 - j * 0.045
                    v.slab((xm - half, yi - 0.04, 0.30 - j * 0.02), (xm + half, yi + 0.04, 0.315 - j * 0.02), "shade")
                for xw in (xa, xb):                      # the swing arms
                    v.rod((xm + (0.06 if xw > xm else -0.06), yi, 0.46), (xw, yi, r_w), 0.035, "shade", 8)
                v.bolts((xm, s * (ty - 0.2) + s * 0.001, 0.47), Y * s, 0.075, 6, r=0.012)
        # final drive housing at the sprocket
        v.use("hull")
        v.lathe((spr[0], s * 0.90, spr[1]), Y * s, [(0.22, 0.0), (0.22, 0.13), (0.16, 0.2), (0.0, 0.24), (0.0, 0.0)], "upper", v.n(22, 8))
        v.socket("dust." + ("left" if s > 0 else "right"), (dust_x, s * ty, 0.05), (-1, 0, 0.3), part="vehicle")


def panzer4(fine=True, tag="GER", gun="long", paint=None, skirts=None, zimmerit=False):
    """Panzer IV: 5.92 m hull, 2.88 m wide, 2.68 m high; eight small road wheels a side on leaf-sprung bogies, four
    return rollers, drive at the front. She served the whole war with two guns and two coats, and this one model is
    both: gun="long" is the 7.5 cm L/48 with its double-baffle muzzle brake (1942 on), "short" the 7.5 cm L/24 with
    none (1939 to early 1942), "all" builds both barrels for the game's file, which shows one (its `swaps`);
    paint "grey" (to early 1943) or "yellow" (1943 on); skirts (May 1943 on, with the yellow coat): True hangs
    them all, a string names the hull plates this tank has lost ("l2,r4")."""
    v = Works("PanzerIV", "Panzer IV", "tank", tag, fine, paint=GER_COATS[paint][0] if paint else None)
    v.root(hull_pivot=(0.0, 0.0, 0.75))
    m = v.m

    # ---- the hull: a tub between the tracks, a superstructure over them -----------------------------
    hw = 0.925
    tub = [(-2.62, 0.44), (-2.74, 0.66), (-2.74, 1.10), (1.72, 1.10), (2.52, 1.00), (2.62, 0.64), (2.25, 0.40), (-2.40, 0.40)]
    v.hull([(x, hw, z) for x, z in tub], coat, bevel=0.014)
    sw = 1.31
    v.hull([(1.67, sw, 1.72), (1.74, sw, 1.10), (-0.95, sw, 1.72), (-2.72, sw, 1.60), (-2.72, sw, 1.10)], coat, bevel=0.014)
    # track guards: one long plate a side, turned down at each end
    for s in (LEFT, RIGHT):
        y0, y1 = s * 0.93, s * 1.44
        v.slab((-2.80, y0, 1.085), (2.62, y1, 1.10), coat, 0.004)
        v.plate([(2.62, y0, 1.10), (2.62, y1, 1.10), (2.94, y1, 0.90), (2.94, y0, 0.90)][::int(s)], 0.012, "upper", 0.003)
        v.plate([(-2.80, y1, 1.10), (-2.80, y0, 1.10), (-3.00, y0, 0.84), (-3.00, y1, 0.84)][::int(s)], 0.012, "upper", 0.003)
        if fine:
            for x in (-2.2, -1.2, 0.0, 1.0, 2.0):          # the brackets under them
                v.slab((x - 0.015, s * 0.93, 0.98), (x + 0.015, s * 1.40, 1.085), "shade")
            # the lip along the outer edge
            v.slab((-2.80, s * 1.425, 1.06), (2.62, s * 1.44, 1.10), "upper")
    # the nose: bolted brake and gearbox hatches on the glacis, spare links across the lower plate
    gl = Vector((2.52 - 1.72, 0, 1.00 - 1.10)).normalized()
    gn = Vector((-gl.z, 0, gl.x))
    for y in (-0.52, 0.52):
        c = Vector((2.12, y, 1.10 + (2.12 - 1.72) * (1.00 - 1.10) / (2.52 - 1.72)))
        v.put(bm_box((0.5, 0.44, 0.022), c + gn * 0.011, rot(0, math.degrees(math.atan2(gl.z, gl.x)))), "upper", 0.006)
        if fine:
            v.bolts(c + gn * 0.022, gn, 0.0, 1, r=0.012)
            for dx in (-0.2, 0.2):
                for dy in (-0.17, 0.17):
                    v.bolt(c + gl * dx + Y * dy + gn * 0.022, gn, 0.012)
            v.vent(c + gl * 0.02 + gn * 0.02, 0.07, 0.035)
    c = Vector((2.18, 0.0, 1.10 + (2.18 - 1.72) * (1.00 - 1.10) / (2.52 - 1.72)))
    v.put(bm_box((0.42, 0.5, 0.02), c + gn * 0.01, rot(0, math.degrees(math.atan2(gl.z, gl.x)))), "upper", 0.006)
    if fine:
        for dx in (-0.17, 0.17):
            for dy in (-0.2, 0.0, 0.2):
                v.bolt(c + gl * dx + Y * dy + gn * 0.02, gn, 0.012)
        v.handle(c + Y * -0.08 + gn * 0.02, c + Y * 0.08 + gn * 0.02, gn, 0.007, 0.03, "steel")
    # the nose plate stands nearly upright from (2.62, 0.64) to (2.52, 1.00): a run of spare links across it
    nf = Vector((0.36, 0, 0.10)).normalized()
    v.spare_links(Vector((2.57, -0.78, 0.82)) + nf * 0.002, Vector((2.57, 0.78, 0.82)) + nf * 0.002, 13, 0.12, 0.34, nf,
                  style={"kind": "plate", "grouser": 0.014})
    for y in (-0.62, 0.62):
        v.hook((2.50, y, 0.50), Vector((0.8, 0, -0.5)).normalized(), 0.085)
        v.hook((-2.72, y, 0.62), -X, 0.085)
    # the driver's plate: his visor on the left, the hull machine gun in its ball on the right
    v.visor((1.705, 0.55, 1.47), Vector((1.0, 0, 0.11)).normalized(), 0.36, 0.13, 0.07)
    v.mg_ball((1.71, -0.55, 1.43), Vector((1.0, 0, 0.05)).normalized(), 0.115, socket="hull_mg")
    if fine:
        v.port((1.2, sw + 0.002, 1.48), Y * 1.0, 0.05)
        v.port((1.2, -sw - 0.002, 1.48), Y * -1.0, 0.05)
        # weld lines where the plates meet
        for s in (LEFT, RIGHT):
            v.weld((1.70, s * 0.3, 1.105), (1.70, s * 1.28, 1.105), X)
    # hatches for the driver and the radio operator
    v.hatch_rect("hatch_driver", "hull", (1.22, 0.62, 1.72), (0.46, 0.50), Z, X, hinge="front", opens=105.0)
    v.hatch_rect("hatch_radio", "hull", (1.22, -0.62, 1.72), (0.46, 0.50), Z, X, hinge="front", opens=105.0)
    v.use("hull")
    # the engine deck: two big doors with grilles, and the air louvres on each side
    dz = (1.60 - 1.72) / (-2.72 + 0.95)
    dn = Vector((-dz, 0, 1)).normalized()
    da = Vector((1, 0, dz)).normalized()
    for y in (-0.56, 0.56):
        c = Vector((-1.85, y, 1.72 + (-1.85 + 0.95) * dz))
        v.put(bm_box((1.25, 0.9, 0.02), c + dn * 0.01, rot(0, math.degrees(math.atan2(da.z, da.x)))), "upper", 0.006)
        v.grille(c + da * 0.18 + dn * 0.02, (0.62, 0.6), dn, Y, slats=9, deep=0.022)
        if fine:
            v.handle(c + da * -0.45 + Y * -0.1 + dn * 0.02, c + da * -0.45 + Y * 0.1 + dn * 0.02, dn, 0.007, 0.035, "steel")
            for hy in (-0.3, 0.3):
                v.hinge(c + da * 0.62 + Y * (hy - 0.05) + dn * 0.02, c + da * 0.62 + Y * (hy + 0.05) + dn * 0.02, 0.013)
    for s in (LEFT, RIGHT):
        v.slab((-2.62, s * sw, 1.16), (-1.25, s * (sw + 0.09), 1.52), "upper", 0.012)
        v.louvres((-1.93, s * (sw + 0.091), 1.34), (1.2, 0.28), Y * s, 5)
    # the rear plate: the main silencer across it, the small one for the traverse engine beside it
    v.silencer((-2.92, -0.62, 0.93), (-2.92, 0.46, 0.93), 0.15, pipe=[(-2.74, 0.2, 1.02), (-2.86, 0.2, 1.0), (-2.92, 0.2, 0.96)], tail=((-2.92, -0.62, 0.93), (-0.3, -1, -0.2)))
    v.silencer((-2.88, 0.56, 0.95), (-2.88, 0.86, 0.95), 0.085, bands=1, tail=((-2.88, 0.86, 0.95), (-0.5, 1, -0.3)))
    v.socket("exhaust.1", (-2.98, -0.78, 0.9), (-0.3, -1, -0.2))
    v.socket("exhaust.2", (-2.94, 0.98, 0.92), (-0.5, 1, -0.3))
    if fine:
        for y in (-0.5, 0.0, 0.34):
            v.slab((-2.80, y - 0.02, 0.80), (-2.74, y + 0.02, 1.06), "upper")
        v.hook((-2.74, 0.0, 0.56), -X, 0.1)
    # lamps, tools and stowage
    v.headlamp((2.38, 1.14, 1.10), X, 0.075, stalk=0.09)
    v.headlamp((2.38, -1.14, 1.10), X, 0.06, stalk=0.07, cover=True)
    v.jack((-0.75, -1.22, 1.10), 0.0)
    v.jack_block((-2.25, 1.20, 1.10), (0.4, 0.24, 0.13))
    v.shovel((0.15, 1.36, 1.125), 0.0, 0.95)
    v.axe((-1.0, -1.38, 1.125), 180.0, 0.75, roll=90.0)
    v.crowbar((0.05, -1.38, 1.118), 0.0, 1.35)
    v.cable([(1.55, 1.2, 1.13), (0.9, 1.26, 1.125), (-0.6, 1.26, 1.125), (-1.1, 1.22, 1.125), (-1.25, 1.12, 1.125), (-1.1, 1.04, 1.125), (0.9, 1.04, 1.125), (1.5, 1.08, 1.13)], 0.013)
    if fine:
        # two spare road wheels in their rack on the left
        for x in (-1.72, -1.2):
            v.wheel_disc((x, 1.39, 1.36), Y, 0.235, 0.085, 0.032, bolts=6, sides=24)
        v.slab((-2.0, 1.33, 1.10), (-0.92, 1.345, 1.42), "upper")
        v.box((0.5, 0.2, 0.17), (1.55, -1.2, 1.185), "upper", 0.01)        # the track tool box
        v.rod((0.75, -1.2, 1.17), (1.05, -1.2, 1.17), 0.06, "red", 12)     # the fire extinguisher
    v.aerial("aerial", "hull", (-2.35, 1.22, 1.60), 2.0)

    # ---- the running gear ----------------------------------------------------------------------------
    pz4_gear(v)

    # ---- the turret -----------------------------------------------------------------------------------
    x0, zt = 0.18, 1.72
    v.use("hull")
    v.ring_well((x0, 0.0, zt), 0.80)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    base = [(1.02, 0.60), (0.22, 0.93), (-0.72, 0.86), (-1.06, 0.50)]
    roof = [(0.92, 0.48, 2.21), (0.24, 0.65, 2.27), (-0.62, 0.61, 2.29), (-0.93, 0.40, 2.29)]
    v.hull([(x0 + x, y, zt + 0.02) for x, y in base] + [(x0 + x, y, z) for x, y, z in roof], coat, bevel=0.016)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.82), (0.0, 0.84), (0.0, 0.0)], "shade", v.n(36, 12))        # the ring's collar
    # the bin on the turret's back
    v.bin((x0 - 1.52, -0.46, 1.86), (x0 - 1.06, 0.46, 2.22), bevel=0.03, lid_side=(-1, 0, 0), latches=3)
    # side doors: two leaves each side, the fore leaf a part that opens
    for letter, s in SIDES:
        p0 = Vector((x0 + 0.22, s * 0.93, zt + 0.02))
        p1 = Vector((x0 - 0.72, s * 0.86, zt + 0.02))
        p2 = Vector((x0 + 0.24, s * 0.65, 2.27))
        p3 = Vector((x0 - 0.62, s * 0.61, 2.29))
        n = plane_of(p0, p1, p2, p0 + Y * s)
        cc = (p0 + p1 + p2 + p3) / 4 + n * 0.004
        along = (p0 - p1).normalized()
        upw = n.cross(along) * s
        for k, nm in ((0.21, "hatch_gunner" if s > 0 else "hatch_loader"), (-0.19, None)):
            c = cc + along * k
            if nm:
                v.hatch_rect(nm, "turret", c, (0.36, 0.40), n, along, hinge="front", opens=110.0, thick=0.022, well=True)
                v.use(nm)
                v.port(c + along * -0.02 + n * 0.022, n, 0.04)
                v.use("turret")
            else:
                v.hatch_rect("door_" + letter, "turret", c, (0.36, 0.40), n, along, hinge="back", opens=110.0, thick=0.022, well=True)
                v.use("turret")
        # vision port on the fore side plate, lifting eye above
        q0, q1 = Vector((x0 + 1.02, s * 0.60, zt + 0.02)), Vector((x0 + 0.92, s * 0.48, 2.21))
        nf2 = plane_of(q0, p0, q1, q0 + Y * s + X)
        v.port((q0 + p0 + q1 + p2) / 4 + nf2 * 0.004, nf2, 0.05)
        v.lift_eye((x0 + 0.62, s * 0.5, 2.235), Z, X, 0.035)
    v.lift_eye((x0 - 0.86, 0.0, 2.292), Z, Y, 0.035)
    # the commander's cupola and its lid
    cup = v.cupola((x0 - 0.56, 0.0, 2.285), 0.335, 0.34, slits=5, start=math.radians(36))
    v.hatch_round("hatch_commander", "turret", cup + Z * 0.0, 0.25, Z, -X, thick=0.03, opens=105.0)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    v.vent((x0 + 0.42, -0.2, 2.245), 0.12, 0.05)
    v.port((x0 + 0.2, 0.32, 2.262), Z, 0.05)
    # turret face: the gunner's sight port and a visor each side of the gun
    if fine:
        for y in (-0.38, 0.38):
            v.visor((x0 + 0.985, y, 2.02), Vector((1, 0, 0.18)).normalized(), 0.16, 0.09, 0.03, brow=False)

    # ---- the gun --------------------------------------------------------------------------------------
    tx, tz = x0 + 0.92, 1.98
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    # the mantlet: a curved shield, and the armoured housing of the recoil gear
    plan = [(0.10, -0.47), (0.24, -0.40), (0.30, -0.2), (0.30, 0.2), (0.24, 0.40), (0.10, 0.47)]
    v.prism([(tx + x, y) for x, y in plan], tz - 0.27, tz + 0.27, "upper", bevel=0.018, segs=2)
    v.lathe((tx + 0.28, 0, tz), X, [(0.52, 0.0), (0.52, 0.115), (0.46, 0.135), (0.0, 0.17), (0.0, 0.0)], "upper", v.n(24, 8))
    v.slab((tx + 0.28, -0.11, tz - 0.25), (tx + 0.62, 0.11, tz - 0.12), "upper", 0.012)
    if fine:
        v.bolts((tx + 0.30, 0, tz), X, 0.20, 8, r=0.013)
    v.mg((tx + 0.30, -0.30, tz + 0.02), X, 0.34)
    v.socket("coax", (tx + 0.66, -0.30, tz + 0.02), X)
    v.part("barrel", "gun", (tx + 0.80, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.42], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    # two guns for the one mounting: each is its own part on the sliding barrel, and the game shows one of them
    if gun in ("all", "long"):
        v.part("barrel_long", "barrel", (tx + 0.80, 0.0, tz), kind="fixed", note="the long 7.5 cm (L/43, L/48) with its muzzle brake: 1942 on")
        v.barrel((tx + 0.80, 0, tz), 2.06, 0.075, r0=0.098, r1=0.066, brake="double", steps=((0.36, None),), sleeve=None)
        v.socket("muzzle", (tx + 2.88, 0, tz), X)          # (7.02 m over all: the muzzle 1.10 m ahead of the hull)
    if gun in ("all", "short"):
        v.part("barrel_short", "barrel", (tx + 0.80, 0.0, tz), kind="fixed", note="the short 7.5 cm (L/24), no muzzle brake: 1939 to early 1942")
        v.barrel((tx + 0.80, 0, tz), 0.62, 0.075, r0=0.100, r1=0.086, brake=None, sleeve=None)
        v.socket("muzzle.short" if gun == "all" else "muzzle", (tx + 1.44, 0, tz), X, note="the short gun's muzzle: it stops short of her nose")
    v.m.info["bore"] = 0.075
    v.m.info["brake"] = gun != "short"
    skirt_parts, skirt_plates = pz4_skirts(v, x0, zt, set(skirts.split(",")) if isinstance(skirts, str) else ()) if skirts else ([], [])
    # ---- zimmerit: the ribbed paste on her upright plates, on tanks built from September 1943 to September 1944 ----
    zim = []
    if zimmerit and fine:
        v.part("zimmerit_hull", "hull", (0.0, 0.0, 1.10), kind="fixed", note="the ribbed paste on her hull's sides and driver's plate (shown with the zimmerit swap)")
        zim.append("zimmerit_hull")
        v.use("zimmerit_hull")
        k = 0
        for s in (LEFT, RIGHT):
            k = v.zimmerit([(1.72, s * sw, 1.12), (-0.95, s * sw, 1.12), (-2.70, s * sw, 1.12)], [(1.665, s * sw, 1.70), (-0.95, s * sw, 1.70), (-2.70, s * sw, 1.585)], Y * s, start=k)
        dn_ = Vector((0.62, 0.0, 0.07)).normalized()
        v.zimmerit([Vector((1.738, -sw + 0.03, 1.12)) + dn_ * 0.002, Vector((1.738, sw - 0.03, 1.12)) + dn_ * 0.002],
                   [Vector((1.672, -sw + 0.03, 1.70)) + dn_ * 0.002, Vector((1.672, sw - 0.03, 1.70)) + dn_ * 0.002], dn_, start=k)
        if skirts:                                          # the shield round her turret wore it too (the hull's hung plates did not)
            sz0, sz1 = zt + 0.08, zt + 0.58
            v.part("zimmerit_shield", "skirt_turret", (x0 - 0.4, 0.0, sz0), kind="fixed", note="the ribbed paste on her turret's shield (shown with the zimmerit swap and the skirts)")
            zim.append("zimmerit_shield")

            def sheet(p, q, s, cut=0.0, k=0):
                a, b = Vector((x0 + p[0], s * p[1], 0.0)), Vector((x0 + q[0], s * q[1], 0.0))
                d = (b - a).normalized()
                n = Vector((d.y, -d.x, 0.0))
                if n.dot(Vector(((a.x + b.x) / 2 - (x0 - 0.4), (a.y + b.y) / 2, 0.0))) < 0:
                    n = -n
                a, b = a + d * 0.012, b - d * 0.012
                return v.zimmerit([a + Z * (sz0 + 0.015), b + Z * (sz0 + 0.015)], [a + Z * (sz1 - 0.015 - cut), b + Z * (sz1 - 0.015)], n, start=k)
            for letter, s in SIDES:
                v.use("zimmerit_shield")
                k = sheet(PZ4_SHIELD[0], PZ4_SHIELD[1], s, cut=0.18, k=k)
                k = sheet(PZ4_SHIELD[2], PZ4_SHIELD[3], s, k=k)
                k = sheet(PZ4_SHIELD[3], PZ4_SHIELD[4], s, k=k)
                name = f"zimmerit_door_{letter}"
                door = f"skirt_door_{letter}"
                v.part(name, door, tuple(v.m.parts[v.m.part_at[door]].pivot), kind="fixed", note="the ribbed paste on the shield's door (shown with the zimmerit swap and the skirts)")
                zim.append(name)
                v.use(name)
                k = sheet(PZ4_SHIELD[1], PZ4_SHIELD[2], s, k=k)
            v.use("zimmerit_shield")
            sheet(PZ4_SHIELD[4], (PZ4_SHIELD[4][0], -PZ4_SHIELD[4][1]), 1.0, k=k)
    v.m.use("barrel")

    # ---- sockets for what happens to her ------------------------------------------------------------------
    v.socket("engine", (-1.9, 0.0, 1.66), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-1.7, 0.3, 1.66), Z, part="hull")
    v.socket("fire.2", (x0 - 0.56, 0.0, 2.6), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.22, 0.62, 1.74), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", (2.62, 0.2, 0.9), X, part="hull")
    v.socket("hit.left", (0.2, 1.31, 1.4), Y, part="hull")
    v.socket("hit.right", (0.2, -1.31, 1.4), -Y, part="hull")
    v.socket("hit.rear", (-2.74, 0.0, 1.2), -X, part="hull")
    v.socket("hit.turret", (x0 + 0.6, 0.78, 2.0), Y, part="turret")
    v.socket("case", (x0 - 0.2, -0.75, 2.0), -Y, part="turret", note="where the empty case is thrown out")
    for s in (LEFT, RIGHT):
        v.mark("GER", (-0.25, s * 1.322, 1.42), Y * s, 0.17, part="hull")               # (painted over the paste where she wears it)
    v.tank_clips(recoil=0.38, kick=0.9, gun_low=-8.0, hatches=["hatch_commander", "skirt_door_l", "skirt_door_r", "hatch_gunner", "hatch_loader", "hatch_driver", "hatch_radio"],
                 traverse=14.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.9, 2.9, 0.42, 172.0))
    swaps = {"how": "one model for the whole war. gun: show the chosen gun's parts and scale the other's to nothing (a part scaled to "
                    "nothing is not drawn), and use its sockets in place of the default ones. paint: put the chosen coat's colours "
                    "in place of the paints of the same names. " + BY_MONTH_HOW,
             "paint": coats(v, paint or "grey"), "by_month": PZ4_MONTHS}
    if zim:
        swaps["how"] += " " + ZIMMERIT_HOW
        swaps["zimmerit"] = zimmerit_swap(zim)
    if gun == "all":
        swaps["gun"] = {"default": "long", "choices": {
            "long": {"what": "7.5 cm L/43 and L/48, double-baffle muzzle brake", "years": "1942 on", "parts": ["barrel_long"], "sockets": {"muzzle": "muzzle"}, "muzzle_brake": True},
            "short": {"what": "7.5 cm L/24, no muzzle brake", "years": "1939 to early 1942", "parts": ["barrel_short"], "sockets": {"muzzle": "muzzle.short"}, "muzzle_brake": False}}}
    if skirts:
        swaps["skirts"] = {"default": "off", "choices": {
            "off": {"what": "no skirts", "years": "to April 1943", "parts": []},
            "on": {"what": "thin plates hung on rails along her hull, a shield round her turret's sides and back", "years": "1943-05 on, with the dark yellow coat", "parts": skirt_parts,
                   "lose": {"parts": skirt_plates, "how": "each tank hides none, one or two of these hull plates at random, so a company is not one tank copied; "
                                                           "the rails and the turret's shield always stay"}}}}
    m.info.setdefault("file", {})["swaps"] = swaps
    # no name: a German crew painted rings round the barrel (b477), on whichever of her two guns she carries
    for bp, mz, st in (("barrel_long", "muzzle", 0.075), ("barrel_short", "muzzle.short" if gun == "all" else "muzzle", 0.05)):
        if bp in v.m.part_at:
            v.kill_rings(bp, muzzle=mz, step=st)
    return v


def panther(fine=True, tag="GER", paint=None):
    """Panther Ausf. G: 6.87 m hull, 3.27 m wide, 2.99 m high; sloped plate all round, eight axles of big interleaved
    wheels a side with the track lying on them, drive at the front; the long 7.5 cm L/70 in a rounded mantlet."""
    v = Works("Panther", "Panther", "tank", tag, fine, paint=GER_COATS[paint or "yellow"][0])       # (she came after the grey: dark yellow from her first day)
    v.root(hull_pivot=(0.0, 0.0, 0.85))
    hw, sw, st, zf, zr = 0.90, 1.63, 1.26, 1.10, 1.76
    # ---- the hull: a tub between the tracks; over them a box whose every side leans in ---------------
    v.hull([(x, hw, z) for x, z in [(-2.95, 0.56), (-3.10, 0.62), (-3.21, 1.10), (3.21, 1.10), (3.36, 1.00), (2.62, 0.56)]], coat, bevel=0.014)
    v.hull([(3.21, sw, zf), (2.27, st, zr), (-3.37, st, zr), (-3.21, sw, zf)], coat, bevel=0.016)
    gl = Vector((2.27 - 3.36, 0, zr - 1.00)).normalized()          # up the glacis
    gn = Vector((gl.z, 0, -gl.x))                                    # out of it
    sn = [Vector((0, s * (zr - zf), sw - st)).normalized() for s in (1.0, -1.0)]       # out of each side plate

    def side_y(z):
        return sw - (z - zf) * (sw - st) / (zr - zf)
    # the nose: the hull machine gun in its ball on the right of the glacis, towing eyes, the track guards' front flaps
    g_at = lambda u, y: Vector((3.36, y, 1.00)) + gl * u + gn * 0.002      # noqa: E731  (a place on the glacis, u metres up it)
    v.mg_ball(g_at(0.62, -0.62), gn, 0.12, socket="hull_mg")
    if fine:
        v.weld(g_at(0.18, -0.86), g_at(0.18, 0.86), gn)
        for y in (-0.3, 0.3):                                        # the gun's travel crutch, folded
            v.rod(g_at(0.95, y), g_at(1.16, y * 0.4), 0.018, "upper", 6)
    for s in (LEFT, RIGHT):
        v.hook((3.22, s * 0.84, 0.86), X, 0.1)
        v.hook((-3.14, s * 0.80, 0.72), -X, 0.1)
        v.plate([(3.21, s * 0.94, 1.115), (3.21, s * 1.66, 1.115), (3.66, s * 1.66, 0.98), (3.66, s * 0.94, 0.98)][::int(s)], 0.008, "upper", 0.003)
        v.slab((-3.30, s * 0.94, 1.10), (3.21, s * 1.66, 1.112), "upper", 0.003)
    # the roof: the driver's and the wireless operator's lids, their periscopes, the ventilator between
    v.hatch_rect("hatch_driver", "hull", (1.62, 0.66, zr), (0.52, 0.42), Z, X, hinge="left", opens=100.0, thick=0.03)
    v.hatch_rect("hatch_radio", "hull", (1.62, -0.66, zr), (0.52, 0.42), Z, X, hinge="right", opens=100.0, thick=0.03)
    v.use("hull")
    for y in (0.62, -0.62):
        v.periscope((2.02, y, zr), X, (0.09, 0.2, 0.06))
    v.vent((1.9, 0.0, zr), 0.13, 0.05)
    v.headlamp((2.9, 0.98, 1.40), X, 0.07, stalk=0.08, cover=True)
    # the engine deck: the big hatch in the middle, a round fan grille each side with a square grille fore and aft of it
    v.slab((-3.2, -0.52, zr), (-1.62, 0.52, zr + 0.025), "upper", 0.008)
    if fine:
        for x in (-2.9, -1.9):
            v.handle((x, -0.12, zr + 0.025), (x, 0.12, zr + 0.025), Z, 0.008, 0.04, "steel")
        for y in (-0.3, 0.3):
            v.ball((-2.4, y, zr + 0.025), 0.1, "upper", 12, 6, dome=True, squash=0.5)
    for s in (LEFT, RIGHT):
        v.fan_grille((-2.40, s * 0.90, zr), 0.30)
        for x in (-1.84, -2.96):
            v.grille((x, s * 0.90, zr), (0.38, 0.6), Z, Y, slats=7, deep=0.02)
    v.aerial("aerial", "hull", (-1.5, 1.05, zr), 2.0)
    # the tail plate: two exhaust pipes standing up it, the round engine door, a stowage bin each side, the jack
    rb = Vector((-3.37 + 3.10, 0, zr - 0.62)).normalized()          # up the tail plate
    rn = Vector((-rb.z, 0, rb.x))
    r_at = lambda u, y: Vector((-3.10, y, 0.62)) + rb * u + rn * 0.002       # noqa: E731
    v.lathe(r_at(0.55, 0.0), rn, [(0.03, 0.0), (0.03, 0.2), (0.0, 0.24)], "upper", v.n(24, 8))
    if fine:
        v.bolts(r_at(0.55, 0.0) + rn * 0.03, rn, 0.15, 8, r=0.012)
    for i, y in enumerate((0.3, -0.3)):
        v.lathe(r_at(0.36, y), rn, [(0.1, 0.0), (0.07, 0.07), (0.0, 0.1)], "upper", v.n(14, 6))
        v.tube([r_at(0.36, y) + rn * 0.06, r_at(0.42, y) + rn * 0.14, r_at(1.12, y) + rn * 0.14], 0.05, "rust", v.n(10, 5))
        v.socket(f"exhaust.{i + 1}", r_at(1.14, y) + rn * 0.14, (-0.3, 0, 1))
    for s in (LEFT, RIGHT):
        v.bin(r_at(0.42, s * 0.5) + rn * 0.001, r_at(1.0, s * 1.16) + rn * 0.3, bevel=0.02, latches=2, lid_side=(-1, 0, 0))
    v.jack(r_at(0.2, 0.0) + rn * 0.06, 90.0)
    # along the sides: skirts over the tracks, the cleaning rods' tube on the left, tools, the tow cable on the roof edge
    v.skirts(-3.0, 3.0, 1.66, 0.84, 1.10, count=6)
    zt_ = 1.52
    v.rod((-1.9, side_y(zt_) + 0.075, zt_), (0.3, side_y(zt_) + 0.075, zt_), 0.075, "upper", v.n(16, 8))
    if fine:
        for x in (-1.6, 0.0):
            v.slab((x - 0.02, side_y(zt_ - 0.09), zt_ - 0.09), (x + 0.02, side_y(zt_) + 0.16, zt_ + 0.09), "shade")
        v.shovel((1.2, -side_y(1.5) - 0.03, 1.5), 0.0, 0.95, roll=-60.0)
        v.crowbar((-0.4, -side_y(1.36) - 0.02, 1.36), 0.0, 1.5)
        v.axe((0.2, -side_y(1.62) - 0.03, 1.62), 180.0, 0.8, roll=60.0)
        v.cable([(1.9, 1.12, zr + 0.014), (0.6, 1.16, zr + 0.014), (-1.3, 1.16, zr + 0.014)], 0.014)
        v.cable([(1.9, -1.12, zr + 0.014), (0.6, -1.16, zr + 0.014), (-1.3, -1.16, zr + 0.014)], 0.014)

    # ---- the running gear -------------------------------------------------------------------------------
    ty, r_w = 1.31, 0.43
    xs = [-2.10 + 0.56 * i for i in range(8)]
    v.running(xs, r_w, ty, (2.95, 0.80, 0.385), (-2.90, 0.62, 0.31), top_on_wheels=True,
              wheel=dict(w=0.10, tyre=0.035, bolts=12, sides=32, hub=0.3, axle=0.3),
              per_axle=lambda i: {"gap": 0.34} if i % 2 == 0 else {"gap": 0.05, "bolts": 0},
              sprocket_kw=dict(teeth=17, wide=0.44, tooth=0.035, holes=0, bolts=12, sides=30, hub=0.36),
              idler_kw=dict(wide=0.4, kind="spoke", spokes=8),
              track=dict(pitch=0.15, wide=0.66, thick=0.03, sag=0.02, style={"kind": "plate", "horn": "twin", "horn_gap": 0.148, "horn_high": 0.08, "grouser": 0.016, "cleats": True}))
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((2.95, s * 0.88, 0.80), Y * s, [(0.2, 0.0), (0.2, 0.15), (0.12, 0.24), (0.0, 0.27), (0.0, 0.0)], "upper", v.n(22, 8))

    # ---- the turret --------------------------------------------------------------------------------------
    x0, zt = -0.15, zr
    v.ring_well((x0, 0.0, zt), 0.83)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    th = 0.78
    plan = sym_plan([(1.02, 0.82), (0.30, 1.10), (-1.30, 0.86)])
    top = inset_plan(plan, [0.35, 0.35, 0.35, 0.35, 0.35, 0.16])
    v.prism([(x0 + x, y) for x, y in plan], zt + 0.02, zt + th, coat, top=[(x0 + x, y) for x, y in top], bevel=0.016, segs=2, angle=14.0)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.86), (0.0, 0.88), (0.0, 0.0)], "shade", v.n(36, 12))
    roof = zt + th
    cup = v.cupola((x0 - 0.42, 0.36, roof), 0.36, 0.27, slits=7, start=0.2)
    v.hatch_round("hatch_commander", "turret", cup, 0.26, Z, Y, thick=0.03, opens=105.0)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    v.vent((x0 + 0.42, -0.1, roof), 0.12, 0.05)
    v.periscope((x0 + 0.45, -0.42, roof), X, (0.08, 0.16, 0.06))
    for at in ((x0 + 0.62, 0.42), (x0 + 0.62, -0.42), (x0 - 0.85, 0.0)):
        v.lift_eye((at[0], at[1], roof), Z, X if at[1] else Y, 0.035)
    # the round door in the turret's back plate
    bn = Vector((-math.cos(math.radians(24.6)), 0, math.sin(math.radians(24.6))))
    bc = Vector((x0 - 1.30 + 0.35 * 0.5, 0.0, zt + 0.02 + (th - 0.02) * 0.5)) + bn * 0.002
    v.hatch_round("hatch_loader", "turret", bc, 0.23, bn, Y, thick=0.03, opens=110.0)
    v.use("turret")
    for s in (LEFT, RIGHT):
        v.mark("GER", (x0 - 0.50, s * 0.845, zt + 0.40), Vector((0.16 * 0, s * 0.906, 0.42)), 0.17, part="turret")

    # ---- the gun --------------------------------------------------------------------------------------------
    tx, tz = x0 + 1.02, zt + 0.36
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 18.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx - 0.02, 0, tz), Y, [(0.70, 0.0), (0.70, 0.25), (0.64, 0.31), (-0.64, 0.31), (-0.70, 0.25), (-0.70, 0.0)], "upper", v.n(40, 12))     # the mantlet: a drum lying across her face
    v.lathe((tx + 0.2, 0, tz), X, [(0.62, 0.0), (0.62, 0.125), (0.55, 0.15), (0.0, 0.2), (0.0, 0.0)], "upper", v.n(24, 8))                       # the gun's collar
    v.mg((tx + 0.26, -0.33, tz + 0.02), X, 0.14)
    v.socket("coax", (tx + 0.42, -0.33, tz + 0.02), X)
    if fine:
        v.lathe((tx + 0.292, 0.33, tz + 0.03), X, [(0.0, 0.0), (0.0, 0.035)], "black", 10)        # the gunner's sight
    v.part("barrel", "gun", (tx + 0.8, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.5], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.8, 0, tz), 3.84, 0.075, r0=0.105, r1=0.068, brake="double", steps=((0.3, None),))
    v.socket("muzzle", (tx + 0.8 + 3.86, 0, tz), X)
    v.m.info["bore"] = 0.075

    # ---- sockets for what happens to her ----------------------------------------------------------------------
    v.socket("engine", (-2.4, 0.0, zr + 0.03), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-2.4, 0.9, zr + 0.03), Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.62, 0.66, zr + 0.02), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(0.5, 0.2), gn, part="hull")
    v.socket("hit.left", (0.2, side_y(1.4), 1.4), sn[0], part="hull")
    v.socket("hit.right", (0.2, -side_y(1.4), 1.4), sn[1], part="hull")
    v.socket("hit.rear", r_at(0.7, 0.0), rn, part="hull")
    v.socket("hit.turret", (x0 + 0.5, 0.9, zt + 0.4), Y, part="turret")
    v.socket("case", (x0 - 1.1, 0.0, zt + 0.4), -X, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.45, kick=0.8, gun_low=-8.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver", "hatch_radio"],
                 traverse=15.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-1.2, 3.2, 0.45, 168.0))
    v.m.info.setdefault("file", {})["swaps"] = {"how": "paint: the coat's colours stand in place of the paints of the same names. She wore dark yellow all her life.",
                                                "paint": coats(v, "yellow", only=("yellow",))}
    v.kill_rings("barrel")                                             # no name: a German crew painted rings round the barrel (b477)
    return v


def tiger(fine=True, tag="GER", paint=None, zimmerit=False):
    """Tiger I: 6.32 m hull, 3.56 m wide, 3.00 m high; a flat-sided box on broad tracks, eight axles of interleaved
    wheels a side, drive at the front; the 8.8 cm L/56 in a horseshoe turret with a flat slab of a mantlet."""
    v = Works("Tiger", "Tiger", "tank", tag, fine, paint=GER_COATS[paint][0] if paint else None)
    v.dice = 0.2
    v.worn_kw = dict(layers=2, marks=0.45, shade=True, gear=0.8, cast=lambda p, part: part == "gun")
    v.root(hull_pivot=(0.0, 0.0, 0.85))
    hw, sw, zf, zr = 0.92, 1.57, 1.20, 1.80
    # ---- the hull ------------------------------------------------------------------------------------------
    v.hull([(x, hw, z) for x, z in [(-3.02, 0.47), (-3.16, 0.62), (-3.16, 1.24), (2.15, 1.27), (2.80, 1.16), (2.98, 0.78), (2.50, 0.47)]], coat, bevel=0.016)
    v.hull([(2.22, sw, zf), (2.12, sw, zr), (-3.16, sw, zr), (-3.16, sw, zf)], coat, bevel=0.016)
    fn = Vector((0.6, 0, 0.1)).normalized()                           # out of the driver's plate
    f_at = lambda y, z: Vector((2.22 - (z - zf) * 0.1 / 0.6, y, z)) + fn * 0.002      # noqa: E731
    v.visor(f_at(0.56, 1.56), fn, 0.42, 0.14, 0.08)
    v.mg_ball(f_at(-0.62, 1.52), fn, 0.125, socket="hull_mg")
    if fine:
        # spare links across the nose plate
        nn = Vector((0.38, 0, 0.2)).normalized()
        v.spare_links(Vector((2.89, -0.78, 0.97)) + nn * 0.004, Vector((2.89, 0.78, 0.97)) + nn * 0.004, 12, 0.13, 0.36, nn, style={"kind": "plate", "grouser": 0.016, "cleats": True})
        v.weld((2.16, -0.9, 1.275), (2.16, 0.9, 1.275), Vector((0.3, 0, 1)).normalized())
    v.headlamp((2.2, 0.0, 1.28), X, 0.075, stalk=0.07, cover=True)
    for s in (LEFT, RIGHT):
        v.hook((2.96, s * 0.80, 0.70), X, 0.11)
        v.hook((-3.16, s * 0.80, 0.80), -X, 0.11)
        # the track guards: a strip along the box, a flap turned down at each end
        v.slab((-3.16, s * 1.55, zf - 0.012), (2.3, s * 1.80, zf), "upper", 0.003)
        v.plate([(2.3, s * 0.96, zf), (2.3, s * 1.80, zf), (3.12, s * 1.80, 1.02), (3.12, s * 0.96, 1.02)][::int(s)], 0.008, "upper", 0.003)
        v.plate([(-3.16, s * 1.80, zf), (-3.16, s * 0.96, zf), (-3.42, s * 0.96, 0.86), (-3.42, s * 1.80, 0.86)][::int(s)], 0.008, "upper", 0.003)
    # the roof: round lids for the driver and the wireless operator, their periscopes, the ventilator
    v.hatch_round("hatch_driver", "hull", (1.55, 0.86, zr), 0.25, Z, -X, thick=0.03, opens=105.0)
    v.hatch_round("hatch_radio", "hull", (1.55, -0.86, zr), 0.25, Z, -X, thick=0.03, opens=105.0)
    v.use("hull")
    v.vent((1.85, 0.0, zr), 0.12, 0.05)
    # the engine deck: the long hatch down the middle, grilles over the radiators and fans each side
    v.slab((-3.05, -0.5, zr), (-1.35, 0.5, zr + 0.03), "upper", 0.008)
    if fine:
        v.ball((-2.0, 0.0, zr + 0.03), 0.16, "upper", 14, 6, dome=True, squash=0.45)
        v.handle((-2.9, -0.12, zr + 0.03), (-2.9, 0.12, zr + 0.03), Z, 0.008, 0.04, "steel")
    for s in (LEFT, RIGHT):
        for x in (-1.72, -2.72):
            v.grille((x, s * 1.02, zr), (0.6, 0.82), Z, Y, slats=9, deep=0.022)
        v.slab((-2.38, s * 0.62, zr), (-2.06, s * 1.42, zr + 0.02), "upper", 0.006)
    v.aerial("aerial", "hull", (-1.25, -1.3, zr), 2.0)
    # the tail plate: two tall silencers in their shields, the jack across the foot of it
    for i, y in enumerate((0.42, -0.42)):
        v.silencer((-3.32, y, 1.08), (-3.32, y, 1.74), 0.115, bands=2, pipe=[(-3.16, y, 0.92), (-3.32, y, 0.94), (-3.32, y, 1.08)])
        if fine:
            v.lathe((-3.32, y, 1.0), Z, [(0.0, 0.16), (0.82, 0.16), (0.82, 0.172), (0.0, 0.172)], "upper", v.n(18, 8))
        v.socket(f"exhaust.{i + 1}", (-3.32, y, 1.80), (-0.3, 0, 1))
    v.jack((-3.24, 0.0, 0.92), 90.0, long=0.6)
    if fine:
        for y in (1.0, -1.0):
            v.lathe((-3.17, y, 1.5), -X, [(0.16, 0.0), (0.16, 0.17), (0.0, 0.19)], "upper", v.n(18, 8))     # the air cleaners' places
        v.cable([(1.9, 1.3, zr + 0.016), (0.4, 1.42, zr + 0.016), (-1.2, 1.42, zr + 0.016), (-1.5, 1.2, zr + 0.016), (-0.4, 1.18, zr + 0.016), (1.5, 1.18, zr + 0.016)], 0.016)
        v.cable([(1.9, -1.3, zr + 0.016), (0.4, -1.42, zr + 0.016), (-1.2, -1.42, zr + 0.016)], 0.016)
        v.shovel((2.0, 0.3, 1.24), 90.0, 0.9)
        # on the front roof, what her crew mend a track with: the sledge, the wire cutters, two bars; the cleaning rods along her right side
        v.rod((2.02, -0.75, 1.262), (2.02, -0.05, 1.262), 0.016, "wood", 8)
        v.box((0.07, 0.07, 0.16), (2.02, -0.80, 1.275), "steel", 0.008, turn=rot(0.0, 0.0, 90.0))
        for k in range(3):
            v.rod((1.2 - 0.02 * k, -1.44 + 0.045 * k, zr + 0.02), (-1.1 - 0.02 * k, -1.44 + 0.045 * k, zr + 0.02), 0.014, "wood", 6)
        for x in (0.9, -0.8):
            v.slab((x - 0.02, -1.47, zr), (x + 0.02, -1.32, zr + 0.042), "upper", 0.003)
        # the tail: the convoy lamp on the left, the starting handle's cover between the silencers, the track tools' box
        v.lathe((-3.165, 1.25, 1.02), -X, [(0.0, 0.0), (0.0, 0.05), (0.05, 0.05), (0.06, 0.03), (0.06, 0.0)], "upper", 12)
        v.lathe((-3.165, 0.0, 1.02), -X, [(0.0, 0.0), (0.0, 0.07), (0.02, 0.07), (0.03, 0.05), (0.03, 0.0)], "upper", 14)
        v.bolts((-3.195, 0.0, 1.02), -X, 0.05, 4, r=0.01, high=0.008)
        v.crowbar((0.3, -1.25, zr + 0.016), 0.0, 1.6)
        v.axe((1.0, 1.0, zr + 0.016), 0.0, 0.8)
    for s in (LEFT, RIGHT):
        v.mark("GER", (-0.6, s * (sw + 0.012), 1.5), Y * s, 0.2, part="hull")             # (painted over the paste where she wears it)

    # ---- the running gear --------------------------------------------------------------------------------
    ty, r_w = 1.415, 0.40
    xs = [-1.90 + 0.52 * i for i in range(8)]
    v.running(xs, r_w, ty, (2.72, 0.85, 0.40), (-2.74, 0.62, 0.30), top_on_wheels=True,
              wheel=dict(w=0.095, tyre=0.03, bolts=12, sides=32, hub=0.32, axle=0.3),
              per_axle=lambda i: {"gap": 0.40} if i % 2 == 1 else {"gap": 0.19, "bolts": 0},      # interleaved: the first axle's wheels outermost, the next axle's close behind them and half hidden
              sprocket_kw=dict(teeth=20, wide=0.5, tooth=0.035, bolts=12, sides=30, hub=0.34),
              idler_kw=dict(wide=0.44, kind="dish", holes=0),
              track=dict(pitch=0.13, wide=0.725, thick=0.032, sag=0.02, style={"kind": "plate", "horn": "twin", "horn_gap": 0.17, "horn_high": 0.085, "grouser": 0.016, "cleats": True}),
              sprung=(0.08, 0.14))
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((2.72, s * 0.90, 0.85), Y * s, [(0.2, 0.0), (0.2, 0.16), (0.12, 0.26), (0.0, 0.29), (0.0, 0.0)], "upper", v.n(22, 8))
        if fine:
            v.bolts((2.72, s * (0.90 + 0.165), 0.85), Y * s, 0.2, 12, r=0.014, high=0.01)
            # a shackle hanging in each towing eye, front and back
            v.tube([(3.02, s * 0.74, 0.72), (3.10, s * 0.74, 0.64), (3.12, s * 0.80, 0.56), (3.10, s * 0.86, 0.64), (3.02, s * 0.86, 0.72)], 0.02, "steel", 6)
            v.tube([(-3.22, s * 0.74, 0.82), (-3.30, s * 0.74, 0.74), (-3.32, s * 0.80, 0.66), (-3.30, s * 0.86, 0.74), (-3.22, s * 0.86, 0.82)], 0.02, "steel", 6)
            # the track guard along her side is four plates, each bolted on: the joins and their bolts
            for x in (-1.80, -0.44, 0.92):
                v.slab((x - 0.006, s * 1.56, zf - 0.016), (x + 0.006, s * 1.80, zf + 0.004), "shade")
                for dx in (-0.06, 0.06):
                    v.bolt((x + dx, s * 1.62, zf), Z, 0.011, 0.007)

    # ---- the turret: one plate bent round in a horseshoe ----------------------------------------------------
    x0, zt = 0.12, zr
    v.ring_well((x0, 0.0, zt), 0.93)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    th, R, cx = 0.80, 1.02, -0.18
    k = v.n(26, 8)
    arc = [(cx + R * math.cos(math.radians(a)), R * math.sin(math.radians(a))) for a in [86.0 + 188.0 * i / k for i in range(k + 1)]]
    plan = [(1.14, 0.75)] + arc + [(1.14, -0.75)]
    v.prism([(x0 + x, y) for x, y in plan], zt + 0.02, zt + th, coat, bevel=0.014, segs=2, angle=30.0)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.95), (0.0, 0.97), (0.0, 0.0)], "shade", v.n(36, 12))
    roof = zt + th
    cup = v.cupola((x0 - 0.36, 0.5, roof), 0.36, 0.2, slits=7, start=0.2)
    v.hatch_round("hatch_commander", "turret", cup, 0.26, Z, -Y, thick=0.03, opens=105.0)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_loader", "turret", (x0 + 0.12, -0.5, roof), (0.52, 0.44), Z, X, hinge="front", opens=100.0, thick=0.028)
    v.use("turret")
    v.vent((x0 - 0.5, -0.1, roof), 0.13, 0.05)
    v.periscope((x0 + 0.62, -0.5, roof), X, (0.08, 0.16, 0.06))
    v.bin((x0 - 1.74, -0.62, zt + 0.16), (x0 - 1.22, 0.62, zt + 0.66), bevel=0.05, latches=2, lid_side=(-1, 0, 0))
    if fine:
        # spare links hung on the turret's sides
        for s, n_links in ((1.0, 5), (-1.0, 3)):
            a, b = Vector((x0 + 0.98, s * 0.79, zt + 0.42)), Vector((x0 + 0.1, s * 0.985, zt + 0.42))
            way = (b - a).normalized()
            out = Vector((way.y, -way.x, 0)) * s
            v.spare_links(a + out * 0.012, a + out * 0.012 + way * (0.13 * n_links), n_links, 0.13, 0.58, out, style={"kind": "plate", "grouser": 0.016, "cleats": True}, bar=False)
        # the loader's escape hatch in the right of the turret's back, and the plugged port opposite it
        for s, big in ((-1.0, True), (1.0, False)):
            ang = math.radians(118.0 * s)
            o = Vector((math.cos(ang), math.sin(ang), 0.0))
            c = Vector((x0 + cx, 0.0, zt + 0.40)) + o * (R + 0.006)
            if big:
                v.lathe(c, o, [(0.0, 0.0), (0.0, 0.25), (0.028, 0.25), (0.04, 0.22), (0.04, 0.0)], "upper", v.n(26, 10))
                v.bolts(c + o * 0.04, o, 0.12, 4, r=0.014, high=0.01)
                side = Vector((-o.y, o.x, 0.0))
                v.slab(c + o * 0.02 + side * 0.25 - Z * 0.07 - Vector((0.02, 0.02, 0)), c + o * 0.05 + side * 0.31 + Z * 0.07 + Vector((0.02, 0.02, 0)), "upper", 0.006)      # its hinge
            else:
                v.lathe(c, o, [(0.0, 0.0), (0.0, 0.085), (0.03, 0.075), (0.045, 0.04), (0.045, 0.0)], "upper", v.n(16, 8))
    for s in (LEFT, RIGHT):
        ang = math.radians(150.0 * s)
        v.mark("GER", (x0 + cx + (R + 0.012) * math.cos(ang), (R + 0.012) * math.sin(ang), zt + 0.42), (math.cos(ang), math.sin(ang), 0), 0.16, part="turret")

    # ---- the gun ---------------------------------------------------------------------------------------------
    tx, tz = x0 + 1.14, zt + 0.40
    v.part("gun", "turret", (tx - 0.1, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 15.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.slab((tx + 0.005, -0.73, tz - 0.31), (tx + 0.16, 0.73, tz + 0.31), "upper", 0.016, segs=2)          # the mantlet: a slab right across her face
    v.slab((tx + 0.16, -0.3, tz - 0.25), (tx + 0.24, 0.3, tz + 0.25), "upper", 0.014, segs=2)
    v.lathe((tx + 0.22, 0, tz), X, [(0.55, 0.0), (0.55, 0.15), (0.42, 0.19), (0.0, 0.25), (0.0, 0.0)], "upper", v.n(26, 8))
    v.mg((tx + 0.16, -0.44, tz + 0.02), X, 0.12)
    v.socket("coax", (tx + 0.3, -0.44, tz + 0.02), X)
    if fine:
        for y in (0.44, 0.54):
            v.lathe((tx + 0.162, y, tz + 0.06), X, [(0.0, 0.0), (0.0, 0.03)], "black", 10)           # the two eyes of the gunner's sight
    v.part("barrel", "gun", (tx + 0.75, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.6], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.75, 0, tz), 3.15, 0.088, r0=0.135, r1=0.086, brake="double", steps=((0.4, 0.105),))
    v.socket("muzzle", (tx + 0.75 + 3.17, 0, tz), X)
    v.m.info["bore"] = 0.088

    # ---- zimmerit: the ribbed paste on her upright plates, on tanks built from September 1943 to September 1944 ----
    zim = []
    if zimmerit and fine:
        v.part("zimmerit_hull", "hull", (0.0, 0.0, zf), kind="fixed", note="the ribbed paste on her hull's sides, front and tail (shown with the zimmerit swap)")
        v.part("zimmerit_turret", "turret", (x0, 0.0, zt), kind="fixed", note="the ribbed paste round her turret (shown with the zimmerit swap)")
        zim = ["zimmerit_hull", "zimmerit_turret"]
        v.use("zimmerit_hull")
        k = 0
        for s in (LEFT, RIGHT):
            k = v.zimmerit([(2.19, s * sw, zf + 0.02), (-3.14, s * sw, zf + 0.02)], [(2.11, s * sw, zr - 0.02), (-3.14, s * sw, zr - 0.02)], Y * s, start=k)
        k = v.zimmerit([f_at(-sw + 0.03, zf + 0.02), f_at(sw - 0.03, zf + 0.02)], [f_at(-sw + 0.03, zr - 0.02), f_at(sw - 0.03, zr - 0.02)], fn, start=k)
        k = v.zimmerit([(-3.16, -hw + 0.02, 0.66), (-3.16, hw - 0.02, 0.66)], [(-3.16, -hw + 0.02, 1.22), (-3.16, hw - 0.02, 1.22)], -X, start=k)
        v.zimmerit([(-3.16, -sw + 0.02, zf + 0.03), (-3.16, sw - 0.02, zf + 0.03)], [(-3.16, -sw + 0.02, zr - 0.02), (-3.16, sw - 0.02, zr - 0.02)], -X, start=k)
        v.use("zimmerit_turret")
        ring = [(x0 + x, y) for x, y in plan]
        v.zimmerit([(x, y, zt + 0.045) for x, y in ring], [(x, y, zt + th - 0.03) for x, y in ring],
                   lambda mid: (Vector((mid.x - x0 - cx, mid.y, 0.0)) if mid.x < x0 + cx + R * 0.07 else Vector((0.2675, 1.249 if mid.y > 0 else -1.249, 0.0))).normalized())
        v.use("turret")

    spray(v, paint)
    # ---- sockets for what happens to her ------------------------------------------------------------------
    v.socket("engine", (-2.2, 0.0, zr + 0.03), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-2.2, 1.0, zr + 0.03), Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.55, 0.86, zr + 0.02), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", f_at(0.1, 1.5), fn, part="hull")
    v.socket("hit.left", (0.2, sw, 1.5), Y, part="hull")
    v.socket("hit.right", (0.2, -sw, 1.5), -Y, part="hull")
    v.socket("hit.rear", (-3.16, 0.0, 1.4), -X, part="hull")
    v.socket("hit.turret", (x0 + 0.3, 0.96, zt + 0.4), Y, part="turret")
    v.socket("case", (x0 + 0.12, -0.5, roof), Z, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.55, kick=0.9, gun_low=-8.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver", "hatch_radio"],
                 traverse=6.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_02", blow=(-1.3, 3.4, 0.48, 165.0))
    swaps = {"how": "paint: put the chosen coat's colours in place of the paints of the same names. " + ZIMMERIT_HOW + " " + BY_MONTH_HOW,
             "paint": coats(v, paint or "grey"), "by_month": sprayed(TIGER_MONTHS)}
    if zim:
        swaps["zimmerit"] = zimmerit_swap(zim)
    v.m.info.setdefault("file", {})["swaps"] = swaps
    v.kill_rings("barrel")                                             # no name: a German crew painted rings round the barrel (b477)
    return v
