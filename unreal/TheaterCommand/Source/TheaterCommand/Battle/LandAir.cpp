#include "Battle/LandAir.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <type_traits>

// (each a*b+c rounded as two steps, never fused into one: clang fuses them by default where the machine has a fused
// multiply-add, as the Mac's arm64 has, and the lines across a canopy, routed through many near ties, then came out up
// to 0.1 m elsewhere lying and nearly 1 m falling in than on a PC. Unreal's Mac build passes -ffp-contract=off to all
// of this module already; this keeps it so wherever LandAir is built, as the test's run.sh does)
#if defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace LandAir
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Rad = Pi / 180.0;
		// The way in: the level run before the push-over (metres); how far past its own side's front a lane starts at the
		// least (the art director: never over its own men), and how near the strip it may be to count; the trail's gap
		// (seconds: each dive its own beat, the next pilot running in through the leader's dust).
		constexpr double LevelIn = 1100.0, OwnClear = 150.0, OwnAside = 60.0, GapLeast = 6.0, GapMost = 12.0;
		// The climb away and the break: the climb's curve, the straight past the bottom, the turn's radius and how far it
		// turns (metres, degrees); the roll into and out of it (seconds); and how long a plane is drawn after its bottom.
		constexpr double ClimbCurve = 250.0, Straight = 350.0, TurnR = 700.0, TurnMost = 100.0, Roll = 1.2, Away = 16.0, LevelOver = 120.0;
		// The ground's guns: how far from the run's middle a block may stand and fire, the blocks and guns a block, and
		// how far they reach (metres), and their tracers' speed (the kit's tracer.speed_mps).
		constexpr double PostsWithin = 800.0, Reach = 900.0, TracerMps = 760.0;
		constexpr int PostBlocks = 4, PostGuns = 2;
		// A downed pilot (-TheaterLandChutes): his army's infantry clips' lengths (go_prone, stand_up, kneel_down) and the
		// run's stride (metres a loop of run, its loop's seconds); go_prone's time where, played back, he is up on his
		// knees.
		constexpr double ProneS = 1.9, StandS = 0.6, KneelS = 0.5, RunStride = 2.451, RunLoop = 0.8, KneeT = 0.95;
		// The art director's notes on them (24 November, 02:26Z): out under LowExit (metres over the ground) he pulls
		// at once and lands before his canopy has filled; else it fills over 45 to 50 m of his fall from his pull. His
		// way through the air dies away as a falling man's does (BodyDrag, a metre's: g over the square of the 55 m/s a
		// man falls at), and at once as his canopy fills (its drag as it holds him up, his sink's, and CanopyHolds, a
		// second's at full, so that its last few metres a second go too). A man 4.1 m under his canopy swings about 4 s
		// a swing (SwingS), from 14 degrees to a gentle 5, settling over SwingSettle (seconds). A wind over DragWind
		// (metres a second) drags him on the ground. A heavy man (one in five) comes down at 6 to 7 m/s.
		constexpr double LowExit = 60.0, FillLeast = 45.0, FillMost = 50.0, BodyDrag = 9.81 / (55.0 * 55.0), CanopyHolds = 2.0;
		constexpr double SwingS = 4.1, SwingFirst = 14.0, SwingLast = 5.0, SwingSettle = 7.0, DragWind = 4.0, HeavyShare = 0.2;
		// After a hard landing he limps: his run clip at this share of its pace, hurried off his hurt leg by Hitch (a
		// share of the loop's pace either way), and only so far as this share of a sound man's run.
		constexpr double LimpPace = 0.65, Hitch = 0.45, LimpFar = 0.6;
		// The scene's blend into each of his clips from the one before (real seconds), and into the harness's kneel a
		// longer one: go_prone on his knees is far from kneel_down's last pose (in the soldier files his pelvis 55
		// degrees off it, his right thigh 68 to 72, and his pelvis 0.18 m on), and no time of go_prone is much nearer (45
		// degrees at best, half up). Over 0.5 s he settles into his kneel rather than lurching into it, and is still for
		// the rest of his 1 to 1.5 s at the harness (a review of 24 November; Mac shot 8 looks for it).
		constexpr double BlendS = 0.25, KneelBlendS = 0.5;
		// How long his canopy takes to fall in once he is down (real seconds), on an even clock: each part of its falling
		// in eases in and out on its own (HeapShape), so that none moves much faster than the rest (a review of 24
		// November: over 2.5 s on an eased clock its crown had swept down at 17 m/s at its fastest).
		constexpr double FallInS = 3.2;

		struct Dice
		{
			uint64_t S;
			explicit Dice(uint64_t Seed) : S(Seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull) {}
			double U()
			{
				S += 0x9E3779B97F4A7C15ull;
				uint64_t Z = S;
				Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
				Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
				Z ^= Z >> 31;
				return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
			}
			double U(double A, double B) { return A + (B - A) * U(); }
		};

		double Smooth(double X)
		{
			X = std::clamp(X, 0.0, 1.0);
			return X * X * (3.0 - 2.0 * X);
		}

		V3 Plus(const V3& A, const V3& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
		V3 Times(const V3& A, double K) { return { A.X * K, A.Y * K, A.Z * K }; }
		double Length(const V3& A) { return std::sqrt(A.X * A.X + A.Y * A.Y + A.Z * A.Z); }
		/** The way a heading points (degrees clockwise from north), and its right hand. */
		V3 Ahead(double Heading) { return { std::sin(Heading * Rad), std::cos(Heading * Rad), 0.0 }; }
		V3 RightOf(double Heading) { return { std::cos(Heading * Rad), -std::sin(Heading * Rad), 0.0 }; }
	}

	Machine MachineOf(const std::string& Nat, int Year)
	{
		// The ground-attack machine each air force flew, of the models the kit has (the art director's unit table; the
		// Thunderbolt and the Typhoon are not sculpted, so the Americans fly the Mustang from 1944 and the British the
		// Spitfire from 1943, both of which strafed in Normandy). The Germans the Stuka through 1943 (still their main dive
		// bomber in the East at Kursk) till the Fw 190's fighter-bomber took its place (1944); the Russians always the
		// Il-2, low and shallow; a nation with none of its own a plain fighter in its paint.
		Machine M;
		const bool bEmpire = Nat == "UK" || Nat == "CAN" || Nat == "AUS" || Nat == "NZL" || Nat == "SAF" || Nat == "IND";
		if (Nat == "GER" && Year < 1944)
		{
			// (from high, rolled over into a 75 degree dive, its two guns a short burst on the way down, out at about 300 m;
			// once the bombs are drawn, the bomb leaving it at about 600 m is the shot)
			M.Model = "Stuka"; M.Span = 13.8; M.Speed = 140.0; M.High = 1800.0; M.Low = 300.0; M.Dive = 75.0; M.Climb = 30.0; M.Gun = "rifle"; M.Rate = 16.0;
			M.Open = 1100.0; M.Shut = 700.0; M.PushG = 6.5; M.PullG = 6.0;
		}
		else if (Nat == "GER")
		{
			M.Model = "Fw190"; M.Span = 10.5; M.Speed = 140.0; M.Gun = "cannon20"; M.Rate = 14.0;
		}
		else if (Nat == "USA")
		{
			M.Model = Year >= 1944 ? "Mustang" : "Warhawk"; M.Span = Year >= 1944 ? 11.3 : 11.4; M.Speed = Year >= 1944 ? 140.0 : 125.0; M.Gun = "hmg"; M.Rate = 22.0;
			M.High = Year >= 1944 ? 800.0 : 700.0;
		}
		else if (bEmpire)
		{
			M.Model = Year >= 1943 ? "Spitfire" : "Hurricane"; M.Span = Year >= 1943 ? 11.2 : 12.2; M.Speed = Year >= 1943 ? 135.0 : 115.0; M.High = Year >= 1943 ? 800.0 : 700.0;
			M.Gun = Year >= 1942 ? "cannon20" : "rifle"; M.Rate = Year >= 1942 ? 14.0 : 18.0;
		}
		else if (Nat == "SOV")
		{
			M.Model = "Sturmovik"; M.Span = 14.6; M.Speed = 105.0; M.High = 160.0; M.Low = 15.0; M.Dive = 8.0; M.Climb = 18.0; M.Gun = "cannon20"; M.Rate = 12.0;
			M.Open = 700.0; M.Shut = 200.0; M.PullG = 3.0;
		}
		else if (Nat == "JAP" || Nat == "JPN")
		{
			M.Model = "Zero"; M.Span = 12.0; M.Speed = 120.0; M.Gun = "cannon20"; M.Rate = 12.0;
		}
		else
		{
			M.Model = "Fighter"; M.Span = 10.6; M.Speed = 120.0; M.Gun = "hmg"; M.Rate = 18.0;
		}
		return M;
	}

	void Sky::Build(const LandFeed::Recording& Rec, const GroundFn& Ground, double InLife)
	{
		Plans.clear();
		GroundAt = Ground;
		Life = std::clamp(InLife, 0.25, 8.0);
		NotesN = Rec.Air.size();
		auto GroundZ = [this](double X, double Y) { return GroundAt ? GroundAt(X, Y) : 0.0; };
		auto PlanOf = [this](int Flight) -> Plan* {
			for (auto It = Plans.rbegin(); It != Plans.rend(); ++It)
			{
				if (It->Flight == Flight) return &*It;
			}
			return nullptr;
		};
		// Each leg, by its coming over its run.
		for (const LandFeed::AirNote& N : Rec.Air)
		{
			if (N.What != "land:air-in" || N.Side < 0) continue;
			Plan P;
			P.Flight = N.Flight;
			P.Side = N.Side;
			P.Models = std::clamp(N.Models, 1, 4);
			P.Year = N.Year;
			P.Nat = N.Nat;
			P.M = MachineOf(N.Nat, N.Year);
			P.Tin = N.T;
			for (int K = 0; K < 4; ++K) P.Run[K] = N.Run[K];
			const double Dx = P.Run[2] - P.Run[0], Dy = P.Run[3] - P.Run[1];
			P.Len = std::max(1.0, std::hypot(Dx, Dy));
			P.Fx = Dx / P.Len;
			P.Fy = Dy / P.Len;
			if (std::hypot(Dx, Dy) < 1e-6) { P.Fx = std::sin(N.Heading); P.Fy = std::cos(N.Heading); }
			P.Head = std::atan2(P.Fx, P.Fy) / Rad;
			P.Falls.assign(static_cast<size_t>(P.Models), Down());
			Plans.push_back(std::move(P));
		}
		// Its strafe: the strip's width and how long the rules have its guns rake it.
		std::vector<double> Secs(Plans.size(), -1.0);
		for (const LandFeed::AirNote& N : Rec.Air)
		{
			if (N.What != "land:strafe") continue;
			if (Plan* P = PlanOf(N.Flight))
			{
				P->Width = std::clamp(N.Width, 10.0, 200.0);
				Secs[static_cast<size_t>(P - Plans.data())] = N.Secs;
			}
		}
		for (size_t I = 0; I < Plans.size(); ++I)
		{
			Plan& P = Plans[I];
			const Machine& M = P.M;
			P.Base = GroundZ(P.Run[0] + P.Fx * P.Len * 0.5, P.Run[1] + P.Fy * P.Len * 0.5);
			// Its lane: from the strip's start, or 150 m past its own side's front where that stands on the strip (the
			// rules' strip is the rules'; only where its guns are seen to strike moves on), leaving 40 m of it at the least.
			P.Us = 0.0;
			if (const LandFeed::View* V = Rec.ViewAt(P.Tin))
			{
				for (const LandFeed::Unit& U : V->Units)
				{
					if (U.Side != P.Side || U.Str <= 0.05) continue;
					const double Along = (U.X - P.Run[0]) * P.Fx + (U.Y - P.Run[1]) * P.Fy, Aside = (U.X - P.Run[0]) * P.Fy - (U.Y - P.Run[1]) * P.Fx;
					if (std::fabs(Aside) <= P.Width * 0.5 + OwnAside && Along >= -OwnClear && Along <= P.Len) P.Us = std::max(P.Us, Along + OwnClear);
				}
			}
			P.Us = std::min(P.Us, std::max(0.0, P.Len - 40.0));
			// Its way in, as a line: level at its height, the push-over into its dive (a circle of the push's g), the dive
			// down the line aimed at its lane's middle, and the pull-out (a circle of the pull's g) to its bottom.
			const double V = M.Speed, G = 9.81;
			P.Gam = std::clamp(M.Dive, 3.0, 85.0) * Rad;
			P.Rq = V * V / (G * std::max(0.5, M.PushG));
			P.Rp = V * V / (G * std::max(0.5, M.PullG - 1.0));
			const double Sn = std::sin(P.Gam), Tn = std::tan(P.Gam), Half = std::tan(P.Gam * 0.5);
			const double Drop = std::max(10.0, M.High - M.Low);
			double E = P.Rq * Half, Dd = P.Rp * Half;
			if ((E + Dd) * Sn > Drop)
			{
				// (no room for both turns at their g between its heights: both tighter, so the dive is all turn)
				const double K = Drop / ((E + Dd) * Sn);
				P.Rq *= K; P.Rp *= K; E *= K; Dd *= K;
			}
			const double Um = (P.Us + P.Len) * 0.5;
			P.Uq = Um - M.High / Tn - E;
			P.Uc = Um - M.Low / Tn + Dd;
			P.Ua = P.Uq - LevelIn;
			P.Seg[0] = LevelIn;
			P.Seg[1] = P.Rq * P.Gam;
			P.Seg[2] = std::max(0.0, Drop / Sn - E - Dd);
			P.Seg[3] = P.Rp * P.Gam;
			const double Sb = P.Seg[0] + P.Seg[1] + P.Seg[2] + P.Seg[3];
			// Its guns: open once its lane's start is within the machine's range, shut once its lane's end is within its
			// closest (or at the bottom); at least a second's burst.
			double F0 = -1.0, F1 = -1.0;
			for (double S = 0.0; S <= Sb; S += 2.0)
			{
				double U = 0.0, H = 0.0, Pitch = 0.0;
				WayIn(P, S, U, H, Pitch);
				if (F0 < 0.0 && std::hypot(U - P.Us, H) <= M.Open) F0 = S;
				if (F0 >= 0.0 && std::hypot(U - P.Len, H) <= M.Shut) { F1 = S; break; }
			}
			if (F0 < 0.0) F0 = std::max(0.0, Sb - 3.0 * V);
			if (F1 < 0.0) F1 = Sb;
			F1 = std::min(Sb, std::max(F1, F0 + V));
			P.Sf0 = F0;
			P.Pass = std::max(0.5, (F1 - F0) / V);
			P.Bottom = (Sb - F0) / V;
			// (the trail: each dive its own beat, 6 to 12 s apart, longer where the rules have the strip raked longer)
			const double Rules = (Secs[I] > 0.0 ? Secs[I] : P.Len / 120.0) * Life;
			Dice Dg(static_cast<uint64_t>(P.Flight) * 2654435761ull + 0x6A9ull);
			P.Gap = P.Models > 1 ? std::clamp(std::max((Rules - P.Pass) / (P.Models - 1), Dg.U(GapLeast, GapLeast + 3.0)), GapLeast, GapMost) : 0.0;
		}
		// The planes it cost: the last of the leg not yet lost, hit in its pass or its pull-out.
		for (const LandFeed::AirNote& N : Rec.Air)
		{
			if (N.What != "air:lost") continue;
			Plan* P = PlanOf(N.Flight);
			if (!P) continue;
			int Slot = P->Models - 1;
			while (Slot >= 0 && P->Falls[static_cast<size_t>(Slot)].How != Fall::None) --Slot;
			if (Slot < 0) continue;
			const Fall How = N.Kind == "explode" ? Fall::Explode : N.Kind == "spin" ? Fall::Spin : Fall::Burn;
			PlanFall(*P, Slot, How, (N.T - P->Tin) * Life);
			// (its pilot, where the rules say he got out: never from a plane blown apart)
			if (N.Chutes > 0 && How != Fall::Explode && P->Falls[static_cast<size_t>(Slot)].How != Fall::None)
			{
				const V3 Wind = Rec.Weather.bWind ? V3{ Rec.Weather.WindX, Rec.Weather.WindY, 0.0 } : V3{ 2.6, 1.2, 0.0 };
				PlanChute(*P, Slot, Wind);
				Down& D = P->Falls[static_cast<size_t>(Slot)];
				// (down, he runs from the nearest enemy block from where he gets up: the other side's, by the view as he
				// landed)
				const V3 UpAt = D.DragTo;
				const LandFeed::View* V = Rec.ViewAt(P->Tin + D.Landed / Life);
				double Best = 1e18, Ex = 0.0, Ey = 0.0;
				if (V)
				{
					for (const LandFeed::Unit& U : V->Units)
					{
						if (U.Side < 0 || U.Side == P->Side || U.Str <= 0.05) continue;
						const double Dd = std::hypot(U.X - UpAt.X, U.Y - UpAt.Y);
						if (Dd < Best) { Best = Dd; Ex = U.X; Ey = U.Y; }
					}
				}
				D.RunHeading = Best < 1e17 && Best > 1.0 ? std::atan2(UpAt.X - Ex, UpAt.Y - Ey) / Rad : D.Face;
				Dice Dr(static_cast<uint64_t>(P->Flight) * 92821ull + static_cast<uint64_t>(Slot) * 7193ull + 0xC47E0ull);
				const double Far = Dr.U(20.0, 40.0), Speed = Dr.U(4.2, 5.0);
				// (after a hard landing he limps, on his run clip slowed: not so far, his feet keeping to its stride, and
				// ending on a half loop, where its hitch has caught up)
				const double Goes = D.bLimp ? std::max(0.5, std::round(2.0 * Far * LimpFar / RunStride) * 0.5) * RunStride : Far;
				D.RunTo = Plus(UpAt, Times(Ahead(D.RunHeading), Goes));
				D.RunTo.Z = GroundZ(D.RunTo.X, D.RunTo.Y);
				D.RunS = D.bLimp ? Goes / (D.Pace * RunStride / RunLoop) : Far / Speed;
			}
		}
		// The ground's guns: the other side's blocks nearest the run's middle as the leg comes, two guns a block, each
		// in its own ragged bursts from as the first plane dives till the last has gone.
		for (size_t I = 0; I < Plans.size(); ++I)
		{
			Plan& P = Plans[I];
			const LandFeed::View* V = Rec.ViewAt(P.Tin);
			if (!V) continue;
			const double Mx = P.Run[0] + P.Fx * P.Len * 0.5, My = P.Run[1] + P.Fy * P.Len * 0.5;
			std::vector<std::pair<double, const LandFeed::Unit*>> Near;
			for (const LandFeed::Unit& U : V->Units)
			{
				if (U.Side < 0 || U.Side == P.Side || U.Str <= 0.05) continue;
				const double D = std::hypot(U.X - Mx, U.Y - My);
				if (D <= PostsWithin) Near.emplace_back(D, &U);
			}
			std::sort(Near.begin(), Near.end(), [](const auto& A, const auto& B) { return A.first != B.first ? A.first < B.first : A.second->Id < B.second->Id; });
			if (Near.size() > static_cast<size_t>(PostBlocks)) Near.resize(static_cast<size_t>(PostBlocks));
			const double End = (P.Models - 1) * P.Gap + P.Bottom + 5.0;
			int K = 0;
			for (const auto& Pair : Near)
			{
				for (int G = 0; G < PostGuns; ++G, ++K)
				{
					Post Po;
					Po.Key = static_cast<int>(I) * 8 + K;
					Dice D(static_cast<uint64_t>(P.Flight) * 7919ull + static_cast<uint64_t>(Pair.second->Id) * 104729ull + static_cast<uint64_t>(G) * 31ull + 0xA11ull);
					const double Ox = D.U(-12.0, 12.0), Oy = D.U(-12.0, 12.0);
					Po.At = { Pair.second->X + Ox, Pair.second->Y + Oy, 0.0 };
					Po.At.Z = GroundZ(Po.At.X, Po.At.Y) + 1.1;
					for (double T = D.U(-3.0, -1.0); T < End;)
					{
						Burst B;
						B.On = T;
						B.Off = T + D.U(1.0, 2.4);
						B.Across = D.U(-18.0, 18.0);
						B.Up = D.U(-6.0, 14.0);
						B.Lead = D.U(0.2, 0.7);
						Po.Bursts.push_back(B);
						T = B.Off + D.U(0.5, 1.5);
					}
					P.Posts.push_back(std::move(Po));
				}
			}
		}
	}

	void Sky::WayIn(const Plan& P, double S, double& U, double& H, double& Pitch)
	{
		const Machine& M = P.M;
		const double Sn = std::sin(P.Gam), Cs = std::cos(P.Gam);
		if (S <= P.Seg[0])
		{
			U = P.Ua + S;
			H = M.High;
			Pitch = 0.0;
			return;
		}
		S -= P.Seg[0];
		if (S <= P.Seg[1])
		{
			const double F = S / std::max(1.0, P.Rq);
			U = P.Uq + P.Rq * std::sin(F);
			H = M.High - P.Rq * (1.0 - std::cos(F));
			Pitch = -F;
			return;
		}
		S -= P.Seg[1];
		const double U2 = P.Uq + P.Rq * Sn, H2 = M.High - P.Rq * (1.0 - Cs);
		if (S <= P.Seg[2])
		{
			U = U2 + S * Cs;
			H = H2 - S * Sn;
			Pitch = -P.Gam;
			return;
		}
		S -= P.Seg[2];
		const double F = P.Gam - std::min(S, P.Seg[3]) / std::max(1.0, P.Rp);
		U = P.Uc - P.Rp * std::sin(F);
		H = M.Low + P.Rp * (1.0 - std::cos(F));
		Pitch = -F;
	}

	bool Sky::Fly(const Plan& P, int Slot, double Tau, Sample& Out) const
	{
		const Machine& M = P.M;
		const double V = M.Speed;
		const double Tp = Tau - Slot * P.Gap;
		const double First = -P.Sf0 / V;
		if (Tp < First || Tp > P.Bottom + Away) return false;
		const int N = P.Models;
		const double W0 = N > 1 ? (Slot - (N - 1) * 0.5) * std::min(18.0, P.Width / N) : 0.0;
		double U = 0.0, W = W0, H = M.High, Slope = 0.0, Turned = 0.0, Bank = 0.0, Dived = 0.0;
		bool bIn = false;
		if (Tp <= P.Bottom)
		{
			// Coming in high down the run's line, the push-over, the dive, the pull-out (at its speed along its way).
			WayIn(P, P.Sf0 + V * Tp, U, H, Dived);
			bIn = true;
		}
		else
		{
			// The climb away from the bottom, and the break, left and right in turn.
			const double S = V * (Tp - P.Bottom), Tc = std::tan(M.Climb * Rad);
			H = S < ClimbCurve ? M.Low + Tc * S * S / (2.0 * ClimbCurve) : M.Low + Tc * (ClimbCurve * 0.5 + S - ClimbCurve);
			Slope = S < ClimbCurve ? Tc * S / ClimbCurve : Tc;
			// (levelling off some way over its height before the dive, smoothly: it goes home, it does not climb away)
			const double Top = M.High + LevelOver, Soft = 60.0;
			if (H > Top - Soft)
			{
				const double E = std::exp(-(H - (Top - Soft)) / Soft);
				H = Top - Soft * E;
				Slope *= E;
			}
			const double Side = Slot % 2 ? 1.0 : -1.0;
			if (S <= Straight)
			{
				U = P.Uc + S;
			}
			else
			{
				const double Arc = S - Straight, Most = TurnMost * Rad, Phi = std::min(Arc / TurnR, Most), Past = std::max(0.0, Arc - Most * TurnR);
				U = P.Uc + Straight + TurnR * std::sin(Phi) + Past * std::cos(Phi);
				W = W0 + Side * (TurnR * (1.0 - std::cos(Phi)) + Past * std::sin(Phi));
				Turned = Side * Phi / Rad;
				const double Full = std::atan(V * V / (9.81 * TurnR)) / Rad;
				Bank = Side * Full * Smooth(Arc / (V * Roll)) * (1.0 - Smooth(Past / (V * Roll)));
			}
		}
		Out.T = Tau;
		Out.At.X = P.Run[0] + P.Fx * U + P.Fy * W;
		Out.At.Y = P.Run[1] + P.Fy * U - P.Fx * W;
		Out.At.Z = P.Base + H;
		if (GroundAt) Out.At.Z = std::max(Out.At.Z, GroundAt(Out.At.X, Out.At.Y) + 6.0);
		Out.Heading = P.Head + Turned;
		Out.Pitch = (bIn ? Dived : std::atan(Slope)) / Rad;
		Out.Bank = Bank;
		return true;
	}

	void Sky::Flown(const Plan& P, int Slot, double Tau, Plane& Out) const
	{
		// (its velocity across a twentieth of a second, one-sided at the ends of the time it is drawn)
		Sample A, B;
		double Ta = Tau - 0.025, Tb = Tau + 0.025;
		if (!Fly(P, Slot, Ta, A)) { Ta = Tau; Fly(P, Slot, Ta, A); }
		if (!Fly(P, Slot, Tb, B)) { Tb = Tau; Fly(P, Slot, Tb, B); }
		Out.Vel = Tb > Ta ? Times(Plus(B.At, Times(A.At, -1.0)), 1.0 / (Tb - Ta)) : V3();
		const double Tp = Tau - Slot * P.Gap;
		Out.bFiring = Tp >= 0.0 && Tp <= P.Pass;
		if (Out.bFiring)
		{
			const int N = P.Models;
			const double W0 = N > 1 ? (Slot - (N - 1) * 0.5) * std::min(18.0, P.Width / N) : 0.0, U = P.Us + (P.Len - P.Us) * Tp / std::max(0.1, P.Pass);
			Out.Aim = { P.Run[0] + P.Fx * U + P.Fy * W0, P.Run[1] + P.Fy * U - P.Fx * W0, 0.0 };
			Out.Aim.Z = GroundAt ? GroundAt(Out.Aim.X, Out.Aim.Y) : 0.0;
			const V3 Right = RightOf(Out.Heading);
			for (int K = 0; K < 2; ++K) Out.Muzzle[K] = Plus(Plus(Out.At, Times(Right, K ? 2.4 : -2.4)), { 0.0, 0.0, -0.3 });
		}
	}

	void Sky::PlanFall(Plan& P, int Slot, Fall How, double Tau)
	{
		Down& D = P.Falls[static_cast<size_t>(Slot)];
		D = Down();
		D.How = How;
		// (blown up in its pass or as it pulls out; on fire or spinning once it is climbing away, so it has the height
		// to go down in: the rules lose it as the run ends, which is about then)
		const double S0 = Slot * P.Gap;
		D.Hit = How == Fall::Explode ? std::clamp(Tau, S0 + 0.5 * P.Pass, S0 + P.Pass) : std::clamp(Tau, S0 + P.Bottom + 2.0, S0 + P.Bottom + 3.5);
		Sample A, B;
		if (!Fly(P, Slot, D.Hit - 0.025, A) || !Fly(P, Slot, D.Hit + 0.025, B)) { D.How = Fall::None; return; }
		Fly(P, Slot, D.Hit, A);
		D.HitAt = A.At;
		D.HitVel = Times(Plus(B.At, Times(A.At, -1.0)), 40.0);
		auto GroundZ = [this](double X, double Y) { return GroundAt ? GroundAt(X, Y) : 0.0; };
		Dice Dc(static_cast<uint64_t>(P.Flight) * 6151ull + static_cast<uint64_t>(Slot) * 389ull + 0xFA11ull);
		const double Side = Dc.U() < 0.5 ? -1.0 : 1.0, Dt = 1.0 / 30.0;
		V3 Pos = A.At;
		if (How == Fall::Explode)
		{
			// Blown apart: its pieces fall in the kit; its engine and its fuel come down together, slowed, burning.
			V3 Vel = Times(D.HitVel, 0.55);
			Vel.Z += 6.0;
			double T = 0.0;
			while (T < 20.0 && Pos.Z > GroundZ(Pos.X, Pos.Y))
			{
				Vel.Z -= 9.81 * Dt;
				Pos = Plus(Pos, Times(Vel, Dt));
				T += Dt;
			}
			D.Crash = D.Hit + T;
			D.CrashAt = { Pos.X, Pos.Y, GroundZ(Pos.X, Pos.Y) };
			D.CrashVel = Vel;
			return;
		}
		double Heading = A.Heading, Pitch = A.Pitch, Bank = A.Bank, T = 0.0;
		const double Speed = std::max(60.0, Length(D.HitVel));
		V3 Centre = Plus(Pos, Times(RightOf(Heading), Side * 24.0)), Drift = Times(Ahead(Heading), Speed * 0.5);
		// (struck, it stops climbing at once: a spin drops away from the start, about 3 s from 150 m, the art director's)
		double Theta = std::atan2(Pos.Y - Centre.Y, Pos.X - Centre.X), Sink = std::min(-20.0, D.HitVel.Z), Way = Speed;
		V3 Was = Pos;
		D.Path.push_back({ D.Hit, Pos, Heading, Pitch, Bank });
		while (T < 25.0)
		{
			T += Dt;
			if (How == Fall::Burn)
			{
				// On fire: its nose going down into a long shallow dive, turning and rocking a little, losing way.
				Pitch = std::max(Pitch - 18.0 * Dt, -28.0);
				Way *= std::exp(-Dt / 12.0);
				Heading += Side * 7.0 * Dt;
				const double Want = Side * 40.0 + 6.0 * std::sin(1.7 * T);
				Bank += std::clamp(Want - Bank, -25.0 * Dt, 25.0 * Dt);
				Pos = Plus(Pos, Times({ std::sin(Heading * Rad) * std::cos(Pitch * Rad), std::cos(Heading * Rad) * std::cos(Pitch * Rad), std::sin(Pitch * Rad) }, Way * Dt));
			}
			else
			{
				// Out of control: a tight spiral steepening to the ground, its way on dying away.
				Drift = Times(Drift, std::exp(-Dt / 2.0));
				Centre = Plus(Centre, Times(Drift, Dt));
				Theta += -Side * 210.0 * Rad * Dt;
				Sink = std::max(Sink - 25.0 * Dt, -70.0);
				Pos = { Centre.X + 24.0 * std::cos(Theta), Centre.Y + 24.0 * std::sin(Theta), Pos.Z + Sink * Dt };
				const double Dx = Pos.X - Was.X, Dy = Pos.Y - Was.Y;
				if (Dx * Dx + Dy * Dy > 1e-8) Heading = std::atan2(Dx, Dy) / Rad;
				Pitch = -(35.0 + 35.0 * std::min(1.0, T / 1.2));
				Bank = Side * 70.0;
			}
			const double G = GroundZ(Pos.X, Pos.Y);
			if (Pos.Z <= G + 1.0)
			{
				Pos.Z = G;
				D.Path.push_back({ D.Hit + T, Pos, Heading, Pitch, Bank });
				break;
			}
			D.Path.push_back({ D.Hit + T, Pos, Heading, Pitch, Bank });
			Was = Pos;
		}
		D.Crash = D.Path.back().T;
		D.CrashAt = D.Path.back().At;
		const Sample& Before = D.Path.size() >= 2 ? D.Path[D.Path.size() - 2] : D.Path.back();
		D.CrashVel = Times(Plus(D.CrashAt, Times(Before.At, -1.0)), 1.0 / std::max(1e-3, D.Crash - Before.T));
	}

	void Sky::PlanChute(Plan& P, int Slot, const V3& Wind)
	{
		Down& D = P.Falls[static_cast<size_t>(Slot)];
		if (D.Path.size() < 2) return;
		auto GroundZ = [this](double X, double Y) { return GroundAt ? GroundAt(X, Y) : 0.0; };
		auto PathAt = [&D](double Tau, V3& At, V3& Vel)
		{
			auto It = std::upper_bound(D.Path.begin(), D.Path.end(), Tau, [](double Tm, const Sample& S) { return Tm < S.T; });
			const Sample& B = It == D.Path.end() ? D.Path.back() : *It;
			const Sample& A = It == D.Path.begin() ? D.Path.front() : *(It - 1);
			const double K = B.T > A.T ? std::clamp((Tau - A.T) / (B.T - A.T), 0.0, 1.0) : 0.0;
			At = Plus(A.At, Times(Plus(B.At, Times(A.At, -1.0)), K));
			Vel = Times(Plus(B.At, Times(A.At, -1.0)), 1.0 / std::max(1e-3, B.T - A.T));
		};
		Dice Dc(static_cast<uint64_t>(P.Flight) * 15485863ull + static_cast<uint64_t>(Slot) * 3571ull + 0xC47E1ull);
		// Over the side a second or two after the hit (out of a spin sooner, while he still can), never in her last
		// 0.8 s; and where she is too low by then, at the highest she is after the hit.
		const double Last = std::max(D.Hit + 0.1, D.Crash - 0.8);
		double Leave = std::min(D.Hit + (D.How == Fall::Spin ? Dc.U(0.5, 0.9) : Dc.U(1.0, 2.0)), Last);
		V3 At, Vel;
		PathAt(Leave, At, Vel);
		if (At.Z - GroundZ(At.X, At.Y) < 60.0)
		{
			double High = -1e9;
			for (const Sample& S : D.Path)
			{
				if (S.T < D.Hit + 0.3 || S.T > Last) continue;
				const double H = S.At.Z - GroundZ(S.At.X, S.At.Y);
				if (H > High) { High = H; Leave = S.T; }
			}
			PathAt(Leave, At, Vel);
		}
		D.bChute = true;
		D.Out = Leave;
		D.Drop.clear();
		// (the draws as they were and in their order: how long he falls free, how fast he comes down; how long he lies is
		// drawn after his way down)
		const double Delay = Dc.U(1.0, 1.6), Sink0 = Dc.U(5.0, 6.5), Dt = 1.0 / 30.0;
		// The art director's notes (24 November, 02:26Z), on dice of their own so that the rest is drawn as it was: one
		// man in five heavy (6 to 7 m/s); his silk a little warmer or cooler; his canopy's gores and its size in the air;
		// how far it takes to fill from his pull (out too low, how far under the ground it would have); how long he works
		// at his harness; how far a wind drags him, and how fast; which way his swing starts.
		Dice Dn(static_cast<uint64_t>(P.Flight) * 49157ull + static_cast<uint64_t>(Slot) * 6151ull + 0xC47E3ull);
		const bool bHeavy = Dn.U() < HeavyShare;
		const double Warm = Dn.U(-1.0, 1.0);
		D.Gores = 24 + 2 * std::min(2, static_cast<int>(Dn.U() * 3.0));
		D.Across = Dn.U(5.0, 5.5);
		D.Deep = D.Across * Dn.U(0.46, 0.5);
		const double FillAt = Dn.U(FillLeast, FillMost), Past = Dn.U(6.0, 14.0);
		D.ReleaseS = Dn.U(1.0, 1.5);
		const double DragM = Dn.U(1.0, 2.0), DragMps = Dn.U(1.0, 1.5);
		D.SwingSide = Dn.U() < 0.5 ? -1.0 : 1.0;
		const double Sink = bHeavy ? 6.0 + (Sink0 - 5.0) / 1.5 : Sink0;
		// (her off-white for every nation, aircrew's silk: about 4% warmer or cooler a chute)
		D.Silk[0] = 0.86 * (1.0 + 0.04 * Warm);
		D.Silk[1] = 0.85;
		D.Silk[2] = 0.80 * (1.0 - 0.04 * Warm);
		// He leaves her with her way across but none of her climb: a man never goes up out of a plane unless she is
		// zooming, and these are going down (her first note). Falling free nothing holds him up (his own drag is a few
		// per cent at these speeds), so he is at least half g t squared under where he left her. His way through the air
		// (across: her way, less the wind's) dies away as a falling man's does, toward the wind's, which he is in from the
		// first (her second: the wind is not a push that builds from rest), and at once as his canopy fills. He pulls
		// after Delay, or sooner once he has fallen as far as a man from rest falls in it (out of a plane already going
		// down fast), and his canopy fills over FillAt of his fall from there, its hold growing as its mouth does: his way
		// spent, he is with the air (within 1 m/s of the wind) within 3 s of his pull, and never falls straight down as
		// release 1's pilot did ("drops about 50 m straight down before the wind takes him" was her note of that fault).
		// Out under LowExit he pulls at once, and it would fill only Past metres under the ground: he lands before it has.
		const double Have = At.Z - GroundZ(At.X, At.Y);
		const bool bLow = Have < LowExit;
		const double Pull = bLow ? 0.3 : Delay, Fill = bLow ? Have + Past : FillAt;
		Vel.Z = std::min(Vel.Z, 0.0);
		double T = 0.0, Heading = std::atan2(Vel.X, Vel.Y) / Rad, PullZ = 0.0, Open = 0.0;
		bool bPulled = false;
		V3 Pos = At;
		D.Opened = -1.0;
		while (T < 400.0)
		{
			if (!bPulled && (T >= Pull - 1e-9 || At.Z - Pos.Z >= 4.905 * Pull * Pull))
			{
				bPulled = true;
				PullZ = Pos.Z;
			}
			Open = bPulled ? std::clamp((PullZ - Pos.Z) / Fill, 0.0, 1.0) : 0.0;
			if (Open >= 1.0 && D.Opened < 0.0) D.Opened = D.Out + T;
			const double Rx = Vel.X - Wind.X, Ry = Vel.Y - Wind.Y;
			const double Keep = std::exp(-((BodyDrag + 9.81 * Open * Open / (Sink * Sink)) * std::hypot(Rx, Ry) + CanopyHolds * Open * Open) * Dt);
			Vel.X = Wind.X + Rx * Keep;
			Vel.Y = Wind.Y + Ry * Keep;
			for (int K = 0; K < 4; ++K)
			{
				const double Hold = 9.81 * Vel.Z * Vel.Z * Open * Open / (Sink * Sink);
				Vel.Z = std::min(0.0, std::max(Vel.Z + (Hold - 9.81) * Dt * 0.25, -55.0));
			}
			if (Vel.X * Vel.X + Vel.Y * Vel.Y > 0.04) Heading = std::atan2(Vel.X, Vel.Y) / Rad;
			D.Drop.push_back({ D.Out + T, Pos, Heading, Open, 0.0 });
			Pos = Plus(Pos, Times(Vel, Dt));
			T += Dt;
			const double Gn = GroundZ(Pos.X, Pos.Y);
			if (Pos.Z <= Gn)
			{
				Pos.Z = Gn;
				Open = bPulled ? std::clamp((PullZ - Pos.Z) / Fill, 0.0, 1.0) : 0.0;
				D.Drop.push_back({ D.Out + T, Pos, Heading, Open, 0.0 });
				break;
			}
		}
		if (D.Opened < 0.0) D.Opened = D.Out + T;
		D.Landed = D.Out + T;
		D.LandAt = Pos;
		D.Face = Heading;
		D.Lie = Dc.U(1.5, 3.5);
		D.OpenDown = Open;
		D.bHard = Open < 1.0;
		// (his canopy falls in downwind of him, the way the wind goes, or his own, and lies stretched out along it, its
		// middle HeapOff on: the art director's verdict of 24 November 14:34Z, on dice of its own, so that the rest is
		// drawn as it was)
		const double Wh = Wind.X * Wind.X + Wind.Y * Wind.Y > 0.04 ? std::atan2(Wind.X, Wind.Y) / Rad : Heading;
		Dice Dl(static_cast<uint64_t>(P.Flight) * 52711ull + static_cast<uint64_t>(Slot) * 7919ull + 0xC47E5ull);
		D.Folds = static_cast<uint32_t>(Dl.U() * 4294967295.0);
		D.Wind = std::hypot(Wind.X, Wind.Y);
		D.Heap = Plus(Pos, Times(Ahead(Wh), HeapOff(D.Folds, D.Across, D.Wind, D.Face, Wh)));
		D.Heap.Z = GroundZ(D.Heap.X, D.Heap.Y);
		// Down, real seconds from his landing. He goes over flat (sooner after a hard landing), and the scene swaps him
		// for the crowd's man as he lies under his falling canopy (her third note: so that nobody sees him pop). In a wind
		// over DragWind his canopy drags him a metre or two downwind (her fourth). He lies a moment, as he did (after a
		// hard landing 3 to 5 s: her fifth), gets up onto his knees (go_prone played back, slower after a hard landing)
		// and works his harness free, kneeling, his hands at his chest, for 1 to 1.5 s before he stands; after a hard
		// landing he limps.
		D.FallS = D.bHard ? 0.35 : 0.5;
		D.SwapS = D.FallS + 0.4;
		const bool bDrag = std::hypot(Wind.X, Wind.Y) > DragWind;
		D.DragS = bDrag ? DragM / DragMps : 0.0;
		D.DragTo = bDrag ? Plus(Pos, Times(Ahead(Wh), DragM)) : Pos;
		D.DragTo.Z = GroundZ(D.DragTo.X, D.DragTo.Y);
		D.Rise = D.bHard ? D.Lie + 1.5 : std::max(D.FallS + D.Lie, D.SwapS + D.DragS + 0.4);
		D.UpS = D.bHard ? 1.4 : 1.0;
		D.bLimp = D.bHard;
		D.Pace = D.bLimp ? LimpPace : 1.0;
	}

	size_t Sky::ChutesPlanned() const
	{
		size_t N = 0;
		for (const Plan& P : Plans)
		{
			for (const Down& D : P.Falls) N += D.bChute ? 1u : 0u;
		}
		return N;
	}

	void Sky::Chutes(double T, std::vector<Chute>& Out) const
	{
		Out.clear();
		for (size_t I = 0; I < Plans.size(); ++I)
		{
			const Plan& P = Plans[I];
			const double Tau = (T - P.Tin) * Life;
			for (int Slot = 0; Slot < P.Models; ++Slot)
			{
				const Down& D = P.Falls[static_cast<size_t>(Slot)];
				if (!D.bChute || D.Drop.empty() || Tau < D.Out) continue;
				Chute C;
				C.Key = static_cast<int>(I) * 8 + Slot;
				C.Side = P.Side;
				C.Nat = P.Nat;
				C.Landed = D.LandAt;
				C.Heap = D.Heap;
				C.HeapHeading = std::atan2(D.Heap.X - D.LandAt.X, D.Heap.Y - D.LandAt.Y) / Rad;
				for (int K = 0; K < 3; ++K) C.Silk[K] = D.Silk[K];
				C.Gores = D.Gores;
				C.Across = D.Across;
				C.Deep = D.Deep;
				C.Folds = D.Folds;
				C.Wind = D.Wind;
				C.bHard = D.bHard;
				if (Tau < D.Landed)
				{
					auto It = std::upper_bound(D.Drop.begin(), D.Drop.end(), Tau, [](double Tm, const Sample& S) { return Tm < S.T; });
					const Sample& B = It == D.Drop.end() ? D.Drop.back() : *It;
					const Sample& A = It == D.Drop.begin() ? D.Drop.front() : *(It - 1);
					const double K = B.T > A.T ? std::clamp((Tau - A.T) / (B.T - A.T), 0.0, 1.0) : 0.0;
					C.At = Plus(A.At, Times(Plus(B.At, Times(A.At, -1.0)), K));
					C.Vel = Times(Plus(B.At, Times(A.At, -1.0)), 1.0 / std::max(1e-3, B.T - A.T));
					C.Open = A.Pitch + (B.Pitch - A.Pitch) * K;
					C.Heading = A.Heading;
					// (his swing under it once it is full: a long pendulum, about 4 s a swing for a man 4.1 m under it, from
					// 14 degrees settling to a gentle 5 that never quite dies, the air moving him: the art director)
					const double Since = Tau - D.Opened;
					const double Most = SwingLast * (1.0 + 0.2 * std::sin(2.0 * Pi * Since / 17.0)) + (SwingFirst - SwingLast) * std::exp(-Since / SwingSettle);
					C.Swing = Since > 0.0 ? D.SwingSide * Most * std::sin(2.0 * Pi * Since / SwingS) : 0.0;
					Out.push_back(C);
					continue;
				}
				// Down: over flat as he lands, in his chute's mesh, and swapped for the crowd's man as he lies under his
				// falling canopy; dragged with it in a wind; lying a moment; up onto his knees, his harness worked free,
				// up, running from the enemy (limping after a hard landing), flat again where he goes to ground.
				C.bDown = true;
				C.Open = D.OpenDown;
				C.Slump = std::clamp((Tau - D.Landed) / FallInS, 0.0, 1.0);
				const double S = Tau - D.Landed;
				const double Kneel = D.Rise + D.UpS, Up = Kneel + D.ReleaseS, Off = Up + StandS, Gone = Off + D.RunS;
				const double Drag = D.DragS > 0.0 ? Smooth((S - D.SwapS) / D.DragS) : 0.0;
				const V3 Lies = Plus(D.LandAt, Times(Plus(D.DragTo, Times(D.LandAt, -1.0)), Drag));
				C.Dragged = std::hypot(Lies.X - D.LandAt.X, Lies.Y - D.LandAt.Y);
				C.Landed = { Lies.X, Lies.Y, GroundAt ? GroundAt(Lies.X, Lies.Y) : Lies.Z };
				C.Heap = Plus(C.Landed, Plus(D.Heap, Times(D.LandAt, -1.0)));
				C.Heap.Z = GroundAt ? GroundAt(C.Heap.X, C.Heap.Y) : D.Heap.Z;
				C.At = C.Landed;
				C.Heading = D.Face;
				C.Face = D.Face;
				C.Rate = 0.0;
				C.Blend = BlendS;
				C.bInMesh = S < D.SwapS;
				C.Fallen = Smooth(S / D.FallS);
				if (C.bInMesh)
				{
					Out.push_back(C);
					continue;
				}
				const V3 UpAt = D.DragTo;
				if (S < D.Rise)
				{
					C.Clip = "go_prone";
					C.Time = ProneS;
				}
				else if (S < Kneel)
				{
					// (up onto his knees: go_prone played back from lying to where he is on them)
					C.Clip = "go_prone";
					C.Rate = -(ProneS - KneeT) / D.UpS;
					C.Time = ProneS + C.Rate * (S - D.Rise);
				}
				else if (S < Up)
				{
					// (his harness worked free, kneeling, his hands at his chest: kneel_down's last pose, which is stand_up's
					// first, so that he rises from it without a slide)
					C.Clip = "kneel_down";
					C.Time = KneelS;
					C.Blend = KneelBlendS;
				}
				else if (S < Off)
				{
					C.Clip = "stand_up";
					C.Time = S - Up;
					C.Rate = 1.0;
					// (turning to the way he will run as he gets up)
					double Turn = std::fmod(D.RunHeading - D.Face, 360.0);
					if (Turn > 180.0) Turn -= 360.0;
					if (Turn < -180.0) Turn += 360.0;
					C.Heading = D.Face + Turn * Smooth((S - Up) / StandS);
				}
				else if (S < Gone)
				{
					const V3 Way = Plus(D.RunTo, Times(UpAt, -1.0));
					const double Far = Length(Way);
					double Along = 0.0, Speed = 0.0;
					C.Clip = "run";
					if (D.bLimp)
					{
						// (limping: his run clip at a share of its pace, hurried off his hurt leg (the right, whose foot is down
						// at the loop's start) and lingering on his good one, his feet keeping to its stride)
						const double Loops = (S - Off) * D.Pace / RunLoop, Whole = std::floor(Loops), F = Loops - Whole;
						const double Warped = F + Hitch / (2.0 * Pi) * std::sin(2.0 * Pi * F), Ends = std::max(0.5, D.RunS * D.Pace / RunLoop);
						Along = std::min(1.0, (Whole + Warped) / Ends);
						C.Rate = D.Pace * (1.0 + Hitch * std::cos(2.0 * Pi * F));
						C.Time = Warped * RunLoop;
						Speed = Far / Ends * C.Rate / RunLoop;
					}
					else
					{
						Along = (S - Off) / std::max(1e-3, D.RunS);
						C.Time = std::fmod(Along * Far / RunStride * RunLoop, RunLoop);
						C.Rate = Far / RunStride * RunLoop / std::max(1e-3, D.RunS);
						Speed = Far / std::max(1e-3, D.RunS);
					}
					C.At = Plus(UpAt, Times(Way, Along));
					C.At.Z = GroundAt ? GroundAt(C.At.X, C.At.Y) : D.RunTo.Z;
					C.Heading = D.RunHeading;
					C.Vel = Times(Ahead(D.RunHeading), Speed);
				}
				else
				{
					C.Clip = "go_prone";
					C.Time = std::min(S - Gone, ProneS);
					C.Rate = S - Gone < ProneS ? 1.0 : 0.0;
					C.At = D.RunTo;
					C.Heading = D.RunHeading;
				}
				Out.push_back(C);
			}
		}
	}

	void SilkMesh::Clear()
	{
		Points.clear();
		Facings.clear();
		Colours.clear();
		Triangles.clear();
		Outward.clear();
		Seams.clear();
		Across.clear();
		Over.clear();
		MidLow.clear();
		HarnessLow = -1e9;
		Kept = LyingKept();
	}

	namespace
	{
		/** A canopy's radius and depth as it fills (Open 0, a streamer of silk 2.6 m long; 1, full), and how far under
		 * the mesh's origin its skirt is (its lines not yet taut while it is a streamer). */
		void Filling(const Chute& C, double Open, double& Radius, double& Deep, double& Under)
		{
			const double K = Smooth(Open / 0.3), O = std::clamp(Open, 0.0, 1.0);
			Radius = C.Across * 0.5 * (0.06 + 0.94 * std::pow(O, 1.4));
			Deep = 2.6 + (C.Deep - 2.6) * K;
			Under = (LinesLong - 0.8) * (1.0 - K);
		}

		V3 Minus(const V3& A, const V3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
		V3 Cross(const V3& A, const V3& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		V3 Unit(const V3& A, const V3& Else)
		{
			const double Long = Length(A);
			return Long > 1e-12 ? Times(A, 1.0 / Long) : Else;
		}

		/** How far out a canopy's widest is, its gores' billow and all, for a radius of 1 (so that a canopy's across is
		 * the art director's across at its widest, not its seams'): its ring at its widest, a gore's middle. */
		double Widest() { return 1.035; }

		/**
		 * Where its hem is on a gore (Gore's Phi there); how its skirt draws in under its widest (by the Curl-th power of
		 * the way down it: under 2, so that it curves in from its widest on, no straight drop before it does; a review
		 * of 24 November: at 2.5 it had dropped straight 0.8 m and bevelled in at the bottom); and how round its crown
		 * is (Crown: 2 is a quarter-ellipse, flat-topped on a canopy half as deep as it is wide, a drum; less, rounder).
		 */
		double HemPhi() { return std::acos(HemIn); }
		constexpr double Curl = 1.5, Crown = 1.5;

		/**
		 * A point of a canopy's gore for a widest radius of 1 and a depth of 1 (the art director's verdict of 24 November
		 * 14:34Z): Phi from -HemPhi at its hem, through 0 at its widest, WidestDown under its crown, to Pi / 2 at its
		 * crown. Over its widest its crown, a dome rounder than a quarter-ellipse (Crown); under it its skirt, curving in
		 * all the way down to a hem HemIn as wide, its silk there heading in toward its lines (a round canopy's hem
		 * pulled in by them, not dropped straight). How far out (R) and how high over its hem (Z), and its outer face's
		 * facing out and up for a canopy A wide and D deep: (Nr / A, Nz / D).
		 */
		void Gore(double Phi, double& R, double& Z, double& Nr, double& Nz)
		{
			if (Phi >= 0.0)
			{
				const double Cs = std::max(0.0, std::cos(Phi)), Sn = std::sin(Phi);
				R = std::pow(Cs, 2.0 / Crown);
				Z = 1.0 - WidestDown + WidestDown * Sn;
				Nr = WidestDown * Cs;
				Nz = 2.0 / Crown * std::pow(Cs, 2.0 / Crown - 1.0) * Sn;
				return;
			}
			const double T = std::min(1.0, -Phi / HemPhi()), Low = 1.0 - WidestDown;
			R = 1.0 - (1.0 - HemIn) * std::pow(T, Curl);
			Z = Low * (1.0 - T);
			Nr = Low;
			Nz = -(1.0 - HemIn) * Curl * std::pow(T, Curl - 1.0);
		}

		/** A gore's four columns (its seam, beside it, its middle, beside the next): the way round its Col-th point is
		 * (radians from x), and how far its silk billows out there for a radius of 1 at its widest. */
		double Round(int Col, int Cols)
		{
			const int Q = Col % 4;
			return 2.0 * Pi * (Col / 4 + (Q == 0 ? 0.0 : Q == 1 ? 0.2 : Q == 2 ? 0.5 : 0.8)) / (Cols / 4);
		}
		double Billow(int Col) { return Col % 4 == 2 ? 0.035 : Col % 4 == 0 ? 0.0 : 0.02; }
		/** Its shade at a point: its seams faint darker lines (the silk doubled there, which is what shows most against
		 * the sun), its hem a little darker too. */
		double Hemmed(int Ring, int Col) { return (Col % 4 == 0 ? 0.88 : 1.0) * (Ring == 0 ? 0.93 : 1.0); }

		/**
		 * The rest of a canopy's silk once its outer face's points are in Out (Ringed rings of Cols points, its hem
		 * first, then its crown; their facings and colours): its inner face, the same points facing the other way, so
		 * that it is seen from under it; each face's triangles, each facing out of the silk on its own side; and its
		 * skirt's seams.
		 */
		void Sew(SilkMesh& Out, int Ringed, int Cols)
		{
			const int Each = Ringed * Cols + 1;
			const size_t Half = static_cast<size_t>(Each);
			Out.Points.resize(2 * Half);
			Out.Facings.resize(2 * Half);
			Out.Colours.resize(6 * Half);
			for (size_t K = 0; K < Half; ++K)
			{
				Out.Points[Half + K] = Out.Points[K];
				Out.Facings[Half + K] = Times(Out.Facings[K], -1.0);
			}
			std::copy(Out.Colours.begin(), Out.Colours.begin() + static_cast<std::ptrdiff_t>(3 * Half), Out.Colours.begin() + static_cast<std::ptrdiff_t>(3 * Half));
			// (each face's triangles, wound the same way, the inner face's facing its outer's turned round: written in place)
			const size_t Tris = static_cast<size_t>(2 * (Ringed - 1) * Cols + Cols), T0 = Out.Triangles.size(), O0 = Out.Outward.size();
			Out.Triangles.resize(T0 + 6 * Tris);
			Out.Outward.resize(O0 + 2 * Tris);
			int* Tr = Out.Triangles.data() + T0;
			V3* Ow = Out.Outward.data() + O0;
			const V3* Fc = Out.Facings.data();
			const auto Tri = [&Tr, &Ow, Fc](int A, int B, int C2)
			{
				*Tr++ = A;
				*Tr++ = B;
				*Tr++ = C2;
				const V3 Sum = Plus(Plus(Fc[A], Fc[B]), Fc[C2]);
				*Ow++ = Times(Sum, 1.0 / std::max(1e-9, Length(Sum)));
			};
			for (int R = 0; R + 1 < Ringed; ++R)
			{
				for (int Col = 0; Col < Cols; ++Col)
				{
					const int A = R * Cols + Col, B = R * Cols + (Col + 1 == Cols ? 0 : Col + 1);
					Tri(A, B, B + Cols);
					Tri(A, B + Cols, A + Cols);
				}
			}
			for (int Col = 0; Col < Cols; ++Col)
			{
				Tri((Ringed - 1) * Cols + Col, (Ringed - 1) * Cols + (Col + 1 == Cols ? 0 : Col + 1), Ringed * Cols);
			}
			// (its inner face's the same, on its own points, each facing its outer's turned round)
			const int* Outer = Out.Triangles.data() + T0;
			const V3* OuterOw = Out.Outward.data() + O0;
			for (size_t T = 0; T < 3 * Tris; ++T) *Tr++ = Outer[T] + Each;
			for (size_t T = 0; T < Tris; ++T) *Ow++ = Times(OuterOw[T], -1.0);
			for (int Col = 0; Col < Cols; Col += 4) Out.Seams.push_back(Col);
		}

		/**
		 * A canopy in the air (the art director's sixth note, 24 to 28 panels, their seams faint; and her verdict of 24
		 * November 14:34Z, its hem pulled in): its skirt's middle at Base, Rx along x and Ry across (radii at its widest),
		 * Deep from its skirt to its crown, on Gore's curve. Each gore billows out a little between its seams. Its nine
		 * rings: its hem, three as its skirt curves in, its widest, and four up to its crown.
		 */
		void Dome(SilkMesh& Out, const V3& Base, double Rx, double Ry, double Deep, int Gores, const double Silk[3])
		{
			const double Rings[] = { -HemPhi(), -0.8 * HemPhi(), -0.55 * HemPhi(), -0.3 * HemPhi(), 0.0, 0.4, 0.8, 1.15, 1.42 };
			constexpr int Ringed = 9;
			const int Cols = std::max(8, Gores) * 4;
			const double Dz = std::max(0.05, Deep), Ax = std::max(0.02, Rx), Ay = std::max(0.02, Ry);
			for (int R = 0; R < Ringed; ++R)
			{
				double Gr = 0.0, Gz = 0.0, Nr = 0.0, Nz = 0.0;
				Gore(Rings[R], Gr, Gz, Nr, Nz);
				for (int Col = 0; Col < Cols; ++Col)
				{
					const double Theta = Round(Col, Cols), Out1 = Gr * (1.0 + Billow(Col) * Gr) / Widest();
					Out.Points.push_back({ Base.X + Ax * Out1 * std::cos(Theta), Base.Y + Ay * Out1 * std::sin(Theta), Base.Z + Dz * Gz });
					Out.Facings.push_back(Unit({ Nr * std::cos(Theta) / Ax, Nr * std::sin(Theta) / Ay, Nz / Dz }, { 0.0, 0.0, 1.0 }));
					for (int K = 0; K < 3; ++K) Out.Colours.push_back(Silk[K] * Hemmed(R, Col));
				}
			}
			Out.Points.push_back({ Base.X, Base.Y, Base.Z + Dz });
			Out.Facings.push_back({ 0.0, 0.0, 1.0 });
			for (int K = 0; K < 3; ++K) Out.Colours.push_back(Silk[K]);
			Sew(Out, Ringed, Cols);
		}

		/**
		 * How a pilot's canopy lies once down (the art director's verdict of 24 November 14:34Z: "Stretch it 2-3x
		 * downwind in loose folds, the far end lifting a little, lines running back to him"), drawn from his own number
		 * (Chute::Folds) and the wind: its length along the wind (a canopy 5 to 5.5 m across lies 6.3 to 8.5 m long, a
		 * stronger wind stretching it), its width (2.55 to 2.9 times shorter: its edges' folds below widen it), how far
		 * past his harness its near end is gathered (2 to 3.5 m: its 4.1 m lines a little slack, less so in a stronger
		 * wind), how high its far end stands on the air it still holds (0.3 to 0.62 m, more in a stronger wind) and how
		 * much that swells it; its axis's bend and where its far end lies across; its four or five long folds, each along
		 * it from near its near end to some way on, its place across, its drift and wander across as it goes, its width
		 * and its height (one big fold and the rest smaller, in no order); the gaps between them (graded, the widest 3.5
		 * times the narrowest, three times with five, the wider where two folds need more room, and all spread together
		 * where need be so that no two run into one: a review of 24 November found the narrowest gaps held up to a floor
		 * alone, which left half of them alike); and each edge's waver and swell. (Two creases across it were drawn here
		 * once: their dice are thrown still, so that the rest fall as they did.)
		 *
		 * And (the art director's note of 24 November 17:40Z: from above both pilots' silk had read as "a smooth cocoon or
		 * a gourd", an even outline and only lengthwise ridges), on dice of their own: three or four folds across it,
		 * where it bunched as it collapsed, each on a ring of its own from 0.15 to 0.64 along it (no two side by side),
		 * graded in height and dealt out in no order, each highest somewhere across it and fading either way (most across
		 * part of its width), slanted and bowed across it, and bunched up toward the ring before it or the one after
		 * (away from a fold two rings on); its outline uneven: one side wider than the other, each ring's edge jagged a
		 * little either side (each like its neighbours a little), and its gores folded at its edges, one or two flipped
		 * over and lying flat out beyond its wide side and one tucked under its other side, from its fourth ring to its
		 * eighth; two or three of its hem's gores flipped back over its top at its near end, their fold pulled back past
		 * its hem toward him, a roll of silk on the ground under his lines, so that the lines to them and beside them run
		 * across it (as far as its gathering lets them: no more than about 0.3 m past their seams' last 10 cm), and a gore
		 * or two tucked under on the other side of its top's middle; the rest of its hem in a wave round its skirt, back
		 * toward him in some gores and on in others; its gores wandering across it as they go; and its lifted far end
		 * slumped to one side (HeapShape's: away from its wider side), its crest off its middle, its top leaning over that
		 * way, and buckled on the other.
		 */
		constexpr int LyingRings = 13;
		struct Lay
		{
			double Long = 7.0, Wide = 2.8, Gap = 2.5, Lift = 0.5, Bulb = 0.0, Bend = 0.0, FarSide = 0.0;
			int Ridges = 4;
			double At[5] = {}, Drift[5] = {}, Wave[5] = {}, Phase[5] = {}, Width[5] = {}, High[5] = {}, From[5] = {}, To[5] = {};
			double EdgeWave[2] = {}, EdgePhase[2] = {}, EdgeSwell[2] = {};
			double Loose = 0.0;
			// (her note of 17:40Z) Its folds across it, ring by ring (none: 0 high): how high (metres) at its highest, where
			// across it that is (a share of its half-width from its middle, to its right more than nothing) and how far
			// either way it reaches before it fades (a share of its half-width), slanted and bowed how far (metres along it
			// at its edge), and bunched toward the ring before it (less than nothing) or the one after (a share of the gap),
			// its crest rounded over onto that ring and the ring on its other side let down a little (HeapShape)
			int Crosses = 0;
			double Cross[LyingRings] = {}, CrossAt[LyingRings] = {}, CrossReach[LyingRings] = {}, CrossSkew[LyingRings] = {}, CrossBow[LyingRings] = {},
				CrossBunch[LyingRings] = {};
			// Its outline: by how much its wider side is wider (a share of its half-width; which side is HeapShape's: its
			// axis's bend's outer side, so that it is the wider about the wind line), each ring's jag either side (-1 to 1,
			// each ring's a little like its neighbours'); its gores folded at its edges (on which side, 0 its wider; the ring
			// it is about, its fourth to its eighth, whole on it and the next and rounded off over the one before and the
			// one after those; how far out, flipped over and lying flat, more than nothing; in, tucked under, less: metres)
			int Lobes = 0;
			double Wider = 0.0, Jag[2][LyingRings] = {};
			int LobeSide[3] = {}, LobeRing[3] = {};
			double LobeOut[3] = {};
			// Its skirt's gores flipped over at its near end: round its skirt where (radians from the way it falls), how
			// far either way flipped whole (radians); their fold pulled back toward him past where their hem was (metres
			// at most: HeapShape's gathering may let it less), and their hem drawn in toward its middle (a share of how far
			// off its middle it was, taken from 0.7 of that: what is left of their spread across); and on the other side of
			// its top's middle a gore or two tucked under (where round it, how far either way whole, and how far on under it
			// at most)
			double FlipAt = 3.14, FlipCore = 0.3, FlipBack = 0.35, FlipOver = 0.5, TuckAt = 3.14, TuckCore = 0.1, TuckOn = 0.12;
			// The rest of its hem: how far each gore's hem lies back toward him or on from where it is gathered (metres), in
			// a wave round its skirt (waves round, and where), so that its near edge is uneven
			double HemWave = 0.0, HemWaves = 2.5, HemPhase = 0.0;
			// Its far end's slump (each how much: HeapShape gives it its side, away from its wider side): its crest's place
			// across (a share of its half-width), how far its top leans over that way and how far the whole of it lies over
			// that way (metres), and how deep the buckle across its lifted top on the side it slumps from (a share of its
			// lift)
			double Lean = 0.0, LeanOver = 0.0, Kink = 0.0, Buckle = 0.0;
			// Its gores' wander across it as it goes (so that its seams and its long folds' crests do not run straight
			// and even from end to end, as a melon's ribs): in two waves round it, each how far (metres), how many times
			// along it and from where, and from where round it
			double Wander[2] = {}, WanderAlong[2] = {}, WanderFrom[2] = {}, WanderRound[2] = {};
			// (her note of 17:40Z, (3), after a second look: its lines had crossed only the flap at its near end) How many of
			// its lines lie across it, two to four; for each, its pick among the seams that may (0 to 1), how much farther
			// along than it must it crosses to (0 to 1: half a ring), its curl across as it goes (either way, a share of the
			// way across) and its bow along (rings), its wiggle's phase, on which ring it comes onto its top (0 to 1: its
			// first to its second), from its seam at its top's middle, from which edge, and, should it come on over its
			// near end, how far across (0 to 1)
			int Strewn = 0;
			double StrewnPick[4] = {}, StrewnFar[4] = {}, StrewnCurl[4] = {}, StrewnBow[4] = {}, StrewnWiggle[4] = {}, StrewnRing[4] = {}, StrewnWay[4] = {}, StrewnFront[4] = {};
			// (her note of 17:40Z, (2), after a second look: its far end had been a neat dome, its seams running cleanly to
			// a point at its tip) Its far end broken up: its vent over toward the side it slumps to (a share of half its
			// width); a crease across its dome on its eleventh ring, how deep (a share of its air there) and how slanted
			// (metres along it at its edges, either way); the edge of its narrow side folded back under it, from where
			// across (a share of its half-width) and how that slants along it; and its seams kinked over it, ring by ring
			// (how far across, metres, and each ring's two phases)
			double Vent = 0.3, CreaseDeep = 0.6, CreaseSlant = 0.3, UnderAt = 0.5, UnderSlant = 0.0, Kinked = 0.07, KinkPhase[4][2] = {};
		};
		Lay LayOf(uint32_t Folds, double Across, double Wind)
		{
			Lay L;
			Dice D(Folds);
			const double Blow = std::clamp(Wind / 6.0, 0.0, 1.0);
			L.Long = Across * (1.25 + 0.15 * Blow + D.U(0.0, 0.15));
			L.Wide = L.Long / D.U(2.55, 2.9);
			L.Gap = 2.0 + 0.5 * Blow + D.U(0.0, 1.0);
			L.Lift = std::min(0.62, 0.3 + 0.25 * Blow + D.U(0.0, 0.12));
			L.Bulb = D.U(0.0, 0.12);
			L.Bend = D.U(-0.35, 0.35);
			L.FarSide = D.U(-0.3, 0.3);
			L.Ridges = D.U() < 0.5 ? 4 : 5;
			// (their heights graded and dealt out across it in no order, a big fold broader than a small one; across it, a
			// share of its half-width, from its middle)
			int Order[5] = { 0, 1, 2, 3, 4 };
			constexpr double Heights[5] = { 0.27, 0.18, 0.13, 0.1, 0.085 };
			for (int K = L.Ridges - 1; K > 0; --K) std::swap(Order[K], Order[std::min(K, static_cast<int>(D.U() * (K + 1)))]);
			for (int K = 0; K < L.Ridges; ++K)
			{
				L.High[K] = Heights[Order[K]] * D.U(0.9, 1.1);
				L.Width[K] = (0.03 + 0.17 * L.High[K]) * D.U(0.85, 1.15);
				L.Drift[K] = D.U(-0.1, 0.1);
				L.Wave[K] = D.U(0.6, 1.6);
				L.Phase[K] = D.U();
				L.From[K] = D.U(0.0, 0.08);
				L.To[K] = D.U(0.55, 1.0);
			}
			// (the gaps between them graded, the widest 3.5 times the narrowest (three times with five), the wider going
			// where two folds need more room to stand apart: 1.4 times their widths, and the bigger one's flank half-way
			// between them under a third of the smaller's height, and a little for their wander. All spread together,
			// never one alone, to span Wide in all or more until each two stand apart, so that the grading holds: a review
			// of 24 November found the narrowest held up to a floor alone, which left half of them alike)
			double Need[4] = {}, Gaps[4] = {}, Span = 0.0;
			int ByNeed[4] = { 0, 1, 2, 3 };
			for (int K = 0; K + 1 < L.Ridges; ++K)
			{
				const int Big = L.High[K] >= L.High[K + 1] ? K : K + 1, Small = 2 * K + 1 - Big;
				const double Flank = 2.0 * L.Width[Big] * std::sqrt(std::log(std::max(1.0, L.High[Big] / (0.3 * L.High[Small]))));
				Need[K] = 0.08 + std::max(1.4 * (L.Width[K] + L.Width[K + 1]), Flank);
			}
			std::stable_sort(ByNeed, ByNeed + (L.Ridges - 1), [&Need](int A, int B) { return Need[A] < Need[B]; });
			const double Most = L.Ridges == 5 ? 2.0 : 2.5;
			for (int R = 0; R + 1 < L.Ridges; ++R)
			{
				const int K = ByNeed[R];
				Gaps[K] = (1.0 + Most * R / (L.Ridges - 2.0)) * D.U(0.92, 1.08);
				Span += Gaps[K];
			}
			const double Wide = D.U(1.0, 1.25);
			double Spread = Wide / Span;
			for (int K = 0; K + 1 < L.Ridges; ++K) Spread = std::max(Spread, Need[K] / Gaps[K]);
			Spread = std::min(Spread, 1.6 / Span);
			double Across2 = 0.0;
			for (int K = 0; K < L.Ridges; ++K)
			{
				L.At[K] = Across2;
				if (K + 1 < L.Ridges) Across2 += Gaps[K] * Spread;
			}
			const double Shift = -0.5 * L.At[L.Ridges - 1] + D.U(-0.06, 0.06);
			for (int K = 0; K < L.Ridges; ++K) L.At[K] += Shift;
			for (int K = 0; K < 2; ++K)
			{
				L.EdgeWave[K] = D.U(0.8, 2.0);
				L.EdgePhase[K] = D.U();
				L.EdgeSwell[K] = D.U(-0.08, 0.08);
				// (two creases across it were drawn here: its folds across it are its own now, below, but their dice are
				// thrown still, so that the rest fall as they did)
				D.U();
				D.U();
			}
			L.Loose = D.U();
			// (her note of 24 November 17:40Z, on dice of their own, the same seed's: so that all the above fall as they did,
			// and each pilot's heap is his own, not another's mirrored)
			Dice E(static_cast<uint64_t>(Folds) | (0xC47E7ull << 32));
			// (its folds across it: three or four of the rings from 0.15 to 0.64 along it, no two side by side; their
			// heights graded and dealt out in no order; each highest somewhere across it and fading either way, some all
			// the way across and most part of it; slanted, bowed, and bunched up toward one side)
			const int Want = E.U() < 0.45 ? 3 : 4;
			int Cand[7] = { 2, 3, 4, 5, 6, 7, 8 };
			for (int K = 6; K > 0; --K) std::swap(Cand[K], Cand[std::min(K, static_cast<int>(E.U() * (K + 1)))]);
			bool bTaken[LyingRings] = {};
			for (int K = 0; K < 7 && L.Crosses < Want; ++K)
			{
				const int R = Cand[K];
				if (bTaken[R - 1] || bTaken[R + 1]) continue;
				bTaken[R] = true;
				++L.Crosses;
			}
			int Graded[4] = { 0, 1, 2, 3 };
			for (int K = L.Crosses - 1; K > 0; --K) std::swap(Graded[K], Graded[std::min(K, static_cast<int>(E.U() * (K + 1)))]);
			constexpr double CrossHeights[4] = { 0.23, 0.19, 0.155, 0.125 };
			for (int R = 0, N = 0; R < LyingRings; ++R)
			{
				if (!bTaken[R]) continue;
				L.Cross[R] = CrossHeights[Graded[N++]] * E.U(0.9, 1.1);
				L.CrossAt[R] = E.U(-0.6, 0.6);
				L.CrossReach[R] = E.U(0.7, 1.3);
				L.CrossSkew[R] = E.U(-0.18, 0.18);
				const double BowWay = E.U() < 0.5 ? -1.0 : 1.0;
				L.CrossBow[R] = BowWay * E.U(0.06, 0.14);
				// (bunched toward one side, away from a fold two rings on, so that the two do not run together into one)
				double Toward = E.U() < 0.5 ? -1.0 : 1.0;
				const bool bBefore = R >= 2 && bTaken[R - 2], bAfter = R + 2 < LyingRings && bTaken[R + 2];
				if (bBefore != bAfter) Toward = bAfter ? -1.0 : 1.0;
				L.CrossBunch[R] = Toward * E.U(0.3, 0.45);
			}
			// (its outline: its wider side wider, each ring's jag (each a little like its neighbours', so that its edge
			// wavers rather than zigzags), one or two gores flipped over and lying flat on its wide side, about three
			// rings apart, and one tucked under on the other; all from its third ring to its eighth, none on its far end,
			// whose slump is its own)
			L.Wider = E.U(0.07, 0.11);
			double Raw[2][LyingRings] = {};
			for (int S = 0; S < 2; ++S)
				for (int R = 0; R < LyingRings; ++R) Raw[S][R] = E.U(-1.0, 1.0);
			for (int S = 0; S < 2; ++S)
				for (int R = 0; R < LyingRings; ++R) L.Jag[S][R] = 0.6 * Raw[S][R] + 0.2 * (Raw[S][std::max(0, R - 1)] + Raw[S][std::min(LyingRings - 1, R + 1)]);
			const int Flips = E.U() < 0.5 ? 1 : 2;
			const int FirstLobe = 3 + static_cast<int>(E.U() * (Flips == 1 ? 5.0 : 2.0));
			for (int K = 0; K < Flips; ++K)
			{
				L.LobeSide[K] = 0;
				L.LobeRing[K] = K == 0 ? FirstLobe : std::min(7, FirstLobe + 3 + static_cast<int>(E.U() * 2.0));
				L.LobeOut[K] = E.U(0.3, 0.45);
			}
			L.LobeSide[Flips] = 1;
			L.LobeRing[Flips] = 3 + static_cast<int>(E.U() * 5.0);
			L.LobeOut[Flips] = -E.U(0.22, 0.32);
			L.Lobes = Flips + 1;
			// (its skirt's gores flipped over at its near end: on one side of its top's middle, two or three gores whole,
			// their fold pulled back toward him and their hem over toward its other side; a gore or two tucked under on
			// the other side; and the rest of its hem's waver)
			const double FlipWay = E.U() < 0.5 ? -1.0 : 1.0;
			L.FlipAt = Pi + FlipWay * E.U(0.35, 0.7);
			L.FlipCore = E.U(0.24, 0.32);
			L.FlipBack = E.U(0.38, 0.46);
			L.FlipOver = E.U(0.0, 0.2);
			L.TuckAt = Pi - FlipWay * E.U(0.3, 0.6);
			L.TuckCore = E.U(0.08, 0.16);
			L.TuckOn = E.U(0.1, 0.16);
			L.HemWave = E.U(0.05, 0.09);
			L.HemWaves = E.U(3.0, 5.0);
			L.HemPhase = E.U(0.0, 2.0 * Pi);
			// (its far end's slump to one side)
			L.Lean = E.U(0.42, 0.55);
			L.LeanOver = E.U(0.25, 0.38);
			L.Kink = E.U(0.04, 0.1);
			L.Buckle = E.U(0.4, 0.6);
			// (its gores' wander)
			for (int K = 0; K < 2; ++K)
			{
				L.Wander[K] = E.U(0.05, 0.09);
				L.WanderAlong[K] = E.U(0.9, 1.7);
				L.WanderFrom[K] = E.U();
				L.WanderRound[K] = E.U(0.0, 2.0 * Pi);
			}
			// (its lines across it, on dice of their own, so that all the above fall as they did)
			Dice S(static_cast<uint64_t>(Folds) | (0x57AEull << 36));
			L.Strewn = 2;
			if (S.U() < 0.55) ++L.Strewn;
			if (S.U() < 0.3) ++L.Strewn;
			for (int K = 0; K < 4; ++K)
			{
				L.StrewnPick[K] = S.U();
				L.StrewnFar[K] = S.U();
				L.StrewnCurl[K] = S.U(-0.35, 0.35);
				L.StrewnBow[K] = S.U(-0.35, 0.35);
				L.StrewnWiggle[K] = S.U(0.0, 2.0 * Pi);
				L.StrewnRing[K] = S.U();
				L.StrewnWay[K] = S.U();
				L.StrewnFront[K] = S.U();
			}
			// (its far end's breakup, on dice of their own as well)
			Dice F(static_cast<uint64_t>(Folds) | (0xFA2Dull << 36));
			L.Vent = F.U(0.22, 0.38);
			// (its crease letting down a little over half its air there: a review of 24 November 17:40Z, after a second look,
			// had found its far end at eye height a sawtooth, 0.17 m lower 0.9 along it, its inside showing along the crease,
			// where it had let down 80 to 95%; the same draw, so that the rest fall as they did)
			L.CreaseDeep = F.U(0.55, 0.7);
			L.CreaseSlant = F.U(0.22, 0.38);
			if (F.U() < 0.5) L.CreaseSlant = -L.CreaseSlant;
			L.UnderAt = F.U(0.5, 0.66);
			L.UnderSlant = F.U(-0.08, 0.08);
			L.Kinked = F.U(0.045, 0.07);
			for (int K = 0; K < 4; ++K)
			{
				L.KinkPhase[K][0] = F.U(0.0, 2.0 * Pi);
				L.KinkPhase[K][1] = F.U(0.0, 2.0 * Pi);
			}
			return L;
		}

		/**
		 * The ground under a canopy lying (a review of 24 November: laid point by point, its triangles had cut through a
		 * bank, a ditch's sides and a crater's rim between its points), read once a make on a grid Step metres square
		 * (in the canopy's own space, heights from its origin), not once a point twice: each point is laid on it, and each
		 * edge kept over it. At is the grid's height there (between its points, as a flat cell would have it), raised by
		 * how sharply the ground crests about the cell (its points' most bend, the height a crest between two of them
		 * can stand over the cell's flat face: a step or a bank's shoulder narrower than the cell, which half of it had
		 * let the silk cut 8 mm into). Hug, which its silk is laid and kept on, is no higher than that and no higher than
		 * the cell's highest corner raised by half that bend: a crest between two points stands over the higher of them
		 * by at most half its bend (a ridge with straight sides, its peak half-way between them; a round crest by an eighth),
		 * and a step between them stands no higher than the higher. At, the whole bend over its flat face, reads a
		 * hedgerow bank 1 m high 0.74 m higher than it is and the bank 0.8 m high along it 0.8 m higher, and the silk laid
		 * and kept on it had floated up to 2.4 m over the ground round them (a fourth review); its lines are still kept
		 * over At.
		 */
		struct UnderGrid
		{
			double X0 = 0.0, Y0 = 0.0, Step = 0.25, Most = -1e18;
			int Nx = 0, Ny = 0;
			// (and each cell's, at its first corner: its corners' most bend, and its highest corner raised by half that)
			std::vector<double> Z, Crest, CellBend, CellTop;
			template <typename Fn>
			void Read(double MinX, double MinY, double MaxX, double MaxY, const Fn& Field)
			{
				Step = 0.25;
				const double Wx = MaxX - MinX + 2.0 * Step, Wy = MaxY - MinY + 2.0 * Step;
				// (no more than some 2,500 heights a make: coarser over a canopy still spread high in the air)
				while ((Wx / Step + 2.0) * (Wy / Step + 2.0) > 2500.0) Step *= 1.25;
				X0 = MinX - Step;
				Y0 = MinY - Step;
				Nx = static_cast<int>(std::ceil((MaxX + Step - X0) / Step)) + 1;
				Ny = static_cast<int>(std::ceil((MaxY + Step - Y0) / Step)) + 1;
				Z.assign(static_cast<size_t>(Nx * Ny), 0.0);
				Crest.assign(static_cast<size_t>(Nx * Ny), 0.0);
				for (int J = 0; J < Ny; ++J)
					for (int I = 0; I < Nx; ++I) Z[static_cast<size_t>(J * Nx + I)] = Field(X0 + I * Step, Y0 + J * Step);
				for (int J = 0; J < Ny; ++J)
				{
					for (int I = 0; I < Nx; ++I)
					{
						const double Here = Z[static_cast<size_t>(J * Nx + I)];
						double Bend = 0.0;
						if (I > 0 && I + 1 < Nx) Bend = std::max(Bend, 2.0 * Here - Z[static_cast<size_t>(J * Nx + I - 1)] - Z[static_cast<size_t>(J * Nx + I + 1)]);
						if (J > 0 && J + 1 < Ny) Bend = std::max(Bend, 2.0 * Here - Z[static_cast<size_t>((J - 1) * Nx + I)] - Z[static_cast<size_t>((J + 1) * Nx + I)]);
						Crest[static_cast<size_t>(J * Nx + I)] = Bend;
						Most = std::max(Most, Here + Bend);
					}
				}
				CellBend.assign(static_cast<size_t>(Nx * Ny), 0.0);
				CellTop.assign(static_cast<size_t>(Nx * Ny), 0.0);
				const size_t Up = static_cast<size_t>(Nx);
				for (int J = 0; J + 1 < Ny; ++J)
				{
					for (int I = 0; I + 1 < Nx; ++I)
					{
						const size_t K = static_cast<size_t>(J * Nx + I);
						CellBend[K] = std::max(std::max(Crest[K], Crest[K + 1]), std::max(Crest[K + Up], Crest[K + Up + 1]));
						CellTop[K] = std::max(std::max(Z[K], Z[K + 1]), std::max(Z[K + Up], Z[K + Up + 1])) + 0.5 * CellBend[K];
					}
				}
			}
			double At(double X, double Y) const
			{
				if (Nx < 2 || Ny < 2) return 0.0;
				const double U = (X - X0) / Step, V = (Y - Y0) / Step;
				const int I = std::clamp(static_cast<int>(std::floor(U)), 0, Nx - 2), J = std::clamp(static_cast<int>(std::floor(V)), 0, Ny - 2);
				const double A = std::clamp(U - I, 0.0, 1.0), B = std::clamp(V - J, 0.0, 1.0);
				const size_t K = static_cast<size_t>(J * Nx + I), Up = static_cast<size_t>(Nx);
				const double Flat = (Z[K] * (1.0 - A) + Z[K + 1] * A) * (1.0 - B) + (Z[K + Up] * (1.0 - A) + Z[K + Up + 1] * A) * B;
				return Flat + CellBend[K];
			}
			double Hug(double X, double Y) const
			{
				if (Nx < 2 || Ny < 2) return 0.0;
				const double U = (X - X0) / Step, V = (Y - Y0) / Step;
				const int I = std::clamp(static_cast<int>(std::floor(U)), 0, Nx - 2), J = std::clamp(static_cast<int>(std::floor(V)), 0, Ny - 2);
				const double A = std::clamp(U - I, 0.0, 1.0), B = std::clamp(V - J, 0.0, 1.0);
				const size_t K = static_cast<size_t>(J * Nx + I), Up = static_cast<size_t>(Nx);
				const double Flat = (Z[K] * (1.0 - A) + Z[K + 1] * A) * (1.0 - B) + (Z[K + Up] * (1.0 - A) + Z[K + Up + 1] * A) * B;
				return std::min(Flat + CellBend[K], CellTop[K]);
			}
		};

		/**
		 * A point of a canopy's outer face between its points (Pts: Ringed rings of Cols, its hem first), at ring R and
		 * column C round it (both with their fractions), on the very triangle the scene draws there (Sew's two a quad,
		 * split from its first ring's first point to the next ring's next); and (Fc, its points' facings: none, none
		 * given) its facing there, its corners' between them.
		 */
		V3 SurfAt(const std::vector<V3>& Pts, const std::vector<V3>* Fc, int Ringed, int Cols, double R, double C, V3* Facing)
		{
			const int R0 = std::clamp(static_cast<int>(std::floor(R)), 0, Ringed - 2);
			const double A = std::clamp(R - R0, 0.0, 1.0), Cf = std::floor(C), B = std::clamp(C - Cf, 0.0, 1.0);
			int C0 = static_cast<int>(Cf);
			if (C0 < 0 || C0 >= Cols) C0 = ((C0 % Cols) + Cols) % Cols;
			const int C1 = C0 + 1 == Cols ? 0 : C0 + 1;
			const size_t I00 = static_cast<size_t>(R0 * Cols + C0), I01 = static_cast<size_t>(R0 * Cols + C1);
			const size_t I10 = I00 + static_cast<size_t>(Cols), I11 = I01 + static_cast<size_t>(Cols);
			const double W00 = B >= A ? 1.0 - B : 1.0 - A, W01 = B >= A ? B - A : 0.0, W10 = B >= A ? 0.0 : A - B, W11 = B >= A ? A : B;
			const auto Mix = [&](const std::vector<V3>& V)
			{
				return Plus(Plus(Times(V[I00], W00), Times(V[I01], W01)), Plus(Times(V[I10], W10), Times(V[I11], W11)));
			};
			if (Facing) *Facing = Fc ? Unit(Mix(*Fc), { 0.0, 0.0, 1.0 }) : V3{ 0.0, 0.0, 1.0 };
			return Mix(Pts);
		}

		/** The column (with its fraction) at Theta round a canopy of Cols columns (Round's, the other way). */
		double ColOf(double Theta, int Cols)
		{
			const double Seams = Cols / 4, G0 = Theta / (2.0 * Pi) * Seams, G = G0 - std::floor(G0 / Seams) * Seams;
			const double Gore = std::floor(G), F = G - Gore;
			return 4.0 * Gore + (F < 0.2 ? F / 0.2 : F < 0.5 ? 1.0 + (F - 0.2) / 0.3 : F < 0.8 ? 2.0 + (F - 0.5) / 0.3 : 3.0 + (F - 0.8) / 0.2);
		}
	}

	void CanopyShape(const Chute& C, double Open, SilkMesh& Out)
	{
		Out.Clear();
		double Radius = 0.0, Deep = 0.0, Under = 0.0;
		Filling(C, Open, Radius, Deep, Under);
		Dome(Out, { 0.0, 0.0, -Under }, Radius, Radius, Deep, C.Gores, C.Silk);
	}

	double HeapOff(uint32_t Folds, double Across, double Wind, double Face, double Way)
	{
		const Lay L = LayOf(Folds, Across, Wind);
		const V3 H = OverHarness(1.0, ProneEndX, ProneEndY);
		const double Turn = (Face - Way) * Rad;
		return H.X * std::cos(Turn) - H.Y * std::sin(Turn) + L.Gap + 0.5 * L.Long;
	}

	void HeapShape(const Chute& C, double Slump, double Off, SilkMesh& Out, const GroundFn& Ground)
	{
		SilkMesh::LyingKept KeptWas;
		std::swap(KeptWas, Out.Kept);
		Out.Clear();
		const double K = std::clamp(Slump, 0.0, 1.0);
		const Lay L = LayOf(C.Folds, C.Across, C.Wind);
		// As it was as he landed: over him on its lines, its skirt LinesLong over his harness (the air's canopy, on 13
		// rings: five as its skirt curves in, its widest, seven up to its crown; a review of 24 November: on 14 it had
		// been twice the air's points, and it went down to 11; the art director's note of 17:40Z took it to 13, for its
		// folds across it). Along it as it lies (Along: its hem 0, gathered at its near end; its crown 1, at its far end),
		// each ring about as far along as it is up its gore, a little over half a metre apart where it folds across, its
		// last three in its far end's round.
		const double Hem = HemPhi();
		const double Rings[LyingRings] = { -Hem, -0.8 * Hem, -0.58 * Hem, -0.385 * Hem, -0.19 * Hem, 0.0, 0.3, 0.55, 0.79, 1.0, 1.2, 1.36, 1.47 };
		const double Along[LyingRings] = { 0.0, 0.07, 0.15, 0.22, 0.29, 0.36, 0.45, 0.54, 0.64, 0.74, 0.84, 0.92, 0.975 };
		constexpr int Ringed = LyingRings;
		const int Cols = std::max(8, C.Gores) * 4;
		double Radius = 0.0, Deep = 0.0, Under = 0.0;
		Filling(C, C.Open, Radius, Deep, Under);
		const double Skirt = LinesLong + HangFeet - Under, Dz = std::max(0.05, Deep), Ax = std::max(0.02, Radius);
		// (his harness as he lies, where its lines meet him: its near end is gathered Gap past it)
		const V3 H = OverHarness(1.0, ProneEndX, ProneEndY);
		const double Turn = (C.Face - C.HeapHeading) * Rad;
		const double Hx = H.X * std::cos(Turn) - H.Y * std::sin(Turn), Hy = H.X * std::sin(Turn) + H.Y * std::cos(Turn);
		const double Nx = std::max(Off - 0.5 * L.Long, Hx + 1.5), Ny = 0.6 * Hy;
		// Falling in: it loses its air over about the first half (Sag), its crown sinking, its side toward him blown in
		// (Blown: a canopy losing its air in a wind folds in on itself in a crescent, its upwind side over its downwind
		// one), and tips forward on its lines about his harness (Tip, to a quarter turn), so that it falls in downwind of
		// him and down; it goes slack (Slack: its far half first, over the first third), its round section flattening,
		// its side toward him coming down onto the other, and its silk crumpling; it streams out along the wind (Stream),
		// and each point comes down onto the field in its own time (Settled: its near end and its front first, its far
		// end not long after), flapping and billowing along it until it is down (a review of 24 November: it had kept its
		// shape, a smooth torpedo, through the middle of its falling in). Each spread over most of its falling in, so that
		// no part of it moves much faster than the rest (its slump an even clock: Sky::Chutes). Nothing of it comes
		// nearer the ground than Clear (metres: its silk's own loft).
		const double Sag = Smooth(K / 0.5), Tip = 0.5 * Pi * Smooth(K / 0.95), Stream = Smooth(K / 0.65);
		const double Clear = 0.03, Pivot = HangFeet;
		const double TipC = std::cos(Tip), TipS = std::sin(Tip);
		// (what each column round it and each ring along it share, worked out once: a make is 1,457 points a face on 28
		// gores (13 rings of 112 and its crown), and the scene makes it some twenty times as it falls in)
		struct Column
		{
			double Theta, Cs, Sn, Back, Top, Arch, Over, Rim, Flip, Tuck, Bil, Late, S3, C3, S5, C5, S7, C7, Loose, Round2, Round3, Kink[4];
			size_t Edge;    // (as wide as a double: no padding, so that a column kept as doubles is all its own bytes)
		};
		struct Ring
		{
			double U, Gr, Gz, Blown, Slack, Billow, X, Fore, Axis, Half[2], Rim[2], Bunch, Loose, Pocket, End, Cross, CrossAt, CrossReach, Skew, Bow, Bunched,
				Buckle, Wander2, Wander3, A1S, A1C, A2S, A2C, A5S, A5C;
			double Fold[5], Mid[5];
			double Broken, Vent, Crease, Creased, Under, UnderAt, KinkC[2], KinkS[2];
		};
		// (its axis as it lies: from his lines' end of it, its bend, where its far end lies across and its slump; and its
		// wider side, the outer side of that bend over its middle, so that about the wind line through where its lines
		// gather it is wider on that side still (a review of 24 November: drawn either way, about that line one side had
		// come out no wider than the other in half of them); and its far end slumped the other way, so that it is uneven
		// about the line from where its lines gather to its tip as well, not only aslant)
		double Bowed = 0.0;
		for (int R = 3; R <= 9; ++R) Bowed += Ny * (1.0 - Smooth(Along[R] / 0.5)) + L.FarSide * Along[R] + L.Bend * std::sin(Pi * Along[R]) - Ny;
		const int WideSide = Bowed >= 0.0 ? 0 : 1;
		const double LeanWay = WideSide == 0 ? -1.0 : 1.0, Lean = LeanWay * L.Lean, LeanOver = LeanWay * L.LeanOver, Kink = LeanWay * L.Kink;
		// (and bowed that way 0.12 m over its middle at the least, so that one lying all but straight along its wind
		// line is still wider one side of it)
		const double Bows = -LeanWay * std::max(0.0, 0.12 - std::fabs(Bowed) / 7.0);
		const auto AxisAt = [&L, Ny, Kink, Bows](double U)
		{
			const double Pocket = Smooth((U - 0.5) / 0.3);
			return Ny * (1.0 - Smooth(U / 0.5)) + L.FarSide * U + (L.Bend + Bows) * std::sin(Pi * U) + Kink * Pocket * Pocket;
		};
		// (as it lies, and each column round it, worked out at its last make for the same chute lying the same way, kept:
		// none of it changes as it falls in)
		const double KeyNow[] = { static_cast<double>(C.Folds), static_cast<double>(Cols), C.Across, C.Deep, C.Wind, C.Open, Off, H.X, H.Y, H.Z, Hx, Hy };
		const size_t Each0 = static_cast<size_t>(Ringed * Cols + 1);
		static_assert(std::is_trivially_copyable_v<Column> && sizeof(Column) % sizeof(double) == 0 && sizeof(size_t) == sizeof(double),
			"a column kept as doubles, with no padding");
		constexpr size_t PerColumn = sizeof(Column) / sizeof(double);
		const bool bKept = KeptWas.Key.size() == std::size(KeyNow) && std::equal(KeyNow, KeyNow + std::size(KeyNow), KeptWas.Key.begin())
			&& KeptWas.Lies.size() == Each0 && KeptWas.Crossing.size() == Each0 && KeptWas.Columns.size() == PerColumn * static_cast<size_t>(Cols);
		// (its far end's crest across, a share of its half-width, to its right more than nothing: Lean)
		std::vector<Column> Cl(static_cast<size_t>(Cols));
		if (bKept) std::memcpy(static_cast<void*>(Cl.data()), KeptWas.Columns.data(), sizeof(Column) * Cl.size());
		for (int Col = 0; Col < Cols && !bKept; ++Col)
		{
			Column& Q = Cl[static_cast<size_t>(Col)];
			Q.Theta = Round(Col, Cols);
			Q.Cs = std::cos(Q.Theta);
			Q.Sn = std::sin(Q.Theta);
			Q.Back = std::max(0.0, -Q.Cs);
			Q.Top = 0.5 * (1.0 - Q.Cs);
			// (its far end's air: across it, its crest at Lean, round as it was (|cos|) were Lean none; and how far its top
			// leans over there, a parabola's, so that its edges stay its edges)
			const double Across = (Q.Sn - Lean) / (Q.Sn >= Lean ? 1.0 - Lean : 1.0 + Lean);
			Q.Over = std::max(0.0, 1.0 - Across * Across);
			Q.Arch = std::sqrt(Q.Over);
			// (its edges' share of their folds and jags: none within 0.88 of its half-width of its middle, so that its long
			// folds lie where they were laid, all at its edge)
			Q.Rim = (Q.Sn >= 0.0 ? 1.0 : -1.0) * Smooth((std::fabs(Q.Sn) - 0.88) / 0.12);
			// (its skirt's gores flipped over: whole within FlipCore of FlipAt round it, none 0.16 rad on; and those tucked
			// under, within TuckCore of TuckAt, none 0.12 rad on)
			const double Off2 = std::fabs(std::remainder(Q.Theta - L.FlipAt, 2.0 * Pi));
			Q.Flip = 1.0 - Smooth((Off2 - L.FlipCore) / 0.16);
			Q.Tuck = (1.0 - Q.Flip) * (1.0 - Smooth((std::fabs(std::remainder(Q.Theta - L.TuckAt, 2.0 * Pi)) - L.TuckCore) / 0.12));
			Q.Bil = Billow(Col);
			Q.Late = 0.06 * (1.0 - Q.Cs);
			Q.S3 = std::sin(3.0 * Q.Theta);
			Q.C3 = std::cos(3.0 * Q.Theta);
			Q.S7 = std::sin(7.0 * Q.Theta);
			Q.C7 = std::cos(7.0 * Q.Theta);
			Q.S5 = std::sin(5.0 * Q.Theta);
			Q.C5 = std::cos(5.0 * Q.Theta);
			Q.Loose = std::cos(1.5 * Pi * Q.Sn + L.Loose * 5.0);
			Q.Round2 = std::sin(2.0 * Q.Theta + L.WanderRound[0]);
			Q.Round3 = std::sin(3.0 * Q.Theta + L.WanderRound[1]);
			Q.Edge = Q.Sn >= 0.0 ? 0u : 1u;
			// (its far end's seams kinked: in two waves across it, so that its two layers keep together at its edges)
			Q.Kink[0] = std::sin(1.5 * Pi * Q.Sn);
			Q.Kink[1] = std::cos(1.5 * Pi * Q.Sn);
			Q.Kink[2] = std::sin(2.5 * Pi * Q.Sn);
			Q.Kink[3] = std::cos(2.5 * Pi * Q.Sn);
		}
		std::vector<Ring> Rg(static_cast<size_t>(Ringed));
		for (int R = 0; R < Ringed; ++R)
		{
			Ring& G = Rg[static_cast<size_t>(R)];
			double Nr = 0.0, Nz = 0.0;
			Gore(Rings[R], G.Gr, G.Gz, Nr, Nz);
			const double U = Along[R];
			G.U = U;
			G.Blown = 0.85 * Sag * std::sin(Pi * std::min(1.0, 0.15 + U));
			G.Slack = Smooth(K / (0.5 - 0.2 * U));
			G.Billow = 0.3 * std::sin(2.0 * Pi * (1.6 * U - 1.1 * K + L.Loose));
			// (as it lies: its far end rounding off, narrower and lower over its last fifth, down to the ground at its
			// crown; its near end gathered; its edges' waver, its wider side wider, and its edges' jags and folds; its far
			// end swelling on the side it slumps to and drawn in on the other, its silk drawn up over its crest; its axis;
			// its folds' share here, along it and across it, a fold across it lower toward its near end, which stands
			// lower; and its far end's buckle)
			G.End = U > 0.78 ? std::sqrt(std::max(0.0, 1.0 - std::pow((U - 0.78) / 0.22, 2.0))) : 1.0;
			G.Pocket = Smooth((U - 0.5) / 0.3);
			const double Open2 = Smooth(U / 0.3);
			for (int E = 0; E < 2; ++E)
			{
				const double Waver = 1.0 + L.EdgeSwell[E] * std::sin(Pi * U) + 0.08 * std::sin(2.0 * Pi * (L.EdgeWave[E] * U + L.EdgePhase[E]))
					+ 0.04 * std::sin(2.0 * Pi * (2.7 * L.EdgeWave[E] * U + L.Loose));
				const double Side = (E == WideSide ? 1.0 : -1.0) * L.Wider * Open2 + G.Pocket * G.Pocket * ((E == 0) == (Lean > 0.0) ? 0.08 : -0.06);
				// (its far end lopsided: rounding off later on the side it slumps to, sooner on the other)
				const double Ends = U > 0.78 ? std::sqrt(std::max(0.0, 1.0 - std::pow((U - 0.78) / (0.22 * ((E == 0) == (Lean > 0.0) ? 1.08 : 0.93)), 2.0))) : 1.0;
				G.Half[E] = 0.5 * L.Wide * (0.3 + 0.7 * Smooth(U / 0.3)) * (1.0 + L.Bulb * G.Pocket) * Ends * Waver * (1.0 + Side);
				G.Rim[E] = L.Jag[E][R] * (R < 2 ? 0.015 : R > 10 ? 0.025 : 0.045) * G.End;
			}
			for (int Lb = 0; Lb < L.Lobes; ++Lb)
			{
				// (each lobe rounded along it: about whole on its ring and the next, less on the rings either side)
				constexpr double Shape[5] = { 0.35, 0.85, 1.0, 0.55, 0.15 };
				const int Past = R - L.LobeRing[Lb];
				if (Past >= -1 && Past <= 3) G.Rim[L.LobeSide[Lb] == 0 ? WideSide : 1 - WideSide] += L.LobeOut[Lb] * Shape[Past + 1];
			}
			// (its far end broken up, from its tenth ring: its last two rings and its vent drawn over toward the side it
			// slumps to; a crease across its dome on its eleventh ring, slanted and let down, the rings either side drawn
			// in toward it; over its last two rings the edge of its narrow side folded back under it; and its seams kinked
			// ring by ring. Its ninth ring and those before it as they were, so that its folds across it and its length
			// are its own)
			G.Broken = R < 10 ? 0.0 : 1.0;
			constexpr double VentOn[LyingRings] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.6, 1.0 };
			G.Axis = AxisAt(U);
			G.Vent = (Lean > 0.0 ? 1.0 : -1.0) * L.Vent * 0.5 * L.Wide * VentOn[R];
			G.X = Nx + L.Long * U;
			G.Crease = R == 11 ? L.CreaseDeep : 0.0;
			G.Creased = R == 11 ? L.CreaseSlant : R == 10 || R == 12 ? 0.5 * L.CreaseSlant : 0.0;
			if (R == 10 || R == 12) G.X += (R == 10 ? 1.0 : -1.0) * 0.05;   // (5 cm: at 10 its silk had folded over there)
			G.Under = R < 11 ? 0.0 : 1.0;
			G.UnderAt = L.UnderAt + L.UnderSlant * (R - 11.5);
			for (int Ph = 0; Ph < 2; ++Ph)
			{
				const double Phase = R >= Ringed - 4 ? L.KinkPhase[R - (Ringed - 4)][Ph] : 0.0;
				G.KinkC[Ph] = std::cos(Phase);
				G.KinkS[Ph] = std::sin(Phase);
			}
			G.Cross = L.Cross[R] * (U < 0.18 ? 0.45 : U < 0.25 ? 0.7 : 1.0) * (U < 0.25 ? std::min(1.0, L.Lift / 0.5) : 1.0) * (1.0 - 0.6 * G.Pocket);
			G.CrossAt = L.CrossAt[R];
			G.CrossReach = std::max(0.1, L.CrossReach[R]);
			G.Skew = L.CrossSkew[R];
			G.Bow = L.CrossBow[R];
			G.Bunched = L.Long * L.CrossBunch[R] * (L.CrossBunch[R] < 0.0 ? (R > 0 ? U - Along[R - 1] : 0.0) : (R + 1 < Ringed ? Along[R + 1] - U : 0.0));
			G.Buckle = L.Buckle * (R == 10 ? 1.0 : R == 9 ? 0.35 : R == 11 ? 0.45 : 0.0);
			G.Wander2 = L.Wander[0] * std::sin(2.0 * Pi * (L.WanderAlong[0] * U + L.WanderFrom[0])) * Smooth(U / 0.15) * G.End;
			G.Wander3 = L.Wander[1] * std::sin(2.0 * Pi * (L.WanderAlong[1] * U + L.WanderFrom[1])) * Smooth(U / 0.15) * G.End;
			G.Fore = 0.05 + 0.15 * (1.0 - U);
			for (int F = 0; F < L.Ridges; ++F)
			{
				const double Along2 = Smooth((U - L.From[F]) / 0.08) * (1.0 - Smooth((U - L.To[F]) / 0.15));
				G.Mid[F] = L.At[F] + L.Drift[F] * (U - 0.5) + 0.035 * std::sin(2.0 * Pi * (L.Wave[F] * U + L.Phase[F]));
				G.Fold[F] = L.High[F] * Along2 * (0.8 + 0.2 * std::sin(2.0 * Pi * (0.7 * L.Wave[F] * U + 1.3 * L.Phase[F]))) * (1.0 - 0.65 * G.Pocket);
			}
			G.Bunch = 0.08 * (1.0 - Smooth(U / 0.15));
			G.Loose = 0.03 * std::sin(2.0 * Pi * (1.3 * U + L.Loose));
			const double A1 = 6.0 * L.Loose + 4.0 * U + 9.0 * K, A2 = 11.0 * L.Loose + 9.0 * U - 14.0 * K;
			G.A1S = std::sin(A1);
			G.A1C = std::cos(A1);
			G.A2S = std::sin(A2);
			G.A2C = std::cos(A2);
			const double A5 = 7.0 * U + 6.0 * L.Loose;
			G.A5S = std::sin(A5);
			G.A5C = std::cos(A5);
		}
		{
			// (a canopy lying bent across the wind, or with gores flipped out at its edge, has no more silk to spread across
			// it: its widths cut back, a quarter at most, so that all told, its axis's bend and its edges' folds too, it lies
			// no wider than its length over 2.12)
			auto Spans = [&Rg](double Share)
			{
				double Lo = 1e9, Hi = -1e9;
				for (const Ring& Rn : Rg)
				{
					Hi = std::max(Hi, Rn.Axis + Share * Rn.Half[0] + Rn.Rim[0]);
					Lo = std::min(Lo, Rn.Axis - Share * Rn.Half[1] - Rn.Rim[1]);
				}
				return Hi - Lo;
			};
			const double Cap = L.Long / 2.12, Full = Spans(1.0);
			if (Full > Cap)
			{
				const double None = Spans(0.0), Share = std::clamp((Cap - None) / std::max(1e-6, Full - None), 0.75, 1.0);
				for (Ring& Rn : Rg)
				{
					Rn.Half[0] *= Share;
					Rn.Half[1] *= Share;
				}
			}
		}
		// Each point: as it falls (Was, from its skirt's middle as it was, its side toward him blown in, its crown sinking,
		// tipped over), as it lies (Lies: its two layers of silk, a few long folds in its top one running along it and
		// three or four across it, its edges jagged and folded, its far end standing on the air it holds, slumped to one
		// side and buckled; over the ground, which is added once read), the lowest its top may be pressed to under a line
		// lying over it (Floor: never through its under layer, nor below a flipped hem's own height less 1 cm), how far
		// down it is (Settled), how it flaps (Flap: up and across, until it is down), and how much of a fold across it is
		// there (Crossing: 0 to 1, for its facing).
		const size_t Each = static_cast<size_t>(Ringed * Cols + 1);
		std::vector<V3> Was(Each), Lies;
		std::vector<double> Settled(Each), Flap(Each), Floor, Crossing;
		Out.Points.resize(Each);
		// (as kept, taken back without making them anew; Floor is wanted only to lay it)
		if (bKept)
		{
			Lies.swap(KeptWas.Lies);
			Crossing.swap(KeptWas.Crossing);
		}
		else
		{
			Lies.resize(Each);
			Crossing.assign(Each, 0.0);
			Floor.assign(Each, Clear);
		}
		const double Slumps = Lean > 0.0 ? 1.0 : -1.0;
		// (each fold across it: how much of it there is round each ring, highest at CrossAt across it and fading either
		// way; none on a ring with none, worked out once: only to lay it)
		std::vector<double> Envs(bKept ? 0 : static_cast<size_t>(Ringed * Cols), 0.0);
		for (int R = 0; R < Ringed && !bKept; ++R)
		{
			const Ring& G = Rg[static_cast<size_t>(R)];
			if (G.Cross <= 0.0) continue;
			for (int Col = 0; Col < Cols; ++Col)
			{
				const double Aslant = (Cl[static_cast<size_t>(Col)].Sn - G.CrossAt) / G.CrossReach;
				Envs[static_cast<size_t>(R * Cols + Col)] = std::exp(-Aslant * Aslant);
			}
		}
		const auto EnvAt = [&Envs, Cols](int R, int Col) { return R >= 0 && R < Ringed ? Envs[static_cast<size_t>(R * Cols + Col)] : 0.0; };
		for (int R = 0; R < Ringed; ++R)
		{
			const Ring& G = Rg[static_cast<size_t>(R)];
			const double Early = 0.06 + 0.14 * G.U;
			for (int Col = 0; Col < Cols; ++Col)
			{
				const Column& Q = Cl[static_cast<size_t>(Col)];
				const size_t P = static_cast<size_t>(R * Cols + Col);
				const double Crumple = 1.0 - 0.2 * G.Slack * (0.5 + 0.5 * (Q.S5 * G.A5C + Q.C5 * G.A5S));
				const double Out1 = Ax * G.Gr * (1.0 + Q.Bil * G.Gr) / Widest() * Crumple, Blown = G.Blown * Q.Back;
				const double Rx = Out1 * Q.Cs * (1.0 + 0.1 * Sag) + Blown * Ax, Ry = Out1 * Q.Sn * (1.0 + 0.1 * Sag) * (1.0 - 0.25 * Blown);
				const double Rz = Skirt - Pivot + Dz * G.Gz * (1.0 - 0.6 * Sag) - 0.35 * Blown * Dz;
				Was[P] = { Rx * TipC + Rz * TipS, Ry, Pivot - Rx * TipS * (1.0 - 0.8 * G.Slack) + Rz * TipC };
				if (!bKept)
				{
					// (its folds across it: each highest somewhere across it and fading either way; and where one is, on its ring
					// and those either side, its long folds a little flattened, bunched across)
					const double Env = EnvAt(R, Col), Before = EnvAt(R - 1, Col), After = EnvAt(R + 1, Col);
					const double Crossed = std::max({ Env, Before, After });
					double F = 0.0;
					for (int Fd = 0; Fd < L.Ridges; ++Fd)
					{
						const double Aside = (Q.Sn - G.Mid[Fd]) / L.Width[Fd];
						if (std::fabs(Aside) < 3.5) F += G.Fold[Fd] * std::exp(-Aside * Aside);
					}
					F *= 1.0 - 0.3 * Crossed * (1.0 - Smooth((G.U - 0.15) / 0.15));
					// (its far end's air, buckled across its top on the side it slumps from; on its narrow side coming down to
					// where it is folded under, and let down along its crease, deeper on its narrow side; and over its folded
					// edge, the rest lifted by the silk under it)
					double Arch = Q.Arch;
					const bool bNarrow = Q.Sn * Slumps < 0.0;
					if (G.Under > 0.0 && bNarrow)
					{
						const double Across2 = (Q.Sn - Lean) / (G.UnderAt + std::fabs(Lean));
						Arch += (std::sqrt(std::max(0.0, 1.0 - Across2 * Across2)) - Arch) * G.Under;
					}
					const double Air = L.Lift * G.Pocket * Arch * G.End * (1.0 - G.Buckle * Smooth((0.4 - Slumps * Q.Sn) / 0.6)) * (1.0 - G.Crease * (0.8 - 0.2 * Slumps * Q.Sn));
					const double High = 0.045 + G.Bunch + F * G.End + std::max(0.0, G.Loose * Q.Loose) + Air;
					const double Low = Clear + (0.3 * F + 0.05 * G.Pocket * Arch) * G.End + (bNarrow ? 0.012 * G.Under : 0.0);
					// (a fold across it, highest somewhere across it and fading either way, lifts its long folds and the troughs
					// between them alike, the whole of its top there, and its underside a little; and bunches it up toward the
					// ring before it or after it, the more where it stands higher, slanted and bowed)
					Crossing[P] = Env;
					// (and its crest rounded over onto the ring it is bunched toward, its other flank steep, the ring there a
					// little let down)
					double Shoulder = 0.0, Dip = 0.0;
					if (Before > 0.0)
					{
						const double Was2 = Rg[static_cast<size_t>(R - 1)].Cross * Before;
						(L.CrossBunch[R - 1] > 0.0 ? Shoulder : Dip) = Was2;
					}
					if (After > 0.0)
					{
						const double Next2 = Rg[static_cast<size_t>(R + 1)].Cross * After;
						double& Into = L.CrossBunch[R + 1] < 0.0 ? Shoulder : Dip;
						Into = std::max(Into, Next2);
					}
					double Z = Low + (High - Low) * Q.Top * (1.0 - 0.8 * Dip) + (G.Cross * Env + 0.5 * Shoulder) * G.End * (0.12 + 0.88 * std::min(1.0, Q.Top / 0.6));
					// (its edges' folds: a gore flipped over outward lies flat on the ground, its two layers one on the other)
					const double Rim = G.Rim[Q.Edge] * Q.Rim;
					if (Rim * Q.Sn > 0.0) Z += (Clear + 0.004 + 0.022 * Q.Top - Z) * std::min(1.0, std::fabs(Rim) / 0.15) * Q.Rim * Q.Rim;
					double X = G.X + G.Fore * Q.Cs + G.Skew * Q.Sn + G.Bow * (Q.Sn * Q.Sn - 0.5) + G.Bunched * (0.35 + 0.65 * Env);
					const double Wandered = G.Wander2 * Q.Round2 + G.Wander3 * Q.Round3;
					double Y = G.Half[Q.Edge] * Q.Sn + Rim + Wandered + LeanOver * G.Pocket * G.End * Q.Over * Q.Top;
					if (G.Broken > 0.0)
					{
						// (its far end: its crease slanted across it; its seams kinked, in two waves across it, both its layers
						// alike so that they keep together at its edges; and its narrow side past UnderAt folded back under it,
						// flat on the ground, its two layers the other way up, under the rest)
						X += G.Creased * Q.Sn;
						const double Wave = (Q.Kink[0] * G.KinkC[0] + Q.Kink[1] * G.KinkS[0]) + 0.5 * (Q.Kink[2] * G.KinkC[1] + Q.Kink[3] * G.KinkS[1]);
						Y += L.Kinked * G.Broken * Wave;
						X += 0.8 * L.Kinked * G.Broken * (Q.Kink[1] * G.KinkC[0] - Q.Kink[0] * G.KinkS[0]);
						// (its last ring no farther on than its tip)
						if (R == Ringed - 1) X = std::min(X, Nx + L.Long - 0.06);
						const double Fold = -Slumps * G.UnderAt * G.Half[Q.Edge], Past = -Slumps * (Y - Fold);
						if (bNarrow && Past > 0.0)
						{
							Y += Slumps * 1.8 * Past * G.Under;
							Z += (Clear + 0.002 + 0.004 * (1.0 - Q.Top) - Z) * G.Under * Smooth(Past / 0.08);
						}
					}
					Lies[P] = { X, G.Axis + G.Vent + Y, Z };
					Floor[P] = Low + 0.012 * Q.Top;
				}
				Settled[P] = Smooth((K - Early - Q.Late) / 0.6);
				const double Flapping = 0.6 * (Q.S3 * G.A1C + Q.C3 * G.A1S) + 0.4 * (Q.S7 * G.A2C + Q.C7 * G.A2S);
				Flap[P] = Sag * (1.0 - Settled[P]) * (0.55 * std::min(1.0, 0.3 + G.Gr) * Flapping + G.Billow);
			}
		}
		const int Seams = Cols / 4;
		const V3 Hn{ Hx, Hy, H.Z };
		if (!bKept)
		{
			// (its hem as it lies where not flipped: each gore's a little back toward him or on, in a wave round it, so that
			// its near edge wavers)
			for (int Col = 0; Col < Cols; ++Col)
			{
				const Column& Q = Cl[static_cast<size_t>(Col)];
				Lies[static_cast<size_t>(Col)].X -= L.HemWave * std::sin(L.HemWaves * Q.Theta + L.HemPhase) * Q.Back * (1.0 - Q.Flip) * (1.0 - Q.Tuck);
			}
			// (its skirt's gores flipped over at its near end (the art director's note of 24 November 17:40Z: two or three
			// gores flipped over at its hem, and a few of its lines across its silk): their fold, its second ring, pulled
			// back toward him past its near edge and laid on the ground there along their lines, a roll of silk under his
			// lines; their hem carried on toward where its seams gather and drawn in toward its middle (keeping some of its
			// spread across, so that they lie as a flap, not a point), and laid on its silk there; so that their lines, and
			// those beside them, run over their silk. As far as its gathering lets them (a review of 24
			// November): their seams still behind the middle of its hem's (5 cm or more, its lines running back from the
			// same half of its skirt all the while it falls in), and no silk more than 0.46 m nearer him than that middle.
			// And one or two gores on the other side of its top's middle tucked under, their hem lying a little on under it)
			V3 Gather;
			for (int Sm = 0; Sm < Seams; ++Sm) Gather = Plus(Gather, Times(Lies[static_cast<size_t>(4 * Sm)], 1.0 / Seams));
			const std::vector<V3> Laid(Lies.begin(), Lies.begin() + 3 * Cols);
			// (the top of its silk as laid, under a point: its top layer's highest there, each column's along it between its
			// first three rings)
			const auto TopAt = [&Laid, &Cl, Cols](double X, double Y, double Else)
			{
				double Best = -1e9;
				for (int Col = 0; Col < Cols; ++Col)
				{
					if (Cl[static_cast<size_t>(Col)].Cs > -0.2) continue;
					for (int R = 0; R < 2; ++R)
					{
						const V3& A = Laid[static_cast<size_t>(R * Cols + Col)];
						const V3& B = Laid[static_cast<size_t>((R + 1) * Cols + Col)];
						if ((X - A.X) * (X - B.X) > 0.0) continue;
						const double T = (X - A.X) / std::max(1e-9, B.X - A.X);
						if (std::fabs(A.Y + (B.Y - A.Y) * T - Y) < 0.04) Best = std::max(Best, A.Z + (B.Z - A.Z) * T);
					}
				}
				return Best > -1e8 ? Best : Else;
			};
			double Moved = 0.0;
			for (int Col = 0; Col < Cols; ++Col)
			{
				const Column& Q = Cl[static_cast<size_t>(Col)];
				V3& Hem0 = Lies[static_cast<size_t>(Col)];
				const V3 Was0 = Hem0;
				if (Q.Flip > 0.0)
				{
					const double W = Q.Flip;
					const double Tx = std::max(Was0.X, Gather.X - 0.06), Ty = Rg[0].Axis + (0.7 - L.FlipOver) * (Was0.Y - Rg[0].Axis);
					const double HemX = Was0.X + W * (Tx - Was0.X), HemY = Was0.Y + W * (Ty - Was0.Y);
					Hem0 = { HemX, HemY, Was0.Z + W * (TopAt(HemX, HemY, Was0.Z) + 0.022 - Was0.Z) };
					Floor[static_cast<size_t>(Col)] = Hem0.Z - 0.01;
				}
				else if (Q.Tuck > 0.0)
				{
					const double W = Q.Tuck;
					Hem0 = { Was0.X + W * std::max(0.0, std::min(L.TuckOn, Gather.X - 0.06 - Was0.X)), Was0.Y, Was0.Z - 0.015 * W };
				}
				if (Col % 4 == 0) Moved += (Hem0.X - Was0.X) / Seams;
			}
			// (their fold back toward him along their lines, as far as it may go)
			const double Back = Gather.X + Moved - 0.46;
			for (int Col = 0; Col < Cols; ++Col)
			{
				const double W = Cl[static_cast<size_t>(Col)].Flip;
				if (W <= 0.0) continue;
				V3& Fold = Lies[static_cast<size_t>(Cols + Col)];
				const V3& Hem0 = Lies[static_cast<size_t>(Col)];
				const double FoldX = std::max(Back, Laid[static_cast<size_t>(Col)].X - L.FlipBack);
				const double FoldY = Hem0.Y + (Hn.Y - Hem0.Y) * (Hem0.X - FoldX) / std::max(0.5, Hem0.X - Hn.X);
				Fold = { Fold.X + W * (FoldX - Fold.X), Fold.Y + W * (FoldY - Fold.Y), Fold.Z + W * (Clear + 0.02 - Fold.Z) };
				Floor[static_cast<size_t>(Cols + Col)] = Clear;
			}
		}
		// (her note of 24 November 17:40Z, (3), after a second look, and the reviews of what followed: its lines had
		// crossed only the flap at its near end, all running to one point; then, laid on past their seams' ends over its
		// gathering, they had still all run from that point, lain more along it than across it, too long, floating over its
		// folds and buried under its edge. It slid and slewed as it fell in, and two to four of its lines were thrown over
		// its near part: each from his harness across the grass, bowed out to one side of the rest, either to the edge of
		// its near part on that side (from its first ring to its third, not where its lines gather) or over its near end
		// aside of its gathering (0.2 m or more from it, a third to four fifths of the way out to an edge), onto its top
		// and across it at a slant (28 to 36 degrees off its wind line at the least, as steep as each may lie, 45 on the
		// mean; bowed along it, curling across it and a little wiggle), up over its folds and down into them, to an edge,
		// over that and down to the ground beside it (on under there, hidden, back to its hem). Each laid out on its silk's
		// own rings and round it (a Strand's path: from its seam's end on its hem, where it was in the air, round its near
		// part to where it comes onto its top, and across to its other edge), so that it lies on its silk as that falls in
		// and wherever it lies; as its lines go slack it is drawn over its silk along that path (below). 1 to 2.35 m of it
		// on its silk as laid (to 2.55 m in later tries, should fewer lie so; the drawn line, over its folds, a little
		// longer), 0.35 m or more of that past 0.15 of its length from where its lines gather (its body), none of it
		// farther along than 0.45 of its length, none under its flipped gores' flap, its way across the grass clear of its
		// near part, and from his harness to its far edge no longer than a real canopy's line (RealLines times its spread
		// across: the air's hang draws its lines short, LinesLong; the line's seam's end, where it is sewn, stays on its
		// gathered hem with the rest). The rest run to its gathered hem as they did.)
		struct Strand
		{
			int Seam = 0;
			double Th0 = 0.0, Ri = 0.0, Si = 1.0, Re = 0.0, Se = -1.0, Curl = 0.0, Bow = 0.0, Wiggle = 0.0;
		};
		std::vector<Strand> Strands;
		constexpr double Rise = 0.012, Rope = 0.015;
		// (a point of a strand's path at T, from -1, its seam's end on its hem, to 0, its edge where it comes onto its top,
		// and on to 1, its other edge: its ring and column there, with their fractions; its top's points round from its
		// middle (Sn 0) to its edges (-1 and 1) as Round's columns go)
		const auto PathAt = [Cols](const Strand& S, double T, double& R, double& Cf)
		{
			if (T < 0.0)
			{
				const double A = T + 1.0;
				R = A * S.Ri;
				Cf = ColOf(S.Th0 + (Pi - std::asin(S.Si) - S.Th0) * A, Cols);
				return;
			}
			R = S.Ri + (S.Re - S.Ri) * T + S.Bow * std::sin(Pi * T);
			const double Sn = S.Si + (S.Se - S.Si) * (T + S.Curl * T * (1.0 - T)) + 0.05 * std::sin(3.0 * Pi * T + S.Wiggle) * std::sin(Pi * T);
			Cf = ColOf(Pi - std::asin(std::clamp(Sn, -1.0, 1.0)), Cols);
		};
		constexpr int PathNear = 6, PathOver = 80, PathFine = PathNear + PathOver + 1;
		const auto PathT = [](int I) { return I < PathNear ? -1.0 + static_cast<double>(I) / PathNear : static_cast<double>(I - PathNear) / PathOver; };
		// (its edge at ring R on side Side (1 its right), its silk's outermost there: where it goes over it, and the way
		// out from it across the ground, square to its edge)
		const auto EdgeOut = [Cols](const std::vector<V3>& Pts, double R, double Side, V3& Out2)
		{
			const double Ce = ColOf(Side > 0.0 ? 0.5 * Pi : 1.5 * Pi, Cols);
			const V3 A = SurfAt(Pts, nullptr, Ringed, Cols, std::max(0.0, R - 0.25), Ce, nullptr), B = SurfAt(Pts, nullptr, Ringed, Cols, R + 0.25, Ce, nullptr);
			const double Tx = B.X - A.X, Ty = B.Y - A.Y, Tl = std::max(1e-9, std::hypot(Tx, Ty));
			Out2 = { -Ty / Tl, Tx / Tl, 0.0 };
			if (Out2.Y * Side < 0.0) Out2 = { Ty / Tl, -Tx / Tl, 0.0 };
			return SurfAt(Pts, nullptr, Ringed, Cols, R, Ce, nullptr);
		};
		// (where a strand comes onto its silk and the way out from there: its edge, or, one coming on over its near end (Si
		// short of its edge), its top there and the way out toward his harness)
		const auto EntryOut = [&EdgeOut, &Hn, Cols](const std::vector<V3>& Pts, const Strand& S, V3& Out2)
		{
			if (std::fabs(S.Si) > 0.999) return EdgeOut(Pts, S.Ri, S.Si, Out2);
			const V3 P = SurfAt(Pts, nullptr, Ringed, Cols, S.Ri, ColOf(Pi - std::asin(S.Si), Cols), nullptr);
			const double Wx = Hn.X - P.X, Wy = Hn.Y - P.Y, Wl = std::max(1e-9, std::hypot(Wx, Wy));
			Out2 = { Wx / Wl, Wy / Wl, 0.0 };
			return P;
		};
		// (its way across the grass from his harness to its edge, bowed out that side by 0.07 of its length: at T, 0 to 1)
		const auto GrassAt = [](const V3& From, const V3& To, double Side, double T)
		{
			const double Wx = To.X - From.X, Wy = To.Y - From.Y, Wl = std::max(1e-9, std::hypot(Wx, Wy));
			double Nx2 = -Wy / Wl, Ny2 = Wx / Wl;
			if (Ny2 * Side < 0.0) { Nx2 = -Nx2; Ny2 = -Ny2; }
			const double Aside = 0.07 * Wl * std::sin(Pi * T);
			return V3{ From.X + Wx * T + Nx2 * Aside, From.Y + Wy * T + Ny2 * Aside, 0.0 };
		};
		// (laid once, from its lying shape before the ground, and kept with it)
		constexpr size_t PerStrand = 9;
		const std::vector<double>& LaidWas = KeptWas.Laid;
		if (bKept && LaidWas.size() % PerStrand == 0)
		{
			for (size_t Q = 0; Q < LaidWas.size(); Q += PerStrand)
			{
				Strand S;
				S.Seam = static_cast<int>(LaidWas[Q]);
				S.Th0 = LaidWas[Q + 1]; S.Ri = LaidWas[Q + 2]; S.Si = LaidWas[Q + 3]; S.Re = LaidWas[Q + 4];
				S.Se = LaidWas[Q + 5]; S.Curl = LaidWas[Q + 6]; S.Bow = LaidWas[Q + 7]; S.Wiggle = LaidWas[Q + 8];
				Strands.push_back(S);
			}
		}
		else
		{
			const auto RingOf = [&Along](double U)
			{
				int R = 0;
				while (R + 2 < Ringed && Along[R + 1] <= U) ++R;
				return R + std::clamp((U - Along[R]) / (Along[R + 1] - Along[R]), 0.0, 1.0);
			};
			// (a gore flipped or tucked at its near end, or beside one: its hem and fold lie elsewhere)
			const auto Moved = [&Cl, Cols](double Cf)
			{
				const int C0 = ((static_cast<int>(std::floor(Cf)) % Cols) + Cols) % Cols;
				for (int D = -1; D <= 2; ++D)
				{
					const Column& Q = Cl[static_cast<size_t>((C0 + D + Cols) % Cols)];
					if (Q.Flip > 0.0 || Q.Tuck > 0.0) return true;
				}
				return false;
			};
			// (its flipped gores' silk, from their hem laid on its top at its near end back to their fold and on to their
			// third ring: its triangles and the box round them, and whether a point lies under one)
			std::vector<V3> Flapped;
			double FlapLo[2] = { 1e9, 1e9 }, FlapHi[2] = { -1e9, -1e9 };
			for (int Col = 0; Col < Cols; ++Col)
			{
				const int Next = (Col + 1) % Cols;
				if (Cl[static_cast<size_t>(Col)].Flip <= 0.0 && Cl[static_cast<size_t>(Next)].Flip <= 0.0) continue;
				for (int R = 0; R < 2; ++R)
				{
					const V3& A = Lies[static_cast<size_t>(R * Cols + Col)];
					const V3& B = Lies[static_cast<size_t>(R * Cols + Next)];
					const V3& Ca = Lies[static_cast<size_t>((R + 1) * Cols + Col)];
					const V3& Cb = Lies[static_cast<size_t>((R + 1) * Cols + Next)];
					Flapped.insert(Flapped.end(), { A, B, Cb, A, Cb, Ca });
					for (const V3* Q : { &A, &B, &Ca, &Cb })
					{
						FlapLo[0] = std::min(FlapLo[0], Q->X); FlapHi[0] = std::max(FlapHi[0], Q->X);
						FlapLo[1] = std::min(FlapLo[1], Q->Y); FlapHi[1] = std::max(FlapHi[1], Q->Y);
					}
				}
			}
			const auto UnderFlap = [&Flapped, &FlapLo, &FlapHi](const V3& P)
			{
				if (P.X < FlapLo[0] - 0.05 || P.X > FlapHi[0] + 0.05 || P.Y < FlapLo[1] - 0.05 || P.Y > FlapHi[1] + 0.05) return false;
				for (size_t T = 0; T + 2 < Flapped.size(); T += 3)
				{
					const V3& A = Flapped[T];
					const V3& B = Flapped[T + 1];
					const V3& C2 = Flapped[T + 2];
					const double D = (B.X - A.X) * (C2.Y - A.Y) - (C2.X - A.X) * (B.Y - A.Y);
					if (std::fabs(D) < 1e-12) continue;
					const double Wb = ((P.X - A.X) * (C2.Y - A.Y) - (C2.X - A.X) * (P.Y - A.Y)) / D, Wc = ((B.X - A.X) * (P.Y - A.Y) - (P.X - A.X) * (B.Y - A.Y)) / D;
					if (Wb < -0.1 || Wc < -0.1 || Wb + Wc > 1.1) continue;
					if (A.Z + (B.Z - A.Z) * Wb + (C2.Z - A.Z) * Wc > P.Z - 0.02) return true;
				}
				return false;
			};
			// (its length as it lies, from its near end to its tip (its crown, not yet laid: Nx + L.Long), and where its
			// lines gather: its body is past 0.15 of that from there; and the farthest along it a line may reach)
			double MinX = 1e9, MaxX = Nx + L.Long, GatherX = 0.0;
			for (size_t P = 0; P + 1 < Each; ++P)
			{
				MinX = std::min(MinX, Lies[P].X);
				MaxX = std::max(MaxX, Lies[P].X);
			}
			double GatherY = 0.0;
			for (int Sm = 0; Sm < Seams; ++Sm)
			{
				GatherX += Lies[static_cast<size_t>(4 * Sm)].X / Seams;
				GatherY += Lies[static_cast<size_t>(4 * Sm)].Y / Seams;
			}
			const double Body = GatherX + 0.15 * (MaxX - MinX), Farthest = RingOf(0.45);
			// (the seams that may: a line runs back to him from it, and its gore and those beside it lie as laid at its near
			// end; the nearest its top's middle first, picked on its dice. How near, exactly: in whole half-seams round from
			// it (by its sine's size, mirror seams had tied to the last bit or within a few, so that the libm's rounding chose
			// which line was laid where, a Mac's apart from a PC's). Between two as near, its far side's first, but for the
			// widest two a line runs back from (whether or not they may), its near side's first: the order glibc's sines gave
			// for the gores the scene makes, 24, 26 and 28)
			int Widest2 = 0;
			for (int Sm = 0; Sm < Seams; ++Sm)
			{
				if (LineBack(static_cast<size_t>(Sm), static_cast<size_t>(Seams))) Widest2 = std::max(Widest2, std::abs(2 * Sm - Seams));
			}
			std::vector<std::pair<double, int>> May;
			for (int Sm = 0; Sm < Seams; ++Sm)
			{
				if (!LineBack(static_cast<size_t>(Sm), static_cast<size_t>(Seams)) || Moved(4.0 * Sm)) continue;
				const int Away2 = std::abs(2 * Sm - Seams);
				const bool bFirst = Away2 == Widest2 ? 2 * Sm < Seams : 2 * Sm > Seams;
				May.emplace_back(2.0 * Away2 + (bFirst ? 0.0 : 1.0), Sm);
			}
			std::sort(May.begin(), May.end());
			const std::vector<std::pair<double, int>> Each2 = May;
			// (no more than six of its lines over its silk in all, with those over its flipped gores' flap: those to its
			// flipped gores and the gores beside them; one at the least)
			int Flapping = 0;
			for (int Sm = 0; Sm < Seams; ++Sm)
			{
				bool bOver = false;
				for (int D = -1; D <= 1; ++D) bOver = bOver || Cl[static_cast<size_t>((4 * Sm + D + Cols) % Cols)].Flip > 0.3;
				Flapping += LineBack(static_cast<size_t>(Sm), static_cast<size_t>(Seams)) && bOver;
			}
			const int Wanted = std::min(L.Strewn, std::max(1, 6 - Flapping));
			// (how much of one coming on over its near end lies over its near part before it comes onto its top: from its
			// hem there, across the ground, to where it comes on; and where it first lies over its silk)
			const auto NearOver = [&](const Strand& S, V3& From)
			{
				From = SurfAt(Lies, nullptr, Ringed, Cols, S.Ri, ColOf(Pi - std::asin(std::clamp(S.Si, -1.0, 1.0)), Cols), nullptr);
				if (std::fabs(S.Si) > 0.999) return 0.0;
				const V3 B = From;
				From = SurfAt(Lies, nullptr, Ringed, Cols, 0.0, ColOf(Pi - std::asin(S.Si), Cols), nullptr);
				return std::hypot(B.X - From.X, B.Y - From.Y);
			};
			// (whether a strand lies as it should on its silk as laid: Least to Most of it on its silk, BodyLeast of that past
			// its body's start, none of it farther along than Farthest and a little, Steep or more off its wind line from edge
			// to edge and on downwind (over its flipped gores' flap, should it cross that, as over the rest), coming onto it
			// within his lines' length in the air of his
			// harness (LinesLong), and from his harness (the grass bowed, 1.02 times the straight) over its silk (a twentieth
			// more for its folds) and over its far edge to the ground no longer than a real line (RealLines))
			const double Budget = 0.98 * RealLines * C.Across;
			const auto Fits = [&](const Strand& S, double Least, double Most, double BodyLeast, double Steep)
			{
				constexpr int Samples = 16;
				V3 First, Back, Onto;
				double Silk = NearOver(S, Onto), Over3 = Silk, OnBody = 0.0;
				for (int I = 0; I <= Samples; ++I)
				{
					double R = 0.0, Cf = 0.0;
					PathAt(S, static_cast<double>(I) / Samples, R, Cf);
					if (R > Farthest + 0.25) return false;
					const V3 Now = SurfAt(Lies, nullptr, Ringed, Cols, R, Cf, nullptr);
					if (I == 0) First = Now;
					else
					{
						const double Step = std::hypot(Now.X - Back.X, Now.Y - Back.Y);
						Silk += Step;
						Over3 += Length(Minus(Now, Back));
						OnBody += Now.X > Body ? Step : 0.0;
					}
					Back = Now;
				}
				const double Slant = std::atan2(std::fabs(Back.Y - Onto.Y), Back.X - Onto.X), Grass = Length(Minus(First, Hn));
				return Silk >= Least && Silk <= Most && OnBody >= BodyLeast && Slant >= Steep && Grass <= LinesLong - 0.15 && 1.03 * Grass + 1.1 * Over3 + 0.3 <= Budget;
			};
			// (whether its way across the grass from his harness to just outside its edge at In keeps 3 cm or more outside its
			// near part that side, and off its flap: each point of its rings before that edge's, and its flipped gores')
			const auto Clear2 = [&](const V3& In, const V3& Way, double Side, double Ri)
			{
				const V3 To{ In.X + 0.06 * Way.X, In.Y + 0.06 * Way.Y, 0.0 };
				constexpr int Samples = 12;
				V3 Gr[Samples + 1];
				for (int I = 0; I <= Samples; ++I) Gr[I] = GrassAt(Hn, To, Side, static_cast<double>(I) / Samples);
				const int Upto = std::min(Ringed - 1, static_cast<int>(std::floor(Ri)));
				for (int R = 0; R <= Upto; ++R)
				{
					for (int Col = 0; Col < Cols; ++Col)
					{
						const V3& Q = Lies[static_cast<size_t>(R * Cols + Col)];
						if (Q.X >= To.X - 0.05 || Q.X <= Gr[0].X) continue;
						int I = 0;
						while (I + 1 < Samples && Gr[I + 1].X < Q.X) ++I;
						const double T = std::clamp((Q.X - Gr[I].X) / std::max(1e-9, Gr[I + 1].X - Gr[I].X), 0.0, 1.0);
						if (Side * (Gr[I].Y + (Gr[I + 1].Y - Gr[I].Y) * T - Q.Y) < 0.03) return false;
					}
				}
				for (int I = 1; I < Samples && !Flapped.empty(); ++I)
				{
					if (UnderFlap({ Gr[I].X, Gr[I].Y, Rope })) return false;
				}
				return true;
			};
			// (laid from a seam, on its own dice: which edge it comes onto (its own seam's side of its top, or either where its
			// seam is at its middle) and on which ring (its first to its second, where its near part narrows to its gathering),
			// and how much farther along than it must it crosses to its other edge: there 0.3 m or more past its body's start,
			// and on by up to half a ring as diced. Tried again as needed farther along that edge, from a little nearer or
			// farther along its first, from its other edge, and then over its near end (where its near end lies far from his
			// harness or its grass would cross its near part), a third to four fifths of the way out to either edge, onto its
			// top on its first ring or nearer; none coming onto its silk where another does, nor crossing less steeply than
			// Steep)
			const auto Lay = [&](int Sm, int Ln, double Least, double Most, double BodyLeast, double Steep, Strand& S)
			{
				S.Seam = Sm;
				S.Th0 = 2.0 * Pi * Sm / Seams;
				S.Curl = L.StrewnCurl[Ln];
				S.Bow = L.StrewnBow[Ln];
				S.Wiggle = L.StrewnWiggle[Ln];
				const double Sn0 = std::sin(S.Th0);
				const double Side0 = std::fabs(Sn0) > 0.2 ? (Sn0 > 0.0 ? 1.0 : -1.0) : (L.StrewnWay[Ln] < 0.5 ? -1.0 : 1.0);
				const double Ri0 = 0.6 + 0.9 * L.StrewnRing[Ln], On0 = 0.5 * L.StrewnFar[Ln];
				constexpr double Rings4[4] = { 0.0, 0.3, -0.3, 0.6 };
				for (int Go = 0; Go < 16; ++Go)
				{
					const bool bFront = Go >= 8;
					const double Sign = (bFront ? (Go < 12 ? 1.0 : -1.0) : (Go < 4 ? 1.0 : -1.0)) * Side0;
					S.Si = bFront ? Sign * (0.05 + 0.25 * L.StrewnFront[Ln] + 0.2 * (Go / 2 % 2)) : Sign;
					S.Ri = bFront ? 0.2 + 0.3 * L.StrewnRing[Ln] + 0.25 * (Go % 2) : std::clamp(Ri0 + Rings4[Go % 4], 0.35, 2.2);
					bool bTaken = false;
					for (const Strand& O : Strands) bTaken = bTaken || ((O.Si > 0.0) == (Sign > 0.0) && (std::fabs(O.Si) > 0.999) != bFront && std::fabs(O.Ri - S.Ri) < 0.5);
					if (bTaken) continue;
					V3 Way;
					const V3 In = EntryOut(Lies, S, Way);
					if (!bFront && !Clear2(In, Way, S.Si, S.Ri)) continue;
					// (one coming on over its near end aside of where its lines gather, not over it: 0.2 m or more)
					bool bNear = false;
					for (int Q = 1; Q <= 8 && bFront; ++Q)
					{
						const V3 G2 = GrassAt(Hn, In, S.Si, Q / 8.0);
						bNear = bNear || std::hypot(G2.X - GatherX, G2.Y - GatherY) < 0.2;
					}
					if (bNear) continue;
					V3 Onto;
					const double Over = NearOver(S, Onto);
					// (off its other edge; or, one coming on over its near end (near its middle), off that same side, or should
					// that not be steep enough, its other)
					for (int Off3 = 0; Off3 < (bFront ? 2 : 1); ++Off3)
					{
						S.Se = (Off3 == 0) == bFront ? Sign : -Sign;
						const double Ce = ColOf(S.Se > 0.0 ? 0.5 * Pi : 1.5 * Pi, Cols);
						double Rb = S.Ri + 0.5;
						while (Rb < Farthest && SurfAt(Lies, nullptr, Ringed, Cols, Rb, Ce, nullptr).X < Body + 0.75 * BodyLeast) Rb += 0.25;
						for (int On = 0; On < 5; ++On)
						{
							S.Re = Rb + On0 + 0.25 * On;
							if (S.Re > Farthest) break;
							const V3 Off2 = SurfAt(Lies, nullptr, Ringed, Cols, S.Re, Ce, nullptr);
							if (std::atan2(std::fabs(Off2.Y - Onto.Y), Off2.X - Onto.X) < Steep) break;
							if (Over + Length(Minus(Off2, In)) > Most - 0.05) continue;
							if (Fits(S, Least, Most, BodyLeast, Steep)) return true;
						}
					}
				}
				return false;
			};
			// (as many as it may, each 1 to 2.5 m on its silk; then, should fewer than two lie so, on a wide canopy whose body
			// is wider than that across, up to 3 m; and should none, the first that may laid less strictly: shorter, less
			// of it past its body's start, less steeply (a review: one in some thousand pilots had had none across it))
			constexpr double PassMost[3] = { 2.35, 2.45, 2.55 }, PassSteep[3] = { 36.0, 30.0, 28.0 };
			for (int Pass = 0; Pass < 3; ++Pass)
			{
				for (int Ln = 0; static_cast<int>(Strands.size()) < (Pass == 0 ? Wanted : std::min(Wanted, 2)) && !May.empty(); Ln = std::min(Ln + 1, 3))
				{
					const size_t Pick = std::min(May.size() - 1, static_cast<size_t>(L.StrewnPick[Ln] * static_cast<double>(May.size())));
					const int Sm = May[Pick].second;
					May.erase(May.begin() + static_cast<std::ptrdiff_t>(Pick));
					Strand S;
					if (!Lay(Sm, Ln, 1.0, PassMost[Pass], 0.35, PassSteep[Pass] * Rad, S)) continue;
					Strands.push_back(S);
					// (and none from the seams either side of it, so that they lie apart)
					May.erase(std::remove_if(May.begin(), May.end(), [Sm, Seams](const std::pair<double, int>& Mp) { return (Mp.second - Sm + Seams + 1) % Seams <= 2; }), May.end());
				}
				if (Pass < 2)
				{
					// (the seams that may again, but those laid and those beside them)
					May = Each2;
					for (const Strand& S : Strands)
					{
						const int Sm = S.Seam;
						May.erase(std::remove_if(May.begin(), May.end(), [Sm, Seams](const std::pair<double, int>& Mp) { return (Mp.second - Sm + Seams + 1) % Seams <= 2; }), May.end());
					}
				}
			}
			for (int Last = 0; Last < 2; ++Last)
			{
				for (size_t Pick = 0; Strands.empty() && Pick < Each2.size(); ++Pick)
				{
					Strand S;
					if (Lay(Each2[Pick].second, static_cast<int>(Pick % 4), 0.7, Last == 0 ? 2.6 : 3.0, 0.3, (Last == 0 ? 25.0 : 20.0) * Rad, S)) Strands.push_back(S);
				}
			}
		}
		for (const Strand& S : Strands) Out.Kept.Laid.insert(Out.Kept.Laid.end(), { static_cast<double>(S.Seam), S.Th0, S.Ri, S.Si, S.Re, S.Se, S.Curl, S.Bow, S.Wiggle });
		if (!bKept)
		{
			// (his lines as they lie (LandBattleAir's DownShape, slack): from his harness, each in two lengths, its middle
			// let down to the lower end's height and bowed aside by 0.06 of its length, every other line the other way.
			// Where one runs over its silk (its flipped gores' lines, over their own silk and any beside it), the silk under
			// it is pressed down under it, so that the line lies on it and not in it (a review of 24 November: lines had
			// gone 7 cm into it), never lower than the silk under that; its seams, where the lines end, where they are)
			double Furthest = -1e9;
			std::vector<V3> Ends(static_cast<size_t>(Seams));
			for (int Sm = 0; Sm < Seams; ++Sm)
			{
				Ends[static_cast<size_t>(Sm)] = Lies[static_cast<size_t>(4 * Sm)];
				Furthest = std::max(Furthest, Ends[static_cast<size_t>(Sm)].X);
			}
			int Near = 1;
			while (Near < Ringed && Nx + L.Long * Along[Near] - 0.6 < Furthest + 0.3) ++Near;
			// (the points a line may lie over: its top layer's, and a little round its edges, short of its seams' ends
			// on its hem)
			std::vector<size_t> Beneath;
			Beneath.reserve(static_cast<size_t>(std::min(Ringed, Near + 1) * Cols));
			for (int R = 0; R < std::min(Ringed, Near + 1); ++R)
				for (int Col = 0; Col < Cols; ++Col)
					if (Cl[static_cast<size_t>(Col)].Cs < 0.2 && (R > 0 || Col % 4 != 0)) Beneath.push_back(static_cast<size_t>(R * Cols + Col));
			for (int Sm = 0; Sm < Seams; ++Sm)
			{
				if (!LineBack(static_cast<size_t>(Sm), static_cast<size_t>(Seams))) continue;
				const V3 E = Ends[static_cast<size_t>(Sm)];
				const V3 Way = Minus(E, Hn);
				const double Size = Length(Way), Flat = std::max(1e-9, std::hypot(Way.X, Way.Y));
				const double Bow = (Sm % 2 == 0 ? 1.0 : -1.0) * 0.06 * Size;
				const V3 Mid{ 0.5 * (Hn.X + E.X) - Way.Y / Flat * Bow, 0.5 * (Hn.Y + E.Y) + Way.X / Flat * Bow, std::min(Hn.Z, E.Z) };
				for (int Seg = 0; Seg < 2; ++Seg)
				{
					const V3& A = Seg == 0 ? Hn : Mid;
					const V3& B = Seg == 0 ? Mid : E;
					const double Dx = B.X - A.X, Dy = B.Y - A.Y, Dd = std::max(1e-12, Dx * Dx + Dy * Dy);
					const double X0 = std::min(A.X, B.X) - 0.25, X1 = std::max(A.X, B.X) + 0.25;
					for (const size_t P : Beneath)
					{
						V3& Q = Lies[P];
						if (Q.X < X0 || Q.X > X1) continue;
						const double Ex = Q.X - E.X, Ey = Q.Y - E.Y;
						if (Ex * Ex + Ey * Ey < 0.0016) continue;
						const double T = std::clamp(((Q.X - A.X) * Dx + (Q.Y - A.Y) * Dy) / Dd, 0.0, 1.0);
						const double Ax2 = Q.X - A.X - T * Dx, Ay2 = Q.Y - A.Y - T * Dy, D2 = Ax2 * Ax2 + Ay2 * Ay2;
						if (D2 > 0.0625) continue;
						const double Cap = A.Z + T * (B.Z - A.Z) - 0.018 + 0.3 * std::max(0.0, std::sqrt(D2) - 0.025);
						if (Q.Z > Cap) Q.Z = std::max(Cap, Floor[P]);
					}
				}
			}
		}
		for (int R = 0; R < Ringed; ++R)
		{
			for (int Col = 0; Col < Cols; ++Col)
			{
				const Column& Q = Cl[static_cast<size_t>(Col)];
				const size_t P = static_cast<size_t>(R * Cols + Col);
				Out.Points[P] = { Was[P].X + (Lies[P].X - Was[P].X) * Stream, Was[P].Y + (Lies[P].Y - Was[P].Y) * Stream + 0.5 * Flap[P] * Q.Sn, 0.0 };
			}
		}
		{
			// (its crown: as it falls, the top of its gores; as it lies, at its far end's tip, on the ground, its round
			// coming down to it, toward the side its far end slumps to and drawn over that way with its vent; coming down
			// and billowing as its last ring does on the whole)
			const size_t P = Each - 1;
			const double Rz = Skirt - Pivot + Dz * (1.0 - 0.6 * Sag);
			Was[P] = { Rz * TipS, 0.0, Pivot + Rz * TipC };
			Lies[P] = { Nx + L.Long, L.FarSide + Kink + 0.5 * LeanOver + (Lean > 0.0 ? 1.0 : -1.0) * L.Vent * 0.5 * L.Wide, Clear };
			Settled[P] = Smooth((K - 0.06 - 0.14 - 0.06) / 0.6);
			Flap[P] = Sag * (1.0 - Settled[P]) * 0.3 * std::sin(2.0 * Pi * (1.6 - 1.1 * K + L.Loose));
			Out.Points[P] = { Was[P].X + (Lies[P].X - Was[P].X) * Stream, Was[P].Y + (Lies[P].Y - Was[P].Y) * Stream, 0.0 };
		}
		// The ground under it, once (none: a flat field), and each point's height: from as it falls down onto it as it
		// lies over it, flapping, never nearer it than Clear; then each edge kept over it (its ends lifted where the ground
		// between them stands higher than the edge would, a bank's or a rim's crest: the nearer end the more).
		const V3 Fw = Ahead(C.HeapHeading), Rt = RightOf(C.HeapHeading);
		UnderGrid G;
		if (Ground)
		{
			// (and to his harness, where its lines run back to him across the ground)
			double MinX = Hn.X, MinY = Hn.Y, MaxX = Hn.X, MaxY = Hn.Y;
			for (size_t P = 0; P < Each; ++P)
			{
				for (const V3* Q : { &Out.Points[P], &Lies[P] })
				{
					MinX = std::min(MinX, Q->X); MaxX = std::max(MaxX, Q->X);
					MinY = std::min(MinY, Q->Y); MaxY = std::max(MaxY, Q->Y);
				}
			}
			G.Read(MinX, MinY, MaxX, MaxY, [&](double X, double Y) { return Ground(C.Landed.X + Fw.X * X + Rt.X * Y, C.Landed.Y + Fw.Y * X + Rt.Y * Y) - C.Landed.Z; });
		}
		for (size_t P = 0; P < Each; ++P)
		{
			V3& At = Out.Points[P];
			const double LiesZ = Lies[P].Z + G.Hug(Lies[P].X, Lies[P].Y);
			At.Z = std::max(Was[P].Z + (LiesZ - Was[P].Z) * Settled[P] + Flap[P], G.Hug(At.X, At.Y) + Clear);
		}
		if (Ground)
		{
			// (an edge wholly over the highest the grid reads, as a canopy still high in the air, is left as it is. Each
			// edge's shortfall is worked out from its points as they were laid, not as its other edges have lifted them,
			// and shared between its ends by how near each is to it: the least lift that clears it, short by S at T along
			// the edge, is S (1 - T) / ((1 - T)^2 + T^2) at its near end and S T / the same at its far end, S at both for a
			// shortfall at its middle. Each point is then lifted once, by the most any one of its edges asks of it, and
			// every edge is still kept over the ground, as lifting either end more only lifts it more. A review: lifting both
			// ends of each edge by all its shortfall, edge after edge, had lifted the tip of its crown, an end of every edge
			// of its last ring's fan, once for each of them: lying along a bank under one edge it had stood up to 6.7 m over
			// the ground like a tent pole, and the silk round a hedgerow bank's crest had floated up to 2.4 m over it)
			const double Over = G.Most + Clear;
			std::vector<double> Lift(Each, 0.0);
			auto Keep = [&Out, &G, &Lift, Clear, Over](size_t A, size_t B)
			{
				const V3& Pa = Out.Points[A];
				const V3& Pb = Out.Points[B];
				if (Pa.Z >= Over && Pb.Z >= Over) return;
				const int Steps = 2 + static_cast<int>(std::hypot(Pb.X - Pa.X, Pb.Y - Pa.Y) / (0.5 * G.Step));
				double La = 0.0, Lb = 0.0;
				for (int S = 1; S < Steps; ++S)
				{
					const double T = static_cast<double>(S) / Steps;
					const double Short = G.Hug(Pa.X + (Pb.X - Pa.X) * T, Pa.Y + (Pb.Y - Pa.Y) * T) + Clear - (Pa.Z + (Pb.Z - Pa.Z) * T);
					if (Short <= 0.0) continue;
					const double Share = Short / ((1.0 - T) * (1.0 - T) + T * T);
					La = std::max(La, Share * (1.0 - T));
					Lb = std::max(Lb, Share * T);
				}
				Lift[A] = std::max(Lift[A], La);
				Lift[B] = std::max(Lift[B], Lb);
			};
			for (int R = 0; R < Ringed; ++R)
			{
				for (int Col = 0; Col < Cols; ++Col)
				{
					const size_t A = static_cast<size_t>(R * Cols + Col), B = static_cast<size_t>(R * Cols + (Col + 1 == Cols ? 0 : Col + 1));
					const size_t Ua = R + 1 < Ringed ? A + static_cast<size_t>(Cols) : Each - 1, Ub = R + 1 < Ringed ? B + static_cast<size_t>(Cols) : Each - 1;
					Keep(A, B);
					Keep(A, Ua);
					if (Ub != Ua) Keep(A, Ub);
				}
			}
			for (size_t P = 0; P < Each; ++P) Out.Points[P].Z += Lift[P];
		}
		Out.Facings.assign(Each, { 0.0, 0.0, 1.0 });
		Out.Colours.resize(Each * 3);
		{
			double* Cs = Out.Colours.data();
			for (int R = 0; R < Ringed; ++R)
				for (int Col = 0; Col < Cols; ++Col)
					for (int Ch = 0; Ch < 3; ++Ch) *Cs++ = C.Silk[Ch] * Hemmed(R, Col);
			for (int Ch = 0; Ch < 3; ++Ch) *Cs++ = C.Silk[Ch];
		}
		// (each point's facing from where the points about it are: round its ring by up it toward its crown, by the rings
		// either side of it. Where a fold across it is (the art director's note of 24 November 17:40Z), once it streams
		// out, up it as the silk runs on from it to the next ring: those folds stand two rings apart or more, and by the
		// rings either side of each, a crest's and a trough's alike, they shade flat; so there a trough faces up its
		// fold's near side and a crest down its far side, each fold a darker band and a lighter one. And its flipped
		// gores' fold, a roll of silk at its near edge, faces out of the roll, back toward him, so that its silk either
		// side of it, one layer turned over onto the other, is each wound its own way)
		const auto Pt = [&Out, Cols](int R, int Col) -> const V3& { return Out.Points[static_cast<size_t>(R * Cols + (Col < 0 ? Col + Cols : Col >= Cols ? Col - Cols : Col))]; };
		const V3 Tip2 = Out.Points[Each - 1];
		for (int R = 0; R < Ringed; ++R)
		{
			for (int Col = 0; Col < Cols; ++Col)
			{
				const Column& Q = Cl[static_cast<size_t>(Col)];
				const size_t P = static_cast<size_t>(R * Cols + Col);
				double Ahead = Crossing[P];
				if (R > 0) Ahead = std::max(Ahead, Crossing[P - static_cast<size_t>(Cols)]);
				if (R + 1 < Ringed) Ahead = std::max(Ahead, Crossing[P + static_cast<size_t>(Cols)]);
				Ahead *= Stream;
				const V3& Here = Pt(R, Col);
				const V3 Way = Minus(Pt(R, Col + 1), Pt(R, Col - 1));
				const V3 Up = Plus(Minus(R + 1 < Ringed ? Pt(R + 1, Col) : Tip2, Here), Times(Minus(Here, Pt(R > 0 ? R - 1 : R, Col)), 1.0 - Ahead));
				V3 Facing = Unit(Cross(Way, Up), { 0.0, 0.0, 1.0 });
				const double Rolled = R == 1 ? Q.Flip * Stream : 0.0;
				if (Rolled > 0.0) Facing = Unit(Plus(Times(Facing, 1.0 - Rolled), Times(V3{ -0.97, 0.0, 0.25 }, Rolled)), Facing);
				Out.Facings[P] = Facing;
			}
		}
		V3 Sum;
		for (int Col = 0; Col < Cols; ++Col) Sum = Plus(Sum, Cross(Minus(Pt(Ringed - 1, Col), Tip2), Minus(Pt(Ringed - 1, Col + 1), Tip2)));
		Out.Facings[Each - 1] = Unit(Sum, { 0.0, 0.0, 1.0 });
		Sew(Out, Ringed, Cols);
		Out.Kept.Key.assign(KeyNow, KeyNow + std::size(KeyNow));
		Out.Kept.Lies.swap(Lies);
		Out.Kept.Crossing.swap(Crossing);
		// (its columns as kept, handed back as they came when kept: none of them is changed after they are worked out)
		if (bKept) Out.Kept.Columns.swap(KeptWas.Columns);
		else
		{
			Out.Kept.Columns.resize(PerColumn * Cl.size());
			std::memcpy(Out.Kept.Columns.data(), static_cast<const void*>(Cl.data()), sizeof(Column) * Cl.size());
		}
		// (the lines to its gathered hem: the lowest each one's middle may lie (LineLies), lying: high enough that both its
		// lengths are Rope (the 12 mm cord's half and 3 mm) over the ground under them, bowed or straight (a review: on a
		// slope falling away downwind a line had gone some 0.3 m into it))
		Out.Across.clear();
		Out.Over.clear();
		Out.MidLow.assign(static_cast<size_t>(Seams), -1e9);
		// (and the ground those across it are kept over from his harness, and while they are not yet slack (LineGround):
		// read where they are, but never more than 0.1 m under the grid's. The grid's allowance for a crest between its
		// points (UnderGrid) stands a bank 1 m wide 0.6 to 0.8 m higher than it is, and a ditch's lip 0.4 m: lifted over
		// that, five times a rise from his harness and the more the less slack they are, his lines had still been flung
		// up to 4 m off a bank (a third review). The 0.1 m keeps them clear as they move between makes, as the grid's
		// allowance had: 0.05 had left one 3 cm in a ditch's lip. And the lowest those across it start from at his
		// harness (HarnessLow: LineLies))
		const auto LineGround = [&](double X, double Y)
		{
			const double Read = Ground ? Ground(C.Landed.X + Fw.X * X + Rt.X * Y, C.Landed.Y + Fw.Y * X + Rt.Y * Y) - C.Landed.Z : 0.0;
			return std::min(G.At(X, Y), Read + 0.1);
		};
		Out.HarnessLow = LineGround(Hn.X, Hn.Y) + Rope;
		for (int Sm = 0; Sm < Seams; ++Sm)
		{
			if (!LineBack(static_cast<size_t>(Sm), static_cast<size_t>(Seams))) continue;
			const V3& E = Out.Points[static_cast<size_t>(4 * Sm)];
			const V3 Way = Minus(E, Hn);
			const double Flat = std::max(1e-9, std::hypot(Way.X, Way.Y)), Bow = (Sm % 2 == 0 ? 1.0 : -1.0) * 0.06 * Length(Way);
			double Low = -1e9;
			for (const double Aside : { 0.0, Bow })
			{
				const V3 Mid{ 0.5 * (Hn.X + E.X) - Way.Y / Flat * Aside, 0.5 * (Hn.Y + E.Y) + Way.X / Flat * Aside, 0.0 };
				Low = std::max(Low, G.At(Mid.X, Mid.Y) + Rope);
				for (int Q = 1; Q < 8 && Ground; ++Q)
				{
					const double T = Q / 8.0;
					for (const V3* End : { &Hn, &E })
					{
						const double X = End->X + (Mid.X - End->X) * T, Y = End->Y + (Mid.Y - End->Y) * T, Z = std::max(End->Z, G.At(End->X, End->Y) + Rope);
						Low = std::max(Low, Z + (G.At(X, Y) + Rope - Z) / T);
					}
				}
			}
			Out.MidLow[static_cast<size_t>(Sm)] = Low;
		}
		// (its lines across it as it now is (her note of 24 November 17:40Z, (3); and the reviews: they had sunk into its
		// folds as it settled, then risen through them at its last make, floated over its folds, and ended buried under its
		// edge). Each strand's path on its silk as it now is, laid over the top of whatever silk lies there (TopAt: a fold
		// over it, a flipped gore, its under layer; within 0.4 m over it, not its far side still standing over it as it
		// falls in), Rise over that, so that it lies on its silk and moves with it as it falls in, never in it. Drawn over
		// its silk as its lines go slack (LineSlack: taut, none of it, at its seam's end on its hem; slack, from its edge
		// where it comes onto its top to its other edge), LineSilk points evenly along it; over its far edge where its silk
		// ends, and down to the ground 4 cm beyond. And from his harness as he lies across the grass to 4 cm outside where
		// its silk ends at its first edge, bowed out by 0.07 of its length (Clear2 kept it clear of its near part), lying on
		// the ground but where it rises to him, and up onto its silk's edge there. Then where the straight between two of
		// its points would dip into its silk (over a fold's crest), the two lifted clear of it, the nearer the more; and
		// always each point Rope over the ground, each span kept over it as its silk's edges are, both its ends lifted
		// where the ground between stands higher)
		if (!Strands.empty() && LineSlack(K) <= 0.0)
		{
			// (taut: each at its seam's end on its hem, as the rest)
			for (const Strand& S : Strands)
			{
				Out.Across.push_back(S.Seam);
				Out.Over.insert(Out.Over.end(), static_cast<size_t>(LineVia), Out.Points[static_cast<size_t>(4 * S.Seam)]);
			}
		}
		else if (!Strands.empty())
		{
			const double Slack = LineSlack(K);
			const size_t Lines = Strands.size();
			// (each strand's path finely as its silk now is, from its seam's end on its hem round its near part to its edge
			// and across to its other edge, and how far along it each point is; how far along it its line is drawn as slack
			// as it is (over its far edge and down to the ground a nominal Drop more, all of it slack); and the way out over
			// each of those edges)
			constexpr double OutTo = 0.6, CellX = 0.15, CellY = 0.05, Near = 0.003, Above2 = 0.4, Drop = 0.3;
			std::vector<V3> Fine(Lines * static_cast<size_t>(PathFine)), Ways(Lines * 2);
			std::vector<double> FineArc(Fine.size(), 0.0);
			std::vector<int> Upto(Lines, PathFine), Start(Lines, 0);
			double Lo[2] = { std::min(1e9, Hn.X), std::min(1e9, Hn.Y) }, Hi[2] = { std::max(-1e9, Hn.X), std::max(-1e9, Hn.Y) }, Farthest = 0.0;
			for (size_t Ln = 0; Ln < Lines; ++Ln)
			{
				const Strand& S = Strands[Ln];
				V3* F = &Fine[Ln * PathFine];
				double* Fa = &FineArc[Ln * PathFine];
				double Rs[PathFine];
				for (int I = 0; I < PathFine; ++I)
				{
					double Cf = 0.0;
					PathAt(S, PathT(I), Rs[I], Cf);
					F[I] = SurfAt(Out.Points, nullptr, Ringed, Cols, Rs[I], Cf, nullptr);
				}
				for (int I = 1; I < PathFine; ++I) Fa[I] = Fa[I - 1] + Length(Minus(F[I], F[I - 1]));
				const double Reach2 = (Fa[PathFine - 1] + Drop) * Slack, From2 = Fa[PathNear] * Slack;
				int& U = Upto[Ln];
				U = PathNear + 1;
				while (U < PathFine && Fa[U - 1] <= Reach2) ++U;
				int& I0 = Start[Ln];
				while (I0 + 1 < PathNear && Fa[I0 + 1] <= From2) ++I0;
				for (int I = I0; I < U; ++I)
				{
					Lo[0] = std::min(Lo[0], F[I].X); Hi[0] = std::max(Hi[0], F[I].X);
					Lo[1] = std::min(Lo[1], F[I].Y); Hi[1] = std::max(Hi[1], F[I].Y);
					Farthest = std::max(Farthest, Rs[I]);
				}
				EntryOut(Out.Points, S, Ways[2 * Ln]);
				EdgeOut(Out.Points, S.Re, S.Se, Ways[2 * Ln + 1]);
			}
			Lo[0] -= OutTo; Lo[1] -= OutTo; Hi[0] += OutTo; Hi[1] += OutTo;
			// (its triangles facing up over its rings as far as its lines are drawn and three on, both its faces, each in the
			// squares of a 15 by 5 cm grid over where its lines lie that it reaches over or near: those its paths cross as far
			// as they are drawn, those out over their edges, those its lines cross from his harness, and those between where
			// it is drawn onto its silk from and its first edge)
			constexpr double PerX = 1.0 / CellX, PerY = 1.0 / CellY;
			const int Gx = static_cast<int>((Hi[0] - Lo[0]) * PerX) + 1, Gy = static_cast<int>((Hi[1] - Lo[1]) * PerY) + 1;
			const auto CellOf = [&](double X, double Y) { return std::clamp(static_cast<int>((Y - Lo[1]) * PerY), 0, Gy - 1) * Gx + std::clamp(static_cast<int>((X - Lo[0]) * PerX), 0, Gx - 1); };
			std::vector<unsigned char> Used(static_cast<size_t>(Gx * Gy), 0);
			constexpr double Wide2 = 0.02;
			const auto MarkTo = [&](double X0, double Y0, double X1, double Y1)
			{
				// (and those within Wide2 of it across: its line drawn straight between its points, off it a little)
				const int Subs = std::max(1, static_cast<int>(std::hypot(X1 - X0, Y1 - Y0) / CellY));
				for (int Q = 0; Q <= Subs; ++Q)
				{
					const double T = Q / static_cast<double>(Subs), X = X0 + (X1 - X0) * T, Y = Y0 + (Y1 - Y0) * T;
					for (const double Dy : { -Wide2, Wide2 }) Used[static_cast<size_t>(CellOf(X, Y + Dy))] = 1;
					Used[static_cast<size_t>(CellOf(X, Y))] = 1;
				}
			};
			for (size_t Ln = 0; Ln < Lines; ++Ln)
			{
				const V3* F = &Fine[Ln * PathFine];
				for (int I = Start[Ln]; I + 1 < Upto[Ln]; ++I) MarkTo(F[I].X, F[I].Y, F[I + 1].X, F[I + 1].Y);
				if (Upto[Ln] == PathFine)
				{
					const V3& From = F[PathFine - 1];
					const V3& Way = Ways[2 * Ln + 1];
					MarkTo(From.X, From.Y, From.X + Way.X * (OutTo + 0.05), From.Y + Way.Y * (OutTo + 0.05));
				}
				const V3& In = F[PathNear];
				V3 GrassWas = Hn;
				for (int Q = 1; Q <= 12; ++Q)
				{
					// (and over its last third 6 cm either side: it is drawn to where it comes off its silk, not quite there)
					const V3 B = GrassAt(Hn, In, Strands[Ln].Si, Q / 12.0);
					MarkTo(GrassWas.X, GrassWas.Y, B.X, B.Y);
					for (const double Dy : { -0.06, 0.06 })
						if (Q > 8) MarkTo(GrassWas.X, GrassWas.Y + Dy, B.X, B.Y + Dy);
					GrassWas = B;
				}
				// (and from where it is drawn onto its silk from, between its hem and where it comes on, out that way as far as
				// it is slack; and all the way out from where it comes on)
				const V3& Way0 = Ways[2 * Ln];
				const double Out3 = std::min(OutTo + 0.05, (OutTo + 0.05) * Slack + 0.1);
				MarkTo(In.X, In.Y, In.X + Way0.X * (OutTo + 0.05), In.Y + Way0.Y * (OutTo + 0.05));
				for (int I = Start[Ln]; I <= std::min(Start[Ln] + 1, PathNear - 1); ++I)
				{
					const int Along2 = std::max(1, static_cast<int>(std::hypot(F[I + 1].X - F[I].X, F[I + 1].Y - F[I].Y) / CellY));
					for (int U = 0; U < Along2; ++U)
					{
						const double Fx = F[I].X + (F[I + 1].X - F[I].X) * U / Along2, Fy = F[I].Y + (F[I + 1].Y - F[I].Y) * U / Along2;
						const double Tx = In.X + Way0.X * Out3, Ty = In.Y + Way0.Y * Out3;
						MarkTo(Fx, Fy, Fx + (Tx - Fx) * Slack, Fy + (Ty - Fy) * Slack);
					}
				}
			}
			// (how many of its squares are marked up to each: a box's count at a glance)
			std::vector<int> Marked(static_cast<size_t>((Gx + 1) * (Gy + 1)), 0);
			for (int J = 0; J < Gy; ++J)
				for (int I = 0; I < Gx; ++I)
					Marked[static_cast<size_t>((J + 1) * (Gx + 1) + I + 1)] = Used[static_cast<size_t>(J * Gx + I)] + Marked[static_cast<size_t>(J * (Gx + 1) + I + 1)]
						+ Marked[static_cast<size_t>((J + 1) * (Gx + 1) + I)] - Marked[static_cast<size_t>(J * (Gx + 1) + I)];
			// (whether any of a row's squares from I0 to I1 is marked)
			const auto RowHas = [&Marked, Gx](int J, int I0, int I1)
			{
				const int* M = Marked.data();
				return M[(J + 1) * (Gx + 1) + I1 + 1] - M[(J + 1) * (Gx + 1) + I0] - M[J * (Gx + 1) + I1 + 1] + M[J * (Gx + 1) + I0] > 0;
			};
			// (its quads, each its two triangles: where a point is on one, its corners' shares of it; how far out of it each
			// share may go and the point still be within Near of it; and its height there, never over its highest corner)
			struct Side3
			{
				double X0, X1, Y0, Y1, Ax, Ay, Wb[2], Wc[2], Least[3], Z0, Sx, Sy, Highest;
			};
			struct Flat
			{
				double X0, X1, Y0, Y1, Highest, Band;
				Side3 Sides[2];
				int Planes;
			};
			std::vector<Flat> Flats;
			std::vector<int> Starts(static_cast<size_t>(Gx * Gy + 1), 0), Items;
			{
				struct Spans
				{
					int I0, I1, J0, J1;
				};
				std::vector<Spans> Found;
				const int Rings2 = std::min(Ringed - 1, static_cast<int>(Farthest) + 3);
				Flats.reserve(static_cast<size_t>(Rings2 * Cols / 2));
				Found.reserve(static_cast<size_t>(Rings2 * Cols / 2));
				// (its outer face's, either way up: each triangle's two faces lie together, one of them facing up; and where
				// each of its points falls on the grid)
				const int* Tr = Out.Triangles.data();
				const size_t Verts = static_cast<size_t>((Rings2 + 1) * Cols);
				std::vector<double> OnGrid(2 * Verts);
				for (size_t P = 0; P < Verts; ++P)
				{
					OnGrid[2 * P] = (Out.Points[P].X - Lo[0]) * PerX;
					OnGrid[2 * P + 1] = (Out.Points[P].Y - Lo[1]) * PerY;
				}
				for (size_t Qd = 0; Qd < static_cast<size_t>(Rings2 * Cols); ++Qd)
				{
					// (Sew's two a quad: its corners A, B, B on and A on)
					const int* T2 = Tr + 6 * Qd;
					const size_t Ia = static_cast<size_t>(T2[0]), Ib = static_cast<size_t>(T2[1]), Ic = static_cast<size_t>(T2[2]), Id = static_cast<size_t>(T2[5]);
					const double Gx0 = std::min({ OnGrid[2 * Ia], OnGrid[2 * Ib], OnGrid[2 * Ic], OnGrid[2 * Id] }) - Near * PerX;
					const double Gx1 = std::max({ OnGrid[2 * Ia], OnGrid[2 * Ib], OnGrid[2 * Ic], OnGrid[2 * Id] }) + Near * PerX;
					const double Gy0 = std::min({ OnGrid[2 * Ia + 1], OnGrid[2 * Ib + 1], OnGrid[2 * Ic + 1], OnGrid[2 * Id + 1] }) - Near * PerY;
					const double Gy1 = std::max({ OnGrid[2 * Ia + 1], OnGrid[2 * Ib + 1], OnGrid[2 * Ic + 1], OnGrid[2 * Id + 1] }) + Near * PerY;
					if (Gx1 < 0.0 || Gy1 < 0.0 || Gx0 > Gx || Gy0 > Gy) continue;
					const int I0 = std::max(0, static_cast<int>(Gx0)), I1 = std::min(Gx - 1, static_cast<int>(Gx1));
					int J0 = std::max(0, static_cast<int>(Gy0)), J1 = std::min(Gy - 1, static_cast<int>(Gy1));
					while (J0 <= J1 && !RowHas(J0, I0, I1)) ++J0;
					while (J1 >= J0 && !RowHas(J1, I0, I1)) --J1;
					if (J0 > J1) continue;
					Flat F;
					F.Planes = 0;
					F.X0 = F.Y0 = 1e9;
					F.X1 = F.Y1 = F.Highest = -1e9;
					for (int Sd = 0; Sd < 2; ++Sd)
					{
						const size_t Ja = static_cast<size_t>(T2[3 * Sd]), Jb = static_cast<size_t>(T2[3 * Sd + 1]), Jc = static_cast<size_t>(T2[3 * Sd + 2]);
						if (Out.Facings[Ja].Z + Out.Facings[Jb].Z + Out.Facings[Jc].Z == 0.0) continue;
						const V3& Pa = Out.Points[Ja];
						const V3& Pb = Out.Points[Jb];
						const V3& Pc = Out.Points[Jc];
						const double D = (Pb.X - Pa.X) * (Pc.Y - Pa.Y) - (Pc.X - Pa.X) * (Pb.Y - Pa.Y);
						if (std::fabs(D) < 1e-12) continue;
						const double Bx = Pb.X - Pa.X, By = Pb.Y - Pa.Y, Cx = Pc.X - Pa.X, Cy = Pc.Y - Pa.Y;
						const double Bz = Pb.Z - Pa.Z, Cz = Pc.Z - Pa.Z, Nx3 = By * Cz - Bz * Cy, Ny3 = Bz * Cx - Bx * Cz;
						// (none round a steep one, over 63 degrees: its height run on past its edge stands far over the silk
						// beside it, where its layers gather at its hem and at their edges, and its lines had floated there)
						const bool bSteep = Nx3 * Nx3 + Ny3 * Ny3 > 4.0 * D * D;
						const double Per = 1.0 / D, Out2 = bSteep ? 0.0 : -Near * std::fabs(Per);
						Side3& Pl = F.Sides[F.Planes++];
						Pl.Ax = Pa.X; Pl.Ay = Pa.Y;
						Pl.Wb[0] = Cy * Per; Pl.Wb[1] = -Cx * Per;
						Pl.Wc[0] = -By * Per; Pl.Wc[1] = Bx * Per;
						Pl.Least[0] = Out2 * std::sqrt((Cx - Bx) * (Cx - Bx) + (Cy - By) * (Cy - By));
						Pl.Least[1] = Out2 * std::sqrt(Cx * Cx + Cy * Cy);
						Pl.Least[2] = Out2 * std::sqrt(Bx * Bx + By * By);
						Pl.Z0 = Pa.Z;
						Pl.Sx = (Pb.Z - Pa.Z) * Pl.Wb[0] + (Pc.Z - Pa.Z) * Pl.Wc[0];
						Pl.Sy = (Pb.Z - Pa.Z) * Pl.Wb[1] + (Pc.Z - Pa.Z) * Pl.Wc[1];
						Pl.Highest = std::max({ Pa.Z, Pb.Z, Pc.Z });
						Pl.X0 = std::min({ Pa.X, Pb.X, Pc.X }) - Near; Pl.X1 = std::max({ Pa.X, Pb.X, Pc.X }) + Near;
						Pl.Y0 = std::min({ Pa.Y, Pb.Y, Pc.Y }) - Near; Pl.Y1 = std::max({ Pa.Y, Pb.Y, Pc.Y }) + Near;
						F.X0 = std::min(F.X0, Pl.X0); F.X1 = std::max(F.X1, Pl.X1);
						F.Y0 = std::min(F.Y0, Pl.Y0); F.Y1 = std::max(F.Y1, Pl.Y1);
						F.Highest = std::max(F.Highest, Pl.Highest);
					}
					if (F.Planes == 0) continue;
					Flats.push_back(F);
					Found.push_back({ I0, I1, J0, J1 });
					for (int J = J0; J <= J1; ++J)
					{
						if (!RowHas(J, I0, I1)) continue;
						for (int I = I0; I <= I1; ++I) Starts[static_cast<size_t>(J * Gx + I + 1)] += Used[static_cast<size_t>(J * Gx + I)];
					}
				}
				for (size_t Q = 1; Q < Starts.size(); ++Q) Starts[Q] += Starts[Q - 1];
				Items.resize(static_cast<size_t>(Starts.back()));
				// (each square's, highest first, in 64 bands of height (Band: the top of its own), so that a point's search
				// stops at the first band no higher than what it has found)
				double Zlo = 1e9, Zhi = -1e9;
				for (const Flat& F : Flats)
				{
					Zlo = std::min(Zlo, F.Highest);
					Zhi = std::max(Zhi, F.Highest);
				}
				const double Tall = std::max(1e-6, (Zhi - Zlo) / 63.0);
				int Banded[65] = {};
				std::vector<int> Order(Flats.size()), BandOf(Flats.size());
				for (size_t F = 0; F < Flats.size(); ++F)
				{
					const int B = std::clamp(static_cast<int>((Zhi - Flats[F].Highest) / Tall), 0, 63);
					Flats[F].Band = Zhi - B * Tall + 1e-9;
					BandOf[F] = B;
					++Banded[B + 1];
				}
				for (int B = 1; B < 65; ++B) Banded[B] += Banded[B - 1];
				for (size_t F = 0; F < Flats.size(); ++F) Order[static_cast<size_t>(Banded[BandOf[F]]++)] = static_cast<int>(F);
				std::vector<int> Next(Starts.begin(), Starts.end() - 1);
				for (const int Fo : Order)
				{
					const Spans& Sp = Found[static_cast<size_t>(Fo)];
					for (int J = Sp.J0; J <= Sp.J1; ++J)
					{
						if (!RowHas(J, Sp.I0, Sp.I1)) continue;
						for (int I = Sp.I0; I <= Sp.I1; ++I)
							if (Used[static_cast<size_t>(J * Gx + I)]) Items[static_cast<size_t>(Next[static_cast<size_t>(J * Gx + I)]++)] = Fo;
					}
				}
			}
			// (the top of its silk at a point: the highest of it there or within Near of it, no more than Above2 over Below;
			// none, -1e9; or, asked only whether it is any higher than Least, Least for no)
			const auto TopAt = [&](double X, double Y, double Below, double Least = -1e9)
			{
				if (X < Lo[0] || X > Hi[0] || Y < Lo[1] || Y > Hi[1]) return Least;
				const int Cn = CellOf(X, Y);
				double Best = Least;
				for (int Q = Starts[static_cast<size_t>(Cn)]; Q < Starts[static_cast<size_t>(Cn + 1)]; ++Q)
				{
					const Flat& F = Flats[static_cast<size_t>(Items[static_cast<size_t>(Q)])];
					if (F.Band < Best) break;
					if (X < F.X0 || X > F.X1 || Y < F.Y0 || Y > F.Y1) continue;
					for (int Sd = 0; Sd < F.Planes; ++Sd)
					{
						const Side3& Pl = F.Sides[Sd];
						if (X < Pl.X0 || X > Pl.X1 || Y < Pl.Y0 || Y > Pl.Y1) continue;
						const double Dx = X - Pl.Ax, Dy = Y - Pl.Ay;
						const double Wb = Pl.Wb[0] * Dx + Pl.Wb[1] * Dy, Wc = Pl.Wc[0] * Dx + Pl.Wc[1] * Dy;
						if (Wb < Pl.Least[1] || Wc < Pl.Least[2] || 1.0 - Wb - Wc < Pl.Least[0]) continue;
						const double Z = std::min(Pl.Highest, Pl.Z0 + Pl.Sx * Dx + Pl.Sy * Dy);
						if (Z <= Below + Above2) Best = std::max(Best, Z);
					}
				}
				return Best;
			};
			Out.Over.resize(Lines * static_cast<size_t>(LineVia));
			const double Hh = std::max(0.0, Hn.Z - G.At(Hn.X, Hn.Y));
			// (where the straights from his harness are kept over the ground from: his harness, or the ground over it where he
			// lies sunk into a bank or a hummock (HarnessLow, as LineLies draws them taut from). The man is drawn flat at his
			// feet's height, his harness up to 0.37 m in such ground: from there the point next to it had been lifted by five
			// times that or more, and a line across its silk had stood up 5 to 7 m off the grass, a pole (a third review). The
			// straight from his harness itself comes up out of the ground by him)
			const V3 Hl{ Hn.X, Hn.Y, std::max(Hn.Z, Out.HarnessLow) };
			// (along the straight from A to B, each 2 cm: the top of its silk there and Rise over it, or the ground and Rope
			// over it; where its silk ends or begins between two (found to 3 mm), over its edge 1.5 cm out from it, as a
			// cord's thickness hangs off it, and on the ground 3 cm out, none of the ground's nearer; added to Pts, A not,
			// and, should Stop, only to where its silk first ends)
			// (finely as it settles and lies, more coarsely as it falls in: its silk moving, its lines not yet lying on it)
			const bool bFine = K >= 0.75 - 1e-9;
			const double Pace = bFine ? 0.02 : 0.04;
			const auto Drape = [&](const V3& A, const V3& B, bool bStop, std::vector<V3>& Pts)
			{
				const double Dx = B.X - A.X, Dy = B.Y - A.Y, Len = std::hypot(Dx, Dy);
				if (Len < 1e-6) return;
				const int N = std::max(1, static_cast<int>(std::ceil(Len / Pace)));
				const auto TopOn = [&](double T) { return TopAt(A.X + Dx * T, A.Y + Dy * T, A.Z + (B.Z - A.Z) * T); };
				const auto Ground2 = [&](double T) { const double X = A.X + Dx * T, Y = A.Y + Dy * T; return V3{ X, Y, G.At(X, Y) + Rope }; };
				const size_t Mine = Pts.size();
				double WasT = 0.0, WasTop = TopOn(0.0), Clear3 = -1.0;
				// (one out from its silk (Stop) starts on it, Rise over its top, though its top be not found there: TopAt
				// passes over its steepest triangles, as its skirt stands falling in. Else all of it had been walked as ground,
				// and its line come down OutTo out from its edge)
				if (bStop && WasTop < -1e8) WasTop = A.Z - Rise;
				for (int Q = 1; Q <= N; ++Q)
				{
					const double T = static_cast<double>(Q) / N;
					double Top = TopOn(T);
					// (a crack between its layers where it folds over, not its edge: silk again within 4 cm)
					for (int Ahead = 1; Ahead <= 2 && Top < -1e8 && WasTop > -1e8; ++Ahead) Top = TopOn(T + 0.02 * Ahead / Len);
					if ((Top > -1e8) != (WasTop > -1e8))
					{
						const bool bOff = WasTop > -1e8;
						double In = bOff ? WasT : T, Out3 = bOff ? T : WasT, TopIn = bOff ? WasTop : Top;
						for (int Halve = 0; Halve < 3; ++Halve)
						{
							const double M = 0.5 * (In + Out3), Tm = TopOn(M);
							if (Tm > -1e8) { In = M; TopIn = Tm; }
							else Out3 = M;
						}
						const double Edge = 0.5 * (In + Out3), Way = (bOff ? 1.0 : -1.0) / Len;
						const V3 Lip{ A.X + Dx * (Edge + 0.015 * Way), A.Y + Dy * (Edge + 0.015 * Way), TopIn + Rise };
						if (bOff)
						{
							Pts.push_back(Lip);
							Pts.push_back(Ground2(Edge + 0.03 * Way));
							if (bStop) return;
							Clear3 = Edge + 0.03 * Way;
						}
						else
						{
							while (Pts.size() > Mine && (Pts.back().X - A.X) * Dx + (Pts.back().Y - A.Y) * Dy > (Edge - 0.03 / Len) * Len * Len) Pts.pop_back();
							if (Edge - 0.03 / Len > 0.0) Pts.push_back(Ground2(Edge - 0.03 / Len));
							Pts.push_back(Lip);
						}
					}
					if (Top > -1e8) Pts.push_back({ A.X + Dx * T, A.Y + Dy * T, Top + Rise });
					else if (T > Clear3) Pts.push_back(Ground2(T));
					WasT = T;
					WasTop = Top;
				}
			};
			std::vector<V3> Route, Head, Cand;
			std::vector<double> Arc, CandArc;
			Route.reserve(PathFine + 40);
			Cand.reserve(PathFine + 80);
			constexpr int Want = LineVia - LineGrass + 2;
			for (size_t Ln = 0; Ln < Lines; ++Ln)
			{
				const Strand& S = Strands[Ln];
				const V3* F = &Fine[Ln * PathFine];
				// (its route over its silk as it now is: its fine path, Rise over the top of its silk there, from its seam's end
				// on its hem round to its edge where it comes on, and across to its other edge; out over that edge to where
				// its silk ends; and down to the ground beyond)
				Route.clear();
				size_t NearAt = 0, RouteOf[PathFine] = {};
				bool Skipped[PathFine] = {};
				for (int I = Start[Ln]; I < Upto[Ln]; ++I)
				{
					if (!bFine && I > Start[Ln] + 1 && I != PathNear && I + 1 < Upto[Ln] && (I - Start[Ln]) % 2 == 1)
					{
						Skipped[I] = true;
						continue;
					}
					const V3 Here{ F[I].X, F[I].Y, TopAt(F[I].X, F[I].Y, F[I].Z, F[I].Z) + Rise };
					if (I > Start[Ln] && bFine)
					{
						// (and half-way from the last, should its silk stand higher there: a crest between, a flipped gore's fold)
						const V3& Back = Route.back();
						double R = 0.0, Cf = 0.0;
						PathAt(S, 0.5 * (PathT(I - 1) + PathT(I)), R, Cf);
						const V3 Mid = SurfAt(Out.Points, nullptr, Ringed, Cols, R, Cf, nullptr);
						const double Top = TopAt(Mid.X, Mid.Y, Mid.Z, Mid.Z) + Rise;
						if (Top > 0.5 * (Back.Z + Here.Z) + 0.003) Route.push_back({ Mid.X, Mid.Y, Top });
					}
					if (I == PathNear) NearAt = Route.size();
					RouteOf[I] = Route.size();
					Route.push_back(Here);
				}
				const size_t ExitAt = Route.size() - 1;
				const double* Fa = &FineArc[Ln * PathFine];
				const double Reach2 = (Fa[PathFine - 1] + Drop) * Slack;
				const bool bOff = Upto[Ln] == PathFine && Reach2 > Fa[PathFine - 1];
				if (bOff)
				{
					const V3 Exit = Route.back();
					Drape(Exit, Plus(Exit, Times(Ways[2 * Ln + 1], OutTo)), true, Route);
				}
				Arc.assign(Route.size(), 0.0);
				for (size_t I = 1; I < Route.size(); ++I) Arc[I] = Arc[I - 1] + Length(Minus(Route[I], Route[I - 1]));
				// (how far along its route each of its path's points comes: those passed over as it falls in, between their two)
				double ArcOf[PathFine] = {};
				for (int I = Start[Ln]; I < Upto[Ln]; ++I)
				{
					if (!Skipped[I]) ArcOf[I] = Arc[RouteOf[I]];
				}
				for (int I = Start[Ln] + 1; I + 1 < Upto[Ln]; ++I)
				{
					if (Skipped[I]) ArcOf[I] = ArcOf[I - 1] + (ArcOf[I + 1] - ArcOf[I - 1]) * (Fa[I] - Fa[I - 1]) / std::max(1e-12, Fa[I + 1] - Fa[I - 1]);
				}
				const auto RouteAt = [&](double A)
				{
					size_t I = 0;
					while (I + 2 < Route.size() && Arc[I + 1] < A) ++I;
					const double T = std::clamp((A - Arc[I]) / std::max(1e-12, Arc[I + 1] - Arc[I]), 0.0, 1.0);
					return Plus(Route[I], Times(Minus(Route[I + 1], Route[I]), T));
				};
				// (drawn over its silk as far as its lines are slack: from its seam's end toward where it comes on, and as far
				// along its route as that (over its far edge and down, a nominal Drop for all of that); and from there to the
				// grass out over its first edge, as far as that)
				double A1 = Arc.back();
				if (bOff) A1 = Arc[ExitAt] + (Arc.back() - Arc[ExitAt]) * std::min(1.0, (Reach2 - Fa[PathFine - 1]) / Drop);
				else
				{
					int I = 0;
					while (I + 2 < Upto[Ln] && Fa[I + 1] < Reach2) ++I;
					const double T = std::clamp((Reach2 - Fa[I]) / std::max(1e-12, Fa[I + 1] - Fa[I]), 0.0, 1.0);
					A1 = ArcOf[I] + (ArcOf[I + 1] - ArcOf[I]) * T;
				}
				double A0 = Arc[NearAt];
				{
					const int I = Start[Ln];
					const double T = std::clamp((Fa[PathNear] * Slack - Fa[I]) / std::max(1e-12, Fa[I + 1] - Fa[I]), 0.0, 1.0);
					A0 = std::min({ A0, A1, ArcOf[I] + (ArcOf[I + 1] - ArcOf[I]) * T });
				}
				const V3 First = RouteAt(A0), Last = RouteAt(A1);
				Head.clear();
				const V3 Entry = Route[NearAt];
				Drape(Entry, Plus(Entry, Times(Ways[2 * Ln], OutTo)), true, Head);
				const V3 Outside = Head.empty() ? Entry : Head.back();
				V3 Land = Plus(First, Times(Minus(Outside, First), Slack));
				const double LandTop = TopAt(Land.X, Land.Y, Land.Z);
				Land.Z = std::max(G.At(Land.X, Land.Y) + Rope, LandTop > -1e8 ? LandTop + Rise : -1e9);
				// (the points it may go through: from there in to where it is drawn onto its silk from, each 2 cm, on its silk's
				// top or the ground (as straight as it is taut); then its route)
				Cand.clear();
				Cand.push_back(Land);
				Drape(Land, First, false, Cand);
				if (Cand.size() > 1) Cand.pop_back();
				{
					const double Fx = First.X - Land.X, Fy = First.Y - Land.Y, F2 = std::max(1e-12, Fx * Fx + Fy * Fy);
					for (size_t I = 1; I < Cand.size(); ++I)
					{
						const double T = std::clamp(((Cand[I].X - Land.X) * Fx + (Cand[I].Y - Land.Y) * Fy) / F2, 0.0, 1.0), Taut2 = Land.Z + (First.Z - Land.Z) * T;
						Cand[I].Z += (1.0 - Slack) * std::max(0.0, Taut2 - Cand[I].Z);
					}
				}
				Cand.push_back(First);
				for (size_t I = 0; I < Route.size(); ++I)
				{
					if (Arc[I] > A0 + 1e-6 && Arc[I] < A1 - 1e-6) Cand.push_back(Route[I]);
				}
				Cand.push_back(Last);
				// (its points, from where it comes onto the grass to its end: of those, where it turns most (over a fold's crest
				// and down in its trough, up onto its edge and down off it), from its two ends, each next where the line through
				// those so far stands farthest off it, its height counting thrice; evenly along them, were there no more of
				// them than it has points)
				V3 P[LineVia];
				double Pos[Want];
				const int Nc = static_cast<int>(Cand.size());
				if (Nc <= Want)
				{
					CandArc.assign(Cand.size(), 0.0);
					for (size_t I = 1; I < Cand.size(); ++I) CandArc[I] = CandArc[I - 1] + Length(Minus(Cand[I], Cand[I - 1]));
					size_t I = 0;
					for (int J = 0; J < Want; ++J)
					{
						const double A = CandArc.back() * J / (Want - 1.0);
						while (I + 2 < Cand.size() && CandArc[I + 1] < A) ++I;
						const double T = Cand.size() < 2 ? 0.0 : std::clamp((A - CandArc[I]) / std::max(1e-12, CandArc[I + 1] - CandArc[I]), 0.0, 1.0);
						P[LineGrass - 2 + J] = Cand.size() < 2 ? Cand[0] : Plus(Cand[I], Times(Minus(Cand[I + 1], Cand[I]), T));
						Pos[J] = static_cast<double>(I) + T;
					}
				}
				else
				{
					// (each span between two kept, the one of those between that stands farthest off it, found once)
					struct Span2
					{
						int A = 0, B = 0, Best = -1;
						double Most = -1.0;
					};
					const auto Score = [&Cand](Span2& Sp)
					{
						const V3& A = Cand[static_cast<size_t>(Sp.A)];
						const V3 Ab = Minus(Cand[static_cast<size_t>(Sp.B)], A);
						const double Ab2 = std::max(1e-12, Ab.X * Ab.X + Ab.Y * Ab.Y + Ab.Z * Ab.Z);
						Sp.Best = -1;
						Sp.Most = -1.0;
						for (int I = Sp.A + 1; I < Sp.B; ++I)
						{
							const V3 Ap = Minus(Cand[static_cast<size_t>(I)], A);
							const double T = std::clamp((Ap.X * Ab.X + Ap.Y * Ab.Y + Ap.Z * Ab.Z) / Ab2, 0.0, 1.0);
							const double Ox = Ap.X - Ab.X * T, Oy = Ap.Y - Ab.Y * T, Oz = Ap.Z - Ab.Z * T;
							const double How = 9.0 * Oz * Oz + Ox * Ox + Oy * Oy + 1e-4 * Ab2;
							if (How > Sp.Most) { Sp.Most = How; Sp.Best = I; }
						}
					};
					Span2 Spans[Want];
					int Kept[Want] = { 0, Nc - 1 }, Count = 2, Made = 1;
					Spans[0].A = 0;
					Spans[0].B = Nc - 1;
					Score(Spans[0]);
					while (Count < Want)
					{
						int Pick = -1;
						for (int Sp = 0; Sp < Made; ++Sp)
						{
							if (Spans[Sp].Best >= 0 && (Pick < 0 || Spans[Sp].Most > Spans[Pick].Most)) Pick = Sp;
						}
						if (Pick < 0) break;
						const Span2 Was2 = Spans[Pick];
						Kept[Count++] = Was2.Best;
						Spans[Pick].B = Was2.Best;
						Score(Spans[Pick]);
						Spans[Made].A = Was2.Best;
						Spans[Made].B = Was2.B;
						Score(Spans[Made]);
						++Made;
					}
					std::sort(Kept, Kept + Count);
					for (int J = 0; J < Want; ++J)
					{
						P[LineGrass - 2 + J] = Cand[static_cast<size_t>(Kept[std::min(Count - 1, J)])];
						Pos[J] = Kept[std::min(Count - 1, J)];
					}
				}
				// (across the grass from his harness as he lies to there, bowed out, on the ground but where it rises to him,
				// over its silk should it cross any)
				for (int J = 1; J + 1 < LineGrass; ++J)
				{
					const double T = static_cast<double>(J) / (LineGrass - 1);
					const V3 On = GrassAt(Hn, Land, S.Si, T);
					P[J - 1] = { On.X, On.Y, G.At(On.X, On.Y) + std::max(Rope, Hh * (1.0 - T) - 4.0 * T * (1.0 - T) * Hh) };
					const double Top = TopAt(On.X, On.Y, P[J - 1].Z);
					if (Top > -1e8) P[J - 1].Z = std::max(P[J - 1].Z, Top + Rise);
				}
				// (never in its silk: where the straight between two of its points would pass under one of the points it might
				// have gone through (each Rise over its silk's top), both lifted, the nearer the more)
				{
					double Lift[LineVia] = {};
					for (int J = 0; J + 1 < Want; ++J)
					{
						const V3& A = P[LineGrass - 2 + J];
						const V3& B = P[LineGrass - 1 + J];
						const double Bx = B.X - A.X, By = B.Y - A.Y, B2 = Bx * Bx + By * By;
						if (B2 < 1e-10) continue;
						for (int I = static_cast<int>(std::floor(Pos[J])) + 1; static_cast<double>(I) < Pos[J + 1] && I < Nc; ++I)
						{
							const V3& Q = Cand[static_cast<size_t>(I)];
							const double T = std::clamp(((Q.X - A.X) * Bx + (Q.Y - A.Y) * By) / B2, 0.0, 1.0);
							const double Short = Q.Z - (A.Z + (B.Z - A.Z) * T);
							if (Short <= 0.0) continue;
							const double Norm = (1.0 - T) * (1.0 - T) + T * T;
							Lift[LineGrass - 2 + J] = std::max(Lift[LineGrass - 2 + J], Short * (1.0 - T) / Norm);
							Lift[LineGrass - 1 + J] = std::max(Lift[LineGrass - 1 + J], Short * T / Norm);
						}
					}
					for (int J = 0; J < LineVia; ++J) P[J].Z += Lift[J];
				}
				// (nor where the straight between two, as drawn, would pass under its silk's top (the edge of a layer over
				// another between them, a crest across it): both lifted half Rise clear of it, the nearer the more. Looked at
				// each 2 cm, from where it comes off the grass (each 1 cm where it comes up onto its silk, over the edges of
				// its layers there))
				{
					double Lift[LineVia] = {};
					const double Every = bFine ? 0.02 : 0.04;
					for (int J = LineGrass - 3; J + 1 < LineVia; ++J)
					{
						const V3& A = P[J];
						const V3& B = P[J + 1];
						const int Subs = std::max(2, static_cast<int>(std::ceil(std::hypot(B.X - A.X, B.Y - A.Y) / (J < LineGrass + 3 && bFine ? 0.5 * Every : Every))));
						for (int Q = 1; Q < Subs; ++Q)
						{
							const double T = Q / static_cast<double>(Subs), Z = A.Z + (B.Z - A.Z) * T, Clear3 = Z - 0.5 * Rise;
							const double Short = TopAt(A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T, Z, Clear3) - Clear3;
							if (Short <= 0.0) continue;
							const double Norm = (1.0 - T) * (1.0 - T) + T * T;
							Lift[J] = std::max(Lift[J], Short * (1.0 - T) / Norm);
							Lift[J + 1] = std::max(Lift[J + 1], Short * T / Norm);
						}
					}
					for (int J = 0; J < LineVia; ++J) P[J].Z += Lift[J];
				}
				// (never under the ground: each point, and each span both its ends lifted where the ground between stands
				// higher, the nearer the more)
				for (int J = 0; J < LineVia; ++J) P[J].Z = std::max(P[J].Z, G.At(P[J].X, P[J].Y) + Rope);
				if (Ground)
				{
					double Lift[LineVia] = {};
					for (int J = -1; J + 1 < LineVia; ++J)
					{
						const V3& A = J < 0 ? Hl : P[J];
						const V3& B = P[J + 1];
						const int Subs = std::max(2, static_cast<int>(std::hypot(B.X - A.X, B.Y - A.Y) / 0.05));
						for (int Q = 1; Q < Subs; ++Q)
						{
							const double T = Q / static_cast<double>(Subs);
							const double Ux = A.X + (B.X - A.X) * T, Uy = A.Y + (B.Y - A.Y) * T;
							const double Short = (J < 0 ? LineGround(Ux, Uy) : G.At(Ux, Uy)) + Rope - (A.Z + (B.Z - A.Z) * T);
							if (Short <= 0.0) continue;
							if (J < 0)
							{
								// (from his harness, which stays (on the ground over it, should he lie sunk into it): the next
								// point lifted, where the ground itself rises a fifth of the way on or more (LineGround))
								if (T >= 0.2) Lift[0] = std::max(Lift[0], Short / T);
								continue;
							}
							const double Norm = (1.0 - T) * (1.0 - T) + T * T;
							Lift[J] = std::max(Lift[J], Short * (1.0 - T) / Norm);
							Lift[J + 1] = std::max(Lift[J + 1], Short * T / Norm);
						}
					}
					for (int J = 0; J < LineVia; ++J) P[J].Z += Lift[J];
				}
				// (and drawn while its lines are not yet slack (LineLies: across the grass each of its first points from the
				// straight between his harness and where it comes onto its silk, toward these as slack as they are), never
				// under the ground: each of those raised as much more as its share of slack takes it over the ground there and
				// over it along the straights either side, the nearer the more; a review: on a field of hummocks and across a
				// ditch they had cut 0.2 m into the ground as it fell in. From his harness where the scene draws it (DownShape:
				// in the mesh's own space, his feet at its origin and his man flat and rigid, Hn), not lifted by the ground
				// under it: a second review found the lines from the harness drawn cutting up to 0.23 m into hummocks that
				// those from a harness lifted onto them had cleared. But taut from the ground over it where he lies sunk into
				// it (Hl, as LineLies draws them), and over the ground itself (LineGround): a third review found the straight
				// from his harness in a bank running 0.3 m under the ground most of the way to its silk as it fell in, and the
				// grass lifted over it 5 to 7 m)
				if (Ground && Slack < 1.0)
				{
					const V3& Harness = Hl;
					V3 Q[LineGrass + 2];
					Q[0] = Harness;
					for (int J = 1; J <= LineGrass; ++J)
					{
						const V3 Taut = Plus(Harness, Times(Minus(P[LineGrass], Harness), J / (LineGrass + 1.0)));
						Q[J] = Plus(Taut, Times(Minus(P[J - 1], Taut), Slack));
					}
					Q[LineGrass + 1] = P[LineGrass];
					double Need[LineGrass + 2] = {};
					for (int J = 0; J <= LineGrass; ++J)
					{
						const V3& A = Q[J];
						const V3& B = Q[J + 1];
						const int Subs = std::max(2, static_cast<int>(std::hypot(B.X - A.X, B.Y - A.Y) / 0.05));
						for (int Sq = 1; Sq <= Subs; ++Sq)
						{
							const double T = Sq / static_cast<double>(Subs);
							const double Short = LineGround(A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T) + Rope - (A.Z + (B.Z - A.Z) * T);
							if (Short <= 0.0) continue;
							if (J == 0) { if (T >= 0.2) Need[1] = std::max(Need[1], Short / T); continue; }
							if (J == LineGrass) { if (T <= 0.8) Need[J] = std::max(Need[J], Short / (1.0 - T)); continue; }
							const double Norm = (1.0 - T) * (1.0 - T) + T * T;
							Need[J] = std::max(Need[J], Short * (1.0 - T) / Norm);
							Need[J + 1] = std::max(Need[J + 1], Short * T / Norm);
						}
					}
					// (by its share of slack, as LineLies lets them down by it, but by no less than 0.01: held to 0.05, a make
					// with less (0.028 at a slump of 0.35) had lifted them too little and left them in the ground; with no
					// least, one barely slack would be lifted out of all reason, for the scene to draw toward between makes)
					for (int J = 1; J <= LineGrass; ++J) P[J - 1].Z += Need[J] / std::max(Slack, 0.01);
				}
				Out.Across.push_back(S.Seam);
				std::copy(P, P + LineVia, Out.Over.begin() + static_cast<std::ptrdiff_t>(Ln * static_cast<size_t>(LineVia)));
			}
		}
	}

	double LineSlack(double Slump)
	{
		return Smooth((Slump - 0.3) / 0.5);
	}

	int LineLies(const SilkMesh& Silk, size_t Seam, const V3& Harness, double Slack, V3 (&Out)[LinePoints])
	{
		if (Seam >= Silk.Seams.size() || static_cast<size_t>(Silk.Seams[Seam]) >= Silk.Points.size()) return 0;
		const V3& E = Silk.Points[static_cast<size_t>(Silk.Seams[Seam])];
		const double Sl = std::clamp(Slack, 0.0, 1.0);
		const V3 Way = Minus(E, Harness);
		size_t Slot = Silk.Across.size();
		for (size_t K = 0; K < Silk.Across.size(); ++K)
		{
			if (static_cast<size_t>(Silk.Across[K]) == Seam) Slot = K;
		}
		if (Slot >= Silk.Across.size() || Silk.Over.size() < (Slot + 1) * static_cast<size_t>(LineVia))
		{
			const double Flat = std::hypot(Way.X, Way.Y), Bow = (Seam % 2 == 0 ? 1.0 : -1.0) * 0.06 * Sl * Length(Way);
			const V3 Aside = Flat > 1e-9 ? V3{ -Way.Y / Flat, Way.X / Flat, 0.0 } : V3{};
			const double Low = Seam < Silk.MidLow.size() ? Silk.MidLow[Seam] : -1e9;
			Out[0] = Harness;
			Out[1] = { 0.5 * (Harness.X + E.X) + Aside.X * Bow, 0.5 * (Harness.Y + E.Y) + Aside.Y * Bow, std::max(Low, 0.5 * (Harness.Z + E.Z) - 0.5 * Sl * std::fabs(E.Z - Harness.Z)) };
			Out[2] = E;
			return 3;
		}
		// (across its silk: across the grass from taut, straight from his harness to where it is drawn onto its silk from,
		// to lying, through its first Over points; and on over its silk and its edge through the rest, as HeapShape laid
		// them for its slump. Taut from the ground over his harness where he lies sunk into a bank or a hummock
		// (HarnessLow), as HeapShape keeps them over the ground from: the first straight comes up out of the ground by him,
		// and the rest run on over it, not through it)
		const V3* Via = &Silk.Over[Slot * static_cast<size_t>(LineVia)];
		const V3 From{ Harness.X, Harness.Y, std::max(Harness.Z, Silk.HarnessLow) };
		Out[0] = Harness;
		for (int K = 1; K <= LineGrass; ++K)
		{
			const V3 Taut = Plus(From, Times(Minus(Via[LineGrass], From), K / (LineGrass + 1.0)));
			Out[K] = Plus(Taut, Times(Minus(Via[K - 1], Taut), Sl));
		}
		for (int K = LineGrass + 1; K < LinePoints; ++K) Out[K] = Via[K - 1];
		return LinePoints;
	}

	void SilkStep::Clear()
	{
		FromSlump = ToSlump = -1.0;
		From.Clear();
		To.Clear();
	}

	void SilkStep::Make(const Chute& C, double Slump, double Off, const GroundFn& Ground)
	{
		// (on whole steps of its slump, so that the step it is now in is the last make's next: made once)
		const double S = std::clamp(Slump, 0.0, 1.0);
		const double Key = std::min(1.0, std::floor(S / SlumpStep + 1e-9) * SlumpStep);
		if (Moving() && std::fabs(ToSlump - Key) < 1e-9)
		{
			std::swap(From, To);
		}
		else
		{
			HeapShape(C, Key, Off, From, Ground);
		}
		FromSlump = Key;
		ToSlump = std::min(1.0, Key + SlumpStep);
		if (ToSlump > FromSlump + 1e-9)
		{
			// (what it works out once as it lies, from the shape just made, should it not have it)
			if (To.Kept.Key != From.Kept.Key) To.Kept = From.Kept;
			HeapShape(C, ToSlump, Off, To, Ground);
		}
		else
		{
			To = SilkMesh();
			ToSlump = FromSlump;
		}
	}

	double SilkStep::Mix(double Slump) const
	{
		return Moving() ? std::clamp((Slump - FromSlump) / (ToSlump - FromSlump), 0.0, 1.0) : 0.0;
	}

	void SilkStep::At(double Slump, SilkMesh& Out) const
	{
		const double M = Mix(Slump);
		const size_t N = From.Points.size();
		Out.Points.resize(N);
		Out.Facings.resize(N);
		const bool bTo = Moving() && To.Points.size() == N;
		for (size_t K = 0; K < N; ++K)
		{
			const V3& A = From.Points[K];
			const V3& Fa = From.Facings[K];
			if (!bTo || M <= 0.0)
			{
				Out.Points[K] = A;
				Out.Facings[K] = Fa;
				continue;
			}
			const V3& B = To.Points[K];
			const V3& Fb = To.Facings[K];
			Out.Points[K] = { A.X + (B.X - A.X) * M, A.Y + (B.Y - A.Y) * M, A.Z + (B.Z - A.Z) * M };
			Out.Facings[K] = Unit({ Fa.X + (Fb.X - Fa.X) * M, Fa.Y + (Fb.Y - Fa.Y) * M, Fa.Z + (Fb.Z - Fa.Z) * M }, Fa);
		}
		Out.Seams = From.Seams;
		// (its lines across it, moved with it, and the lowest its other lines' middles lie and they start from at his
		// harness (each make reads the ground under it afresh))
		Out.MidLow.resize(From.MidLow.size());
		const bool bLow = bTo && To.MidLow.size() == From.MidLow.size();
		for (size_t K = 0; K < From.MidLow.size(); ++K) Out.MidLow[K] = bLow ? From.MidLow[K] + (To.MidLow[K] - From.MidLow[K]) * M : From.MidLow[K];
		Out.HarnessLow = bTo ? From.HarnessLow + (To.HarnessLow - From.HarnessLow) * M : From.HarnessLow;
		Out.Across = From.Across;
		Out.Over.resize(From.Over.size());
		const bool bOver = bTo && To.Over.size() == From.Over.size() && To.Across == From.Across;
		for (size_t K = 0; K < From.Over.size(); ++K)
		{
			const V3& A = From.Over[K];
			Out.Over[K] = bOver ? Plus(A, Times(Minus(To.Over[K], A), M)) : A;
		}
	}

	void SilkStep::Wound(SilkMesh& Out) const
	{
		At(0.5 * (FromSlump + ToSlump), Out);
		Out.Colours = From.Colours;
		Out.Triangles = From.Triangles;
		Out.Outward.resize(From.Outward.size());
		const bool bTo = Moving() && To.Outward.size() == From.Outward.size();
		for (size_t K = 0; K < From.Outward.size(); ++K)
		{
			Out.Outward[K] = bTo ? Unit(Plus(From.Outward[K], To.Outward[K]), From.Outward[K]) : From.Outward[K];
		}
	}

	bool LineBack(size_t Seam, size_t Seams)
	{
		// (Dome's seams go round from x, the way it falls: those whose place round it is behind its middle, which tip
		// back toward him as it falls in and lie on top of it, toward him, gathered at its near end; one just abreast
		// of it is left out)
		return Seams > 0 && std::cos(2.0 * Pi * static_cast<double>(Seam) / static_cast<double>(Seams)) < -1e-9;
	}

	V3 OverShift(double Fallen, double EndX, double EndY)
	{
		const double F = std::clamp(Fallen, 0.0, 1.0);
		return { (EndX - OverHip * std::sin(OverPitch * Rad)) * F, EndY * F, (ProneHips - OverHip * std::cos(OverPitch * Rad)) * F };
	}

	V3 OverHips(double Fallen, double EndX, double EndY)
	{
		const double F = std::clamp(Fallen, 0.0, 1.0), Over = OverPitch * F * Rad;
		return Plus({ OverHip * std::sin(Over), 0.0, OverHip * std::cos(Over) }, OverShift(F, EndX, EndY));
	}

	V3 OverHarness(double Fallen, double EndX, double EndY)
	{
		const double F = std::clamp(Fallen, 0.0, 1.0), Over = OverPitch * F * Rad;
		return Plus({ std::sin(Over), 0.0, std::cos(Over) }, OverShift(F, EndX, EndY));
	}

	bool SilkMoved(double Shape, double WasShape)
	{
		const bool bDown = Shape >= 2.0;
		return bDown != (WasShape >= 2.0) || std::abs(Shape - WasShape) > (bDown ? SlumpStep : OpenStep) || (Shape >= 1.0 && WasShape < 1.0) || (Shape >= 3.0 && WasShape < 3.0);
	}

	bool ManMoved(double Lying, double WasLying)
	{
		return std::abs(Lying - WasLying) > LyingStep || (Lying >= 1.0 && WasLying < 1.0) || ((Lying < 0.0) != (WasLying < 0.0));
	}

	double Glow(const V3& Eye, const V3& Canopy, const V3& TowardSun)
	{
		const V3 Look = Plus(Canopy, Times(Eye, -1.0));
		const double Far = Length(Look), SunLong = Length(TowardSun);
		if (Far < 1e-9 || SunLong < 1e-9) return 0.0;
		const V3 Way = Times(Look, 1.0 / Far), Sun = Times(TowardSun, 1.0 / SunLong);
		if (Sun.Z <= 0.0) return 0.0;
		// (a low sun's light fades in; looking up into it, a little always; against the sun, most)
		const double Day = std::clamp(Sun.Z / 0.1, 0.0, 1.0);
		const double Against = Way.X * Sun.X + Way.Y * Sun.Y + Way.Z * Sun.Z;
		const double Under = std::clamp(Way.Z * 2.0, 0.0, 1.0);
		return Day * (0.15 * Under + 0.75 * Smooth((Against - 0.3) / 0.65));
	}

	void Sky::At(double T, std::vector<Plane>& OutPlanes, std::vector<Gunner>& OutGuns) const
	{
		OutPlanes.clear();
		OutGuns.clear();
		for (size_t I = 0; I < Plans.size(); ++I)
		{
			const Plan& P = Plans[I];
			const double Tau = (T - P.Tin) * Life;
			if (Tau < -60.0) continue;
			const size_t From = OutPlanes.size();
			for (int Slot = 0; Slot < P.Models; ++Slot)
			{
				const Down& D = P.Falls[static_cast<size_t>(Slot)];
				Plane Pl;
				Pl.Key = static_cast<int>(I) * 8 + Slot;
				Pl.Leg = static_cast<int>(I);
				Pl.Slot = Slot;
				Pl.Side = P.Side;
				Pl.Nat = P.Nat;
				Pl.Model = P.M.Model;
				Pl.Gun = P.M.Gun;
				Pl.Span = P.M.Span;
				if (D.How != Fall::None && Tau >= D.Hit)
				{
					if (D.How == Fall::Explode) continue;
					Pl.How = D.How;
					if (Tau >= D.Crash)
					{
						// (where it struck, nose in, burning on)
						const Sample& Last = D.Path.back();
						Pl.bWreck = true;
						Pl.At = { Last.At.X, Last.At.Y, Last.At.Z - 0.6 };
						Pl.Heading = Last.Heading;
						Pl.Pitch = -18.0;
						Pl.Bank = Last.Bank * 0.3;
						OutPlanes.push_back(Pl);
						continue;
					}
					auto It = std::upper_bound(D.Path.begin(), D.Path.end(), Tau, [](double Tm, const Sample& S) { return Tm < S.T; });
					const Sample& B = It == D.Path.end() ? D.Path.back() : *It;
					const Sample& A = It == D.Path.begin() ? D.Path.front() : *(It - 1);
					const double K = B.T > A.T ? std::clamp((Tau - A.T) / (B.T - A.T), 0.0, 1.0) : 0.0;
					Pl.At = Plus(A.At, Times(Plus(B.At, Times(A.At, -1.0)), K));
					Pl.Vel = Times(Plus(B.At, Times(A.At, -1.0)), 1.0 / std::max(1e-3, B.T - A.T));
					Pl.Heading = A.Heading + (B.Heading - A.Heading) * K;
					Pl.Pitch = A.Pitch + (B.Pitch - A.Pitch) * K;
					Pl.Bank = A.Bank + (B.Bank - A.Bank) * K;
					OutPlanes.push_back(Pl);
					continue;
				}
				Sample S;
				if (!Fly(P, Slot, Tau, S)) continue;
				Pl.At = S.At;
				Pl.Heading = S.Heading;
				Pl.Pitch = S.Pitch;
				Pl.Bank = S.Bank;
				Flown(P, Slot, Tau, Pl);
				OutPlanes.push_back(Pl);
			}
			// The ground's guns on the nearest plane of the leg still in the air, short of its lead.
			for (const Post& Po : P.Posts)
			{
				const Burst* B = nullptr;
				for (const Burst& Bu : Po.Bursts)
				{
					if (Tau >= Bu.On && Tau < Bu.Off) { B = &Bu; break; }
				}
				if (!B) continue;
				const Plane* Aim = nullptr;
				double Best = Reach;
				for (size_t K = From; K < OutPlanes.size(); ++K)
				{
					if (OutPlanes[K].bWreck) continue;
					const double D = Length(Plus(OutPlanes[K].At, Times(Po.At, -1.0)));
					if (D < Best) { Best = D; Aim = &OutPlanes[K]; }
				}
				if (!Aim) continue;
				Gunner G;
				G.Key = Po.Key;
				G.From = Po.At;
				G.To = Plus(Plus(Aim->At, Times(Aim->Vel, B->Lead * Best / TracerMps)), Plus(Times(RightOf(Aim->Heading), B->Across), { 0.0, 0.0, B->Up }));
				// (always up into the air: a plane going down low is shot at over it, never into the ground)
				G.To.Z = std::max(G.To.Z, (GroundAt ? GroundAt(G.To.X, G.To.Y) : 0.0) + 12.0);
				OutGuns.push_back(G);
			}
		}
	}

	void Sky::Between(double From, double To, std::vector<Happen>& Out) const
	{
		Out.clear();
		if (!(To > From)) return;
		for (size_t I = 0; I < Plans.size(); ++I)
		{
			const Plan& P = Plans[I];
			for (int Slot = 0; Slot < P.Models; ++Slot)
			{
				const Down& D = P.Falls[static_cast<size_t>(Slot)];
				if (D.How == Fall::None) continue;
				const double Hit = P.Tin + D.Hit / Life, Crash = P.Tin + D.Crash / Life;
				Happen H;
				H.Key = static_cast<int>(I) * 8 + Slot;
				H.How = D.How;
				H.Span = P.M.Span;
				if (Hit > From && Hit <= To)
				{
					H.Kind = Happen::What::Hit;
					H.T = Hit;
					H.At = D.HitAt;
					H.Vel = D.HitVel;
					Out.push_back(H);
				}
				if (Crash > From && Crash <= To)
				{
					H.Kind = Happen::What::Crash;
					H.T = Crash;
					H.At = D.CrashAt;
					H.Vel = D.CrashVel;
					Out.push_back(H);
				}
			}
		}
		std::stable_sort(Out.begin(), Out.end(), [](const Happen& A, const Happen& B) { return A.T < B.T; });
	}

	int Sky::Rounds(double From, double To, double Rate) const
	{
		if (!(To > From) || Rate <= 0.0) return 0;
		return std::max(0, static_cast<int>(std::floor(To * Life * Rate) - std::floor(From * Life * Rate)));
	}
}
