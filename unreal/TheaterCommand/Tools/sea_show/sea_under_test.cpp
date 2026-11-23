// The view under the water's cloud test (U1), run by Tools/sea_show/run.sh: the game's own Battle/SeaUnderCore, the sea
// show and its feed (no engine), over every recorded action and the stand-ins. The director is fed as ASeaBattle::PlanShots
// feeds it (a quarter second a step, the same trial), against the very shots PlanShots plans (mirrored here, each line it
// is mirrored from pinned in the source); the look is played as the scene plays it (30 frames a second), its sun the
// stage's own. Usage: sea_under_test <Source/TheaterCommand> recording.json ...
#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"
#include "Battle/SeaUnderCore.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace SeaUnder;

namespace
{
	constexpr double Pi = 3.14159265358979323846;
	int Checks = 0, Failed = 0;

	void Check(const std::string& What, bool bOk, const std::string& Got)
	{
		++Checks;
		Failed += bOk ? 0 : 1;
		std::printf("  %s  %s: %s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.c_str());
	}

	void Note(const std::string& What, const std::string& Got) { std::printf("  note  %s: %s\n", What.c_str(), Got.c_str()); }

	std::string Num(double X, int Places = 1)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, X);
		return Text;
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream File(Path);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}

	double Gap(const P3& A, const P3& B) { return std::sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y) + (A.Z - B.Z) * (A.Z - B.Z)); }

	/** In the picture: inside the frame the lens gives (its width across, 16:10), the eye level (no roll), as the camera has it. */
	bool InPicture(const EyePose& E, const P3& P)
	{
		const P3 F = E.Look - E.Eye, V = P - E.Eye;
		const double LF = Gap(E.Look, E.Eye);
		if (LF < 1e-6) return false;
		const P3 Fwd = F * (1.0 / LF);
		P3 Side(Fwd.Y, -Fwd.X, 0.0); // (level, to the right of the way it looks)
		const double LS = std::hypot(Side.X, Side.Y);
		if (LS < 1e-6) Side = P3(1.0, 0.0, 0.0);
		else Side = Side * (1.0 / LS);
		const P3 Upward(Side.Y * Fwd.Z - Side.Z * Fwd.Y, Side.Z * Fwd.X - Side.X * Fwd.Z, Side.X * Fwd.Y - Side.Y * Fwd.X);
		const double Z = V.X * Fwd.X + V.Y * Fwd.Y + V.Z * Fwd.Z;
		if (Z <= 0.0) return false;
		const double Across = std::tan(E.Lens * 0.5 * Pi / 180.0);
		const double X = V.X * Side.X + V.Y * Side.Y + V.Z * Side.Z, Y = V.X * Upward.X + V.Y * Upward.Y + V.Z * Upward.Z;
		return std::fabs(X) / Z <= Across && std::fabs(Y) / Z <= Across * 10.0 / 16.0;
	}

	// -------------------------------------------------------------------------------------------------------------
	// The game's own lines this test mirrors: each must still read as it did, or the mirror is stale.

	struct Pin { const char* File; const char* Line; };
	const Pin Pins[] = {
		// the stage's sun (OpenOn and SeaParts::SunAt)
		{ "Battle/SeaBattle.cpp", "double Hour = 8.0 + static_cast<double>((Seed * 2654435761ull) % 7000ull) / 1000.0;" },
		{ "Battle/SeaBattle.cpp", "SunElevation = FMath::Clamp(SunElevation, 12.0, 70.0);" },
		{ "Battle/SeaBattle.cpp", "const double Day = 30.4 * FMath::Clamp(Month0, 0, 11) + 15.0;" },
		{ "Battle/SeaBattle.cpp", "const double Decl = FMath::DegreesToRadians(23.44) * FMath::Sin(2.0 * UE_DOUBLE_PI * (284.0 + Day) / 365.0);" },
		{ "Battle/SeaBattle.cpp", "const double Phi = FMath::DegreesToRadians(FMath::Clamp(Latitude, -80.0, 80.0));" },
		{ "Battle/SeaBattle.cpp", "const double H = FMath::DegreesToRadians(15.0 * (Hour - 12.0));" },
		{ "Battle/SeaBattle.cpp", "const double SinUp = FMath::Sin(Phi) * FMath::Sin(Decl) + FMath::Cos(Phi) * FMath::Cos(Decl) * FMath::Cos(H);" },
		{ "Battle/SeaBattle.cpp", "const double Bearing = FMath::Atan2(-FMath::Sin(H) * FMath::Cos(Decl), FMath::Sin(Decl) * FMath::Cos(Phi) - FMath::Cos(Decl) * FMath::Sin(Phi) * FMath::Cos(H));" },
		{ "Battle/SeaBattle.cpp", "const float Want = (bMain ? 7.0f : 0.75f) * (Under ? Under->LightShare() : 1.0f);" },
		// PlanShots: the trial, its eye, and the shots and their moments
		{ "Battle/SeaBattle.cpp", "static constexpr double CutBefore = 4.0;" },
		{ "Battle/SeaBattle.cpp", "Trial.Eye = SeaShow::Vec(Mid, Mid - 3000.0, 900.0);" },
		{ "Battle/SeaBattle.cpp", "Trial.Eye = SeaShow::Vec(X / Count, Y / Count - 3000.0, 900.0);" },
		{ "Battle/SeaBattle.cpp", "for (double T = S.BeginT; T <= S.EndT + 1e-6; T += 0.25, ++Step)" },
		{ "Battle/SeaBattle.cpp", "UnderPlan.Step(T, Trial); // U1 hook (under water): the step's effects read before they go" },
		{ "Battle/SeaBattle.cpp", "UnderPlan.Told(F.Head.T, F.Pieces, F.Events); // U1 hook (under water): its events and the boats' hunters" },
		{ "Battle/SeaBattle.cpp", "UnderPlan.Begin(S.Rec.Board, SeaFeed::SideOf(S.Rec.Views.front().Head.Me) == 1 ? 1 : 0, Under ? Under->UnheldClear() : SeaUnder::UnheldClearBy());" },
		{ "Battle/SeaBattle.cpp", "Add(Fleets, TEXT(\"the-fleets\"), Contact >= 0.0 ? FMath::Max(S.BeginT + 12.0, Contact - 40.0) : S.BeginT + 30.0);" },
		{ "Battle/SeaBattle.cpp", "double LineAt = Line.At + 20.0;" },
		{ "Battle/SeaBattle.cpp", "if (Broadside.At >= 0.0 && FMath::Abs(LineAt - Broadside.At) < 8.0 && Broadside.At - 8.0 >= Line.At + 10.0)" },
		{ "Battle/SeaBattle.cpp", "Add(Broadside, TEXT(\"broadside\"), Broadside.At + 0.2);" },
		{ "Battle/SeaBattle.cpp", "Add(Struck, TEXT(\"a-hit\"), Struck.At + 0.5);" },
		{ "Battle/SeaBattle.cpp", "Add(Run, TEXT(\"torpedo-tracks\"), Run.At + 20.0);" },
		{ "Battle/SeaBattle.cpp", "Add(Strike, TEXT(\"the-strike\"), Strike.At - 2.5);" },
		{ "Battle/SeaBattle.cpp", "Add(Down, TEXT(\"going-down\"), Down.At + 20.0);" },
		{ "Battle/SeaBattle.cpp", "Add(Funnels, TEXT(\"funnels\"), Funnels.At + 2.0);" },
		{ "Battle/SeaBattle.cpp", "Add(Whole, TEXT(\"the-battle\"), Contact >= 0.0 ? Contact + (S.EndT - Contact) * 0.45 : (S.BeginT + S.EndT) * 0.5);" },
		{ "Battle/SeaBattle.cpp", "Add(Burning, TEXT(\"the-end\"), S.EndT - 8.0);" },
		{ "Battle/SeaBattle.cpp", "Add(Whole, TEXT(\"the-end\"), S.EndT - 8.0);" },
		{ "Battle/SeaBattle.cpp", "if (Funnels.At < 0.0 && T >= S.BeginT + 20.0)" },
		{ "Battle/SeaBattle.cpp", "if (Big.Doing != SeaShow::Fate::Line || Big.Speed < 10.0 || Rank(Big.Class) < 2)" },
		{ "Battle/SeaBattle.cpp", "auto Rank = [](const std::string& Class) { return Class == \"bb\" ? 4 : Class == \"cv\" ? 3 : Class == \"ca\" ? 2 : 1; };" },
		{ "Battle/SeaBattle.cpp", "const bool bDuel = T - LastSalvoAt[0] <= 30.0 && T - LastSalvoAt[1] <= 30.0;" },
		// the month's scene: its pace, and the windows handed to the director only there
		{ "Battle/SeaBattle.cpp", "Speed = FMath::Clamp((Window.To - Window.From) / Window.Screen, 0.25, FastestWindow);" },
		{ "Battle/SeaBattle.cpp", "Speed = FMath::Clamp((S.EndT - S.BeginT) / 40.0, 1.0, FastestWindow);" },
		{ "Battle/SeaBattle.cpp", "constexpr double CaptionHold = 6.0, CaptionFade = 0.8, FastestWindow = 6.0, LongestScene = 90.0;" },
		// the show's fixed step (the sea show's ChargesDown limit allows one) and its charge's burst under a boat
		{ "Battle/SeaShow.h", "static constexpr double Fixed = 0.05;" },
		{ "Battle/SeaShow.cpp", "C.At.X = Boat->P.X + C.Off.X;" },
		{ "Battle/SeaShow.cpp", "if (Boat->Keel > 0.0) C.At.Z = -Boat->Keel;" },
		// the month scene's screen: U before the skip, a modifier key alone never skipping
		{ "Battle/SeaBattleScreen.cpp", "if (Event.GetKey().IsModifierKey())" },
		{ "Battle/SeaBattleScreen.cpp", "Battle->UnderKey(Event.IsShiftDown());" },
		{ "Battle/SeaBattleScreen.cpp", "if (!Event.IsRepeat())" },
		// the player's own dive (FSeaUnder::PressU, Tick, EyeFor) and a boat's end (FSeaUnder::Effect), as played below
		{ "Battle/SeaUnder.cpp", "const int32 Chosen = SeaUnder::HandBoatFor(BoatsNow, Held, BoardOf(EyeAt), Now, ClearBy);" },
		{ "Battle/SeaUnder.cpp", "HandSide = SeaUnder::HandOf(Chosen);" },
		{ "Battle/SeaUnder.cpp", "if (!SeaUnder::HandHolds(BoatsNow, Held, HandBoat, Now, ClearBy))" },
		// (the clearance from an unfound boat by the water's sight in use, for the player's dive and PlanShots' director alike)
		{ "Battle/SeaUnder.cpp", "ClearBy = SeaUnder::UnheldClearBy(DaySee, NightSee);" },
		{ "Battle/SeaUnder.cpp", "Pose = SeaUnder::EyeByHand(SeaUnder::PoseOf(*Boat), HullsOf(Show), Now - HandSince, HandSide);" },
		{ "Battle/SeaUnder.cpp", "Sea.Ended(Now, E.Key);" },
	};

	// -------------------------------------------------------------------------------------------------------------
	// The stage's sun (ASeaBattle::OpenOn: the hour from the seed, SeaParts::SunAt, the height kept 12 to 70 degrees)

	void StageSun(const SeaFeed::Recording& Rec, double& OutFrom, double& OutUp)
	{
		const uint64_t Seed = Rec.Seed ? Rec.Seed : 239;
		const double Hour = 8.0 + static_cast<double>((Seed * 2654435761ull) % 7000ull) / 1000.0;
		const double Day = 30.4 * std::clamp(Rec.Board.Month, 0, 11) + 15.0;
		const double Decl = 23.44 * Pi / 180.0 * std::sin(2.0 * Pi * (284.0 + Day) / 365.0);
		const double Phi = std::clamp(Rec.Board.Lat, -80.0, 80.0) * Pi / 180.0;
		const double H = 15.0 * (Hour - 12.0) * Pi / 180.0;
		const double SinUp = std::sin(Phi) * std::sin(Decl) + std::cos(Phi) * std::cos(Decl) * std::cos(H);
		OutUp = std::clamp(std::asin(std::clamp(SinUp, -1.0, 1.0)) * 180.0 / Pi, 12.0, 70.0);
		const double Bearing = std::atan2(-std::sin(H) * std::cos(Decl), std::sin(Decl) * std::cos(Phi) - std::cos(Decl) * std::sin(Phi) * std::cos(H));
		OutFrom = std::fmod(Bearing * 180.0 / Pi + 360.0, 360.0);
	}

	// -------------------------------------------------------------------------------------------------------------
	// What the scene puts in the water (FSeaUnder::Effect, mirrored)

	/** An ahead salvo's bombs, as FSeaUnder::Effect puts them in: ConvoyLooks' plan and seed, so they fall where its splashes are. */
	void AheadInto(Water& Sea, const SeaShow::Show& Live, const SeaShow::Fx& E, double T)
	{
		ConvoyLook::DropIn In;
		In.Kind = E.What;
		In.N = static_cast<int>(std::lround(E.Size));
		In.Spread = E.Spread;
		In.Sinking = E.Sinking;
		In.Until = E.Until;
		In.At = P3(E.At.X, E.At.Y, E.At.Z);
		In.Aim = P3(E.Aim.X, E.Aim.Y, 0.0);
		In.Vel = P3(E.Vel.X, E.Vel.Y, 0.0);
		if (const SeaShow::ShipOut* Escort = Live.Ship(E.Key)) { In.Beam = Escort->Beam; In.Long = Escort->Long; }
		double Deepest = 120.0;
		for (const SeaShow::ShipOut& Ship : Live.Ships())
			if (Ship.Class == "ss") Deepest = std::max(Deepest, Ship.Keel + 30.0);
		Sea.AheadDropped(T, In, static_cast<uint64_t>(static_cast<uint32_t>(E.Key)) * 7919ull + static_cast<uint64_t>(std::max(0.0, T) * 10.0), Deepest);
	}

	/**
	 * How each pattern charge's burst lies against the charges drawn the frame before (Water::Charges): an ordinary one where
	 * one of its escort's was drawn (1.5 m); the booked blow (sea:depthChargeHit, the burst's Size 1) where one of its
	 * escort's was drawn within twice its lethal reach of her keel, within that reach of one (SeaShow's rule: burst where it
	 * is, or moved that reach toward her); only where none lay so near, the pattern laid off her, is it put by her.
	 */
	struct BurstTally
	{
		int Met = 0, Off = 0, BookedNear = 0, BookedOff = 0, LaidOff = 0;
		double WorstOff = 0.0, WorstBooked = 0.0, FarthestLaid = 0.0;

		void Take(const SeaShow::Fx& E, const std::vector<Drum>& Before, const std::map<int, BoatNow>& Boats, double Lethal)
		{
			const P3 Blown(E.At.X, E.At.Y, E.At.Z);
			double Best = 1e18;
			for (const Drum& D : Before)
				if (D.Escort == E.Key) Best = std::min(Best, Gap(D.At, Blown));
			if (E.Size > 0.5 && Best > 1.5)
			{
				const BoatNow* Her = nullptr;
				double HerFar = 1e18;
				for (const auto& Each : Boats)
					if (Gap(Each.second.At, Blown) < HerFar) { HerFar = Gap(Each.second.At, Blown); Her = &Each.second; }
				double DrumFar = 1e18;
				for (const Drum& D : Before)
					if (Her && D.Escort == E.Key) DrumFar = std::min(DrumFar, Gap(D.At, Her->At));
				if (DrumFar <= 2.0 * Lethal + 0.5)
				{
					++BookedNear;
					WorstBooked = std::max(WorstBooked, Best);
					BookedOff += Best > Lethal + 0.5 ? 1 : 0;
				}
				else
				{
					++LaidOff;
					FarthestLaid = std::max(FarthestLaid, DrumFar);
				}
			}
			else { ++Met; WorstOff = std::max(WorstOff, Best); Off += Best > 1.5 ? 1 : 0; }
		}
	};

	// -------------------------------------------------------------------------------------------------------------
	// A run over a boat: a recording of the test's own (no battle's), read through the game's own reader

	/**
	 * One escort coming up astern of a boat lying 60 m down and passing her, both steaming north (he at 8 m/s, she at 3: the
	 * show keeps their hulls apart, so he passes her some 200 m off), his asdic on her from 10 s, lost at 30 s. Her hunters'
	 * list (extra.heard) in every other view only, the reader keeping it between: none to 11 s, him and a surface ship of her
	 * own side far off from 12 s to 50 s, then that ship alone. DropT: he drops a pattern of three, Spread across, set to her
	 * depth (below 0: none); HitT: a sea:depthChargeHit on her told then (below 0: none).
	 */
	std::string RunOver(double DropT, double Spread, double HitT)
	{
		struct Told { double T; std::string Json; };
		std::vector<Told> Events = {
			{ 10.0, "{\"t\":10,\"what\":\"sea:sonar\",\"unit\":20,\"side\":\"def\",\"text\":\"\",\"extra\":{\"on\":30,\"kind\":\"asdic\",\"range\":420}}" },
			{ 30.0, "{\"t\":30,\"what\":\"sea:sonar\",\"unit\":20,\"side\":\"def\",\"text\":\"\",\"extra\":{\"on\":30,\"kind\":\"asdic\",\"lost\":\"close\"}}" },
		};
		if (DropT >= 0.0)
			Events.push_back({ DropT, "{\"t\":" + Num(DropT, 4) + ",\"what\":\"sea:depthCharge\",\"unit\":20,\"side\":\"def\",\"text\":\"\",\"extra\":{\"on\":30,\"n\":3,\"set\":60,"
			                          "\"sinking\":21,\"spread\":" + Num(Spread, 4) + ",\"kind\":\"pattern\"}}" });
		if (HitT >= 0.0)
			Events.push_back({ HitT, "{\"t\":" + Num(HitT, 4) + ",\"what\":\"sea:depthChargeHit\",\"unit\":30,\"side\":\"def\",\"text\":\"\",\"extra\":{\"from\":20,\"harm\":0.5}}" });
		auto Unit = [](int Id, const char* Side, const char* Class, double X, double Y, double Speed, const std::string& More)
		{
			return "{\"id\":" + std::to_string(Id) + ",\"side\":\"" + Side + "\",\"kind\":\"" + Class + "\",\"name\":\"" + Class + " " + std::to_string(Id) +
			       "\",\"nat\":\"" + (std::string(Side) == "def" ? "UK" : "GER") + "\",\"x\":" + Num(X, 2) + ",\"y\":" + Num(Y, 2) +
			       ",\"face\":0,\"str\":1,\"mor\":1,\"state\":\"steady\",\"figures\":1,\"figures0\":1,\"d\":0,\"act\":\"march\",\"speed\":" + Num(Speed, 2) +
			       ",\"engaged\":-1,\"extra\":{\"cls\":\"" + Class + "\",\"ships0\":1,\"ships\":1,\"sunk\":0,\"fleet\":\"" + Side + "\"" + More + "}}";
		};
		std::string Views;
		size_t Next = 0;
		std::sort(Events.begin(), Events.end(), [](const Told& L, const Told& R) { return L.T < R.T; });
		for (int K = 0; K <= 120; ++K)
		{
			const double T = K;
			std::string Heard;
			if (K % 2 == 0) Heard = std::string(",\"heard\":") + (K < 12 ? "[]" : K <= 50 ? "[20,40]" : "[40]");
			std::string Said;
			for (; Next < Events.size() && Events[Next].T <= T + 1e-9; ++Next) Said += (Said.empty() ? "" : ",") + Events[Next].Json;
			Views += std::string(K ? "," : "") + "{\"t\":" + std::to_string(K) + ",\"limit\":120,\"speed\":1,\"me\":\"def\",\"contact\":0,\"units\":[" +
			         Unit(20, "def", "dd", 10000.0, 8700.0 + 8.0 * T, 8.0, "") + "," +
			         Unit(30, "att", "ss", 10000.0, 9000.0 + 3.0 * T, 3.0, ",\"dive\":\"deep\",\"keel\":60,\"scope\":false" + Heard) + "," +
			         Unit(40, "att", "dd", 15000.0, 15000.0, 0.0, "") + "],\"events\":[" + Said + "]}";
		}
		return "{\"what\":\"the view under the water's own run over a boat (the test's, not a battle's)\",\"recording\":1,\"key\":\"run-over\",\"seed\":241,\"step\":1,"
		       "\"field\":{\"size\":20000,\"edge\":5000,\"zoneName\":\"Western Approaches\",\"title\":\"A run over a boat (the test's own)\",\"when\":{\"year\":1942,\"month\":11},"
		       "\"lat\":55,\"lon\":-20,\"att\":{\"nat\":\"GER\",\"adj\":\"German\"},\"def\":{\"nat\":\"UK\",\"adj\":\"British\"},"
		       "\"classes\":{\"dd\":{\"len\":95,\"beam\":10,\"speed\":12,\"turn\":0.12,\"reach\":4000},\"ss\":{\"len\":67,\"beam\":6.2,\"speed\":7,\"turn\":0.1,\"reach\":3000}},"
		       "\"light\":{\"sun\":-24,\"moon\":{\"lit\":0.45,\"up\":22,\"from\":140},\"vis\":2500},\"subs\":{\"scope\":13,\"deep\":80,\"crush\":250,\"dived\":3.9},"
		       "\"dc\":{\"sink\":3.0,\"lethal\":6.0}},\"views\":[" + Views + "],\"after\":{\"lines\":[],\"level\":\"\",\"winner\":\"def\",\"seconds\":120}}";
	}

	/**
	 * Where to drop so his starboard thrower's charge goes down where she will be, from the run with no drop (a drop steers no
	 * one, so the ships go as they went): of a pattern of three SeaShow lays the middle one half the spread ahead of his stern
	 * and the spread's half less his half beam to starboard, bursting half his run and its sinking (21 s) after the drop. The
	 * moment (to a tenth of a second) and the spread that put it nearest her; false where none comes
	 * within 2 m. The moment the test then tells the blow at is the show's own (Replay), so this only aims. (The show keeps
	 * his hull some 200 m off hers, SeaShow::KeepClear, so only a wide pattern's thrown charge comes down where she will be:
	 * up to 600 m across, the mechanism tried, not the drill.)
	 */
	bool AimOver(const SeaFeed::Recording& Rec, double& OutDrop, double& OutSpread, double& OutMiss)
	{
		struct Seen { P3 At; double Heading = 0.0, Speed = 0.0, Long = 95.0, Beam = 10.0; P3 Her; };
		std::vector<Seen> Each;
		SeaShow::Show Live(Rec.Seed ? Rec.Seed : 239);
		Live.SetField(Rec.Board);
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& V = Rec.Views[Taken++];
			Live.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
		};
		TakeNext();
		const double EndT = Rec.Views.back().Head.T;
		for (int Fr = 0; Fr <= static_cast<int>(std::floor(EndT * 30.0 + 1e-6)); ++Fr)
		{
			const double T = Fr / 30.0;
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
			Live.Advance(T);
			Live.Effects.clear();
			Seen Now;
			for (const SeaShow::ShipOut& Ship : Live.Ships())
			{
				if (Ship.Piece == 20) { Now.At = P3(Ship.At.X, Ship.At.Y, 0.0); Now.Heading = Ship.Heading; Now.Speed = Ship.Speed; Now.Long = Ship.Long; Now.Beam = Ship.Beam; }
				if (Ship.Piece == 30) Now.Her = P3(Ship.At.X, Ship.At.Y, 0.0);
			}
			Each.push_back(Now);
		}
		OutMiss = 1e18;
		for (size_t Fr = 20 * 30; Fr + 1 < Each.size(); Fr += 3)
		{
			const Seen& At = Each[Fr];
			const P3 Ahead = ConvoyLook::AheadOf(At.Heading), Right = ConvoyLook::RightOf(At.Heading);
			const P3 Stern = At.At - Ahead * (0.5 * At.Long);
			double Spread = 120.0;
			P3 Rel;
			bool bIn = true;
			for (int Again = 0; Again < 4 && bIn; ++Again)
			{
				const double Bursts = Fr / 30.0 + 0.5 * Spread / std::max(3.0, At.Speed) + 21.0;
				const size_t Then = static_cast<size_t>(std::llround(Bursts * 30.0));
				bIn = Then < Each.size();
				if (!bIn) break;
				Rel = Each[Then].Her - Stern;
				Spread = 2.0 * (Rel.X * Right.X + Rel.Y * Right.Y) + At.Beam;
			}
			if (!bIn || Spread < 40.0 || Spread > 600.0) continue;
			const double Miss = std::hypot(Rel.X * Ahead.X + Rel.Y * Ahead.Y - 0.5 * Spread, Rel.X * Right.X + Rel.Y * Right.Y - (0.5 * Spread - 0.5 * At.Beam));
			if (Miss < OutMiss) { OutMiss = Miss; OutDrop = Fr / 30.0; OutSpread = Spread; }
		}
		return OutMiss <= 2.0;
	}

	/** A recording played as the scene plays it (30 frames a second), each charge drawn as the view under the water draws
	 * it: its pattern bursts tallied; each booked blow against its own charge (drawn the frame before, gone in its frame); and
	 * the first moment a pattern's charge not yet due, the nearest of its escort's to the boat it went in on as the show
	 * reckons it (SeaShow's sea:depthChargeHit: going down at the charge's rate to its burst), lies past a metre more than its
	 * lethal reach of her keel and within twice that reach less half a metre (where a blow told then moves it that reach). */
	struct Replayed
	{
		BurstTally Tally;
		double NearT = -1.0, NearFar = 0.0;
		int Booked = 0;
		double FromOwn = -1.0, OwnFar = -1.0; // the booked blow from its own charge as drawn the frame before; that charge from her keel
	};

	Replayed Replay(const SeaFeed::Recording& Rec)
	{
		Replayed Out;
		SeaShow::Show Live(Rec.Seed ? Rec.Seed : 239);
		Live.SetField(Rec.Board);
		const SeaShow::Field& Board = Live.Board();
		const double Lethal = Board.ChargeLethal, Sink = std::max(0.5, Board.ChargeSink);
		Water Sea;
		Sea.Begin(Board);
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& V = Rec.Views[Taken++];
			Live.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
		};
		TakeNext();
		std::vector<SeaShow::Show::ChargeDown> Down;
		std::vector<Drum> Drums, DrumsBefore;
		const double BeginT = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
		const int Frames = static_cast<int>(std::floor((EndT + 1e-6 - BeginT) * 30.0)) + 1;
		for (int Fr = 0; Fr < Frames; ++Fr)
		{
			const double T = BeginT + Fr / 30.0;
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
			Live.Advance(T);
			std::map<int, BoatNow> Boats;
			for (const SeaShow::ShipOut& Ship : Live.Ships())
				if (Ship.Class == "ss" && Ship.Doing != SeaShow::Fate::Gone && !Boats.count(Ship.Piece))
					Boats[Ship.Piece] = BoatNow{ P3(Ship.At.X, Ship.At.Y, -Ship.Keel), ConvoyLook::AheadOf(Ship.Heading) * Ship.Speed };
			Live.ChargesDown(Down);
			Sea.Remember(T, Down, Boats);
			Sea.Charges(T, Drums);
			for (const SeaShow::Fx& E : Live.Effects)
			{
				if (E.Kind != SeaShow::FxKind::ChargeBurst || E.What != "pattern") continue;
				Out.Tally.Take(E, DrumsBefore, Boats, Lethal);
				if (E.Size < 0.5) continue;
				const P3 Blown(E.At.X, E.At.Y, E.At.Z);
				const Drum* Own = nullptr;
				for (const Drum& B : DrumsBefore)
				{
					bool bStill = false;
					for (const Drum& D : Drums) bStill = bStill || D.Key == B.Key;
					if (B.Escort == E.Key && !bStill && (!Own || Gap(B.At, Blown) < Gap(Own->At, Blown))) Own = &B;
				}
				++Out.Booked;
				const BoatNow* Her = nullptr;
				for (const auto& Each : Boats)
					if (!Her || Gap(Each.second.At, Blown) < Gap(Her->At, Blown)) Her = &Each.second;
				if (Own && Her)
				{
					Out.FromOwn = std::max(Out.FromOwn, Gap(Own->At, Blown));
					Out.OwnFar = Gap(Own->At, Her->At);
				}
			}
			Live.Effects.clear();
			if (Out.NearT < 0.0)
			{
				auto NowOf = [T, Sink](const SeaShow::Show::ChargeDown& C) { return P3(C.At.X, C.At.Y, std::min(0.0, C.At.Z + std::max(0.0, C.Bursts - T) * Sink)); };
				for (const SeaShow::Show::ChargeDown& C : Down)
				{
					const auto Her = Boats.find(C.Boat);
					if (C.Kind != "pattern" || C.bOnBoat || C.Bursts < T + 0.5 || Her == Boats.end()) continue;
					const double Far = Gap(NowOf(C), Her->second.At);
					bool bNearest = true;
					for (const SeaShow::Show::ChargeDown& Other : Down)
						bNearest = bNearest && !(&Other != &C && Other.Escort == C.Escort && std::fabs(Other.Bursts - T) <= 15.0 && Gap(NowOf(Other), Her->second.At) < Far);
					if (bNearest && Far > Lethal + 1.0 && Far <= 2.0 * Lethal - 0.5)
					{
						Out.NearT = T;
						Out.NearFar = Far;
						break;
					}
				}
			}
			DrumsBefore = Drums;
		}
		return Out;
	}

	// -------------------------------------------------------------------------------------------------------------
	// The director, fed as PlanShots feeds it, and PlanShots' own shots

	struct Planned
	{
		std::vector<Window> Windows;
		Sight Held;
		std::vector<double> Busy;               // the other shots' cut moments (the filler left out)
		std::vector<std::pair<std::string, double>> Shots;
		bool bWhole = true;                     // no shot this mirror leaves out (a straddle, both lines)
	};

	int RankOf(const std::string& Class) { return Class == "bb" ? 4 : Class == "cv" ? 3 : Class == "ca" ? 2 : 1; }

	/** Busy: the cuts to use (empty: those the shots below give); Staged: the month's scene (its beats, or the whole at its pace). */
	Planned Plan(const SeaFeed::Recording& Rec, double Step, const std::vector<double>* Busy, bool bStaged)
	{
		Planned Out;
		SeaShow::Show Trial(Rec.Seed ? Rec.Seed : 239);
		Trial.SetField(Rec.Board);
		const double Mid = Rec.Board.Size * 0.5;
		Trial.Eye = SeaShow::Vec(Mid, Mid - 3000.0, 900.0);
		Director Dir;
		Dir.Begin(Rec.Board, SeaFeed::SideOf(Rec.Views.front().Head.Me) == 1 ? 1 : 0, UnheldClearBy());
		size_t Given = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& F = Rec.Views[Given++];
			Trial.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
			Dir.Told(F.Head.T, F.Pieces, F.Events);
		};
		TakeNext();
		const double BeginT = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
		double Contact = -1.0, LineT = -1.0, BroadT = -1.0, StruckT = -1.0, RunT = -1.0, StrikeT = -1.0, DownT = -1.0, FunnelsT = -1.0;
		bool bBigGuns = false;
		int LineSide = -1;
		double LastSalvoAt[2] = { -1e9, -1e9 };
		const int Steps = static_cast<int>(std::floor((EndT + 1e-6 - BeginT) / Step)) + 1;
		for (int K = 0; K < Steps; ++K)
		{
			const double T = BeginT + K * Step;
			while (Given < Rec.Views.size() && Rec.Views[Given - 1].Head.T <= T + 1e-9) TakeNext();
			Trial.Advance(T);
			if (Contact < 0.0 && Trial.ContactAt() >= 0.0) Contact = Trial.ContactAt();
			if (FunnelsT < 0.0 && T >= BeginT + 20.0)
				for (const SeaShow::ShipOut& Big : Trial.Ships())
				{
					if (FunnelsT >= 0.0) break;
					if (Big.Doing != SeaShow::Fate::Line || Big.Speed < 10.0 || RankOf(Big.Class) < 2) continue;
					for (const SeaShow::ShipOut& Small : Trial.Ships())
						if (Small.Doing == SeaShow::Fate::Line && Small.Speed >= 10.0 && Small.Class == "dd" && Small.Side == Big.Side &&
						    std::hypot(Small.At.X - Big.At.X, Small.At.Y - Big.At.Y) <= 1000.0) { FunnelsT = T; break; }
				}
			for (const SeaShow::Fx& E : Trial.Effects)
			{
				switch (E.Kind)
				{
				case SeaShow::FxKind::Salvo:
					if (E.Side == 0 || E.Side == 1) LastSalvoAt[E.Side] = T;
					if (E.Size >= 180.0) Out.bWhole = false; // (a straddle's fall of shot is not mirrored: no convoy has one)
					if (BroadT < 0.0 || (!bBigGuns && E.What == "bb")) { BroadT = T; bBigGuns = E.What == "bb"; }
					break;
				case SeaShow::FxKind::Line:
					if (LineT < 0.0 || (LineSide != 0 && E.Side == 0)) { LineT = T; LineSide = E.Side; }
					break;
				case SeaShow::FxKind::ShellHit: if (StruckT < 0.0) StruckT = T; break;
				case SeaShow::FxKind::TrackStart: if (RunT < 0.0) RunT = T; break;
				case SeaShow::FxKind::BombHit:
				case SeaShow::FxKind::NearMiss: if (StrikeT < 0.0) StrikeT = T; break;
				case SeaShow::FxKind::Sink: if (DownT < 0.0) DownT = T; break;
				default: break;
				}
			}
			Dir.Step(T, Trial);
			Trial.Effects.clear();
			if (T - LastSalvoAt[0] <= 30.0 && T - LastSalvoAt[1] <= 30.0) Out.bWhole = false; // (both lines in a gun duel: not mirrored)
			double X = 0.0, Y = 0.0;
			int Count = 0;
			for (const auto& Pair : Trial.Pieces()) { X += Pair.second.X; Y += Pair.second.Y; ++Count; }
			if (Count > 0) Trial.Eye = SeaShow::Vec(X / Count, Y / Count - 3000.0, 900.0);
		}
		auto Add = [&Out](const char* Name, double At) { Out.Shots.push_back({ Name, At }); };
		Add("the-fleets", Contact >= 0.0 ? std::max(BeginT + 12.0, Contact - 40.0) : BeginT + 30.0);
		if (LineT >= 0.0)
		{
			double LineAt = LineT + 20.0;
			if (BroadT >= 0.0 && std::fabs(LineAt - BroadT) < 8.0 && BroadT - 8.0 >= LineT + 10.0) LineAt = BroadT - 8.0;
			Add("the-line", LineAt);
		}
		if (BroadT >= 0.0) Add("broadside", BroadT + 0.2);
		if (StruckT >= 0.0) Add("a-hit", StruckT + 0.5);
		if (RunT >= 0.0) Add("torpedo-tracks", RunT + 20.0);
		if (StrikeT >= 0.0) Add("the-strike", StrikeT - 2.5);
		if (DownT >= 0.0) Add("going-down", DownT + 20.0);
		if (FunnelsT >= 0.0) Add("funnels", FunnelsT + 2.0);
		Add("the-battle", Contact >= 0.0 ? Contact + (EndT - Contact) * 0.45 : (BeginT + EndT) * 0.5);
		Add("the-end", EndT - 8.0);
		for (const auto& Shot : Out.Shots)
			if (Shot.first != "the-battle") Out.Busy.push_back(Shot.second - 4.0);
		// (the month's scene: its beats; with none, the whole at its one pace, as RunMonthScene plays it)
		std::vector<Director::Beat> Beats;
		if (bStaged)
		{
			for (const SeaFeed::Beat& B : Rec.Beats) Beats.push_back({ B.From, B.To, B.Screen });
			if (Beats.empty()) Beats.push_back({ BeginT, EndT, (EndT - BeginT) / std::clamp((EndT - BeginT) / 40.0, 1.0, 6.0) });
		}
		Out.Windows = Dir.Finish(Busy ? *Busy : Out.Busy, Beats, BeginT, EndT);
		Out.Held = Dir.Sighted();
		return Out;
	}

	std::string Listed(const std::vector<Window>& Windows)
	{
		std::string Text;
		for (const Window& W : Windows)
			Text += (Text.empty() ? "" : "; ") + W.Name + " boat " + std::to_string(W.BoatPiece) + " " + Num(W.From) + "-" + Num(W.Until) + (W.Pace != 1.0 ? " at " + Num(W.Pace, 2) + "x" : "");
		return Text.empty() ? "none" : Text;
	}

	/** The director's own rules over one plan. */
	void Rules(const std::string& How, const SeaFeed::Recording& Rec, const Planned& First, const Planned& Again, const Planned& Fine, bool bStaged)
	{
		const double BeginT = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
		const size_t N = First.Windows.size();
		bool bSame = Again.Windows.size() == N, bNear = Fine.Windows.size() == N;
		int Held = 0, Short = 0, Spaced = 0, Screened = 0, Clear = 0;
		for (size_t K = 0; K < N; ++K)
		{
			const Window& W = First.Windows[K];
			bSame = bSame && K < Again.Windows.size() && W.From == Again.Windows[K].From && W.Until == Again.Windows[K].Until && W.Boat == Again.Windows[K].Boat;
			bNear = bNear && K < Fine.Windows.size() && std::fabs(W.From - Fine.Windows[K].From) <= 0.3 && std::fabs(W.Until - Fine.Windows[K].Until) <= 0.3 &&
			        W.Boat == Fine.Windows[K].Boat;
			Held += First.Held.Covers(W.BoatPiece, W.Side, W.From, W.Until) ? 1 : 0;
			Short += W.Until > W.From && W.Until - W.From <= (W.End >= 0.0 ? Director::LongestWithEnd : Director::Longest) + 1e-9 ? 1 : 0;
			Spaced += K == 0 || W.From >= First.Windows[K - 1].Until + Director::Apart - 1e-9 ? 1 : 0;
			Screened += (W.Until - W.From) / W.Pace >= Director::ScreenLeast - 1e-9 ? 1 : 0;
			bool bClear = true;
			for (const double Cut : First.Busy) bClear = bClear && !(Cut > W.From - Director::ClearCut + 1e-6 && Cut < W.Until - 1e-6);
			Clear += bClear ? 1 : 0;
		}
		const std::string Of = " of " + std::to_string(N);
		Check(How + ": the same recording, the same windows", bSame, Listed(First.Windows));
		Check(How + ": the windows hold at a tenth of a second a step", bNear, std::to_string(Fine.Windows.size()) + " windows");
		Check(How + ": each window's boat the player's own or held all through", Held == static_cast<int>(N), std::to_string(Held) + Of);
		Check(How + ": each window 45 s at the most (60 through her end)", Short == static_cast<int>(N), std::to_string(Short) + Of);
		Check(How + ": the windows 20 s apart at the least", Spaced == static_cast<int>(N), std::to_string(Spaced) + Of);
		Check(How + ": one window and one more every 150 s at the most", static_cast<double>(N) <= 1.0 + (EndT - BeginT) / Director::EveryMore, std::to_string(N));
		Check(How + ": every window clear of the scene's other shots' cuts (3 s after one, or up in it)", Clear == static_cast<int>(N), std::to_string(Clear) + Of);
		if (bStaged)
		{
			int Inside = 0;
			for (const Window& W : First.Windows)
			{
				bool bIn = Rec.Beats.empty();
				for (const SeaFeed::Beat& B : Rec.Beats) bIn = bIn || (W.From >= B.From - 1e-9 && W.HoldUntil <= B.To + 1e-9);
				Inside += bIn ? 1 : 0;
			}
			Check(How + ": each window and its hold inside one beat, 3 s of screen at the least", Inside == static_cast<int>(N) && Screened == static_cast<int>(N),
			      std::to_string(Inside) + Of + " inside, " + std::to_string(Screened) + Of + " long enough");
		}
	}
}

int main(int ArgC, char** ArgV)
{
	if (ArgC < 2)
	{
		std::printf("usage: sea_under_test <Source/TheaterCommand> recording.json ...\n");
		return 2;
	}
	const std::string Source = ArgV[1];
	std::printf("the view under the water (U1)\n");
	// THE GAME'S OWN LINES THIS MIRRORS
	{
		int Found = 0;
		std::string Missing;
		std::map<std::string, std::string> Texts;
		for (const Pin& P : Pins)
		{
			if (!Texts.count(P.File)) Texts[P.File] = ReadAll(Source + "/" + P.File);
			if (Texts[P.File].find(P.Line) != std::string::npos) ++Found;
			else Missing += std::string(Missing.empty() ? "" : " | ") + P.File + ": " + P.Line;
		}
		const int All = static_cast<int>(sizeof(Pins) / sizeof(Pins[0]));
		Check("the lines mirrored here still read so in the game's source", Found == All, std::to_string(Found) + " of " + std::to_string(All) + (Missing.empty() ? "" : "; missing " + Missing));
		// (the director fed before the trial's effects are cleared, in the same loop; U in the month scene before its skip)
		const std::string& Battle = Texts["Battle/SeaBattle.cpp"];
		const size_t Fed = Battle.find("UnderPlan.Step(T, Trial);"), Cleared = Battle.find("Trial.Effects.clear();", Fed == std::string::npos ? 0 : Fed);
		const size_t Lines = Battle.find("BOTH LINES IN ONE PICTURE"), Windows = Battle.find("UnderPlan.Finish(");
		Check("PlanShots feeds the director each step's effects before they are cleared, and plans the windows after both lines",
		      Fed != std::string::npos && Cleared != std::string::npos && Cleared - Fed < 200 && Lines != std::string::npos && Windows != std::string::npos && Windows > Lines,
		      Fed != std::string::npos ? "fed" : "not fed");
		const std::string& Screen = Texts["Battle/SeaBattleScreen.cpp"];
		const size_t Modifier = Screen.find("IsModifierKey()"), Key = Screen.find("Battle->UnderKey("), Skip = Screen.find("Battle->SkipMonthScene();");
		Check("the month scene's screen: a modifier alone and U never skip it", Modifier != std::string::npos && Key != std::string::npos && Skip != std::string::npos && Modifier < Skip && Key < Skip,
		      Key != std::string::npos ? "U handed over" : "no U");
	}
	// THE LOOK'S OWN NUMBERS (no recording)
	{
		bool bMurk = true;
		for (double Depth = 0.0; Depth <= 300.0; Depth += 10.0)
			for (double Day = 0.0; Day <= 1.0; Day += 0.25)
			{
				const Murk M = MurkAt(Depth, Day, Day > 0.5 ? 40.0 : -20.0);
				bMurk = bMurk && M.See >= SeeNight && M.See <= SeeDay && M.G >= 0.0 && M.G <= 0.1 && M.SunShare >= 0.08 && M.SunShare <= 0.6 && FogDensity(M.See, 100.0) > 0.0;
			}
		Check("the murk within its bounds at every depth and light", bMurk, "0-300 m");
		const double R = GlobeAt(132.0, 50.0, 0.177);
		Check("a Mk VII's globe at 50 m about 4.4 m at its widest (Cole)", std::fabs(R - 4.4) < 0.3 && GlobeAt(132.0, 50.0, 2.0) < 0.0, Num(R, 2) + " m");
		std::vector<CeilingPoint> Face;
		CeilingAt(P3(0.0, 0.0, -20.0), 0.0, 1.0, 40.0, 180.0, MurkAt(20.0, 1.0, 40.0), Face);
		const size_t Outside = 1 + static_cast<size_t>(CeilingRings - 1) * CeilingSpokes; // (the skirt's first point)
		Check("the sea's face from below: its points, Snell's window brighter than outside it", Face.size() == 1 + static_cast<size_t>(CeilingRings) * CeilingSpokes && Face[0].G > Face[Outside].G,
		      std::to_string(Face.size()) + " points");
		double Worst = 1e9;
		for (double Depth = 1.5; Depth <= 300.0; Depth += 0.5) Worst = std::min(Worst, RippleWave(Depth) / CeilingCell(Depth));
		Check("the face's ripples 2.5 of its cells long at the least at every depth (no crawling)", Worst >= 2.5, Num(Worst, 2) + " cells at the shortest");
		Water Sea;
		Sea.Begin(SeaShow::Field());
		for (int K = 0; K < 6; ++K) Sea.Burst(10.0 + 3.0 * K, P3(20.0 * K, 0.0, -60.0), "pattern");
		std::vector<Creak> One, Two;
		double LastOne = -1e9, LastTwo = -1e9;
		for (int K = 0; K < 1200; ++K)
		{
			const double T = K / 30.0;
			Sea.CreaksBetween(T, T + 1.0 / 30.0, 0.0, 30ull * 64ull, 30.0 + T, 30.0 + T + 1.0 / 30.0, P3(0.0, 0.0, -60.0), LastOne, One);
			Sea.CreaksBetween(T, T + 1.0 / 30.0, 0.0, 30ull * 64ull, 30.0 + T, 30.0 + T + 1.0 / 30.0, P3(0.0, 0.0, -60.0), LastTwo, Two);
		}
		bool bSpaced = One.size() == Two.size() && !One.empty();
		for (size_t K = 0; K < One.size() && bSpaced; ++K) bSpaced = One[K].T == Two[K].T && One[K].Which == Two[K].Which && (K == 0 || One[K].T - One[K - 1].T >= 0.8);
		Check("the hull's creaks the same each time and 0.8 s apart at the least", bSpaced, std::to_string(One.size()) + " creaks");
	}
	// THE ONE RULE ON WHAT MAY BE NEAR HER (no recording): an enemy boat the escorts do not hold within the clearance of her
	// (UnheldClearBy: 232.5 m in the water's own sight) keeps the eye up, for the director's windows and the player's own
	// dive alike (SeaUnder::ClearOfUnheld)
	{
		const double ClearBy = UnheldClearBy();
		Sight Escorts;
		Escorts.Begin(1); // (the player the escorts' side: the boats, side 0, held only once found)
		SeaShow::Event Found;
		Found.T = 100.0;
		Found.What = "sea:sonar";
		Found.Kind = "asdic";
		Found.Unit = 20;
		Found.On = 30;
		Escorts.Told({ Found });
		auto Pack = [](double Off) { return std::vector<BoatAt>{ BoatAt{ 7, 30, 0, 1000.0, 1000.0, true, true, true }, BoatAt{ 8, 31, 0, 1000.0 + Off, 1000.0, true, true, true } }; };
		const std::vector<BoatAt> Close = Pack(150.0), Apart = Pack(250.0);
		const P3 Over(1000.0, 700.0, 900.0);
		Check("an unheld boat 150 m from a held one keeps the eye up beside her (refused, and a dive on her ended)",
		      !ClearOfUnheld(Close, Escorts, 7, 120.0, ClearBy) && HandBoatFor(Close, Escorts, Over, 120.0, ClearBy) < 0 && !HandHolds(Close, Escorts, 7, 120.0, ClearBy),
		      "the boat chosen: " + std::to_string(HandBoatFor(Close, Escorts, Over, 120.0, ClearBy)));
		Check("at 250 m the eye goes under beside the held one, and stays", ClearOfUnheld(Apart, Escorts, 7, 120.0, ClearBy) && HandBoatFor(Apart, Escorts, Over, 120.0, ClearBy) == 7 &&
		      HandHolds(Apart, Escorts, 7, 120.0, ClearBy), "the boat chosen: " + std::to_string(HandBoatFor(Apart, Escorts, Over, 120.0, ClearBy)));
		std::vector<BoatAt> Lost = Close;
		Lost[1].bAfloat = Lost[1].bUnderWay = false;
		std::vector<BoatAt> Stopped = Apart;
		Stopped[0].bUnderWay = false;
		Check("before she is found none; a sunk boat near her keeps nothing up; one not under way is not chosen", HandBoatFor(Apart, Escorts, Over, 90.0, ClearBy) < 0 &&
		      HandBoatFor(Lost, Escorts, Over, 120.0, ClearBy) == 7 && HandBoatFor(Stopped, Escorts, Over, 120.0, ClearBy) < 0 &&
		      HandBoatFor(Apart, Escorts, P3(1000.0, 1000.0 - HandReach - 10.0, 900.0), 120.0, ClearBy) < 0,
		      "before " + std::to_string(HandBoatFor(Apart, Escorts, Over, 90.0, ClearBy)) + ", by a sunk one " + std::to_string(HandBoatFor(Lost, Escorts, Over, 120.0, ClearBy)));
		Sight Own;
		Own.Begin(0); // (the player the boats' side: his own always, a pack-mate of his near her his own too)
		Check("the player's own boats: always, side by side", HandBoatFor(Close, Own, Over, 50.0, ClearBy) == 7 && HandHolds(Close, Own, 8, 50.0, ClearBy),
		      std::to_string(HandBoatFor(Close, Own, Over, 50.0, ClearBy)));
		Check("his dive's side the same for the same boat each time", HandOf(7) == HandOf(7) && (HandOf(7) == 1 || HandOf(7) == -1), std::to_string(HandOf(7)));
		// (the clearance by the water's sight in use: a clearer water keeps the eye up for a boat farther off, and at every
		// sight the knob allows an unheld boat at the clearance leaves the eye under a hundredth of her light, her near end
		// half her length nearer and the eye 25 m off the boat it is beside)
		const double Clearest = UnheldClearBy(300.0, 300.0);
		bool bDark = ClearBy >= 200.0 && std::fabs(UnheldClearBy(SeeDay, SeeNight) - ClearBy) < 1e-9;
		std::string Worst;
		double WorstLeft = 0.0;
		for (const double Day : { 10.0, 40.0, 60.0, 85.0, 150.0, 220.0, 300.0 })
			for (const double Night : { 10.0, 60.0, 300.0 })
			{
				const double Left = LightLeft(UnheldClearBy(Day, Night) - 25.0 - 0.5 * 67.0, std::max(Day, Night));
				if (Left > WorstLeft) { WorstLeft = Left; Worst = Num(Day, 0) + "/" + Num(Night, 0); }
				bDark = bDark && Left < 0.01;
			}
		Check("the clearance from an unheld boat by the water's sight: under a hundredth of her light left there at every sight the knob allows",
		      bDark, Num(ClearBy, 1) + " m at the water's own sight, " + Num(Clearest, 1) + " m at 300 m; at worst " + Num(WorstLeft, 5) + " of her light (" + Worst + ")");
		Check("at a 300 m sight an unheld boat 250 m from the held one keeps the eye up (refused, and a dive on her ended)",
		      !ClearOfUnheld(Apart, Escorts, 7, 120.0, Clearest) && HandBoatFor(Apart, Escorts, Over, 120.0, Clearest) < 0 && !HandHolds(Apart, Escorts, 7, 120.0, Clearest),
		      "the boat chosen: " + std::to_string(HandBoatFor(Apart, Escorts, Over, 120.0, Clearest)));
	}
	// WHO HOLDS HER (no recording): found by sonar, sight or a blow, never by a pattern alone; a contact held while her hunter
	// runs in after he loses her, and not at all once she or he is sunk; her hunters' lists, where the views carry them,
	// in place of the sonar events
	{
		auto Said = [](double T, const char* What, int Unit, int On, const char* Kind, const char* Lost)
		{
			SeaShow::Event E;
			E.T = T;
			E.What = What;
			E.Unit = Unit;
			E.On = On;
			E.Kind = Kind;
			E.Lost = Lost;
			return E;
		};
		Sight Dropped;
		Dropped.Begin(1);
		SeaShow::Event Pattern = Said(100.0, "sea:depthCharge", 22, 30, "pattern", "");
		Pattern.N = 10;
		Dropped.Told({ Pattern });
		const bool bNothing = !Dropped.Covers(30, 0, 101.0, 101.0) && Dropped.FoundAt(30) < 0.0;
		SeaShow::Event Blow = Said(130.0, "sea:depthChargeHit", 30, -1, "", "");
		Blow.From = 22;
		Dropped.Told({ Blow });
		Check("a pattern dropped on a boat no escort holds finds nothing; a blow on her holds her 90 s",
		      bNothing && Dropped.Covers(30, 0, 130.0, 130.0 + Sight::AttackHolds) && !Dropped.Covers(30, 0, 129.0, 129.0) && !Dropped.Covers(30, 0, 221.0, 221.0),
		      std::string(bNothing ? "nothing from the pattern" : "held by the pattern") + ", found at " + Num(Dropped.FoundAt(30)));
		Sight Run;
		Run.Begin(1);
		Run.Told({ Said(100.0, "sea:sonar", 20, 30, "asdic", ""), Said(150.0, "sea:sonar", 20, 30, "asdic", "close"), Said(105.0, "sea:sonar", 21, 30, "bearing", "") });
		int By = -1, ByAfter = -1;
		const bool bPings = Run.Pinged(30, 149.0, By) && By == 20 && !Run.Pinged(30, 151.0, ByAfter);
		Check("a contact lost close holds her 90 s more (her hunter running in on her), no longer; the pings stop as it is lost; a bearing finds nothing",
		      Run.Covers(30, 0, 100.0, 150.0 + Sight::AttackHolds) && !Run.Covers(30, 0, 241.0, 241.0) && !Run.Covers(30, 0, 99.0, 99.0) && bPings,
		      std::string(bPings ? "pinged to 150 s" : "pinged wrongly") + ", held to " + Num(150.0 + Sight::AttackHolds));
		// (a contact closed only by her end, by her hunter's, or by a loss told "sunk": not held a second later, nor held again)
		Sight Hers, His, Ended;
		for (Sight* Each : { &Hers, &His, &Ended }) Each->Begin(1);
		Hers.Told({ Said(100.0, "sea:sonar", 20, 30, "asdic", ""), Said(150.0, "sea:sunk", 30, -1, "", ""), Said(160.0, "sea:sonar", 20, 30, "asdic", "") });
		His.Told({ Said(100.0, "sea:sonar", 21, 31, "radar", ""), Said(150.0, "sea:sunk", 21, -1, "", "") });
		Ended.Told({ Said(100.0, "sea:sonar", 21, 31, "sight", ""), Said(150.0, "sea:sonar", 21, 31, "sight", "sunk") });
		Check("a contact closed only by sea:sunk (hers, or her hunter's) or a loss told sunk is not held 1 s later",
		      Hers.Covers(30, 0, 100.0, 150.0) && !Hers.Covers(30, 0, 151.0, 151.0) && !Hers.Covers(30, 0, 161.0, 161.0) && His.Covers(31, 0, 100.0, 150.0) &&
		      !His.Covers(31, 0, 151.0, 151.0) && Ended.Covers(31, 0, 100.0, 150.0) && !Ended.Covers(31, 0, 151.0, 151.0),
		      std::string(Hers.Covers(30, 0, 151.0, 151.0) ? "hers held on" : "hers over") + ", " + (His.Covers(31, 0, 151.0, 151.0) ? "his held on" : "his over") + ", " +
		      (Ended.Covers(31, 0, 151.0, 151.0) ? "a loss told sunk held on" : "a loss told sunk over"));
	}
	// A BOOKED BLOW BY HER, AND HER HUNTERS' LISTS (the test's own run over a boat, read through the game's own reader: no
	// battle's recording has a charge pass so near a boat)
	{
		SeaFeed::Recording Over;
		std::string Error;
		const bool bRead = SeaFeed::ReadRecording(RunOver(-1.0, 0.0, -1.0), Over, Error) && !Over.Views.empty();
		Check("the test's own run over a boat read through the game's reader", bRead, bRead ? std::to_string(Over.Views.size()) + " views" : Error);
		if (bRead)
		{
			// Her hunters' lists: kept by the reader through the views that leave them out; where the views carry them they hold
			// her (the player's escorts in them, AttackHolds after one leaves), not the sonar events; one of her own side in them
			// holds nothing.
			Sight Hunted;
			Hunted.Begin(1);
			bool bKept = true;
			for (const SeaFeed::Frame& F : Over.Views)
			{
				Hunted.Told(F.Events);
				Hunted.Heard(F.Head.T, F.Pieces);
				for (const SeaShow::Piece& P : F.Pieces)
					if (P.Id == 30) bKept = bKept && P.bHeard && P.Heard == (F.Head.T < 11.5 ? std::vector<int>{} : F.Head.T < 51.5 ? std::vector<int>{ 20, 40 } : std::vector<int>{ 40 });
			}
			Check("her hunters' lists (extra.heard) kept through the views that leave them out, and holding her in place of the sonar events",
			      bKept && !Hunted.Covers(30, 0, 11.0, 11.0) && Hunted.Covers(30, 0, 12.0, 52.0 + Sight::AttackHolds) && !Hunted.Covers(30, 0, 53.0 + Sight::AttackHolds, 53.0 + Sight::AttackHolds),
			      std::string(bKept ? "kept" : "not kept") + "; the sonar's gain at 10 s " + (Hunted.Covers(30, 0, 11.0, 11.0) ? "read" : "left for the lists") + ", found at " + Num(Hunted.FoundAt(30)));
			// The booked blow: told as one of his charges passes within twice its lethal reach of her keel, not within it, it bursts
			// that reach nearer her than where that charge was drawn the frame before (SeaShow's rule), counted by the very tally
			// the recordings' look is checked with. His pattern aimed from the run with none (AimOver), the moment to tell the
			// blow at taken from the show's own charges in the run with it (Replay), then the run told so played again.
			const double Lethal = Over.Board.ChargeLethal;
			double Drop = -1.0, Spread = 0.0, Miss = 0.0;
			const bool bAimed = AimOver(Over, Drop, Spread, Miss);
			Replayed First, Hit;
			SeaFeed::Recording Laid, Struck;
			if (bAimed && SeaFeed::ReadRecording(RunOver(Drop, Spread, -1.0), Laid, Error) && !Laid.Views.empty()) First = Replay(Laid);
			if (First.NearT >= 0.0 && SeaFeed::ReadRecording(RunOver(Drop, Spread, First.NearT), Struck, Error) && !Struck.Views.empty()) Hit = Replay(Struck);
			const std::string Aim = bAimed ? "dropped at " + Num(Drop, 2) + " s across " + Num(Spread, 1) + " m (aimed " + Num(Miss, 2) + " m off her)" : "no aim within 2 m of her (best " + Num(Miss, 2) + " m)";
			Check("a blow told as his charge passes within twice its lethal reach of her keel bursts within that reach of where it was drawn the frame before",
			      bAimed && First.NearT >= 0.0 && Hit.Tally.BookedNear >= 1 && Hit.Tally.BookedOff == 0 && Hit.Booked == 1 && Hit.FromOwn >= 0.0 && Hit.FromOwn <= Lethal + 0.5,
			      Aim + "; " + (First.NearT < 0.0 ? std::string("no charge passed so near her") : "told at " + Num(First.NearT, 2) + " s, the charge " + Num(First.NearFar, 2) + " m from her keel; " +
			      std::to_string(Hit.Tally.BookedNear) + " booked by her, burst " + Num(Hit.FromOwn, 2) + " m from where it was drawn (" + Num(Hit.OwnFar, 2) + " m from her keel)"));
		}
	}
	for (int A = 2; A < ArgC; ++A)
	{
		const std::string Path = ArgV[A];
		SeaFeed::Recording Rec;
		std::string Error;
		const std::string Text = ReadAll(Path);
		if (Text.empty() || !SeaFeed::ReadRecording(Text, Rec, Error) || Rec.Views.empty())
		{
			std::printf("%s: %s\n", Path.c_str(), Error.c_str());
			return 2;
		}
		std::printf("%s\n", Path.c_str());
		bool bBoats = false;
		for (const SeaFeed::Frame& V : Rec.Views)
			for (const SeaShow::Piece& P : V.Pieces) bBoats = bBoats || P.Class == "ss";
		const Planned Watched = Plan(Rec, 0.25, nullptr, false), Staged = Plan(Rec, 0.25, nullptr, true);
		if (!bBoats)
		{
			Check("a fleet action has no window under the water, watched or staged", Watched.Windows.empty() && Staged.Windows.empty(),
			      std::to_string(Watched.Windows.size()) + " and " + std::to_string(Staged.Windows.size()));
			continue;
		}
		const int Mine = SeaFeed::SideOf(Rec.Views.front().Head.Me) == 1 ? 1 : 0;
		std::string ShotList;
		for (const auto& Shot : Watched.Shots) ShotList += (ShotList.empty() ? "" : ", ") + Shot.first + " " + Num(Shot.second);
		Note("the scene's other shots (PlanShots, mirrored)", ShotList);
		Check("the shot plan mirrored whole (no straddle or gun duel this mirror leaves out)", Watched.bWhole, Watched.bWhole ? "whole" : "a shot not mirrored");
		// THE DIRECTOR, watched (against the real shots) and staged (the month's scene)
		{
			const Planned Again = Plan(Rec, 0.25, nullptr, false), Fine = Plan(Rec, 0.1, &Watched.Busy, false);
			Rules("watched", Rec, Watched, Again, Fine, false);
			const Planned AgainStaged = Plan(Rec, 0.25, nullptr, true), FineStaged = Plan(Rec, 0.1, &Staged.Busy, true);
			Rules("staged", Rec, Staged, AgainStaged, FineStaged, true);
		}
		if (!Watched.Windows.empty())
		{
			std::vector<double> Busy = Watched.Busy;
			const double Cut = Watched.Windows[0].From + 8.0;
			Busy.push_back(Cut);
			const Planned Crowded = Plan(Rec, 0.25, &Busy, false);
			bool bClear = true;
			for (const Window& W : Crowded.Windows) bClear = bClear && !(Cut > W.From - Director::ClearCut + 1e-6 && Cut < W.Until - 1e-6);
			Check("a window keeps clear of another shot's cut put in it (comes up in it)", bClear, "a cut at " + Num(Cut) + " s: " + Listed(Crowded.Windows));
		}
		if (Path.find("convoy-night") != std::string::npos)
		{
			for (const Planned* P : { &Watched, &Staged })
			{
				bool bA = false, bB = false;
				for (const Window& W : P->Windows)
				{
					bA = bA || (W.BoatPiece == 30 && W.What == Kind::Pattern && W.From >= 330.0 && W.From <= 332.5);
					bB = bB || (W.BoatPiece == 31 && W.What == Kind::Ahead && W.From >= 470.0 && W.From <= 476.0 && W.End >= 480.0);
				}
				Check(std::string(P == &Watched ? "watched" : "staged") + ": the convoy night goes under for Boat A's pattern at 330 s and Boat B's ahead salvo at 470 s, through her end",
				      bA && bB, Listed(P->Windows));
			}
			bool bDive = false;
			for (const Window& W : Watched.Windows) bDive = bDive || W.What == Kind::Dive;
			Check("Boat B's dive deep at 285 s gives way to the going-down shot (its cut at 288 s)", !bDive, Listed(Watched.Windows));
			// (a radar contact on Boat A from 63 s, lost at 68 s, does not hold her through her dive deep at 150 s: injected
			// here, the stand-in left as it is)
			SeaFeed::Recording Radar = Rec;
			bool bGained = false, bLost = false;
			for (SeaFeed::Frame& V : Radar.Views)
			{
				for (const double At : { 63.0, 68.0 })
				{
					bool& bDone = At < 65.0 ? bGained : bLost;
					if (bDone || V.Head.T < At) continue;
					SeaShow::Event E;
					E.T = At;
					E.What = "sea:sonar";
					E.Kind = "radar";
					E.Unit = 20;
					E.On = 30;
					E.Lost = At < 65.0 ? "" : "deep";
					V.Events.push_back(E);
					bDone = true;
				}
			}
			const Planned Seen = Plan(Radar, 0.25, nullptr, false);
			bool bNone = true;
			for (const Window& W : Seen.Windows) bNone = bNone && !(W.BoatPiece == 30 && W.From < 170.0);
			Check("a radar contact lost before her dive does not take the eye under with her", bNone && bGained && bLost, Listed(Seen.Windows));
			// (Boat A is never in an escort's sonar: without the blow on her at 330 s, Escort 3's pattern alone finds nothing, and
			// the eye stays up beside her, watched and staged; the stand-in left as it is)
			SeaFeed::Recording Unhit = Rec;
			int Dropped = 0;
			for (SeaFeed::Frame& V : Unhit.Views)
			{
				const auto Before = V.Events.size();
				V.Events.erase(std::remove_if(V.Events.begin(), V.Events.end(), [](const SeaShow::Event& E) { return E.What == "sea:depthChargeHit" && E.Unit == 30; }), V.Events.end());
				Dropped += static_cast<int>(Before - V.Events.size());
			}
			const Planned UnhitWatched = Plan(Unhit, 0.25, nullptr, false), UnhitStaged = Plan(Unhit, 0.25, nullptr, true);
			bool bUnseen = Dropped == 1;
			for (const Planned* P : { &UnhitWatched, &UnhitStaged })
				for (const Window& W : P->Windows) bUnseen = bUnseen && !(W.BoatPiece == 30 && W.Until >= 300.0 && W.From <= 400.0);
			Check("a pattern on Boat A no escort holds, her blow at 330 s taken out: no window on her near it, watched or staged", bUnseen,
			      std::to_string(Dropped) + " blow taken out; " + Listed(UnhitWatched.Windows) + " | " + Listed(UnhitStaged.Windows));
		}
		if (Path.find("sub-air-mediterranean") != std::string::npos)
		{
			for (const Planned* P : { &Watched, &Staged })
			{
				bool bOne = false, bTwo = false;
				for (const Window& W : P->Windows)
				{
					bOne = bOne || (W.BoatPiece == 1 && W.What == Kind::Pattern);
					bTwo = bTwo || (W.BoatPiece == 2 && W.What == Kind::Pattern);
				}
				Check(std::string(P == &Watched ? "watched" : "staged") + ": the player's own boats, under for the patterns on boats 1 and 2", bOne && bTwo, Listed(P->Windows));
			}
		}
		// THE LOOK, played as the scene plays it, for the watched windows and then the staged ones
		double SunFrom = 0.0, SunUp = 45.0;
		StageSun(Rec, SunFrom, SunUp);
		const double Daylight = DaylightOf(SunUp);
		Note("the stage's sun", Num(SunUp) + " deg up from " + Num(SunFrom, 0) + " (the recording's own: " + Num(Rec.Board.SunUp) + "); daylight " + Num(Daylight, 2));
		for (const Planned* P : { &Watched, &Staged })
		{
			const std::string How = P == &Watched ? "watched" : "staged";
			const std::vector<Window>& Windows = P->Windows;
			const size_t N = Windows.size();
			SeaShow::Show Live(Rec.Seed ? Rec.Seed : 239);
			Live.SetField(Rec.Board);
			const SeaShow::Field& Board = Live.Board();
			Water Sea;
			Sea.Begin(Board);
			size_t Taken = 0;
			auto TakeNext = [&]()
			{
				const SeaFeed::Frame& V = Rec.Views[Taken++];
				Live.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
			};
			TakeNext();
			std::vector<SeaShow::Show::ChargeDown> Down;
			std::vector<Drum> Drums, DrumsBefore;
			std::map<size_t, int> FramesIn, Framed;
			std::map<size_t, std::set<uint64_t>> SeenSinking;
			std::map<int, double> SunkAt; // (a boat's ship key: her Sink)
			int Dry = 0, InHull = 0, InOther = 0, InFace = 0, Jumps = 0, OverBudget = 0, Fast = 0, Unheld = 0;
			int EarlyAir = 0, AirOffHer = 0, AirFrames = 0, AfterEnd = 0, AheadStruck = 0, AheadHigh = 0;
			double FastestSideways = 0.0, HighestStruck = -1e18;
			BurstTally Bursts;
			std::vector<Drum> BombsBefore, BombsAfter;
			P3 LastEye;
			size_t LastWindow = static_cast<size_t>(-1);
			const double BeginT = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
			const int Frames = static_cast<int>(std::floor((EndT + 1e-6 - BeginT) * 30.0)) + 1;
			for (int Fr = 0; Fr < Frames; ++Fr)
			{
				const double T = BeginT + Fr / 30.0;
				while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
				Live.Advance(T);
				std::map<int, BoatNow> Boats;
				std::vector<Hull> Hulls;
				for (const SeaShow::ShipOut& Ship : Live.Ships())
				{
					if (Ship.Doing == SeaShow::Fate::Gone) continue;
					Hulls.push_back(HullOf(Ship));
					if (Ship.Class == "ss" && !Boats.count(Ship.Piece))
						Boats[Ship.Piece] = BoatNow{ P3(Ship.At.X, Ship.At.Y, -Ship.Keel), ConvoyLook::AheadOf(Ship.Heading) * Ship.Speed };
				}
				Live.ChargesDown(Down);
				Sea.Remember(T, Down, Boats);
				for (const SeaShow::Fx& E : Live.Effects)
				{
					if (E.Kind == SeaShow::FxKind::ChargeBurst)
					{
						// (each charge, where it was drawn the frame before, is where it bursts: the nearest of its escort's; the
						// booked blow by her, BurstTally)
						if (E.What == "pattern") Bursts.Take(E, DrumsBefore, Boats, Board.ChargeLethal);
						// (an ahead salvo's: the bomb that bursts, gone from those sinking, never one more than 5 m over the burst)
						if (E.What == "ahead") Sea.AheadSinking(T, BombsBefore);
						Sea.Burst(T, P3(E.At.X, E.At.Y, E.At.Z), E.What);
						if (E.What == "ahead")
						{
							Sea.AheadSinking(T, BombsAfter);
							for (const Drum& B : BombsBefore)
							{
								bool bStill = false;
								for (const Drum& Left : BombsAfter) bStill = bStill || Left.Key == B.Key;
								if (bStill) continue;
								++AheadStruck;
								HighestStruck = std::max(HighestStruck, B.At.Z - E.At.Z);
								AheadHigh += B.At.Z > E.At.Z + Water::AheadAbove + 1e-9 ? 1 : 0;
							}
						}
					}
					else if (E.Kind == SeaShow::FxKind::ChargeDrop && E.What == "ahead")
					{
						AheadInto(Sea, Live, E, T);
					}
					else if (E.Kind == SeaShow::FxKind::Sink)
					{
						// (as FSeaUnder::Effect: every boat's end, whichever is posed; her air hers alone)
						const SeaShow::ShipOut* Lost = Live.Ship(E.Key);
						if (Lost && Lost->Class == "ss")
						{
							Sea.Ended(T, E.Key);
							SunkAt.emplace(E.Key, T);
						}
					}
				}
				Live.Effects.clear();
				Sea.Charges(T, Drums);
				// (no charge slides sideways faster than 3 m a second: a charge told on her eased there over its fall)
				for (const Drum& D : Drums)
					for (const Drum& B : DrumsBefore)
						if (B.Key == D.Key)
						{
							const double Sideways = std::hypot(D.At.X - B.At.X, D.At.Y - B.At.Y) * 30.0;
							FastestSideways = std::max(FastestSideways, Sideways);
							Fast += Sideways > 3.0 ? 1 : 0;
						}
				DrumsBefore = Drums;
				for (size_t K = 0; K < N; ++K)
				{
					const Window& W = Windows[K];
					if (T < W.From || T > W.HoldUntil) continue;
					const SeaShow::ShipOut* Boat = Live.Ship(W.Boat);
					const BoatPose Pose = Boat ? PoseOf(*Boat) : BoatPose();
					EyePose Eye;
					if (!EyeOf(W, Pose, Hulls, T, Eye)) continue;
					// (a clean cut through the face: never an eye half in it)
					InFace += Eye.Eye.Z > -1.5 + 1e-9 && Eye.Eye.Z < 3.0 - 1e-9 ? 1 : 0;
					if (T > W.Until) continue;
					++FramesIn[K];
					Dry += Eye.Eye.Z > -1.5 + 1e-9 || !Eye.bUnder ? 1 : 0;
					const Murk M = MurkAt(-Eye.Eye.Z, Daylight, SunUp);
					if (Pose.bThere)
					{
						const P3 Ahead = ConvoyLook::AheadOf(Pose.Heading);
						const P3 Rel = Eye.Eye - Pose.At;
						const double Along = std::max(-0.5 * Pose.Long, std::min(0.5 * Pose.Long, Rel.X * Ahead.X + Rel.Y * Ahead.Y));
						InHull += Gap(Eye.Eye, Pose.At + Ahead * Along) < 0.5 * Pose.Beam + 6.0 - 0.01 ? 1 : 0;
						// (she in the picture and not lost in the murk: some point of her, bow to stern, in it with 0.35 of its light left)
						bool bSeen = false;
						for (int J = 0; J <= 4; ++J)
						{
							const P3 Point = Pose.At + Ahead * (Pose.Long * (J / 4.0 - 0.5));
							bSeen = bSeen || (InPicture(Eye, Point) && LightLeft(Gap(Eye.Eye, Point), M.See) >= 0.35);
						}
						Framed[K] += bSeen ? 1 : 0;
					}
					for (const Hull& H : Hulls)
						if (!(H.bBoat && H.Key == W.Boat) && InsideHull(Eye.Eye, H, 1.0)) ++InOther;
					// (no enemy boat the escorts do not hold in the picture, near enough to be made out)
					for (const SeaShow::ShipOut& Ship : Live.Ships())
					{
						if (Ship.Class != "ss" || Ship.Key == W.Boat || Ship.Side == Mine || Ship.Doing == SeaShow::Fate::Gone || P->Held.Covers(Ship.Piece, Ship.Side, T, T)) continue;
						const BoatPose Other = PoseOf(Ship);
						const P3 Ahead = ConvoyLook::AheadOf(Other.Heading);
						for (int J = 0; J <= 4; ++J)
						{
							const P3 Point = Other.At + Ahead * (Other.Long * (J / 4.0 - 0.5));
							if (InPicture(Eye, Point) && Gap(Eye.Eye, Point) <= M.See) { ++Unheld; break; }
						}
					}
					Jumps += LastWindow == K && Gap(Eye.Eye, LastEye) > 1.5 ? 1 : 0;
					LastEye = Eye.Eye;
					LastWindow = K;
					Frame F;
					Sea.Lay(T, Eye.Eye, &Pose, Daylight, SunUp, SunFrom, F);
					OverBudget += (F.Motes.size() > static_cast<size_t>(Water::MostMotes) || F.Globes.size() > static_cast<size_t>(Water::MostGlobes) ||
					               F.Drums.size() > static_cast<size_t>(Water::MostDrums) || F.Bubbles.size() + F.Air.size() > static_cast<size_t>(Water::MostBubbles) ||
					               F.Shafts.size() > static_cast<size_t>(Water::MostShafts)) ? 1 : 0;
					// (her air only once she is lost, and from her hull: never another boat's end on one afloat)
					const auto Lost = SunkAt.find(Pose.Key);
					const bool bEnded = Pose.bThere && Lost != SunkAt.end() && T >= Lost->second;
					AfterEnd += bEnded ? 1 : 0;
					AirFrames += !F.Air.empty() ? 1 : 0;
					EarlyAir += !F.Air.empty() && !bEnded ? 1 : 0;
					for (const Glob& G : F.Air) AirOffHer += std::hypot(G.At.X - Pose.At.X, G.At.Y - Pose.At.Y) > 0.5 * Pose.Long + 1.0 ? 1 : 0;
					for (const Drum& D : F.Drums)
						if (!D.bBomb && InPicture(Eye, D.At) && LightLeft(Gap(Eye.Eye, D.At), M.See) >= 0.1) SeenSinking[K].insert(D.Key);
				}
			}
			int WellFramed = 0, PatternsSeen = 0, Patterns = 0;
			std::string Framing;
			for (size_t K = 0; K < N; ++K)
			{
				WellFramed += FramesIn[K] > 0 && Framed[K] >= 0.8 * FramesIn[K] ? 1 : 0;
				Framing += (Framing.empty() ? "" : ", ") + Num(FramesIn[K] > 0 ? 100.0 * Framed[K] / FramesIn[K] : 0.0, 0) + "%";
				if (Windows[K].What != Kind::Pattern) continue;
				++Patterns;
				PatternsSeen += SeenSinking[K].size() >= 3 ? 1 : 0;
			}
			const std::string Of = " of " + std::to_string(N);
			Check(How + ": the eye under the sea all through each window (1.5 m at the least)", Dry == 0, std::to_string(Dry) + " frames above it");
			Check(How + ": the eye through the face in a clean cut, never in it (under 1.5 m, or 3 m up)", InFace == 0, std::to_string(InFace) + " frames in it");
			Check(How + ": the eye never inside her hull (her half beam and 6 m)", InHull == 0, std::to_string(InHull) + " frames");
			Check(How + ": the eye never inside another ship or boat", InOther == 0, std::to_string(InOther) + " frames");
			Check(How + ": she is in the picture and not lost in the murk, four frames in five", WellFramed == static_cast<int>(N), std::to_string(WellFramed) + Of + " (" + Framing + ")");
			Check(How + ": no enemy boat the escorts do not hold in the picture", Unheld == 0, std::to_string(Unheld) + " frames");
			Check(How + ": the eye moves smoothly (1.5 m a frame at the most)", Jumps == 0, std::to_string(Jumps) + " jumps");
			std::string Seen;
			for (size_t K = 0; K < N; ++K) Seen += (Seen.empty() ? "" : ", ") + std::to_string(SeenSinking[K].size());
			Check(How + ": a pattern's charges seen sinking past her (3 at the least in the picture)", PatternsSeen == Patterns, std::to_string(PatternsSeen) + " of " + std::to_string(Patterns) + " (" + Seen + ")");
			Check(How + ": each charge drawn where it bursts (1.5 m)", Bursts.Off == 0, std::to_string(Bursts.Met) + " bursts, worst " + Num(Bursts.WorstOff, 2) + " m");
			Check(How + ": each booked blow with a charge drawn within twice its reach of her keel bursts within that reach of a charge drawn the frame before", Bursts.BookedOff == 0,
			      std::to_string(Bursts.BookedNear) + " such, worst " + Num(Bursts.WorstBooked, 2) + " m");
			if (Bursts.LaidOff > 0)
				Note(How + ": booked blows with no charge of the escort's drawn within twice its reach of her keel (the pattern laid off her: the show's boat and drop places, an ask), burst by her",
				     std::to_string(Bursts.LaidOff) + ", the nearest charge to her as far as " + Num(Bursts.FarthestLaid, 1) + " m");
			Check(How + ": no charge drawn sliding sideways faster than 3 m a second", Fast == 0, std::to_string(Fast) + " frames, the fastest " + Num(FastestSideways, 2) + " m/s");
			Check(How + ": the look within its budgets (160 motes, 12 globes, 40 drums, 240 bubbles and air, 6 shafts)", OverBudget == 0, std::to_string(OverBudget) + " frames over");
			// (a hit told sooner than its flight and keel/7 s after the throw, as the proposed rule has it at the soonest, is the
			// rules' to time: the water bursts it at the battle's place alone and lets no bomb burst above her)
			const std::string Struck = std::to_string(AheadStruck) + " bombs burst on her" + (AheadStruck ? " (the highest " + Num(HighestStruck, 1) + " m over the burst)" : "") + ", " +
			                           std::to_string(Sea.AheadEarly()) + " ahead bursts told before any bomb was down there, at the battle's place alone";
			Check(How + ": no ahead bomb bursts more than 5 m over the burst (one told before any could sink there bursts at the battle's place alone)", AheadHigh == 0, Struck);
			if (Path.find("convoy-night") != std::string::npos)
				Check(How + ": the rules' own timing of Boat B's ahead hit (480 s, 10 s after the throw: sooner than any bomb sinks to her 80 m) bursts at the battle's place, no bomb above her",
				      Sea.AheadEarly() >= 1 && AheadHigh == 0, Struck);
			Check(How + ": a boat's air only after her own end, and from her hull (never another boat's end on one afloat)", EarlyAir == 0 && AirOffHer == 0 && (AfterEnd == 0 || AirFrames > 0),
			      std::to_string(EarlyAir) + " frames early, " + std::to_string(AirOffHer) + " streams off her; " + std::to_string(AirFrames) + " frames of air in " + std::to_string(AfterEnd) + " after an end");
		}
		// THE PLAYER'S OWN DIVE (U), as FSeaUnder::PressU, Tick and EyeFor play it, against the watched plan's Sight (the one
		// TakeWindows gives the scene): every 15 s U pressed, up if down, else with the eye over each boat in turn (300 m south
		// of her, 900 m up) down beside the boat HandBoatFor gives; the dive ended as soon as she no longer holds (HandHolds).
		{
			SeaShow::Show Live(Rec.Seed ? Rec.Seed : 239);
			Live.SetField(Rec.Board);
			Water Sea;
			Sea.Begin(Live.Board());
			size_t Taken = 0;
			auto TakeNext = [&]()
			{
				const SeaFeed::Frame& V = Rec.Views[Taken++];
				Live.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
			};
			TakeNext();
			const Sight& Held = Watched.Held;
			const double ClearBy = UnheldClearBy();
			std::vector<SeaShow::Show::ChargeDown> Down;
			std::vector<BoatAt> Afloat;
			std::vector<Hull> Hulls;
			std::map<int, BoatNow> Moving;
			std::map<int, double> SunkAt;
			std::set<int> Beside;
			Frame F;
			int HandBoat = -1, HandSide = 1, Presses = 0, Dives = 0, Refused = 0, LetGo = 0, LastBoat = -1, LastFr = -1;
			int DiveFrames = 0, NotHeld = 0, UnheldIn = 0, Dry = 0, InHull = 0, InOther = 0, Jumps = 0, EarlyAir = 0, OverBudget = 0;
			bool bInjected = false, bBite = false, bKeptUp = false;
			double HandSince = 0.0;
			P3 LastEye;
			const double BeginT = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
			const int Frames = static_cast<int>(std::floor((EndT + 1e-6 - BeginT) * 30.0)) + 1;
			for (int Fr = 0; Fr < Frames; ++Fr)
			{
				const double T = BeginT + Fr / 30.0;
				while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
				Live.Advance(T);
				Hulls.clear();
				Moving.clear();
				for (const SeaShow::ShipOut& Ship : Live.Ships())
				{
					if (Ship.Doing == SeaShow::Fate::Gone) continue;
					Hulls.push_back(HullOf(Ship));
					if (Ship.Class == "ss") Moving.emplace(Ship.Piece, BoatNow{ P3(Ship.At.X, Ship.At.Y, -Ship.Keel), ConvoyLook::AheadOf(Ship.Heading) * Ship.Speed });
				}
				Live.ChargesDown(Down);
				Sea.Remember(T, Down, Moving);
				for (const SeaShow::Fx& E : Live.Effects)
				{
					if (E.Kind == SeaShow::FxKind::ChargeBurst) Sea.Burst(T, P3(E.At.X, E.At.Y, E.At.Z), E.What);
					else if (E.Kind == SeaShow::FxKind::ChargeDrop && E.What == "ahead") AheadInto(Sea, Live, E, T);
					else if (E.Kind == SeaShow::FxKind::Sink)
					{
						const SeaShow::ShipOut* Lost = Live.Ship(E.Key);
						if (Lost && Lost->Class == "ss")
						{
							Sea.Ended(T, E.Key);
							SunkAt.emplace(E.Key, T);
						}
					}
				}
				Live.Effects.clear();
				BoatsOf(Live, Afloat);
				if (HandBoat >= 0 && !HandHolds(Afloat, Held, HandBoat, T, ClearBy))
				{
					HandBoat = -1;
					++LetGo;
				}
				if (Fr > 0 && Fr % 450 == 0)
				{
					if (HandBoat >= 0) HandBoat = -1;
					else if (!Afloat.empty())
					{
						const BoatAt& Over = Afloat[static_cast<size_t>(Presses) % Afloat.size()];
						++Presses;
						const int Chosen = HandBoatFor(Afloat, Held, P3(Over.X, Over.Y - 300.0, 900.0), T, ClearBy);
						if (Chosen < 0) ++Refused;
						else
						{
							HandBoat = Chosen;
							HandSince = T;
							HandSide = HandOf(Chosen);
							++Dives;
							Beside.insert(Chosen);
						}
					}
				}
				if (HandBoat < 0) continue;
				const SeaShow::ShipOut* Boat = Live.Ship(HandBoat);
				if (!Boat || Boat->Doing == SeaShow::Fate::Gone)
				{
					HandBoat = -1; // (EyeFor: none once she is gone)
					continue;
				}
				const BoatPose Pose = PoseOf(*Boat);
				const EyePose Eye = EyeByHand(Pose, Hulls, T - HandSince, HandSide);
				++DiveFrames;
				NotHeld += Held.Covers(Boat->Piece, Boat->Side, T, T) ? 0 : 1;
				Dry += Eye.Eye.Z > -1.5 + 1e-9 ? 1 : 0;
				const P3 Ahead = ConvoyLook::AheadOf(Pose.Heading);
				const P3 Rel = Eye.Eye - Pose.At;
				const double Along = std::clamp(Rel.X * Ahead.X + Rel.Y * Ahead.Y, -0.5 * Pose.Long, 0.5 * Pose.Long);
				InHull += Gap(Eye.Eye, Pose.At + Ahead * Along) < 0.5 * Pose.Beam + 6.0 - 0.01 ? 1 : 0;
				for (const Hull& H : Hulls)
					if (!(H.bBoat && H.Key == HandBoat) && InsideHull(Eye.Eye, H, 1.0)) ++InOther;
				const Murk M = MurkAt(-Eye.Eye.Z, Daylight, SunUp);
				for (const SeaShow::ShipOut& Ship : Live.Ships())
				{
					if (Ship.Class != "ss" || Ship.Key == HandBoat || Ship.Side == Mine || Ship.Doing == SeaShow::Fate::Gone || Held.Covers(Ship.Piece, Ship.Side, T, T)) continue;
					const BoatPose Other = PoseOf(Ship);
					const P3 OtherAhead = ConvoyLook::AheadOf(Other.Heading);
					for (int J = 0; J <= 4; ++J)
					{
						const P3 Point = Other.At + OtherAhead * (Other.Long * (J / 4.0 - 0.5));
						if (InPicture(Eye, Point) && Gap(Eye.Eye, Point) <= M.See) { ++UnheldIn; break; }
					}
				}
				if (!bInjected && Boat->Side != Mine)
				{
					// A pack-mate the escorts have not found, 30 m beyond her from the eye at her depth, put into the boats the
					// rule reads (the show left as it is): in the frame within the murk were the eye let go under, so the dive
					// beside her is refused and one already going is ended.
					bInjected = true;
					const double Across = std::hypot(Pose.At.X - Eye.Eye.X, Pose.At.Y - Eye.Eye.Y);
					const P3 Beyond = Across > 1e-6 ? Pose.At + P3(Pose.At.X - Eye.Eye.X, Pose.At.Y - Eye.Eye.Y, 0.0) * (30.0 / Across) : Pose.At;
					std::vector<BoatAt> Packed = Afloat;
					Packed.push_back(BoatAt{ 1 << 20, 1 << 20, Boat->Side, Beyond.X, Beyond.Y, true, true, true });
					bBite = InPicture(Eye, Beyond) && Gap(Eye.Eye, Beyond) <= M.See;
					bKeptUp = HandBoatFor(Packed, Held, P3(Pose.At.X, Pose.At.Y - 300.0, 900.0), T, ClearBy) != HandBoat && !HandHolds(Packed, Held, HandBoat, T, ClearBy);
				}
				Jumps += LastBoat == HandBoat && LastFr == Fr - 1 && Gap(Eye.Eye, LastEye) > 1.5 ? 1 : 0;
				LastEye = Eye.Eye;
				LastBoat = HandBoat;
				LastFr = Fr;
				Sea.Lay(T, Eye.Eye, &Pose, Daylight, SunUp, SunFrom, F);
				OverBudget += (F.Drums.size() > static_cast<size_t>(Water::MostDrums) || F.Bubbles.size() + F.Air.size() > static_cast<size_t>(Water::MostBubbles)) ? 1 : 0;
				const auto Ended = SunkAt.find(Pose.Key);
				EarlyAir += !F.Air.empty() && (Ended == SunkAt.end() || T < Ended->second) ? 1 : 0;
			}
			std::string Whom;
			for (const int Key : Beside) Whom += (Whom.empty() ? "" : ", ") + std::to_string(Key);
			const std::string Tally = std::to_string(Dives) + " dives of " + std::to_string(Presses) + " presses down (" + std::to_string(Refused) + " refused, " +
			                          std::to_string(LetGo) + " ended as she no longer held), beside ships " + (Whom.empty() ? "none" : Whom) + ", " + std::to_string(DiveFrames) + " frames";
			if (Path.find("convoy-night") != std::string::npos || Path.find("sub-air-mediterranean") != std::string::npos)
				Check("by hand: U takes the eye under beside a boat he may see", Dives > 0, Tally);
			else
				Note("by hand", Tally);
			Check("by hand: every boat gone under beside his own or held, all through the dive", NotHeld == 0, std::to_string(NotHeld) + " frames not");
			Check("by hand: no enemy boat the escorts do not hold in the picture", UnheldIn == 0, std::to_string(UnheldIn) + " frames");
			Check("by hand: the eye under the sea (1.5 m), out of her hull (her half beam and 6 m) and every other", Dry == 0 && InHull == 0 && InOther == 0,
			      std::to_string(Dry) + " frames above, " + std::to_string(InHull) + " in her, " + std::to_string(InOther) + " in another");
			Check("by hand: the eye moves smoothly (1.5 m a frame at the most)", Jumps == 0, std::to_string(Jumps) + " jumps");
			Check("by hand: her air only after her own end, within the budgets", EarlyAir == 0 && OverBudget == 0, std::to_string(EarlyAir) + " frames early, " + std::to_string(OverBudget) + " over");
			if (Path.find("convoy-night") != std::string::npos)
				Check("by hand: an unfound pack-mate put 30 m beyond a held boat, in the frame were the eye let under, keeps it up", bInjected && bBite && bKeptUp,
				      std::string(bInjected ? "put in" : "never put in") + (bBite ? ", in the frame" : ", not in the frame") + (bKeptUp ? ", kept up" : ", gone under"));
		}
	}
	std::printf("%d checks, %d failed\n%s\n", Checks, Failed, Failed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failed ? 1 : 0;
}
