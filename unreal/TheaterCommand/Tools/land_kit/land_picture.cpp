// A small software painter for the ground battle's picture sheet (Brian learns by looking): each vehicle cut and posed
// by the game's own code (Battle/LandKit's bodies and Vehicle::PinPose, so the picture shows exactly what the game will
// draw), painted with its own point colours, under a low morning sun with soft sky light, on plain earth. Three-quarter
// views. Writes one picture a panel (PPM) and a list of captions; land_sheet.py lays them out on the sheet.
// Usage: land_picture <Content folder> <out folder>

#include "Battle/LandHouse.h"
#include "Battle/LandKit.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
		// National marks (release 2, LandKit's Dress::bMarks): painted on her skin by the game's own code, in her owner's
		// period marking for the month (the British flash, then star); bClose looks at the mark that faces the camera most,
		// close enough that it fills nearly half the picture's height.
		bool bMarks = false, bClose = false;
		int CloseAmong = 99; // (the close look picks among her first this many marks: the Tiger's hull crosses, not her turret's)
		int Year = 1944, Month = 7;
		// A farmhouse's panel instead (release 2, LandHouse, LAND_KIT_HOUSE=1): its damage (0 whole to 3 a shell), its
		// level (0 near, 1 middle, 2 far), roof and look; the sun's own bearing where given; a close look aimed at a point
		// from a way (bAim).
		int House = -1, HouseLevel = 0, HouseRoof = 0;
		int HouseKind = 0; // (0 the farmhouse, 1 its barn, 2 its byre: LAND_KIT_HOUSE=farm)
		uint64_t HouseKey = 1;
		double SunBearing = -1000.0;
		bool bAim = false;
		V3 AimAt, AimFrom;
		double AimDistance = 5.0;
	};

	/** A farmhouse's level as triangles, its floor FloorUp over the plain earth. */
	void AddHouse(const LandField::Mesh& M, double FloorUp, Scene& S)
	{
		for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
		{
			Tri Out;
			for (int K = 0; K < 3; ++K)
			{
				const size_t I = M.Triangles[T + static_cast<size_t>(K)];
				Out.P[K] = V3(M.Positions[I * 3], M.Positions[I * 3 + 1], M.Positions[I * 3 + 2] + FloorUp);
				Out.N[K] = V3(M.Normals[I * 3], M.Normals[I * 3 + 1], M.Normals[I * 3 + 2]);
				Out.C[K] = FromPicture(M.Colours[I].R, M.Colours[I].G, M.Colours[I].B);
				// (only what stands above the earth frames the picture)
				if (Out.P[K].Z > -0.05)
				{
					S.Lo = V3(std::min(S.Lo.X, Out.P[K].X), std::min(S.Lo.Y, Out.P[K].Y), std::min(S.Lo.Z, Out.P[K].Z));
					S.Hi = V3(std::max(S.Hi.X, Out.P[K].X), std::max(S.Hi.Y, Out.P[K].Y), std::max(S.Hi.Z, Out.P[K].Z));
				}
			}
			S.Tris.push_back(Out);
		}
	}

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
		const double SunBearing = (P.SunBearing > -999.0 ? P.SunBearing : P.CameraBearing + 62.0) * Rad, SunUp = 15.0 * Rad;
		const V3 Sun(std::sin(SunBearing) * std::cos(SunUp), std::cos(SunBearing) * std::cos(SunUp), std::sin(SunUp));
		ShadowMap Shadow;
		Shadow.Build(S, Sun);

		// The camera: three-quarter view, framed on her.
		const double Fov = 28.0 * Rad;
		const double F = (H * 0.5) / std::tan(Fov * 0.5);
		const double Bearing = (P.CameraBearing) * Rad + V.Heading, Up = P.CameraUp * Rad;
		V3 Toward(std::sin(Bearing) * std::cos(Up), std::cos(Bearing) * std::cos(Up), std::sin(Up));
		V3 Target = (S.Lo + S.Hi) * 0.5;
		double Distance = 20.0;
		// (a close look at one mark: the one facing the camera most, seen a little from her quarter and above)
		std::vector<MarkPlace> Places;
		V.MarksNow(Places);
		const MarkPlace* Close = nullptr;
		for (const MarkPlace& K : Places)
		{
			if (P.bClose && &K - Places.data() < P.CloseAmong && (!Close || K.Facing.Unit().Dot(Toward) > Close->Facing.Unit().Dot(Toward)))
			{
				Close = &K;
			}
		}
		if (P.bAim)
		{
			Toward = P.AimFrom.Unit();
			Target = P.AimAt;
			Distance = P.AimDistance;
		}
		else if (Close)
		{
			Toward = (Close->Facing.Unit() * 0.85 + Toward * 0.3 + V3(0.0, 0.0, 0.2)).Unit();
			Target = Close->At;
			Distance = Close->Radius / (0.42 * std::tan(Fov * 0.5));
		}
		V3 Cam, Fwd, Right, CamUp;
		auto Frame = [&]()
		{
			Cam = Target + Toward * Distance;
			Fwd = (Target - Cam).Unit();
			Right = Fwd.Cross(V3(0.0, 0.0, 1.0)).Unit();
			CamUp = Right.Cross(Fwd).Unit();
		};
		for (int Pass = 0; Pass < (Close || P.bAim ? 0 : 4); ++Pass)
		{
			Frame();
			double X0 = 1e9, X1 = -1e9, Y0 = 1e9, Y1 = -1e9;
			for (const Tri& T : S.Tris)
			{
				for (const V3& Q : T.P)
				{
					if (Q.Z < -0.05)
					{
						continue; // (what is under the earth, a plinth's footing, a heap's rim)
					}
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
		// (LAND_KIT_CAMERA=1: where the camera stood and what it looked at, beside the picture, for measuring it)
		if (std::getenv("LAND_KIT_CAMERA"))
		{
			if (std::FILE* Cf = std::fopen((Path + ".camera").c_str(), "wb"))
			{
				std::fprintf(Cf, "%.6f %.6f %.6f %.6f %.6f %.6f\n", Cam.X, Cam.Y, Cam.Z, Target.X, Target.Y, Target.Z);
				std::fclose(Cf);
			}
		}

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

namespace
{
	/** A painted panel read back (P6, 8 bits), for laying two side by side. */
	bool ReadPicture(const std::string& Path, int& W, int& H, std::vector<uint8_t>& Out)
	{
		std::FILE* File = std::fopen(Path.c_str(), "rb");
		if (!File)
		{
			return false;
		}
		int Most = 0;
		const bool bOk = std::fscanf(File, "P6 %d %d %d", &W, &H, &Most) == 3 && Most == 255 && std::fgetc(File) != EOF && W > 0 && H > 0;
		if (bOk)
		{
			Out.resize(static_cast<size_t>(W) * H * 3);
		}
		const bool bRead = bOk && std::fread(Out.data(), 1, Out.size(), File) == Out.size();
		std::fclose(File);
		return bRead;
	}
	bool WritePicture(const std::string& Path, int W, int H, const std::vector<uint8_t>& Pixels)
	{
		std::FILE* File = std::fopen(Path.c_str(), "wb");
		if (!File)
		{
			return false;
		}
		std::fprintf(File, "P6\n%d %d\n255\n", W, H);
		std::fwrite(Pixels.data(), 1, Pixels.size(), File);
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
	auto ModelFor = [&](const std::string& Name, const std::string& Nation, const Panel& For) -> const Model*
	{
		const std::string Key = Name + "|" + Nation + (For.bMarks ? "|marks " + std::to_string(For.Year * 100 + For.Month) : std::string());
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
			D.bMarks = For.bMarks;
			D.Year = For.Year;
			D.Month = For.Month;
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
	// (LAND_KIT_MARKS=1: the national marks' own sheet instead, release 2's -TheaterLandMarks)
	const bool bMarksSheet = std::getenv("LAND_KIT_MARKS") != nullptr;
	struct MarkLook
	{
		const char* File;
		const char* Model;
		const char* Nation;
		int Year, Month;
		bool bClose;
		double Bearing, Up, Soot, DeathAge;
		const char* Caption;
		int CloseAmong = 99;
	};
	const MarkLook MarkLooks[] = {
		{ "marks_sherman", "Sherman", "USA", 1944, 7, false, 42.0, 15.0, 0.0, -1.0, "M4 Sherman, American, July 1944: the white star, hull and turret" },
		{ "marks_sherman_close", "Sherman", "USA", 1944, 7, true, 42.0, 15.0, 0.0, -1.0, "the Sherman's hull star, close" },
		{ "marks_tiger", "Tiger", "GER", 1944, 7, false, -128.0, 16.0, 0.0, -1.0, "Tiger, German, July 1944: the cross on her hull's sides only" },
		{ "marks_tiger_close", "Tiger", "GER", 1944, 7, true, -128.0, 12.0, 0.0, -1.0, "the Tiger's hull cross, close", 2 },
		{ "marks_t34", "T34", "SOV", 1943, 7, false, 42.0, 16.0, 0.0, -1.0, "T-34, Soviet, July 1943: the plain red star on the turret" },
		{ "marks_t34_close", "T34", "SOV", 1943, 7, true, 42.0, 14.0, 0.0, -1.0, "the T-34's red star on her sloped turret" },
		{ "marks_sexton", "Sexton", "UK", 1944, 7, false, 42.0, 15.0, 0.0, -1.0, "Sexton, British, July 1944: the Allies' white star" },
		{ "marks_light_uk_1942", "LightTank", "UK", 1942, 5, true, 42.0, 14.0, 0.0, -1.0, "light tank, British, May 1942: the white-red-white flash" },
		{ "marks_semovente", "Semovente", "ITA", 1942, 10, true, 42.0, 14.0, 0.0, -1.0, "Semovente, Italian, October 1942: the company's red patch" },
		{ "marks_light_jap", "LightTank", "JAP", 1942, 1, true, -30.0, 14.0, 0.0, -1.0, "light tank, Japanese, January 1942: the star on her front" },
		{ "marks_jeep", "Jeep", "USA", 1944, 7, false, 42.0, 22.0, 0.0, -1.0, "jeep, American: bonnet and side stars" },
		{ "marks_truck", "Truck", "GER", 1944, 7, true, 42.0, 14.0, 0.0, -1.0, "the lorry in German hands: the cross on her door" },
		{ "marks_panther", "Panther", "GER", 1944, 7, false, 42.0, 16.0, 0.0, -1.0, "Panther, German, July 1944: the cross on her hull's sides" },
		{ "marks_panther_close", "Panther", "GER", 1944, 7, true, 42.0, 12.0, 0.0, -1.0, "the Panther's hull cross, close" },
		{ "marks_stug", "StuG3", "GER", 1943, 7, false, 42.0, 15.0, 0.0, -1.0, "StuG III, German, July 1943: the hull's crosses" },
	};
	for (const MarkLook& K : MarkLooks)
	{
		if (!bMarksSheet)
		{
			break;
		}
		Panel P;
		P.File = K.File;
		P.ModelName = K.Model;
		P.Nation = K.Nation;
		P.Year = K.Year;
		P.Month = K.Month;
		P.bMarks = true;
		P.bClose = K.bClose;
		P.CameraBearing = K.Bearing;
		P.CameraUp = K.Up;
		P.SootShare = K.Soot;
		P.Pin.DeathAge = K.DeathAge;
		P.Caption = K.Caption;
		P.CloseAmong = K.CloseAmong;
		Panels.push_back(P);
	}
	// (LAND_KIT_HOUSE=1: the farmhouse pilot's own sheet instead, release 2's -TheaterLandTrial=houses; LAND_KIT_HOUSE=farm
	// its barn's and byre's)
	const bool bHouseSheet = std::getenv("LAND_KIT_HOUSE") != nullptr;
	const bool bFarmSheet = bHouseSheet && std::string(std::getenv("LAND_KIT_HOUSE")) == "farm";
	auto HouseSpec = [](const Panel& P)
	{
		LandHouse::Spec H;
		// (the bocage's farmstead's three, as LandField lays them)
		H.Kind = P.HouseKind;
		H.Long = P.HouseKind == 1 ? 15.0 : P.HouseKind == 2 ? 13.0 : 14.0;
		H.Deep = P.HouseKind == 1 ? 8.4 : P.HouseKind == 2 ? 6.8 : 7.2;
		H.Storeys = P.HouseKind == 0 ? 2 : 1;
		H.Roof = P.HouseRoof;
		H.Hurt = P.House;
		H.Key = P.HouseKey;
		return H;
	};
	if (bHouseSheet)
	{
		struct HouseLook
		{
			const char* File;
			int Hurt, Level, Roof;
			uint64_t Key;
			double Bearing, Up, Sun;
			int Aim; // -1 the whole house; 0 a ground-floor window of the front; 1 the door; 2 the chimney and the eaves;
			         // 3 the whole house from AimDistance (the level switches' views, painted as the game shows them)
			double AimDistance;
			const char* Caption;
			int Kind; // 0 the farmhouse, 1 its barn, 2 its byre
		};
		const HouseLook HouseLooks[] = {
			{ "house_slate", 0, 0, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "Norman farmhouse: stone by stone, slate by slate", 0 },
			{ "house_slate_back", 0, 0, 0, 7, 140.0, 12.0, 210.0, -1, 0.0, "from behind: gutter, downpipe, water butt", 0 },
			{ "house_tile", 0, 0, 1, 2, 34.0, 12.0, 110.0, -1, 0.0, "another: cream limestone, flat clay tiles", 0 },
			{ "house_window", 0, 0, 0, 7, -24.0, 4.0, 62.0, 0, 3.6, "close: quoins, sill, casements, shutters", 0 },
			{ "house_door", 0, 0, 0, 7, 20.0, 6.0, 80.0, 1, 5.6, "close: the door, its keystone, fanlight and step", 0 },
			{ "house_eaves", 0, 0, 0, 7, -30.0, 6.0, 60.0, 2, 7.5, "close: chimney, ridge tiles, eaves' kick", 0 },
			{ "house_roofhole", 1, 0, 0, 7, -38.0, 24.0, 62.0, -1, 0.0, "damage 1: a shell through the roof", 0 },
			{ "house_gable", 2, 0, 0, 7, -38.0, 16.0, 62.0, -1, 0.0, "damage 2: a gable down", 0 },
			{ "house_shell", 3, 0, 0, 7, -38.0, 18.0, 62.0, -1, 0.0, "damage 3: burnt out to a shell", 0 },
			{ "house_mid", 0, 1, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the middle level (drawn from about 90 m)", 0 },
			{ "house_far", 0, 2, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the far level (drawn from 300 m)", 0 },
			{ "house_gable_far", 2, 2, 0, 7, -38.0, 16.0, 62.0, -1, 0.0, "a gable down, far level", 0 },
			// (the level switches as the game shows them: the land battle's 70 degree lens on a screen 1,920 wide sees at 90 m
			// what this painter's 28 degree one, 560 wide, sees at 55 m, and at 300 m what it sees at 184 m; laid in pairs below)
			{ "switch90_near", 0, 0, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "", 0 },
			{ "switch90_mid", 0, 1, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "", 0 },
			{ "switch300_mid", 0, 1, 0, 7, -38.0, 4.0, 62.0, 3, 184.0, "", 0 },
			{ "switch300_far", 0, 2, 0, 7, -38.0, 4.0, 62.0, 3, 184.0, "", 0 },
			{ "switch300_near", 0, 0, 0, 7, -38.0, 4.0, 62.0, 3, 184.0, "", 0 }, // (not laid out: for measuring the far against it)
			{ "switch90_shell_near", 3, 0, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "", 0 },
			{ "switch90_shell_mid", 3, 1, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "", 0 },
			// (close looks for working on it, painted only when LAND_KIT_ONLY names them)
			{ "zz_shell_close", 3, 0, 0, 7, -38.0, 14.0, 62.0, 3, 21.0, "", 0 },
			{ "zz_shell_mid", 3, 1, 0, 7, -38.0, 14.0, 62.0, 3, 21.0, "", 0 },
			{ "zz_gable_close", 2, 0, 0, 7, -38.0, 14.0, 62.0, 3, 21.0, "", 0 },
			{ "zz_shell_inner", 3, 0, 0, 7, -12.0, 28.0, 62.0, 3, 11.0, "", 0 },
			{ "zz_house_verge", 0, 0, 0, 8, 70.0, 8.0, 62.0, 3, 16.0, "", 0 },
		};
		// (the farm's barn and byre: LAND_KIT_HOUSE=farm)
		const HouseLook FarmLooks[] = {
			{ "barn_slate", 0, 0, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "Norman barn: cart doors under an oak lintel, slits", 1 },
			{ "barn_open", 0, 0, 1, 4, 26.0, 10.0, 100.0, -1, 0.0, "another, flat tile: a leaf open on the dark", 1 },
			{ "barn_back", 0, 0, 0, 7, 140.0, 12.0, 210.0, -1, 0.0, "from behind: slits, no gutter, the loft's door", 1 },
			{ "barn_door", 0, 0, 0, 7, 18.0, 6.0, 80.0, 1, 7.5, "close: the cart doors, ledged, on strap hinges", 1 },
			{ "barn_door_open", 0, 0, 1, 8, -30.0, 8.0, 100.0, 1, 9.0, "close: both leaves open, the barn's dark and straw", 1 },
			{ "barn_roofhole", 1, 0, 0, 7, -38.0, 26.0, 62.0, -1, 0.0, "damage 1: roof holed, hay on the loft", 1 },
			{ "barn_gable", 2, 0, 0, 7, -38.0, 18.0, 62.0, -1, 0.0, "damage 2: a gable down", 1 },
			{ "barn_shell", 3, 0, 0, 7, -38.0, 20.0, 62.0, -1, 0.0, "damage 3: burnt out, the bare rubble inside", 1 },
			{ "barn_mid", 0, 1, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the middle level (from about 90 m)", 1 },
			{ "barn_far", 0, 2, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the far level (from 300 m)", 1 },
			{ "byre_slate", 0, 0, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the byre: stable doors, barred windows", 2 },
			{ "byre_tile", 0, 0, 1, 2, 30.0, 12.0, 110.0, -1, 0.0, "another, flat tile, limestone", 2 },
			{ "byre_door", 0, 0, 0, 7, 22.0, 5.0, 80.0, 1, 5.0, "close: a stable door, its window", 2 },
			{ "byre_roofhole", 1, 0, 0, 7, -38.0, 26.0, 62.0, -1, 0.0, "damage 1: roof holed", 2 },
			{ "byre_gable", 2, 0, 0, 7, -38.0, 18.0, 62.0, -1, 0.0, "damage 2: a gable down", 2 },
			{ "byre_shell", 3, 0, 0, 7, -38.0, 20.0, 62.0, -1, 0.0, "damage 3: burnt out", 2 },
			{ "byre_mid", 0, 1, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the middle level (from about 90 m)", 2 },
			{ "byre_far", 0, 2, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the far level (from 300 m)", 2 },
			{ "barn_shell_mid", 3, 1, 0, 7, -38.0, 20.0, 62.0, -1, 0.0, "barn burnt out, middle level", 1 },
			// (the level switches as the game shows them, as the farmhouse's)
			{ "barn_switch90_near", 0, 0, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "barn at 90 m: near level", 1 },
			{ "barn_switch90_mid", 0, 1, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "barn at 90 m: middle level", 1 },
			{ "barn_switch300_far", 0, 2, 0, 7, -38.0, 4.0, 62.0, 3, 184.0, "barn at 300 m: far level", 1 },
			{ "byre_switch90_near", 0, 0, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "byre at 90 m: near level", 2 },
			{ "byre_switch90_mid", 0, 1, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "byre at 90 m: middle level", 2 },
			{ "barn_switch300_mid", 0, 1, 0, 7, -38.0, 4.0, 62.0, 3, 184.0, "barn at 300 m: middle level", 1 },
			// (close looks for working on them, painted only when LAND_KIT_ONLY names them)
			{ "zz_barn_inside", 3, 0, 0, 7, -12.0, 30.0, 62.0, 3, 13.0, "", 1 },
			{ "zz_barn_pitch", 0, 0, 0, 8, 70.0, 8.0, 62.0, 3, 16.0, "", 1 },
		};
		// LAND_KIT_ONLY=<name,name>: only these panels (and a switch's two halves), for a quick look
		const char* Only = std::getenv("LAND_KIT_ONLY");
		const std::vector<HouseLook> Looks = bFarmSheet ? std::vector<HouseLook>(std::begin(FarmLooks), std::end(FarmLooks)) : std::vector<HouseLook>(std::begin(HouseLooks), std::end(HouseLooks));
		for (const HouseLook& K : Looks)
		{
			if (!Only && K.File[0] == 'z') continue;
			if (Only && ("," + std::string(Only) + ",").find("," + std::string(K.File) + ",") == std::string::npos
				&& !(std::strncmp(K.File, "switch", 6) == 0 && ("," + std::string(Only) + ",").find(",switch") != std::string::npos))
			{
				continue;
			}
			Panel P;
			P.File = K.File;
			P.House = K.Hurt;
			P.HouseLevel = K.Level;
			P.HouseRoof = K.Roof;
			P.HouseKey = K.Key;
			P.HouseKind = K.Kind;
			P.CameraBearing = K.Bearing;
			P.CameraUp = K.Up;
			P.SunBearing = K.Sun;
			P.Caption = K.Caption;
			if (K.Aim >= 0)
			{
				std::vector<LandHouse::Hole> Holes;
				LandHouse::Holes(HouseSpec(P), Holes);
				V3 At(0.0, 0.0, 0.0);
				for (const LandHouse::Hole& H : Holes)
				{
					// (a window low in the front, or its door: a barn's cart doors, a byre's stable door)
					if (H.Face == 0 && ((K.Aim == 0 && (H.What == 0 || H.What == 7) && H.Z0 < 2.0) || (K.Aim == 1 && (H.What == 1 || H.What == 3 || H.What == 6))))
					{
						At = V3(H.X, H.Y, 0.15 + 0.5 * (H.Z0 + H.Z1));
						break;
					}
				}
				if (K.Aim == 2)
				{
					At = V3(6.4, 0.9, 0.15 + LandHouse::RidgeOf(HouseSpec(P)) - 0.2);
				}
				if (K.Aim == 3)
				{
					At = V3(0.0, 0.0, 0.15 + 0.5 * LandHouse::RidgeOf(HouseSpec(P)));
				}
				P.bAim = true;
				P.AimAt = At;
				P.AimFrom = V3(std::sin(K.Bearing * Rad) * std::cos(K.Up * Rad), std::cos(K.Bearing * Rad) * std::cos(K.Up * Rad), std::sin(K.Up * Rad));
				P.AimDistance = K.AimDistance;
			}
			Panels.push_back(P);
		}
	}
	struct TankLook
	{
		const char* Model;
		const char* Nation;
		const char* Title;
	};
	const TankLook Tanks[3] = { { "Sherman", "USA", "M4 Sherman" }, { "PanzerIV", "GER", "Panzer IV" }, { "Tiger", "GER", "Tiger" } };
	for (const TankLook& T : Tanks)
	{
		if (bMarksSheet || bHouseSheet)
		{
			break;
		}
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
	if (!bMarksSheet && !bHouseSheet)
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
		if (P.House >= 0)
		{
			LandHouse::Levels L;
			LandHouse::Build(HouseSpec(P), L);
			const LandField::Mesh& M = P.HouseLevel == 0 ? L.Near : P.HouseLevel == 1 ? L.Mid : L.Far;
			Scene S;
			AddHouse(M, 0.15, S);
			{
				const LandHouse::Spec Hs = HouseSpec(P);
				S.Feet.push_back({ -0.5 * Hs.Long, -0.5 * Hs.Deep, 0.5 * Hs.Long, 0.5 * Hs.Deep });
			}
			const std::string Path = OutDir + "/" + P.File + ".ppm";
			Vehicle None;
			if (!Paint(S, P, None, Path))
			{
				std::printf("cannot write %s\n", Path.c_str());
				return 1;
			}
			if (!P.Caption.empty())
			{
				std::fprintf(Captions, "%s\t%s (%zu triangles)\n", P.File.c_str(), P.Caption.c_str(), M.Tris());
			}
			std::printf("  %s (near %zu, middle %zu, far %zu triangles)\n", Path.c_str(), L.Near.Tris(), L.Mid.Tris(), L.Far.Tris());
			continue;
		}
		const Model* M = ModelFor(P.ModelName, P.Nation, P);
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
		if (!P.bMarks)
		{
			AddMarks(V, S); // (the pictures' own flat marks, where the game's are off)
		}
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
	// The level switches: the house's left half as one level draws it and its right half as the next, at the distance the
	// game changes from one to the other, the middle of the picture enlarged pixel for pixel (Zoom) so the step shows.
	if (bHouseSheet)
	{
		struct Switch
		{
			const char* File;
			const char* Left;
			const char* Right;
			int Zoom;
			const char* Caption;
		};
		const Switch Switches[] = {
			{ "house_switch90", "switch90_near", "switch90_mid", 2, "at 90 m, enlarged 2x: near | middle" },
			{ "house_switch300", "switch300_mid", "switch300_far", 4, "at 300 m, enlarged 4x: middle | far" },
			{ "house_switch90_shell", "switch90_shell_near", "switch90_shell_mid", 2, "the shell at 90 m, 2x: near | middle" },
		};
		for (const Switch& K : Switches)
		{
			int LW = 0, LH = 0, RW = 0, RH = 0;
			std::vector<uint8_t> L, R;
			if (!ReadPicture(OutDir + "/" + K.Left + ".ppm", LW, LH, L) || !ReadPicture(OutDir + "/" + K.Right + ".ppm", RW, RH, R) || LW != RW || LH != RH)
			{
				continue;
			}
			std::vector<uint8_t> Out(L.size());
			for (int Y = 0; Y < LH; ++Y)
			{
				for (int X = 0; X < LW; ++X)
				{
					const int SX = LW / 2 + (X - LW / 2) / K.Zoom - (X < LW / 2 && (X - LW / 2) % K.Zoom ? 1 : 0);
					const int SY = LH / 2 + (Y - LH / 2) / K.Zoom - (Y < LH / 2 && (Y - LH / 2) % K.Zoom ? 1 : 0);
					const std::vector<uint8_t>& From = X < LW / 2 ? L : R;
					for (int C = 0; C < 3; ++C)
					{
						Out[(static_cast<size_t>(Y) * LW + X) * 3 + C] = X == LW / 2 ? 235 : From[(static_cast<size_t>(SY) * LW + SX) * 3 + C];
					}
				}
			}
			if (!WritePicture(OutDir + "/" + K.File + ".ppm", LW, LH, Out))
			{
				return 1;
			}
			std::fprintf(Captions, "%s\t%s\n", K.File, K.Caption);
			std::printf("  %s/%s.ppm (%s | %s)\n", OutDir.c_str(), K.File, K.Left, K.Right);
		}
	}
	std::fclose(Captions);
	return 0;
}
