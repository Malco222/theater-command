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

	// Scorched ground (release 2, the "scorch" look, Field::AddScorch): a burnt-out tank's ground, laid with no fire
	// (as the game is with the look off), as the fire takes hold (her grass browned), and once she has burnt a while
	// (her soot core over it); a 105's hole laid before she burnt, a mortar bomb's after, torn into her scorch.
	struct Wreck
	{
		double X = 0.0, Y = 0.0, Ax = 0.8, Ay = 0.6, Long = 6.3, Wide = 3.6;
	};
	const double WindX = 2.6, WindY = 1.2; // (the game's wind when the recording has none: LandBattle's)

	void LayWreck(Laid& Out, const std::string& Word, const Wreck& W, int Stage)
	{
		Out.Holes = LandCrater::Field();
		Out.Holes.bVariety = true;
		Out.Holes.Begin(GroundZ, 777u, [Word](double X, double Y) { return GroundColour(Word, X, Y); });
		Out.Holes.Add(9.0, -5.0, "howitzer", 0.0, Word, 0.7, 0.7);
		if (Stage >= 1) Out.Holes.AddScorch(0, W.X, W.Y, W.Ax, W.Ay, W.Long, W.Wide, WindX, WindY, 0.35, Word);
		if (Stage >= 2) Out.Holes.AddScorch(0, W.X, W.Y, W.Ax, W.Ay, W.Long, W.Wide, WindX, WindY, 1.0, Word);
		Out.Holes.Add(-5.0, 4.0, "mortar", 0.0, Word, -0.7, 0.7);
		Out.Ground = MakeGround(Word, -24.0, 24.0, -20.0, 24.0);
		for (const std::vector<double>& Cut : Out.Holes.TakeCuts())
		{
			LandCrater::CutOut(Out.Ground, Cut);
		}
	}

	/** A box, each face its own points (lit flat): along A, Ay her way, HalfL by HalfW, Z0 to Z0 + High. */
	void AddBox(LandField::Mesh& M, double Cx, double Cy, double Z0, double Ax, double Ay, double HalfL, double HalfW, double High, LandField::Rgb8 C)
	{
		const double Lx = -Ay, Ly = Ax;
		double P[8][3];
		for (int K = 0; K < 8; ++K)
		{
			const double U = (K & 1) ? HalfL : -HalfL, V = (K & 2) ? HalfW : -HalfW;
			P[K][0] = Cx + Ax * U + Lx * V;
			P[K][1] = Cy + Ay * U + Ly * V;
			P[K][2] = (K & 4) ? Z0 + High : Z0;
		}
		const int Faces[6][4] = { { 4, 5, 7, 6 }, { 1, 3, 7, 5 }, { 0, 4, 6, 2 }, { 2, 6, 7, 3 }, { 0, 1, 5, 4 }, { 0, 2, 3, 1 } };
		for (const auto& F : Faces)
		{
			const double* A = P[F[0]];
			const double* B = P[F[1]];
			const double* Cc = P[F[2]];
			double Nx = (B[1] - A[1]) * (Cc[2] - A[2]) - (B[2] - A[2]) * (Cc[1] - A[1]);
			double Ny = (B[2] - A[2]) * (Cc[0] - A[0]) - (B[0] - A[0]) * (Cc[2] - A[2]);
			double Nz = (B[0] - A[0]) * (Cc[1] - A[1]) - (B[1] - A[1]) * (Cc[0] - A[0]);
			const double L = std::sqrt(Nx * Nx + Ny * Ny + Nz * Nz);
			Nx /= L, Ny /= L, Nz /= L;
			const uint32_t First = static_cast<uint32_t>(M.Colours.size());
			for (int K = 0; K < 4; ++K)
			{
				M.Positions.insert(M.Positions.end(), { static_cast<float>(P[F[K]][0]), static_cast<float>(P[F[K]][1]), static_cast<float>(P[F[K]][2]) });
				M.Normals.insert(M.Normals.end(), { static_cast<float>(Nx), static_cast<float>(Ny), static_cast<float>(Nz) });
				M.Colours.push_back(C);
			}
			M.Triangles.insert(M.Triangles.end(), { First, First + 1, First + 2, First, First + 2, First + 3 });
		}
	}

	Picture PaintWreck(const Laid& L, const Camera& Cam, const Wreck* Hull)
	{
		Picture P(1200, 640);
		Paint(P, Cam, L.Ground);
		for (int K = 0; K < L.Holes.Patches(); ++K)
		{
			for (const LandField::Mesh& M : L.Holes.PatchAt(K).Sections) Paint(P, Cam, M);
		}
		if (Hull)
		{
			// (a stand-in for her burnt-out hull and turret: the soot black-brown a wreck goes to, LandKit's BurntColour)
			LandField::Mesh Box;
			const LandField::Rgb8 Burnt = { 38, 33, 29 };
			const double Z = GroundZ(Hull->X, Hull->Y);
			AddBox(Box, Hull->X, Hull->Y, Z, Hull->Ax, Hull->Ay, 0.5 * Hull->Long, 0.5 * Hull->Wide, 1.3, Burnt);
			AddBox(Box, Hull->X - 0.4 * Hull->Ax, Hull->Y - 0.4 * Hull->Ay, Z + 1.3, Hull->Ax, Hull->Ay, 1.3, 1.15, 0.8, Burnt);
			Paint(P, Cam, Box);
		}
		return P;
	}

	/** The picture's luma (Rec. 709 on its sRGB) round where a field point is drawn, 3 by 3 pixels; -1 off the picture. */
	double LumaAt(const Picture& P, const Camera& Cam, double X, double Y, double Z)
	{
		double Sx = 0.0, Sy = 0.0, D = 0.0;
		if (!Cam.Project(X, Y, Z, P.W, P.H, Sx, Sy, D)) return -1.0;
		const int Cx = static_cast<int>(Sx), Cy = static_cast<int>(Sy);
		if (Cx < 1 || Cy < 1 || Cx >= P.W - 1 || Cy >= P.H - 1) return -1.0;
		double Sum = 0.0;
		for (int J = -1; J <= 1; ++J)
		{
			for (int I = -1; I <= 1; ++I)
			{
				const size_t K = static_cast<size_t>((Cy + J) * P.W + Cx + I);
				Sum += 0.2126 * Srgb(P.Rgb[K * 3]) + 0.7152 * Srgb(P.Rgb[K * 3 + 1]) + 0.0722 * Srgb(P.Rgb[K * 3 + 2]);
			}
		}
		return Sum / 9.0;
	}

	double Median(std::vector<double> V)
	{
		if (V.empty()) return -1.0;
		std::nth_element(V.begin(), V.begin() + static_cast<std::ptrdiff_t>(V.size() / 2), V.end());
		return V[V.size() / 2];
	}

	void ScorchSheet(const std::string& Dir, const std::string& Word)
	{
		Wreck W;
		Laid None, Hold, Out;
		LayWreck(None, Word, W, 0);
		LayWreck(Hold, Word, W, 1);
		LayWreck(Out, Word, W, 2);
		// (from a man's height some 22 m off, her hull stood in; and from above with her hull lifted away)
		const double Ex = -14.0, Ey = -17.0;
		const Camera Low(Ex, Ey, 5.5, 0.0, 0.0, 0.0, 52.0, 1200);
		const Camera Above(0.0, -0.5, 40.0, 0.0, 0.0, 0.0, 34.0, 1200);
		const char* Names[3] = { "off", "hold", "burnt" };
		const Laid* Ls[3] = { &None, &Hold, &Out };
		Picture Tops[3] = { Picture(1, 1), Picture(1, 1), Picture(1, 1) };
		for (int K = 0; K < 3; ++K)
		{
			const Picture Side = PaintWreck(*Ls[K], Low, &W);
			if (!Write(Side, Dir + "/scorch-" + Word + "-" + Names[K] + ".ppm", 260.0)) std::fprintf(stderr, "could not write\n");
			Tops[K] = PaintWreck(*Ls[K], Above, nullptr);
			if (!Write(Tops[K], Dir + "/scorch-" + Word + "-" + Names[K] + "-above.ppm", 1e9)) std::fprintf(stderr, "could not write\n");
		}
		// Each look's laying, and the switch's: laid only so far, once (no doubling).
		{
			LandCrater::Field& F = Out.Holes;
			const double Has = F.Scorched(0);
			const bool bAgain = F.AddScorch(0, W.X, W.Y, W.Ax, W.Ay, W.Long, W.Wide, WindX, WindY, 1.0, Word);
			const bool bLess = F.AddScorch(0, W.X, W.Y, W.Ax, W.Ay, W.Long, W.Wide, WindX, WindY, 0.35, Word);
			int Records = 0;
			for (const LandCrater::Field::Hole& H : F.All()) Records += H.Burnt > 0.0 ? 1 : 0;
			int NoneRecords = 0;
			for (const LandCrater::Field::Hole& H : None.Holes.All()) NoneRecords += (H.Burnt != 0.0 || H.BurntOf != 0 || H.BurntX != 0.0 || H.BurntY != 0.0) ? 1 : 0;
			char Buf[200];
			std::snprintf(Buf, sizeof Buf, "laid to %.2f in %d records; again %s, less %s; with none laid %d holes carry any of it", Has, Records, bAgain ? "laid" : "refused", bLess ? "laid" : "refused", NoneRecords);
			Check(Has == 1.0 && Records == 2 && !bAgain && !bLess && NoneRecords == 0, "her scorch laid once each way, and nothing of it on a hole with none", Buf);
		}
		// Its film lies on what is there and is read as a face: TopAt at least its height at each triangle's middle, and
		// no triangle of it under the ground (but in a dug hole's cut).
		{
			long Tris = 0, Missed = 0, Under = 0, Sunk = 0;
			const LandCrater::Field& F = Out.Holes;
			for (const LandCrater::Field::Hole& H : F.All())
			{
				if (!(H.Burnt > 0.0)) continue;
				const LandField::Mesh& M = F.Section(H.In);
				// (its triangles: the middle's fan and the rings' quads, as AddScorch lays them)
				const uint32_t Last = H.First + 1u + static_cast<uint32_t>(9 * H.Sides);
				for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
				{
					const uint32_t A = M.Triangles[T], B = M.Triangles[T + 1], C = M.Triangles[T + 2];
					if (A < H.First || A >= Last || B < H.First || B >= Last || C < H.First || C >= Last) continue;
					const double Px = (M.Positions[A * 3] + M.Positions[B * 3] + M.Positions[C * 3]) / 3.0;
					const double Py = (M.Positions[A * 3 + 1] + M.Positions[B * 3 + 1] + M.Positions[C * 3 + 1]) / 3.0;
					const double Pz = (M.Positions[A * 3 + 2] + M.Positions[B * 3 + 2] + M.Positions[C * 3 + 2]) / 3.0;
					++Tris;
					double Top = 0.0;
					if (!F.TopAt(Px, Py, Top)) ++Missed;
					else if (Top < Pz - 0.002) ++Under;
					if (!F.InCut(Px, Py) && Pz < GroundZ(Px, Py) + 0.001)
					{
						++Sunk;
						std::printf("  sunk: %.2f %.2f, %.4f over the ground; corners %.4f %.4f %.4f over\n", Px, Py, Pz - GroundZ(Px, Py), M.Positions[A * 3 + 2] - GroundZ(M.Positions[A * 3], M.Positions[A * 3 + 1]),
							M.Positions[B * 3 + 2] - GroundZ(M.Positions[B * 3], M.Positions[B * 3 + 1]), M.Positions[C * 3 + 2] - GroundZ(M.Positions[C * 3], M.Positions[C * 3 + 1]));
					}
				}
			}
			double Bowl = 0.0;
			const bool bBowl = F.TopAt(-5.0, 4.0, Bowl);
			char Buf[220];
			std::snprintf(Buf, sizeof Buf, "%ld triangles, %ld with no face, %ld under it, %ld under the ground; the later mortar bomb's bowl %.2f m under the ground", Tris, Missed, Under, Sunk, bBowl ? GroundZ(-5.0, 4.0) - Bowl : -1.0);
			Check(Tris > 400 && Missed == 0 && Under == 0 && Sunk == 0 && bBowl && GroundZ(-5.0, 4.0) - Bowl > 0.15, "her scorch lies on the ground as a face, and a later hole is torn through it", Buf);
		}
		if (Word != "earth")
		{
			return;
		}
		// The art director's marks for it, on earth, read from above: her soot core dark (median luma 60 or less), her
		// browned ring between (85 to 120), the ground untouched (within 10) by 1.2 m past it, and the ring half her
		// length and 2 m out (within 15%). Read only where no shell hole reaches.
		const auto NearHole = [&](const LandCrater::Field& F, double X, double Y)
		{
			for (const LandCrater::Field::Hole& H : F.All())
			{
				if (H.Burnt > 0.0) continue;
				if (std::hypot(X - H.X, Y - H.Y) < H.Out + 0.3) return true;
			}
			return false;
		};
		const double Reach = LandCrater::Field::ScorchReach(W.Long);
		const LandCrater::Field::Hole* Brown = nullptr;
		for (const LandCrater::Field::Hole& H : Hold.Holes.All()) if (H.Burnt > 0.0) Brown = &H;
		if (!Brown)
		{
			Check(false, "her browned grass laid", "none");
			return;
		}
		const LandField::Mesh& BM = Hold.Holes.Section(Brown->In);
		const LandCrater::Field::Hole* Soot = nullptr;
		for (const LandCrater::Field::Hole& H : Out.Holes.All()) if (H.Burnt >= 1.0) Soot = &H;
		if (!Soot)
		{
			Check(false, "her soot core laid", "none");
			return;
		}
		const LandField::Mesh& SM = Out.Holes.Section(Soot->In);
		// (where her soot's feather ends on a way: its last ring)
		const auto SootOut = [&](int K)
		{
			const uint32_t I = Soot->First + 1u + static_cast<uint32_t>(8 * Soot->Sides + K);
			return std::hypot(SM.Positions[I * 3] - W.X, SM.Positions[I * 3 + 1] - W.Y);
		};
		const int N = Brown->Sides;
		const auto EdgeAt = [&](int K)
		{
			const uint32_t I = Brown->First + 1u + static_cast<uint32_t>(5 * N + K);
			return std::hypot(BM.Positions[I * 3] - W.X, BM.Positions[I * 3 + 1] - W.Y);
		};
		std::vector<double> Core, RingHold, RingOut, Past, Radii;
		double WorstPast = 0.0;
		const double Lx = -W.Ay, Ly = W.Ax;
		for (double Y = -10.0; Y <= 10.0; Y += 0.25)
		{
			for (double X = -10.0; X <= 10.0; X += 0.25)
			{
				if (NearHole(Out.Holes, X, Y)) continue;
				const double U = (X - W.X) * W.Ax + (Y - W.Y) * W.Ay, V = (X - W.X) * Lx + (Y - W.Y) * Ly;
				if (std::fabs(U) <= 0.5 * W.Long + 0.6 && std::fabs(V) <= 0.5 * W.Wide + 0.6)
				{
					Core.push_back(LumaAt(Tops[2], Above, X, Y, GroundZ(X, Y)));
				}
			}
		}
		for (int K = 0; K < N; ++K)
		{
			const double A = 2.0 * Pi * K / N, Ca = std::cos(A), Sa = std::sin(A);
			const double E = EdgeAt(K);
			const double Down = (Ca * WindX + Sa * WindY) / std::hypot(WindX, WindY);
			double Ring = 0.0;
			int RingN = 0;
			for (double R = 0.55 * E; R <= 0.9 * E; R += 0.05)
			{
				const double X = W.X + Ca * R, Y = W.Y + Sa * R;
				if (NearHole(Out.Holes, X, Y)) continue;
				const double U = (X - W.X) * W.Ax + (Y - W.Y) * W.Ay, V = (X - W.X) * Lx + (Y - W.Y) * Ly;
				if (std::fabs(U) <= 0.5 * W.Long + 0.9 && std::fabs(V) <= 0.5 * W.Wide + 0.9) continue; // (under her hull)
				const double Z = GroundZ(X, Y);
				const double Lh = LumaAt(Tops[1], Above, X, Y, Z), L0 = LumaAt(Tops[0], Above, X, Y, Z);
				RingHold.push_back(Lh);
				if (Down < 0.2 && R > SootOut(K) + 0.1) RingOut.push_back(LumaAt(Tops[2], Above, X, Y, Z)); // (past her soot's feather)
				Ring += std::fabs(Lh - L0);
				++RingN;
			}
			for (const double R : { E + 1.2, E + 1.35, E + 1.5 })
			{
				const double X = W.X + Ca * R, Y = W.Y + Sa * R;
				if (NearHole(Out.Holes, X, Y)) continue;
				const double Z = GroundZ(X, Y);
				const double L0 = LumaAt(Tops[0], Above, X, Y, Z);
				for (int S = 1; S <= 2; ++S)
				{
					const double D = std::fabs(LumaAt(Tops[S], Above, X, Y, Z) - L0);
					Past.push_back(D);
					WorstPast = std::max(WorstPast, D);
				}
			}
			// (where the browning shows to: the farthest point still half as far from the ground's own luma as the ring)
			if (RingN < 3) continue;
			const double Half = 0.5 * Ring / RingN;
			double Seen = 0.0;
			bool bClear = true;
			for (double R = 0.5 * E; R <= E + 2.0; R += 0.05)
			{
				const double X = W.X + Ca * R, Y = W.Y + Sa * R;
				if (NearHole(Out.Holes, X, Y)) { bClear = false; break; }
				const double Z = GroundZ(X, Y);
				if (std::fabs(LumaAt(Tops[1], Above, X, Y, Z) - LumaAt(Tops[0], Above, X, Y, Z)) >= Half) Seen = R;
			}
			if (bClear) Radii.push_back(Seen / Reach);
		}
		char Buf[240];
		const double CoreL = Median(Core), HoldL = Median(RingHold), OutL = Median(RingOut), RadiusR = Median(Radii);
		std::snprintf(Buf, sizeof Buf, "median luma %.0f over %zu points", CoreL, Core.size());
		Check(Core.size() > 100 && CoreL >= 0.0 && CoreL <= 60.0, "burnt out: her soot core dark", Buf);
		std::snprintf(Buf, sizeof Buf, "median luma %.0f as the fire takes hold (%zu points), %.0f burnt out, upwind (%zu)", HoldL, RingHold.size(), OutL, RingOut.size());
		Check(RingHold.size() > 100 && RingOut.size() > 20 && HoldL >= 85.0 && HoldL <= 120.0 && OutL >= 85.0 && OutL <= 120.0, "her browned ring between the soot and the grass", Buf);
		std::snprintf(Buf, sizeof Buf, "%zu points 1.2 to 1.5 m past it, median %.1f off, at most %.1f", Past.size(), Median(Past), WorstPast);
		Check(Past.size() > 60 && WorstPast <= 10.0, "the ground untouched 1.2 m past her ring", Buf);
		std::snprintf(Buf, sizeof Buf, "browning seen to %.2f of %.2f m (median of %zu ways)", RadiusR, Reach, Radii.size());
		Check(Radii.size() >= 10 && RadiusR >= 0.85 && RadiusR <= 1.15, "her ring half her length and 2 m out", Buf);
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
	// Scorched ground round a burnt-out tank (release 2, the "scorch" look): its pictures on every ground, its checks.
	ScorchSheet(Dir, Word);
	std::printf("%s\n", Failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failures ? 1 : 0;
}
