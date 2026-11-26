#include "Battle/FreeEye.h"

#include <algorithm>
#include <cmath>

namespace FreeEye
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Rad = Pi / 180.0;

		/** One step of a critically damped spring of rate W toward Want (X and its swing carried). */
		void Spring(double& X, double& Swing, double Want, double W, double Dt)
		{
			const double Off = X - Want;
			const double Mix = Swing + Off * W;
			const double Decay = std::exp(-W * Dt);
			X = Want + (Off + Mix * Dt) * Decay;
			Swing = (Swing - Mix * W * Dt) * Decay;
		}

		/** A wrap that never loops, however far A is (a turn typed a million degrees away). */
		double WrapNear(double A, double Near)
		{
			return Near + std::remainder(A - Near, 2.0 * Pi);
		}

		double Clean(double V)
		{
			return std::isfinite(V) ? V : 0.0;
		}

		/** A spring held to its fastest and hardest: from a swing already faster, slowed no harder than Brake; never faster
		 * than it can still stop from by where it is wanted, so it comes to rest there and does not run past (into a cap
		 * above all). Speeding up is held to Push, slowing down to Brake (Brake >= Push): a move another move has made
		 * give way slows at its own hardest. Where a hold acts the step is a trapezoid of the swings, so the place measured
		 * frame to frame changes its speed by no more than the push, on any frames; where none acts it is the spring's
		 * own exact step, which never runs past where it is wanted even over a long frame. */
		void Held(double& X, double& Swing, double Want, double W, double Fastest, double Push, double Brake, double Dt)
		{
			double X1 = X, Sw = Swing;
			Spring(X1, Sw, Want, W, Dt);
			const double Free = Sw;
			const double StopFrom = std::sqrt(2.0 * Brake * std::abs(Want - X));
			const double Cap = std::max(std::min(Fastest, StopFrom), std::abs(Swing) - Brake * Dt);
			const double Up = Swing >= 0.0 ? Push : Brake, Down = Swing >= 0.0 ? Brake : Push;
			Sw = std::clamp(Sw, Swing - Down * Dt, Swing + Up * Dt);
			Sw = std::clamp(Sw, -Cap, Cap);
			X = Sw == Free ? X1 : X + 0.5 * (Swing + Sw) * Dt;
			Swing = Sw;
			// (at rest to the last bit: a spring's tail would otherwise creep in the last digits for a minute)
			if (std::abs(Want - X) < 1e-11 && std::abs(Swing) < 1e-10)
			{
				X = Want;
				Swing = 0.0;
			}
		}

		/** The same for a point: the push is held on the swing as a whole (its change in any direction), not each axis
		 * on its own, so a slant move is no harder than a straight one. */
		void HeldVec(Vec& X, Vec& Swing, const Vec& Want, double W, double Fastest, double Push, double Dt, double Rest)
		{
			Vec X1 = X, Sw = Swing;
			Spring(X1.X, Sw.X, Want.X, W, Dt);
			Spring(X1.Y, Sw.Y, Want.Y, W, Dt);
			Spring(X1.Z, Sw.Z, Want.Z, W, Dt);
			const double StopFrom = std::sqrt(2.0 * Push * (Want - X).Length());
			const double Cap = std::max(std::min(Fastest, StopFrom), Swing.Length() - Push * Dt);
			bool bHeld = false;
			const double Fast = Sw.Length();
			if (Fast > Cap)
			{
				Sw = Sw * (Cap / Fast);
				bHeld = true;
			}
			const Vec Change = Sw - Swing;
			const double Kick = Change.Length();
			if (Kick > Push * Dt)
			{
				Sw = Swing + Change * (Push * Dt / Kick);
				bHeld = true;
			}
			X = bHeld ? X + (Swing + Sw) * (0.5 * Dt) : X1;
			Swing = Sw;
			if ((Want - X).Length() < Rest && Swing.Length() < Rest)
			{
				X = Want;
				Swing = Vec{};
			}
		}

		/** Turned clockwise seen from above by A radians (as a heading turns). */
		Vec TurnedBy(const Vec& V, double A)
		{
			const double C = std::cos(A), Sn = std::sin(A);
			return Vec{ V.X * C + V.Y * Sn, -V.X * Sn + V.Y * C, V.Z };
		}
	}

	double Eye::ArmOf(double AtZoom) const
	{
		const double Near = std::max(Set.ArmNear, 1e-3);
		const double Far = std::max(Set.ArmFar, Near);
		return Near * std::pow(Far / Near, std::clamp(AtZoom, 0.0, 1.0));
	}

	double Eye::ZoomOf(double Length) const
	{
		const double Near = std::max(Set.ArmNear, 1e-3);
		const double Far = std::max(Set.ArmFar, Near);
		if (!(Far > Near) || !(Length > Near)) return 0.0;
		return std::clamp(std::log(Length / Near) / std::log(Far / Near), 0.0, 1.0);
	}

	Vec Eye::BoomOf(double AtHeading, double AtTilt, double Length) const
	{
		const double Level = Length * std::cos(AtTilt);
		return Vec{ -std::sin(AtHeading) * Level, -std::cos(AtHeading) * Level, Length * std::sin(AtTilt) };
	}

	double Eye::FloorAt(const World* InWorld, double X, double Y) const
	{
		if (InWorld)
		{
			const double F = InWorld->Floor(X, Y);
			if (std::isfinite(F)) return F;
		}
		return Set.FloorFlat;
	}

	void Eye::Begin(const Vec& EyeAt, const Vec& LookAt, const Vec& Frame)
	{
		const Vec From = EyeAt.Finite() ? EyeAt : Vec{};
		Vec Way = LookAt.Finite() ? LookAt - From : Vec{};
		const double Dist = Way.Length();
		if (!(Dist > 1e-9)) Way = BoomOf(H, T, 1.0) * -1.0;
		const Vec Dir = Way.Unit();
		H = WrapNear(std::atan2(Dir.X, Dir.Y), 0.0);
		T = std::clamp(std::asin(std::clamp(-Dir.Z, -1.0, 1.0)), Set.TiltLeast * Rad, Set.TiltMost * Rad);
		const double Length = std::clamp(Dist > 1e-9 ? Dist : Set.ArmNear, Set.ArmNear, std::max(Set.ArmFar, Set.ArmNear));
		S = ZoomOf(Length);
		WantH = H;
		WantT = T;
		WantS = S;
		SwingH = SwingT = SwingS = 0.0;
		// (the pivot on the line of sight, at the boom's length, so the first frame's eye is where his camera stood)
		const Vec Pivot = From - BoomOf(H, T, ArmOf(S));
		RefAt = Frame.Finite() ? Frame : Vec{};
		RefVel = Vec{};
		RelV = Pivot - RefAt;
		WantRelV = RelV;
		SwingRel = Vec{};
		LiftZ = SwingLift = 0.0;
		LiftGoalLast = 0.0;
		bLiftGoalKnown = false;
		bLocked = false;
		LockId = -1;
		bMeasureNext = bCoast = false;
		FrameLast = RefAt;
		bFrameIdKnown = false;
		bInLockLast = false;
		InIdLast = -1;
		bAnchorHeadingGood = false;
		Compose(false);
	}

	void Eye::Want(double InHeading, double InTilt, double InZoom)
	{
		if (std::isfinite(InHeading)) WantH = WrapNear(InHeading, H);
		if (std::isfinite(InTilt)) WantT = std::clamp(InTilt, Set.TiltLeast * Rad, Set.TiltMost * Rad);
		if (std::isfinite(InZoom)) WantS = std::clamp(InZoom, 0.0, 1.0);
	}

	void Eye::Compose(bool bCutNow)
	{
		const double Length = ArmOf(S);
		const Vec Boom = BoomOf(H, T, Length);
		Now.Eye = Pivot() + Boom;
		Now.Look = Now.Eye - Boom * (std::max(Length, Set.LookLeast) / Length);
		Now.Fov = Set.FovNear + (Set.FovFar - Set.FovNear) * std::clamp(S, 0.0, 1.0);
		Now.bCut = bCutNow;
	}

	Pose Eye::Update(const Input& In, double RealDt, const World* InWorld)
	{
		const double Dt = std::isfinite(RealDt) ? std::clamp(RealDt, 0.0, 0.25) : 0.0;
		if (!(Dt > 0.0))
		{
			// (no time, no change: not even a lock taken, which waits for a frame with time in it)
			Pose Same = Now;
			Same.bCut = false;
			return Same;
		}

		// The frame: kept from its last good place; its speed measured while its id holds.
		const Vec FrameAt = In.Frame.Finite() ? In.Frame : FrameLast;
		const bool bFrameNew = bFrameIdKnown && In.FrameId != FrameIdLast;
		const Vec FrameVel = bFrameNew ? Vec{} : (FrameAt - FrameLast) * (1.0 / Dt);

		// What was: the pivot without the lift (the lift is the world's, kept across any change of reference) and its
		// speed in the world: riding the frame, the frame's speed as measured this frame (on the first frame after Begin
		// it is known only now), else the anchor's as last known (a skip's old anchor is no longer fed).
		const Vec WasPivot = RefAt + RelV;
		const Vec WasVel = (!bLocked && !bFrameNew ? FrameVel : RefVel) + SwingRel;

		// The lock: it takes on a rise of bLock or a new id while it is held; it goes on bLock falling, on bLetGo, and,
		// where a pan does not keep it, on his pan.
		const bool bAnchorGood = In.Anchor.Finite();
		const bool bRiseIn = In.bLock && (!bInLockLast || In.AnchorId != InIdLast);
		const bool bHand = Clean(In.PanRight) != 0.0 || Clean(In.PanAhead) != 0.0 || Clean(In.Rise) != 0.0 ||
			Clean(In.SlideRight) != 0.0 || Clean(In.SlideAhead) != 0.0;
		bool bLockNow = false, bLetGoNow = false;
		if (bRiseIn && !In.bLetGo && bAnchorGood)
		{
			bLockNow = true;
		}
		else if (bLocked && (!In.bLock || In.bLetGo || In.AnchorId != LockId))
		{
			bLetGoNow = true;
		}
		else if (bLocked && bHand && !Set.bPanKeepsLock)
		{
			bLetGoNow = true;
		}
		bInLockLast = In.bLock && (bAnchorGood || bLocked || !bRiseIn);
		InIdLast = In.AnchorId;
		if (bLockNow)
		{
			bLocked = true;
			LockId = In.AnchorId;
		}
		if (bLetGoNow) bLocked = false;

		// The new reference and its speed (known when fed or measured; not on a new thing's first frame).
		Vec NewRef, NewVel;
		bool bVelKnown = true, bTeleport = false;
		Vec AnchorNow = AnchorLast;
		if (bLocked)
		{
			AnchorNow = bAnchorGood ? In.Anchor : AnchorLast;
			const bool bFed = In.bAnchorVel && In.AnchorVel.Finite();
			if (!bLockNow)
			{
				const double Speed = (bFed ? In.AnchorVel : AnchorVelLast).Length();
				bTeleport = (AnchorNow - AnchorLast).Length() > Set.TeleportBeyond + Speed * Dt * 3.0;
			}
			const bool bFresh = bLockNow || bTeleport;
			const double SteadyWas = SteadyRefZ;
			if (Set.SteadyZ > 0.0 && !bFresh)
			{
				SteadyRefZ += (AnchorNow.Z - SteadyRefZ) * (1.0 - std::exp(-Dt / Set.SteadyZ));
			}
			else
			{
				SteadyRefZ = AnchorNow.Z;
			}
			NewRef = Vec{ AnchorNow.X, AnchorNow.Y, SteadyRefZ };
			const double SteadyVel = bFresh ? 0.0 : (SteadyRefZ - SteadyWas) / Dt;
			if (bFed)
			{
				NewVel = Vec{ In.AnchorVel.X, In.AnchorVel.Y, Set.SteadyZ > 0.0 ? SteadyVel : In.AnchorVel.Z };
			}
			else if (!bFresh)
			{
				NewVel = (NewRef - RefAt) * (1.0 / Dt);
			}
			else
			{
				bVelKnown = false;
			}
			AnchorVelLast = bFed ? In.AnchorVel : bFresh ? Vec{} : (AnchorNow - AnchorLast) * (1.0 / Dt);
			AnchorLast = AnchorNow;
		}
		else
		{
			NewRef = FrameAt;
			NewVel = FrameVel;
			bVelKnown = !bFrameNew;
		}
		FrameLast = FrameAt;
		FrameIdLast = In.FrameId;
		bFrameIdKnown = true;

		// A change of reference: carried (the pivot's place and its speed in the world kept, then a glide), or cut when
		// its rest is too far to glide to.
		bool bCutNow = false;
		const bool bChange = bLockNow || bLetGoNow || (!bLocked && bFrameNew);
		if (bChange)
		{
			bMeasureNext = !bVelKnown;
			bCoast = !bLocked;
			if (bLockNow && (WasPivot - NewRef).Length() > Set.CutBeyond)
			{
				bCutNow = true;
				RelV = WantRelV = SwingRel = Vec{};
				bMeasureNext = false;
			}
			else
			{
				// (where the new reference stood a frame ago, so the pivot goes on from where it was at its own speed)
				RelV = WasPivot - (NewRef - NewVel * Dt);
				SwingRel = WasVel - NewVel;
				// (locked: he centres on it; let go: the view comes to rest in the frame where its own speed takes it)
				WantRelV = bLocked ? Vec{} : RelV + SwingRel * (1.0 / std::max(Set.Ease, 1e-3));
			}
		}
		else if (bTeleport)
		{
			bCutNow = true;
			SwingRel = Vec{};
			bMeasureNext = false;
		}
		else if (bMeasureNext)
		{
			// (the new reference's speed, measured over its first frame: the pivot was carried as if it stood still, which
			// put it right on that frame; from this one its swing is taken against the speed now known)
			SwingRel -= NewVel;
			if (bCoast) WantRelV = RelV + SwingRel * (1.0 / std::max(Set.Ease, 1e-3));
			bMeasureNext = false;
		}
		if (bCutNow)
		{
			SwingH = SwingT = SwingS = SwingLift = 0.0;
			++CutCount;
		}
		RefAt = NewRef;
		RefVel = NewVel;

		// His hand: pan and rise by the boom, a slide by what the picture shows, the wheel, the turn and the tilt.
		const double ArmNow = ArmOf(S);
		const double Reach = std::max(ArmNow, Set.PanArmLeast);
		const Vec Right{ std::cos(H), -std::sin(H), 0.0 };
		const Vec Ahead{ std::sin(H), std::cos(H), 0.0 };
		const double PanRight = std::clamp(Clean(In.PanRight), -1.0, 1.0);
		const double PanAhead = std::clamp(Clean(In.PanAhead), -1.0, 1.0);
		const double Rise = std::clamp(Clean(In.Rise), -1.0, 1.0);
		WantRelV += Right * (PanRight * Set.PanRate * Reach * Dt) + Ahead * (PanAhead * Set.PanRate * Reach * Dt);
		WantRelV.Z += Rise * Set.RiseRate * Reach * Dt;
		const double Aspect = std::isfinite(In.Aspect) && In.Aspect > 0.1 ? std::min(In.Aspect, 10.0) : 1.6;
		const double Tall = 2.0 * ArmNow * std::tan(0.5 * Now.Fov * Rad);
		WantRelV += Right * (std::clamp(Clean(In.SlideRight), -4.0, 4.0) * Tall * Aspect) +
			Ahead * (std::clamp(Clean(In.SlideAhead), -4.0, 4.0) * Tall);
		WantS -= std::clamp(Clean(In.Zoom), -100.0, 100.0) * Set.ZoomStep;
		WantH += std::clamp(Clean(In.Turn), -3600.0, 3600.0) * Rad;
		WantT += std::clamp(Clean(In.Tilt), -3600.0, 3600.0) * Rad;

		// Ride: the anchor's turn this frame turns the view with it (the bow stays where it was in the picture), and
		// where he wants to sit in her frame turns with her too. His offset itself follows that on its spring: turned at
		// once, an offset of some hundreds of metres (mid-glide onto her) would be flung sideways.
		const bool bHeadingGood = std::isfinite(In.AnchorHeading);
		if (bLocked && In.bRide && !bLockNow && !bTeleport && bHeadingGood && bAnchorHeadingGood)
		{
			const double Turned = WrapNear(In.AnchorHeading - AnchorHeadingLast, 0.0);
			H += Turned;
			WantH += Turned;
			WantRelV = TurnedBy(WantRelV, Turned);
		}
		if (bHeadingGood) AnchorHeadingLast = In.AnchorHeading;
		bAnchorHeadingGood = bLocked && (bHeadingGood || bAnchorHeadingGood);

		// The caps on what he wants, so the springs come to rest AT a cap, softly.
		H = WrapNear(H, 0.0);
		WantH = WrapNear(WantH, H);
		WantT = std::clamp(WantT, Set.TiltLeast * Rad, Set.TiltMost * Rad);
		WantS = std::clamp(WantS, 0.0, 1.0);
		for (int Axis = 0; Axis < 3; ++Axis)
		{
			// (so far no hand reaches, an hour of a key held included; it keeps every sum finite)
			double& C = Axis == 0 ? WantRelV.X : Axis == 1 ? WantRelV.Y : WantRelV.Z;
			C = std::clamp(C, -1e8, 1e8);
		}
		const Vec Up{ 0.0, 0.0, LiftZ };
		const bool bBox = In.bBox && In.BoxMin.Finite() && In.BoxMax.Finite();
		const Vec BoxLo{ std::min(In.BoxMin.X, In.BoxMax.X), std::min(In.BoxMin.Y, In.BoxMax.Y), std::min(In.BoxMin.Z, In.BoxMax.Z) };
		const Vec BoxHi{ std::max(In.BoxMin.X, In.BoxMax.X), std::max(In.BoxMin.Y, In.BoxMax.Y), std::max(In.BoxMin.Z, In.BoxMax.Z) };
		auto InBox = [&BoxLo, &BoxHi](const Vec& P)
		{
			return Vec{ std::clamp(P.X, BoxLo.X, BoxHi.X), std::clamp(P.Y, BoxLo.Y, BoxHi.Y), std::clamp(P.Z, BoxLo.Z, BoxHi.Z) };
		};
		if (bBox) WantRelV = InBox(RefAt + WantRelV + Up) - RefAt - Up;
		{
			// (the eye under the sky's ceiling: a steep far view is held short of it first, then his offset is held down)
			const double PivotZ = RefAt.Z + WantRelV.Z + LiftZ;
			const double Sn = std::sin(WantT);
			if (Sn > 0.0 && PivotZ + ArmOf(WantS) * Sn > Set.EyeMost)
			{
				WantS = std::min(WantS, ZoomOf((Set.EyeMost - PivotZ) / Sn));
			}
			const double Over = PivotZ + ArmOf(WantS) * Sn - Set.EyeMost;
			if (Over > 0.0) WantRelV.Z -= Over;
		}

		// The eye kept off the aircraft or the ship (AnchorClear): the boom is made long enough that the eye's way from
		// where it is to where it comes to rest keeps clear of the anchor, a glide onto a new anchor included (a glide
		// that skimmed the anchor would have to be pushed off it every frame, a jolt). Only the boom's goal is raised, not
		// his own zoom, so the boom comes back in as the pivot arrives. The shortest such boom is found by halving.
		double SGoal = WantS;
		const double Clear = bLocked && std::isfinite(In.AnchorClear) ? std::max(In.AnchorClear, 0.0) : 0.0;
		if (Clear > 0.0)
		{
			const Vec From = RefAt + RelV + Up, To = RefAt + WantRelV + Up;
			auto Keeps = [&](double AtZoom)
			{
				const double Length = ArmOf(AtZoom);
				const Vec A = From + BoomOf(H, T, Length) - AnchorNow, B = To + BoomOf(WantH, WantT, Length) - AnchorNow;
				const Vec Way = B - A;
				const double Long = Way.X * Way.X + Way.Y * Way.Y + Way.Z * Way.Z;
				const double U = Long > 0.0 ? std::clamp(-(A.X * Way.X + A.Y * Way.Y + A.Z * Way.Z) / Long, 0.0, 1.0) : 0.0;
				return (A + Way * U).Length() >= Clear * 1.05;
			};
			if (!Keeps(SGoal))
			{
				double Lo = SGoal, Hi = 1.0;
				for (int K = 0; K < 24; ++K)
				{
					const double Mid = 0.5 * (Lo + Hi);
					(Keeps(Mid) ? Hi : Lo) = Mid;
				}
				SGoal = Hi;
			}
		}

		// The springs. His offset (and the glide onto a new reference) held as a point; the turn, the tilt and the zoom
		// share one budget, each slowed in proportion to what the others ask, so all three together swing the eye no
		// harder than one alone (their pulls on the eye add up: the boom turning while it grows is pushed sideways too).
		const double Feel = std::max(ArmNow, Set.FeelArm);
		HeldVec(RelV, SwingRel, WantRelV, Set.Ease, Set.GlideFastest * Feel, Set.GlidePush * Feel, Dt, 1e-10 * Feel);
		const double FastH = std::max(Set.TurnFastest * Rad, 1e-6), FastT = std::max(Set.TiltFastest * Rad, 1e-6);
		const double FastS = std::max(Set.ZoomFastest, 1e-6);
		auto Asks = [](double X, double Want, double Swing, double W, double Fastest)
		{
			return std::min(1.0, std::max(std::abs(Swing), 0.5 * W * std::abs(Want - X)) / Fastest);
		};
		const double Share = 1.0 / std::max(1.0, Asks(H, WantH, SwingH, Set.TurnEase, FastH) + Asks(T, WantT, SwingT, Set.TiltEase, FastT) +
			Asks(S, SGoal, SwingS, Set.ZoomEase, FastS));
		Held(H, SwingH, WantH, Set.TurnEase, FastH * Share, Set.TurnPush * Rad * Share, Set.TurnPush * Rad, Dt);
		Held(T, SwingT, WantT, Set.TiltEase, FastT * Share, Set.TiltPush * Rad * Share, Set.TiltPush * Rad, Dt);
		Held(S, SwingS, SGoal, Set.ZoomEase, FastS * Share, Set.ZoomPush * Share, Set.ZoomPush, Dt);
		H = WrapNear(H, 0.0);
		WantH = WrapNear(WantH, H);
		T = std::clamp(T, Set.TiltLeast * Rad, Set.TiltMost * Rad);
		S = std::clamp(S, 0.0, 1.0);

		// The floor's lift: the whole view carried up where the boom would put the eye under FloorClear over the floor
		// round it or ahead of it, eased (fast up, slow down) and held to its push.
		const double Length = ArmOf(S);
		const double FeelNow = std::max(Length, Set.FeelArm);
		const Vec Boom = BoomOf(H, T, Length);
		const Vec Eye0 = RefAt + RelV + Boom;
		double Highest = FloorAt(InWorld, Eye0.X, Eye0.Y);
		for (int K = 0; K < 8; ++K)
		{
			const double A = K * (Pi / 4.0);
			Highest = std::max(Highest, FloorAt(InWorld, Eye0.X + Set.FloorReach * std::sin(A), Eye0.Y + Set.FloorReach * std::cos(A)));
		}
		const Vec Going{ RefVel.X + SwingRel.X, RefVel.Y + SwingRel.Y, 0.0 };
		for (int K = 1; K <= 2; ++K)
		{
			const Vec At = Eye0 + Going * (Set.FloorAhead * 0.5 * K);
			Highest = std::max(Highest, FloorAt(InWorld, At.X, At.Y));
		}
		const double LiftGoal = std::max(0.0, Highest + Set.FloorClear - Eye0.Z);
		// (a goal climbing, a crest coming up under him or his own descent, is led by the spring's own lag at that rate,
		// so the eye keeps its clearance instead of trailing it by a metre; led no more than the clearance itself)
		const double GoalRate = bLiftGoalKnown && !bCutNow ? (LiftGoal - LiftGoalLast) / Dt : 0.0;
		const double Lead = std::min(std::max(GoalRate, 0.0) * 2.0 / std::max(Set.LiftRise, 1e-3), Set.FloorClear);
		LiftGoalLast = LiftGoal;
		bLiftGoalKnown = true;
		if (bCutNow)
		{
			LiftZ = LiftGoal;
			SwingLift = 0.0;
		}
		else
		{
			const double Goal = LiftGoal > 0.0 ? LiftGoal + Lead : 0.0;
			Held(LiftZ, SwingLift, Goal, Goal > LiftZ ? Set.LiftRise : Set.LiftSettle, 1e9, Set.LiftPush * FeelNow, Set.LiftPush * FeelNow, Dt);
		}
		if (LiftZ < 0.0)
		{
			LiftZ = 0.0;
			SwingLift = std::max(SwingLift, 0.0);
		}

		// The hard caps, last: nothing here moves the view unless a cap moved or two eased moves met one together. Where
		// two meet, the higher wins: the floor (the eye is never under the ground or a wave), then the ceiling, then the
		// anchor's clearance, then the pivot's box. The clearance pushes the eye out away from her and never down (under
		// a ship a push down would put the eye in the sea), the floor taken again wherever that push took it.
		auto EyeNow = [this, &Boom]() { return RefAt + RelV + Vec{ 0.0, 0.0, LiftZ } + Boom; };
		if (bBox)
		{
			const Vec Pv = RefAt + RelV + Vec{ 0.0, 0.0, LiftZ };
			const Vec Kept = InBox(Pv);
			if ((Kept - Pv).Length() > 0.0)
			{
				RelV += Kept - Pv;
				++HardStopCount;
			}
		}
		bool bLiftedHard = false;
		auto FloorHold = [&]()
		{
			const Vec E = EyeNow();
			const double Under = FloorAt(InWorld, E.X, E.Y) + Set.FloorHard - E.Z;
			if (Under > 0.0)
			{
				LiftZ += Under;
				SwingLift = std::max(SwingLift, 0.0);
				bLiftedHard = true;
			}
		};
		FloorHold();
		for (int Pass = 0; Pass < 8 && Clear > 0.0; ++Pass)
		{
			const Vec Gap = EyeNow() - AnchorNow;
			const double Near = Gap.Length();
			if (Near >= Clear) break;
			Vec Way = Near > 1e-9 ? Gap * (1.0 / Near) : Boom.Unit();
			if (Way.Z < 0.0 || Pass > 0)
			{
				// (a second pass goes level: the floor lifted it back toward her)
				Way.Z = Pass > 0 ? 0.0 : std::max(Way.Z, 0.0);
				Way = Way.Length() > 1e-9 ? Way.Unit() : Vec{ 1.0, 0.0, 0.0 };
			}
			// (how far along that way the eye leaves her clearance)
			const double Along = Gap.X * Way.X + Gap.Y * Way.Y + Gap.Z * Way.Z;
			RelV += Way * (std::sqrt(std::max(0.0, Along * Along - (Near * Near - Clear * Clear))) - Along + 1e-9);
			++HardStopCount;
			FloorHold();
		}
		const double Above = EyeNow().Z - Set.EyeMost;
		if (Above > 0.0)
		{
			RelV.Z -= Above;
			SwingRel.Z = std::min(SwingRel.Z, 0.0);
			++HardStopCount;
			FloorHold();
		}
		if (bLiftedHard) ++HardLiftCount;

		Compose(bCutNow);
		return Now;
	}
}
