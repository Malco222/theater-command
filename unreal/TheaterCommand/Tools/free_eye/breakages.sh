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
	'T = std::clamp(T, std::min(Set.TiltLeast * Rad, TiltWas), std::max(Set.TiltMost * Rad, TiltWas));' \
	'T = std::clamp(T, -3.0, 3.0) + 0.0 * TiltWas;'
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
	'WantRelV = Vec{ Hold(P.X, Lo.X, Hi.X), Hold(P.Y, Lo.Y, Hi.Y), Hold(P.Z, Lo.Z, Hi.Z) } - RefAt - Up;' \
	'WantRelV = Vec{ P.X, P.Y, P.Z } - RefAt - Up; (void)Hold; (void)Lo; (void)Hi;'
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
	'Set.LiftPush * FeelNow, Dt, LiftGoal > 0.0 ? GoalRate : 0.0);' \
	'Set.LiftPush * FeelNow, Dt, LiftGoal > 0.0 ? GoalRate : 0.0); LiftZ = Goal;'
# Lock, carry, let go, teleport (rules 3 to 6).
add "cut distance" \
	'if (std::max(Off.Length(), Stop.Length()) > Set.CutBeyond) return true;' \
	'if (std::max(Off.Length(), Stop.Length()) > Set.CutBeyond * 10.0) return true;'
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
	'const Vec WasVel = bChordGood ? ChordVel : (!bLocked && !bFrameNew ? FrameVel : RefVel) + SwingRel;' \
	'const Vec WasVel = bChordGood ? ChordVel : (!bLocked && !bFrameNew ? RefVel : RefVel) + SwingRel; (void)FrameVel;'
add "let go comes to rest at once" \
	'WantRelV = bLocked ? Vec{} : StopOf(RelV, SwingRel);' \
	'WantRelV = bLocked ? Vec{} : WasPivot - NewRef;'
add "lock takes again without a rise" \
	'const bool bRiseIn = In.bLock && (!bInLockLast || In.AnchorId != InIdLast);' \
	'const bool bRiseIn = In.bLock && (!bLocked || In.AnchorId != InIdLast);'
add "teleport" \
	'bTeleport = Jumped(AnchorNow - AnchorLast, Known, !bFed && !bMeasureNext);' \
	'bTeleport = Jumped(AnchorNow - AnchorLast, Known, !bFed && !bMeasureNext) && false;'
add "teleport on a fast anchor's hitch" \
	'const double Slack = Set.TeleportBeyond + Known.Length() * Dt * 3.0;' \
	'const double Slack = Set.TeleportBeyond + 0.0 * Known.Length() * Dt * 3.0;'
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
	'Held(T, SwingT, TGoal, Set.TiltEase, FastT * Share, Set.TiltPush * Rad * Share, Set.TiltPush * Rad, Dt);' \
	'Held(T, SwingT, TGoal, Set.TiltEase, FastT * Share, 1e9, 1e9, Dt);'
add "zoom's push" \
	'Held(S, SwingS, SGoal, Set.ZoomEase, FastS * Share, Set.ZoomPush * Share, Set.ZoomPush, Dt);' \
	'Held(S, SwingS, SGoal, Set.ZoomEase, FastS * Share, 1e9, 1e9, Dt);'
add "zoom's fastest" \
	'Held(S, SwingS, SGoal, Set.ZoomEase, FastS * Share, Set.ZoomPush * Share, Set.ZoomPush, Dt);' \
	'Held(S, SwingS, SGoal, Set.ZoomEase, 1e9, Set.ZoomPush * Share, Set.ZoomPush, Dt);'
add "offset's push (pan, slide, glide)" \
	'HeldVec(RelV, SwingRel, WantRelV, Set.Ease, Fastest, Set.GlidePush * Feel, Dt, 1e-10 * Feel);' \
	'HeldVec(RelV, SwingRel, WantRelV, Set.Ease, Fastest, 1e9, Dt, 1e-10 * Feel);'
add "offset's fastest (the glide)" \
	'HeldVec(RelV, SwingRel, WantRelV, Set.Ease, Fastest, Set.GlidePush * Feel, Dt, 1e-10 * Feel);' \
	'HeldVec(RelV, SwingRel, WantRelV, Set.Ease, 1e9 + 0.0 * Fastest, Set.GlidePush * Feel, Dt, 1e-10 * Feel);'
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
		// (out of its range' \
	'WantH = WrapNear(WantH, H);
		// (out of its range'
add "numbers not finite" \
	'return std::isfinite(V) ? V : 0.0;' \
	'return V;'
add "Begin's pivot on the line of sight" \
	'Vec Pivot = From - BoomOf(H, T, ArmOf(S));' \
	'Vec Pivot = From - BoomOf(H, T, ArmOf(S) * 0.5);'

# Review round 1: what the test did not see, and the mechanisms that mended what it found.
# His hand, by the amount it says (rule 2).
add "turn in radians, not degrees" \
	'WantH += std::clamp(Clean(In.Turn), -3600.0, 3600.0) * Rad;' \
	'WantH += std::clamp(Clean(In.Turn), -3600.0, 3600.0) * Rad * 57.29577951308232;'
add "turn reversed" \
	'WantH += std::clamp(Clean(In.Turn), -3600.0, 3600.0) * Rad;' \
	'WantH -= std::clamp(Clean(In.Turn), -3600.0, 3600.0) * Rad;'
add "tilt reversed" \
	'WantT += std::clamp(Clean(In.Tilt), -3600.0, 3600.0) * Rad;' \
	'WantT -= std::clamp(Clean(In.Tilt), -3600.0, 3600.0) * Rad;'
add "pan right reversed" \
	'const Vec Right{ std::cos(H), -std::sin(H), 0.0 };' \
	'const Vec Right{ -std::cos(H), std::sin(H), 0.0 };'
add "ahead's axes swapped" \
	'const Vec Ahead{ std::sin(H), std::cos(H), 0.0 };' \
	'const Vec Ahead{ std::cos(H), std::sin(H), 0.0 };'
add "slide right reversed" \
	'WantRelV += Right * (std::clamp(Clean(In.SlideRight)' \
	'WantRelV -= Right * (std::clamp(Clean(In.SlideRight)'
add "slide ignores the aspect" \
	'Tall * Aspect) +' \
	'Tall * (Aspect * 0.0 + 1.0)) +'
add "slide by the full lens as the half" \
	'const double Tall = 2.0 * ArmNow * std::tan(0.5 * Now.Fov * Rad);' \
	'const double Tall = 2.0 * ArmNow * std::tan(Now.Fov * Rad);'
add "slide by the wanted boom" \
	'const double Tall = 2.0 * ArmNow * std::tan(0.5 * Now.Fov * Rad);' \
	'const double Tall = 2.0 * ArmOf(WantS) * std::tan(0.5 * Now.Fov * Rad);'
add "rise reversed" \
	'WantRelV.Z += Rise * Set.RiseRate * Reach * Dt;' \
	'WantRelV.Z -= Rise * Set.RiseRate * Reach * Dt;'
add "rise at the pan's rate" \
	'WantRelV.Z += Rise * Set.RiseRate * Reach * Dt;' \
	'WantRelV.Z += Rise * Set.PanRate * Reach * Dt;'
add "pan's least boom dropped" \
	'const double Reach = std::max(ArmNow, Set.PanArmLeast);' \
	'const double Reach = std::max(ArmNow, Set.PanArmLeast * 0.0);'
add "zoom step not ZoomStep" \
	'WantS -= std::clamp(Clean(In.Zoom), -100.0, 100.0) * Set.ZoomStep;' \
	'WantS -= std::clamp(Clean(In.Zoom), -100.0, 100.0) * 0.05;'
add "ground style: a slide keeps the lock" \
	'Clean(In.SlideRight) != 0.0 || Clean(In.SlideAhead) != 0.0;' \
	'Clean(In.SlideAhead) != 0.0;'
add "lens from the wanted zoom" \
	'Now.Fov = Set.FovNear + (Set.FovFar - Set.FovNear) * std::clamp(S, 0.0, 1.0);' \
	'Now.Fov = Set.FovNear + (Set.FovFar - Set.FovNear) * std::clamp(WantS, 0.0, 1.0);'
# The springs and holds.
add "tilt's spring at the turn's rate" \
	'Held(T, SwingT, TGoal, Set.TiltEase,' \
	'Held(T, SwingT, TGoal, Set.TurnEase,'
add "tilt's fastest halved" \
	'FastT = std::max(Set.TiltFastest * Rad, 1e-6);' \
	'FastT = std::max(Set.TiltFastest * Rad * 0.5, 1e-6);'
add "FeelArm dropped from the offset's holds" \
	'const double Feel = std::max(ArmNow, Set.FeelArm);' \
	'const double Feel = std::max(ArmNow, Set.FeelArm * 0.0);'
add "held stop from 4x the brake" \
	'Closing >= 0.9 * std::sqrt(2.0 * Brake * Dist)' \
	'Closing >= 0.9 * std::sqrt(8.0 * Brake * Dist)'
add "held, even brake set on its want never" \
	'if (std::abs(Step) >= Dist && (Slow < Brake || std::abs(Sw) <= Brake * Dt))' \
	'if (std::abs(Step) >= Dist && Slow < 0.0)'
add "held point's stop from 4x the push" \
	'8.0 * Push * Dist - 4.0 * PushDt * Swing.Length())) - PushDt);' \
	'32.0 * Push * Dist - 4.0 * PushDt * Swing.Length())) - PushDt);'
add "held point set on its want only under the push" \
	'>= Dist && (Brake < Push || Sw.Length() <= Push * Dt))' \
	'>= Dist && (Brake < Push))'
# Want, Begin, RealDt, the world (rules 13 and 14).
add "Want lets a tilt not finite through" \
	'if (std::isfinite(InTilt)) WantT = std::clamp(InTilt, Set.TiltLeast * Rad, Set.TiltMost * Rad);' \
	'if (!std::isnan(InTilt)) WantT = std::clamp(InTilt, Set.TiltLeast * Rad, Set.TiltMost * Rad);'
add "Begin's tilt clamped on the first frame" \
	'T = std::asin(std::clamp(-Dir.Z, -1.0, 1.0));' \
	'T = std::clamp(std::asin(std::clamp(-Dir.Z, -1.0, 1.0)), Set.TiltLeast * Rad, Set.TiltMost * Rad);'
add "second Begin keeps the swings" \
	'SwingH = SwingT = SwingS = 0.0;
		// (the pivot' \
	'// (the pivot'
add "second Begin keeps the lift" \
	'LiftZ = SwingLift = 0.0;
		LiftGoalLast' \
	'LiftGoalLast'
add "second Begin keeps the offset's swing" \
	'SwingRel = Vec{};
		LiftZ = SwingLift' \
	'LiftZ = SwingLift'
add "Begin over the ceiling not let down" \
	'if (bBeginCut) Pivot.Z -= Above;' \
	'if (bBeginCut) Pivot.Z -= 0.0;'
add "Begin under the floor not said a cut" \
	'if (bFirst && !bCutNow && Eye0.Z + LiftZ < Highest + Set.FloorHard)' \
	'if (bFirst && !bCutNow && Eye0.Z + LiftZ < Highest + Set.FloorHard - 1e9)'
add "the first frame's lift eased" \
	'else if (!bFirst)
		{' \
	'else
		{'
add "a frame of no time repeats a cut" \
	'Same.bCut = false;' \
	'Same.bCut = Same.bCut;'
add "a frame of no time drops the floor" \
	'const double Under = FloorAt(InWorld, Now.Eye.X, Now.Eye.Y) + Set.FloorHard - Now.Eye.Z;' \
	'const double Under = FloorAt(InWorld, Now.Eye.X, Now.Eye.Y) + Set.FloorHard - Now.Eye.Z - 1e9;'
add "RealDt not finite as a quarter second" \
	'const double Dt = std::isfinite(RealDt) ? std::clamp(RealDt, 0.0, 0.25) : 0.0;' \
	'const double Dt = std::isfinite(RealDt) ? std::clamp(RealDt, 0.0, 0.25) : 0.25;'
add "a floor not finite let through" \
	'if (std::isfinite(F)) return F;' \
	'if (!std::isnan(F) || true) return F;'
add "FloorFlat ignored" \
	'return Set.FloorFlat;' \
	'return Set.FloorFlat * 0.0;'
# Frames, carries, cuts, teleports (rules 3, 5, 6).
add "a new FrameId ignored" \
	'const bool bFrameNew = bFrameJump || (bFrameIdKnown && In.FrameId != FrameIdLast);' \
	'const bool bFrameNew = bFrameJump;'
add "a frame's jump taken as its speed" \
	'const bool bFrameNew = bFrameJump || (bFrameIdKnown && In.FrameId != FrameIdLast);' \
	'const bool bFrameNew = (bFrameJump && false) || (bFrameIdKnown && In.FrameId != FrameIdLast);'
add "carry not from the pivot's own speed" \
	'bChordGood = bPivotEndGood && !bCutNow && HardStopCount == StopsWas;' \
	'bChordGood = false && bPivotEndGood && !bCutNow && HardStopCount == StopsWas;'
add "carry from a speed across a cut" \
	'bChordGood = bPivotEndGood && !bCutNow && HardStopCount == StopsWas;' \
	'bChordGood = bPivotEndGood && StopsWas >= 0;'
add "let go comes to rest at v / Ease only" \
	'return At + Swing * std::max(1.0 / std::max(Set.Ease, 1e-3), Swing.Length() / (2.0 * GlidePush));' \
	'return At + Swing * (1.0 / std::max(Set.Ease, 1e-3));'
add "coast's rest not set once measured" \
	'if (bCoast)
			{
				WantRelV = StopOf(RelV, SwingRel);' \
	'if (bCoast)
			{'
add "bLetGo ignored on a rise of bLock" \
	'if (bRiseIn && !In.bLetGo && bAnchorGood)' \
	'if (bRiseIn && bAnchorGood)'
add "a lock on an anchor not finite" \
	'if (bRiseIn && !In.bLetGo && bAnchorGood)' \
	'if (bRiseIn && !In.bLetGo)'
add "a cut keeps the turn, tilt and zoom swings" \
	'RelV = WantRelV = SwingRel = Vec{};
			SwingH = SwingT = SwingS = SwingLift = 0.0;' \
	'RelV = WantRelV = SwingRel = Vec{};
			SwingLift = 0.0;'
add "a cut judged from his want" \
	'const Vec Off = WasPivot - NewRef;' \
	'const Vec Off = RefAt + WantRelV - NewRef;'
add "a glide of any length" \
	'GlideFast / GlidePush > Set.GlideLongest) return true;' \
	'GlideFast / GlidePush > Set.GlideLongest * 1e9) return true;'
add "a glide through the anchor's clearance" \
	'return Nearest(Off + EyeBy, Stop + EyeBy) < Clear;' \
	'return Nearest(Off + EyeBy, Stop + EyeBy) < 0.0;'
add "a teleport keeps the offset's swing" \
	'bCutNow = true;
			SwingRel = Vec{};
			SwingLift = 0.0;' \
	'bCutNow = true;
			SwingLift = 0.0;'
add "a teleport's slack one frame of speed" \
	'const double Slack = Set.TeleportBeyond + Known.Length() * Dt * 3.0;' \
	'const double Slack = Set.TeleportBeyond + Known.Length() * Dt * 1.0;'
add "a cut keeps the lift" \
	'LiftZ = LiftGoal;' \
	'LiftZ = std::max(LiftZ, LiftGoal);'
# Ride and steady height (rules 7 and 8).
add "ride on the frame of a skip" \
	'if (bLocked && In.bRide && !bLockNow && bHeadingGood && bAnchorHeadingGood)' \
	'if (bLocked && In.bRide && bHeadingGood && bAnchorHeadingGood)'
add "ride does not turn where he sits" \
	'WantRelV = TurnedBy(WantRelV, Turned);' \
	'WantRelV = TurnedBy(WantRelV, 0.0 * Turned);'
add "steady height's time doubled" \
	'(1.0 - std::exp(-Dt / Set.SteadyZ));' \
	'(1.0 - std::exp(-Dt / (2.0 * Set.SteadyZ)));'
# The lift (rule 9).
add "the lift never settles" \
	'const double Goal = LiftGoal > 0.0 ? LiftGoal + Lead : 0.0;' \
	'const double Goal = LiftGoal > 0.0 ? LiftGoal + Lead : LiftZ;'
add "the lift settles at the wrong rate" \
	'Goal > LiftZ ? Set.LiftRise : Set.LiftSettle' \
	'Goal > LiftZ ? Set.LiftRise : Set.LiftSettle * 2.0'
add "the lift's push not by the boom" \
	'Set.LiftPush * FeelNow, Set.LiftPush * FeelNow, Dt, LiftGoal' \
	'Set.LiftPush * (FeelNow * 0.0 + 10.0), Set.LiftPush * (FeelNow * 0.0 + 10.0), Dt, LiftGoal'
add "the hard floor at FloorClear" \
	'const double Under = FloorAt(InWorld, E.X, E.Y) + Set.FloorHard - E.Z;' \
	'const double Under = FloorAt(InWorld, E.X, E.Y) + Set.FloorClear - E.Z;'
add "the lift's look ahead dropped" \
	'const Vec At = Eye0 + Going * (Set.FloorAhead * 0.5 * K);' \
	'const Vec At = Eye0 + Going * (0.0 * K);'
# The caps (rule 10).
add "the ceiling keeps a rising swing" \
	'SwingRel.Z = std::min(SwingRel.Z, 0.0);' \
	'SwingRel.Z = std::min(SwingRel.Z, 1e300);'
add "the ceiling's way not looked along" \
	'BoomUp = std::max(BoomUp, ArmOf(AtS) * std::sin(AtT));' \
	'BoomUp = std::max(BoomUp, 0.0 * ArmOf(AtS) * std::sin(AtT));'
add "the anchor's clearance kept unlocked" \
	'const double Clear = bLocked && std::isfinite(In.AnchorClear)' \
	'const double Clear = std::isfinite(In.AnchorClear)'
add "a box's corners not sorted" \
	'Vec{ std::min(In.BoxMin.X, In.BoxMax.X),' \
	'Vec{ (In.BoxMin.X),'
add "the box not led by the push" \
	'BoxBy / (2.0 * std::max(Set.GlidePush' \
	'0.0 * BoxBy / (2.0 * std::max(Set.GlidePush'
add "his descent sinks under the floor" \
	'if (WantRelV.Z < WantZWas && WantRelV.Z < Least) WantRelV.Z = std::min(WantZWas, Least);' \
	'if (WantRelV.Z < WantZWas && WantRelV.Z < Least - 1e9) WantRelV.Z = std::min(WantZWas, Least);'

# What review round 2 found: pace changes, the measure frame, the box carried and not finite, the lift's climb,
# the point brake, an anchor not finite, Begin again, and the gaps it named.
add "a teleport stops his turn, tilt and zoom" \
	'SwingRel = Vec{};
			SwingLift = 0.0;
			bMeasureNext = false;' \
	'SwingRel = Vec{};
			SwingLift = 0.0;
			SwingH = SwingT = SwingS = 0.0;
			bMeasureNext = false;'
add "no ride on a teleport's frame" \
	'if (bLocked && In.bRide && !bLockNow && bHeadingGood && bAnchorHeadingGood)' \
	'if (bLocked && In.bRide && !bLockNow && !bTeleport && bHeadingGood && bAnchorHeadingGood)'
add "a pace change taken as a jump" \
	'std::max(3.0, Set.PaceJump)' \
	'std::max(3.0, 0.0 * Set.PaceJump)'
add "a step across the way taken as movement" \
	'return Across > Slack || Along < -Slack ||' \
	'return Across > 1e300 || Along < -1e300 ||'
add "the anchor's movement forgotten in a pause" \
	'if (Moved.Length() > 0.0)' \
	'if (true)'
add "the frame's movement forgotten in a pause" \
	'if (FrameVel.Length() > 0.0)' \
	'if (true)'
add "the measure frame's jump let through" \
	'bMeasureNext ? WasVel :' \
	'bMeasureNext ? WasVel * 100.0 :'
add "the box's walls taken as still" \
	'const Vec LoBy = RefVel - LoVel, HiBy = RefVel - HiVel;' \
	'const Vec LoBy = RefVel + LoVel * 0.0, HiBy = RefVel + HiVel * 0.0;'
add "the box's walls taken as riding" \
	'const Vec LoBy = RefVel - LoVel, HiBy = RefVel - HiVel;' \
	'const Vec LoBy = LoVel * 0.0, HiBy = HiVel * 0.0;'
add "a box first given taken as still" \
	'(BoxHi - BoxHiLast) * (1.0 / Dt) : RefVel;' \
	'(BoxHi - BoxHiLast) * (1.0 / Dt) : Vec{};'
add "a box's last bit counted a stop" \
	'if (Out > 1e-9 * std::max(1.0, Pv.Length())) ++HardStopCount;' \
	'if (Out > 0.0) ++HardStopCount;'
add "a box not finite dropped" \
	'const bool bBox = In.bBox && (bBoxGood || bBoxKnown);' \
	'const bool bBox = In.bBox && bBoxGood && (bBoxGood || bBoxKnown);'
add "a box not finite left standing" \
	'BoxLoLast + BoxLoVel * Dt' \
	'BoxLoLast + BoxLoVel * 0.0 * Dt'
add "the held spring blind to its want's speed" \
	'const double Closing = Toward - Away;' \
	'const double Closing = Toward - 0.0 * Away;'
add "the held cap blind to its want's speed" \
	'StopFrom + std::max(Away, 0.0)' \
	'StopFrom + 0.0 * std::max(Away, 0.0)'
add "the lift's lead capped" \
	'const double Lead = GoalRate * 2.0 / std::max(Set.LiftRise, 1e-3);' \
	'const double Lead = std::min(GoalRate * 2.0 / std::max(Set.LiftRise, 1e-3), 1.0);'
add "a hard lift stops the eye dead" \
	'SwingLift = std::max({ SwingLift, Carried, 0.0 });' \
	'SwingLift = std::max({ SwingLift, 0.0 * Carried, 0.0 });'
add "a hard lift carries a step" \
	'GoalRate, GoalRateWas, FeelNow' \
	'GoalRate, GoalRate + 0.0 * GoalRateWas, FeelNow'
add "the point brake for a swing across its way" \
	'Toward > 0.9 * Swing.Length() &&' \
	'Toward > 0.0 * Swing.Length() &&'
add "an anchor not finite stands still (fed)" \
	'AnchorLast + (bFed ? In.AnchorVel : RefVel) * Dt;' \
	'AnchorLast + (bFed ? In.AnchorVel * 0.0 : RefVel) * Dt;'
add "an anchor not finite stands still (measured)" \
	'AnchorLast + (bFed ? In.AnchorVel : RefVel) * Dt;' \
	'AnchorLast + (bFed ? In.AnchorVel : RefVel * 0.0) * Dt;'
add "Begin keeps the pivot's chord" \
	'bPivotEndGood = bChordGood = false;
		bBoxKnown' \
	'bBoxKnown'
add "Begin keeps the frame's id and movement" \
	'bFrameIdKnown = bFrameMoving = false;
		FrameMoving = Vec{};' \
	'(void)0;'
add "a measured carry never too far" \
	'else if (TooFar(SwingRel))' \
	'else if (false && TooFar(SwingRel))'
add "the ceiling's stop leaves the floor" \
	'SwingRel.Z = std::min(SwingRel.Z, 0.0);
				++HardStopCount;
			}
			FloorHold();' \
	'SwingRel.Z = std::min(SwingRel.Z, 0.0);
				++HardStopCount;
			}'
add "lock input kept on a lock not taken" \
	'bInLockLast = In.bLock && (bAnchorGood || bLocked || !bRiseIn);' \
	'bInLockLast = In.bLock;'
add "steady height ignored when fed" \
	'Set.SteadyZ > 0.0 ? SteadyVel : In.AnchorVel.Z' \
	'Set.SteadyZ > 1e300 ? SteadyVel : In.AnchorVel.Z'
add "the ceiling lets a tilt down through" \
	'if (BoomNow * std::sin(TGoal) > Room)' \
	'if (BoomNow * std::sin(TGoal) > Room + 1e300)'
add "Begin's cut not counted" \
	'if (bCutNow) ++CutCount;' \
	'if (bCutNow && false) ++CutCount;'
add "a lift on no time not counted" \
	'++HardLiftCount;
				Compose(false);' \
	'Compose(false);'
add "the clearance blind to the floor's lift" \
	'E.Z = std::max(E.Z, FloorAt(InWorld, E.X, E.Y) + Set.FloorClear);' \
	'E.Z = std::max(E.Z, -1e300 + 0.0 * FloorAt(InWorld, E.X, E.Y));'
add "the clearance's push lands on its edge" \
	'- Along + 1e-9);' \
	'- Along + 0.0);'

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
