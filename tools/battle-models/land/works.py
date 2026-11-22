"""The works: where a vehicle is put together, given its clips, and sent out.

    v = Works("PanzerIV", "Panzer IV", "tank", "GER", fine)     # motorpool + gear + fittings
    ... build ...
    v.tank_clips(...)                                           # the clips every tank has
    ob, arm = to_scene(v.m)                                      # the Blender mesh on its skeleton, for pictures and export

PARTS of a tracked vehicle (names the game can rely on):
    vehicle (root, on the ground under her middle)
      hull (rocks on her springs) > turret (trains) > gun (lifts) > barrel (runs back)
                                  > hatch_commander, hatch_loader ...      hull > hatch_driver, hatch_radio ...
      wheel_l1.. wheel_r1.., sprocket_l/r, idler_l/r, roller_l1.. (kind spin; radius in `running`)
      track_l, track_r > track_l.001 ... (the links)
"""

import math
import random
import zlib

import bpy
import numpy as np
from mathutils import Quaternion, Vector

import rigid
from fittings import Fittings
from gear import Gear
from motorpool import LEFT, RIGHT, SIDES, X, Y, Z, Shop
from motorpool import frame as frame_of
from rigid import hex_rgb


# A nation whose marking changed with the months (the historian's ruling of 2026-11-18). Britain and the Commonwealth:
# no national mark at first, then the recognition flash (three equal upright stripes, white, red, white), then the
# white five-pointed Allied star.
MARK_MONTHS = {
    "UK": [{"to": "1941-10", "mark": "none"}, {"from": "1941-11", "to": "1943-06", "mark": "flash"}, {"from": "1943-07", "mark": "star"}],
}


# Where a crew paints its tank's name once she has come through three battles (Brian's pick b477, 2026-11-18; the
# historian's table historian/tank-names-b477.md; the art director's look art-director/battles/b477-battle-details-look.md).
# Each army as its crews really did it: a name on the hull or the turret, the Soviet slogan on the turret in its own
# letters, and for Germany no name but thin rings round the barrel. Armies that painted neither have no place.
NAME_INK = {"white": "E6E2D8", "black": "141414"}
NAME_HOW = ("Once her unit has come through three battles the game paints her crew's mark here. kind `name`: an invented name (four to ten letters "
            "to a line) in brush-painted capitals, never a printed face: the middle of it at `at` on the plate that looks along `facing`, "
            "the writing running along `along` with its top toward `up`, the letters `high` tall and the whole no longer than that place's `long`; it "
            "rides `part`, and a place is given for each side that carries one. A name too long for one line at `high`: on two lines where the place "
            "has `lines` 2 and the name two words (letters `high_two` tall, or `high` where that is not given), else painted smaller, but never "
            "under `least`; a name that fits neither way is not for this tank. `writing` down: the characters stand one under another, `long` the "
            "room downward. `script` japanese: her name is in Japanese characters, and only the names in brush-alphabet.json `japanese.names` have "
            "theirs drawn; for any other name paint nothing (never Latin letters on her); a name of a single character is `high_one` tall. "
            "kind `rings`: no name, but `most` thin bands round the barrel, the first at `at` and each next one `step` on along `toward` (to the breech), "
            "`wide` across, lying on the barrel whose radius there is `radius`; one ring, then more as she lasts. `ink`: for a name off-white on grey, "
            "olive and green coats and near-black on dark yellow; rings off-white on every coat. Matt, never brighter than her national marking; it "
            "wears and scorches with her paint.")
NAME_LEAST = 0.09                                           # (the art director: never smaller than this on a hull)


def name_ink(hexs):
    """Off-white on a dark coat, near-black on a pale or yellow one (the art director's rule)."""
    r, g, b = (int(hexs[i:i + 2], 16) for i in (0, 2, 4))
    return "black" if (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0 > 0.46 else "white"


class Works(Shop, Gear, Fittings):
    def root(self, hull_pivot=(0.0, 0.0, 0.9), rock=(-4.0, 4.0)):
        self.part("vehicle", None, (0, 0, 0), kind="root", note="on the ground under her middle; the game moves and turns this")
        self.part("hull", "vehicle", hull_pivot, axis=(0, -1, 0), limits=list(rock), kind="turn", note="rocks on her springs: + nose up (clips also roll and bob her)")
        return self

    # ---- clips every tracked or wheeled vehicle has -------------------------------------------------
    def ring_well(self, at, r, part="hull"):
        """What is under a turret: the race it turns on and the dark of the fighting compartment (seen only when the
        turret has been blown off her)."""
        self.ring_r = r
        self.lathe(at, Z, [(0.0, r - 0.05), (0.014, r - 0.05), (0.014, r), (0.0, r)], "steel", self.n(40, 12), part=part)
        self.lathe(at, Z, [(0.003, 0.0), (0.003, r - 0.05)], "black", self.n(40, 12), part=part)

    def running(self, xs, r, ty, sprocket, idler, rollers=(), wheel=None, track=None, drive="front", top_on_wheels=False, per_axle=None,
                sprocket_kw=None, idler_kw=None, roller_kw=None, wheel_z=None, wheel_parent=None, sprung=None):
        """Both sides' running gear in one go: road wheels at `xs` (radius r, track centre `ty` out from the middle),
        the sprocket and idler as (x, z, r), return rollers as (x, z, r), and the track round them all. `drive` says
        which end the sprocket is at. With no rollers and `top_on_wheels`, the top run lies on the road wheels.
        `per_axle(i)` gives extra road-wheel settings for axle i (interleaved wheels alternate). `wheel_parent(letter, i)`
        names the part a road wheel hangs from (a sprung arm of its own, made before this is called) in place of the vehicle.
        `sprung` (down, up), metres: every road wheel hangs from a part of its own (`axle_<l><n>`) that slides up and down
        on its spring by that much, and the file's `suspension` table lists them, so a game can work each wheel over the ground."""
        wheel = dict(wheel or {})
        track = dict(track or {})
        z = r if wheel_z is None else wheel_z
        table = []
        for letter, s in SIDES:
            self.use("vehicle")
            for i, x in enumerate(xs):
                kw = dict(wheel)
                if per_axle:
                    kw.update(per_axle(i))
                hangs = wheel_parent(letter, i) if wheel_parent else "vehicle"
                if sprung and not wheel_parent:
                    hangs = f"axle_{letter}{i + 1}"
                    self.part(hangs, "vehicle", (x, s * ty, z), slide=(0, 0, 1), slide_limits=[-sprung[0], sprung[1]], axis=Y, kind="slide",
                              note="a road wheel's spring: + lifts the wheel (metres)")
                    table.append({"part": hangs, "wheel": f"wheel_{letter}{i + 1}", "side": "left" if s > 0 else "right", "x": round(x, 3), "limits": [-sprung[0], sprung[1]]})
                self.road_wheel(f"wheel_{letter}{i + 1}", hangs, x, s * ty, z, r, s, **kw)
                self.use("vehicle")
        if table:
            self.m.info.setdefault("file", {})["suspension"] = {
                "kind": "slides", "wheels": table,
                "how": "each road wheel hangs on a spring of its own: slide its part up by how far the ground under that wheel stands above the level she rests "
                       "on (metres, inside its limits), and the wheel rides up with it. Leave them at 0 on level ground."}
        for letter, s in SIDES:
            self.use("vehicle")
            self.sprocket(f"sprocket_{letter}", "vehicle", sprocket[0], s * ty, sprocket[1], sprocket[2], s, **(sprocket_kw or {}))
            self.idler(f"idler_{letter}", "vehicle", idler[0], s * ty, idler[1], idler[2], s, **(idler_kw or {}))
            for i, (x, zz, rr) in enumerate(rollers):
                self.roller(f"roller_{letter}{i + 1}", "vehicle", x, s * ty, zz, rr, s, **(roller_kw or {}))
            front, rear = (sprocket, idler) if drive == "front" else (idler, sprocket)
            ground = [(x, z, r) for x in sorted(xs)]
            if rollers:
                top = sorted([tuple(q) for q in rollers], key=lambda q: -q[0])
            elif top_on_wheels:
                top = [(x, z, r) for x in sorted(xs, reverse=True)]
            else:
                top = []
            self.track(letter, s, s * ty, ground + [tuple(front)] + top + [tuple(rear)], **track)
            self.use("vehicle")
            back = min(min(xs) - r, rear[0] - rear[2])
            self.socket("dust." + ("left" if s > 0 else "right"), (back - 0.1, s * ty, 0.05), (-1, 0, 0.3), part="vehicle")

    def skirts(self, x0, x1, y, z0, z1, count=5, thick=0.006, gap=0.012, part="hull", paint="upper", rail=True):
        """Thin skirt plates hung along each side over the top of the tracks, on a rail."""
        if not self.fine:
            for s in (LEFT, RIGHT):
                self.slab((x0, s * y, z0), (x1, s * (y + thick), z1), paint, part=part)
            return
        step = (x1 - x0) / count
        for s in (LEFT, RIGHT):
            for k in range(count):
                a, b = x0 + k * step + gap / 2, x0 + (k + 1) * step - gap / 2
                self.slab((a, s * y, z0 + self.rng.uniform(-0.006, 0.006)), (b, s * (y + thick), z1), paint, 0.002, part=part)
                for xx in (a + 0.12, b - 0.12):
                    self.slab((xx - 0.02, s * (y - 0.03), z1 - 0.07), (xx + 0.02, s * (y + thick + 0.004), z1 - 0.02), "shade", part=part)
            if rail:
                self.slab((x0, s * (y - 0.035), z1 - 0.055), (x1, s * (y - 0.02), z1 - 0.03), "shade", part=part)

    def fan_grille(self, c, r, up=Z, part=None, bars=6):
        """A round grille over an engine fan: a ring, the dark below, a mesh of bars."""
        c, up = Vector(c), Vector(up).normalized()
        self.lathe(c, up, [(0.0, r - 0.04), (0.02, r - 0.04), (0.02, r), (0.0, r)], "upper", self.n(28, 10), part=part)
        self.lathe(c, up, [(0.004, 0.0), (0.004, r - 0.04)], "black", self.n(20, 8), part=part)
        if self.fine:
            _, a, b = frame_of(up)
            for k in range(bars):
                t = -1.0 + 2.0 * (k + 0.5) / bars
                half = math.sqrt(max(0.0, 1.0 - t * t)) * (r - 0.04)
                for u, w in ((a, b), (b, a)):
                    self.rod(c + up * 0.012 + w * (t * (r - 0.04)) - u * half, c + up * 0.012 + w * (t * (r - 0.04)) + u * half, 0.006, "upper", 4, caps=False, part=part)
            self.ball(c + up * 0.012, r * 0.16, "upper", 8, 5, part=part, dome=True, squash=0.6)

    def mark(self, tag, at, facing, size=0.22, part="hull", both=False, shown=None):
        """A national marking for the game to paint here (and the pictures show): each nation's plain period marking.
        Where a nation's marking changed with the months (MARK_MONTHS) the file carries the table as `mark_months`,
        for the game to paint the one that fits the campaign's month; `shown` is the one the pictures draw."""
        f = Vector(facing).normalized()
        self.m.marks.append({"tag": tag, "at": [round(at[0], 3), round(-at[1], 3), round(at[2], 3)], "facing": [round(f.x, 3), round(-f.y, 3), round(f.z, 3)],
                             "radius": size, "part": part, "both": both, **({"shown": shown} if shown else {})})
        if tag in MARK_MONTHS:
            self.m.info.setdefault("file", {})["mark_months"] = {"how": "paint the mark whose months hold the campaign's month (from and to are both counted in); `none` is bare paint.",
                                                                 tag: MARK_MONTHS[tag]}

    def camouflage(self, **kw):
        """The coat's second colour (`upper2`) sprayed over about a third of her in big uneven soft-edged blotches
        (camo.py): for the vehicles every small nation shares, which the game paints in each nation's own two colours.
        The last thing a builder does: nothing may be added to her after it."""
        import camo
        from motorpool import NATION
        self.m.camo = camo.lay3(self.m, **kw) if kw.pop("three", False) else camo.lay(self.m, **kw)
        if self.tag != "_" and NATION.get(self.tag, (None, None))[1] is None:
            # (built for a nation with one coat colour, France's own 75 among them: the second is the first, and she stays one coat)
            self.m.paints["upper2"] = self.m.paints["upper"]
        return self.m.camo

    # ---- the crew's name, or their rings (b477) -----------------------------------------------------------------
    def _skin(self, part=None):
        """The mesh as she stands, to cast rays at (one part's faces only, if a part is named)."""
        from mathutils.bvhtree import BVHTree
        m = self.m
        key = (len(m.faces), part)
        if getattr(self, "_skin_key", None) != key:
            if part is None:
                ids = list(range(len(m.faces)))
            else:
                want = m.part_at[part]
                ids = [i for i, p in enumerate(m.fpart) if p == want]
            self._skin_key, self._skin_ids = key, ids
            self._skin_tree = BVHTree.FromPolygons([tuple(p) for p in m.verts], [tuple(m.faces[i]) for i in ids])
        return self._skin_tree, self._skin_ids

    def _name_file(self, kind, **kw):
        d = self.m.info.setdefault("file", {}).setdefault("name_place", {"how": NAME_HOW, "when": "her unit has come through three battles", "kind": kind})
        coat = self.m.paints.get("upper", "565C3A")
        ink = {"use": name_ink(coat), **NAME_INK}
        swaps = self.m.info.get("file", {}).get("swaps", {}).get("paint", {}).get("choices", {})
        if swaps:
            ink["by_coat"] = {k: name_ink(c["paints"]["upper"]) for k, c in swaps.items()}
        d["ink"] = ink
        d.update(kw)
        return d

    def name_place(self, near, long=0.9, high=0.12, part="hull", sides=(LEFT, RIGHT), lines=1, writing="across", reach=(1.3, 0.4), parts=None, bend=0.014, note="",
                   high_two=None, script=None, high_one=None, ink=None):
        """Where her crew paints her name: a flat run of her own painted plate on the side of `part`, as near to
        `near` (x, z) as one is found, `long` by `high` (times `lines`; writing "down": `high` across and `long` down).
        Found on the mesh as she stands, so call it when she is built. The game paints the word; the file carries the place.
        reach: how far from `near` to look, along and up. parts: other parts whose plate will do as well.
        near may be {LEFT: (x, z), RIGHT: (x, z)} when her two sides are not alike.
        lines: how many lines the place has room for. (2, 1) asks for two lines' room (their letters `high_two` tall)
        where a place nearly as long has it, and for one line's where none has: a name over ten letters goes on two lines.
        bend: how far the plate may leave the flat under the word (a cast turret's side is never flat: the game lays
        the letters on the skin, as it does her marking).
        script: "japanese" says the word is in Japanese characters: only the names whose characters brushhand has
        (`brushhand.JAPANESE`) are painted, any other not at all. high_one: how tall a name of a single character is
        painted (the place is found with room for it). ink: "white" or "black" where the record overrules the coat's rule."""
        m = self.m
        tree, ids = self._skin()
        ok_parts = {m.part_at[p] for p in ([part] + list(parts or ()))}
        ok_paints = {m.paint_at[p] for p in m.paints if p.split(".")[0] in COATS}
        step = 0.03
        nx, nz = int(reach[0] / step), int(reach[1] / step)
        wants = tuple(lines) if isinstance(lines, (tuple, list)) else (lines,)
        high_two = high_two or high

        def room(n_lines):                                  # (along, up) for so many lines
            if writing == "down":
                return max(high * 1.25, (high_one or 0.0) * 1.12), long
            return long, (high * 1.25 if n_lines == 1 else high_two * (1.25 * n_lines + 0.1 * (n_lines - 1)))

        places = []
        both = near
        for s in sides:
            near = both[s] if isinstance(both, dict) else both
            grid = {}
            for i in range(-nx, nx + 1):
                for j in range(-nz, nz + 1):
                    x, z = near[0] + i * step, near[1] + j * step
                    hit = tree.ray_cast(Vector((x, s * 6.0, z)), Vector((0.0, -s, 0.0)), 6.0)
                    if hit[0] is None:
                        continue
                    f = ids[hit[2]]
                    if m.fpart[f] in ok_parts and m.fpaint[f] in ok_paints and hit[1].y * s > 0.45:
                        grid[(i, j)] = (hit[0], hit[1])
            taken = []
            for mk in m.marks:                              # her national marking keeps its own room
                on = mk.get("part", "hull")
                if (on == part or on in (parts or ())) and (mk.get("both") or -mk["at"][1] * s > 0):
                    taken.append((mk["at"][0], mk["at"][2], mk["radius"] + 0.06))

            def clear(pc, nc, half_run, half_up, fs=0.012):
                """A closer look at a place that passed: nothing stands off the plate across it (a hand rail, a
                hinge, a box's edge), which the coarse look can miss between its rows."""
                up_ = (Z - nc * Z.dot(nc)).normalized()
                run_ = up_.cross(nc)
                na, nb = int(half_run / fs), int(half_up / fs)
                bad, tot, rows = 0, 0, []
                for b in range(-nb, nb + 1):
                    row = []
                    for a in range(-na, na + 1):
                        hit = tree.ray_cast(pc + run_ * (a * fs) + up_ * (b * fs) + nc * 0.5, -nc, 1.2)
                        tot += 1
                        if hit[0] is None or m.fpart[ids[hit[2]]] not in ok_parts or m.fpaint[ids[hit[2]]] not in ok_paints:
                            bad += 1
                            row.append(None)
                            continue
                        row.append(hit[3])
                        if (row[-2:-1] and row[-2] is not None and abs(row[-2] - hit[3]) > 0.016) or (rows and rows[-1][len(row) - 1] is not None and abs(rows[-1][len(row) - 1] - hit[3]) > 0.016):
                            bad += 1
                    rows.append(row)
                return bad <= 0.006 * tot

            def search(along_len, up_len, least_shrink):
                for shrink in (1.0, 0.88, 0.76, 0.64, 0.52):
                    if shrink < least_shrink:
                        return None
                    hx, hz = max(1, math.ceil(along_len * shrink / 2 / step - 0.2)), max(1, math.ceil(up_len / 2 / step - 0.2))
                    passed = []
                    for (i, j), (pc, nc) in grid.items():
                        x, z = near[0] + i * step, near[1] + j * step
                        if any(abs(x - tx) < hx * step + r and abs(z - tz) < hz * step + r for tx, tz, r in taken):
                            continue
                        flat, n, nsum = 0, 0, Vector((0, 0, 0))
                        for a in range(-hx, hx + 1):
                            for b in range(-hz, hz + 1):
                                n += 1
                                g = grid.get((i + a, j + b))
                                if g is not None and abs((g[0] - pc).dot(nc)) < bend and g[1].dot(nc) > (0.93 if bend < 0.02 else 0.86):
                                    flat += 1
                                    nsum += g[1]
                        if flat >= 0.97 * n:
                            passed.append((math.hypot(i * step, 1.6 * j * step), i, j, pc, nsum.normalized()))
                    for d, _, _, pc, nc in sorted(passed, key=lambda q: q[:3]):
                        if clear(pc, nc, along_len * shrink / 2 + 0.06, up_len / 2):          # (a hand's breadth clear at each end: no word hard against a door or a pack)
                            return (d, pc, nc, shrink)
                return None

            best, got = None, wants[-1]
            for k, n_lines in enumerate(wants):             # the roomier place first, if it is nearly as long
                best = search(*room(n_lines), 0.0 if k == len(wants) - 1 else 0.86)
                if best is not None:
                    got = n_lines
                    break
            if best is None:
                continue
            _, at, n, shrink = best
            up = (Z - n * Z.dot(n)).normalized()
            run = up.cross(n)
            f3 = lambda p: [round(p[0], 3), round(-p[1], 3), round(p[2], 3)]            # (the file's y is to the right)
            places.append({"side": "left" if s > 0 else "right", "at": f3(at), "facing": f3(n), "along": f3(run), "up": f3(up), "part": part,
                           "long": round(long * shrink, 3), "lines": got})
        if places:
            more = {}
            if any(p["lines"] > 1 for p in places) and high_two != high:
                more["high_two"] = high_two
            if high_one:
                more["high_one"] = high_one
            if script:
                more.update(script=script)
                note = (note + " " if note else "") + "Only the names in brush-alphabet.json `japanese.names` have their characters drawn: for any other, paint nothing here."
            if note:
                more["note"] = note
            d_ = self._name_file("name", high=high, least=NAME_LEAST, lines=max(p["lines"] for p in places), writing=writing, places=places, **more)
            if ink:
                d_["ink"] = {"use": ink, **NAME_INK}
        return places

    def kill_rings(self, part, muzzle="muzzle", most=5, wide=0.025, step=0.075, note=""):
        """Where a German crew paints its rings: round the barrel, behind the muzzle (and its brake), where the tube
        runs even. part: the barrel's part (it runs back with the gun, and the rings with it)."""
        m = self.m
        tree, ids = self._skin(part)
        sk = next(s for s in m.sockets if s["name"] == muzzle)
        at, d = Vector(sk["at"]), Vector(sk["facing"]).normalized()
        side = d.cross(Z).normalized()

        def radius(t):
            hit = tree.ray_cast(at - d * t + side * 0.6, -side, 0.6)
            return None if hit[0] is None else 0.6 - hit[3]
        need = (most - 1) * step + wide + 0.06
        t, found = 0.04, None
        while t < 2.2 and found is None:
            rs = [radius(t + k * 0.02) for k in range(int(need / 0.02) + 1)]
            if all(r is not None and 0.025 < r < 0.2 for r in rs) and max(rs) - min(rs) < 0.012 and all(abs(q - r) < 0.0012 for q, r in zip(rs, rs[1:])):
                found = t
            t += 0.02
        if found is None:
            return None
        first = found + 0.03 + wide / 2
        f3 = lambda p: [round(p[0], 3), round(-p[1], 3), round(p[2], 3)]
        ring = {"part": part, "muzzle": muzzle, "at": f3(at - d * first), "toward": f3(-d), "radius": [round(radius(first + k * step), 4) for k in range(most)],
                "wide": wide, "step": step, "most": most}
        d_ = self._name_file("rings", **({"note": note} if note else {}))
        d_["ink"] = {"use": "white", **NAME_INK}             # (off-white on every coat: dark rings on a yellow barrel read as its joints)
        d_.setdefault("rings", []).append(ring)
        return ring

    def spring(self, t, kick, freq=2.4, damp=3.2):
        """How a sprung hull answers a knock at t = 0: a rocking that dies away."""
        return kick * math.exp(-damp * t) * math.sin(2 * math.pi * freq * t)

    def hull_keys(self, long, pitch=None, roll=None, bob=None, fps=30, surge=None):
        """Keys for the hull from functions of time (degrees, degrees, metres): one `rot` and one `move` track.
        `surge` is the hull thrown along her length on her springs (metres, + forward)."""
        hp = self.m.parts[self.m.part_at["hull"]]
        rot_keys, mv = [], []
        n = max(2, int(round(long * fps)))
        for i in range(n + 1):
            t = long * i / n
            q = Quaternion(hp.axis, math.radians(pitch(t) if pitch else 0.0)) @ Quaternion(X, math.radians(roll(t) if roll else 0.0))
            rot_keys.append([t, q.w, q.x, q.y, q.z])
            mv.append([t, surge(t) if surge else 0.0, 0.0, bob(t) if bob else 0.0])
        return {"part": "hull", "rot": rot_keys, "move": mv}

    def sooted(self, reach=0.6, most=0.85):
        """Soot round every exhaust outlet and up the plate above it, point by point (the file's `soot`)."""
        pipes = [Vector(s["at"]) for s in self.m.sockets if s["name"].startswith("exhaust")]

        def weather(p, n, paint, part):
            if part.startswith(("track", "wheel", "sprocket", "idler", "roller")):
                return 0.0, 0.0
            s = 0.0
            for at in pipes:
                d = Vector(p) - at
                d.z *= 0.4 if d.z > 0.0 else 1.5
                s = max(s, max(0.0, 1.0 - max(0.0, d.length - 0.05) / reach) ** 1.8)
            return 0.0, s * most
        if pipes:
            self.m.weather = weather
        return self

    def worn(self, dust=1.0, grime=1.0, cast=None, colour=None, layers=1, foot=0.95, top=1.45, tracks=0.27, marks=0.75, shade=False, gear=0.5, rim=False, disc=0.55, tyre=0.45, lie=0.24,
             shade_deep=None, ground=None):
        """What a vehicle in the field carries, in the file itself (the battle draws no textures: a point's colour is
        all there is). Dust, point by point, in the file's `dust` (`wear` stays paint chipped to the metal): thick on her running gear and the foot of her
        hull, thinning fast between `foot` and `top` (metres up) so that her upper plates stay her own paint, lying a
        little on whatever looks up, less on what faces the way she goes and more on her tail, in uneven patches and
        never a level line. What `dust` goes toward is `dust_colour`, chosen for her coat (`dirt_for`), or `colour`.
        layers=2 (a vehicle that wears a coat of the dust's own tone, or may by a paint swap): what lies on her upper
        faces goes in a channel of its own, `haze`, toward `haze_colour`, so that the dirt low on her can be damp
        earth and the dust on top a pale haze. Her tracks carry the dirt too (`tracks`), and her national marks are
        dulled by it (`marks`: each mark's own `dust`). A wheel's dirt is read in the wheel's own place, so wheels of
        one shape carry the same. rim: a wheel goes round, so its dirt cannot lie low on it; it is packed where a wheel
        keeps it, round the rim's edge and in the ring of hub bolts, and the open face between is nearer her paint
        (the art director, 22 Nov: dirt on a wheel must not read as a flat tint). rim may be three shares of the wheel's
        radius, for a wheel whose rings of points stand elsewhere: the bolts' ring out to the first, the rim's edge from
        the second to the third. disc: how much of the dirt the open face keeps; tyre: how much a rubber tyre keeps.
        lie: how much dust lies on what looks up.
        And on `soot`, over what the exhaust and the worn edges already lay there: her paint a little uneven, plate by
        plate, as weeks of sun, rain and hands leave it. `cast(point, part)` says where she is a casting: a casting's
        skin is uneven in patches a foot or two across, lighter and darker, and sits a shade under the rolled plate.
        Soft, all of it: no streak fine enough to read as wood grain. shade: the shadow in her creases too (`shaded`),
        shade_deep how dark the deepest crease goes (1 - shade_deep; the E7 pass: 0.42, so 0.58), with the pass's knee, bounce and film. ground: her home ground
        (GROUND_DIRT: field, desert, italy, steppe, winter, tropic): her dirt in its colour, and every ground's in the file's
        `dust_by_ground`. Call it after her last piece is on; plates must have been cut finer (`dice`) for it to show on them."""
        self._worn = True
        base = self.m.weather
        rs = random.Random(zlib.crc32((self.name + ":worn").encode()))

        def field(lo, hi, n=7):
            ws = []
            for _ in range(n):
                d = Vector((rs.gauss(0, 1), rs.gauss(0, 1), rs.gauss(0, 1) * 0.6)).normalized()
                ws.append((d * (2.0 * math.pi / rs.uniform(lo, hi)), rs.uniform(0.0, 2.0 * math.pi)))
            return lambda p: 0.5 + 0.5 * sum(math.sin(k.dot(p) + ph) for k, ph in ws) / math.sqrt(n) * 1.25
        big, small, plates = field(0.9, 2.2), field(0.28, 0.6), field(1.4, 3.0)
        skin = field(0.45, 0.95, 6)
        coat_hex = self.m.paints.get("upper", "565C3A")
        low_c, top_c = ground_dirt(ground, coat_hex) if ground else dirt_for(coat_hex)
        two = layers == 2 and (not ground or top_c not in (low_c, coat_hex.upper()))
        self.m.info["dust"] = colour or low_c
        file = self.m.info.setdefault("file", {})
        file["dust_colour"] = self.m.info["dust"]
        file["dust_is"] = ("road dirt: `dust` (0 to 100 a point) is how far the point's finished paint has gone to `dust_colour`; thick low on "
                           "her, thin by her shoulders, on her tracks too. (`wear` is still paint chipped to the metal, as it was.) A mark's own "
                           "`dust` dulls it the same way: paint the mark, then take it that far to `dust_colour`. Work it out on the coat she "
                           "wears (after a paint swap; a swap's choice may name its own dust_colour and haze_colour), from these numbers: the "
                           ".glb's COLOR_0 is the same thing worked out for the coat she was built in. A game on other ground may use that "
                           "ground's own colour. Lay no other dust, sun fade or camouflage of the game's own over a file that has `dust`.")
        if two:
            self.m.info["haze"] = top_c
            file["haze_colour"] = top_c
            file["dust_is"] += (" `haze` (0 to 100, where the mesh has it) is the thin pale dust lying on what looks up: after `dust`, take the "
                                "point that far to `haze_colour`.")
        if ground:
            file["home_ground"] = ground
            file["dust_by_ground"] = {"how": GROUND_DIRT_HOW, **{g: dict(zip(("dust_colour", "haze_colour"), ground_dirt(g, coat_hex))) for g in GROUND_DIRT}}
        for mk in self.m.marks:
            mk["dust"] = round(100 * marks * dust)
        takes = ("upper", "upper2", "upper3", "shade", "rubber", "canvas", "cloth", "wood", "steel", "leather", "rope", "rust")
        spots = [(Vector(c), r, least) for c, r, least in getattr(self, "dusted", [])]
        spins = {p.name: p.pivot for p in self.m.parts if p.kind == "spin"}
        reach = {}                                      # (how far each wheel's own points stand from its axle)
        if rim:
            for f, k in zip(self.m.faces, self.m.fpart):
                part = self.m.parts[k]
                if part.kind == "spin" and part.axis is not None:
                    for i in f:
                        q = Vector(self.m.verts[i]) - part.pivot
                        reach[part.name] = max(reach.get(part.name, 0.0), (q - part.axis * q.dot(part.axis)).length)
        axes = {p.name: p.axis for p in self.m.parts if p.kind == "spin" and p.axis is not None}
        # (where a wheel keeps its dirt, as shares of its radius: the hub's bolts out to the first, the rim's edge from the second to the third)
        rim_at = rim if isinstance(rim, (tuple, list)) else (0.42, 0.68, 0.92)

        def weather(p, n, paint, part):
            w, s = base(p, n, paint, part) if base else (0.0, 0.0)
            kind = paint.split(".")[0]
            p = Vector(p)
            if part.startswith("track"):
                # (every link goes round and round through the same dirt: all alike, a little uneven)
                if kind in ("track", "rubber", "steel"):
                    return w, s, 0.0, min(0.6, tracks * dust * (0.75 + 0.5 * small(p)))
                return w, s, 0.0, 0.0
            if kind not in takes:
                return w, s, 0.0, 0.0
            if part in spins:
                # (a wheel: its dirt read in its own place, so that every wheel of one shape carries the same and the game can draw them as one mesh)
                p = p - spins[part] + Vector((0.0, 0.0, 0.5))
            patch = min(1.5, max(0.25, 0.1 + 1.7 * big(p))) if part not in spins else 0.9 + 0.7 * (small(p) - 0.5)
            low = max(0.0, min(1.0, (1.95 - p.z) / 1.5)) ** 1.25 * 0.95
            t = max(0.0, min(1.0, (p.z - foot) / max(top - foot, 1e-3)))
            low *= 1.0 - 0.75 * t * t * (3.0 - 2.0 * t)
            lying = lie * max(0.0, n[2]) ** 2 * (0.4 + 1.2 * small(p))
            face = (1.0 - 0.6 * max(0.0, n[0]) if p.x > 0.0 else 1.0) * (1.0 + 0.15 * max(0.0, -n[0]))
            # (the wind of her going keeps her face cleaner, and her tail rides in her own dust)
            d, lying = low * patch * face, lying * face
            if not two:
                d += lying
            if part.startswith(("wheel", "sprocket", "idler", "roller", "arm", "axle")):
                d = max(d, gear * patch)                # (her running gear is never cleaner than this)
                if part in reach:
                    q = p - Vector((0.0, 0.0, 0.5))
                    rn = (q - axes[part] * q.dot(axes[part])).length / max(reach[part], 1e-3)
                    ramp = max(0.0, min(1.0, (rn - rim_at[1]) / max(rim_at[2] - rim_at[1], 1e-3)))
                    d *= 0.9 if rn < 0.2 else 1.4 if rn < rim_at[0] else disc + (1.55 - disc) * ramp * ramp * (3.0 - 2.0 * ramp)
            if kind == "rubber":
                d *= tyre                               # (a tyre's tread is wiped clean every turn; the dirt stays in its walls)
            for c, r, least in spots:                   # (what hangs on her and is never wiped: spare links)
                if (p - c).length_squared < r * r:
                    d = max(d, least * (0.7 + 0.6 * small(p)) * (0.6 + 0.6 * max(0.0, n[2])))
            g = 0.0
            if kind in COATS or kind == "upper3":
                g = 0.17 * max(0.0, min(1.0, 2.4 * plates(p) - 0.75)) + 0.10 * max(0.0, small(p) - 0.5)
                if cast and cast(p, part):
                    x = 0.05 + 0.36 * (max(0.0, min(1.0, skin(p))) - 0.5)
                    if x >= 0.0:
                        d, g = max(d, x), 0.0
                    else:
                        g = max(g * 0.5, -2.0 * x)
            return w, max(s, min(0.38, g * grime)), (min(0.45, lying * dust) if two else 0.0), min(0.88, d * dust)
        self.m.weather = weather
        if shade:
            # (the E7 pass, shade_deep given: the creases to 1 - shade_deep, an open face left clear (knee), the ground's
            # own light under her (bounce) and no film along a long plate or trail (film), so her lit side darkens by
            # under the art director's 8%: 1.4 to 7.3% on the seven theatre vehicles built so far)
            self.shaded(**({"deep": shade_deep, "knee": 0.3, "bounce": 0.4, "film": 0.3} if shade_deep is not None else {}))
        return self

    def shaded(self, reach=0.5, deep=0.7, rays=20, knee=0.0, bounce=1.0, film=0.0):
        """The shadow in her creases, point by point, in the file's `shade` (1 in the open, less where little of the
        sky is seen: between her road wheels, under her track guards, round the turret ring, under the gun's shield).
        The battle's light casts no such small shadows, so the file carries them: the game multiplies a point's colour
        by its `shade`. Worked out as she stands at rest, each point looking out over the half of the sky its facing
        sees, as far as `reach`. A part that goes round sees only itself, and every ring about its axle takes one even
        shade, so nothing dark goes round with a wheel and wheels of one shape carry the same; a track's links take none.
        knee: how much of the sky may be shut before a point darkens at all (the E7 pass: an open face keeps 0.95 or
        more, the shade gathers in the creases); bounce: what a ray stopped by the ground counts (the ground gives back
        its own light: under 1, a face low by the ground is not darkened as a crease is). 0 and 1: as before."""
        m = self.m

        def bake(me, vpart):
            from mathutils.bvhtree import BVHTree
            nv = len(m.verts)
            out = np.ones(nv)
            g = 30.0
            ground = [(-g, -g, 0.0), (g, -g, 0.0), (g, g, 0.0), (-g, g, 0.0)]
            tree = BVHTree.FromPolygons([tuple(v) for v in m.verts] + ground, [tuple(f) for f in m.faces] + [(nv, nv + 1, nv + 2, nv + 3)])
            normals = np.zeros(nv * 3)
            me.vertices.foreach_get("normal", normals)
            normals = normals.reshape(-1, 3)
            rs = random.Random(7)
            dirs = []
            for k in range(rays):                           # (spread evenly over the half sphere, more of them near the facing)
                z = math.sqrt((k + 0.5) / rays)
                a = 2.399963 * k
                r = math.sqrt(1.0 - z * z)
                dirs.append((r * math.cos(a), r * math.sin(a), z))
            track = [p.name.startswith("track") for p in m.parts]
            # (a part that goes round sees only itself: wheels of one shape then carry the same shade and can be drawn as one mesh)
            own = {}
            for k, part in enumerate(m.parts):
                if part.kind == "spin":
                    fs = [f for f, fp in zip(m.faces, m.fpart) if fp == k]
                    if fs:
                        ids = sorted({i for f in fs for i in f})
                        at = {i: j for j, i in enumerate(ids)}
                        own[k] = BVHTree.FromPolygons([tuple(m.verts[i]) for i in ids], [tuple(at[i] for i in f) for f in fs])
            whole = tree
            nf = len(m.faces)
            for i in range(nv):
                if track[vpart[i]]:
                    continue
                tree = own.get(int(vpart[i]), whole)
                n = Vector(normals[i])
                if n.length_squared < 1e-9:
                    continue
                n.normalize()
                u = n.cross(Vector((0.0, 0.0, 1.0)) if abs(n.z) < 0.9 else Vector((1.0, 0.0, 0.0))).normalized()
                v = n.cross(u)
                p = Vector(m.verts[i]) + n * 0.012
                shut = 0.0
                for a, b, c in dirs:
                    hit = tree.ray_cast(p, u * a + v * b + n * c, reach)
                    if hit[0] is not None:
                        shut += (1.0 - 0.6 * hit[3] / reach) * (bounce if tree is whole and hit[2] >= nf else 1.0)
                occ = min(1.0, shut / rays * 1.15)
                if knee > 0.0:
                    occ = max(0.0, (occ - knee) / (1.0 - knee))
                out[i] = 1.0 - deep * occ
            if film > 0.0:                              # (no film: a point on a long edge dimmed the less)
                longest = np.zeros(nv)
                for e in me.edges:
                    a, b = e.vertices
                    d = (Vector(m.verts[a]) - Vector(m.verts[b])).length
                    longest[a] = max(longest[a], d)
                    longest[b] = max(longest[b], d)
                out = 1.0 - (1.0 - out) * np.clip(film / np.maximum(longest, 1e-6), 0.0, 1.0)
            for k, part in enumerate(m.parts):          # (a part that goes round: every ring of it one even shade)
                if part.kind == "spin" and part.axis is not None:
                    idx = np.nonzero(vpart == k)[0]
                    rings = {}
                    for i in idx:
                        q = Vector(m.verts[i]) - part.pivot
                        along = q.dot(part.axis)
                        rings.setdefault((round(along / 0.012), round((q - part.axis * along).length / 0.012)), []).append(i)
                    for ring in rings.values():
                        out[ring] = out[ring].mean()
            return np.clip(out, 1.0 - deep, 1.0)
        m.shade = bake
        m.info.setdefault("file", {})["shade_is"] = ("`shade` (0 to 1 a point, where the mesh has it, as the soldiers' files have it) is how open the point is to "
                                                     "the sky: multiply the point's finished colour by it. The shadow in her creases, which the battle's light does not cast.")
        return self

    def tank_clips(self, recoil=0.35, kick=0.9, gun_low=-8.0, hatches=(), traverse=14.0, idle_cue="tank_idle_loop", drive_cue="tank_loop",
                   gun_cue="tank_gun_01", blow=(-0.9, 2.9, 0.42, 172.0), has_turret=True, mg="coax", exhausts=None, fire_sockets=None, open_lids=1):
        """The clips a tank has. Nothing here moves her over the ground or trains her turret: the game does that (the
        root, the `turret` and `gun` parts, the wheels and the track clips). These are what her own body does.

        recoil: metres the barrel runs back; kick: degrees the hull rocks to the shot; gun_low: where the gun droops to
        when she is knocked out; hatches: lids that are thrown open when she is knocked out, the commander's first
        (open_lids: how many of them `open_up` opens for him: one, or both halves of a lid in two);
        blow: where the turret comes to rest when her ammunition goes (back, to the left, up, turned over degrees)."""
        m = self.m
        ex = exhausts or [s["name"] for s in m.sockets if s["name"].startswith("exhaust")]
        fires = fire_sockets or [s["name"] for s in m.sockets if s["name"].startswith("fire.")]
        two_pi = 2 * math.pi
        ev = lambda t, fx, socket=None, cue=None: {"t": round(t, 3), "fx": fx, "socket": socket, "cue": cue}

        # idle: the engine ticking over shakes her a very little
        m.clips["idle"] = {"long": 2.0, "loop": True, "about": "engine running, standing still",
                           "tracks": [self.hull_keys(2.0, roll=lambda t: 0.05 * math.sin(two_pi * 5 * t / 2.0 * 2), bob=lambda t: 0.0012 * math.sin(two_pi * 7 * t / 2.0 * 2))],
                           "events": [ev(0.0, "engine_idle", "engine", idle_cue)] + [ev(0.0, "exhaust_idle", e) for e in ex]}
        m.clips["start"] = {"long": 1.5, "loop": False, "after": "hold", "about": "the engine is started: she shudders, a gout of smoke from the exhaust",
                            "tracks": [self.hull_keys(1.5, roll=lambda t: 0.35 * math.exp(-2.2 * t) * math.sin(two_pi * 6 * t), bob=lambda t: 0.004 * math.exp(-2.0 * t) * math.sin(two_pi * 8 * t))],
                            "events": [ev(0.0, "engine_start", "engine", "tank_start")] + [ev(0.15, "exhaust_start", e) for e in ex]}
        # drive: she pitches and rolls gently as the ground passes under her (whole turns in the loop, so it joins)
        m.clips["drive"] = {"long": 2.0, "loop": True, "about": "under way: the hull's own motion (the game rolls the wheels, runs the tracks and moves her)",
                            "tracks": [self.hull_keys(2.0, pitch=lambda t: 0.38 * math.sin(two_pi * t / 2.0 * 2) + 0.16 * math.sin(two_pi * t / 2.0 * 5 + 1.0),
                                                      roll=lambda t: 0.3 * math.sin(two_pi * t / 2.0 * 1 + 0.6) + 0.12 * math.sin(two_pi * t / 2.0 * 4),
                                                      bob=lambda t: 0.006 * math.sin(two_pi * t / 2.0 * 3 + 0.3) + 0.003 * math.sin(two_pi * t / 2.0 * 7))],
                            "events": [ev(0.0, "engine_drive", "engine", drive_cue)] + [ev(0.0, "exhaust_drive", e) for e in ex]
                            + [ev(0.0, "dust_trail", "dust.left"), ev(0.0, "dust_trail", "dust.right")]}
        m.clips["move_off"] = {"long": 1.2, "loop": False, "after": "hold", "about": "she moves off: the nose lifts as the tracks bite, and settles",
                               "tracks": [self.hull_keys(1.2, pitch=lambda t: 1.3 * math.exp(-3.0 * t) * math.sin(two_pi * 1.1 * t) * (1.0 if t > 0 else 0.0), bob=lambda t: 0.004 * math.exp(-3 * t) * math.sin(two_pi * 2.0 * t))],
                               "events": [ev(0.0, "exhaust_rev", e) for e in ex]}
        # stop: the nose goes down as the brakes bite and stays down while she slows; as she comes to rest (`rest`
        # seconds in) it comes back up, a little over, and settles
        rest, dip = 1.3, 1.6

        def braking(t):
            if t < rest:
                return -dip * rigid.smooth(t / 0.45) * (1.0 - 0.12 * rigid.smooth((t - 0.45) / (rest - 0.45)))
            a = t - rest
            return -dip * 0.88 * math.exp(-3.0 * a) * math.cos(two_pi * 0.85 * a)
        m.clips["stop"] = {"long": 2.6, "loop": False, "after": "hold", "rest": rest,
                           "about": "she brakes: start it as the brakes go on. The nose dips and stays down while she slows; she is at rest `rest` seconds in "
                                    "(play it faster or slower to fit), and the nose comes back up, a little over, and settles",
                           "tracks": [self.hull_keys(2.6, pitch=braking, bob=lambda t: -0.012 * rigid.smooth(t / 0.45) if t < rest else -0.012 * math.exp(-3.0 * (t - rest)) * math.cos(two_pi * 0.85 * (t - rest)))],
                           "events": [ev(0.0, "brakes", "dust.left"), ev(0.0, "brakes", "dust.right")]}

        # fire: the barrel runs back and out again; the hull's answer is a clip of its own
        # (back in a twentieth of a second, held a moment on the buffer, then run out slowly by the recuperator: a full second)
        def run_back(t):
            if t < 0.05:
                return recoil * rigid.smooth(t / 0.05)
            if t < 0.13:
                return recoil
            return recoil * max(0.0, 1.0 - rigid.smooth((t - 0.13) / 1.0))
        keys = [[t, round(run_back(t), 4)] for t in (0.0, 0.0125, 0.025, 0.0375, 0.05, 0.13, 0.2, 0.28, 0.36, 0.45, 0.55, 0.63, 0.72, 0.8, 0.9, 1.0, 1.08, 1.13, 2.5)]
        m.clips["fire"] = {"long": 2.5, "loop": False, "after": "hold", "about": "the main gun fires: flash and blast at 0.0, the barrel runs back at once and out again over a second, the empty case a moment after",
                           "tracks": [{"part": "barrel", "slide": keys}],
                           "events": [ev(0.0, "muzzle_flash_big", "muzzle", gun_cue), ev(0.0, "muzzle_blast_dust", "muzzle")]
                                     + ([ev(1.5, "case_clang", "case", "tank_shell_case")] if any(s["name"] == "case" for s in m.sockets) else [])}   # (a machine-gun tankette throws no case)
        # (the hull rocks back about `kick` degrees a fifth of a second after the shot, and has settled a second after it)
        m.clips["fire.rock"] = {"long": 1.6, "loop": False, "after": "hold",
                                "about": "the hull's answer to the shot, for a gun trained ahead: she is thrown back on her springs, the nose lifts about a degree, and she settles in a "
                                         "second. With the turret trained off, turn this about the turret's bearing or play it weaker",
                                "tracks": [self.hull_keys(1.6, pitch=lambda t: kick * 2.25 * math.exp(-3.6 * t) * math.sin(two_pi * 1.0 * t),
                                                          bob=lambda t: -0.008 * math.exp(-4 * t) * math.sin(two_pi * 2.0 * t),
                                                          surge=lambda t: -0.045 * kick * math.exp(-4.5 * t) * math.sin(two_pi * 1.1 * t))],
                                "events": []}
        if mg and any(s["name"] == mg for s in m.sockets):
            m.clips["mg_burst"] = {"long": 1.0, "loop": False, "after": "hold", "about": "a burst from the machine gun beside the main gun: ten rounds",
                                   "tracks": [], "events": [ev(0.0, "muzzle_flash", mg, "tank_mg_burst")] + [ev(0.075 * k, "muzzle_flash", mg) for k in range(1, 10)]}
        if any(s["name"] == "hull_mg" for s in m.sockets):
            m.clips["hull_mg_burst"] = {"long": 1.0, "loop": False, "after": "hold", "about": "a burst from the hull machine gun",
                                        "tracks": [], "events": [ev(0.0, "muzzle_flash", "hull_mg", "tank_mg_burst")] + [ev(0.075 * k, "muzzle_flash", "hull_mg") for k in range(1, 10)]}
        roof = getattr(self, "roof", None)
        if roof and any(s["name"] == "roof_mg" for s in m.sockets):
            shots = int(round(1.0 / roof["every"]))
            m.clips["roof_mg_burst"] = {"long": 1.0, "loop": False, "after": "hold",
                                        "about": "a burst from the roof machine gun: only while her commander is up in his hatch (open_up) with his hands on "
                                                 "grip.roof_mg.l and grip.roof_mg.r, and never with the roof_mg swap off (no gun on her roof). "
                                                 + ("The .50 at about 550 rounds a minute" if roof["kind"] == "m2" else "The MG 34 at about 850 rounds a minute"),
                                        "tracks": [], "events": [ev(0.0, "muzzle_flash", "roof_mg", "tank_mg_burst")] + [ev(roof["every"] * k, "muzzle_flash", "roof_mg") for k in range(1, shots)]}
        # hit but still fighting
        m.clips["hit"] = {"long": 0.9, "loop": False, "after": "hold", "about": "struck, not stopped: she jolts",
                          "tracks": [self.hull_keys(0.9, pitch=lambda t: 0.8 * math.exp(-5 * t) * math.sin(two_pi * 2.2 * t), roll=lambda t: 1.1 * math.exp(-5 * t) * math.sin(two_pi * 1.9 * t + 0.4))],
                          "events": [ev(0.0, "hit_sparks", None, "shell_impact_near")]}

        # hatches
        def lid_keys(name, t0, fast=False):
            p = m.parts[m.part_at[name]]
            top = p.limits[1]
            if fast:    # thrown open: it flies up, strikes its stop and bounces
                return [[0.0, 0.0], [t0, 0.0], [t0 + 0.10, top * 0.75], [t0 + 0.16, top], [t0 + 0.24, top * 0.86], [t0 + 0.34, top], [t0 + 0.42, top * 0.95], [t0 + 0.5, top]]
            return [[0.0, 0.0], [t0, 0.0], [t0 + 0.25, top * 0.35], [t0 + 0.6, top * 0.92], [t0 + 0.75, top]]

        def lifted(name):
            """The part a lid is lifted on before it swings (hatch_swing's `<name>_lift`), or None."""
            lift = name + "_lift"
            return lift if lift in m.part_at and m.parts[m.part_at[name]].parent == m.part_at[lift] else None

        def lid_tracks(name, t0, fast=False):
            """A lid's tracks: its turn, and for a lid that is lifted up its post before it swings, the lift first."""
            lift = lifted(name)
            if not lift:
                return [{"part": name, "turn": lid_keys(name, t0, fast)}]
            up, dt = m.parts[m.part_at[lift]].slide_limits[1], (0.08 if fast else 0.15)
            return [{"part": lift, "slide": [[0.0, 0.0], [t0, 0.0], [t0 + dt, up]]}, {"part": name, "turn": lid_keys(name, t0 + dt, fast)}]
        hs = [h for h in hatches if h in m.part_at]
        if hs:
            m.clips["open_up"] = {"long": 1.0, "loop": False, "after": "hold", "about": "the commander's hatch is opened (she travels opened up; play it backward to close down for action)",
                                  "tracks": [tr for h in hs[:open_lids] for tr in lid_tracks(h, 0.1)], "events": [ev(0.1, "hatch_open", "commander")]}
        # knocked out: struck, the gun droops, the crew's hatches are thrown open, smoke, then fire from the engine
        tr = [self.hull_keys(3.0, pitch=lambda t: 1.0 * math.exp(-4.5 * t) * math.sin(two_pi * 2.0 * t) - 0.5 * rigid.smooth(t / 1.2),
                             roll=lambda t: 1.4 * math.exp(-4.5 * t) * math.sin(two_pi * 1.7 * t + 0.3) + 1.2 * rigid.smooth(t / 1.5),
                             bob=lambda t: -0.02 * rigid.smooth(t / 1.0))]
        if "gun" in m.part_at:
            tr.append({"part": "gun", "turn": [[0.0, 0.0], [0.25, 0.0], [0.9, gun_low * 0.8], [1.2, gun_low], [1.35, gun_low * 0.93], [1.5, gun_low]]})
        for k, h in enumerate(hs):
            tr += lid_tracks(h, 0.35 + 0.22 * k, fast=(k == 0))
        m.clips["knocked_out"] = {"long": 3.0, "loop": False, "after": "hold",
                                  "about": "knocked out: struck at 0.0, hatches thrown open as the crew bail out, the gun droops, smoke, then fire at the engine; holds its last pose (follow with `burn`)",
                                  "tracks": tr,
                                  "events": [ev(0.0, "hit", None, "tank_knocked_out"), ev(0.35, "hatch_blown", "commander"), ev(0.5, "smoke_burst", "engine"),
                                             ev(0.9, "bail_out", "commander"), ev(1.2, "fire_start", "engine", "fire_start"), ev(1.6, "smoke_loop", "engine")]}
        m.clips["burn"] = {"long": 2.0, "loop": True, "about": "burning: fire at the engine and out of her hatches, black smoke (loop after knocked_out or brew_up)",
                           "tracks": [], "events": [ev(0.0, "fire_loop", "engine", "fire_loop")] + [ev(0.0, "fire_loop", f) for f in fires] + [ev(0.0, "smoke_loop", "engine")]}
        # brew up: her ammunition goes. A jet of flame from the hatches, then the blast lifts the turret off its ring
        if has_turret and "turret" in m.part_at:
            back, left, rest, over = blow
            ring = m.parts[m.part_at["turret"]].pivot
            t_blast, t_fly = 0.55, 1.25
            g = 9.81
            land_z = rest - ring.z                         # how far below the ring the turret's own origin comes to rest
            vz = (land_z + 0.5 * g * t_fly * t_fly) / t_fly
            axis = Vector((1.0, 0.25, 0.0)).normalized()
            rk, mk = [[0.0, 1.0, 0.0, 0.0, 0.0], [t_blast, 1.0, 0.0, 0.0, 0.0]], [[0.0, 0.0, 0.0, 0.0], [t_blast, 0.0, 0.0, 0.0]]
            n = 14
            for i in range(1, n + 1):
                a = i / n
                t = a * t_fly
                q = Quaternion(axis, math.radians(over * a ** 0.9))
                rk.append([t_blast + t, q.w, q.x, q.y, q.z])
                mk.append([t_blast + t, back * a, left * a, vz * t - 0.5 * g * t * t])
            # it strikes, bounces a hand's breadth and lies
            q = Quaternion(axis, math.radians(over + 6.0))
            rk += [[t_blast + t_fly + 0.12, q.w, q.x, q.y, q.z]]
            mk += [[t_blast + t_fly + 0.12, back * 1.04, left * 1.05, land_z + 0.10]]
            q = Quaternion(axis, math.radians(over + 3.0))
            rk += [[t_blast + t_fly + 0.26, q.w, q.x, q.y, q.z], [4.0, q.w, q.x, q.y, q.z]]
            mk += [[t_blast + t_fly + 0.26, back * 1.06, left * 1.08, land_z], [4.0, back * 1.06, left * 1.08, land_z]]
            tr = [{"part": "turret", "rot": rk, "move": mk},
                  self.hull_keys(4.0, pitch=lambda t: (1.6 * math.exp(-4 * (t - t_blast)) * math.sin(two_pi * 1.6 * (t - t_blast)) if t > t_blast else 0.0),
                                 roll=lambda t: (1.2 * math.exp(-4 * (t - t_blast)) * math.sin(two_pi * 1.4 * (t - t_blast) + 0.5) if t > t_blast else 0.0),
                                 bob=lambda t: (-0.05 * math.exp(-5 * (t - t_blast)) * math.sin(two_pi * 2.2 * (t - t_blast)) - 0.03 * rigid.smooth((t - t_blast) / 0.6) if t > t_blast else 0.0))]
            for k, h in enumerate(hs):
                tr += lid_tracks(h, 0.05 + 0.04 * k, fast=True)
            m.clips["brew_up"] = {"long": 4.0, "loop": False, "after": "hold",
                                  "about": "her ammunition goes: flame jets from the hatches, then the blast throws the turret off its ring to lie beside her; holds its last pose (follow with `burn`)",
                                  "tracks": tr,
                                  "events": [ev(0.0, "hit", None, "tank_knocked_out"), ev(0.05, "flame_jet", "fire.2")] + [ev(0.1, "flame_jet", f) for f in fires if f != "fire.2"]
                                  + [ev(t_blast, "magazine_blast", "ring", "explosion_debris"), ev(t_blast + 0.05, "fire_loop", "ring", "fire_loop"), ev(t_blast + t_fly, "turret_lands", "ring", "shell_impact_near"),
                                     ev(t_blast + 0.3, "smoke_column", "ring")]}
        if m.weather is None:
            self.sooted()
        if self.dice:
            self.worn(**getattr(self, "worn_kw", {}))
        m.info.setdefault("file", {})["controls"] = {
            "turret": {"part": "turret", "what": "trained by the game: + to the right (clockwise from above)", "degrees_per_second": traverse},
            "gun": {"part": "gun", "what": "laid by the game inside its limits: + muzzle up"},
            "hatches": hs,
        }
        lifts = {h: lifted(h) for h in hs if lifted(h)}
        if lifts:
            m.info["file"]["controls"]["hatch_lifts"] = {
                **lifts, "what": "these lids do not flip up: each is lifted up its post (slide the part named, to its slide_limits' top) and then "
                                 "swung flat about the post (turn the hatch, + opens). Open: lift, then swing; close: swing back, then lower. "
                                 "The clips open_up, knocked_out and brew_up already do it so."}
        if roof and "roof_mg" in m.part_at:
            m.info["file"]["controls"]["roof_mg"] = {
                "part": "roof_mg", "lay": "roof_mg_cradle", "degrees_per_second": 60.0, "kind": roof["kind"],
                "what": "aimed by the game inside the two parts' limits, and only while her commander is up (open_up played) with his hands on "
                        "grip.roof_mg.l and grip.roof_mg.r: roof_mg + to the right, roof_mg_cradle + muzzle up. With no commander up it is not fired, "
                        "and with the file's roof_mg swap on `off` she has no roof gun at all: never aim it or play roof_mg_burst. "
                        + ("The post stands in front of his hatch: he stays facing ahead and his hands follow the grips."
                           if roof["kind"] == "m2" else
                           "The mount rides the rail round his cupola, so roof_mg turns about the cupola's middle: turn him with it (his facing is roof_mg's bearing). "
                           "Draw the gun itself (roof_mg_cradle) only while he mans it, and scale it to nothing otherwise (crews kept the MG 34 inside "
                           "till they needed it; parked anywhere, its stock would hang over the open hatch where he stands): the carriage, roof_mg, stays.")}
        return self

    def running_table(self):
        """What the game needs to roll her: every spinning part with its radius, and each track's links and pitch."""
        return {"wheels": [{"part": n, "radius": round(r, 4)} for n, r in self.wheels], "tracks": list(self.belts.values()),
                "how": "turn each wheel by (distance run on its side) / radius, in radians; play tracks.left and tracks.right at (speed of that side) / stride * long"}


# ---- to Blender --------------------------------------------------------------------------------------

def _math(nt, op, a=None, b=None, clamp=False, c=None):
    n = nt.nodes.new("ShaderNodeMath")
    n.operation, n.use_clamp = op, clamp
    for k, v in enumerate((a, b, c)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[k].default_value = v
        else:
            nt.links.new(v, n.inputs[k])
    return n.outputs[0]


def _mix(nt, blend="MIX", fac=1.0):
    n = nt.nodes.new("ShaderNodeMix")
    n.data_type, n.blend_type = "RGBA", blend
    n.inputs[0].default_value = fac
    return n, n.inputs[0], n.inputs[6], n.inputs[7], n.outputs[2]


def _noise(nt, vec, scale, detail=4.0, rough=0.55, stretch=None):
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = scale
    nz.inputs["Detail"].default_value = detail
    nz.inputs["Roughness"].default_value = rough
    if stretch:
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = stretch
        nt.links.new(vec, mp.inputs["Vector"])
        vec = mp.outputs["Vector"]
    nt.links.new(vec, nz.inputs["Vector"])
    return nz.outputs[0]


def _ramp(nt, fac, lo, hi, a=0.0, b=1.0):
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.clamp = True
    mr.inputs["From Min"].default_value, mr.inputs["From Max"].default_value = lo, hi
    mr.inputs["To Min"].default_value, mr.inputs["To Max"].default_value = a, b
    nt.links.new(fac, mr.inputs["Value"])
    return mr.outputs["Result"]


EARTH = "6F5F45"         # damp earth: the dirt from the ground up on a coat too near the road dust's own tone for dust to show on it
HAZE = "B0AB9B"          # and the thin pale dust that lies on such a coat's upper faces: a grey haze, not more yellow
DUST = "9A8A6A"          # the dust of the battle stage's road: what she carries unless the picture says other ground

# The dirt of each ground, from the ground up (the art director's E7 surface pass, 22 Nov 17:55Z: "dirt rises from the
# ground up in the ground's colour, as a by-ground setting"): (low, haze, other). Low is what she carries on her running
# gear and the foot of her hull; haze is the thin dust lying on what looks up; other is the same ground in another tone,
# for a coat too near the low one. Each is checked against her coat by the 9-step lightness rule (ground_dirt).
GROUND_DIRT = {
    "field": (DUST, HAZE, EARTH),                 # road dust, its grey haze, damp earth (France, the Low Countries, China's roads)
    "desert": ("9C845C", "C2AA78", "7A6648"),     # sand packed low, pale on what looks up, darker sand
    "italy": ("A09682", "C4BCA8", "857A66"),      # the pale Italian dust
    "steppe": ("463C32", "9A9282", "6A5E4C"),     # black steppe earth low, a grey dust on top in the dry, brown earth
    "winter": ("4E463C", "A9A9A3", "6E665A"),     # slush and mud
    "tropic": ("5E4634", None, "4A3426"),         # red-brown mud low (on a dark coat the wetter, darker mud) and no haze on top: a wet climate (the art director, 22 Nov 20:54Z: the Philippines, Guadalcanal)
}
GROUND_DIRT_HOW = ("dust_by_ground: the dirt of each ground (field, desert, italy, steppe, winter, tropic), worked out for her build coat: on a field of "
                   "that ground take `dust` toward its dust_colour and `haze` toward its haze_colour in place of the file's own. A paint "
                   "swap's choice that names its own dust_colour and haze_colour (her desert coat) is already worked out for its ground.")


def ground_dirt(ground, coat):
    """(low, haze) of a ground's dirt on a coat, by the art director's rule: dirt must differ from the paint under it by
    9 or more in lightness or it does not show. A low that is too near the coat takes the ground's other tone, then
    its pale haze. A haze too near it is no haze: the coat itself (the art director's E7 mend, 22 Nov 19:27Z: on the
    Light Stone the roof is the largest thing the battle camera sees, and it reads as paint; it was the low before)."""
    low, top, other = GROUND_DIRT[ground]
    c = lightness(coat)
    for cand in (low, other, top):
        if cand and abs(lightness(cand) - c) >= 9.0:
            low = cand
            break
    if top is None or abs(lightness(top) - c) < 9.0:          # (a ground with no haze, or a haze too near the coat: the coat itself)
        top = coat.upper()
    return low, top


def choice_dirt(model, home=None):
    """Gives each of a file's paint choices without its own dirt the dirt of the ground it is worn on: a choice the
    desert rows name (swaps.paint.by_ground.desert) the desert's, the others her home ground's (none: left as they are)."""
    sw = model.info.get("file", {}).get("swaps", {}).get("paint", {})
    desert = {r.get("paint") for r in sw.get("by_ground", {}).get("desert", [])}
    for key, ch in sw.get("choices", {}).items():
        if "dust_colour" in ch:
            continue
        g = "desert" if key in desert else home
        if not g:
            continue
        upper = ch.get("paints", {}).get("upper", model.paints.get("upper", "565C3A"))
        low, top = ground_dirt(g, upper)
        ch["dust_colour"] = low
        if "haze_colour" in model.info.get("file", {}):
            ch["haze_colour"] = top
EDGE_DARK = 0.22         # how far a plate's cut-back edge goes dark where boots and hands rub it: in the file it rides on `soot`
#                          (wear on an edge goes darker, never whiter: a pale line along every edge reads as chalk)
COATS = ("upper", "upper2", "shade")


def burn_sources(model, ring_r=0.85):
    """Where a vehicle burns when she is knocked out, for `land_look(burn=...)`: the turret ring, the engine deck and
    every hatch that fire stands out of, each with how far its black reaches (metres)."""
    out = []
    for s in model.sockets:
        n = s["name"]
        if n == "ring":
            out.append({"at": Vector(s["at"]), "reach": 1.2, "ring": ring_r})
        elif n == "engine":
            out.append({"at": Vector(s["at"]), "reach": 1.5})
        elif n.startswith("fire."):
            out.append({"at": Vector(s["at"]), "reach": 0.8})
    return out


def land_look(model, dust=1.0, wear=1.0, dust_hex=DUST, dust_top=1.45, burn=None):
    """What the pictures add to a paint that the numbers do not carry. She has been driven: dust in the colour of the
    ground she stands on, thick on the running gear and gone by the middle of the hull, lying on whatever looks up;
    mud packed where a wheel's dish or a corner keeps it; dirt in every recess; paint rubbed through only where the
    flagged edges say boots and hands come; soot round the exhaust; and a track that is steel: dark, rusty in its
    hollows, bright where wheels and ground polish it. Returns the `look` for rigid.to_blender. (The game does its
    own; the file carries wear and soot point by point.)"""
    lo = Vector((min(v.x for v in model.verts), min(v.y for v in model.verts), min(v.z for v in model.verts)))
    hi = Vector((max(v.x for v in model.verts), max(v.y for v in model.verts), max(v.z for v in model.verts)))
    size = hi - lo
    dust_rgb = hex_rgb(dust_hex)
    mud_rgb = tuple(c * 0.3 for c in dust_rgb)             # the same earth, packed and not yet dry
    pipes = [Vector(s["at"]) for s in model.sockets if s["name"].startswith("exhaust")]
    splash = list(getattr(model, "splash", []))            # skirts hung beside the tracks: (how far out they hang, their feet, the mud's top)
    dried_rgb = tuple(c * 0.56 for c in dust_rgb)

    def look(paint, mat, nt, bsdf, mix):
        key = paint.split(".")[0]
        if key == "lamp":                                  # a lamp's glass, unlit: dark, with a soft shine (never the sky's mirror)
            bsdf.inputs["Roughness"].default_value = 0.34
            if "Specular IOR Level" in bsdf.inputs:
                bsdf.inputs["Specular IOR Level"].default_value = 0.3
            return
        if key == "glass":
            bsdf.inputs["Roughness"].default_value = 0.06
            if "Specular IOR Level" in bsdf.inputs:
                bsdf.inputs["Specular IOR Level"].default_value = 0.9
            return
        if key == "black":
            bsdf.inputs["Roughness"].default_value = 0.95
            if "Specular IOR Level" in bsdf.inputs:
                bsdf.inputs["Specular IOR Level"].default_value = 0.1
            return
        tc = nt.nodes.new("ShaderNodeTexCoord")
        # the point's own place on the model, in metres (it rides with the part when a turret turns)
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = size
        mp.inputs["Location"].default_value = lo
        nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"])
        pos = mp.outputs["Vector"]
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(pos, sep.inputs[0])
        z = sep.outputs[2]
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        nsep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(geo.outputs["Normal"], nsep.inputs[0])
        big = _noise(nt, pos, 0.9, 1.5, 0.45)
        mid = _noise(nt, pos, 4.5, 4.0)
        fine = _noise(nt, pos, 22.0, 6.0, 0.6)
        col = mix.outputs[2]
        at = nt.nodes.new("ShaderNodeAttribute")
        at.attribute_name = "Edge"
        edge_at = at.outputs["Fac"]
        # how shut-in a place is: a corner under a fitting, the dish of a wheel (nook); the seam round a hatch, the
        # foot of a bolt (crack). Dirt gathers there and nothing rubs it out.
        ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
        ao.samples = 4
        ao.inputs["Distance"].default_value = 0.14
        nook = _ramp(nt, ao.outputs["AO"], 0.5, 0.95, 1.0, 0.0)
        ao2 = nt.nodes.new("ShaderNodeAmbientOcclusion")
        ao2.samples = 3
        ao2.inputs["Distance"].default_value = 0.03
        crack = _ramp(nt, ao2.outputs["AO"], 0.55, 0.97, 1.0, 0.0)

        def toward(base, rgb, fac, blend="MIX"):
            _, f, a, b, o = _mix(nt, blend)
            if isinstance(fac, (int, float)):
                f.default_value = fac
            else:
                nt.links.new(fac, f)
            nt.links.new(base, a)
            if isinstance(rgb, tuple):
                b.default_value = (*rgb[:3], 1.0)
            else:
                nt.links.new(rgb, b)
            return o

        def tone(fac, lo_pos, lo_col, hi_pos, hi_col):
            r = nt.nodes.new("ShaderNodeValToRGB")
            r.color_ramp.elements[0].position, r.color_ramp.elements[0].color = lo_pos, (*lo_col, 1)
            r.color_ramp.elements[1].position, r.color_ramp.elements[1].color = hi_pos, (*hi_col, 1)
            nt.links.new(fac, r.inputs["Fac"])
            return r.outputs["Color"]

        # ---- the dust: thick by the ground, gone by the middle of the hull; lying on what looks up; kept in corners
        # (a film, never a coat of paint: at its thickest the colour under it still shows, and it lies in patches)
        patchy = _math(nt, "MULTIPLY", _ramp(nt, mid, 0.28, 0.72, 0.3, 1.0), _ramp(nt, big, 0.2, 0.8, 0.7, 1.0))
        low = _math(nt, "MULTIPLY", _math(nt, "POWER", _ramp(nt, z, 0.3, dust_top, 0.66, 0.0), 1.25), patchy)
        lying = _math(nt, "MULTIPLY", _ramp(nt, nsep.outputs[2], 0.5, 0.97, 0.0, 1.0), _ramp(nt, z, dust_top - 0.5, dust_top + 0.5, 0.55, 0.13))
        lying = _math(nt, "MULTIPLY", lying, _ramp(nt, mid, 0.3, 0.7, 0.45, 1.0))
        grime = _math(nt, "MAXIMUM", low, lying)
        grime = _math(nt, "ADD", grime, _math(nt, "MULTIPLY", nook, _ramp(nt, z, 0.3, dust_top + 0.4, 0.22, 0.1)))
        # ---- the mud: packed where a recess low down keeps it, and thrown up in spots behind the tracks
        thrown = _math(nt, "MULTIPLY", _ramp(nt, _noise(nt, pos, 8.0, 3.0, 0.6), 0.58, 0.68, 0.0, 1.0), _ramp(nt, z, 0.25, 1.0, 0.6, 0.0))
        mud = _math(nt, "MAXIMUM", _math(nt, "MULTIPLY", nook, _ramp(nt, z, 0.3, 1.15, 0.85, 0.0)), thrown)
        # ---- the soot: black round each exhaust outlet and up the plate above it
        soot = None
        for p in pipes:
            d = nt.nodes.new("ShaderNodeVectorMath")
            d.operation = "SUBTRACT"
            nt.links.new(pos, d.inputs[0])
            d.inputs[1].default_value = p
            ds = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(d.outputs[0], ds.inputs[0])
            dz = _math(nt, "ADD", _math(nt, "MULTIPLY", _math(nt, "MAXIMUM", ds.outputs[2], 0.0), 0.4), _math(nt, "MULTIPLY", _math(nt, "MINIMUM", ds.outputs[2], 0.0), 1.5))
            cb = nt.nodes.new("ShaderNodeCombineXYZ")
            nt.links.new(ds.outputs[0], cb.inputs[0])
            nt.links.new(ds.outputs[1], cb.inputs[1])
            nt.links.new(dz, cb.inputs[2])
            ln = nt.nodes.new("ShaderNodeVectorMath")
            ln.operation = "LENGTH"
            nt.links.new(cb.outputs[0], ln.inputs[0])
            s = _math(nt, "POWER", _ramp(nt, ln.outputs["Value"], 0.06, 0.66, 1.0, 0.0), 1.6)
            soot = s if soot is None else _math(nt, "MAXIMUM", soot, s)
        if soot is not None:
            soot = _math(nt, "MULTIPLY", soot, _ramp(nt, mid, 0.25, 0.75, 0.6, 1.0))

        base = col
        rough, metal = None, None
        if key in COATS:
            # the coat is never one colour: faded here, darker there; rain has run down it
            base = toward(base, tone(big, 0.25, (0.90, 0.90, 0.89), 0.75, (1.06, 1.06, 1.05)), 1.0, "MULTIPLY")
            # (a few streaks here and there, of unlike widths: fine even ones all over a big plate read as planks)
            streak = _noise(nt, pos, 1.0, 2.0, 0.5, stretch=(12.0, 12.0, 0.8))
            where = _ramp(nt, _noise(nt, pos, 1.0, 1.0, 0.5, stretch=(3.1, 3.1, 0.4)), 0.47, 0.66, 0.0, 1.0)      # which stretches of plate carry them
            side_on = _ramp(nt, _math(nt, "ABSOLUTE", nsep.outputs[2]), 0.0, 0.55, 1.0, 0.0)
            base = toward(base, tone(streak, 0.34, (0.80, 0.78, 0.74), 0.60, (1.03, 1.03, 1.02)), _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", side_on, where), 0.42 * wear), "MULTIPLY")
            # recesses keep their dirt: a dark wash in every corner, darker in the seams round hatches and bolts
            base = toward(base, (0.36, 0.32, 0.27), _math(nt, "MULTIPLY", nook, 0.6), "MULTIPLY")
            base = toward(base, (0.22, 0.19, 0.16), _math(nt, "MULTIPLY", crack, 0.75), "MULTIPLY")
            # edges where boots and hands come: paint rubbed thin and dark (wear goes darker, never whiter: a pale
            # line along an edge reads as chalk), then knocked through to dark steel in places
            rub = _math(nt, "MULTIPLY", edge_at, _ramp(nt, mid, 0.25, 0.75, 0.2, 1.0))
            chip = _math(nt, "MULTIPLY", edge_at, _ramp(nt, fine, 0.48, 0.58, 0.0, 1.0), clamp=True)
            base = toward(base, (0.60, 0.57, 0.53), _math(nt, "MULTIPLY", rub, 0.5 * wear, clamp=True), "MULTIPLY")
            base = toward(base, (0.07, 0.065, 0.06), _math(nt, "MULTIPLY", chip, 0.8 * wear, clamp=True))
            rough = _math(nt, "SUBTRACT", _ramp(nt, big, 0.2, 0.8, 0.46, 0.6), _math(nt, "MULTIPLY", chip, 0.2), clamp=True)
        elif key == "track":
            # steel links: dark brown-grey, rust in the hollows, bare bright steel where wheels and ground polish them
            scuff = _noise(nt, pos, 70.0, 3.0, 0.6)
            shine = _math(nt, "MULTIPLY", edge_at, _math(nt, "MULTIPLY", _ramp(nt, mid, 0.2, 0.8, 0.7, 1.0), _ramp(nt, scuff, 0.3, 0.7, 0.75, 1.0)))
            dull = _math(nt, "SUBTRACT", 1.0, edge_at, clamp=True)
            rusty = _math(nt, "MULTIPLY", _ramp(nt, mid, 0.42, 0.62, 0.15, 1.0), _ramp(nt, fine, 0.35, 0.7, 0.2, 0.55))
            base = toward(base, hex_rgb("4C3226"), _math(nt, "MULTIPLY", dull, rusty))
            base = toward(base, hex_rgb("1E1A18"), _math(nt, "MULTIPLY", dull, _math(nt, "MULTIPLY", crack, 0.7)))
            base = toward(base, (0.40, 0.40, 0.42), shine)
            # (steel all through: even the dark of a link has a metal's sheen, which is what tells it from wood)
            metal = _math(nt, "MAXIMUM", _math(nt, "MULTIPLY", shine, 0.95), _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", 1.0, rusty), 0.45))
            rough = _ramp(nt, shine, 0.0, 1.0, 0.7, 0.42)
            grime = _math(nt, "MULTIPLY", grime, _ramp(nt, edge_at, 0.0, 1.0, 0.4, 0.06))
            mud = _math(nt, "MULTIPLY", mud, _ramp(nt, edge_at, 0.0, 1.0, 0.8, 0.0))
            # the run lying on the road takes the road's dust: the dustiest steel on her. (By where the link is now,
            # not where it was built, so the dust stays on the ground run while the track goes round.)
            wsep = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(geo.outputs["Position"], wsep.inputs[0])
            on_road = _math(nt, "MULTIPLY", _ramp(nt, wsep.outputs[2], 0.07, 0.22, 1.0, 0.0), _ramp(nt, mid, 0.25, 0.75, 0.6, 1.0))
            grime = _math(nt, "MAXIMUM", grime, _math(nt, "MULTIPLY", on_road, _ramp(nt, edge_at, 0.0, 1.0, 0.85, 0.55)))
        elif key == "rust":
            # an exhaust that has been red-hot and rained on: dark rust brown, blotched darker, flecked lighter
            base = toward(base, hex_rgb("241A16"), _ramp(nt, _noise(nt, pos, 11.0, 4.0, 0.6), 0.38, 0.62, 0.0, 0.8))
            base = toward(base, hex_rgb("6A4028"), _ramp(nt, _noise(nt, pos, 46.0, 3.0, 0.6), 0.58, 0.72, 0.0, 0.55))
            rough = 0.94
            grime = _math(nt, "MULTIPLY", grime, 0.5)
            bump = nt.nodes.new("ShaderNodeBump")
            bump.inputs["Strength"].default_value = 0.35
            bump.inputs["Distance"].default_value = 0.004
            nt.links.new(_noise(nt, pos, 60.0, 4.0, 0.6), bump.inputs["Height"])
            nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        elif key == "wood":
            wave = nt.nodes.new("ShaderNodeTexWave")
            wave.inputs["Scale"].default_value = 9.0
            wave.inputs["Distortion"].default_value = 5.0
            wave.inputs["Detail"].default_value = 3.0
            nt.links.new(pos, wave.inputs["Vector"])
            base = toward(base, tone(wave.outputs[1], 0.0, (0.6, 0.55, 0.5), 1.0, (1.15, 1.12, 1.1)), 0.55, "MULTIPLY")
            rough = 0.8
        elif key in ("canvas", "cloth", "rope"):
            rough = 0.95
            weave = _noise(nt, pos, 160.0, 2.0, 0.5)
            bump = nt.nodes.new("ShaderNodeBump")
            bump.inputs["Strength"].default_value = 0.25
            bump.inputs["Distance"].default_value = 0.003
            nt.links.new(_math(nt, "ADD", weave, _math(nt, "MULTIPLY", big, 3.0)), bump.inputs["Height"])
            nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
            base = toward(base, tone(big, 0.0, (0.7, 0.7, 0.7), 1.0, (1.15, 1.15, 1.15)), 0.5, "MULTIPLY")
        elif key == "rubber":
            rough = 0.88
            grime = _math(nt, "MULTIPLY", grime, 0.8)
        elif key in ("steel", "bright", "brass"):
            metal = 0.85 if key != "steel" else 0.7
            rough = _ramp(nt, fine, 0.2, 0.8, 0.3, 0.55)
            grime = _math(nt, "MULTIPLY", grime, 0.7)
        if key not in COATS and key != "track":
            base = toward(base, (0.30, 0.27, 0.23), _math(nt, "MULTIPLY", _math(nt, "MAXIMUM", nook, crack), 0.5), "MULTIPLY")
        # the dust, the mud over it, and the soot last of all (the exhaust lays it afresh every hour she runs)
        grime = _math(nt, "MULTIPLY", grime, 0.86 * dust, clamp=True)
        mud = _math(nt, "MULTIPLY", mud, 0.9 * dust, clamp=True)
        base = toward(base, dust_rgb, grime)
        base = toward(base, mud_rgb, mud)
        if splash and key in COATS:
            # skirts hang in what the tracks throw up: mud dried thick along their feet, thinning in ragged fingers to a
            # third of the way up, still wet and dark at the very foot, spots of it flung higher
            absy = _math(nt, "ABSOLUTE", sep.outputs[1])
            finger = _noise(nt, pos, 1.0, 3.0, 0.55, stretch=(16.0, 16.0, 3.0))
            speck = _ramp(nt, _noise(nt, pos, 38.0, 3.0, 0.6), 0.3, 0.6, 0.72, 1.0)
            spot = _ramp(nt, _noise(nt, pos, 11.0, 3.0, 0.6), 0.60, 0.68, 0.0, 1.0)
            for y_out, z0, z1 in splash:
                on = _math(nt, "GREATER_THAN", absy, y_out)
                edge = _math(nt, "ADD", _ramp(nt, z, z0, z1, 0.0, 1.0), _math(nt, "ADD", _math(nt, "MULTIPLY", finger, 0.9), _math(nt, "MULTIPLY", mid, 0.4)))
                dried = _math(nt, "MULTIPLY", _ramp(nt, edge, 0.95, 1.45, 1.0, 0.0), speck)
                flung = _math(nt, "MULTIPLY", spot, _ramp(nt, z, z0, z0 + 2.2 * (z1 - z0), 0.85, 0.0))
                cake = _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", on, _math(nt, "MAXIMUM", dried, flung)), 0.92 * dust, clamp=True)
                wet = _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", on, _ramp(nt, edge, 0.6, 0.9, 1.0, 0.0)), 0.75 * dust, clamp=True)
                base = toward(base, dried_rgb, cake)
                base = toward(base, mud_rgb, wet)
                grime = _math(nt, "MAXIMUM", grime, cake)
        if soot is not None:
            soot = _math(nt, "MULTIPLY", soot, 0.95, clamp=True)
            base = toward(base, (0.012, 0.011, 0.010), soot)
        burnt = None
        if burn:
            # she has burnt: black round what burnt (the turret ring, the engine deck, the hatches fire stood out of),
            # the paint beyond it blistered to dark brown-grey streaks, smoke lying on all that looks up. How far gone
            # she is rides the object's own colour (its red, 0 to 1), so a clip can burn her as it runs.
            info = nt.nodes.new("ShaderNodeObjectInfo")
            sc = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(info.outputs["Color"], sc.inputs[0])
            amount = sc.outputs[0]
            gone = _math(nt, "SUBTRACT", 1.0, amount)        # (what has not burnt yet: the black and the brown creep out from the fire as this falls)
            run = _noise(nt, pos, 1.0, 3.0, 0.5, stretch=(26.0, 26.0, 1.2))      # (streaks that run down her sides)
            ragged = _math(nt, "MULTIPLY", _ramp(nt, mid, 0.25, 0.75, 0.6, 1.0), _ramp(nt, run, 0.3, 0.7, 0.8, 1.0))
            core, wide, ring_black = None, None, None
            for src in burn:
                d = nt.nodes.new("ShaderNodeVectorMath")
                d.operation = "SUBTRACT"
                nt.links.new(pos, d.inputs[0])
                d.inputs[1].default_value = src["at"]
                ds = nt.nodes.new("ShaderNodeSeparateXYZ")
                nt.links.new(d.outputs[0], ds.inputs[0])
                # (soot goes up and lies wide; down her sides it reaches a shorter way)
                dz = _math(nt, "ADD", _math(nt, "MULTIPLY", _math(nt, "MAXIMUM", ds.outputs[2], 0.0), 0.55), _math(nt, "MULTIPLY", _math(nt, "MINIMUM", ds.outputs[2], 0.0), 1.7))
                cb = nt.nodes.new("ShaderNodeCombineXYZ")
                nt.links.new(ds.outputs[0], cb.inputs[0])
                nt.links.new(ds.outputs[1], cb.inputs[1])
                nt.links.new(dz, cb.inputs[2])
                ln = nt.nodes.new("ShaderNodeVectorMath")
                ln.operation = "LENGTH"
                nt.links.new(cb.outputs[0], ln.inputs[0])
                r, edge = src["reach"], src.get("ring", 0.0)             # (the ring's black starts at its own edge)
                c = _ramp(nt, ln.outputs["Value"], edge + r * 0.1, edge + r, 1.0, 0.0)
                w = _ramp(nt, ln.outputs["Value"], edge + r * 0.6, edge + r * 2.4, 1.0, 0.0)
                core = c if core is None else _math(nt, "MAXIMUM", core, c)
                wide = w if wide is None else _math(nt, "MAXIMUM", wide, w)
                if src.get("ring"):      # the ring and all under it once she has blown: the turret's foot, its basket, the well in the roof
                    flat = nt.nodes.new("ShaderNodeCombineXYZ")
                    nt.links.new(ds.outputs[0], flat.inputs[0])
                    nt.links.new(ds.outputs[1], flat.inputs[1])
                    fl = nt.nodes.new("ShaderNodeVectorMath")
                    fl.operation = "LENGTH"
                    nt.links.new(flat.outputs[0], fl.inputs[0])
                    disc = _math(nt, "MULTIPLY", _ramp(nt, fl.outputs["Value"], src["ring"], src["ring"] + 0.22, 1.0, 0.0),
                                 _math(nt, "MULTIPLY", _ramp(nt, ds.outputs[2], 0.04, 0.12, 1.0, 0.0), _ramp(nt, ds.outputs[2], -0.9, -0.6, 0.0, 1.0)))
                    ring_black = _math(nt, "MULTIPLY", disc, _ramp(nt, amount, 0.3, 0.5, 0.0, 1.0))
            # black: a ragged collar round each fire, full by the end out to about two fifths of its reach
            black = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", _math(nt, "MULTIPLY", core, ragged), _math(nt, "MULTIPLY", gone, 0.6)), 2.5, clamp=True)
            if ring_black is not None:
                black = _math(nt, "MAXIMUM", black, ring_black)
            # the paint beyond it, blistered: all of her above the track guards by the end, in streaks; thickest near the fires
            z0 = 0.34 * body_top(model)
            every = _ramp(nt, z, z0, z0 + 0.45, 0.0, 0.6)
            streaks = _math(nt, "MULTIPLY", _ramp(nt, run, 0.3, 0.65, 0.4, 1.0), _ramp(nt, big, 0.2, 0.8, 0.75, 1.0))
            brown = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", _math(nt, "MULTIPLY", _math(nt, "MAXIMUM", wide, every), streaks), _math(nt, "MULTIPLY", gone, 0.7)), 2.2, clamp=True)
            fall = _math(nt, "MULTIPLY", _ramp(nt, nsep.outputs[2], 0.3, 0.9, 0.0, 0.5), amount)        # (smoke lies on all that looks up)
            brown = _math(nt, "MAXIMUM", brown, fall)
            base = toward(base, tone(big, 0.3, (0.036, 0.029, 0.024), 0.75, (0.07, 0.044, 0.03)), _math(nt, "MULTIPLY", brown, 0.92))
            base = toward(base, (0.012, 0.011, 0.010), black)
            burnt = _math(nt, "MAXIMUM", black, _math(nt, "MULTIPLY", brown, 0.8))
        nt.links.new(base, bsdf.inputs["Base Color"])
        covered = _math(nt, "MAXIMUM", grime, mud)
        if burnt is not None:
            covered = _math(nt, "MAXIMUM", covered, burnt)
        if soot is not None:
            covered = _math(nt, "MAXIMUM", covered, soot)
        if rough is None:
            rough = bsdf.inputs["Roughness"].default_value
        r = _math(nt, "ADD", _math(nt, "MULTIPLY", rough, _math(nt, "SUBTRACT", 1.0, covered)), _math(nt, "MULTIPLY", covered, 0.96), clamp=True)
        nt.links.new(r, bsdf.inputs["Roughness"])
        if metal is not None:
            m = _math(nt, "MULTIPLY", metal, _math(nt, "SUBTRACT", 1.0, covered), clamp=True)
            nt.links.new(m, bsdf.inputs["Metallic"])
    return look


def body_top(model):
    """How high she stands without her aerial."""
    skip = {i for i, p in enumerate(model.parts) if p.name.startswith("aerial")}
    return max(model.verts[i].z for f, pi in zip(model.faces, model.fpart) if pi not in skip for i in f)


def lightness(hx):
    """CIE lightness (0 to 100) of a paint."""
    lin = [((c / 12.92) if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4) for c in (int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4))]
    y = 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2]
    return 116.0 * (y ** (1.0 / 3.0) if y > 0.008856 else 7.787 * y + 16.0 / 116.0) - 16.0


def dirt_for(coat):
    """(wear colour, haze colour) for a coat, by the art director's rule (22 Nov): dirt must differ from the paint under
    it by 9 or more in lightness or it does not show. Road dust where it does; on a coat of the dust's own tone (the
    German dark yellow), damp earth from the ground up and a pale grey haze on what looks up."""
    return (DUST, DUST) if abs(lightness(coat) - lightness(DUST)) >= 9.0 else (EARTH, HAZE)


def touched(model, flags):
    """Edge wear only where boots and hands come. Every cut-back plate edge is flagged when it is made; this keeps
    the flag whole on hatches and doors, most of it on edges a boot comes down on (the track guards, the decks, the
    roof) and on the front edges the crew climb up by, and leaves only a trace anywhere else. (Flags on other paints,
    a track link's polished faces among them, are left as made.)"""
    names = list(model.paints)
    coat = {i for i, n in enumerate(names) if n.split(".")[0] in COATS}
    front = max(v.x for v in model.verts)
    out = list(flags)
    for k, e in enumerate(out):
        if e <= 0.0 or model.fpaint[k] not in coat:
            continue
        name = model.parts[model.fpart[k]].name
        f = model.faces[k]
        c = sum((model.verts[i] for i in f), Vector()) / len(f)
        n = (model.verts[f[1]] - model.verts[f[0]]).cross(model.verts[f[2]] - model.verts[f[1]])
        n = n.normalized() if n.length > 1e-12 else Vector((0, 0, 0))
        if name.startswith(("hatch", "door")):
            w = 1.0
        elif name.startswith(("wheel", "sprocket", "idler", "roller")):
            w = 0.3
        else:
            w = 0.08
            if n.z > 0.25 and c.z > 0.7:
                w = 0.6 if name == "hull" else 0.4
            if n.x > 0.3 and c.x > front * 0.5:
                w = max(w, 0.75)
        out[k] = e * w
    return out


def to_scene(model, look="land", crease=50.0, weighted=True, **look_kw):
    """The model as a mesh on its skeleton (rigid.to_blender), then given weighted normals: a flat plate stays flat and
    its cut-back edge carries all the rounding, which is what makes an edge look rolled rather than drawn."""
    lk = land_look(model, **look_kw) if look == "land" else look
    ob, arm = rigid.to_blender(model, lk, crease=crease)
    me = ob.data
    fe = list(getattr(model, "fedge", []))[:len(model.faces)]
    fe += [0.0] * (len(model.faces) - len(fe))
    fe = touched(model, fe)
    total = np.zeros(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get("loop_total", total)
    model._ledge = np.repeat(np.array(fe, dtype=float), total)
    me.attributes.new("Edge", "FLOAT", "CORNER").data.foreach_set("value", model._ledge)
    if look != "land" and model._ledge.any():
        # (the pictures' look draws edge wear itself; without it, the edges' dark goes into the corner colours the .glb carries)
        lv = np.zeros(len(me.loops), dtype=np.int32)
        me.loops.foreach_get("vertex_index", lv)
        lface = np.repeat(np.array(model.fpaint, dtype=np.int32), total)
        tint = rigid.tint_of(model, model._wear[lv], np.maximum(model._soot[lv], model._ledge * EDGE_DARK), model._lpaint, lface, model._lblend,
                             model._lblend2, model._haze[lv] if model._haze is not None else None, model._shade[lv] if model._shade is not None else None,
                             model._dust[lv] if model._dust is not None else None)
        me.color_attributes["Col"].data.foreach_set("color", np.concatenate([tint, np.ones((len(tint), 1))], axis=1).ravel())
    laid = getattr(model, "fnormal", None)
    if weighted and laid is not None and len(laid) == len(model.faces) and abs(getattr(model, "fnormal_crease", -1.0) - crease) < 1e-6:
        # (camouflage cut her plates finer: the corners' facings were read before the cutting and carried onto the new points)
        me.normals_split_custom_set([n for f in laid for n in f])
    elif weighted:
        mod = ob.modifiers.new("wn", "WEIGHTED_NORMAL")
        mod.keep_sharp = True
        mod.weight = 50
        mod.mode = "FACE_AREA"
        bpy.context.view_layer.objects.active = ob
        ob.select_set(True)
        while ob.modifiers[0].name != "wn":
            bpy.ops.object.modifier_move_up(modifier="wn")
        bpy.ops.object.modifier_apply(modifier="wn")
    return ob, arm


def gather_land(model, ob):
    """rigid.gather, with every corner on a plate's cut-back edge darkened a little (on `soot`): the mesh as the game
    reads it. (A point on an edge is two points in the file, one for the flat face and one for the edge, so the dark
    stays a thin line and does not spread over the plate.)"""
    me = ob.data
    lv = np.zeros(len(me.loops), dtype=np.int32)
    me.loops.foreach_get("vertex_index", lv)
    ledge = getattr(model, "_ledge", None)
    if ledge is None or not ledge.any():
        return rigid.gather(model, ob)
    me.calc_loop_triangles()
    nl = len(me.loops)
    co = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    cn = np.zeros(nl * 3)
    me.corner_normals.foreach_get("vector", cn)
    cn = cn.reshape(-1, 3)
    tris = np.zeros(len(me.loop_triangles) * 3, dtype=np.int32)
    me.loop_triangles.foreach_get("loops", tris)
    pos, nor = np.round(co[lv], 3), np.round(cn, 2)
    part, paint = model._vpart[lv], model._lpaint
    wear = np.round(model._wear[lv] * 100)
    soot, blend = np.round(np.maximum(model._soot[lv], ledge * EDGE_DARK) * 100), np.round(model._lblend * 100)
    more = rigid.extras(model, lv)
    key = np.concatenate([pos, nor, part[:, None], paint[:, None], wear[:, None], soot[:, None], blend[:, None]] + [a[:, None] for a in more.values()], axis=1)
    _, first, inverse = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inverse = inverse.ravel()
    order = np.argsort(first)
    rank = np.empty_like(order)
    rank[order] = np.arange(len(order))
    first = first[order]
    index = rank[inverse]
    p, n = pos[first].copy(), nor[first].copy()
    p[:, 1] *= -1.0
    n[:, 1] *= -1.0
    t = index[tris].reshape(-1, 3)[:, [0, 2, 1]]

    def flat(a, digits):
        return [round(float(x), digits) if digits else int(x) for x in a.ravel()]
    return {"points": flat(p, 3), "facings": flat(n, 2), "paints": flat(paint[first], 0), "triangles": flat(t, 0),
            "blend": flat(blend[first], 0), "soot": flat(soot[first], 0), "wear": flat(wear[first], 0), "part": flat(part[first], 0),
            **{k: ([round(float(x) / 100.0, 2) for x in a[first]] if k == "shade" else flat(a[first], 0)) for k, a in more.items()}}


def bake_actions(model, arm, fps=30):
    """rigid.bake_actions, but a clip that runs a track on by one link (it has a `stride`) ends on its last pose and
    does not come back to its first: looped, every link has become the next, and nothing is seen to jump."""
    arm.animation_data_create()
    made = []
    for name, clip in model.clips.items():
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        arm.animation_data.action = act
        moved = {tr["part"] for tr in clip["tracks"]}
        n = max(1, int(round(clip["long"] * fps)))
        for f in range(n + 1):
            last = f == n
            t = min(f / fps, clip["long"])
            if clip.get("loop"):
                t = (clip["long"] - 1e-6) if (last and "stride" in clip) else (0.0 if last else t)
            pose = rigid.pose_of(model, clip, t)
            for part in moved:
                pb = arm.pose.bones[part]
                q, mv, sc = pose[part]
                pb.rotation_quaternion, pb.location, pb.scale = q, mv, sc
                pb.keyframe_insert("rotation_quaternion", frame=f + 1)
                pb.keyframe_insert("location", frame=f + 1)
                pb.keyframe_insert("scale", frame=f + 1)
        made.append(act)
    for pb in arm.pose.bones:
        pb.rotation_quaternion, pb.location, pb.scale = Quaternion(), Vector(), Vector((1, 1, 1))
    arm.animation_data.action = None
    return made
