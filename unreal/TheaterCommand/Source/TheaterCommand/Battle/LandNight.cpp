#include "Battle/LandNight.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace LandNight
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		double Smooth(double U)
		{
			U = std::clamp(U, 0.0, 1.0);
			return U * U * (3.0 - 2.0 * U);
		}

		Vec Flat(const Vec& V)
		{
			const double L = std::hypot(V.X, V.Y);
			return L > 1e-9 ? Vec(V.X / L, V.Y / L, 0.0) : Vec(0.0, 1.0, 0.0);
		}

		/** The right hand of a way along the ground (field X east, Y north). */
		Vec RightOf(const Vec& Way)
		{
			return Vec(Way.Y, -Way.X, 0.0);
		}

		void Put(std::string& Out, double V)
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%.1f", V);
			Out += Buffer;
		}
	}

	const char* NameOf(Beat B)
	{
		switch (B)
		{
		case Beat::Out: return "out";
		case Beat::Near: return "near";
		case Beat::Flare: return "flare";
		default: return "dawn";
		}
	}

	bool Prelude::Begin(const LandFeed::Recording& InRec, std::function<double(double, double)> Height, std::function<bool(const Vec&)> Free,
		double InStart, const Settings& InSettings)
	{
		*this = Prelude();
		const LandFeed::Patrol& N = InRec.Night;
		if (!N.bOn || N.Route.size() < 2) return false;
		Rec = &InRec;
		HeightAt = std::move(Height);
		FreeAt = std::move(Free);
		Set = InSettings;
		Start = InStart;
		for (size_t K = 0; K < InRec.Tracks.size(); ++K)
		{
			const LandFeed::Track& T = InRec.Tracks[K];
			if (LandFeed::IsPatrolMan(T) && T.Block == N.Unit && !T.At.empty()) Patrol.push_back(static_cast<int>(K));
		}
		std::sort(Patrol.begin(), Patrol.end(), [&InRec](int A, int B) { return InRec.Tracks[static_cast<size_t>(A)].K < InRec.Tracks[static_cast<size_t>(B)].K; });
		if (Patrol.empty()) return false;
		const LandFeed::Track& Lead = InRec.Tracks[static_cast<size_t>(Patrol.front())];
		const LandFeed::Track& Last = InRec.Tracks[static_cast<size_t>(Patrol.back())];
		const double Begun = N.Route.front().T, Home = std::max(N.Route.back().T, Last.At.back().T);

		// The shots: going out (all of them moving), near the post found (the leader at his nearest, a breath before) or,
		// with none found, at the far end of the way; under the first flare (a breath before it bursts). In battle time's
		// order, none running into the next.
		std::vector<Shot> Want;
		{
			// (once the last man of the file has stepped off: the leader's way, its pace, his place in the file)
			double Way = 0.0;
			for (size_t K = 1; K < N.Route.size(); ++K) Way += std::hypot(N.Route[K].X - N.Route[K - 1].X, N.Route[K].Y - N.Route[K - 1].Y);
			const double Pace = std::max(0.3, Way / std::max(1.0, N.Route.back().T - Begun));
			const double Moving = Begun + 4.0 + (Last.K - Lead.K) * LandFeed::PatrolGap / Pace;
			Want.push_back({ Beat::Out, std::min(Moving, Begun + 0.25 * (Home - Begun)), Set.OutFor });
		}
		{
			double Best = 1e18, BestT = Begun;
			for (double T = Begun; T <= Home; T += 2.0)
			{
				const LandFeed::Spot S = LandFeed::SpotAt(Lead, T);
				const double D = N.bFound ? std::hypot(S.X - N.FoundX, S.Y - N.FoundY) : -std::hypot(S.X - N.Route.front().X, S.Y - N.Route.front().Y);
				if (D < Best - 0.25)
				{
					Best = D;
					BestT = T;
				}
			}
			Want.push_back({ Beat::Near, BestT - 0.6 * Set.NearFor * Set.Rate, Set.NearFor });
		}
		for (const LandFeed::Patrol::Point& F : N.Flares)
		{
			if (F.T > Begun && F.T < Home)
			{
				Want.push_back({ Beat::Flare, F.T - 2.0, Set.FlareFor });
				break;
			}
		}
		std::stable_sort(Want.begin(), Want.end(), [](const Shot& A, const Shot& B) { return A.From < B.From; });
		// (a shot that would run into the next is moved back to end where the next begins, or left out if that is before
		// the one before it ends)
		for (size_t K = Want.size(); K-- > 1;)
		{
			const double Ends = Want[K - 1].From + Want[K - 1].Real * Set.Rate;
			if (Ends > Want[K].From) Want[K - 1].From = Want[K].From - Want[K - 1].Real * Set.Rate;
		}
		double Free0 = Begun;
		for (const Shot& S : Want)
		{
			if (S.From < Free0 - 1e-6) continue;
			Plan.push_back(S);
			Free0 = S.From + S.Real * Set.Rate;
		}
		if (Plan.empty()) return false;
		NightEnds = 0.0;
		for (const Shot& S : Plan) NightEnds += S.Real;
		bBegun = true;
		return true;
	}

	Vec Prelude::MenMiddle(double T, Vec* OutLead, Vec* OutLast) const
	{
		Vec Sum;
		int Men = 0;
		for (int Ix : Patrol)
		{
			const LandFeed::Track& Tr = Rec->Tracks[static_cast<size_t>(Ix)];
			if (Tr.Out >= 0.0 && T > Tr.Out) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, T);
			const Vec At(S.X, S.Y, HeightAt ? HeightAt(S.X, S.Y) : 0.0);
			if (Men == 0 && OutLead) *OutLead = At;
			if (OutLast) *OutLast = At;
			Sum = Sum + At;
			++Men;
		}
		return Men ? Sum * (1.0 / Men) : Vec(Rec->Night.Route.front().X, Rec->Night.Route.front().Y, HeightAt ? HeightAt(Rec->Night.Route.front().X, Rec->Night.Route.front().Y) : 0.0);
	}

	Vec Prelude::Stand(const Vec& Want, double Over) const
	{
		// The eye at a place Over metres over the ground there; where it cannot stand (a bank, a hedge, a house), the
		// nearest of a few places round it that it can.
		const auto At = [this, Over](double X, double Y) { return Vec(X, Y, (HeightAt ? HeightAt(X, Y) : 0.0) + Over); };
		Vec Best = At(Want.X, Want.Y);
		if (!FreeAt || FreeAt(Best)) return Best;
		for (int Ring = 1; Ring <= 3; ++Ring)
		{
			for (int K = 0; K < 8; ++K)
			{
				const double A = K * Pi / 4.0;
				const Vec Try = At(Want.X + std::cos(A) * 1.5 * Ring, Want.Y + std::sin(A) * 1.5 * Ring);
				if (FreeAt(Try)) return Try;
			}
		}
		return Best;
	}

	LandEye::Pose Prelude::PoseFor(const Shot& S, double T, double RealDt, bool bFirst)
	{
		LandEye::Pose P;
		P.bCut = bFirst;
		Vec Lead, Last;
		const Vec Middle = MenMiddle(T, &Lead, &Last);
		const LandFeed::Patrol& N = Rec->Night;
		if (S.Kind == Beat::Out)
		{
			// Low behind the file and a little to its side, going with it: the men ahead, the enemy's ground beyond.
			const Vec Way = Flat(Lead - Last);
			const Vec Want = Last - Way * 7.0 + RightOf(Way) * 2.5;
			const Vec Eye = Stand(Want, 1.1);
			const Vec Look = Middle + Way * 4.0 + Vec(0.0, 0.0, 0.9);
			if (bFirst || !bEyeSet)
			{
				EyeAt = Eye;
				LookAt = Look;
			}
			else
			{
				// (eased after them: a corner of the way turns the camera, not jerks it)
				const double Share = std::min(1.0, RealDt * 1.2);
				EyeAt = EyeAt + (Eye - EyeAt) * Share;
				LookAt = LookAt + (Look - LookAt) * Share;
			}
			P.Eye = EyeAt;
			P.Look = LookAt;
			P.Fov = 48.0;
			P.Shot = LandEye::Kind::Track;
		}
		else if (S.Kind == Beat::Near)
		{
			// Down in the grass behind the last man of the file and to its side, along the men to the post they found (or
			// out at the enemy's ground from the far end of the way): set once, the men crawling up it. The lens turned to
			// take in every man and the post, and widened if they need it.
			if (bFirst || !bEyeSet)
			{
				const Vec Toward = N.bFound ? Vec(N.FoundX, N.FoundY, HeightAt ? HeightAt(N.FoundX, N.FoundY) : 0.0) : Lead + Flat(Lead - Vec(N.Route.front().X, N.Route.front().Y, 0.0)) * 150.0;
				const Vec Way = Flat(Toward - Last);
				EyeAt = Stand(Last - Way * 4.0 + RightOf(Way) * 1.8, 0.8);
				// (each man's and the post's angle off the way, seen from the eye; the lens on the middle of the widest two)
				double Lo = 1e9, Hi = -1e9;
				const auto Take = [&](const Vec& At)
				{
					const double Dx = At.X - EyeAt.X, Dy = At.Y - EyeAt.Y;
					const double A = std::atan2(Way.X * Dy - Way.Y * Dx, Way.X * Dx + Way.Y * Dy);
					Lo = std::min(Lo, A);
					Hi = std::max(Hi, A);
				};
				Take(Toward);
				for (int Ix : Patrol)
				{
					const LandFeed::Track& Tr = Rec->Tracks[static_cast<size_t>(Ix)];
					if (Tr.Out >= 0.0 && T > Tr.Out) continue;
					const LandFeed::Spot Sp = LandFeed::SpotAt(Tr, T);
					Take(Vec(Sp.X, Sp.Y, 0.0));
				}
				const double Mid = 0.5 * (Lo + Hi);
				NearFov = std::clamp((Hi - Lo) * 180.0 / Pi + 10.0, 38.0, 64.0);
				const Vec Dir(Way.X * std::cos(Mid) - Way.Y * std::sin(Mid), Way.X * std::sin(Mid) + Way.Y * std::cos(Mid), 0.0);
				// (tipped 3 degrees down: the near man's back and the post both in)
				LookAt = EyeAt + Dir * 60.0 - Vec(0.0, 0.0, 60.0 * std::tan(3.0 * Pi / 180.0));
			}
			P.Eye = EyeAt;
			P.Look = LookAt;
			P.Fov = NearFov;
			P.Shot = LandEye::Kind::Low;
		}
		else
		{
			// Behind the men lying flat, the camera on the ground and tipped up at the flare hanging over the enemy.
			if (bFirst || !bEyeSet)
			{
				Vec Over = Middle + Vec(0.0, 60.0, 0.0);
				for (const LandFeed::Patrol::Point& F : N.Flares)
				{
					if (F.T >= S.From)
					{
						Over = Vec(F.X, F.Y, (HeightAt ? HeightAt(F.X, F.Y) : 0.0) + Set.FlareHigh);
						break;
					}
				}
				Vec Way = Flat(Over - Middle);
				if (std::hypot(Over.X - Middle.X, Over.Y - Middle.Y) < 20.0) Way = Flat(Lead - Last);
				EyeAt = Stand(Middle - Way * 8.0 + RightOf(Way) * 2.0, 0.7);
				const double Away = std::max(20.0, std::hypot(Over.X - EyeAt.X, Over.Y - EyeAt.Y));
				const double Rise = std::atan2(Over.Z - EyeAt.Z, Away);
				const double Pitch = std::clamp(Rise - 12.0 * Pi / 180.0, 4.0 * Pi / 180.0, 22.0 * Pi / 180.0);
				LookAt = EyeAt + (Way * std::cos(Pitch) + Vec(0.0, 0.0, std::sin(Pitch))) * 60.0;
			}
			P.Eye = EyeAt;
			P.Look = LookAt;
			P.Fov = 60.0;
			P.Shot = LandEye::Kind::Wide;
		}
		bEyeSet = true;
		return P;
	}

	Frame Prelude::Update(double RealDt)
	{
		Frame F;
		if (!bBegun) return F;
		Real += std::max(0.0, RealDt);
		const double Dip = Set.DipFor * 0.5;
		if (Real < NightEnds + Dip)
		{
			// The night's shots, then its last held as the picture goes down to black.
			double Into = std::min(Real, NightEnds - 1e-6);
			size_t K = 0;
			while (K + 1 < Plan.size() && Into >= Plan[K].Real)
			{
				Into -= Plan[K].Real;
				++K;
			}
			const bool bFirst = static_cast<int>(K) != ShotNow;
			ShotNow = static_cast<int>(K);
			const Shot& S = Plan[K];
			const double Over = Real > NightEnds ? Real - NightEnds : 0.0;
			F.bNight = true;
			F.Kind = S.Kind;
			F.Battle = S.From + (Into + Over) * Set.Rate;
			F.Pose = PoseFor(S, F.Battle, RealDt, bFirst);
			F.Dark = 1.0;
			F.Black = Smooth(Over / Dip);
			FlaresAt(Rec->Night, F.Battle, HeightAt, Set, F.Flares);
			return F;
		}
		// The battle's own, the picture coming up from black and the light rising to the day's.
		const double Since = Real - NightEnds - Dip;
		F.bNight = false;
		F.Kind = Beat::Dawn;
		F.Battle = Start;
		F.Black = 1.0 - Smooth(Since / Dip);
		F.Dark = 1.0 - Smooth(Since / std::max(0.1, Set.DawnFor));
		ShotNow = static_cast<int>(Plan.size());
		return F;
	}

	void Prelude::Skip()
	{
		if (bBegun && Real < NightEnds) Real = NightEnds;
	}

	void FlaresAt(const LandFeed::Patrol& P, double T, const std::function<double(double, double)>& Height, const Settings& S, std::vector<Flare>& Out)
	{
		Out.clear();
		for (size_t K = 0; K < P.Flares.size(); ++K)
		{
			const LandFeed::Patrol::Point& Fl = P.Flares[K];
			const double Since = T - Fl.T;
			if (Since < -S.FlareRise || Since > LandFeed::PatrolFlareBurns) continue;
			const double Ground = Height ? Height(Fl.X, Fl.Y) : 0.0;
			Flare F;
			F.Id = static_cast<int>(K);
			if (Since < 0.0)
			{
				// (climbing from the gun: a dull spark)
				const double U = 1.0 + Since / S.FlareRise;
				F.bRising = true;
				F.At = Vec(Fl.X, Fl.Y, Ground + S.FlareHigh * (0.35 + 0.65 * U));
				F.Light = 0.06;
			}
			else
			{
				// (burst: swinging under its parachute as it sinks, flickering, dying in its last two seconds)
				const double Swing = 2.0 * Pi * Since / S.FlareSwingFor;
				F.At = Vec(Fl.X + std::sin(Swing) * S.FlareSwing, Fl.Y + std::cos(Swing * 0.5) * S.FlareSwing * 0.5,
					Ground + std::max(20.0, S.FlareHigh - S.FlareFall * Since));
				const double Up = std::min(1.0, Since / 0.3);
				const double Dies = std::clamp((LandFeed::PatrolFlareBurns - Since) / 2.0, 0.0, 1.0);
				const double Flicker = 0.93 + 0.07 * std::sin(T * 37.0 + K) * std::sin(T * 23.0 + 2.0 * K);
				F.Light = Up * Dies * Flicker;
			}
			Out.push_back(F);
		}
	}

	std::string StandInPatrol(const LandFeed::Recording& Rec, bool bFound, bool bCaught)
	{
		if (Rec.Views.empty()) return std::string();
		const auto IsFoot = [&Rec](int Id)
		{
			for (const LandFeed::Block& B : Rec.Blocks)
			{
				if (B.Id == Id) return B.Model.empty() && B.Team.empty();
			}
			return false;
		};
		// The attacker's foot block nearest the defender's, and that one.
		const LandFeed::View& V = Rec.Views.front();
		const LandFeed::Unit* From = nullptr;
		const LandFeed::Unit* To = nullptr;
		double Best = 1e18;
		for (const LandFeed::Unit& A : V.Units)
		{
			if (A.Side != 0 || !IsFoot(A.Id)) continue;
			for (const LandFeed::Unit& D : V.Units)
			{
				if (D.Side != 1 || !IsFoot(D.Id)) continue;
				const double Gap = std::hypot(D.X - A.X, D.Y - A.Y);
				if (Gap < Best)
				{
					Best = Gap;
					From = &A;
					To = &D;
				}
			}
		}
		if (!From || !To || Best < 200.0) return std::string();
		const double Ux = (To->X - From->X) / Best, Uy = (To->Y - From->Y) / Best;
		const double Short = Best - 120.0;
		const double HaltX = From->X + Ux * Short, HaltY = From->Y + Uy * Short;
		// (out and back in 1,700 s with 100 s at the halt, as the rules' night: -2400 to -600)
		const double T0 = -2400.0, T1 = -600.0, Each = 0.5 * (T1 - T0 - 100.0);
		std::string J = "{\"side\":0,\"unit\":" + std::to_string(From->Id) + ",\"t0\":-2400,\"t1\":-600,\"men\":4";
		if (bFound)
		{
			J += ",\"found\":{\"x\":";
			Put(J, To->X);
			J += ",\"y\":";
			Put(J, To->Y);
			J += ",\"kind\":\"mg\"}";
		}
		if (bCaught)
		{
			const double At = T0 + Each - 60.0;
			const double Share = (At - T0) / Each;
			J += ",\"alerted\":";
			Put(J, At);
			J += ",\"flares\":[[";
			Put(J, At);
			J += ",";
			Put(J, From->X + Ux * Short * Share + Uy * 25.0);
			J += ",";
			Put(J, From->Y + Uy * Short * Share - Ux * 25.0);
			J += "]]";
		}
		const auto Point = [&J](double T, double X, double Y, bool bComma)
		{
			if (bComma) J += ",";
			J += "[";
			Put(J, T);
			J += ",";
			Put(J, X);
			J += ",";
			Put(J, Y);
			J += "]";
		};
		J += ",\"route\":[";
		Point(T0, From->X, From->Y, false);
		Point(T0 + Each, HaltX, HaltY, true);
		Point(T0 + Each + 100.0, HaltX, HaltY, true);
		Point(T1, From->X, From->Y, true);
		J += "]}";
		return J;
	}

	int AddStandInPatrol(std::string& Text, LandFeed::Recording& Rec, bool bFound, bool bCaught, std::string& OutError)
	{
		if (Rec.Night.bOn) return 0;
		const std::string Root = StandInPatrol(Rec, bFound, bCaught);
		const size_t Close = Text.find_last_of('}');
		if (Root.empty() || Close == std::string::npos) return 0;
		Text.insert(Close, ",\"patrol\":" + Root);
		const int Men = LandFeed::AddPatrolMen(Text);
		if (!LandFeed::ReadRecording(Text, Rec, OutError)) return 0;
		return Men;
	}
}
