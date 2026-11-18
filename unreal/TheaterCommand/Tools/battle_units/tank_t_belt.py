"""Tank helper: checks the pictures' moving belt (links travel the right way and the drive loop closes):  python tank_t_belt.py"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sys, time, os
import tank, numpy as np
st = tank.Stage(ground=False)
st.play("drive")
ob = st.belts.objs[0][0]
def co():
    a = np.zeros(len(ob.data.vertices) * 3, dtype=np.float32); ob.data.vertices.foreach_get("co", a); return a.reshape(-1, 3)
st.sc.frame_set(0); a = co()
st.sc.frame_set(1); b = co()
st.sc.frame_set(0); c = co()
d = b - a
print("travel per frame", st.clips["drive"].travel(1 / 30))
low = a[:, 2] < 0.12; top = (a[:, 2] > 0.7) & (np.abs(a[:, 0]) < 1.5)
print("bottom dx mean", d[low, 0].mean(), "dz", np.abs(d[low, 2]).max())
print("top dx mean", d[top, 0].mean())
print("frame0 repeat same:", np.abs(c - a).max())
# loop seam: frame 60 vs frame 0 positions as sets (links shift by 21)
st.sc.frame_set(60); e = co()
nv = len(st.belts.tv)
A = a.reshape(tank.PATH.links, nv, 3); E = e.reshape(tank.PATH.links, nv, 3)
print("loop: min over shifts", min(np.abs(np.roll(E, s, 0) - A).max() for s in range(tank.PATH.links)))
