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
		void Held(double& X, double& Swing, double Want, double W, double Fastest, double Push, double Brake, double Dt, double WantRate = 0.0)
		{
			// (a want that itself moves at WantRate, the floor's lift under a climbing floor, is closed on and stopped short
			// of by the speed relative to it: X may go as much faster than the stop allows as the want draws away)
			// (coming in near as fast as the brake can still stop it, or faster than the spring's own step takes without
			// running past, it is braked evenly onto its want instead, no harder than the brake, and set on it on the
			// frame it gets there: a stiff spring held to its fastest otherwise brakes too late and runs past)
			const double Left = Want - X, Dist = std::abs(Left);
			const double Toward = Left > 0.0 ? Swing : Left < 0.0 ? -Swing : 0.0;
			const double Away = Left > 0.0 ? WantRate : Left < 0.0 ? -WantRate : 0.0;
			const double Closing = Toward - Away;
			if (Dist > 1e-11 && Toward > 0.0 && Closing > 0.0 && (Closing > W * Dist || Closing >= 0.9 * std::sqrt(2.0 * Brake * Dist)))
			{
				const double Slow = std::min(Toward * Toward / (2.0 * Dist), Brake);
				const double Sw = Swing * (std::max(0.0, Toward - Slow * Dt) / Toward);
				const double Step = 0.5 * (Swing + Sw) * Dt;
				if (std::abs(Step) >= Dist && (Slow < Brake || std::abs(Sw) <= Brake * Dt))
				{
					X = Want;
					Swing = 0.0;
				}
				else
				{
					X += Step;
					Swing = Sw;
				}
				return;
			}
			double X1 = X, Sw = Swing;
			Spring(X1, Sw, Want, W, Dt);
			const double Free = Sw;
			const double StopFrom = std::sqrt(2.0 * Brake * std::abs(Want - X));
			const double Cap = std::max(std::min(Fastest, StopFrom + std::max(Away, 0.0)), std::abs(Swing) - Brake * Dt);
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
		 * on its own, so a slant move is no harder than a straight one. A point coming in as fast as its push can still
		 * stop it, or faster than the spring's own step can take without running past (W times the way left), is braked
		 * evenly onto its want instead (no harder than the push), and set on it on the frame it gets there: held to its
		 * fastest and its push, the spring's own step would run past at the end of a long glide or a fast let go. */
		void HeldVec(Vec& X, Vec& Swing, const Vec& Want, double W, double Fastest, double Push, double Dt, double Rest)
		{
			const Vec Left = Want - X;
			const double Dist = Left.Length();
			const double Toward = Dist > 0.0 ? (Swing.X * Left.X + Swing.Y * Left.Y + Swing.Z * Left.Z) / Dist : 0.0;
			// (only a swing that points at the want, near enough: one mostly across the way would be braked by its part
			// toward it alone and circle the want for a minute in the last digits)
			if (Dist > Rest && Toward > 0.9 * Swing.Length() && (Toward > W * Dist || Toward >= std::sqrt(2.0 * Push * Dist) - Push * Dt))
			{
				// (the brake that stops it on the want, no harder than the push)
				const double Fast = Swing.Length();
				const double Brake = std::min(Toward * Toward / (2.0 * Dist), Push);
				const Vec Sw = Swing * (std::max(0.0, Fast - Brake * Dt) / Fast);
				const Vec Step = (Swing + Sw) * (0.5 * Dt);
				// (set on it where this step reaches it: braked under the push, or at the push with no more than one
				// step of it left, which stops there no harder than the push)
				if ((Step.X * Left.X + Step.Y * Left.Y + Step.Z * Left.Z) / Dist >= Dist && (Brake < Push || Sw.Length() <= Push * Dt))
				{
					X = Want;
					Swing = Vec{};
				}
				else
				{
					X += Step;
					Swing = Sw;
				}
				return;
			}
			Vec X1 = X, Sw = Swing;
			Spring(X1.X, Sw.X, Want.X, W, Dt);
			Spring(X1.Y, Sw.Y, Want.Y, W, Dt);
			Spring(X1.Z, Sw.Z, Want.Z, W, Dt);
			// (no faster than it can still stop from where this step ends: v^2 = 2 Push (Dist - (v0 + v) Dt / 2))
			const double PushDt = Push * Dt;
			const double StopFrom = 0.5 * (std::sqrt(std::max(0.0, PushDt * PushDt + 8.0 * Push * Dist - 4.0 * PushDt * Swing.Length())) - PushDt);
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
		// (the tilt as shot, even past its range: only what he wants is held to the range, and the tilt eases into it
		// from the shot, so a shot straight down or straight up is not turned a degree or ten on the first frame)
		T = std::asin(std::clamp(-Dir.Z, -1.0, 1.0));
		const double Length = std::clamp(Dist > 1e-9 ? Dist : Set.ArmNear, Set.ArmNear, std::max(Set.ArmFar, Set.ArmNear));
		S = ZoomOf(Length);
		WantH = H;
		WantT = std::clamp(T, Set.TiltLeast * Rad, Set.TiltMost * Rad);
		WantS = S;
		SwingH = SwingT = SwingS = 0.0;
		// (the pivot on the line of sight, at the boom's length, so the first frame's eye is where his camera stood; a
		// shot over the ceiling is let down under it, the whole view together, and the first frame says it is a cut)
		Vec Pivot = From - BoomOf(H, T, ArmOf(S));
		const double Above = From.Z - Set.EyeMost;
		bBeginCut = Above > 0.0;
		if (bBeginCut) Pivot.Z -= Above;
		RefAt = Frame.Finite() ? Frame : Vec{};
		RefVel = Vec{};
		RelV = Pivot - RefAt;
		WantRelV = RelV;
		SwingRel = Vec{};
		LiftZ = SwingLift = 0.0;
		LiftGoalLast = LiftRateLast = 0.0;
		bLiftGoalKnown = false;
		bLocked = false;
		LockId = -1;
		AnchorMoving = Vec{};
		bAnchorMoving = false;
		bMeasureNext = bCoast = false;
		FrameLast = RefAt;
		bFrameIdKnown = bFrameMoving = false;
		FrameMoving = Vec{};
		bInLockLast = false;
		InIdLast = -1;
		bAnchorHeadingGood = false;
		bPivotEndGood = bChordGood = false;
		bBoxKnown = bBoxWas = false;
		bFirst = true;
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
			// (no time, no change: not even a lock taken, which waits for a frame with time in it; only the floor's hard
			// rule holds even here, should the scene have moved a wave under the eye with no time passing)
			const double Under = FloorAt(InWorld, Now.Eye.X, Now.Eye.Y) + Set.FloorHard - Now.Eye.Z;
			if (Under > 0.0)
			{
				LiftZ += Under;
				SwingLift = std::max(SwingLift, 0.0);
				++HardLiftCount;
				Compose(false);
			}
			Pose Same = Now;
			Same.bCut = false;
			return Same;
		}
		const int StopsWas = HardStopCount;

		// Rule 6's test of a thing that may have jumped: its step this frame against the speed it is known to have. Where
		// that speed is only measured (a frame, an anchor whose velocity is not fed), the battle's pace may have changed
		// since (paused, or 0.5, to 10 on a hitch): a step along its last measured way may be up to PaceJump times as
		// long, a step across that way or back may not.
		auto Jumped = [this, Dt](const Vec& Step, const Vec& Known, bool bMeasured)
		{
			const double Slack = Set.TeleportBeyond + Known.Length() * Dt * 3.0;
			if (!bMeasured || !(Known.Length() > 0.0)) return Step.Length() > Slack;
			const Vec Way = Known.Unit();
			const double Along = Step.X * Way.X + Step.Y * Way.Y + Step.Z * Way.Z;
			const double Across = (Step - Way * Along).Length();
			return Across > Slack || Along < -Slack || Along > Set.TeleportBeyond + Known.Length() * Dt * std::max(3.0, Set.PaceJump);
		};

		// The frame: kept from its last good place; its speed measured while its id holds. A step that jumped (above,
		// against its last movement, kept through a pause) is a new frame as well, not a speed: a raid's middle jumps
		// when a group joins it or leaves it, and that is no speed to carry. (Its first step after Begin is judged
		// against Begin's frame, with no speed yet to allow for.)
		const Vec FrameAt = In.Frame.Finite() ? In.Frame : FrameLast;
		const Vec FrameStep = FrameAt - FrameLast;
		const bool bFrameJump = Jumped(FrameStep, bFrameIdKnown && bFrameMoving ? FrameMoving : Vec{}, true);
		const bool bFrameNew = bFrameJump || (bFrameIdKnown && In.FrameId != FrameIdLast);
		const Vec FrameVel = bFrameNew ? Vec{} : FrameStep * (1.0 / Dt);

		// What was: the pivot without the lift (the lift is the world's, kept across any change of reference) and its
		// speed in the world. That speed is the pivot's own over the last frame where it is known (the reference it rode
		// is not fed any more after a skip or a let go, and a velocity fed for the end of the last frame is half a frame
		// late for a turning aircraft); else, riding the frame, the frame's speed as measured this frame, or the
		// anchor's as last known.
		const Vec WasPivot = RefAt + RelV;
		const Vec WasVel = bChordGood ? ChordVel : (!bLocked && !bFrameNew ? FrameVel : RefVel) + SwingRel;

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
			const bool bFed = In.bAnchorVel && In.AnchorVel.Finite();
			// (an anchor not finite this frame is the last good one carried on at its velocity, fed or as last measured, so
			// the reference does not stand still while the thing flies on and then jump to catch it up)
			AnchorNow = bAnchorGood ? In.Anchor : AnchorLast + (bFed ? In.AnchorVel : RefVel) * Dt;
			if (!bLockNow)
			{
				// (against its own velocity where it is fed; on the frame its speed is first measured, against the speed the
				// pivot had, the view it was taken from; else against its last measured movement)
				const Vec Known = bFed ? In.AnchorVel : bMeasureNext ? WasVel : bAnchorMoving ? AnchorMoving : Vec{};
				bTeleport = Jumped(AnchorNow - AnchorLast, Known, !bFed && !bMeasureNext);
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
			// (its last movement, kept through a pause and a teleport, forgotten on a new lock)
			if (bLockNow) bAnchorMoving = false;
			const Vec Moved = bFed ? In.AnchorVel : bFresh ? Vec{} : (AnchorNow - AnchorLast) * (1.0 / Dt);
			if (Moved.Length() > 0.0)
			{
				AnchorMoving = Moved;
				bAnchorMoving = true;
			}
			AnchorLast = AnchorNow;
		}
		else
		{
			NewRef = FrameAt;
			NewVel = FrameVel;
			bVelKnown = !bFrameNew;
		}
		if (FrameVel.Length() > 0.0)
		{
			FrameMoving = FrameVel;
			bFrameMoving = true;
		}
		FrameLast = FrameAt;
		FrameIdLast = In.FrameId;
		bFrameIdKnown = true;

		// The glide's push as the boom stands now, and the anchor's clearance.
		const double FeelWas = std::max(ArmOf(S), Set.FeelArm);
		const double GlidePush = std::max(Set.GlidePush * FeelWas, 1e-9);
		const double Clear = bLocked && std::isfinite(In.AnchorClear) ? std::max(In.AnchorClear, 0.0) : 0.0;
		// Where his offset comes to rest from a swing, braked no harder than the glide's push: the spring's own v / Ease
		// on, or v^2 / (2 push) on where that is further (a fast let go: wanted any nearer, the held spring runs past
		// its want and comes back).
		auto StopOf = [this, GlidePush](const Vec& At, const Vec& Swing)
		{
			return At + Swing * std::max(1.0 / std::max(Set.Ease, 1e-3), Swing.Length() / (2.0 * GlidePush));
		};
		// (the nearest a straight way from A to B comes to the origin)
		auto Nearest = [](const Vec& A, const Vec& B)
		{
			const Vec Way = B - A;
			const double Long = Way.X * Way.X + Way.Y * Way.Y + Way.Z * Way.Z;
			const double U = Long > 0.0 ? std::clamp(-(A.X * Way.X + A.Y * Way.Y + A.Z * Way.Z) / Long, 0.0, 1.0) : 0.0;
			return (A + Way * U).Length();
		};
		// A lock is a cut, not a glide, where its glide would take the pivot further than CutBeyond from where it is
		// (the anchor far off, or so much faster or slower that braking the difference at the glide's push falls
		// kilometres behind it), or where the eye's way while that is braked passes inside the anchor's clearance (an
		// aircraft met head on, or one passing through the eye): no boom grows fast enough to keep clear of that.
		auto TooFar = [&](const Vec& Swing)
		{
			const Vec Off = WasPivot - NewRef;
			const Vec Stop = Off + Swing * (Swing.Length() / (2.0 * GlidePush));
			if (std::max(Off.Length(), Stop.Length()) > Set.CutBeyond) return true;
			// (and where the glide, braking the difference and then gliding back at its fastest, would take longer than
			// GlideLongest: a drone's metre-long boom glides at some thirty metres a second)
			const double GlideFast = std::max(Set.GlideFastest * FeelWas, 1e-9);
			if (Swing.Length() / GlidePush + Stop.Length() / GlideFast + GlideFast / GlidePush > Set.GlideLongest) return true;
			if (!(Clear > 0.0)) return false;
			const Vec EyeBy = NewRef + Vec{ 0.0, 0.0, LiftZ } + BoomOf(H, T, ArmOf(S)) - AnchorNow;
			return Nearest(Off + EyeBy, Stop + EyeBy) < Clear;
		};
		auto TakeCut = [&]()
		{
			RelV = WantRelV = SwingRel = Vec{};
			SwingH = SwingT = SwingS = SwingLift = 0.0;
			bMeasureNext = false;
			// (the boom no shorter than the anchor's clearance, so the eye stands clear of her from the cut on)
			if (Clear > 0.0 && ArmOf(S) < Clear * 1.06) S = ZoomOf(Clear * 1.06);
			++CutCount;
		};

		// A change of reference: carried (the pivot's place and its speed in the world kept, then a glide), or cut when
		// it is too far to glide.
		bool bCutNow = bFirst && bBeginCut;
		if (bCutNow) ++CutCount;
		const bool bChange = bLockNow || bLetGoNow || (!bLocked && bFrameNew);
		if (bChange)
		{
			bMeasureNext = !bVelKnown;
			bCoast = !bLocked;
			// (where the new reference stood a frame ago, so the pivot goes on from where it was at its own speed)
			RelV = WasPivot - (NewRef - NewVel * Dt);
			SwingRel = WasVel - NewVel;
			// (a new anchor whose speed is not known yet is judged by its place now, and by its speed on the next frame)
			if (bLockNow && TooFar(bVelKnown ? SwingRel : Vec{}))
			{
				bCutNow = true;
				TakeCut();
			}
			else
			{
				// (locked: he centres on it; let go: the view comes to rest in the frame where its own speed takes it)
				WantRelV = bLocked ? Vec{} : StopOf(RelV, SwingRel);
			}
		}
		else if (bTeleport)
		{
			// (the picture jumps with it, but his own turn, tilt and zoom go on as they were, and the ride still turns
			// the view with its heading: it is the same thing, only elsewhere)
			bCutNow = true;
			SwingRel = Vec{};
			SwingLift = 0.0;
			bMeasureNext = false;
			++CutCount;
		}
		else if (bMeasureNext)
		{
			// (the new reference's speed, measured over its first frame: the pivot was carried as if it stood still, which
			// put it right on that frame; from this one its swing is taken against the speed now known)
			SwingRel -= NewVel;
			bMeasureNext = false;
			if (bCoast)
			{
				WantRelV = StopOf(RelV, SwingRel);
			}
			else if (TooFar(SwingRel))
			{
				bCutNow = true;
				TakeCut();
			}
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
		const double WantZWas = WantRelV.Z;
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
		if (bLocked && In.bRide && !bLockNow && bHeadingGood && bAnchorHeadingGood)
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
		{
			// (his own descent stops where the eye he wants stands FloorClear over the floor under it, as the view is lifted
			// now: held lower, the want would sink on under the ground for as long as the key is held, the lift chasing it
			// at the key's speed until the hard floor took over, and coming back up would take as long again)
			const Vec WantEye = RefAt + WantRelV + Up + BoomOf(WantH, WantT, ArmOf(WantS));
			const double Least = FloorAt(InWorld, WantEye.X, WantEye.Y) + Set.FloorClear - (WantEye.Z - WantRelV.Z);
			if (WantRelV.Z < WantZWas && WantRelV.Z < Least) WantRelV.Z = std::min(WantZWas, Least);
		}
		// The pivot's box: a box with a corner not finite is the last good one, moved on at its walls' last speeds; those
		// speeds are measured frame to frame (a box carried with the raid moves with it), a box first given taken as riding
		// with the reference (a still one is known still from the next frame).
		const bool bBoxGood = In.BoxMin.Finite() && In.BoxMax.Finite();
		const bool bBox = In.bBox && (bBoxGood || bBoxKnown);
		const Vec BoxLo = !bBoxGood ? BoxLoLast + BoxLoVel * Dt : Vec{ std::min(In.BoxMin.X, In.BoxMax.X), std::min(In.BoxMin.Y, In.BoxMax.Y), std::min(In.BoxMin.Z, In.BoxMax.Z) };
		const Vec BoxHi = !bBoxGood ? BoxHiLast + BoxHiVel * Dt : Vec{ std::max(In.BoxMin.X, In.BoxMax.X), std::max(In.BoxMin.Y, In.BoxMax.Y), std::max(In.BoxMin.Z, In.BoxMax.Z) };
		const Vec LoVel = bBox && bBoxWas ? (BoxLo - BoxLoLast) * (1.0 / Dt) : RefVel;
		const Vec HiVel = bBox && bBoxWas ? (BoxHi - BoxHiLast) * (1.0 / Dt) : RefVel;
		if (bBox)
		{
			BoxLoLast = BoxLo;
			BoxHiLast = BoxHi;
			BoxLoVel = LoVel;
			BoxHiVel = HiVel;
			bBoxKnown = true;
		}
		bBoxWas = bBox;
		auto InBox = [&BoxLo, &BoxHi](const Vec& P)
		{
			return Vec{ std::clamp(P.X, BoxLo.X, BoxHi.X), std::clamp(P.Y, BoxLo.Y, BoxHi.Y), std::clamp(P.Z, BoxLo.Z, BoxHi.Z) };
		};
		// (each wall held where it will stand against the reference by the time his offset's spring has taken up their
		// difference of speeds, 2 / Ease and this frame on, or as long as his offset's push takes to match it where that
		// is longer: a reference running at a still wall is met by his offset running back, not stopped on the wall; a
		// wall that moves with the reference is held exactly where it stands)
		const Vec LoBy = RefVel - LoVel, HiBy = RefVel - HiVel;
		const double BoxBy = std::max(LoBy.Length(), HiBy.Length());
		if (bBox)
		{
			const double LeadTakes = std::max(2.0 / std::max(Set.Ease, 1e-3), BoxBy / (2.0 * std::max(Set.GlidePush * std::max(ArmNow, Set.FeelArm), 1e-9)));
			const Vec P = RefAt + WantRelV + Up;
			const Vec Lo = BoxLo - LoBy * (LeadTakes + Dt), Hi = BoxHi - HiBy * (LeadTakes + Dt);
			auto Hold = [](double At, double Least, double Most) { return Least <= Most ? std::clamp(At, Least, Most) : 0.5 * (Least + Most); };
			WantRelV = Vec{ Hold(P.X, Lo.X, Hi.X), Hold(P.Y, Lo.Y, Hi.Y), Hold(P.Z, Lo.Z, Hi.Z) } - RefAt - Up;
		}
		{
			// (the eye under the sky's ceiling: a steep far view is held short of it first, then his offset is held down
			// by the highest the boom's end stands on its way from where it is to where it is wanted, so the pivot makes
			// room before a long boom tilted down as it grows lifts the eye through the ceiling, and waits for a long
			// boom to come in before it rises)
			const double PivotZ = RefAt.Z + WantRelV.Z + LiftZ;
			const double Sn = std::sin(WantT);
			if (Sn > 0.0 && PivotZ + ArmOf(WantS) * Sn > Set.EyeMost)
			{
				WantS = std::min(WantS, ZoomOf((Set.EyeMost - PivotZ) / Sn));
			}
			// (the way taken as each going at its fastest: the zoom gets there long before a slow tilt does)
			const double ZoomTakes = std::abs(WantS - S) / std::max(Set.ZoomFastest, 1e-6);
			const double TiltTakes = std::abs(WantT - T) / std::max(Set.TiltFastest * Rad, 1e-6);
			const double Takes = std::max(ZoomTakes, TiltTakes);
			double BoomUp = ArmOf(WantS) * Sn;
			for (int K = 0; K < 8; ++K)
			{
				const double U = Takes > 0.0 ? K / 8.0 * Takes : 0.0;
				const double AtS = ZoomTakes > U ? S + (WantS - S) * (U / ZoomTakes) : WantS;
				const double AtT = TiltTakes > U ? T + (WantT - T) * (U / TiltTakes) : WantT;
				BoomUp = std::max(BoomUp, ArmOf(AtS) * std::sin(AtT));
			}
			const double Over = PivotZ + BoomUp - Set.EyeMost;
			if (Over > 0.0) WantRelV.Z -= Over;
		}

		// The eye kept off the aircraft or the ship (AnchorClear): the boom is made long enough that the eye's way from
		// where it is, by where its offset's swing carries it, to where it comes to rest keeps clear of the anchor, a
		// glide onto a new anchor included, and the eye taken as the floor's lift will carry it (a glide that skimmed the
		// anchor would have to be pushed off it every frame, a jolt). Only the boom's goal is raised, not his own zoom,
		// so the boom comes back in as the pivot arrives. The shortest such boom is found by halving.
		double SGoal = WantS;
		if (Clear > 0.0)
		{
			const Vec From = RefAt + RelV, To = RefAt + WantRelV;
			const Vec By = From + SwingRel * (SwingRel.Length() / (2.0 * GlidePush));
			auto EyeOff = [&](const Vec& At, double AtHeading, double AtTilt, double Length)
			{
				Vec E = At + BoomOf(AtHeading, AtTilt, Length);
				E.Z = std::max(E.Z, FloorAt(InWorld, E.X, E.Y) + Set.FloorClear);
				return E - AnchorNow;
			};
			auto Keeps = [&](double AtZoom)
			{
				const double Length = ArmOf(AtZoom);
				// (and the pivot gone where it goes with the boom still as it is: a tilt the ceiling holds back may stay so)
				const Vec A = EyeOff(From, H, T, Length), M = EyeOff(By, H, T, Length), B = EyeOff(To, WantH, WantT, Length);
				const Vec Held = EyeOff(To, H, T, Length);
				return std::min({ Nearest(A, M), Nearest(M, B), Nearest(M, Held) }) >= Clear * 1.05;
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

		// (and the boom's goals wait for the pivot under the ceiling: a long boom grows or tilts down, or one tilted up
		// comes in, only as far as the pivot where it is and where its swing takes it leaves room)
		double TGoal = WantT;
		{
			const double PivotZ = RefAt.Z + std::max(RelV.Z + std::max(SwingRel.Z, 0.0) * (SwingRel.Z / (2.0 * GlidePush)), WantRelV.Z) + LiftZ;
			const double Room = Set.EyeMost - PivotZ;
			const double Sn = std::sin(T);
			if (Sn > 1e-9 && ArmOf(SGoal) * Sn > Room) SGoal = std::min(SGoal, std::max(S, ZoomOf(Room / Sn)));
			if (Sn < -1e-9 && ArmOf(SGoal) * Sn > Room) SGoal = std::max(SGoal, std::min(S, ZoomOf(Room / Sn)));
			const double BoomNow = ArmOf(S);
			if (BoomNow * std::sin(TGoal) > Room) TGoal = std::min(TGoal, std::max(T, std::asin(std::clamp(Room / BoomNow, -1.0, 1.0))));
		}

		// The springs. His offset (and the glide onto a new reference) held as a point; the turn, the tilt and the zoom
		// share one budget, each slowed in proportion to what the others ask, so all three together swing the eye no
		// harder than one alone (their pulls on the eye add up: the boom turning while it grows is pushed sideways too).
		// Inside a box his offset may run as fast as the reference it has to stand against at a wall.
		const double Feel = std::max(ArmNow, Set.FeelArm);
		const double Fastest = Set.GlideFastest * Feel + (bBox ? BoxBy : 0.0);
		HeldVec(RelV, SwingRel, WantRelV, Set.Ease, Fastest, Set.GlidePush * Feel, Dt, 1e-10 * Feel);
		const double FastH = std::max(Set.TurnFastest * Rad, 1e-6), FastT = std::max(Set.TiltFastest * Rad, 1e-6);
		const double FastS = std::max(Set.ZoomFastest, 1e-6);
		auto Asks = [](double X, double Want, double Swing, double W, double Fast)
		{
			return std::min(1.0, std::max(std::abs(Swing), 0.5 * W * std::abs(Want - X)) / Fast);
		};
		const double Share = 1.0 / std::max(1.0, Asks(H, WantH, SwingH, Set.TurnEase, FastH) + Asks(T, TGoal, SwingT, Set.TiltEase, FastT) +
			Asks(S, SGoal, SwingS, Set.ZoomEase, FastS));
		const double TiltWas = T;
		Held(H, SwingH, WantH, Set.TurnEase, FastH * Share, Set.TurnPush * Rad * Share, Set.TurnPush * Rad, Dt);
		// (a shot past the tilt's range eases into it from the second frame: the first is the director's shot exactly)
		if (!bFirst || (T >= Set.TiltLeast * Rad && T <= Set.TiltMost * Rad))
		{
			Held(T, SwingT, TGoal, Set.TiltEase, FastT * Share, Set.TiltPush * Rad * Share, Set.TiltPush * Rad, Dt);
		}
		Held(S, SwingS, SGoal, Set.ZoomEase, FastS * Share, Set.ZoomPush * Share, Set.ZoomPush, Dt);
		H = WrapNear(H, 0.0);
		WantH = WrapNear(WantH, H);
		// (out of its range only where a director's shot began it, and then only coming back)
		T = std::clamp(T, std::min(Set.TiltLeast * Rad, TiltWas), std::max(Set.TiltMost * Rad, TiltWas));
		S = std::clamp(S, 0.0, 1.0);

		// The floor's lift: the whole view carried up where the boom would put the eye under FloorClear over the floor
		// round it or ahead of it, eased (fast up, slow down) and held to its push.
		const double Length = ArmOf(S);
		const double FeelNow = std::max(Length, Set.FeelArm);
		const Vec Boom = BoomOf(H, T, Length);
		const Vec Eye0 = RefAt + RelV + Boom;
		double Highest = FloorAt(InWorld, Eye0.X, Eye0.Y);
		// (a director's shot under the floor: put clear at once on the first frame, and said to be a cut)
		if (bFirst && !bCutNow && Eye0.Z + LiftZ < Highest + Set.FloorHard)
		{
			bCutNow = true;
			++CutCount;
		}
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
		// so the eye keeps its clearance instead of trailing it, and the spring is let climb as fast as the goal does on
		// top of what it may still stop from: held to a stop on a goal that runs away from it, it would trail a floor
		// rising faster than some 25 m/s by the whole clearance, put clear at once on every frame)
		const double GoalRate = bLiftGoalKnown && !bCutNow ? std::max((LiftGoal - LiftGoalLast) / Dt, 0.0) : 0.0;
		const double Lead = GoalRate * 2.0 / std::max(Set.LiftRise, 1e-3);
		const double GoalRateWas = LiftRateLast;
		LiftRateLast = GoalRate;
		LiftGoalLast = LiftGoal;
		bLiftGoalKnown = true;
		if (bCutNow)
		{
			LiftZ = LiftGoal;
			SwingLift = 0.0;
		}
		else if (!bFirst)
		{
			// (not on the first frame after Begin, which is the director's shot exactly; it starts on the next)
			const double Goal = LiftGoal > 0.0 ? LiftGoal + Lead : 0.0;
			Held(LiftZ, SwingLift, Goal, Goal > LiftZ ? Set.LiftRise : Set.LiftSettle, 1e9, Set.LiftPush * FeelNow, Set.LiftPush * FeelNow, Dt, LiftGoal > 0.0 ? GoalRate : 0.0);
		}
		if (LiftZ < 0.0)
		{
			LiftZ = 0.0;
			SwingLift = std::max(SwingLift, 0.0);
		}

		// The hard caps, last: nothing here moves the view unless a cap moved (or, by a centimetre or two, his offset's
		// spring came in on a ceiling-held height while moving sideways). Where two meet, the higher wins: the floor (the
		// eye is never under the ground or a wave), then the ceiling, then the anchor's clearance, then the pivot's box. The clearance pushes the eye out away from her and never down (under
		// a ship a push down would put the eye in the sea), the floor taken again wherever that push took it.
		auto EyeNow = [this, &Boom]() { return RefAt + RelV + Vec{ 0.0, 0.0, LiftZ } + Boom; };
		if (bBox)
		{
			const Vec Pv = RefAt + RelV + Vec{ 0.0, 0.0, LiftZ };
			const Vec Kept = InBox(Pv);
			// (a last bit's rounding of a want held at a wall that moves with the reference is no stop)
			const double Out = (Kept - Pv).Length();
			RelV += Kept - Pv;
			if (Out > 1e-9 * std::max(1.0, Pv.Length())) ++HardStopCount;
		}
		bool bLiftedHard = false;
		auto FloorHold = [&]()
		{
			const Vec E = EyeNow();
			const double Under = FloorAt(InWorld, E.X, E.Y) + Set.FloorHard - E.Z;
			if (Under > 0.0)
			{
				// (the eye put clear goes on up at the speed it was carried, where the floor's goal has climbed that fast
				// over this frame and the last, a climb and not a step, and its push can stop that within the boom's feel:
				// a floor that stops rising does not stop the eye dead)
				LiftZ += Under;
				const double Carried = std::min({ SwingLift + Under / Dt, GoalRate, GoalRateWas, FeelNow * std::sqrt(2.0 * std::max(Set.LiftPush, 0.0)) });
				SwingLift = std::max({ SwingLift, Carried, 0.0 });
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
			// (a last bit's rounding of a want that stands the eye at the ceiling is no stop)
			if (Above > 1e-9 * std::max(1.0, std::abs(Set.EyeMost)))
			{
				SwingRel.Z = std::min(SwingRel.Z, 0.0);
				++HardStopCount;
			}
			FloorHold();
		}
		if (bLiftedHard) ++HardLiftCount;

		// The pivot's own speed over this frame, for the next change of reference (not across a cut or a hard stop,
		// which are no speed).
		const Vec PivotNow = RefAt + RelV;
		ChordVel = (PivotNow - PivotEnd) * (1.0 / Dt);
		bChordGood = bPivotEndGood && !bCutNow && HardStopCount == StopsWas;
		PivotEnd = PivotNow;
		bPivotEndGood = true;
		bFirst = false;

		Compose(bCutNow);
		return Now;
	}
}
