"""The gun's firing film, or a few frames of it: gv_t_film.py <w> <h> <samples> <frames, comma separated | all> [tag]"""
import sys
args = sys.argv[1:]
sys.argv = [sys.argv[0]]
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import guns_vehicles as gv
w, h, n = int(args[0]), int(args[1]), int(args[2])
frames = [int(x) for x in args[3].split(",")] if len(args) > 3 and args[3] != "all" else None
gv.gun_film(gv.make_unit("gun"), w=w, h=h, samples=n, frames=frames, tag=args[4] if len(args) > 4 else "firing_test")
