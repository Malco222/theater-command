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


SKIN = [0.6, 0.25]                                           # (roughness and shine of a painted skin: the war's paints were matt)
BACK_LIGHT = [0.5]                                           # (how much of the light off the cloud and haze below reaches her belly)


GLOSS = [0.36, 0.4]                                          # (the one gloss coat, overall sea blue: a modest sheen, never a mirror)


def plane_look(paint, mat, nt, bsdf, mix, bare=False, gloss=False):
    """What the pictures add to a paint: the disc of a turning airscrew is nearly clear, glass shines; a skin left
    unpainted (the file's `finish`) is metal."""
    if paint in ("blur", "blur_tip"):                        # (how clear: as the file says under `clear`)
        bsdf.inputs["Alpha"].default_value = 0.07 if paint == "blur" else 0.035
        bsdf.inputs["Roughness"].default_value = 0.6
        bsdf.inputs["Specular IOR Level"].default_value = 0.1
    if paint in ("upper", "upper2", "under", "nose", "rivet", "rivet_under"):
        bsdf.inputs["Roughness"].default_value = SKIN[0]     # (matt paint: it takes a soft light along its curves and mirrors nothing)
        bsdf.inputs["Specular IOR Level"].default_value = SKIN[1]
        if gloss:
            bsdf.inputs["Roughness"].default_value = GLOSS[0]
            bsdf.inputs["Specular IOR Level"].default_value = GLOSS[1]
        if bare:
            bsdf.inputs["Metallic"].default_value = 0.85
            bsdf.inputs["Roughness"].default_value = 0.4
    if paint == "glass":                                     # (clear, so that her crew are seen: the file says so under `clear`)
        bsdf.inputs["Alpha"].default_value = 0.3
        bsdf.inputs["Roughness"].default_value = 0.04
    if paint in ("lamp_red", "lamp_green", "lamp_white", "lens"):
        bsdf.inputs["Roughness"].default_value = 0.1


class Air:
    """The upper air: sky, the sea far below, a few clouds between, and the effects (with the air going by)."""

    def __init__(self, speed=95.0, alt=2600.0, sun_turn=148.0, clouds=0.36, seed=5):
        kit.fresh()
        self.speed, self.alt = speed, alt
        self.sea = stage.Sea(wave=1.6, far=70000.0, cells=420, fine=24.0, haze=(3500.0, 55000.0), speed=speed, sky_too=False, sun_turn=sun_turn, haze_mix=1.0, haze_glow=0.8 * 1.6, back_light=BACK_LIGHT[0])
        bpy.data.objects["sea"].location.z = -alt
        stage.sky(sun_elev=44.0, sun_turn=sun_turn, level=0.8, clouds=True, high=True)
        self.fx = stage.Effects(seed=seed, wind=(-speed, 0.0, 0.0))
        self.models = {}
        self.far_air = (1440.0, Vector((0.4, 0.47, 0.58)))   # (for a scene to set as `fx.haze`: far smoke half way to the sky at a kilometre)
        self.cam = kit.camera("cam", 50.0)
        self.cam.data.clip_end = 120000.0
        self.clouds = stage.Clouds(-alt + 1150.0, cover=clouds, back_light=BACK_LIGHT[0]) if clouds else None
        self.sounds, self.heard = [], []         # (for the clip's sound track: `soundtrack.py` mixes them)

    def cue(self, name, t0, where, t1=None, kind="shot", gain=0.0, drift=False, rate=1.0):
        """Something heard, for the clip's sound track: the sound pack's `name` from t0 (to t1 for a loop, or for a
        run of bursts), where it is: a point, a Plane, or a function of nothing giving a point. kind: "shot" plays
        once, "loop" goes round from t0 to t1, "burst" is laid end to end from t0 to t1 (guns), "pass" is a fly-past
        recording set so that its loudest moment falls when she is nearest. `drift`: a point left in the air, which
        goes astern. Only names and numbers go into the file; the tools carry no sound."""
        self.sounds.append(dict(name=name, t0=float(t0), t1=None if t1 is None else float(t1), kind=kind, gain=gain, drift=drift, rate=rate, where=where, path=[]))

    def hear(self, t, eye, look):
        """Called once a frame, after the camera is set: where the camera is and where everything heard is."""
        self.heard.append([round(t, 4)] + [round(v, 2) for v in eye] + [round(v, 2) for v in look])
        for s in self.sounds:
            if t < s["t0"] - 0.3 or (s["t1"] is not None and t > s["t1"] + 0.5) or (s["t1"] is None and t > s["t0"] + 14.0):
                continue
            w = s["where"]
            p = w.at if isinstance(w, Plane) else (w() if callable(w) else Vector(w))
            p = Vector(p[0]) if isinstance(p, tuple) and len(p) == 2 else Vector(p)
            if s["drift"]:
                p = p + self.fx.wind * (t - s["t0"])
            s["path"].append([round(t, 4), round(p.x, 2), round(p.y, 2), round(p.z, 2)])

    def sound_file(self, path, seconds, fps):
        import json
        out = dict(seconds=seconds, fps=fps, speed=self.speed, camera=self.heard,
                   sounds=[{k: v for k, v in s.items() if k != "where"} for s in self.sounds if s["path"]])
        with open(path, "w") as f:
            json.dump(out, f)
        return path

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
    def __init__(self, air, name, at=(0, 0, 0), badges=True, record=None, scheme=None):
        self.air, self.name = air, name
        self.m, self.f = air.model(name)
        gloss = False
        if scheme:                                           # (as she is seen in a month, YYYY-MM: her coat of paint and her markings then)
            coats = [c for c in self.m.info["file"].get("schemes", []) if c["from"] <= scheme]
            if coats:
                self.m.paints.update(coats[-1]["colours"])
                gloss = coats[-1].get("finish") == "gloss"
            for mk in self.m.marks or []:
                mk["tag"] = planes.mark_in(mk["tag"], scheme, mk.get("place"))
        bare = self.m.info["file"].get("finish") == "bare metal"
        self.ob, self.arm = rigid.to_blender(self.m, (lambda *a: plane_look(*a, bare=bare, gloss=gloss)), crease=50.0)
        self.badges = marks.put(self.m, self.ob, self.arm, record) if badges else []      # (record: what she has done, painted on her)
        self.at, self.q, self.pose = Vector(at), Quaternion(), {}
        self.vel = Vector()                                  # (against the formation)
        self.v_air = air.speed
        self.clip = self.m.clips
        self.load = {}
        self.carry("bombs" if self.f.role == "bomber" else "torpedo")      # (a land bomber is seen with her bombs, a torpedo bomber with her torpedo)
        self.put(self.at)

    def carry(self, load):
        """Which of her loads she is seen with, where she has two (a torpedo, or bombs for bombing from high up)."""
        self.load = self.play("load." + load, 0.0)

    def play(self, clip, t):
        return rigid.pose_of(self.m, self.clip[clip], max(t, 0.0)) if t >= 0 and clip in self.clip else {}

    def put(self, at=None, *poses, q=None):
        if at is not None:
            self.at = Vector(at)
        if q is not None:
            self.q = q
        self.pose = rigid.merge(self.load, *poses)
        rigid.apply_pose(self.arm, self.pose, model=self.m)
        self.arm.location = self.at
        self.arm.rotation_mode = "QUATERNION"
        self.arm.rotation_quaternion = self.q

    def spot(self, socket):
        p, d = rigid.place(self.m, self.pose, socket)
        return self.at + self.q @ p, self.q @ d

    def where(self, socket):
        w = lambda: self.spot(socket)
        w.plane = self                                       # (so that what burns there knows which way she is going)
        return w

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
    """What a stricken machine leaves behind her from one place on her: black smoke, a thin grey streamer, or flame.
    Flame is drawn as tongues: each a ragged streak rooted where she burns and lying along the air going past her,
    alive for a frame or two, with a hot yellow heart at the root and scraps torn off it downstream that go dark into
    the smoke. (Soft balls of flame run together into a smooth orange sheet; tongues do not.)"""
    g = fx.rng
    plane = getattr(where, "plane", None)
    step = 0.017 * min(max(size, 0.45), 1.0)                 # (a small black trail is laid closer: at the big one's spacing a fighter's stood as a string of beads)

    def going():
        """Her way against the formation, and the way the air streams past her."""
        v = plane.vel.copy() if plane is not None else Vector()
        u = v + X * (plane.air.speed if plane is not None else -fx.wind.x)
        return v, (-u.normalized() if u.length > 1.0 else -X)

    def across(flow, far, dice=g):
        """A step to one side of the stream."""
        side = flow.cross(Z)
        side = side.normalized() if side.length > 1e-6 else Y.copy()
        up = side.cross(flow)
        a = dice.uniform(0, 6.283)
        return (side * math.cos(a) + up * math.sin(a)) * far

    def black(t, at, facing):
        v, flow = going()
        # (it gathers a few metres behind the fire, so the flame is seen against the sky and not through its own smoke)
        # (each puff is drawn out along the stream, so that near her they run together into one rope and never stand as a row of balls)
        # (the art director, 2026-11-18: seen from near by, seconds later, the trail "breaks into a chain of separate
        #  ovals". Puffs that died at odd times and drifted apart left the rest standing alone. So: all of an age live
        #  as long as each other, they drift less, they are rounder, and the column thins as one.)
        # (the art director, 2026-11-18 15:28Z: "the trails are ropes", as wide and as dark at the old end as at the
        #  wreck. Now: born narrow and dense; it fades from birth and greys; more than a third of it is short lived, so
        #  the old end has half the smoke of the young; it comes in thicker and thinner gusts and wanders a little, and
        #  one puff in eight strays off to the side, so its edge breaks into puffs and gaps.)
        # (a machine falling fast lays her puffs farther apart: they are drawn out the more along her path, or the
        #  old end of a fighter's trail is a string of beads)
        u = v + X * (plane.air.speed if plane is not None else -fx.wind.x)       # (her way through the air)
        more = max(1, min(4, int(round(u.length / 95.0))))
        fast = max(1.0, u.length / (95.0 * more)) ** 1.3
        for j in range(1, more):                             # (the ones between: where she was a part of a step ago)
            black_one(t, at - u * (step * j / more), v, flow, fast)
        black_one(t, at, v, flow, fast)

    def black_one(t, at, v, flow, fast):
        c = g.uniform(0.03, 0.09)
        young, stray = g.random() < 0.38, g.random() < 0.12
        big = g.uniform(0.75, 1.2) * (0.78 + 0.5 * abs(math.sin(3.1 * t + 0.7) * math.sin(1.3 * t)))
        wander = across(flow, 1.0) * 0.0 + Vector((0.0, math.sin(1.9 * t + 1.0), math.cos(2.7 * t))) * (0.55 * size)
        life = g.uniform(1.4, 3.2) if young else g.uniform(8.6, 9.4)
        end = g.uniform(6.5, 9.5) * size * big * (0.42 if young else 1.0) * (0.55 if stray else 1.0)
        drift = 6.5 if stray else 2.5
        p = fx.puff(t, at + flow * 3.2 * size + wander + Vector((g.uniform(-0.5, 0.5), g.uniform(-0.5, 0.5), g.uniform(-0.5, 0.5))) * size, vel=(g.uniform(-2.5, 2.5), g.uniform(-drift, drift), g.uniform(-drift, drift)),
                    life=life, r=(max(0.4, 0.45 * size), end), col=(c * 1.9, c * 1.2, c), col2=(c * 2.4 + 0.035, c * 2.4 + 0.035, c * 2.4 + 0.04), alpha=g.uniform(0.72, 0.9), drag=0.45,
                    fade_in=0.06, grow=0.5, hold=0.0, cool=2.2, wind=1.0, squash=(g.uniform(1.5, 2.0) * fast, 1.0, g.uniform(0.85, 1.1)))
        p["ob"].rotation_euler = ((flow + across(flow, g.uniform(0.0, 0.25))).normalized().to_track_quat("X", "Z") @ Quaternion(X, g.uniform(0, 6.283))).to_euler()

    def thin(t, at, facing):
        c = g.uniform(0.45, 0.62) if size > 0.5 else g.uniform(0.2, 0.3)       # (a jet's thread is darker than a leak of glycol)
        fx.puff(t, at, vel=(g.uniform(-2, 2), g.uniform(-1.5, 1.5), g.uniform(-1.5, 1.5)), life=g.uniform(2.6, 4.0), r=(0.4 * size, g.uniform(2.4, 4.0) * size), col=(c, c, c), alpha=0.38,
                drag=0.5, fade_in=0.03, grow=0.6, wind=1.0)

    def tongue(t, root, flow, v, long, kind, col, col2, life, drift, alpha=0.92, splay=0.16, fat=(0.2, 0.32), dice=g, name="puff"):
        d = (flow + across(flow, dice.uniform(0.0, splay), dice)).normalized()
        wide = long * dice.uniform(*fat)
        # (its blunt root end is set back into the place that burns: showing, a row of them read as round heads with tails)
        p = fx.puff(t, root + d * (0.36 * long), vel=v, life=life, r=(0.72, 1.0), col=col, col2=col2, alpha=alpha, kind=kind, wind=drift, squash=(0.5 * long, 0.5 * wide, 0.5 * wide),
                    fade_in=0.006, grow=0.5, hold=0.55, cool=1.0, name=name, rng=None if dice is g else dice)
        p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion(X, dice.uniform(0, 6.283))).to_euler()

    q = random.Random(1000 + 17 * len(fx.streams))           # (the sheet's and the soot's own dice)

    def out_of_skin(at):
        """Away from her skin at the place that burns: straight out from her middle line; up, if the place is on that
        line or out on a wing."""
        if plane is None:
            return Z.copy()
        nose, up, rel = plane.q @ X, plane.q @ Z, at - plane.at
        side = rel - nose * rel.dot(nose)
        if side.length < 0.25 or abs(side.dot(plane.q @ Y)) > 1.6:
            return up
        return side.normalized()

    def flame(t, at, facing):
        # (its colours are thrown dim, as the sea's fires are: under the picture's tone curve a bright flame bleaches to
        #  peach whatever its hue. The art director measured this fire at about 227,169,138 where deep orange is about
        #  210,115,55; see the README, "Flame's colour is measured".)
        v, flow = going()
        gust = 0.72 + 0.5 * abs(math.sin(23.0 * t) * math.sin(7.3 * t + 1.0))        # (it flares and sinks)
        k = g.uniform(0.85, 1.15)
        # (The art director, 2026-11-18 15:28Z: the fire "lies flat on the skin, like paint ... tongues must leave the
        #  skin at the source and peel back into the air behind", the root "a narrow bright slit ... widening as it goes
        #  back". Broad tongues laid along her skin stood half inside her, and what showed was a patch on her panels.
        #  So: every tongue starts a hand off the skin and leans out from it, more the longer it is; tongues are
        #  narrower, so the fire is narrow at its root and only as wide as its spread downstream.)
        out = out_of_skin(at)
        at = at + out * 0.14 * size
        lean = lambda far: (flow + out * far).normalized()
        # rooted on her, round the place that burns: thin tongues, the streaks in the fire
        tongue(t, at + flow * g.uniform(0.3, 1.2) * size + across(flow, g.uniform(0.05, 0.2) * size), lean(g.uniform(0.08, 0.3)), v, g.uniform(1.7, 3.9) * size * gust, "tongue",
               (0.14 * k, 0.026 * k, 0.004), (0.1, 0.014, 0.002), g.uniform(0.07, 0.15), g.uniform(0.02, 0.08), fat=(0.14, 0.24))
        # the body of the fire: broad tongues from the same root lying one on another along the stream, so that they are
        # one torn sheet of flame streaming back from the place that burns, and not strokes side by side. (The art
        # director, 2026-11-18: "join the strokes into one torn sheet ... into its smoke trail".)
        if q.random() < 0.62:
            kk = q.uniform(0.85, 1.12)
            tongue(t, at + flow * q.uniform(0.0, 0.5) * size + across(flow, q.uniform(0.0, 0.1) * size, q), lean(q.uniform(0.1, 0.24)), v, q.uniform(3.3, 5.6) * size * gust, "lick" if q.random() < 0.7 else "tongue",
                   (0.14 * kk, 0.026 * kk, 0.004), (0.1, 0.014, 0.002), q.uniform(0.09, 0.16), q.uniform(0.02, 0.06), splay=0.1, fat=(0.18, 0.3), dice=q, name="sheet")
        # torn off its end, in line with it, and going red
        # (few, close and short lived: many of them, flung wide and far down the smoke, read as orange petals blowing about)
        if g.random() < 0.4:
            tongue(t, at + (flow + out * 0.2) * g.uniform(2.0, 4.6) * size * gust + across(flow, g.uniform(0.0, 0.32) * size), flow, v, g.uniform(1.0, 2.2) * size, "lick",
                   (0.12 * k, 0.021 * k, 0.0035), (0.07, 0.01, 0.002), g.uniform(0.07, 0.14), g.uniform(0.15, 0.3), alpha=0.85, splay=0.12, fat=(0.3, 0.46))
        # a scrap of it going out in the head of the smoke
        if g.random() < 0.09:
            tongue(t, at + (flow + out * 0.2) * g.uniform(4.2, 6.2) * size + across(flow, g.uniform(0.0, 0.4) * size), flow, v, g.uniform(0.6, 1.1) * size, "lick",
                   (0.07, 0.013, 0.003), (0.03, 0.011, 0.006), g.uniform(0.1, 0.18), g.uniform(0.3, 0.5), alpha=0.5, splay=0.16)
        # the heart of it, where the petrol or the oil comes out
        if g.random() < 0.4:
            # (a short solid yellow tongue lying along the stream, inside the fire; as a round glowing ball it sat on the
            #  engine like a bead, and thin it lay over a pale skin as a peach film)
            # (and thin: the bright slit at the root)
            p = fx.puff(t, at + flow * g.uniform(0.1, 0.5) * size + across(flow, g.uniform(0.0, 0.1) * size), vel=v, life=g.uniform(0.05, 0.1), r=(0.2 * size, g.uniform(0.34, 0.5) * size * gust),
                        col=(0.193, 0.111, 0.0), col2=(0.17, 0.07, 0.001), alpha=0.95, kind="heart", wind=0.03, squash=(3.6, 0.6, 0.6), fade_in=0.006, grow=0.6, cool=1.0)
            p["ob"].rotation_euler = lean(0.12).to_track_quat("X", "Z").to_euler()
        # its smoke is born black-brown in the end of the flame, drawn out along the stream: the fire goes into its
        # trail with no gap of clear air between them
        c = q.uniform(0.014, 0.04)
        p = fx.puff(t, at + (flow + out * 0.2) * q.uniform(1.7, 4.4) * size * gust + across(flow, q.uniform(0.0, 0.35) * size, q), life=q.uniform(0.45, 0.9), r=(0.55 * size, q.uniform(1.5, 2.4) * size),
                    col=(c * 1.4, c, c * 0.75), col2=(c * 1.7, c * 1.6, c * 1.5), alpha=q.uniform(0.75, 0.92), fade_in=0.012, grow=0.6, hold=0.15, wind=1.0, squash=(2.4, 1.0, 1.0), name="soot", rng=q)
        p["ob"].rotation_euler = (flow.to_track_quat("X", "Z") @ Quaternion(X, q.uniform(0, 6.283))).to_euler()
    make, every = {"black": (black, step), "thin": (thin, 0.04), "flame": (flame, 0.0115)}[kind]
    fx.stream(where, every, make, start=start, end=end)
    if kind == "flame" and callable(where):
        # the warm light it throws on the panels round it: just off her skin, a little downstream of the root
        fx.firelight(where, 260.0 * size * size, start, end, off=lambda at, facing: at + out_of_skin(at) * 0.7 * size + going()[1] * 0.9 * size)


def left_behind(fx, b, share=0.8, k=2.4, way=None):
    """Where what a burst leaves in the air has got to, b seconds on, as the formation sees it, and how it is going
    then: born with `share` of her way through the air (the formation's own, unless `way` is given) and stopped by
    the air, it hangs for a moment and then goes astern."""
    way = -fx.wind if way is None else way
    e = math.exp(-k * b)
    return way * (share * (1.0 - e) / k) + fx.wind * b, way * (share * e)


def fireball(fx, t0, where, size=1.0):
    """A machine blows up in the air. She is flying fast, so none of it is a round ball: a lumpy torn core of fire lying
    back from the burst along her path, a few long tongues thrown back out of it, a solid yellow heart inside it, and
    black-brown smoke born at once inside the fire in many small puffs, with orange glowing through it for a few
    frames. The smoke is left hanging where she blew up as a dark cloud, growing and thinning, while her pieces fall
    away; scraps of her are dark specks, and some trail a thread of smoke. (The art director, 2026-11-18: an even ring
    of tongues of one tone, with no smoke and gone in three frames, was "a flat orange star of spikes ... a paint
    splat, the cartoon look".)"""
    g = fx.rng
    plane = getattr(where, "plane", None)
    going = {"way": -fx.wind}

    def any_way(dice, flat=1.0):
        d = Vector((dice.uniform(-1, 1), dice.uniform(-1, 1), dice.uniform(-1, 1) * flat))
        return d.normalized() if d.length > 0.05 else Z.copy()

    def lay(t, at, d, long, wide, kind, col, col2, life, dice, share=0.8, fling=(3.0, 10.0), alpha=0.92, name="puff"):
        off, v = left_behind(fx, max(t - t0, 0.0), share, way=going["way"])
        p = fx.puff(t, at + off + d * long * 0.3, vel=v + d * dice.uniform(*fling) * size, life=life, r=(0.7, 1.0), col=col, col2=col2, alpha=alpha, kind=kind, drag=2.4, wind=1.0,
                    squash=(0.5 * long, 0.5 * wide, 0.5 * wide), fade_in=0.008, grow=0.5, hold=0.5, cool=1.0, name=name, rng=None if dice is g else dice)
        p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion(X, dice.uniform(0, 6.283))).to_euler()

    def make(t, at, facing):
        q = random.Random(int(at.x * 53.0 + at.y * 7.0 + at.z * 3.0) + 577)
        # (her own way through the air at that moment: a machine met head-on leaves her fire and smoke the other way)
        way = (plane.vel + X * plane.air.speed) if plane is not None else -fx.wind
        way = way if way.length > 1.0 else -fx.wind
        going["way"] = way
        aft = -way.normalized()
        # the core: fat torn lumps of flame lying back from the burst, alive for two or three frames. (Deep orange going
        # dark at their ends: lumps with yellow roots made the whole core one pale yellow mass.)
        # (The art director, 2026-11-18 15:28Z, of the first frames: "still a starburst: thin spines ... the flat cards
        #  read as crumpled paper or leaves". So the lumps are nearly as wide as long, and the long tongues are three
        #  to five, lie astern, and none is more than three times its width.)
        for i in range(int(9 + 13 * size)):
            d = (any_way(g) * 0.6 + aft * 0.9).normalized()
            long = g.uniform(3.2, 5.2) * size
            k = g.uniform(0.8, 1.15)
            root = at + aft * g.uniform(-0.6, 3.6) * size + any_way(g) * g.uniform(0.0, 1.0) * size
            lay(t, root, d, long, long * g.uniform(0.62, 0.88), "lick" if i % 4 else "tongue", (0.14 * k, 0.026 * k, 0.004), (0.1, 0.014, 0.002), g.uniform(0.24, 0.34), g, share=g.uniform(0.74, 0.86))
        # a few long tongues thrown out of it, back and to the side, each with its yellow root in the core
        for i in range(int(3 + 2 * size)):
            d = (any_way(g) * 0.4 + aft * 1.0).normalized()
            long = g.uniform(5.0, 7.5) * size
            k = g.uniform(0.8, 1.15)
            root = at + aft * g.uniform(0.0, 3.0) * size + any_way(g) * g.uniform(0.0, 0.5) * size
            lay(t, root, d, long, long * g.uniform(0.34, 0.42), "tongue", (0.14 * k, 0.026 * k, 0.004), (0.1, 0.014, 0.002), g.uniform(0.2, 0.3), g, share=g.uniform(0.62, 0.78), fling=(6.0, 16.0))
        # its yellow heart: short and solid, inside the core (never a round glowing ball, and never a thin film)
        for i in range(int(5 + 5 * size)):
            d = (any_way(q) * 0.7 + aft * 0.6).normalized()
            long = q.uniform(1.5, 2.7) * size
            root = at + aft * q.uniform(0.3, 2.4) * size + any_way(q) * q.uniform(0.0, 0.7) * size
            lay(t, root, d, long, long * q.uniform(0.45, 0.7), "heart", (0.193, 0.111, 0.0), (0.17, 0.07, 0.001), q.uniform(0.1, 0.19), q, fling=(2.0, 6.0), alpha=0.95, name="heart")
        # black-brown smoke, born at once inside the fire in many small puffs, no two of a darkness, most of them at
        # the burst and fewer strung back along her path, each drawn out a little along it (round, the outlying ones
        # stood as a row of balls)
        def smoke(b, back, out, fling, life, r, c, alpha, grow, fade_in, hold, cool):
            off, v = left_behind(fx, b, q.uniform(0.7, 0.86), way=way)
            warm = q.random() < 0.4
            p = fx.puff(t + b, at + off + aft * back * size + any_way(q) * out * size, vel=v + any_way(q) * fling * size, life=life, r=(r[0] * size, r[1] * size),
                        col=(c * (1.4 if warm else 1.15), c, c * (0.72 if warm else 0.88)), col2=(c * 1.6, c * 1.55, c * 1.5), alpha=alpha, drag=2.4, fade_in=fade_in, grow=grow, wind=1.0,
                        hold=hold, cool=cool, squash=(q.uniform(1.25, 1.7), 1.0, q.uniform(0.85, 1.1)), name="soot", rng=q)
            p["ob"].rotation_euler = ((aft + any_way(q) * 0.45).normalized().to_track_quat("X", "Z") @ Quaternion(X, q.uniform(0, 6.283))).to_euler()
        for i in range(int(30 + 40 * size)):
            # (a third of it with the burst itself: the first frame has its smoke)
            smoke(0.0 if i % 3 == 0 else q.uniform(0.0, 0.11), -0.5 + 8.5 * q.random() ** 1.7, q.uniform(0.2, 1.7), q.uniform(3, 20), q.uniform(3.0, 5.0), (q.uniform(0.45, 0.85), q.uniform(2.2, 4.0)),
                  q.uniform(0.014, 0.055), q.uniform(0.7, 0.92), 0.5, 0.025, 0.14, 2.0)
        # fire still glowing through it, in patches on its outside, for a few frames
        for i in range(int(10 + 14 * size)):
            d = (any_way(q) * 0.8 + aft * 0.5).normalized()
            long = q.uniform(1.4, 3.0) * size
            root = at + aft * q.uniform(0.0, 6.0) * size + any_way(q) * q.uniform(0.7, 2.2) * size
            lay(t + q.uniform(0.1, 0.42), root, d, long, long * q.uniform(0.4, 0.6), "lick", (0.12, 0.021, 0.0035), (0.05, 0.009, 0.002), q.uniform(0.15, 0.3), q, fling=(1.0, 5.0), alpha=0.85, name="glow")
        # the cloud that is left hanging: bigger puffs born as it spreads, dark, that grow and thin
        for i in range(14):
            # (born small inside the cloud and as dark as it: born big and paler they stood in front of it as round glassy discs)
            smoke(q.uniform(0.12, 0.6), q.uniform(0.0, 7.0), q.uniform(0.3, 2.0), q.uniform(3, 14), q.uniform(5.0, 8.0), (0.7, q.uniform(6.0, 10.5)),
                  q.uniform(0.014, 0.04), q.uniform(0.5, 0.7), 0.55, 0.12, 0.2, 1.5)
        # scraps of her: dark specks flung out and falling, one in six trailing a wisp of smoke for a moment
        # (the art director, 2026-11-18 15:28Z: long straight grey threads behind dark chips "read as rockets". Now a
        #  wisp a few metres long that curls, is gone almost as it is laid, and stops within a second.)
        for i in range(int(8 + 8 * size)):
            v = way * q.uniform(0.55, 0.95) + any_way(q) * q.uniform(12, 42) * size
            big, life, drag = q.uniform(0.06, 0.15), q.uniform(2.4, 3.4), q.uniform(0.7, 1.5)
            fx.puff(t, at, vel=v, life=life, r=(big, big), col=(0.03, 0.028, 0.026), alpha=1.0, kind="bit", drag=drag, gravity=9.8, wind=1.0,
                    squash=(q.uniform(0.8, 2.2), 1.0, q.uniform(0.3, 0.7)), fade_in=0.004, hold=0.9, name="bit", rng=q)
            if i % 6 == 0:
                def spot(p0=at.copy(), v=v, k=drag, born=t):
                    a = max(fx.now - born, 0.0)
                    going = v * math.exp(-k * a) + Vector((0.0, 0.0, -9.8 * a))           # (through the air: the thread lies along it)
                    return p0 + v * ((1.0 - math.exp(-k * a)) / k) + fx.wind * a + Vector((0.0, 0.0, -4.9 * a * a)), (going.normalized() if going.length > 0.5 else X)

                def thread(tt, pos, facing):
                    # (each puff drawn out along the scrap's way: round, the first of them stood as a row of dots)
                    c = q.uniform(0.1, 0.18)
                    curl = Vector((math.sin(31.0 * tt), math.cos(23.0 * tt + 1.0), math.sin(17.0 * tt + 2.0))) * 0.5
                    p = fx.puff(tt, pos + curl, vel=curl * 6.0, life=q.uniform(0.05, 0.13), r=(0.2, q.uniform(0.4, 0.6)), col=(c, c, c), alpha=0.3, fade_in=0.01, grow=0.6, wind=1.0, squash=(2.0, 1.0, 1.0), name="soot", rng=q)
                    p["ob"].rotation_euler = (facing.to_track_quat("X", "Z") @ Quaternion(X, q.uniform(0, 6.283))).to_euler()
                fx.stream(spot, 0.012, thread, start=t + 0.06, end=t + min(life, q.uniform(0.5, 0.9)))      # (it burns out)
        fx.light(t, at, 4.0e3 * size * size, 0.14, r=3.0)          # (a frame or two, and only what is close to it is lit)
    fx.once(t0, where, make)


TRACER = {"one_in": 5, "col": (0.15, 0.133, 0.0), "col2": (0.15, 0.12, 0.0), "long": 16.0, "thick": 0.09, "life": 1.25, "least": 2.6, "flicker": 1.3, "screen": 0.5}
# (Brian, 2026-11-18: "i want tracer fire shown as well. 1 out of every 5 bullets with yellow". As a belt was loaded:
#  every fifth round a tracer, the four between them not seen. Its yellow is thrown dim, like the flames: a brighter one
#  bleaches to cream under the picture's tone curve, and cream is lost against a pale sky; and with nearly as much
#  green in it as red (radiance 1.05, 0.93, 0 shows about 213, 199, 104): the flames' 1.4, 0.85, 0 is amber, and
#  through a soft edge over the sky it was straw (219, 189, 135 measured). It is drawn out as a film
#  camera's shutter would draw it (16 m: a forty-eighth of a second), and its core never thinner than about three
#  pixels, with a faint glow as wide again round it: a round a hand wide is not seen at 300 m, its light is.)
# (The art director, 2026-11-18 15:57Z: "the dashes lie scattered at every angle like straw"; wanted, each gun a stream,
#  "dashes evenly spaced along one path from the gun to its target". At its true 850 m/s a round goes 70 m between two
#  pictures of a clip of twelve a second, and a pair of guns' tracers are 170 m apart: one dash or two to a gun in any
#  picture, and no stream. So on the screen a round goes at half its speed ("screen"), and lives twice as long to reach
#  as far: a pair's dashes are 85 m apart, four or five on the way to a fighter 400 m off, and each steps 35 m a
#  picture, which the eye follows. A game drawing sixty pictures a second can let them fly at their true speed.)


def guns(fx, plane, t0, t1, sockets=None, every=0.085, aim=None, speed=780.0, cue=None):
    """A machine's guns: a flicker at each muzzle for every round, and every fifth round a yellow tracer going away
    (toward `aim()` if given, else along the bore). `cue`: the sound of them, for the clip's sound track."""
    g = fx.rng
    names = sockets or [s["name"] for s in plane.m.sockets if s["name"].startswith("gun.")]
    if cue and names:
        plane.air.cue(cue, t0, plane, t1=t1, kind="burst")
    v_seen = speed * TRACER["screen"]

    def gun(k):
        belt = {"n": 2 * k}                                  # (the guns' belts are out of step: their tracers do not all leave together)
        ph = 1.7 * (k // 2) + 0.9 * len(fx.streams)          # (a pair of guns in one mounting wander together)

        def make(t, at, facing):
            way = (aim() - at).normalized() if aim else facing
            # (a gunner's hand: the stream wanders a little as a whole, slowly, besides each round's own scatter)
            way = (way + Vector((math.sin(2.3 * t + ph), math.sin(1.9 * t + 2.0 * ph), math.sin(2.9 * t + 0.6 * ph))) * 0.006).normalized()
            # (a short yellow tongue along the bore: as a round bright ball each gun wore a cream bead, and a wing's guns a string of them)
            f = fx.puff(t, at + way * 0.55, vel=plane.vel, life=0.04, r=(0.8, 1.0), col=(0.193, 0.111, 0.0), col2=(0.17, 0.07, 0.001), alpha=1.0, kind="heart", wind=0.0, fade_in=0.004, hold=0.8,
                        squash=(g.uniform(0.45, 0.8), 0.16, 0.16), least=TRACER["flicker"], name="flick", rng=g)
            f["ob"].rotation_euler = (way.to_track_quat("X", "Z") @ Quaternion(X, g.uniform(0, 6.283))).to_euler()
            belt["n"] += 1
            if belt["n"] % TRACER["one_in"] == 0:
                way = (way + Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-1, 1))) * 0.003).normalized()
                go = way * v_seen + plane.vel
                way = go.normalized()                        # (it lies along its own path as the camera's formation sees it)
                p = fx.puff(t, at + way * 8.0, vel=go, life=TRACER["life"], r=(TRACER["thick"], TRACER["thick"]), col=TRACER["col"], col2=TRACER["col2"], alpha=1.0,
                            kind="tracer", wind=0.0, squash=(0.5 * TRACER["long"] / TRACER["thick"], 1.0, 1.0), fade_in=0.005, hold=0.9, least=TRACER["least"])
                p["ob"].rotation_euler = way.to_track_quat("X", "Z").to_euler()
        return make
    for k, nm in enumerate(names):
        fx.stream(plane.where(nm), every, gun(k), start=t0 + 0.02 * k, end=t1)


def flak(air, t0, at, size=1.0):
    """A heavy shell bursts among them: an orange-red flash for a frame or two, then a ragged black cloud, densest in
    its middle and thin and torn at its edge, with a few wisps and stubby arms, left standing in the air (so a formation
    flies on past it) and going from black through charcoal to grey-brown as it spreads. `at`: where, as the formation
    sees it at that moment. Returns nothing; whoever is near it is shaken by `jolt`."""
    fx = air.fx
    q = random.Random(int(at[0] * 31.0 + at[1] * 17.0 + at[2] * 7.0 + t0 * 101.0))
    at = Vector(at)

    def any_way():
        d = Vector((q.uniform(-1, 1), q.uniform(-1, 1), q.uniform(-1, 1)))
        return d.normalized() if d.length > 0.05 else Z.copy()

    def make(t, _at, facing):
        # the flash: a lumpy orange-red ball in its middle and a few short tongues out of it, seen for a frame or two
        # before the smoke closes over it (the smoke comes up over a tenth of a second)
        for i in range(9):
            d = any_way()
            lump = i < 4
            long = (q.uniform(2.2, 3.0) if lump else q.uniform(2.6, 4.2)) * size
            wide = long * (q.uniform(0.7, 0.9) if lump else q.uniform(0.36, 0.5))
            p = fx.puff(t, at + d * long * (0.12 if lump else 0.3), vel=d * q.uniform(3, 9) * size, life=q.uniform(0.15, 0.2), r=(0.75, 1.0), col=(0.14, 0.026, 0.004), col2=(0.1, 0.014, 0.002), alpha=0.94,
                        kind="heart" if lump else "tongue", drag=3.0, wind=1.0, squash=(0.5 * long, 0.5 * wide, 0.5 * wide), fade_in=0.004, grow=0.5, hold=0.6, cool=1.0, name="flak", rng=q)
            p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion(X, q.uniform(0, 6.283))).to_euler()

        def smoke(d, reach, r0, r1, alpha, late=0.0, squash=None, thin=False):
            # (thrown `reach` metres from the middle and stopped there by the air: drag 3.2, so its speed is 3.2 x reach.
            #  Each is left to the air a little differently, so the cloud draws out astern as it ages; it goes from
            #  black through charcoal to a grey-brown.)
            #  (Its thin parts are a shade browner and lighter: thin black over a blue sky was a dark blue glass.)
            c = q.uniform(0.022, 0.04) if thin else q.uniform(0.008, 0.024)
            k = q.uniform(0.85, 1.2)
            p = fx.puff(t + late, at + d * q.uniform(0.1, 0.6) * size, vel=d * 3.2 * reach * size, life=q.uniform(6.5, 9.5), r=(r0 * size, r1 * size),
                        col=(c * 1.3, c * 1.05, c * 0.85) if thin else (c * 1.15, c, c * 0.9), col2=(0.105 * k, 0.09 * k, 0.076 * k), alpha=alpha, drag=3.2, fade_in=0.11, grow=0.45, wind=q.uniform(0.986, 1.014), hold=0.12, cool=1.25,
                        squash=squash or (1.0, 1.0, 1.0), name="flak", rng=q)
            if squash:
                p["ob"].rotation_euler = ((d + any_way() * 0.5).normalized().to_track_quat("X", "Z") @ Quaternion(X, q.uniform(0, 6.283))).to_euler()
        lean = any_way()
        # the smoke. (As three dozen small puffs thrown wide it stood as a bunch of separate dots, "frogspawn"; as big
        # dense puffs one on another, "inkblots": as black at its edge as in its middle, a hard ball. Now the dense
        # stuff is only its heart; round that, thinner lumps, more on one side; round those, a thin veil the sky shows
        # through; and wisps and arms out of it, thinner outward. No two bursts have the same share of each.)
        body = q.uniform(0.75, 1.25)
        for i in range(int(7 * size * body) + 4):            # its heart
            smoke(any_way(), q.uniform(0.2, 1.5), q.uniform(1.0, 1.5), q.uniform(2.6, 3.8), q.uniform(0.78, 0.94), q.uniform(0.0, 0.03))
        for i in range(int(11 * size) + 4):                  # the lumps round it: fewer on one side than the other
            d = any_way()
            if d.dot(lean) < -0.2 and q.random() < 0.65:
                d = -d
            smoke(d, q.uniform(1.8, 4.4), q.uniform(0.8, 1.3), q.uniform(2.6, 4.2), q.uniform(0.38, 0.68), q.uniform(0.0, 0.05))
        for i in range(int(9 * size) + 3):                   # the veil: big, thin, torn
            d = any_way()
            if d.dot(lean) < 0.0 and q.random() < 0.5:
                d = -d
            smoke(d, q.uniform(3.2, 6.4), q.uniform(1.3, 2.0), q.uniform(4.2, 6.6), q.uniform(0.14, 0.3), q.uniform(0.0, 0.06), thin=True)
        # wisps: short ragged streaks drawn out of its edge (long thin ones stood out of it like slivers of glass)
        for i in range(q.choice((3, 4, 5))):
            smoke(any_way(), q.uniform(3.8, 6.0), q.uniform(0.6, 0.9), q.uniform(1.7, 2.5), q.uniform(0.36, 0.56), q.uniform(0.0, 0.05), squash=(q.uniform(1.5, 2.1), 0.85, 0.8), thin=True)
        for k in range(q.choice((1, 2, 2, 3))):              # its arms: each a few puffs one on the next, smaller and thinner outward
            d = any_way()
            n = q.choice((3, 4, 5))
            for j in range(n):
                dd = (d + any_way() * 0.16).normalized()
                smoke(dd, 3.9 + 1.4 * j + q.uniform(-0.4, 0.4), 1.1 - 0.13 * j, 3.0 - 0.4 * j, q.uniform(0.62, 0.8) - 0.09 * j, 0.008 * j)
        fx.light(t, at, 2.5e3 * size * size, 0.1, r=2.0)
    fx.once(t0, (at, X), make)
    air.cue("flak_burst_01", t0, at, drift=True, kind="shot")


def jolt(b, t0, at, size=1.0, reach=110.0):
    """A burst near a bomber throws her: away from it, harder the nearer, when its blast reaches her; she rings for
    half a second and is back in her place. (`ride` adds up what is in `b.jolts`.)"""
    away = b.home - Vector(at)
    far = away.length
    if far < reach:
        push = min(2.2, 30.0 * size / max(far, 12.0))
        b.jolts.append((t0 + far / 340.0, away.normalized() * push + Z * (0.4 * push)))


class Flight:
    """A machine flown through points at moments (as the formation sees them): she points the way she goes through the
    air and banks into her turns; `rolls` (moment, degrees) are added to her bank, eased from one to the next."""

    def __init__(self, keys, rolls=()):
        self.keys = sorted((float(t), Vector(p)) for t, p in keys)
        self.rolls = sorted((float(t), float(r)) for t, r in rolls)

    def pos(self, t):
        k = self.keys
        if t <= k[0][0]:
            v = (k[1][1] - k[0][1]) / (k[1][0] - k[0][0])
            return k[0][1] + v * (t - k[0][0])
        if t >= k[-1][0]:
            v = (k[-1][1] - k[-2][1]) / (k[-1][0] - k[-2][0])
            return k[-1][1] + v * (t - k[-1][0])
        i = max(j for j in range(len(k) - 1) if k[j][0] <= t)

        def slope(j):
            a, b = max(j - 1, 0), min(j + 1, len(k) - 1)
            return (k[b][1] - k[a][1]) / (k[b][0] - k[a][0])
        h = k[i + 1][0] - k[i][0]
        s = (t - k[i][0]) / h
        return ((2 * s ** 3 - 3 * s ** 2 + 1) * k[i][1] + (s ** 3 - 2 * s ** 2 + s) * h * slope(i)
                + (-2 * s ** 3 + 3 * s ** 2) * k[i + 1][1] + (s ** 3 - s ** 2) * h * slope(i + 1))

    def roll(self, t):
        r = self.rolls
        if not r:
            return 0.0
        if t <= r[0][0]:
            return r[0][1]
        for (t0, a), (t1, b) in zip(r, r[1:]):
            if t <= t1:
                return a + (b - a) * smooth((t - t0) / (t1 - t0))
        return r[-1][1]

    def at(self, t, speed):
        """Where she is, how she lies, and her way against the formation."""
        e = 0.06
        p, a, b = self.pos(t), self.pos(t - e), self.pos(t + e)
        vel = (b - a) / (2 * e)
        acc = (b - 2 * p + a) / (e * e)
        fwd = vel + X * speed
        fwd = fwd.normalized() if fwd.length > 1.0 else X.copy()
        lift = Vector((acc.x, acc.y, 0.0)) + Z * 9.8         # (she banks into a turn; rolling over to pull down is given by `rolls`)
        lift = lift - fwd * lift.dot(fwd)
        up = lift.normalized() if lift.length > 0.5 else Z.copy()
        if abs(up.dot(fwd)) > 0.99:
            up = Y.copy()
        side = up.cross(fwd).normalized()
        up = fwd.cross(side).normalized()
        from mathutils import Matrix
        q = Matrix((fwd, side, up)).transposed().to_quaternion() @ Quaternion(X, math.radians(self.roll(t)))
        return p, q, vel


def fly(plane, flight, t, clip="fly", *more):
    at, q, vel = flight.at(t, plane.air.speed)
    plane.vel = vel
    plane.put(at, plane.play(clip, t), *more, q=q)


def strikes(fx, plane, t0, t1, sockets=("fuel", "engine.1", "tank.port")):
    """Hits sparkling on a machine under fire."""
    g = fx.rng

    def make(t, at, facing):
        # (a spit of orange sparks and a torn bit flung off; a round glowing ball here was "a smooth white bead")
        p0 = at + Vector((g.uniform(-1.5, 1.5), g.uniform(-1.0, 1.0), g.uniform(-0.3, 0.6)))
        for i in range(4):
            d = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-1, 1)))
            d = d.normalized() if d.length > 0.05 else Z.copy()
            long = g.uniform(0.45, 1.0) * 1.25
            wide = long * g.uniform(0.28, 0.42) * 0.5        # (thin spits; fat ones were petals stuck on her)
            k = g.uniform(0.8, 1.2)
            p = fx.puff(t, p0 + d * long * 0.4, vel=plane.vel + d * g.uniform(3, 9), life=g.uniform(0.07, 0.12), r=(0.6, 1.0), col=(0.15 * k, 0.028 * k, 0.004), col2=(0.1, 0.014, 0.002), alpha=0.92,
                        kind="tongue" if i % 2 else "lick", drag=3.0, wind=0.0, squash=(0.5 * long, 0.5 * wide, 0.5 * wide), fade_in=0.006, grow=0.5, hold=0.4, cool=1.0)
            p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion(X, g.uniform(0, 6.283))).to_euler()
        if g.random() < 0.6:
            v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-0.4, 1))).normalized() * g.uniform(8, 22)
            fx.puff(t, p0, vel=plane.vel + v, life=g.uniform(0.5, 1.0), r=(0.1, 0.06), col=(0.08, 0.07, 0.06), alpha=1.0, drag=0.5, gravity=9.8, wind=0.4, fade_in=0.01)
    for k, nm in enumerate(sockets):
        fx.stream(plane.where(nm), 0.13, make, start=t0 + 0.04 * k, end=t1)


# ---- the shots ---------------------------------------------------------------------------------------

def flypast(A, opt):
    T = 8.0
    name = opt.get("plane", "Spitfire")
    lead = Plane(A, name, (0, 0, 0))
    mate = Plane(A, name, (-34.0, -24.0, -5.0))
    span = lead.f.half * 2.0
    for p in (lead, mate):                                   # (a jet leaves a thin dark thread from each tailpipe)
        for k in p.m.sockets:
            if k["name"].startswith("jet."):
                trail(A.fx, p.where(k["name"]), -1.5, kind="thin", size=0.2)

    def step(t):
        lead.put(None, lead.play("fly", t))
        mate.put(None, mate.play("fly", t + 1.3))
        A.at(t)
        ang = math.radians(-128.0 + 300.0 * smooth(t / T) * 0.999 + 20.0 * t / T)
        # (the nearest a battle's camera comes: about a wing's length off her, so her pilot and her rivets are seen)
        r = span * (1.3 - 0.3 * math.sin(math.pi * t / T))
        kit.look_at(A.cam, (math.cos(ang) * r - 0.8, math.sin(ang) * r, span * (0.36 - 0.24 * math.sin(math.pi * t / T))), (-0.8, -0.4, 0.0))
    A.cam.data.lens = 40.0
    return T, step


def shot_down(A, opt):
    T = 11.0
    dt = 1.0 / float(opt.get("fps", 12))
    bomber, fighter = opt.get("bomber", "He111"), opt.get("fighter", "Spitfire")
    lead = Plane(A, bomber, (0, 0, 0))
    left = Plane(A, bomber, (-38.0, 34.0, 7.0))              # the one that is lost (to port of the leader, nearest the camera)
    # (stepped up like the other wing bomber, not down: seen from the camera she then flies above the stricken one's
    #  smoke. Lower, she flew behind it for a second and came out of it a crisp brown cut-out with her green gone,
    #  "a ghost plane": the art director, 11:16 UTC on 18 November 2026. Smoke in front of a machine should soften
    #  her edges and grey her; these puffs only tint what is behind them.)
    right = Plane(A, bomber, (-38.0, -34.0, 7.0))
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
            # (the thin grey streamers begin once the pieces are clear of the fireball's smoke: begun inside it, their
            #  pale young puffs stood in front of the black cloud as round glassy discs)
            trail(fx, p.where("break.wing.port"), t0 + 0.6, kind="thin")
            trail(fx, p.where("break.tail"), t0 + 0.6, kind="thin")
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
        b = t - p.t0
        if p.way == "fall_explode" and b > 0.0:
            # (the fireball's smoke is left hanging where she blew up while her pieces fall away: the camera goes on ahead
            #  of the wreck and looks back along her path, so that the cloud stands behind the pieces and above them for
            #  the rest of the cut. From abeam the cloud is out of the picture in a third of a second.)
            a0 = p.t0 - p.cut0
            s = smooth(min(b / 1.4, 1.0))
            abeam = Vector((far * 0.55 - 14.0 * a0, far * 0.95, -far * 0.03 - 1.0 * a0))
            ahead = Vector((far * 1.3, far * 1.0, -far * 0.1))
            eye = p.at + abeam.lerp(ahead, s)
            cloud = p.home + left_behind(fx, b)[0]
            to_her, to_cloud = (p.at - eye).normalized(), (cloud - eye).normalized()
            look = eye + (to_her + to_cloud * s).normalized() * far
        A.cam.data.lens = 42.0
        kit.look_at(A.cam, eye, look)
    return T, step


# ---- B-17s under attack (Brian, 2026-11-18: "can i see more b-17 battle scenes in 3d with fighters?") --------------
# What is heard, by machine (the sound director's mix.json, section air: no Bf 109 was ever recorded, so it takes the
# nearest real engine, the Spitfire's single V12, and one heavy gun's burst; "plane_guns" is six to eight guns together).
DRONE_DB = -5.0          # (the formation's drone, under the guns: they are to stand 8 to 10 dB over it)
GUN_SOUND = {"Bf109": "gunner_burst", "Fw190": "plane_guns", "Mustang": "plane_guns"}
PASS_SOUND = {"Bf109": "spitfire_pass", "Fw190": "fw190_pass", "Mustang": "p51_pass"}

FRONT_GUNS = ("turret.9", "turret.10", "turret.1", "turret.2", "turret.3", "turret.4")       # top turret, chin, cheeks
REAR_GUNS = ("turret.7", "turret.8", "turret.11", "turret.12", "turret.9", "turret.10")       # tail, ball, top turret


def bombers(A, spots, T, scheme="1944-01"):
    """A box of B-17s, each droning, each riding the air a little in her own time."""
    out = []
    for k, at in enumerate(spots):
        b = Plane(A, "B17", at, scheme=scheme)
        b.home, b.phase, b.firing, b.jolts = Vector(at), 0.9 * k, [], []
        A.cue("b17_drone_loop", 0.0, b, t1=T, kind="loop", gain=DRONE_DB)
        out.append(b)
    return out


def ride(b, t, at=None, *more, q=None):
    """A bomber in her place in the box at t (or at `at`), her gunners watching, and working their guns while they fire."""
    ph = b.phase
    p = (b.home if at is None else Vector(at)) + Vector((0.8 * math.sin(0.31 * t + ph), 0.6 * math.sin(0.43 * t + 2.0 * ph), 0.9 * math.sin(0.52 * t + 1.3 * ph)))
    for t0, push in b.jolts:                                 # (a near burst: thrown, and ringing back to her place)
        if 0.0 <= t - t0 < 1.6:
            a_ = t - t0
            p += push * (math.exp(-3.4 * a_) * math.sin(10.5 * a_) * 1.35)
    busy = any(t0 <= t <= t1 for t0, t1 in b.firing)
    b.put(p, b.play("fly", t + ph), b.play("watch", t + ph), b.play("fire.turrets", t) if busy else {}, *more, q=q)


def gunners(A, b, t0, t1, sockets, target, speed=850.0):
    """A bomber's gunners firing from t0 to t1 at whatever `target()` gives (a point)."""
    b.firing.append((t0, t1))
    guns(A.fx, b, t0, t1, sockets=list(sockets), every=0.08, aim=target, speed=speed, cue="gunner_burst")


def nearest(b, foes, ahead):
    """The nearest of `foes` still ahead of a bomber (or astern of her), as a point to shoot at: a function of nothing."""
    def pick():
        best = None
        for f in foes:
            dx = f.at.x - b.at.x
            if (dx > 12.0) != ahead or getattr(f, "lost", False):
                continue
            d = (f.at - b.at).length
            if best is None or d < best[0]:
                best = (d, f)
        if best is None:
            return b.at + X * (600.0 if ahead else -600.0)
        f = best[1]
        return f.at + f.vel * (best[0] / (850.0 * TRACER["screen"]))     # (led by how far she flies while the rounds are on their way)
    return pick


def b17_head_on(A, opt):
    """Twelve o'clock high: five Fw 190s come head-on through a box of B-17s, a three and then a pair. The bombers' top
    turrets, chin turrets and cheek guns hammer at them as they close; one bomber's engine is set on fire and one
    fighter goes down through the box burning. Seen from behind and above the rear three, looking the way they fly."""
    T = 8.0
    fx = A.fx
    fx.haze = A.far_air
    bs = bombers(A, [(0, 0, 0), (-38, 34, 7), (-38, -34, 7), (150, -80, 28), (112, -46, 35), (112, -114, 35)], T)
    lead, left, right = bs[0], bs[1], bs[2]
    runs = [  # (her path, her bank, when she fires, at whom)
        (Flight([(0, (1078, 8, 40)), (3.4, (245, 6, 20)), (4.0, (98, 5, 9)), (4.4, (0, 3, -13)), (4.9, (-118, 0, -52)), (5.6, (-280, -10, -130))], [(3.5, 0), (4.1, 180), (5.6, 180)]), (1.87, 3.71), lead),
        (Flight([(0, (1078, -22, 44)), (3.4, (245, -20, 26)), (4.4, (0, -19, 24)), (5.1, (-172, -22, 50)), (5.8, (-340, -30, 90))], [(4.2, 0), (5.2, 360)]), (1.9, 3.7), right),
        (Flight([(0, (1103, 34, 38)), (3.5, (245, 36, 22)), (4.2, (74, 44, 14)), (4.5, (0, 52, 2)), (5.0, (-120, 64, -36)), (5.7, (-285, 80, -110))], [(3.7, 0), (4.3, -150), (5.7, -170)]), (2.0, 3.8), left),
        (Flight([(0, (1568, -6, 42)), (5.4, (245, -4, 22)), (5.9, (122, -2, 12)), (6.4, (0, 2, -16)), (6.9, (-120, 6, -60)), (7.6, (-290, 10, -150))], [(5.5, 0), (6.1, 160), (7.6, 200)]), (3.9, 5.25), lead),
        (Flight([(0, (1592, 22, 46)), (5.5, (245, 20, 27)), (6.5, (0, 17, 25)), (7.2, (-172, 20, 46)), (8.0, (-368, 26, 80))], [(6.3, 0), (7.3, -360)]), (4.0, 5.8), left),
    ]
    fs = []
    for path, (t0, t1), prey in runs:
        f = Plane(A, "Fw190")
        f.path = path
        fly(f, path, 0.0)
        guns(fx, f, t0, t1, aim=lambda prey=prey: prey.at + Vector((2.0, 0.0, 0.0)), speed=760.0, cue="plane_guns")
        A.cue("fw190_pass", 0.0, f, kind="pass")
        fs.append(f)
    # the gunners: at whoever is nearest ahead while they come on, then the tail and ball guns after them as they go
    for b, (a0, a1, r0, r1) in zip(bs, [(1.3, 6.35, 4.5, 7.4), (1.5, 6.4, 4.6, 7.5), (1.4, 6.35, 4.6, 7.3), (0.7, 5.8, 4.0, 6.6), (0.9, 5.9, 4.1, 6.7), (0.8, 5.85, 4.2, 6.6)]):
        gunners(A, b, a0, a1, FRONT_GUNS[:4] if b is not lead else FRONT_GUNS, nearest(b, fs, True))
        gunners(A, b, r0, r1, REAR_GUNS[:4], nearest(b, fs, False))
    # what the cannon do: the port wing bomber's No. 2 engine is set on fire; the leader and one ahead are hit and smoke
    strikes(fx, left, 3.0, 3.75, sockets=("engine.2", "tank.port"))
    A.cue("bullet_crack", 3.0, left)
    trail(fx, left.where("engine.2"), 3.3, kind="thin", size=0.9)
    trail(fx, left.where("engine.2"), 3.8, kind="flame", size=1.0)
    trail(fx, left.where("engine.2"), 3.8, kind="black", size=0.9)
    strikes(fx, lead, 3.1, 3.6, sockets=("fuel", "engine.3"))
    A.cue("bullet_crack", 3.25, lead)
    trail(fx, lead.where("engine.3"), 3.6, kind="thin", size=0.8)
    strikes(fx, bs[3], 2.5, 3.1, sockets=("engine.1", "fuel"))
    trail(fx, bs[3].where("engine.1"), 3.0, kind="thin", size=0.8)
    # and what the gunners do: the fourth fighter is hit as she closes, and goes down through the box burning
    hit = fs[3]
    strikes(fx, hit, 5.0, 5.4, sockets=("engine.1", "fuel"))
    trail(fx, hit.where("engine.1"), 5.3, kind="flame", size=0.8)
    trail(fx, hit.where("engine.1"), 5.3, kind="black", size=0.9)
    A.cue("plane_down", 5.4, hit)
    trail(fx, fs[2].where("engine.1"), 4.0, kind="thin", size=0.4)

    def step(t):
        for b in bs:
            ride(b, t, b.home + Vector((-0.5 * max(t - 4.0, 0.0) ** 2, 0, -0.25 * max(t - 4.0, 0.0) ** 2)) if b is left else None)
        for f in fs:
            fly(f, f.path, t, "fly", f.play("fire", t) if any(s["name"] == "plane_guns" and s["where"] is f and s["t0"] <= t <= s["t1"] for s in A.sounds) else {})
        # (the art director, 2026-11-18 15:57Z: from behind and above the whole box "the bombers are small, the frame
        #  half empty, the fighters specks". Now from inside the box: just above and behind the port wing bomber's
        #  tail, she across the near third of the picture and the rest stacked round her, the horizon in the lower
        #  third, and the fighters coming through close by the camera.)
        eye = left.at + Vector((-30.0, -6.0, 6.5))
        look = eye + Vector((180.0, -24.0 + 1.2 * t, 15.5))
        A.cam.data.lens = 28.0
        kit.look_at(A.cam, eye, look)
        A.at(t)                                              # (after the camera: a tracer is drawn as thick as the camera needs to see it)
        A.hear(t, eye, look)
    return T, step


def b17_escort(A, opt):
    """Little friends: two Bf 109s on a B-17's tail, her gunners firing, cannon shells sparkling on her; two P-51s come
    down on them. Seen first over the nearer 109's shoulder as it closes (its fire going out to her, hers coming back),
    then from her tail guns: that 109 blows up astern and its smoke is left hanging there while the Mustang comes on
    through and past. (No machine here closes faster than it could fly: a 109 creeping up astern at 40 to 45 m/s over
    the bombers' 95, a Mustang coming down at about 100 over.)"""
    T, cut, burst = 8.0, 3.0, 4.0
    fx = A.fx
    fx.haze = A.far_air
    charlie, b2, b3 = bombers(A, [(0, 0, 0), (-66, -34, 13), (-30, -72, 20)], T)
    a, b = Plane(A, "Bf109"), Plane(A, "Bf109")
    one, two = Plane(A, "Mustang", scheme="1944-01"), Plane(A, "Mustang", scheme="1944-01")
    # (she comes up from low astern as she closes, so that from the bomber's tail she is seen against the sky and
    #  not against the cloud below; and to within 100 m, where she is big in the gunner's sight)
    a.path = Flight([(0, (-300, 5, -20)), (2.0, (-200, 4, -11)), (3.0, (-150, 3, 3)), (4.0, (-100, 2, 15)), (5.0, (-56, 2, 22))])
    b.path = Flight([(0, (-270, 28, -8)), (3.3, (-150, 26, 0)), (4.0, (-124, 38, -9)), (4.8, (-102, 80, -52)), (6.2, (-72, 198, -190)), (8.0, (-42, 368, -420))], [(3.3, 0), (3.9, -110), (6.0, -60), (8.0, -30)])
    one.path = Flight([(0, (-600, 60, 150)), (1.8, (-440, 34, 68)), (3.0, (-340, 14, 26)), (4.0, (-250, 12, 10)), (4.5, (-205, 16, 10)), (5.0, (-160, 20, 11)), (5.6, (-106, 23, 12)), (6.2, (-52, 25, 13)),
                       (7.0, (20, 28, 22)), (8.0, (110, 34, 50))], [(5.0, 0), (6.2, 360)])
    two.path = Flight([(0, (-700, 110, 170)), (2.4, (-500, 84, 80)), (3.8, (-360, 66, 34)), (4.6, (-285, 80, -10)), (5.4, (-215, 128, -80)), (6.4, (-170, 215, -185)), (8.0, (-110, 370, -400))])
    for f in (a, b, one, two):
        fly(f, f.path, 0.0)
    # the 109s at her; her gunners, and her neighbours', at them
    guns(fx, a, 0.8, 3.5, aim=lambda: charlie.at + Vector((-4.0, 0.0, 0.5)), speed=760.0, cue=GUN_SOUND["Bf109"])
    guns(fx, b, 1.6, 3.2, aim=lambda: charlie.at + Vector((-2.0, 3.0, 0.0)), speed=760.0, cue=GUN_SOUND["Bf109"])
    foes = [a, b]
    gunners(A, charlie, 0.4, 3.9, ("turret.7", "turret.8", "turret.11", "turret.12"), nearest(charlie, [a], False))
    gunners(A, charlie, 1.2, 3.5, ("turret.6", "turret.9", "turret.10"), nearest(charlie, [b], False))
    gunners(A, b2, 0.9, 3.6, ("turret.7", "turret.8", "turret.6"), nearest(b2, foes, False))
    gunners(A, b3, 1.5, 3.4, ("turret.7", "turret.8"), nearest(b3, foes, False))
    strikes(fx, charlie, 1.4, 3.3, sockets=("break.tail", "tank.port", "engine.2"))
    A.cue("bullet_crack", 1.5, charlie)
    A.cue("bullet_crack", 2.6, charlie)
    # (no smoke from her here: whatever she trails goes astern straight through the tail guns' view of the 109,
    #  and close to the camera it stood as a grey blur over a third of the picture)
    # the Mustangs
    guns(fx, one, 3.0, 3.95, aim=lambda: a.at + a.vel * ((a.at - one.at).length / (850.0 * TRACER["screen"])), speed=850.0, cue="plane_guns")
    strikes(fx, a, 3.35, 4.0, sockets=("fuel", "engine.1", "tank.port"))
    fireball(fx, burst, a.where("fuel"), size=0.9)
    A.cue("plane_explode", burst, a)
    A.cue("aircraft_break_up", burst + 0.05, a)
    trail(fx, a.where("fuel"), burst + 0.1, kind="black", size=0.9)
    trail(fx, a.where("fuel"), burst + 0.1, kind="flame", size=0.9)
    trail(fx, a.where("tank.starboard"), burst + 0.15, kind="flame", size=0.6)
    trail(fx, a.where("break.wing.port"), burst + 0.6, kind="thin")
    trail(fx, a.where("break.tail"), burst + 0.6, kind="thin")
    guns(fx, two, 4.3, 5.3, aim=lambda: b.at + b.vel * ((b.at - two.at).length / (850.0 * TRACER["screen"])), speed=850.0, cue="plane_guns")
    strikes(fx, b, 4.9, 5.3, sockets=("engine.1",))
    trail(fx, b.where("engine.1"), 5.0, kind="thin", size=0.5)
    A.cue("p51_pass", 0.0, one, kind="pass")
    A.cue("fighter_engine_loop", 0.0, two, t1=T, kind="loop")
    # (no "plane_down" for her: a machine blown apart does not wind its engines up as it falls)
    seen = {"t": None, "at": None, "way": None}

    def step(t):
        dt_ = t - seen["t"] if seen["t"] is not None else 0.0
        for k in (charlie, b2, b3):
            ride(k, t, None, k.play("hit", t - 1.5) if k is charlie else {})
        if t < burst:
            fly(a, a.path, t, "fly", a.play("fire", t) if 0.8 <= t <= 3.5 else {}, a.play("hit", t - 3.4))
        else:
            if seen["at"] is None:
                seen["at"], seen["way"] = a.at.copy(), a.vel + X * A.speed
            a.fall(dt_, drag=0.3)
            a.put(None, a.play("fall_explode", t - burst))
        fly(b, b.path, t, "fly", b.play("fire", t) if 1.6 <= t <= 3.2 else {})
        fly(one, one.path, t, "fly", one.play("fire", t) if 3.0 <= t <= 3.95 else {})
        fly(two, two.path, t, "fly", two.play("fire", t) if 4.3 <= t <= 5.3 else {})
        seen["t"] = t
        if t < cut:
            # over the 109's shoulder: it low on the left and close, the bombers ahead, the second 109 beyond it
            # (looking between her and the bomber, so her whole tail and near wing are in the picture)
            eye = a.at + Vector((-27.0 + 1.5 * t, -7.5, 5.2 - 0.4 * t))
            look = eye + ((charlie.at - eye).normalized() * 0.62 + (a.at - eye).normalized() * 0.38).normalized() * 100.0
            A.cam.data.lens = 40.0
        else:
            # from her tail guns. After the burst the camera holds the wreck and the smoke it left, and at the end
            # lets the wreck fall out of the picture and stays with the cloud.
            # (the art director, 2026-11-18 15:57Z: from behind her tail the picture was "pale and empty". Now from
            #  on her back, looking aft past her fin, which stands dark down one side of the picture; the camera looks
            #  up a little, so the fighter comes in against the sky, and her tail guns' fire goes out from under it.)
            eye = charlie.at + Vector((-6.4, 1.15, 2.7))
            look = a.at + Z * (0.07 * (a.at - eye).length)
            A.cam.data.lens = 55.0
            if seen["at"] is not None:
                s = t - burst
                cloud = seen["at"] + left_behind(fx, s, way=seen["way"])[0]
                w = 0.5 * smooth(s / 0.8) + 0.5 * smooth((s - 2.4) / 1.4)
                to_her, to_cloud = (a.at - eye).normalized(), (cloud - eye).normalized()
                look = eye + (to_her * (1.0 - w) + to_cloud * w).normalized() * 100.0 + Z * (7.0 * (1.0 - w))
                A.cam.data.lens = 55.0 - 10.0 * smooth(s / 1.0)
        kit.look_at(A.cam, eye, look)
        A.at(t)
        A.hear(t, eye, look)
    return T, step


def b17_flak(A, opt):
    """Flak, and a straggler: shells burst among a box of B-17s; one bursts under the starboard wing bomber and sets her
    outer engine on fire. She falls back out of the box, two Bf 109s come down on her, her gunners get one of them, and
    her crew take to their parachutes. Seen from off her starboard bow, going back with her."""
    T, t_hit, t_wing = 9.0, 2.3, 6.6
    fx = A.fx
    fx.haze = A.far_air
    bs = bombers(A, [(0, 0, 0), (-38, 34, 7), (-38, -34, 7), (-150, 20, 30), (-188, 54, 37), (-188, -14, 37)], T)
    lead, her = bs[0], bs[2]
    h = her.home
    her.path = Flight([(0, h), (t_hit, h), (4.0, h + Vector((-10, -3, -5))), (6.0, h + Vector((-36, -10, -18))), (7.5, h + Vector((-66, -16, -34))), (9.0, h + Vector((-104, -24, -58))), (10.0, h + Vector((-134, -30, -78)))],
                      [(t_hit, 0), (5.0, 9), (9.0, 24)])
    for t0, at, size in [(0.15, (70, -24, -14), 1.0), (0.5, (120, 10, 24), 1.3), (0.8, (60, 70, 12), 0.7), (1.2, (95, -30, -24), 0.85), (1.5, (20, 50, 30), 1.15), (1.9, (64, -10, 26), 0.65),
                         (t_hit, tuple(h + Vector((8, -10, -5))), 1.2), (2.6, (110, -20, 4), 0.8), (3.1, (50, 40, -18), 1.35), (3.6, (130, 34, 16), 0.7), (4.3, (30, -6, 38), 1.0), (5.2, (90, 20, -10), 1.25),
                         (6.1, (60, 60, 6), 0.75), (7.0, (120, -30, 34), 1.1), (7.9, (40, 10, -20), 0.9)]:
        flak(A, t0, at, size)
        for k in bs:                                         # (and whoever is near it is thrown)
            jolt(k, t0, at, size)
    strikes(fx, her, t_hit, t_hit + 0.3, sockets=("engine.4", "tank.starboard"))
    fireball(fx, t_hit + 0.02, her.where("engine.4"), size=0.3)
    trail(fx, her.where("engine.4"), t_hit + 0.05, kind="flame", size=1.25)
    trail(fx, her.where("engine.4"), t_hit + 0.05, kind="black", size=1.15)
    trail(fx, her.where("tank.starboard"), 6.5, kind="flame", size=0.9)
    a, b = Plane(A, "Bf109"), Plane(A, "Bf109")
    a.path = Flight([(0, (-700, -80, 150)), (3.0, (-470, -66, 104)), (5.0, (-250, -52, 40)), (6.0, (-150, -48, 2)), (6.6, (-95, -58, 2)), (7.2, (-30, -84, 26)), (8.0, (70, -130, 70)), (9.0, (190, -190, 120))],
                    [(6.4, 0), (7.0, 60), (8.0, 0)])
    b.path = Flight([(0, (-790, -40, 170)), (3.9, (-470, -30, 104)), (5.9, (-250, -24, 36)), (6.6, (-185, -24, 12)), (7.2, (-140, -24, 0))])
    for f in (a, b):
        fly(f, f.path, 0.0)
    guns(fx, a, 4.9, 6.2, aim=lambda: her.at, speed=760.0, cue=GUN_SOUND["Bf109"])
    guns(fx, b, 5.8, 6.5, aim=lambda: her.at, speed=760.0, cue=GUN_SOUND["Bf109"])
    strikes(fx, her, 5.3, 6.4, sockets=("break.tail", "fuel", "tank.port"))
    foes = [a, b]
    gunners(A, her, 4.4, 7.3, ("turret.7", "turret.8", "turret.9", "turret.10", "turret.5", "turret.11", "turret.12"), nearest(her, foes, False))
    gunners(A, lead, 4.8, 6.6, ("turret.7", "turret.8", "turret.9", "turret.10"), nearest(lead, foes, False))
    for k in bs[3:]:
        gunners(A, k, 5.0 + 0.2 * bs.index(k), 6.7, ("turret.11", "turret.12", "turret.1", "turret.2"), nearest(k, foes, True))
    # her gunners get the second of them: a wing off, and down she goes
    strikes(fx, b, 6.2, t_wing, sockets=("break.wing.port", "fuel"))
    fireball(fx, t_wing, b.where("break.wing.port"), size=0.3)
    trail(fx, b.where("stub.port"), t_wing + 0.05, kind="flame", size=0.8)
    trail(fx, b.where("stub.port"), t_wing + 0.05, kind="black", size=0.8)
    trail(fx, b.where("break.wing.port"), t_wing + 0.1, kind="thin")
    A.cue("aircraft_break_up", t_wing, b)
    A.cue("plane_down", t_wing + 0.2, b)
    A.cue(PASS_SOUND["Bf109"], 0.0, a, kind="pass")
    # her crew go while the fighters are still on her: the first canopies crack open inside the clip
    crew = [Chute(A, 5.4 + 0.55 * k, her, "hatch" if k % 2 == 0 else "hatch.belly") for k in range(4)]
    for c in crew:
        if c.t0 + c.delay + 0.6 <= T - 1.0:
            A.cue("parachute_open", c.t0 + c.delay + 0.6, (lambda c=c: c.at))
    seen = {"t": None}

    def step(t):
        dt_ = t - seen["t"] if seen["t"] is not None else 0.0
        for k in bs:
            if k is not her:
                ride(k, t)
        at, q, vel = her.path.at(t, A.speed)
        her.vel = vel
        ride(her, t, at, her.play("hit", t - t_hit), her.play("bail_out", t - 6.6), q=q)
        fly(a, a.path, t, "fly", a.play("fire", t) if 4.9 <= t <= 6.2 else {})
        if t < t_wing:
            fly(b, b.path, t, "fly", b.play("fire", t) if 5.8 <= t <= 6.5 else {})
        else:
            b.lost = True
            b.fall(dt_, drag=0.16)
            b.put(None, b.play("fall_spin", t - t_wing))
        for c in crew:
            c.step(t, dt_)
        seen["t"] = t
        eye = Vector((34.0, -100.0, -8.0)) + (her.at - her.home) * 0.75
        look = her.at + Vector((-4.0, 0.0, 2.0))
        A.cam.data.lens = 45.0
        kit.look_at(A.cam, eye, look)
        A.at(t)
        A.hear(t, eye, look)
    return T, step



SHOTS = {"flypast": flypast, "shot_down": shot_down, "ways_down": ways_down, "b17_head_on": b17_head_on, "b17_escort": b17_escort, "b17_flak": b17_flak}
# (the fly-past's camera goes nearly all the way round her and leaves a gap astern to starboard: the sun stands so that its
#  glare off her wings falls in that gap, or her camouflage is washed out to cream where the camera looks up-sun)
SUN = {"flypast": 122.0}


def label(title, span, parts, year=None, tech=None):
    """The line under a machine on the sheet; a machine that only comes late in the war says from which year, and one
    that has to be researched says so."""
    return (f"{title.replace(' (our own model)', '')} · span {span:.1f} m · {parts} parts"
            + (f" · from {year}" if year else " · by research" if tech else ""))


def sheet(argv):
    out = argv[0]
    names = argv[1].split(",") if len(argv) > 1 and argv[1] != "all" else list(planes.BUILDERS)
    cols = int(argv[2]) if len(argv) > 2 else 3
    w, h, spp = (int(v) for v in (argv[3] if len(argv) > 3 else "640x360x10").split("x"))
    tmp = os.path.splitext(out)[0] + "-parts"
    os.makedirs(tmp, exist_ok=True)
    A = Air(clouds=12.0)                                     # (a full overcast below: a plain pale ground to read each machine against)
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
        labels.append(label(p.m.title, span, len(p.m.parts), p.f.year, p.m.info["file"].get("tech")))
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
    if "skin" in opt:
        SKIN[:] = [float(v) for v in opt["skin"].split(",")]
    A = Air(sun_turn=float(opt.get("sun", SUN.get(shot, 148.0))))
    stage.render_setup(w, h, int(opt.get("samples", 12)))
    T, step = SHOTS[shot](A, opt)
    only = ({int(v) - 1 for v in opt["frames"].split(",")} if opt["frames"] != "none" else set()) if "frames" in opt else None
    stage.film(folder, T, fps, step, only=only)
    if A.sounds and "part" not in opt:                       # (what is heard, for `soundtrack.py`: names, moments and places only)
        print("made", A.sound_file(os.path.join(out, f"air-{shot}-sound.json"), T, fps))
    if only is None:
        kit.encode(folder, os.path.join(out, f"air-{shot}"), fps=fps, gif_width=640)
        print("made", os.path.join(out, f"air-{shot}.mp4"))


if __name__ == "__main__":
    main()
