"""Fetch the CMU motion-capture takes the soldier's clips are made from (mocap.CLIPS), as BVH, into mocap_cmu/bvh/.

    python fetch_cmu.py            the takes mocap.py uses
    python fetch_cmu.py 02_01 ...  any others

The BVH files are the 2010 MotionBuilder-friendly conversion of the CMU Graphics Lab Motion Capture Database by
B. Hahne (cgspeed.com), mirrored on GitHub by una-dinosauria/cmu-mocap. CMU: "This data is free for use in research
and commercial projects worldwide." Credit (see CREDITS.md): "The data used in this project was obtained from
mocap.cs.cmu.edu. The database was created with funding from NSF EIA-0196217."

The files are not kept in the repository (about 0.5 to 4 MB each): this script brings them back.
"""

import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "bvh")
URL = "https://raw.githubusercontent.com/una-dinosauria/cmu-mocap/master/data/{s:03d}/{t}.bvh"
TAKES = ["35_01", "35_18", "77_02", "21_06", "136_09", "77_16", "140_01", "90_18", "77_19", "79_73", "77_09", "111_03"]


def fetch(takes=TAKES, out=OUT):
    os.makedirs(out, exist_ok=True)
    for t in takes:
        dst = os.path.join(out, t + ".bvh")
        if os.path.exists(dst) and os.path.getsize(dst) > 1000:
            continue
        url = URL.format(s=int(t.split("_")[0]), t=t)
        print("fetch", url, flush=True)
        with urllib.request.urlopen(url, timeout=60) as r:
            data = r.read()
        if not data.startswith(b"HIERARCHY"):
            raise RuntimeError(f"{t}: not a BVH file")
        with open(dst + ".part", "wb") as f:
            f.write(data)
        os.replace(dst + ".part", dst)
    return out


if __name__ == "__main__":
    fetch(sys.argv[1:] or TAKES)
