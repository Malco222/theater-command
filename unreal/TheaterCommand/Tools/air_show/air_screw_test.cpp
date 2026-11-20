// The airscrew test: every sculpted aircraft in Content/Models has its airscrews found where Tools/build_plane.py put
// them (Battle/AirScrew), each with the blades it was given, and every blade triangle in one of them. Usage:
// air_screw_test <model.json ...>; a model that is not an aircraft is passed over.
#include "Battle/AirScrew.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	std::vector<double> NearNumbers(const std::string& Text, const char* Key)
	{
		std::vector<double> Out;
		const size_t Near = Text.find("\"near\"");
		const size_t At = Near == std::string::npos ? std::string::npos : Text.find(Key, Near);
		const size_t Open = At == std::string::npos ? std::string::npos : Text.find('[', At);
		const size_t Close = Open == std::string::npos ? std::string::npos : Text.find(']', Open);
		if (Close == std::string::npos) return Out;
		const char* P = Text.c_str() + Open + 1;
		const char* End = Text.c_str() + Close;
		while (P < End)
		{
			char* Next = nullptr;
			const double V = std::strtod(P, &Next);
			if (Next == P) { ++P; continue; }
			Out.push_back(V);
			P = Next;
		}
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	int Bad = 0, Models = 0;
	for (int A = 1; A < Argc; ++A)
	{
		std::ifstream In(Argv[A]);
		std::stringstream Buf;
		Buf << In.rdbuf();
		const std::string Text = Buf.str();
		if (Text.find("\"kind\": \"aircraft\"") == std::string::npos && Text.find("\"kind\":\"aircraft\"") == std::string::npos) continue;
		++Models;
		const std::vector<double> Flat = NearNumbers(Text, "\"points\"");
		const std::vector<double> Tri = NearNumbers(Text, "\"triangles\"");
		const std::vector<int> Paints = AirScrew::NearPaints(Text);
		std::vector<AirScrew::Vec3> Points;
		for (size_t K = 0; K + 2 < Flat.size(); K += 3) Points.push_back({ Flat[K], Flat[K + 1], Flat[K + 2] });
		std::vector<bool> Blade(Paints.size());
		int BladePoints = 0;
		for (size_t K = 0; K < Paints.size(); ++K) { Blade[K] = Paints[K] == AirScrew::BladePaint; BladePoints += Blade[K] ? 1 : 0; }
		std::vector<int> Triangles;
		for (double V : Tri) Triangles.push_back(static_cast<int>(V));
		const AirScrew::Found F = AirScrew::Find(Points, Blade, Triangles);
		int BladeTris = 0, Taken = 0;
		for (size_t T = 0; T + 2 < Triangles.size(); T += 3)
		{
			const bool bAll = Blade[static_cast<size_t>(Triangles[T])] && Blade[static_cast<size_t>(Triangles[T + 1])] && Blade[static_cast<size_t>(Triangles[T + 2])];
			BladeTris += bAll ? 1 : 0;
			Taken += F.BladeTriangle[T / 3] ? 1 : 0;
		}
		bool bOk = Points.size() == Paints.size() && BladePoints > 0 && !F.Screws.empty() && Taken == BladeTris;
		std::printf("%s: %d blade points, %zu screws, %d of %d blade triangles taken\n", Argv[A], BladePoints, F.Screws.size(), Taken, BladeTris);
		for (const AirScrew::Screw& S : F.Screws)
		{
			std::printf("   hub (%.2f, %.2f, %.2f) axis (%.2f, %.2f, %.2f) radius %.2f, %d blades\n", S.Hub.X, S.Hub.Y, S.Hub.Z, S.Axis.X, S.Axis.Y, S.Axis.Z, S.Radius, S.Blades);
			// (a shaft along the machine, x forward in the file; a hub on the machine's middle line or paired across it)
			if (std::fabs(S.Axis.X) < 0.98 || S.Blades < 2 || S.Blades > 4 || S.Radius < 0.9 || S.Radius > 2.4) bOk = false;
		}
		if (F.Screws.size() != 1 && F.Screws.size() != 2 && F.Screws.size() != 4) bOk = false;
		if (!bOk) { ++Bad; std::printf("  FAIL\n"); }
	}
	std::printf("%d aircraft, %d failed\n", Models, Bad);
	return Bad == 0 && Models > 0 ? 0 : 1;
}
