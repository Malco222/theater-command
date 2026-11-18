"""Films of a ship for the reviews.

    python show.py <name> <shot> <out folder> [frames=a,b,c] [fps=12] [size=960x540] [samples=16]

shots:  turn        a turn round her under way: wake, funnel smoke, her turrets training out
        broadside   she fires her main guns to starboard and is straddled
        sink        a magazine hit: she breaks in two and goes down (shown faster than life)
        capsize     she is hit, lists, rolls over and goes down (shown faster than life)

With frames= only those frames are rendered (numbered from 1), for a quick look. The clip is written beside the
frames as <name>-<shot>.mp4 and .gif.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Quaternion, Vector  # noqa: E402

import deckpark  # noqa: E402
import kit  # noqa: E402
import rigid  # noqa: E402
import ships  # noqa: E402
import stage  # noqa: E402
from rigid import smooth  # noqa: E402

ONE = (Quaternion(), Vector(), Vector((1, 1, 1)))


def fade(pose, w):
    """A pose at part strength (w from 0, at rest, to 1)."""
    return {k: (Quaternion().slerp(q, w), mv * w, sc) for k, (q, mv, sc) in pose.items()}


class Show:
    def __init__(self, name, tmp, speed=12.0, wind=None, month=None, **sea):
        kit.fresh()
        self.m, self.yard = ships.BUILDERS[name](True)
        ships.in_month(self.m, month)                               # (her decks as that month had them)
        self.ob, self.arm = rigid.to_blender(self.m, stage.ship_look)
        self.park = deckpark.range_on(self.m, self.arm, month)      # (a carrier's aircraft, ranged on her deck)
        h = self.yard.h
        self.L, self.B = h.L, h.B
        self.sea = stage.Sea(speed=speed, **sea)
        bow, stern = h.frame_x(1.0), h.frame_x(-1.0)
        self.wake = self.sea.wake(h.L, h.B, tmp, half_of=lambda x: h.half_water(h.u_of(x)) if stern < x < bow else 0.0, bow=bow, stern=stern)
        self.fx = stage.Effects(wind=wind or (-speed - 1.0, 1.5, 0.0))
        self.pose = {}
        self.cam = kit.camera("cam", 35.0)
        self.cam.data.clip_end = 40000.0
        self.clip = self.m.clips

    def where(self, socket):
        return lambda: rigid.place(self.m, self.pose, socket)

    def set(self, *poses):
        self.pose = rigid.merge(*poses)
        rigid.apply_pose(self.arm, self.pose, model=self.m)

    def play(self, clip, t):
        return rigid.pose_of(self.m, self.clip[clip], max(t, 0.0)) if t >= 0 else {}

    def funnels(self, **kw):
        for s in self.m.sockets:
            if s["name"].startswith("funnel."):
                self.fx.funnel_smoke(self.where(s["name"]), **kw)

    def spray(self, **kw):
        self.fx.bow_spray(self.where("wake.bow"), **kw)

    def preroll(self, secs=16.0):
        t = -secs
        while t < 0:
            self.fx.at(t)
            t += 0.25

    def train(self, bearing, lift, how=1.0, which=("turret.", "director.", "sec.S")):
        """Her guns laid on a bearing (0 ahead, 90 to starboard), lifted `lift` degrees; `how` from 0 (at rest) to 1."""
        out = {}
        for p in self.m.parts:
            if not p.name.startswith(which) or p.kind != "turn":
                continue
            if ".gun." in p.name:
                out.update(rigid.aim(self.m, p.name, lift * how))
                continue
            rest = getattr(p, "rest", 0.0)
            turn = ((bearing - rest + 180.0) % 360.0) - 180.0
            if p.limits:
                turn = min(max(turn, p.limits[0]), p.limits[1])
            out.update(rigid.aim(self.m, p.name, turn * how))
        return out

    def fire(self, turret, t0):
        """Her effects for one turret's salvo (the guns' run-back is the model's own clip)."""
        clip = self.clip["fire." + turret.split(".")[1]]
        first = True
        for e in clip["events"]:
            if e["fx"].startswith("muzzle_flash"):
                # (one light for the turret's salvo, at its first gun: a light for every gun held the glare four frames)
                self.fx.gun(t0 + e["t"], self.where(e["socket"]), bore=self.m.info.get("bore", 0.38), light=first)
                first = False


def turn(S, out, opt):
    T = 8.0
    S.funnels()
    S.spray()
    S.preroll()
    L = S.L

    def step(t):
        a = smooth((t - 0.8) / 4.5)
        S.set(S.play("sail", t), S.train(60.0, 15.0, a))
        S.sea.at(t)
        S.fx.at(t)
        ang = math.radians(-62.0 + 360.0 * t / T)
        kit.look_at(S.cam, (math.cos(ang) * L * 1.18 - 0.1 * L, math.sin(ang) * L * 1.18, L * 0.22), (-0.1 * L, 0, 9.0))
    return T, step


def broadside(S, out, opt):
    T = 7.0
    S.funnels()
    S.spray()
    L = S.L
    mains = [p.name for p in S.m.parts if p.name.startswith("turret.") and ".gun." not in p.name]
    t_fire = {tn: 1.02 + (0.0 if S.m.parts[S.m.part_at[tn]].pivot.x > 0 else 0.3) for tn in mains}     # (just off a frame's instant, so a flash is seen young)
    for tn, t0 in t_fire.items():
        S.fire(tn, t0)
    S.fx.splash(3.5, (62.0, -46.0, 0.0), high=58.0)
    S.fx.splash(3.75, (-24.0, -74.0, 0.0), high=52.0)
    S.fx.splash(3.65, (-46.0, 96.0, 0.0), high=55.0)
    S.set(S.play("sail", 0.0), S.train(80.0, 13.0))
    S.preroll()
    S.cam.data.lens = 35.0

    def step(t):
        S.set(S.play("sail", t), S.train(80.0, 13.0), S.play("broadside.heel", t - 1.0), *[S.play("fire." + tn.split(".")[1], t - t0) for tn, t0 in t_fire.items()])
        S.sea.at(t)
        S.fx.at(t)
        kit.look_at(S.cam, (L * 0.86 - 1.5 * t, -L * 0.5, 9.0 + 0.3 * t), (L * 0.02, 0, 13.0))
    return T, step


def _doomed(S, t_hit, t_end_fx, kind):
    """What a sinking has in common: she is under fire and burning, takes the blow, loses way, her wake dies."""
    S.funnels(end=t_hit)
    S.spray(end=t_hit + 1.0)
    S.fx.fire(S.where("fire.1"), start=-12.0, size=0.8)
    S.fx.splash(0.45, (34.0, -62.0, 0.0), high=50.0)
    S.fx.splash(0.8, (-72.0, 58.0, 0.0), high=50.0)
    S.set(S.play("sail", 0.0))
    S.preroll()


def _goes_under(S, half, every=5.0):
    """Where a half of a broken ship is going under: the places along both her sides where the sea has climbed more
    than half way up to her deck's edge and her deck is not yet deep under. Gives a function of nothing: those
    places now, each (a place on the sea, the way outward from her there)."""
    h, bx = S.yard.h, S.yard.break_x
    x_end = h.frame_x(0.97) if half == "fore" else h.frame_x(-0.97)
    n = max(2, int(abs(x_end - bx) / every))
    rest = []
    for i in range(n + 1):
        x = bx + (x_end - bx) * i / n
        u = h.u_of(x)
        rest.append((h.point(u, 1.0), h.point(u, 0.0), Vector((x, 0.0, 0.0))))

    def where():
        out = []
        for edge, water, mid in rest:
            c = rigid.carried(S.m, S.pose, half, mid)
            for hand in (1.0, -1.0):
                e = rigid.carried(S.m, S.pose, half, Vector((edge.x, hand * edge.y, edge.z)))
                w = rigid.carried(S.m, S.pose, half, Vector((water.x, hand * water.y, water.z)))
                if not (-2.0 < e.z < 0.55 * edge.z) or w.z > 0.0:
                    continue
                p = w.lerp(e, -w.z / (e.z - w.z)) if e.z > 0.0 else e
                way = Vector((p.x - c.x, p.y - c.y, 0.0))
                if way.length > 0.5:
                    out.append((Vector((p.x, p.y, 0.0)), way.normalized()))
        return out
    return where


def sink(S, out, opt):
    T, t_hit, quick = 10.5, 1.7, 2.5
    L = S.L
    _doomed(S, t_hit, T, "break")
    S.fx.burst(t_hit - 0.35, S.where("fire.2"), size=1.3)
    S.fx.blast(t_hit, S.where("fire.2"), size=1.0)
    S.fx.fire(S.where("fire.3"), start=t_hit + 0.2, size=1.2)
    S.fx.fire(S.where("fire.2"), start=t_hit + 0.2, end=t_hit + 3.0, size=1.5)
    bx = S.yard.break_x
    if any(s["name"] == "break" for s in S.m.sockets):      # (steam and smoke pour from the gap until it goes under)
        S.fx.steam(S.where("break"), t_hit + 0.15, t_hit + 5.0)
    # The white water: heaviest in the middle, where the two broken ends go down (air bursting up, and a dark sheen
    # of oil spreading from there); lighter along each half's own side where the sea climbs it.
    for half in ("fore", "aft"):
        S.fx.froth(_goes_under(S, half), t_hit + 0.9, T + 2)
    S.fx.boil((bx, 0.0), t_hit + 2.4, T + 2, r=(40.0, 28.0), rise=2.2)
    S.fx.slick(t_hit + 3.0, (bx, 0, 0), r=62.0)

    def step(t):
        after = t - t_hit
        sail = fade(S.play("sail", t), 1.0 - smooth(after / 1.5))
        S.set(sail, S.play("break", after * quick))
        way = S.sea.speed * (t if after < 0 else t_hit + 2.5 * (1 - math.exp(-after / 2.5)))
        S.sea.at(t, way)
        S.wake.default_value = 1.0 - smooth(after / 3.0)
        S.fx.at(t)
        kit.look_at(S.cam, (-L * 0.3 + 2.0 * t, -L * 1.22, L * 0.13), (0.0, 0, 16.0))
    return T, step


def capsize(S, out, opt):
    T, t_hit, quick = 10.5, 1.4, 2.6
    L = S.L
    _doomed(S, t_hit, T, "capsize")
    for k, (name, dt) in enumerate((("hit.starboard.2", 0.0), ("hit.starboard.4", 0.35))):       # two torpedoes strike her starboard side
        at = rigid.place(S.m, {}, name)[0]
        S.fx.splash(t_hit + dt, (at.x, at.y - 3.0, 0.0), high=62.0, r=7.0)
    S.fx.burst(t_hit + 0.05, S.where("fire.2"), size=1.0)
    S.fx.fire(S.where("fire.2"), start=t_hit + 0.3, size=1.1)
    mid = rigid.place(S.m, {}, "fire.2")[0]
    S.fx.boil((mid.x, -8.0), t_hit + 4.0, T + 2, r=(40.0, 20.0), rise=3.0)
    S.fx.slick(t_hit + 5.5, (mid.x, -10.0, 0), r=50.0)

    def step(t):
        after = t - t_hit
        sail = fade(S.play("sail", t), 1.0 - smooth(after / 1.5))
        S.set(sail, S.play("hit", after), S.play("capsize.starboard", (after - 0.5) * quick))
        way = S.sea.speed * (t if after < 0 else t_hit + 3.0 * (1 - math.exp(-after / 3.0)))
        S.sea.at(t, way)
        S.wake.default_value = 1.0 - smooth(after / 3.5)
        S.fx.at(t)
        kit.look_at(S.cam, (L * 0.42 - 2.0 * t, -L * 1.12, L * 0.12), (0.0, 0, 12.0))
    return T, step


SHOTS = {"turn": turn, "broadside": broadside, "sink": sink, "capsize": capsize}


def main():
    name, shot, out = sys.argv[1], sys.argv[2], sys.argv[3]
    opt = dict(a.split("=", 1) for a in sys.argv[4:])
    fps = int(opt.get("fps", 12))
    w, h = (int(v) for v in opt.get("size", "960x540").split("x"))
    folder = os.path.join(out, f"{name}-{shot}")
    os.makedirs(folder, exist_ok=True)
    slow = shot in ("sink", "capsize")
    S = Show(name, folder, speed=12.0, wind=(-7.0, 2.0, 0.0) if slow else None, month=opt.get("month"))
    stage.render_setup(w, h, int(opt.get("samples", 16)))
    T, step = SHOTS[shot](S, out, opt)
    only = {int(v) - 1 for v in opt["frames"].split(",")} if "frames" in opt else None
    stage.film(folder, T, fps, step, only=only)
    if only is None:
        kit.encode(folder, os.path.join(out, f"{name}-{shot}"), fps=fps, gif_width=640)
        print("made", os.path.join(out, f"{name}-{shot}.mp4"))


main()
