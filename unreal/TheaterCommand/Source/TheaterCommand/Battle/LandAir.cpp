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
		// Spitfire from 1943, both of which strafed in Normandy). The Germans the Stuka till the Fw 190's fighter-bomber
		// took its place (1943); the Russians always the Il-2, low and shallow; a nation with none of its own a plain
		// fighter in its paint.
		Machine M;
		const bool bEmpire = Nat == "UK" || Nat == "CAN" || Nat == "AUS" || Nat == "NZL" || Nat == "SAF" || Nat == "IND";
		if (Nat == "GER" && Year < 1943)
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
