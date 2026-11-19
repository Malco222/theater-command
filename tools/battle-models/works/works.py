"""Every work by name: BUILDERS[name](fine=True) -> rigid.Model."""
import os
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import importlib  # noqa: E402

BUILDERS = {}
for _name in ("posts", "guns", "concrete", "obstacles", "dug", "pits", "bunkers", "towers"):
    try:
        _mod = importlib.import_module(_name)
    except ModuleNotFoundError as e:
        if e.name != _name:
            raise
        continue
    except Exception:  # noqa: BLE001  (one module mid-edit must not stop the others' pictures)
        print(f"works: module {_name} did not load:", file=sys.stderr)
        traceback.print_exc()
        continue
    BUILDERS.update(_mod.BUILDERS)
