#!/bin/bash
# Pearl's pin (the release 2 ocean plan, step 0: measure and guard rails). The fleet actions' new sea is built beside
# what Pearl Harbor, Midway, the air battles and the convoy looks already draw, never in it. This holds the lines the
# plan keeps word for word to a SHA-256 each (Tools/sea_waves/pearl.sha):
# - the bodies of AAirStage::Raise, RaiseSea, BuildSea, MovingSea(bool), ShoreAt, MoveShip, Steer, GroundZ and
#   GroundAside, and their declarations in AirStage.h;
# - SeaFace::MakeFoam and MakeShore;
# - BattleLook::AirHaze, AirSkyBegin, AirSkyEnd and Grade, and BattleLook's shared defaults (CampaignBattleLook.h whole);
# - ConvoyLooksCore.{h,cpp} whole; ShipGuns.* and LandMen.* whole (byte copies, never edited);
# - Pearl Harbor's and Midway's stages (Content/Data/Stages/oahu.json and midway.json) whole;
# - the air battle's haze over a stage's sea (AirBattle.cpp): the two inks, the fog's colour and AirHaze's call (30 km,
#   1.2 times), and the haze band's tint (pinned from the release 2 join, 2f89c8e5: AirHaze's body holds the fade, these
#   lines hold the colour and where it starts).
# And against 2f89c8e5 (PEARL_BASE to name another): TheaterCommand.Build.cs still says "bUseUnity = false;", and
# CampaignMapActor.*, CampaignHUD.* and CampaignPlayerController.* have no change at all. (The base moved from 10f85ffb
# with the release 2 join 2f89c8e5: release 2's campaign lanes changed all three outside the ocean lane's history, from
# 23 November on (release 1.5's air campaign d0266a02, 1.5.1, the map branch sgfei1, lku6by, the ship list 485bb365, and
# the sea scene's own "The month at sea" row 5286a1fd), and the air scene only hides the map, its pieces and the HUD's
# frame while a stage stands, so Pearl's and Midway's sea draw nothing from them.)
# The three campaign files are a hard gate on the sea lanes' own branches (ocean-r2, d2zd89, wonnkr and its lanes): there
# a change since the base means a sea lane touched them. An assembled release build joins the map and UI lanes as well,
# which change them by right, so there the assembler, and only the assembler, sets PEARL_CAMPAIGN=report (the sea
# scene's ruling, 25 November 22:25Z): each changed file is said, with every commit since the base that touched it (sha
# and subject), so the pre-check sees at a glance whether a sea commit is among them, and it still fails if one is (a
# subject starting "Sea " or "Storms and night at sea", the sea lanes' own), or if the change is not yet committed. A sea
# lane never changes those files, joined or not. Report mode never touches the Pearl and Midway pins above: they gate in
# every mode.
# A body is found by its definition at the head of a line and runs to its closing brace (strings, characters and
# comments skipped). Usage: Tools/sea_waves/pearl_pin.sh [--write | --campaign-only]  (--campaign-only runs the campaign
# files' check alone, for its test, Tools/sea_waves/pearl_pin_test.sh, on a repository of its own named by PEARL_ROOT;
# PEARL_PINS names another pins file, for the same test; --write re-pins pearl.sha from this tree: only with
# the change it pins, in the same commit where that commit's tree has this pin; the deck crews' 5ff6b021, which the
# wolf-pack nights' merge 55419eb6 brought, moved ConvoyLooksCore.cpp, and its re-pin came at the release 2 join).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${PEARL_ROOT:-$(cd "$HERE/../.." && pwd)}"
PINS="${PEARL_PINS:-$HERE/pearl.sha}"
BASE="${PEARL_BASE:-2f89c8e5}"
CAMPAIGN="${PEARL_CAMPAIGN:-gate}"
MODE="${1:-}"
STATUS=0
if [ "$CAMPAIGN" != "gate" ] && [ "$CAMPAIGN" != "report" ]; then
  echo "  FAIL PEARL_CAMPAIGN=$CAMPAIGN: only report (the assembler's, on an assembled release build) or nothing"; STATUS=1
fi
if [ "$MODE" != "--campaign-only" ]; then
python3 -I - "$ROOT" "$PINS" "$MODE" <<'PY' || STATUS=1
import hashlib, os, re, sys

root, pins_path, mode = sys.argv[1], sys.argv[2], sys.argv[3]
S = "Source/TheaterCommand/"
BODIES = [
    (S + "Battle/AirStage.cpp", "AAirStage::Raise"),
    (S + "Battle/AirStage.cpp", "AAirStage::RaiseSea"),
    (S + "Battle/AirStage.cpp", "AAirStage::BuildSea"),
    (S + "Battle/AirStage.cpp", "AAirStage::MovingSea"),
    (S + "Battle/AirStage.cpp", "AAirStage::ShoreAt"),
    (S + "Battle/AirStage.cpp", "AAirStage::MoveShip"),
    (S + "Battle/AirStage.cpp", "AAirStage::Steer"),
    (S + "Battle/AirStage.cpp", "AAirStage::GroundZ"),
    (S + "Battle/AirStage.cpp", "AAirStage::GroundAside"),
    (S + "Battle/SeaFace.cpp", "SeaFace::MakeFoam"),
    (S + "Battle/SeaFace.cpp", "SeaFace::MakeShore"),
    (S + "Campaign/CampaignBattleLook.cpp", "BattleLook::AirHaze"),
    (S + "Campaign/CampaignBattleLook.cpp", "BattleLook::AirSkyBegin"),
    (S + "Campaign/CampaignBattleLook.cpp", "BattleLook::AirSkyEnd"),
    (S + "Campaign/CampaignBattleLook.cpp", "BattleLook::Grade"),
]
LINES = [
    (S + "Battle/AirStage.h", "bool Raise(const FString& Key, const FVector2D& Aim, double Turn, const FVector2D& Target, double InUnitsPerMetre, FBattleEffects* InEffects);"),
    (S + "Battle/AirStage.h", "bool RaiseSea(const FVector2D& Middle, double InSunAzimuth, double InSunElevation, double InUnitsPerMetre, FBattleEffects* InEffects, uint64 Seed);"),
    (S + "Battle/AirStage.h", "void Steer(int32 Id, const FVector2D& At, double Face, double Heel, double Pitch, double Heave = 0.0);"),
    (S + "Battle/AirStage.h", "double GroundZ(const FVector& Where) const;"),
    (S + "Battle/AirStage.h", "std::function<double(double, double)> GroundAside() const;"),
    (S + "Battle/AirStage.h", "void BuildSea();"),
    (S + "Battle/AirStage.h", "UMaterialInstanceDynamic* MovingSea(bool bShore);"),
    (S + "Battle/AirStage.h", "FVector2D ShoreAt(double East, double North) const;"),
    (S + "Battle/AirStage.h", "void MoveShip(FShip& Ship, double Now);"),
    # the haze over a stage's sea: its ink, the land's, the fog's colour, AirHaze's call and the haze band's tint
    (S + "Battle/AirBattle.cpp", "static const FLinearColor AirHazeInk(0.55f, 0.66f, 0.78f);"),
    (S + "Battle/AirBattle.cpp", "double R = 0.42, G = 0.60, B = 0.88;", "SeaHazeInk"),
    (S + "Battle/AirBattle.cpp", "Fog->SetFogInscatteringColor(bOverLand ? AirParts::AirHazeInk : AirParts::SeaHazeInk());"),
    (S + "Battle/AirBattle.cpp", "BattleLook::AirHaze(*Fog, UnitsPerMetre, bOverLand ? 7500.0 : 30000.0, bOverLand ? 2.6 : 1.2);"),
    (S + "Battle/AirBattle.cpp", "HazeBandInk->SetVectorParameterValue(TEXT(\"Tint\"), (bBandOverLand ? AirParts::AirHazeInk : AirParts::SeaHazeInk()) * static_cast<float>(HazeBandStrength));"),
]
FILES = [
    S + "Campaign/CampaignBattleLook.h",
    S + "Battle/ConvoyLooksCore.h", S + "Battle/ConvoyLooksCore.cpp",
    S + "Battle/ShipGuns.h", S + "Battle/ShipGuns.cpp",
    S + "Battle/LandMen.h", S + "Battle/LandMen.cpp",
    "Content/Data/Stages/oahu.json", "Content/Data/Stages/midway.json",
]


def read(path):
    with open(os.path.join(root, path), "rb") as f:
        return f.read()


def body(text, name):
    """The definition of name: from the line it begins (at the head of a line, not a declaration) to its closing brace."""
    lines = text.split("\n")
    head = re.compile(r"^(?![\s#/])(?!.*;\s*$).*(?<![\w:])" + re.escape(name) + r"\s*\(")
    starts = [i for i, line in enumerate(lines) if head.match(line)]
    if len(starts) != 1:
        raise SystemExit(f"pearl_pin: {name} is defined {len(starts)} times (one wanted)")
    start = starts[0]
    src = "\n".join(lines[start:])
    depth, seen, i, n = 0, False, 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith("//", i):
            i = src.find("\n", i)
            i = n if i < 0 else i
            continue
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c == '"' or c == "'":
            if c == '"' and i > 0 and src[i - 1] == "R":  # (a raw string: R"(...)")
                close = src.find(")\"", i)
                i = n if close < 0 else close + 2
                continue
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            i = j + 1
            continue
        if c == "{":
            depth += 1
            seen = True
        elif c == "}":
            depth -= 1
            if seen and depth == 0:
                end = src.find("\n", i)
                return src[: n if end < 0 else end]
        i += 1
    raise SystemExit(f"pearl_pin: {name}'s body does not close")


items = []
for path, name in BODIES:
    text = read(path).decode("utf-8")
    items.append((f"body {path} {name}", hashlib.sha256(body(text, name).encode("utf-8")).hexdigest()))
for path, line, *named in LINES:
    text = read(path).decode("utf-8")
    found = sum(1 for l in text.split("\n") if l.strip() == line)
    items.append((f"line {path} {named[0] if named else line.split('(')[0].split()[-1]}", hashlib.sha256(line.encode("utf-8")).hexdigest() if found == 1 else f"missing-or-twice-{found}"))
for path in FILES:
    items.append((f"file {path}", hashlib.sha256(read(path)).hexdigest()))

if mode == "--write":
    with open(pins_path, "w", encoding="utf-8") as f:
        f.write("# What the release 2 ocean plan keeps word for word, each by its SHA-256 (pearl_pin.sh; step 0, pinned at 10f85ffb).\n")
        f.write("# A change here lands only with the change it pins, in the same commit.\n")
        for label, sha in items:
            f.write(f"{sha}  {label}\n")
    print(f"pearl_pin: pearl.sha written, {len(items)} pins")
pins = {}
with open(pins_path, encoding="utf-8") as f:
    for line in f:
        if line.strip() and not line.startswith("#"):
            sha, label = line.rstrip("\n").split("  ", 1)
            pins[label] = sha
failed = 0
for label, sha in items:
    ok = pins.get(label) == sha
    failed += 0 if ok else 1
    print(f"  {'ok  ' if ok else 'FAIL'} {label}" + ("" if ok else f" (pinned {pins.get(label, 'nothing')[:16]}, now {sha[:16]})"))
for label in sorted(set(pins) - {l for l, _ in items}):
    failed += 1
    print(f"  FAIL {label} is pinned but no longer checked")
print(f"pearl_pin: {len(items)} pins, {failed} changed")
sys.exit(1 if failed else 0)
PY
# Against the commit the plan was made on: the module still compiled file by file, and the campaign's own three
# untouched (no new lines in CampaignMapActor, CampaignHUD or CampaignPlayerController).
if grep -Eq '^[[:space:]]*bUseUnity = false;[[:space:]]*$' "$ROOT/Source/TheaterCommand/TheaterCommand.Build.cs"; then
  echo "  ok   TheaterCommand.Build.cs keeps \"bUseUnity = false;\""
else
  echo "  FAIL TheaterCommand.Build.cs no longer says \"bUseUnity = false;\""; STATUS=1
fi
fi
if ! git -C "$ROOT" rev-parse --verify --quiet "$BASE^{commit}" > /dev/null; then
  echo "  FAIL the commit $BASE is not in this repository"; STATUS=1
else
  for F in CampaignMapActor CampaignHUD CampaignPlayerController; do
    H="Source/TheaterCommand/Campaign/$F.h"; C="Source/TheaterCommand/Campaign/$F.cpp"
    if git -C "$ROOT" diff --quiet "$BASE" -- "$H" "$C"; then
      echo "  ok   Campaign/$F.{h,cpp} as at $BASE"
      continue
    fi
    # (every commit since the base that itself touched the file, merges left out, side branches followed)
    TOUCHED="$(git -C "$ROOT" log --full-history --no-merges --format='%h %s' "$BASE..HEAD" -- "$H" "$C")"
    SEA="$(printf '%s\n' "$TOUCHED" | grep -E '^[0-9a-f]+ (Sea |Storms and night at sea)' || true)"
    if [ "$CAMPAIGN" = "report" ]; then
      echo "  note Campaign/$F.{h,cpp} changed since $BASE (PEARL_CAMPAIGN=report, an assembled release build: said, not failed); the commits that touched it:"
    else
      echo "  FAIL Campaign/$F.{h,cpp} changed since $BASE; the commits that touched it:"; STATUS=1
    fi
    if [ -n "$TOUCHED" ]; then printf '%s\n' "$TOUCHED" | sed 's/^/         /'; else echo "         none committed"; fi
    git -C "$ROOT" diff --stat "$BASE" -- "$H" "$C" | sed 's/^/       /'
    if [ "$CAMPAIGN" = "report" ]; then
      if [ -n "$SEA" ]; then
        echo "  FAIL Campaign/$F.{h,cpp} was touched by a sea lane's commit, which no build may carry:"; printf '%s\n' "$SEA" | sed 's/^/         /'; STATUS=1
      fi
      if ! git -C "$ROOT" diff --quiet HEAD -- "$H" "$C"; then
        echo "  FAIL Campaign/$F.{h,cpp} has a change not yet committed, so no commit can be named for it"; STATUS=1
      fi
    fi
  done
fi
exit $STATUS
