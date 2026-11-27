// The dogfight probe (Brian, 27 November 2026 01:25Z: "i never see the fighters shooting each other down or dog
// fighting"; "it looks like they loop around vs actually realistic dog fights"; "i want the pilots AI to be very smart
// and realistic as real fighter pilots back then"). It plays a recorded air battle through the game's own flight core
// (Battle/AirShow), reading only what the core hands the scene (Show::Planes, Show::Middle), and measures how its
// fighters fight fighters: how each fighter that falls to a fighter is seen to go (the shooter on its tail and firing,
// or not), how much of a fight is flat circling, how much is up and down, how long one sits on another's tail, how far
// the fights are from the bombers and whether they drift down, and how the fights open (a bounce from above, or not).
//
// Usage: air_dogfight_probe <recording.json> [frames a second, 30] [csv out]
// AIR_DOGFIGHTS=1 flies it with the new pilots (Show::SetDogfights), as the row realDogfights does in the game.
// The cloud test's gate (run.sh): AIR_DOG_TAIL_MORE_THAN=N fails the run unless more than N of the fighters' falls are
// shown with the shooter on the victim's tail and firing (the old pilots' count of the same battle), and
// AIR_DOG_FLAT_UNDER=R unless its flat circles come under R a fighter-minute of fighting.
//
// Definitions (each sample is one frame):
// - A duel frame: a fighter in a fight whose Target is an enemy fighter, Task attacking, evading, covering or breaking.
// - On a tail: within 1 km of its target, inside the target's rear 60-degree cone, its own nose within 30 degrees of it.
// - A flat circle: a run of one fighter's duel frames turning one way, more than 6 deg/s, with its height inside a
//   300 m band, that adds up to 360 degrees of heading. Counted once per 360 degrees.
// - Vertical: |pitch| over 20 degrees.
// - A clean kill shown: a fighter's fall begun with an enemy fighter firing at it in the 2 s before, from inside 600 m,
//   inside its rear 90-degree cone (behind its wing line, within 45 degrees of its tail), or from the front quarter
//   (a head-on pass, counted apart).
// - A bounce: a duel opened (first frame a fighter takes an enemy fighter for its target) from 300 m or more above.

#include "Battle/AirDogfightTypes.h"
#include "Battle/AirFeed.h"
#include "Battle/AirGround.h"
#include "Battle/AirShow.h"
#include "Battle/AirSky.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	constexpr double Pi = 3.14159265358979323846;
	constexpr double Deg = Pi / 180.0;

	using AirShow::Vec;

	Vec NoseOf(const AirShow::PlaneOut& P)
	{
		const double H = P.Heading * Deg, G = P.Pitch * Deg;
		return Vec(std::sin(H) * std::cos(G), std::cos(H) * std::cos(G), std::sin(G));
	}
	double Len(const Vec& V) { return std::sqrt(V.Dot(V)); }
	double Wrap(double A)
	{
		while (A > Pi) A -= 2.0 * Pi;
		while (A < -Pi) A += 2.0 * Pi;
		return A;
	}
	double Median(std::vector<double> V)
	{
		if (V.empty()) return 0.0;
		std::sort(V.begin(), V.end());
		return V[V.size() / 2];
	}
	double Pct(std::vector<double> V, double Q)
	{
		if (V.empty()) return 0.0;
		std::sort(V.begin(), V.end());
		return V[std::min(V.size() - 1, static_cast<size_t>(Q * V.size()))];
	}

	struct Mine
	{
		// a run of turning one way, for the flat circles
		double RunTurn = 0.0, RunLo = 1e18, RunHi = -1e18;
		int RunSign = 0;
		double LastH = 0.0;
		bool bHas = false;
		int LastTarget = -1;
		bool bWasDuel = false;
		bool bFalling = false;
		// what fired on him lately: shooter key -> last air time
		std::map<int, double> FiredOnBy;
		double EvadingAt = -1e9;
		double DuelSince = -1.0;
		double DuelAlt0 = 0.0;
	};
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: air_dogfight_probe <recording.json> [frames-per-second] [csv]\n");
		return 2;
	}
	const std::string Path = Argv[1];
	const double Fps = Argc > 2 ? std::atof(Argv[2]) : 30.0;
	const std::string CsvPath = Argc > 3 ? Argv[3] : std::string();
	std::ifstream In(Path, std::ios::binary);
	if (!In) { std::fprintf(stderr, "cannot read %s\n", Path.c_str()); return 2; }
	std::stringstream Buffer;
	Buffer << In.rdbuf();
	AirFeed::Recording Rec;
	std::string Error;
	if (!AirFeed::ReadRecording(Buffer.str(), Rec, Error)) { std::fprintf(stderr, "%s: %s\n", Path.c_str(), Error.c_str()); return 2; }
	// (AIR_SEED: another throw of the scene's own dice, the battle the same, to see a measure is not one throw's luck)
	const uint64_t Throw = std::getenv("AIR_SEED") ? std::strtoull(std::getenv("AIR_SEED"), nullptr, 10) : 0ull;
	AirShow::Show Show((Rec.Seed ? Rec.Seed : 238) + Throw * 7919ull);
	AirFeed::Apply(Rec.Board, Show);
	if (const char* On = std::getenv("AIR_DOGFIGHTS"); On && std::string(On) == "1")
	{
		// (as the game lays it: the table's types for the battle's date, and the sun where the sky puts it)
		Show.SetDogfights(true);
		const char* TablePath = std::getenv("AIR_PERF");
		std::ifstream TableIn(TablePath ? TablePath : "Content/Data/Air/performance.json", std::ios::binary);
		std::stringstream TableText;
		TableText << TableIn.rdbuf();
		auto Table = std::make_shared<AirPerf::Table>();
		std::string Why;
		if (!AirPerf::Read(TableText.str(), *Table, Why)) { std::fprintf(stderr, "performance table: %s\n", Why.c_str()); return 2; }
		Show.SetTypes(AirDogfightTypes::FinderOf(Table, AirDogfightTypes::YearOf(Rec.Board.Year, Rec.Board.Month + 1)));
		const AirSky::Placing Place = AirFeed::PlacingOf(Rec.Board, Rec.Views.front().Groups, Rec.Seed ? Rec.Seed : 238);
		Show.SetSun(AirShow::Vec(Place.Sun.X, Place.Sun.Y, Place.Sun.Z));
	}
	const std::shared_ptr<const AirGround::BoardLand> BoardGround =
		Rec.Board.StageKey.empty() ? std::make_shared<const AirGround::BoardLand>(Rec.Board.Land) : std::shared_ptr<const AirGround::BoardLand>();
	if (BoardGround && !BoardGround->IsEmpty()) Show.Ground = [BoardGround](double X, double Y) { return BoardGround->At(X, Y); };

	std::ofstream Csv;
	if (!CsvPath.empty())
	{
		Csv.open(CsvPath);
		Csv << "t,key,group,side,model,x,y,z,heading,pitch,bank,speed,task,target,firing,falling,leader\n";
	}

	const double Dt = 1.0 / Fps;
	size_t Taken = 0;
	auto TakeNext = [&]()
	{
		const AirFeed::Frame& F = Rec.Views[Taken++];
		Show.Take(F.Head.T, F.Groups, F.Events, F.Ships, &F.Marks);
	};
	TakeNext();
	if (Taken < Rec.Views.size()) TakeNext();
	double T = Rec.Views.front().Head.T;
	const double EndT = Rec.Views.back().Head.T;

	std::map<int, Mine> State;
	// totals
	double DuelFrames = 0, TailFrames = 0, VertFrames = 0, FiringFrames = 0, TurningFrames = 0, FrameCount = 0;
	int FlatCircles = 0, Bounces = 0, DuelOpens = 0, HeadOnOpens = 0;
	std::vector<double> DuelFromBombers, DuelAltBelowBombers, DuelSpeed, DuelTurn, FireRange, DuelLength, DuelSink;
	int FighterFalls = 0, CleanTail = 0, CleanHeadOn = 0, FiredNoAspect = 0, NoShooter = 0, VictimUnaware = 0;
	std::vector<double> KillRange, KillFromBombers;
	int CsvEvery = std::max(1, static_cast<int>(std::lround(Fps / 2.0))), FrameNo = 0;

	while (T < EndT - 1e-9)
	{
		T = std::min(EndT, T + Dt);
		while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
		Show.Advance(T);
		++FrameNo;
		const double AirT = Show.Shown();
		const std::vector<AirShow::PlaneOut>& Planes = Show.Planes();
		std::map<int, const AirShow::PlaneOut*> ByKey;
		std::vector<Vec> BomberMids;
		std::set<int> BomberGroups;
		for (const AirShow::PlaneOut& P : Planes)
		{
			ByKey[P.Key] = &P;
			if (!P.bFighter && P.bShown && !P.bFalling && !P.bOnGround) BomberGroups.insert(P.Group);
		}
		double BomberAlt = 0.0;
		for (int Gid : BomberGroups)
		{
			Vec M;
			double Face = 0.0;
			if (Show.Middle(Gid, M, Face)) { BomberMids.push_back(M); BomberAlt = std::max(BomberAlt, M.Z); }
		}
		auto FromBombers = [&](const Vec& At)
		{
			double Best = 1e18;
			for (const Vec& M : BomberMids) Best = std::min(Best, Len(At - M));
			return Best;
		};
		for (const AirShow::PlaneOut& P : Planes)
		{
			if (!P.bFighter || !P.bShown || P.bOnGround) continue;
			Mine& S = State[P.Key];
			if (Csv.is_open() && FrameNo % CsvEvery == 0)
			{
				char Line[512];
				std::snprintf(Line, sizeof Line, "%.2f,%d,%d,%d,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%s,%d,%d,%d,%d\n", AirT, P.Key, P.Group, P.Side,
					P.Model.c_str(), P.At.X, P.At.Y, P.At.Z, P.Heading, P.Pitch, P.Bank, P.Speed, P.Task.c_str(), P.Target, P.bFiring ? 1 : 0, P.bFalling ? 1 : 0, P.Leader);
				Csv << Line;
			}
			// A fall begun: was it seen?
			if (P.bFalling && !S.bFalling)
			{
				S.bFalling = true;
				// the shooters that fired on him in the last 2 s
				int Best = -1;
				double BestT = -1e9;
				for (const auto& [By, At] : S.FiredOnBy) if (AirT - At < 2.0 && At > BestT) { Best = By; BestT = At; }
				bool bByFighter = false;
				for (const AirShow::PlaneOut& Q : Planes) if (Q.Key == Best && Q.bFighter) bByFighter = true;
				// only falls the battle gave to a fighter count (we read the shooter from who fired; those with none we count apart)
				if (Best < 0) { ++NoShooter; ++FighterFalls; continue; }
				if (!bByFighter) continue;
				++FighterFalls;
				const AirShow::PlaneOut* Q = ByKey.count(Best) ? ByKey[Best] : nullptr;
				if (!Q) { ++FiredNoAspect; continue; }
				const Vec Rel = Q->At - P.At; // from victim to shooter
				const double D = Len(Rel);
				const Vec VNose = NoseOf(P);
				const double Behind = -VNose.Dot(Rel) / std::max(D, 1.0); // 1: dead astern
				KillRange.push_back(D);
				KillFromBombers.push_back(FromBombers(P.At));
				if (D < 600.0 && Behind > std::cos(45.0 * Deg)) ++CleanTail;
				else if (D < 900.0 && Behind < -std::cos(45.0 * Deg)) ++CleanHeadOn;
				else ++FiredNoAspect;
				if (AirT - S.EvadingAt > 5.0) ++VictimUnaware;
				continue;
			}
			if (P.bFalling) continue;
			// fired on
			if (P.bFiring && P.Target >= 0) State[P.Target].FiredOnBy[P.Key] = AirT;
			if (P.Task == "evading") S.EvadingAt = AirT;
			const AirShow::PlaneOut* Tg = P.Target >= 0 && ByKey.count(P.Target) ? ByKey[P.Target] : nullptr;
			const bool bDuel = Tg && Tg->bFighter && Tg->Side != P.Side && !Tg->bFalling &&
				(P.Task == "attacking" || P.Task == "evading" || P.Task == "covering" || P.Task == "breaking" || P.Task == "positioning");
			const bool bInFight = bDuel || P.Task == "evading";
			if (bDuel && (P.Target != S.LastTarget || !S.bWasDuel))
			{
				++DuelOpens;
				const double Above = P.At.Z - Tg->At.Z;
				if (Above > 300.0) ++Bounces;
				const Vec Rel = Tg->At - P.At;
				if (NoseOf(*Tg).Dot(Rel) < -0.7 * Len(Rel)) ++HeadOnOpens;
				S.DuelSince = AirT;
				S.DuelAlt0 = P.At.Z;
			}
			if (!bDuel && S.bWasDuel && S.DuelSince >= 0.0)
			{
				const double L = AirT - S.DuelSince;
				if (L > 2.0)
				{
					DuelLength.push_back(L);
					DuelSink.push_back((S.DuelAlt0 - P.At.Z) / L * 60.0);
				}
				S.DuelSince = -1.0;
			}
			S.bWasDuel = bDuel;
			S.LastTarget = P.Target;
			++FrameCount;
			const double H = P.Heading * Deg;
			double Turn = 0.0;
			if (S.bHas) Turn = Wrap(H - S.LastH) / Dt;
			S.LastH = H;
			S.bHas = true;
			if (!bInFight)
			{
				S.RunTurn = 0.0;
				S.RunSign = 0;
				S.RunLo = 1e18;
				S.RunHi = -1e18;
				continue;
			}
			DuelFrames += 1.0;
			DuelSpeed.push_back(P.Speed);
			DuelTurn.push_back(std::fabs(Turn) / Deg);
			if (std::fabs(P.Pitch) > 20.0) VertFrames += 1.0;
			if (P.bFiring) FiringFrames += 1.0;
			if (BomberMids.size()) DuelFromBombers.push_back(FromBombers(P.At));
			if (BomberAlt > 0.0) DuelAltBelowBombers.push_back(BomberAlt - P.At.Z);
			if (Tg)
			{
				const Vec Rel = Tg->At - P.At;
				const double D = Len(Rel);
				const bool bInCone = NoseOf(*Tg).Dot(Rel) < -std::cos(30.0 * Deg) * D; // I am behind him: his nose points away from me
				const bool bNoseOn = NoseOf(P).Dot(Rel) > std::cos(30.0 * Deg) * D;
				if (D < 1000.0 && bInCone && bNoseOn) TailFrames += 1.0;
				if (P.bFiring) FireRange.push_back(D);
			}
			// flat circles
			const int Sign = std::fabs(Turn) > 6.0 * Deg ? (Turn > 0 ? 1 : -1) : 0;
			if (Sign != 0) TurningFrames += 1.0;
			if (Sign == 0 || Sign != S.RunSign)
			{
				S.RunSign = Sign;
				S.RunTurn = 0.0;
				S.RunLo = S.RunHi = P.At.Z;
			}
			if (Sign != 0)
			{
				S.RunTurn += std::fabs(Turn) * Dt;
				S.RunLo = std::min(S.RunLo, P.At.Z);
				S.RunHi = std::max(S.RunHi, P.At.Z);
				if (S.RunHi - S.RunLo > 300.0)
				{
					// not flat: start over from here
					S.RunTurn = 0.0;
					S.RunLo = S.RunHi = P.At.Z;
				}
				else if (S.RunTurn >= 2.0 * Pi)
				{
					++FlatCircles;
					S.RunTurn -= 2.0 * Pi;
				}
			}
		}
	}
	const double DuelSeconds = DuelFrames * Dt;
	std::printf("%s (%s)  %s pilots\n", Rec.Key.c_str(), Rec.Board.Title.c_str(), Show.Dogfights() ? "NEW" : "old");
	std::printf("  fighter falls to fighters %d: shooter on his tail and firing %d (%.0f%%), head-on %d, firing from another angle %d, no shooter seen firing %d; victim never broke %d\n",
		FighterFalls, CleanTail, FighterFalls ? 100.0 * CleanTail / FighterFalls : 0.0, CleanHeadOn, FiredNoAspect, NoShooter, VictimUnaware);
	std::printf("  kill range median %.0f m; kills from the bombers median %.0f m\n", Median(KillRange), Median(KillFromBombers));
	std::printf("  duel time %.0f fighter-s; on a tail %.1f%%; vertical (|pitch|>20) %.1f%%; firing %.1f%%; turning %.1f%%\n", DuelSeconds,
		DuelFrames ? 100.0 * TailFrames / DuelFrames : 0.0, DuelFrames ? 100.0 * VertFrames / DuelFrames : 0.0, DuelFrames ? 100.0 * FiringFrames / DuelFrames : 0.0,
		DuelFrames ? 100.0 * TurningFrames / DuelFrames : 0.0);
	std::printf("  flat circles %d (%.2f a fighter-minute of fighting)\n", FlatCircles, DuelSeconds > 0 ? FlatCircles / (DuelSeconds / 60.0) : 0.0);
	std::printf("  duels opened %d: bounces from 300 m above %d (%.0f%%), head-on %d\n", DuelOpens, Bounces, DuelOpens ? 100.0 * Bounces / DuelOpens : 0.0, HeadOnOpens);
	std::printf("  duel length median %.1f s (p90 %.1f); height lost a minute in a duel median %.0f m\n", Median(DuelLength), Pct(DuelLength, 0.9), Median(DuelSink));
	std::printf("  fighting from the bombers median %.0f m (p90 %.0f); below the bombers median %.0f m\n", Median(DuelFromBombers), Pct(DuelFromBombers, 0.9), Median(DuelAltBelowBombers));
	std::printf("  speed median %.0f m/s; turn median %.1f deg/s (p90 %.1f); firing range median %.0f m (p10 %.0f, p90 %.0f)\n", Median(DuelSpeed), Median(DuelTurn), Pct(DuelTurn, 0.9),
		Median(FireRange), Pct(FireRange, 0.1), Pct(FireRange, 0.9));
	int Failed = 0;
	if (const char* More = std::getenv("AIR_DOG_TAIL_MORE_THAN"))
	{
		const int Least = std::atoi(More);
		const bool bPass = CleanTail > Least;
		std::printf("  gate: falls shown on a tail %d, more than %d: %s\n", CleanTail, Least, bPass ? "pass" : "FAIL");
		Failed += bPass ? 0 : 1;
	}
	if (const char* Under = std::getenv("AIR_DOG_FLAT_UNDER"))
	{
		const double Most = std::atof(Under);
		const double Rate = DuelSeconds > 0 ? FlatCircles / (DuelSeconds / 60.0) : 0.0;
		const bool bPass = Rate < Most;
		std::printf("  gate: flat circles %.2f a fighter-minute, under %.2f: %s\n", Rate, Most, bPass ? "pass" : "FAIL");
		Failed += bPass ? 0 : 1;
	}
	return Failed ? 1 : 0;
}
