"""Clips of the works for Brian.

    python film.py <scene> <out folder> [WxHxSPP] [fps] [only=a-b] [still=t]

Scenes: battery (a heavy flak gun in its pit lays on a formation and fires, the line's works about it),
breach (a pillbox behind its wire is hit and knocked out).
Frames go to <out>/<scene>/0001.png ...; the clip to <out>/<scene>.mp4 and .gif.
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import kit  # noqa: E402
import look  # noqa: E402
import stage  # noqa: E402
import works  # noqa: E402
from scene import Set, ease, keys  # noqa: E402

Z = Vector((0, 0, 1))


# ---- what the land adds to the sea's puffs ---------------------------------------------------------------

def dust(fx, t0, at, size=1.0, n=14, col=(0.56, 0.48, 0.36), up=0.6, out=5.0, life=(2.4, 4.2), alpha=0.32):
    """Dust lifted off the ground: a low ring rolling outward, thinning as it goes."""
    g = fx.rng
    at = Vector(at)

    def make(t, p0, facing):
        for i in range(n):
            a = g.uniform(0, 2 * math.pi)
            v = Vector((math.cos(a), math.sin(a), 0)) * g.uniform(0.4, 1.0) * out * size + Z * g.uniform(0.1, 1.0) * up * size
            c = g.uniform(0.88, 1.08)
            fx.puff(t + g.uniform(0, 0.05), p0 + Vector((math.cos(a), math.sin(a), 0)) * g.uniform(0.2, 1.2) * size + Z * 0.15, vel=v, life=g.uniform(*life),
                    r=(0.35 * size, g.uniform(1.1, 2.0) * size), col=(col[0] * c, col[1] * c, col[2] * c), alpha=g.uniform(0.6, 1.0) * alpha, drag=g.uniform(2.2, 3.4),
                    rise=0.25, fade_in=0.06, grow=0.5, wind=0.9, squash=(1.0, 1.0, 0.7))
    fx.once(t0, (at, Z), make)


def muzzle(fx, t0, where, bore=0.088, light=True, smoke=1.0):
    """A gun on land fires: a ragged tongue of flame down the bore's line with licks thrown sideways, a white-hot heart
    at the muzzle for an instant, and a cloud of grey-brown smoke that rolls out, hangs and drifts."""
    g = fx.rng
    k = bore / 0.088

    def make(t, at, facing):
        side = facing.cross(Z).normalized()
        up = side.cross(facing)
        L = 1.3 * k
        p = fx.puff(t, at + facing * L * 0.9, vel=facing * 5.0, life=0.14, r=(L * 0.75, L * 1.1), col=(0.62, 0.25, 0.05), col2=(0.5, 0.13, 0.025), alpha=1.0, kind="tongue", drag=6.0,
                    wind=0.0, squash=(1.0, 0.45, 0.45), fade_in=0.01, grow=0.4)
        p["ob"].rotation_euler = facing.to_track_quat("X", "Z").to_euler()
        for i in range(4):
            a = g.uniform(0, 2 * math.pi)
            d = (facing * g.uniform(0.5, 1.0) + (side * math.cos(a) + up * math.sin(a)) * g.uniform(0.5, 0.9)).normalized()
            R = g.uniform(0.35, 0.6) * k
            p = fx.puff(t, at + d * R * 0.9, vel=d * 3.0, life=g.uniform(0.09, 0.13), r=(R * 0.8, R * 1.2), col=(0.55, 0.2, 0.04), col2=(0.42, 0.1, 0.02), alpha=0.95, kind="lick",
                        drag=6.0, wind=0.0, squash=(1.0, 0.5, 0.5), fade_in=0.01, grow=0.4)
            p["ob"].rotation_euler = d.to_track_quat("X", "Z").to_euler()
        fx.puff(t, at + facing * 0.25 * k, life=0.09, r=(0.22 * k, 0.42 * k), col=(1.0, 0.86, 0.6), col2=(1.0, 0.6, 0.25), alpha=0.9, kind="glow", wind=0.0, fade_in=0.01)
        for i in range(int(12 * smoke)):
            sp = g.uniform(5.0, 24.0) * k
            v = facing * sp + side * g.uniform(-3.5, 3.5) * k + up * g.uniform(-1.5, 3.5) * k
            c = g.uniform(0.62, 0.8)
            fx.puff(t + 0.03, at + facing * 0.3 * k, vel=v, life=g.uniform(4.0, 7.0), r=(0.3 * k, g.uniform(1.2, 2.3) * k), col=(0.66, 0.48, 0.28), col2=(c, c * 0.95, c * 0.87),
                    alpha=g.uniform(0.4, 0.62), drag=g.uniform(2.6, 3.6), rise=0.35, fade_in=0.05, grow=0.5, wind=0.9)
        if light:
            fx.light(t, at + facing * 1.2 * k, 9000.0 * k * k, 0.14, r=0.5)
    fx.once(t0, where, make)


def flak_burst(fx, t0, at, size=1.0):
    """A heavy shell bursting high up: a wink of flame and a round black puff that hangs and drifts."""
    g = fx.rng
    at = Vector(at)

    def make(t, p0, facing):
        fx.puff(t, p0, life=0.2, r=(4.0 * size, 9.0 * size), col=(1.0, 0.8, 0.45), col2=(1.0, 0.4, 0.1), alpha=1.0, kind="glow", wind=0.0, fade_in=0.01, grow=0.4)
        for i in range(9):
            v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-0.6, 1))).normalized() * g.uniform(10, 34) * size
            c = g.uniform(0.04, 0.1)
            fx.puff(t + 0.03, p0, vel=v, life=g.uniform(9, 14), r=(4.0 * size, g.uniform(9, 14) * size), col=(0.2, 0.14, 0.1), col2=(c, c, c), alpha=0.85, drag=3.2,
                    rise=0.4, fade_in=0.04, grow=0.4, wind=1.0, hold=0.5)
    fx.once(t0, (at, Z), make)


def chimney(fx, where, start=0.0, end=1e9, size=1.0, dark=0.06):
    """Black smoke pouring from a work that has been knocked out."""
    g = fx.rng

    def make(t, at, facing):
        c = dark * g.uniform(0.7, 1.5)
        fx.puff(t, at + Vector((g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), 0)) * size, vel=facing * g.uniform(1.5, 3.0) + Vector((g.uniform(-0.4, 0.4), g.uniform(-0.4, 0.4), g.uniform(1.6, 3.0))),
                life=g.uniform(7, 11), r=(0.5 * size, g.uniform(3.0, 5.0) * size), col=(c * 1.8, c * 1.3, c), col2=(c, c, c), alpha=0.8, drag=0.5, rise=1.3, fade_in=0.2, grow=0.6,
                wind=1.0, hold=0.3)
    fx.stream(where, 0.14, make, start=start, end=end)


def flames(fx, where, start=0.0, end=1e9, size=1.0):
    g = fx.rng

    def make(t, at, facing):
        fx.puff(t, at + Vector((g.uniform(-0.4, 0.4), g.uniform(-0.4, 0.4), 0.1)) * size, vel=(0, 0, g.uniform(1.5, 3.0)), life=g.uniform(0.4, 0.7), r=(0.5 * size, 0.2 * size),
                col=(1.0, 0.62, 0.2), col2=(1.0, 0.3, 0.06), alpha=0.95, kind="glow", wind=0.3, fade_in=0.05, squash=(1.0, 1.0, 1.7))
    fx.stream(where, 0.07, make, start=start, end=end)


def shell_hit(fx, t0, at, size=1.0, earth=True):
    """A shell bursting on a work or the ground: flash, a fan of thrown earth and dust, grey-brown smoke."""
    g = fx.rng
    at = Vector(at)
    fx.burst(t0, (at, Z), size=0.22 * size)

    def make(t, p0, facing):
        for i in range(16):      # thrown earth: up and out, falling back
            v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0.9, 2.6))).normalized() * g.uniform(7, 16) * size ** 0.5
            c = g.uniform(0.8, 1.1)
            fx.puff(t, p0 + Z * 0.2, vel=v, life=g.uniform(1.6, 2.6), r=(0.3 * size, g.uniform(0.7, 1.4) * size), col=(0.3 * c, 0.24 * c, 0.17 * c), alpha=0.75, gravity=9.8,
                    drag=0.5, fade_in=0.02, grow=0.4, wind=0.2, hold=0.5)
    if earth:
        fx.once(t0, (at, Z), make)
    dust(fx, t0 + 0.05, at, size=1.3 * size, n=18, out=7.0, alpha=0.4)


class Case:
    """An empty case thrown from the breech: it flies back, drops and lies."""

    def __init__(self, cal=0.088, long=0.57):
        bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=cal * 0.62, depth=long)
        self.ob = bpy.context.object
        self.ob.data.materials.append(kit.material("case", kit.hex_rgb("B38F4B"), rough=0.35, metal=1.0))
        for p in self.ob.data.polygons:
            p.use_smooth = True
        self.ob.hide_render = True
        self.t0 = None

    def throw(self, t0, at, back, floor):
        self.t0, self.at, self.floor = t0, Vector(at), floor
        self.v = Vector(back).normalized() * 3.2 + Z * 1.0
        self.side = Vector(back).cross(Z).normalized()

    def at_time(self, t):
        if self.t0 is None or t < self.t0:
            self.ob.hide_render = True
            return
        self.ob.hide_render = False
        a = t - self.t0
        p = self.at + self.v * a - Z * 4.9 * a * a
        spin = a * 9.0
        if p.z < self.floor + 0.06:
            # (lands and lies where it fell)
            tl = (self.v.z + math.sqrt(self.v.z ** 2 + 19.6 * max(self.at.z - self.floor - 0.06, 0.0))) / 9.8
            p = self.at + self.v * tl - Z * 4.9 * tl * tl + Vector((self.v.x, self.v.y, 0)) * 0.12 * min(a - tl, 1.0)
            p.z = self.floor + 0.06
            spin = 0.0
        self.ob.location = p
        d = (Vector((self.v.x, self.v.y, 0)).normalized() * math.cos(spin) + Z * math.sin(spin))
        self.ob.rotation_euler = d.to_track_quat("Z", "Y").to_euler()


# ---- the battery ------------------------------------------------------------------------------------------

def battery(fx, cam, have):
    s = Set()
    if "FlakPit" in have:
        gun = s.put("FlakPit").mounted["gun"]
    else:
        gun = s.put("HeavyFlak", (0.0, 0.0, 0.0))
    if "Pillbox" in have:
        s.put("Pillbox", (32.0, -9.0), -6.0)
    if "MGNest" in have:
        s.put("MGNest", (17.0, 11.0), 6.0)
    if "Wire" in have:
        for k in range(-5, 6):
            s.put("Wire", (42.0 + 0.5 * math.sin(k * 1.7), k * 8.4), 0.0)
    if "DragonsTeeth" in have:
        for k in range(-3, 4):
            s.put("DragonsTeeth", (54.0, k * 12.0), 0.0)
    if "Hedgehogs" in have:
        s.put("Hedgehogs", (48.0, 6.0), 0.0)
    light = None
    if "LightFlak" in have:
        light = s.put("SandbagRing", (30.0, 22.0), 0.0).mounted["gun"] if "SandbagRing" in have else s.put("LightFlak", (30.0, 22.0), 0.0)
    s.lay(seed=3, flat=70.0, sun_turn=125.0, sun_elev=34.0, trees=True, grass_area=62.0, grass_count=230000)

    fires = [1.73, 4.73, 7.73]              # (a fiftieth before a frame, so the frame catches the flash young)
    case = Case()
    for tf in fires:
        muzzle(fx, tf, lambda: gun.where("muzzle"), bore=0.088)
        dust(fx, tf + 0.04, (2.6, 0.0, 0.1), size=1.0, n=12, out=4.0)
        dust(fx, tf + 0.04, (-0.5, 0.0, 0.1), size=0.7, n=8, out=3.0, alpha=0.22)
    # the battery's other guns have the range already: bursts walk across the sky ahead
    g = random.Random(4)
    for k in range(15):
        t = -5.0 + k * 0.95 + g.uniform(-0.3, 0.3)
        flak_burst(fx, t, (2300.0 + g.uniform(-300, 300), 1150.0 - k * 120.0 + g.uniform(-90, 90), 470.0 + g.uniform(-70, 90)), size=g.uniform(1.8, 2.6))
    for tf in fires:
        flak_burst(fx, tf + 1.6, (980.0 + g.uniform(-60, 60), -150.0 + 150.0 * (tf / 9.0) + g.uniform(-40, 40), 600.0 + g.uniform(-40, 40)), size=1.2)
    # the light gun beyond hammers away in clips of four
    light_from = [0.4, 3.4, 6.2]
    if light:
        for t0 in light_from:
            for k in range(4):
                tt = t0 + k * 0.5 - 0.02
                muzzle(fx, tt, lambda: light.where("muzzle"), bore=0.034, smoke=0.5, light=False)

    def lay(t):
        turn = keys([(0.0, -16.0), (1.5, 3.0), (4.5, 7.0), (7.5, 11.0), (9.0, 12.0)], t)
        lift = keys([(0.0, 21.0), (1.5, 33.0), (4.5, 34.5), (7.5, 36.0), (9.0, 36.0)], t)
        return turn, lift

    def step(t):
        turn, lift = lay(t)
        poses = [gun.aim("mount", turn), gun.aim("cradle", lift), gun.aim("handwheel.traverse", turn * 72.0), gun.aim("handwheel.elevate", lift * 45.0)]
        last = [tf for tf in fires if tf <= t]
        if last:
            poses.append(gun.clip("fire", t - last[-1]))
        gun.set(*poses)
        if last and t - last[-1] >= 0.8:
            if case.t0 != last[-1] + 0.8:
                at, back = gun.where("breech")
                case.throw(last[-1] + 0.8, at, back, 0.9)
        case.at_time(t)
        if light:
            lp = [light.aim("mount", -8.0 + 2.0 * t), light.aim("cradle", 40.0 + 0.6 * t)]
            on = [t0 for t0 in light_from if t0 <= t < t0 + 2.0]
            if on:
                lp.append(light.clip("fire", t - on[0]))
            light.set(*lp)
        # the camera walks round behind the gun, looking up the barrel's way
        bearing = 214.0 - 38.0 * t / 9.0
        far = 13.0 - 1.5 * t / 9.0
        high = 3.3 - 0.4 * t / 9.0
        b = math.radians(bearing)
        eye = Vector((math.cos(b) * far, math.sin(b) * far, high))
        kit.look_at(cam, eye, Vector((3.5, 0.5, 3.3)))
        fx.at(t)

    return step, 9.0


# ---- the breach -------------------------------------------------------------------------------------------

def breach(fx, cam, have):
    s = Set()
    box = s.put("Pillbox", (0.0, 0.0), 0.0)
    wires = []
    if "Wire" in have:
        for k in range(-2, 3):
            wires.append(s.put("Wire", (9.5, k * 8.4), 0.0))
    if "Hedgehogs" in have:
        s.put("Hedgehogs", (15.0, -3.0), 0.0)
    if "Fieldworks" in have:
        s.put("Fieldworks", (-1.0, 9.0), 0.0)
        s.put("Fieldworks", (-1.0, -17.0), 0.0)
    s.lay(seed=5, flat=60.0, sun_turn=120.0, sun_elev=36.0, trees=True)
    hit = 2.0
    near = [(0.5, (7.0, 3.5, 0.0)), (1.25, (5.0, -2.5, 0.0))]
    for t0, at in near:
        shell_hit(fx, t0, at, size=1.0)
    at_hit, _ = box.where("hit")
    shell_hit(fx, hit, at_hit, size=1.5, earth=False)
    dust(fx, hit + 0.1, at_hit, size=1.6, n=22, col=(0.6, 0.6, 0.57), out=6.0, alpha=0.45)
    chimney(fx, lambda: (box.where("hit")[0] + Vector((-0.4, 0, 0.5)), Vector((1, 0, 0.3))), start=hit + 0.25, size=0.9)
    chimney(fx, lambda: (box.where("smoke")[0] + Vector((0, 0, 0.6)), Z), start=hit + 0.5, size=0.5)
    flames(fx, lambda: (box.where("fire")[0] + Vector((1.2, 0, 0.2)), Z), start=hit + 0.7, size=0.8)
    fx.light(hit + 0.7, at_hit + Vector((-0.6, 0, 0.3)), 900.0, long=30.0, col=(1.0, 0.5, 0.2), r=0.5)
    mg_until = 1.75

    def step(t):
        poses = []
        if t < mg_until:
            poses.append(box.clip("fire", t % box.m.clips["fire"]["long"]))
            poses.append(box.clip("idle", 0.0))
        elif t < hit:
            poses.append(box.clip("idle", 0.0))
        else:
            poses.append(box.clip("knocked_out", t - hit))
        box.set(*poses)
        bearing = keys([(0.0, -24.0), (7.0, -40.0)], t)
        far = keys([(0.0, 15.0), (7.0, 12.5)], t)
        b = math.radians(bearing)
        shake = 0.05 * math.exp(-(t - hit) * 5.0) * math.sin((t - hit) * 60.0) if t >= hit else 0.0
        eye = Vector((math.cos(b) * far, math.sin(b) * far, 2.6 + shake))
        kit.look_at(cam, eye, Vector((0.6, 0.0, 1.3 + shake)))
        fx.at(t)

    # the machine gun's flashes while it still fires
    g = random.Random(2)
    k = 0
    while k * 0.1 < mg_until - 0.25:
        tt = k * 0.1
        if (tt % 1.1) < 0.65:
            fx.once(tt, lambda: box.where("muzzle"), lambda t, at, facing: fx.puff(t, at + facing * 0.25, vel=facing * 3.0, life=0.09, r=(0.1, 0.28), col=(1.0, 0.8, 0.4),
                                                                                    col2=(1.0, 0.5, 0.15), alpha=1.0, kind="glow", wind=0.0, fade_in=0.01, squash=(1.8, 1.0, 1.0)))
        k += 1
    return step, 7.0


SCENES = {"battery": battery, "breach": breach}


def main():
    name, out = sys.argv[1], sys.argv[2]
    size = next((a for a in sys.argv[3:] if "x" in a and a[0].isdigit()), "960x540x24")
    w, h, spp = (int(v) for v in size.split("x"))
    fps = next((int(a) for a in sys.argv[3:] if a.isdigit()), 12)
    only = next((a[5:] for a in sys.argv if a.startswith("only=")), None)
    still = next((float(a[6:]) for a in sys.argv if a.startswith("still=")), None)
    kit.fresh()
    cam = look.cam(35.0)
    fx = stage.Effects(seed=7, wind=(-1.4, 0.8, 0.0))
    step, seconds = SCENES[name](fx, cam, set(works.BUILDERS))
    sc = stage.render_setup(w, h, spp)
    sc.cycles.max_bounces = 5
    sc.cycles.diffuse_bounces = 3
    folder = os.path.join(out, name)
    if still is not None:
        # (the smoke remembers: walk up to the moment)
        n = int(round(still * fps))
        for i in range(n + 1):
            step(i / fps)
        kit.still(os.path.join(out, f"{name}-{still:05.2f}.png"))
        return
    sel = None
    if only:
        a, b = (int(v) for v in only.split("-"))
        sel = set(range(a - 1, b))
    stage.film(folder, seconds, fps, step, only=sel)
    if not only:
        kit.encode(folder, os.path.join(out, name), fps=fps, gif_width=640)


main()
