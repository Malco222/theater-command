#!/bin/bash
# Deliberate breakages of Battle/FreeEye.cpp, one at a time on a copy: each must make the cloud test FAULT. Each is a
# literal piece of the core and what it becomes; a piece not found exactly once is reported (ANCHOR GONE) and counts
# as a breakage passing, so a change to the core that moves a line makes this fail loudly instead of testing nothing.
# Prints a table; exits non-zero if any breakage passes. Usage: Tools/free_eye/breakages.sh [name ...]
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CORE="$ROOT/Source/TheaterCommand/Battle/FreeEye.cpp"
WORK="${FREE_EYE_BREAK:-${TMPDIR:-/tmp}/free_eye_break}"
mkdir -p "$WORK"

NAMES=()
OLDS=()
NEWS=()
add() { NAMES+=("$1"); OLDS+=("$2"); NEWS+=("$3"); }

# The caps (rule 10).
add "tilt cap (want and hard)" \
	'WantT = std::clamp(WantT, Set.TiltLeast * Rad, Set.TiltMost * Rad);
		WantS = std::clamp(WantS, 0.0, 1.0);' \
	'WantT = std::clamp(WantT, -3.0, 3.0);
		WantS = std::clamp(WantS, 0.0, 1.0);'
add "tilt cap (hard only)" \
	'T = std::clamp(T, Set.TiltLeast * Rad, Set.TiltMost * Rad);' \
	'T = std::clamp(T, -3.0, 3.0) + 0.02;'
add "boom cap" \
	'return Near * std::pow(Far / Near, std::clamp(AtZoom, 0.0, 1.0));' \
	'return Near * std::pow(Far / Near, AtZoom * 1.1 - 0.05);'
add "ceiling, soft" \
	'if (Over > 0.0) WantRelV.Z -= Over;' \
	'if (Over > 1e300) WantRelV.Z -= Over;'
add "ceiling, hard" \
	'const double Above = EyeNow().Z - Set.EyeMost;' \
	'const double Above = EyeNow().Z - Set.EyeMost - 50.0;'
add "box, soft" \
	'if (bBox) WantRelV = InBox(RefAt + WantRelV + Up) - RefAt - Up;' \
	'if (bBox) WantRelV = WantRelV + Up - Up;'
add "box, hard" \
	'const Vec Kept = InBox(Pv);' \
	'const Vec Kept = Pv;'
add "anchor clearance, soft" \
	'if (!Keeps(SGoal))' \
	'if (!Keeps(SGoal) && Clear > 1e9)'
add "anchor clearance, hard" \
	'if (Near >= Clear) break;' \
	'if (Near >= 0.0) break;'
# The floor (rule 9).
add "floor's hard clamp" \
	'const double Under = FloorAt(InWorld, E.X, E.Y) + Set.FloorHard - E.Z;' \
	'const double Under = FloorAt(InWorld, E.X, E.Y) + Set.FloorHard - E.Z - 50.0;'
add "lift off" \
	'const double LiftGoal = std::max(0.0, Highest + Set.FloorClear - Eye0.Z);' \
	'const double LiftGoal = 0.0 * std::max(0.0, Highest + Set.FloorClear - Eye0.Z);'
add "lift slow up" \
	'Goal > LiftZ ? Set.LiftRise : Set.LiftSettle' \
	'Goal > LiftZ ? Set.LiftSettle : Set.LiftSettle'
add "lift not led" \
	'const double Goal = LiftGoal > 0.0 ? LiftGoal + Lead : 0.0;' \
	'const double Goal = LiftGoal > 0.0 ? LiftGoal + 0.0 * Lead : 0.0;'
add "lift unheld (a step)" \
	'Held(LiftZ, SwingLift, Goal, Goal > LiftZ ? Set.LiftRise : Set.LiftSettle, 1e9, Set.LiftPush * FeelNow, Set.LiftPush * FeelNow, Dt);' \
	'Held(LiftZ, SwingLift, Goal, Goal > LiftZ ? Set.LiftRise : Set.LiftSettle, 1e9, Set.LiftPush * FeelNow, Set.LiftPush * FeelNow, Dt); LiftZ = Goal;'
# Lock, carry, let go, teleport (rules 3 to 6).
add "cut distance" \
	'if (bLockNow && (WasPivot - NewRef).Length() > Set.CutBeyond)' \
	'if (bLockNow && (WasPivot - NewRef).Length() > Set.CutBeyond * 10.0)'
add "carry of place" \
	'RelV = WasPivot - (NewRef - NewVel * Dt);' \
	'RelV = WasPivot - NewRef + (WasPivot - NewRef) * 0.0 - (WasPivot - NewRef);'
add "carry of speed" \
	'SwingRel = WasVel - NewVel;' \
	'SwingRel = (WasVel - NewVel) * 0.0;'
add "carry of a measured speed" \
	'SwingRel -= NewVel;' \
	'SwingRel -= NewVel * 0.0;'
add "carry from the frame's own speed" \
	'const Vec WasVel = (!bLocked && !bFrameNew ? FrameVel : RefVel) + SwingRel;' \
	'const Vec WasVel = (!bLocked && !bFrameNew ? RefVel : RefVel) + SwingRel;'
add "let go comes to rest at once" \
	'WantRelV = bLocked ? Vec{} : RelV + SwingRel * (1.0 / std::max(Set.Ease, 1e-3));' \
	'WantRelV = bLocked ? Vec{} : WasPivot - NewRef;'
add "lock takes again without a rise" \
	'const bool bRiseIn = In.bLock && (!bInLockLast || In.AnchorId != InIdLast);' \
	'const bool bRiseIn = In.bLock && (!bLocked || In.AnchorId != InIdLast);'
add "teleport" \
	'bTeleport = (AnchorNow - AnchorLast).Length() > Set.TeleportBeyond + Speed * Dt * 3.0;' \
	'bTeleport = (AnchorNow - AnchorLast).Length() > Set.TeleportBeyond * 100.0 + Speed * Dt * 3.0;'
add "teleport on a fast anchor's hitch" \
	'bTeleport = (AnchorNow - AnchorLast).Length() > Set.TeleportBeyond + Speed * Dt * 3.0;' \
	'bTeleport = (AnchorNow - AnchorLast).Length() > Set.TeleportBeyond + Speed * 0.0 * Dt * 3.0;'
add "pan's lock rule" \
	'else if (bLocked && bHand && !Set.bPanKeepsLock)' \
	'else if (bLocked && bHand && Set.bPanKeepsLock && !Set.bPanKeepsLock)'
# Ride and steady height (rules 7 and 8).
add "ride" \
	'H += Turned;
			WantH += Turned;' \
	'H += 0.0 * Turned;
			WantH += 0.0 * Turned;'
add "steady height" \
	'SteadyRefZ += (AnchorNow.Z - SteadyRefZ) * (1.0 - std::exp(-Dt / Set.SteadyZ));' \
	'SteadyRefZ = AnchorNow.Z;'
# The holds (rule 2).
add "turn's push" \
	'Held(H, SwingH, WantH, Set.TurnEase, FastH * Share, Set.TurnPush * Rad * Share, Set.TurnPush * Rad, Dt);' \
	'Held(H, SwingH, WantH, Set.TurnEase, FastH * Share, 1e9, 1e9, Dt);'
add "turn's fastest" \
	'Held(H, SwingH, WantH, Set.TurnEase, FastH * Share, Set.TurnPush * Rad * Share, Set.TurnPush * Rad, Dt);' \
	'Held(H, SwingH, WantH, Set.TurnEase, 1e9, Set.TurnPush * Rad * Share, Set.TurnPush * Rad, Dt);'
add "tilt's push" \
	'Held(T, SwingT, WantT, Set.TiltEase, FastT * Share, Set.TiltPush * Rad * Share, Set.TiltPush * Rad, Dt);' \
	'Held(T, SwingT, WantT, Set.TiltEase, FastT * Share, 1e9, 1e9, Dt);'
add "zoom's push" \
	'Held(S, SwingS, SGoal, Set.ZoomEase, FastS * Share, Set.ZoomPush * Share, Set.ZoomPush, Dt);' \
	'Held(S, SwingS, SGoal, Set.ZoomEase, FastS * Share, 1e9, 1e9, Dt);'
add "zoom's fastest" \
	'Held(S, SwingS, SGoal, Set.ZoomEase, FastS * Share, Set.ZoomPush * Share, Set.ZoomPush, Dt);' \
	'Held(S, SwingS, SGoal, Set.ZoomEase, 1e9, Set.ZoomPush * Share, Set.ZoomPush, Dt);'
add "offset's push (pan, slide, glide)" \
	'HeldVec(RelV, SwingRel, WantRelV, Set.Ease, Set.GlideFastest * Feel, Set.GlidePush * Feel, Dt, 1e-10 * Feel);' \
	'HeldVec(RelV, SwingRel, WantRelV, Set.Ease, Set.GlideFastest * Feel, 1e9, Dt, 1e-10 * Feel);'
add "offset's fastest (the glide)" \
	'HeldVec(RelV, SwingRel, WantRelV, Set.Ease, Set.GlideFastest * Feel, Set.GlidePush * Feel, Dt, 1e-10 * Feel);' \
	'HeldVec(RelV, SwingRel, WantRelV, Set.Ease, 1e9, Set.GlidePush * Feel, Dt, 1e-10 * Feel);'
add "shared budget of turn, tilt and zoom" \
	'const double Share = 1.0 / std::max(1.0, ' \
	'const double Share = 1.0 + 0.0 * std::max(1.0, '
add "rest to the last bit" \
	'if (std::abs(Want - X) < 1e-11 && std::abs(Swing) < 1e-10)' \
	'if (std::abs(Want - X) < 0.0 && std::abs(Swing) < 1e-10)'
add "exact step where nothing holds" \
	'X = Sw == Free ? X1 : X + 0.5 * (Swing + Sw) * Dt;' \
	'X = Sw == Free + 1e300 ? X1 : X + 0.5 * (Swing + Sw) * Dt;'
# Lens, look, robustness (rules 11, 12, 14).
add "lens" \
	'Now.Fov = Set.FovNear + (Set.FovFar - Set.FovNear) * std::clamp(S, 0.0, 1.0);' \
	'Now.Fov = Set.FovFar + (Set.FovNear - Set.FovFar) * std::clamp(S, 0.0, 1.0);'
add "look's least length" \
	'Now.Look = Now.Eye - Boom * (std::max(Length, Set.LookLeast) / Length);' \
	'Now.Look = Now.Eye - Boom * (Length / Length);'
add "Dt clamp" \
	'const double Dt = std::isfinite(RealDt) ? std::clamp(RealDt, 0.0, 0.25) : 0.0;' \
	'const double Dt = std::isfinite(RealDt) ? std::clamp(RealDt, 0.0, 10.0) : 0.0;'
add "the wrap" \
	'H = WrapNear(H, 0.0);
		WantH = WrapNear(WantH, H);
		T = std::clamp(' \
	'WantH = WrapNear(WantH, H);
		T = std::clamp('
add "numbers not finite" \
	'return std::isfinite(V) ? V : 0.0;' \
	'return V;'
add "Begin's pivot on the line of sight" \
	'const Vec Pivot = From - BoomOf(H, T, ArmOf(S));' \
	'const Vec Pivot = From - BoomOf(H, T, ArmOf(S) * 0.5);'

ONLY=("$@")
RESULTS=()
PASSED=0
TOTAL=0
for I in "${!NAMES[@]}"; do
	NAME="${NAMES[$I]}"
	if [ "${#ONLY[@]}" -gt 0 ]; then
		KEEP=0
		for O in "${ONLY[@]}"; do [ "$O" = "$NAME" ] && KEEP=1; done
		[ "$KEEP" = 1 ] || continue
	fi
	TOTAL=$((TOTAL + 1))
	COPY="$WORK/FreeEye.cpp"
	cp "$CORE" "$COPY"
	FOUND="$(OLD="${OLDS[$I]}" NEW="${NEWS[$I]}" perl -0777 -i -pe 'BEGIN { $o = $ENV{OLD}; $n = $ENV{NEW}; } $c = () = /\Q$o\E/g; if ($c == 1) { s/\Q$o\E/$n/; } END { print STDERR $c; }' "$COPY" 2>&1 >/dev/null)"
	if [ "$FOUND" != "1" ]; then
		RESULTS+=("$(printf '%-44s ANCHOR GONE (found %s times): this breakage tests nothing' "$NAME" "$FOUND")")
		PASSED=$((PASSED + 1))
		continue
	fi
	LOG="$WORK/log.txt"
	if FREE_EYE_SRC="$COPY" FREE_EYE_ONE=1 FREE_EYE_OUT="$WORK/out" nice -n 10 "$HERE/run.sh" > "$LOG" 2>&1; then
		RESULTS+=("$(printf '%-44s PASSED: the test did not see it' "$NAME")")
		PASSED=$((PASSED + 1))
	else
		if grep -q "^FAULT" "$LOG"; then
			FAULTS="$(grep -c '^FAULT' "$LOG")"
			FIRST="$(grep -m 1 '^FAULT' "$LOG" | cut -c 8-110)"
			RESULTS+=("$(printf '%-44s caught, %3d faults: %s' "$NAME" "$FAULTS" "$FIRST")")
		else
			RESULTS+=("$(printf '%-44s caught (it does not build or the test fell over): %s' "$NAME" "$(grep -m 1 -E 'error|Assert|Segmentation' "$LOG" | cut -c 1-90)")")
		fi
	fi
done
printf '%s\n' "${RESULTS[@]}"
echo "$((TOTAL - PASSED)) of $TOTAL breakages caught"
[ "$PASSED" -eq 0 ]
