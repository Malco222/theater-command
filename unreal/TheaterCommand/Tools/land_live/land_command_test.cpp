// The general's clipboard's cloud test (release 2): LandCommand::General reads a live battle as the 3D game grows it
// (header, then each delta through LandFeed::AppendDelta) and the test checks what the screen will rely on:
//  - every block of the view has its tag, eased from the view at or before the screen's time toward the next, never
//    off the line between them; the chips say whose it is (his staff's, his own, on the air, the enemy's, unseen);
//  - picking, Shift-picking, a box, Tab and clearing take only his blocks that can take orders;
//  - a click on an enemy attacks it (guns alone fire on it), on open ground goes there, at the double when asked; a
//    line, hold, fall back and back to staff each make the rules' own command; every command is JSON the rules read;
//  - in the setup step, a block's ground check (MayStand) never says yes where the rules' ground says no for want of room;
//  - the walk pressed in code for the Mac's builder (LandScript, -TheaterLandScript) finds woods on his ground for two of
//    his units, a unit to send forward and a point inside the field, an enemy he can see to attack and cover to fire into.
// The commands and placings it makes are written out (commands.jsonl, placings.jsonl) for command-check.ts, which gives
// them to the rules at the same moments of the same battle and checks each is taken.
//  - neighbours join the fight (release 2): a corps coming up from next door is told, named and counted down while it
//    comes (his own; the enemy's his staff knows of, uncounted), and is on the field the moment it stops coming.
// Usage: land_command_test <folder (header.json, deltas.jsonl)> <setup folder (header.json, deltas.jsonl)> [join folder]

#include "Battle/LandCommand.h"
#include "Battle/LandFeed.h"
#include "Battle/LandJson.h"
#include "Battle/LandScript.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
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

	bool IsJson(const std::string& Text, const char* Do)
	{
		LandJson::Json Root;
		std::string Error;
		return LandJson::Parse(Text, Root, Error) && Root.IsObject() && Root["do"].Str() == Do;
	}

	/** Distance from P to the segment A-B. */
	double OffLine(double Px, double Py, double Ax, double Ay, double Bx, double By)
	{
		const double Dx = Bx - Ax, Dy = By - Ay, L2 = Dx * Dx + Dy * Dy;
		const double K = L2 > 0.0 ? std::clamp(((Px - Ax) * Dx + (Py - Ay) * Dy) / L2, 0.0, 1.0) : 0.0;
		return std::hypot(Px - (Ax + Dx * K), Py - (Ay + Dy * K));
	}

	bool Grow(const std::string& Folder, LandFeed::Recording& Rec, std::vector<std::string>& Lines)
	{
		std::string Header, Error;
		if (!LandJson::ReadFile(Folder + "/header.json", Header) || !LandFeed::ReadRecording(Header, Rec, Error))
		{
			std::printf("%s/header.json: %s\n", Folder.c_str(), Error.c_str());
			return false;
		}
		std::ifstream In(Folder + "/deltas.jsonl");
		std::string Line;
		while (std::getline(In, Line))
		{
			if (!Line.empty()) Lines.push_back(Line);
		}
		return !Lines.empty();
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: land_command_test <folder> <setup folder>\n");
		return 2;
	}
	const std::string Folder = Argv[1], SetupFolder = Argv[2];

	// ---- The battle under way ----
	LandFeed::Recording Rec;
	std::vector<std::string> Deltas;
	if (!Grow(Folder, Rec, Deltas)) return 2;
	LandCommand::General G;
	G.Begin(Rec);
	Check(G.Side() == Rec.Me && Rec.Me >= 0, "the general commands the side the header says");
	std::ofstream Out(Folder + "/commands.jsonl");
	const double Times[] = { 6.0, 22.0, 31.0, 62.0, 121.0, 201.0 };
	size_t NextTime = 0;
	std::map<std::string, int> Made;
	int Tags = 0, Eased = 0, Chips[6] = { 0, 0, 0, 0, 0, 0 };
	double WorstOff = 0.0;
	int RatioSeen[2] = { 0, 0 }, MenSeen = -1;
	int WalkAt = 0, WalkForward = 0, WalkFoe = 0, WalkCover = 0, WalkFoesSeen = 0;
	for (const std::string& Text : Deltas)
	{
		LandFeed::Grown Grew;
		std::string Error;
		if (!LandFeed::AppendDelta(Text, Rec, Grew, Error))
		{
			std::printf("delta: %s\n", Error.c_str());
			return 1;
		}
		// Every half second of the screen (2 battle seconds behind the rules), the tags.
		for (double Screen = std::max(0.0, Rec.Fought - 2.5); Screen <= Rec.Fought - 2.0 + 1e-9; Screen += 0.25)
		{
			G.Update(Rec, Screen);
			const LandFeed::View* A = Rec.ViewAt(Screen);
			Check(A && G.Tags().size() == A->Units.size(), "a tag for every block of the view");
			const size_t At = static_cast<size_t>(A - Rec.Views.data());
			const LandFeed::View* B = At + 1 < Rec.Views.size() ? &Rec.Views[At + 1] : nullptr;
			for (const LandCommand::Tag& T : G.Tags())
			{
				++Tags;
				++Chips[static_cast<int>(T.Says)];
				const LandFeed::Unit* Ua = nullptr;
				const LandFeed::Unit* Ub = nullptr;
				for (const LandFeed::Unit& U : A->Units) if (U.Id == T.Block) Ua = &U;
				if (B) for (const LandFeed::Unit& U : B->Units) if (U.Id == T.Block) Ub = &U;
				Check(Ua != nullptr, "each tag is a block of the view");
				if (!Ua) continue;
				const double Off = Ub ? OffLine(T.X, T.Y, Ua->X, Ua->Y, Ub->X, Ub->Y) : std::hypot(T.X - Ua->X, T.Y - Ua->Y);
				WorstOff = std::max(WorstOff, Off);
				if (Ub && std::hypot(T.X - Ua->X, T.Y - Ua->Y) > 1e-6) ++Eased;
				Check((T.Says == LandCommand::Chip::Radio) == (T.bMine && Ua->OrderIn >= 0.0 && Ua->State != "gone" && Ua->State != "withdrawn"), "on the air exactly when his order is on its way");
				Check(!T.bOrderable || (T.bMine && T.State != "routing" && T.Kind != "fort"), "only his fighting blocks take orders");
				// (the plate's real men and the drawing ratio: said for every block met, never for one not met)
				Check(T.bKnown ? T.Men >= 0 && T.Men <= T.Men0 : T.Men < 0, "real men said for a block met, and only for one met");
				if (T.Ratio > 0 && T.Side >= 0 && T.Side <= 1) RatioSeen[T.Side] = T.Ratio;
				MenSeen = std::max(MenSeen, T.Men);
			}
		}

		if (NextTime >= sizeof(Times) / sizeof(Times[0]) || Rec.Fought + 1e-9 < Times[NextTime]) continue;
		// ---- At this moment: picking, then the orders ----
		const double Screen = Rec.Fought - 2.0;
		G.Update(Rec, Screen);
		{
			// (the walk's choices at this moment, as the game would make them)
			++WalkAt;
			const int Rifle = LandScript::Mine(G, "inf", {});
			const int Goer = Rifle >= 0 ? Rifle : LandScript::Mine(G, "", {});
			double Wx = 0.0, Wy = 0.0;
			if (Goer >= 0 && LandScript::Forward(G, Rec.Ground, Goer, 120.0, Wx, Wy))
			{
				++WalkForward;
				Check(Wx >= 0.0 && Wy >= 0.0 && Wx <= Rec.Ground.W && Wy <= Rec.Ground.H, "the walk's forward point is on the field");
			}
			bool bSeen = false;
			for (const LandCommand::Tag& T : G.Tags()) bSeen = bSeen || (!T.bMine && T.bKnown && !T.bHidden && !T.bVague && LandScript::Standing(T));
			WalkFoesSeen += bSeen ? 1 : 0;
			const LandCommand::Tag* Me = Goer >= 0 ? G.TagOf(Goer) : nullptr;
			const int Foe = Me ? LandScript::Foe(G, Me->X, Me->Y) : -1;
			Check((Foe >= 0) == (bSeen && Me != nullptr), "the walk finds an enemy to attack exactly when he can see one");
			if (const LandCommand::Tag* F = Foe >= 0 ? G.TagOf(Foe) : nullptr)
			{
				++WalkFoe;
				Check(!F->bMine && F->bKnown && !F->bHidden, "the walk attacks only an enemy he can see");
			}
			if (Goer >= 0 && LandScript::CoverFor(G, Rec.Ground, {}, Goer, Wx, Wy) == LandScript::Cover::Woods)
			{
				++WalkCover;
				Check(LandScript::Woods(Rec.Ground, Wx, Wy), "the walk's cover to fire into is woods");
			}
		}
		std::vector<const LandCommand::Tag*> Mine, Foes, Guns;
		for (const LandCommand::Tag& T : G.Tags())
		{
			if (T.bOrderable) (T.Kind == "art" ? Guns : Mine).push_back(&T);
			else if (!T.bMine && T.State != "gone" && T.State != "withdrawn") Foes.push_back(&T);
		}
		++NextTime;
		if (Mine.size() < 2 || Foes.empty()) continue;
		G.Pick(Mine[0]->Block, false);
		Check(G.Chosen() == std::vector<int>{ Mine[0]->Block }, "a click picks one");
		G.Pick(Mine[1]->Block, true);
		Check(G.Chosen().size() == 2, "Shift adds one");
		G.Pick(Mine[1]->Block, true);
		Check(G.Chosen().size() == 1, "Shift again puts it down");
		std::vector<int> All;
		for (const LandCommand::Tag& T : G.Tags()) All.push_back(T.Block);
		G.Choose(All, false);
		Check(G.Chosen().size() == Mine.size() + Guns.size(), "a box round everything takes only his blocks that take orders");
		const int First = G.Next(false);
		Check(G.Chosen().size() == 1 && G.Chosen()[0] == First, "Tab picks one");
		const int Second = G.Next(false);
		Check(Second != First || Mine.size() + Guns.size() == 1, "Tab moves on");
		Check(G.Next(true) == First, "Shift-Tab comes back");
		G.Clear();
		Check(G.Chosen().empty() && G.Click(0.0, 0.0, 10.0, false).empty(), "nothing in hand, no order");
		Check(G.MineAt(Mine[0]->X, Mine[0]->Y, 5.0) >= 0, "a click on his block's middle finds one of his");
		Check(G.EnemyAt(Foes[0]->X, Foes[0]->Y, 5.0) >= 0, "a click on an enemy's middle finds an enemy");

		// The orders, each as the rules take it, written out for the rules to answer.
		auto Give = [&](const std::string& Command, const char* Do)
		{
			Check(!Command.empty() && IsJson(Command, Do), std::string("a ") + Do + " order is the rules' JSON: " + Command);
			if (Command.empty()) return;
			++Made[Do];
			// (with the battle time he gave it, as the scene adds it)
			Out << Rec.Fought << '\t' << Command.substr(0, Command.size() - 1) << ",\"at\":" << LandCommand::Number(Screen) << "}\n";
		};
		G.Choose({ Mine[0]->Block, Mine[1]->Block }, false);
		const int Foe = G.EnemyAt(Foes[0]->X, Foes[0]->Y, 5.0);
		const std::string Hit = G.Click(Foes[0]->X, Foes[0]->Y, 5.0, false);
		Check(Hit == G.Attack(Foe), "a click on an enemy attacks it");
		Give(Hit, "attack");
		// (shown at once, before the views catch up with it; and gone once they have)
		const std::vector<int> Sent = G.Chosen();
		G.Sent(Sent, "attack", {}, Foe, Rec.Fought + 1.0);
		G.Update(Rec, Screen);
		for (int Id : Sent)
		{
			const LandCommand::Tag* T = G.TagOf(Id);
			Check(T && T->Says == LandCommand::Chip::Radio && T->OrderTarget == Foe && T->OrderKind == "attack", "an order just given shows on the air at once");
		}
		G.Update(Rec, Rec.Fought + 1.0);
		G.Update(Rec, Screen);
		for (int Id : Sent)
		{
			const LandCommand::Tag* T = G.TagOf(Id);
			const LandFeed::View* V = Rec.ViewAt(Screen);
			for (const LandFeed::Unit& U : V->Units)
			{
				if (U.Id == Id) Check(T && T->OrderTarget == U.OrderTarget, "once the views have caught up, the tag says what they say");
			}
		}
		G.Choose(Sent, false);
		// (open ground ahead of them: 150 m toward the enemy's edge from the first, clear of every enemy block)
		const double Ahead = G.Side() == 0 ? 150.0 : -150.0;
		const double Gx = Mine[0]->X, Gy = Mine[0]->Y + Ahead;
		if (G.EnemyAt(Gx, Gy, 30.0) < 0)
		{
			const std::string Go = G.Click(Gx, Gy, 30.0, NextTime % 2 == 0);
			Check(Go == G.Move(Gx, Gy, NextTime % 2 == 0), "a click on open ground goes there");
			Check((Go.find("\"run\":true") != std::string::npos) == (NextTime % 2 == 0), "at the double only when asked");
			Give(Go, "move");
		}
		const std::vector<LandCommand::Ghost> Ghosts = G.LineUp(Gx - 200.0, Gy, Gx + 200.0, Gy);
		Check(Ghosts.size() == 2 && std::fabs(Ghosts[0].X - (Gx - 200.0)) < 1e-6 && std::fabs(Ghosts[1].X - (Gx + 200.0)) < 1e-6, "a line puts the two at its ends");
		Check(!Ghosts.empty() && std::cos(Ghosts[0].Face - (G.Side() == 0 ? 0.0 : 3.14159265358979)) > 0.99, "a line faces the enemy's side");
		Give(G.Line(Gx - 200.0, Gy, Gx + 200.0, Gy), "line");
		Give(G.Hold(), "hold");
		// (reconnaissance by fire: one of his foot units fires into the cover where an enemy stands, no target named)
		G.Choose({ Mine[0]->Block }, false);
		const std::string Into = G.FireInto(Foes[0]->X, Foes[0]->Y);
		Check(IsJson(Into, "land:fire") && Into.find("\"target\"") == std::string::npos, "fire into cover names a point, not a target");
		Give(Into, "land:fire");
		G.Choose(Sent, false);
		if (!Guns.empty())
		{
			G.Choose({ Guns[0]->Block }, false);
			const std::string Fire = G.Click(Foes[0]->X, Foes[0]->Y, 5.0, false);
			Check(IsJson(Fire, "land:fire"), "guns alone fire on an enemy clicked");
			Give(Fire, "land:fire");
		}
		// (handed back: one he has taken, if any)
		for (const LandCommand::Tag* T : Mine)
		{
			if (!T->bTaken) continue;
			G.Choose({ T->Block }, false);
			Give(G.ToStaff(), "land:staff");
			break;
		}
		if (NextTime == sizeof(Times) / sizeof(Times[0]))
		{
			G.Choose({ Mine.back()->Block }, false);
			Give(G.FallBack(), "withdraw");
		}
		G.Clear();
	}
	Out.close();
	std::printf("tags: %d read, %d eased between views (worst %.6f m off the line between them); chips staff %d, yours %d, radio %d, enemy %d, unseen %d, gone %d\n",
		Tags, Eased, WorstOff, Chips[0], Chips[1], Chips[2], Chips[3], Chips[4], Chips[5]);
	Check(WorstOff < 1e-6, "every tag on the line between its two views");
	Check(Chips[2] > 0 && Chips[1] > 0, "some of his blocks were seen taken and with an order on the air");
	std::printf("plates: one figure for %d men on the attack, %d on the defence; the biggest block %d men\n", RatioSeen[0], RatioSeen[1], MenSeen);
	Check(RatioSeen[0] > 1 && RatioSeen[1] > 1 && MenSeen > 0, "each side's drawing ratio and its blocks' real men are known");
	std::printf("the walk: at %d moments, forward %d, an enemy seen %d and attacked %d, woods to fire into %d\n", WalkAt, WalkForward, WalkFoesSeen, WalkFoe, WalkCover);
	Check(WalkAt > 0 && WalkForward == WalkAt && WalkFoe > 0, "the walk finds a unit to send forward each time, and an enemy to attack once one is seen");
	std::printf("orders made:");
	for (const auto& [Do, N] : Made) std::printf(" %s %d", Do.c_str(), N);
	std::printf("\n");

	// ---- Hedge cutters and smoke on call (Brian, 22 November 14:19Z): the rules mark his own blocks' tools, and his
	// clipboard shows what they mark, never the enemy's. Then the same battle with every tool taken away (nothing shows),
	// and with one of his tanks given cutters and two blocks given smoke, and an enemy given both (not his: never shown). ----
	{
		LandCommand::General Ruled;
		Ruled.Begin(Rec);
		Ruled.Update(Rec, Rec.Fought - 2.0);
		int RuledSmoke = 0, RuledCutters = 0;
		for (const LandCommand::Tag& T : Ruled.Tags())
		{
			if (T.bMine && T.Smoke >= 0) RuledSmoke++;
			if (T.bMine && T.bCutter) RuledCutters++;
			Check(T.bMine || (!T.bCutter && T.Smoke < 0), "the enemy's tools are never shown (the rules' own marks)");
		}
		std::printf("the rules' tools: smoke for %d of his blocks, cutters for %d (no hedges on this field)\n", RuledSmoke, RuledCutters);
		Check(RuledSmoke > 0, "the rules give his sections smoke (smoke on call is on)");

		LandFeed::Recording Bare = Rec;
		for (LandFeed::View& V : Bare.Views)
			for (LandFeed::Unit& U : V.Units)
			{
				U.bCutter = false;
				U.Smoke = -1;
			}
		LandCommand::General Plain;
		Plain.Begin(Bare);
		Plain.Update(Bare, Bare.Fought - 2.0);
		std::vector<int> All;
		for (const LandCommand::Tag& T : Plain.Tags())
		{
			if (T.bOrderable) All.push_back(T.Block);
			Check(!T.bCutter && T.Smoke < 0, "no cutters or smoke shown where the rules say none");
		}
		Plain.Choose(All, false);
		Check(Plain.Cutter() < 0 && Plain.Smokers().empty() && Plain.Cut(10.0, 10.0).empty() && Plain.SmokeLine(0, 0, 10, 10).empty(), "no cut or smoke order without the tools");

		LandFeed::Recording Kit = Bare;
		int Tank = -1, Foe = -1;
		std::vector<int> Smoky;
		for (LandFeed::View& V : Kit.Views)
		{
			for (LandFeed::Unit& U : V.Units)
			{
				const LandCommand::Tag* T = Plain.TagOf(U.Id);
				if (!T || T->State == "gone" || T->State == "withdrawn") continue;
				if (T->bOrderable && T->Kind == "arm" && (Tank < 0 || Tank == U.Id))
				{
					Tank = U.Id;
					U.bCutter = true;
				}
				else if (T->bOrderable && T->Kind == "inf" && (Smoky.size() < 2 || std::find(Smoky.begin(), Smoky.end(), U.Id) != Smoky.end()))
				{
					if (std::find(Smoky.begin(), Smoky.end(), U.Id) == Smoky.end()) Smoky.push_back(U.Id);
					U.Smoke = 2;
				}
				else if (!T->bMine && (Foe < 0 || Foe == U.Id))
				{
					Foe = U.Id;
					U.bCutter = true;
					U.Smoke = 3;
				}
			}
		}
		Check(Tank >= 0 && Smoky.size() == 2 && Foe >= 0, "the battle has a tank of his, two of his sections and an enemy to give the tools to");
		LandCommand::General K;
		K.Begin(Kit);
		K.Update(Kit, Kit.Fought - 2.0);
		const LandCommand::Tag* Cutting = K.TagOf(Tank);
		const LandCommand::Tag* Theirs = K.TagOf(Foe);
		Check(Cutting && Cutting->bCutter, "his tank with cutters is shown with them");
		Check(Theirs && !Theirs->bCutter && Theirs->Smoke < 0, "the enemy's tools are never shown");
		K.Choose(Smoky, false);
		Check(K.Cutter() < 0 && K.Cut(5.0, 5.0).empty(), "no cut order without a cutter in hand");
		K.Choose(All, false);
		Check(K.Cutter() == Tank, "the cutter in hand is found among the rest");
		const std::string Cut = K.Cut(120.25, 340.5);
		Check(IsJson(Cut, "land:cut") && Cut == "{\"do\":\"land:cut\",\"units\":[" + std::to_string(Tank) + "],\"x\":120.25,\"y\":340.5}", "a cut is the rules' JSON for the one tank: " + Cut);
		const std::vector<int> Layers = K.Smokers();
		Check(Layers.size() == 2 && std::find(Layers.begin(), Layers.end(), Tank) == Layers.end(), "only those with smoke lay it");
		const std::string Smoke = K.SmokeLine(100.0, 200.0, 300.0, 200.0);
		Check(IsJson(Smoke, "land:smoke") && Smoke.find("\"x0\":100") != std::string::npos && Smoke.find("\"y1\":200") != std::string::npos && Smoke.find(std::to_string(Tank) + "]") == std::string::npos, "smoke is the rules' JSON for a line, from those with it: " + Smoke);
		std::printf("cutters and smoke: tank %d cuts (%s), sections lay smoke (%s)\n", Tank, Cut.c_str(), Smoke.c_str());
	}

	// ---- Hedge snapping: the nearest point of a hedge within reach ----
	{
		const std::vector<std::pair<double, double>> Hedge = { { 0.0, 0.0 }, { 100.0, 0.0 }, { 100.0, 100.0 } };
		const std::vector<std::pair<double, double>> Far = { { 500.0, 500.0 }, { 600.0, 500.0 } };
		const std::vector<std::pair<double, double>> Dot = { { 50.0, 60.0 } };
		double Sx = -1.0, Sy = -1.0;
		Check(LandCommand::NearestOnLines({ &Hedge, &Far }, 40.0, 7.0, 10.0, Sx, Sy) && std::fabs(Sx - 40.0) < 1e-9 && std::fabs(Sy) < 1e-9, "a point near a hedge snaps onto it");
		Check(LandCommand::NearestOnLines({ &Hedge, &Far }, 104.0, 50.0, 10.0, Sx, Sy) && std::fabs(Sx - 100.0) < 1e-9 && std::fabs(Sy - 50.0) < 1e-9, "onto the nearest of its runs");
		Check(LandCommand::NearestOnLines({ &Hedge, &Far }, 103.0, -3.0, 10.0, Sx, Sy) && std::fabs(Sx - 100.0) < 1e-9 && std::fabs(Sy) < 1e-9, "a point off a corner snaps to the corner");
		Check(!LandCommand::NearestOnLines({ &Hedge, &Far }, 50.0, 50.0, 10.0, Sx, Sy), "none where no hedge comes within reach");
		Check(LandCommand::NearestOnLines({ nullptr, &Dot }, 52.0, 60.0, 5.0, Sx, Sy) && std::fabs(Sx - 50.0) < 1e-9, "a hedge of one point, and an empty slot, are borne");
	}

	// ---- The setup step: his ground ----
	LandFeed::Recording Setup;
	std::vector<std::string> SetupDeltas;
	if (!Grow(SetupFolder, Setup, SetupDeltas)) return 2;
	LandCommand::General S;
	S.Begin(Setup);
	for (const std::string& Text : SetupDeltas)
	{
		LandFeed::Grown Grew;
		std::string Error;
		Check(LandFeed::AppendDelta(Text, Setup, Grew, Error), "the setup's first piece reads: " + Error);
	}
	S.Update(Setup, 0.0);
	Check(S.Deploying() && Setup.bSetup, "the setup step is open");
	const double* Room = S.Room();
	Check(Room[2] > Room[0] && Room[3] > Room[1], "his ground has room");
	std::ofstream Places(SetupFolder + "/placings.jsonl");
	int Asked = 0, Yes = 0, Standing = 0;
	for (const LandCommand::Tag& T : S.Tags())
	{
		if (!T.bOrderable) continue;
		// (where it stands, drawn up by his staff, and a grid over the field: what the screen says, for the rules to answer)
		Standing += S.MayStand(T.Block, T.X, T.Y) ? 1 : 0;
		for (double X = 25.0; X < Setup.Ground.W; X += Setup.Ground.W / 9.0)
		{
			for (double Y = 25.0; Y < Setup.Ground.H; Y += Setup.Ground.H / 9.0)
			{
				const bool bMay = S.MayStand(T.Block, X, Y);
				Places << T.Block << '\t' << LandCommand::Number(X) << '\t' << LandCommand::Number(Y) << '\t' << (bMay ? 1 : 0) << '\n';
				++Asked;
				Yes += bMay ? 1 : 0;
			}
		}
	}
	std::printf("setup: room [%.0f, %.0f, %.0f, %.0f]; %d placings asked, %d inside his ground; %d blocks stand where their staff drew them up inside it\n",
		Room[0], Room[1], Room[2], Room[3], Asked, Yes, Standing);
	Check(Asked > 0 && Yes > 0 && Yes < Asked, "some places on his ground, some off it");

	// ---- The walk's two units into the woods (the rules are asked to place them and hide them: command-check.ts) ----
	std::ofstream WoodsOut(SetupFolder + "/woods.jsonl");
	std::vector<int> Carried;
	std::vector<LandScript::Point> Taken;
	for (int K = 0; K < 2; ++K)
	{
		int Block = LandScript::Mine(S, "inf", Carried);
		if (Block < 0) Block = LandScript::Mine(S, "", Carried);
		double Wx = 0.0, Wy = 0.0;
		if (Block < 0 || !LandScript::WoodsFor(S, Setup.Ground, Block, Taken, Wx, Wy)) continue;
		Check(LandScript::Woods(Setup.Ground, Wx, Wy) && S.MayStand(Block, Wx, Wy), "the walk's woods are woods, on his ground");
		const LandCommand::Tag* T = S.TagOf(Block);
		std::printf("the walk: block %d (%s) carried %.0f m into the woods at %.0f, %.0f\n", Block, T ? T->Short.c_str() : "?", T ? std::hypot(Wx - T->X, Wy - T->Y) : 0.0, Wx, Wy);
		WoodsOut << Block << '\t' << LandCommand::Number(Wx) << '\t' << LandCommand::Number(Wy) << '\n';
		Carried.push_back(Block);
		Taken.push_back({ Wx, Wy });
	}
	WoodsOut.close();
	Check(Carried.size() == 2, "the walk finds woods on his ground for two of his units");

	// ---- Drawing up while he places his army (Brian, 26 November 2026: "move my units in a formation before the battle
	// begins"): several carried as one, a line drawn on his ground, a formation's word. What the screen would ask is
	// written for the rules to answer (command-check.ts: drawn.jsonl, drawups.jsonl). ----
	{
		std::vector<int> All;
		for (const LandCommand::Tag& T : S.Tags())
		{
			if (T.bOrderable) All.push_back(T.Block);
		}
		Check(All.size() >= 3, "he has several units to draw up");
		std::ofstream Drawn(SetupFolder + "/drawn.jsonl"), DrawUps(SetupFolder + "/drawups.jsonl");
		// One in hand, or the one held not in hand: carried alone.
		S.Choose({ All[0] }, false);
		const LandCommand::Tag* Held = S.TagOf(All[0]);
		std::vector<LandCommand::Ghost> One = S.CarryAll(All[0], Held->X + 10.0, Held->Y);
		Check(One.size() == 1 && One[0].Block == All[0] && std::fabs(One[0].X - Held->X - 10.0) < 1e-9, "one in hand is carried alone");
		S.Choose({ All[1], All[2] }, false);
		One = S.CarryAll(All[0], Held->X, Held->Y);
		Check(One.size() == 1 && One[0].Block == All[0], "a unit not in hand is carried alone, whoever is in hand");
		// All in hand, one held: every one keeps its place from it and its facing.
		S.Choose(All, false);
		const double MidX = (Room[0] + Room[2]) * 0.5, MidY = (Room[1] + Room[3]) * 0.5;
		double Cx = 0.0, Cy = 0.0;
		for (int Id : All)
		{
			Cx += S.TagOf(Id)->X / static_cast<double>(All.size());
			Cy += S.TagOf(Id)->Y / static_cast<double>(All.size());
		}
		// (a small step toward the middle of his ground, so the whole group stays on it)
		const double StepX = MidX > Cx ? 15.0 : -15.0, StepY = MidY > Cy ? 5.0 : -5.0;
		const std::vector<LandCommand::Ghost> Group = S.CarryAll(All[0], Held->X + StepX, Held->Y + StepY);
		Check(Group.size() == All.size(), "several in hand are carried as one");
		bool bKept = true;
		for (const LandCommand::Ghost& One2 : Group)
		{
			const LandCommand::Tag* T = S.TagOf(One2.Block);
			bKept = bKept && T && std::fabs(One2.X - T->X - StepX) < 1e-9 && std::fabs(One2.Y - T->Y - StepY) < 1e-9 && std::fabs(One2.Face - T->Face) < 1e-12;
		}
		Check(bKept, "each keeps its place from the one he holds, and its facing");
		Check(S.MayStandAll(Group), "a small step on his own ground: they may all stand");
		Check(!S.MayStandAll(S.CarryAll(All[0], Held->X, Room[3] + 500.0)) && !S.MayStandAll({}), "carried off his ground, or none carried: not all may stand");
		const std::string GroupJson = LandCommand::General::PlaceMany(Group);
		LandJson::Json GroupRead;
		std::string GroupError;
		Check(LandJson::Parse(GroupJson, GroupRead, GroupError) && GroupRead.IsArray() && GroupRead.Count() == Group.size() && GroupRead.At(0).Count() == 4, "the group's places are a list of [id, x, y, face]");
		Check(LandCommand::General::PlaceMany({}).empty(), "no places, nothing asked");
		Drawn << "carry\t" << GroupJson << '\n';
		// A line drawn across the middle of his ground, long enough for them: from end to end, facing the enemy.
		const double Half = std::min(1000.0, (Room[2] - Room[0]) * 0.5 - 100.0);
		const std::vector<LandCommand::Ghost> Line = S.DrawnLine(MidX - Half, MidY, MidX + Half, MidY);
		Check(Line.size() == All.size(), "a line drawn stands every one in hand");
		bool bFaces = true, bAcross = true, bApart = true;
		for (size_t K = 0; K < Line.size(); ++K)
		{
			bFaces = bFaces && std::cos(Line[K].Face - (S.Side() == 0 ? 0.0 : 3.14159265358979)) > 0.99;
			bAcross = bAcross && std::fabs(Line[K].Y - MidY) < 1e-6 && Line[K].X > MidX - Half - 1e-6 && Line[K].X < MidX + Half + 1e-6;
			if (K > 0)
			{
				const LandCommand::Tag* A = S.TagOf(Line[K - 1].Block);
				const LandCommand::Tag* B = S.TagOf(Line[K].Block);
				bApart = bApart && Line[K].X - Line[K - 1].X >= (A->W + B->W) * 0.5 + 12.0;
			}
		}
		Check(bFaces, "a drawn line faces the enemy's side");
		Check(bAcross, "a level line stands them along it, between its ends");
		Check(bApart, "no two nearer than they can stand");
		if (Line.size() >= 2)
		{
			Check(std::fabs(Line.front().X - (MidX - Half)) < 1e-6 && std::fabs(Line.back().X - (MidX + Half)) < 1e-6, "a line long enough runs from end to end");
		}
		Check(S.MayStandAll(Line), "a line across the middle of his ground: they may all stand");
		Drawn << "line\t" << LandCommand::General::PlaceMany(Line) << '\n';
		// A click (no length): about the point, as close as they can stand, level.
		const std::vector<LandCommand::Ghost> Close = S.DrawnLine(MidX, MidY, MidX + 2.0, MidY + 1.0);
		double CloseMid = 0.0;
		bool bCloseApart = true;
		for (size_t K = 0; K < Close.size(); ++K)
		{
			CloseMid += Close[K].X / static_cast<double>(Close.size());
			if (K > 0)
			{
				const LandCommand::Tag* A = S.TagOf(Close[K - 1].Block);
				const LandCommand::Tag* B = S.TagOf(Close[K].Block);
				bCloseApart = bCloseApart && Close[K].X - Close[K - 1].X >= (A->W + B->W) * 0.5 + 12.0;
			}
		}
		Check(Close.size() == All.size() && bCloseApart && std::fabs((Close.front().X + Close.back().X) * 0.5 - (MidX + 1.0)) < 1e-6, "a line too short for them: about its middle, as close as they can stand");
		(void)CloseMid;
		if (S.MayStandAll(Close)) Drawn << "close\t" << LandCommand::General::PlaceMany(Close) << '\n';
		// A slanted line: up the field more than across it, they stand clear by their depths.
		const std::vector<LandCommand::Ghost> Slant = S.DrawnLine(MidX - 40.0, Room[1] + 40.0, MidX + 40.0, Room[3] - 40.0);
		if (S.MayStandAll(Slant)) Drawn << "slant\t" << LandCommand::General::PlaceMany(Slant) << '\n';
		Drawn.close();
		// A formation's word: those in hand named; none in hand, none named (every one of his).
		S.Choose({ All[0], All[1], All[2] }, false);
		const std::string Three = S.DrawUp("wedge");
		Check(Three == "[\"wedge\",[" + std::to_string(All[0]) + "," + std::to_string(All[1]) + "," + std::to_string(All[2]) + "]]", "a formation's word names those in hand");
		DrawUps << Three << '\n';
		S.Clear();
		Check(S.DrawUp("line") == "[\"line\",[]]", "none in hand: none named");
		for (const char* Kind : { "line", "two", "wedge", "column" }) DrawUps << S.DrawUp(Kind) << '\n';
		DrawUps.close();
		std::printf("drawing up: %zu units; a group carried as one, a line of %.0f m, a click's line and a slanted one written for the rules\n", All.size(), Half * 2.0);

		// ---- Groups on the number keys (Brian, 26 November 2026: "hit the #1 and the units tied to it are all selected") ----
		typedef LandCommand::General::Tied Tied;
		Check(S.Tie(1).Count == 0 && LandCommand::General::TiedWords(S.Tie(1), 1).empty(), "nothing in hand, nothing tied");
		Check(!S.Take(1, false) && S.GroupOf(All[0]) == 0, "a number with nothing tied takes nothing");
		S.Choose({ All[0], All[1] }, false);
		Tied T1 = S.Tie(1);
		Check(T1.Count == 2 && T1.Moved == 0 && T1.From.empty() && LandCommand::General::TiedWords(T1, 1) == "Two units tied to 1.", "two tied to 1: " + LandCommand::General::TiedWords(T1, 1));
		Check(S.GroupOf(All[0]) == 1 && S.GroupOf(All[1]) == 1 && S.GroupOf(All[2]) == 0 && S.TagOf(All[0])->Group == 1 && S.TagOf(All[2])->Group == 0, "each tied unit's tag carries its number");
		S.Choose({ All[2] }, false);
		Tied T2 = S.Tie(2);
		Check(LandCommand::General::TiedWords(T2, 2) == "One unit tied to 2.", "one tied to 2: " + LandCommand::General::TiedWords(T2, 2));
		// Taking a group: it alone in hand, or added to those in hand; his hand kept where the number is empty.
		Check(S.Take(1, false) && S.Chosen().size() == 2 && S.TagOf(All[0])->bChosen && !S.TagOf(All[2])->bChosen, "the number takes its units in hand, alone");
		Check(S.Take(2, true) && S.Chosen().size() == 3, "with Shift it is added to those in hand");
		Check(!S.Take(7, false) && S.Chosen().size() == 3, "an empty number leaves his hand as it was");
		// One number a unit: the last tie wins, and the note says where it came from.
		S.Choose({ All[1], All[2] }, false);
		Tied T3 = S.Tie(3);
		Check(T3.Count == 2 && T3.Moved == 2 && T3.From.size() == 2 && LandCommand::General::TiedWords(T3, 3) == "Two units tied to 3; two of them leave 1 and 2.", "the last tie wins: " + LandCommand::General::TiedWords(T3, 3));
		Check(S.GroupOf(All[1]) == 3 && S.GroupOf(All[2]) == 3 && S.GroupOf(All[0]) == 1 && !S.Take(2, false), "each unit has one number, and 2 is now empty");
		S.Choose({ All[0], All[1] }, false);
		Tied T4 = S.Tie(3);
		Check(LandCommand::General::TiedWords(T4, 3) == "Two units tied to 3; one of them leaves 1.", "one joins from 1: " + LandCommand::General::TiedWords(T4, 3));
		Tied Many;
		Many.Count = 12;
		Many.Moved = 3;
		Many.From = { 2, 4, 5 };
		Check(LandCommand::General::TiedWords(Many, 9) == "12 units tied to 9; three of them leave 2, 4 and 5.", "counts in words to ten: " + LandCommand::General::TiedWords(Many, 9));
		double Gx = 0.0, Gy = 0.0;
		Check(S.GroupAt(3, Gx, Gy) && std::fabs(Gx - (S.TagOf(All[0])->X + S.TagOf(All[1])->X + S.TagOf(All[2])->X) / 3.0) < 1e-9, "a group's middle is the middle of its units");
		Check(!S.GroupAt(1, Gx, Gy), "an empty group has no middle");
		Check(S.Tie(0).Count == 0 && S.Tie(10).Count == 0, "only 1 to 9 are numbers");
		// The tags keep their numbers when they are read again, and a new battle begins with none.
		S.Update(Setup, 0.0);
		Check(S.TagOf(All[0])->Group == 3 && S.Take(3, false) && S.Chosen().size() == 3, "the numbers hold when the tags are read again");
		LandCommand::General Fresh = S;
		Fresh.Begin(Setup);
		Fresh.Update(Setup, 0.0);
		Check(Fresh.GroupOf(All[0]) == 0 && !Fresh.Take(3, false), "a new battle begins with nothing tied");
		S.Clear();
	}

	// ---- Neighbours join the fight (release 2): a battle with a corps coming up from next door on each side ----
	if (Argc >= 4)
	{
		LandFeed::Recording Join;
		std::vector<std::string> JoinDeltas;
		if (!Grow(Argv[3], Join, JoinDeltas)) return 2;
		LandCommand::General J;
		J.Begin(Join);
		const int Me = Join.Me;
		std::vector<int> Joiners[2];
		for (const LandFeed::Block& B : Join.Blocks)
		{
			if (!B.bJoins || B.Side < 0 || B.Side > 1) continue;
			Joiners[B.Side].push_back(B.Id);
			Check(B.Corps != 0, "a joiner's corps is said");
		}
		Check(Me >= 0 && Me <= 1 && !Joiners[0].empty() && !Joiners[1].empty(), "the header names joiners on both sides");
		int Told = 0, FoeTold = 0, CorpsMen = 0;
		double LastIn = 1e9, LastToldAt = -1.0, ArrivedAt = -1.0;
		// (hold until relieved and the rider ahead: join-check.ts tells one of his blocks to wait, and his corps sends a rider)
		int Waiting = -1, WaitTags = 0, WaitWrong = 0, Riders = 0, RiderPicked = 0, HelpSaid = 0;
		double WaitDue = -1.0;
		std::string RiderSays, WaitOrder;
		std::string CorpsName, CorpsEdge;
		for (const std::string& Text : JoinDeltas)
		{
			LandFeed::Grown Grew;
			std::string Error;
			if (!LandFeed::AppendDelta(Text, Join, Grew, Error))
			{
				std::printf("join delta: %s\n", Error.c_str());
				return 1;
			}
			const double Screen = Join.Fought - 2.0;
			if (Screen < 0.0 || Me < 0 || Me > 1) continue;
			J.Update(Join, Screen);
			for (const LandCommand::Tag& T : J.Tags())
			{
				if (!T.bWaits) continue;
				if (!T.bMine || (Waiting >= 0 && T.Block != Waiting)) ++WaitWrong;
				Waiting = T.Block;
				WaitDue = T.WaitDue;
				++WaitTags;
			}
			for (const LandFeed::Rider& R : J.Riders())
			{
				Check(R.Side == Me && !R.Says.empty() && R.Men > 0, "a rider is his own side's, and says what is coming");
				++Riders;
				RiderSays = R.Says;
				if (J.RiderAt(R.X + 3.0, R.Y - 2.0, 10.0) >= 0) ++RiderPicked;
			}
			if (J.HelpComing()) ++HelpSaid;
			if (WaitOrder.empty() && J.HelpComing())
			{
				// (HOLD TILL RELIEVED with everything of his in hand: only those on foot are told)
				std::vector<int> All;
				for (const LandCommand::Tag& T : J.Tags())
				{
					if (T.bOrderable) All.push_back(T.Block);
				}
				J.Choose(All, false);
				WaitOrder = J.Wait();
				for (int Id : J.Waiters())
				{
					const LandCommand::Tag* T = J.TagOf(Id);
					Check(T && T->Kind != "arm" && T->Kind != "art" && T->Kind != "mot", "only blocks on foot are told to wait");
				}
				J.Clear();
			}
			const LandCommand::Neighbour* Own = nullptr;
			for (const LandCommand::Neighbour& N : J.Neighbours())
			{
				Check(N.Units > 0 && N.In >= 0.0 && !N.Edge.empty(), "a corps coming says how many units, how soon and on which edge");
				Check(N.X >= -1.0 && N.Y >= -1.0 && N.X <= Join.Ground.W + 1.0 && N.Y <= Join.Ground.H + 1.0, "it comes on at a point on the field's edge");
				Check(N.bMine == (N.Side == Me), "his own corps is his, the enemy's the enemy's");
				Check(N.bMine ? N.Men > 0 : N.Men < 0, "his own corps' men said, the enemy's not counted");
				if (N.bMine)
				{
					Own = &N;
					++Told;
				}
				else
				{
					++FoeTold;
				}
			}
			bool bOn = false;
			for (const LandCommand::Tag& T : J.Tags())
			{
				if (std::find(Joiners[Me].begin(), Joiners[Me].end(), T.Block) != Joiners[Me].end()) bOn = true;
			}
			if (Own)
			{
				Check(!bOn, "his corps is not on the field while it is still coming");
				Check(Own->In <= LastIn + 1e-6, "its time counts down");
				Check(!Own->Name.empty(), "his corps coming is named");
				LastIn = Own->In;
				LastToldAt = Screen;
				CorpsName = Own->Name;
				CorpsEdge = Own->Edge;
				CorpsMen = Own->Men;
			}
			if (bOn && ArrivedAt < 0.0) ArrivedAt = Screen;
		}
		std::printf("neighbours: his corps (%s, %d men, by the %s edge) told coming %d times, the enemy's %d; last told at %.1f with %.1f s to go; on the field from %.1f\n",
			CorpsName.c_str(), CorpsMen, CorpsEdge.c_str(), Told, FoeTold, LastToldAt, LastIn, ArrivedAt);
		Check(Told > 0, "his corps coming is told before it comes");
		Check(ArrivedAt >= 0.0, "his corps walks on");
		Check(ArrivedAt < 0.0 || LastToldAt < 0.0 || ArrivedAt - LastToldAt <= 1.01, "it is on the field the moment it stops coming");
		Check(LastIn <= 1.01, "its time runs down to its coming");
		std::printf("hold until relieved: block %d waits (help due at %.1f) in %d screens, help coming said in %d; the wait order %s; the rider in %d screens (%d found by a click near him): %s\n",
			Waiting, WaitDue, WaitTags, HelpSaid, WaitOrder.c_str(), Riders, RiderPicked, RiderSays.c_str());
		Check(WaitTags > 0 && WaitWrong == 0 && WaitDue > 0.0, "his waiting block's tag says it waits and when help is due, and no other's does");
		Check(IsJson(WaitOrder, "land:wait"), "the wait order is the rules' JSON");
		Check(Riders > 0 && RiderPicked == Riders, "his corps' despatch rider is read and found by a click near him");
	}

	std::printf(Failures ? "FAIL (%d)\n" : "clipboard held\n", Failures);
	return Failures ? 1 : 0;
}
