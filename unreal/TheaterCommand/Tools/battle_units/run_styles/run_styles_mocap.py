"""The run styles' motion capture: more CMU running takes, cut into loops on the reference man the way mocap.py cuts
its own (make_loop, fit_arms, ease_spikes), written in clips.json's form to run_styles_clips.json. Run under bpyenv
(human.build makes the reference man's soles for the ground and the foot lock).

    python mocap_cmu/fetch_cmu.py 35_18 09_02 16_36 09_03     (the takes, into LOOKS_TOOLS/mocap_cmu/bvh)
    LOOKS_TOOLS=... bpyenv/bin/python run_styles_mocap.py [names...]
"""
import json
import os
import sys

import numpy as np

LOOKS = os.environ.get("LOOKS_TOOLS", "/home/user/work/looks/battle_units")
sys.path.insert(0, LOOKS)
import mocap  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "run_styles_clips.json")
REST_REF = os.path.join(LOOKS, "mocap_cmu", "rest_ref.json")
CROUCH = (0.14, 28)          # (the advance's, as mocap.py lays it over the run)
STYLES = [
    dict(name="run_a", take="35_18", loop=True, head=0, ground=10, note="Subject 35, run/jog (the file's own run: a check)"),
    dict(name="run_b", take="09_02", loop=True, head=0, ground=10, note="Subject 9, run: a longer, bouncier stride"),
    dict(name="run_c", take="16_36", loop=True, head=0, ground=10, note="Subject 16, run/jog: shorter, quicker steps"),
    dict(name="run_d", take="09_03", loop=True, head=0, ground=10, note="Subject 9, run (another take): high knees"),
]
# (looked at and left out: 16_35, subject 16 again; 02_03, no clean footfalls; 38_03 and 127_03, turning, no steady
# stretch; 143_01, a sprint too short to loop)
STYLES += [dict(s, name=s["name"].replace("run_", "advance_"), head=None, crouch=CROUCH,
                note=s["note"] + ", with the advance's crouch laid over it") for s in list(STYLES)]


def build(names=None):
    import kit
    import human
    kit.fresh(mocap.FPS)
    man = human.build(seed=0, nation="GER", stand=0.02, curl=(30, 45, 25))
    rest = mocap.Rest.of(man["armature"], man["body"])
    data = json.load(open(OUT)) if os.path.exists(OUT) else {}
    for spec in STYLES:
        if names and spec["name"] not in names:
            continue
        m, info = mocap.make_loop(spec, rest, verbose=True)
        info["fit_arms_deg"] = mocap.fit_arms(m, loop=True)
        info["eased_deg_s"] = mocap.ease_spikes(m, loop=True)
        c = mocap.clip_data(m, info, spec)
        c["build"] = {k: v for k, v in info.items() if k != "contacts"}
        c["build"]["ik"] = {k: (v if not isinstance(v, dict) else v.get("ik_err_max")) for k, v in info.get("contacts", {}).items()}
        data[spec["name"]] = c
    data["_about"] = dict(mocap.ABOUT, made="run_styles_mocap.py: more CMU running takes, cut as mocap.py cuts its own")
    rr = json.load(open(REST_REF))
    data["_rest"] = {"names": rr["names"], "parent": rr["parent"], "R": rr["R"],
                     "about": "the reference man's rest (mocap_cmu/rest_ref.json): bone order (parents first), parent "
                              "names, rest turns (Blender's axes)"}
    with open(OUT + ".part", "w") as f:
        json.dump(data, f, separators=(",", ":"))
    os.replace(OUT + ".part", OUT)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    build(args or None)
