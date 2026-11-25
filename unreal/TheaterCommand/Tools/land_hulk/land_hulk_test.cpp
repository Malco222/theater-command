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
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
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
		/** What each pixel shows, for the measure (0 untagged; 1 + a wreck's Coat; 32 + her patch's Scorch), how lit it is
		 * (the sun's light on it, 0.42 to 1), how much its face looks up (its facing's z, toward the eye), and how high it
		 * stands over the mesh's origin (a wreck's anchor: her height over the ground under her middle, metres); and whether
		 * its triangle's three corners all carry its tag (the coat itself, not drawn mixed with the one beside it). */
		std::vector<uint8_t> Tag, Pure;
		std::vector<float> Lit, Up, Hgt;
		/** The box lines and rings are drawn in (the map's, not its key's). */
		int ClipW = 0, ClipH = 0;
		Image(int InW, int InH)
			: W(InW), H(InH), Rgb(static_cast<size_t>(InW) * static_cast<size_t>(InH) * 3, 0), Depth(static_cast<size_t>(InW) * static_cast<size_t>(InH), 1e30f),
			  Tag(static_cast<size_t>(InW) * static_cast<size_t>(InH), 0), Pure(static_cast<size_t>(InW) * static_cast<size_t>(InH), 0), Lit(static_cast<size_t>(InW) * static_cast<size_t>(InH), 0.0f),
			  Up(static_cast<size_t>(InW) * static_cast<size_t>(InH), 0.0f), Hgt(static_cast<size_t>(InW) * static_cast<size_t>(InH), 0.0f), ClipW(InW), ClipH(InH)
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
					Img.Up[Px0] = static_cast<float>(Nm.Z);
					Img.Hgt[Px0] = static_cast<float>(W0 * M.Positions[A * 3 + 2] + W1 * M.Positions[B * 3 + 2] + W2 * M.Positions[C * 3 + 2]);
					Img.Tag[Px0] = bTags ? static_cast<uint8_t>(TagBase + (*Tags)[W0 >= W1 && W0 >= W2 ? A : W1 >= W2 ? B : C]) : 0;
					Img.Pure[Px0] = bTags && (*Tags)[A] == (*Tags)[B] && (*Tags)[B] == (*Tags)[C] ? 1 : 0;
					const LandField::Rgb8 &Ca = M.Colours[A], &Cb = M.Colours[B], &Cc = M.Colours[C];
					Img.Put(I, J, Light * (W0 * Ca.R + W1 * Cb.R + W2 * Cc.R), Light * (W0 * Ca.G + W1 * Cb.G + W2 * Cc.G), Light * (W0 * Ca.B + W1 * Cb.B + W2 * Cc.B));
				}
		}
	}

	/** A pixel's class by its colour alone, as the art director reads a picture (and as the review of round 2 measured
	 * it): dark (no channel over 55: soot, char), bright orange (flash rust), brown rust, pale grey-buff (grey, 97 and
	 * over: scorched paint), darker grey (bare steel, paint), and the rest. */
	enum class Hue : int { Dark, Orange, BrownRust, PaleGrey, Grey, Other };
	constexpr int Hues = 6;
	const char* HueName(int H)
	{
		static const char* Names[Hues] = { "dark", "orange", "brown rust", "pale grey-buff", "grey", "other" };
		return H >= 0 && H < Hues ? Names[H] : "?";
	}
	int HueOf(const uint8_t* C)
	{
		const int R = C[0], G = C[1], B = C[2];
		const int Hi = std::max({ R, G, B }), Lo = std::min({ R, G, B });
		if (Hi <= 55) return static_cast<int>(Hue::Dark);
		if (R >= 105 && R >= 2.6 * std::max(B, 1) && R - G >= 35) return static_cast<int>(Hue::Orange);
		if (R >= 60 && R - G >= 22 && R >= 1.6 * std::max(B, 1)) return static_cast<int>(Hue::BrownRust);
		if (Hi - Lo <= 30) return static_cast<int>(Hi >= 97 ? Hue::PaleGrey : Hue::Grey);
		return static_cast<int>(Hue::Other);
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
		// (bLive: laid for a live battle, which knows none of the recorded ways at its open: how near they come is told, not
		// faulted, and is the scene's guard's to clear (case 9); its lanes are kept LiveLaneClear)
		const auto CheckPlaces = [&](const LandHulk::Yard& Y, const std::string& Label, const LandCrater::Field* Holes, const std::vector<Line>* Ruts, bool bLive = false)
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
			// (her run-off track, the art director's carry of 24 November, not a dead-straight ladder: snaking, folded back on
			// itself once with the fold showing from above, its links flat on the ground, none through her hull, her road
			// wheels or the ground. Round 2 let a half-track's short track through with a sway of 5 cm: the snake now reads
			// at the game's scale, 8 cm and more off its own smooth line, and turns from one way to the other (on a
			// half-track's short track the 3 m the line is smoothed over leaves only a metre or so to count turns in); the
			// doubled end angled out off the links under it by some 15 in 100 of her track's width and more)
			int Tracks = 0, Unfolded = 0, Straight = 0, UnderHer = 0, Through = 0, TurnsLeast = 1 << 30, Hidden = 0;
			double SwayLeast = 1e9, SwayMost = 0.0, ClearLeast = 1e9, LowLeast = 1e9, HighMost = -1e9, AsideLeast = 1e9;
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
				if (Sway < 0.08 || Turns < 1) ++Straight;
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
				// (the doubled end's free link off the flat one it lies on, as a share of her track's width)
				if (Flat + static_cast<size_t>(std::max(0, Hk.Curl)) < N && HalfW > 0.0)
				{
					const size_t Last = N - 1, Under = Flat - 1 - std::min(Flat - 1, Last - Flat - static_cast<size_t>(Hk.Curl));
					const double Aside = std::hypot(Hk.Strip[Last].first - Hk.Strip[Under].first, Hk.Strip[Last].second - Hk.Strip[Under].second) / (2.0 * HalfW);
					AsideLeast = std::min(AsideLeast, Aside);
					if (Aside < 0.15) ++Hidden;
				}
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
				std::printf("%s: %d run-off tracks: %d folded back on themselves once, the doubled end's free link at least %.0f%% of her track's width off the link under it; snaking (each link off its own smooth line up to %.2f to %.2f m, turning from one way to the other at least %d times); their links' inner edges at least %.2f m off her footprint (her road wheels); their points low off her %.3f to %.2f m over the ground, %d through it\n",
					Label.c_str(), Tracks, Tracks - Unfolded, AsideLeast < 1e8 ? 100.0 * AsideLeast : 0.0, SwayLeast, SwayMost, TurnsLeast, ClearLeast, LowLeast, HighMost, Through);
				if (Unfolded > 0) Fault(Label + ": " + std::to_string(Unfolded) + " run-off tracks not folded back on themselves");
				if (Hidden > 0) Fault(Label + ": " + std::to_string(Hidden) + " run-off tracks' folds hidden from above (the doubled end straight over the links under it)");
				if (Straight > 0) Fault(Label + ": " + std::to_string(Straight) + " run-off tracks lying straight (off their own smooth line under 8 cm, or never turning from one way to the other)");
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
			if (!bLive && VehicleMin < LandHulk::VehicleClear) Fault(Label + ": a wreck within 8 m of a recorded vehicle's way");
			if (!bLive && ManMin < LandHulk::ManClear) Fault(Label + ": a wreck within 1.5 m of a recorded man's way");
			if (!bLive && LaneMin < LandHulk::LaneClear - 0.5) Fault(Label + ": a wreck within 5.5 m of a lane's middle");
			if (bLive && LaneMin < LandHulk::LiveLaneClear - 0.5) Fault(Label + ": a wreck within 14.5 m of a lane's middle (a live battle's columns drive them)");
			if (!bLive && BlastMin < LandHulk::BlastClear - 0.5) Fault(Label + ": a wreck where a shell of this battle lands");
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

			// The look sheets: four of them (a thrown turret, a run-off track, a soft-skinned one, another) at each age (each
			// drawn at the column's own age), from the south-east, 30 degrees up, on their own ground and patch; and the
			// measure of each: each wreck's lit faces (the sun's light on them 0.75 and over, the faces the art director reads)
			// by what each pixel shows and by its colour alone, its share and median sRGB, and her patch's. Three sheets: her
			// near level in the sheet's sun from the north-west (look_), her far level in the same (look_far_: what the game
			// shows beyond 140 m, where the strategy camera mostly sits; round 2 measured only the near level, and the far one
			// had turned one orange coat again), and her near level with the sun from the south-east, on the sides the camera
			// sees (look_side_: her cooler sides' paint and the rust's runs, which the first sheet shows only in shade).
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
			const V3 Sun = V3(-0.45, 0.35, 0.82).Unit();
			// (the side sheet's: from the south-east, 35 degrees up, on the sides the camera sees)
			const V3 SideSun = V3(0.6, -0.55, 0.57).Unit();
			// (what each cell's pixels show: a wreck's coats lit and all, by colour, on her sides, her patch's scorches)
			struct Tally
			{
				std::vector<uint8_t> Lit[LandHulk::Coats], Patch[LandHulk::Scorches], Side[LandHulk::Coats], ByHue[Hues];
				size_t LitN = 0, AllN = 0, All[LandHulk::Coats] = {}, PatchN = 0, PatchDark = 0, SideN = 0;
				/** Her lit faces' edges (the art director's last round: round 2's coats met at hard lines and its patches lay
				 * anywhere, so she read as camouflage): each pair of side-by-side pixels (across and down) both on her lit
				 * faces, how many step in luma by more than EdgeHard, and their steps' sum. A plate's own outline against its
				 * shade or another plate counts too, so it is read against another build's, not against none. */
				size_t EdgeN = 0, EdgeHardN = 0;
				double EdgeSum = 0.0;
				/** Her flash rust lit (the art director's carry of 24 November 21:01Z: a month on, the flash rust on a sunlit turret
				 * top read hot, almost glowing), for its brightest, not only its median: on her sunlit tops (facing z 0.5 and over),
				 * and on all her lit faces (the side sheet's sun on her sides); each, and of each the pixels drawn of the flash rust
				 * alone (Pure: not mixed with the paint beside it, whose blue and green the picture draws paler); and those lit
				 * pixels of the flash rust alone as painted, before the sun's light on them (its own mottle, not its faces' light). */
				std::vector<uint8_t> FlashTop, FlashTopPure, FlashLit, FlashLitPure, FlashCoat;
				/** Her faces by where the weather reaches them (the same carry: from half a year the old rust read as one even brown;
				 * a little of her own paint wanted low and on her sheltered faces): every pixel of hers shown, lit or in shade, on
				 * her open sides (facing z under 0.5, her upper half), low on her (facing z under 0.5, under half her height over
				 * her anchor) and on her sky faces (0.5 and over); how many, her paint left among them (her own or scorched), and
				 * lit, her paint left and her old rust. */
				enum Zones : int { Open = 0, Low = 1, Sky = 2 };
				size_t ZoneN[3] = {};
				std::vector<uint8_t> ZonePaint[3], ZoneLitPaint[3], ZoneLitRust[3];
				size_t ZoneOwn[3] = {};
				double HardShare() const { return EdgeN > 0 ? 100.0 * static_cast<double>(EdgeHardN) / static_cast<double>(EdgeN) : 0.0; }
				double MeanStep() const { return EdgeN > 0 ? EdgeSum / static_cast<double>(EdgeN) : 0.0; }
			};
			constexpr double LitFrom = 0.75;
			// (a pixel's luma as the picture shows it: its sRGB numbers weighed as Rec. 709's primaries, sRGB's own)
			constexpr double EdgeHard = 45.0;
			const auto Luma = [](const uint8_t* C) { return 0.2126 * C[0] + 0.7152 * C[1] + 0.0722 * C[2]; };
			const auto Median = [](const std::vector<uint8_t>& V, int Channel)
			{
				std::vector<uint8_t> One;
				for (size_t K = static_cast<size_t>(Channel); K < V.size(); K += 3) One.push_back(V[K]);
				if (One.empty()) return -1;
				std::nth_element(One.begin(), One.begin() + static_cast<std::ptrdiff_t>(One.size() / 2), One.end());
				return static_cast<int>(One[One.size() / 2]);
			};
			const auto PaintSheet = [&](bool bNearLevel, const V3& Light, const std::string& File, const std::string& Title, std::vector<std::vector<Tally>>& Cells,
										std::vector<Tally>& Pooled)
			{
				Cells.assign(Pick.size(), std::vector<Tally>(5));
				Pooled.assign(5, Tally());
				Image Sheet(CellW * 5, Head + CellH * static_cast<int>(std::max<size_t>(1, Pick.size())));
				Sheet.Fill(0, 0, Sheet.W, Sheet.H, 34, 34, 36);
				std::string Labels;
				const auto Count = [&](Tally& T, Tally& P, int X0, int Y0, double Tall)
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
										Into->ByHue[HueOf(C)].insert(Into->ByHue[HueOf(C)].end(), { C[0], C[1], C[2] });
										if (std::fabs(Sheet.Up[Px]) < 0.4)
										{
											++Into->SideN;
											Into->Side[Tag - 1].insert(Into->Side[Tag - 1].end(), { C[0], C[1], C[2] });
										}
										if (Tag - 1 == static_cast<int>(LandHulk::Coat::Flash))
										{
											const bool bPure = Sheet.Pure[Px] != 0, bTop = Sheet.Up[Px] >= 0.5f;
											Into->FlashLit.insert(Into->FlashLit.end(), { C[0], C[1], C[2] });
											if (bPure)
											{
												Into->FlashLitPure.insert(Into->FlashLitPure.end(), { C[0], C[1], C[2] });
												const double Un = 1.0 / static_cast<double>(Sheet.Lit[Px]);
												for (int K = 0; K < 3; ++K) Into->FlashCoat.push_back(static_cast<uint8_t>(std::min(255.0, std::round(C[K] * Un))));
											}
											if (bTop) Into->FlashTop.insert(Into->FlashTop.end(), { C[0], C[1], C[2] });
											if (bTop && bPure) Into->FlashTopPure.insert(Into->FlashTopPure.end(), { C[0], C[1], C[2] });
										}
									}
									// (her faces by where the weather reaches them, lit or in shade)
									const int Zone = Sheet.Up[Px] >= 0.5f ? Tally::Sky : Sheet.Hgt[Px] < 0.5 * Tall ? Tally::Low : Tally::Open;
									++Into->ZoneN[Zone];
									const bool bPaint = Tag - 1 == static_cast<int>(LandHulk::Coat::Paint) || Tag - 1 == static_cast<int>(LandHulk::Coat::Scorched);
									if (bPaint) Into->ZonePaint[Zone].insert(Into->ZonePaint[Zone].end(), { C[0], C[1], C[2] });
									Into->ZoneOwn[Zone] += Tag - 1 == static_cast<int>(LandHulk::Coat::Paint) ? 1 : 0;
									if (Sheet.Lit[Px] >= LitFrom && bPaint) Into->ZoneLitPaint[Zone].insert(Into->ZoneLitPaint[Zone].end(), { C[0], C[1], C[2] });
									if (Sheet.Lit[Px] >= LitFrom && Tag - 1 == static_cast<int>(LandHulk::Coat::OldRust)) Into->ZoneLitRust[Zone].insert(Into->ZoneLitRust[Zone].end(), { C[0], C[1], C[2] });
								}
								// (her lit faces' step to the pixel across and to the one down, both in the cell, hers and lit)
								if (Sheet.Lit[Px] >= LitFrom)
								{
									for (const int Across : { 1, 0 })
									{
										const int Ni = I + Across, Nj = J + 1 - Across;
										if (Ni >= X0 + CellW || Nj >= Y0 + CellH) continue;
										const size_t Nx = static_cast<size_t>(Nj) * static_cast<size_t>(Sheet.W) + static_cast<size_t>(Ni);
										if (Sheet.Tag[Nx] < 1 || Sheet.Tag[Nx] > LandHulk::Coats || !(Sheet.Lit[Nx] >= LitFrom)) continue;
										const double Step = std::fabs(Luma(C) - Luma(&Sheet.Rgb[Nx * 3]));
										for (Tally* Into : { &T, &P })
										{
											++Into->EdgeN;
											Into->EdgeHardN += Step > EdgeHard ? 1 : 0;
											Into->EdgeSum += Step;
										}
									}
								}
							}
							else if (Tag >= 32 && Tag < 32 + LandHulk::Scorches)
							{
								for (Tally* Into : { &T, &P })
								{
									++Into->PatchN;
									Into->PatchDark += std::max({ C[0], C[1], C[2] }) <= 60 ? 1 : 0;
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
						Draw(Sheet, V, GroundAround(G, Cx, Cy, 16.0, 0.4, Anchor), Anchor, Light);
						LandField::Mesh M;
						LandHulk::Dressing Nd;
						Nd.Months = Ages[I];
						Y.Patch(Pick[R], M, &Nd);
						Draw(Sheet, V, M, Anchor, Light, &Nd.Coats, 32);
						Y.Dress(Pick[R], bNearLevel, M, &Nd);
						Draw(Sheet, V, M, Anchor, Light, &Nd.Coats, 1);
						for (int E = 0; E < CellW; ++E) Sheet.Put(X0 + E, Y0, 34, 34, 36);
						for (int E = 0; E < CellH; ++E) Sheet.Put(X0, Y0 + E, 34, 34, 36);
						// (her height over her anchor, for her lower half)
						double Tall = 0.0;
						for (size_t P = 2; P < M.Positions.size(); P += 3) Tall = std::max(Tall, static_cast<double>(M.Positions[P]));
						Count(Cells[R][static_cast<size_t>(I)], Pooled[static_cast<size_t>(I)], X0, Y0, Tall);
						if (R == 0) Labels += std::to_string(X0 + 8) + " 8 " + AgeWords[I] + (I == 4 ? "   " + Title : std::string()) + "\n";
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
				Sheet.Save(OutDir + "/" + File + "_" + Name + ".ppm");
				std::ofstream(OutDir + "/" + File + "_" + Name + ".txt") << Labels;
			};
			std::vector<std::vector<Tally>> Cells, FarCells, SideCells;
			std::vector<Tally> Pooled, FarPooled, SidePooled;
			PaintSheet(true, Sun, "look", "near level (within 140 m), sun from the north-west", Cells, Pooled);
			PaintSheet(false, Sun, "look_far", "FAR level (as beyond 140 m), sun from the north-west", FarCells, FarPooled);
			PaintSheet(true, SideSun, "look_side", "near level, sun from the south-east on the sides seen", SideCells, SidePooled);
			// The scorches from above (the review of round 2: her patch's ash, burnt grass and fuel within 5 of one another,
			// one drop shadow): the look sheets' camera, low from the south-east, hides her fuel's stain behind her when it
			// lies on her far side, so all twenty a week on from straight above (north up), as the strategy camera mostly
			// sees them, in the sheet's sun, each with her hull over her patch; her patch's zones measured here.
			Tally Above;
			{
				const int AbW = 300, AbH = 240, Cols = 5;
				const size_t Many = std::min<size_t>(20, Aged[0].Hulks().size());
				const int Rows = static_cast<int>((Many + Cols - 1) / Cols);
				Image Sheet(AbW * Cols, Head + AbH * std::max(1, Rows));
				Sheet.Fill(0, 0, Sheet.W, Sheet.H, 34, 34, 36);
				std::string Labels = "8 8 a week on: all twenty's scorched patches from straight above (north up), the sun from the north-west\n";
				for (size_t K = 0; K < Many; ++K)
				{
					const LandHulk::Hulk& H = Aged[0].Hulks()[K];
					const V3 Anchor(H.X, H.Y, H.Z);
					const int X0 = static_cast<int>(K % Cols) * AbW, Y0 = Head + static_cast<int>(K / Cols) * AbH;
					const View V = View::Looking(V3(H.X, H.Y, H.Z), 0.0, 1.5707, 16.0, X0, Y0, X0 + AbW, Y0 + AbH);
					Draw(Sheet, V, GroundAround(G, H.X, H.Y, 10.0, 0.4, Anchor), Anchor, Sun);
					LandField::Mesh M;
					LandHulk::Dressing Nd;
					Nd.Months = Ages[0];
					Aged[0].Patch(K, M, &Nd);
					Draw(Sheet, V, M, Anchor, Sun, &Nd.Coats, 32);
					Aged[0].Dress(K, true, M, &Nd);
					Draw(Sheet, V, M, Anchor, Sun, &Nd.Coats, 1);
					for (int E = 0; E < AbW; ++E) Sheet.Put(X0 + E, Y0, 34, 34, 36);
					for (int E = 0; E < AbH; ++E) Sheet.Put(X0, Y0 + E, 34, 34, 36);
					for (int J = Y0; J < Y0 + AbH; ++J)
						for (int I = X0; I < X0 + AbW; ++I)
						{
							const size_t Px = static_cast<size_t>(J) * static_cast<size_t>(Sheet.W) + static_cast<size_t>(I);
							const int Tag = Sheet.Tag[Px];
							if (Tag < 32 || Tag >= 32 + LandHulk::Scorches) continue;
							const uint8_t* C = &Sheet.Rgb[Px * 3];
							++Above.PatchN;
							Above.PatchDark += std::max({ C[0], C[1], C[2] }) <= 60 ? 1 : 0;
							Above.Patch[Tag - 32].insert(Above.Patch[Tag - 32].end(), { C[0], C[1], C[2] });
						}
					Labels += std::to_string(X0 + 6) + " " + std::to_string(Y0 + AbH - 20) + " " + H.Nation + " " + H.Model + "\n";
				}
				Sheet.Save(OutDir + "/scorch_" + Name + ".ppm");
				std::ofstream(OutDir + "/scorch_" + Name + ".txt") << Labels;
			}
			// The measure, as a table (and a .csv beside each picture).
			std::ofstream Csv(OutDir + "/look_" + Name + ".csv");
			Csv << "recording,sheet,wreck,age,what,class,share,share_all,r,g,b\n";
			const auto Row = [&](const Tally& T, const std::string& Sheet, const std::string& Who, int I, bool bPrint) -> std::string
			{
				std::string Line, Hues2, Sides;
				char Buf[160];
				for (int C = 0; C < LandHulk::Coats; ++C)
				{
					const size_t N = T.Lit[C].size() / 3;
					const double Of = T.LitN > 0 ? 100.0 * static_cast<double>(N) / static_cast<double>(T.LitN) : 0.0;
					const double OfAll = T.AllN > 0 ? 100.0 * static_cast<double>(T.All[C]) / static_cast<double>(T.AllN) : 0.0;
					const int R = Median(T.Lit[C], 0), Gm = Median(T.Lit[C], 1), B = Median(T.Lit[C], 2);
					Csv << Name << "," << Sheet << "," << Who << "," << AgeWords[I] << ",wreck lit," << LandHulk::CoatName(static_cast<LandHulk::Coat>(C)) << "," << Of << "," << OfAll << "," << R
						<< "," << Gm << "," << B << "\n";
					if (N > 0)
					{
						std::snprintf(Buf, sizeof(Buf), "  %s %.0f%% (%d,%d,%d)", LandHulk::CoatName(static_cast<LandHulk::Coat>(C)), Of, R, Gm, B);
						Line += Buf;
					}
					const size_t Ns = T.Side[C].size() / 3;
					const double OfSide = T.SideN > 0 ? 100.0 * static_cast<double>(Ns) / static_cast<double>(T.SideN) : 0.0;
					Csv << Name << "," << Sheet << "," << Who << "," << AgeWords[I] << ",sides lit," << LandHulk::CoatName(static_cast<LandHulk::Coat>(C)) << "," << OfSide << ",,"
						<< Median(T.Side[C], 0) << "," << Median(T.Side[C], 1) << "," << Median(T.Side[C], 2) << "\n";
					if (Ns > 0 && OfSide >= 0.5)
					{
						std::snprintf(Buf, sizeof(Buf), "  %s %.0f%% (%d,%d,%d)", LandHulk::CoatName(static_cast<LandHulk::Coat>(C)), OfSide, Median(T.Side[C], 0), Median(T.Side[C], 1),
							Median(T.Side[C], 2));
						Sides += Buf;
					}
				}
				for (int Hc = 0; Hc < Hues; ++Hc)
				{
					const size_t N = T.ByHue[Hc].size() / 3;
					const double Of = T.LitN > 0 ? 100.0 * static_cast<double>(N) / static_cast<double>(T.LitN) : 0.0;
					const int R = Median(T.ByHue[Hc], 0), Gm = Median(T.ByHue[Hc], 1), B = Median(T.ByHue[Hc], 2);
					Csv << Name << "," << Sheet << "," << Who << "," << AgeWords[I] << ",wreck lit by colour," << HueName(Hc) << "," << Of << ",," << R << "," << Gm << "," << B << "\n";
					if (N == 0 || Of < 0.5) continue;
					std::snprintf(Buf, sizeof(Buf), "  %s %.0f%% (%d,%d,%d)", HueName(Hc), Of, R, Gm, B);
					Hues2 += Buf;
				}
				// (her lit faces' edges: the share of side-by-side pairs stepping over EdgeHard in luma, and the mean step)
				Csv << Name << "," << Sheet << "," << Who << "," << AgeWords[I] << ",lit edges,hard (luma step over " << EdgeHard << ")," << T.HardShare() << "," << T.EdgeN << ",,,\n";
				Csv << Name << "," << Sheet << "," << Who << "," << AgeWords[I] << ",lit edges,mean luma step," << T.MeanStep() << "," << T.EdgeN << ",,,\n";
				std::string Ground;
				for (int C = 1; C < LandHulk::Scorches; ++C)
				{
					const size_t N = T.Patch[C].size() / 3;
					const double Of = T.PatchN > 0 ? 100.0 * static_cast<double>(N) / static_cast<double>(T.PatchN) : 0.0;
					const int R = Median(T.Patch[C], 0), Gm = Median(T.Patch[C], 1), B = Median(T.Patch[C], 2);
					Csv << Name << "," << Sheet << "," << Who << "," << AgeWords[I] << ",patch," << LandHulk::ScorchName(static_cast<LandHulk::Scorch>(C)) << "," << Of << ",," << R << "," << Gm
						<< "," << B << "\n";
					if (N == 0 || Of < 0.5) continue;
					std::snprintf(Buf, sizeof(Buf), "  %s %.0f%% (%d,%d,%d)", LandHulk::ScorchName(static_cast<LandHulk::Scorch>(C)), Of, R, Gm, B);
					Ground += Buf;
				}
				if (T.PatchN > 0)
				{
					std::snprintf(Buf, sizeof(Buf), "  [%.0f%% at or below 60]", 100.0 * static_cast<double>(T.PatchDark) / static_cast<double>(T.PatchN));
					Ground += Buf;
				}
				if (bPrint)
				{
					std::printf("     %-18s lit %5zu px:%s\n", AgeWords[I], T.LitN, Line.c_str());
					std::printf("     %-18s  by colour:%s\n", "", Hues2.c_str());
					std::printf("     %-18s  hard edges: %.1f%% of %zu side-by-side pairs of her lit pixels step over %.0f in luma; the mean step %.2f\n", "", T.HardShare(), T.EdgeN,
						EdgeHard, T.MeanStep());
					if (Sheet == "side") std::printf("     %-18s  her sides %5zu px:%s\n", "", T.SideN, Sides.c_str());
					if (Sheet == "near") std::printf("     %-18s patch %5zu px:%s\n", "", T.PatchN, Ground.empty() ? "  the field's own" : Ground.c_str());
				}
				return Line;
			};
			const auto LitShare = [](const Tally& T, LandHulk::Coat C) { return T.LitN > 0 ? static_cast<double>(T.Lit[static_cast<int>(C)].size() / 3) / static_cast<double>(T.LitN) : 0.0; };
			const auto SideShare = [](const Tally& T, LandHulk::Coat C) { return T.SideN > 0 ? static_cast<double>(T.Side[static_cast<int>(C)].size() / 3) / static_cast<double>(T.SideN) : 0.0; };
			const auto HueShare = [](const Tally& T, Hue H) { return T.LitN > 0 ? static_cast<double>(T.ByHue[static_cast<int>(H)].size() / 3) / static_cast<double>(T.LitN) : 0.0; };
			const auto Orange = [&](const Tally& T) { return LitShare(T, LandHulk::Coat::Flash) + LitShare(T, LandHulk::Coat::Run); };
			const auto Rusted = [&](const Tally& T) { return Orange(T) + LitShare(T, LandHulk::Coat::OldRust); };
			std::printf("the look, measured on its sheet (lit faces: the sun's light on them %.2f and over; each coat's share of her lit pixels and its median sRGB in the picture, and by colour alone; her patch's, of its pixels shown):\n", LitFrom);
			for (size_t R = 0; R < Pick.size(); ++R)
			{
				std::printf("  %zu %s\n", R + 1, Describe(Aged[0].Hulks()[Pick[R]]).c_str());
				for (int I = 0; I < 5; ++I) Row(Cells[R][static_cast<size_t>(I)], "near", std::to_string(R + 1) + " " + Aged[0].Hulks()[Pick[R]].Model, I, true);
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
				Row(Pooled[static_cast<size_t>(I)], "near", "all four", I, true);
			}
			// (her lit faces' hard edges, each of the four and all four at each age, near level: camouflage's hard lines, read
			// against another build's)
			{
				const auto EdgeLine = [&](const std::vector<Tally>& Ts)
				{
					std::string Edges;
					for (int I = 0; I < 5; ++I)
					{
						char Buf[120];
						std::snprintf(Buf, sizeof(Buf), "%s%s %.1f%% (mean step %.2f)", I > 0 ? "; " : "", AgeWords[I], Ts[static_cast<size_t>(I)].HardShare(), Ts[static_cast<size_t>(I)].MeanStep());
						Edges += Buf;
					}
					return Edges;
				};
				std::printf("  hard edges of her lit faces (side-by-side pixels stepping over %.0f in luma), near level:\n", EdgeHard);
				for (size_t R = 0; R < Pick.size(); ++R) std::printf("    %s: %s\n", Aged[0].Hulks()[Pick[R]].Model.c_str(), EdgeLine(Cells[R]).c_str());
				std::printf("    all four: %s\n", EdgeLine(Pooled).c_str());
			}
			// (each wreck's flash rust a month on, by tag and by colour: the art director's 15 to 25 in 100 of her)
			std::string EachMonth;
			for (size_t R = 0; R < Pick.size(); ++R)
			{
				char Buf[120];
				std::snprintf(Buf, sizeof(Buf), "%s%s %.0f%% (by colour %.0f%% orange)", EachMonth.empty() ? "" : "; ", Aged[0].Hulks()[Pick[R]].Model.c_str(), 100.0 * Orange(Cells[R][1]),
					100.0 * HueShare(Cells[R][1], Hue::Orange));
				EachMonth += Buf;
			}
			std::printf("  each a month on, flash rust of her lit faces: %s\n", EachMonth.c_str());
			// (loose guards, so a later change that loses her patchwork is caught: a week on soot over a third to two thirds
			// and next to no orange, by tag or by colour; a month on some rust; the old rust's red no more than 2.2 times its
			// blue)
			const double WeekSoot = LitShare(Pooled[0], LandHulk::Coat::Soot), WeekFlash = Orange(Pooled[0]), WeekOrange = HueShare(Pooled[0], Hue::Orange);
			const double MonthRust = Rusted(Pooled[1]);
			bool bOldDull = true;
			for (int I = 2; I < 5; ++I)
			{
				const Tally& T = Pooled[static_cast<size_t>(I)];
				const int R = Median(T.Lit[static_cast<int>(LandHulk::Coat::OldRust)], 0), B = Median(T.Lit[static_cast<int>(LandHulk::Coat::OldRust)], 2);
				if (R >= 0 && B > 0 && R > 2.25 * B) bOldDull = false;
			}
			if (WeekSoot < 0.33 || WeekSoot > 0.67 || WeekFlash > 0.1 || WeekOrange > 0.08 || MonthRust < 0.1 || !bOldDull)
				Fault(Name + ": the look's measure off its marks (a week on soot " + std::to_string(WeekSoot) + ", orange " + std::to_string(WeekFlash) + ", by colour " +
					std::to_string(WeekOrange) + "; a month on rust " + std::to_string(MonthRust) + (bOldDull ? "" : "; the old rust too red") + ")");
			// Her patch a week on: its three zones told apart (the review of round 2: ash, burnt grass and fuel all within 5
			// of one another and of round 1's drop shadow, 38,35,26): their median colours 15 and more apart, measured on
			// all twenty from above (the scorch sheet).
			{
				const Tally& T = Above;
				const LandHulk::Scorch Zones[3] = { LandHulk::Scorch::Ash, LandHulk::Scorch::Burnt, LandHulk::Scorch::Fuel };
				int Mid[3][3];
				for (int Z = 0; Z < 3; ++Z)
					for (int C = 0; C < 3; ++C) Mid[Z][C] = Median(T.Patch[static_cast<int>(Zones[Z])], C);
				double Least = 1e9;
				std::string Apart;
				for (int Za = 0; Za < 3; ++Za)
					for (int Zb = Za + 1; Zb < 3; ++Zb)
					{
						const double D = std::sqrt(static_cast<double>((Mid[Za][0] - Mid[Zb][0]) * (Mid[Za][0] - Mid[Zb][0]) + (Mid[Za][1] - Mid[Zb][1]) * (Mid[Za][1] - Mid[Zb][1])
							+ (Mid[Za][2] - Mid[Zb][2]) * (Mid[Za][2] - Mid[Zb][2])));
						if (Mid[Za][0] >= 0 && Mid[Zb][0] >= 0) Least = std::min(Least, D);
						Apart += (Apart.empty() ? "" : ", ") + std::string(LandHulk::ScorchName(Zones[Za])) + " to " + LandHulk::ScorchName(Zones[Zb]) + " " + std::to_string(static_cast<int>(std::lround(D)));
					}
				const auto ZoneOf = [&](LandHulk::Scorch Z) { return T.PatchN > 0 ? 100.0 * static_cast<double>(T.Patch[static_cast<int>(Z)].size() / 3) / static_cast<double>(T.PatchN) : 0.0; };
				std::printf("  her patch a week on, all twenty from above (the scorch sheet), its zones: ash %.0f%% (%d,%d,%d), burnt grass %.0f%% (%d,%d,%d), rim %.0f%% (%d,%d,%d), fuel %.0f%% (%d,%d,%d); apart %s; %.0f%% of the patch at or below 60 (round 2: 24 to 52)\n",
					ZoneOf(LandHulk::Scorch::Ash), Mid[0][0], Mid[0][1], Mid[0][2], ZoneOf(LandHulk::Scorch::Burnt), Mid[1][0], Mid[1][1], Mid[1][2], ZoneOf(LandHulk::Scorch::Rim),
					Median(T.Patch[static_cast<int>(LandHulk::Scorch::Rim)], 0), Median(T.Patch[static_cast<int>(LandHulk::Scorch::Rim)], 1), Median(T.Patch[static_cast<int>(LandHulk::Scorch::Rim)], 2),
					ZoneOf(LandHulk::Scorch::Fuel), Mid[2][0], Mid[2][1], Mid[2][2], Apart.c_str(), T.PatchN > 0 ? 100.0 * static_cast<double>(T.PatchDark) / static_cast<double>(T.PatchN) : 0.0);
				if (!(Least >= 15.0) || Least > 1e8) Fault(Name + ": her patch's ash, burnt grass and fuel not told apart a week on (" + Apart + ")");
			}
			// The far level, beside the near one: the same patchwork's shares a week on (round 2's far level was a third
			// flash orange and next to none scorched, its near one 3 and 22 in 100).
			std::printf("  her far level (as the game shows her beyond 140 m), all four:\n");
			for (int I = 0; I < 5; ++I) Row(FarPooled[static_cast<size_t>(I)], "far", "all four", I, true);
			{
				const double NearFlash = Orange(Pooled[0]), FarFlash = Orange(FarPooled[0]);
				const double NearScorch = LitShare(Pooled[0], LandHulk::Coat::Scorched), FarScorch = LitShare(FarPooled[0], LandHulk::Coat::Scorched);
				const double NearOrange = HueShare(Pooled[0], Hue::Orange), FarOrange = HueShare(FarPooled[0], Hue::Orange);
				const double NearPale = HueShare(Pooled[0], Hue::PaleGrey), FarPale = HueShare(FarPooled[0], Hue::PaleGrey);
				const double NearMonth = Rusted(Pooled[1]), FarMonth = Rusted(FarPooled[1]);
				std::printf("  near against far a week on: orange %.0f%% / %.0f%% (by colour %.0f%% / %.0f%%), scorched %.0f%% / %.0f%% (pale grey-buff by colour %.0f%% / %.0f%%); a month on rust %.0f%% / %.0f%%\n",
					100 * NearFlash, 100 * FarFlash, 100 * NearOrange, 100 * FarOrange, 100 * NearScorch, 100 * FarScorch, 100 * NearPale, 100 * FarPale, 100 * NearMonth, 100 * FarMonth);
				if (std::fabs(FarFlash - NearFlash) > 0.05 || std::fabs(FarScorch - NearScorch) > 0.06 || FarOrange > NearOrange + 0.05 || std::fabs(FarMonth - NearMonth) > 0.08)
					Fault(Name + ": the far level's patchwork off the near one's");
			}
			// The side sheet: her sides in the sun (the first sheet's sun is behind the sides its camera sees): her own paint
			// low on her cooler sides a week on, the rust's runs down them a month on.
			std::printf("  her sides in the sun (the side sheet, near level, the sun from the south-east), all four:\n");
			for (int I = 0; I < 5; ++I) Row(SidePooled[static_cast<size_t>(I)], "side", "all four", I, true);
			{
				const double WeekPaint = SideShare(SidePooled[0], LandHulk::Coat::Paint), MonthRuns = SideShare(SidePooled[1], LandHulk::Coat::Run);
				const double MonthSideRust = SideShare(SidePooled[1], LandHulk::Coat::Flash) + MonthRuns;
				std::printf("  her sides: her own paint a week on %.0f%%; a month on flash rust %.0f%% of them, %.0f%% in runs\n", 100 * WeekPaint, 100 * MonthSideRust, 100 * MonthRuns);
				if (WeekPaint < 0.1 || MonthRuns < 0.02) Fault(Name + ": her sides show no paint kept a week on, or no runs a month on");
			}
			// The art director's carries (24 November 21:01Z, the look passed), measured.
			// (1) A month on, the flash rust on a sunlit turret top read hot, almost glowing: its brightest held at about 150, 85,
			// 45 in the picture, at most 5 over it in each channel, measured three ways: the 2 in 100 of its pixels brightest by
			// value, their median colour; each channel's own 98th in 100 of its pixels drawn of the flash rust alone (Pure:
			// its plates' edges, rusting points among the scorched paint, are drawn mixed with that paint, their blue and green
			// paler, not hotter); and the red's and the green's own 98th in 100 of all its pixels, its blends too (their blue
			// left out: a blend's blue is the pale paint's beside it, not a hotter rust's). On the first sheet near and far, on
			// her sunlit tops (facing up); on the side sheet, on all her lit faces (its sun on her sides, squarely on some). For each of the four with 30 such pixels and more, and for
			// all four. And with it held, the flash rust still about 130, 70, 38 (the first sheet, all four, its median: red 118
			// to 142, green 62 to 78, blue 30 to 46) and still mottled (the flash rust alone as painted, before the light: its
			// red's 90th in 100 less its median, MottleFrom and more, its brighter half not one even coat. Round 3's spread 35
			// to 40 with its brightest at 190; the fix round's first try held its brightest by making it one even coat, 7 to 9;
			// the lower half is mostly its rusting points mixed with the grey steel, read neither way).
			// (2) From half a year her old rust read as one even brown: a little of her paint wanted low and on her sheltered
			// faces. On the side sheet, every pixel of hers lit or in shade, her paint left (her own or scorched) on her open
			// sides (her upper half, facing z under 0.5), low on her (her lower half) and on her sky faces (facing z 0.5 and
			// over), all four: low on her kept (from half a year four fifths at least of a month on's), more low on her than
			// on her open sides (15 in 100 and more), a little only on her open sides (no more than 8 in 100 over a month on's)
			// and none new on her sky faces (her own paint there no more than 2 in 100 over a month on's: the soot washing off
			// them shows the scorched paint under it); lit low on her, her paint left paler than her old rust there (12
			// and more in luma: paint, not grime; the fix round's first try's olive patches high on her open plates read as
			// grime, and its open sides held her own paint on a fifth of them). And a few runs down her sides darker than
			// her old rust (her lit sides: those whose luma lies 10 and more under her old rust's median there, 1.5 in 100 of
			// them and more, all four).
			{
				const auto Pct = [](const std::vector<uint8_t>& V, int Channel, double Q)
				{
					std::vector<uint8_t> One;
					for (size_t K = static_cast<size_t>(Channel); K < V.size(); K += 3) One.push_back(V[K]);
					if (One.empty()) return -1;
					const size_t At = std::min(One.size() - 1, static_cast<size_t>(Q * static_cast<double>(One.size())));
					std::nth_element(One.begin(), One.begin() + static_cast<std::ptrdiff_t>(At), One.end());
					return static_cast<int>(One[At]);
				};
				const int Hot[3] = { 150, 85, 45 };
				constexpr int HotBy = 5;
				constexpr size_t HotFew = 30;
				std::string Hottest;
				bool bHot = false;
				// (the brightest: the 2 in 100 of them brightest by their value, their highest channel, as the eye finds the hot
				// ones; their median colour)
				const auto Top2 = [&Median](const std::vector<uint8_t>& V, int Out[3])
				{
					std::vector<std::pair<int, size_t>> ByValue;
					for (size_t K = 0; K + 2 < V.size(); K += 3) ByValue.emplace_back(std::max({ V[K], V[K + 1], V[K + 2] }), K);
					Out[0] = Out[1] = Out[2] = -1;
					if (ByValue.empty()) return;
					std::sort(ByValue.begin(), ByValue.end(), [](const std::pair<int, size_t>& P, const std::pair<int, size_t>& Q) { return P.first > Q.first || (P.first == Q.first && P.second < Q.second); });
					const size_t Many = std::max<size_t>(1, ByValue.size() / 50);
					std::vector<uint8_t> Hottest2;
					for (size_t K = 0; K < Many; ++K) Hottest2.insert(Hottest2.end(), { V[ByValue[K].second], V[ByValue[K].second + 1], V[ByValue[K].second + 2] });
					for (int C = 0; C < 3; ++C) Out[C] = Median(Hottest2, C);
				};
				const auto Brightest = [&](const std::vector<uint8_t>& All, const std::vector<uint8_t>& Alone, const std::string& Sheet, const std::string& Faces, const std::string& Who)
				{
					const size_t N = All.size() / 3, Na = Alone.size() / 3;
					int Top[3], Own[3], Mixed98[3];
					Top2(All, Top);
					for (int C = 0; C < 3; ++C)
					{
						Own[C] = Pct(Alone, C, 0.98);
						Mixed98[C] = Pct(All, C, 0.98);
					}
					char Buf[300];
					std::snprintf(Buf, sizeof(Buf), "%s%s %s (%d,%d,%d) of %zu px, its own 98th (%d,%d,%d) of %zu px [all its pixels' (%d,%d,%d)], median (%d,%d,%d)", Hottest.empty() ? "" : "\n      ",
						Who.c_str(), Sheet.c_str(), Top[0], Top[1], Top[2], N, Own[0], Own[1], Own[2], Na, Mixed98[0], Mixed98[1], Mixed98[2], Median(All, 0), Median(All, 1), Median(All, 2));
					Hottest += Buf;
					Csv << Name << "," << Sheet << "," << Who << "," << AgeWords[1] << ",flash on " << Faces << ",brightest (2 in 100 by value)," << N << ",," << Top[0] << "," << Top[1] << "," << Top[2] << "\n";
					Csv << Name << "," << Sheet << "," << Who << "," << AgeWords[1] << ",flash on " << Faces << ",each channel's 98th in 100 (the flash rust alone)," << Na << ",," << Own[0] << "," << Own[1] << ","
						<< Own[2] << "\n";
					Csv << Name << "," << Sheet << "," << Who << "," << AgeWords[1] << ",flash on " << Faces << ",each channel's 98th in 100 (all its pixels)," << N << ",," << Mixed98[0] << "," << Mixed98[1] << ","
						<< Mixed98[2] << "\n";
					for (int C = 0; C < 3; ++C)
					{
						if (N >= HotFew && Top[C] > Hot[C] + HotBy) bHot = true;
						if (Na >= HotFew && Own[C] > Hot[C] + HotBy) bHot = true;
						if (C < 2 && N >= HotFew && Mixed98[C] > Hot[C] + HotBy) bHot = true;
					}
				};
				for (size_t R = 0; R <= Pick.size(); ++R)
				{
					const bool bAll = R == Pick.size();
					const std::string Who = bAll ? std::string("all four") : std::to_string(R + 1) + " " + Aged[0].Hulks()[Pick[R]].Model;
					const Tally& Near = bAll ? Pooled[1] : Cells[R][1];
					const Tally& Far = bAll ? FarPooled[1] : FarCells[R][1];
					const Tally& Side = bAll ? SidePooled[1] : SideCells[R][1];
					Brightest(Near.FlashTop, Near.FlashTopPure, "near", "sunlit tops", Who);
					Brightest(Far.FlashTop, Far.FlashTopPure, "far", "sunlit tops", Who);
					Brightest(Side.FlashLit, Side.FlashLitPure, "side", "lit faces", Who);
				}
				std::printf("  a month on, her flash rust's brightest lit (the 2 in 100 brightest by value, their median; each channel's 98th of the flash rust alone; the red's and green's 98th of all its pixels; wanted about 150,85,45 at most): near and far on her sunlit tops, side on her lit faces:\n      %s\n", Hottest.c_str());
				if (bHot) Fault(Name + ": a month on, her flash rust's brightest lit over about 150,85,45");
				// (its median and its mottle, a month on, the first sheet)
				constexpr int MottleFrom = 11;
				std::string Mottled;
				for (size_t R = 0; R <= Pick.size(); ++R)
				{
					const bool bAll = R == Pick.size();
					const Tally& T = bAll ? Pooled[1] : Cells[R][1];
					const std::vector<uint8_t>& Fl = T.Lit[static_cast<int>(LandHulk::Coat::Flash)];
					char Buf[200];
					std::snprintf(Buf, sizeof(Buf), "%s%s (%d,%d,%d), as painted its red %d / %d / %d", Mottled.empty() ? "" : "; ", bAll ? "all four" : Aged[0].Hulks()[Pick[R]].Model.c_str(), Median(Fl, 0), Median(Fl, 1),
						Median(Fl, 2), Pct(T.FlashCoat, 0, 0.1), Pct(T.FlashCoat, 0, 0.5), Pct(T.FlashCoat, 0, 0.9));
					Mottled += Buf;
				}
				const std::vector<uint8_t>& Flash = Pooled[1].Lit[static_cast<int>(LandHulk::Coat::Flash)];
				const int Fr = Median(Flash, 0), Fg = Median(Flash, 1), Fb = Median(Flash, 2), Spread = Pct(Pooled[1].FlashCoat, 0, 0.9) - Pct(Pooled[1].FlashCoat, 0, 0.5);
				std::printf("  a month on, her flash rust lit, its median, and as painted (before the light, the flash rust alone) its red's 10th / 50th / 90th in 100 (wanted about 130,70,38, mottled): %s\n", Mottled.c_str());
				Csv << Name << ",near,all four," << AgeWords[1] << ",flash lit,as painted: red's 90th in 100 less its median," << Spread << ",,,,\n";
				if (Fr < 118 || Fr > 142 || Fg < 62 || Fg > 78 || Fb < 30 || Fb > 46 || Spread < MottleFrom)
					Fault(Name + ": a month on, her flash rust off about 130,70,38 or not mottled (" + std::to_string(Fr) + "," + std::to_string(Fg) + "," + std::to_string(Fb) + ", its red's spread " + std::to_string(Spread) + ")");
				// (her paint left by where the weather reaches her; her runs on her lit sides, and the darker ones)
				const auto Luma3 = [&Luma](int R, int Gm, int B)
				{
					const uint8_t C[3] = { static_cast<uint8_t>(std::clamp(R, 0, 255)), static_cast<uint8_t>(std::clamp(Gm, 0, 255)), static_cast<uint8_t>(std::clamp(B, 0, 255)) };
					return Luma(C);
				};
				struct Old
				{
					double Share[3] = {}, Own[3] = {}, PaintLuma = -1.0, RustLuma = -1.0, Runs = 0.0, Darker = 0.0;
				};
				const char* ZoneWords[3] = { "her open sides", "low on her", "her sky faces" };
				const auto OldOf = [&](const Tally& T, const std::string& Who, int I, std::string& Said)
				{
					Old O;
					std::string Zones;
					for (int Z = 0; Z < 3; ++Z)
					{
						O.Share[Z] = T.ZoneN[Z] > 0 ? static_cast<double>(T.ZonePaint[Z].size() / 3) / static_cast<double>(T.ZoneN[Z]) : 0.0;
						O.Own[Z] = T.ZoneN[Z] > 0 ? static_cast<double>(T.ZoneOwn[Z]) / static_cast<double>(T.ZoneN[Z]) : 0.0;
						char Buf[140];
						std::snprintf(Buf, sizeof(Buf), "%s%s %.0f%% (%d,%d,%d; her own %.0f%%) of %zu px", Z > 0 ? ", " : "", ZoneWords[Z], 100.0 * O.Share[Z], Median(T.ZonePaint[Z], 0), Median(T.ZonePaint[Z], 1),
							Median(T.ZonePaint[Z], 2), 100.0 * O.Own[Z], T.ZoneN[Z]);
						Zones += Buf;
						Csv << Name << ",side," << Who << "," << AgeWords[I] << ",paint left (her own or scorched)," << ZoneWords[Z] << "," << 100.0 * O.Share[Z] << "," << T.ZoneN[Z] << "," << Median(T.ZonePaint[Z], 0)
							<< "," << Median(T.ZonePaint[Z], 1) << "," << Median(T.ZonePaint[Z], 2) << "\n";
					}
					const std::vector<uint8_t>& LowPaint = T.ZoneLitPaint[Tally::Low];
					const std::vector<uint8_t>& LowRust = T.ZoneLitRust[Tally::Low];
					if (!LowPaint.empty()) O.PaintLuma = Luma3(Median(LowPaint, 0), Median(LowPaint, 1), Median(LowPaint, 2));
					if (!LowRust.empty()) O.RustLuma = Luma3(Median(LowRust, 0), Median(LowRust, 1), Median(LowRust, 2));
					Csv << Name << ",side," << Who << "," << AgeWords[I] << ",lit low on her,paint left (her own or scorched)," << O.PaintLuma << "," << LowPaint.size() / 3 << "," << Median(LowPaint, 0) << ","
						<< Median(LowPaint, 1) << "," << Median(LowPaint, 2) << "\n";
					Csv << Name << ",side," << Who << "," << AgeWords[I] << ",lit low on her,old rust," << O.RustLuma << "," << LowRust.size() / 3 << "," << Median(LowRust, 0) << "," << Median(LowRust, 1) << ","
						<< Median(LowRust, 2) << "\n";
					const std::vector<uint8_t>& Runs = T.Side[static_cast<int>(LandHulk::Coat::Run)];
					const std::vector<uint8_t>& Rust = T.Side[static_cast<int>(LandHulk::Coat::OldRust)];
					O.Runs = T.SideN > 0 ? static_cast<double>(Runs.size() / 3) / static_cast<double>(T.SideN) : 0.0;
					const double RustLuma = Rust.empty() ? -1.0 : Luma3(Median(Rust, 0), Median(Rust, 1), Median(Rust, 2));
					size_t NDark = 0;
					for (size_t K = 0; K + 2 < Runs.size(); K += 3) NDark += RustLuma >= 0.0 && Luma(&Runs[K]) <= RustLuma - 10.0 ? 1 : 0;
					O.Darker = T.SideN > 0 ? static_cast<double>(NDark) / static_cast<double>(T.SideN) : 0.0;
					char Buf[600];
					std::snprintf(Buf, sizeof(Buf), "%s%s: her paint left (her own or scorched) on %s; lit low on her, her paint left (%d,%d,%d) luma %.0f, her old rust (%d,%d,%d) luma %.0f; runs %.1f%% of her lit sides (%d,%d,%d), darker %.1f%% (her old rust there (%d,%d,%d))",
						Said.empty() ? "" : "\n      ", AgeWords[I], Zones.c_str(), Median(LowPaint, 0), Median(LowPaint, 1), Median(LowPaint, 2), O.PaintLuma, Median(LowRust, 0), Median(LowRust, 1), Median(LowRust, 2),
						O.RustLuma, 100.0 * O.Runs, Median(Runs, 0), Median(Runs, 1), Median(Runs, 2), 100.0 * O.Darker, Median(Rust, 0), Median(Rust, 1), Median(Rust, 2));
					Said += Buf;
					Csv << Name << ",side," << Who << "," << AgeWords[I] << ",runs on lit sides,darker than her old rust (luma 10 under)," << 100.0 * O.Darker << "," << 100.0 * O.Runs << "," << Median(Runs, 0) << ","
						<< Median(Runs, 1) << "," << Median(Runs, 2) << "\n";
					return O;
				};
				std::printf("  from half a year, her paint left and her darker runs (the side sheet):\n");
				for (size_t R = 0; R < Pick.size(); ++R)
				{
					std::string Said;
					for (int I = 1; I < 5; ++I) OldOf(SideCells[R][static_cast<size_t>(I)], std::to_string(R + 1) + " " + Aged[0].Hulks()[Pick[R]].Model, I, Said);
					std::printf("    %s:\n      %s\n", Aged[0].Hulks()[Pick[R]].Model.c_str(), Said.c_str());
				}
				std::string Said;
				Old Was[5];
				for (int I = 0; I < 5; ++I) Was[I] = OldOf(SidePooled[static_cast<size_t>(I)], "all four", I, Said);
				std::printf("    all four:\n      %s\n", Said.c_str());
				std::string Even;
				for (int I = 2; I < 5; ++I)
				{
					const Old& O = Was[I];
					const Old& Month = Was[1];
					std::string Off;
					if (O.Share[Tally::Low] < 0.8 * Month.Share[Tally::Low]) Off += " her paint low on her not kept;";
					if (!(O.Share[Tally::Low] >= O.Share[Tally::Open] + 0.15)) Off += " her paint left not low on her;";
					if (O.Share[Tally::Open] > Month.Share[Tally::Open] + 0.08) Off += " more than a little on her open sides;";
					if (O.Own[Tally::Sky] > Month.Own[Tally::Sky] + 0.02) Off += " her own paint new on her sky faces;";
					if (!(O.PaintLuma >= 0.0 && O.RustLuma >= 0.0 && O.PaintLuma >= O.RustLuma + 12.0)) Off += " her paint left low on her not paler than her old rust there;";
					if (O.Darker < 0.015) Off += " too few darker runs;";
					if (!Off.empty()) Even += std::string(" ") + AgeWords[I] + ":" + Off;
				}
				if (!Even.empty()) Fault(Name + ": from half a year, her old rust one even brown (" + Even + " )");
			}
			// And every one of the twenty (not only the sheet's four), by area: what faces up on her near and far levels a
			// week on, each corner's coat a third of its triangle's area. (The near level's orange is its plates' edges, a
			// point on each, which this measure weighs by the triangles round it, three times what the picture shows; the far
			// level has none, its edges under a pixel at 140 m: so its scorched paint is held to the near level's scorched
			// and edges together, and its orange to no more than the near level's.)
			{
				double Area[2][LandHulk::Coats] = {}, Total[2] = { 0.0, 0.0 };
				for (size_t K = 0; K < Aged[0].Hulks().size(); ++K)
				{
					for (int Lv = 0; Lv < 2; ++Lv)
					{
						LandField::Mesh Dm;
						LandHulk::Dressing Nd;
						Nd.Months = 0.25;
						Aged[0].Dress(K, Lv == 0, Dm, &Nd);
						for (size_t T = 0; T + 2 < Dm.Triangles.size(); T += 3)
						{
							const uint32_t Ia = Dm.Triangles[T], Ib = Dm.Triangles[T + 1], Ic = Dm.Triangles[T + 2];
							const V3 Pa(Dm.Positions[Ia * 3], Dm.Positions[Ia * 3 + 1], Dm.Positions[Ia * 3 + 2]), Pb(Dm.Positions[Ib * 3], Dm.Positions[Ib * 3 + 1], Dm.Positions[Ib * 3 + 2]);
							const V3 Pc(Dm.Positions[Ic * 3], Dm.Positions[Ic * 3 + 1], Dm.Positions[Ic * 3 + 2]);
							const V3 Nm = (Pb - Pa).Cross(Pc - Pa);
							const double Ar = 0.5 * Nm.Length();
							if (!(Ar > 0.0) || std::fabs(Nm.Z) < 0.7 * 2.0 * Ar) continue;
							for (const uint32_t Vx : { Ia, Ib, Ic })
								if (Vx < Nd.Coats.size()) Area[Lv][Nd.Coats[Vx]] += Ar / 3.0;
							Total[Lv] += Ar;
						}
					}
				}
				const auto Of = [&](int Lv, LandHulk::Coat C) { return Total[Lv] > 0.0 ? 100.0 * Area[Lv][static_cast<int>(C)] / Total[Lv] : 0.0; };
				const double NearF = Of(0, LandHulk::Coat::Flash) + Of(0, LandHulk::Coat::Run), FarF = Of(1, LandHulk::Coat::Flash) + Of(1, LandHulk::Coat::Run);
				std::printf("  all twenty a week on, what faces up by area, near / far: soot %.0f%% / %.0f%%, scorched %.0f%% / %.0f%%, bare %.0f%% / %.0f%%, orange %.0f%% / %.0f%%, paint %.0f%% / %.0f%%\n",
					Of(0, LandHulk::Coat::Soot), Of(1, LandHulk::Coat::Soot), Of(0, LandHulk::Coat::Scorched), Of(1, LandHulk::Coat::Scorched), Of(0, LandHulk::Coat::Bare), Of(1, LandHulk::Coat::Bare),
					NearF, FarF, Of(0, LandHulk::Coat::Paint), Of(1, LandHulk::Coat::Paint));
				if (FarF > NearF + 3.0 || std::fabs(Of(0, LandHulk::Coat::Scorched) + NearF - Of(1, LandHulk::Coat::Scorched) - FarF) > 6.0
					|| std::fabs(Of(0, LandHulk::Coat::Soot) - Of(1, LandHulk::Coat::Soot)) > 6.0)
					Fault(Name + ": the twenty's far levels' orange, scorched paint or soot off their near levels'");
			}
		}

		// 2b. The lorries and half-tracks burnt out (the art director's note, 24 November): a lorry's canvas tilt burnt off
		// its hoops (every nation's: round 1 drew the tilt as rusting steel), her bed's floor laid again as planks, charred,
		// a quarter to two fifths gone, her glass gone, her tyres burnt to the rims; a half-track's canvas, leather, seats and glass
		// likewise. Every nation's wrecks, twenty of each laid (the stand-in), each dressed and counted against her own model,
		// by this test's own list of what burns and what is wood, read by each paint's stem before any dot (round 2 matched
		// whole names here as in the dressing, and missed the M3's 640 points of "canvas.seat" and the T-34's logs, "wood.end"
		// and "wood.split"); a thrown turret's open ring the size of her turret's own base.
		{
			const auto Stem = [](const std::string& P) { return P.substr(0, P.find('.')); };
			const auto BurnsAway = [&Stem](const std::string& P)
			{
				static const char* Cloth[] = { "canvas", "leather", "rope", "cloth", "torn", "skin" };
				const std::string S = Stem(P);
				return std::any_of(std::begin(Cloth), std::end(Cloth), [&S](const char* C) { return S == C; });
			};
			std::string Seen;
			int Soft = 0, Wrong = 0, Seats = 0, SeatsChecked = 0, Logs = 0, LogsWood = 0, Rings = 0, RingsWrong = 0;
			double RingLeast = 1e9, RingMost = 0.0;
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
					if (!Mo) continue;
					// (her own model's canvas, leather, rope and cloth, glass, wood, tilt and pneumatic tyres, on the bodies
					// drawn; and her bed's floor: the flat of wood facing up holding the most of it)
					int Gone = 0, Glass = 0, Tyres = 0, Wood = 0, Seat = 0, Log = 0;
					std::map<int64_t, double> UpWood;
					double UpWoodAll = 0.0;
					const int OffB = H.TrackOff != 0 ? Mo->TrackBody[H.TrackOff < 0 ? 0 : 1] : -1;
					for (const LandKit::Body& Bo : Mo->Bodies)
					{
						if (Bo.Name == "aerial" || Bo.Name.find("roof_mg") != std::string::npos || Bo.Name == "mg_mount") continue;
						if (OffB >= 0 && &Bo == &Mo->Bodies[static_cast<size_t>(OffB)]) continue;
						const std::string& Part = Mo->Parts[static_cast<size_t>(Bo.Part)].Name;
						Tyres += Part.find("wheel_front") != std::string::npos || Part.find("wheel_back") != std::string::npos ? 1 : 0;
						const LandKit::Mesh& Me = Bo.Near;
						const auto PaintOf = [&](size_t I) { return I < Me.Paints.size() && Me.Paints[I] < Mo->PaintNames.size() ? Mo->PaintNames[Me.Paints[I]] : std::string(); };
						for (size_t I = 0; I < Me.Paints.size(); ++I)
						{
							const std::string Paint = PaintOf(I);
							Gone += BurnsAway(Paint) ? 1 : 0;
							Glass += Stem(Paint) == "glass" || Stem(Paint) == "lamp" ? 1 : 0;
							Wood += Stem(Paint) == "wood" ? 1 : 0;
							Seat += Paint.rfind("canvas.", 0) == 0 ? 1 : 0;
							Log += Paint.rfind("wood.", 0) == 0 ? 1 : 0;
						}
						for (size_t T = 0; T + 2 < Me.Triangles.size(); T += 3)
						{
							const uint32_t Ia = Me.Triangles[T], Ib = Me.Triangles[T + 1], Ic = Me.Triangles[T + 2];
							if (Stem(PaintOf(Ia)) != "wood" || Stem(PaintOf(Ib)) != "wood" || Stem(PaintOf(Ic)) != "wood") continue;
							const double Za = Me.Points[Ia * 3 + 2], Zb = Me.Points[Ib * 3 + 2], Zc = Me.Points[Ic * 3 + 2];
							if (std::fabs(Za - Zb) > 0.005 || std::fabs(Za - Zc) > 0.005 || Me.Normals[Ia * 3 + 2] < 0.95 || Me.Normals[Ib * 3 + 2] < 0.95 || Me.Normals[Ic * 3 + 2] < 0.95) continue;
							const V3 Pa(Me.Points[Ia * 3], Me.Points[Ia * 3 + 1], Za), Pb(Me.Points[Ib * 3], Me.Points[Ib * 3 + 1], Zb), Pc(Me.Points[Ic * 3], Me.Points[Ic * 3 + 1], Zc);
							const double Ar = 0.5 * (Pb - Pa).Cross(Pc - Pa).Length();
							UpWood[std::llround((Bo.Pivot.Z + Za) * 100.0)] += Ar;
							UpWoodAll += Ar;
						}
					}
					LandField::Mesh M;
					LandHulk::Dressing Nd;
					Y.Dress(K, true, M, &Nd);
					bool bOk = Nd.Burnt == Gone && Nd.Glass == Glass;
					if (Mo->Name == "M3Halftrack" || H.Model == "M3Halftrack")
					{
						Seats += Seat;
						SeatsChecked += 1;
						bOk = bOk && Seat > 0;
					}
					if (Log > 0)
					{
						Logs += Log;
						LogsWood += Nd.Wood;
					}
					const bool bBed = H.Model == "Truck";
					// (her wood dressed as charred wood: every point of it, but her bed's floor, which her planks replace)
					if (!bBed && Wood > 0) bOk = bOk && Nd.Wood >= Wood * 95 / 100;
					// (a thrown turret's open ring: within her turret's own base, not over the roof round it)
					if (H.bTurretOff && Mo->Turret >= 0)
					{
						double Low = 1e9, Reach = 0.0;
						const LandKit::Part& Tp = Mo->Parts[static_cast<size_t>(Mo->Turret)];
						for (const LandKit::Body& Bo : Mo->Bodies)
						{
							if (Bo.Part != Mo->Turret) continue;
							for (size_t I = 0; I < Bo.Near.Count(); ++I) Low = std::min(Low, Bo.Pivot.Z + Bo.Near.Points[I * 3 + 2]);
							for (size_t I = 0; I < Bo.Near.Count(); ++I)
								if (Bo.Pivot.Z + Bo.Near.Points[I * 3 + 2] <= Low + 0.04)
									Reach = std::max(Reach, std::hypot(Bo.Pivot.X + Bo.Near.Points[I * 3] - Tp.Pivot.X, Bo.Pivot.Y + Bo.Near.Points[I * 3 + 1] - Tp.Pivot.Y));
						}
						++Rings;
						RingLeast = std::min(RingLeast, Nd.RingR);
						RingMost = std::max(RingMost, Nd.RingR);
						if (!(Nd.Hole > 0 && Nd.RingR <= Reach && Nd.RingR >= 0.8 * Reach)) ++RingsWrong;
					}
					if (!H.bSoft)
					{
						Wrong += bOk ? 0 : 1;
						continue;
					}
					++Soft;
					const bool bTilt = Mo->PartIndex("tilt") >= 0;
					bOk = bOk && Nd.Rims == Tyres && (!bTilt || Nd.Hoops > 0);
					std::string BedSaid;
					if (bBed)
					{
						// (her floor, by this test's own read of her model: the level of wood facing up that holds the most)
						int64_t FloorKey = INT64_MIN;
						double FloorArea = 0.0;
						for (const auto& [Key, Ar] : UpWood)
							if (Ar > FloorArea)
							{
								FloorArea = Ar;
								FloorKey = Key;
							}
						const double FloorZ = static_cast<double>(FloorKey) / 100.0;
						const double GoneShare = Nd.Planks > 0 ? static_cast<double>(Nd.PlanksGone) / Nd.Planks : 0.0;
						const bool bFloor = FloorKey != INT64_MIN && std::fabs(Nd.BedTop - FloorZ) <= 0.02 && std::fabs(Nd.PlankTop - FloorZ) <= 0.02 && Nd.BedDropped >= 0.8 * FloorArea
							&& Nd.BedDropped >= 0.8 * Nd.BedArea && Nd.WoodKept >= 0.9 * (UpWoodAll - FloorArea);
						bOk = bOk && bFloor && Nd.Planks > 0 && GoneShare >= 0.2 && GoneShare <= 0.45;
						char Buf[320];
						std::snprintf(Buf, sizeof(Buf), ", her bed's floor (%.2f m up, %.1f square metres, her own model's %.2f m) taken off %.1f square metres and laid again as %d planks, their tops %.2f m up, %d burnt through (%.0f%%); her benches and other wood facing up kept, %.1f of %.1f square metres",
							Nd.BedTop, Nd.BedArea, FloorZ, Nd.BedDropped, Nd.Planks, Nd.PlankTop, Nd.PlanksGone, 100.0 * GoneShare, Nd.WoodKept, UpWoodAll - FloorArea);
						BedSaid = Buf;
					}
					Wrong += bOk ? 0 : 1;
					const std::string Key = H.Nation + " " + H.Model;
					if (Said[Key] && bOk) continue;
					Said[Key] = true;
					char Line[640];
					std::snprintf(Line, sizeof(Line), "%s%s: %d points of canvas, leather, rope and cloth burnt away%s%s, %d of glass gone, %d tyres to the rims%s%s", Seen.empty() ? "" : "; ",
						Key.c_str(), Nd.Burnt, Seat > 0 ? (" (her " + std::to_string(Seat) + " points of canvas seats among them)").c_str() : "",
						bTilt ? (" (the tilt's hoops kept, " + std::to_string(Nd.Hoops) + " points)").c_str() : "", Nd.Glass, Nd.Rims, BedSaid.c_str(), bOk ? "" : " (WRONG)");
					Seen += Line;
				}
			}
			std::printf("lorries and half-tracks burnt out, every nation's (%d soft wrecks): %s\n", Soft, Seen.c_str());
			std::printf("every nation's wrecks by their paints' stems: the M3's canvas seats, %d points in %d M3s, burnt away; the T-34's logs, %d points, dressed among %d points of charred wood; %d thrown turrets' open rings %.2f to %.2f m across their middles, each within her turret's own base\n",
				Seats, SeatsChecked, Logs, LogsWood, Rings, RingLeast < 1e8 ? RingLeast : 0.0, RingMost);
			if (Soft == 0 || Wrong > 0) Fault(Name + ": " + std::to_string(Wrong) + " wrecks with their canvas, seats, glass, wood, bed or tyres not burnt as asked");
			if (SeatsChecked > 0 && Seats == 0) Fault(Name + ": the M3's canvas seats not found");
			if (RingsWrong > 0) Fault(Name + ": " + std::to_string(RingsWrong) + " thrown turrets' open rings out of her turret's own base");
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
			std::printf("none asked: %d laid, %d read, %d places covered; nothing fought over and no live battle's ground (case 9 lays a live battle's): %d laid\n", N0, None.Read, Covered, N1);
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
			// The Soviet lorry close (the review of round 2: her bed's floor, not her benches, laid again as planks with gaps
			// that show the ground): the first of twenty Soviet wrecks laid as the stand-in lays them, a week on, from above
			// (north up) and from the south-east, 40 degrees up.
			{
				LandHulk::Ask Q = AskOf(20, 1.0);
				Q.bAsLaid = true;
				Q.Nats = { { "SOV", 1 } };
				LandHulk::Yard Sov;
				Sov.Lay(F, Q, ReadModel, Have);
				size_t Lorry = Sov.Hulks().size();
				for (size_t K = 0; K < Sov.Hulks().size() && Lorry == Sov.Hulks().size(); ++K)
					if (Sov.Hulks()[K].Model == "Truck") Lorry = K;
				if (Lorry < Sov.Hulks().size())
				{
					const LandHulk::Hulk& H = Sov.Hulks()[Lorry];
					const int LorryW = 520, LorryH = 420;
					Image Shot(LorryW * 2, LorryH);
					const V3 Anchor(H.X, H.Y, H.Z);
					LandField::Mesh Ground = GroundAround(G, H.X, H.Y, 9.0, 0.1, Anchor), Scorched, Her;
					LandHulk::Dressing Nd;
					Nd.Months = 0.25;
					Sov.Patch(Lorry, Scorched, &Nd);
					Sov.Dress(Lorry, true, Her, &Nd);
					const View Above = View::Looking(V3(H.X, H.Y, H.Z + 1.0), 0.0, 1.5707, 62.0, 0, 0, LorryW, LorryH);
					const View Side = View::Looking(V3(H.X, H.Y, H.Z + 1.0), 2.35, 0.7, 58.0, LorryW, 0, 2 * LorryW, LorryH);
					for (const View* V : { &Above, &Side })
					{
						Draw(Shot, *V, Ground, Anchor, Sun);
						Draw(Shot, *V, Scorched, Anchor, Sun);
						Draw(Shot, *V, Her, Anchor, Sun);
					}
					for (int E = 0; E < LorryH; ++E) Shot.Put(LorryW, E, 34, 34, 36);
					Shot.Save(OutDir + "/lorry_" + Name + ".ppm");
					std::ofstream(OutDir + "/lorry_" + Name + ".txt") << "8 8 SOV lorry a week on, from above (north up): " << Nd.Planks << " planks, " << Nd.PlanksGone
																	   << " burnt through, the ground through the gaps\n"
																	   << LorryW + 8 << " 8 from the south-east, 40 degrees up: bare hoops, no glass, rims\n";
				}
			}
			std::printf("pictures: %s/placement_%s.ppm, %s/look_%s.ppm, %s/look_far_%s.ppm, %s/look_side_%s.ppm, %s/scorch_%s.ppm, %s/lorry_%s.ppm%s (their words beside them, .txt)\n", OutDir.c_str(), Name.c_str(),
				OutDir.c_str(), Name.c_str(), OutDir.c_str(), Name.c_str(), OutDir.c_str(), Name.c_str(), OutDir.c_str(), Name.c_str(), OutDir.c_str(), Name.c_str(), Tracked.empty() ? "" : (", " + OutDir + "/track_" + Name + ".ppm").c_str());
		}

		// 9. A live battle (release 2: every battle the campaign plays, the rules fighting it as it is shown): the same
		// battle as its header opens it, nothing fought (its tracks, rounds, events, air notes, craters, cuts, wires,
		// views and phases taken away, bLive set, the setup step's room the attacker's zone, as live.ts opens it), laid as
		// the scene lays it (LandHulk::FieldOf reads a live header's Likely). Off the ground its men and machines are likely
		// to cross: its start areas, the ground between its fronts, its lanes LiveLaneClear; clear of where every unit of
		// the battle stood at its start (the box of each side's first positions) by StartClear; every point on open ground
		// as above; the same twice. Then the battle as it was fought read against them piece by piece, as the scene's
		// guard reads the live pieces (Yard::Reached): each wreck it comes near cleared away, the same as read whole, and
		// every wreck left standing as clear of its ways as a recorded battle's wrecks are of its own.
		{
			LandFeed::Recording LiveRec = Rec;
			LiveRec.bLive = true;
			LiveRec.Tracks.clear();
			LiveRec.Rounds.clear();
			LiveRec.Events.clear();
			LiveRec.Air.clear();
			LiveRec.Craters.clear();
			LiveRec.Cuts.clear();
			LiveRec.Wires.clear();
			LiveRec.WireCuts.clear();
			LiveRec.WireMends.clear();
			LiveRec.Held.clear();
			LiveRec.Aid.clear();
			LiveRec.Views.clear();
			LiveRec.Phases.clear();
			LiveRec.Contact = -1.0;
			LiveRec.End = 0.0;
			LiveRec.Fought = 0.0;
			LiveRec.bOver = false;
			LiveRec.bSetup = true;
			for (int K = 0; K < 4; ++K) LiveRec.Room[K] = Rec.Ground.ZoneAtt[K];
			const LandHulk::Field FL = LandHulk::FieldOf(LiveRec, G, nullptr, {});
			std::printf("\n-- %s, a live battle (its header alone, nothing fought): %zu points stand for the ground fought over; %zu vehicles' ways, %zu men's, %zu blasts; fronts %.0f m (the attacker's) and %.0f m (the defender's) as drawn up%s; %zu old lines and %zu places a fight gathers\n",
				Name.c_str(), FL.Fought.size(), FL.Vehicles.size(), FL.Men.size(), FL.Blasts.size(), FL.Live.FrontAtt, FL.Live.FrontDef, FL.Live.bJoins ? ", a corps from next door coming up" : "",
				FL.Lines.Lines.size(), FL.Lines.Spots.size());
			if (!FL.Live.bOn || !FL.Vehicles.empty() || !FL.Men.empty() || FL.bAxis) Fault(Name + ": a live header not read as a live battle's field");
			{
				LandHulk::Yard None;
				const int N0 = None.Lay(FL, AskOf(0, 1.0), ReadModel, Have);
				std::printf("%s, a live battle with none asked: %d laid, %d read\n", Name.c_str(), N0, None.Read);
				if (N0 != 0 || None.Read != 0) Fault(Name + ": a live battle's wrecks with none asked");
			}
			if (!LiveRec.Blocks.empty())
			{
				// (a corps from next door coming up, its last block's: it walks on by an edge, so every edge kept JoinClear)
				LandFeed::Recording Joined = LiveRec;
				Joined.Blocks.back().bJoins = true;
				const LandHulk::Field FJ = LandHulk::FieldOf(Joined, G, nullptr, {});
				LandHulk::Yard YJ;
				const int NJ = YJ.Lay(FJ, AskOf(40, 1.0), ReadModel, Have);
				double EdgeMin = 1e9;
				for (const Spot& S : SpotsOf(YJ, 3)) EdgeMin = std::min({ EdgeMin, S.X, Rec.Ground.W - S.X, S.Y, Rec.Ground.H - S.Y });
				std::printf("%s, a live battle with a corps from next door coming up: %d laid, every point of them at least %.1f m in from the field's edges (it walks on some 200 m)\n",
					Name.c_str(), NJ, EdgeMin);
				if (!FJ.Live.bJoins || EdgeMin < LandHulk::JoinClear) Fault(Name + ": a live battle's wreck on a corps from next door's way on");
			}
			LandHulk::Yard YL;
			const auto LiveT0 = std::chrono::steady_clock::now();
			const int LiveLaid = YL.Lay(FL, AskOf(40, 1.0), ReadModel, Have);
			std::printf("live laid: %s (%.0f ms, %d models read)\n", YL.Said().c_str(), Since(LiveT0), YL.Read);
			for (size_t K = 0; K < YL.Hulks().size(); ++K)
				std::printf("  %2zu  (live) %s at (%.0f, %.0f), group %d\n", K + 1, Describe(YL.Hulks()[K]).c_str(), YL.Hulks()[K].X, YL.Hulks()[K].Y, YL.Hulks()[K].Group + 1);
			if (LiveLaid == 0) Fault(Name + ": none laid on a live battle");
			if (!YL.bLive || YL.Said().find("live battle") == std::string::npos) Fault(Name + ": the live battle's lay not said");
			CheckPlaces(YL, Name + ", a live battle", nullptr, nullptr, true);

			// (where the rules drew each side up, against where its units stood at the battle's start: each block's middle,
			// the mean of its men's and machines' first points, to the nearest of its side's modelled blocks' boxes; and each
			// of them alone, whose formation reaches past its block's box: a battery's lorries behind its guns, its observers
			// forward with the line)
			double ModelMiss[2] = { 0.0, 0.0 }, BlockMiss[2] = { 0.0, 0.0 };
			int Stood = 0, InZone = 0, Within[2] = { 0, 0 }, Counted[2] = { 0, 0 }, Blocks[2] = { 0, 0 };
			std::map<std::string, int> OutRoles;
			std::map<int, std::array<double, 3>> Middles;
			Near Firsts;
			const auto OffBoxes = [&FL](int Side, double X, double Y)
			{
				double Off = 1e9;
				for (const std::vector<double>& Box : FL.Live.Stands)
				{
					if (Box.size() < 5 || static_cast<int>(Box[4]) != Side) continue;
					Off = std::min(Off, std::hypot(std::max({ Box[0] - X, 0.0, X - Box[2] }), std::max({ Box[1] - Y, 0.0, Y - Box[3] })));
				}
				return Off;
			};
			for (const LandFeed::Track& T : Rec.Tracks)
			{
				const LandFeed::Block* B = Rec.BlockOf(T.Block);
				if (T.At.empty() || !B || B->Side < 0 || B->Side > 1 || B->bJoins) continue;
				const double X = T.At[0].X, Y = T.At[0].Y;
				Firsts.Add(X, Y);
				++Stood;
				const double* Z = B->Side == 0 ? Rec.Ground.ZoneAtt : Rec.Ground.ZoneDef;
				InZone += X >= Z[0] && X <= Z[2] && Y >= Z[1] && Y <= Z[3] ? 1 : 0;
				const double Off = OffBoxes(B->Side, X, Y);
				const size_t Side = static_cast<size_t>(B->Side);
				ModelMiss[Side] = std::max(ModelMiss[Side], Off);
				Within[Side] += Off <= 0.0 ? 1 : 0;
				if (Off > 0.0) ++OutRoles[T.Role];
				++Counted[Side];
				std::array<double, 3>& Mid = Middles[T.Block];
				Mid[0] += X;
				Mid[1] += Y;
				Mid[2] += 1.0;
			}
			for (const auto& [Id, Mid] : Middles)
			{
				const LandFeed::Block* B = Rec.BlockOf(Id);
				const size_t Side = static_cast<size_t>(B->Side);
				BlockMiss[Side] = std::max(BlockMiss[Side], OffBoxes(B->Side, Mid[0] / Mid[2], Mid[1] / Mid[2]));
				++Blocks[Side];
			}
			std::string OutSaid;
			for (const auto& [Role, Count] : OutRoles) OutSaid += (OutSaid.empty() ? "" : ", ") + std::to_string(Count) + " " + Role;
			std::printf("%s, a live battle, where the rules draw it up (units.ts drawUp%s%s): each block's middle (its men's and machines' first points) against the modelled blocks: the attacker's %d, the farthest %.1f m out of its box; the defender's %d, %.1f m; each of them alone, the attacker's %d of %d in their boxes, the farthest %.1f m out; the defender's %d of %d, %.1f m (out: %s); %d of %d in their own side's zone\n",
				Name.c_str(), FL.Live.bDrawnUp[0] ? "" : "; the attacker's not modelled, its zone kept", FL.Live.bDrawnUp[1] ? "" : "; the defender's not modelled, its zone kept",
				Blocks[0], BlockMiss[0], Blocks[1], BlockMiss[1], Within[0], Counted[0], ModelMiss[0], Within[1], Counted[1], ModelMiss[1], OutSaid.empty() ? "none" : OutSaid.c_str(), InZone, Stood);
			if (BlockMiss[0] > 5.0 || BlockMiss[1] > 5.0) Fault(Name + ": a live battle's drawn-up model more than 5 m off the middle of where a block stood at its start");
			if (ModelMiss[0] > LandHulk::StartClear - 15.0 || ModelMiss[1] > LandHulk::StartClear - 15.0)
				Fault(Name + ": a live battle's man or machine at its start less than 15 m inside the ground kept round where its blocks draw up");

			// (off the likely ground's core; off the rest of it too but for the groups along the old lines and at the
			// junctions and the village's edge; clear of where every unit stood at the start by StartClear, less how far the
			// model missed them)
			double CoreMin = 1e9, OtherMin = 1e9, StartMin = 1e9;
			std::vector<uint8_t> Outer(YL.Hulks().size(), 0);
			for (const Spot& S : SpotsOf(YL, 3))
			{
				const double Core = LandHulk::FromCore(FL, S.X, S.Y), All = LandHulk::FromLikely(FL, S.X, S.Y);
				CoreMin = std::min(CoreMin, Core);
				const LandHulk::Where At = YL.Hulks()[S.K].At;
				if (At != LandHulk::Where::OldLine && At != LandHulk::Where::Spot) OtherMin = std::min(OtherMin, All);
				if (All < 0.0) Outer[S.K] = 1;
				StartMin = std::min(StartMin, Firsts.To(S.X, S.Y, 400.0));
			}
			std::string OuterSaid;
			for (size_t K = 0; K < Outer.size(); ++K)
				if (Outer[K] != 0) OuterSaid += (OuterSaid.empty() ? "" : ", ") + std::to_string(K + 1);
			std::printf("%s, a live battle: every point at least %.1f m off the core of the ground it is likely to cross (where its blocks draw up and the setup room grown %.0f m, the ground between its fronts and %.0f m round it, the field's edges%s); the groups on the axis or strewn at least %.1f m off all of it (the rest of its start areas and what it falls back over too); %d groups along the old lines or at the village's edge or a junction lie on that rest (wrecks %s); every point %.1f m from where any of its units stood at its start\n",
				Name.c_str(), CoreMin, LandHulk::StartClear, LandHulk::AdvanceMargin, FL.Live.bJoins ? ", the side edges a corps from next door walks on by" : "", OtherMin, YL.OnLikely,
				OuterSaid.empty() ? "none" : OuterSaid.c_str(), StartMin);
			if (LiveLaid > 0 && CoreMin < 0.0) Fault(Name + ": a live battle's wreck on the core of the ground it is likely to cross");
			if (LiveLaid > 0 && OtherMin < 0.0) Fault(Name + ": a live battle's wreck on the axis or strewn on the ground it is likely to cross");
			if (LiveLaid > 0 && StartMin < LandHulk::StartClear - std::max(ModelMiss[0], ModelMiss[1]) - 1.0)
				Fault(Name + ": a live battle's wreck within " + std::to_string(static_cast<int>(LandHulk::StartClear)) + " m of where its units stood at its start (less the model's miss)");

			// (the battle as it was fought against that ground: how far its ways went out of the core, and out of the rest)
			{
				size_t Points = 0, OutCore = 0, OutAll = 0;
				double CoreMost = -1e9, AllMost = -1e9;
				for (const LandFeed::Track& T : Rec.Tracks)
				{
					const LandFeed::Block* B = Rec.BlockOf(T.Block);
					if (B && B->bJoins) continue;
					for (const LandFeed::Point& P : T.At)
					{
						const double Core = LandHulk::FromCore(FL, P.X, P.Y), All = LandHulk::FromLikely(FL, P.X, P.Y);
						++Points;
						OutCore += Core > 0.0 ? 1 : 0;
						OutAll += All > 0.0 ? 1 : 0;
						CoreMost = std::max(CoreMost, Core);
						AllMost = std::max(AllMost, All);
					}
				}
				std::printf("%s, the battle as it was fought against that ground: %zu of %zu kept points of its men's and machines' ways off its core (the farthest %.1f m off), %zu off all of it (%.1f m)\n",
					Name.c_str(), OutCore, Points, std::max(0.0, CoreMost), OutAll, std::max(0.0, AllMost));
			}

			// (the same twice: every wreck, her pose and her near, far and patch meshes)
			{
				LandHulk::Yard Again;
				Again.Lay(FL, AskOf(40, 1.0), ReadModel, Have);
				bool bSame = Again.Hulks().size() == YL.Hulks().size() && Again.Said() == YL.Said();
				for (size_t K = 0; bSame && K < YL.Hulks().size(); ++K)
				{
					const LandHulk::Hulk &P = YL.Hulks()[K], &Q = Again.Hulks()[K];
					bSame = P.X == Q.X && P.Y == Q.Y && P.Z == Q.Z && P.Heading == Q.Heading && P.Model == Q.Model && P.Nation == Q.Nation && P.Turret == Q.Turret
						&& P.bTurretOff == Q.bTurretOff && P.TrackOff == Q.TrackOff && P.Open == Q.Open && P.Seed == Q.Seed;
					for (int Level = 0; bSame && Level < 3; ++Level)
					{
						LandField::Mesh Mp, Mq;
						if (Level < 2)
						{
							YL.Dress(K, Level == 0, Mp);
							Again.Dress(K, Level == 0, Mq);
						}
						else
						{
							YL.Patch(K, Mp);
							Again.Patch(K, Mq);
						}
						bSame = HashOf(Mp) == HashOf(Mq);
					}
				}
				std::printf("%s, a live battle, the same twice: %s\n", Name.c_str(), bSame ? "yes (every wreck, its pose and its meshes, near, far and its patch)" : "NO");
				if (!bSame) Fault(Name + ": a live battle's wrecks not the same twice");
			}

			// (her outline and patch for a live battle's shell holes, Over: every point of her near level and her patch
			// within half a metre of what it reads; and how many of the battle's own shells, bombs and grenades, as it was
			// fought, land on a wreck or her patch, their holes then not dug: the scene's Dig)
			{
				size_t Points = 0, Missed = 0;
				for (size_t K = 0; K < YL.Hulks().size(); ++K)
				{
					const LandHulk::Hulk& Hk = YL.Hulks()[K];
					for (int Level = 0; Level < 2; ++Level)
					{
						LandField::Mesh M;
						if (Level == 0) YL.Dress(K, true, M);
						else YL.Patch(K, M);
						for (size_t I = 0; I < M.Colours.size(); I += 2)
						{
							++Points;
							if (!YL.Over(Hk.X + M.Positions[I * 3], Hk.Y + M.Positions[I * 3 + 1], 0.5)) ++Missed;
						}
					}
				}
				int OnHer = 0;
				for (const auto& P : F.Blasts) OnHer += YL.Over(P.first, P.second, 1.0) ? 1 : 0;
				std::printf("%s, a live battle: %zu of %zu points of the wrecks and their patches outside what Over reads; %d of the battle's %zu shells, bombs and grenades land on a wreck or her patch (their holes not dug there)\n",
					Name.c_str(), Missed, Points, OnHer, F.Blasts.size());
				if (Missed > 0) Fault(Name + ": a live battle's wreck or patch outside what Over reads (a hole would open under her)");
			}

			// The battle as it was fought, read as the scene's guard reads the live pieces (every 30 s of battle, each
			// moving man's last point provisional, as a live piece gives it), and read whole: the same wrecks marked, each
			// with the same time a man or machine first comes near her. And each piece's cost at a live piece's own
			// cadence (every track's last kept point and its provisional one read, as a half-second piece gives them).
			LandHulk::Yard Pieces = YL, Whole = YL, Cost = YL;
			std::vector<size_t> Seen, SeenWhole;
			std::vector<size_t> ByPiece;
			// (and as release 2 plays a live battle: no machine pinned (the live drill stops a knocked-out one where she is
			// hit, so her track never jumps: here her points from the pin on already stand there), the crews bailing out
			// (the ship list's bail-out, on) and the tanks rolled smoothly (smoothtanks, on: 0.4 s))
			constexpr double ShipBlend = 0.4;
			LandHulk::Yard PiecesR2 = YL, WholeR2 = YL;
			std::vector<size_t> SeenR2, SeenWholeR2, ByPieceR2;
			LandFeed::Recording RecR2 = Rec;
			for (LandFeed::Track& T : RecR2.Tracks) T.bPinned = false;
			std::vector<uint8_t> EverHeading(YL.Hulks().size(), 0);
			int PieceCount = 0;
			double Last = 0.0, CostMs = 0.0, CostMost = 0.0;
			for (const LandFeed::Track& T : Rec.Tracks)
				if (!T.At.empty()) Last = std::max(Last, T.At.back().T);
			for (double Cut = 30.0; Cut < Last + 30.0; Cut += 30.0)
			{
				LandFeed::Recording Part;
				Part.Tracks.reserve(Rec.Tracks.size());
				for (const LandFeed::Track& T : Rec.Tracks)
				{
					LandFeed::Track P;
					P.Block = T.Block;
					P.K = T.K;
					P.J = T.J;
					P.Role = T.Role;
					size_t Kept = 0;
					while (Kept < T.At.size() && T.At[Kept].T <= Cut) ++Kept;
					P.At.assign(T.At.begin(), T.At.begin() + static_cast<std::ptrdiff_t>(Kept));
					if (Kept > 0 && Kept < T.At.size())
					{
						// (where he stands at the cut, between his last kept point and the next: provisional)
						const LandFeed::Point& A0 = T.At[Kept - 1];
						const LandFeed::Point& B0 = T.At[Kept];
						const double U = B0.T > A0.T ? (Cut - A0.T) / (B0.T - A0.T) : 0.0;
						LandFeed::Point Tail;
						Tail.T = Cut;
						Tail.X = A0.X + (B0.X - A0.X) * U;
						Tail.Y = A0.Y + (B0.Y - A0.Y) * U;
						Tail.F = A0.F;
						P.At.push_back(Tail);
						P.bTail = true;
					}
					P.bPinned = T.bPinned && T.PinFrom <= Cut;
					P.PinFrom = T.PinFrom;
					P.PinX = T.PinX;
					P.PinY = T.PinY;
					P.Fell = T.Fell <= Cut ? T.Fell : -1.0;
					P.Out = T.Out <= Cut ? T.Out : -1.0;
					Part.Tracks.push_back(std::move(P));
				}
				for (const size_t K : Pieces.Reached(Part, Seen)) ByPiece.push_back(K);
				for (size_t K = 0; K < EverHeading.size(); ++K) EverHeading[K] = EverHeading[K] != 0 || Pieces.Heading(K) ? 1 : 0;
				{
					LandFeed::Recording PartR2 = Part;
					for (LandFeed::Track& T : PartR2.Tracks) T.bPinned = false;
					for (const size_t K : PiecesR2.Reached(PartR2, SeenR2, LandHulk::BailReach, ShipBlend)) ByPieceR2.push_back(K);
				}
				// (a live piece's cost: each track read from its last kept point before the provisional one)
				std::vector<size_t> Cadence(Part.Tracks.size(), 0);
				for (size_t T = 0; T < Part.Tracks.size(); ++T) Cadence[T] = Part.Tracks[T].At.size() >= 2 ? Part.Tracks[T].At.size() - 2 : 0;
				const auto CostT0 = std::chrono::steady_clock::now();
				Cost.Reached(Part, Cadence);
				const double Ms = Since(CostT0);
				CostMs += Ms;
				CostMost = std::max(CostMost, Ms);
				++PieceCount;
			}
			for (const size_t K : Pieces.Reached(Rec, Seen)) ByPiece.push_back(K);
			std::vector<size_t> ByWhole = Whole.Reached(Rec, SeenWhole);
			for (const size_t K : PiecesR2.Reached(RecR2, SeenR2, LandHulk::BailReach, ShipBlend)) ByPieceR2.push_back(K);
			const std::vector<size_t> ByWholeR2 = WholeR2.Reached(RecR2, SeenWholeR2, LandHulk::BailReach, ShipBlend);
			std::vector<size_t> PieceSet = ByPiece, WholeSet = ByWhole;
			std::sort(PieceSet.begin(), PieceSet.end());
			std::sort(WholeSet.begin(), WholeSet.end());
			bool bSameDue = true;
			for (const size_t K : WholeSet) bSameDue = bSameDue && std::fabs(Pieces.DueOf(K) - Whole.DueOf(K)) < 1e-6;
			std::string Which;
			for (const size_t K : WholeSet)
			{
				char One[64];
				std::snprintf(One, sizeof(One), "%zu at %.1f s", K + 1, Whole.DueOf(K));
				Which += (Which.empty() ? "" : ", ") + std::string(One);
			}
			std::string Headed;
			int HeadedOnly = 0;
			for (size_t K = 0; K < EverHeading.size(); ++K)
			{
				if (EverHeading[K] == 0) continue;
				Headed += (Headed.empty() ? "" : ", ") + std::to_string(K + 1);
				HeadedOnly += Whole.Marked(K) ? 0 : 1;
			}
			// (every wreck left standing against the battle's ways as drawn: each kept point to the next, every 0.25 m, and
			// a knocked-out machine's pin; her every point, as a recorded battle's wrecks are read)
			Near LiveVehicles, LiveMen;
			for (const LandFeed::Track& T : Rec.Tracks)
			{
				Line L;
				for (const LandFeed::Point& P : T.At) L.emplace_back(P.X, P.Y);
				Near& To = T.IsVehicle() ? LiveVehicles : LiveMen;
				To.AddLine(L, 0.25);
				if (T.bPinned) To.Add(T.PinX, T.PinY);
			}
			double StandVehicle = 1e9, StandMan = 1e9, GoneVehicle = 1e9, GoneMan = 1e9;
			for (const Spot& S : SpotsOf(YL, 3))
			{
				const bool bGone = Whole.Marked(S.K);
				double& V = bGone ? GoneVehicle : StandVehicle;
				double& M = bGone ? GoneMan : StandMan;
				V = std::min(V, LiveVehicles.To(S.X, S.Y, 30.0));
				M = std::min(M, LiveMen.To(S.X, S.Y, 10.0));
			}
			// (each one marked: no way came within its reach of her before her time, every way read up to it)
			double BeforeDue = 1e9;
			for (const size_t K : WholeSet)
			{
				const LandHulk::Hulk& Hk = YL.Hulks()[K];
				const double Due = Whole.DueOf(K);
				for (const LandFeed::Track& T : Rec.Tracks)
				{
					const double Far = T.IsVehicle() ? LandHulk::VehicleReach : LandHulk::ManReach;
					for (size_t I = 1; I < T.At.size(); ++I)
					{
						const LandFeed::Point &P0 = T.At[I - 1], &P1 = T.At[I];
						if (P0.T >= Due) break;
						const double U = P1.T > P0.T ? std::min(1.0, (Due - 1e-6 - P0.T) / (P1.T - P0.T)) : 1.0;
						const double Bx = P0.X + (P1.X - P0.X) * U, By = P0.Y + (P1.Y - P0.Y) * U;
						for (const LandHulk::Ring& R : Hk.Rings)
						{
							const double Dx = Bx - P0.X, Dy = By - P0.Y, L2 = Dx * Dx + Dy * Dy;
							const double W = L2 > 0.0 ? std::clamp(((R.X - P0.X) * Dx + (R.Y - P0.Y) * Dy) / L2, 0.0, 1.0) : 0.0;
							BeforeDue = std::min(BeforeDue, std::hypot(P0.X + W * Dx - R.X, P0.Y + W * Dy - R.Y) - R.R - Far);
						}
					}
					if (T.bPinned && T.PinFrom < Due)
						for (const LandHulk::Ring& R : Hk.Rings) BeforeDue = std::min(BeforeDue, std::hypot(T.PinX - R.X, T.PinY - R.Y) - R.R - Far);
				}
			}
			char Gone[160] = "none marked";
			if (!WholeSet.empty())
				std::snprintf(Gone, sizeof(Gone), "those marked came within %.1f and %.1f m, and no way within its reach of one before her time (by %.2f m at the least)", GoneVehicle < 1e8 ? GoneVehicle : 0.0,
					GoneMan < 1e8 ? GoneMan : 0.0, BeforeDue < 1e8 ? BeforeDue : 0.0);
			std::printf("%s, a live battle fought as recorded, read as the guard reads its pieces (%d pieces of 30 s, then whole): %zu of %zu wrecks marked to be cleared (%s), %s, %s; %s; those left standing at least %.1f m from a vehicle's way and %.1f m from a man's (read out to 30 and 10 m); a man or machine heading for %s at some piece's end (%d of them never marked)\n",
				Name.c_str(), PieceCount, WholeSet.size(), YL.Hulks().size(), Which.empty() ? "none" : Which.c_str(), PieceSet == WholeSet ? "the same as read whole" : "NOT the same as read whole",
				bSameDue ? "each at the same time" : "NOT at the same times", Gone, StandVehicle, StandMan, Headed.empty() ? "none" : Headed.c_str(), HeadedOnly);
			std::printf("%s, a live battle: the guard's cost at a live piece's cadence (every track's last two kept points read against every wreck, and each moving one's way on): %.3f ms a piece on the average, %.3f ms at the most, over %d pieces\n",
				Name.c_str(), PieceCount > 0 ? CostMs / PieceCount : 0.0, CostMost, PieceCount);
			if (PieceSet != WholeSet || ByPiece.size() != PieceSet.size() || !bSameDue) Fault(Name + ": the guard read piece by piece not as read whole (or a wreck marked twice, or at another time)");
			{
				// (release 2's read: the same piece by piece as whole; every wreck the read above marks marked too, none
				// later; and every wreck left standing at least BailReach from where a knocked-out tank stands, her place
				// at her fall, and as clear of the ways as above)
				std::vector<size_t> PieceSetR2 = ByPieceR2, WholeSetR2 = ByWholeR2;
				std::sort(PieceSetR2.begin(), PieceSetR2.end());
				std::sort(WholeSetR2.begin(), WholeSetR2.end());
				bool bSameDueR2 = true, bCovers = true;
				for (const size_t K : WholeSetR2) bSameDueR2 = bSameDueR2 && std::fabs(PiecesR2.DueOf(K) - WholeR2.DueOf(K)) < 1e-6;
				for (const size_t K : WholeSet) bCovers = bCovers && WholeR2.Marked(K) && WholeR2.DueOf(K) <= Whole.DueOf(K) + 1e-9;
				std::string WhichR2;
				for (const size_t K : WholeSetR2)
				{
					char One[64];
					std::snprintf(One, sizeof(One), "%zu at %.1f s", K + 1, WholeR2.DueOf(K));
					WhichR2 += (WhichR2.empty() ? "" : ", ") + std::string(One);
				}
				Near DeadTanks;
				int DeadCount = 0;
				for (const LandFeed::Track& T : RecR2.Tracks)
				{
					if (T.Role != "tank" || T.Fell < 0.0 || T.At.empty()) continue;
					// (her place a hair before her fall, her kept points joined straight, as LandKit's PlaceAt holds her)
					const double When = T.Fell - 1e-3;
					size_t Hi = 0;
					while (Hi < T.At.size() && T.At[Hi].T <= When) ++Hi;
					double Px = T.At[Hi == 0 ? 0 : Hi - 1].X, Py = T.At[Hi == 0 ? 0 : Hi - 1].Y;
					if (Hi > 0 && Hi < T.At.size())
					{
						const LandFeed::Point& L = T.At[Hi - 1];
						const LandFeed::Point& H = T.At[Hi];
						const double Span = H.T - L.T;
						if (Span > 1e-9 && std::hypot(H.X - L.X, H.Y - L.Y) <= 25.0 * Span)
						{
							Px = L.X + (H.X - L.X) * (When - L.T) / Span;
							Py = L.Y + (H.Y - L.Y) * (When - L.T) / Span;
						}
					}
					DeadTanks.Add(Px, Py);
					++DeadCount;
				}
				double StandVehicleR2 = 1e9, StandManR2 = 1e9, StandCrew = 1e9;
				for (const Spot& S : SpotsOf(YL, 3))
				{
					if (WholeR2.Marked(S.K)) continue;
					StandVehicleR2 = std::min(StandVehicleR2, LiveVehicles.To(S.X, S.Y, 30.0));
					StandManR2 = std::min(StandManR2, LiveMen.To(S.X, S.Y, 10.0));
				}
				// (the crews' reach from her outline, her rings, as the guard reads it)
				for (size_t K = 0; K < YL.Hulks().size(); ++K)
				{
					if (WholeR2.Marked(K)) continue;
					for (const LandHulk::Ring& R : YL.Hulks()[K].Rings) StandCrew = std::min(StandCrew, DeadTanks.To(R.X, R.Y, 100.0 + R.R) - R.R);
				}
				std::printf("%s, a live battle fought as recorded, as release 2 plays it (no machine pinned, crews bailing out to %.0f m of a knocked-out tank, %d of them, tanks rolled smoothly over %.1f s), read as the guard reads its pieces: %zu of %zu wrecks marked to be cleared (%s), %s, %s; every one the read above marks marked too, none later: %s; those left standing at least %.1f m from a knocked-out tank (her outline, read out to 100 m), %.1f m from a vehicle's way and %.1f m from a man's (read out to 30 and 10 m)\n",
					Name.c_str(), LandHulk::BailReach, DeadCount, ShipBlend, WholeSetR2.size(), YL.Hulks().size(), WhichR2.empty() ? "none" : WhichR2.c_str(),
					PieceSetR2 == WholeSetR2 && ByPieceR2.size() == PieceSetR2.size() ? "the same as read whole" : "NOT the same as read whole", bSameDueR2 ? "each at the same time" : "NOT at the same times",
					bCovers ? "yes" : "NO", StandCrew, StandVehicleR2, StandManR2);
				if (PieceSetR2 != WholeSetR2 || ByPieceR2.size() != PieceSetR2.size() || !bSameDueR2) Fault(Name + ": release 2's guard read piece by piece not as read whole (or a wreck marked twice, or at another time)");
				if (StandCrew < LandHulk::BailReach) Fault(Name + ": a wreck left standing within the crews' reach of a knocked-out tank (release 2's read)");
			}
			if (StandVehicle < LandHulk::VehicleClear) Fault(Name + ": a wreck left standing within 8 m of a live vehicle's way");
			if (StandMan < LandHulk::ManClear) Fault(Name + ": a wreck left standing within 1.5 m of a live man's way");
			if (BeforeDue < -0.01) Fault(Name + ": a way within its reach of a marked wreck before her time");
			{
				// (cleared, the eye away: the grass stands where she lay, Covers; Over no longer keeps a hole off her)
				LandHulk::Sight Away;
				Away.EyeX = -5000.0;
				Away.LookX = -6000.0;
				const std::vector<size_t> Swept = Whole.Clear(0.0, Away);
				int StillCovers = 0;
				for (const size_t K : Swept)
				{
					const LandHulk::Hulk& Hk = YL.Hulks()[K];
					StillCovers += Whole.Covers(Hk.X, Hk.Y) || Whole.Over(Hk.X, Hk.Y, 0.0) ? 1 : 0;
				}
				if (StillCovers > 0 || Swept.size() < WholeSet.size()) Fault(Name + ": a wreck the guard cleared still kept the grass or a hole off (or one marked not cleared with the eye away)");
			}
			if (YL.Hulks().size() >= 3)
			{
				// The guard made to work, each on its own copy of the yard so only what it sends counts: a tank sent along x
				// straight through wreck 1 in three pieces (the first two ending on a provisional point, the second's 30 m
				// short of her reach, the third's next point past her); a man walking past wreck 2 just out of his reach,
				// then through her middle; a knocked-out lorry far off, then pinned on wreck 3; then the men placed again
				// (fewer tracks), one standing on wreck 3 marked before. Then when each goes: the eye on her, away, on a cut,
				// and on her at her time; a tank heading for her, not yet near; a tank's crew bailing out; a man leaving.
				const LandHulk::Hulk& H1 = YL.Hulks()[0];
				const LandHulk::Hulk& H2 = YL.Hulks()[1];
				const LandHulk::Hulk& H3 = YL.Hulks()[2];
				const auto Way = [](const char* Role, std::initializer_list<LandFeed::Point> Points)
				{
					LandFeed::Track T;
					T.Role = Role;
					T.At.assign(Points.begin(), Points.end());
					return T;
				};
				const auto At = [](double T, double X, double Y)
				{
					LandFeed::Point P;
					P.T = T;
					P.X = X;
					P.Y = Y;
					return P;
				};
				const auto Has = [](const std::vector<size_t>& L, size_t K) { return std::find(L.begin(), L.end(), K) != L.end(); };
				// (the eye: 120 m south of a wreck and 30 m up, looking at her; or looking the other way; or 6 km off)
				const auto On = [](const LandHulk::Hulk& Her)
				{
					LandHulk::Sight E;
					E.EyeX = Her.X;
					E.EyeY = Her.Y - 120.0;
					E.EyeZ = Her.Z + 30.0;
					E.LookX = Her.X;
					E.LookY = Her.Y;
					E.LookZ = Her.Z;
					return E;
				};
				const auto Off = [&On](const LandHulk::Hulk& Her)
				{
					LandHulk::Sight E = On(Her);
					E.LookY = Her.Y - 240.0;
					return E;
				};
				// (the tank: her westmost ring's edge, then 30 m and a vehicle's reach short of it)
				double West = H1.X;
				for (const LandHulk::Ring& R : H1.Rings) West = std::min(West, R.X - R.R);
				const double Short = West - LandHulk::VehicleReach - 30.0;
				LandHulk::Yard Tank = YL;
				std::vector<size_t> TankSeen;
				LandFeed::Recording T1, T2, T3;
				T1.Tracks.push_back(Way("tank", { At(0, Short - 200.0, H1.Y), At(30, Short - 100.0, H1.Y) }));
				T1.Tracks[0].bTail = true;
				T2.Tracks.push_back(Way("tank", { At(0, Short - 200.0, H1.Y), At(30, Short - 100.0, H1.Y), At(60, Short, H1.Y) }));
				T2.Tracks[0].bTail = true;
				T3.Tracks.push_back(Way("tank", { At(0, Short - 200.0, H1.Y), At(30, Short - 100.0, H1.Y), At(90, H1.X + 60.0, H1.Y) }));
				const std::vector<size_t> CT1 = Tank.Reached(T1, TankSeen);
				const std::vector<size_t> CT2 = Tank.Reached(T2, TankSeen);
				const bool bTankShort = !Tank.Marked(0) && !Has(CT1, 0) && !Has(CT2, 0);
				const std::vector<size_t> CT3 = Tank.Reached(T3, TankSeen);
				// (her time: when its way from 30 s to 90 s, 60 m past her middle, crosses into her reach; she is not cleared
				// on her mark alone, nor with the eye on her before her time)
				const double TankDue = Tank.DueOf(0);
				const bool bTankThrough = Has(CT3, 0) && Tank.Marked(0) && !Tank.Cleared(0) && TankDue > 60.0 && TankDue < 90.0;
				std::vector<int> How;
				const std::vector<size_t> E1 = Tank.Clear(TankDue - 5.0, On(H1), &How);
				const bool bEyeOn = !Has(E1, 0) && !Tank.Cleared(0) && Tank.Covers(H1.X, H1.Y) && Tank.InSight(0, On(H1));
				const std::vector<size_t> E2 = Tank.Clear(TankDue - LandHulk::ClearLead, On(H1), &How);
				const bool bAtTime = Has(E2, 0) && !How.empty() && How.back() == 2 && Tank.Cleared(0) && !Tank.Over(H1.X, H1.Y, 0.0) && !Tank.Covers(H1.X, H1.Y);
				// (the same, the eye looking away, or cutting, long before her time)
				LandHulk::Yard Fresh = YL;
				std::vector<size_t> FreshSeen;
				Fresh.Reached(T3, FreshSeen);
				LandHulk::Yard AwayYard = Fresh, CutYard = Fresh;
				How.clear();
				const std::vector<size_t> E3 = AwayYard.Clear(TankDue - 20.0, Off(H1), &How);
				const bool bAway = Has(E3, 0) && !How.empty() && How.back() == 0 && !AwayYard.InSight(0, Off(H1));
				LandHulk::Sight Cutting = On(H1);
				Cutting.bCut = true;
				How.clear();
				const std::vector<size_t> E4 = CutYard.Clear(TankDue - 20.0, Cutting, &How);
				const bool bOnCut = Has(E4, 0) && !How.empty() && How.back() == 1;
				// (too small to see go: the eye 6 km off looking at her)
				LandHulk::Sight Afar = On(H1);
				Afar.EyeY = H1.Y - 6000.0;
				const bool bSmall = !YL.InSight(0, Afar);
				// (the man: past wreck 2 at her rings' farthest reach and his own, and 0.3 m more; then through her)
				double Round2 = 0.0;
				for (const LandHulk::Ring& R : H2.Rings) Round2 = std::max(Round2, std::hypot(R.X - H2.X, R.Y - H2.Y) + R.R);
				const double Past = H2.Y + Round2 + LandHulk::ManReach + 0.3;
				LandHulk::Yard Man = YL;
				std::vector<size_t> ManSeen;
				LandFeed::Recording M1, M2;
				M1.Tracks.push_back(Way("rifle", { At(0, H2.X - 10.0, Past), At(30, H2.X + 10.0, Past) }));
				M2.Tracks.push_back(Way("rifle", { At(0, H2.X - 10.0, Past), At(30, H2.X + 10.0, Past), At(60, H2.X, H2.Y) }));
				const std::vector<size_t> CM1 = Man.Reached(M1, ManSeen);
				const bool bManPast = !Has(CM1, 1) && !Man.Marked(1);
				const std::vector<size_t> CM2 = Man.Reached(M2, ManSeen);
				const bool bManThrough = Has(CM2, 1) && Man.Marked(1) && Man.DueOf(1) > 30.0 && Man.DueOf(1) < 60.0;
				// (the lorry: far off, then knocked out and pinned on wreck 3; then the men placed again)
				LandHulk::Yard Pin = YL;
				std::vector<size_t> PinSeen;
				LandFeed::Recording L1, L2, Again;
				L1.Tracks.push_back(Way("lorry", { At(0, H3.X + 300.0, H3.Y + 300.0) }));
				L1.Tracks.push_back(Way("rifle", { At(0, H3.X - 300.0, H3.Y + 300.0) }));
				L2 = L1;
				L2.Tracks[0].bPinned = true;
				L2.Tracks[0].PinFrom = 60.0;
				L2.Tracks[0].PinX = H3.X;
				L2.Tracks[0].PinY = H3.Y;
				Again.Tracks.push_back(Way("rifle", { At(0, H3.X, H3.Y) }));
				const std::vector<size_t> CL1 = Pin.Reached(L1, PinSeen);
				const bool bPinFar = !Has(CL1, 2) && !Pin.Marked(2);
				const std::vector<size_t> CL2 = Pin.Reached(L2, PinSeen);
				const bool bPinned = Has(CL2, 2) && Pin.Marked(2) && Pin.DueOf(2) == 60.0;
				const std::vector<size_t> CL3 = Pin.Reached(Again, PinSeen);
				const bool bAgain = !Has(CL3, 2) && PinSeen.size() == 1;
				// (a tank heading for wreck 1, 60 m short of her reach and coming on at 8 m/s: heading, not marked; she goes
				// with the eye away, never with it on her; the next piece, the tank turned away: no longer heading)
				LandHulk::Yard Head = YL;
				std::vector<size_t> HeadSeen;
				LandFeed::Recording Hd1, Hd2;
				Hd1.Tracks.push_back(Way("tank", { At(0, Short - 30.0 - 16.0, H1.Y), At(2, Short - 30.0, H1.Y) }));
				Hd1.Tracks[0].bTail = true;
				Head.Reached(Hd1, HeadSeen);
				const bool bHeading = Head.Heading(0) && !Head.Marked(0);
				const std::vector<size_t> Hs1 = Head.Clear(2.0, On(H1));
				const bool bHeadStays = !Has(Hs1, 0) && !Head.Cleared(0);
				Hd2.Tracks.push_back(Way("tank", { At(0, Short - 30.0 - 16.0, H1.Y), At(2, Short - 30.0, H1.Y), At(4, Short - 30.0, H1.Y - 16.0) }));
				Hd2.Tracks[0].bTail = true;
				LandHulk::Yard Turned = Head;
				Turned.Reached(Hd2, HeadSeen);
				const bool bTurned = !Turned.Heading(0) && !Turned.Marked(0);
				const std::vector<size_t> Hs2 = Head.Clear(2.0, Off(H1));
				const bool bHeadAway = Has(Hs2, 0) && Head.Cleared(0);
				// (a tank knocked out 40 m off wreck 3: her crew bailing out reaches her where the scene's crews get out, not
				// where they stay inside; a lorry's crew is the crowd's own already)
				LandHulk::Yard Crew = YL, NoCrew = YL, Lorry = YL;
				std::vector<size_t> CrewSeen, NoCrewSeen, LorrySeen;
				LandFeed::Recording Kill;
				Kill.Tracks.push_back(Way("tank", { At(0, H3.X + 40.0, H3.Y + 200.0), At(30, H3.X + 40.0, H3.Y) }));
				Kill.Tracks[0].bPinned = true;
				Kill.Tracks[0].PinFrom = 30.0;
				Kill.Tracks[0].PinX = H3.X + 40.0;
				Kill.Tracks[0].PinY = H3.Y;
				Crew.Reached(Kill, CrewSeen, LandHulk::BailReach);
				NoCrew.Reached(Kill, NoCrewSeen);
				LandFeed::Recording KillLorry = Kill;
				KillLorry.Tracks[0].Role = "lorry";
				Lorry.Reached(KillLorry, LorrySeen, LandHulk::BailReach);
				const bool bCrews = Crew.Marked(2) && Crew.DueOf(2) == 30.0 && !NoCrew.Marked(2) && !Lorry.Marked(2);
				// (a man walking south at wreck 2 and leaving the field with his block where he is 5 m outside his reach of
				// her nearest ring: he is drawn on for a second)
				const auto FromRings2 = [&H2](double X, double Y)
				{
					double Most = 1e18;
					for (const LandHulk::Ring& R : H2.Rings) Most = std::min(Most, std::hypot(X - R.X, Y - R.Y) - R.R);
					return Most;
				};
				double LeaveLo = 0.0, LeaveHi = 200.0;
				for (int K = 0; K < 60; ++K)
				{
					const double Mid = 0.5 * (LeaveLo + LeaveHi);
					if (FromRings2(H2.X, H2.Y + Mid) < LandHulk::ManReach + 5.0) LeaveLo = Mid;
					else LeaveHi = Mid;
				}
				const double LeaveY = H2.Y + LeaveHi;
				LandHulk::Yard Leave = YL;
				std::vector<size_t> LeaveSeen;
				LandFeed::Recording Lv;
				Lv.Tracks.push_back(Way("rifle", { At(0, H2.X, LeaveY + 90.0), At(45, H2.X, LeaveY) }));
				Lv.Tracks[0].Out = 45.0;
				Leave.Reached(Lv, LeaveSeen);
				LandFeed::Recording Stay = Lv;
				Stay.Tracks[0].Out = -1.0;
				LandHulk::Yard Stays = YL;
				std::vector<size_t> StaySeen;
				Stays.Reached(Stay, StaySeen);
				const bool bLeave = Leave.Marked(1) && Leave.DueOf(1) == 45.0 && !Stays.Marked(1);
				// (the same tank as a live battle gives her: no pin (the live drill stops her where she is hit, so her track
				// never jumps), knocked out at 10 s between kept points 100 m either side of wreck 3, 40 m off her: her crews
				// reach her from where she is at her fall, not her first or last kept point's place; nor where they stay in)
				LandHulk::Yard LiveCrew = YL, LiveNoCrew = YL;
				std::vector<size_t> LiveCrewSeen, LiveNoCrewSeen;
				LandFeed::Recording LiveKill;
				LiveKill.Tracks.push_back(Way("tank", { At(0, H3.X + 40.0, H3.Y + 100.0), At(20, H3.X + 40.0, H3.Y - 100.0) }));
				LiveKill.Tracks[0].Fell = 10.0;
				LiveCrew.Reached(LiveKill, LiveCrewSeen, LandHulk::BailReach);
				LiveNoCrew.Reached(LiveKill, LiveNoCrewSeen);
				const bool bLiveCrews = !LiveKill.Tracks[0].bPinned && LiveCrew.Marked(2) && LiveCrew.DueOf(2) == 10.0 && !LiveNoCrew.Marked(2);
				// (a tank creeping north at 2 m/s past wreck 1, her way 1.5 m outside her reach of the westmost ring, then off
				// at 8 m/s 20 m on: left as drawn on her kept points' line; drawn rolling smoothly over 0.4 s, marked, her bell
				// reaching the faster piece, BlendLead 0.4 8 = 2.56 m more; read in two pieces, the first ending as she sets
				// off: left by the first, BlendLead 0.4 2 = 0.64 m more, and marked by the second, which reads her creep
				// again, at the same time as read whole)
				const double Passing = West - LandHulk::VehicleReach - 1.5;
				LandFeed::Recording Smooth;
				Smooth.Tracks.push_back(Way("tank", { At(0, Passing, H1.Y - 20.0), At(20, Passing, H1.Y + 20.0), At(20.5, Passing, H1.Y + 24.0), At(45, Passing, H1.Y + 220.0) }));
				LandHulk::Yard Plain = YL, Blended = YL;
				std::vector<size_t> PlainSeen, BlendedSeen;
				Plain.Reached(Smooth, PlainSeen);
				Blended.Reached(Smooth, BlendedSeen, 0.0, 0.4);
				LandFeed::Recording Smooth1 = Smooth;
				Smooth1.Tracks[0].At.resize(2);
				LandHulk::Yard BlendedPieces = YL;
				std::vector<size_t> BlendedPiecesSeen;
				BlendedPieces.Reached(Smooth1, BlendedPiecesSeen, 0.0, 0.4);
				const bool bFirstLeft = !BlendedPieces.Marked(0);
				BlendedPieces.Reached(Smooth, BlendedPiecesSeen, 0.0, 0.4);
				const bool bBlend = !Plain.Marked(0) && Blended.Marked(0) && Blended.DueOf(0) < 20.0 && bFirstLeft && BlendedPieces.Marked(0)
					&& std::fabs(BlendedPieces.DueOf(0) - Blended.DueOf(0)) < 1e-9;
				// (the eye as the camera is posed: the shot's pose looking 50 degrees west of wreck 1 on a 40 degree lens, the
				// player's drag turning the camera 50 degrees back onto her: in the camera's picture, not in the pose's)
				LandHulk::Sight Pose = On(H1);
				Pose.Fov = 40.0;
				const double Turn = 50.0 * 3.14159265358979323846 / 180.0;
				const double PoseX = H1.X - std::sin(Turn) * 120.0, PoseY = Pose.EyeY + std::cos(Turn) * 120.0;
				Pose.LookX = PoseX;
				Pose.LookY = PoseY;
				const double Wide = 2.0 * std::atan(std::tan(20.0 * 3.14159265358979323846 / 180.0) * Pose.Aspect) * 180.0 / 3.14159265358979323846;
				const LandHulk::Sight Camera = LandHulk::SightAlong(Pose.EyeX, Pose.EyeY, Pose.EyeZ, H1.X - Pose.EyeX, H1.Y - Pose.EyeY, H1.Z - Pose.EyeZ, Wide, Pose.Aspect, Pose.Lines, false);
				const bool bCamera = YL.InSight(0, Camera) && !YL.InSight(0, Pose) && std::fabs(Camera.Fov - 40.0) < 1e-6;
				const bool bOk = bTankShort && bTankThrough && bEyeOn && bAtTime && bAway && bOnCut && bSmall && bManPast && bManThrough && bPinFar && bPinned && bAgain && bHeading
					&& bHeadStays && bTurned && bHeadAway && bCrews && bLeave && bLiveCrews && bBlend && bCamera;
				std::printf("%s, a live battle, the guard made to work: %s (a tank's pieces to 30 m short of wreck 1's reach left her: %s; its next piece through her marked her at %.1f s: %s; the eye on her 5 s before kept her: %s, and cleared her %.1f s before: %s; the eye away 20 s before cleared her at once: %s, a cut: %s; 6 km off she is too small to see go: %s; a man %.1f m outside his reach of wreck 2 left her: %s, then through her marked her: %s; a lorry far off left wreck 3: %s, pinned on her marked her from her fall: %s; the men placed again, one on her, marked her no second time: %s; a tank coming on at wreck 1 60 m short of her reach: heading, not marked: %s, kept with the eye on her: %s, turned away no longer heading: %s, cleared with the eye away: %s; a tank knocked out 40 m off wreck 3 marked her where crews bail out, not where they stay in, nor for a lorry: %s; the same as a live battle knocks her out, no pin, her place at her fall between kept points: %s; a man leaving the field 5 m outside his reach of wreck 2 marked her as he left: %s; a tank creeping by 1.5 m outside her reach of wreck 1, then off fast, left her on her kept points' line and marked her rolled smoothly over 0.4 s, read whole or in two pieces: %s; a drag turning the camera 50 degrees from the shot's pose onto wreck 1 read from the camera as posed, not the pose: %s)\n",
					Name.c_str(), bOk ? "each marked when a piece came near her, cleared at the first moment the eye could not see her go or at her time" : "NOT as it should be",
					bTankShort ? "yes" : "NO", TankDue, bTankThrough ? "yes" : "NO", bEyeOn ? "yes" : "NO", LandHulk::ClearLead, bAtTime ? "yes" : "NO", bAway ? "yes" : "NO", bOnCut ? "yes" : "NO",
					bSmall ? "yes" : "NO", Past - H2.Y - Round2 - LandHulk::ManReach, bManPast ? "yes" : "NO", bManThrough ? "yes" : "NO", bPinFar ? "yes" : "NO", bPinned ? "yes" : "NO",
					bAgain ? "yes" : "NO", bHeading ? "yes" : "NO", bHeadStays ? "yes" : "NO", bTurned ? "yes" : "NO", bHeadAway ? "yes" : "NO", bCrews ? "yes" : "NO", bLiveCrews ? "yes" : "NO",
					bLeave ? "yes" : "NO", bBlend ? "yes" : "NO", bCamera ? "yes" : "NO");
				if (!bOk) Fault(Name + ": the live guard did not mark or clear the wrecks as it should (or cleared one too soon)");
			}

			// The field from above, all of it: the start areas, the ground between the fronts and its margin (tinted red),
			// the battle's ways as it was fought, where its units stood at its start, and the live battle's wrecks ringed
			// in their nation's colour and numbered (a red cross: cleared by the guard as the battle was fought); four close.
			{
				const double FieldW = Rec.Ground.W > 0.0 ? Rec.Ground.W : 3000.0, FieldH = Rec.Ground.H > 0.0 ? Rec.Ground.H : 2000.0;
				const int Size = 1400, Key = 600;
				const double Scale = Size / FieldW;
				const int MapH = static_cast<int>(std::ceil(FieldH * Scale));
				const int CanvasH = std::max(MapH, 1280);
				Image Map(Size + Key, CanvasH);
				Map.ClipW = Size;
				Map.ClipH = MapH;
				Map.Fill(Size, 0, Size + Key, CanvasH, 30, 30, 32);
				Map.Fill(0, MapH, Size, CanvasH, 30, 30, 32);
				const auto ToPx = [&](double X, double Y, double& Px, double& Py)
				{
					Px = X * Scale;
					Py = (FieldH - Y) * Scale;
				};
				for (int J = 0; J < MapH; ++J)
					for (int I = 0; I < Size; ++I)
					{
						const double X = (I + 0.5) / Scale, Y = FieldH - (J + 0.5) / Scale;
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
						if (LandHulk::FromCore(FL, X, Y) <= 0.0) Tint(200, 60, 50, 0.35);
						else if (LandHulk::FromLikely(FL, X, Y) <= 0.0) Tint(220, 150, 60, 0.25);
						Map.Put(I, J, 0.62 * R + 40.0, 0.62 * Gr + 40.0, 0.62 * B + 40.0);
					}
				for (const Line& L : F.Men)
					for (size_t K = 0; K + 1 < L.size(); K += 2)
					{
						double Ax, Ay, Bx, By;
						ToPx(L[K].first, L[K].second, Ax, Ay);
						ToPx(L[K + 1].first, L[K + 1].second, Bx, By);
						Map.Segment(Ax, Ay, Bx, By, 250, 250, 250, 0.08);
					}
				for (const Line& L : F.Vehicles)
					for (size_t K = 0; K + 1 < L.size(); ++K)
					{
						double Ax, Ay, Bx, By;
						ToPx(L[K].first, L[K].second, Ax, Ay);
						ToPx(L[K + 1].first, L[K + 1].second, Bx, By);
						Map.Segment(Ax, Ay, Bx, By, 20, 20, 20, 0.6);
					}
				// (the start areas as the header gives them, white; the ground between the fronts, yellow)
				for (const double* Z : { Rec.Ground.ZoneAtt, Rec.Ground.ZoneDef })
				{
					double Ax, Ay, Bx, By;
					ToPx(Z[0], Z[1], Ax, Ay);
					ToPx(Z[2], Z[3], Bx, By);
					Map.Segment(Ax, Ay, Bx, Ay, 255, 255, 255, 0.9);
					Map.Segment(Bx, Ay, Bx, By, 255, 255, 255, 0.9);
					Map.Segment(Bx, By, Ax, By, 255, 255, 255, 0.9);
					Map.Segment(Ax, By, Ax, Ay, 255, 255, 255, 0.9);
				}
				for (size_t K = 0; K < FL.Live.Advance.size(); ++K)
				{
					const auto& A0 = FL.Live.Advance[K];
					const auto& B0 = FL.Live.Advance[(K + 1) % FL.Live.Advance.size()];
					double Ax, Ay, Bx, By;
					ToPx(A0.first, A0.second, Ax, Ay);
					ToPx(B0.first, B0.second, Bx, By);
					Map.Segment(Ax, Ay, Bx, By, 250, 220, 60, 0.9);
				}
				// (where the rules draw each block up, as modelled: cyan)
				for (const std::vector<double>& Box : FL.Live.Stands)
				{
					double Ax, Ay, Bx, By;
					ToPx(Box[0], Box[1], Ax, Ay);
					ToPx(Box[2], Box[3], Bx, By);
					Map.Segment(Ax, Ay, Bx, Ay, 80, 220, 230, 0.9);
					Map.Segment(Bx, Ay, Bx, By, 80, 220, 230, 0.9);
					Map.Segment(Bx, By, Ax, By, 80, 220, 230, 0.9);
					Map.Segment(Ax, By, Ax, Ay, 80, 220, 230, 0.9);
				}
				// (where each unit stood at the battle's start: the attacker's blue-white, the defender's orange)
				for (const LandFeed::Track& T : Rec.Tracks)
				{
					const LandFeed::Block* B = Rec.BlockOf(T.Block);
					if (T.At.empty() || !B) continue;
					double Px, Py;
					ToPx(T.At[0].X, T.At[0].Y, Px, Py);
					if (B->Side == 0) Map.Blend(static_cast<int>(Px), static_cast<int>(Py), 170, 200, 255, 0.9);
					else Map.Blend(static_cast<int>(Px), static_cast<int>(Py), 255, 160, 60, 0.9);
				}
				std::string Labels;
				int Line0 = 16;
				Labels += std::to_string(Size + 14) + " " + std::to_string(Line0) + " " + Name + " as a live battle: 40 burnt out here a month ago, laid from its header alone\n";
				Line0 += 20;
				{
					const std::string WholeSaid = YL.Said();
					size_t From = 0;
					while (From < WholeSaid.size())
					{
						size_t To = std::min(WholeSaid.size(), From + 90);
						if (To < WholeSaid.size())
						{
							const size_t Space = WholeSaid.rfind(' ', To);
							if (Space != std::string::npos && Space > From) To = Space;
						}
						Labels += std::to_string(Size + 14) + " " + std::to_string(Line0) + " " + WholeSaid.substr(From, To - From) + "\n";
						Line0 += 18;
						From = To < WholeSaid.size() && WholeSaid[To] == ' ' ? To + 1 : To;
					}
				}
				Line0 += 8;
				const V3 Sun = V3(-0.45, 0.35, 0.82).Unit();
				for (size_t K = 0; K < YL.Hulks().size(); ++K)
				{
					const LandHulk::Hulk& H = YL.Hulks()[K];
					double R, Gr, B, Px, Py;
					NationColour(H.Nation, R, Gr, B);
					ToPx(H.X, H.Y, Px, Py);
					Map.Ring(Px, Py, 7.0, 1.4, R, Gr, B);
					Map.Fill(static_cast<int>(Px) - 1, static_cast<int>(Py) - 1, static_cast<int>(Px) + 2, static_cast<int>(Py) + 2, R, Gr, B);
					const bool bGone = Whole.Cleared(K);
					if (bGone)
					{
						Map.Segment(Px - 8, Py - 8, Px + 8, Py + 8, 255, 40, 40, 1.0);
						Map.Segment(Px - 8, Py + 8, Px + 8, Py - 8, 255, 40, 40, 1.0);
					}
					Labels += std::to_string(static_cast<int>(Px) + 8) + " " + std::to_string(static_cast<int>(Py) - 20) + " " + std::to_string(K + 1) + "\n";
					Labels += std::to_string(Size + 14) + " " + std::to_string(Line0) + " " + std::to_string(K + 1) + "  " + Describe(H) + ", group " + std::to_string(H.Group + 1)
						+ (bGone ? ", cleared by the guard" : "") + "\n";
					Line0 += 18;
				}
				Map.Fill(20, MapH - 30, 20 + static_cast<int>(500.0 * Scale), MapH - 26, 255, 255, 255);
				Labels += "20 " + std::to_string(MapH - 52) + " 500 m\n";
				Line0 += 10;
				const char* Keys[] = { "rings: GER blue, USA green, SOV red, GBR yellow; a red cross: marked and cleared by the guard as the battle was fought",
					"tinted red: the likely ground's core, kept clear (where its blocks draw up, the setup room, between the fronts, edges)",
					"tinted orange: the rest of it (start areas, what it falls back over): only old lines, junctions, the village's edge",
					"white boxes the start areas (the header's zones), cyan its blocks as the rules draw them up, yellow between the fronts",
					"dark lines the vehicles' ways, faint white the men's, as the battle was fought; dots where its units stood at its start" };
				for (const char* K : Keys)
				{
					Labels += std::to_string(Size + 14) + " " + std::to_string(Line0) + " " + K + "\n";
					Line0 += 18;
				}
				// (four close looks, from the south-east, under the key)
				const int Close = 270;
				const int CloseTop = std::max(Line0 + 10, CanvasH - 2 * Close - 10);
				for (size_t K = 0; K < std::min<size_t>(4, YL.Hulks().size()); ++K)
				{
					const LandHulk::Hulk& H = YL.Hulks()[K];
					const int X0p = Size + static_cast<int>(K % 2) * Close + 20, Y0p = CloseTop + static_cast<int>(K / 2) * Close;
					const V3 Anchor(H.X, H.Y, H.Z);
					const View V = View::Looking(V3(H.X, H.Y, H.Z + 0.8), 2.35, 0.52, 20.0, X0p, Y0p, X0p + Close - 6, Y0p + Close - 6);
					Draw(Map, V, GroundAround(G, H.X, H.Y, 14.0, 0.5, Anchor), Anchor, Sun);
					LandField::Mesh M;
					YL.Patch(K, M);
					Draw(Map, V, M, Anchor, Sun);
					YL.Dress(K, true, M);
					Draw(Map, V, M, Anchor, Sun);
					Labels += std::to_string(X0p + 4) + " " + std::to_string(Y0p + 4) + " " + std::to_string(K + 1) + "\n";
				}
				Map.Save(OutDir + "/placement_live_" + Name + ".ppm");
				std::ofstream(OutDir + "/placement_live_" + Name + ".txt") << Labels;
				std::printf("picture: %s/placement_live_%s.ppm (its words beside it, .txt)\n", OutDir.c_str(), Name.c_str());
			}
		}
	}
	std::printf("\n%s (%d faults)\n", Faults == 0 ? "old wrecks: all checks passed" : "old wrecks: FAILED", Faults);
	return Faults == 0 ? 0 : 1;
}
