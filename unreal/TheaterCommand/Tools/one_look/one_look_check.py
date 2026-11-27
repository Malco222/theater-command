#!/usr/bin/env python3
"""One look for the whole war on the campaign map, its cloud check of the table (Tools/one_look/run.sh).

Reads the art director's table (Content/Data/Campaign/unit-table.json), the sculpted models it names
(Content/Models) and the map's own source, and checks that what the table's "one_look" says can be painted and is
what the ruling asks: each dominion's last dated mark and fin are whole and its own; each coat is the coat of the
machine the model follows, by that machine's own file, or the model's own with only its nose and boss changed (the
Mustang, whose coat Brian named himself); each mark that is drawn as another is one the painter draws;
and Germany's tanks take the nation's grey in every year as they are played. It writes the months of the dated marks
for the C++ test, which runs the game's own reckoning (Campaign/OneLook.h) over them.

Usage: one_look_check.py <project root> <out folder>
"""
import json
import os
import re
import sys

ROOT, OUT = sys.argv[1], sys.argv[2]
SRC = os.path.join(ROOT, "Source", "TheaterCommand", "Campaign")
MODELS = os.path.join(ROOT, "Content", "Models")

checks = 0
faults = 0


def check(ok, what):
    global checks, faults
    checks += 1
    if not ok:
        faults += 1
        print("FAULT: " + what)


def is_hex(code):
    return isinstance(code, str) and re.fullmatch(r"[0-9A-Fa-f]{6}", code) is not None


def rgb(code):
    return tuple(int(code[k:k + 2], 16) for k in (0, 2, 4))


def luma(code):
    r, g, b = rgb(code)
    return 0.299 * r + 0.587 * g + 0.114 * b


def is_red(code):
    r, g, b = rgb(code)
    return r >= 150 and g < 90 and b < 90


def readable(layer):
    """As the game's reader has it (CampaignUnitTable.cpp, ReadLayers): a colour and one of the four shapes."""
    if not isinstance(layer, dict) or not is_hex(layer.get("c")):
        return False
    for shape, least in (("ring", 2), ("tri", 2), ("star", 3), ("rect", 4)):
        if isinstance(layer.get(shape), list) and len(layer[shape]) >= least:
            return True
    return False


def model(name):
    path = os.path.join(MODELS, name + ".json")
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8") as told:
        return json.load(told)


with open(os.path.join(ROOT, "Content", "Data", "Campaign", "unit-table.json"), encoding="utf-8") as told:
    table = json.load(told)

one = table.get("one_look")
check(isinstance(one, dict) and isinstance(one.get("coats"), dict) and isinstance(one.get("marks"), dict),
      "the table has a one_look with coats and marks")
if not isinstance(one, dict):
    print("%d checks, %d faults" % (checks, faults))
    sys.exit(1)

# ---------------------------------------------------------------- the dated marks: the last of each stands all war
print("Dated marks (marking_months): the mark each wears with one look, in every month")
dated = []
last_marks = {}
for tag, row in table["countries"].items():
    months = row.get("marking_months")
    if not months:
        continue
    froms = []
    for entry in months:
        match = re.fullmatch(r"(\d{4})-(\d{2})", entry.get("from", ""))
        check(match is not None and 1 <= int(match.group(2)) <= 12, "%s: a dated mark's month reads as a year and a month" % tag)
        layers = entry.get("insignia", [])
        check(len(layers) > 0 and all(readable(one_layer) for one_layer in layers),
              "%s %s: every layer of the mark is one the game's reader reads, so no dated mark is left out of the count" % (tag, entry.get("from")))
        if match:
            froms.append(int(match.group(1)) * 12 + int(match.group(2)) - 1)
    check(all(froms[k] < froms[k + 1] for k in range(len(froms) - 1)), "%s: its dated marks are earliest first, so the last listed is the latest" % tag)
    check(len(froms) == len(months) and 1 <= len(froms) <= 4, "%s: one to four dated marks, each with its month" % tag)
    last = months[-1]
    colours = [one_layer["c"].upper() for one_layer in last["insignia"]]
    fin = last.get("fin", {})
    bands = fin.get("bands", [])
    check(len(set(colours)) >= 2, "%s: its last mark has two colours at the least" % tag)
    check(len(bands) >= 2 and all(is_hex(band) for band in bands), "%s: its last mark has its own rudder stripes, so the British flash does not stay beside its own roundel" % tag)
    check(not any(is_red(code) for code in colours) and not any(is_red(code) for code in bands if is_hex(code)),
          "%s: no red in the mark it wears all war (none beside Japan's red disc)" % tag)
    check(row.get("marking") == "as built", "%s: its first mark was the British one its machines are built with" % tag)
    last_marks[tag] = json.dumps(last["insignia"], sort_keys=True)
    dated.append((tag, froms))
    print("  %s  %-44s roundel %s  fin %s  (was British until %s%s)" % (
        tag, last.get("words", ""), "/".join(colours), "/".join(bands), months[0]["from"],
        ", changed again %s" % months[-1]["from"] if len(months) > 1 else ""))
check(len(dated) >= 1, "the table dates some country's marks")
check(len(set(last_marks.values())) == len(last_marks), "no two of them wear the same mark: each dominion's is its own")
check(sorted(last_marks) == ["AUS", "IND", "NZL", "SAF"], "the countries with dated marks are the four the ruling names (AUS, IND, NZL, SAF)")
for tag in ("UK", "CAN"):
    check(not table["countries"].get(tag, {}).get("marking_months"), "%s has no dated mark: it keeps the British roundel all war" % tag)

with open(os.path.join(OUT, "one_look_dated.inc"), "w", encoding="utf-8") as out:
    for tag, froms in dated:
        padded = froms + [0] * (4 - len(froms))
        out.write('\t{ "%s", %d, { %s } },\n' % (tag, len(froms), ", ".join(str(month) for month in padded)))

# ---------------------------------------------------------------- the coats: each is the coat of the machine it follows
with open(os.path.join(SRC, "CampaignAircraft.cpp"), encoding="utf-8") as told:
    aircraft_source = told.read()
OUTER = [0, 1, 2, 3, 4, 6, 15]  # upper, upper2, under, nose, boss, frame, stain
print("Coats: the colours each model wears with one look (its file's own in brackets)")
for name, coat in one["coats"].items():
    mine = model(name)
    check(mine is not None, "%s: there is a sculpted model of that name" % name)
    if mine is None:
        continue
    own = mine.get("colours", [])
    check(mine.get("kind") == "aircraft" and len(own) >= 18, "%s: an aircraft with its own colours in its file" % name)
    check(mine.get("nation_paint") is not True, "%s: not a plain machine painted for any nation" % name)
    numbers = {}
    for key, code in coat.get("colours", {}).items():
        check(key.isdigit() and int(key) < len(own), "%s: paint %s is one of the file's paints" % (name, key))
        check(is_hex(code), "%s: paint %s is a colour" % (name, key))
        if key.isdigit() and is_hex(code):
            numbers[int(key)] = code.upper()
    if coat.get("own") is True:
        # (A coat of her OWN: the owner named it himself. Brian, 27 November 2026: "also shouldnt the p-51 mustangs be
        # silver with black paint? why are they green?" She keeps her file's coat in every month; the row may change
        # her nose and her boss, nothing else, and follows no other machine.)
        check("follows" not in coat and "number_ink" not in coat, "%s: a coat of her own follows no other machine and keeps her own side number's ink" % name)
        check(len(numbers) > 0 and set(numbers) <= {3, 4}, "%s: a coat of her own changes only her nose and her boss (paints 3 and 4)" % name)
        laid = [numbers.get(k, own[k].upper()) for k in range(len(own))]
        check(all(laid[k] == own[k].upper() for k in range(len(own)) if k not in (3, 4)), "%s: laid, every paint but her nose and boss is her file's own" % name)
        ink = mine.get("number_ink", "F2F0E8")
        check(abs(luma(ink) - luma(laid[0])) >= 60.0, "%s: her side number stands off her coat (60 levels or more)" % name)
        check(all(abs(luma(laid[k]) - luma(laid[0])) >= 60.0 for k in (3, 4) if k in numbers), "%s: her nose and boss stand off her coat (60 levels or more)" % name)
        if name == "Mustang":
            spread = [max(rgb(laid[k])) - min(rgb(laid[k])) for k in (0, 1, 2, 6)]
            check(all(luma(laid[k]) >= 150.0 for k in (0, 1, 2, 6)) and max(spread) <= 12,
                  "Mustang: her coat is bare metal, a pale grey with no colour in it, above, below and on her frame (%s)" % ", ".join(laid[k] for k in (0, 1, 2, 6)))
            check(all(luma(laid[k]) <= 40.0 and max(rgb(laid[k])) - min(rgb(laid[k])) <= 8 for k in (3, 4)),
                  "Mustang: her nose and boss are black, with no red (%s, %s)" % (laid[3], laid[4]))
        print("  %-8s her own      %s" % (name, "  ".join("%d %s [%s]" % (k, laid[k], own[k].upper()) for k in OUTER)))
        print("  %-8s side number %s (her own)" % ("", ink.upper()))
        continue
    check(sorted(numbers) == OUTER, "%s: the whole outer coat is given (upper, upper2, under, nose, boss, frame, stain)" % name)
    followed = model(coat.get("follows", ""))
    check(followed is not None, "%s: the machine it follows (%s) is a sculpted model" % (name, coat.get("follows")))
    if followed is not None:
        theirs = [code.upper() for code in followed["colours"]]
        check(all(numbers.get(k) == theirs[k] for k in OUTER), "%s: each colour is %s's own, by its file" % (name, coat["follows"]))
        check(coat.get("number_ink", "").upper() == followed.get("number_ink", "").upper(), "%s: its side number is in %s's ink" % (name, coat["follows"]))
        # (the nation flies the one and then the other: the map's own picker says so)
        picker = re.search(r'\?\s*(?:FString\()?TEXT\("%s"\)\)?\s*:\s*TEXT\("%s"\)' % (re.escape(name), re.escape(coat["follows"])), aircraft_source)
        check(picker is not None, "%s follows %s in the map's own choice of a country's fighter (CampaignAircraft.cpp)" % (name, coat["follows"]))
        laid = [numbers.get(k, own[k].upper()) for k in range(len(own))]
        check(all(laid[k] == (theirs[k] if k in OUTER else own[k].upper()) for k in range(18)), "%s: laid, her outer coat is %s's and the rest her own" % (name, coat["follows"]))
        ink = coat.get("number_ink", mine.get("number_ink", "F2F0E8"))
        check(abs(luma(ink) - luma(laid[0])) >= 60.0, "%s: her side number stands off her coat (60 levels or more)" % name)
        check(abs(luma(laid[0]) - luma(laid[2])) >= 20.0, "%s: her under side is told from her upper side" % name)
        print("  %-8s follows %-8s %s" % (name, coat["follows"], "  ".join("%d %s [%s]" % (k, laid[k], own[k].upper()) for k in OUTER)))
        print("  %-8s side number %s [%s]" % ("", ink.upper(), mine.get("number_ink", "").upper()))

# ---------------------------------------------------------------- the marks: one mark for a nation's machines
carried = {}
for file_name in sorted(os.listdir(MODELS)):
    if not file_name.endswith(".json"):
        continue
    with open(os.path.join(MODELS, file_name), encoding="utf-8") as told:
        head = told.read(4096)
    if '"kind": "aircraft"' not in head and '"kind":"aircraft"' not in head:
        continue
    each = model(file_name[:-5])
    for mark in each.get("marks", []) if isinstance(each.get("marks"), list) else []:
        carried.setdefault(mark.get("tag"), set()).add(file_name[:-5])
with open(os.path.join(SRC, "CampaignMiniatures.cpp"), encoding="utf-8") as told:
    painter = told.read()
# (A mark of the painter's own: no file carries it. The table draws it wherever the mark the files carry is asked for,
# so a nation's mark is mended in one place for its aircraft, its tanks and the map's pieces, and no file changes.)
PAINTERS_OWN = {"GER-BALKEN"}
print("Marks: drawn as another with one look")
for asked, drawn in one["marks"].items():
    check(asked in carried, "%s: some sculpted aircraft carries that mark" % asked)
    check((drawn in carried) != (drawn in PAINTERS_OWN),
          "%s: the mark drawn in its place (%s) is one some sculpted aircraft carries already, or the painter's own and carried by no file" % (asked, drawn))
    check('Tag == TEXT("%s")' % drawn in painter, "%s: the painter draws %s" % (asked, drawn))
    check(drawn not in one["marks"], "%s: the mark drawn in its place is not itself drawn as another" % asked)
    print("  %s (%s) is drawn as %s (%s)" % (asked, ", ".join(sorted(carried.get(asked, []))), drawn,
                                           "the painter's own" if drawn in PAINTERS_OWN else "as " + ", ".join(sorted(carried.get(drawn, [])))))
american = sorted(tag for tag in carried if tag and tag.startswith("USA"))
check(sorted(set(one["marks"].get(tag, tag) for tag in american)) == ["USA-BAR"],
      "with one look every American machine carries the one mark, the star and bar (the marks carried are %s)" % ", ".join(american))
barred = re.search(r'Tag == TEXT\("USA-BAR"\)\)\s*\{(.*?)\n\t\}', painter, re.S)
check(barred is not None
      and re.search(r'Bar\(-0\.3, -2\.05, 0\.3, 2\.05, Navy\);\s*\+\+Layer;\s*Bar\(-0\.24, -1\.98, 0\.24, 1\.98, White\);', barred.group(1)) is not None
      and re.search(r'Disc\(1\.0, Navy\);\s*Star\(0\.95, White\);', barred.group(1)) is not None
      and "Red" not in barred.group(1) and "Disc(0.2" not in barred.group(1),
      "the American mark is the white star in its blue disc with a white bar each side, edged in blue, and no red anywhere")

# (The German cross. Brian, 26 November 2026: "the german logo is wrong on the FW-190". The art director, 27 November:
# one cross at its real proportions for aircraft, tanks and the map. Each number below is a share of the cross's length.)
german_marks = sorted(tag for tag in carried if tag and tag.startswith("GER"))
check(german_marks == ["GER"] and one["marks"].get("GER") == "GER-BALKEN",
      "with one look every German machine carries the one cross, at its real proportions (the marks carried are %s)" % ", ".join(german_marks))


def crosses(tag):
    """The layers of a mark the painter draws as crosses, lowest first: (arm, half width, colour) in the mark's radius."""
    branch = re.search(r'Tag == TEXT\("%s"\)\)\s*\{(.*?)\n\t\}' % re.escape(tag), painter, re.S)
    if branch is None:
        return None
    body = "\n".join(line for line in branch.group(1).split("\n") if not line.strip().startswith("//"))
    laid = re.findall(r'Cross\(([0-9.]+), ([0-9.]+), (\w+)\);', body)
    rest = re.sub(r'Cross\([0-9.]+, [0-9.]+, \w+\);', "", body).strip()
    return None if rest else [(float(arm), float(half), colour) for arm, half, colour in laid]


mended = crosses("GER-BALKEN")
check(mended is not None and [colour for _, _, colour in mended] == ["Black", "White", "Black"] and all(arm == 1.0 for arm, _, _ in mended),
      "the mended German cross is three crosses and nothing else, black, white, black, each arm the full length: nothing lies across an arm's end")
if mended is not None and len(mended) == 3:
    edge, white, bar = (half for _, half, _ in mended)
    check(abs(bar - 0.25) < 1e-9, "its black bars are a quarter of its length wide (%.4f)" % (bar,))
    check(abs((white - bar) / 2.0 - 0.125) < 1e-9, "its white is an eighth of its length each side (%.4f)" % ((white - bar) / 2.0,))
    check(abs((edge - white) / 2.0 - 0.03125) < 1e-9, "its thin black edge is a thirty-second of its length (%.5f)" % ((edge - white) / 2.0,))
    print("  the German cross with one look: bars %.4f, white %.4f each side, edge %.5f of its length; an arm is %.4f across in all"
          % (bar, (white - bar) / 2.0, (edge - white) / 2.0, edge))
check(crosses("GER") == [(1.0, 0.36, "White"), (0.94, 0.19, "Black")], "without one look the German cross is drawn as it was, to the number")

# ---------------------------------------------------------------- Germany's tanks: grey in every year
print("German tanks on the map")
german = table["countries"]["GER"].get("paint", "")
check(is_hex(german) and max(rgb(german)) - min(rgb(german)) <= 16 and luma(german) < 110.0, "Germany's paint in the table is a dark grey (%s)" % german)
for name in ("PanzerIV", "Tiger", "Panther"):
    tank = model(name)
    check(tank is not None and "colours" not in tank and "kind" not in tank and "Tools/build_tank.py" in tank.get("about", ""),
          "%s: the sculpted model has no colours of its own, so as played it is in Germany's grey in every year" % name)
    print("  %-8s sculpted: Germany's grey %s in every year (the model has no colours of its own)" % (name, german))
with open(os.path.join(SRC, "CampaignVehicles.cpp"), encoding="utf-8") as told:
    vehicles = told.read()
check(re.search(r'static void PanzerIV\(FMiniatureMesh& M\)\s*\{\s*const FBuild B\(M, Hex\(TEXT\("51575C"\)\)\);', vehicles) is not None, "the code-built Panzer IV is in grey 51575C")
check('Hex(UnitTable::OneLook() ? TEXT("51575C") : TEXT("9C8A58"))' in vehicles, "the code-built Tiger takes the Panzer IV's grey with one look, and its dark yellow without")
check('Hex(bGreyAllWar ? TEXT("51575C") : TEXT("9C8A58"))' in vehicles and re.search(r'if \(!bGreyAllWar\)\s*\{\s*B\.Plate\([^;]*Leaf\);\s*B\.Plate\([^;]*Earth\);\s*B\.Plate\([^;]*Earth\);\s*\}', vehicles) is not None,
      "the code-built Panther takes the Panzer IV's grey with one look, and none of its stripes")
print("  Tiger, Panther code-built stand-ins (seen only with -TheaterCodeBuilt): 9C8A58 dark yellow without one look, 51575C with")

# ---------------------------------------------------------------- the source: the switch is off as it comes, and reaches what it must
with open(os.path.join(SRC, "CampaignUnitTable.cpp"), encoding="utf-8") as told:
    unit_table = told.read()
check(re.search(r'TEXT\("tc\.Map\.OneLook"\), 0,', unit_table) is not None, "tc.Map.OneLook is off as it comes")
check('TEXT("TheaterOneLook")' in unit_table, "-TheaterOneLook turns it on")
check(re.search(r'int32 UnitTable::MarkEra\(const FString& Tag\)\s*\{[^}]*TheaterOneLook::EraOf\(Look->DatedFrom\.GetData\(\), Look->DatedFrom\.Num\(\), MonthNow, OneLook\(\)\)', unit_table) is not None,
      "the mark that stands is reckoned by Campaign/OneLook.h, which the C++ test runs")
check('TheaterOneLook::MonthOfTurn(Turn)' in unit_table and 'TheaterOneLook::MonthOf(' in unit_table, "the month of a turn and of a dated mark are reckoned by Campaign/OneLook.h")
check('(UnitTable::OneLook() ? TEXT("|one") : TEXT(""))' in painter, "a sculpted model painted with one look is kept apart from one painted without")
check('BuiltKey += TEXT("|one");' in vehicles, "a code-built model painted with one look is kept apart from one painted without")
check('UnitTable::OneLookCoat(Name, CoatLaid, InkLaid, bInkLaid)' in painter and 'PaintedNumerals(At, Mark.Radius, Number, Facing, SideInk)' in painter,
      "the painter lays the table's coat on an aircraft and paints its number in the coat's ink")
check('const FString Tag = UnitTable::OneLookMark(TagAsked);' in painter, "every mark the painter draws passes through the table's one look")

print("%d checks, %d faults" % (checks, faults))
sys.exit(1 if faults else 0)
