"""Films and sheets of the aircraft, for the reviews.

    python airshow.py sheet <out.png> [name,name,...] [cols] [WxHxSPP]     every machine in flight, one small picture each
    python airshow.py <shot> <out folder> [frames=a,b,c] [fps=12] [size=960x540] [samples=12]

shots:  flypast     a turn round a pair of fighters in flight
        shot_down   three bombers in a vic are attacked from astern; one is set on fire and her crew take to their parachutes
        ways_down   the three ways a machine is lost, one after another: blown apart, a wing shot off, going down burning

The formation stays where it is and the air goes by (the sea and the clouds slide astern, smoke is left behind), as a
camera flying with it would see. The clip is written beside the frames as air-<shot>.mp4 and .gif.
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Quaternion, Vector  # noqa: E402

import kit  # noqa: E402
import marks  # noqa: E402
import planes  # noqa: E402
import rigid  # noqa: E402
import stage  # noqa: E402
from planes import q_of  # noqa: E402
from rigid import smooth  # noqa: E402

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


def plane_look(paint, mat, nt, bsdf, mix):
    """What the pictures add to a paint: the disc of a turning airscrew is nearly clear, glass shines."""
    if paint == "blur":
        bsdf.inputs["Alpha"].default_value = 0.14
        bsdf.inputs["Roughness"].default_value = 0.3
    if paint in ("upper", "upper2", "under", "nose"):
        bsdf.inputs["Roughness"].default_value = 0.5


class Air:
    """The upper air: sky, the sea far below, a few clouds between, and the effects (with the air going by)."""

    def __init__(self, speed=95.0, alt=2600.0, sun_turn=148.0, clouds=0.36, seed=5):
        kit.fresh()
        self.speed, self.alt = speed, alt
        self.sea = stage.Sea(wave=1.6, far=70000.0, cells=420, fine=24.0, haze=(3500.0, 55000.0), speed=speed, sky_too=False, sun_turn=sun_turn, haze_mix=1.0, haze_glow=0.8 * 1.6)
        bpy.data.objects["sea"].location.z = -alt
        stage.sky(sun_elev=44.0, sun_turn=sun_turn, level=0.8, clouds=True, high=True)
        self.fx = stage.Effects(seed=seed, wind=(-speed, 0.0, 0.0))
        self.models = {}
        self.cam = kit.camera("cam", 50.0)
        self.cam.data.clip_end = 120000.0
        self.clouds = stage.Clouds(-alt + 1150.0, cover=clouds) if clouds else None

    def model(self, name):
        if name not in self.models:
            self.models[name] = planes.parachute(True) if name == "Parachute" else planes.BUILDERS[name](True)
        return self.models[name]

    def at(self, t):
        self.sea.at(t)
        if self.clouds:
            self.clouds.at(t, self.speed)
        self.fx.at(t)


class Plane:
    def __init__(self, air, name, at=(0, 0, 0), badges=True):
        self.air, self.name = air, name
        self.m, self.f = air.model(name)
        self.ob, self.arm = rigid.to_blender(self.m, plane_look, crease=50.0)
        self.badges = marks.put(self.m, self.ob, self.arm) if badges else []
        self.at, self.q, self.pose = Vector(at), Quaternion(), {}
        self.vel = Vector()                                  # (against the formation)
        self.v_air = air.speed
        self.clip = self.m.clips
        self.put(self.at)

    def play(self, clip, t):
        return rigid.pose_of(self.m, self.clip[clip], max(t, 0.0)) if t >= 0 and clip in self.clip else {}

    def put(self, at=None, *poses, q=None):
        if at is not None:
            self.at = Vector(at)
        if q is not None:
            self.q = q
        self.pose = rigid.merge(*poses)
        rigid.apply_pose(self.arm, self.pose, model=self.m)
        self.arm.location = self.at
        self.arm.rotation_mode = "QUATERNION"
        self.arm.rotation_quaternion = self.q

    def spot(self, socket):
        p, d = rigid.place(self.m, self.pose, socket)
        return self.at + self.q @ p, self.q @ d

    def where(self, socket):
        return lambda: self.spot(socket)

    def nose(self):
        """Which way she points now (her clip's own tipping included)."""
        q = self.pose.get("plane", (Quaternion(),))[0]
        return self.q @ (q @ X)

    def fly_on(self, dt, dive=True):
        """Carried on along her nose (a machine still flying, or diving in flames)."""
        n = self.nose()
        if dive:
            self.v_air = min(self.v_air + 9.8 * max(-n.z, 0.0) * dt, 170.0)
        self.vel = n * self.v_air - X * self.air.speed
        self.at = self.at + self.vel * dt

    def fall(self, dt, drag=0.22, terminal=75.0):
        """Carried on as a wreck: it loses its way and drops."""
        u = self.vel + X * self.air.speed                    # over the ground
        u.x -= u.x * drag * dt
        u.y -= u.y * drag * dt
        u.z += (-9.8 - u.z * (9.8 / terminal)) * dt
        self.vel = u - X * self.air.speed
        self.at = self.at + self.vel * dt

    def hide(self):
        self.ob.hide_render = True
        for b in self.badges:
            b.hide_render = True


class Chute:
    """An airman and his parachute: he leaves a machine, falls clear, it streams, fills and lets him down."""

    def __init__(self, air, t0, plane, socket, delay=0.7):
        self.air, self.t0, self.plane, self.socket, self.delay = air, t0, plane, socket, delay
        self.m = air.model("Parachute")
        self.ob, self.arm = rigid.to_blender(self.m)
        self.arm.rotation_mode = "QUATERNION"
        self.at, self.vel, self.out = Vector(), Vector(), False
        self.ob.hide_render = True

    def step(self, t, dt):
        a = t - self.t0
        if a < 0:
            return
        if not self.out:
            self.at, _ = self.plane.spot(self.socket)
            self.vel = self.plane.vel.copy() + Vector((0, 0, -3.0))
            self.out = True
            self.ob.hide_render = False
        opened = smooth((a - self.delay - 0.5) / 1.2)
        w = self.vel - self.air.fx.wind                      # through the air
        k = 0.1 + 2.1 * opened
        w = w + (Vector((0, 0, -9.8)) - w * k) * dt
        self.vel = w + self.air.fx.wind
        self.at = self.at + self.vel * dt
        pose = rigid.pose_of(self.m, self.m.clips["open"], max(a - self.delay, 0.0))
        if a - self.delay > 1.8:
            pose = rigid.pose_of(self.m, self.m.clips["drift"], a - self.delay - 1.8)
        rigid.apply_pose(self.arm, pose, model=self.m)
        self.arm.location = self.at
        up = (-w).normalized() if w.length > 1.0 else Z
        up = (up * (1.0 - 0.8 * opened) + Z * (0.8 * opened + 0.001)).normalized()
        self.arm.rotation_quaternion = Z.rotation_difference(up)


# ---- effects sized for aircraft ----------------------------------------------------------------------

def trail(fx, where, start, end=1e9, kind="black", size=1.0):
    g = fx.rng

    def black(t, at, facing):
        c = g.uniform(0.03, 0.09)
        big = g.uniform(0.6, 1.25)
        fx.puff(t, at + Vector((g.uniform(-0.5, 0.5), g.uniform(-0.5, 0.5), g.uniform(-0.5, 0.5))) * size, vel=(g.uniform(-5, 5), g.uniform(-3.5, 3.5), g.uniform(-3.5, 3.5)), life=g.uniform(4.5, 7.5),
                r=(0.5 * size, g.uniform(5.0, 8.5) * size * big), col=(c * 1.5, c * 1.15, c), col2=(c * 1.7, c * 1.7, c * 1.7), alpha=g.uniform(0.5, 0.75), drag=0.45, fade_in=0.05, grow=0.6,
                hold=0.25, wind=1.0, squash=(g.uniform(1.0, 1.6), 1.0, g.uniform(0.8, 1.1)))

    def thin(t, at, facing):
        c = g.uniform(0.45, 0.62)
        fx.puff(t, at, vel=(g.uniform(-2, 2), g.uniform(-1.5, 1.5), g.uniform(-1.5, 1.5)), life=g.uniform(2.6, 4.0), r=(0.4 * size, g.uniform(2.4, 4.0) * size), col=(c, c, c), alpha=0.38,
                drag=0.5, fade_in=0.03, grow=0.6, wind=1.0)

    def flame(t, at, facing):
        # (tongues, not balls: each lick is drawn out along the stream)
        for _ in range(3):
            p = fx.puff(t + g.uniform(0, 0.01), at + Vector((g.uniform(-0.3, 0.3), g.uniform(-0.22, 0.22), g.uniform(-0.18, 0.18))) * size, vel=(g.uniform(-6, 6), g.uniform(-1.2, 1.2), g.uniform(-1.2, 1.2)),
                        life=g.uniform(0.06, 0.2), r=(0.24 * size, g.uniform(0.42, 0.72) * size), col=(0.26, 0.085, 0.012), col2=(0.16, 0.022, 0.003), alpha=0.8, kind="glow", wind=0.86,
                        squash=(3.0, 1.0, 1.0), fade_in=0.008, grow=0.6)
            p["ob"].rotation_euler = (g.uniform(0, 6.28), g.uniform(-0.12, 0.12), g.uniform(-0.12, 0.12))
    make, every = {"black": (black, 0.022), "thin": (thin, 0.04), "flame": (flame, 0.011)}[kind]
    fx.stream(where, every, make, start=start, end=end)


def fireball(fx, t0, where, size=1.0):
    """A machine blows up in the air: a ball of burning petrol, black smoke left hanging, scraps flung out."""
    g = fx.rng

    def make(t, at, facing):
        for i in range(6):
            fx.puff(t + 0.01 * i, at + Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-0.8, 0.8))) * size, vel=Vector((g.uniform(-8, 8), g.uniform(-8, 8), g.uniform(-6, 8))) * size,
                    life=g.uniform(0.2, 0.4), r=(0.8 * size, g.uniform(1.6, 2.6) * size), col=(1.0, 0.62, 0.2), col2=(0.5, 0.14, 0.02), alpha=1.0, kind="glow", drag=3.0, wind=0.9, fade_in=0.01, grow=0.45)
        for i in range(12):
            k = g.uniform(0.6, 1.25)
            fx.puff(t + 0.015 * i, at + Vector((g.uniform(-2, 2), g.uniform(-2, 2), g.uniform(-1.5, 1.5))) * size, vel=Vector((g.uniform(-16, 16), g.uniform(-16, 16), g.uniform(-12, 16))) * size,
                    life=g.uniform(0.4, 0.85), r=(1.2 * size, g.uniform(3.0, 5.2) * size), col=(0.36 * k, 0.1 * k, 0.012 * k), col2=(0.1, 0.012, 0.002), alpha=0.9, kind="glow", drag=3.0, wind=0.9, fade_in=0.01, grow=0.45)
        for i in range(22):
            v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-1, 1))).normalized() * g.uniform(8, 30) * size
            c = g.uniform(0.04, 0.12)
            fx.puff(t + 0.05, at, vel=v, life=g.uniform(5, 9), r=(1.6 * size, g.uniform(4.5, 8.5) * size), col=(0.4, 0.2, 0.08), col2=(c, c, c), alpha=0.7, drag=2.6, fade_in=0.04,
                    grow=0.45, wind=1.0, hold=0.3, cool=5.0)
        for i in range(10):                                  # scraps: small, dark, flung far and falling
            v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-0.6, 1))).normalized() * g.uniform(25, 70) * size
            fx.puff(t, at, vel=v + Vector((40, 0, 0)), life=g.uniform(1.4, 2.6), r=(0.22, 0.12), col=(0.08, 0.07, 0.06), alpha=1.0, drag=0.5, gravity=9.8, wind=0.5, fade_in=0.01)
        fx.light(t, at, 6.0e4 * size * size, 0.25, r=3.0)
    fx.once(t0, where, make)


def guns(fx, plane, t0, t1, sockets=None, every=0.085, aim=None, speed=780.0):
    """A machine's guns: a flicker at each muzzle and tracer going away (toward `aim()` if given, else along the bore)."""
    g = fx.rng
    names = sockets or [s["name"] for s in plane.m.sockets if s["name"].startswith("gun.")]

    def make(t, at, facing):
        way = (aim() - at).normalized() if aim else facing
        fx.puff(t, at + way * 0.4, vel=plane.vel, life=0.045, r=(0.12, 0.3), col=(0.75, 0.4, 0.12), alpha=1.0, kind="glow", wind=0.0, fade_in=0.005)
        if g.random() < 0.34:
            way = (way + Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-1, 1))) * 0.006).normalized()
            p = fx.puff(t, at + way * 8.0, vel=way * speed + plane.vel, life=0.62, r=(0.085, 0.085), col=(0.9, 0.62, 0.3), col2=(0.8, 0.4, 0.15), alpha=1.0, kind="glow", wind=0.0,
                        squash=(60.0, 1.0, 1.0), fade_in=0.005, hold=0.9)
            p["ob"].rotation_euler = way.to_track_quat("X", "Z").to_euler()
    for k, nm in enumerate(names):
        fx.stream(plane.where(nm), every, make, start=t0 + 0.02 * k, end=t1)


def strikes(fx, plane, t0, t1, sockets=("fuel", "engine.1", "tank.port")):
    """Hits sparkling on a machine under fire."""
    g = fx.rng

    def make(t, at, facing):
        fx.puff(t, at + Vector((g.uniform(-1.5, 1.5), g.uniform(-1.0, 1.0), g.uniform(-0.3, 0.6))), vel=plane.vel, life=0.09, r=(0.2, 0.45), col=(0.9, 0.7, 0.4), alpha=1.0, kind="glow", wind=0.0, fade_in=0.005)
    for k, nm in enumerate(sockets):
        fx.stream(plane.where(nm), 0.13, make, start=t0 + 0.04 * k, end=t1)


# ---- the shots ---------------------------------------------------------------------------------------

def flypast(A, opt):
    T = 8.0
    name = opt.get("plane", "Spitfire")
    lead = Plane(A, name, (0, 0, 0))
    mate = Plane(A, name, (-34.0, -24.0, -5.0))
    span = lead.f.half * 2.0

    def step(t):
        lead.put(None, lead.play("fly", t))
        mate.put(None, mate.play("fly", t + 1.3))
        A.at(t)
        ang = math.radians(-128.0 + 300.0 * smooth(t / T) * 0.999 + 20.0 * t / T)
        r = span * (1.7 + 0.25 * math.sin(math.pi * t / T))
        kit.look_at(A.cam, (math.cos(ang) * r - 1.5, math.sin(ang) * r, span * (0.42 - 0.3 * math.sin(math.pi * t / T))), (-1.5, -1.0, 0.0))
    A.cam.data.lens = 40.0
    return T, step


def shot_down(A, opt):
    T = 11.0
    dt = 1.0 / float(opt.get("fps", 12))
    bomber, fighter = opt.get("bomber", "He111"), opt.get("fighter", "Spitfire")
    lead = Plane(A, bomber, (0, 0, 0))
    left = Plane(A, bomber, (-38.0, 34.0, 7.0))              # the one that is lost (to port of the leader, nearest the camera)
    right = Plane(A, bomber, (-38.0, -34.0, -7.0))
    f1 = Plane(A, fighter)
    f2 = Plane(A, fighter)
    t_hit, t_fall, t_bail = 2.5, 2.9, 4.3
    fx = A.fx

    def fighter_path(t, lag):
        """Coming in from astern and above on the port bomber, breaking away over her to port."""
        a = t - lag
        close = 430.0 - 62.0 * a                             # how far astern of her
        brk = smooth((a - 5.6) / 1.6)
        at = left.at0 + Vector((-max(close, -40.0 - 60.0 * (a - 6.0)) if a < 7.6 else 140.0, 4.0 + 46.0 * brk * (a - 5.6), 36.0 * (1.0 - smooth(a / 5.6)) + 9.0 + 70.0 * brk * max(a - 5.6, 0.0) * 0.5))
        q = q_of(roll=-64.0 * brk, pitch=-4.5 * (1.0 - smooth(a / 5.6)) + 16.0 * brk, yaw=24.0 * brk)
        return at, q
    left.at0 = left.at.copy()
    for k, (f, lag) in enumerate(((f1, -4.6), (f2, -3.5))):
        f.lag = lag
        guns(fx, f, 0.5 + 1.1 * k, 2.6 + 1.0 * k)
    strikes(fx, left, 1.3, 2.7)
    # her gunners answer
    for b, (t0, t1) in ((left, (0.4, 2.3)), (lead, (0.9, 3.4)), (right, (1.4, 3.6))):
        tur = [s["name"] for s in b.m.sockets if s["name"].startswith("turret.") and s["at"].x < 1.0]
        guns(fx, b, t0, t1, sockets=tur[:2], every=0.11, aim=lambda: f1.at if fx.t is None or fx.t < 2.8 else f2.at, speed=700.0)
    planes_hit = left
    fireball(fx, t_hit, planes_hit.where("engine.1"), size=0.3)
    trail(fx, planes_hit.where("engine.1"), t_hit, kind="flame", size=1.25)
    trail(fx, planes_hit.where("engine.1"), t_hit, kind="black", size=1.15)
    trail(fx, planes_hit.where("tank.port"), t_fall + 2.5, kind="flame", size=0.9)
    trail(fx, right.where("engine.2"), 3.3, kind="thin", size=0.8)
    crew = [Chute(A, t_bail + 0.5 + 0.8 * k, left, "hatch" if k % 2 == 0 else "hatch.belly") for k in range(4)]
    seen = {"t": None}

    def step(t):
        lead.put(None, lead.play("fly", t))
        right.put(None, right.play("fly", t + 2.1))
        if t < t_fall:
            left.put(None, left.play("fly", t + 0.9), left.play("hit", t - t_hit))
        else:
            if seen["t"] is not None:
                left.fly_on(t - seen["t"])
            left.put(None, left.play("fall_burn", t - t_fall))
        for f in (f1, f2):
            at, q = fighter_path(t, f.lag)
            f.vel = (at - f.at) / dt if seen["t"] is not None else Vector((62, 0, 0))
            f.put(at, f.play("fly", t), q=q)
        for c in crew:
            c.step(t, t - seen["t"] if seen["t"] is not None else 0.0)
        seen["t"] = t
        A.at(t)
        # the camera hangs in the air while the formation comes up and passes close by; it goes down beside the
        # stricken one for a few seconds, then lets her go and stays with her crew under their parachutes
        d = t - seen.get("cam_t", t)
        seen["cam_t"] = t
        seen["cam_x"] = seen.get("cam_x", 215.0 - 88.0 * t) - d * (88.0 * (1.0 - smooth((t - 3.4) / 1.6)) + 95.0 * smooth((t - 7.4) / 1.4))
        eye = Vector((seen["cam_x"], 122.0 + 9.0 * max(t - 4.5, 0.0), 22.0 - 30.0 * max(t - 4.5, 0.0)))
        look = left.at.copy()
        out = [c.at for c in crew if c.out]
        w = 0.0
        if out:
            mid = sum(out, Vector()) / len(out)
            w = smooth((t - 6.4) / 2.0)
            look = look.lerp(mid + Vector((0, 0, 14.0)), w)
        dist = (look - eye).length
        A.cam.data.lens = min(max(dist * (0.5 - 0.27 * w), 40.0), 230.0)
        kit.look_at(A.cam, eye, look + Vector((0, 0, 1.0)))
    return T, step


def ways_down(A, opt):
    """Three cuts, 4 s each: blown apart, a wing shot off, going down burning."""
    dt = 1.0 / float(opt.get("fps", 12))
    cuts = [("Zero", "fall_explode", 0.9), ("Bf109", "fall_spin", 0.9), ("Lancaster", "fall_burn", 0.7)]
    each = 4.0
    T = each * len(cuts)
    fx = A.fx
    ps = []
    for k, (name, clip, t_at) in enumerate(cuts):
        p = Plane(A, name, (0.0, 900.0 * k, 0.0))
        p.t0, p.way, p.cut0 = k * each + t_at, clip, k * each
        p.home = p.at.copy()
        ps.append(p)
        t0 = p.t0
        first = len(fx.streams)
        strikes(fx, p, t0 - 0.7, t0)
        if clip == "fall_explode":
            fireball(fx, t0, p.where("fuel"), size=0.9)
            trail(fx, p.where("fuel"), t0 + 0.1, kind="black", size=0.9)
            trail(fx, p.where("fuel"), t0 + 0.1, kind="flame", size=0.9)
            trail(fx, p.where("tank.starboard"), t0 + 0.15, kind="flame", size=0.6)
            trail(fx, p.where("break.wing.port"), t0 + 0.15, kind="thin")
            trail(fx, p.where("break.tail"), t0 + 0.15, kind="thin")
        elif clip == "fall_spin":
            fireball(fx, t0, p.where("break.wing.port"), size=0.3)
            trail(fx, p.where("stub.port"), t0 + 0.05, kind="flame", size=0.8)
            trail(fx, p.where("stub.port"), t0 + 0.05, kind="black", size=0.8)
            trail(fx, p.where("break.wing.port"), t0 + 0.1, kind="thin")
        else:
            fireball(fx, t0, p.where("engine.1"), size=0.35)
            trail(fx, p.where("engine.1"), t0, kind="flame", size=1.5)
            trail(fx, p.where("engine.1"), t0, kind="black", size=1.5)
            trail(fx, p.where("tank.port"), t0 + 2.0, kind="flame", size=1.0)
        p.streams = fx.streams[first:]
    seen = {"t": None, "cut": 0}

    def step(t):
        k = min(int(t / each + 1e-6), len(cuts) - 1)
        step_dt = t - seen["t"] if seen["t"] is not None else 0.0
        if k != seen["cut"]:
            old = ps[seen["cut"]]
            old.hide()
            for st in old.streams:
                st["end"] = -1.0
            for pf in fx.live:
                pf["life"] = min(pf["life"], 0.0)
            seen["cut"] = k
        for p in ps:
            a = t - p.t0
            if a < 0:
                p.put(p.home, p.play("fly", t), p.play("hit", t - (p.t0 - 0.6)))
            else:
                if p.way == "fall_burn":
                    p.fly_on(step_dt)
                else:
                    p.fall(step_dt, drag=0.3 if p.way == "fall_explode" else 0.16)
                p.put(None, p.play(p.way, a))
        seen["t"] = t
        A.at(t)
        p = ps[k]
        a = t - p.cut0
        span = p.f.half * 2.0
        far = max(span * 2.6, 30.0)
        eye = p.home + Vector((far * 0.55 - 14.0 * a, far * 0.95, -far * 0.03 - 1.0 * a))
        look = p.home.lerp(p.at, 0.82)
        A.cam.data.lens = 42.0
        kit.look_at(A.cam, eye, look)
    return T, step


SHOTS = {"flypast": flypast, "shot_down": shot_down, "ways_down": ways_down}


def sheet(argv):
    out = argv[0]
    names = argv[1].split(",") if len(argv) > 1 and argv[1] != "all" else list(planes.BUILDERS)
    cols = int(argv[2]) if len(argv) > 2 else 3
    w, h, spp = (int(v) for v in (argv[3] if len(argv) > 3 else "640x360x10").split("x"))
    tmp = os.path.splitext(out)[0] + "-parts"
    os.makedirs(tmp, exist_ok=True)
    A = Air(clouds=12)
    stage.render_setup(w, h, spp)
    pics, labels = [], []
    for name in names:
        p = Plane(A, name)
        p.put((0, 0, 0), p.play("fly", 0.6))
        A.at(0.0)
        span = p.f.half * 2.0
        A.cam.data.lens = 50.0
        kit.look_at(A.cam, (span * 0.95, span * 0.92, span * 0.5), (-p.f.long * 0.06, 0, -0.2))
        path = os.path.join(tmp, name + ".png")
        kit.still(path)
        pics.append(path)
        labels.append(f"{p.m.title.replace(' (our own model)', '')} · span {span:.1f} m · {len(p.m.parts)} parts")
        print(name, "done", flush=True)
        for o in [p.ob, p.arm] + p.badges:
            bpy.data.objects.remove(o, do_unlink=True)
    kit.sheet(pics, out, cols=cols, labels=labels)
    print("made", out)


def main():
    if sys.argv[1] == "sheet":
        return sheet(sys.argv[2:])
    shot, out = sys.argv[1], sys.argv[2]
    opt = dict(a.split("=", 1) for a in sys.argv[3:])
    fps = int(opt.get("fps", 12))
    w, h = (int(v) for v in opt.get("size", "960x540").split("x"))
    folder = os.path.join(out, f"air-{shot}")
    os.makedirs(folder, exist_ok=True)
    A = Air()
    stage.render_setup(w, h, int(opt.get("samples", 12)))
    T, step = SHOTS[shot](A, opt)
    only = {int(v) - 1 for v in opt["frames"].split(",")} if "frames" in opt else None
    stage.film(folder, T, fps, step, only=only)
    if only is None:
        kit.encode(folder, os.path.join(out, f"air-{shot}"), fps=fps, gif_width=640)
        print("made", os.path.join(out, f"air-{shot}.mp4"))


main()
