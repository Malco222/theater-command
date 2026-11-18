"""Triangle counts of every model at LOD0/1/2 (no rendering)."""
import sys
sys.argv = [sys.argv[0]]
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import guns_vehicles as gv
for k in ("gun", "ammo", "lorry", "hanomag", "m3"):
    u = gv.make_unit(k)
    print(k, [u.geo(l).tris() for l in (0, 1, 2)])
