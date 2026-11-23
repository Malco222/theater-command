// Crater variety's picture and checks (release 2, LandCrater::Field::bVariety; Brian, 23 November: "make the explosion
// craters different varations so its not always the same"). Plain C++, no engine: lays a field of shell holes on a
// gently rolling patch of ground the way the scene does (Field::Add, the ground cut over the dug bowls), with the
// switch off and on, and paints them seen from a man's height a little way off, lit by a low sun (a small software
// painter, PPM). Also checks, with the switch on, that each hole is its own, that the long holes lie along the way their
// rounds came, and that with the switch off the holes are what they were.
//
// Usage: crater_sheet <folder> [ground word: earth, sand, snow, mud, blackearth, woods] (Tools/land_crater/run.sh)
#include "Battle/LandCrater.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
	constexpr double Pi = 3.14159265358979323846;

	double GroundZ(double X, double Y) { return 0.35 * std::sin(X * 0.045) * std::cos(Y * 0.038) + 0.12 * std::sin(X * 0.17 + Y * 0.11); }

	// (the ground's colours as the game's are: its points some 4.2 m apart, so its colour changes over metres, not
	// centimetres; the fine grain is the material's, laid alike over the ground and the holes: Grain. A fine pattern in
	// the ground's own colours showed every hole's thrown soil as a smooth patch with straight sides, which the game,
	// its holes on the ground's own material, never draws)
	LandField::Rgb8 GroundColour(const std::string& Word, double X, double Y)
	{
		const double N = 0.5 + 0.5 * std::sin(X * 0.31 + std::sin(Y * 0.23) * 1.5) * std::cos(Y * 0.27 - X * 0.09);
		const auto C = [N](int R, int G, int B, double By)
		{
			LandField::Rgb8 O;
			O.R = static_cast<uint8_t>(std::clamp(R * (1.0 + By * (N - 0.5)), 0.0, 255.0));
			O.G = static_cast<uint8_t>(std::clamp(G * (1.0 + By * (N - 0.5)), 0.0, 255.0));
			O.B = static_cast<uint8_t>(std::clamp(B * (1.0 + By * (N - 0.5)), 0.0, 255.0));
			return O;
		};
		if (Word == "sand") return C(192, 168, 124, 0.12);
		if (Word == "snow") return C(226, 230, 236, 0.05);
		if (Word == "mud") return C(88, 76, 56, 0.15);
		if (Word == "blackearth") return C(150, 140, 86, 0.15);
		if (Word == "woods") return C(70, 78, 44, 0.2);
		return C(92, 112, 52, 0.2); // (July pasture)
	}

	struct Picture
	{
		int W = 1200, H = 640;
		std::vector<float> Rgb, Depth;
		Picture(int InW, int InH) : W(InW), H(InH), Rgb(static_cast<size_t>(InW * InH * 3), 0.0f), Depth(static_cast<size_t>(InW * InH), 1e30f) {}
	};

	double Lin(double C) { C /= 255.0; return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4); }
	double Srgb(double L) { L = std::clamp(L, 0.0, 1.0); return 255.0 * (L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(L, 1.0 / 2.4) - 0.055); }

	/** The camera: at Eye, looking toward At; a pinhole of Fov degrees across. */
	struct Camera
	{
		double Ex, Ey, Ez, Fx, Fy, Fz, Rx, Ry, Rz, Ux, Uy, Uz, Focal;
		Camera(double EyeX, double EyeY, double EyeZ, double AtX, double AtY, double AtZ, double Fov, int W)
			: Ex(EyeX), Ey(EyeY), Ez(EyeZ)
		{
			Fx = AtX - EyeX, Fy = AtY - EyeY, Fz = AtZ - EyeZ;
			double L = std::sqrt(Fx * Fx + Fy * Fy + Fz * Fz);
			Fx /= L, Fy /= L, Fz /= L;
			Rx = Fy, Ry = -Fx, Rz = 0.0;
			L = std::sqrt(Rx * Rx + Ry * Ry);
			Rx /= L, Ry /= L;
			Ux = Ry * Fz - Rz * Fy, Uy = Rz * Fx - Rx * Fz, Uz = Rx * Fy - Ry * Fx;
			Focal = 0.5 * W / std::tan(0.5 * Fov * Pi / 180.0);
		}
		bool Project(double X, double Y, double Z, int W, int H, double& Sx, double& Sy, double& D) const
		{
			const double Dx = X - Ex, Dy = Y - Ey, Dz = Z - Ez;
			D = Dx * Fx + Dy * Fy + Dz * Fz;
			if (D < 0.1) return false;
			Sx = 0.5 * W + Focal * (Dx * Rx + Dy * Ry + Dz * Rz) / D;
			Sy = 0.5 * H - Focal * (Dx * Ux + Dy * Uy + Dz * Uz) / D;
			return true;
		}
	};

	/** M_LandSurface's grain (Tools/land_look's T_LandGrain, about 2 m a repeat), over every face alike: a share to
	 * scale its colour by. */
	double Grain(double X, double Y)
	{
		const double A = std::sin(X * 3.1 + Y * 1.7) * std::cos(Y * 2.9 - X * 0.8);
		const double B = std::sin(X * 7.3 - Y * 5.1 + std::sin(Y * 4.0)) * std::cos(X * 6.1 + Y * 8.3);
		const double C = std::sin(X * 17.0 + Y * 11.0) * std::sin(Y * 15.0 - X * 9.0);
		return 1.0 + 0.07 * A + 0.06 * B + 0.05 * C;
	}

	void Paint(Picture& P, const Camera& Cam, const LandField::Mesh& M)
	{
		// (a low morning sun from the south-east, a sky's light from above)
		const double Sx0 = 0.55, Sy0 = -0.62, Sz0 = 0.56;
		const double SL = std::sqrt(Sx0 * Sx0 + Sy0 * Sy0 + Sz0 * Sz0);
		const double Lx = Sx0 / SL, Ly = Sy0 / SL, Lz = Sz0 / SL;
		const size_t Np = M.Colours.size();
		std::vector<double> Sx(Np), Sy(Np), Dp(Np), Lit(Np * 3);
		std::vector<char> Ok(Np);
		for (size_t I = 0; I < Np; ++I)
		{
			Ok[I] = Cam.Project(M.Positions[I * 3], M.Positions[I * 3 + 1], M.Positions[I * 3 + 2], P.W, P.H, Sx[I], Sy[I], Dp[I]) ? 1 : 0;
			const double Nx = M.Normals[I * 3], Ny = M.Normals[I * 3 + 1], Nz = M.Normals[I * 3 + 2];
			const double Sun = std::max(0.0, Nx * Lx + Ny * Ly + Nz * Lz);
			const double Sky = 0.5 + 0.5 * Nz;
			const double K = 1.25 * Sun + 0.42 * Sky;
			Lit[I * 3] = Lin(M.Colours[I].R) * K * 1.02;
			Lit[I * 3 + 1] = Lin(M.Colours[I].G) * K;
			Lit[I * 3 + 2] = Lin(M.Colours[I].B) * K * 0.97;
		}
		for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
		{
			const size_t A = M.Triangles[T], B = M.Triangles[T + 1], C = M.Triangles[T + 2];
			if (!Ok[A] || !Ok[B] || !Ok[C]) continue;
			const double X0 = std::floor(std::min({ Sx[A], Sx[B], Sx[C] })), X1 = std::ceil(std::max({ Sx[A], Sx[B], Sx[C] }));
			const double Y0 = std::floor(std::min({ Sy[A], Sy[B], Sy[C] })), Y1 = std::ceil(std::max({ Sy[A], Sy[B], Sy[C] }));
			const double Den = (Sx[B] - Sx[A]) * (Sy[C] - Sy[A]) - (Sx[C] - Sx[A]) * (Sy[B] - Sy[A]);
			if (std::fabs(Den) < 1e-9) continue;
			for (int Y = std::max(0, static_cast<int>(Y0)); Y <= std::min(P.H - 1, static_cast<int>(Y1)); ++Y)
			{
				for (int X = std::max(0, static_cast<int>(X0)); X <= std::min(P.W - 1, static_cast<int>(X1)); ++X)
				{
					const double Px = X + 0.5, Py = Y + 0.5;
					const double U = ((Px - Sx[A]) * (Sy[C] - Sy[A]) - (Sx[C] - Sx[A]) * (Py - Sy[A])) / Den;
					const double V = ((Sx[B] - Sx[A]) * (Py - Sy[A]) - (Px - Sx[A]) * (Sy[B] - Sy[A])) / Den;
					if (U < -1e-6 || V < -1e-6 || U + V > 1.0 + 1e-6) continue;
					const double W0 = 1.0 - U - V;
					const double Dz = W0 * Dp[A] + U * Dp[B] + V * Dp[C];
					const size_t Pi_ = static_cast<size_t>(Y * P.W + X);
					if (Dz >= P.Depth[Pi_] - 1e-4) continue;
					P.Depth[Pi_] = static_cast<float>(Dz);
					const double Wx = W0 * M.Positions[A * 3] + U * M.Positions[B * 3] + V * M.Positions[C * 3];
					const double Wy = W0 * M.Positions[A * 3 + 1] + U * M.Positions[B * 3 + 1] + V * M.Positions[C * 3 + 1];
					const double G = Grain(Wx, Wy);
					for (int Ch = 0; Ch < 3; ++Ch)
					{
						P.Rgb[Pi_ * 3 + static_cast<size_t>(Ch)] = static_cast<float>(G * (W0 * Lit[A * 3 + static_cast<size_t>(Ch)] + U * Lit[B * 3 + static_cast<size_t>(Ch)] + V * Lit[C * 3 + static_cast<size_t>(Ch)]));
					}
				}
			}
		}
	}

	bool Write(const Picture& P, const std::string& Path, double HazeAt)
	{
		FILE* F = std::fopen(Path.c_str(), "wb");
		if (!F) return false;
		std::fprintf(F, "P6\n%d %d\n255\n", P.W, P.H);
		std::vector<unsigned char> Row(static_cast<size_t>(P.W * 3));
		for (int Y = 0; Y < P.H; ++Y)
		{
			for (int X = 0; X < P.W; ++X)
			{
				const size_t I = static_cast<size_t>(Y * P.W + X);
				const bool bSky = P.Depth[I] > 1e29f;
				for (int Ch = 0; Ch < 3; ++Ch)
				{
					const double Sky[3] = { 0.62, 0.7, 0.8 };
					// (the distance's haze)
					const double Haze = bSky ? 1.0 : 1.0 - std::exp(-P.Depth[I] / HazeAt);
					const double L = (1.0 - Haze) * P.Rgb[I * 3 + static_cast<size_t>(Ch)] + Haze * Sky[Ch];
					Row[static_cast<size_t>(X * 3 + Ch)] = static_cast<unsigned char>(std::lround(Srgb(L)));
				}
			}
			std::fwrite(Row.data(), 1, Row.size(), F);
		}
		std::fclose(F);
		return true;
	}

	struct Laid
	{
		LandCrater::Field Holes;
		LandField::Mesh Ground;
	};

	/** The ground, 0.2 m squares, X0..X1 by Y0..Y1, in its own colours. */
	LandField::Mesh MakeGround(const std::string& Word, double X0, double X1, double Y0, double Y1)
	{
		LandField::Mesh M;
		const double Step = 0.2;
		const int Nx = static_cast<int>((X1 - X0) / Step) + 1, Ny = static_cast<int>((Y1 - Y0) / Step) + 1;
		for (int J = 0; J < Ny; ++J)
		{
			for (int I = 0; I < Nx; ++I)
			{
				const double X = X0 + I * Step, Y = Y0 + J * Step;
				M.Positions.insert(M.Positions.end(), { static_cast<float>(X), static_cast<float>(Y), static_cast<float>(GroundZ(X, Y)) });
				const double E = 0.05;
				double Nx_ = -(GroundZ(X + E, Y) - GroundZ(X - E, Y)) / (2 * E), Ny_ = -(GroundZ(X, Y + E) - GroundZ(X, Y - E)) / (2 * E), Nz = 1.0;
				const double L = std::sqrt(Nx_ * Nx_ + Ny_ * Ny_ + Nz * Nz);
				M.Normals.insert(M.Normals.end(), { static_cast<float>(Nx_ / L), static_cast<float>(Ny_ / L), static_cast<float>(Nz / L) });
				M.Colours.push_back(GroundColour(Word, X, Y));
			}
		}
		for (int J = 0; J + 1 < Ny; ++J)
		{
			for (int I = 0; I + 1 < Nx; ++I)
			{
				const uint32_t A = static_cast<uint32_t>(J * Nx + I), B = A + 1, C = A + static_cast<uint32_t>(Nx), D = C + 1;
				M.Triangles.insert(M.Triangles.end(), { A, B, D, A, D, C });
			}
		}
		return M;
	}

	/** A barrage's holes on Word's ground: the rows mortar bombs, a tank's high explosive, 105s and 150s, each round
	 * coming from the south-west (the guns' side), a little either way. */
	void Lay(Laid& Out, const std::string& Word, bool bVariety)
	{
		Out.Holes = LandCrater::Field();
		Out.Holes.bVariety = bVariety;
		Out.Holes.Begin(GroundZ, 4242u, [Word](double X, double Y) { return GroundColour(Word, X, Y); });
		const char* Rows[] = { "mortar", "gun75", "howitzer", "heavy" };
		for (int R = 0; R < 4; ++R)
		{
			for (int K = 0; K < 6; ++K)
			{
				const double X = -25.0 + K * 10.0 + (R % 2) * 3.0, Y = -12.0 + R * 9.0;
				const double Way = Pi / 4.0 + 0.25 * std::sin(K * 1.7 + R);
				Out.Holes.Add(X, Y, Rows[R], 0.0, Word, std::cos(Way), std::sin(Way));
			}
		}
		// (two holes torn into each other, as a barrage lays them)
		Out.Holes.Add(24.0, 20.0, "howitzer", 0.0, Word, 0.7, 0.7);
		Out.Holes.Add(25.6, 20.9, "howitzer", 0.0, Word, 0.7, 0.7);
		Out.Ground = MakeGround(Word, -40.0, 40.0, -30.0, 40.0);
		for (const std::vector<double>& Cut : Out.Holes.TakeCuts())
		{
			LandCrater::CutOut(Out.Ground, Cut);
		}
	}

	void PaintLaid(const Laid& L, const std::string& Path, double EyeX, double EyeY, double EyeZ, double AtX, double AtY, double Fov = 62.0,
		double HazeAt = 260.0)
	{
		Picture P(1200, 640);
		const Camera Cam(EyeX, EyeY, EyeZ, AtX, AtY, 0.0, Fov, P.W);
		Paint(P, Cam, L.Ground);
		for (int K = 0; K < L.Holes.Patches(); ++K)
		{
			for (const LandField::Mesh& M : L.Holes.PatchAt(K).Sections) Paint(P, Cam, M);
		}
		if (!Write(P, Path, HazeAt)) std::fprintf(stderr, "could not write %s\n", Path.c_str());
	}

	int Failures = 0;
	void Check(bool bOk, const char* What, const std::string& Detail)
	{
		std::printf("  %s  %s (%s)\n", bOk ? "ok  " : "FAIL", What, Detail.c_str());
		if (!bOk) ++Failures;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: crater_sheet <folder> [ground word]\n");
		return 2;
	}
	const std::string Dir = Argv[1];
	const std::string Word = Argc > 2 ? Argv[2] : "earth";
	Laid Off, On;
	Lay(Off, Word, false);
	Lay(On, Word, true);

	// Each its own: no two of a row's holes the same size and length; the long ones along their rounds' way.
	{
		int Rows = 0, Spread = 0, Along = 0, Long = 0;
		double MostLong = 1.0;
		for (int R = 0; R < 4; ++R)
		{
			double Lo = 1e9, Hi = 0.0;
			for (int K = 0; K < 6; ++K)
			{
				const LandCrater::Field::Hole& H = On.Holes.All()[static_cast<size_t>(R * 6 + K)];
				Lo = std::min(Lo, H.R);
				Hi = std::max(Hi, H.R);
				MostLong = std::max(MostLong, H.Long);
				if (H.Long > 1.25)
				{
					++Long;
					const double Way = Pi / 4.0 + 0.25 * std::sin(K * 1.7 + R);
					const double Off_ = std::fabs(std::remainder(H.Heading - Way, 2.0 * Pi));
					if (Off_ < 0.2) ++Along;
				}
			}
			++Rows;
			if (Hi > 1.12 * Lo) ++Spread;
		}
		char Buf[200];
		std::snprintf(Buf, sizeof Buf, "%d of %d rows spread over 12%%; %d long holes, %d along their rounds' way; longest %.2f", Spread, Rows, Long, Along, MostLong);
		Check(Spread == Rows && Long >= 4 && Along == Long, "each hole its own size; the long ones lie along the way their rounds came", Buf);
		int OffSame = 0;
		for (const LandCrater::Field::Hole& H : Off.Holes.All()) OffSame += H.Long == 1.0 ? 1 : 0;
		std::snprintf(Buf, sizeof Buf, "%d of %zu round", OffSame, Off.Holes.All().size());
		Check(static_cast<size_t>(OffSame) == Off.Holes.All().size(), "with the switch off every hole is round, as before", Buf);
	}

	// A body lies on each hole's face, the switch on: wherever a hole's triangle stands, TopAt is at least that high
	// (but a flooded hole's water, which a man wades in: Size::Puddle).
	{
		long Points = 0, Missed = 0, Under = 0, Water = 0;
		const auto IsWater = [](const LandField::Rgb8& C)
		{
			return (C.R == LandCrater::PuddleWater.R && C.G == LandCrater::PuddleWater.G && C.B == LandCrater::PuddleWater.B)
				|| (C.R == LandCrater::PuddleShore.R && C.G == LandCrater::PuddleShore.G && C.B == LandCrater::PuddleShore.B);
		};
		for (int P = 0; P < On.Holes.Patches(); ++P)
		{
			for (const LandField::Mesh& M : On.Holes.PatchAt(P).Sections)
			{
				for (size_t Tr = 0; Tr + 2 < M.Triangles.size(); Tr += 3)
				{
					const float* A = &M.Positions[M.Triangles[Tr] * 3];
					const float* B = &M.Positions[M.Triangles[Tr + 1] * 3];
					const float* C = &M.Positions[M.Triangles[Tr + 2] * 3];
					if (IsWater(M.Colours[M.Triangles[Tr]]) && IsWater(M.Colours[M.Triangles[Tr + 1]]) && IsWater(M.Colours[M.Triangles[Tr + 2]]))
					{
						++Water;
						continue;
					}
					const double Px = (A[0] + B[0] + C[0]) / 3.0, Py = (A[1] + B[1] + C[1]) / 3.0, Pz = (A[2] + B[2] + C[2]) / 3.0;
					++Points;
					double Top = 0.0;
					if (!On.Holes.TopAt(Px, Py, Top)) { ++Missed; continue; }
					// (a centimetre's grace: where a hole's thrown soil steps down into an older bowl, its steep few-centimetre
					// triangles can read a few millimetres under at their middles; a body laid there sinks no deeper)
					if (Top < Pz - 0.01)
					{
						++Under;
						std::printf("  under: at %.3f %.3f, triangle %.3f, face %.3f; corners (%.3f %.3f %.3f) (%.3f %.3f %.3f) (%.3f %.3f %.3f)\n", Px, Py, Pz, Top, A[0], A[1], A[2], B[0], B[1], B[2], C[0], C[1], C[2]);
					}
				}
			}
		}
		char Buf[200];
		std::snprintf(Buf, sizeof Buf, "%ld triangles' middles, %ld with no face, %ld under it; %ld of flooded holes' water", Points, Missed, Under, Water);
		Check(Missed == 0 && Under == 0, "the switch on, a body still lies on every hole's face", Buf);
	}

	// (from a man's height near the mortar bombs' and the tank rounds' rows, the 105s and 150s beyond; and from above)
	if (std::getenv("CRATER_OUT"))
	{
		for (const Laid* L : { &Off, &On })
		{
			std::printf("  switch %s, reach (Out/R) by hole:", L == &Off ? "off" : "on");
			for (const LandCrater::Field::Hole& H : L->Holes.All()) std::printf(" %.1f/%.1f", H.Out, H.R);
			std::printf("\n");
		}
	}
	// No hole through the world: wherever a dug hole's cut took the ground away, a hole's face is there (both ways).
	for (const Laid* L : { &Off, &On })
	{
		long Inside = 0, Bare = 0;
		double WorstX = 0.0, WorstY = 0.0;
		for (const LandCrater::Field::Hole& H : L->Holes.All())
		{
			if (H.Cut.empty()) continue;
			for (double Y = H.Y - 4.013; Y <= H.Y + 4.0; Y += 0.05)
			{
				for (double X = H.X - 4.017; X <= H.X + 4.0; X += 0.05)
				{
					if (!LandCrater::Inside(H.Cut, X, Y)) continue;
					++Inside;
					double Top = 0.0;
					if (!L->Holes.TopAt(X, Y, Top)) { ++Bare; WorstX = X; WorstY = Y; }
				}
			}
		}
		char Buf[200];
		std::snprintf(Buf, sizeof Buf, "switch %s: %ld points inside the cuts, %ld with no face (last at %.2f, %.2f)", L == &Off ? "off" : "on", Inside, Bare, WorstX, WorstY);
		Check(Bare == 0, "no hole through the world: a face wherever the ground is cut", Buf);
	}

	PaintLaid(Off, Dir + "/craters-" + Word + "-off.ppm", -21.0, -20.0, 2.2, -9.0, -6.0, 66.0);
	PaintLaid(On, Dir + "/craters-" + Word + "-on.ppm", -21.0, -20.0, 2.2, -9.0, -6.0, 66.0);
	PaintLaid(Off, Dir + "/craters-" + Word + "-off-above.ppm", 0.0, 0.5, 60.0, 0.0, 2.0, 62.0, 1e9);
	PaintLaid(On, Dir + "/craters-" + Word + "-on-above.ppm", 0.0, 0.5, 60.0, 0.0, 2.0, 62.0, 1e9);
	if (std::getenv("CRATER_ZOOM"))
	{
		double Zx = 0.0, Zy = 0.0;
		std::sscanf(std::getenv("CRATER_ZOOM"), "%lf %lf", &Zx, &Zy);
		PaintLaid(On, Dir + "/zoom-" + Word + ".ppm", Zx, Zy - 0.3, 9.0, Zx, Zy, 50.0, 1e9);
	}
	std::printf("%s\n", Failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failures ? 1 : 0;
}
