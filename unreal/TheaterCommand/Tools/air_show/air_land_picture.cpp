// The ground under a raid as pictures (Source/TheaterCommand/Battle/AirLand), painted by the very painter the scene
// uses, then looked at from the bomber's height by a small software painter: the ground's three pictures with their
// smaller copies (as the scene's textures), sampled as the engine's trilinear filter does, under the scene's haze.
// Not the game's picture: Unreal's light, tone curve, clouds and their shadows are not drawn; the ground's colours are
// shown as painted (the art director judges the paint), the haze as AirBattle.cpp sets it.
//
//   Tools/air_show/air_land_picture <recording.json> <towns.json> <out folder> [north x] [north y]
//   AIR_LAND_PICTURES=1 Tools/air_show/run.sh <recording.json>
//
// Writes <key>-land-<n>.ppm: the eyes (behind the raid at 25,000 ft, straight down on the aim, over the town at
// 3 km, the low run over it at 900 m, the way in 25 km out), and the near picture's middle as painted (8 m a texel).
// True north on the board comes from the recording (place.ground.north); a recording from before it takes the two
// numbers after the folder (Bremen's board: -0.730 -0.684).

#include "Battle/AirFeed.h"
#include "Battle/AirLand.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
	struct V3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		V3() = default;
		V3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
		V3 operator+(const V3& O) const { return { X + O.X, Y + O.Y, Z + O.Z }; }
		V3 operator-(const V3& O) const { return { X - O.X, Y - O.Y, Z - O.Z }; }
		V3 operator*(double S) const { return { X * S, Y * S, Z * S }; }
		double Dot(const V3& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		V3 Cross(const V3& O) const { return { Y * O.Z - Z * O.Y, Z * O.X - X * O.Z, X * O.Y - Y * O.X }; }
		double Length() const { return std::sqrt(Dot(*this)); }
		V3 Unit(const V3& Else = V3(1.0, 0.0, 0.0)) const
		{
			const double L = Length();
			return L > 1e-9 ? *this * (1.0 / L) : Else;
		}
	};

	double Linear(double C255)
	{
		const double C = C255 / 255.0;
		return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
	}
	double ToPicture(double L)
	{
		L = std::max(0.0, std::min(1.0, L));
		return 255.0 * (L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(L, 1.0 / 2.4) - 0.055);
	}

	// The haze as the scene sets Unreal's height fog (as air_sky_picture.cpp: density 0.00045, falloff 0.006, from 2 km,
	// at most 0.8, toward 0.55, 0.66, 0.78).
	const double HazeCol[3] = { 0.55, 0.66, 0.78 };
	constexpr double HazeDensity = 0.00045 / 1000.0 * 100.0, HazeFalloff = 0.006 / 1000.0 * 100.0, HazeStart = 2000.0, HazeMost = 0.8;
	double HazeOf(const V3& Eye, const V3& P)
	{
		const double D = (P - Eye).Length();
		if (D <= HazeStart) return 0.0;
		const V3 Way = (P - Eye) * (1.0 / D);
		const double Z0 = Eye.Z + Way.Z * HazeStart, Rise = P.Z - Z0, Long = D - HazeStart;
		const double X = HazeFalloff * Rise;
		const double Line = std::fabs(X) < 1e-6 ? 1.0 : (1.0 - std::exp2(-X)) / X;
		const double Depth = HazeDensity * std::exp2(-HazeFalloff * Z0) * Long * Line;
		return std::min(HazeMost, 1.0 - std::exp2(-Depth));
	}

	/** A picture and its smaller copies, linear colour, each halving a plain box mean (as the scene's Halve). */
	struct Mipped
	{
		AirLand::Frame F;
		std::vector<int> W, H;
		std::vector<std::vector<float>> Px; // linear red, green, blue

		void Make(const AirLand::Frame& InF, const std::vector<uint8_t>& Rgba)
		{
			F = InF;
			std::vector<float> L(static_cast<size_t>(F.W) * F.H * 3);
			for (size_t I = 0; I < static_cast<size_t>(F.W) * F.H; ++I)
				for (int C = 0; C < 3; ++C) L[I * 3 + C] = static_cast<float>(Linear(Rgba[I * 4 + C]));
			int CW = F.W, CH = F.H;
			W.push_back(CW);
			H.push_back(CH);
			Px.push_back(L);
			while (CW > 1 || CH > 1)
			{
				const int NW = std::max(1, CW / 2), NH = std::max(1, CH / 2);
				std::vector<float> S(static_cast<size_t>(NW) * NH * 3);
				const std::vector<float>& B = Px.back();
				for (int J = 0; J < NH; ++J)
					for (int I = 0; I < NW; ++I)
						for (int C = 0; C < 3; ++C)
						{
							double Sum = 0.0;
							int N = 0;
							for (int DJ = 0; DJ < 2; ++DJ)
								for (int DI = 0; DI < 2; ++DI)
								{
									const int X = std::min(CW - 1, I * 2 + DI), Y = std::min(CH - 1, J * 2 + DJ);
									Sum += B[(static_cast<size_t>(Y) * CW + X) * 3 + C];
									++N;
								}
							S[(static_cast<size_t>(J) * NW + I) * 3 + C] = static_cast<float>(Sum / N);
						}
				CW = NW;
				CH = NH;
				W.push_back(CW);
				H.push_back(CH);
				Px.push_back(std::move(S));
			}
		}

		void Bilinear(int Level, double U, double V, double Out[3]) const
		{
			const int LW = W[Level], LH = H[Level];
			const double X = U * LW - 0.5, Y = V * LH - 0.5;
			const int X0 = static_cast<int>(std::floor(X)), Y0 = static_cast<int>(std::floor(Y));
			const double FX = X - X0, FY = Y - Y0;
			const std::vector<float>& P = Px[Level];
			for (int C = 0; C < 3; ++C) Out[C] = 0.0;
			for (int DJ = 0; DJ < 2; ++DJ)
				for (int DI = 0; DI < 2; ++DI)
				{
					const int XX = std::clamp(X0 + DI, 0, LW - 1), YY = std::clamp(Y0 + DJ, 0, LH - 1);
					const double Wt = (DI ? FX : 1.0 - FX) * (DJ ? FY : 1.0 - FY);
					for (int C = 0; C < 3; ++C) Out[C] += Wt * P[(static_cast<size_t>(YY) * LW + XX) * 3 + C];
				}
		}

		/** Trilinear: the level whose texel matches the footprint's longer side (metres), between two levels. */
		void Sample(double X, double Y, double Footprint, double Out[3]) const
		{
			const double U = (X - F.X0) / (F.W * F.Step), V = (F.Y1() - Y) / (F.H * F.Step);
			const double Lod = std::clamp(std::log2(std::max(1e-6, Footprint / F.Step)), 0.0, static_cast<double>(W.size() - 1));
			const int L0 = static_cast<int>(std::floor(Lod)), L1 = std::min(static_cast<int>(W.size()) - 1, L0 + 1);
			const double T = Lod - L0;
			double A[3], B[3];
			Bilinear(L0, U, V, A);
			Bilinear(L1, U, V, B);
			for (int C = 0; C < 3; ++C) Out[C] = A[C] + (B[C] - A[C]) * T;
		}
	};

	struct Camera
	{
		V3 Eye, Right, Up, Forward;
		double Ppr = 1000.0;
		int W = 1600, H = 900;
		void Look(const V3& From, const V3& To, double FovDegrees)
		{
			Eye = From;
			Forward = (To - From).Unit();
			Right = Forward.Cross(V3(0.0, 0.0, 1.0)).Unit(V3(1.0, 0.0, 0.0));
			Up = Right.Cross(Forward);
			Ppr = W / (2.0 * std::tan(FovDegrees * 3.14159265358979 / 360.0));
		}
		V3 Ray(double Sx, double Sy) const { return (Forward + Right * ((Sx - W * 0.5) / Ppr) + Up * ((H * 0.5 - Sy) / Ppr)).Unit(); }
	};

	void SavePpm(const std::string& Path, int W, int H, const std::vector<double>& Lin)
	{
		std::ofstream F(Path, std::ios::binary);
		F << "P6\n" << W << " " << H << "\n255\n";
		for (double V : Lin)
		{
			const unsigned char B = static_cast<unsigned char>(ToPicture(V) + 0.5);
			F.write(reinterpret_cast<const char*>(&B), 1);
		}
	}

	template <typename Fn>
	void InBands(int Rows, Fn Body)
	{
		const int N = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
		std::vector<std::thread> Pool;
		for (int T = 0; T < N; ++T)
		{
			const int A = Rows * T / N, B = Rows * (T + 1) / N;
			Pool.emplace_back([A, B, &Body]() { Body(A, B); });
		}
		for (std::thread& Th : Pool) Th.join();
	}

	double Seconds(std::chrono::steady_clock::time_point Since)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - Since).count();
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: air_land_picture <recording.json> <towns.json> <out folder> [north x] [north y]\n");
		return 2;
	}
	auto ReadAll = [](const char* Path)
	{
		std::ifstream In(Path, std::ios::binary);
		std::stringstream Text;
		Text << In.rdbuf();
		return Text.str();
	};
	AirFeed::Recording Rec;
	std::string Error;
	if (!AirFeed::ReadRecording(ReadAll(Argv[1]), Rec, Error))
	{
		std::fprintf(stderr, "%s: %s\n", Argv[1], Error.c_str());
		return 2;
	}
	const std::string Plans = ReadAll(Argv[2]);
	const std::string Dir = Argv[3];
	const AirFeed::Sky& B = Rec.Board;

	AirLand::Place P;
	P.AimX = B.TargetX;
	P.AimY = B.TargetY;
	P.NorthX = B.NorthX;
	P.NorthY = B.NorthY;
	if (!B.bNorthGiven && Argc >= 6)
	{
		P.NorthX = std::atof(Argv[4]);
		P.NorthY = std::atof(Argv[5]);
	}
	P.Terrain = B.Terrain.empty() ? "plains" : B.Terrain;
	P.Name = B.PlaceName;
	P.Month = B.Month + 1; // (the feed's months count from 0)
	P.Seed = Rec.Seed;
	P.Land = B.Land;
	P.Rivers = B.Rivers;

	auto Start = std::chrono::steady_clock::now();
	const AirLand::Painter Painter(P, Plans);
	const double Built = Seconds(Start);
	const AirLand::Tally& Laid = Painter.Laid();
	std::printf("%s: %s, plan %s, %d villages, %d country roads, %d bridges, %d spires, %d line pieces, %s; north (%.3f, %.3f)%s; laid in %.2f s\n",
	            Rec.Key.c_str(), P.Name.c_str(), Laid.Plan.c_str(), Laid.Villages, Laid.Links, Laid.Bridges, Laid.Spires, Laid.Segments,
	            Laid.bCoast ? "coast" : "no sea", P.NorthX, P.NorthY, B.bNorthGiven ? " (the recording's)" : " (given)", Built);

	const std::vector<AirLand::Frame> Frames = AirLand::Frames(P);
	std::vector<Mipped> Pictures(Frames.size());
	std::vector<uint8_t> NearRgba;
	for (size_t K = 0; K < Frames.size(); ++K)
	{
		const AirLand::Frame& F = Frames[K];
		std::vector<uint8_t> Rgba(static_cast<size_t>(F.W) * F.H * 4);
		std::vector<float> Heights(static_cast<size_t>(F.W) * F.H);
		Start = std::chrono::steady_clock::now();
		InBands(F.H, [&](int A, int Bb) { Painter.PaintRows(F, A, Bb, Rgba.data(), Heights.data()); });
		const double Painted = Seconds(Start);
		Start = std::chrono::steady_clock::now();
		if (AirLand::Painter::Shades(F))
		{
			InBands(F.H, [&](int A, int Bb) { Painter.ShadeRows(F, A, Bb, Rgba.data(), Heights.data()); });
		}
		std::printf("  picture %zu: %d x %d at %.0f m a texel, painted in %.2f s, shaded in %.2f s (%u threads)\n", K, F.W, F.H, F.Step, Painted,
		            Seconds(Start), std::thread::hardware_concurrency());
		Pictures[K].Make(F, Rgba);
		if (K == 0) NearRgba = Rgba;
	}

	// The near picture's middle as painted: 1600 x 1200 texels round the aim (12.8 by 9.6 km).
	{
		const AirLand::Frame& F = Frames[0];
		const int W = 1600, H = 1200;
		const int CX = static_cast<int>((P.AimX - F.X0) / F.Step), CY = static_cast<int>((F.Y1() - P.AimY) / F.Step);
		std::vector<double> Lin(static_cast<size_t>(W) * H * 3);
		for (int J = 0; J < H; ++J)
			for (int I = 0; I < W; ++I)
			{
				const int X = std::clamp(CX - W / 2 + I, 0, F.W - 1), Y = std::clamp(CY - H / 2 + J, 0, F.H - 1);
				for (int C = 0; C < 3; ++C) Lin[(static_cast<size_t>(J) * W + I) * 3 + C] = Linear(NearRgba[(static_cast<size_t>(Y) * F.W + X) * 4 + C]);
			}
		SavePpm(Dir + "/" + Rec.Key + "-land-0.ppm", W, H, Lin);
	}

	struct Shot
	{
		const char* Name;
		V3 From, To;
		double Fov;
	};
	const double AX = P.AimX, AY = P.AimY, High = 7620.0;
	const Shot Shots[] = {
		{ "behind the raid at 25,000 ft", V3(AX - 1500.0, AY - 11000.0, High), V3(AX, AY + 1500.0, 0.0), 55.0 },
		{ "straight down on the aim from 25,000 ft", V3(AX, AY, High), V3(AX, AY + 0.01, 0.0), 45.0 },
		{ "over the town at 3 km", V3(AX - 3500.0, AY - 5500.0, 3000.0), V3(AX, AY, 0.0), 55.0 },
		{ "the low run at 900 m", V3(AX - 1200.0, AY - 3800.0, 900.0), V3(AX, AY + 600.0, 0.0), 60.0 },
		{ "the way in, 25 km out at 25,000 ft", V3(AX + 2000.0, AY - 34000.0, High), V3(AX, AY - 18000.0, 0.0), 60.0 },
	};
	int N = 1;
	for (const Shot& S : Shots)
	{
		Camera Cam;
		Cam.Look(S.From, S.To, S.Fov);
		std::vector<double> Lin(static_cast<size_t>(Cam.W) * Cam.H * 3);
		const double Top[3] = { Linear(120), Linear(150), Linear(190) };
		Start = std::chrono::steady_clock::now();
		InBands(Cam.H, [&](int A, int Bb) {
			for (int Y = A; Y < Bb; ++Y)
				for (int X = 0; X < Cam.W; ++X)
				{
					double* Out = &Lin[(static_cast<size_t>(Y) * Cam.W + X) * 3];
					const V3 D = Cam.Ray(X + 0.5, Y + 0.5);
					if (D.Z > -1e-5)
					{
						const double Up = std::min(1.0, D.Z * 3.0);
						for (int C = 0; C < 3; ++C) Out[C] = HazeCol[C] + (Top[C] - HazeCol[C]) * Up;
						continue;
					}
					const V3 Hit = Cam.Eye + D * (-Cam.Eye.Z / D.Z);
					// (the footprint: how far the next pixel's ray lands, along and across, the longer of the two)
					const V3 DX = Cam.Ray(X + 1.5, Y + 0.5), DY = Cam.Ray(X + 0.5, Y + 1.5);
					double Foot = 0.0;
					for (const V3& Dn : { DX, DY })
					{
						if (Dn.Z < -1e-6)
						{
							const V3 H2 = Cam.Eye + Dn * (-Cam.Eye.Z / Dn.Z);
							Foot = std::max(Foot, (H2 - Hit).Length());
						}
						else
						{
							Foot = 1e6;
						}
					}
					double Col[3] = { Linear(40), Linear(62), Linear(76) };
					for (const Mipped& M : Pictures)
					{
						if (M.F.Holds(Hit.X, Hit.Y))
						{
							M.Sample(Hit.X, Hit.Y, Foot, Col);
							break;
						}
					}
					const double F = HazeOf(Cam.Eye, Hit);
					for (int C = 0; C < 3; ++C) Out[C] = Col[C] * (1.0 - F) + HazeCol[C] * F;
				}
		});
		const std::string Path = Dir + "/" + Rec.Key + "-land-" + std::to_string(N++) + ".ppm";
		SavePpm(Path, Cam.W, Cam.H, Lin);
		std::printf("  %s -> %s (%.2f s)\n", S.Name, Path.c_str(), Seconds(Start));
	}
	return 0;
}
