// The ground under a raid as pictures (Source/TheaterCommand/Battle/AirLand), painted by the very painter the scene
// uses, then looked at from the bomber's height by a small software painter: the ground's three pictures with their
// smaller copies (as the scene's textures), sampled as the engine's trilinear filter does, under the scene's haze.
// Not the game's picture: Unreal's light, tone curve, clouds and their shadows are not drawn; the ground's colours are
// shown as painted (the art director judges the paint), the haze as AirBattle.cpp sets it.
//
//   Tools/air_show/air_land_picture <recording.json> <towns.json> <out folder> [north x] [north y] [latitude] [hour]
//   AIR_LAND_PICTURES=1 Tools/air_show/run.sh <recording.json>
//
// With a latitude and an hour (local sun time) the sun is the place's at the recording's month (AirLand::SunAt), else
// the scene's fixed one. AIR_LAND_HAZE=<k> thickens the haze k times and AIR_LAND_HAZE_START=<m> starts it m metres
// out (to try the far ground's lift before the scene takes it). AIR_LAND_FULL=1 also writes the whole near picture. AIR_LAND_DECK=<cover> also draws shot 1 under a mock deck of cloud at 1,800 m with that share of the sky
// covered, and its shadows on the ground. Each shot's measures (the art director's: luma, the grain as the mean spread
// in 8 by 8 pixel squares, the spread over the patch, by what the ground is; and the ground's luma by distance) go to
// <key>-land-measures.txt.
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
	constexpr double HazeDensity = 0.00045 / 1000.0 * 100.0, HazeFalloff = 0.006 / 1000.0 * 100.0, HazeMost = 0.8;
	double HazeScale = 1.0, HazeStart = 2000.0;
	double HazeOf(const V3& Eye, const V3& P)
	{
		const double D = (P - Eye).Length();
		if (D <= HazeStart) return 0.0;
		const V3 Way = (P - Eye) * (1.0 / D);
		const double Z0 = Eye.Z + Way.Z * HazeStart, Rise = P.Z - Z0, Long = D - HazeStart;
		const double X = HazeFalloff * Rise;
		const double Line = std::fabs(X) < 1e-6 ? 1.0 : (1.0 - std::exp2(-X)) / X;
		const double Depth = HazeScale * HazeDensity * std::exp2(-HazeFalloff * Z0) * Long * Line;
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
		// (AIR_LAND_W, AIR_LAND_H: the picture's size; the scene on Brian's Mac draws some 1080 by 670 and fills out the window)
		int W = std::getenv("AIR_LAND_W") ? std::atoi(std::getenv("AIR_LAND_W")) : 1600, H = std::getenv("AIR_LAND_H") ? std::atoi(std::getenv("AIR_LAND_H")) : 900;
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

	// A mock deck of cloud (value noise, nothing of the scene's own sky): its cover at a point, 0 to 1.
	double Hash01(int64_t X, int64_t Y)
	{
		uint64_t H = static_cast<uint64_t>(X) * 0x9E3779B97F4A7C15ull ^ (static_cast<uint64_t>(Y) + 0x632BE59BD9B4E019ull) * 0xBF58476D1CE4E5B9ull;
		H ^= H >> 31;
		H *= 0x94D049BB133111EBull;
		H ^= H >> 29;
		return static_cast<double>(H >> 11) * (1.0 / 9007199254740992.0);
	}
	double ValueNoise(double X, double Y)
	{
		const double FX = std::floor(X), FY = std::floor(Y);
		const int64_t IX = static_cast<int64_t>(FX), IY = static_cast<int64_t>(FY);
		double TX = X - FX, TY = Y - FY;
		TX = TX * TX * (3.0 - 2.0 * TX);
		TY = TY * TY * (3.0 - 2.0 * TY);
		const double A = Hash01(IX, IY), B = Hash01(IX + 1, IY), C = Hash01(IX, IY + 1), D = Hash01(IX + 1, IY + 1);
		return (A + (B - A) * TX) + ((C + (D - C) * TX) - (A + (B - A) * TX)) * TY - 0.5;
	}
	double DeckField(double X, double Y)
	{
		double Sum = 0.0, Amp = 1.0, F = 1.0 / 2600.0;
		for (int O = 0; O < 5; ++O)
		{
			Sum += Amp * ValueNoise(X * F + 17.3 * O, Y * F - 9.1 * O);
			Amp *= 0.5;
			F *= 2.0;
		}
		return Sum;
	}
	double DeckThreshold = 0.0;
	constexpr double DeckHigh = 1800.0;
	double DeckCover(double X, double Y)
	{
		const double V = DeckField(X, Y) - DeckThreshold;
		return std::clamp((V + 0.03) / 0.06, 0.0, 1.0);
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
	if (Argc >= 8)
	{
		AirLand::SunAt(std::atof(Argv[6]), P.Month, std::atof(Argv[7]), P.NorthX, P.NorthY, P.SunX, P.SunY, P.SunZ);
	}
	if (const char* K = std::getenv("AIR_LAND_HAZE"))
	{
		HazeScale = std::max(0.0, std::atof(K));
	}
	if (const char* K = std::getenv("AIR_LAND_HAZE_START"))
	{
		HazeStart = std::max(0.0, std::atof(K));
	}
	const double Deck = std::getenv("AIR_LAND_DECK") ? std::clamp(std::atof(std::getenv("AIR_LAND_DECK")), 0.0, 1.0) : 0.0;
	{
		const double Flat = std::hypot(P.SunX, P.SunY);
		std::printf("sun %.1f degrees up%s; haze x%.2f from %.0f m\n", std::atan2(P.SunZ, Flat) * 180.0 / 3.14159265358979, Argc >= 8 ? " (the place's)" : " (the scene's fixed)", HazeScale, HazeStart);
	}

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
		if (K == 0 && std::getenv("AIR_LAND_FULL"))
		{
			// (the whole near picture as painted, for a look anywhere along the way in)
			std::vector<double> Lin(static_cast<size_t>(F.W) * F.H * 3);
			for (size_t I = 0; I < static_cast<size_t>(F.W) * F.H; ++I)
				for (int C = 0; C < 3; ++C) Lin[I * 3 + C] = Linear(Rgba[I * 4 + C]);
			SavePpm(Dir + "/" + Rec.Key + "-land-near.ppm", F.W, F.H, Lin);
		}
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
		bool bDeck = false;
	};
	if (Deck > 0.0)
	{
		std::vector<double> Field;
		for (int K = 0; K < 40000; ++K) Field.push_back(DeckField(P.AimX + (Hash01(K, 1) - 0.5) * 80000.0, P.AimY + (Hash01(K, 2) - 0.5) * 80000.0));
		std::sort(Field.begin(), Field.end());
		DeckThreshold = Field[static_cast<size_t>((1.0 - Deck) * (Field.size() - 1))];
	}
	const double SunL = std::sqrt(P.SunX * P.SunX + P.SunY * P.SunY + P.SunZ * P.SunZ);
	const V3 Sun(P.SunX / SunL, P.SunY / SunL, P.SunZ / SunL);
	std::FILE* Measures = std::fopen((Dir + "/" + Rec.Key + "-land-measures.txt").c_str(), "w");
	static const char* Kinds[7] = { "sea", "fields", "woods", "river", "town", "old town", "village" };
	const double AX = P.AimX, AY = P.AimY, High = 7620.0;
	const Shot Shots[] = {
		{ "behind the raid at 25,000 ft", V3(AX - 1500.0, AY - 11000.0, High), V3(AX, AY + 1500.0, 0.0), 55.0 },
		{ "straight down on the aim from 25,000 ft", V3(AX, AY, High), V3(AX, AY + 0.01, 0.0), 45.0 },
		{ "over the town at 3 km", V3(AX - 3500.0, AY - 5500.0, 3000.0), V3(AX, AY, 0.0), 55.0 },
		{ "the low run at 900 m", V3(AX - 1200.0, AY - 3800.0, 900.0), V3(AX, AY + 600.0, 0.0), 60.0 },
		{ "the way in, 25 km out at 25,000 ft", V3(AX + 2000.0, AY - 34000.0, High), V3(AX, AY - 18000.0, 0.0), 60.0 },
		{ "behind the raid at 25,000 ft over the deck", V3(AX - 1500.0, AY - 11000.0, High), V3(AX, AY + 1500.0, 0.0), 55.0, true },
	};
	int N = 1;
	std::vector<Shot> Use(std::begin(Shots), std::end(Shots));
	// (AIR_LAND_CAM="eye east north up  look east north up  lens degrees", metres from the aim: one shot of one's own, in place of these)
	if (const char* C = std::getenv("AIR_LAND_CAM"))
	{
		double E[7] = {};
		std::sscanf(C, "%lf %lf %lf %lf %lf %lf %lf", &E[0], &E[1], &E[2], &E[3], &E[4], &E[5], &E[6]);
		Use.clear();
		Use.push_back(Shot{ "custom", V3(AX + E[0], AY + E[1], E[2]), V3(AX + E[3], AY + E[4], E[5]), E[6] });
	}
	// (the scene reads the ground through the anisotropic filter, 8 samples at most along a pixel's long side: build 1782's
	// soft Mac stills; AIR_LAND_ANISO=1 is the plain trilinear the scene had before)
	const int Aniso = std::getenv("AIR_LAND_ANISO") ? std::max(1, std::atoi(std::getenv("AIR_LAND_ANISO"))) : 8;
	for (const Shot& S : Use)
	{
		if (S.bDeck && Deck <= 0.0)
		{
			continue;
		}
		const int Number = S.bDeck ? 1 : N++;
		Camera Cam;
		Cam.Look(S.From, S.To, S.Fov);
		std::vector<double> Lin(static_cast<size_t>(Cam.W) * Cam.H * 3);
		const double Top[3] = { Linear(120), Linear(150), Linear(190) };
		std::vector<signed char> What(static_cast<size_t>(Cam.W) * Cam.H, -1);
		std::vector<float> Away(static_cast<size_t>(Cam.W) * Cam.H, -1.0f);
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
					// (the footprint: how far the next pixel's ray lands, along and across: the longer for plain trilinear, both for the anisotropic)
					const V3 DX = Cam.Ray(X + 1.5, Y + 0.5), DY = Cam.Ray(X + 0.5, Y + 1.5);
					double Foot = 0.0;
					V3 Ax[2];
					double Ln[2] = { 1e6, 1e6 };
					int AK = 0;
					for (const V3& Dn : { DX, DY })
					{
						if (Dn.Z < -1e-6)
						{
							const V3 H2 = Cam.Eye + Dn * (-Cam.Eye.Z / Dn.Z);
							Ax[AK] = H2 - Hit;
							Ln[AK] = Ax[AK].Length();
						}
						Foot = std::max(Foot, Ln[AK]);
						++AK;
					}
					double Col[3] = { Linear(40), Linear(62), Linear(76) };
					for (const Mipped& M : Pictures)
					{
						if (M.F.Holds(Hit.X, Hit.Y))
						{
							if (Aniso <= 1 || Foot >= 1e5)
							{
								M.Sample(Hit.X, Hit.Y, Foot, Col);
							}
							else
							{
								const int Mj = Ln[0] >= Ln[1] ? 0 : 1;
								const double Lmaj = Ln[Mj], Lmin = std::max(1e-6, Ln[1 - Mj]);
								const int Taps = std::clamp(static_cast<int>(std::ceil(Lmaj / Lmin)), 1, Aniso);
								const double Fp = std::max(Lmin, Lmaj / Aniso);
								for (int C = 0; C < 3; ++C) Col[C] = 0.0;
								for (int K = 0; K < Taps; ++K)
								{
									const V3 At = Hit + Ax[Mj] * ((K + 0.5) / Taps - 0.5);
									double One[3];
									M.Sample(At.X, At.Y, Fp, One);
									for (int C = 0; C < 3; ++C) Col[C] += One[C] / Taps;
								}
							}
							break;
						}
					}
					const size_t At = static_cast<size_t>(Y) * Cam.W + X;
					What[At] = static_cast<signed char>(Painter.What(Hit.X, Hit.Y));
					Away[At] = static_cast<float>(std::hypot(Hit.X - Cam.Eye.X, Hit.Y - Cam.Eye.Y));
					double Cloud = 0.0;
					V3 OnDeck;
					if (S.bDeck)
					{
						// (the deck's shadow on the ground, then the deck itself between the eye and the ground)
						const V3 Up = Hit + Sun * (DeckHigh / std::max(0.05, Sun.Z));
						const double Shade = 1.0 - 0.6 * DeckCover(Up.X, Up.Y);
						for (int C = 0; C < 3; ++C) Col[C] *= Shade;
						OnDeck = Cam.Eye + D * ((DeckHigh - Cam.Eye.Z) / D.Z);
						Cloud = DeckCover(OnDeck.X, OnDeck.Y);
						if (Cloud > 0.0)
						{
							What[At] = 7;
						}
					}
					const double F = HazeOf(Cam.Eye, Hit);
					for (int C = 0; C < 3; ++C) Out[C] = Col[C] * (1.0 - F) + HazeCol[C] * F;
					if (Cloud > 0.0)
					{
						const double Lumps = 0.84 + 0.16 * std::clamp(0.5 + 2.0 * DeckField(OnDeck.X * 3.0, OnDeck.Y * 3.0), 0.0, 1.0);
						const double Tops[3] = { 0.80 * Lumps, 0.81 * Lumps, 0.84 * Lumps };
						const double FD = HazeOf(Cam.Eye, OnDeck);
						for (int C = 0; C < 3; ++C) Out[C] = Out[C] * (1.0 - Cloud) + (Tops[C] * (1.0 - FD) + HazeCol[C] * FD) * Cloud;
					}
				}
		});
		const std::string Path = Dir + "/" + Rec.Key + "-land-" + std::to_string(Number) + (S.bDeck ? "-deck" : "") + ".ppm";
		SavePpm(Path, Cam.W, Cam.H, Lin);
		std::printf("  %s -> %s (%.2f s)\n", S.Name, Path.c_str(), Seconds(Start));

		// The measures, on the picture as written (8-bit sRGB): luma 0.299 R + 0.587 G + 0.114 B.
		std::vector<double> Luma(static_cast<size_t>(Cam.W) * Cam.H);
		for (size_t I = 0; I < Luma.size(); ++I)
		{
			Luma[I] = 0.299 * std::round(ToPicture(Lin[I * 3])) + 0.587 * std::round(ToPicture(Lin[I * 3 + 1])) + 0.114 * std::round(ToPicture(Lin[I * 3 + 2]));
		}
		std::fprintf(Measures, "%s (%s)\n  %-9s %7s %6s %6s %7s\n", Path.c_str(), S.Name, "ground", "share", "luma", "grain", "spread");
		for (int K = 0; K < 8; ++K)
		{
			double Sum = 0.0, Sum2 = 0.0, Grain = 0.0;
			long Count = 0, Squares = 0;
			for (size_t I = 0; I < Luma.size(); ++I)
			{
				if (What[I] == K)
				{
					Sum += Luma[I];
					Sum2 += Luma[I] * Luma[I];
					++Count;
				}
			}
			for (int Y0 = 0; Y0 + 8 <= Cam.H; Y0 += 8)
			{
				for (int X0 = 0; X0 + 8 <= Cam.W; X0 += 8)
				{
					double A = 0.0, A2 = 0.0;
					bool bAll = true;
					for (int DY = 0; DY < 8 && bAll; ++DY)
					{
						for (int DX = 0; DX < 8; ++DX)
						{
							const size_t I = static_cast<size_t>(Y0 + DY) * Cam.W + X0 + DX;
							if (What[I] != K)
							{
								bAll = false;
								break;
							}
							A += Luma[I];
							A2 += Luma[I] * Luma[I];
						}
					}
					if (bAll)
					{
						Grain += std::sqrt(std::max(0.0, A2 / 64.0 - (A / 64.0) * (A / 64.0)));
						++Squares;
					}
				}
			}
			if (Count > 200)
			{
				const double Mean = Sum / Count;
				std::fprintf(Measures, "  %-9s %6.1f%% %6.1f %6.2f %7.2f\n", K == 7 ? "cloud" : Kinds[K], 100.0 * Count / Luma.size(), Mean,
				             Squares ? Grain / Squares : 0.0, std::sqrt(std::max(0.0, Sum2 / Count - Mean * Mean)));
			}
		}
		std::fprintf(Measures, "  ground (not cloud, not sea) by distance from under the eye, km: luma\n   ");
		for (int Band = 0; Band < 8; ++Band)
		{
			double Sum = 0.0;
			long Count = 0;
			for (size_t I = 0; I < Luma.size(); ++I)
			{
				if (What[I] > 0 && What[I] < 7 && Away[I] >= Band * 5000.0 && Away[I] < (Band + 1) * 5000.0)
				{
					Sum += Luma[I];
					++Count;
				}
			}
			if (Count > 200)
			{
				std::fprintf(Measures, " %d-%d: %.1f", Band * 5, Band * 5 + 5, Sum / Count);
			}
		}
		std::fprintf(Measures, "\n");
	}
	std::fclose(Measures);
	return 0;
}
