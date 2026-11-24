#include "Battle/LandAir.h"

#include <algorithm>
#include <cmath>

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
			for (int Side = 0; Side < 2; ++Side)
			{
				const int First = Side * Each;
				for (int R = 0; R + 1 < Ringed; ++R)
				{
					for (int Col = 0; Col < Cols; ++Col)
					{
						const int A = First + R * Cols + Col, B = First + R * Cols + (Col + 1) % Cols;
						Tri(A, B, B + Cols);
						Tri(A, B + Cols, A + Cols);
					}
				}
				for (int Col = 0; Col < Cols; ++Col)
				{
					Tri(First + (Ringed - 1) * Cols + Col, First + (Ringed - 1) * Cols + (Col + 1) % Cols, First + Ringed * Cols);
				}
			}
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
			// its lines lie across it, two to four; for each, its pick among the seams that may (0 to 1), how far along it
			// it may reach at most (a share of its length: HeapShape draws it in to what the line's length lets it), its
			// curl across as it goes (either way, a share of the way across) and its bow along (rings), and its wiggle's
			// phase
			int Strewn = 0;
			double StrewnPick[4] = {}, StrewnReach[4] = {}, StrewnCurl[4] = {}, StrewnBow[4] = {}, StrewnWiggle[4] = {};
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
				L.StrewnReach[K] = S.U(0.26, 0.46);
				L.StrewnCurl[K] = S.U(-0.45, 0.45);
				L.StrewnBow[K] = S.U(-0.5, 0.5);
				L.StrewnWiggle[K] = S.U(0.0, 2.0 * Pi);
			}
			// (its far end's breakup, on dice of their own as well)
			Dice F(static_cast<uint64_t>(Folds) | (0xFA2Dull << 36));
			L.Vent = F.U(0.22, 0.38);
			L.CreaseDeep = F.U(0.8, 0.95);
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
		 * let the silk cut 8 mm into).
		 */
		struct UnderGrid
		{
			double X0 = 0.0, Y0 = 0.0, Step = 0.25, Most = -1e18;
			int Nx = 0, Ny = 0;
			std::vector<double> Z, Crest;
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
			}
			double At(double X, double Y) const
			{
				if (Nx < 2 || Ny < 2) return 0.0;
				const double U = (X - X0) / Step, V = (Y - Y0) / Step;
				const int I = std::clamp(static_cast<int>(std::floor(U)), 0, Nx - 2), J = std::clamp(static_cast<int>(std::floor(V)), 0, Ny - 2);
				const double A = std::clamp(U - I, 0.0, 1.0), B = std::clamp(V - J, 0.0, 1.0);
				const size_t K = static_cast<size_t>(J * Nx + I), Up = static_cast<size_t>(Nx);
				const double Flat = (Z[K] * (1.0 - A) + Z[K + 1] * A) * (1.0 - B) + (Z[K + Up] * (1.0 - A) + Z[K + Up + 1] * A) * B;
				return Flat + std::max(std::max(Crest[K], Crest[K + 1]), std::max(Crest[K + Up], Crest[K + Up + 1]));
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
			const int C0 = ((static_cast<int>(Cf) % Cols) + Cols) % Cols, C1 = (C0 + 1) % Cols;
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
			int Edge;
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
		// (its far end's crest across, a share of its half-width, to its right more than nothing: Lean)
		std::vector<Column> Cl(static_cast<size_t>(Cols));
		for (int Col = 0; Col < Cols; ++Col)
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
			Q.Edge = Q.Sn >= 0.0 ? 0 : 1;
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
			if (R == 10 || R == 12) G.X += (R == 10 ? 1.0 : -1.0) * 0.1;
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
		std::vector<V3> Was(Each), Lies(Each);
		std::vector<double> Settled(Each), Flap(Each), Floor(Each, Clear), Crossing(Each, 0.0);
		Out.Points.resize(Each);
		const double Slumps = Lean > 0.0 ? 1.0 : -1.0;
		// (each fold across it: how much of it there is round each ring, highest at CrossAt across it and fading either
		// way; none on a ring with none, worked out once)
		std::vector<double> Envs(static_cast<size_t>(Ringed * Cols), 0.0);
		for (int R = 0; R < Ringed; ++R)
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
				Settled[P] = Smooth((K - Early - Q.Late) / 0.6);
				const double Flapping = 0.6 * (Q.S3 * G.A1C + Q.C3 * G.A1S) + 0.4 * (Q.S7 * G.A2C + Q.C7 * G.A2S);
				Flap[P] = Sag * (1.0 - Settled[P]) * (0.55 * std::min(1.0, 0.3 + G.Gr) * Flapping + G.Billow);
			}
		}
		const int Seams = Cols / 4;
		const V3 Hn{ Hx, Hy, H.Z };
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
		// (her note of 24 November 17:40Z, (3), after a second look: its lines had crossed only the flap at its near end, by
		// 0.2 to 0.3 m, all running to one point. It slid and slewed as it fell in, and two to four of the lines to its top's
		// gores were drawn across it: each from the grass to its own seam's end on its gathered hem as the rest, then on
		// over its gathering onto its top past it, and over its top at a slant, in a loose curve (bowed along it, curled
		// round it, a little wiggle), to the edge on the far side of its top's middle from its seam (one at its middle to
		// its narrow side), a third of its length along it or more, over that edge and down under it (its gore's hem was
		// carried round under there as it slewed). Each as far along it as the line's length lets it, lying slack (from his
		// harness to its hem in the air, up his riser and along its line, LineLong; lying slack 1.6 times that at most: a
		// real canopy's lines are about as long as its flat width, 7 to 8 m, which the air's short hang under-draws), and no
		// farther than its own reach; none over a flipped or tucked gore's near end, nor under its flap, whose silk lies
		// elsewhere or over it. Laid out on its silk's own rings and round it (Strand: its points' rings and columns), its
		// points where its silk turns most along its way (over a fold's crest, down into its trough), so that it lies on its
		// silk as that falls in and wherever it lies, sagging into its folds. The rest run to its gathered hem as they did.)
		struct Strand
		{
			int Seam = 0;
			double R[LineVia] = {}, C[LineVia] = {}, Inward = 0.0;
		};
		std::vector<Strand> Strands;
		constexpr double Rise = 0.012, Onto = 2.1;
		constexpr int Last = LineVia - 2;
		{
			const double LineLong = 0.8 + std::hypot(Ax * HemIn / Widest() - 0.25, LinesLong - 0.8);
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
			// (the seams that may: a line runs back to him from it, and its gore and those beside it lie as laid at its near
			// end; the nearest its top's middle first, picked on its dice)
			std::vector<std::pair<double, int>> May;
			for (int Sm = 0; Sm < Seams; ++Sm)
			{
				const double Th = 2.0 * Pi * Sm / Seams;
				if (!LineBack(static_cast<size_t>(Sm), static_cast<size_t>(Seams)) || Moved(4.0 * Sm)) continue;
				May.emplace_back(std::fabs(std::sin(Th)), Sm);
			}
			std::sort(May.begin(), May.end());
			// (no more than six of its lines over its silk in all, with those over its flipped gores' flap: those to its
			// flipped gores and the gores beside them)
			int Flapping = 0;
			for (int Sm = 0; Sm < Seams; ++Sm)
			{
				bool bOver = false;
				for (int D = -1; D <= 1; ++D) bOver = bOver || Cl[static_cast<size_t>((4 * Sm + D + Cols) % Cols)].Flip > 0.3;
				Flapping += LineBack(static_cast<size_t>(Sm), static_cast<size_t>(Seams)) && bOver;
			}
			const int Wanted = std::max(2, std::min(L.Strewn, 6 - Flapping));
			const double NarrowSn = WideSide == 0 ? -1.0 : 1.0;
			Strand S;
			for (int Ln = 0; static_cast<int>(Strands.size()) < Wanted && !May.empty(); Ln = std::min(Ln + 1, 3))
			{
				const size_t Pick = std::min(May.size() - 1, static_cast<size_t>(L.StrewnPick[Ln] * static_cast<double>(May.size())));
				const int Sm = May[Pick].second;
				May.erase(May.begin() + static_cast<std::ptrdiff_t>(Pick));
				const double Th0 = 2.0 * Pi * Sm / Seams, Sn0 = std::sin(Th0);
				// (to the edge on the far side of its top's middle from its seam: round to it on its top, and on round it,
				// under)
				// (or from one out toward its edge, in toward its middle and back out to that edge farther along, in a loop)
				const bool bLoop = std::fabs(Sn0) >= 0.6;
				const double ExitSn = std::fabs(Sn0) < 0.15 ? NarrowSn : (Sn0 > 0.0) == bLoop ? 1.0 : -1.0;
				const double Exit = ExitSn < 0.0 ? 1.5 * Pi : 0.5 * Pi, TopWay = ExitSn < 0.0 ? -1.0 : 1.0, ThTop = Exit + TopWay * 0.3;
				const double Inward = bLoop ? (Th0 < Pi ? 1.0 : -1.0) * 0.75 * (1.0 + 0.4 * L.StrewnCurl[Ln]) : 0.0;
				S.Seam = Sm;
				S.Inward = -ExitSn;
				S.R[1] = 0.0;
				S.C[1] = 4.0 * Sm;
				const V3& Hem0 = Lies[static_cast<size_t>(4 * Sm)];
				// (laid out to reach Ru, its rings: onto its top past its gathering on its own gore, and from there across at
				// a slant to Ru, evenly round it and along it, a little bowed and curled, and its wiggle, each point on its
				// silk; then to its edge there (that ring's outermost point that side, a little round from where it reaches)
				// and under it)
				const auto LayOut = [&](double Ru)
				{
					for (int J = 2; J < Last; ++J)
					{
						const double T = (J - 2.0) / (Last - 3.0), Curled = T + L.StrewnCurl[Ln] * T * (1.0 - T);
						const double R = std::max(Onto, Onto + (Ru - Onto) * T + L.StrewnBow[Ln] * std::sin(Pi * T));
						const double Th = Th0 + (ThTop - Th0) * Curled + (Inward + 0.05 * std::sin(3.0 * Pi * T + L.StrewnWiggle[Ln])) * std::sin(Pi * T);
						S.R[J] = R;
						S.C[J] = ColOf(Th, Cols);
						if (R < 3.0 && !Flapped.empty() && UnderFlap(SurfAt(Lies, nullptr, Ringed, Cols, R, S.C[J], nullptr))) return false;
					}
					// (and from its seam's end onto its top, straight, clear of its flap)
					V3 Up = SurfAt(Lies, nullptr, Ringed, Cols, S.R[2], S.C[2], nullptr);
					Up.Z += Rise;
					for (int Q = 1; Q < 4 && !Flapped.empty(); ++Q)
					{
						if (UnderFlap(Plus(Hem0, Times(Minus(Up, Hem0), Q / 4.0)))) return false;
					}
					const int Rw = std::clamp(static_cast<int>(std::lround(S.R[Last - 1])), 2, Ringed - 2), Cw = static_cast<int>(std::lround(ColOf(Exit, Cols)));
					double Out2 = -1e9;
					for (int D = -6; D <= 6; ++D)
					{
						const int Col = (Cw + D + Cols) % Cols;
						const double Y = ExitSn * Lies[static_cast<size_t>(Rw * Cols + Col)].Y;
						if (Y > Out2) { Out2 = Y; S.R[Last] = Rw; S.C[Last] = Col; }
					}
					return true;
				};
				// (how long it lies so on its silk, from its seam's end to its edge, and under it)
				const auto Long = [&]()
				{
					V3 Back = Hem0;
					double Sum = 0.05;
					for (int J = 2; J <= Last; ++J)
					{
						const V3 Now = SurfAt(Lies, nullptr, Ringed, Cols, S.R[J], S.C[J], nullptr);
						Sum += Length(Minus(Now, Back));
						Back = Now;
					}
					return Sum;
				};
				// (from his harness slack across the grass to its seam's end, its middle bowed aside, and what that leaves;
				// tried from its reach in, first by the straight from its seam's end to its edge there, at the least as
				// long, and the way laid out on its silk as much longer than that as the last one tried)
				const double Left = 1.55 * LineLong - 1.01 * Length(Minus(Hem0, Hn));
				double Longer = 1.0;
				bool bLaid = false;
				for (double U = L.StrewnReach[Ln]; U >= 0.25 && !bLaid; U -= 0.03)
				{
					const double Ru = RingOf(U);
					const double Beeline = Length(Minus(SurfAt(Lies, nullptr, Ringed, Cols, Ru, ColOf(Exit, Cols), nullptr), Hem0));
					if (Beeline * Longer > Left || !LayOut(Ru)) continue;
					const double Way = Long();
					Longer = std::max(1.0, 0.95 * Way / std::max(1e-6, Beeline));
					bLaid = Way <= Left;
				}
				if (!bLaid) continue;
				// (its points where its silk turns most along its way: its way on its top four times as finely, and from where
				// it comes onto it and where it reaches, each next point the one farthest off the line through those so far)
				{
					constexpr int Fine = 4, Steps = (Last - 3) * Fine + 1, Picks = Last - 2;
					const auto Dot3 = [](const V3& A, const V3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; };
					double Rf[Steps], Cf[Steps];
					V3 Pf[Steps];
					for (int I = 0; I < Steps; ++I)
					{
						const int J = 2 + std::min(Last - 4, I / Fine);
						const double T = (I - (J - 2) * Fine) / static_cast<double>(Fine);
						Rf[I] = S.R[J] + (S.R[J + 1] - S.R[J]) * T;
						Cf[I] = S.C[J] + (S.C[J + 1] - S.C[J]) * T;
						Pf[I] = SurfAt(Lies, nullptr, Ringed, Cols, Rf[I], Cf[I], nullptr);
					}
					int Kept[Picks] = { 0, Steps - 1 };
					int Count = 2;
					while (Count < Picks)
					{
						int Best = -1, At = 0;
						double Most = -1.0;
						for (int Seg = 0; Seg + 1 < Count; ++Seg)
						{
							const V3& A = Pf[Kept[Seg]];
							const V3 Ab = Minus(Pf[Kept[Seg + 1]], A);
							const double Ab2 = std::max(1e-12, Dot3(Ab, Ab));
							for (int I = Kept[Seg] + 1; I < Kept[Seg + 1]; ++I)
							{
								const V3 Ap = Minus(Pf[I], A);
								const V3 Aside2 = Minus(Ap, Times(Ab, std::clamp(Dot3(Ap, Ab) / Ab2, 0.0, 1.0)));
								// (its height off the line counting thrice its way aside, and its longest gaps a little, so that it
								// keeps to its curve where its silk is smooth)
								const double How = 9.0 * Aside2.Z * Aside2.Z + Aside2.X * Aside2.X + Aside2.Y * Aside2.Y + 1e-4 * Ab2;
								if (How > Most) { Most = How; Best = I; At = Seg + 1; }
							}
						}
						if (Best < 0) break;
						for (int K2 = Count; K2 > At; --K2) Kept[K2] = Kept[K2 - 1];
						Kept[At] = Best;
						++Count;
					}
					for (int J = 2; J < Last; ++J)
					{
						const int I = Kept[std::min(Count - 1, J - 2)];
						S.R[J] = Rf[I];
						S.C[J] = Cf[I];
					}
				}
				Strands.push_back(S);
				// (and none from the seams either side of it, so that they lie apart)
				May.erase(std::remove_if(May.begin(), May.end(), [Sm, Seams](const std::pair<double, int>& Mp) { return (Mp.second - Sm + Seams + 1) % Seams <= 2; }), May.end());
			}
		}
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
		// lies over it, flapping, never nearer it than Clear; then each edge kept over it (both its ends lifted where the
		// ground between them stands higher than the edge would: a bank's or a rim's crest).
		const V3 Fw = Ahead(C.HeapHeading), Rt = RightOf(C.HeapHeading);
		UnderGrid G;
		if (Ground)
		{
			double MinX = 1e18, MinY = 1e18, MaxX = -1e18, MaxY = -1e18;
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
			const double LiesZ = Lies[P].Z + G.At(Lies[P].X, Lies[P].Y);
			At.Z = std::max(Was[P].Z + (LiesZ - Was[P].Z) * Settled[P] + Flap[P], G.At(At.X, At.Y) + Clear);
		}
		if (Ground)
		{
			// (an edge wholly over the highest the grid reads, as a canopy still high in the air, is left as it is)
			const double Over = G.Most + Clear;
			auto Keep = [&Out, &G, Clear, Over](size_t A, size_t B)
			{
				V3& Pa = Out.Points[A];
				V3& Pb = Out.Points[B];
				if (Pa.Z >= Over && Pb.Z >= Over) return;
				const int Steps = 2 + static_cast<int>(std::hypot(Pb.X - Pa.X, Pb.Y - Pa.Y) / (0.5 * G.Step));
				double Short = 0.0;
				for (int S = 1; S < Steps; ++S)
				{
					const double T = static_cast<double>(S) / Steps;
					Short = std::max(Short, G.At(Pa.X + (Pb.X - Pa.X) * T, Pa.Y + (Pb.Y - Pa.Y) * T) + Clear - (Pa.Z + (Pb.Z - Pa.Z) * T));
				}
				Pa.Z += Short;
				Pb.Z += Short;
			};
			for (int R = 0; R < Ringed; ++R)
			{
				for (int Col = 0; Col < Cols; ++Col)
				{
					const size_t A = static_cast<size_t>(R * Cols + Col), B = static_cast<size_t>(R * Cols + (Col + 1) % Cols);
					const size_t Ua = R + 1 < Ringed ? A + static_cast<size_t>(Cols) : Each - 1, Ub = R + 1 < Ringed ? B + static_cast<size_t>(Cols) : Each - 1;
					Keep(A, B);
					Keep(A, Ua);
					Keep(A, Ub);
				}
			}
		}
		Out.Facings.assign(Each, { 0.0, 0.0, 1.0 });
		Out.Colours.reserve(Each * 3);
		for (int R = 0; R < Ringed; ++R)
			for (int Col = 0; Col < Cols; ++Col)
				for (int Ch = 0; Ch < 3; ++Ch) Out.Colours.push_back(C.Silk[Ch] * Hemmed(R, Col));
		for (int Ch = 0; Ch < 3; ++Ch) Out.Colours.push_back(C.Silk[Ch]);
		// (each point's facing from where the points about it are: round its ring by up it toward its crown, by the rings
		// either side of it. Where a fold across it is (the art director's note of 24 November 17:40Z), once it streams
		// out, up it as the silk runs on from it to the next ring: those folds stand two rings apart or more, and by the
		// rings either side of each, a crest's and a trough's alike, they shade flat; so there a trough faces up its
		// fold's near side and a crest down its far side, each fold a darker band and a lighter one. And its flipped
		// gores' fold, a roll of silk at its near edge, faces out of the roll, back toward him, so that its silk either
		// side of it, one layer turned over onto the other, is each wound its own way)
		const auto Pt = [&Out, Cols](int R, int Col) -> const V3& { return Out.Points[static_cast<size_t>(R * Cols + (Col + Cols) % Cols)]; };
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
		// (its lines across it as it now is: from the grass, slack as the rest (their middle let down to the lower end's
		// height and bowed aside by 0.06 of the line's length, every other line the other way), to its seam's end and on
		// over its silk, each of its points over it where it was laid out, Rise (the 12 mm cord's half-thickness and 6 mm)
		// over it; to its edge, and under it on the ground; never under the ground. Down (its slump at its end: lying, its
		// lines slack, made once), over the top of its silk there (the highest of its triangles facing up, as it stands
		// within 1.4 cm of the point: where a fold or its under layer lies over the gore it was laid on, or stands up
		// beside the line, over that), and where the straight between two would dip into its silk anywhere along it (every
		// 2.5 cm: over a fold's crest), the two lifted clear of it, the nearer the more)
		Out.Across.clear();
		Out.Over.clear();
		if (!Strands.empty())
		{
			const size_t Lines = Strands.size();
			const bool bDown = K >= 0.999;
			std::vector<V3> Pts(Lines * static_cast<size_t>(LineVia));
			double Lo[2] = { 1e9, 1e9 }, Hi[2] = { -1e9, -1e9 }, Farthest = 0.0;
			for (size_t Ln = 0; Ln < Lines; ++Ln)
			{
				const Strand& S = Strands[Ln];
				for (int J = 1; J <= Last; ++J)
				{
					V3& P = Pts[Ln * LineVia + static_cast<size_t>(J)];
					P = SurfAt(Out.Points, nullptr, Ringed, Cols, S.R[J], S.C[J], nullptr);
					Lo[0] = std::min(Lo[0], P.X); Hi[0] = std::max(Hi[0], P.X);
					Lo[1] = std::min(Lo[1], P.Y); Hi[1] = std::max(Hi[1], P.Y);
					Farthest = std::max(Farthest, S.R[J]);
				}
			}
			// (down, its triangles facing up over its rings as far as its lines reach and three on, both its faces, each in
			// the squares of a 15 cm grid over where its lines lie that it reaches over or near)
			constexpr double Cell = 0.15, Near = 0.0142;
			const int Gx = static_cast<int>((Hi[0] - Lo[0]) / Cell) + 1, Gy = static_cast<int>((Hi[1] - Lo[1]) / Cell) + 1;
			struct Flat
			{
				double X0, X1, Y0, Y1, Ax, Ay, Wb[2], Wc[2], Least[3], Z0, Sx, Sy, Aside, Highest;
			};
			std::vector<Flat> Flats;
			std::vector<int> Starts, Items;
			if (bDown)
			{
				const int Rings2 = std::min(Ringed - 1, static_cast<int>(Farthest) + 3);
				// (the squares its lines are over, as they are sampled below)
				std::vector<unsigned char> Used(static_cast<size_t>(Gx * Gy), 0);
				const auto Mark = [&](const V3& At) { Used[static_cast<size_t>(std::clamp(static_cast<int>((At.Y - Lo[1]) / Cell), 0, Gy - 1) * Gx + std::clamp(static_cast<int>((At.X - Lo[0]) / Cell), 0, Gx - 1))] = 1; };
				for (size_t Ln = 0; Ln < Lines; ++Ln)
				{
					const V3* P = &Pts[Ln * LineVia];
					for (int J = 1; J <= Last; ++J) Mark(P[J]);
					for (int J = 1; J < Last; ++J)
					{
						const int Subs = std::max(2, static_cast<int>(std::hypot(P[J + 1].X - P[J].X, P[J + 1].Y - P[J].Y) / 0.025));
						for (int Q = 1; Q < Subs; ++Q) Mark(Plus(P[J], Times(Minus(P[J + 1], P[J]), Q / static_cast<double>(Subs))));
					}
				}
				struct Spans
				{
					int I0, I1, J0, J1;
				};
				std::vector<Spans> Found;
				Flats.reserve(static_cast<size_t>(2 * Rings2 * Cols));
				Found.reserve(Flats.capacity());
				Starts.assign(static_cast<size_t>(Gx * Gy + 1), 0);
				const size_t PerFace = static_cast<size_t>(2 * (Ringed - 1) * Cols + Cols);
				const int* Tr = Out.Triangles.data();
				for (size_t Face = 0; Face < 2; ++Face)
				{
					for (size_t T = Face * PerFace; T < Face * PerFace + static_cast<size_t>(2 * Rings2 * Cols); ++T)
					{
						const size_t Ia = static_cast<size_t>(Tr[3 * T]), Ib = static_cast<size_t>(Tr[3 * T + 1]), Ic = static_cast<size_t>(Tr[3 * T + 2]);
						if (Out.Facings[Ia].Z + Out.Facings[Ib].Z + Out.Facings[Ic].Z <= 0.0) continue;
						const V3& Pa = Out.Points[Ia];
						const V3& Pb = Out.Points[Ib];
						const V3& Pc = Out.Points[Ic];
						const double X0 = std::min({ Pa.X, Pb.X, Pc.X }) - Near, X1 = std::max({ Pa.X, Pb.X, Pc.X }) + Near;
						const double Y0 = std::min({ Pa.Y, Pb.Y, Pc.Y }) - Near, Y1 = std::max({ Pa.Y, Pb.Y, Pc.Y }) + Near;
						if (X1 < Lo[0] || X0 > Hi[0] || Y1 < Lo[1] || Y0 > Hi[1]) continue;
						const double D = (Pb.X - Pa.X) * (Pc.Y - Pa.Y) - (Pc.X - Pa.X) * (Pb.Y - Pa.Y);
						if (std::fabs(D) < 1e-12) continue;
						const int I0 = std::max(0, static_cast<int>((X0 - Lo[0]) / Cell)), I1 = std::min(Gx - 1, static_cast<int>((X1 - Lo[0]) / Cell));
						const int J0 = std::max(0, static_cast<int>((Y0 - Lo[1]) / Cell)), J1 = std::min(Gy - 1, static_cast<int>((Y1 - Lo[1]) / Cell));
						int Over = 0;
						for (int J = J0; J <= J1; ++J)
							for (int I = I0; I <= I1; ++I) Over += Used[static_cast<size_t>(J * Gx + I)];
						if (Over == 0) continue;
						// (where a point is on it, its corners' shares of it; how far out of it each share may go and the point
						// still be within Near of it; and its height there, as steep as it is a centimetre either way of it at
						// most, never over its highest corner)
						const double Per = 1.0 / D, Out2 = -Near * std::fabs(Per);
						const double Bx = Pb.X - Pa.X, By = Pb.Y - Pa.Y, Cx = Pc.X - Pa.X, Cy = Pc.Y - Pa.Y;
						Flat F;
						F.X0 = X0; F.X1 = X1; F.Y0 = Y0; F.Y1 = Y1; F.Ax = Pa.X; F.Ay = Pa.Y;
						F.Wb[0] = Cy * Per; F.Wb[1] = -Cx * Per;
						F.Wc[0] = -By * Per; F.Wc[1] = Bx * Per;
						F.Least[0] = Out2 * std::sqrt((Cx - Bx) * (Cx - Bx) + (Cy - By) * (Cy - By));
						F.Least[1] = Out2 * std::sqrt(Cx * Cx + Cy * Cy);
						F.Least[2] = Out2 * std::sqrt(Bx * Bx + By * By);
						F.Z0 = Pa.Z;
						F.Sx = (Pb.Z - Pa.Z) * F.Wb[0] + (Pc.Z - Pa.Z) * F.Wc[0];
						F.Sy = (Pb.Z - Pa.Z) * F.Wb[1] + (Pc.Z - Pa.Z) * F.Wc[1];
						F.Aside = 0.01 * (std::fabs(F.Sx) + std::fabs(F.Sy));
						F.Highest = std::max({ Pa.Z, Pb.Z, Pc.Z });
						Flats.push_back(F);
						Found.push_back({ I0, I1, J0, J1 });
						for (int J = J0; J <= J1; ++J)
							for (int I = I0; I <= I1; ++I) Starts[static_cast<size_t>(J * Gx + I + 1)] += Used[static_cast<size_t>(J * Gx + I)];
					}
				}
				for (size_t Q = 1; Q < Starts.size(); ++Q) Starts[Q] += Starts[Q - 1];
				Items.resize(static_cast<size_t>(Starts.back()));
				std::vector<int> Next(Starts.begin(), Starts.end() - 1);
				for (size_t F = 0; F < Found.size(); ++F)
					for (int J = Found[F].J0; J <= Found[F].J1; ++J)
						for (int I = Found[F].I0; I <= Found[F].I1; ++I)
							if (Used[static_cast<size_t>(J * Gx + I)]) Items[static_cast<size_t>(Next[static_cast<size_t>(J * Gx + I)]++)] = static_cast<int>(F);
			}
			// (the top of its silk at a point: the highest of them there or within Near of it, as high as it is anywhere a
			// centimetre either way of it; so that a line over a fold's flank, or beside where a fold stands up, is clear of
			// it all round)
			const auto TopAt = [&](double X, double Y)
			{
				const int I = std::clamp(static_cast<int>((X - Lo[0]) / Cell), 0, Gx - 1), J = std::clamp(static_cast<int>((Y - Lo[1]) / Cell), 0, Gy - 1);
				double Best = -1e9;
				for (int Q = Starts[static_cast<size_t>(J * Gx + I)]; Q < Starts[static_cast<size_t>(J * Gx + I + 1)]; ++Q)
				{
					const Flat& F = Flats[static_cast<size_t>(Items[static_cast<size_t>(Q)])];
					if (X < F.X0 || X > F.X1 || Y < F.Y0 || Y > F.Y1) continue;
					const double Dx = X - F.Ax, Dy = Y - F.Ay;
					const double Wb = F.Wb[0] * Dx + F.Wb[1] * Dy, Wc = F.Wc[0] * Dx + F.Wc[1] * Dy;
					if (Wb < F.Least[1] || Wc < F.Least[2] || 1.0 - Wb - Wc < F.Least[0]) continue;
					Best = std::max(Best, std::min(F.Highest, F.Z0 + F.Sx * Dx + F.Sy * Dy + F.Aside));
				}
				return Best;
			};
			Out.Over.resize(Lines * static_cast<size_t>(LineVia));
			for (size_t Ln = 0; Ln < Lines; ++Ln)
			{
				const Strand& S = Strands[Ln];
				V3* P = &Pts[Ln * LineVia];
				for (int J = 1; J <= Last; ++J) P[J].Z = (bDown ? std::max(P[J].Z, TopAt(P[J].X, P[J].Y)) : P[J].Z) + Rise;
				double Lift[LineVia] = {};
				for (int J = 1; J < Last && bDown; ++J)
				{
					const int Subs = std::max(2, static_cast<int>(std::hypot(P[J + 1].X - P[J].X, P[J + 1].Y - P[J].Y) / 0.025));
					for (int Q = 1; Q < Subs; ++Q)
					{
						const double T = Q / static_cast<double>(Subs);
						const V3 At = Plus(P[J], Times(Minus(P[J + 1], P[J]), T));
						const double Short = TopAt(At.X, At.Y) + Rise - At.Z;
						if (Short <= 0.0) continue;
						const double Norm = (1.0 - T) * (1.0 - T) + T * T;
						Lift[J] = std::max(Lift[J], Short * (1.0 - T) / Norm);
						Lift[J + 1] = std::max(Lift[J + 1], Short * T / Norm);
					}
				}
				for (int J = 1; J <= Last; ++J) P[J].Z += Lift[J];
				P[LineVia - 1] = { P[Last].X, P[Last].Y + S.Inward * 0.05, 0.0 };
				for (int J = 1; J < LineVia; ++J) P[J].Z = std::max(J + 1 < LineVia ? P[J].Z : -1e9, G.At(P[J].X, P[J].Y) + 0.015);
				const V3 Way = Minus(P[1], Hn);
				const double Flat = std::max(1e-9, std::hypot(Way.X, Way.Y)), Bow = (S.Seam % 2 == 0 ? 1.0 : -1.0) * 0.06 * Length(Way);
				P[0] = { 0.5 * (Hn.X + P[1].X) - Way.Y / Flat * Bow, 0.5 * (Hn.Y + P[1].Y) + Way.X / Flat * Bow, std::min(Hn.Z, P[1].Z) };
				Out.Across.push_back(S.Seam);
				std::copy(P, P + LineVia, Out.Over.begin() + static_cast<std::ptrdiff_t>(Ln * LineVia));
			}
		}
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
			Out[0] = Harness;
			Out[1] = { 0.5 * (Harness.X + E.X) + Aside.X * Bow, 0.5 * (Harness.Y + E.Y) + Aside.Y * Bow, 0.5 * (Harness.Z + E.Z) - 0.5 * Sl * std::fabs(E.Z - Harness.Z) };
			Out[2] = E;
			return 3;
		}
		// (across its silk: taut, evenly along the straight from his harness to its seam's point; lying, from his harness
		// through its Over points; in between, each point between the two)
		const V3* Via = &Silk.Over[Slot * static_cast<size_t>(LineVia)];
		for (int K = 0; K < LinePoints; ++K)
		{
			const V3 Taut = Plus(Harness, Times(Way, static_cast<double>(K) / (LinePoints - 1)));
			const V3 Lies = K == 0 ? Harness : Via[K - 1];
			Out[K] = Plus(Taut, Times(Minus(Lies, Taut), Sl));
		}
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
		// (its lines across it, moved with it)
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
