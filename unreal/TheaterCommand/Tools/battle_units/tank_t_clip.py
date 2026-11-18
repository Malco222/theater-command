"""Tank helper: test frames of a studio clip:  python tank_t_clip.py drive 0,15,30 [x:y:z:dist:pitch:yaw:lens] [tag]  -> WORK/c_<clip><tag>_<frame>.png"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sys, time, os
import tank, kit, bpy
from mathutils import Vector
tank.FAST = True
name = sys.argv[1]
TAG = sys.argv[4] if len(sys.argv) > 4 else ''
frames = [int(f) for f in sys.argv[2].split(",")]
st = tank.Stage()
st.play(name)
tank.aim(st.cam, tank.CLIP_VIEWS[name] if len(sys.argv) < 4 else tuple(
    [tuple(float(v) for v in sys.argv[3].split(":")[:3])] + [float(v) for v in sys.argv[3].split(":")[3:]]))
tank.picture_fx(st, name)
c = st.clips[name]
tank.ground_motion(st, c)
tank.render_cfg()
for f in frames:
    st.sc.frame_set(f)
    t = time.time()
    kit.still(tank.WORK + f"c_{name}{TAG}_{f:03d}.png")
    print(f, round(time.time() - t, 1), flush=True)
