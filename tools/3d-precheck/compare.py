#!/usr/bin/env python3
"""Prints what one checked tree adds over another:  compare.py BASE_LABEL HEAD_LABEL

Diagnostics are matched by file, kind, message and the text of the source line, not by line number, so code that only
moved does not count as new.
"""
import collections, json, sys

HERE = "/home/user/precheck"


def load(label):
    s = json.load(open(f"{HERE}/out/{label}/summary.json"))
    c = collections.Counter()
    where = {}
    for f, r in s["results"].items():
        for d in r["diags"]:
            key = (d["file"], d["kind"], d["msg"], d["src"])
            c[key] += 1
            where.setdefault(key, (d["line"], d["notes"], f))
    return s, c, where


base, bc, _ = load(sys.argv[1])
head, hc, where = load(sys.argv[2])
new = hc - bc
errors = [(k, n) for k, n in new.items() if k[1] != "warning"]
warnings = [(k, n) for k, n in new.items() if k[1] == "warning"]
failed = [f for f, r in head["results"].items() if r["code"] != 0]
print(f"files checked: {len(head['results'])}; files that fail: {len(failed)}; new errors: {sum(n for _, n in errors)}; new warnings: {sum(n for _, n in warnings)}")
for group, title in ((errors, "ERROR"), (warnings, "warning")):
    for (f, kind, msg, src), n in sorted(group, key=lambda x: (x[0][0], where[x[0]][0])):
        line, notes, unit = where[(f, kind, msg, src)]
        print(f"  {title} {f}:{line}{' (x%d)' % n if n > 1 else ''}: {msg[:300]}")
        if src:
            print(f"      > {src[:220]}")
        for note in notes[:2]:
            print(f"      note: {note[:220]}")
        if not f.startswith("Source") and unit:
            print(f"      (while compiling {unit})")
for n in head["notes"]:
    print("  note:", n)
# the Mac's last step: pieces a file needs that nothing defines (only when both trees were checked with --link)
missing = []
if head.get("undefined") is not None:
    old = {u["symbol"] for u in (base.get("undefined") or [])}
    missing = [u for u in head["undefined"] if u["symbol"] not in old]
    print(f"link step: {len(missing)} new pieces declared and used but never written")
    for u in missing:
        print(f"  UNDEFINED {u['symbol'][:300]}   (needed by {', '.join(u['needed_by'])})")
else:
    print("link step: not run")
modfail = any("MODULE CHECK FAILED" in n for n in head["notes"])
verdict = "PASS" if not failed and not errors and not missing and not modfail else "FAIL"
print("VERDICT:", verdict + (" with warnings" if verdict == "PASS" and warnings else ""))
