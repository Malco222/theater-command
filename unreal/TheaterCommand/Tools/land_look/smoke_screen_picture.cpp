// Paints the smoke screen (TheaterFx::Battle::Screen, the effects kit's own code, no engine) as the kit lays it, from a
// man's eye 120 m off across the wind, at a few moments after it lands: a phosphorus round's and a burning bomb's, so
// its look can be judged before a Mac build. A plain software painter: each puff the kit lays is its middle and two
// rings of points, filled as the game's mesh fills them and laid over a plain sky and field, far to near.
//
//     g++ -std=c++20 -O2 -I Source/TheaterCommand Tools/land_look/smoke_screen_picture.cpp
//         Source/TheaterCommand/Effects/TheaterFxCore.cpp Source/TheaterCommand/Battle/LandJson.cpp -o /tmp/smoke_screen
//     /tmp/smoke_screen Content/Effects/battle-fx.json <out folder>        (PPM pictures; convert makes them PNG)
#include "Battle/LandJson.h"
#include "Effects/TheaterFxCore.h"

#include <algorithm>
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
		void Tri(const double* P0, const double* P1, const double* P2, const double* C0, const double* C1, const double* C2)
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
					double C[4];
					for (int K = 0; K < 4; ++K) C[K] = A * C0[K] + Bw * C1[K] + Cw * C2[K];
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
			auto Srgb = [](double V)
			{
				V = std::clamp(V, 0.0, 1.0);
				const double S = V <= 0.0031308 ? 12.92 * V : 1.055 * std::pow(V, 1.0 / 2.4) - 0.055;
				return static_cast<unsigned char>(std::lround(S * 255.0));
			};
			for (size_t I = 0; I < R.size(); ++I)
			{
				const unsigned char Px[3] = { Srgb(R[I]), Srgb(G[I]), Srgb(B[I]) };
				std::fwrite(Px, 1, 3, F);
			}
			std::fclose(F);
			return true;
		}
	};
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
	const double Moments[] = { 0.4, 1.2, 4.0, 12.0, 30.0, 60.0, 85.0 };
	int Status = 0;
	for (const char* Kind : { "wp", "canister" })
	{
		TheaterFx::Battle Fx;
		Fx.Setup(&Recipe, &Flat, nullptr, 7u);
		Fx.SetWind(Vec(2.6, 1.2, 0.0)); // (the land battle's own wind when the recording gives none)
		Fx.Screen(0.0, Vec(0.0, 0.0, 0.0), 18.0, 60.0, Kind, 1);
		int Shot = 0;
		size_t MostPuffs = 0;
		for (const double Now : Moments)
		{
			// (stepped up to the moment at 30 frames a second, as the game's frames would lay it)
			TheaterFx::Battle::View Eye;
			Eye.Eye = Vec(10.0, -120.0, 1.7);
			const Vec Look = Vec(10.0, 0.0, 4.0) - Eye.Eye;
			Eye.Forward = Look.Unit();
			Eye.Right = Eye.Forward.Cross(Vec(0.0, 0.0, 1.0)).Unit();
			Eye.Up = Eye.Right.Cross(Eye.Forward).Unit();
			const double HalfView = 0.5 * 40.0 * 3.14159265358979 / 180.0;
			Picture P;
			Eye.PixelsPerRadian = P.W / (2.0 * std::tan(HalfView));
			Eye.HalfAcross = std::tan(HalfView);
			Eye.HalfUp = Eye.HalfAcross * P.H / P.W;
			Eye.Sun = Vec(-0.4, -0.5, 0.75).Unit();
			int Puffs = 0;
			for (double T = (Shot == 0 ? 0.0 : Moments[Shot - 1]); T <= Now + 1e-9; T += 1.0 / 30.0) Puffs = Fx.Draw(T, Eye);
			MostPuffs = std::max(MostPuffs, static_cast<size_t>(Puffs));
			const double Ppr = Eye.PixelsPerRadian;
			const double Horizon = P.H * 0.5 - Ppr * std::atan2(Eye.Forward.Z, std::hypot(Eye.Forward.X, Eye.Forward.Y));
			P.Backdrop(Horizon);
			const std::vector<Vec>& Pts = Fx.Points();
			const std::vector<uint8_t>& Rgba = Fx.Colours();
			const int Per = Fx.PerPuff(), Ring = Fx.Ring();
			const double Gain = Fx.PictureGain();
			auto Screen = [&](const Vec& W, double* S)
			{
				const Vec D = W - Eye.Eye;
				const double Z = D.Dot(Eye.Forward);
				if (Z < 0.1) return false;
				S[0] = P.W * 0.5 + Ppr * D.Dot(Eye.Right) / Z;
				S[1] = P.H * 0.5 - Ppr * D.Dot(Eye.Up) / Z;
				return true;
			};
			for (int Each = 0; Each < Puffs; ++Each)
			{
				const int First = Each * Per;
				std::vector<double> S(static_cast<size_t>(Per) * 2), C(static_cast<size_t>(Per) * 4);
				bool bOk = true;
				for (int K = 0; K < Per && bOk; ++K)
				{
					bOk = Screen(Pts[static_cast<size_t>(First + K)], &S[static_cast<size_t>(K) * 2]);
					for (int Q = 0; Q < 4; ++Q)
					{
						const double V = Rgba[static_cast<size_t>(First + K) * 4 + static_cast<size_t>(Q)] / 255.0;
						C[static_cast<size_t>(K) * 4 + static_cast<size_t>(Q)] = Q < 3 ? V * Gain : V;
					}
				}
				if (!bOk) continue;
				for (int K = 0; K < Ring; ++K)
				{
					const int Nx = (K + 1) % Ring;
					const int In0 = 1 + K, In1 = 1 + Nx, Out0 = 1 + Ring + K, Out1 = 1 + Ring + Nx;
					auto At = [&](int I) { return &S[static_cast<size_t>(I) * 2]; };
					auto Co = [&](int I) { return &C[static_cast<size_t>(I) * 4]; };
					P.Tri(At(0), At(In0), At(In1), Co(0), Co(In0), Co(In1));
					P.Tri(At(In0), At(In1), At(Out1), Co(In0), Co(In1), Co(Out1));
					P.Tri(At(In0), At(Out1), At(Out0), Co(In0), Co(Out1), Co(Out0));
				}
			}
			char Name[256];
			std::snprintf(Name, sizeof Name, "%s/screen_%s_%02d_%04.1fs.ppm", Out.c_str(), Kind, Shot, Now);
			if (!P.Save(Name)) Status = 1;
			std::printf("%-8s %5.1f s: %d puffs\n", Kind, Now, Puffs);
			++Shot;
		}
		std::printf("%-8s most puffs %zu\n", Kind, MostPuffs);
	}
	return Status;
}
