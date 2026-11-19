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
import concrete  # noqa: E402
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


def seen_only(p, on=True):
    """A flame that is seen but lights nothing (its light is a lamp's, placed and measured): a burst's own glow on
    the wall and ground beside it washed the whole picture white."""
    p["ob"].visible_diffuse = not on
    p["ob"].visible_glossy = not on
    return p


def muzzle(fx, t0, where, bore=0.088, light=True, smoke=1.0):
    """A gun on land fires: a ragged tongue of flame down the bore's line with licks thrown sideways, a white-hot heart
    at the muzzle for an instant, and a cloud of grey-brown smoke that rolls out, hangs and drifts."""
    g = fx.rng
    k = bore / 0.088

    def make(t, at, facing):
        side = facing.cross(Z).normalized()
        up = side.cross(facing)
        L = 1.3 * k
        # (flame and its light live for less than a frame of a 12 a second clip: on the frame after the shot there is
        # only young smoke, thin and already strung out, which thickens as it rolls. Flame held into that frame, in
        # smoke still bunched at the muzzle, read as a balloon)
        p = fx.puff(t, at + facing * L * 0.9, vel=facing * 5.0, life=0.09, r=(L * 0.75, L * 1.1), col=(0.62, 0.25, 0.05), col2=(0.5, 0.13, 0.025), alpha=1.0, kind="tongue", drag=6.0,
                    wind=0.0, squash=(1.0, 0.45, 0.45), fade_in=0.01, grow=0.4)
        p["ob"].rotation_euler = facing.to_track_quat("X", "Z").to_euler()
        seen_only(p, False)
        for i in range(4):
            a = g.uniform(0, 2 * math.pi)
            d = (facing * g.uniform(0.5, 1.0) + (side * math.cos(a) + up * math.sin(a)) * g.uniform(0.5, 0.9)).normalized()
            R = g.uniform(0.35, 0.6) * k
            p = fx.puff(t, at + d * R * 0.9, vel=d * 3.0, life=g.uniform(0.06, 0.085), r=(R * 0.8, R * 1.2), col=(0.55, 0.2, 0.04), col2=(0.42, 0.1, 0.02), alpha=0.95, kind="lick",
                        drag=6.0, wind=0.0, squash=(1.0, 0.5, 0.5), fade_in=0.01, grow=0.4)
            p["ob"].rotation_euler = d.to_track_quat("X", "Z").to_euler()
        fx.puff(t, at + facing * 0.25 * k, life=0.09, r=(0.22 * k, 0.42 * k), col=(1.0, 0.86, 0.6), col2=(1.0, 0.6, 0.25), alpha=0.9, kind="glow", wind=0.0, fade_in=0.01)
        for i in range(int(12 * smoke)):
            sp = g.uniform(5.0, 24.0) * k
            v = facing * sp + side * g.uniform(-3.5, 3.5) * k + up * g.uniform(-1.5, 3.5) * k
            c = g.uniform(0.62, 0.8)
            fx.puff(t + 0.03, at + facing * 0.3 * k, vel=v, life=g.uniform(4.0, 7.0), r=(0.2 * k, g.uniform(1.2, 2.3) * k), col=(0.5, 0.4, 0.29), col2=(c, c * 0.95, c * 0.87),
                    alpha=g.uniform(0.4, 0.62), drag=g.uniform(2.6, 3.6), rise=0.35, fade_in=0.2, grow=0.5, wind=0.9)
        if light:
            # (the flash lights the gun and its pit for one frame and falls away over the ground beyond: a lamp with a
            # wide soft cone turned back on the gun's foot. It is out before the next frame, and the smoke ahead of
            # the muzzle stands outside the cone: lit from within, the young smoke was a cream balloon)
            fx.light(t, at + facing * 1.4 * k, 9000.0 * k * k, 0.08, r=0.5)
            ob = fx.lights[-1]["ob"]
            ob.data.type = "SPOT"
            ob.data.spot_size, ob.data.spot_blend = math.radians(120.0), 1.0
            foot = at - facing * 4.6 * k
            foot.z = max(0.0, foot.z - 1.2 * k)
            ob.rotation_euler = (foot - ob.location).to_track_quat("-Z", "Y").to_euler()
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
    """Fire in an opening: ragged tongues rooted where it burns and licking upward, never loose balls of light."""
    g = fx.rng

    def make(t, at, facing):
        d = (Z + Vector((g.uniform(-0.35, 0.35), g.uniform(-0.35, 0.35), 0.0))).normalized()
        R = g.uniform(0.3, 0.5) * size
        root = at + Vector((g.uniform(-0.35, 0.35), g.uniform(-0.35, 0.35), 0.0)) * size
        p = fx.puff(t, root + d * R * 0.8, vel=d * g.uniform(0.3, 0.9), life=g.uniform(0.22, 0.4), r=(R * 0.7, R * 1.15), col=(0.6, 0.24, 0.05), col2=(0.42, 0.1, 0.02), alpha=0.95,
                    kind="tongue" if g.random() < 0.4 else "lick", wind=0.15, fade_in=0.04, grow=0.5, squash=(1.0, 0.5, 0.5))
        p["ob"].rotation_euler = d.to_track_quat("X", "Z").to_euler()
        seen_only(p)
    fx.stream(where, 0.07, make, start=start, end=end)


# A shell's flame: deep orange, no blue in it and not too bright (the picture's tone curve bleaches a bright flame to
# peach or salmon whatever its colour), going to dull red as it dies.
FLAME, FLAME_OLD = (0.16, 0.042, 0.0), (0.1, 0.018, 0.0)


def shell_hit(fx, t0, at, size=1.0, earth=True, lamp=None, glare=450.0, flame=1.0, wall=None):
    """A shell bursting on a work or the ground: a wink of ragged flame, a fan of thrown earth and dust, grey-brown
    smoke. Give t0 a fiftieth of a second before a frame. `lamp` is where the burst's light stands from the burst
    (off a wall it has struck, or it burns the wall white), `glare` its strength for a burst of size 1, `flame` the size of its tongues of flame beside the burst's own.
    `wall` is the outward way of a wall it has struck: the flame is then a star spat along the wall's face and out from it."""
    g = fx.rng
    at = Vector(at)
    lamp = Vector(lamp) if lamp is not None else Z * 0.6

    def flash(t, p0, facing):
        # (a field gun's shell, not a battleship's: a wink of flame and a knot of dark smoke that thins in seconds.
        # The flame is a star of ragged tongues thrown up and out, gone before the next frame: a ball of light here
        # read as a cream balloon, and its light, held over two frames, lit the young smoke into bubbles)
        out = Vector(wall).normalized() if wall is not None else None
        along = Z.cross(out) if out is not None else None
        for i in range(8):
            a = g.uniform(0, 2 * math.pi)
            h = g.uniform(0.15, 0.8)         # (how far it leans from upright: never level, for one thrown at the camera is a ball)
            d = Vector((math.cos(a) * h, math.sin(a) * h, 1.0)).normalized()
            R = g.uniform(0.38, 0.7) * size * flame
            ox, oy = g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3)
            root = p0 + Z * 0.1 + Vector((ox, oy, 0.0)) * size      # (roots apart: heaped on one point their hot hearts make a ball)
            thin = 0.36
            if out is not None:
                # (on a wall the upright tongues stood side by side as two smooth-bottomed balloons: here they are
                # thin and spat all ways along the face, a little out from it. The same draws, so nothing after moves)
                fan = math.radians(-30.0 + 240.0 * a / (2 * math.pi))
                d = (out * (0.25 + 0.5 * h) + along * math.cos(fan) + Z * math.sin(fan)).normalized()
                root = p0 + out * 0.12 + along * ox * 1.3 * size + Z * oy * 0.8 * size
                thin = 0.12 + 0.08 * h
                R *= 0.9
            elif earth:
                # (in soft ground the flame is low and wide, spat out sideways under the earth it throws: standing
                # tall on the grass it read as a bonfire)
                w = 0.55 + 0.9 * h
                d = Vector((math.cos(a) * w, math.sin(a) * w, 0.8)).normalized()
                R *= 0.72 if i else 0.45         # (the first is the one with the white-hot heart: kept small)
                root = p0 + Z * 0.04 + Vector((ox, oy, 0.0)) * size * 1.3
                thin = 0.3 if i else 0.22
            p = fx.puff(t, root + d * R * 0.85, vel=d * 4.0, life=g.uniform(0.06, 0.085), r=(R * 0.8, R * 1.2), col=FLAME, col2=FLAME_OLD, alpha=1.0,
                        kind="tongue" if i < 1 else "lick", drag=6.0, wind=0.0, squash=(1.0, thin, thin), fade_in=0.01, grow=0.4)
            p["ob"].rotation_euler = d.to_track_quat("X", "Z").to_euler()
            seen_only(p)
        if out is not None:
            keep = g.getstate()              # (more scraps of flame, some alive into a second frame; drawn aside, so the smoke that follows is as it was)
            for i in range(14):
                fan = math.radians(g.uniform(-35.0, 215.0))
                d = (out * g.uniform(0.2, 0.7) + along * math.cos(fan) + Z * math.sin(fan)).normalized()
                R = g.uniform(0.16, 0.44) * size * flame
                root = p0 + out * 0.1 + along * g.uniform(-0.45, 0.45) * size * flame + Z * g.uniform(-0.3, 0.3) * size * flame
                thin = g.uniform(0.2, 0.38)
                p = fx.puff(t, root + d * R * 0.85, vel=d * 5.0, life=g.uniform(0.06, 0.15), r=(R * 0.8, R * 1.25), col=FLAME, col2=FLAME_OLD, alpha=1.0,
                            kind="lick", drag=6.0, wind=0.0, squash=(1.0, thin, thin), fade_in=0.01, grow=0.4)
                p["ob"].rotation_euler = d.to_track_quat("X", "Z").to_euler()
                seen_only(p)
            g.setstate(keep)
        if earth and out is None:
            # A shell in soft earth: a dark column taller than wide, ragged at the top, that leans with the wind and
            # tears into uneven puffs, darker low and paler as it climbs. (It was thirteen puffs born on one spot,
            # which hung as one soft round brown ball for a second before they spread.) The numbers the old knot
            # drew are drawn and thrown away, and the new ones come from a die of their own, so nothing after moves.
            for _ in range(13 * 11):
                g.random()
            keep = g.getstate()
            q = random.Random(1000 + int(round(t0 * 100)))
            # the first throw: black earth already up round the flame as it winks, half hiding it; it falls back
            # (many, small and thrown nearly straight up, so that they stand as one spiked fountain taller than wide:
            # sixteen bigger ones flung wide were a clump of dark balls)
            for i in range(30):
                a, w = q.uniform(0, 2 * math.pi), q.uniform(0.04, 0.5)
                d = Vector((math.cos(a) * w, math.sin(a) * w, 1.0)).normalized()
                start = p0 + Z * 0.08 + Vector((math.cos(a), math.sin(a), 0.0)) * q.uniform(0.0, 0.32) * size + d * q.uniform(0.1, 0.6) * size
                fx.puff(t, start, vel=d * q.uniform(5.0, 9.5) * size ** 0.5, life=q.uniform(0.5, 1.0), r=(q.uniform(0.08, 0.15) * size, q.uniform(0.26, 0.42) * size),
                        col=(0.035, 0.026, 0.018), col2=(0.15, 0.115, 0.08), alpha=q.uniform(0.75, 0.95), drag=2.4, gravity=7.0, fade_in=0.005, grow=0.6, wind=0.4, hold=0.35, cool=1.0)
            # the column: many small puffs thrown up at all speeds (f: 0 stays at the foot, 1 reaches the top), born
            # after the flame's lamp is out (lit young, they are cream bubbles)
            for i in range(50):
                f = q.random() if i < 44 else q.uniform(1.0, 1.12)          # (the last six: bumps on the top, never beads above it)
                v = Vector((q.uniform(-1, 1) * (0.5 + 0.7 * f), q.uniform(-1, 1) * (0.5 + 0.7 * f), 0.6 + 7.2 * f)) * size ** 0.5
                pale = min(1.0, f)
                c0, c1 = Vector((0.13, 0.1, 0.075)), Vector((0.3, 0.27, 0.24))
                thin = i >= 44
                fx.puff(t + q.uniform(0.09, 0.2), p0 + Z * 0.15 + Vector((q.uniform(-0.25, 0.25), q.uniform(-0.25, 0.25), 0.0)) * size, vel=v,
                        life=(q.uniform(1.9, 3.0) + 0.9 * pale) if not thin else q.uniform(1.0, 1.6),
                        r=(q.uniform(0.1, 0.18) * size, (q.uniform(0.38, 0.62) + 0.3 * pale) * size if not thin else q.uniform(0.3, 0.42) * size),
                        col=(0.07, 0.052, 0.038), col2=tuple(c0.lerp(c1, pale) * q.uniform(0.9, 1.1)), alpha=(0.7 - 0.32 * pale) * q.uniform(0.85, 1.1),
                        drag=2.8, rise=0.25 + 0.7 * pale, fade_in=0.03, grow=0.5, wind=0.5 + 0.9 * pale + q.uniform(-0.15, 0.25), cool=1.3)
            g.setstate(keep)
        else:
            for i in range(13):      # (many small puffs kept close make one ragged knot; a few big ones, or small ones flung apart, read as balls)
                v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0.5, 1.6))).normalized() * g.uniform(1.2, 4.6) * size ** 0.5
                c = g.uniform(0.08, 0.16)
                fx.puff(t + 0.04, p0 + Z * 0.3, vel=v, life=g.uniform(2.6, 4.4), r=(0.32 * size, g.uniform(0.75, 1.35) * size), col=(0.2, 0.15, 0.11), col2=(c, c * 0.97, c * 0.93),
                        alpha=g.uniform(0.45, 0.65), drag=2.8, rise=0.9, fade_in=0.04, grow=0.45, wind=0.9)
        # (its light is the near ground's for one frame: the whole field must not jump)
        fx.light(t, p0 + lamp, glare * size * size, 0.08, r=0.8)
    fx.once(t0, (at, Z), flash)

    def make(t, p0, facing):
        # the earth it throws: most of it a brown fountain that stands a moment and falls back as a veil; the clods
        # that fly clear are solid lumps (see Clods). Soft round puffs of it hung in the sky as beads with rims.
        for i in range(26):
            v = Vector((g.uniform(-1, 1) * 1.5, g.uniform(-1, 1) * 1.5, g.uniform(4.2, 8.2))) * size ** 0.5
            c = g.uniform(0.85, 1.15)
            # (born as the flame's lamp goes out: lit by it, this brown veil was a cream bubble in the flame's heart)
            fx.puff(t + 0.085, p0 + Z * 0.15 + Vector((g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), 0.0)) * size, vel=v, life=g.uniform(1.0, 1.7), r=(0.16 * size, g.uniform(0.34, 0.62) * size),
                    col=(0.25 * c, 0.185 * c, 0.12 * c), col2=(0.3 * c, 0.24 * c, 0.17 * c), alpha=g.uniform(0.3, 0.46), gravity=9.8, drag=0.9, fade_in=0.03, grow=0.5, wind=0.3)
    if earth:
        fx.once(t0, (at, Z), make)
    dust(fx, t0 + 0.05, at, size=0.9 * size, n=16, out=6.0, alpha=0.24)


class Clods:
    """The earth a ground burst throws clear: solid lumps of no shape in many sizes, most of them small and low, each
    on its own arc and drawn out along its flight while it flies (the eye sees a thrown clod as a streak)."""

    def __init__(self, t0, at, count=44, seed=1, size=1.0):
        g = random.Random(seed)
        self.t0, self.at, self.items = t0, Vector(at), []
        mat = kit.material("clod", (0.14, 0.1, 0.065), rough=1.0, spec=0.05)
        for i in range(count):
            r = (0.012 + 0.05 * g.random() ** 2.6) * size
            verts, faces = concrete.lump(g, r, g.uniform(0.5, 0.9), 7)
            me = bpy.data.meshes.new("clod")
            me.from_pydata([tuple(v) for v in verts], [], faces)
            me.materials.append(mat)
            ob = bpy.data.objects.new("clod", me)
            bpy.context.scene.collection.objects.link(ob)
            ob.rotation_mode = "QUATERNION"
            ob.scale = (0.0, 0.0, 0.0)
            a = g.uniform(0, 2 * math.pi)
            out = g.uniform(0.5, 3.2) * size ** 0.5
            up = (2.2 + 5.2 * g.random() ** 2.3) * size ** 0.5        # (few go high, and none hangs in the sky)
            self.items.append((ob, Vector((math.cos(a) * out, math.sin(a) * out, up)), r))

    def set(self, t):
        a = t - self.t0
        for ob, v, r in self.items:
            if a < 0.0:
                ob.scale = (0.0, 0.0, 0.0)
                continue
            down = (v.z + math.sqrt(v.z * v.z + 2 * 9.8 * 0.15)) / 9.8         # when it lands again
            if a >= down:
                ob.location = self.at + Vector((v.x * down, v.y * down, r * 0.3))
                ob.scale = (1.0, 1.0, 1.0)
                continue
            ob.location = self.at + Vector((v.x * a, v.y * a, 0.15 + v.z * a - 4.9 * a * a))
            now = Vector((v.x, v.y, v.z - 9.8 * a))
            ob.rotation_quaternion = now.to_track_quat("X", "Z")
            ob.scale = (1.0 + min(now.length, 9.0) * 0.16, 1.0, 1.0)


def _step(nt, v, lo, hi):
    n = nt.nodes.new("ShaderNodeMapRange")
    n.interpolation_type = "SMOOTHSTEP"
    n.inputs["From Min"].default_value, n.inputs["From Max"].default_value = lo, hi
    nt.links.new(v, n.inputs["Value"])
    return n.outputs["Result"]


def scorch(item, mid=(0.0, 1.05), half=(1.1, 0.85), deep=1.2):
    """Soot and cracks round a hole blown in a work's face that looks toward +x, laid in its concrete as the game lays
    a scorch mark: nothing shows until the values this returns are raised (soot, cracks: each 0 to 1). `mid` and `half`
    are the hole's middle and half-size (along the face, up) as the work stands at rest; the mark keeps to the wall
    further out than `deep`. It is drawn where each point stood at rest, so what falls carries its own away."""
    M = stage._math
    item.ob.add_rest_position_attribute = True
    knobs = []
    for mat in item.ob.data.materials:
        if not mat or "concrete" not in mat.name or not mat.use_nodes:
            continue
        nt = mat.node_tree
        bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf is None:
            continue
        at = nt.nodes.new("ShaderNodeAttribute")
        at.attribute_type, at.attribute_name = "GEOMETRY", "rest_position"
        xyz = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(at.outputs["Vector"], xyz.inputs[0])
        x, y, z = xyz.outputs[0], xyz.outputs[1], xyz.outputs[2]

        def oval(cy, cz, ry, rz):
            u, w = M(nt, "DIVIDE", M(nt, "SUBTRACT", y, cy), ry), M(nt, "DIVIDE", M(nt, "SUBTRACT", z, cz), rz)
            return M(nt, "SQRT", M(nt, "ADD", M(nt, "MULTIPLY", u, u), M(nt, "MULTIPLY", w, w)))
        rim = _step(nt, oval(mid[0], mid[1], half[0], half[1]), 0.8, 1.0)                # (nothing in the middle: that is the hole, and what fell out of it)
        wall = _step(nt, x, deep, deep + 0.3)
        soot_reach = M(nt, "SUBTRACT", 1.0, _step(nt, oval(mid[0], mid[1] + 0.45, half[0] * 1.7, half[1] * 1.95), 0.45, 1.0))        # (soot climbs: its patch stands higher than the hole)
        streak = nt.nodes.new("ShaderNodeTexNoise")
        streak.inputs["Scale"].default_value, streak.inputs["Detail"].default_value = 1.0, 5.0
        grain = nt.nodes.new("ShaderNodeMapping")
        grain.inputs["Scale"].default_value = (7.0, 7.0, 1.5)
        nt.links.new(at.outputs["Vector"], grain.inputs["Vector"])
        nt.links.new(grain.outputs["Vector"], streak.inputs["Vector"])
        uneven = M(nt, "ADD", 0.35, M(nt, "MULTIPLY", streak.outputs[0], 1.3), clamp=True)
        k_soot = nt.nodes.new("ShaderNodeValue")
        k_soot.outputs[0].default_value = 0.0
        soot = M(nt, "MULTIPLY", M(nt, "MULTIPLY", M(nt, "MULTIPLY", rim, wall), M(nt, "MULTIPLY", M(nt, "MULTIPLY", soot_reach, uneven), 1.7, clamp=True)), k_soot.outputs[0], clamp=True)
        # cracks: the edges of broken cells, widest at the hole and closing as they run out
        cells = nt.nodes.new("ShaderNodeTexVoronoi")
        cells.feature = "DISTANCE_TO_EDGE"
        cells.inputs["Scale"].default_value = 2.0
        warp = nt.nodes.new("ShaderNodeTexNoise")
        warp.inputs["Scale"].default_value = 1.7
        nt.links.new(at.outputs["Vector"], warp.inputs["Vector"])
        little = nt.nodes.new("ShaderNodeVectorMath")
        little.operation = "SCALE"
        little.inputs["Scale"].default_value = 0.2
        nt.links.new(warp.outputs["Color"], little.inputs[0])
        bent = nt.nodes.new("ShaderNodeVectorMath")
        bent.operation = "ADD"
        nt.links.new(at.outputs["Vector"], bent.inputs[0])
        nt.links.new(little.outputs[0], bent.inputs[1])
        nt.links.new(bent.outputs[0], cells.inputs["Vector"])
        crack_reach = M(nt, "SUBTRACT", 1.0, _step(nt, oval(mid[0], mid[1] + 0.1, half[0] * 1.65, half[1] * 1.7), 0.55, 1.0))
        width = M(nt, "MULTIPLY", crack_reach, 0.032)
        line = M(nt, "LESS_THAN", cells.outputs["Distance"], width)
        k_crack = nt.nodes.new("ShaderNodeValue")
        k_crack.outputs[0].default_value = 0.0
        cracks = M(nt, "MULTIPLY", M(nt, "MULTIPLY", line, M(nt, "MULTIPLY", rim, wall)), k_crack.outputs[0], clamp=True)
        dark = M(nt, "MAXIMUM", M(nt, "MULTIPLY", soot, 0.93), M(nt, "MULTIPLY", cracks, 0.72))
        mix = nt.nodes.new("ShaderNodeMixRGB")
        mix.inputs["Color2"].default_value = (0.018, 0.017, 0.016, 1.0)
        base = bsdf.inputs["Base Color"]
        if base.links:
            nt.links.new(base.links[0].from_socket, mix.inputs["Color1"])
        else:
            mix.inputs["Color1"].default_value = base.default_value
        nt.links.new(dark, mix.inputs["Fac"])
        nt.links.new(mix.outputs["Color"], base)
        knobs.append((k_soot, k_crack))

    def lay(soot, cracks):
        for a, b in knobs:
            a.outputs[0].default_value, b.outputs[0].default_value = soot, cracks
    return lay


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
        gun = s.put("FlakPit", nation="GER").mounted["gun"]
    else:
        gun = s.put("HeavyFlak", (0.0, 0.0, 0.0), nation="GER")
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
        light = s.put("SandbagRing", (30.0, 22.0), 0.0, nation="GER").mounted["gun"] if "SandbagRing" in have else s.put("LightFlak", (30.0, 22.0), 0.0, nation="GER")
    # (the worn ground is drawn fine about the middle of the line, not about the gun alone; a lorry track comes up
    # from the rear to the pit)
    s.lay(seed=3, flat=80.0, fine=45.0, centre=(22.0, 3.0), sun_turn=125.0, sun_elev=34.0, trees=True, grass_area=66.0, grass_count=260000,
          tracks=[[(-7.5, -1.0), (-16.0, -3.0), (-30.0, 4.0), (-60.0, 2.0)]])

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
        # (a line never stops in a cut: the run on either hand of the pillbox is closed by its end pieces)
        for y in (16.0, -16.0):
            s.put("FieldworksEnd_start", (-1.0, y - 10.0), 0.0)
            s.put("Fieldworks", (-1.0, y), 0.0)
            s.put("FieldworksEnd", (-1.0, y + 10.0), 0.0)
    s.lay(seed=5, flat=60.0, fine=33.0, sun_turn=120.0, sun_elev=36.0, trees=True, grass_area=44.0, grass_count=160000)
    hit = 1.98                                # (each burst a fiftieth before a frame, so the frame catches its flame young)
    near = [(0.48, (6.5, 1.5, 0.0)), (1.23, (5.0, -2.5, 0.0))]     # (both where the camera sees them fall)
    clods = []
    for k, (t0, at) in enumerate(near):
        shell_hit(fx, t0, at, size=1.0)
        clods.append(Clods(t0, at, seed=11 + k))
    mark = scorch(box)
    at_hit, out_hit = box.where("hit")
    shell_hit(fx, hit, at_hit, size=1.5, earth=False, lamp=(1.6, 0.0, 0.4), glare=180.0, flame=0.6, wall=out_hit)
    dust(fx, hit + 0.1, at_hit, size=1.5, n=16, col=(0.6, 0.6, 0.57), out=5.0, alpha=0.3)
    chimney(fx, lambda: (box.where("hit")[0] + Vector((-0.4, 0, 0.5)), Vector((1, 0, 0.3))), start=hit + 0.25, size=0.9)
    chimney(fx, lambda: (box.where("smoke")[0] + Vector((0, 0, 0.6)), Z), start=hit + 0.5, size=0.5)
    flames(fx, lambda: (box.where("fire")[0] + Vector((1.2, 0, 0.2)), Z), start=hit + 0.7, size=0.8)
    # (the fire within glows in the broken opening; strong enough to burn the walls white, it read as a lantern)
    fx.light(hit + 0.7, at_hit + Vector((-0.9, 0, 0.2)), 25.0, long=30.0, col=(1.0, 0.42, 0.14), r=0.5)
    glow, flick = fx.lights[-1], random.Random(9)
    murk = random.Random(4)

    def room_smoke(t, p0, facing):
        v = Vector((murk.uniform(-1, 1), murk.uniform(-1, 1), murk.uniform(0.0, 0.6))) * 0.14
        c = murk.uniform(0.03, 0.07)
        fx.puff(t, p0 + Vector((murk.uniform(-0.7, 0.7), murk.uniform(-0.8, 0.8), murk.uniform(-0.35, 0.35))), vel=v, life=murk.uniform(2.4, 3.4), r=(0.45, murk.uniform(0.55, 0.75)),
                col=(c, c, c), alpha=murk.uniform(0.8, 0.95), drag=0.5, rise=0.1, fade_in=0.1, grow=0.5, wind=0.0)
    # (thick at once: through a thin room the far embrasures showed the daylight beyond as pale lit panes)
    fx.stream((Vector((0.0, 0.0, 1.0)), Z), 0.08, room_smoke, start=hit + 0.06, end=7.5)
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
        for c in clods:
            c.set(t)
        # the scorch comes with the blow (cracks at once, the blast's own black), and the smoke goes on sooting the wall
        mark(0.0 if t < hit else min(1.0, 0.5 + 0.5 * (t - hit) / 2.5), 0.0 if t < hit else 1.0)
        glow["power"] = 25.0 * flick.uniform(0.55, 1.3)          # (a fire's light is never steady)
        fx.at(t)

    # the machine gun's flashes while it still fires: a small ragged tongue at the muzzle, alive for less than a frame,
    # so the burst flickers (a ball of light here read as a pale egg in the embrasure)
    g = random.Random(2)

    def mg_flash(t, at, facing):
        R = g.uniform(0.11, 0.17)
        p = fx.puff(t, at + facing * (0.06 + R * 0.9), vel=facing * 2.0, life=g.uniform(0.05, 0.065), r=(R * 0.8, R * 1.15), col=(0.62, 0.27, 0.06), col2=(0.5, 0.14, 0.03), alpha=1.0,
                    kind="tongue", drag=6.0, wind=0.0, squash=(1.0, 0.42, 0.42), fade_in=0.008, grow=0.4)
        p["ob"].rotation_euler = facing.to_track_quat("X", "Z").to_euler()
        seen_only(p)

    k = 0
    while k * 0.1 < mg_until - 0.25:
        tt = k * 0.1
        if (tt % 1.1) < 0.65:
            fx.once(tt - 0.02, lambda: box.where("muzzle"), mg_flash)
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
