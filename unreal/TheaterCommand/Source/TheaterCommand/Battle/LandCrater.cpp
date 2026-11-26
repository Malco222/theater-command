#include "Battle/LandCrater.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace LandCrater
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		/** A small steady random source (splitmix64): the same hole on every machine. */
		struct Dice
		{
			uint64_t State = 0;
			explicit Dice(uint64_t Seed) : State(Seed * 0xD1B54A32D192ED03ull + 0x632BE59BD9B4E019ull) {}
			double U()
			{
				State += 0x9E3779B97F4A7C15ull;
				uint64_t Z = State;
				Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
				Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
				Z ^= Z >> 31;
				return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
			}
			double U(double A, double B) { return A + (B - A) * U(); }
			int Whole(int A, int B) { return A + static_cast<int>(std::min(static_cast<double>(B - A), std::floor(U() * (B - A + 1)))); }
		};

		uint64_t Mix(uint64_t A, uint64_t B)
		{
			uint64_t Z = A ^ (B + 0x9E3779B97F4A7C15ull + (A << 6) + (A >> 2));
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}

		uint8_t Byte(double V)
		{
			return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(V)), 0, 255));
		}

		Rgb8 Rgb(int R, int G, int B)
		{
			Rgb8 C;
			C.R = Byte(R);
			C.G = Byte(G);
			C.B = Byte(B);
			return C;
		}

		Rgb8 Mixed(const Rgb8& A, const Rgb8& B, double T)
		{
			T = std::clamp(T, 0.0, 1.0);
			Rgb8 C;
			C.R = Byte(A.R + (B.R - A.R) * T);
			C.G = Byte(A.G + (B.G - A.G) * T);
			C.B = Byte(A.B + (B.B - A.B) * T);
			return C;
		}

		Rgb8 Toned(const Rgb8& A, double K)
		{
			Rgb8 C;
			C.R = Byte(A.R * K);
			C.G = Byte(A.G * K);
			C.B = Byte(A.B * K);
			return C;
		}

		double Smooth(double T)
		{
			T = std::clamp(T, 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}

		/** The angle from A to B, wrapped into -Pi..Pi. */
		double Turn(double A, double B)
		{
			double D = std::fmod(B - A, 2.0 * Pi);
			if (D > Pi) D -= 2.0 * Pi;
			if (D < -Pi) D += 2.0 * Pi;
			return D;
		}

		/** The rings round a hole's middle: five inside its crest, four out over its thrown soil (Lay's Rings). */
		constexpr int HoleRings = 9;
		/** Lay's ring that the cut runs along (the wall's 0.86: over the ground, short of the crest). */
		constexpr int CutRing = 3;
		/** A dug hole's soot at its middle: the burst's blackened soil (never a black disc: a share of it). */
		const Rgb8 Soot = { 46, 38, 30 };
		/** A clod's points: its peak and the five round its foot. */
		constexpr int ClodPoints = 6;

		/** The height of triangle A, B, C over X, Y (field metres), when X, Y is in it seen from above. */
		bool OnTri(const float* P, uint32_t A, uint32_t B, uint32_t C, double X, double Y, double& OutZ)
		{
			const float* Pa = P + static_cast<size_t>(A) * 3;
			const float* Pb = P + static_cast<size_t>(B) * 3;
			const float* Pc = P + static_cast<size_t>(C) * 3;
			const double Ux = Pb[0] - Pa[0], Uy = Pb[1] - Pa[1], Vx = Pc[0] - Pa[0], Vy = Pc[1] - Pa[1];
			const double Den = Ux * Vy - Uy * Vx;
			if (std::fabs(Den) < 1e-10)
			{
				return false;
			}
			const double Wx = X - Pa[0], Wy = Y - Pa[1];
			const double S = (Wx * Vy - Wy * Vx) / Den, T = (Ux * Wy - Uy * Wx) / Den;
			constexpr double Slack = -1e-6;
			if (S < Slack || T < Slack || S + T > 1.0 - Slack)
			{
				return false;
			}
			OutZ = Pa[2] + S * (Pb[2] - Pa[2]) + T * (Pc[2] - Pa[2]);
			return true;
		}

		/** As OnTri, with the shares of B and C at X, Y (A's the rest). */
		bool OnTriShares(const float* P, uint32_t A, uint32_t B, uint32_t C, double X, double Y, double& OutZ, double& OutS, double& OutT)
		{
			const float* Pa = P + static_cast<size_t>(A) * 3;
			const float* Pb = P + static_cast<size_t>(B) * 3;
			const float* Pc = P + static_cast<size_t>(C) * 3;
			const double Ux = Pb[0] - Pa[0], Uy = Pb[1] - Pa[1], Vx = Pc[0] - Pa[0], Vy = Pc[1] - Pa[1];
			const double Den = Ux * Vy - Uy * Vx;
			if (std::fabs(Den) < 1e-10)
			{
				return false;
			}
			const double Wx = X - Pa[0], Wy = Y - Pa[1];
			OutS = (Wx * Vy - Wy * Vx) / Den;
			OutT = (Ux * Wy - Uy * Wx) / Den;
			constexpr double Slack = -1e-6;
			if (OutS < Slack || OutT < Slack || OutS + OutT > 1.0 - Slack)
			{
				return false;
			}
			OutZ = Pa[2] + OutS * (Pb[2] - Pa[2]) + OutT * (Pc[2] - Pa[2]);
			return true;
		}

		/** A point into the mesh: its place and colour (its normal is summed from its triangles at the end). */
		uint32_t Put(LandField::Mesh& M, double X, double Y, double Z, const Rgb8& C)
		{
			const uint32_t Index = static_cast<uint32_t>(M.Colours.size());
			M.Positions.push_back(static_cast<float>(X));
			M.Positions.push_back(static_cast<float>(Y));
			M.Positions.push_back(static_cast<float>(Z));
			M.Normals.push_back(0.0f);
			M.Normals.push_back(0.0f);
			M.Normals.push_back(0.0f);
			M.Colours.push_back(C);
			return Index;
		}

		void Tri(LandField::Mesh& M, uint32_t A, uint32_t B, uint32_t C)
		{
			M.Triangles.push_back(A);
			M.Triangles.push_back(B);
			M.Triangles.push_back(C);
		}

		/** Each point's normal from the triangles round it (area-weighted), for the points from First on. */
		void Normals(LandField::Mesh& M, size_t FirstPoint, size_t FirstIndex)
		{
			const std::vector<float>& P = M.Positions;
			std::vector<float>& N = M.Normals;
			for (size_t T = FirstIndex; T + 2 < M.Triangles.size(); T += 3)
			{
				const size_t A = M.Triangles[T], B = M.Triangles[T + 1], C = M.Triangles[T + 2];
				const double Ux = P[B * 3] - P[A * 3], Uy = P[B * 3 + 1] - P[A * 3 + 1], Uz = P[B * 3 + 2] - P[A * 3 + 2];
				const double Vx = P[C * 3] - P[A * 3], Vy = P[C * 3 + 1] - P[A * 3 + 1], Vz = P[C * 3 + 2] - P[A * 3 + 2];
				const double Nx = Uy * Vz - Uz * Vy, Ny = Uz * Vx - Ux * Vz, Nz = Ux * Vy - Uy * Vx;
				for (const size_t K : { A, B, C })
				{
					N[K * 3] += static_cast<float>(Nx);
					N[K * 3 + 1] += static_cast<float>(Ny);
					N[K * 3 + 2] += static_cast<float>(Nz);
				}
			}
			for (size_t K = FirstPoint; K < M.Colours.size(); ++K)
			{
				const double Nx = N[K * 3], Ny = N[K * 3 + 1], Nz = N[K * 3 + 2];
				const double Long = std::sqrt(Nx * Nx + Ny * Ny + Nz * Nz);
				if (Long > 1e-12 && Nz > -0.2 * Long)
				{
					N[K * 3] = static_cast<float>(Nx / Long);
					N[K * 3 + 1] = static_cast<float>(Ny / Long);
					N[K * 3 + 2] = static_cast<float>(Nz / Long);
				}
				else
				{
					N[K * 3] = 0.0f;
					N[K * 3 + 1] = 0.0f;
					N[K * 3 + 2] = 1.0f;
				}
			}
		}

		/** The drawn ground's own normal at a place, as its near tiles' points are lit: Lit's (the ground's own, exact),
		 * or the slope across the tiles' spacing (250 m tiles of 60 squares, LandField's NearQuads); not the fine slope a hole's or a strip's own few
		 * triangles take from the ground under them. What lies flat on the ground lit as the ground round it is, in a low
		 * sun: a thrown soil's edge lit by its own slope stood out as a ruled polygon, a track's strip as a flat stripe
		 * (the art director's cut 4, shot 07, 22 November). */
		struct Up
		{
			float X = 0.0f, Y = 0.0f, Z = 1.0f;
		};
		Up GroundUp(const UpFn& Lit, const HeightFn& Height, double X, double Y)
		{
			if (Lit)
			{
				double Nx = 0.0, Ny = 0.0, Nz = 1.0;
				Lit(X, Y, Nx, Ny, Nz);
				Up U;
				if (Nz == Nz && Nz > 0.0)
				{
					U.X = static_cast<float>(Nx);
					U.Y = static_cast<float>(Ny);
					U.Z = static_cast<float>(Nz);
				}
				return U;
			}
			constexpr double S = 250.0 / 60.0;
			const double Dx = (Height(X + S, Y) - Height(X - S, Y)) / (2.0 * S), Dy = (Height(X, Y + S) - Height(X, Y - S)) / (2.0 * S);
			const double Long = std::sqrt(Dx * Dx + Dy * Dy + 1.0);
			Up U;
			if (Long == Long && Long > 0.0)
			{
				U.X = static_cast<float>(-Dx / Long);
				U.Y = static_cast<float>(-Dy / Long);
				U.Z = static_cast<float>(1.0 / Long);
			}
			return U;
		}
		/** Point K's normal W of the way from its own to Ground's (unit again). */
		void LeanTo(LandField::Mesh& M, size_t K, const Up& Ground, double W)
		{
			float* N = &M.Normals[K * 3];
			const double Nx = N[0] + (Ground.X - N[0]) * W, Ny = N[1] + (Ground.Y - N[1]) * W, Nz = N[2] + (Ground.Z - N[2]) * W;
			const double Long = std::sqrt(Nx * Nx + Ny * Ny + Nz * Nz);
			if (Long > 1e-9)
			{
				N[0] = static_cast<float>(Nx / Long);
				N[1] = static_cast<float>(Ny / Long);
				N[2] = static_cast<float>(Nz / Long);
			}
		}
		/** A hash of a place (to the centimetre) and a salt, as a share 0 to 1: the same at a piece's end as at the next
		 * piece's start, so a strip's ragged edge runs on unbroken. */
		double PlaceShare(double X, double Y, uint64_t Salt)
		{
			const uint64_t Qx = static_cast<uint64_t>(static_cast<int64_t>(std::llround(X * 100.0)));
			const uint64_t Qy = static_cast<uint64_t>(static_cast<int64_t>(std::llround(Y * 100.0)));
			return static_cast<double>(Mix(Mix(Qx, Qy), Salt) >> 11) * (1.0 / 9007199254740992.0);
		}
	}

	Size SizeOf(const std::string& Name, double CalibreMm)
	{
		Size S;
		auto Set = [&S](double Across, double Rim, double Reach, int Clods, double Clod, int Sides)
		{
			S.Across = Across;
			S.Rim = Rim;
			S.Reach = Reach;
			S.Clods = Clods;
			S.Clod = Clod;
			S.Sides = Sides;
		};
		// (by the calibre where the name says no more; the rims low, a lip and not a mound: the second cut's holes stood
		// as molehills, the art director's third item, 21 November: "a shell hole is a shallow dip with a low rim and a
		// dark fan of thrown clods")
		const double Mm = CalibreMm == CalibreMm && CalibreMm > 0.0 ? CalibreMm : 75.0;
		if (Name == "grenade") Set(0.55, 0.03, 1.7, 4, 0.06, 10);
		else if (Name == "light") Set(0.9, 0.06, 1.8, 7, 0.09, 12);
		else if (Name == "mortar") Set(1.3, 0.09, 1.9, 10, 0.12, 14);
		else if (Name == "howitzer" || Name == "field" || Name == "how105") Set(2.8, 0.19, 2.1, 20, 0.22, 20);
		else if (Name == "heavy" || Name == "how150") Set(4.0, 0.27, 2.2, 24, 0.3, 22);
		else if (Mm >= 140.0) Set(4.0, 0.27, 2.2, 24, 0.3, 22);
		else if (Mm >= 100.0) Set(2.8, 0.19, 2.1, 20, 0.22, 20);
		else if (Mm >= 85.0) Set(1.8, 0.13, 2.0, 14, 0.16, 16); // an 88's high explosive
		else if (Mm >= 70.0) Set(1.5, 0.11, 1.9, 12, 0.14, 16); // a 75's
		else if (Mm >= 20.0) Set(0.9, 0.06, 1.8, 7, 0.09, 12);
		// How deep it is dug (Brian, 22 November: "explosions should create craters"): soft Normandy loam under a
		// quick fuze, as the rounds were set against men in the open: an 81 mm bomb's bowl about 0.3 m at its middle, a
		// 75's 0.4, an 88's 0.45, a 105's 0.7, a 150's a metre. A grenade only scuffs the turf.
		if (S.Across >= 1.7) S.Deep = S.Across >= 3.5 ? 1.0 : S.Across >= 2.5 ? 0.7 : 0.45;
		else if (S.Across >= 1.4) S.Deep = 0.38;
		else if (S.Across >= 1.2) S.Deep = 0.3;
		else if (S.Across >= 0.8) S.Deep = 0.18;
		return S;
	}

	Soil SoilOf(const std::string& Ground)
	{
		// (the art director's to check: Normandy's brown loam under July grass. The hollow damp and dark, never black; the
		// crest broken dry soil, lighter; the thrown soil's far edge going toward the grass's olive, so it melts into it)
		Soil P;
		if (Ground == "town")
		{
			P.Hollow = Rgb(74, 68, 60);
			P.Wall = Rgb(104, 97, 87);
			P.Crest = Rgb(146, 139, 127);
			P.Thrown = Rgb(150, 144, 133);
			P.Edge = Rgb(128, 124, 112);
			P.Clod = Rgb(132, 126, 116);
			P.Turf = Rgb(112, 106, 96);
		}
		else if (Ground == "mud")
		{
			P.Hollow = Rgb(44, 36, 28);
			P.Wall = Rgb(60, 49, 38);
			P.Crest = Rgb(86, 71, 54);
			P.Thrown = Rgb(78, 65, 50);
			P.Edge = Rgb(74, 70, 50);
			P.Clod = Rgb(62, 51, 40);
			P.Turf = Rgb(66, 74, 42);
		}
		else if (Ground == "woods")
		{
			P.Hollow = Rgb(50, 39, 29);
			P.Wall = Rgb(70, 55, 40);
			P.Crest = Rgb(102, 83, 60);
			P.Thrown = Rgb(88, 72, 52);
			P.Edge = Rgb(74, 66, 46);
			P.Clod = Rgb(72, 57, 41);
			P.Turf = Rgb(62, 70, 38);
		}
		else if (Ground == "sand")
		{
			P.Hollow = Rgb(112, 92, 66);
			P.Wall = Rgb(140, 118, 88);
			P.Crest = Rgb(176, 152, 116);
			P.Thrown = Rgb(170, 147, 112);
			P.Edge = Rgb(164, 144, 112);
			P.Clod = Rgb(130, 110, 82);
			P.Turf = Rgb(120, 104, 76);
		}
		else if (Ground == "blackearth")
		{
			// (the steppe's chernozem, a metre deep: near black in the hollow, dry grey-brown crumbs on the crest, thrown
			// dark over the ripe corn and the buff grass; the art director's to check)
			P.Hollow = Rgb(34, 29, 25);
			P.Wall = Rgb(48, 41, 35);
			P.Crest = Rgb(80, 72, 62);
			P.Thrown = Rgb(58, 51, 44);
			P.Edge = Rgb(98, 92, 64);
			P.Clod = Rgb(52, 45, 38);
			P.Turf = Rgb(106, 102, 66);
		}
		else if (Ground == "snow")
		{
			P.Hollow = Rgb(46, 38, 31);
			P.Wall = Rgb(62, 52, 43);
			P.Crest = Rgb(92, 82, 72);
			P.Thrown = Rgb(84, 74, 64);
			P.Edge = Rgb(150, 148, 146);
			P.Clod = Rgb(70, 60, 50);
			P.Turf = Rgb(190, 194, 200);
		}
		else
		{
			P.Hollow = Rgb(46, 35, 26);
			P.Wall = Rgb(70, 54, 39);
			P.Crest = Rgb(112, 93, 68);
			P.Thrown = Rgb(82, 65, 46); // (a dark fan of fresh loam over the field: the art director, 21 November)
			P.Edge = Rgb(86, 86, 52);
			P.Clod = Rgb(74, 57, 40);
			P.Turf = Rgb(72, 86, 42);
		}
		// Crater variety (Size::Subsoil, Size::Scorch): the soil from under the topsoil, lighter than it (Normandy's
		// yellow-brown clay under the loam, the steppe's loess under its black earth, paler sand under the sun-dark
		// crust, plaster and brick dust in a town), and how far the burst's soot shows on the thin edge (snow's white
		// shows it far; sand a little; elsewhere the edge stays the ground's own colour)
		P.Sub = Ground == "town" ? Rgb(168, 160, 148)
			: Ground == "mud" ? Rgb(104, 88, 64)
			: Ground == "woods" ? Rgb(128, 104, 72)
			: Ground == "sand" ? Rgb(196, 176, 140)
			: Ground == "blackearth" ? Rgb(132, 112, 80)
			: Ground == "snow" ? Rgb(120, 98, 70)
			: Rgb(150, 124, 84);
		P.EdgeSoot = Ground == "snow" ? 1.0 : Ground == "sand" ? 0.35 : 0.0;
		return P;
	}

	void Lay(double X, double Y, const Size& S, const Soil& Paint, uint64_t Seed, const HeightFn& Height, double Lift, LandField::Mesh& Out,
		const ColourFn& Under, std::vector<double>* OutCut, const UpFn& Lit, const HeightFn& Thin, const std::function<bool(double X, double Y)>& Avoid)
	{
		if (OutCut)
		{
			OutCut->clear();
		}
		if (!(S.Across > 0.05) || !Height || !(X == X) || !(Y == Y))
		{
			return;
		}
		Dice D(Seed);
		const double R = 0.5 * S.Across;
		const double RimHigh = std::max(0.0, S.Rim);
		const double ReachOf = std::max(1.2, S.Reach);
		const double Feather = std::min(1.5, 0.9 * R) * std::clamp(S.Feather, 0.0, 1.0); // (how far past the thrown soil's reach it thins into the grass)
		const int N = std::clamp(S.Sides, 8, 48);
		const size_t FirstPoint = Out.Colours.size(), FirstIndex = Out.Triangles.size();

		// Its outline: a few slow waves round it and a little roughness, so no two holes are round alike; the crest's
		// height ragged from side to side; rays of thrown soil, narrow tongues running out past the rest.
		const double Waves[3] = { D.U(0.03, 0.08), D.U(0.02, 0.06), D.U(0.01, 0.04) };
		const double Phases[3] = { D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi) };
		const int Rays = D.Whole(4, 7);
		std::vector<std::pair<double, double>> RayOf(static_cast<size_t>(Rays));
		for (std::pair<double, double>& Ray : RayOf)
		{
			Ray = { D.U(0.0, 2.0 * Pi), D.U(0.35, 0.9) * (ReachOf - 1.0) };
		}
		const double Lean = D.U(0.0, 2.0 * Pi), LeanBy = D.U(0.0, 0.18); // (thrown more to one side: the shell came in at a slant)
		// Crater variety (Size::Vary): drawn from its own dice, so the hole's own draws are the same with it or without
		// it; none of it is read while Vary is 0.
		const bool bVary = S.Vary != 0;
		Dice V(bVary ? S.Vary : 1u);
		const double LongK = bVary ? std::clamp(S.Long, 1.0, 2.2) : 1.0;
		const double AxisA = std::pow(LongK, 0.6), AxisB = std::pow(LongK, -0.4);
		const double Throw = bVary ? std::clamp(S.Throw, 0.0, 1.0) : 0.0;
		const double Heading = S.Heading;
		// (its radius by the way from its middle: an ellipse along Heading, each point kept on its own side of the hole,
		// so Field::TopAt finds it by its angle as before)
		const auto Stretch = [&](double A)
		{
			const double C = std::cos(A - Heading) / AxisA, Sn = std::sin(A - Heading) / AxisB;
			return 1.0 / std::sqrt(C * C + Sn * Sn);
		};
		// (its thrown soil's reach by its way from the round's: two wings some 60 degrees either side of straight on, a
		// fan straight on, little behind, where the burst's cone threw nothing)
		const auto Wings = [&](double A)
		{
			const double Dd = std::fabs(Turn(Heading, A));
			const double Wq = (Dd - 1.05) / 0.42, Oq = Dd / 0.4, Bq = (Pi - Dd) / 0.75;
			return 1.0 + Throw * (0.7 * std::exp(-Wq * Wq) + 0.45 * std::exp(-Oq * Oq) - 0.55 * std::exp(-Bq * Bq));
		};
		const auto IntoWings = [&]()
		{
			const double Pick = V.U();
			return Heading + (Pick < 0.4 ? 1.05 : Pick < 0.8 ? -1.05 : 0.0) + V.U(-0.32, 0.32);
		};
		if (bVary && Throw > 0.0)
		{
			for (std::pair<double, double>& Ray : RayOf)
			{
				if (V.U() < Throw)
				{
					Ray.first = IntoWings();
				}
				Ray.second *= 1.0 + 0.25 * Throw;
			}
		}
		// (the burst's soot: a few soft-sided rays of blackened soil out over the thrown soil, each its own way, width,
		// reach and darkness, the longest thrown on downrange by a flat-shooting gun's slanting burst. Six to ten narrow
		// spikes read as a drawn star, the art director's round 1, 23 November: four to nine, each a wedge wider than
		// the hole's sides, its edges soft)
		std::vector<std::array<double, 4>> Spikes;
		if (bVary && S.Scorch > 0.0)
		{
			const int Many = V.Whole(4, 9);
			for (int K = 0; K < Many; ++K)
			{
				const double Way = Throw > 0.0 && V.U() < 0.6 * Throw ? IntoWings() : V.U(0.0, 2.0 * Pi);
				const double Width = V.U(0.22, 0.6);
				const double Far = V.U(0.3, 0.75) + 0.35 * Throw * std::max(0.0, std::cos(Turn(Heading, Way)));
				Spikes.push_back({ Way, Width, Far, V.U(0.55, 1.0) });
			}
		}
		const auto Star = [&Spikes](double A, double T)
		{
			double Most = T < 0.2 ? 0.4 : 0.0; // (soot all round close in, the rays further)
			for (const std::array<double, 4>& Sp : Spikes)
			{
				const double Q = Turn(Sp[0], A) / Sp[1];
				Most = std::max(Most, Sp[3] * std::exp(-Q * Q) * std::clamp((Sp[2] - T) / 0.3 + 0.5, 0.0, 1.0));
			}
			return Most;
		};
		// (a deep hole's subsoil: lighter soil in patches round its crest, each side's share with its neighbours')
		std::vector<double> SubK;
		if (bVary && S.Subsoil > 0.0)
		{
			std::vector<double> Raw(static_cast<size_t>(N));
			for (double& Rw : Raw)
			{
				Rw = V.U() < 1.5 * S.Subsoil ? V.U(0.45, 0.9) : 0.0;
			}
			SubK.resize(static_cast<size_t>(N));
			for (int K = 0; K < N; ++K)
			{
				SubK[static_cast<size_t>(K)] = 0.5 * Raw[static_cast<size_t>(K)] + 0.25 * (Raw[static_cast<size_t>((K + 1) % N)] + Raw[static_cast<size_t>((K + N - 1) % N)]);
			}
		}
		// (the thrown soil's outer band broken from side to side, the art director's round 1: thick where a ray or a wing
		// threw it, elsewhere some sides thick and some thinning early into the ground, so its dark edge runs out in
		// tongues of uneven length and never along the rings' straight sides)
		std::vector<double> Thick6, Thick3;
		if (bVary)
		{
			Thick6.resize(static_cast<size_t>(N));
			Thick3.resize(static_cast<size_t>(N));
			for (int K = 0; K < N; ++K)
			{
				const double A = 2.0 * Pi * K / N;
				double RayW = Throw > 0.0 ? std::clamp((Wings(A) - 1.0) / 0.5, 0.0, 1.0) : 0.0;
				for (const std::pair<double, double>& Ray : RayOf)
				{
					const double T = Turn(Ray.first, A) / 0.22;
					RayW = std::max(RayW, std::exp(-T * T));
				}
				const double Pick = V.U();
				Thick6[static_cast<size_t>(K)] = std::clamp(RayW + (Pick < 0.45 ? V.U(0.45, 0.9) : V.U(0.0, 0.2)), 0.0, 1.0);
				Thick3[static_cast<size_t>(K)] = std::clamp(RayW + V.U(0.35, 1.0), 0.0, 1.0);
			}
		}
		std::vector<double> Crest(static_cast<size_t>(N)), Reach(static_cast<size_t>(N)), RimK(static_cast<size_t>(N)), Tone(static_cast<size_t>(N));
		for (int K = 0; K < N; ++K)
		{
			const double A = 2.0 * Pi * K / N;
			const size_t Kk = static_cast<size_t>(K);
			Crest[Kk] = 1.0 + Waves[0] * std::cos(2.0 * A + Phases[0]) + Waves[1] * std::cos(3.0 * A + Phases[1]) + Waves[2] * std::cos(5.0 * A + Phases[2]) + D.U(-0.05, 0.05);
			double Out_ = ReachOf * (0.88 + 0.12 * std::cos(A - Lean)) * (1.0 + LeanBy * std::cos(A - Lean));
			for (const std::pair<double, double>& Ray : RayOf)
			{
				const double T = Turn(Ray.first, A) / 0.22;
				Out_ += Ray.second * std::exp(-T * T);
			}
			Reach[Kk] = std::max(1.25, Out_);
			RimK[Kk] = D.U(0.65, 1.3) * (1.0 + 0.5 * LeanBy * std::cos(A - Lean));
			Tone[Kk] = D.U(0.86, 1.12);
			if (bVary)
			{
				// (stretched along the round's way; its soil thrown into the wings and forward; its crest higher
				// where the soil piled forward, lower behind)
				Crest[Kk] *= Stretch(A);
				Reach[Kk] = std::clamp(1.0 + (Out_ - 1.0) * Wings(A), 1.25, 1.45 * ReachOf); // (never flung past half as far again)
				RimK[Kk] *= 1.0 + 0.6 * Throw * std::cos(A - Heading);
			}
		}
		{
			// (the crest's height ragged, but never a spike: each side with a share of its neighbours')
			const std::vector<double> Was = RimK;
			for (int K = 0; K < N; ++K)
			{
				RimK[static_cast<size_t>(K)] = 0.5 * Was[static_cast<size_t>(K)] + 0.25 * (Was[static_cast<size_t>((K + 1) % N)] + Was[static_cast<size_t>((K + N - 1) % N)]);
			}
		}
		// (crater variety: the bowl's tone from side to side with its neighbours' and a third as strong. Each side's own
		// tone, the same from the middle out, fanned the bowl into pie slices on the sand, the art director's round 1)
		std::vector<double> BowlTone, BowlCrest;
		if (bVary)
		{
			BowlTone.resize(static_cast<size_t>(N));
			BowlCrest.resize(static_cast<size_t>(N));
			const auto Near = [N](const std::vector<double>& Of, int K)
			{
				return 0.5 * Of[static_cast<size_t>(K)] + 0.25 * (Of[static_cast<size_t>((K + 1) % N)] + Of[static_cast<size_t>((K + N - 1) % N)]);
			};
			for (int K = 0; K < N; ++K)
			{
				BowlTone[static_cast<size_t>(K)] = 1.0 + 0.3 * (Near(Tone, K) - 1.0);
				BowlCrest[static_cast<size_t>(K)] = Near(Crest, K);
			}
		}

		// The rings: inside the crest by their share of its radius (the floor, the wall, the crest), outside by their
		// share of the way out to the thrown soil's edge.
		struct RingOf
		{
			bool bOut;
			double T;
		};
		static constexpr RingOf Rings[] = { { false, 0.3 }, { false, 0.55 }, { false, 0.72 }, { false, 0.86 }, { false, 1.0 }, { true, 0.12 }, { true, 0.32 }, { true, 0.6 }, { true, 1.0 } };
		constexpr int NRings = static_cast<int>(sizeof(Rings) / sizeof(Rings[0]));
		static_assert(NRings == HoleRings, "Field::TopAt reads the rings as laid here");
		static_assert(Rings[CutRing].T > 0.85 && Rings[CutRing].T < 0.87 && !Rings[CutRing].bOut, "the cut runs along the wall's 0.86 ring");
		// (dug: the bowl under the ground, a round floor falling to Deep at its middle, its wall coming up through the
		// ground between the 0.72 and the 0.86 rings; Deep 0 lays the old shallow form over the ground)
		const double Deep = std::max(0.0, S.Deep);
		const double Fresh = 1.0 - std::clamp(S.Weathered, 0.0, 1.0); // (an old hole's blast soot washed out)
		const auto BowlAt = [Deep](double T) { const double K = T / 0.78; return -Deep * std::max(0.0, 1.0 - K * K); };
		const double Base = 0.03 + std::max(0.0, Lift); // (the floor over the drawn ground: nothing of it may sink under the ground's own triangles)
		const double Ground0 = Height(X, Y);
		const uint32_t Middle = Deep > 0.0
			? Put(Out, X, Y, Ground0 + Base - Deep * D.U(0.96, 1.04), Mixed(Toned(Paint.Hollow, D.U(0.82, 0.92)), Soot, D.U(0.3, 0.45) * Fresh))
			: Put(Out, X, Y, Ground0 + Base, Toned(Paint.Hollow, D.U(0.82, 0.92)));
		std::vector<uint32_t> Index(static_cast<size_t>(NRings * N));
		std::vector<double> RingR(bVary ? static_cast<size_t>(N) : 0u, 0.0); // (crater variety: each side's last ring's radius)
		std::vector<char> Stopped(bVary ? static_cast<size_t>(N) : 0u, 0); // (and whether its thrown soil stopped at an older bowl)
		// (crater variety: a flooded hole's water level, flat across the bowl, and its wet ring just over it)
		const double Pud = bVary && Deep > 0.0 ? std::clamp(S.Puddle, 0.0, 0.6) : 0.0;
		const double WaterZ = Ground0 + Base - Deep * (1.0 - Pud);
		for (int Ring = 0; Ring < NRings; ++Ring)
		{
			const RingOf& Rg = Rings[Ring];
			for (int K = 0; K < N; ++K)
			{
				const size_t Kk = static_cast<size_t>(K);
				const bool bRim = Rg.bOut && Rg.T >= 1.0; // (the thrown soil's thin edge over the grass: the most ragged)
				// (crater variety: the bowl's points a quarter as jittered, round a crest smoothed from side to side, so its
				// faces lie smooth and no spokes run in to its middle: the art director's round 1, the sand's umbrella)
				const bool bSmooth = bVary && !Rg.bOut && Deep > 0.0 && Rg.T < 0.8;
				const auto Jitter = [bSmooth](double J) { return bSmooth ? 1.0 + 0.25 * (J - 1.0) : J; };
				const double Turned = bRim ? D.U(-0.35, 0.35) : D.U(-0.12, 0.12);
				const double A = 2.0 * Pi * (K + (bSmooth ? 0.25 * Turned : Turned)) / N;
				const double Rc = R * (bSmooth ? BowlCrest[Kk] : Crest[Kk]);
				double Radius = 0.0, Over = 0.0;
				Rgb8 Colour, Unturfed;
				double TurfShare = -1.0;
				if (!Rg.bOut)
				{
					Radius = Rc * Rg.T * Jitter(D.U(0.96, 1.04));
					if (Deep > 0.0 && Rg.T < 0.8)
					{
						// the bowl: its floor sooty at the middle where the burst was, damp dark earth, the wall's foot
						// lightening toward the dry soil it rises to
						const double Bowl = BowlAt(Rg.T) * Jitter(D.U(0.92, 1.06));
						Over = Bowl + D.U(-0.02, 0.02) * Deep * (bSmooth ? 0.25 : 1.0);
						Colour = Rg.T < 0.4 ? Mixed(Paint.Hollow, Soot, (bSmooth ? 0.225 + 0.25 * (D.U(0.15, 0.3) - 0.225) : D.U(0.15, 0.3)) * Fresh) : Rg.T < 0.6 ? Mixed(Paint.Hollow, Paint.Wall, 0.18) : Mixed(Paint.Hollow, Paint.Wall, 0.55);
					}
					else if (Rg.T <= 0.56)
					{
						// the hollow's floor: damp and dark, a few small humps of fallen soil
						Over = D.U(0.0, 0.025) * std::min(1.0, R);
						Colour = Mixed(Paint.Hollow, Paint.Wall, Rg.T > 0.5 ? 0.35 : 0.08);
					}
					else
					{
						// the wall, rising to the crest, dark at its foot to the dry soil at the top
						const double W = (Rg.T - 0.5) / 0.5;
						Over = RimHigh * RimK[Kk] * std::pow(Smooth(W), 1.25);
						Colour = Rg.T >= 1.0 ? Paint.Crest : Mixed(Paint.Wall, Paint.Crest, Smooth(W) * 0.8);
					}
					if (Paint.Bared > 0.0 && Rg.T >= 0.5 && Rg.T < 1.0 && D.U() < Paint.Bared)
					{
						// (an old hole's sides: here and there the bare soil still showing through the grass)
						Colour = Mixed(Paint.Bare, Colour, D.U(0.0, 0.3));
					}
				}
				else
				{
					// the thrown soil, falling away from the crest to a thin ragged edge over the grass
					Radius = Rc * (1.0 + Rg.T * (Reach[Kk] - 1.0)) * (bRim ? D.U(0.78, 1.12) : D.U(0.96, 1.04));
					if (bRim)
					{
						// (feathered into the grass: the soil thinning out over a metre and a half past where it lay
						// thick, its edge broken, so the eye finds no line where the hole's earth ends. Ended at the
						// thrown soil's reach it drew a hard polygon round every hole: the art director, 23 November)
						// (each draw in its own statement: the same hole on every compiler)
						const double Far = D.U(0.6, 1.4);
						const double Short = D.U() < 0.25 ? D.U(0.35, 0.7) : 1.0;
						Radius += Feather * Far * Short;
					}
					// (falling away fast from the crest: a narrow lip, the thrown soil beyond it a thin layer on the
					// ground, not the flank of a mound)
					Over = bRim ? 0.0 : RimHigh * RimK[Kk] * std::pow(1.0 - Rg.T, 3.0) * D.U(0.85, 1.15);
					Colour = Rg.T < 0.2 ? Mixed(Paint.Crest, Paint.Thrown, 0.45) : Rg.T < 0.4 ? Paint.Thrown : Rg.T < 0.7 ? Mixed(Paint.Thrown, Paint.Edge, 0.55) : Paint.Edge;
					Unturfed = Colour;
					// (torn turf and darker lumps of soil lying in it, so it is never one flat colour)
					const double Patch = D.U();
					if (Patch < 0.22)
					{
						TurfShare = D.U(0.35, 0.7);
						Colour = Mixed(Colour, Paint.Turf, TurfShare);
					}
					else if (Patch < 0.42) Colour = Toned(Mixed(Colour, Paint.Clod, 0.6), D.U(0.8, 0.95));
					else if (Deep > 0.0 && Rg.T < 0.4 && Patch < 0.62) Colour = Mixed(Colour, Soot, D.U(0.15, 0.35) * Fresh); // (blast-blackened soil thrown with it)
				}
				double Px = X + std::cos(A) * Radius, Py = Y + std::sin(A) * Radius;
				if (bVary && Avoid && Rg.bOut)
				{
					// (crater variety: its thrown soil stops at an older hole's rim. Laid on the older bowl's floor, or
					// reaching over it to the far side, its triangles drew a sheet across the bowl. A side stopped at the
					// older bowl's edge goes on in steps of a few centimetres, falling into the bowl as its wall does)
					const double Inner = RingR[Kk], Least = Inner + 0.03;
					const auto Crosses = [&](double Rr)
					{
						for (const double T : { 0.25, 0.5, 0.75, 1.0 })
						{
							const double Rt = Inner + T * (Rr - Inner);
							if (Avoid(X + std::cos(A) * Rt, Y + std::sin(A) * Rt))
							{
								return true;
							}
						}
						return false;
					};
					if (Stopped[Kk])
					{
						Radius = Least;
					}
					for (int Try = 0; Try < 12 && Radius > Least && Crosses(Radius); ++Try)
					{
						Radius = std::max(Least, Radius - std::max(0.08, 0.12 * (Radius - Inner)));
					}
					if (Radius > Least && Crosses(Radius))
					{
						Radius = Least;
					}
					Stopped[Kk] = Stopped[Kk] || (Radius <= Least && Crosses(Radius));
					Px = X + std::cos(A) * Radius;
					Py = Y + std::sin(A) * Radius;
				}
				if (bVary)
				{
					RingR[Kk] = Radius;
				}
				// (the thin edge lies on the grass: 2 mm over what is there, no step. At 0.6 of Base, 1.8 cm, its lip cast a thin
				// dark line on the grass round every hole in the Mac's stills, 23 November, a seam where none should be)
				const bool bLip = Rg.bOut && Rg.T >= 1.0;
				if (Rg.bOut && Under && TurfShare >= 0.0)
				{
					// (turf torn from this ground: its own colour, a little darker for the soil on it)
					Colour = Mixed(Unturfed, Mixed(Paint.Turf, Under(Px, Py), 0.7), TurfShare);
				}
				Rgb8 Painted = bSmooth ? Toned(Colour, 1.0 + (BowlTone[Kk] * Jitter(D.U(0.92, 1.08)) - 1.0) * Paint.Mottle)
					: Toned(Colour, 1.0 + (Tone[Kk] * D.U(0.92, 1.08) - 1.0) * Paint.Mottle);
				if (Rg.bOut && Under)
				{
					// The thrown soil fades into the ground it lies on: its thin edge is the ground's own colour (no
					// outline, whatever was torn), the ring inside it leans that way.
					const Rgb8 Ground = Under(Px, Py);
					if (bVary && !bRim)
					{
						// (crater variety: each side's own share, so the dark soil runs out in tongues of uneven length)
						const double Fade = Rg.T >= 0.6 ? 1.0 - 0.75 * Thick6[Kk] : Rg.T >= 0.3 ? 0.55 * (1.0 - Thick3[Kk]) : 0.0;
						Painted = Mixed(Painted, Ground, Fade);
					}
					else
					{
						Painted = bRim ? Ground : Rg.T >= 0.6 ? Mixed(Painted, Ground, 0.4) : Painted;
					}
				}
				if (bVary)
				{
					// (crater variety: the subsoil in patches on its crest and the soil thrown nearest it; the burst's
					// soot star over the thrown soil, onto its thin edge only where the ground shows it: snow, sand)
					const double SubW = !Rg.bOut ? (Rg.T >= 1.0 ? 1.0 : Rg.T >= 0.86 ? 0.55 : 0.0) : (Rg.T < 0.2 ? 0.8 : Rg.T < 0.4 ? 0.45 : 0.0);
					if (!SubK.empty() && SubW > 0.0)
					{
						Painted = Mixed(Painted, Toned(Paint.Sub, Tone[Kk]), SubK[Kk] * SubW);
					}
					if (Rg.bOut && !bRim && Rg.T >= 0.6)
					{
						// (the wings' soil lying thicker: less of the ground through it)
						Painted = Mixed(Painted, Toned(Paint.Thrown, Tone[Kk]), std::clamp(0.6 * (Wings(A) - 1.0), 0.0, 0.4));
					}
					// (never on the thin edge: its colour is the ground's, or the edge shows. Each point a little more or less,
					// so a ray's sides are never ruled)
					const double SootW = Rg.bOut ? (bRim ? 0.0 : 1.0) : (Rg.T >= 0.86 ? 0.45 : 0.0);
					if (!Spikes.empty() && SootW > 0.0)
					{
						Painted = Mixed(Painted, Soot, 0.75 * S.Scorch * Fresh * SootW * Star(A, Rg.bOut ? Rg.T : 0.0) * V.U(0.75, 1.0));
					}
				}
				// (the thrown soil on the ground as drawn: on HeightAt's smooth height its thin edge sank under the drawn
				// triangles at a quarter of its points on a slope's bend, pre-check's probe, 23 November)
				const double Gz = Rg.bOut && Thin ? Thin(Px, Py) : Height(Px, Py);
				const double Pz = Gz + (bLip ? (S.Feather > 0.0 ? 0.002 : 0.6 * Base) : Base + Over);
				if (Pud > 0.0 && !Rg.bOut && Pz > WaterZ - 0.03 && Pz < WaterZ + 0.06 + 0.06 * Deep)
				{
					// (the wet ring over the water: the soil soaked a hand up the bowl's side, a darkening of it, never
					// black: the art director's carry, 23 November)
					Painted = Toned(Painted, 0.82);
				}
				Index[static_cast<size_t>(Ring * N + K)] = Put(Out, Px, Py, Pz, Painted);
			}
		}
		if (OutCut && Deep > 0.0)
		{
			// The cut: the hull of the wall's 0.86 ring (Andrew's monotone chain), counter-clockwise. Its points stand
			// over the ground; where the hull bridges a dent in the ring it runs a little further out, under the wall
			// still higher there, so the ground's cut edge is never seen.
			std::vector<std::pair<double, double>> Pts;
			for (int K = 0; K < N; ++K)
			{
				const uint32_t I = Index[static_cast<size_t>(CutRing * N + K)];
				Pts.emplace_back(Out.Positions[static_cast<size_t>(I) * 3], Out.Positions[static_cast<size_t>(I) * 3 + 1]);
			}
			std::sort(Pts.begin(), Pts.end());
			std::vector<std::pair<double, double>> Hull(2 * Pts.size());
			size_t Hn = 0;
			const auto Cross = [](const std::pair<double, double>& O, const std::pair<double, double>& A, const std::pair<double, double>& B)
			{
				return (A.first - O.first) * (B.second - O.second) - (A.second - O.second) * (B.first - O.first);
			};
			for (size_t I = 0; I < Pts.size(); ++I)
			{
				while (Hn >= 2 && Cross(Hull[Hn - 2], Hull[Hn - 1], Pts[I]) <= 0.0) --Hn;
				Hull[Hn++] = Pts[I];
			}
			for (size_t I = Pts.size() - 1, Lower = Hn + 1; I-- > 0;)
			{
				while (Hn >= Lower && Cross(Hull[Hn - 2], Hull[Hn - 1], Pts[I]) <= 0.0) --Hn;
				Hull[Hn++] = Pts[I];
			}
			if (Hn > 3)
			{
				for (size_t I = 0; I + 1 < Hn; ++I) // (the last is the first again)
				{
					OutCut->push_back(Hull[I].first);
					OutCut->push_back(Hull[I].second);
				}
				// (every point of the bowl inside the cut: a wall's foot thrown a little wide on a side where the crest
				// dips is drawn in toward the middle, keeping its depth under the ground there)
				for (int Ring = 0; Ring < CutRing; ++Ring)
				{
					for (int K = 0; K < N; ++K)
					{
						const size_t I = static_cast<size_t>(Index[static_cast<size_t>(Ring * N + K)]) * 3;
						float* P = &Out.Positions[I];
						for (int Try = 0; Try < 8 && !Inside(*OutCut, P[0], P[1]); ++Try)
						{
							const double Below = P[2] - Height(P[0], P[1]);
							P[0] = static_cast<float>(X + (P[0] - X) * 0.92);
							P[1] = static_cast<float>(Y + (P[1] - Y) * 0.92);
							P[2] = static_cast<float>(Height(P[0], P[1]) + Below);
						}
					}
				}
			}
		}
		for (int K = 0; K < N; ++K)
		{
			const int K1 = (K + 1) % N;
			Tri(Out, Middle, Index[static_cast<size_t>(K)], Index[static_cast<size_t>(K1)]);
			for (int Ring = 0; Ring + 1 < NRings; ++Ring)
			{
				const uint32_t A = Index[static_cast<size_t>(Ring * N + K)], B = Index[static_cast<size_t>(Ring * N + K1)];
				const uint32_t C = Index[static_cast<size_t>((Ring + 1) * N + K)], E = Index[static_cast<size_t>((Ring + 1) * N + K1)];
				Tri(Out, A, C, E);
				Tri(Out, A, E, B);
			}
		}

		// The clods and torn turf: thrown out over the rim, more along the rays, the big ones nearer.
		for (int I = 0; I < std::max(0, S.Clods); ++I)
		{
			double A = D.U(0.0, 2.0 * Pi);
			if (Rays > 0 && D.U() < 0.5)
			{
				const double Ray = RayOf[static_cast<size_t>(D.Whole(0, Rays - 1))].first;
				A = Ray + D.U(-0.2, 0.2);
			}
			if (bVary && Throw > 0.0 && V.U() < 0.6 * Throw)
			{
				A = IntoWings(); // (crater variety: the clods thrown into the wings and forward)
			}
			const double Far0 = R * (1.08 + std::pow(D.U(), 1.4) * (ReachOf - 1.0) * 1.35);
			// (never in toward the bowl behind it, where the wings threw less: a clod lies out past the crest)
			const double Far = bVary ? Stretch(A) * std::max(1.2 * R, Far0 * (1.0 + 0.6 * (Wings(A) - 1.0))) : Far0;
			const double Cx = X + std::cos(A) * Far, Cy = Y + std::sin(A) * Far;
			const double Big = S.Clod * D.U(0.35, 1.0) * (Far < 1.5 * R ? 1.0 : 0.75);
			const bool bTurf = D.U() < 0.3;
			const double Gz = Height(Cx, Cy);
			const Rgb8 Side = Toned(Paint.Clod, D.U(0.85, 1.1));
			const Rgb8 Turf = Under ? Mixed(Paint.Turf, Under(Cx, Cy), 0.7) : Paint.Turf;
			const Rgb8 Top = bTurf ? Toned(Turf, D.U(0.85, 1.1)) : Toned(Mixed(Paint.Clod, Paint.Crest, 0.4), D.U(0.9, 1.1));
			const double PeakX = Cx + D.U(-0.2, 0.2) * Big;
			const double PeakY = Cy + D.U(-0.2, 0.2) * Big;
			const double PeakZ = Gz + Base + Big * (bTurf ? D.U(0.22, 0.36) : D.U(0.32, 0.55));
			const uint32_t Peak = Put(Out, PeakX, PeakY, PeakZ, Top); // (a low lump, never a cone)
			constexpr int Around = 5;
			uint32_t Foot[Around];
			const double Turned = D.U(0.0, 2.0 * Pi);
			for (int K = 0; K < Around; ++K)
			{
				const double B = Turned + 2.0 * Pi * K / Around + D.U(-0.25, 0.25);
				const double Rr = Big * D.U(0.6, 1.1) * (bTurf ? 1.3 : 1.0);
				const double Fx = Cx + std::cos(B) * Rr, Fy = Cy + std::sin(B) * Rr;
				// (each foot on the ground under it: a clod lying across an older hole's edge sinks into neither side)
				Foot[K] = Put(Out, Fx, Fy, Height(Fx, Fy) + Base * 0.5 - 0.01, Side);
			}
			for (int K = 0; K < Around; ++K)
			{
				Tri(Out, Peak, Foot[K], Foot[(K + 1) % Around]);
			}
		}
		// Crater variety, the art director's round 1: the spray, specks of soil and torn turf thrown out over the thin
		// edge and past it, thinning as they go, most along the rays and into the wings; on snow and sand some of them
		// blackened by the burst. Laid as the clods are (a peak and five round its foot), so TopAt reads them as clods.
		for (int I = 0; bVary && I < std::max(0, S.Spray); ++I)
		{
			const double Pick = V.U();
			double A = V.U(0.0, 2.0 * Pi);
			if (Rays > 0 && Pick < 0.45)
			{
				A = RayOf[static_cast<size_t>(V.Whole(0, Rays - 1))].first + V.U(-0.3, 0.3);
			}
			else if (Pick < 0.45 + 0.5 * Throw)
			{
				A = IntoWings();
			}
			const double Aw = std::fmod(std::fmod(A, 2.0 * Pi) + 2.0 * Pi, 2.0 * Pi);
			const size_t Kk = static_cast<size_t>(std::lround(Aw / (2.0 * Pi) * N)) % static_cast<size_t>(N);
			const double Re = Reach[Kk];
			const double Far = R * Stretch(A) * Crest[Kk] * (1.0 + (Re - 1.0) * V.U(0.55, 0.85)) + std::pow(V.U(), 1.25) * (0.6 * (Re - 1.0) * R + 1.2 * Feather);
			const double Cx = X + std::cos(A) * Far, Cy = Y + std::sin(A) * Far;
			const double Big = std::max(0.025, S.Clod * V.U(0.12, 0.32));
			const bool bTurf = V.U() < 0.25;
			const Rgb8 Side = Toned(Paint.Clod, V.U(0.85, 1.0));
			Rgb8 Top = bTurf ? Toned(Under ? Mixed(Paint.Turf, Under(Cx, Cy), 0.7) : Paint.Turf, V.U(0.8, 1.0)) : Toned(Mixed(Paint.Clod, Paint.Thrown, 0.5), V.U(0.8, 1.05));
			const double Sooty = V.U(0.3, 0.8);
			if (!bTurf && Paint.EdgeSoot > 0.0)
			{
				Top = Mixed(Top, Soot, Paint.EdgeSoot * S.Scorch * Fresh * Sooty);
			}
			const double Gz = Height(Cx, Cy);
			const double PeakX = Cx + V.U(-0.2, 0.2) * Big;
			const double PeakY = Cy + V.U(-0.2, 0.2) * Big;
			const uint32_t Peak = Put(Out, PeakX, PeakY, Gz + Base + Big * V.U(0.2, 0.35), Top); // (a speck lying flat)
			constexpr int Around = ClodPoints - 1;
			uint32_t Foot[Around];
			const double Turned = V.U(0.0, 2.0 * Pi);
			for (int K = 0; K < Around; ++K)
			{
				const double B = Turned + 2.0 * Pi * K / Around + V.U(-0.25, 0.25);
				const double Rr = Big * V.U(0.6, 1.1) * (bTurf ? 1.3 : 1.0);
				const double Fx = Cx + std::cos(B) * Rr, Fy = Cy + std::sin(B) * Rr;
				Foot[K] = Put(Out, Fx, Fy, Height(Fx, Fy) + Base * 0.5 - 0.01, Side);
			}
			for (int K = 0; K < Around; ++K)
			{
				Tri(Out, Peak, Foot[K], Foot[(K + 1) % Around]);
			}
		}
		// Crater variety: a flooded hole's water, a flat disc at its level reaching just under the bowl's sides all round
		// (each side's edge where the bowl stands a few centimetres over it, so the bowl hides it). Not read by TopAt.
		if (Pud > 0.0)
		{
			const uint32_t Pool = Put(Out, X, Y, WaterZ, PuddleWater);
			std::vector<uint32_t> Shore(static_cast<size_t>(N));
			for (int K = 0; K < N; ++K)
			{
				const double A = 2.0 * Pi * K / N;
				const double Rc = R * Crest[static_cast<size_t>(K)];
				double T = 0.05;
				for (; T < 0.72; T += 0.01)
				{
					const double Bx = X + std::cos(A) * Rc * T, By = Y + std::sin(A) * Rc * T;
					if (Height(Bx, By) + Base + BowlAt(T) >= WaterZ + 0.03 + 0.05 * Deep)
					{
						break;
					}
				}
				Shore[static_cast<size_t>(K)] = Put(Out, X + std::cos(A) * Rc * T, Y + std::sin(A) * Rc * T, WaterZ, PuddleShore);
			}
			for (int K = 0; K < N; ++K)
			{
				Tri(Out, Pool, Shore[static_cast<size_t>(K)], Shore[static_cast<size_t>((K + 1) % N)]);
			}
		}
		Normals(Out, FirstPoint, FirstIndex);
		// The thrown soil lit as the ground it lies on: its thin edge wholly by the ground's own normal (lit by its own
		// slope down from the lip, a low sun drew the edge as a ruled polygon, brighter on one side and darker on the
		// other: the art director's cut 4, shot 07), the rings inside it more and more by their own, the lip by its own.
		for (int Ring = 0; Ring < NRings; ++Ring)
		{
			const RingOf& Rg = Rings[Ring];
			if (!Rg.bOut)
			{
				continue;
			}
			const double W = Rg.T >= 1.0 ? 1.0 : Rg.T >= 0.6 ? 0.9 : Rg.T >= 0.32 ? 0.55 : 0.15;
			for (int K = 0; K < N; ++K)
			{
				const size_t I = static_cast<size_t>(Index[static_cast<size_t>(Ring * N + K)]);
				LeanTo(Out, I, GroundUp(Lit, Height, Out.Positions[I * 3], Out.Positions[I * 3 + 1]), W);
			}
		}
	}

	/**
	 * A breach's torn earth (Field::AddBreach): a grid in the bank's frame, u along it and v to its left, 0.3 m a
	 * square, laid row by row from the far right corner (Field::OnBreach reads it so). Over the gap the bank's earth is
	 * gone down to the field (Height less Rise), the floor churned; the bank's cut ends are faces of fresh earth falling
	 * into the gap over a metre or so, ragged from side to side, a lip of torn turf at their tops; on the side she drove
	 * out to the spoil lies heaped half a metre or less, turf and clods in it, a little spilled on the other side. The
	 * ground drawn is cut over the bank's footprint across the gap and its faces (OutCut, a rectangle, counter-clockwise);
	 * the grid runs half a metre past the cut on every side as a skin a few centimetres over the ground, its outer edge
	 * the ground's own colour and lit as the ground, so no seam shows where it meets the bank's own mesh.
	 */
	static void LayBreach(const Breach& B, const Soil& Paint, uint64_t Seed, const HeightFn& Height, const RiseFn& Rise, LandField::Mesh& Out,
		const ColourFn& Under, const UpFn& Lit, std::vector<double>& OutCut, Field::Hole& H)
	{
		OutCut.clear();
		Dice D(Seed);
		const double Step = 0.3;
		const double Half = std::max(1.2, B.Half);
		const double Ramp = 1.0;
		const double CutU = Half + Ramp, MU = 0.45;
		const double Foot = std::clamp(B.Foot, 0.8, 2.5), CutV = Foot + 0.25;
		const double SpoilOut = CutV + 2.6, BackOut = CutV + 0.9, MV = 0.5;
		const int Through = B.Through < 0 ? -1 : 1;
		// (u from the far left end to the far right; v across, the spoil's side out to SpoilOut, the other to BackOut)
		const double U0 = -(CutU + MU), U1 = CutU + MU;
		const double VLo = Through > 0 ? -(BackOut + MV) : -(SpoilOut + MV), VHi = Through > 0 ? SpoilOut + MV : BackOut + MV;
		// (a span a whole number of steps long is that many squares, not one more of no width)
		const int NU = static_cast<int>(std::ceil((U1 - U0) / Step - 1e-6)) + 1, NV = static_cast<int>(std::ceil((VHi - VLo) / Step - 1e-6)) + 1;
		const double Cy = std::cos(B.Yaw), Sy = std::sin(B.Yaw);
		const auto World = [&](double U, double V, double& X, double& Y) { X = B.X + U * Cy - V * Sy; Y = B.Y + U * Sy + V * Cy; };

		// Its shape's dice: the faces' ragged line, the churned floor, the spoil's lumps, its height.
		const double JagP[4] = { D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi) };
		const double ChurnP[4] = { D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi) };
		const double LumpP[3] = { D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi) };
		const double SpoilHigh = D.U(0.3, 0.48), SpoilAt = D.U(0.7, 1.1), SpoilLean = D.U(-0.4, 0.4);
		// (where a face's foot meets the floor, by side and across the bank: ragged by a third of a metre either way)
		const auto FaceAt = [&](int Side, double Vt)
		{
			const double* P = Side < 0 ? JagP : JagP + 2;
			return Half + 0.22 * std::sin(Vt * 2.3 + P[0]) + 0.12 * std::sin(Vt * 5.1 + P[1]);
		};
		const auto Churn = [&](double U, double V)
		{
			return 0.035 * std::sin(U * 5.3 + ChurnP[0]) * std::sin(V * 4.1 + ChurnP[1]) + 0.022 * std::sin(U * 9.7 + V * 3.3 + ChurnP[2]) + 0.015 * std::sin(V * 11.0 - U * 2.0 + ChurnP[3]);
		};
		const auto Lumps = [&](double U, double V)
		{
			return 0.5 + 0.25 * std::sin(U * 3.1 + LumpP[0]) * std::sin(V * 2.7 + LumpP[1]) + 0.25 * std::sin(U * 6.3 - V * 4.9 + LumpP[2]);
		};

		const size_t FirstPoint = Out.Colours.size(), FirstIndex = Out.Triangles.size();
		H.U0 = U0;
		H.V0 = VLo;
		H.Step = Step;
		H.NU = NU;
		H.NV = NV;
		H.CutU = CutU;
		H.CutV = CutV;
		H.SpoilOut = SpoilOut;
		H.BackOut = BackOut;
		std::vector<double> Fade(static_cast<size_t>(NU * NV), 0.0); // (how much of each point's normal is the ground's)
		for (int J = 0; J < NV; ++J)
		{
			const double V = std::min(VHi, VLo + J * Step), Vt = V * Through;
			for (int I = 0; I < NU; ++I)
			{
				const double U = std::min(U1, U0 + I * Step), Au = std::fabs(U);
				double X = 0.0, Y = 0.0;
				World(U, V, X, Y);
				const double Gz = Height(X, Y), Rz = std::max(0.0, Rise(X, Y)), Floor = Gz - Rz;
				const Rgb8 Ground = Under ? Under(X, Y) : Paint.Edge;
				const bool bEdge = I == 0 || J == 0 || I == NU - 1 || J == NV - 1;
				const bool bInCut = Au <= CutU && Vt >= -CutV && Vt <= CutV;
				double Z = 0.0;
				Rgb8 C;
				double W = 0.0;
				if (bEdge)
				{
					// the skin's outer edge: on the ground, its own colour, lit as it is
					Z = Gz + 0.012;
					C = Ground;
					W = 1.0;
				}
				else if (bInCut)
				{
					const double FaceFoot = FaceAt(U < 0.0 ? -1 : 1, Vt);
					// (the share of the bank left standing: none over the gap, all of it a little short of the cut's edge)
					const double Kept = Smooth(std::clamp((Au - FaceFoot) / std::max(0.2, CutU - 0.15 - FaceFoot), 0.0, 1.0));
					// (raised a little toward the cut's edges, so the bank's own mesh beyond never shows over the skin)
					const double Edgeness = std::max(Smooth(std::clamp((Au - (CutU - 0.35)) / 0.35, 0.0, 1.0)), Smooth(std::clamp((std::fabs(Vt) - (CutV - 0.35)) / 0.35, 0.0, 1.0)));
					const double Lip = Rz > 0.2 ? 0.09 * std::exp(-std::pow((Au - (FaceFoot + 0.6 * (CutU - FaceFoot))) / 0.22, 2.0)) : 0.0;
					Z = Floor + Rz * Kept + 0.03 + 0.07 * Edgeness + Churn(U, V) * (1.0 - Kept) + Lip;
					const double Tone = D.U(0.88, 1.1);
					if (Kept < 0.12)
					{
						// the gap's floor: the bank's earth churned by her tracks, torn turf and darker clods lying in it
						const double Patch = D.U();
						C = Mixed(Paint.Hollow, Paint.Wall, D.U(0.3, 0.6));
						if (Patch < 0.14) C = Mixed(C, Mixed(Paint.Turf, Ground, 0.6), D.U(0.4, 0.7));
						else if (Patch < 0.26) C = Mixed(C, Paint.Clod, 0.6);
					}
					else if (Kept < 0.86)
					{
						// a cut end's face: fresh earth, damp and dark at its foot, drier toward the torn lip
						C = Mixed(Paint.Wall, Paint.Crest, Smooth((Kept - 0.12) / 0.74) * 0.7);
						if (D.U() < 0.12) C = Mixed(C, Paint.Clod, 0.5); // (roots and stones in it)
					}
					else
					{
						// the bank's top and sides left: its own colour, torn turf at the lip
						C = Mixed(Ground, Mixed(Paint.Turf, Paint.Crest, 0.3), (1.0 - Edgeness) * Smooth((1.0 - Kept) / 0.14) * 0.6);
					}
					C = Toned(C, Tone);
				}
				else
				{
					// outside the cut: the skin over the ground and the bank's ends, the spoil on her side, a spill on the other
					const double Du = std::max(0.0, Au - CutU), Dv = Vt > CutV ? Vt - CutV : Vt < -CutV ? -CutV - Vt : 0.0;
					const double Far = std::clamp(std::max(Du / MU, Vt > CutV ? (Vt - (SpoilOut - 0.2)) / MV : Vt < -CutV ? (Dv - (BackOut - CutV - 0.2)) / MV : 0.0), 0.0, 1.0);
					Z = Gz + 0.015 + 0.085 * (1.0 - Smooth(std::min(1.0, std::max(Du / MU, Dv / 0.6))));
					C = Ground;
					W = Smooth(Far);
					if (Vt > CutV && Au < Half + 1.6)
					{
						// the spoil: the bank's earth pushed out, heaped highest a metre out, falling away to nothing
						const double Out_ = Vt - CutV, Room = SpoilOut - CutV;
						const double Across = Out_ < SpoilAt ? Smooth(Out_ / SpoilAt) : 1.0 - Smooth((Out_ - SpoilAt) / std::max(0.3, Room - SpoilAt));
						const double Along = 1.0 - Smooth(std::clamp((Au - (Half - 0.4 + SpoilLean * (U > 0.0 ? 1.0 : -1.0))) / 1.6, 0.0, 1.0));
						const double Heap = SpoilHigh * Across * Along * (0.65 + 0.7 * Lumps(U, V));
						Z += Heap;
						if (Heap > 0.02)
						{
							const double Patch = D.U();
							C = Heap > 0.18 ? Mixed(Paint.Thrown, Paint.Crest, 0.35) : Mixed(Paint.Thrown, Ground, 0.35);
							if (Patch < 0.24) C = Mixed(C, Mixed(Paint.Turf, Ground, 0.65), D.U(0.45, 0.8)); // (turf torn off the bank's top)
							else if (Patch < 0.38) C = Mixed(C, Paint.Clod, 0.6);
							C = Toned(C, D.U(0.88, 1.1));
							W = std::min(W, 0.15);
						}
						else
						{
							C = Mixed(Ground, Paint.Edge, 0.3 * (1.0 - Far));
						}
					}
					else if (Vt < -CutV && Au < Half + 0.8)
					{
						// a little spilled back on the side she came from
						const double Out_ = -CutV - Vt, Room = BackOut - CutV;
						const double Spill = 0.06 * (1.0 - Smooth(Out_ / Room)) * (1.0 - Smooth(std::clamp((Au - Half + 0.3) / 1.1, 0.0, 1.0)));
						Z += Spill * (0.6 + 0.8 * Lumps(U, V));
						C = Mixed(Ground, Paint.Thrown, std::min(1.0, Spill / 0.04) * 0.7);
					}
					else
					{
						// over the bank beyond the cut: its own colour, a little torn soil near the cut's edge
						C = Mixed(Ground, Paint.Crest, 0.25 * (1.0 - Smooth(std::min(1.0, Du / MU))) * (Du > 0.0 ? 1.0 : 0.0));
					}
					C = Mixed(C, Ground, Far);
				}
				Put(Out, X, Y, Z, C);
				Fade[static_cast<size_t>(J * NU + I)] = W;
			}
		}
		for (int J = 0; J + 1 < NV; ++J)
		{
			for (int I = 0; I + 1 < NU; ++I)
			{
				const uint32_t A = static_cast<uint32_t>(FirstPoint) + static_cast<uint32_t>(J * NU + I), Bq = A + 1u, Cq = A + static_cast<uint32_t>(NU), Dq = Cq + 1u;
				Tri(Out, A, Bq, Dq);
				Tri(Out, A, Dq, Cq);
			}
		}

		// The clods: lumps of the bank's earth and turf on the floor and the spoil, the bigger ones in the spoil.
		const int Clods = D.Whole(12, 18);
		H.Clods = Clods;
		for (int K = 0; K < Clods; ++K)
		{
			const bool bSpoil = D.U() < 0.6;
			const double U = D.U(-Half - 0.4, Half + 0.4), Vt = bSpoil ? D.U(CutV - 0.2, SpoilOut - 0.6) : D.U(-CutV + 0.3, CutV - 0.3);
			double Cx = 0.0, Cyy = 0.0;
			World(U, Vt * Through, Cx, Cyy);
			const double Big = (bSpoil ? D.U(0.12, 0.28) : D.U(0.08, 0.18));
			const bool bTurf = D.U() < 0.35;
			double Gz = 0.0;
			{
				// (on the breach's own face there: the nearest grid point's height)
				const int I = std::clamp(static_cast<int>(std::lround((U - U0) / Step)), 0, NU - 1);
				const int J = std::clamp(static_cast<int>(std::lround((Vt * Through - VLo) / Step)), 0, NV - 1);
				Gz = Out.Positions[(FirstPoint + static_cast<size_t>(J * NU + I)) * 3 + 2];
			}
			const Rgb8 Ground = Under ? Under(Cx, Cyy) : Paint.Turf;
			const Rgb8 Side = Toned(Paint.Clod, D.U(0.85, 1.1));
			const Rgb8 Top = bTurf ? Toned(Mixed(Paint.Turf, Ground, 0.7), D.U(0.85, 1.1)) : Toned(Mixed(Paint.Clod, Paint.Crest, 0.4), D.U(0.9, 1.1));
			const uint32_t Peak = Put(Out, Cx, Cyy, Gz + Big * (bTurf ? D.U(0.25, 0.4) : D.U(0.35, 0.6)), Top);
			constexpr int Around = ClodPoints - 1; // (TopAt reads them so)
			uint32_t FootAt[Around];
			const double Turned = D.U(0.0, 2.0 * Pi);
			for (int F = 0; F < Around; ++F)
			{
				const double A = Turned + 2.0 * Pi * F / Around + D.U(-0.25, 0.25);
				const double Rr = Big * D.U(0.6, 1.1) * (bTurf ? 1.3 : 1.0);
				FootAt[F] = Put(Out, Cx + std::cos(A) * Rr, Cyy + std::sin(A) * Rr, Gz - 0.01, Side);
			}
			for (int F = 0; F < Around; ++F)
			{
				Tri(Out, Peak, FootAt[F], FootAt[(F + 1) % Around]);
			}
		}
		Normals(Out, FirstPoint, FirstIndex);
		for (int K = 0; K < NU * NV; ++K)
		{
			const double W = Fade[static_cast<size_t>(K)];
			if (W > 0.0)
			{
				const size_t P = FirstPoint + static_cast<size_t>(K);
				LeanTo(Out, P, GroundUp(Lit, Height, Out.Positions[P * 3], Out.Positions[P * 3 + 1]), W);
			}
		}

		// The cut: the bank's footprint across the gap and its faces, counter-clockwise seen from above.
		double Corners[4][2];
		const double Cu[4] = { -CutU, CutU, CutU, -CutU }, Cv[4] = { -CutV, -CutV, CutV, CutV };
		for (int K = 0; K < 4; ++K)
		{
			World(Cu[K], Cv[K], Corners[K][0], Corners[K][1]);
		}
		// (u then v is a turn of x then y, so these run counter-clockwise already)
		for (int K = 0; K < 4; ++K)
		{
			OutCut.push_back(Corners[K][0]);
			OutCut.push_back(Corners[K][1]);
		}
	}

	namespace
	{
		/**
		 * Crater variety (Field::bVariety): this round's own hole, from the size SizeOf gives every round of its kind,
		 * by its weapon's slant, its fuze, the ground and its own dice (Own: the hole's seed).
		 */
		void Varied(Size& S, const std::string& Name, double CalibreMm, const std::string& Ground, uint64_t Own, double WayX, double WayY)
		{
			Dice V(Mix(Own, 0x5EEDC4A7E4ull));
			S.Vary = Mix(Own, 0xC4A7E4F00Dull) | 1u;
			const double Mm = CalibreMm == CalibreMm && CalibreMm > 0.0 ? CalibreMm : 75.0;
			// Its slant, by the weapon (how long against how wide; how much soil it threw forward and into the wings): a
			// tank's or an anti-tank gun's high explosive comes in nearly flat; a field gun's at a middling slant; a
			// howitzer's falls more steeply; a mortar bomb nearly straight down; a grenade rolls and bursts on the ground.
			struct Slant
			{
				double L0, L1, T0, T1;
			};
			const bool bFlat = Name == "gun75" || Name == "light" || (Name != "field" && Name != "mortar" && Name != "grenade" && Name != "howitzer"
				&& Name != "how105" && Name != "heavy" && Name != "how150" && Mm < 70.0);
			const Slant Sl = Name == "grenade" ? Slant { 1.0, 1.08, 0.0, 0.1 }
				: Name == "mortar" ? Slant { 1.0, 1.1, 0.05, 0.2 }
				: bFlat ? Slant { 1.35, 1.8, 0.55, 0.85 }
				: Name == "howitzer" || Name == "how105" || Name == "heavy" || Name == "how150" ? Slant { 1.08, 1.4, 0.25, 0.5 }
				: Slant { 1.15, 1.45, 0.3, 0.55 }; // (a field gun's, and any other by its calibre)
			const bool bShell = Name != "grenade";
			S.Long = V.U(Sl.L0, Sl.L1);
			S.Throw = V.U(Sl.T0, Sl.T1);
			// (turned along the way the round came; a round of no known way its own)
			if (std::hypot(WayX, WayY) > 0.2)
			{
				S.Heading = std::atan2(WayY, WayX) + V.U(-0.12, 0.12);
			}
			else
			{
				S.Heading = V.U(0.0, 2.0 * Pi);
			}
			// Its own size: a quarter either way, one in ten a big one (a soft patch, a round heavier with its charge).
			double Big = V.U(0.78, 1.25);
			if (V.U() < 0.1)
			{
				Big = V.U(1.25, 1.5);
			}
			S.Across *= Big;
			S.Deep *= V.U(0.75, 1.25) * std::sqrt(Big);
			S.Rim *= V.U(0.75, 1.35);
			S.Reach *= V.U(0.92, 1.12);
			S.Clods = static_cast<int>(std::lround(S.Clods * V.U(0.7, 1.4)));
			S.Scorch = V.U(0.3, 0.85);
			// Its fuze: most burst at once on the ground; one in eight a delay, bursting under it, deep and narrow, a high
			// rim, big clods, little soot on top; one in ten a graze, glancing on before it burst: a shallow, wide,
			// sooty scoop, longer.
			if (bShell)
			{
				const double Fuze = V.U();
				if (Fuze < 0.12)
				{
					S.Deep *= 1.45;
					S.Across *= 0.85;
					S.Rim *= 1.35;
					S.Throw *= 0.5;
					S.Long = 1.0 + 0.5 * (S.Long - 1.0);
					S.Scorch *= 0.35;
					S.Clods = static_cast<int>(std::lround(S.Clods * 1.4));
					S.Clod *= 1.25;
				}
				else if (Fuze < 0.22)
				{
					S.Deep *= 0.45;
					S.Across *= 1.15;
					S.Rim *= 0.6;
					S.Scorch = std::min(1.0, S.Scorch * 1.4);
					S.Long *= 1.15;
				}
			}
			// The subsoil a deep hole throws up on its crest.
			S.Subsoil = S.Deep >= 0.6 ? V.U(0.2, 0.55) * std::min(1.0, S.Deep) : S.Deep >= 0.35 ? V.U(0.0, 0.25) : 0.0;
			// The ground's own ways: mud slumps wide and low, its soot smothered; sand sprays wide and shallow; on snow the
			// thrown earth and the soot reach far over the white.
			if (Ground == "mud")
			{
				S.Across *= 1.1;
				S.Rim *= 0.75;
				S.Deep *= 0.85;
				S.Scorch *= 0.6;
			}
			else if (Ground == "sand")
			{
				S.Across *= 1.12;
				S.Rim *= 0.7;
				S.Deep *= 0.8;
				S.Reach *= 1.1;
				S.Scorch *= 0.7;
			}
			else if (Ground == "snow")
			{
				S.Reach *= 1.12;
				S.Scorch = std::min(1.0, S.Scorch * 1.5);
			}
			S.Rim = std::min(S.Rim, 0.4);
			S.Deep = std::min(S.Deep, 1.6);
			S.Clod = std::min(S.Clod, 0.32);
			S.Reach = std::min(S.Reach, 2.5);
			S.Long = std::clamp(S.Long, 1.0, 2.2);
			S.Clods = std::clamp(S.Clods, 0, 40);
			// The art director's round 1, 23 November: more points round it, so no edge of its runs straight for long
			// (a big hole's 22 sides drew straight lines a metre and more long on the grass beside it).
			S.Sides = std::min(48, static_cast<int>(std::lround(S.Sides * 1.4)));
			if (S.Long > 1.4)
			{
				S.Sides += 4; // (a long hole's ends rounded as its middle)
			}
			// Its spray: specks of soil out over the thin edge and past it, more on snow and sand where they show, more
			// from a graze's scoop than a delay's deep hole.
			S.Spray = static_cast<int>(std::lround((6.0 + S.Clods * V.U(0.9, 1.5)) * (Ground == "snow" || Ground == "sand" ? 1.25 : 1.0)));
			S.Spray = std::clamp(S.Spray, 0, 48);
			// A deep hole in mud holds water: about half of them.
			if (Ground == "mud" && S.Deep >= 0.3 && V.U() < 0.55)
			{
				S.Puddle = V.U(0.22, 0.45);
			}
		}
	}

	void Field::Begin(HeightFn InHeight, uint64_t InSeed, ColourFn InUnder)
	{
		Height = std::move(InHeight);
		Under = std::move(InUnder);
		Seed = InSeed ? InSeed : 1;
		Holes.clear();
		Breaches.clear();
		Scorches.clear();
		PatchList.clear();
		PatchOf.clear();
		Changed.clear();
		Cuts.clear();
		Squares.clear();
	}

	int64_t Field::SquareOf(int I, int J)
	{
		return (static_cast<int64_t>(I) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(J));
	}

	bool Field::Add(double X, double Y, const std::string& Name, double CalibreMm, const std::string& Ground, double WayX, double WayY)
	{
		return AddAged(X, Y, Name, CalibreMm, Ground, -1.0, WayX, WayY);
	}

	bool Field::AddOld(double X, double Y, const std::string& Name, double CalibreMm, const std::string& Ground, double Months)
	{
		return AddAged(X, Y, Name, CalibreMm, Ground, std::max(0.0, Months));
	}

	bool Field::AddAged(double X, double Y, const std::string& Name, double CalibreMm, const std::string& Ground, double Months, double WayX,
		double WayY)
	{
		if (!Height || Ground == "water" || static_cast<int>(Holes.size()) >= Most || !(X == X) || !(Y == Y) || std::fabs(X) > 1e6 || std::fabs(Y) > 1e6)
		{
			return false;
		}
		Size S = SizeOf(Name, CalibreMm);
		if (!(S.Across > 0.05))
		{
			return false;
		}
		// (on rock a round splinters and scars the stone, it does not dig a bowl: a third as deep; the effects thread's
		// ask with its "rock" word, 22 November)
		if (Ground == "rock")
		{
			S.Deep *= 0.35;
		}
		const int Number = static_cast<int>(Holes.size());
		const uint64_t Own = Mix(Mix(Seed, static_cast<uint64_t>(Number) + 1u), static_cast<uint64_t>(std::llround(X * 100.0)) * 2654435761ull ^ static_cast<uint64_t>(std::llround(Y * 100.0)));
		if (bVariety)
		{
			Varied(S, Name, CalibreMm, Ground, Own, WayX, WayY);
		}
		// (how far its crest reaches along its long axis: its radius every way when round)
		const double LongAxis = S.Vary ? std::pow(S.Long, 0.6) : 1.0;
		// (an old hole, by the art director's marks, 23 November: at six months its bowl 80% as deep and its crest 70% as
		// high, the soot, the clods and the splinters gone; at a year 65% and 50%; older, half as deep. Between them in
		// a straight line; a month or two old, nearly as fresh but for the soot the first rains wash out)
		const bool bOld = Months >= 0.0;
		const auto ByAge = [Months](double At0, double At6, double At12, double At24)
		{
			const double M = std::max(0.0, Months);
			return M <= 6.0 ? At0 + (At6 - At0) * M / 6.0 : M <= 12.0 ? At6 + (At12 - At6) * (M - 6.0) / 6.0 : M <= 24.0 ? At12 + (At24 - At12) * (M - 12.0) / 12.0 : At24;
		};
		if (bOld)
		{
			S.Weathered = std::min(1.0, 0.4 + 0.1 * Months);
			S.Deep *= ByAge(1.0, 0.8, 0.65, 0.5);
			S.Rim *= ByAge(1.0, 0.7, 0.5, 0.4);
			S.Reach *= ByAge(1.0, 0.88, 0.78, 0.72);
			S.Clods = 0;
			S.Spray = 0; // (crater variety: its specks long gone into the grass; its water, if it holds any, still there)
		}
		if (bOldEdge)
		{
			S.Feather = 0.0;
		}
		if (S.Deep > 0.0 && Dig)
		{
			// (dug only in open ground, its middle and all round its crest: a lane's floor, a bank or a yard keeps the
			// shallow form laid over it)
			bool bOpen = Dig(X, Y);
			for (int K = 0; K < 8 && bOpen; ++K)
			{
				const double A = Pi * K / 4.0;
				bOpen = Dig(X + std::cos(A) * 0.55 * S.Across * LongAxis, Y + std::sin(A) * 0.55 * S.Across * LongAxis);
			}
			if (!bOpen)
			{
				S.Deep = 0.0;
			}
		}
		const Place In = PlaceFor(X, Y);
		// (each hole its own few millimetres over the ground, so two laid over each other never flicker)
		LandField::Mesh& Into = PatchList[static_cast<size_t>(In.Patch)].Sections[static_cast<size_t>(In.Section)];
		Hole H;
		H.First = static_cast<uint32_t>(Into.Colours.size());
		const size_t FirstIndex = Into.Triangles.size();
		// (laid on what is there now: the floor of an older hole where it is torn in one)
		const HeightFn Surface = [this](double Px, double Py) { return SurfaceAt(Px, Py); };
		std::vector<double> CutLine;
		// (rock's thrown soil the region's own earth: the desert's sand, Italy's red earth; else the bocage's loam)
		Soil Paint = SoilOf((Ground == "earth" || Ground == "rock") && !Earth.empty() ? Earth : Ground);
		if (bOld)
		{
			// Grown over by six months (the art director's marks, 23 November): the worn rim grassed and a little paler than
			// the field round it (8% at six months, 5% at a year, the dry grass on its crest), the bowl's floor darker and
			// greener (15%, then 10%, older 6%: damp, the grass lusher there), its sides between; bare soil showing on no
			// more than a seventh of the sides at six months, none at a year. Under six months the fresh soil's colours
			// grow toward these by the month. The field's colour round it is what it is measured against, so a hole in
			// stubble is paler stubble on its rim and a hole in pasture greener pasture in its bowl.
			const Rgb8 Grown = Under ? Under(X, Y) : Paint.Turf;
			const double By = std::clamp(Months / 6.0, 0.0, 1.0);
			const Rgb8 Rim = Toned(Grown, ByAge(1.08, 1.08, 1.05, 1.03));
			const Rgb8 Dark = Toned(Grown, ByAge(0.85, 0.85, 0.9, 0.94));
			const double Green = ByAge(1.0, 1.0, 0.7, 0.45); // (the bowl's lusher green, wearing into the field's)
			Rgb8 Bowl = Dark;
			Bowl.R = Byte(Dark.R * (1.0 - 0.06 * Green));
			Bowl.B = Byte(Dark.B * (1.0 - 0.1 * Green));
			const Soil Was = Paint;
			Paint.Thrown = Mixed(Was.Thrown, Rim, By);
			Paint.Edge = Mixed(Was.Edge, Grown, std::min(1.0, By + 0.1));
			Paint.Crest = Mixed(Was.Crest, Rim, By);
			Paint.Turf = Mixed(Was.Turf, Grown, 0.6 + 0.4 * By);
			Paint.Clod = Mixed(Was.Clod, Toned(Rim, 0.95), By);
			Paint.Hollow = Mixed(Was.Hollow, Bowl, By);
			Paint.Wall = Mixed(Was.Wall, Mixed(Bowl, Rim, 0.45), By);
			Paint.Bare = Mixed(Was.Wall, Was.Crest, 0.4);
			Paint.Bared = Months < 6.0 ? 1.0 - 0.85 * By : 0.15 * std::clamp((12.0 - Months) / 6.0, 0.0, 1.0);
			// (grass is evener than torn soil: each point's tone wanders less as it grows over)
			Paint.Mottle = 1.0 - 0.6 * By;
		}
		// (the thrown soil on the ground as drawn, or on an older hole's faces inside its cut)
		HeightFn Thin;
		if (Drawn)
		{
			Thin = [this](double Px, double Py)
			{
				double Z = 0.0;
				if (InCut(Px, Py) && TopAt(Px, Py, Z))
				{
					return Z;
				}
				return std::max(Height(Px, Py), Drawn(Px, Py));
			};
		}
		if (S.Vary)
		{
			// Crater variety: its thrown soil lies on an older hole's where it reaches over it, in that soil's colour and
			// light at its thin edge, so no edge of it shows there (SoilAt). Read before this hole is laid, so its own
			// faces are never read.
			const HeightFn ThinGround = Thin;
			Thin = [this, ThinGround](double Px, double Py)
			{
				double Z = 0.0;
				if (InCut(Px, Py) && TopAt(Px, Py, Z))
				{
					return Z;
				}
				const double Bed = ThinGround ? ThinGround(Px, Py) : Height(Px, Py);
				Rgb8 C;
				double Ux = 0.0, Uy = 0.0, Uz = 1.0;
				return SoilAt(Px, Py, Z, C, Ux, Uy, Uz) && Z > Bed ? Z : Bed;
			};
		}
		const ColourFn UnderSoil = !S.Vary ? Under : ColourFn([this](double Px, double Py)
		{
			double Z = 0.0, Ux = 0.0, Uy = 0.0, Uz = 1.0;
			Rgb8 C;
			if (!InCut(Px, Py) && SoilAt(Px, Py, Z, C, Ux, Uy, Uz))
			{
				return C;
			}
			return Under ? Under(Px, Py) : Rgb8();
		});
		const UpFn LitSoil = !S.Vary ? Lit : UpFn([this, Surface](double Px, double Py, double& OutX, double& OutY, double& OutZ)
		{
			double Z = 0.0;
			Rgb8 C;
			if (!InCut(Px, Py) && SoilAt(Px, Py, Z, C, OutX, OutY, OutZ))
			{
				return;
			}
			const Up G = GroundUp(Lit, Surface, Px, Py); // (the ground's own, as Lay reads it without this)
			OutX = G.X;
			OutY = G.Y;
			OutZ = G.Z;
		});
		Lay(X, Y, S, Paint, Own, Surface, 0.004 * (Number % 7), Into, S.Vary && Under ? UnderSoil : Under, S.Deep > 0.0 ? &CutLine : nullptr, S.Vary ? LitSoil : Lit, Thin,
			S.Vary ? std::function<bool(double, double)>([this](double Px, double Py) { return InCut(Px, Py); }) : std::function<bool(double, double)>());
		H.Deep = CutLine.empty() ? 0.0 : S.Deep;
		// (an old hole's grass and corn stand round it but over its bowl: on its worn rim they have grown back)
		H.BareR = bOld ? (H.Deep > 0.0 ? 0.92 : 0.55) * 0.5 * S.Across : 0.0;
		H.X = X;
		H.Y = Y;
		H.R = 0.5 * S.Across;
		H.Rim = S.Rim;
		if (S.Vary)
		{
			H.Long = S.Long;
			H.Heading = S.Heading;
		}
		H.In = In;
		H.Sides = std::clamp(S.Sides, 8, 48);
		H.Clods = std::max(0, S.Clods) + (S.Vary ? std::max(0, S.Spray) : 0); // (the spray's specks laid as clods are, after them)
		// (how far it reaches: its farthest point, the thrown soil's ragged edge or a clod's foot)
		for (size_t P = H.First; P < Into.Colours.size(); ++P)
		{
			const double Dx = Into.Positions[P * 3] - X, Dy = Into.Positions[P * 3 + 1] - Y;
			H.Out = std::max(H.Out, std::sqrt(Dx * Dx + Dy * Dy));
		}
		Keep(H, std::move(CutLine), FirstIndex, std::max(1.2 * H.R, H.Out));
		return true;
	}

	Field::Place Field::PlaceFor(double X, double Y)
	{
		// (its patch: the square of the field its middle is in, numbered when its first hole is laid)
		const int PatchI = static_cast<int>(std::floor(X / PatchSize));
		const int PatchJ = static_cast<int>(std::floor(Y / PatchSize));
		const auto Known = PatchOf.find(SquareOf(PatchI, PatchJ));
		int PatchNumber = 0;
		if (Known == PatchOf.end())
		{
			PatchNumber = static_cast<int>(PatchList.size());
			PatchOf.emplace(SquareOf(PatchI, PatchJ), PatchNumber);
			Patch Fresh;
			Fresh.I = PatchI;
			Fresh.J = PatchJ;
			PatchList.push_back(std::move(Fresh));
		}
		else
		{
			PatchNumber = Known->second;
		}
		Patch& Own_ = PatchList[static_cast<size_t>(PatchNumber)];
		Place In;
		In.Patch = PatchNumber;
		In.Section = Own_.Count / PerChunk;
		if (static_cast<size_t>(In.Section) >= Own_.Sections.size())
		{
			Own_.Sections.resize(static_cast<size_t>(In.Section) + 1);
		}
		++Own_.Count;
		return In;
	}

	void Field::Keep(Hole& H, std::vector<double>&& CutLine, size_t FirstIndex, double Reach)
	{
		const int Number = static_cast<int>(Holes.size());
		const Place In = H.In;
		if (!CutLine.empty())
		{
			// The older holes it is torn in: what of them lay inside its cut is cut away (their floors and rims would
			// stand in the air over its bowl), each section once.
			double X0 = 1e30, Y0 = 1e30, X1 = -1e30, Y1 = -1e30;
			for (size_t K = 0; K + 1 < CutLine.size(); K += 2)
			{
				X0 = std::min(X0, CutLine[K]);
				X1 = std::max(X1, CutLine[K]);
				Y0 = std::min(Y0, CutLine[K + 1]);
				Y1 = std::max(Y1, CutLine[K + 1]);
			}
			std::vector<Place> Done;
			for (const Hole& Old : Holes)
			{
				if (Old.X + Old.Out < X0 || Old.X - Old.Out > X1 || Old.Y + Old.Out < Y0 || Old.Y - Old.Out > Y1)
				{
					continue;
				}
				if (std::any_of(Done.begin(), Done.end(), [&Old](const Place& P) { return P.Patch == Old.In.Patch && P.Section == Old.In.Section; }))
				{
					continue;
				}
				Done.push_back(Old.In);
				const bool bOwn = Old.In.Patch == In.Patch && Old.In.Section == In.Section;
				LandField::Mesh& Cut = PatchList[static_cast<size_t>(Old.In.Patch)].Sections[static_cast<size_t>(Old.In.Section)];
				if (CutOut(Cut, CutLine, bOwn ? FirstIndex : 0, bOwn ? Cut.Triangles.size() : 0)
					&& !std::any_of(Changed.begin(), Changed.end(), [&Old](const Place& P) { return P.Patch == Old.In.Patch && P.Section == Old.In.Section; }))
				{
					Changed.push_back(Old.In);
				}
			}
			H.Cut = CutLine;
			Cuts.push_back(std::move(CutLine));
		}
		const double HX = H.X, HY = H.Y;
		Holes.push_back(H);
		constexpr double Square = 8.0;
		for (int J = static_cast<int>(std::floor((HY - Reach) / Square)); J <= static_cast<int>(std::floor((HY + Reach) / Square)); ++J)
		{
			for (int I = static_cast<int>(std::floor((HX - Reach) / Square)); I <= static_cast<int>(std::floor((HX + Reach) / Square)); ++I)
			{
				Squares[SquareOf(I, J)].push_back(Number);
			}
		}
		const bool bListed = std::any_of(Changed.begin(), Changed.end(), [&In](const Place& P) { return P.Patch == In.Patch && P.Section == In.Section; });
		if (!bListed)
		{
			Changed.push_back(In);
		}
	}

	std::vector<Field::Place> Field::TakeChanged()
	{
		std::vector<Place> Out;
		Out.swap(Changed);
		return Out;
	}

	std::vector<std::vector<double>> Field::TakeCuts()
	{
		std::vector<std::vector<double>> Out;
		Out.swap(Cuts);
		return Out;
	}

	bool Field::InCut(double X, double Y) const
	{
		return CutAfter(-1, X, Y);
	}

	bool Field::CutAfter(int After, double X, double Y) const
	{
		if (Squares.empty() || !(X == X) || !(Y == Y))
		{
			return false;
		}
		constexpr double Square = 8.0;
		const auto Found = Squares.find(SquareOf(static_cast<int>(std::floor(X / Square)), static_cast<int>(std::floor(Y / Square))));
		if (Found == Squares.end())
		{
			return false;
		}
		for (const int K : Found->second)
		{
			const Hole& H = Holes[static_cast<size_t>(K)];
			if (K > After && !H.Cut.empty() && Inside(H.Cut, X, Y))
			{
				return true;
			}
		}
		return false;
	}

	double Field::SurfaceAt(double X, double Y) const
	{
		double Z = 0.0;
		if (InCut(X, Y) && TopAt(X, Y, Z))
		{
			return Z;
		}
		return Height ? Height(X, Y) : 0.0;
	}

	bool Field::Bare(double X, double Y) const
	{
		if (Squares.empty())
		{
			return false;
		}
		constexpr double Square = 8.0;
		const auto Found = Squares.find(SquareOf(static_cast<int>(std::floor(X / Square)), static_cast<int>(std::floor(Y / Square))));
		if (Found == Squares.end())
		{
			return false;
		}
		for (const int K : Found->second)
		{
			const Hole& H = Holes[static_cast<size_t>(K)];
			if (H.bBreach)
			{
				// (a breach: its torn gap and the spoil she pushed out, not the grass round them)
				const double Dx = X - H.X, Dy = Y - H.Y, Cy = std::cos(H.Yaw), Sy = std::sin(H.Yaw);
				const double U = Dx * Cy + Dy * Sy, V = (-Dx * Sy + Dy * Cy) * H.Through;
				if (std::fabs(U) <= H.CutU + 0.2 && V >= -(H.CutV + 0.3) && V <= H.SpoilOut - 0.3)
				{
					return true;
				}
				continue;
			}
			double D2 = (X - H.X) * (X - H.X) + (Y - H.Y) * (Y - H.Y);
			if (H.Long != 1.0)
			{
				// (crater variety: a long hole's clearing long with it)
				const double Ca = std::cos(H.Heading), Sa = std::sin(H.Heading);
				const double U = ((X - H.X) * Ca + (Y - H.Y) * Sa) / std::pow(H.Long, 0.6), V = (-(X - H.X) * Sa + (Y - H.Y) * Ca) / std::pow(H.Long, -0.4);
				D2 = U * U + V * V;
			}
			if (H.BareR > 0.0)
			{
				if (D2 < H.BareR * H.BareR)
				{
					return true;
				}
				continue;
			}
			// (a fresh hole: bare to its crest, then a ragged clearing, a tuft here and there left standing in the thrown
			// soil and more the further out, not a ruled circle of grass round it: the art director, 23 November)
			const double In = bOldEdge ? 1.2 * H.R : 1.12 * H.R, Out = bOldEdge ? 1.2 * H.R : 1.3 * H.R;
			if (D2 >= Out * Out)
			{
				continue;
			}
			if (D2 < In * In)
			{
				return true;
			}
			uint64_t Mix = (static_cast<uint64_t>(static_cast<int64_t>(std::floor(X * 50.0))) * 0x9E3779B97F4A7C15ull) ^ (static_cast<uint64_t>(static_cast<int64_t>(std::floor(Y * 50.0))) * 0xC2B2AE3D27D4EB4Full);
			Mix ^= Mix >> 31;
			Mix *= 0xBF58476D1CE4E5B9ull;
			Mix ^= Mix >> 29;
			const double Chance = static_cast<double>(Mix >> 11) * (1.0 / 9007199254740992.0);
			if (Chance >= (std::sqrt(D2) - In) / (Out - In))
			{
				return true;
			}
		}
		return false;
	}

	bool Field::OnBreach(const Hole& H, double X, double Y, double& OutZ) const
	{
		if (!H.bBreach || H.NU < 2 || H.NV < 2)
		{
			return false;
		}
		const double Dx = X - H.X, Dy = Y - H.Y, Cy = std::cos(H.Yaw), Sy = std::sin(H.Yaw);
		const double U = (Dx * Cy + Dy * Sy - H.U0) / H.Step, V = (-Dx * Sy + Dy * Cy - H.V0) / H.Step;
		if (!(U >= 0.0) || !(V >= 0.0) || U > H.NU - 1 || V > H.NV - 1)
		{
			return false;
		}
		const int I0 = std::min(H.NU - 2, static_cast<int>(U)), J0 = std::min(H.NV - 2, static_cast<int>(V));
		const LandField::Mesh& M = Section(H.In);
		const float* P = M.Positions.data();
		// (its square first; a point on a square's edge, its positions rounded to floats, or in the last row or column
		// (narrower than a step where the span is not a whole number of them) may lie in a neighbour's)
		for (int K = 0; K < 9; ++K)
		{
			const int I = I0 + (K == 0 ? 0 : (K - 1) % 3 - 1), J = J0 + (K == 0 ? 0 : (K - 1) / 3 - 1);
			if (I < 0 || J < 0 || I > H.NU - 2 || J > H.NV - 2 || (K > 0 && I == I0 && J == J0))
			{
				continue;
			}
			const uint32_t A = H.First + static_cast<uint32_t>(J * H.NU + I), B = A + 1u, C = A + static_cast<uint32_t>(H.NU), D = C + 1u;
			// (the two triangles of its square as laid: A, B, D and A, D, C; a point on their shared edge is in both)
			double Z = 0.0;
			if (OnTri(P, A, B, D, X, Y, Z) || OnTri(P, A, D, C, X, Y, Z))
			{
				OutZ = Z;
				return true;
			}
		}
		return false;
	}

	bool Field::BreachFloor(double X, double Y, double& OutZ) const
	{
		if (Breaches.empty() || !(X == X) || !(Y == Y))
		{
			return false;
		}
		// (the newest breach whose cut holds it: a shell hole torn in the gap later has its own bowl, which the tank
		// rides over as she rides over any other. A breach's reach first, a few multiplies, before its cut's outline)
		for (auto It = Breaches.rbegin(); It != Breaches.rend(); ++It)
		{
			const Hole& H = Holes[static_cast<size_t>(*It)];
			const double Dx = X - H.X, Dy = Y - H.Y;
			if (Dx * Dx + Dy * Dy <= H.Out * H.Out && !H.Cut.empty() && Inside(H.Cut, X, Y) && OnBreach(H, X, Y, OutZ))
			{
				return true;
			}
		}
		return false;
	}

	bool Field::AddBreach(const Breach& B, const RiseFn& Rise, const std::string& Ground)
	{
		if (!Height || !Rise || static_cast<int>(Holes.size()) >= Most || !(B.X == B.X) || !(B.Y == B.Y) || !(B.Yaw == B.Yaw)
			|| std::fabs(B.X) > 1e6 || std::fabs(B.Y) > 1e6 || !(B.Half > 0.5))
		{
			return false;
		}
		const int Number = static_cast<int>(Holes.size());
		const Place In = PlaceFor(B.X, B.Y);
		const uint64_t Own = Mix(Mix(Seed ^ 0xB4EAC4ull, static_cast<uint64_t>(Number) + 1u), static_cast<uint64_t>(std::llround(B.X * 100.0)) * 2654435761ull ^ static_cast<uint64_t>(std::llround(B.Y * 100.0)));
		LandField::Mesh& Into = PatchList[static_cast<size_t>(In.Patch)].Sections[static_cast<size_t>(In.Section)];
		Hole H;
		H.bBreach = true;
		H.First = static_cast<uint32_t>(Into.Colours.size());
		const size_t FirstIndex = Into.Triangles.size();
		H.X = B.X;
		H.Y = B.Y;
		H.Yaw = B.Yaw;
		H.Through = B.Through < 0 ? -1 : 1;
		H.In = In;
		std::vector<double> CutLine;
		LayBreach(B, SoilOf(Ground == "earth" && !Earth.empty() ? Earth : Ground), Own, Height, Rise, Into, Under, Lit, CutLine, H);
		if (CutLine.empty() || H.NU < 2)
		{
			return false;
		}
		H.R = H.CutU;
		for (size_t P = H.First; P < Into.Colours.size(); ++P)
		{
			const double Dx = Into.Positions[P * 3] - B.X, Dy = Into.Positions[P * 3 + 1] - B.Y;
			H.Out = std::max(H.Out, std::sqrt(Dx * Dx + Dy * Dy));
		}
		Keep(H, std::move(CutLine), FirstIndex, H.Out);
		Breaches.push_back(Number);
		return true;
	}

	double Field::Scorched(int Of) const
	{
		double Most_ = 0.0;
		for (const int K : Scorches)
		{
			const Hole& H = Holes[static_cast<size_t>(K)];
			if (H.BurntOf == Of + 1)
			{
				Most_ = std::max(Most_, H.Burnt);
			}
		}
		return Most_;
	}

	bool Field::AddScorch(int Of, double X, double Y, double AlongX, double AlongY, double Long, double Wide, double WindX, double WindY, double Burnt,
		const std::string& Ground)
	{
		if (!Height || Ground == "water" || Of < 0 || static_cast<int>(Holes.size()) >= Most || !(X == X) || !(Y == Y) || std::fabs(X) > 1e6
			|| std::fabs(Y) > 1e6 || !(Long > 0.5) || !(Wide > 0.3) || !(Burnt > 0.0) || Burnt <= Scorched(Of))
		{
			return false;
		}
		Burnt = std::min(1.0, Burnt);
		const bool bCore = Burnt >= 1.0;
		const double AlongL = std::hypot(AlongX, AlongY);
		const double Ax = AlongL > 1e-6 ? AlongX / AlongL : 0.0, Ay = AlongL > 1e-6 ? AlongY / AlongL : 1.0;
		const double WindL = std::hypot(WindX, WindY);
		const double Wx = WindL > 1e-6 && WindL == WindL ? WindX / WindL : 0.0, Wy = WindL > 1e-6 && WindL == WindL ? WindY / WindL : 0.0;
		const std::string Word = (Ground == "earth" || Ground == "rock") && !Earth.empty() ? Earth : Ground;
		const Soil Paint = SoilOf(Word);
		// What a burning vehicle leaves round her (the effects thread's brew-ups; Brian, 25 November: "make the grass and
		// battlefield graphics better and more realistic"): the grass browned and burnt off round her, darker toward her;
		// where no grass grows the sand darkened by her smoke, the snow melted to wet slush and earth, a town's stones
		// sooted. Then, once she has burnt a while, her soot core: the burnt oil, paint and rubber that fell round her,
		// black-brown, flecked with grey ash, carried downwind.
		Rgb8 Brown = Rgb(100, 84, 58), Straw = Rgb(134, 116, 78), Char = Rgb(50, 42, 34);
		double BrownBy = 0.8;
		if (Word == "sand")
		{
			Brown = Rgb(128, 108, 82);
			Straw = Rgb(150, 130, 100);
			Char = Rgb(84, 70, 56);
			BrownBy = 0.45;
		}
		else if (Word == "snow")
		{
			Brown = Rgb(112, 106, 98);
			Straw = Rgb(150, 146, 140);
			Char = Rgb(64, 54, 44);
			BrownBy = 0.7;
		}
		else if (Word == "town")
		{
			Brown = Rgb(96, 90, 82);
			Straw = Rgb(112, 106, 98);
			Char = Rgb(62, 56, 50);
			BrownBy = 0.45;
		}
		else if (Word == "mud")
		{
			Brown = Rgb(62, 52, 40);
			Straw = Rgb(74, 64, 50);
			Char = Rgb(44, 37, 30);
			BrownBy = 0.45;
		}
		else if (Word == "blackearth")
		{
			Brown = Rgb(112, 94, 62); // (the steppe's buff grass gone to straw and ash)
			Straw = Rgb(146, 128, 86);
			Char = Rgb(52, 45, 36);
		}
		const Rgb8 SootCore = Rgb(38, 32, 26), Ash = Rgb(118, 114, 106);
		// (the same outline for her browned grass and her soot core: her own dice, not the record's number)
		Dice D(Mix(Mix(Seed ^ 0x5C0BC4ull, static_cast<uint64_t>(Of) + 1u), static_cast<uint64_t>(std::llround(X * 10.0)) * 2654435761ull ^ static_cast<uint64_t>(std::llround(Y * 10.0))));
		const double Wave[3] = { D.U(0.03, 0.05), D.U(0.02, 0.035), D.U(0.01, 0.02) };
		const double Phase[3] = { D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi) };
		const double CoreWave[2] = { D.U(0.03, 0.05), D.U(0.015, 0.03) };
		const double CorePhase[2] = { D.U(0.0, 2.0 * Pi), D.U(0.0, 2.0 * Pi) };
		// (patches over the ground, not each point its own: a point's own dice drew spokes out from her middle)
		const uint64_t Salt = Mix(Seed ^ 0x5C0BC4ull, static_cast<uint64_t>(Of) + 1u);
		const auto Patchy = [Salt](double Px, double Py, double Across, uint64_t Kind)
		{
			const double Gx = Px / Across, Gy = Py / Across;
			const double Fx = std::floor(Gx), Fy = std::floor(Gy), Tx = Gx - Fx, Ty = Gy - Fy;
			const double Sx_ = Tx * Tx * (3.0 - 2.0 * Tx), Sy_ = Ty * Ty * (3.0 - 2.0 * Ty);
			const uint64_t K = Mix(Salt, Kind);
			const double A00 = PlaceShare(Fx, Fy, K), A10 = PlaceShare(Fx + 1.0, Fy, K), A01 = PlaceShare(Fx, Fy + 1.0, K), A11 = PlaceShare(Fx + 1.0, Fy + 1.0, K);
			return (A00 + (A10 - A00) * Sx_) + ((A01 + (A11 - A01) * Sx_) - (A00 + (A10 - A00) * Sx_)) * Sy_;
		};
		const double Reach = ScorchReach(Long);
		const double HalfLong = 0.5 * Long + 0.8, HalfWide = 0.5 * Wide + 0.8;
		// (her outline at a way A: the browned grass a ragged ring a tenth either way, carried a little downwind; her
		// soot her footprint's rounded box and 0.8 m more, carried downwind up to 1.6 times as far)
		// (the grass browned further downwind, as the fire ran with the wind; her soot kept 0.85 m inside it, so its
		// feathered edge ends in her browned grass, never on the green)
		const auto BrownEdge = [&](double A)
		{
			const double Down = std::max(0.0, std::cos(A) * Wx + std::sin(A) * Wy);
			// (and tongues where the fire ran on through the grass a little further)
			const double Tongue = 0.16 * (Patchy(3.2 * std::cos(A), 3.2 * std::sin(A), 1.0, 7u) - 0.5);
			const double Rag = 1.0 + Wave[0] * std::sin(3.0 * A + Phase[0]) + Wave[1] * std::sin(5.0 * A + Phase[1]) + Wave[2] * std::sin(8.0 * A + Phase[2]) + Tongue;
			return (Reach - 0.35) * Rag * (1.0 + 0.3 * std::pow(Down, 1.5)); // (its feather shows it to about Reach)
		};
		const auto Edge = [&](double A)
		{
			if (!bCore)
			{
				return BrownEdge(A);
			}
			const double Cx = std::cos(A), Sx = std::sin(A);
			const double Down = std::max(0.0, Cx * Wx + Sx * Wy);
			const double U = std::fabs(Cx * Ax + Sx * Ay) / HalfLong, Vv = std::fabs(-Cx * Ay + Sx * Ax) / HalfWide;
			const double Box = 1.0 / std::pow(std::pow(U, 4.0) + std::pow(Vv, 4.0), 0.25);
			const double Rag = 1.0 + CoreWave[0] * std::sin(4.0 * A + CorePhase[0]) + CoreWave[1] * std::sin(7.0 * A + CorePhase[1]);
			return std::min(Box * Rag * (1.0 + 0.6 * std::pow(Down, 1.5)), BrownEdge(A) - 0.85);
		};
		// What lies there now, to lie on: an older hole's soil or her own browned grass (in its colour and light), a dug
		// hole's floor inside its cut, else the ground as drawn.
		struct Bed
		{
			double Z = 0.0;
			Rgb8 C;
			Up N;
		};
		const auto BedAt = [&](double Px, double Py)
		{
			Bed B;
			const double Flat = Drawn ? std::max(Height(Px, Py), Drawn(Px, Py)) : Height(Px, Py);
			const bool bCut = InCut(Px, Py);
			double Z = 0.0, Ux = 0.0, Uy = 0.0, Uz = 1.0;
			Rgb8 C;
			if (SoilAt(Px, Py, Z, C, Ux, Uy, Uz) && (bCut || Z > Flat))
			{
				B.Z = Z;
				B.C = C;
				B.N.X = static_cast<float>(Ux);
				B.N.Y = static_cast<float>(Uy);
				B.N.Z = static_cast<float>(Uz);
				return B;
			}
			B.Z = bCut && TopAt(Px, Py, Z) ? Z : Flat;
			B.C = Under ? Under(Px, Py) : Paint.Turf;
			B.N = GroundUp(Lit, Height, Px, Py);
			return B;
		};
		constexpr int N = 36;
		// (the rings: six out to her outline, three feathering past it into what lies there: 1.2 m for the grass, 1.05 m
		// for the soot (BrownEdge keeps it inside the grass's); the lift over the bed, a centimetre inside thinning to 2 mm at the edge, so no step casts a line)
		const double Share[6] = { 0.2, 0.4, 0.58, 0.74, 0.88, 1.0 };
		const double CoreShare[6] = { 0.2, 0.4, 0.6, 0.75, 0.88, 1.0 };
		const double Past[3] = { 0.3, 0.7, 1.2 }, CorePast[3] = { 0.35, 0.7, 1.05 };
		const double Lift[HoleRings] = { 0.01, 0.01, 0.01, 0.01, 0.009, 0.008, 0.006, 0.004, 0.002 };
		const double CoreLift[HoleRings] = { 0.006, 0.006, 0.006, 0.006, 0.005, 0.004, 0.003, 0.002, 0.002 };
		const double BrownOf[HoleRings] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.45, 0.15, 0.0 };
		const double SootOf[HoleRings] = { 0.88, 0.88, 0.88, 0.85, 0.78, 0.62, 0.36, 0.14, 0.0 };
		const Place In = PlaceFor(X, Y);
		LandField::Mesh& Into = PatchList[static_cast<size_t>(In.Patch)].Sections[static_cast<size_t>(In.Section)];
		Hole H;
		H.First = static_cast<uint32_t>(Into.Colours.size());
		const size_t FirstIndex = Into.Triangles.size();
		// A point's colour: T its ring's share of the way out to her outline (over 1 past it), Share how much of the
		// burn reaches it (1 inside the outline, feathering to 0 at the last ring).
		const auto Lay_ = [&](double Px, double Py, double Up_, double T, double Burn)
		{
			const Bed B = BedAt(Px, Py);
			Rgb8 C = B.C;
			const double Big = Patchy(Px, Py, 1.7, 1u), Small = Patchy(Px, Py, 1.0, 2u), Pt = 0.65 * Big + 0.35 * Small;
			if (!bCore)
			{
				// (burnt black near her, brown further out, singed straw at the edge with the green showing through in
				// patches; black patches and straw patches all through, as a grass fire leaves it)
				const double Singed = T > 0.75 ? std::clamp((Pt - 0.5) / 0.3, 0.0, 1.0) * std::clamp((T - 0.75) / 0.25, 0.0, 1.0) : 0.0;
				const Rgb8 Burnt_ = Mixed(Brown, Straw, std::clamp((Pt - 0.45) * 1.6, 0.0, 1.0) * 0.7);
				C = Mixed(C, Burnt_, BrownBy * Burn * (1.0 - 0.6 * Singed));
				const double Black = 0.6 * std::clamp((0.6 - T) / 0.6, 0.0, 1.0) + 0.4 * std::clamp((0.42 - Pt) / 0.2, 0.0, 1.0) * (T < 0.95 ? 1.0 : 0.0);
				C = Mixed(C, Char, std::min(0.8, Black * Burn));
			}
			else
			{
				// (black-brown, darker in patches where her fuel and oil ran, grey ash lying in drifts)
				C = Mixed(C, SootCore, std::min(0.95, Burn * (0.82 + 0.22 * (1.0 - Pt))));
				const double Drift = Patchy(Px, Py, 1.4, 4u);
				if (T <= 0.9 && Drift > 0.62)
				{
					C = Mixed(C, Ash, 0.4 * std::min(1.0, (Drift - 0.62) / 0.25));
				}
			}
			const uint32_t I = Put(Into, Px, Py, B.Z + Up_, C);
			Into.Normals[static_cast<size_t>(I) * 3] = B.N.X;
			Into.Normals[static_cast<size_t>(I) * 3 + 1] = B.N.Y;
			Into.Normals[static_cast<size_t>(I) * 3 + 2] = B.N.Z;
			return I;
		};
		const uint32_t Middle = Lay_(X, Y, bCore ? CoreLift[0] : Lift[0], 0.0, bCore ? SootOf[0] : 1.0);
		std::vector<double> EdgeOf(static_cast<size_t>(N));
		for (int K = 0; K < N; ++K)
		{
			EdgeOf[static_cast<size_t>(K)] = Edge(2.0 * Pi * K / N);
		}
		for (int Ring = 0; Ring < HoleRings; ++Ring)
		{
			for (int K = 0; K < N; ++K)
			{
				const double A = 2.0 * Pi * K / N, E = EdgeOf[static_cast<size_t>(K)];
				const double Rr = Ring < 6 ? E * (bCore ? CoreShare[Ring] : Share[Ring]) : E + (bCore ? CorePast[Ring - 6] : Past[Ring - 6]);
				const double Px = X + std::cos(A) * Rr, Py = Y + std::sin(A) * Rr;
				// (the feathering rings by patches, so the burnt edge runs out in uneven tongues; the last the bed's own
				// colour, so no edge shows)
				const double Jit = Ring >= 6 && Ring < HoleRings - 1 ? 0.7 + 0.55 * Patchy(Px, Py, 0.8, 3u) : 1.0;
				const double T = Ring < 6 ? (bCore ? CoreShare[Ring] : Share[Ring]) : 1.0 + 0.1 * (Ring - 5);
				Lay_(Px, Py, bCore ? CoreLift[Ring] : Lift[Ring], T, std::min(1.0, (bCore ? SootOf[Ring] / SootOf[0] : BrownOf[Ring]) * Jit));
			}
		}
		for (int K = 0; K < N; ++K)
		{
			const int K1 = (K + 1) % N;
			const auto Ring_ = [&H](int R, int Side) { return H.First + 1u + static_cast<uint32_t>(R * N + Side); };
			Tri(Into, Middle, Ring_(0, K), Ring_(0, K1));
			for (int Ring = 0; Ring + 1 < HoleRings; ++Ring)
			{
				Tri(Into, Ring_(Ring, K), Ring_(Ring + 1, K), Ring_(Ring + 1, K1));
				Tri(Into, Ring_(Ring, K), Ring_(Ring + 1, K1), Ring_(Ring, K1));
			}
		}
		H.X = X;
		H.Y = Y;
		H.In = In;
		H.Sides = N;
		// (the grass and corn burnt off to about her browned ring, raggedly: Bare's fresh clearing, 1.12 to 1.3 of R;
		// her soot core clears nothing more)
		H.R = bCore ? 0.0 : Reach / 1.15;
		H.Burnt = Burnt;
		H.BurntOf = Of + 1;
		H.BurntX = Wx;
		H.BurntY = Wy;
		for (size_t P = H.First; P < Into.Colours.size(); ++P)
		{
			const double Dx = Into.Positions[P * 3] - X, Dy = Into.Positions[P * 3 + 1] - Y;
			H.Out = std::max(H.Out, std::sqrt(Dx * Dx + Dy * Dy));
		}
		const int Number = static_cast<int>(Holes.size());
		Keep(H, std::vector<double>(), FirstIndex, H.Out);
		Scorches.push_back(Number);
		return true;
	}

	bool Field::SoilAt(double X, double Y, double& OutZ, Rgb8& OutColour, double& OutUpX, double& OutUpY, double& OutUpZ) const
	{
		if (Squares.empty() || !(X == X) || !(Y == Y))
		{
			return false;
		}
		constexpr double Square = 8.0;
		const auto Found = Squares.find(SquareOf(static_cast<int>(std::floor(X / Square)), static_cast<int>(std::floor(Y / Square))));
		if (Found == Squares.end())
		{
			return false;
		}
		bool bAny = false;
		double Best = -1e30;
		const auto Take = [&](const LandField::Mesh& M, uint32_t A, uint32_t B, uint32_t C)
		{
			double Z = 0.0, Sb = 0.0, Tc = 0.0;
			if (!OnTriShares(M.Positions.data(), A, B, C, X, Y, Z, Sb, Tc) || !(Z > Best))
			{
				return;
			}
			const double Sa = 1.0 - Sb - Tc;
			const Rgb8 &Ca = M.Colours[A], &Cb = M.Colours[B], &Cc = M.Colours[C];
			OutColour.R = Byte(Sa * Ca.R + Sb * Cb.R + Tc * Cc.R);
			OutColour.G = Byte(Sa * Ca.G + Sb * Cb.G + Tc * Cc.G);
			OutColour.B = Byte(Sa * Ca.B + Sb * Cb.B + Tc * Cc.B);
			double Up[3] = { 0.0, 0.0, 1.0 };
			if (M.Normals.size() >= M.Positions.size())
			{
				for (int Ax = 0; Ax < 3; ++Ax)
				{
					Up[Ax] = Sa * M.Normals[static_cast<size_t>(A) * 3 + static_cast<size_t>(Ax)] + Sb * M.Normals[static_cast<size_t>(B) * 3 + static_cast<size_t>(Ax)]
						+ Tc * M.Normals[static_cast<size_t>(C) * 3 + static_cast<size_t>(Ax)];
				}
			}
			const double L = std::sqrt(Up[0] * Up[0] + Up[1] * Up[1] + Up[2] * Up[2]);
			if (L > 1e-6 && Up[2] > 0.0)
			{
				OutUpX = Up[0] / L;
				OutUpY = Up[1] / L;
				OutUpZ = Up[2] / L;
			}
			else
			{
				OutUpX = 0.0;
				OutUpY = 0.0;
				OutUpZ = 1.0;
			}
			Best = Z;
			bAny = true;
		};
		// (newest first, as TopAt: a dug hole's cut takes away the older holes' faces inside it)
		const std::vector<int>& Listed = Found->second;
		for (auto It = Listed.rbegin(); It != Listed.rend(); ++It)
		{
			const Hole& H = Holes[static_cast<size_t>(*It)];
			const bool bLast = !H.Cut.empty() && Inside(H.Cut, X, Y);
			const double Dx = X - H.X, Dy = Y - H.Y;
			if (!H.bBreach && Dx * Dx + Dy * Dy <= H.Out * H.Out && H.Sides >= 3)
			{
				const LandField::Mesh& M = Section(H.In);
				const int N = H.Sides;
				const uint32_t Middle = H.First;
				const auto Ring = [&H, N](int R, int Side) { return H.First + 1u + static_cast<uint32_t>(R * N + (Side % N + N) % N); };
				double A = std::atan2(Dy, Dx);
				if (A < 0.0) A += 2.0 * Pi;
				const int Side0 = static_cast<int>(std::floor(A / (2.0 * Pi) * N));
				for (int Side = Side0 - 1; Side <= Side0 + 1; ++Side)
				{
					Take(M, Middle, Ring(0, Side), Ring(0, Side + 1));
					for (int R = 0; R + 1 < HoleRings; ++R)
					{
						Take(M, Ring(R, Side), Ring(R + 1, Side), Ring(R + 1, Side + 1));
						Take(M, Ring(R, Side), Ring(R + 1, Side + 1), Ring(R, Side + 1));
					}
				}
			}
			if (bLast)
			{
				break;
			}
		}
		if (bAny)
		{
			OutZ = Best;
		}
		return bAny;
	}

	bool Field::TopAt(double X, double Y, double& OutZ) const
	{
		if (Squares.empty() || !(X == X) || !(Y == Y))
		{
			return false;
		}
		constexpr double Square = 8.0;
		const auto Found = Squares.find(SquareOf(static_cast<int>(std::floor(X / Square)), static_cast<int>(std::floor(Y / Square))));
		if (Found == Squares.end())
		{
			return false;
		}
		bool bAny = false;
		double Best = -1e30;
		const auto Take = [&](const float* P, uint32_t A, uint32_t B, uint32_t C)
		{
			double Z = 0.0;
			if (OnTri(P, A, B, C, X, Y, Z) && Z > Best)
			{
				Best = Z;
				bAny = true;
			}
		};
		// (newest first: a dug hole's cut takes away the older holes' faces inside it, so none older is read there)
		const std::vector<int>& Listed = Found->second;
		for (auto It = Listed.rbegin(); It != Listed.rend(); ++It)
		{
			const int K = *It;
			const Hole& H = Holes[static_cast<size_t>(K)];
			const double Dx = X - H.X, Dy = Y - H.Y;
			const bool bLast = !H.Cut.empty() && Inside(H.Cut, X, Y);
			if (H.bBreach)
			{
				double Z = 0.0;
				if (Dx * Dx + Dy * Dy <= H.Out * H.Out)
				{
					if (OnBreach(H, X, Y, Z) && Z > Best)
					{
						Best = Z;
						bAny = true;
					}
					// its clods (LayBreach's: a peak and five round its foot, after the grid's points)
					const float* P = Section(H.In).Positions.data();
					const uint32_t Clod0 = H.First + static_cast<uint32_t>(H.NU * H.NV);
					for (int C = 0; C < H.Clods; ++C)
					{
						const uint32_t Peak = Clod0 + static_cast<uint32_t>(C * ClodPoints);
						const float* Pk = P + static_cast<size_t>(Peak) * 3;
						if (std::fabs(X - Pk[0]) > 0.6 || std::fabs(Y - Pk[1]) > 0.6)
						{
							continue;
						}
						for (int F = 0; F < ClodPoints - 1; ++F)
						{
							Take(P, Peak, Peak + 1u + static_cast<uint32_t>(F), Peak + 1u + static_cast<uint32_t>((F + 1) % (ClodPoints - 1)));
						}
					}
				}
				if (bLast)
				{
					break;
				}
				continue;
			}
			if (Dx * Dx + Dy * Dy > H.Out * H.Out || H.Sides < 3)
			{
				continue;
			}
			const LandField::Mesh& M = Section(H.In);
			const float* P = M.Positions.data();
			const int N = H.Sides;
			const uint32_t Middle = H.First;
			const auto Ring = [&H, N](int R, int Side) { return H.First + 1u + static_cast<uint32_t>(R * N + (Side % N + N) % N); };
			// The rings as laid (Lay): the side the point is towards and its two neighbours (each ring's points are
			// turned a little off their side, the thin edge's most), the floor's fan and the quads out from it.
			double A = std::atan2(Dy, Dx);
			if (A < 0.0) A += 2.0 * Pi;
			const int Side0 = static_cast<int>(std::floor(A / (2.0 * Pi) * N));
			for (int Side = Side0 - 1; Side <= Side0 + 1; ++Side)
			{
				Take(P, Middle, Ring(0, Side), Ring(0, Side + 1));
				for (int R = 0; R + 1 < HoleRings; ++R)
				{
					Take(P, Ring(R, Side), Ring(R + 1, Side), Ring(R + 1, Side + 1));
					Take(P, Ring(R, Side), Ring(R + 1, Side + 1), Ring(R, Side + 1));
				}
			}
			// Its clods: a peak and five round its foot, each a low lump.
			const uint32_t Clod0 = H.First + 1u + static_cast<uint32_t>(HoleRings * N);
			for (int C = 0; C < H.Clods; ++C)
			{
				const uint32_t Peak = Clod0 + static_cast<uint32_t>(C * ClodPoints);
				const float* Pk = P + static_cast<size_t>(Peak) * 3;
				if (std::fabs(X - Pk[0]) > 0.6 || std::fabs(Y - Pk[1]) > 0.6)
				{
					continue;
				}
				for (int F = 0; F < ClodPoints - 1; ++F)
				{
					Take(P, Peak, Peak + 1u + static_cast<uint32_t>(F), Peak + 1u + static_cast<uint32_t>((F + 1) % (ClodPoints - 1)));
				}
			}
			if (bLast)
			{
				break;
			}
		}
		if (bAny)
		{
			OutZ = Best;
		}
		return bAny;
	}
	bool Inside(const std::vector<double>& Outline, double X, double Y)
	{
		const size_t N = Outline.size() / 2;
		if (N < 3)
		{
			return false;
		}
		for (size_t I = 0; I < N; ++I)
		{
			const size_t J = (I + 1) % N;
			const double Ax = Outline[2 * I], Ay = Outline[2 * I + 1], Bx = Outline[2 * J], By = Outline[2 * J + 1];
			if ((Bx - Ax) * (Y - Ay) - (By - Ay) * (X - Ax) < 0.0)
			{
				return false;
			}
		}
		return true;
	}

	namespace
	{
		/** A point of a piece being cut: its place, normal and colour, and its own index when it is one of the mesh's. */
		struct CutPt
		{
			double P[3] = { 0.0, 0.0, 0.0 }, N[3] = { 0.0, 0.0, 1.0 }, C[3] = { 0.0, 0.0, 0.0 };
			int64_t Own = -1;
			double PloughUv[2] = { 0.0, 0.0 }; // (ploughland's two numbers, where the mesh carries them: LandField::Mesh::Uvs)
		};

		CutPt Between(const CutPt& A, const CutPt& B, double T)
		{
			CutPt O;
			for (int K = 0; K < 3; ++K)
			{
				O.P[K] = A.P[K] + (B.P[K] - A.P[K]) * T;
				O.N[K] = A.N[K] + (B.N[K] - A.N[K]) * T;
				O.C[K] = A.C[K] + (B.C[K] - A.C[K]) * T;
			}
			for (int K = 0; K < 2; ++K) O.PloughUv[K] = A.PloughUv[K] + (B.PloughUv[K] - A.PloughUv[K]) * T;
			return O;
		}

		/** Sutherland and Hodgman: the part of a convex piece on the inner side of the line A to B (its left, for a
		 * counter-clockwise outline), or on its outer side. */
		void ClipBy(const std::vector<CutPt>& In, double Ax, double Ay, double Bx, double By, bool bInner, std::vector<CutPt>& Out)
		{
			Out.clear();
			const size_t N = In.size();
			for (size_t I = 0; I < N; ++I)
			{
				const CutPt& Cur = In[I];
				const CutPt& Next = In[(I + 1) % N];
				double Dc = (Bx - Ax) * (Cur.P[1] - Ay) - (By - Ay) * (Cur.P[0] - Ax);
				double Dn = (Bx - Ax) * (Next.P[1] - Ay) - (By - Ay) * (Next.P[0] - Ax);
				if (!bInner)
				{
					Dc = -Dc;
					Dn = -Dn;
				}
				const bool bCur = Dc >= 0.0, bNext = Dn >= 0.0;
				if (bCur)
				{
					Out.push_back(Cur);
				}
				if (bCur != bNext)
				{
					Out.push_back(Between(Cur, Next, Dc / (Dc - Dn)));
				}
			}
		}

		double Area3(const double* A, const double* B, const double* C)
		{
			const double Ux = B[0] - A[0], Uy = B[1] - A[1], Uz = B[2] - A[2];
			const double Vx = C[0] - A[0], Vy = C[1] - A[1], Vz = C[2] - A[2];
			const double Nx = Uy * Vz - Uz * Vy, Ny = Uz * Vx - Ux * Vz, Nz = Ux * Vy - Uy * Vx;
			return 0.5 * std::sqrt(Nx * Nx + Ny * Ny + Nz * Nz);
		}
	}

	bool CutOut(LandField::Mesh& M, const std::vector<double>& Outline, size_t KeepFrom, size_t KeepTo)
	{
		const size_t N = Outline.size() / 2;
		if (N < 3 || M.Triangles.empty())
		{
			return false;
		}
		double X0 = 1e30, Y0 = 1e30, X1 = -1e30, Y1 = -1e30;
		for (size_t K = 0; K < N; ++K)
		{
			X0 = std::min(X0, Outline[2 * K]);
			X1 = std::max(X1, Outline[2 * K]);
			Y0 = std::min(Y0, Outline[2 * K + 1]);
			Y1 = std::max(Y1, Outline[2 * K + 1]);
		}
		const auto Side = [&Outline, N](size_t E, double X, double Y)
		{
			const size_t F = (E + 1) % N;
			const double Ax = Outline[2 * E], Ay = Outline[2 * E + 1], Bx = Outline[2 * F], By = Outline[2 * F + 1];
			return (Bx - Ax) * (Y - Ay) - (By - Ay) * (X - Ax);
		};
		const auto PointOf = [&M](uint32_t I)
		{
			CutPt O;
			const size_t K = static_cast<size_t>(I) * 3;
			for (int D = 0; D < 3; ++D)
			{
				O.P[D] = M.Positions[K + static_cast<size_t>(D)];
				O.N[D] = M.Normals[K + static_cast<size_t>(D)];
			}
			const Rgb8& C = M.Colours[I];
			O.C[0] = C.R;
			O.C[1] = C.G;
			O.C[2] = C.B;
			O.Own = I;
			if (M.Uvs.size() == 2 * M.Points())
			{
				O.PloughUv[0] = M.Uvs[static_cast<size_t>(I) * 2];
				O.PloughUv[1] = M.Uvs[static_cast<size_t>(I) * 2 + 1];
			}
			return O;
		};
		std::vector<uint32_t> Kept;
		Kept.reserve(M.Triangles.size());
		bool bCut = false;
		std::vector<CutPt> Rest, Piece, Next;
		std::vector<std::pair<CutPt, uint32_t>> Made;
		const auto IndexOf = [&M, &Made](const CutPt& Q) -> uint32_t
		{
			if (Q.Own >= 0)
			{
				return static_cast<uint32_t>(Q.Own);
			}
			for (const std::pair<CutPt, uint32_t>& Was : Made)
			{
				if (Was.first.P[0] == Q.P[0] && Was.first.P[1] == Q.P[1] && Was.first.P[2] == Q.P[2])
				{
					return Was.second;
				}
			}
			// (a mesh with ploughland's two numbers a point keeps them on every point the cut makes, read between)
			const bool bPloughUvs = !M.Uvs.empty() && M.Uvs.size() == 2 * M.Points();
			const uint32_t I = Put(M, Q.P[0], Q.P[1], Q.P[2], Rgb(static_cast<int>(std::lround(Q.C[0])), static_cast<int>(std::lround(Q.C[1])), static_cast<int>(std::lround(Q.C[2]))));
			if (bPloughUvs)
			{
				M.Uvs.push_back(static_cast<float>(Q.PloughUv[0]));
				M.Uvs.push_back(static_cast<float>(Q.PloughUv[1]));
			}
			const double L = std::sqrt(Q.N[0] * Q.N[0] + Q.N[1] * Q.N[1] + Q.N[2] * Q.N[2]);
			const size_t K = static_cast<size_t>(I) * 3;
			M.Normals[K] = static_cast<float>(L > 1e-9 ? Q.N[0] / L : 0.0);
			M.Normals[K + 1] = static_cast<float>(L > 1e-9 ? Q.N[1] / L : 0.0);
			M.Normals[K + 2] = static_cast<float>(L > 1e-9 ? Q.N[2] / L : 1.0);
			Made.emplace_back(Q, I);
			return I;
		};
		const size_t Count = M.Triangles.size();
		for (size_t T = 0; T + 2 < Count; T += 3)
		{
			const uint32_t A = M.Triangles[T], B = M.Triangles[T + 1], C = M.Triangles[T + 2];
			if (T >= KeepFrom && T < KeepTo)
			{
				Kept.insert(Kept.end(), { A, B, C });
				continue;
			}
			const float* Pa = &M.Positions[static_cast<size_t>(A) * 3];
			const float* Pb = &M.Positions[static_cast<size_t>(B) * 3];
			const float* Pc = &M.Positions[static_cast<size_t>(C) * 3];
			if (std::max({ Pa[0], Pb[0], Pc[0] }) < X0 || std::min({ Pa[0], Pb[0], Pc[0] }) > X1 || std::max({ Pa[1], Pb[1], Pc[1] }) < Y0 || std::min({ Pa[1], Pb[1], Pc[1] }) > Y1)
			{
				Kept.insert(Kept.end(), { A, B, C });
				continue;
			}
			// (wholly outside one of the outline's sides: untouched; wholly inside all: gone)
			bool bApart = false, bAllIn = true;
			for (size_t E = 0; E < N && !bApart; ++E)
			{
				const double Sa = Side(E, Pa[0], Pa[1]), Sb = Side(E, Pb[0], Pb[1]), Sc = Side(E, Pc[0], Pc[1]);
				bApart = Sa < 0.0 && Sb < 0.0 && Sc < 0.0;
				bAllIn = bAllIn && Sa >= 0.0 && Sb >= 0.0 && Sc >= 0.0;
			}
			if (bApart)
			{
				Kept.insert(Kept.end(), { A, B, C });
				continue;
			}
			bCut = true;
			if (bAllIn)
			{
				continue;
			}
			// What lies outside the outline, as convex pieces: outside its first side; inside the first and outside
			// the second; and on round. What is left inside every side is the part cut away.
			Rest = { PointOf(A), PointOf(B), PointOf(C) };
			Made.clear();
			for (size_t E = 0; E < N && Rest.size() >= 3; ++E)
			{
				const size_t F = (E + 1) % N;
				ClipBy(Rest, Outline[2 * E], Outline[2 * E + 1], Outline[2 * F], Outline[2 * F + 1], false, Piece);
				if (Piece.size() >= 3)
				{
					for (size_t K = 1; K + 1 < Piece.size(); ++K)
					{
						if (Area3(Piece[0].P, Piece[K].P, Piece[K + 1].P) > 1e-9)
						{
							Kept.push_back(IndexOf(Piece[0]));
							Kept.push_back(IndexOf(Piece[K]));
							Kept.push_back(IndexOf(Piece[K + 1]));
						}
					}
				}
				ClipBy(Rest, Outline[2 * E], Outline[2 * E + 1], Outline[2 * F], Outline[2 * F + 1], true, Next);
				Rest.swap(Next);
			}
		}
		if (bCut)
		{
			M.Triangles.swap(Kept);
		}
		return bCut;
	}

	void Dug::Begin(const std::vector<LandField::Tile>* InTiles)
	{
		Tiles = InTiles;
		Parts.clear();
		Boxes.clear();
		Split_.clear();
		Changed.clear();
		CutCount = 0;
		if (Tiles)
		{
			Parts.resize(Tiles->size());
			Boxes.resize(Tiles->size());
		}
	}

	void Dug::SplitTile(int T)
	{
		const LandField::Tile& Tile = (*Tiles)[static_cast<size_t>(T)];
		const LandField::Mesh& Near = Tile.Near;
		std::vector<LandField::Mesh>& Into = Parts[static_cast<size_t>(T)];
		std::vector<float>& Box = Boxes[static_cast<size_t>(T)];
		Into.assign(static_cast<size_t>(Split * Split), LandField::Mesh());
		Box.assign(static_cast<size_t>(Split * Split * 4), 0.0f);
		for (int K = 0; K < Split * Split; ++K)
		{
			Box[static_cast<size_t>(K) * 4] = Box[static_cast<size_t>(K) * 4 + 1] = 1e30f;
			Box[static_cast<size_t>(K) * 4 + 2] = Box[static_cast<size_t>(K) * 4 + 3] = -1e30f;
		}
		const double W = (Tile.X1 - Tile.X0) / Split, H = (Tile.Y1 - Tile.Y0) / Split;
		std::vector<std::vector<int32_t>> NewOf(static_cast<size_t>(Split * Split));
		for (size_t Tr = 0; Tr + 2 < Near.Triangles.size(); Tr += 3)
		{
			double Mx = 0.0, My = 0.0;
			for (int V = 0; V < 3; ++V)
			{
				Mx += Near.Positions[static_cast<size_t>(Near.Triangles[Tr + static_cast<size_t>(V)]) * 3];
				My += Near.Positions[static_cast<size_t>(Near.Triangles[Tr + static_cast<size_t>(V)]) * 3 + 1];
			}
			const int I = std::clamp(static_cast<int>(std::floor((Mx / 3.0 - Tile.X0) / W)), 0, Split - 1);
			const int J = std::clamp(static_cast<int>(std::floor((My / 3.0 - Tile.Y0) / H)), 0, Split - 1);
			const size_t Pk = static_cast<size_t>(J * Split + I);
			LandField::Mesh& Part = Into[Pk];
			std::vector<int32_t>& Map = NewOf[Pk];
			if (Map.empty())
			{
				Map.assign(Near.Points(), -1);
			}
			for (int V = 0; V < 3; ++V)
			{
				const uint32_t Old = Near.Triangles[Tr + static_cast<size_t>(V)];
				if (Map[Old] < 0)
				{
					Map[Old] = static_cast<int32_t>(Part.Points());
					for (int D = 0; D < 3; ++D)
					{
						Part.Positions.push_back(Near.Positions[static_cast<size_t>(Old) * 3 + static_cast<size_t>(D)]);
						Part.Normals.push_back(Near.Normals[static_cast<size_t>(Old) * 3 + static_cast<size_t>(D)]);
					}
					Part.Colours.push_back(Near.Colours[Old]);
					// (ploughland's furrows, on trial: the tile's two numbers a point go with the point, so a shelled
					// tile keeps its furrows; a tile without them keeps none)
					if (Near.Uvs.size() == 2 * Near.Points())
					{
						Part.Uvs.push_back(Near.Uvs[static_cast<size_t>(Old) * 2]);
						Part.Uvs.push_back(Near.Uvs[static_cast<size_t>(Old) * 2 + 1]);
					}
					float* B = &Box[Pk * 4];
					const float Px = Near.Positions[static_cast<size_t>(Old) * 3], Py = Near.Positions[static_cast<size_t>(Old) * 3 + 1];
					B[0] = std::min(B[0], Px);
					B[1] = std::min(B[1], Py);
					B[2] = std::max(B[2], Px);
					B[3] = std::max(B[3], Py);
				}
				Part.Triangles.push_back(static_cast<uint32_t>(Map[Old]));
			}
		}
		Split_.push_back(T);
	}

	void Dug::Cut(const std::vector<double>& Outline)
	{
		if (!Tiles || Outline.size() < 6)
		{
			return;
		}
		double X0 = 1e30, Y0 = 1e30, X1 = -1e30, Y1 = -1e30;
		for (size_t K = 0; K + 1 < Outline.size(); K += 2)
		{
			X0 = std::min(X0, Outline[K]);
			X1 = std::max(X1, Outline[K]);
			Y0 = std::min(Y0, Outline[K + 1]);
			Y1 = std::max(Y1, Outline[K + 1]);
		}
		++CutCount;
		for (size_t T = 0; T < Tiles->size(); ++T)
		{
			const LandField::Tile& Tile = (*Tiles)[T];
			constexpr double Slack = 12.0; // (a bank's or lane's points may stand a little past the tile's square)
			if (X1 < Tile.X0 - Slack || X0 > Tile.X1 + Slack || Y1 < Tile.Y0 - Slack || Y0 > Tile.Y1 + Slack || Tile.Near.Empty())
			{
				continue;
			}
			if (Parts[T].empty())
			{
				SplitTile(static_cast<int>(T));
			}
			for (int P = 0; P < Split * Split; ++P)
			{
				const float* B = &Boxes[T][static_cast<size_t>(P) * 4];
				if (X1 < B[0] || X0 > B[2] || Y1 < B[1] || Y0 > B[3])
				{
					continue;
				}
				if (CutOut(Parts[T][static_cast<size_t>(P)], Outline))
				{
					const Place Where{ static_cast<int>(T), P };
					if (!std::any_of(Changed.begin(), Changed.end(), [&Where](const Place& C) { return C.Tile == Where.Tile && C.Part == Where.Part; }))
					{
						Changed.push_back(Where);
					}
				}
			}
		}
	}

	std::vector<int> Dug::TakeSplit()
	{
		std::vector<int> Out;
		Out.swap(Split_);
		return Out;
	}

	std::vector<Dug::Place> Dug::TakeChanged()
	{
		std::vector<Place> Out;
		Out.swap(Changed);
		return Out;
	}

	void Ruts::Cut(const std::vector<double>& Outline)
	{
		if (Outline.size() < 6)
		{
			return;
		}
		double X0 = 1e30, Y0 = 1e30, X1 = -1e30, Y1 = -1e30;
		for (size_t K = 0; K + 1 < Outline.size(); K += 2)
		{
			X0 = std::min(X0, Outline[K]);
			X1 = std::max(X1, Outline[K]);
			Y0 = std::min(Y0, Outline[K + 1]);
			Y1 = std::max(Y1, Outline[K + 1]);
		}
		for (size_t P = 0; P < PatchList.size(); ++P)
		{
			Patch& Pt = PatchList[P];
			constexpr double Slack = 8.0; // (a piece belongs to the square its middle is in; its strips reach a little past it)
			if (X1 < Pt.I * PatchSize - Slack || X0 > (Pt.I + 1) * PatchSize + Slack || Y1 < Pt.J * PatchSize - Slack || Y0 > (Pt.J + 1) * PatchSize + Slack)
			{
				continue;
			}
			for (size_t Sc = 0; Sc < Pt.Sections.size(); ++Sc)
			{
				if (CutOut(Pt.Sections[Sc], Outline))
				{
					const Place Where{ static_cast<int>(P), static_cast<int>(Sc) };
					if (!std::any_of(Changed.begin(), Changed.end(), [&Where](const Place& C) { return C.Patch == Where.Patch && C.Section == Where.Section; }))
					{
						Changed.push_back(Where);
					}
				}
			}
		}
	}

	void Ruts::Begin(HeightFn InHeight, ColourFn InUnder, KindFn InKind)
	{
		Height = std::move(InHeight);
		UnderColour = std::move(InUnder);
		Kind = std::move(InKind);
		Was.clear();
		Laid.clear();
		PatchList.clear();
		PatchOf.clear();
		Changed.clear();
		Squares.clear();
		Count = 0;
	}

	double Ruts::TrackWidthOf(const std::string& Name, const std::string& Kind)
	{
		auto Has = [&Name](const char* Word) { return Name.find(Word) != std::string::npos; };
		if (Has("Tiger")) return 0.725;
		if (Has("Panther")) return 0.66;
		if (Has("Sherman")) return 0.42;
		if (Has("Panzer") || Has("Pz") || Has("StuG")) return 0.40;
		if (Kind == "tank") return 0.45;
		if (Kind == "gun") return 0.16;
		return 0.26; // (a lorry's or a carrier's tyres)
	}

	void Ruts::Lift(int V)
	{
		if (V >= 0 && static_cast<size_t>(V) < Was.size())
		{
			Was[static_cast<size_t>(V)].bOn = false;
		}
	}

	std::vector<Ruts::Place> Ruts::TakeChanged()
	{
		std::vector<Place> Out;
		Out.swap(Changed);
		return Out;
	}

	Ruts::Patch& Ruts::PatchFor(double X, double Y, Place& OutIn)
	{
		const int PatchI = static_cast<int>(std::floor(X / PatchSize));
		const int PatchJ = static_cast<int>(std::floor(Y / PatchSize));
		const auto Known = PatchOf.find(SquareOf(PatchI, PatchJ));
		int Number = 0;
		if (Known == PatchOf.end())
		{
			Number = static_cast<int>(PatchList.size());
			PatchOf.emplace(SquareOf(PatchI, PatchJ), Number);
			Patch Fresh;
			Fresh.I = PatchI;
			Fresh.J = PatchJ;
			PatchList.push_back(std::move(Fresh));
		}
		else
		{
			Number = Known->second;
		}
		Patch& P = PatchList[static_cast<size_t>(Number)];
		OutIn.Patch = Number;
		OutIn.Section = P.Count / PerChunk;
		if (static_cast<size_t>(OutIn.Section) >= P.Sections.size())
		{
			P.Sections.resize(static_cast<size_t>(OutIn.Section) + 1);
		}
		++P.Count;
		return P;
	}

	void Ruts::Strip(LandField::Mesh& Out, double AX, double AY, double BX, double BY, double Half, double Edge, const Rgb8& Paint, double Lift,
		uint64_t Salt) const
	{
		const double Dx = BX - AX, Dy = BY - AY;
		const double Long = std::sqrt(Dx * Dx + Dy * Dy);
		if (!(Long > 1e-6))
		{
			return;
		}
		// Across her way, left to right: the ground's own colour at the outer edge, a feathered band part paint and part
		// ground, the pressed strip (two points), the band, the edge; laid counter-clockwise, lit as the ground round it.
		// Each end its own widths and tones from its place, so the edges run ragged and the pressed strip mottled piece to
		// piece, and one piece's end is the next one's start (the art director's cut 4, shot 07: "the track marks are
		// hard-edged flat stripes", "feather it").
		const double Nx = -Dy / Long, Ny = Dx / Long;
		uint32_t A[StripAcross], B[StripAcross];
		for (int End = 0; End < 2; ++End)
		{
			const double Cx = End ? BX : AX, Cy = End ? BY : AY;
			const Up Here = GroundUp(Lit, Height, Cx, Cy);
			double Off[StripAcross];
			Rgb8 Colour[StripAcross];
			for (int Side = 0; Side < 2; ++Side)
			{
				const uint64_t Own = Salt * 2u + static_cast<uint64_t>(Side);
				const double Sign = Side ? -1.0 : 1.0;
				const int OuterK = Side ? StripAcross - 1 : 0, BandK = Side ? StripAcross - 2 : 1, PressK = Side ? StripAcross - 3 : 2;
				Off[OuterK] = Sign * (Half + Edge * (0.75 + 0.55 * PlaceShare(Cx, Cy, Own ^ 0x0e1ull)));
				Off[BandK] = Sign * (Half + Edge * (0.25 + 0.3 * PlaceShare(Cx, Cy, Own ^ 0xba2ull)));
				Off[PressK] = Sign * Half * (0.78 + 0.22 * PlaceShare(Cx, Cy, Own ^ 0x9e3ull));
				const Rgb8 Ground = UnderColour ? UnderColour(Cx + Nx * Off[OuterK], Cy + Ny * Off[OuterK]) : Paint;
				Colour[OuterK] = Ground;
				Colour[BandK] = Mixed(Paint, Ground, 0.35 + 0.3 * PlaceShare(Cx, Cy, Own ^ 0x5f4ull));
				Colour[PressK] = Toned(Paint, 0.88 + 0.18 * PlaceShare(Cx, Cy, Own ^ 0x7c5ull));
			}
			for (int K = 0; K < StripAcross; ++K)
			{
				const double Px = Cx + Nx * Off[K], Py = Cy + Ny * Off[K];
				const uint32_t P = Put(Out, Px, Py, Height(Px, Py) + Lift, Colour[K]);
				Out.Normals[static_cast<size_t>(P) * 3] = Here.X;
				Out.Normals[static_cast<size_t>(P) * 3 + 1] = Here.Y;
				Out.Normals[static_cast<size_t>(P) * 3 + 2] = Here.Z;
				(End ? B : A)[K] = P;
			}
		}
		for (int K = 0; K + 1 < StripAcross; ++K)
		{
			Tri(Out, A[K], A[K + 1], B[K + 1]);
			Tri(Out, A[K], B[K + 1], B[K]);
		}
	}

	bool Ruts::Drive(int V, double LX, double LY, double RX, double RY, double Width)
	{
		if (V < 0 || !Height || !Kind || !(LX == LX) || !(LY == LY) || !(RX == RX) || !(RY == RY) || !(Width > 0.0))
		{
			return false;
		}
		if (static_cast<size_t>(V) >= Was.size())
		{
			Was.resize(static_cast<size_t>(V) + 1);
		}
		Last& L = Was[static_cast<size_t>(V)];
		const double MX = 0.5 * (LX + RX), MY = 0.5 * (LY + RY);
		const double PX = 0.5 * (L.LX + L.RX), PY = 0.5 * (L.LY + L.RY);
		const double Moved = std::sqrt((MX - PX) * (MX - PX) + (MY - PY) * (MY - PY));
		auto Mark = [&]() { L.bOn = true; L.LX = LX; L.LY = LY; L.RX = RX; L.RY = RY; };
		if (!L.bOn || Moved > 8.0 * Step)
		{
			// (her first sight, or a jump: no mark across it)
			Mark();
			return false;
		}
		if (Moved < Step)
		{
			return false;
		}
		if (Count >= Most)
		{
			Mark();
			return false;
		}
		if (DugAt && (DugAt(MX, MY) || DugAt(PX, PY) || DugAt(0.5 * (MX + PX), 0.5 * (MY + PY)) || DugAt(LX, LY) || DugAt(RX, RY)))
		{
			// (across a dug hole: the ground is gone there, no mark across its bowl)
			Mark();
			return false;
		}
		const double Apart = std::sqrt((LX - RX) * (LX - RX) + (LY - RY) * (LY - RY));
		const Under Mid = Kind(0.5 * (MX + PX), 0.5 * (MY + PY));
		Place In;
		Patch& P = PatchFor(0.5 * (MX + PX), 0.5 * (MY + PY), In);
		LandField::Mesh& Out = P.Sections[static_cast<size_t>(In.Section)];
		const double Own = 0.001 * (V % 16); // (each tank her own millimetre, so two crossing never flicker, nor a column on one line)
		// In standing corn, the corn between her tracks (and under them) laid flat: straw pressed down, greyer than the
		// standing ears and darker than the open ground, its stalks' shadows and the earth showing between them (a paler
		// straw read as a mat laid under her: the art director's cut 4, shot 07), its edges as ragged as the tracks'.
		if (Mid == Under::Corn && Apart > Width)
		{
			const Rgb8 Laid_ = Mixed(UnderColour ? UnderColour(MX, MY) : Rgb(150, 136, 96), Rgb(118, 104, 72), 0.5);
			Strip(Out, PX, PY, MX, MY, 0.5 * Apart, 0.6, Laid_, 0.012 + Own, static_cast<uint64_t>(V) * 3u);
		}
		// Each track's own strip: pressed grass with the soil in it, crushed straw in the soil, or a dark rut.
		for (int Side = 0; Side < 2; ++Side)
		{
			const double AX = Side ? L.RX : L.LX, AY = Side ? L.RY : L.LY, BX = Side ? RX : LX, BY = Side ? RY : LY;
			const Under Here = Kind(0.5 * (AX + BX), 0.5 * (AY + BY));
			if (Here == Under::None)
			{
				continue;
			}
			const Rgb8 Ground = UnderColour ? UnderColour(0.5 * (AX + BX), 0.5 * (AY + BY)) : Rgb(100, 100, 70);
			const Rgb8 Paint = Here == Under::Grass ? Mixed(Ground, Rgb(80, 74, 48), 0.55)
				: Here == Under::Corn ? Mixed(Ground, Rgb(108, 92, 64), 0.6)
				: Mixed(Ground, Rgb(70, 58, 43), 0.55);
			// (pressed across most of her track, then a wide feathered band: the plates' ends and the turf they tore)
			Strip(Out, AX, AY, BX, BY, 0.36 * Width, 0.5 * Width, Paint, 0.02 + Own, static_cast<uint64_t>(V) * 3u + 1u + static_cast<uint64_t>(Side));
		}
		Piece Pc;
		Pc.AX = PX;
		Pc.AY = PY;
		Pc.BX = MX;
		Pc.BY = MY;
		Pc.Apart = Apart;
		Pc.Width = Width;
		Pc.bCorn = Mid == Under::Corn;
		Keep(Pc);
		++Count;
		Touched(In);
		Mark();
		return true;
	}

	void Ruts::Keep(const Piece& Pc)
	{
		const int Number = static_cast<int>(Laid.size());
		Laid.push_back(Pc);
		const double Reach = 0.5 * Pc.Apart + Pc.Width;
		constexpr double Square = 8.0;
		const int I0 = static_cast<int>(std::floor((std::min(Pc.AX, Pc.BX) - Reach) / Square)), I1 = static_cast<int>(std::floor((std::max(Pc.AX, Pc.BX) + Reach) / Square));
		const int J0 = static_cast<int>(std::floor((std::min(Pc.AY, Pc.BY) - Reach) / Square)), J1 = static_cast<int>(std::floor((std::max(Pc.AY, Pc.BY) + Reach) / Square));
		for (int J = J0; J <= J1; ++J)
		{
			for (int I = I0; I <= I1; ++I)
			{
				Squares[SquareOf(I, J)].push_back(Number);
			}
		}
	}

	void Ruts::Touched(const Place& In)
	{
		const bool bListed = std::any_of(Changed.begin(), Changed.end(), [&In](const Place& C) { return C.Patch == In.Patch && C.Section == In.Section; });
		if (!bListed)
		{
			Changed.push_back(In);
		}
	}

	int Ruts::LayOld(const std::vector<std::vector<std::pair<double, double>>>& Ways, double Months, bool bWet, uint64_t Seed)
	{
		if (!Height || !Kind)
		{
			return 0;
		}
		Dice D(Mix(Seed, 0x0D5A75ull));
		const double M = std::max(0.0, Months);
		// (a grassed rut's green: whole at three months, a quarter at two years)
		const double Shows = M < 3.0 ? 1.0 : std::clamp(1.0 - 0.75 * (M - 3.0) / 21.0, 0.25, 1.0);
		const bool bBare = bWet || M < 3.0; // (dried mud or puddled: nothing grows in them yet)
		int Pieces_ = 0;
		uint64_t Salt = Mix(Seed, 0x01DA75ull);
		for (const std::vector<std::pair<double, double>>& Way : Ways)
		{
			// (the way at an even step along it, each point's heading from its neighbours, so the pieces join at a bend)
			std::vector<std::pair<double, double>> At;
			for (size_t K = 0; K + 1 < Way.size(); ++K)
			{
				const double AX = Way[K].first, AY = Way[K].second, BX = Way[K + 1].first, BY = Way[K + 1].second;
				const double Long = std::hypot(BX - AX, BY - AY);
				if (!(Long > 1e-6) || Long > 1e4)
				{
					continue;
				}
				const int Steps = std::max(1, static_cast<int>(std::ceil(Long / OldStep)));
				for (int I = (At.empty() ? 0 : 1); I <= Steps; ++I)
				{
					const double T = static_cast<double>(I) / Steps;
					At.emplace_back(AX + (BX - AX) * T, AY + (BY - AY) * T);
				}
			}
			if (At.size() < 2)
			{
				continue;
			}
			const double Apart = D.U(2.5, 2.9);
			const double Width = D.U(0.5, 0.7);
			std::vector<std::pair<double, double>> Left(At.size()), Right(At.size());
			for (size_t K = 0; K < At.size(); ++K)
			{
				const std::pair<double, double>& P0 = At[K > 0 ? K - 1 : 0];
				const std::pair<double, double>& P1 = At[K + 1 < At.size() ? K + 1 : K];
				const double Dx = P1.first - P0.first, Dy = P1.second - P0.second, Long = std::max(1e-6, std::hypot(Dx, Dy));
				const double Nx = -Dy / Long, Ny = Dx / Long;
				// (each rut wandering its own few centimetres: a column's tracks never quite on one line)
				const double WanderL = 0.08 * std::sin(0.11 * static_cast<double>(K) + 1.7 * static_cast<double>(Salt % 7));
				const double WanderR = 0.08 * std::sin(0.13 * static_cast<double>(K) + 0.9 * static_cast<double>(Salt % 5));
				Left[K] = { At[K].first + Nx * (0.5 * Apart + WanderL), At[K].second + Ny * (0.5 * Apart + WanderL) };
				Right[K] = { At[K].first - Nx * (0.5 * Apart + WanderR), At[K].second - Ny * (0.5 * Apart + WanderR) };
			}
			for (size_t K = 0; K + 1 < At.size(); ++K)
			{
				if (Pieces_ >= MostOld || Count >= Most)
				{
					return Pieces_;
				}
				const double MX = 0.5 * (At[K].first + At[K + 1].first), MY = 0.5 * (At[K].second + At[K + 1].second);
				if (DugAt && (DugAt(At[K].first, At[K].second) || DugAt(At[K + 1].first, At[K + 1].second) || DugAt(MX, MY)))
				{
					continue;
				}
				Under Sides[2];
				bool bAny = false;
				for (int Side = 0; Side < 2; ++Side)
				{
					const std::vector<std::pair<double, double>>& Line = Side ? Right : Left;
					Sides[Side] = Kind(0.5 * (Line[K].first + Line[K + 1].first), 0.5 * (Line[K].second + Line[K + 1].second));
					bAny = bAny || (Sides[Side] != Under::None && Sides[Side] != Under::Corn);
				}
				if (!bAny)
				{
					continue;
				}
				Place In;
				Patch& P = PatchFor(MX, MY, In);
				LandField::Mesh& Out = P.Sections[static_cast<size_t>(In.Section)];
				for (int Side = 0; Side < 2; ++Side)
				{
					++Salt;
					if (Sides[Side] == Under::None || Sides[Side] == Under::Corn)
					{
						continue;
					}
					const std::vector<std::pair<double, double>>& Line = Side ? Right : Left;
					const double AX = Line[K].first, AY = Line[K].second, BX = Line[K + 1].first, BY = Line[K + 1].second;
					const Rgb8 Ground = UnderColour ? UnderColour(0.5 * (AX + BX), 0.5 * (AY + BY)) : Rgb(100, 100, 70);
					Rgb8 Paint;
					if (bWet)
					{
						Paint = Mixed(Ground, Rgb(44, 38, 30), 0.72); // (water standing in them, wet mud at their sides)
					}
					else if (M < 3.0)
					{
						Paint = Mixed(Ground, Rgb(138, 124, 98), 0.62); // (the spring's mud dried in them, pale)
					}
					else
					{
						// (grassed over: the ruts hold the wet, so their grass grows lusher and darker than the field's)
						Rgb8 Lush = Toned(Ground, 0.84);
						Lush.R = Byte(Lush.R * 0.94);
						Lush.B = Byte(Lush.B * 0.88);
						Paint = Mixed(Ground, Lush, Shows);
					}
					Strip(Out, AX, AY, BX, BY, 0.4 * Width, 0.6 * Width, Paint, 0.017, Salt);
				}
				if (bBare)
				{
					Piece Pc;
					Pc.AX = At[K].first;
					Pc.AY = At[K].second;
					Pc.BX = At[K + 1].first;
					Pc.BY = At[K + 1].second;
					Pc.Apart = Apart;
					Pc.Width = Width;
					Keep(Pc);
				}
				++Count;
				++Pieces_;
				Touched(In);
			}
		}
		return Pieces_;
	}

	bool Ruts::Flat(double X, double Y) const
	{
		if (Squares.empty() || !(X == X) || !(Y == Y))
		{
			return false;
		}
		constexpr double Square = 8.0;
		const auto Found = Squares.find(SquareOf(static_cast<int>(std::floor(X / Square)), static_cast<int>(std::floor(Y / Square))));
		if (Found == Squares.end())
		{
			return false;
		}
		const double Share = PlaceShare(X, Y, 0xf1a7ull);
		for (const int K : Found->second)
		{
			const Piece& Pc = Laid[static_cast<size_t>(K)];
			const double Dx = Pc.BX - Pc.AX, Dy = Pc.BY - Pc.AY;
			const double Long2 = Dx * Dx + Dy * Dy;
			if (!(Long2 > 1e-12))
			{
				continue;
			}
			const double T = ((X - Pc.AX) * Dx + (Y - Pc.AY) * Dy) / Long2;
			if (T < -0.05 || T > 1.05)
			{
				continue;
			}
			// (each thing its own reach, by its place: crushed to a ragged edge, some at a track's edge left standing and
			// some past it bent down, never a band ruled straight through the grass; the art director's cut 4, shot 07)
			const double Off = std::fabs(((X - Pc.AX) * Dy - (Y - Pc.AY) * Dx) / std::sqrt(Long2));
			const double Reach = 0.5 * Pc.Width * (0.55 + 0.75 * Share);
			if (Pc.bCorn ? Off <= 0.5 * Pc.Apart + Reach : std::fabs(Off - 0.5 * Pc.Apart) <= Reach)
			{
				return true;
			}
		}
		return false;
	}

	std::vector<std::vector<std::pair<double, double>>> OldWays(const std::vector<std::pair<double, double>>& Fought, int Battles, uint64_t Seed,
		const std::vector<std::pair<double, double>>& Gates, double Heading, const std::function<bool(double X, double Y)>& Open)
	{
		std::vector<std::vector<std::pair<double, double>>> Ways;
		if (Battles <= 0)
		{
			return Ways;
		}
		// The fought-over ground, as LayScars reads it: the middle four fifths each way, 300 m at least.
		std::vector<double> Xs, Ys;
		for (const auto& P : Fought)
		{
			if (P.first == P.first && P.second == P.second)
			{
				Xs.push_back(P.first);
				Ys.push_back(P.second);
			}
		}
		if (Xs.empty())
		{
			return Ways;
		}
		std::sort(Xs.begin(), Xs.end());
		std::sort(Ys.begin(), Ys.end());
		const auto At = [](const std::vector<double>& V, double Q) { return V[std::min(V.size() - 1, static_cast<size_t>(Q * static_cast<double>(V.size() - 1) + 0.5))]; };
		double X0 = At(Xs, 0.1), X1 = At(Xs, 0.9), Y0 = At(Ys, 0.1), Y1 = At(Ys, 0.9);
		const auto Widen = [](double& A, double& B) { const double Mid = 0.5 * (A + B), Half = std::max(150.0, 0.5 * (B - A)); A = Mid - Half; B = Mid + Half; };
		Widen(X0, X1);
		Widen(Y0, Y1);
		const auto InBox = [&](double X, double Y) { return X >= X0 && X <= X1 && Y >= Y0 && Y <= Y1; };
		const int Wanted = std::min(10, 3 + 2 * Battles);
		Dice D(Mix(Seed, 0x3A7Dull));
		// (a leg from A to B, bowed a little to one side, a point every 6 m; false, and nothing added, where it would cross
		// what a column cannot drive over, but within 6 m of its ends, the gateways, whose floor reads as the bank's)
		const auto Leg = [&D, &Open](std::vector<std::pair<double, double>>& Way, double AX, double AY, double BX, double BY)
		{
			const double Long = std::hypot(BX - AX, BY - AY);
			const double Bow = D.U(-0.12, 0.12) * Long;
			const double Cx = 0.5 * (AX + BX) - (BY - AY) / std::max(1e-6, Long) * Bow, Cy = 0.5 * (AY + BY) + (BX - AX) / std::max(1e-6, Long) * Bow;
			const auto On = [&](double T) { const double U = 1.0 - T; return std::pair<double, double>(U * U * AX + 2.0 * U * T * Cx + T * T * BX, U * U * AY + 2.0 * U * T * Cy + T * T * BY); };
			if (Open)
			{
				const int Looks = std::max(2, static_cast<int>(Long / 1.2));
				for (int I = 0; I <= Looks; ++I)
				{
					const double T = static_cast<double>(I) / Looks;
					if (T * Long < 6.0 || (1.0 - T) * Long < 6.0) continue;
					const std::pair<double, double> P = On(T);
					if (!Open(P.first, P.second)) return false;
				}
			}
			const int Steps = std::max(2, static_cast<int>(Long / 6.0));
			for (int I = Way.empty() ? 0 : 1; I <= Steps; ++I)
			{
				Way.push_back(On(static_cast<double>(I) / Steps));
			}
			return true;
		};
		std::vector<std::pair<double, double>> Gate;
		for (const auto& G : Gates)
		{
			if (InBox(G.first, G.second)) Gate.push_back(G);
		}
		if (Gate.size() >= 4)
		{
			// Through the gateways: from one to another, on and on, never back the way it came.
			for (int W = 0; W < Wanted * 3 && static_cast<int>(Ways.size()) < Wanted; ++W)
			{
				size_t Here = std::min(Gate.size() - 1, static_cast<size_t>(D.U() * static_cast<double>(Gate.size())));
				std::vector<size_t> Been{ Here };
				std::vector<std::pair<double, double>> Way;
				double Was = 0.0;
				const int Legs = 2 + static_cast<int>(D.U() * 3.0);
				for (int L = 0; L < Legs; ++L)
				{
					std::vector<size_t> Next;
					for (size_t K = 0; K < Gate.size(); ++K)
					{
						if (std::find(Been.begin(), Been.end(), K) != Been.end()) continue;
						const double Dx = Gate[K].first - Gate[Here].first, Dy = Gate[K].second - Gate[Here].second;
						const double Long = std::hypot(Dx, Dy);
						if (Long < 30.0 || Long > 220.0) continue;
						if (L > 0 && std::cos(std::atan2(Dy, Dx) - Was) < std::cos(75.0 * Pi / 180.0)) continue;
						Next.push_back(K);
					}
					if (Next.empty())
					{
						break;
					}
					// (a gateway that way a column could reach without crossing a hedge: tried in a seeded order)
					bool bTook = false;
					while (!Next.empty() && !bTook)
					{
						const size_t Pick = std::min(Next.size() - 1, static_cast<size_t>(D.U() * static_cast<double>(Next.size())));
						const size_t To = Next[Pick];
						Next.erase(Next.begin() + static_cast<std::ptrdiff_t>(Pick));
						if (Leg(Way, Gate[Here].first, Gate[Here].second, Gate[To].first, Gate[To].second))
						{
							Was = std::atan2(Gate[To].second - Gate[Here].second, Gate[To].first - Gate[Here].first);
							Been.push_back(To);
							Here = To;
							bTook = true;
						}
					}
					if (!bTook)
					{
						break;
					}
				}
				if (Way.size() >= 2)
				{
					Ways.push_back(std::move(Way));
				}
			}
			if (!Ways.empty())
			{
				return Ways;
			}
		}
		// Open ground: across it along the battle's heading (else the line its ground spreads along most), wandering.
		double Head = Heading;
		if (!(Head == Head))
		{
			double Mx = 0.0, My = 0.0;
			for (size_t K = 0; K < Xs.size(); ++K) { Mx += Xs[K]; My += Ys[K]; }
			Mx /= static_cast<double>(Xs.size());
			My /= static_cast<double>(Xs.size());
			double Sxx = 0.0, Syy = 0.0, Sxy = 0.0;
			for (const auto& P : Fought)
			{
				if (!(P.first == P.first && P.second == P.second)) continue;
				Sxx += (P.first - Mx) * (P.first - Mx);
				Syy += (P.second - My) * (P.second - My);
				Sxy += (P.first - Mx) * (P.second - My);
			}
			Head = 0.5 * std::atan2(2.0 * Sxy, Sxx - Syy);
		}
		const double Cx = 0.5 * (X0 + X1), Cy = 0.5 * (Y0 + Y1), Half = 0.5 * std::max(X1 - X0, Y1 - Y0);
		for (int W = 0; W < Wanted; ++W)
		{
			// (starting behind the ground's middle, off to a side, and driving on through it)
			const double Back = D.U(0.4, 0.9) * Half, Aside = D.U(-0.8, 0.8) * Half;
			const double Hx = std::cos(Head), Hy = std::sin(Head);
			double X = Cx - Hx * Back - Hy * Aside, Y = Cy - Hy * Back + Hx * Aside;
			double A = Head + D.U(-0.3, 0.3);
			const double Long = D.U(150.0, 450.0);
			if (Open && !Open(X, Y))
			{
				continue;
			}
			std::vector<std::pair<double, double>> Way{ { X, Y } };
			for (double Gone = 0.0; Gone < Long; Gone += 6.0)
			{
				A += D.U(-0.07, 0.07) + 0.1 * std::remainder(Head - A, 2.0 * Pi);
				// (ended where it meets what a column cannot drive over: checked every 1.2 m of the step)
				bool bBlocked = false;
				for (int K = 1; K <= 5 && Open && !bBlocked; ++K)
				{
					bBlocked = !Open(X + std::cos(A) * 1.2 * K, Y + std::sin(A) * 1.2 * K);
				}
				if (bBlocked)
				{
					break;
				}
				X += std::cos(A) * 6.0;
				Y += std::sin(A) * 6.0;
				Way.emplace_back(X, Y);
			}
			if (Way.size() >= 11) // (60 m at least, or no way to read)
			{
				Ways.push_back(std::move(Way));
			}
		}
		return Ways;
	}

	OldLines OldLinesOf(const LandField::Ground& G, double TownX, double TownY, double TownR)
	{
		OldLines Out;
		for (const LandField::HedgeLine& H : G.Hedges())
		{
			if (H.Points.size() >= 2) Out.Lines.push_back(H.Points);
		}
		const std::vector<std::vector<std::pair<double, double>>>& Lanes = G.Lanes();
		for (const std::vector<std::pair<double, double>>& L : Lanes)
		{
			if (L.size() >= 2) Out.Lines.push_back(L);
		}
		for (size_t A = 0; A < Lanes.size(); ++A)
		{
			if (Lanes[A].size() < 2) continue;
			for (const std::pair<double, double>& End : { Lanes[A].front(), Lanes[A].back() })
			{
				bool bMeets = false;
				for (size_t B = 0; B < Lanes.size() && !bMeets; ++B)
				{
					if (B == A) continue;
					for (const std::pair<double, double>& P : Lanes[B])
					{
						if (std::fabs(P.first - End.first) < 8.0 && std::fabs(P.second - End.second) < 8.0)
						{
							bMeets = true;
							break;
						}
					}
				}
				if (bMeets) Out.Spots.push_back(End);
			}
		}
		if (TownR > 0.0)
		{
			for (int K = 0; K < 10; ++K)
			{
				const double A = 2.0 * Pi * (K + 0.5) / 10.0;
				Out.Spots.emplace_back(TownX + std::cos(A) * TownR, TownY + std::sin(A) * TownR);
			}
		}
		return Out;
	}

	int LayScars(Field& Holes, const std::vector<std::pair<double, double>>& Fought, int Battles, double Months, uint64_t Seed,
		const std::function<std::string(double X, double Y)>& GroundWord, const OldLines& Lines)
	{
		if (Battles <= 0 || Fought.empty() || !GroundWord)
		{
			return 0;
		}
		// The fought-over ground: the middle four fifths of where the men and machines went, each way, 300 m at least.
		std::vector<double> Xs, Ys;
		Xs.reserve(Fought.size());
		Ys.reserve(Fought.size());
		for (const auto& P : Fought)
		{
			if (P.first == P.first && P.second == P.second)
			{
				Xs.push_back(P.first);
				Ys.push_back(P.second);
			}
		}
		if (Xs.empty())
		{
			return 0;
		}
		std::sort(Xs.begin(), Xs.end());
		std::sort(Ys.begin(), Ys.end());
		const auto At = [](const std::vector<double>& V, double Q) { return V[std::min(V.size() - 1, static_cast<size_t>(Q * static_cast<double>(V.size() - 1) + 0.5))]; };
		double X0 = At(Xs, 0.1), X1 = At(Xs, 0.9), Y0 = At(Ys, 0.1), Y1 = At(Ys, 0.9);
		const auto Widen = [](double& A, double& B) { const double Mid = 0.5 * (A + B), Half = std::max(150.0, 0.5 * (B - A)); A = Mid - Half; B = Mid + Half; };
		Widen(X0, X1);
		Widen(Y0, Y1);
		const int Wanted = std::min(Field::Most / 6, 150 * Battles);
		Dice D(Mix(Seed, 0x5CA25ull));
		const auto InBox = [&](double X, double Y) { return X >= X0 && X <= X1 && Y >= Y0 && Y <= Y1; };

		// The old lines: the pieces of hedge, wall and lane over the fought-over ground, 4 m long at least.
		struct Piece
		{
			double AX, AY, BX, BY, Long, Ux, Uy;
		};
		std::vector<Piece> Pieces;
		for (const std::vector<std::pair<double, double>>& Line : Lines.Lines)
		{
			for (size_t K = 0; K + 1 < Line.size(); ++K)
			{
				const double AX = Line[K].first, AY = Line[K].second, BX = Line[K + 1].first, BY = Line[K + 1].second;
				const double Long = std::hypot(BX - AX, BY - AY);
				if (Long >= 4.0 && Long < 1e5 && InBox(0.5 * (AX + BX), 0.5 * (AY + BY)))
				{
					Pieces.push_back({ AX, AY, BX, BY, Long, (BX - AX) / Long, (BY - AY) / Long });
				}
			}
		}
		std::vector<std::pair<double, double>> Spots;
		for (const std::pair<double, double>& P : Lines.Spots)
		{
			if (InBox(P.first, P.second)) Spots.push_back(P);
		}
		// (a few fronts, more the more battles were fought here: each the pieces lying in one heading within 30 m of a
		// first piece's line, the shells falling mostly on one side of it, the side held)
		struct Front
		{
			std::vector<size_t> Of;
			double Long = 0.0;
			double Side = 1.0;
		};
		std::vector<Front> Fronts;
		if (!Pieces.empty())
		{
			double Total = 0.0;
			for (const Piece& Pc : Pieces) Total += Pc.Long;
			const int Want = std::min(5, 2 + Battles);
			for (int F = 0; F < Want * 3 && static_cast<int>(Fronts.size()) < Want; ++F)
			{
				double Pick = D.U() * Total;
				size_t First = 0;
				while (First + 1 < Pieces.size() && Pick > Pieces[First].Long) Pick -= Pieces[First++].Long;
				const Piece& A = Pieces[First];
				Front Fr;
				for (size_t K = 0; K < Pieces.size(); ++K)
				{
					const Piece& B = Pieces[K];
					const double Mx = 0.5 * (B.AX + B.BX) - A.AX, My = 0.5 * (B.AY + B.BY) - A.AY;
					if (std::fabs(A.Ux * B.Ux + A.Uy * B.Uy) >= 0.85 && std::fabs(Mx * A.Uy - My * A.Ux) <= 30.0)
					{
						Fr.Of.push_back(K);
						Fr.Long += B.Long;
					}
				}
				Fr.Side = D.U() < 0.5 ? 1.0 : -1.0;
				// (a front of one short hedge is no line to find: 60 m of it at least)
				if (Fr.Long >= 60.0) Fronts.push_back(std::move(Fr));
			}
		}

		int Laid = 0;
		// (clusters: a barrage's dozen, a mortar's few; tried a while longer than wanted, since open ground is not all)
		for (int Tries = 0; Laid < Wanted && Tries < Wanted * 6; )
		{
			double CX = X0 + (X1 - X0) * D.U(), CY = Y0 + (Y1 - Y0) * D.U();
			const bool bBarrage = D.U() < 0.45;
			const int InIt = bBarrage ? 8 + static_cast<int>(D.U() * 9.0) : 2 + static_cast<int>(D.U() * 4.0);
			double Spread = bBarrage ? 35.0 : 12.0;
			// (along a front: the cluster's middle a few metres off a piece of it, the barrage walked along it, not across)
			double Ux = 1.0, Uy = 0.0, Along = 1.0, Across = 1.0;
			const double Where = D.U();
			if (!Fronts.empty() && Where < 0.68)
			{
				const Front& Fr = Fronts[std::min(Fronts.size() - 1, static_cast<size_t>(D.U() * static_cast<double>(Fronts.size())))];
				double Pick = D.U() * Fr.Long;
				size_t K = 0;
				while (K + 1 < Fr.Of.size() && Pick > Pieces[Fr.Of[K]].Long) Pick -= Pieces[Fr.Of[K++]].Long;
				const Piece& Pc = Pieces[Fr.Of[K]];
				const double T = std::clamp(Pick / Pc.Long, 0.0, 1.0);
				const double Held = D.U() < 0.8 ? Fr.Side : -Fr.Side;
				const double Off = Held * D.U(3.0, 18.0);
				CX = Pc.AX + (Pc.BX - Pc.AX) * T - Pc.Uy * Off;
				CY = Pc.AY + (Pc.BY - Pc.AY) * T + Pc.Ux * Off;
				Ux = Pc.Ux;
				Uy = Pc.Uy;
				Along = 1.2;
				Across = 0.3;
			}
			else if (!Spots.empty() && Where < (Fronts.empty() ? 0.6 : 0.88))
			{
				// (at a junction or the village's edge: a tighter knot)
				const std::pair<double, double>& P = Spots[std::min(Spots.size() - 1, static_cast<size_t>(D.U() * static_cast<double>(Spots.size())))];
				const double A = D.U(0.0, 2.0 * Pi), Rr = D.U(0.0, 15.0);
				CX = P.first + std::cos(A) * Rr;
				CY = P.second + std::sin(A) * Rr;
				Spread *= 0.6;
			}
			// (which battle before: the last the most, an earlier one's holes older by some months each)
			const int Before = Battles > 1 ? static_cast<int>(D.U() * D.U() * Battles) : 0;
			const double Older = Months + Before * (2.0 + 4.0 * D.U());
			for (int K = 0; K < InIt && Laid < Wanted; ++K, ++Tries)
			{
				// (about the cluster's middle, thicker at it: the sum of two throws each way, drawn out along a front)
				const double Sa = Spread * Along * (D.U() + D.U() - 1.0);
				const double Sc = Spread * Across * (D.U() + D.U() - 1.0);
				const double X = CX + Ux * Sa - Uy * Sc, Y = CY + Uy * Sa + Ux * Sc;
				const double Kind = D.U();
				const double Age = std::max(0.0, Older + D.U() * 0.5);
				const std::string Ground = GroundWord(X, Y);
				if (Ground.empty() || Ground == "water")
				{
					continue;
				}
				const char* Name = Kind < 0.4 ? "mortar" : Kind < 0.6 ? "gun75" : Kind < 0.9 ? "field" : "heavy";
				if (Holes.AddOld(X, Y, Name, Kind < 0.6 && Kind >= 0.4 ? 75.0 : 0.0, Ground, Age))
				{
					++Laid;
				}
			}
		}
		return Laid;
	}
}
