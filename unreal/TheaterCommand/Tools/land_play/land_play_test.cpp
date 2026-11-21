// The ground battle's replay, sound and camera cores played through a whole recorded battle at 60 frames a second,
// with no engine (Battle/LandFeed, LandPlay, LandSound, LandEye, LandDesk): what the game plays, counted.
//
//   land_play_test <recording.json> <mix.json> [out folder]
//
// It checks: every round fired once and landed once; every 'shell' event one blast (paired with its kill round where
// the rules fired one); every shot heard has a take, none on the screen dropped; the same take never twice within
// 0.15 s; the eye never under the ground, never cutting faster than every 3 s, and a lens near the ground never down
// among standing corn or tall grass (a stand-in for the field's corn: the rules' 'field' cells a metre high, 'rough'
// 0.8 m); the bar starts level and ends on the winner's side. It lays a shell hole for every shell that lands in the
// ground (LandCrater) and checks them: torn earth, brown, a rim over the ground, each round's size, the cost. It reports
// how many of the falls the camera saw, and the cores' time a frame.

#include "Battle/LandCrater.h"
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
	// (the stand-in for the field's corn: LandField's own is laid only in the game)
	auto CornAt = [&Rec](double X, double Y)
	{
		const std::string& G = Rec.Ground.GroundAt(X, Y);
		return G == "field" ? 1.05 : G == "rough" ? 0.8 : 0.0;
	};
	EW.Crop = CornAt;
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
	// The lens near the ground over corn: frames, and how far over the ears it was at the lowest.
	int LowOverCorn = 0, LowInCorn = 0;
	double LeastOverEars = 1e9;
	// The shell holes, laid as the game lays them (LandBattle's MakeCraters): every shell, bomb and grenade that lands
	// in the ground, none on armour.
	LandCrater::Field Holes;
	Holes.Begin([&Rec](double X, double Y) { return Rec.Ground.HeightAt(X, Y); }, Rec.Seed ? Rec.Seed : 1);
	std::map<std::string, int> HolesBy;
	double HoleMs = 0.0, HoleMsMost = 0.0;
	// What the game sends again for them in a frame: the most points, the most meshes, the widest mesh (metres).
	size_t HoleSentMost = 0;
	int HoleSentMeshesMost = 0, HolePatchesSeen = 0;
	double HoleMeshWidest = 0.0;

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
			if ((E.Kind == LandPlay::FxKind::Shell || E.Kind == LandPlay::FxKind::Size) && !E.bArmour)
			{
				auto H0 = std::chrono::steady_clock::now();
				if (Holes.Add(E.At.X, E.At.Y, E.Name, E.CalibreMm, E.Ground.empty() ? std::string("earth") : E.Ground)) ++HolesBy[E.Name];
				const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - H0).count();
				HoleMs += Ms;
				HoleMsMost = std::max(HoleMsMost, Ms);
			}
			static const char* Names[] = { "muzzle", "shots", "shell", "size", "skip", "glance", "plate" };
			++FxCount[Names[static_cast<int>(E.Kind)]];
			if (std::isnan(E.At.X) || std::isnan(E.At.Y) || std::isnan(E.At.Z)) Fault("an effect at no number", Battle);
		}
		// What the game sends again this frame (LandBattle's MakeCraters): each changed section's patch is one mesh, and
		// the engine copies all of that mesh's sections and redraws the shadows under its bounds. Count those points
		// and how wide the widest such mesh is: a patch, never the whole field.
		{
			const std::vector<LandCrater::Field::Place> Sent = Holes.TakeChanged();
			std::vector<int> Meshes;
			for (const LandCrater::Field::Place& Where : Sent)
			{
				if (Where.Patch < 0 || Where.Patch >= Holes.Patches() || Where.Section < 0 || Where.Section >= static_cast<int>(Holes.PatchAt(Where.Patch).Sections.size()))
				{
					Fault("a shell hole's section is sent to no patch", Battle);
					continue;
				}
				if (Where.Patch > HolePatchesSeen) Fault("a shell hole patch is numbered past the next", Battle);
				HolePatchesSeen = std::max(HolePatchesSeen, Where.Patch + 1);
				if (std::find(Meshes.begin(), Meshes.end(), Where.Patch) == Meshes.end()) Meshes.push_back(Where.Patch);
			}
			size_t Points = 0;
			for (const int K : Meshes)
			{
				const LandCrater::Field::Patch& Pa = Holes.PatchAt(K);
				double X0 = 1e18, X1 = -1e18, Y0 = 1e18, Y1 = -1e18;
				for (const LandField::Mesh& M : Pa.Sections)
				{
					Points += M.Points();
					for (size_t I = 0; I < M.Points(); ++I)
					{
						X0 = std::min(X0, static_cast<double>(M.Positions[I * 3]));
						X1 = std::max(X1, static_cast<double>(M.Positions[I * 3]));
						Y0 = std::min(Y0, static_cast<double>(M.Positions[I * 3 + 1]));
						Y1 = std::max(Y1, static_cast<double>(M.Positions[I * 3 + 1]));
					}
				}
				if (X1 >= X0) HoleMeshWidest = std::max(HoleMeshWidest, std::max(X1 - X0, Y1 - Y0));
			}
			HoleSentMost = std::max(HoleSentMost, Points);
			HoleSentMeshesMost = std::max(HoleSentMeshesMost, static_cast<int>(Meshes.size()));
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
		if (Pose.Shot == LandEye::Kind::Low || Pose.Shot == LandEye::Kind::Shoulder || Pose.Shot == LandEye::Kind::Tank)
		{
			const double Corn = CornAt(Pose.Eye.X, Pose.Eye.Y);
			if (Corn > 0.0)
			{
				const double OverEars = Under - Corn;
				if (Pose.Shot == LandEye::Kind::Low) ++LowOverCorn;
				LeastOverEars = std::min(LeastOverEars, OverEars);
				if (OverEars < 0.05)
				{
					++LowInCorn;
					Fault("a lens near the ground is down among the corn", Battle);
				}
			}
		}
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
	std::printf("\ncorn: %d frames of the low shot over corn, the lens near the ground at least %.2f m over the ears (%d frames among the stalks)\n", LowOverCorn, LeastOverEars < 1e8 ? LeastOverEars : 0.0, LowInCorn);
	// The picture run's low shot asked of a man in the corn (as the art director's 04 asks one): it keeps its kind and
	// is held just over the ears, never among the stalks.
	{
		int InCorn = -1;
		double AskAt = 0.0;
		for (size_t K = 0; K < Rec.Tracks.size() && InCorn < 0; ++K)
		{
			const LandFeed::Track& T = Rec.Tracks[K];
			if (T.bTwin || !T.IsMan() || T.Role == "mortar" || T.Role == "fo") continue;
			for (double When = std::max(T.Start(), Contact); When < Rec.End - 6.0; When += 5.0)
			{
				if (T.Fell >= 0.0 && T.Fell < When + 6.0) break;
				const LandFeed::Spot Sp = LandFeed::SpotAt(T, When);
				if (CornAt(Sp.X, Sp.Y) > 0.5) { InCorn = static_cast<int>(K); AskAt = When; break; }
			}
		}
		if (InCorn >= 0)
		{
			LandEye::Director Asked;
			Asked.Begin(Rec, EW, ES);
			int Seen = 0, Among = 0;
			double Least = 1e9;
			for (int F = 0; F < 4 * 60; ++F)
			{
				if (F == 0) Asked.Ask(LandEye::Kind::Low, InCorn);
				const LandEye::Pose P = Asked.Update(AskAt + F * Dt, 1000.0 + F * Dt, Dt);
				if (P.Shot != LandEye::Kind::Low) continue;
				const double Corn = CornAt(P.Eye.X, P.Eye.Y);
				if (Corn <= 0.0) continue;
				++Seen;
				const double Over = P.Eye.Z - Rec.Ground.HeightAt(P.Eye.X, P.Eye.Y) - Corn;
				Least = std::min(Least, Over);
				if (Over < 0.05) ++Among;
			}
			std::printf("corn: a low shot asked of track %d in the corn at %.0f s: %d frames over corn, at least %.2f m over the ears\n", InCorn, AskAt, Seen, Seen ? Least : 0.0);
			if (Among > 0) Fault("an asked low shot is down among the corn");
			if (Seen == 0) Fault("an asked low shot on a man in the corn was not taken over the corn");
		}
	}
	// The shell holes: torn earth, brown, a rim standing over the ground, each round's size; and their cost.
	{
		size_t Points = 0, Tris = 0;
		int Bad = 0, NotBrown = 0, FlatRim = 0, Sections = 0, Astray = 0;
		std::vector<const LandField::Mesh*> Each;
		for (int K = 0; K < Holes.Patches(); ++K)
		{
			for (const LandField::Mesh& M : Holes.PatchAt(K).Sections)
			{
				Each.push_back(&M);
			}
		}
		for (const LandField::Mesh* Mp : Each)
		{
			const LandField::Mesh& M = *Mp;
			++Sections;
			Points += M.Points();
			Tris += M.Tris();
			for (size_t I = 0; I < M.Points(); ++I)
			{
				const float* P = &M.Positions[I * 3];
				const float* N = &M.Normals[I * 3];
				if (!std::isfinite(P[0]) || !std::isfinite(P[1]) || !std::isfinite(P[2]) || std::abs(std::sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]) - 1.0f) > 1e-3f) ++Bad;
				if (P[2] < Rec.Ground.HeightAt(P[0], P[1]) - 0.02) ++Bad; // (nothing sunk under the ground)
			}
			for (const uint32_t T : M.Triangles) if (T >= M.Points()) ++Bad;
			for (const LandField::Rgb8& C : M.Colours)
			{
				// (earth: red over green over blue, or the torn turf's olive; the damp hollow dark, but never a grey or a black)
				const bool bBrown = C.R >= C.B + 10 && C.G >= C.B + 6 && C.R + C.G + C.B >= 75;
				if (!bBrown) ++NotBrown;
			}
		}
		// each hole's crest over the ground: its highest point within its radius at least half its rim; its middle bare
		const std::vector<LandCrater::Field::Hole>& All = Holes.All();
		for (size_t Hk = 0; Hk < All.size(); ++Hk)
		{
			const LandCrater::Field::Hole& H = All[Hk];
			double Most = 0.0;
			const LandField::Mesh& M = Holes.Section(H.In);
			// (its middle in its patch's square: the patches are places, not the order the holes were laid in)
			const LandCrater::Field::Patch& Pa = Holes.PatchAt(H.In.Patch);
			if (std::floor(H.X / LandCrater::Field::PatchSize) != Pa.I || std::floor(H.Y / LandCrater::Field::PatchSize) != Pa.J) ++Astray;
			for (size_t I = 0; I < M.Points(); ++I)
			{
				const float* P = &M.Positions[I * 3];
				if (std::hypot(P[0] - H.X, P[1] - H.Y) < 1.15 * H.R) Most = std::max(Most, P[2] - Rec.Ground.HeightAt(P[0], P[1]));
			}
			if (Most < 0.5 * H.Rim) ++FlatRim;
			if (!Holes.Bare(H.X, H.Y)) ++Bad;
		}
		std::printf("holes: %d laid (", Holes.Count());
		for (auto& P : HolesBy) std::printf(" %s %d", P.first.c_str(), P.second);
		std::printf(" ), %zu points and %zu triangles in %d sections of %d patches (one mesh each); %.3f ms each on average, %.3f ms the most\n", Points, Tris, Sections, Holes.Patches(), HoleMs / std::max(1, Holes.Count()), HoleMsMost);
		std::printf("holes sent again in a frame: at most %zu points in %d meshes, the widest mesh %.1f m across\n", HoleSentMost, HoleSentMeshesMost, HoleMeshWidest);
		if (Astray > 0) Fault("a shell hole is kept in a patch its middle is not in");
		if (HolePatchesSeen != Holes.Patches()) Fault("a shell hole patch was never sent to the game");
		// (a hole's thrown soil and clods reach some 2.5 times its radius: a 150's 5 m past the square's edge)
		if (HoleMeshWidest > LandCrater::Field::PatchSize + 12.0) Fault("a shell hole mesh the game sends again is wider than its patch and the thrown soil round it");
		if (Bad > 0) Fault("a shell hole's mesh is broken (a point at no number, a normal not a unit, an index past its points, sunk under the ground, its middle not bare)");
		if (NotBrown > 0) Fault("a shell hole has a colour that is no earth's");
		if (FlatRim > 0) Fault("a shell hole's rim does not stand over the ground");
		if (Holes.Count() > 0 && Points > static_cast<size_t>(Holes.Count()) * 400) Fault("the shell holes cost more than 400 points each");
		// each round's size: a mortar's under a 75's under an 88's under a 105's
		const double Mortar = LandCrater::SizeOf("mortar", 81.0).Across, G75 = LandCrater::SizeOf("gun75", 75.0).Across, G88 = LandCrater::SizeOf("gun75", 88.0).Across, H105 = LandCrater::SizeOf("howitzer", 105.0).Across;
		if (!(Mortar < G75 && G75 < G88 && G88 < H105)) Fault("the shell holes are not sized by the round");
		if (LandCrater::SizeOf("grenade", 0.0).Across <= 0.0) Fault("a grenade leaves no scuff");
	}
	std::printf("falls: %d men, %d in the picture as they fell, %d within 150 m of the eye; tanks knocked out %d, %d in the picture\n", Falls, FallsSeen, FallsNear, Knocked, KnockedSeen);
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
