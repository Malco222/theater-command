"""Tank helper: stills of the resting tank from given views:  python tank_t_view.py name:tx:ty:tz:dist:pitch:yaw:lens ...  -> WORK/v_<name>.png"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sys, time, os
import tank, kit
tank.FAST = True
st = tank.Stage()
st.rest()
tank.render_cfg()
for spec in sys.argv[1:]:
    name, tx, ty, tz, dist, pitch, yaw, lens = spec.split(":")
    tank.aim(st.cam, ((float(tx), float(ty), float(tz)), float(dist), float(pitch), float(yaw), float(lens)))
    t = time.time(); kit.still(tank.WORK + f"v_{name}.png"); print(name, round(time.time() - t, 1))
