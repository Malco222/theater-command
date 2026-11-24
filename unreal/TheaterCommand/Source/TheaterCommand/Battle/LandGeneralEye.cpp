#include "Battle/LandGeneralEye.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace LandGeneralEye
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

		double WrapNear(double A, double Near)
		{
			while (A - Near > Pi) A -= 2.0 * Pi;
			while (A - Near < -Pi) A += 2.0 * Pi;
			return A;
		}
	}

	double Eye::Ground(double X, double Y) const
	{
		if (Here.Height)
		{
			const double Z = Here.Height(X, Y);
			if (Z == Z) return Z;
		}
		return Rec ? Rec->Ground.HeightAt(X, Y) : 0.0;
	}

	double Eye::CropAt(double X, double Y) const
	{
		if (!Here.Crop) return 0.0;
		// (the corn stands where it stands: kept by the half-metre square for a couple of seconds, the scene asked
		// once, not a hundred times a frame)
		const long long Key = (static_cast<long long>(std::floor(X * 2.0)) << 32) ^ static_cast<long long>(static_cast<unsigned int>(static_cast<int>(std::floor(Y * 2.0))));
		const auto It = Crops.find(Key);
		if (It != Crops.end() && Clock - It->second.second < 2.0) return It->second.first;
		double C = Here.Crop(X, Y);
		C = C == C ? std::clamp(C, 0.0, 3.0) : 0.0;
		if (Crops.size() > 16384) Crops.clear();
		Crops[Key] = std::make_pair(C, Clock);
		return C;
	}

	bool Eye::Clear(const Vec& At, double R, bool bFree) const
	{
		// (Free is the film's ground-level word, a house's floor or a bank's hedge whatever the height: asked only
		// within FreeUnder of the ground; Solid has the things' own shapes)
		++Asked;
		const double G = Ground(At.X, At.Y);
		if (At.Z > G + Set.Tallest) return true;
		// (the point itself first: most points looked at and refused are inside the thing)
		if (Here.Solid && Here.Solid(At, -1) != 0) return false;
		const Vec Round[] = { Vec(1, 0, 0), Vec(-1, 0, 0), Vec(0, 1, 0), Vec(0, -1, 0), Vec(0, 0, -0.6), Vec(0, 0, 0.6) };
		if (Here.Free && At.Z < G + (bFree ? Set.FreeUnder : Set.FreeHard))
		{
			if (!Here.Free(At)) return false;
			for (const Vec& D : Round)
			{
				if (!Here.Free(At + D * R)) return false;
			}
		}
		if (!Here.Solid) return true;
		// (things kept R off; men only ManClear: the view may stand among the men, never in one; no man stands this
		// high over the ground, even on a tank)
		const double Rm = std::min(R, Set.ManClear);
		const bool bMen = R > Rm && At.Z < G + 4.5;
		for (const Vec& D : Round)
		{
			const int What = Here.Solid(At + D * R, -1);
			if (What == 2 || (What == 1 && R <= Rm)) return false;
			if (bMen && Here.Solid(At + D * Rm, -1) == 1) return false;
		}
		return true;
	}

	double Eye::ArmAt(double Zoom) const
	{
		const double Far = Set.Highest / std::sin(Set.TiltFar * Rad);
		return Set.NearArm * std::pow(Far / Set.NearArm, std::clamp(Zoom, 0.0, 1.0));
	}

	double Eye::TiltAt(double Zoom) const
	{
		return (Set.TiltNear + (Set.TiltFar - Set.TiltNear) * std::clamp(Zoom, 0.0, 1.0)) * Rad;
	}

	double Eye::AimAt(double Zoom) const
	{
		const double U = 1.0 - std::clamp(Zoom, 0.0, 1.0);
		return Set.NearAim * U * U;
	}

	double Eye::UpAt(double Zoom) const
	{
		return AimAt(Zoom) + ArmAt(Zoom) * std::sin(TiltAt(Zoom));
	}

	double Eye::TiltOwn(double Zoom, double Own) const
	{
		// (his tilt over the automatic one, met softly: a quarter-square across TiltSoft either side of it, so the view
		// never turns a corner as he tilts back down to it or as the zoom carries the automatic tilt past his)
		const double Auto = TiltAt(Zoom);
		const double Soft = Set.TiltSoft * Rad;
		const double D = std::min(Own, Set.TiltMost * Rad) - Auto;
		const double Over = Soft <= 0.0 ? std::max(D, 0.0) : D >= Soft ? D : D <= -Soft ? 0.0 : (D + Soft) * (D + Soft) / (4.0 * Soft);
		return Auto + Over;
	}

	double Eye::TiltMostAt(double Zoom) const
	{
		// (toward a man's shoulder his tilt is handed back to the automatic one, as the eased tilt can follow: zoomed
		// right in, the view stands with the men and looks along the ground as it always has)
		const double Low = TiltAt(Zoom) - (Set.TiltSoft + 1.0) * Rad;
		const double U = std::clamp(Zoom / std::max(1e-6, Set.TiltOwnFrom), 0.0, 1.0);
		return Low + (Set.TiltMost * Rad - Low) * U * U * (3.0 - 2.0 * U);
	}

	double Eye::UpOwn(double Zoom, double Own) const
	{
		return AimAt(Zoom) + ArmAt(Zoom) * std::sin(TiltOwn(Zoom, Own));
	}

	double Eye::TopZoom() const
	{
		double Lo = 0.0, Hi = 1.0;
		for (int K = 0; K < 40; ++K)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(UpOwn(Mid, Set.TopTilt * Rad) < Set.TopUp ? Lo : Hi) = Mid;
		}
		return 0.5 * (Lo + Hi);
	}

	double Eye::TiltDegrees() const
	{
		return TiltFor(S, Tl) / Rad;
	}

	void Eye::TiltWish(const Input& In)
	{
		const double Soft = Set.TiltSoft * Rad;
		// (his own tilt begun where the automatic one is, just under its soft floor: the view does not move as it is set)
		auto Own = [&]()
		{
			if (bOwnTilt) return;
			bOwnTilt = true;
			Tl = WantTl = TlGoal = TiltAt(S) - Soft;
			SwingTl = 0.0;
		};
		if (In.bTopView)
		{
			if (bTop)
			{
				// (pressed again: back to the zoom and the tilt it had, or to tilting with the zoom)
				bTop = false;
				WantS = TopWasS;
				if (bTopWasOwn) WantTl = TopWasTl;
				else bToAuto = true;
			}
			else
			{
				bTopWasOwn = bOwnTilt && !bToAuto;
				TopWasTl = WantTl;
				TopWasS = WantS;
				Own();
				bToAuto = false;
				bTop = true;
				// (almost straight down, from about TopUp up, or from as high as it already was)
				WantTl = Set.TopTilt * Rad;
				WantS = std::max(WantS, TopZoom());
			}
		}
		if (In.Tilt != 0.0)
		{
			Own();
			bTop = false;
			bToAuto = false;
			// (from where the view is tilted, never from under the automatic tilt's floor, so a press always moves it;
			// tilted all the way back down, the view goes back to tilting with the zoom)
			const double Floor = TiltAt(WantS) - Soft - Rad;
			WantTl = std::min(std::max(WantTl, Floor) + In.Tilt * Rad, Set.TiltMost * Rad);
			if (In.Tilt < 0.0 && WantTl <= Floor)
			{
				WantTl = Floor;
				bToAuto = true;
			}
		}
		if (bToAuto) WantTl = TiltAt(WantS) - Soft - Rad;
		// (the top view keeps its tilt wanted, whatever a wait at a thing let go of: it gets there once clear)
		if (bTop) WantTl = Set.TopTilt * Rad;
	}

	void Eye::Index()
	{
		if (!Rec || Rec->Tracks.size() == TracksSeen) return;
		TracksSeen = Rec->Tracks.size();
		std::map<int, std::vector<int>> Of;
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			if (!Rec->Tracks[I].bTwin) Of[Rec->Tracks[I].Block].push_back(static_cast<int>(I));
		}
		ByBlock.assign(Of.begin(), Of.end());
	}

	bool Eye::BlockAt(int Block, double Battle, double& OutX, double& OutY) const
	{
		auto It = std::lower_bound(ByBlock.begin(), ByBlock.end(), Block, [](const std::pair<int, std::vector<int>>& P, int B) { return P.first < B; });
		if (It == ByBlock.end() || It->first != Block) return false;
		double Sx = 0.0, Sy = 0.0;
		int N = 0;
		for (const int I : It->second)
		{
			const LandFeed::Track& Tr = Rec->Tracks[static_cast<size_t>(I)];
			if (Tr.At.empty() || Tr.Start() > Battle || (Tr.Fell >= 0.0 && Tr.Fell <= Battle) || (Tr.Out >= 0.0 && Tr.Out <= Battle)) continue;
			const LandFeed::Spot P = LandFeed::SpotAt(Tr, Battle);
			Sx += P.X;
			Sy += P.Y;
			++N;
		}
		if (N == 0) return false;
		OutX = Sx / N;
		OutY = Sy / N;
		return true;
	}

	void Eye::ClampWanted()
	{
		const double M = Set.Margin;
		const double W = Rec ? Rec->Ground.W : 0.0, Ht = Rec ? Rec->Ground.H : 0.0;
		if (W > 0.0) WantX = std::clamp(WantX, -M, W + M);
		if (Ht > 0.0) WantY = std::clamp(WantY, -M, Ht + M);
		WantS = std::clamp(WantS, 0.0, 1.0);
	}

	bool Eye::Begin(const LandFeed::Recording& InRecording, const LandEye::World& InWorld, const Vec& LookAtIn, double Heading, double Up,
		const Settings& InSettings)
	{
		Rec = &InRecording;
		Here = InWorld;
		Set = InSettings;
		TracksSeen = static_cast<size_t>(-1);
		ByBlock.clear();
		Index();
		// The zoom whose eye stands Up over the look-at (the height grows with the zoom).
		double Lo = 0.0, Hi = 1.0;
		for (int K = 0; K < 40; ++K)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(UpAt(Mid) < Up ? Lo : Hi) = Mid;
		}
		WantX = Tx = LookAtIn.X;
		WantY = Ty = LookAtIn.Y;
		WantS = S = 0.5 * (Lo + Hi);
		WantH = H = Heading;
		ClampWanted();
		Tx = WantX;
		Ty = WantY;
		S = WantS;
		SwingX = SwingY = SwingH = SwingS = 0.0;
		Lift = LiftSwing = 0.0;
		PushX = PushY = PushSwingX = PushSwingY = 0.0;
		bBrake = false;
		BrakeNeed = BrakeFor = StuckFor = 0.0;
		for (AsideMemo& M : Memo) M.When = -1e9;
		Tops.clear();
		Crops.clear();
		bGliding = false;
		Follow = -1;
		Tl = WantTl = SwingTl = TlGoal = 0.0;
		bOwnTilt = bToAuto = bTop = bTopWasOwn = false;
		bFirst = true;
		Now = LandEye::Pose();
		return true;
	}

	Eye::Kept Eye::Keep() const
	{
		Kept K;
		K.GroundEased = GroundEased;
		K.GroundSwing = GroundSwing;
		K.AimZ = AimZ;
		K.PushX = PushX;
		K.PushY = PushY;
		K.PushSwingX = PushSwingX;
		K.PushSwingY = PushSwingY;
		K.Lift = Lift;
		K.LiftSwing = LiftSwing;
		K.NeedWas = NeedWas;
		K.Hurried = Hurried;
		K.bBrake = bBrake;
		K.BrakeNeed = BrakeNeed;
		K.BrakeFor = BrakeFor;
		K.StuckFor = StuckFor;
		return K;
	}

	void Eye::Restore(const Kept& K)
	{
		GroundEased = K.GroundEased;
		GroundSwing = K.GroundSwing;
		AimZ = K.AimZ;
		PushX = K.PushX;
		PushY = K.PushY;
		PushSwingX = K.PushSwingX;
		PushSwingY = K.PushSwingY;
		Lift = K.Lift;
		LiftSwing = K.LiftSwing;
		NeedWas = K.NeedWas;
		Hurried = K.Hurried;
		bBrake = K.bBrake;
		BrakeNeed = K.BrakeNeed;
		BrakeFor = K.BrakeFor;
		StuckFor = K.StuckFor;
	}

	int Eye::Place(double Dt, bool bHold, Vec& OutEye, Vec& OutLook)
	{
		// On the arm, then over the ground, the corn and any thing in its way.
		const double ArmNow = ArmAt(S), TiltNow = TiltFor(S, Tl);
		const Vec F2(std::sin(H), std::cos(H), 0.0);
		{
			// (the ground under the look-at, taken over a patch the size of the view and eased, so the view does not
			// bob over every bank, furrow and shell hole it slides across)
			const double R = std::clamp(0.08 * ArmNow * std::cos(TiltNow), 0.0, 25.0);
			double G = Ground(Tx, Ty);
			if (R > 0.5) G = 0.2 * (G + Ground(Tx + R, Ty) + Ground(Tx - R, Ty) + Ground(Tx, Ty + R) + Ground(Tx, Ty - R));
			if (bFirst) { GroundEased = G; GroundSwing = 0.0; }
			else Spring(GroundEased, GroundSwing, G, Set.GroundEase, Dt);
		}
		AimZ = GroundEased + AimAt(S);
		const Vec Look(Tx, Ty, AimZ);
		Vec Eye0 = Look - F2 * (ArmNow * std::cos(TiltNow)) + Vec(0.0, 0.0, ArmNow * std::sin(TiltNow));
		// (what the eye keeps clear of: the wanted clearance, and a nearer hard one it never comes inside, so the eased
		// lift has room to lag a frame or two without a snap)
		auto FloorAt = [this](const Vec& At, bool bHard = false)
		{
			const Vec Round[] = { Vec(0, 0, 0), Vec(0.8, 0, 0), Vec(-0.8, 0, 0), Vec(0, 0.8, 0), Vec(0, -0.8, 0) };
			double Grounds[5];
			double Highest = -1e9;
			for (int K = 0; K < 5; ++K)
			{
				Grounds[K] = Ground(At.X + Round[K].X, At.Y + Round[K].Y);
				Highest = std::max(Highest, Grounds[K]);
			}
			// (well over the tallest corn there is, the corn is not asked)
			const double Least = bHard ? Set.HardGround : Set.GroundClear, Over = bHard ? Set.HardOverCrop : Set.OverCrop;
			if (At.Z > Highest + 3.0 + Over + 0.5) return Highest + Least;
			double Floor = -1e9;
			for (int K = 0; K < 5; ++K)
			{
				Floor = std::max(Floor, Grounds[K] + std::max(Least, CropAt(At.X + Round[K].X, At.Y + Round[K].Y) + Over));
			}
			return Floor;
		};
		const double Want = Set.ThingClear, Hard = Set.HardClear;
		// (a thing in the way, or the ground or the corn too near: the whole view rises over it, eye and look-at
		// together, as a crane would carry it, so the picture is lifted and never tipped; a tall thing near a low eye,
		// a tree's trunk under its crown or a house's wall, is stepped round instead, the view carried aside)
		auto LiftWithin = [&](const Vec& At, double Most, double& Out)
		{
			const double Base = std::max(0.0, FloorAt(At) - At.Z);
			// (half a metre at a time near, a metre further up)
			for (double L = 0.0; L <= Most + 1e-9; L += L < 4.0 ? 0.5 : 1.0)
			{
				if (Clear(Vec(At.X, At.Y, At.Z + Base + L), Want))
				{
					// (to a tenth, so the eased lift is fed a slope, not a staircase)
					while (L - 0.1 >= -1e-9 && Clear(Vec(At.X, At.Y, At.Z + Base + L - 0.1), Want)) L -= 0.1;
					Out = Base + std::max(0.0, L);
					return true;
				}
			}
			Out = Base + Most;
			return false;
		};
		const double Reach = std::max(Set.LiftLow, 0.5 * (Eye0.Z - Ground(Eye0.X, Eye0.Y)));
		// (open sky over a low eye: no crown or eaves over it, so it can always rise straight up; a view near the
		// ground never stands under a tree, it steps out from under first)
		auto SkyOver = [&](const Vec& At)
		{
			const double Top = Ground(At.X, At.Y) + Set.Tallest;
			for (const double Up : { 1.5, 3.0, 5.0, 8.0, 12.0, 18.0 })
			{
				if (At.Z + Up > Top) break;
				if (!Clear(At + Vec(0.0, 0.0, Up), Hard + 0.2, false)) return false;
			}
			return true;
		};
		// (a step aside from a tall thing at a point: the nearest place round it a short lift clears under open sky,
		// keeping to the side already stepped to; Off is the step the point already has)
		// (the eye barely moves between frames: a step found or not found a moment ago near the same place is taken
		// again, the found one checked once more, so the search runs about once in a fifth of a second)
		auto AsideAt = [&](const Vec& At, const Vec& Off, Vec& OutD, double& OutL)
		{
			for (const AsideMemo& M : Memo)
			{
				if (Clock - M.When > 0.2 || (M.At - At).Length() > 0.4) continue;
				if (!M.bOk) return false;
				double L = 0.0;
				if (LiftWithin(At + M.D, 2.0, L) && SkyOver(At + M.D + Vec(0.0, 0.0, L)))
				{
					OutD = M.D;
					OutL = L;
					return true;
				}
			}
			double Best = 1e9;
			bool bFound = false;
			auto Remember = [&](bool bOk)
			{
				AsideMemo& M = Memo[MemoNext];
				MemoNext = (MemoNext + 1) % static_cast<int>(sizeof(Memo) / sizeof(Memo[0]));
				M.At = At;
				M.D = OutD;
				M.When = Clock;
				M.bOk = bOk;
				return bOk;
			};
			for (const double Rr : { 1.0, 2.0, 3.0, 4.5, 6.0 })
			{
				for (int K = 0; K < 12; ++K)
				{
					const double A = K * Pi / 6.0;
					const Vec D(std::sin(A) * Rr, std::cos(A) * Rr, 0.0);
					double L = 0.0;
					// (a column shut at the short lift's top is shut below it too, near enough: one look, not five)
					if (!Clear(At + D + Vec(0.0, 0.0, std::max(0.0, FloorAt(At + D) - At.Z) + 2.0), Want)) continue;
					if (!LiftWithin(At + D, 2.0, L)) continue;
					const double Cost = Rr + 2.0 * L + 0.3 * std::hypot(Off.X + D.X - PushX, Off.Y + D.Y - PushY);
					if (Cost < Best && SkyOver(At + D + Vec(0.0, 0.0, L)))
					{
						Best = Cost;
						OutD = D;
						OutL = L;
						bFound = true;
					}
				}
				if (bFound) return Remember(true);
			}
			return Remember(false);
		};
		// (over a tall thing that cannot be stepped round, a tree standing in a hedge: how high the view must go, in
		// coarse steps from where a short lift failed)
		// (a thing's top stays where it is: kept by the metre square for a moment, as the tent looks at the same
		// trees frame after frame)
		auto TopOver = [&](const Vec& At, double From)
		{
			const long long Key = (static_cast<long long>(std::floor(At.X)) << 32) ^ static_cast<long long>(static_cast<unsigned int>(static_cast<int>(std::floor(At.Y))));
			const auto It = Tops.find(Key);
			if (It != Tops.end() && Clock - It->second.second < 0.5 && It->second.first - At.Z >= From + 1.5 - 1e-9) return std::min(Set.LiftMost, It->second.first - At.Z);
			double Top = Set.LiftMost;
			for (double L = From + 1.5; L <= Set.LiftMost + 1e-9; L += 1.5)
			{
				if (Clear(Vec(At.X, At.Y, At.Z + L), Want))
				{
					Top = L;
					break;
				}
			}
			if (Tops.size() > 4096) Tops.clear();
			Tops[Key] = std::make_pair(At.Z + Top, Clock);
			return Top;
		};
		double Need = 0.0;
		Vec PushWant;
		const bool bLow = Eye0.Z < Ground(Eye0.X, Eye0.Y) + Set.Tallest + 2.0;
		if (bHold)
		{
			// (held where it was: the step aside as it is, and the lift there)
			PushWant = Vec(PushX, PushY, 0.0);
			if (bLow && !LiftWithin(Eye0 + PushWant, Reach, Need)) LiftWithin(Eye0 + PushWant, Set.LiftMost, Need);
		}
		else if (bLow && (!LiftWithin(Eye0, Reach, Need) || !SkyOver(Eye0 + Vec(0.0, 0.0, Need))))
		{
			if (!AsideAt(Eye0, Vec(), PushWant, Need))
			{
				// (no step aside found: stay where the view has stepped to, and rise there)
				PushWant = Vec(PushX, PushY, 0.0);
				LiftWithin(Eye0 + PushWant, Set.LiftMost, Need);
			}
		}
		else if (bLow)
		{
			// (the lift is shaped like a tent over each thing round the eye, sloping Slope m a metre: the eye rises
			// before it reaches a hedge whichever way it comes, by a slide, a turn or a zoom as well as a pan)
			for (const double Rr : { 1.5, 3.0, 4.5, 6.0 })
			{
				if (Rr * Set.Slope <= 0.0) break;
				for (int K = 0; K < 8; ++K)
				{
					const double A = (K + 0.5 * (Rr == 3.0 || Rr == 6.0)) * Pi / 4.0;
					double L = 0.0;
					const Vec At = Eye0 + Vec(std::sin(A) * Rr, std::cos(A) * Rr, 0.0);
					if (!LiftWithin(At, Reach, L)) L = TopOver(At, L);
					Need = std::max(Need, L - Set.Slope * Rr);
				}
			}
		}
		// (how fast and hard the lift may climb: by the eye's distance to what it looks at, so a view near the ground
		// climbs a hedge gently and a high one is not held back)
		const double Far = std::max(ArmNow, 1.0);
		const double Fastest = std::max(1.5, Set.LiftSpeed * Far), Hardest = Set.LiftPush * Far;
		// (the way up shut over the eye, under a crown or eaves: the lift reaches no higher than that, however fast it
		// may climb; in Sedan's woods a turn under a crown ran on into the next tree and stopped dead in a frame)
		double Ceiling = 1e9;
		if (bLow && !bFirst && !bHold)
		{
			const Vec Column = Eye0 + Vec(PushX, PushY, 0.0);
			for (double Up = 0.5; Up <= 2.0 + 1e-9; Up += 0.5)
			{
				if (!Clear(Column + Vec(0.0, 0.0, Lift + Up), Hard, false))
				{
					Ceiling = Lift + Up - 0.5;
					break;
				}
			}
		}
		// (the lift it could reach in Tau seconds at the most, a little short of it)
		auto Reachable = [&](double Tau)
		{
			const double V0 = std::clamp(LiftSwing, 0.0, Fastest);
			const double T1 = std::min(Tau, (Fastest - V0) / Hardest);
			return std::min(Ceiling, Lift + 0.8 * (V0 * T1 + 0.5 * Hardest * T1 * T1 + Fastest * (Tau - T1)));
		};
		bool bBrakeNow = false;
		double BrakeNeedNow = 0.0;
		if (bLow && !bFirst && !bHold && Dt > 1e-4)
		{
			// (and where the arm is carrying the eye over the next 1.6 s, so it climbs at most Climb m/s before it gets
			// there, as an operator sees the hedge coming and stands up; a tall thing coming is stepped round early)
			// (the way the look-at, the zoom and the turn are going, each no further than where it is wanted: the
			// springs stop there, so a zoom coming in is not carried on into the ground)
			auto Toward = [](double X, double V, double W, double Tau) { return X + std::clamp(V * Tau, std::min(0.0, W - X), std::max(0.0, W - X)); };
			const double WantHNear = WrapNear(WantH, H);
			auto ArmEyeAt = [&](double Tau)
			{
				const double Xa = Toward(Tx, SwingX, WantX, Tau), Ya = Toward(Ty, SwingY, WantY, Tau);
				const double Sa = std::clamp(Toward(S, SwingS, WantS, Tau), 0.0, 1.0), Ha = Toward(H, SwingH, WantHNear, Tau);
				const double A = ArmAt(Sa), Ta = TiltFor(Sa, Toward(Tl, SwingTl, TlGoal, Tau));
				return Vec(Xa, Ya, GroundEased + AimAt(Sa)) - Vec(std::sin(Ha), std::cos(Ha), 0.0) * (A * std::cos(Ta)) + Vec(0.0, 0.0, A * std::sin(Ta));
			};
			if ((ArmEyeAt(1.6) - Eye0).Length() > 0.05)
			{
				// (and where the lift cannot climb in time, within the time the view takes to stop, it brakes)
				const double StopIn = 2.0 / (bGliding ? Set.Glide : std::min({ Set.Ease, Set.ZoomEase, Set.TurnEase, bOwnTilt ? Set.TiltEase : Set.Ease })) + 0.15;
				for (int K = 1; K <= 16; ++K)
				{
					const double Tau = 0.1 * K;
					// (and the step aside as it is going there)
					const Vec Ahead = ArmEyeAt(Tau) + Vec(Toward(PushX, PushSwingX, PushWant.X, Tau), Toward(PushY, PushSwingY, PushWant.Y, Tau), 0.0);
					double L = 0.0;
					bool bTall = false;
					if (!LiftWithin(Ahead, Reach, L))
					{
						Vec D;
						if (AsideAt(Ahead, PushWant, D, L) && (PushWant + D).Length() <= Set.PushMost) PushWant = PushWant + D;
						else L = TopOver(Ahead, L);
						bTall = true;
					}
					Need = std::max(Need, L - Set.Climb * Tau);
					if (Tau <= StopIn && L - 0.4 > Reachable(Tau))
					{
						bBrakeNow = true;
						BrakeNeedNow = std::max(BrakeNeedNow, L);
					}
					// (and where the eye will stand by then, stepped aside and lifted only as far as the springs are
					// carrying it now, inside the near clearance: it would stop dead there in a frame (a turn under a
					// crown in Sedan's woods, 3,382 deg/s/s); it brakes now instead)
					if (Tau <= StopIn && !bBrakeNow && !Clear(Ahead + Vec(0.0, 0.0, Toward(Lift, LiftSwing, std::max(Lift, L), Tau)), Hard, false))
					{
						bBrakeNow = true;
						BrakeNeedNow = std::max(BrakeNeedNow, L);
					}
					if (bTall) break;
				}
			}
		}
		if (bBrake && (Lift >= BrakeNeed - 0.3 || BrakeFor > Set.BrakeLongest))
		{
			// (climbed, or waited long enough: on again; after a long wait, not braking again for a second)
			BrakeFor = BrakeFor > Set.BrakeLongest ? -1.0 : 0.0;
			bBrake = false;
		}
		if (bBrakeNow && !bFirst && BrakeFor >= 0.0)
		{
			BrakeNeed = bBrake ? std::max(BrakeNeed, BrakeNeedNow) : BrakeNeedNow;
			bBrake = true;
		}
		if (bBrake) Need = std::max(Need, BrakeNeed);
		// (gliding fast and low, as a drone flies: lifted with its speed, so the trees on its way are under it, not met in
		// a frame between two looks ahead; it settles as the glide slows. Never past the Ceiling over it: under a crown the
		// glide's own lift would leave the lift short of its need, and the stuck rule would cut for nothing seen; the
		// pre-check's read of 6240956e)
		if (bLow && bGliding && !bFirst) Need = std::max(Need, std::min({ Set.LiftMost, Set.GlideLift * std::hypot(SwingX, SwingY), Ceiling }));
		if (bFirst)
		{
			PushX = PushWant.X;
			PushY = PushWant.Y;
			PushSwingX = PushSwingY = 0.0;
		}
		else if (bHold) PushSwingX = PushSwingY = 0.0;
		else
		{
			Spring(PushX, PushSwingX, PushWant.X, Set.PushRate, Dt);
			Spring(PushY, PushSwingY, PushWant.Y, Set.PushRate, Dt);
		}
		const Vec Eye1 = Eye0 + Vec(PushX, PushY, 0.0);
		// (coming down after a hedge, never down through a tree's crown or a roof: the view settles only as far as the
		// way down is open, and stays over the thing until it is passed)
		auto UpFrom = [&](double L, double R)
		{
			const double From = L;
			for (; L <= Set.LiftMost + 1e-9; L += 0.5)
			{
				if (Clear(Eye1 + Vec(0.0, 0.0, L), R, R >= Want))
				{
					// (and no higher than it must: back down in tenths toward where it began)
					while (L - 0.1 >= From && Clear(Eye1 + Vec(0.0, 0.0, L - 0.1), R, R >= Want)) L -= 0.1;
					return L;
				}
			}
			return Set.LiftMost;
		};
		if (!bFirst && Need < Lift)
		{
			if (!Clear(Eye1 + Vec(0.0, 0.0, Lift), Want)) Need = std::max(Need, UpFrom(Lift, Want));
			else
			{
				double L = Lift;
				while (L - 0.5 >= Need && Clear(Eye1 + Vec(0.0, 0.0, L - 0.5), Want)) L -= 0.5;
				if (L - 0.5 >= Need) Need = L;
			}
		}
		const double LiftBefore = Lift, SwingBefore = LiftSwing;
		NeedWas = Need;
		Hurried = 0;
		if (bFirst) Lift = Need;
		else
		{
			// (eased, and never faster or harder than the picture bears)
			// (and never up into a thing: it climbs only as far as the way up is open, a little short of it)
			double Goal = Need;
			if (Need > Lift + 0.05)
			{
				for (double Up = 0.5; Up <= 2.0 + 1e-9 && Lift + Up - 0.5 < Need; Up += 0.5)
				{
					if (!Clear(Eye1 + Vec(0.0, 0.0, Lift + Up), Hard, false))
					{
						Goal = std::min(Need, Lift + Up - 0.5);
						break;
					}
				}
			}
			// (shut in under leaves or eaves with nowhere to step: a moment, then a cut to where it should be)
			StuckFor = Goal < Need - 0.5 ? StuckFor + Dt : 0.0;
			// (over a thing while the zoom brings the arm down, the lift must climb as fast as the arm comes down: led
			// by that speed, so the eased lift does not lag behind it into the near clearance)
			if (Goal > 0.05 && Goal >= Need - 1e-9)
			{
				// (and with his own tilt, as fast as the tilt brings it down too)
				const double ArmDown = bOwnTilt ? -((UpOwn(S + 1e-3 * SwingS, Tl + 1e-3 * SwingTl) - UpOwn(S, Tl)) / 1e-3 + GroundSwing)
					: -((UpAt(S + 1e-3) - UpAt(S)) / 1e-3 * SwingS + GroundSwing);
				if (ArmDown > 0.0) Goal += std::min(ArmDown, Fastest) * 2.0 / Set.RiseRate;
			}
			double L = Lift, Sw = LiftSwing;
			Spring(L, Sw, Goal, Goal > Lift ? Set.RiseRate : Set.SettleRate, Dt);
			Sw = std::clamp(Sw, LiftSwing - Hardest * Dt, LiftSwing + Hardest * Dt);
			Sw = std::clamp(Sw, -Fastest, Fastest);
			Lift += 0.5 * (LiftSwing + Sw) * Dt;
			LiftSwing = Sw;
		}
		Vec EyeAt = Eye1 + Vec(0.0, 0.0, Lift);
		if (EyeAt.Z < FloorAt(EyeAt, true) || !Clear(EyeAt, Hard, false))
		{
			// (the spring is late: inside the near clearance it is hurried up to it, fast but eased; only an eye that
			// would stand in the thing itself, or at the ground, is put clear at once)
			const double Floor = FloorAt(EyeAt, true) - Eye1.Z;
			const double Up = UpFrom(std::max(Lift, Floor), Hard);
			const double Was = Lift;
			Hurried = 1;
			if (!bFirst)
			{
				// (hurried, but its push held under what the picture bears at this distance, HurryPush: at the nearest
				// zoom, a few metres from its look-at, a spring left free lifted the whole view 2 cm in a frame, 1,274
				// degrees a second squared in Kursk; what it cannot climb in time is put clear with a cut, below)
				double L = LiftBefore, Sw = SwingBefore;
				Spring(L, Sw, std::max(Up, Need), Set.HurryRate, Dt);
				const double Push = Set.HurryPush * Far;
				Sw = std::clamp(Sw, SwingBefore - Push * Dt, SwingBefore + Push * Dt);
				Lift = LiftBefore + 0.5 * (SwingBefore + Sw) * Dt;
				LiftSwing = Sw;
			}
			Lift = std::max(Lift, Was);
			EyeAt = Eye1 + Vec(0.0, 0.0, Lift);
			// (more than a small step in a frame is a jolt the picture would feel: better a cut)
			if (EyeAt.Z < Ground(EyeAt.X, EyeAt.Y) + 0.25 || !Clear(EyeAt, 0.05, false) || Lift - Was > Set.HurryMost)
			{
				Hurried = 2;
				Lift = Up;
			}
		}
		if (Hurried == 2 || StuckFor > Set.StuckMost)
		{
			// (put clear at once, at the height it wants, and the picture cuts)
			Hurried = 2;
			Lift = UpFrom(std::max(Lift, Need), Want);
			LiftSwing = 0.0;
			StuckFor = 0.0;
			bBrake = false;
			EyeAt = Eye1 + Vec(0.0, 0.0, Lift);
		}
		OutEye = EyeAt;
		OutLook = Look + Vec(PushX, PushY, Lift);
		return Hurried;
	}

	LandEye::Pose Eye::Update(const Input& In, double RealDt, double Battle)
	{
		if (!Rec) return Now;
		Index();
		const double Dt = std::clamp(RealDt, 0.0, 0.25);
		Clock += Dt;
		Asked = 0;
		const Vec Fwd(std::sin(H), std::cos(H), 0.0);
		const Vec Right(std::cos(H), -std::sin(H), 0.0);
		const double Arm = ArmAt(S), Tilt = TiltFor(S, Tl);
		// (the keys' pan by the zoom alone, as fast at his own tilt as at the automatic one: looking straight down the
		// arm's reach across the ground is nothing)
		const double Across = Arm * std::cos(TiltAt(S));

		// What the player asked.
		if (In.bGoTo)
		{
			WantX = In.GoTo.X;
			WantY = In.GoTo.Y;
			bGliding = true;
			Follow = -1;
		}
		if (In.GoBlock >= 0)
		{
			Follow = In.GoBlock;
			bGliding = true;
		}
		const double Px = std::clamp(In.PanRight + (In.bLockEdgePan ? 0.0 : In.EdgeRight), -1.0, 1.0);
		const double Py = std::clamp(In.PanAhead + (In.bLockEdgePan ? 0.0 : In.EdgeAhead), -1.0, 1.0);
		const bool bSlide = In.SlideRight != 0.0 || In.SlideAhead != 0.0;
		if (Px != 0.0 || Py != 0.0 || bSlide)
		{
			Follow = -1;
			bGliding = false;
		}
		if (Px != 0.0 || Py != 0.0)
		{
			const double Speed = Set.PanSpeed * Across + Set.PanLeast;
			const Vec Go = (Right * Px + Fwd * Py) * (Speed * Dt);
			WantX += Go.X;
			WantY += Go.Y;
		}
		Vec SlideGo;
		if (bSlide)
		{
			// (the picture's width at the look-at: a slide across the whole picture carries the ground across it)
			const double Fov = Set.FovNear + (Set.FovFar - Set.FovNear) * S;
			const double Wide = 2.0 * Arm * std::tan(0.5 * Fov * Rad) * 16.0 / 9.0;
			const Vec Go = Right * (In.SlideRight * Wide) + Fwd * (In.SlideAhead * Wide * 9.0 / 16.0 / std::max(0.35, std::sin(Tilt)));
			WantX += Go.X;
			WantY += Go.Y;
			if (!bBrake) SlideGo = Go;
		}
		if (Follow >= 0)
		{
			double Bx = 0.0, By = 0.0;
			if (BlockAt(Follow, Battle, Bx, By))
			{
				WantX = Bx;
				WantY = By;
			}
			else Follow = -1;
		}
		WantS -= In.Zoom * Set.ZoomStep;
		// (following a unit, the view stays a little back from its men: riding with them is the film's)
		if (Follow >= 0) WantS = std::max(WantS, Set.FollowNearest);
		WantH += In.Turn * Rad;
		ClampWanted();
		TiltWish(In);
		if (Rec->Ground.W > 0.0) Tx = std::clamp(Tx, -Set.Margin, Rec->Ground.W + Set.Margin);
		if (Rec->Ground.H > 0.0) Ty = std::clamp(Ty, -Set.Margin, Rec->Ground.H + Set.Margin);

		// Eased toward it.
		const double Tx0 = Tx, Ty0 = Ty, S0 = S, H0 = H, Tl0 = Tl;
		if (Rec->Ground.W > 0.0) Tx = std::clamp(Tx + SlideGo.X, -Set.Margin, Rec->Ground.W + Set.Margin);
		if (Rec->Ground.H > 0.0) Ty = std::clamp(Ty + SlideGo.Y, -Set.Margin, Rec->Ground.H + Set.Margin);
		if (bFirst)
		{
			Tx = WantX;
			Ty = WantY;
			S = WantS;
			H = WantH;
			Tl = TlGoal = std::min(WantTl, TiltMostAt(S));
		}
		else
		{
			const double Rate = bGliding ? Set.Glide : Set.Ease;
			WantH = WrapNear(WantH, H);
			if (bBrake)
			{
				// (braking at a thing the lift cannot climb in time: each move dies away smoothly, a spring let go at the
				// point it would come to rest, while the player's wishes wait close by)
				WantX = Tx + std::clamp(WantX - Tx, -2.0, 2.0);
				WantY = Ty + std::clamp(WantY - Ty, -2.0, 2.0);
				WantS = S + std::clamp(WantS - S, -0.05, 0.05);
				WantH = H + std::clamp(WantH - H, -10.0 * Rad, 10.0 * Rad);
				Spring(Tx, SwingX, Tx + SwingX / Rate, Rate, Dt);
				Spring(Ty, SwingY, Ty + SwingY / Rate, Rate, Dt);
				Spring(S, SwingS, S + SwingS / Set.ZoomEase, Set.ZoomEase, Dt);
				Spring(H, SwingH, H + SwingH / Set.TurnEase, Set.TurnEase, Dt);
				if (bOwnTilt)
				{
					WantTl = Tl + std::clamp(WantTl - Tl, -5.0 * Rad, 5.0 * Rad);
					Spring(Tl, SwingTl, Tl + SwingTl / Set.TiltEase, Set.TiltEase, Dt);
				}
				BrakeFor += Dt;
				++Braked;
			}
			else
			{
				Spring(Tx, SwingX, WantX, Rate, Dt);
				Spring(Ty, SwingY, WantY, Rate, Dt);
				Spring(H, SwingH, WantH, Set.TurnEase, Dt);
				// (a spring held to its fastest and hardest: from a swing already faster, slowed no harder than the push)
				auto Held = [Dt](double& X, double& Swing, double Want, double W, double Fastest, double Push)
				{
					double X1 = X, Sw = Swing;
					Spring(X1, Sw, Want, W, Dt);
					const double Cap = std::max(Fastest, std::abs(Swing) - Push * Dt);
					Sw = std::clamp(std::clamp(Sw, Swing - Push * Dt, Swing + Push * Dt), -Cap, Cap);
					X += 0.5 * (Swing + Sw) * Dt;
					Swing = Sw;
				};
				if (bOwnTilt)
				{
					// (with his own tilt, the zoom and the tilt eased and never faster or harder than the picture bears:
					// the top view's swing of some fifty degrees and its climb from a man's shoulder take a second or two;
					// his tilt eases toward what he wants as far as the zoom lets it, handed back toward the shoulder)
					Held(S, SwingS, WantS, Set.ZoomEase, Set.ZoomFastest, Set.ZoomPush);
					TlGoal = std::min(WantTl, TiltMostAt(S));
					Held(Tl, SwingTl, TlGoal, Set.TiltEase, Set.TiltFastest * Rad, Set.TiltPush * Rad);
				}
				else Spring(S, SwingS, WantS, Set.ZoomEase, Dt);
				if (BrakeFor < 0.0) BrakeFor = std::min(0.0, BrakeFor + Dt);
			}
			S = std::clamp(S, 0.0, 1.0);
			if (bGliding && std::hypot(WantX - Tx, WantY - Ty) < 1.0 && std::hypot(SwingX, SwingY) < 1.0) bGliding = false;
		}
		if (Follow >= 0 && !bGliding && !In.bFollow) Follow = -1;
		H = WrapNear(H, 0.0);
		WantH = WrapNear(WantH, H);

		// Where the eye stands. Where this frame's move would carry it inside the near clearance faster than the lift
		// can climb, the view waits where it was this once, as a man stops at a hedge, while the lift climbs.
		const Kept Before = Keep();
		Vec EyeAt, LookAt3;
		const int Hu = Place(Dt, false, EyeAt, LookAt3);
		if (Hu > 0 && !bFirst)
		{
			// (half the move first, then none: the least stop that keeps it clear)
			const Kept After = Keep();
			const double Tx1 = Tx, Ty1 = Ty, S1 = S, H1 = H, Tl1 = Tl;
			const double Turned = WrapNear(H1, H0) - H0;
			bool bTook = false;
			for (const double F : { 0.5, 0.0 })
			{
				Restore(Before);
				Tx = Tx0 + F * (Tx1 - Tx0);
				Ty = Ty0 + F * (Ty1 - Ty0);
				S = S0 + F * (S1 - S0);
				H = WrapNear(H0 + F * Turned, 0.0);
				Tl = Tl0 + F * (Tl1 - Tl0);
				Vec EyeHeld, LookHeld;
				const int Got = Place(Dt, true, EyeHeld, LookHeld);
				if (Got == 0 || (F == 0.0 && Got < Hu))
				{
					SwingX *= F;
					SwingY *= F;
					SwingS *= F;
					SwingH *= F;
					SwingTl *= F;
					if (bOwnTilt) WantTl = Tl + std::clamp(WantTl - Tl, -5.0 * Rad, 5.0 * Rad);
					WantX = Tx + std::clamp(WantX - Tx, -2.0, 2.0);
					WantY = Ty + std::clamp(WantY - Ty, -2.0, 2.0);
					WantS = S + std::clamp(WantS - S, -0.05, 0.05);
					WantH = H + std::clamp(WrapNear(WantH, H) - H, -10.0 * Rad, 10.0 * Rad);
					EyeAt = EyeHeld;
					LookAt3 = LookHeld;
					++Waited;
					// (and it waits until the lift has climbed to what the move wanted)
					if (BrakeFor >= 0.0)
					{
						BrakeNeed = std::max(bBrake ? BrakeNeed : 0.0, std::max(After.NeedWas, After.Lift));
						bBrake = true;
					}
					bTook = true;
					break;
				}
			}
			if (!bTook)
			{
				Restore(After);
				Tx = Tx1;
				Ty = Ty1;
				S = S1;
				H = H1;
				Tl = Tl1;
			}
		}
		// (tilted all the way back to the automatic tilt, under its soft floor where his own tilt no longer shows: the
		// view tilts with the zoom again, without a step)
		// (and the zoom come to rest, so its own easing goes back to the automatic one's without a step)
		if (bOwnTilt && bToAuto && !bTop && Tl <= TiltAt(S) - Set.TiltSoft * Rad && std::abs(WantS - S) < 0.005 && std::abs(SwingS) < 0.05)
		{
			bOwnTilt = bToAuto = false;
			Tl = WantTl = SwingTl = TlGoal = 0.0;
		}

		Now.Eye = EyeAt;
		Now.Look = LookAt3;
		Now.Fov = Set.FovNear + (Set.FovFar - Set.FovNear) * S;
		Now.Roll = 0.0;
		Now.Shot = LandEye::Kind::Wide;
		Now.Subject = -1;
		// (where the view could not be kept clear smoothly, in a wood, and was put clear at once, the picture cuts)
		Now.bCut = bFirst || Hurried == 2;
		bFirst = false;
		return Now;
	}
}
