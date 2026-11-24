"""landlook.py on the Eastern Front's grounds: `ground=winter` stands her on snow, washed white (wash=0 keeps her summer coat).

    python eastlook.py KV1 <out> front,side 16 960x540 ground=winter [wash=0.85] [snow=0.8] [turret=cast]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import winter  # noqa: E402

opt = dict(a.split("=", 1) for a in sys.argv[6:] if "=" in a)
winter.install(wash=float(opt.get("wash", 0.85)), snow=float(opt.get("snow", 0.8)))
import vehicles  # noqa: E402

_build = vehicles.build


def build(name, fine=True, tag=None, whole=False, **choice):
    if "turret" in opt and not whole:                    # (the KV-1's turret: welded or cast)
        choice.setdefault("turret", opt["turret"])
    return _build(name, fine, tag, whole, **choice)


vehicles.build = build
import landlook  # noqa: E402,F401  (it runs on import)
