// The view under the water's cloud test (U1), run by Tools/sea_show/run.sh: the game's own Battle/SeaUnderCore, the sea
// show and its feed (no engine), over every recorded action and the stand-ins. The director is fed as ASeaBattle::PlanShots
// feeds it (a quarter second a step, the same trial), against the very shots PlanShots plans (mirrored here, each line it
// is mirrored from pinned in the source); the look is played as the scene plays it (30 frames a second), its sun the
// stage's own. Who is found is the show's alone (SeaShow's SetMine, ShipOut::bShown, the scene's player's side), the
// very rule the surface draws by: the trial, the look and the player's own dive all ask it, and no enemy boat the show
// hides is ever beside the eye or in its picture. Usage: sea_under_test <Source/TheaterCommand> recording.json ...
#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"
#include "Battle/SeaUnderCore.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace SeaUnder;

namespace
{
	constexpr double Pi = 3.14159265358979323846;
	int Checks = 0, Failed = 0, KnownFailed = 0;

	std::string Num(double X, int Places = 1)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, X);
		return Text;
	}

	/**
	 * The real nights' known failures under the water, kept as sea_show_test keeps the show's (the same rule, a list of its
	 * own): the naval rules' side of a failure on one of the convoy battle's real nights, until its rules fix lands, read
	 * from the list run.sh names (SEA_UNDER_KNOWN: Tools/sea_show/known_failures_under.txt; a line each, tab apart: the
	 * recording's file name without .json, the check's name as printed here, and the most its Measure may read). Within
	 * that it prints KNOWN and fails nothing; past it, a check with no Measure, or any check not listed, fails as ever; one
	 * listed that passes is told, to be taken off the list, and so is one listed for a night played that no check of that
	 * name ran on (its check gone or renamed). Night is the recording being played (empty for the test's own cases: never
	 * known).
	 */
	std::map<std::pair<std::string, std::string>, double> Known;
	std::set<std::pair<std::string, std::string>> Ran;
	std::set<std::string> Played;
	std::string Night;

	void ReadKnown()
	{
		const char* KnownPath = std::getenv("SEA_UNDER_KNOWN");
		if (!KnownPath) return;
		std::ifstream KnownIn(KnownPath);
		std::string Row;
		while (std::getline(KnownIn, Row))
		{
			if (Row.empty() || Row[0] == '#') continue;
			const size_t Tab1 = Row.find('\t'), Tab2 = Tab1 == std::string::npos ? std::string::npos : Row.find('\t', Tab1 + 1);
			if (Tab1 == 0 || Tab2 == std::string::npos) continue;
			Known[{ Row.substr(0, Tab1), Row.substr(Tab1 + 1, Tab2 - Tab1 - 1) }] = std::atof(Row.c_str() + Tab2 + 1);
		}
	}

	void Check(const std::string& What, bool bOk, const std::string& Got, double Measure = -1.0)
	{
		++Checks;
		if (!Night.empty()) Ran.insert({ Night, What });
		const auto Listed = Night.empty() ? Known.end() : Known.find({ Night, What });
		if (!bOk && Listed != Known.end() && Measure >= 0.0 && Measure <= Listed->second + 1e-9)
		{
			std::printf("  KNOWN %s: %s (known on this night, the rules' side: %s against up to %s)\n", What.c_str(), Got.c_str(), Num(Measure, 2).c_str(),
			            Num(Listed->second, 1).c_str());
			++KnownFailed;
			return;
		}
		Failed += bOk ? 0 : 1;
		std::printf("  %s  %s: %s%s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.c_str(),
		            Listed == Known.end() ? "" : bOk ? " (listed as known: take it off the list)" : (" (past its known bound, " + Num(Listed->second, 1) + ")").c_str());
	}

	void Note(const std::string& What, const std::string& Got) { std::printf("  note  %s: %s\n", What.c_str(), Got.c_str()); }

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
		{ "Battle/SeaBattle.cpp", "UnderPlan.Told(F.Head.T, F.Pieces, F.Events); // U1 hook (under water): its events and the escorts' echoes (the pings)" },
		{ "Battle/SeaBattle.cpp", "UnderPlan.Begin(S.Rec.Board, Under ? Under->UnheldClear() : SeaUnder::UnheldClearBy());" },
		// (who is found: the scene's show and its trial alike, the player's side, SeaShow's SetMine)
		{ "Battle/SeaBattle.cpp", "S.Show.SetMine(FParse::Param(FCommandLine::Get(), TEXT(\"TheaterSeaShowAll\")) ? -1 : SeaFeed::SideOf(S.Rec.Views.front().Head.Me));" },
		{ "Battle/SeaBattle.cpp", "Trial.SetMine(S.Show.MineSide());" },
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
		// (both lines in one picture: each second of the gun duel kept, and the search for its moment, BothLinesAt)
		{ "Battle/SeaBattle.cpp", "if (Step % 4 == 0 && (bDuel || LinesSeen.Num() > 0))" },
		{ "Battle/SeaBattle.cpp", "if ((Ship.Side == 0 || Ship.Side == 1) && Ship.bShown && (Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping))" },
		{ "Battle/SeaBattle.cpp", "One.bLine = Ship.LineAt >= 0;" },
		{ "Battle/SeaBattle.cpp", "One.bBig = Rank(Ship.Class) >= 2 || Ship.Class == \"cl\";" },
		{ "Battle/SeaBattle.cpp", "static constexpr double Least = 7.0, Most = 20.0, Holds = 0.85, Edge = 0.92, Tall = 9.0 / 16.0;" },
		{ "Battle/SeaBattle.cpp", "const int32 Mine = SeaFeed::SideOf(S.Rec.Views.front().Head.Me) == 1 ? 1 : 0;" },
		{ "Battle/SeaBattle.cpp", "const FVector D = FVector(P.X, P.Y, 8.0) - Pos;" },
		{ "Battle/SeaBattle.cpp", "const double HalfX = Edge * FMath::Tan(FMath::DegreesToRadians(Lens * 0.5)), HalfY = HalfX * Tall;" },
		{ "Battle/SeaBattle.cpp", "return Each < 0.5 ? 0.0 : (In[0] + In[1]) / static_cast<double>(Of[0] + Of[1]);" },
		{ "Battle/SeaBattle.cpp", "static constexpr int32 MostTries = 3000;" },
		{ "Battle/SeaBattle.cpp", "for (int32 I = FirstDuel == INDEX_NONE ? LinesSeen.Num() : FirstDuel + 15; I + static_cast<int32>(Least) < LinesSeen.Num() && LinesShare < Holds && Tried < MostTries; I += Stride)" },
		{ "Battle/SeaBattle.cpp", "bClear = bClear && !(Other.At > At - 7.0 && Other.At < At + Least);" },
		{ "Battle/SeaBattle.cpp", "if (One.Side == Side && (bAnyLine ? One.bLine : !bAnyBig || One.bBig))" },
		{ "Battle/SeaBattle.cpp", "if (Sides[K] == Mine && Where(Seen, Keys[K], From) && Where(Then, Keys[K], To))" },
		{ "Battle/SeaBattle.cpp", "for (const double Turn : { 0.0, 35.0, -35.0, 70.0, -70.0 })" },
		{ "Battle/SeaBattle.cpp", "if (Way.Size() > 1.0)" },
		{ "Battle/SeaBattle.cpp", "for (int32 Farther = 0; Farther < 14 && LinesShare < Holds && Tried < MostTries; ++Farther)" },
		{ "Battle/SeaBattle.cpp", "const double Back = 300.0 * FMath::Pow(1.25, Farther);" },
		{ "Battle/SeaBattle.cpp", "for (const double Up : { 300.0, 450.0, 600.0 })" },
		{ "Battle/SeaBattle.cpp", "for (const double Lens : { 50.0, 60.0 })" },
		{ "Battle/SeaBattle.cpp", "const int32 Lo = Xs.Num() * 3 / 100, Hi = Xs.Num() - 1 - Lo;" },
		{ "Battle/SeaBattle.cpp", "F = (F + FVector::CrossProduct(F, FVector::UpVector).GetSafeNormal() * ((Xs[Lo] + Xs[Hi]) * 0.5)).GetSafeNormal();" },
		{ "Battle/SeaBattle.cpp", "const double Tip = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Lens * 0.5)) * Tall / 3.0);" },
		{ "Battle/SeaBattle.cpp", "for (int32 J = 0; J <= static_cast<int32>(Least) && Least7 > LinesShare; ++J)" },
		{ "Battle/SeaBattle.cpp", "Stride = HereBest < 0.6 ? 5 : 1;" },
		{ "Battle/SeaBattle.cpp", "if (LinesFrom != INDEX_NONE && LinesShare >= 0.6)" },
		{ "Battle/SeaBattle.cpp", "Add(Lines, TEXT(\"both-lines\"), Lines.At);" },
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
		{ "Battle/SeaUnder.cpp", "const int32 Chosen = SeaUnder::HandBoatFor(BoatsNow, BoardOf(EyeAt), ClearBy);" },
		{ "Battle/SeaUnder.cpp", "HandSide = SeaUnder::HandOf(Chosen);" },
		{ "Battle/SeaUnder.cpp", "if (!SeaUnder::HandHolds(BoatsNow, HandBoat, ClearBy))" },
		// (the clearance from an unfound boat by the water's sight in use, for the player's dive and PlanShots' director alike)
		{ "Battle/SeaUnder.cpp", "ClearBy = SeaUnder::UnheldClearBy(DaySee, NightSee);" },
		{ "Battle/SeaUnder.cpp", "if (!Boat || Boat->Doing == SeaShow::Fate::Gone || !Boat->bShown || SeaUnder::CrewUp(*Boat, Now))" },
		{ "Battle/SeaUnder.cpp", "Pose = SeaUnder::EyeByHand(SeaUnder::PoseOf(*Boat), HullsOf(Show), Now - HandSince, HandSide);" },
		// (a window's eye posed beside her only while the show draws her and her crew are below: else up over its pattern)
		{ "Battle/SeaUnder.cpp", "SeaUnder::BoatPose Her = Boat ? SeaUnder::PoseOf(*Boat) : SeaUnder::BoatPose();" },
		{ "Battle/SeaUnder.cpp", "Her.bThere = Her.bThere && Boat != nullptr && !SeaUnder::CrewUp(*Boat, Now);" },
		{ "Battle/SeaUnder.cpp", "Sea.Ended(Now, E.Key);" },
		// (the ways a pattern's eye under her is tried from, Director::SeenMostWay: each 10 degrees, of length 1 as sin and cos
		// round it; THE LOOK'S OWN NUMBERS poses each)
		{ "Battle/SeaUnderCore.cpp", "const double Angle = K * 10.0 * Pi / 180.0;" },
		{ "Battle/SeaUnderCore.cpp", "Try.Near = P3(std::sin(Angle), std::cos(Angle), 0.0);" },
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
	// Recordings of the test's own (no battle's), read through the game's own reader

	/** An event told at its time, as the recording carries it. */
	struct Told { double T; std::string Json; };
	/** A ship or boat in them: her piece, side, class, place at 0 s and speed north, her views' extras (More), and from
	 * SunkAt on (below 0: never) her views saying she is gone, as a battle's recording does once she is sunk. */
	struct Craft { int Id; const char* Side; const char* Class; double X, Y, Speed, SunkAt; std::string More; };

	/** Views a second apart to Seconds, the player Me; each event in the view of its moment; a boat's hunters' list
	 * (extra.heard) where HeardAt gives one for her piece and the view's second (an empty string: that view leaves it out). */
	std::string Recorded(const char* Key, const char* What, std::vector<Told> Events, const std::vector<Craft>& Crafts, int Seconds, const char* Me,
	                     const std::function<std::string(int, int)>& HeardAt)
	{
		auto Unit = [](const Craft& C, double T, const std::string& More)
		{
			const bool bSunk = C.SunkAt >= 0.0 && T >= C.SunkAt - 1e-9;
			return "{\"id\":" + std::to_string(C.Id) + ",\"side\":\"" + C.Side + "\",\"kind\":\"" + C.Class + "\",\"name\":\"" + C.Class + " " + std::to_string(C.Id) +
			       "\",\"nat\":\"" + (std::string(C.Side) == "def" ? "UK" : "GER") + "\",\"x\":" + Num(C.X, 2) + ",\"y\":" + Num(C.Y + C.Speed * T, 2) +
			       ",\"face\":0,\"str\":" + (bSunk ? "0" : "1") + ",\"mor\":1,\"state\":\"" + (bSunk ? "gone" : "steady") + "\",\"figures\":" + (bSunk ? "0" : "1") +
			       ",\"figures0\":1,\"d\":0,\"act\":\"march\",\"speed\":" + Num(bSunk ? 0.0 : C.Speed, 2) + ",\"engaged\":-1,\"extra\":{\"cls\":\"" + C.Class +
			       "\",\"ships0\":1,\"ships\":" + (bSunk ? "0" : "1") + ",\"sunk\":" + (bSunk ? "1" : "0") + ",\"fleet\":\"" + C.Side + "\"" + More + "}}";
		};
		std::string Views;
		size_t Next = 0;
		std::sort(Events.begin(), Events.end(), [](const Told& L, const Told& R) { return L.T < R.T; });
		for (int K = 0; K <= Seconds; ++K)
		{
			const double T = K;
			std::string Said;
			for (; Next < Events.size() && Events[Next].T <= T + 1e-9; ++Next) Said += (Said.empty() ? "" : ",") + Events[Next].Json;
			std::string Units;
			for (const Craft& C : Crafts)
			{
				const std::string Heard = HeardAt ? HeardAt(C.Id, K) : std::string();
				Units += (Units.empty() ? "" : ",") + Unit(C, T, C.More + (Heard.empty() ? "" : ",\"heard\":" + Heard));
			}
			Views += std::string(K ? "," : "") + "{\"t\":" + std::to_string(K) + ",\"limit\":" + std::to_string(Seconds) + ",\"speed\":1,\"me\":\"" + Me +
			         "\",\"contact\":0,\"units\":[" + Units + "],\"events\":[" + Said + "]}";
		}
		return std::string("{\"what\":\"") + What + "\",\"recording\":1,\"key\":\"" + Key + "\",\"seed\":241,\"step\":1,"
		       "\"field\":{\"size\":20000,\"edge\":5000,\"zoneName\":\"Western Approaches\",\"title\":\"A run over a boat (the test's own)\",\"when\":{\"year\":1942,\"month\":11},"
		       "\"lat\":55,\"lon\":-20,\"att\":{\"nat\":\"GER\",\"adj\":\"German\"},\"def\":{\"nat\":\"UK\",\"adj\":\"British\"},"
		       "\"classes\":{\"dd\":{\"len\":95,\"beam\":10,\"speed\":12,\"turn\":0.12,\"reach\":4000},\"ss\":{\"len\":67,\"beam\":6.2,\"speed\":7,\"turn\":0.1,\"reach\":3000}},"
		       "\"light\":{\"sun\":-24,\"moon\":{\"lit\":0.45,\"up\":22,\"from\":140},\"vis\":2500},\"subs\":{\"scope\":13,\"deep\":80,\"crush\":250,\"dived\":3.9},"
		       "\"dc\":{\"sink\":3.0,\"lethal\":6.0}},\"views\":[" + Views + "],\"after\":{\"lines\":[],\"level\":\"\",\"winner\":\"def\",\"seconds\":" + std::to_string(Seconds) + "}}";
	}

	/** A boat lying 60 m down, as the recordings of the test's own have one. */
	const std::string Deep = ",\"dive\":\"deep\",\"keel\":60,\"scope\":false";

	/**
	 * A run over a boat: one escort coming up astern of a boat lying 60 m down and passing her, both steaming north (he at
	 * 8 m/s, she at 3), she 200 m to starboard of his track, so he passes her some 200 m off: the battle's own places, where the
	 * show draws a boat deep (SeaShow::KeepClear: she keeps no water at the surface, so it no longer holds his hull off hers;
	 * the review of 24 November, the second), his asdic on her from 10 s, lost at 30 s. Her hunters' list (extra.heard) in every other view only, the reader keeping it between: none to 11 s, him and a
	 * surface ship of her own side far off from 12 s to 50 s, then that ship alone. DropT: he drops a pattern of three, Spread
	 * across, set to her depth (below 0: none); HitT: a sea:depthChargeHit on her told then (below 0: none). Me: the player;
	 * Seconds: how long it runs.
	 */
	std::string RunOver(double DropT, double Spread, double HitT, const char* Me = "def", int Seconds = 120)
	{
		std::vector<Told> Events = {
			{ 10.0, "{\"t\":10,\"what\":\"sea:sonar\",\"unit\":20,\"side\":\"def\",\"text\":\"\",\"extra\":{\"on\":30,\"kind\":\"asdic\",\"range\":420}}" },
			{ 30.0, "{\"t\":30,\"what\":\"sea:sonar\",\"unit\":20,\"side\":\"def\",\"text\":\"\",\"extra\":{\"on\":30,\"kind\":\"asdic\",\"lost\":\"close\"}}" },
		};
		if (DropT >= 0.0)
			Events.push_back({ DropT, "{\"t\":" + Num(DropT, 4) + ",\"what\":\"sea:depthCharge\",\"unit\":20,\"side\":\"def\",\"text\":\"\",\"extra\":{\"on\":30,\"n\":3,\"set\":60,"
			                          "\"sinking\":21,\"spread\":" + Num(Spread, 4) + ",\"kind\":\"pattern\"}}" });
		if (HitT >= 0.0)
			Events.push_back({ HitT, "{\"t\":" + Num(HitT, 4) + ",\"what\":\"sea:depthChargeHit\",\"unit\":30,\"side\":\"def\",\"text\":\"\",\"extra\":{\"from\":20,\"harm\":0.5}}" });
		const std::vector<Craft> Crafts = { { 20, "def", "dd", 10000.0, 8700.0, 8.0, -1.0, "" }, { 30, "att", "ss", 10200.0, 9000.0, 3.0, -1.0, Deep },
		                                    { 40, "att", "dd", 15000.0, 15000.0, 0.0, -1.0, "" } };
		return Recorded("run-over", "the view under the water's own run over a boat (the test's, not a battle's)", Events, Crafts, Seconds, Me,
		                [](int Id, int K) { return Id != 30 || K % 2 != 0 ? std::string() : std::string(K < 12 ? "[]" : K <= 50 ? "[20,40]" : "[40]"); });
	}

	/** An event of the test's own: a sonar contact (Lost empty: gained), a pattern dropped on her, a blow on her, an end. */
	Told SonarSaid(double T, int By, int On, const char* Kind, const char* Lost)
	{
		return { T, "{\"t\":" + Num(T, 2) + ",\"what\":\"sea:sonar\",\"unit\":" + std::to_string(By) + ",\"side\":\"def\",\"text\":\"\",\"extra\":{\"on\":" + std::to_string(On) +
		            ",\"kind\":\"" + Kind + "\"" + (*Lost ? std::string(",\"lost\":\"") + Lost + "\"" : std::string(",\"range\":900")) + "}}" };
	}
	Told PatternSaid(double T, int By, int On)
	{
		return { T, "{\"t\":" + Num(T, 2) + ",\"what\":\"sea:depthCharge\",\"unit\":" + std::to_string(By) + ",\"side\":\"def\",\"text\":\"\",\"extra\":{\"on\":" + std::to_string(On) +
		            ",\"n\":10,\"set\":60,\"sinking\":20,\"spread\":120,\"kind\":\"pattern\"}}" };
	}
	Told BlowSaid(double T, int By, int On)
	{
		return { T, "{\"t\":" + Num(T, 2) + ",\"what\":\"sea:depthChargeHit\",\"unit\":" + std::to_string(On) + ",\"side\":\"def\",\"text\":\"\",\"extra\":{\"from\":" + std::to_string(By) +
		            ",\"harm\":0.5}}" };
	}
	Told SunkSaid(double T, int Unit, const char* Side, const char* Class)
	{
		return { T, "{\"t\":" + Num(T, 2) + ",\"what\":\"sea:sunk\",\"unit\":" + std::to_string(Unit) + ",\"side\":\"" + Side + "\",\"text\":\"\",\"extra\":{\"cls\":\"" + Class +
		            "\",\"how\":\"crushed\"}}" };
	}

	// -------------------------------------------------------------------------------------------------------------
	// Who the show draws (SetMine), step by step

	/** One step of a show: every boat where it has her (SeaUnder::BoatsOf: drawn or hidden, her crew up or below). */
	struct StepSeen { double T = 0.0; std::vector<BoatAt> Boats; };

	/** The player's side as the scene's show takes it (SetMine: SideOf, -1 for neither). */
	int MineOf(const SeaFeed::Recording& Rec) { return SeaFeed::SideOf(Rec.Views.front().Head.Me); }

	/** A recording played through the show as the scene plays it (SetMine, the player's side), a quarter second a step. */
	std::vector<StepSeen> StepsOf(const SeaFeed::Recording& Rec)
	{
		std::vector<StepSeen> Out;
		SeaShow::Show Live(Rec.Seed ? Rec.Seed : 239);
		Live.SetField(Rec.Board);
		Live.SetMine(MineOf(Rec));
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& V = Rec.Views[Taken++];
			Live.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
		};
		TakeNext();
		const double BeginT = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
		for (int K = 0; BeginT + K * 0.25 <= EndT + 1e-6; ++K)
		{
			const double T = BeginT + K * 0.25;
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
			Live.Advance(T);
			Live.Effects.clear();
			Out.push_back(StepSeen{ T, {} });
			BoatsOf(Live, Out.back().Boats);
		}
		return Out;
	}

	/** Her (by piece) at the last step at or before T; null where the show has no such boat then. */
	const BoatAt* BoatThen(const std::vector<StepSeen>& Steps, int Piece, double T)
	{
		const BoatAt* Her = nullptr;
		for (const StepSeen& S : Steps)
		{
			if (S.T > T + 1e-9) break;
			Her = nullptr;
			for (const BoatAt& B : S.Boats)
				if (B.Piece == Piece) Her = &B;
		}
		return Her;
	}
	bool DrawnAt(const std::vector<StepSeen>& Steps, int Piece, double T)
	{
		const BoatAt* Her = BoatThen(Steps, Piece, T);
		return Her && Her->bShown;
	}

	/** Her first spell drawn from From on: its first and last step (below 0: none). */
	void FirstSpell(const std::vector<StepSeen>& Steps, int Piece, double From, double& OutFirst, double& OutLast)
	{
		OutFirst = OutLast = -1.0;
		for (const StepSeen& S : Steps)
		{
			if (S.T < From - 1e-9) continue;
			bool bDrawn = false;
			for (const BoatAt& B : S.Boats) bDrawn = bDrawn || (B.Piece == Piece && B.bShown);
			if (bDrawn)
			{
				if (OutFirst < 0.0) OutFirst = S.T;
				OutLast = S.T;
			}
			else if (OutFirst >= 0.0) return;
		}
	}

	/** Her spells drawn, said: "drawn 58.0-160.0, 300.0-450.0". */
	std::string Spells(const std::vector<StepSeen>& Steps, int Piece)
	{
		std::string Text;
		double From = -1.0, Last = -1.0;
		auto Said = [&Text, &From, &Last]() { Text += (Text.empty() ? "" : ", ") + Num(From) + "-" + Num(Last); };
		for (const StepSeen& S : Steps)
		{
			bool bDrawn = false;
			for (const BoatAt& B : S.Boats) bDrawn = bDrawn || (B.Piece == Piece && B.bShown);
			if (bDrawn)
			{
				if (From < 0.0) From = S.T;
				Last = S.T;
			}
			else if (From >= 0.0)
			{
				Said();
				From = -1.0;
			}
		}
		if (From >= 0.0) Said();
		return "boat " + std::to_string(Piece) + " drawn " + (Text.empty() ? std::string("never") : Text);
	}

	/** Her (by ship key) at each step from the one at or before Since through the one at or after Until (the scene's frames
	 * lie between them): Each is asked of her at each, and of her end where she went down in it (W.End); false at once. */
	bool EveryStep(const std::vector<StepSeen>& Steps, const Window& W, const std::function<bool(const BoatAt*, bool)>& Each)
	{
		size_t From = 0;
		while (From + 1 < Steps.size() && Steps[From + 1].T <= W.From + 1e-9) ++From;
		for (size_t K = From; K < Steps.size(); ++K)
		{
			const StepSeen& S = Steps[K];
			const BoatAt* Her = nullptr;
			for (const BoatAt& B : S.Boats)
				if (B.Key == W.Boat) Her = &B;
			if (!Each(Her, W.End >= 0.0 && S.T >= W.End)) return false;
			if (S.T >= W.Until - 1e-9) break;
		}
		return true;
	}

	/** Drawn by the show all through and her crew below (or ended and gone, where her end is in it). */
	bool DrawnThrough(const std::vector<StepSeen>& Steps, const Window& W)
	{
		return EveryStep(Steps, W, [](const BoatAt* Her, bool bEnded) { return ((!Her || !Her->bAfloat) && bEnded) || (Her && Her->bAfloat && Her->bShown && !Her->bStruck && !Her->bGun); });
	}

	/** Opened on her only where the show drew her as it opened, or finds her within it. */
	bool OpensFound(const std::vector<StepSeen>& Steps, const Window& W)
	{
		bool bFirst = true, bHidden = false, bFound = false;
		EveryStep(Steps, W, [&](const BoatAt* Her, bool)
		{
			const bool bDrawn = Her && Her->bShown;
			if (bFirst) bHidden = !bDrawn;
			else bFound = bFound || bDrawn;
			bFirst = false;
			return true;
		});
		return !bHidden || bFound;
	}

	/**
	 * Where to drop so his starboard thrower's charge goes down where she will be, from the run with no drop (a drop steers no
	 * one, so the ships go as they went): of a pattern of three SeaShow lays the middle one half the spread ahead of his stern
	 * and the spread's half less his half beam to starboard, bursting half his run and its sinking (21 s) after the drop. The
	 * moment (to a tenth of a second) and the spread that put it nearest her; false where none comes
	 * within 2 m. The moment the test then tells the blow at is the show's own (Replay), so this only aims. (He passes some
	 * 200 m off her, RunOver, so only a wide pattern's thrown charge comes down where she will be: up to 600 m across, the
	 * mechanism tried, not the drill.)
	 */
	bool AimOver(const SeaFeed::Recording& Rec, double& OutDrop, double& OutSpread, double& OutMiss)
	{
		struct Seen { P3 At; double Heading = 0.0, Speed = 0.0, Long = 95.0, Beam = 10.0; P3 Her; };
		std::vector<Seen> Each;
		SeaShow::Show Live(Rec.Seed ? Rec.Seed : 239);
		Live.SetField(Rec.Board);
		Live.SetMine(MineOf(Rec)); // (as the scene's show)
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
		Live.SetMine(MineOf(Rec)); // (as the scene's show)
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
		Sight Pings;                            // the asdic on each boat's hull (the trial's views)
		std::vector<StepSeen> Steps;            // every boat at each step of the trial (SetMine: whom the show draws)
		std::vector<double> Busy;               // the other shots' cut moments (the filler left out)
		std::vector<std::pair<std::string, double>> Shots;
		bool bWhole = true;                     // no shot this mirror leaves out (a straddle)
	};

	int RankOf(const std::string& Class) { return Class == "bb" ? 4 : Class == "cv" ? 3 : Class == "ca" ? 2 : 1; }

	/** A second of the gun duel as PlanShots keeps it (each second once it begins): whether in the duel, and each side's
	 * ships the show draws in their line or limping. */
	struct LineShip { int Key = -1, Side = -1; double X = 0.0, Y = 0.0; bool bLine = false, bBig = false; };
	struct LinesMoment { double T = 0.0; bool bDuel = false; std::vector<LineShip> Ships; };

	double DotOf(const P3& A, const P3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	P3 CrossOf(const P3& A, const P3& B) { return P3(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X); }
	/** FVector::GetSafeNormal (and FVector2D's, Z 0): none where it has next to no length. */
	P3 SafeNormal(const P3& A)
	{
		const double Square = DotOf(A, A);
		if (Square == 1.0) return A;
		if (Square < 1e-8) return P3();
		return A * (1.0 / std::sqrt(Square));
	}

	/**
	 * BOTH LINES IN ONE PICTURE, as ASeaBattle::PlanShots searches for it (mirrored line for line, its lines pinned above):
	 * from 15 s into the gun duel, each second clear of the other shots, each side's line weighed from places nearest
	 * first, the eye riding with the player's own ships; the moment its shot is taken at (its cut 4 s before), or below 0.
	 */
	double BothLinesAt(const std::vector<LinesMoment>& Seen, const std::vector<std::pair<std::string, double>>& Shots, int Mine)
	{
		constexpr double Least = 7.0, Most = 20.0, Holds = 0.85, Edge = 0.92, Tall = 9.0 / 16.0, CutBefore = 4.0;
		constexpr int MostTries = 3000;
		static_assert(Most > Least, "held past the 7 s (its hold, not mirrored: no moment hangs on it)");
		const int Count = static_cast<int>(Seen.size());
		int FirstDuel = -1;
		for (int K = 0; K < Count && FirstDuel < 0; ++K) FirstDuel = Seen[K].bDuel ? K : -1;
		auto Where = [](const LinesMoment& M, int Key, double& OutX, double& OutY)
		{
			for (const LineShip& One : M.Ships)
				if (One.Key == Key) { OutX = One.X; OutY = One.Y; return true; }
			return false;
		};
		auto Across = [](const P3& Pos, const P3& F, double PX, double PY, double& OutX, double& OutY)
		{
			const P3 R = SafeNormal(CrossOf(F, P3(0.0, 0.0, 1.0)));
			const P3 U = CrossOf(R, F);
			const P3 D = P3(PX, PY, 8.0) - Pos;
			const double Z = DotOf(D, F);
			if (Z < 1.0) return false;
			OutX = DotOf(D, R) / Z;
			OutY = DotOf(D, U) / Z;
			return true;
		};
		auto Share = [&](const LinesMoment& M, const std::vector<int>& Keys, const std::vector<int>& Sides, const P3& Pos, const P3& F, double Lens)
		{
			const double HalfX = Edge * std::tan(Lens * 0.5 * Pi / 180.0), HalfY = HalfX * Tall;
			int In[2] = { 0, 0 }, Of[2] = { 0, 0 };
			for (size_t K = 0; K < Keys.size(); ++K)
			{
				double PX = 0.0, PY = 0.0, X = 0.0, Y = 0.0;
				if (!Where(M, Keys[K], PX, PY)) continue;
				++Of[Sides[K]];
				In[Sides[K]] += Across(Pos, F, PX, PY, X, Y) && std::fabs(X) <= HalfX && std::fabs(Y) <= HalfY ? 1 : 0;
			}
			if (Of[0] == 0 || Of[1] == 0) return 0.0;
			const double Each = std::min(In[0] / static_cast<double>(Of[0]), In[1] / static_cast<double>(Of[1]));
			return Each < 0.5 ? 0.0 : (In[0] + In[1]) / static_cast<double>(Of[0] + Of[1]);
		};
		double LinesShare = -1.0, LinesAt = -1.0;
		int LinesFrom = -1, Tried = 0, Stride = 1;
		for (int I = FirstDuel < 0 ? Count : FirstDuel + 15; I + static_cast<int>(Least) < Count && LinesShare < Holds && Tried < MostTries; I += Stride)
		{
			Stride = 1;
			const LinesMoment& M = Seen[static_cast<size_t>(I)];
			const double At = M.T + CutBefore;
			bool bClear = M.bDuel;
			for (const auto& Other : Shots) bClear = bClear && !(Other.second > At - 7.0 && Other.second < At + Least);
			if (!bClear) continue;
			std::vector<int> Keys, Sides;
			P3 Middle[2];
			int Of[2] = { 0, 0 };
			for (int Side = 0; Side < 2; ++Side)
			{
				bool bAnyLine = false, bAnyBig = false;
				for (const LineShip& One : M.Ships)
				{
					bAnyLine = bAnyLine || (One.Side == Side && One.bLine);
					bAnyBig = bAnyBig || (One.Side == Side && One.bBig);
				}
				for (const LineShip& One : M.Ships)
					if (One.Side == Side && (bAnyLine ? One.bLine : !bAnyBig || One.bBig))
					{
						Keys.push_back(One.Key);
						Sides.push_back(Side);
						Middle[Side] = Middle[Side] + P3(One.X, One.Y, 0.0);
						++Of[Side];
					}
			}
			if (Of[0] == 0 || Of[1] == 0) continue;
			Middle[0] = Middle[0] * (1.0 / Of[0]);
			Middle[1] = Middle[1] * (1.0 / Of[1]);
			const LinesMoment& Then = Seen[static_cast<size_t>(I + static_cast<int>(Least))];
			P3 Way;
			int Riding = 0;
			for (size_t K = 0; K < Keys.size(); ++K)
			{
				double FX = 0.0, FY = 0.0, TX = 0.0, TY = 0.0;
				if (Sides[K] == Mine && Where(M, Keys[K], FX, FY) && Where(Then, Keys[K], TX, TY))
				{
					Way = Way + P3(TX - FX, TY - FY, 0.0) * (1.0 / Least);
					++Riding;
				}
			}
			Way = Riding > 0 ? Way * (1.0 / Riding) : P3();
			const P3 OverTo = SafeNormal(Middle[1 - Mine] - Middle[Mine]);
			if (std::fabs(OverTo.X) <= 1e-4 && std::fabs(OverTo.Y) <= 1e-4) continue;
			const P3 Between = (Middle[0] + Middle[1]) * 0.5;
			std::vector<P3> Looks, Offs;
			for (const double Turn : { 0.0, 35.0, -35.0, 70.0, -70.0 })
			{
				// (FVector2D::GetRotated: counterclockwise by the degrees)
				const double S = std::sin(Turn * Pi / 180.0), C = std::cos(Turn * Pi / 180.0);
				Looks.push_back(P3(C * OverTo.X - S * OverTo.Y, S * OverTo.X + C * OverTo.Y, 0.0));
				Offs.push_back(Middle[Mine]);
			}
			if (std::hypot(Way.X, Way.Y) > 1.0)
			{
				Looks.push_back(SafeNormal(Way));
				Offs.push_back(Between);
			}
			double HereBest = -1.0;
			for (int Farther = 0; Farther < 14 && LinesShare < Holds && Tried < MostTries; ++Farther)
			{
				const double Back = 300.0 * std::pow(1.25, Farther);
				for (const double Up : { 300.0, 450.0, 600.0 })
				{
					for (size_t L = 0; L < Looks.size(); ++L)
					{
						for (const double Lens : { 50.0, 60.0 })
						{
							const P3 Ground = Offs[L] - Looks[L] * Back;
							const P3 EyeP(Ground.X, Ground.Y, Up);
							P3 F = SafeNormal(P3(Looks[L].X, Looks[L].Y, 0.0));
							for (int Again = 0; Again < 2; ++Again)
							{
								std::vector<double> Xs;
								for (const int Key : Keys)
								{
									double PX = 0.0, PY = 0.0, X = 0.0, Y = 0.0;
									if (Where(M, Key, PX, PY) && Across(EyeP, F, PX, PY, X, Y)) Xs.push_back(X);
								}
								if (Xs.empty()) break;
								std::sort(Xs.begin(), Xs.end());
								const size_t Lo = Xs.size() * 3 / 100, Hi = Xs.size() - 1 - Lo;
								F = SafeNormal(F + SafeNormal(CrossOf(F, P3(0.0, 0.0, 1.0))) * ((Xs[Lo] + Xs[Hi]) * 0.5));
							}
							const double Tip = std::atan(std::tan(Lens * 0.5 * Pi / 180.0) * Tall / 3.0);
							F = P3(F.X * std::cos(Tip), F.Y * std::cos(Tip), -std::sin(Tip));
							++Tried;
							double Least7 = 1.0;
							for (int J = 0; J <= static_cast<int>(Least) && Least7 > LinesShare; ++J)
								Least7 = std::min(Least7, Share(Seen[static_cast<size_t>(I + J)], Keys, Sides, EyeP + P3(Way.X * J, Way.Y * J, 0.0), F, Lens));
							HereBest = std::max(HereBest, Least7);
							if (Least7 > LinesShare)
							{
								LinesShare = Least7;
								LinesFrom = I;
								LinesAt = At;
							}
							if (LinesShare >= Holds) break;
						}
						if (LinesShare >= Holds) break;
					}
					if (LinesShare >= Holds) break;
				}
			}
			Stride = HereBest < 0.6 ? 5 : 1;
		}
		return LinesFrom >= 0 && LinesShare >= 0.6 ? LinesAt : -1.0;
	}

	/** Busy: the cuts to use (empty: those the shots below give); Staged: the month's scene (its beats, or the whole at its pace). */
	Planned Plan(const SeaFeed::Recording& Rec, double Step, const std::vector<double>* Busy, bool bStaged)
	{
		Planned Out;
		SeaShow::Show Trial(Rec.Seed ? Rec.Seed : 239);
		Trial.SetField(Rec.Board);
		Trial.SetMine(MineOf(Rec)); // (the scene's show's, and so its trial's: SeaShow's SetMine)
		const double Mid = Rec.Board.Size * 0.5;
		Trial.Eye = SeaShow::Vec(Mid, Mid - 3000.0, 900.0);
		Director Dir;
		Dir.Begin(Rec.Board, UnheldClearBy());
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
		std::vector<LinesMoment> Lines;
		const int PerSecond = std::max(1, static_cast<int>(std::lround(1.0 / Step)));
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
			Out.Steps.push_back(StepSeen{ T, {} });
			BoatsOf(Trial, Out.Steps.back().Boats);
			// (the gun duel's seconds, for both lines in one picture: once it begins, every second)
			const bool bDuel = T - LastSalvoAt[0] <= 30.0 && T - LastSalvoAt[1] <= 30.0;
			if (K % PerSecond == 0 && (bDuel || !Lines.empty()))
			{
				Lines.push_back(LinesMoment{ T, bDuel, {} });
				for (const SeaShow::ShipOut& Ship : Trial.Ships())
					if ((Ship.Side == 0 || Ship.Side == 1) && Ship.bShown && (Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping))
						Lines.back().Ships.push_back(LineShip{ Ship.Key, Ship.Side, Ship.At.X, Ship.At.Y, Ship.LineAt >= 0, RankOf(Ship.Class) >= 2 || Ship.Class == "cl" });
			}
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
		const double BothAt = BothLinesAt(Lines, Out.Shots, SeaFeed::SideOf(Rec.Views.front().Head.Me) == 1 ? 1 : 0);
		if (BothAt >= 0.0) Add("both-lines", BothAt);
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
		Out.Pings = Dir.Sighted();
		return Out;
	}

	std::string Listed(const std::vector<Window>& Windows)
	{
		std::string Text;
		for (const Window& W : Windows)
			Text += (Text.empty() ? "" : "; ") + W.Name + " boat " + std::to_string(W.BoatPiece) + " " + Num(W.From) + "-" + Num(W.Until) + (W.Pace != 1.0 ? " at " + Num(W.Pace, 2) + "x" : "");
		return Text.empty() ? "none" : Text;
	}

	/** The charges of the pattern the escort (piece EscortPiece) dropped at DropT, as the show laid them (its ChargesDown half
	 * a second after the drop, before any blow told later could move one): each one's level distance from the boat (piece
	 * BoatPiece) where the plan's trial had her as it was due to burst (below 0: she was not there then). */
	std::vector<double> LaidFrom(const SeaFeed::Recording& Rec, const std::vector<StepSeen>& Steps, double DropT, int EscortPiece, int BoatPiece)
	{
		std::vector<double> Out;
		SeaShow::Show Live(Rec.Seed ? Rec.Seed : 239);
		Live.SetField(Rec.Board);
		Live.SetMine(MineOf(Rec)); // (as the scene's show)
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& V = Rec.Views[Taken++];
			Live.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
		};
		TakeNext();
		const double BeginT = Rec.Views.front().Head.T, UntilT = std::min(DropT + 0.5, Rec.Views.back().Head.T);
		for (int Fr = 0; BeginT + Fr / 30.0 <= UntilT + 1e-9; ++Fr)
		{
			const double T = BeginT + Fr / 30.0;
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
			Live.Advance(T);
			Live.Effects.clear();
		}
		int EscortKey = -1;
		for (const SeaShow::ShipOut& Ship : Live.Ships())
			if (Ship.Piece == EscortPiece) EscortKey = Ship.Key;
		std::vector<SeaShow::Show::ChargeDown> Down;
		Live.ChargesDown(Down);
		for (const SeaShow::Show::ChargeDown& C : Down)
		{
			if (C.Escort != EscortKey || C.Kind != "pattern" || std::fabs(C.Dropped - DropT) > 1e-3) continue;
			const BoatAt* Her = BoatThen(Steps, BoatPiece, C.Bursts);
			Out.push_back(Her ? std::hypot(C.At.X - Her->X, C.At.Y - Her->Y) : -1.0);
		}
		return Out;
	}

	/** The director's own rules over one plan. */
	void Rules(const std::string& How, const SeaFeed::Recording& Rec, const Planned& First, const Planned& Again, const Planned& Fine, bool bStaged)
	{
		const double BeginT = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
		const size_t N = First.Windows.size();
		bool bSame = Again.Windows.size() == N, bNear = Fine.Windows.size() == N;
		int Held = 0, Opened = 0, Short = 0, Spaced = 0, Screened = 0, Clear = 0;
		std::string Hidden;
		for (size_t K = 0; K < N; ++K)
		{
			const Window& W = First.Windows[K];
			bSame = bSame && K < Again.Windows.size() && W.From == Again.Windows[K].From && W.Until == Again.Windows[K].Until && W.Boat == Again.Windows[K].Boat;
			bNear = bNear && K < Fine.Windows.size() && std::fabs(W.From - Fine.Windows[K].From) <= 0.3 && std::fabs(W.Until - Fine.Windows[K].Until) <= 0.3 &&
			        W.Boat == Fine.Windows[K].Boat;
			// (the trial's own steps, SetMine: the show's own say on whom it draws, the very rule the surface draws by)
			const bool bDrawn = DrawnThrough(First.Steps, W);
			Held += bDrawn ? 1 : 0;
			Opened += OpensFound(First.Steps, W) ? 1 : 0;
			if (!bDrawn) Hidden += (Hidden.empty() ? "; not: " : ", ") + Spells(First.Steps, W.BoatPiece);
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
		Check(How + ": each window's boat drawn by the show all through (the player's own, or found: SetMine), her crew below", Held == static_cast<int>(N),
		      std::to_string(Held) + Of + Hidden);
		Check(How + ": no window opens on a boat the show hides unless the show finds her within it (the trial's steps)", Opened == static_cast<int>(N), std::to_string(Opened) + Of);
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
	ReadKnown();
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
		// (a pattern's eye under her looks the way it is given, Window::Near, for every way SeenMostWay tries, each 10 degrees,
		// whatever the last bit of its length as the maths library's sin and cos round it, a bit under 1 or over; only none,
		// Near 0, looks toward the pattern's middle from her Hand side: so the shot is the way the trial chose, on any build)
		{
			Window W;
			W.What = Kind::Pattern;
			W.Escort = 5;
			W.From = 0.0;
			W.Until = 60.0;
			W.HoldUntil = 72.0;
			W.FirstBurst = 40.0;
			W.Aim = P3(30.0, 10.0, 0.0);
			W.Hand = 1;
			BoatPose Her;
			Her.Key = 3;
			Her.bThere = true;
			Her.Heading = 30.0;
			Her.Keel = 60.0;
			Her.At = P3(0.0, 0.0, -MiddleOf(Her.Keel));
			// (the way the eye looks, level: before the first burst's last 8 s it stands on that way, looking up along it)
			auto LooksWay = [&](const P3& Near, P3& OutWay)
			{
				Window Try = W;
				Try.Near = Near;
				EyePose Eye;
				if (!EyeOf(Try, Her, std::vector<Hull>(), 0.5, Eye)) return false;
				const double Across = std::hypot(Eye.Look.X - Eye.Eye.X, Eye.Look.Y - Eye.Eye.Y);
				if (Across < 1e-6) return false;
				OutWay = P3((Eye.Look.X - Eye.Eye.X) / Across, (Eye.Look.Y - Eye.Eye.Y) / Across, 0.0);
				return true;
			};
			int Ways = 0, Held = 0;
			double WorstWay = 0.0;
			std::string Lost;
			for (int K = 0; K < 36; ++K)
			{
				const double Angle = K * 10.0 * Pi / 180.0;
				const P3 Way(std::sin(Angle), std::cos(Angle), 0.0);
				for (const double Bit : { 1.0 - 4e-16, 1.0, 1.0 + 4e-16 })
				{
					++Ways;
					P3 Got;
					const bool bPosed = LooksWay(Way * Bit, Got);
					const double Off = bPosed ? std::hypot(Got.X - Way.X, Got.Y - Way.Y) : 1e9;
					WorstWay = std::max(WorstWay, Off);
					if (Off < 1e-9) ++Held;
					else if (Lost.find(std::to_string(K * 10) + " ") == std::string::npos) Lost += std::to_string(K * 10) + " ";
				}
			}
			const P3 Right = ConvoyLook::RightOf(Her.Heading);
			const P3 Middle = P3(W.Aim.X - Her.At.X, W.Aim.Y - Her.At.Y, 0.0) + Right * (-W.Hand * 15.0);
			const double Mid = std::hypot(Middle.X, Middle.Y);
			P3 None;
			const bool bNone = LooksWay(P3(), None) && std::hypot(None.X - Middle.X / Mid, None.Y - Middle.Y / Mid) < 1e-9;
			Check("a pattern's eye under her looks each way SeenMostWay tries (every 10 degrees, its length a last bit under or over 1), and only with none toward the pattern's middle",
			      Held == Ways && bNone,
			      std::to_string(Held) + " of " + std::to_string(Ways) + " ways held, worst " + Num(WorstWay, 12) + (Lost.empty() ? "" : "; posed off the way at degrees " + Lost) +
			          (bNone ? "; none: the pattern's middle" : "; none: not the pattern's middle"));
		}
	}
	// THE ONE RULE ON WHAT MAY BE NEAR HER (no recording): a boat the show hides (not found: BoatAt::bShown, SeaShow's SetMine)
	// within the clearance of her (UnheldClearBy: 232.5 m in the water's own sight) keeps the eye up, for the director's
	// windows and the player's own dive alike (SeaUnder::ClearOfUnheld); so does a boat at her deck gun on the surface, the
	// war on her deck the surface's; and a boat struck and up, or at her gun, is never gone under beside
	{
		const double ClearBy = UnheldClearBy();
		// (boat 7 found, so the show draws her; boat 8 of her pack the escorts have not found, so it hides her)
		auto Pack = [](double Off)
		{
			return std::vector<BoatAt>{ BoatAt{ 7, 30, 0, 1000.0, 1000.0, true, true, true, false, false }, BoatAt{ 8, 31, 0, 1000.0 + Off, 1000.0, false, true, true, false, false } };
		};
		const std::vector<BoatAt> Close = Pack(150.0), Apart = Pack(250.0);
		const P3 Over(1000.0, 700.0, 900.0);
		Check("an unheld boat 150 m from a held one keeps the eye up beside her (refused, and a dive on her ended)",
		      !ClearOfUnheld(Close, 7, ClearBy) && HandBoatFor(Close, Over, ClearBy) < 0 && !HandHolds(Close, 7, ClearBy),
		      "the boat chosen: " + std::to_string(HandBoatFor(Close, Over, ClearBy)));
		Check("at 250 m the eye goes under beside the held one, and stays", ClearOfUnheld(Apart, 7, ClearBy) && HandBoatFor(Apart, Over, ClearBy) == 7 && HandHolds(Apart, 7, ClearBy),
		      "the boat chosen: " + std::to_string(HandBoatFor(Apart, Over, ClearBy)));
		std::vector<BoatAt> Before = Apart;
		Before[0].bShown = false;
		std::vector<BoatAt> Lost = Close;
		Lost[1].bAfloat = Lost[1].bUnderWay = false;
		std::vector<BoatAt> Stopped = Apart;
		Stopped[0].bUnderWay = false;
		Check("before she is found none; a sunk boat near her keeps nothing up; one not under way is not chosen", HandBoatFor(Before, Over, ClearBy) < 0 &&
		      HandBoatFor(Lost, Over, ClearBy) == 7 && HandBoatFor(Stopped, Over, ClearBy) < 0 && HandBoatFor(Apart, P3(1000.0, 1000.0 - HandReach - 10.0, 900.0), ClearBy) < 0,
		      "before " + std::to_string(HandBoatFor(Before, Over, ClearBy)) + ", by a sunk one " + std::to_string(HandBoatFor(Lost, Over, ClearBy)));
		// (the deck gun: a boat at it, drawn, 150 m from her keeps the eye up as a hidden one does, and the same boat quiet
		// does not; a boat struck and up, or at her gun, is never gone under beside, nor a dive beside her kept)
		std::vector<BoatAt> Gunning = Close, Quiet = Close;
		Gunning[1].bShown = Quiet[1].bShown = true;
		Gunning[1].bGun = true;
		std::vector<BoatAt> StruckUp = Apart, AtGun = Apart;
		StruckUp[0].bStruck = true;
		AtGun[0].bGun = true;
		Check("a boat at her deck gun 150 m from her keeps the eye up (refused, and a dive ended), quiet she does not; one struck and up, or at her gun, is never gone under beside",
		      !ClearOfUnheld(Gunning, 7, ClearBy) && HandBoatFor(Gunning, Over, ClearBy) < 0 && !HandHolds(Gunning, 7, ClearBy) && ClearOfUnheld(Quiet, 7, ClearBy) &&
		      HandBoatFor(Quiet, Over, ClearBy) == 7 && HandBoatFor(StruckUp, Over, ClearBy) < 0 && !HandHolds(StruckUp, 7, ClearBy) && HandBoatFor(AtGun, Over, ClearBy) < 0 &&
		      !HandHolds(AtGun, 7, ClearBy),
		      "by one at her gun " + std::to_string(HandBoatFor(Gunning, Over, ClearBy)) + ", by one quiet " + std::to_string(HandBoatFor(Quiet, Over, ClearBy)) + ", struck " +
		      std::to_string(HandBoatFor(StruckUp, Over, ClearBy)) + ", at her gun " + std::to_string(HandBoatFor(AtGun, Over, ClearBy)));
		// (her crew up on deck, as the show has her: struck and up, or her gun laid, or a round fired within GunQuiet s)
		SeaShow::ShipOut Below;
		Below.Class = "ss";
		SeaShow::ShipOut Surrendered = Below, Laid = Below, Fired = Below, Quieted = Below, Escort = Below;
		Surrendered.Struck = 0.0;
		Laid.bGunLaid = true;
		Fired.FiredAt = 100.0 - GunQuiet + 0.5;
		Quieted.FiredAt = 100.0 - GunQuiet - 0.5;
		Escort.Class = "dd";
		Escort.bGunLaid = true;
		Check("her crew up on deck (CrewUp): struck and up, or her deck gun laid or fired within 6 s; else below, and a ship's gun never",
		      !CrewUp(Below, 100.0) && CrewUp(Surrendered, 100.0) && CrewUp(Laid, 100.0) && CrewUp(Fired, 100.0) && !CrewUp(Quieted, 100.0) && !CrewUp(Escort, 100.0),
		      std::string(CrewUp(Fired, 100.0) ? "a round 5.5 s ago up" : "a round 5.5 s ago below") + ", " + (CrewUp(Quieted, 100.0) ? "6.5 s ago up" : "6.5 s ago below"));
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
		      !ClearOfUnheld(Apart, 7, Clearest) && HandBoatFor(Apart, Over, Clearest) < 0 && !HandHolds(Apart, 7, Clearest),
		      "the boat chosen: " + std::to_string(HandBoatFor(Apart, Over, Clearest)));
	}
	// WHO IS FOUND (recordings of the test's own, read through the game's reader): the show's alone (SeaShow's SetMine,
	// ShipOut::bShown), the very rule the surface draws by, as the view under the water reads it (SeaUnder::BoatsOf), and
	// the asdic's pings on her hull, the one thing of her hunt the show does not say (SeaUnder::Sight). The show's rule: a
	// contact gained finds her and holds her till its loss, a radio bearing finds nothing, an attack on her (a pattern,
	// made on a contact) or a blow finds her, and each keeps her found FoundFor (90 s) after, a contact lost to her hunter's
	// end or told "sunk" too; her own end is drawn as she goes down; the player's own boats always.
	{
		const double Found = SeaShow::Show::FoundFor;
		const std::vector<Craft> Hunters = { { 20, "def", "dd", 10000.0, 8000.0, 3.0, -1.0, "" }, { 21, "def", "dd", 12000.0, 8000.0, 3.0, -1.0, "" },
		                                     { 22, "def", "dd", 14000.0, 8000.0, 3.0, -1.0, "" }, { 30, "att", "ss", 10000.0, 9000.0, 3.0, -1.0, Deep },
		                                     { 31, "att", "ss", 12000.0, 9000.0, 3.0, -1.0, Deep }, { 32, "att", "ss", 14000.0, 9000.0, 3.0, -1.0, Deep } };
		auto Played = [&Hunters](const char* Key, const std::vector<Told>& Events, const std::vector<Craft>* Others, SeaFeed::Recording& Out, Sight& OutPings)
		{
			std::string Error;
			const bool bRead = SeaFeed::ReadRecording(Recorded(Key, "the view under the water's own hunt (the test's, not a battle's)", Events, Others ? *Others : Hunters, 300, "def", nullptr),
			                                          Out, Error) && !Out.Views.empty();
			OutPings.Begin();
			for (const SeaFeed::Frame& F : Out.Views)
			{
				OutPings.Told(F.Events);
				OutPings.Echoed(F.Head.T, F.Pieces);
			}
			return bRead ? StepsOf(Out) : std::vector<StepSeen>();
		};
		// (a spell drawn from First to Last, told at About: from it, FoundFor after, within a step and a half)
		auto Spell = [](double First, double Last, double From, double To) { return std::fabs(First - From) <= 0.6 && std::fabs(Last - To) <= 0.6; };
		{
			SeaFeed::Recording Rec;
			Sight Pings;
			const std::vector<StepSeen> Steps = Played("hunt-pattern", { PatternSaid(100.0, 22, 30), BlowSaid(130.0, 21, 31) }, nullptr, Rec, Pings);
			double First = -1.0, Last = -1.0, BlowFirst = -1.0, BlowLast = -1.0;
			FirstSpell(Steps, 30, 0.0, First, Last);
			FirstSpell(Steps, 31, 0.0, BlowFirst, BlowLast);
			Check("a pattern dropped on a boat no escort holds finds her (an attack is made on a contact), as a blow on her does: each FoundFor (90 s), then hidden",
			      !Steps.empty() && Spell(First, Last, 100.0, 100.0 + Found) && Spell(BlowFirst, BlowLast, 130.0, 130.0 + Found),
			      Spells(Steps, 30) + "; " + Spells(Steps, 31));
		}
		{
			SeaFeed::Recording Rec;
			Sight Pings;
			const std::vector<StepSeen> Steps = Played("hunt-lost", { SonarSaid(100.0, 20, 30, "asdic", ""), SonarSaid(150.0, 20, 30, "asdic", "close"), SonarSaid(105.0, 21, 31, "bearing", "") },
			                                           nullptr, Rec, Pings);
			double First = -1.0, Last = -1.0, Bearing = -1.0, BearingLast = -1.0;
			FirstSpell(Steps, 30, 0.0, First, Last);
			FirstSpell(Steps, 31, 0.0, Bearing, BearingLast);
			int By = -1, ByAfter = -1;
			const bool bPings = Pings.Pinged(30, 149.0, By) && By == 20 && !Pings.Pinged(30, 151.0, ByAfter) && !Pings.Pinged(30, 99.0, ByAfter);
			Check("a contact lost close keeps her found 90 s more (her hunter running in on her), no longer; the pings stop as it is lost; a bearing finds nothing",
			      !Steps.empty() && Spell(First, Last, 100.0, 150.0 + Found) && Bearing < 0.0 && bPings,
			      Spells(Steps, 30) + "; " + Spells(Steps, 31) + "; " + (bPings ? "pinged 100-150 s" : "pinged wrongly"));
		}
		{
			// (hers: sunk at 150 s, a stale contact on her told at 160 s; his: her hunter sunk at 150 s; a loss told "sunk")
			std::vector<Craft> Ending = Hunters;
			Ending[1].SunkAt = 150.0; // (escort 21)
			Ending[3].SunkAt = 150.0; // (boat 30)
			SeaFeed::Recording Rec;
			Sight Pings;
			const std::vector<StepSeen> Steps = Played("hunt-ended", { SonarSaid(100.0, 20, 30, "asdic", ""), SunkSaid(150.0, 30, "att", "ss"), SonarSaid(160.0, 20, 30, "asdic", ""),
			                                                           SonarSaid(100.0, 21, 31, "radar", ""), SunkSaid(150.0, 21, "def", "dd"),
			                                                           SonarSaid(100.0, 22, 32, "sight", ""), SonarSaid(150.0, 22, 32, "sight", "sunk") },
			                                           &Ending, Rec, Pings);
			double HisFirst = -1.0, HisLast = -1.0, LossFirst = -1.0, LossLast = -1.0, HerFirst = -1.0, HerLast = -1.0;
			FirstSpell(Steps, 31, 0.0, HisFirst, HisLast);
			FirstSpell(Steps, 32, 0.0, LossFirst, LossLast);
			FirstSpell(Steps, 30, 0.0, HerFirst, HerLast);
			// (drawn from her contact on, going down from 150 s, every step the show still has her: till her wreck is gone)
			double HerThere = -1.0;
			for (const StepSeen& S : Steps)
				for (const BoatAt& B : S.Boats)
					if (B.Piece == 30) HerThere = S.T;
			const BoatAt* Going = BoatThen(Steps, 30, 151.0);
			const bool bHerEnd = Going && Going->bShown && !Going->bUnderWay && std::fabs(HerFirst - 100.0) <= 0.6 && HerThere > 151.0 && std::fabs(HerLast - HerThere) <= 1e-9;
			int By = -1;
			const bool bPings = Pings.Pinged(30, 149.0, By) && !Pings.Pinged(30, 151.0, By) && !Pings.Pinged(30, 161.0, By) && !Pings.Pinged(31, 120.0, By) && !Pings.Pinged(32, 120.0, By);
			Check("a contact closed by her hunter's end (sea:sunk) or a loss told sunk keeps her found 90 s, as any loss; her own end drawn as she goes down; the pings stop at once",
			      !Steps.empty() && Spell(HisFirst, HisLast, 100.0, 150.0 + Found) && Spell(LossFirst, LossLast, 100.0, 150.0 + Found) && bHerEnd && bPings,
			      Spells(Steps, 31) + "; " + Spells(Steps, 32) + "; " + Spells(Steps, 30) + (bHerEnd ? " (going down from 150 s, drawn till the show lets her wreck go at " + Num(HerThere) + " s)" : " (her end not drawn)") + "; " +
			      (bPings ? "pinged hers to 150 s, never by radar or sight" : "pinged wrongly"));
		}
		{
			// (his own boats: the boats' side the player's, two of his pack 150 m apart with an escort's contact never on them)
			const std::vector<Craft> Pack = { { 20, "def", "dd", 10000.0, 8000.0, 3.0, -1.0, "" }, { 30, "att", "ss", 10000.0, 9000.0, 3.0, -1.0, Deep },
			                                  { 31, "att", "ss", 10150.0, 9000.0, 3.0, -1.0, Deep } };
			SeaFeed::Recording Rec;
			std::string Error;
			const bool bRead = SeaFeed::ReadRecording(Recorded("own-pack", "the player's own pack (the test's, not a battle's)", {}, Pack, 60, "att", nullptr), Rec, Error) && !Rec.Views.empty();
			const std::vector<StepSeen> Steps = bRead ? StepsOf(Rec) : std::vector<StepSeen>();
			int Hidden = 0, Chosen = -1, Kept = 0;
			for (const StepSeen& S : Steps)
			{
				for (const BoatAt& B : S.Boats) Hidden += B.bShown ? 0 : 1;
				const BoatAt* Her = nullptr;
				for (const BoatAt& B : S.Boats)
					if (B.Piece == 30) Her = &B;
				if (!Her) continue;
				if (Chosen == -1) Chosen = HandBoatFor(S.Boats, P3(Her->X, Her->Y - 300.0, 900.0), UnheldClearBy()) == Her->Key ? Her->Key : -2;
				for (const BoatAt& B : S.Boats) Kept += HandHolds(S.Boats, B.Key, UnheldClearBy()) ? 1 : 0;
			}
			const int All = static_cast<int>(Steps.size()) * 2;
			Check("the player's own boats: always drawn by the show (SetMine), gone under beside, side by side", bRead && !Steps.empty() && Hidden == 0 && Chosen >= 0 && Kept == All,
			      std::to_string(Hidden) + " steps hidden, " + std::to_string(Kept) + " of " + std::to_string(All) + " kept, the boat chosen " + std::to_string(Chosen));
		}
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
			// Her hunters' lists: kept by the reader through the views that leave them out; where the views carry them the show
			// holds her by them (SeaShow's Hunt: while an escort of the player's is in them, FoundFor after the last leaves), a
			// sonar contact gained finding her FoundFor as any does; one of her own side in them holds nothing. (The run played on
			// to 200 s, so the hold's end is in it.)
			SeaFeed::Recording Longer;
			const bool bLonger = SeaFeed::ReadRecording(RunOver(-1.0, 0.0, -1.0, "def", 200), Longer, Error) && !Longer.Views.empty();
			bool bKept = bLonger;
			for (const SeaFeed::Frame& F : Longer.Views)
				for (const SeaShow::Piece& P : F.Pieces)
					if (P.Id == 30) bKept = bKept && P.bHeard && P.Heard == (F.Head.T < 11.5 ? std::vector<int>{} : F.Head.T < 51.5 ? std::vector<int>{ 20, 40 } : std::vector<int>{ 40 });
			const std::vector<StepSeen> Hunted = bLonger ? StepsOf(Longer) : std::vector<StepSeen>();
			double HeldFrom = -1.0, HeldTo = -1.0;
			FirstSpell(Hunted, 30, 0.0, HeldFrom, HeldTo);
			double Again = -1.0, AgainTo = -1.0;
			FirstSpell(Hunted, 30, HeldTo + 0.5, Again, AgainTo);
			Check("her hunters' lists (extra.heard) kept through the views that leave them out, and holding her in the show: found from the sonar's gain at 10 s, held while "
			      "an escort of the player's is in them, FoundFor after he leaves them at 52 s, never by one of her own side",
			      bKept && std::fabs(HeldFrom - 10.0) <= 0.6 && std::fabs(HeldTo - (52.0 + SeaShow::Show::FoundFor)) <= 0.6 && Again < 0.0,
			      std::string(bKept ? "kept" : "not kept") + "; " + Spells(Hunted, 30));
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
			// A pattern is gone under for by where its charges were laid, hit or miss alike (the Director's PatternSeen, from
			// SeaShow's ChargeBurst Aim and Until): that aimed pattern (its thrown charge laid where she will be) gives a window
			// on her told as a miss, and told with the blow above (that very charge booked as it nears her keel); the same drop
			// across 40 m, its charges laid along his track 100 m and more off her, gives none, told as a miss and told with a
			// blow on her 10 s after the drop (the show puts that one by her, none of his lying near her keel). The other shots'
			// cuts are left out, so the pattern alone decides.
			{
				const std::vector<double> NoCuts;
				const double Across = 40.0, BlowAfter = 10.0;
				SeaFeed::Recording Wide, WideStruck;
				const bool bNearRead = !Laid.Views.empty() && !Struck.Views.empty();
				const bool bFarRead = bAimed && SeaFeed::ReadRecording(RunOver(Drop, Across, -1.0), Wide, Error) && !Wide.Views.empty() &&
				                      SeaFeed::ReadRecording(RunOver(Drop, Across, Drop + BlowAfter), WideStruck, Error) && !WideStruck.Views.empty();
				auto OnHer = [](const Planned& P)
				{
					bool bOn = false;
					for (const Window& W : P.Windows) bOn = bOn || (W.What == Kind::Pattern && W.BoatPiece == 30);
					return bOn;
				};
				// (the nearest laid, how many within PatternSeen m, as "nearest 3.2 m, 1 of 3 within 60 m")
				auto Nearest = [](const std::vector<double>& Each, int& OutWithin)
				{
					double Least = 1e18;
					OutWithin = 0;
					for (const double Far : Each)
					{
						if (Far < 0.0) continue;
						Least = std::min(Least, Far);
						OutWithin += Far <= Director::PatternSeen ? 1 : 0;
					}
					return Least;
				};
				bool bHolds = bNearRead && bFarRead;
				std::string Said = bHolds ? std::string() : std::string("a run not read");
				if (bHolds)
				{
					const Replayed FarBlow = Replay(WideStruck);
					struct Case { const char* Name; const SeaFeed::Recording* Rec; bool bNear; };
					const Case Cases[] = { { "aimed, a miss", &Laid, true }, { "aimed, the blow", &Struck, true }, { "across 40 m, a miss", &Wide, false },
					                       { "across 40 m, the blow", &WideStruck, false } };
					for (const Case& One : Cases)
					{
						const Planned P = Plan(*One.Rec, 0.25, &NoCuts, false);
						const std::vector<double> Each = LaidFrom(*One.Rec, P.Steps, Drop, 20, 30);
						int Within = 0;
						const double Least = Nearest(Each, Within);
						const bool bOn = OnHer(P);
						bHolds = bHolds && Each.size() == 3 && (One.bNear ? Least <= Director::PatternSeen && bOn : Least >= 100.0 && !bOn);
						Said += (Said.empty() ? "" : "; ") + std::string(One.Name) + ": laid " + Num(Least, 1) + " m from her at the nearest, " + std::to_string(Within) + " of " +
						        std::to_string(Each.size()) + " within " + Num(Director::PatternSeen, 0) + " m, " + (bOn ? "gone under" : "no window");
					}
					// (the blows were booked: the aimed one by its own charge, the wide one put by her)
					bHolds = bHolds && Hit.Booked == 1 && FarBlow.Booked == 1 && FarBlow.Tally.LaidOff == 1;
					Said += "; blows booked " + std::to_string(Hit.Booked) + " and " + std::to_string(FarBlow.Booked) + " (" + std::to_string(FarBlow.Tally.LaidOff) + " put by her)";
				}
				Check("a pattern gone under for by where its charges were laid, hit or miss alike: one laid within 60 m of her gives a window, one laid 100 m and more off her none",
				      bHolds, Said);
			}
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
		// (the night as the known list names it: the file's name without .json)
		Night = Path.substr(Path.find_last_of("/\\") == std::string::npos ? 0 : Path.find_last_of("/\\") + 1);
		if (Night.size() > 5 && Night.compare(Night.size() - 5, 5, ".json") == 0) Night.resize(Night.size() - 5);
		Played.insert(Night);
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
		Check("the shot plan mirrored whole (no straddle this mirror leaves out; both lines in a gun duel mirrored, BothLinesAt)", Watched.bWhole, Watched.bWhole ? "whole" : "a shot not mirrored");
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
			// here, the stand-in left as it is. The show keeps her found FoundFor after the loss, to 158 s, and no longer: what is
			// left of her dive's window by then is too short to go under for)
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
			double Since = -1.0, To = -1.0;
			FirstSpell(Seen.Steps, 30, 66.0, Since, To);
			const bool bLetGo = Since >= 0.0 && Since <= 66.0 && To <= 68.0 + SeaShow::Show::FoundFor + 2.5 && !DrawnAt(Seen.Steps, 30, 200.0);
			Check("a radar contact lost before her dive does not take the eye under with her (the show: found FoundFor after its loss at 68 s, no longer)", bNone && bGained && bLost && bLetGo,
			      Listed(Seen.Windows) + "; " + Spells(Seen.Steps, 30));
			// (Boat A with no escort's contact on her and no blow: her asdic contact at 300 s and her blow at 330 s taken out, the
			// stand-in left as it is. The show hides her until Escort 22's pattern on her at 330 s finds her (an attack is made on a
			// contact: SeaShow's sea:depthCharge), so no window on her opens before it, watched or staged, and none while hidden)
			SeaFeed::Recording Unhit = Rec;
			int Dropped = 0;
			for (SeaFeed::Frame& V : Unhit.Views)
			{
				const auto Before = V.Events.size();
				V.Events.erase(std::remove_if(V.Events.begin(), V.Events.end(), [](const SeaShow::Event& E)
				{
					return (E.What == "sea:depthChargeHit" && E.Unit == 30) || (E.What == "sea:sonar" && E.On == 30 && E.T >= 299.0 && E.T <= 301.0);
				}), V.Events.end());
				Dropped += static_cast<int>(Before - V.Events.size());
			}
			const Planned UnhitWatched = Plan(Unhit, 0.25, nullptr, false), UnhitStaged = Plan(Unhit, 0.25, nullptr, true);
			bool bUnseen = Dropped == 2;
			for (const Planned* P : { &UnhitWatched, &UnhitStaged })
				for (const Window& W : P->Windows) bUnseen = bUnseen && !(W.BoatPiece == 30 && W.Until >= 300.0 && W.From < 330.0) && DrawnThrough(P->Steps, W);
			const bool bHidden = !DrawnAt(UnhitWatched.Steps, 30, 329.75) && !DrawnAt(UnhitWatched.Steps, 30, 300.5) && DrawnAt(UnhitWatched.Steps, 30, 330.5);
			Check("a pattern on Boat A no escort holds, her contact at 300 s and her blow at 330 s taken out: hidden by the show until the pattern finds her, no window on her before it, watched or staged",
			      bUnseen && bHidden, std::to_string(Dropped) + " events taken out; " + Spells(UnhitWatched.Steps, 30) + "; " + Listed(UnhitWatched.Windows) + " | " + Listed(UnhitStaged.Windows));
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
		// A BOAT UP WITH HER CREW ON DECK, as the show has her (struck and up after a sea:surrender; at her deck gun on the
		// surface after a sea:gun: laid, or a round within 6 s): the war on her deck is the surface's, so no window is under
		// beside her while they are, watched or staged (the trial's own steps)
		{
			std::set<int> Struck, Gunners;
			std::map<int, std::string> Classes;
			for (const SeaFeed::Frame& V : Rec.Views)
			{
				for (const SeaShow::Piece& P : V.Pieces) Classes[P.Id] = P.Class;
				for (const SeaShow::Event& E : V.Events)
				{
					if (E.What == "sea:surrender" && E.Unit >= 0) Struck.insert(E.Unit);
					if (E.What == "sea:gun" && E.Unit >= 0) Gunners.insert(E.Unit);
				}
			}
			std::set<int> Up;
			for (const int Piece : Struck) Up.insert(Piece);
			for (const int Piece : Gunners)
				if (Classes[Piece] == "ss") Up.insert(Piece);
			if (!Up.empty())
			{
				bool bAllUp = true, bNoneUnder = true;
				std::string Said;
				for (const int Piece : Up)
				{
					double StruckAt = -1.0, GunAt = -1.0;
					for (const StepSeen& S : Watched.Steps)
						for (const BoatAt& B : S.Boats)
						{
							if (B.Piece != Piece) continue;
							if (B.bStruck && StruckAt < 0.0) StruckAt = S.T;
							if (B.bGun && GunAt < 0.0) GunAt = S.T;
						}
					bAllUp = bAllUp && (!Struck.count(Piece) || StruckAt >= 0.0) && (!Gunners.count(Piece) || GunAt >= 0.0);
					Said += (Said.empty() ? "boat " : "; boat ") + std::to_string(Piece) + (StruckAt >= 0.0 ? " struck and up from " + Num(StruckAt) : std::string()) +
					        (GunAt >= 0.0 ? " at her gun from " + Num(GunAt) : std::string());
					for (const Planned* P : { &Watched, &Staged })
						for (const Window& W : P->Windows)
						{
							if (W.BoatPiece != Piece) continue;
							const bool bBelow = EveryStep(P->Steps, W, [](const BoatAt* Her, bool) { return !Her || (!Her->bStruck && !Her->bGun); });
							bNoneUnder = bNoneUnder && bBelow;
							Said += std::string(", ") + (P == &Watched ? "watched " : "staged ") + Num(W.From) + "-" + Num(W.Until) + (bBelow ? "" : " (her crew up in it)");
						}
				}
				Check("a boat up with her crew on deck (struck and up, or at her deck gun, as the show has her): so in the show, and no window under beside her while they are, watched or staged",
				      bAllUp && bNoneUnder, Said);
			}
		}
		// (the night's booked blows from patterns as the battle books them: each from a pattern its escort laid within its
		// lethal reach of the battle's own boat where it dropped, and told as that pattern bursts at her (its drop and its
		// sinking time, within a view's three seconds), as the convoy field books them; or not, as the stand-ins' hand-made
		// blows come some seconds after their patterns are spent. On a night whose every such blow is so, the show drawing
		// her at her place where she is attacked (sea_show_test), every booked blow bursts by a charge drawn. The review of
		// 24 November, the second)
		int BlowsAtPattern = 0, BlowsElse = 0;
		{
			auto HerPlace = [&Rec](int Id, double When, double& OutX, double& OutY)
			{
				const SeaShow::Piece* Was = nullptr;
				const SeaShow::Piece* Then = nullptr;
				double TWas = 0.0, TThen = 0.0;
				for (const SeaFeed::Frame& W : Rec.Views)
				{
					const SeaShow::Piece* Here = nullptr;
					for (const SeaShow::Piece& Q : W.Pieces)
						if (Q.Id == Id) Here = &Q;
					if (!Here) continue;
					if (W.Head.T <= When + 1e-9) { Was = Here; TWas = W.Head.T; }
					else { Then = Here; TThen = W.Head.T; break; }
				}
				if (!Was) return false;
				const double Fr = Then && TThen > TWas ? std::clamp((When - TWas) / (TThen - TWas), 0.0, 1.0) : 0.0;
				OutX = Was->X + ((Then ? Then->X : Was->X) - Was->X) * Fr;
				OutY = Was->Y + ((Then ? Then->Y : Was->Y) - Was->Y) * Fr;
				return true;
			};
			std::map<std::pair<int, int>, const SeaShow::Event*> LastPattern; // (by escort and boat)
			for (const SeaFeed::Frame& V : Rec.Views)
				for (const SeaShow::Event& E : V.Events)
				{
					if (E.What == "sea:depthCharge" && E.Kind != "ahead" && E.On >= 0) LastPattern[{ E.Unit, E.On }] = &E;
					if (E.What != "sea:depthChargeHit" || E.Unit < 0) continue;
					const auto Drop = LastPattern.find({ E.From, E.Unit });
					if (Drop == LastPattern.end()) continue; // (an ahead salvo's hit, or none told: not a pattern's)
					const SeaShow::Event& D = *Drop->second;
					double HerX = 0.0, HerY = 0.0;
					const bool bOnHer = D.bAt && HerPlace(D.On, D.T, HerX, HerY) && std::hypot(D.X - HerX, D.Y - HerY) <= Rec.Board.ChargeLethal;
					const double Bursts = D.T + (D.Sinking >= 0.0 ? D.Sinking : (D.Set >= 0.0 ? D.Set : 100.0) / std::max(0.5, Rec.Board.ChargeSink));
					if (bOnHer && std::fabs(E.T - Bursts) <= 3.0) ++BlowsAtPattern;
					else ++BlowsElse;
				}
		}
		const bool bLaysOnHer = BlowsAtPattern > 0 && BlowsElse == 0;
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
			Live.SetMine(MineOf(Rec)); // (the scene's show: S.Show.SetMine)
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
			int Dry = 0, InHull = 0, InOther = 0, InFace = 0, Jumps = 0, OverBudget = 0, Fast = 0, Unheld = 0, GunIn = 0, HerHidden = 0, HerUp = 0;
			int EarlyAir = 0, AirOffHer = 0, AirFrames = 0, AfterEnd = 0, AheadStruck = 0, AheadHigh = 0;
			std::map<size_t, bool> OpenedHidden, FoundIn; // (each window: the show hid her as it opened; it drew her in it)
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
					// (as FSeaUnder::EyeFor: posed beside her only while the show draws her, PoseOf, and her crew are below)
					BoatPose Pose = Boat ? PoseOf(*Boat) : BoatPose();
					Pose.bThere = Pose.bThere && Boat != nullptr && !CrewUp(*Boat, T);
					if (T <= W.Until)
					{
						const bool bDrawn = Boat && Boat->bShown;
						if (!OpenedHidden.count(K)) OpenedHidden[K] = !bDrawn;
						FoundIn[K] = FoundIn[K] || bDrawn;
					}
					EyePose Eye;
					if (!EyeOf(W, Pose, Hulls, T, Eye)) continue;
					// (a clean cut through the face: never an eye half in it)
					InFace += Eye.Eye.Z > -1.5 + 1e-9 && Eye.Eye.Z < 3.0 - 1e-9 ? 1 : 0;
					if (T > W.Until) continue;
					++FramesIn[K];
					Dry += Eye.Eye.Z > -1.5 + 1e-9 || !Eye.bUnder ? 1 : 0;
					// (the boat beside the eye drawn by the show every frame it is under, her crew below: never one it hides)
					const bool bAfloat = Boat && Boat->Doing != SeaShow::Fate::Gone;
					HerHidden += bAfloat && !Boat->bShown ? 1 : 0;
					HerUp += bAfloat && CrewUp(*Boat, T) ? 1 : 0;
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
					// (no boat the show hides in the picture, near enough to be made out, nor one at her deck gun on the surface)
					for (const SeaShow::ShipOut& Ship : Live.Ships())
					{
						if (Ship.Class != "ss" || Ship.Doing == SeaShow::Fate::Gone) continue;
						const bool bHidden = !Ship.bShown, bAtGun = CrewUp(Ship, T) && Ship.Struck < 0.0;
						if (!bHidden && !bAtGun) continue;
						const P3 Ahead = ConvoyLook::AheadOf(Ship.Heading);
						const P3 Middle(Ship.At.X, Ship.At.Y, -MiddleOf(std::max(0.0, Ship.Keel)));
						for (int J = 0; J <= 4; ++J)
						{
							const P3 Point = Middle + Ahead * (Ship.Long * (J / 4.0 - 0.5));
							if (InPicture(Eye, Point) && Gap(Eye.Eye, Point) <= M.See)
							{
								Unheld += bHidden ? 1 : 0;
								GunIn += bAtGun ? 1 : 0;
								break;
							}
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
			Check(How + ": no boat the show hides in the picture (its own bShown, the surface's rule)", Unheld == 0, std::to_string(Unheld) + " frames");
			int OpenedBad = 0;
			for (size_t K = 0; K < N; ++K) OpenedBad += OpenedHidden.count(K) && OpenedHidden[K] && !FoundIn[K] ? 1 : 0;
			Check(How + ": the boat beside the eye drawn by the show every frame under, her crew below (never struck and up, nor at her gun)", HerHidden == 0 && HerUp == 0,
			      std::to_string(HerHidden) + " frames hidden, " + std::to_string(HerUp) + " with her crew up");
			Check(How + ": no window opens on a boat the show hides unless the show finds her within it (the scene's frames)", OpenedBad == 0,
			      std::to_string(OpenedBad) + Of + " opened on her hidden and never found");
			Check(How + ": no boat at her deck gun on the surface in the picture from under the water", GunIn == 0, std::to_string(GunIn) + " frames");
			Check(How + ": the eye moves smoothly (1.5 m a frame at the most)", Jumps == 0, std::to_string(Jumps) + " jumps");
			std::string Seen;
			for (size_t K = 0; K < N; ++K) Seen += (Seen.empty() ? "" : ", ") + std::to_string(SeenSinking[K].size());
			Check(How + ": a pattern's charges seen sinking past her (3 at the least in the picture)", PatternsSeen == Patterns, std::to_string(PatternsSeen) + " of " + std::to_string(Patterns) + " (" + Seen + ")");
			Check(How + ": each charge drawn where it bursts (1.5 m)", Bursts.Off == 0, std::to_string(Bursts.Met) + " bursts, worst " + Num(Bursts.WorstOff, 2) + " m");
			Check(How + ": each booked blow with a charge drawn within twice its reach of her keel bursts within that reach of a charge drawn the frame before", Bursts.BookedOff == 0,
			      std::to_string(Bursts.BookedNear) + " such, worst " + Num(Bursts.WorstBooked, 2) + " m");
			if (bLaysOnHer)
				Check(How + ": on a night whose every booked blow comes as its pattern, laid on her, bursts at her, each bursts by a charge drawn within twice its reach of her keel (none laid off her)",
				      Bursts.LaidOff == 0, std::to_string(Bursts.LaidOff) + " laid off her" + (Bursts.LaidOff ? ", the nearest charge to her as far as " + Num(Bursts.FarthestLaid, 1) + " m" : std::string()) +
				                               " (" + std::to_string(BlowsAtPattern) + " blows booked as their patterns burst at her)",
				      Bursts.FarthestLaid); // (its measure, for the known list: how far off her the nearest charge lay, m)
			else if (Bursts.LaidOff > 0)
				Note(How + ": booked blows with no charge of the escort's drawn within twice its reach of her keel (the pattern laid off her: the show's boat and drop places, an ask), burst by her",
				     std::to_string(Bursts.LaidOff) + ", the nearest charge to her as far as " + Num(Bursts.FarthestLaid, 1) + " m (" + std::to_string(BlowsElse) +
				         " blows the battle booked off a pattern's own burst at her)");
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
		// THE PLAYER'S OWN DIVE (U), as FSeaUnder::PressU, Tick and EyeFor play it, against the scene's own show (SetMine: whom
		// it draws, BoatsOf): every 15 s U pressed, up if down, else with the eye over each boat in turn (300 m south of her,
		// 900 m up) down beside the boat HandBoatFor gives; the dive ended as soon as she no longer holds (HandHolds).
		{
			SeaShow::Show Live(Rec.Seed ? Rec.Seed : 239);
			Live.SetField(Rec.Board);
			Live.SetMine(MineOf(Rec)); // (the scene's show: S.Show.SetMine)
			Water Sea;
			Sea.Begin(Live.Board());
			size_t Taken = 0;
			auto TakeNext = [&]()
			{
				const SeaFeed::Frame& V = Rec.Views[Taken++];
				Live.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
			};
			TakeNext();
			const double ClearBy = UnheldClearBy();
			std::vector<SeaShow::Show::ChargeDown> Down;
			std::vector<BoatAt> Afloat;
			std::vector<Hull> Hulls;
			std::map<int, BoatNow> Moving;
			std::map<int, double> SunkAt;
			std::set<int> Beside;
			Frame F;
			int HandBoat = -1, HandSide = 1, Presses = 0, Dives = 0, Refused = 0, LetGo = 0, LastBoat = -1, LastFr = -1;
			int DiveFrames = 0, NotHeld = 0, UnheldIn = 0, GunIn = 0, Dry = 0, InHull = 0, InOther = 0, Jumps = 0, EarlyAir = 0, OverBudget = 0;
			bool bInjected = false, bBite = false, bKeptUp = false, bGunKeptUp = false;
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
				if (HandBoat >= 0 && !HandHolds(Afloat, HandBoat, ClearBy))
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
						const int Chosen = HandBoatFor(Afloat, P3(Over.X, Over.Y - 300.0, 900.0), ClearBy);
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
				// (HandHolds above has let go of her the frame the show hides her or her crew come up: counted, then EyeFor's own
				// guard, so not one frame shows her)
				++DiveFrames;
				const bool bNotHers = !Boat->bShown || CrewUp(*Boat, T);
				NotHeld += bNotHers ? 1 : 0;
				if (bNotHers)
				{
					HandBoat = -1;
					continue;
				}
				const BoatPose Pose = PoseOf(*Boat);
				const EyePose Eye = EyeByHand(Pose, Hulls, T - HandSince, HandSide);
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
					if (Ship.Class != "ss" || Ship.Doing == SeaShow::Fate::Gone) continue;
					const bool bHidden = !Ship.bShown, bAtGun = CrewUp(Ship, T) && Ship.Struck < 0.0;
					if (!bHidden && !bAtGun) continue;
					const P3 OtherAhead = ConvoyLook::AheadOf(Ship.Heading);
					const P3 Middle(Ship.At.X, Ship.At.Y, -MiddleOf(std::max(0.0, Ship.Keel)));
					for (int J = 0; J <= 4; ++J)
					{
						const P3 Point = Middle + OtherAhead * (Ship.Long * (J / 4.0 - 0.5));
						if (InPicture(Eye, Point) && Gap(Eye.Eye, Point) <= M.See)
						{
							UnheldIn += bHidden ? 1 : 0;
							GunIn += bAtGun ? 1 : 0;
							break;
						}
					}
				}
				if (!bInjected && Boat->Side != Mine)
				{
					// A pack-mate the escorts have not found (the show hides her), 30 m beyond her from the eye at her depth, put
					// into the boats the rule reads (the show left as it is): in the frame within the murk were the eye let go
					// under, so the dive beside her is refused and one already going is ended. So too for the same boat drawn,
					// at her deck gun on the surface.
					bInjected = true;
					const double Across = std::hypot(Pose.At.X - Eye.Eye.X, Pose.At.Y - Eye.Eye.Y);
					const P3 Beyond = Across > 1e-6 ? Pose.At + P3(Pose.At.X - Eye.Eye.X, Pose.At.Y - Eye.Eye.Y, 0.0) * (30.0 / Across) : Pose.At;
					std::vector<BoatAt> Packed = Afloat, Gunned = Afloat;
					Packed.push_back(BoatAt{ 1 << 20, 1 << 20, Boat->Side, Beyond.X, Beyond.Y, false, true, true, false, false });
					Gunned.push_back(BoatAt{ 1 << 20, 1 << 20, Boat->Side, Beyond.X, Beyond.Y, true, true, true, false, true });
					bBite = InPicture(Eye, Beyond) && Gap(Eye.Eye, Beyond) <= M.See;
					const P3 Overhead(Pose.At.X, Pose.At.Y - 300.0, 900.0);
					bKeptUp = HandBoatFor(Packed, Overhead, ClearBy) != HandBoat && !HandHolds(Packed, HandBoat, ClearBy);
					bGunKeptUp = HandBoatFor(Gunned, Overhead, ClearBy) != HandBoat && !HandHolds(Gunned, HandBoat, ClearBy);
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
			Check("by hand: every boat gone under beside drawn by the show (his own, or found), her crew below, all through the dive", NotHeld == 0, std::to_string(NotHeld) + " frames not");
			Check("by hand: no boat the show hides in the picture", UnheldIn == 0, std::to_string(UnheldIn) + " frames");
			Check("by hand: no boat at her deck gun on the surface in the picture", GunIn == 0, std::to_string(GunIn) + " frames");
			Check("by hand: the eye under the sea (1.5 m), out of her hull (her half beam and 6 m) and every other", Dry == 0 && InHull == 0 && InOther == 0,
			      std::to_string(Dry) + " frames above, " + std::to_string(InHull) + " in her, " + std::to_string(InOther) + " in another");
			Check("by hand: the eye moves smoothly (1.5 m a frame at the most)", Jumps == 0, std::to_string(Jumps) + " jumps");
			Check("by hand: her air only after her own end, within the budgets", EarlyAir == 0 && OverBudget == 0, std::to_string(EarlyAir) + " frames early, " + std::to_string(OverBudget) + " over");
			if (Path.find("convoy-night") != std::string::npos)
			{
				Check("by hand: an unfound pack-mate put 30 m beyond a held boat, in the frame were the eye let under, keeps it up", bInjected && bBite && bKeptUp,
				      std::string(bInjected ? "put in" : "never put in") + (bBite ? ", in the frame" : ", not in the frame") + (bKeptUp ? ", kept up" : ", gone under"));
				Check("by hand: a pack-mate at her deck gun on the surface put there instead keeps it up too", bInjected && bBite && bGunKeptUp,
				      std::string(bInjected ? "put in" : "never put in") + (bGunKeptUp ? ", kept up" : ", gone under"));
			}
		}
	}
	Night.clear();
	// (a line for a night played that no check of its name ran on: its check gone or renamed, to be taken off or mended)
	for (const auto& Line : Known)
		if (Played.count(Line.first.first) && !Ran.count(Line.first))
			std::printf("  note  listed as known on %s, but no check of that name ran there (take its line off the list, or mend its name): %s\n",
			            Line.first.first.c_str(), Line.first.second.c_str());
	std::printf("%d checks, %d failed", Checks, Failed);
	if (KnownFailed) std::printf(", %d known (the rules' side, each within its bound)", KnownFailed);
	std::printf("\n");
	if (Failed) std::printf("SOME CHECKS FAILED\n");
	else if (KnownFailed) std::printf("ALL CHECKS PASSED (%d known failures, each within its bound)\n", KnownFailed);
	else std::printf("ALL CHECKS PASSED\n");
	return Failed ? 1 : 0;
}
