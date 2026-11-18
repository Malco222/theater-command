// The game kit's own preview: the battle effects code the game runs (unreal/.../Effects/TheaterFxCore.cpp), drawn
// here by a small software painter so that the art director can see what the code draws without the Mac.
//
//   python3 flatten.py ../fx.json > /tmp/recipe.txt
//   SRC=../../../unreal/TheaterCommand/Source/TheaterCommand
//   clang++ -O2 -std=c++20 -I $SRC fxpreview.cpp $SRC/Effects/TheaterFxCore.cpp -o fxpreview
//   ./fxpreview /tmp/recipe.txt <out folder> <size> <ground> <frames, e.g. 4,8,40> [WxH] [back up]
//
// It draws what the game's mesh would carry: the same puffs, in the same order, with the same colours and alphas,
// laid over a plain field (grass, a gentle swell, a sky) with five men of 1.75 m standing round the hit. The flash's
// lamp lifts the ground round it as the game's lamp would. Unreal's tone curve is not copied: the colours here are
// the recipe's picture colours put straight on the screen.

#include "Effects/TheaterFxCore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using TheaterFx::Battle;
using TheaterFx::Rgb;
using TheaterFx::Vec;

namespace
{
	class FlatRecipe : public TheaterFx::Recipe
	{
	public:
		std::map<std::string, double> Numbers;
		std::map<std::string, std::string> Words;
		bool Find(const char* Path, double& Out) const override
		{
			auto It = Numbers.find(Path);
			if (It == Numbers.end())
			{
				return false;
			}
			Out = It->second;
			return true;
		}
		bool FindWord(const char* Path, std::string& Out) const override
		{
			auto It = Words.find(Path);
			if (It == Words.end())
			{
				return false;
			}
			Out = It->second;
			return true;
		}
	};

	double Swell(void*, double X, double Y)
	{
		return 0.35 * std::sin(X * 0.045) * std::cos(Y * 0.037) + 0.15 * std::sin((X + Y) * 0.11);
	}

	double ToLinear(double C255)
	{
		const double C = C255 / 255.0;
		return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
	}

	double ToPicture(double L)
	{
		L = std::max(0.0, std::min(1.0, L));
		return 255.0 * (L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(L, 1.0 / 2.4) - 0.055);
	}

	struct Canvas
	{
		int W = 960, H = 540;
		std::vector<double> R, G, B, Depth;
		void Init(int InW, int InH)
		{
			W = InW;
			H = InH;
			R.assign(static_cast<size_t>(W * H), 0.0);
			G = R;
			B = R;
			Depth.assign(static_cast<size_t>(W * H), 1e30);
		}
		void Save(const std::string& Path) const
		{
			std::ofstream F(Path, std::ios::binary);
			F << "P6\n" << W << " " << H << "\n255\n";
			for (size_t I = 0; I < R.size(); ++I)
			{
				const unsigned char Px[3] = { static_cast<unsigned char>(ToPicture(R[I])), static_cast<unsigned char>(ToPicture(G[I])), static_cast<unsigned char>(ToPicture(B[I])) };
				F.write(reinterpret_cast<const char*>(Px), 3);
			}
		}
	};

	struct Camera
	{
		Vec Eye, Right, Up, Forward;
		double Ppr = 1000.0;
		int W = 960, H = 540;
		bool Project(const Vec& P, double& Sx, double& Sy, double& Z) const
		{
			const Vec D = P - Eye;
			Z = D.Dot(Forward);
			if (Z < 0.2)
			{
				return false;
			}
			Sx = W * 0.5 + D.Dot(Right) / Z * Ppr;
			Sy = H * 0.5 - D.Dot(Up) / Z * Ppr;
			return true;
		}
		Vec Ray(double Sx, double Sy) const
		{
			return (Forward + Right * ((Sx - W * 0.5) / Ppr) + Up * ((H * 0.5 - Sy) / Ppr)).Unit();
		}
	};

	double Noise(double X, double Y)
	{
		const double A = std::sin(X * 0.73 + std::sin(Y * 0.31) * 2.0) + std::sin(Y * 0.57 + std::sin(X * 0.23) * 2.0);
		const double B = std::sin(X * 3.1 + Y * 1.7) * std::sin(Y * 2.9 - X * 1.3);
		return 0.5 + 0.3 * A * 0.5 + 0.2 * B;
	}

	void Paint(Canvas& C, const Camera& Cam, const Battle::Lamp* Lamps, int NLamps, const std::vector<Vec>& Men, bool bSnow)
	{
		// (grass, or a snowfield under a snow hit, so that a white skirt is judged on white)
		const double GrassA[3] = { ToLinear(bSnow ? 226 : 124), ToLinear(bSnow ? 230 : 132), ToLinear(bSnow ? 236 : 74) };
		const double GrassB[3] = { ToLinear(bSnow ? 196 : 98), ToLinear(bSnow ? 202 : 96), ToLinear(bSnow ? 212 : 60) };
		const double Haze[3] = { ToLinear(206), ToLinear(214), ToLinear(222) };
		for (int Y = 0; Y < C.H; ++Y)
		{
			for (int X = 0; X < C.W; ++X)
			{
				const size_t I = static_cast<size_t>(Y * C.W + X);
				const Vec D = Cam.Ray(X + 0.5, Y + 0.5);
				double Col[3];
				if (D.Z < -1e-4)
				{
					// march to the ground: plane first, then two corrections for the swell
					double T = -Cam.Eye.Z / D.Z;
					for (int K = 0; K < 3; ++K)
					{
						const Vec P = Cam.Eye + D * T;
						T = (Swell(nullptr, P.X, P.Y) - Cam.Eye.Z) / D.Z;
					}
					const Vec P = Cam.Eye + D * T;
					const double N = Noise(P.X * 0.35, P.Y * 0.35) * 0.7 + Noise(P.X * 3.0, P.Y * 3.0) * 0.3;
					for (int K = 0; K < 3; ++K)
					{
						Col[K] = GrassA[K] + (GrassB[K] - GrassA[K]) * N;
					}
					double Lift[3] = { 1.0, 1.0, 1.0 };
					for (int L = 0; L < NLamps; ++L)
					{
						const double Dist = (P - Lamps[L].At).Length();
						if (Dist < Lamps[L].Radius)
						{
							// (as the game's lamp: inverse square, windowed to nothing at its radius; 64 puts the passed lift of
							// 1.24x on the ground 3 m from a 105, whose lamp hangs 16 m up)
							const double Window = 1.0 - std::pow(Dist / Lamps[L].Radius, 4.0);
							const double F = Lamps[L].Strength * 64.0 / std::max(Dist * Dist, 1.0) * Window * Window;
							Lift[0] += F * Lamps[L].Col.R / std::max(0.01f, Lamps[L].Col.G);
							Lift[1] += F;
							Lift[2] += F * Lamps[L].Col.B / std::max(0.01f, Lamps[L].Col.G);
						}
					}
					const double Fog = 1.0 - std::exp(-std::max(0.0, T - 60.0) / 700.0);
					for (int K = 0; K < 3; ++K)
					{
						Col[K] = Col[K] * Lift[K] * (1.0 - Fog) + Haze[K] * Fog;
					}
					C.Depth[I] = (D * T).Dot(Cam.Forward);
				}
				else
				{
					const double Up = std::min(1.0, D.Z * 4.0);
					const double Top[3] = { ToLinear(150), ToLinear(178), ToLinear(214) };
					for (int K = 0; K < 3; ++K)
					{
						Col[K] = Haze[K] + (Top[K] - Haze[K]) * Up;
					}
				}
				C.R[I] = Col[0];
				C.G[I] = Col[1];
				C.B[I] = Col[2];
			}
		}
		// the men, 1.75 m: a pale figure, as the white soldiers in the Blender stills
		for (const Vec& M : Men)
		{
			double Sx0, Sy0, Z0, Sx1, Sy1, Z1;
			if (!Cam.Project(M, Sx0, Sy0, Z0) || !Cam.Project(M + Vec(0, 0, 1.75), Sx1, Sy1, Z1))
			{
				continue;
			}
			const double Tall = Sy0 - Sy1;
			const double Wide = std::max(1.0, Tall * 0.26);
			for (int Y = static_cast<int>(Sy1); Y <= static_cast<int>(Sy0); ++Y)
			{
				const double F = (Y - Sy1) / std::max(Tall, 1.0);
				const double Half = F < 0.14 ? Wide * 0.32 : (F < 0.55 ? Wide * 0.5 : Wide * 0.36);
				for (int X = static_cast<int>(Sx0 - Half); X <= static_cast<int>(Sx0 + Half); ++X)
				{
					if (X < 0 || Y < 0 || X >= C.W || Y >= C.H)
					{
						continue;
					}
					const size_t I = static_cast<size_t>(Y * C.W + X);
					if (Z0 < C.Depth[I])
					{
						const double Shade = X < Sx0 ? 0.62 : 0.78;
						C.R[I] = C.G[I] = C.B[I] = Shade;
						C.Depth[I] = Z0;
					}
				}
			}
		}
	}

	void Triangle(Canvas& C, const Camera& Cam, const Vec P[3], const unsigned char* Rgba[3])
	{
		double Sx[3], Sy[3], Z[3];
		for (int K = 0; K < 3; ++K)
		{
			if (!Cam.Project(P[K], Sx[K], Sy[K], Z[K]))
			{
				return;
			}
		}
		const double Area = (Sx[1] - Sx[0]) * (Sy[2] - Sy[0]) - (Sx[2] - Sx[0]) * (Sy[1] - Sy[0]);
		if (std::fabs(Area) < 1e-9)
		{
			return;
		}
		const int X0 = std::max(0, static_cast<int>(std::floor(std::min({ Sx[0], Sx[1], Sx[2] }))));
		const int X1 = std::min(C.W - 1, static_cast<int>(std::ceil(std::max({ Sx[0], Sx[1], Sx[2] }))));
		const int Y0 = std::max(0, static_cast<int>(std::floor(std::min({ Sy[0], Sy[1], Sy[2] }))));
		const int Y1 = std::min(C.H - 1, static_cast<int>(std::ceil(std::max({ Sy[0], Sy[1], Sy[2] }))));
		for (int Y = Y0; Y <= Y1; ++Y)
		{
			for (int X = X0; X <= X1; ++X)
			{
				const double Px = X + 0.5, Py = Y + 0.5;
				const double W0 = ((Sx[1] - Px) * (Sy[2] - Py) - (Sx[2] - Px) * (Sy[1] - Py)) / Area;
				const double W1 = ((Sx[2] - Px) * (Sy[0] - Py) - (Sx[0] - Px) * (Sy[2] - Py)) / Area;
				const double W2 = 1.0 - W0 - W1;
				if (W0 < 0 || W1 < 0 || W2 < 0)
				{
					continue;
				}
				const size_t I = static_cast<size_t>(Y * C.W + X);
				const double Deep = W0 * Z[0] + W1 * Z[1] + W2 * Z[2];
				if (Deep > C.Depth[I])
				{
					continue; // (behind the ground or a man: as Unreal's depth test for see-through things)
				}
				const double A = (W0 * Rgba[0][3] + W1 * Rgba[1][3] + W2 * Rgba[2][3]) / 255.0;
				if (A <= 0.0)
				{
					continue;
				}
				const double Cr = (W0 * Rgba[0][0] + W1 * Rgba[1][0] + W2 * Rgba[2][0]) / 255.0;
				const double Cg = (W0 * Rgba[0][1] + W1 * Rgba[1][1] + W2 * Rgba[2][1]) / 255.0;
				const double Cb = (W0 * Rgba[0][2] + W1 * Rgba[1][2] + W2 * Rgba[2][2]) / 255.0;
				C.R[I] = C.R[I] * (1.0 - A) + Cr * A;
				C.G[I] = C.G[I] * (1.0 - A) + Cg * A;
				C.B[I] = C.B[I] * (1.0 - A) + Cb * A;
			}
		}
	}

	void Puffs(Canvas& C, const Camera& Cam, const Battle& Fx, int N)
	{
		const std::vector<Vec>& P = Fx.Points();
		const std::vector<uint8_t>& Rgba = Fx.Colours();
		const int Rim = Fx.Ring();
		for (int Each = 0; Each < N; ++Each)
		{
			const int First = Each * Fx.PerPuff();
			auto Tri = [&](int A, int B, int Cc)
			{
				const Vec Pts[3] = { P[static_cast<size_t>(A)], P[static_cast<size_t>(B)], P[static_cast<size_t>(Cc)] };
				const unsigned char* Cols[3] = { &Rgba[static_cast<size_t>(A) * 4], &Rgba[static_cast<size_t>(B) * 4], &Rgba[static_cast<size_t>(Cc) * 4] };
				Triangle(C, Cam, Pts, Cols);
			};
			for (int K = 0; K < Rim; ++K)
			{
				const int Next = (K + 1) % Rim;
				const int In0 = First + 1 + K, In1 = First + 1 + Next, Out0 = First + 1 + Rim + K, Out1 = First + 1 + Rim + Next;
				Tri(First, In1, In0);
				Tri(In0, In1, Out1);
				Tri(In0, Out1, Out0);
			}
		}
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 6)
	{
		std::fprintf(stderr, "fxpreview recipe.txt out size ground frames [WxH] [back up]\n");
		return 1;
	}
	FlatRecipe Book;
	{
		std::ifstream F(Argv[1]);
		std::string Line;
		while (std::getline(F, Line))
		{
			const size_t Tab = Line.find('\t');
			if (Tab == std::string::npos)
			{
				continue;
			}
			const std::string Key = Line.substr(0, Tab), Val = Line.substr(Tab + 1);
			if (!Val.empty() && Val[0] == '=')
			{
				Book.Words[Key] = Val.substr(1);
			}
			else
			{
				Book.Numbers[Key] = std::stod(Val);
			}
		}
	}
	const std::string OutDir = Argv[2], Size = Argv[3], Ground = Argv[4];
	std::vector<int> Frames;
	{
		std::stringstream S(Argv[5]);
		std::string Part;
		while (std::getline(S, Part, ','))
		{
			Frames.push_back(std::stoi(Part));
		}
	}
	int W = 960, H = 540;
	if (Argc > 6)
	{
		std::sscanf(Argv[6], "%dx%d", &W, &H);
	}
	const double Back = Argc > 7 ? std::stod(Argv[7]) : 40.0, UpBy = Argc > 8 ? std::stod(Argv[8]) : 14.0;

	Battle Fx;
	const char* SeedIs = std::getenv("FX_SEED"); // (FX_SEED=n: another throw of the dice, for the same hit seen several ways)
	Fx.Setup(&Book, &Swell, nullptr, SeedIs ? static_cast<uint32_t>(std::strtoul(SeedIs, nullptr, 10)) : 3u);
	Fx.SetWind(Vec(1.6, 0.4, 0.0));
	const double Scale = Book.Numbers.count("sizes." + Size + ".scale") ? Book.Numbers["sizes." + Size + ".scale"] : 1.0;
	const char* GrowIs = std::getenv("FX_FRAME_AS"); // (FX_FRAME_AS=size: frame the shot as for that size, so two sizes can be seen from one camera)
	const double FrameScale = GrowIs && Book.Numbers.count(std::string("sizes.") + GrowIs + ".scale") ? Book.Numbers[std::string("sizes.") + GrowIs + ".scale"] : Scale;
	const double Grow = std::pow(std::max(0.6, FrameScale), 0.75);
	const Vec Hit(0.0, 0.0, Swell(nullptr, 0.0, 0.0));
	const Vec Away = Vec(-0.35, -1.0, 0.0).Unit();
	Camera Cam;
	Cam.W = W;
	Cam.H = H;
	Cam.Eye = Hit + Away * (Back * Grow) + Vec(0, 0, UpBy * Grow);
	const Vec Look = Hit + (Vec(0, 0, 5.0) - Away * 6.0) * Grow;
	Cam.Forward = (Look - Cam.Eye).Unit();
	Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
	Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
	Cam.Ppr = W / (2.0 * std::tan(std::atan(18.0 / 35.0))); // (a 35 mm lens, as the stills)
	std::vector<Vec> Men;
	const double MenAt = 16.0 * std::max(1.0, std::pow(Scale, 0.6));
	for (int I = 0; I < 5; ++I)
	{
		const double A = (200.0 + 28.0 * I) * 3.14159265 / 180.0;
		const double X = std::cos(A) * MenAt, Y = std::sin(A) * MenAt;
		Men.push_back(Vec(X, Y, Swell(nullptr, X, Y)));
	}
	// a man at the hit's foot too, just off the crater's lip on the camera's side
	{
		const double Off = Book.Numbers["sizes." + Size + ".crater_m"] * 0.6 + 2.0;
		const Vec M = Hit + Away * Off + Cam.Right * 1.0;
		Men.push_back(Vec(M.X, M.Y, Swell(nullptr, M.X, M.Y)));
	}

	Battle::View View;
	View.Eye = Cam.Eye;
	View.Right = Cam.Right;
	View.Up = Cam.Up;
	View.Forward = Cam.Forward;
	View.PixelsPerRadian = Cam.Ppr;
	if (const char* SunIs = std::getenv("FX_SUN")) // (FX_SUN=x,y,z: the way toward the sun; by default high behind the eye's left)
	{
		double X = 0.0, Y = 0.0, Z = 1.0;
		if (std::sscanf(SunIs, "%lf,%lf,%lf", &X, &Y, &Z) == 3)
		{
			View.Sun = Vec(X, Y, Z).Unit();
		}
	}
	Fx.Draw(-0.1, View); // (the eye is known before the hit, as in the game)
	Fx.Shell(-0.02, Hit, Size.c_str(), Ground.c_str(), false);
	std::sort(Frames.begin(), Frames.end());
	for (int Frame : Frames)
	{
		const double Now = Frame / 30.0;
		const Battle::Jolt J = Fx.Shake(Now, Cam.Eye);
		Camera Shot = Cam;
		Shot.Eye = Cam.Eye + J.Offset;
		View.Eye = Shot.Eye;
		const int N = Fx.Draw(Now, View);
		Battle::Lamp Lamps[2];
		const int NL = Fx.Lamps(Now, Lamps, 2);
		Canvas C;
		C.Init(W, H);
		Paint(C, Shot, Lamps, NL, Men, Ground == "snow");
		Puffs(C, Shot, Fx, N);
		char Name[256];
		std::snprintf(Name, sizeof(Name), "%s/%s-%s-f%03d.ppm", OutDir.c_str(), Size.c_str(), Ground.c_str(), Frame);
		C.Save(Name);
		std::printf("frame %d: %d puffs, %d lamps, jolt %.3f m\n", Frame, N, NL, J.Offset.Length());
	}
	return 0;
}
