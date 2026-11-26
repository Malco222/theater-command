// The men's picture sheets for Brian, painted from Battle/LandMen's own skinning (the crowd as the scene will play
// it, its looks and colours) by a small software painter: a depth buffer, a low sun with a soft shadow map, light from
// the sky and the ground, 2x2 samples a pixel. Not the game's renderer: its light and its ground are stand-ins.
//   falls.ppm   the three falls, from the side: each hit to rest in six steps, an American and a German
//   battle.ppm  a moment of the recorded battle from a low eye
// Usage: land_men_picture <recording.json> <soldiers folder> <armies.json> <out folder>
// LAND_MEN_MOMENT="t eyeX eyeY eyeUp lookX lookY lookUp fov" picks another moment (field metres, up over the ground).
#include "Battle/LandMen.h"
#include "land_json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace LandMen;

namespace
{
	std::string ReadFile(const std::string& Path)
	{
		std::ifstream F(Path, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	struct D3
	{
		double X = 0, Y = 0, Z = 0;
	};
	D3 operator+(D3 A, D3 B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
	D3 operator-(D3 A, D3 B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
	D3 operator*(D3 A, double K) { return { A.X * K, A.Y * K, A.Z * K }; }
	double Dot(D3 A, D3 B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	D3 Cross(D3 A, D3 B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
	D3 Unit(D3 A)
	{
		const double L = std::sqrt(Dot(A, A));
		return L > 1e-12 ? A * (1.0 / L) : D3 { 0, 0, 1 };
	}

	double ToLinear(double C) { C /= 255.0; return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4); }
	double ToSrgb(double C) { C = std::max(0.0, std::min(1.0, C)); return C <= 0.0031308 ? C * 12.92 : 1.055 * std::pow(C, 1.0 / 2.4) - 0.055; }
	D3 Lin(int R, int G, int B) { return { ToLinear(R), ToLinear(G), ToLinear(B) }; }

	// Value noise for the ground's mottling.
	double Hash2(int X, int Y)
	{
		uint32_t H = static_cast<uint32_t>(X) * 374761393u + static_cast<uint32_t>(Y) * 668265263u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return static_cast<double>(H ^ (H >> 16)) / 4294967295.0;
	}
	double Noise(double X, double Y)
	{
		const int X0 = static_cast<int>(std::floor(X)), Y0 = static_cast<int>(std::floor(Y));
		const double U = X - X0, V = Y - Y0;
		const double SU = U * U * (3 - 2 * U), SV = V * V * (3 - 2 * V);
		const double A = Hash2(X0, Y0), B = Hash2(X0 + 1, Y0), C = Hash2(X0, Y0 + 1), D = Hash2(X0 + 1, Y0 + 1);
		return (A * (1 - SU) + B * SU) * (1 - SV) + (C * (1 - SU) + D * SU) * SV;
	}
	/** The mottling at a point seen with a pixel Foot metres across: the octaves finer than the pixel fade to their
	 * mean, so the far ground does not shimmer into rings. */
	double Fbm(double X, double Y, double Foot)
	{
		auto Octave = [&](double Scale, double Weight) {
			const double K = std::max(0.0, std::min(1.0, 1.5 - 2.0 * Foot / Scale));
			return Weight * (K * Noise(X / Scale, Y / Scale) + (1.0 - K) * 0.5);
		};
		return Octave(6.0, 0.5) + Octave(1.4, 0.3) + Octave(0.3, 0.2);
	}

	/** What is painted: points with a facing and a colour (linear), and triangles; the ground is coloured as it is lit. */
	struct Scene
	{
		std::vector<D3> P, N, A;
		std::vector<uint8_t> IsGround;
		std::vector<uint32_t> T;
		void AddMan(const Look& Lk, const std::vector<float>& At, const std::vector<float>& Fa)
		{
			const uint32_t Base = static_cast<uint32_t>(P.size());
			for (int I = 0; I < Lk.Points; ++I)
			{
				const size_t K = static_cast<size_t>(I) * 3;
				P.push_back({ At[K], At[K + 1], At[K + 2] });
				N.push_back(Unit({ Fa[K], Fa[K + 1], Fa[K + 2] }));
				A.push_back({ ToLinear(Lk.Rgb[K]), ToLinear(Lk.Rgb[K + 1]), ToLinear(Lk.Rgb[K + 2]) });
				IsGround.push_back(0);
			}
			for (const uint32_t V : Lk.Triangles) T.push_back(Base + V);
		}
		/** The ground over a rectangle in cells of Step metres, leaving out the cells inside a hole (where a finer grid
		 * lies) and lowered a little there so their seam never shows the sky. */
		void AddGround(double X0, double Y0, double X1, double Y1, double Step, const GroundFn& G, const double* Hole = nullptr)
		{
			const int NX = static_cast<int>(std::ceil((X1 - X0) / Step)), NY = static_cast<int>(std::ceil((Y1 - Y0) / Step));
			const uint32_t Base = static_cast<uint32_t>(P.size());
			for (int J = 0; J <= NY; ++J)
			{
				for (int I = 0; I <= NX; ++I)
				{
					const double X = X0 + I * Step, Y = Y0 + J * Step;
					const Ground Gr = G(X, Y);
					P.push_back({ X, Y, Gr.Height - (Hole ? 0.03 : 0.0) });
					N.push_back({ Gr.Normal.X, Gr.Normal.Y, Gr.Normal.Z });
					A.push_back({ 0, 0, 0 });
					IsGround.push_back(1);
				}
			}
			for (int J = 0; J < NY; ++J)
			{
				for (int I = 0; I < NX; ++I)
				{
					if (Hole)
					{
						const double CX = X0 + (I + 0.5) * Step, CY = Y0 + (J + 0.5) * Step;
						if (CX > Hole[0] && CX < Hole[2] && CY > Hole[1] && CY < Hole[3]) continue;
					}
					const uint32_t A0 = Base + static_cast<uint32_t>(J * (NX + 1) + I);
					const uint32_t B0 = A0 + 1, C0 = A0 + static_cast<uint32_t>(NX + 1), D0 = C0 + 1;
					T.insert(T.end(), { A0, B0, D0, A0, D0, C0 });
				}
			}
		}
	};

	struct Light
	{
		D3 ToSun;
		D3 Sun, Sky, Bounce, Haze;
		double HazeK = 0.0;
		D3 EarthA, EarthB;        // the ground's two colours, mottled between
		double Exposure = 1.0;
	};

	/** The sun's view of the scene: the nearest depth toward the sun in each texel. */
	struct ShadowMap
	{
		D3 U, V, L;
		double U0 = 0, V0 = 0, Texel = 0.01;
		int Res = 2048;
		std::vector<float> Depth;

		void Build(const Scene& S, const D3& ToSun, D3 Lo, D3 Hi, int R)
		{
			Res = R;
			L = Unit(ToSun);
			U = Unit(Cross({ 0, 0, 1 }, L));
			if (Dot(U, U) < 0.5) U = { 1, 0, 0 };
			V = Cross(L, U);
			double UMin = 1e18, UMax = -1e18, VMin = 1e18, VMax = -1e18;
			for (int C = 0; C < 8; ++C)
			{
				const D3 Q { C & 1 ? Hi.X : Lo.X, C & 2 ? Hi.Y : Lo.Y, C & 4 ? Hi.Z : Lo.Z };
				UMin = std::min(UMin, Dot(Q, U)); UMax = std::max(UMax, Dot(Q, U));
				VMin = std::min(VMin, Dot(Q, V)); VMax = std::max(VMax, Dot(Q, V));
			}
			U0 = UMin;
			V0 = VMin;
			Texel = std::max(UMax - UMin, VMax - VMin) / Res;
			Depth.assign(static_cast<size_t>(Res) * static_cast<size_t>(Res), -1e30f);
			for (size_t K = 0; K + 2 < S.T.size(); K += 3)
			{
				double PX[3], PY[3], PD[3];
				for (int C = 0; C < 3; ++C)
				{
					const D3& Q = S.P[S.T[K + static_cast<size_t>(C)]];
					PX[C] = (Dot(Q, U) - U0) / Texel;
					PY[C] = (Dot(Q, V) - V0) / Texel;
					PD[C] = Dot(Q, L);
				}
				const double Area = (PX[1] - PX[0]) * (PY[2] - PY[0]) - (PX[2] - PX[0]) * (PY[1] - PY[0]);
				if (std::fabs(Area) < 1e-12) continue;
				const int X0 = std::max(0, static_cast<int>(std::floor(std::min({ PX[0], PX[1], PX[2] }))));
				const int X1 = std::min(Res - 1, static_cast<int>(std::ceil(std::max({ PX[0], PX[1], PX[2] }))));
				const int Y0 = std::max(0, static_cast<int>(std::floor(std::min({ PY[0], PY[1], PY[2] }))));
				const int Y1 = std::min(Res - 1, static_cast<int>(std::ceil(std::max({ PY[0], PY[1], PY[2] }))));
				for (int Y = Y0; Y <= Y1; ++Y)
				{
					for (int X = X0; X <= X1; ++X)
					{
						const double CX = X + 0.5, CY = Y + 0.5;
						const double W0 = ((PX[1] - CX) * (PY[2] - CY) - (PX[2] - CX) * (PY[1] - CY)) / Area;
						const double W1 = ((PX[2] - CX) * (PY[0] - CY) - (PX[0] - CX) * (PY[2] - CY)) / Area;
						const double W2 = 1.0 - W0 - W1;
						if (W0 < -1e-9 || W1 < -1e-9 || W2 < -1e-9) continue;
						const float D = static_cast<float>(W0 * PD[0] + W1 * PD[1] + W2 * PD[2]);
						float& Cell = Depth[static_cast<size_t>(Y) * static_cast<size_t>(Res) + static_cast<size_t>(X)];
						Cell = std::max(Cell, D);
					}
				}
			}
		}

		/** How much of the sun reaches a point (1 none of it shaded), soft over a few texels. The point is looked up a
		 * little off its face (along its facing) so a face does not shade itself. */
		double Lit(const D3& At, const D3& Normal) const
		{
			const double Slope = std::sqrt(std::max(0.0, 1.0 - Dot(Normal, L) * Dot(Normal, L)));
			const D3 Q = At + Normal * (Texel * (1.0 + 2.5 * Slope));
			const double Bias = Texel * 1.0 + 0.002;
			const double FX = (Dot(Q, U) - U0) / Texel - 0.5, FY = (Dot(Q, V) - V0) / Texel - 0.5;
			const double D = Dot(Q, L) + Bias;
			double Sum = 0.0, Weight = 0.0;
			for (int J = -2; J <= 2; ++J)
			{
				for (int I = -2; I <= 2; ++I)
				{
					const int X = static_cast<int>(std::floor(FX)) + I, Y = static_cast<int>(std::floor(FY)) + J;
					if (X < 0 || Y < 0 || X >= Res || Y >= Res) { Sum += 1.0; Weight += 1.0; continue; }
					const double Wt = 1.0 / (1.0 + 0.35 * (I * I + J * J));
					Sum += Wt * (Depth[static_cast<size_t>(Y) * static_cast<size_t>(Res) + static_cast<size_t>(X)] > D ? 0.0 : 1.0);
					Weight += Wt;
				}
			}
			return Sum / Weight;
		}
	};

	struct Camera
	{
		D3 Eye, Fwd, Right, Up;
		double Focal = 1.0;
		int W = 0, H = 0;
		Camera(D3 E, D3 Look, double FovDeg, int Width, int Height) : Eye(E), W(Width), H(Height)
		{
			Fwd = Unit(Look - E);
			Right = Unit(Cross(Fwd, { 0, 0, 1 }));
			Up = Cross(Right, Fwd);
			Focal = (W * 0.5) / std::tan(FovDeg * 3.14159265358979 / 360.0);
		}
	};

	/** Linear colour, W x H. */
	struct Picture
	{
		int W = 0, H = 0;
		std::vector<D3> C;
		std::vector<double> Z;
	};

	D3 ToEyeOf(const Camera& Cam, const D3& Q) { return Cam.Eye - Q; }

	D3 SkyAt(const Light& Lt, const D3& Dir)
	{
		const double E = std::max(0.0, Dir.Z);
		const double K = std::pow(E, 0.6);
		return Lt.Haze * (1.0 - K) + Lt.Sky * 1.15 * K;
	}

	Picture Paint(const Scene& S, const Camera& Cam, const Light& Lt, const ShadowMap& Sh)
	{
		Picture Pic;
		Pic.W = Cam.W;
		Pic.H = Cam.H;
		Pic.C.assign(static_cast<size_t>(Cam.W) * static_cast<size_t>(Cam.H), D3());
		Pic.Z.assign(Pic.C.size(), 1e30);
		for (int Y = 0; Y < Cam.H; ++Y)
		{
			for (int X = 0; X < Cam.W; ++X)
			{
				const D3 Dir = Unit(Cam.Fwd * Cam.Focal + Cam.Right * (X + 0.5 - Cam.W * 0.5) - Cam.Up * (Y + 0.5 - Cam.H * 0.5));
				Pic.C[static_cast<size_t>(Y) * static_cast<size_t>(Cam.W) + static_cast<size_t>(X)] = SkyAt(Lt, Dir);
			}
		}
		const double Near = 0.05;
		struct Vx { D3 V; D3 P, N, A; };
		std::vector<Vx> Poly, Next;
		for (size_t K = 0; K + 2 < S.T.size(); K += 3)
		{
			Poly.clear();
			bool bGround = false;
			for (int C = 0; C < 3; ++C)
			{
				const uint32_t I = S.T[K + static_cast<size_t>(C)];
				const D3 R = S.P[I] - Cam.Eye;
				Poly.push_back({ { Dot(R, Cam.Right), Dot(R, Cam.Up), Dot(R, Cam.Fwd) }, S.P[I], S.N[I], S.A[I] });
				bGround = bGround || S.IsGround[I];
			}
			// Cut away what is behind the eye.
			Next.clear();
			for (size_t C = 0; C < Poly.size(); ++C)
			{
				const Vx& A = Poly[C];
				const Vx& B = Poly[(C + 1) % Poly.size()];
				const bool bA = A.V.Z >= Near, bB = B.V.Z >= Near;
				if (bA) Next.push_back(A);
				if (bA != bB)
				{
					const double U = (Near - A.V.Z) / (B.V.Z - A.V.Z);
					Next.push_back({ A.V + (B.V - A.V) * U, A.P + (B.P - A.P) * U, A.N + (B.N - A.N) * U, A.A + (B.A - A.A) * U });
				}
			}
			if (Next.size() < 3) continue;
			for (size_t F = 1; F + 1 < Next.size(); ++F)
			{
				const Vx* Tri[3] = { &Next[0], &Next[F], &Next[F + 1] };
				double SX[3], SY[3], IZ[3];
				for (int C = 0; C < 3; ++C)
				{
					IZ[C] = 1.0 / Tri[C]->V.Z;
					SX[C] = Cam.W * 0.5 + Cam.Focal * Tri[C]->V.X * IZ[C];
					SY[C] = Cam.H * 0.5 - Cam.Focal * Tri[C]->V.Y * IZ[C];
				}
				const double Area = (SX[1] - SX[0]) * (SY[2] - SY[0]) - (SX[2] - SX[0]) * (SY[1] - SY[0]);
				if (std::fabs(Area) < 1e-12) continue;
				const int X0 = std::max(0, static_cast<int>(std::floor(std::min({ SX[0], SX[1], SX[2] }))));
				const int X1 = std::min(Cam.W - 1, static_cast<int>(std::ceil(std::max({ SX[0], SX[1], SX[2] }))));
				const int Y0 = std::max(0, static_cast<int>(std::floor(std::min({ SY[0], SY[1], SY[2] }))));
				const int Y1 = std::min(Cam.H - 1, static_cast<int>(std::ceil(std::max({ SY[0], SY[1], SY[2] }))));
				for (int Y = Y0; Y <= Y1; ++Y)
				{
					for (int X = X0; X <= X1; ++X)
					{
						const double CX = X + 0.5, CY = Y + 0.5;
						double W0 = ((SX[1] - CX) * (SY[2] - CY) - (SX[2] - CX) * (SY[1] - CY)) / Area;
						double W1 = ((SX[2] - CX) * (SY[0] - CY) - (SX[0] - CX) * (SY[2] - CY)) / Area;
						double W2 = 1.0 - W0 - W1;
						if (W0 < -1e-9 || W1 < -1e-9 || W2 < -1e-9) continue;
						const double Iz = W0 * IZ[0] + W1 * IZ[1] + W2 * IZ[2];
						const double Z = 1.0 / Iz;
						const size_t Px = static_cast<size_t>(Y) * static_cast<size_t>(Cam.W) + static_cast<size_t>(X);
						if (Z >= Pic.Z[Px]) continue;
						Pic.Z[Px] = Z;
						// Perspective-correct weights.
						W0 *= IZ[0] * Z; W1 *= IZ[1] * Z; W2 *= IZ[2] * Z;
						const D3 Q = Tri[0]->P * W0 + Tri[1]->P * W1 + Tri[2]->P * W2;
						D3 Nn = Unit(Tri[0]->N * W0 + Tri[1]->N * W1 + Tri[2]->N * W2);
						const D3 ToEye = Cam.Eye - Q;
						if (!bGround && Dot(Nn, ToEye) < 0.0) Nn = Nn * -1.0;
						D3 Alb;
						if (bGround)
						{
							// A pixel's width on the ground there (grazing views stretch it).
							const double Foot = Z / Cam.Focal / std::max(0.08, std::fabs(Dot(Unit(ToEyeOf(Cam, Q)), Nn)));
							const double M = Fbm(Q.X, Q.Y, Foot);
							const double K2 = std::max(0.0, std::min(1.0, (M - 0.3) * 1.6));
							Alb = Lt.EarthA * (1.0 - K2) + Lt.EarthB * K2;
							const double Fine = std::max(0.0, std::min(1.0, 1.5 - 2.0 * Foot / 0.11));
							Alb = Alb * (1.0 + Fine * 0.24 * (Noise(Q.X * 9.0, Q.Y * 9.0) - 0.5));
						}
						else Alb = Tri[0]->A * W0 + Tri[1]->A * W1 + Tri[2]->A * W2;
						const double Sun = std::max(0.0, Dot(Nn, Lt.ToSun)) * Sh.Lit(Q, Nn);
						const D3 Hemi = Lt.Sky * (0.55 + 0.45 * Nn.Z) + Lt.Bounce * (0.45 - 0.45 * Nn.Z);
						D3 Col { Alb.X * (Lt.Sun.X * Sun + Hemi.X), Alb.Y * (Lt.Sun.Y * Sun + Hemi.Y), Alb.Z * (Lt.Sun.Z * Sun + Hemi.Z) };
						const double Dist = std::sqrt(Dot(ToEye, ToEye));
						const double Fog = 1.0 - std::exp(-Dist * Lt.HazeK);
						Col = Col * (1.0 - Fog) + Lt.Haze * Fog;
						Pic.C[Px] = Col;
					}
				}
			}
		}
		return Pic;
	}

	/** The picture shrunk by 2 (its 2x2 samples averaged), exposed and toned, into 8-bit sRGB at (OX, OY) of a sheet. */
	void Put(const Picture& Pic, double Exposure, std::vector<uint8_t>& Sheet, int SheetW, int OX, int OY)
	{
		const int W = Pic.W / 2, H = Pic.H / 2;
		for (int Y = 0; Y < H; ++Y)
		{
			for (int X = 0; X < W; ++X)
			{
				D3 S;
				for (int J = 0; J < 2; ++J)
				{
					for (int I = 0; I < 2; ++I) S = S + Pic.C[static_cast<size_t>(Y * 2 + J) * static_cast<size_t>(Pic.W) + static_cast<size_t>(X * 2 + I)];
				}
				S = S * (0.25 * Exposure);
				const double V[3] = { S.X, S.Y, S.Z };
				for (int C = 0; C < 3; ++C)
				{
					// A gentle shoulder for the sunlit highlights.
					const double T = V[C] / (1.0 + 0.25 * V[C]);
					Sheet[(static_cast<size_t>(OY + Y) * static_cast<size_t>(SheetW) + static_cast<size_t>(OX + X)) * 3 + static_cast<size_t>(C)] =
						static_cast<uint8_t>(std::lround(255.0 * ToSrgb(T * 1.25)));
				}
			}
		}
	}

	bool WritePpm(const std::string& Path, const std::vector<uint8_t>& Rgb, int W, int H)
	{
		std::ofstream F(Path, std::ios::binary);
		F << "P6\n" << W << " " << H << "\n255\n";
		F.write(reinterpret_cast<const char*>(Rgb.data()), static_cast<std::streamsize>(Rgb.size()));
		return static_cast<bool>(F);
	}

	D3 SunFrom(double AzimuthDeg, double ElevationDeg)
	{
		const double A = AzimuthDeg * 3.14159265358979 / 180.0, E = ElevationDeg * 3.14159265358979 / 180.0;
		return { std::sin(A) * std::cos(E), std::cos(A) * std::cos(E), std::sin(E) };
	}

	Light Daylight(double SunAz, double SunEl)
	{
		Light Lt;
		Lt.ToSun = SunFrom(SunAz, SunEl);
		const double Low = std::max(0.0, std::min(1.0, (30.0 - SunEl) / 25.0));
		Lt.Sun = D3 { 1.0, 0.93 - 0.08 * Low, 0.82 - 0.16 * Low } * 2.4;
		Lt.Sky = D3 { 0.36, 0.43, 0.56 } * 0.75;
		Lt.Bounce = D3 { 0.30, 0.25, 0.19 } * 0.35;
		Lt.Haze = D3 { 0.62, 0.66, 0.70 } * 0.95;
		return Lt;
	}

	void Skinned(Crowd& C, int Man, std::vector<float>& At, std::vector<float>& Fa)
	{
		At.resize(static_cast<size_t>(C.PointsOf(Man, Near)) * 3);
		Fa.resize(At.size());
		C.Skin(Man, Near, At.data(), Fa.data());
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 5)
	{
		std::fprintf(stderr, "usage: land_men_picture <recording.json> <soldiers folder> <armies.json> <out folder>\n");
		return 2;
	}
	const std::string OutDir = Argv[4];
	std::map<std::string, std::shared_ptr<Soldier>> Files;
	std::string Err;
	for (const char* Army : { "GER", "POL", "USA", "USA_bar", "USA_nco", "USA_bazooka", "USA_loader" })
	{
		auto S = std::make_shared<Soldier>();
		const std::string Text = ReadFile(std::string(Argv[2]) + "/soldier_" + Army + ".json");
		if (Text.empty() && Army[0] == 'U') continue; // (the GI's files, when they are here)
		if (!S->Load(Text, Err)) { std::printf("soldier %s: %s\n", Army, Err.c_str()); return 1; }
		Files[Army] = S;
	}
	const std::string Armies = ReadFile(Argv[3]);

	// ---- The falls: six men on flat earth, each hit at 3 s: standing aimed, running, kneeling; an American, then a
	// German. Played by the crowd from a small recording of their own, as the battle's men are, so each takes the fall
	// the crowd picks for him (the names in brackets are the README's table, used only by a file with no deaths map).
	{
		const char* Falls[3][2] = { { "fire_stand", "die_back" }, { "run", "die_forward" }, { "wait", "die_kneel" } };
		std::ostringstream R;
		R << "{\"seed\": 11, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"}],"
		  << "\"men\": [";
		const double Hit = 3.0;
		for (int Row = 0; Row < 6; ++Row)
		{
			const int Fall = Row / 2, Army = Row % 2;
			const double X = 40.0 * Row;
			if (Row) R << ",";
			R << "{\"b\": " << (Army ? 2 : 1) << ", \"k\": " << (Row + 3) << ", \"r\": \"rifle\", \"at\": ";
			if (Fall == 1) R << "[[0, " << X - 3.3 * Hit << ", 0, 1.5708], [" << Hit << ", " << X << ", 0, 1.5708], [" << 2 * Hit << ", " << X + 3.3 * Hit << ", 0, 1.5708]]";
			else R << "[[0, " << X << ", 0, 1.5708], [10, " << X << ", 0, 1.5708]]";
			R << ", \"ch\": [[0, \"" << Falls[Fall][0] << "\", \"none\", 0, 0, 0, 0]], \"d\": " << Hit << "}";
		}
		R << "], \"rounds\": []}";
		const GroundFn Flat = [](double, double) { return Ground(); };
		// Which fall each man takes is the crowd's to pick (the soldier file's deaths map, by his stance and where the
		// round came from; the README's table without one). A first crowd plays to just after the hit to learn each
		// pick and its length; the second, built the same, is the one drawn (the picks are the seed's, so they match).
		std::string Picked[6];
		{
			Crowd Probe;
			for (const auto& KV : Files) Probe.AddSoldier(KV.first, KV.second);
			if (!Probe.ReadArmies(Armies, Err) || !Probe.Build(R.str(), Flat, Err)) { std::printf("falls: %s\n", Err.c_str()); return 1; }
			Probe.SetEye(-1000, -1000, 2);
			for (double T = 0.0; T <= Hit + 0.5; T += 1.0 / 60.0) Probe.Update(T);
			for (int Row = 0; Row < 6; ++Row) Picked[Row] = Probe.Info(Probe.Find(Row % 2 ? 2 : 1, Row + 3)).FallClip;
		}
		Crowd C;
		for (const auto& KV : Files) C.AddSoldier(KV.first, KV.second);
		if (!C.ReadArmies(Armies, Err) || !C.Build(R.str(), Flat, Err)) { std::printf("falls: %s\n", Err.c_str()); return 1; }
		C.SetEye(-1000, -1000, 2);

		// Each man's six steps: the moment he is hit, four through the fall, and laid at rest.
		const int Cols = 6, Rows = 6, SheetW = 1600, CellH = 190;
		struct Step { std::vector<float> At, Fa; };
		std::vector<std::vector<Step>> Steps(Rows);
		std::vector<double> Times;
		std::vector<std::pair<int, int>> Order;
		for (int Row = 0; Row < Rows; ++Row)
		{
			const int Man = C.Find(Row % 2 ? 2 : 1, Row + 3);
			const Soldier& S = *C.SoldierOf(Man);
			const int Ix = S.ClipIndex(Picked[Row].empty() ? std::string(Falls[Row / 2][1]) : Picked[Row]);
			const double Long = Ix >= 0 ? S.Clips[static_cast<size_t>(Ix)].Long : 1.5;
			for (int Col = 0; Col < Cols; ++Col) Order.push_back({ Row, Col }), Times.push_back(Hit + (Col < Cols - 1 ? Long * Col / (Cols - 1) : Long + 0.1));
			Steps[static_cast<size_t>(Row)].resize(Cols);
		}
		std::vector<size_t> Idx(Times.size());
		for (size_t K = 0; K < Idx.size(); ++K) Idx[K] = K;
		std::stable_sort(Idx.begin(), Idx.end(), [&](size_t A, size_t B) { return Times[A] < Times[B]; });
		double Now = 0.0;
		for (const size_t K : Idx)
		{
			while (Now + 1.0 / 60.0 < Times[K]) { Now += 1.0 / 60.0; C.Update(Now); }
			Now = Times[K];
			C.Update(Now);
			const int Row = Order[K].first, Col = Order[K].second;
			const int Man = C.Find(Row % 2 ? 2 : 1, Row + 3);
			Step& St = Steps[static_cast<size_t>(Row)][static_cast<size_t>(Col)];
			Skinned(C, Man, St.At, St.Fa);
		}
		for (int Row = 0; Row < Rows; ++Row)
		{
			const int Man = C.Find(Row % 2 ? 2 : 1, Row + 3);
			const ManInfo& In = C.Info(Man);
			std::printf("falls row %d: %s %s, %s (%s, %s), face %d, helmet %s\n", Row, In.Army.c_str(), In.Role.c_str(), In.FallClip.c_str(),
				In.FallStance.c_str(), In.FallKind.c_str(), In.Face, In.Finish >= 0 ? C.SoldierOf(Man)->Finishes[static_cast<size_t>(In.Finish)].Name.c_str() : "own");
		}

		std::vector<uint8_t> Sheet(static_cast<size_t>(SheetW) * static_cast<size_t>(CellH * Rows) * 3, 236);
		// Seen from his right (south of him; he faces east), the sun low behind the eye on the left: his shadow falls
		// away from us across the earth.
		Light Lt = Daylight(215.0, 13.0);
		Lt.EarthA = Lin(118, 98, 76);
		Lt.EarthB = Lin(96, 80, 62);
		Lt.HazeK = 0.0;
		for (int Row = 0; Row < Rows; ++Row)
		{
			const int Man = C.Find(Row % 2 ? 2 : 1, Row + 3);
			// One framing for the row: around everything of him over the six steps.
			double XLo = 1e9, XHi = -1e9, YLo = 1e9, YHi = -1e9;
			for (const Step& St : Steps[static_cast<size_t>(Row)])
			{
				for (size_t K = 0; K < St.At.size(); K += 3)
				{
					XLo = std::min(XLo, static_cast<double>(St.At[K])); XHi = std::max(XHi, static_cast<double>(St.At[K]));
					YLo = std::min(YLo, static_cast<double>(St.At[K + 1])); YHi = std::max(YHi, static_cast<double>(St.At[K + 1]));
				}
			}
			// (Back far enough that a rifle thrown toward the eye stays in the picture.)
			const double Back = std::max(4.6, 4.6 - YLo * 1.6);
			const double YMid = 0.5 * (YLo + YHi);
			const double XMid = 0.5 * (XLo + XHi);
			for (int Col = 0; Col < Cols; ++Col)
			{
				const int OX = (SheetW * Col) / Cols, OX1 = (SheetW * (Col + 1)) / Cols;
				const int CW = OX1 - OX - 4, CH = CellH - 4;
				Scene Sc;
				const Step& St = Steps[static_cast<size_t>(Row)][static_cast<size_t>(Col)];
				Sc.AddMan(C.LookOf(Man, Near), St.At, St.Fa);
				Sc.AddGround(XMid - 8.0, -6.0, XMid + 8.0, 10.0, 0.5, Flat);
				// Wide enough for his whole fall and tall enough for him standing (2.1 m) at his distance.
				const double Span = std::max(XHi - XLo + 0.6, 2.25 * CW / CH);
				const double Fov = 2.0 * std::atan(Span * 0.5 / (Back + YMid)) * 180.0 / 3.14159265358979;
				const Camera Cam({ XMid, -Back, 1.45 }, { XMid, YMid, 0.7 }, Fov, CW * 2, CH * 2);
				ShadowMap Sh;
				Sh.Build(Sc, Lt.ToSun, { XMid - 3.0, -2.5, -0.1 }, { XMid + 3.0, 3.5, 2.0 }, 1536);
				const Picture Pic = Paint(Sc, Cam, Lt, Sh);
				Put(Pic, 1.0, Sheet, SheetW, OX + 2, Row * CellH + 2);
			}
		}
		if (!WritePpm(OutDir + "/falls.ppm", Sheet, SheetW, CellH * Rows)) { std::printf("cannot write falls.ppm\n"); return 1; }
		std::printf("wrote %s/falls.ppm (%dx%d)\n", OutDir.c_str(), SheetW, CellH * Rows);
	}

	// ---- A moment of the battle: the recording's men at one time, from a low eye.
	{
		const std::string RecText = ReadFile(Argv[1]);
		LandJson::Value Rec;
		if (!LandJson::Parse(RecText, Rec)) { std::printf("cannot read %s\n", Argv[1]); return 1; }
		struct Field
		{
			int Cols = 1, Rows = 1;
			double Cell = 25.0;
			std::vector<float> H;
			float At(int Cx, int Ry) const
			{
				Cx = std::max(0, std::min(Cols - 1, Cx));
				Ry = std::max(0, std::min(Rows - 1, Ry));
				return H[static_cast<size_t>(Ry) * static_cast<size_t>(Cols) + static_cast<size_t>(Cx)];
			}
		};
		auto Fd = std::make_shared<Field>();
		const LandJson::Value& F = Rec["field"];
		Fd->Cols = F["cols"].Int(120);
		Fd->Rows = F["rows"].Int(80);
		Fd->Cell = F["cell"].Num(25.0);
		for (size_t I = 0; I < F["height"].Count(); ++I) Fd->H.push_back(static_cast<float>(F["height"].At(I).Num()));
		const GroundFn GroundAt = [Fd](double X, double Y) {
			const double FX = X / Fd->Cell - 0.5, FY = Y / Fd->Cell - 0.5;
			const int C0 = static_cast<int>(std::floor(FX)), R0 = static_cast<int>(std::floor(FY));
			const double U = FX - C0, V = FY - R0;
			const double H00 = Fd->At(C0, R0), H10 = Fd->At(C0 + 1, R0), H01 = Fd->At(C0, R0 + 1), H11 = Fd->At(C0 + 1, R0 + 1);
			Ground G;
			G.Height = static_cast<float>(H00 * (1 - U) * (1 - V) + H10 * U * (1 - V) + H01 * (1 - U) * V + H11 * U * V);
			const double DX = ((H10 - H00) * (1 - V) + (H11 - H01) * V) / Fd->Cell, DY = ((H01 - H00) * (1 - U) + (H11 - H10) * U) / Fd->Cell;
			const double L = std::sqrt(DX * DX + DY * DY + 1.0);
			G.Normal = { static_cast<float>(-DX / L), static_cast<float>(-DY / L), static_cast<float>(1.0 / L) };
			return G;
		};
		Crowd C;
		for (const auto& KV : Files) C.AddSoldier(KV.first, KV.second);
		if (!C.ReadArmies(Armies, Err) || !C.Build(RecText, GroundAt, Err)) { std::printf("battle: %s\n", Err.c_str()); return 1; }

		// The '6th Infantry' block's firing line at 3 min 6.8 s, seen from its east end at a crouching man's eye, looking
		// west along it with a long lens: a rifleman hit 0.7 s before, going down on his knees with his rifle dropped,
		// one creeping up toward the eye, one prone at his rifle with a man cowering behind him, the gunner on the
		// right, and the next block's line on the skyline.
		double Mo[8] = { 186.8, 1700.0, 1058.0, 1.1, 1662.0, 1054.5, 0.5, 30.0 };
		if (const char* E = std::getenv("LAND_MEN_MOMENT"))
		{
			std::istringstream In(E);
			for (double& V : Mo) In >> V;
		}
		for (double T = 0.0; T < Mo[0]; T += 1.0 / 60.0) C.Update(T);
		C.Update(Mo[0]);
		const double EyeZ = GroundAt(Mo[1], Mo[2]).Height + Mo[3];
		const D3 Eye { Mo[1], Mo[2], EyeZ }, Look { Mo[4], Mo[5], GroundAt(Mo[4], Mo[5]).Height + Mo[6] };
		C.SetEye(Eye.X, Eye.Y, Eye.Z);
		Scene Sc;
		int Drawn = 0;
		double Lo[2] = { 1e18, 1e18 }, Hi[2] = { -1e18, -1e18 };
		const D3 Fwd = Unit(Look - Eye);
		for (int I = 0; I < C.Count(); ++I)
		{
			if (!C.Visible(I)) continue;
			double X, Y, Z;
			float Fc;
			C.PlaceOf(I, X, Y, Z, Fc);
			const D3 R { X - Eye.X, Y - Eye.Y, Z - Eye.Z };
			if (Dot(R, Fwd) < 0.5 || Dot(R, R) > 400.0 * 400.0) continue;
			std::vector<float> At, Fa;
			Skinned(C, I, At, Fa);
			Sc.AddMan(C.LookOf(I, Near), At, Fa);
			if (Dot(R, R) < 90.0 * 90.0)
			{
				// (The sun's map covers the men near the eye; beyond, a shadow is too small to see.)
				Lo[0] = std::min(Lo[0], X); Lo[1] = std::min(Lo[1], Y);
				Hi[0] = std::max(Hi[0], X); Hi[1] = std::max(Hi[1], Y);
			}
			const ManInfo& In = C.Info(I);
			const Playing Pl = C.PlayingOf(I);
			std::printf("battle: %s b%d k%d %s at %.0f m, %s%s\n", In.Army.c_str(), In.Block, In.K, In.Role.c_str(), std::sqrt(Dot(R, R)), Pl.Clip.c_str(),
				Pl.bShooting ? " (shooting)" : "");
			++Drawn;
		}
		// The ground about the men: fine near the eye, coarser beyond.
		const double Fine[4] = { Eye.X - 120.0, Eye.Y - 120.0, Eye.X + 120.0, Eye.Y + 120.0 };
		Sc.AddGround(Fine[0], Fine[1], Fine[2], Fine[3], 0.75, GroundAt);
		Sc.AddGround(Eye.X - 1600.0, Eye.Y - 1600.0, Eye.X + 1600.0, Eye.Y + 1600.0, 10.0, GroundAt, Fine);
		// July, half past nine in the morning in Normandy: the sun in the east-south-east, some 40 degrees up.
		Light Lt = Daylight(108.0, 38.0);
		Lt.EarthA = Lin(98, 104, 66);
		Lt.EarthB = Lin(112, 100, 70);
		Lt.HazeK = 1.0 / 900.0;
		ShadowMap Sh;
		Sh.Build(Sc, Lt.ToSun, { std::min(Lo[0], Eye.X) - 10.0, std::min(Lo[1], Eye.Y) - 10.0, EyeZ - 20.0 },
			{ std::max(Hi[0], Eye.X) + 10.0, std::max(Hi[1], Eye.Y) + 10.0, EyeZ + 10.0 }, 4096);
		const int W = 1600, H = 720;
		const Camera Cam(Eye, Look, Mo[7], W * 2, H * 2);
		const Picture Pic = Paint(Sc, Cam, Lt, Sh);
		std::vector<uint8_t> Out(static_cast<size_t>(W) * static_cast<size_t>(H) * 3);
		Put(Pic, 1.0, Out, W, 0, 0);
		if (!WritePpm(OutDir + "/battle.ppm", Out, W, H)) { std::printf("cannot write battle.ppm\n"); return 1; }
		std::printf("wrote %s/battle.ppm (%dx%d, %d men)\n", OutDir.c_str(), W, H, Drawn);
	}
	return 0;
}
