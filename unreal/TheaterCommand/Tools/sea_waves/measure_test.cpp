// The release 2 ocean plan, step 0 (measure first; nothing on screen changes): the cloud test of the two engine-free
// pieces the sea battle's end-of-battle lines are made of, Battle/SeaFrames (the frames by thread) and
// Battle/SeaLookSwitches (the "Sea look:" line), and of the game's own lines that feed them, read in SeaBattle.cpp.
// It writes the lines it made, with the numbers they hold, to OUT/measure_lines.txt, which bench_table.py's self-test
// reads back, so the bench's reader is held to the game's own writer.
// Usage: measure_test SRC OUT   (SRC: Source/TheaterCommand; OUT: a folder to write in)

#include "Battle/SeaFrames.h"
#include "Battle/SeaLookSwitches.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Checks = 0, Failed = 0;

	void Check(const std::string& What, bool bOk, const std::string& Got)
	{
		++Checks;
		Failed += bOk ? 0 : 1;
		std::printf("  %s  %s: %s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.c_str());
	}

	std::string Num(double X, int Places = 2)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, X);
		return Text;
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		std::stringstream Buf;
		Buf << In.rdbuf();
		return Buf.str();
	}

	std::string Lower(std::string Text)
	{
		std::transform(Text.begin(), Text.end(), Text.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
		return Text;
	}

	bool Has(const std::string& Text, const std::string& Part) { return Text.find(Part) != std::string::npos; }

	/** The frame-time line's own rule, written out as the game writes it (SayFrameTimes): FloorToInt32 of Share x (N - 1)
	 * + 0.5, clamped into the sorted list. */
	double Reference(const std::vector<float>& Sorted, double Share)
	{
		const int N = static_cast<int>(Sorted.size());
		const int Place = static_cast<int>(std::floor(Share * (N - 1) + 0.5));
		return Sorted[static_cast<std::size_t>(std::clamp(Place, 0, N - 1))];
	}

	/** A seeded stream of its own (a frame-time-like spread: mostly near 9 ms, a tail to 40). */
	struct Stream
	{
		std::uint64_t S;
		double Next()
		{
			S = S * 6364136223846793005ULL + 1442695040888963407ULL;
			return static_cast<double>(S >> 11) / 9007199254740992.0;
		}
	};

	/** What a threads line holds, read back by its own words (what bench_table.py reads, held here to the writer): the
	 * frames, the play left out, the battle's clock at the first and last frame counted, the mean, 95th and 99th of the
	 * game, render, RHI, chip, the most of the three and the whole frame (V, in that order), the frames over their slowest
	 * thread and their share, and the ground asks' mean, 95th and most. */
	struct ThreadsRead
	{
		int N = 0, Over = 0;
		double From = 0.0, First = 0.0, Last = 0.0, Share = 0.0, OverMs = 0.0;
		double V[18] = {};
		double A[3] = {};
	};

	bool ReadThreads(const std::string& Line, ThreadsRead& R)
	{
		// (the RHI's note, where it is said, comes between its numbers and the chip's: read in two halves)
		const std::size_t Chip = Line.find(", graphics chip ");
		if (Chip == std::string::npos)
		{
			return false;
		}
		const bool bFirst = std::sscanf(Line.c_str(),
			"Sea battle: threads over %d frames from %lf s of play (the battle's %lf to %lf s): game %lf/%lf/%lf ms, "
			"render %lf/%lf/%lf ms, RHI %lf/%lf/%lf ms",
			&R.N, &R.From, &R.First, &R.Last, &R.V[0], &R.V[1], &R.V[2], &R.V[3], &R.V[4], &R.V[5], &R.V[6], &R.V[7], &R.V[8]) == 13;
		const bool bSecond = std::sscanf(Line.c_str() + Chip,
			", graphics chip %lf/%lf/%lf ms, the most of the three (game, render, chip) %lf/%lf/%lf ms, the whole frame "
			"%lf/%lf/%lf ms (mean/95th/99th in the hundred); %d frames (%lf%%) more than %lf ms over their slowest thread; "
			"the kit's ground asked %lf times a frame on the mean, %lf at the 95th, %lf at most",
			&R.V[9], &R.V[10], &R.V[11], &R.V[12], &R.V[13], &R.V[14], &R.V[15], &R.V[16], &R.V[17], &R.Over, &R.Share, &R.OverMs,
			&R.A[0], &R.A[1], &R.A[2]) == 15;
		return bFirst && bSecond;
	}

	/** The numbers a threads line holds, written as the line writes them, for bench_table.py's self-test to hold its
	 * reading to: in the line's order, every number of it. */
	std::string Numbers(const SeaFrames::Record& Rec, double From)
	{
		const SeaFrames::Spread S[6] = { SeaFrames::Of(Rec.Game), SeaFrames::Of(Rec.Render), SeaFrames::Of(Rec.Rhi), SeaFrames::Of(Rec.Chip), SeaFrames::Of(Rec.Most), SeaFrames::Of(Rec.Whole) };
		const SeaFrames::Spread A = SeaFrames::Of(Rec.Ground);
		std::string Out = std::to_string(Rec.Num()) + ' ' + Num(From, 0) + ' ' + Num(Rec.FirstT, 0) + ' ' + Num(Rec.LastT, 0);
		for (const SeaFrames::Spread& One : S)
		{
			Out += ' ' + Num(One.Mean) + ' ' + Num(One.P95) + ' ' + Num(One.P99);
		}
		Out += ' ' + std::to_string(Rec.Over) + ' ' + Num(100.0 * Rec.Over / static_cast<double>(Rec.Num()), 1) + ' ' + Num(SeaFrames::Record::OverMs, 0);
		return Out + ' ' + Num(A.Mean, 0) + ' ' + Num(A.P95, 0) + ' ' + Num(A.Worst, 0);
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "usage: measure_test SRC OUT\n");
		return 2;
	}
	const std::string Src = Argv[1], Out = Argv[2];
	std::ofstream Lines(Out + "/measure_lines.txt");
	Lines << "# measure_test: lines the game's own writers (Battle/SeaFrames, Battle/SeaLookSwitches) made, each after what it holds\n";
	std::vector<std::string> Said;

	std::printf("The frames by thread (Battle/SeaFrames)\n");
	{
		// 1 to 100 out of order: the mean 50.5, the 95th 95, the 99th 99, the worst 100 by the frame-time line's rule
		std::vector<float> Hundred;
		for (int K = 0; K < 100; ++K)
		{
			Hundred.push_back(static_cast<float>(1 + (K * 37) % 100));
		}
		const SeaFrames::Spread S = SeaFrames::Of(Hundred);
		Check("1 to 100 out of order: 100 frames, mean 50.5, 95th 95, 99th 99, worst 100", S.N == 100 && S.Mean == 50.5 && S.P95 == 95.0 && S.P99 == 99.0 && S.Worst == 100.0,
			std::to_string(S.N) + ", " + Num(S.Mean) + ", " + Num(S.P95) + ", " + Num(S.P99) + ", " + Num(S.Worst));
		const SeaFrames::Spread None = SeaFrames::Of({});
		Check("no frames: nothing, and no fault", None.N == 0 && None.Mean == 0.0 && None.P99 == 0.0 && SeaFrames::ShareOf({}, 0.95) == 0.0, "0");
		const SeaFrames::Spread One = SeaFrames::Of({ 7.25f });
		Check("one frame: every measure is it", One.N == 1 && One.Mean == 7.25 && One.P95 == 7.25 && One.P99 == 7.25 && One.Worst == 7.25, Num(One.P99));
		// a battle's worth of frames, odd and even counts, against the rule as SayFrameTimes writes it
		Stream R{ 20261124 };
		bool bSame = true;
		std::string Where = "every share agrees";
		for (const int N : { 2, 3, 99, 100, 101, 1000, 10007, 36001 })
		{
			std::vector<float> V;
			for (int K = 0; K < N; ++K)
			{
				const double U = R.Next();
				V.push_back(static_cast<float>(U < 0.97 ? 8.0 + 3.0 * U : 12.0 + 900.0 * (U - 0.97)));
			}
			const SeaFrames::Spread Got = SeaFrames::Of(V);
			std::sort(V.begin(), V.end());
			double Sum = 0.0;
			for (const float X : V)
			{
				Sum += X;
			}
			for (const double Share : { 0.0, 0.5, 0.95, 0.99, 1.0 })
			{
				if (SeaFrames::ShareOf(V, Share) != Reference(V, Share))
				{
					bSame = false;
					Where = std::to_string(N) + " frames at " + Num(Share);
				}
			}
			if (Got.P95 != Reference(V, 0.95) || Got.P99 != Reference(V, 0.99) || Got.Worst != V.back() || std::fabs(Got.Mean - Sum / N) > 1e-9 * Sum)
			{
				bSame = false;
				Where = std::to_string(N) + " frames, the spread";
			}
		}
		Check("the 95th and 99th by the frame-time line's own rule (the value at the share of N - 1, rounded), 2 to 36,001 frames", bSame, Where);

		SeaFrames::Record Rec;
		Rec.Add(3.0, 5.0, 1.0, 4.0, 5.5, 7, 250.0);
		Rec.Add(9.0, 1.0, 40.0, 2.0, 41.0, 0, 250.1);
		Rec.Add(1.0, 2.0, 0.5, 30.0, 34.5, 12, 250.2);
		Check("the most of the three is each frame's slowest of game, render and chip; the RHI thread said beside them, not in them",
			Rec.Num() == 3 && Rec.Most[0] == 5.0f && Rec.Most[1] == 9.0f && Rec.Most[2] == 30.0f && Rec.Rhi[1] == 40.0f,
			Num(Rec.Most[0]) + ", " + Num(Rec.Most[1]) + ", " + Num(Rec.Most[2]));
		Check("the kit's ground asks kept a frame", Rec.Ground[0] == 7.0f && Rec.Ground[1] == 0.0f && Rec.Ground[2] == 12.0f, "7, 0, 12");
		Check("the whole frame kept a frame, and the battle's clock at the first and the last frame counted",
			Rec.Whole[0] == 5.5f && Rec.Whole[2] == 34.5f && Rec.FirstT == 250.0 && Rec.LastT == 250.2, Num(Rec.FirstT, 1) + " to " + Num(Rec.LastT, 1));
		Check("a frame more than 2 ms over its slowest thread of the four counted (only the third: the second's RHI was its slowest)", Rec.Over == 1, std::to_string(Rec.Over));

		SeaFrames::Record Full;
		for (std::size_t K = 0; K < SeaFrames::Record::MostFrames + 5; ++K)
		{
			Full.Add(1.0, 1.0, 1.0, 1.0, 1.0, 1, static_cast<double>(K));
		}
		Check("at most a million frames, as the frame-time line keeps", Full.Num() == SeaFrames::Record::MostFrames && Full.Most.size() == Full.Num() && Full.Ground.size() == Full.Num() &&
				Full.Rhi.size() == Full.Num() && Full.Whole.size() == Full.Num() && Full.LastT == static_cast<double>(SeaFrames::Record::MostFrames - 1),
			std::to_string(Full.Num()));
	}

	std::printf("The threads' line\n");
	{
		// a run made to known numbers: game 4..6, render 2..3, RHI 1..3, chip 7..11, the whole frame 9..15 (now and then
		// over its slowest thread), ground 0..40, from the battle's 250 s on
		SeaFrames::Record Rec;
		Stream R{ 7 };
		for (int K = 0; K < 3600; ++K)
		{
			const double U = R.Next(), W = R.Next(), Z = R.Next(), Q = R.Next();
			Rec.Add(4.0 + 2.0 * U, 2.0 + W, 1.0 + 2.0 * Q, 7.0 + 4.0 * Z, 7.0 + 4.0 * Z + 4.0 * Q, static_cast<int>(40.0 * U), 250.0 + K / 60.0);
		}
		const SeaFrames::Spread G = SeaFrames::Of(Rec.Game), D = SeaFrames::Of(Rec.Render), H = SeaFrames::Of(Rec.Rhi), C = SeaFrames::Of(Rec.Chip), M = SeaFrames::Of(Rec.Most),
								W = SeaFrames::Of(Rec.Whole), A = SeaFrames::Of(Rec.Ground);
		const std::string Line = SeaFrames::Line(Rec, 60.0);
		ThreadsRead Got;
		const bool bRead = ReadThreads(Line, Got);
		const double Want[18] = { G.Mean, G.P95, G.P99, D.Mean, D.P95, D.P99, H.Mean, H.P95, H.P99, C.Mean, C.P95, C.P99, M.Mean, M.P95, M.P99, W.Mean, W.P95, W.P99 };
		bool bNear = bRead && Got.N == 3600 && Got.From == 60.0 && Got.First == 250.0 && std::fabs(Got.Last - Rec.LastT) <= 0.5 && Got.Over == Rec.Over && Got.Over > 0 &&
					 Got.Over < 3600 && std::fabs(Got.Share - 100.0 * Rec.Over / 3600.0) <= 0.05 && Got.OverMs == 2.0 && std::fabs(Got.A[0] - A.Mean) <= 0.5 &&
					 std::fabs(Got.A[1] - A.P95) <= 0.5 && std::fabs(Got.A[2] - A.Worst) <= 0.5;
		for (int K = 0; K < 18; ++K)
		{
			bNear = bNear && std::fabs(Got.V[K] - Want[K]) <= 0.005;
		}
		Check("the line read back by its own words gives the frames, the play left out, the battle's span, every mean, 95th and 99th, and the frames over their slowest thread",
			bNear, Line);
		Check("the line ends whole, and says nothing of a graphics chip not timed, or an RHI thread at 0, when they were", !Line.empty() && Line.back() == '.' && !Has(Line, "not timed") && !Has(Line, "read 0"),
			std::to_string(Line.size()) + " characters");
		Lines << "threads\t" << Numbers(Rec, 60.0) << '\t' << Line << '\n';
		Said.push_back(Line);

		SeaFrames::Record Untimed;
		Untimed.Add(5.0, 3.0, 0.0, 0.0, 9.0, 2, 1.5);
		Untimed.Add(6.0, 3.5, 0.0, 0.0, 6.5, 4, 1.6);
		const std::string Blind = SeaFrames::Line(Untimed, 1.0);
		Check("a graphics chip that read 0 every frame is said to be not timed on this machine, and an RHI thread at 0 to be in the render thread's",
			Has(Blind, ", RHI 0.00/0.00/0.00 ms (the RHI thread read 0 every frame: its work is in the render thread's), graphics chip 0.00/0.00/0.00 ms,") &&
				Has(Blind, "(the graphics chip read 0 every frame: not timed on this machine).") && ReadThreads(Blind, Got) && Got.Over == 1,
			Blind);
		Lines << "threads\t" << Numbers(Untimed, 1.0) << '\t' << Blind << '\n';
		Said.push_back(Blind);

		SeaFrames::Record Huge;
		Huge.Add(1.0e9, 1.0e9, 1.0e9, 1.0e9, 1.0e9, 2000000000, 1.0e9);
		const std::string Big = SeaFrames::Line(Huge, 1.0e6);
		Check("the line fits its room with every number at its widest (a frame of a million seconds)", Big.size() < 1000 && Big.back() == '.' && ReadThreads(Big, Got), std::to_string(Big.size()) + " of 1,023 characters");

		const std::string Empty = SeaFrames::Line(SeaFrames::Record(), 60.0);
		Check("no frames counted: said so", Empty == "Sea battle: threads: no frames counted from 60 s of play.", Empty);
		Lines << "nothreads\t60\t" << Empty << '\n';
		Said.push_back(Empty);
	}

	std::printf("The sea's switches and the \"Sea look:\" line (Battle/SeaLookSwitches)\n");
	{
		using namespace SeaLookSwitches;
		Check("the plan's section 3: 34 switches of the release 2 sea (step 6 adds -TheaterSeaIce=, step 11 -TheaterSeaStars= and -TheaterSeaNightFight=)", Count == 34, std::to_string(Count));
		std::set<std::string> Names;
		bool bNamed = true, bFilled = true, bOnAt = true;
		const std::set<std::string> OnAts = { "1", "2", "3", "tier", "own", "with Swell", "with Day", "review", "extra", "none" };
		for (const Switch& One : All)
		{
			const std::string Name = One.Name;
			Names.insert(Lower(Name));
			bNamed = bNamed && Name.rfind("TheaterSea", 0) == 0 && Name.find(' ') == std::string::npos && Name.find('=') == (TakesValue(One) ? Name.size() - 1 : std::string::npos);
			bFilled = bFilled && std::string(One.Today).size() > 0 && std::string(One.Step).size() > 0;
			bOnAt = bOnAt && OnAts.count(One.OnAt) == 1;
		}
		Check("every name its own (whatever the case), each starting TheaterSea, a value's ending in its one '='", bNamed && static_cast<int>(Names.size()) == Count, std::to_string(Names.size()) + " names");
		Check("every switch has today's value and the step that builds it", bFilled, "32");
		Check("every switch turned on by a tier, the tier itself, its own, another switch, review pictures, one at a time or nothing", bOnAt, "32");
		int Collide = 0;
		std::string Which;
		for (const Switch& A : All)
		{
			for (const Switch& B : All)
			{
				// (the engine's value reader finds a value's name with its '=' anywhere on the line; a flag only as a whole word)
				if (&A != &B && TakesValue(A) && Has("-" + Lower(B.Name) + "1", Lower(A.Name)))
				{
					++Collide;
					Which = std::string(A.Name) + " in " + B.Name;
				}
			}
		}
		Check("no name with its '=' inside another's, as the engine's value reader would find it", Collide == 0, Collide ? Which : "none");
		std::string Built;
		for (const Switch& One : All)
		{
			Built += One.bBuilt ? (Built.empty() ? "" : ", ") + std::string(One.Name) : "";
		}
		Check("built so far: step 1(a)'s -TheaterSeaMarksPool=, step 11's -TheaterSeaDay=, -TheaterSeaVisContrast= and -TheaterSeaStars=, step 6's -TheaterSeaSpray= and -TheaterSeaIce=, step 11's -TheaterSeaNightFight=, step 13's five weather switches (each lands with its step; step 1(b)'s -TheaterSeaFarEdge= not yet)",
			Built == "TheaterSeaMarksPool=, TheaterSeaDay=, TheaterSeaVisContrast=, TheaterSeaStars=, TheaterSeaSpray=, TheaterSeaIce=, TheaterSeaNightFight=, TheaterSeaClouds=, TheaterSeaCloudShade=, TheaterSeaRain=, TheaterSeaSkyStandIn=, TheaterSeaWeather=",
			Built.empty() ? "none" : Built);

		const std::string Plain = Line(0, {});
		Check("the line with nothing asked: tier 0, release 1.5's sea as made, every switch at today's value",
			Plain == "Sea look: tier 0 (release 1.5's sea, as made); 12 of the release 2 sea's 34 switches built, in force: -TheaterSeaMarksPool=0, -TheaterSeaDay=seed, -TheaterSeaVisContrast=0.05, -TheaterSeaStars=0, -TheaterSeaSpray=0, -TheaterSeaIce=0, -TheaterSeaNightFight=0, -TheaterSeaClouds=0, -TheaterSeaCloudShade=0, -TheaterSeaRain=0, -TheaterSeaSkyStandIn=0, -TheaterSeaWeather=none.", Plain);
		Lines << "look\t0||\t" << Plain << '\n';
		Said.push_back(Plain);

		// asked out of the table's order: said in the table's order, as asked for and not built
		std::vector<Asked> Given(2);
		for (int K = 0; K < Count; ++K)
		{
			Given[0].Index = std::string(All[K].Name) == "TheaterSeaProbe" ? K : Given[0].Index;
			Given[1].Index = std::string(All[K].Name) == "TheaterSeaLook=" ? K : Given[1].Index;
		}
		Given[1].Value = "2";
		const std::string AskedLine = Line(0, Given);
		Check("switches asked for before their step lands: said, in the table's order, to change nothing",
			AskedLine == "Sea look: tier 0 (release 1.5's sea, as made); 12 of the release 2 sea's 34 switches built, in force: -TheaterSeaMarksPool=0, -TheaterSeaDay=seed, -TheaterSeaVisContrast=0.05, -TheaterSeaStars=0, -TheaterSeaSpray=0, -TheaterSeaIce=0, -TheaterSeaNightFight=0, -TheaterSeaClouds=0, -TheaterSeaCloudShade=0, -TheaterSeaRain=0, -TheaterSeaSkyStandIn=0, -TheaterSeaWeather=none; "
						 "asked for but not built yet, so nothing changes: -TheaterSeaLook=2, -TheaterSeaProbe.", AskedLine);
		Lines << "look\t0|-TheaterSeaLook=2, -TheaterSeaProbe|\t" << AskedLine << '\n';
		Said.push_back(AskedLine);
		std::vector<Asked> Bench(1);
		for (int K = 0; K < Count; ++K)
		{
			Bench[0].Index = std::string(All[K].Name) == "TheaterSeaLook=" ? K : Bench[0].Index;
		}
		Bench[0].Value = "1";
		const std::string BenchLine = Line(0, Bench);
		Lines << "look\t0|-TheaterSeaLook=1|\t" << BenchLine << '\n';
		Said.push_back(BenchLine);

		// step 1(a)'s switch as the scene takes it (OpenOn, TakeOnOff): on only when asked for as 1, its value in force
		// written back, so the line says what was applied (the bench's A/B of the marks' meshes reads it)
		auto PoolAsked = [](const char* Value)
		{
			Look Asks;
			for (int K = 0; K < Count; ++K)
			{
				if (std::string(All[K].Name) == "TheaterSeaMarksPool=")
				{
					Asks.Given.push_back(Asked{ K, Value });
				}
			}
			return Asks;
		};
		Look On = PoolAsked("1"), Junk = PoolAsked("yes"), None;
		const bool bOn = TakeOnOff(On, "TheaterSeaMarksPool="), bJunk = TakeOnOff(Junk, "TheaterSeaMarksPool="), bNone = TakeOnOff(None, "TheaterSeaMarksPool=");
		const std::string PoolLine = Line(On), JunkLine = Line(Junk);
		Check("-TheaterSeaMarksPool=1 keeps the marks' meshes and says so; anything else asked is today's, said as 0; not asked, today's and said as today's",
			bOn && !bJunk && !bNone
				&& PoolLine == "Sea look: tier 0 (release 1.5's sea, as made); 12 of the release 2 sea's 34 switches built, in force: -TheaterSeaMarksPool=1 (asked), -TheaterSeaDay=seed, -TheaterSeaVisContrast=0.05, -TheaterSeaStars=0, -TheaterSeaSpray=0, -TheaterSeaIce=0, -TheaterSeaNightFight=0, -TheaterSeaClouds=0, -TheaterSeaCloudShade=0, -TheaterSeaRain=0, -TheaterSeaSkyStandIn=0, -TheaterSeaWeather=none."
				&& JunkLine == "Sea look: tier 0 (release 1.5's sea, as made); 12 of the release 2 sea's 34 switches built, in force: -TheaterSeaMarksPool=0 (asked), -TheaterSeaDay=seed, -TheaterSeaVisContrast=0.05, -TheaterSeaStars=0, -TheaterSeaSpray=0, -TheaterSeaIce=0, -TheaterSeaNightFight=0, -TheaterSeaClouds=0, -TheaterSeaCloudShade=0, -TheaterSeaRain=0, -TheaterSeaSkyStandIn=0, -TheaterSeaWeather=none."
				&& Line(None) == Plain,
			PoolLine);
		Lines << "look\t0||-TheaterSeaMarksPool=1 (asked)\t" << PoolLine << '\n';
		Said.push_back(PoolLine);

		// a table of the test's own, some built: what a later step's line says
		const Switch Mine[] = {
			{ "TheaterSeaA=", "5", "1", "1", true },
			{ "TheaterSeaB", "off", "extra", "14", true },
			{ "TheaterSeaC=", "0", "2", "11", false },
			{ "TheaterSeaD=", "none", "own", "3", true },
		};
		std::vector<Asked> Some(2);
		Some[0].Index = 0;
		Some[0].Value = "7";
		Some[1].Index = 2;
		Some[1].Value = "1";
		const std::string Later = LineOf(Mine, 4, 1, Some);
		Check("built switches said with their value in force, asked or today's, and a flag's off",
			Later == "Sea look: tier 1; 3 of the release 2 sea's 4 switches built, in force: -TheaterSeaA=7 (asked), -TheaterSeaB off, -TheaterSeaD=none; "
					 "asked for but not built yet, so nothing changes: -TheaterSeaC=1.", Later);
		Lines << "look\t1|-TheaterSeaC=1|-TheaterSeaA=7 (asked), -TheaterSeaB off, -TheaterSeaD=none\t" << Later << '\n';
		Said.push_back(Later);
		std::vector<Asked> Flag(1);
		Flag[0].Index = 1;
		const std::string Flagged = LineOf(Mine, 4, 3, Flag);
		Check("a built flag asked for: said asked", Has(Flagged, "Sea look: tier 3; 3 of the release 2 sea's 4 switches built, in force: -TheaterSeaA=5, -TheaterSeaB (asked), -TheaterSeaD=none.") && !Has(Flagged, "not built"), Flagged);
		Lines << "look\t3||-TheaterSeaA=5, -TheaterSeaB (asked), -TheaterSeaD=none\t" << Flagged << '\n';
		Said.push_back(Flagged);
		Check("a value spelled as on the command line, a flag's after a space",
			Spelled(Mine[0], "7") == "-TheaterSeaA=7" && Spelled(Mine[1], "") == "-TheaterSeaB" && Spelled(Mine[1], "off") == "-TheaterSeaB off", Spelled(Mine[1], "off"));

		// the look read once as the scene opens (OpenOn), from whatever asks: here a list standing for the command line
		const std::vector<std::pair<std::string, std::string>> CommandLine = { { "TheaterSeaD=", "dusk" }, { "TheaterSeaB", "stray" }, { "TheaterSeaA=", "7" } };
		int Asks = 0;
		const Look Opened = ReadOf(Mine, 4, [&CommandLine, &Asks](const Switch& One, std::string& Value)
			{
				++Asks;
				for (const auto& Typed : CommandLine)
				{
					if (Typed.first == One.Name)
					{
						Value = Typed.second;
						return true;
					}
				}
				return false;
			});
		Check("the look read once: every switch asked about once, those given kept in the table's order, a flag with no value, the tier 0",
			Asks == 4 && Opened.Tier == 0 && Opened.Given.size() == 3 && Opened.Given[0].Index == 0 && Opened.Given[0].Value == "7" && Opened.Given[1].Index == 1 &&
				Opened.Given[1].Value.empty() && Opened.Given[2].Index == 3 && Opened.Given[2].Value == "dusk",
			std::to_string(Opened.Given.size()) + " given");
		Look Stepped = Opened;
		Stepped.Tier = 2;
		Check("the line said from the look as applied (a tier stepped down, say), not from the command line again",
			LineOf(Mine, 4, Stepped.Tier, Stepped.Given) == "Sea look: tier 2; 3 of the release 2 sea's 4 switches built, in force: -TheaterSeaA=7 (asked), -TheaterSeaB (asked), -TheaterSeaD=dusk (asked).",
			LineOf(Mine, 4, Stepped.Tier, Stepped.Given));
		const Look Bare = Read([](const Switch&, std::string&) { return false; });
		Check("nothing asked: the release 2 sea's line as release 1.5's sea, as made", Line(Bare) == Plain && Bare.Tier == 0, Line(Bare));
	}

	std::printf("The game's own lines that feed them (Battle/SeaBattle.cpp, .h)\n");
	{
		const std::string Cpp = ReadAll(Src + "/Battle/SeaBattle.cpp"), H = ReadAll(Src + "/Battle/SeaBattle.h");
		Check("the sea battle's source read", Cpp.size() > 100000 && H.size() > 1000, std::to_string(Cpp.size()) + " bytes");
		const std::size_t Kit = Cpp.find("if (!Effects->Begin(Root, UnitsPerMetre, [this](const FVector& Point)");
		const std::size_t Deck = Kit == std::string::npos ? std::string::npos : Cpp.find("FVector Deck;", Kit);
		const std::size_t Ask = Kit == std::string::npos ? std::string::npos : Cpp.find("++GroundAsks;", Kit);
		Check("the kit's ground counts each ask, first thing in it", Kit != std::string::npos && Ask != std::string::npos && Ask < Deck && Deck - Kit < 400, Kit == std::string::npos ? "the kit's ground not found" : "found");
		const std::size_t Frame = Cpp.find("FrameTimes.Add(static_cast<float>(Took));");
		const std::size_t Add = Cpp.find("Threads.Add(FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), "
										 "FPlatformTime::ToMilliseconds(GRHIThreadTime),\n\t\t\t\tFPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()), 1000.0 * Took, GroundAsks, S.BattleT);");
		const std::size_t From = Cpp.find("if (RealT >= ThreadsFrom)", Frame == std::string::npos ? 0 : Frame);
		const std::size_t Reset = Add == std::string::npos ? std::string::npos : Cpp.find("GroundAsks = 0;", Add);
		const std::size_t Next = Cpp.find("RealT += Dt;", Frame == std::string::npos ? 0 : Frame);
		Check("each counted frame's game, render, RHI and chip times kept with its whole frame, its ground asks and the battle's clock, from the play the bench leaves out, the asks then cleared",
			Frame != std::string::npos && From != std::string::npos && Add != std::string::npos && From < Add && Add - Frame < 600 && Reset != std::string::npos && Reset < Next, "Tick");
		const std::size_t Say = Cpp.find("void ASeaBattle::SayFrameTimes()");
		const std::size_t Times = Cpp.find("Sea battle: frame times over %d frames", Say == std::string::npos ? 0 : Say);
		const std::size_t Then = Cpp.find("SayThreadsAndLook(); // (the release 2 ocean plan, step 0)", Say == std::string::npos ? 0 : Say);
		const std::size_t UnderToo = Cpp.find("Under->SayFrameTimes();", Say == std::string::npos ? 0 : Say);
		Check("the threads' line and the \"Sea look:\" line said once a battle, after the frame-time line (the month scene's too: its EndPlay says it)",
			Say != std::string::npos && Times != std::string::npos && Then != std::string::npos && Times < Then && Then < UnderToo && Has(Cpp, "void ASeaBattle::EndPlay(const EEndPlayReason::Type Reason)\n{\n\tSayFrameTimes();"),
			"SayFrameTimes");
		const std::size_t Body = Cpp.find("void ASeaBattle::SayThreadsAndLook() const");
		const std::string Rest = Body == std::string::npos ? "" : Cpp.substr(Body, 1500);
		const std::size_t Open = Cpp.find("bool ASeaBattle::OpenOn(const FString& Text, const FString& InKey, const FString& Where)\n{");
		const std::string Opening = Open == std::string::npos ? "" : Cpp.substr(Open, 1200);
		Check("the switches read once as the scene opens, as the engine reads them, a value to the next space, a flag whole; the line said from what was read",
			Has(Rest, "SeaFrames::Line(Threads, ThreadsFrom)") && Has(Rest, "SeaLookSwitches::Line(SeaLook)") && !Has(Rest, "FCommandLine") &&
				Has(Opening, "SeaLook = SeaLookSwitches::Read(") &&
				Has(Opening, "SeaLookSwitches::TakesValue(One) ? FParse::Value(FCommandLine::Get(), *Name, Got, false) : FParse::Param(FCommandLine::Get(), *Name)") &&
				Opening.find("SeaLook = ") < Opening.find("SeaFeed::Recording Rec;") && Has(H, "SeaLookSwitches::Look SeaLook;") && Has(H, "#include \"Battle/SeaLookSwitches.h\""),
			Body == std::string::npos || Open == std::string::npos ? "SayThreadsAndLook or OpenOn not found" : "OpenOn, SayThreadsAndLook");
		Check("the bench's run: counted from its drop, over at its span or the battle's end (\"drop,end\"), read only from the command line",
			Has(Cpp, "FParse::Value(FCommandLine::Get(), TEXT(\"TheaterSeaBench=\"), Bench, false)") && Has(Cpp, "Quit(TEXT(\"the bench run is over\"));") &&
				Has(Cpp, "BenchUntil = BenchSpan > 0.0 ? ThreadsFrom + FMath::Max(10.0, BenchSpan) : 1.0e12;") &&
				Has(Cpp, "if (BenchUntil > 0.0 && (RealT >= BenchUntil || S.BattleT >= S.EndT + 12.0))") && Has(H, "double ThreadsFrom = 1.0;") && Has(H, "double BenchUntil = -1.0;"),
			"-TheaterSeaBench=");
		const std::size_t Failed = Cpp.find("if (!BeginReplay(Asked))");
		const std::string Failing = Failed == std::string::npos ? "" : Cpp.substr(Failed, 700);
		Check("a bench run whose recorded action cannot be played quits at once, not left to the bench's watchdog",
			Has(Failing, "else if (BenchUntil > 0.0)") && Has(Failing, "Quit(TEXT(\"the bench's recorded action could not be played\"));") &&
				Failing.find("Quit(TEXT(\"the bench's recorded action") < Failing.find("SetActorTickEnabled(false);"),
			Failed == std::string::npos ? "BeginReplay's failure not found" : "Tick");
		Check("the threads' times read as the engine's own speed log reads them (milliseconds from the cycle counts)",
			Has(Cpp, "#include \"RenderTimer.h\"") && Has(Cpp, "#include \"RHI.h\"") && Has(H, "#include \"Battle/SeaFrames.h\""), "includes");
	}

	std::printf("Words\n");
	{
		int Dice = 0;
		for (const std::string& One : Said)
		{
			Dice += Has(Lower(One), "dice") ? 1 : 0;
		}
		Check("no line says dice", Dice == 0, std::to_string(Said.size()) + " lines");
	}

	std::printf("%d checks, %d failed\n%s\n", Checks, Failed, Failed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failed ? 1 : 0;
}
