// The BEFORE reel's own check (the ship detail plan, R1.1): Battle/SeaReel.h, the game's own code, held to the Reel
// agent's spec and the Mac's run sheet (sea-scene/ship-detail/mac/BEFORE-REEL.md), and its hooks in Battle/SeaBattle.cpp
// read from the source: every one runs only under its switch, so with neither switch asked the scene is as it was.
// Usage: reel_test <Source/TheaterCommand>   (built by Tools/ship_detail/run.sh)

#include "Battle/SeaReel.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Checks = 0, Failures = 0;

	void Check(bool bOk, const std::string& What, const std::string& Got)
	{
		++Checks;
		Failures += bOk ? 0 : 1;
		std::printf("  %s  %s: %s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.c_str());
	}

	bool Has(const std::string& In, const std::string& What)
	{
		return In.find(What) != std::string::npos;
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		std::stringstream Out;
		Out << In.rdbuf();
		return Out.str();
	}

	std::string Joined(const std::vector<std::string>& Names)
	{
		std::string Out;
		for (const std::string& One : Names)
		{
			Out += (Out.empty() ? "" : ",") + One;
		}
		return Out;
	}

	/** A function's body in the source, from its definition at the head of a line to its closing brace. */
	std::string BodyOf(const std::string& Text, const std::string& Head)
	{
		const size_t At = Text.find("\n" + Head);
		if (At == std::string::npos)
		{
			return std::string();
		}
		const size_t Open = Text.find('{', At);
		int Depth = 0;
		for (size_t K = Open; K < Text.size(); ++K)
		{
			Depth += Text[K] == '{' ? 1 : Text[K] == '}' ? -1 : 0;
			if (Depth == 0)
			{
				return Text.substr(At + 1, K + 1 - At - 1);
			}
		}
		return std::string();
	}

	SeaLookSwitches::Look Asking(const std::vector<std::pair<std::string, std::string>>& Asked)
	{
		SeaLookSwitches::Look Out;
		for (const auto& [Name, Value] : Asked)
		{
			for (int K = 0; K < SeaLookSwitches::Count; ++K)
			{
				if (Name == SeaLookSwitches::All[K].Name)
				{
					Out.Given.push_back(SeaLookSwitches::Asked{ K, Value });
				}
			}
		}
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::printf("usage: reel_test <Source/TheaterCommand>\n");
		return 2;
	}
	const std::string Src = Argv[1];

	std::printf("The shots kept (-TheaterSeaReel=)\n");
	{
		Check(SeaReel::Unnumbered("05-a-hit") == "a-hit" && SeaReel::Unnumbered("112-the-end") == "the-end" && SeaReel::Unnumbered("a-hit") == "a-hit" &&
				  SeaReel::Unnumbered("05") == "05" && SeaReel::Unnumbered("escort-3-burst") == "escort-3-burst",
			"a planned name without its number (NN-), and a name with none as it is", SeaReel::Unnumbered("05-a-hit"));
		const std::vector<std::string> Asked = SeaReel::ReadReel(" funnels, a-hit ,going-down,fall-of-shot,05-a-hit,, ");
		Check(Joined(Asked) == "funnels,a-hit,going-down,fall-of-shot", "the names asked, in order, each once, any number dropped", Joined(Asked));
		Check(SeaReel::ReadReel("off").empty() && SeaReel::ReadReel("0").empty() && SeaReel::ReadReel("  ").empty(), "off, 0 or nothing: no reel", "none");
		const std::vector<std::string> Plan = { "01-the-fleets", "02-broadside", "03-fall-of-shot", "04-a-hit", "05-funnels", "06-going-down", "07-the-end" };
		std::string Kept;
		for (const std::string& Name : Plan)
		{
			Kept += SeaReel::Keeps(Asked, Name, false) ? (Kept.empty() ? "" : ",") + Name : "";
		}
		Check(Kept == "03-fall-of-shot,04-a-hit,05-funnels,06-going-down", "the planned shots named kept, with their numbers, the rest left", Kept);
		const std::vector<std::string> Under = { "under" };
		Check(SeaReel::Keeps(Under, "08-escort-1", true) && !SeaReel::Keeps(Under, "09-escort-1-burst", false) && !SeaReel::Keeps(Under, "10-escort-2", false),
			"\"under\": the first window under the water only (the scene says which; never its -burst picture)", "08-escort-1");
		Check(SeaReel::HoldUntil(100.0, 200.0) == 108.0 && SeaReel::HoldUntil(100.0, 106.0) == 103.0 && SeaReel::HoldUntil(100.0, 101.0) == 100.0,
			"held until 8 s after its picture, or 3 s before the next kept shot, never before the picture",
			"108, 103, 100");
	}

	std::printf("The ride strip (-TheaterSeaRideStrip=)\n");
	{
		const SeaReel::Ride R = SeaReel::ReadRide("64,200,10");
		Check(R.bOn && R.Ship == 64 && R.From == 200.0 && R.Seconds == 10.0, "ship, from, seconds read", std::to_string(R.Ship));
		Check(SeaReel::ReadRide("64,200,90").Seconds == 30.0 && SeaReel::ReadRide("64,200,0.2").Seconds == 1.0, "seconds held to 1 to 30", "30, 1");
		Check(!SeaReel::ReadRide("64,200").bOn && !SeaReel::ReadRide("x,200,10").bOn && !SeaReel::ReadRide("64,,10").bOn && !SeaReel::ReadRide("-1,200,10").bOn &&
				  !SeaReel::ReadRide("64,200,0").bOn && !SeaReel::ReadRide("off").bOn,
			"anything else read is off", "off");
		Check(SeaReel::RidePicture("samar", 64, 0) == "samar-ride-64-0000.png" && SeaReel::RidePicture("samar", 64, 600) == "samar-ride-64-0600.png",
			"its pictures <key>-ride-<ship>-NNNN.png, as the run sheet counts them", SeaReel::RidePicture("samar", 64, 600));
		// The scene as the Mac runs it, -benchmark -fps=60: a skip to 20 s before, then the engine's fixed step, a float's
		// 1/60 (and a double's, and a step a hair short), the frame taken after the step, as TakePictures follows StepTo.
		for (const double Step : { static_cast<double>(1.0f / 60.0f), 1.0 / 60.0, 1.0 / 60.0 - 1e-9 })
		{
			double T = 180.0;
			int Frames = 0;
			std::string First, Last;
			for (int Tick = 0; Tick < 3000 && !SeaReel::RideOver(R, T); ++Tick)
			{
				T += Step;
				if (SeaReel::InRide(R, T))
				{
					const std::string Line = SeaReel::RideLine(Frames, T, 64, "dd", 1.234, -0.4, 0.12);
					First = First.empty() ? Line : First;
					Last = Line;
					++Frames;
				}
			}
			char Said[64];
			std::snprintf(Said, sizeof(Said), "a step of %.12f s", Step);
			Check(Frames == 601 && Has(First, "ride frame 0000 at 200.000 s") && Has(Last, "ride frame 0600 at 210.000 s"),
				std::string("601 frames from 200.000 to 210.000 s, ") + Said, std::to_string(Frames) + " frames; " + Last);
		}
		const std::string Line = SeaReel::RideLine(7, 200.11667, 64, "dd", 1.234, -0.4, 0.12);
		Check(Line == "Sea battle: ride frame 0007 at 200.117 s, ship 64 (dd): heel +1.23, pitch -0.40 degrees, heave +0.12 m.", "a ride frame's line", Line);
	}

	std::printf("The switches as the scene takes them (OpenOn), and the \"Sea look:\" line\n");
	{
		SeaLookSwitches::Look Both = Asking({ { "TheaterSeaReel=", "funnels,a-hit,going-down,fall-of-shot" } });
		SeaReel::Taken T = SeaReel::Take(Both, true);
		const std::string Said = SeaLookSwitches::Line(Both);
		Check(Joined(T.Shots) == "funnels,a-hit,going-down,fall-of-shot" && !T.Strip.bOn && Has(Said, "-TheaterSeaReel=funnels,a-hit,going-down,fall-of-shot (asked)") &&
				  !Has(Said, "not built"),
			"in a picture run the reel taken and said as built, with its shots (the run sheet's first check)", Said.substr(Said.find("-TheaterSeaReel")));
		SeaLookSwitches::Look Watched = Asking({ { "TheaterSeaReel=", "funnels" }, { "TheaterSeaRideStrip=", "64,200,10" } });
		T = SeaReel::Take(Watched, false);
		const std::string Off = SeaLookSwitches::Line(Watched);
		Check(T.Shots.empty() && !T.Strip.bOn && Has(Off, "-TheaterSeaReel=off (asked)") && Has(Off, "-TheaterSeaRideStrip=off (asked)"),
			"outside a picture run (or in the month's scene) neither is taken, and the line says off", "off");
		SeaLookSwitches::Look Two = Asking({ { "TheaterSeaReel=", "funnels" }, { "TheaterSeaRideStrip=", "64,200,10" } });
		T = SeaReel::Take(Two, true);
		const std::string Ride = SeaLookSwitches::Line(Two);
		Check(T.Shots.empty() && T.Strip.bOn && Has(Ride, "-TheaterSeaRideStrip=64,200.000,10 (asked)") && Has(Ride, "-TheaterSeaReel=off (asked)"),
			"both asked: the ride strip taken, the reel not", Ride.substr(Ride.find("-TheaterSeaReel")));
		SeaLookSwitches::Look None;
		T = SeaReel::Take(None, true);
		const std::string Plain = SeaLookSwitches::Line(None);
		Check(T.Shots.empty() && !T.Strip.bOn && Has(Plain, "-TheaterSeaReel=off, -TheaterSeaRideStrip=off.") && !Has(Plain, "(asked)"),
			"neither asked: neither taken, both said at today's off", Plain.substr(Plain.find("-TheaterSeaReel")));
	}

	std::printf("The frames counted, and the lines\n");
	{
		using SeaReel::Counts;
		Check(Counts(true, true, false, 50.0, 0.016, 120.0, 110.0, false, 0.0), "a settled frame on the shot, before its picture, counts", "counted");
		Check(!Counts(false, true, false, 50.0, 0.016, 120.0, 110.0, false, 0.0) && !Counts(true, false, false, 50.0, 0.016, 120.0, 110.0, false, 0.0) &&
				  !Counts(true, true, true, 50.0, 0.016, 120.0, 110.0, false, 0.0) && !Counts(true, true, false, 0.5, 0.016, 120.0, 110.0, false, 0.0) &&
				  !Counts(true, true, false, 50.0, 31.0, 120.0, 110.0, false, 0.0),
			"not off the shot, settling, a frame apart, in the first second, nor a held game", "left out");
		Check(!Counts(true, true, false, 50.0, 0.016, 120.0, 119.5, false, 0.0) && !Counts(true, true, false, 50.0, 0.016, 120.0, 121.0, false, 0.0) &&
				  !Counts(true, true, false, 50.0, 0.016, 120.0, 121.0, true, 0.3) && Counts(true, true, false, 50.0, 0.016, 120.0, 124.0, true, 0.7),
			"nor within 0.6 s of the picture, either side; the hold after it counts", "left out, then counted");
		SeaReel::Held H;
		const std::string Empty = SeaReel::ShotLine("05-a-hit", "a hit", 1024, 235.3, H);
		Check(Empty == "Sea battle: reel 05-a-hit (a hit, ship 1024) at 235.3 s: no frames counted; 0 ships laid in it, 0.0 ms.", "a shot with no frames says so", Empty);
		for (int K = 0; K < 100; ++K)
		{
			H.Add(5.0 + 0.01 * K, 7.0, 1.0, 9.0 + 0.02 * K, 16.7, 230.0 + 0.1 * K, 40 + K % 5, 12, -1, -1);
		}
		H.Lay(3.25);
		const std::string Line = SeaReel::ShotLine("05-a-hit", "a hit", 1024, 235.3, H);
		Check(Has(Line, "Sea battle: reel 05-a-hit (a hit, ship 1024) at 235.3 s: 100 frames from 230.0 to 239.9 s; game ") &&
				  Has(Line, " ms, render ") && Has(Line, " ms, graphics chip ") && Has(Line, ", the most of the three ") && Has(Line, ", the whole frame 16.70/16.70/16.70 ms (mean/95th/99th); ") &&
				  Has(Line, "ship parts on screen 42/44/44, 12 on their far mesh at the middle; draw calls not read; 1 ships laid in it, 3.2 ms.") && Has(Line, "graphics chip 9.99/"),
			"a shot's line as the run sheet's section 10 has it (draw calls not read on an engine without them)", Line);
		SeaReel::Held Read;
		for (int K = 0; K < 20; ++K)
		{
			Read.Add(5.0, 7.0, 1.0, 9.0, 16.7, 230.0 + K, 40, 12, 1500 + K, 2000000 + 1000 * K);
		}
		const std::string Calls = SeaReel::ShotLine("02-broadside", "a broadside", -1, 120.0, Read);
		Check(Has(Calls, "Sea battle: reel 02-broadside (a broadside) at 120.0 s: ") && Has(Calls, "draw calls 1510/1518/1519 and primitives 2.01/2.02/2.02 million (middle/95th/most); "),
			"with the engine's counters read: draw calls and primitives, middle, 95th and most", Calls.substr(Calls.find("draw calls")));
		const std::string Laid = SeaReel::LaidLine(64, "dd", "JPN", "Destroyer_JAP.json", 3.21, 180.0, true);
		Check(Laid == "Sea battle: laid ship 64 (dd, JPN, Destroyer_JAP.json) in 3.2 ms of the game thread at 180.0 s (while skipping).", "a ship's laying", Laid);
		Check(SeaReel::LaidSum({ 2.0f, 3.0f, 9.0f }) == "Sea battle: reel: 3 ships laid, 3.0 ms at the middle, 9.0 at the 95th, 9.0 at most." &&
				  SeaReel::LaidSum({}) == "Sea battle: reel: no ships laid.",
			"the layings' sum at the end", SeaReel::LaidSum({ 2.0f, 3.0f, 9.0f }));
		const std::string Open = SeaReel::OpenLine({ "funnels", "a-hit", "the-moon" }, 14, { { "05-a-hit", 235.3 }, { "09-funnels", 400.0 } }, { "the-moon" });
		Check(Open == "Sea battle: reel of funnels, a-hit, the-moon: 2 of the 14 planned shots kept (05-a-hit at 235.3 s, 09-funnels at 400.0 s); none planned for the-moon; "
					  "each held after its picture until 8 s after it or 3 s before the next kept shot.",
			"the reel's opening line", Open);
		bool bClean = true;
		for (const std::string& One : { Empty, Line, Calls, Laid, Open, SeaReel::LaidSum({ 1.0f }), SeaReel::RideLine(0, 200.0, 64, "dd", 0.0, 0.0, 0.0) })
		{
			bClean = bClean && !Has(One, "Sea battle: threads") && !Has(One, "frame times over") && !Has(One, "dice");
		}
		Check(bClean, "no reel line holds \"Sea battle: threads\" or \"frame times over\" (the bench's reader never takes one for its own)", "none");
	}

	std::printf("The hooks in the scene (Battle/SeaBattle.cpp), read from the source\n");
	{
		const std::string Cpp = ReadAll(Src + "/Battle/SeaBattle.cpp"), H = ReadAll(Src + "/Battle/SeaBattle.h");
		Check(Cpp.size() > 100000 && H.size() > 1000, "the sea battle's source read", std::to_string(Cpp.size()) + " bytes");
		// (the kinds' words in ESeaShot's order)
		const size_t Enum = H.find("enum class ESeaShot : uint8 { ");
		std::vector<std::string> Kinds;
		if (Enum != std::string::npos)
		{
			std::string Rest = H.substr(Enum + 30, H.find('}', Enum) - Enum - 30);
			std::string One;
			for (const char Ch : Rest + ",")
			{
				if (Ch == '/')
				{
					Kinds.push_back(SeaReel::Trimmed(One));
					break;
				}
				if (Ch == ',')
				{
					Kinds.push_back(SeaReel::Trimmed(One));
					One.clear();
				}
				else
				{
					One.push_back(Ch);
				}
			}
		}
		Check(static_cast<int>(Kinds.size()) == SeaReel::Kinds && Kinds.front() == "Fleets" && Kinds[4] == "Hit" && Kinds.back() == "Under",
			"a word for each of the scene's shot kinds, in their order", std::to_string(Kinds.size()) + " kinds, " + std::to_string(SeaReel::Kinds) + " words");
		const std::string Open = BodyOf(Cpp, "bool ASeaBattle::OpenOn(");
		Check(Has(Open, "const SeaReel::Taken Reel = SeaReel::Take(SeaLook, !ShotFolder.IsEmpty() && !bMonthScene);") &&
				  Open.find("SeaReel::Take(") > Open.find("SeaLook = SeaLookSwitches::Read(") && Open.find("SeaReel::Take(") < Open.find("PlanShots();"),
			"H1: both read once, from the look, in a picture run only and never in the month's scene", "OpenOn");
		Check(Has(Open, "PlanShots();\n\tif (bReel || bRideStrip)\n\t{\n\t\tReelPlan();"), "H2: the shots kept, or the ride strip's one, right after PlanShots, under the switches",
			"OpenOn");
		const std::string Lay = BodyOf(Cpp, "void ASeaBattle::LayAndSteer()");
		Check(Has(Lay, "const uint64 LaidFrom = bReel || bRideStrip ? FPlatformTime::Cycles64() : 0;") &&
				  Has(Lay, "Stage->SetShips(TArray<FStageShip>{ Given });\n\t\t\tif (bReel || bRideStrip)\n\t\t\t{\n\t\t\t\tReelLaid("),
			"H3: each laying timed and said under the switches", "LayAndSteer");
		const std::string Tick = BodyOf(Cpp, "void ASeaBattle::Tick(");
		Check(Has(Tick, "if (bReel)\n\t{\n\t\tReelCount(Took);") && Tick.find("ReelCount(Took);") < Tick.find("bLastFrameApart = false;") &&
				  Tick.find("ReelCount(Took);") > Tick.find("GroundAsks = 0;"),
			"H4: the frame counted to its shot under the switch, before the frame's apart mark is cleared", "Tick");
		const std::string Take = BodyOf(Cpp, "void ASeaBattle::TakePictures()");
		Check(Has(Take, "if (bRideStrip)\n\t{\n\t\tRideStripFrame();") && Has(Take, "bLastFrameApart = true;\n\t\tif (bReel)") &&
				  Has(Take, "else if (bReel && PictureAt >= 0.0 && S.BattleT < ReelHoldUntil)") &&
				  Take.find("else if (bReel && PictureAt") < Take.find("else if (PictureAt >= 0.0 && RealT >= PictureAt + 0.5)") &&
				  Has(Take, "if (bReel)\n\t\t{\n\t\t\tReelSayShot(Shot);") && Take.find("ReelSayShot(Shot);") < Take.find("Shot.bTaken = true;"),
			"H5: the ride strip's own pictures, the hold set at the picture and kept before the half second's step, the shot said before it is marked taken",
			"TakePictures");
		const std::string Say = BodyOf(Cpp, "void ASeaBattle::SayFrameTimes()");
		Check(Has(Say, "SayThreadsAndLook(); // (the release 2 ocean plan, step 0)\n\tif (bReel || bRideStrip)") , "H6: the layings' sum after the threads' and look lines, under the switches",
			"SayFrameTimes");
		bool bUntouched = true;
		std::string Which;
		for (const char* Name : { "void ASeaBattle::PlanShots()", "void ASeaBattle::AimEye()", "bool ASeaBattle::ShotEye(" })
		{
			const std::string Body = BodyOf(Cpp, Name);
			const bool bClean = !Body.empty() && !Has(Body, "Reel") && !Has(Body, "RideStrip");
			bUntouched = bUntouched && bClean;
			Which += bClean ? "" : std::string(Name) + " ";
		}
		Check(bUntouched, "PlanShots, AimEye and ShotEye not touched", Which.empty() ? "none touched" : Which);
		bool bApart = true;
		for (const char* File : { "/Battle/SeaShow.cpp", "/Battle/SeaShow.h", "/Battle/AirStage.cpp", "/Battle/AirStage.h", "/Campaign/CampaignMiniatures.cpp" })
		{
			const std::string Text = ReadAll(Src + File);
			bApart = bApart && !Text.empty() && !Has(Text, "SeaReel") && !Has(Text, "TheaterSeaReel") && !Has(Text, "TheaterSeaRideStrip");
		}
		Check(bApart, "the reel outside the show, the stage and the miniatures", "SeaShow.*, AirStage.*, CampaignMiniatures.cpp");
		// (every use of the reel's state in the scene is in its own functions or under a switch)
		size_t Loose = 0;
		std::string LooseAt;
		size_t From = 0;
		const std::vector<std::string> Own = { "void ASeaBattle::ReelPlan()", "void ASeaBattle::ReelCount(", "void ASeaBattle::ReelLaid(", "void ASeaBattle::ReelSayShot(",
			"void ASeaBattle::RideStripFrame()" };
		std::vector<std::pair<size_t, size_t>> Spans;
		for (const std::string& Name : Own)
		{
			const std::string Body = BodyOf(Cpp, Name);
			const size_t At = Body.empty() ? std::string::npos : Cpp.find(Body);
			Spans.push_back({ At, At == std::string::npos ? At : At + Body.size() });
		}
		Check(Spans.size() == 5 && std::all_of(Spans.begin(), Spans.end(), [](const auto& S) { return S.first != std::string::npos; }), "the reel's five functions found", "5");
		for (size_t At = Cpp.find("Reel", From); At != std::string::npos; At = Cpp.find("Reel", At + 1))
		{
			bool bOwn = false;
			for (const auto& [A, B] : Spans)
			{
				bOwn = bOwn || (At >= A && At < B);
			}
			const size_t LineAt = Cpp.rfind('\n', At) + 1;
			const std::string Line = Cpp.substr(LineAt, Cpp.find('\n', At) - LineAt);
			// (allowed outside: the includes, the draw calls' switch, its comments, the switch's own reading, and a line
			// under the switches)
			const bool bAllowed = bOwn || Has(Line, "#include \"Battle/SeaReel.h\"") || Has(Line, "THEATER_SEA_REEL_DRAW_CALLS") || Line.find("//") < Line.find("Reel") ||
				Has(Line, "SeaReel::Take(SeaLook") || Has(Line, "ReelShots = Reel.Shots;") || Has(Line, "ReelRide = Reel.Strip;") || Has(Line, "bReel = !ReelShots.empty();") ||
				Has(Line, "bRideStrip = ReelRide.bOn;") || Has(Line, "if (bReel") || Has(Line, "bReel || bRideStrip") || Has(Line, "else if (bReel && ");
			if (!bAllowed)
			{
				const size_t Guard = Cpp.rfind("if (bReel", At);
				const size_t Close = Guard == std::string::npos ? std::string::npos : Cpp.find("\n\t}", Guard);
				const bool bUnder = Guard != std::string::npos && At - Guard < 400 && (Close == std::string::npos || Close > At);
				if (!bUnder)
				{
					++Loose;
					LooseAt += (LooseAt.empty() ? "" : " | ") + SeaReel::Trimmed(Line);
				}
			}
		}
		Check(Loose == 0, "every use of the reel's state is in its own functions, the switches' reading, or under a switch (with neither asked, the scene is as it was)",
			Loose ? LooseAt : "none loose");
	}

	std::printf("%d checks, %d failed\n%s\n", Checks, Failures, Failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failures ? 1 : 0;
}
