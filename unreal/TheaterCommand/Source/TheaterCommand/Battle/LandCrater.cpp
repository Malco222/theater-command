#include "Battle/LandCrater.h"

#include <algorithm>
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
		return P;
	}

	void Lay(double X, double Y, const Size& S, const Soil& Paint, uint64_t Seed, const HeightFn& Height, double Lift, LandField::Mesh& Out,
		const ColourFn& Under, std::vector<double>* OutCut)
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
		}
		{
			// (the crest's height ragged, but never a spike: each side with a share of its neighbours')
			const std::vector<double> Was = RimK;
			for (int K = 0; K < N; ++K)
			{
				RimK[static_cast<size_t>(K)] = 0.5 * Was[static_cast<size_t>(K)] + 0.25 * (Was[static_cast<size_t>((K + 1) % N)] + Was[static_cast<size_t>((K + N - 1) % N)]);
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
		const auto BowlAt = [Deep](double T) { const double K = T / 0.78; return -Deep * std::max(0.0, 1.0 - K * K); };
		const double Base = 0.03 + std::max(0.0, Lift); // (the floor over the drawn ground: nothing of it may sink under the ground's own triangles)
		const double Ground0 = Height(X, Y);
		const uint32_t Middle = Deep > 0.0
			? Put(Out, X, Y, Ground0 + Base - Deep * D.U(0.96, 1.04), Mixed(Toned(Paint.Hollow, D.U(0.82, 0.92)), Soot, D.U(0.3, 0.45)))
			: Put(Out, X, Y, Ground0 + Base, Toned(Paint.Hollow, D.U(0.82, 0.92)));
		std::vector<uint32_t> Index(static_cast<size_t>(NRings * N));
		for (int Ring = 0; Ring < NRings; ++Ring)
		{
			const RingOf& Rg = Rings[Ring];
			for (int K = 0; K < N; ++K)
			{
				const size_t Kk = static_cast<size_t>(K);
				const bool bRim = Rg.bOut && Rg.T >= 1.0; // (the thrown soil's thin edge over the grass: the most ragged)
				const double A = 2.0 * Pi * (K + (bRim ? D.U(-0.35, 0.35) : D.U(-0.12, 0.12))) / N;
				const double Rc = R * Crest[Kk];
				double Radius = 0.0, Over = 0.0;
				Rgb8 Colour, Unturfed;
				double TurfShare = -1.0;
				if (!Rg.bOut)
				{
					Radius = Rc * Rg.T * D.U(0.96, 1.04);
					if (Deep > 0.0 && Rg.T < 0.8)
					{
						// the bowl: its floor sooty at the middle where the burst was, damp dark earth, the wall's foot
						// lightening toward the dry soil it rises to
						Over = BowlAt(Rg.T) * D.U(0.92, 1.06) + D.U(-0.02, 0.02) * Deep;
						Colour = Rg.T < 0.4 ? Mixed(Paint.Hollow, Soot, D.U(0.15, 0.3)) : Rg.T < 0.6 ? Mixed(Paint.Hollow, Paint.Wall, 0.18) : Mixed(Paint.Hollow, Paint.Wall, 0.55);
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
				}
				else
				{
					// the thrown soil, falling away from the crest to a thin ragged edge over the grass
					Radius = Rc * (1.0 + Rg.T * (Reach[Kk] - 1.0)) * (bRim ? D.U(0.78, 1.12) : D.U(0.96, 1.04));
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
					else if (Deep > 0.0 && Rg.T < 0.4 && Patch < 0.62) Colour = Mixed(Colour, Soot, D.U(0.15, 0.35)); // (blast-blackened soil thrown with it)
				}
				const double Px = X + std::cos(A) * Radius, Py = Y + std::sin(A) * Radius;
				const double Edge = Rg.bOut && Rg.T >= 1.0 ? 0.6 : 1.0; // (the thin edge a little lower: it lies on the grass)
				if (Rg.bOut && Under && TurfShare >= 0.0)
				{
					// (turf torn from this ground: its own colour, a little darker for the soil on it)
					Colour = Mixed(Unturfed, Mixed(Paint.Turf, Under(Px, Py), 0.7), TurfShare);
				}
				Rgb8 Painted = Toned(Colour, Tone[Kk] * D.U(0.92, 1.08));
				if (Rg.bOut && Under)
				{
					// The thrown soil fades into the ground it lies on: its thin edge is the ground's own colour (no
					// outline, whatever was torn), the ring inside it leans that way.
					const Rgb8 Ground = Under(Px, Py);
					Painted = bRim ? Ground : Rg.T >= 0.6 ? Mixed(Painted, Ground, 0.4) : Painted;
				}
				Index[static_cast<size_t>(Ring * N + K)] = Put(Out, Px, Py, Height(Px, Py) + Base * Edge + Over, Painted);
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
				A = RayOf[static_cast<size_t>(D.Whole(0, Rays - 1))].first + D.U(-0.2, 0.2);
			}
			const double Far = R * (1.08 + std::pow(D.U(), 1.4) * (ReachOf - 1.0) * 1.35);
			const double Cx = X + std::cos(A) * Far, Cy = Y + std::sin(A) * Far;
			const double Big = S.Clod * D.U(0.35, 1.0) * (Far < 1.5 * R ? 1.0 : 0.75);
			const bool bTurf = D.U() < 0.3;
			const double Gz = Height(Cx, Cy);
			const Rgb8 Side = Toned(Paint.Clod, D.U(0.85, 1.1));
			const Rgb8 Turf = Under ? Mixed(Paint.Turf, Under(Cx, Cy), 0.7) : Paint.Turf;
			const Rgb8 Top = bTurf ? Toned(Turf, D.U(0.85, 1.1)) : Toned(Mixed(Paint.Clod, Paint.Crest, 0.4), D.U(0.9, 1.1));
			const uint32_t Peak = Put(Out, Cx + D.U(-0.2, 0.2) * Big, Cy + D.U(-0.2, 0.2) * Big, Gz + Base + Big * (bTurf ? D.U(0.22, 0.36) : D.U(0.32, 0.55)), Top); // (a low lump, never a cone)
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
		Normals(Out, FirstPoint, FirstIndex);
	}

	void Field::Begin(HeightFn InHeight, uint64_t InSeed, ColourFn InUnder)
	{
		Height = std::move(InHeight);
		Under = std::move(InUnder);
		Seed = InSeed ? InSeed : 1;
		Holes.clear();
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

	bool Field::Add(double X, double Y, const std::string& Name, double CalibreMm, const std::string& Ground)
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
		if (S.Deep > 0.0 && Dig)
		{
			// (dug only in open ground, its middle and all round its crest: a lane's floor, a bank or a yard keeps the
			// shallow form laid over it)
			bool bOpen = Dig(X, Y);
			for (int K = 0; K < 8 && bOpen; ++K)
			{
				const double A = Pi * K / 4.0;
				bOpen = Dig(X + std::cos(A) * 0.55 * S.Across, Y + std::sin(A) * 0.55 * S.Across);
			}
			if (!bOpen)
			{
				S.Deep = 0.0;
			}
		}
		const int Number = static_cast<int>(Holes.size());
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
		const uint64_t Own = Mix(Mix(Seed, static_cast<uint64_t>(Number) + 1u), static_cast<uint64_t>(std::llround(X * 100.0)) * 2654435761ull ^ static_cast<uint64_t>(std::llround(Y * 100.0)));
		// (each hole its own few millimetres over the ground, so two laid over each other never flicker)
		LandField::Mesh& Into = Own_.Sections[static_cast<size_t>(In.Section)];
		Hole H;
		H.First = static_cast<uint32_t>(Into.Colours.size());
		const size_t FirstIndex = Into.Triangles.size();
		// (laid on what is there now: the floor of an older hole where it is torn in one)
		const HeightFn Surface = [this](double Px, double Py) { return SurfaceAt(Px, Py); };
		std::vector<double> CutLine;
		Lay(X, Y, S, SoilOf(Ground == "earth" && !Earth.empty() ? Earth : Ground), Own, Surface, 0.004 * (Number % 7), Into, Under, S.Deep > 0.0 ? &CutLine : nullptr);
		H.Deep = CutLine.empty() ? 0.0 : S.Deep;
		H.X = X;
		H.Y = Y;
		H.R = 0.5 * S.Across;
		H.Rim = S.Rim;
		H.In = In;
		H.Sides = std::clamp(S.Sides, 8, 48);
		H.Clods = std::max(0, S.Clods);
		// (how far it reaches: its farthest point, the thrown soil's ragged edge or a clod's foot)
		for (size_t P = H.First; P < Into.Colours.size(); ++P)
		{
			const double Dx = Into.Positions[P * 3] - X, Dy = Into.Positions[P * 3 + 1] - Y;
			H.Out = std::max(H.Out, std::sqrt(Dx * Dx + Dy * Dy));
		}
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
		Holes.push_back(H);
		const double Bare_ = std::max(1.2 * H.R, H.Out);
		constexpr double Square = 8.0;
		for (int J = static_cast<int>(std::floor((Y - Bare_) / Square)); J <= static_cast<int>(std::floor((Y + Bare_) / Square)); ++J)
		{
			for (int I = static_cast<int>(std::floor((X - Bare_) / Square)); I <= static_cast<int>(std::floor((X + Bare_) / Square)); ++I)
			{
				Squares[SquareOf(I, J)].push_back(Number);
			}
		}
		const bool bListed = std::any_of(Changed.begin(), Changed.end(), [&In](const Place& P) { return P.Patch == In.Patch && P.Section == In.Section; });
		if (!bListed)
		{
			Changed.push_back(In);
		}
		return true;
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
			const double Bare_ = 1.2 * H.R;
			if ((X - H.X) * (X - H.X) + (Y - H.Y) * (Y - H.Y) < Bare_ * Bare_)
			{
				return true;
			}
		}
		return false;
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
			const uint32_t I = Put(M, Q.P[0], Q.P[1], Q.P[2], Rgb(static_cast<int>(std::lround(Q.C[0])), static_cast<int>(std::lround(Q.C[1])), static_cast<int>(std::lround(Q.C[2]))));
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

	void Ruts::Strip(LandField::Mesh& Out, double AX, double AY, double BX, double BY, double Half, double Edge, const Rgb8& Paint, double Lift) const
	{
		const double Dx = BX - AX, Dy = BY - AY;
		const double Long = std::sqrt(Dx * Dx + Dy * Dy);
		if (!(Long > 1e-6))
		{
			return;
		}
		// (across her way, left to right: the faded edge, the pressed strip, the faded edge; laid counter-clockwise)
		const double Nx = -Dy / Long, Ny = Dx / Long;
		const double Off[4] = { Half + Edge, Half, -Half, -(Half + Edge) };
		const size_t FirstPoint = Out.Colours.size(), FirstIndex = Out.Triangles.size();
		uint32_t A[4], B[4];
		for (int K = 0; K < 4; ++K)
		{
			const bool bEdge = K == 0 || K == 3;
			for (int End = 0; End < 2; ++End)
			{
				const double Px = (End ? BX : AX) + Nx * Off[K], Py = (End ? BY : AY) + Ny * Off[K];
				const Rgb8 Ground = UnderColour ? UnderColour(Px, Py) : Paint;
				(End ? B : A)[K] = Put(Out, Px, Py, Height(Px, Py) + Lift, bEdge ? Ground : Paint);
			}
		}
		for (int K = 0; K < 3; ++K)
		{
			Tri(Out, A[K], A[K + 1], B[K + 1]);
			Tri(Out, A[K], B[K + 1], B[K]);
		}
		Normals(Out, FirstPoint, FirstIndex);
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
		// In standing corn, the corn between her tracks (and under them) laid flat: straw pressed down, a little paler
		// and greyer than the standing ears.
		if (Mid == Under::Corn && Apart > Width)
		{
			const Rgb8 Laid_ = Mixed(UnderColour ? UnderColour(MX, MY) : Rgb(150, 136, 96), Rgb(150, 136, 98), 0.45);
			Strip(Out, PX, PY, MX, MY, 0.5 * Apart, 0.35, Laid_, 0.012 + Own);
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
			const Rgb8 Paint = Here == Under::Grass ? Mixed(Ground, Rgb(84, 78, 50), 0.5)
				: Here == Under::Corn ? Mixed(Ground, Rgb(108, 92, 64), 0.6)
				: Mixed(Ground, Rgb(70, 58, 43), 0.55);
			Strip(Out, AX, AY, BX, BY, 0.42 * Width, 0.22 * Width, Paint, 0.02 + Own);
		}
		Piece Pc;
		Pc.AX = PX;
		Pc.AY = PY;
		Pc.BX = MX;
		Pc.BY = MY;
		Pc.Apart = Apart;
		Pc.Width = Width;
		Pc.bCorn = Mid == Under::Corn;
		const int Number = static_cast<int>(Laid.size());
		Laid.push_back(Pc);
		const double Reach = 0.5 * Apart + Width;
		constexpr double Square = 8.0;
		const int I0 = static_cast<int>(std::floor((std::min(PX, MX) - Reach) / Square)), I1 = static_cast<int>(std::floor((std::max(PX, MX) + Reach) / Square));
		const int J0 = static_cast<int>(std::floor((std::min(PY, MY) - Reach) / Square)), J1 = static_cast<int>(std::floor((std::max(PY, MY) + Reach) / Square));
		for (int J = J0; J <= J1; ++J)
		{
			for (int I = I0; I <= I1; ++I)
			{
				Squares[SquareOf(I, J)].push_back(Number);
			}
		}
		++Count;
		const bool bListed = std::any_of(Changed.begin(), Changed.end(), [&In](const Place& C) { return C.Patch == In.Patch && C.Section == In.Section; });
		if (!bListed)
		{
			Changed.push_back(In);
		}
		Mark();
		return true;
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
			const double Across = std::fabs(((X - Pc.AX) * Dy - (Y - Pc.AY) * Dx) / std::sqrt(Long2));
			if (Pc.bCorn ? Across <= 0.5 * Pc.Apart + 0.5 * Pc.Width : std::fabs(Across - 0.5 * Pc.Apart) <= 0.5 * Pc.Width)
			{
				return true;
			}
		}
		return false;
	}
}
