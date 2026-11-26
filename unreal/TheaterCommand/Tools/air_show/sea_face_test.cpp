// The sea's face (Battle/SeaFace): the shore picture each stage's moving sea reads, made from the stage's own land,
// reef and works as AAirStage makes it, and the whitecaps' picture. Checks where the water lies calm, where the trade
// wind raises whitecaps and where the surf lies, and writes the pictures (raw RGBA, a line of text first) for the
// stand-in frames. Usage: sea_face_test <Content/Data/Stages> <out folder>
#include "Battle/AirFeed.h"
#include "Battle/SeaFace.h"

#include <algorithm>
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
		std::printf("  %s %s\n", bOk ? "ok  " : "FAIL", What.c_str());
		Failures += bOk ? 0 : 1;
	}

	bool ReadStage(const std::string& Path, std::vector<SeaFace::Part>& Parts)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In)
		{
			return false;
		}
		std::stringstream Text;
		Text << In.rdbuf();
		AirFeed::Json Root;
		std::string Error;
		if (!AirFeed::Parse(Text.str(), Root, Error))
		{
			std::printf("  %s: %s\n", Path.c_str(), Error.c_str());
			return false;
		}
		const AirFeed::Json& Meshes = Root["meshes"];
		for (size_t M = 0; M < Meshes.Count(); ++M)
		{
			const AirFeed::Json& Mesh = Meshes.At(M);
			const std::string Name = Mesh["name"].Str("");
			// (as AAirStage gives them: the land, the reef and the works; not the shallows, which are water)
			if (Name != "land" && Name != "reef" && Name != "works")
			{
				continue;
			}
			SeaFace::Part Part;
			const AirFeed::Json& V = Mesh["v"];
			for (size_t K = 0; K + 2 < V.Count(); K += 3)
			{
				Part.X.push_back(V.At(K).Num() * 0.1);
				Part.Y.push_back(V.At(K + 1).Num() * 0.1);
			}
			const AirFeed::Json& T = Mesh["t"];
			for (size_t K = 0; K < T.Count(); ++K)
			{
				Part.Triangles.push_back(T.At(K).Int());
			}
			Parts.push_back(std::move(Part));
		}
		return true;
	}

	void Write(const std::string& Path, const std::string& Head, const std::vector<uint8_t>& Pixels)
	{
		std::ofstream Out(Path, std::ios::binary);
		Out << Head << "\n";
		Out.write(reinterpret_cast<const char*>(Pixels.data()), static_cast<std::streamsize>(Pixels.size()));
	}

	/** The shore picture's four numbers at a stage point (nearest pixel). */
	struct Here
	{
		double Edge = 0.0, Caps = 0.0, Open = 0.0, Shut = 0.0;
	};
	Here At(const SeaFace::Shore& S, double X, double Y)
	{
		const int I = std::clamp(static_cast<int>((X - S.West) / S.Span * S.Size), 0, S.Size - 1);
		const int J = std::clamp(static_cast<int>((Y - S.South) / S.Span * S.Size), 0, S.Size - 1);
		const uint8_t* P = &S.Pixels[(static_cast<size_t>(J) * S.Size + I) * 4];
		return { (P[0] / 255.0 - 0.5) * 2.0 * SeaFace::EdgeReach, P[1] / 255.0, P[2] / 255.0, P[3] / 255.0 };
	}

	/** The mean of one of the numbers over the water pixels in a box (stage metres) whose distance from the edge lies in [Near, FarOut]. */
	double Mean(const SeaFace::Shore& S, double X0, double Y0, double X1, double Y1, double Near, double FarOut, int Which, int& Count)
	{
		double Sum = 0.0;
		Count = 0;
		const double Pixel = S.Span / S.Size;
		for (double Y = Y0; Y <= Y1; Y += Pixel)
		{
			for (double X = X0; X <= X1; X += Pixel)
			{
				const Here H = At(S, X, Y);
				if (H.Edge < Near || H.Edge > FarOut)
				{
					continue;
				}
				Sum += Which == 1 ? H.Caps : Which == 2 ? H.Open : H.Shut;
				++Count;
			}
		}
		return Count ? Sum / Count : 0.0;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: sea_face_test <Content/Data/Stages> <out folder>\n");
		return 2;
	}
	const std::string Stages = Argv[1], Out = Argv[2];
	char Line[256];
	for (const char* Key : { "oahu", "midway" })
	{
		std::vector<SeaFace::Part> Parts;
		if (!ReadStage(Stages + "/" + Key + ".json", Parts))
		{
			std::printf("%s: not read\n", Key);
			++Failures;
			continue;
		}
		const auto Began = std::chrono::steady_clock::now();
		const SeaFace::Shore S = SeaFace::MakeShore(Parts, 70.0);
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Began).count();
		std::printf("%s: shore picture %d a side over %.1f km, from %.0f,%.0f, made in %.0f ms\n", Key, S.Size, S.Span / 1000.0, S.West, S.South, Ms);
		std::snprintf(Line, sizeof(Line), "shore %d %.3f %.3f %.3f", S.Size, S.West, S.South, S.Span);
		Write(Out + "/shore-" + Key + ".rgba", Line, S.Pixels);
		int N = 0;
		if (std::string(Key) == "oahu")
		{
			// Pearl Harbor's lochs: the water within 4 km of the stage's middle (Ford Island) lies shut in, closed to the
			// ocean, and raises no whitecaps.
			const double Shut = Mean(S, -4000, -2500, 4000, 4000, 40, 1e9, 3, N);
			Check(N > 200 && Shut > 0.8, "Pearl Harbor's lochs lie shut in: " + std::to_string(Shut) + " over " + std::to_string(N) + " pixels");
			const double Caps = Mean(S, -4000, -2500, 4000, 4000, 40, 1e9, 1, N);
			Check(Caps < 0.05, "no whitecaps in the lochs: " + std::to_string(Caps));
			const double Open = Mean(S, -4000, -2500, 4000, 4000, 0, 60, 2, N);
			Check(Open < 0.3, "no surf on the lochs' shores: " + std::to_string(Open));
			// The south shore in the trade wind's lee, from Barbers Point to Diamond Head: few whitecaps within 2 km of
			// its edge (the reef's outer edge where it has one), all of them 8 to 10 km out; the edge open to the ocean.
			double Close = 0.0, Out8 = 0.0, Surf = 0.0;
			int Lines = 0;
			for (double X = -14000.0; X <= 10000.0; X += 2000.0)
			{
				double Coast = 0.0;
				for (double Y = -16000.0; Y < -3000.0; Y += 20.0)
				{
					if (At(S, X, Y).Edge < 0.0)
					{
						Coast = Y;
						break;
					}
				}
				if (Coast == 0.0)
				{
					continue;
				}
				++Lines;
				Close += 0.5 * (At(S, X, Coast - 1000.0).Caps + At(S, X, Coast - 2000.0).Caps);
				Out8 += 0.5 * (At(S, X, Coast - 8000.0).Caps + At(S, X, Coast - 10000.0).Caps);
				Surf += At(S, X, Coast - 40.0).Open;
			}
			Lines = std::max(Lines, 1);
			Check(Lines >= 10 && Close / Lines < 0.2, "few whitecaps within 2 km of the south shore: " + std::to_string(Close / Lines) + " on " + std::to_string(Lines) + " lines");
			Check(Out8 / Lines > 0.75, "the full sea 8 to 10 km off it: " + std::to_string(Out8 / Lines));
			Check(Surf / Lines > 0.5, "the south shore's edge lies open to the ocean (its surf): " + std::to_string(Surf / Lines));
			const double Sea = At(S, 0, -25000).Caps;
			Check(Sea > 0.9 && At(S, 0, -25000).Shut < 0.05, "the open sea 25 km south: whitecaps " + std::to_string(Sea));
		}
		else
		{
			// Midway: the lagoon inside the reef is calm, the reef's rim open to the ocean, the sea round it whitecapped.
			const Here Middle = At(S, 0, 0);
			Check(Middle.Shut > 0.95 && Middle.Caps < 0.05, "Midway's lagoon lies calm: shut " + std::to_string(Middle.Shut));
			const double Rim = Mean(S, -9000, -9000, 9000, 9000, -30, 30, 2, N);
			Check(N > 100 && Rim > 0.6, "the reef's rim lies open to the ocean: " + std::to_string(Rim));
			const double Windward = 0.5 * (At(S, 8000, 6000).Caps + At(S, 9000, 2000).Caps);
			Check(Windward > 0.8, "the sea to windward is whitecapped: " + std::to_string(Windward));
		}
	}
	const auto Began = std::chrono::steady_clock::now();
	const std::vector<uint8_t> Foam = SeaFace::MakeFoam(512, 160.0);
	const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Began).count();
	double Covered = 0.0;
	for (size_t P = 0; P < Foam.size(); P += 4)
	{
		Covered += Foam[P] / 255.0;
	}
	Covered /= Foam.size() / 4;
	std::printf("whitecaps' picture: 512 a side, made in %.0f ms, foam over %.2f%% of it\n", Ms, Covered * 100.0);
	Check(Covered > 0.08 && Covered < 0.24, "the whitecaps cover 8 to 24% of their picture (each shows for part of its cycle)");
	// No seams: wherever a read between two neighbouring pixels takes any foam, both pixels hold one whitecap's time
	// (two whitecaps meeting, or one beside the empty sea's 0, lit a thin white line round it in v3's close frames).
	int Seams = 0;
	for (int J = 0; J < 512; ++J)
	{
		for (int I = 0; I < 512; ++I)
		{
			const size_t P = (static_cast<size_t>(J) * 512 + I) * 4;
			for (const size_t Q : { (static_cast<size_t>(J) * 512 + (I + 1) % 512) * 4, (static_cast<size_t>((J + 1) % 512) * 512 + I) * 4 })
			{
				if ((Foam[P] > 0 || Foam[Q] > 0) && Foam[P + 1] != Foam[Q + 1])
				{
					++Seams;
				}
			}
		}
	}
	Check(Seams == 0, "no seams where a read mixes two whitecaps' times: " + std::to_string(Seams));
	Write(Out + "/foam.rgba", "foam 512 160", Foam);
	std::printf("%s\n", Failures ? "SEA FACE: FAILED" : "sea face: all checks pass");
	return Failures ? 1 : 0;
}
