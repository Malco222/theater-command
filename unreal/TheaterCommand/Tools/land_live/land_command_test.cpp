// The general's clipboard's cloud test (release 2): LandCommand::General reads a live battle as the 3D game grows it
// (header, then each delta through LandFeed::AppendDelta) and the test checks what the screen will rely on:
//  - every block of the view has its tag, eased from the view at or before the screen's time toward the next, never
//    off the line between them; the chips say whose it is (his staff's, his own, on the air, the enemy's, unseen);
//  - picking, Shift-picking, a box, Tab and clearing take only his blocks that can take orders;
//  - a click on an enemy attacks it (guns alone fire on it), on open ground goes there, at the double when asked; a
//    line, hold, fall back and back to staff each make the rules' own command; every command is JSON the rules read;
//  - in the setup step, a block's ground check (MayStand) never says yes where the rules' ground says no for want of room.
// The commands and placings it makes are written out (commands.jsonl, placings.jsonl) for command-check.ts, which gives
// them to the rules at the same moments of the same battle and checks each is taken.
// Usage: land_command_test <folder (header.json, deltas.jsonl)> <setup folder (header.json, deltas.jsonl)>

#include "Battle/LandCommand.h"
#include "Battle/LandFeed.h"
#include "Battle/LandJson.h"

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
			}
		}

		if (NextTime >= sizeof(Times) / sizeof(Times[0]) || Rec.Fought + 1e-9 < Times[NextTime]) continue;
		// ---- At this moment: picking, then the orders ----
		const double Screen = Rec.Fought - 2.0;
		G.Update(Rec, Screen);
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
	std::printf("orders made:");
	for (const auto& [Do, N] : Made) std::printf(" %s %d", Do.c_str(), N);
	std::printf("\n");

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

	std::printf(Failures ? "FAIL (%d)\n" : "clipboard held\n", Failures);
	return Failures ? 1 : 0;
}
