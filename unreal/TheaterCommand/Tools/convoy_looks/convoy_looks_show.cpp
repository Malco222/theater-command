// The submarine scene's looks against the show itself: a recorded convoy night (the sea show's stand-in, read only)
// played through the game's own Battle/SeaShow at 30 frames a second, and the looks' core (Battle/ConvoyLooksCore) fed
// from it as the scene feeds it. Checks that what the looks draw agrees with what the show does: every boat, as the
// show steers her, never pops where she can be seen, rides surfaced where the show has her, and shows of the model the
// stage draws (the kit's code-built submarine, typed here from its code, apart from the looks' own rows) only what her
// state says: at periscope depth the head of her taller periscope while the show has it up and nothing while it is
// down, nothing deep, the swirl as the last of her bridge goes under and feathers only with all of it under; every
// pattern's charges go in where the show bursts them, and before; the ahead thrower's ring is in before its bursts; a
// starshell lights and sinks. And against the sea scene's wreck model (the men in the water, every ship's, a boat's too:
// the show's, which the looks never draw): a boat's men go over her side from her deck as the looks ride her (blown, high,
// once forced up), and every man climbing a rescuer's side climbs a net the looks have down there, hung while she stands
// by as the scene has it and in once she is under way with no one on it. Plain C++, no engine. Usage:
// Tools/convoy_looks/run.sh (it runs this after the core's).

#include "Battle/AirFeed.h"
#include "Battle/ConvoyLooksCore.h"
#include "Battle/DeckCrewCore.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using namespace ConvoyLook;

	int ShowFailures = 0, ShowChecks = 0;

	void ShowCheck(bool bOk, const std::string& What)
	{
		++ShowChecks;
		if (!bOk)
		{
			++ShowFailures;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	std::string Said(const char* Format, double A, double B = 0.0, double C = 0.0)
	{
		char Line[256];
		std::snprintf(Line, sizeof(Line), Format, A, B, C);
		return Line;
	}

	/**
	 * The looks each of the convoy battle's real nights must tell (the design director's ruling of 03:40Z, 26 November, on
	 * convoy_looks: each look proved on the night that tells it, every look told at least once on a real night and drawn
	 * every time it is told), read from the table run.sh names (CONVOY_LOOKS_NIGHTS: Tools/convoy_looks/night_looks.txt; a
	 * line each, tab apart: the recording's file name without .json, as the known lists name it, and the looks it must tell,
	 * space apart, of "pattern" (a depth-charge pattern dropped: its charges bursting), "ahead" (an ahead-thrown salvo with a
	 * hit booked from it: its bombs bursting on her) and "starshell"). A night in the table is a real night: it must tell
	 * every look of its line, and every look it tells, of its line or not, is drawn every time it is told; across the real
	 * nights played each look is told on one at the least. A real night is also any recording played from the real nights'
	 * folder run.sh names (CONVOY_LOOKS_REAL: Tools/sea_show/recordings), in the table or not: one there with no line in the
	 * table fails, its looks are still each drawn every time told, and it counts across the real nights; so the table
	 * emptied or cut short never stops the real nights being asked. A recording neither in the table nor in that folder (a
	 * stand-in) is checked as before (the night's own check, the looks told at least one and each told drawn). The table not
	 * named, not read, or with a line that is not a night and its looks, or the folder not named, fails the run.
	 */
	const char* const LookNames[] = { "pattern", "ahead", "starshell" };
	std::map<std::string, std::set<std::string>> NightLooks;
	bool bNightLooksRead = false;
	std::string NightLooksWhy;
	// (the real nights' folder, as run.sh names it: empty, not named)
	std::string RealFolder;
	// (the real nights played, and on which of them each look was told)
	std::set<std::string> RealPlayed;
	std::map<std::string, std::set<std::string>> ToldOnReal;

	void ReadNightLooks()
	{
		const char* TablePath = std::getenv("CONVOY_LOOKS_NIGHTS");
		if (!TablePath || !*TablePath)
		{
			NightLooksWhy = "not named";
			return;
		}
		std::ifstream TableIn(TablePath);
		if (!TableIn)
		{
			NightLooksWhy = std::string("cannot read ") + TablePath;
			return;
		}
		std::string Row;
		int Line = 0;
		while (std::getline(TableIn, Row))
		{
			++Line;
			if (!Row.empty() && Row.back() == '\r') Row.pop_back();
			if (Row.empty() || Row[0] == '#') continue;
			const size_t Tab = Row.find('\t');
			const std::string Night = Tab == std::string::npos ? std::string() : Row.substr(0, Tab);
			std::set<std::string> Looks;
			std::string Odd;
			if (Tab != std::string::npos)
			{
				std::istringstream Words(Row.substr(Tab + 1));
				std::string Word;
				while (Words >> Word)
				{
					bool bKnown = false;
					for (const char* Name : LookNames) bKnown = bKnown || Word == Name;
					if (!bKnown) Odd = Word;
					Looks.insert(Word);
				}
			}
			if (Night.empty() || Night.find_first_of(" \t") != std::string::npos || Looks.empty() || !Odd.empty() || NightLooks.count(Night))
			{
				NightLooksWhy = "line " + std::to_string(Line) + " is not a night and its looks" + (Odd.empty() ? std::string() : " (no look \"" + Odd + "\")") +
				                (NightLooks.count(Night) ? " (" + Night + " twice)" : std::string()) + ": " + Row;
				NightLooks.clear();
				return;
			}
			NightLooks[Night] = Looks;
		}
		bNightLooksRead = true;
	}

	/** Whether a recording lies in the real nights' folder (CONVOY_LOOKS_REAL), the same folder however its path is spelt. */
	bool InRealFolder(const std::string& Path)
	{
		if (RealFolder.empty()) return false;
		std::error_code Err;
		const std::filesystem::path Folder = std::filesystem::weakly_canonical(RealFolder, Err);
		if (Err) return false;
		const std::filesystem::path File = std::filesystem::weakly_canonical(Path, Err);
		return !Err && File.parent_path() == Folder;
	}

	/** The night as the table names it: the recording's file name without .json. */
	std::string NightOf(const std::string& Path)
	{
		const size_t Slash = Path.find_last_of("/\\");
		std::string Night = Path.substr(Slash == std::string::npos ? 0 : Slash + 1);
		if (Night.size() > 5 && Night.compare(Night.size() - 5, 5, ".json") == 0) Night.resize(Night.size() - 5);
		return Night;
	}

	/** A charge as the looks drew it going in: its escort, when it lands, where, whether the ahead thrower's (its ring's
	 * middle), and the pattern's sinking time. */
	struct Landing
	{
		int Key = 0;
		double Lands = 0.0;
		P3 To, Middle;
		bool bAhead = false;
		double Sinking = 0.0, Ring = 0.0;
	};

	/**
	 * The boat as the stage draws her, steered with the looks' heave and the show's trim (plus bow up, about her waterline
	 * amidships). Heights over the still sea: her casing amidships, the bridge's top at both ends of its plan and the
	 * jumping wire's head on it, the periscopes' heads. Two models:
	 * - the code-built one, typed from Campaign/CampaignShips.cpp's Submarine() (built by her navy: 67 m by 6.2 for "GER"
	 *   or none, 95 by 8.3 for "USA", 80 by 6.2 for any other; the stem's rake 2 m forward of the hull), stretched by
	 *   AirStage's FitHull to AirFeed::HullOf's size: deck 1.65, the bridge's top 6.25 from 5.4 forward to 4.6 aft, the
	 *   wire's head 5.0 forward at 6.5, fixed periscopes' heads 1.5 forward at 10.65 and amidships at 9.15;
	 * - the battle modeller's (Content/Models/Ships/Submarine_GER, _USA, or Submarine for any other navy), drawn as made,
	 *   typed from the files (as Tools/battle_ships_in.py leaves them, the attack periscope raised), which the stage draws
	 *   wherever the game has them: Sculpted().
	 */
	struct StageBoat
	{
		double Up = 1.0, Along = 1.0;
		double Deck = 1.65, Top = 6.25, Fore = 5.4, Aft = -4.6, WireAlong = 5.0, WireTop = 6.5, ScopeAlong = 1.5, ScopeTop = 10.65, LowAlong = 0.0, LowTop = 9.15;

		StageBoat(const std::string& Nation, double FitLong, double FitBeam)
		{
			const double Hull = Nation == "USA" ? 95.0 : (Nation.empty() || Nation == "GER") ? 67.0 : 80.0, Beam = Nation == "USA" ? 8.3 : 6.2;
			Up = FitBeam / Beam;
			Along = FitLong / (Hull + 2.0);
		}
		static StageBoat Sculpted(const std::string& Nation)
		{
			StageBoat B(std::string(), 69.0, 6.2);
			B.Up = 1.0;
			B.Along = 1.0;
			if (Nation == "GER")
			{
				B.Deck = 1.77; B.Top = 5.3; B.Fore = 3.5; B.Aft = -6.5; B.WireAlong = 0.0; B.WireTop = 5.94;
				B.ScopeAlong = -0.2; B.ScopeTop = 10.49; B.LowAlong = -1.2; B.LowTop = 5.79;
			}
			else if (Nation == "USA")
			{
				B.Deck = 2.6; B.Top = 6.5; B.Fore = 6.5; B.Aft = -5.0; B.WireAlong = 2.4; B.WireTop = 9.04;
				B.ScopeAlong = 2.4; B.ScopeTop = 11.79; B.LowAlong = 1.4; B.LowTop = 7.09;
			}
			else
			{
				B.Deck = 2.0; B.Top = 5.6; B.Fore = 4.5; B.Aft = -7.5; B.WireAlong = 0.4; B.WireTop = 6.44;
				B.ScopeAlong = 0.4; B.ScopeTop = 10.99; B.LowAlong = -0.6; B.LowTop = 6.29;
			}
			return B;
		}
		double At(double Heave, double Pitch, double Off, double High) const
		{
			const double R = Pitch * 3.14159265358979323846 / 180.0;
			return Heave + High * Up * std::cos(R) + Off * Along * std::sin(R);
		}
		double Bridge(double Heave, double Pitch) const { return std::max({ At(Heave, Pitch, Fore, Top), At(Heave, Pitch, Aft, Top), At(Heave, Pitch, WireAlong, WireTop) }); }
		double Scope(double Heave, double Pitch) const { return At(Heave, Pitch, ScopeAlong, ScopeTop); }
		double Low(double Heave, double Pitch) const { return At(Heave, Pitch, LowAlong, LowTop); }
	};

	struct BoatSeen
	{
		BoatState State;
		BoatModel Model;
		Drawn Size;
		StageBoat Stage{ std::string(), 95.0, 8.3 };
		bool bSeen = false;
		int Piece = -1;
		double LastHeave = 0.0, WorstPop = 0.0, WorstOff = 0.0, PopAt = 0.0, OffAt = 0.0;
		// (what of the stage's model shows, against her state: frames at periscope depth with the show's periscope up and
		// settled, how many show its head out, the least it shows, the most of her shorter periscope and her bridge; with it
		// down and settled, the most of her highest point; deep, the same)
		int UpFrames = 0, UpShown = 0, DownFrames = 0, DeepFrames = 0;
		double UpLeast = 99.0, UpLowMost = -99.0, UpBridgeMost = -99.0, DownMost = -99.0, DeepMost = -99.0;
		double ScopeSince = -1.0, DownSince = -1.0;
		int Swirls = 0, Feathers = 0;
		double SwirlLeast = 99.0, SwirlMost = -99.0, FeatherBridgeMost = -99.0;
		// (her deck as the looks ride her, over her own waterline: the swell's heave taken off, as the scene's men's heights)
		double Deck = 0.0, Down = 0.0;
		bool bNow = false;
		int HighFrames = 0;
		// (struck: since when, as the show has her up (below 0: not), how she ends, and how the looks ride her this frame: her
		// drawn heave, her trim and heel, where and which way; her vents' spurts; scuttled told)
		double Struck = -1.0, Heave = 0.0, Pitch = 0.0, Heel = 0.0, Heading = 0.0;
		P3 At;
		int Vents = 0;
		bool bStruck = false, bScuttled = false;
		// (sent down: when (the show's Sink), and whether by the stern as the stage takes her (SeaShow::StageEndOf); the last
		// moment a man was on her or in her hatch; her scuttling charges' moments, and their puffs)
		double SunkAt = -1.0, LastHandAt = -1.0;
		bool bByStern = false;
		std::vector<double> ChargesAt;
		int Puffs = 0;
	};

	/** The show's casing rows (SeaShow::DeckOf, where a struck boat's men stand) against the looks' (SculptedOf, which
	 * Tools/convoy_looks/convoy_looks_test holds to the model files): the same boat, the same casing, her hatches on its
	 * stretches and her tower inside her beam. */
	void CheckDeckRows()
	{
		const struct
		{
			const char* Nation;
			const char* File;
		} Navies[] = { { "GER", "Submarine_GER" }, { "", "Submarine" }, { "USA", "Submarine_USA" }, { "ITA", "Submarine" }, { "JPN", "Submarine" } };
		auto Same = [](double A, double B) { return std::fabs(A - B) < 1e-9; };
		for (const auto& Navy : Navies)
		{
			const SeaShow::BoatDeck D = SeaShow::DeckOf(Navy.Nation);
			const BoatModel M = SculptedOf(Navy.File);
			const bool bSame = Same(D.Long, M.Long) && Same(D.Wide, M.Wide) && Same(D.TowerFore, M.TowerFore) && Same(D.TowerAft, M.TowerAft) && Same(D.Bridge, M.TowerTop - 1.2) &&
			                   Same(D.ForeFrom, M.ForeFrom) && Same(D.ForeTo, M.ForeTo) && Same(D.AftFrom, M.AftFrom) && Same(D.AftTo, M.AftTo) && Same(D.Edge, M.Edge) &&
			                   Same(D.Deck, M.Casing) && Same(D.RiseFrom, M.RiseFrom) && Same(D.Rise, M.Rise) && Same(D.Fall, M.Fall) && Same(D.GunFrom, M.GunFrom) && Same(D.GunTo, M.GunTo) &&
			                   Same(D.ForeHatch, M.ForeHatch) && Same(D.AftHatch, M.AftHatch) && Same(D.Top, M.Top);
			const std::string Named = std::string(*Navy.Nation ? Navy.Nation : "no navy");
			ShowCheck(bSame, "the show's casing row for " + Named + " is the looks' " + Navy.File + "'s (her men stand on her as she is drawn)");
			ShowCheck(D.ForeHatch >= D.ForeFrom && D.ForeHatch <= D.ForeTo && D.AftHatch >= D.AftFrom && D.AftHatch <= D.AftTo && D.TowerHalf < 0.5 * D.Wide,
			          "and its hatches open on the stretches its men stand on, its tower inside its beam (" + Named + ")");
			for (const double Along : { D.AftFrom, D.AftTo, D.ForeFrom, D.ForeTo })
				for (const double Across : { 0.0, 0.85, D.Edge })
					ShowCheck(Same(SeaShow::CasingAt(D, Along, Across), CasingAt(M, Along, Across)), "and the show's casing height is the looks' (" + Named + ")");
		}
		// The stage's sinking as the show has a struck boat's crew ride her down (SeaShow::StageSink) and as the looks throw
		// her vents and charges from her (StageSinking, which Tools/convoy_looks/convoy_looks_test holds to the stage's lines).
		bool bSameSink = true;
		for (const bool bByStern : { true, false })
		{
			for (double T = -1.0; T < 90.0; T += 0.37)
			{
				double ShowDepth = 0.0, ShowPitch = 0.0, LookDepth = 0.0, LookPitch = 0.0;
				SeaShow::StageSink(T, 10.49, bByStern, ShowDepth, ShowPitch);
				StageSinking(T, 10.49, bByStern, LookDepth, LookPitch);
				bSameSink = bSameSink && Same(ShowDepth, LookDepth) && Same(ShowPitch, LookPitch);
			}
		}
		ShowCheck(bSameSink, "the show's stage sinking (a struck boat's crew riding her down) is the looks' (her vents and charges thrown from her)");
		ShowCheck(SeaShow::StageEndOf("scuttled", 3.0, -1.0) == "stern" && SeaShow::StageEndOf("scuttled", -1.0, 0.5) == "stern" && SeaShow::StageEndOf("scuttled", -1.0, -1.0).empty() &&
		              SeaShow::StageEndOf("crushed", 3.0, -1.0).empty(),
		          "the stage takes a boat that struck and was scuttled down by the stern, any other by her look's own end");
	}

	/** A rescuer as the looks keep her (FConvoyLooks::FRescue): until when, her nets. */
	struct RescueSeen
	{
		double Until = 0.0, SteppedAt = -1.0, AwaySince = -1.0;
		std::vector<NetHung> Nets;
	};

	int PlayNight(const std::string& Path)
	{
		std::ifstream File(Path);
		if (!File)
		{
			std::printf("cannot read %s\n", Path.c_str());
			return 2;
		}
		std::stringstream Text;
		Text << File.rdbuf();
		SeaFeed::Recording Rec;
		std::string Error;
		if (!SeaFeed::ReadRecording(Text.str(), Rec, Error) || Rec.Views.empty())
		{
			std::printf("%s: %s\n", Path.c_str(), Error.c_str());
			return 2;
		}
		SeaShow::Show Night(Rec.Seed ? Rec.Seed : 239);
		Night.SetField(Rec.Board);
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& V = Rec.Views[Taken++];
			Night.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
		};
		TakeNext();
		if (Taken < Rec.Views.size()) TakeNext();

		const SeaShow::Field& Board = Night.Board();
		const double Sky = SkyLit(Board.bLight, Board.SunUp, Board.MoonUp, Board.MoonLit);
		// What the battle tells of the night, read from the recording, not the show (the checks below of what a night holds ask
		// for each where it is told: a stand-in was made with all of it, a real night has what its rules and its kit gave it):
		// the rescues it tells (sea:rescue: the naval rules send one only on the player's order, so a night fought by the
		// computer on both sides has none); the seconds each boat is told at periscope depth with her periscope up (her dive
		// "periscope", her scope up, her keel within half a metre of it or not told; the convoy field's crash dive passes
		// through it in a view); a pattern dropped, an ahead-thrown salvo with a hit booked from the same escort within 60 s
		// (the show bursts that salvo's bombs only on such a hit), and a starshell.
		int RescuesTold = 0;
		bool bPatternTold = false, bAheadHitTold = false, bStarTold = false;
		std::map<int, double> ToldAtScope;
		// (and each of them one by one, for a night in the looks table, each drawn every time it is told: each pattern dropped,
		// when, by which escort's piece and where the battle laid it (its point, where it tells one); each ahead-thrown hit,
		// when and from which; each starshell, when)
		struct PatternTold
		{
			double T = 0.0;
			int Escort = -1;
			bool bPlace = false;
			double X = 0.0, Y = 0.0;
			// (the boat's piece it was laid on, the depth it was set to and its sinking time, as the battle tells them: below
			// 0, not told)
			int On = -1;
			double Set = -1.0, Sinking = -1.0;
			int N = 0; // (its charges, as the battle tells them; 0, not told)
		};
		std::vector<PatternTold> PatternsToldAt;
		std::vector<std::pair<double, int>> AheadHitsToldAt;
		std::vector<double> StarsToldAt;
		// (every depth-charge blow told: when, from which escort's piece, on which boat's; and the views' times, the battle's
		// steps, for the rules' moment a pattern bursts at)
		struct BlowTold
		{
			double T = 0.0;
			int From = -1, On = -1;
		};
		std::vector<BlowTold> BlowsToldAt;
		std::vector<double> ViewTimes;
		for (const SeaFeed::Frame& V : Rec.Views) ViewTimes.push_back(V.Head.T);
		{
			std::map<int, std::string> DiveWas;
			std::map<int, double> AheadAt;
			for (size_t I = 0; I < Rec.Views.size(); ++I)
			{
				const SeaFeed::Frame& V = Rec.Views[I];
				const double For = I + 1 < Rec.Views.size() ? Rec.Views[I + 1].Head.T - V.Head.T : 0.0;
				for (const SeaShow::Piece& P : V.Pieces)
				{
					if (P.Class != "ss") continue;
					if (!P.Dive.empty()) DiveWas[P.Id] = P.Dive;
					if (DiveWas[P.Id] == "periscope" && P.bScope && (P.Keel < 0.0 || std::fabs(P.Keel - Board.ScopeKeel) <= 0.5)) ToldAtScope[P.Id] += For;
				}
				for (const SeaShow::Event& E : V.Events)
				{
					RescuesTold += E.What == "sea:rescue" ? 1 : 0;
					bStarTold = bStarTold || E.What == "sea:starshell";
					if (E.What == "sea:starshell") StarsToldAt.push_back(E.T);
					if (E.What == "sea:depthCharge")
					{
						if (E.Kind == "ahead") AheadAt[E.Unit] = E.T;
						else
						{
							bPatternTold = true;
							PatternsToldAt.push_back({ E.T, E.Unit, E.bAim || E.bAt, E.bAim ? E.AimX : E.X, E.bAim ? E.AimY : E.Y, E.On, E.Set, E.Sinking, E.N });
						}
					}
					if (E.What == "sea:depthChargeHit")
					{
						BlowsToldAt.push_back({ E.T, E.From, E.Unit });
						const auto Thrown = AheadAt.find(E.From);
						const bool bAheadHit = Thrown != AheadAt.end() && E.T - Thrown->second <= 60.0;
						bAheadHitTold = bAheadHitTold || bAheadHit;
						if (bAheadHit) AheadHitsToldAt.push_back({ E.T, E.From });
					}
				}
			}
		}
		std::map<int, BoatSeen> Boats;
		std::map<int, RescueSeen> Rescues;
		std::map<int, bool> Jumped, Climbed;
		int Jumpers = 0, ClimbFrames = 0, ClimbOn = 0, ClimbersOn = 0, Climbers = 0, ClimbUnnamed = 0, NetsAway = 0, MostNets = 0, ClimbOver = 0;
		double JumpWorst = 0.0, JumpRawWorst = 0.0, ClimbHeaveLow = 99.0, ClimbHeaveHigh = -99.0, StoodBy = 0.0, ClimbOffDrawnLow = 99.0, ClimbOffDrawnHigh = -99.0;
		std::vector<Landing> Landings;
		int Drops = 0, PatternBursts = 0, AheadBursts = 0, Stars = 0;
		// (the looks drawn one by one, for the looks table: each pattern's drop (when, its escort's piece and own number, its
		// charges, their run and their sinking time, and its run's middle, the Fx's Aim), each charge's burst (when, its escort's piece and own number, an ahead
		// bomb's or not, when it was due as laid) and each starshell (when))
		struct DropDrawn
		{
			double T = 0.0;
			int Piece = -1, Key = 0, N = 0;
			double Run = 0.0, Sinking = 0.0;
			double MidX = 0.0, MidY = 0.0;
		};
		struct BurstDrawn
		{
			double T = 0.0;
			int Piece = -1, Key = 0;
			bool bAhead = false;
			double Due = 0.0;
			// (the depth it was laid at, the Fx's Aim; and the depth it was drawn at as it burst: the view under the water's own
			// rule, going down at the charge's rate to its burst (sea_under_test's NowOf), from the show's list of charges in the
			// water the frame before, carried on to the burst's frame; +1 where it was not in that list)
			double LaidZ = 0.0, DrawnZ = 1.0;
		};
		std::vector<DropDrawn> PatternDrops;
		std::vector<BurstDrawn> BurstsDrawn;
		std::vector<SeaShow::Show::ChargeDown> DownBefore; // (the show's charges in the water, the frame before)
		std::vector<double> StarsDrawnAt;
		// (a struck boat's flag: frames it should fly, those it does not, those it streams to windward in a breeze, those any of
		// it is near the sea, those it is still up after hauled down; her men on her casing against it as the looks ride her)
		int FlagFrames = 0, FlagMissing = 0, FlagBreezy = 0, FlagWindward = 0, FlagUnder = 0, FlagStaysUp = 0, HandFrames = 0, HandsWorstKey = 0, HandsLate = 0;
		double HandsWorst = 0.0, HandsWorstAt = 0.0, HandsLateMost = 0.0;
		// (sent down, every man off her within SeaShow's HurryFor of it; her scuttling charges no sooner than ChargesFrom after
		// it, and after the last of them)
		const double HandsGoneIn = 5.0, ChargesFrom = 6.5;
		double WorstMiss = 0.0, LeastLead = 1e9, AheadOff = -1e9, AheadBeyond = -1e9;
		// (where the battle has a boat at a moment, between its views before and after: the boat piece nearest a point)
		auto BattleBoatNear = [&Rec](double At, double X, double Y, double& PX, double& PY)
		{
			size_t B = 0;
			while (B + 1 < Rec.Views.size() && Rec.Views[B].Head.T < At) ++B;
			const size_t A = B > 0 ? B - 1 : 0;
			const double TA = Rec.Views[A].Head.T, TB = Rec.Views[B].Head.T;
			const double Fr = TB > TA ? std::clamp((At - TA) / (TB - TA), 0.0, 1.0) : 0.0;
			double Best = 1e18;
			for (const SeaShow::Piece& Was : Rec.Views[A].Pieces)
			{
				if (Was.Class != "ss") continue;
				double X1 = Was.X, Y1 = Was.Y;
				for (const SeaShow::Piece& Then : Rec.Views[B].Pieces)
					if (Then.Id == Was.Id)
					{
						X1 = Was.X + (Then.X - Was.X) * Fr;
						Y1 = Was.Y + (Then.Y - Was.Y) * Fr;
					}
				const double D = std::hypot(X1 - X, Y1 - Y);
				if (D < Best)
				{
					Best = D;
					PX = X1;
					PY = Y1;
				}
			}
			return Best < 1e17;
		};
		const double Step = 1.0 / 30.0;
		double T = Rec.Views.front().Head.T;
		const double EndT = Rec.Views.back().Head.T;
		while (true)
		{
			Night.Advance(T);
			for (auto& Pair : Boats) Pair.second.bNow = false;
			for (const SeaShow::ShipOut& Ship : Night.Ships())
			{
				if (Ship.Class != "ss" || Ship.Dive.empty()) continue;
				BoatSeen& B = Boats[Ship.Key];
				B.Piece = Ship.Piece;
				if (!B.bSeen)
				{
					// (the stage draws the battle modeller's boat for her navy, as made: AirStage's ShipFileOf)
					const std::string Made = Ship.Nation == "GER" ? "Submarine_GER" : Ship.Nation == "USA" ? "Submarine_USA" : "Submarine";
					B.Model = SculptedOf(Made);
					B.Size = Drawn();
					B.Stage = StageBoat::Sculpted(Ship.Nation);
				}
				BoatIn In;
				In.T = T;
				In.Dive = Ship.Dive;
				In.Keel = Ship.Keel;
				In.Dived = Ship.Dived;
				In.bScope = Ship.bScope;
				In.Heave = Ship.Heave;
				In.Pitch = Ship.Pitch;
				In.Speed = Ship.Speed;
				In.At = P3(Ship.At.X, Ship.At.Y, 0.0);
				In.Heading = Ship.Heading;
				In.ScopeKeel = Board.ScopeKeel;
				In.Lit = Ship.Dive == "deep" ? Sky : 0.0;
				const BoatOut Out = StepBoat(B.State, In, B.Model, B.Size);
				// (the show's very first frame has her untrimmed, no keel: not hers yet, the looks begin her on the next)
				if (!(Ship.Keel > 0.0)) continue;
				const double Top = Out.Heave + B.Model.Top * B.Size.Up;
				// (a pop only counts where some of her can be seen: her top within 3 m of the surface)
				if (B.bSeen && Top > -3.0 && std::fabs(Out.Heave - B.LastHeave) > B.WorstPop)
				{
					B.WorstPop = std::fabs(Out.Heave - B.LastHeave);
					B.PopAt = T;
				}
				// (surfaced, she rides where the show has her, less her trim awash: the deck of the stage's model 0.5 m out; her
				// tanks blown, forced up or scuttled, none of it: riding high, her model's own waterline at the sea)
				const double ShowHeave = Ship.Heave - Ship.Dived;
				const double Awash = (B.Stage.Deck * B.Stage.Up - 0.5) * (1.0 - Smooth(0.0, 1.0, B.State.Blown));
				if (Ship.Dive == "surfaced" && Ship.Dived < 0.05 && std::fabs(Out.Heave - (ShowHeave - Awash)) > B.WorstOff)
				{
					B.WorstOff = std::fabs(Out.Heave - (ShowHeave - Awash));
					B.OffAt = T;
				}
				if (Ship.Dive == "surfaced" && Ship.Dived < 0.05 && B.State.Blown >= 1.0) ++B.HighFrames;
				// What of the stage's model shows: at periscope depth, settled (the periscope up or down 5 s), the head of her
				// taller periscope out while the show has it up and nothing while it is down; deep, nothing.
				const double Bridge = B.Stage.Bridge(Out.Heave, Ship.Pitch), Scope = B.Stage.Scope(Out.Heave, Ship.Pitch), Low = B.Stage.Low(Out.Heave, Ship.Pitch);
				const bool bAtScope = Ship.Dive == "periscope" && std::fabs(Ship.Keel - Board.ScopeKeel) < 0.05;
				B.ScopeSince = bAtScope && Ship.bScope ? (B.ScopeSince < 0.0 ? T : B.ScopeSince) : -1.0;
				B.DownSince = bAtScope && !Ship.bScope ? (B.DownSince < 0.0 ? T : B.DownSince) : -1.0;
				if (B.ScopeSince >= 0.0 && T - B.ScopeSince > 5.0)
				{
					++B.UpFrames;
					B.UpShown += Scope > 0.0 ? 1 : 0;
					B.UpLeast = std::min(B.UpLeast, Scope);
					B.UpLowMost = std::max(B.UpLowMost, Low);
					B.UpBridgeMost = std::max(B.UpBridgeMost, Bridge);
				}
				if (B.DownSince >= 0.0 && T - B.DownSince > 5.0)
				{
					++B.DownFrames;
					B.DownMost = std::max(B.DownMost, Scope);
				}
				if (Ship.Dive == "deep" && Ship.Keel > Board.ScopeKeel + 6.0)
				{
					++B.DeepFrames;
					B.DeepMost = std::max(B.DeepMost, std::max(Scope, Bridge));
				}
				for (const BoatSplash& Sp : Out.Splashes)
				{
					if (Sp.Kind == SplashKind::Vent) ++B.Vents;
					if (Sp.Kind == SplashKind::Scuttle) B.ChargesAt.push_back(T);
					if (Sp.Kind == SplashKind::Puff) ++B.Puffs;
					if (Sp.Kind == SplashKind::Swirl)
					{
						++B.Swirls;
						B.SwirlLeast = std::min(B.SwirlLeast, Bridge);
						B.SwirlMost = std::max(B.SwirlMost, Bridge);
					}
					else if (Sp.Kind == SplashKind::Feather)
					{
						++B.Feathers;
						B.FeatherBridgeMost = std::max(B.FeatherBridgeMost, Bridge);
					}
				}
				B.LastHeave = Out.Heave;
				B.Deck = Out.DeckZ - Ship.Heave;
				B.Down = Ship.Down;
				B.bNow = true;
				B.bSeen = true;
				B.Struck = Ship.Struck;
				B.bStruck = B.bStruck || Ship.Struck >= 0.0;
				B.Heave = Out.Heave;
				B.Pitch = Ship.Pitch;
				B.Heel = Ship.Heel;
				B.Heading = Ship.Heading;
				B.At = In.At;
				// Her white flag, as FConvoyLooks::Lay flies it: run up once the show has her up struck, streaming to leeward of
				// the air over her, over the sea, and down once she is taken or goes.
				FlagIn Flag;
				if (FlagInputsOf(B.Model, B.Size, In, Out, P3(Night.Wind().X, Night.Wind().Y, 0.0), Ship.Struck, Ship.Lowered, Ship.Key, Flag))
				{
					const FlagOut Flown = FlagAt(Flag, T);
					const bool bUp = Ship.Struck > FlagRaiseFor() && Ship.Lowered < 0.0, bDown = Ship.Lowered > FlagLowerFor();
					if (bUp)
					{
						++FlagFrames;
						FlagMissing += Flown.bShown ? 0 : 1;
					}
					if (bDown) FlagStaysUp += Flown.bShown ? 1 : 0;
					if (Flown.bShown)
					{
						double Lowest = 1e9, Streamed = 0.0;
						const double Breeze = std::hypot(Flag.Air.X, Flag.Air.Y);
						for (int J = 0; J < FlagDown; ++J)
						{
							for (int I = 0; I < FlagAlong; ++I) Lowest = std::min(Lowest, Flown.Point[J][I].Z);
							Streamed += ((Flown.Point[J][FlagAlong - 1].X - Flown.Point[J][0].X) * Flag.Air.X + (Flown.Point[J][FlagAlong - 1].Y - Flown.Point[J][0].Y) * Flag.Air.Y) /
							        std::max(1e-9, Breeze) / FlagDown;
						}
						FlagUnder += Lowest < 0.5 ? 1 : 0;
						if (bUp && Breeze > 3.0)
						{
							++FlagBreezy;
							FlagWindward += Streamed < 0.5 * FlagFly() ? 1 : 0;
						}
					}
				}
			}
			// A struck boat's crew on her casing (the show's Hands): standing, walking and running on it as the looks ride her
			// (her drawn heave, her casing's height there with her trim and heel), and once she is sent down as the stage takes
			// her down (its sinking, by the stern where she was scuttled); none of them, nor of her captor's party, on her or in
			// her hatches more than HandsGoneIn after it.
			for (const SeaShow::ManOut& Man : Night.Hands())
			{
				if (Man.Doing != SeaShow::Swim::Boat)
				{
					for (auto& [Key, B] : Boats)
					{
						if (!B.bNow || B.Struck < 0.0) continue;
						const double H = B.Heading * 3.14159265358979323846 / 180.0;
						const double Dx = Man.At.X - B.At.X, Dy = Man.At.Y - B.At.Y;
						if (std::fabs(Dx * std::sin(H) + Dy * std::cos(H)) > 0.5 * B.Model.Long || std::fabs(Dx * std::cos(H) - Dy * std::sin(H)) > 0.5 * B.Model.Wide + 1.5) continue;
						B.LastHandAt = T;
						if (B.SunkAt >= 0.0 && T - B.SunkAt > HandsGoneIn + 0.5 * Step)
						{
							++HandsLate;
							HandsLateMost = std::max(HandsLateMost, T - B.SunkAt);
						}
						break;
					}
				}
				if (Man.Doing != SeaShow::Swim::Stand && Man.Doing != SeaShow::Swim::Walk && Man.Doing != SeaShow::Swim::HandsUp && Man.Doing != SeaShow::Swim::Run) continue;
				for (const auto& [Key, B] : Boats)
				{
					if (!B.bNow || B.Struck < 0.0) continue;
					const double H = B.Heading * 3.14159265358979323846 / 180.0;
					const double Dx = Man.At.X - B.At.X, Dy = Man.At.Y - B.At.Y;
					const double Along = Dx * std::sin(H) + Dy * std::cos(H), Across = Dx * std::cos(H) - Dy * std::sin(H);
					if (std::fabs(Along) > 0.5 * B.Model.Long || std::fabs(Across) > 0.5 * B.Model.Wide) continue;
					double StageDown = 0.0, StageTrim = 0.0;
					if (B.SunkAt >= 0.0) StageSinking(T - B.SunkAt, B.Model.Top * B.Size.Up, B.bByStern, StageDown, StageTrim);
					const double Trim = (B.Pitch + StageTrim) * 3.14159265358979323846 / 180.0, Heel = B.Heel * 3.14159265358979323846 / 180.0;
					const double Casing = B.Heave - StageDown + CasingAt(B.Model, Along, Across) * (1.0 - 0.8 * B.Down) * std::cos(Trim) + Along * std::sin(Trim) - Across * std::sin(Heel);
					++HandFrames;
					if (std::fabs(Man.At.Z - Casing) > HandsWorst)
					{
						HandsWorst = std::fabs(Man.At.Z - Casing);
						HandsWorstAt = T;
						HandsWorstKey = Key;
					}
					break;
				}
			}
			// The sea scene's men going over a boat's side: from her deck as the looks ride her (the scene lowers its height as
			// she goes, by four fifths of her going: taken off here, so it is her deck's height the scene had).
			for (const SeaShow::ManOut& Man : Night.Men())
			{
				if (Man.Doing != SeaShow::Swim::Jump || Jumped.count(Man.Key)) continue;
				Jumped[Man.Key] = true;
				const auto It = Boats.find(Man.Ship);
				if (It == Boats.end() || !It->second.bNow) continue;
				++Jumpers;
				const double Free = Man.Drop / std::max(0.2, 1.0 - 0.8 * It->second.Down);
				JumpWorst = std::max(JumpWorst, std::fabs(Free - It->second.Deck));
				JumpRawWorst = std::max(JumpRawWorst, std::fabs(Man.Drop - It->second.Deck));
			}
			// Each rescuer's nets, stepped as the scene steps the looks (before its effects), from the scene's men near her.
			for (auto It = Rescues.begin(); It != Rescues.end();)
			{
				RescueSeen& R = It->second;
				const SeaShow::ShipOut* Ship = Night.Ship(It->first);
				const bool bRescue = T <= R.Until;
				if (!Ship || (!bRescue && R.Nets.empty()))
				{
					It = Rescues.erase(It);
					continue;
				}
				RescuerIn In;
				In.At = P3(Ship->At.X, Ship->At.Y, 0.0);
				In.Heading = Ship->Heading;
				In.Long = Ship->Long;
				In.Beam = Ship->Beam;
				In.Speed = Ship->Speed;
				std::vector<ManIn> Near;
				for (const SeaShow::ManOut& Man : Night.Men())
				{
					if (Man.Doing != SeaShow::Swim::Swim && Man.Doing != SeaShow::Swim::Climb) continue;
					ManIn& M = Near.emplace_back();
					M.At = P3(Man.At.X, Man.At.Y, Man.At.Z);
					M.Heading = Man.Heading;
					M.Doing = Man.Doing == SeaShow::Swim::Climb ? ManDoing::Climb : ManDoing::Swim;
				}
				const double Dt = R.SteppedAt >= 0.0 ? std::clamp(T - R.SteppedAt, 0.0, 10.0) : 0.0;
				R.SteppedAt = T;
				StepNets(R.Nets, In, Near, bRescue, Dt);
				MostNets = std::max(MostNets, static_cast<int>(R.Nets.size()));
				const bool bStanding = bRescue && StandsBy(Ship->Speed);
				if (bStanding) StoodBy += Step;
				// (every man the scene has climbing her side is on a net that is down; none climbs over her rail)
				bool bAnyOn = false;
				for (const SeaShow::ManOut& Man : Night.Men())
				{
					if (Man.Doing != SeaShow::Swim::Climb) continue;
					ManIn M;
					M.At = P3(Man.At.X, Man.At.Y, Man.At.Z);
					M.Doing = ManDoing::Climb;
					double Along = 0.0, Side = 0.0;
					bool bClimbing = false;
					if (!ClimbPlace(In, M, Along, Side, bClimbing)) continue;
					bAnyOn = true;
					++ClimbFrames;
					const bool bOn = OnNets(R.Nets, Side, Along, 1.0);
					ClimbOn += bOn ? 1 : 0;
					ClimbOver += Man.At.Z > FreeOf(Ship->Class) + 0.01 ? 1 : 0;
					if (!Climbed.count(Man.Key))
					{
						Climbed[Man.Key] = true;
						++Climbers;
						ClimbersOn += bOn ? 1 : 0;
					}
					// (how far out from her side as the stage draws her the scene has him: told)
					const P3 Ri = RightOf(Ship->Heading);
					const double Out = std::fabs((Man.At.X - Ship->At.X) * Ri.X + (Man.At.Y - Ship->At.Y) * Ri.Y) - 0.5 * AirFeed::HullOf(Ship->Class, std::string(), Ship->Nation).Beam;
					ClimbOffDrawnLow = std::min(ClimbOffDrawnLow, Out);
					ClimbOffDrawnHigh = std::max(ClimbOffDrawnHigh, Out);
					ClimbHeaveLow = std::min(ClimbHeaveLow, Ship->Heave);
					ClimbHeaveHigh = std::max(ClimbHeaveHigh, Ship->Heave);
				}
				// (under way, or her time over, and no one on a net: all in within the time a net takes)
				R.AwaySince = !bStanding && !bAnyOn ? (R.AwaySince < 0.0 ? T : R.AwaySince) : -1.0;
				if (R.AwaySince >= 0.0 && T - R.AwaySince > NetDropFor() + 2.0 * Step)
				{
					for (const NetHung& N : R.Nets) NetsAway += N.Out > 0.0 ? 1 : 0;
				}
				++It;
			}
			// (a man of the scene's climbing a ship no rescue named: none should be)
			for (const SeaShow::ManOut& Man : Night.Men())
			{
				if (Man.Doing != SeaShow::Swim::Climb) continue;
				bool bNamed = false;
				for (const auto& [Key, R] : Rescues)
				{
					const SeaShow::ShipOut* Ship = Night.Ship(Key);
					if (!Ship) continue;
					RescuerIn In;
					In.At = P3(Ship->At.X, Ship->At.Y, 0.0);
					In.Heading = Ship->Heading;
					In.Long = Ship->Long;
					In.Beam = Ship->Beam;
					ManIn M;
					M.At = P3(Man.At.X, Man.At.Y, Man.At.Z);
					M.Doing = ManDoing::Climb;
					double Along = 0.0, Side = 0.0;
					bool bClimbing = false;
					bNamed = bNamed || ClimbPlace(In, M, Along, Side, bClimbing);
				}
				ClimbUnnamed += bNamed ? 0 : 1;
			}
			for (const SeaShow::Fx& E : Night.Effects)
			{
				// (an escort's own number is her piece's id times 64 and her place in its line, SeaShow::ShipOut::Key; below 0, none)
				const int EscortPiece = E.Key >= 0 ? E.Key / 64 : -1;
				if (E.Kind == SeaShow::FxKind::ChargeDrop && E.What == "pattern")
					PatternDrops.push_back({ T, EscortPiece, E.Key, static_cast<int>(std::lround(E.Size)), E.Until, E.Sinking, E.Aim.X, E.Aim.Y });
				if (E.Kind == SeaShow::FxKind::ChargeBurst)
				{
					double DrawnZ = 1.0;
					for (const SeaShow::Show::ChargeDown& C : DownBefore)
						if (C.Escort == E.Key && std::hypot(C.At.X - E.Aim.X, C.At.Y - E.Aim.Y) < 0.01)
							DrawnZ = std::min(0.0, C.At.Z + std::max(0.0, C.Bursts - T) * std::max(0.5, Board.ChargeSink));
					BurstsDrawn.push_back({ T, EscortPiece, E.Key, E.What == "ahead", E.Until, E.Aim.Z, DrawnZ });
				}
				if (E.Kind == SeaShow::FxKind::Starshell) StarsDrawnAt.push_back(T);
			}
			for (const SeaShow::Fx& E : Night.Effects)
			{
				// (as FConvoyLooks::Effect: a boat forced up blows her tanks, one diving floods them; a boat scuttled is brought up
				// and blown; a rescue's nets kept until its time is over)
				if (E.Kind == SeaShow::FxKind::Dive && E.What == "surface" && E.Text == "forced") Blow(Boats[E.Key].State, true);
				if (E.Kind == SeaShow::FxKind::Dive && E.What == "dive" && Boats.count(E.Key)) Blow(Boats[E.Key].State, false);
				// (and as it does for a boat that struck: her tanks blown as she comes up or rides; scuttled, her vents opened, she
				// being up already)
				if (E.Kind == SeaShow::FxKind::Surrender) Blow(Boats[E.Key].State, true);
				if (E.Kind == SeaShow::FxKind::Sink && Boats.count(E.Key))
				{
					// (the stage takes her down from now: by the stern where she struck and was scuttled, as the scene tells it)
					BoatSeen& Sent = Boats[E.Key];
					const SeaShow::ShipOut* Her = Night.Ship(E.Key);
					Sent.SunkAt = T;
					Sent.bByStern = Her && SeaShow::StageEndOf(E.What, Her->Struck, Her->Lowered) == "stern";
				}
				if (E.Kind == SeaShow::FxKind::Sink && E.What == "scuttled" && Boats.count(E.Key))
				{
					Boats[E.Key].bScuttled = true;
					if (Boats[E.Key].Struck >= 0.0)
					{
						OpenVents(Boats[E.Key].State, T);
					}
					else
					{
						ForceUp(Boats[E.Key].State, T, Boats[E.Key].State.Sub);
						Blow(Boats[E.Key].State, true);
					}
				}
				if (E.Kind == SeaShow::FxKind::Rescue)
				{
					RescueSeen& R = Rescues[E.Key];
					R.Until = std::max(R.Until, E.Until > T ? E.Until : T + 600.0);
				}
				if (E.Kind == SeaShow::FxKind::ChargeDrop)
				{
					++Drops;
					DropIn In;
					In.Kind = E.What;
					In.N = static_cast<int>(std::lround(E.Size));
					In.Spread = E.Spread;
					In.Sinking = E.Sinking;
					In.Until = E.Until;
					In.At = P3(E.At.X, E.At.Y, E.At.Z);
					In.Aim = P3(E.Aim.X, E.Aim.Y, 0.0);
					In.Vel = P3(E.Vel.X, E.Vel.Y, 0.0);
					if (const SeaShow::ShipOut* Escort = Night.Ship(E.Key))
					{
						In.Beam = Escort->Beam;
						In.Long = Escort->Long;
					}
					for (const DropShot& Shot : DropPlan(In, static_cast<uint64_t>(E.Key) * 7919ull + static_cast<uint64_t>(T * 10.0)))
					{
						Landing L;
						L.Key = E.Key;
						L.Lands = T + Shot.Dt + Shot.Flight;
						L.To = Shot.To;
						L.bAhead = Shot.Side == 2;
						L.Middle = In.Aim;
						L.Sinking = In.Sinking;
						L.Ring = In.Spread > 0.0 ? In.Spread * 0.5 : 20.0;
						Landings.push_back(L);
					}
				}
				else if (E.Kind == SeaShow::FxKind::ChargeBurst)
				{
					const bool bAhead = E.What == "ahead";
					double Nearest = 1e9;
					const Landing* Near = nullptr;
					for (const Landing& L : Landings)
					{
						if (L.Key != E.Key || L.bAhead != bAhead || L.Lands > T + 0.5) continue;
						const double D = std::hypot(L.To.X - E.At.X, L.To.Y - E.At.Y);
						if (D < Nearest)
						{
							Nearest = D;
							Near = &L;
						}
					}
					ShowCheck(Near != nullptr, Said("a burst at %.1f s (escort %.0f) has a charge drawn going in before it", T, E.Key));
					if (!Near) continue;
					if (bAhead)
					{
						// (the ahead thrower's bombs burst only on a hit, the show placing them at the boat where she is then, wherever the
						// salvo was aimed: the ring is drawn where the show aimed it. The battle's own boat has moved on from its drop
						// as the bombs sank, so the burst lies off the ring as far as she does, and no more than the 30 m the show may
						// draw her from her place where she is attacked (sea_show_test): on the real nights it lay 124 m off, the boat
						// drawn far from her place. The review of 24 November, the second)
						++AheadBursts;
						const double FromMiddle = std::hypot(E.At.X - Near->Middle.X, E.At.Y - Near->Middle.Y);
						AheadOff = std::max(AheadOff, FromMiddle - Near->Ring);
						double BX = 0.0, BY = 0.0;
						const bool bHers = BattleBoatNear(T, E.At.X, E.At.Y, BX, BY);
						const double Hers = bHers ? std::hypot(BX - Near->Middle.X, BY - Near->Middle.Y) : 0.0;
						AheadBeyond = std::max(AheadBeyond, FromMiddle - Hers);
						ShowCheck(bHers && FromMiddle - Hers <= 30.0,
						          Said("an ahead bomb's burst at %.1f s lies off the ring's middle no more than the battle's boat does, and 30 m (%.1f m, hers %.1f m)", T, FromMiddle, Hers));
						ShowCheck(T > Near->Lands, Said("an ahead bomb's burst at %.1f s comes after its ring is in (%.1f s after)", T, T - Near->Lands));
					}
					else if (E.Size < 0.5)
					{
						// (a booked burst, Size 1, the show places at the boat it kills or hurts: not on the pattern's line)
						++PatternBursts;
						WorstMiss = std::max(WorstMiss, Nearest);
						LeastLead = std::min(LeastLead, T - Near->Lands);
						ShowCheck(Nearest <= 9.0, Said("a pattern's burst at %.1f s lies on a charge drawn going in (%.1f m off)", T, Nearest));
						ShowCheck(T - Near->Lands >= 0.5 * Near->Sinking && T - Near->Lands <= Near->Sinking + 3.0,
						          Said("a pattern's burst at %.1f s comes as its charge has sunk (%.1f s after it went in, sinking %.1f)", T, T - Near->Lands, Near->Sinking));
					}
				}
				else if (E.Kind == SeaShow::FxKind::Starshell)
				{
					++Stars;
					StarIn In;
					In.Burst = P3(E.At.X, E.At.Y, E.At.Z);
					In.T0 = T;
					In.Until = E.Until > T ? E.Until : T + StarBurnUsual();
					In.Wind = P3(Night.Wind().X, Night.Wind().Y, 0.0);
					const StarOut Lit = StarAt(In, T + 1.0);
					const StarOut Late = StarAt(In, In.Until - 0.5);
					const StarOut Gone = StarAt(In, In.Until + 10.0);
					ShowCheck(Lit.bBurning && Lit.Light > 0.9, Said("a starshell at %.1f s lights (%.2f)", T, Lit.Light));
					ShowCheck(Late.Flare.Z < Lit.Flare.Z - 0.5 * (In.Until - T) && Late.Flare.Z > 40.0, Said("a starshell at %.1f s sinks as it burns, still high at the end (%.0f m to %.0f m)", T, Lit.Flare.Z, Late.Flare.Z));
					ShowCheck(!Gone.bShown && Gone.Light == 0.0, Said("a starshell at %.1f s is gone after it burns out", T));
					ShowCheck(LitBy(Lit, P3(E.At.X, E.At.Y, 0.0)) > 0.2, Said("a starshell at %.1f s lights the sea under it (%.2f)", T, LitBy(Lit, P3(E.At.X, E.At.Y, 0.0))));
				}
			}
			Night.ChargesDown(DownBefore);
			Night.Effects.clear();
			if (T >= EndT - 1e-9) break;
			T = std::min(EndT, T + Step);
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
		}

		ShowCheck(!Boats.empty(), "the night has boats");
		for (const auto& [Key, B] : Boats)
		{
			ShowCheck(B.WorstPop < 0.25, Said("boat %.0f never pops where she can be seen (worst %.3f m in a frame, at %.1f s)", Key, B.WorstPop, B.PopAt));
			ShowCheck(B.WorstOff < 0.3, Said("boat %.0f surfaced rides where the show has her, her casing awash, or high once blown (worst %.2f m off, at %.1f s)", Key, B.WorstOff, B.OffAt));
			// (some such frames where the battle holds her there long enough to settle: 30 s told, the scene's 20 s or so from the
			// surface to periscope depth and its 5 s settling; every stand-in boat is told there 56 s and more. Every such frame,
			// on any night, shows the head)
			const auto AtScope = ToldAtScope.find(B.Piece);
			const double ToldUp = AtScope != ToldAtScope.end() ? AtScope->second : 0.0;
			ShowCheck((B.UpFrames > 0 || ToldUp < 30.0) && B.UpShown == B.UpFrames && (B.UpFrames == 0 || B.UpLeast > 0.5),
			          Said("boat %.0f at periscope depth with the show's periscope up: the stage's model shows its taller periscope's head in every frame (%.0f), at least %.2f m", Key, B.UpFrames, B.UpLeast) +
			              Said(" (told there %.0f s)", ToldUp));
			ShowCheck(B.UpFrames == 0 || (B.UpLowMost < 0.0 && B.UpBridgeMost < -1.0),
			          Said("boat %.0f, the periscope up: her shorter periscope (at most %.2f m) and her bridge (at most %.2f m) under", Key, B.UpLowMost, B.UpBridgeMost));
			ShowCheck(B.DownFrames == 0 || B.DownMost < -0.3, Said("boat %.0f at periscope depth with the show's periscope down: nothing of the stage's model out (its highest point at most %.2f m)", Key, B.DownMost));
			ShowCheck(B.DeepFrames == 0 || B.DeepMost < -3.0, Said("boat %.0f running deep: nothing of the stage's model near the surface (at most %.2f m)", Key, B.DeepMost));
			ShowCheck(B.Swirls == 0 || (B.SwirlLeast > -0.2 && B.SwirlMost <= 0.0),
			          Said("boat %.0f: each swirl comes as the last of the stage's model's bridge goes under (%.2f to %.2f m)", Key, B.SwirlLeast, B.SwirlMost));
			ShowCheck(B.FeatherBridgeMost < 0.0, Said("boat %.0f: feathers (%.0f) only while all of the stage's model's bridge is under (at most %.2f m)", Key, B.Feathers, B.FeatherBridgeMost));
		}
		ShowCheck(Jumpers > 0 && JumpWorst < 0.35, Said("a boat's men (%.0f) go over her side from her deck as the looks ride her (at most %.2f m off; %.2f m as they leave it)", Jumpers, JumpWorst, JumpRawWorst));
		// (climbers where the battle tells a rescue, every one on a net down; where it tells none, none: every stand-in tells one)
		ShowCheck((RescuesTold == 0 && ClimbFrames == 0) || (ClimbFrames > 0 && ClimbOn == ClimbFrames),
		          Said("every man the scene has climbing a rescuer's side is on a net that is down (%.0f of %.0f man-frames; %.0f rescues told)", ClimbOn, ClimbFrames, RescuesTold));
		ShowCheck((RescuesTold == 0 && Climbers == 0) || (Climbers > 0 && ClimbersOn == Climbers), Said("and his net was down before he reached it (%.0f of %.0f men)", ClimbersOn, Climbers));
		ShowCheck(ClimbOver == 0, Said("no man climbs over the net's top (her deck's height): %.0f man-frames", ClimbOver));
		ShowCheck(ClimbUnnamed == 0, Said("no man climbs a ship no rescue named (%.0f man-frames)", ClimbUnnamed));
		ShowCheck(NetsAway == 0, Said("no net down while she is under way or her rescue over and no one is on it (%.0f net-frames)", NetsAway));
		// (each where the battle tells it, and the night tells one of them at the least: the stand-ins tell all three; a real night
		// what its kit and its contacts gave it, a pattern and a starshell in 1940, ahead-thrown salvos and no pattern in 1943)
		ShowCheck((bPatternTold || bAheadHitTold || bStarTold) && (!bPatternTold || PatternBursts > 0) && (!bAheadHitTold || AheadBursts > 0) && (!bStarTold || Stars > 0),
		          Said("the night has pattern bursts (%.0f), ahead bursts (%.0f) and a starshell (%.0f)", PatternBursts, AheadBursts, Stars) +
		              Said(" where told (%.0f, %.0f, %.0f)", bPatternTold ? 1.0 : 0.0, bAheadHitTold ? 1.0 : 0.0, bStarTold ? 1.0 : 0.0));
		// A real night, one in the looks table or played from the real nights' folder (the design director's ruling of 03:40Z,
		// 26 November): it has its line in the table and tells every look of it, and every look it tells is drawn every time it
		// is told, each telling matched to its own drawing, never before it and at most LooksLate s after: a pattern its drop by
		// that escort, laid where the battle laid it (its run's middle within a charge's lethal reach of the point the battle
		// tells, where it tells one: a pattern drawn off it, as hunted's Escort 9's was drawn 75-176 m off it from her drawn
		// stern on round 6a's recordings, is not the battle's pattern), and every one of its charges bursting, those of hers due
		// as laid from its first going in to its last, sunk (a burst on the boat a hit is booked on is one of them); an ahead-thrown hit its bombs bursting by that escort; a
		// starshell its shell. A night neither in the table nor from that folder (a stand-in): the check above.
		const std::string NightName = NightOf(Path);
		const auto Listed = NightLooks.find(NightName);
		const bool bRealFolder = InRealFolder(Path);
		if (Listed != NightLooks.end() || bRealFolder)
		{
			RealPlayed.insert(NightName);
			// (a real night by its folder must have its line: the table never left short of a night played from there)
			if (bRealFolder) ShowCheck(Listed != NightLooks.end(), NightName + " (a real night: Tools/sea_show/recordings) has its line in the looks table");
			const std::set<std::string> NoLooks;
			const std::set<std::string>& LineLooks = Listed != NightLooks.end() ? Listed->second : NoLooks;
			const double LooksLate = 2.0, LooksEarly = 0.1;
			auto InTime = [LooksLate, LooksEarly](double DrawnT, double ToldT) { return DrawnT >= ToldT - LooksEarly && DrawnT <= ToldT + LooksLate; };
			int PatternsDrawn = 0, ChargesDue = 0, ChargesBurst = 0, PatternsOff = 0;
			double PatternLate = 0.0, PatternOffMost = 0.0;
			const double OnPlace = Board.ChargeLethal;
			std::vector<bool> DropTaken(PatternDrops.size(), false);
			for (const PatternTold& Pt : PatternsToldAt)
			{
				const double ToldT = Pt.T;
				const int Escort = Pt.Escort;
				const DropDrawn* Drop = nullptr;
				for (size_t I = 0; I < PatternDrops.size() && !Drop; ++I)
				{
					if (DropTaken[I] || PatternDrops[I].Piece != Escort || !InTime(PatternDrops[I].T, ToldT)) continue;
					DropTaken[I] = true;
					Drop = &PatternDrops[I];
				}
				if (!Drop) continue;
				int Burst = 0;
				for (const BurstDrawn& B : BurstsDrawn)
					if (!B.bAhead && B.Key == Drop->Key && B.Due >= ToldT + Drop->Sinking - 0.5 && B.Due <= ToldT + Drop->Run + Drop->Sinking + 0.5) ++Burst;
				ChargesDue += Drop->N;
				ChargesBurst += std::min(Burst, Drop->N);
				PatternLate = std::max(PatternLate, Drop->T - ToldT);
				const double Off = Pt.bPlace ? std::hypot(Drop->MidX - Pt.X, Drop->MidY - Pt.Y) : 0.0;
				PatternOffMost = std::max(PatternOffMost, Off);
				PatternsOff += Off > OnPlace ? 1 : 0;
				PatternsDrawn += Burst >= Drop->N && Off <= OnPlace ? 1 : 0;
			}
			// Each pattern laid all at once on the spot the battle tells bursts at the battle's moment, every charge at the
			// depth the battle set it to (the review of 27 November, of naval round 6b: the check above takes its window from
			// the drop's own run and sinking, and moves with them; a run timed at her drawn speed again, first-pack's charges
			// bursting up to 17 s after its blow, passed it). The moment from the recording alone: the battle's blow from that
			// escort on that boat where it tells one after the drop (within SeaShow::LateHitWithin of the rules' moment), else
			// the rules' moment (sim.ts: chargePattern's charges due at the drop and their sinking time rounded up to the whole
			// second, burstCharges bursting them at the first step of the battle, a view, at or after that). Every pattern
			// burst drawn of that escort from its drop to its next drop (or the night's end): exactly the charges the battle
			// tells, each in the frame of that moment (within a frame after it), laid at the set depth and drawn at it as it
			// burst (the view under the water's rule, from the frame before: going down at the charge's rate to its burst).
			// A pattern whose set depth or sinking the battle does not tell fails it: the rules' moment cannot be had.
			int AtOnceTold = 0, AtOnceRight = 0, AtOnceCharges = 0, AtOnceChargesRight = 0, NoSpot = 0;
			double AtOnceEarly = 0.0, AtOnceLate = 0.0, AtOnceZOff = 0.0;
			for (const PatternTold& Pt : PatternsToldAt)
			{
				if (!Pt.bPlace)
				{
					++NoSpot; // (where the battle tells no spot: laid from her stern as she runs, not all at once)
					continue;
				}
				++AtOnceTold;
				const bool bFigures = Pt.Set >= 0.0 && Pt.Sinking >= 0.0;
				const double Whole = std::ceil(std::max(0.0, Pt.Sinking) - 1e-9);
				double Moment = Pt.T + Whole;
				for (const double ViewT : ViewTimes)
					if (ViewT >= Pt.T + Whole - 1e-6)
					{
						Moment = ViewT;
						break;
					}
				double Blow = -1.0;
				for (const BlowTold& B : BlowsToldAt)
					if (Blow < 0.0 && B.From == Pt.Escort && B.On == Pt.On && B.T >= Pt.T - 1e-9 && B.T <= Moment + SeaShow::LateHitWithin + 1e-9) Blow = B.T;
				const double Want = Blow >= 0.0 ? Blow : Moment;
				const int Charges = std::clamp(Pt.N > 0 ? Pt.N : 10, 1, 30);
				const DropDrawn* Drop = nullptr;
				for (const DropDrawn& D : PatternDrops)
					if (!Drop && D.Piece == Pt.Escort && InTime(D.T, Pt.T)) Drop = &D;
				double NextDrop = 1e18;
				if (Drop)
					for (const DropDrawn& D : PatternDrops)
						if (D.Key == Drop->Key && D.T > Drop->T + 1e-9) NextDrop = std::min(NextDrop, D.T);
				int Burst = 0, Right = 0;
				double Early = 0.0, Late = 0.0, ZOff = 0.0;
				for (const BurstDrawn& B : BurstsDrawn)
				{
					if (!Drop || B.bAhead || B.Key != Drop->Key || B.T < Drop->T - 1e-9 || B.T >= NextDrop - 1e-9) continue;
					++Burst;
					const double Off = B.T - Want;
					const double Deep = bFigures ? std::max(std::fabs(B.LaidZ + Pt.Set), std::fabs(B.DrawnZ + Pt.Set)) : 1e9;
					Early = std::max(Early, -Off);
					Late = std::max(Late, Off);
					ZOff = std::max(ZOff, Deep);
					Right += bFigures && Off >= -1e-6 && Off <= Step + 1e-6 && Deep <= 0.5 ? 1 : 0;
				}
				AtOnceCharges += Charges;
				AtOnceChargesRight += std::min(Right, Charges);
				AtOnceEarly = std::max(AtOnceEarly, Early);
				AtOnceLate = std::max(AtOnceLate, Late);
				AtOnceZOff = std::max(AtOnceZOff, ZOff);
				const bool bRight = bFigures && Drop && Burst == Charges && Right == Charges;
				AtOnceRight += bRight ? 1 : 0;
				std::printf("%s: the pattern of piece %d dropped at %.1f s on piece %d, set %.0f m, sinking %.1f s: the rules' moment %.1f s, the battle's blow %s; %d of its %d "
				            "charges drawn bursting (%d of them in that frame, at the set depth), the earliest %.3f s before it, the latest %.3f s after it, the farthest %.2f m "
				            "off the set depth%s\n",
				            NightName.c_str(), Pt.Escort, Pt.T, Pt.On, Pt.Set, Pt.Sinking, Moment, Blow >= 0.0 ? Said("%.1f s", Blow).c_str() : "not told", Burst, Charges, Right,
				            Early, Late, ZOff, bRight ? "" : bFigures ? (Drop ? " (WRONG)" : " (no drop drawn)") : " (its set depth or sinking not told)");
			}
			int AheadDrawn = 0;
			double AheadLate = 0.0;
			for (const auto& [ToldT, Escort] : AheadHitsToldAt)
			{
				double First = -1.0;
				for (const BurstDrawn& B : BurstsDrawn)
					if (B.bAhead && B.Piece == Escort && InTime(B.T, ToldT) && (First < 0.0 || B.T < First)) First = B.T;
				if (First < 0.0) continue;
				++AheadDrawn;
				AheadLate = std::max(AheadLate, First - ToldT);
			}
			int StarsDrawn = 0;
			double StarLate = 0.0;
			std::vector<bool> StarTaken(StarsDrawnAt.size(), false);
			for (const double ToldT : StarsToldAt)
			{
				for (size_t I = 0; I < StarsDrawnAt.size(); ++I)
				{
					if (StarTaken[I] || !InTime(StarsDrawnAt[I], ToldT)) continue;
					StarTaken[I] = true;
					++StarsDrawn;
					StarLate = std::max(StarLate, StarsDrawnAt[I] - ToldT);
					break;
				}
			}
			const int Told[3] = { static_cast<int>(PatternsToldAt.size()), static_cast<int>(AheadHitsToldAt.size()), static_cast<int>(StarsToldAt.size()) };
			std::string Line;
			for (const std::string& Look : LineLooks) Line += (Line.empty() ? "" : " ") + Look;
			if (Line.empty()) Line = "no line";
			const std::string Real = NightName + " (the looks table: " + Line + "): ";
			ShowCheck(AtOnceRight == AtOnceTold,
			          Real +
			              "every pattern laid all at once on the spot the battle tells bursts at the battle's moment (its blow, else the rules' moment: the first view at or after "
			              "the drop and its whole seconds of sinking), every charge in that frame and at its set depth: " +
			              Said("%.0f of %.0f", AtOnceRight, AtOnceTold) +
			              Said(" (%.0f of %.0f charges; the earliest %.3f s before it,", AtOnceChargesRight, AtOnceCharges, AtOnceEarly) +
			              Said(" the latest %.3f s after it, against a frame of %.3f;", AtOnceLate, Step) + Said(" the farthest %.2f m off its set depth, against 0.5;", AtOnceZOff) +
			              Said(" %.0f told with no spot, not laid all at once)", NoSpot));
			for (int K = 0; K < 3; ++K)
			{
				const std::string Look = LookNames[K];
				if (Told[K] > 0) ToldOnReal[Look].insert(NightName);
				if (LineLooks.count(Look)) ShowCheck(Told[K] > 0, Real + "tells " + Look + ", as its line says" + Said(" (told %.0f times)", Told[K]));
				else if (Told[K] > 0) std::printf("  note  %s tells %s (%d times), not in its line of the looks table: add it there\n", NightName.c_str(), Look.c_str(), Told[K]);
			}
			ShowCheck(PatternsDrawn == Told[0], Real + Said("every pattern told drawn, its drop by its escort, laid where the battle laid it and every one of its charges bursting: %.0f of %.0f",
			                                                PatternsDrawn, Told[0]) +
			                                        Said(" (%.0f of %.0f charges burst; drawn at most %.2f s after it was told;", ChargesBurst, ChargesDue, PatternLate) +
			                                        Said(" %.0f laid off its point, its run's middle as far as %.1f m from it, against %.1f)", PatternsOff, PatternOffMost, OnPlace));
			ShowCheck(AheadDrawn == Told[1],
			          Real + Said("every ahead-thrown hit told drawn, its bombs bursting by its escort: %.0f of %.0f (at most %.2f s after it was told)", AheadDrawn, Told[1], AheadLate));
			ShowCheck(StarsDrawn == Told[2], Real + Said("every starshell told drawn: %.0f of %.0f (at most %.2f s after it was told)", StarsDrawn, Told[2], StarLate));
			std::printf("%s's looks as the table asks (%s): pattern told %d, drawn %d (%d of %d charges burst); ahead-thrown hits told %d, drawn %d; starshells told %d, drawn %d; the latest "
			            "%.2f s after its telling\n",
			            NightName.c_str(), Line.c_str(), Told[0], PatternsDrawn, ChargesBurst, ChargesDue, Told[1], AheadDrawn, Told[2], StarsDrawn, std::max({ PatternLate, AheadLate, StarLate }));
		}
		// A boat that struck (sea:surrender): her flag, her men on her casing, her vents. Where the night has none, nothing asked.
		int StruckBoats = 0;
		for (const auto& [Key, B] : Boats)
		{
			if (!B.bStruck)
			{
				ShowCheck(B.Vents == 0, Said("boat %.0f, which never struck, throws no air from her vents (%.0f spurts)", Key, B.Vents));
				continue;
			}
			++StruckBoats;
			ShowCheck(!B.bScuttled || B.Vents >= 3, Said("boat %.0f struck and scuttled: her vents' air along her sides as she floods (%.0f spurts)", Key, B.Vents));
			const double FirstCharge = B.ChargesAt.empty() ? -1.0 : B.ChargesAt.front();
			ShowCheck(B.bScuttled ? B.ChargesAt.size() == static_cast<size_t>(ScuttleCharges) && B.Puffs == ScuttleCharges : B.ChargesAt.empty() && B.Puffs == 0,
			          Said("boat %.0f struck: her scuttling charges both go off where she was scuttled, none where not (%.0f charges, %.0f puffs)", Key, static_cast<double>(B.ChargesAt.size()), B.Puffs));
			ShowCheck(B.ChargesAt.empty() || (B.SunkAt >= 0.0 && FirstCharge >= B.SunkAt + ChargesFrom && FirstCharge > B.LastHandAt),
			          Said("boat %.0f: her first charge %.1f s after she was sent down, ", Key, FirstCharge - B.SunkAt) +
			              Said("%.1f s after the last man was off her", FirstCharge - B.LastHandAt));
		}
		// (any boat that never struck: no charges)
		for (const auto& [Key, B] : Boats)
			if (!B.bStruck) ShowCheck(B.ChargesAt.empty() && B.Puffs == 0, Said("boat %.0f, which never struck, has no scuttling charge (%.0f)", Key, static_cast<double>(B.ChargesAt.size())));
		if (StruckBoats > 0)
		{
			ShowCheck(FlagFrames > 0 && FlagMissing == 0 && FlagStaysUp == 0,
			          Said("a struck boat's white flag flies from her ensign's place once the show has her up (%.0f frames, %.0f without it), and is down once hauled down (%.0f up after)", FlagFrames,
			               FlagMissing, FlagStaysUp));
			ShowCheck(FlagUnder == 0 && FlagWindward == 0,
			          Said("and streams to leeward in a breeze (%.0f of %.0f breezy frames to windward), clear of the sea (%.0f frames within half a metre)", FlagWindward, FlagBreezy, FlagUnder));
			ShowCheck(HandFrames > 0 && HandsWorst < 0.35,
			          Said("her men on her casing stand on it as the looks ride her: %.0f man-frames, the worst %.2f m off, at %.1f s", HandFrames, HandsWorst, HandsWorstAt) +
			              Said(" (boat %.0f)", HandsWorstKey));
			ShowCheck(HandsLate == 0, Said("sent down, no man on her or in her hatches more than %.0f s after (%.0f man-frames, at most %.1f s)", HandsGoneIn, HandsLate, HandsLateMost));
		}
		std::printf("%s: %zu boats, %d drops, %d pattern bursts (worst %.1f m off a charge drawn going in, the least %.1f s after it), %d ahead bursts (at most %.1f m outside the ring the show aimed, "
		            "%.1f m farther from its middle than the battle's boat), %d starshells\n",
		            Rec.Key.c_str(), Boats.size(), Drops, PatternBursts, WorstMiss, LeastLead, AheadBursts, AheadOff, AheadBeyond, Stars);
		std::printf("  the scene's men: %d went over a boat's side, at most %.2f m off her deck as the looks ride her; %d climbed a rescuer's side, %d man-frames, every one on a net down (most nets on one "
		            "ship %d); she stood by %.0f s; her heave while men climbed %.2f to %.2f m (the net rides it, the scene's climbers do not); the climbers %.2f to %.2f m out "
		            "from her side as the stage draws her\n",
		            Jumpers, JumpWorst, Climbers, ClimbFrames, MostNets, StoodBy, ClimbHeaveLow, ClimbHeaveHigh, ClimbOffDrawnLow, ClimbOffDrawnHigh);
		if (StruckBoats > 0)
		{
			std::printf("  struck boats: %d; their flags %d frames up, %d breezy; their men %d man-frames on their casings, at most %.2f m off them\n", StruckBoats, FlagFrames, FlagBreezy, HandFrames,
			            HandsWorst);
			for (const auto& [Key, B] : Boats)
			{
				if (!B.bStruck) continue;
				// (one kept afloat, a prize, was never sent down: nothing to time)
				if (B.SunkAt < 0.0)
				{
					std::printf("  struck boat %d: not sent down (a prize); %d vent spurts, %d charges\n", Key, B.Vents, static_cast<int>(B.ChargesAt.size()));
					continue;
				}
				std::printf("  struck boat %d: %s, sent down at %.1f s%s, the last man off her %.1f s after; %d vent spurts, %d charges (%.1f s and %.1f s after)\n", Key,
				            B.bScuttled ? "scuttled" : "not scuttled", B.SunkAt, B.bByStern ? " by the stern" : "", B.LastHandAt - B.SunkAt, B.Vents, static_cast<int>(B.ChargesAt.size()),
				            B.ChargesAt.empty() ? -1.0 : B.ChargesAt.front() - B.SunkAt, B.ChargesAt.size() < 2 ? -1.0 : B.ChargesAt[1] - B.SunkAt);
			}
		}
		for (const auto& [Key, B] : Boats)
		{
			std::printf("  boat %d: worst pop %.3f m (at %.1f s); surfaced, at most %.2f m off the show's trim awash or blown (at %.1f s), %d frames riding high; the stage's model: periscope up %d frames, its head %.2f m out at"
			            " least, the shorter's at most %.2f, the bridge's %.2f; periscope down %d frames, highest point at most %.2f; deep %d frames, at most %.2f; %d swirls as the bridge went"
			            " under (%.2f to %.2f m), %d feathers (the bridge at most %.2f m)\n",
			            Key, B.WorstPop, B.PopAt, B.WorstOff, B.OffAt, B.HighFrames, B.UpFrames, B.UpLeast, B.UpLowMost, B.UpBridgeMost, B.DownFrames, B.DownMost, B.DeepFrames, B.DeepMost, B.Swirls,
			            B.SwirlLeast, B.SwirlMost, B.Feathers, B.FeatherBridgeMost);
		}
		return 0;
	}

	/** An enemy boat's looks as the scene steps them (FConvoyLooks::StepBoats) out of the player's sight, beside a twin
	 * stepped as if she were in sight all along. */
	struct UnseenSeen
	{
		BoatState Hid, Open;
		BoatModel Model;
		bool bSeen = false, bWasShown = true;
		int HiddenFrames = 0, HiddenShallow = 0, Reveals = 0;
		int Thrown = 0, TwinThrown = 0, Laid = 0, ShownOff = 0;
		double MostApart = 0.0;
		/** Her first swirl after each time she is found, and her twin's after the same moment (-1: none yet; RevealAt -1
		 * between), until she is hidden again or the night ends: the same moment, her clock having run on unseen. */
		double RevealAt = -1.0, HidSwirl = -1.0, TwinSwirl = -1.0;
		int SwirlsAfter = 0, SwirlsOff = 0;
	};

	/**
	 * The night again as the player's side sees it (SetMine from the views' "me", as the scene's OpenOn): each enemy boat's
	 * looks stepped as the scene steps them, her bShown the show's, beside a twin in sight all along. Hidden, she rides as
	 * her twin (found, she is where she would be: no jump) and throws nothing on the sea, no splash and no new swirl; in
	 * sight, she throws as her twin does, and found deep she lays her first swirl when her twin does.
	 */
	int PlayUnseen(const std::string& Path)
	{
		std::ifstream File(Path);
		if (!File) return 2;
		std::stringstream Text;
		Text << File.rdbuf();
		SeaFeed::Recording Rec;
		std::string Error;
		if (!SeaFeed::ReadRecording(Text.str(), Rec, Error) || Rec.Views.empty()) return 2;
		SeaShow::Show Night(Rec.Seed ? Rec.Seed : 239);
		Night.SetField(Rec.Board);
		const int Mine = SeaFeed::SideOf(Rec.Views.front().Head.Me);
		Night.SetMine(Mine);
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& V = Rec.Views[Taken++];
			Night.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
		};
		TakeNext();
		if (Taken < Rec.Views.size()) TakeNext();
		const SeaShow::Field& Board = Night.Board();
		const double Sky = SkyLit(Board.bLight, Board.SunUp, Board.MoonUp, Board.MoonLit);
		std::map<int, UnseenSeen> Boats;
		const double Step = 1.0 / 30.0;
		double T = Rec.Views.front().Head.T;
		const double EndT = Rec.Views.back().Head.T;
		while (true)
		{
			Night.Advance(T);
			for (const SeaShow::ShipOut& Ship : Night.Ships())
			{
				if (Ship.Class != "ss" || Ship.Dive.empty() || Ship.Side == Mine) continue;
				UnseenSeen& B = Boats[Ship.Key];
				if (!B.bSeen) B.Model = SculptedOf(Ship.Nation == "GER" ? "Submarine_GER" : Ship.Nation == "USA" ? "Submarine_USA" : "Submarine");
				BoatIn In;
				In.T = T;
				In.Dive = Ship.Dive;
				In.Keel = Ship.Keel;
				In.Dived = Ship.Dived;
				In.bScope = Ship.bScope;
				In.Heave = Ship.Heave;
				In.Pitch = Ship.Pitch;
				In.Speed = Ship.Speed;
				In.At = P3(Ship.At.X, Ship.At.Y, 0.0);
				In.Heading = Ship.Heading;
				In.ScopeKeel = Board.ScopeKeel;
				In.Lit = Ship.Dive == "deep" ? Sky : 0.0;
				const BoatOut Twin = StepBoat(B.Open, In, B.Model, Drawn());
				// (as the scene: the light over her worked out in sight or out of it, so her swirls' clock runs on unseen)
				In.bShown = Ship.bShown;
				const BoatOut Out = StepBoat(B.Hid, In, B.Model, Drawn());
				// (found, her first swirl and her twin's: settled once both are laid, she is hidden again, or the night ends)
				auto Settle = [&B]()
				{
					if (B.RevealAt >= 0.0 && (B.HidSwirl >= 0.0 || B.TwinSwirl >= 0.0))
					{
						++B.SwirlsAfter;
						if (std::fabs(B.HidSwirl - B.TwinSwirl) > 1e-9) ++B.SwirlsOff;
					}
					B.RevealAt = -1.0;
				};
				if (B.bSeen && Ship.bShown && !B.bWasShown)
				{
					B.RevealAt = T;
					B.HidSwirl = B.TwinSwirl = -1.0;
				}
				if (B.RevealAt >= 0.0)
				{
					for (const Swirl& W : B.Hid.Swirls)
						if (B.HidSwirl < 0.0 && W.T0 >= B.RevealAt) B.HidSwirl = W.T0;
					for (const Swirl& W : B.Open.Swirls)
						if (B.TwinSwirl < 0.0 && W.T0 >= B.RevealAt) B.TwinSwirl = W.T0;
					if ((B.HidSwirl >= 0.0 && B.TwinSwirl >= 0.0) || !Ship.bShown || T >= EndT - 1e-9) Settle();
				}
				if (!(Ship.Keel > 0.0)) continue;
				B.MostApart = std::max({ B.MostApart, std::fabs(Out.Heave - Twin.Heave), std::fabs(Out.BridgeZ - Twin.BridgeZ), std::fabs(Out.ScopeZ - Twin.ScopeZ) });
				if (!Ship.bShown)
				{
					++B.HiddenFrames;
					// (near enough the surface that her twin might throw something: the line the deep frames are judged from)
					if (Ship.Keel <= Board.ScopeKeel + 6.0) ++B.HiddenShallow;
					B.Thrown += static_cast<int>(Out.Splashes.size());
					B.TwinThrown += static_cast<int>(Twin.Splashes.size());
					// (a swirl laid this frame while she is hidden: none)
					for (const Swirl& W : B.Hid.Swirls)
						if (W.T0 > T - Step * 0.5) ++B.Laid;
				}
				else if (Out.Splashes.size() != Twin.Splashes.size()) ++B.ShownOff;
				if (B.bSeen && Ship.bShown && !B.bWasShown) ++B.Reveals;
				B.bWasShown = Ship.bShown;
				B.bSeen = true;
			}
			Night.Effects.clear();
			if (T >= EndT - 1e-9) break;
			T = std::min(EndT, T + Step);
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
		}
		int Hidden = 0, HiddenShallow = 0, Found = 0, TwinThrown = 0, SwirledAfter = 0;
		for (const auto& [Key, B] : Boats)
		{
			Hidden += B.HiddenFrames;
			HiddenShallow += B.HiddenShallow;
			Found += B.Reveals;
			TwinThrown += B.TwinThrown;
			SwirledAfter += B.SwirlsAfter;
			ShowCheck(B.MostApart < 1e-9, Said("boat %.0f out of the player's sight rides as her twin in it (%.2g m apart at the most): found, she is where she would be", Key, B.MostApart));
			ShowCheck(B.Thrown == 0 && B.Laid == 0, Said("boat %.0f out of the player's sight throws nothing on the sea (%.0f splashes, %.0f swirls laid)", Key, B.Thrown, B.Laid));
			ShowCheck(B.ShownOff == 0, Said("boat %.0f in sight throws as her twin (%.0f frames otherwise)", Key, B.ShownOff));
			ShowCheck(B.SwirlsOff == 0, Said("boat %.0f found lays her first swirl when her twin in sight does (%.0f times found with a swirl after, %.0f otherwise)", Key, B.SwirlsAfter,
			                                 B.SwirlsOff));
		}
		// (their twins threw something, so that throwing nothing hidden means something: where any was hidden near the surface,
		// where a twin throws; one hidden only deep throws nothing in sight or out of it. Every stand-in hides one near it)
		ShowCheck(Boats.empty() || (Hidden > 0 && Found > 0 && (HiddenShallow == 0 || TwinThrown > 0)),
		          Said("the night's enemy boats out of the player's sight a while (%.0f boat-frames), then found (%.0f times); their twins in sight threw %.0f splashes then", Hidden, Found, TwinThrown) +
		              Said(" (%.0f boat-frames hidden near the surface)", HiddenShallow));
		std::printf("%s as the player's side sees it: %zu enemy boats, %d boat-frames out of sight, found %d times (%d with a swirl after, laid when her twin's)\n", Rec.Key.c_str(),
		            Boats.size(), Hidden, Found, SwirledAfter);
		return 0;
	}

	/** The gun crews' sailor files the scene reads (Content/Models/Soldiers, CONVOY_LOOKS_CONTENT), every navy's. */
	bool ReadCrews(SeaSailors::Crew& Out)
	{
		const char* Content = std::getenv("CONVOY_LOOKS_CONTENT");
		if (Content == nullptr) return false;
		for (const char* Navy : { "GER", "ITA", "JAP", "SOV", "UK", "USA" })
		{
			std::ifstream File(std::string(Content) + "/Models/Soldiers/soldier_" + Navy + "_sailor.json");
			std::stringstream Text;
			Text << File.rdbuf();
			std::string Why;
			if (!File || !Out.AddFile(std::string(Navy) == "UK" ? "GBR" : Navy, Text.str(), Why)) return false;
		}
		return true;
	}

	/** The model the stage draws a ship of (AirStage's ShipFileOf, its rows this checks against TestCrew's pins): a boat's
	 * by her navy (Submarine_GER, Submarine_USA, else Submarine), a merchant's by her key's roll. */
	std::string StagedFileOf(const SeaShow::ShipOut& Ship)
	{
		if (Ship.Class == "ss")
		{
			return Ship.Nation == "GER" ? "Submarine_GER" : Ship.Nation == "USA" ? "Submarine_USA" : "Submarine";
		}
		const uint32_t Roll = (static_cast<uint32_t>(Ship.Key) * 2654435761u >> 16) % 20u;
		return Roll < 13u ? "Freighter" : Roll < 18u ? "Tanker" : "Troopship";
	}

	/** The nights' totals of the gun crews' checks (each night may have no gun told). */
	int CrewNights = 0, CrewManned = 0, CrewRounds = 0, CrewEligible = 0, CrewRams = 0, CrewHiddenFrames = 0, CrewShut = 0;

	/** Whether a gun's loader is shown at his post with no round in his hands short of the breech shut (waiting with his
	 * hands out, reaching for the next, past breech_close, or standing clear of the breech): as the scene draws him. */
	bool ShutAtPost(const DeckCrew::Watch& W, const DeckCrew::Blocks& K)
	{
		const int Loader = static_cast<int>(DeckCrew::Role::Loader);
		const DeckCrew::Shown& Is = W.Men[Loader];
		return W.Hands[Loader].Now == DeckCrew::Phase::AtGun && W.SentAt < 0.0 && Is.bShown &&
		       (Is.What != SeaSailors::Act::Loader || Is.Time < K.Taken || Is.Time >= K.Shut);
	}

	/** Whether a gun's layer, trainer and loader are shown at their posts (none sent below). */
	bool AtPosts(const DeckCrew::Watch& W)
	{
		bool bAll = W.SentAt < 0.0;
		for (int R = 0; R < DeckCrew::Roles; ++R)
		{
			bAll = bAll && (R == static_cast<int>(DeckCrew::Role::Passer) || (W.Hands[R].Now == DeckCrew::Phase::AtGun && W.Men[R].bShown));
		}
		return bAll;
	}

	/**
	 * The night again as the scene steps its deck guns' crews (DeckCrew::Step, from TellOf each frame at 30 a second,
	 * SetMine as the scene's; one watch a ship, as the scene keeps them, each gun's mount and shape its own), every boat's
	 * and merchant's whose model has a gun the files' crew can man: none shown before her gun is first laid; none on a
	 * boat's deck where it is within FreeboardLeast of the sea (her deck under each man as SeaShow::HandAt sets the show's
	 * own men on her casing: less how far she is down, with her pitch, heave and heel), nor longer than DownWithin after she
	 * is told down; none from the moment she strikes, nor with any of the show's own men out on her casing (her gun's crew
	 * are of her crew, never more men than the show's); none drawn on a ship the show hides; a merchant changing guns, her
	 * crew at the new one at once (the old one's gone the same frame: counted, the scene's rule); every round the show
	 * fires from a gun the files' crew can man, the first of a gun's and those fired as her crew are still coming up her
	 * hatch among them, fired with its layer, its trainer and its loader shown at their posts and no round in the loader's
	 * hands short of the breech shut (the review of 25 November: a quarter of a night's rounds fired with her crew below or
	 * the loader mid-load); and for each round the loader could have loaded in his own time (at the gun all the time since
	 * the round before, the show's rounds no quicker than his clip at its pace) his ram heard before it goes.
	 */
	int PlayGunCrews(const std::string& Path, const SeaSailors::Crew& Sailors)
	{
		const double DownWithin = 4.5, FreeboardLeast = 0.1;
		std::ifstream File(Path);
		if (!File) return 2;
		std::stringstream Text;
		Text << File.rdbuf();
		SeaFeed::Recording Rec;
		std::string Error;
		if (!SeaFeed::ReadRecording(Text.str(), Rec, Error) || Rec.Views.empty()) return 2;
		SeaShow::Show Night(Rec.Seed ? Rec.Seed : 239);
		Night.SetField(Rec.Board);
		Night.SetMine(SeaFeed::SideOf(Rec.Views.front().Head.Me));
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& V = Rec.Views[Taken++];
			Night.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
		};
		TakeNext();
		if (Taken < Rec.Views.size()) TakeNext();
		struct Mounted
		{
			ConvoyLook::GunMount Mount;
			DeckCrew::GunShape Shape;
			int File = -1;
		};
		struct Crewed
		{
			DeckCrew::Watch W;
			int MountKey = -1;
			double FirstLaid = -1.0, LastFire = -1.0, LastRam = -1.0, AtSince = -1.0;
		};
		std::map<int, Mounted> Mounts; // each gun's, by the scene's MountKey (her key twice, and 1 for a merchant's stern gun)
		std::map<int, Crewed> Guns;    // each ship's watch, by her key (the scene's GunWatches)
		std::map<int, DeckCrew::Blocks> Kits;
		int ShownEarly = 0, ShownAwash = 0, ShownLate = 0, ShownStruck = 0, ShownWithHands = 0, DrawnHidden = 0, Rounds = 0, Eligible = 0, Rammed = 0, Manned = 0, HiddenFrames = 0;
		int Shut = 0, Snapped = 0;
		std::string Open;
		int GunChanges = 0, ChangedAway = 0;
		double LateMost = 0.0, DeckLeast = 99.0;
		const double Every = 1.0 / 30.0;
		double T = Rec.Views.front().Head.T;
		const double EndT = Rec.Views.back().Head.T;
		while (true)
		{
			Night.Advance(T);
			for (const SeaShow::ShipOut& Ship : Night.Ships())
			{
				if (Ship.Class != "ss" && Ship.Class != "mv") continue;
				const bool bAftMount = Ship.Class == "mv" && Ship.bAftGun;
				const int MountKey = Ship.Key * 2 + (bAftMount ? 1 : 0);
				auto Known = Mounts.find(MountKey);
				if (Known == Mounts.end())
				{
					Mounted Fresh;
					Fresh.Mount = MountOf(StagedFileOf(Ship), bAftMount);
					Fresh.Shape = DeckCrew::ShapeOf(Fresh.Mount);
					Fresh.File = Fresh.Shape.File.empty() ? -1 : Sailors.FileFor(Ship.Nation);
					Known = Mounts.emplace(MountKey, Fresh).first;
				}
				const Mounted& Gun = Known->second;
				if (Gun.File < 0) continue;
				auto Kit = Kits.find(Gun.File);
				if (Kit == Kits.end()) Kit = Kits.emplace(Gun.File, DeckCrew::BlocksOf(Sailors, Gun.File)).first;
				if (!Kit->second.bGun) continue;
				Crewed& G = Guns[Ship.Key];
				// (a merchant changing guns while hers is manned: the scene's one watch stands the old crew down and calls
				// the new one's at once)
				const bool bChanged = G.MountKey >= 0 && G.MountKey != MountKey && G.W.bManned;
				G.MountKey = MountKey;
				// (as the scene: each frame from what the show says, the cues to the sound path)
				const DeckCrew::Told Said = DeckCrew::TellOf(Ship);
				std::vector<DeckCrew::Cue> Cues;
				const bool bFires = G.W.LastT >= 0.0 && Ship.FiredAt > G.W.SeenFired + 1e-9;
				const int Loader = static_cast<int>(DeckCrew::Role::Loader);
				// (the loader as drawn the frame before: at his post with the breech shut, or not)
				const bool bShutBefore = ShutAtPost(G.W, Kit->second);
				const bool bUpBefore = AtPosts(G.W);
				if (bFires)
				{
					++Rounds;
					const bool bCould = G.LastFire >= 0.0 && G.AtSince >= 0.0 && G.AtSince <= G.LastFire &&
					                    Ship.FiredAt - G.LastFire >= DeckCrew::FastestReload(Kit->second, Gun.Shape.bBoat) && G.W.SentAt < 0.0;
					if (bCould)
					{
						++Eligible;
						Rammed += G.LastRam > G.LastFire && G.LastRam <= Ship.FiredAt + 1e-9 ? 1 : 0;
					}
					G.LastFire = Ship.FiredAt;
				}
				DeckCrew::Step(G.W, Said, Gun.Shape, Gun.Mount, Kit->second, T, &Cues);
				for (const DeckCrew::Cue& Each : Cues)
				{
					if (Each.Name == Kit->second.RamCue) G.LastRam = T;
				}
				if (bFires)
				{
					// (every round: its crew at their posts as it goes, the breech shut on it; the drawn frame before it, or
					// this one where they were not)
					const bool bShut = AtPosts(G.W) && (bShutBefore || ShutAtPost(G.W, Kit->second));
					Shut += bShut ? 1 : 0;
					Snapped += bShut && !(bUpBefore && bShutBefore) ? 1 : 0;
					if (!bShut && Open.size() < 400)
					{
						char Line[160];
						std::snprintf(Line, sizeof(Line), " %s %d at %.2f (phases %d,%d,%d; clip %.2f%s)", Ship.Class.c_str(), Ship.Key, Ship.FiredAt, static_cast<int>(G.W.Hands[0].Now),
						              static_cast<int>(G.W.Hands[1].Now), static_cast<int>(G.W.Hands[Loader].Now), G.W.Clip, G.W.bWaiting ? ", waiting" : "");
						Open += Line;
					}
				}
				const bool bLoaderAt = G.W.Hands[Loader].Now == DeckCrew::Phase::AtGun && G.W.SentAt < 0.0;
				G.AtSince = bLoaderAt ? (G.AtSince < 0.0 ? T : G.AtSince) : -1.0;
				if (G.FirstLaid < 0.0 && Ship.bGunLaid) G.FirstLaid = T;
				bool bAny = false, bAwash = false;
				for (int R = 0; R < DeckCrew::Roles; ++R)
				{
					const DeckCrew::Shown& Is = G.W.Men[R];
					bAny = bAny || Is.bShown;
					DrawnHidden += !Ship.bShown && DeckCrew::Drawn(Is, Ship) ? 1 : 0;
					if (!Is.bShown || !Gun.Shape.bBoat) continue;
					// (her deck under him over the sea, as SeaShow::HandAt sets her men on her casing)
					const double Along = Gun.Mount.Along + Is.At.Root.Fore;
					const double Deck = DeckCrew::DeckUnder(Gun.Shape, Gun.Mount, Is.At.Root.Fore, Is.At.Root.Right) - Ship.Dived + Along * std::sin(Ship.Pitch * 3.14159265358979323846 / 180.0) + Ship.Heave -
					                    Is.At.Root.Right * std::sin(Ship.Heel * 3.14159265358979323846 / 180.0);
					DeckLeast = std::min(DeckLeast, Deck);
					bAwash = bAwash || Deck <= FreeboardLeast;
				}
				int HandsOut = 0;
				if (bAny)
				{
					for (const SeaShow::ManOut& Hand : Night.Hands()) HandsOut += Hand.Ship == Ship.Key ? 1 : 0;
				}
				Manned += bAny ? 1 : 0;
				HiddenFrames += bAny && !Ship.bShown ? 1 : 0;
				ShownEarly += bAny && G.FirstLaid < 0.0 ? 1 : 0;
				ShownAwash += bAwash ? 1 : 0;
				ShownStruck += bAny && Ship.Struck >= 0.0 ? 1 : 0;
				ShownWithHands += bAny && HandsOut > 0 ? 1 : 0;
				if (bChanged)
				{
					++GunChanges;
					// (the old gun's crew gone, the new one's at it this same frame where it is laid)
					bool bAtNew = G.W.bAft == bAftMount;
					for (int R = 0; R < DeckCrew::Roles; ++R)
					{
						bAtNew = bAtNew && (R == static_cast<int>(DeckCrew::Role::Passer) || (G.W.Men[R].bShown && G.W.Hands[R].Now == DeckCrew::Phase::AtGun));
					}
					ChangedAway += Said.bLaid && !bAtNew ? 1 : 0;
				}
				if (bAny && G.W.SentAt >= 0.0 && T > G.W.SentAt + DownWithin)
				{
					++ShownLate;
					LateMost = std::max(LateMost, T - G.W.SentAt);
				}
			}
			Night.Effects.clear();
			if (T >= EndT - 1e-9) break;
			T = std::min(EndT, T + Every);
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
		}
		const std::string N = "[" + Rec.Key + "'s gun crews] ";
		ShowCheck(ShownEarly == 0, N + Said("none shown before her gun is first laid (%.0f gun-frames)", ShownEarly));
		ShowCheck(ShownAwash == 0 && ShownLate == 0, N + Said("none on a boat's deck within %.1f m of the sea (%.0f gun-frames; her deck under a man %.2f m over it at the least)", FreeboardLeast, ShownAwash, DeckLeast < 98.0 ? DeckLeast : 0.0) +
		                                                 Said(", nor %.1f s after she is told down (%.0f gun-frames, %.2f s at the most)", DownWithin, ShownLate, LateMost));
		ShowCheck(ShownStruck == 0 && ShownWithHands == 0, N + Said("none of her gun's crew from the moment she strikes (%.0f gun-frames), nor with any of the show's own men out on her casing (%.0f): ", ShownStruck, ShownWithHands) +
		                                                       "her gun's crew are of her crew, never more men than the show's");
		ShowCheck(ChangedAway == 0, N + Said("a merchant changing guns while manned (%.0f times): the old one's crew gone the same frame, the new one's at it at once where it is laid (%.0f not)", GunChanges, ChangedAway));
		ShowCheck(DrawnHidden == 0, N + Said("none drawn on a ship the show hides (%.0f men-frames; %.0f gun-frames manned while she is hidden)", DrawnHidden, HiddenFrames));
		ShowCheck(Shut == Rounds, N + Said("every round fired with its layer, trainer and loader at their posts and the breech shut: %.0f of %.0f", Shut, Rounds) +
		                              Said(" (%.0f of them as her crew were still coming up, or the loader's round short of the breech: at their posts at once)", Snapped) + Open);
		ShowCheck(Rammed == Eligible, N + Said("the loader's ram heard before each round he could have loaded in his own time: %.0f of %.0f (%.0f rounds)", Rammed, Eligible, Rounds));
		++CrewNights;
		CrewManned += Manned;
		CrewRounds += Rounds;
		CrewEligible += Eligible;
		CrewRams += Rammed;
		CrewHiddenFrames += HiddenFrames;
		CrewShut += Shut;
		std::printf("%s's gun crews: %zu guns of %zu ships, %d gun-frames manned (%d while hidden), %d rounds (%d at a manned gun with the breech shut, %d of them with her crew put at their posts as it fired), %d the loader could load, %d rammed before; %d gun changes; least deck under a man %.2f m\n",
		            Rec.Key.c_str(), Mounts.size(), Guns.size(), Manned, HiddenFrames, Rounds, Shut, Snapped, Eligible, Rammed, GunChanges, DeckLeast);
		return 0;
	}
}

int main(int ArgC, char** ArgV)
{
	int Status = 0;
	CheckDeckRows();
	SeaSailors::Crew Sailors;
	const bool bCrews = ReadCrews(Sailors);
	ShowCheck(bCrews, "the gun crews' sailor files, every navy's (CONVOY_LOOKS_CONTENT: the game's Content)");
	ReadNightLooks();
	ShowCheck(bNightLooksRead, "the real nights' looks table read, every line a night and its looks (CONVOY_LOOKS_NIGHTS: Tools/convoy_looks/night_looks.txt)" +
	                               (bNightLooksRead ? std::string() : ": " + NightLooksWhy));
	if (const char* Folder = std::getenv("CONVOY_LOOKS_REAL")) RealFolder = Folder;
	ShowCheck(!RealFolder.empty(), "the real nights' folder named, each recording played from it a real night (CONVOY_LOOKS_REAL: Tools/sea_show/recordings)");
	for (int I = 1; I < ArgC; ++I)
	{
		Status = std::max(Status, PlayNight(ArgV[I]));
		Status = std::max(Status, PlayUnseen(ArgV[I]));
		if (bCrews) Status = std::max(Status, PlayGunCrews(ArgV[I], Sailors));
	}
	// Across the real nights played (the looks table's, and every recording played from the real nights' folder), each look
	// told on one of them at the least, and so drawn there every time it is told (each night's own checks, above): the design
	// director's ruling of 03:40Z, 26 November, that every look is still told on a real night, so that each of its checks
	// still bites. (No real night played, a checkout without the real nights' recordings: nothing to ask, and said so.)
	if (!RealPlayed.empty())
	{
		std::string Played;
		for (const std::string& Night : RealPlayed) Played += (Played.empty() ? "" : ", ") + Night;
		for (const char* Look : LookNames)
		{
			std::string On;
			for (const std::string& Night : ToldOnReal[Look]) On += (On.empty() ? "" : ", ") + Night;
			ShowCheck(!ToldOnReal[Look].empty(), std::string("across the real nights played (") + Played + "), " + Look + " told on one of them at the least (on " + (On.empty() ? "none" : On) + ")");
			std::printf("the real nights' looks: %s told on %s\n", Look, On.empty() ? "none of them" : On.c_str());
		}
	}
	else std::printf("the real nights' looks: no real night played (none of the looks table, none from the real nights' folder), so nothing asked across them\n");
	for (const auto& Row : NightLooks)
		if (!RealPlayed.count(Row.first)) std::printf("  note  %s is in the looks table but was not played\n", Row.first.c_str());
	ShowCheck(!bCrews || CrewNights == 0 || (CrewManned > 0 && CrewEligible > 0 && CrewRams > 0 && CrewShut == CrewRounds),
	          Said("the nights' gun crews at their guns (%.0f gun-frames), the loader's ram heard (%.0f times) before %.0f rounds he could load", CrewManned, CrewRams, CrewEligible) +
	              Said("; %.0f of the %.0f rounds fired at a manned gun with the breech shut", CrewShut, CrewRounds));
	std::printf("convoy looks against the show: %d checks, %d failed\n", ShowChecks, ShowFailures);
	return ShowFailures > 0 ? 1 : Status;
}
