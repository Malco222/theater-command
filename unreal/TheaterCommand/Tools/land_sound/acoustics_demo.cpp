// Release 2's battle acoustics (Battle/LandSound: -TheaterLandSoundTravel, -TheaterLandSoundTails,
// -TheaterLandSoundEcho), one part at a time for a person's ear. A few small made-up scenes, a rifleman, a machine
// gun, a tank's gun, a section's rifles, each on the same made-up ground (a wood's edge 100 m to the north, a farmhouse
// 60 m to the east), are heard by the game's own sound core with the parts off and with them on, and what the core
// asks the scene to start is written as the land_play test writes its takes (E a frame's ear, T a take), for
// Tools/land_sound/render_clip.py to lay on a track from the recordings themselves. Nothing here is in the game: it
// is how Brian hears what each number in mix.json land.acoustics does before a Mac build is spent on it.
//
//   acoustics_demo <Content/Data/Sound/mix.json> <Sounds/Real> <out folder>
//   writes <out>/<scene>-off.txt and <out>/<scene>-on.txt, and prints each scene's takes.
#include "Battle/LandPlay.h"
#include "Battle/LandSound.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using LandPlay::Vec;

namespace
{
	double WavSeconds(const std::string& Path)
	{
		// (RIFF: the fmt chunk's byte rate, the data chunk's size; 1.5 s where there is no such recording)
		std::ifstream F(Path, std::ios::binary);
		if (!F) return 1.5;
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
			else if (std::string(Id, 4) == "data") return Rate > 0 ? static_cast<double>(Size) / Rate : 1.5;
			else F.seekg(static_cast<std::streamoff>(Size) + (Size & 1), std::ios::cur);
		}
		return 1.5;
	}

	struct Cue
	{
		double At;
		LandPlay::Noise What;
	};

	struct Scene
	{
		const char* Name;
		const char* Says;
		double Long;
		std::vector<Cue> Cues;
	};

	LandPlay::Noise Shot(const char* Weapon, const char* Army, const Vec& From, const Vec& To, int Rounds = 1, double PerSecond = 1.0)
	{
		LandPlay::Noise N;
		N.Kind = LandPlay::NoiseKind::Shot;
		N.Weapon = Weapon;
		N.Army = Army;
		N.At = N.From = From;
		N.To = To;
		N.Rounds = Rounds;
		N.PerSecond = PerSecond;
		N.bMiss = true;
		return N;
	}
}

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		std::printf("usage: acoustics_demo <mix.json> <Sounds/Real> <out folder>\n");
		return 2;
	}
	std::string MixText;
	{
		std::ifstream F(argv[1]);
		std::stringstream Whole;
		Whole << F.rdbuf();
		MixText = Whole.str();
	}
	const std::string Sounds = std::string(argv[2]) + "/", OutDir = std::string(argv[3]) + "/";
	std::map<std::string, double> Seconds;
	auto Length = [&Seconds, &Sounds](const std::string& Name)
	{
		auto Known = Seconds.find(Name);
		if (Known != Seconds.end()) return Known->second;
		return Seconds[Name] = WavSeconds(Sounds + Name + ".wav");
	};

	// The ear stands at the middle, a man's height up, looking north at the wood.
	std::vector<Scene> Scenes;
	{
		// (a rifleman 300 m off to the west, aiming at the ear: four rounds, each passing 3 m wide)
		Scene S{ "far-rifle", "A German rifleman 300 m to the west fires four aimed rounds that pass 3 m from the ear", 12.0, {} };
		for (int K = 0; K < 4; ++K) S.Cues.push_back({ 1.0 + K * 2.6, Shot("rifle", "GER", Vec(-300.0, 0.0, 1.5), Vec(60.0, 3.0, 1.5)) });
		Scenes.push_back(S);
	}
	{
		// (a machine gun 90 m to the south-west firing across the front: five bursts)
		Scene S{ "machine-gun", "An American machine gun 90 m to the south-west fires five bursts across the front", 13.0, {} };
		const int Rounds[] = { 6, 9, 5, 8, 7 };
		for (int K = 0; K < 5; ++K) S.Cues.push_back({ 1.0 + K * 2.3, Shot("mg", "USA", Vec(-60.0, -67.0, 1.2), Vec(240.0, -67.0, 1.2), Rounds[K], 8.0) });
		Scenes.push_back(S);
	}
	{
		// (a tank 150 m to the west fires her gun three times, away from the ear)
		Scene S{ "tank-gun", "A tank 150 m to the west fires her gun three times", 15.0, {} };
		for (int K = 0; K < 3; ++K) S.Cues.push_back({ 1.0 + K * 4.5, Shot("tank", "GER", Vec(-150.0, 0.0, 2.0), Vec(-150.0, -600.0, 2.0)) });
		Scenes.push_back(S);
	}
	{
		// (a section's rifles 30 to 70 m off to the south and west, firing away from the ear, a ragged dozen shots)
		Scene S{ "near-rifles", "A section's rifles 30 to 70 m off fire a ragged dozen shots, away from the ear", 14.0, {} };
		const double Where[][2] = { { -40.0, -20.0 }, { -55.0, -35.0 }, { -30.0, -45.0 }, { -65.0, -10.0 }, { -48.0, -52.0 } };
		const double When[] = { 1.0, 1.9, 2.5, 3.8, 4.4, 5.9, 6.3, 7.6, 8.8, 9.3, 10.6, 11.5 };
		for (int K = 0; K < 12; ++K)
		{
			const double* W = Where[(K * 3) % 5];
			S.Cues.push_back({ When[K], Shot("rifle", "USA", Vec(W[0], W[1], 1.5), Vec(W[0] - 200.0, W[1] - 300.0, 1.5)) });
		}
		Scenes.push_back(S);
	}

	for (const Scene& S : Scenes)
	{
		std::printf("%s: %s\n", S.Name, S.Says);
		for (int On = 0; On < 2; ++On)
		{
			LandSound::Mixer Mix;
			std::string Error;
			if (!Mix.Begin(MixText, 7, Error))
			{
				std::printf("cannot read the mix: %s\n", Error.c_str());
				return 2;
			}
			Mix.Length = Length;
			Mix.bTravel = Mix.bTails = Mix.bEcho = On == 1;
			Mix.CoverOf = [](double X, double Y, double& OutGroundZ)
			{
				OutGroundZ = 0.0;
				if (Y > 100.0 && Y < 400.0) return 1;
				if (X > 60.0 && X < 72.0 && std::fabs(Y) < 6.0) return 2;
				return 0;
			};
			LandSound::Listener Ear;
			Ear.At = Vec(0.0, 0.0, 1.6);
			Ear.CameraHeight = 1.6;
			const std::string Path = OutDir + S.Name + (On ? "-on.txt" : "-off.txt");
			std::FILE* Out = std::fopen(Path.c_str(), "w");
			if (!Out)
			{
				std::printf("cannot write %s\n", Path.c_str());
				return 2;
			}
			size_t Next = 0;
			const double Dt = 1.0 / 60.0;
			for (double Real = Dt; Real < S.Long; Real += Dt)
			{
				std::vector<LandSound::Take> Takes;
				while (Next < S.Cues.size() && S.Cues[Next].At <= Real)
				{
					LandPlay::Noise N = S.Cues[Next].What;
					N.T = Real;
					Mix.Hear(N, Real, Real, 1.0, Ear, Takes);
					++Next;
				}
				// (the eye a step behind the ear, looking north: the renderer's left and right)
				std::fprintf(Out, "E %.4f %.4f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.2f %.3f\n", Real, Real, 0.0, -1.0, 1.6, 0.0, 0.0, 1.6, Ear.At.X, Ear.At.Y, Ear.At.Z, Ear.CameraHeight, 1.0);
				for (const LandSound::Take& T : Takes)
				{
					std::fprintf(Out, "T %.4f %s %.2f %.2f %.2f %s %s %.3f %.5f %.4f %.4f %.4f %.4f %d %.0f %.0f %d %d\n", Real + std::max(0.0, T.Delay), T.Name.c_str(), T.At.X, T.At.Y, T.At.Z, T.Family.c_str(), T.Bus.c_str(),
						T.Db, T.Pitch, T.From, T.Cut, T.CutFade, T.FadeIn, T.bSpatial ? 1 : 0, T.LpfHz, T.HpfHz, T.bEcho ? 1 : 0, T.bBig ? 1 : 0);
					const double Far = (T.At - Ear.At).Length();
					std::printf("  %s %6.2f s  %-22s %s%s  from %4.0f m  %+6.1f dB at the ear%s\n", On ? "on " : "off", Real + std::max(0.0, T.Delay), T.Name.c_str(),
						T.bEcho ? "ECHO" : T.From > 0.2 && T.Cut <= 0.0 && T.Family == "mg" ? "TAIL" : "    ", T.Cut > 0.0 ? " (cut)" : "", Far,
						T.Db + (T.bSpatial ? Mix.LossDb(T.Family, Far) : 0.0), T.LpfHz > 0.0 ? ", dulled" : "");
				}
			}
			std::fclose(Out);
		}
	}
	return 0;
}
