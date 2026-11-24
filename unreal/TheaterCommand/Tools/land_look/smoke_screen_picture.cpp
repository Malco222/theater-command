// Paints the smoke screen (TheaterFx::Battle::Screen, the effects kit's own code, no engine) as the kit lays it, from a
// man's eye 120 m off across the wind, at a few moments after it lands: a phosphorus round's, a burning bomb's and a tank
// commander's smoke grenade's (Kind "grenade", its plume down the wind), so
// its look can be judged before a Mac build. A plain software painter: each puff the kit lays is its middle and two
// rings of points, filled as the game's mesh fills them and laid over a plain sky and field, far to near; the field
// hides what is under it, softly over its last 0.25 m, as the game's depth fade does (a piece lying on it never).
//
// The grenade also gets, at 30 s, a frame from 40 m at a man's 1.5 m eye (a wider lens, the canister and the first of its
// plume) and one from the game's camera 25 m up (its shadow on the ground), and at every moment a measure printed along
// its plume: how solid it is (the most opaque pixel of the plume's column at each distance down the wind, painted over
// black and over white, so its opacity is exact), and the smoke's own colour where it is solid, its top and its underside.
//
//     g++ -std=c++20 -O2 -I Source/TheaterCommand Tools/land_look/smoke_screen_picture.cpp
//         Source/TheaterCommand/Effects/TheaterFxCore.cpp Source/TheaterCommand/Battle/LandJson.cpp -o /tmp/smoke_screen
//     /tmp/smoke_screen Content/Effects/battle-fx.json <out folder>        (PPM pictures; convert makes them PNG)
// (24 November: the horizon was drawn a little high in the pictures before, 14 px above the middle, not 14 below)
#include "Battle/LandJson.h"
#include "Effects/TheaterFxCore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace
{
	using TheaterFx::Vec;

	class JsonRecipe : public TheaterFx::Recipe
	{
	public:
		std::map<std::string, double> Numbers;
		std::map<std::string, std::string> Words;
		void Walk(const LandJson::Json& J, const std::string& Path)
		{
			using T = LandJson::Json::Type;
			if (J.Kind == T::Object)
			{
				for (size_t I = 0; I < J.Keys.size(); ++I) Walk(J.Values[I], Path.empty() ? J.Keys[I] : Path + "." + J.Keys[I]);
			}
			else if (J.Kind == T::Array)
			{
				for (size_t I = 0; I < J.Items.size(); ++I) Walk(J.Items[I], Path + "." + std::to_string(I));
			}
			else if (J.Kind == T::Number || J.Kind == T::Bool)
			{
				Numbers[Path] = J.Num();
			}
			else if (J.Kind == T::String)
			{
				Words[Path] = J.Text;
			}
		}
		bool Find(const char* Path, double& Out) const override
		{
			const auto It = Numbers.find(Path);
			if (It == Numbers.end()) return false;
			Out = It->second;
			return true;
		}
		bool FindWord(const char* Path, std::string& Out) const override
		{
			const auto It = Words.find(Path);
			if (It == Words.end()) return false;
			Out = It->second;
			return true;
		}
	};

	double Flat(void*, double, double) { return 0.0; }

	double Srgb01(double V)
	{
		V = std::clamp(V, 0.0, 1.0);
		return V <= 0.0031308 ? 12.92 * V : 1.055 * std::pow(V, 1.0 / 2.4) - 0.055;
	}

	struct Picture
	{
		int W = 960, H = 540;
		std::vector<double> R, G, B;
		Picture() : R(W * H), G(W * H), B(W * H) {}
		void Backdrop(double Horizon)
		{
			for (int Y = 0; Y < H; ++Y)
				for (int X = 0; X < W; ++X)
				{
					const size_t I = static_cast<size_t>(Y * W + X);
					if (Y < Horizon)
					{
						const double T = Y / std::max(1.0, Horizon);
						R[I] = 0.32 + 0.25 * T; G[I] = 0.45 + 0.22 * T; B[I] = 0.70 + 0.12 * T; // (a pale blue sky, lighter low)
					}
					else
					{
						const double T = (Y - Horizon) / std::max(1.0, H - Horizon);
						R[I] = 0.09 + 0.03 * T; G[I] = 0.13 + 0.04 * T; B[I] = 0.05 + 0.01 * T; // (a summer field, linear)
					}
				}
		}
		void Fill(double V)
		{
			std::fill(R.begin(), R.end(), V);
			std::fill(G.begin(), G.end(), V);
			std::fill(B.begin(), B.end(), V);
		}
		void Tri(const double* P0, const double* P1, const double* P2, const double* C0, const double* C1, const double* C2, bool bCut)
		{
			const double MinX = std::max(0.0, std::floor(std::min({ P0[0], P1[0], P2[0] })));
			const double MaxX = std::min(W - 1.0, std::ceil(std::max({ P0[0], P1[0], P2[0] })));
			const double MinY = std::max(0.0, std::floor(std::min({ P0[1], P1[1], P2[1] })));
			const double MaxY = std::min(H - 1.0, std::ceil(std::max({ P0[1], P1[1], P2[1] })));
			const double Area = (P1[0] - P0[0]) * (P2[1] - P0[1]) - (P2[0] - P0[0]) * (P1[1] - P0[1]);
			if (std::fabs(Area) < 1e-9) return;
			for (int Y = static_cast<int>(MinY); Y <= static_cast<int>(MaxY); ++Y)
				for (int X = static_cast<int>(MinX); X <= static_cast<int>(MaxX); ++X)
				{
					const double Px = X + 0.5, Py = Y + 0.5;
					const double A = ((P1[0] - Px) * (P2[1] - Py) - (P2[0] - Px) * (P1[1] - Py)) / Area;
					const double Bw = ((P2[0] - Px) * (P0[1] - Py) - (P0[0] - Px) * (P2[1] - Py)) / Area;
					const double Cw = 1.0 - A - Bw;
					if (A < 0.0 || Bw < 0.0 || Cw < 0.0) continue;
					double C[5];
					for (int K = 0; K < 5; ++K) C[K] = A * C0[K] + Bw * C1[K] + Cw * C2[K];
					if (bCut) C[3] *= std::clamp(C[4] / 0.25, 0.0, 1.0); // (the ground hides what is under it, softly, as the game's depth fade does)
					const size_t I = static_cast<size_t>(Y * W + X);
					R[I] = C[0] * C[3] + R[I] * (1.0 - C[3]);
					G[I] = C[1] * C[3] + G[I] * (1.0 - C[3]);
					B[I] = C[2] * C[3] + B[I] * (1.0 - C[3]);
				}
		}
		bool Save(const std::string& Path) const
		{
			FILE* F = std::fopen(Path.c_str(), "wb");
			if (!F) return false;
			std::fprintf(F, "P6\n%d %d\n255\n", W, H);
			for (size_t I = 0; I < R.size(); ++I)
			{
				const unsigned char Px[3] = { static_cast<unsigned char>(std::lround(Srgb01(R[I]) * 255.0)),
					static_cast<unsigned char>(std::lround(Srgb01(G[I]) * 255.0)), static_cast<unsigned char>(std::lround(Srgb01(B[I]) * 255.0)) };
				std::fwrite(Px, 1, 3, F);
			}
			std::fclose(F);
			return true;
		}
	};

	// (an eye at From looking at To, HalfDeg half its view across)
	TheaterFx::Battle::View EyeAt(const Vec& From, const Vec& To, double HalfDeg, const Picture& P)
	{
		TheaterFx::Battle::View Eye;
		Eye.Eye = From;
		Eye.Forward = (To - From).Unit();
		Eye.Right = Eye.Forward.Cross(Vec(0.0, 0.0, 1.0)).Unit();
		Eye.Up = Eye.Right.Cross(Eye.Forward).Unit();
		const double HalfView = HalfDeg * 3.14159265358979 / 180.0;
		Eye.PixelsPerRadian = P.W / (2.0 * std::tan(HalfView));
		Eye.HalfAcross = std::tan(HalfView);
		Eye.HalfUp = Eye.HalfAcross * P.H / P.W;
		Eye.Sun = Vec(-0.4, -0.5, 0.75).Unit();
		return Eye;
	}

	bool OnScreen(const TheaterFx::Battle::View& Eye, const Picture& P, const Vec& W, double* S)
	{
		const Vec D = W - Eye.Eye;
		const double Z = D.Dot(Eye.Forward);
		if (Z < 0.1) return false;
		S[0] = P.W * 0.5 + Eye.PixelsPerRadian * D.Dot(Eye.Right) / Z;
		S[1] = P.H * 0.5 - Eye.PixelsPerRadian * D.Dot(Eye.Up) / Z;
		return true;
	}

	// (the puffs the kit laid for Eye, painted far to near over what P holds)
	void Paint(const TheaterFx::Battle& Fx, int Puffs, const TheaterFx::Battle::View& Eye, Picture& P, bool bGroundOnly = false)
	{
		const std::vector<Vec>& Pts = Fx.Points();
		const std::vector<uint8_t>& Rgba = Fx.Colours();
		const int Per = Fx.PerPuff(), Ring = Fx.Ring();
		const double Gain = Fx.PictureGain();
		for (int Each = 0; Each < Puffs; ++Each)
		{
			const int First = Each * Per;
			std::vector<double> S(static_cast<size_t>(Per) * 2), C(static_cast<size_t>(Per) * 5);
			bool bOk = true;
			double Low = 1e9, High = -1e9;
			for (int K = 0; K < Per && bOk; ++K)
			{
				const Vec& W = Pts[static_cast<size_t>(First + K)];
				bOk = OnScreen(Eye, P, W, &S[static_cast<size_t>(K) * 2]);
				for (int Q = 0; Q < 4; ++Q)
				{
					const double V = Rgba[static_cast<size_t>(First + K) * 4 + static_cast<size_t>(Q)] / 255.0;
					C[static_cast<size_t>(K) * 5 + static_cast<size_t>(Q)] = Q < 3 ? V * Gain : V;
				}
				C[static_cast<size_t>(K) * 5 + 4] = W.Z; // (the field is flat at 0)
				Low = std::min(Low, W.Z);
				High = std::max(High, W.Z);
			}
			if (!bOk) continue;
			const bool bCut = !(Low > -0.05 && High < 0.3); // (a piece lying on the ground, a shadow or a mark, is never cut by it)
			if (bGroundOnly && bCut) continue;
			for (int K = 0; K < Ring; ++K)
			{
				const int Nx = (K + 1) % Ring;
				const int In0 = 1 + K, In1 = 1 + Nx, Out0 = 1 + Ring + K, Out1 = 1 + Ring + Nx;
				auto At = [&](int I) { return &S[static_cast<size_t>(I) * 2]; };
				auto Co = [&](int I) { return &C[static_cast<size_t>(I) * 5]; };
				P.Tri(At(0), At(In0), At(In1), Co(0), Co(In0), Co(In1), bCut);
				P.Tri(At(In0), At(In1), At(Out1), Co(In0), Co(In1), Co(Out1), bCut);
				P.Tri(At(In0), At(Out1), At(Out0), Co(In0), Co(Out1), Co(Out0), bCut);
			}
		}
	}

	// (the grenade's plume measured: painted again over black and over white, its opacity at each pixel is exact and its
	// own colour is what it laid over black over that opacity. At each distance down the wind from the canister, the
	// column of pixels through the plume's line there: its most opaque pixel, how tall the part over half opaque stands,
	// and where it is over 0.6 opaque, its top fifth's and its bottom fifth's own colour, as the picture writes them)
	void Measure(const TheaterFx::Battle& Fx, int Puffs, const TheaterFx::Battle::View& Eye, const Vec& Down, const char* Label)
	{
		Picture Black, White;
		Black.Fill(0.0);
		White.Fill(1.0);
		Paint(Fx, Puffs, Eye, Black);
		Paint(Fx, Puffs, Eye, White);
		std::printf("  %s  (metres down the wind: most opaque | metres over half opaque, how high its middle is | top fifth | underside fifth)\n", Label);
		for (const double M : { 1.0, 3.0, 6.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0 })
		{
			double S[2];
			if (!OnScreen(Eye, Black, Down * M + Vec(0.0, 0.0, 2.0), S)) continue;
			const int Cx = static_cast<int>(S[0]);
			if (Cx < 2 || Cx >= Black.W - 2) continue;
			// (heights there: the pixel rows of the ground and of 10 m up, so a row is a height)
			double G0[2], G10[2];
			OnScreen(Eye, Black, Down * M, G0);
			OnScreen(Eye, Black, Down * M + Vec(0.0, 0.0, 10.0), G10);
			const double PerRow = 10.0 / std::max(1e-6, G0[1] - G10[1]);
			double Most = 0.0, Sum = 0.0, SumZ = 0.0;
			int Over = 0;
			std::vector<std::array<double, 4>> Solid; // (from the top down: opacity and own colour)
			for (int Y = 0; Y < Black.H; ++Y)
			{
				double A = 0.0, Cr = 0.0, Cg = 0.0, Cb = 0.0;
				for (int Dx = -2; Dx <= 2; ++Dx)
				{
					const size_t I = static_cast<size_t>(Y * Black.W + Cx + Dx);
					A += 1.0 - (White.G[I] - Black.G[I]);
					Cr += Black.R[I];
					Cg += Black.G[I];
					Cb += Black.B[I];
				}
				A /= 5.0;
				Most = std::max(Most, A);
				Sum += A;
				SumZ += A * (G0[1] - (Y + 0.5)) * PerRow;
				Over += A > 0.5 ? 1 : 0;
				if (A > 0.6) Solid.push_back({ A, Cr / 5.0 / A, Cg / 5.0 / A, Cb / 5.0 / A });
			}
			// (pixels to metres at that distance)
			const double Far = (Down * M + Vec(0.0, 0.0, 2.0) - Eye.Eye).Dot(Eye.Forward);
			const double Tall = Over * Far / Eye.PixelsPerRadian;
			std::printf("  %4.0f m: %4.2f | %4.1f m tall, middle %4.1f m up", M, Most, Tall, Sum > 1e-6 ? SumZ / Sum : 0.0);
			if (Solid.size() >= 10)
			{
				const size_t Fifth = std::max<size_t>(1, Solid.size() / 5);
				double T[3] = { 0, 0, 0 }, U[3] = { 0, 0, 0 };
				for (size_t K = 0; K < Fifth; ++K)
					for (int Q = 0; Q < 3; ++Q)
					{
						T[Q] += Solid[K][static_cast<size_t>(Q + 1)];
						U[Q] += Solid[Solid.size() - 1 - K][static_cast<size_t>(Q + 1)];
					}
				auto Byte = [&](double V) { return std::lround(Srgb01(V / static_cast<double>(Fifth)) * 255.0); };
				std::printf(" | %3ld,%3ld,%3ld | %3ld,%3ld,%3ld", Byte(T[0]), Byte(T[1]), Byte(T[2]), Byte(U[0]), Byte(U[1]), Byte(U[2]));
			}
			std::printf("\n");
		}
	}

	// (paints what the kit laid for Eye over the sky and field, and saves it)
	bool Save(const TheaterFx::Battle& Fx, int Puffs, const TheaterFx::Battle::View& Eye, const std::string& Name, bool bGroundOnly = false)
	{
		Picture P;
		// (the horizon: below the middle when the eye looks up, above it when it looks down)
		const double Horizon = P.H * 0.5 + Eye.PixelsPerRadian * Eye.Forward.Z / std::max(1e-6, std::hypot(Eye.Forward.X, Eye.Forward.Y));
		P.Backdrop(Horizon);
		Paint(Fx, Puffs, Eye, P, bGroundOnly);
		return P.Save(Name);
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "usage: smoke_screen_picture battle-fx.json out-folder\n");
		return 2;
	}
	std::string Text, Error;
	LandJson::Json Root;
	if (!LandJson::ReadFile(Argv[1], Text) || !LandJson::Parse(Text, Root, Error))
	{
		std::fprintf(stderr, "cannot read %s: %s\n", Argv[1], Error.c_str());
		return 1;
	}
	JsonRecipe Recipe;
	Recipe.Walk(Root, "");
	const std::string Out = Argv[2];
	const std::vector<double> RoundMoments = { 0.4, 1.2, 4.0, 12.0, 30.0, 60.0, 85.0 };
	// (a tank commander's smoke grenade, 12.5 m across for 75 s: its first seconds, swelling, its plume, its last 15 s)
	const std::vector<double> GrenadeMoments = { 0.5, 2.0, 5.0, 12.0, 30.0, 62.0, 70.0, 85.0 };
	const Vec Wind(2.6, 1.2, 0.0); // (the land battle's own wind when the recording gives none)
	const Vec Down = Vec(Wind.X, Wind.Y, 0.0).Unit();
	int Status = 0;
	for (const char* Kind : { "wp", "canister", "grenade" })
	{
		const bool bGrenade = std::string(Kind) == "grenade";
		const std::vector<double>& Moments = bGrenade ? GrenadeMoments : RoundMoments;
		TheaterFx::Battle Fx;
		Fx.Setup(&Recipe, &Flat, nullptr, 7u);
		Fx.SetWind(Wind);
		Fx.Screen(0.0, Vec(0.0, 0.0, 0.0), bGrenade ? 12.5 : 18.0, bGrenade ? 75.0 : 60.0, Kind, 1);
		int Shot = 0;
		size_t MostPuffs = 0;
		const Picture Size;
		// (a grenade's plume runs down the wind: the eye on its middle)
		const TheaterFx::Battle::View Eye = EyeAt(Vec(10.0, -120.0, 1.7), bGrenade ? Vec(22.0, 0.0, 3.0) : Vec(10.0, 0.0, 4.0), 20.0, Size);
		for (const double Now : Moments)
		{
			// (stepped up to the moment at 30 frames a second, as the game's frames would lay it)
			int Puffs = 0;
			for (double T = (Shot == 0 ? 0.0 : Moments[static_cast<size_t>(Shot - 1)]); T <= Now + 1e-9; T += 1.0 / 30.0) Puffs = Fx.Draw(T, Eye);
			MostPuffs = std::max(MostPuffs, static_cast<size_t>(Puffs));
			char Name[256];
			std::snprintf(Name, sizeof Name, "%s/screen_%s_%02d_%04.1fs.ppm", Out.c_str(), Kind, Shot, Now);
			if (!Save(Fx, Puffs, Eye, Name)) Status = 1;
			std::printf("%-8s %5.1f s: %d puffs\n", Kind, Now, Puffs);
			if (bGrenade)
			{
				Measure(Fx, Puffs, Eye, Down, "from 120 m");
				if (std::fabs(Now - 30.0) < 1e-6)
				{
					// (the same moment from 40 m off square across the plume at a man's 1.5 m eye, a wider lens, on its first
					// 15 m; and from the game's camera 25 m up)
					const Vec Aside(-Down.Y, Down.X, 0.0);
					const TheaterFx::Battle::View Near = EyeAt(Down * 15.0 - Aside * 40.0 + Vec(0.0, 0.0, 1.5), Down * 15.0 + Vec(0.0, 0.0, 3.0), 32.0, Size);
					const int NearPuffs = Fx.Draw(Now, Near);
					std::snprintf(Name, sizeof Name, "%s/screen_%s_near40m_%04.1fs.ppm", Out.c_str(), Kind, Now);
					if (!Save(Fx, NearPuffs, Near, Name)) Status = 1;
					Measure(Fx, NearPuffs, Near, Down, "from 40 m, 1.5 m eye");
					const TheaterFx::Battle::View High = EyeAt(Vec(-15.0, -55.0, 25.0), Vec(18.0, 6.0, 0.0), 28.0, Size);
					const int HighPuffs = Fx.Draw(Now, High);
					std::snprintf(Name, sizeof Name, "%s/screen_%s_high25m_%04.1fs.ppm", Out.c_str(), Kind, Now);
					if (!Save(Fx, HighPuffs, High, Name)) Status = 1;
					std::snprintf(Name, sizeof Name, "%s/screen_%s_high25m_shadow_only_%04.1fs.ppm", Out.c_str(), Kind, Now);
					if (!Save(Fx, HighPuffs, High, Name, true)) Status = 1; // (its shadow alone, the smoke left out)
					Fx.Draw(Now, Eye); // (and back to the first eye, so the frames that follow are as they were)
				}
			}
			++Shot;
		}
		std::printf("%-8s most puffs %zu\n", Kind, MostPuffs);
	}
	return Status;
}
