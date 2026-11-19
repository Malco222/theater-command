"""The shipyard: the pieces a warship is built from, each made from numbers.

A hull is a skin over frames from stern to bow (a real hull form: flared bows, a rounded cruiser stern or a square
transom, the turn of the bilge, the keel rising aft to the screws). On it stand houses, funnels, masts, turrets that
train, guns that lift and run back, directors that turn, boats, cranes, rails. Everything is in metres, to the
measurements of the type; x forward, z up from the waterline, y to port (see rigid.py).

The hull is in two parts, `fore` and `aft`, meeting at a frame amidships, so that a ship whose magazine goes up can
break her back. Everything that stands on her belongs to whichever half it stands on.
"""

import math
import random

from mathutils import Matrix, Vector, noise

from rigid import PORT, STBD, X, Y, Z, Model, lerp, smooth

# Paints every ship has (a navy's own greys are put in by its livery)
COMMON = {
    "hull": "7A848C", "upper": "7A848C", "deck": "4E5459", "planks": "B79E74", "flight": "4E5459",
    "boot": "1E2023", "antifoul": "6B2F26", "steel": "2E3134", "black": "17181A", "canvas": "B7B09A", "boat": "D8D2C4",
    "glass": "1B2A33", "roof": "5B6369", "torn": "241D1A", "rope": "3A3630", "brass": "8C7A4A", "line": "E8E4DA",
    "red": "B01818", "white": "E8E4DA", "lino": "7A4A32", "cork": "8E8672", "bag": "8F8A78", "door": "5E666C", "seam": "5E666C",
}


def livery(tag, cls, hull_hex=None):
    """A navy's paint (the art director's ruling on the ships, 2026-11-14, and the unit table's hull greys)."""
    jap, fra = tag in ("JAP", "JPN"), tag in ("FRA", "VIC", "FFR")
    hull = hull_hex or {"GER": "6A7278", "JAP": "5C6266", "JPN": "5C6266", "USA": "5F6E80", "ITA": "A2A8A6", "FRA": "8E9AA6", "VIC": "8E9AA6",
                        "SOV": "565E6A"}.get(tag, "7A848C")
    upper = "A9AEB0" if tag == "GER" else hull
    deck = "3E4448" if cls == "submarine" else "565C61"       # steel decks: the houses' tops, a destroyer's deck (wood is "planks")
    flight = "B79E74" if (jap or tag == "USA") else "4E5459"
    p = dict(COMMON)
    p.update({"hull": hull, "upper": upper, "deck": deck, "planks": "B79E74", "flight": flight})
    # (a turret's roof a quarter darker than its sides, as they were painted against the eye above)
    p["roof"] = "".join("%02X" % int(int(upper[i:i + 2], 16) * 0.78) for i in (0, 2, 4))
    dark = lambda hx, k: "".join("%02X" % int(int(hx[i:i + 2], 16) * k) for i in (0, 2, 4))
    p["door"], p["seam"] = dark(upper, 0.8), dark(hull, 0.86)
    if cls == "submarine":
        p["upper"] = hull
        p["hull"] = "".join("%02X" % int(int(hull[i:i + 2], 16) * 0.8) for i in (0, 2, 4))
    return p


class Hull:
    """The shape of a hull, and where its skin is."""

    def __init__(self, L, B, D, T, sheer=2.5, sheer_aft=0.0, rake=6.0, counter=3.0, stern="cruiser", flare=0.45,
                 step_at=None, step=0.0, fine=1.9, full=0.62, bilge=5.0, wl_fore=0.5, transom=0.6, step_aft_at=None, step_aft=0.0, tumble=1.0):
        self.L, self.B, self.D, self.T = L, B, D, T
        self.sheer, self.sheer_aft, self.rake, self.counter = sheer, sheer_aft, rake, counter
        self.stern, self.flare, self.step_at, self.step = stern, flare, step_at, step
        self.step_aft_at, self.step_aft = step_aft_at, step_aft
        self.fine, self.full, self.bilge, self.wl_fore, self.transom = fine, full, bilge, wl_fore, transom
        self.tumble = tumble          # how wide the deck is beside her greatest beam (a submarine's casing is half of it)
        self.Lwl = L - rake - (counter if stern == "cruiser" else 0.0)
        self.x_aft = -L / 2.0 + (counter if stern == "cruiser" else 0.0)

    # (u runs from -1 at the after end of the waterline to +1 at its fore end)
    def frame_x(self, u):
        return self.x_aft + (u + 1.0) * 0.5 * self.Lwl

    def u_of(self, x):
        return (x - self.x_aft) / self.Lwl * 2.0 - 1.0

    def lean(self, u):
        """How far the frame leans at the deck: forward at the stem, aft at a cruiser stern."""
        if u > 0.6:
            return self.rake * ((u - 0.6) / 0.4) ** 2
        if u < -0.72 and self.stern == "cruiser":
            return -self.counter * ((-u - 0.72) / 0.28) ** 2
        return 0.0

    def half_deck(self, u):
        return self.beam(u) * self.tumble

    def beam(self, u):
        if u >= 0.0:
            return self.B * 0.5 * max(0.0, 1.0 - u ** self.fine) ** self.full
        if self.stern == "transom":
            return self.B * 0.5 * lerp(1.0, self.transom, smooth((-u - 0.25) / 0.75))
        return self.B * 0.5 * max(0.0, 1.0 - (-u) ** 3.4) ** 0.5

    def half_water(self, u):
        d = self.beam(u)
        if u > 0.2:
            return d * lerp(0.96, self.wl_fore, smooth((u - 0.2) / 0.8)) * (1.0 if u < 0.97 else max(0.0, (1.0 - u) / 0.03) ** 0.5)
        if u < -0.6 and self.stern == "cruiser":
            return d * lerp(0.96, 0.7, ((-u - 0.6) / 0.4) ** 2)
        return d * 0.96

    def deck_z(self, x):
        u = self.u_of(x)
        z = self.D + self.sheer * max(0.0, u) ** 2.2 + self.sheer_aft * max(0.0, -u) ** 2.0
        if self.step_at is not None and x > self.step_at:
            z += self.step
        if self.step_aft_at is not None and x < self.step_aft_at:
            z -= self.step_aft
        return z

    def keel(self, u):
        t = self.T
        if u < -0.4:
            t *= 1.0 - 0.78 * smooth((-u - 0.4) / 0.6)
        if u > 0.82:
            t *= 1.0 - 0.55 * ((u - 0.82) / 0.18) ** 2
        return max(t, 0.5)

    def point(self, u, where, hand=PORT):
        """A point of the skin on frame u. `where`: below water -1 (keel) .. 0 (waterline); above 0 .. 1 (deck edge)."""
        x = self.frame_x(u)
        hw, hd = max(self.half_water(u), 0.12), max(self.half_deck(u), 0.12)
        zd = self.deck_z(x + self.lean(u))
        if where <= 0.0:
            m = lerp(self.bilge, 1.5, min(1.0, abs(u) ** 1.6))
            phi = (1.0 + where) * math.pi / 2.0
            y = hw * math.sin(phi) ** (2.0 / m)
            z = -self.keel(u) * math.cos(phi) ** (2.0 / m)
            cut = 0.0
            if u > 0.75:   # the forefoot is cut back under water
                cut = -self.rake * 0.35 * ((u - 0.75) / 0.25) ** 2 * (z / -self.keel(u)) ** 2
            return Vector((x + cut, hand * y, z))
        fl = lerp(1.0, 1.0 + 1.6 * self.flare, smooth((u - 0.15) / 0.6)) if u > 0.15 else 1.0
        if self.tumble < 1.0:
            fl = 2.2                   # her side rounds in to the casing
        y = hw + (hd - hw) * where ** fl
        return Vector((x + self.lean(u) * where ** 1.3, hand * y, zd * where))

    def side(self, x, z, hand=PORT):
        """The skin's point and outward facing nearest a place along the ship at a height above water."""
        u = self.u_of(x)
        for _ in range(4):   # (the frames lean, so find the frame whose point at this height is at x)
            w = min(max(z / max(self.deck_z(self.frame_x(u) + self.lean(u)), 0.1), 0.0), 1.0)
            p = self.point(u, w, hand)
            u += (x - p.x) / self.Lwl * 2.0
            u = min(max(u, -1.0), 1.0)
        w = min(max(z / max(self.deck_z(self.frame_x(u) + self.lean(u)), 0.1), 0.0), 1.0)
        p = self.point(u, w, hand)
        a = self.point(min(u + 0.01, 1.0), w, hand) - self.point(max(u - 0.01, -1.0), w, hand)
        b = self.point(u, min(w + 0.05, 1.0), hand) - self.point(u, max(w - 0.05, 0.0), hand)
        n = a.cross(b)
        if n.length < 1e-9:
            n = Vector((0, hand, 0))
        n.normalize()
        if n.y * hand < 0:
            n = -n
        return p, n

    def half_at(self, x):
        """Half the deck's width at a place along the ship."""
        return self.side(x, self.deck_z(x), PORT)[0].y


class Yard:
    """A ship being built."""

    def __init__(self, model, hull, break_x=0.0):
        self.m, self.h, self.break_x = model, hull, break_x
        m = model
        m.part("ship", None, (0, 0, 0), kind="root", note="the whole ship: rolls, pitches, lists and sinks about the waterline amidships")
        keel = (break_x, 0, -hull.T * 0.5)
        m.part("fore", "ship", keel, kind="fixed", note="the fore half of the hull and all that stands on it")
        m.part("aft", "ship", keel, kind="fixed", note="the after half")
        self.fire_points = []

    def half(self, x):
        return "fore" if x >= self.break_x else "aft"

    def on(self, x):
        self.m.use(self.half(x))
        return self.m

    def deck(self, x):
        return self.h.deck_z(x) + 0.02

    # ---- the hull ------------------------------------------------------------------------------------
    def hull(self, deck_paint="planks", frames=None, deck_from=None, deck_to=None, boot=None, above=None, tear=None, line=None):
        """`boot`: the height of the top of her boot topping, where her freeboard is too low for the usual rule (a
        submarine's); `above`: the heights of the rows of points up her side, as shares of her freeboard (with `boot`,
        of her side above the boot); `tear`: how far the line she breaks along runs fore and aft of the break, in
        metres (None: by her beam; 0: a clean cut, and none of what a torn end shows); `line`: a height over the
        waterline where her paint changes, level all along her (the American two-colour schemes: navy blue up to the
        lowest point of her sheer, haze grey above it): a row of points is laid along it and her side above it is
        painted `upper`.

        Where she breaks (the art director, 09:24 UTC on 18 November 2026: "She parts along a straight, clean vertical
        line, flat plating on both sides of a dark wedge, like a model sawn in two... destruction must look broken,
        never cut away"). So her two halves meet along a ragged line: the frame at the break has its points thrown
        fore and aft by turns, and both skins share it, so that whole she shows nothing and broken each half ends in
        teeth of plating. The deck's last strip on each half is a part of its own (`tear.fore`, `tear.aft`), hinged
        across her, which the `break` clip heaves up. And behind each ragged edge is what a torn end shows
        (`torn_ends`)."""
        m, h = self.m, self.h
        n = frames or (96 if m.fine else 40)
        us = [-1.0 + 2.0 * k / n for k in range(n + 1)]
        ub = h.u_of(self.break_x)
        if tear is None:
            tear = min(max(0.08 * h.B, 0.6), 2.6)
        self.tear, self.jag = tear, None
        # (frames close together either side of a step in the deck, so that the step is a wall and not a ramp)
        for sx in (h.step_at, h.step_aft_at):
            if sx is not None:
                su = h.u_of(sx)
                us = [u for u in us if abs(u - su) > 1.2 / n] + [su - 0.0012, su + 0.0012]
        # (no frame nearer the break than the ragged line reaches)
        us = sorted(set([u for u in us if abs(u - ub) > max(0.6 / n, (tear + 0.5) / h.Lwl * 2.0 if tear > 0 else 0.0)] + [ub]))
        boot_lo, boot_hi = -0.45, 0.85
        below = [-1.0, -0.86, -0.68, -0.48, -0.3, -0.16] if m.fine else [-1.0, -0.7, -0.35]
        above = above or ([0.22, 0.42, 0.62, 0.82, 1.0] if m.fine else [0.5, 1.0])

        def shares_of(u):
            zd = h.deck_z(h.frame_x(u) + h.lean(u))
            wb = -0.07                                                      # the foot of the boot topping
            wt = min(boot_hi / zd, 0.18) if boot is None else min(boot / zd, 0.6)
            if boot is None:
                if line is not None:
                    # (the line is the same row on every frame, the rows below it shared out between the boot and
                    # it and those above between it and the deck's edge, so that the skin's quads never cross it)
                    s_ = min(max(line / zd, wt + 0.025), 0.99)
                    under, over = ((0.25, 0.5, 0.75), (0.5, 1.0)) if m.fine else ((0.5,), (1.0,))
                    return below + [wb, 0.0, wt] + [wt + (s_ - wt) * f for f in under] + [s_] + [s_ + (1.0 - s_) * f for f in over]
                rows = [a for a in above if a > wt + 0.03]
                return below + [wb, 0.0, wt] + rows
            # (shares of what is left of her side above the boot: the same count of rows on every frame)
            return below + [wb, 0.0, wt] + [wt + (1.0 - wt) * a for a in above]

        def ring(u):
            shares = shares_of(u)
            port = [h.point(u, w, PORT) for w in shares]
            # the deck's edge: the hull's own paint comes a little way in over the deck (the waterway)
            edge = port[-1]
            inn = Vector((edge.x, max(edge.y - 0.35, edge.y * 0.5), edge.z + 0.02))
            port.append(inn)
            star = [Vector((p.x, -p.y, p.z)) for p in reversed(port)]
            return star[:-1] + port if abs(port[0].y) < 1e-6 else star + port

        def paint(mid, n_):
            if mid.z < boot_lo * 0.6:
                return "antifoul"
            if mid.z < (boot_hi * 0.75 if boot is None else boot - 0.01):
                return "boot"
            if line is not None and mid.z > line + 0.01:
                return "upper"
            return "hull"

        rings = [ring(u) for u in us]
        k = us.index(ub)
        across = 7 if m.fine else 3
        deck_jag = [0.0] * across
        if tear > 0:
            # the ragged line: every point of the break's frame but those of her deck's edge and waterway (the first
            # two and the last two) is thrown fore or aft, and the deck's own points across her likewise
            rg = random.Random(int(h.L * 10) + 7)
            whole = rings[k]
            jag, sign = [0.0] * len(whole), 1.0
            for i in range(2, len(whole) - 2):
                sign = -sign if rg.random() < 0.72 else sign
                jag[i] = sign * tear * rg.uniform(0.35, 1.0)
            rings[k] = [Vector((p.x + d, p.y, p.z)) for p, d in zip(whole, jag)]
            self.jag = (shares_of(ub), jag)
            for j in range(1, across - 1):
                sign = -sign if rg.random() < 0.72 else sign
                deck_jag[j] = sign * tear * 0.8 * rg.uniform(0.35, 1.0)
        torn = tear > 0 and m.fine
        m.use("aft")
        m.skin(rings[:k + 1], paint, caps=(True, not torn), closed=False, cap_paint="torn")
        m.use("fore")
        m.skin(rings[k:], paint, caps=(not torn, True), closed=False, cap_paint="torn")
        # the deck, laid across from waterway to waterway with a little camber
        for part, sel in (("aft", us[:k + 1]), ("fore", us[k:])):
            m.use(part)
            rows = []
            for u in sel:
                e = h.point(u, 1.0, PORT)
                half = max(e.y - 0.35, e.y * 0.5)
                x = e.x
                if deck_from is not None and (x < deck_from or x > deck_to):
                    pass
                rows.append([Vector((x + (deck_jag[j] if u == ub else 0.0), half * (1.0 - 2.0 * j / (across - 1)),
                                     e.z + 0.02 + 0.22 * (1.0 - (1.0 - 2.0 * j / (across - 1)) ** 2) * min(1.0, half / 4.0))) for j in range(across)])
            if tear > 0 and len(rows) > 2:
                # (the deck's last strip is a flap hinged across her at the frame next the break: see `hull` above)
                flap, rows = (rows[-2:], rows[:-1]) if part == "aft" else (rows[:2], rows[1:])
                hinge = flap[0] if part == "aft" else flap[1]
                name = m.part("tear." + part, part, (hinge[0].x, 0.0, hinge[across // 2].z), axis=(0, 1, 0), limits=[-50.0, 50.0], kind="turn",
                              note="the deck at her break: hinged across her, heaved up by the `break` clip when her back breaks (" + ("+" if part == "fore" else "-") + " lifts its torn edge)")
                m.use(name)
                m.sheet(flap, deck_paint, Z, smooth=False)
                m.sheet([[p - Z * 0.05 for p in row] for row in flap], "torn", -Z, smooth=False)
                m.use(part)
            m.sheet(rows, deck_paint, Z, smooth=False)
        if tear > 0:
            m.socket("break", (self.break_x + 0.8, 0.0, h.deck_z(self.break_x) - 1.0), (0, 0, 1), "fore",
                     "where her back breaks: steam and smoke pour from the gap until it goes under")
        if torn:
            self.torn_ends(whole, rings[k], ring, paint)
        if m.fine:   # the plating's seams along each side, scribed, and the edge of the armour belt
            for hand in (PORT, STBD):
                for share, lo, hi in ((0.36, -0.9, 0.93), (0.68, -0.93, 0.95)):
                    for part, sel in (("aft", [u for u in us if lo <= u <= ub]), ("fore", [u for u in us if ub <= u <= hi])):
                        m.use(part)
                        pts, nrm = [], []
                        for u in sel:
                            p = h.point(u, share, hand)
                            a = h.point(min(u + 0.01, 1.0), share, hand) - h.point(max(u - 0.01, -1.0), share, hand)
                            b = h.point(u, share + 0.05, hand) - h.point(u, share - 0.05, hand)
                            nn = a.cross(b).normalized()
                            if nn.y * hand < 0:
                                nn = -nn
                            pts.append(p + X * self.jag_at(share, hand) if u == ub else p)
                            nrm.append(nn)
                        m.strip(pts, nrm, 0.035, "seam", 0.012)
        m.socket("wake.bow", (h.frame_x(1.0), 0, 0), (1, 0, 0), "ship", "the stem at the waterline: the bow wave's curl and the V of the wake start here")
        m.socket("wake.stern", (h.frame_x(-1.0), 0, 0), (-1, 0, 0), "ship", "the stern at the waterline: the ribbon of churned water starts here")
        m.info.setdefault("file", {}).update({"length": h.L, "beam": h.B, "draught": h.T, "waterline": [round(h.frame_x(-1.0), 2), round(h.frame_x(1.0), 2)]})

    def jag_at(self, w, hand):
        """How far fore (+) or aft the ragged line of her break lies from the break itself, at a share `w` of her side
        (as `Hull.point` takes it) on one hand."""
        if self.jag is None:
            return 0.0
        shares, jag = self.jag
        w = min(max(w, shares[0]), shares[-1])
        i = max(j for j in range(len(shares) - 1) if shares[j] <= w)
        t = (w - shares[i]) / max(shares[i + 1] - shares[i], 1e-9)
        # (the frame's points run from the starboard waterway down round her keel and up to the port waterway)
        at = (lambda j: len(jag) - 1 - len(shares) + j) if hand == PORT else (lambda j: len(shares) - j)
        return lerp(jag[at(i)], jag[at(i + 1)], t)

    def half_of(self, p, hand):
        """Which half a place on her side belongs to: which side of the ragged line it lies."""
        zd = self.h.deck_z(p.x)
        return "fore" if p.x >= self.break_x + self.jag_at(min(max(p.z / max(zd, 0.1), 0.0), 1.0), hand) else "aft"

    def torn_ends(self, whole, ragged, ring, paint):
        """What each half shows where her back is broken (only a fine model has it; whole, all of it is inside her).
        Behind the ragged edge of her plating: the plating's own thickness; a dark lining to her skin, so that the
        inside of her is not seen through; a bulkhead a few metres in; two of her frames standing bare, the nearer one
        uncovered wherever the plating is torn back past it; her decks between, broken off short or long; and tongues
        of plating bent inward, which reach past the break into the other half while she is whole and stand out of
        the torn end when the halves part. `whole` is the break's frame, `ragged` the same thrown fore and aft."""
        m, h, bx, tear = self.m, self.h, self.break_x, self.tear
        rg = random.Random(int(h.L * 10) + 11)
        back = tear + 1.8
        zc = (h.deck_z(bx) - h.T) * 0.5
        nb = len(ragged)

        def inward(p):
            v = Vector((0.0, -p.y, zc - p.z))
            return v.normalized() if v.length > 1e-6 else Z.copy()

        def inset(p, d):
            return p + inward(p) * min(d, max((Vector((0.0, p.y, p.z - zc))).length - 0.05, 0.0))

        def section(x):
            got = ring(h.u_of(x))
            return got if len(got) == nb else [Vector((x, p.y, p.z)) for p in whole]
        quads = [(i, i + 1, nb + i + 1, nb + i) for i in range(nb - 1)]
        for part, sg in (("fore", 1.0), ("aft", -1.0)):
            m.use(part)
            face = lambda mid, sg=sg: Vector((-sg, 0.0, 0.0))             # (toward the other half)
            lining = [inset(p, 0.07) for p in ragged]
            wall = [inset(p, 0.07) for p in section(bx + sg * back)]
            m.add(ragged + lining, quads, "torn", False, lambda mid, sg=sg: Vector((-sg, 0.0, 0.0)) - inward(mid) * 0.2)      # the plating's thickness
            m.add(lining + wall, quads, "torn", False, inward)                                                               # the lining
            m.add(wall + [Vector((bx + sg * back, 0.0, zc))], [(i, i + 1, nb) for i in range(nb - 1)] + [(nb - 1, 0, nb)], "torn", False, face)   # the bulkhead
            # her frames: an angle bar round her inside, pale against the dark
            for dist in (0.3 * tear + 0.15, back - 0.7):
                xr = bx + sg * dist
                sec = section(xr)
                foot, lip = [inset(p, 0.06) for p in sec], [inset(p, 0.42) for p in sec]
                near = [Vector((p.x - sg * 0.09, p.y, p.z)) for p in foot]
                near_lip = [Vector((p.x - sg * 0.09, p.y, p.z)) for p in lip]
                far_lip = [Vector((p.x + sg * 0.09, p.y, p.z)) for p in lip]
                m.add(near + near_lip, quads, "upper", False, face)                           # its web, toward the break
                m.add(near_lip + far_lip, quads, "upper", False, inward)                      # its flange, toward her middle
                far = [Vector((p.x + sg * 0.09, p.y, p.z)) for p in foot]
                m.add(far + far_lip, quads, "torn", False, lambda mid, sg=sg: Vector((sg, 0.0, 0.0)))
                m.add(near + far, quads, "upper", False, lambda mid: -inward(mid))             # its heel, where the plating was
            # her decks below: each a shelf from the bulkhead toward the break, broken off ragged
            z = h.deck_z(bx) - 2.6
            while z > 0.4:
                wide = h.side(bx, z, PORT)[0].y - 0.3
                if wide > 1.0:
                    edge = [(bx - sg * tear * rg.uniform(-0.9, 0.7), wide * (1.0 - 2.0 * i / 6.0)) for i in range(7)]
                    plan = [(bx + sg * back, -wide), (bx + sg * back, wide)] + edge
                    m.prism(plan, z - 0.1, z, "torn", top_paint="deck", bottom=True)
                z -= 2.6
            # tongues of plating bent inward
            for i in range(2, nb - 3):
                if rg.random() > 0.55:
                    continue
                a, b = inset(ragged[i], 0.22), inset(ragged[i + 1], 0.22)
                long, bend = (tear + 1.2) * rg.uniform(0.45, 1.0), rg.uniform(0.15, 0.9)
                mid_ = (a + b) * 0.5
                rows = []
                for st in (0.0, 0.5, 1.0):
                    shift = Vector((-sg * long * st, 0.0, 0.0)) + inward(mid_) * bend * st * st
                    rows.append([a.lerp(mid_, 0.45 * st) + shift, b.lerp(mid_, 0.45 * st) + shift])
                flat = [p for row in rows for p in row]
                faces = [(0, 1, 3, 2), (2, 3, 5, 4)]
                first = len(m.verts)
                m.add(flat, faces, paint, False, lambda mid: -inward(mid))
                for j in range(len(flat)):                     # (scorched: more toward its torn tip)
                    m.vsoot[first + j] = 0.35 + 0.2 * (j // 2)
                m.add([p + inward(p) * 0.03 for p in flat], faces, "torn", False, inward)

    def portholes(self, x0, x1, drop, every=3.2, r=0.17, skip=()):
        """A row of scuttles along each side, so far below the deck's edge."""
        if not self.m.fine:
            return
        x = x0
        while x <= x1:
            if not any(a <= x <= b for a, b in skip):
                for hand in (PORT, STBD):
                    z = self.h.deck_z(x) - drop
                    if z > 1.6:
                        p, n = self.h.side(x, z, hand)
                        self.m.use(self.half_of(p, hand)).disc(p + n * 0.025, n, r, "glass", 8)
            x += every

    def rails(self, x0, x1, inset=0.25, every=2.6, high=1.05, z_of=None, y_of=None, both=True):
        """Guard rails: stanchions with two wires."""
        if not self.m.fine:
            return
        m = self.m
        for hand in ((PORT, STBD) if both else (PORT,)):
            pts = []
            x = x0
            while x <= x1 + 1e-6:
                y = (y_of(x) if y_of else self.h.half_at(x) - inset) * hand
                z = z_of(x) if z_of else self.deck(x)
                pts.append(Vector((x, y, z)))
                x += every
            for a, b in zip(pts, pts[1:]):
                self.on(a.x)
                m.rod(a, a + Z * high, 0.03, "upper", sides=3, caps=(False, True), smooth=False)
                for hz in (high, high * 0.55):
                    m.rod(a + Z * hz, b + Z * hz, 0.018, "rope", sides=3, caps=(False, False), smooth=False)
            if pts:
                self.on(pts[-1].x)
                m.rod(pts[-1], pts[-1] + Z * high, 0.03, "upper", sides=3, caps=(False, True), smooth=False)

    def screws_and_rudder(self, screws=4, blades=4, r=2.3, rudders=1):
        """Under the stern: shafts, brackets, screws that turn, and the rudder."""
        m, h = self.m, self.h
        xs = h.frame_x(-0.86)
        out = [0.26, 0.55] if screws == 4 else ([0.0, 0.42] if screws == 3 else ([0.0] if screws == 1 else [0.38]))
        k = 0
        for share in out:
            for hand in ((PORT, STBD) if share > 0 else (PORT,)):
                k += 1
                y = hand * h.half_water(-0.8) * share * 1.05
                x = xs + (2.0 if share > 0.4 else -4.0)
                z = -h.keel(-0.86) - r * 0.35
                z = min(z, -r - 0.6)
                m.use("aft")
                m.rod((x + 16.0, y * 0.9, z + 1.6), (x + 0.3, y, z), 0.28, "antifoul", sides=6)
                m.rod((x + 1.2, y, z), (x + 1.6, y * 0.82, z + 2.2), 0.14, "antifoul", sides=4)
                m.rod((x + 1.2, y, z), (x + 1.6, y * 1.12, z + 2.2), 0.14, "antifoul", sides=4)
                name = m.part(f"screw.{k}", "aft", (x, y, z), axis=(1 if hand == PORT else -1, 0, 0), kind="spin",
                              note="a screw: turns while she is under way (outward-turning, about two turns a second at speed)")
                m.use(name)
                m.lathe((x + 0.3, y, z), -X, [(0.0, 0.34), (0.5, 0.3), (0.9, 0.05)], "brass", 8)
                for b in range(blades):
                    a = 2.0 * math.pi * b / blades
                    along = Vector((0, math.cos(a), math.sin(a)))
                    across = Vector((1, 0, 0)).cross(along)
                    rings = []
                    for sh, chord, twist in ((0.14, 0.2, 60.0), (0.45, 0.42, 42.0), (0.8, 0.4, 28.0), (1.0, 0.12, 22.0)):
                        t = math.radians(twist) * (1 if hand == PORT else -1)
                        flat = across * math.cos(t) + X * math.sin(t)
                        up = flat.cross(along)
                        mid = Vector((x, y, z)) + along * r * sh
                        rings.append([mid + flat * (math.cos(2 * math.pi * j / 6) * chord * r) + up * (math.sin(2 * math.pi * j / 6) * 0.03 * r) for j in range(6)])
                    m.skin(rings, "brass", (True, True))
                m.socket(f"wash.{k}", (x - 1.0, y, 0.0), (-1, 0, 0), "ship", "over a screw: the churned water rises here")
        # the rudder
        for i in range(rudders):
            y = 0.0 if rudders == 1 else (i - 0.5) * 2.0 * h.half_water(-0.85) * 0.45
            x = h.frame_x(-0.93)
            top = -h.keel(-0.93) + 0.15
            high = min(max(h.T * 0.62, 2.0), h.T + top - 0.2)
            long = high * 0.75
            name = m.part("rudder" if rudders == 1 else f"rudder.{i + 1}", "aft", (x, y, top), axis=(0, 0, -1), limits=[-35, 35], kind="turn",
                          note="the rudder: plus is helm to starboard")
            m.use(name)
            plan = [(x + long * 0.3, y), (x + long * 0.05, y + 0.22), (x - long * 0.7, y + 0.04), (x - long * 0.7, y - 0.04), (x + long * 0.05, y - 0.22)]
            lower = [(x + long * 0.22, y), (x + long * 0.02, y + 0.16), (x - long * 0.55, y + 0.03), (x - long * 0.55, y - 0.03), (x + long * 0.02, y - 0.16)]
            m.prism(lower, top - high, top, "antifoul", top=plan, bottom=True, smooth=False)

    # ---- things that stand on her -----------------------------------------------------------------------
    def house(self, x0, x1, half, z, high, paint="upper", r=0.8, y=0.0, top="deck", taper=0.0, part=None, rail=None, walls=True, square=(False, False),
              steps=None, smooth=False):
        """`steps`: how finely its corners are rounded (a house whose ends are half rounds, seen close, wants more than
        three); `smooth`: its walls are shaded as one round skin."""
        m = self.m.use(part) if part else self.on((x0 + x1) * 0.5)
        m.house(x0, x1, half, z, high, paint, r, y, top_paint=top, taper=taper, steps=(steps or 3) if self.m.fine else 1, square=square, smooth=smooth)
        fore, rear = max(x0, x1), min(x0, x1)
        if rail is None:
            rail = fore - rear >= 4.0 and half >= 2.4 and top == "deck"
        if m.fine and rail:
            plan = Model.round_rect(fore - 0.15, rear + 0.15, y - half + 0.15, y + half - 0.15, max(r - 0.15, 0.1), 2)
            n = len(plan)
            for k in range(n):
                a, b = Vector((*plan[k], z + high)), Vector((*plan[(k + 1) % n], z + high))
                steps = max(1, int((b - a).length / 2.4))
                for j in range(steps):
                    p, q = a + (b - a) * (j / steps), a + (b - a) * ((j + 1) / steps)
                    m.rod(p, p + Z * 1.0, 0.028, "upper", sides=3, caps=(False, True), smooth=False)
                    for hz in (1.0, 0.55):
                        m.rod(p + Z * hz, q + Z * hz, 0.016, "rope", sides=3, caps=(False, False), smooth=False)
        if m.fine and walls and high >= 2.2 and fore - rear > 6.0 and taper == 0.0:
            # doors, and a row of scuttles, on each side wall
            k = 0
            x = rear + r + 1.2
            while x < fore - r - 1.0:
                for hand in (PORT, STBD):
                    yw = y + hand * (half + 0.02)
                    if k % 4 == 1:
                        m.box((x, yw, z + 1.0), (0.75, 0.05, 1.85), "door")
                        m.box((x, yw + hand * 0.03, z + 1.25), (0.1, 0.04, 0.1), "steel")
                    else:
                        m.disc((x, yw + hand * 0.01, z + min(high - 0.7, 1.75)), (0, hand, 0), 0.16, "glass", 8)
                x += 2.3
                k += 1
        return z + high

    def windows(self, x, half, z, high=0.9, wide=None, y=0.0, back=None, part=None):
        """A band of bridge windows across a front (and a little way down each side)."""
        m = self.m.use(part) if part else self.on(x)
        w = wide or half * 1.8
        d = 1 if (back is None or back >= 0) else -1
        if not m.fine:
            m.box((x + d * 0.03, y, z + high / 2), (0.06, w, high), "glass")
        else:
            n = max(2, int(w / 0.95))
            pane = w / n
            for k in range(n):
                m.box((x + d * 0.03, y - w / 2 + (k + 0.5) * pane, z + high / 2), (0.06, pane - 0.14, high), "glass")
        if back:
            n = max(1, int(abs(back) / 0.95))
            pane = abs(back) / n
            for hand in (PORT, STBD):
                for k in range(n if m.fine else 1):
                    if m.fine:
                        m.box((x - d * (k + 0.5) * pane, y + hand * (half + 0.03), z + high / 2), (pane - 0.14, 0.06, high), "glass")
                    else:
                        m.box((x - back / 2, y + hand * (half + 0.03), z + high / 2), (abs(back), 0.06, high), "glass")

    def funnel(self, x, z, high, long, wide, lean=0.08, cap=0.1, y=0.0, pipes=True, name=None, flat=False):
        """`flat`: flat-sided, its plan a rectangle with rounded corners (an American carrier's) rather than an oval."""
        m = self.on(x)
        n = 20 if m.fine else 10
        dx = -high * lean
        if flat:
            plan0 = Model.round_rect(x + long * 0.5, x - long * 0.5, y - wide * 0.5, y + wide * 0.5, wide * 0.3, 3 if m.fine else 1)
            if m.fine:        # (points every metre or so along its straight walls: its soot runs down them in tongues)
                dense = []
                for (ax, ay), (bx, by) in zip(plan0, plan0[1:] + plan0[:1]):
                    steps = max(1, round(math.hypot(bx - ax, by - ay) / 1.05))
                    dense += [(lerp(ax, bx, i / steps), lerp(ay, by, i / steps)) for i in range(steps)]
                plan0 = dense
            plan1 = list(plan0)                                   # (upright walls: plates and platforms can be laid on them)
            n = len(plan0)
        else:
            plan0, plan1 = Model.oval(x, y, long * 0.5, wide * 0.5, n), Model.oval(x, y, long * 0.47, wide * 0.46, n)
        zc = z + high * (1.0 - cap)
        # (a flat-sided funnel is one big wall: more rings up it, so that its soot can be laid by height)
        ts = ((0.0, 0.34, 0.54, 0.66, 0.76, 0.85, 0.93, 1.0) if m.fine else (0.0, 0.62, 1.0)) if flat else (0.0, 0.5, 1.0)
        rings = [[Vector((px + dx * t, py, z + high * (1.0 - cap) * t)) for (px, py), (qx, qy) in zip(plan0, plan1) for px, py in [(lerp(px, qx, t), lerp(py, qy, t))]] for t in ts]
        first = len(m.verts)
        m.skin(rings, "upper", (False, False))
        if flat:
            # Her soot: the top third darkened, fading down in uneven tongues, further down the after side. (The art
            # director, 11:00 UTC on 18 November 2026: left to the weathering alone, which fades over the top half of
            # a three-ring wall, the funnel was "a plain slab".)
            for i, t in enumerate(ts):
                for j, q in enumerate(rings[i]):
                    aft = 0.5 - (q.x - x) / long
                    reach = 0.31 + 0.13 * aft + 0.1 * noise.noise(Vector((q.x * 0.3, q.y * 0.3, 4.2))) + 0.07 * noise.noise(Vector((q.x * 1.3, q.y * 1.3, 9.1)))
                    # (strong: a tint is laid in linear light, and one that halves the light darkens the picture by a quarter)
                    m.vsoot[first + i * n + j] = 0.9 * min(1.0, max(0.0, (t - (1.0 - reach)) / reach)) ** 0.9
        top = [Vector((p.x + dx * cap * 1.0, p.y, z + high)) for p in rings[-1]]
        m.skin([rings[-1], top], "black", (False, False))
        # the mouth: a dark ring inside, a little way down
        inner = [Vector((x + dx * (1.0 + cap) + (p.x - (x + dx * (1.0 + cap))) * 0.86, y + (p.y - y) * 0.86, z + high)) for p in top]
        m.add(top + inner, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], "black", False, lambda q: Z)
        low = [Vector((p.x, p.y, z + high - 1.6)) for p in inner]
        m.skin([low, inner], "black", (True, False), inside=True, cap_paint="black")
        # a band of soot round the top and down the after side comes from the weathering
        if pipes and m.fine:
            if flat:          # steam pipes up her after face, each to its own height, clipped to the wall, a dark lip at its head
                for off, r, over in ((-0.8, 0.16, 0.55), (0.42, 0.1, 0.25), (0.86, 0.13, -0.5)):
                    px, py = x - long * 0.5 - r - 0.1, y + off
                    m.rod((px, py, z), (px, py, z + high + over), r, "upper", sides=6)
                    m.lathe((px, py, z + high + over - 0.02), Z, [(0.0, r), (0.1, r * 1.45), (0.28, r * 1.45)], "black", 6)
                    for share in (0.22, 0.48, 0.74, 0.93):
                        m.box((px + (r + 0.15) * 0.5, py, z + high * share), (r + 0.15, r * 2.6, 0.07), "upper")
            for hand in (PORT, STBD) if not flat else ():
                m.rod((x + long * 0.5 - 0.1, y + hand * wide * 0.16, z), (x + long * 0.47 + dx, y + hand * wide * 0.16, z + high * 0.98), 0.12, "upper", sides=5)
                m.rod((x - long * 0.5 + 0.1, y + hand * wide * 0.16, z), (x - long * 0.47 + dx, y + hand * wide * 0.16, z + high * 0.9), 0.1, "upper", sides=5)
            # a grating over the mouth
            for j in range(-3, 4) if flat else range(-2, 3):      # (each bar reaches the mouth's rim: the mouth of a flat-sided funnel is as wide all along)
                span = wide * 0.88 if flat else wide * 0.8 * math.sqrt(max(0.05, 1.0 - (j * 0.3) ** 2))
                m.box((x + dx * (1 + cap) + j * long * (0.12 if flat else 0.14), y, z + high + 0.02), (0.08, span, 0.05), "steel")
        k = sum(1 for s in m.sockets if s["name"].startswith("funnel.")) + 1
        m.socket(name or f"funnel.{k}", (x + dx * (1 + cap), y, z + high + 0.3), (-0.25, 0, 1), self.half(x), "the funnel's mouth: her smoke rises here, more when she works up speed")
        self.soot_at.append((x + dx, y, z + high, long))
        return z + high

    soot_at = None

    def mast(self, x, z, high, lean=0.04, tripod=0.0, yards=((0.72, 0.16),), top=None, y=0.0):
        """A pole mast (or a tripod, its legs so far apart at the foot), with yards and a top."""
        m = self.on(x)
        head = Vector((x - high * lean, y, z + high))
        m.lathe((x, y, z), head - Vector((x, y, z)), [(0.0, 0.34 if tripod else 0.28), (high * 0.6, 0.22), (high, 0.09)], "upper", 7 if m.fine else 5)
        if tripod:
            j = Vector((x - high * lean * 0.62, y, z + high * 0.62))
            for hand in (PORT, STBD):
                m.rod((x - tripod, y + hand * tripod * 0.55, z), j, 0.2, "upper", sides=6 if m.fine else 4)
        for share, arm in yards:
            c = Vector((x - high * lean * share, y, z + high * share))
            m.rod(c + Y * high * arm, c - Y * high * arm, 0.07, "upper", rb=0.07, sides=4)
        if top:
            c = Vector((x - high * lean * top[0], y, z + high * top[0]))
            m.lathe(c - Z * 0.2, Z, [(0.0, top[1] * 0.75), (0.25, top[1]), (1.5, top[1]), (1.75, top[1] * 0.9)], "upper", 10 if m.fine else 6)
        return head

    def wire(self, a, b, r=0.022, sag=0.0):
        if not self.m.fine:
            return
        a, b = Vector(a), Vector(b)
        self.on(a.x)
        if sag <= 0:
            self.m.rod(a, b, r, "rope", sides=3, caps=(False, False), smooth=False)
            return
        pts = [a + (b - a) * (k / 6.0) - Z * sag * 4.0 * (k / 6.0) * (1.0 - k / 6.0) for k in range(7)]
        for p, q in zip(pts, pts[1:]):
            self.m.rod(p, q, r, "rope", sides=3, caps=(False, False), smooth=False)

    def boat(self, x, y, z, long, paint="boat", davits=True):
        m = self.on(x)
        rings = []
        for t in (-0.5, -0.42, -0.2, 0.1, 0.35, 0.5):
            w = long * 0.15 * max(0.05, 1.0 - abs(t / 0.5) ** 2.4) ** 0.6
            hgh = long * 0.11
            rings.append([Vector((x + t * long, y + w * c, z + hgh * (0.15 + s))) for c, s in ((1, 1), (0.92, 0.35), (0.5, 0.0), (0, -0.08), (-0.5, 0.0), (-0.92, 0.35), (-1, 1))])
        m.skin(rings, paint, (True, True), closed=False)
        # the canvas cover over her
        m.sheet([[r[0] + Z * 0.02, (r[0] + r[-1]) / 2 + Z * (0.02 + long * 0.035), r[-1] + Z * 0.02] for r in rings], "canvas", Z)
        if m.fine:
            for t in (-0.28, 0.28):
                m.box((x + t * long, y, z + 0.08), (0.25, long * 0.26, 0.16), "upper")

    def crane(self, x, y, z, high, reach, bearing=180.0):
        m = self.on(x)
        m.lathe((x, y, z), Z, [(0.0, 0.5), (high * 0.8, 0.32), (high, 0.2)], "upper", 7)
        a = math.radians(bearing)
        tip = Vector((x + math.cos(a) * reach, y + math.sin(a) * reach, z + high * 0.92))
        foot = Vector((x, y, z + high * 0.3))
        m.rod(foot, tip, 0.22, "upper", rb=0.12, sides=5)
        self.wire(Vector((x, y, z + high)), tip, 0.03)
        self.wire(tip, tip - Z * (high * 0.5), 0.025)

    def searchlight(self, x, y, z, r=0.55, facing=(1, 0, 0)):
        m = self.on(x)
        m.lathe((x, y, z), Z, [(0.0, 0.18), (0.5, 0.14)], "upper", 5)
        f = Vector(facing).normalized()
        m.lathe(Vector((x, y, z + 0.5 + r)) - f * r * 0.6, f, [(0.0, r * 0.7), (r * 0.35, r), (r * 1.1, r)], "upper", 10, caps=(True, False))
        m.disc(Vector((x, y, z + 0.5 + r)) + f * r * 0.5, f, r * 0.94, "glass", 10)

    def float_(self, x, y, z, facing, long=2.6, wide=1.2):
        """A life raft lashed to a turret's side or a bulkhead."""
        if not self.m.fine:
            return
        f = Vector(facing).normalized()
        side = f.cross(Z)
        verts, faces = [], []
        mid = Vector((x, y, z))
        ring = [mid + side * (math.cos(2 * math.pi * k / 10) * long / 2) + Z * (math.sin(2 * math.pi * k / 10) * wide / 2) for k in range(10)]
        for sc, off in ((1.0, 0.0), (1.0, 0.22), (0.62, 0.22), (0.62, 0.0)):
            verts += [mid + (p - mid) * sc + f * off for p in ring]
        for layer in range(3):
            faces += [(layer * 10 + k, layer * 10 + (k + 1) % 10, (layer + 1) * 10 + (k + 1) % 10, (layer + 1) * 10 + k) for k in range(10)]
        self.m.add(verts, faces, "cork", False, lambda q: (q - mid) - f * (q - mid).dot(f) * 0.0 + f * 0.3)

    def light_gun(self, x, y, z, barrels=2, tub=True, facing=0.0, long=1.9, slim=False):
        """Light anti-aircraft guns in a tub. `slim`: a single gun seen close and in the open (a submarine's): a slender
        pedestal in the ship's grey, a small body with its drum, shoulder rests."""
        m = self.on(x)
        if tub:
            m.tub((x, y, z), 1.35 if barrels > 1 else 1.0, 0.95, "upper", sides=12 if m.fine else 6, floor="deck")
        a = math.radians(facing)
        f = Vector((math.cos(a) * 0.8, math.sin(a) * 0.8, 0.6)).normalized()
        s = Vector((-math.sin(a), math.cos(a), 0))
        if slim and m.fine:
            m.lathe((x, y, z), Z, [(0.0, 0.3), (0.06, 0.3), (0.14, 0.11), (1.0, 0.08)], "upper", 12)
            at = Vector((x, y, z + 1.15))
            m.rod(at - f * 0.5, at + f * long, 0.028, "steel", sides=6)
            m.rod(at - f * 0.45, at + f * 0.3, 0.07, "steel", sides=8)                   # the body, and the drum on top of it
            up = s.cross(f)
            if up.z < 0:
                up = -up
            m.lathe(at - f * 0.05 + up * 0.07, up, [(0.0, 0.13), (0.11, 0.13)], "steel", 10)
            for hand in (PORT, STBD):                                                    # the shoulder rests
                m.rod(at - f * 0.45 + s * hand * 0.05, at - f * 0.8 + s * hand * 0.2, 0.018, "steel", sides=4)
                m.rod(at - f * 0.8 + s * hand * 0.2 - Z * 0.14, at - f * 0.8 + s * hand * 0.2 + Z * 0.06, 0.022, "steel", sides=4)
            m.rod(Vector((x, y, z + 1.0)), at - f * 0.05, 0.05, "upper", sides=6)
        else:
            m.lathe((x, y, z), Z, [(0.0, 0.35), (0.9, 0.22)], "steel", 6)
            for k in range(barrels):
                o = s * ((k - (barrels - 1) / 2.0) * 0.32)
                m.rod(Vector((x, y, z + 1.15)) + o - f * 0.4, Vector((x, y, z + 1.15)) + o + f * long, 0.05, "steel", sides=4)
            if m.fine:
                m.box(Vector((x, y, z + 1.15)), (0.7, 0.3 + 0.3 * barrels, 0.45), "steel", yaw=facing, pitch=35)
        n = sum(1 for sk in m.sockets if sk["name"].startswith("aa.")) + 1
        m.socket(f"aa.{n}", Vector((x, y, z + 1.15)) + f * long, f, self.half(x), "a light gun's muzzles: tracer starts here")

    def capstan(self, x, y, z):
        self.on(x).lathe((x, y, z), Z, [(0.0, 0.55), (0.25, 0.4), (0.7, 0.4), (0.8, 0.55), (0.95, 0.5)], "steel", 10)

    def bollards(self, x, y, z, k=1.0, sides=6):
        m = self.on(x)
        for d in (-0.5 * k, 0.5 * k):
            m.lathe((x + d, y, z), Z, [(0.0, 0.2 * k), (0.6 * k, 0.2 * k), (0.62 * k, 0.27 * k), (0.72 * k, 0.27 * k)], "steel", sides)

    def vent(self, x, y, z, r=0.45, high=1.4):
        m = self.on(x)
        m.lathe((x, y, z), Z, [(0.0, r * 0.6), (high * 0.7, r * 0.6)], "upper", 8)
        m.ball((x, y, z + high * 0.7), (r, r, r * 0.6), "upper", 8, 4, top_only=True)

    def anchor_gear(self, x_hawse, x_cap, spread=2.2, breakwater=None):
        """On the forecastle: the cables from the hawse pipes to the capstans, the anchors at the bows, a breakwater."""
        m, h = self.m, self.h
        for hand in (PORT, STBD):
            y0 = hand * min(spread, h.half_at(x_hawse) * 0.55)
            self.on(x_cap)
            pts = [Vector((lerp(x_hawse, x_cap, t), lerp(y0, hand * spread * 0.55, t), 0)) for t in [k / 8.0 for k in range(9)]]
            pts = [Vector((p.x, p.y, self.deck(p.x) + 0.02)) for p in pts]
            if m.fine:
                m.strip(pts, [Z] * len(pts), 0.14, "steel", 0.03)
            self.capstan(x_cap, hand * spread * 0.55, self.deck(x_cap))
            ka = max(0.42, min(1.0, h.B / 30.0))          # (a destroyer's anchor is less than half a battleship's)
            m.disc(Vector((x_hawse, y0, self.deck(x_hawse) + 0.04)), Z, 0.4 * ka, "black", 8)
            # the anchor, housed in its pipe on the bow
            p, n = h.side(x_hawse + 2.5 * ka, self.deck(x_hawse) - max(2.6 * ka, 1.3), hand)
            m.disc(p + n * 0.03, n, 0.55 * ka, "black", 8)
            m.box(p + n * 0.18 - Z * 0.7 * ka, (0.4 * ka, 0.35 * ka, 1.7 * ka), "steel", yaw=math.degrees(math.atan2(n.y, n.x)))
            m.box(p + n * 0.2 - Z * 1.5 * ka, (1.5 * ka, 0.3 * ka, 0.35 * ka), "steel", yaw=math.degrees(math.atan2(n.y, n.x)) + 90)
            self.rust_at.append((p.x, p.y, p.z - 0.3, 5.0 * ka))
        if breakwater:
            xb, wide, high = breakwater
            for hand in (PORT, STBD):
                a, b = Vector((xb + wide * 0.35, 0, self.deck(xb + wide * 0.35))), Vector((xb, hand * wide, self.deck(xb)))
                self.on(xb).add([a, b, b + Z * high, a + Z * high, a - X * 0.1, b - X * 0.1, b - X * 0.1 + Z * high, a - X * 0.1 + Z * high],
                                [(0, 1, 2, 3), (4, 5, 6, 7), (3, 2, 6, 7)], "upper", False, None)
                self.m.add([a, b, b + Z * high, a + Z * high], [(3, 2, 1, 0)], "upper", False, None)

    rust_at = None

    # ---- guns ------------------------------------------------------------------------------------------
    def turret(self, name, x, z, size, barrels, long, bearing=0.0, bore=0.2, raised=0.0, y=0.0, style="heavy", limits=(-145, 145),
               elev=(-3, 40), rate=5.0, rest_elev=3.0, floats=True, parent=None):
        """A gun turret on its barbette. It trains about its middle; each gun lifts on its trunnion and runs back.
        `bearing` is where it points at rest (0 ahead, 180 astern). Returns the names of its gun parts."""
        m = self.m
        half = parent or self.half(x)
        S = size
        base = z + raised
        if raised > 0 or style == "heavy":
            m.use(half)
            m.lathe((x, y, z - 0.05), Z, [(0.0, S * 0.47), (raised + 0.45, S * 0.47)], "upper", 20 if m.fine else 10, caps=(False, True))
        rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
        o = Vector((x, y, base + 0.45))

        def W(p):
            return o + rot @ Vector(p)
        tname = m.part(name, half, (x, y, base + 0.45), axis=(0, 0, -1), limits=list(limits), kind="turn",
                       note=f"trains: plus turns it clockwise seen from above, so its bearing is its rest bearing plus the turn (0 ahead, 90 to starboard, 180 astern); rest bearing {int(bearing)}; about {rate:g} degrees a second")
        m.parts[m.part_at[tname]].rest = bearing
        m.parts[m.part_at[tname]].rate = rate
        m.use(tname)
        if style == "heavy":
            Lf, Lr, hh = 0.66 * S, 0.80 * S, 0.30 * S
            lo = [(Lf, 0.30 * S), (0.22 * S, 0.5 * S), (-0.5 * S, 0.5 * S), (-Lr, 0.34 * S), (-Lr, -0.34 * S), (-0.5 * S, -0.5 * S), (0.22 * S, -0.5 * S), (Lf, -0.30 * S)]
            hi = [(Lf - 0.3 * S, 0.27 * S), (0.16 * S, 0.45 * S), (-0.5 * S, 0.45 * S), (-Lr + 0.04 * S, 0.31 * S), (-Lr + 0.04 * S, -0.31 * S), (-0.5 * S, -0.45 * S),
                  (0.16 * S, -0.45 * S), (Lf - 0.3 * S, -0.27 * S)]
        elif style == "medium":     # a cruiser's or a secondary turret: boxier, a rounded face
            Lf, Lr, hh = 0.62 * S, 0.62 * S, 0.36 * S
            lo = [(Lf, 0.26 * S), (0.36 * S, 0.5 * S), (-Lr, 0.5 * S), (-Lr, -0.5 * S), (0.36 * S, -0.5 * S), (Lf, -0.26 * S)]
            hi = [(Lf - 0.2 * S, 0.24 * S), (0.3 * S, 0.46 * S), (-Lr + 0.03 * S, 0.46 * S), (-Lr + 0.03 * S, -0.46 * S), (0.3 * S, -0.46 * S), (Lf - 0.2 * S, -0.24 * S)]
        elif style == "open":        # a submarine's deck gun: no shield at all, the gun in its cradle on a pedestal
            Lf, Lr, hh = 0.3 * S, 0.3 * S, 0.66 * S
            lo = hi = []
        elif style == "house":       # an American twin gun house: a tall slab-sided box, its face sloping back
            Lf, Lr, hh = 0.58 * S, 0.62 * S, 0.62 * S
            lo = [(Lf, 0.42 * S), (0.44 * S, 0.5 * S), (-Lr, 0.5 * S), (-Lr, -0.5 * S), (0.44 * S, -0.5 * S), (Lf, -0.42 * S)]
            hi = [(Lf - 0.27 * S, 0.4 * S), (0.2 * S, 0.48 * S), (-Lr + 0.02 * S, 0.48 * S), (-Lr + 0.02 * S, -0.48 * S), (0.2 * S, -0.48 * S), (Lf - 0.27 * S, -0.4 * S)]
        else:                        # a destroyer's gun in its shield, open at the back
            Lf, Lr, hh = 0.5 * S, 0.42 * S, 0.5 * S
            lo = [(Lf, 0.3 * S), (0.2 * S, 0.5 * S), (-Lr, 0.5 * S), (-Lr, -0.5 * S), (0.2 * S, -0.5 * S), (Lf, -0.3 * S)]
            hi = [(Lf - 0.22 * S, 0.26 * S), (0.1 * S, 0.44 * S), (-Lr, 0.44 * S), (-Lr, -0.44 * S), (0.1 * S, -0.44 * S), (Lf - 0.22 * S, -0.26 * S)]
        nlo = len(lo)
        if style == "open":
            # (the pedestal stands on what is 0.45 under the mounting's floor: the deck, or the round platform laid for it)
            m.lathe(W((0, 0, -0.45)), Z, [(0.0, 0.33 * S), (0.08, 0.33 * S), (0.2, 0.17 * S), (0.45 + hh * 0.5 - 0.14 * S, 0.14 * S)], "upper", 14 if m.fine else 6)
            if m.fine:
                m.box(W((0.0, 0, hh * 0.5 - 0.13 * S)), (0.36 * S, 0.3 * S, 0.08 * S), "upper", yaw=bearing)                 # the saddle, and a cheek each side of the cradle
                for hand in (PORT, STBD):
                    m.box(W((0.0, hand * 0.14 * S, hh * 0.5 - 0.03 * S)), (0.36 * S, 0.035 * S, 0.26 * S), "upper", yaw=bearing)
                    wheel = W((-0.15 * S, hand * 0.16 * S, hh * 0.5 - 0.06 * S))                                             # the layer's and the trainer's handwheels
                    side = rot @ Vector((0, hand, 0))
                    m.rod(wheel, wheel + side * 0.09 * S, 0.014 * S, "steel", sides=4)
                    m.lathe(wheel + side * 0.09 * S, side, [(0.0, 0.1 * S), (0.02 * S, 0.1 * S)], "steel", 10)
        else:
            vl = [W((p[0], p[1], 0.0)) for p in lo]
            vh = [W((p[0], p[1], hh)) for p in hi]
            mid = W((0, 0, hh * 0.5))
            m.add(vl + vh, [(k, (k + 1) % nlo, nlo + (k + 1) % nlo, nlo + k) for k in range(nlo)], "upper", False, lambda q: q - mid)
            ctr = W((sum(p[0] for p in hi) / nlo, 0, hh + 0.04 * S if style == "heavy" else hh + 0.02 * S))
            m.add(vh + [ctr], [(k, (k + 1) % nlo, nlo) for k in range(nlo)], "roof", False, lambda q: Z)
            m.add(vl + [W((0, 0, 0))], [(k, (k + 1) % nlo, nlo) for k in range(nlo)], "steel", False, lambda q: -Z)
        if style == "heavy" and m.fine:
            for hand in (PORT, STBD):     # the rangefinder's hoods on each side, the sighting hoods on the roof
                m.box(W((-0.38 * S, hand * 0.49 * S, hh * 0.62)), (0.16 * S, 0.1 * S, 0.1 * S), "upper", yaw=bearing)
                m.box(W((Lf - 0.42 * S, hand * 0.2 * S, hh + 0.045 * S)), (0.09 * S, 0.07 * S, 0.05 * S), "roof", yaw=bearing)
                if floats:
                    f = rot @ Vector((0, hand, 0))
                    c = W((-0.1 * S, hand * 0.475 * S, hh * 0.5))
                    self.float_(c.x, c.y, c.z, f, long=min(2.8, S * 0.3), wide=min(1.2, hh * 0.6))
            m.box(W((-Lr + 0.2 * S, 0, hh + 0.05 * S)), (0.12 * S, 0.3 * S, 0.06 * S), "roof", yaw=bearing)    # a ventilator at the back
        if style == "shield" and m.fine:
            m.box(W((-0.05 * S, 0, 0.2 * S)), (0.5 * S, 0.3 * S, 0.3 * S), "steel", yaw=bearing)                 # the breech and its mounting
        # the guns
        guns = []
        fwd = rot @ X
        lat = rot @ Y
        gap = S * (0.21 if barrels >= 4 else 0.27 if barrels == 3 else 0.42 if style == "house" else 0.3)
        tr_x = Lf - (0.36 * S if style == "heavy" else 0.3 * S)
        for k in range(barrels):
            off = (k - (barrels - 1) / 2.0) * gap
            piv = W((tr_x, off, hh * 0.5))
            gname = m.part(f"{name}.gun.{k + 1}", tname, piv, axis=tuple(-lat), limits=list(elev), slide=tuple(-fwd), slide_limits=[0.0, round(bore * 5.0, 2)], kind="turn",
                           note="a gun: plus lifts the muzzle; slides back along its own length when it fires")
            m.use(gname)
            e = math.radians(rest_elev)
            axis = (fwd * math.cos(e) + Z * math.sin(e)).normalized()
            reach = Lf - tr_x
            prof = [(0.0, bore * 1.9), (reach + long * 0.16, bore * 1.75), (reach + long * 0.18, bore * 1.35), (reach + long * 0.55, bore * 1.12), (reach + long * 0.97, bore * 0.92),
                    (reach + long * 0.975, bore * 1.02), (reach + long, bore * 1.0)]
            m.lathe(piv, axis, prof, "upper", 10 if m.fine else 6, caps=(True, False))
            tip = piv + axis * (reach + long)
            m.disc(tip - axis * 0.02, axis, bore * 0.98, "upper", 10 if m.fine else 6)
            m.disc(tip + axis * 0.005, axis, bore * 0.55, "black", 8 if m.fine else 6)
            if style == "open":         # her breech, behind the trunnions: it lifts and drops with the gun
                m.rod(piv - axis * 0.42 * S, piv, bore * 2.2, "steel", sides=10 if m.fine else 6)
                if m.fine:
                    m.box(piv - axis * 0.38 * S, (0.1 * S, bore * 4.6, bore * 4.6), "steel", yaw=bearing, pitch=rest_elev)
            if style not in ("shield", "open"):       # the canvas blast bag where the gun leaves the house
                m.lathe(piv + axis * (reach - 0.25 * bore), axis, [(0.0, bore * 2.7), (bore * 1.6, bore * 2.5), (bore * 5.0, bore * 1.75)], "bag", 10 if m.fine else 6, caps=(False, False))
            m.socket(f"muzzle.{name}.{k + 1}", tip, axis, gname, "flash, smoke and the shell leave here")
            guns.append(gname)
        return tname, guns

    def director(self, name, x, z, arm, y=0.0, r=None, high=None, bearing=0.0, parent=None):
        """A fire-control director: a round house with the arms of its rangefinder; it turns with the guns it directs."""
        m = self.m
        r = r or arm * 0.3
        high = high or arm * 0.32
        dn = m.part(name, parent or self.half(x), (x, y, z), axis=(0, 0, -1), kind="turn", note=f"a director: turns with the guns it serves; rest bearing {int(bearing)}")
        m.parts[m.part_at[dn]].rest = bearing
        m.use(dn)
        m.lathe((x, y, z), Z, [(0.0, r * 0.8), (0.2, r), (high, r * 0.96), (high + r * 0.25, r * 0.6)], "upper", 14 if m.fine else 8)
        rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
        lat = rot @ Y
        fwd = rot @ X
        c = Vector((x, y, z + high * 0.55))
        m.lathe(c - lat * arm, lat, [(0.0, arm * 0.07), (arm * 0.12, arm * 0.085), (arm * 1.88, arm * 0.085), (arm * 2.0, arm * 0.07)], "upper", 8 if m.fine else 5)
        for hand in (-1, 1):
            m.disc(c + lat * hand * arm * 0.93 + fwd * arm * 0.087, fwd, arm * 0.055, "glass", 6)
        return dn

    def tubes(self, name, x, z, n=4, long=7.4, parent=None, y=0.0, rest=0.0, limits=(-110, 110)):
        """Torpedo tubes on a turntable; they train out abeam to fire. `rest` is the bearing they lie on (0 ahead, 90 to starboard)."""
        m = self.m
        tn = m.part(name, parent or self.half(x), (x, y, z), axis=(0, 0, -1), limits=list(limits), kind="turn",
                    note=f"torpedo tubes: train out abeam to fire; plus turns them clockwise seen from above; rest bearing {int(rest)}")
        m.parts[m.part_at[tn]].rest = rest
        m.use(tn)
        rot = Matrix.Rotation(-math.radians(rest), 3, "Z")
        o = Vector((x, y, z))
        fwd = rot @ X

        def W(p):
            return o + rot @ Vector(p)
        m.lathe((x, y, z), Z, [(0.0, 1.5), (0.5, 1.5)], "upper", 12 if m.fine else 6)
        for k in range(n):
            off = (k - (n - 1) / 2.0) * 0.72
            m.lathe(W((-long / 2, off, 0.95)), fwd, [(0.0, 0.3), (long * 0.1, 0.33), (long, 0.33)], "upper", 8 if m.fine else 5)
            m.disc(W((long / 2 + 0.01, off, 0.95)), fwd, 0.27, "black", 8)
            m.socket(f"torpedo.{name}.{k + 1}", W((long / 2, off, 0.95)), fwd, tn, "a torpedo leaves here and splashes in abeam")
        m.box(W((-long * 0.1, 0, 1.45)), (1.6, n * 0.72, 0.5), "upper", yaw=-rest)
        return tn


def weather_for(yard):
    """Rust and soot for a ship in service: a little rust weeping down the sides, more toward the waterline and under
    the hawse pipes; soot on the funnel tops and what stands abaft them. Light: a ship in service, not a wreck."""
    h = yard.h
    rusty = getattr(yard, "rusty", 1.0)          # (a tramp steamer shows more rust than a ship of war)

    def w(p, n, paint, part):
        wear = soot = 0.0
        if paint in ("hull", "boot"):
            up = min(max(p.z / max(h.deck_z(p.x), 0.5), 0.0), 1.0)
            streak = max(0.0, noise.noise(Vector((p.x * 0.9, 7.3, 0.0))) * 1.5 - 0.35) * max(0.0, noise.noise(Vector((p.x * 0.07, 2.1, 0.0))) + 0.45)
            wear = min(0.26 * rusty, (streak * 0.55 * (1.0 - up * 0.7) + 0.04 * (1.0 - up)) * rusty)
            wear += 0.05 * rusty * max(0.0, noise.noise(Vector((p.x * 0.05, p.z * 0.4, 3.1))))
            for (x, y, z, reach) in yard.rust_at:
                if abs(p.x - x) < 0.9 and p.y * y > 0 and z - reach < p.z <= z + 0.5:
                    wear = max(wear, 0.5 * (1.0 - (z - p.z) / reach))
            soot = 0.06 * (0.5 + 0.5 * noise.noise(Vector((p.x * 0.05, 31.0, p.z * 0.2))))
        elif paint in ("antifoul",):
            wear = 0.12 * max(0.0, noise.noise(Vector((p.x * 0.2, p.y * 0.5, p.z * 0.5))))
        elif paint in ("upper", "roof"):
            soot = 0.04 * (0.5 + 0.5 * noise.noise(Vector((p.x * 0.11, p.y * 0.3, p.z * 0.3))))
            wear = 0.05 * max(0.0, noise.noise(Vector((p.x * 0.4, p.y * 0.4, p.z * 0.9))) * 1.5)
            for (x, y, z, long) in yard.soot_at:
                d = Vector((p.x - x, p.y - y, p.z - z))
                if -long * 4.0 < d.x < long * 0.6 and abs(d.y) < long and d.z < 3.0:
                    near = max(0.0, 1.0 - abs(d.z + 1.0) / 5.0) * max(0.0, 1.0 - max(0.0, -d.x - long * 0.4) / (long * 3.6))
                    soot = max(soot, 0.55 * near * (1.0 if d.x < long * 0.2 else 0.4))
        elif paint in ("planks", "deck", "flight"):
            soot = 0.07 * (0.5 + 0.5 * noise.noise(Vector((p.x * 0.09, p.y * 0.25, 0.0))))
        return wear, soot
    return w
