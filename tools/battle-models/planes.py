"""The aircraft as battle models.

The game already has its aircraft, sculpted by `Tools/build_plane.py` from the numbers in `plane_numbers.py` (copies
of both are in `game/`). Nothing is sculpted again here: each machine is built by that script and then taken apart
into the parts a battle moves:

    plane               the fuselage (the root: the game flies it, the clips only tip and roll it)
    wing.port/starboard each wing outboard of the fuselage, with whatever it carries; breaks away at its root
    tail                the rear fuselage with tailplane, fins and tailwheel; breaks away behind the wing
    prop.N              the blades of each airscrew (numbered from the port side), turning about its hub
    prop.N.blur         the disc a fast-turning airscrew is seen as
    bombs               what hangs under her and can be dropped (where she has it)

Where a part breaks away both torn ends are closed with the paint `torn`. The paints are the game's own, in its
order, so the game's colours, camouflage patches (blend), soot and wear all carry through; `torn` and `blur` are
added after them.

Also here: the parachute and the bomb, small models of their own.

    import planes;  m, facts = planes.BUILDERS["Spitfire"](True)
"""
import math
import os

import bmesh
import bpy
from mathutils import Quaternion, Vector

import rigid
from rigid import Model, smooth

HERE = os.path.dirname(os.path.abspath(__file__))
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
_G = None


def game():
    """The game's own aircraft sculptor, loaded without running it."""
    global _G
    if _G is None:
        path = os.path.join(HERE, "game", "build_plane.py")
        src = open(path).read().split("\ndef main():")[0]
        _G = {"__file__": path, "__name__": "build_plane"}
        exec(compile(src, path, "exec"), _G)
    return _G


def names():
    return list(game()["PLANES"])


# What kind of machine each is, and how many are aboard (the parachutes a lost one can leave).
CREW = {"Bf109": 1, "Spitfire": 1, "Zero": 1, "Mustang": 1, "Yak": 1, "Fw190": 1, "Hurricane": 1, "Warhawk": 1, "Fighter": 1,
        "Stuka": 2, "Sturmovik": 2, "He111": 5, "Lancaster": 7, "B17": 10, "Bomber": 5}
ROLE = {"Stuka": "dive bomber", "Sturmovik": "attack", "He111": "bomber", "Lancaster": "bomber", "B17": "bomber", "Bomber": "bomber"}


def port(v):
    """A place in the game's axes (y to the right) as the model has it (y to port)."""
    return Vector((v[0], -v[1], v[2]))


class Facts:
    """What the clips and the films need to know about a machine."""


def convert(name, fine=True):
    G = game()
    s = G["PLANES"][name]
    PAINTS = G["PAINTS"]
    parts, hull = G["build"](s, fine)
    colours = G["colours_of"](s)
    paint_names = list(PAINTS)
    paints = {k: colours.get(k, "808080") for k in paint_names}
    paints["torn"] = "2B2825"
    paints["blur"] = "3A3A36"
    role = ROLE.get(name, "fighter")
    m = Model(name, s.get("title", name), "aircraft", paints, fine)
    wing, tailw = G["Wing"](s["wing"]), G["Wing"](s["tail"])
    half = wing.tip

    # ---- where she breaks ------------------------------------------------------------------------
    by_kind = {}
    for ob in parts.objects:
        by_kind.setdefault(ob.name.split(".")[0], []).append(ob)
    root_mid = 0.5 * (wing.lead(wing.root) + wing.trail(wing.root))
    side = hull.w(-root_mid)
    rings_y = sorted({round(abs(v.co.y), 4) for ob in by_kind["wing"] for v in ob.data.vertices})
    y_cut = next((y for y in rings_y if y >= side + 0.12), rings_y[-2])
    rings_x = sorted({round(v.co.x, 4) for ob in by_kind["hull"] for v in ob.data.vertices})
    a, b = wing.trail(wing.root), tailw.lead(tailw.root)
    hood_back = min([min(r[0] for r in h[0]) for h in s.get("hoods", [])] + [a])
    want = min(b + 0.38 * (a - b), hood_back - 0.2)
    ok = [x for x in rings_x if b + 0.15 < x < a - 0.1] or rings_x
    x_cut = min(ok, key=lambda x: abs(x - want))

    # ---- the airscrews ---------------------------------------------------------------------------
    hubs = []
    if not s.get("engines"):
        hub_x = sorted((r[0] for r in s["hull"]), reverse=True)[s.get("spinner", 2) - 1] + 0.06
        hubs.append(Vector((hub_x, 0.0, hull.z(-hub_x))))
    for (out, lead, z, is_radial, back) in s.get("engines", []):
        for hand in (1.0, -1.0):
            hubs.append(Vector((lead + (1.96 if is_radial else 2.16), out * hand, z)))
    hubs.sort(key=lambda h: h.y)                       # (the game's y is to the right: port first)
    screw = s.get("screw", 1.5 if not s.get("engines") else 1.7)

    def side_of(y):
        return "wing.port" if y < 0.0 else "wing.starboard"      # (the game's y)

    m.part("plane", None, (0, 0, 0), kind="root", note="the fuselage; the game flies her, the clips only tip and roll her")
    wing_z = wing.z(y_cut)
    for hand, nm in ((-1.0, "wing.port"), (1.0, "wing.starboard")):
        m.part(nm, "plane", port((root_mid, hand * y_cut, wing_z)), kind="free", note="breaks away at the root (fall_spin, fall_explode)")
    m.part("tail", "plane", (x_cut, 0.0, hull.z(-x_cut)), kind="free", note="the rear fuselage and tail; breaks away (fall_explode)")
    def paint_of(ob):
        own = int(ob["paint"])
        return own if own >= 0 or not len(ob.data.polygons) else ob.data.attributes["paint"].data[0].value
    has_bombs = any(paint_of(ob) == PAINTS["bomb"] for ob in by_kind.get("extra", []))
    if has_bombs:
        m.part("bombs", "plane", (0, 0, 0), kind="slide", slide=(0, 0, -1), slide_limits=(0.0, 60.0), note="what she drops (bombs_away)")
    prop_parent = []
    for k, h in enumerate(hubs):
        parent = side_of(h.y) if abs(h.y) > y_cut else "plane"
        prop_parent.append(parent)
        m.part(f"prop.{k + 1}", parent, port(h), axis=(1, 0, 0), kind="spin", note="the blades; plus turns them clockwise seen from the cockpit")
        m.part(f"prop.{k + 1}.blur", parent, port(h), axis=(1, 0, 0), kind="spin", note="the disc a fast-turning airscrew is seen as; hidden (scaled away) when it is stopped")

    def where(kind, c, mean, ob):
        if kind == "hull":
            return "tail" if c.x < x_cut else "plane"
        if kind == "wing":
            return side_of(c.y) if abs(c.y) > y_cut else "plane"
        if kind in ("tail", "fin", "tailwheel", "tailwheel_leg"):
            return "tail"
        if kind == "blade":
            k = min(range(len(hubs)), key=lambda i: (hubs[i] - Vector((hubs[i].x, mean.y, mean.z))).length)
            return f"prop.{k + 1}"
        if kind in ("lines", "frames", "glints"):
            mean = c
        if kind == "extra" and has_bombs and paint_of(ob) == PAINTS["bomb"]:
            return "bombs"
        if kind in ("wire",):
            return "plane"
        if abs(mean.y) > y_cut and mean.x > x_cut:
            return side_of(mean.y)
        if mean.x < x_cut:
            return "tail"
        return "plane"

    # ---- every piece, face by face ---------------------------------------------------------------
    for ob in parts.objects:
        kind = ob.name.split(".")[0]
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bm.verts.ensure_lookup_table()
        bm.faces.ensure_lookup_table()
        layer = bm.faces.layers.int.get("paint")
        by_point = bm.verts.layers.int.get("coat")
        blend_l, soot_l = bm.verts.layers.float.get("blend"), bm.verts.layers.float.get("soot")
        own = int(ob["paint"])
        mean = sum((v.co for v in bm.verts), Vector()) / max(len(bm.verts), 1)
        per_face = kind in ("hull", "wing", "lines", "frames", "glints")
        whole = None if per_face else where(kind, mean, mean, ob)
        fpart = [whole or where(kind, f.calc_center_median(), mean, ob) for f in bm.faces]
        verts, coat, blend, soot, index = [], [], [], [], {}
        out = {}                                             # part -> (faces, paints, by_point)

        def point(v, part):
            key = (v.index, part)
            if key not in index:
                index[key] = len(verts)
                verts.append(port(v.co))
                coat.append(paint_names[v[by_point]] if by_point is not None else None)
                blend.append(v[blend_l] if blend_l is not None else 0.0)
                soot.append(v[soot_l] if soot_l is not None else 0.0)
            return index[key]

        for f, part in zip(bm.faces, fpart):
            of_face = own if own >= 0 else f[layer]
            corners = tuple(reversed([point(l.vert, part) for l in f.loops]))      # (mirrored, so turned round)
            faces, fp, fc = out.setdefault(part, ([], [], []))
            faces.append(corners)
            fp.append(paint_names[of_face] if of_face >= 0 else "upper")
            fc.append(of_face < 0)
        # (where a skin is cut between two parts, both torn ends are closed)
        caps = {}
        if kind in ("hull", "wing"):
            for e in bm.edges:
                if len(e.link_faces) == 2:
                    pa, pb = fpart[e.link_faces[0].index], fpart[e.link_faces[1].index]
                    if pa != pb:
                        caps.setdefault(tuple(sorted((pa, pb))), []).append(e)
        base = len(m.verts)
        m.verts.extend(verts)
        m.vcoat.extend([m.paint_at[c] if c is not None else -1 for c in coat])
        m.vblend.extend(blend)
        m.vsoot.extend(soot)
        for part, (faces, fp, fc) in out.items():
            pi = m.part_at[part]
            for f, p, c in zip(faces, fp, fc):
                m.faces.append(tuple(base + i for i in f))
                m.fpaint.append(m.paint_at[p])
                m.fpart.append(pi)
                m.fsmooth.append(True)
                m.fcoat.append(c)
        for (pa, pb), edges in caps.items():
            ring = sorted({v.index for e in edges for v in e.verts})
            pts = [port(bm.verts[i].co) for i in ring]
            mid = sum(pts, Vector()) / len(pts)
            # (put the ring in order round its middle: it lies in one plane, across the wing or the fuselage)
            away = Y if kind == "wing" else X
            u = (pts[0] - mid).normalized()
            w = away.cross(u)
            order = sorted(range(len(pts)), key=lambda i: math.atan2((pts[i] - mid).dot(w), (pts[i] - mid).dot(u)))
            ring_pts = [pts[i] for i in order]
            n = len(ring_pts)
            for part, other in ((pa, pb), (pb, pa)):
                def mid_of(p):
                    fs = [f for f, q in zip(bm.faces, fpart) if q == p]
                    return port(sum((f.calc_center_median() for f in fs), Vector()) / len(fs))
                way = mid_of(other) - mid_of(part)
                m.use(part)
                m.add(ring_pts + [mid], [(k, (k + 1) % n, n) for k in range(n)], "torn", False, lambda q, way=way: way)
        bm.free()
        bpy.data.objects.remove(ob, do_unlink=True)

    # ---- the disc of each airscrew ---------------------------------------------------------------
    for k, h in enumerate(hubs):
        m.use(f"prop.{k + 1}.blur")
        c = port(h) + X * 0.02
        steps = 28 if fine else 14
        ring = [c + Vector((0.0, math.cos(2 * math.pi * i / steps), math.sin(2 * math.pi * i / steps))) * screw for i in range(steps)]
        m.add(ring + [c], [(i, (i + 1) % steps, steps) for i in range(steps)], "blur", True, lambda q: X)

    # ---- sockets ---------------------------------------------------------------------------------
    for k, h in enumerate(hubs):
        m.socket(f"engine.{k + 1}", port(h) - X * 0.5, X, part=prop_parent[k], note="fire and smoke start here")
    n = 0
    for (out_, back, length, bore) in s.get("guns", []):
        for hand in (-1.0, 1.0):
            w = G["Wing"](s["wing"], hand)
            at = w.at(out_, 0.02, True)
            n += 1
            m.socket(f"gun.{n}", port((at.x + length, at.y, at.z - 0.03)), X, part=side_of(hand) if out_ > y_cut else "plane", note="a fixed gun's muzzle")
    if not s.get("engines") and role in ("fighter", "attack", "dive bomber"):
        x = hubs[0].x - 1.0
        top = hull.at(x, math.radians(90.0))
        m.socket("gun.nose", (top.x, 0.0, top.z + 0.03), X, part="plane", note="the guns over the engine")
    n = 0
    for kind, values in s.get("extras", []):
        if kind == "rod" and values[4] == "steel":
            a_, b_ = Vector(values[0]), Vector(values[1])
            if abs(b_.y) > 0.3 and abs(b_.x - a_.x) < 0.2:
                continue
            for hand in ((1.0, -1.0) if (a_.y != 0.0 or b_.y != 0.0) else (1.0,)):
                n += 1
                p, q = Vector((a_.x, a_.y * hand, a_.z)), Vector((b_.x, b_.y * hand, b_.z))
                m.socket(f"turret.{n}", port(q), port(q - p), part="tail" if q.x < x_cut else "plane", note="a gunner's muzzle")
    belly = hull.at(root_mid, -math.pi / 2.0)
    m.socket("bombs", (belly.x, 0.0, belly.z - 0.1), -Z, part="plane", note="where her bombs leave")
    hoods = s.get("hoods", [])
    if hoods:
        st = hoods[0][0]
        hx = 0.5 * (max(r[0] for r in st) + min(r[0] for r in st))
        hz = max(r[1] + r[3] for r in st)
        m.socket("hatch", (hx, 0.0, hz + 0.1), Z, part="plane", note="where her crew leave")
    else:
        top = hull.at(root_mid + 1.0, math.pi / 2.0)
        m.socket("hatch", (top.x, 0.0, top.z + 0.1), Z, part="plane", note="where her crew leave")
    m.socket("hatch.belly", (belly.x - 1.2, 0.0, hull.at(belly.x - 1.2, -math.pi / 2.0).z - 0.1), -Z, part="plane", note="a bomber's crew leave by the floor")
    for hand, nm in ((-1.0, "port"), (1.0, "starboard")):
        w = G["Wing"](s["wing"], hand)
        tip = w.at(half * 0.985, 0.5, True)
        m.socket(f"wingtip.{nm}", port(tip), -X, part=f"wing.{nm}", note="a trail from the tip in a hard turn")
        rootp = w.at(y_cut, 0.45, True)
        m.socket(f"break.wing.{nm}", port(rootp), port((0.0, -hand, 0.0)), part=f"wing.{nm}", note="the torn root of the wing: it burns and trails")
        m.socket(f"stub.{nm}", port(rootp), port((0.0, hand, 0.0)), part="plane", note="the stub left on the fuselage: fuel burns here")
        tank = w.at(max(y_cut + 0.6, half * 0.3), 0.4, True)
        m.socket(f"tank.{nm}", port(tank), Z, part=f"wing.{nm}", note="a wing tank: a fire in the wing starts here")
    m.socket("fuel", (root_mid, 0.0, hull.z(-root_mid)), Z, part="plane", note="the middle of her: where she blows up")
    m.socket("break.tail", (x_cut, 0.0, hull.z(-x_cut)), -X, part="tail", note="the torn end of the rear fuselage")
    m.socket("trail", (hull.back, 0.0, hull.z(-hull.back)), -X, part="tail", note="smoke streams from here when she is hit")

    # ---- what the game's file also carries -------------------------------------------------------
    nose_back = s.get("nose_back") if s.get("nose_back") is not None else hull.front - (hull.front - hull.back) * 0.22
    wide = hull.w(0.0)

    def weather(v, n, paint, part):
        w, y = 0.0, abs(v.y)
        if wide * 0.8 < y < wide + 0.9 and n[2] > 0.5 and abs(v.z) < 1.0:
            w = max(w, 0.70 * (1.0 - (y - wide * 0.8) / (0.9 + wide * 0.2)))
        if v.x > nose_back - 0.25:
            w = max(w, 0.38)
        if y > wide and n[0] > 0.75 and y < half:
            w = max(w, 0.45)
        return w, 0.0
    m.weather = weather
    m.marks = G["marks_of"](s, hull)
    m.info.update({"rust": "9A9C98", "soot": colours["soot"], "wear_show": 0.3,
                   "file": {"nation_paint": bool(s.get("nation_paint")), "number_ink": colours["ink"], "span": round(2.0 * half, 3),
                            "role": role, "crew": CREW.get(name, 1), "engines": len(hubs)}})
    f = Facts()
    f.name, f.spec, f.hull, f.wing, f.half, f.y_cut, f.x_cut, f.hubs, f.screw = name, s, hull, wing, half, y_cut, x_cut, [port(h) for h in hubs], screw
    f.role, f.crew, f.has_bombs, f.long = role, CREW.get(name, 1), has_bombs, hull.front - hull.back
    f.prop_parent = prop_parent
    clips_for_plane(m, f)
    return m, f


# ---- the clips ----------------------------------------------------------------------------------------

def q_of(roll=0.0, pitch=0.0, yaw=0.0):
    """An attitude: yaw to port, then nose up, then a roll to starboard (degrees), in the model's axes."""
    return (Quaternion(Z, math.radians(yaw)) @ Quaternion(Y, math.radians(-pitch)) @ Quaternion(X, math.radians(roll)))


def rot_keys(f, long, step=0.1):
    """Keys of a turning over time: f(t) -> quaternion."""
    n = max(1, int(round(long / step)))
    out = []
    last = None
    for k in range(n + 1):
        t = long * k / n
        q = f(t)
        if last is not None and q.dot(last) < 0.0:
            q = -q
        last = q
        out.append([round(t, 3), q.w, q.x, q.y, q.z])
    return out


def move_keys(f, long, step=0.1):
    n = max(1, int(round(long / step)))
    return [[round(long * k / n, 3), *f(long * k / n)] for k in range(n + 1)]


def loose(m, clip, part, parent_q, world_q, world_move, long, step=0.1):
    """A part that has come away: it is still the fuselage's child, so what it does in the open air (turning as
    world_q(t), drifting as world_move(t) from where it was) is worked back through the fuselage's own turning."""
    P = m.parts[m.part_at[part]]
    root = m.parts[P.parent]

    def local(t):
        qb = parent_q(t)
        inv = qb.inverted()
        q = inv @ world_q(t)
        at = P.pivot + Vector(world_move(t))                              # where its pivot should be, in the root's frame
        mv = inv @ (at - root.pivot) + root.pivot - P.pivot
        return q, mv
    clip["tracks"].append({"part": part, "rot": rot_keys(lambda t: local(t)[0], long, step), "move": move_keys(lambda t: tuple(local(t)[1]), long, step)})


def props(m, f, how):
    """The tracks of the airscrews: 'fast' (the disc alone), 'slow' (blades alone, turning), 'dead' (blades alone, still)."""
    def tracks(long):
        out = []
        for k in range(len(f.hubs)):
            blades, disc = f"prop.{k + 1}", f"prop.{k + 1}.blur"
            if how == "fast":
                out.append({"part": blades, "scale": [[0.0, 0.02]]})
                out.append({"part": disc, "turn": [[0.0, 0.0], [long, 360.0 * max(1, round(long * 1.5))]]})
            elif how == "slow":
                out.append({"part": blades, "turn": [[0.0, 0.0], [long, 360.0 * max(1, round(long * 1.2))]]})
                out.append({"part": disc, "scale": [[0.0, 0.001]]})
            else:
                out.append({"part": disc, "scale": [[0.0, 0.001]]})
        return out
    return tracks


def clips_for_plane(m, f):
    C = m.clips
    eng = [f"engine.{k + 1}" for k in range(len(f.hubs))]
    big = f.half > 9.0
    # -- flying --
    long = 4.0
    C["fly"] = {"long": long, "loop": True, "about": "Under way: every airscrew a turning disc, and she rides the air a little. The game flies her; this only lives on top.",
                "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=(0.6 if big else 1.4) * math.sin(2 * math.pi * t / long), pitch=(0.25 if big else 0.5) * math.sin(4 * math.pi * t / long + 0.7)), long, 0.25)}]
                + props(m, f, "fast")(long),
                "events": [{"t": 0.0, "fx": "engine_loop", "socket": eng[0], "cue": "aero_engine_loop"}]}
    C["idle"] = {"long": 2.5, "loop": True, "about": "Engines ticking over on the ground or the deck: the blades are seen going round.",
                 "tracks": props(m, f, "slow")(2.5), "events": [{"t": 0.0, "fx": "engine_loop", "socket": eng[0], "cue": "aero_engine_idle"}]}
    C["stopped"] = {"long": 0.1, "loop": False, "after": "hold", "about": "Engines stopped: blades still, no disc. For a parked machine.", "tracks": props(m, f, "dead")(0.1), "events": []}
    for nm, sign in (("bank.port", -1.0), ("bank.starboard", 1.0)):
        deg = 28.0 if big else 62.0
        C[nm] = {"long": 1.6 if big else 0.9, "loop": False, "after": "hold", "about": "Rolls into a turn and holds it (lay it over `fly`; play it backwards to roll out).",
                 "tracks": [{"part": "plane", "rot": rot_keys(lambda t, s_=sign, d=deg, L=(1.6 if big else 0.9): q_of(roll=s_ * d * smooth(t / L)), 1.6 if big else 0.9, 0.1)}], "events": []}
    if not big:
        C["peel_off"] = {"long": 1.8, "loop": False, "after": "hold", "about": "Half-rolls and pulls down into a dive (the game then flies her down).",
                         "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=-150.0 * smooth(t / 1.1), pitch=-55.0 * smooth((t - 0.5) / 1.3)), 1.8, 0.1)}], "events": []}
    # -- shooting --
    guns = [s_["name"] for s_ in m.sockets if s_["name"].startswith("gun.")]
    if guns:
        C["fire"] = {"long": 0.5, "loop": True, "about": "Her fixed guns firing: a flicker at each muzzle, and she shakes with it.",
                     "tracks": [{"part": "plane", "move": [[0.0, 0, 0, 0], [0.06, -0.012, 0, 0.004], [0.12, 0, 0, 0], [0.18, -0.012, 0, -0.004], [0.25, 0, 0, 0], [0.31, -0.012, 0, 0.004], [0.37, 0, 0, 0], [0.43, -0.012, 0, -0.004], [0.5, 0, 0, 0]]}],
                     "events": [{"t": round(0.08 * k, 2), "fx": "muzzle_flash_small", "socket": g, "cue": "aircraft_guns" if k == 0 else ""} for k in range(6) for g in guns]}
    turrets = [s_["name"] for s_ in m.sockets if s_["name"].startswith("turret.")]
    if turrets:
        C["fire.turrets"] = {"long": 0.6, "loop": True, "about": "Her gunners firing back: a flicker at each gunner's muzzle.", "tracks": [],
                             "events": [{"t": round(0.1 * k + 0.03 * i, 2), "fx": "muzzle_flash_small", "socket": g, "cue": "aircraft_gunner" if k == 0 and i == 0 else ""} for k in range(5) for i, g in enumerate(turrets)]}
    if f.has_bombs or f.role in ("bomber", "dive bomber", "attack"):
        tracks = []
        if f.has_bombs:
            tracks.append({"part": "bombs", "slide": [[0.0, 0.0], [0.5, 1.2], [1.0, 4.9], [1.5, 11.0]], "scale": [[0.0, 1.0], [1.4, 1.0], [1.5, 0.001]]})
        C["bombs_away"] = {"long": 1.5, "loop": False, "after": "hold", "about": "Her bombs leave" + (": what hangs under her drops clear and is gone (the game's falling bombs take over)." if f.has_bombs else " from her bay (the game's falling bombs are spawned at `bombs`)."),
                           "tracks": tracks + [{"part": "plane", "move": [[0.0, 0, 0, 0], [0.4, 0, 0, 0.25 if not big else 0.12], [1.5, 0, 0, 0]]}],
                           "events": [{"t": 0.0, "fx": "bombs_away", "socket": "bombs", "cue": "bomb_release"}]}
    # -- being hit --
    C["hit"] = {"long": 0.7, "loop": False, "about": "Struck: she staggers and a puff of smoke and scraps flies from her.",
                "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=9.0 * math.sin(t * 18.0) * (1 - t / 0.7), pitch=-3.0 * math.sin(t * 9.0) * (1 - t / 0.7)), 0.7, 0.05)}],
                "events": [{"t": 0.0, "fx": "hit_flash", "socket": "fuel", "cue": "aircraft_hit"}, {"t": 0.02, "fx": "debris_small", "socket": "fuel", "cue": ""}]}
    C["smoking"] = {"long": 2.0, "loop": True, "about": "Damaged but flying: a thin grey stream from an engine.", "tracks": [],
                    "events": [{"t": 0.0, "fx": "smoke_trail_thin", "socket": eng[-1], "cue": ""}]}
    # -- the three ways down (ruling 217: a loss is one model falling) --
    long = 9.0
    C["fall_burn"] = {"long": long, "loop": False, "after": "hold", "about": "Shot down burning: an engine alight, she drops a wing and goes down in a steepening dive, trailing fire and black smoke. "
                      "The game carries her down; this is her attitude, her fire and her smoke. Play `bail_out` over it if her crew get out.",
                      "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=-(18.0 * smooth(t / 2.0) + 50.0 * (t / long) ** 2 * (1.0 if not big else 0.6)), pitch=-(8.0 * smooth(t / 1.5) + 52.0 * smooth((t - 1.0) / 6.0)), yaw=10.0 * smooth(t / long)), long, 0.2)}]
                      + props(m, f, "slow")(long)[:2] + props(m, f, "fast")(long)[2:],
                      "events": [{"t": 0.0, "fx": "engine_fire", "socket": eng[0], "cue": "aircraft_fire"}, {"t": 0.0, "fx": "smoke_trail_black", "socket": eng[0], "cue": ""},
                                 {"t": 2.5, "fx": "fire", "socket": "stub.port" if len(eng) == 1 else "tank.port", "cue": ""}, {"t": 0.0, "fx": "engine_dying", "socket": eng[0], "cue": "aero_engine_dying"}]}
    # (a wing gone: she rolls hard toward the missing wing and whirls down; the wing flutters away behind)
    long = 8.0
    spin_q = lambda t: q_of(yaw=-70.0 * t * smooth(t / 1.5)) @ q_of(pitch=-72.0 * smooth(t / 1.8)) @ q_of(roll=-(200.0 if not big else 110.0) * t * smooth(t / 1.0))
    clip = {"long": long, "loop": False, "after": "hold", "about": "A wing shot off: the port wing breaks away at the root, she whirls down out of control, the wing fluttering down behind her. "
            "The game carries her down.",
            "tracks": [{"part": "plane", "rot": rot_keys(spin_q, long, 0.05)}] + props(m, f, "slow")(long),
            "events": [{"t": 0.0, "fx": "hit_flash", "socket": "break.wing.port", "cue": "aircraft_break_up"}, {"t": 0.05, "fx": "fire", "socket": "stub.port", "cue": ""},
                       {"t": 0.05, "fx": "smoke_trail_black", "socket": "stub.port", "cue": ""}, {"t": 0.1, "fx": "smoke_trail_thin", "socket": "break.wing.port", "cue": ""}]}
    drag = (26.0 if not big else 18.0)
    loose(m, clip, "wing.port", spin_q,
          lambda t: q_of(roll=40.0 * t + 260.0 * t * smooth(t / 2.0), pitch=25.0 * math.sin(t * 2.2), yaw=-95.0 * t),
          lambda t: (-drag * t - 3.0 * t * t, 1.5 * t + 4.0 * math.sin(t * 1.3), 5.0 * t + 2.2 * t * t), long, 0.05)
    C["fall_spin"] = clip
    # (blown apart: both wings and the tail fly off; the pieces tumble and fall away from each other)
    long = 6.0
    body_q = lambda t: q_of(pitch=-80.0 * smooth(t / 2.5), roll=140.0 * t, yaw=25.0 * t)
    clip = {"long": long, "loop": False, "after": "gone", "about": "Blown up: a burst of fire amidships, both wings and the tail fly off and the pieces tumble away, burning. "
            "The game carries the wreck down and takes it away at the end.",
            "tracks": [{"part": "plane", "rot": rot_keys(body_q, long, 0.05)}] + props(m, f, "dead")(long),
            "events": [{"t": 0.0, "fx": "explode", "socket": "fuel", "cue": "aircraft_explode"}, {"t": 0.0, "fx": "debris", "socket": "fuel", "cue": ""},
                       {"t": 0.1, "fx": "fire", "socket": "fuel", "cue": ""}, {"t": 0.1, "fx": "smoke_trail_black", "socket": "fuel", "cue": ""},
                       {"t": 0.15, "fx": "fire", "socket": "tank.starboard", "cue": ""}, {"t": 0.15, "fx": "smoke_trail_thin", "socket": "break.wing.port", "cue": ""},
                       {"t": 0.15, "fx": "smoke_trail_thin", "socket": "break.tail", "cue": ""}]}
    k = 1.0 if not big else 0.7
    loose(m, clip, "wing.port", body_q, lambda t: q_of(roll=-330.0 * t * k, pitch=30.0 * t, yaw=-120.0 * t * k),
          lambda t: (-22.0 * t - 2.0 * t * t, 9.0 * t * math.exp(-t / 3.0) + 3.0 * t, 6.0 * t + 2.0 * t * t), long, 0.05)
    loose(m, clip, "wing.starboard", body_q, lambda t: q_of(roll=280.0 * t * k, pitch=-50.0 * t, yaw=150.0 * t * k),
          lambda t: (-17.0 * t - 2.5 * t * t, -11.0 * t * math.exp(-t / 3.0) - 2.0 * t, 3.0 * t + 2.6 * t * t), long, 0.05)
    loose(m, clip, "tail", body_q, lambda t: q_of(pitch=420.0 * t * k, yaw=60.0 * t, roll=45.0 * t),
          lambda t: (-30.0 * t - 1.5 * t * t, 2.0 * t, 9.0 * t + 1.5 * t * t), long, 0.05)
    C["fall_explode"] = clip
    # -- her crew --
    crew = min(f.crew, 5)
    C["bail_out"] = {"long": 0.8 + 0.7 * crew, "loop": False, "about": f"Her crew take to their parachutes: {crew} leave one after another (the game spawns the Parachute model at each `chute` event).",
                     "tracks": [], "events": [{"t": round(0.5 + 0.7 * k, 2), "fx": "chute", "socket": "hatch" if (k % 2 == 0 or not big) else "hatch.belly", "cue": ""} for k in range(crew)]}


# ---- the parachute ------------------------------------------------------------------------------------

def parachute(fine=True):
    """An airman under his parachute. The root is his harness (where the lines meet); the canopy opens above it."""
    m = Model("Parachute", "Parachute and airman (our own model)", "parachute",
              {"silk": "E8E5D8", "silk2": "D5D1C2", "line": "CFCBBB", "suit": "5B5A48", "skin": "C59B7C", "boots": "2B2622", "harness": "8D8462", "helmet": "4A3A2A"}, fine)
    m.part("chute", None, (0, 0, 0), kind="root", note="the harness point, where the lines meet; the game lets it down")
    m.part("canopy", "chute", (0, 0, 0), kind="free", note="canopy and lines; scaled from a streamer to full as it opens")
    m.part("airman", "chute", (0, 0, 0), axis=(0, 1, 0), kind="turn", limits=(-25, 25), note="he swings under it")
    R, top, skirt = 3.4, 7.6, 5.4                      # the canopy's radius, the height of its crown and of its skirt
    gores = 20 if fine else 10
    m.use("canopy")
    rows = 7 if fine else 4
    rings = []
    for j in range(rows + 1):
        a = (math.pi / 2.0) * (1.0 - j / rows) * 0.96 + 0.04      # from near the crown (a vent) down to the skirt
        ring = []
        for k in range(gores * 2):
            ang = 2 * math.pi * k / (gores * 2)
            puff = 1.0 + (0.045 if k % 2 else -0.02) * math.sin(math.pi * j / rows) + (0.03 if k % 2 else -0.03) * (j / rows) ** 3
            r = R * math.cos(a) * puff
            ring.append(Vector((r * math.cos(ang), r * math.sin(ang), skirt + (top - skirt) * math.sin(a))))
        rings.append(ring)
    n = gores * 2
    verts = [p for ring in rings for p in ring]
    faces = [(r * n + k, r * n + (k + 1) % n, (r + 1) * n + (k + 1) % n, (r + 1) * n + k) for r in range(rows) for k in range(n)]
    mid = Vector((0, 0, skirt + 0.5))
    m.add(verts, faces, lambda c, nn: "silk" if int((math.atan2(c.y, c.x) % (2 * math.pi)) / (2 * math.pi / gores)) % 2 == 0 else "silk2", True, lambda c: c - mid)
    for k in range(gores):
        ang = 2 * math.pi * (k + 0.0) / gores
        foot = rings[-1][(2 * k) % n]
        if fine or k % 2 == 0:
            m.rod(foot, Vector((0.16 * math.cos(ang), 0.16 * math.sin(ang), 0.25)), 0.012, "line", sides=3, caps=(False, False))
    # the airman: hanging in his harness, arms up to the risers
    m.use("airman")
    m.ball((0, 0, -0.42), (0.17, 0.2, 0.33), "suit", 10, 6)                    # body
    m.ball((0, 0, -0.02), (0.105, 0.1, 0.12), "helmet", 10, 6)                 # head
    m.ball((0.07, 0, -0.03), (0.06, 0.075, 0.085), "skin", 8, 5)               # face
    for sgn in (1.0, -1.0):
        m.rod((0, 0.2 * sgn, -0.2), (0.04, 0.2 * sgn, 0.24), 0.05, "suit", 0.04, sides=6)       # arm up
        m.rod((0.04, 0.2 * sgn, 0.24), (0.02, 0.17 * sgn, 0.27), 0.04, "skin", sides=5)
        m.rod((0.0, 0.1 * sgn, -0.7), (0.12, 0.11 * sgn, -1.12), 0.075, "suit", 0.06, sides=6)   # thigh
        m.rod((0.12, 0.11 * sgn, -1.12), (0.02, 0.11 * sgn, -1.52), 0.06, "suit", 0.05, sides=6)  # shin
        m.rod((0.02, 0.11 * sgn, -1.52), (0.16, 0.11 * sgn, -1.58), 0.05, "boots", sides=5)       # boot
        m.rod((0.0, 0.17 * sgn, -0.25), (0.0, 0.16 * sgn, 0.25), 0.014, "harness", sides=3)       # riser
    m.box((-0.17, 0, -0.38), (0.12, 0.3, 0.4), "harness")                       # the pack on his back
    m.socket("feet", (0.05, 0, -1.6), -Z, part="airman", note="where he touches down")
    m.socket("crown", (0, 0, top), Z, part="canopy")
    C = m.clips
    C["open"] = {"long": 1.8, "loop": False, "after": "hold", "about": "The parachute streams, fills and steadies. Start it as he leaves the machine.",
                 "tracks": [{"part": "canopy", "scale": [[0.0, 0.05, 0.05, 0.55], [0.5, 0.1, 0.1, 1.25], [0.9, 0.55, 0.55, 1.12], [1.15, 1.1, 1.1, 0.93], [1.45, 0.96, 0.96, 1.03], [1.8, 1.0, 1.0, 1.0]]},
                            {"part": "airman", "turn": [[0.0, 22.0], [0.9, 16.0], [1.25, -12.0], [1.8, 5.0]]}],
                 "events": [{"t": 1.0, "fx": "chute_crack", "socket": "crown", "cue": "parachute_open"}]}
    C["drift"] = {"long": 5.0, "loop": True, "about": "Coming down: he swings gently and the canopy breathes. The game lets him down at about 6 m a second, drifting with the wind.",
                  "tracks": [{"part": "airman", "turn": [[0.0, 5.0], [1.25, 0.0], [2.5, -5.0], [3.75, 0.0], [5.0, 5.0]]},
                             {"part": "canopy", "scale": [[0.0, 1.0, 1.0, 1.0], [1.25, 1.02, 1.02, 0.985], [2.5, 1.0, 1.0, 1.0], [3.75, 0.985, 0.985, 1.012], [5.0, 1.0, 1.0, 1.0]]},
                             {"part": "chute", "rot": rot_keys(lambda t: q_of(roll=3.0 * math.sin(2 * math.pi * t / 5.0), pitch=2.0 * math.sin(2 * math.pi * t / 5.0 + 1.3)), 5.0, 0.25)}],
                  "events": []}
    C["collapse"] = {"long": 2.5, "loop": False, "after": "hold", "about": "Down: the canopy spills and settles on the sea or the ground.",
                     "tracks": [{"part": "canopy", "scale": [[0.0, 1.0, 1.0, 1.0], [0.8, 1.15, 1.2, 0.5], [2.5, 1.3, 1.35, 0.06]], "move": [[0.0, 0, 0, 0], [2.5, 2.5, 0.6, -1.2]]}],
                     "events": [{"t": 0.0, "fx": "splash_small", "socket": "feet", "cue": ""}]}
    return m


# ---- the bomb -----------------------------------------------------------------------------------------

def bomb(fine=True):
    """A general-purpose bomb as it falls: a 250 kg case with its tail fins in a ring."""
    m = Model("Bomb", "Bomb (our own model)", "bomb", {"case": "3A3D38", "band": "A0803A", "fin": "2E302C"}, fine)
    m.part("bomb", None, (0, 0, 0), kind="root", note="the game drops it; the clip tips its nose down")
    sides = 14 if fine else 8
    prof = [(0.82, 0.0), (0.74, 0.07), (0.6, 0.14), (0.4, 0.175), (-0.1, 0.18), (-0.45, 0.13), (-0.8, 0.05)]
    rings = [[Vector((x, r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides))) for k in range(sides)] for x, r in prof]
    m.skin(rings, lambda c, n: "band" if 0.42 < c.x < 0.56 else "case", caps=(True, True))
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        d = Vector((0, math.cos(a), math.sin(a)))
        m.add([Vector((-0.42, 0, 0)) + d * 0.1, Vector((-0.86, 0, 0)) + d * 0.04, Vector((-0.86, 0, 0)) + d * 0.2, Vector((-0.6, 0, 0)) + d * 0.2], [(0, 1, 2, 3)], "fin", False)
    ring = [[Vector((x, 0.2 * math.cos(2 * math.pi * k / sides), 0.2 * math.sin(2 * math.pi * k / sides))) for k in range(sides)] for x in (-0.7, -0.86)]
    m.skin(ring, "fin")
    m.socket("nose", (0.82, 0, 0), X, note="it bursts here")
    m.clips["fall"] = {"long": 6.0, "loop": False, "after": "hold", "about": "Released level, its nose drops as it falls (the game carries it forward and down).",
                       "tracks": [{"part": "bomb", "rot": rot_keys(lambda t: q_of(pitch=-78.0 * smooth(t / 5.0), roll=40.0 * t), 6.0, 0.25)}], "events": []}
    return m


BUILDERS = {n: (lambda fine=True, n=n: convert(n, fine)) for n in ("Bf109", "Spitfire", "Zero", "Mustang", "Yak", "Fw190", "Hurricane", "Warhawk", "Fighter",
                                                                  "Stuka", "Sturmovik", "He111", "Lancaster", "B17", "Bomber")}
