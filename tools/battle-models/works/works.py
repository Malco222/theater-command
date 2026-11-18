"""Every work by name: BUILDERS[name](fine=True) -> rigid.Model."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import importlib  # noqa: E402

BUILDERS = {}
for _name in ("posts", "obstacles", "concrete", "guns", "dug"):
    try:
        _mod = importlib.import_module(_name)
    except ModuleNotFoundError as e:
        if e.name != _name:
            raise
        continue
    BUILDERS.update(_mod.BUILDERS)
