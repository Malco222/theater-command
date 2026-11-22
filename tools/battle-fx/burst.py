"""Battle effects: a shell, mortar bomb, grenade or bomb landing on ground, laid by the recipe in fx.json.

    import burst
    F = burst.Field(height=stage.height, wind=(1.6, 0.4, 0.0))
    burst.ground_burst(F, t0=1.5, at=(x, y), size="how105", ground="earth", seed=3, cam=camera)
    ...for every frame: F.at(t); cam offset = F.shake(t, cam_location)

The look, layer by layer (the art director's sheet, explosions-look.md): a one-frame flash that lights the ground
round it; a knee-high ring of dust skidding out; a spiky crown of dark earth thrown up in jets with clods at their
tips; clods falling on true gravity, the big ones bouncing once; a charcoal-brown cloud that climbs, leans downwind
and lingers; and a crater that stays. Smoke and dust are volumes (vol.py): soft, self-shaded, glowing when backlit.
Flame is the land team's ragged tongues, dim and deep orange (stage.py Effects, works/film.py).
"""
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
for p in (os.path.join(HERE, "..", "battle-models"), os.path.join(HERE, "..", "battle-models", "works"), HERE):
    if p not in sys.path:
        sys.path.insert(0, p)

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import kit  # noqa: E402
import stage as _stage  # noqa: E402
import concrete  # noqa: E402
import vol  # noqa: E402

RECIPE = json.load(open(os.path.join(HERE, "fx.json")))
Z = Vector((0.0, 0.0, 1.0))

# A flame's own colour as the renders need it: dim and deep orange, or the AgX tone curve bleaches it to peach
# (the fortifications thread's finding, 18 Nov). It measures about 207, 117, 66 in the finished picture.
FLAME, FLAME_OLD = (0.16, 0.042, 0.0), (0.1, 0.018, 0.0)

# What each ground throws, as albedo (linear) for the renders. `soil` is the wet earth from underneath (about 0.55 of
# the ground's brightness), `dust` the dry stuff that skids and hangs, `smoke` the cloud's colour once the soil in it
# has settled out, `clod` the lumps' paint. The recipe's grounds table says which of these show and how much.
STUFF = {
    "earth": dict(soil=(0.055, 0.042, 0.03), dust=(0.30, 0.26, 0.20), smoke=(0.075, 0.064, 0.054), clod=(0.075, 0.056, 0.038)),
    "sand": dict(soil=(0.36, 0.29, 0.19), dust=(0.52, 0.43, 0.30), smoke=(0.40, 0.34, 0.25), clod=(0.30, 0.24, 0.16), heavy=(0.1, 0.075, 0.05)),
    "snow": dict(soil=(0.045, 0.036, 0.028), dust=(0.80, 0.82, 0.85), smoke=(0.06, 0.055, 0.05), clod=(0.05, 0.04, 0.03), first=(0.85, 0.87, 0.9)),
    "mud": dict(soil=(0.035, 0.027, 0.02), dust=(0.16, 0.13, 0.10), smoke=(0.06, 0.052, 0.044), clod=(0.04, 0.03, 0.022)),
    "town": dict(soil=(0.30, 0.29, 0.27), dust=(0.48, 0.46, 0.42), smoke=(0.30, 0.29, 0.27), clod=(0.36, 0.34, 0.31)),
    "woods": dict(soil=(0.05, 0.04, 0.03), dust=(0.24, 0.21, 0.16), smoke=(0.07, 0.062, 0.052), clod=(0.07, 0.052, 0.036), splinter=(0.55, 0.45, 0.3)),
}


class Field:
    """Everything the effects need, stepped together: puffs (surface and volume), clods, lights, craters and the
    camera's jolts. `height(x, y)` is the ground's height (battle_set.Stage.height)."""

    def __init__(self, height=None, seed=7, wind=(1.6, 0.4, 0.0), volumes=True):
        self.fx = _stage.Effects(seed=seed, wind=wind)
        self.vol = vol.Volumes(self.fx) if volumes else None
        self.height = height or (lambda x, y: 0.0)
        self.movers = []          # things with set(t): clods, craters
        self.jolts = []           # (t0, strength, push) for the camera
        self.rng = random.Random(seed)
        self._mats = {}

    def ground(self, x, y):
        return Vector((x, y, self.height(x, y)))

    def puff(self, t0, at, kind="smoke", **kw):
        """Volume kinds (smoke, dust, earth) as volumes when the field has them; the rest as the land team's puffs."""
        if self.vol is not None and kind in vol.Volumes.KINDS:
            return self.vol.puff(t0, at, kind=kind, **kw)
        if kind in vol.Volumes.KINDS:
            kind = "smoke"
        return self.fx.puff(t0, at, kind=kind, **kw)

    def material(self, name, rgb):
        if name not in self._mats:
            self._mats[name] = kit.material("fx_" + name, rgb, rough=1.0, spec=0.05)
        return self._mats[name]

    def at(self, t):
        self.fx.at(t)
        for m in self.movers:
            m.set(t)

    def shake(self, t, cam_at, seed=0):
        """The camera's offset at t (a Vector) and its roll in degrees, from every jolt so far. A jolt is mostly up
        and down, decays fast and is gone in half a second; the roll is under half a degree."""
        c = RECIPE["camera"]["shake"]
        off, roll = Vector((0.0, 0.0, 0.0)), 0.0
        for k, (t0, s, push) in enumerate(self.jolts):
            a = t - t0
            if a < 0.0 or a > 0.6:
                continue
            env = math.exp(-a / 0.11) * min(1.0, a / 0.012)
            w = 2 * math.pi * 11.0
            off += Vector((math.sin(w * a * 0.83 + k) * c["side_m_at_full"], math.sin(w * a * 0.71 + 2 * k) * c["side_m_at_full"] * 0.6,
                           math.sin(w * a + 0.6 + k) * c["up_down_m_at_full"])) * s * env
            roll += math.sin(w * a * 0.9 + k) * c["roll_deg_max"] * s * env
            if push is not None:
                off += push * (1.0 - math.exp(-a / 0.05)) * math.exp(-a / 0.4)
        return off, max(-c["roll_deg_max"], min(c["roll_deg_max"], roll))


class Clods:
    """Thrown earth that flies clear as solid lumps: each on its own arc under true gravity, drawn out along its flight
    (the eye sees a thrown clod as a streak); the big ones bounce once and each lands with a small puff."""

    def __init__(self, F, t0, at, launches, rgb, seed=1, ground="earth"):
        g = random.Random(seed)
        self.F, self.t0, self.at, self.items = F, t0, Vector(at), []
        mat = F.material("clod_" + ground, rgb)
        for vel, r in launches:
            verts, faces = concrete.lump(g, r, g.uniform(0.5, 0.9), 7)
            me = bpy.data.meshes.new("clod")
            me.from_pydata([tuple(v) for v in verts], [], faces)
            me.materials.append(mat)
            ob = bpy.data.objects.new("clod", me)
            F.fx.coll.objects.link(ob)
            ob.rotation_mode = "QUATERNION"
            ob.scale = (0.0, 0.0, 0.0)
            ob.hide_render = True
            path = self._path(Vector(vel), r)
            self.items.append((ob, path, r))
            land = path[0]
            if r > 0.03 * 1.0:
                # a puff where it lands: its own earth knocked up, low and brief
                p = path[0][1]
                F.fx.once(t0 + land[0], (p, Z), lambda t, p0, f, r=r: F.puff(t, p0 + Z * 0.1, kind="dust", vel=Z * 0.4, life=g.uniform(1.4, 2.4),
                                                                            r=(r * 2.0, r * 6.0), col=STUFF[ground]["dust"], alpha=0.35, drag=2.0, rise=0.15,
                                                                            fade_in=0.04, grow=0.5, wind=0.6, squash=(1.0, 1.0, 0.6)))

    def _path(self, v, r):
        """Where it lands (and, for a big clod, where it lands again after its one bounce): [(t, at, v_after)]."""
        F, p = self.F, Vector(self.at)
        legs = []
        for bounce in range(2 if r > 0.05 else 1):
            ground_z = F.height(p.x, p.y)
            for _ in range(3):           # (the ground's height where it comes down, found by trying)
                dz = p.z - ground_z
                tl = (v.z + math.sqrt(max(0.0, v.z * v.z + 2 * 9.8 * dz))) / 9.8
                q = p + Vector((v.x, v.y, 0.0)) * tl
                ground_z = F.height(q.x, q.y)
            q = Vector((p.x + v.x * tl, p.y + v.y * tl, ground_z + r * 0.3))
            vin = Vector((v.x, v.y, v.z - 9.8 * tl))
            vout = Vector((vin.x * 0.35, vin.y * 0.35, -vin.z * 0.28))
            start = legs[-1][0] if legs else 0.0
            legs.append((start + tl, q, p.copy(), v.copy(), start))
            p, v = q, vout
            if v.z < 0.8:
                break
        return legs

    def set(self, t):
        a = t - self.t0
        for ob, legs, r in self.items:
            if a < 0.0:
                ob.hide_render = True
                ob.scale = (0.0, 0.0, 0.0)
                continue
            ob.hide_render = False
            leg = next((L for L in legs if a < L[0]), None)
            if leg is None:
                ob.location = legs[-1][1]
                ob.scale = (1.0, 1.0, 1.0)
                continue
            tl, q, p0, v0, start = leg
            s = a - start
            ob.location = p0 + Vector((v0.x * s, v0.y * s, v0.z * s - 4.9 * s * s))
            now = Vector((v0.x, v0.y, v0.z - 9.8 * s))
            if now.length > 1e-3:
                ob.rotation_quaternion = now.to_track_quat("X", "Z")
            ob.scale = (1.0 + min(now.length, 14.0) * 0.12, 1.0, 1.0)


class Crater:
    """A fresh shell hole that appears under the crown and stays for the whole battle (battle_set's crater, darker
    and scorched while fresh)."""

    def __init__(self, F, t0, at, across, seed=0):
        import battle_set
        self.t0 = t0
        r = max(0.2, across / 2.0)
        self.ob = battle_set._crater(_StageLike(F), at[0], at[1], r, None, seed)
        self.ob.hide_render = True
        self.ob.color = (0.55, 0.5, 0.45, 1.0)

    def set(self, t):
        self.ob.hide_render = t < self.t0


class _StageLike:
    def __init__(self, F):
        self.height = F.height


def _fan(g, n_heavy, n_light, C):
    """The crown's jets as (direction, heavy, reach as a share of the crown's height). A wide fan: the heavy jets spread
    round the compass, leaning from near upright out to half the fan's angle, one of them near upright and the tallest;
    the light ones fill between at any lean and shorter."""
    half = C["fan_deg"] / 2.0
    out = []
    a0 = g.uniform(0, 2 * math.pi)
    for i in range(n_heavy):
        a = a0 + 2 * math.pi * i / n_heavy + g.uniform(-0.35, 0.35)
        lean = g.uniform(2.0, 12.0) if i == 0 else g.uniform(*C["heavy_lean"]) * half
        reach = 1.0 if i == 0 else g.uniform(*C["heavy_reach"])
        out.append((_dir(a, lean), True, reach))
    for i in range(n_light):
        a = a0 + 2 * math.pi * (i + 0.5) / n_light + g.uniform(-0.4, 0.4)
        out.append((_dir(a, g.uniform(0.0, 1.0) * half), False, g.uniform(*C["fill_reach"])))
    return out


def _dir(a, lean_deg):
    lean = math.radians(lean_deg)
    return Vector((math.cos(a) * math.sin(lean), math.sin(a) * math.sin(lean), math.cos(lean)))


def ground_burst(F, t0, at, size="how105", ground="earth", seed=1, cam=None, flash=True, light_scale=1.0):
    """A shell or bomb landing at `at` (x, y on the ground, or x, y, z) at t0. Give t0 a fiftieth of a second before a
    frame, so the frame catches its flame young. `cam` (the camera object) lets a close hit throw earth past the lens."""
    R = RECIPE
    S, G = R["sizes"][size], R["grounds"][ground]
    st = STUFF[ground]
    g = random.Random(seed * 7919 + 17)
    s = S["scale"]
    k = s ** 0.5                                   # (speeds and radii grow with the root of the size)
    at = Vector(at) if len(at) == 3 else F.ground(at[0], at[1])
    crown_h = g.uniform(*S["crown_m"]) * (G.get("crown_slow", 1.0) ** 0.0)
    slow = G.get("crown_slow", 1.0)

    # -- the flash: a small low star of ragged flame under one frame, and a real light for about one frame
    def wink(t, p0, facing):
        for i in range(7):
            a = g.uniform(0, 2 * math.pi)
            h = g.uniform(0.15, 0.8)
            w = 0.55 + 0.9 * h
            d = Vector((math.cos(a) * w, math.sin(a) * w, 0.8)).normalized()
            Rr = g.uniform(0.38, 0.7) * k * (0.72 if i else 0.45) * min(1.0, 1.2 / max(k, 1e-3) ** 0.3)
            root = p0 + Z * 0.04 + Vector((g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), 0.0)) * k * 1.3
            p = F.fx.puff(t, root + d * Rr * 0.85, vel=d * 4.0, life=g.uniform(0.028, 0.04), r=(Rr * 0.8, Rr * 1.2), col=FLAME, col2=FLAME_OLD, alpha=1.0,
                          kind="tongue" if i < 1 else "lick", drag=6.0, wind=0.0, squash=(1.0, 0.3, 0.3), fade_in=0.005, grow=0.4)
            p["ob"].rotation_euler = d.to_track_quat("X", "Z").to_euler()
            p["ob"].visible_diffuse = p["ob"].visible_glossy = False
        # the glow left inside the crown's foot for 3 to 5 frames, half hidden by the earth going up round it
        for i in range(5):
            d = (Z + Vector((g.uniform(-0.5, 0.5), g.uniform(-0.5, 0.5), 0.0))).normalized()
            Rr = g.uniform(0.25, 0.45) * k
            p = F.fx.puff(t + 0.03, p0 + Z * 0.25 * k + d * Rr * 0.5, vel=d * 1.5, life=g.uniform(0.1, 0.16), r=(Rr * 0.8, Rr * 1.1), col=FLAME_OLD,
                          col2=(0.05, 0.01, 0.0), alpha=0.85, kind="lick", drag=4.0, wind=0.0, squash=(1.0, 0.45, 0.45), fade_in=0.01, grow=0.5)
            p["ob"].rotation_euler = d.to_track_quat("X", "Z").to_euler()
            p["ob"].visible_diffuse = p["ob"].visible_glossy = False
        if flash:
            # (lifts what is within about 30 m by 1.1 to 1.3 for a frame: stood a little up so the grass at its foot is
            # not burnt white, and out by the third frame)
            F.fx.light(t, p0 + Z * (3.5 * k), 900.0 * s * light_scale, 0.085, col=(1.0, 0.77, 0.55), r=0.6)
    F.fx.once(t0, (at, Z), wink)

    # -- the shock: one low skirt of the ground's own dust, knee-high, racing out and thinning. Neighbours overlap by
    #    about half and each puff is stretched along the ring, so it reads as one skirt, never separate pale dabs.
    SR = R["burst"]["shock_ring"]
    sk = SR["skirt"]
    reach = SR["reach_m"] * k
    ring_col = tuple(Vector(st["dust"]) * SR["colour_from_ground"])

    def ring(t, p0, facing):
        n_out, n_in = int(sk["puffs_outer"] * max(0.7, k ** 0.5)), int(sk["puffs_inner"] * max(0.7, k ** 0.5))
        for i in range(n_out + n_in):
            # (the front edge thickest; the rest spread from it back toward the hole, one skirt, never two rings)
            speed = 1.0 if i < n_out else g.uniform(0.25, 0.95) ** 0.6
            a = 2 * math.pi * (i + g.uniform(-0.4, 0.4)) / (n_out if i < n_out else n_in)
            out = Vector((math.cos(a), math.sin(a), 0.0))
            drag = sk["drag"] * g.uniform(0.85, 1.15)
            v = out * reach * speed * drag * g.uniform(0.85, 1.0) + Z * g.uniform(0.1, 0.4) * k ** 0.5
            r1 = sk["end_radius_m"] * k * g.uniform(0.85, 1.2) * (0.85 if speed < 1.0 else 1.0)
            q = F.puff(t + g.uniform(0.0, 0.02), p0 + out * g.uniform(0.4, 1.0) * k - Z * sk["sunk_m"] * k, kind="dust", vel=v,
                       life=g.uniform(*sk["life_s"]) * min(2.0, k), r=(sk["start_radius_m"] * k, r1), col=tuple(Vector(ring_col) * g.uniform(0.9, 1.08)),
                       alpha=g.uniform(*sk["alpha"]) * G.get("dust", 1.0) ** 0.4, drag=drag, rise=0.1, fade_in=0.02, grow=sk["grow"], wind=0.6,
                       squash=(sk["stretch_along_ring"], 1.0, sk["flatten"]), hold=0.1)
            q["ob"].rotation_euler = (0.0, 0.0, a + math.pi / 2)      # (long along the ring; turned only about the upright)
    F.fx.once(t0 + 0.01, (at, Z), ring)

    # -- the crown: a wide fan of a few heavy dark jets of uneven length, each ending in a clod, with lighter dustier
    #    jets between them; thickness is a share of the crown's height, and a jet has as many puffs as its length needs
    C = R["burst"]["crown"]
    tips = []
    first = st.get("first")
    heavy_col = st.get("heavy", st["soil"])
    tilt = Vector((g.uniform(-1, 1), g.uniform(-1, 1), 0.0)).normalized() * math.tan(math.radians(g.uniform(*C["tilt_deg"])))
    jets = _fan(g, g.randint(*C["heavy_jets"]), g.randint(*C["fill_jets"]), C)

    def crown(t, p0, facing):
        KD = C["drag"]
        for d, heavy, reachf in jets:
            d = (d + tilt).normalized()
            reachj = crown_h * reachf
            V = reachj * KD * 1.12 / slow ** 0.5
            thick = g.uniform(*C["heavy_thick"]) if heavy else g.uniform(*C["fill_thick"])
            rf, rt = C["jet_foot_r"] * crown_h * thick, C["jet_tip_r"] * crown_h * thick
            shade = g.uniform(*C["heavy_shade"]) if heavy else g.uniform(0.9, 1.3)
            # (puffs stepped along the jet by their own length, overlapping by about half, so a jet of any size stays
            # one ragged streak: spaced evenly, a bomb's thin tip puffs stood apart as beads)
            fs, f = [], g.uniform(0.0, 0.05)
            while f < 1.0:
                fs.append(f)
                f += (rt + (rf - rt) * (1.0 - f) ** 0.8) * C["stretch_along_jet"] * C["overlap"] / reachj * g.uniform(0.7, 1.2)
            for f in fs:
                sp = V * (0.05 + 0.95 * f ** 0.9)
                rl = rt + (rf - rt) * (1.0 - f) ** 0.8
                rr = rl * (g.uniform(1.4, 2.0) if g.random() < C["lump_chance"] else g.uniform(0.8, 1.1))
                if heavy:
                    col = Vector(heavy_col) * shade
                else:
                    col = Vector(st["soil"]).lerp(Vector(st["dust"]), g.uniform(0.25, 0.6)) * shade
                if first is not None and f > 0.45:        # (snow: white snow thrown up first, the earth under it)
                    col = Vector(first)
                # (past tip_breakup the jet breaks into ragged clumps flying a little apart: never a smooth sausage)
                brk = 0.4 if f < C["tip_breakup"] else 1.1
                side = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-1, 1))) * rl * brk * KD
                p = F.puff(t + 0.01 + 0.025 * (1 - f), p0 + Z * 0.1 + d * 0.25 * k, kind="earth", vel=d * sp + side, life=g.uniform(1.2, 2.0) * slow,
                           r=(rr, rr * g.uniform(1.8, 2.4)), col=tuple(col), col2=st["dust"] if first is None else tuple(col),
                           alpha=1.0 if heavy else (0.55 - 0.2 * f), drag=KD, gravity=9.8 / slow, fade_in=0.01, grow=0.5, wind=0.35,
                           squash=(C["stretch_along_jet"], 0.85, 0.85), hold=0.4, cool=0.7)
                p["ob"].rotation_euler = d.to_track_quat("X", "Z").to_euler()
            if heavy:
                for c in range(g.randint(*C["tip_clods"])):
                    dd = (d + Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-1, 1))) * 0.08).normalized()
                    tips.append((dd * math.sqrt(2 * 9.8 * reachj) * g.uniform(0.9, 1.1), g.uniform(*C["tip_clod_m"]) * k))
    F.fx.once(t0 + 0.005, (at, Z), crown)

    # -- the body: a dense dark mound of earth and dust at the crown's foot from frame 4, which slumps into the smoke
    B = C["body"]

    def body(t, p0, facing):
        KD = 5.0
        for i in range(int(B["puffs"] * max(0.7, k ** 0.5))):
            a = g.uniform(0, 2 * math.pi)
            rad = crown_h * B["width"] * g.random() ** 0.6
            hgt = crown_h * B["height"] * g.random() ** 1.3
            to = Vector((math.cos(a) * rad, math.sin(a) * rad, hgt)) + tilt * hgt
            rr = crown_h * g.uniform(*B["puff_r"])
            col = Vector(heavy_col).lerp(Vector(st["dust"]), g.uniform(0.0, 0.35)) * g.uniform(*B["shade"])
            q = F.puff(t + g.uniform(0.03, B["from_frame"] / 30.0), p0 + Z * 0.15 * k, kind="earth", vel=to * KD, life=g.uniform(1.4, 2.4) * slow,
                       r=(rr * 0.5, rr), col=tuple(col), col2=st["smoke"], alpha=g.uniform(*B["alpha"]), drag=KD, gravity=2.5 / slow,
                       fade_in=0.03, grow=0.35, wind=0.4, squash=(1.0, 1.0, 0.85), hold=0.45, cool=1.2)
            q["ob"].rotation_euler = (0.0, 0.0, g.uniform(0, 2 * math.pi))
    F.fx.once(t0 + 0.01, (at, Z), body)

    # -- the veil: fine earth that stands a moment over the crown's foot and falls back
    def veil(t, p0, facing):
        for i in range(int(30 * max(0.7, k))):
            v = Vector((g.uniform(-1, 1) * 1.6, g.uniform(-1, 1) * 1.6, g.uniform(4.0, 8.5))) * k / slow ** 0.5
            F.puff(t + g.uniform(0.06, 0.14), p0 + Z * 0.2 + Vector((g.uniform(-0.4, 0.4), g.uniform(-0.4, 0.4), 0.0)) * k, kind="dust", vel=v,
                   life=g.uniform(1.4, 2.4) * slow, r=(0.25 * k, g.uniform(0.7, 1.2) * k), col=st["dust"], alpha=g.uniform(0.25, 0.4) * G.get("dust", 1.0) ** 0.5,
                   gravity=6.0 / slow, drag=1.2, fade_in=0.05, grow=0.5, wind=0.5, hold=0.3)
    F.fx.once(t0, (at, Z), veil)

    # -- the smoke: the crown collapses into a rolling charcoal-brown cloud that climbs, leans downwind and stays
    linger = g.uniform(*S["linger_s"]) * G.get("haze_lingers", 1.0)
    n_smoke = int(130 * max(0.4, s) ** 0.85)

    def smoke(t, p0, facing):
        # many small puffs born up the crown's height as it falls in, torn apart by their own speeds: a few big
        # ones made one smooth egg (strip 1)
        for i in range(n_smoke):
            f = g.random() ** 0.9                           # (0 at the foot, 1 at the top of the crown)
            born = t + 0.1 + 0.5 * g.random() * (0.4 + f)
            hgt = crown_h * (0.05 + 0.8 * f)
            spread = (0.5 + 2.2 * f) * k
            pos = p0 + tilt * hgt * 0.6 + Vector((g.gauss(0, 1) * spread * 0.6, g.gauss(0, 1) * spread * 0.6, hgt * 0.45))
            v = Vector((g.gauss(0, 1), g.gauss(0, 1), 0.0)) * 1.2 * k + Z * (hgt * g.uniform(0.6, 1.5))
            c = g.uniform(0.75, 1.25)
            base = Vector(st["smoke"]) * c if g.random() > 0.25 else Vector(st["dust"]) * 0.55 * c
            warm = Vector((0.11, 0.05, 0.02)) if (f < 0.25 and born - t < 0.22) else base
            F.puff(born, pos, kind="smoke", vel=v, life=linger * g.uniform(0.45, 1.0), r=(g.uniform(0.3, 0.6) * k, g.uniform(1.0, 2.0) * k * (1.0 + 0.5 * f)),
                   col=tuple(warm), col2=tuple(base), alpha=g.uniform(0.3, 0.6), drag=g.uniform(1.4, 2.2), rise=g.uniform(0.25, 0.9) * k ** 0.5,
                   fade_in=0.08, grow=0.45, wind=g.uniform(0.5, 1.0), hold=0.15, cool=linger / 0.2)
    F.fx.once(t0, (at, Z), smoke)

    # -- the clods: one at each jet's tip, and more thrown clear low and wide
    launches = []
    n_more = int(G.get("clods", 1.0) * 50 * max(0.5, s))
    for i in range(n_more):
        a = g.uniform(0, 2 * math.pi)
        out = g.uniform(1.0, 6.0) * k
        up = (2.0 + 9.0 * g.random() ** 2.0) * k
        launches.append((Vector((math.cos(a) * out, math.sin(a) * out, up)), (0.03 + 0.15 * g.random() ** 2.2) * k))
    if cam is not None:
        to_cam = Vector(cam.location) - at
        if to_cam.length < R["burst"]["debris"]["lens"]["within_m"] * max(1.0, k):
            # (a close hit throws a few lumps past the lens: what puts the player in the hole)
            for i in range(g.randint(*R["burst"]["debris"]["lens"]["pieces"])):
                T = g.uniform(0.7, 1.4)
                miss = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-0.6, 0.4))).normalized() * g.uniform(1.2, 3.0)
                aim = Vector(cam.location) + miss - at
                v = (aim + Vector((0.0, 0.0, 4.9 * T * T))) / T
                launches.append((v, g.uniform(0.05, 0.12)))
    Clods_ = Clods(F, t0 + 0.02, at + Z * 0.15, [(v, r) for v, r in launches] + [(v, r) for v, r in tips if G.get("clods", 1.0) > 0.05],
                   st["clod"], seed=seed, ground=ground)
    F.movers.append(Clods_)

    # -- the scar
    if S["crater_m"] > 0.0 and ground not in ("water",):
        F.movers.append(Crater(F, t0 + 0.25, (at.x, at.y), S["crater_m"], seed))

    # -- the camera's jolt
    if cam is not None:
        dist = (Vector(cam.location) - at).length
        within = S["shake_within_m"]
        if dist < within:
            strength = min(1.0, s) * (1.0 - dist / within) ** 2
            push = None
            if S.get("push"):
                push = (Vector(cam.location) - at).normalized() * R["camera"]["shake"]["push_m"].get(size, 0.4) * (1.0 - dist / within)
            F.jolts.append((t0, strength, push))
    return dict(at=at, crown=crown_h, linger=linger)
