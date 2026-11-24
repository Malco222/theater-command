// A small software painter for the ground battle's picture sheet (Brian learns by looking): each vehicle cut and posed
// by the game's own code (Battle/LandKit's bodies and Vehicle::PinPose, so the picture shows exactly what the game will
// draw), painted with its own point colours, under a low morning sun with soft sky light, on plain earth. Three-quarter
// views. Writes one picture a panel (PPM) and a list of captions; land_sheet.py lays them out on the sheet.
// Usage: land_picture <Content folder> <out folder>

#include "Battle/LandKit.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace LandKit;

namespace
{
	constexpr double Pi = 3.14159265358979323846;
	constexpr double Rad = Pi / 180.0;

	bool ReadFile(const std::string& Path, std::string& Out)
	{
		std::ifstream F(Path, std::ios::binary);
		if (!F)
		{
			return false;
		}
		std::stringstream S;
		S << F.rdbuf();
		Out = S.str();
		return true;
	}

	double Lin(double C255)
	{
		const double C = C255 / 255.0;
		return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
	}
	double Clamp01(double X) { return X < 0.0 ? 0.0 : X > 1.0 ? 1.0 : X; }
	double Smooth(double E0, double E1, double X)
	{
		const double T = Clamp01((X - E0) / (E1 - E0));
		return T * T * (3.0 - 2.0 * T);
	}

	struct Colour
	{
		double R = 0.0, G = 0.0, B = 0.0;
		Colour() = default;
		Colour(double InR, double InG, double InB) : R(InR), G(InG), B(InB) {}
		Colour operator*(double S) const { return Colour(R * S, G * S, B * S); }
		Colour operator*(const Colour& O) const { return Colour(R * O.R, G * O.G, B * O.B); }
		Colour operator+(const Colour& O) const { return Colour(R + O.R, G + O.G, B + O.B); }
	};
	Colour Mix(const Colour& A, const Colour& B, double T) { return A * (1.0 - T) + B * T; }
	Colour FromPicture(double R, double G, double B) { return Colour(Lin(R), Lin(G), Lin(B)); }

	/** Value noise for the earth. */
	double Hash2(int64_t X, int64_t Y)
	{
		uint64_t H = static_cast<uint64_t>(X) * 0x9E3779B97F4A7C15ull ^ (static_cast<uint64_t>(Y) + 0x632BE59BD9B4E019ull) * 0xBF58476D1CE4E5B9ull;
		H ^= H >> 31;
		H *= 0xD6E8FEB86659FD93ull;
		H ^= H >> 32;
		return static_cast<double>(H >> 11) * (1.0 / 9007199254740992.0);
	}
	double Noise2(double X, double Y)
	{
		const double FX = std::floor(X), FY = std::floor(Y);
		const int64_t IX = static_cast<int64_t>(FX), IY = static_cast<int64_t>(FY);
		const double U = Smooth(0.0, 1.0, X - FX), V = Smooth(0.0, 1.0, Y - FY);
		const double A = Hash2(IX, IY), B = Hash2(IX + 1, IY), C = Hash2(IX, IY + 1), D = Hash2(IX + 1, IY + 1);
		return (A + (B - A) * U) * (1.0 - V) + (C + (D - C) * U) * V;
	}

	/** One triangle to draw: field points, normals and colours (linear) at its corners. */
	struct Tri
	{
		V3 P[3], N[3];
		Colour C[3];
	};

	/** What the light and the camera see. */
	struct Scene
	{
		std::vector<Tri> Tris;
		struct Box
		{
			double X0, Y0, X1, Y1;
		};
		std::vector<Box> Feet; // the ground footprints of bodies that touch the ground (the earth darkens under them)
		V3 Lo = V3(1e9, 1e9, 1e9), Hi = V3(-1e9, -1e9, -1e9);
	};

	/** A vehicle's bodies as triangles, at her pose, each point's colour taken toward burnt black by its soot weight. */
	void AddVehicle(const Vehicle& V, double SootShare, Scene& S)
	{
		const Model& M = V.Look();
		const Colour Burnt = FromPicture(M.BurntColour[0], M.BurntColour[1], M.BurntColour[2]);
		for (size_t B = 0; B < M.Bodies.size(); ++B)
		{
			const Body& Bo = M.Bodies[B];
			const Mesh& Me = Bo.Near;
			const Place& Pl = V.Bodies()[B];
			const size_t N = Me.Count();
			std::vector<V3> P(N), Nm(N);
			std::vector<Colour> C(N);
			V3 Lo(1e9, 1e9, 1e9), Hi(-1e9, -1e9, -1e9);
			for (size_t I = 0; I < N; ++I)
			{
				P[I] = Pl.Point(V3(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2]));
				Nm[I] = Pl.Way(V3(Me.Normals[I * 3], Me.Normals[I * 3 + 1], Me.Normals[I * 3 + 2]));
				const Colour Own = FromPicture(Me.Colours[I * 4], Me.Colours[I * 4 + 1], Me.Colours[I * 4 + 2]);
				C[I] = Mix(Own, Burnt, Clamp01(Me.Soot[I] * SootShare));
				Lo = V3(std::min(Lo.X, P[I].X), std::min(Lo.Y, P[I].Y), std::min(Lo.Z, P[I].Z));
				Hi = V3(std::max(Hi.X, P[I].X), std::max(Hi.Y, P[I].Y), std::max(Hi.Z, P[I].Z));
			}
			for (size_t T = 0; T + 2 < Me.Triangles.size(); T += 3)
			{
				Tri Out;
				for (int K = 0; K < 3; ++K)
				{
					const uint32_t I = Me.Triangles[T + static_cast<size_t>(K)];
					Out.P[K] = P[I];
					Out.N[K] = Nm[I];
					Out.C[K] = C[I];
				}
				S.Tris.push_back(Out);
			}
			if (N > 0)
			{
				S.Lo = V3(std::min(S.Lo.X, Lo.X), std::min(S.Lo.Y, Lo.Y), std::min(S.Lo.Z, Lo.Z));
				S.Hi = V3(std::max(S.Hi.X, Hi.X), std::max(S.Hi.Y, Hi.Y), std::max(S.Hi.Z, Hi.Z));
				if (Lo.Z < 0.3)
				{
					S.Feet.push_back({ Lo.X, Lo.Y, Hi.X, Hi.Y });
				}
			}
		}
	}

	/** The national marks as the lead will lay them: a flat decal at each place the kit gives (a white star for the
	 * Americans, a black cross edged white for the Germans). */
	void AddMarks(const Vehicle& V, Scene& S)
	{
		std::vector<MarkPlace> Marks;
		V.MarksNow(Marks);
		for (const MarkPlace& K : Marks)
		{
			if (!K.bOwner)
			{
				continue;
			}
			const V3 N = K.Facing.Unit();
			V3 Up = V3(0.0, 0.0, 1.0) - N * N.Z;
			if (Up.Length() < 0.3)
			{
				Up = V3(std::sin(V.Heading), std::cos(V.Heading), 0.0); // on a deck: the cross upright to her nose
			}
			Up = Up.Unit();
			const V3 Across = Up.Cross(N).Unit();
			auto At = [&](double A, double U, double Lift) { return K.At + N * Lift + Across * A + Up * U; };
			auto Quad = [&](double A0, double U0, double A1, double U1, double Lift, const Colour& C)
			{
				Tri T1, T2;
				const V3 P0 = At(A0, U0, Lift), P1 = At(A1, U0, Lift), P2 = At(A1, U1, Lift), P3 = At(A0, U1, Lift);
				T1.P[0] = P0; T1.P[1] = P1; T1.P[2] = P2;
				T2.P[0] = P0; T2.P[1] = P2; T2.P[2] = P3;
				for (int I = 0; I < 3; ++I)
				{
					T1.N[I] = N;
					T2.N[I] = N;
					T1.C[I] = C;
					T2.C[I] = C;
				}
				S.Tris.push_back(T1);
				S.Tris.push_back(T2);
			};
			const double R = K.Radius;
			const bool bAmerican = K.Tag == "USA" || (K.Tag == "_" && V.Nation == "USA");
			const bool bGerman = K.Tag == "GER" || (K.Tag == "_" && V.Nation == "GER");
			if (!bAmerican && !bGerman)
			{
				continue; // (no other nation's mark is drawn yet: the T-34's red star is not the Germans' cross)
			}
			if (bAmerican)
			{
				const Colour White = FromPicture(214, 211, 196);
				for (int I = 0; I < 10; ++I)
				{
					const double A0 = (I * 36.0) * Rad, A1 = ((I + 1) * 36.0) * Rad;
					const double R0 = I % 2 == 0 ? R : R * 0.382, R1 = I % 2 == 0 ? R * 0.382 : R;
					Tri T;
					T.P[0] = At(0.0, 0.0, 0.012);
					T.P[1] = At(R0 * std::sin(A0), R0 * std::cos(A0), 0.012);
					T.P[2] = At(R1 * std::sin(A1), R1 * std::cos(A1), 0.012);
					for (int J = 0; J < 3; ++J)
					{
						T.N[J] = N;
						T.C[J] = White;
					}
					S.Tris.push_back(T);
				}
			}
			else
			{
				const Colour White = FromPicture(206, 204, 194), Black = FromPicture(24, 24, 24);
				const double Wide = R * 0.36, Edge = R * 0.09;
				Quad(-R, -Wide, R, Wide, 0.012, White);
				Quad(-Wide, -R, Wide, R, 0.012, White);
				Quad(-R + Edge, -Wide + Edge, R - Edge, Wide - Edge, 0.016, Black);
				Quad(-Wide + Edge, -R + Edge, Wide - Edge, R - Edge, 0.016, Black);
			}
		}
	}

	struct Panel
	{
		std::string File, Caption;
		std::string ModelName, Nation;
		Vehicle::Pin Pin;
		double SootShare = 0.0;
		double CameraBearing = 40.0, CameraUp = 16.0; // degrees: from her toward the camera (0 ahead of her), and above
		bool bFullRecoil = false;                      // find the moment the barrel is fully back
	};

	/** The sun's shadow: depth toward the sun, an orthographic map over everything that casts. */
	struct ShadowMap
	{
		V3 L, A, B;            // toward the sun, and the map's two ways
		double A0 = 0, B0 = 0, Texel = 0.01;
		int Size = 2048;
		std::vector<float> Depth;

		void Build(const Scene& S, const V3& Sun)
		{
			L = Sun.Unit();
			A = V3(0.0, 0.0, 1.0).Cross(L).Unit(V3(1.0, 0.0, 0.0));
			B = L.Cross(A).Unit();
			double AMin = 1e9, AMax = -1e9, BMin = 1e9, BMax = -1e9;
			for (const Tri& T : S.Tris)
			{
				for (const V3& P : T.P)
				{
					AMin = std::min(AMin, P.Dot(A));
					AMax = std::max(AMax, P.Dot(A));
					BMin = std::min(BMin, P.Dot(B));
					BMax = std::max(BMax, P.Dot(B));
				}
			}
			A0 = AMin - 0.2;
			B0 = BMin - 0.2;
			Texel = std::max(AMax - AMin, BMax - BMin) / (Size - 50);
			Depth.assign(static_cast<size_t>(Size) * Size, -1e9f);
			for (const Tri& T : S.Tris)
			{
				double X[3], Y[3], D[3];
				for (int K = 0; K < 3; ++K)
				{
					X[K] = (T.P[K].Dot(A) - A0) / Texel;
					Y[K] = (T.P[K].Dot(B) - B0) / Texel;
					D[K] = T.P[K].Dot(L);
				}
				const int X0 = std::max(0, static_cast<int>(std::floor(std::min({ X[0], X[1], X[2] }))));
				const int X1 = std::min(Size - 1, static_cast<int>(std::ceil(std::max({ X[0], X[1], X[2] }))));
				const int Y0 = std::max(0, static_cast<int>(std::floor(std::min({ Y[0], Y[1], Y[2] }))));
				const int Y1 = std::min(Size - 1, static_cast<int>(std::ceil(std::max({ Y[0], Y[1], Y[2] }))));
				const double Area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
				if (std::fabs(Area) < 1e-12)
				{
					continue;
				}
				for (int Py = Y0; Py <= Y1; ++Py)
				{
					for (int Px = X0; Px <= X1; ++Px)
					{
						const double Cx = Px + 0.5, Cy = Py + 0.5;
						const double W0 = ((X[1] - Cx) * (Y[2] - Cy) - (X[2] - Cx) * (Y[1] - Cy)) / Area;
						const double W1 = ((X[2] - Cx) * (Y[0] - Cy) - (X[0] - Cx) * (Y[2] - Cy)) / Area;
						const double W2 = 1.0 - W0 - W1;
						if (W0 < -1e-9 || W1 < -1e-9 || W2 < -1e-9)
						{
							continue;
						}
						float& Cell = Depth[static_cast<size_t>(Py) * Size + Px];
						Cell = std::max(Cell, static_cast<float>(W0 * D[0] + W1 * D[1] + W2 * D[2]));
					}
				}
			}
		}

		/** How much sun reaches P (0 to 1): a few samples round it, for a soft edge. */
		double Lit(const V3& P, double Bias) const
		{
			const double X = (P.Dot(A) - A0) / Texel, Y = (P.Dot(B) - B0) / Texel, D = P.Dot(L);
			double Sum = 0.0;
			int Count = 0;
			for (int J = -2; J <= 2; ++J)
			{
				for (int I = -2; I <= 2; ++I)
				{
					const int Px = static_cast<int>(std::floor(X + I * 1.2)), Py = static_cast<int>(std::floor(Y + J * 1.2));
					++Count;
					if (Px < 0 || Py < 0 || Px >= Size || Py >= Size)
					{
						Sum += 1.0;
						continue;
					}
					Sum += D + Bias >= Depth[static_cast<size_t>(Py) * Size + Px] ? 1.0 : 0.0;
				}
			}
			return Sum / Count;
		}
	};

	bool Paint(const Scene& S, const Panel& P, const Vehicle& V, const std::string& Path)
	{
		const int Out = 3; // supersampled three times each way
		const int W = 560 * Out, H = 420 * Out;
		// The low sun of a July morning in Normandy, from the camera's right (fifteen degrees up).
		const double SunBearing = (P.CameraBearing + 62.0) * Rad, SunUp = 15.0 * Rad;
		const V3 Sun(std::sin(SunBearing) * std::cos(SunUp), std::cos(SunBearing) * std::cos(SunUp), std::sin(SunUp));
		ShadowMap Shadow;
		Shadow.Build(S, Sun);

		// The camera: three-quarter view, framed on her.
		const double Fov = 28.0 * Rad;
		const double F = (H * 0.5) / std::tan(Fov * 0.5);
		const double Bearing = (P.CameraBearing) * Rad + V.Heading, Up = P.CameraUp * Rad;
		const V3 Toward(std::sin(Bearing) * std::cos(Up), std::cos(Bearing) * std::cos(Up), std::sin(Up));
		V3 Target = (S.Lo + S.Hi) * 0.5;
		double Distance = 20.0;
		V3 Cam, Fwd, Right, CamUp;
		auto Frame = [&]()
		{
			Cam = Target + Toward * Distance;
			Fwd = (Target - Cam).Unit();
			Right = Fwd.Cross(V3(0.0, 0.0, 1.0)).Unit();
			CamUp = Right.Cross(Fwd).Unit();
		};
		for (int Pass = 0; Pass < 4; ++Pass)
		{
			Frame();
			double X0 = 1e9, X1 = -1e9, Y0 = 1e9, Y1 = -1e9;
			for (const Tri& T : S.Tris)
			{
				for (const V3& Q : T.P)
				{
					const V3 D = Q - Cam;
					const double Z = D.Dot(Fwd);
					const double X = D.Dot(Right) / Z, Y = D.Dot(CamUp) / Z;
					X0 = std::min(X0, X);
					X1 = std::max(X1, X);
					Y0 = std::min(Y0, Y);
					Y1 = std::max(Y1, Y);
				}
			}
			const double Half = std::tan(Fov * 0.5);
			const double Fill = std::max((X1 - X0) / (2.0 * Half * W / H), (Y1 - Y0) / (2.0 * Half)) / 0.80;
			Target = Target + Right * ((X0 + X1) * 0.5 * Distance) + CamUp * ((Y0 + Y1) * 0.5 * Distance);
			Distance *= Fill;
		}
		Frame();

		// The vehicle drawn into a depth, a normal, a colour and a place for every pixel.
		const size_t Count = static_cast<size_t>(W) * H;
		std::vector<float> Depth(Count, 1e30f);
		std::vector<V3> Normal(Count), Where(Count);
		std::vector<Colour> Albedo(Count);
		for (const Tri& T : S.Tris)
		{
			double X[3], Y[3], Z[3];
			bool bBehind = false;
			for (int K = 0; K < 3; ++K)
			{
				const V3 D = T.P[K] - Cam;
				Z[K] = D.Dot(Fwd);
				if (Z[K] < 0.1)
				{
					bBehind = true;
				}
				X[K] = W * 0.5 + D.Dot(Right) / Z[K] * F;
				Y[K] = H * 0.5 - D.Dot(CamUp) / Z[K] * F;
			}
			if (bBehind)
			{
				continue;
			}
			const double Area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
			if (std::fabs(Area) < 1e-12)
			{
				continue;
			}
			const int X0 = std::max(0, static_cast<int>(std::floor(std::min({ X[0], X[1], X[2] }))));
			const int X1 = std::min(W - 1, static_cast<int>(std::ceil(std::max({ X[0], X[1], X[2] }))));
			const int Y0 = std::max(0, static_cast<int>(std::floor(std::min({ Y[0], Y[1], Y[2] }))));
			const int Y1 = std::min(H - 1, static_cast<int>(std::ceil(std::max({ Y[0], Y[1], Y[2] }))));
			for (int Py = Y0; Py <= Y1; ++Py)
			{
				for (int Px = X0; Px <= X1; ++Px)
				{
					const double Cx = Px + 0.5, Cy = Py + 0.5;
					double B0 = ((X[1] - Cx) * (Y[2] - Cy) - (X[2] - Cx) * (Y[1] - Cy)) / Area;
					double B1 = ((X[2] - Cx) * (Y[0] - Cy) - (X[0] - Cx) * (Y[2] - Cy)) / Area;
					double B2 = 1.0 - B0 - B1;
					if (B0 < -1e-9 || B1 < -1e-9 || B2 < -1e-9)
					{
						continue;
					}
					// (perspective-correct: weights over each corner's depth)
					const double Q0 = B0 / Z[0], Q1 = B1 / Z[1], Q2 = B2 / Z[2];
					const double Sum = Q0 + Q1 + Q2;
					const double Zp = 1.0 / Sum;
					const size_t I = static_cast<size_t>(Py) * W + Px;
					if (Zp >= Depth[I])
					{
						continue;
					}
					B0 = Q0 * Zp;
					B1 = Q1 * Zp;
					B2 = Q2 * Zp;
					Depth[I] = static_cast<float>(Zp);
					Normal[I] = T.N[0] * B0 + T.N[1] * B1 + T.N[2] * B2;
					Where[I] = T.P[0] * B0 + T.P[1] * B1 + T.P[2] * B2;
					Albedo[I] = T.C[0] * B0 + T.C[1] * B1 + T.C[2] * B2;
				}
			}
		}

		// Light: the sun (warm, low), the sky (cool, all round), the earth's bounce; plain earth and the morning haze.
		const Colour SunLight = Colour(1.0, 0.84, 0.64) * 2.7;
		const Colour SkyLight = Colour(0.50, 0.60, 0.80) * 0.62;
		const Colour Bounce = Colour(0.36, 0.30, 0.22) * 0.30;
		// (a damp brown loam, the patches faint, so the ground reads as plain earth and not as sand)
		const Colour Earth = FromPicture(102, 90, 72), EarthDark = FromPicture(90, 79, 63), Haze = FromPicture(196, 192, 182);
		std::vector<Colour> Lit(Count);
		for (int Py = 0; Py < H; ++Py)
		{
			for (int Px = 0; Px < W; ++Px)
			{
				const size_t I = static_cast<size_t>(Py) * W + Px;
				const V3 Ray = (Fwd + Right * ((Px + 0.5 - W * 0.5) / F) - CamUp * ((Py + 0.5 - H * 0.5) / F)).Unit();
				double GroundZ = 1e30;
				V3 Ground;
				if (Ray.Z < -1e-6)
				{
					const double T = -Cam.Z / Ray.Z;
					Ground = Cam + Ray * T;
					GroundZ = T * Ray.Dot(Fwd);
				}
				if (Depth[I] < GroundZ)
				{
					V3 N = Normal[I].Unit();
					if (N.Dot(Ray) > 0.0)
					{
						N = -N;
					}
					const V3 Pt = Where[I];
					const double Sunny = std::max(0.0, N.Dot(Shadow.L)) * Shadow.Lit(Pt + N * 0.015, 0.02);
					const double Sky = 0.55 + 0.45 * N.Z;
					const double Low = 0.72 + 0.28 * Smooth(0.0, 0.9, Pt.Z); // the running gear sits in the shade of the hull
					Lit[I] = Albedo[I] * (SunLight * Sunny + SkyLight * (Sky * Low) + Bounce * (0.5 - 0.5 * N.Z));
				}
				else if (GroundZ < 1e29)
				{
					const double Patch = Noise2(Ground.X * 0.35, Ground.Y * 0.35) * 0.6 + Noise2(Ground.X * 1.3, Ground.Y * 1.3) * 0.4;
					const double Grain = 0.9 + 0.2 * Noise2(Ground.X * 9.0, Ground.Y * 9.0);
					const Colour Soil = Mix(Earth, EarthDark, Smooth(0.25, 0.85, Patch)) * Grain;
					double Under = 1.0;
					for (const Scene::Box& B : S.Feet)
					{
						const double DX = std::max({ B.X0 - Ground.X, 0.0, Ground.X - B.X1 });
						const double DY = std::max({ B.Y0 - Ground.Y, 0.0, Ground.Y - B.Y1 });
						const double D = std::sqrt(DX * DX + DY * DY);
						Under = std::min(Under, 1.0 - 0.55 * std::exp(-(D / 0.55) * (D / 0.55)));
					}
					const double Sunny = Shadow.L.Z * Shadow.Lit(Ground, 0.03);
					Colour C = Soil * (SunLight * Sunny + SkyLight * Under);
					const double Far = (Ground - Cam).Length();
					C = Mix(C, Haze * 1.2, 1.0 - std::exp(-Far / 900.0));
					Lit[I] = C;
				}
				else
				{
					Lit[I] = Haze * 1.25;
				}
			}
		}

		// Into the picture: exposed, a gentle film curve, sRGB, every 3 x 3 pixels made one.
		const int OW = W / Out, OH = H / Out;
		std::vector<uint8_t> Picture(static_cast<size_t>(OW) * OH * 3);
		auto Curve = [](double X)
		{
			X *= 0.62;
			const double Y = (X * (2.51 * X + 0.03)) / (X * (2.43 * X + 0.59) + 0.14);
			const double C = Clamp01(Y);
			return C <= 0.0031308 ? C * 12.92 : 1.055 * std::pow(C, 1.0 / 2.4) - 0.055;
		};
		for (int Y = 0; Y < OH; ++Y)
		{
			for (int X = 0; X < OW; ++X)
			{
				double R = 0.0, G = 0.0, B = 0.0;
				for (int J = 0; J < Out; ++J)
				{
					for (int I = 0; I < Out; ++I)
					{
						const Colour& C = Lit[static_cast<size_t>(Y * Out + J) * W + (X * Out + I)];
						R += Curve(C.R);
						G += Curve(C.G);
						B += Curve(C.B);
					}
				}
				const double K = 255.0 / (Out * Out);
				uint8_t* Px = &Picture[(static_cast<size_t>(Y) * OW + X) * 3];
				Px[0] = static_cast<uint8_t>(std::lround(std::min(255.0, R * K)));
				Px[1] = static_cast<uint8_t>(std::lround(std::min(255.0, G * K)));
				Px[2] = static_cast<uint8_t>(std::lround(std::min(255.0, B * K)));
			}
		}
		std::FILE* File = std::fopen(Path.c_str(), "wb");
		if (!File)
		{
			return false;
		}
		std::fprintf(File, "P6\n%d %d\n255\n", OW, OH);
		std::fwrite(Picture.data(), 1, Picture.size(), File);
		std::fclose(File);
		return true;
	}
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::printf("usage: land_picture <Content folder> <out folder>\n");
		return 2;
	}
	const std::string Content = argv[1], OutDir = argv[2];
	const GroundFn Flat = [](double, double, double& OutZ, V3& OutNormal)
	{
		OutZ = 0.0;
		OutNormal = V3(0.0, 0.0, 1.0);
	};
	std::map<std::string, std::unique_ptr<Model>> Models;
	auto ModelFor = [&](const std::string& Name, const std::string& Nation) -> const Model*
	{
		const std::string Key = Name + "|" + Nation;
		if (!Models.count(Key))
		{
			std::string Text, Error;
			if (!ReadFile(Content + "/Models/Battle/" + Name + ".json", Text))
			{
				std::printf("cannot read %s.json\n", Name.c_str());
				return nullptr;
			}
			auto M = std::make_unique<Model>();
			Dress D;
			D.Nation = Nation;
			if (!M->Load(Text, D, Error))
			{
				std::printf("%s: %s\n", Name.c_str(), Error.c_str());
				return nullptr;
			}
			Models[Key] = std::move(M);
		}
		return Models[Key].get();
	};

	std::vector<Panel> Panels;
	struct TankLook
	{
		const char* Model;
		const char* Nation;
		const char* Title;
	};
	const TankLook Tanks[3] = { { "Sherman", "USA", "M4 Sherman" }, { "PanzerIV", "GER", "Panzer IV" }, { "Tiger", "GER", "Tiger" } };
	for (const TankLook& T : Tanks)
	{
		Panel A;
		A.File = std::string(T.Model) + "_a";
		A.ModelName = T.Model;
		A.Nation = T.Nation;
		A.Pin.Turret = -40.0;
		A.Pin.Hatches = 1.0;
		A.CameraBearing = 42.0;
		A.CameraUp = 17.0;
		A.Caption = std::string(T.Title) + ": turret trained 40\xC2\xB0 off the hull, hatches open";
		Panels.push_back(A);
		Panel B;
		B.File = std::string(T.Model) + "_b";
		B.ModelName = T.Model;
		B.Nation = T.Nation;
		B.bFullRecoil = true;
		B.CameraBearing = 72.0;
		B.CameraUp = 15.0;
		B.Caption = std::string(T.Title) + ": the frame after a shot, barrel fully back";
		Panels.push_back(B);
		Panel C;
		C.File = std::string(T.Model) + "_c";
		C.ModelName = T.Model;
		C.Nation = T.Nation;
		C.Pin.Turret = -14.0;
		C.Pin.DeathAge = 3.5;
		C.SootShare = 0.35;
		C.CameraBearing = 36.0;
		C.CameraUp = 18.0;
		C.Caption = std::string(T.Title) + ": knocked out, hatches thrown, gun drooped";
		Panels.push_back(C);
	}
	{
		Panel D;
		D.File = "Tiger_d";
		D.ModelName = "Tiger";
		D.Nation = "GER";
		D.Pin.DeathAge = 4.5;
		D.Pin.bBrewUp = true;
		D.SootShare = 1.0;
		D.CameraBearing = -48.0;
		D.CameraUp = 20.0;
		D.Caption = "Tiger: brewed up and burnt out, her turret thrown beside her";
		Panels.push_back(D);
		Panel G;
		G.File = "Howitzer105";
		G.ModelName = "Howitzer105";
		G.Nation = "USA";
		G.Pin.Gun = 35.0;
		G.CameraBearing = 62.0;
		G.CameraUp = 14.0;
		G.Caption = "105 mm howitzer M2A1: deployed, trails spread, laid at 35\xC2\xB0";
		Panels.push_back(G);
		Panel L;
		L.File = "leFH18";
		L.ModelName = "leFH18";
		L.Nation = "GER";
		L.Pin.Gun = 35.0;
		L.CameraBearing = 62.0;
		L.CameraUp = 14.0;
		L.Caption = "10.5 cm leFH 18: deployed, trails spread, laid at 35\xC2\xB0";
		Panels.push_back(L);
	}

	std::FILE* Captions = std::fopen((OutDir + "/captions.txt").c_str(), "wb");
	if (!Captions)
	{
		std::printf("cannot write to %s\n", OutDir.c_str());
		return 1;
	}
	for (Panel& P : Panels)
	{
		const Model* M = ModelFor(P.ModelName, P.Nation);
		if (!M)
		{
			return 1;
		}
		Vehicle V;
		if (P.bFullRecoil)
		{
			// The moment of the fire clip the barrel is furthest back.
			double Best = 0.0, BestAge = 0.0;
			for (double Age = 0.0; Age <= 0.6; Age += 0.005)
			{
				P.Pin.FireAge = Age;
				V.PinPose(*M, P.Pin, Flat);
				if (V.Recoil > Best)
				{
					Best = V.Recoil;
					BestAge = Age;
				}
			}
			P.Pin.FireAge = BestAge;
			std::printf("  %s: barrel fully back %.2f m at %.3f s after the shot\n", P.ModelName.c_str(), Best, BestAge);
		}
		V.PinPose(*M, P.Pin, Flat);
		Scene S;
		AddVehicle(V, P.SootShare, S);
		const size_t Unmarked = S.Tris.size();
		AddMarks(V, S);
		// (a fire chars the marks with the paint round them)
		const Colour Burnt = FromPicture(M->BurntColour[0], M->BurntColour[1], M->BurntColour[2]);
		for (size_t I = Unmarked; I < S.Tris.size(); ++I)
		{
			for (Colour& C : S.Tris[I].C)
			{
				C = Mix(C, Burnt, Clamp01(0.85 * P.SootShare));
			}
		}
		const std::string Path = OutDir + "/" + P.File + ".ppm";
		if (!Paint(S, P, V, Path))
		{
			std::printf("cannot write %s\n", Path.c_str());
			return 1;
		}
		std::fprintf(Captions, "%s\t%s\n", P.File.c_str(), P.Caption.c_str());
		std::printf("  %s (%zu triangles)\n", Path.c_str(), S.Tris.size());
	}
	std::fclose(Captions);
	return 0;
}
