"""The recipe (fx.json) flattened for the preview: one line per number, "path<TAB>value", and per word,
"path<TAB>=word". Paths are dotted, with a list's places as numbers, as the game's reader takes them.

    python3 flatten.py ../fx.json > recipe.txt
"""
import json
import sys


def walk(v, path, out):
    if isinstance(v, dict):
        for k, x in v.items():
            walk(x, f"{path}.{k}" if path else k, out)
    elif isinstance(v, list):
        for i, x in enumerate(v):
            walk(x, f"{path}.{i}", out)
    elif isinstance(v, bool):
        out.append(f"{path}\t{1 if v else 0}")
    elif isinstance(v, (int, float)):
        out.append(f"{path}\t{v!r}")
    elif isinstance(v, str) and "\n" not in v and "\t" not in v:
        out.append(f"{path}\t={v}")


lines = []
walk(json.load(open(sys.argv[1])), "", lines)
print("\n".join(lines))
