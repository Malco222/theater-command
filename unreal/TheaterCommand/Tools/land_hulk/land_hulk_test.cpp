// Old wrecks' cloud test (release 2, -TheaterLandOldWrecks; Brian, 23 November 20:09Z: the burnt-out wrecks of a battle
// stay on the field for the next battle there). Lays them on each recording's field as LandBattle's LayOldWrecks does
// (LandHulk::FieldOf, the battle's own models from LandKit::Battle) and checks what the brief asks: the same twice;
// none on a lane, a bank, a house, a wall, a yard or garden, a wood, water, marsh or standing corn, every point of each
// read where it lies; 8 m and more from every recorded vehicle's way, 1.5 m from every man's; none on another; no more
// than the cap; fewer as the months go; each nation's share as the recording's nats give it; the recording's key read
// whatever is missing or odd in it; with no key and no switch, none at all. Then on an old field (two battles' scars
// and old ruts laid first, as the scene lays them): none on an old shell hole or an old rut. Every point of each scorched
// patch on open ground too; the grass kept out of every hull's footprint, her corners and all; each thrown turret clear
// of her own hull and run-off track, its gun not through her hull, its box for the camera on the ground. The battle's own
// models taken only for wrecks of its own month; the Mac's stand-in laying as many as it asks; none over a field
// telephone's wire; on a winter field the patch under the snow and snow on what faces up. The dressed meshes: every
// number finite, every corner a point, every point near the ground under it, the rust spreading as the months go. Times
// the laying and the dressing, and paints the field from above with the wrecks on it (placement_<recording>.ppm) and a
// sheet of four of them as the months go (look_<recording>.ppm), for the art director.
// Usage: land_hulk_test <Content> <out dir> <recording.json ...>
#include "Battle/Fates.h"
#include "Battle/LandCrater.h"
#include "Battle/LandFeed.h"
#include "Battle/LandField.h"
#include "Battle/LandHulk.h"
#include "Battle/LandJson.h"
#include "Battle/LandKit.h"
#include "Battle/LandSight.h"
// (LandScape's header is built with its own tests' flags, not -Wconversion: only its KitWanted is read here)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include "Battle/LandScape.h"
#pragma GCC diagnostic pop

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
	using LandKit::V3;
	using Line = std::vector<std::pair<double, double>>;

	int Faults = 0;
	void Fault(const std::string& Why)
	{
		std::printf("FAIL: %s\n", Why.c_str());
		++Faults;
	}

	double Since(std::chrono::steady_clock::time_point T0)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	}

	bool ReadText(const std::filesystem::path& Path, std::string& Out)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In) return false;
		std::stringstream Buf;
		Buf << In.rdbuf();
		Out = Buf.str();
		return true;
	}

	/** A mesh's hash (FNV-1a over its numbers), for the same-twice check. */
	uint64_t HashOf(const LandField::Mesh& M)
	{
		uint64_t H = 1469598103934665603ull;
		const auto Eat = [&H](const void* Data, size_t Bytes)
		{
			const unsigned char* P = static_cast<const unsigned char*>(Data);
			for (size_t I = 0; I < Bytes; ++I)
			{
				H ^= P[I];
				H *= 1099511628211ull;
			}
		};
		Eat(M.Positions.data(), M.Positions.size() * sizeof(float));
		Eat(M.Normals.data(), M.Normals.size() * sizeof(float));
		Eat(M.Colours.data(), M.Colours.size() * sizeof(LandField::Rgb8));
		Eat(M.Triangles.data(), M.Triangles.size() * sizeof(uint32_t));
		return H;
	}

	/** Points of lines on the field, every metre, by 4 m squares: the distance from a place to the nearest. */
	class Near
	{
	public:
		void Add(double X, double Y) { Of[KeyOf(Sq(X), Sq(Y))].push_back({ X, Y }); }
		void AddLine(const Line& L, double Step)
		{
			if (L.size() == 1) Add(L[0].first, L[0].second);
			for (size_t K = 0; K + 1 < L.size(); ++K)
			{
				const double Dx = L[K + 1].first - L[K].first, Dy = L[K + 1].second - L[K].second, Long = std::hypot(Dx, Dy);
				if (!(Long < 1e5)) continue;
				const int Steps = std::max(1, static_cast<int>(std::ceil(Long / Step)));
				for (int I = 0; I <= Steps; ++I)
				{
					const double T = static_cast<double>(I) / Steps;
					Add(L[K].first + Dx * T, L[K].second + Dy * T);
				}
			}
		}
		/** The nearest within Reach, or Reach. */
		double To(double X, double Y, double Reach) const
		{
			double Best = Reach;
			const int I0 = Sq(X - Reach), I1 = Sq(X + Reach), J0 = Sq(Y - Reach), J1 = Sq(Y + Reach);
			for (int J = J0; J <= J1; ++J)
				for (int I = I0; I <= I1; ++I)
				{
					const auto F = Of.find(KeyOf(I, J));
					if (F == Of.end()) continue;
					for (const auto& P : F->second) Best = std::min(Best, std::hypot(P.first - X, P.second - Y));
				}
			return Best;
		}

	private:
		std::unordered_map<int64_t, Line> Of;
		static int Sq(double V) { return static_cast<int>(std::floor(V / 4.0)); }
		static int64_t KeyOf(int I, int J) { return (static_cast<int64_t>(I) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(J)); }
	};

	// ---- a small painter: orthographic views of the field's meshes, z-buffered, lit by one sun ----

	struct Image
	{
		int W = 0, H = 0;
		std::vector<uint8_t> Rgb;
		std::vector<float> Depth;
		/** What each pixel shows, for the measure (0 untagged; 1 + a wreck's Coat; 32 + her patch's Scorch), and how lit
		 * it is (the sun's light on it, 0.42 to 1). */
		std::vector<uint8_t> Tag;
		std::vector<float> Lit;
		/** The box lines and rings are drawn in (the map's, not its key's). */
		int ClipW = 0, ClipH = 0;
		Image(int InW, int InH)
			: W(InW), H(InH), Rgb(static_cast<size_t>(InW) * static_cast<size_t>(InH) * 3, 0), Depth(static_cast<size_t>(InW) * static_cast<size_t>(InH), 1e30f),
			  Tag(static_cast<size_t>(InW) * static_cast<size_t>(InH), 0), Lit(static_cast<size_t>(InW) * static_cast<size_t>(InH), 0.0f), ClipW(InW), ClipH(InH)
		{
		}
		void Put(int X, int Y, double R, double G, double B)
		{
			if (X < 0 || Y < 0 || X >= W || Y >= H) return;
			uint8_t* P = &Rgb[(static_cast<size_t>(Y) * static_cast<size_t>(W) + static_cast<size_t>(X)) * 3];
			P[0] = static_cast<uint8_t>(std::clamp(R, 0.0, 255.0));
			P[1] = static_cast<uint8_t>(std::clamp(G, 0.0, 255.0));
			P[2] = static_cast<uint8_t>(std::clamp(B, 0.0, 255.0));
		}
		void Blend(int X, int Y, double R, double G, double B, double A)
		{
			if (X < 0 || Y < 0 || X >= ClipW || Y >= ClipH) return;
			const uint8_t* P = &Rgb[(static_cast<size_t>(Y) * static_cast<size_t>(W) + static_cast<size_t>(X)) * 3];
			Put(X, Y, P[0] + (R - P[0]) * A, P[1] + (G - P[1]) * A, P[2] + (B - P[2]) * A);
		}
		void Fill(int X0, int Y0, int X1, int Y1, double R, double G, double B)
		{
			for (int Y = Y0; Y < Y1; ++Y)
				for (int X = X0; X < X1; ++X) Put(X, Y, R, G, B);
		}
		void Ring(double Cx, double Cy, double Rad, double Thick, double R, double G, double B)
		{
			const int X0 = static_cast<int>(std::floor(Cx - Rad - Thick)), X1 = static_cast<int>(std::ceil(Cx + Rad + Thick));
			const int Y0 = static_cast<int>(std::floor(Cy - Rad - Thick)), Y1 = static_cast<int>(std::ceil(Cy + Rad + Thick));
			for (int Y = Y0; Y <= Y1; ++Y)
				for (int X = X0; X <= X1; ++X)
				{
					const double D = std::fabs(std::hypot(X + 0.5 - Cx, Y + 0.5 - Cy) - Rad);
					if (D < Thick) Blend(X, Y, R, G, B, std::min(1.0, Thick - D));
				}
		}
		void Segment(double Ax, double Ay, double Bx, double By, double R, double G, double B, double A)
		{
			const int Steps = std::max(1, static_cast<int>(std::ceil(std::hypot(Bx - Ax, By - Ay))));
			for (int I = 0; I <= Steps; ++I)
			{
				const double T = static_cast<double>(I) / Steps;
				Blend(static_cast<int>(Ax + (Bx - Ax) * T), static_cast<int>(Ay + (By - Ay) * T), R, G, B, A);
			}
		}
		bool Save(const std::string& Path) const
		{
			std::ofstream Out(Path, std::ios::binary);
			if (!Out) return false;
			Out << "P6\n" << W << " " << H << "\n255\n";
			Out.write(reinterpret_cast<const char*>(Rgb.data()), static_cast<std::streamsize>(Rgb.size()));
			return static_cast<bool>(Out);
		}
	};

	/** An orthographic eye: a point's place on the picture (Right and Up across it, Scale pixels a metre about Mid) and
	 * its depth along Toward. Clip: the picture's box drawn into. */
	struct View
	{
		V3 Mid, Right, Up, Toward;
		double Scale = 1.0, Cx = 0.0, Cy = 0.0;
		int X0 = 0, Y0 = 0, X1 = 0, Y1 = 0;
		static View Looking(const V3& At, double Azimuth, double Elevation, double Scale, int X0, int Y0, int X1, int Y1)
		{
			View V;
			V.Mid = At;
			const V3 ToEye(std::sin(Azimuth) * std::cos(Elevation), std::cos(Azimuth) * std::cos(Elevation), std::sin(Elevation));
			V.Toward = -ToEye;
			V.Right = std::fabs(Elevation) > 1.5 ? V3(1.0, 0.0, 0.0) : V.Toward.Cross(V3(0.0, 0.0, 1.0)).Unit();
			V.Up = std::fabs(Elevation) > 1.5 ? V3(0.0, 1.0, 0.0) : V.Right.Cross(V.Toward).Unit();
			V.Scale = Scale;
			V.Cx = 0.5 * (X0 + X1);
			V.Cy = 0.5 * (Y0 + Y1);
			V.X0 = X0;
			V.Y0 = Y0;
			V.X1 = X1;
			V.Y1 = Y1;
			return V;
		}
		void Place(const V3& P, double& Sx, double& Sy, double& Sz) const
		{
			const V3 D = P - Mid;
			Sx = Cx + D.Dot(Right) * Scale;
			Sy = Cy - D.Dot(Up) * Scale;
			Sz = D.Dot(Toward);
		}
	};

	/** A mesh drawn in a view (its points At metres from Origin), lit by Sun, each triangle's corners' colours mixed; each
	 * pixel tagged TagBase + the nearest corner's tag (Tags: one a point; none: 0). */
	void Draw(Image& Img, const View& V, const LandField::Mesh& M, const V3& Origin, const V3& Sun, const std::vector<uint8_t>* Tags = nullptr, int TagBase = 0)
	{
		const bool bTags = Tags && Tags->size() == M.Colours.size();
		const size_t N = M.Colours.size();
		std::vector<double> Sx(N), Sy(N), Sz(N);
		for (size_t I = 0; I < N; ++I)
		{
			V.Place(V3(M.Positions[I * 3], M.Positions[I * 3 + 1], M.Positions[I * 3 + 2]) + Origin, Sx[I], Sy[I], Sz[I]);
		}
		for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
		{
			const uint32_t A = M.Triangles[T], B = M.Triangles[T + 1], C = M.Triangles[T + 2];
			if (A >= N || B >= N || C >= N) continue;
			V3 Nm(M.Normals[A * 3] + M.Normals[B * 3] + M.Normals[C * 3], M.Normals[A * 3 + 1] + M.Normals[B * 3 + 1] + M.Normals[C * 3 + 1],
				M.Normals[A * 3 + 2] + M.Normals[B * 3 + 2] + M.Normals[C * 3 + 2]);
			Nm = Nm.Unit();
			if (Nm.Dot(V.Toward) > 0.0) Nm = -Nm;
			const double Light = 0.42 + 0.58 * std::max(0.0, Nm.Dot(Sun));
			const double MinX = std::min({ Sx[A], Sx[B], Sx[C] }), MaxX = std::max({ Sx[A], Sx[B], Sx[C] });
			const double MinY = std::min({ Sy[A], Sy[B], Sy[C] }), MaxY = std::max({ Sy[A], Sy[B], Sy[C] });
			const int I0 = std::max(V.X0, static_cast<int>(std::floor(MinX))), I1 = std::min(V.X1 - 1, static_cast<int>(std::ceil(MaxX)));
			const int J0 = std::max(V.Y0, static_cast<int>(std::floor(MinY))), J1 = std::min(V.Y1 - 1, static_cast<int>(std::ceil(MaxY)));
			const double Area = (Sx[B] - Sx[A]) * (Sy[C] - Sy[A]) - (Sx[C] - Sx[A]) * (Sy[B] - Sy[A]);
			if (std::fabs(Area) < 1e-12) continue;
			for (int J = J0; J <= J1; ++J)
				for (int I = I0; I <= I1; ++I)
				{
					const double Px = I + 0.5, Py = J + 0.5;
					const double W0 = ((Sx[B] - Px) * (Sy[C] - Py) - (Sx[C] - Px) * (Sy[B] - Py)) / Area;
					const double W1 = ((Sx[C] - Px) * (Sy[A] - Py) - (Sx[A] - Px) * (Sy[C] - Py)) / Area;
					const double W2 = 1.0 - W0 - W1;
					if (W0 < -1e-9 || W1 < -1e-9 || W2 < -1e-9) continue;
					const double Z = W0 * Sz[A] + W1 * Sz[B] + W2 * Sz[C];
					float& D = Img.Depth[static_cast<size_t>(J) * static_cast<size_t>(Img.W) + static_cast<size_t>(I)];
					if (Z >= D) continue;
					D = static_cast<float>(Z);
					const size_t Px0 = static_cast<size_t>(J) * static_cast<size_t>(Img.W) + static_cast<size_t>(I);
					Img.Lit[Px0] = static_cast<float>(Light);
					Img.Tag[Px0] = bTags ? static_cast<uint8_t>(TagBase + (*Tags)[W0 >= W1 && W0 >= W2 ? A : W1 >= W2 ? B : C]) : 0;
					const LandField::Rgb8 &Ca = M.Colours[A], &Cb = M.Colours[B], &Cc = M.Colours[C];
					Img.Put(I, J, Light * (W0 * Ca.R + W1 * Cb.R + W2 * Cc.R), Light * (W0 * Ca.G + W1 * Cb.G + W2 * Cc.G), Light * (W0 * Ca.B + W1 * Cb.B + W2 * Cc.B));
				}
		}
	}

	/** The ground round a place as a mesh (Half metres each way, Step apart), as drawn and coloured, for a close look. */
	LandField::Mesh GroundAround(const LandField::Ground& G, double X, double Y, double Half, double Step, const V3& Origin)
	{
		LandField::Mesh M;
		const int N = static_cast<int>(std::ceil(2.0 * Half / Step)) + 1;
		for (int J = 0; J < N; ++J)
			for (int I = 0; I < N; ++I)
			{
				const double Px = X - Half + I * Step, Py = Y - Half + J * Step;
				double Nx = 0.0, Ny = 0.0, Nz = 1.0;
				G.DrawnUp(Px, Py, Nx, Ny, Nz);
				const double Z = G.HeightAt(Px, Py);
				M.Positions.insert(M.Positions.end(), { static_cast<float>(Px - Origin.X), static_cast<float>(Py - Origin.Y), static_cast<float>(Z - Origin.Z) });
				M.Normals.insert(M.Normals.end(), { static_cast<float>(Nx), static_cast<float>(Ny), static_cast<float>(Nz) });
				M.Colours.push_back(G.ColourAt(Px, Py));
			}
		for (int J = 0; J + 1 < N; ++J)
			for (int I = 0; I + 1 < N; ++I)
			{
				const uint32_t A = static_cast<uint32_t>(J * N + I), B = A + 1, C = A + static_cast<uint32_t>(N), D = C + 1;
				M.Triangles.insert(M.Triangles.end(), { A, B, D, A, D, C });
			}
		return M;
	}

	/** A nation's mark on the map. */
	void NationColour(const std::string& Nat, double& R, double& G, double& B)
	{
		if (Nat == "GER") { R = 70; G = 200; B = 255; }
		else if (Nat == "SOV") { R = 255; G = 70; B = 60; }
		else if (Nat == "USA") { R = 120; G = 255; B = 90; }
		else if (Nat == "GBR" || Nat == "CAN") { R = 255; G = 220; B = 60; }
		else { R = 255; G = 140; B = 255; }
	}

	const char* WhereWord(LandHulk::Where W)
	{
		switch (W)
		{
		case LandHulk::Where::OldLine: return "old line";
		case LandHulk::Where::Spot: return "junction";
		case LandHulk::Where::Axis: return "axis";
		default: return "strewn";
		}
	}
	const char* DrawnWord(LandField::Drawn D)
	{
		switch (D)
		{
		case LandField::Drawn::Pasture: return "pasture";
		case LandField::Drawn::Crop: return "crop";
		case LandField::Drawn::Woods: return "woods";
		case LandField::Drawn::Rough: return "rough";
		case LandField::Drawn::Works: return "works";
		case LandField::Drawn::Village: return "village";
		case LandField::Drawn::Yard: return "yard";
		case LandField::Drawn::Lane: return "lane";
		case LandField::Drawn::Bank: return "bank";
		case LandField::Drawn::Floor: return "floor";
		case LandField::Drawn::Wall: return "wall";
		case LandField::Drawn::Water: return "water";
		case LandField::Drawn::Marsh: return "marsh";
		default: return "beyond";
		}
	}

	/** Every point of a yard's wrecks on the field (each dressed at its near level), every Stride'th, with its wreck. */
	struct Spot
	{
		double X, Y, Z;
		size_t K;
	};
	std::vector<Spot> SpotsOf(const LandHulk::Yard& Y, size_t Stride, std::vector<LandField::Mesh>* Keep = nullptr)
	{
		std::vector<Spot> Out;
		for (size_t K = 0; K < Y.Hulks().size(); ++K)
		{
			const LandHulk::Hulk& H = Y.Hulks()[K];
			LandField::Mesh M;
			Y.Dress(K, true, M);
			for (size_t I = 0; I < M.Colours.size(); I += Stride)
			{
				Out.push_back({ H.X + M.Positions[I * 3], H.Y + M.Positions[I * 3 + 1], H.Z + M.Positions[I * 3 + 2], K });
			}
			if (Keep) Keep->push_back(std::move(M));
		}
		return Out;
	}

	/** How far a place on the field lies from a wreck's hull footprint (her model's HullLo to HullHi; 0 on it). */
	double FromFootprint(const LandHulk::Hulk& H, const LandKit::Model& M, double X, double Y)
	{
		const double Fx = std::sin(H.Heading), Fy = std::cos(H.Heading), Rx = std::cos(H.Heading), Ry = -std::sin(H.Heading);
		const double A = (X - H.X) * Fx + (Y - H.Y) * Fy, B = (X - H.X) * Rx + (Y - H.Y) * Ry;
		return std::hypot(std::max({ M.HullLo.X - A, 0.0, A - M.HullHi.X }), std::max({ M.HullLo.Y - B, 0.0, B - M.HullHi.Y }));
	}

	/** One line on a hulk, for the log and the picture's key. */
	std::string Describe(const LandHulk::Hulk& H)
	{
		const long Whole = std::lround(H.Months);
		const std::string Age = Whole < 1 ? std::string("under a month") : std::to_string(Whole) + (Whole == 1 ? " month" : " months");
		char Text[256];
		std::snprintf(Text, sizeof(Text), "%s %s, %s, %s%s%s%s", H.Nation.c_str(), H.Model.c_str(), Age.c_str(), WhereWord(H.At),
			H.bTurretOff ? ", turret thrown" : "", H.TrackOff != 0 ? ", track off" : "", H.bSoft ? ", soft-skinned" : "");
		return Text;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::printf("usage: land_hulk_test <Content> <out dir> <recording.json ...>\n");
		return 2;
	}
	const std::filesystem::path Content = Argv[1];
	const std::string OutDir = Argv[2];
	Fates::Table Table;
	{
		std::string Text, Error;
		if (!ReadText(Content / "Data/Battle/fates.json", Text) || !Table.Load(Text, Error))
		{
			std::printf("FAIL: Content/Data/Battle/fates.json not read (%s)\n", Error.c_str());
			return 1;
		}
	}
	const std::filesystem::path ModelDir = Content / "Models/Battle";
	int FileReads = 0;
	const LandKit::Battle::ReadFn ReadModel = [&ModelDir, &FileReads](const std::string& Name, std::string& Out)
	{
		++FileReads;
		return ReadText(ModelDir / Name, Out);
	};

	// LandFeed reads the campaign's key, whatever is missing or odd in it (on the smallest recording there is, so the
	// rest of what it reads is the same with and without it).
	{
		struct Case
		{
			const char* Key;
			int Count;
			double Months;
			const char* Nats;
			double Per;
		};
		const Case Cases[] = {
			{ "", 0, 0.0, "", 1.0 },
			{ "\"oldWrecks\": {\"count\": 12, \"months\": 7.5, \"nats\": {\"GER\": 8, \"SOV\": 4}}, ", 12, 7.5, "GER 8,SOV 4,", 1.0 },
			{ "\"oldWrecks\": {\"count\": 200, \"months\": 2}, ", 60, 2.0, "", 1.0 },
			{ "\"oldWrecks\": {\"count\": -3, \"months\": -2}, ", 0, 0.0, "", 1.0 },
			{ "\"oldWrecks\": {\"months\": 5}, ", 0, 5.0, "", 1.0 },
			{ "\"oldWrecks\": {\"count\": \"12\", \"months\": \"x\", \"nats\": [1, 2]}, ", 0, 0.0, "", 1.0 },
			{ "\"oldWrecks\": {\"count\": 9, \"nats\": {\"GER\": 0, \"USA\": -1, \"\": 4, \"SOV\": 2.6, \"ITA\": \"3\", \"GBR\": null}}, ", 9, 0.0, "SOV 3,", 1.0 },
			{ "\"oldWrecks\": 5, ", 0, 0.0, "", 1.0 },
			{ "\"oldWrecks\": null, ", 0, 0.0, "", 1.0 },
			{ "\"oldWrecks\": {}, ", 0, 0.0, "", 1.0 },
			// (the battle's scale, "per": the campaign's real armoured vehicles to a drawn tank, a tenth at most)
			{ "\"oldWrecks\": {\"count\": 60, \"months\": 1, \"per\": 7.5}, ", 60, 1.0, "", 7.5 },
			{ "\"oldWrecks\": {\"count\": 40, \"per\": 0.4}, ", 40, 0.0, "", 1.0 },
			{ "\"oldWrecks\": {\"count\": 40, \"per\": -3}, ", 40, 0.0, "", 1.0 },
			{ "\"oldWrecks\": {\"count\": 40, \"per\": \"10\"}, ", 40, 0.0, "", 1.0 },
			{ "\"oldWrecks\": {\"count\": 40, \"per\": null}, ", 40, 0.0, "", 1.0 },
			{ "\"oldWrecks\": {\"count\": 40, \"per\": 5000}, ", 40, 0.0, "", 1000.0 },
		};
		std::string Base;
		ReadText(Argv[3], Base);
		const size_t Open = Base.find('{');
		size_t Tracks = 0, Rounds = 0;
		int Read = 0;
		for (const Case& C : Cases)
		{
			std::string Text = Base;
			if (Open != std::string::npos) Text.insert(Open + 1, C.Key);
			LandFeed::Recording R;
			std::string Error;
			if (!LandFeed::ReadRecording(Text, R, Error))
			{
				Fault(std::string("the recording not read with ") + C.Key + ": " + Error);
				continue;
			}
			std::string Nats;
			for (const auto& P : R.OldWreckNats) Nats += P.first + " " + std::to_string(P.second) + ",";
			if (R.OldWreckCount != C.Count || std::fabs(R.OldWreckMonths - C.Months) > 1e-9 || Nats != C.Nats || std::fabs(R.OldWreckPer - C.Per) > 1e-9)
			{
				Fault(std::string("oldWrecks read as ") + std::to_string(R.OldWreckCount) + ", " + std::to_string(R.OldWreckMonths) + ", [" + Nats + "], per " +
					std::to_string(R.OldWreckPer) + " from " + C.Key);
			}
			if (Read == 0)
			{
				Tracks = R.Tracks.size();
				Rounds = R.Rounds.size();
			}
			else if (R.Tracks.size() != Tracks || R.Rounds.size() != Rounds)
			{
				Fault(std::string("the rest of the recording read otherwise with ") + C.Key);
			}
			++Read;
		}
		std::printf("LandFeed: the oldWrecks key read %d ways (absent, whole, over the cap, below nothing, a part missing, wrong kinds, odd nations, not an object, null, empty; the scale per whole, under 1, below nothing, a string, null, over 1000)\n", Read);
	}

	// The battle's scale (Brian's rule: one model stands for as many real things in every part of a battle): the key's
	// count is real machines burnt out, drawn Per to a wreck, at least one where any burnt, before the scrap men's share.
	{
		struct Scale
		{
			int Count;
			double Per;
			double Months;
			int Drawn;
			int Kept;
		};
		const Scale Scales[] = {
			{ 60, 1.0, 1.0, 60, 20 },      // one to one: as before (and at most 20 on a field)
			{ 40, 1.0, 18.0, 40, 13 },     // one to one, 18 months on: as before (Kept(40, 18))
			{ 60, 7.5, 1.0, 8, 8 },        // an armoured division's 150 drawn as 20 tanks: 60 burnt out (the cap) is 8
			{ 60, 3.0, 12.0, 20, 10 },     // 20 drawn, half left at a year
			{ 150, 10.0, 1.0, 6, 6 },      // over the cap: 60 burnt out, a tenth of them drawn
			{ 3, 10.0, 1.0, 1, 1 },        // fewer than a drawn tank stands for: still one
			{ 3, 10.0, 48.0, 1, 0 },       // and the scrap men take her in time
			{ 0, 10.0, 1.0, 0, 0 },        // none burnt: none
			{ 40, 0.5, 1.0, 40, 20 },      // under one: one to one
			{ 40, std::nan(""), 1.0, 40, 20 }, // not a number: one to one
			{ 40, 1e9, 1.0, 1, 1 },        // a thousand at most: still one
		};
		int Checked = 0;
		for (const Scale& C : Scales)
		{
			const int Drawn = LandHulk::Models(C.Count, C.Per);
			const int Kept = LandHulk::Kept(C.Count, C.Months, C.Per);
			if (Drawn != C.Drawn || Kept != C.Kept)
			{
				Fault("the battle's scale: " + std::to_string(C.Count) + " burnt out at " + std::to_string(C.Per) + " to a wreck, " + std::to_string(C.Months) +
					" months on: " + std::to_string(Drawn) + " drawn and " + std::to_string(Kept) + " kept, not " + std::to_string(C.Drawn) + " and " +
					std::to_string(C.Kept));
			}
			++Checked;
		}
		if (LandHulk::Kept(40, 6.0) != LandHulk::Kept(40, 6.0, 1.0)) Fault("the battle's scale: Kept's one to one not its default");
		std::printf("the battle's scale: %d counts drawn at their per (60 at 7.5 to a wreck: %d drawn; 3 at 10: %d; none: %d; per under 1 or odd: one to one)\n", Checked,
			LandHulk::Models(60, 7.5), LandHulk::Models(3, 10.0), LandHulk::Models(0, 10.0));
	}

	for (int A = 3; A < Argc; ++A)
	{
		const std::filesystem::path RecPath = Argv[A];
		const std::string Name = RecPath.stem().string();
		std::string Text, Error;
		LandFeed::Recording Rec;
		if (!ReadText(RecPath, Text) || !LandFeed::ReadRecording(Text, Rec, Error))
		{
			Fault(Name + ": not read: " + Error);
			continue;
		}
		std::printf("\n== %s: %s (%s against %s), %zu tracks\n", Name.c_str(), Rec.Title.c_str(), Rec.Sides[0].Nat.c_str(), Rec.Sides[1].Nat.c_str(), Rec.Tracks.size());

		// The field as the scene lays it, the steppe's or a region's kit read as it reads it.
		const auto FieldT0 = std::chrono::steady_clock::now();
		LandField::Field Words = LandSight::FieldOf(Rec);
		if (Words.Steppe.bOn)
		{
			for (const char* Kit : { "Khata", "BurntChimney", "TelegraphPole", "WattleFence", "WellSweep", "Haystack", "Birch" })
			{
				std::string KitText;
				if (ReadText(Content / "Models/East" / (std::string(Kit) + ".json"), KitText)) Words.Steppe.Kit[Kit] = KitText;
			}
		}
		else
		{
			std::string Folder;
			std::vector<std::string> Names;
			if (LandScape::KitWanted(Words, Folder, Names))
			{
				for (const std::string& Kit : Names)
				{
					std::string KitText;
					if (ReadText(Content / "Models" / Folder / (Kit + ".json"), KitText)) Words.Kit[Kit] = KitText;
				}
			}
		}
		const LandField::Ground G(Words);
		std::printf("field: laid in %.0f ms (%s)\n", Since(FieldT0), G.LookWord());

		// The battle's own armour, as the scene's MakeArmour reads it: its models (each a nation) the wrecks are drawn
		// from where it has them, and its month and ground.
		const auto ArmourT0 = std::chrono::steady_clock::now();
		const LandField::Ground* Gp = &G;
		const LandKit::GroundFn GroundAt = [Gp](double X, double Y, double& OutZ, V3& OutNormal)
		{
			const LandField::Surface Under = Gp->At(X, Y);
			OutZ = Under.Z;
			OutNormal = V3(Under.NX, Under.NY, Under.NZ);
		};
		LandKit::Battle Armour;
		if (!Armour.Load(Text, ReadModel, GroundAt, &Table, Rec.Seed ? Rec.Seed : 1, Error))
		{
			Fault(Name + ": the battle's armour not loaded: " + Error);
			continue;
		}
		const LandKit::Battle* ArmourPtr = &Armour;
		const LandHulk::Yard::HaveFn Have = [ArmourPtr](const std::string& Model, const std::string& Nation) -> const LandKit::Model*
		{
			const auto Found = ArmourPtr->Models().find(Model + "|" + Nation);
			return Found == ArmourPtr->Models().end() ? nullptr : &Found->second;
		};
		std::printf("armour: %zu vehicles, %zu models, %d-%02d, %s, loaded in %.0f ms\n", Armour.Vehicles().size(), Armour.Models().size(), Armour.Year, Armour.Month,
			Armour.Terrain.c_str(), Since(ArmourT0));
		const auto AskOf = [&](int Count, double Months)
		{
			LandHulk::Ask Q;
			Q.Count = Count;
			Q.Months = Months;
			Q.Year = Armour.Year;
			Q.Month = Armour.Month;
			Q.Terrain = Armour.Terrain;
			Q.Seed = Rec.Seed ? Rec.Seed : 1;
			return Q;
		};

		// A field as at the battle's open with no scars and no old ruts (their switches off).
		const LandHulk::Field F = LandHulk::FieldOf(Rec, G, nullptr, {});
		Near VehicleNear, ManNear, LaneNear, BlastNear;
		for (const Line& L : F.Vehicles) VehicleNear.AddLine(L, 0.5);
		for (const Line& L : F.Men) ManNear.AddLine(L, 0.5);
		for (const Line& L : F.Lanes) LaneNear.AddLine(L, 0.5);
		for (const auto& P : F.Blasts) BlastNear.Add(P.first, P.second);
		std::printf("field read: %zu points fought over, %zu vehicles' ways, %zu men's, %zu blasts, %zu lanes, %zu banks, walls and wood edges, %zu things standing, %zu old lines and %zu places a fight gathers\n",
			F.Fought.size(), F.Vehicles.size(), F.Men.size(), F.Blasts.size(), F.Lanes.size(), F.Hedges.size(), F.Standing.size(), F.Lines.Lines.size(), F.Lines.Spots.size());

		// Where they lie: every point of every wreck (its near level), read where it lies.
		const auto CheckPlaces = [&](const LandHulk::Yard& Y, const std::string& Label, const LandCrater::Field* Holes, const std::vector<Line>* Ruts)
		{
			const std::vector<Spot> All = SpotsOf(Y, 3);
			std::map<std::string, int> Wrong;
			double VehicleMin = 1e9, ManMin = 1e9, LaneMin = 1e9, BlastMin = 1e9, RutMin = 1e9;
			int OnHole = 0, Below = 0, Above = 0, Shared = 0;
			double Lowest = 1e9;
			// (each wreck's lowest point, and a thrown turret's, off the ground under it: settled, not floating)
			std::vector<double> HullLow(Y.Hulks().size(), 1e9), TurretLow(Y.Hulks().size(), 1e9);
			Near RutNear;
			if (Ruts)
				for (const Line& L : *Ruts) RutNear.AddLine(L, 0.5);
			// (who holds each half-metre square: none held by two)
			std::unordered_map<int64_t, size_t> Held;
			for (const Spot& S : All)
			{
				const LandField::Drawn What = G.At(S.X, S.Y).What;
				const bool bCorn = What == LandField::Drawn::Crop && G.CornAt(S.X, S.Y);
				if (bCorn) ++Wrong["standing corn"];
				else if (!(What == LandField::Drawn::Pasture || What == LandField::Drawn::Rough || What == LandField::Drawn::Beyond || What == LandField::Drawn::Crop)) ++Wrong[DrawnWord(What)];
				VehicleMin = std::min(VehicleMin, VehicleNear.To(S.X, S.Y, 30.0));
				ManMin = std::min(ManMin, ManNear.To(S.X, S.Y, 10.0));
				LaneMin = std::min(LaneMin, LaneNear.To(S.X, S.Y, 20.0));
				BlastMin = std::min(BlastMin, BlastNear.To(S.X, S.Y, 20.0));
				if (Ruts) RutMin = std::min(RutMin, RutNear.To(S.X, S.Y, 20.0));
				if (Holes && (Holes->Bare(S.X, S.Y) || Holes->InCut(S.X, S.Y))) ++OnHole;
				const double Under = G.At(S.X, S.Y).Z;
				Lowest = std::min(Lowest, S.Z - Under);
				const LandHulk::Hulk& Of = Y.Hulks()[S.K];
				const LandKit::Model* Mo = Y.ModelOf(S.K);
				const double FromTurret = std::hypot(S.X - Of.TurretX, S.Y - Of.TurretY);
				if (Of.bTurretOff && Mo && FromTurret < 0.5 * (Mo->TurretHi.Y - Mo->TurretLo.Y)) TurretLow[S.K] = std::min(TurretLow[S.K], S.Z - Under);
				else if (std::hypot(S.X - Of.X, S.Y - Of.Y) < 2.0) HullLow[S.K] = std::min(HullLow[S.K], S.Z - Under);
				if (S.Z < Under - 0.6) ++Below;
				if (S.Z > Under + 4.5) ++Above;
				const int64_t Key = (static_cast<int64_t>(std::floor(S.X * 2.0)) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(static_cast<int>(std::floor(S.Y * 2.0))));
				const auto Got = Held.emplace(Key, S.K);
				if (!Got.second && Got.first->second != S.K) ++Shared;
			}
			// (her scorched patch on open ground too, every point of it as Patch lays it: its ragged edge reaches some 2.4 m
			// past her outline, onto a trench's works or a garden on the steppe's first lay)
			size_t PatchPoints = 0;
			for (size_t K = 0; K < Y.Hulks().size(); ++K)
			{
				const LandHulk::Hulk& Hk = Y.Hulks()[K];
				LandField::Mesh M;
				Y.Patch(K, M);
				for (size_t I = 0; I < M.Colours.size(); ++I)
				{
					const double Px = Hk.X + M.Positions[I * 3], Py = Hk.Y + M.Positions[I * 3 + 1];
					++PatchPoints;
					const LandField::Drawn What = G.At(Px, Py).What;
					if (What == LandField::Drawn::Crop && G.CornAt(Px, Py)) ++Wrong["standing corn (a patch)"];
					else if (!(What == LandField::Drawn::Pasture || What == LandField::Drawn::Rough || What == LandField::Drawn::Beyond || What == LandField::Drawn::Crop))
						++Wrong[std::string(DrawnWord(What)) + " (a patch)"];
					if (Holes && (Holes->Bare(Px, Py) || Holes->InCut(Px, Py))) ++OnHole;
				}
			}
			// (the grass kept out of every hull's footprint, her corners and all, every 10 cm; each thrown turret clear of
			// her own hull and run-off track, its gun not through her hull, and its box for the camera standing on the
			// ground)
			size_t FootPoints = 0, FootBare = 0;
			int Thrown = 0, OwnHit = 0, BadBox = 0;
			double BoxTop = 0.0;
			for (size_t K = 0; K < Y.Hulks().size(); ++K)
			{
				const LandHulk::Hulk& Hk = Y.Hulks()[K];
				const LandKit::Model* Mo = Y.ModelOf(K);
				if (!Mo) continue;
				const double Fx = std::sin(Hk.Heading), Fy = std::cos(Hk.Heading), Rx = std::cos(Hk.Heading), Ry = -std::sin(Hk.Heading);
				for (double Along = Mo->HullLo.X; Along <= Mo->HullHi.X; Along += 0.1)
					for (double Across = Mo->HullLo.Y; Across <= Mo->HullHi.Y; Across += 0.1)
					{
						++FootPoints;
						if (!Y.Covers(Hk.X + Fx * Along + Rx * Across, Hk.Y + Fy * Along + Ry * Across)) ++FootBare;
					}
				if (!Hk.bTurretOff) continue;
				++Thrown;
				const double TurretHalf = 0.5 * std::hypot(Mo->TurretHi.X - Mo->TurretLo.X, Mo->TurretHi.Y - Mo->TurretLo.Y);
				bool bHit = FromFootprint(Hk, *Mo, Hk.TurretX, Hk.TurretY) < TurretHalf;
				for (const auto& P : Hk.Strip) bHit = bHit || std::hypot(P.first - Hk.TurretX, P.second - Hk.TurretY) < TurretHalf;
				if (Hk.bTurretGun) bHit = bHit || FromFootprint(Hk, *Mo, Hk.TurretMuzzle.X, Hk.TurretMuzzle.Y) < 0.3;
				OwnHit += bHit ? 1 : 0;
				const double Under = G.At(Hk.TurretX, Hk.TurretY).Z, Floor = Hk.TurretZ + Hk.TurretBoxLo.Z, Top = Hk.TurretZ + Hk.TurretBoxHi.Z;
				if (std::fabs(Floor - Under) > 0.75 || Top - Floor < 0.4 || Top - Floor > 3.5 || Hk.TurretBoxHi.X - Hk.TurretBoxLo.X < 0.5) ++BadBox;
				BoxTop = std::max(BoxTop, Top - Under);
			}
			std::printf("%s: %zu points of the scorched patches read with the wrecks'; %zu of %zu points of the hulls' footprints with grass standing; %d thrown turrets, %d on her own hull, track or gun, %d camera boxes amiss (the highest %.1f m over the grass)\n",
				Label.c_str(), PatchPoints, FootBare, FootPoints, Thrown, OwnHit, BadBox, BoxTop);
			if (FootBare > 0) Fault(Label + ": grass standing through a wreck's hull");
			if (OwnHit > 0) Fault(Label + ": a thrown turret on her own hull or run-off track, or its gun through her hull");
			if (BadBox > 0) Fault(Label + ": a thrown turret's box for the camera off the ground or out of size");
			// (her run-off track, the art director's carry of 24 November, not a dead-straight ladder: snaking gently, folded
			// back on itself once, its links flat on the ground, none through her hull, her road wheels or the ground)
			int Tracks = 0, Unfolded = 0, Straight = 0, UnderHer = 0, Through = 0, TurnsLeast = 1 << 30;
			double SwayLeast = 1e9, SwayMost = 0.0, ClearLeast = 1e9, LowLeast = 1e9, HighMost = -1e9;
			for (size_t K = 0; K < Y.Hulks().size(); ++K)
			{
				const LandHulk::Hulk& Hk = Y.Hulks()[K];
				const LandKit::Model* Mo = Y.ModelOf(K);
				if (Hk.TrackOff == 0 || !Mo || Hk.Strip.size() < 16) continue;
				++Tracks;
				const size_t N = Hk.Strip.size(), Flat = Hk.Fold > 0 ? std::min(N, static_cast<size_t>(Hk.Fold)) : N;
				if (Hk.Fold <= 0 || Hk.Curl <= 0 || Flat + static_cast<size_t>(Hk.Curl) >= N) ++Unfolded;
				// (the sway: each flat link's middle off its own smooth line, the mean of the links a metre and a half either
				// side; and how often it turns from bending one way to the other)
				const double Pitch = std::hypot(Hk.Strip[1].first - Hk.Strip[0].first, Hk.Strip[1].second - Hk.Strip[0].second);
				const size_t Reach = std::max<size_t>(2, static_cast<size_t>(std::lround(1.5 / std::max(0.05, Pitch))));
				double Sway = 0.0;
				int Turns = 0, Was = 0;
				for (size_t L = Reach; L + Reach < Flat; ++L)
				{
					double Mx = 0.0, My = 0.0;
					for (size_t J = L - Reach; J <= L + Reach; ++J)
					{
						Mx += Hk.Strip[J].first;
						My += Hk.Strip[J].second;
					}
					Mx /= static_cast<double>(2 * Reach + 1);
					My /= static_cast<double>(2 * Reach + 1);
					Sway = std::max(Sway, std::hypot(Hk.Strip[L].first - Mx, Hk.Strip[L].second - My));
					const double Ax = Hk.Strip[L].first - Hk.Strip[L - Reach].first, Ay = Hk.Strip[L].second - Hk.Strip[L - Reach].second;
					const double Bx = Hk.Strip[L + Reach].first - Hk.Strip[L].first, By = Hk.Strip[L + Reach].second - Hk.Strip[L].second;
					const double Bend = (Ax * By - Ay * Bx) / std::max(1e-9, std::hypot(Ax, Ay) * std::hypot(Bx, By));
					const int Way = Bend > 0.01 ? 1 : (Bend < -0.01 ? -1 : 0);
					if (Way != 0 && Was != 0 && Way != Was) ++Turns;
					if (Way != 0) Was = Way;
				}
				SwayLeast = std::min(SwayLeast, Sway);
				SwayMost = std::max(SwayMost, Sway);
				TurnsLeast = std::min(TurnsLeast, Turns);
				if (Sway < 0.04 && Turns == 0) ++Straight;
				// (its links' inner edge off her footprint, which holds her road wheels: half her track's width, her own
				// model's, inside each flat link's middle)
				const int TB = Mo->TrackBody[Hk.TrackOff < 0 ? 0 : 1];
				double Y0 = 1e9, Y1w = -1e9;
				if (TB >= 0)
				{
					const LandKit::Mesh& Tm = Mo->Bodies[static_cast<size_t>(TB)].Near;
					for (size_t I = 0; I < Tm.Count(); ++I)
					{
						Y0 = std::min(Y0, static_cast<double>(Tm.Points[I * 3 + 1]));
						Y1w = std::max(Y1w, static_cast<double>(Tm.Points[I * 3 + 1]));
					}
				}
				const double HalfW = Y1w > Y0 ? 0.5 * (Y1w - Y0) : 0.0;
				double Clear = 1e9;
				for (size_t L = 0; L < Flat; ++L) Clear = std::min(Clear, FromFootprint(Hk, *Mo, Hk.Strip[L].first, Hk.Strip[L].second) - HalfW);
				ClearLeast = std::min(ClearLeast, Clear);
				if (Clear < 0.0) ++UnderHer;
				// (its points as dressed, off her footprint and low: on the ground, not through it)
				LandField::Mesh Dm;
				Y.Dress(K, true, Dm);
				for (size_t I = 0; I < Dm.Colours.size(); ++I)
				{
					const double Px = Hk.X + Dm.Positions[I * 3], Py = Hk.Y + Dm.Positions[I * 3 + 1], Pz = Hk.Z + Dm.Positions[I * 3 + 2];
					if (FromFootprint(Hk, *Mo, Px, Py) < 0.15 || (Hk.bTurretOff && std::hypot(Px - Hk.TurretX, Py - Hk.TurretY) < 2.5)) continue;
					const double Over = Pz - G.HeightAt(Px, Py);
					if (Over > 0.6) continue;
					LowLeast = std::min(LowLeast, Over);
					HighMost = std::max(HighMost, Over);
					if (Over < -0.03) ++Through;
				}
			}
			if (Tracks > 0)
			{
				std::printf("%s: %d run-off tracks: %d folded back on themselves once; snaking (each link off its own smooth line up to %.2f to %.2f m, turning from one way to the other at least %d times); their links' inner edges at least %.2f m off her footprint (her road wheels); their points low off her %.3f to %.2f m over the ground, %d through it\n",
					Label.c_str(), Tracks, Tracks - Unfolded, SwayLeast, SwayMost, TurnsLeast, ClearLeast, LowLeast, HighMost, Through);
				if (Unfolded > 0) Fault(Label + ": " + std::to_string(Unfolded) + " run-off tracks not folded back on themselves");
				if (Straight > 0) Fault(Label + ": " + std::to_string(Straight) + " run-off tracks lying dead straight");
				if (UnderHer > 0) Fault(Label + ": " + std::to_string(UnderHer) + " run-off tracks under her road wheels");
				if (Through > 0) Fault(Label + ": " + std::to_string(Through) + " points of run-off tracks through the ground");
			}
			// (none across a cattle's path: its trodden pieces under no wreck's outline)
			int OnPath = 0;
			std::vector<LandField::Placed> Laid;
			for (const LandHulk::Hulk& Hk : Y.Hulks())
			{
				for (const LandHulk::Ring& R : Hk.Rings)
				{
					G.Clutter(R.X - R.R, R.Y - R.R, R.X + R.R, R.Y + R.R, Laid);
					for (const LandField::Placed& P : Laid)
						if (P.Kind == LandField::Base::Trodden && std::hypot(P.X - R.X, P.Y - R.Y) < R.R) ++OnPath;
				}
			}
			if (OnPath > 0) Fault(Label + ": " + std::to_string(OnPath) + " pieces of a cattle's path under a wreck");
			int Floating = 0, Sunk = 0;
			double HullMost = -1e9, TurretMost = -1e9;
			for (size_t K = 0; K < Y.Hulks().size(); ++K)
			{
				// (her tracks or wheels on the ground at her middle, up to a few centimetres of a slope's bend; a turret's
				// lowest point within 15 cm, its lid or its ring on the grass)
				if (HullLow[K] < 1e8) HullMost = std::max(HullMost, HullLow[K]);
				if (TurretLow[K] < 1e8) TurretMost = std::max(TurretMost, TurretLow[K]);
				if ((HullLow[K] < 1e8 && HullLow[K] > 0.08) || (TurretLow[K] < 1e8 && TurretLow[K] > 0.15)) ++Floating;
				if ((HullLow[K] < 1e8 && HullLow[K] < -0.5) || (TurretLow[K] < 1e8 && TurretLow[K] < -0.5)) ++Sunk;
			}
			std::printf("%s: each wreck's lowest point under her middle at most %.2f m off the ground, a thrown turret's %.2f m; %d floating, %d sunk deep\n", Label.c_str(), HullMost,
				TurretMost, Floating, Sunk);
			if (Floating > 0 || Sunk > 0) Fault(Label + ": a wreck or a thrown turret not settled on the ground");
			std::string WrongText;
			for (const auto& [Kind, N] : Wrong) WrongText += (WrongText.empty() ? "" : ", ") + std::to_string(N) + " on " + Kind;
			std::printf("%s: %zu wrecks, %zu points read: %s; %d pieces of a cattle's path under one; nearest a vehicle's way %.1f m, a man's %.1f m, a lane's middle %.1f m, a blast %.1f m%s; the lowest point %.2f m off the ground; %d half-metre squares shared\n",
				Label.c_str(), Y.Hulks().size(), All.size(), WrongText.empty() ? "every one on open ground" : WrongText.c_str(), OnPath, VehicleMin, ManMin, LaneMin, BlastMin,
				Ruts ? (", an old rut " + std::to_string(RutMin).substr(0, 4) + " m").c_str() : "", Lowest, Shared);
			if (!Wrong.empty()) Fault(Label + ": a wreck on " + WrongText);
			if (VehicleMin < LandHulk::VehicleClear) Fault(Label + ": a wreck within 8 m of a recorded vehicle's way");
			if (ManMin < LandHulk::ManClear) Fault(Label + ": a wreck within 1.5 m of a recorded man's way");
			if (LaneMin < LandHulk::LaneClear - 0.5) Fault(Label + ": a wreck within 5.5 m of a lane's middle");
			if (BlastMin < LandHulk::BlastClear - 0.5) Fault(Label + ": a wreck where a shell of this battle lands");
			if (Ruts && RutMin < LandHulk::WayClear - 0.5) Fault(Label + ": a wreck on an old rut");
			if (OnHole > 0) Fault(Label + ": " + std::to_string(OnHole) + " points of wrecks on an old shell hole");
			if (Below > 0 || Above > 0) Fault(Label + ": " + std::to_string(Below) + " points sunk deep, " + std::to_string(Above) + " floating");
			if (Shared > 0) Fault(Label + ": wrecks lying on one another");
			if (static_cast<int>(Y.Hulks().size()) > LandHulk::Most) Fault(Label + ": over the cap");
		};

		// 1. Forty burnt out here a month ago, the battle's two sides': laid, read, the same twice.
		LandHulk::Yard Y1;
		auto T0 = std::chrono::steady_clock::now();
		const int Laid = Y1.Lay(F, AskOf(40, 1.0), ReadModel, Have);
		const double LayMs = Since(T0);
		std::printf("laid: %s (%.0f ms, %d models read)\n", Y1.Said().c_str(), LayMs, Y1.Read);
		for (const std::string& S : Y1.StandIns) std::printf("  stand-in: %s\n", S.c_str());
		if (!F.Fought.empty() && Laid == 0) Fault(Name + ": none laid on a fought-over field");
		if (Laid != LandHulk::Kept(40, 1.0) && Y1.NoRoom == 0) Fault(Name + ": laid other than kept, with room");
		CheckPlaces(Y1, Name + ", a fresh field", nullptr, nullptr);
		for (size_t K = 0; K < Y1.Hulks().size(); ++K) std::printf("  %2zu  %s at (%.0f, %.0f), group %d\n", K + 1, Describe(Y1.Hulks()[K]).c_str(), Y1.Hulks()[K].X, Y1.Hulks()[K].Y, Y1.Hulks()[K].Group + 1);
		{
			LandHulk::Yard Y2;
			Y2.Lay(F, AskOf(40, 1.0), ReadModel, Have);
			bool bSame = Y2.Hulks().size() == Y1.Hulks().size() && Y2.Said() == Y1.Said();
			for (size_t K = 0; bSame && K < Y1.Hulks().size(); ++K)
			{
				const LandHulk::Hulk &P = Y1.Hulks()[K], &Q = Y2.Hulks()[K];
				bSame = P.X == Q.X && P.Y == Q.Y && P.Z == Q.Z && P.Heading == Q.Heading && P.Model == Q.Model && P.Nation == Q.Nation && P.Turret == Q.Turret
					&& P.bTurretOff == Q.bTurretOff && P.TrackOff == Q.TrackOff && P.Open == Q.Open && P.Seed == Q.Seed;
				for (int Level = 0; bSame && Level < 3; ++Level)
				{
					LandField::Mesh Mp, Mq;
					if (Level < 2)
					{
						Y1.Dress(K, Level == 0, Mp);
						Y2.Dress(K, Level == 0, Mq);
					}
					else
					{
						Y1.Patch(K, Mp);
						Y2.Patch(K, Mq);
					}
					bSame = HashOf(Mp) == HashOf(Mq);
				}
			}
			std::printf("the same twice: %s\n", bSame ? "yes (every wreck, its pose and its meshes, near, far and its patch)" : "NO");
			if (!bSame) Fault(Name + ": not the same twice");
		}

		// 2. The meshes: finite, whole, timed; the rust spreading as the months go (Count 60 lays the same twenty for any
		// months up to a year and a half: only their age differs).
		{
			size_t NearPoints = 0, FarPoints = 0, NearTris = 0, FarTris = 0, PatchTris = 0;
			int Bad = 0;
			const auto DressT0 = std::chrono::steady_clock::now();
			double NearMs = 0.0, FarMs = 0.0;
			for (size_t K = 0; K < Y1.Hulks().size(); ++K)
			{
				for (int Level = 0; Level < 3; ++Level)
				{
					LandField::Mesh M;
					const auto Each = std::chrono::steady_clock::now();
					if (Level < 2) Y1.Dress(K, Level == 0, M);
					else Y1.Patch(K, M);
					const double Took = Since(Each);
					const size_t N = M.Colours.size();
					if (M.Positions.size() != N * 3 || M.Normals.size() != N * 3 || M.Triangles.size() % 3 != 0 || M.Triangles.empty()) ++Bad;
					for (const float V : M.Positions) Bad += std::isfinite(V) ? 0 : 1;
					for (const float V : M.Normals) Bad += std::isfinite(V) ? 0 : 1;
					for (const uint32_t I : M.Triangles) Bad += I < N ? 0 : 1;
					if (Level == 0)
					{
						NearMs += Took;
						NearPoints += N;
						NearTris += M.Triangles.size() / 3;
					}
					else if (Level == 1)
					{
						FarMs += Took;
						FarPoints += N;
						FarTris += M.Triangles.size() / 3;
					}
					else
					{
						PatchTris += M.Triangles.size() / 3;
					}
				}
			}
			std::printf("dressed: %zu near points (%zu triangles, %.1f ms), %zu far (%zu triangles, %.1f ms), patches %zu triangles; all %.0f ms; %d faults\n", NearPoints, NearTris, NearMs,
				FarPoints, FarTris, FarMs, PatchTris, Since(DressT0), Bad);
			if (Bad > 0) Fault(Name + ": a dressed mesh with a NaN, a stray corner or a broken count");
			const double Ages[] = { 0.25, 1.0, 6.0, 12.0, 18.0 };
			const char* AgeWords[] = { "a week on", "a month on", "six months", "a year", "a year and a half" };
			double Share[5] = { 0, 0, 0, 0, 0 }, Dark[5] = { 0, 0, 0, 0, 0 };
			std::vector<LandHulk::Yard> Aged(5);
			bool bSameLay = true;
			for (int I = 0; I < 5; ++I)
			{
				// (the battle's month put on by the age, so they burnt in the same month: the same machines, the same coats)
				LandHulk::Ask Q = AskOf(60, Ages[I]);
				const int Later = Armour.Year * 12 + Armour.Month - 1 + static_cast<int>(std::lround(Ages[I]));
				Q.Year = Later / 12;
				Q.Month = Later % 12 + 1;
				Aged[static_cast<size_t>(I)].Lay(F, Q, ReadModel, Have);
				const LandHulk::Yard& Y = Aged[static_cast<size_t>(I)];
				size_t Rust = 0, All = 0;
				double Bright = 0.0;
				for (size_t K = 0; K < Y.Hulks().size(); ++K)
				{
					LandField::Mesh M;
					LandHulk::Dressing Nd;
					Y.Dress(K, true, M, &Nd);
					for (size_t P = 0; P < M.Colours.size() && P < Nd.Coats.size(); ++P)
					{
						++All;
						const LandHulk::Coat C = static_cast<LandHulk::Coat>(Nd.Coats[P]);
						if (C == LandHulk::Coat::Flash || C == LandHulk::Coat::OldRust)
						{
							++Rust;
							Bright += M.Colours[P].R + M.Colours[P].G + M.Colours[P].B;
						}
					}
					if (Nd.Coats.size() != M.Colours.size()) Fault(Name + ": a dressed wreck's coats not one a point");
					if (I > 0)
					{
						const LandHulk::Hulk &P = Aged[0].Hulks()[std::min(K, Aged[0].Hulks().size() - 1)], &Same = Y.Hulks()[K];
						bSameLay = bSameLay && Aged[0].Hulks().size() == Y.Hulks().size() && P.X == Same.X && P.Y == Same.Y && P.Model == Same.Model;
					}
				}
				Share[I] = All > 0 ? static_cast<double>(Rust) / static_cast<double>(All) : 0.0;
				Dark[I] = Rust > 0 ? Bright / (3.0 * static_cast<double>(Rust)) : 0.0;
			}
			std::printf("rust as the months go (the same twenty, each at her own age: flash and old rust, of every point): %.0f%% a week on, %.0f%% at a month, %.0f%% at six, %.0f%% at a year, %.0f%% at a year and a half; its brightness %.0f, %.0f, %.0f, %.0f, %.0f (orange browning)\n",
				100 * Share[0], 100 * Share[1], 100 * Share[2], 100 * Share[3], 100 * Share[4], Dark[0], Dark[1], Dark[2], Dark[3], Dark[4]);
			if (!bSameLay) Fault(Name + ": the same count laid otherwise at another age");
			if (!(Share[1] > Share[0] && Share[2] > Share[1] && Share[4] >= Share[2] - 0.02)) Fault(Name + ": the rust not spreading as the months go");

			// The look sheet: four of them (a thrown turret, a run-off track, a soft-skinned one, another) at each age (each
			// drawn at the column's own age), from the south-east, 30 degrees up, on their own ground and patch; and the
			// measure of it: each wreck's lit faces (the sun's light on them 0.75 and over, the faces the art director reads)
			// by what each pixel shows, its share and median sRGB, and her patch's.
			std::vector<size_t> Pick;
			const auto Choose = [&](auto Test)
			{
				for (size_t K = 0; K < Aged[0].Hulks().size(); ++K)
					if (Test(Aged[0].Hulks()[K]) && std::find(Pick.begin(), Pick.end(), K) == Pick.end())
					{
						Pick.push_back(K);
						return;
					}
			};
			Choose([](const LandHulk::Hulk& H) { return H.bTurretOff; });
			Choose([](const LandHulk::Hulk& H) { return H.TrackOff != 0; });
			Choose([](const LandHulk::Hulk& H) { return H.bSoft; });
			Choose([](const LandHulk::Hulk& H) { return !H.bSoft && !H.bTurretOff && H.TrackOff == 0; });
			for (size_t K = 0; Pick.size() < 4 && K < Aged[0].Hulks().size(); ++K)
				if (std::find(Pick.begin(), Pick.end(), K) == Pick.end()) Pick.push_back(K);
			const int CellW = 440, CellH = 330, Head = 30;
			Image Sheet(CellW * 5, Head + CellH * static_cast<int>(std::max<size_t>(1, Pick.size())));
			Sheet.Fill(0, 0, Sheet.W, Sheet.H, 34, 34, 36);
			std::string Labels;
			const V3 Sun = V3(-0.45, 0.35, 0.82).Unit();
			// (what each cell's pixels show: a wreck's coats lit and all, her patch's scorches)
			struct Tally
			{
				std::vector<uint8_t> Lit[LandHulk::Coats], Patch[LandHulk::Scorches];
				size_t LitN = 0, AllN = 0, All[LandHulk::Coats] = {}, PatchN = 0;
			};
			std::vector<std::vector<Tally>> Cells(Pick.size(), std::vector<Tally>(5));
			std::vector<Tally> Pooled(5);
			constexpr double LitFrom = 0.75;
			const auto Count = [&](Tally& T, Tally& P, int X0, int Y0)
			{
				for (int J = Y0; J < Y0 + CellH; ++J)
					for (int I = X0; I < X0 + CellW; ++I)
					{
						const size_t Px = static_cast<size_t>(J) * static_cast<size_t>(Sheet.W) + static_cast<size_t>(I);
						const int Tag = Sheet.Tag[Px];
						const uint8_t* C = &Sheet.Rgb[Px * 3];
						if (Tag >= 1 && Tag <= LandHulk::Coats)
						{
							for (Tally* Into : { &T, &P })
							{
								++Into->AllN;
								++Into->All[Tag - 1];
								if (Sheet.Lit[Px] >= LitFrom)
								{
									++Into->LitN;
									Into->Lit[Tag - 1].insert(Into->Lit[Tag - 1].end(), { C[0], C[1], C[2] });
								}
							}
						}
						else if (Tag >= 32 && Tag < 32 + LandHulk::Scorches)
						{
							for (Tally* Into : { &T, &P })
							{
								++Into->PatchN;
								Into->Patch[Tag - 32].insert(Into->Patch[Tag - 32].end(), { C[0], C[1], C[2] });
							}
						}
					}
			};
			for (size_t R = 0; R < Pick.size(); ++R)
			{
				for (int I = 0; I < 5; ++I)
				{
					const LandHulk::Yard& Y = Aged[static_cast<size_t>(I)];
					const LandHulk::Hulk& H = Y.Hulks()[Pick[R]];
					const V3 Anchor(H.X, H.Y, H.Z);
					double Cx = H.X, Cy = H.Y;
					if (H.bTurretOff)
					{
						Cx = 0.5 * (H.X + H.TurretX);
						Cy = 0.5 * (H.Y + H.TurretY);
					}
					const int X0 = I * CellW, Y0 = Head + static_cast<int>(R) * CellH;
					const View V = View::Looking(V3(Cx, Cy, H.Z + 0.8), 2.35, 0.52, H.bTurretOff || H.TrackOff != 0 ? 26.0 : 34.0, X0, Y0, X0 + CellW, Y0 + CellH);
					Draw(Sheet, V, GroundAround(G, Cx, Cy, 16.0, 0.4, Anchor), Anchor, Sun);
					LandField::Mesh M;
					LandHulk::Dressing Nd;
					Nd.Months = Ages[I];
					Y.Patch(Pick[R], M, &Nd);
					Draw(Sheet, V, M, Anchor, Sun, &Nd.Coats, 32);
					Y.Dress(Pick[R], true, M, &Nd);
					Draw(Sheet, V, M, Anchor, Sun, &Nd.Coats, 1);
					for (int E = 0; E < CellW; ++E) Sheet.Put(X0 + E, Y0, 34, 34, 36);
					for (int E = 0; E < CellH; ++E) Sheet.Put(X0, Y0 + E, 34, 34, 36);
					Count(Cells[R][static_cast<size_t>(I)], Pooled[static_cast<size_t>(I)], X0, Y0);
					if (R == 0) Labels += std::to_string(X0 + 8) + " 8 " + AgeWords[I] + "\n";
					if (I == 0)
					{
						std::string Who = Describe(Aged[0].Hulks()[Pick[R]]);
						// (her own age out of it: each column draws her at its age)
						const size_t Comma = Who.find(", ");
						const size_t Next = Comma == std::string::npos ? std::string::npos : Who.find(", ", Comma + 2);
						if (Next != std::string::npos) Who.erase(Comma, Next - Comma);
						Labels += std::to_string(X0 + 6) + " " + std::to_string(Y0 + CellH - 20) + " " + Who.substr(0, 60) + " (each column her age)\n";
					}
				}
			}
			Sheet.Save(OutDir + "/look_" + Name + ".ppm");
			std::ofstream(OutDir + "/look_" + Name + ".txt") << Labels;
			// The measure, as a table (and a .csv beside the picture).
			const auto Median = [](std::vector<uint8_t> V, int Channel)
			{
				std::vector<uint8_t> One;
				for (size_t K = static_cast<size_t>(Channel); K < V.size(); K += 3) One.push_back(V[K]);
				if (One.empty()) return -1;
				std::nth_element(One.begin(), One.begin() + static_cast<std::ptrdiff_t>(One.size() / 2), One.end());
				return static_cast<int>(One[One.size() / 2]);
			};
			std::ofstream Csv(OutDir + "/look_" + Name + ".csv");
			Csv << "recording,wreck,age,what,class,share_lit,share_all,r,g,b\n";
			const auto Row = [&](const Tally& T, const std::string& Who, int I, bool bPrint) -> std::string
			{
				std::string Line;
				char Buf[160];
				for (int C = 0; C < LandHulk::Coats; ++C)
				{
					const size_t N = T.Lit[C].size() / 3;
					const double Of = T.LitN > 0 ? 100.0 * static_cast<double>(N) / static_cast<double>(T.LitN) : 0.0;
					const double OfAll = T.AllN > 0 ? 100.0 * static_cast<double>(T.All[C]) / static_cast<double>(T.AllN) : 0.0;
					const int R = Median(T.Lit[C], 0), Gm = Median(T.Lit[C], 1), B = Median(T.Lit[C], 2);
					Csv << Name << "," << Who << "," << AgeWords[I] << ",wreck lit," << LandHulk::CoatName(static_cast<LandHulk::Coat>(C)) << "," << Of << "," << OfAll << "," << R << "," << Gm
						<< "," << B << "\n";
					if (N == 0) continue;
					std::snprintf(Buf, sizeof(Buf), "  %s %.0f%% (%d,%d,%d)", LandHulk::CoatName(static_cast<LandHulk::Coat>(C)), Of, R, Gm, B);
					Line += Buf;
				}
				std::string Ground;
				for (int C = 1; C < LandHulk::Scorches; ++C)
				{
					const size_t N = T.Patch[C].size() / 3;
					const double Of = T.PatchN > 0 ? 100.0 * static_cast<double>(N) / static_cast<double>(T.PatchN) : 0.0;
					const int R = Median(T.Patch[C], 0), Gm = Median(T.Patch[C], 1), B = Median(T.Patch[C], 2);
					Csv << Name << "," << Who << "," << AgeWords[I] << ",patch," << LandHulk::ScorchName(static_cast<LandHulk::Scorch>(C)) << "," << Of << ",," << R << "," << Gm << ","
						<< B << "\n";
					if (N == 0 || Of < 0.5) continue;
					std::snprintf(Buf, sizeof(Buf), "  %s %.0f%% (%d,%d,%d)", LandHulk::ScorchName(static_cast<LandHulk::Scorch>(C)), Of, R, Gm, B);
					Ground += Buf;
				}
				if (bPrint)
				{
					std::printf("     %-18s lit %5zu px:%s\n", AgeWords[I], T.LitN, Line.c_str());
					std::printf("     %-18s patch %5zu px:%s\n", "", T.PatchN, Ground.empty() ? "  the field's own" : Ground.c_str());
				}
				return Line;
			};
			std::printf("the look, measured on its sheet (lit faces: the sun's light on them %.2f and over; each coat's share of her lit pixels and its median sRGB in the picture; her patch's, of its pixels shown):\n", LitFrom);
			for (size_t R = 0; R < Pick.size(); ++R)
			{
				std::printf("  %zu %s\n", R + 1, Describe(Aged[0].Hulks()[Pick[R]]).c_str());
				for (int I = 0; I < 5; ++I) Row(Cells[R][static_cast<size_t>(I)], std::to_string(R + 1) + " " + Aged[0].Hulks()[Pick[R]].Model, I, true);
			}
			std::printf("  all four, against the art director's numbers (24 November):\n");
			const char* Wants[] = {
				"soot 25-40 over about half; scorched grey-buff 100-110; bare grey 85-95; her paint low on the cool sides; orange only on edges",
				"flash rust about 130,70,38 on the bare steel only, 15-25% of her, with runs",
				"old rust about 80-90,52-56,38-42 (red at most 2.2 times blue); soot off the sky faces; the paint flaking, some staying",
				"as six months", "as six months" };
			for (int I = 0; I < 5; ++I)
			{
				std::printf("    %s (wanted: %s)\n", AgeWords[I], Wants[I]);
				Row(Pooled[static_cast<size_t>(I)], "all four", I, true);
			}
			// (loose guards, so a later change that loses her patchwork is caught: a week on soot over a third to two thirds
			// and next to no orange, a month on some rust, the old rust's red no more than 2.2 times its blue)
			const auto LitShare = [](const Tally& T, LandHulk::Coat C) { return T.LitN > 0 ? static_cast<double>(T.Lit[static_cast<int>(C)].size() / 3) / static_cast<double>(T.LitN) : 0.0; };
			const double WeekSoot = LitShare(Pooled[0], LandHulk::Coat::Soot), WeekFlash = LitShare(Pooled[0], LandHulk::Coat::Flash);
			const double MonthRust = LitShare(Pooled[1], LandHulk::Coat::Flash) + LitShare(Pooled[1], LandHulk::Coat::OldRust);
			bool bOldDull = true;
			for (int I = 2; I < 5; ++I)
			{
				const Tally& T = Pooled[static_cast<size_t>(I)];
				const int R = Median(T.Lit[static_cast<int>(LandHulk::Coat::OldRust)], 0), B = Median(T.Lit[static_cast<int>(LandHulk::Coat::OldRust)], 2);
				if (R >= 0 && B > 0 && R > 2.25 * B) bOldDull = false;
			}
			if (WeekSoot < 0.33 || WeekSoot > 0.67 || WeekFlash > 0.1 || MonthRust < 0.1 || !bOldDull)
				Fault(Name + ": the look's measure off its marks (a week on soot " + std::to_string(WeekSoot) + ", orange " + std::to_string(WeekFlash) + "; a month on rust " +
					std::to_string(MonthRust) + (bOldDull ? "" : "; the old rust too red") + ")");
		}

		// 2b. The lorries and half-tracks burnt out (the art director's note, 24 November): a lorry's canvas tilt burnt off
		// its hoops (every nation's: round 1 drew the tilt as rusting steel), her bed's planks charred and some gone, her
		// glass gone, her tyres burnt to the rims; a half-track's canvas, leather and glass likewise. Every nation's soft
		// wrecks, twenty of each laid (the stand-in), each dressed and counted against her own model.
		{
			std::string Seen;
			int Soft = 0, Wrong = 0;
			std::map<std::string, bool> Said;
			for (const char* Nat : { "GBR", "SOV", "ITA", "JAP", "GER", "USA" })
			{
				LandHulk::Ask Q = AskOf(20, 1.0);
				Q.bAsLaid = true;
				Q.Nats = { { Nat, 1 } };
				LandHulk::Yard Y;
				Y.Lay(F, Q, ReadModel, Have);
				for (size_t K = 0; K < Y.Hulks().size(); ++K)
				{
					const LandHulk::Hulk& H = Y.Hulks()[K];
					const LandKit::Model* Mo = Y.ModelOf(K);
					if (!H.bSoft || !Mo) continue;
					++Soft;
					// (her own model's canvas, leather, rope and cloth, glass, tilt and pneumatic tyres, on the bodies drawn)
					int Gone = 0, Glass = 0, Tyres = 0;
					for (const LandKit::Body& Bo : Mo->Bodies)
					{
						if (Bo.Name == "aerial" || Bo.Name.find("roof_mg") != std::string::npos || Bo.Name == "mg_mount") continue;
						const std::string& Part = Mo->Parts[static_cast<size_t>(Bo.Part)].Name;
						Tyres += Part.find("wheel_front") != std::string::npos || Part.find("wheel_back") != std::string::npos ? 1 : 0;
						for (const uint8_t P : Bo.Near.Paints)
						{
							const std::string Paint = P < Mo->PaintNames.size() ? Mo->PaintNames[P] : std::string();
							Gone += Paint == "canvas" || Paint == "leather" || Paint == "rope" || Paint == "cloth" || Paint == "torn" || Paint == "skin" ? 1 : 0;
							Glass += Paint == "glass" || Paint == "lamp" ? 1 : 0;
						}
					}
					LandField::Mesh M;
					LandHulk::Dressing Nd;
					Y.Dress(K, true, M, &Nd);
					const bool bTilt = Mo->PartIndex("tilt") >= 0;
					const bool bBed = H.Model == "Truck";
					const bool bOk = Nd.Burnt == Gone && Nd.Glass == Glass && Nd.Rims == Tyres && (!bTilt || Nd.Hoops > 0) && (!bBed || (Nd.Planks > 0 && Nd.PlanksGone > 0 && Nd.PlanksGone < Nd.Planks));
					Wrong += bOk ? 0 : 1;
					const std::string Key = H.Nation + " " + H.Model;
					if (Said[Key] && bOk) continue;
					Said[Key] = true;
					char Line[320];
					std::snprintf(Line, sizeof(Line), "%s%s: %d points of canvas, leather, rope and cloth burnt away%s, %d of glass gone, %d tyres to the rims%s%s", Seen.empty() ? "" : "; ",
						Key.c_str(), Nd.Burnt, bTilt ? (" (the tilt's hoops kept, " + std::to_string(Nd.Hoops) + " points)").c_str() : "", Nd.Glass, Nd.Rims,
						bBed ? (", her bed's " + std::to_string(Nd.Planks) + " planks charred, " + std::to_string(Nd.PlanksGone) + " burnt through").c_str() : "", bOk ? "" : " (WRONG)");
					Seen += Line;
				}
			}
			std::printf("lorries and half-tracks burnt out, every nation's (%d soft wrecks): %s\n", Soft, Seen.c_str());
			if (Soft == 0 || Wrong > 0) Fault(Name + ": " + std::to_string(Wrong) + " soft wrecks with their canvas, glass, bed or tyres not burnt as asked");
		}

		// 3. Fewer as the months go (twenty burnt out: 20, 17, 10, 3 left), each nation's share by the nats, the cap.
		{
			const double Ages[] = { 1.0, 6.0, 12.0, 24.0, 60.0 };
			int Was = 1 << 20;
			std::string Seen;
			bool bFalling = true;
			for (const double M : Ages)
			{
				LandHulk::Yard Y;
				const int N = Y.Lay(F, AskOf(20, M), ReadModel, Have);
				Seen += std::to_string(N) + "/" + std::to_string(LandHulk::Kept(20, M)) + " at " + std::to_string(static_cast<int>(M)) + " months; ";
				if (N > Was || (N == Was && N > 1)) bFalling = false;
				if (N > LandHulk::Kept(20, M)) Fault(Name + ": more laid than the salvage left");
				Was = N;
			}
			std::printf("twenty burnt out, laid of left: %s%s\n", Seen.c_str(), bFalling ? "falling" : "NOT falling");
			if (!bFalling) Fault(Name + ": the wrecks not fewer as the months go");
			LandHulk::Ask Q = AskOf(40, 1.0);
			Q.Nats = { { "GER", 30 }, { "USA", 10 } };
			LandHulk::Yard Y;
			Y.Lay(F, Q, ReadModel, Have);
			int Ger = 0, Usa = 0;
			for (const LandHulk::Hulk& H : Y.Hulks()) (H.Nation == "GER" ? Ger : Usa) += 1;
			std::printf("nats GER 30, USA 10 (twenty kept): %d German, %d American\n", Ger, Usa);
			if (Y.NoRoom == 0 && (Ger != 15 || Usa != 5)) Fault(Name + ": the nations not shared as the nats give them");
			LandHulk::Yard Most;
			Most.Lay(F, AskOf(LandHulk::MostCount + 40, 0.0), ReadModel, Have);
			if (Most.Wanted != LandHulk::MostCount || static_cast<int>(Most.Hulks().size()) > LandHulk::Most) Fault(Name + ": the cap not kept");
			std::printf("the cap: %d asked, %d counted, %zu laid (at most %d)\n", LandHulk::MostCount + 40, Most.Wanted, Most.Hulks().size(), LandHulk::Most);
		}

		// 4. With nothing asked (no key, no switch; the key there but its count 0; a live battle with no tracks yet),
		// none, and nothing covered.
		{
			LandHulk::Yard None;
			const int N0 = None.Lay(F, AskOf(0, 6.0), ReadModel, Have);
			LandHulk::Field Bare = F;
			Bare.Fought.clear();
			LandHulk::Yard Live;
			const int N1 = Live.Lay(Bare, AskOf(30, 1.0), ReadModel, Have);
			int Covered = 0;
			for (const auto& P : F.Fought) Covered += None.Covers(P.first, P.second) || Live.Covers(P.first, P.second) ? 1 : 0;
			std::printf("none asked: %d laid, %d read, %d places covered; a live battle (no tracks at its open): %d laid\n", N0, None.Read, Covered, N1);
			if (N0 != 0 || N1 != 0 || Covered != 0 || None.Read != 0) Fault(Name + ": wrecks with none asked");
		}

		// 5. The battle's own models taken only for wrecks that burnt this month (dressed for its coats and kit); a month
		// older, each read and dressed for hers. And the Mac's stand-in (-TheaterLandOldWrecks=N): N laid, at most 20,
		// whatever the months.
		{
			const auto Own = [&](const LandHulk::Yard& Y, int& Drawn, int& Held)
			{
				Drawn = Held = 0;
				for (size_t K = 0; K < Y.Hulks().size(); ++K)
				{
					const LandKit::Model* Battle = Have(Y.Hulks()[K].Model, Y.Hulks()[K].Nation);
					Held += Battle ? 1 : 0;
					Drawn += Battle && Y.ModelOf(K) == Battle ? 1 : 0;
				}
			};
			LandHulk::Yard Now;
			Now.Lay(F, AskOf(40, 0.0), ReadModel, Have);
			int NowDrawn = 0, NowHeld = 0, OldDrawn = 0, OldHeld = 0;
			Own(Now, NowDrawn, NowHeld);
			Own(Y1, OldDrawn, OldHeld);
			std::printf("the battle's own models: %d of the %d wrecks of this month whose model it holds drawn from them (%d read); %d of %d a month older (%d read, each dressed for her month)\n",
				NowDrawn, NowHeld, Now.Read, OldDrawn, OldHeld, Y1.Read);
			if (NowDrawn != NowHeld || OldDrawn != 0) Fault(Name + ": the battle's own models taken for a wreck of another month, or not for one of this");
			LandHulk::Ask Q = AskOf(12, 18.0);
			Q.bAsLaid = true;
			LandHulk::Yard Stand;
			const int Twelve = Stand.Lay(F, Q, ReadModel, Have);
			Q.Count = 40;
			LandHulk::Yard Forty;
			const int Capped = Forty.Lay(F, Q, ReadModel, Have);
			const std::string Said = Stand.Said();
			std::printf("the stand-in, 18 months on: =12 lays %d (as burnt out, %d), =40 lays %d (at most %d): \"%s\"\n", Twelve, LandHulk::Kept(12, 18.0), Capped, LandHulk::Most,
				Said.substr(0, Said.find("field)") + 6).c_str());
			if ((Twelve != 12 && Stand.NoRoom == 0) || Stand.Wanted != 12 || Capped > LandHulk::Most || (Capped != LandHulk::Most && Forty.NoRoom == 0))
				Fault(Name + ": the stand-in not laying as many as it asks");

			// (the key at the battle's scale: 60 burnt out at 7.5 real machines to a drawn tank lays 8, and says so; the
			// stand-in reads no scale, so =12 still lays 12)
			LandHulk::Ask Scaled = AskOf(60, 1.0);
			Scaled.Per = 7.5;
			LandHulk::Yard Eight;
			const int AtScale = Eight.Lay(F, Scaled, ReadModel, Have);
			Q.Count = 12;
			Q.Per = 10.0;
			LandHulk::Yard StandScaled;
			StandScaled.Lay(F, Q, ReadModel, Have);
			const std::string SaidScaled = Eight.Said();
			std::printf("the battle's scale on the field: 60 burnt out at 7.5 to a wreck lays %d of %d; the stand-in =12 with a scale wants %d: \"%s\"\n", AtScale, Eight.Wanted,
				StandScaled.Wanted, SaidScaled.substr(0, SaidScaled.find("field)") + 6).c_str());
			if (Eight.Wanted != 8 || (AtScale != 8 && Eight.NoRoom == 0) || StandScaled.Wanted != 12 || SaidScaled.find("one for every 7.5") == std::string::npos)
				Fault(Name + ": the battle's scale not drawn, or the stand-in scaled");
		}

		// 6. The field telephones' wires (release 2; as the scene's -TheaterLandPhones makes them from the battle, before
		// the wrecks are laid): none laid over one (her outline WireClear off it, her corners a little past her rings).
		{
			LandFeed::Recording Phoned = Rec;
			if (Phoned.Wires.empty()) LandSight::PhonesFromBattle(Phoned);
			const LandHulk::Field Wired = LandHulk::FieldOf(Phoned, G, nullptr, {});
			Near WireNear;
			for (const Line& L : Wired.Wires) WireNear.AddLine(L, 0.25);
			LandHulk::Yard YW;
			YW.Lay(Wired, AskOf(40, 1.0), ReadModel, Have);
			double WireMin = 30.0, WasMin = 30.0;
			for (const Spot& S : SpotsOf(YW, 3)) WireMin = std::min(WireMin, WireNear.To(S.X, S.Y, 30.0));
			for (const Spot& S : SpotsOf(Y1, 3)) WasMin = std::min(WasMin, WireNear.To(S.X, S.Y, 30.0));
			std::printf("with the field telephones (%zu wires): %zu laid; nearest a wire %.1f m (laid without them, %.1f m)\n", Wired.Wires.size(), YW.Hulks().size(), WireMin, WasMin);
			if (WireMin < LandHulk::WireClear - 0.5) Fault(Name + ": a wreck lying over a field telephone's wire");
		}

		// 7. A winter field (the same ground with snow lying, as the Ardennes in December), six months on: her patch under
		// the snow fallen since she burnt (the field's own colour round it), the earth and the weeds' stain off her low
		// parts, and snow on what faces up on her; against the same field in summer.
		{
			LandField::Field Winter = Words;
			Winter.Snow = 0.9;
			const LandField::Ground GW(Winter);
			const LandHulk::Field FW = LandHulk::FieldOf(Rec, GW, nullptr, {});
			const auto Look = [](const LandHulk::Yard& Y, const LandField::Ground& On, double& PatchOff, double& UpWhite)
			{
				double Off = 0.0, Up = 0.0, White = 0.0;
				size_t N = 0;
				for (size_t K = 0; K < Y.Hulks().size(); ++K)
				{
					const LandHulk::Hulk& Hk = Y.Hulks()[K];
					LandField::Mesh M;
					Y.Patch(K, M);
					for (size_t I = 0; I < M.Colours.size(); ++I)
					{
						const LandField::Rgb8 C = M.Colours[I], Fc = On.ColourAt(Hk.X + M.Positions[I * 3], Hk.Y + M.Positions[I * 3 + 1]);
						Off += std::fabs((C.R + C.G + C.B) - (Fc.R + Fc.G + Fc.B)) / 3.0;
						++N;
					}
					Y.Dress(K, true, M);
					for (size_t I = 0; I < M.Colours.size(); ++I)
					{
						if (M.Normals[I * 3 + 2] < 0.85f) continue;
						++Up;
						White += std::min({ M.Colours[I].R, M.Colours[I].G, M.Colours[I].B }) > 190 ? 1.0 : 0.0;
					}
				}
				PatchOff = N > 0 ? Off / static_cast<double>(N) : 0.0;
				UpWhite = Up > 0.0 ? White / Up : 0.0;
			};
			LandHulk::Yard Summer, Snowy;
			Summer.Lay(F, AskOf(40, 6.0), ReadModel, Have);
			Snowy.Lay(FW, AskOf(40, 6.0), ReadModel, Have);
			double SummerOff = 0.0, SummerWhite = 0.0, SnowOff = 0.0, SnowWhite = 0.0;
			Look(Summer, G, SummerOff, SummerWhite);
			Look(Snowy, GW, SnowOff, SnowWhite);
			std::printf("a winter field (snow lying %.2f), six months on: %zu laid; her patch off the field's own colour by %.0f on average (in summer %.0f); %.0f%% of what faces up on her white (in summer %.0f%%)\n",
				GW.SnowLying(), Snowy.Hulks().size(), SnowOff, SummerOff, 100.0 * SnowWhite, 100.0 * SummerWhite);
			if (GW.SnowLying() > 0.5 && (SnowOff > 0.5 * SummerOff || SnowWhite < 0.4 || SummerWhite > 0.05)) Fault(Name + ": the wrecks not under the winter's snow");
		}

		// 8. An old field: two battles' scars and two of old ruts laid first, as the scene lays them; none on a hole or
		// a rut.
		LandCrater::Field Holes;
		Holes.Begin([&Rec](double X, double Y) { return Rec.Ground.HeightAt(X, Y); }, Rec.Seed ? Rec.Seed : 1);
		Holes.Dig = LandCrater::Field::DigOn(G);
		std::vector<Line> Ruts;
		{
			std::vector<std::pair<double, double>> Gates;
			for (const LandField::HedgeOpening& O : G.Openings()) Gates.emplace_back(O.X, O.Y);
			const LandCrater::Field* HolesPtr = &Holes;
			LandCrater::LayScars(Holes, F.Fought, 2, 6.0, Rec.Seed ? Rec.Seed : 1,
				[Gp, HolesPtr](double X, double Y) -> std::string
				{
					const LandField::Surface Under = Gp->At(X, Y);
					if (Under.What == LandField::Drawn::Floor || Under.What == LandField::Drawn::Wall || Under.What == LandField::Drawn::Water || Under.What == LandField::Drawn::Marsh) return std::string();
					if (HolesPtr->Dig && !HolesPtr->Dig(X, Y)) return std::string();
					return std::string(Under.Fx);
				},
				F.Lines);
			const auto DriveOpen = [Gp](double X, double Y)
			{
				LandField::Drawn What = LandField::Drawn::Pasture;
				Gp->HeightAt(X, Y, &What);
				return What == LandField::Drawn::Pasture || What == LandField::Drawn::Crop || What == LandField::Drawn::Rough || What == LandField::Drawn::Works || What == LandField::Drawn::Beyond;
			};
			Ruts = LandCrater::OldWays(F.Fought, 2, Rec.Seed ? Rec.Seed : 1, Gates, F.bAxis ? F.Axis : std::nan(""), DriveOpen);
		}
		const LandHulk::Field Old = LandHulk::FieldOf(Rec, G, &Holes, Ruts);
		LandHulk::Yard YOld;
		YOld.Lay(Old, AskOf(40, 1.0), ReadModel, Have);
		std::printf("on an old field (%d old holes, %zu old ways): %s\n", Holes.Count(), Ruts.size(), YOld.Said().c_str());
		CheckPlaces(YOld, Name + ", an old field", &Holes, &Ruts);

		// The field from above: the ground as drawn (its kinds marked), the recorded ways, this battle's blasts, and the
		// wrecks dressed and drawn to scale, each ringed in its nation's colour and numbered (the key beside it).
		{
			double Wx0 = 1e9, Wx1 = -1e9, Wy0 = 1e9, Wy1 = -1e9;
			for (const LandHulk::Hulk& H : Y1.Hulks())
			{
				Wx0 = std::min(Wx0, H.X);
				Wx1 = std::max(Wx1, H.X);
				Wy0 = std::min(Wy0, H.Y);
				Wy1 = std::max(Wy1, H.Y);
			}
			if (Y1.Hulks().empty())
			{
				Wx0 = Wy0 = 0.0;
				Wx1 = Wy1 = 500.0;
			}
			const double Span = std::max(Wx1 - Wx0, Wy1 - Wy0) + 120.0, Mx = 0.5 * (Wx0 + Wx1), My = 0.5 * (Wy0 + Wy1);
			const int Size = 1400, Key = 560;
			const double Scale = Size / Span;
			Image Map(Size + Key, Size);
			Map.ClipW = Size;
			Map.Fill(Size, 0, Size + Key, Size, 30, 30, 32);
			const auto ToPx = [&](double X, double Y, double& Px, double& Py)
			{
				Px = (X - (Mx - 0.5 * Span)) * Scale;
				Py = ((My + 0.5 * Span) - Y) * Scale;
			};
			for (int J = 0; J < Size; ++J)
				for (int I = 0; I < Size; ++I)
				{
					const double X = Mx - 0.5 * Span + (I + 0.5) / Scale, Y = My + 0.5 * Span - (J + 0.5) / Scale;
					LandField::Drawn What = LandField::Drawn::Pasture;
					G.HeightAt(X, Y, &What);
					const LandField::Rgb8 C = G.ColourAt(X, Y);
					double R = C.R, Gr = C.G, B = C.B;
					const auto Tint = [&](double Tr, double Tg, double Tb, double Amount) { R += (Tr - R) * Amount; Gr += (Tg - Gr) * Amount; B += (Tb - B) * Amount; };
					switch (What)
					{
					case LandField::Drawn::Lane: Tint(214, 196, 150, 0.85); break;
					case LandField::Drawn::Bank: Tint(40, 66, 34, 0.8); break;
					case LandField::Drawn::Floor: Tint(150, 70, 60, 0.9); break;
					case LandField::Drawn::Wall: Tint(120, 120, 120, 0.9); break;
					case LandField::Drawn::Village:
					case LandField::Drawn::Yard: Tint(170, 150, 120, 0.45); break;
					case LandField::Drawn::Woods: Tint(30, 56, 30, 0.75); break;
					case LandField::Drawn::Water: Tint(60, 110, 170, 0.9); break;
					case LandField::Drawn::Marsh: Tint(80, 120, 110, 0.7); break;
					case LandField::Drawn::Crop:
						if (G.CornAt(X, Y)) Tint(222, 196, 96, 0.55);
						break;
					default: break;
					}
					// (faded, so the wrecks and the ways read over it)
					Map.Put(I, J, 0.62 * R + 40.0, 0.62 * Gr + 40.0, 0.62 * B + 40.0);
				}
			for (const Line& L : F.Men)
				for (size_t K = 0; K + 1 < L.size(); K += 2)
				{
					double Ax, Ay, Bx, By;
					ToPx(L[K].first, L[K].second, Ax, Ay);
					ToPx(L[K + 1].first, L[K + 1].second, Bx, By);
					Map.Segment(Ax, Ay, Bx, By, 250, 250, 250, 0.12);
				}
			for (const Line& L : F.Vehicles)
				for (size_t K = 0; K + 1 < L.size(); ++K)
				{
					double Ax, Ay, Bx, By;
					ToPx(L[K].first, L[K].second, Ax, Ay);
					ToPx(L[K + 1].first, L[K + 1].second, Bx, By);
					Map.Segment(Ax, Ay, Bx, By, 20, 20, 20, 0.75);
					Map.Segment(Ax + 1, Ay, Bx + 1, By, 20, 20, 20, 0.75);
				}
			for (const auto& P : F.Blasts)
			{
				double Px, Py;
				ToPx(P.first, P.second, Px, Py);
				Map.Blend(static_cast<int>(Px), static_cast<int>(Py), 90, 40, 20, 0.6);
			}
			const View Top = View::Looking(V3(Mx, My, 0.0), 0.0, 0.5 * 3.14159265358979, Scale, 0, 0, Size, Size);
			const V3 Sun = V3(-0.45, 0.35, 0.82).Unit();
			std::string Labels;
			int Line0 = 16;
			Labels += std::to_string(Size + 14) + " " + std::to_string(Line0) + " " + Name + ": 40 burnt out here a month ago, the battle's two sides'\n";
			Line0 += 20;
			{
				// (the whole of what was laid, broken at the spaces to the key's width rather than cut off)
				const std::string Whole = Y1.Said();
				size_t From = 0;
				while (From < Whole.size())
				{
					size_t To = std::min(Whole.size(), From + 84);
					if (To < Whole.size())
					{
						const size_t Space = Whole.rfind(' ', To);
						if (Space != std::string::npos && Space > From) To = Space;
					}
					Labels += std::to_string(Size + 14) + " " + std::to_string(Line0) + " " + Whole.substr(From, To - From) + "\n";
					Line0 += 18;
					From = To < Whole.size() && Whole[To] == ' ' ? To + 1 : To;
				}
			}
			Line0 += 8;
			for (size_t K = 0; K < Y1.Hulks().size(); ++K)
			{
				const LandHulk::Hulk& H = Y1.Hulks()[K];
				LandField::Mesh M;
				Y1.Patch(K, M);
				Draw(Map, Top, M, V3(H.X, H.Y, H.Z), Sun);
				Y1.Dress(K, true, M);
				Draw(Map, Top, M, V3(H.X, H.Y, H.Z), Sun);
				double R, Gr, B, Px, Py;
				NationColour(H.Nation, R, Gr, B);
				ToPx(H.X, H.Y, Px, Py);
				Map.Ring(Px, Py, 16.0, 1.6, R, Gr, B);
				if (H.bTurretOff)
				{
					double Tx, Ty;
					ToPx(H.TurretX, H.TurretY, Tx, Ty);
					Map.Ring(Tx, Ty, 5.0, 1.0, 255, 255, 255);
				}
				Labels += std::to_string(static_cast<int>(Px) + 15) + " " + std::to_string(static_cast<int>(Py) - 26) + " " + std::to_string(K + 1) + "\n";
				Labels += std::to_string(Size + 14) + " " + std::to_string(Line0) + " " + std::to_string(K + 1) + "  " + Describe(H) + ", group " + std::to_string(H.Group + 1) + "\n";
				Line0 += 18;
			}
			// (a scale bar, 100 m)
			Map.Fill(20, Size - 30, 20 + static_cast<int>(100.0 * Scale), Size - 26, 255, 255, 255);
			Labels += "20 " + std::to_string(Size - 52) + " 100 m\n";
			Line0 += 14;
			const char* Keys[] = { "rings: GER blue, USA green, SOV red, GBR yellow; a white ring a thrown turret", "dark lines the recorded vehicles' ways, faint white the men's",
				"pale tan lanes, dark green banks and woods, red-brown houses, gold standing corn", "the wrecks drawn to scale from their own dressed meshes, from above" };
			for (const char* K : Keys)
			{
				Labels += std::to_string(Size + 14) + " " + std::to_string(Line0) + " " + K + "\n";
				Line0 += 18;
			}
			// (the close looks: four of them from the south-east, under the key)
			const int Close = 260;
			for (size_t K = 0; K < std::min<size_t>(4, Y1.Hulks().size()); ++K)
			{
				const LandHulk::Hulk& H = Y1.Hulks()[K];
				const int X0p = Size + static_cast<int>(K % 2) * Close + 20, Y0p = Size - 2 * Close - 20 + static_cast<int>(K / 2) * Close;
				const V3 Anchor(H.X, H.Y, H.Z);
				const View V = View::Looking(V3(H.X, H.Y, H.Z + 0.8), 2.35, 0.52, 20.0, X0p, Y0p, X0p + Close - 6, Y0p + Close - 6);
				Draw(Map, V, GroundAround(G, H.X, H.Y, 14.0, 0.5, Anchor), Anchor, Sun);
				LandField::Mesh M;
				Y1.Patch(K, M);
				Draw(Map, V, M, Anchor, Sun);
				Y1.Dress(K, true, M);
				Draw(Map, V, M, Anchor, Sun);
				Labels += std::to_string(X0p + 4) + " " + std::to_string(Y0p + 4) + " " + std::to_string(K + 1) + "\n";
			}
			Map.Save(OutDir + "/placement_" + Name + ".ppm");
			std::ofstream(OutDir + "/placement_" + Name + ".txt") << Labels;

			// Her run-off track close (the art director's carry, 24 November: not a dead-straight ladder): each wreck with a
			// track off from above and from low on the side it lies, a week on.
			std::vector<size_t> Tracked;
			for (size_t K = 0; K < Y1.Hulks().size() && Tracked.size() < 4; ++K)
				if (Y1.Hulks()[K].TrackOff != 0) Tracked.push_back(K);
			if (!Tracked.empty())
			{
				const int TrackW = 520, TrackH = 360;
				Image Close2(TrackW * 2, TrackH * static_cast<int>(Tracked.size()));
				std::string Said;
				for (size_t R = 0; R < Tracked.size(); ++R)
				{
					const size_t K = Tracked[R];
					const LandHulk::Hulk& H = Y1.Hulks()[K];
					double Sx = 0.0, Sy = 0.0;
					for (const auto& P : H.Strip)
					{
						Sx += P.first / static_cast<double>(H.Strip.size());
						Sy += P.second / static_cast<double>(H.Strip.size());
					}
					const double Cx = 0.5 * (Sx + H.X), Cy = 0.5 * (Sy + H.Y);
					const V3 Anchor(H.X, H.Y, H.Z);
					LandField::Mesh Ground = GroundAround(G, Cx, Cy, 20.0, 0.25, Anchor), Scorched, Her;
					LandHulk::Dressing Nd;
					Nd.Months = 0.25;
					Y1.Patch(K, Scorched, &Nd);
					Y1.Dress(K, true, Her, &Nd);
					const int Y0 = static_cast<int>(R) * TrackH;
					// (from above, north up; and from low on her track's side, looking across it at her)
					const View Above = View::Looking(V3(Cx, Cy, H.Z), 0.0, 1.5707, 34.0, 0, Y0, TrackW, Y0 + TrackH);
					const View Low = View::Looking(V3(Sx, Sy, H.Z + 0.3), std::atan2(Sx - H.X, Sy - H.Y), 0.3, 40.0, TrackW, Y0, 2 * TrackW, Y0 + TrackH);
					for (const View* V : { &Above, &Low })
					{
						Draw(Close2, *V, Ground, Anchor, Sun);
						Draw(Close2, *V, Scorched, Anchor, Sun);
						Draw(Close2, *V, Her, Anchor, Sun);
					}
					for (int E = 0; E < 2 * TrackW; ++E) Close2.Put(E, Y0, 34, 34, 36);
					for (int E = 0; E < TrackH; ++E) Close2.Put(TrackW, Y0 + E, 34, 34, 36);
					Said += "8 " + std::to_string(Y0 + 8) + " " + Describe(H) + ": from above (north up), " + std::to_string(H.Fold) + " links flat, " + std::to_string(H.Curl)
						+ " turned over, " + std::to_string(static_cast<int>(H.Strip.size()) - H.Fold - H.Curl) + " doubled back on top\n";
					Said += std::to_string(TrackW + 8) + " " + std::to_string(Y0 + 8) + " from low on the side her track lies, a week on\n";
				}
				Close2.Save(OutDir + "/track_" + Name + ".ppm");
				std::ofstream(OutDir + "/track_" + Name + ".txt") << Said;
			}
			std::printf("pictures: %s/placement_%s.ppm, %s/look_%s.ppm%s (their words beside them, .txt)\n", OutDir.c_str(), Name.c_str(), OutDir.c_str(), Name.c_str(),
				Tracked.empty() ? "" : (", " + OutDir + "/track_" + Name + ".ppm").c_str());
		}
	}
	std::printf("\n%s (%d faults)\n", Faults == 0 ? "old wrecks: all checks passed" : "old wrecks: FAILED", Faults);
	return Faults == 0 ? 0 : 1;
}
