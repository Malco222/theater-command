"""Carriers: the lorry every army's infantry rides in, and the two armoured half-tracks.

    Truck         a 3-ton 4x2 cargo lorry of about 1939, our own design in the manner of its years and of no one
                  maker: a long bonnet behind an upright radiator, swept mudguards, a square steel cab, a planked
                  body with benches under a canvas tilt on five hoops.
    Hanomag       the German armoured half-track: a long faceted body widest at a knuckle line, a pointed nose,
                  front wheels under swept guards, interleaved road wheels under a flat track guard, two doors
                  in the stern, a machine gun behind a shield over the cab.
    M3Halftrack   the American half-track: a lorry's bonnet behind armoured shutters and a roller, a square box
                  of flat plate behind an armoured windscreen, a short band track on a four-wheel bogie between
                  a big sprocket and idler, a heavy machine gun on a pedestal.

PARTS   vehicle (root) > hull (the body, rocks on its springs) > tailboard | door_l, door_r | door_rear; mg_mount
        (trains) ; tilt (the lorry's canvas: the swap `tilt` takes it off)
        vehicle > steer_l > wheel_front_l, steer_r > wheel_front_r (the front wheels: the game steers and rolls them)
        vehicle > wheel_back_l, wheel_back_r (a lorry's back wheels), or a half-track's running gear as a tank's
CLIPS   idle, start, drive, move_off, stop, hit, knocked_out, burn; dismount (the tailboard dropped, the stern doors
        thrown open: the men get down; play it backward to close up); mg_burst (the half-tracks)
SOCKETS driver, rider.N (a seated man, facing the socket's way), gunner; hook (a towed gun's eye goes on it);
        engine, exhaust.1, fire.N, hit.*, dust.*; mg
Every shape is our own mesh, made from numbers here.
"""

import math

import bmesh
from mathutils import Matrix, Quaternion, Vector

import rigid
from guns import GunWorks, sheet
from motorpool import LEFT, RIGHT, SIDES, X, Y, Z
from tanks_any import rivets, turn_to
from tanks_ger import GER_COATS, coat, coats


def wall(v, quad, inward, thick=0.014, paint=coat, bevel=0.003):
    """A plate: its outer face on `quad` (four corners going round), `thick` deep toward `inward`."""
    q = [Vector(p) for p in quad]
    n = Vector(inward).normalized() * thick
    v.hull([tuple(p) for p in q] + [tuple(p + n) for p in q], paint, bevel, both=False)


def outward(quad, away_from):
    """The unit way a plate faces: square to it, on the side away from the point `away_from`."""
    a, b, c = Vector(quad[0]), Vector(quad[1]), Vector(quad[2])
    n = (b - a).cross(c - a).normalized()
    return n if n.dot(a - Vector(away_from)) > 0 else -n


class CarrierWorks(GunWorks):
    # ---- wheels on tyres -----------------------------------------------------------------------------------
    def lorry_wheel(self, name, parent, c, s, r, w=0.2, rr=None, twin=False, bolts=6, holes=0):
        """A wheel on a pneumatic tyre, a part that rolls. twin: two tyres side by side on one hub."""
        c, out = Vector(c), Vector((0.0, s, 0.0))
        rr = rr or r * 0.56
        self.part(name, parent, c, axis=Y, kind="spin", note=f"rolls: + forward; radius {r:.3f} m")
        self.wheels.append((name, r))
        offs = (-(w / 2 + 0.012), w / 2 + 0.012) if twin else (0.0,)
        for o in offs:
            self.tyre(c + out * o, out, r, rr, w, lugs=20)
        self.wheel_disc(c + out * (offs[-1] + w * 0.1), out, rr * 1.05, w * 0.6, tyre=0.0, kind="dish", hub=0.42, bolts=bolts, holes=holes, dish=0.2, cap=0.75, sides=32)
        if twin and self.fine:      # (the inner wheel's disc, seen from under her)
            self.wheel_disc(c + out * (offs[0] - w * 0.1), out, rr * 1.05, w * 0.6, tyre=0.0, kind="flat", hub=0.42, bolts=0, face=-1.0, sides=self.n(24, 10), simple=True)
        back = offs[0] - w * 0.5
        self.lathe(c + out * (back - 0.07), out, [(0.08, 0.0), (0.08, rr * 0.74), (0.0, rr * 0.74), (0.0, 0.0)], "shade", self.n(18, 8))        # the brake drum
        return name

    def steered_wheel(self, letter, s, x, y, r, w=0.2, rr=None, lock=32.0, **kw):
        """A front wheel on its kingpin: steer_<letter> turns it (the game steers), wheel_front_<letter> rolls."""
        self.part(f"steer_{letter}", "vehicle", (x, s * (y - w * 0.75), r), axis=(0, 0, -1), limits=[-lock, lock], kind="turn",
                  note="a front wheel on its kingpin: + steers to the right")
        self.rod((x, s * (y - w * 0.75), r - 0.1), (x, s * (y - w * 0.75), r + 0.1), 0.028, "shade", 8)
        return self.lorry_wheel(f"wheel_front_{letter}", f"steer_{letter}", (x, s * y, r), s, r, w, rr, **kw)

    def leaf_spring(self, xa, y, z_end, z_mid, half, leaves=5, wide=0.06, part=None):
        """A leaf spring along x: its ends hung at z_end, its middle clamped on the axle at z_mid, the leaves stepped."""
        halves = [half * (1.0 - 0.17 * k) for k in range(leaves)]
        stations = sorted({round(xa + sgn * h, 4) for h in halves for sgn in (-1, 1)} | {round(xa, 4)})
        top = lambda x: z_mid + (z_end - z_mid) * ((x - xa) / half) ** 2          # noqa: E731
        deep = lambda x: 0.011 * sum(1 for h in halves if abs(x - xa) <= h + 1e-6)   # noqa: E731
        prof = [(x, top(x)) for x in stations] + [(x, top(x) - deep(x)) for x in reversed(stations)]
        if part:
            self.use(part)
        self.extrude(prof, y - wide / 2, y + wide / 2, "shade")
        self.slab((xa - 0.05, y - wide / 2 - 0.012, z_mid - leaves * 0.011 - 0.03), (xa + 0.05, y + wide / 2 + 0.012, z_mid + 0.02), "shade", 0.004)
        for sgn in (-1, 1):       # the hangers
            self.slab((xa + sgn * half - 0.025, y - wide / 2 - 0.008, z_end - 0.03), (xa + sgn * half + 0.025, y + wide / 2 + 0.008, z_end + 0.07), "shade", 0.004)

    def mudguard(self, c, r0, a0, a1, y0, y1, tail=None, thick=0.014, paint=coat, step=11):
        """A swept mudguard over a wheel at c (x, z): an arc of plate from a0 to a1 degrees (0 forward, 90 over the
        top), carried across from y0 to y1; `tail`: more points (x, z) it runs on to behind the wheel."""
        n = max(2, int(round((a1 - a0) / step)))
        outer = [(c[0] + r0 * math.cos(math.radians(a0 + (a1 - a0) * i / n)), c[1] + r0 * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
        outer += list(tail or [])
        inner = []
        for i in range(len(outer) - 1, -1, -1):     # the under face, `thick` below, square to the run of the plate
            a, b = outer[max(i - 1, 0)], outer[min(i + 1, len(outer) - 1)]
            t = Vector((b[0] - a[0], b[1] - a[1])).normalized()
            inner.append((outer[i][0] - t.y * thick, outer[i][1] + t.x * thick))
        self.extrude(outer + inner, min(y0, y1), max(y0, y1), paint, 0.004)

    # ---- what a carrier does ---------------------------------------------------------------------------------
    def carrier_clips(self, idle_cue, drive_cue, dismount=(), mg=None, soft=1.0):
        """The clips a lorry or a half-track has. dismount: (part, degrees, when) for each board or door that is
        thrown open for the men to get down; mg: the socket her machine gun fires from; soft: how soft her springs
        are beside a tank's."""
        m = self.m
        two_pi = 2 * math.pi
        ev = lambda t, fx, socket=None, cue=None: {"t": round(t, 3), "fx": fx, "socket": socket, "cue": cue}      # noqa: E731
        ex = [s["name"] for s in m.sockets if s["name"].startswith("exhaust")]
        fires = [s["name"] for s in m.sockets if s["name"].startswith("fire.")]
        steers = [p for p in ("steer_l", "steer_r") if p in m.part_at]
        k = soft
        m.clips["idle"] = {"long": 2.0, "loop": True, "about": "engine running, standing still",
                           "tracks": [self.hull_keys(2.0, roll=lambda t: 0.07 * k * math.sin(two_pi * 5 * t), bob=lambda t: 0.0016 * math.sin(two_pi * 7 * t))],
                           "events": [ev(0.0, "engine_idle", "engine", idle_cue)] + [ev(0.0, "exhaust_idle", e) for e in ex]}
        m.clips["start"] = {"long": 1.5, "loop": False, "after": "hold", "about": "the engine is started: she shudders, a puff from the exhaust",
                            "tracks": [self.hull_keys(1.5, roll=lambda t: 0.4 * k * math.exp(-2.2 * t) * math.sin(two_pi * 6 * t), bob=lambda t: 0.004 * math.exp(-2.0 * t) * math.sin(two_pi * 8 * t))],
                            "events": [ev(0.0, "engine_start", "engine")] + [ev(0.15, "exhaust_start", e) for e in ex]}
        m.clips["drive"] = {"long": 2.0, "loop": True, "about": "under way: the body's own motion on its springs (the game rolls and steers the wheels, runs the tracks and moves her)",
                            "tracks": [self.hull_keys(2.0, pitch=lambda t: k * (0.42 * math.sin(two_pi * t) + 0.2 * math.sin(two_pi * t * 2.5 + 1.0)),
                                                      roll=lambda t: k * (0.4 * math.sin(two_pi * t * 0.5 + 0.6) + 0.18 * math.sin(two_pi * t * 2)),
                                                      bob=lambda t: k * (0.008 * math.sin(two_pi * t * 1.5 + 0.3) + 0.004 * math.sin(two_pi * t * 3.5)))],
                            "events": [ev(0.0, "engine_drive", "engine", drive_cue)] + [ev(0.0, "exhaust_drive", e) for e in ex]
                            + [ev(0.0, "dust_trail", "dust.left"), ev(0.0, "dust_trail", "dust.right")]}
        m.clips["move_off"] = {"long": 1.2, "loop": False, "after": "hold", "about": "she moves off: the nose lifts as she takes up the drive, and settles",
                               "tracks": [self.hull_keys(1.2, pitch=lambda t: 1.5 * k * math.exp(-3.0 * t) * math.sin(two_pi * 1.1 * t), bob=lambda t: 0.005 * math.exp(-3 * t) * math.sin(two_pi * 2.0 * t))],
                               "events": [ev(0.0, "exhaust_rev", e) for e in ex]}
        rest, dip = 1.2, 1.9 * k

        def braking(t):
            if t < rest:
                return -dip * rigid.smooth(t / 0.4) * (1.0 - 0.12 * rigid.smooth((t - 0.4) / (rest - 0.4)))
            a = t - rest
            return -dip * 0.88 * math.exp(-3.0 * a) * math.cos(two_pi * 0.9 * a)
        m.clips["stop"] = {"long": 2.5, "loop": False, "after": "hold", "rest": rest,
                           "about": "she brakes: start it as the brakes go on. The nose dips and stays down while she slows; she is at rest `rest` seconds in "
                                    "(play it faster or slower to fit), and the nose comes back up, a little over, and settles",
                           "tracks": [self.hull_keys(2.5, pitch=braking, bob=lambda t: -0.014 * rigid.smooth(t / 0.4) if t < rest else -0.014 * math.exp(-3.0 * (t - rest)) * math.cos(two_pi * 0.9 * (t - rest)))],
                           "events": [ev(0.0, "brakes", "dust.left"), ev(0.0, "brakes", "dust.right")]}
        m.clips["hit"] = {"long": 0.9, "loop": False, "after": "hold", "about": "struck, not stopped: she jolts",
                          "tracks": [self.hull_keys(0.9, pitch=lambda t: 1.0 * k * math.exp(-5 * t) * math.sin(two_pi * 2.2 * t), roll=lambda t: 1.5 * k * math.exp(-5 * t) * math.sin(two_pi * 1.9 * t + 0.4))],
                          "events": [ev(0.0, "hit_sparks", None, "shell_impact_near")]}
        # knocked out: struck, she sinks on a burst front tyre, the wheels flop over, smoke, then fire at the engine
        tr = [self.hull_keys(3.0, pitch=lambda t: 1.2 * math.exp(-4.5 * t) * math.sin(two_pi * 2.0 * t) - 1.3 * rigid.smooth(t / 1.1),
                             roll=lambda t: 1.6 * math.exp(-4.5 * t) * math.sin(two_pi * 1.7 * t + 0.3) + 2.2 * rigid.smooth(t / 1.3),
                             bob=lambda t: -0.03 * rigid.smooth(t / 1.0))]
        for p in steers:
            tr.append({"part": p, "turn": [[0.0, 0.0], [0.2, 0.0], [0.75, 15.0], [0.9, 13.0], [1.05, 14.0], [3.0, 14.0]]})
        m.clips["knocked_out"] = {"long": 3.0, "loop": False, "after": "hold",
                                  "about": "knocked out: struck at 0.0, she sinks on a burst tyre, smoke, then fire at the engine; holds its last pose (follow with `burn`)",
                                  "tracks": tr,
                                  "events": [ev(0.0, "hit", None, "shell_impact_near_02"), ev(0.4, "smoke_burst", "engine"), ev(0.9, "bail_out", "driver"),
                                             ev(1.2, "fire_start", "engine", "fire_small_loop"), ev(1.6, "smoke_loop", "engine")]}
        m.clips["burn"] = {"long": 2.0, "loop": True, "about": "burning: fire at the engine and in the body, black smoke (loop after knocked_out)",
                           "tracks": [], "events": [ev(0.0, "fire_loop", "engine", "fire_small_loop")] + [ev(0.0, "fire_loop", f) for f in fires] + [ev(0.0, "smoke_loop", "engine")]}
        if dismount:
            tracks = []
            for part, top, t0 in dismount:
                tracks.append({"part": part, "turn": [[0.0, 0.0], [t0, 0.0], [t0 + 0.22, top * 0.8], [t0 + 0.32, top], [t0 + 0.42, top * 0.9], [t0 + 0.55, top], [1.2, top]]})
            m.clips["dismount"] = {"long": 1.2, "loop": False, "after": "hold", "about": "the way down is thrown open for the men (the game sets them down from the rider sockets); play it backward to close up",
                                   "tracks": tracks, "events": [ev(dismount[0][2], "board_drop", "step")]}
        if mg:
            m.clips["mg_burst"] = {"long": 1.0, "loop": False, "after": "hold", "about": "a burst from her machine gun: ten rounds",
                                   "tracks": [], "events": [ev(0.0, "muzzle_flash", mg, "tank_mg_burst")] + [ev(0.075 * i, "muzzle_flash", mg) for i in range(1, 10)]}
        if m.weather is None:
            self.sooted()
        ctl = m.info.setdefault("file", {}).setdefault("controls", {})
        if steers:
            lim = m.parts[m.part_at[steers[0]]].limits
            ctl["steer"] = {"parts": steers, "what": "the game steers the front wheels inside their lock: + to the right", "limits": lim}
        if "mg_mount" in m.part_at:
            ctl["mg"] = {"part": "mg_mount", "what": "trained by the game inside its limits: + to the right", "degrees_per_second": 40.0}
        ctl["doors"] = [d[0] for d in dismount]
        return self


def chaikin(pts, times=2):
    """A polyline with its corners cut round."""
    for _ in range(times):
        out = [pts[0]]
        for a, b in zip(pts, pts[1:]):
            out += [(a[0] * 0.75 + b[0] * 0.25, a[1] * 0.75 + b[1] * 0.25), (a[0] * 0.25 + b[0] * 0.75, a[1] * 0.25 + b[1] * 0.75)]
        out.append(pts[-1])
        pts = out
    return pts


def canvas_tilt(v, hoop, xs_h, skirt, lift=0.03, sag=0.035, per_bay=6):
    """A canvas cover stretched over hoops at xs_h: `hoop` is one hoop's line across her (y, z) from one side's foot
    over the top to the other; the canvas lies `lift` outside it, sags between the hoops and hangs to `skirt`. Both
    faces are made, so it is seen from inside through the open stern; the forward end is closed."""
    line = chaikin(hoop, 2 if v.fine else 1)
    per_bay = per_bay if v.fine else 2
    n = len(line)
    mid = Vector((0.0, sum(z for _, z in line) / n))
    pts2 = []
    for i, (y, z) in enumerate(line):
        a, b = Vector(line[max(i - 1, 0)]), Vector(line[min(i + 1, n - 1)])
        t = (b - a).normalized()
        nrm = Vector((t.y, -t.x))
        if nrm.dot(Vector((y, z)) - mid) < 0:
            nrm = -nrm
        pts2.append((Vector((y, z)) + nrm * lift, nrm, min(1.0, max(0.25, (z - skirt) / max(0.01, (max(q[1] for q in line) - skirt)) * 1.4))))
    # (the skirt: straight down from each foot)
    pts2 = [(Vector((pts2[0][0].x, skirt)), pts2[0][1], 0.0)] + pts2 + [(Vector((pts2[-1][0].x, skirt)), pts2[-1][1], 0.0)]
    xs = []
    for a, b in zip(xs_h, xs_h[1:]):
        xs += [a + (b - a) * i / per_bay for i in range(per_bay)]
    xs.append(xs_h[-1])
    pitch = abs(xs_h[1] - xs_h[0])
    for face in (1.0, -1.0):       # the outside, then the inside a finger's breadth under it
        bm = bmesh.new()
        rows = []
        for x in xs:
            g = math.sin(math.pi * ((x - xs_h[0]) / pitch % 1.0)) ** 1.5
            rows.append([bm.verts.new((x, p.x - nrm.x * (sag * g * wgt + (0.008 if face < 0 else 0.0)), p.y - nrm.y * (sag * g * wgt + (0.008 if face < 0 else 0.0)))) for p, nrm, wgt in pts2])
        for r0, r1 in zip(rows, rows[1:]):
            for i in range(len(r0) - 1):
                f = (r0[i], r1[i], r1[i + 1], r0[i + 1])
                bm.faces.new(f if face > 0 else f[::-1])
        if face > 0:               # the forward end, closed
            end = rows[0] if xs[0] > xs[-1] else rows[-1]
            c = bm.verts.new((end[0].co.x, 0.0, (skirt + max(q.co.z for q in end)) / 2))
            for i in range(len(end) - 1):
                bm.faces.new((c, end[i + 1], end[i]))
            bm.faces.new((c, end[0], end[-1]))
        v.put(bm, "canvas", 0.0, recalc=False)


# =====================================================================================================================
def truck(fine=True, tag="_", tilt=True):
    """The 3-ton lorry: 6.0 m long, 2.2 m wide, 2.2 m to the cab's roof, 2.75 m over her tilt. Ten men ride on
    her benches; a gun's trail goes on her hook."""
    v = CarrierWorks("Truck", "3-ton lorry", "carrier", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.78), rock=(-5.0, 5.0))
    R, W, RR = 0.46, 0.2, 0.265
    xf, xr, yf, yr = 1.82, -1.72, 0.84, 0.80
    zf0, zf1, yfr = 0.64, 0.78, 0.41                       # the frame's rails: foot, top, how far out

    # ---- under her: axles, springs, wheels (on the root: they stay on the road while the body rocks) -----------
    v.use("vehicle")
    v.tube([(xf, -yf + 0.16, R), (xf, -0.34, R - 0.07), (xf, 0.34, R - 0.07), (xf, yf - 0.16, R)], 0.04, "shade", 8)       # the front axle beam, dropped in the middle
    v.rod((xf - 0.16, -yf + 0.2, R - 0.02), (xf - 0.16, yf - 0.2, R - 0.02), 0.014, "steel", 6)                             # the track rod
    v.rod((xr, -yr + 0.24, R), (xr, yr - 0.24, R), 0.055, "shade", 12)                                                      # the back axle
    v.ball((xr, 0.0, R), 0.17, "shade", 14, 10)                                                                             # its differential
    v.rod((xr + 0.15, 0.0, R + 0.02), (0.95, 0.0, 0.66), 0.03, "steel", 8)                                                  # the propeller shaft
    for s in (LEFT, RIGHT):
        v.leaf_spring(xf, s * yfr, zf0 - 0.02, R + 0.07, 0.52)
        v.leaf_spring(xr, s * yfr, zf0 - 0.02, R + 0.09, 0.66, leaves=7)
    for letter, s in SIDES:
        v.steered_wheel(letter, s, xf, yf, R, W, RR)
        v.use("vehicle")
        v.lorry_wheel(f"wheel_back_{letter}", "vehicle", (xr, s * yr, R), s, R, W, RR, twin=True)
        v.use("vehicle")
        v.socket("dust." + ("left" if s > 0 else "right"), (xr - R - 0.1, s * yr, 0.05), (-1, 0, 0.3), part="vehicle")

    # ---- the frame ------------------------------------------------------------------------------------------
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.slab((-3.05, s * yfr - 0.03, zf0), (2.6, s * yfr + 0.03, zf1), "shade", 0.006)
    for x in (2.5, 1.1, -0.4, -2.45, -3.0):
        v.slab((x - 0.04, -yfr, zf0 + 0.02), (x + 0.04, yfr, zf1 - 0.02), "shade")

    # ---- the bonnet behind its radiator, the swept mudguards ---------------------------------------------------
    def section(hw, top, x, z0=0.84, sh=0.16):
        # (one side of a bonnet's section: an upright side, a round shoulder, a flat top)
        return [(x, hw, z0)] + [(x, hw - sh + sh * math.cos(math.radians(a)), top - sh + sh * math.sin(math.radians(a))) for a in (0.0, 18.0, 36.0, 54.0, 72.0, 90.0)]
    nose_x, scut_x, cab_x, cab_back = 2.58, 1.42, 1.14, 0.1
    hw_c, cabz0, belt, sill, roof = 0.95, 0.84, 1.45, 1.52, 2.12
    v.hull(section(0.40, 1.44, nose_x) + section(0.58, 1.50, scut_x), coat, 0.0, angle=40.0)
    v.hull(section(0.425, 1.47, nose_x + 0.07, z0=0.80) + section(0.425, 1.47, nose_x - 0.01, z0=0.80), coat, 0.0, angle=40.0)      # the radiator's shell
    v.grille((nose_x + 0.072, 0.0, 1.15), (0.58, 0.62), X, Z, slats=12, deep=0.02)
    v.lathe((nose_x + 0.03, 0.0, 1.47), Z, [(0.035, 0.0), (0.035, 0.022), (0.012, 0.03), (0.0, 0.03), (0.0, 0.0)], "steel", 10)   # the radiator cap
    v.rod((nose_x - 0.01, 0.0, 1.441), (scut_x, 0.0, 1.501), 0.011, coat, 6)                                                 # the hinge the bonnet's halves fold on
    for s in (LEFT, RIGHT):
        nrm = Vector((0.18, s * 1.16, 0.0)).normalized()
        along = Vector((nrm.y, -nrm.x, 0.0)) * s
        v.louvres(Vector((2.0, s * 0.49, 1.12)) + nrm * 0.003, (0.28, 0.72), nrm, count=10, down=along)
        if fine:
            v.handle(Vector((2.36, s * 0.435, 0.95)) + nrm * 0.004, Vector((2.26, s * 0.45, 0.95)) + nrm * 0.004, nrm, r=0.007, reach=0.03, paint="steel")
        v.mudguard((xf, R), R + 0.13, 4.0, 170.0, s * 0.60, s * 1.08, tail=[(1.2, 0.545)])
        v.plate([(1.28, s * 0.56, 0.86), (2.38, s * 0.56, 0.86), (2.38, s * 0.62, 0.72), (1.28, s * 0.62, 0.72)][::int(s)], 0.008, "shade")   # the apron inside the guard
        v.headlamp((2.2, s * 0.69, 0.9), X, 0.1, stalk=0.2)
        v.slab((2.58, s * yfr - 0.025, 0.66), (2.75, s * yfr + 0.025, 0.72), "shade")                                       # the bumper's irons
        v.hook((2.81, s * 0.5, 0.67), X, 0.07)
    v.slab((2.74, -1.03, 0.6), (2.8, 1.03, 0.74), coat, 0.014, 2)                                                           # the bumper

    # ---- the cab: steel below the windows, pillars and a domed roof above ------------------------------------
    v.hull(section(0.58, 1.50, scut_x) + [(cab_x, hw_c, cabz0), (cab_x, hw_c, belt - 0.04), (cab_x, hw_c - 0.05, sill - 0.02), (cab_x, 0.78, sill)], coat, 0.012)      # the cowl
    v.slab((cab_back, -hw_c, cabz0), (cab_x, hw_c, belt), coat, 0.02, 2)
    v.slab((cab_x - 0.07, -hw_c + 0.02, belt - 0.01), (cab_x, hw_c - 0.02, sill), coat, 0.008)
    v.hull([(cab_x - 0.05, hw_c - 0.01, 2.0), (cab_x + 0.02, hw_c - 0.02, 2.045), (cab_x - 0.22, hw_c - 0.13, roof), (cab_back + 0.16, hw_c - 0.13, roof),
            (cab_back - 0.01, hw_c - 0.01, 2.05), (cab_back - 0.01, hw_c - 0.01, 2.0)], coat, 0.02)                           # the roof, a peak over the windscreen
    v.slab((cab_back, -hw_c + 0.01, belt - 0.02), (cab_back + 0.04, hw_c - 0.01, 2.02), coat, 0.006)                         # the back wall
    v.slab((cab_back - 0.005, -0.3, 1.62), (cab_back + 0.002, 0.3, 1.86), "glass")
    for s in (LEFT, RIGHT):
        a0, a1 = Vector((cab_x, s * hw_c, belt)), Vector((cab_x - 0.125, s * (hw_c - 0.02), 2.03))
        v.hull([tuple(a0), tuple(a0 - X * 0.07), tuple(a1), tuple(a1 - X * 0.07), tuple(a0 - Y * s * 0.06), tuple(a0 - X * 0.07 - Y * s * 0.06), tuple(a1 - Y * s * 0.06), tuple(a1 - X * 0.07 - Y * s * 0.06)],
               coat, 0.006, both=False)                                                                                    # the windscreen pillar
        yg = s * (hw_c - 0.022)
        v.hull([(cab_back + 0.04, yg, belt), (cab_x - 0.05, yg, belt), (cab_x - 0.165, yg, 2.01), (cab_back + 0.04, yg, 2.01),
                (cab_back + 0.04, yg - s * 0.006, belt), (cab_x - 0.05, yg - s * 0.006, belt), (cab_x - 0.165, yg - s * 0.006, 2.01), (cab_back + 0.04, yg - s * 0.006, 2.01)], "glass", 0.0, both=False)
        v.slab((cab_x - 0.47, s * (hw_c - 0.035), belt), (cab_x - 0.44, s * (hw_c - 0.005), 2.01), coat)                      # the quarter light's bar
        v.slab((cab_back + 0.09, s * hw_c, cabz0 + 0.04), (cab_x - 0.07, s * (hw_c + 0.006), belt - 0.012), coat, 0.002)     # the door
        v.handle((cab_back + 0.16, s * (hw_c + 0.006), 1.3), (cab_back + 0.28, s * (hw_c + 0.006), 1.3), Y * s, r=0.008, reach=0.03, paint="steel")
        if fine:
            for z in (1.0, 1.32):
                v.hinge((cab_x - 0.075, s * (hw_c + 0.01), z - 0.04), (cab_x - 0.075, s * (hw_c + 0.01), z + 0.04), 0.012)
        v.slab((cab_back + 0.02, s * 0.62, 0.5), (1.22, s * 1.07, 0.53), coat, 0.006)                                        # the running board
        for x in (cab_back + 0.2, 1.0):
            v.slab((x - 0.02, s * 0.44, 0.53), (x + 0.02, s * 1.0, 0.56), "shade")
    # the windscreen: two panes and the post between them
    v.hull([(cab_x - 0.03, 0.88, sill), (cab_x - 0.03, -0.88, sill), (cab_x - 0.142, 0.86, 2.01), (cab_x - 0.142, -0.86, 2.01),
            (cab_x - 0.036, 0.88, sill), (cab_x - 0.036, -0.88, sill), (cab_x - 0.148, 0.86, 2.01), (cab_x - 0.148, -0.86, 2.01)], "glass", 0.0, both=False)
    v.hull([(cab_x - 0.015, 0.02, sill), (cab_x - 0.015, -0.02, sill), (cab_x - 0.128, 0.02, 2.02), (cab_x - 0.128, -0.02, 2.02),
            (cab_x - 0.045, 0.02, sill), (cab_x - 0.045, -0.02, sill), (cab_x - 0.158, 0.02, 2.02), (cab_x - 0.158, -0.02, 2.02)], coat, 0.004, both=False)
    if fine:        # the wipers' spindles, the driving mirror on its arm
        v.rod((cab_x - 0.3, hw_c - 0.01, 1.9), (cab_x - 0.22, hw_c + 0.2, 1.84), 0.008, "steel", 6)
        v.lathe((cab_x - 0.22, hw_c + 0.2, 1.84), X, [(0.012, 0.0), (0.012, 0.06), (0.0, 0.065), (0.0, 0.0)], "steel", 12)

    # ---- the body: a planked floor, three boards a side, benches along them, a tailboard that drops ------------
    bx0, bx1, bw, fl, side_h = -3.2, -0.02, 1.1, 1.1, 0.52
    v.slab((bx0, -bw, fl - 0.06), (bx1, bw, fl), "wood", 0.004)
    for x in (-3.05, -2.4, -1.05, -0.5, -0.15):                                                                              # the bearers under it (none over the wheels)
        v.slab((x - 0.04, -0.58, zf1 + 0.06), (x + 0.04, 0.58, fl - 0.06), "shade")
    for s in (LEFT, RIGHT):
        v.slab((bx0 + 0.05, s * yfr - 0.045, zf1), (bx1 - 0.05, s * yfr + 0.045, zf1 + 0.06), "wood")
        v.slab((bx0, s * (bw - 0.07), fl - 0.13), (bx1, s * (bw - 0.01), fl - 0.06), "shade", 0.004)                         # the rail under the floor's edge
        for k in range(3):
            z0 = fl + k * (side_h / 3)
            v.slab((bx0, s * (bw - 0.03), z0 + 0.004), (bx1, s * bw, z0 + side_h / 3 - 0.004), coat, 0.003)
        for x in (bx0 + 0.07, -2.4, -1.6, -0.82, bx1 - 0.09):                                                                # the iron straps
            v.slab((x - 0.03, s * bw, fl - 0.06), (x + 0.03, s * (bw + 0.008), fl + side_h), "shade", 0.002)
            rivets(v, (x, s * (bw + 0.009), fl + 0.06), (x, s * (bw + 0.009), fl + side_h - 0.06), Y * s, 0.17, r=0.008)
        v.slab((bx0 + 0.12, s * (bw - 0.42), fl + 0.42), (bx1 - 0.12, s * (bw - 0.05), fl + 0.455), "wood", 0.004)            # the bench
        for x in (bx0 + 0.3, -2.2, -1.6, -1.0, bx1 - 0.3):
            v.slab((x - 0.02, s * (bw - 0.4), fl), (x + 0.02, s * (bw - 0.36), fl + 0.42), "wood")
        v.slab((xr - 0.66, s * 0.62, 0.3), (xr - 0.63, s * 1.04, fl - 0.08), "rubber")                                        # the mud flap
        v.lathe((bx0 - 0.002, s * 0.95, fl - 0.17), -X, [(0.03, 0.0), (0.03, 0.04), (0.0, 0.045), (0.0, 0.0)], "red", 10)     # the tail lamp
    for k in range(4):                                                                                                       # the headboard, behind the cab
        v.slab((bx1 - 0.03, -bw, fl + k * 0.2 + 0.004), (bx1, bw, fl + (k + 1) * 0.2 - 0.004), coat, 0.003)
    for y in (-bw + 0.05, 0.0, bw - 0.05):
        v.slab((bx1 - 0.045, y - 0.03, fl - 0.04), (bx1 - 0.03, y + 0.03, fl + 0.8), "shade", 0.002)
    v.part("tailboard", "hull", (bx0, 0.0, fl), axis=(0, -1, 0), limits=[0.0, 178.0], kind="turn", note="the tailboard: + drops it (90 lies level, 178 hangs down)")
    for k in range(3):
        z0 = fl + k * (side_h / 3)
        v.slab((bx0 - 0.03, -bw + 0.035, z0 + 0.004), (bx0, bw - 0.035, z0 + side_h / 3 - 0.004), coat, 0.003)
    for y in (-0.75, 0.0, 0.75):
        v.slab((bx0 - 0.038, y - 0.03, fl), (bx0 - 0.03, y + 0.03, fl + side_h), "shade", 0.002)
        rivets(v, (bx0 - 0.039, y, fl + 0.06), (bx0 - 0.039, y, fl + side_h - 0.06), -X, 0.17, r=0.008)
    v.use("hull")
    v.socket("step", (bx0 - 0.3, 0.0, fl - 0.4), -X, part="hull", note="behind the tailboard: where the men come down")

    # ---- the tilt: five hoops and the canvas over them, open at the stern ----------------------------------------
    top_z = fl + side_h
    hoop = [(-(bw - 0.02), top_z - 0.2), (-(bw - 0.02), 2.3), (-(bw - 0.14), 2.52), (-(bw - 0.42), 2.66), (0.0, 2.71),
            (bw - 0.42, 2.66), (bw - 0.14, 2.52), (bw - 0.02, 2.3), (bw - 0.02, top_z - 0.2)]
    xs_h = [bx0 + 0.05 + i * ((bx1 - 0.06) - (bx0 + 0.05)) / 4 for i in range(5)]
    if tilt:
        v.part("tilt", "hull", (-1.6, 0.0, top_z), kind="fixed", note="the canvas tilt on its hoops (the swap `tilt` takes it off)")
        line = chaikin(hoop, 2)
        for x in (xs_h if fine else xs_h[:1]):
            v.tube([(x, y, z) for y, z in line], 0.018, "shade", 6)
        canvas_tilt(v, hoop, xs_h, top_z - 0.2)
        v.roll((bx0 - 0.03, -0.8, 2.6), (bx0 - 0.03, 0.8, 2.6), r=0.06, paint="canvas", straps=3)                              # the stern flap, rolled up
        if fine:
            for s in (LEFT, RIGHT):                                                                                           # the cords that hold the skirt down
                for x in (bx0 + 0.45, -2.0, -1.2, -0.45):
                    v.rod((x, s * (bw + 0.03), top_z - 0.2), (x, s * (bw + 0.012), top_z - 0.36), 0.005, "rope", 4, caps=False)
        v.use("hull")

    # ---- what hangs under her: the tank, the spare wheel, the silencer, the hook ---------------------------------
    v.rod((0.55, -0.74, 0.6), (-0.3, -0.74, 0.6), 0.19, coat, v.n(20, 10))
    for x in (0.4, -0.15):
        v.lathe((x, -0.74, 0.6), X, [(0.04, 0.19), (0.04, 0.198), (0.0, 0.198), (0.0, 0.19)], "shade", v.n(20, 10))
    v.rod((0.3, -0.86, 0.72), (0.3, -0.96, 0.8), 0.03, "steel", 8)
    v.tyre((-2.62, 0.0, 0.5), Z, R, RR, W, lugs=20)                                                                          # the spare, slung under her tail
    v.lathe((-2.62, 0.0, 0.46), Z, [(0.06, 0.0), (0.06, RR * 0.6), (0.03, RR * 1.02), (0.0, RR * 1.02), (0.0, 0.0)], coat, v.n(20, 8))
    v.silencer((0.5, 0.58, 0.55), (-0.3, 0.58, 0.55), 0.075, bands=2, pipe=[(1.5, 0.48, 0.72), (0.95, 0.58, 0.58), (0.5, 0.58, 0.55)], tail=((-0.3, 0.58, 0.55), (-0.6, 0.8, -0.1)))
    v.socket("exhaust.1", (-0.42, 0.74, 0.52), (-0.6, 0.8, -0.1))
    v.slab((-3.09, -0.5, 0.62), (-3.01, 0.5, 0.8), "shade", 0.006)
    v.hook((-3.09, 0.0, 0.7), -X, 0.09)
    v.socket("hook", (-3.17, 0.0, 0.7), -X, part="hull", note="the towing hook: a gun's eye (her socket `tow`) goes on it")

    # ---- where the men are, where she is hurt -------------------------------------------------------------------
    v.socket("driver", (0.55, 0.45, 1.25), X, part="hull", note="the driver, seated at the wheel (left-hand drive)")
    v.socket("rider.cab", (0.55, -0.45, 1.25), X, part="hull", note="the man beside the driver")
    k = 0
    for s in (LEFT, RIGHT):
        for x in (-0.45, -1.05, -1.65, -2.25, -2.85):
            k += 1
            v.socket(f"rider.{k}", (x, s * (bw - 0.24), fl + 0.455), (0, -s, 0), part="hull", note="a man seated on the bench, facing inboard")
    v.socket("load", (-1.6, 0.0, fl), Z, part="hull", note="the middle of the floor: stores stand here")
    v.socket("engine", (2.0, 0.0, 1.48), Z, part="hull", note="the bonnet: smoke and the first fire")
    v.socket("fire.1", (0.6, 0.0, 2.0), Z, part="hull", note="out of the cab")
    v.socket("fire.2", (-1.6, 0.0, fl + 0.3), Z, part="hull", note="in the body")
    v.socket("hit.front", (nose_x + 0.08, 0.0, 1.15), X, part="hull")
    v.socket("hit.left", (0.6, hw_c, 1.2), Y, part="hull")
    v.socket("hit.right", (0.6, -hw_c, 1.2), -Y, part="hull")
    v.socket("hit.rear", (bx0 - 0.03, 0.0, fl + 0.3), -X, part="hull")
    for s in (LEFT, RIGHT):
        v.mark(tag, (0.62, s * (hw_c + 0.007), 1.14), Y * s, 0.15, part="hull")
    v.carrier_clips("lorry_idle_loop", "lorry_drive_loop", dismount=[("tailboard", 176.0, 0.05)], soft=1.25)
    v.m.info.setdefault("file", {})["swaps"] = {
        "how": "tilt: off scales the part `tilt` to nothing (a part scaled to nothing is not drawn), and her benches and riders are seen. The coat is the nation's own paint.",
        "tilt": {"default": "on", "choices": {"on": {"what": "under her canvas tilt", "parts": ["tilt"]}, "off": {"what": "the tilt stowed: an open body", "parts": []}}}}
    return v


# =====================================================================================================================
def _body_lids(v, at, size, up, along, count=1):
    """Flat lids on a plate: a raised panel each, hinges on one edge, a handle on the other."""
    n, u = Vector(up).normalized(), Vector(along).normalized()
    w = n.cross(u)
    for k in range(count):
        c = Vector(at) + w * ((k - (count - 1) / 2) * (size[1] + 0.03))
        v.box((size[0], size[1], 0.012), c + n * 0.006, coat, 0.004, turn=turn_to(u, n))
        if v.fine:
            for f in (-0.3, 0.3):
                v.hinge(c + u * (size[0] * f - 0.035) - w * (size[1] / 2) + n * 0.012, c + u * (size[0] * f + 0.035) - w * (size[1] / 2) + n * 0.012, 0.009)
            v.handle(c - u * 0.05 + w * (size[1] / 2 - 0.05) + n * 0.012, c + u * 0.05 + w * (size[1] / 2 - 0.05) + n * 0.012, n, r=0.006, reach=0.025)


def hanomag(fine=True, tag="GER", paint=None):
    """The armoured half-track (Sd.Kfz. 251): 5.8 m long, 2.1 m wide, 1.76 m to her top edge. A driver and his
    mate under the cab roof, ten men on the benches, a machine gun behind a shield."""
    v = CarrierWorks("Hanomag", "Sd.Kfz. 251 half-track", "carrier", tag, fine, paint=GER_COATS[paint or "grey"][0])
    v.root(hull_pivot=(0.0, 0.0, 0.8), rock=(-4.0, 4.0))
    R, W, RR = 0.44, 0.19, 0.25
    xf, yf = 2.14, 0.84
    zg, zk, zt = 0.94, 1.22, 1.76                          # the track guards, the knuckle, the top edge
    yg, yk, yt = 0.86, 1.04, 0.86                          # how far out the body is at each
    xa, xb = 0.95, -1.75                                   # the body is full width from the cab's front to here
    xk, xr2 = -2.62, -2.36                                 # the stern: its knuckle, its head and foot
    ykr, ytr = 0.66, 0.56                                  # how far out the stern's corners are
    x_roof0, x_roof1 = 0.62, 0.12                          # the cab roof

    # ---- running gear: steered wheels ahead, six interleaved road wheels, the sprocket leading -------------------
    v.use("vehicle")
    v.tube([(xf, -yf + 0.15, R), (xf, -0.3, R - 0.05), (xf, 0.3, R - 0.05), (xf, yf - 0.15, R)], 0.036, "shade", 8)
    v.rod((xf - 0.14, -yf + 0.2, R - 0.02), (xf - 0.14, yf - 0.2, R - 0.02), 0.013, "steel", 6)
    v.leaf_spring(xf + 0.06, 0.0, 0.66, R + 0.04, 0.0 + 0.5, leaves=6, wide=0.07)
    for letter, s in SIDES:
        v.steered_wheel(letter, s, xf, yf, R, W, RR, lock=28.0)
    ty, r_w, thick = 0.86, 0.3, 0.024
    xs = [-1.84 + 0.34 * i for i in range(6)]
    v.running(xs, r_w, ty, (0.64, 0.47, 0.27), (-2.26, 0.36, 0.3), top_on_wheels=True, wheel_z=r_w + thick,
              wheel=dict(w=0.055, tyre=0.03, bolts=6, sides=30, hub=0.34, axle=0.2, holes=6),
              per_axle=lambda i: {"gap": 0.16} if i % 2 == 0 else {"gap": 0.035, "bolts": 0},
              sprocket_kw=dict(teeth=12, wide=0.22, tooth=0.03, holes=6, bolts=8, sides=28, hub=0.42),
              idler_kw=dict(wide=0.2, kind="dish", holes=6),
              track=dict(pitch=0.14, wide=0.28, thick=thick, sag=0.012, style={"kind": "plate", "horn": "centre", "horn_high": 0.06, "grouser": 0.012, "cleats": False, "bar": 0.36}))

    # ---- the chassis tub between the tracks, the floor, the track guards ------------------------------------------
    v.use("hull")
    v.hull([(1.3, 0.5, 0.52), (1.3, 0.5, zg), (xr2, 0.5, zg), (xr2 + 0.08, 0.5, 0.54)], "shade", 0.01)
    v.slab((xr2, -0.52, 0.7), (xa - 0.3, 0.52, 0.74), "shade", 0.0)                                                          # the floor
    for s in (LEFT, RIGHT):
        v.slab((xr2 + 0.02, s * 0.5, zg - 0.018), (xa + 0.02, s * (yk + 0.01), zg), coat, 0.004)
        v.plate([(xr2 + 0.02, s * 0.56, zg), (xr2 + 0.02, s * (yk + 0.01), zg), (xr2 - 0.16, s * (yk + 0.01), zg - 0.2), (xr2 - 0.16, s * 0.56, zg - 0.2)][::-int(s)], 0.012, coat, 0.003)   # the guard's tail
        v.mudguard((xf, R), R + 0.12, 8.0, 100.0, s * 0.60, s * (yk + 0.01), tail=[(1.5, zg + 0.015), (xa + 0.02, zg)])

    # ---- the nose: the engine's armour, narrowing to a point ---------------------------------------------------
    yb0, yb1 = 0.62, 0.34                                  # the bonnet's half-width at the scuttle and at its front
    zb0, zb1 = 1.38, 1.22                                  # and its height there
    nose = [(xa, yg, zg), (xa, yk, zk), (xa, yb0, zb0), (2.56, yb1, zb1), (2.76, 0.46, 0.98), (2.58, 0.4, 0.62), (xa, 0.78, 0.62)]
    v.hull(nose, coat, 0.012)
    top_n = outward([(xa, yb0, zb0), (xa, -yb0, zb0), (2.56, -yb1, zb1)], (1.8, 0, 0.9))
    slope = Vector((2.56 - xa, 0.0, zb1 - zb0)).normalized()
    deck = lambda u, y: Vector((xa, y, zb0)) + slope * u + top_n * 0.002      # noqa: E731
    _body_lids(v, deck(0.62, 0.0), (0.86, 0.36), top_n, slope, count=2)                                                      # the engine's two lids
    v.box((0.3, 0.5, 0.014), deck(1.36, 0.0) + top_n * 0.007, coat, 0.004, turn=turn_to(slope, top_n))                       # the cooling flap ahead of them
    if fine:
        v.handle(deck(1.44, -0.08), deck(1.44, 0.08), top_n, r=0.006, reach=0.025)
    nose_n = outward([(2.56, yb1, zb1), (2.56, -yb1, zb1), (2.76, -0.46, 0.98)], (1.8, 0, 0.9))
    v.grille(Vector((2.66, 0.0, 1.1)) + nose_n * 0.004, (0.5, 0.17), nose_n, Y, slats=4, deep=0.02)                           # the armoured intake across her nose
    for s in (LEFT, RIGHT):
        side_n = outward([(xa, s * yk, zk), (xa, s * yb0, zb0), (2.56, s * yb1, zb1)], (1.8, 0, 0.9))
        v.box((0.5, 0.16, 0.05), Vector((1.6, s * 0.76, 1.2)) + side_n * 0.02, coat, 0.012, turn=turn_to(X, side_n))           # the cowl over the engine's side vent
        v.hook((2.74, s * 0.3, 0.84), X, 0.07)
        v.headlamp((2.3, s * 0.84, 0.86), X, 0.075, stalk=0.16, cover=True)
        if fine:                                                                                                             # the width marker: a ball on a stalk
            v.rod((2.42, s * 1.0, 0.72), (2.42, s * 1.0, 1.14), 0.006, "steel", 5)
            v.ball((2.42, s * 1.0, 1.15), 0.018, "white", 8, 6)
    v.shovel((1.55, 0.95, 0.975), 0.0, 0.95)
    v.axe((1.55, -0.95, 0.975), 0.0, 0.75)

    # ---- the cab's front: the driver's plate with its two visors, a cheek plate each side -------------------------
    kF, bF, tF = (xa, yk, zk), (xa, yb0, zb0), (x_roof0, yt, zt)
    v.hull([(xa, yg, zg), kF, bF, tF, (x_roof0 - 0.12, yk, zk), (x_roof0 - 0.12, yg, zg), (x_roof0 - 0.12, yt, zt)], coat, 0.012)
    front_n = outward([bF, (xa, -yb0, zb0), (x_roof0, -yt, zt)], (0.0, 0, 1.2))
    up_f = Vector((x_roof0 - xa, 0.0, zt - zb0)).normalized()
    for s in (LEFT, RIGHT):
        v.visor(Vector((xa, s * 0.36, zb0)) + up_f * 0.24 + front_n * 0.004, front_n, 0.36, 0.12, 0.045)
        cheek_n = outward([kF if s > 0 else (xa, -yk, zk), (xa, s * yb0, zb0), (x_roof0, s * yt, zt)], (0.0, 0, 1.2))
        mid = (Vector((xa, s * yk, zk)) + Vector((xa, s * yb0, zb0)) + Vector((x_roof0, s * yt, zt)) * 1.4) / 3.4
        v.visor(mid + cheek_n * 0.004, cheek_n, 0.24, 0.1, 0.04)
    wall(v, [(x_roof0 - 0.12, yt, zt), (x_roof0 - 0.12, -yt, zt), (x_roof1, -yt, zt), (x_roof1, yt, zt)], -Z, 0.014)          # the cab roof
    if fine:
        rivets(v, (x_roof0 - 0.04, -yt + 0.05, zt + 0.001), (x_roof0 - 0.04, yt - 0.05, zt + 0.001), Z, 0.14)
        rivets(v, (x_roof1 + 0.04, -yt + 0.05, zt + 0.001), (x_roof1 + 0.04, yt - 0.05, zt + 0.001), Z, 0.14)

    # ---- the body's walls: plate below the knuckle leaning out, plate above it leaning in, open overhead ----------
    x0 = x_roof0 - 0.12
    for s in (LEFT, RIGHT):
        low = [(x0, s * yg, zg), (xb, s * yg, zg), (xb, s * yk, zk), (x0, s * yk, zk)]
        upp = [(x0, s * yk, zk), (xb, s * yk, zk), (xb, s * yt, zt), (x0, s * yt, zt)]
        low_t = [(xb, s * yg, zg), (xr2, s * ytr, zg), (xk, s * ykr, zk), (xb, s * yk, zk)]
        upp_t = [(xb, s * yk, zk), (xk, s * ykr, zk), (xr2, s * ytr, zt), (xb, s * yt, zt)]
        for quad in (low, upp, low_t, upp_t):
            n = outward(quad, (-1.0, 0.0, 1.3))
            wall(v, quad, -n, 0.014)
        n_up = outward(upp, (-1.0, 0.0, 1.3))
        v.mark("GER", tuple(Vector((-0.75, s * (yk + yt) / 2, (zk + zt) / 2)) + n_up * 0.002), n_up, 0.2, part="hull")
        if fine:
            rivets(v, Vector((x0 - 0.06, s * yk, zk + 0.05)) + n_up * 0.002 - Y * s * 0.017, Vector((xb + 0.06, s * yk, zk + 0.05)) + n_up * 0.002 - Y * s * 0.017, n_up, 0.16)
            rivets(v, Vector((x0 - 0.06, s * yt, zt - 0.05)) + n_up * 0.002 + Y * s * 0.017, Vector((xb + 0.06, s * yt, zt - 0.05)) + n_up * 0.002 + Y * s * 0.017, n_up, 0.16)
        # the lockers along her side, under the knuckle
        for a, b in ((x0 - 0.1, -0.42), (-0.5, -1.1), (-1.18, xb - 0.02)):
            v.hull([(a, s * (yg + 0.01), zg), (a, s * (yk + 0.005), zg), (a, s * (yk + 0.005), zk - 0.03), (b, s * (yg + 0.01), zg), (b, s * (yk + 0.005), zg), (b, s * (yk + 0.005), zk - 0.03)], coat, 0.006, both=False)
            if fine:
                v.handle(((a + b) / 2 - 0.05, s * (yk + 0.006), zg + 0.16), ((a + b) / 2 + 0.05, s * (yk + 0.006), zg + 0.16), Y * s, r=0.006, reach=0.022)
                for x in (a + 0.08, b - 0.08):
                    v.hinge((x - 0.03, s * (yk + 0.006), zk - 0.05), (x + 0.03, s * (yk + 0.006), zk - 0.05), 0.008)
        # inside: the bench over the track, its back against the wall
        v.slab((xb - 0.35, s * 0.5, zg), (x_roof1 - 0.25, s * (yg - 0.02), zg + 0.09), "leather", 0.02, 2)
        v.hull([(xb - 0.35, s * (yg - 0.03), zg + 0.1), (x_roof1 - 0.25, s * (yg - 0.03), zg + 0.1), (xb - 0.35, s * (yk - 0.05), zk + 0.02), (x_roof1 - 0.25, s * (yk - 0.05), zk + 0.02),
                (xb - 0.35, s * (yg - 0.09), zg + 0.1), (x_roof1 - 0.25, s * (yg - 0.09), zg + 0.1), (xb - 0.35, s * (yk - 0.11), zk + 0.02), (x_roof1 - 0.25, s * (yk - 0.11), zk + 0.02)], "leather", 0.01, both=False)
        if fine:                                                                                                             # rifles clipped upright behind the bench
            for x in (-0.6, -1.0, -1.4):
                v.rod((x, s * (yk - 0.09), zk + 0.06), (x, s * (yt - 0.03), zt - 0.1), 0.014, "wood", 6)
    # the driver's and his mate's seats, the wheel, the dash
    v.slab((x0 - 0.02, -yg + 0.02, zg), (x0, yg - 0.02, zk + 0.16), "shade")
    for s in (LEFT, RIGHT):
        v.slab((0.14, s * 0.18, 0.74), (0.5, s * 0.6, 1.0), "leather", 0.03, 2)
        v.slab((0.1, s * 0.18, 1.0), (0.18, s * 0.6, 1.36), "leather", 0.03, 2)
    v.rod((0.46, 0.38, 1.2), (0.28, 0.38, 1.32), 0.012, "steel", 6)
    v.tube([(0.28 + 0.1 * math.sin(a) * 0.55, 0.38 + 0.17 * math.cos(a), 1.32 + 0.17 * math.sin(a) * 0.83) for a in [2 * math.pi * i / 14 for i in range(14)]], 0.011, "black", 6, closed=True)

    # ---- the stern: two doors that open outward ------------------------------------------------------------------
    for letter, s in SIDES:
        v.part(f"door_{letter}", "hull", (xr2, s * ytr, (zg + zt) / 2), axis=(0, 0, -s), limits=[0.0, 110.0], kind="turn", note="a stern door: + opens it outward")
        upper = [(xk, s * ykr, zk), (xk, s * 0.008, zk), (xr2, s * 0.008, zt), (xr2, s * ytr, zt)]
        lower = [(xr2, s * ytr, zg), (xr2, s * 0.008, zg), (xk, s * 0.008, zk), (xk, s * ykr, zk)]
        for quad in (upper, lower):
            n = outward(quad, (-1.0, 0.0, 1.3))
            wall(v, quad, -n, 0.014)
        n_u = outward(upper, (-1.0, 0.0, 1.3))
        if fine:
            v.handle(Vector((xk + 0.02, s * 0.1, zk + 0.1)) + n_u * 0.012, Vector((xk + 0.06, s * 0.1, zk + 0.22)) + n_u * 0.012, n_u, r=0.008, reach=0.03, paint="steel")
            for z, x in ((zk + 0.36, xk + 0.2), (zg + 0.08, xr2 - 0.07)):
                v.hinge((x, s * (ytr + 0.04), z - 0.04), (x, s * (ytr + 0.04), z + 0.04), 0.012)
    v.use("hull")
    v.hook((xr2 - 0.02, 0.0, 0.7), -X, 0.085)
    v.socket("hook", (xr2 - 0.1, 0.0, 0.7), -X, part="hull", note="the towing hook: a gun's eye (her socket `tow`) goes on it")
    v.socket("step", (xk - 0.35, 0.0, 0.5), -X, part="hull", note="behind the stern doors: where the men come down")
    for s in (LEFT, RIGHT):
        v.lathe((xr2 - 0.17, s * 0.9, zg - 0.12), -X, [(0.025, 0.0), (0.025, 0.035), (0.0, 0.04), (0.0, 0.0)], "red", 10)
        v.jerrycan((-2.05, s * (yk - 0.03), zg + 0.004), 90.0)
    v.roll((xr2 + 0.1, -0.5, zt + 0.07), (xr2 + 0.1, 0.5, zt + 0.07), r=0.075, paint="canvas", straps=3)                      # her tarpaulin, rolled across the stern

    # ---- the machine gun behind its shield, over the cab roof's after edge ---------------------------------------
    gx, gz = x_roof1 + 0.06, zt
    v.part("mg_mount", "hull", (gx, 0.0, gz), axis=(0, 0, -1), limits=[-32.0, 32.0], kind="turn", note="the forward machine gun on its mount: + trains to the right")
    v.rod((gx, 0.0, gz - 0.02), (gx, 0.0, gz + 0.16), 0.022, "shade", 8)
    plate_out = [(-0.3, 0.0), (0.3, 0.0), (0.3, 0.2), (0.17, 0.33), (0.05, 0.33), (0.05, 0.2), (-0.05, 0.2), (-0.05, 0.33), (-0.17, 0.33), (-0.3, 0.2)]
    sheet(v, plate_out, [], (gx + 0.1, 0.0, gz + 0.03), Y, Vector((-0.17, 0.0, 0.985)), 0.01, coat, 0.003)
    v.mg((gx - 0.22, 0.0, gz + 0.25), X, length=0.86, r=0.017)
    v.slab((gx - 0.36, -0.02, gz + 0.19), (gx - 0.2, 0.02, gz + 0.27), "wood", 0.01)                                           # the butt
    v.slab((gx - 0.1, -0.03, gz + 0.14), (gx + 0.02, 0.03, gz + 0.24), "steel", 0.006)                                         # the cradle
    v.socket("mg", (gx + 0.66, 0.0, gz + 0.25), X, part="mg_mount", note="the machine gun's muzzle")
    v.socket("gunner", (gx - 0.55, 0.0, 0.74), X, part="hull", note="the gunner stands here, behind the shield")
    v.use("hull")
    v.aerial("aerial", "hull", (x_roof1 + 0.2, -(yt - 0.03), zt), 2.0)
    v.use("hull")

    # ---- sockets ---------------------------------------------------------------------------------------------------
    v.socket("driver", (0.32, 0.38, 1.0), X, part="hull", note="the driver, seated at the wheel")
    v.socket("rider.cab", (0.32, -0.38, 1.0), X, part="hull", note="the man beside the driver")
    k = 0
    for s in (LEFT, RIGHT):
        for x in (-0.4, -0.8, -1.2, -1.6, -2.0):
            k += 1
            v.socket(f"rider.{k}", (x, s * 0.68, zg + 0.09), (0, -s, 0), part="hull", note="a man seated on the bench, facing inboard")
    v.socket("exhaust.1", (1.2, 0.92, 0.5), (0.1, 1.0, -0.2), note="behind the left front wheel")
    v.socket("engine", tuple(deck(0.62, 0.0) + top_n * 0.05), top_n, part="hull", note="the engine's lids: smoke and the first fire")
    v.socket("fire.1", (-0.9, 0.0, 1.2), Z, part="hull", note="in the open body")
    v.socket("fire.2", tuple(deck(1.3, 0.0) + top_n * 0.05), top_n, part="hull")
    v.socket("hit.front", (2.7, 0.0, 1.05), X, part="hull")
    v.socket("hit.left", (-0.5, yk, zk), Y, part="hull")
    v.socket("hit.right", (-0.5, -yk, zk), -Y, part="hull")
    v.socket("hit.rear", (xk, 0.0, zk), -X, part="hull")
    v.carrier_clips("halftrack_idle_loop", "halftrack_drive_loop", dismount=[("door_l", 105.0, 0.05), ("door_r", 105.0, 0.12)], mg="mg", soft=0.9)
    v.m.info["bore"], v.m.info["brake"] = 0.008, False
    v.m.info.setdefault("file", {})["swaps"] = {"how": "paint: put the chosen coat's colours in place of the paints of the same names.", "paint": coats(v, paint or "grey")}
    return v


# =====================================================================================================================
def m3_halftrack(fine=True, tag="USA"):
    """The half-track M3: 6.2 m long with her roller, 2.2 m wide, 2.0 m to her windscreen's head. A driver and two
    beside him, ten men on the benches, a heavy machine gun on a pedestal."""
    v = CarrierWorks("M3Halftrack", "Half-track M3", "carrier", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.85), rock=(-4.0, 4.0))
    R, W, RR = 0.48, 0.21, 0.27
    xf, yf = 2.05, 0.82
    zf0, zf1, yfr = 0.7, 0.84, 0.42
    hw, zs, ztop = 0.98, 0.94, 1.72                         # the body's half-width, the foot of its plate, its top edge
    cab_x, bx0 = 1.2, -2.98                                 # the windscreen's foot, the stern

    # ---- running gear: a driven front axle, a four-wheel bogie under a band track -------------------------------
    v.use("vehicle")
    v.rod((xf, -yf + 0.16, R), (xf, yf - 0.16, R), 0.05, "shade", 10)
    v.ball((xf, -0.16, R), 0.15, "shade", 12, 8)                                                                             # the front differential
    v.rod((xf - 0.14, -0.16, R + 0.02), (0.6, -0.1, 0.62), 0.026, "steel", 8)
    v.rod((xf - 0.17, -yf + 0.2, R - 0.03), (xf - 0.17, yf - 0.2, R - 0.03), 0.013, "steel", 6)
    for s in (LEFT, RIGHT):
        v.leaf_spring(xf, s * yfr, zf0 - 0.02, R + 0.08, 0.54, leaves=6)
    for letter, s in SIDES:
        v.steered_wheel(letter, s, xf, yf, R, W, RR, lock=30.0, holes=6)
    ty, r_w, thick = 0.84, 0.155, 0.03
    xs = [-1.62, -1.3, -0.98, -0.66]
    v.running(xs, r_w, ty, (0.0, 0.43, 0.37), (-2.28, 0.41, 0.35), rollers=[(-1.14, 0.74, 0.07)], wheel_z=r_w + thick,
              wheel=dict(w=0.07, tyre=0.03, gap=0.09, bolts=0, sides=24, hub=0.42, axle=0.1),
              sprocket_kw=dict(teeth=18, wide=0.26, tooth=0.022, holes=6, bolts=6, sides=30, hub=0.3),
              idler_kw=dict(wide=0.24, kind="dish", holes=6),
              roller_kw=dict(wide=0.14, tyre=0.012, reach=0.3),
              track=dict(pitch=0.1, wide=0.3, thick=thick, sag=0.014, style={"kind": "block", "chevron": False, "horn": "centre", "horn_high": 0.07, "grouser": 0.008, "gap": False}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):         # the bogie: a frame hung from the chassis, a rocker for each pair of wheels, the spring between them
        yo = s * (ty + 0.115)
        v.hull([(-1.46, yo, 0.62), (-0.82, yo, 0.62), (-1.0, yo, 0.8), (-1.28, yo, 0.8), (-1.46, yo - s * 0.03, 0.62), (-0.82, yo - s * 0.03, 0.62), (-1.0, yo - s * 0.03, 0.8), (-1.28, yo - s * 0.03, 0.8)], coat, 0.006, both=False)
        v.slab((-1.2, s * 0.5, 0.66), (-1.08, s * (ty + 0.1), 0.8), "shade", 0.006)
        v.rod((-1.14, yo, 0.36), (-1.14, yo, 0.64), 0.07, "shade", 12)                                                         # the volute spring's housing
        for ca, pair in ((-1.46, xs[:2]), (-0.82, xs[2:])):
            for xw in pair:
                v.rod((ca, yo, 0.4), (xw, yo, r_w + thick), 0.026, "shade", 8)
            v.rod((ca, yo, 0.4), (-1.14, yo, 0.42), 0.024, "shade", 8)
            if fine:
                for xw in pair:
                    v.lathe((xw, yo - s * 0.01, r_w + thick), Y * s, [(0.035, 0.0), (0.035, 0.04), (0.0, 0.05), (0.0, 0.0)], "upper", 10)

    # ---- the frame, the roller, the bumper ------------------------------------------------------------------------
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.slab((bx0 + 0.1, s * yfr - 0.03, zf0), (2.78, s * yfr + 0.03, zf1), "shade", 0.006)
        v.hull([(2.7, s * 0.5, 0.56), (2.7, s * 0.5, 0.86), (3.02, s * 0.5, 0.62), (3.02, s * 0.5, 0.7), (2.7, s * 0.54, 0.56), (2.7, s * 0.54, 0.86), (3.02, s * 0.54, 0.62), (3.02, s * 0.54, 0.7)], coat, 0.004, both=False)
    v.slab((2.7, -0.95, 0.66), (2.76, 0.95, 0.8), coat, 0.012, 2)
    v.part("roller", "hull", (3.02, 0.0, 0.66), axis=Y, kind="spin", note="the unditching roller: it turns when her nose meets a bank")
    v.lathe((3.02, -0.47, 0.66), Y, [(0.94, 0.0), (0.94, 0.14), (0.92, 0.155), (0.02, 0.155), (0.0, 0.14), (0.0, 0.0)], coat, v.n(24, 10))
    v.use("hull")

    # ---- the bonnet behind armoured shutters, flat mudguards ------------------------------------------------------
    nose_x = 2.62
    v.hull([(nose_x, 0.5, 0.86), (nose_x, 0.5, 1.4), (nose_x, 0.0, 1.44), (cab_x, 0.66, 0.86), (cab_x, 0.66, 1.47), (cab_x, 0.0, 1.52)], coat, 0.012)
    v.slab((nose_x - 0.02, -0.52, 0.84), (nose_x + 0.03, 0.52, 1.43), coat, 0.008)
    if fine:                                                                                                                 # the shutters: four slats, shut
        for k in range(4):
            z = 0.93 + 0.125 * k
            v.box((0.012, 0.92, 0.115), (nose_x + 0.04, 0.0, z), coat, 0.004, turn=Matrix.Rotation(math.radians(-14.0), 3, "Y"))
    else:
        v.slab((nose_x + 0.03, -0.46, 0.88), (nose_x + 0.045, 0.46, 1.4), coat)
    v.rod((nose_x, 0.0, 1.441), (cab_x, 0.0, 1.521), 0.011, coat, 6)
    for s in (LEFT, RIGHT):
        nrm = Vector((0.16, s * 1.42, 0.0)).normalized()
        along = Vector((nrm.y, -nrm.x, 0.0)) * s
        v.louvres(Vector((1.95, s * 0.58, 1.12)) + nrm * 0.003, (0.26, 0.8), nrm, count=11, down=along)
        v.mudguard((xf, R), R + 0.12, 2.0, 150.0, s * 0.62, s * 1.06, tail=[(cab_x + 0.05, 0.7)])
        v.headlamp((2.42, s * 0.78, 1.02), X, 0.085, stalk=0.14)
        if fine:                                                                                                             # the lamp's brush guard
            for dy in (-0.07, 0.0, 0.07):
                v.tube([(2.4, s * 0.78 + dy, 1.06), (2.5, s * 0.78 + dy, 1.1), (2.52, s * 0.78 + dy, 1.2), (2.44, s * 0.78 + dy, 1.28)], 0.005, "shade", 4)

    # ---- the body: flat plate all round, an armoured windscreen, doors with drop flaps -----------------------------
    v.slab((bx0, -hw, zs - 0.03), (cab_x, hw, zs), "shade", 0.0)                                                              # the floor
    for s in (LEFT, RIGHT):
        wall(v, [(cab_x, s * hw, zs - 0.04), (bx0, s * hw, zs - 0.04), (bx0, s * hw, ztop), (cab_x, s * hw, ztop)], (0, -s, 0), 0.012)
        if fine:
            rivets(v, (cab_x - 0.06, s * (hw + 0.001), ztop - 0.05), (bx0 + 0.06, s * (hw + 0.001), ztop - 0.05), Y * s, 0.15, r=0.009)
            rivets(v, (cab_x - 0.06, s * (hw + 0.001), zs + 0.04), (bx0 + 0.06, s * (hw + 0.001), zs + 0.04), Y * s, 0.15, r=0.009)
            for x in (0.05, -0.75, -1.9):
                rivets(v, (x, s * (hw + 0.001), zs + 0.12), (x, s * (hw + 0.001), ztop - 0.12), Y * s, 0.15, r=0.009)
        # the cab door: a panel proud of the plate, its upper flap folded down outside
        v.slab((0.16, s * hw, zs), (cab_x - 0.08, s * (hw + 0.012), ztop - 0.32), coat, 0.003)
        v.slab((0.18, s * (hw + 0.012), ztop - 0.6), (cab_x - 0.1, s * (hw + 0.024), ztop - 0.33), coat, 0.003)
        v.handle((0.26, s * (hw + 0.024), zs + 0.4), (0.38, s * (hw + 0.024), zs + 0.4), Y * s, r=0.008, reach=0.03, paint="steel")
        # the mine rack along her side
        v.slab((-0.2, s * (hw + 0.012), 1.18), (bx0 + 0.25, s * (hw + 0.13), 1.2), coat, 0.003)
        v.slab((-0.2, s * (hw + 0.118), 1.2), (bx0 + 0.25, s * (hw + 0.13), 1.36), coat, 0.003)
        if fine:
            for x in (-0.2, -1.05, -1.9, bx0 + 0.25):
                v.slab((x - 0.012, s * (hw + 0.012), 1.2), (x + 0.012, s * (hw + 0.13), 1.36), coat)
        # inside: the fuel tank behind the cab, the bench, its back
        v.slab((-0.12, s * 0.5, zs), (0.12, s * (hw - 0.02), zs + 0.62), coat, 0.012, 2)
        v.slab((bx0 + 0.1, s * 0.56, zs + 0.36), (-0.16, s * (hw - 0.03), zs + 0.44), "canvas", 0.02, 2)
        v.slab((bx0 + 0.1, s * (hw - 0.09), zs + 0.46), (-0.16, s * (hw - 0.03), zs + 0.76), "canvas", 0.02, 2)
        for x in (bx0 + 0.3, -1.5, -0.4):
            v.slab((x - 0.015, s * 0.58, zs), (x + 0.015, s * 0.62, zs + 0.36), "shade")
        v.slab((0.22, s * 0.2, zs + 0.3), (0.62, s * 0.7, zs + 0.4), "canvas", 0.03, 2)                                        # the cab's seats
        v.slab((0.16, s * 0.2, zs + 0.4), (0.24, s * 0.7, zs + 0.8), "canvas", 0.03, 2)
        v.lathe((bx0 - 0.002, s * 0.86, zs + 0.08), -X, [(0.03, 0.0), (0.03, 0.04), (0.0, 0.045), (0.0, 0.0)], "red", 10)
        v.slab((bx0 - 0.1, s * 0.5, 0.66), (bx0 - 0.02, s * 0.9, 0.78), coat, 0.01)                                            # the bumperette
    # the stern plate, and the door in it
    for y0, y1 in ((-hw, -0.34), (0.34, hw)):
        wall(v, [(bx0, y0, zs - 0.04), (bx0, y1, zs - 0.04), (bx0, y1, ztop), (bx0, y0, ztop)], X, 0.012)
    v.part("door_rear", "hull", (bx0, 0.34, (zs + ztop) / 2), axis=(0, 0, -1), limits=[0.0, 120.0], kind="turn", note="the stern door: + opens it outward")
    wall(v, [(bx0, -0.335, zs - 0.03), (bx0, 0.335, zs - 0.03), (bx0, 0.335, ztop), (bx0, -0.335, ztop)], X, 0.012)
    v.handle((bx0 - 0.001, -0.26, zs + 0.4), (bx0 - 0.001, -0.14, zs + 0.4), -X, r=0.008, reach=0.03, paint="steel")
    if fine:
        for z in (zs + 0.12, ztop - 0.12):
            v.hinge((bx0 - 0.012, 0.34, z - 0.04), (bx0 - 0.012, 0.34, z + 0.04), 0.012)
    v.use("hull")
    # the windscreen: an armoured plate leaning back, a slit before each seat
    ws = [(cab_x, hw, 1.47), (cab_x, -hw, 1.47), (cab_x - 0.12, -hw, 2.0), (cab_x - 0.12, hw, 2.0)]
    ws_n = outward(ws, (0.0, 0.0, 1.5))
    wall(v, ws, -ws_n, 0.012)
    up_w = Vector((-0.12, 0.0, 0.53)).normalized()
    for y in (0.46, -0.46):
        v.visor(Vector((cab_x, y, 1.47)) + up_w * 0.3 + ws_n * 0.004, ws_n, 0.4, 0.11, 0.03, brow=False)
    for s in (LEFT, RIGHT):
        wall(v, [(cab_x - 0.057, s * hw, ztop), (cab_x - 0.12, s * hw, 2.0), (cab_x - 0.62, s * hw, ztop)], (0, -s, 0), 0.012)        # the cheek beside the screen
    v.slab((cab_x - 0.02, -hw + 0.012, zs), (cab_x, hw - 0.012, 1.47), coat)                                                   # the scuttle, the dash
    v.rod((cab_x - 0.06, 0.45, 1.3), (cab_x - 0.34, 0.45, 1.5), 0.013, "steel", 6)
    v.tube([(cab_x - 0.34 + 0.19 * math.sin(a) * 0.58, 0.45 + 0.19 * math.cos(a), 1.5 + 0.19 * math.sin(a) * 0.81) for a in [2 * math.pi * i / 16 for i in range(16)]], 0.012, "black", 6, closed=True)

    # ---- the heavy machine gun on its pedestal ---------------------------------------------------------------------
    px, pz = -0.55, 2.02
    v.rod((px, 0.0, zs), (px, 0.0, pz - 0.14), 0.045, coat, 12)
    v.lathe((px, 0.0, zs), Z, [(0.03, 0.0), (0.03, 0.16), (0.0, 0.18), (0.0, 0.0)], coat, 14)
    v.part("mg_mount", "hull", (px, 0.0, pz - 0.14), axis=(0, 0, -1), kind="turn", note="the machine gun on its pedestal: trains all round, + to the right")
    v.slab((px - 0.05, -0.05, pz - 0.14), (px + 0.05, 0.05, pz - 0.05), "steel", 0.008)
    v.slab((px - 0.28, -0.045, pz - 0.06), (px + 0.2, 0.045, pz + 0.07), "steel", 0.012, 2)                                    # the receiver
    v.mg((px + 0.2, 0.0, pz + 0.01), X, length=1.1, r=0.022)
    for dy in (-0.035, 0.035):
        v.rod((px - 0.28, dy, pz + 0.0), (px - 0.4, dy, pz + 0.0), 0.012, "wood", 6)                                           # the spade grips
    v.slab((px - 0.12, 0.05, pz - 0.14), (px + 0.14, 0.24, pz + 0.04), coat, 0.01)                                             # the ammunition box
    v.socket("mg", (px + 1.32, 0.0, pz + 0.01), X, part="mg_mount", note="the machine gun's muzzle")
    v.use("hull")
    v.socket("gunner", (px - 0.5, 0.0, zs), X, part="hull", note="the gunner stands here")
    v.aerial("aerial", "hull", (0.05, -(hw - 0.08), ztop), 2.2)
    v.use("hull")
    v.roll((bx0 + 0.12, -0.7, ztop + 0.07), (bx0 + 0.12, 0.7, ztop + 0.07), r=0.075, paint="canvas", straps=3)
    v.silencer((0.2, 0.6, 0.6), (-0.5, 0.6, 0.6), 0.07, bands=2, pipe=[(1.3, 0.5, 0.78), (0.7, 0.6, 0.64), (0.2, 0.6, 0.6)], tail=((-0.5, 0.6, 0.6), (-0.5, 0.85, -0.1)))
    v.socket("exhaust.1", (-0.6, 0.74, 0.56), (-0.5, 0.85, -0.1))
    v.hook((bx0 - 0.02, 0.0, 0.74), -X, 0.09)
    v.socket("hook", (bx0 - 0.11, 0.0, 0.74), -X, part="hull", note="the towing hook: a gun's eye (her socket `tow`) goes on it")
    v.socket("step", (bx0 - 0.35, 0.0, 0.5), -X, part="hull", note="behind the stern door: where the men come down")

    # ---- sockets ---------------------------------------------------------------------------------------------------
    v.socket("driver", (0.42, 0.45, zs + 0.4), X, part="hull", note="the driver, seated at the wheel")
    v.socket("rider.cab", (0.42, -0.45, zs + 0.4), X, part="hull", note="the man beside the driver")
    k = 0
    for s in (LEFT, RIGHT):
        for x in (-0.5, -1.0, -1.5, -2.0, -2.5):
            k += 1
            v.socket(f"rider.{k}", (x, s * 0.72, zs + 0.44), (0, -s, 0), part="hull", note="a man seated on the bench, facing inboard")
    v.socket("engine", (1.9, 0.0, 1.5), Z, part="hull", note="the bonnet: smoke and the first fire")
    v.socket("fire.1", (-1.3, 0.0, zs + 0.4), Z, part="hull", note="in the open body")
    v.socket("fire.2", (0.5, 0.0, 1.6), Z, part="hull", note="in the cab")
    v.socket("hit.front", (nose_x + 0.05, 0.0, 1.15), X, part="hull")
    v.socket("hit.left", (-0.9, hw, 1.4), Y, part="hull")
    v.socket("hit.right", (-0.9, -hw, 1.4), -Y, part="hull")
    v.socket("hit.rear", (bx0, 0.0, 1.3), -X, part="hull")
    for s in (LEFT, RIGHT):
        v.mark("USA", (-1.6, s * (hw + 0.001), 1.52), Y * s, 0.14, part="hull")
    v.carrier_clips("halftrack_idle_loop", "halftrack_drive_loop", dismount=[("door_rear", 115.0, 0.05)], mg="mg", soft=1.0)
    v.m.info["bore"], v.m.info["brake"] = 0.0127, False
    return v
