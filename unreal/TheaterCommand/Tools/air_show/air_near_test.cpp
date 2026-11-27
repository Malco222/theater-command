// The finer B-17's close-ups made a few a frame, the nearest first (Battle/AirNear). Usage: air_near_test <Source dir>.
//
// The law is played on the stock raid's box as the box eye sees it (11 of 12 bombers inside 300 m) and on the 48-bomber
// raid's near half (20 inside 300 m, the Mac's worst burst), frame after frame as the game plays it: each frame the
// chosen are made, and the next frame asks again. The game's side is read as text: the word, the guard's one line and
// Draw's choice before its loop.
#include "Battle/AirNear.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Failed = 0;

	void Check(bool bOk, const std::string& What)
	{
		std::printf("  %s  %s\n", bOk ? "ok   " : "FAIL ", What.c_str());
		Failed += bOk ? 0 : 1;
	}

	int Count(const std::string& Text, const std::string& Find)
	{
		int N = 0;
		for (size_t At = Text.find(Find); At != std::string::npos; At = Text.find(Find, At + Find.size()))
		{
			++N;
		}
		return N;
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		std::stringstream Out;
		Out << In.rdbuf();
		return Out.str();
	}

	/** A box: N bombers, the I-th at Near + I * Step metres, keyed in an order that is not their distance. */
	std::vector<AirNear::FSeen> Box(int N, double Near, double Step)
	{
		std::vector<AirNear::FSeen> Out;
		for (int I = 0; I < N; ++I)
		{
			AirNear::FSeen One;
			One.Key = (I * 7 + 3) % N + 100;
			One.Away = Near + I * Step;
			Out.push_back(One);
		}
		return Out;
	}

	/** Frames played as the game plays them: each frame the chosen are made (taken out of the unmade). Returns how many
	 * were made in each frame until none is chosen. */
	std::vector<int> Play(std::vector<AirNear::FSeen> Unmade, int Made, int Most, int PerFrame, std::vector<int>* Order = nullptr)
	{
		std::vector<int> Frames;
		for (int Frame = 0; Frame < 1000; ++Frame)
		{
			const std::vector<int> Chosen = AirNear::MayMake(Unmade, Made, Most, PerFrame, 300.0);
			if (Chosen.empty())
			{
				break;
			}
			Frames.push_back(static_cast<int>(Chosen.size()));
			Made += static_cast<int>(Chosen.size());
			for (const int Key : Chosen)
			{
				if (Order) Order->push_back(Key);
				Unmade.erase(std::remove_if(Unmade.begin(), Unmade.end(), [Key](const AirNear::FSeen& S) { return S.Key == Key; }), Unmade.end());
			}
		}
		return Frames;
	}
}

int main(int Argc, char** Argv)
{
	std::printf("air near: the finer B-17's close-ups made a few a frame, the nearest first\n");

	// (a) the stock raid's box from the box eye: 12 bombers, 11 inside 300 m (the designer's probe), the guard at 8
	{
		std::vector<AirNear::FSeen> B = Box(12, 40.0, 25.0); // 40 .. 315 m: 11 inside 300 m
		const std::vector<int> Chosen = AirNear::MayMake(B, 0, 8, 1000, 300.0);
		std::vector<AirNear::FSeen> Sorted = B;
		std::sort(Sorted.begin(), Sorted.end(), [](const AirNear::FSeen& A, const AirNear::FSeen& C) { return A.Away < C.Away; });
		bool bNearest = Chosen.size() == 8;
		for (size_t I = 0; bNearest && I < Chosen.size(); ++I)
		{
			bNearest = Chosen[I] == Sorted[I].Key;
		}
		Check(bNearest, "the guard at 8: the 8 nearest are chosen, nearest first, not the first 8 in the battle's order");
		Check(AirNear::MayMake(B, 0, 1000, 1000, 300.0).size() == 11, "the guard at 1000: all 11 inside 300 m, none past it");
		Check(AirNear::MayMake(B, 0, 0, 1000, 300.0).empty() && AirNear::MayMake(B, 8, 8, 1000, 300.0).empty(), "no room (the guard at 0, or 8 made of 8): none");
		Check(AirNear::MayMake(B, 0, 8, 0, 300.0).empty(), "the word off (0 a frame): none chosen, so the game asks its old guard");
	}
	// (b) the order the battle lists them in never matters
	{
		std::vector<AirNear::FSeen> B = Box(20, 30.0, 12.0);
		const std::vector<int> First = AirNear::MayMake(B, 2, 1000, 3, 300.0);
		std::mt19937 Dice(1127);
		bool bSame = true;
		for (int Shuffle = 0; Shuffle < 50 && bSame; ++Shuffle)
		{
			std::shuffle(B.begin(), B.end(), Dice);
			bSame = AirNear::MayMake(B, 2, 1000, 3, 300.0) == First;
		}
		// (two at one distance: the smaller key first)
		std::vector<AirNear::FSeen> Tie = { { 9, 50.0, false }, { 4, 50.0, false }, { 7, 20.0, false } };
		const std::vector<int> Tied = AirNear::MayMake(Tie, 0, 10, 2, 300.0);
		Check(bSame && Tied == std::vector<int>({ 7, 4 }), "shuffled 50 times, the same choice; at one distance the smaller key first");
	}
	// (c) the Mac's worst burst: 20 inside 300 m at once (the 48-bomber raid's near half, t 2.0), the guard at 1000
	{
		const std::vector<AirNear::FSeen> B = Box(20, 30.0, 13.0);
		const std::vector<int> Old = Play(B, 0, 1000, 1000);
		std::vector<int> Order;
		const std::vector<int> Two = Play(B, 0, 1000, 2, &Order);
		const bool bTwo = Two.size() == 10 && std::all_of(Two.begin(), Two.end(), [](int N) { return N == 2; });
		bool bNearFirst = Order.size() == 20;
		for (size_t I = 1; bNearFirst && I < Order.size(); ++I)
		{
			double Was = 0.0, Is = 0.0;
			for (const AirNear::FSeen& S : B)
			{
				if (S.Key == Order[I - 1]) Was = S.Away;
				if (S.Key == Order[I]) Is = S.Away;
			}
			bNearFirst = Was <= Is;
		}
		Check(Old.size() == 1 && Old[0] == 20, "no limit: all 20 in one frame (the 201 ms frame)");
		Check(bTwo && bNearFirst, "2 a frame: 20 made over 10 frames (a sixth of a second at 60 fps), 2 in each, the nearest first");
		const std::vector<int> One = Play(B, 0, 8, 1);
		Check(One.size() == 8, "1 a frame with the guard at 8: 8 frames of one, then none");
	}
	// (d) the ridden: never chosen here (the game makes hers whatever the law says), but counted when made
	{
		std::vector<AirNear::FSeen> B = Box(6, 20.0, 10.0);
		B[0].bRidden = true;
		const std::vector<int> Chosen = AirNear::MayMake(B, 0, 1000, 1000, 300.0);
		Check(Chosen.size() == 5 && std::find(Chosen.begin(), Chosen.end(), B[0].Key) == Chosen.end(), "the ridden one is never chosen (she has hers anyway)");
		Check(AirNear::MayMake(B, 3, 4, 1000, 300.0).size() == 1, "made ones (ridden or not, as the old guard counts) use up the room");
	}

	// The game's side, as text
	if (Argc > 1)
	{
		const std::string Src = Argv[1];
		const std::string Battle = ReadAll(Src + "/Battle/AirBattle.cpp");
		const std::string Head = ReadAll(Src + "/Battle/AirBattle.h");
		Check(Count(Battle, "#include \"Battle/AirNear.h\"") == 1 && Count(Battle, "TEXT(\"TheaterAirB17FinePerFrame=\")") == 1
				&& Count(Battle, "int32 Asked = 0;\n\t\tFParse::Value(FCommandLine::Get(), TEXT(\"TheaterAirB17FinePerFrame=\"), Asked);") == 1,
			"the word -TheaterAirB17FinePerFrame=N, read once, 0 unless asked");
		Check(Count(Battle, "if (B17FinePerFrame() > 0 ? Made.bMayMake : Kept < B17FineNearMost())") == 1
				&& Count(Battle, "if (!Made.bFine || Made.bNearMade)") == 1,
			"the guard: the word off asks the old count, on only those Draw chose; anything not a finer B-17, or made, as before");
		Check(Count(Battle, "if (S.B17FineOn == 1 && B17FinePerFrame() > 0)") == 1
				&& Count(Battle, "AirNear::MayMake(Fine, FineMade, B17FineNearMost(), B17FinePerFrame(), 300.0)") == 1
				&& Count(Battle, "Each.Value.bMayMake = false;") == 1,
			"Draw chooses before its loop only with Finer B-17 on and the word given, every choice cleared first");
		const size_t Choose = Battle.find("AirNear::MayMake(Fine, FineMade");
		const size_t Loop = Battle.find("for (const AirShow::PlaneOut& P : S.Show.Planes())\n\t{\n\t\tSeen.Add(P.Key);");
		Check(Choose != std::string::npos && Loop != std::string::npos && Choose < Loop, "the choice comes before the frame's loop that makes them");
		Check(Count(Battle, "One.Away = FVector::Dist(World(P.At.X, P.At.Y, P.At.Z), EyeAt) / U;") == 1
				&& Count(Battle, "const double Away = FVector::Dist(Where, EyeAt) / U;") == 1
				&& Count(Battle, "Away < (Lit->bNear ? 450.0 : 300.0)") == 1,
			"her distance measured as the loop measures it, and 300 m as the loop's first near distance");
		Check(Count(Head, "bool bMayMake = false;") == 1 && Count(Head, "static int32 B17FinePerFrame();") == 1, "her mark and the word's reader declared");
	}
	else
	{
		std::printf("  (no Source dir: the game's text not read)\n");
	}

	if (Failed == 0)
	{
		std::printf("air near: all checks pass\n");
	}
	else
	{
		std::printf("air near: %d FAILED\n", Failed);
	}
	return Failed == 0 ? 0 : 1;
}
