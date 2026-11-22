"""Fittings: everything bolted, welded, hinged or strapped to a vehicle.

Guns and their muzzle brakes, hatches that open (each a part of its own), cupolas, visors, machine-gun mounts, lamps,
tools, tow cables, spare links, jerrycans, bins, aerials, silencers, grilles. Small fittings are left off the plain
(far) mesh.
"""

import math
import random
import zlib

import bpy  # noqa: F401
import bmesh
from mathutils import Matrix, Vector

from motorpool import LEFT, RIGHT, X, Y, Z, bm_box, bm_extrude, bm_hull, bm_move, bm_prism, frame, outline, rot, round_plan


class Fittings:
    # ---- guns ----------------------------------------------------------------------------------------
    def barrel(self, base, length, bore, r0=None, r1=None, brake=None, steps=(), paint="upper", sleeve=None, sides=24, facing=X, part=None, jacket=None):
        """A gun barrel from `base` along `facing`: `r0` thick at the breech end, `r1` at the muzzle. steps: (how far
        along as a fraction, radius) where it steps down; sleeve: (length, radius) of the fixed sleeve at its root;
        brake: None, 'double' (two baffles), 'single', 'pepper' (a perforated sleeve), 'cone' (a flash cone)."""
        r0 = r0 or bore * 1.25
        r1 = r1 or bore * 0.92
        sides = self.n(sides, 8)
        L = length
        prof = []                                       # from the muzzle back to the root, round the outside
        bl = 0.0
        if brake == "double":
            bl = bore * 5.6
            rb = bore * 1.55
            f = L - bl
            prof += [(L, bore * 0.62), (L, rb * 0.78), (L - bore * 0.5, rb), (L - bore * 1.3, rb), (L - bore * 1.5, rb * 0.62), (L - bore * 2.5, rb * 0.62),
                     (L - bore * 2.7, rb * 1.06), (L - bore * 3.7, rb * 1.06), (L - bore * 3.9, rb * 0.66), (f + bore * 0.6, rb * 0.66), (f, r1 * 1.12)]
        elif brake == "single":
            bl = bore * 3.4
            rb = bore * 1.6
            f = L - bl
            prof += [(L, bore * 0.62), (L, rb * 0.8), (L - bore * 0.5, rb), (L - bore * 1.9, rb), (L - bore * 2.2, rb * 0.66), (f + bore * 0.5, rb * 0.66), (f, r1 * 1.12)]
        elif brake == "pepper":
            bl = bore * 4.5
            rb = bore * 1.0
            f = L - bl
            prof += [(L, bore * 0.62), (L, rb), (f + bore * 0.3, rb), (f, r1)]
        elif brake == "cone":
            bl = bore * 2.2
            f = L - bl
            prof += [(L, bore * 0.62), (L, bore * 1.5), (L - 0.012, bore * 1.55), (f, r1)]
        else:
            f = L
            prof += [(L, bore * 0.56), (L, r1 - 0.004), (L - 0.006, r1)]
        last = f
        rr = r1
        for frac, rad in sorted(steps, reverse=True):
            x = L * frac
            ra = lerp_r(r0, r1, x / L)
            prof += [(x + 0.012, lerp_r(r0, r1, (x + 0.012) / L) if rad is None else rr), (x, rad if rad is not None else ra)]
            rr = rad if rad is not None else ra
            last = x
        prof += [(0.0, r0 if not steps else max(r0, rr)), (0.0, 0.0)]
        if part:
            self.m.use(part)
        self.lathe(base, facing, [(L - bore * 1.2, 0.0), (L - bore * 1.2, bore * 0.56), (L, bore * 0.56 if not brake else bore * 0.62)], "black", sides)
        self.lathe(base, facing, prof, paint, sides)
        if brake in ("double", "single") and self.fine:
            # the side openings of the baffles, shown dark
            ax, side, up = frame(facing)
            M = Matrix((ax, side, up)).transposed()
            for d in ((L - bore * 2.0, L - bore * 4.4) if brake == "double" else (L - bore * 2.7,)):
                for s in (1.0, -1.0):
                    bm = bm_box((bore * 0.86, 0.01, bore * 1.0), (0, 0, 0))
                    bm_move(bm, at=Vector(base) + ax * d + side * (s * bore * 0.99), turn=M)
                    self.put(bm, "black")
        if brake == "pepper" and self.fine:
            ax, side, up = frame(facing)
            for row in range(4):
                for k in range(6):
                    a = 2 * math.pi * (k + 0.5 * (row % 2)) / 6
                    c = Vector(base) + ax * (L - bore * (0.7 + row * 1.0)) + (side * math.cos(a) + up * math.sin(a)) * (bore * 1.0 + 0.001)
                    self.lathe(c, side * math.cos(a) + up * math.sin(a), [(0.0, 0.0), (0.0, bore * 0.26)], "black", 6)
        if sleeve:
            sl, sr = sleeve
            self.lathe(base, facing, [(sl, r0), (sl, sr * 0.92), (sl - 0.02, sr), (0.0, sr), (0.0, 0.0)], paint, sides)
        if jacket:      # (a recoil cylinder or cooling jacket beside or over the barrel)
            jl, jr, jo = jacket
            self.rod(Vector(base) + Vector(jo), Vector(base) + Vector(jo) + Vector(facing).normalized() * jl, jr, paint, 14)

    def mg(self, base, facing=X, length=0.55, r=0.016, paint="steel", jacket=True, part=None):
        """A machine gun's barrel in its jacket."""
        b = Vector(base)
        f = Vector(facing).normalized()
        if part:
            self.m.use(part)
        if jacket and self.fine:
            self.lathe(b, f, [(length * 0.8, 0.0), (length * 0.8, r * 1.5), (0.0, r * 1.5), (0.0, 0.0)], paint, 10)
            self.lathe(b, f, [(length, 0.0), (length, r * 0.7), (length * 0.8, r * 0.7)], paint, 8)
            self.lathe(b, f, [(length + 0.001, 0.0), (length + 0.001, r * 0.4)], "black", 6)
        else:
            self.lathe(b, f, [(length, 0.0), (length, r), (0.0, r * 1.2), (0.0, 0.0)], paint, 8)

    def mg_ball(self, at, facing, r=0.11, paint="upper", length=0.42, part=None, socket=None, ring=True):
        """A ball-mounted machine gun in a plate."""
        at, f = Vector(at), Vector(facing).normalized()
        if part:
            self.m.use(part)
        if ring:
            self.lathe(at - f * 0.02, f, [(0.035, r * 0.95), (0.035, r * 1.28), (0.0, r * 1.36), (0.0, 0.0)], paint, self.n(20, 8))
            self.bolts(at + f * 0.016, f, r * 1.2, 8, r=0.011)
        # the ball
        prof = [(r * 0.86, 0.0)] + [(math.sin(a) * r, math.cos(a) * r) for a in (math.radians(d) for d in (62, 44, 26, 8))]
        self.lathe(at - f * 0.02, f, prof, paint, self.n(20, 8))
        self.mg(at + f * (r * 0.8), f, length, part=None)
        if socket:
            self.socket(socket, at + f * (r * 0.8 + length), f)

    # ---- zimmerit --------------------------------------------------------------------------------------
    def zimmerit(self, foot, head, out, pitch=0.04, deep=0.0055, panel=0.15, seam=0.008, paint="upper", part=None, start=0, grime=0.2):
        """The ribbed paste Germany's tanks wore from late 1943 to late 1944, as real ribs (the battle draws no
        textures): a thin sheet of level ridges over a plate, trowelled on in narrow panels about a hand wide, with a
        dark seam between each and its neighbour, and no two panels' rows quite in step or quite the same pitch.
        `foot` and `head`: the plate's lower and upper edge as rows of points, one over the other (two each for a
        flat plate, more round a bend); `out`: the way the plate faces, or f(middle) -> that way. A ridge is a round
        bead: two faces shaded as one over its crest and cut apart at the furrow (each ridge has its own points
        there), and the furrows are a little darker point by point (`grime`, in the file's soot), so the ribs show
        in any light without hard bands. Not on the plain (far) mesh. Returns how many panels it laid (so the next
        sheet's rows can start out of step)."""
        if not self.fine:
            return 0
        foot, head = [Vector(p) for p in foot], [Vector(p) for p in head]
        way = out if callable(out) else (lambda mid, o=Vector(out).normalized(): o)
        verts, faces, dark, count = [], [], [], start
        sv, sf = [], []                                    # the seams: a dark strip down the plate between two panels
        for i in range(len(foot) - 1):
            run = (foot[i + 1] - foot[i]).length
            k = max(1, int(math.ceil(run / panel - 1e-6))) if panel else 1
            gap = min(seam, run / k * 0.2) / run
            for j in range(k):
                sa, tb = j / k, (j + 1) / k
                ta = sa + gap
                o = way((foot[i].lerp(foot[i + 1], (sa + tb) / 2) + head[i].lerp(head[i + 1], (sa + tb) / 2)) / 2)
                q = len(sv)
                sv += [foot[i].lerp(foot[i + 1], sa) + o * 0.001, foot[i].lerp(foot[i + 1], ta) + o * 0.001,
                       head[i].lerp(head[i + 1], ta) + o * 0.001, head[i].lerp(head[i + 1], sa) + o * 0.001]
                sf.append((q, q + 1, q + 2, q + 3))
                fa, fb = foot[i].lerp(foot[i + 1], ta), foot[i].lerp(foot[i + 1], tb)
                ha, hb = head[i].lerp(head[i + 1], ta), head[i].lerp(head[i + 1], tb)
                z0, z1 = max(fa.z, fb.z), max(ha.z, hb.z)            # (a head that slopes cuts each row off where it meets it)
                pp = pitch * (1.0 + 0.06 * ((count * 5) % 7 - 3) / 3.0)        # each panel's own hand: its pitch, its start, its depth
                dp = deep * (1.0 + 0.15 * ((count * 3) % 5 - 2) / 2.0)
                shift = pp * ((count * 0.37) % 1.0)
                count += 1
                if z1 - z0 < pp:
                    continue

                def depth(z):
                    u = ((z + shift) / pp) % 1.0
                    return 0.0015 + dp * (1.0 - abs(2.0 * u - 1.0))
                rows = [(z0, False)]                       # (height, a furrow?)
                n0 = int(math.floor((z0 + shift) / pp))
                for n in range(n0, n0 + int((z1 - z0) / pp) + 3):
                    for u in (0.0, 0.5):
                        z = (n + u) * pp - shift
                        if z0 + 0.003 < z < z1 - 0.003:
                            rows.append((z, u == 0.0))
                rows.append((z1, False))

                def row(z, furrow):
                    d = 0.0015 if furrow else depth(z)
                    q = len(verts)
                    dark.extend([grime * (1.0 - (d - 0.0015) / dp)] * 2)
                    for lo, hi, far in ((fa, ha, hb), (fb, hb, ha)):
                        if z <= hi.z + 1e-9:
                            verts.append(lo.lerp(hi, (z - lo.z) / max(hi.z - lo.z, 1e-9)) + o * d)
                        else:
                            verts.append(hi.lerp(far, (z - hi.z) / max(far.z - hi.z, 1e-9)) + o * d)
                    return q
                below = row(*rows[0])
                for z, furrow in rows[1:]:
                    q = row(z, furrow)
                    faces.append((below, below + 1, q + 1, q))
                    below = row(z, furrow) if furrow else q            # (the next ridge starts on points of its own)
        if faces:
            base = len(self.m.verts)
            self.m.add(verts, faces, paint, True, way, part)
            self.m.vsoot[base:base + len(dark)] = dark
            base = len(self.m.verts)
            self.m.add(sv, sf, paint, False, way, part)
            self.m.vsoot[base:base + len(sv)] = [0.5] * len(sv)
        return count

    # ---- hatches -------------------------------------------------------------------------------------
    def hatch_rect(self, name, parent, corner, size, up=Z, along=X, hinge="back", thick=0.02, opens=95.0, paint="upper", handle=True, well=True, split=False, lip=0.0):
        """A flat rectangular hatch as a part that opens. `corner` is its middle on the plate, `up` the plate's
        outward way, `along` a way lying in the plate (its length runs along it). `hinge`: which edge it hangs on
        (back, front, left, right, taking `along` as forward)."""
        c = Vector(corner)
        n = Vector(up).normalized()
        u = Vector(along)
        u = (u - n * u.dot(n)).normalized()
        v = n.cross(u)
        hx, hy = size[0] / 2.0, size[1] / 2.0
        edge = {"back": (-u * hx, v), "front": (u * hx, -v), "left": (v * hy, u), "right": (-v * hy, -u)}[hinge]
        pivot = c + edge[0] + n * (thick * 0.5)
        # (plus opens: the lid lifts away from the plate about its hinge line. Turned + about `axis` by the right hand,
        # the lid's middle, which lies from the hinge toward -edge[0], must go out along n: axis x (-edge[0]) = +n, so
        # axis = -edge[1]. Until 22 November it was +edge[1], and every lid swung down into the hull.)
        axis = -edge[1]
        self.part(name, parent, pivot, axis=axis, limits=[0.0, opens], kind="turn", note="a hatch: + opens")
        M = Matrix((u, v, n)).transposed()
        bm = bm_box((size[0], size[1], thick), (0, 0, 0))
        bm_move(bm, at=c + n * (thick * 0.5), turn=M)
        self.put(bm, paint, 0.005)
        if self.fine:
            # hinges and a handle
            for s in (-0.6, 0.6):
                h0 = pivot + axis * (s * (hy if hinge in ("back", "front") else hx)) * 1.0
                self.rod(h0 - axis * 0.04, h0 + axis * 0.04, 0.012, paint, 8)
            if handle:
                far = c - edge[0] * 0.55 + n * thick
                self.handle(far - axis * 0.06, far + axis * 0.06, n, 0.007, 0.035, "steel")
            if lip:
                pass
        if well:
            # what shows when it is open: the dark of the inside, just under the plate's face
            self.use(parent)
            bm = bm_box((size[0] * 0.9, size[1] * 0.9, 0.004), (0, 0, 0))
            bm_move(bm, at=c + n * 0.0015, turn=M)
            self.put(bm, "black")
        self.use(parent)
        return name

    def hatch_round(self, name, parent, c, r, up=Z, hinge_way=-X, thick=0.025, opens=100.0, paint="upper", dome=0.02, handle=True, well=True, split=False):
        """A round hatch lid as a part that opens, hinged on the side `hinge_way`."""
        c = Vector(c)
        n = Vector(up).normalized()
        hw = Vector(hinge_way)
        hw = (hw - n * hw.dot(n)).normalized()
        pivot = c + hw * r + n * (thick * 0.5)
        axis = n.cross(hw)          # (+ lifts the lid out along n: the middle lies at -hw from the hinge, and n x hw x (-hw) = +n)
        self.part(name, parent, pivot, axis=axis, limits=[0.0, opens], kind="turn", note="a hatch: + opens")
        sides = self.n(24, 10)
        self.lathe(c, n, [(thick + dome, 0.0), (thick + dome * 0.7, r * 0.6), (thick, r * 0.94), (thick * 0.6, r), (0.0, r), (0.0, 0.0)], paint, sides)
        if self.fine:
            self.rod(pivot - axis * 0.06, pivot + axis * 0.06, 0.014, paint, 8)
            self.box((0.05, 0.05, 0.012), c + hw * (r * 0.8) + n * (thick + 0.004), paint, turn=None)
            if handle:
                far = c - hw * (r * 0.55) + n * (thick + dome * 0.6)
                self.handle(far - axis * 0.05, far + axis * 0.05, n, 0.007, 0.03, "steel")
        if well:
            self.use(parent)
            self.lathe(c, n, [(0.002, 0.0), (0.002, r * 0.9)], "black", sides)
        self.use(parent)
        return name

    # ---- the roof machine gun only the commander fires ------------------------------------------------
    def roof_gun(self, kind, foot, arc, lay, parent="turret", ring=None, reach=None, high=None):
        """The machine gun on the turret roof (Brian, 22 November: it fires only while the commander is up in his
        hatch and has hold of it; the gun beside the main gun and the hull gun do the rest). Two parts the game aims:
        `roof_mg` swings it round (+ to the right, clockwise from above; limits `arc`) and its child `roof_mg_cradle`
        tips it (+ muzzle up; limits `lay`). On the cradle: socket `roof_mg` at the muzzle along the barrel, and
        `grip.roof_mg.l` / `grip.roof_mg.r` where his hands go.

        kind "m2": America's .50 heavy machine gun on a pintle post in front of the commander's hatch, in his reach
        (`foot`: the post's foot on the roof; `high`: the trunnions over it). Its whole body lies behind the
        trunnions, the spade grips at its back plate, a box of 105 rounds on the tray at its left.
        kind "mg34": Germany's MG 34 on the anti-aircraft rail round the cupola's top (`foot`: the middle of the
        cupola's top; `ring`: the rail's radius; `reach`: how far out the gun's trunnions stand). Its mount rides the
        rail, so `roof_mg` turns about the cupola's own middle: the gun looks out over the rim wherever the commander,
        standing in the cupola, turns to face. The belt drum on its left; his left hand over the butt, his right on
        the pistol grip.

        Both parts are made on the plain mesh too (the far mesh indexes the same parts), where the gun is only a lump."""
        f = Vector(foot)
        fine = self.fine
        steel, brass = "steel", self.own_paint("brass", "7E6A3E")
        self.use(parent)
        if kind == "m2":
            high = high or 0.30
            t = f + Z * high
            b = t.z + 0.045                                  # the bore's height
            # the pintle's socket on the roof and its post (fixed to the turret)
            if fine:
                self.lathe(f - Z * 0.02, Z, [(0.05, 0.0), (0.05, 0.045), (0.025, 0.05), (0.0, 0.075), (0.0, 0.0)], "upper", self.n(18, 8))
                self.bolts(f + Z * 0.012, Z, 0.062, 4, r=0.009)
            self.rod(f, f + Z * (high - 0.10), 0.024, steel, 12)
            self.part("roof_mg", parent, f, axis=(0, 0, -1), limits=list(arc), kind="turn",
                      note="the roof machine gun on its post: + swings it to the right; the game aims it only while her commander is up and has hold of it")
            if fine:
                self.rod(f + Z * (high - 0.17), f + Z * (high - 0.07), 0.033, steel, 12)                  # the pintle's sleeve over the post
                for s in (LEFT, RIGHT):                                                                  # the cradle's two cheeks
                    self.slab((t.x - 0.045, t.y + s * 0.056, t.z - 0.10), (t.x + 0.05, t.y + s * 0.07, t.z + 0.025), steel, 0.004)
                self.slab((t.x - 0.045, t.y - 0.07, t.z - 0.11), (t.x + 0.05, t.y + 0.07, t.z - 0.085), steel, 0.004)
            self.part("roof_mg_cradle", "roof_mg", t, axis=(0, -1, 0), limits=list(lay), kind="turn", note="the gun tips on its trunnions: + muzzle up")
            if fine:
                self.rod((t.x, t.y - 0.078, t.z), (t.x, t.y + 0.078, t.z), 0.012, steel, 8)               # the trunnion pin
            # the receiver: a long flat box behind the trunnions, its feed cover on top, the back plate and spade grips
            self.slab((t.x - 0.42, t.y - 0.05, b - 0.105), (t.x + 0.04, t.y + 0.05, b + 0.035), steel, 0.008 if fine else 0.0)
            if fine:
                self.slab((t.x - 0.39, t.y - 0.044, b + 0.035), (t.x - 0.10, t.y + 0.044, b + 0.047), steel, 0.004)     # the feed cover
                self.slab((t.x - 0.37, t.y - 0.012, b + 0.047), (t.x - 0.355, t.y + 0.012, b + 0.085), steel, 0.002)   # the rear sight's leaf, up
                self.slab((t.x - 0.30, t.y - 0.052, b - 0.06), (t.x - 0.05, t.y - 0.05, b + 0.02), steel)              # (the side plates' rivet line)
                for k in range(5):
                    for s in (LEFT, RIGHT):
                        self.lathe((t.x - 0.36 + 0.07 * k, t.y + s * 0.0505, b - 0.075), Y * s, [(0.004, 0.0), (0.004, 0.006), (0.0, 0.007), (0.0, 0.0)], steel, 6)
                self.slab((t.x - 0.445, t.y - 0.062, b - 0.11), (t.x - 0.42, t.y + 0.062, b + 0.032), steel, 0.004)    # the back plate
                for s in (LEFT, RIGHT):
                    self.rod((t.x - 0.44, t.y + s * 0.045, b - 0.03), (t.x - 0.50, t.y + s * 0.045, b - 0.03), 0.009, steel, 8)
                    self.rod((t.x - 0.505, t.y + s * 0.045, b - 0.095), (t.x - 0.505, t.y + s * 0.045, b + 0.03), 0.0145, "wood", 10, r2=0.013)   # a spade grip
                self.slab((t.x - 0.475, t.y - 0.028, b - 0.04), (t.x - 0.462, t.y + 0.028, b - 0.018), steel, 0.002)     # the butterfly trigger between them
                # the barrel's perforated support at the receiver's front, the barrel, its muzzle and front sight
                self.lathe((t.x + 0.04, t.y, b), X, [(0.17, 0.0), (0.17, 0.031), (0.01, 0.034), (0.0, 0.034), (0.0, 0.0)], steel, self.n(16, 8))
                for row in range(3):
                    for k in range(6):
                        a = 2 * math.pi * (k + 0.5 * (row % 2)) / 6
                        w = Vector((0.0, math.cos(a), math.sin(a)))
                        self.lathe(Vector((t.x + 0.07 + row * 0.045, t.y, b)) + w * 0.0345, w, [(0.0, 0.0), (0.0, 0.0085)], "black", 6)
                self.rod((t.x + 0.21, t.y, b), (t.x + 1.19, t.y, b), 0.0235, steel, 12, r2=0.021)
                self.lathe((t.x + 1.17, t.y, b), X, [(0.035, 0.0), (0.035, 0.0245), (0.0, 0.0245), (0.0, 0.0)], steel, self.n(12, 6))
                self.lathe((t.x + 1.2052, t.y, b), X, [(0.0, 0.0), (0.0, 0.0085)], "black", 8)
                self.slab((t.x + 0.17, t.y - 0.004, b + 0.03), (t.x + 0.19, t.y + 0.004, b + 0.062), steel)
                # the box of rounds on its tray at the gun's left, and the belt running from it into the feed
                self.slab((t.x - 0.30, t.y + 0.05, b - 0.115), (t.x + 0.0, t.y + 0.175, b - 0.105), steel, 0.003)
                self.slab((t.x - 0.285, t.y + 0.058, b - 0.105), (t.x - 0.015, t.y + 0.168, b + 0.075), "upper", 0.006)
                self.slab((t.x - 0.29, t.y + 0.163, b + 0.07), (t.x - 0.01, t.y + 0.175, b + 0.15), "upper", 0.003)        # its lid, standing open on its hinge
                self.slab((t.x - 0.27, t.y + 0.068, b + 0.071), (t.x - 0.03, t.y + 0.158, b + 0.076), "black")            # the open mouth
                for k in range(6):                           # rounds in their links, rising out of the box and over into the feed
                    u = k / 5.0
                    c = Vector((t.x - 0.10, t.y + 0.125 - 0.075 * u, b + 0.072 + 0.03 * math.sin(math.pi * u * 0.8) - 0.03 * u * u))
                    self.lathe(c - X * 0.069, X, [(0.138, 0.0), (0.138, 0.005), (0.10, 0.0105), (0.0, 0.0105), (0.0, 0.0)], brass, 6)
                    self.slab(c - Vector((0.012, 0.007, 0.012)), c + Vector((0.012, 0.007, -0.006)), steel)              # its link
            else:
                self.slab((t.x - 0.285, t.y + 0.05, b - 0.105), (t.x - 0.015, t.y + 0.17, b + 0.075), "upper")
            muzzle, grips = Vector((t.x + 1.21, t.y, b)), [(s, Vector((t.x - 0.505, t.y + s * 0.045, b - 0.03)), Z) for s in (LEFT, RIGHT)]
            grip_note = "the commander's {} hand on the {} spade grip (its middle; the handle stands upright)"
        else:
            ring = ring or 0.40
            reach = reach or ring + 0.08
            high = high or 0.27
            t = f + X * reach + Z * high
            b = t.z + 0.03
            # the rail round the cupola's top, on short stays (fixed to the turret)
            k_n = self.n(40, 12)
            rail = [f + Vector((math.cos(a) * ring, math.sin(a) * ring, 0.035)) for a in (2 * math.pi * k / k_n for k in range(k_n))]
            self.tube(rail, 0.011, "upper", 6, closed=True)
            if fine:
                for k in range(6):
                    a = 2 * math.pi * (k + 0.5) / 6
                    w = Vector((math.cos(a), math.sin(a), 0.0))
                    self.rod(f + w * (ring - 0.05) - Z * 0.05, f + w * ring + Z * 0.03, 0.009, "upper", 6)
            self.part("roof_mg", parent, f, axis=(0, 0, -1), limits=list(arc), kind="turn",
                      note="the roof machine gun's mount riding the rail round the cupola: + carries it round to the right about the cupola's middle; "
                           "the game aims it only while her commander is up and has hold of it (he turns with it)")
            # the carriage on the rail, and the arm up to the gun's trunnions
            self.box((0.10, 0.07, 0.045), f + X * ring + Z * 0.04, "upper", 0.006 if fine else 0.0)
            if fine:
                for s in (LEFT, RIGHT):
                    self.lathe(f + X * (ring - 0.03) + Y * (s * 0.036) + Z * 0.035, Y * s, [(0.012, 0.0), (0.012, 0.016), (0.0, 0.016), (0.0, 0.0)], steel, 8)
                    self.lathe(f + X * (ring + 0.03) + Y * (s * 0.036) + Z * 0.035, Y * s, [(0.012, 0.0), (0.012, 0.016), (0.0, 0.016), (0.0, 0.0)], steel, 8)
                self.tube([f + X * ring + Z * 0.06, f + X * (ring + 0.02) + Z * (high * 0.55), t - Z * 0.04], 0.016, "upper", 8)
                self.box((0.05, 0.075, 0.04), t - Z * 0.035, "upper", 0.004)
            self.part("roof_mg_cradle", "roof_mg", t, axis=(0, -1, 0), limits=list(lay), kind="turn", note="the gun tips in its mount: + muzzle up")
            bake = self.own_paint("bakelite", "2B2622")
            # the receiver, round, with its feed cover over it; behind it the butt, under it the pistol grip
            self.rod((t.x - 0.36, t.y, b), (t.x - 0.03, t.y, b), 0.026, steel, 14 if fine else 6)
            if fine:
                self.rod((t.x - 0.03, t.y, b), (t.x - 0.005, t.y, b), 0.03, steel, 14)                                  # the jacket's ring
                self.slab((t.x - 0.30, t.y - 0.036, b + 0.012), (t.x - 0.06, t.y + 0.036, b + 0.045), steel, 0.006)       # the feed cover
                self.slab((t.x - 0.075, t.y - 0.006, b + 0.045), (t.x - 0.06, t.y + 0.006, b + 0.075), steel)             # the rear sight
                stock = [(t.x - 0.355, 0.021, b + 0.022), (t.x - 0.355, 0.021, b - 0.035), (t.x - 0.62, 0.024, b + 0.012), (t.x - 0.62, 0.024, b - 0.105),
                         (t.x - 0.50, 0.019, b - 0.06)]
                self.put(bm_hull([(x, t.y + s * y, z) for x, y, z in stock for s in (1.0, -1.0)]), bake, 0.006)
                self.slab((t.x - 0.632, t.y - 0.026, b - 0.108), (t.x - 0.618, t.y + 0.026, b + 0.016), steel, 0.003)    # the butt plate
                self.tube([(t.x - 0.24, t.y, b - 0.02), (t.x - 0.255, t.y, b - 0.07), (t.x - 0.275, t.y, b - 0.13)], 0.016, bake, 8)     # the pistol grip
                self.tube([(t.x - 0.20, t.y, b - 0.025), (t.x - 0.205, t.y, b - 0.06), (t.x - 0.235, t.y, b - 0.065)], 0.004, steel, 4)   # the trigger guard
                # the jacket, full of holes; the muzzle's booster cone; the front sight
                self.lathe((t.x - 0.005, t.y, b), X, [(0.465, 0.0), (0.465, 0.0215), (0.0, 0.0215), (0.0, 0.0)], steel, 14)
                for k in range(8):
                    for a in (math.radians(d) for d in (30, 90, 150, 210, 270, 330)):
                        w = Vector((0.0, math.cos(a), math.sin(a)))
                        self.lathe(Vector((t.x + 0.03 + 0.052 * k + (0.026 if (int(math.degrees(a)) // 60) % 2 else 0.0), t.y, b)) + w * 0.022, w,
                                   [(0.0, 0.0), (0.0, 0.0065)], "black", 6)
                self.lathe((t.x + 0.46, t.y, b), X, [(0.11, 0.0), (0.11, 0.013), (0.075, 0.022), (0.0, 0.0235), (0.0, 0.0)], steel, 12)
                self.lathe((t.x + 0.5702, t.y, b), X, [(0.0, 0.0), (0.0, 0.0055)], "black", 8)
                self.slab((t.x + 0.40, t.y - 0.003, b + 0.02), (t.x + 0.415, t.y + 0.003, b + 0.05), steel)
                # the belt drum hung on the left of the feed, and the belt's end going in
                drum = Vector((t.x - 0.19, t.y + 0.085, b - 0.04))
                self.lathe(drum - X * 0.05, X, [(0.10, 0.0), (0.10, 0.058), (0.09, 0.064), (0.012, 0.068), (0.0, 0.066), (0.0, 0.0)], steel, 18)
                self.slab((t.x - 0.215, t.y + 0.02, b - 0.005), (t.x - 0.165, t.y + 0.055, b + 0.012), brass)
                self.handle(drum - X * 0.03 + Z * 0.066, drum + X * 0.03 + Z * 0.066, Z, 0.004, 0.018, steel)
            else:
                self.slab((t.x - 0.62, t.y - 0.02, b - 0.08), (t.x - 0.35, t.y + 0.02, b + 0.02), bake)
            muzzle = Vector((t.x + 0.575, t.y, b))
            grips = [(LEFT, Vector((t.x - 0.50, t.y, b + 0.03)), X), (RIGHT, Vector((t.x - 0.262, t.y, b - 0.08)), -Z)]
            grip_note = None
        self.socket("roof_mg", muzzle, X, part="roof_mg_cradle", note="the roof machine gun's muzzle, facing along the barrel")
        for s, at, way in grips:
            hand = "left" if s > 0 else "right"
            note = grip_note.format(hand, hand) if grip_note else ("the commander's left hand, laid over the top of the butt" if s > 0 else "the commander's right hand on the pistol grip (its middle)")
            self.socket(f"grip.roof_mg.{'l' if s > 0 else 'r'}", at, way, part="roof_mg_cradle", note=note)
        self.roof = {"kind": kind, "every": 0.11 if kind == "m2" else 0.07}
        self.use(parent)
        return t

    def cupola(self, c, r, high, slits=5, paint="upper", wall=0.05, ring=True, part=None, start=0.0, top_lip=0.03, sides=28):
        """A commander's cupola: a drum with vision slits round it. Returns the middle of its top (for the hatch)."""
        c = Vector(c)
        if part:
            self.m.use(part)
        sides = self.n(sides, 10)
        self.lathe(c, Z, [(high, r - wall), (high, r - 0.012), (high - 0.012, r), (high * 0.62, r * 1.04), (0.03, r * 1.04), (0.0, r * 1.1), (0.0, 0.0)], paint, sides)
        self.lathe(c, Z, [(high - 0.03, 0.0), (high - 0.03, r - wall), (high, r - wall)], "shade", sides)
        if self.fine:
            for k in range(slits):
                a = start + 2 * math.pi * k / slits
                d = Vector((math.cos(a), math.sin(a), 0))
                t = Vector((-d.y, d.x, 0))
                m = Matrix((d, t, Z)).transposed()
                bm = bm_box((0.05, r * 0.5, high * 0.3), (0, 0, 0))
                bm_move(bm, at=c + d * (r * 1.04) + Z * (high * 0.42), turn=m)
                self.put(bm, paint, 0.004)
                bm = bm_box((0.012, r * 0.36, 0.018), (0, 0, 0))
                bm_move(bm, at=c + d * (r * 1.04 + 0.022) + Z * (high * 0.44), turn=m)
                self.put(bm, "glass")
        return c + Z * high

    def periscope(self, at, facing=X, size=(0.07, 0.14, 0.07), paint="upper", part=None, up=Z):
        if not self.fine:
            return
        at = Vector(at)
        f = Vector(facing).normalized()
        yaw = math.degrees(math.atan2(f.y, f.x))
        if part:
            self.m.use(part)
        self.box(size, at + Z * (size[2] / 2), paint, 0.006, rot(yaw))
        self.box((0.006, size[1] * 0.8, size[2] * 0.45), at + Z * (size[2] * 0.6) + f * (size[0] / 2 + 0.001), "glass", turn=rot(yaw))

    def visor(self, at, facing, wide=0.34, high=0.12, thick=0.06, paint="upper", slit=True, part=None, brow=True):
        """A driver's visor block on a plate."""
        at, f = Vector(at), Vector(facing).normalized()
        yaw = math.degrees(math.atan2(f.y, f.x))
        pitch = math.degrees(math.asin(max(-1.0, min(1.0, f.z))))
        m = rot(yaw, pitch)
        if part:
            self.m.use(part)
        self.put(bm_box((thick, wide, high), at + f * (thick / 2), m), paint, 0.008)
        if self.fine:
            if slit:
                self.put(bm_box((0.006, wide * 0.62, high * 0.2), at + f * (thick + 0.001), m), "glass")
            if brow:
                self.put(bm_box((thick * 1.5, wide * 1.06, 0.012), at + f * (thick * 0.75) + (m @ Z) * (high / 2 + 0.006), m), paint)
            for s in (-1, 1):
                self.bolt(at + f * thick + (m @ Y) * (s * wide * 0.42) + (m @ Z) * (-high * 0.3), f, 0.011)

    def port(self, at, facing, r=0.045, paint="upper", part=None):
        """A pistol or vision port: a round plug in a plate."""
        if not self.fine:
            return
        if part:
            self.m.use(part)
        self.lathe(Vector(at), facing, [(0.022, 0.0), (0.022, r * 0.7), (0.012, r), (0.0, r * 1.1), (0.0, 0.0)], paint, 12)

    def vent(self, at, r=0.11, high=0.05, paint="upper", part=None):
        """An armoured ventilator on a roof: a low mushroom."""
        if part:
            self.m.use(part)
        self.lathe(Vector(at), Z, [(high, 0.0), (high, r * 0.7), (high * 0.7, r), (high * 0.45, r), (high * 0.4, r * 0.55), (0.0, r * 0.55), (0.0, 0.0)], paint, self.n(18, 8))

    def lift_eye(self, at, facing=Z, across=Y, r=0.035, paint="upper", part=None):
        """A lifting eye: a half ring standing on a plate."""
        if not self.fine:
            return
        at, f, a = Vector(at), Vector(facing).normalized(), Vector(across).normalized()
        pts = [at + a * (math.cos(t) * r) + f * (math.sin(t) * r) for t in (math.pi * k / 6 for k in range(7))]
        if part:
            self.m.use(part)
        self.tube(pts, r * 0.24, paint, 6)

    def hook(self, at, facing=X, size=0.09, paint="upper", part=None):
        """A towing eye plate with its shackle."""
        at, f = Vector(at), Vector(facing).normalized()
        yaw = math.degrees(math.atan2(f.y, f.x))
        if part:
            self.m.use(part)
        self.put(bm_box((size * 1.2, size * 0.35, size * 1.3), at + f * (size * 0.5), rot(yaw)), paint, 0.008)
        if self.fine:
            c = at + f * (size * 1.0)
            side = Vector((-f.y, f.x, 0))
            pts = [c + side * (s * size * 0.36) + f * (math.sin(t) * size * 0.75) - Z * ((1 - math.cos(t)) * size * 0.2) for s, t in ((1, 0.0), (1, 0.9), (0.6, 1.4), (0, 1.57), (-0.6, 1.4), (-1, 0.9), (-1, 0.0))]
            self.tube(pts, size * 0.11, "steel", 6)

    # ---- lamps, aerials --------------------------------------------------------------------------------
    def headlamp(self, at, facing=X, r=0.075, paint="upper", stalk=0.08, cover=False, part=None):
        at, f = Vector(at), Vector(facing).normalized()
        if part:
            self.m.use(part)
        sides = self.n(16, 8)
        self.lathe(at + Z * stalk, f, [(r * 0.5, r * 0.96), (r * 0.5, r), (0.0, r), (-r * 0.6, r * 0.78), (-r * 0.95, r * 0.35), (-r * 1.0, 0.0)], paint, sides)
        # (the glass is a shallow dome of dark glass: a flat bright one mirrors the sky and reads as a lit white disc)
        self.lathe(at + Z * stalk, f, [(r * 0.64, 0.0), (r * 0.62, r * 0.36), (r * 0.57, r * 0.7), (r * 0.5, r * 0.96)], "upper" if cover else "lamp", sides)
        if cover and self.fine:
            self.box((0.004, r * 1.0, r * 0.12), at + Z * stalk + f * (r * 0.5 + 0.002), "lamp", turn=rot(math.degrees(math.atan2(f.y, f.x))))
        if stalk > 0 and self.fine:
            self.rod(at, at + Z * (stalk - r * 0.6), r * 0.22, paint, 8)

    def aerial(self, name, parent, at, high=2.0, r=0.006, base=0.035, lean=(0.0, 0.0), paint="steel"):
        """A rod aerial on a sprung base, as a part that can whip."""
        at = Vector(at)
        self.part(name, parent, at, axis=(0, -1, 0), limits=[-25.0, 25.0], kind="turn", note="the aerial: whips fore and aft")
        self.lathe(at, Z, [(base * 2.2, 0.0), (base * 2.2, base * 0.45), (base * 1.2, base * 0.8), (0.0, base), (0.0, 0.0)], "black", 10)
        top = at + Vector((lean[0], lean[1], high))
        self.lathe(at + Z * (base * 2.0), top - at, [(high, 0.0), (high, r * 0.5), (0.0, r), (0.0, 0.0)], paint, 6)
        self.socket("aerial", top, Z)
        self.use(parent)

    # ---- tools and stowage ---------------------------------------------------------------------------
    def shovel(self, at, yaw=0.0, long=0.95, part=None, tilt=0.0, roll=0.0):
        if not self.fine:
            return
        m = rot(yaw, tilt, roll)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.rod(at, at + m @ Vector((long * 0.68, 0, 0)), 0.016, "wood", 8)
        blade = [(long * 0.66, -0.035), (long * 0.72, -0.09), (long * 0.97, -0.085), (long, 0.0), (long * 0.97, 0.085), (long * 0.72, 0.09), (long * 0.66, 0.035)]
        bm = bm_prism(blade, -0.005, 0.005)
        bm_move(bm, at=at, turn=m)
        self.put(bm, "steel")
        self.rod(at + m @ Vector((-0.04, -0.05, 0)), at + m @ Vector((-0.04, 0.05, 0)), 0.014, "wood", 6)

    def axe(self, at, yaw=0.0, long=0.8, part=None, tilt=0.0, roll=0.0):
        if not self.fine:
            return
        m = rot(yaw, tilt, roll)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.rod(at, at + m @ Vector((long, 0, 0)), 0.015, "wood", 8)
        head = [(long * 0.86, -0.02), (long * 0.84, 0.12), (long * 0.99, 0.14), (long * 0.97, -0.02)]
        bm = bm_prism(head, -0.014, 0.014)
        bm_move(bm, at=at, turn=m)
        self.put(bm, "steel")

    def crowbar(self, at, yaw=0.0, long=1.3, part=None, tilt=0.0):
        if not self.fine:
            return
        m = rot(yaw, tilt)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.tube([at + m @ Vector(p) for p in ((0, 0, 0), (long * 0.9, 0, 0), (long * 0.97, 0, 0.012), (long, 0, 0.04))], 0.011, "steel", 6)

    def jack(self, at, yaw=0.0, part=None, long=0.52):
        if not self.fine:
            return
        m = rot(yaw)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.put(bm_box((long, 0.12, 0.13), at + Z * 0.065, m), "upper", 0.008)
        self.rod(at + m @ Vector((-long * 0.6, 0, 0.07)), at + m @ Vector((long * 0.6, 0, 0.07)), 0.03, "upper", 10)
        self.put(bm_box((0.05, 0.18, 0.05), at + m @ Vector((long * 0.3, 0, 0.14)), m), "upper")

    def jack_block(self, at, size=(0.42, 0.22, 0.13), yaw=0.0, part=None):
        """The wooden block a jack stands on, in its steel straps."""
        if not self.fine:
            return
        m = rot(yaw)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.put(bm_box(size, at + Z * size[2] / 2, m), "wood", 0.006)
        for s in (-0.3, 0.3):
            self.put(bm_box((0.03, size[1] + 0.012, size[2] + 0.008), at + m @ Vector((s * size[0], 0, size[2] / 2)), m), "upper")

    def jerrycan(self, at, yaw=0.0, paint="upper", part=None, lay=False):
        """A 20-litre can."""
        m = rot(yaw, 0.0, 90.0 if lay else 0.0)
        at = Vector(at)
        if part:
            self.m.use(part)
        w, d, h = 0.34, 0.16, 0.40
        lift = Vector((0, 0, d / 2 if lay else h / 2))
        self.put(bm_box((w, d, h), at + lift, m), paint, 0.022, segs=2)
        if self.fine:
            top = at + lift + m @ Vector((0, 0, h / 2))
            for s in (-1, 0, 1):
                self.tube([top + m @ Vector(p) for p in ((-0.07, s * 0.035, 0.0), (-0.06, s * 0.035, 0.03), (0.02, s * 0.035, 0.03), (0.03, s * 0.035, 0.0))], 0.007, paint, 5)
            self.rod(top + m @ Vector((0.11, 0, -0.005)), top + m @ Vector((0.13, 0, 0.04)), 0.022, paint, 8)
            # the pressed X in its side
            for s in (1, -1):
                for a in (0.86, -0.86):
                    self.put(bm_box((0.012, 0.004, 0.3), at + lift + m @ Vector((-0.02, s * (d / 2 + 0.001), -0.02)), m @ Matrix.Rotation(a, 3, "Y")), paint)

    def bin(self, a, b, paint="upper", bevel=0.012, lid=True, part=None, latches=2, lid_side=None):
        """A stowage bin by two opposite corners, with a lid line and latches on the side `lid_side` (a unit way)."""
        a, b = Vector(a), Vector(b)
        if part:
            self.m.use(part)
        self.slab(a, b, paint, bevel, segs=2)
        if not self.fine or not lid:
            return
        lo = Vector((min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)))
        hi = Vector((max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)))
        c = (lo + hi) / 2
        s = hi - lo
        self.slab((lo.x - 0.006, lo.y - 0.006, hi.z - 0.035), (hi.x + 0.006, hi.y + 0.006, hi.z + 0.004), paint, 0.006)
        ls = Vector(lid_side) if lid_side is not None else Vector((0, 1 if c.y >= 0 else -1, 0))
        across = Vector((-ls.y, ls.x, 0))
        reach = abs(ls.x) * s.x / 2 + abs(ls.y) * s.y / 2
        span = abs(across.x) * s.x + abs(across.y) * s.y
        for k in range(latches):
            t = (k + 0.5) / latches - 0.5
            p = Vector((c.x, c.y, hi.z - 0.05)) + ls * (reach + 0.008) + across * (t * span * 0.7)
            self.box((0.02 + 0.006 * abs(ls.x), 0.02 + 0.006 * abs(ls.y), 0.06), p, "steel")

    def roll(self, a, b, r=0.11, paint="canvas", straps=2, part=None, sag=0.0):
        """A rolled tarpaulin or bedroll between two points, with straps."""
        a, b = Vector(a), Vector(b)
        if part:
            self.m.use(part)
        ax = (b - a)
        L = ax.length
        g = self.rng
        k = 7 if self.fine else 2
        prof = [(L, 0.0), (L, r * 0.55), (L - r * 0.25, r * 0.95)]
        for i in range(k - 1, 0, -1):
            prof.append((L * i / k, r * (1.0 + g.uniform(-0.06, 0.06))))
        prof += [(r * 0.25, r * 0.95), (0.0, r * 0.55), (0.0, 0.0)]
        self.lathe(a, ax, prof, paint, self.n(14, 6))
        if self.fine:
            for i in range(straps):
                t = (i + 0.5) / straps
                self.lathe(a, ax, [(L * t + 0.02, r * 0.9), (L * t + 0.02, r * 1.04), (L * t - 0.02, r * 1.04), (L * t - 0.02, r * 0.9)], "leather", 14)

    def crate(self, at, size=(0.55, 0.3, 0.25), yaw=0.0, paint="wood", part=None):
        m = rot(yaw)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.put(bm_box(size, at + Z * size[2] / 2, m), paint, 0.006)
        if self.fine:
            for s in (-0.36, 0.36):
                self.put(bm_box((0.035, size[1] + 0.014, size[2] + 0.01), at + m @ Vector((s * size[0], 0, size[2] / 2)), m), paint)
            for s in (-1, 1):
                self.tube([at + m @ Vector(p) for p in ((s * size[0] / 2, -0.06, size[2] * 0.65), (s * (size[0] / 2 + 0.03), -0.05, size[2] * 0.55), (s * (size[0] / 2 + 0.03), 0.05, size[2] * 0.55), (s * size[0] / 2, 0.06, size[2] * 0.65))], 0.006, "rope", 5)

    def bucket(self, at, r=0.13, high=0.26, paint=None, part=None):
        """A pail: dull galvanised grey, its inside a darker grey, its wire handle hanging down one side (a near-black
        pail seen from above reads as a top hat)."""
        if not self.fine:
            return
        if part:
            self.m.use(part)
        at = Vector(at)
        paint = paint or self.own_paint("zinc", "686D66")
        self.lathe(at, Z, [(high, r * 0.94), (high, r), (0.0, r * 0.78), (0.0, 0.0)], paint, 14)
        self.lathe(at, Z, [(high * 0.3, 0.0), (high * 0.3, r * 0.8), (high, r * 0.94)], self.own_paint("zinc.in", "4A4E49"), 14)
        tip = math.radians(105.0)                           # the handle, swung down past level to lie on her side
        self.tube([at + Vector((1.06 * r * math.cos(a), 1.06 * r * math.sin(a) * math.sin(tip), high + 1.06 * r * math.sin(a) * math.cos(tip)))
                   for a in [math.pi * k / 8 for k in range(9)]], 0.004, "steel", 5)

    def belt_box(self, at, yaw=0.0, paint=None, part=None):
        """A machine gun's belt box standing on a floor: dark field grey, its lid a lip wider, a carrying handle on it,
        a catch on its side. `at` is the floor under its middle; it lies along `yaw`."""
        if part:
            self.m.use(part)
        m = rot(yaw)
        at = Vector(at)
        paint = paint or self.own_paint("box.grey", "4A5446")
        self.put(bm_box((0.26, 0.085, 0.148), at + Z * 0.074, m), paint, 0.005)
        self.put(bm_box((0.268, 0.093, 0.024), at + Z * 0.160, m), paint, 0.004)
        if self.fine:
            self.put(bm_box((0.014, 0.005, 0.04), at + m @ Vector((0.0, 0.0485, 0.142)), m), "steel")
            self.handle(at + m @ Vector((-0.05, 0.0, 0.172)), at + m @ Vector((0.05, 0.0, 0.172)), Z, r=0.005, reach=0.018, paint="steel")

    def lamp_guard(self, lamp, r, on, out, across, up, bar=0.0125, paint="upper", part=None):
        """A brush guard over a headlamp that stands off a sloped plate: two hoops of round bar crossing over the lamp,
        their feet on the plate, and the lamp's stem carried down to the plate. lamp: the lamp's middle, r its
        radius; on: any point of the plate; out: the plate's facing; across, up: two directions along it. Each hoop
        is two straight legs and a half-round top, in stout bar and her own paint: thin dark rods splayed from the
        lamp read as a spider's legs."""
        if not self.fine:
            return
        if part:
            self.m.use(part)
        lamp, on, out, across, up = Vector(lamp), Vector(on), Vector(out).normalized(), Vector(across).normalized(), Vector(up).normalized()
        off = (lamp - on).dot(out)                          # how far the lamp's middle stands off the plate
        foot = lamp - out * off
        wide = r + 0.045
        leg = max(0.03, off + r + 0.04 - wide)
        for along in (across, up):
            pts = [foot - along * wide - out * 0.004, foot - along * wide + out * leg]
            pts += [foot - along * (wide * math.cos(math.pi * k / 8)) + out * (leg + wide * math.sin(math.pi * k / 8)) for k in range(1, 8)]
            pts += [foot + along * wide + out * leg, foot + along * wide - out * 0.004]
            self.tube(pts, bar, paint, 8)
        if out.z > 0.2:                                     # her stem, straight down to the plate
            self.rod(lamp - Z * (off / out.z + 0.004), lamp - Z * 0.07, r * 0.27, paint, 8)

    def cable(self, points, r=0.012, paint="steel", eyes=True, part=None):
        """A tow cable laid along a row of points, an eye at each end."""
        if not self.fine:
            return
        if part:
            self.m.use(part)
        pts = [Vector(p) for p in points]
        self.tube(pts, r, paint, 6)
        if eyes:
            for end, nxt in ((pts[0], pts[1]), (pts[-1], pts[-2])):
                d = (end - nxt).normalized()
                _, side, up = frame(d)
                if abs(d.z) < 0.9:
                    side = d.cross(Z).normalized()
                c = end + d * 0.06
                ring = [c + d * (math.cos(t) * 0.06) + side * (math.sin(t) * 0.04) for t in (2 * math.pi * k / 10 for k in range(10))]
                self.tube(ring, r * 1.25, paint, 5, closed=True)

    def spare_links(self, a, b, count, pitch, wide, facing, paint="upper", thick=0.03, part=None, horn=False, style=None, bar=True, dust=0.5):
        """A run of spare track links lying on a plate from a toward b, their backs to the plate, held by a flat bar
        across them. The links are the same links the track is made of, painted over in her coat as the crews did:
        hung spares do no work, so nothing on them is polished and they are never brighter than the plate they hang on."""
        if not self.fine:
            return
        a, b = Vector(a), Vector(b)
        f = Vector(facing).normalized()
        along = (b - a).normalized()
        across = f.cross(along)
        st = dict(style or {})
        st.setdefault("paint", paint)
        if not horn:
            st["horn"] = None
        shape = self.link_shape(st, pitch, wide, thick)
        if part:
            self.m.use(part)
        # (hung by hand and knocked about since: one or two of a row sit a few degrees off true, none quite in line;
        #  and nothing wipes them, so the road's dust lies on them as it does on the plate behind: `dusted`, which
        #  Works.worn reads)
        rs = random.Random(zlib.crc32(repr((tuple(round(x, 3) for x in a), count)).encode()))
        askew = set(rs.sample(range(count), min(count, 1 + count // 4)))
        if not hasattr(self, "dusted"):
            self.dusted = []
        self.dusted.append((a + along * (pitch * count / 2) + f * thick * 0.5, math.hypot(pitch * count / 2, wide / 2) + 0.03, dust))
        for k in range(count):
            c = a + along * (pitch * (k + 0.5)) + f * 0.004 + across * rs.uniform(-0.006, 0.006)
            turn = math.radians(rs.choice((-1, 1)) * rs.uniform(2.5, 4.5) if k in askew else rs.uniform(-0.6, 0.6))
            al, ac = along * math.cos(turn) + across * math.sin(turn), across * math.cos(turn) - along * math.sin(turn)
            for vs, fs, pa, shine in shape:
                before = len(self.m.faces)
                self.m.add([c + al * u + ac * v + f * w for u, v, w in vs], fs, pa, False, None, part)
                # (no polish and no wear: on the coat a flag is edge wear, and nothing rubs a hung spare)
                self.flag(before, None)
        if bar:
            m = Matrix((along, across, f)).transposed()
            for off in (-wide * 0.3, wide * 0.3):
                bm = bm_box((pitch * count + 0.05, 0.03, 0.006), (0, 0, 0))
                bm_move(bm, at=a + along * (pitch * count / 2) + across * off + f * (thick + 0.02), turn=m)
                self.put(bm, "upper", part=part)
    def silencer(self, a, b, r, paint="rust", bands=2, pipe=None, part=None, tail=None, sides=20):
        """A silencer drum from a to b; `pipe` the points of its inlet pipe; `tail` (point, way) of its outlet stub."""
        a, b = Vector(a), Vector(b)
        if part:
            self.m.use(part)
        L = (b - a).length
        e = min(r * 0.35, L * 0.12)
        self.lathe(a, b - a, [(L, 0.0), (L, r * 0.6), (L - e, r), (e, r), (0.0, r * 0.6), (0.0, 0.0)], paint, self.n(sides, 8))
        if self.fine:
            for k in range(bands):
                t = L * (k + 1) / (bands + 1)
                self.lathe(a, b - a, [(t + 0.015, r), (t + 0.015, r * 1.035), (t - 0.015, r * 1.035), (t - 0.015, r)], "steel", self.n(sides, 8))
            if pipe:
                self.tube(pipe, r * 0.3, paint, 8)
        if tail:
            p, way = Vector(tail[0]), Vector(tail[1]).normalized()
            self.lathe(p, way, [(0.12, r * 0.2), (0.12, r * 0.26), (0.0, r * 0.26)], paint, 10)
            self.lathe(p, way, [(0.02, 0.0), (0.02, r * 0.2), (0.12, r * 0.2)], "black", 10)

    def grille(self, c, size, up=Z, along=X, slats=8, paint="upper", frame_w=0.025, deep=0.02, part=None, mesh=False):
        """An engine grille: a frame with slats over the dark of the engine room. `along` is the way the slats run."""
        c = Vector(c)
        n = Vector(up).normalized()
        u = Vector(along)
        u = (u - n * u.dot(n)).normalized()
        v = n.cross(u)
        M = Matrix((u, v, n)).transposed()
        if part:
            self.m.use(part)

        def bx(sz, at, p, bev=0.0):
            bm = bm_box(sz, (0, 0, 0))
            bm_move(bm, at=c + M @ Vector(at), turn=M)
            self.put(bm, p, bev)
        bx((size[0] - frame_w, size[1] - frame_w, 0.004), (0, 0, 0.002), "black")
        if not self.fine:
            bx((size[0], size[1], deep * 0.6), (0, 0, deep * 0.3), paint)
            return
        for s in (-1, 1):
            bx((size[0], frame_w, deep), (0, s * (size[1] - frame_w) / 2, deep / 2), paint, 0.004)
            bx((frame_w, size[1], deep), (s * (size[0] - frame_w) / 2, 0, deep / 2), paint, 0.004)
        for k in range(slats):
            t = (k + 0.5) / slats - 0.5
            bm = bm_box((size[0] - frame_w * 2, 0.006, deep * 1.3), (0, 0, 0))
            bm_move(bm, turn=Matrix.Rotation(math.radians(32.0), 3, "X"))
            bm_move(bm, at=c + M @ Vector((0, t * (size[1] - frame_w * 2), deep * 0.55)), turn=M)
            self.put(bm, paint)

    def louvres(self, c, size, facing, count=5, paint="upper", part=None, down=Z):
        """Pressed louvres in a side plate: a row of small hoods over slots."""
        if not self.fine:
            return
        c = Vector(c)
        f = Vector(facing).normalized()
        d = Vector(down).normalized()
        u = d.cross(f).normalized()
        if part:
            self.m.use(part)
        M = Matrix((u, d, f)).transposed()
        for k in range(count):
            t = (k + 0.5) / count - 0.5
            at = c + d * (t * size[1])
            bm = bm_box((size[0], size[1] / count * 0.55, 0.004), (0, 0, 0))
            bm_move(bm, at=at + f * 0.001, turn=M)
            self.put(bm, "black")
            bm = bm_box((size[0] + 0.02, 0.006, 0.03), (0, 0, 0))
            bm_move(bm, turn=Matrix.Rotation(math.radians(-35.0), 3, "X"))
            bm_move(bm, at=at - d * (size[1] / count * 0.3) + f * 0.012, turn=M)
            self.put(bm, paint)


def lerp_r(r0, r1, t):
    return r0 + (r1 - r0) * t
