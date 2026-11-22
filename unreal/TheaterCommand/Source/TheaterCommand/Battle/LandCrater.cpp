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

		/** The rings round a hole's middle: four inside its crest, four out over its thrown soil (Lay's Rings). */
		constexpr int HoleRings = 8;
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
		const ColourFn& Under)
	{
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
		static const RingOf Rings[] = { { false, 0.55 }, { false, 0.72 }, { false, 0.86 }, { false, 1.0 }, { true, 0.12 }, { true, 0.32 }, { true, 0.6 }, { true, 1.0 } };
		constexpr int NRings = static_cast<int>(sizeof(Rings) / sizeof(Rings[0]));
		static_assert(NRings == HoleRings, "Field::TopAt reads the rings as laid here");
		const double Base = 0.03 + std::max(0.0, Lift); // (the floor over the drawn ground: nothing of it may sink under the ground's own triangles)
		const double Ground0 = Height(X, Y);
		const uint32_t Middle = Put(Out, X, Y, Ground0 + Base, Toned(Paint.Hollow, D.U(0.82, 0.92)));
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
					if (Rg.T <= 0.56)
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
				Foot[K] = Put(Out, Cx + std::cos(B) * Rr, Cy + std::sin(B) * Rr, Gz + Base * 0.5 - 0.01, Side);
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
		const Size S = SizeOf(Name, CalibreMm);
		if (!(S.Across > 0.05))
		{
			return false;
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
		Lay(X, Y, S, SoilOf(Ground), Own, Height, 0.004 * (Number % 7), Into, Under);
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
		for (const int K : Found->second)
		{
			const Hole& H = Holes[static_cast<size_t>(K)];
			const double Dx = X - H.X, Dy = Y - H.Y;
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
		}
		if (bAny)
		{
			OutZ = Best;
		}
		return bAny;
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
		const double Apart = std::sqrt((LX - RX) * (LX - RX) + (LY - RY) * (LY - RY));
		const Under Mid = Kind(0.5 * (MX + PX), 0.5 * (MY + PY));
		Place In;
		Patch& P = PatchFor(0.5 * (MX + PX), 0.5 * (MY + PY), In);
		LandField::Mesh& Out = P.Sections[static_cast<size_t>(In.Section)];
		const double Own = 0.001 * (V % 5); // (each tank her own millimetre, so two crossing never flicker)
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
