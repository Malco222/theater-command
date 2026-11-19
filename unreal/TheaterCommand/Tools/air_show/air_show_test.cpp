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
#include "Battle/AirGround.h"
#include "Battle/AirShow.h"
#include "Battle/AirSky.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <set>
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
	// The day's sky, laid as the scene lays it (the sea under it here: the stage's hills are the engine's).
	const AirSky::Placing SkyAt = AirFeed::PlacingOf(Rec.Board, Rec.Views.front().Groups, Rec.Seed);
	const auto Sky0 = std::chrono::steady_clock::now();
	const AirSky::Field Sky = AirSky::Lay(Rec.Board.Cloud, SkyAt);
	const double SkyLayMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Sky0).count();
	Show.CloudTop = Sky.DeckTop;
	if (Sky.DeckTop > 0.0)
	{
		Show.DeckAt = [&Sky](double X, double Y)
		{
			AirShow::DeckSpan Here;
			Here.Hide = Sky.DeckAt(X, Y, &Here.Foot);
			return Here;
		};
		Show.DeckReach = AirSky::Reach * 5.0 / 6.0; // (as the scene sets it)
	}

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
	double FirstBombAir = -1.0; // (the air clock when the first bombs fall)
	std::vector<AirSky::Vec> FallsAt; // (where each fall began, and on the air clock when, in Z)
	std::vector<double> FallsAir;
	// Falls and parachutes that end in the air (on a cumulus day, none: every one reaches the sea or runs out of time).
	std::map<int, double> FallBegan;
	int FallsEndAloft = 0, ChutesVanished = 0, ChutesDown = 0;
	std::vector<AirShow::Thing> ChutesBefore;
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
			if (E.Kind == AirShow::FxKind::BombHit && FirstBombAir < 0.0) FirstBombAir = Show.AirNow();
			if (E.Kind == AirShow::FxKind::GunStop) GunOn.erase(E.Key);
			if (E.Kind == AirShow::FxKind::Fall)
			{
				++FallStarts;
				FallsAt.push_back(AirSky::Vec(E.At.X, E.At.Y, E.At.Z));
				FallsAir.push_back(Show.AirNow());
				FallBegan[E.Key] = Show.AirNow();
				Debris.Take(Show.AirNow(), E);
				if (E.What != "explode" && E.What != "burn" && E.What != "spin") ++FallStrays;
				if (E.What != "explode") FallsOpen[E.Key] = E.What;
			}
			if (E.Kind == AirShow::FxKind::FallMove && !FallsOpen.count(E.Key)) ++FallStrays;
			if (E.Kind == AirShow::FxKind::FallStop && FallsOpen.erase(E.Key) == 0) ++FallStrays;
			if (E.Kind == AirShow::FxKind::FallStop)
			{
				const auto Began = FallBegan.find(E.Key);
				if (Began != FallBegan.end() && E.At.Z > 2.5 && Show.AirNow() - Began->second < 399.0) ++FallsEndAloft;
				if (Began != FallBegan.end()) FallBegan.erase(Began);
			}
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
		{
			// (each parachute last frame found again this frame, where it should have drifted; one not found has come
			// down if it was at the sea, else it vanished in the air)
			std::vector<AirShow::Thing> ChutesNow;
			for (const AirShow::Thing& Th : Show.Things())
				if (Th.Kind == AirShow::ThingKind::Chute) ChutesNow.push_back(Th);
			for (const AirShow::Thing& Was : ChutesBefore)
			{
				const AirShow::Vec Next = Was.At + Was.Vel * Dt;
				bool bFound = false;
				for (const AirShow::Thing& Now : ChutesNow)
					if ((Now.At - Next).Length() < 6.0 && std::fabs(Now.Age - Was.Age) < 3.0 * Dt + 1e-6) { bFound = true; break; }
				if (bFound) continue;
				if (Was.At.Z <= 2.5 + std::fabs(Was.Vel.Z) * Dt * 2.0) ++ChutesDown;
				else if (Was.Age < 1499.0) ++ChutesVanished;
			}
			ChutesBefore.swap(ChutesNow);
		}

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
	std::printf("  sky: %s%s from %.0f to %.0f m (zone %s, deck chance %.2f), bands %.0f and %.0f m; %zu clouds, %zu puffs, cover %.2f; laid in %.1f ms\n",
	            Rec.Board.Cloud.bGiven ? "" : "(none given) ", Sky.Sky.bDeck ? "a deck" : "scattered cumulus", Sky.Base, Sky.Top, Rec.Board.Cloud.Zone.c_str(),
	            Rec.Board.Cloud.Chance, SkyAt.Band, SkyAt.LowBand, Sky.Clouds.size(), Sky.Puffs.size(), Sky.Cover, SkyLayMs);
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

	// The flyover before the battle (Brian's, approved 19 November): its shots over the battle's first groups as the
	// scene plays them, the battle at x1 under them, a frame every thirtieth of a second.
	int FlyNoLook = 0, FlyLow = 0, FlyJumps = 0, FlyIntoPlane = 0;
	double FlyLeastRaidSeen = 1.0;
	std::set<int> FlyShots;
	bool bFlyBegun = false;
	AirDesk::Flyover Flyover;
	{
		AirShow::Show Fly(Rec.Seed ? Rec.Seed : 238);
		AirFeed::Apply(Rec.Board, Fly);
		size_t FlyTaken = 0;
		auto FlyTake = [&]()
		{
			const AirFeed::Frame& F = Rec.Views[FlyTaken++];
			Fly.Take(F.Head.T, F.Groups, F.Events, F.Ships);
		};
		FlyTake();
		if (FlyTaken < Rec.Views.size()) FlyTake();
		double FlyT = Rec.Views.front().Head.T;
		Fly.Advance(FlyT);
		bFlyBegun = Flyover.Begin(Fly.Groups(), Rec.Board.TargetX, Rec.Board.TargetY);
		const double Step = 1.0 / 30.0;
		AirShow::Vec LastEye;
		int LastIndex = -2;
		for (double RT = 0.0; bFlyBegun && RT <= Flyover.Length() + 0.5; RT += Step)
		{
			FlyT += Step / std::max(Rec.Board.Scale, 0.1);
			while (FlyTaken < Rec.Views.size() && Rec.Views[FlyTaken - 1].Head.T <= FlyT + 1e-9) FlyTake();
			Fly.Advance(FlyT);
			const AirDesk::Flyover::Shot Shot = Flyover.At(RT, Fly.Groups());
			const AirShow::Vec Way = Shot.Look - Shot.Eye;
			if (Way.Length() < 50.0) ++FlyNoLook;
			if (Shot.Eye.Z < 60.0) ++FlyLow;
			if (Shot.Index >= 0) FlyShots.insert(Shot.Index);
			// (within a shot the eye glides: no more than 250 m a second, the formation's speed and the dolly's)
			if (LastIndex == Shot.Index && (Shot.Eye - LastEye).Length() > 250.0 * Step) ++FlyJumps;
			int RaidPlanes = 0, RaidInView = 0;
			for (const AirShow::PlaneOut& P : Fly.Planes())
			{
				if (!P.bShown) continue;
				const AirShow::Vec To = P.At - Shot.Eye;
				if (To.Length() < 40.0) ++FlyIntoPlane;
				if (P.Group == Flyover.RaidGroup())
				{
					++RaidPlanes;
					// (within 30 degrees of the look: a 55 degree lens holds it)
					if (To.Unit().Dot(Way.Unit()) > std::cos(30.0 * 3.14159265358979 / 180.0)) ++RaidInView;
				}
			}
			// (the raid's shot: most of its formation in the picture, from its first second to its last)
			if (Shot.Index == 0 && RT > 1.0 && RaidPlanes > 0) FlyLeastRaidSeen = std::min(FlyLeastRaidSeen, static_cast<double>(RaidInView) / RaidPlanes);
			LastEye = Shot.Eye;
			LastIndex = Shot.Index;
		}
	}
	std::printf("  flyover: raid group %d, guard group %d, %.1f s of shots, %zu shots\n", Flyover.RaidGroup(), Flyover.GuardGroup(), Flyover.Length(), FlyShots.size());

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
	// The sky (AirSky): the day the battle gave, laid inside its heights, scattered, within the painter's room, the same
	// every time, its clearing kept over the target as the wind carries the clouds, and turned to an eye within a
	// frame's budget.
	auto OutOfHeight = [](const AirSky::Field& Fd)
	{
		// (each cloud from the day's base up and no taller than the day; each puff's middle within its cloud's foot and
		// top)
		int Out = 0;
		for (const AirSky::Cloud& Cl : Fd.Clouds)
		{
			if (Cl.At.Z < Fd.Base - 0.5 || Cl.H > Fd.Top - Fd.Base + 0.5) ++Out;
			for (int K = Cl.First; K < Cl.First + Cl.Count; ++K)
			{
				const AirSky::Puff& Pf = Fd.Puffs[static_cast<size_t>(K)];
				if (Pf.At.Z < Cl.At.Z - 0.5 || Pf.At.Z > Cl.At.Z + Cl.H + 0.5) ++Out;
			}
		}
		return Out;
	};
	const bool bCumulus = !Sky.Sky.bDeck;
	{
		const AirSky::Field Again = AirSky::Lay(Rec.Board.Cloud, SkyAt);
		bool bSame = Again.Puffs.size() == Sky.Puffs.size();
		for (size_t K = 0; bSame && K < Sky.Puffs.size(); ++K)
			bSame = Again.Puffs[K].At.X == Sky.Puffs[K].At.X && Again.Puffs[K].At.Y == Sky.Puffs[K].At.Y && Again.Puffs[K].At.Z == Sky.Puffs[K].At.Z && Again.Puffs[K].R == Sky.Puffs[K].R;
		// The wind's carry when the first bombs fall, at the battle's end, and two hours on (the field repeats: any carry
		// leaves it whole).
		const double EndAir = Show.AirNow();
		const AirSky::Vec AtBombs = Sky.DriftAt(FirstBombAir > 0.0 ? FirstBombAir : 0.0), AtEnd = Sky.DriftAt(EndAir), AtLast = Sky.DriftAt(EndAir + 7200.0);
		AirSky::Painter Paint;
		std::vector<AirSky::Vec> Points, Shadows;
		std::vector<uint8_t> Rgba, ShadowRgba;
		double WorstPaintMs = 0.0;
		int Nearer = 0, MostDrawn = 0, MostDropped = 0, LeastNearFall = 1 << 30, MostShadows = 0, DarkAmongPuffs = 0;
		// (as the scene paints: the shadows laid apart, for their own mesh under everything)
		Paint.Paint(Sky, AirSky::Vec(Rec.Board.TargetX, Rec.Board.TargetY, 9000.0), AirSky::Vec(), SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Points, Rgba, &Shadows, &ShadowRgba); // (warm: the room made)
		// (eyes behind the raid, low over the target, near the sea, and behind the battle's farthest falls from the target
		// and its middle one, where the fight is)
		std::vector<AirSky::Vec> Eyes = { AirSky::Vec(Rec.Board.TargetX, Rec.Board.TargetY - 6000.0, 5000.0), AirSky::Vec(Rec.Board.TargetX + 3000.0, Rec.Board.TargetY, 1200.0),
		                                  AirSky::Vec(Rec.Board.TargetX - 9000.0, Rec.Board.TargetY + 4000.0, 300.0) };
		std::vector<size_t> ByFar(FallsAt.size());
		for (size_t K = 0; K < ByFar.size(); ++K) ByFar[K] = K;
		auto FromTarget = [&](size_t K) { return std::hypot(FallsAt[K].X - Rec.Board.TargetX, FallsAt[K].Y - Rec.Board.TargetY); };
		std::sort(ByFar.begin(), ByFar.end(), [&](size_t A, size_t B) { return FromTarget(A) > FromTarget(B); });
		const size_t FallEyes = Eyes.size();
		if (!ByFar.empty())
		{
			for (const size_t K : { ByFar.front(), ByFar[ByFar.size() / 2] })
				Eyes.push_back(AirSky::Vec(FallsAt[K].X, FallsAt[K].Y - 1500.0, FallsAt[K].Z + 300.0));
		}
		const double FarthestFall = ByFar.empty() ? 0.0 : FromTarget(ByFar.front());
		for (size_t EyeIndex = 0; EyeIndex < Eyes.size(); ++EyeIndex)
		{
			const AirSky::Vec& E = Eyes[EyeIndex];
			// (the middle of 15 paints, and the least of three such tries: one paint is a few milliseconds, and one slow
			// stretch of a shared machine inside a 50 ms window is not the painter's; the pre-check, 19 November 16:11Z)
			double LeastMiddle = 1e18;
			for (int Try = 0; Try < 3; ++Try)
			{
				std::vector<double> Took;
				for (int Rep = 0; Rep < 15; ++Rep)
				{
					const auto P0 = std::chrono::steady_clock::now();
					Paint.Paint(Sky, E, AtBombs, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Points, Rgba, &Shadows, &ShadowRgba);
					Took.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - P0).count());
				}
				std::nth_element(Took.begin(), Took.begin() + 7, Took.end());
				LeastMiddle = std::min(LeastMiddle, Took[7]);
			}
			WorstPaintMs = std::max(WorstPaintMs, LeastMiddle);
			MostDrawn = std::max(MostDrawn, Paint.Drawn());
			MostDropped = std::max(MostDropped, Paint.Dropped());
			MostShadows = std::max(MostShadows, Paint.ShadowsDrawn());
			for (size_t K = 0; K < Points.size(); ++K)
			{
				if (Rgba[K * 4 + 3] > 0 && Rgba[K * 4] < 60 && Rgba[K * 4 + 1] < 60 && Rgba[K * 4 + 2] < 60) ++DarkAmongPuffs;
			}
			if (EyeIndex >= FallEyes) LeastNearFall = std::min(LeastNearFall, Paint.Drawn());
			// (laid far to near: each puff's middle no farther from the eye than the one before)
			double Last = 1e18;
			for (size_t K = 0; K < Points.size(); K += static_cast<size_t>(Paint.PerPuff()))
			{
				const AirSky::Vec D = Points[K] - (E - AtBombs);
				const double Far = D.Dot(D);
				// (a cloud the clearing has taken is a point at its middle: it keeps its place in the order)
				if (Far > Last + 1e-6) ++Nearer;
				Last = Far;
			}
		}
		// Under the scene's cloud ink (1.3, AirBattle.cpp), only the sunlit tops rise: painted with the ink, no point is
		// brighter than without it, the brightest are as they were, and the shaded sides and bases are the ink's share
		// darker, so that the ink brings them back where they were (the art director, 16:31Z).
		int InkBrighter = 0, InkTopsMoved = 0, InkShadeOff = 0, InkTops = 0, InkShades = 0;
		{
			AirSky::Painter Plain, Inked;
			Inked.SetInk(1.3);
			std::vector<AirSky::Vec> PlainPoints, InkedPoints;
			std::vector<uint8_t> PlainRgba, InkedRgba;
			const AirSky::Vec E = Eyes.front();
			Plain.Paint(Sky, E, AtBombs, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), PlainPoints, PlainRgba);
			Inked.Paint(Sky, E, AtBombs, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), InkedPoints, InkedRgba);
			int Top = 0; // (the brightest green laid: a sunlit top)
			for (size_t K = 1; K < PlainRgba.size(); K += 4) Top = std::max(Top, static_cast<int>(PlainRgba[K]));
			for (size_t K = 0; K + 3 < PlainRgba.size() && K < InkedRgba.size(); K += 4)
			{
				const int R0 = PlainRgba[K], G0 = PlainRgba[K + 1], R1 = InkedRgba[K], G1 = InkedRgba[K + 1];
				if (R1 > R0 + 1 || G1 > G0 + 1) ++InkBrighter;
				if (G0 >= Top - 1)
				{
					++InkTops;
					if (std::abs(G1 - G0) > 1) ++InkTopsMoved;
				}
				// (a base or a shaded side: no brighter than the shaded side's own paint, 170 of green in a picture,
				// some 102 laid)
				if (G0 > 0 && G0 <= 102)
				{
					++InkShades;
					if (std::abs(G1 * 1.3 - G0) > 2.0) ++InkShadeOff;
				}
			}
		}
		// The clearing over the target (within 1.8 km of it), and the field round it, as first laid, when the first
		// bombs fall, at the battle's end and two hours on.
		double WorstNear = 0.0, LeastField = 1.0, MostField = 0.0;
		for (const AirSky::Vec& D : { AirSky::Vec(), AtBombs, AtEnd, AtLast })
		{
			WorstNear = std::max(WorstNear, Sky.CoverOf(D, 0.0, 1800.0));
			const double Field = Sky.CoverOf(D, 4500.0, 9000.0);
			LeastField = std::min(LeastField, Field);
			MostField = std::max(MostField, Field);
		}
		// The same clearing as the painter draws it (the game's own path: its eased lift, its clearing, its far and near
		// puffs): the cloud drawn within 1.8 km of an eye 2.5 km over the tops, over the target and over 16 points of the
		// field round it (4.5 to 9 km out), as alpha times a puff's area a square kilometre, at each carry.
		auto PaintedOver = [&Sky, &SkyAt](AirSky::Painter& Pt, double X, double Y, const AirSky::Vec& D, std::vector<AirSky::Vec>& Pts, std::vector<uint8_t>& Cols)
		{
			const AirSky::Vec E(X, Y, Sky.Top + 2500.0);
			Pt.Paint(Sky, E, D, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Pts, Cols);
			const size_t Per = static_cast<size_t>(Pt.PerPuff()), Sides = static_cast<size_t>(Pt.Sides());
			double Sum = 0.0;
			for (size_t K = 0; K + Per <= Pts.size(); K += Per)
			{
				const double A = Cols[K * 4 + 3] / 255.0;
				if (A <= 0.0) continue;
				const AirSky::Vec Mid = Pts[K] + D;
				if (Cols[K * 4 + 0] < 60 && Cols[K * 4 + 1] < 60 && Cols[K * 4 + 2] < 60) continue; // (a cloud's shadow: dark, on the sea or the land)
				if (std::hypot(Mid.X - X, Mid.Y - Y) > 1800.0) continue;
				double R2 = 0.0;
				for (size_t J = 0; J < Sides; ++J)
				{
					const AirSky::Vec Rim = Pts[K + 1 + Sides + J] - Pts[K];
					R2 += Rim.Dot(Rim) / static_cast<double>(Sides);
				}
				Sum += A * R2 * 1e-6;
			}
			return Sum / (3.14159265358979 * 1.8 * 1.8);
		};
		double WorstPaintedShare = 0.0, LeastPaintedField = 1e9;
		{
			AirSky::Painter Over;
			std::vector<AirSky::Vec> Pts;
			std::vector<uint8_t> Cols;
			for (const AirSky::Vec& D : { AirSky::Vec(), AtBombs, AtEnd, AtLast })
			{
				const double NearSum = PaintedOver(Over, Rec.Board.TargetX, Rec.Board.TargetY, D, Pts, Cols);
				double FieldSum = 0.0;
				for (int K = 0; K < 16; ++K)
				{
					const double A = 2.0 * 3.14159265358979 * K / 16.0, Out = K % 2 ? 5500.0 : 8000.0;
					FieldSum += PaintedOver(Over, Rec.Board.TargetX + Out * std::cos(A), Rec.Board.TargetY + Out * std::sin(A), D, Pts, Cols) / 16.0;
				}
				LeastPaintedField = std::min(LeastPaintedField, FieldSum);
				WorstPaintedShare = std::max(WorstPaintedShare, FieldSum > 0.0 ? NearSum / FieldSum : 1.0);
			}
		}
		// Cloud where the fight is: the share of the sky under cloud within 4 km of each fall more than 9 km from the
		// target, where the wind had carried the field when it fell.
		auto CoverNear = [&Sky](double CX, double CY, const AirSky::Vec& D)
		{
			int Spots = 0, Under = 0;
			for (int GX = 0; GX < 24; ++GX)
				for (int GY = 0; GY < 24; ++GY)
				{
					const double LX = CX - 4000.0 + (GX + 0.5) * 8000.0 / 24 - D.X, LY = CY - 4000.0 + (GY + 0.5) * 8000.0 / 24 - D.Y;
					++Spots;
					for (size_t K = 0; K < Sky.Clouds.size(); ++K)
					{
						const AirSky::Cloud& Cl = Sky.Clouds[K];
						const AirSky::Vec Sh = Sky.ShiftNear(static_cast<int>(K), LX, LY);
						const double Ex = Cl.At.X + Sh.X - LX, Ey = Cl.At.Y + Sh.Y - LY, R = Cl.W * 0.45;
						if (Ex * Ex + Ey * Ey <= R * R && Sky.Place(static_cast<int>(K), LX, LY, D).Show >= 0.5)
						{
							++Under;
							break;
						}
					}
				}
			return Spots ? static_cast<double>(Under) / Spots : 0.0;
		};
		double LeastFar = 1.0, SumFar = 0.0;
		int FarFalls = 0;
		for (size_t K = 0; K < FallsAt.size(); ++K)
		{
			if (std::hypot(FallsAt[K].X - Rec.Board.TargetX, FallsAt[K].Y - Rec.Board.TargetY) < 9000.0) continue;
			const double Here = CoverNear(FallsAt[K].X, FallsAt[K].Y, Sky.DriftAt(FallsAir[K]));
			LeastFar = std::min(LeastFar, Here);
			SumFar += Here;
			++FarFalls;
		}
		// A thin cumulus day (a 400 m deep layer, inside the Hawaii zone's own ranges).
		AirSky::Day Thin;
		Thin.bGiven = true;
		Thin.Base = 1100.0;
		Thin.Top = 1500.0;
		const AirSky::Field Shallow = AirSky::Lay(Thin, SkyAt);
		const int Outside = OutOfHeight(Sky) + (bCumulus ? OutOfHeight(Shallow) : 0);
		Limits.push_back({ "the day's sky read from the battle", Rec.Board.Cloud.bGiven && Rec.Board.Cloud.Top >= Rec.Board.Cloud.Base + 299.0,
		                   std::string(Rec.Board.Cloud.bDeck ? "deck " : "cumulus ") + Num(Rec.Board.Cloud.Base, 0) + " to " + Num(Rec.Board.Cloud.Top, 0) + " m", "given, tops 300 m over the base" });
		Limits.push_back({ "cloud inside the day's base and tops (a thin day too)", Outside == 0 && !Sky.Clouds.empty() && !Shallow.Clouds.empty(), std::to_string(Outside) + " outside", "0" });
		Limits.push_back({ bCumulus ? "cumulus scattered, about 3/8 (share under cloud)" : "a deck covers the sky (share under cloud)",
		                   bCumulus ? Sky.Cover >= 0.31 && Sky.Cover <= 0.45 : Sky.Cover >= 0.625, Num(Sky.Cover, 2), bCumulus ? "0.31 to 0.45" : "5/8 or more" });
		if (bCumulus)
		{
			Limits.push_back({ "the target's clearing kept as the clouds drift", WorstNear <= 0.6 * LeastField && LeastField >= 0.29 && MostField <= 0.47,
			                   "near " + Num(WorstNear, 2) + ", field " + Num(LeastField, 2) + " to " + Num(MostField, 2) + " (drift " + Num(AtBombs.Length(), 0) + " m at the bombs, " + Num(AtLast.Length(), 0) + " at most)",
			                   "near at most 6/10 of the field, the field whole at every carry" });
			Limits.push_back({ "the clearing as the painter draws it", WorstPaintedShare <= 0.6 && LeastPaintedField > 0.0,
			                   "near " + Num(WorstPaintedShare, 2) + " of the field at worst (field " + Num(LeastPaintedField, 3) + " at least)", "at most 6/10 of the field at every carry" });
		}
		Limits.push_back({ "clouds where the fight is, far from the target too", FarFalls == 0 || (LeastFar >= (bCumulus ? 0.12 : 0.45) && SumFar / FarFalls >= (bCumulus ? 0.25 : 0.6)),
		                   std::to_string(FarFalls) + " falls past 9 km (farthest " + Num(FarthestFall / 1000.0, 1) + " km): cover " + Num(LeastFar, 2) + " at least, " + Num(FarFalls ? SumFar / FarFalls : 0.0, 2) + " on average",
		                   bCumulus ? "at least 0.12, 0.25 on average" : "at least 0.45, 0.6 on average" });
		Limits.push_back({ "the sky within the painter's room", MostDropped == 0 && LeastNearFall >= 300,
		                   std::to_string(MostDrawn) + " puffs at most of " + std::to_string(Sky.MostPuffs) + ", " + std::to_string(MostDropped) + " left out; " + std::to_string(LeastNearFall == (1 << 30) ? 0 : LeastNearFall) + " drawn round the falls' eyes at least",
		                   "none left out, 300 or more round the fight" });
		Limits.push_back({ "the same sky every time", bSame, bSame ? "same" : "different", "same" });
		Limits.push_back({ "the sky laid far to near for each eye", Nearer == 0, std::to_string(Nearer) + " out of order", "0" });
		// (5 ms on this machine: the art director's light, each puff's own lump and its soft edges away from the sun, and the
		// shadows laid apart made a paint about 1.8 times what it was when the mark was 4, some 3 ms here, up to 4.2 under
		// clang on a busy machine; the mark is there to catch the next such step, and the Mac's cores are faster)
		Limits.push_back({ "the sky turned to an eye within budget", WorstPaintMs < 5.0, Num(WorstPaintMs, 2) + " ms", "under 5 ms (the least of three middles of 15)" });
		// (the shadows in their own room, none among the puffs: their mesh is drawn under the ground's fires and smoke)
		Limits.push_back({ "the flyover shows the raid and what it comes for", bFlyBegun && FlyShots.count(0) && FlyShots.count(1) && FlyLeastRaidSeen >= 0.6 && Flyover.Length() <= 14.0,
		                   std::to_string(FlyShots.size()) + " shots, " + Num(Flyover.Length()) + " s, at least " + Num(FlyLeastRaidSeen * 100.0, 0) + "% of the raid in view",
		                   "the raid and the target shown, 60% of the raid in view, 14 s or less" });
		Limits.push_back({ "the flyover's eye glides clear", FlyNoLook == 0 && FlyLow == 0 && FlyJumps == 0 && FlyIntoPlane == 0,
		                   std::to_string(FlyJumps) + " jumps, " + std::to_string(FlyLow) + " low, " + std::to_string(FlyIntoPlane) + " in a plane, " + std::to_string(FlyNoLook) + " looks lost",
		                   "0 of each" });
		// The stage's ground as the scene paints it (AirGround; the art director on build 1734): the grass varied and
		// patched with red earth, the beach a band along the water, the shallows darkening into the deep (not where a
		// lagoon is), the runways clear of the land; every triangle's points there.
		{
			const std::string Base = Path.substr(0, Path.rfind("/AirBattles/") == std::string::npos ? 0 : Path.rfind("/AirBattles/"));
			const std::string GroundPath = Base + "/Stages/" + Rec.Board.StageKey + ".json";
			std::ifstream GroundIn(GroundPath);
			std::stringstream GroundText;
			GroundText << GroundIn.rdbuf();
			AirFeed::Json GroundJson;
			std::string GroundWhy;
			const bool bGround = !Rec.Board.StageKey.empty() && GroundIn && AirFeed::Parse(GroundText.str(), GroundJson, GroundWhy);
			std::vector<std::string> PaintNames;
			std::vector<AirGround::Rgb8> PaintColours;
			const AirFeed::Json& PaletteJson = GroundJson["palette"];
			for (size_t K = 0; bGround && K < PaletteJson.Count(); ++K)
			{
				PaintNames.push_back(PaletteJson.At(K)["name"].Str(""));
				const unsigned long Packed = std::strtoul(PaletteJson.At(K)["hex"].Str("#808080").c_str() + 1, nullptr, 16);
				PaintColours.push_back({ static_cast<uint8_t>((Packed >> 16) & 255u), static_cast<uint8_t>((Packed >> 8) & 255u), static_cast<uint8_t>(Packed & 255u) });
			}
			std::vector<AirGround::Part> GroundParts;
			const AirFeed::Json& MeshesJson = GroundJson["meshes"];
			for (size_t K = 0; bGround && K < MeshesJson.Count(); ++K)
			{
				const AirFeed::Json& M = MeshesJson.At(K);
				AirGround::Part Part;
				Part.Name = M["name"].Str("");
				for (size_t I = 0; I + 2 < M["v"].Count(); I += 3)
				{
					Part.X.push_back(M["v"].At(I).Num() * 0.1);
					Part.Y.push_back(M["v"].At(I + 1).Num() * 0.1);
					Part.Z.push_back(M["v"].At(I + 2).Num() * 0.1);
				}
				for (size_t I = 0; I < M["c"].Count(); ++I) Part.Paints.push_back(M["c"].At(I).Int());
				for (size_t I = 0; I < M["t"].Count(); ++I) Part.Tris.push_back(M["t"].At(I).Int());
				GroundParts.push_back(std::move(Part));
			}
			AirGround::Tally GroundTally;
			const auto GroundStart = std::chrono::steady_clock::now();
			AirGround::Focus GroundNear;
			GroundNear.X = Rec.Board.StageAimX;
			GroundNear.Y = Rec.Board.StageAimY;
			GroundNear.Radius = 4000.0; // (as AAirStage::BuildGround asks)
			const std::vector<AirGround::Drawn> GroundDrawn = AirGround::Paint(GroundParts, PaintNames, PaintColours, { 0x1C, 0x44, 0x53 }, &GroundTally, GroundNear);
			const double GroundMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - GroundStart).count();
			const int CanePaint = static_cast<int>(std::find(PaintNames.begin(), PaintNames.end(), std::string("cane")) - PaintNames.begin());
			int BadIndex = 0, GrewWrong = 0;
			size_t TrisBefore = 0, TrisAfter = 0;
			double SumL = 0.0, SumLL = 0.0;
			int Grass = 0;
			for (size_t K = 0; K < GroundDrawn.size() && K < GroundParts.size(); ++K)
			{
				const AirGround::Drawn& D = GroundDrawn[K];
				for (const int I : D.Tris) BadIndex += I < 0 || static_cast<size_t>(I) >= D.X.size() || D.Colours.size() != D.X.size();
				// (the land's and the shallows' rims as long as they were: a point laid on an edge is shared by both its
				// triangles, so no crack opens; the rest as they were)
				const bool bSplit = D.Name == "land" || D.Name == "shallows";
				const double RimWas = bSplit ? AirGround::RimLength(GroundParts[K].X, GroundParts[K].Y, GroundParts[K].Tris) : 0.0;
				GrewWrong += bSplit ? std::fabs(AirGround::RimLength(D.X, D.Y, D.Tris) - RimWas) > 0.5 + 1e-6 * RimWas || D.Tris.size() < GroundParts[K].Tris.size()
				                    : D.Tris.size() != GroundParts[K].Tris.size();
				TrisBefore += GroundParts[K].Tris.size() / 3;
				TrisAfter += D.Tris.size() / 3;
				if (D.Name != "land") continue;
				for (size_t I = 0; I < GroundParts[K].Paints.size(); ++I)
				{
					// (the grass within 6 km of the stage's aim: how much its brightness varies)
					if (GroundParts[K].Paints[I] == CanePaint && std::hypot(D.X[I] - Rec.Board.StageAimX, D.Y[I] - Rec.Board.StageAimY) < 6000.0)
					{
						const double L = AirGround::Luma(D.Colours[I]);
						SumL += L;
						SumLL += L * L;
						++Grass;
					}
				}
			}
			const double GrassSpread = Grass > 1 ? std::sqrt(std::max(0.0, SumLL / Grass - (SumL / Grass) * (SumL / Grass))) : 0.0;
			// The yard (the historian's 1941 blocks, tools/stages/yard.py): each row whole; each ship in a dock inside its
			// basin and along it, Shaw inside YFD-2's walls; no ship's hull on a shop's pad, the apron, a quay or a dock
			// not hers (the historian's berths of 19 November: every hull clear); the land under each block between its
			// skirt's foot and its top (the cloud pre-check on round 6: pads lay under the land); the paved tops worn.
			int YardRows = 0, YardBad = 0, DockedIn = 0, DockedOut = 0, ShipUnder = 0, LandOver = 0, LandUnder = 0, HullAground = 0, HullOnPier = 0;
			std::string Aground;
			const AirFeed::Json& YardJson = GroundJson["yard"];
			struct YardBlock { double X, Y, Ground, Long, Wide, Heading; int Kind; };
			std::vector<YardBlock> Blocks;
			for (size_t K = 0; bGround && K < YardJson.Count(); ++K)
			{
				const AirFeed::Json& Row = YardJson.At(K);
				const int Kind = Row.Count() >= 8 ? Row.At(7).Int() : -1;
				YardBad += Row.Count() != 8 || Kind < 0 || Kind > 4 || Row.At(3).Num() <= 0.0 || Row.At(4).Num() <= 0.0;
				if (Row.Count() >= 8) Blocks.push_back({ Row.At(0).Num(), Row.At(1).Num(), Row.At(2).Num(), Row.At(3).Num(), Row.At(4).Num(), Row.At(6).Num(), Kind });
				++YardRows;
			}
			// (a point's place along a block's length and across it, metres from its middle)
			auto InBlock = [](const YardBlock& Block, double Px, double Py, double Pad, double& Along, double& Across)
			{
				const double H = Block.Heading * 3.14159265358979 / 180.0;
				Along = (Px - Block.X) * std::sin(H) + (Py - Block.Y) * std::cos(H);
				Across = (Px - Block.X) * std::cos(H) - (Py - Block.Y) * std::sin(H);
				return std::fabs(Along) <= Block.Long * 0.5 + Pad && std::fabs(Across) <= Block.Wide * 0.5 + Pad;
			};
			{
				// (the record's ships are on the board: turned back to the stage as AAirStage::BoardToStage does)
				const double Ct = std::cos(Rec.Board.StageTurn), St = std::sin(Rec.Board.StageTurn);
				for (const AirFeed::ShipRow& Row : Rec.Views.front().ShipRows)
				{
					const double Dx = Row.X - Rec.Board.TargetX, Dy = Row.Y - Rec.Board.TargetY;
					const double Sx = Rec.Board.StageAimX + Dx * Ct + Dy * St, Sy = Rec.Board.StageAimY - Dx * St + Dy * Ct;
					const double Heading = Row.Face + Rec.Board.StageTurn; // radians, the compass's way
					const double ShipLong = Row.Class == "bb" ? 190.0 : Row.Class == "dd" ? 106.0 : Row.Class == "ss" ? 95.0 : Row.Class == "cv" ? 250.0 : 180.0;
					const double ShipBeam = Row.Class == "bb" ? 32.0 : Row.Class == "dd" ? 10.0 : Row.Class == "ss" ? 8.0 : Row.Class == "cv" ? 30.0 : 19.0;
					const bool bDocked = Row.bDock || Row.Berth == "YFD-2";
					// (her hull's outline every 5 m and its middle line)
					std::vector<std::pair<double, double>> Hull;
					for (double A = -0.5; A <= 0.5 + 1e-9; A += 5.0 / ShipLong)
					{
						for (const double B : { -0.5, 0.0, 0.5 })
						{
							Hull.push_back({ Sx + std::sin(Heading) * ShipLong * A + std::cos(Heading) * ShipBeam * B, Sy + std::cos(Heading) * ShipLong * A - std::sin(Heading) * ShipBeam * B });
						}
					}
					bool bInside = false, bUnder = false;
					for (const YardBlock& Block : Blocks)
					{
						double Along = 0.0, Across = 0.0;
						const double Skew = std::fabs(std::sin(Heading - Block.Heading * 3.14159265358979 / 180.0)); // (0 along the block, either way)
						if (bDocked && (Block.Kind == 1 || Block.Kind == 3))
						{
							InBlock(Block, Sx, Sy, 0.0, Along, Across);
							const double BasinLong = Block.Kind == 1 ? Block.Long - 20.0 : Block.Long, BasinWide = Block.Kind == 1 ? Block.Wide - 15.0 : Block.Wide - 6.0;
							bInside = bInside || (std::fabs(Along) + ShipLong * 0.5 <= BasinLong * 0.5 + 1.0 && std::fabs(Across) <= BasinWide * 0.5 && Skew < std::sin(5.0 * 3.14159265358979 / 180.0));
							continue;
						}
						const double Pad = Block.Kind == 0 ? 12.0 : 0.0;
						for (const auto& [Px, Py] : Hull) bUnder = bUnder || InBlock(Block, Px, Py, Pad, Along, Across);
					}
					if (!bDocked)
					{
						// (and none of it on the land or a pier's top, as the stage file lays them: a docked ship lies on land)
						double MinX = Hull.front().first, MaxX = MinX, MinY = Hull.front().second, MaxY = MinY;
						for (const auto& [Px, Py] : Hull)
						{
							MinX = std::min(MinX, Px), MaxX = std::max(MaxX, Px), MinY = std::min(MinY, Py), MaxY = std::max(MaxY, Py);
						}
						for (const AirGround::Part& Mesh : GroundParts)
						{
							if (Mesh.Name != "land" && Mesh.Name != "works") continue;
							bool bOn = false;
							for (size_t Corner = 0; Corner + 2 < Mesh.Tris.size() && !bOn; Corner += 3)
							{
								const size_t I0 = static_cast<size_t>(Mesh.Tris[Corner]), I1 = static_cast<size_t>(Mesh.Tris[Corner + 1]), I2 = static_cast<size_t>(Mesh.Tris[Corner + 2]);
								if (I0 >= Mesh.X.size() || I1 >= Mesh.X.size() || I2 >= Mesh.X.size()) continue;
								const double Ax = Mesh.X[I0], Ay = Mesh.Y[I0], Bx = Mesh.X[I1], By = Mesh.Y[I1], Cx = Mesh.X[I2], Cy = Mesh.Y[I2];
								if (std::max({ Ax, Bx, Cx }) < MinX || std::min({ Ax, Bx, Cx }) > MaxX || std::max({ Ay, By, Cy }) < MinY || std::min({ Ay, By, Cy }) > MaxY) continue;
								const double Den = (By - Cy) * (Ax - Cx) + (Cx - Bx) * (Ay - Cy);
								if (std::fabs(Den) < 1e-9) continue; // (a pier's side, upright)
								for (const auto& [Px, Py] : Hull)
								{
									const double L0 = ((By - Cy) * (Px - Cx) + (Cx - Bx) * (Py - Cy)) / Den, L1 = ((Cy - Ay) * (Px - Cx) + (Ax - Cx) * (Py - Cy)) / Den;
									if (L0 > 1e-6 && L1 > 1e-6 && 1.0 - L0 - L1 > 1e-6)
									{
										bOn = true;
										break;
									}
								}
							}
							if (bOn)
							{
								(Mesh.Name == "land" ? HullAground : HullOnPier) += 1;
								Aground += (Aground.empty() ? "" : ", ") + (Row.Name.empty() ? Row.Class : Row.Name) + (Row.Berth.empty() ? " " + std::to_string(Row.Id) + " at " + Num(Sx, 0) + " " + Num(Sy, 0) : " at " + Row.Berth) + (Mesh.Name == "land" ? " on land" : " on a pier");
							}
						}
					}
					ShipUnder += bUnder;
					if (bDocked)
					{
						(bInside ? DockedIn : DockedOut) += 1;
					}
				}
			}
			// (the land's points under each block's top, a shop's with its pad: none above its top, none below its skirt)
			for (const AirGround::Drawn& D : GroundDrawn)
			{
				if (D.Name != "land") continue;
				for (const YardBlock& Block : Blocks)
				{
					if (Block.Kind > 2) continue;
					const double Pad = Block.Kind == 0 ? 12.0 : 0.0;
					const double Top = Block.Kind == 1 ? 0.2 : 0.25, Foot = -4.5; // (AAirStage::BuildWorks's floor, paving and skirt)
					for (size_t I = 0; I < D.X.size(); ++I)
					{
						double Along = 0.0, Across = 0.0;
						if (!InBlock(Block, D.X[I], D.Y[I], Pad, Along, Across)) continue;
						LandOver += D.Z[I] > Block.Ground + Top;
						LandUnder += D.Z[I] < Block.Ground + Foot;
					}
				}
			}
			// (the pads' and the apron's paving as AAirStage::BuildWorks lays it, in the stage's concrete)
			AirGround::Drawn Paved;
			Paved.Name = "paving";
			int PanelCount = 0;
			double PanelSum = 0.0, PanelSquares = 0.0, PanelLow = 255.0, PanelHigh = 0.0;
			{
				const auto Found = std::find(PaintNames.begin(), PaintNames.end(), std::string("concrete"));
				const AirGround::Rgb8 Concrete = Found == PaintNames.end() ? AirGround::Rgb8{ 0x8F, 0x8B, 0x83 } : PaintColours[static_cast<size_t>(Found - PaintNames.begin())];
				for (const YardBlock& Block : Blocks)
				{
					if (Block.Kind != 0 && Block.Kind != 2) continue;
					const double Pad = Block.Kind == 0 ? 24.0 : 0.0;
					const double H = Block.Heading * 3.14159265358979 / 180.0;
					for (const AirGround::Panel& Square : AirGround::Paving(Block.X, Block.Y, Block.Long + Pad, Block.Wide + Pad, Block.Heading, Concrete))
					{
						const double L = AirGround::Luma(Square.Colour);
						PanelSum += L;
						PanelSquares += L * L;
						PanelLow = std::min(PanelLow, L);
						PanelHigh = std::max(PanelHigh, L);
						++PanelCount;
						const int First = static_cast<int>(Paved.X.size());
						for (const auto& [A, B] : { std::pair<double, double>{ -0.5, -0.5 }, { 0.5, -0.5 }, { 0.5, 0.5 }, { -0.5, 0.5 } })
						{
							Paved.X.push_back(Square.X + std::sin(H) * Square.Long * A + std::cos(H) * Square.Wide * B);
							Paved.Y.push_back(Square.Y + std::cos(H) * Square.Long * A - std::sin(H) * Square.Wide * B);
							Paved.Z.push_back(Block.Ground + 0.3);
							Paved.Colours.push_back(Square.Colour);
						}
						for (const int I : { 0, 1, 2, 0, 2, 3 }) Paved.Tris.push_back(First + I);
					}
				}
			}
			const double PanelMean = PanelCount ? PanelSum / PanelCount : 0.0;
			const double PanelSpread = PanelCount > 1 ? std::sqrt(std::max(0.0, PanelSquares / PanelCount - PanelMean * PanelMean)) : 0.0;
			const bool bOahu = Rec.Board.StageKey == "oahu";
			std::printf("  yard (%s): %d blocks, %d malformed, %d docked ships inside their docks and %d outside, %d hulls on a block, %d on land and %d on a pier%s%s%s; land points %d over a block's top, %d under its skirt; %d paving slabs, luma %.1f (spread %.1f, %.0f..%.0f)\n",
			            Rec.Board.StageKey.c_str(), YardRows, YardBad, DockedIn, DockedOut, ShipUnder, HullAground, HullOnPier, Aground.empty() ? "" : " (", Aground.c_str(), Aground.empty() ? "" : ")", LandOver, LandUnder, PanelCount, PanelMean, PanelSpread, PanelLow, PanelHigh);
			Limits.push_back({ "every hull in the water, clear of the land and the piers", bGround && HullAground == 0 && HullOnPier == 0,
			                   std::to_string(HullAground) + " on land, " + std::to_string(HullOnPier) + " on a pier" + (Aground.empty() ? std::string() : ": " + Aground),
			                   "0 of each (the historian's berths of 19 November; the docked ships aside)" });
			Limits.push_back({ "the yard's blocks whole, the docked ships in their docks", bGround && YardBad == 0 && DockedOut == 0 && ShipUnder == 0 && LandOver == 0 && LandUnder == 0 && (!bOahu || (YardRows >= 10 && DockedIn == 4)),
			                   std::to_string(YardRows) + " blocks, " + std::to_string(YardBad) + " malformed, " + std::to_string(DockedIn) + " docked in, " + std::to_string(DockedOut) + " out, " + std::to_string(ShipUnder) + " hulls on a block, " + std::to_string(LandOver) + " land points over a top, " + std::to_string(LandUnder) + " under a skirt",
			                   "at Oahu Pennsylvania, Cassin and Downes in Dry Dock No. 1 along its 211 degrees and Shaw in YFD-2; no hull on a block; the land under each block between its skirt and its top" });
			Limits.push_back({ "the yard's paving worn, about 140 luma", !bOahu || (PanelCount > 0 && PanelMean >= 134.0 && PanelMean <= 146.0 && PanelSpread >= 3.0 && PanelSpread <= 9.0),
			                   std::to_string(PanelCount) + " slabs, luma " + Num(PanelMean) + ", spread " + Num(PanelSpread),
			                   "at Oahu luma 134 to 146, spread 3 to 9 (the art director: about 140, stains and patching of 6 to 10)" });
			if (std::getenv("AIR_GROUND_DUMP") && bGround)
			{
				// (the painted ground for Tools/air_show/ground_picture.py: a top-down picture of the stage as painted)
				std::ofstream Dump(OutDir + "/ground-" + Rec.Board.StageKey + ".bin", std::ios::binary);
				auto Word = [&Dump](uint32_t W) { Dump.write(reinterpret_cast<const char*>(&W), 4); };
				Word(static_cast<uint32_t>(GroundDrawn.size() + 1));
				auto Part = [&Dump, &Word](const AirGround::Drawn& D)
				{
					Word(static_cast<uint32_t>(D.Name.size()));
					Dump.write(D.Name.data(), static_cast<std::streamsize>(D.Name.size()));
					Word(static_cast<uint32_t>(D.X.size()));
					for (size_t I = 0; I < D.X.size(); ++I)
					{
						const float Place[3] = { static_cast<float>(D.X[I]), static_cast<float>(D.Y[I]), static_cast<float>(D.Z[I]) };
						Dump.write(reinterpret_cast<const char*>(Place), sizeof(Place));
						const uint8_t Rgb[4] = { D.Colours[I].R, D.Colours[I].G, D.Colours[I].B, 255 };
						Dump.write(reinterpret_cast<const char*>(Rgb), 4);
					}
					Word(static_cast<uint32_t>(D.Tris.size()));
					Dump.write(reinterpret_cast<const char*>(D.Tris.data()), static_cast<std::streamsize>(D.Tris.size() * sizeof(int)));
				};
				for (const AirGround::Drawn& D : GroundDrawn) Part(D);
				Part(Paved);
			}
			std::printf("  ground (%s): %d points laid near the battle (%zu triangles, %zu before), %d sand splits, shallows %d faded and %d kept, %d sheltered shore points, %d shallows points by a wall, %d earth, %d concrete worn, %d runway points lifted, grass spread %.1f luma over %d points, %.0f ms%s\n",
			            Rec.Board.StageKey.c_str(), GroundTally.Refined, TrisAfter, TrisBefore, GroundTally.SandSplit, GroundTally.ShoreFaded, GroundTally.ShoreKept, GroundTally.Sheltered, GroundTally.Walled, GroundTally.Earth, GroundTally.Worn,
			            GroundTally.RunwayRaised, GrassSpread, Grass, GroundMs, GroundTally.bLagoon ? ", a lagoon: the shallows keep their edge" : "");
			const bool bShore = GroundTally.bLagoon ? GroundTally.ShoreFaded == 0 : GroundTally.ShoreFaded > 0 && GroundTally.ShoreKept > 0 && GroundTally.ShoreFaded * 4 >= GroundTally.ShoreKept;
			Limits.push_back({ "the stage's ground painted as the art director asked", bGround && BadIndex == 0 && GrewWrong == 0 && bShore && GroundTally.SandSplit > 0 && GroundTally.RunwayRaised > 0 && TrisAfter <= TrisBefore + 90000 &&
			                   (!bOahu || (GroundTally.Earth > 0 && GrassSpread >= 5.0 && GroundTally.Refined > 0 && GroundTally.Sheltered > 0 && GroundTally.Worn > 0 && GroundTally.Walled > 0)),
			                   bGround ? std::to_string(GroundTally.SandSplit) + " sand splits, " + std::to_string(GroundTally.ShoreFaded) + " shallows points faded, " + std::to_string(GroundTally.Earth) + " earth, grass spread " + Num(GrassSpread) + ", " + std::to_string(GroundTally.Refined) + " laid near, " + std::to_string(TrisAfter - TrisBefore) + " triangles more, " + std::to_string(BadIndex) + " bad points, " + std::to_string(GrewWrong) + " cracked or lost"
			                           : "no stage at " + GroundPath,
			                   "sand split, shallows faded (kept at a lagoon), runways lifted; at Oahu earth patches and the grass varied by 5 luma or more, the ground split near the harbour, sheltered shores plain, concrete worn; no cracks; 90,000 triangles more at most" });
		}
		Limits.push_back({ "the cloud ink lifts only the sunlit tops", InkBrighter == 0 && InkTopsMoved == 0 && InkShadeOff == 0 && InkTops > 0 && InkShades > 0,
		                   std::to_string(InkBrighter) + " brighter, " + std::to_string(InkTopsMoved) + " of " + std::to_string(InkTops) + " tops moved, " + std::to_string(InkShadeOff) + " of " + std::to_string(InkShades) + " shades off",
		                   "0 brighter, tops as they were, shades the ink's share darker" });
		Limits.push_back({ "the clouds' shadows laid apart from the puffs", DarkAmongPuffs == 0 && (!bCumulus || Rec.Board.bNight || MostShadows > 0),
		                   std::to_string(MostShadows) + " shadows at most, " + std::to_string(DarkAmongPuffs) + " dark points among the puffs",
		                   "none among the puffs; some on a cumulus day" });
		Limits.push_back({ "falls and parachutes end on the sea on a cumulus day", !bCumulus || (Show.CloudTop == 0.0 && FallsEndAloft == 0 && ChutesVanished == 0),
		                   std::to_string(FallsEndAloft) + " falls and " + std::to_string(ChutesVanished) + " parachutes end in the air (" + std::to_string(ChutesDown) + " parachutes down)",
		                   "0 and 0, with no deck top" });
	}

	// The sky's other days, laid round this battle's target with its own bands: a deck under the bombers (moved to 1.5
	// to 2.5 km under the lowest above 2,000 m, its thickness kept; over the lowest band where that is the band, and
	// between the two where both fly), a strike at ships with its hole, high ground, a ticket with no sky; and this
	// battle flown again under a deck, where falls begun above it end in it where a tile of it is drawn, and go on down
	// through its holes, past its reach, and from under it.
	{
		AirSky::Day Deck;
		Deck.bGiven = true;
		Deck.bDeck = true;
		Deck.Base = 2600.0;
		Deck.Top = 3600.0;
		const double Band = SkyAt.Band;
		const AirSky::Field Under = AirSky::Lay(Deck, SkyAt);
		Limits.push_back({ "a deck 1.5 to 2.5 km under the lowest bombers", Band > 2000.0 && Under.Top <= Band - 1500.0 + 0.5 && Under.Top >= Band - 2500.0 - 0.5 && std::fabs((Under.Top - Under.Base) - 1000.0) < 0.5 && Under.DeckTop == Under.Top,
		                   "band " + Num(Band, 0) + ", top " + Num(Under.Top, 0) + " m (moved " + Num(Under.Moved, 0) + "), " + Num(Under.Top - Under.Base, 0) + " m thick", "top 1,500 to 2,500 m under the band, 1,000 m thick" });
		Limits.push_back({ "a deck covers the sky (share under cloud)", Under.Cover >= 0.625, Num(Under.Cover, 2), "5/8 or more" });
		AirSky::Day Deep = Deck;
		Deep.Base = 600.0;
		Deep.Top = 1200.0;
		const AirSky::Field Raised = AirSky::Lay(Deep, SkyAt);
		Limits.push_back({ "a deck far under the bombers raised to them", Raised.Top >= Band - 2500.0 - 0.5 && Raised.Top <= Band - 1500.0 + 0.5 && std::fabs((Raised.Top - Raised.Base) - 600.0) < 0.5,
		                   "top " + Num(Raised.Top, 0) + " m (" + Raised.MovedWhy + "), " + Num(Raised.Top - Raised.Base, 0) + " m thick", "top 1,500 to 2,500 m under the band, 600 m thick" });
		AirSky::Day Low = Deck;
		Low.Base = 900.0;
		Low.Top = 1500.0;
		AirSky::Placing Lowest = SkyAt;
		Lowest.Band = 0.0;
		Lowest.LowBand = 1800.0;
		const AirSky::Field Over = AirSky::Lay(Low, Lowest);
		AirSky::Placing Mixed = SkyAt;
		Mixed.Band = 4500.0;
		Mixed.LowBand = 1800.0;
		const AirSky::Field Between = AirSky::Lay(Low, Mixed);
		Limits.push_back({ "the lowest band flies under its deck", Over.Base >= 1800.0 + 600.0 - 0.5 && Between.Base >= 2400.0 - 0.5 && Between.Top <= 3000.0 + 0.5 && Between.Top - Between.Base >= 299.5,
		                   "base " + Num(Over.Base, 0) + " m (" + Over.MovedWhy + "); with 4,500 m over it " + Num(Between.Base, 0) + " to " + Num(Between.Top, 0) + " m",
		                   "base at least 2,400 m; between, 2,400 to 3,000 m" });
		AirSky::Placing Ships = SkyAt;
		Ships.bShips = true;
		const AirSky::Field Hole = AirSky::Lay(Deck, Ships);
		// (the nearest puff drawn there with any body: a tenth of its alpha or more)
		double Nearest = 1e9;
		for (size_t K = 0; K < Hole.Clouds.size(); ++K)
		{
			const AirSky::Cloud& Cl = Hole.Clouds[K];
			const AirSky::Placed Pl = Hole.Place(static_cast<int>(K), Ships.TargetX, Ships.TargetY, AirSky::Vec());
			for (int J = Cl.First; J < Cl.First + Cl.Count; ++J)
			{
				const AirSky::Puff& Pf = Hole.Puffs[static_cast<size_t>(J)];
				if (Pl.Show * Pf.Alpha < 0.1 * Pf.Alpha) continue;
				Nearest = std::min(Nearest, std::hypot(Pf.At.X + Pl.Shift.X - Ships.TargetX, Pf.At.Y + Pl.Shift.Y - Ships.TargetY));
			}
		}
		const bool bHoleOpen = Hole.DeckAt(Ships.TargetX, Ships.TargetY) == 0.0 && Hole.DeckAt(Ships.TargetX + 1500.0, Ships.TargetY - 1500.0) == 0.0;
		Limits.push_back({ "a deck keeps a hole over the ships", Nearest >= 2200.0 && Hole.Cover >= 0.625 && bHoleOpen && Hole.DriftAt(3600.0).Length() == 0.0,
		                   Num(Nearest, 0) + " m to the nearest cloud, cover " + Num(Hole.Cover, 2) + (bHoleOpen ? ", open to falls" : ", closed to falls"), "at least 2,200 m, cover 5/8, open, never drifting" });
		const int DeckOutside = OutOfHeight(Under) + OutOfHeight(Raised) + OutOfHeight(Over) + OutOfHeight(Between) + OutOfHeight(Hole);
		Limits.push_back({ "deck tiles inside their own base and tops", DeckOutside == 0, std::to_string(DeckOutside) + " outside", "0" });

		// High ground: a broad mountain of 2,000 m 4 km off the target, a steep ridge of 1,000 m 6 km east (as steep as
		// Oahu's Koolau), a peak of 3,800 m 6 km north. Every puff's body clear of the ground under it, cloud lifted
		// over it, no deck tile within 1.5 km of the bombers, and the peak standing through the deck.
		const double TX = SkyAt.TargetX, TY = SkyAt.TargetY;
		const std::function<double(double, double)> Grounds[] = {
			[TX, TY](double X, double Y) { return 2000.0 * std::exp(-((X - TX - 4000.0) * (X - TX - 4000.0) + (Y - TY) * (Y - TY)) / (2.0 * 3000.0 * 3000.0)); },
			[TX](double X, double) { return 1000.0 * std::exp(-(X - TX - 6000.0) * (X - TX - 6000.0) / (2.0 * 700.0 * 700.0)); },
			[TX, TY](double X, double Y) { return 3800.0 * std::exp(-((X - TX) * (X - TX) + (Y - TY - 6000.0) * (Y - TY - 6000.0)) / (2.0 * 2500.0 * 2500.0)); },
		};
		const double Feature[3][2] = { { TX + 4000.0, TY }, { TX + 6000.0, TY }, { TX, TY + 6000.0 } };
		// (a field as the painter draws it, the wind carrying cumulus over the ground for 20 minutes (a deck: once),
		// painted whenever the painter asks to be: every drawn puff's heart ring 150 m over the highest ground under it,
		// its rim 30 m, each cloud's foot 300 m over the ground under its puffs' hearts, and no cloud's lift jumping more
		// than 8 m a second of air)
		struct Swept
		{
			int Buried = 0, Popped = 0, Paints = 0;
			double LeastHeart = 1e9, LeastRim = 1e9, LeastFoot = 1e9, MostRate = 0.0;
		};
		auto Sweep = [&SkyAt](const AirSky::Field& Laid, const std::function<double(double, double)>& Beneath, const AirSky::Vec& Eye, double Last, Swept& Out)
		{
			auto GroundAt = [&Beneath](double X, double Y) { return std::max(0.0, Beneath(X, Y)); };
			AirSky::Painter Pt;
			std::vector<AirSky::Vec> Pts;
			std::vector<uint8_t> Cols;
			std::vector<double> Before(Laid.Clouds.size(), -1.0), BeforeAir(Laid.Clouds.size(), 0.0);
			for (double Air = 0.0; Air <= Last + 1e-9; Air += 0.5)
			{
				const AirSky::Vec D = Laid.DriftAt(Air);
				if (!Pt.Stale(Eye, D)) continue;
				Pt.Paint(Laid, Eye, D, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Pts, Cols);
				++Out.Paints;
				const AirSky::Vec Seen = Eye - D;
				for (size_t I = 0; I < Laid.Clouds.size(); ++I)
				{
					const AirSky::Cloud& Cl = Laid.Clouds[I];
					const int Index = static_cast<int>(I);
					const double Shown = Pt.ShownLift(Index);
					const AirSky::Vec Shift = Laid.ShiftNear(Index, Seen.X, Seen.Y);
					const double WX = Cl.At.X + Shift.X + D.X, WY = Cl.At.Y + Shift.Y + D.Y;
					if (Shown < 0.0 || std::hypot(WX - Eye.X, WY - Eye.Y) > 9000.0)
					{
						Before[I] = -1.0;
						continue;
					}
					if (Before[I] >= 0.0)
					{
						const double Rate = std::fabs(Shown - Before[I]) / std::max(0.5, Air - BeforeAir[I]);
						Out.MostRate = std::max(Out.MostRate, Rate);
						if (Rate > 8.0) ++Out.Popped;
					}
					Before[I] = Shown;
					BeforeAir[I] = Air;
					const AirSky::Placed Pl = Laid.PlaceLifted(Index, Shift, D, Shown);
					if (Pl.Squash <= 0.0 || Pl.Show < 0.002) continue;
					double UnderHearts = 0.0;
					for (int J = Cl.First; J < Cl.First + Cl.Count; ++J)
					{
						const AirSky::Puff& Pf = Laid.Puffs[static_cast<size_t>(J)];
						const double PX = Pf.At.X + Shift.X + D.X, PY = Pf.At.Y + Shift.Y + D.Y, PZ = Cl.At.Z + Shown + (Pf.At.Z - Cl.At.Z) * Pl.Squash;
						const double Flat = std::max(0.08, Pf.Flat * Pl.Squash);
						double UnderHeart = GroundAt(PX, PY), UnderRim = UnderHeart;
						for (int K = 0; K < 8; ++K)
						{
							const double A = 2.0 * 3.14159265358979 * K / 8.0;
							UnderHeart = std::max(UnderHeart, GroundAt(PX + 0.55 * Pf.R * std::cos(A), PY + 0.55 * Pf.R * std::sin(A)));
							UnderRim = std::max(UnderRim, GroundAt(PX + Pf.R * std::cos(A), PY + Pf.R * std::sin(A)));
						}
						UnderRim = std::max(UnderRim, UnderHeart);
						UnderHearts = std::max(UnderHearts, UnderHeart);
						const double Heart = PZ - 0.55 * Pf.R * Flat - UnderHeart, Rim = PZ - Pf.R * Flat - UnderRim;
						Out.LeastHeart = std::min(Out.LeastHeart, Heart);
						Out.LeastRim = std::min(Out.LeastRim, Rim);
						if (Heart < 150.0 || Rim < 30.0) ++Out.Buried;
					}
					if (UnderHearts > 0.5) Out.LeastFoot = std::min(Out.LeastFoot, Cl.At.Z + Shown - UnderHearts);
				}
			}
		};
		auto SweptTold = [](const Swept& W, double Ms)
		{
			return std::to_string(W.Buried) + " puffs too low; heart ring " + Num(W.LeastHeart, 0) + " m, rim " + Num(W.LeastRim, 0) + " m, foot " + Num(W.LeastFoot, 0) + " m over the ground at least; lift " +
			       Num(W.MostRate, 1) + " m a second at most (" + std::to_string(W.Paints) + " paints, " + Num(Ms / 1000.0, 1) + " s)";
		};
		auto SweptOk = [](const Swept& W) { return W.Buried == 0 && W.LeastFoot >= 299.5 && W.Popped == 0; };
		int Lifted = 0, TooHigh = 0, Pierced = 0;
		Swept OverHills;
		bool bPeakOpen = true;
		const auto Sweep0 = std::chrono::steady_clock::now();
		for (int G = 0; G < 3; ++G)
		{
			AirSky::Placing Hills = SkyAt;
			Hills.Ground = Grounds[G];
			for (const AirSky::Day& Day : { Rec.Board.Cloud, Deck, Low })
			{
				const AirSky::Field Laid = AirSky::Lay(Day, Hills);
				// (each cloud where it is drawn by the high ground, as first laid: its copy nearest it, lifted and thinned)
				for (size_t I = 0; I < Laid.Clouds.size(); ++I)
				{
					const AirSky::Cloud& Cl = Laid.Clouds[I];
					const AirSky::Placed Pl = Laid.Place(static_cast<int>(I), Feature[G][0], Feature[G][1], AirSky::Vec());
					if (Pl.Squash <= 0.0)
					{
						++Pierced;
						continue;
					}
					if (Pl.Lift > 0.0) ++Lifted;
					const double Cap = std::max(Hills.Band - 1500.0, Laid.Top);
					if (Day.bDeck && Hills.Band > 2000.0 && Cl.At.Z + Pl.Lift + Pl.Squash * Cl.H > Cap + 0.5) ++TooHigh;
				}
				if (G == 2 && Day.bDeck && Laid.DeckAt(TX, TY + 6000.0) != 0.0) bPeakOpen = false;
				Sweep(Laid, Grounds[G], AirSky::Vec(Feature[G][0], Feature[G][1] - 4000.0, Laid.Top + 1000.0), Day.bDeck ? 0.0 : 1200.0, OverHills);
			}
		}
		const double SweepMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Sweep0).count();
		Limits.push_back({ "cloud lifted over high ground, the deck under the band", Lifted > 0 && TooHigh == 0 && Pierced > 0 && bPeakOpen,
		                   std::to_string(Lifted) + " lifted, " + std::to_string(TooHigh) + " tiles too high, " + std::to_string(Pierced) + " left out for the peak" + (bPeakOpen ? "" : ", the peak under cloud"),
		                   "some lifted, 0 too high, the peak through the deck" });
		Limits.push_back({ "drawn clear of high ground as the wind carries it", SweptOk(OverHills), SweptTold(OverHills, SweepMs), "heart 150, rim 30, foot 300 or more; at most 8 m a second" });

		// The same over Oahu's own ground, the stage's 150 m grid (Content/Data/Stages/oahu.json, as AAirStage reads it,
		// its aim on the target, unturned), with the wind carrying the day's cumulus over Kaala and over the Koolau's
		// highest; and the painter asked no more often over the island than over the sea, for an eye flying round the
		// harbour (its lift eases only as the wind carries it, whatever the eye does).
		{
			const std::string StagePath = Path.substr(0, Path.rfind("/AirBattles/") == std::string::npos ? 0 : Path.rfind("/AirBattles/")) + "/Stages/oahu.json";
			std::ifstream StageIn(StagePath);
			std::stringstream StageText;
			StageText << StageIn.rdbuf();
			AirFeed::Json StageJson;
			std::string StageWhy;
			const bool bStage = StageIn && AirFeed::Parse(StageText.str(), StageJson, StageWhy);
			const AirFeed::Json& Grid = StageJson["ground"];
			const double E0 = Grid["e0"].Num(), N0 = Grid["n0"].Num(), Cell = Grid["cell"].Num(150.0);
			const int Cols = Grid["cols"].Int(), Rows = Grid["rows"].Int();
			std::vector<double> Heights;
			for (size_t K = 0; K < Grid["z"].Count(); ++K) Heights.push_back(Grid["z"].At(K).Num());
			const bool bGrid = bStage && Cols > 0 && Rows > 0 && Heights.size() == static_cast<size_t>(Cols) * static_cast<size_t>(Rows);
			const std::function<double(double, double)> Oahu = [&Heights, E0, N0, Cell, Cols, Rows, TX, TY](double X, double Y)
			{
				const int Col = static_cast<int>(std::floor((X - TX - E0) / Cell)), Row = static_cast<int>(std::floor((Y - TY - N0) / Cell));
				if (Col < 0 || Row < 0 || Col >= Cols || Row >= Rows) return 0.0;
				return std::max(0.0, Heights[static_cast<size_t>(Row) * static_cast<size_t>(Cols) + static_cast<size_t>(Col)]);
			};
			// (Kaala, the island's highest, and the Koolau's highest east of the harbour)
			double KaalaX = TX, KaalaY = TY, KaalaZ = -1.0, KoolauX = TX, KoolauY = TY, KoolauZ = -1.0;
			for (int Row = 0; bGrid && Row < Rows; ++Row)
				for (int Col = 0; Col < Cols; ++Col)
				{
					const double Z = Heights[static_cast<size_t>(Row) * static_cast<size_t>(Cols) + static_cast<size_t>(Col)];
					const double X = TX + E0 + (Col + 0.5) * Cell, Y = TY + N0 + (Row + 0.5) * Cell;
					if (Z > KaalaZ) { KaalaZ = Z; KaalaX = X; KaalaY = Y; }
					if (X > TX + 2000.0 && Z > KoolauZ) { KoolauZ = Z; KoolauX = X; KoolauY = Y; }
				}
			AirSky::Placing OahuAt = SkyAt;
			OahuAt.Ground = Oahu;
			const AirSky::Field OverOahu = AirSky::Lay(Rec.Board.Cloud, OahuAt);
			Swept Island;
			const auto Island0 = std::chrono::steady_clock::now();
			if (bGrid)
			{
				Sweep(OverOahu, Oahu, AirSky::Vec(KaalaX, KaalaY - 4000.0, OverOahu.Top + 1000.0), 1200.0, Island);
				Sweep(OverOahu, Oahu, AirSky::Vec(KoolauX, KoolauY - 4000.0, OverOahu.Top + 1000.0), 1200.0, Island);
			}
			const double IslandMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Island0).count();
			Limits.push_back({ "drawn clear of Oahu's own ground as the wind carries it", bGrid && SweptOk(Island),
			                   bGrid ? "Kaala " + Num(KaalaZ, 0) + " m, Koolau " + Num(KoolauZ, 0) + " m: " + SweptTold(Island, IslandMs) : "no stage ground at " + StagePath,
			                   "heart 150, rim 30, foot 300 or more; at most 8 m a second" });
			// (an eye flying round the harbour 8 km out at 3,000 m and 120 m/s, a minute at 60 frames a second)
			const AirSky::Field OverSea = AirSky::Lay(Rec.Board.Cloud, SkyAt);
			auto PaintsFlying = [&SkyAt, TX, TY](const AirSky::Field& Laid)
			{
				AirSky::Painter Pt;
				std::vector<AirSky::Vec> Pts;
				std::vector<uint8_t> Colours;
				int Count = 0;
				for (int Frame = 0; Frame < 3600; ++Frame)
				{
					const double Secs = Frame / 60.0, A = Secs * 120.0 / 8000.0;
					const AirSky::Vec Eye(TX + 8000.0 * std::cos(A), TY + 8000.0 * std::sin(A), 3000.0);
					const AirSky::Vec D = Laid.DriftAt(Secs);
					if (!Pt.Stale(Eye, D)) continue;
					Pt.Paint(Laid, Eye, D, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Pts, Colours);
					++Count;
				}
				return Count;
			};
			const int PaintsIsland = bGrid ? PaintsFlying(OverOahu) : 0, PaintsSea = PaintsFlying(OverSea);
			Limits.push_back({ "the sky painted no more often over the island than the sea", bGrid && PaintsIsland <= PaintsSea + PaintsSea / 4 + 20,
			                   std::to_string(PaintsIsland) + " paints over Oahu, " + std::to_string(PaintsSea) + " over the sea, of 3,600 frames", "at most a quarter more over the island" });
		}
		AirFeed::Json Bare;
		std::string Why;
		AirFeed::Sky Old;
		const bool bRead = AirFeed::Parse("{\"place\":{\"kind\":\"city\",\"name\":\"Essen\"}}", Bare, Why) && AirFeed::ReadSky(Bare, Old);
		const AirSky::Field Plain = AirSky::Lay(Old.Cloud, AirFeed::PlacingOf(Old, {}, 0));
		Limits.push_back({ "a ticket with no sky draws cumulus", bRead && !Old.Cloud.bGiven && !Plain.Sky.bDeck && !Plain.Clouds.empty() && Plain.DeckTop == 0.0,
		                   bRead ? std::to_string(Plain.Clouds.size()) + " cumulus" : "unread", "cumulus, no deck" });

		// This battle flown again under a deck, twice. First at 3,500 to 4,500 m, not a day's height but a test's (laid
		// for a band of 6,500 m), so that about half its losses begin above it and half under it, with its hole over the
		// middle of where they begin, over the sea. Then the lowest band's deck (laid 900 to 1,500 m, put at 2,400 to 3,000 m over a band of
		// 1,800 m) round the 3,800 m peak, whose tiles near it are lifted over the laid top. The eye is the main run's,
		// the deck drawn out to the scene's reach. Every end is judged against the drawn top puffs found tile by tile, not
		// through the deck's own lookup: an end in the deck lies in a drawn tile, between its foot and 150 m under its
		// top puff's middle (halfway, in a thin tile); and nothing that came out over a tile's foot is ever left falling,
		// shown, inside a drawn tile.
		struct Flown
		{
			int AboveFalls = 0, InsideFalls = 0, UnderFalls = 0, UnderLifted = 0, InDeck = 0, Through = 0, UnderDown = 0, OnHigh = 0, WrongEnd = 0, TimedOut = 0, WrongDeck = 0;
			int PiecesDown = 0, ShownInside = 0, FellInside = 0, Mismatch = 0, Lookups = 0, RowInClear = 0, RowsLetGo = 0;
			double Ms = 0.0;
		};
		const AirShow::Vec MainEye = Show.Eye;
		auto FlyUnder = [&](const AirSky::Field& Test, const std::function<double(double, double)>& Ground, const AirShow::Vec& Eye)
		{
			const auto Fly0 = std::chrono::steady_clock::now();
			Flown Out;
			AirShow::Show Decked(Rec.Seed ? Rec.Seed : 238);
			AirFeed::Apply(Rec.Board, Decked);
			Decked.Ground = Ground;
			Decked.CloudTop = Test.DeckTop;
			Decked.DeckAt = [&Test](double X, double Y)
			{
				AirShow::DeckSpan Here;
				Here.Hide = Test.DeckAt(X, Y, &Here.Foot);
				return Here;
			};
			Decked.DeckReach = AirSky::Reach * 5.0 / 6.0;
			Decked.DeckBase = Test.Base;
			Decked.Eye = Eye;
			auto Floor = [&Ground](double X, double Y) { return Ground ? std::max(0.0, Ground(X, Y)) : 0.0; };
			// (the deck over a point found tile by tile: the nearest tile whose top puff, at its copy nearest the point,
			// reaches over it within eight tenths of its reach and is drawn there, more than half shown)
			auto Drawn = [&](double X, double Y)
			{
				AirShow::DeckSpan Best;
				++Out.Lookups;
				if (std::hypot(X - Decked.Eye.X, Y - Decked.Eye.Y) > Decked.DeckReach) return Best;
				double BestFar = 1e18;
				for (size_t K = 0; K < Test.Clouds.size(); ++K)
				{
					const AirSky::Cloud& Cl = Test.Clouds[K];
					if (Cl.bScrap) continue; // (a scrap in the ships' hole is not the deck: falls pass through it)
					const AirSky::Puff& Top = Test.Puffs[static_cast<size_t>(Cl.First)];
					const AirSky::Vec Shift = Test.ShiftNear(static_cast<int>(K), X, Y);
					const double Ex = Top.At.X + Shift.X - X, Ey = Top.At.Y + Shift.Y - Y, Far = Ex * Ex + Ey * Ey;
					if (Far > 0.64 * Top.R * Top.R || Far >= BestFar) continue;
					const AirSky::Placed Pl = Test.Place(static_cast<int>(K), X, Y, AirSky::Vec());
					if (Pl.Squash <= 0.0 || Pl.Show < 0.5) continue;
					BestFar = Far;
					Best.Foot = Cl.At.Z + Pl.Lift;
					Best.Hide = Cl.At.Z + Pl.Lift + (Top.At.Z - Cl.At.Z) * Pl.Squash;
				}
				return Best;
			};
			auto Hides = [](const AirShow::DeckSpan& H) { return H.Hide - std::min(150.0, 0.5 * (H.Hide - H.Foot)); };
			// (a lookup is wanted only where a tile could be: from the laid base up to the highest a lift could raise one)
			const double LowestFoot = Test.Base - 1.0, HighestTop = Test.Top + (Ground ? 5000.0 : 1.0);
			struct Begun
			{
				bool bAbove = false, bRow = false;
				double Air = 0.0, RowZ = 0.0;
			};
			std::map<int, Begun> Begins;
			size_t Next = 0;
			for (double At = Rec.Views.front().Head.T; At < EndT - 1e-9;)
			{
				At = std::min(EndT, At + 1.0 / 30.0);
				while (Next < Rec.Views.size() && Rec.Views[Next].Head.T <= At + 1.0 + 1e-9)
				{
					Decked.Take(Rec.Views[Next].Head.T, Rec.Views[Next].Groups, Rec.Views[Next].Events, Rec.Views[Next].Ships);
					++Next;
				}
				Decked.Advance(At);
				for (const AirShow::Fx& E : Decked.Effects)
				{
					if (E.Kind == AirShow::FxKind::Fall)
					{
						const AirShow::DeckSpan Here = Drawn(E.At.X, E.At.Y);
						const bool bAbove = E.At.Z > Test.DeckTop;
						Out.AboveFalls += bAbove ? 1 : 0;
						Out.InsideFalls += !bAbove && E.At.Z > Test.Base ? 1 : 0;
						Out.UnderFalls += E.At.Z <= Test.Base ? 1 : 0;
						const bool bOver = Here.Hide > 0.0 && E.At.Z > Here.Foot;
						// (above the laid top, but under the foot of a tile lifted over high ground)
						Out.UnderLifted += bAbove && Here.Hide > 0.0 && !bOver ? 1 : 0;
						Begins[E.Key] = { bAbove, bOver, Decked.AirNow(), bOver ? std::min(Here.Hide, E.At.Z) : 0.0 };
						// (the kit's debris ends in the drawn tile over where the fall began, no higher than the fall, else
						// on the sea or the ground)
						const double Want = bOver ? std::min(Here.Hide, E.At.Z) : Floor(E.At.X, E.At.Y);
						if (std::fabs(E.Deck - Want) > 0.5) ++Out.WrongDeck;
					}
					if (E.Kind == AirShow::FxKind::FallStop)
					{
						const auto Found = Begins.find(E.Key);
						if (Found == Begins.end()) continue;
						const AirShow::DeckSpan Here = Drawn(E.At.X, E.At.Y);
						double Foot = 0.0;
						const double Hide = std::hypot(E.At.X - Decked.Eye.X, E.At.Y - Decked.Eye.Y) > Decked.DeckReach ? 0.0 : Test.DeckAt(E.At.X, E.At.Y, &Foot);
						if (std::fabs(Hide - Here.Hide) > 0.5 || (Hide > 0.0 && std::fabs(Foot - Here.Foot) > 0.5)) ++Out.Mismatch;
						const double Land = Floor(E.At.X, E.At.Y);
						const bool bDown = E.At.Z <= Land + 2.5;
						const bool bInCloud = Here.Hide > 0.0 && E.At.Z > Here.Foot - 0.5 && E.At.Z <= Hides(Here) + 0.5;
						if (bInCloud) ++Out.InDeck;
						else if (bDown && Found->second.bAbove) ++Out.Through;
						else if (bDown) ++Out.UnderDown;
						else if (Decked.AirNow() - Found->second.Air >= 399.0) ++Out.TimedOut;
						else ++Out.WrongEnd;
						if (bDown && Land > 2.5) ++Out.OnHigh;
						Begins.erase(Found);
					}
					if ((E.Kind == AirShow::FxKind::Crash || E.Kind == AirShow::FxKind::Splash) && E.What == "piece") ++Out.PiecesDown;
					// (the debris row sheds only over a drawn tile, above its foot and above the height it was told; let
					// go (Deck -1) once not; the move on which a fall ends in a tile aside)
					if (E.Kind == AirShow::FxKind::FallMove)
					{
						const auto Found = Begins.find(E.Key);
						if (Found == Begins.end() || !Found->second.bRow) continue;
						if (E.Deck < 0.0)
						{
							Found->second.bRow = false;
							++Out.RowsLetGo;
							continue;
						}
						const AirShow::DeckSpan Here = E.At.Z < LowestFoot ? AirShow::DeckSpan() : Drawn(E.At.X, E.At.Y);
						const bool bEnding = Here.Hide > 0.0 && E.At.Z > Here.Foot && E.At.Z < Hides(Here) - 0.5;
						if (!bEnding && (Here.Hide <= 0.0 || E.At.Z <= Here.Foot || E.At.Z < Found->second.RowZ - 0.5)) ++Out.RowInClear;
					}
				}
				// (no falling aircraft still falling inside a drawn tile, whatever it passed before: under one tile's foot it
				// may yet fall into a lower one beside it)
				for (const AirShow::PlaneOut& P : Decked.Planes())
				{
					if (!P.bFalling || !P.bShown || P.At.Z < LowestFoot || P.At.Z > HighestTop) continue; // (one blown apart is not drawn)
					const AirShow::DeckSpan Here = Drawn(P.At.X, P.At.Y);
					if (Here.Hide > 0.0 && P.At.Z > Here.Foot && P.At.Z < Hides(Here) - 0.5) ++Out.FellInside;
				}
				// (nor a piece or a parachute)
				for (const AirShow::Thing& Th : Decked.Things())
				{
					if (Th.Kind == AirShow::ThingKind::Bomb || Th.At.Z < LowestFoot || Th.At.Z > HighestTop) continue;
					const AirShow::DeckSpan Here = Drawn(Th.At.X, Th.At.Y);
					if (Here.Hide > 0.0 && Th.At.Z > Here.Foot && Th.At.Z < Hides(Here) - 1.0) ++Out.ShownInside;
				}
				Decked.Effects.clear();
			}
			Out.Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Fly0).count();
			return Out;
		};
		auto Told = [](const Flown& F)
		{
			return std::to_string(F.AboveFalls) + " begun above it (" + std::to_string(F.UnderLifted) + " under a lifted tile), " + std::to_string(F.InsideFalls) + " in its layer, " +
			       std::to_string(F.UnderFalls) + " under it; " + std::to_string(F.InDeck) + " end in it, " + std::to_string(F.Through) + " from above through a hole or past it, " +
			       std::to_string(F.UnderDown) + " from under it on the ground or sea, " + std::to_string(F.OnHigh) + " on high ground; " + std::to_string(F.WrongEnd) + " end in clear air, " +
			       std::to_string(F.FellInside) + " left falling in cloud, " + std::to_string(F.TimedOut) + " timed out, " + std::to_string(F.WrongDeck) + " wrong debris ends, " +
			       std::to_string(F.ShownInside) + " small things shown inside, " + std::to_string(F.Mismatch) + " lookups unlike the tiles; debris rows " + std::to_string(F.RowsLetGo) +
			       " let go, " + std::to_string(F.RowInClear) + " moves shedding in clear air; " + std::to_string(F.PiecesDown) + " pieces down (" +
			       Num(F.Ms / 1000.0, 1) + " s)";
		};
		// (where the main run's falls began: the middles of their places and heights)
		std::vector<double> FallXs, FallYs, FallZs;
		for (const AirSky::Vec& F : FallsAt)
		{
			FallXs.push_back(F.X);
			FallYs.push_back(F.Y);
		}
		for (const AirSky::Vec& F : FallsAt) FallZs.push_back(F.Z);
		std::sort(FallXs.begin(), FallXs.end());
		std::sort(FallYs.begin(), FallYs.end());
		std::sort(FallZs.begin(), FallZs.end());
		AirSky::Day TestDay = Deck;
		TestDay.Base = 3500.0;
		TestDay.Top = 4500.0;
		AirSky::Placing TestAt = SkyAt;
		TestAt.Band = 6500.0;
		TestAt.LowBand = 0.0;
		TestAt.bShips = true;
		// (its hole laid over the middle of where the falls began, not over the ships, so that some falls begun above
		// the deck go down through the hole and the rest end in its tiles round it, whatever the day's tiles happen to
		// leave open: on a deck day of eight tenths, twenty falls over the tiles could all end in them)
		if (!FallXs.empty())
		{
			TestAt.TargetX = FallXs[FallXs.size() / 2];
			TestAt.TargetY = FallYs[FallYs.size() / 2];
		}
		const AirSky::Field Test = AirSky::Lay(TestDay, TestAt);
		const Flown Sea = FlyUnder(Test, std::function<double(double, double)>(), MainEye);
		Limits.push_back({ "under a deck, falls begun above it end in it where drawn",
		                   Sea.WrongDeck == 0 && Sea.WrongEnd == 0 && Sea.FellInside == 0 && Sea.ShownInside == 0 && Sea.Mismatch == 0 && Sea.RowInClear == 0 && Sea.TimedOut <= 2 && Sea.InDeck > 0 && Sea.Through > 0 && (Sea.UnderFalls == 0 || Sea.UnderDown > 0),
		                   Told(Sea), "every end in a drawn tile or on the sea, some of each; none in clear air or left in cloud" });
		// (a high table of ground where the battle's falls are thickest, the middle of where they began, 150 m under
		// the middle of their heights, so that falls begin over it above the laid top but under the tiles lifted over
		// it; not a height of these islands but a test's; the eye on it)
		// (its middle 1.5 km east of theirs, so that its steep edge, where tiles side by side are lifted unlike, is
		// where falls are too)
		const double PX = (FallXs.empty() ? TX : FallXs[FallXs.size() / 2]) + 1500.0, PY = FallYs.empty() ? TY + 6000.0 : FallYs[FallYs.size() / 2];
		const double PH = FallZs.empty() ? 3800.0 : std::min(4500.0, std::max(1000.0, FallZs[FallZs.size() / 2] - 150.0));
		const std::function<double(double, double)> Peak = [PX, PY, PH](double X, double Y)
		{
			const double Q = ((X - PX) * (X - PX) + (Y - PY) * (Y - PY)) / (2.0 * 3000.0 * 3000.0);
			return PH * std::exp(-Q * Q);
		};
		AirSky::Placing PeakAt = Lowest;
		PeakAt.Ground = Peak;
		const AirSky::Field Peaked = AirSky::Lay(Low, PeakAt);
		const Flown Hill = FlyUnder(Peaked, Peak, AirShow::Vec(PX, PY - 6000.0, 5000.0));
		Limits.push_back({ "the lowest band's deck lifted over high ground in the fight",
		                   Hill.WrongDeck == 0 && Hill.WrongEnd == 0 && Hill.FellInside == 0 && Hill.ShownInside == 0 && Hill.Mismatch == 0 && Hill.RowInClear == 0 && Hill.TimedOut <= 2 && Hill.UnderLifted > 0 && Hill.OnHigh > 0,
		                   Told(Hill), "every end in a drawn tile or on the ground, some under a lifted tile and on the peak; none in clear air or left in cloud" });
		// A step in the deck, sharper than any lift makes: west of the middle of where falls begin, a tile whose foot is
		// higher than any fall; east of it a thick tile low down. Every fall west of the step is under a tile's foot,
		// and one that drifts east must still end in the low tile: passing under one tile never lets a fall, a piece
		// or a parachute through a lower one beside it.
		{
			const double SX = FallXs.empty() ? TX : FallXs[FallXs.size() / 2];
			const double LowFoot = 100.0, LowHide = 3000.0, LowEnds = LowHide - 150.0;
			AirShow::Show Stepped(Rec.Seed ? Rec.Seed : 238);
			AirFeed::Apply(Rec.Board, Stepped);
			Stepped.Eye = MainEye;
			Stepped.DeckAt = [SX, LowFoot, LowHide](double X, double)
			{
				AirShow::DeckSpan Here;
				Here.Foot = X < SX ? 20000.0 : LowFoot;
				Here.Hide = X < SX ? 20500.0 : LowHide;
				return Here;
			};
			std::map<int, bool> BegunWest;
			int Crossed = 0, Stuck = 0, ThingsStuck = 0;
			size_t Next = 0;
			for (double At = Rec.Views.front().Head.T; At < EndT - 1e-9;)
			{
				At = std::min(EndT, At + 1.0 / 30.0);
				while (Next < Rec.Views.size() && Rec.Views[Next].Head.T <= At + 1.0 + 1e-9)
				{
					Stepped.Take(Rec.Views[Next].Head.T, Rec.Views[Next].Groups, Rec.Views[Next].Events, Rec.Views[Next].Ships);
					++Next;
				}
				Stepped.Advance(At);
				for (const AirShow::Fx& E : Stepped.Effects)
				{
					if (E.Kind == AirShow::FxKind::Fall) BegunWest[E.Key] = E.At.X < SX;
					if (E.Kind != AirShow::FxKind::FallStop) continue;
					const auto Found = BegunWest.find(E.Key);
					if (Found == BegunWest.end()) continue;
					if (Found->second && E.At.X >= SX && E.At.Z > LowFoot && E.At.Z <= LowEnds + 0.5) ++Crossed;
					BegunWest.erase(Found);
				}
				for (const AirShow::PlaneOut& P : Stepped.Planes())
				{
					if (P.bFalling && P.bShown && P.At.X >= SX && P.At.Z > LowFoot && P.At.Z < LowEnds - 0.5) ++Stuck;
				}
				for (const AirShow::Thing& Th : Stepped.Things())
				{
					if (Th.Kind != AirShow::ThingKind::Bomb && Th.At.X >= SX && Th.At.Z > LowFoot && Th.At.Z < LowEnds - 1.0) ++ThingsStuck;
				}
				Stepped.Effects.clear();
			}
			Limits.push_back({ "a fall under one tile's foot still ends in a lower one beside it",
			                   Crossed > 0 && Stuck == 0 && ThingsStuck == 0,
			                   std::to_string(Crossed) + " falls begun west of the step end in the low tile east of it; " + std::to_string(Stuck) +
			                       " frames of a plane left falling in it, " + std::to_string(ThingsStuck) + " of a piece or parachute",
			                   "some cross and end there; none left falling inside" });
		}
	}

	int Failed = 0;
	for (const Limit& L : Limits)
	{
		std::printf("  %s %-58s %s (%s)\n", L.bOk ? "ok  " : "FAIL", L.Name, L.Got.c_str(), L.Want);
		if (!L.bOk) ++Failed;
	}

	// A live battle whose first view names no model (the bridge's opening view before the rules' mend): every group's
	// first view carries a wrong stand-in, marked as a guess. The show takes it, then the next view a battle second
	// ahead as the scene does, then draws: every aircraft, and every group as shown, must be its own model by then.
	if (Rec.Views.size() >= 2)
	{
		AirShow::Show Live(Rec.Seed ? Rec.Seed : 238);
		AirFeed::Apply(Rec.Board, Live);
		std::vector<AirShow::Group> First = Rec.Views[0].Groups;
		std::map<int, std::string> Own;
		for (const AirShow::Group& G : Rec.Views[1].Groups) Own[G.Id] = G.Model;
		for (AirShow::Group& G : First)
		{
			G.Model = G.bFighter ? "Bf109" : "B17";
			G.bModelGuessed = true;
		}
		Live.Take(Rec.Views[0].Head.T, First, Rec.Views[0].Events, Rec.Views[0].Ships);
		Live.Take(Rec.Views[1].Head.T, Rec.Views[1].Groups, Rec.Views[1].Events, Rec.Views[1].Ships);
		Live.Advance(Rec.Views[0].Head.T + 1.0 / Fps);
		int Wrong = 0, Planes = 0;
		for (const AirShow::PlaneOut& P : Live.Planes())
		{
			++Planes;
			Wrong += Own.count(P.Group) && Own[P.Group] != P.Model ? 1 : 0;
		}
		for (const auto& G : Live.Groups()) Wrong += Own.count(G.first) && Own[G.first] != G.second.Model ? 1 : 0;
		const Limit L{ "a live battle's guessed models made over by the next view", Wrong == 0 && Planes > 0, std::to_string(Wrong) + " wrong (of " + std::to_string(Planes) + " aircraft and their groups)", "0 wrong" };
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
