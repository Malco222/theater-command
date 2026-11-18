"""A quick look at one model of guns_vehicles.py: gv_t_look.py <model> "<view>;<view>" <w> <h> <samples> [rest | <clip>@<frame>] [tag]
Views are the model's own (front, rear, detail, game) or a tuple ((x, y, z), distance, pitch, yaw, lens). Writes
$GV_OUT/look_<Name>_<tag>_<view>.png (GV_OUT defaults to /home/user/art/guns/)."""
import sys, time
args = sys.argv[1:]
sys.argv = [sys.argv[0]]
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import guns_vehicles as gv
import kit
unit = gv.make_unit(args[0])
views = args[1].split(";")
w, h, n = int(args[2]), int(args[3]), int(args[4])
pose = args[5] if len(args) > 5 else None
st = gv.Stage(unit)
if pose and pose != "rest":
    st.play(pose.split("@")[0])
    st.sc.frame_set(int(pose.split("@")[1]))
else:
    st.rest(unit.picture_pose())
gv.render_cfg(w, h, n)
for v in views:
    t = time.time()
    view = unit.views()[v] if v in unit.views() else eval(v)
    gv.aim(st.cam, view)
    tag = (args[6] + "_" if len(args) > 6 else "") + (v[:12] if v in unit.views() else "custom")
    kit.still(os.path.join(os.environ.get("GV_OUT", "/home/user/art/guns/"), f"look_{unit.name}_{tag}.png"))
    print(v, round(time.time() - t, 1), "s", flush=True)
