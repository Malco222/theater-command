// The game kit's own preview: the battle effects code the game runs (unreal/.../Effects/TheaterFxCore.cpp), drawn
// here by a small software painter so that the art director can see what the code draws without the Mac.
//
//   python3 flatten.py ../fx.json > /tmp/recipe.txt
//   SRC=../../../unreal/TheaterCommand/Source/TheaterCommand
//   clang++ -O2 -std=c++20 -I $SRC fxpreview.cpp $SRC/Effects/TheaterFxCore.cpp -o fxpreview
//   ./fxpreview /tmp/recipe.txt <out folder> <size> <ground> <frames, e.g. 4,8,40> [WxH] [back up]
//     (size "mg": a machine gun's burst from 30 m away into the ground there, in place of a shell; FX_MG_VIEW=side|behind)
//   ./fxpreview /tmp/recipe.txt <out folder> air <rifle|hmg|cannon20> <frames> [WxH] [chase|side|close|far|target|hit|trail|wake|map|fall]
//   ./fxpreview /tmp/recipe.txt <out folder> oil <sunk|burning|horizon> <frames> [WxH]
//
// The oil shot: one oil fire on a flat sea (FX_REACH metres of oil, 100; seen from FX_DIST metres off, 1200, at
// FX_EYE_H metres, 8, looking at FX_LOOK_H metres up its column, 150), or "horizon": three fires 3, 6 and 9 km off.
// Frames count from when it was lit; FX_SINCE=s opens with it already burning that long. The wind is FX_WIND_X,
// FX_WIND_Y metres a second (3, 0.8). FX_DUSK=1 dims the sea and the smoke's light, so the fire's lamp shows.
//
// The air shot: a fighter at 1,500 m and 150 m/s in a banked turn (FX_TURN=degrees a second, 3 by default) fires its
// wing guns, laid to meet 300 m ahead, from the first frame; AirGun is called sixty times a second as the game would.
// FX_DUSK=1 darkens the sky and the smoke to dusk. The hit and trail shots turn it round: the aircraft is struck by
// that kind of gun along its wing and nose from 0.2 s to 1 s, and its engine trails FX_TRAIL (smoke, fire, glycol)
// from 0.3 s; hit looks from the side, trail from above and behind, wake at the whole trail from FX_DIST m off the beam
// (2500). FX_SPAN=metres gives TrailStart the aircraft's span (a fire's flame is sized by it); FX_FIRE_X=metres forward
// of the middle puts the trail's point (2.9, the cowl; 1, the wing root). map is the strategy
// camera: the guns firing seen from FX_DIST metres (1400) high abeam, where flashes are held at their pixel floor.
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

	void Paint(Canvas& C, const Camera& Cam, const Battle::Lamp* Lamps, int NLamps, const std::vector<Vec>& Men, bool bSnow, bool bSea = false)
	{
		// (grass, or a snowfield under a snow hit, so that a white skirt is judged on white; or the open sea, flat, for
		// the oil fires, its haze drawn out to kilometres)
		const double GrassA[3] = { ToLinear(bSea ? 58 : bSnow ? 226 : 124), ToLinear(bSea ? 80 : bSnow ? 230 : 132), ToLinear(bSea ? 98 : bSnow ? 236 : 74) };
		const double GrassB[3] = { ToLinear(bSea ? 44 : bSnow ? 196 : 98), ToLinear(bSea ? 64 : bSnow ? 202 : 96), ToLinear(bSea ? 82 : bSnow ? 212 : 60) };
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
					for (int K = 0; K < 3 && !bSea; ++K)
					{
						const Vec P = Cam.Eye + D * T;
						T = (Swell(nullptr, P.X, P.Y) - Cam.Eye.Z) / D.Z;
					}
					const Vec P = Cam.Eye + D * T;
					const double N = bSea ? Noise(P.X * 0.02, P.Y * 0.08) * 0.6 + Noise(P.X * 0.3, P.Y * 0.9) * 0.4 : Noise(P.X * 0.35, P.Y * 0.35) * 0.7 + Noise(P.X * 3.0, P.Y * 3.0) * 0.3;
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
					const double Fog = 1.0 - std::exp(-std::max(0.0, T - 60.0) / (bSea ? 6000.0 : 700.0));
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

	/** A flat-coloured triangle that hides what is behind it (the aircraft in the air shot). */
	void Solid(Canvas& C, const Camera& Cam, const Vec P[3], const double Col[3])
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
					continue;
				}
				C.R[I] = Col[0];
				C.G[I] = Col[1];
				C.B[I] = Col[2];
				C.Depth[I] = Deep;
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

namespace
{
	double FlatSea(void*, double, double) { return 0.0; }

	/** The oil fires (see the top of the file). */
	int Oil(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Shot, std::vector<int> Frames, int W, int H)
	{
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		Battle Fx;
		Fx.Setup(&Book, &FlatSea, nullptr, 3u);
		Fx.SetWind(Vec(EnvNum("FX_WIND_X", 3.0), EnvNum("FX_WIND_Y", 0.8), 0.0));
		const double Since = EnvNum("FX_SINCE", 0.0);
		Camera Cam;
		Cam.W = W;
		Cam.H = H;
		Cam.Ppr = W / (2.0 * std::tan(std::atan(18.0 / 35.0)));
		if (Shot == "horizon")
		{
			// (three fires far off, as the sea battle's low camera sees the ships lost: two slicks and one ship afire)
			Fx.OilFire(0.0, 1, Vec(-900.0, 3000.0, 0.0), 110.0, "sunk", 0.0, Since);
			Fx.OilFire(0.0, 2, Vec(1400.0, 6000.0, 0.0), 80.0, "sunk", 0.0, Since);
			Fx.OilFire(0.0, 3, Vec(2900.0, 9000.0, 0.0), 70.0, "burning", 0.0, Since);
			Cam.Eye = Vec(0.0, 0.0, EnvNum("FX_EYE_H", 8.0));
			Cam.Forward = Vec(0.0, 1.0, 0.06).Unit();
		}
		else
		{
			// (one fire, Shot its kind, FX_REACH metres of oil, seen from FX_DIST metres off at FX_EYE_H metres)
			const double Reach = EnvNum("FX_REACH", 100.0), Dist = EnvNum("FX_DIST", 1200.0);
			Fx.OilFire(0.0, 1, Vec(0.0, 0.0, 0.0), Reach, Shot.c_str(), 0.0, Since);
			Cam.Eye = Vec(-0.3, -1.0, 0.0).Unit() * Dist + Vec(0.0, 0.0, EnvNum("FX_EYE_H", 8.0));
			Cam.Forward = (Vec(0.0, 0.0, EnvNum("FX_LOOK_H", 150.0)) - Cam.Eye).Unit();
		}
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
		Battle::View View;
		View.Eye = Cam.Eye;
		View.Right = Cam.Right;
		View.Up = Cam.Up;
		View.Forward = Cam.Forward;
		View.PixelsPerRadian = Cam.Ppr;
		if (const char* SunIs = std::getenv("FX_SUN")) // (FX_SUN=x,y,z: the way toward the sun)
		{
			double X = 0.0, Y = 0.0, Z = 1.0;
			if (std::sscanf(SunIs, "%lf,%lf,%lf", &X, &Y, &Z) == 3)
			{
				View.Sun = Vec(X, Y, Z).Unit();
			}
		}
		const bool bDusk = std::getenv("FX_DUSK") != nullptr;
		if (bDusk)
		{
			View.SmokeLight = Rgb(0.22f, 0.2f, 0.24f);
			View.SunLight = Rgb(0.0f, 0.0f, 0.0f);
		}
		std::sort(Frames.begin(), Frames.end());
		int Drawn = 0;
		const std::vector<Vec> Nobody;
		for (int Frame : Frames)
		{
			for (; Drawn < Frame; ++Drawn)
			{
				Fx.Draw(Drawn / 30.0, View);
			}
			Drawn = Frame + 1;
			const double Now = Frame / 30.0;
			const int N = Fx.Draw(Now, View);
			Battle::Lamp Lamps[2];
			const int NL = Fx.Lamps(Now, Lamps, 2, Fx.DayOf(View.SmokeLight));
			Canvas C;
			C.Init(W, H);
			Paint(C, Cam, Lamps, NL, Nobody, false, true);
			if (bDusk)
			{
				for (size_t I = 0; I < C.R.size(); ++I)
				{
					C.R[I] *= 0.22;
					C.G[I] *= 0.2;
					C.B[I] *= 0.26;
				}
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/oil-%s-f%04d.ppm", OutDir.c_str(), Shot.c_str(), Frame);
			C.Save(Name);
			std::printf("frame %d: %d puffs, %d lamps\n", Frame, N, NL);
		}
		return 0;
	}

	/** The air shot (see the top of the file). */
	int Air(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Kind, std::vector<int> Frames, int W, int H, const std::string& ShotName)
	{
		Battle Fx;
		const char* SeedIs = std::getenv("FX_SEED");
		Fx.Setup(&Book, &Swell, nullptr, SeedIs ? static_cast<uint32_t>(std::strtoul(SeedIs, nullptr, 10)) : 3u);
		if (ShotName != "above")
		{
			Fx.SetDrawBeyond(Fx.AirBeyond()); // (as the air scene does: UseAirDistance)
		}
		Fx.SetWind(Vec(1.6, 0.4, 0.0));
		const bool bDusk = std::getenv("FX_DUSK") != nullptr;
		const char* TurnIs = std::getenv("FX_TURN");
		const double Pi = 3.14159265358979;
		const double Speed = 150.0, Turn = (TurnIs ? std::atof(TurnIs) : 3.0) * Pi / 180.0, Height = 1500.0;
		const Vec ZUp(0.0, 0.0, 1.0);
		struct Frame
		{
			Vec At, Fw, Up, Right;
		};
		auto Fly = [&](double T)
		{
			// (a level turn to the left: the nose swings at Turn, the wings banked as a true turn needs)
			Frame F;
			const double Psi = Turn * T;
			const double R = Turn > 1e-6 ? Speed / Turn : 0.0;
			F.At = Turn > 1e-6 ? Vec(R * std::sin(Psi), R * (1.0 - std::cos(Psi)), Height) : Vec(Speed * T, 0.0, Height);
			F.Fw = Vec(std::cos(Psi), std::sin(Psi), 0.0);
			const double Bank = std::atan(Speed * Turn / 9.81);
			const Vec Left = ZUp.Cross(F.Fw);
			F.Up = (ZUp * std::cos(Bank) + Left * std::sin(Bank)).Unit();
			F.Right = F.Fw.Cross(F.Up).Unit();
			return F;
		};
		// the guns, in the aircraft's own frame (forward, right, up), metres
		std::vector<Vec> Guns;
		auto Pair = [&](double Fwd, double Out, double Up_)
		{
			Guns.push_back(Vec(Fwd, Out, Up_));
			Guns.push_back(Vec(Fwd, -Out, Up_));
		};
		if (Kind == "cannon20")
		{
			Pair(1.7, 2.0, -0.32);
			Pair(1.6, 2.6, -0.32);
		}
		else if (Kind == "rifle")
		{
			for (double Out : { 2.4, 2.8, 3.2, 3.6 })
			{
				Pair(1.2, Out, -0.3);
			}
		}
		else
		{
			for (double Out : { 2.3, 2.75, 3.2 })
			{
				Pair(1.25, Out, -0.3);
			}
		}
		auto Place = [](const Frame& F, const Vec& L) { return F.At + F.Fw * L.X + F.Right * L.Y + F.Up * L.Z; };
		auto Level_ = [](const Frame& F) { return Vec(F.Fw.X, F.Fw.Y, 0.0).Unit(); };
		auto Right_ = [&](const Frame& F) { return Vec(F.Fw.X, F.Fw.Y, 0.0).Unit().Cross(ZUp).Unit(); };
		const double Meet = 300.0;
		const bool bStruck = ShotName == "hit" || ShotName == "trail" || ShotName == "wake";
		// "fall": the aircraft falls from 0.5 s, FX_FALL its way (burn, spin, explode), shedding its debris row into a
		// deck FX_DECK metres below (400); seen from FX_DIST metres off the beam (140). FX_SPAN its span (11)
		const bool bFall = ShotName == "fall";
		const char* FallIs = std::getenv("FX_FALL");
		const std::string FallKind = FallIs ? FallIs : "burn";
		const double FallFrom = 0.5;
		auto Fall = [&](double T)
		{
			if (!bFall || T <= FallFrom)
			{
				return Fly(T);
			}
			const Frame F0 = Fly(FallFrom);
			const Vec L0 = Vec(F0.Fw.X, F0.Fw.Y, 0.0).Unit();
			const Vec R0 = L0.Cross(ZUp).Unit();
			const double U = T - FallFrom;
			Frame F;
			if (FallKind == "explode")
			{
				return Fly(T); // (gone, not drawn: its path only gives the moment's velocity)
			}
			if (FallKind == "spin")
			{
				// (a spin: nose well down, turning about a steep line through the air, sinking at about 45 m/s)
				const double Om = 2.0 * Pi / 2.8, Rr = 18.0, Settle = std::min(1.0, U / 2.0);
				const double A = Om * U;
				const Vec Mid = F0.At + L0 * (Speed * 1.2 * (1.0 - std::exp(-U))) - R0 * Rr;
				F.At = Mid + (R0 * std::cos(A) + L0 * std::sin(A)) * (Rr * Settle) - ZUp * (45.0 * U - 22.0 * (1.0 - std::exp(-U)));
				const Vec Round = (L0 * std::cos(A) - R0 * std::sin(A)).Unit();
				const double Nose = 1.05 * Settle;
				F.Fw = (Round * std::cos(Nose) - ZUp * std::sin(Nose)).Unit();
				F.Up = (ZUp * std::cos(Nose) + Round * std::sin(Nose)).Unit();
				const double Roll = 2.5 * A;
				const Vec Side = F.Fw.Cross(F.Up).Unit();
				F.Up = (F.Up * std::cos(Roll) + Side * std::sin(Roll)).Unit();
				F.Right = F.Fw.Cross(F.Up).Unit();
				return F;
			}
			// (burning: the nose drops into a steepening dive and the speed builds, rolling slowly)
			Vec At = F0.At;
			const double Dt = 1.0 / 120.0;
			double Th = 0.0, Sp = Speed;
			for (double Tau = 0.0; Tau < U; Tau += Dt)
			{
				Th = std::min(1.0, 0.22 * Tau);
				Sp = Speed + 9.0 * Tau;
				At = At + (L0 * std::cos(Th) - ZUp * std::sin(Th)) * (Sp * std::min(Dt, U - Tau));
			}
			F.At = At;
			F.Fw = (L0 * std::cos(Th) - ZUp * std::sin(Th)).Unit();
			F.Up = (ZUp * std::cos(Th) + L0 * std::sin(Th)).Unit();
			const double Roll = 0.35 * U;
			F.Up = (F.Up * std::cos(Roll) + F.Fw.Cross(F.Up).Unit() * std::sin(Roll)).Unit();
			F.Right = F.Fw.Cross(F.Up).Unit();
			return F;
		};
		const char* TrailIs = std::getenv("FX_TRAIL");
		const std::string TrailKind = TrailIs ? TrailIs : "smoke";
		TheaterFx::Dice Spots(11);
		// flak: "burst" is one burst beside the aircraft's path, watched from a fixed eye; "flak" is heavy guns firing
		// round the aircraft as it flies on, bursts every 0.9 s or so (ragged), more behind than ahead
		// "above" is one burst 60 m over the fields seen as the campaign camera sees it: FX_DIST metres off (75), looking
		// down FX_PITCH degrees (50; the campaign camera never looks flatter than 22), facing FX_YAW degrees (90: the
		// game's test sun, 42 degrees up toward 249, then stands behind the eye's left)
		const bool bAbove = ShotName == "above";
		const bool bFlak = ShotName == "flak" || ShotName == "burst" || bAbove;
		const Frame Start = Fly(0.15);
		const Vec StartLevel = Vec(Start.Fw.X, Start.Fw.Y, 0.0).Unit();
		const Vec StartRight = StartLevel.Cross(ZUp).Unit();
		const Vec BurstAt = bAbove ? Vec(0.0, 0.0, 60.0) : Start.At + StartLevel * 60.0 - StartRight * 25.0 + ZUp * 4.0;
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		Vec AirSun = Battle::View().Sun; // (FX_SUN=x,y,z: the way toward the sun)
		if (const char* SunIs = std::getenv("FX_SUN"))
		{
			double X = 0.0, Y = 0.0, Z = 1.0;
			if (std::sscanf(SunIs, "%lf,%lf,%lf", &X, &Y, &Z) == 3)
			{
				AirSun = Vec(X, Y, Z).Unit();
			}
		}
		double NextBurst = 0.2;
		double Shown = 0.0; // (the time being drawn: the wake shot frames the whole trail laid so far)
		auto Shoot = [&](const Frame& F)
		{
			const Vec Level = Vec(F.Fw.X, F.Fw.Y, 0.0).Unit();
			const Vec LevelRight = Level.Cross(ZUp).Unit();
			Camera Cam;
			Cam.W = W;
			Cam.H = H;
			if (bAbove)
			{
				const double Pitch = EnvNum("FX_PITCH", 50.0) * Pi / 180.0, Yaw = EnvNum("FX_YAW", 90.0) * Pi / 180.0;
				Cam.Forward = Vec(std::cos(Pitch) * std::cos(Yaw), std::cos(Pitch) * std::sin(Yaw), -std::sin(Pitch));
				Cam.Eye = BurstAt - Cam.Forward * EnvNum("FX_DIST", 75.0);
			}
			else if (ShotName == "burst")
			{
				Cam.Eye = BurstAt + StartRight * (std::getenv("FX_FAR") ? 160.0 : 95.0) - StartLevel * 10.0 + ZUp * 8.0;
				Cam.Forward = (BurstAt + StartLevel * (std::getenv("FX_FAR") ? 22.0 : 8.0) - Cam.Eye).Unit();
			}
			else if (ShotName == "flak")
			{
				// (from well off the beam, so the wake of greying bursts the aircraft leaves behind is in the picture)
				Cam.Eye = F.At - Level * 650.0 + LevelRight * 950.0 + ZUp * 120.0;
				Cam.Forward = (F.At - Level * 520.0 - Cam.Eye).Unit();
			}
			else if (ShotName == "side")
			{
				// (off the outside wing, the aircraft left of the middle and its streams running off to the right)
				Cam.Eye = F.At + LevelRight * 45.0 - Level * 8.0 + ZUp * 3.0;
				Cam.Forward = ((F.At - Cam.Eye).Unit() + Level * 0.4).Unit();
			}
			else if (ShotName == "close")
			{
				// (the close view: off and under the right wing, about 12 m from its guns (FX_CLOSE_OUT metres out, 14),
				// looking across at the chutes and the spray of cases and links streaming aft; the licks and lamp show within
				// 30 m, cases within 60 m)
				if (std::getenv("FX_CLOSE_AFT"))
				{
					// (FX_CLOSE_AFT=1: behind and under the right wing, FX_CLOSE_BACK metres aft (14), looking up the stream)
					Cam.Eye = F.At - F.Fw * EnvNum("FX_CLOSE_BACK", 14.0) + F.Right * 3.6 - F.Up * 3.5;
					Cam.Forward = (F.At + F.Right * 2.8 - F.Up * 1.2 - F.Fw * 2.0 - Cam.Eye).Unit();
				}
				else
				{
					Cam.Eye = F.At + F.Right * EnvNum("FX_CLOSE_OUT", 14.0) - F.Fw * 3.0 - F.Up * 4.0;
					Cam.Forward = (F.At + F.Right * 2.8 - F.Fw * 1.0 - F.Up * 1.5 - Cam.Eye).Unit();
				}
			}
			else if (ShotName == "far")
			{
				Cam.Eye = F.At + Level * 520.0 + LevelRight * 260.0 + ZUp * 40.0;
				Cam.Forward = (F.At + Level * 260.0 - Cam.Eye).Unit();
			}
			else if (bFall)
			{
				// (from off the beam of where the fall began, following the fall; an explosion's pieces carried on along
				// the path, slowing and sinking)
				const Frame F0 = Fly(FallFrom);
				const Vec L0 = Vec(F0.Fw.X, F0.Fw.Y, 0.0).Unit();
				const Vec R0 = L0.Cross(ZUp).Unit();
				const double U = std::max(0.0, Shown - FallFrom);
				const Vec Look = FallKind == "explode" ? F0.At + L0 * (Speed * 0.7 * (1.0 - std::exp(-U))) - ZUp * (10.0 * std::max(0.0, U - 1.0)) : F.At;
				const double Off = EnvNum("FX_DIST", 140.0);
				Cam.Eye = F0.At + R0 * Off + L0 * (0.6 * Off) + ZUp * (0.15 * Off);
				Cam.Forward = (Look - Cam.Eye).Unit();
			}
			else if (ShotName == "hit")
			{
				Cam.Eye = F.At + LevelRight * 26.0 - Level * 6.0 + ZUp * 7.0;
				Cam.Forward = ((F.At - Cam.Eye).Unit() - Level * 0.15).Unit();
			}
			else if (ShotName == "wake")
			{
				// (the whole trail laid so far, from far off the beam: FX_DIST metres, 2500)
				const Vec Mid = Fly(0.5 * Shown).At;
				Cam.Eye = Mid + StartRight * EnvNum("FX_DIST", 2500.0) + ZUp * 250.0;
				Cam.Forward = (Mid - Cam.Eye).Unit();
			}
			else if (ShotName == "map")
			{
				// (the strategy camera: FX_DIST metres off (1400), high above and abeam, the guns firing)
				const double Far = EnvNum("FX_DIST", 1400.0);
				Cam.Eye = F.At + LevelRight * (0.8 * Far) + ZUp * (0.55 * Far) - Level * (0.2 * Far);
				Cam.Forward = (F.At + Level * 150.0 - Cam.Eye).Unit();
			}
			else if (ShotName == "trail")
			{
				Cam.Eye = F.At - Level * 120.0 + LevelRight * 160.0 + ZUp * 60.0;
				Cam.Forward = (F.At - Level * 80.0 - Cam.Eye).Unit();
			}
			else if (ShotName == "target")
			{
				// (from just off the point the guns meet, looking back down the streams at the aircraft)
				Cam.Eye = F.At + Level * 140.0 + LevelRight * 9.0 + ZUp * 4.0;
				Cam.Forward = (F.At - Cam.Eye).Unit();
			}
			else
			{
				Cam.Eye = F.At - Level * 30.0 + ZUp * 9.0 + LevelRight * 4.0;
				Cam.Forward = (F.At + Level * 200.0 - Cam.Eye).Unit();
			}
			Cam.Right = Cam.Forward.Cross(ZUp).Unit();
			Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
			Cam.Ppr = W / (2.0 * std::tan(std::atan(18.0 / 35.0)));
			return Cam;
		};
		// (every step between the pictures is drawn too, as the game draws every frame: a flash's lamp and the shortest
		// flickers go by the frames drawn)
		auto DrawBetween = [&](int StepNo, int Until, double T, const Frame& F)
		{
			if (StepNo == Until)
			{
				return;
			}
			const Camera C = Shoot(F);
			Battle::View V;
			V.Eye = C.Eye;
			V.Right = C.Right;
			V.Up = C.Up;
			V.Forward = C.Forward;
			V.PixelsPerRadian = C.Ppr;
			V.Sun = AirSun;
			if (bDusk)
			{
				V.SmokeLight = Rgb(0.22f, 0.22f, 0.27f);
			}
			Fx.Draw(T, V);
		};
		std::sort(Frames.begin(), Frames.end());
		int Step = 0;
		Battle::View View;
		for (int FrameNo : Frames)
		{
			const int Until = FrameNo * 2; // (frames at 30 a second, calls at 60)
			for (; Step <= Until; ++Step)
			{
				const double T = Step / 60.0;
				Shown = T;
				const Frame F = Fall(T);
				if (bFall)
				{
					const Vec Vel = (Fall(T + 0.01).At - F.At) * 100.0;
					const Vec Engine = Place(F, Vec(EnvNum("FX_FIRE_X", 2.9), 0.75, -0.35));
					if (Step == 6 && FallKind != "explode")
					{
						Fx.TrailStart(T, 1, Engine, Vel, FallKind == "spin" ? "smoke" : "fire", EnvNum("FX_SPAN", 11.0));
					}
					if (Step >= 6 && FallKind != "explode")
					{
						Fx.TrailMove(T, 1, Engine, Vel);
					}
					if (Step == static_cast<int>(FallFrom * 60.0))
					{
						Fx.AirFall(T, 7, F.At, Vel, FallKind.c_str(), EnvNum("FX_SPAN", 11.0), Height - EnvNum("FX_DECK", 400.0));
					}
					else if (Step > static_cast<int>(FallFrom * 60.0))
					{
						Fx.AirFallMove(T, 7, F.At, Vel);
					}
					DrawBetween(Step, Until, T, F);
					continue;
				}
				if (bStruck)
				{
					// (struck: a hit every 0.07 s or so along the right wing and the nose; the engine trails from 0.3 s)
					if (T >= 0.2 && T <= 1.0 && Step % 4 == 0)
					{
						const Vec Spot(Spots.U(-1.0, 3.5), Spots.U(-0.5, 4.5), Spots.U(-0.2, 0.4));
						Fx.AirHit(T, Place(F, Spot), (F.Up + F.Right * 0.3).Unit(), F.Fw * Speed, Kind.c_str());
					}
					const Vec Engine = Place(F, Vec(EnvNum("FX_FIRE_X", 2.9), 0.75, -0.35)); // (an exhaust on the cowling's near side; FX_FIRE_X=1 a wing root)
					if (Step == 18)
					{
						Fx.TrailStart(T, 1, Engine, F.Fw * Speed, TrailKind.c_str(), EnvNum("FX_SPAN", 0.0)); // (FX_SPAN: the aircraft's span, metres)
					}
					if (Step >= 18)
					{
						Fx.TrailMove(T, 1, Engine, F.Fw * Speed);
					}
					DrawBetween(Step, Until, T, F);
					continue;
				}
				if (bFlak)
				{
					if (ShotName == "burst" || bAbove)
					{
						if (Step == 9)
						{
							Fx.AirFlak(T, BurstAt, "heavy");
						}
					}
					else
					{
						for (int Guard = 0; NextBurst <= T && Guard < 4; ++Guard)
						{
							Vec Where;
							do
							{
								Where = F.At + Level_(F) * Spots.U(-80.0, 30.0) + Right_(F) * (Spots.Bell() * 35.0) + ZUp * (Spots.Bell() * 14.0);
							} while ((Where - F.At).Length() < 18.0);
							Fx.AirFlak(T, Where, "heavy");
							NextBurst += 0.9 * Spots.U(0.6, 1.4);
						}
					}
					DrawBetween(Step, Until, T, F);
					continue;
				}
				for (size_t G = 0; G < Guns.size(); ++G)
				{
					const Vec Muzzle = Place(F, Guns[G]);
					const Vec Aim = F.At + F.Fw * Meet; // (each gun laid on one point ahead: they converge there)
					Fx.AirGun(T, static_cast<int>(G), Muzzle, Aim, F.Fw * Speed, Kind.c_str());
				}
				DrawBetween(Step, Until, T, F);
			}
			const double Now = FrameNo / 30.0;
			Shown = Now;
			const Frame F = Fall(Now);
			const Camera Cam = Shoot(F);
			View.Eye = Cam.Eye;
			View.Right = Cam.Right;
			View.Up = Cam.Up;
			View.Forward = Cam.Forward;
			View.PixelsPerRadian = Cam.Ppr;
			View.Sun = AirSun;
			if (bDusk)
			{
				View.SmokeLight = Rgb(0.22f, 0.22f, 0.27f);
			}
			const int N = Fx.Draw(Now, View);
			Canvas C;
			C.Init(W, H);
			Battle::Lamp AirLamps[2];
			const int NAirLamps = bAbove ? Fx.Lamps(Now, AirLamps, 2, Fx.DayOf(View.SmokeLight)) : 0; // (over the fields: the flash's lamp on the ground)
			Paint(C, Cam, AirLamps, NAirLamps, {}, false);
			// the aircraft: a plain olive-drab shape, lit by the sun, so the flash and streams are judged on a wing
			if (!(bFall && FallKind == "explode" && Now > FallFrom))
			{
				const Vec Sun = View.Sun.Unit();
				const double Base[3] = { ToLinear(92), ToLinear(98), ToLinear(74) };
				auto Face = [&](const Vec& A, const Vec& B, const Vec& Cc)
				{
					const Vec Pa = Place(F, A), Pb = Place(F, B), Pc = Place(F, Cc);
					const Vec Nn = (Pb - Pa).Cross(Pc - Pa).Unit();
					const double Lit = 0.45 + 0.55 * std::fabs(Nn.Dot(Sun));
					const double Col[3] = { Base[0] * Lit, Base[1] * Lit, Base[2] * Lit };
					const Vec Tri[3] = { Pa, Pb, Pc };
					Solid(C, Cam, Tri, Col);
				};
				auto Quad = [&](const Vec& A, const Vec& B, const Vec& Cc, const Vec& D)
				{
					Face(A, B, Cc);
					Face(A, Cc, D);
				};
				// fuselage: three sections, nose to tail (forward, half width, half height)
				const double Sx[3] = { 4.5, 1.5, -5.0 }, Hw[3] = { 0.45, 0.62, 0.16 }, Hh[3] = { 0.5, 0.78, 0.3 };
				for (int K = 0; K < 2; ++K)
				{
					auto Ring = [&](int I, double Ay, double Az) { return Vec(Sx[I], Hw[I] * Ay, Hh[I] * Az); };
					const double Ys[5] = { 1, -1, -1, 1, 1 }, Zs[5] = { 1, 1, -1, -1, 1 };
					for (int E = 0; E < 4; ++E)
					{
						Quad(Ring(K, Ys[E], Zs[E]), Ring(K, Ys[E + 1], Zs[E + 1]), Ring(K + 1, Ys[E + 1], Zs[E + 1]), Ring(K + 1, Ys[E], Zs[E]));
					}
				}
				// wings, tailplane, fin
				for (double S : { 1.0, -1.0 })
				{
					Quad(Vec(1.35, 0.6 * S, -0.32), Vec(0.95, 5.6 * S, -0.1), Vec(-0.3, 5.6 * S, -0.1), Vec(-0.95, 0.6 * S, -0.32));
					Quad(Vec(-4.1, 0.15 * S, 0.05), Vec(-4.4, 2.1 * S, 0.1), Vec(-5.0, 2.1 * S, 0.1), Vec(-5.1, 0.15 * S, 0.05));
				}
				Quad(Vec(-3.8, 0.0, 0.3), Vec(-4.6, 0.0, 1.9), Vec(-5.2, 0.0, 1.9), Vec(-5.1, 0.0, 0.3));
			}
			if (bDusk)
			{
				for (size_t I = 0; I < C.R.size(); ++I)
				{
					C.R[I] *= 0.16;
					C.G[I] *= 0.16;
					C.B[I] *= 0.2;
				}
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/air-%s-%s%s%s-f%03d.ppm", OutDir.c_str(), bFlak ? "flak" : Kind.c_str(), ShotName.c_str(), bStruck ? ("-" + TrailKind).c_str() : (bFall ? ("-" + FallKind).c_str() : ""), bDusk ? "-dusk" : "", FrameNo);
			C.Save(Name);
			std::printf("frame %d: %d puffs\n", FrameNo, N);
		}
		return 0;
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
	if (Size == "oil")
	{
		return Oil(Book, OutDir, Ground, Frames, W, H);
	}
	if (Size == "air")
	{
		return Air(Book, OutDir, Ground, Frames, W, H, Argc > 7 ? Argv[7] : "chase");
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
	const Vec MgGun = Vec(-28.0, 12.0, Swell(nullptr, -28.0, 12.0) + 0.6); // (the "mg" shot's gun)
	if (const char* MgView = std::getenv("FX_MG_VIEW"); Size == "mg" && MgView)
	{
		// (FX_MG_VIEW=side: square to the line of fire, 30 m off it; =behind: 10 m behind the gun and 3 m up, along it)
		const Vec Fire = (Hit - MgGun).Unit();
		const Vec Mid = (MgGun + Hit) * 0.5;
		const Vec Square = Fire.Cross(Vec(0, 0, 1)).Unit();
		Cam.Eye = std::string(MgView) == "behind" ? MgGun - Fire * 10.0 + Vec(0, 0, 3.0) : Mid - Square * 30.0 + Vec(0, 0, 6.0);
		Cam.Forward = ((std::string(MgView) == "behind" ? Hit : Mid) - Cam.Eye).Unit();
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
	}
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
	if (const char* LightIs = std::getenv("FX_SUNLIGHT")) // (FX_SUNLIGHT=r,g,b: the sun's own light, linear; white by default)
	{
		float R = 1.0f, G = 1.0f, B = 1.0f;
		if (std::sscanf(LightIs, "%f,%f,%f", &R, &G, &B) == 3)
		{
			View.SunLight = Rgb(R, G, B);
		}
	}
	Fx.Draw(-0.1, View); // (the eye is known before the hit, as in the game)
	if (Size == "mg")
	{
		// (a machine gun's burst from 30 m off to the left into the ground by the hit: close tracer and strikes)
		const Vec Gun = MgGun;
		Fx.Muzzle(-0.02, Gun, (Hit - Gun).Unit(), "mg");
		Fx.Shots(-0.02, 13, Gun, Hit + Vec(0.0, 0.0, 0.1), "mg", 25, 15.0, Ground.c_str());
	}
	else if (!std::getenv("FX_NOHIT")) // (FX_NOHIT: the same shot with no hit, the ground alone, to measure a burst against)
	{
		Fx.Shell(-0.02, Hit, Size.c_str(), Ground.c_str(), false);
	}
	std::sort(Frames.begin(), Frames.end());
	int Drawn = 0;
	for (int Frame : Frames)
	{
		// (every frame before the picture is drawn too, as the game draws every frame: a flash's lamp goes by frames)
		for (; Drawn < Frame; ++Drawn)
		{
			Fx.Draw(Drawn / 30.0, View);
		}
		Drawn = Frame + 1;
		const double Now = Frame / 30.0;
		const Battle::Jolt J = Fx.Shake(Now, Cam.Eye);
		Camera Shot = Cam;
		Shot.Eye = Cam.Eye + J.Offset;
		View.Eye = Shot.Eye;
		const int N = Fx.Draw(Now, View);
		Battle::Lamp Lamps[2];
		const int NL = Fx.Lamps(Now, Lamps, 2, Fx.DayOf(View.SmokeLight)); // (a plain day: the lamp at its day share)
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
