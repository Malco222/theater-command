// Old wrecks' cloud test (release 2, -TheaterLandOldWrecks; Brian, 23 November 20:09Z: the burnt-out wrecks of a battle
// stay on the field for the next battle there). Lays them on each recording's field as LandBattle's LayOldWrecks does
// (LandHulk::FieldOf, the battle's own models from LandKit::Battle) and checks what the brief asks: the same twice;
// none on a lane, a bank, a house, a wall, a yard or garden, a wood, water, marsh or standing corn, every point of each
// read where it lies; 8 m and more from every recorded vehicle's way, 1.5 m from every man's; none on another; no more
// than the cap; fewer as the months go; each nation's share as the recording's nats give it; the recording's key read
// whatever is missing or odd in it; with no key and no switch, none at all. Then on an old field (two battles' scars
// and old ruts laid first, as the scene lays them): none on an old shell hole or an old rut. The dressed meshes: every
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
		/** The box lines and rings are drawn in (the map's, not its key's). */
		int ClipW = 0, ClipH = 0;
		Image(int InW, int InH) : W(InW), H(InH), Rgb(static_cast<size_t>(InW) * static_cast<size_t>(InH) * 3, 0), Depth(static_cast<size_t>(InW) * static_cast<size_t>(InH), 1e30f), ClipW(InW), ClipH(InH) {}
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

	/** A mesh drawn in a view (its points At metres from Origin), lit by Sun, each triangle's corners' colours mixed. */
	void Draw(Image& Img, const View& V, const LandField::Mesh& M, const V3& Origin, const V3& Sun)
	{
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

	/** Whether a colour reads as rust: orange to deep brown (its hue 5 to 40 degrees, well saturated, not black). */
	bool Rusty(const LandField::Rgb8& C)
	{
		const double R = C.R, G = C.G, B = C.B, Hi = std::max({ R, G, B }), Lo = std::min({ R, G, B });
		if (Hi < 45.0 || Hi - Lo < 0.35 * Hi || Hi != R) return false;
		const double Hue = 60.0 * (G - B) / (Hi - Lo);
		return Hue >= 5.0 && Hue <= 40.0;
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
		};
		const Case Cases[] = {
			{ "", 0, 0.0, "" },
			{ "\"oldWrecks\": {\"count\": 12, \"months\": 7.5, \"nats\": {\"GER\": 8, \"SOV\": 4}}, ", 12, 7.5, "GER 8,SOV 4," },
			{ "\"oldWrecks\": {\"count\": 200, \"months\": 2}, ", 60, 2.0, "" },
			{ "\"oldWrecks\": {\"count\": -3, \"months\": -2}, ", 0, 0.0, "" },
			{ "\"oldWrecks\": {\"months\": 5}, ", 0, 5.0, "" },
			{ "\"oldWrecks\": {\"count\": \"12\", \"months\": \"x\", \"nats\": [1, 2]}, ", 0, 0.0, "" },
			{ "\"oldWrecks\": {\"count\": 9, \"nats\": {\"GER\": 0, \"USA\": -1, \"\": 4, \"SOV\": 2.6, \"ITA\": \"3\", \"GBR\": null}}, ", 9, 0.0, "SOV 3," },
			{ "\"oldWrecks\": 5, ", 0, 0.0, "" },
			{ "\"oldWrecks\": null, ", 0, 0.0, "" },
			{ "\"oldWrecks\": {}, ", 0, 0.0, "" },
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
			if (R.OldWreckCount != C.Count || std::fabs(R.OldWreckMonths - C.Months) > 1e-9 || Nats != C.Nats)
			{
				Fault(std::string("oldWrecks read as ") + std::to_string(R.OldWreckCount) + ", " + std::to_string(R.OldWreckMonths) + ", [" + Nats + "] from " + C.Key);
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
		std::printf("LandFeed: the oldWrecks key read %d ways (absent, whole, over the cap, below nothing, a part missing, wrong kinds, odd nations, not an object, null, empty)\n", Read);
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
					Y.Dress(K, true, M);
					for (const LandField::Rgb8& C : M.Colours)
					{
						++All;
						if (Rusty(C))
						{
							++Rust;
							Bright += C.R + C.G + C.B;
						}
					}
					if (I > 0)
					{
						const LandHulk::Hulk &P = Aged[0].Hulks()[std::min(K, Aged[0].Hulks().size() - 1)], &Same = Y.Hulks()[K];
						bSameLay = bSameLay && Aged[0].Hulks().size() == Y.Hulks().size() && P.X == Same.X && P.Y == Same.Y && P.Model == Same.Model;
					}
				}
				Share[I] = All > 0 ? static_cast<double>(Rust) / static_cast<double>(All) : 0.0;
				Dark[I] = Rust > 0 ? Bright / (3.0 * static_cast<double>(Rust)) : 0.0;
			}
			std::printf("rust as the months go (the same twenty): %.0f%% a week on, %.0f%% at a month, %.0f%% at six, %.0f%% at a year, %.0f%% at a year and a half; its brightness %.0f, %.0f, %.0f, %.0f, %.0f (orange browning)\n",
				100 * Share[0], 100 * Share[1], 100 * Share[2], 100 * Share[3], 100 * Share[4], Dark[0], Dark[1], Dark[2], Dark[3], Dark[4]);
			if (!bSameLay) Fault(Name + ": the same count laid otherwise at another age");
			if (!(Share[1] > Share[0] && Share[2] > Share[1] && Share[4] >= Share[2] - 0.02)) Fault(Name + ": the rust not spreading as the months go");

			// The look sheet: four of them (a thrown turret, a run-off track, a soft-skinned one, another) at each age,
			// from the south-east, 30 degrees up, on their own ground and patch.
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
					Y.Patch(Pick[R], M);
					Draw(Sheet, V, M, Anchor, Sun);
					Y.Dress(Pick[R], true, M);
					Draw(Sheet, V, M, Anchor, Sun);
					for (int E = 0; E < CellW; ++E) Sheet.Put(X0 + E, Y0, 34, 34, 36);
					for (int E = 0; E < CellH; ++E) Sheet.Put(X0, Y0 + E, 34, 34, 36);
					if (R == 0)
					{
						const char* Age = I == 0 ? "a week on" : I == 1 ? "a month on" : I == 2 ? "six months" : I == 3 ? "a year" : "a year and a half";
						Labels += std::to_string(X0 + 8) + " 8 " + Age + "\n";
					}
					if (I == 0) Labels += std::to_string(X0 + 6) + " " + std::to_string(Y0 + CellH - 20) + " " + Describe(Aged[0].Hulks()[Pick[R]]).substr(0, 60) + "\n";
				}
			}
			Sheet.Save(OutDir + "/look_" + Name + ".ppm");
			std::ofstream(OutDir + "/look_" + Name + ".txt") << Labels;
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

		// 5. An old field: two battles' scars and two of old ruts laid first, as the scene lays them; none on a hole or
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
			std::printf("pictures: %s/placement_%s.ppm, %s/look_%s.ppm (their words beside them, .txt)\n", OutDir.c_str(), Name.c_str(), OutDir.c_str(), Name.c_str());
		}
	}
	std::printf("\n%s (%d faults)\n", Faults == 0 ? "old wrecks: all checks passed" : "old wrecks: FAILED", Faults);
	return Faults == 0 ? 0 : 1;
}
