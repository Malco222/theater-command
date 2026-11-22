// The live ground battle's cloud test (release 2): the game's own LandFeed grows a battle delta by delta as the rules
// fight it (src/battle/land/live.ts, written out by deltas.ts), and the test checks what the 3D readers will rely on:
//  - at every delta, every man sampled at every step already fought is where the battle read whole has him (within the
//    two kept points' slack, 0.1 m each, and the tenth of a metre they are rounded to);
//  - once over, the battle grown is the battle read whole: every track's kept points, changes, shots, fall, out and
//    scars, the rounds, events, views, phases, craters, contact and end, and no provisional point left;
//  - the fire (LandPlay) played from the live battle two battle seconds behind the rules, frame by frame, is the fire
//    played from the battle read whole: every muzzle flash, burst, landing, blast, fall and sound source, at the same
//    times, from within half a metre of the same places;
//  - a delta that names a track out of turn is refused.
// Usage: land_live_test <folder with header.json, deltas.jsonl, whole.json>

#include "Battle/LandFeed.h"
#include "Battle/LandJson.h"
#include "Battle/LandPlay.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Failures = 0;

	void Check(bool bOk, const std::string& What)
	{
		if (bOk) return;
		++Failures;
		if (Failures <= 30) std::printf("FAIL: %s\n", What.c_str());
	}

	bool Near(double A, double B, double Tol = 1e-9) { return std::fabs(A - B) <= Tol; }
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::printf("usage: land_live_test <folder>\n");
		return 2;
	}
	const std::string Folder = Argv[1];
	std::string HeaderText, WholeText, Error;
	if (!LandJson::ReadFile(Folder + "/header.json", HeaderText) || !LandJson::ReadFile(Folder + "/whole.json", WholeText))
	{
		std::printf("cannot read %s/header.json or whole.json\n", Folder.c_str());
		return 2;
	}
	LandFeed::Recording Live, Whole;
	if (!LandFeed::ReadRecording(HeaderText, Live, Error)) { std::printf("header: %s\n", Error.c_str()); return 1; }
	if (!LandFeed::ReadRecording(WholeText, Whole, Error)) { std::printf("whole: %s\n", Error.c_str()); return 1; }
	Check(Live.bLive, "the header reads as live");
	Check(!Whole.bLive, "the whole reads as a recording");
	Check(Live.Tracks.empty() && Live.Rounds.empty() && Live.Views.empty(), "the header has nothing fought");
	Check(Live.Blocks.size() == Whole.Blocks.size(), "the header names every block");

	// The fire, played both ways a frame at a time (60 frames a second, the battle's clock at half life's).
	LandPlay::Replay PlayLive, PlayWhole;
	PlayLive.Begin(Live, LandPlay::World());
	PlayWhole.Begin(Whole, LandPlay::World());
	double Shown = 0.0;
	long Frames = 0, FxSeen = 0, NoiseSeen = 0, LossSeen = 0, FarFx = 0;
	double FxWorst = 0.0;
	auto PlayTo = [&](double Until)
	{
		std::vector<LandPlay::Effect> FxA, FxB;
		std::vector<LandPlay::Noise> NoA, NoB;
		std::vector<LandPlay::Loss> LoA, LoB;
		while (Shown + 1e-9 < Until)
		{
			const double To = std::min(Until, Shown + 1.0 / 120.0);
			FxA.clear(); FxB.clear(); NoA.clear(); NoB.clear(); LoA.clear(); LoB.clear();
			PlayLive.Step(Shown, To, FxA, NoA, LoA);
			PlayWhole.Step(Shown, To, FxB, NoB, LoB);
			++Frames;
			bool bSame = FxA.size() == FxB.size() && NoA.size() == NoB.size() && LoA.size() == LoB.size();
			for (size_t I = 0; bSame && I < FxA.size(); ++I)
			{
				const LandPlay::Effect& A = FxA[I];
				const LandPlay::Effect& B = FxB[I];
				bSame = Near(A.T, B.T) && A.Kind == B.Kind && A.Round == B.Round && A.Name == B.Name && A.Rounds == B.Rounds;
				const double D = (A.At - B.At).Length();
				FxWorst = std::max(FxWorst, D);
				if (D > 0.5) ++FarFx;
			}
			for (size_t I = 0; bSame && I < NoA.size(); ++I) bSame = Near(NoA[I].T, NoB[I].T) && NoA[I].Kind == NoB[I].Kind && NoA[I].Track == NoB[I].Track;
			for (size_t I = 0; bSame && I < LoA.size(); ++I) bSame = Near(LoA[I].T, LoB[I].T) && LoA[I].Track == LoB[I].Track && LoA[I].By == LoB[I].By;
			if (!bSame) Check(false, "the fire differs in the frame to " + std::to_string(To) + ": " + std::to_string(FxA.size()) + "/" + std::to_string(FxB.size()) + " effects, "
				+ std::to_string(NoA.size()) + "/" + std::to_string(NoB.size()) + " sounds, " + std::to_string(LoA.size()) + "/" + std::to_string(LoB.size()) + " losses");
			FxSeen += static_cast<long>(FxB.size());
			NoiseSeen += static_cast<long>(NoB.size());
			LossSeen += static_cast<long>(LoB.size());
			Shown = To;
		}
	};

	std::ifstream In(Folder + "/deltas.jsonl");
	std::string Line;
	int Deltas = 0;
	double PrevT = -1.0, Worst = 0.0, AppendMs = 0.0, MostMs = 0.0;
	long Samples = 0;
	size_t MostBytes = 0;
	while (std::getline(In, Line))
	{
		if (Line.empty()) continue;
		LandFeed::Grown G;
		const auto A = std::chrono::steady_clock::now();
		const bool bOk = LandFeed::AppendDelta(Line, Live, G, Error);
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - A).count();
		AppendMs += Ms;
		MostMs = std::max(MostMs, Ms);
		MostBytes = std::max(MostBytes, Line.size());
		if (!bOk) { std::printf("delta %d: %s\n", Deltas, Error.c_str()); return 1; }
		++Deltas;
		PlayLive.Extend(G);
		// The screen two battle seconds behind the rules (to the end once it is over).
		PlayTo(Live.bOver ? Live.End + 8.0 : std::max(0.0, G.T - 2.0));
		Check(Live.Fought == G.T, "Fought follows the delta's t");
		Check(G.T >= PrevT, "the deltas go forward");
		// Every man at every step fought since the last delta (and one step before it), sampled from what has come so far,
		// against the battle read whole.
		for (size_t I = 0; I < Live.Tracks.size() && I < Whole.Tracks.size(); I += 3)
		{
			const LandFeed::Track& T = Live.Tracks[I];
			const LandFeed::Track& W = Whole.Tracks[I];
			for (double S = std::max(T.Start(), PrevT - 0.1); S <= G.T + 1e-9; S += 0.1)
			{
				if (S < T.Start() - 1e-9) continue;
				if (W.Out >= 0.0 && S >= W.Out) continue;
				const LandFeed::Spot P = LandFeed::SpotAt(T, S), Q = LandFeed::SpotAt(W, S);
				const double D = std::hypot(P.X - Q.X, P.Y - Q.Y);
				Worst = std::max(Worst, D);
				++Samples;
				if (D > 0.25) Check(false, "track " + std::to_string(I) + " at " + std::to_string(S) + " is " + std::to_string(D) + " m from the whole");
			}
		}
		PrevT = G.T;
	}
	Check(Deltas > 0, "deltas were read");
	Check(Live.bOver, "the last delta ends the battle");
	Check(Samples > 10000, "enough samples");

	// Once over: the battle grown is the battle read whole.
	Check(Live.Tracks.size() == Whole.Tracks.size(), "tracks " + std::to_string(Live.Tracks.size()) + " v " + std::to_string(Whole.Tracks.size()));
	int TailsLeft = 0;
	for (size_t I = 0; I < Live.Tracks.size() && I < Whole.Tracks.size(); ++I)
	{
		const LandFeed::Track& T = Live.Tracks[I];
		const LandFeed::Track& W = Whole.Tracks[I];
		const std::string Who = "track " + std::to_string(I) + " (" + std::to_string(T.Block) + "," + std::to_string(T.K) + "," + std::to_string(T.J) + " " + T.Role + ")";
		TailsLeft += T.bTail ? 1 : 0;
		Check(T.Block == W.Block && T.K == W.K && T.J == W.J && T.Role == W.Role && T.Fo == W.Fo, Who + ": who");
		Check(T.At.size() == W.At.size(), Who + ": points " + std::to_string(T.At.size()) + " v " + std::to_string(W.At.size()));
		for (size_t P = 0; P < T.At.size() && P < W.At.size(); ++P)
		{
			const LandFeed::Point& A = T.At[P];
			const LandFeed::Point& B = W.At[P];
			if (!(Near(A.T, B.T) && Near(A.X, B.X) && Near(A.Y, B.Y) && Near(A.F, B.F))) { Check(false, Who + ": point " + std::to_string(P)); break; }
		}
		Check(T.Ch.size() == W.Ch.size(), Who + ": changes");
		for (size_t C = 0; C < T.Ch.size() && C < W.Ch.size(); ++C)
		{
			const LandFeed::Change& A = T.Ch[C];
			const LandFeed::Change& B = W.Ch[C];
			if (!(Near(A.T, B.T) && A.Pose == B.Pose && A.Cover == B.Cover && A.bAim == B.bAim && Near(A.AimX, B.AimX) && Near(A.AimY, B.AimY)
				&& A.bHidden == B.bHidden && A.bButtoned == B.bButtoned)) { Check(false, Who + ": change " + std::to_string(C)); break; }
		}
		Check(T.Shots == W.Shots, Who + ": shots");
		Check(Near(T.Fell, W.Fell), Who + ": fell");
		Check(Near(T.Out, W.Out), Who + ": out");
		Check(T.Marks.size() == W.Marks.size(), Who + ": scars");
		Check(T.bTwin == W.bTwin, Who + ": twin");
	}
	Check(TailsLeft == 0, std::to_string(TailsLeft) + " provisional points left at the end");
	Check(Live.Rounds.size() == Whole.Rounds.size(), "rounds " + std::to_string(Live.Rounds.size()) + " v " + std::to_string(Whole.Rounds.size()));
	for (size_t I = 0; I < Live.Rounds.size() && I < Whole.Rounds.size(); ++I)
	{
		const LandFeed::Round& A = Live.Rounds[I];
		const LandFeed::Round& B = Whole.Rounds[I];
		if (!(A.N == B.N && Near(A.T, B.T) && Near(A.L, B.L) && A.By == B.By && A.K == B.K && A.J == B.J && A.W == B.W && A.On == B.On && A.Ok == B.Ok
			&& A.Oj == B.Oj && Near(A.X, B.X) && Near(A.Y, B.Y) && A.R == B.R && A.Fc == B.Fc)) { Check(false, "round " + std::to_string(I)); break; }
	}
	Check(Live.Events.size() == Whole.Events.size(), "events");
	for (size_t I = 0; I < Live.Events.size() && I < Whole.Events.size(); ++I)
		Check(Near(Live.Events[I].T, Whole.Events[I].T) && Live.Events[I].What == Whole.Events[I].What && Live.Events[I].Text == Whole.Events[I].Text, "event " + std::to_string(I));
	Check(Live.Views.size() == Whole.Views.size(), "views");
	for (size_t I = 0; I < Live.Views.size() && I < Whole.Views.size(); ++I)
	{
		const LandFeed::View& A = Live.Views[I];
		const LandFeed::View& B = Whole.Views[I];
		bool bSame = Near(A.T, B.T) && A.Units.size() == B.Units.size() && Near(A.BalanceAtt, B.BalanceAtt);
		for (size_t U = 0; bSame && U < A.Units.size(); ++U)
			bSame = A.Units[U].Id == B.Units[U].Id && Near(A.Units[U].X, B.Units[U].X) && A.Units[U].bTaken == B.Units[U].bTaken
				&& A.Units[U].OrderKind == B.Units[U].OrderKind && A.Units[U].OrderTarget == B.Units[U].OrderTarget && Near(A.Units[U].OrderIn, B.Units[U].OrderIn);
		if (!bSame) { Check(false, "view " + std::to_string(I)); break; }
	}
	Check(Live.Phases.size() == Whole.Phases.size(), "phases' blocks");
	for (const auto& P : Whole.Phases)
	{
		const std::vector<LandFeed::Phase>* L = Live.PhasesOf(P.first);
		Check(L && L->size() == P.second.size(), "phases of block " + std::to_string(P.first));
		for (size_t I = 0; L && I < L->size() && I < P.second.size(); ++I)
			if (!(Near((*L)[I].T, P.second[I].T) && (*L)[I].Ph == P.second[I].Ph)) { Check(false, "phase " + std::to_string(I) + " of block " + std::to_string(P.first)); break; }
	}
	Check(Live.Craters.size() == Whole.Craters.size(), "craters " + std::to_string(Live.Craters.size()) + " v " + std::to_string(Whole.Craters.size()));
	for (size_t I = 0; I < Live.Craters.size() && I < Whole.Craters.size(); ++I)
		Check(Near(Live.Craters[I].X, Whole.Craters[I].X) && Near(Live.Craters[I].At, Whole.Craters[I].At), "crater " + std::to_string(I));
	for (size_t I = 0; I < Live.Blocks.size() && I < Whole.Blocks.size(); ++I)
		Check(Live.Blocks[I].CrewNames == Whole.Blocks[I].CrewNames, "crews' names of block " + std::to_string(Live.Blocks[I].Id));
	Check(Near(Live.Contact, Whole.Contact), "contact");
	Check(Near(Live.End, Whole.End), "end " + std::to_string(Live.End) + " v " + std::to_string(Whole.End));
	Check(Live.Result.Winner == Whole.Result.Winner && Live.Result.How == Whole.Result.How, "result");
	// The views carry the general's marks where he gave orders (deltas.ts gives a few when asked).
	int Taken = 0, OnAir = 0;
	for (const LandFeed::View& V : Live.Views)
		for (const LandFeed::Unit& U : V.Units) { Taken += U.bTaken ? 1 : 0; OnAir += U.OrderIn > 0.0 ? 1 : 0; }

	// A delta naming a track out of turn is refused.
	{
		LandFeed::Recording R;
		LandFeed::ReadRecording(HeaderText, R, Error);
		LandFeed::Grown G;
		Check(!LandFeed::AppendDelta("{\"t\":0.1,\"men\":[{\"n\":5,\"b\":1,\"k\":0,\"r\":\"rifle\",\"at\":[[0.1,1,2,0]]}],\"tails\":[],\"rounds\":[],\"events\":[],\"views\":[],\"phases\":[],\"contact\":null}", R, G, Error),
			"a track out of turn is refused");
	}

	std::printf("%s: %d deltas to t=%.1f; worst %.3f m over %ld samples; append %.2f ms a delta (most %.1f ms, biggest %.0f KB); %zu tracks, %zu rounds, %zu views; taken %d, on the air %d\n",
		Live.Key.c_str(), Deltas, Live.Fought, Worst, Samples, AppendMs / std::max(1, Deltas), MostMs, MostBytes / 1024.0, Live.Tracks.size(), Live.Rounds.size(), Live.Views.size(), Taken, OnAir);
	std::printf("the fire: %ld frames, %ld effects, %ld sounds, %ld losses played the same; places within %.3f m (%ld over 0.5 m); blasts paired %d (whole %d), alone %d (%d)\n",
		Frames, FxSeen, NoiseSeen, LossSeen, FxWorst, FarFx, PlayLive.BlastsPaired(), PlayWhole.BlastsPaired(), PlayLive.BlastsAlone(), PlayWhole.BlastsAlone());
	Check(FarFx == 0, "every effect within half a metre");
	Check(PlayLive.BlastsPaired() == PlayWhole.BlastsPaired() && PlayLive.RoundsPlayed() == PlayWhole.RoundsPlayed(), "the same rounds and blasts");
	std::printf("%s\n", Failures ? "FAILED" : "all held");
	return Failures ? 1 : 0;
}
