// The ground battle's replay, sound and camera cores played through a whole recorded battle at 60 frames a second,
// with no engine (Battle/LandFeed, LandPlay, LandSound, LandEye, LandDesk): what the game plays, counted.
//
//   land_play_test <recording.json> <mix.json> [out folder]
//
// It checks: every round fired once and landed once; every 'shell' event one blast (paired with its kill round where
// the rules fired one); every shot heard has a take, none on the screen dropped; the same take never twice within
// 0.15 s; the eye never under the ground, never cutting faster than every 3 s; the bar starts level and ends on the
// winner's side. It reports how many of the falls the camera saw, and the cores' time a frame.

#include "Battle/LandDesk.h"
#include "Battle/LandEye.h"
#include "Battle/LandFeed.h"
#include "Battle/LandJson.h"
#include "Battle/LandPlay.h"
#include "Battle/LandSound.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
	using LandPlay::Vec;

	int Faults = 0;

	void Fault(const char* What, double T = -1.0)
	{
		if (Faults < 30) std::printf("  FAULT%s%s\n", T >= 0.0 ? (" at " + std::to_string(T) + ": ").c_str() : ": ", What);
		++Faults;
	}

	/** Whether a point is in the camera's picture (16:9, the pose's vertical lens). */
	bool InPicture(const LandEye::Pose& P, const Vec& At, double Margin = 1.0)
	{
		const Vec Fwd = (P.Look - P.Eye).Unit();
		const Vec To = At - P.Eye;
		const double Depth = To.Dot(Fwd);
		if (Depth < 0.5) return false;
		const Vec Right = Vec(Fwd.Y, -Fwd.X, 0.0).Unit();
		const Vec Up = Vec(Right.Y * Fwd.Z - Right.Z * Fwd.Y, Right.Z * Fwd.X - Right.X * Fwd.Z, Right.X * Fwd.Y - Right.Y * Fwd.X);
		const double TanV = std::tan(P.Fov * 0.5 * 3.14159265358979 / 180.0) * Margin;
		const double TanH = TanV * 16.0 / 9.0;
		return std::abs(To.Dot(Right)) / Depth <= TanH && std::abs(To.Dot(Up)) / Depth <= TanV;
	}
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::printf("usage: land_play_test <recording.json> <mix.json> [out folder]\n");
		return 2;
	}
	std::string Text, MixText, Error;
	auto T0 = std::chrono::steady_clock::now();
	if (!LandJson::ReadFile(argv[1], Text)) { std::printf("cannot read %s\n", argv[1]); return 2; }
	LandFeed::Recording Rec;
	if (!LandFeed::ReadRecording(Text, Rec, Error)) { std::printf("cannot read the recording: %s\n", Error.c_str()); return 2; }
	const double ReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	if (!LandJson::ReadFile(argv[2], MixText)) { std::printf("cannot read %s\n", argv[2]); return 2; }
	std::printf("%s: \"%s\", %s, %zu blocks, %zu tracks (%zu twins left out), %zu rounds, %zu events, %.1f s; read in %.0f ms\n",
		Rec.Key.c_str(), Rec.Title.c_str(), Rec.Date.c_str(), Rec.Blocks.size(), Rec.Tracks.size(),
		static_cast<size_t>(std::count_if(Rec.Tracks.begin(), Rec.Tracks.end(), [](const LandFeed::Track& T) { return T.bTwin; })),
		Rec.Rounds.size(), Rec.Events.size(), Rec.End, ReadMs);

	LandPlay::World PW;
	LandPlay::Replay Play;
	Play.Begin(Rec, PW);
	LandSound::Mixer Mix;
	if (!Mix.Begin(MixText, Rec.Seed, Error)) { std::printf("cannot read the mix: %s\n", Error.c_str()); return 2; }
	// How long each take plays, from the recordings' own headers (Sounds/Real beside Content), as the game reads them:
	// the limits and the same-piece rule use it. A take not in Sounds/Real is counted (the game would play it silent).
	std::map<std::string, double> Seconds;
	std::set<std::string> Missing;
	{
		std::string Real0 = argv[2];
		const size_t Cut = Real0.rfind("Content/");
		Real0 = (Cut == std::string::npos ? std::string("./") : Real0.substr(0, Cut)) + "Sounds/Real/";
		Mix.Length = [&Seconds, &Missing, Real0](const std::string& Name)
		{
			auto Known = Seconds.find(Name);
			if (Known != Seconds.end()) return Known->second;
			double Long = 1.5;
			std::ifstream F(Real0 + Name + ".wav", std::ios::binary);
			if (!F) Missing.insert(Name);
			else
			{
				// RIFF: the fmt chunk's byte rate, the data chunk's size.
				char Head[12];
				F.read(Head, 12);
				uint32_t Rate = 0;
				while (F)
				{
					char Id[4];
					uint32_t Size = 0;
					F.read(Id, 4);
					F.read(reinterpret_cast<char*>(&Size), 4);
					if (!F) break;
					if (std::string(Id, 4) == "fmt ")
					{
						char Fmt[16];
						F.read(Fmt, 16);
						std::memcpy(&Rate, Fmt + 8, 4);
						F.seekg(static_cast<std::streamoff>(Size) - 16 + (Size & 1), std::ios::cur);
					}
					else if (std::string(Id, 4) == "data") { if (Rate > 0) Long = static_cast<double>(Size) / Rate; break; }
					else F.seekg(static_cast<std::streamoff>(Size) + (Size & 1), std::ios::cur);
				}
			}
			Seconds[Name] = Long;
			return Long;
		};
	}
	LandEye::World EW;
	LandEye::Settings ES;
	ES.Seed = Rec.Seed;
	LandEye::Director Eye;
	Eye.Begin(Rec, EW, ES);
	LandDesk::Desk Desk;
	Desk.Begin(Rec, 0);

	LandEye::Pose Pose;
	Mix.OnScreen = [&](const Vec& At) { return InPicture(Pose, At, 1.1); };

	const double Fps = 60.0, Dt = 1.0 / Fps;
	// The clock: twice life before the armies meet, then as life (the scene's own default), to the end and 6 s more.
	double Battle = 0.0, Real = 0.0;
	std::vector<LandPlay::Effect> Fx;
	std::vector<LandPlay::Noise> Noises;
	std::vector<LandPlay::Loss> Losses;
	std::vector<LandSound::Take> Takes;
	std::vector<LandSound::Bed> Beds;
	std::map<int, int> Fired, Landed;
	std::map<std::string, int> FxCount, NoiseCount;
	std::map<std::string, std::vector<std::pair<double, double>>> TakeAt; // piece -> (starts, pitch) of every copy
	double MostLate = 0.0;
	int VoiceLines = 0;
	int SameTakeTooSoon = 0, FallsSeen = 0, FallsNear = 0, Falls = 0, KnockedSeen = 0, Knocked = 0;
	double LastCutReal = -100.0, ShortestShot = 1e9, MeterMax = 0.0, MeterSum = 0.0;
	int MeterN = 0;
	// The meter's raw heat every 0.25 s of real time (the sound director sets fullAt from it): battle second, heat.
	std::vector<std::pair<double, double>> HeatSamples;
	std::vector<std::pair<double, double>> MeterSamples;
	double NextHeatSample = 0.0, BlockHeat = 0.0;
	double Slowest = 0.0, Total = 0.0;
	int Frames = 0;
	std::map<std::string, double> ShotSeconds;
	const double Contact = Rec.Contact >= 0.0 ? Rec.Contact : 0.0;
	double KnotStart = -1.0, KnotEnd = -1.0;
	double LowestEye = 1e9;
	std::vector<std::string> CutLines;

	while (Battle < Rec.End + 6.0)
	{
		const double Speed = Battle < Contact - 10.0 ? 2.0 : 1.0;
		const double From = Battle;
		Battle += Dt * Speed;
		Real += Dt;
		auto F0 = std::chrono::steady_clock::now();
		Fx.clear();
		Noises.clear();
		Losses.clear();
		Takes.clear();
		Play.Step(From, Battle, Fx, Noises, Losses);
		Pose = Eye.Update(Battle, Real, Dt);
		const double Ground = Rec.Ground.HeightAt(Pose.Look.X, Pose.Look.Y);
		LandSound::Listener Ear;
		Ear.CameraHeight = Pose.Eye.Z - Rec.Ground.HeightAt(Pose.Eye.X, Pose.Eye.Y);
		Ear.At = Vec(Pose.Look.X, Pose.Look.Y, Ground + Mix.EarHeightFactor() * std::max(0.0, Ear.CameraHeight));
		// (on the ground among the men the ear is where the eye is: the look point may be 70 m ahead)
		if (Ear.CameraHeight < 4.0) Ear.At = Pose.Eye;
		for (const LandPlay::Noise& N : Noises) Mix.Hear(N, Real, Battle, Speed, Ear, Takes);
		int Fighting = 0, Blocks = 0;
		if (const LandFeed::View* V = Rec.ViewAt(Battle))
		{
			for (const LandFeed::Unit& U : V->Units)
			{
				++Blocks;
				if (U.Act == "fight" && std::hypot(U.X - Ear.At.X, U.Y - Ear.At.Y) < 600.0) ++Fighting;
			}
		}
		Mix.Tick(Real, Dt, Ear, Fighting, Blocks ? static_cast<double>(Fighting) / Blocks : 0.0, Battle >= Contact, "open", Beds);
		const LandDesk::Words& W = Desk.Update(Battle, Dt);
		const double FrameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - F0).count();
		Slowest = std::max(Slowest, FrameMs);
		Total += FrameMs;
		++Frames;

		if (KnotStart < 0.0) KnotStart = W.Knot;
		KnotEnd = W.Knot;
		for (const LandPlay::Effect& E : Fx)
		{
			static const char* Names[] = { "muzzle", "shots", "shell", "size", "skip", "glance", "plate" };
			++FxCount[Names[static_cast<int>(E.Kind)]];
			if (std::isnan(E.At.X) || std::isnan(E.At.Y) || std::isnan(E.At.Z)) Fault("an effect at no number", Battle);
		}
		for (const LandPlay::Noise& N : Noises)
		{
			static const char* Names[] = { "shot", "landing", "glance", "holed", "fall", "knocked_out", "contact", "broken" };
			++NoiseCount[Names[static_cast<int>(N.Kind)]];
		}
		for (const LandSound::Take& T : Takes)
		{
			// The same piece of a recording never within 0.15 s of itself, or within its own length when shorter.
			const double Starts = Real + T.Delay;
			char Piece[32];
			std::snprintf(Piece, sizeof(Piece), "@%.3f", T.From);
			const double Plays = T.Cut > 0.0 ? T.Cut : std::max(0.02, Mix.Length(T.Name) - T.From);
			// (on time at another pitch, 3.5% or more from EVERY copy still sounding, is no copy)
			std::vector<std::pair<double, double>>& Copies = TakeAt[T.Name + Piece];
			for (const auto& Copy : Copies)
			{
				const bool bOtherPitch = Copy.second > 0.0 && std::abs(T.Pitch / Copy.second - 1.0) >= 0.035;
				if (Starts < Copy.first + std::min(0.15, Plays) - 1e-9 && Starts > Copy.first - 1e-9 && !bOtherPitch) { ++SameTakeTooSoon; break; }
			}
			Copies.push_back({ Starts, T.Pitch });
			// (a shot heard more than a quarter second after it is seen, by the rule's holding: none)
			if (T.Delay > 0.0 && T.Family != "body" && T.Family != "voices") MostLate = std::max(MostLate, T.Delay);
			if (T.Bus == "voices") ++VoiceLines;
		}
		for (const LandPlay::Loss& L : Losses)
		{
			const double Far = (L.At - Pose.Eye).Length();
			const bool bSeen = InPicture(Pose, L.At + Vec(0.0, 0.0, 0.8)) && Far < (Pose.Fov < 32.0 ? 400.0 : 200.0);
			if (L.bVehicle) { ++Knocked; if (bSeen) ++KnockedSeen; }
			else
			{
				++Falls;
				if (bSeen) ++FallsSeen;
				if (Far < 150.0) ++FallsNear;
			}
		}
		if (Pose.bCut)
		{
			if (Real - LastCutReal < ShortestShot && LastCutReal > 0.0) ShortestShot = Real - LastCutReal;
			LastCutReal = Real;
			char Line[160];
			std::snprintf(Line, sizeof(Line), "%6.1f %-8s subject %d", Battle, LandEye::NameOf(Pose.Shot), Pose.Subject);
			CutLines.push_back(Line);
		}
		ShotSeconds[LandEye::NameOf(Pose.Shot)] += Dt;
		const double Under = Pose.Eye.Z - Rec.Ground.HeightAt(Pose.Eye.X, Pose.Eye.Y);
		LowestEye = std::min(LowestEye, Under);
		if (Under < 0.2) Fault("the eye is under the ground", Battle);
		if (std::isnan(Pose.Eye.X) || std::isnan(Pose.Look.X)) Fault("the eye at no number", Battle);
		MeterMax = std::max(MeterMax, Mix.Meter());
		if (Battle > Contact) { MeterSum += Mix.Meter(); ++MeterN; }
		if (Real >= NextHeatSample) { HeatSamples.emplace_back(Battle, Mix.Heat()); MeterSamples.emplace_back(Battle, Mix.Meter()); NextHeatSample += 0.25; }
		BlockHeat += 3.0 * Fighting * Dt / 6.0;
	}

	// Every round fired once and landed once (the replay's own count: the cursor passes each).
	if (Play.RoundsPlayed() != static_cast<int>(Rec.Rounds.size())) Fault("not every round was fired");
	int ShellEvents = 0;
	for (const LandFeed::Event& E : Rec.Events) if (E.What == "shell") ++ShellEvents;
	if (Play.BlastsPaired() + Play.BlastsAlone() != ShellEvents) Fault("a 'shell' event was not one blast");
	const int ShotNoises = NoiseCount["shot"];
	if (ShotNoises != static_cast<int>(Rec.Rounds.size())) Fault("a round had no shot to hear");
	if (Mix.ShotsOnScreenDropped > 0) Fault("a shot on the screen was dropped");
	if (SameTakeTooSoon > 0) Fault("the same take started twice within 0.15 s");
	if (ShortestShot < 3.0) Fault("the camera cut within 3 s");
	if (std::abs(KnotStart - 0.5) > 0.06) Fault("the bar did not start level");
	const bool bAttWon = Rec.Result.Winner == "att";
	if ((KnotEnd > 0.5) != bAttWon) Fault("the bar did not end on the winner's side");

	std::printf("rounds: %d fired, %d shell events (%d one blast with their kill round, %d alone)\n", Play.RoundsPlayed(), ShellEvents, Play.BlastsPaired(), Play.BlastsAlone());
	std::printf("effects:");
	for (auto& P : FxCount) std::printf(" %s %d", P.first.c_str(), P.second);
	std::printf("\nsound: %d shots, %d taken, %d out of hearing, %d dropped by the limits (%d on the screen), %d cracks overhead, %d landings, %d falls; %d takes moved for the 0.15 s rule\n",
		Mix.ShotsHeard, Mix.ShotTakes, Mix.ShotsOutOfHearing, Mix.ShotsDropped, Mix.ShotsOnScreenDropped, Mix.Cracks, Mix.Landings, Mix.Falls, Mix.Delayed);
	std::printf("land: %d bursts cut to their rounds, %d runs of cracks, %d rounds struck the ground near the ear (%d whined off), %d Garand clips pinged (%d reloads), %d shouts (%d voice lines in all), %d shells heard coming in; the longest a take was held for the 0.15 s rule %.2f s, %d started on time at another pitch instead\n",
		Mix.BurstsCut, Mix.CrackRuns, Mix.GroundStrikes, Mix.Whines, Mix.Clips, Mix.Reloads, Mix.Shouts, VoiceLines, Mix.Incoming, Mix.MostDelay, Mix.Repitched);
	if (Mix.MostDelay > 0.1) Fault("a take was held more than 0.1 s by the same-take rule (heard after its flash)");
	std::printf("bursts: the most any cut MG burst plays past its last round %.3f s (0 or less: no extra rounds)\n", Mix.MostBurstOver);
	{
		// The GI's clip cues: a reload is two pieces of garand_reload (clip in now, the rod slamming 0.246 s later, the
		// slam's onset at 2.744 s into the file landing 0.33 s after the cue), and the squad leader's Thompson is 4
		// rounds and no more.
		LandSound::Listener Near;
		Near.At = Vec(0.0, 0.0, 1.5);
		Near.CameraHeight = 1.5;
		std::vector<LandSound::Take> Cue;
		Mix.HearBody("garand_reload", "USA", Vec(5.0, 0.0, 1.0), 10000.0, Near, Cue);
		const bool bReload = Cue.size() == 2 && Cue[0].Name == "garand_reload" && Cue[1].Name == "garand_reload"
			&& std::abs(Cue[1].Delay + (2.744 - Cue[1].From) - 0.33) < 0.02 && Cue[0].Delay < 0.02;
		Cue.clear();
		Mix.HearBody("thompson_burst", "USA", Vec(5.0, 0.0, 1.0), 10010.0, Near, Cue);
		bool bSmg = Cue.size() == 1;
		if (bSmg)
		{
			const LandSound::Take& B = Cue[0];
			const double Fifth = B.Name == "mp40_burst_01" ? 0.382 : 0.75;
			// (mp40_burst_02 is four rounds as recorded: played whole, Cut 0)
			const double Ends = B.Cut > 0.0 ? B.From + B.Cut + B.CutFade : Fifth;
			bSmg = B.Family == "small_arms" && Ends <= Fifth + 1e-6 && Ends > 0.292;
		}
		// After his first burst, a USA leader's rifle round plays no take (his clip carries it); a rifleman's still does.
		LandPlay::Noise Lead;
		Lead.Kind = LandPlay::NoiseKind::Shot;
		Lead.At = Vec(8.0, 0.0, 1.2);
		Lead.From = Lead.At;
		Lead.To = Vec(300.0, 0.0, 0.0);
		Lead.Weapon = "rifle";
		Lead.Army = "USA";
		Lead.Role = "officer";
		Lead.Track = -1;
		Cue.clear();
		Mix.Hear(Lead, 10020.0, 10020.0, 1.0, Near, Cue);
		const bool bLeaderQuiet = std::none_of(Cue.begin(), Cue.end(), [](const LandSound::Take& K) { return K.Family == "small_arms"; });
		Lead.Role = "rifle";
		Cue.clear();
		Mix.Hear(Lead, 10030.0, 10030.0, 1.0, Near, Cue);
		const bool bRiflemanHeard = !Cue.empty();
		// (a vehicle cue with no recording: the mix names it silent, so nothing missing is asked for)
		if (!Mix.CueTake("field_gun_breech").empty() && !Mix.Length("field_gun_breech")) Fault("the field gun's breech cue asks for a take that is not in Sounds/Real");
		if (!bLeaderQuiet) Fault("a squad leader's round played a rifle take on top of his Thompson burst");
		if (!bRiflemanHeard) Fault("a rifleman's round after the Thompson burst was not heard");
		std::printf("clip cues: reload %s (%zu pieces), Thompson burst %s\n", bReload ? "in step" : "WRONG", static_cast<size_t>(2), bSmg ? "4 rounds" : "WRONG");
		if (!bReload) Fault("the GI's garand_reload cue did not give the two reload pieces in step with his hands");
		if (!bSmg) Fault("the squad leader's thompson_burst cue did not give one 4-round burst");
	}
	if (Mix.MostBurstOver > 1e-6) Fault("a cut MG burst plays past its last round (more rounds heard than fired)");
	if (!Missing.empty())
	{
		std::printf("takes the game has no recording of (silent):");
		for (const std::string& M : Missing) std::printf(" %s", M.c_str());
		std::printf("\n");
	}
	std::printf("takes:");
	int Shown = 0;
	std::vector<std::pair<int, std::string>> ByCount;
	for (auto& P : Mix.TakesByName) ByCount.emplace_back(P.second, P.first);
	std::sort(ByCount.rbegin(), ByCount.rend());
	for (auto& P : ByCount) { if (Shown++ < 18) std::printf(" %s %d", P.second.c_str(), P.first); }
	std::printf("\nmeter: highest %.2f, mean after contact %.2f\n", MeterMax, MeterN ? MeterSum / MeterN : 0.0);
	{
		auto Percentiles = [](std::vector<double> V, const char* What)
		{
			if (V.empty()) return;
			std::sort(V.begin(), V.end());
			auto At = [&](double P) { return V[std::min(V.size() - 1, static_cast<size_t>(P * (V.size() - 1) + 0.5))]; };
			std::printf("heat %s (%zu samples): p10 %.1f, p50 %.1f, p90 %.1f, p99 %.1f, most %.1f\n", What, V.size(), At(0.10), At(0.50), At(0.90), At(0.99), V.back());
		};
		std::vector<double> All, After, Before;
		for (auto& H : HeatSamples) { All.push_back(H.second); (H.first >= Contact ? After : Before).push_back(H.second); }
		Percentiles(All, "the whole run");
		Percentiles(Before, "before contact");
		Percentiles(After, "after contact");
		// The quietest 10 s after contact (40 samples), and the busiest, as means.
		double Quiet = 1e9, QuietAt = 0.0, Busy = -1.0, BusyAt = 0.0;
		for (size_t I = 0; I + 40 <= HeatSamples.size(); ++I)
		{
			if (HeatSamples[I].first < Contact) continue;
			double Sum = 0.0;
			for (size_t J = I; J < I + 40; ++J) Sum += HeatSamples[J].second;
			if (Sum / 40 < Quiet) { Quiet = Sum / 40; QuietAt = HeatSamples[I].first; }
			if (Sum / 40 > Busy) { Busy = Sum / 40; BusyAt = HeatSamples[I].first; }
		}
		std::printf("heat: the run %.0f s of battle, %.0f s real, contact at %.0f s; quietest 10 s after contact %.1f (from %.0f s), busiest %.1f (from %.0f s)\n",
			Battle, Real, Contact, Quiet, QuietAt, Busy, BusyAt);
		// Each key's share of the heat (each noise stays in the 6 s window, so a block fighting for 6 s weighs as one noise).
		double Sum = BlockHeat;
		for (auto& P : Mix.HeatBy) Sum += P.second;
		// The music's marks: its thick-of-it drop over 0.6, its lull under 0.3.
		int Over = 0, Under = 0, OverAfter = 0, UnderAfter = 0, AfterN = 0;
		for (auto& M : MeterSamples)
		{
			Over += M.second > 0.6;
			Under += M.second < 0.3;
			if (M.first >= Contact) { ++AfterN; OverAfter += M.second > 0.6; UnderAfter += M.second < 0.3; }
		}
		const double N = static_cast<double>(std::max<size_t>(1, MeterSamples.size()));
		std::printf("meter time: over 0.6 %.0f%%, under 0.3 %.0f%% of the run; after contact over 0.6 %.0f%%, under 0.3 %.0f%%\n",
			100.0 * Over / N, 100.0 * Under / N, 100.0 * OverAfter / std::max(1, AfterN), 100.0 * UnderAfter / std::max(1, AfterN));
		std::printf("heat by kind:");
		for (auto& P : Mix.HeatBy) std::printf(" %s %.0f%%", P.first.c_str(), 100.0 * P.second / std::max(1e-9, Sum));
		std::printf(" blocks fighting %.0f%%\n", 100.0 * BlockHeat / std::max(1e-9, Sum));
	}
	std::printf("camera: %d cuts, shortest shot %.1f s, lowest eye %.2f m over the ground; time in each shot:", Eye.Cuts(), ShortestShot, LowestEye);
	for (auto& P : ShotSeconds) std::printf(" %s %.0f s", P.first.c_str(), P.second);
	std::printf("\nfalls: %d men, %d in the picture as they fell, %d within 150 m of the eye; tanks knocked out %d, %d in the picture\n", Falls, FallsSeen, FallsNear, Knocked, KnockedSeen);
	std::printf("bar: starts %.3f, ends %.3f (%s won)\n", KnotStart, KnotEnd, Rec.Result.Winner.c_str());
	std::printf("cores: %.3f ms a frame on average, %.2f ms the slowest, over %d frames\n", Total / std::max(1, Frames), Slowest, Frames);
	if (argc > 3)
	{
		std::ofstream Log(std::string(argv[3]) + "/" + Rec.Key + "-cuts.txt");
		for (const std::string& L : CutLines) Log << L << "\n";
		// Every take the battle started, and how often (which recordings the game must carry in Sounds/Real).
		std::ofstream Heard(std::string(argv[3]) + "/" + Rec.Key + "-takes.txt");
		for (auto& P : ByCount) Heard << P.second << " " << P.first << "\n";
	}
	std::printf("%s\n", Faults == 0 ? "PASS" : ("FAIL: " + std::to_string(Faults) + " faults").c_str());
	return Faults == 0 ? 0 : 1;
}
