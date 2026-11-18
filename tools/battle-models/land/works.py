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

import bpy
import numpy as np
from mathutils import Quaternion, Vector

import rigid
from fittings import Fittings
from gear import Gear
from motorpool import LEFT, RIGHT, SIDES, X, Y, Z, Shop
from motorpool import frame as frame_of
from rigid import hex_rgb


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
                sprocket_kw=None, idler_kw=None, roller_kw=None, wheel_z=None):
        """Both sides' running gear in one go: road wheels at `xs` (radius r, track centre `ty` out from the middle),
        the sprocket and idler as (x, z, r), return rollers as (x, z, r), and the track round them all. `drive` says
        which end the sprocket is at. With no rollers and `top_on_wheels`, the top run lies on the road wheels.
        `per_axle(i)` gives extra road-wheel settings for axle i (interleaved wheels alternate)."""
        wheel = dict(wheel or {})
        track = dict(track or {})
        z = r if wheel_z is None else wheel_z
        for letter, s in SIDES:
            self.use("vehicle")
            for i, x in enumerate(xs):
                kw = dict(wheel)
                if per_axle:
                    kw.update(per_axle(i))
                self.road_wheel(f"wheel_{letter}{i + 1}", "vehicle", x, s * ty, z, r, s, **kw)
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

    def mark(self, tag, at, facing, size=0.22, part="hull", both=False):
        """A national marking for the game to paint here (and the pictures show): each nation's plain period marking."""
        f = Vector(facing).normalized()
        self.m.marks.append({"tag": tag, "at": [round(at[0], 3), round(-at[1], 3), round(at[2], 3)], "facing": [round(f.x, 3), round(-f.y, 3), round(f.z, 3)],
                             "radius": size, "part": part, "both": both})

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

    def tank_clips(self, recoil=0.35, kick=0.9, gun_low=-8.0, hatches=(), traverse=14.0, idle_cue="tank_idle_loop", drive_cue="tank_loop",
                   gun_cue="tank_gun_01", blow=(-0.9, 2.9, 0.42, 172.0), has_turret=True, mg="coax", exhausts=None, fire_sockets=None):
        """The clips a tank has. Nothing here moves her over the ground or trains her turret: the game does that (the
        root, the `turret` and `gun` parts, the wheels and the track clips). These are what her own body does.

        recoil: metres the barrel runs back; kick: degrees the hull rocks to the shot; gun_low: where the gun droops to
        when she is knocked out; hatches: lids that are thrown open when she is knocked out, the commander's first;
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
                           "events": [ev(0.0, "muzzle_flash_big", "muzzle", gun_cue), ev(0.0, "muzzle_blast_dust", "muzzle"), ev(1.5, "case_clang", "case", "tank_shell_case")]}
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
        hs = [h for h in hatches if h in m.part_at]
        if hs:
            m.clips["open_up"] = {"long": 1.0, "loop": False, "after": "hold", "about": "the commander's hatch is opened (she travels opened up; play it backward to close down for action)",
                                  "tracks": [{"part": hs[0], "turn": lid_keys(hs[0], 0.1)}], "events": [ev(0.1, "hatch_open", "commander")]}
        # knocked out: struck, the gun droops, the crew's hatches are thrown open, smoke, then fire from the engine
        tr = [self.hull_keys(3.0, pitch=lambda t: 1.0 * math.exp(-4.5 * t) * math.sin(two_pi * 2.0 * t) - 0.5 * rigid.smooth(t / 1.2),
                             roll=lambda t: 1.4 * math.exp(-4.5 * t) * math.sin(two_pi * 1.7 * t + 0.3) + 1.2 * rigid.smooth(t / 1.5),
                             bob=lambda t: -0.02 * rigid.smooth(t / 1.0))]
        if "gun" in m.part_at:
            tr.append({"part": "gun", "turn": [[0.0, 0.0], [0.25, 0.0], [0.9, gun_low * 0.8], [1.2, gun_low], [1.35, gun_low * 0.93], [1.5, gun_low]]})
        for k, h in enumerate(hs):
            tr.append({"part": h, "turn": lid_keys(h, 0.35 + 0.22 * k, fast=(k == 0))})
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
                tr.append({"part": h, "turn": lid_keys(h, 0.05 + 0.04 * k, fast=True)})
            m.clips["brew_up"] = {"long": 4.0, "loop": False, "after": "hold",
                                  "about": "her ammunition goes: flame jets from the hatches, then the blast throws the turret off its ring to lie beside her; holds its last pose (follow with `burn`)",
                                  "tracks": tr,
                                  "events": [ev(0.0, "hit", None, "tank_knocked_out"), ev(0.05, "flame_jet", "fire.2")] + [ev(0.1, "flame_jet", f) for f in fires if f != "fire.2"]
                                  + [ev(t_blast, "magazine_blast", "ring", "explosion_debris"), ev(t_blast + 0.05, "fire_loop", "ring", "fire_loop"), ev(t_blast + t_fly, "turret_lands", "ring", "shell_impact_near"),
                                     ev(t_blast + 0.3, "smoke_column", "ring")]}
        if m.weather is None:
            self.sooted()
        m.info.setdefault("file", {})["controls"] = {
            "turret": {"part": "turret", "what": "trained by the game: + to the right (clockwise from above)", "degrees_per_second": traverse},
            "gun": {"part": "gun", "what": "laid by the game inside its limits: + muzzle up"},
            "hatches": hs,
        }
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


DUST = "9A8A6A"          # the dust of the battle stage's road: what she carries unless the picture says other ground
EDGE_WEAR = 0.4          # how far a plate's cut-back edge goes toward bare worn metal (the file's `wear`, 0..100)
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
            streak = _noise(nt, pos, 1.0, 3.0, 0.5, stretch=(26.0, 26.0, 1.6))
            side_on = _ramp(nt, _math(nt, "ABSOLUTE", nsep.outputs[2]), 0.0, 0.55, 1.0, 0.0)
            base = toward(base, tone(streak, 0.36, (0.78, 0.76, 0.72), 0.62, (1.06, 1.06, 1.05)), _math(nt, "MULTIPLY", side_on, 0.42 * wear), "MULTIPLY")
            # recesses keep their dirt: a dark wash in every corner, darker in the seams round hatches and bolts
            base = toward(base, (0.36, 0.32, 0.27), _math(nt, "MULTIPLY", nook, 0.6), "MULTIPLY")
            base = toward(base, (0.22, 0.19, 0.16), _math(nt, "MULTIPLY", crack, 0.75), "MULTIPLY")
            # edges where boots and hands come: paint rubbed thin and pale, then knocked through to dark steel in places
            rub = _math(nt, "MULTIPLY", edge_at, _ramp(nt, mid, 0.25, 0.75, 0.2, 1.0))
            chip = _math(nt, "MULTIPLY", edge_at, _ramp(nt, fine, 0.48, 0.58, 0.0, 1.0), clamp=True)
            base = toward(base, (0.50, 0.50, 0.47), _math(nt, "MULTIPLY", rub, 0.4 * wear, clamp=True))
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
        # (the pictures' look draws edge wear itself; without it, the wear goes into the corner colours the .glb carries)
        lv = np.zeros(len(me.loops), dtype=np.int32)
        me.loops.foreach_get("vertex_index", lv)
        lface = np.repeat(np.array(model.fpaint, dtype=np.int32), total)
        tint = rigid.tint_of(model, np.maximum(model._wear[lv], model._ledge * EDGE_WEAR), model._soot[lv], model._lpaint, lface, model._lblend)
        me.color_attributes["Col"].data.foreach_set("color", np.concatenate([tint, np.ones((len(tint), 1))], axis=1).ravel())
    if weighted:
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
    """rigid.gather, with the wear of every corner on a plate's cut-back edge raised: the mesh as the game reads it.
    (A point on an edge is two points in the file, one for the flat face and one for the edge, so the wear stays a
    thin line and does not spread over the plate.)"""
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
    wear = np.round(np.maximum(model._wear[lv], ledge * EDGE_WEAR) * 100)
    soot, blend = np.round(model._soot[lv] * 100), np.round(model._lblend * 100)
    key = np.concatenate([pos, nor, part[:, None], paint[:, None], wear[:, None], soot[:, None], blend[:, None]], axis=1)
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
            "blend": flat(blend[first], 0), "soot": flat(soot[first], 0), "wear": flat(wear[first], 0), "part": flat(part[first], 0)}


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
