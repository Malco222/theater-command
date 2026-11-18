"""Tank helper: a quick look at one LOD:  python tank_t_look.py <lod> <yaw> <pitch> <dist> <out.png> [tx,ty,tz]"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sys, time, os
import tank
a = sys.argv[1:]
lod = int(a[0]); yaw = float(a[1]); pitch = float(a[2]); dist = float(a[3]); out = a[4]
tgt = tuple(float(v) for v in a[5].split(",")) if len(a) > 5 else (0, 0, 1.1)
t = time.time()
tank.quick_look(out, lod, yaw=yaw, pitch=pitch, dist=dist, target=tgt)
print("took", round(time.time() - t, 1))
