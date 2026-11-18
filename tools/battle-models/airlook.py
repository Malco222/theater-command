"""A close look at one aircraft, for judging her detail.

    python airlook.py <name> <out folder> [view,view,...] [WxHxSPP] [clip=name@t ...] [record=JSON] [tag=word] [scheme=YYYY-MM] [owner=<unit-table.json>#TAG]

views:  front   from ahead and to port, a little above (the sheet's view, closer)
        hood    close on her hood and nose, from ahead and above
        rear    from astern and to starboard, above
        over    from astern and above, a little to port (for a bank, the flaps, the control surfaces)
        under   from below and ahead
        tail, waist, belly   close on a bomber's tail guns, her waist windows, her ball turret
        side    square on her port side
        quarter her whole port side from astern and above: her wing, side and fin marks together
        record  close on the record painted on her port side (kills, missions); rudder: on a German fighter's rudder;
                nose: a bomber's port nose from a little further off; ace: a fighter's pilot and his score together
record= is what she has done, e.g. record='{"kills": [["GER", 14], ["ITA", 3]], "missions": 32, "lead": [12, 19]}'
(see marks.record); tag= is added to the pictures' names; scheme=YYYY-MM shows her as she is in that month: her coat of paint and her markings then.
owner=<unit-table.json>#TAG shows a plain machine as that country flies her: its paint and paint2, its insignia, its rudder flag.
A clip named after the views is laid over `fly` at the time given (bank.port@0.9, watch@2.0, flaps.down@1.5).
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402

import kit  # noqa: E402
import stage  # noqa: E402
from mathutils import Vector  # noqa: E402

from airshow import Air, Plane  # noqa: E402


def main():
    name, out = sys.argv[1], sys.argv[2]
    views = sys.argv[3].split(",") if len(sys.argv) > 3 else ["front", "hood", "rear", "under"]
    w, h, spp = (int(v) for v in (sys.argv[4] if len(sys.argv) > 4 else "1280x720x12").split("x"))
    extra = [a.split("=", 1)[1] for a in sys.argv[5:] if a.startswith("clip=")]
    rec = next((json.loads(a.split("=", 1)[1]) for a in sys.argv[5:] if a.startswith("record=")), None)
    tag = next(("-" + a.split("=", 1)[1] for a in sys.argv[5:] if a.startswith("tag=")), "")
    os.makedirs(out, exist_ok=True)
    A = Air(clouds=12.0)
    stage.render_setup(w, h, spp)
    scheme = next((a.split("=", 1)[1] for a in sys.argv[5:] if a.startswith("scheme=")), None)
    owner = next((a.split("=", 1)[1] for a in sys.argv[5:] if a.startswith("owner=")), None)      # (owner=<unit-table.json>#TAG: a plain machine in that country's coat, marking and rudder flag)
    if owner:
        table, country = owner.split("#")
        owner = json.load(open(table))["countries"][country]
    p = Plane(A, name, record=rec, scheme=scheme, owner=owner)
    for c in extra:
        if c.startswith("load."):                        # (clip=load.bombs@0: she is seen with that load, not her usual one)
            p.carry(c.split("@")[0][5:])
    extra = [c for c in extra if not c.startswith("load.")]
    poses = [p.play("fly", 0.6)] + [p.play(c.split("@")[0], float(c.split("@")[1])) for c in extra]
    p.put((0, 0, 0), *poses)
    A.at(0.0)
    s, L = p.f.half * 2.0, p.f.long
    hood = next((k["at"] for k in p.m.sockets if k["name"] == "hatch"), None)
    hx, hz = (hood.x, hood.z) if hood is not None else (0.0, 1.0)
    shots = {"front": ((s * 0.62, s * 0.56, s * 0.3), (L * 0.02, 0, 0.0), 50.0),
             "hood": ((hx + 3.4 + L * 0.12, 2.6 + L * 0.06, hz + 1.5), (hx + 0.2, 0, hz - 0.45), 50.0),
             "rear": ((-s * 0.62, -s * 0.5, s * 0.34), (-L * 0.08, 0, 0.1), 50.0),
             "over": ((-s * 0.55, s * 0.25, s * 0.6), (0, 0, 0.0), 50.0),
             "tail": ((p.f.hull.back - 3.6, 3.3, 1.5), (p.f.hull.back + 1.3, 0, 0.45), 50.0),
             "waist": ((-L * 0.22 - 3.2, 6.4, 1.5), (-L * 0.22, 0, 0.2), 50.0),
             "belly": ((-L * 0.17 + 4.6, 5.2, -3.4), (-L * 0.17, 0, -0.9), 50.0),
             "under": ((s * 0.5, s * 0.42, -s * 0.4), (0, 0, -0.1), 50.0),
             "side": ((0.0, s * 1.0, 0.6), (-L * 0.06, 0, 0.2), 50.0),
             "quarter": ((-s * 0.7, s * 0.6, s * 0.32), (-L * 0.1, 0, 0.15), 35.0)}      # (her whole port side from astern: wing, side and fin marks together)
    back = p.f.hull.back
    front = p.f.hull.front
    shots["glass"] = ((front + 3.2, 3.6, 1.7), (front - 1.7, 0, 0.25), 50.0)          # (into a glass nose, at the men in it)
    shots["tailside"] = ((back + 0.9, 3.4, 0.75), (back + 0.75, 0, 0.36), 50.0)      # (abeam of a tail gunner's windows)
    for k, c, radii in p.f.turrets:                      # (straight in at the window of a ball turret, from where its guns point)
        if k in p.f.balls:
            d = sum(((b - a) for a, b in p.f.guns_of.get(k, [])), Vector()).normalized()
            shots["ball"] = (tuple(c + d * 2.6 + Vector((0.0, 0.9, 0.35))), tuple(c), 50.0)
    t = getattr(p.m, "tally", None) or {}
    slots = [Vector((q[0], -q[1], q[2])) for k in ("missions", "lead", "kills") for q in t.get(k, {}).get("slots", [])]
    if slots:                                            # (her record: abeam of it and a little above, so that the rows are read)
        mid = sum(slots, Vector()) / len(slots)
        wide = max(q.x for q in slots) - min(q.x for q in slots)
        d = max(2.3, wide * 1.45)
        shots["record"] = ((mid.x + 0.12 * d, mid.y + 0.9 * d, mid.z + 0.42 * d), tuple(mid), 50.0)
        shots["nose"] = ((mid.x + 1.6 + 0.45 * d, mid.y + 1.9 * d, mid.z + 0.8 * d), (mid.x + 0.6, mid.y, mid.z), 50.0)
        shots["ace"] = ((mid.x + 1.9, mid.y + 3.0, mid.z + 1.5), (mid.x - 0.1, mid.y, mid.z + 0.12), 50.0)      # (the pilot and his score together)
    bars = [Vector((q[0], -q[1], q[2])) for q in t.get("rudder", {}).get("slots", [])]
    if bars:
        mid = sum(bars, Vector()) / len(bars)
        shots["rudder"] = ((mid.x - 0.7, mid.y + 2.6, mid.z + 0.7), tuple(mid), 50.0)
    for v in views:
        eye, look, lens = shots[v]
        A.cam.data.lens = lens
        kit.look_at(A.cam, eye, look)
        path = os.path.join(out, f"{name}-{v}{tag}.png")
        kit.still(path)
        print("made", path, flush=True)


main()
