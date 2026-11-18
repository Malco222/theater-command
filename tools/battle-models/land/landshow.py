"""A land vehicle on the field: stills for a sheet, and clips of her in motion.

    python landshow.py <Name> <out folder> sheet            # six stills on the field and a sheet of them
    python landshow.py <Name> <out folder> action           # she drives up, halts, trains her gun and fires
    python landshow.py <Name> <out folder> brewup           # she is struck and her ammunition goes
    python landshow.py <Name> <out folder> faces            # one still of each gun and coat she wore (vehicles.FACES)
    options: fps=15 size=960x540 spp=20 only=12,40 (frames, to try a clip) tmp=<folder for frames>
             field=battle (the field battle's own ground and trees; or meadow, steppe, desert: this folder's plain field)
             gun=short paint=yellow (what the builder offers: a Panzer IV's short gun, her dark yellow coat)

Everything a clip shows is the model's own rig and clips (the same the game plays), with the effects the clips' events
name drawn by ground.Air. The game moves her, rolls her wheels and runs her tracks; here Show.drive does that.
"""
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Quaternion, Vector  # noqa: E402

import ground  # noqa: E402
import kit  # noqa: E402
import landmarks  # noqa: E402
import rigid  # noqa: E402
import stage  # noqa: E402
import vehicles  # noqa: E402
import works  # noqa: E402

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


def ease(a, b, t):
    f = min(max((t - a) / (b - a), 0.0), 1.0)
    return f * f * (3 - 2 * f)


class Show:
    def __init__(self, name, field="battle", wind=None, sun=(25.0, 34.0), grass=(-34.0, 46.0, -30.0, 30.0), density=26.0, tag=None, build=None, **field_kw):
        kit.fresh()
        t = time.time()
        self.v = vehicles.build(name, True, tag, **(build or {}))
        self.m = self.v.m
        # the dust she carries is the ground she stands on
        dust_hex = "9A8A6A" if field == "battle" else ground.KINDS[field][3]
        self.ob, self.arm = works.to_scene(self.m, dust_hex=dust_hex)
        landmarks.put(self.m, self.ob, self.arm)
        belt = next(iter(self.v.belts.values())) if self.v.belts else None
        rut = abs(belt["y"]) if belt else 0.9
        if field == "battle":
            self.field = ground.Battle(rut=rut, rut_wide=belt["wide"] if belt else 0.3, pitch=belt["pitch"] if belt else 0.12, **field_kw)
        else:
            self.field = ground.Field(kind=field, rut=rut, grass=grass, density=density, **field_kw)
        self.sun_at(*sun)
        self.fx = ground.Air(wind=wind or (-1.4, 1.9, 0.0))
        self.pose, self.at, self.heading, self.run = {}, Vector(), 0.0, {"l": 0.0, "r": 0.0}
        self.cam = kit.camera("cam", 50.0)
        self.cam.data.clip_end = 20000.0
        self.clip = self.m.clips
        xs, zs = [p.x for p in self.m.verts], [p.z for p in self.m.verts]
        hull = [p.x for p in self.m.verts if abs(p.y) > 0.6] or xs       # (her body without the gun's overhang)
        # (how far off the cameras stand goes by the hull, so a short gun and a long one are framed alike)
        # (and how high by her body, so a tall aerial does not send them up into the air)
        self.L, self.H, self.cx = max(max(xs) - min(xs), (max(hull) - min(hull)) * 1.27), min(max(zs), 1.36 * works.body_top(self.m)), (max(xs) + min(xs)) / 2
        print(f"{name}: {self.m.triangles()} triangles, built and staged in {time.time() - t:.1f}s", flush=True)

    def sun_at(self, bearing, elev):
        """The sun stands at this bearing (degrees, from +x toward +y) and height."""
        self.field.sun.rotation_euler = (math.radians(90.0 - elev), 0.0, math.radians(bearing + 90.0))

    # -- where she is
    def place(self, at, heading=0.0):
        self.at, self.heading = Vector(at), heading
        self.arm.location = self.at
        self.arm.rotation_euler = (0.0, 0.0, math.radians(heading))

    def world(self, p, d=None):
        q = Quaternion(Z, math.radians(self.heading))
        return (self.at + q @ Vector(p), q @ Vector(d)) if d is not None else self.at + q @ Vector(p)

    def where(self, socket):
        return lambda: self.world(*rigid.place(self.m, self.pose, socket))

    def has(self, socket):
        return any(s["name"] == socket for s in self.m.sockets)

    def sockets(self, prefix):
        return [s["name"] for s in self.m.sockets if s["name"].startswith(prefix)]

    # -- how she stands
    def set(self, *poses):
        self.pose = rigid.merge(*poses)
        rigid.apply_pose(self.arm, self.pose, model=self.m)

    def play(self, clip, t, weight=1.0):
        if t < 0 or clip not in self.clip:
            return {}
        c = self.clip[clip]
        pose = rigid.pose_of(self.m, c, t if c.get("loop") else min(t, c["long"]))
        if weight < 0.999:
            pose = {k: (Quaternion().slerp(q, weight), mv * weight, sc) for k, (q, mv, sc) in pose.items()}
        return pose

    def rolled(self, left, right=None):
        """Her wheels turned and her tracks run on for the distance each side has gone (metres)."""
        right = left if right is None else right
        out = {}
        for part, radius in self.v.wheels:
            side = part.split("_")[-1][0]
            out.update(rigid.aim(self.m, part, math.degrees((left if side == "l" else right) / radius)))
        for name, dist in (("tracks.left", left), ("tracks.right", right)):
            if name in self.clip:
                c = self.clip[name]
                out.update(rigid.pose_of(self.m, c, (dist / c["stride"]) * c["long"]))
        return out

    def train(self, bearing=0.0, lift=0.0):
        out = {}
        if "turret" in self.m.part_at:
            out.update(rigid.aim(self.m, "turret", bearing))
        if "gun" in self.m.part_at:
            out.update(rigid.aim(self.m, "gun", lift))
        return out

    def about_turret(self, pose, bearing):
        """A hull pose made for a gun trained ahead, turned to answer a gun trained off."""
        if "hull" not in pose or abs(bearing) < 0.5:
            return pose
        q, mv, sc = pose["hull"]
        axis, ang = q.to_axis_angle()
        out = dict(pose)
        turn = Quaternion(Z, -math.radians(bearing))
        out["hull"] = (Quaternion(turn @ axis, ang), turn @ mv, sc)
        return out

    # -- cameras
    def look(self, eye, target, lens=50.0):
        self.cam.data.lens = lens
        kit.look_at(self.cam, eye, target)

    def preroll(self, secs=4.0, t0=0.0):
        t = t0 - secs
        while t < t0:
            self.fx.at(t)
            t += 0.1


def choice(opt):
    """What the command line asks of the builder: gun=short paint=yellow."""
    return {k: opt[k] for k in ("gun", "paint") if k in opt}


def options(argv):
    opt = dict(a.split("=", 1) for a in argv if "=" in a)
    w, h = (int(a) for a in opt.get("size", "960x540").split("x"))
    only = set(int(a) - 1 for a in opt["only"].split(",")) if opt.get("only") else None
    return opt, w, h, int(opt.get("spp", 20)), int(opt.get("fps", 15)), only


def finish(S, name, out, what, tmp, fps, only):
    if only is None:
        kit.encode(tmp, os.path.join(out, "clips", f"{name}-{what}"), fps=fps, gif_width=640)
        print("wrote", os.path.join(out, "clips", f"{name}-{what}.mp4"), flush=True)


# ---- the sheet ---------------------------------------------------------------------------------------

def sheet(name, out, opt, w, h, spp):
    S = Show(name, field=opt.get("field", "battle"), grass=(-16.0, 22.0, -22.0, 22.0), density=float(opt.get("grass", 26.0)), build=choice(opt))
    stage.render_setup(w, h, samples=spp)
    L, H, cx = S.L, S.H, S.cx
    m = S.m
    bearing = 28.0 if "turret" in m.part_at else 0.0
    opened = {}
    for hname in m.info.get("file", {}).get("controls", {}).get("hatches", [])[:1]:
        opened.update(rigid.aim(m, hname, m.parts[m.part_at[hname]].limits[1] if m.parts[m.part_at[hname]].limits else 90.0))
    shots = [
        ("front", "From the front", (cx + L * 0.98, -L * 0.8, H * 0.78), (cx + 0.2, 0, H * 0.4), 50, 60.0, {}),
        ("rear", "From the rear", (cx - L * 1.0, L * 0.8, H * 0.85), (cx - 0.2, 0, H * 0.4), 50, 55.0, {}),
        ("side", "Side on", (cx, -L * 1.55, H * 0.5), (cx, 0, H * 0.42), 50, 50.0, {}),
        ("game", "As the battle camera sees her", (cx + 16 * 0.866 * 0.8, -16 * 0.866 * 0.6, 8.0), (cx, 0, H * 0.3), 50, 60.0, S.train(bearing, 4.0)),
        ("gear", "Running gear, close", (cx + L * 0.47, -L * 0.42, 0.6), (cx + L * 0.27, -1.0, 0.5), 50, 55.0, {}),
        ("trained", "Gun trained, hatch open", (cx + L * 0.75, -L * 0.62, H * 1.15), (cx + 0.3, 0, H * 0.55), 55, 60.0, rigid.merge(S.train(bearing, 8.0), opened)),
    ]
    only = opt.get("views", "").split(",") if opt.get("views") else None
    files, labels = [], []
    for key, label, eye, tgt, lens, sun, pose in shots:
        path = os.path.join(out, "sheets", "stills", f"{name}-{key}.png")
        files.append(path)
        labels.append(label)
        if only and key not in only:
            continue
        S.place((0, 0, 0))
        S.set(pose)
        S.look(eye, tgt, lens)
        S.sun_at(math.degrees(math.atan2(eye[1] - tgt[1], eye[0] - tgt[0])) + sun, 36.0)
        t = time.time()
        kit.still(path)
        print(f"  {key}: {time.time() - t:.1f}s", flush=True)
    if all(os.path.exists(f) for f in files):            # (with views=, the stills re-shot join the others on the sheet)
        title = f"{m.title}   {m.triangles():,} triangles near, {len(m.parts)} moving parts, {len(m.clips)} clips"
        kit.sheet(files, os.path.join(out, "sheets", f"{name}.png"), cols=3, labels=labels, title=title)
        print("wrote", os.path.join(out, "sheets", f"{name}.png"), flush=True)


# ---- the faces she showed through the war ------------------------------------------------------------

def faces(name, out, opt, w, h, spp):
    """One still of each face of her that vehicles.FACES names (a gun, a coat), from the same place, side by side."""
    files, labels, eye = [], [], None
    for k, (words, pick) in enumerate(vehicles.FACES.get(name, [])):
        S = Show(name, field=opt.get("field", "battle"), build=pick)
        stage.render_setup(w, h, samples=spp)
        if eye is None:
            xs = [p.x for p in S.m.verts if abs(p.y) > 0.6]              # (the hull, not the gun: every face from one place)
            L, cx, H = max(xs) - min(xs), (max(xs) + min(xs)) / 2, S.H
            # (far enough off, and looking far enough forward, that the longest gun's muzzle is in the picture)
            eye, tgt = (cx + L * 1.16, -L * 0.9, H * 0.8), (cx + 0.6, 0, H * 0.4)
        S.place((0, 0, 0))
        S.set({})
        S.look(eye, tgt, 44)
        S.sun_at(math.degrees(math.atan2(eye[1] - tgt[1], eye[0] - tgt[0])) + 60.0, 36.0)
        path = os.path.join(out, "sheets", "stills", f"{name}-face{k + 1}.png")
        t = time.time()
        kit.still(path)
        print(f"  {words}: {time.time() - t:.1f}s", flush=True)
        files.append(path)
        labels.append(words)
    if files:
        kit.sheet(files, os.path.join(out, "sheets", f"{name}-faces.png"), cols=len(files), labels=labels, title=f"{S.m.title}: one model, the faces she showed through the war")
        print("wrote", os.path.join(out, "sheets", f"{name}-faces.png"), flush=True)


# ---- the whole motor pool on one field ---------------------------------------------------------------

def pool(names, out, opt, w, h, spp):
    """Every vehicle named, drawn up in echelon on the battle's ground: the picture of them all together, a second
    from the other end of the row, and a sheet of the two. `gap=` metres between neighbours, `step=` metres each
    stands behind her neighbour, `as=` the file's name."""
    kit.fresh()
    t = time.time()
    stood = []
    for name in names:
        v = vehicles.build(name, True)
        ob, arm = works.to_scene(v.m, dust_hex="9A8A6A")
        landmarks.put(v.m, ob, arm)
        stood.append((v, arm))
    field = ground.Battle(rut=1.2, rut_wide=0.4, ruts=False)
    stage.render_setup(w, h, samples=spp)
    cam = kit.camera("cam", 50.0)
    cam.data.clip_end = 20000.0
    n, gap, step = len(stood), float(opt.get("gap", 4.5)), float(opt.get("step", 1.5))
    x_at = float(opt.get("x", 0.0))
    for i, (v, arm) in enumerate(stood):
        arm.location = (x_at - step * i, ((n - 1) / 2 - i) * gap, 0.0)
    if "look" in opt:                                    # (what stands in the row's way: say so, do not render)
        lo, hi = Vector((x_at - step * n - 6.0, -n * gap / 2 - 2.0, -1.0)), Vector((x_at + 6.0, n * gap / 2 + 2.0, 6.0))
        for ob in bpy.context.scene.objects:
            if ob.type != "MESH" or any(ob.parent == arm for _, arm in stood) or max(ob.dimensions) > 80.0:
                continue
            c = [ob.matrix_world @ Vector(b) for b in ob.bound_box]
            a, b = Vector((min(p.x for p in c), min(p.y for p in c), min(p.z for p in c))), Vector((max(p.x for p in c), max(p.y for p in c), max(p.z for p in c)))
            if all(a[k] < hi[k] and b[k] > lo[k] for k in range(3)):
                print(f"  in the way: {ob.name}  x {a.x:.1f}..{b.x:.1f}  y {a.y:.1f}..{b.y:.1f}  z {a.z:.1f}..{b.z:.1f}", flush=True)
    print(f"{n} vehicles staged in {time.time() - t:.1f}s", flush=True)
    wide = (n - 1) * gap
    mid = Vector((x_at - step * (n - 1) / 2, 0.0, 1.1))
    name = opt.get("as", "pool")
    shots = [("a", (mid.x + 0.62 * wide + 16.0, -(0.5 * wide + 15.0), 4.6), 38.0), ("b", (mid.x + 0.45 * wide + 15.0, 0.5 * wide + 17.0, 3.4), 40.0)]
    lens = float(opt.get("lens", 62.0))
    files = []
    for key, eye, sun in shots:
        if opt.get("views") and key not in opt["views"].split(","):
            continue
        cam.data.lens = lens
        kit.look_at(cam, eye, mid)
        field.sun.rotation_euler = (math.radians(90.0 - 36.0), 0.0, math.radians(math.degrees(math.atan2(eye[1] - mid.y, eye[0] - mid.x)) + (sun if eye[1] < 0 else -sun) + 90.0))
        path = os.path.join(out, "sheets", "stills", f"{name}-{key}.png")
        tick = time.time()
        kit.still(path)
        files.append(path)
        print(f"  {key}: {time.time() - tick:.1f}s", flush=True)
    if len(files) == 2:
        kit.sheet(files, os.path.join(out, "sheets", f"{name}.png"), cols=1, labels=[", ".join(v.m.title for v, _ in stood), "The same row from its other end"],
                  title="The motor pool so far, drawn up on the battle's ground")
        print("wrote", os.path.join(out, "sheets", f"{name}.png"), flush=True)


# ---- she drives up, halts, trains and fires ----------------------------------------------------------

def action(name, out, opt, w, h, spp, fps, only):
    S = Show(name, field=opt.get("field", "battle"), density=float(opt.get("grass", 26.0)), build=choice(opt))
    stage.render_setup(w, h, samples=spp)
    m = S.m
    v0, t1, t2 = 6.5, 2.4, 3.9                       # under way at 6.5 m/s, braking from t1, at rest at t2
    x_end = 0.0
    x_start = x_end - (v0 * t1 + v0 * (t2 - t1) / 2)
    # she halts and settles; then the gun is laid (the turret comes round at its own rate, the barrel lifts a touch as
    # it comes on); a breath; the shot
    bearing, lift, ramp = float(opt.get("bearing", -18.0)), 2.5, 0.25
    rate = m.info.get("file", {}).get("controls", {}).get("turret", {}).get("degrees_per_second", 14.0)
    t_train0 = t2 + 0.45
    t_train1 = t_train0 + abs(bearing) / rate + ramp
    t_lift0, t_lift1 = t_train1 - 0.5, t_train1 + 0.3
    t_fire = math.ceil((t_lift1 + 0.4) * fps) / fps - 0.012      # (the shot falls just before a frame, so the flash's one bright instant is seen)
    t_mg = t_fire + 2.0
    long = float(opt.get("long", t_mg + 1.9))
    brake_rate = S.clip["stop"].get("rest", 0.0) / (t2 - t1) if "stop" in S.clip else 0.0
    print(f"  halts {t2:.2f}s, lays {t_train0:.2f} to {t_lift1:.2f}s, fires {t_fire:.3f}s (frame {int(t_fire * fps) + 2}), {long:.1f}s in all", flush=True)

    def trained(t):
        """The turret's bearing: it gathers way, turns at its rate, and slows onto the target."""
        a, T = t - t_train0, t_train1 - t_train0
        if a <= 0.0:
            return 0.0
        if a >= T:
            return bearing
        if a < ramp:
            d = rate * a * a / (2 * ramp)
        elif a < T - ramp:
            d = rate * (a - ramp / 2)
        else:
            d = abs(bearing) - rate * (T - a) ** 2 / (2 * ramp)
        return math.copysign(d, bearing)

    def speed(t):
        return v0 if t < t1 else (v0 * (t2 - t) / (t2 - t1) if t < t2 else 0.0)

    def gone(t):
        if t < t1:
            return v0 * t
        if t < t2:
            return v0 * t1 + v0 * (t - t1) - 0.5 * v0 * (t - t1) ** 2 / (t2 - t1)
        return v0 * t1 + v0 * (t2 - t1) / 2

    for s in S.sockets("dust."):
        S.fx.dust(S.where(s), speed, start=-3.0, end=t2 + 0.2)
    for s in S.sockets("exhaust."):
        S.fx.exhaust(S.where(s), rev=lambda t: 0.9 if t < t1 else 0.35, start=-3.0)
    has_turret = "turret" in m.part_at
    if S.has("muzzle"):
        S.fx.gun(t_fire, S.where("muzzle"), bore=m.info.get("bore", 0.075), brake=m.info.get("brake", True))
        S.fx.shaken(t_fire + 0.02, [S.where(s) for s in S.sockets("dust.")] + [S.where("engine")] if S.has("engine") else [])
    if S.has("coax"):
        S.fx.mg(t_mg, S.where("coax"))
    eye = Vector((13.5, -8.2, 1.55))

    def step(t):
        d = gone(t)
        S.place((x_start + d, 0, 0))
        b = trained(t) if has_turret else 0.0
        stop = S.play("stop", (t - t1) * brake_rate) if brake_rate else S.play("stop", t - (t2 - 0.35))
        poses = [S.rolled(d), S.play("drive", t, weight=min(1.0, speed(t) / 3.0)) if t < t2 else {}, stop,
                 S.train(b, lift * ease(t_lift0, t_lift1, t)), S.play("fire", t - t_fire), S.about_turret(S.play("fire.rock", t - t_fire), b)]
        S.set(*poses)
        S.fx.at(t)
        # the camera stands by the road and follows her, a little behind her going
        look = Vector((x_start + gone(t - 0.25) + 0.6, 0.0, S.H * 0.5))
        S.look(eye, look, 46.0)

    # the dust and exhaust of the seconds before the clip begins
    t = -3.0
    while t < 0.0:
        S.place((x_start + v0 * t, 0, 0))
        S.set(S.rolled(v0 * t))
        S.fx.at(t)
        t += 1.0 / fps
    tmp = opt.get("tmp", os.path.join(out, "frames", f"{name}-action"))
    stage.film(tmp, long, fps, step, only=only)
    finish(S, name, out, "action", tmp, fps, only)


# ---- she is struck and her ammunition goes -----------------------------------------------------------

def brewup(name, out, opt, w, h, spp, fps, only):
    S = Show(name, field=opt.get("field", "battle"), density=float(opt.get("grass", 26.0)), grass=(-26.0, 30.0, -30.0, 26.0), build=choice(opt))
    stage.render_setup(w, h, samples=spp)
    m = S.m
    t_hit = 0.8
    clip = "brew_up" if "brew_up" in S.clip else "knocked_out"
    long = float(opt.get("long", 8.0))
    blast = next((e["t"] for e in S.clip[clip]["events"] if e["fx"] == "magazine_blast"), None)
    side = "hit.turret" if S.has("hit.turret") else "hit.left"
    S.fx.strike(t_hit, S.where(side))
    if S.has("engine"):
        S.fx.exhaust(S.where("exhaust.1"), rev=0.3, start=-2.0, end=t_hit) if S.has("exhaust.1") else None
    hatches = [s for s in S.sockets("fire.")]
    on_turret = {s["name"] for s in m.sockets if s["name"].startswith("fire.") and m.parts[s["part"]].name not in ("hull", "vehicle")}
    if blast is not None:
        tb = t_hit + blast
        for s in hatches:                             # flame stands out of every hatch for the half second before she goes
            S.fx.flames(S.where(s), start=t_hit + 0.08, end=tb, size=1.5, lean=Vector((0, 0, 0)))
        S.fx.brew_up(tb, S.where("ring"), size=1.0)
        burning = ["ring"] + [s for s in hatches if s not in on_turret]
    else:
        tb = t_hit + 1.2
        burning = (["engine"] if S.has("engine") else []) + hatches
    for i, s in enumerate(burning):
        S.fx.flames(S.where(s), start=tb + 0.15 * i, size=1.25 if s == "ring" else 0.8)
        S.fx.smoke_from(S.where(s), start=tb + 0.3, size=1.0 if s in ("ring", "engine") else 0.55, every=0.1 if s in ("ring", "engine") else 0.22)
    S.fx.fire_light(S.where(burning[0]), tb, 1e9, power=1500.0)
    if S.has("engine") and "engine" not in burning:
        S.fx.smoke_from(S.where("engine"), start=tb + 1.0, size=0.7, thick=0.8, every=0.16)
    eye = Vector((14.5, 10.5, 1.7))           # (on the side her turret falls)
    S.sun_at(-20.0, 34.0)

    def step(t):
        S.place((0, 0, 0))
        S.set(S.play("idle", t) if t < t_hit else {}, S.play("hit", t - t_hit) if blast is not None and t < t_hit + blast else {}, S.play(clip, t - t_hit))
        S.fx.at(t)
        up = ease(t_hit + 0.4, t_hit + 3.0, t)
        S.look(eye + Vector((1.0, 1.0, 0.3)) * up, Vector((0.0, 0.3, S.H * 0.55 + 1.7 * up)), 40.0 - 4.0 * up)

    S.preroll(2.0)
    tmp = opt.get("tmp", os.path.join(out, "frames", f"{name}-brewup"))
    stage.film(tmp, long, fps, step, only=only)
    finish(S, name, out, "brewup", tmp, fps, only)


def main():
    name, out, what = sys.argv[1], sys.argv[2], sys.argv[3]
    opt, w, h, spp, fps, only = options(sys.argv[4:])
    if what == "sheet":
        sheet(name, out, opt, w if "size" in opt else 1280, h if "size" in opt else 720, int(opt.get("spp", 40)))
    elif what == "action":
        action(name, out, opt, w, h, spp, fps, only)
    elif what == "brewup":
        brewup(name, out, opt, w, h, spp, fps, only)
    elif what == "faces":
        faces(name, out, opt, w if "size" in opt else 1280, h if "size" in opt else 720, int(opt.get("spp", 40)))
    elif what == "pool":                                 # python landshow.py PanzerIV,Panther,Tiger <out> pool
        pool(name.split(","), out, opt, w if "size" in opt else 1600, h if "size" in opt else 800, int(opt.get("spp", 40)))


if __name__ == "__main__":
    main()
