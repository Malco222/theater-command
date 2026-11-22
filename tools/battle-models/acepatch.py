"""Patch another battle-models tree (one whose planes.py is its own: the Yak's, the C.202's, the what-ifs') for an ace's name line (acepanel.py) and her pilot kept inside her skin (crewfit.py), both copied in. Idempotent. usage: python3 acepatch.py <tree>"""
import os, shutil, sys
tree = sys.argv[1]
here = os.path.dirname(os.path.abspath(__file__))

def edit(path, old, new, count=1):
    s = open(path).read()
    if new in s:
        return "already"
    n = s.count(old)
    assert n == count, (path, old[:60], n)
    open(path, "w").write(s.replace(old, new))
    return "patched"

P = os.path.join(tree, "planes.py")
print("planes per_row", edit(P, "    per_row = TALLY_ROW.get(name, per_row)\n",
    "    per_row = TALLY_ROW.get(name, per_row)\n"
    "    if role == \"fighter\":\n"
    "        import acepanel\n"
    "        per_row = acepanel.ROW.get(name, per_row)\n"))
print("planes name", edit(P, "    z = top\n    bombs, stars = [], []\n",
    "    z = top\n"
    "    if role == \"fighter\":                                  # (an ace's name: one line where her first row of kills was, the rows beneath it; acepanel.py)\n"
    "        out[\"name\"], z = acepanel.line(name, x0, xs_kill[-1] - r_kill, top, side, down)\n"
    "    bombs, stars = [], []\n"))
print("planes doc", edit(P, "each nation's own habit, never a party emblem, no names, nose art or\n",
    "each nation's own habit, never a party emblem, no names (but an ace's own: acepanel.py, 2026-11-22), nose art or\n"))

M = os.path.join(tree, "marks.py")
s = open(M).read()
NAME = ("    if rec.get(\"name\") and \"name\" in t:                # (an ace's name over his kills: acepanel.py)\n"
        "        import acepanel\n"
        "        out += acepanel.marks(t[\"name\"], rec[\"name\"], rec.get(\"ink\") or acepanel.ink_under(model, t[\"name\"]))\n")
if NAME not in s:
    a = s.index("\ndef record(")
    b = s.index("\n    return out\n", a)
    # (the last `return out` of record: the one just before the next top-level def)
    nxt = s.index("\ndef ", a + 5)
    while True:
        c = s.find("\n    return out\n", b + 1)
        if c == -1 or c > nxt:
            break
        b = c
    s = s[:b + 1] + NAME + s[b + 1:]
    print("marks record patched")
else:
    print("marks record already")
old = "else layers(tag) if isinstance(tag, list) else shapes(tag)"
new = "else layers(tag) if isinstance(tag, list) else mk[\"drawn\"] if \"drawn\" in mk else shapes(tag)"
if new not in s:
    assert s.count(old) == 1, s.count(old)
    s = s.replace(old, new)
    print("marks put patched")
open(M, "w").write(s)

C = os.path.join(tree, "codes.py")
if os.path.exists(C):
    print("codes", edit(C,
        "                boxes.append((q[0] - r, q[0] + r, q[2] - r, q[2] + r))\n        self.tally = boxes",
        "                boxes.append((q[0] - r, q[0] + r, q[2] - r, q[2] + r))\n"
        "        name = (getattr(m, \"tally\", None) or {}).get(\"name\") or {}\n"
        "        for q in name.get(\"line\", []):                    # (an ace's name over them: acepanel.py)\n"
        "            r = 0.6 * name.get(\"cap\", 0.05)\n"
        "            boxes.append((q[0] - r, q[0] + r, q[2] - r, q[2] + r))\n"
        "        self.tally = boxes"))
U = os.path.join(tree, "unitmarks.py")
if os.path.exists(U):
    print("unitmarks", edit(U,
        "            avoid += [(q[0], q[2], slot.get(\"radius\", 0.05)) for q in slot[\"slots\"]]\n",
        "            avoid += [(q[0], q[2], slot.get(\"radius\", 0.05)) for q in slot[\"slots\"]]\n"
        "    name = (getattr(m, \"tally\", None) or {}).get(\"name\")\n"
        "    if name:                                                 # (an ace's name over them: acepanel.py)\n"
        "        avoid += [(q[0], q[2], 0.6 * name.get(\"cap\", 0.05)) for q in name[\"line\"]]\n"))
F = os.path.join(tree, "fitout.py")                 # (her pilot kept inside her skin: crewfit.py)
print("fitout import", edit(F, "import math\nimport zlib\n", "import math\nimport crewfit\nimport zlib\n"))
print("fitout seat", edit(F,
    "        z = body_top(hood, x, -y) - 0.118 - 0.03 - SEAT_DROP.get(f.name, 0.0)\n"
    "        at = figure(m, (x, y, z), face, vest=single)\n",
    "        z = body_top(hood, x, -y) - 0.118 - 0.03 - SEAT_DROP.get(f.name, 0.0)\n"
    "        fits = single and face == 0.0 and crewfit.wanted(f)    # (a fighter's pilot: kept inside her skin, crewfit.py)\n"
    "        if fits:\n"
    "            top = crewfit.skin_top(f, hull)\n"
    "            low = crewfit.drop(hull, x, z, top, hood)\n"
    "            z -= low\n"
    "            n0 = len(m.verts)\n"
    "        at = figure(m, (x, y, z), face, vest=single)\n"))
print("fitout plain", edit(F,
    "        if not m.fine:\n            continue\n        m.box(at(-0.2, 0, -0.3)",
    "        if not m.fine:\n            if fits:\n                crewfit.squash(m, hull, n0, top, hood=hood)\n            continue\n        m.box(at(-0.2, 0, -0.3)"))
print("fitout fine", edit(F,
    "        first = False\n",
    "        if fits:\n"
    "            moved = crewfit.squash(m, hull, n0, top, 0.04 if getattr(f, \"cockpit\", None) else crewfit.SPARE, hood)\n"
    "            if low or moved:\n"
    "                print(f\"crewfit {f.name}: {low:.3f} lower, {moved:.3f} drawn in\", flush=True)\n"
    "        first = False\n"))
for mod in ("acepanel.py", "crewfit.py"):
    src = os.path.join(here, mod)
    if os.path.abspath(os.path.join(tree, mod)) != os.path.abspath(src):
        shutil.copy(src, os.path.join(tree, mod))
print("ok", tree)
