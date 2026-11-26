"""Take the dates off every soldier file's helmet finishes, so a man's markings and paint never hang on the year.

Brian's standing rule (26 November 2026, 03:56Z): unit markings and paint never depend on the year, for tanks, troops,
planes and ships. A soldier file's "helmets" table gave each finish (the national shield, the sun badge, the red star,
the Italian stencil, the sergeant's bar, the nets, the camouflage paint) a "from" and a "to" month, and the battle drew
only the finishes whose months held its date: a Polish file's finishes fitted September and October 1939 only, the
American's from 1942, everyone's stopped in 1945, so a battle outside those months had men in bare helmets with no badge
at all. This takes "from" and "to" off every finish: each finish fits any date, chosen by its season, its front and its
share as before (LandMen.cpp reads a missing "from" as 1900 and a missing "to" as 2100).

The file's text is changed only where a finish's two dates stand; everything else keeps its bytes, and the result is
read back and checked against the file as it was, less those dates.

    python3 finishes_any_year.py FILE_OR_FOLDER...           # take the dates off (writes through a temp file)
    python3 finishes_any_year.py --check FILE_OR_FOLDER...   # list any finish still dated; exit 1 if one is

A folder means every soldier_*.json in it. Run it (or --check) after any tool that writes a soldier file's helmets.
"""
import json
import os
import re
import sys
import tempfile

DATE = re.compile(r'"(?:from|to)"\s*:\s*"\d{4}-\d{2}"')

# The file's own note on picking (`about_variety`, variety.py's words), said again without the months.
NOTE = [
    ("Each finish has `from` and `to` (YYYY-MM), `season` (any, summer, winter), `front` (any, east, west, south, "
     "desert, pacific, china) and `share` (how common it was).",
     "Each finish has `season` (any, summer, winter), `front` (any, east, west, south, desert, pacific, china) and "
     "`share` (how common it was), and no `from` or `to` month (Brian's standing rule, 26 November 2026: a man's "
     "markings and paint never hang on the year)."),
    ("the finishes whose months hold the battle's date and whose season and front fit (any fits all)",
     "the finishes whose season and front fit (any fits all), whatever the battle's date"),
    ("When no finish fits the date, draw the first finish (or his own helmet).",
     "When no finish fits his season and front, he wears his own helmet in his army's helmet colour."),
    # (this tool's first wording, 26 November 04:10Z, said again as LandMen.cpp draws it)
    ("When no finish fits his season and front, draw the first finish (or his own helmet).",
     "When no finish fits his season and front, he wears his own helmet in his army's helmet colour."),
]


def old_note(doc):
    a = doc.get("about_variety")
    return isinstance(a, str) and any(o in a for o, _ in NOTE)


def soldier_files(paths):
    out = []
    for p in paths:
        if os.path.isdir(p):
            out += sorted(os.path.join(p, f) for f in os.listdir(p) if f.startswith("soldier_") and f.endswith(".json"))
        else:
            out.append(p)
    return out


def dated(doc):
    """The finishes in a parsed file that still carry a date: (index, name, from, to)."""
    return [(i, h.get("name"), h.get("from"), h.get("to")) for i, h in enumerate(doc.get("helmets") or [])
            if isinstance(h, dict) and ("from" in h or "to" in h)]


def undated_text(text):
    """The file's text with each helmet finish's "from" and "to" taken out (with the comma beside each)."""
    i = text.find('"helmets"')
    if i < 0:
        return text
    # The helmets table is one JSON array: find its end by counting brackets outside strings.
    j = text.index("[", i)
    depth, k, in_str, esc = 0, j, False, False
    while True:
        c = text[k]
        if in_str:
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "[":
            depth += 1
        elif c == "]":
            depth -= 1
            if depth == 0:
                break
        k += 1
    table = text[j:k + 1]
    # A date with a comma after it ("from":"1939-09",) or, the last key of its finish, one before it (,"to":"1945-08").
    table = re.sub(DATE.pattern + r"\s*,\s*", "", table)
    table = re.sub(r"\s*,\s*" + DATE.pattern, "", table)
    text = text[:j] + table + text[k + 1:]
    for o, n in NOTE:
        text = text.replace(o, n)
    return text


def main(argv):
    check = "--check" in argv
    paths = soldier_files([a for a in argv if a != "--check"])
    if not paths:
        print(__doc__)
        return 2
    bad = 0
    for p in paths:
        with open(p, encoding="utf-8") as f:
            text = f.read()
        doc = json.loads(text)
        left = dated(doc)
        if check:
            if left:
                bad += 1
                print(f"DATED {p}: {len(left)} finishes, e.g. {left[0]}")
            elif old_note(doc):
                bad += 1
                print(f"NOTE  {p}: its about_variety still picks finishes by date")
            continue
        if not left and not old_note(doc):
            print(f"ok    {p}: no dated finish")
            continue
        new = undated_text(text)
        want = json.loads(text)
        for h in want.get("helmets") or []:
            if isinstance(h, dict):
                h.pop("from", None)
                h.pop("to", None)
        if isinstance(want.get("about_variety"), str):
            for o, n in NOTE:
                want["about_variety"] = want["about_variety"].replace(o, n)
        got = json.loads(new)
        if got != want:
            bad += 1
            print(f"FAIL  {p}: the edited text does not read back as the file less its dates; left as it was")
            continue
        d = os.path.dirname(os.path.abspath(p))
        fd, tmp = tempfile.mkstemp(prefix=".undated_", suffix=".json", dir=d)
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(new)
        try:
            os.chmod(tmp, os.stat(p).st_mode & 0o777)
        except OSError:
            pass
        os.replace(tmp, p)
        print(f"done  {p}: {len(left)} finishes undated ({len(text) - len(new)} bytes less)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
