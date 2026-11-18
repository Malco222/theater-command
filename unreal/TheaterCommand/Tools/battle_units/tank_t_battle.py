"""Tank helper: the battle-stage clips. Dry run (build, step the effects, print the course):  python tank_t_battle.py hero
Test frames at a size:  python tank_t_battle.py hero 0,120,121 960x540x8   (or ko for the knocked_out stage); frames land in WORK/clip_<name>/"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sys, time, os
import tank, kit, bpy
from mathutils import Vector
tank.FAST = True
which = sys.argv[1]
frames = [int(f) for f in sys.argv[2].split(",")] if len(sys.argv) > 2 and sys.argv[2] else None
size = sys.argv[3].split("x") if len(sys.argv) > 3 else None
if frames is None:
    # dry run: build only
    orig = tank.BattleScene.render
    def dry(self, out_name, w, h, samples, frames=None):
        print("streams", len(self.fx.streams), "shots", len(self.fx.shots), "frame_end", self.st.sc.frame_end)
        for f in range(0, self.st.sc.frame_end + 1):
            self.now = f / tank.FPS
            self.st.sc.frame_set(f)
            self.fx.at(self.now)
        print("live puffs at end", len(self.fx.live), "objects", len(self.fx.coll.objects))
        for t in (0.0, 2.0, 3.4, 4.0, 6.0, 8.0):
            print(t, "s", round(self.course.s(t), 2), "at", tuple(round(v, 2) for v in self.course.matrix(t).translation))
        print("muzzle at fire", self.socket("socket_muzzle", tank.T_FIRE))
        return None
    tank.BattleScene.render = dry
kw = {}
if size:
    kw = dict(w=int(size[0]), h=int(size[1]), samples=int(size[2]))
t = time.time()
(tank.hero if which == "hero" else tank.ko_stage)(frames=frames, **kw)
print("total", round(time.time() - t, 1))
