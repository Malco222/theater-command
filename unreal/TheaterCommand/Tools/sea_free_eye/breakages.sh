#!/bin/bash
# Deliberate breakages of the sea free eye (the sea free eye plan, 5d), one at a time on a copy of the source: each must
# make the cloud test (Tools/sea_free_eye/run.sh, g++, --quick, on a recording with divisions and one with boats the
# player has not found) fail at least one check. Each is a literal piece of the glue, the scene or the switches and what
# it becomes; a piece not found exactly once is reported (ANCHOR GONE) and counts as a breakage passing, so a change that
# moves a line makes this fail loudly instead of testing nothing. A copy that does not build is reported too (BUILD
# FAILED) and counts the same: a breakage must be a working program the checks catch.
# Prints a table; exits non-zero if any breakage passes. Usage: Tools/sea_free_eye/breakages.sh [M1 M2 ...]
#   SEA_FREE_EYE_BREAK is where the copy and its builds go (default ${TMPDIR:-/tmp}/sea_free_eye_break).
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
WORK="${SEA_FREE_EYE_BREAK:-${TMPDIR:-/tmp}/sea_free_eye_break}"
mkdir -p "$WORK"
RECS=("$ROOT/Content/Data/SeaBattles/samar.json" "$ROOT/Tools/sea_show/recordings/convoy-hunters.json")
GLUE=Battle/SeaFreeEyeGlue.cpp
GLUEH=Battle/SeaFreeEyeGlue.h
SCENE=Battle/SeaBattle.cpp

IDS=()
WHATS=()
FILES=()
OLDS=()
NEWS=()
# add <id> <what> <file under Source/TheaterCommand> <piece> <what it becomes>
add() { IDS+=("$1"); WHATS+=("$2"); FILES+=("$3"); OLDS+=("$4"); NEWS+=("$5"); }

# Picks and locks (1c, 1g).
add M1 "bShown dropped from MayPick" "$GLUE" \
	'return Ship.bShown && (Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping);' \
	'return (Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping);'
add M2 "sinking ships pickable" "$GLUE" \
	'return Ship.bShown && (Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping);' \
	'return Ship.bShown && (Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping || Ship.Doing == SeaShow::Fate::Sinking);'
add M3 "a lock kept on a ship lost from sight" "$GLUE" \
	$'\t\t\t\tconst SeaShow::ShipOut* Ship = Find(Ships, St.L.Key);\n\t\t\t\tif (!Ship || !MayPick(*Ship))' \
	$'\t\t\t\tconst SeaShow::ShipOut* Ship = Find(Ships, St.L.Key);\n\t\t\t\tif (!Ship)'
add M4 "Tab by key, not by line" "$GLUE" \
	$'\t\t\t\tconst int LineA = A->LineAt < 0 ? 1 : 0, LineB = B->LineAt < 0 ? 1 : 0;\n\t\t\t\tif (LineA != LineB) return LineA < LineB;\n\t\t\t\tif (A->LineAt != B->LineAt) return A->LineAt < B->LineAt;\n\t\t\t\tif (A->Piece != B->Piece) return A->Piece < B->Piece;\n\t\t\t\tif (A->Slot != B->Slot) return A->Slot < B->Slot;\n' \
	''
add M5 "the enemy first in Tab" "$GLUE" \
	'const int SideA = A->Side == MySide ? 0 : 1, SideB = B->Side == MySide ? 0 : 1;' \
	'const int SideA = A->Side == MySide ? 1 : 0, SideB = B->Side == MySide ? 1 : 0;'
add M6 "a division's id not renewed" "$GLUE" \
	'St.L.Id = St.NextDivisionId++;' \
	'St.L.Id = St.L.Id + 0 * St.NextDivisionId;'
add M7 "hidden ships in a division (its middle)" "$GLUE" \
	'if (Ship.Piece == Piece && MayPick(Ship) && Ship.Doing == SeaShow::Fate::Line)' \
	'if (Ship.Piece == Piece && Ship.Doing == SeaShow::Fate::Line)'
# The floor and the ceiling (1a, 1d, 1i).
add M8 "the floor 0 under the moving sea and the gale" "$GLUE" \
	'const double Z = Drawn ? Drawn(Context, X, Y) : 0.0;' \
	'const double Z = 0.0 * (Drawn ? Drawn(Context, X, Y) : 0.0);'
add M9 "EyeMost 3000" "$GLUEH" \
	'FloorClear = 3.0, EyeMost = 1500.0,' \
	'FloorClear = 3.0, EyeMost = 3000.0,'
add M10 "FloorClear 0.2" "$GLUEH" \
	'FloorClear = 3.0, EyeMost = 1500.0,' \
	'FloorClear = 0.2, EyeMost = 1500.0,'
# Real seconds (1e, 2e).
add M11 "the free eye's seconds times the pace" "$SCENE" \
	$'\tFreeDt = DeltaSeconds;' \
	$'\tFreeDt = DeltaSeconds * Pace;'
add M12 "a lock's velocity per battle step (held to 0.1 s)" "$GLUE" \
	'In.AnchorVel = In.bAnchorVel ? (At - St.L.Last) * (1.0 / Dt) : Vec{};' \
	'In.AnchorVel = In.bAnchorVel ? (At - St.L.Last) * (1.0 / std::min(Dt, 0.1)) : Vec{};'
# Keys (1b).
add M13 "Shift+W pans" "$GLUE" \
	'case KeyName::Up: return bShift ? TiltDown : PanAhead;' \
	'case KeyName::Up: return bShift ? (TiltDown | PanAhead) : PanAhead;'
add M14 "Tab reaches a hidden ship" "$GLUE" \
	$'\t\t\tif (MayPick(Ship))\n\t\t\t{\n\t\t\t\tFound.push_back(&Ship);' \
	$'\t\t\tif (MayPick(Ship) || (!Ship.bShown && Ship.Doing == SeaShow::Fate::Line))\n\t\t\t{\n\t\t\t\tFound.push_back(&Ship);'
# The director (2a, 2c, 2d).
add M15 "one byte changed in PlanShots" "$SCENE" \
	'Trial.Eye = SeaShow::Vec(Mid, Mid - 3000.0, 900.0);' \
	'Trial.Eye = SeaShow::Vec(Mid, Mid - 3000.0, 901.0);'
add M16 "the switch taken in the month's scene and picture runs" "$GLUE" \
	'Out.bOn = bFullBattle && One.Value == "1";' \
	'Out.bOn = (bFullBattle || true) && One.Value == "1";'
add M17 "the lens left vertical" "$GLUE" \
	'const double Want = HorizontalLens(P.Fov, Aspect);' \
	'const double Want = P.Fov;'
add M18 "rays hit hidden hulls" "$GLUE" \
	$'\t\t\tif (!MayPick(Ship))\n\t\t\t{\n\t\t\t\tcontinue;\n\t\t\t}\n\t\t\tconst Vec Mid = PlaceOf(Ship);\n\t\t\tconst double Far' \
	$'\t\t\tif (!MayPick(Ship) && Ship.bShown)\n\t\t\t{\n\t\t\t\tcontinue;\n\t\t\t}\n\t\t\tconst Vec Mid = PlaceOf(Ship);\n\t\t\tconst double Far'
# The critic's (mark C18).
add M19 "a hidden ship's place nudges the pose" "$GLUE" \
	$'\t\tFeed.FrameId = 0;' \
	$'\t\tFeed.FrameId = 0;\n\t\tfor (const SeaShow::ShipOut& Hid : Ships)\n\t\t{\n\t\t\tFeed.PanRight += Hid.bShown ? 0.0 : 1e-9 * Hid.At.X;\n\t\t}'
add M20 "a pick kept after its ship is lost" "$GLUE" \
	$'\t\t\t\tconst SeaShow::ShipOut* Ship = Find(Ships, St.PickKey);\n\t\t\t\tif (!Ship || !MayPick(*Ship))' \
	$'\t\t\t\tconst SeaShow::ShipOut* Ship = Find(Ships, St.PickKey);\n\t\t\t\tif (!Ship)'
add M21 "UnderWindow left as the director's while free" "$SCENE" \
	$'\t\tif (bFreeNow)\n\t\t{\n\t\t\tUnderWindow = INDEX_NONE;\n\t\t}\n' \
	''
add M22 "the director's window term kept in the free cut" "$GLUE" \
	'return bEyeCut || bPoseCut;' \
	'return bEyeCut || bPoseCut || (bWindowChanged && bMoved);'
add M23 "the floor not held at max(drawn, 0)" "$GLUE" \
	'return std::isfinite(Z) ? std::max(Z, 0.0) : 0.0;' \
	'return std::isfinite(Z) ? Z : 0.0;'
add M24 "the first locked frame's velocity guessed" "$GLUE" \
	$'\t\t\tIn.bAnchorVel = St.L.bLast && Dt > 0.0;\n\t\t\tIn.AnchorVel = In.bAnchorVel ? (At - St.L.Last) * (1.0 / Dt) : Vec{};' \
	$'\t\t\tIn.bAnchorVel = Dt > 0.0;\n\t\t\tIn.AnchorVel = St.L.bLast ? (At - St.L.Last) * (1.0 / Dt) : Vec{ 8.0 * std::sin(Heading), 8.0 * std::cos(Heading), 0.0 };'
add M25 "C keeps the lock" "$GLUE" \
	$'\t\tSt.PickKey = St.PickPiece = -1;\n\t\tLetGo(St);' \
	$'\t\tSt.PickKey = St.PickPiece = -1;'
add M26 "the lens eased over a fixed 0.6 s" "$GLUE" \
	'const double Step = Swing * Dt;' \
	'const double Step = Left * std::min(1.0, Dt / 0.6);'
add M27 "J's repeat honoured" "$GLUE" \
	'H.bJ = H.bJ || (bDown && !bRepeat);' \
	'H.bJ = H.bJ || bDown;'
add M28 "the switch rows built with OpenOn's take removed" "$SCENE" \
	'const SeaFreeEye::Taken FreeTaken = SeaFreeEye::Take(SeaLook, !bMonthScene && ShotFolder.IsEmpty());' \
	'const SeaFreeEye::Taken FreeTaken;'
add M29 "one byte changed in AimEye's director part" "$SCENE" \
	'From.Z = FMath::Max(From.Z, 3.0 * UnitsPerMetre);' \
	'From.Z = FMath::Max(From.Z, 2.0 * UnitsPerMetre);'
add M30 "funnels chosen again on the director's shot while free" "$SCENE" \
	'TicksSinceCut == 0, FreeHero)' \
	'TicksSinceCut == 0, FreeHero) || ShotNow != FunnelsShot'
# The build's own (this branch's fixes, each with the check that found it).
add M31 "a lock that took the camera sent on the take's own frame" "$GLUE" \
	$'\t\tif (bBegin)\n\t\t{\n\t\t\tSt.L.bLast = false;\n\t\t}\n\t\telse\n\t\t{\n\t\t\tFeedLock(St, Ships, Long, Dt, Free.Last().Eye, Feed);\n\t\t}' \
	$'\t\tFeedLock(St, Ships, Long, Dt, Free.Last().Eye, Feed);'
add M32 "a lock's clearance all at once" "$GLUE" \
	'In.AnchorClear = std::min(Clear, St.L.ClearFrom + ClearGrows * St.L.ClearFor);' \
	'In.AnchorClear = Clear + 0.0 * (St.L.ClearFrom + ClearGrows * St.L.ClearFor);'
add M33 "C's own frame left to the take" "$GLUE" \
	$'\t\tif (H.bC)\n\t\t{\n\t\t\tSpent(H);' \
	$'\t\tif (H.bC && false)\n\t\t{\n\t\t\tSpent(H);'
add M34 "a take over the ceiling left to the core's let down" "$GLUE" \
	'if (From.Z > Free.Set.EyeMost)' \
	'if (From.Z > Free.Set.EyeMost + 1e9)'
add M35 "the lens eased on the take's own frame" "$GLUE" \
	$'\t\tif (bBegin)\n\t\t{\n\t\t\tSt.Lens = In.DirLens;\n\t\t}\n\t\telse if (P.bCut)' \
	$'\t\tif (P.bCut && !bBegin)'
add M36 "the drive's height counting the director's frames" "$GLUE" \
	$'\t\t\tif (bFree)\n\t\t\t{\n\t\t\t\tD.Highest = std::max(D.Highest, EyeZ);' \
	$'\t\t\tif (bFree || EyeZ > -1e9)\n\t\t\t{\n\t\t\t\tD.Highest = std::max(D.Highest, EyeZ);'
# LOCK ON is J in all three scenes (the UI director, 27 Nov 06:30Z): L stays the campaign's (Forces).
add M37 "the sea screen taking L for the lock again" "Battle/SeaBattleScreen.cpp" \
	'Key == EKeys::J ? KeyName::J :' \
	'Key == EKeys::L ? KeyName::J :'
# The second review's (27 Nov): the verifier's own breakages V1 to V5, and one for each mend after it.
add V1 "the screen answering Handled for every key while free" "Battle/SeaBattleScreen.cpp" \
	$'\treturn FreeKeyOf(Event, true) ? FReply::Handled() : FReply::Unhandled();' \
	$'\treturn FreeKeyOf(Event, true) || (Battle.IsValid() && Battle->HasFreeEye()) ? FReply::Handled() : FReply::Unhandled();'
add V2 "J's nearest in the picture passing over by Doing only" "$GLUE" \
	$'\t\t\t\tif (!MayPick(Ship))\n\t\t\t\t{\n\t\t\t\t\tcontinue;\n\t\t\t\t}\n\t\t\t\tconst Vec Way = PlaceOf(Ship) - Eye;' \
	$'\t\t\t\tif (!(Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping))\n\t\t\t\t{\n\t\t\t\t\tcontinue;\n\t\t\t\t}\n\t\t\t\tconst Vec Way = PlaceOf(Ship) - Eye;'
add V3 "the drive's worst whole second its best" "$GLUE" \
	'std::min(Worst, static_cast<double>(D.StepSecondFrames[K][Second]))' \
	'std::max(Worst, static_cast<double>(D.StepSecondFrames[K][Second]))'
add V3b "the drive's whole seconds counted with no slack" "$GLUE" \
	'std::floor(std::max(0.0, D.T - D.Starts[K] + DriveSlack))' \
	'std::floor(std::max(0.0, D.T - D.Starts[K]))'
add V4 "a release Handled though the screen never took its press" "$GLUE" \
	'return bTook && Out != KeyTaken::NotMine;' \
	'return (bTook || true) && Out != KeyTaken::NotMine;'
add V4b "a chord's press taken by the screen" "$GLUE" \
	'if ((St.ScreenKeys & Bit) == 0u && bChord)' \
	'if ((St.ScreenKeys & Bit) == 0u && bChord && false)'
add V5 "the wheel's sign flipped at the screen" "Battle/SeaBattleScreen.cpp" \
	$'\tBattle->FreeWheel(Event.GetWheelDelta());' \
	$'\tBattle->FreeWheel(-Event.GetWheelDelta());'
add M38 "a let go dropping her clearance at once" "$GLUE" \
	'if (!bBegin && Free.Locked() >= 0 && !Feed.bLock && Free.WantZoom() < Free.Zoom())' \
	'if (!bBegin && Free.Locked() >= 0 && !Feed.bLock && Free.WantZoom() < Free.Zoom() && false)'
add M39 "Rise fed whole against the ceiling" "$GLUE" \
	'if (Feed.Rise > 0.0)' \
	'if (Feed.Rise > 0.0 && false)'
add M40 "a Tab's lock leaving an older click's pick" "$GLUE" \
	$'\t\t\tSt.PickKey = Now.Key;\n\t\t\tSt.PickPiece = Now.Key >= 0 ? -1 : Now.Piece;\n' \
	''
add M41 "U's dive aimed from where the free eye was flown" "$SCENE" \
	'Under->PressU(bShift, State->Show, State->BattleT, bFreeNow ? FreeDirAt : EyeAt);' \
	'Under->PressU(bShift, State->Show, State->BattleT, EyeAt);'

# replace <file> <piece> <what it becomes>: exactly once, or it says how many and changes nothing.
replace() {
  python3 -I - "$1" "$2" "$3" <<'PY'
import sys
Path, Old, New = sys.argv[1], sys.argv[2], sys.argv[3]
with open(Path, encoding="utf-8") as F:
    Text = F.read()
N = Text.count(Old)
if N != 1:
    print(N)
    sys.exit(3)
with open(Path, "w", encoding="utf-8") as F:
    F.write(Text.replace(Old, New, 1))
PY
}

run_copy() {
  SEA_FREE_EYE_SRC="$WORK/src" SEA_FREE_EYE_ONE=1 SEA_FREE_EYE_OUT="$WORK/out" nice -n 12 "$HERE/run.sh" --quick "${RECS[@]}" > "$WORK/run.txt" 2>&1
}

# The copy, and the copy unbroken first: it must pass, or every breakage would fail for nothing.
rm -rf "$WORK/src"
cp -r "$SRC" "$WORK/src"
if ! run_copy; then
  echo "the unbroken copy does not pass: nothing to break"; tail -n 20 "$WORK/run.txt"
  exit 2
fi
echo "the unbroken copy: $(tail -n 1 "$WORK/run.txt")"

WANT=("$@")
BAD=0
RAN=0
printf "%-4s %-58s %s\n" "" "breakage" "caught by"
for K in "${!IDS[@]}"; do
  ID="${IDS[$K]}"
  if [ "${#WANT[@]}" -gt 0 ]; then
    Hit=0
    for W in "${WANT[@]}"; do [ "$W" = "$ID" ] && Hit=1; done
    [ $Hit -eq 1 ] || continue
  fi
  RAN=$((RAN + 1))
  F="$WORK/src/${FILES[$K]}"
  cp "$SRC/${FILES[$K]}" "$F"
  if ! N="$(replace "$F" "${OLDS[$K]}" "${NEWS[$K]}")"; then
    printf "%-4s %-58s %s\n" "$ID" "${WHATS[$K]}" "ANCHOR GONE (found ${N:-?} times)"
    BAD=$((BAD + 1))
    continue
  fi
  run_copy
  STATUS=$?
  cp "$SRC/${FILES[$K]}" "$F"
  CAUGHT="$(grep -E '^  FAIL' "$WORK/run.txt" | awk '{ print $2 }' | sort -u | tr '\n' ' ')"
  if grep -q "error:" "$WORK/run.txt"; then
    printf "%-4s %-58s %s\n" "$ID" "${WHATS[$K]}" "BUILD FAILED: $(grep -m 1 'error:' "$WORK/run.txt" | cut -c1-120)"
    BAD=$((BAD + 1))
  elif [ $STATUS -ne 0 ] && [ -n "$CAUGHT" ]; then
    printf "%-4s %-58s %s\n" "$ID" "${WHATS[$K]}" "$CAUGHT($(tail -n 1 "$WORK/run.txt" | sed 's/^sea_free_eye_test: //'))"
  else
    printf "%-4s %-58s %s\n" "$ID" "${WHATS[$K]}" "PASSED: the checks did not see it (exit $STATUS)"
    BAD=$((BAD + 1))
  fi
done
echo "sea_free_eye breakages: $RAN run, $BAD not caught"
[ $BAD -eq 0 ]
