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
		double GroundSnow = 0.0; // (a burnt-out house's winter panels: the snow lying on the ground round it, 0 to 1)
		double GroundSand = 0.0; // (the desert's houses: pale sand and grit for the plain earth)
		V3 Lo = V3(1e9, 1e9, 1e9), Hi = V3(-1e9, -1e9, -1e9);
	};

	/** A vehicle's bodies as triangles, at her pose, each point's colour taken toward its burnt colour by its soot weight
	 * (the model's one sooty black; with the burnt coat on, the point's own soot, rust or ash: Mesh::BurntOf). */
	void AddVehicle(const Vehicle& V, double SootShare, Scene& S)
	{
		const Model& M = V.Look();
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
				const uint8_t* To = Me.BurntOf(I, M.BurntColour);
				C[I] = Mix(Own, FromPicture(To[0], To[1], To[2]), Clamp01(Me.Soot[I] * SootShare));
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
		// A coat the rules picked for her army (LandKit's Dress::Coat, release 2: "japan"); empty: as her owner has her.
		std::string Coat;
		int CloseAmong = 99; // (the close look picks among her first this many marks: the Tiger's hull crosses, not her turret's)
		int Year = 1944, Month = 7;
		// The burnt coat (release 2, LandKit's Dress::bBurntCoat, the trial word "burntcoat"): her wreck in soot, rust and
		// ash; and others standing in the same picture (LAND_KIT_BURNT=1: one burnt out among live ones, from 150 m).
		bool bBurntCoat = false;
		// No coat or mark by the battle's month (release 2, LandKit's Dress::bAnyYear, the trial word "anyyear").
		bool bAnyYear = false;
		struct Other
		{
			std::string ModelName, Nation;
			Vehicle::Pin Pin;
			double SootShare = 0.0;
		};
		std::vector<Other> Others;
		// A farmhouse's panel instead (release 2, LandHouse, LAND_KIT_HOUSE=1): its damage (0 whole to 3 a shell), its
		// level (0 near, 1 middle, 2 far), roof and look; the sun's own bearing where given; a close look aimed at a point
		// from a way (bAim).
		int House = -1, HouseLevel = 0, HouseRoof = 0;
		int HouseKind = 0; // (0 the farmhouse, 1 its barn, 2 its byre: LAND_KIT_HOUSE=farm; 3 the church; 6 a village house)
		double HouseLong = 12.4, HouseDeep = 6.8; // (a village house's footprint, LAND_KIT_HOUSE=village)
		int HouseStoreys = 2;
		double HouseSnow = 0.0; // (the east's houses: the snow on its roof)
		uint64_t HouseKey = 1;
		double SunBearing = -1000.0;
		bool bAim = false;
		V3 AimAt, AimFrom;
		double AimDistance = 5.0;
		// The support weapons' sheet (LAND_KIT_SUPPORT=1): the battle's own daylight, a high near-white sun, in place of
		// the low warm morning (the art director rules paint by it); a 1.75 m bar standing by her, a man's height, banded
		// every quarter metre; a close look at one of her sockets (the feed: the belt going in), from a bearing and a
		// height as the whole look's, so far off.
		bool bDay = false;
		double Bar = 0.0;
		std::string CloseSocket;
		// What stands higher than this (metres) is left out of the framing: a measuring bar's top, so that a low weapon
		// fills her picture and the bar's bands still give her size.
		double FrameHigh = 1e9;
		double CloseDistance = 1.6;
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

	/** A measuring bar standing on the earth at a place, so high: 5 cm square, banded light and dark every quarter metre. */
	void AddBar(double X, double Y, double High, Scene& S)
	{
		const double Half = 0.025;
		for (double Z0 = 0.0; Z0 < High - 1e-6; Z0 += 0.25)
		{
			const double Z1 = std::min(High, Z0 + 0.25);
			const Colour Band = (static_cast<int>(std::floor(Z0 / 0.25 + 0.5)) % 2) == 0 ? FromPicture(226, 222, 210) : FromPicture(150, 44, 36);
			const V3 Corner[4] = { V3(X - Half, Y - Half, 0.0), V3(X + Half, Y - Half, 0.0), V3(X + Half, Y + Half, 0.0), V3(X - Half, Y + Half, 0.0) };
			for (int K = 0; K < 4; ++K)
			{
				const V3 A = Corner[K], B = Corner[(K + 1) % 4];
				const V3 Out = V3(B.Y - A.Y, A.X - B.X, 0.0).Unit();
				const V3 Q[4] = { V3(A.X, A.Y, Z0), V3(B.X, B.Y, Z0), V3(B.X, B.Y, Z1), V3(A.X, A.Y, Z1) };
				const int Order[6] = { 0, 1, 2, 0, 2, 3 };
				for (int T = 0; T < 6; T += 3)
				{
					Tri Face;
					for (int C = 0; C < 3; ++C)
					{
						Face.P[C] = Q[Order[T + C]];
						Face.N[C] = Out;
						Face.C[C] = Band;
					}
					S.Tris.push_back(Face);
				}
			}
		}
		// (its top)
		const V3 Top[4] = { V3(X - Half, Y - Half, High), V3(X + Half, Y - Half, High), V3(X + Half, Y + Half, High), V3(X - Half, Y + Half, High) };
		const int Order[6] = { 0, 1, 2, 0, 2, 3 };
		for (int T = 0; T < 6; T += 3)
		{
			Tri Face;
			for (int C = 0; C < 3; ++C)
			{
				Face.P[C] = Top[Order[T + C]];
				Face.N[C] = V3(0.0, 0.0, 1.0);
				Face.C[C] = FromPicture(226, 222, 210);
			}
			S.Tris.push_back(Face);
		}
		S.Lo = V3(std::min(S.Lo.X, X - Half), std::min(S.Lo.Y, Y - Half), std::min(S.Lo.Z, 0.0));
		S.Hi = V3(std::max(S.Hi.X, X + Half), std::max(S.Hi.Y, Y + Half), std::max(S.Hi.Z, High));
		S.Feet.push_back({ X - Half, Y - Half, X + Half, Y + Half });
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
		// (LAND_KIT_SCALE=2: each picture twice as wide and high, for a sheet read close)
		const int Scale = std::getenv("LAND_KIT_SCALE") ? std::max(1, std::min(4, std::atoi(std::getenv("LAND_KIT_SCALE")))) : 1;
		const int W = 560 * Scale * Out, H = 420 * Scale * Out;
		// The low sun of a July morning in Normandy, from the camera's right (fifteen degrees up).
		// (the support weapons' sheet, bDay: the battle's own daylight, the sun 46 degrees up and near white)
		const double SunBearing = (P.SunBearing > -999.0 ? P.SunBearing : P.CameraBearing + 62.0) * Rad, SunUp = (P.bDay ? 46.0 : 15.0) * Rad;
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
					if (Q.Z < -0.05 || Q.Z > P.FrameHigh)
					{
						continue; // (what is under the earth, a plinth's footing, a heap's rim; the top of a measuring bar)
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
		const Colour SunLight = P.bDay ? Colour(1.0, 0.97, 0.91) * 2.15 : Colour(1.0, 0.84, 0.64) * 2.7;
		const Colour SkyLight = P.bDay ? Colour(0.58, 0.66, 0.80) * 0.80 : Colour(0.50, 0.60, 0.80) * 0.62;
		const Colour Bounce = P.bDay ? Colour(0.34, 0.30, 0.24) * 0.34 : Colour(0.36, 0.30, 0.22) * 0.30;
		// (a damp brown loam, the patches faint, so the ground reads as plain earth and not as sand)
		const Colour Earth = FromPicture(102, 90, 72), EarthDark = FromPicture(90, 79, 63), Haze = FromPicture(196, 192, 182);
		std::vector<Colour> Lit(Count);
		std::vector<uint8_t> Body(Count, 0); // (the vehicle's own pixels, LAND_KIT_MASK=1: for measuring her without the ground)
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
					// (its bias grows with the slope to the sun and the map's texel, the soft lookup reaching 2.4 texels round:
					// a large flat wall in a slanting sun, a church's far off, otherwise shadows itself in streaks)
					const double NL = std::max(0.0, N.Dot(Shadow.L)), Slope = std::min(4.0, std::sqrt(std::max(0.0, 1.0 - NL * NL)) / std::max(NL, 0.05));
					const double Sunny = NL * Shadow.Lit(Pt + N * 0.015, 0.02 + 2.5 * Shadow.Texel * Slope);
					const double Sky = 0.55 + 0.45 * N.Z;
					const double Low = 0.72 + 0.28 * Smooth(0.0, 0.9, Pt.Z); // the running gear sits in the shade of the hull
					Lit[I] = Albedo[I] * (SunLight * Sunny + SkyLight * (Sky * Low) + Bounce * (0.5 - 0.5 * N.Z));
					Body[I] = Sunny > 0.25 ? 2 : 1;
				}
				else if (GroundZ < 1e29)
				{
					const double Patch = Noise2(Ground.X * 0.35, Ground.Y * 0.35) * 0.6 + Noise2(Ground.X * 1.3, Ground.Y * 1.3) * 0.4;
					const double Grain = 0.9 + 0.2 * Noise2(Ground.X * 9.0, Ground.Y * 9.0);
					const Colour Loam = Mix(Mix(Earth, EarthDark, Smooth(0.25, 0.85, Patch)) * Grain, FromPicture(196, 170, 128) * (0.92 + 0.08 * Smooth(0.25, 0.85, Patch)) * (0.6 + 0.4 * Grain), S.GroundSand);
					const Colour Soil = Mix(Loam, FromPicture(240, 242, 246) * (0.97 + 0.03 * Grain), S.GroundSnow);
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
		if (std::getenv("LAND_KIT_MASK"))
		{
			// (255 the vehicle in the sun, 128 the vehicle in shade, 0 not all the vehicle)
			std::vector<uint8_t> Mask(static_cast<size_t>(OW) * OH);
			for (int Y = 0; Y < OH; ++Y)
			{
				for (int X = 0; X < OW; ++X)
				{
					int Hers = 0, Sunlit = 0;
					for (int J = 0; J < Out; ++J)
					{
						for (int I = 0; I < Out; ++I)
						{
							const uint8_t Of = Body[static_cast<size_t>(Y * Out + J) * W + (X * Out + I)];
							Hers += Of != 0;
							Sunlit += Of == 2;
						}
					}
					Mask[static_cast<size_t>(Y) * OW + X] = static_cast<uint8_t>(Hers < Out * Out ? 0 : Sunlit == Out * Out ? 255 : 128);
				}
			}
			if (std::FILE* Mf = std::fopen((Path + ".mask.pgm").c_str(), "wb"))
			{
				std::fprintf(Mf, "P5\n%d %d\n255\n", OW, OH);
				std::fwrite(Mask.data(), 1, Mask.size(), Mf);
				std::fclose(Mf);
			}
		}
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
		// (the month belongs in the key whatever the panel shows, not only for a marks panel: a model whose file gives
		// its army's face by month (the Type 97's khaki, then two camouflages) is a different model in each, and two
		// panels a year apart were being painted from whichever was loaded first)
		const std::string Key = Name + "|" + Nation + "|" + std::to_string(For.Year * 100 + For.Month) + (For.bMarks ? "|marks" : "") + (For.Coat.empty() ? "" : "|coat " + For.Coat)
			+ (For.bBurntCoat ? "|burnt" : "") + (For.bAnyYear ? "|anyyear" : "");
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
			D.Coat = For.Coat;
			D.bBurntCoat = For.bBurntCoat;
			D.bAnyYear = For.bAnyYear;
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
	// (LAND_KIT_ANYYEAR=1: the trial word "anyyear"'s own sheet instead, release 2; Brian, 26 November 2026: "have the
	// paint be on it regardless of the year". Each pair is one vehicle by the month, as shipped, and in any year: a
	// German tank of 1944 in the grey of 1940, a British one of 1940 with her flash)
	const bool bAnyYearSheet = std::getenv("LAND_KIT_ANYYEAR") != nullptr;
	struct YearLook
	{
		const char* File;
		const char* Model;
		const char* Nation;
		int Year, Month;
		bool bAnyYear, bClose;
		double Bearing, Up;
		const char* Caption;
	};
	const YearLook YearLooks[] = {
		{ "anyyear_a_pz4_1940", "PanzerIV", "GER", 1940, 5, false, false, 42.0, 15.0, "Panzer IV, May 1940, by the month: tank grey" },
		{ "anyyear_b_pz4_1944_month", "PanzerIV", "GER", 1944, 7, false, false, 42.0, 15.0, "Panzer IV, July 1944, by the month (as shipped)" },
		{ "anyyear_c_pz4_1944", "PanzerIV", "GER", 1944, 7, true, false, 42.0, 15.0, "Panzer IV, July 1944, ANY YEAR: tank grey" },
		{ "anyyear_d_tiger_month", "Tiger", "GER", 1944, 7, false, false, 42.0, 16.0, "Tiger, July 1944, by the month (as shipped)" },
		{ "anyyear_e_tiger", "Tiger", "GER", 1944, 7, true, false, 42.0, 16.0, "Tiger, July 1944, ANY YEAR: tank grey" },
		{ "anyyear_f_panther_month", "Panther", "GER", 1944, 7, false, false, 42.0, 16.0, "Panther, July 1944, by the month (as shipped)" },
		{ "anyyear_g_panther", "Panther", "GER", 1944, 7, true, false, 42.0, 16.0, "Panther, July 1944, ANY YEAR: the army's grey laid in" },
		{ "anyyear_h_hummel", "Hummel", "GER", 1944, 7, true, false, 42.0, 16.0, "Hummel, July 1944, ANY YEAR: the army's grey laid in" },
		{ "anyyear_i_truck_month", "Truck", "GER", 1944, 7, false, false, 42.0, 16.0, "the lorry in German hands, July 1944, by the month" },
		{ "anyyear_j_truck", "Truck", "GER", 1944, 7, true, false, 42.0, 16.0, "the lorry in German hands, July 1944, ANY YEAR: grey" },
		{ "anyyear_k_light_uk_month", "LightTank", "UK", 1940, 6, false, true, 42.0, 14.0, "light tank, British, June 1940, by the month: no mark" },
		{ "anyyear_l_light_uk", "LightTank", "UK", 1940, 6, true, true, 42.0, 14.0, "light tank, British, June 1940, ANY YEAR: the flash" },
		{ "anyyear_m_sexton_month", "Sexton", "UK", 1944, 7, false, false, 42.0, 15.0, "Sexton, British, July 1944, by the month: the white star" },
		{ "anyyear_n_sexton", "Sexton", "UK", 1944, 7, true, false, 42.0, 15.0, "Sexton, British, July 1944, ANY YEAR: the flash" },
		{ "anyyear_o_type97_month", "Type97", "JAP", 1943, 8, false, false, 72.0, 15.0, "Type 97, August 1943, by the month: her later coat" },
		{ "anyyear_p_type97", "Type97", "JAP", 1943, 8, true, false, 72.0, 15.0, "Type 97, August 1943, ANY YEAR: her first coat" },
	};
	for (const YearLook& K : YearLooks)
	{
		if (!bAnyYearSheet)
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
		P.bAnyYear = K.bAnyYear;
		P.bClose = K.bClose;
		P.CameraBearing = K.Bearing;
		P.CameraUp = K.Up;
		P.Caption = K.Caption;
		Panels.push_back(P);
	}
	// (LAND_KIT_HOUSE=1: the farmhouse pilot's own sheet instead, release 2's -TheaterLandTrial=houses; LAND_KIT_HOUSE=farm
	// its barn's and byre's)
	const bool bHouseSheet = std::getenv("LAND_KIT_HOUSE") != nullptr;
	const bool bFarmSheet = bHouseSheet && std::string(std::getenv("LAND_KIT_HOUSE")) == "farm";
	const bool bChurchSheet = bHouseSheet && std::string(std::getenv("LAND_KIT_HOUSE")) == "church";
	const bool bVillageSheet = bHouseSheet && std::string(std::getenv("LAND_KIT_HOUSE")) == "village";
	const bool bEastSheet = bHouseSheet && std::string(std::getenv("LAND_KIT_HOUSE")) == "east";
	const bool bDesertSheet = bHouseSheet && std::string(std::getenv("LAND_KIT_HOUSE")) == "desert";
	auto HouseSpec = [](const Panel& P)
	{
		LandHouse::Spec H;
		// (the bocage's farmstead's three, as LandField lays them)
		H.Kind = P.HouseKind;
		// (and the village's church, its nave as LandField's LayChurch lays it, its chancel and tower with it)
		H.Long = P.HouseKind == 1 ? 15.0 : P.HouseKind == 2 ? 13.0 : P.HouseKind == 3 ? 18.0 : 14.0;
		H.Deep = P.HouseKind == 1 ? 8.4 : P.HouseKind == 2 ? 6.8 : P.HouseKind == 3 ? 8.6 : 7.2;
		H.Storeys = P.HouseKind == 0 ? 2 : 1;
		// (a village house: its own footprint and storeys, as LandField lays them along the village's lanes; the east's houses
		// their own footprints)
		if (P.HouseKind >= 6)
		{
			H.Long = P.HouseLong;
			H.Deep = P.HouseDeep;
			H.Storeys = P.HouseStoreys;
		}
		H.Roof = P.HouseRoof;
		H.Hurt = P.House;
		H.Key = P.HouseKey;
		H.Snow = P.HouseSnow;
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
			         // 3 the whole house from AimDistance (the level switches' views, painted as the game shows them); the
			         // church's: 4 its belfry and spire's foot, 5 the tower's west door, 6 the cross, 7 where tower and nave meet,
			         // 8 the tower's south face under its lower string course; a village house's: 9 its shop, 13 the biggest
			         // place up its front where its render has come away, 14 its strikes there at their thickest; the izba's 17,
			         // 18, 19, the khata's 20, 21 and the burnt-out house's 22, 23, 24 (as set below)
			double AimDistance;
			const char* Caption;
			int Kind; // 0 the farmhouse, 1 its barn, 2 its byre, 3 the church, 6 a village house, 7 the east's izba, 8 the
			          // steppe's khata, 9 a burnt-out house, 10 the desert's flat-roofed house
			double Long = 12.4, Deep = 6.8; // (a village house's)
			int Storeys = 2;
			double Snow = 0.0; // (the east's: the snow lying on its roof, 0 to 1)
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
			{ "zz_gable90_near", 2, 0, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "", 0 },
			{ "zz_gable90_mid", 2, 1, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "", 0 },
			{ "zz_shell_inner", 3, 0, 0, 7, -12.0, 28.0, 62.0, 3, 11.0, "", 0 },
			{ "zz_house_verge", 0, 0, 0, 8, 70.0, 8.0, 62.0, 3, 16.0, "", 0 },
		};
		// (the farm's barn and byre: LAND_KIT_HOUSE=farm)
		const HouseLook FarmLooks[] = {
			{ "barn_slate", 0, 0, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "Norman barn: cart doors under an oak lintel, slits", 1 },
			{ "barn_open", 0, 0, 1, 4, 26.0, 10.0, 100.0, -1, 0.0, "another, flat tile: a leaf open on the dark", 1 },
			{ "barn_back", 0, 0, 0, 7, 140.0, 12.0, 210.0, -1, 0.0, "from behind: slits, no gutter, the loft's door", 1 },
			{ "barn_door", 0, 0, 0, 7, 18.0, 6.0, 80.0, 1, 7.5, "close: the cart doors, ledged, on strap hinges", 1 },
			{ "barn_door_open", 0, 0, 1, 8, -30.0, 8.0, 100.0, 1, 9.0, "close: both leaves open, the day on the straw inside", 1 },
			{ "barn_roofhole", 1, 0, 0, 7, -38.0, 26.0, 62.0, -1, 0.0, "damage 1: roof holed, hay on the loft", 1 },
			{ "barn_gable", 2, 0, 0, 7, -38.0, 18.0, 62.0, -1, 0.0, "damage 2: a gable down", 1 },
			{ "barn_shell", 3, 0, 0, 7, -38.0, 20.0, 62.0, -1, 0.0, "damage 3: burnt out, the bare rubble inside", 1 },
			{ "barn_shell_mid", 3, 1, 0, 7, -38.0, 20.0, 62.0, -1, 0.0, "burnt out, middle level (same camera)", 1 },
			{ "barn_mid", 0, 1, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the middle level (from about 90 m)", 1 },
			{ "barn_far", 0, 2, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the far level (from 300 m)", 1 },
			{ "byre_slate", 0, 0, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the byre: stable doors, barred windows", 2 },
			{ "byre_tile", 0, 0, 1, 2, 30.0, 12.0, 110.0, -1, 0.0, "another, flat tile, limestone", 2 },
			{ "byre_door", 0, 0, 0, 7, 22.0, 5.0, 80.0, 1, 5.0, "close: a stable door, its window", 2 },
			{ "byre_roofhole", 1, 0, 0, 7, -38.0, 26.0, 62.0, -1, 0.0, "damage 1: roof holed", 2 },
			{ "byre_gable", 2, 0, 0, 7, -38.0, 18.0, 62.0, -1, 0.0, "damage 2: a gable down", 2 },
			{ "byre_shell", 3, 0, 0, 7, -38.0, 20.0, 62.0, -1, 0.0, "damage 3: burnt out", 2 },
			{ "byre_shell_mid", 3, 1, 0, 7, -38.0, 20.0, 62.0, -1, 0.0, "burnt out, middle level (same camera)", 2 },
			{ "byre_mid", 0, 1, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the middle level (from about 90 m)", 2 },
			{ "byre_far", 0, 2, 0, 7, -38.0, 11.0, 62.0, -1, 0.0, "the far level (from 300 m)", 2 },
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
			{ "zz_barn_gable90_near", 2, 0, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "", 1 },
			{ "zz_barn_gable90_mid", 2, 1, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "", 1 },
			{ "zz_gable90_back_near", 2, 0, 0, 7, 140.0, 4.0, 210.0, 3, 55.0, "", 0 },
			{ "zz_gable90_back_mid", 2, 1, 0, 7, 140.0, 4.0, 210.0, 3, 55.0, "", 0 },
		};
		// (the village's church: LAND_KIT_HOUSE=church)
		const HouseLook ChurchLooks[] = {
			{ "church_whole", 0, 0, 0, 7, -38.0, 10.0, 62.0, -1, 0.0, "Norman church: nave, chancel, tower and stone spire", 3 },
			{ "church_east", 0, 0, 0, 7, 128.0, 9.0, 210.0, -1, 0.0, "from the east: the chancel, the nave's gable over it", 3 },
			{ "church_other", 0, 0, 0, 2, 32.0, 12.0, 110.0, -1, 0.0, "another, its other stone", 3 },
			{ "church_door", 0, 0, 0, 7, 18.0, 4.0, 80.0, 1, 9.0, "close: the south door, voussoirs, hood mould, oak", 3 },
			{ "church_window", 0, 0, 0, 7, -22.0, 2.0, 62.0, 0, 5.0, "close: a nave window, leaded, under its ring", 3 },
			{ "church_west", 0, 0, 0, 7, -90.0, 6.0, 120.0, 5, 13.0, "the tower's west door and window", 3 },
			{ "church_belfry", 0, 0, 0, 7, -60.0, 8.0, 62.0, 4, 15.0, "close: belfry, parapet, pinnacles, lucarnes", 3 },
			{ "church_tower_weather", 0, 0, 0, 7, -25.0, 6.0, 62.0, 8, 9.0, "close: the tower weathered under its string course", 3 },
			{ "church_cross", 0, 0, 0, 7, -40.0, 6.0, 62.0, 6, 6.5, "close: the finial, the cross, the gilt cock", 3 },
			{ "church_cross_b", 0, 0, 0, 7, 62.0, 12.0, 62.0, 6, 6.5, "the cross and cock, another heading", 3 },
			{ "church_roofhole", 1, 0, 0, 7, -38.0, 26.0, 62.0, -1, 0.0, "damage: a shell through the nave's roof", 3 },
			{ "church_mid", 0, 1, 0, 7, -38.0, 10.0, 62.0, -1, 0.0, "the middle level (from about 90 m)", 3 },
			{ "church_far", 0, 2, 0, 7, -38.0, 10.0, 62.0, -1, 0.0, "the far level (from 300 m)", 3 },
			{ "church_roofhole_mid", 1, 1, 0, 7, -38.0, 26.0, 62.0, -1, 0.0, "roof holed, middle level", 3 },
			{ "church_roofhole_far", 1, 2, 0, 7, -38.0, 26.0, 62.0, -1, 0.0, "roof holed, far level", 3 },
			{ "church_hole_mid", 1, 1, 0, 7, -10.0, 42.0, 62.0, 3, 24.0, "the hole, middle level, seen at 24 m", 3 },
			{ "church_hole_far", 1, 2, 0, 7, -10.0, 42.0, 62.0, 3, 24.0, "the hole, far level, seen at 24 m", 3 },
			{ "church_switch90_near", 0, 0, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "church at 90 m: near level", 3 },
			{ "church_switch90_mid", 0, 1, 0, 7, -38.0, 4.0, 62.0, 3, 55.0, "church at 90 m: middle level", 3 },
			{ "church_switch300_near", 0, 0, 0, 7, -38.0, 4.0, 62.0, 3, 184.0, "", 3 }, // (not laid out: for measuring the far against it)
			{ "church_switch300_mid", 0, 1, 0, 7, -38.0, 4.0, 62.0, 3, 184.0, "church at 300 m: middle level", 3 },
			{ "church_switch300_far", 0, 2, 0, 7, -38.0, 4.0, 62.0, 3, 184.0, "church at 300 m: far level", 3 },
			// (close looks for working on it, painted only when LAND_KIT_ONLY names them)
			{ "zz_church_junction", 0, 0, 0, 7, -55.0, 28.0, 62.0, 7, 14.0, "", 3 },
			{ "zz_church_chancel", 0, 0, 0, 7, 60.0, 22.0, 62.0, 3, 24.0, "", 3 },
		};
		// (the village's houses: LAND_KIT_HOUSE=village; aim 9 its shop's front, 13 a place its render came away, 14 its
		// strikes, 15 the front's broken end where a gable fell (to the house's +x), 16 the -x gable's attic)
		const HouseLook VillageLooks[] = {
			{ "village_render_shop", 0, 0, 0, 10, -36.0, 10.0, 62.0, -1, 0.0, "village house: lime render, its shop below", 6, 12.4, 6.8, 2 },
			{ "village_shop", 0, 0, 0, 10, -20.0, 4.0, 62.0, 9, 7.5, "close: the shop's front, fascia, glass, door", 6, 12.4, 6.8, 2 },
			{ "village_render_spall", 0, 0, 0, 3, 18.0, 4.0, 100.0, 13, 3.4, "close: the render come away to the rubble, its cracks", 6, 12.4, 6.8, 2 },
			{ "village_lime_shop", 0, 0, 1, 11, 30.0, 10.0, 110.0, -1, 0.0, "another: cream limestone, flat tile, its shop boarded", 6, 11.4, 6.6, 2 },
			{ "village_rubble", 0, 0, 0, 5, -40.0, 11.0, 62.0, -1, 0.0, "another: the bocage's rubble, slate", 6, 13.6, 7.0, 2 },
			{ "village_cottage", 0, 0, 1, 12, 34.0, 12.0, 110.0, -1, 0.0, "a cottage of one storey, rendered, tile", 6, 9.4, 6.4, 1 },
			{ "village_pocks", 1, 0, 0, 36, -16.0, 4.0, 62.0, 14, 3.0, "close: a burst's splinters in the render", 6, 12.4, 6.8, 2 },
			{ "village_roofhole", 1, 0, 0, 10, 150.0, 22.0, 62.0, -1, 0.0, "damage 1: a shell through the roof (from behind)", 6, 12.4, 6.8, 2 },
			{ "village_gable", 2, 0, 0, 10, -36.0, 16.0, 62.0, -1, 0.0, "damage 2: a gable down", 6, 12.4, 6.8, 2 },
			{ "village_broken_top", 2, 0, 0, 10, -30.0, 10.0, 62.0, 15, 6.5, "close: its broken top, the render gone in a band", 6, 12.4, 6.8, 2 },
			{ "village_top_above", 2, 0, 0, 10, -15.0, 20.0, 62.0, 15, 3.0, "close, from above: the broken top's stones", 6, 12.4, 6.8, 2 },
			{ "village_shell", 3, 0, 0, 10, -36.0, 18.0, 62.0, -1, 0.0, "damage 3: burnt out to a shell", 6, 12.4, 6.8, 2 },
			{ "village_mid", 0, 1, 0, 10, -36.0, 10.0, 62.0, -1, 0.0, "the middle level (from about 90 m)", 6, 12.4, 6.8, 2 },
			{ "village_far", 0, 2, 0, 10, -36.0, 10.0, 62.0, -1, 0.0, "the far level (from 300 m)", 6, 12.4, 6.8, 2 },
			{ "village_shell_mid", 3, 1, 0, 10, -36.0, 18.0, 62.0, -1, 0.0, "burnt out, middle level", 6, 12.4, 6.8, 2 },
			{ "village_switch90_near", 0, 0, 0, 10, -36.0, 4.0, 62.0, 3, 55.0, "house at 90 m: near level", 6, 12.4, 6.8, 2 },
			{ "village_switch90_mid", 0, 1, 0, 10, -36.0, 4.0, 62.0, 3, 55.0, "house at 90 m: middle level", 6, 12.4, 6.8, 2 },
			{ "village_switch300_near", 0, 0, 0, 10, -36.0, 4.0, 62.0, 3, 184.0, "", 6, 12.4, 6.8, 2 }, // (not laid out: for measuring)
			{ "village_switch300_mid", 0, 1, 0, 10, -36.0, 4.0, 62.0, 3, 184.0, "house at 300 m: middle level", 6, 12.4, 6.8, 2 },
			{ "village_switch300_far", 0, 2, 0, 10, -36.0, 4.0, 62.0, 3, 184.0, "house at 300 m: far level", 6, 12.4, 6.8, 2 },
			// (close looks for working on it, painted only when LAND_KIT_ONLY names them)
			{ "zz_roof_front", 1, 0, 0, 10, -36.0, 22.0, 62.0, -1, 0.0, "", 6, 12.4, 6.8, 2 },
			{ "zz_attic", 1, 0, 0, 10, -120.0, 25.0, 62.0, 16, 9.0, "", 6, 12.4, 6.8, 2 },
			{ "zz_top_b", 2, 0, 0, 10, 20.0, 35.0, 62.0, 15, 3.0, "", 6, 12.4, 6.8, 2 },
			{ "zz_farm_top", 2, 0, 0, 7, -30.0, 10.0, 62.0, 15, 6.5, "", 0, 14.0, 7.2, 2 },
		};
		// (the east's houses: LAND_KIT_HOUSE=east; the izba, Kind 7, Roof 0 planks or 1 thatch, its footprint LandFarm's 7.5 by
		// 7.2 m; aim 17 its street gable's windows, 18 its front corner's log ends, 19 its ridge's end at the street gable)
		const HouseLook EastLooks[] = {
			{ "izba_planks", 0, 0, 0, 7, 42.0, 10.0, 62.0, -1, 0.0, "the east: a log izba, carved frames, plank roof", 7, 7.5, 7.2, 1 },
			{ "izba_thatch", 0, 0, 1, 3, 38.0, 11.0, 62.0, -1, 0.0, "another: thatched, other paint", 7, 7.5, 7.2, 1 },
			{ "izba_back", 0, 0, 0, 7, 205.0, 12.0, 250.0, -1, 0.0, "from behind: the woodpile along the back wall", 7, 7.5, 7.2, 1 },
			{ "izba_porch", 0, 0, 0, 7, -32.0, 9.0, 62.0, -1, 0.0, "from the other side: the porch, the plank door", 7, 7.5, 7.2, 1 },
			{ "izba_window", 0, 0, 0, 7, 18.0, 4.0, 62.0, 0, 3.4, "close: a carved frame, its shutters, the curtain", 7, 7.5, 7.2, 1 },
			{ "izba_gable", 0, 0, 0, 7, 80.0, 6.0, 62.0, 17, 7.0, "close: the street gable, three windows, its towel", 7, 7.5, 7.2, 1 },
			{ "izba_door", 0, 0, 0, 7, -18.0, 6.0, 62.0, 1, 5.5, "close: the porch on its posts, the door", 7, 7.5, 7.2, 1 },
			{ "izba_corner", 0, 0, 0, 7, 48.0, 8.0, 62.0, 18, 2.8, "close: the corner, log ends, rings, boulders", 7, 7.5, 7.2, 1 },
			{ "izba_horse", 0, 0, 0, 7, 62.0, 14.0, 62.0, 19, 3.6, "close: the ridge log's horse head, the barge boards", 7, 7.5, 7.2, 1 },
			{ "izba_thatch_close", 0, 0, 1, 3, 60.0, 16.0, 62.0, 19, 4.5, "close: the thatch's courses, the straw ridge, poles", 7, 7.5, 7.2, 1 },
			{ "izba_paint_blue", 0, 0, 0, 13, 18.0, 4.0, 62.0, 0, 4.4, "paint, one colour a house, worn: faded blue", 7, 7.5, 7.2, 1 },
			{ "izba_paint_white", 0, 0, 0, 1, 18.0, 4.0, 62.0, 0, 4.4, "white, its shutters bare wood", 7, 7.5, 7.2, 1 },
			{ "izba_paint_red", 0, 0, 0, 10, 18.0, 4.0, 62.0, 0, 4.4, "ochre-red, its shutters bare wood", 7, 7.5, 7.2, 1 },
			{ "izba_paint_green", 0, 0, 0, 26, 18.0, 4.0, 62.0, 0, 4.4, "faded green, its shutters white", 7, 7.5, 7.2, 1 },
			{ "izba_snow", 0, 0, 0, 7, 42.0, 10.0, 62.0, -1, 0.0, "in winter: the snow on its roof", 7, 7.5, 7.2, 1, 1.0 },
			{ "izba_thatch_snow", 0, 0, 1, 3, 38.0, 11.0, 62.0, -1, 0.0, "thatched, in winter", 7, 7.5, 7.2, 1, 1.0 },
			{ "izba_snow_mid", 0, 1, 0, 7, 42.0, 10.0, 62.0, -1, 0.0, "in winter, middle level", 7, 7.5, 7.2, 1, 1.0 },
			{ "izba_mid", 0, 1, 0, 7, 42.0, 10.0, 62.0, -1, 0.0, "the middle level (from about 90 m)", 7, 7.5, 7.2, 1 },
			{ "izba_far", 0, 2, 0, 7, 42.0, 10.0, 62.0, -1, 0.0, "the far level (from 300 m)", 7, 7.5, 7.2, 1 },
			{ "izba_thatch_mid", 0, 1, 1, 3, 38.0, 11.0, 62.0, -1, 0.0, "thatched, middle level", 7, 7.5, 7.2, 1 },
			{ "izba_thatch_far", 0, 2, 1, 3, 38.0, 11.0, 62.0, -1, 0.0, "thatched, far level", 7, 7.5, 7.2, 1 },
			{ "izba_switch90_near", 0, 0, 0, 7, 42.0, 4.0, 62.0, 3, 55.0, "izba at 90 m: near level", 7, 7.5, 7.2, 1 },
			{ "izba_switch90_mid", 0, 1, 0, 7, 42.0, 4.0, 62.0, 3, 55.0, "izba at 90 m: middle level", 7, 7.5, 7.2, 1 },
			{ "izba_switch300_near", 0, 0, 0, 7, 42.0, 4.0, 62.0, 3, 184.0, "", 7, 7.5, 7.2, 1 }, // (not laid out: for measuring)
			{ "izba_switch300_mid", 0, 1, 0, 7, 42.0, 4.0, 62.0, 3, 184.0, "izba at 300 m: middle level", 7, 7.5, 7.2, 1 },
			{ "izba_switch300_far", 0, 2, 0, 7, 42.0, 4.0, 62.0, 3, 184.0, "izba at 300 m: far level", 7, 7.5, 7.2, 1 },
			{ "izba_thatch_switch90_near", 0, 0, 1, 3, 38.0, 4.0, 62.0, 3, 55.0, "thatched at 90 m: near level", 7, 7.5, 7.2, 1 },
			{ "izba_thatch_switch90_mid", 0, 1, 1, 3, 38.0, 4.0, 62.0, 3, 55.0, "thatched at 90 m: middle level", 7, 7.5, 7.2, 1 },
			// (the khata, Kind 8, the kit's Khata's footprint 9.18 by 6.46 m; aim 20 its eaves' corner, 21 a hip by the ridge)
			{ "khata_front", 0, 0, 0, 7, 36.0, 10.0, 62.0, -1, 0.0, "the steppe: a whitewashed khata, hipped thatch", 8, 9.18, 6.46, 1 },
			{ "khata_other", 0, 0, 0, 4, -40.0, 11.0, 62.0, -1, 0.0, "another: its wash, its paint", 8, 9.18, 6.46, 1 },
			{ "khata_back", 0, 0, 0, 7, 210.0, 12.0, 250.0, -1, 0.0, "from behind", 8, 9.18, 6.46, 1 },
			{ "khata_window", 0, 0, 0, 7, 16.0, 4.0, 62.0, 0, 3.4, "close: a window deep in the clay (some shuttered)", 8, 9.18, 6.46, 1 },
			{ "khata_door", 0, 0, 0, 7, -20.0, 6.0, 62.0, 1, 5.0, "close: the door, the prizba", 8, 9.18, 6.46, 1 },
			{ "khata_eaves", 0, 0, 0, 7, 50.0, 8.0, 62.0, 20, 3.4, "close: the eaves cut over the rafters' ends", 8, 9.18, 6.46, 1 },
			{ "khata_hip", 0, 0, 0, 7, 60.0, 22.0, 62.0, 21, 4.5, "close: the hip, the ridge's roll, the chimney", 8, 9.18, 6.46, 1 },
			{ "khata_snow", 0, 0, 0, 7, 36.0, 10.0, 62.0, -1, 0.0, "in winter", 8, 9.18, 6.46, 1, 1.0 },
			{ "khata_mid", 0, 1, 0, 7, 36.0, 10.0, 62.0, -1, 0.0, "the middle level (from about 90 m)", 8, 9.18, 6.46, 1 },
			{ "khata_far", 0, 2, 0, 7, 36.0, 10.0, 62.0, -1, 0.0, "the far level (from 300 m)", 8, 9.18, 6.46, 1 },
			{ "khata_snow_mid", 0, 1, 0, 7, 36.0, 10.0, 62.0, -1, 0.0, "in winter, middle level", 8, 9.18, 6.46, 1, 1.0 },
			{ "khata_switch90_near", 0, 0, 0, 7, 36.0, 4.0, 62.0, 3, 55.0, "khata at 90 m: near level", 8, 9.18, 6.46, 1 },
			{ "khata_switch90_mid", 0, 1, 0, 7, 36.0, 4.0, 62.0, 3, 55.0, "khata at 90 m: middle level", 8, 9.18, 6.46, 1 },
			{ "khata_switch300_near", 0, 0, 0, 7, 36.0, 4.0, 62.0, 3, 184.0, "", 8, 9.18, 6.46, 1 }, // (not laid out: for measuring)
			{ "khata_switch300_mid", 0, 1, 0, 7, 36.0, 4.0, 62.0, 3, 184.0, "khata at 300 m: middle level", 8, 9.18, 6.46, 1 },
			{ "khata_switch300_far", 0, 2, 0, 7, 36.0, 4.0, 62.0, 3, 184.0, "khata at 300 m: far level", 8, 9.18, 6.46, 1 },
			// (the burnt-out house, Kind 9: Roof 0 an izba burnt down on the izba's footprint, 1 a khata roofless on the khata's;
			// aim 22 its stove's mouth, 23 its chimney's broken top, 24 its front corner at the ground, 25 its stove's side, 26 a khata's wall top)
			{ "burnt_logs", 0, 0, 0, 7, 36.0, 14.0, 62.0, -1, 0.0, "burnt out: an izba's logs, its stove standing", 9, 7.5, 7.2, 1 },
			{ "burnt_clay", 0, 0, 1, 7, 36.0, 14.0, 62.0, -1, 0.0, "a khata burnt out: its clay walls, roofless", 9, 9.18, 6.46, 1 },
			{ "burnt_other", 0, 0, 0, 4, -40.0, 16.0, 62.0, -1, 0.0, "another: the stove the other way", 9, 7.5, 7.2, 1 },
			{ "burnt_stove", 0, 0, 0, 7, 20.0, 8.0, 62.0, 22, 3.2, "close: the stove's mouth, its plaster fallen", 9, 7.5, 7.2, 1 },
			{ "burnt_patch", 0, 0, 0, 7, 22.0, 6.0, 62.0, 25, 3.3, "close: the plaster torn from the stove's bricks", 9, 7.5, 7.2, 1 },
			{ "burnt_chimney", 0, 0, 0, 7, 40.0, 10.0, 62.0, 23, 3.0, "close: the chimney's broken top", 9, 7.5, 7.2, 1 },
			{ "burnt_corner", 0, 0, 0, 7, 48.0, 18.0, 62.0, 24, 3.4, "close: charred logs in the ashes", 9, 7.5, 7.2, 1 },
			{ "burnt_clay_corner", 0, 0, 1, 7, 48.0, 14.0, 62.0, 24, 3.8, "close: the clay wall's corner at the ground", 9, 9.18, 6.46, 1 },
			{ "burnt_clay_top", 0, 0, 1, 7, 24.0, 18.0, 62.0, 26, 6.5, "close: the clay wall's broken top, its soot, a rafter", 9, 9.18, 6.46, 1 },
			{ "burnt_snow", 0, 0, 0, 7, 36.0, 14.0, 62.0, -1, 0.0, "in winter", 9, 7.5, 7.2, 1, 1.0 },
			{ "burnt_clay_snow", 0, 0, 1, 7, 36.0, 14.0, 62.0, -1, 0.0, "the khata's, in winter", 9, 9.18, 6.46, 1, 1.0 },
			{ "burnt_mid", 0, 1, 0, 7, 36.0, 14.0, 62.0, -1, 0.0, "the middle level (from about 90 m)", 9, 7.5, 7.2, 1 },
			{ "burnt_far", 0, 2, 0, 7, 36.0, 14.0, 62.0, -1, 0.0, "the far level (from 300 m)", 9, 7.5, 7.2, 1 },
			{ "burnt_clay_mid", 0, 1, 1, 7, 36.0, 14.0, 62.0, -1, 0.0, "the khata's, middle level", 9, 9.18, 6.46, 1 },
			{ "burnt_switch90_near", 0, 0, 0, 7, 36.0, 4.0, 62.0, 3, 55.0, "burnt out at 90 m: near level", 9, 7.5, 7.2, 1 },
			{ "burnt_switch90_mid", 0, 1, 0, 7, 36.0, 4.0, 62.0, 3, 55.0, "burnt out at 90 m: middle level", 9, 7.5, 7.2, 1 },
			{ "burnt_switch300_near", 0, 0, 0, 7, 36.0, 4.0, 62.0, 3, 184.0, "", 9, 7.5, 7.2, 1 }, // (not laid out: for measuring)
			{ "burnt_switch300_mid", 0, 1, 0, 7, 36.0, 4.0, 62.0, 3, 184.0, "burnt out at 300 m: middle level", 9, 7.5, 7.2, 1 },
			{ "burnt_switch300_far", 0, 2, 0, 7, 36.0, 4.0, 62.0, 3, 184.0, "burnt out at 300 m: far level", 9, 7.5, 7.2, 1 },
		};
		// (the desert's houses: LAND_KIT_HOUSE=desert; the flat-roofed house, Kind 10, Roof 0 limewashed, 1 its bare mud, 2 its
		// main room of two storeys, its footprint LandDesert's 11 m square; aim 27 its yard's gate; 100 and over: the first
		// thing of that kind less 100 that LandHouse::Holes lists, 200 and over the second)
		const HouseLook DesertLooks[] = {
			{ "dsr_white", 0, 0, 0, 7, 32.0, 20.0, 62.0, -1, 0.0, "a limewashed house round its walled yard", 10, 11.0, 11.0, 1 },
			{ "dsr_mud", 0, 0, 1, 3, 32.0, 20.0, 62.0, -1, 0.0, "bare mud; the yard wall bare above the waist", 10, 11.0, 11.0, 1 },
			{ "dsr_two", 0, 0, 2, 5, 32.0, 20.0, 62.0, -1, 0.0, "another: its main room of two storeys", 10, 11.0, 11.0, 2 },
			{ "dsr_back", 0, 0, 0, 7, 205.0, 14.0, 250.0, -1, 0.0, "from behind", 10, 11.0, 11.0, 1 },
			{ "dsr_above", 0, 0, 0, 7, -30.0, 48.0, 62.0, -1, 0.0, "from above: the roofs, the stair, the yard", 10, 11.0, 11.0, 1 },
			{ "dsr_mud_back", 0, 0, 1, 3, 160.0, 12.0, 110.0, -1, 0.0, "the mud house from behind", 10, 11.0, 11.0, 1 },
			{ "dsr_door", 0, 0, 0, 7, 16.0, 8.0, 62.0, 1, 5.0, "close: the door, its lintel, the water jars", 10, 11.0, 11.0, 1 },
			{ "dsr_window", 0, 0, 0, 7, -14.0, 6.0, 62.0, 0, 3.2, "close: a window, its shutter or its bars", 10, 11.0, 11.0, 1 },
			{ "dsr_gate", 0, 0, 0, 7, 14.0, 7.0, 62.0, 27, 6.0, "close: the yard's gate", 10, 11.0, 11.0, 1 },
			{ "dsr_spout", 0, 0, 0, 7, 196.0, 10.0, 250.0, 121, 4.5, "close: a spout, the roof's mud run down", 10, 11.0, 11.0, 1 },
			{ "dsr_scab", 0, 0, 0, 7, 262.0, 6.0, 250.0, 124, 3.6, "close: plaster come away under the parapet", 10, 11.0, 11.0, 1 },
			{ "dsr_foot", 0, 0, 0, 54, 84.0, 8.0, 62.0, 327, 4.6, "close: a wall's foot the damp has stripped", 10, 11.0, 11.0, 1 },
			{ "dsr_corner", 0, 0, 0, 20, 236.0, 8.0, 250.0, 124, 3.8, "close: the plaster come away round a corner", 10, 11.0, 11.0, 1 },
			{ "dsr_yard_corner", 0, 0, 0, 20, 325.0, 8.0, 62.0, 224, 3.6, "close: and round a corner of the yard wall", 10, 11.0, 11.0, 1 },
			{ "dsr_fallen", 0, 0, 0, 7, 262.0, 10.0, 250.0, 127, 4.2, "close: fallen plaster at the wall's foot", 10, 11.0, 11.0, 1 },
			{ "dsr_worn", 0, 0, 0, 54, 205.0, 14.0, 250.0, -1, 0.0, "a house let go, from behind", 10, 11.0, 11.0, 1 },
			{ "dsr_rug", 0, 0, 0, 12, 10.0, 12.0, 62.0, 134, 3.0, "close: a rug airing on the wing's parapet", 10, 11.0, 11.0, 1 },
			{ "dsr_niche", 0, 0, 0, 12, 16.0, 5.0, 62.0, 131, 2.6, "close: the niches over the door, recessed", 10, 11.0, 11.0, 1 },
			{ "dsr_oven", 0, 0, 0, 7, 150.0, 24.0, 62.0, 119, 3.6, "close: the bread oven, its brushwood", 10, 11.0, 11.0, 1 },
			{ "dsr_stair", 0, 0, 0, 7, 10.0, 22.0, 62.0, 120, 6.5, "close: the stair to the roof", 10, 11.0, 11.0, 1 },
			{ "dsr_shade", 0, 0, 0, 7, -60.0, 30.0, 62.0, 122, 7.0, "close: the shade of palm fronds", 10, 11.0, 11.0, 1 },
			{ "dsr_roof", 0, 0, 0, 7, 10.0, 40.0, 62.0, 125, 5.0, "close: the wing's roof, its parapet", 10, 11.0, 11.0, 1 },
			{ "dsr_mud_sun", 0, 0, 1, 3, 205.0, 14.0, 205.0, -1, 0.0, "the mud house, sun square on its back", 10, 11.0, 11.0, 1 },
			{ "dsr_mud_door", 0, 0, 1, 3, 16.0, 8.0, 62.0, 1, 5.0, "mud house: door, limewash brushed round it", 10, 11.0, 11.0, 1 },
			{ "dsr_mud_wall", 0, 0, 1, 3, 24.0, 8.0, 62.0, 126, 4.0, "the mud house: the yard wall's bare bricks", 10, 11.0, 11.0, 1 },
			{ "dsr_mud_rug", 0, 0, 1, 3, 30.0, 16.0, 350.0, 134, 2.8, "the mud house: its rug", 10, 11.0, 11.0, 1 },
			{ "dsr_mud_foot", 0, 0, 1, 3, 160.0, 8.0, 110.0, 127, 3.6, "mud house: a wall's foot the damp stripped", 10, 11.0, 11.0, 1 },
			{ "dsr_mid", 0, 1, 0, 7, 32.0, 20.0, 62.0, -1, 0.0, "the middle level (from about 90 m)", 10, 11.0, 11.0, 1 },
			{ "dsr_far", 0, 2, 0, 7, 32.0, 20.0, 62.0, -1, 0.0, "the far level (from 300 m)", 10, 11.0, 11.0, 1 },
			{ "dsr_switch90_near", 0, 0, 0, 7, 32.0, 5.0, 62.0, 3, 55.0, "at 90 m: near level", 10, 11.0, 11.0, 1 },
			{ "dsr_switch90_mid", 0, 1, 0, 7, 32.0, 5.0, 62.0, 3, 55.0, "at 90 m: middle level", 10, 11.0, 11.0, 1 },
			{ "dsr_switch300_mid", 0, 1, 0, 7, 32.0, 5.0, 62.0, 3, 184.0, "at 300 m: middle level", 10, 11.0, 11.0, 1 },
			{ "dsr_switch300_far", 0, 2, 0, 7, 32.0, 5.0, 62.0, 3, 184.0, "at 300 m: far level", 10, 11.0, 11.0, 1 },
		};
		// LAND_KIT_ONLY=<name,name>: only these panels (and a switch's two halves), for a quick look
		const char* Only = std::getenv("LAND_KIT_ONLY");
		const std::vector<HouseLook> Looks = bDesertSheet ? std::vector<HouseLook>(std::begin(DesertLooks), std::end(DesertLooks))
			: bEastSheet ? std::vector<HouseLook>(std::begin(EastLooks), std::end(EastLooks))
			: bVillageSheet ? std::vector<HouseLook>(std::begin(VillageLooks), std::end(VillageLooks))
			: bChurchSheet ? std::vector<HouseLook>(std::begin(ChurchLooks), std::end(ChurchLooks))
			: bFarmSheet ? std::vector<HouseLook>(std::begin(FarmLooks), std::end(FarmLooks)) : std::vector<HouseLook>(std::begin(HouseLooks), std::end(HouseLooks));
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
			P.HouseLong = K.Long;
			P.HouseDeep = K.Deep;
			P.HouseStoreys = K.Storeys;
			P.HouseSnow = K.Snow;
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
					if (H.Face == 0 && ((K.Aim == 0 && (((H.What == 0 || H.What == 7) && H.Z0 < 2.0) || H.What == 9)) || (K.Aim == 1 && (H.What == 1 || H.What == 3 || H.What == 6 || H.What == 10)) || (K.Aim == 9 && H.What == 12)))
					{
						At = V3(H.X, H.Y, 0.15 + 0.5 * (H.Z0 + H.Z1));
						break;
					}
				}
				// (a village house's render: the biggest place up its front where it has come away, 13; or the thickest of its
				// strikes there, 14, the one with most others within half a metre)
				if (K.Aim == 13)
				{
					double Most = 0.0;
					for (const LandHouse::Hole& H : Holes)
						if (H.What == 13 && H.Face == 0 && H.Z0 > 1.0 && H.Wide * (H.Z1 - H.Z0) > Most)
						{
							Most = H.Wide * (H.Z1 - H.Z0);
							At = V3(H.X, H.Y, 0.15 + 0.5 * (H.Z0 + H.Z1));
						}
				}
				if (K.Aim == 14)
				{
					int Most = -1;
					for (const LandHouse::Hole& H : Holes)
					{
						if (H.What != 14 || H.Face != 0) continue;
						int N = 0;
						for (const LandHouse::Hole& O : Holes)
							N += O.What == 14 && O.Face == 0 && std::hypot(O.X - H.X, O.Z0 - H.Z0) < 0.5;
						if (N > Most)
						{
							Most = N;
							At = V3(H.X, H.Y, 0.15 + H.Z0);
						}
					}
				}
				if (K.Aim == 15) At = V3(0.5 * K.Long - 1.2, 0.5 * K.Deep, 0.15 + 4.2);
				if (K.Aim == 16) At = V3(-0.5 * K.Long, 0.0, 0.15 + 6.9);
				// (the izba's: its street gable, the +x one, at its windows' tops; its front corner there; its ridge's end)
				if (K.Aim == 17) At = V3(0.5 * K.Long - 0.4, -0.3, 0.15 + 2.6);
				if (K.Aim == 18) At = V3(0.5 * K.Long - 0.45, 0.5 * K.Deep - 0.9, 0.15 + 1.2);
				if (K.Aim == 19) At = V3(0.5 * K.Long - 0.3, -0.3, 0.15 + LandHouse::RidgeOf(HouseSpec(P)) - 0.6);
				// (the khata's: its eaves' front corner over the +x end; a hip by the ridge's end)
				if (K.Aim == 20) At = V3(0.5 * K.Long - 0.7, 0.5 * K.Deep - 0.7, 0.15 + LandHouse::EavesOf(HouseSpec(P)) - 0.3);
				if (K.Aim == 21) At = V3(0.5 * K.Long - 2.2, 0.0, 0.15 + LandHouse::RidgeOf(HouseSpec(P)) - 0.5);
				// (the burnt-out house's: its stove's mouth, its chimney's top, its front corner at the ground; 25 its stove's
				// side to the room, looked at from a little off its square)
				double SideBearing = -1.0;
				for (const LandHouse::Hole& H : Holes)
				{
					if (K.Aim == 22 && H.What == 15) At = V3(H.X, H.Y, 0.5 * (H.Z0 + H.Z1));
					if (K.Aim == 23 && H.What == 16) At = V3(H.X, H.Y, H.Z1 - 0.35);
					if (K.Aim == 25 && H.What == 17)
					{
						At = V3(H.X, H.Y, 0.5 * (H.Z0 + H.Z1));
						SideBearing = H.Face == 1 ? 90.0 + K.Bearing : 270.0 - K.Bearing;
					}
				}
				if (K.Aim == 24) At = V3(0.5 * K.Long - 0.6, 0.5 * K.Deep - 0.6, 0.35);
				// (26: a khata's front wall's broken top, along from its corner)
				if (K.Aim == 26) At = V3(0.5 * K.Long - 2.4, 0.5 * K.Deep - 0.3, 1.5);
				// (the desert house's: 27 its yard's gate; 100 and over, the first of a kind; 200 and over, the second)
				{
					int Seen = 0;
					for (const LandHouse::Hole& H : Holes)
					{
						if (K.Aim == 27 && H.What == 18) At = V3(H.X, H.Y, 1.1);
						if (K.Aim >= 100 && H.What == K.Aim % 100 && Seen++ == K.Aim / 100 - 1) At = V3(H.X, H.Y, 0.04 + 0.5 * (H.Z0 + H.Z1));
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
				// (the church's tower stands west of its nave, 18 m long, the tower 5.8 m square, its cornice at 17.5 m and its
				// spire's point at 28 m)
				if (K.Aim == 4) At = V3(-11.9, 0.0, 0.15 + 19.6);
				if (K.Aim == 5) At = V3(-14.8, 0.0, 0.15 + 4.0);
				if (K.Aim == 6) At = V3(-11.9, 0.0, 0.15 + 29.7);
				if (K.Aim == 7) At = V3(-8.6, 0.0, 0.15 + 10.5);
				if (K.Aim == 8) At = V3(-11.9, 2.9, 0.15 + 6.4);
				P.bAim = true;
				P.AimAt = At;
				const double Bearing = SideBearing >= 0.0 ? SideBearing : K.Bearing;
				P.AimFrom = V3(std::sin(Bearing * Rad) * std::cos(K.Up * Rad), std::cos(Bearing * Rad) * std::cos(K.Up * Rad), std::sin(K.Up * Rad));
				P.AimDistance = K.AimDistance;
			}
			Panels.push_back(P);
		}
	}
	// (LAND_KIT_JAPAN=1: Japan's own tank and field gun instead, release 2's Battle rules row "Japan's own tank and gun"
	// (-TheaterLandRuleJapan): the Type 97 and the 75 mm gun beside the generic light tank and the German howitzer they
	// stand in for today, each in Japan's own coat)
	const bool bJapanSheet = std::getenv("LAND_KIT_JAPAN") != nullptr;
	struct JapanLook
	{
		const char* File;
		const char* Model;
		const char* Nation;
		int Year, Month;
		double Turret, Gun, Hatches, FireAge, DeathAge, Bearing, Up, Soot;
		bool bBrewUp;
		const char* Coat;
		const char* Caption;
	};
	// (the gun, the light tank and the lorry are shared with other armies and have no Japanese coat in their files: the
	// rules send one for her whole army with the row on, blocks[].coat "japan", and the game keeps it with its other
	// owners' coats)
	const JapanLook JapanLooks[] = {
		{ "jap_type97_a", "Type97", "JAP", 1942, 5, -40.0, 4.0, 1.0, -1.0, -1.0, 42.0, 17.0, 0.0, false, "", "Type 97 Chi-Ha, May 1942: turret 40\xC2\xB0 off, hatches open" },
		{ "jap_type97_b", "Type97", "JAP", 1943, 8, 0.0, 6.0, 0.0, 0.08, -1.0, 72.0, 15.0, 0.0, false, "", "Type 97, August 1943: her later coat, just after a shot" },
		{ "jap_type97_c", "Type97", "JAP", 1942, 5, -14.0, -6.0, 0.0, -1.0, 3.5, 36.0, 18.0, 0.35, false, "", "Type 97: knocked out, hatches thrown, gun drooped" },
		{ "jap_type97_d", "Type97", "JAP", 1944, 3, 0.0, 0.0, 0.0, -1.0, 4.5, -48.0, 20.0, 1.0, true, "", "Type 97: brewed up and burnt out" },
		{ "jap_gun75", "Gun75", "JAP", 1942, 5, 0.0, 35.0, 0.0, -1.0, -1.0, 62.0, 14.0, 0.0, false, "japan", "75 mm field gun in Japan's coat: deployed, laid at 35\xC2\xB0" },
		{ "jap_gun75_fire", "Gun75", "JAP", 1942, 5, 0.0, 18.0, 0.0, 0.06, -1.0, 48.0, 12.0, 0.0, false, "japan", "the 75 mm gun just after a shot, the barrel back" },
		{ "jap_light", "LightTank", "JAP", 1942, 5, -40.0, 4.0, 1.0, -1.0, -1.0, 42.0, 17.0, 0.0, false, "japan", "the light tank that still plays her light blocks, in Japan's coat" },
		{ "jap_lorry", "Truck", "JAP", 1942, 5, 0.0, 0.0, 0.0, -1.0, -1.0, 52.0, 16.0, 0.0, false, "japan", "the lorry her infantry ride, in Japan's coat" },
		{ "jap_type97_side", "Type97", "JAP", 1942, 5, 0.0, 2.0, 0.0, -1.0, -1.0, 90.0, 5.0, 0.0, false, "", "Type 97 from the side, May 1942: her road wheels' rubber tyres" },
		{ "jap_type97_side43", "Type97", "JAP", 1943, 8, 0.0, 2.0, 0.0, -1.0, -1.0, -90.0, 7.0, 0.0, false, "", "her other side, August 1943: green, earth and light brown" },
		{ "jap_gun75_was", "Gun75", "JAP", 1942, 5, 0.0, 35.0, 0.0, -1.0, -1.0, 62.0, 14.0, 0.0, false, "", "round 1: the same gun in her maker's olive" },
		{ "jap_light_was", "LightTank", "JAP", 1942, 5, -40.0, 4.0, 1.0, -1.0, -1.0, 42.0, 17.0, 0.0, false, "", "round 1: the light tank as it was" },
	};
	for (const JapanLook& K : JapanLooks)
	{
		if (!bJapanSheet)
		{
			break;
		}
		Panel P;
		P.File = K.File;
		P.ModelName = K.Model;
		P.Nation = K.Nation;
		P.Year = K.Year;
		P.Month = K.Month;
		P.Pin.Turret = K.Turret;
		P.Pin.Gun = K.Gun;
		P.Pin.Hatches = K.Hatches;
		P.Pin.FireAge = K.FireAge;
		P.Pin.DeathAge = K.DeathAge;
		P.Pin.bBrewUp = K.bBrewUp;
		P.CameraBearing = K.Bearing;
		P.CameraUp = K.Up;
		P.SootShare = K.Soot;
		P.Coat = K.Coat;
		P.Caption = K.Caption;
		Panels.push_back(P);
	}
	// (LAND_KIT_SUPPORT=1: the support teams' weapons instead, release 2: each army's machine gun on its mount from her
	// quarter and from the gunner's side, then each army's mortar; =side for every one square from the side)
	const bool bSupportSheet = std::getenv("LAND_KIT_SUPPORT") != nullptr;
	const std::string SupportSet = bSupportSheet ? std::getenv("LAND_KIT_SUPPORT") : "";
	struct SupportLook
	{
		const char* File;
		const char* Model;
		// 0: the whole of her with the 1.75 m bar beside her (on her right; -1: on her left), seen from the side her belt
		// comes in at; 2: close on a socket
		int Kind;
		double Bearing, Up;
		const char* Socket;
		double Distance;
		const char* Caption;
	};
	const SupportLook SupportLooks[] = {
		// (in fours for a sheet four across: each machine gun whole and then close on her feed, two guns to a row)
		{ "sup_mg42", "HmgMG42", 0, -38.0, 18.0, "", 0.0, "Germany: MG 42 on the Lafette, her belt up out of its box" },
		{ "sup_mg42_feed", "HmgMG42", 2, -52.0, 30.0, "feed", 1.35, "every round in its link, over the tray and in under the feed cover" },
		{ "sup_mg34", "HmgMG34", 0, -38.0, 18.0, "", 0.0, "Germany until mid 1942: MG 34 in the same mount" },
		{ "sup_mg34_feed", "HmgMG34", 2, -52.0, 30.0, "feed", 1.35, "the MG 34's round jacket full of holes, the same belt" },
		{ "sup_m1919", "HmgM1919", 0, -38.0, 18.0, "", 0.0, "United States: Browning M1919A4 on the M2 tripod" },
		{ "sup_m1919_feed", "HmgM1919", 2, -52.0, 30.0, "feed", 1.25, "her cloth belt in under the top cover, the fired end out at the right" },
		{ "sup_m1917", "HmgM1917", 0, -38.0, 18.0, "", 0.0, "the water-cooled Browning (Poland's wz. 30)" },
		{ "sup_m1917_feed", "HmgM1917", 2, -52.0, 30.0, "feed", 1.35, "the same close: her belt, the smooth water jacket" },
		{ "sup_vickers", "HmgVickers", -1, 38.0, 16.0, "", 0.0, "Britain: Vickers on the Mk IV tripod, its condenser can" },
		{ "sup_vickers_feed", "HmgVickers", 2, 52.0, 30.0, "feed", 1.35, "her belt in through the feed block from the right; the crank handle" },
		{ "sup_maxim", "HmgMaxim", -1, 38.0, 16.0, "", 0.0, "Soviet Union: Maxim on the Sokolov wheeled mount" },
		{ "sup_maxim_feed", "HmgMaxim", 2, 128.0, 30.0, "feed", 1.5, "from behind her shield: the belt over the wheel and in, the spade grips" },
		{ "sup_type92", "HmgType92", 0, -38.0, 18.0, "", 0.0, "Japan: Type 92, a strip of thirty in from the left" },
		{ "sup_type92_feed", "HmgType92", 2, -52.0, 30.0, "feed", 1.25, "thirty on the strip going in; it comes out bare at the right, her cases thrown clear" },
		{ "sup_hotchkiss", "HmgHotchkiss", 0, -38.0, 18.0, "", 0.0, "France (and China): Hotchkiss of 1914" },
		{ "sup_hotchkiss_feed", "HmgHotchkiss", 2, -52.0, 30.0, "feed", 1.25, "twenty-four on the strip; the five great rings" },
		{ "sup_breda", "HmgBreda37", 0, -38.0, 18.0, "", 0.0, "Italy: Breda Mod. 37" },
		{ "sup_breda_feed", "HmgBreda37", 2, -52.0, 30.0, "feed", 1.25, "her strip of twenty going in; her fired cases are put back in it" },
		{ "sup_mortar_ger", "MortarGER", -1, 50.0, 14.0, "", 0.0, "Germany: 8 cm mortar, three bombs with fins and fuzes laid ready" },
		{ "sup_mortar_ger_sight", "MortarGER", 2, -140.0, 24.0, "sight", 1.5, "from behind and left: the sight, the collar on its two shock absorbers" },
		{ "sup_mortar_usa", "MortarUSA", -1, 50.0, 14.0, "", 0.0, "United States: 81 mm mortar" },
		{ "sup_mortar_uk", "MortarUK", -1, 50.0, 14.0, "", 0.0, "Britain: 3-inch mortar" },
		{ "sup_mortar_sov", "MortarSOV", -1, 50.0, 14.0, "", 0.0, "Soviet Union: 82 mm mortar on its round plate" },
		{ "sup_mortar_jap", "MortarJAP", -1, 50.0, 14.0, "", 0.0, "Japan: Type 97 mortar" },
		{ "sup_mortar_ita", "MortarITA", -1, 50.0, 14.0, "", 0.0, "Italy: 81 mm mortar" },
		{ "sup_mortar_fra", "MortarFRA", -1, 50.0, 14.0, "", 0.0, "France: 81 mm Brandt mortar" },
		{ "sup_mg42_back", "HmgMG42", 2, -155.0, 24.0, "shoulder", 1.7, "as her gunner has her: the bakelite butt, the sight low at the left" },
	};
	for (const SupportLook& K : SupportLooks)
	{
		if (!bSupportSheet)
		{
			break;
		}
		Panel P;
		P.File = K.File;
		P.ModelName = K.Model;
		P.Nation = "any";
		P.CameraBearing = SupportSet == "side" ? 90.0 : K.Bearing;
		P.CameraUp = SupportSet == "side" ? 4.0 : K.Up;
		P.Caption = K.Caption;
		// (the battle's daylight; =warm for the low morning sun the vehicles' sheets have)
		P.bDay = SupportSet != "warm";
		if (K.Kind == 2)
		{
			if (SupportSet == "side")
			{
				continue;
			}
			P.CloseSocket = K.Socket;
			P.CloseDistance = K.Distance;
		}
		else
		{
			P.Bar = K.Kind < 0 ? -1.75 : 1.75;
		}
		Panels.push_back(P);
	}
	// (LAND_KIT_BURNT=1: the burnt coat's own sheet instead, release 2's trial word "burntcoat": a grey hull, an olive one
	// and Japan's khaki, each burnt out as it was (one sooty colour) and with the coat (soot, rust and ash), from her
	// quarter and from above and behind; then one burnt out among live ones from 150 m, both ways)
	const bool bBurntSheet = std::getenv("LAND_KIT_BURNT") != nullptr;
	struct BurntLook
	{
		const char* File;
		const char* Model;
		const char* Nation;
		int Year, Month;
		double Turret, Bearing, Up;
		bool bBrewUp, bCoat;
		const char* Caption;
	};
	const BurntLook BurntLooks[] = {
		{ "burnt_pz4_was", "PanzerIV", "GER", 1941, 6, -14.0, 36.0, 18.0, false, false, "Panzer IV in grey, burnt out: as it was, one sooty colour" },
		{ "burnt_pz4", "PanzerIV", "GER", 1941, 6, -14.0, 36.0, 18.0, false, true, "the same with the burnt coat: soot, rust and ash" },
		{ "burnt_pz4_top", "PanzerIV", "GER", 1941, 6, -14.0, -142.0, 40.0, false, true, "from above and behind: her decks and engine louvres" },
		{ "burnt_sherman_was", "Sherman", "USA", 1944, 7, -14.0, 36.0, 18.0, false, false, "Sherman in olive, burnt out: as it was" },
		{ "burnt_sherman", "Sherman", "USA", 1944, 7, -14.0, 36.0, 18.0, false, true, "the same with the burnt coat" },
		{ "burnt_sherman_top", "Sherman", "USA", 1944, 7, -14.0, -142.0, 40.0, false, true, "from above and behind" },
		{ "burnt_type97_was", "Type97", "JAP", 1942, 5, 0.0, -48.0, 20.0, true, false, "Type 97 in khaki, brewed up: as it was" },
		{ "burnt_type97", "Type97", "JAP", 1942, 5, 0.0, -48.0, 20.0, true, true, "the same with the burnt coat, her turret thrown" },
		{ "burnt_type97_side", "Type97", "JAP", 1942, 5, 0.0, 90.0, 7.0, false, true, "from the side: her tyres burnt to ash on the rims, her tracks rusted" },
		// (LAND_KIT_BURNT=more: the other kinds of thing that burn, for a look at every army's)
		{ "more_t34", "T34", "SOV", 1943, 7, -14.0, 36.0, 18.0, false, true, "T-34" },
		{ "more_tiger", "Tiger", "GER", 1944, 7, -14.0, 36.0, 18.0, false, true, "Tiger" },
		{ "more_truck", "Truck", "GER", 1944, 7, 0.0, 40.0, 18.0, false, true, "lorry" },
		{ "more_hanomag", "Hanomag", "GER", 1944, 7, 0.0, 40.0, 18.0, false, true, "half-track" },
		{ "more_jeep", "Jeep", "USA", 1944, 7, 0.0, 40.0, 20.0, false, true, "jeep" },
		{ "more_gun", "leFH18", "GER", 1944, 7, 0.0, 62.0, 14.0, false, true, "field gun" },
		{ "more_light", "LightTank", "UK", 1942, 5, -14.0, 36.0, 18.0, false, true, "light tank" },
		{ "more_semovente", "Semovente", "ITA", 1942, 10, 0.0, 36.0, 18.0, false, true, "Semovente" },
		{ "more_panther_top", "Panther", "GER", 1944, 7, -14.0, -142.0, 40.0, false, true, "Panther from above and behind" },
		{ "more_m3", "M3Halftrack", "USA", 1944, 7, 0.0, 40.0, 24.0, false, true, "American half-track" },
		{ "more_stug", "StuG3", "GER", 1943, 7, 0.0, 36.0, 18.0, false, true, "assault gun" },
		{ "more_car", "ArmouredCar", "UK", 1942, 5, -14.0, 36.0, 18.0, false, true, "armoured car" },
		// (LAND_KIT_BURNT=bikes: the smallest things that burn, a motorcycle and its sidecar)
		{ "bike_r75_was", "R75", "GER", 1942, 6, 0.0, 40.0, 20.0, false, false, "motorcycle and sidecar, burnt out: as it was" },
		{ "bike_r75", "R75", "GER", 1942, 6, 0.0, 40.0, 20.0, false, true, "the same with the burnt coat" },
		{ "bike_r75_far", "R75", "GER", 1942, 6, 0.0, -130.0, 30.0, false, true, "from above and behind" },
		{ "bike_rikuo", "Rikuo", "JAP", 1942, 5, 0.0, 40.0, 20.0, false, true, "Japan's motorcycle and sidecar" },
		{ "bike_m72", "M72", "SOV", 1943, 7, 0.0, 40.0, 20.0, false, true, "the Soviet one" },
		{ "bike_solo", "Motorcycle", "GER", 1942, 6, 0.0, 40.0, 20.0, false, true, "a motorcycle" },
	};
	const std::string BurntSet = bBurntSheet ? std::getenv("LAND_KIT_BURNT") : "";
	const bool bBurntMore = BurntSet == "more" || BurntSet == "bikes";
	for (const BurntLook& K : BurntLooks)
	{
		if (!bBurntSheet)
		{
			break;
		}
		const std::string Head = std::string(K.File).substr(0, 5);
		if ((Head == "more_" ? "more" : Head == "bike_" ? "bikes" : "") != (bBurntMore ? BurntSet : std::string()))
		{
			continue;
		}
		Panel P;
		P.File = K.File;
		P.ModelName = K.Model;
		P.Nation = K.Nation;
		P.Year = K.Year;
		P.Month = K.Month;
		P.Pin.Turret = K.Turret;
		P.Pin.DeathAge = 4.5;
		P.Pin.bBrewUp = K.bBrewUp;
		P.CameraBearing = K.Bearing;
		P.CameraUp = K.Up;
		P.SootShare = 1.0;
		P.bBurntCoat = K.bCoat;
		// (her marks the game's own, painted on her skin, so they burn with it as they do in the game)
		P.bMarks = true;
		P.Caption = K.Caption;
		Panels.push_back(P);
	}
	for (int Coat = 0; Coat < 2 && bBurntSheet && !bBurntMore; ++Coat)
	{
		// (a troop of Shermans crossing the picture, 150 m off: the second burnt out, the fourth knocked out and not burnt)
		Panel P;
		P.File = Coat ? "burnt_150" : "burnt_150_was";
		P.ModelName = "Sherman";
		P.Nation = "USA";
		P.Pin.X = -16.0;
		P.Pin.Y = 3.0;
		P.Pin.Heading = 0.2;
		P.Pin.Turret = 20.0;
		P.bBurntCoat = Coat != 0;
		const double At[3][3] = { { -5.0, -2.0, -0.3 }, { 6.0, 2.5, 0.1 }, { 17.0, -1.0, 0.5 } };
		for (int I = 0; I < 3; ++I)
		{
			Panel::Other O;
			O.ModelName = "Sherman";
			O.Nation = "USA";
			O.Pin.X = At[I][0];
			O.Pin.Y = At[I][1];
			O.Pin.Heading = At[I][2];
			O.Pin.Turret = I == 1 ? -30.0 : -14.0;
			if (I == 0)
			{
				O.Pin.DeathAge = 4.5;
				O.SootShare = 1.0;
			}
			if (I == 2)
			{
				// (knocked out and never alight: the game lays no soot on her, LandKit's Vehicle::Soot rises only as she burns)
				O.Pin.DeathAge = 3.5;
			}
			P.Others.push_back(O);
		}
		P.bAim = true;
		P.AimAt = V3(0.0, 0.0, 1.2);
		P.AimFrom = V3(0.35, -0.92, 0.12);
		P.AimDistance = 150.0;
		P.Caption = Coat ? "150 m, with the coat: the second is burnt out, the last knocked out" : "150 m, as it was: the second is burnt out, the last knocked out";
		Panels.push_back(P);
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
		if (bMarksSheet || bHouseSheet || bJapanSheet || bBurntSheet || bSupportSheet || bAnyYearSheet)
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
	if (!bMarksSheet && !bHouseSheet && !bJapanSheet && !bBurntSheet && !bSupportSheet && !bAnyYearSheet)
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
			// (a burnt-out house lies in the yard, with no floor to stand over it; in winter the yard's snow round it)
			// (a desert house's yard is the ground's own level, a step over it)
			AddHouse(M, P.HouseKind == 9 ? 0.0 : P.HouseKind == 10 ? 0.04 : 0.15, S);
			{
				const LandHouse::Spec Hs = HouseSpec(P);
				if (Hs.Kind == 9) S.GroundSnow = Smooth(0.03, 0.5, Hs.Snow);
				if (Hs.Kind == 10) S.GroundSand = 1.0;
				S.Feet.push_back({ -0.5 * Hs.Long, -0.5 * Hs.Deep, 0.5 * Hs.Long, 0.5 * Hs.Deep });
				if (Hs.Kind == 3)
				{
					S.Feet.push_back({ 0.5 * Hs.Long, -3.3, 0.5 * Hs.Long + 7.2, 3.3 });
					S.Feet.push_back({ -0.5 * Hs.Long - 5.8, -2.9, -0.5 * Hs.Long, 2.9 });
				}
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
		if (!P.CloseSocket.empty())
		{
			// (a close look at one of her sockets, from the look's own bearing and height)
			V3 SocketPlace, SocketWay;
			if (V.SocketAt(P.CloseSocket, 0.0, SocketPlace, SocketWay))
			{
				const double CloseBearing = P.CameraBearing * Rad + P.Pin.Heading, CloseUp = P.CameraUp * Rad;
				P.bAim = true;
				P.AimAt = SocketPlace;
				P.AimFrom = V3(std::sin(CloseBearing) * std::cos(CloseUp), std::cos(CloseBearing) * std::cos(CloseUp), std::sin(CloseUp));
				P.AimDistance = P.CloseDistance;
			}
			else
			{
				std::printf("  %s has no socket %s to look at\n", P.ModelName.c_str(), P.CloseSocket.c_str());
			}
		}
		if (P.Bar != 0.0)
		{
			// (a man's height standing by her: 0.7 m to her right, or her left, a little behind her pin; the picture is
			// framed on her and on the bar's first metre, so a low weapon is not lost under it)
			const double Side = P.Bar > 0.0 ? 0.7 : -0.7, Back = -0.25;
			const double Sh = std::sin(P.Pin.Heading), Ch = std::cos(P.Pin.Heading);
			P.FrameHigh = std::max(S.Hi.Z + 0.10, 1.0);
			AddBar(P.Pin.X + Sh * Back + Ch * Side, P.Pin.Y + Ch * Back - Sh * Side, std::fabs(P.Bar), S);
		}
		std::vector<std::unique_ptr<Vehicle>> Beside;
		for (const Panel::Other& O : P.Others)
		{
			Panel As = P;
			As.ModelName = O.ModelName;
			As.Nation = O.Nation;
			const Model* Om = ModelFor(O.ModelName, O.Nation, As);
			if (!Om)
			{
				return 1;
			}
			Beside.push_back(std::make_unique<Vehicle>());
			Beside.back()->PinPose(*Om, O.Pin, Flat);
			AddVehicle(*Beside.back(), O.SootShare, S);
		}
		const size_t Unmarked = S.Tris.size();
		if (!P.bMarks && P.Others.empty())
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
		if (bSupportSheet && P.Bar != 0.0)
		{
			// (the art director's Speed line: each file's triangles, near and far, lettered under her whole look)
			size_t NearTris = 0, FarTris = 0;
			for (const Body& Bo : M->Bodies)
			{
				NearTris += Bo.Near.Triangles.size() / 3;
				FarTris += Bo.Far.Triangles.size() / 3;
			}
			std::fprintf(Captions, "%s\t%s (%zu triangles near, %zu far; the post's bands are 25 cm)\n", P.File.c_str(), P.Caption.c_str(), NearTris, FarTris);
		}
		else
		{
			std::fprintf(Captions, "%s\t%s\n", P.File.c_str(), P.Caption.c_str());
		}
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
			{ "church_switch90", "church_switch90_near", "church_switch90_mid", 2, "church at 90 m, enlarged 2x: near | middle" },
			{ "church_switch300", "church_switch300_mid", "church_switch300_far", 4, "church at 300 m, enlarged 4x: middle | far" },
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
