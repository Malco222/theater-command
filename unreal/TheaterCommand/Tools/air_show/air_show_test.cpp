// The air show's cloud test (ruling 238; air 3d-handoff.md sections 9 and 10): plays a recorded air battle
// (Content/Data/AirBattles/<key>.json, from the rules repo's tools/air-record) through the very code the game runs
// (Source/TheaterCommand/Battle/AirShow and AirFeed), at the game's frame rate, and checks that every aircraft flew as
// an aircraft does. It writes a track of the flight for the check pictures (air_show_frames.py).
//
//   Tools/air_show/run.sh Content/Data/AirBattles/oahu.json [out-dir]
//
// Plain C++20, no engine: it builds anywhere with g++ or clang++.

#include "Battle/AirDesk.h"
#include "Battle/AirFeed.h"
#include "Battle/AirShow.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace
{
	const char* FxName(AirShow::FxKind K)
	{
		switch (K)
		{
		case AirShow::FxKind::Gun: return "gun";
		case AirShow::FxKind::GunStop: return "gunstop";
		case AirShow::FxKind::Hit: return "hit";
		case AirShow::FxKind::TrailStart: return "trailstart";
		case AirShow::FxKind::TrailMove: return "trailmove";
		case AirShow::FxKind::TrailStop: return "trailstop";
		case AirShow::FxKind::Flak: return "flak";
		case AirShow::FxKind::Blast: return "blast";
		case AirShow::FxKind::Fall: return "fall";
		case AirShow::FxKind::FallMove: return "fallmove";
		case AirShow::FxKind::FallStop: return "fallstop";
		case AirShow::FxKind::Splash: return "splash";
		case AirShow::FxKind::Crash: return "crash";
		case AirShow::FxKind::BombHit: return "bombhit";
		case AirShow::FxKind::Sound: return "sound";
		case AirShow::FxKind::Dispatch: return "dispatch";
		case AirShow::FxKind::ShipHit: return "shiphit";
		case AirShow::FxKind::ShipSunk: return "shipsunk";
		case AirShow::FxKind::Smoke: return "smoke";
		}
		return "?";
	}

	struct Limit
	{
		const char* Name;
		bool bOk;
		std::string Got;
		const char* Want;
	};

	std::string Num(double V, int Places = 1)
	{
		char Buf[64];
		std::snprintf(Buf, sizeof Buf, "%.*f", Places, V);
		return Buf;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: air_show_test <recording.json> [out-dir] [frames-per-second]\n");
		return 2;
	}
	const std::string Path = Argv[1];
	const std::string OutDir = Argc > 2 ? Argv[2] : ".";
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
	const auto Read0 = std::chrono::steady_clock::now();
	if (!AirFeed::ReadRecording(Buffer.str(), Rec, Error))
	{
		std::fprintf(stderr, "%s: %s\n", Path.c_str(), Error.c_str());
		return 2;
	}
	const double ReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Read0).count();

	AirShow::Show Show(Rec.Seed ? Rec.Seed : 238);
	AirFeed::Apply(Rec.Board, Show);
	Show.Eye = AirShow::Vec(Rec.Board.TargetX, Rec.Board.TargetY - 6000.0, 5000.0);

	// The track: every half battle second, each aircraft and each small thing; every effect as it comes.
	std::ofstream Track(OutDir + "/" + Rec.Key + "-track.csv");
	Track << "t,kind,key,group,side,fighter,model,x,y,z,heading,pitch,bank,shown,falling,ground,extra\n";
	std::map<std::string, int> FxCount;
	std::map<std::string, int> Sounds;
	int MostPlanes = 0, MostThings = 0, LiveGuns = 0, MostLiveGuns = 0;
	std::map<int, bool> GunOn;
	double WorstFrameMs = 0.0, TotalMs = 0.0;
	long Frames = 0;
	int EventsLost = 0, EventsChutes = 0;
	// The falls as the scene hands them to the effects kit: a start, its moves, an end; and the kit's room for debris.
	AirShow::DebrisRoom Debris;
	std::map<int, std::string> FallsOpen;
	int FallStarts = 0, FallStrays = 0, MostDebris = 0;
	// The scene's room for trails, as on the newer kit (24) and an older one (10), a burning aircraft's own first.
	AirShow::TrailRoom Trails24, Trails10;
	Trails24.Most = 24;
	Trails10.Most = 10;
	int PieceTrails = 0, OwnTrails = 0;
	// No group still flies the place's stand-in once a view has named its model.
	int Guessed = 0;
	for (const AirFeed::Frame& F : Rec.Views)
		for (const AirShow::Group& G : F.Groups)
			Guessed += G.bModelGuessed ? 1 : 0;
	for (const AirFeed::Frame& F : Rec.Views)
		for (const AirShow::Event& E : F.Events)
			if (E.What == "air:lost") { ++EventsLost; EventsChutes += E.Chutes; }

	const double Dt = 1.0 / Fps;
	double NextSample = 0.0;
	// The game holds one view in hand ahead of what it shows, so the picture between views has somewhere to go.
	size_t Taken = 0;
	auto TakeNext = [&]()
	{
		const AirFeed::Frame& F = Rec.Views[Taken++];
		Show.Take(F.Head.T, F.Groups, F.Events, F.Ships);
	};
	TakeNext();
	if (Taken < Rec.Views.size()) TakeNext();
	double T = Rec.Views.front().Head.T;
	const double EndT = Rec.Views.back().Head.T;
	while (T < EndT - 1e-9)
	{
		T = std::min(EndT, T + Dt);
		while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
		const auto F0 = std::chrono::steady_clock::now();
		Show.Advance(T);
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - F0).count();
		WorstFrameMs = std::max(WorstFrameMs, Ms);
		TotalMs += Ms;
		++Frames;

		for (const AirShow::Fx& E : Show.Effects)
		{
			const char* Name = FxName(E.Kind);
			++FxCount[Name];
			if (E.Kind == AirShow::FxKind::Sound) ++Sounds[E.What];
			if (E.Kind == AirShow::FxKind::Gun) GunOn[E.Key] = true;
			if (E.Kind == AirShow::FxKind::GunStop) GunOn.erase(E.Key);
			if (E.Kind == AirShow::FxKind::Fall)
			{
				++FallStarts;
				Debris.Take(Show.AirNow(), E);
				if (E.What != "explode" && E.What != "burn" && E.What != "spin") ++FallStrays;
				if (E.What != "explode") FallsOpen[E.Key] = E.What;
			}
			if (E.Kind == AirShow::FxKind::FallMove && !FallsOpen.count(E.Key)) ++FallStrays;
			if (E.Kind == AirShow::FxKind::FallStop && FallsOpen.erase(E.Key) == 0) ++FallStrays;
			if (E.Kind == AirShow::FxKind::TrailStart)
			{
				const bool bPiece = E.Text == "piece";
				PieceTrails += bPiece ? 1 : 0;
				OwnTrails += bPiece ? 0 : 1;
				int Out = 0;
				Trails24.Start(E.Key, bPiece, Out);
				Trails10.Start(E.Key, bPiece, Out);
			}
			if (E.Kind == AirShow::FxKind::TrailStop)
			{
				Trails24.Stop(E.Key);
				Trails10.Stop(E.Key);
			}
			if (E.Kind != AirShow::FxKind::Gun && E.Kind != AirShow::FxKind::TrailMove && E.Kind != AirShow::FxKind::FallMove && E.Kind != AirShow::FxKind::Hit)
			{
				std::string Text = E.What;
				for (char& C : Text) if (C == ',' || C == '\n') C = ';';
				Track << Num(Show.Shown(), 3) << ",fx:" << Name << "," << E.Key << ",0," << E.Side << ",0," << Text << ","
				      << Num(E.At.X) << "," << Num(E.At.Y) << "," << Num(E.At.Z) << ",0,0,0,1,0,0," << Num(E.Size) << "\n";
			}
			else if (E.Kind == AirShow::FxKind::Gun && Show.Shown() >= NextSample - 1e-9)
			{
				Track << Num(Show.Shown(), 3) << ",fx:gun," << E.Key << ",0," << E.Side << ",0," << E.What << ","
				      << Num(E.At.X) << "," << Num(E.At.Y) << "," << Num(E.At.Z) << ",0,0,0,1,0,0,"
				      << Num(E.At.X + E.Aim.X * 400.0) << ";" << Num(E.At.Y + E.Aim.Y * 400.0) << ";" << Num(E.At.Z + E.Aim.Z * 400.0) << "\n";
			}
		}
		Show.Effects.clear();
		if (std::getenv("AIR_SHOW_TRACE") && std::fmod(Show.Shown(), 10.0) < Dt * 0.999)
			std::fprintf(stderr, "== %.1f\n%s", Show.Shown(), Show.Describe().c_str());
		MostDebris = std::max(MostDebris, Debris.Holding(Show.AirNow()));
		LiveGuns = static_cast<int>(GunOn.size());
		MostLiveGuns = std::max(MostLiveGuns, LiveGuns);
		int Shown = 0;
		for (const AirShow::PlaneOut& P : Show.Planes()) if (P.bShown) ++Shown;
		MostPlanes = std::max(MostPlanes, Shown);
		MostThings = std::max(MostThings, static_cast<int>(Show.Things().size()));

		if (Show.Shown() >= NextSample - 1e-9)
		{
			NextSample += 0.5;
			for (const AirShow::PlaneOut& P : Show.Planes())
				Track << Num(Show.Shown(), 3) << ",plane," << P.Key << "," << P.Group << "," << P.Side << "," << (P.bFighter ? 1 : 0) << ","
				      << P.Model << "," << Num(P.At.X) << "," << Num(P.At.Y) << "," << Num(P.At.Z) << "," << Num(P.Heading) << ","
				      << Num(P.Pitch) << "," << Num(P.Bank) << "," << (P.bShown ? 1 : 0) << "," << (P.bFalling ? 1 : 0) << ","
				      << (P.bOnGround ? 1 : 0) << "," << Num(P.Span) << "\n";
			for (const AirShow::Thing& S : Show.Things())
				Track << Num(Show.Shown(), 3) << ",thing:" << (S.Kind == AirShow::ThingKind::Chute ? "chute" : S.Kind == AirShow::ThingKind::Bomb ? (S.bTorpedo ? "torpedo" : "bomb") : "piece")
				      << "," << S.Key << ",0,0,0,," << Num(S.At.X) << "," << Num(S.At.Y) << "," << Num(S.At.Z) << "," << Num(S.Heading)
				      << ",0,0,1," << (S.bOpen ? 1 : 0) << ",0," << Num(S.Size) << "\n";
			for (const auto& G : Show.Groups())
				Track << Num(Show.Shown(), 3) << ",group," << G.first << "," << G.first << "," << G.second.Side << "," << (G.second.bFighter ? 1 : 0) << ","
				      << G.second.Model << "," << Num(G.second.X) << "," << Num(G.second.Y) << "," << Num(G.second.Alt) << ","
				      << Num(G.second.Face * 180.0 / 3.14159265358979) << ",0,0,1,0,0," << G.second.Figures << "\n";
		}
	}
	Track.close();

	const AirShow::Checks& C = Show.Checked();
	std::printf("%s  (%s, %s)  %zu views to %.1f battle s, seed %llu\n", Rec.Key.c_str(), Rec.Board.Title.c_str(), Rec.Board.Date.c_str(),
	            Rec.Views.size(), EndT, static_cast<unsigned long long>(Rec.Seed));
	std::printf("  read %.0f ms; %ld frames at %.0f a second; %.3f ms a frame on average, %.2f ms at worst\n",
	            ReadMs, Frames, Fps, TotalMs / std::max(1L, Frames), WorstFrameMs);
	std::printf("  most aircraft shown at once %d, most small things %d, most guns firing at once %d (budget %d)\n",
	            MostPlanes, MostThings, MostLiveGuns, Show.MostGuns);
	std::printf("  falls %d (battle lost %d), parachutes %d (battle %d), passes %d, breaks under %d / over %d\n",
	            C.Falls, EventsLost, C.Chutes, EventsChutes, C.Passes, C.UnderBreaks, C.OverBreaks);
	std::printf("  gun frames %d, hits %d, flak bursts %d, bombs %d\n", C.GunFrames, C.Hits, C.FlakBursts, C.Bombs);
	std::printf("  debris: %d falls given it, %d left without (%d of them blown apart; room %d puffs, most held at once %d)\n",
	            Debris.Given, Debris.Refused, Debris.RefusedBlasts, Debris.Most, MostDebris);
	std::printf("  trails: %d an aircraft's own, %d pieces'; room 24: most lit %d, refused %d (%d an aircraft's own), pieces put out %d;"
	            " room 10: refused %d (%d an aircraft's own)\n",
	            OwnTrails, PieceTrails, Trails24.Peak, Trails24.Refused, Trails24.RefusedOwn, Trails24.PutOut, Trails10.Refused, Trails10.RefusedOwn);
	{
		// Each group's model as the scene makes it: the first view's, which the feed now fills from the next view to name it.
		std::map<std::string, int> Made;
		for (const AirShow::PlaneOut& P : Show.Planes()) ++Made[std::to_string(P.Group) + ":" + P.Model];
		std::printf("  models still flying at the end:");
		for (const auto& P : Made) std::printf(" %s x%d", P.first.c_str(), P.second);
		std::printf("\n");
	}
	std::printf("  effects:");
	for (const auto& P : FxCount) std::printf(" %s %d", P.first.c_str(), P.second);
	std::printf("\n  sounds:");
	for (const auto& P : Sounds) std::printf(" %s %d", P.first.c_str(), P.second);
	std::printf("\n");

	std::vector<Limit> Limits = {
		{ "no sliding sideways (degrees between nose and path)", C.MostSlip <= 1.0, Num(C.MostSlip, 2), "at most 1" },
		{ "no turn tighter than 210 m", C.LeastRadius >= 205.0, Num(C.LeastRadius), "at least 205" },
		{ "no more than 6 g", C.MostG <= 6.05, Num(C.MostG, 2), "at most 6.05" },
		{ "roll no faster than 160 degrees a second", C.MostRoll <= 161.0, Num(C.MostRoll), "at most 161" },
		{ "fighters in a fight never hover", C.LeastFightSpeed >= 80.0 || C.LeastFightSpeed > 1e8, Num(C.LeastFightSpeed > 1e8 ? 0.0 : C.LeastFightSpeed), "at least 80 m/s (about 290 km/h)" },
		{ "fighters in a fight not too fast", C.MostFightSpeed <= 201.0, Num(C.MostFightSpeed), "at most 200 m/s (a diving pass)" },
		{ "no aircraft pops from place to place", C.MostJump <= 25.0, Num(C.MostJump), "at most 25 m beyond its speed in a frame" },
		{ "never two gunners at one window", C.SharedWindows == 0, std::to_string(C.SharedWindows), "0" },
		{ "a model falls for every aircraft lost", C.Falls >= EventsLost - 2 && C.Falls <= EventsLost + 2, std::to_string(C.Falls), "the battle's count, within 2" },
		{ "the kit told of every fall, its moves only while it falls", FallStarts == C.Falls && FallStrays == 0, std::to_string(FallStarts) + " starts, " + std::to_string(FallStrays) + " stray", "a start a fall, 0 stray" },
		{ "debris within its room", MostDebris <= Debris.Most, std::to_string(MostDebris), "at most the room" },
		{ "every burning aircraft keeps its trail (room 24)", Trails24.RefusedOwn == 0 && Trails24.Peak <= 24, std::to_string(Trails24.RefusedOwn), "0 refused" },
		{ "no group left on the stand-in model", Guessed == 0, std::to_string(Guessed), "0 views" },
		{ "parachutes as the battle says", C.Chutes >= EventsChutes * 8 / 10, std::to_string(C.Chutes), "at least 8 in 10 of the battle's" },
		{ "guns within the effects budget", MostLiveGuns <= Show.MostGuns, std::to_string(MostLiveGuns), "at most MostGuns" },
		{ "flak bursts round the boxes", C.FlakBursts > 0, std::to_string(C.FlakBursts), "some" },
		{ "fighters pass the bombers", C.Passes > 0, std::to_string(C.Passes), "some" },
		{ "a frame fits the budget", TotalMs / std::max(1L, Frames) < 2.0, Num(TotalMs / std::max(1L, Frames), 3), "under 2 ms on average" },
	};
	int Failed = 0;
	for (const Limit& L : Limits)
	{
		std::printf("  %s %-58s %s (%s)\n", L.bOk ? "ok  " : "FAIL", L.Name, L.Got.c_str(), L.Want);
		if (!L.bOk) ++Failed;
	}

	// The screen, as the player sees it: the battle on the screen's clock at x1 (real time) from the start to the end,
	// the words at a few moments, and the after card.
	{
		AirShow::Show Shown(Rec.Seed ? Rec.Seed : 238);
		AirFeed::Apply(Rec.Board, Shown);
		AirDesk::Desk Desk;
		Desk.Begin(Rec.Board, Rec.Views.front().Head.Me);
		AirDesk::Clock Clock;
		size_t Next = 0;
		auto Feed = [&](double Upto)
		{
			while (Next < Rec.Views.size() && Rec.Views[Next].Head.T <= Upto + 1.0 + 1e-9)
			{
				Shown.Take(Rec.Views[Next].Head.T, Rec.Views[Next].Groups, Rec.Views[Next].Events, Rec.Views[Next].Ships);
				Desk.Take(Rec.Views[Next]);
				++Next;
			}
		};
		Feed(0.0);
		double Battle = Rec.Views.front().Head.T, Real = 0.0, MetReal = -1.0;
		const double Frame = 1.0 / 60.0;
		std::string Tag;
		int Printed = 0;
		const double Marks[] = { 5.0, -1.0, 60.0, 180.0, 1e9 };
		AirDesk::Words W;
		while (Battle < EndT - 1e-9 && Real < 3600.0)
		{
			const double Run = Clock.Step(Frame, Desk.Head(), Rec.Board.Scale, Tag);
			Battle = std::min(EndT, Battle + Run);
			Real += Frame;
			Feed(Battle);
			Shown.Advance(Battle);
			Desk.Reach(Shown.Shown());
			for (const AirShow::Fx& E : Shown.Effects)
				if (E.Kind == AirShow::FxKind::Dispatch) Desk.Say(E.What, E.Text, E.Side, E.Key);
			Shown.Effects.clear();
			Desk.Write(Frame, Shown.Groups(), Tag, W);
			if (MetReal < 0.0 && Desk.Head().Contact >= 0.0) MetReal = Real;
			const bool bMark = (Printed == 0 && Real >= Marks[0]) || (Printed == 1 && MetReal >= 0.0 && Real >= MetReal + 1.0)
				|| (Printed == 2 && MetReal >= 0.0 && Real >= MetReal + Marks[2]) || (Printed == 3 && MetReal >= 0.0 && Real >= MetReal + Marks[3]);
			if (bMark)
			{
				++Printed;
				std::printf("\n  screen at %.0f real s (battle %s): [%s]\n    %s against %s; %s; balance %.2f (from %.2f); bars %.2f / %.2f\n    THE AIM: %s / %s / %s\n",
				            Real, W.Clock.c_str(), W.ClockTag.c_str(), W.MySide.c_str(), W.TheirSide.c_str(), W.Chance.c_str(), W.Balance, W.BalanceStart,
				            W.MyBar, W.TheirBar, W.AimTarget.c_str(), W.AimState.c_str(), W.AimLost.c_str());
				if (!W.NearLine.empty()) std::printf("    near line: %s\n", W.NearLine.c_str());
				for (const AirDesk::Tag& Tg : W.Tags)
					std::printf("    tag %s%s: %s, %s%s%s\n", Tg.bMine ? "(yours) " : "", Tg.bFighter ? "fighters" : "bombers", Tg.Name.c_str(), Tg.Count.c_str(),
					            Tg.Doing.empty() ? "" : ", ", Tg.Doing.c_str());
				for (const AirDesk::Dispatch& D : W.Dispatches) std::printf("    dispatch (%.0f s): %s\n", D.Age, D.Text.c_str());
			}
		}
		std::printf("\n  on the screen's clock at x1: the formations meet %.0f real s in; the battle ends %.0f real s in (%.1f real minutes)\n",
		            MetReal, Real, Real / 60.0);
		const AirDesk::Card Card = Desk.CardOf(Rec.Card);
		std::printf("  after card: %s / %s / %s\n", Card.Kicker.c_str(), Card.Title.c_str(), Card.Level.c_str());
		for (const std::string& L : Card.Lines) std::printf("    %s\n", L.c_str());
		for (const AirDesk::Card::Row& R : Card.Rows) std::printf("    %s%s: %s\n", R.bMine ? "(yours) " : "", R.Name.c_str(), R.Words.c_str());
		std::printf("  staff's question: %s\n", Desk.StaffQuestion().c_str());
		if (MetReal < 0.0 || Real >= 3600.0) { std::printf("  FAIL the screen's clock never reached the end\n"); ++Failed; }
	}

	std::printf("%s\n", Failed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failed ? 1 : 0;
}
