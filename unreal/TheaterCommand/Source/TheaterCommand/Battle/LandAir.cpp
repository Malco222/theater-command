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
				C.Slump = Smooth((Tau - D.Landed) / 2.5);
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

		/** Where its hem is on a gore (Gore's Phi there), and how its skirt curls in (the higher, the more of its drawing
		 * in is near its hem). */
		double HemPhi() { return std::acos(HemIn); }
		constexpr double Curl = 2.5;

		/**
		 * A point of a canopy's gore for a widest radius of 1 and a depth of 1 (the art director's verdict of 24 November
		 * 14:34Z): Phi from -HemPhi at its hem, through 0 at its widest, WidestDown under its crown, to Pi / 2 at its
		 * crown. Over its widest a quarter-ellipse, its crown; under it its skirt, drawn in more and more toward its hem
		 * (by the Curl-th power of the way down it) to a hem HemIn as wide, its silk there heading in toward its lines (a
		 * round canopy's hem pulled in by them, not dropped straight). How far out (R) and how high over its hem (Z), and
		 * its outer face's facing out and up for a canopy A wide and D deep: (Nr / A, Nz / D).
		 */
		void Gore(double Phi, double& R, double& Z, double& Nr, double& Nz)
		{
			if (Phi >= 0.0)
			{
				R = std::cos(Phi);
				Z = 1.0 - WidestDown + WidestDown * std::sin(Phi);
				Nr = std::cos(Phi);
				Nz = std::sin(Phi) / WidestDown;
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
			Out.Points.reserve(2 * Half);
			Out.Facings.reserve(2 * Half);
			Out.Colours.reserve(6 * Half);
			for (size_t K = 0; K < Half; ++K)
			{
				Out.Points.push_back(Out.Points[K]);
				Out.Facings.push_back(Times(Out.Facings[K], -1.0));
				for (size_t Ch = 0; Ch < 3; ++Ch) Out.Colours.push_back(Out.Colours[K * 3 + Ch]);
			}
			auto Tri = [&Out](int A, int B, int C2)
			{
				Out.Triangles.insert(Out.Triangles.end(), { A, B, C2 });
				const V3 Sum = Plus(Plus(Out.Facings[static_cast<size_t>(A)], Out.Facings[static_cast<size_t>(B)]), Out.Facings[static_cast<size_t>(C2)]);
				Out.Outward.push_back(Times(Sum, 1.0 / std::max(1e-9, Length(Sum))));
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
		 * Deep from its skirt to its crown, on Gore's curve. Each gore billows out a little between its seams. Its eight
		 * rings: its hem, two as its skirt curls in, its widest, and four up to its crown.
		 */
		void Dome(SilkMesh& Out, const V3& Base, double Rx, double Ry, double Deep, int Gores, const double Silk[3])
		{
			const double Rings[] = { -HemPhi(), -0.75 * HemPhi(), -0.45 * HemPhi(), 0.0, 0.4, 0.8, 1.15, 1.42 };
			constexpr int Ringed = 8;
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
		 * stronger wind stretching it), its width (2.45 to 2.85 times shorter), how far past his harness its near end is
		 * gathered, how high its far end stands on the air it still holds (0.3 to 0.62 m, more in a stronger wind) and
		 * how much that swells it; its axis's bend and where its far end lies across; its four or five long folds, each
		 * along it from near its near end to some way on, its place across (the gaps between them graded, one up to
		 * twice another and more, in no order), its drift and wander across as it goes, its width and its height (one
		 * big fold and the rest smaller, in no order); each edge's waver and swell; and two creases across it, where it
		 * bunched as it fell.
		 */
		struct Lay
		{
			double Long = 7.0, Wide = 2.8, Gap = 1.0, Lift = 0.5, Bulb = 0.0, Bend = 0.0, FarSide = 0.0;
			int Ridges = 4;
			double At[5] = {}, Drift[5] = {}, Wave[5] = {}, Phase[5] = {}, Width[5] = {}, High[5] = {}, From[5] = {}, To[5] = {};
			double EdgeWave[2] = {}, EdgePhase[2] = {}, EdgeSwell[2] = {}, Crease[2] = {}, CreaseHigh[2] = {};
			double Loose = 0.0;
		};
		Lay LayOf(uint32_t Folds, double Across, double Wind)
		{
			Lay L;
			Dice D(Folds);
			const double Blow = std::clamp(Wind / 6.0, 0.0, 1.0);
			L.Long = Across * (1.25 + 0.15 * Blow + D.U(0.0, 0.15));
			L.Wide = L.Long / D.U(2.45, 2.85);
			L.Gap = D.U(0.8, 1.2);
			L.Lift = std::min(0.62, 0.3 + 0.25 * Blow + D.U(0.0, 0.12));
			L.Bulb = D.U(0.0, 0.12);
			L.Bend = D.U(-0.35, 0.35);
			L.FarSide = D.U(-0.3, 0.3);
			L.Ridges = D.U() < 0.5 ? 4 : 5;
			// (their heights, and the gaps between them, graded and dealt out across it in no order, a big fold broader than
			// a small one, and none so near the next that the two run into one; across it, a share of its half-width, from
			// its middle)
			int Order[5] = { 0, 1, 2, 3, 4 }, Spaced[4] = { 0, 1, 2, 3 };
			constexpr double Heights[5] = { 0.27, 0.18, 0.13, 0.1, 0.085 };
			for (int K = L.Ridges - 1; K > 0; --K) std::swap(Order[K], Order[std::min(K, static_cast<int>(D.U() * (K + 1)))]);
			for (int K = L.Ridges - 2; K > 0; --K) std::swap(Spaced[K], Spaced[std::min(K, static_cast<int>(D.U() * (K + 1)))]);
			for (int K = 0; K < L.Ridges; ++K)
			{
				L.High[K] = Heights[Order[K]] * D.U(0.9, 1.1);
				L.Width[K] = (0.04 + 0.28 * L.High[K]) * D.U(0.85, 1.15);
				L.Drift[K] = D.U(-0.2, 0.2);
				L.Wave[K] = D.U(0.6, 1.6);
				L.Phase[K] = D.U();
				L.From[K] = D.U(0.0, 0.08);
				L.To[K] = D.U(0.55, 1.0);
			}
			double Gaps[4] = {}, Span = 0.0;
			for (int K = 0; K + 1 < L.Ridges; ++K)
			{
				Gaps[K] = (1.0 + 1.4 * Spaced[K] / (L.Ridges - 2.0)) * D.U(0.92, 1.08);
				Span += Gaps[K];
			}
			const double Wide = D.U(1.0, 1.25);
			double Across2 = 0.0;
			for (int K = 0; K < L.Ridges; ++K)
			{
				L.At[K] = Across2;
				if (K + 1 < L.Ridges) Across2 += std::max(Gaps[K] * Wide / Span, 1.8 * (L.Width[K] + L.Width[K + 1]));
			}
			const double Shift = -0.5 * L.At[L.Ridges - 1] + D.U(-0.12, 0.12);
			for (int K = 0; K < L.Ridges; ++K) L.At[K] += Shift;
			for (int K = 0; K < 2; ++K)
			{
				L.EdgeWave[K] = D.U(0.8, 2.0);
				L.EdgePhase[K] = D.U();
				L.EdgeSwell[K] = D.U(-0.08, 0.08);
				L.Crease[K] = D.U(0.25, 0.75);
				L.CreaseHigh[K] = D.U(0.03, 0.08);
			}
			L.Loose = D.U();
			return L;
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
		// As it was as he landed: over him on its lines, its skirt LinesLong over his harness (the air's canopy, on more
		// rings: five as its skirt curls in, its widest, eight up to its crown).
		const double Hem = HemPhi();
		const double Rings[] = { -Hem, -0.8 * Hem, -0.6 * Hem, -0.4 * Hem, -0.2 * Hem, 0.0, 0.3, 0.54, 0.735, 0.9, 1.05, 1.19, 1.32, 1.445 };
		constexpr int Ringed = 14;
		const int Cols = std::max(8, C.Gores) * 4;
		double Radius = 0.0, Deep = 0.0, Under = 0.0;
		Filling(C, C.Open, Radius, Deep, Under);
		const double Skirt = LinesLong + HangFeet - Under, Dz = std::max(0.05, Deep), Ax = std::max(0.02, Radius);
		// (the field under it in its own space, its origin his feet on the ground)
		const V3 Fw = Ahead(C.HeapHeading), Rt = RightOf(C.HeapHeading);
		auto Field = [&](double X, double Y)
		{
			return Ground ? Ground(C.Landed.X + Fw.X * X + Rt.X * Y, C.Landed.Y + Fw.Y * X + Rt.Y * Y) - C.Landed.Z : 0.0;
		};
		// (his harness as he lies, where its lines meet him: its near end is gathered Gap past it)
		const V3 H = OverHarness(1.0, ProneEndX, ProneEndY);
		const double Turn = (C.Face - C.HeapHeading) * Rad;
		const double Hx = H.X * std::cos(Turn) - H.Y * std::sin(Turn), Hy = H.X * std::sin(Turn) + H.Y * std::cos(Turn);
		const double Nx = std::max(Off - 0.5 * L.Long, Hx + 0.5), Ny = 0.7 * Hy;
		// Falling in: it loses its air over the first half, its crown sinking and its silk flapping (Sag), and tips
		// forward on its lines about his harness (Tip, to a quarter turn), so that it falls in downwind of him and down;
		// and it settles into how it lies, streaming out along the wind (Stream) a little ahead of coming down (Lie).
		// Nothing of it comes nearer the ground than Clear (metres: its silk's own loft, and a field's hummocks between
		// its points).
		const double Sag = Smooth(K / 0.5), Tip = 0.5 * Pi * Smooth(K), Lie = Smooth((K - 0.1) / 0.9), Stream = Smooth(K / 0.7);
		const double Flap = Sag * (1.0 - Lie), Clear = 0.03;
		const double Pivot = HangFeet;
		auto Falling = [&](double X, double Y, double Z, double Theta, double U)
		{
			// (X, Y, Z: its place from its skirt's middle as it was; Theta its place round it, U up it)
			const double Flutter = 0.45 * Flap * (0.6 * std::sin(3.0 * Theta + 6.0 * L.Loose + 4.0 * U) + 0.4 * std::sin(7.0 * Theta + 11.0 * L.Loose + 9.0 * U));
			const double Rx = (X + Flutter * std::cos(Theta)) * (1.0 + 0.1 * Sag), Ry = (Y + Flutter * std::sin(Theta)) * (1.0 + 0.1 * Sag);
			const double Rz = Skirt - Pivot + (Z + 0.5 * Flutter) * (1.0 - 0.6 * Sag);
			return V3{ Rx * std::cos(Tip) + Rz * std::sin(Tip), Ry, Pivot - Rx * std::sin(Tip) + Rz * std::cos(Tip) };
		};
		// How it lies, at U along it (0 its hem, gathered, 1 its crown, downwind) and Theta round it as it was (0 the
		// front of it, which falls under; Pi its back, toward him, which lies on top): its two layers of silk, a few long
		// folds in its top one running along it and two creases across it, its far end rounded and standing on the air
		// it holds; over the field.
		auto Folds = [&L](double U, double Across)
		{
			double F = 0.0;
			for (int R = 0; R < L.Ridges; ++R)
			{
				const double Along = Smooth((U - L.From[R]) / 0.08) * (1.0 - Smooth((U - L.To[R]) / 0.15));
				const double Mid = L.At[R] + L.Drift[R] * (U - 0.5) + 0.05 * std::sin(2.0 * Pi * (L.Wave[R] * U + L.Phase[R]));
				const double Aside = (Across - Mid) / L.Width[R];
				F += L.High[R] * Along * (0.8 + 0.2 * std::sin(2.0 * Pi * (0.7 * L.Wave[R] * U + 1.3 * L.Phase[R]))) * std::exp(-Aside * Aside);
			}
			return F;
		};
		auto Lying = [&](double U, double Theta)
		{
			const double Sn = std::sin(Theta), Cs = std::cos(Theta), Top = 0.5 * (1.0 - Cs);
			const int Edge = Sn >= 0.0 ? 0 : 1;
			const double End = U > 0.8 ? std::sqrt(std::max(0.0, 1.0 - std::pow((U - 0.8) / 0.22, 2.0))) : 1.0;
			const double Pocket = Smooth((U - 0.5) / 0.3);
			const double Waver = 1.0 + L.EdgeSwell[Edge] * std::sin(Pi * U) + 0.08 * std::sin(2.0 * Pi * (L.EdgeWave[Edge] * U + L.EdgePhase[Edge]))
				+ 0.04 * std::sin(2.0 * Pi * (2.7 * L.EdgeWave[Edge] * U + L.Loose));
			const double Half = 0.5 * L.Wide * (0.3 + 0.7 * Smooth(U / 0.35)) * (1.0 + L.Bulb * Pocket) * End * Waver;
			const double Axis = Ny * (1.0 - Smooth(U / 0.5)) + L.FarSide * U + L.Bend * std::sin(Pi * U);
			const double X = Nx + L.Long * U + (0.05 + 0.15 * (1.0 - U)) * Cs, Y = Axis + Half * Sn;
			const double F = Folds(U, Sn) * (1.0 - 0.85 * Pocket);
			double Crease = 0.0;
			for (int R = 0; R < 2; ++R)
			{
				const double Off2 = (U - L.Crease[R]) / 0.04;
				Crease += L.CreaseHigh[R] * std::exp(-Off2 * Off2) * (0.6 + 0.4 * std::sin(5.0 * Sn + R));
			}
			const double Bunch = 0.08 * (1.0 - Smooth(U / 0.15));
			const double Loose = 0.03 * std::sin(2.0 * Pi * (1.3 * U + L.Loose)) * std::cos(1.5 * Pi * Sn + L.Loose * 5.0);
			const double Arch = std::sqrt(std::max(0.0, 1.0 - Sn * Sn));
			const double High = 0.045 + Bunch + F + Crease + std::max(0.0, Loose) + L.Lift * Pocket * Arch * End;
			const double Low = Clear + 0.3 * (F + Crease) + 0.05 * Pocket * Arch;
			return V3{ X, Y, Low + (High - Low) * Top + Field(X, Y) };
		};
		for (int R = 0; R < Ringed; ++R)
		{
			double Gr = 0.0, Gz = 0.0, Nr = 0.0, Nz = 0.0;
			Gore(Rings[R], Gr, Gz, Nr, Nz);
			// (its place along it as it lies: its ends' rings closer together, its gathered near end and its far end's
			// rounding)
			const double T = static_cast<double>(R) / Ringed, U = T - 0.4 / (2.0 * Pi) * std::sin(2.0 * Pi * T);
			for (int Col = 0; Col < Cols; ++Col)
			{
				const double Theta = Round(Col, Cols), Out1 = Gr * (1.0 + Billow(Col) * Gr) / Widest();
				const V3 Was = Falling(Ax * Out1 * std::cos(Theta), Ax * Out1 * std::sin(Theta), Dz * Gz, Theta, Gz), Lies = Lying(U, Theta);
				V3 At{ Was.X + (Lies.X - Was.X) * Stream, Was.Y + (Lies.Y - Was.Y) * Stream, Was.Z + (Lies.Z - Was.Z) * Lie };
				At.Z = std::max(At.Z, Field(At.X, At.Y) + Clear);
				Out.Points.push_back(At);
				Out.Facings.push_back({ 0.0, 0.0, 1.0 });
				for (int Ch = 0; Ch < 3; ++Ch) Out.Colours.push_back(C.Silk[Ch] * Hemmed(R, Col));
			}
		}
		{
			// (its crown at its far end's tip, a third of the way up the air it holds there)
			const V3 Was = Falling(0.0, 0.0, Dz, 0.0, 1.0);
			const double Tx = Nx + L.Long;
			const V3 Lies{ Tx, L.FarSide, 0.03 + 0.35 * L.Lift + Field(Tx, L.FarSide) };
			V3 At{ Was.X + (Lies.X - Was.X) * Stream, Was.Y + (Lies.Y - Was.Y) * Stream, Was.Z + (Lies.Z - Was.Z) * Lie };
			At.Z = std::max(At.Z, Field(At.X, At.Y) + Clear);
			Out.Points.push_back(At);
			Out.Facings.push_back({ 0.0, 0.0, 1.0 });
			for (int Ch = 0; Ch < 3; ++Ch) Out.Colours.push_back(C.Silk[Ch]);
		}
		// (each point's facing from where the points about it are: round its ring by up it toward its crown)
		const auto P = [&Out, Cols](int R, int Col) -> const V3& { return Out.Points[static_cast<size_t>(R * Cols + (Col + Cols) % Cols)]; };
		const V3 Crown = Out.Points[static_cast<size_t>(Ringed * Cols)];
		for (int R = 0; R < Ringed; ++R)
		{
			for (int Col = 0; Col < Cols; ++Col)
			{
				const V3 Way = Minus(P(R, Col + 1), P(R, Col - 1));
				const V3 Up = Minus(R + 1 < Ringed ? P(R + 1, Col) : Crown, P(R > 0 ? R - 1 : R, Col));
				Out.Facings[static_cast<size_t>(R * Cols + Col)] = Unit(Cross(Way, Up), { 0.0, 0.0, 1.0 });
			}
		}
		V3 Sum;
		for (int Col = 0; Col < Cols; ++Col) Sum = Plus(Sum, Cross(Minus(P(Ringed - 1, Col), Crown), Minus(P(Ringed - 1, Col + 1), Crown)));
		Out.Facings[static_cast<size_t>(Ringed * Cols)] = Unit(Sum, { 0.0, 0.0, 1.0 });
		Sew(Out, Ringed, Cols);
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
