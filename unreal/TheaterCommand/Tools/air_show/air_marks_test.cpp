// The raid on troops' cloud test (Brian, 24 November 2026: "i want every air battle playable" and "if a bomber group is
// bombing troops instead of a factory or town and they get intercepted than i want that playable"). It plays a recorded
// air battle that carries marks (place.marks, view.marks, air:mark-hit, air:strafe: the every-kind contract's sections
// 2 to 4) through the game's own flight core (Battle/AirShow, Battle/AirShowTroops, Battle/AirMarks) at the game's frame
// pace, and checks, from the effects the scene is handed and nothing else:
//
//   - every air:mark-hit gives exactly one MarkHit, at its piece, and never before the drawn bomb that carries it lands
//     (on its piece for a wreck, a burn or a blast; within its blast beside it for a near miss or men scattered) or the
//     strafing rounds that carry it come down on the piece;
//   - no bomb comes down within its blast of any piece but one whose counted hit it carries ("most shots miss, but every
//     hit shows on what it struck"), and no strafing round on a piece whose hit it does not carry;
//   - a strafing pass keeps the fighter's g, its least turn and its roll rate, and pulls out above the ground;
//   - a piece that is out never changes again (a stale word from a view is refused);
//   - the pieces end as the views' last words have them.
//
// A recording with no marks has nothing to check here, and passes. With an out-dir it writes <key>-marks.csv, the
// column and what came down on it, for Tools/air_show/marks_picture.py.
//
// Usage: air_marks_test <recording.json> [out-dir] [frames a second, 60]

#include "Battle/AirDesk.h"
#include "Battle/AirFeed.h"
#include "Battle/AirGround.h"
#include "Battle/AirMarks.h"
#include "Battle/AirShow.h"

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
	struct Row
	{
		std::string Name;
		bool bOk = false;
		std::string Got, Want;
	};

	std::string Num(double V, int Places = 1)
	{
		char Buf[64];
		std::snprintf(Buf, sizeof(Buf), "%.*f", Places, V);
		return Buf;
	}

	struct Seen
	{
		long Frame = 0;
		double Air = 0.0;
		AirShow::Fx F;
	};
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: air_marks_test <recording.json> [out-dir] [frames-per-second]\n");
		return 2;
	}
	const std::string Path = Argv[1];
	const std::string OutDir = Argc > 2 ? Argv[2] : "";
	const double Fps = Argc > 3 ? std::atof(Argv[3]) : 60.0;
	std::ifstream In(Path, std::ios::binary);
	if (!In)
	{
		std::fprintf(stderr, "cannot read %s\n", Path.c_str());
		return 2;
	}
	std::stringstream Buffer;
	Buffer << In.rdbuf();
	AirFeed::Recording Rec;
	std::string Error;
	if (!AirFeed::ReadRecording(Buffer.str(), Rec, Error))
	{
		std::fprintf(stderr, "%s: %s\n", Path.c_str(), Error.c_str());
		return 2;
	}
	const AirMarks::Column& Given = Rec.Board.Marks;
	if (!Given.Has())
	{
		std::printf("== %s: no marks (not a raid on troops): nothing to check\n", Rec.Key.c_str());
		return 0;
	}

	AirShow::Show Show(Rec.Seed ? Rec.Seed : 238);
	AirFeed::Apply(Rec.Board, Show);
	Show.Eye = AirShow::Vec(Rec.Board.TargetX, Rec.Board.TargetY - 6000.0, 5000.0);
	// (the board's own land under a raid with no stage, as AAirBattle::Open hands it to the core)
	const std::shared_ptr<const AirGround::BoardLand> BoardGround =
		Rec.Board.StageKey.empty() ? std::make_shared<const AirGround::BoardLand>(Rec.Board.Land) : std::shared_ptr<const AirGround::BoardLand>();
	if (BoardGround && !BoardGround->IsEmpty()) Show.Ground = [BoardGround](double X, double Y) { return BoardGround->At(X, Y); };

	// What the recording tells: the hits by piece, the strafes, and what each piece is after by the views' words (a
	// piece that is out never changing again).
	std::map<int, int> ToldHits;
	int ToldStrafes = 0, ToldHitsAll = 0;
	std::map<int, AirMarks::State> LastWord;
	std::map<int, int> LastMen;
	for (const AirFeed::Frame& F : Rec.Views)
	{
		for (const AirShow::Event& E : F.Events)
		{
			if (E.What == "air:mark-hit") { ToldHits[E.Piece]++; ToldHitsAll++; }
			if (E.What == "air:strafe") ToldStrafes++;
		}
		for (const AirMarks::Change& C : F.Marks.Pieces)
		{
			const auto Was = LastWord.find(C.Id);
			if (Was != LastWord.end() && Was->second == AirMarks::State::Out) continue;
			LastWord[C.Id] = C.To;
			if (C.Men >= 0) LastMen[C.Id] = C.Men;
		}
	}

	// The play, at the game's frame pace, one view in hand ahead of what is shown.
	std::vector<Seen> BombHits, MarkHits, Strikes, MarkGuns;
	long Frame = 0;
	int OutChanged = 0, StrafeGunFrames = 0;
	std::map<int, AirMarks::State> WasOut;
	std::map<int, int> OutMen;
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
	std::ofstream Csv;
	if (!OutDir.empty())
	{
		Csv.open(OutDir + "/" + Rec.Key + "-marks.csv");
		Csv << "kind,t,air,x,y,a,b,c,d\n";
		for (const AirMarks::Piece& P : Given.Pieces)
			Csv << "piece," << 0 << "," << 0 << "," << P.X << "," << P.Y << "," << P.Id << "," << P.What << "," << P.Face << "," << P.Long << "x" << P.Wide << "\n";
		for (const auto& Pt : Given.Road) Csv << "road,0,0," << Pt.first << "," << Pt.second << ",,,,\n";
	}
	double NextTrack = 0.0;
	while (T < EndT - 1e-9)
	{
		T = std::min(EndT, T + Dt);
		while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
		Show.Advance(T);
		++Frame;
		bool bStrafeGun = false;
		for (const AirShow::Fx& F : Show.Effects)
		{
			Seen S{ Frame, Show.AirNow(), F };
			if (F.Kind == AirShow::FxKind::BombHit) BombHits.push_back(S);
			else if (F.Kind == AirShow::FxKind::MarkHit) MarkHits.push_back(S);
			else if (F.Kind == AirShow::FxKind::Hit && (F.Text == "ground" || F.Text == "water")) Strikes.push_back(S);
			else if (F.Kind == AirShow::FxKind::MarkGun) MarkGuns.push_back(S);
			// (a fighter's fixed guns laid on the ground: a strafing pass's)
			else if (F.Kind == AirShow::FxKind::Gun && F.Text == "fixed" && F.Aim.Z < 50.0) bStrafeGun = true;
		}
		StrafeGunFrames += bStrafeGun ? 1 : 0;
		Show.Effects.clear();
		// (a piece that is out never changes again: its state and its men as they were when it went out)
		for (const AirMarks::Piece& P : Show.Marks().Pieces)
		{
			const auto W = WasOut.find(P.Id);
			if (W != WasOut.end())
			{
				if (P.Now != AirMarks::State::Out || P.Men != OutMen[P.Id]) ++OutChanged;
			}
			else if (P.Now == AirMarks::State::Out)
			{
				WasOut[P.Id] = P.Now;
				OutMen[P.Id] = P.Men;
			}
		}
		// (the strafers' tracks, a sample a tenth of an air second, for the picture)
		if (Csv && Show.AirNow() >= NextTrack)
		{
			NextTrack = Show.AirNow() + 0.1;
			for (const AirShow::PlaneOut& P : Show.Planes())
				if (P.bFighter && P.Side == 0 && P.At.Z < 1500.0 && std::hypot(P.At.X - Rec.Board.TargetX, P.At.Y - Rec.Board.TargetY) < 6000.0)
					Csv << "plane," << T << "," << Show.AirNow() << "," << P.At.X << "," << P.At.Y << "," << P.Key << "," << P.At.Z << "," << (P.bFiring ? 1 : 0) << ",\n";
		}
	}
	const AirShow::Checks& C = Show.Checked();

	// The pieces as given, by id, for the footprints.
	std::map<int, const AirMarks::Piece*> ById;
	for (const AirMarks::Piece& P : Given.Pieces) ById[P.Id] = &P;
	auto NearestOff = [&Given](double X, double Y, int& Id)
	{
		double Best = 1e18;
		Id = -1;
		for (const AirMarks::Piece& P : Given.Pieces)
		{
			const double D = AirMarks::Off(P, X, Y);
			if (D < Best) { Best = D; Id = P.Id; }
		}
		return Best;
	};

	// 1. Every hit, one MarkHit, on its piece, never before what carries it.
	std::map<int, int> ShownHits;
	int MarkOffPiece = 0, MarkBeforeBomb = 0, MarkBombAstray = 0, MarkNoRounds = 0, MarkNoBomb = 0;
	std::map<int, const Seen*> BombById;
	for (const Seen& S : BombHits) if (S.F.Id > 0) BombById[S.F.Id] = &S;
	std::set<int> Carrying; // (the bombs that carry a hit)
	double MostBombOff = 0.0;
	for (const Seen& S : MarkHits)
	{
		ShownHits[S.F.Key]++;
		const auto P = ById.find(S.F.Key);
		if (P == ById.end() || AirMarks::Off(*P->second, S.F.At.X, S.F.At.Y) > 1e-6) { ++MarkOffPiece; continue; }
		if (S.F.What == "strafe")
		{
			// (its rounds: a strike on the piece's footprint at most 2 air s before, and not after)
			bool bFound = false;
			for (const Seen& R : Strikes)
				if (R.Frame <= S.Frame && R.Air >= S.Air - 2.0 && AirMarks::Off(*P->second, R.F.At.X, R.F.At.Y) <= 0.5) bFound = true;
			MarkNoRounds += bFound ? 0 : 1;
			continue;
		}
		if (S.F.Id <= 0) { ++MarkNoBomb; continue; }
		const auto B = BombById.find(S.F.Id);
		if (B == BombById.end()) { ++MarkNoBomb; continue; }
		Carrying.insert(S.F.Id);
		if (B->second->Frame > S.Frame) ++MarkBeforeBomb;
		const double Off = AirMarks::Off(*P->second, B->second->F.At.X, B->second->F.At.Y);
		const bool bBeside = S.F.Text == "near" || S.F.Text == "scatter";
		const double Blast = AirMarks::BlastOf(B->second->F.Size);
		MostBombOff = std::max(MostBombOff, Off);
		if (bBeside ? Off > Blast : Off > 0.05) ++MarkBombAstray;
	}
	int HitsMissing = 0, HitsTwice = 0;
	for (const auto& Pair : ToldHits)
	{
		const int Got = ShownHits.count(Pair.first) ? ShownHits[Pair.first] : 0;
		if (Got < Pair.second) HitsMissing += Pair.second - Got;
		if (Got > Pair.second) HitsTwice += Got - Pair.second;
	}
	for (const auto& Pair : ShownHits) if (!ToldHits.count(Pair.first)) HitsTwice += Pair.second;

	// 2. Every other bomb clear of every piece by more than its blast; no round on a piece whose hit it does not carry.
	int BombsNear = 0, Bombs = 0;
	double LeastClear = 1e18;
	for (const Seen& S : BombHits)
	{
		++Bombs;
		if (Carrying.count(S.F.Id)) continue;
		int Id = -1;
		const double Off = NearestOff(S.F.At.X, S.F.At.Y, Id);
		LeastClear = std::min(LeastClear, Off - AirMarks::BlastOf(S.F.Size));
		if (Off <= AirMarks::BlastOf(S.F.Size)) ++BombsNear;
		if (Csv) Csv << "bomb," << 0 << "," << S.Air << "," << S.F.At.X << "," << S.F.At.Y << "," << S.F.Id << "," << S.F.Size << ",0,\n";
	}
	if (Csv)
	{
		for (const Seen& S : BombHits)
			if (Carrying.count(S.F.Id)) Csv << "bomb," << 0 << "," << S.Air << "," << S.F.At.X << "," << S.F.At.Y << "," << S.F.Id << "," << S.F.Size << ",1,\n";
		for (const Seen& S : MarkHits) Csv << "markhit,0," << S.Air << "," << S.F.At.X << "," << S.F.At.Y << "," << S.F.Key << "," << S.F.What << "," << S.F.Text << "," << S.F.Id << "\n";
		for (const Seen& S : Strikes) Csv << "round,0," << S.Air << "," << S.F.At.X << "," << S.F.At.Y << ",,,,\n";
		for (const Seen& S : MarkGuns) if (S.Frame % 30 == 0) Csv << "flak,0," << S.Air << "," << S.F.At.X << "," << S.F.At.Y << "," << S.F.Aim.X << "," << S.F.Aim.Y << "," << S.F.Aim.Z << ",\n";
		for (const AirMarks::Piece& P : Show.Marks().Pieces)
			Csv << "state,0,0," << P.X << "," << P.Y << "," << P.Id << "," << AirMarks::WordOf(P.Now) << "," << P.Men << ",\n";
	}
	int RoundsOnPieces = 0, RoundsNearColumn = 0;
	for (const Seen& R : Strikes)
	{
		int Id = -1;
		const double Off = NearestOff(R.F.At.X, R.F.At.Y, Id);
		if (Off < 200.0) ++RoundsNearColumn;
		if (Off > 0.0) continue;
		bool bCarried = false;
		for (const Seen& S : MarkHits) bCarried = bCarried || (S.F.Key == Id && S.F.What == "strafe" && std::fabs(S.Air - R.Air) <= 1.5);
		RoundsOnPieces += bCarried ? 0 : 1;
	}

	// 3. The pieces at the end, as the views' last words have them.
	int EndWrong = 0;
	for (const AirMarks::Piece& P : Show.Marks().Pieces)
	{
		const auto W = LastWord.find(P.Id);
		const AirMarks::State Want = W == LastWord.end() ? AirMarks::State::Ok : W->second;
		const auto M = LastMen.find(P.Id);
		if (P.Now != Want || (M != LastMen.end() && P.Men != M->second)) ++EndWrong;
	}

	// 4. The screen's words and the after card speak of the column (AirDesk, as the 3D screen writes them), and STRAFE THE
	// COLUMN sends his own fighter groups only: the views read as the screen reads them, to the end.
	AirDesk::Desk Desk;
	Desk.Begin(Rec.Board, Rec.Views.front().Head.Me);
	for (const AirFeed::Frame& F : Rec.Views) Desk.Take(F);
	Desk.Reach(EndT + 1.0);
	AirDesk::Words W;
	Desk.Write(0.0, Show.Groups(), std::string(), W);
	// (a live recording keeps its after card in end.after, not at the top: read from there where the top has none; and
	// one line of the old target's words added, which the card must turn to the column's)
	AirFeed::After After = Rec.Card;
	if (After.Lines.empty())
	{
		AirFeed::Json Root;
		std::string Why;
		if (AirFeed::Parse(Buffer.str(), Root, Why)) AirFeed::ReadAfter(Root["end"]["after"], After);
	}
	const size_t CardLines = After.Lines.size();
	After.Lines.push_back("The bombers found the target under broken cloud.");
	const AirDesk::Card Card = Desk.CardOf(After);
	auto Says = [](const std::string& Text, const char* Part) { return Text.find(Part) != std::string::npos; };
	bool bCardTarget = Says(Card.Reckoning, "the target") || Says(Card.Reckoning, "The target");
	for (const std::string& Line : Card.Lines) bCardTarget = bCardTarget || Says(Line, "the target") || Says(Line, "The target");
	std::vector<int> All, Fighters;
	for (const AirDesk::Tag& Tg : W.Tags)
	{
		All.push_back(Tg.Id);
		if (Tg.bMine && Tg.bFighter && !Tg.bGone) Fighters.push_back(Tg.Id);
	}
	std::string Want = "{\"do\":\"air:strafe\",\"units\":[";
	for (size_t K = 0; K < Fighters.size(); ++K) Want += (K ? "," : "") + std::to_string(Fighters[K]);
	Want += "]}";
	const std::string Strafe = Desk.Strafe(All, W.Tags);
	const bool bWords = Desk.OnTroops() && Says(W.Title, "The raid on") && Says(W.AimTarget, Given.Under.empty() ? "column" : Given.Under.front().Name.c_str())
		&& Says(W.Chance, "organisation cut") && (Says(W.AimState, "column so far") || Says(W.AimState, "No hits"));
	std::printf("  words: title \"%s\"; top bar \"%s\"; aim \"%s\" / \"%s\"; card \"%s\" (%zu lines of the recording's, and one added)\n",
	            W.Title.c_str(), W.Chance.c_str(), W.AimTarget.c_str(), W.AimState.c_str(), Card.Title.c_str(), CardLines);
	for (const std::string& Line : Card.Lines) std::printf("    card: %s\n", Line.c_str());
	std::printf("  STRAFE THE COLUMN with every group in hand: %s\n", Strafe.empty() ? "(nothing sent)" : Strafe.c_str());

	std::vector<Row> Rows = {
		{ "every air:mark-hit shown once (none missing)", HitsMissing == 0, std::to_string(HitsMissing), "0" },
		{ "no MarkHit twice, nor for a hit not told", HitsTwice == 0, std::to_string(HitsTwice), "0" },
		{ "hits told, hits shown", C.MarkHitsTold == ToldHitsAll && C.MarkHitsShown == ToldHitsAll,
		  std::to_string(C.MarkHitsTold) + " / " + std::to_string(C.MarkHitsShown), std::to_string(ToldHitsAll) },
		{ "every MarkHit at its piece", MarkOffPiece == 0, std::to_string(MarkOffPiece), "0" },
		{ "every hit carried by its bomb or its rounds (none shown unseen)", C.MarkHitsUnseen == 0 && MarkNoBomb == 0 && MarkNoRounds == 0,
		  std::to_string(C.MarkHitsUnseen) + " unseen, " + std::to_string(MarkNoBomb) + " no bomb, " + std::to_string(MarkNoRounds) + " no rounds", "0" },
		{ "no bomb hit shown before its bomb lands", MarkBeforeBomb == 0, std::to_string(MarkBeforeBomb), "0" },
		{ "each carrying bomb on its piece (wreck, burn, blast) or within its blast (near, scatter)", MarkBombAstray == 0,
		  std::to_string(MarkBombAstray) + " (farthest " + Num(MostBombOff) + " m from the footprint)", "0" },
		{ "no other bomb within its blast of a piece", BombsNear == 0,
		  std::to_string(BombsNear) + " of " + std::to_string(Bombs) + " (least clear by " + Num(LeastClear) + " m)", "0" },
		{ "bombs laid on pieces, cleared off them; most moved", true,
		  std::to_string(C.MarkBombsLaid) + ", " + std::to_string(C.MarkBombsCleared) + "; " + Num(C.MarkMostMove) + " m", "(told)" },
		{ "no strafing round on a piece whose hit it does not carry", RoundsOnPieces == 0 && C.StrafeOnPieces == 0,
		  std::to_string(RoundsOnPieces) + " (core's own " + std::to_string(C.StrafeOnPieces) + "; " + std::to_string(RoundsNearColumn) + " rounds within 200 m)", "0" },
		{ "a strafing pass flown for each air:strafe", ToldStrafes == 0 || (C.StrafePasses > 0 && StrafeGunFrames > 0),
		  std::to_string(C.StrafePasses) + " aircraft, " + std::to_string(StrafeGunFrames) + " frames of guns", ToldStrafes ? "some" : "none told" },
		{ "strafing: no more than 6 g", C.StrafeMostG <= 6.05, Num(C.StrafeMostG, 2), "at most 6.05" },
		{ "strafing: no turn tighter than 210 m", ToldStrafes == 0 || C.StrafeLeastRadius >= 205.0, Num(C.StrafeLeastRadius), "at least 205" },
		{ "strafing: roll no faster than 160 degrees a second", C.StrafeMostRoll <= 161.0, Num(C.StrafeMostRoll), "at most 161" },
		{ "strafing: pulled out above the ground", ToldStrafes == 0 || C.StrafeLowest >= 20.0, Num(C.StrafeLowest), "at least 20 m" },
		{ "strafing: down to strafing height", ToldStrafes == 0 || C.StrafeLowest <= 150.0, Num(C.StrafeLowest), "at most 150 m" },
		{ "a piece that is out never changes again", OutChanged == 0, std::to_string(OutChanged) + " (" + std::to_string(C.MarkChangesRefused) + " stale words refused)", "0" },
		{ "the pieces end as the views' last words have them", EndWrong == 0, std::to_string(EndWrong), "0" },
		{ "the column's light flak fired back", C.MarkGunFrames > 0, std::to_string(C.MarkGunFrames) + " frames, " + std::to_string(MarkGuns.size()) + " streams", "some" },
		{ "the screen's words speak of the column (title, top bar, the aim)", bWords, bWords ? "yes" : "no", "yes" },
		{ "the after card speaks of the column, not \"the target\"", !bCardTarget && Says(Card.Title, "The raid on") && Says(Card.Lines.back(), "found the column"),
		  bCardTarget ? "the target" : Card.Title + " / " + Card.Lines.back(), "the column" },
		{ "STRAFE THE COLUMN sends his fighter groups only", Fighters.empty() ? Strafe.empty() : Strafe == Want, Strafe.empty() ? "(nothing)" : Strafe, Fighters.empty() ? "(nothing)" : Want },
	};
	std::printf("== %s: raid on troops: %zu pieces, %d hits told, %d strafes, %ld frames at %.0f a second\n", Rec.Key.c_str(),
	            Given.Pieces.size(), ToldHitsAll, ToldStrafes, Frame, Fps);
	bool bAll = true;
	for (const Row& R : Rows)
	{
		std::printf("  %-4s %-90s %s (want %s)\n", R.bOk ? "ok" : "FAIL", R.Name.c_str(), R.Got.c_str(), R.Want.c_str());
		bAll = bAll && R.bOk;
	}
	std::printf(bAll ? "MARKS CHECKS PASSED\n" : "MARKS CHECKS FAILED\n");
	return bAll ? 0 : 1;
}
