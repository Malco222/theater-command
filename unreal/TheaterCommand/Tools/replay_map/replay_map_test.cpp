// The replay map's cloud test (Tools/replay_map/run.sh): the shipped battles' recordings read as the game reads them
// (LandFeed::ReadRecording) and laid as the map (LandReplay::Build); a live battle's views made by hand, his side's and
// the enemy's before and after they are met; the 20 seconds played at 30 frames a second; and the screen's words, keys
// and the ship list's row read from their files.

#include "Battle/LandFeed.h"
#include "Battle/LandReplay.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Failed = 0, Passed = 0;

	void Check(bool bOk, const std::string& What)
	{
		if (bOk)
		{
			++Passed;
		}
		else
		{
			++Failed;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	bool Near(double A, double B, double Room = 1.0e-6)
	{
		return std::fabs(A - B) <= Room;
	}

	std::string ReadFile(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		std::stringstream S;
		S << In.rdbuf();
		return S.str();
	}

	/** A shipped battle's recording, laid as the map, and checked against the recording itself. */
	void Shipped(const std::string& Path)
	{
		const std::string Name = Path.substr(Path.find_last_of('/') + 1);
		LandFeed::Recording Rec;
		std::string Why;
		const bool bRead = LandFeed::ReadRecording(ReadFile(Path), Rec, Why);
		Check(bRead, Name + ": the recording reads (" + Why + ")");
		if (!bRead)
		{
			return;
		}
		LandReplay::Map M;
		const bool bBuilt = LandReplay::Build(Rec, M, Why);
		Check(bBuilt, Name + ": the map is laid (" + Why + ")");
		if (!bBuilt)
		{
			return;
		}
		Check(Near(M.W, Rec.Ground.W) && Near(M.H, Rec.Ground.H), Name + ": the map is the field's size");
		Check(Near(M.T0, Rec.Views.front().T) && Near(M.T1, Rec.Views.back().T), Name + ": the replay spans the first view to the last");
		Check(M.Me == -1, Name + ": a film's map is nobody's (both sides seen)");

		// Every unit of every view is a piece, with a mark for each view it is in, at the place the view has it.
		size_t Units = 0, Marks = 0, Wrong = 0;
		for (const LandFeed::View& V : Rec.Views)
		{
			for (const LandFeed::Unit& U : V.Units)
			{
				++Units;
				const LandReplay::Piece* P = nullptr;
				for (const LandReplay::Piece& Each : M.Pieces)
				{
					if (Each.Id == U.Id)
					{
						P = &Each;
					}
				}
				if (!P)
				{
					++Wrong;
					continue;
				}
				const LandReplay::Pose At = LandReplay::PoseAt(*P, V.T);
				if (!At.bThere || !Near(At.X, U.X, 1.0e-6) || !Near(At.Y, U.Y, 1.0e-6) || !At.bSeen)
				{
					++Wrong;
				}
			}
		}
		for (const LandReplay::Piece& P : M.Pieces)
		{
			Marks += P.Marks.size();
		}
		Check(Units > 0 && Wrong == 0, Name + ": every unit of every view stands on the map where the view has it (" + std::to_string(Wrong) + " wrong of " + std::to_string(Units) + ")");
		Check(Marks == Units, Name + ": a mark for each unit of each view (" + std::to_string(Marks) + " of " + std::to_string(Units) + ")");

		// Its sizes are its block's.
		size_t SizeWrong = 0;
		for (const LandReplay::Piece& P : M.Pieces)
		{
			for (const LandFeed::Block& B : Rec.Blocks)
			{
				if (B.Id == P.Id && B.W > 0.0 && (!Near(P.W, B.W) || !Near(P.D, B.D) || P.Kind != B.Kind))
				{
					++SizeWrong;
				}
			}
		}
		Check(SizeWrong == 0, Name + ": each piece has its block's frontage, depth and kind");

		// Halfway between two views a block stands halfway between its two places.
		const LandReplay::Piece& First = M.Pieces.front();
		if (First.Marks.size() >= 3)
		{
			size_t K = First.Marks.size() / 2;
			const LandReplay::Mark& A = First.Marks[K];
			const LandReplay::Mark& B = First.Marks[K + 1];
			const LandReplay::Pose Mid = LandReplay::PoseAt(First, (A.T + B.T) * 0.5);
			Check(Near(Mid.X, (A.X + B.X) * 0.5, 1.0e-6) && Near(Mid.Y, (A.Y + B.Y) * 0.5, 1.0e-6), Name + ": between two views, the place in proportion");
		}

		// The ground: the patches cover every woods, town and wet cell once, and nothing else.
		const LandFeed::Field& F = Rec.Ground;
		std::vector<int> Laid(static_cast<size_t>(F.Cols) * static_cast<size_t>(F.Rows), -1);
		size_t Over = 0, Outside = 0;
		for (const LandReplay::Patch& P : M.Patches)
		{
			if (P.X0 < -1.0e-6 || P.Y0 < -1.0e-6 || P.X1 > F.W + 1.0e-6 || P.Y1 > F.H + 1.0e-6 || P.X1 <= P.X0 || P.Y1 <= P.Y0)
			{
				++Outside;
				continue;
			}
			const int C0 = static_cast<int>(std::lround(P.X0 / F.Cell)), C1 = static_cast<int>(std::ceil(P.X1 / F.Cell - 1.0e-6));
			const int R0 = static_cast<int>(std::lround(P.Y0 / F.Cell)), R1 = static_cast<int>(std::ceil(P.Y1 / F.Cell - 1.0e-6));
			for (int R = R0; R < R1; ++R)
			{
				for (int C = C0; C < C1; ++C)
				{
					int& Cell = Laid[static_cast<size_t>(R) * static_cast<size_t>(F.Cols) + static_cast<size_t>(C)];
					if (Cell >= 0)
					{
						++Over;
					}
					Cell = static_cast<int>(P.Kind);
				}
			}
		}
		size_t Missed = 0, Shaded = 0, Cells = 0;
		for (size_t K = 0; K < Laid.size(); ++K)
		{
			LandReplay::Look L = LandReplay::Look::Woods;
			const bool bLook = F.Ground[K] < F.Grounds.size() && LandReplay::LookOf(F.Grounds[F.Ground[K]], L);
			if (bLook)
			{
				++Cells;
			}
			if (bLook && Laid[K] != static_cast<int>(L))
			{
				++Missed;
			}
			if (!bLook && Laid[K] >= 0)
			{
				++Shaded;
			}
		}
		Check(Cells > 0, Name + ": the field has woods, town or wet ground to shade");
		Check(Over == 0 && Outside == 0, Name + ": no patch lies over another or off the field");
		Check(Missed == 0 && Shaded == 0, Name + ": the patches shade every woods, town and wet cell, as its look, and no other (" + std::to_string(Missed) + " missed, " + std::to_string(Shaded) + " shaded wrongly)");
		Check(M.Patches.size() * 3 < Cells, Name + ": the patches are far fewer than the cells (" + std::to_string(M.Patches.size()) + " for " + std::to_string(Cells) + ")");

		// The ways: from the first mark to the last, each kept point at least a step from the one before, never longer
		// than the marks' own path, never shorter than the straight line.
		size_t WayWrong = 0, Movers = 0;
		for (const LandReplay::Piece& P : M.Pieces)
		{
			const std::vector<std::pair<double, double>> Way = LandReplay::WayTo(P, M.T1);
			double Walked = 0.0;
			for (size_t K = 1; K < P.Marks.size(); ++K)
			{
				Walked += std::hypot(P.Marks[K].X - P.Marks[K - 1].X, P.Marks[K].Y - P.Marks[K - 1].Y);
			}
			const double Straight = std::hypot(P.Marks.back().X - P.Marks.front().X, P.Marks.back().Y - P.Marks.front().Y);
			const double Long = LandReplay::Length(Way);
			if (Way.empty() || !Near(Way.front().first, P.Marks.front().X) || !Near(Way.back().first, P.Marks.back().X) || !Near(Way.back().second, P.Marks.back().Y)
				|| Long > Walked + 1.0e-6 || Long < Straight - 1.0e-6)
			{
				++WayWrong;
			}
			for (size_t K = 1; K + 1 < P.Way.size(); ++K)
			{
				if (std::hypot(P.Way[K].X - P.Way[K - 1].X, P.Way[K].Y - P.Way[K - 1].Y) < LandReplay::WayStep - 1.0e-6)
				{
					++WayWrong;
				}
			}
			if (Long >= LandReplay::LeastMove)
			{
				++Movers;
			}
		}
		Check(WayWrong == 0, Name + ": every way runs from the first mark to the last, a step apart, between the path and the straight line");
		Check(Movers > 0, Name + ": some blocks moved far enough for an arrow (" + std::to_string(Movers) + " of " + std::to_string(M.Pieces.size()) + ")");
		std::printf("  %s: %zu pieces, %zu marks, %zu patches for %zu cells, %zu arrows, %.1f s of battle\n", Name.c_str(), M.Pieces.size(), Marks, M.Patches.size(), Cells, Movers,
			M.T1 - M.T0);
	}

	/** A live battle's views made by hand: his side 0, the enemy met at 20 s. */
	void Live()
	{
		LandFeed::Recording Rec;
		Rec.bLive = true;
		Rec.Me = 0;
		Rec.Ground.W = 1000.0;
		Rec.Ground.H = 800.0;
		Rec.Title = "A test field";
		Rec.Sides[0].Adj = "British";
		Rec.Sides[1].Adj = "German";
		LandFeed::Block Mine, Theirs;
		Mine.Id = 1;
		Mine.Side = 0;
		Mine.Kind = "arm";
		Mine.W = 130.0;
		Mine.D = 60.0;
		Theirs.Id = 2;
		Theirs.Side = 1;
		Theirs.Kind = "inf";
		Theirs.W = 150.0;
		Theirs.D = 40.0;
		Rec.Blocks = { Mine, Theirs };
		for (int T = 0; T <= 40; ++T)
		{
			LandFeed::View V;
			V.T = T;
			LandFeed::Unit A, B;
			A.Id = 1;
			A.Side = 0;
			A.X = 100.0 + 10.0 * T;
			A.Y = 100.0;
			A.Face = 1.5707963267948966;
			A.State = T >= 35 ? "routing" : "steady";
			B.Id = 2;
			B.Side = 1;
			B.X = 700.0;
			B.Y = 600.0 - 5.0 * T;
			B.State = T >= 38 ? "gone" : "steady";
			B.bKnown = T >= 20;
			V.Units = { A, B };
			Rec.Views.push_back(V);
		}
		// (a view twice at one time: the later stands)
		LandFeed::View Again = Rec.Views.back();
		Again.Units[0].X = 520.0;
		Rec.Views.push_back(Again);
		LandReplay::Map M;
		std::string Why;
		Check(LandReplay::Build(Rec, M, Why), "live: the map is laid (" + Why + ")");
		Check(M.Me == 0, "live: the map is his side's");
		Check(M.Pieces.size() == 2, "live: two pieces");
		if (M.Pieces.size() != 2)
		{
			return;
		}
		const LandReplay::Piece& A = M.Pieces[0];
		const LandReplay::Piece& B = M.Pieces[1];
		Check(A.Marks.size() == 41 && B.Marks.size() == 41, "live: a view twice at one time is one mark");
		Check(Near(LandReplay::PoseAt(A, 40.0).X, 520.0), "live: the later of two views at one time stands");
		Check(A.Kind == "arm" && Near(A.W, 130.0) && Near(A.D, 60.0), "live: his own block's kind and size from its block");
		Check(B.Kind == "inf", "live: the enemy's kind once met");
		Check(LandReplay::PoseAt(A, 5.0).bSeen, "live: his own always seen");
		Check(!LandReplay::PoseAt(B, 5.0).bSeen && !LandReplay::PoseAt(B, 19.5).bSeen, "live: the enemy not seen before it is met");
		Check(LandReplay::PoseAt(B, 20.0).bSeen && LandReplay::PoseAt(B, 30.0).bSeen, "live: the enemy seen once met");
		Check(LandReplay::PoseAt(A, 36.0).bRouting && !LandReplay::PoseAt(A, 34.0).bRouting, "live: running from the view that says so");
		Check(LandReplay::PoseAt(B, 39.0).bGone && !LandReplay::PoseAt(B, 37.0).bGone, "live: gone from the view that says so");
		Check(!LandReplay::PoseAt(A, -0.5).bThere, "live: nothing on the field before its first view");
		Check(LandReplay::PoseAt(A, -1.0e-7).bThere && Near(LandReplay::PoseAt(A, -1.0e-7).X, 100.0), "live: a hair before its first view, at it");
		Check(Near(LandReplay::PoseAt(A, 12.5).X, 225.0), "live: between views, the place in proportion");
		const std::vector<std::pair<double, double>> Half = LandReplay::WayTo(A, 12.5);
		Check(!Half.empty() && Near(Half.back().first, 225.0) && Near(Half.front().first, 100.0), "live: the way so far ends where it stands now");
		Check(Near(LandReplay::Length(Half), 125.0), "live: the way so far is as long as it went (" + std::to_string(LandReplay::Length(Half)) + ")");
		Check(LandReplay::Length(LandReplay::WayTo(A, 5.0)) < LandReplay::LeastMove, "live: 50 m is no arrow yet");
		Check(LandReplay::Length(LandReplay::WayTo(A, 10.0)) >= LandReplay::LeastMove, "live: 100 m is an arrow");
		Check(LandReplay::WayTo(A, -1.0).empty(), "live: no way before it is there");

		// The corners: facing east (pi/2), the front is east of the middle, the frontage runs north and south.
		const LandReplay::Pose P = LandReplay::PoseAt(A, 0.0);
		const std::vector<std::pair<double, double>> C = LandReplay::Corners(A, P);
		Check(C.size() == 4, "corners: four");
		if (C.size() == 4)
		{
			const double Mx = (C[0].first + C[1].first + C[2].first + C[3].first) / 4.0, My = (C[0].second + C[1].second + C[2].second + C[3].second) / 4.0;
			Check(Near(Mx, P.X, 1.0e-6) && Near(My, P.Y, 1.0e-6), "corners: their middle is the block's place");
			Check(Near(std::hypot(C[1].first - C[0].first, C[1].second - C[0].second), 130.0, 1.0e-6), "corners: the front is the frontage long");
			Check(Near(std::hypot(C[2].first - C[1].first, C[2].second - C[1].second), 60.0, 1.0e-6), "corners: the side is the depth long");
			Check(C[0].first > P.X && C[1].first > P.X && C[2].first < P.X, "corners: facing east, the front is east of the middle");
			Check(C[0].second > C[1].second, "corners: facing east, the front's left end is the north one");
		}

		// Errors.
		LandFeed::Recording Empty;
		Empty.Ground.W = 100.0;
		Empty.Ground.H = 100.0;
		LandReplay::Map None;
		Check(!LandReplay::Build(Empty, None, Why) && !Why.empty(), "no views: no map, and why (" + Why + ")");
		LandFeed::Recording NoField = Rec;
		NoField.Ground.W = 0.0;
		Check(!LandReplay::Build(NoField, None, Why) && !Why.empty(), "no field: no map, and why (" + Why + ")");
	}

	void Fitting()
	{
		LandReplay::Fit F;
		F.Into(3000.0, 2000.0, 100.0, 50.0, 900.0, 900.0);
		Check(Near(F.Scale, 0.3), "fit: the field's width fills the box's width");
		const std::pair<double, double> SouthWest = F.On(0.0, 0.0), NorthEast = F.On(3000.0, 2000.0);
		Check(Near(SouthWest.first, 100.0) && Near(NorthEast.first, 1000.0), "fit: west on the left, east on the right");
		Check(SouthWest.second > NorthEast.second, "fit: north up");
		Check(Near(NorthEast.second, 50.0 + (900.0 - 600.0) * 0.5) && Near(SouthWest.second - NorthEast.second, 600.0), "fit: centred top to bottom, the field's shape kept");
		F.Into(1000.0, 2000.0, 0.0, 0.0, 800.0, 500.0);
		Check(Near(F.Scale, 0.25) && Near(F.On(0.0, 2000.0).first, 275.0), "fit: a tall field fills the height, centred side to side");
	}

	void Playing()
	{
		LandReplay::Map M;
		M.T0 = 10.0;
		M.T1 = 280.0;
		LandReplay::Player P;
		Check(!P.IsOpen(), "player: shut until opened");
		P.Tick(5.0);
		Check(Near(P.Played(), 0.0), "player: shut, no time passes");
		P.Open();
		Check(P.IsOpen() && Near(P.BattleAt(M), 10.0), "player: opens on the first view");
		auto Frames = [&](double Seconds)
		{
			const int Count = static_cast<int>(Seconds * 30.0 + 0.5);
			for (int K = 0; K < Count; ++K)
			{
				P.Tick(1.0 / 30.0);
			}
		};
		Frames(0.9);
		Check(Near(P.BattleAt(M), 10.0), "player: the first view held a second");
		Frames(9.6);
		Check(Near(P.BattleAt(M), 145.0, 0.5), "player: half way through the 20 s, half way through the battle (" + std::to_string(P.BattleAt(M)) + ")");
		Frames(9.5);
		Check(P.IsDone() && Near(P.BattleAt(M), 280.0), "player: at 20 s, the last view");
		Frames(5.0);
		Check(P.IsDone() && Near(P.Played(), LandReplay::Player::Length) && Near(P.BattleAt(M), 280.0), "player: the last view holds");
		P.Again();
		Check(!P.IsDone() && Near(P.BattleAt(M), 10.0), "player: AGAIN from the first view");
		P.Close();
		Check(!P.IsOpen(), "player: CLOSE shuts it");
		Check(LandReplay::Clock(0.0) == "0:00" && LandReplay::Clock(65.4) == "1:05" && LandReplay::Clock(274.7) == "4:34" && LandReplay::Clock(-3.0) == "0:00",
			"clock: minutes and seconds, whole seconds down");
	}

	/** The words between two marks in a file, or empty where either is missing. */
	std::string Between(const std::string& Text, const std::string& From, const std::string& To)
	{
		const size_t A = Text.find(From);
		if (A == std::string::npos)
		{
			return "";
		}
		const size_t B = Text.find(To, A + From.size());
		return B == std::string::npos ? "" : Text.substr(A, B - A);
	}

	/** The screen: the after card's REPLAY MAP and its key, the map's buttons and their keys; the ship list's row off. */
	void Screen(const std::string& ScreenPath, const std::string& ShipPath, const std::string& ScenePath)
	{
		const std::string Text = ReadFile(ScreenPath);
		const std::string Keys = Between(Text, "FReply SLandBattleScreen::OnKeyDown(", "\tif (S->General.bOn && !S->bCard)");
		Check(!Keys.empty(), "screen: OnKeyDown read to the general's keys");
		const std::string Open = Between(Keys, "// (the replay map open", "return FReply::Handled();\n\t}");
		Check(!Open.empty(), "screen: OnKeyDown has the open replay map's keys before CONTINUE's Enter");
		Check(Open.find("EKeys::SpaceBar") != std::string::npos && Open.find("replay:again") != std::string::npos, "screen: SPACE plays the map again");
		Check(Open.find("EKeys::Escape") != std::string::npos && Open.find("replay:close") != std::string::npos, "screen: ESC closes the map");
		Check(Keys.find("Key == EKeys::M && S->bCard") != std::string::npos && Keys.find("replay:open") != std::string::npos, "screen: M opens the map from the after card");
		Check(Keys.find("// (the replay map open") < Keys.find("if (Key == EKeys::Enter && S->bCard)"), "screen: the map's keys come before the after card's Enter");
		const std::string Paint = Between(Text, "void SLandBattleScreen::PaintReplay(", "\n}\n");
		Check(!Paint.empty(), "screen: PaintReplay drawn");
		for (const char* Word : { "TEXT(\"REPLAY MAP\")", "TEXT(\"AGAIN\")", "TEXT(\"SPACE\")", "TEXT(\"CLOSE\")", "TEXT(\"ESC\")", "TEXT(\"NOT YET FOUND\")", "TEXT(\"GONE\")",
				 "\"replay:again\"", "\"replay:close\"" })
		{
			Check(Paint.find(Word) != std::string::npos, std::string("screen: the map shows ") + Word);
		}
		const std::string Card = Between(Text, "// ---- The after card:", "return LayerId + 36;");
		Check(Card.find("TEXT(\"REPLAY MAP\")") != std::string::npos && Card.find("\"replay:open\"") != std::string::npos && Card.find("TEXT(\"M\")") != std::string::npos,
			"screen: the after card offers REPLAY MAP with its key M");
		Check(Card.find("ReplayShown()") != std::string::npos, "screen: the after card offers it only where the map is on and laid");
		const std::string Scene = ReadFile(ScenePath);
		for (const char* Word : { "\"open\"", "\"again\"", "\"close\"" })
		{
			Check(Between(Scene, "void ALandBattle::ReplayDo(", "\n}\n").find(Word) != std::string::npos, std::string("scene: ReplayDo takes ") + Word);
		}
		const std::string Ship = ReadFile(ShipPath);
		const std::string Row = Between(Ship, "{ TEXT(\"replaymap\")", "\n");
		Check(Row.find("TEXT(\"-TheaterReplayMap\"), false,") != std::string::npos, "ship list: the replaymap row, -TheaterReplayMap, OFF");
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 6)
	{
		std::printf("usage: replay_map_test <normandy.json> <prokhorovka.json> <LandBattleScreen.cpp> <Release2Ship.cpp> <LandBattleReplay.cpp>\n");
		return 2;
	}
	Shipped(Argv[1]);
	Shipped(Argv[2]);
	Live();
	Fitting();
	Playing();
	Screen(Argv[3], Argv[4], Argv[5]);
	std::printf("replay map: %d passed, %d failed\n", Passed, Failed);
	return Failed == 0 ? 0 : 1;
}
