"""Several works stood together on one piece of ground, for the pictures and the clips."""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Quaternion, Vector  # noqa: E402

import ground  # noqa: E402
import look  # noqa: E402
import rigid  # noqa: E402
import works  # noqa: E402


# (works whose own file does not say what stands at their sockets)
TAKES = {"FlakPit": {"gun": "HeavyFlak"}, "SandbagRing": {"gun": "LightFlak"}}


class Item:
    def __init__(self, name, m, ob, arm, at, yaw):
        self.name, self.m, self.ob, self.arm, self.at, self.yaw = name, m, ob, arm, at, yaw
        self.pose = {}

    def set(self, *poses):
        """Lays poses over each other and sets the work so (parts not named go back to rest)."""
        self.pose = rigid.merge(*poses) if poses else {}
        rigid.apply_pose(self.arm, self.pose, model=self.m)
        return self.pose

    def clip(self, name, t):
        c = self.m.clips.get(name)
        return rigid.pose_of(self.m, c, max(0.0, min(t, c["long"])) if not c.get("loop") else t) if c else {}

    def aim(self, part, degrees):
        return rigid.aim(self.m, part, degrees)

    def where(self, socket):
        """A socket's place and facing on the ground as the work is posed now."""
        bpy.context.view_layer.update()
        return look.where(self.m, self.arm, self.pose, socket)

    def has(self, socket):
        return any(s["name"] == socket for s in self.m.sockets)


class Set:
    def __init__(self):
        self.items = []

    def put(self, name, at=(0.0, 0.0, 0.0), yaw=0.0, rest="idle", mount=True, **kw):
        at = tuple(at) + (0.0,) * (3 - len(at))
        m = works.BUILDERS[name](True, **kw)
        ob, arm = look.stand(m, at, yaw)
        it = Item(name, m, ob, arm, at, yaw)
        if rest and rest in m.clips:
            it.set(it.clip(rest, 0.0))
        self.items.append(it)
        # what stands at its sockets as a model of its own (a flak pit's gun, a tower's guns)
        it.mounted = {}
        if mount:
            for socket, what in {**TAKES.get(name, {}), **m.info.get("takes", {})}.items():
                if what in works.BUILDERS and it.has(socket):
                    p, d = it.where(socket)
                    it.mounted[socket] = self.put(what, tuple(p), math.degrees(math.atan2(d.y, d.x)), mount=False, **({"nation": kw["nation"]} if "nation" in kw else {}))
        return it

    def lay(self, **kw):
        """The ground under them all: each work's holes, trodden earth and bare patches where it stands."""
        digs, worn, clear, paths, tracks = [], [], [], [], []
        for it in self.items:
            if it.at[2] > 0.5:                       # (what stands up on another work wears no ground)
                continue
            xy = (it.at[0], it.at[1])
            c, s = math.cos(math.radians(it.yaw)), math.sin(math.radians(it.yaw))
            digs += [ground.placed(d, xy, it.yaw) for d in it.m.info.get("dig", [])]
            clear += [ground.placed(d, xy, it.yaw) for d in it.m.info.get("clear", [])]
            for w in list(it.m.info.get("worn", [])) + ground.skirt(it.m):
                worn.append((xy[0] + w[0] * c - w[1] * s, xy[1] + w[0] * s + w[1] * c) + tuple(w[2:]))
            paths += [ground.placed(p, xy, it.yaw) for p in ground.paths_of(it.m, seed=len(paths))]
            tracks += [ground.placed(p, xy, it.yaw) for p in ground.tracks_of(it.m)]
        worn += list(kw.pop("worn", []))
        paths += list(kw.pop("paths", []))
        tracks += list(kw.pop("tracks", []))
        self.ground = ground.Ground(digs=digs, worn=worn, clear=clear, paths=paths, tracks=tracks, **kw)
        return self.ground


def ease(t, t0, t1):
    """0 before t0, 1 after t1, a smooth run between."""
    if t1 <= t0:
        return 1.0 if t >= t1 else 0.0
    k = min(max((t - t0) / (t1 - t0), 0.0), 1.0)
    return k * k * (3 - 2 * k)


def keys(pairs, t):
    """A value along (time, value) pairs, eased from each to the next."""
    if t <= pairs[0][0]:
        return pairs[0][1]
    for (ta, a), (tb, b) in zip(pairs, pairs[1:]):
        if t <= tb:
            return a + (b - a) * ease(t, ta, tb)
    return pairs[-1][1]
