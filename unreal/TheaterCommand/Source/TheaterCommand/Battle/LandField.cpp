#include "Battle/LandField.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace LandField
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		/** The band round the field laid in full (bocage, banks, trees), so a camera at the field's edge sees no change. */
		constexpr double Band = 250.0;
		/** The ring beyond the band, out to this far past the field's edges. */
		constexpr double RingOut = 5000.0;
		/** The region raster's pixel (parcels, lanes, distances): fine enough that a hedge's line is good to a metre. */
		constexpr double Res = 2.0;
		/** A sunken lane's wall: the run from the floor's edge up to the field. */
		constexpr double LaneWall = 1.2;
		/** The tiles: 250 m, the near ground 60 quads a side (4.2 m) in the field and 30 (8.3 m) in the band, the far 15 (16.7 m). */
		constexpr double TileSize = 250.0;
		constexpr int NearQuads = 60, BandQuads = 30, FarQuads = 15;
		/** A way wider than this either side (a vehicle) opens the hedges it crosses; a man's only parts their shrubs. */
		constexpr double VehicleHalf = 1.2;

		// ------------------------------------------------------------------ numbers

		uint64_t Mix(uint64_t H)
		{
			H ^= H >> 33;
			H *= 0xFF51AFD7ED558CCDull;
			H ^= H >> 33;
			H *= 0xC4CEB9FE1A85EC53ull;
			H ^= H >> 33;
			return H;
		}
		uint64_t Hash(uint64_t A, uint64_t B)
		{
			return Mix(A * 0x9E3779B97F4A7C15ull ^ Mix(B + 0x632BE59BD9B4E019ull));
		}
		uint64_t Hash(uint64_t A, uint64_t B, uint64_t C)
		{
			return Hash(Hash(A, B), C);
		}
		double Unit(uint64_t H)
		{
			return static_cast<double>(H >> 11) / 9007199254740992.0;
		}

		/** A stream of numbers from a seed: the same ground every time for the same field. */
		struct Rng
		{
			uint64_t State;
			explicit Rng(uint64_t Seed) : State(Mix(Seed + 0x51ED2701ull)) {}
			double Next()
			{
				State += 0x9E3779B97F4A7C15ull;
				return Unit(Mix(State));
			}
			double Range(double Lo, double Hi) { return Lo + (Hi - Lo) * Next(); }
			int Below(int N) { return std::min(N - 1, static_cast<int>(Next() * N)); }
		};

		double Clamp01(double X) { return std::clamp(X, 0.0, 1.0); }
		double Smooth(double E0, double E1, double X)
		{
			const double T = Clamp01((X - E0) / (E1 - E0));
			return T * T * (3.0 - 2.0 * T);
		}
		double Lerp(double A, double B, double T) { return A + (B - A) * T; }
		double Sq(double X) { return X * X; }

		/** A lattice point's value, -1 to 1: one mix of its place and the seed (the noise is most of the build's time). */
		double Lattice(int64_t I, int64_t J, uint64_t Seed)
		{
			return Unit(Mix(static_cast<uint64_t>(I) * 0x9E3779B97F4A7C15ull + static_cast<uint64_t>(J) * 0xC2B2AE3D27D4EB4Full + Seed * 0x165667B19E3779F9ull)) * 2.0 - 1.0;
		}
		double Fade(double T) { return T * T * T * (T * (T * 6.0 - 15.0) + 10.0); }

		/** Smooth value noise, -1 to 1, its lattice one unit apart. */
		double Noise(double X, double Y, uint64_t Seed)
		{
			const double FX = std::floor(X), FY = std::floor(Y);
			const int64_t I = static_cast<int64_t>(FX), J = static_cast<int64_t>(FY);
			const double U = Fade(X - FX), V = Fade(Y - FY);
			const double A = Lattice(I, J, Seed), B = Lattice(I + 1, J, Seed), C = Lattice(I, J + 1, Seed), D = Lattice(I + 1, J + 1, Seed);
			return Lerp(Lerp(A, B, U), Lerp(C, D, U), V);
		}

		/** Fractal noise, about -1 to 1 (mostly within a half): octaves of Noise, each half the size and weight. */
		double Fbm(double X, double Y, double Scale, int Octaves, uint64_t Seed)
		{
			double Sum = 0.0, Weight = 1.0, Total = 0.0, F = 1.0 / Scale;
			for (int O = 0; O < Octaves; ++O)
			{
				Sum += Weight * Noise(X * F + O * 17.31, Y * F - O * 9.13, Seed + static_cast<uint64_t>(O) * 101u);
				Total += Weight;
				Weight *= 0.5;
				F *= 2.03;
			}
			return Sum / Total;
		}

		double Lattice3(int64_t I, int64_t J, int64_t K, uint64_t Seed)
		{
			return Unit(Mix(static_cast<uint64_t>(I) * 0x9E3779B97F4A7C15ull + static_cast<uint64_t>(J) * 0xC2B2AE3D27D4EB4Full + static_cast<uint64_t>(K) * 0xD6E8FEB86659FD93ull + Seed * 0x165667B19E3779F9ull)) * 2.0 - 1.0;
		}
		/** The same in three dimensions (the shrubs' and crowns' lumps). */
		double Noise3(double X, double Y, double Z, uint64_t Seed)
		{
			const double FX = std::floor(X), FY = std::floor(Y), FZ = std::floor(Z);
			const int64_t I = static_cast<int64_t>(FX), J = static_cast<int64_t>(FY), K = static_cast<int64_t>(FZ);
			const double U = Fade(X - FX), V = Fade(Y - FY), W = Fade(Z - FZ);
			double C[2][2][2];
			for (int A = 0; A < 2; ++A)
				for (int B = 0; B < 2; ++B)
					for (int D = 0; D < 2; ++D) C[A][B][D] = Lattice3(I + A, J + B, K + D, Seed);
			const double X00 = Lerp(C[0][0][0], C[1][0][0], U), X10 = Lerp(C[0][1][0], C[1][1][0], U);
			const double X01 = Lerp(C[0][0][1], C[1][0][1], U), X11 = Lerp(C[0][1][1], C[1][1][1], U);
			return Lerp(Lerp(X00, X10, V), Lerp(X01, X11, V), W);
		}

		// ------------------------------------------------------------------ geometry

		struct V2
		{
			double X = 0.0, Y = 0.0;
		};
		V2 operator+(V2 A, V2 B) { return { A.X + B.X, A.Y + B.Y }; }
		V2 operator-(V2 A, V2 B) { return { A.X - B.X, A.Y - B.Y }; }
		V2 operator*(V2 A, double S) { return { A.X * S, A.Y * S }; }
		double Dot(V2 A, V2 B) { return A.X * B.X + A.Y * B.Y; }
		double Len(V2 A) { return std::sqrt(Dot(A, A)); }
		V2 Unit2(V2 A)
		{
			const double L = Len(A);
			return L > 1e-12 ? A * (1.0 / L) : V2{ 1.0, 0.0 };
		}
		V2 Perp(V2 A) { return { -A.Y, A.X }; }
		V2 Lerp2(V2 A, V2 B, double T) { return { A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T }; }

		struct V3
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
		};
		V3 operator+(V3 A, V3 B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
		V3 operator-(V3 A, V3 B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
		V3 operator*(V3 A, double S) { return { A.X * S, A.Y * S, A.Z * S }; }
		double Dot3(V3 A, V3 B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		V3 Cross(V3 A, V3 B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		V3 Unit3(V3 A, V3 Else = { 0.0, 0.0, 1.0 })
		{
			const double L = std::sqrt(Dot3(A, A));
			return L > 1e-12 ? A * (1.0 / L) : Else;
		}

		using Line = std::vector<V2>;

		/** Distance from P to the segment AB, and where along it (0 to 1) the nearest point is. */
		double SegDist(V2 P, V2 A, V2 B, double& T)
		{
			const V2 D = B - A;
			const double LL = Dot(D, D);
			T = LL > 1e-12 ? Clamp01(Dot(P - A, D) / LL) : 0.0;
			return Len(P - (A + D * T));
		}

		double LineLength(const Line& L)
		{
			double Sum = 0.0;
			for (size_t K = 1; K < L.size(); ++K) Sum += Len(L[K] - L[K - 1]);
			return Sum;
		}

		/** The line through points about Step apart, its ends kept. */
		Line Resample(const Line& L, double Step)
		{
			Line Out;
			if (L.size() < 2) return L;
			const double Total = LineLength(L);
			const int N = std::max(1, static_cast<int>(std::lround(Total / Step)));
			const double Each = Total / N;
			Out.push_back(L.front());
			size_t Seg = 1;
			double Before = 0.0, SegLen = Len(L[1] - L[0]);
			for (int K = 1; K < N; ++K)
			{
				const double Want = K * Each;
				while (Seg + 1 < L.size() && Before + SegLen < Want)
				{
					Before += SegLen;
					++Seg;
					SegLen = Len(L[Seg] - L[Seg - 1]);
				}
				const double T = SegLen > 1e-12 ? Clamp01((Want - Before) / SegLen) : 0.0;
				Out.push_back(Lerp2(L[Seg - 1], L[Seg], T));
			}
			Out.push_back(L.back());
			return Out;
		}

		/** Corners cut a quarter each side (Chaikin), the ends kept: a staircase becomes a gentle curve. */
		Line Chaikin(const Line& L, int Times)
		{
			Line Cur = L;
			for (int Pass = 0; Pass < Times && Cur.size() > 2; ++Pass)
			{
				Line Next;
				Next.push_back(Cur.front());
				for (size_t K = 0; K + 1 < Cur.size(); ++K)
				{
					const V2 A = Cur[K], B = Cur[K + 1];
					if (K > 0) Next.push_back(Lerp2(A, B, 0.25));
					if (K + 2 < Cur.size()) Next.push_back(Lerp2(A, B, 0.75));
				}
				Next.push_back(Cur.back());
				Cur.swap(Next);
			}
			return Cur;
		}

		/** Douglas-Peucker: the fewest points that keep the line within Tol. */
		void SimplifyInto(const Line& L, size_t A, size_t B, double Tol, std::vector<char>& Keep)
		{
			if (B <= A + 1) return;
			double Worst = -1.0;
			size_t At = A;
			for (size_t K = A + 1; K < B; ++K)
			{
				double T;
				const double D = SegDist(L[K], L[A], L[B], T);
				if (D > Worst)
				{
					Worst = D;
					At = K;
				}
			}
			if (Worst > Tol)
			{
				Keep[At] = 1;
				SimplifyInto(L, A, At, Tol, Keep);
				SimplifyInto(L, At, B, Tol, Keep);
			}
		}
		Line Simplify(const Line& L, double Tol)
		{
			if (L.size() < 3) return L;
			std::vector<char> Keep(L.size(), 0);
			Keep.front() = Keep.back() = 1;
			SimplifyInto(L, 0, L.size() - 1, Tol, Keep);
			Line Out;
			for (size_t K = 0; K < L.size(); ++K)
				if (Keep[K]) Out.push_back(L[K]);
			return Out;
		}

		/** The line moved sideways by D (left of its way, + to the left), each point along the mean of its sides' normals. */
		Line Offset(const Line& L, double D)
		{
			Line Out(L.size());
			for (size_t K = 0; K < L.size(); ++K)
			{
				const V2 Before = K > 0 ? Unit2(L[K] - L[K - 1]) : Unit2(L[1] - L[0]);
				const V2 After = K + 1 < L.size() ? Unit2(L[K + 1] - L[K]) : Before;
				const V2 N = Unit2(Perp(Unit2(Before + After)));
				const double Cos = std::max(0.5, Dot(N, Perp(Before)));
				Out[K] = L[K] + N * (D / Cos);
			}
			return Out;
		}

		/** Whether segments AB and CD cross, and where along AB (0 to 1). */
		bool Crosses(V2 A, V2 B, V2 C, V2 D, double& T)
		{
			const V2 R = B - A, S = D - C;
			const double Den = R.X * S.Y - R.Y * S.X;
			if (std::fabs(Den) < 1e-12) return false;
			const V2 Q = C - A;
			T = (Q.X * S.Y - Q.Y * S.X) / Den;
			const double U = (Q.X * R.Y - Q.Y * R.X) / Den;
			return T >= 0.0 && T <= 1.0 && U >= 0.0 && U <= 1.0;
		}

		/** Segments filed by square: each segment in every square its box, padded by Pad, touches, so one square answers a point. */
		struct SegGrid
		{
			double X0 = 0.0, Y0 = 0.0, Size = 10.0;
			int NX = 0, NY = 0;
			std::vector<std::vector<uint32_t>> Cells;

			void Init(double InX0, double InY0, double InX1, double InY1, double InSize)
			{
				X0 = InX0;
				Y0 = InY0;
				Size = InSize;
				NX = std::max(1, static_cast<int>(std::ceil((InX1 - InX0) / Size)));
				NY = std::max(1, static_cast<int>(std::ceil((InY1 - InY0) / Size)));
				Cells.assign(static_cast<size_t>(NX) * NY, {});
			}
			void Add(uint32_t Id, V2 A, V2 B, double Pad)
			{
				const int I0 = std::clamp(static_cast<int>(std::floor((std::min(A.X, B.X) - Pad - X0) / Size)), 0, NX - 1);
				const int I1 = std::clamp(static_cast<int>(std::floor((std::max(A.X, B.X) + Pad - X0) / Size)), 0, NX - 1);
				const int J0 = std::clamp(static_cast<int>(std::floor((std::min(A.Y, B.Y) - Pad - Y0) / Size)), 0, NY - 1);
				const int J1 = std::clamp(static_cast<int>(std::floor((std::max(A.Y, B.Y) + Pad - Y0) / Size)), 0, NY - 1);
				for (int J = J0; J <= J1; ++J)
					for (int I = I0; I <= I1; ++I) Cells[static_cast<size_t>(J) * NX + I].push_back(Id);
			}
			const std::vector<uint32_t>* At(double X, double Y) const
			{
				if (Cells.empty()) return nullptr;
				const int I = static_cast<int>(std::floor((X - X0) / Size)), J = static_cast<int>(std::floor((Y - Y0) / Size));
				if (I < 0 || J < 0 || I >= NX || J >= NY) return nullptr;
				return &Cells[static_cast<size_t>(J) * NX + I];
			}
		};

		// ------------------------------------------------------------------ colour

		/** A colour while it is mixed: sRGB 0 to 255 (the painting is done in the picture's own values, as the art director measures). */
		struct Col
		{
			double R = 0.0, G = 0.0, B = 0.0;
		};
		Col Rgb(double R, double G, double B) { return { R, G, B }; }
		Col Mixed(const Col& A, const Col& B, double T)
		{
			T = Clamp01(T);
			return { Lerp(A.R, B.R, T), Lerp(A.G, B.G, T), Lerp(A.B, B.B, T) };
		}
		Col Scaled(const Col& A, double K) { return { A.R * K, A.G * K, A.B * K }; }
		/** Lighter or darker by L, warmer (+) or cooler by W: patches and grain. */
		Col Varied(const Col& A, double L, double W) { return { A.R * L * (1.0 + W), A.G * L, A.B * L * (1.0 - W) }; }
		Rgb8 Byte(const Col& C)
		{
			Rgb8 Out;
			Out.R = static_cast<uint8_t>(std::clamp(std::lround(C.R), 0L, 255L));
			Out.G = static_cast<uint8_t>(std::clamp(std::lround(C.G), 0L, 255L));
			Out.B = static_cast<uint8_t>(std::clamp(std::lround(C.B), 0L, 255L));
			return Out;
		}

		// ------------------------------------------------------------------ the ground's kinds

		/** What a pixel of the region raster is. Seeded: beyond the field, its kind its parcel's seed's. */
		enum Rg : uint8_t
		{
			RgOpen,
			RgField,
			RgWoods,
			RgRough,
			RgWorks,
			RgVillage,
			RgMarsh,
			RgWater,
			RgLane,
			RgYard,
			RgSeeded,
			RgCount
		};
		bool IsFarmland(int K) { return K == RgOpen || K == RgField || K == RgRough || K == RgWorks; }

		enum HedgeKind
		{
			HkHedgerow = 0,
			HkLaneBank = 1,
			HkWoodEdge = 2,
			HkGarden = 3,
			HkWall = 4,
			HkStrip = 5
		};

		/** What a parcel grows (its colour and its clutter). */
		enum Crop
		{
			CpGrazed,
			CpLush,
			CpHayCut,
			CpMeadow,
			CpWheat,
			CpBarley,
			CpOats,
			CpGreenWheat,
			CpStubble,
			CpRoots,
			CpRough,
			CpWorks,
			CpGarden,
			CpYard,
			CpWoods,
			CpOther
		};

		struct Lane
		{
			Line P;              // the middle line, points about 2 m apart
			double Half = 2.2;   // half the floor's width
		};
		struct LaneSeg
		{
			V2 A, B;
			int Lane = 0;
		};

		/** A bank's line as laid (points about 2 m apart, fewer where it runs straight), the bank's height and foot width at each. */
		struct Run
		{
			Line P;
			std::vector<float> H, W;
			int Kind = HkHedgerow;
			uint64_t Key = 0;
		};
		struct BankSeg
		{
			uint32_t Run = 0, K = 0;
		};
		struct WallRun
		{
			Line P;
			std::vector<float> H;
			double Thick = 0.55;
			uint64_t Key = 0;
		};

		struct ParcelInfo
		{
			int Kind = RgOpen;
			double Area = 0.0, CX = 0.0, CY = 0.0, Angle = 0.0, Lands = 15.0;
			int Crop = CpGrazed;
			int Seed = -1;
			bool bOrchard = false;
			uint64_t Key = 0;
		};

		enum HouseType
		{
			HtHouse,
			HtFarmhouse,
			HtBarn,
			HtByre,
			HtChurch
		};
		enum Damage
		{
			DmNone,
			DmRoofHole,
			DmGable,
			DmShell
		};
		struct House
		{
			V2 C;
			double Yaw = 0.0, Long = 10.0, Deep = 6.5;
			int Storeys = 1;
			int Roof = 0; // 0 slate, 1 tile
			int Type = HtHouse;
			int Hurt = DmNone;
			int Front = 1; // the side of the long axis its door faces (+1: its left, +V)
			int Part = 0;  // a church's: 0 nave, 1 chancel, 2 tower
			uint64_t Key = 0;
			double Floor = 0.0, Foot = 0.0;
		};

		/** A farmstead's yard: its frame (U along the lane, V away from it) and its box in that frame. */
		struct YardBox
		{
			V2 O, U, V;
			double U0 = 0.0, U1 = 0.0, V0 = 0.0, V1 = 0.0;
		};

		/** A pixel of the occupancy raster: what stands on it (bits). */
		enum Occ : uint8_t
		{
			OcHouse = 1,   // inside a building's footprint
			OcNear = 2,    // within a metre or two of one (no shrub, no tree)
			OcWall = 4,    // on a stone wall's line
			OcMan = 8,      // a man's way: no tree trunk
			OcVehicle = 16, // a vehicle's way: nothing standing
			OcGate = 32     // a field gateway: the ground trodden
		};
	}

	struct Ground::Impl
	{
		Field Fd;
		double FW = 0.0, FH = 0.0;
		uint64_t Seed = 1;
		double MeanZ = 0.0;
		bool bDesert = false;
		std::vector<uint8_t> CellRg;   // each cell's region (a road cell: what lies round it)
		std::vector<uint8_t> CellRoad; // road or bridge
		std::vector<uint8_t> FxOfCell; // the effects word's number per ground index

		// The region raster over the field and its band (Res metres a pixel).
		double RX0 = 0.0, RY0 = 0.0;
		int NX = 0, NY = 0;
		std::vector<uint8_t> Rgn;
		std::vector<int32_t> Pid;
		std::vector<float> DistE; // metres to the parcel's edge
		std::vector<float> LaneD; // metres outside the nearest lane's floor (negative on it)
		std::vector<uint8_t> OccR;

		std::vector<ParcelInfo> Parcels;
		// The plots the parcels are cut from (one lattice of blocks over the field, the band and the ring, the same
		// everywhere): the frame's turn.
		double Theta0 = 0.0;
		std::vector<YardBox> Yards;
		std::vector<V2> OrchardAt;

		std::vector<Lane> Lanes;
		std::vector<LaneSeg> LSegs;
		SegGrid LGrid;
		std::vector<Run> Runs;
		std::vector<BankSeg> BSegs;
		SegGrid BGrid;
		std::vector<WallRun> Walls;
		std::vector<House> Houses;
		std::vector<V2> WayA, WayB;
		std::vector<double> WayHalf;
		SegGrid WGrid;
		struct Spot
		{
			V2 P;
			double Yaw = 0.0;
			int What = 0; // 0 well, 1 cart, 2 rubble stones
		};
		std::vector<Spot> Props;

		std::vector<HedgeLine> HedgeOut;
		std::vector<std::vector<std::pair<double, double>>> LaneOut;
		std::vector<std::vector<Placed>> Inst;
		std::vector<BaseMesh> Bases;
		std::vector<Tile> TileList, RingList;
		Tally Count;

		explicit Impl(const Field& F);

		// heights
		double Cells(double X, double Y) const;
		double Rolling(double X, double Y) const;
		double Outside(double X, double Y) const;
		double Land(double X, double Y) const;
		double Ridges(double X, double Y) const;
		double Terrain(double X, double Y) const { return Land(X, Y) + Ridges(X, Y); }
		double Sunk(double X, double Y) const;
		double Carve(double X, double Y, double Z, double* OutEdge) const;
		double BankAt(double X, double Y) const;
		double Top(double X, double Y, Drawn* What) const;
		int HouseAt(double X, double Y) const;

		// raster lookups
		bool InRaster(double X, double Y) const { return X >= RX0 && Y >= RY0 && X < RX0 + NX * Res && Y < RY0 + NY * Res; }
		size_t Pix(double X, double Y) const
		{
			const int I = std::clamp(static_cast<int>((X - RX0) / Res), 0, NX - 1), J = std::clamp(static_cast<int>((Y - RY0) / Res), 0, NY - 1);
			return static_cast<size_t>(J) * NX + I;
		}
		double Bilinear(const std::vector<float>& R, double X, double Y) const;
		int CellAt(double X, double Y) const;
		uint8_t SmoothCellRegion(double X, double Y) const;
		bool WoodsBeyond(double X, double Y) const;
		V2 PlotFrame(double X, double Y) const;
		int Plot(double X, double Y, double* OutGap, uint8_t* OutKind) const;
		int CropOf(int SeedIndex, int Kind) const;
		void MakePlots();
		/** The tiles across the field and its band (the last row and column may reach a little past the band). */
		int TilesX() const { return static_cast<int>(std::ceil((FW + 2.0 * Band) / TileSize - 1e-6)); }
		int TilesY() const { return static_cast<int>(std::ceil((FH + 2.0 * Band) / TileSize - 1e-6)); }
		double Canopy(double X, double Y) const;

		// building
		void ReadCells();
		void MakeLanes();
		void MakeRaster();
		void MakeWays();
		void MakeSites();
		bool Fits(const House& H, double Margin) const;
		void Stamp(const House& H, double Margin);
		void FloorOf(House& H) const;
		void AddWall(const Line& L, double High, double Thick, uint64_t Key);
		void MakeParcels();
		void MakeDistances();
		void MakeHedges();
		void MakePlants();
		void Plant(Base B, double X, double Y, double Yaw, double Scale, double ScaleZ, Rgb8 Tint);

		// meshes
		Col GroundColour(double X, double Y) const;
		Col FarColour(double X, double Y) const;
		void MakeTiles();
		void MakeRing();
		void MakeBases();
	};
}

namespace LandField
{
	namespace
	{
		const char* const FxWords[] = { "earth", "town", "woods", "water", "mud", "sand", "snow" };

		/** Catmull-Rom weights for the four samples round T (0 to 1 between the middle two). */
		void CatmullRom(double T, double W[4])
		{
			const double T2 = T * T, T3 = T2 * T;
			W[0] = 0.5 * (-T3 + 2.0 * T2 - T);
			W[1] = 0.5 * (3.0 * T3 - 5.0 * T2 + 2.0);
			W[2] = 0.5 * (-3.0 * T3 + 4.0 * T2 + T);
			W[3] = 0.5 * (T3 - T2);
		}

		/** A sunken lane's floor across: a crown in the middle, two wheel ruts, a little kerb at the walls' feet. */
		double Rut(double D, double Half)
		{
			const double Ruts = 0.075 * std::exp(-Sq((D - 0.8) / 0.22));
			const double Crown = 0.04 * std::exp(-Sq(D / 0.4));
			const double Kerb = 0.07 * Smooth(Half - 0.5, Half, D);
			return Crown + Kerb - Ruts;
		}

		/** The lane's wall across, 0 at the floor's edge to 1 at the field: steep in its middle (the same in the mesh and in At). */
		const double WallKnot[4] = { 0.0, 0.35, 0.7, 1.0 };
		const double WallRise[4] = { 0.0, 0.22, 0.78, 1.0 };
		double WallShape(double T)
		{
			if (T <= 0.0) return 0.0;
			if (T >= 1.0) return 1.0;
			for (int K = 1; K < 4; ++K)
				if (T <= WallKnot[K]) return Lerp(WallRise[K - 1], WallRise[K], (T - WallKnot[K - 1]) / (WallKnot[K] - WallKnot[K - 1]));
			return 1.0;
		}
		/**
		 * A bank across, T from its middle (0) out to its foot (1, half its foot width out) and on to BankEdge, where the
		 * mesh's edge has sunk BankSink under the ground (so no crack shows where it meets the ground): its rise above the
		 * ground for a bank H high, flat on top, straight between the points the mesh lays, so At stands a man exactly on
		 * what is drawn. Below the ground it is under the ground's mesh, and At takes the ground there.
		 */
		constexpr double BankEdge = 1.15, BankSink = 0.15;
		/** (a Norman bank: a flat top some 1.1 to 1.4 m across for the hedge, steep faces of earth and grass, a spread foot) */
		constexpr double BankTop = 0.45, BankFace = 0.85, BankKnee = 0.35;
		const double BankAcross[6] = { -BankEdge, -BankFace, -BankTop, BankTop, BankFace, BankEdge };
		double BankProfile(double T, double H)
		{
			T = std::fabs(T);
			if (T <= BankTop) return H;
			if (T <= BankFace) return H * Lerp(1.0, BankKnee, (T - BankTop) / (BankFace - BankTop));
			const double U = Clamp01((T - BankFace) / (BankEdge - BankFace));
			return H * BankKnee * (1.0 - U) - BankSink * U;
		}
	}

	// ====================================================================== heights

	double Ground::Impl::Cells(double X, double Y) const
	{
		const double U = X / Fd.Cell - 0.5, V = Y / Fd.Cell - 0.5;
		const double FU = std::floor(U), FV = std::floor(V);
		const int I = static_cast<int>(FU), J = static_cast<int>(FV);
		double WX[4], WY[4];
		CatmullRom(U - FU, WX);
		CatmullRom(V - FV, WY);
		double Z = 0.0;
		for (int B = 0; B < 4; ++B)
		{
			const int Row = std::clamp(J - 1 + B, 0, Fd.Rows - 1);
			double Sum = 0.0;
			for (int A = 0; A < 4; ++A)
			{
				const int Column = std::clamp(I - 1 + A, 0, Fd.Cols - 1);
				Sum += WX[A] * Fd.Height[static_cast<size_t>(Row) * Fd.Cols + Column];
			}
			Z += WY[B] * Sum;
		}
		return Z;
	}

	double Ground::Impl::Outside(double X, double Y) const
	{
		const double DX = std::max({ 0.0, -X, X - FW }), DY = std::max({ 0.0, -Y, Y - FH });
		return std::sqrt(DX * DX + DY * DY);
	}

	/** Beyond the field: the country rolls in long swells of a few metres, as the bocage round Saint-Lo does. */
	double Ground::Impl::Rolling(double X, double Y) const
	{
		return MeanZ + 18.0 * Fbm(X, Y, 2600.0, 3, Seed ^ 0xB1) + 5.0 * Fbm(X, Y, 700.0, 2, Seed ^ 0xB2);
	}

	/**
	 * The land without lanes, banks or ploughing: the cells' heights smoothed, and an undulation of 0.2 to 0.6 m (no
	 * field is a table). Beyond the field it turns over a kilometre into the rolling country, and falls away with the
	 * earth's curve (2 m at 5 km), so the far edge sinks under the haze.
	 */
	double Ground::Impl::Land(double X, double Y) const
	{
		double Z = Cells(X, Y);
		const double Amp = 0.25 + 0.4 * (0.5 + 0.5 * Noise(X / 420.0, Y / 420.0, Seed ^ 0xA1));
		Z += Amp * (0.75 * Fbm(X, Y, 80.0, 2, Seed ^ 0xA2) + 0.35 * Noise(X / 24.0, Y / 24.0, Seed ^ 0xA3));
		const double D = Outside(X, Y);
		if (D > 0.0)
		{
			Z = Lerp(Z, Rolling(X, Y), Smooth(0.0, 1200.0, D));
			Z -= D * D / 12.74e6;
		}
		return Z;
	}

	/**
	 * A corn field's ploughed lands: ridges 13 to 19 m apart, 7 cm high, along the parcel's length, flat on its
	 * headlands where the plough turned (4 to 10 m in from its edge). Seen at a low sun they band the corn.
	 */
	double Ground::Impl::Ridges(double X, double Y) const
	{
		if (Parcels.empty() || DistE.empty() || !InRaster(X, Y)) return 0.0;
		const int P = Pid[Pix(X, Y)];
		if (P < 0) return 0.0;
		const ParcelInfo& Q = Parcels[static_cast<size_t>(P)];
		if (Q.Kind != RgField) return 0.0;
		const double Across = -(X - Q.CX) * std::sin(Q.Angle) + (Y - Q.CY) * std::cos(Q.Angle);
		return 0.07 * std::cos(2.0 * Pi * Across / Q.Lands) * Smooth(4.0, 10.0, Bilinear(DistE, X, Y));
	}

	/** How deep a lane runs below the fields: 0.6 to 1.4 m in the bocage, little in the village, none out past the band. */
	double Ground::Impl::Sunk(double X, double Y) const
	{
		double D = 0.6 + 0.8 * (0.5 + 0.5 * Noise(X / 150.0, Y / 150.0, Seed ^ 0xC1));
		if (Fd.bTown) D *= 0.15 + 0.85 * Smooth(Fd.TownR * 0.55, Fd.TownR * 1.1, std::hypot(X - Fd.TownX, Y - Fd.TownY));
		D *= 1.0 - Smooth(Band - 80.0, Band - 15.0, Outside(X, Y));
		return D;
	}

	/**
	 * The ground Z with the lanes cut into it: each lane's floor level across (its middle line's land less how deep it
	 * is sunk), wheel-rutted, then a steep wall up to the field over LaneWall metres. Where lanes meet, the lowest
	 * wins. OutEdge (if asked): metres outside the nearest lane's floor (negative on it).
	 */
	double Ground::Impl::Carve(double X, double Y, double Z, double* OutEdge) const
	{
		const std::vector<uint32_t>* Near = LGrid.At(X, Y);
		double Out = Z, Edge = 1e6;
		if (Near)
		{
			const V2 P{ X, Y };
			for (const uint32_t Id : *Near)
			{
				const LaneSeg& S = LSegs[Id];
				const double Half = Lanes[static_cast<size_t>(S.Lane)].Half;
				double T;
				const double D = SegDist(P, S.A, S.B, T);
				Edge = std::min(Edge, D - Half);
				if (D >= Half + LaneWall) continue;
				const V2 C = Lerp2(S.A, S.B, T);
				const double Floor = Land(C.X, C.Y) - Sunk(C.X, C.Y);
				const double Zl = D <= Half ? Floor + Rut(D, Half) : Lerp(Floor + Rut(Half, Half), Z, WallShape((D - Half) / LaneWall));
				Out = std::min(Out, Zl);
			}
		}
		if (OutEdge) *OutEdge = Edge;
		return Out;
	}

	/** The highest bank's rise above the ground here (0 off the banks). */
	double Ground::Impl::BankAt(double X, double Y) const
	{
		const std::vector<uint32_t>* Near = BGrid.At(X, Y);
		if (!Near) return 0.0;
		double Best = 0.0;
		const V2 P{ X, Y };
		for (const uint32_t Id : *Near)
		{
			const BankSeg& S = BSegs[Id];
			const Run& R = Runs[S.Run];
			double T;
			const double D = SegDist(P, R.P[S.K], R.P[S.K + 1], T);
			const double W = 0.5 * Lerp(R.W[S.K], R.W[S.K + 1], T);
			if (D >= W * BankEdge) continue;
			Best = std::max(Best, BankProfile(D / W, Lerp(R.H[S.K], R.H[S.K + 1], T)));
		}
		return Best;
	}

	int Ground::Impl::HouseAt(double X, double Y) const
	{
		if (!InRaster(X, Y) || !(OccR[Pix(X, Y)] & OcHouse)) return -1;
		for (size_t K = 0; K < Houses.size(); ++K)
		{
			const House& H = Houses[K];
			const V2 D{ X - H.C.X, Y - H.C.Y };
			const double U = D.X * std::cos(H.Yaw) + D.Y * std::sin(H.Yaw), V = -D.X * std::sin(H.Yaw) + D.Y * std::cos(H.Yaw);
			if (std::fabs(U) <= 0.5 * H.Long && std::fabs(V) <= 0.5 * H.Deep) return static_cast<int>(K);
		}
		return -1;
	}

	/** The surface drawn: the land, the lanes cut in, the banks on top; a house's floor inside it. */
	double Ground::Impl::Top(double X, double Y, Drawn* What) const
	{
		double Z = Terrain(X, Y);
		double Edge = 1e6;
		if (!InRaster(X, Y) || LaneD[Pix(X, Y)] < LaneWall + 3.0) Z = Carve(X, Y, Z, &Edge);
		const double B = BankAt(X, Y);
		Z += B;
		const int H = HouseAt(X, Y);
		if (H >= 0) Z = Houses[static_cast<size_t>(H)].Floor;
		if (What)
		{
			if (H >= 0) *What = Drawn::Floor;
			else if (B > 0.05) *What = Drawn::Bank;
			else if (Edge < LaneWall) *What = Drawn::Lane;
			else if (!InRaster(X, Y)) *What = Drawn::Beyond;
			else
			{
				const size_t P = Pix(X, Y);
				if (OccR[P] & OcWall) *What = Drawn::Wall;
				else
				{
					switch (Rgn[P])
					{
					case RgField: *What = Drawn::Crop; break;
					case RgWoods: *What = Drawn::Woods; break;
					case RgRough: *What = Drawn::Rough; break;
					case RgWorks: *What = Drawn::Works; break;
					case RgVillage: *What = Drawn::Village; break;
					case RgYard: *What = Drawn::Yard; break;
					case RgWater: *What = Drawn::Water; break;
					case RgMarsh: *What = Drawn::Marsh; break;
					case RgLane: *What = Drawn::Lane; break;
					default:
						*What = Pid[P] >= 0 && Parcels[static_cast<size_t>(Pid[P])].Kind == RgField ? Drawn::Crop : Drawn::Pasture;
						break;
					}
				}
			}
		}
		return Z;
	}

	double Ground::Impl::Bilinear(const std::vector<float>& R, double X, double Y) const
	{
		const double U = (X - RX0) / Res - 0.5, V = (Y - RY0) / Res - 0.5;
		const int I = std::clamp(static_cast<int>(std::floor(U)), 0, NX - 2), J = std::clamp(static_cast<int>(std::floor(V)), 0, NY - 2);
		const double A = Clamp01(U - I), B = Clamp01(V - J);
		const size_t K = static_cast<size_t>(J) * NX + I;
		return Lerp(Lerp(R[K], R[K + 1], A), Lerp(R[K + NX], R[K + NX + 1], A), B);
	}

	int Ground::Impl::CellAt(double X, double Y) const
	{
		const int I = std::clamp(static_cast<int>(std::floor(X / Fd.Cell)), 0, Fd.Cols - 1), J = std::clamp(static_cast<int>(std::floor(Y / Fd.Cell)), 0, Fd.Rows - 1);
		return J * Fd.Cols + I;
	}

	// ====================================================================== the rules' cells

	void Ground::Impl::ReadCells()
	{
		const size_t N = static_cast<size_t>(Fd.Cols) * Fd.Rows;
		Fd.Ground.resize(N, 0);
		Fd.Height.resize(N, 0.0);
		if (!Fd.Storeys.empty()) Fd.Storeys.resize(N, 0);
		bDesert = Fd.Terrain == "desert";
		std::vector<uint8_t> RgOf(Fd.Grounds.size(), RgOpen), RoadOf(Fd.Grounds.size(), 0);
		FxOfCell.assign(Fd.Grounds.size(), 0);
		for (size_t K = 0; K < Fd.Grounds.size(); ++K)
		{
			const std::string& G = Fd.Grounds[K];
			uint8_t R = RgOpen, Fx = bDesert ? 5 : 0;
			if (G == "field") R = RgField;
			else if (G == "woods" || G == "forest")
			{
				R = RgWoods;
				Fx = 2;
			}
			else if (G == "rough" || G == "rock") R = RgRough;
			else if (G == "marsh")
			{
				R = RgMarsh;
				Fx = 4;
			}
			else if (G == "water" || G == "blown")
			{
				R = RgWater;
				Fx = 3;
			}
			else if (G == "road") RoadOf[K] = 1;
			else if (G == "bridge")
			{
				RoadOf[K] = 1;
				Fx = 1;
			}
			else if (G == "buildings" || G == "suburb" || G == "industry" || G == "park")
			{
				R = RgVillage;
				Fx = 1;
			}
			else if (G == "works" || G == "bunker") R = RgWorks;
			if ((G == "rock" || G == "bunker") && bDesert) Fx = 0;
			RgOf[K] = R;
			FxOfCell[K] = Fx;
		}
		CellRg.assign(N, RgOpen);
		CellRoad.assign(N, 0);
		double Sum = 0.0;
		for (size_t K = 0; K < N; ++K)
		{
			const int G = Fd.Ground[K];
			const bool bKnown = G >= 0 && static_cast<size_t>(G) < RgOf.size();
			CellRg[K] = bKnown ? RgOf[static_cast<size_t>(G)] : static_cast<uint8_t>(RgOpen);
			CellRoad[K] = bKnown ? RoadOf[static_cast<size_t>(G)] : 0;
			Sum += Fd.Height[K];
		}
		MeanZ = N ? Sum / static_cast<double>(N) : 0.0;
		// A road cell is a lane through whatever lies round it: its region is its neighbours' most common.
		for (int J = 0; J < Fd.Rows; ++J)
			for (int I = 0; I < Fd.Cols; ++I)
			{
				const size_t K = static_cast<size_t>(J) * Fd.Cols + I;
				if (!CellRoad[K]) continue;
				int Votes[RgCount] = {};
				for (int DJ = -1; DJ <= 1; ++DJ)
					for (int DI = -1; DI <= 1; ++DI)
					{
						const int A = I + DI, B = J + DJ;
						if (A < 0 || B < 0 || A >= Fd.Cols || B >= Fd.Rows) continue;
						const size_t Q = static_cast<size_t>(B) * Fd.Cols + A;
						if (!CellRoad[Q]) ++Votes[CellRg[Q]];
					}
				int Best = RgOpen;
				for (int R = 0; R < RgCount; ++R)
					if (Votes[R] > Votes[Best]) Best = R;
				CellRg[K] = static_cast<uint8_t>(Best == RgWater ? RgOpen : Best);
			}
	}

	// ====================================================================== the lanes

	/**
	 * The rules' road cells thinned to lines one cell wide, traced into lines between their ends and meetings, the
	 * staircases smoothed into gentle bends and made to wander a little, as Norman lanes do. A lane that meets the
	 * field's edge runs on through the band.
	 */
	void Ground::Impl::MakeLanes()
	{
		const int C = Fd.Cols, R = Fd.Rows;
		std::vector<uint8_t> M(CellRoad);
		auto Get = [&M, C, R](int I, int J) -> int { return (I >= 0 && J >= 0 && I < C && J < R) ? M[static_cast<size_t>(J) * C + I] : 0; };
		// Zhang and Suen's thinning.
		for (bool bChanged = true; bChanged;)
		{
			bChanged = false;
			for (int Sub = 0; Sub < 2; ++Sub)
			{
				std::vector<size_t> Gone;
				for (int J = 0; J < R; ++J)
					for (int I = 0; I < C; ++I)
					{
						if (!M[static_cast<size_t>(J) * C + I]) continue;
						const int P2 = Get(I, J + 1), P3 = Get(I + 1, J + 1), P4 = Get(I + 1, J), P5 = Get(I + 1, J - 1);
						const int P6 = Get(I, J - 1), P7 = Get(I - 1, J - 1), P8 = Get(I - 1, J), P9 = Get(I - 1, J + 1);
						const int B = P2 + P3 + P4 + P5 + P6 + P7 + P8 + P9;
						const int Seq[9] = { P2, P3, P4, P5, P6, P7, P8, P9, P2 };
						int A = 0;
						for (int K = 0; K < 8; ++K)
							if (!Seq[K] && Seq[K + 1]) ++A;
						if (B < 2 || B > 6 || A != 1) continue;
						const bool bGo = Sub == 0 ? (P2 * P4 * P6 == 0 && P4 * P6 * P8 == 0) : (P2 * P4 * P8 == 0 && P2 * P6 * P8 == 0);
						if (bGo) Gone.push_back(static_cast<size_t>(J) * C + I);
					}
				for (const size_t K : Gone) M[K] = 0;
				if (!Gone.empty()) bChanged = true;
			}
		}
		// Neighbours: side by side, or corner to corner where no side cell joins them already.
		auto Linked = [&Get](int I, int J, int DI, int DJ) -> bool {
			if (!Get(I + DI, J + DJ)) return false;
			if (DI == 0 || DJ == 0) return true;
			return !Get(I + DI, J) && !Get(I, J + DJ);
		};
		static const int Dirs[8][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } };
		auto Degree = [&Linked](int I, int J) {
			int D = 0;
			for (const auto& Dd : Dirs)
				if (Linked(I, J, Dd[0], Dd[1])) ++D;
			return D;
		};
		std::unordered_set<int64_t> Used;
		const int64_t NC = static_cast<int64_t>(C) * R;
		auto Key = [NC](int A, int B) { return static_cast<int64_t>(std::min(A, B)) * NC + std::max(A, B); };
		std::vector<std::vector<int>> Paths;
		auto Walk = [&](int Start, int Next) {
			std::vector<int> Path{ Start, Next };
			Used.insert(Key(Start, Next));
			int Prev = Start, Cur = Next;
			while (Degree(Cur % C, Cur / C) == 2)
			{
				int Found = -1;
				for (const auto& Dd : Dirs)
				{
					if (!Linked(Cur % C, Cur / C, Dd[0], Dd[1])) continue;
					const int Q = (Cur / C + Dd[1]) * C + Cur % C + Dd[0];
					if (Q != Prev)
					{
						Found = Q;
						break;
					}
				}
				if (Found < 0 || Used.count(Key(Cur, Found))) break;
				Used.insert(Key(Cur, Found));
				Path.push_back(Found);
				Prev = Cur;
				Cur = Found;
			}
			Paths.push_back(Path);
		};
		for (int Pass = 0; Pass < 2; ++Pass)
			for (int J = 0; J < R; ++J)
				for (int I = 0; I < C; ++I)
				{
					if (!Get(I, J)) continue;
					const int D = Degree(I, J);
					if ((Pass == 0) == (D == 2)) continue; // ends and meetings first, then what is left (rings)
					for (const auto& Dd : Dirs)
					{
						if (!Linked(I, J, Dd[0], Dd[1])) continue;
						const int P = J * C + I, Q = (J + Dd[1]) * C + I + Dd[0];
						if (!Used.count(Key(P, Q))) Walk(P, Q);
					}
				}
		auto OnBorder = [C, R](int P) {
			const int I = P % C, J = P / C;
			return I == 0 || J == 0 || I == C - 1 || J == R - 1;
		};
		int Index = 0;
		for (const std::vector<int>& Path : Paths)
		{
			// A spur of a cell or two off a lane is the thinning's crumb, not a lane.
			const bool bLooseA = Degree(Path.front() % C, Path.front() / C) == 1 && !OnBorder(Path.front());
			const bool bLooseB = Degree(Path.back() % C, Path.back() / C) == 1 && !OnBorder(Path.back());
			if (Path.size() < 4 && (bLooseA || bLooseB)) continue;
			Line L;
			for (const int P : Path) L.push_back({ (P % C + 0.5) * Fd.Cell, (P / C + 0.5) * Fd.Cell });
			// Out through the band: a lane that reaches the field's edge goes on beyond it.
			for (int End = 0; End < 2; ++End)
			{
				const int P = End == 0 ? Path.front() : Path.back();
				if (!OnBorder(P)) continue;
				const int I = P % C, J = P / C;
				const V2 Out = I == 0 ? V2{ -1, 0 } : I == C - 1 ? V2{ 1, 0 } : J == 0 ? V2{ 0, -1 } : V2{ 0, 1 };
				const V2 At = End == 0 ? L.front() : L.back();
				const double Side = 30.0 * Noise(At.X / 100.0, At.Y / 100.0, Seed ^ 0xE5);
				const V2 Edge = At + Out * (0.5 * Fd.Cell);
				const V2 Far = Edge + Out * (Band + 40.0) + Perp(Out) * Side;
				if (End == 0)
				{
					L.insert(L.begin(), Edge);
					L.insert(L.begin(), Far);
				}
				else
				{
					L.push_back(Edge);
					L.push_back(Far);
				}
			}
			L = Resample(Chaikin(L, 3), 2.0);
			// Wander: long slow bends of up to a dozen metres either way over some 250 m (within the rules' road cells
			// and the ones beside them), and a few metres over some 80 m; none at the ends (they meet other lanes), and
			// little in the village, whose houses front the lane.
			const double Total = LineLength(L);
			Line W(L.size());
			double S = 0.0;
			for (size_t K = 0; K < L.size(); ++K)
			{
				if (K > 0) S += Len(L[K] - L[K - 1]);
				const V2 Way = Unit2(L[std::min(K + 1, L.size() - 1)] - L[K > 0 ? K - 1 : 0]);
				const double Amp = 2.5 * std::min({ 1.0, S / 40.0, (Total - S) / 40.0 });
				double Bend = 12.0 * Smooth(0.0, 1.0, std::min({ 1.0, S / 110.0, (Total - S) / 110.0 }));
				if (Fd.bTown) Bend *= Smooth(Fd.TownR * 0.9, Fd.TownR * 1.5, std::hypot(L[K].X - Fd.TownX, L[K].Y - Fd.TownY));
				W[K] = L[K] + Perp(Way) * (Amp * Noise(S / 80.0, Index * 7.31, Seed ^ 0xE1) + Bend * Noise(S / 260.0, Index * 3.77 + 0.5, Seed ^ 0xE2));
			}
			Lane Ln;
			Ln.P = Resample(W, 2.0);
			bool bVillage = false;
			for (const V2& Q : Ln.P)
				if (Fd.bTown && std::hypot(Q.X - Fd.TownX, Q.Y - Fd.TownY) < Fd.TownR) bVillage = true;
			Ln.Half = bVillage ? 2.6 : 2.0 + 0.4 * Unit(Hash(Seed, 0x1A4E, static_cast<uint64_t>(Index)));
			Lanes.push_back(std::move(Ln));
			++Index;
		}
		LGrid.Init(-Band - 60.0, -Band - 60.0, FW + Band + 60.0, FH + Band + 60.0, 10.0);
		for (size_t L = 0; L < Lanes.size(); ++L)
		{
			const Lane& Ln = Lanes[L];
			Count.LaneKm += LineLength(Ln.P) / 1000.0;
			std::vector<std::pair<double, double>> Out;
			for (size_t K = 0; K < Ln.P.size(); ++K)
			{
				Out.push_back({ Ln.P[K].X, Ln.P[K].Y });
				if (K + 1 == Ln.P.size()) break;
				LaneSeg S;
				S.A = Ln.P[K];
				S.B = Ln.P[K + 1];
				S.Lane = static_cast<int>(L);
				LGrid.Add(static_cast<uint32_t>(LSegs.size()), S.A, S.B, Ln.Half + LaneWall + 3.5);
				LSegs.push_back(S);
			}
			LaneOut.push_back(std::move(Out));
		}
		Count.Lanes = static_cast<int>(Lanes.size());
	}

	// ====================================================================== the region raster

	/** The rules' kind at a place, smoothed: the cells' kinds weighed round it, so their edges run in curves and not steps. */
	uint8_t Ground::Impl::SmoothCellRegion(double X, double Y) const
	{
		{
			// Where every cell within reach of the warp is of one kind, that is the kind (most of the field: no noise asked).
			const int CI = static_cast<int>(std::floor(X / Fd.Cell)), CJ = static_cast<int>(std::floor(Y / Fd.Cell));
			const uint8_t First = CellRg[static_cast<size_t>(std::clamp(CJ, 0, Fd.Rows - 1)) * Fd.Cols + std::clamp(CI, 0, Fd.Cols - 1)];
			bool bSame = true;
			for (int DJ = -1; DJ <= 1 && bSame; ++DJ)
				for (int DI = -1; DI <= 1 && bSame; ++DI)
					bSame = CellRg[static_cast<size_t>(std::clamp(CJ + DJ, 0, Fd.Rows - 1)) * Fd.Cols + std::clamp(CI + DI, 0, Fd.Cols - 1)] == First;
			if (bSame) return First;
		}
		const double WX = X + 6.0 * Noise(X / 40.0, Y / 40.0, Seed ^ 0xD1), WY = Y + 6.0 * Noise(X / 40.0, Y / 40.0, Seed ^ 0xD2);
		const double U = WX / Fd.Cell - 0.5, V = WY / Fd.Cell - 0.5;
		const double FU = std::floor(U), FV = std::floor(V);
		const int I = static_cast<int>(FU), J = static_cast<int>(FV);
		const double A = U - FU, B = V - FV;
		double W[RgCount] = {};
		for (int DJ = 0; DJ < 2; ++DJ)
			for (int DI = 0; DI < 2; ++DI)
			{
				const int Column = std::clamp(I + DI, 0, Fd.Cols - 1), Row = std::clamp(J + DJ, 0, Fd.Rows - 1);
				W[CellRg[static_cast<size_t>(Row) * Fd.Cols + Column]] += (DI ? A : 1.0 - A) * (DJ ? B : 1.0 - B);
			}
		int Best = 0;
		for (int K = 1; K < RgCount; ++K)
			if (W[K] > W[Best]) Best = K;
		return static_cast<uint8_t>(Best);
	}

	/** Woods out beyond the field: some one ground in six, in blocks of a few hundred metres. */
	bool Ground::Impl::WoodsBeyond(double X, double Y) const
	{
		return Fbm(X, Y, 520.0, 3, Seed ^ 0xD5) > 0.27;
	}

	void Ground::Impl::MakeRaster()
	{
		RX0 = -Band;
		RY0 = -Band;
		NX = static_cast<int>(std::lround((FW + 2.0 * Band) / Res));
		NY = static_cast<int>(std::lround((FH + 2.0 * Band) / Res));
		const size_t N = static_cast<size_t>(NX) * NY;
		Rgn.assign(N, RgOpen);
		LaneD.assign(N, 1e6f);
		OccR.assign(N, 0);
		for (int J = 0; J < NY; ++J)
			for (int I = 0; I < NX; ++I)
			{
				const double X = RX0 + (I + 0.5) * Res, Y = RY0 + (J + 0.5) * Res;
				uint8_t K;
				if (X >= 0.0 && Y >= 0.0 && X <= FW && Y <= FH) K = SmoothCellRegion(X, Y);
				else
				{
					K = RgSeeded;
					const uint8_t Edge = SmoothCellRegion(std::clamp(X, 1.0, FW - 1.0), std::clamp(Y, 1.0, FH - 1.0));
					if (Edge == RgWoods && Outside(X, Y) < 40.0 + 50.0 * (0.5 + 0.5 * Noise(X / 90.0, Y / 90.0, Seed ^ 0xD7))) K = RgWoods;
					else if (WoodsBeyond(X, Y)) K = RgWoods;
				}
				if (Fd.bTown && K != RgWoods && K != RgWater && K != RgMarsh && std::fabs(X - Fd.TownX) < Fd.TownR + 30.0 && std::fabs(Y - Fd.TownY) < Fd.TownR + 30.0)
				{
					const double WX = 14.0 * Noise(X / 60.0, Y / 60.0, Seed ^ 0xD8), WY = 14.0 * Noise(X / 60.0, Y / 60.0, Seed ^ 0xD9);
					if (std::hypot(X + WX - Fd.TownX, Y + WY - Fd.TownY) < Fd.TownR) K = RgVillage;
				}
				Rgn[static_cast<size_t>(J) * NX + I] = K;
			}
		// The lanes' corridors, and how far every pixel near one lies from its floor.
		for (const LaneSeg& S : LSegs)
		{
			const double Half = Lanes[static_cast<size_t>(S.Lane)].Half, Reach = Half + 10.0;
			const int I0 = std::max(0, static_cast<int>((std::min(S.A.X, S.B.X) - Reach - RX0) / Res));
			const int I1 = std::min(NX - 1, static_cast<int>((std::max(S.A.X, S.B.X) + Reach - RX0) / Res));
			const int J0 = std::max(0, static_cast<int>((std::min(S.A.Y, S.B.Y) - Reach - RY0) / Res));
			const int J1 = std::min(NY - 1, static_cast<int>((std::max(S.A.Y, S.B.Y) + Reach - RY0) / Res));
			for (int J = J0; J <= J1; ++J)
				for (int I = I0; I <= I1; ++I)
				{
					double T;
					const double D = SegDist({ RX0 + (I + 0.5) * Res, RY0 + (J + 0.5) * Res }, S.A, S.B, T) - Half;
					float& Cur = LaneD[static_cast<size_t>(J) * NX + I];
					Cur = std::min(Cur, static_cast<float>(D));
				}
		}
		for (size_t K = 0; K < N; ++K)
			if (LaneD[K] < LaneWall + 0.3 && Rgn[K] != RgWater) Rgn[K] = RgLane;
	}

	/** The recording's men and vehicles: their ways filed, and the ground they cover marked (nothing is built on them). */
	void Ground::Impl::MakeWays()
	{
		WGrid.Init(-Band - 60.0, -Band - 60.0, FW + Band + 60.0, FH + Band + 60.0, 10.0);
		for (const Field::Way& W : Fd.Ways)
		{
			const bool bVehicle = W.HalfWidth >= VehicleHalf;
			// (a vehicle's: every pixel any part of which is on it, so no trunk stands in its way)
			const double Clear = W.HalfWidth + (bVehicle ? 0.8 + 0.71 * Res : 0.6);
			for (size_t K = 0; K + 1 < W.Points.size(); ++K)
			{
				const V2 A{ W.Points[K].first, W.Points[K].second }, B{ W.Points[K + 1].first, W.Points[K + 1].second };
				if (Len(B - A) < 1e-6 && K + 2 < W.Points.size()) continue;
				WGrid.Add(static_cast<uint32_t>(WayA.size()), A, B, 2.5);
				WayA.push_back(A);
				WayB.push_back(B);
				WayHalf.push_back(W.HalfWidth);
				const int I0 = std::max(0, static_cast<int>((std::min(A.X, B.X) - Clear - RX0) / Res));
				const int I1 = std::min(NX - 1, static_cast<int>((std::max(A.X, B.X) + Clear - RX0) / Res));
				const int J0 = std::max(0, static_cast<int>((std::min(A.Y, B.Y) - Clear - RY0) / Res));
				const int J1 = std::min(NY - 1, static_cast<int>((std::max(A.Y, B.Y) + Clear - RY0) / Res));
				for (int J = J0; J <= J1; ++J)
					for (int I = I0; I <= I1; ++I)
					{
						double T;
						if (SegDist({ RX0 + (I + 0.5) * Res, RY0 + (J + 0.5) * Res }, A, B, T) < Clear)
							OccR[static_cast<size_t>(J) * NX + I] |= bVehicle ? OcVehicle : OcMan;
					}
			}
		}
	}
}

namespace LandField
{
	// ====================================================================== the plots

	namespace
	{
		/** The plots' blocks: a lattice this far apart (along and across the frame), each corner jittered. */
		constexpr double BlockU = 560.0, BlockV = 500.0, BlockJitter = 0.22;
		/** A plot is cut again while it is larger than a size drawn between these (square metres). */
		constexpr double PlotMin = 17000.0, PlotMax = 34000.0;

		double Cross2(V2 A, V2 B) { return A.X * B.Y - A.Y * B.X; }
		double QuadArea(const V2 Q[4])
		{
			return 0.5 * (Cross2(Q[0], Q[1]) + Cross2(Q[1], Q[2]) + Cross2(Q[2], Q[3]) + Cross2(Q[3], Q[0]));
		}
	}

	/**
	 * The country's grain: aslant to the line of the attack (which comes from the south), some 30 to 60 degrees off it
	 * one way or the other, so the men's advances cross the hedges and do not run inside them for a field's length.
	 */
	void Ground::Impl::MakePlots()
	{
		Theta0 = (Unit(Hash(Seed, 0x7E7A)) < 0.5 ? 0.25 : 0.75) * Pi + (Unit(Hash(Seed, 0x7E7B)) - 0.5) * (Pi / 6.0);
	}

	/**
	 * A place in the plots' frame: bent by a long slow warp of some 40 m over 750 m and a little one of 8 m over 170 m
	 * (so the hedges run in gentle curves, not ruled lines), then turned to the country's grain.
	 */
	V2 Ground::Impl::PlotFrame(double X, double Y) const
	{
		const double WX = 40.0 * Noise(X / 760.0, Y / 760.0, Seed ^ 0x9A01) + 8.0 * Noise(X / 170.0, Y / 170.0, Seed ^ 0x9A02);
		const double WY = 40.0 * Noise(X / 760.0, Y / 760.0, Seed ^ 0x9A03) + 8.0 * Noise(X / 170.0, Y / 170.0, Seed ^ 0x9A04);
		const double QX = X + WX, QY = Y + WY, C = std::cos(Theta0), S = std::sin(Theta0);
		return { QX * C + QY * S, -QX * S + QY * C };
	}

	/**
	 * The plot at a place, as Norman fields were made: the country in blocks of about half a kilometre (a lattice, its
	 * corners jittered), each block cut across its length into two, and each half again, at a third to two thirds
	 * along and a little askew, until the plot is a field of about 1 to 3 hectares. So the fields are four-sided and
	 * irregular, longer one way than the other, their hedges meeting mostly in T's, and bent a little with the warp.
	 * Its number (under a million), how far the place lies inside its edges (OutGap, metres) and the kind of land the
	 * rules give round its middle (OutKind: open or field; beyond the field's reach about one plot in three is corn).
	 */
	int Ground::Impl::Plot(double X, double Y, double* OutGap, uint8_t* OutKind) const
	{
		const V2 P = PlotFrame(X, Y);
		auto Corner = [this](int64_t I, int64_t J) {
			const uint64_t H = Hash(static_cast<uint64_t>(I), static_cast<uint64_t>(J), Seed ^ 0x9A05);
			return V2{ (static_cast<double>(I) + 2.0 * BlockJitter * (Unit(H) - 0.5)) * BlockU, (static_cast<double>(J) + 2.0 * BlockJitter * (Unit(Mix(H)) - 0.5)) * BlockV };
		};
		const int64_t I0 = static_cast<int64_t>(std::floor(P.X / BlockU)), J0 = static_cast<int64_t>(std::floor(P.Y / BlockV));
		V2 Q[4];
		int64_t BI = I0, BJ = J0;
		double BestIn = -1e18;
		for (int DJ = 0; DJ < 3 && BestIn < 0.0; ++DJ)
			for (int DI = 0; DI < 3 && BestIn < 0.0; ++DI)
			{
				// (the block the place's square names first, then its neighbours, until one holds it)
				const int64_t I = I0 + (DI == 0 ? 0 : DI == 1 ? -1 : 1), J = J0 + (DJ == 0 ? 0 : DJ == 1 ? -1 : 1);
				const V2 C[4] = { Corner(I, J), Corner(I + 1, J), Corner(I + 1, J + 1), Corner(I, J + 1) };
				double Inside = 1e18;
				for (int K = 0; K < 4; ++K) Inside = std::min(Inside, Cross2(C[(K + 1) % 4] - C[K], P - C[K]) / std::max(1e-9, Len(C[(K + 1) % 4] - C[K])));
				if (Inside > BestIn)
				{
					BestIn = Inside;
					BI = I;
					BJ = J;
					for (int K = 0; K < 4; ++K) Q[K] = C[K];
				}
			}
		uint64_t Key = Hash(Seed ^ 0x9A10, static_cast<uint64_t>(BI), static_cast<uint64_t>(BJ));
		for (int Depth = 0; Depth < 10; ++Depth)
		{
			if (QuadArea(Q) < Lerp(PlotMin, PlotMax, Unit(Hash(Key, 1)))) break;
			// Across its length: between the longer pair of opposite sides, now and then the other way where it is squat.
			const double L02 = Len(Q[1] - Q[0]) + Len(Q[2] - Q[3]), L13 = Len(Q[2] - Q[1]) + Len(Q[3] - Q[0]);
			bool bTurn = L13 > L02;
			if (std::max(L02, L13) < 1.3 * std::min(L02, L13) && Unit(Hash(Key, 2)) < 0.3) bTurn = !bTurn;
			const V2 R[4] = { Q[bTurn ? 1 : 0], Q[bTurn ? 2 : 1], Q[bTurn ? 3 : 2], Q[bTurn ? 0 : 3] };
			const double A = 0.34 + 0.32 * Unit(Hash(Key, 3));
			const double B = std::clamp(A + 0.36 * (Unit(Hash(Key, 4)) - 0.5), 0.22, 0.78);
			const V2 Cut0 = Lerp2(R[0], R[1], A), Cut1 = Lerp2(R[3], R[2], B);
			const bool bFirst = Cross2(Cut1 - Cut0, P - Cut0) * Cross2(Cut1 - Cut0, R[0] - Cut0) >= 0.0;
			if (bFirst)
			{
				Q[0] = R[0];
				Q[1] = Cut0;
				Q[2] = Cut1;
				Q[3] = R[3];
			}
			else
			{
				Q[0] = Cut0;
				Q[1] = R[1];
				Q[2] = R[2];
				Q[3] = Cut1;
			}
			Key = Hash(Key, bFirst ? 11u : 12u);
		}
		if (OutGap)
		{
			double Gap = 1e18, T;
			for (int K = 0; K < 4; ++K) Gap = std::min(Gap, SegDist(P, Q[K], Q[(K + 1) % 4], T));
			*OutGap = Gap;
		}
		if (OutKind)
		{
			// Its middle, back out of the frame (the warp undone near enough), and the rules' land there.
			const V2 M = (Q[0] + Q[1] + Q[2] + Q[3]) * 0.25;
			const double C = std::cos(Theta0), S = std::sin(Theta0);
			const V2 Bent{ M.X * C - M.Y * S, M.X * S + M.Y * C };
			V2 W = Bent;
			for (int K = 0; K < 2; ++K)
			{
				const V2 F = PlotFrame(W.X, W.Y);
				W = W + (Bent - V2{ F.X * C - F.Y * S, F.X * S + F.Y * C });
			}
			uint8_t Kind;
			if (Outside(W.X, W.Y) < 100.0)
			{
				// (a plot at the field's edge takes the rules' land, so it runs on the same out into the band)
				const uint8_t G = SmoothCellRegion(std::clamp(W.X, 1.0, FW - 1.0), std::clamp(W.Y, 1.0, FH - 1.0));
				Kind = IsFarmland(G) && G != RgWorks ? G : static_cast<uint8_t>(RgOpen);
			}
			else Kind = Unit(Mix(Key ^ 0x4B1D)) < 0.36 ? RgField : RgOpen;
			*OutKind = Kind;
		}
		return static_cast<int>(Key % 999983u);
	}

	/**
	 * What a parcel grows, by its seed, in July in Normandy: pastures grazed, lush, mown for hay (the aftermath pale) or
	 * standing in seed; corn fields of ripening wheat, barley already gold, oats, a late field still green, a barley field
	 * cut to stubble, a few roots.
	 */
	int Ground::Impl::CropOf(int SeedIndex, int Kind) const
	{
		const double U = Unit(Hash(Seed ^ 0xC80B, static_cast<uint64_t>(SeedIndex), static_cast<uint64_t>(Kind)));
		switch (Kind)
		{
		case RgField: return U < 0.46 ? CpWheat : U < 0.66 ? CpBarley : U < 0.78 ? CpOats : U < 0.86 ? CpGreenWheat : U < 0.94 ? CpStubble : CpRoots;
		case RgOpen: return U < 0.45 ? CpGrazed : U < 0.65 ? CpLush : U < 0.82 ? CpHayCut : CpMeadow;
		case RgRough: return CpRough;
		case RgWorks: return CpWorks;
		case RgVillage: return CpGarden;
		case RgYard: return CpYard;
		case RgWoods: return CpWoods;
		default: return CpOther;
		}
	}

	// ====================================================================== the village and the farmsteads

	/** Whether a building (or a yard's box) stands clear: on the raster, off the lanes, the woods, the water, the ways and what is built. */
	bool Ground::Impl::Fits(const House& H, double Margin) const
	{
		const double C = std::cos(H.Yaw), S = std::sin(H.Yaw);
		const double HU = 0.5 * H.Long + Margin, HV = 0.5 * H.Deep + Margin, R = std::hypot(HU, HV);
		const int I0 = static_cast<int>((H.C.X - R - RX0) / Res) - 1, I1 = static_cast<int>((H.C.X + R - RX0) / Res) + 1;
		const int J0 = static_cast<int>((H.C.Y - R - RY0) / Res) - 1, J1 = static_cast<int>((H.C.Y + R - RY0) / Res) + 1;
		if (I0 < 2 || J0 < 2 || I1 >= NX - 2 || J1 >= NY - 2) return false;
		for (int J = J0; J <= J1; ++J)
			for (int I = I0; I <= I1; ++I)
			{
				const double DX = RX0 + (I + 0.5) * Res - H.C.X, DY = RY0 + (J + 0.5) * Res - H.C.Y;
				const double U = DX * C + DY * S, V = -DX * S + DY * C;
				if (std::fabs(U) > HU || std::fabs(V) > HV) continue;
				const size_t K = static_cast<size_t>(J) * NX + I;
				if (OccR[K] & (OcHouse | OcNear | OcVehicle | OcMan | OcWall)) return false;
				const uint8_t G = Rgn[K];
				if (G == RgWater || G == RgMarsh || G == RgWoods) return false;
				const bool bIn = std::fabs(U) <= 0.5 * H.Long + 0.3 && std::fabs(V) <= 0.5 * H.Deep + 0.3;
				if (LaneD[K] < (bIn ? 0.8f : 0.0f)) return false;
			}
		return true;
	}

	void Ground::Impl::Stamp(const House& H, double Margin)
	{
		const double C = std::cos(H.Yaw), S = std::sin(H.Yaw);
		const double HU = 0.5 * H.Long + Margin, HV = 0.5 * H.Deep + Margin, R = std::hypot(HU, HV);
		const int I0 = std::max(0, static_cast<int>((H.C.X - R - RX0) / Res) - 1), I1 = std::min(NX - 1, static_cast<int>((H.C.X + R - RX0) / Res) + 1);
		const int J0 = std::max(0, static_cast<int>((H.C.Y - R - RY0) / Res) - 1), J1 = std::min(NY - 1, static_cast<int>((H.C.Y + R - RY0) / Res) + 1);
		for (int J = J0; J <= J1; ++J)
			for (int I = I0; I <= I1; ++I)
			{
				const double DX = RX0 + (I + 0.5) * Res - H.C.X, DY = RY0 + (J + 0.5) * Res - H.C.Y;
				const double U = std::fabs(DX * C + DY * S), V = std::fabs(-DX * S + DY * C);
				if (U > HU || V > HV) continue;
				// (a pixel any part of which the footprint covers is the house's: At() then tests the footprint itself)
				const bool bIn = U <= 0.5 * H.Long + 0.75 * Res && V <= 0.5 * H.Deep + 0.75 * Res;
				OccR[static_cast<size_t>(J) * NX + I] |= bIn ? (OcHouse | OcNear) : OcNear;
			}
	}

	/** A building's floor: a little above the highest ground under it (a plinth shows on a slope); its walls reach below the lowest. */
	void Ground::Impl::FloorOf(House& H) const
	{
		const double C = std::cos(H.Yaw), S = std::sin(H.Yaw);
		double Hi = -1e9, Lo = 1e9;
		for (int A = -1; A <= 1; ++A)
			for (int B = -1; B <= 1; ++B)
			{
				const double U = 0.5 * H.Long * A, V = 0.5 * H.Deep * B;
				const double X = H.C.X + U * C - V * S, Y = H.C.Y + U * S + V * C;
				const double Z = Carve(X, Y, Terrain(X, Y), nullptr);
				Hi = std::max(Hi, Z);
				Lo = std::min(Lo, Z);
			}
		H.Floor = Hi + 0.15;
		H.Foot = Lo - 0.6;
	}

	/** A dry-stone wall along a line: its points about 2 m apart, its height wandering a little, its line kept clear. */
	void Ground::Impl::AddWall(const Line& L, double High, double Thick, uint64_t Key)
	{
		if (L.size() < 2 || LineLength(L) < 1.0) return;
		WallRun W;
		W.P = Resample(L, 2.0);
		W.Thick = Thick;
		W.Key = Key;
		double S = 0.0;
		for (size_t K = 0; K < W.P.size(); ++K)
		{
			if (K > 0) S += Len(W.P[K] - W.P[K - 1]);
			W.H.push_back(static_cast<float>(High * (0.93 + 0.14 * (0.5 + 0.5 * Noise(S / 9.0, static_cast<double>(Key % 997), Seed ^ 0x3A11)))));
		}
		for (size_t K = 0; K + 1 < W.P.size(); ++K)
		{
			const V2 A = W.P[K], B = W.P[K + 1];
			const double Reach = 0.5 * Thick + 0.6;
			const int I0 = std::max(0, static_cast<int>((std::min(A.X, B.X) - Reach - RX0) / Res));
			const int I1 = std::min(NX - 1, static_cast<int>((std::max(A.X, B.X) + Reach - RX0) / Res));
			const int J0 = std::max(0, static_cast<int>((std::min(A.Y, B.Y) - Reach - RY0) / Res));
			const int J1 = std::min(NY - 1, static_cast<int>((std::max(A.Y, B.Y) + Reach - RY0) / Res));
			for (int J = J0; J <= J1; ++J)
				for (int I = I0; I <= I1; ++I)
				{
					double T;
					if (SegDist({ RX0 + (I + 0.5) * Res, RY0 + (J + 0.5) * Res }, A, B, T) < Reach) OccR[static_cast<size_t>(J) * NX + I] |= OcWall;
				}
		}
		Count.HedgeKm += LineLength(W.P) / 1000.0;
		HedgeLine Out;
		Out.Kind = HkWall;
		for (const V2& P : W.P) Out.Points.push_back({ P.X, P.Y });
		HedgeOut.push_back(std::move(Out));
		Walls.push_back(std::move(W));
	}

	/**
	 * The village (Le Mesnil on the dry run): its church near the middle, altar to the east, in a walled churchyard;
	 * houses fronting the lanes through it, one or two storeys (the rules' storeys where the cell gives them), slate or
	 * tile; then farmsteads, each a yard walled to its lane with the farmhouse across its back, a barn and a byre down
	 * its sides, a well and a cart, an orchard behind. Some are already hit: a roof holed, a gable down, a shell burnt out.
	 * Nothing is built on the recording's ways.
	 */
	void Ground::Impl::MakeSites()
	{
		Rng R(Seed ^ 0x5173);
		auto Hurt = [&R](House& H, double Odds) {
			const double U = R.Next() / Odds;
			H.Hurt = U < 0.06 ? DmShell : U < 0.18 ? DmGable : U < 0.38 ? DmRoofHole : DmNone;
		};
		auto Wall4 = [](V2 O, V2 U, V2 V, double U0, double U1, double V0, double V1) {
			return Line{ O + U * U0 + V * V0, O + U * U1 + V * V0, O + U * U1 + V * V1, O + U * U0 + V * V1, O + U * U0 + V * V0 };
		};
		if (Fd.bTown)
		{
			bool bDone = false;
			for (int Ring = 0; Ring < 14 && !bDone; ++Ring)
				for (int K = 0; K < 16 && !bDone; ++K)
				{
					const double A = K * 2.0 * Pi / 16.0 + Ring * 0.37, D = Ring * 7.0;
					const V2 C{ Fd.TownX + D * std::cos(A), Fd.TownY + D * std::sin(A) };
					const double Yaw = 0.18 * (Unit(Hash(Seed, 0xC4C4, static_cast<uint64_t>(Ring * 16 + K))) - 0.5);
					const V2 U{ std::cos(Yaw), std::sin(Yaw) }, V = Perp(U);
					House Yard;
					Yard.C = C + U * 0.7;
					Yard.Yaw = Yaw;
					Yard.Long = 31.0 + 12.0;
					Yard.Deep = 8.6 + 12.0;
					if (!Fits(Yard, 0.5)) continue;
					House Nave, Chancel, Tower;
					Nave.C = C;
					Nave.Long = 18.0;
					Nave.Deep = 8.6;
					Chancel.C = C + U * (9.0 + 3.6);
					Chancel.Long = 7.2;
					Chancel.Deep = 6.6;
					Chancel.Part = 1;
					Tower.C = C - U * (9.0 + 2.9);
					Tower.Long = Tower.Deep = 5.8;
					Tower.Part = 2;
					for (House* H : { &Nave, &Chancel, &Tower })
					{
						H->Yaw = Yaw;
						H->Type = HtChurch;
						H->Roof = 0;
						H->Front = 1;
						H->Key = Hash(Seed, 0xC4C5, static_cast<uint64_t>(H->Part));
						FloorOf(*H);
					}
					Nave.Hurt = R.Next() < 0.35 ? DmRoofHole : DmNone;
					Chancel.Floor = Tower.Floor = Nave.Floor;
					// The churchyard: walled, its gate at the west end before the tower's door.
					Stamp(Yard, 0.0);
					const double U0 = -14.8 - 6.0 + 0.7 - 0.7, U1 = 16.2 + 6.0;
					Line West{ C + U * U0 + V * 1.6, C + U * U0 + V * 10.3, C + U * U1 + V * 10.3, C + U * U1 + V * -10.3, C + U * U0 + V * -10.3, C + U * U0 + V * -1.6 };
					AddWall(West, 1.3, 0.6, Hash(Seed, 0xC4C6));
					Houses.push_back(Nave);
					Houses.push_back(Chancel);
					Houses.push_back(Tower);
					Count.bChurch = true;
					bDone = true;
				}
			// Houses fronting the lanes through the village, each side walked along its lane.
			int Placed = 0;
			for (size_t L = 0; L < Lanes.size(); ++L)
			{
				const Line& P = Lanes[L].P;
				const double Half = Lanes[L].Half;
				for (int Side = -1; Side <= 1; Side += 2)
				{
					size_t K = 0;
					while (K + 1 < P.size() && Placed < 42)
					{
						const V2 At = P[K];
						if (std::hypot(At.X - Fd.TownX, At.Y - Fd.TownY) > Fd.TownR * 1.05)
						{
							++K;
							continue;
						}
						const V2 T = Unit2(P[K + 1] - P[K]), N = Perp(T);
						House H;
						H.Type = HtHouse;
						H.Long = R.Range(8.5, 15.0);
						H.Deep = R.Range(6.0, 7.4);
						const int St = Fd.Storeys.empty() ? 0 : Fd.Storeys[static_cast<size_t>(CellAt(At.X, At.Y))];
						H.Storeys = St > 0 ? std::min(St, 3) : (R.Next() < 0.55 ? 2 : 1);
						const double Back = R.Next() < 0.5 ? R.Range(0.2, 0.8) : R.Range(1.0, 4.5);
						H.C = At + T * (0.5 * H.Long) + N * (Side * (Half + 0.9 + Back + 0.5 * H.Deep));
						H.Yaw = std::atan2(T.Y, T.X) + R.Range(-0.04, 0.04);
						H.Front = -Side;
						H.Roof = R.Next() < 0.62 ? 0 : 1;
						H.Key = Mix(R.State);
						Hurt(H, 1.0);
						if (Fits(H, 1.0))
						{
							FloorOf(H);
							Stamp(H, 1.2);
							Houses.push_back(H);
							++Placed;
							K += static_cast<size_t>((H.Long + R.Range(1.5, 11.0)) / 2.0) + 1;
						}
						else K += 2;
					}
				}
			}
		}
		// Farmsteads, on the lanes: two at the village's edge, then out in the bocage, each well apart.
		struct Try
		{
			size_t Lane, K;
			int Side;
			double Key;
		};
		std::vector<Try> Tries;
		for (size_t L = 0; L < Lanes.size(); ++L)
			for (size_t K = 6; K + 6 < Lanes[L].P.size(); K += 9)
				for (int Side = -1; Side <= 1; Side += 2) Tries.push_back({ L, K, Side, R.Next() });
		std::sort(Tries.begin(), Tries.end(), [](const Try& A, const Try& B) { return A.Key < B.Key; });
		std::vector<V2> Farms;
		for (int Pass = 0; Pass < 2; ++Pass)
			for (const Try& Tr : Tries)
			{
				if (Farms.size() >= 7 || (Pass == 0 && Farms.size() >= 2)) break;
				const Line& P = Lanes[Tr.Lane].P;
				const V2 At = P[Tr.K];
				if (At.X < 80.0 || At.Y < 80.0 || At.X > FW - 80.0 || At.Y > FH - 80.0) continue;
				const double FromTown = Fd.bTown ? std::hypot(At.X - Fd.TownX, At.Y - Fd.TownY) : 1e9;
				if (Pass == 0 ? (FromTown < Fd.TownR + 30.0 || FromTown > Fd.TownR + 220.0) : FromTown < Fd.TownR + 120.0) continue;
				bool bNear = false;
				for (const V2& F : Farms)
					if (Len(F - At) < 380.0) bNear = true;
				if (bNear) continue;
				const V2 U = Unit2(P[Tr.K + 1] - P[Tr.K - 1]), V = Perp(U) * static_cast<double>(Tr.Side);
				const double Vs = Lanes[Tr.Lane].Half + LaneWall + 2.5;
				const uint64_t Key = Hash(Seed, 0xFA4A, static_cast<uint64_t>(Tr.Lane * 4096 + Tr.K * 2 + (Tr.Side > 0)));
				House Box;
				Box.C = At + V * (Vs + 13.0);
				Box.Yaw = std::atan2(U.Y, U.X);
				Box.Long = 31.0;
				Box.Deep = 27.0;
				if (!Fits(Box, 0.5)) continue;
				int Farmland = 0, All = 0;
				for (double A = -15.0; A <= 15.0; A += 3.0)
					for (double B = 0.0; B <= 26.0; B += 3.0)
					{
						const V2 Q = At + U * A + V * (Vs + B);
						const uint8_t G = Rgn[Pix(Q.X, Q.Y)];
						++All;
						if (IsFarmland(G) || G == RgSeeded || G == RgVillage) ++Farmland;
					}
				if (Farmland < All * 9 / 10) continue;
				const double YawU = std::atan2(U.Y, U.X), YawV = std::atan2(V.Y, V.X);
				House Home, Barn, Byre;
				Home.Type = HtFarmhouse;
				Home.C = At + U * 0.0 + V * (Vs + 22.4);
				Home.Yaw = YawU;
				Home.Long = 14.0;
				Home.Deep = 7.2;
				Home.Storeys = 2;
				Home.Front = -Tr.Side; // its door to the yard
				Barn.Type = HtBarn;
				Barn.C = At + U * -10.8 + V * (Vs + 9.5);
				Barn.Yaw = YawV;
				Barn.Long = 15.0;
				Barn.Deep = 8.4;
				Barn.Storeys = 1;
				Barn.Front = -1;
				Byre.Type = HtByre;
				Byre.C = At + U * 11.6 + V * (Vs + 11.5);
				Byre.Yaw = YawV;
				Byre.Long = 13.0;
				Byre.Deep = 6.8;
				Byre.Storeys = 1;
				Byre.Front = 1;
				for (House* H : { &Home, &Barn, &Byre })
				{
					H->Roof = Unit(Hash(Key, 0x50F)) < 0.5 ? 0 : 1;
					H->Key = Hash(Key, static_cast<uint64_t>(H->Type));
					Hurt(*H, 1.3);
					FloorOf(*H);
				}
				Stamp(Home, 1.0);
				Stamp(Barn, 1.0);
				Stamp(Byre, 1.0);
				Houses.push_back(Home);
				Houses.push_back(Barn);
				Houses.push_back(Byre);
				// The yard's walls round what the buildings leave open, a gate gap onto the lane.
				const Line Ring = Resample(Wall4(At, U, V, -15.0, 15.0, Vs, Vs + 26.0), 1.0);
				Line Cur;
				auto Flush = [&]() {
					if (Cur.size() >= 3) AddWall(Cur, 1.7, 0.55, Hash(Key, Cur.size(), static_cast<uint64_t>(Walls.size())));
					Cur.clear();
				};
				for (const V2& Q : Ring)
				{
					const double A = Dot(Q - At, U), B = Dot(Q - At, V);
					bool bOpen = std::fabs(A) < 2.3 && B < Vs + 0.5;
					for (const House* H : { &Home, &Barn, &Byre })
					{
						const V2 D = Q - H->C;
						const double HU = std::fabs(D.X * std::cos(H->Yaw) + D.Y * std::sin(H->Yaw)), HV = std::fabs(-D.X * std::sin(H->Yaw) + D.Y * std::cos(H->Yaw));
						if (HU < 0.5 * H->Long + 0.4 && HV < 0.5 * H->Deep + 0.4) bOpen = true;
					}
					if (bOpen) Flush();
					else Cur.push_back(Q);
				}
				Flush();
				YardBox Y;
				Y.O = At;
				Y.U = U;
				Y.V = V;
				Y.U0 = -15.0;
				Y.U1 = 15.0;
				Y.V0 = Vs;
				Y.V1 = Vs + 26.0;
				Yards.push_back(Y);
				for (double A = -15.0; A <= 15.0; A += 1.0)
					for (double B = Vs; B <= Vs + 26.0; B += 1.0)
					{
						const V2 Q = At + U * A + V * B;
						if (InRaster(Q.X, Q.Y)) Rgn[Pix(Q.X, Q.Y)] = RgYard;
					}
				Props.push_back({ At + U * 5.5 + V * (Vs + 9.0), R.Range(0.0, 2.0 * Pi), 0 });
				Props.push_back({ At + U * -3.0 + V * (Vs + 6.0), YawU + R.Range(-0.6, 0.6), 1 });
				OrchardAt.push_back(At + V * (Vs + 26.0 + 32.0));
				Farms.push_back(At);
			}
		Count.Farmsteads = static_cast<int>(Farms.size());
		for (const House& H : Houses)
		{
			if (H.Type == HtChurch) continue;
			++Count.Houses;
			if (H.Hurt != DmNone) ++Count.Damaged;
		}
	}

	// ====================================================================== the parcels

	void Ground::Impl::MakeParcels()
	{
		const size_t N = static_cast<size_t>(NX) * NY;
		constexpr double VStep = 44.0;
		constexpr int64_t VBase = 1000000, YBase = 9000000;
		std::vector<int64_t> Label(N);
		// The plot of every pixel, by blocks of 4 x 4: where a block's corners agree, so does all of it (a parcel is a few
		// thousand pixels; only its edges need each pixel asked).
		std::vector<int32_t> Near(N, -1);
		std::unordered_map<int32_t, uint8_t> PlotKind;
		{
			constexpr int Blk = 4;
			auto Ask = [this, &PlotKind](int I, int J) {
				const double X = RX0 + (I + 0.5) * Res, Y = RY0 + (J + 0.5) * Res;
				const int Id = Plot(X, Y, nullptr, nullptr);
				if (!PlotKind.count(Id))
				{
					uint8_t Kind = RgOpen;
					Plot(X, Y, nullptr, &Kind);
					PlotKind.emplace(Id, Kind);
				}
				return Id;
			};
			const int BX = (NX + Blk - 1) / Blk, BY = (NY + Blk - 1) / Blk;
			std::vector<int32_t> Corner(static_cast<size_t>(BX + 1) * (BY + 1));
			for (int J = 0; J <= BY; ++J)
				for (int I = 0; I <= BX; ++I) Corner[static_cast<size_t>(J) * (BX + 1) + I] = Ask(std::min(I * Blk, NX - 1), std::min(J * Blk, NY - 1));
			for (int J = 0; J < BY; ++J)
				for (int I = 0; I < BX; ++I)
				{
					const int32_t A = Corner[static_cast<size_t>(J) * (BX + 1) + I];
					const bool bOne = A == Corner[static_cast<size_t>(J) * (BX + 1) + I + 1] && A == Corner[static_cast<size_t>(J + 1) * (BX + 1) + I] && A == Corner[static_cast<size_t>(J + 1) * (BX + 1) + I + 1];
					for (int Y = J * Blk; Y < std::min(NY, (J + 1) * Blk); ++Y)
						for (int X = I * Blk; X < std::min(NX, (I + 1) * Blk); ++X)
						{
							const size_t K = static_cast<size_t>(Y) * NX + X;
							const uint8_t G = Rgn[K];
							if (IsFarmland(G) || G == RgSeeded) Near[K] = bOne ? A : Ask(X, Y);
						}
				}
		}
		// Corn or grass by the whole field: where the rules' field and open cells share a plot, the plot is what most of
		// it is, unless the lesser is a good part of it (then the rules' edge parts it, as a hedge would).
		std::unordered_map<int32_t, std::array<int, 2>> Share;
		for (size_t K = 0; K < N; ++K)
			if (Rgn[K] == RgOpen || Rgn[K] == RgField) ++Share[Near[K]][Rgn[K] == RgField ? 1 : 0];
		for (int J = 0; J < NY; ++J)
			for (int I = 0; I < NX; ++I)
			{
				const size_t K = static_cast<size_t>(J) * NX + I;
				const double X = RX0 + (I + 0.5) * Res, Y = RY0 + (J + 0.5) * Res;
				const uint8_t G = Rgn[K];
				if (IsFarmland(G) || G == RgSeeded)
				{
					const int S = Near[K];
					int Kind = G == RgSeeded ? PlotKind[S] : G;
					if (G == RgOpen || G == RgField)
					{
						const std::array<int, 2>& Sh = Share[S];
						const int Most = Sh[1] > Sh[0] ? RgField : RgOpen;
						if (std::min(Sh[0], Sh[1]) < 0.42 * (Sh[0] + Sh[1])) Kind = Most;
					}
					if (G == RgSeeded || Kind != G) Rgn[K] = static_cast<uint8_t>(Kind);
					Label[K] = static_cast<int64_t>(S) * 16 + Kind;
				}
				else if (G == RgVillage)
				{
					// Gardens, yards and orchards: a finer grid of their own.
					const int VI = static_cast<int>(std::floor(X / VStep)), VJ = static_cast<int>(std::floor(Y / VStep));
					double Best = 1e18;
					int64_t Which = 0;
					for (int DJ = -1; DJ <= 1; ++DJ)
						for (int DI = -1; DI <= 1; ++DI)
						{
							const uint64_t H = Hash(static_cast<uint64_t>(VI + DI), static_cast<uint64_t>(VJ + DJ), Seed ^ 0x7111);
							const double SX = (VI + DI + 0.5 + 0.7 * (Unit(H) - 0.5)) * VStep, SY = (VJ + DJ + 0.5 + 0.7 * (Unit(Mix(H)) - 0.5)) * VStep;
							const double D = Sq(SX - X) + Sq(SY - Y);
							if (D < Best)
							{
								Best = D;
								Which = static_cast<int64_t>(VJ + DJ + 1000) * 4096 + (VI + DI + 1000);
							}
						}
					Label[K] = (VBase + Which) * 16 + RgVillage;
				}
				else if (G == RgYard)
				{
					size_t Which = 0;
					double Best = 1e18;
					for (size_t Y2 = 0; Y2 < Yards.size(); ++Y2)
					{
						const double D = Len(Yards[Y2].O - V2{ X, Y });
						if (D < Best)
						{
							Best = D;
							Which = Y2;
						}
					}
					Label[K] = (YBase + static_cast<int64_t>(Which)) * 16 + RgYard;
				}
				else Label[K] = -static_cast<int64_t>(G) - 1;
			}
		// Parcels: the land of one label that hangs together.
		Pid.assign(N, -1);
		std::vector<int64_t> CompLabel;
		std::vector<size_t> Stack;
		for (size_t K0 = 0; K0 < N; ++K0)
		{
			if (Pid[K0] >= 0) continue;
			const int32_t Id = static_cast<int32_t>(CompLabel.size());
			CompLabel.push_back(Label[K0]);
			Pid[K0] = Id;
			Stack.push_back(K0);
			while (!Stack.empty())
			{
				const size_t K = Stack.back();
				Stack.pop_back();
				const int I = static_cast<int>(K % static_cast<size_t>(NX)), J = static_cast<int>(K / static_cast<size_t>(NX));
				const size_t Next[4] = { I > 0 ? K - 1 : K, I + 1 < NX ? K + 1 : K, J > 0 ? K - static_cast<size_t>(NX) : K, J + 1 < NY ? K + static_cast<size_t>(NX) : K };
				for (const size_t Q : Next)
					if (Pid[Q] < 0 && Label[Q] == Label[K])
					{
						Pid[Q] = Id;
						Stack.push_back(Q);
					}
			}
		}
		const size_t NComp = CompLabel.size();
		auto KindOf = [&CompLabel](size_t C) -> int {
			const int64_t L = CompLabel[C];
			return L >= 0 ? static_cast<int>(L & 15) : static_cast<int>(-L - 1);
		};
		// Slivers (where a kind's edge passes near a seed's) join the neighbour they share most edge with.
		std::vector<size_t> Area(NComp, 0), Start(NComp + 1, 0), Pixels(N);
		for (size_t K = 0; K < N; ++K) ++Area[static_cast<size_t>(Pid[K])];
		for (size_t C = 0; C < NComp; ++C) Start[C + 1] = Start[C] + Area[C];
		{
			std::vector<size_t> Fill(Start.begin(), Start.end() - 1);
			for (size_t K = 0; K < N; ++K) Pixels[Fill[static_cast<size_t>(Pid[K])]++] = K;
		}
		std::vector<int32_t> Parent(NComp);
		for (size_t C = 0; C < NComp; ++C) Parent[C] = static_cast<int32_t>(C);
		auto Find = [&Parent](int32_t C) {
			while (Parent[static_cast<size_t>(C)] != C) C = Parent[static_cast<size_t>(C)] = Parent[static_cast<size_t>(Parent[static_cast<size_t>(C)])];
			return C;
		};
		const double PixArea = Res * Res;
		for (size_t C = 0; C < NComp; ++C)
		{
			const int Kind = KindOf(C);
			const double A = static_cast<double>(Area[static_cast<size_t>(Find(static_cast<int32_t>(C)))]) * PixArea;
			const bool bSmall = (IsFarmland(Kind) && A < 2200.0) || (Kind == RgVillage && A < 300.0);
			if (!bSmall || Find(static_cast<int32_t>(C)) != static_cast<int32_t>(C)) continue;
			std::unordered_map<int32_t, int> Shared;
			for (size_t P = Start[C]; P < Start[C + 1]; ++P)
			{
				const size_t K = Pixels[P];
				const int I = static_cast<int>(K % static_cast<size_t>(NX)), J = static_cast<int>(K / static_cast<size_t>(NX));
				const size_t Next[4] = { I > 0 ? K - 1 : K, I + 1 < NX ? K + 1 : K, J > 0 ? K - static_cast<size_t>(NX) : K, J + 1 < NY ? K + static_cast<size_t>(NX) : K };
				for (const size_t Q : Next)
				{
					const int32_t O = Find(Pid[Q]);
					if (O != static_cast<int32_t>(C)) ++Shared[O];
				}
			}
			int32_t Best = -1;
			int BestScore = 0;
			for (const auto& [O, Edges] : Shared)
			{
				const int OK = KindOf(static_cast<size_t>(O));
				const bool bFits = Kind == RgVillage ? OK == RgVillage : IsFarmland(OK);
				if (!bFits) continue;
				const int Score = Edges * (OK == Kind ? 4 : 1);
				if (Score > BestScore || (Score == BestScore && O < Best))
				{
					Best = O;
					BestScore = Score;
				}
			}
			if (Best < 0) continue;
			Parent[C] = Best;
			Area[static_cast<size_t>(Best)] += Area[C];
		}
		for (size_t K = 0; K < N; ++K) Pid[K] = Find(Pid[K]);
		// What each parcel is: its kind, size, middle, length's direction (for ploughing and rows), crop.
		Parcels.assign(NComp, ParcelInfo());
		std::vector<double> SX(NComp, 0.0), SY(NComp, 0.0), SXX(NComp, 0.0), SYY(NComp, 0.0), SXY(NComp, 0.0), Cnt(NComp, 0.0);
		for (int J = 0; J < NY; ++J)
			for (int I = 0; I < NX; ++I)
			{
				const size_t C = static_cast<size_t>(Pid[static_cast<size_t>(J) * NX + I]);
				const double X = RX0 + (I + 0.5) * Res, Y = RY0 + (J + 0.5) * Res;
				SX[C] += X;
				SY[C] += Y;
				SXX[C] += X * X;
				SYY[C] += Y * Y;
				SXY[C] += X * Y;
				Cnt[C] += 1.0;
			}
		double FarmArea = 0.0;
		int Farmed = 0;
		for (size_t C = 0; C < NComp; ++C)
		{
			ParcelInfo& Q = Parcels[C];
			Q.Kind = KindOf(C);
			if (Cnt[C] <= 0.0) continue;
			Q.Area = Cnt[C] * PixArea;
			Q.CX = SX[C] / Cnt[C];
			Q.CY = SY[C] / Cnt[C];
			const double CXX = SXX[C] / Cnt[C] - Q.CX * Q.CX, CYY = SYY[C] / Cnt[C] - Q.CY * Q.CY, CXY = SXY[C] / Cnt[C] - Q.CX * Q.CY;
			Q.Angle = 0.5 * std::atan2(2.0 * CXY, CXX - CYY);
			Q.Key = Hash(Seed ^ 0xBA2C, static_cast<uint64_t>(CompLabel[C]));
			Q.Lands = 13.0 + 6.0 * Unit(Q.Key);
			Q.Seed = CompLabel[C] >= 0 && CompLabel[C] < VBase * 16 ? static_cast<int>(CompLabel[C] >> 4) : -1;
			Q.Crop = CropOf(Q.Seed, Q.Kind);
			if (Q.Kind == RgVillage && Q.Area > 500.0 && Unit(Mix(Q.Key)) < 0.4) Q.bOrchard = true;
			if (Q.Kind == RgOpen && Fd.bTown && std::hypot(Q.CX - Fd.TownX, Q.CY - Fd.TownY) < Fd.TownR + 300.0 && Unit(Mix(Q.Key ^ 0x0C)) < 0.12) Q.bOrchard = true;
			if (IsFarmland(Q.Kind) && Q.CX > -Band && Q.CY > -Band)
			{
				FarmArea += Q.Area;
				++Farmed;
			}
		}
		for (const V2& O : OrchardAt)
		{
			if (!InRaster(O.X, O.Y)) continue;
			ParcelInfo& Q = Parcels[static_cast<size_t>(Pid[Pix(O.X, O.Y)])];
			if (Q.Kind == RgOpen || Q.Kind == RgField || Q.Kind == RgRough)
			{
				Q.bOrchard = true;
				Q.Kind = RgOpen;
				Q.Crop = CpGrazed;
			}
		}
		Count.Parcels = Farmed;
		Count.ParcelMean = Farmed ? std::sqrt(FarmArea / Farmed) : 0.0;
	}

	/** How far each pixel lies inside its parcel from the parcel's edge (a chamfer distance): margins, headlands, woods' edges. */
	void Ground::Impl::MakeDistances()
	{
		const size_t N = static_cast<size_t>(NX) * NY;
		std::vector<int> D(N, 1 << 28);
		for (int J = 0; J < NY; ++J)
			for (int I = 0; I < NX; ++I)
			{
				const size_t K = static_cast<size_t>(J) * NX + I;
				const int32_t P = Pid[K];
				if ((I > 0 && Pid[K - 1] != P) || (I + 1 < NX && Pid[K + 1] != P) || (J > 0 && Pid[K - static_cast<size_t>(NX)] != P) || (J + 1 < NY && Pid[K + static_cast<size_t>(NX)] != P)) D[K] = 0;
			}
		for (int J = 0; J < NY; ++J)
			for (int I = 0; I < NX; ++I)
			{
				const size_t K = static_cast<size_t>(J) * NX + I;
				int V = D[K];
				if (I > 0) V = std::min(V, D[K - 1] + 3);
				if (J > 0)
				{
					V = std::min(V, D[K - static_cast<size_t>(NX)] + 3);
					if (I > 0) V = std::min(V, D[K - static_cast<size_t>(NX) - 1] + 4);
					if (I + 1 < NX) V = std::min(V, D[K - static_cast<size_t>(NX) + 1] + 4);
				}
				D[K] = V;
			}
		for (int J = NY - 1; J >= 0; --J)
			for (int I = NX - 1; I >= 0; --I)
			{
				const size_t K = static_cast<size_t>(J) * NX + I;
				int V = D[K];
				if (I + 1 < NX) V = std::min(V, D[K + 1] + 3);
				if (J + 1 < NY)
				{
					V = std::min(V, D[K + static_cast<size_t>(NX)] + 3);
					if (I + 1 < NX) V = std::min(V, D[K + static_cast<size_t>(NX) + 1] + 4);
					if (I > 0) V = std::min(V, D[K + static_cast<size_t>(NX) - 1] + 4);
				}
				D[K] = V;
			}
		DistE.assign(N, 0.0f);
		for (size_t K = 0; K < N; ++K) DistE[K] = static_cast<float>(D[K] / 3.0 * Res + 0.5 * Res);
	}
}

namespace LandField
{
	namespace
	{
		struct Boundary
		{
			Line P;
			int32_t A = -1, B = -1;
			bool bClosed = false;
		};

		/**
		 * The edges between pixels of different ids, chained into lines that each part one pair of ids: from a meeting
		 * of three or more (or where the pair changes) to the next; rings where a parcel lies inside another. Keep says
		 * which pairs to trace. Points are the pixels' corners, in metres.
		 */
		std::vector<Boundary> TraceEdges(const std::vector<int32_t>& Id, int NX, int NY, double X0, double Y0, double Pixel, const std::function<bool(int32_t, int32_t)>& Keep)
		{
			const size_t SNX = static_cast<size_t>(NX);
			auto PixId = [&Id, SNX](int I, int J) { return Id[static_cast<size_t>(J) * SNX + static_cast<size_t>(I)]; };
			// H(i, j): the corner (i, j) to (i + 1, j), between pixels (i, j - 1) and (i, j). V(i, j): (i, j) to (i, j + 1), between (i - 1, j) and (i, j).
			std::vector<uint8_t> HasH(SNX * static_cast<size_t>(NY + 1), 0), HasV((SNX + 1) * static_cast<size_t>(NY), 0);
			for (int J = 1; J < NY; ++J)
				for (int I = 0; I < NX; ++I)
				{
					const int32_t A = PixId(I, J - 1), B = PixId(I, J);
					if (A != B && Keep(A, B)) HasH[static_cast<size_t>(J) * SNX + I] = 1;
				}
			for (int J = 0; J < NY; ++J)
				for (int I = 1; I < NX; ++I)
				{
					const int32_t A = PixId(I - 1, J), B = PixId(I, J);
					if (A != B && Keep(A, B)) HasV[static_cast<size_t>(J) * (SNX + 1) + I] = 1;
				}
			struct Edge
			{
				bool bH = true;
				int I = 0, J = 0;
			};
			auto Present = [&](const Edge& E) {
				if (E.bH) return E.I >= 0 && E.I < NX && E.J > 0 && E.J < NY && HasH[static_cast<size_t>(E.J) * SNX + E.I];
				return E.J >= 0 && E.J < NY && E.I > 0 && E.I < NX && HasV[static_cast<size_t>(E.J) * (SNX + 1) + E.I];
			};
			auto PairOf = [&](const Edge& E) {
				const int32_t A = E.bH ? PixId(E.I, E.J - 1) : PixId(E.I - 1, E.J), B = PixId(E.I, E.J);
				return std::make_pair(std::min(A, B), std::max(A, B));
			};
			std::vector<uint8_t> VisH(HasH.size(), 0), VisV(HasV.size(), 0);
			auto Visited = [&](const Edge& E) -> uint8_t& { return E.bH ? VisH[static_cast<size_t>(E.J) * SNX + E.I] : VisV[static_cast<size_t>(E.J) * (SNX + 1) + E.I]; };
			auto Incident = [&](int I, int J, Edge Out[4]) {
				int N = 0;
				const Edge All[4] = { { true, I, J }, { true, I - 1, J }, { false, I, J }, { false, I, J - 1 } };
				for (const Edge& E : All)
					if (Present(E)) Out[N++] = E;
				return N;
			};
			auto IsNode = [&](int I, int J) {
				Edge E[4];
				const int N = Incident(I, J, E);
				return N != 2 || PairOf(E[0]) != PairOf(E[1]);
			};
			auto OtherEnd = [](const Edge& E, int I, int J, int& OI, int& OJ) {
				const int AI = E.I, AJ = E.J, BI = E.bH ? E.I + 1 : E.I, BJ = E.bH ? E.J : E.J + 1;
				if (AI == I && AJ == J)
				{
					OI = BI;
					OJ = BJ;
				}
				else
				{
					OI = AI;
					OJ = AJ;
				}
			};
			std::vector<Boundary> Out;
			auto Walk = [&](int I, int J, Edge E) {
				Boundary B;
				const auto Pr = PairOf(E);
				B.A = Pr.first;
				B.B = Pr.second;
				const int SI = I, SJ = J;
				B.P.push_back({ X0 + I * Pixel, Y0 + J * Pixel });
				for (;;)
				{
					Visited(E) = 1;
					int NI, NJ;
					OtherEnd(E, I, J, NI, NJ);
					I = NI;
					J = NJ;
					B.P.push_back({ X0 + I * Pixel, Y0 + J * Pixel });
					if (I == SI && J == SJ)
					{
						B.bClosed = true;
						break;
					}
					if (IsNode(I, J)) break;
					Edge Inc[4];
					const int N = Incident(I, J, Inc);
					bool bFound = false;
					for (int K = 0; K < N; ++K)
						if (!Visited(Inc[K]))
						{
							E = Inc[K];
							bFound = true;
							break;
						}
					if (!bFound) break;
				}
				Out.push_back(std::move(B));
			};
			for (int J = 0; J <= NY; ++J)
				for (int I = 0; I <= NX; ++I)
				{
					Edge Inc[4];
					const int N = Incident(I, J, Inc);
					if (N == 0 || !IsNode(I, J)) continue;
					for (int K = 0; K < N; ++K)
						if (!Visited(Inc[K])) Walk(I, J, Inc[K]);
				}
			for (int J = 0; J <= NY; ++J)
				for (int I = 0; I <= NX; ++I)
				{
					Edge Inc[4];
					const int N = Incident(I, J, Inc);
					for (int K = 0; K < N; ++K)
						if (!Visited(Inc[K])) Walk(I, J, Inc[K]);
				}
			return Out;
		}

		/** A hedge while it is laid: its line at 1 m, which points stay, and (lane-side) the parcel beyond each point. */
		struct Hedge
		{
			Line P;
			std::vector<double> S;
			std::vector<char> Keep;
			std::vector<int32_t> Beyond;
			int Kind = HkHedgerow;
			int32_t A = -1, B = -1;
			uint64_t Key = 0;
		};
	}

	/**
	 * The bocage's hedges: every edge between two parcels (and between a parcel and a wood or the village's gardens)
	 * traced, straightened to its runs and bends, and laid as a hedgerow, a wood's edge, a garden hedge or a stone wall
	 * by what it parts; a bank either side of every lane outside the village. Then opened: where a lane passes, where a
	 * house stands, where the recording's vehicles drove through, and a field gate in most parcels.
	 */
	void Ground::Impl::MakeHedges()
	{
		auto Kind = [this](int32_t P) { return Parcels[static_cast<size_t>(P)].Kind; };
		auto Keep = [&Kind](int32_t A, int32_t B) {
			const int KA = Kind(A), KB = Kind(B);
			for (const int K : { KA, KB })
				if (K == RgLane || K == RgWater || K == RgMarsh || K == RgYard) return false;
			return !(KA == RgWoods && KB == RgWoods);
		};
		std::vector<Hedge> Hs;
		for (const Boundary& B : TraceEdges(Pid, NX, NY, RX0, RY0, Res, Keep))
		{
			Line L = Chaikin(Simplify(B.P, 1.6), 1);
			if (LineLength(L) < 3.0) continue;
			Hedge H;
			H.A = B.A;
			H.B = B.B;
			H.Key = Hash(Seed ^ 0x4ED6, static_cast<uint64_t>(B.A), static_cast<uint64_t>(B.B));
			const int KA = Kind(B.A), KB = Kind(B.B);
			const double U = Unit(H.Key);
			if (KA == RgWoods || KB == RgWoods) H.Kind = HkWoodEdge;
			else if (KA == RgVillage && KB == RgVillage) H.Kind = U < 0.4 ? HkWall : HkGarden;
			else if (KA == RgVillage || KB == RgVillage) H.Kind = U < 0.25 ? HkWall : HkHedgerow;
			else H.Kind = HkHedgerow;
			H.P = Resample(L, 1.0);
			Hs.push_back(std::move(H));
		}
		// A bank each side of every lane, its middle a little beyond the lane's wall, outside the village.
		for (size_t L = 0; L < Lanes.size(); ++L)
			for (int Side = -1; Side <= 1; Side += 2)
			{
				const double Off = Lanes[L].Half + LaneWall + 0.75;
				Hedge H;
				H.Kind = HkLaneBank;
				H.Key = Hash(Seed ^ 0x1A4B, static_cast<uint64_t>(L), static_cast<uint64_t>(Side + 1));
				H.P = Resample(Offset(Lanes[L].P, Side * Off), 1.0);
				H.Beyond.assign(H.P.size(), -1);
				for (size_t K = 0; K < H.P.size(); ++K)
				{
					const V2 Way = Unit2(H.P[std::min(K + 1, H.P.size() - 1)] - H.P[K > 0 ? K - 1 : 0]);
					const V2 Q = H.P[K] + Perp(Way) * (Side * 2.5);
					if (InRaster(Q.X, Q.Y)) H.Beyond[K] = Pid[Pix(Q.X, Q.Y)];
				}
				Hs.push_back(std::move(H));
			}
		// What closes each hedge's points.
		for (Hedge& H : Hs)
		{
			const size_t N = H.P.size();
			H.Keep.assign(N, 1);
			H.S.assign(N, 0.0);
			for (size_t K = 1; K < N; ++K) H.S[K] = H.S[K - 1] + Len(H.P[K] - H.P[K - 1]);
			for (size_t K = 0; K < N; ++K)
			{
				const V2 P = H.P[K];
				if (!InRaster(P.X, P.Y))
				{
					H.Keep[K] = 0;
					continue;
				}
				const size_t Px = Pix(P.X, P.Y);
				const uint8_t O = OccR[Px];
				if (O & (H.Kind == HkWall ? OcHouse : (OcHouse | OcNear | OcWall))) H.Keep[K] = 0;
				if (H.Kind == HkLaneBank)
				{
					// None in the village, the yards or the water, and none across another lane.
					const uint8_t G = Rgn[Px];
					if (G == RgVillage || G == RgYard || G == RgWater || G == RgMarsh) H.Keep[K] = 0;
					if (LaneD[Px] < LaneWall + 0.35) H.Keep[K] = 0;
					const int32_t B = H.Beyond[K];
					if (B >= 0)
					{
						const int KB = Kind(B);
						if (KB == RgVillage || KB == RgYard || KB == RgWater || KB == RgMarsh) H.Keep[K] = 0;
					}
				}
				else if (LaneD[Px] < LaneWall + 0.6) H.Keep[K] = 0;
			}
		}
		// The recording's vehicles drove through: their hedges are open where they crossed, bank and all.
		for (Hedge& H : Hs)
			for (size_t K = 0; K + 1 < H.P.size(); ++K)
			{
				const V2 Mid = Lerp2(H.P[K], H.P[K + 1], 0.5);
				const std::vector<uint32_t>* Near = WGrid.At(Mid.X, Mid.Y);
				if (!Near) continue;
				for (const uint32_t W : *Near)
				{
					if (WayHalf[W] < VehicleHalf) continue;
					double T;
					if (!Crosses(H.P[K], H.P[K + 1], WayA[W], WayB[W], T)) continue;
					const double S = Lerp(H.S[K], H.S[K + 1], T), Half = WayHalf[W] + 0.8;
					bool bNew = false;
					for (size_t Q = 0; Q < H.P.size(); ++Q)
						if (std::fabs(H.S[Q] - S) < Half && H.Keep[Q])
						{
							H.Keep[Q] = 0;
							bNew = true;
						}
					if (bNew) ++Count.WayGaps;
				}
			}
		// And where one drove along a hedge rather than across it, its bank is gone there too (nothing stands on its way).
		for (Hedge& H : Hs)
			for (size_t K = 0; K < H.P.size(); ++K)
			{
				if (!H.Keep[K]) continue;
				const std::vector<uint32_t>* Near = WGrid.At(H.P[K].X, H.P[K].Y);
				if (!Near) continue;
				for (const uint32_t W : *Near)
				{
					double T;
					if (WayHalf[W] >= VehicleHalf && SegDist(H.P[K], WayA[W], WayB[W], T) < WayHalf[W] + 1.2)
					{
						H.Keep[K] = 0;
						break;
					}
				}
			}
		// A field gate in most parcels: on its lane where it has one, else into a neighbour.
		struct Span
		{
			size_t H, K0, K1;
			bool bLane;
		};
		std::unordered_map<int32_t, std::vector<Span>> Spans;
		for (size_t Hi = 0; Hi < Hs.size(); ++Hi)
		{
			const Hedge& H = Hs[Hi];
			if (H.Kind == HkWall || H.Kind == HkGarden) continue;
			size_t K = 0;
			while (K < H.P.size())
			{
				if (!H.Keep[K])
				{
					++K;
					continue;
				}
				size_t E = K;
				const int32_t Side = H.Kind == HkLaneBank ? H.Beyond[K] : -2;
				while (E < H.P.size() && H.Keep[E] && (H.Kind != HkLaneBank || H.Beyond[E] == Side)) ++E;
				if (H.S[E - 1] - H.S[K] >= 12.0)
				{
					if (H.Kind == HkLaneBank)
					{
						if (Side >= 0) Spans[Side].push_back({ Hi, K, E, true });
					}
					else
					{
						Spans[H.A].push_back({ Hi, K, E, false });
						Spans[H.B].push_back({ Hi, K, E, false });
					}
				}
				K = E;
			}
		}
		struct GateAt
		{
			V2 P;
			double Yaw;
		};
		std::vector<GateAt> Gates;
		// One gateway to a field, 3 to 4 m wide, near a corner: on its lane where it has one, else into a neighbour (whose
		// gateway it then is too).
		constexpr double GateHalf = 1.6;
		std::vector<char> bGated(Parcels.size(), 0);
		for (size_t P = 0; P < Parcels.size(); ++P)
		{
			const ParcelInfo& Q = Parcels[P];
			if (!IsFarmland(Q.Kind) || Q.Area < 2200.0 || bGated[P]) continue;
			const auto Found = Spans.find(static_cast<int32_t>(P));
			if (Found == Spans.end()) continue;
			Rng R(Q.Key ^ 0x6A7F);
			std::vector<const Span*> OnLane, Others;
			for (const Span& S : Found->second) (S.bLane ? OnLane : Others).push_back(&S);
			const std::vector<const Span*>& From = !OnLane.empty() && (Others.empty() || R.Next() < 0.75) ? OnLane : Others;
			if (From.empty()) continue;
			const Span& S = *From[static_cast<size_t>(R.Below(static_cast<int>(From.size())))];
			Hedge& H = Hs[S.H];
			const double S0 = H.S[S.K0] + 4.5, S1 = H.S[S.K1 - 1] - 4.5;
			if (S1 <= S0) continue;
			// (some 5 to 10 m in from one end of the run: a corner of the field, or where its hedge was already opened)
			const double Inset = R.Range(0.0, std::min(5.5, S1 - S0));
			const double At = R.Next() < 0.5 ? S0 + Inset : S1 - Inset;
			bool bClear = true;
			for (size_t K = S.K0; K < S.K1; ++K)
				if (std::fabs(H.S[K] - At) < GateHalf + 0.3 && !H.Keep[K]) bClear = false;
			if (!bClear) continue;
			bGated[P] = 1;
			if (!S.bLane)
				for (const int32_t Side : { H.A, H.B })
					if (Side >= 0 && static_cast<size_t>(Side) < bGated.size()) bGated[static_cast<size_t>(Side)] = 1;
			size_t Hinge = S.K0;
			for (size_t K = S.K0; K < S.K1; ++K)
			{
				if (std::fabs(H.S[K] - At) < GateHalf) H.Keep[K] = 0;
				if (H.S[K] <= At - GateHalf) Hinge = K;
			}
			const V2 Way = Unit2(H.P[std::min(Hinge + 1, H.P.size() - 1)] - H.P[Hinge]);
			double Yaw = std::atan2(Way.Y, Way.X);
			if (R.Next() < 0.55)
			{
				// Swung open into the parcel.
				const V2 Inward = V2{ Q.CX, Q.CY } - H.P[Hinge];
				Yaw += (Dot(Inward, Perp(Way)) > 0.0 ? 1.0 : -1.0) * R.Range(1.1, 1.75);
			}
			Gates.push_back({ H.P[Hinge] + Way * 0.15, Yaw });
			// The gateway trodden to earth either side.
			const V2 Mid = H.P[Hinge] + Way * 2.1;
			for (double DY = -6.0; DY <= 6.0; DY += Res)
				for (double DX = -6.0; DX <= 6.0; DX += Res)
					if (DX * DX + DY * DY < 30.0 && InRaster(Mid.X + DX, Mid.Y + DY)) OccR[Pix(Mid.X + DX, Mid.Y + DY)] |= OcGate;
		}
		Count.Gates = static_cast<int>(Gates.size());
		// What stays, laid: banks (with their shrubs and trees later) and walls.
		for (Hedge& H : Hs)
		{
			const size_t N = H.P.size();
			size_t K = 0;
			while (K < N)
			{
				if (!H.Keep[K])
				{
					++K;
					continue;
				}
				size_t E = K;
				while (E < N && H.Keep[E]) ++E;
				const bool bNaturalA = K == 0, bNaturalB = E == N;
				Line Dense(H.P.begin() + static_cast<std::ptrdiff_t>(K), H.P.begin() + static_cast<std::ptrdiff_t>(E));
				K = E;
				if (Dense.size() < 3) continue;
				if (H.Kind == HkWall)
				{
					AddWall(Dense, 1.5 + 0.4 * Unit(H.Key ^ 0x3A), 0.55, H.Key ^ static_cast<uint64_t>(K));
					continue;
				}
				Run R;
				R.Kind = H.Kind;
				R.Key = Hash(H.Key, static_cast<uint64_t>(K));
				R.P = Resample(Dense, 2.0);
				const double U1 = Unit(R.Key), U2 = Unit(Mix(R.Key));
				// A hedgerow's bank 1.1 to 1.4 m high (a little more or less along it), 2.2 to 2.7 m across its foot; a lane's the same
				// over the field beside it, and the lane's wall below it besides.
				double H0 = 1.1 + 0.3 * U1, W0 = 2.2 + 0.5 * U2;
				if (H.Kind == HkLaneBank)
				{
					H0 = 1.15 + 0.3 * U1;
					W0 = 2.3 + 0.5 * U2;
				}
				else if (H.Kind == HkWoodEdge)
				{
					H0 = 0.5 + 0.4 * U1;
					W0 = 1.6 + 0.8 * U2;
				}
				else if (H.Kind == HkGarden)
				{
					H0 = 0.25 + 0.15 * U1;
					W0 = 1.0 + 0.4 * U2;
				}
				// A natural end (a meeting of hedges, or a hedge's end in the open) runs on half its foot and rounds off.
				if (bNaturalA) R.P.insert(R.P.begin(), R.P[0] - Unit2(R.P[1] - R.P[0]) * (0.45 * W0));
				if (bNaturalB) R.P.push_back(R.P.back() + Unit2(R.P.back() - R.P[R.P.size() - 2]) * (0.45 * W0));
				const double Total = LineLength(R.P);
				double S = 0.0;
				for (size_t Q = 0; Q < R.P.size(); ++Q)
				{
					if (Q > 0) S += Len(R.P[Q] - R.P[Q - 1]);
					const double Along = 0.5 + 0.5 * Noise(S / 30.0, static_cast<double>(R.Key % 1009), Seed ^ 0xBA4C);
					double Hq = H0 * (0.9 + 0.2 * Along), Wq = W0 * (0.92 + 0.16 * Along);
					const double FromA = S, FromB = Total - S;
					const double TA = bNaturalA ? (Q == 0 ? 0.03 : 1.0) : Smooth(0.0, 1.8, FromA);
					const double TB = bNaturalB ? (Q + 1 == R.P.size() ? 0.03 : 1.0) : Smooth(0.0, 1.8, FromB);
					const double Taper = std::min(TA, TB);
					Hq *= Taper;
					Wq *= 0.6 + 0.4 * Taper;
					R.H.push_back(static_cast<float>(Hq));
					R.W.push_back(static_cast<float>(Wq));
				}
				// Fewer points where the bank runs straight and even (its mesh and At both go straight between them).
				{
					Run Thin;
					Thin.Kind = R.Kind;
					Thin.Key = R.Key;
					const size_t Pts = R.P.size();
					auto Keep1 = [&Thin, &R](size_t Q) {
						Thin.P.push_back(R.P[Q]);
						Thin.H.push_back(R.H[Q]);
						Thin.W.push_back(R.W[Q]);
					};
					Keep1(0);
					size_t A = 0;
					while (A + 1 < Pts)
					{
						size_t B = A + 1;
						// Its ends (the tapers) at every point; elsewhere as far as 8 m while every point between stays within 20 cm of
						// the line and its height and width within a few centimetres of straight.
						while (A >= 1 && B + 2 < Pts && Len(R.P[B + 1] - R.P[A]) <= 8.0)
						{
							bool bFits = true;
							const size_t C = B + 1;
							for (size_t Q = A + 1; Q < C && bFits; ++Q)
							{
								double T;
								const double D = SegDist(R.P[Q], R.P[A], R.P[C], T);
								if (D > 0.2 || std::fabs(R.H[Q] - Lerp(R.H[A], R.H[C], T)) > 0.06 || std::fabs(R.W[Q] - Lerp(R.W[A], R.W[C], T)) > 0.1) bFits = false;
							}
							if (!bFits) break;
							B = C;
						}
						Keep1(B);
						A = B;
					}
					R = std::move(Thin);
				}
				Count.HedgeKm += Total / 1000.0;
				HedgeLine Out;
				Out.Kind = H.Kind;
				for (const V2& P : R.P) Out.Points.push_back({ P.X, P.Y });
				HedgeOut.push_back(std::move(Out));
				Runs.push_back(std::move(R));
			}
		}
		BGrid.Init(-Band - 20.0, -Band - 20.0, FW + Band + 20.0, FH + Band + 20.0, 8.0);
		for (size_t R = 0; R < Runs.size(); ++R)
			for (size_t K = 0; K + 1 < Runs[R].P.size(); ++K)
			{
				BankSeg S;
				S.Run = static_cast<uint32_t>(R);
				S.K = static_cast<uint32_t>(K);
				BGrid.Add(static_cast<uint32_t>(BSegs.size()), Runs[R].P[K], Runs[R].P[K + 1], 0.5 * BankEdge * std::max(Runs[R].W[K], Runs[R].W[K + 1]) + 0.1);
				BSegs.push_back(S);
			}
		// The gates hang where they were cut (after the banks, so they stand on the ground the banks leave).
		Inst.assign(static_cast<size_t>(Base::Count), {});
		for (const GateAt& G : Gates)
		{
			Rgb8 Tint;
			Plant(Base::Gate, G.P.X, G.P.Y, G.Yaw, 1.0, 1.0, Tint);
		}
	}
}

namespace LandField
{
	namespace
	{
		/** The bases' size as built: the hedge shrubs' height, and each tree's (an instance's scale is its height over this). */
		constexpr double ShrubHigh = 2.8;
		double TreeHigh(Base B)
		{
			switch (B)
			{
			case Base::Oak: return 15.0;
			case Base::Elm: return 18.0;
			case Base::Ash: return 15.0;
			case Base::Beech: return 18.0;
			case Base::Pollard: return 8.0;
			case Base::Apple: return 5.5;
			default: return 1.0;
			}
		}

		Rgb8 Tint8(double R, double G, double B)
		{
			Rgb8 T;
			T.R = static_cast<uint8_t>(std::clamp(std::lround(128.0 * R), 0L, 255L));
			T.G = static_cast<uint8_t>(std::clamp(std::lround(128.0 * G), 0L, 255L));
			T.B = static_cast<uint8_t>(std::clamp(std::lround(128.0 * B), 0L, 255L));
			return T;
		}

		/** A tree's kind and height, by what it grows in: the hedgerows (oak most, elm, ash, a few beech) or a wood. */
		Base HedgeTree(Rng& R, bool bWood, double& High)
		{
			const double U = R.Next();
			Base B;
			if (bWood) B = U < 0.35 ? Base::Oak : U < 0.6 ? Base::Beech : U < 0.82 ? Base::Ash : Base::Elm;
			else B = U < 0.48 ? Base::Oak : U < 0.66 ? Base::Elm : U < 0.84 ? Base::Ash : U < 0.9 ? Base::Beech : Base::Pollard;
			switch (B)
			{
			case Base::Oak: High = R.Range(12.0, 18.0); break;
			case Base::Elm: High = R.Range(14.0, 20.0); break;
			case Base::Ash: High = R.Range(11.0, 17.0); break;
			case Base::Beech: High = R.Range(14.0, 21.0); break;
			default: High = R.Range(6.0, 9.0); break;
			}
			if (bWood) High *= 1.08;
			return B;
		}

		/**
		 * A bocage hedge's tree and its height over the bank's top (the bank lifts it some 1.2 m more, to 10 to 15 m over
		 * the field): oak most, a third of all of them pollarded (a thick stub and a bushy head), elm and ash, a beech now
		 * and then; a row of pollards all pollards.
		 */
		Base BocageTree(Rng& R, bool bPollards, double& High)
		{
			const double U = R.Next();
			const Base B = bPollards || U < 0.34 ? Base::Pollard : U < 0.64 ? Base::Oak : U < 0.77 ? Base::Elm : U < 0.95 ? Base::Ash : Base::Beech;
			switch (B)
			{
			case Base::Oak: High = R.Range(10.0, 14.0); break;
			case Base::Elm: High = R.Range(10.5, 14.5); break;
			case Base::Ash: High = R.Range(9.5, 13.5); break;
			case Base::Beech: High = R.Range(10.5, 14.0); break;
			default: High = R.Range(8.5, 11.5); break;
			}
			return B;
		}
	}

	void Ground::Impl::Plant(Base B, double X, double Y, double Yaw, double Scale, double ScaleZ, Rgb8 Tint)
	{
		Placed P;
		P.Kind = B;
		P.X = static_cast<float>(X);
		P.Y = static_cast<float>(Y);
		P.Z = static_cast<float>(Top(X, Y, nullptr));
		P.Yaw = static_cast<float>(Yaw);
		P.Scale = static_cast<float>(Scale);
		P.ScaleZ = static_cast<float>(ScaleZ);
		P.Tint = Tint;
		Inst[static_cast<size_t>(B)].push_back(P);
	}

	/**
	 * What grows: along every bank its shrubs (hawthorn, blackthorn, hazel, holly, elder) in clumps a couple of metres
	 * apart, sometimes thin enough to see through, brambles at its feet, and every so often a tall tree (some hedges a
	 * row of pollards); the woods' trees, undergrowth thick at their edges; the orchards' apple trees in rows; a lone oak
	 * in some pastures; fruit trees in the gardens. Nothing stands on a house, a wall or a lane's floor, no trunk on a
	 * man's way, nothing at all on a vehicle's.
	 */
	void Ground::Impl::MakePlants()
	{
		auto Clear = [this](double X, double Y, uint8_t Bits) {
			if (!InRaster(X, Y)) return false;
			const size_t P = Pix(X, Y);
			return !(OccR[P] & Bits) && LaneD[P] > 0.4f;
		};
		// A recorded man's way and a hedge's shrub: its stem keeps off his way, and where he crosses the hedge its crown too,
		// so the gap he goes through is open (where he walks along the hedge, only his own width is kept clear of stems).
		auto ManClear = [this](V2 P, V2 Along) {
			const std::vector<uint32_t>* Near = WGrid.At(P.X, P.Y);
			if (!Near) return true;
			for (const uint32_t W : *Near)
			{
				if (WayHalf[W] >= VehicleHalf) continue;
				double T;
				const V2 Dir = Unit2(WayB[W] - WayA[W]);
				const bool bAcross = std::fabs(Dir.X * Along.Y - Dir.Y * Along.X) > 0.5;
				if (SegDist(P, WayA[W], WayB[W], T) < WayHalf[W] + (bAcross ? 1.7 : 0.35)) return false;
			}
			return true;
		};
		// The hedges. A hedgerow's bank and a lane's carry a dense hedge with no gap but the gateways and the ways: its
		// shrubs a metre and a half apart along the bank's top (so its faces of earth and grass show under them), its
		// height running smoothly along it, 2 to 2.8 m over the bank (so it reads as one green wall, not a row of bushes),
		// and a tree every 5 to 15 m, oak, elm and ash and many of them pollards, 10 to 15 m over the field. A wood's
		// edge and a garden's hedge are lower and looser.
		for (const Run& Rn : Runs)
		{
			Rng R(Rn.Key ^ 0x9A17);
			const double Total = LineLength(Rn.P);
			const bool bGarden = Rn.Kind == HkGarden, bBocage = Rn.Kind == HkHedgerow || Rn.Kind == HkLaneBank;
			const double Thin = bBocage ? 0.0 : R.Next() < 0.15 ? 0.3 : 0.06;
			const bool bPollards = bBocage && R.Next() < 0.2;
			const double TreeStep = bGarden ? 1e9 : !bBocage ? 45.0 : bPollards ? R.Range(6.0, 8.5) : R.Range(7.5, 11.0);
			const double Level = R.Range(2.0, 2.5), Along = static_cast<double>(Rn.Key % 1013);
			const double Lo = bGarden ? 1.4 : 1.6, Hi = bGarden ? 2.0 : 3.2;
			size_t Seg = 0;
			double Before = 0.0;
			auto PointAt = [&](double S, V2& Way) {
				while (Seg + 2 < Rn.P.size() && Before + Len(Rn.P[Seg + 1] - Rn.P[Seg]) < S)
				{
					Before += Len(Rn.P[Seg + 1] - Rn.P[Seg]);
					++Seg;
				}
				const double L = Len(Rn.P[Seg + 1] - Rn.P[Seg]);
				Way = Unit2(Rn.P[Seg + 1] - Rn.P[Seg]);
				return Lerp2(Rn.P[Seg], Rn.P[Seg + 1], L > 1e-9 ? Clamp01((S - Before) / L) : 0.0);
			};
			// The shrubs: their kind changes every dozen metres or so, as a hedge's planting does.
			Base Kind = Base::ShrubA;
			double NextKind = 0.0;
			const double Start = bBocage ? R.Range(0.6, 1.0) : R.Range(0.3, 1.0), End = bBocage ? 0.6 : 0.3;
			for (double S = Start; S < Total - End; S += bGarden ? R.Range(1.1, 1.5) : bBocage ? R.Range(1.15, 1.6) : R.Range(1.9, 2.7))
			{
				if (S >= NextKind)
				{
					const double U = R.Next();
					Kind = bGarden ? Base::ShrubB : U < 0.45 ? Base::ShrubA : U < 0.8 ? Base::ShrubB : Base::ShrubC;
					NextKind = S + R.Range(6.0, 18.0);
				}
				if (Thin > 0.0 && R.Next() < Thin) continue;
				V2 Way;
				const V2 P = PointAt(S, Way) + Perp(Way) * (bBocage ? R.Range(-0.28, 0.28) : R.Range(-0.35, 0.35));
				if (bBocage ? !Clear(P.X, P.Y, OcHouse | OcNear | OcWall | OcVehicle) || !ManClear(P, Way) : !Clear(P.X, P.Y, OcHouse | OcNear | OcWall | OcMan | OcVehicle)) continue;
				const double Shade = R.Range(0.9, 1.1);
				double Scale, High;
				if (bBocage)
				{
					Scale = R.Range(0.78, 0.95);
					High = Level + 0.3 * Noise(S / 13.0, Along, Seed ^ 0x9A18) + R.Range(-0.12, 0.12);
				}
				else
				{
					Scale = R.Range(1.0, 1.45);
					High = R.Range(Lo, Hi);
				}
				Plant(Kind, P.X, P.Y, R.Range(0.0, 2.0 * Pi), Scale, High / ShrubHigh, Tint8(Shade * R.Range(0.95, 1.05), Shade, Shade * R.Range(0.92, 1.05)));
				// Brambles and nettles spill from the bank's foot, here and there.
				if (!bGarden && R.Next() < (bBocage ? 0.12 : 0.22))
				{
					const double Foot = 0.5 * Rn.W[std::min(Seg, Rn.W.size() - 1)] * (R.Next() < 0.5 ? -1.0 : 1.0);
					const V2 F = P + Perp(Way) * Foot;
					if (Clear(F.X, F.Y, OcHouse | OcNear | OcWall | OcVehicle) && LaneD[Pix(F.X, F.Y)] > 0.8f)
						Plant(Base::Bramble, F.X, F.Y, R.Range(0.0, 2.0 * Pi), R.Range(0.7, 1.2), R.Range(0.6, 1.1), Tint8(1, 1, 1));
				}
			}
			// The tall trees.
			Seg = 0;
			Before = 0.0;
			// (unevenly: two close here, a wider gap there, 4.5 to 15 m)
			for (double S = TreeStep * R.Range(0.3, 1.0); S < Total - 3.0; S += bBocage ? std::clamp(TreeStep * R.Range(0.5, 1.5), 4.5, 15.0) : TreeStep * R.Range(0.6, 1.4))
			{
				if (S < 3.0) continue;
				V2 Way;
				const V2 P = PointAt(S, Way) + Perp(Way) * R.Range(-0.3, 0.3);
				if (!Clear(P.X, P.Y, OcHouse | OcNear | OcWall | OcMan | OcVehicle)) continue;
				double High = 0.0;
				const Base B = bBocage ? BocageTree(R, bPollards, High) : HedgeTree(R, false, High);
				const double Spread = R.Range(0.85, 1.15);
				Plant(B, P.X, P.Y, R.Range(0.0, 2.0 * Pi), High / TreeHigh(B) * Spread, High / TreeHigh(B), Tint8(R.Range(0.92, 1.06), R.Range(0.94, 1.06), R.Range(0.9, 1.04)));
			}
		}
		// The woods: trees some 7.5 m apart (clearings here and there), undergrowth thick in their edges.
		{
			Rng R(Seed ^ 0x3005);
			const double Step = 7.5;
			for (double Y = RY0 + 0.5 * Step; Y < RY0 + NY * Res; Y += Step)
				for (double X = RX0 + 0.5 * Step; X < RX0 + NX * Res; X += Step)
				{
					if (Rgn[Pix(X, Y)] != RgWoods) continue;
					const double PX = X + R.Range(-2.2, 2.2), PY = Y + R.Range(-2.2, 2.2);
					if (!InRaster(PX, PY) || Rgn[Pix(PX, PY)] != RgWoods || DistE[Pix(PX, PY)] < 1.2f) continue;
					if (Noise(PX / 55.0, PY / 55.0, Seed ^ 0x3006) > 0.5 && R.Next() < 0.75) continue;
					if (!Clear(PX, PY, OcHouse | OcNear | OcWall | OcMan | OcVehicle) || LaneD[Pix(PX, PY)] < LaneWall + 1.5f) continue;
					double High = 0.0;
					const Base B = HedgeTree(R, true, High);
					Plant(B, PX, PY, R.Range(0.0, 2.0 * Pi), High / TreeHigh(B) * R.Range(0.85, 1.1), High / TreeHigh(B), Tint8(R.Range(0.9, 1.05), R.Range(0.92, 1.06), R.Range(0.9, 1.04)));
				}
			const double Under = 3.4;
			for (double Y = RY0 + 0.5 * Under; Y < RY0 + NY * Res; Y += Under)
				for (double X = RX0 + 0.5 * Under; X < RX0 + NX * Res; X += Under)
				{
					const size_t P = Pix(X, Y);
					if (Rgn[P] != RgWoods) continue;
					const bool bEdge = DistE[P] < 7.0f;
					if (R.Next() > (bEdge ? 0.7 : 0.06)) continue;
					const double PX = X + R.Range(-1.4, 1.4), PY = Y + R.Range(-1.4, 1.4);
					if (!InRaster(PX, PY) || Rgn[Pix(PX, PY)] != RgWoods || !Clear(PX, PY, OcHouse | OcNear | OcWall | OcVehicle)) continue;
					const double U = R.Next();
					const Base B = U < 0.45 ? Base::Bramble : U < 0.75 ? Base::ShrubB : Base::ShrubC;
					const double High = B == Base::Bramble ? R.Range(0.8, 1.5) : R.Range(1.6, 3.4) / ShrubHigh;
					Plant(B, PX, PY, R.Range(0.0, 2.0 * Pi), R.Range(0.8, 1.3), High, Tint8(R.Range(0.92, 1.04), R.Range(0.94, 1.06), 1.0));
				}
		}
		// Orchards in rows along their parcel; a lone oak in some pastures; fruit trees in the gardens.
		{
			std::vector<double> X0(Parcels.size(), 1e9), Y0(Parcels.size(), 1e9), X1(Parcels.size(), -1e9), Y1(Parcels.size(), -1e9);
			for (int J = 0; J < NY; ++J)
				for (int I = 0; I < NX; ++I)
				{
					const size_t C = static_cast<size_t>(Pid[static_cast<size_t>(J) * NX + I]);
					if (!Parcels[C].bOrchard) continue;
					const double X = RX0 + (I + 0.5) * Res, Y = RY0 + (J + 0.5) * Res;
					X0[C] = std::min(X0[C], X);
					Y0[C] = std::min(Y0[C], Y);
					X1[C] = std::max(X1[C], X);
					Y1[C] = std::max(Y1[C], Y);
				}
			for (size_t C = 0; C < Parcels.size(); ++C)
			{
				const ParcelInfo& Q = Parcels[C];
				Rng R(Q.Key ^ 0x0AC4);
				if (Q.bOrchard)
				{
					const double Ca = std::cos(Q.Angle), Sa = std::sin(Q.Angle);
					const double Reach = std::hypot(X1[C] - X0[C], Y1[C] - Y0[C]) * 0.5 + 10.0;
					const double MX = 0.5 * (X0[C] + X1[C]), MY = 0.5 * (Y0[C] + Y1[C]);
					for (double V = -Reach; V <= Reach; V += 8.0)
						for (double U = -Reach + (std::fmod(std::fabs(V), 16.0) < 8.0 ? 0.0 : 4.5); U <= Reach; U += 9.0)
						{
							const double PX = MX + U * Ca - V * Sa + R.Range(-0.6, 0.6), PY = MY + U * Sa + V * Ca + R.Range(-0.6, 0.6);
							if (!InRaster(PX, PY)) continue;
							const size_t P = Pix(PX, PY);
							if (static_cast<size_t>(Pid[P]) != C || DistE[P] < 3.5f || !Clear(PX, PY, OcHouse | OcNear | OcWall | OcMan | OcVehicle)) continue;
							if (R.Next() < 0.06) continue; // a tree gone
							const double High = R.Range(4.2, 6.5);
							Plant(Base::Apple, PX, PY, R.Range(0.0, 2.0 * Pi), High / TreeHigh(Base::Apple) * R.Range(0.9, 1.15), High / TreeHigh(Base::Apple), Tint8(R.Range(0.94, 1.06), 1.0, R.Range(0.94, 1.04)));
						}
				}
				else if (Q.Kind == RgOpen && Q.Area > 6000.0 && Unit(Q.Key ^ 0xF1E1D) < 0.2)
				{
					const double PX = Q.CX + R.Range(-15.0, 15.0), PY = Q.CY + R.Range(-15.0, 15.0);
					if (InRaster(PX, PY) && static_cast<size_t>(Pid[Pix(PX, PY)]) == C && DistE[Pix(PX, PY)] > 18.0f && Clear(PX, PY, OcHouse | OcNear | OcWall | OcMan | OcVehicle))
					{
						const double High = R.Range(14.0, 19.0);
						Plant(Base::Oak, PX, PY, R.Range(0.0, 2.0 * Pi), High / 15.0 * R.Range(1.0, 1.2), High / 15.0, Tint8(1, 1, 1));
					}
				}
				else if (Q.Kind == RgVillage && Q.Area > 0.0)
				{
					const int N = static_cast<int>(Q.Area / 420.0);
					for (int K = 0; K < N; ++K)
					{
						const double PX = Q.CX + R.Range(-25.0, 25.0), PY = Q.CY + R.Range(-25.0, 25.0);
						if (!InRaster(PX, PY) || static_cast<size_t>(Pid[Pix(PX, PY)]) != C || DistE[Pix(PX, PY)] < 3.0f || !Clear(PX, PY, OcHouse | OcNear | OcWall | OcMan | OcVehicle)) continue;
						if (R.Next() < 0.6)
						{
							const double High = R.Range(4.0, 6.0);
							Plant(Base::Apple, PX, PY, R.Range(0.0, 2.0 * Pi), High / TreeHigh(Base::Apple), High / TreeHigh(Base::Apple), Tint8(1, 1, 1));
						}
						else Plant(Base::ShrubB, PX, PY, R.Range(0.0, 2.0 * Pi), R.Range(0.8, 1.1), R.Range(1.5, 2.4) / ShrubHigh, Tint8(1, 1.03, 1));
					}
				}
			}
		}
		// The yards' wells and carts.
		for (const Spot& S : Props)
		{
			if (S.What == 0) Plant(Base::Well, S.P.X, S.P.Y, S.Yaw, 1.0, 1.0, Tint8(1, 1, 1));
			else if (S.What == 1) Plant(Base::Cart, S.P.X, S.P.Y, S.Yaw, 1.0, 1.0, Tint8(1, 1, 1));
		}
		for (const std::vector<Placed>& L : Inst) Count.Instances += L.size();
	}
}

namespace LandField
{
	namespace
	{
		// July in the Normandy bocage at nine in the morning, as the eye sees it (sRGB). Every one is the art director's
		// to check; the battle camera's grade greys them (grit direction, item 2), so they are not pre-greyed here.
		const Col PastureGrazed = Rgb(92, 114, 58);  // grazed grass, short, a little yellow in the sun
		const Col PastureLush = Rgb(76, 102, 50);    // a wetter, ungrazed pasture
		const Col HayCut = Rgb(150, 148, 92);        // a meadow mown in June: pale aftermath, hay-coloured
		const Col Meadow = Rgb(126, 128, 74);        // standing grass gone to seed: buff over green
		const Col Wheat = Rgb(166, 140, 76);         // wheat ripening, gold with green in it (a stand of it, its stalks shading one another)
		const Col Barley = Rgb(184, 164, 104);       // barley, ripe and paler
		const Col Oats = Rgb(150, 144, 84);          // oats, greener gold
		const Col GreenWheat = Rgb(120, 128, 68);    // a late field, still green
		const Col Stubble = Rgb(194, 176, 128);      // barley cut: pale stubble
		const Col Roots = Rgb(74, 98, 50);           // beet or potatoes in their rows
		const Col RoughGrass = Rgb(118, 120, 72);    // tussocky rough grazing
		const Col BrambleGreen = Rgb(66, 80, 42);    // brambles, nettles
		const Col Litter = Rgb(72, 62, 44);          // a wood's floor: last year's leaves
		const Col WoodGreen = Rgb(60, 78, 40);       // ivy and dog's mercury on a wood's floor
		const Col Earth = Rgb(112, 92, 68);          // bare earth: banks, lane walls, gateways
		const Col Dust = Rgb(156, 138, 106);         // a lane's dry floor
		const Col RutCol = Rgb(118, 102, 80);        // its wheel ruts
		const Col LaneGrass = Rgb(102, 116, 62);     // the grass down a lane's middle and at its walls' feet
		const Col Churned = Rgb(100, 86, 64);        // fieldworks: dug and trodden
		const Col Spoil = Rgb(130, 112, 84);         // thrown-up spoil
		const Col Garden = Rgb(86, 106, 54);         // gardens and orchards' grass
		const Col YardEarth = Rgb(120, 106, 86);     // a farmyard: trodden earth, straw
		const Col Cobbles = Rgb(134, 128, 116);      // cobbles by the houses
		const Col Muck = Rgb(88, 76, 58);            // the yard's midden and mud
		const Col Margin = Rgb(84, 104, 50);         // field margins: docks, nettles, cow parsley
		const Col HedgeFoot = Rgb(60, 68, 40);       // under the hedge: shade and litter
		const Col BankGrass = Rgb(82, 104, 48);      // a bank's grass and ferns
		const Col Fern = Rgb(96, 124, 52);           // bracken on the banks
		const Col WaterCol = Rgb(58, 66, 64);
		const Col MarshCol = Rgb(86, 96, 62);
		const Col FarHedge = Rgb(50, 66, 34);        // a hedge seen from far off
		const Col CanopyCol = Rgb(52, 70, 34);          // a wood's top from far off

		Col CropColour(int Crop)
		{
			switch (Crop)
			{
			case CpGrazed: return PastureGrazed;
			case CpLush: return PastureLush;
			case CpHayCut: return HayCut;
			case CpMeadow: return Meadow;
			case CpWheat: return Wheat;
			case CpBarley: return Barley;
			case CpOats: return Oats;
			case CpGreenWheat: return GreenWheat;
			case CpStubble: return Stubble;
			case CpRoots: return Roots;
			case CpRough: return RoughGrass;
			case CpWorks: return Churned;
			case CpGarden: return Garden;
			case CpYard: return YardEarth;
			case CpWoods: return Litter;
			default: return PastureGrazed;
			}
		}
		bool IsCorn(int Crop) { return Crop == CpWheat || Crop == CpBarley || Crop == CpOats || Crop == CpGreenWheat; }

		/** Writes meshes: points with their normals and colours, triangles counter-clockwise seen from their front. */
		struct Builder
		{
			Mesh& M;
			explicit Builder(Mesh& InM) : M(InM) {}
			uint32_t Vert(V3 P, V3 N, const Col& C)
			{
				M.Positions.push_back(static_cast<float>(P.X));
				M.Positions.push_back(static_cast<float>(P.Y));
				M.Positions.push_back(static_cast<float>(P.Z));
				M.Normals.push_back(static_cast<float>(N.X));
				M.Normals.push_back(static_cast<float>(N.Y));
				M.Normals.push_back(static_cast<float>(N.Z));
				M.Colours.push_back(Byte(C));
				return static_cast<uint32_t>(M.Colours.size() - 1);
			}
			void Tri(uint32_t A, uint32_t B, uint32_t C)
			{
				M.Triangles.push_back(A);
				M.Triangles.push_back(B);
				M.Triangles.push_back(C);
			}
			/** A flat quad, its corners in order round it, turned to face Toward. */
			void Quad(V3 A, V3 B, V3 C, V3 D, Col CA, Col CB, Col CC, Col CD, V3 Toward)
			{
				V3 N = Cross(B - A, C - A);
				if (Dot3(N, N) < 1e-14) N = Cross(C - A, D - A);
				if (Dot3(N, N) < 1e-14) return;
				N = Unit3(N);
				if (Dot3(N, Toward) < 0.0)
				{
					std::swap(B, D);
					std::swap(CB, CD);
					N = N * -1.0;
				}
				const uint32_t IA = Vert(A, N, CA), IB = Vert(B, N, CB), IC = Vert(C, N, CC), ID = Vert(D, N, CD);
				Tri(IA, IB, IC);
				Tri(IA, IC, ID);
			}
			void Quad1(V3 A, V3 B, V3 C, V3 D, const Col& C1, V3 Toward) { Quad(A, B, C, D, C1, C1, C1, C1, Toward); }
			void Tri1(V3 A, V3 B, V3 C, const Col& C1, V3 Toward)
			{
				V3 N = Cross(B - A, C - A);
				if (Dot3(N, N) < 1e-14) return;
				N = Unit3(N);
				if (Dot3(N, Toward) < 0.0)
				{
					std::swap(B, C);
					N = N * -1.0;
				}
				const uint32_t IA = Vert(A, N, C1), IB = Vert(B, N, C1), IC = Vert(C, N, C1);
				Tri(IA, IB, IC);
			}
			/** A box: its middle and three half-extent vectors; the bottom face left off unless asked. */
			void Box(V3 O, V3 AX, V3 AY, V3 AZ, const Col& C, bool bBottom = false, const Col* Top = nullptr)
			{
				const V3 P[8] = { O - AX - AY - AZ, O + AX - AY - AZ, O + AX + AY - AZ, O - AX + AY - AZ, O - AX - AY + AZ, O + AX - AY + AZ, O + AX + AY + AZ, O - AX + AY + AZ };
				Quad1(P[0], P[1], P[5], P[4], C, AY * -1.0);
				Quad1(P[1], P[2], P[6], P[5], C, AX);
				Quad1(P[2], P[3], P[7], P[6], C, AY);
				Quad1(P[3], P[0], P[4], P[7], C, AX * -1.0);
				Quad1(P[4], P[5], P[6], P[7], Top ? *Top : C, AZ);
				if (bBottom) Quad1(P[0], P[1], P[2], P[3], C, AZ * -1.0);
			}
		};

		/**
		 * A strip swept along a line: sections of points (each section across the line, its points in order from the
		 * line's right to its left), joined section to section. Normals from the strip itself, so a lane's walls and a
		 * bank's flanks are lit as their slopes are. Each pair of sections goes to the mesh Target names for their middle.
		 */
		struct Section
		{
			std::vector<V3> P;
			std::vector<Col> C;
		};
		void Sweep(const std::vector<Section>& S, const std::function<Mesh*(double X, double Y)>& Target)
		{
			if (S.size() < 2) return;
			const size_t K = S[0].P.size();
			std::vector<std::vector<V3>> N(S.size(), std::vector<V3>(K));
			for (size_t A = 0; A < S.size(); ++A)
				for (size_t B = 0; B < K; ++B)
				{
					const V3 Along = S[std::min(A + 1, S.size() - 1)].P[B] - S[A > 0 ? A - 1 : 0].P[B];
					const V3 Across = S[A].P[std::min(B + 1, K - 1)] - S[A].P[B > 0 ? B - 1 : 0];
					N[A][B] = Unit3(Cross(Along, Across));
				}
			for (size_t A = 0; A + 1 < S.size(); ++A)
			{
				const V3 Mid = (S[A].P[K / 2] + S[A + 1].P[K / 2]) * 0.5;
				Mesh* M = Target(Mid.X, Mid.Y);
				if (!M) continue;
				Builder B(*M);
				const uint32_t Base0 = static_cast<uint32_t>(M->Points());
				for (size_t Q = 0; Q < K; ++Q) B.Vert(S[A].P[Q], N[A][Q], S[A].C[Q]);
				for (size_t Q = 0; Q < K; ++Q) B.Vert(S[A + 1].P[Q], N[A + 1][Q], S[A + 1].C[Q]);
				for (size_t Q = 0; Q + 1 < K; ++Q)
				{
					const uint32_t PA = Base0 + static_cast<uint32_t>(Q), PB = PA + 1, PD = PA + static_cast<uint32_t>(K), PC = PD + 1;
					B.Tri(PA, PD, PC);
					B.Tri(PA, PC, PB);
				}
			}
		}

	}

	// ====================================================================== the ground's colour

	/**
	 * The ground's colour at a place: its parcel's crop, varied in patches at two sizes; corn banded along its ploughed
	 * lands and streaked along its rows; a greener, weedier margin round every field, darker in the hedge's shade; the
	 * woods' leaf litter with ivy; trodden gateways; the village's gardens and the yards' earth and cobbles.
	 */
	Col Ground::Impl::GroundColour(double X, double Y) const
	{
		if (!InRaster(X, Y)) return FarColour(X, Y);
		const size_t P = Pix(X, Y);
		const ParcelInfo& Q = Parcels[static_cast<size_t>(Pid[P])];
		const double Edge = Bilinear(DistE, X, Y);
		const double N1 = Fbm(X, Y, 30.0, 2, Seed ^ 0x11), N2 = Noise(X / 7.0, Y / 7.0, Seed ^ 0x12);
		const double Broad = Fbm(X, Y, 320.0, 2, Seed ^ 0x13);
		Col C;
		switch (Rgn[P])
		{
		case RgWoods:
			C = Mixed(Litter, WoodGreen, 0.35 + 0.5 * N1 + 0.4 * Smooth(7.0, 1.0, Edge));
			C = Varied(C, 1.0 + 0.1 * N2, 0.03 * N1);
			return C;
		case RgWater: return WaterCol;
		case RgMarsh: return Varied(MarshCol, 1.0 + 0.08 * N1, 0.0);
		case RgLane:
		{
			const double D = Bilinear(LaneD, X, Y);
			return D < 0.0 ? Varied(Dust, 1.0 + 0.06 * N2, 0.02) : Mixed(Earth, LaneGrass, 0.4 + 0.4 * N1);
		}
		case RgYard:
		{
			C = Mixed(YardEarth, Muck, Smooth(0.1, 0.5, N1));
			if (OccR[P] & OcNear) C = Mixed(C, Cobbles, 0.6);
			return Varied(C, 1.0 + 0.08 * N2, 0.02 * N1);
		}
		case RgVillage:
		{
			C = Q.bOrchard ? Varied(Garden, 1.06, 0.02) : Mixed(Garden, Rgb(104, 112, 60), 0.5 + 0.5 * N1);
			if (OccR[P] & OcNear) C = Mixed(C, Unit(Q.Key) < 0.5 ? Cobbles : YardEarth, 0.7);
			return Varied(C, 1.0 + 0.08 * N2 + 0.05 * Broad, 0.02 * N1);
		}
		case RgWorks:
			C = Mixed(Churned, Spoil, Smooth(0.0, 0.45, N1 + 0.3 * N2));
			C = Mixed(C, RoughGrass, 0.35 * Smooth(0.2, -0.3, N1));
			return Varied(C, 1.0 + 0.07 * N2, 0.0);
		default: break;
		}
		C = CropColour(Q.Crop);
		if (Q.Kind == RgRough)
		{
			C = Mixed(C, BrambleGreen, Smooth(0.15, 0.45, N1));
		}
		else if (Q.Kind == RgField)
		{
			// Corn: banded along the ploughed lands, streaked along the drill rows, lodged and weedy in patches.
			const double Ca = std::cos(Q.Angle), Sa = std::sin(Q.Angle);
			const double Along = (X - Q.CX) * Ca + (Y - Q.CY) * Sa, Across = -(X - Q.CX) * Sa + (Y - Q.CY) * Ca;
			const double Lands = std::cos(2.0 * Pi * Across / Q.Lands);
			const double Streak = Noise(Along / 40.0, Across / 2.5, Seed ^ 0x14);
			// (strong enough to read from a hill or a turret, or a field of corn is a flat yellow sheet)
			C = Varied(C, 1.0 + 0.09 * Lands + 0.08 * Streak + 0.1 * N1, 0.0);
			if (IsCorn(Q.Crop)) C = Mixed(C, GreenWheat, 0.4 * Smooth(0.2, 0.6, N2 + 0.6 * N1));
			if (Q.Crop == CpStubble) C = Mixed(C, Margin, 0.25 * Smooth(0.2, 0.6, N1));
			C = Mixed(C, Margin, 0.55 * Smooth(5.0, 1.5, Edge)); // the headland: thin corn, weeds
		}
		else
		{
			// Grass: darker tufts where the cattle left it, paler where they grazed it down; thistles and docks.
			C = Varied(C, 1.0 + 0.08 * N1, 0.03 * N1);
			C = Mixed(C, Q.Crop == CpHayCut ? Meadow : PastureLush, 0.3 * Smooth(0.2, 0.55, N2));
			C = Mixed(C, Margin, 0.45 * Smooth(5.0, 1.0, Edge));
		}
		C = Mixed(C, HedgeFoot, 0.55 * Smooth(2.6, 0.6, Edge));
		if (OccR[P] & OcGate) C = Mixed(C, Earth, 0.45 * (0.6 + 0.4 * N2));
		return Varied(C, 1.0 + 0.06 * N2 + 0.05 * Broad, 0.02 * Broad);
	}

	/** Beyond the band: the same parcels by their seeds, their crops, woods; hedges as a darkening along the parcels' edges. */
	Col Ground::Impl::FarColour(double X, double Y) const
	{
		const double Broad = Fbm(X, Y, 500.0, 2, Seed ^ 0x15), N1 = Fbm(X, Y, 90.0, 2, Seed ^ 0x16);
		if (WoodsBeyond(X, Y)) return Varied(CanopyCol, 1.0 + 0.12 * N1, 0.02 * Broad);
		double Gap = 0.0;
		uint8_t Kind = RgOpen;
		const int S = Plot(X, Y, &Gap, &Kind);
		Col C = CropColour(CropOf(S, Kind));
		C = Varied(C, 1.0 + 0.08 * N1 + 0.05 * Broad, 0.02 * Broad);
		return Mixed(C, FarHedge, 0.5 * Smooth(9.0, 1.0, Gap));
	}

	// ====================================================================== the field's tiles

	namespace
	{
		struct GridSpec
		{
			double X0 = 0.0, Y0 = 0.0, SX = 250.0, SY = 250.0, Skirt = 1.0;
			int NX = 64, NY = 64;
		};

		/**
		 * A rectangle of ground as a grid: Height and Colour give each point; each quad split along its flatter diagonal;
		 * a skirt hangs from its edges (Skirt metres) to hide the cracks between tiles of different spacing.
		 */
		void GridMesh(Mesh& M, const GridSpec& G, const std::function<double(double, double)>& Height, const std::function<Col(double, double)>& Colour)
		{
			const int NX = G.NX, NY = G.NY, W = NX + 3;
			const double StepX = G.SX / NX, StepY = G.SY / NY;
			std::vector<double> Z(static_cast<size_t>(W) * (NY + 3));
			for (int J = -1; J <= NY + 1; ++J)
				for (int I = -1; I <= NX + 1; ++I) Z[static_cast<size_t>(J + 1) * W + (I + 1)] = Height(G.X0 + I * StepX, G.Y0 + J * StepY);
			auto ZAt = [&Z, W](int I, int J) { return Z[static_cast<size_t>(J + 1) * W + (I + 1)]; };
			Builder B(M);
			const uint32_t Base0 = static_cast<uint32_t>(M.Points());
			for (int J = 0; J <= NY; ++J)
				for (int I = 0; I <= NX; ++I)
				{
					const double X = G.X0 + I * StepX, Y = G.Y0 + J * StepY;
					const V3 Nrm = Unit3({ -(ZAt(I + 1, J) - ZAt(I - 1, J)) / (2.0 * StepX), -(ZAt(I, J + 1) - ZAt(I, J - 1)) / (2.0 * StepY), 1.0 });
					B.Vert({ X, Y, ZAt(I, J) }, Nrm, Colour(X, Y));
				}
			auto Id = [Base0, NX](int I, int J) { return Base0 + static_cast<uint32_t>(J * (NX + 1) + I); };
			for (int J = 0; J < NY; ++J)
				for (int I = 0; I < NX; ++I)
				{
					const uint32_t A = Id(I, J), Bq = Id(I + 1, J), C = Id(I + 1, J + 1), D = Id(I, J + 1);
					if (std::fabs(ZAt(I, J) - ZAt(I + 1, J + 1)) <= std::fabs(ZAt(I + 1, J) - ZAt(I, J + 1)))
					{
						B.Tri(A, Bq, C);
						B.Tri(A, C, D);
					}
					else
					{
						B.Tri(A, Bq, D);
						B.Tri(Bq, C, D);
					}
				}
			if (G.Skirt <= 0.0) return;
			// The skirt, round the edge counter-clockwise, hanging outward-facing.
			std::vector<std::pair<int, int>> Ring;
			for (int I = 0; I < NX; ++I) Ring.push_back({ I, 0 });
			for (int J = 0; J < NY; ++J) Ring.push_back({ NX, J });
			for (int I = NX; I > 0; --I) Ring.push_back({ I, NY });
			for (int J = NY; J > 0; --J) Ring.push_back({ 0, J });
			const size_t R = Ring.size();
			std::vector<uint32_t> Low(R);
			for (size_t K = 0; K < R; ++K)
			{
				const uint32_t Up = Id(Ring[K].first, Ring[K].second);
				const V3 P{ M.Positions[Up * 3], M.Positions[Up * 3 + 1], M.Positions[Up * 3 + 2] - G.Skirt };
				const V3 Nrm{ M.Normals[Up * 3], M.Normals[Up * 3 + 1], M.Normals[Up * 3 + 2] };
				Low[K] = B.Vert(P, Nrm, Col{ static_cast<double>(M.Colours[Up].R), static_cast<double>(M.Colours[Up].G), static_cast<double>(M.Colours[Up].B) });
			}
			for (size_t K = 0; K < R; ++K)
			{
				const size_t L = (K + 1) % R;
				const uint32_t A = Id(Ring[K].first, Ring[K].second), Bq = Id(Ring[L].first, Ring[L].second);
				// (seen from outside the tile, the edge runs left to right as the ring goes counter-clockwise)
				B.Tri(A, Low[K], Low[L]);
				B.Tri(A, Low[L], Bq);
			}
		}

		void Bounds(Tile& T)
		{
			float Lo = 1e30f, Hi = -1e30f;
			for (const Mesh* M : { &T.Near, &T.Far, &T.Built })
				for (size_t K = 2; K < M->Positions.size(); K += 3)
				{
					Lo = std::min(Lo, M->Positions[K]);
					Hi = std::max(Hi, M->Positions[K]);
				}
			T.ZMin = Lo <= Hi ? Lo : 0.0f;
			T.ZMax = Lo <= Hi ? Hi : 0.0f;
		}
	}
}

namespace LandField
{
	namespace
	{
		/** A hole in a wall: along it from A0 to A1, up from Z0 to Z1. What: 0 glass, 1 door, 2 shutters shut, 3 belfry louvre, 4 cart door. */
		struct Opening
		{
			double A0 = 0.0, A1 = 0.0, Z0 = 0.0, Z1 = 0.0;
			int What = 0;
		};

		/** A building's paints: grey-cream limestone, its dressed quoins and reveals, slate or tile, shutters and doors. */
		struct Look
		{
			Col Stone, Dressed, Roof, Inside, Glass, Shutter, Door, Frame;
		};

		Col StoneAt(const Look& L, V3 P, double Floor, uint64_t Seed)
		{
			const double Mottle = Noise((P.X + P.Y) / 0.9, P.Z / 0.55, Seed ^ 0x51);
			const double Streak = Noise((P.X - P.Y) / 0.7, P.Z / 5.0, Seed ^ 0x52);
			Col C = Varied(L.Stone, 1.0 + 0.07 * Mottle - 0.06 * Smooth(0.15, 0.7, Streak), 0.02 * Mottle);
			// Damp and splash at the foot: darker and greener.
			return Mixed(C, Rgb(96, 98, 80), 0.45 * Smooth(0.9, 0.05, P.Z - Floor));
		}

		/** A low heap of fallen stone and earth (a broken gable's rubble), lumpy, its foot sunk. */
		void Mound(Builder& B, V3 O, double RX, double RY, double High, double Yaw, const Col& Stone, uint64_t Seed)
		{
			const int NA = 12, NR = 4;
			const double C = std::cos(Yaw), S = std::sin(Yaw);
			std::vector<V3> P;
			std::vector<Col> K;
			for (int R = 0; R <= NR; ++R)
				for (int A = 0; A < NA; ++A)
				{
					const double T = static_cast<double>(R) / NR, Ang = 2.0 * Pi * A / NA;
					const double Lump = 1.0 + 0.25 * Noise(A * 0.9, R * 1.3, Seed);
					const double X = RX * T * std::cos(Ang) * Lump, Y = RY * T * std::sin(Ang) * Lump;
					const double Z = High * (1.0 - T * T) * (0.8 + 0.3 * Noise(A * 1.7, R * 0.7, Seed ^ 9)) - (R == NR ? 0.25 : 0.0);
					P.push_back({ O.X + X * C - Y * S, O.Y + X * S + Y * C, O.Z + Z });
					K.push_back(Varied(Mixed(Stone, Rgb(104, 92, 76), 0.35 + 0.3 * Noise(A * 2.1, R * 1.9, Seed ^ 3)), 0.85 + 0.2 * Unit(Hash(Seed, static_cast<uint64_t>(R * NA + A))), 0.0));
				}
			for (int R = 0; R < NR; ++R)
				for (int A = 0; A < NA; ++A)
				{
					const int A1 = (A + 1) % NA;
					const V3 P0 = P[static_cast<size_t>(R * NA + A)], P1 = P[static_cast<size_t>(R * NA + A1)], P2 = P[static_cast<size_t>((R + 1) * NA + A1)], P3 = P[static_cast<size_t>((R + 1) * NA + A)];
					B.Quad(P0, P1, P2, P3, K[static_cast<size_t>(R * NA + A)], K[static_cast<size_t>(R * NA + A1)], K[static_cast<size_t>((R + 1) * NA + A1)], K[static_cast<size_t>((R + 1) * NA + A)], { 0, 0, 1 });
				}
		}

		/**
		 * A building as a mesh: walls of limestone with their windows and doors set in (reveals, glass with its frame,
		 * shutters, doors), a steep roof with its overhang, thickness and ridge, chimneys at its gables. A church's nave,
		 * chancel and tower (with its belfry and slate spire) are three of these. Damage: a roof holed with its charred
		 * rafters showing; a gable broken with the roof's end gone and its rubble below; a shell burnt out to jagged walls.
		 */
		void BuildHouse(Mesh& M, const House& H)
		{
			Builder B(M);
			Rng R(H.Key ^ 0xB0D7);
			const bool bChurch = H.Type == HtChurch, bTower = bChurch && H.Part == 2;
			Look L;
			{
				const double Tone = R.Range(0.92, 1.06), Warm = R.Range(-0.04, 0.04);
				L.Stone = Varied(H.Type == HtBarn || H.Type == HtByre ? Rgb(162, 152, 134) : bChurch ? Rgb(184, 174, 152) : Rgb(176, 166, 146), Tone, Warm);
				if (!bChurch && R.Next() < 0.2) L.Stone = Varied(Rgb(192, 182, 160), Tone, 0.0); // limewashed
				L.Dressed = Mixed(L.Stone, Rgb(204, 196, 178), 0.6);
				L.Roof = H.Roof == 1 ? Varied(Rgb(148, 86, 64), R.Range(0.9, 1.08), 0.0) : Varied(Rgb(84, 88, 96), R.Range(0.92, 1.08), 0.0);
				L.Inside = Rgb(54, 50, 46);
				L.Glass = Rgb(36, 40, 46);
				static const Col Paints[4] = { Rgb(98, 112, 120), Rgb(94, 108, 90), Rgb(112, 74, 62), Rgb(168, 158, 136) };
				L.Shutter = Paints[R.Below(4)];
				L.Door = R.Next() < 0.5 ? Rgb(70, 54, 42) : Scaled(L.Shutter, 0.8);
				L.Frame = Rgb(176, 172, 160);
			}
			const V2 U{ std::cos(H.Yaw), std::sin(H.Yaw) }, V = Perp(U);
			auto W = [&](double Uu, double Vv, double Z) { return V3{ H.C.X + U.X * Uu + V.X * Vv, H.C.Y + U.Y * Uu + V.Y * Vv, Z }; };
			const double Long = H.Long, Deep = H.Deep, Floor = H.Floor, Thick = 0.5;
			double Eaves, Pitch;
			switch (H.Type)
			{
			case HtBarn:
				Eaves = Floor + 5.0;
				Pitch = 48.0;
				break;
			case HtByre:
				Eaves = Floor + 3.3;
				Pitch = 48.0;
				break;
			case HtChurch:
				Eaves = Floor + (H.Part == 0 ? 7.5 : H.Part == 1 ? 6.2 : 17.5);
				Pitch = 56.0;
				break;
			default:
				Eaves = Floor + H.Storeys * 2.75 + 0.35;
				Pitch = H.Roof == 1 ? 45.0 : 52.0;
				break;
			}
			const double Tan = std::tan(Pitch * Pi / 180.0);
			const double Ridge = Eaves + 0.5 * Deep * Tan;
			const bool bShell = H.Hurt == DmShell;
			const bool bRoof = !bShell && !bTower;
			const bool bInside = H.Hurt != DmNone;
			const int Broken = H.Hurt == DmGable ? (R.Next() < 0.5 ? -1 : 1) : 0;
			// The walls' top, along the house's length: whole, or broken down where it was hit.
			auto TopAtU = [&](double Uu) {
				if (bShell) return Eaves - 0.5 - 2.2 * (0.5 + 0.5 * Noise(Uu / 1.4, 3.0, H.Key)) - 0.4 * (0.5 + 0.5 * Noise(Uu / 0.35, 7.0, H.Key));
				if (Broken)
				{
					const double In = Broken * Uu - (0.5 * Long - 3.2);
					if (In > 0.0) return Eaves - std::min(1.0, In / 2.0) * (1.0 + 1.6 * (0.5 + 0.5 * Noise(Uu / 0.8, 5.0, H.Key))) - 0.3 * (0.5 + 0.5 * Noise(Uu / 0.3, 2.0, H.Key));
				}
				return 1e9;
			};
			// The four walls: v = +Deep/2 (out +V), v = -Deep/2 (out -V), and the gables u = +Long/2 (out +U), u = -Long/2 (out -U).
			struct Face
			{
				double U0, V0, DU, DV, Len;
				V2 Out;
				bool bGable;
				int End; // a gable's end, -1 or +1
			};
			const Face Faces[4] = {
				{ 0.5 * Long, 0.5 * Deep, -1, 0, Long, V, false, 0 },
				{ -0.5 * Long, -0.5 * Deep, 1, 0, Long, V * -1.0, false, 0 },
				{ 0.5 * Long, -0.5 * Deep, 0, 1, Deep, U, true, 1 },
				{ -0.5 * Long, 0.5 * Deep, 0, -1, Deep, U * -1.0, true, -1 },
			};
			for (int F = 0; F < 4; ++F)
			{
				const Face& Fc = Faces[F];
				const bool bFront = (F == 0 && H.Front > 0) || (F == 1 && H.Front < 0);
				auto At = [&](double A, double Z, double In) {
					return W(Fc.U0 + Fc.DU * A + (Fc.bGable ? -Fc.End * In : 0.0), Fc.V0 + Fc.DV * A + (!Fc.bGable ? -(F == 0 ? 1.0 : -1.0) * In : 0.0), Z);
				};
				auto UOf = [&](double A) { return Fc.U0 + Fc.DU * A; };
				// (a broken gable or a shell's gable is jagged across its width too)
				auto Top = [&](double A) {
					if (!Fc.bGable) return std::min(Eaves, TopAtU(UOf(A)));
					if (Broken != Fc.End && !bShell) return Eaves;
					return std::min(Eaves, TopAtU(UOf(A)) - 0.9 * (0.5 + 0.5 * Noise(A / 0.9, 11.0 + Fc.End, H.Key)));
				};
				// Its openings.
				std::vector<Opening> Ops;
				if (bTower)
				{
					for (int K = 0; K < 2; ++K)
					{
						const double A = Fc.Len * (K == 0 ? 0.32 : 0.68);
						Ops.push_back({ A - 0.32, A + 0.32, Eaves - 3.2, Eaves - 1.3, 3 });
					}
					if (F == 3) Ops.push_back({ 0.5 * Fc.Len - 0.8, 0.5 * Fc.Len + 0.8, Floor, Floor + 2.9, 1 });
					else Ops.push_back({ 0.5 * Fc.Len - 0.15, 0.5 * Fc.Len + 0.15, Floor + 6.0, Floor + 7.4, 0 });
				}
				else if (bChurch)
				{
					if (!Fc.bGable)
					{
						const int N = H.Part == 0 ? 3 : 1;
						for (int K = 0; K < N; ++K)
						{
							const double A = Fc.Len * (K + 0.5) / N;
							Ops.push_back({ A - 0.45, A + 0.45, Floor + 2.5, Floor + 5.6, 0 });
						}
						if (H.Part == 0 && F == 1) Ops.push_back({ Fc.Len * 0.33 - 0.6, Fc.Len * 0.33 + 0.6, Floor, Floor + 2.4, 1 });
					}
					else if (H.Part == 1 && Fc.End == 1) Ops.push_back({ 0.5 * Fc.Len - 0.65, 0.5 * Fc.Len + 0.65, Floor + 2.3, Floor + 5.4, 0 });
				}
				else if (H.Type == HtBarn)
				{
					if (bFront) Ops.push_back({ 0.5 * Fc.Len - 1.7, 0.5 * Fc.Len + 1.7, Floor, Floor + 3.7, 4 });
					if (Fc.bGable) Ops.push_back({ 0.5 * Fc.Len - 0.5, 0.5 * Fc.Len + 0.5, Floor + 3.4, Floor + 4.4, 2 });
				}
				else if (H.Type == HtByre)
				{
					if (bFront)
						for (int K = 0; K < 2; ++K)
						{
							const double A = Fc.Len * (K == 0 ? 0.3 : 0.72);
							Ops.push_back({ A - 0.55, A + 0.55, Floor, Floor + 2.05, 1 });
							Ops.push_back({ A + 1.2, A + 1.8, Floor + 1.2, Floor + 1.9, 2 });
						}
				}
				else
				{
					// A house: windows in rows on its long walls (fewer at the back), a door in the front, a window in some gables.
					const int Rows = H.Storeys;
					const int N = Fc.bGable ? 1 : std::max(1, static_cast<int>((Fc.Len - 1.5) / 3.1)) - (bFront ? 0 : (Fc.Len > 10.0 ? 1 : 0));
					const bool bGableWin = Fc.bGable && R.Next() < 0.5;
					const int Door = bFront ? R.Below(std::max(1, N + 1)) : -1;
					for (int Row = 0; Row < Rows && (!Fc.bGable || bGableWin); ++Row)
					{
						const double Z0 = Floor + Row * 2.75 + (Row == 0 ? 0.95 : 0.85), Z1 = Z0 + (Row == 0 ? 1.35 : 1.2);
						for (int K = 0; K < N + (bFront && Row == 0 ? 1 : 0); ++K)
						{
							const double A = Fc.Len * (K + 0.5) / (N + (bFront && Row == 0 ? 1 : 0));
							if (Row == 0 && K == Door) Ops.push_back({ A - 0.55, A + 0.55, Floor, Floor + 2.15, 1 });
							else Ops.push_back({ A - 0.5, A + 0.5, Z0, Z1, R.Next() < 0.3 ? 2 : 0 });
						}
					}
				}
				// An opening the broken wall's top runs through is gone with it.
				Ops.erase(std::remove_if(Ops.begin(), Ops.end(), [&](const Opening& O) { return std::min(Top(O.A0), Top(O.A1)) < O.Z1 + 0.25; }), Ops.end());
				const V3 Out3{ Fc.Out.X, Fc.Out.Y, 0.0 };
				// The face, cut round its openings into cells, each cell's corners its own stone.
				std::vector<double> AB{ 0.0, Fc.Len }, ZB{ H.Foot, Floor + 0.4, Eaves };
				for (double A = 1.5; A < Fc.Len - 0.3; A += 1.5) AB.push_back(A);
				for (int S = 1; S <= 6; ++S)
					if (Floor + S * 2.75 < Eaves - 0.3) ZB.push_back(Floor + S * 2.75);
				for (const Opening& O : Ops)
				{
					AB.push_back(O.A0);
					AB.push_back(O.A1);
					ZB.push_back(O.Z0);
					ZB.push_back(O.Z1);
				}
				std::sort(AB.begin(), AB.end());
				AB.erase(std::unique(AB.begin(), AB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.02; }), AB.end());
				std::sort(ZB.begin(), ZB.end());
				ZB.erase(std::unique(ZB.begin(), ZB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.02; }), ZB.end());
				const Col Scorch = Rgb(52, 46, 42);
				for (size_t I = 0; I + 1 < AB.size(); ++I)
					for (size_t J = 0; J + 1 < ZB.size(); ++J)
					{
						const double A0 = AB[I], A1 = AB[I + 1], Z0 = ZB[J], Z1 = ZB[J + 1];
						const double AM = 0.5 * (A0 + A1), ZM = 0.5 * (Z0 + Z1);
						bool bHole = false;
						for (const Opening& O : Ops)
							if (AM > O.A0 && AM < O.A1 && ZM > O.Z0 && ZM < O.Z1) bHole = true;
						if (bHole) continue;
						const double T0 = std::min(Z1, Top(A0)), T1 = std::min(Z1, Top(A1));
						if (T0 <= Z0 + 0.01 && T1 <= Z0 + 0.01) continue;
						const V3 P0 = At(A0, Z0, 0.0), P1 = At(A1, Z0, 0.0), P2 = At(A1, std::max(Z0, T1), 0.0), P3 = At(A0, std::max(Z0, T0), 0.0);
						auto Paint = [&](V3 P) {
							Col C = StoneAt(L, P, Floor, H.Key);
							if (bInside) C = Mixed(C, Scorch, 0.5 * Smooth(Eaves - 2.5, Eaves + 0.5, P.Z + 1.2 * Noise(P.X + P.Y, 1.0, H.Key)) * (bShell ? 1.0 : 0.6));
							return C;
						};
						B.Quad(P0, P1, P2, P3, Paint(P0), Paint(P1), Paint(P2), Paint(P3), Out3);
						if (bInside)
						{
							const V3 Q0 = At(A0, Z0, Thick), Q1 = At(A1, Z0, Thick), Q2 = At(A1, std::max(Z0, T1), Thick), Q3 = At(A0, std::max(Z0, T0), Thick);
							B.Quad1(Q0, Q1, Q2, Q3, L.Inside, Out3 * -1.0);
							if (T0 < Z1 - 0.01 || T1 < Z1 - 0.01) B.Quad1(P3, P2, Q2, Q3, Mixed(L.Stone, Scorch, 0.3), { 0, 0, 1 });
						}
					}
				// The openings: reveals of dressed stone, then glass with its frame, shutters, a door, a louvre.
				for (const Opening& O : Ops)
				{
					const double D = O.What == 4 ? 0.12 : 0.22;
					const V3 A0 = At(O.A0, O.Z0, 0.0), A1 = At(O.A1, O.Z0, 0.0), A2 = At(O.A1, O.Z1, 0.0), A3 = At(O.A0, O.Z1, 0.0);
					const V3 B0 = At(O.A0, O.Z0, D), B1 = At(O.A1, O.Z0, D), B2 = At(O.A1, O.Z1, D), B3 = At(O.A0, O.Z1, D);
					const V3 Along = Unit3(A1 - A0);
					B.Quad1(A0, B0, B3, A3, L.Dressed, Along);
					B.Quad1(A1, B1, B2, A2, L.Dressed, Along * -1.0);
					B.Quad1(A3, A2, B2, B3, Scaled(L.Dressed, 0.8), { 0, 0, -1 });
					B.Quad1(A0, A1, B1, B0, Scaled(L.Dressed, 1.05), { 0, 0, 1 });
					const Col Back = O.What == 0 ? L.Glass : O.What == 1 ? L.Door : O.What == 2 ? L.Shutter : O.What == 3 ? Rgb(50, 44, 40) : Rgb(86, 70, 56);
					B.Quad1(B0, B1, B2, B3, bShell && O.What != 3 ? Rgb(30, 28, 26) : Back, Out3);
					if (O.What == 0 && !bShell)
					{
						// The frame: a mullion and a transom just proud of the glass.
						const double AM = 0.5 * (O.A0 + O.A1), ZT = O.Z0 + 0.62 * (O.Z1 - O.Z0), Proud = D - 0.02;
						B.Quad1(At(AM - 0.035, O.Z0, Proud), At(AM + 0.035, O.Z0, Proud), At(AM + 0.035, O.Z1, Proud), At(AM - 0.035, O.Z1, Proud), L.Frame, Out3);
						B.Quad1(At(O.A0, ZT - 0.035, Proud), At(O.A1, ZT - 0.035, Proud), At(O.A1, ZT + 0.035, Proud), At(O.A0, ZT + 0.035, Proud), L.Frame, Out3);
					}
					if (O.What == 2 || O.What == 4)
					{
						// Planks: a darker line down the middle.
						const double AM = 0.5 * (O.A0 + O.A1), Proud = D - 0.01;
						B.Quad1(At(AM - 0.02, O.Z0, Proud), At(AM + 0.02, O.Z0, Proud), At(AM + 0.02, O.Z1, Proud), At(AM - 0.02, O.Z1, Proud), Scaled(Back, 0.6), Out3);
					}
				}
				// The gable's triangle up to the ridge, where the roof still stands over it.
				if (Fc.bGable && bRoof && Broken != Fc.End)
				{
					const V3 G0 = At(0.0, Eaves, 0.0), G1 = At(Fc.Len, Eaves, 0.0), G2 = At(0.5 * Fc.Len, Ridge, 0.0);
					const Col C0 = StoneAt(L, G0, Floor, H.Key), C1 = StoneAt(L, G1, Floor, H.Key), C2 = StoneAt(L, G2, Floor, H.Key);
					V3 N = Unit3(Cross(G1 - G0, G2 - G0));
					if (Dot3(N, Out3) < 0.0) N = N * -1.0;
					const uint32_t I0 = B.Vert(G0, N, C0), I1 = B.Vert(G1, N, C1), I2 = B.Vert(G2, N, C2);
					if (Dot3(Cross(G1 - G0, G2 - G0), Out3) > 0.0) B.Tri(I0, I1, I2);
					else B.Tri(I0, I2, I1);
					if (bInside) B.Tri1(At(0.0, Eaves, Thick), At(Fc.Len, Eaves, Thick), At(0.5 * Fc.Len, Ridge, Thick), L.Inside, Out3 * -1.0);
				}
			}
			if (bInside)
			{
				const double IU = 0.5 * Long - Thick, IV = 0.5 * Deep - Thick;
				B.Quad(W(-IU, -IV, Floor + 0.02), W(IU, -IV, Floor + 0.02), W(IU, IV, Floor + 0.02), W(-IU, IV, Floor + 0.02), Rgb(66, 58, 50), Rgb(80, 72, 62), Rgb(62, 56, 48), Rgb(74, 66, 56), { 0, 0, 1 });
				Mound(B, W(R.Range(-0.2, 0.2) * Long, 0.0, Floor - 0.1), 0.3 * Long, 0.3 * Deep, bShell ? 1.1 : 0.6, H.Yaw, L.Stone, H.Key ^ 0x60);
			}
			if (Broken) Mound(B, W(Broken * (0.5 * Long + 1.0), R.Range(-0.8, 0.8), Floor - 0.25), 2.4, 0.45 * Deep + 0.8, 1.0, H.Yaw, L.Stone, H.Key ^ 0x61);
			// The roof: two slopes of panels with their thickness, overhanging the walls; a hole or a broken end where hit.
			if (bRoof)
			{
				const double Over = 0.35, GOver = 0.25, Th = 0.14;
				const double U0 = -0.5 * Long - GOver, U1 = 0.5 * Long + GOver;
				const int NU = std::max(2, static_cast<int>(std::ceil((U1 - U0) / 1.4))), NV = 3;
				const double EaveZ = Eaves - Over * Tan;
				const int HoleSide = R.Next() < 0.5 ? -1 : 1;
				const int HoleI0 = R.Below(std::max(1, NU - 3)) + 1, HoleI1 = HoleI0 + 1 + R.Below(2), HoleJ0 = R.Below(2), HoleJ1 = HoleJ0 + R.Below(2);
				auto Missing = [&](int S, int I, int J) {
					if (H.Hurt == DmRoofHole && S == HoleSide && I >= HoleI0 && I <= HoleI1 && J >= HoleJ0 && J <= HoleJ1) return true;
					if (Broken)
					{
						const double UM = U0 + (U1 - U0) * (I + 0.5) / NU;
						const double Cut = 0.5 * Long - 2.6 - 1.4 * Unit(Hash(H.Key, static_cast<uint64_t>(S + 3), static_cast<uint64_t>(J)));
						if (Broken * UM > Cut) return true;
					}
					return false;
				};
				auto Near = [&](int S, int I, int J) {
					for (int DI = -1; DI <= 1; ++DI)
						for (int DJ = -1; DJ <= 1; ++DJ)
							if ((DI || DJ) && I + DI >= 0 && I + DI < NU && J + DJ >= 0 && J + DJ < NV && Missing(S, I + DI, J + DJ)) return true;
					return false;
				};
				for (int S = -1; S <= 1; S += 2)
				{
					auto RP = [&](double Uu, double T, double Down) { return W(Uu, S * (0.5 * Deep + Over) * (1.0 - T), EaveZ + T * (Ridge - EaveZ) - Down); };
					const V3 Up = Unit3(V3{ V.X * S * Tan, V.Y * S * Tan, 1.0 });
					for (int I = 0; I < NU; ++I)
						for (int J = 0; J < NV; ++J)
						{
							if (Missing(S, I, J)) continue;
							const double UA = U0 + (U1 - U0) * I / NU, UB = U0 + (U1 - U0) * (I + 1) / NU, TA = static_cast<double>(J) / NV, TB = static_cast<double>(J + 1) / NV;
							const uint64_t PH = Hash(H.Key, static_cast<uint64_t>(I * 8 + J), static_cast<uint64_t>(S + 2));
							Col C = Varied(L.Roof, 0.94 + 0.12 * Unit(PH), 0.0);
							if (J == 0 && Unit(Mix(PH)) < 0.5) C = Mixed(C, H.Roof == 1 ? Rgb(92, 84, 62) : Rgb(132, 132, 104), 0.3); // moss, lichen
							if (H.Hurt != DmNone && Near(S, I, J)) C = Mixed(C, Rgb(36, 32, 30), 0.55);
							const Col C2 = Varied(C, 1.04, 0.0);
							B.Quad(RP(UA, TA, 0.0), RP(UB, TA, 0.0), RP(UB, TB, 0.0), RP(UA, TB, 0.0), C, C, C2, C2, Up);
							B.Quad1(RP(UA, TA, Th), RP(UB, TA, Th), RP(UB, TB, Th), RP(UA, TB, Th), Rgb(70, 60, 52), Up * -1.0);
							if (J == 0) B.Quad1(RP(UA, 0.0, 0.0), RP(UB, 0.0, 0.0), RP(UB, 0.0, Th), RP(UA, 0.0, Th), Scaled(L.Roof, 0.7), V3{ V.X * S, V.Y * S, -0.3 });
							if (I == 0 || Missing(S, I - 1, J)) B.Quad1(RP(UA, TA, 0.0), RP(UA, TB, 0.0), RP(UA, TB, Th), RP(UA, TA, Th), Scaled(L.Roof, 0.75), V3{ -U.X, -U.Y, 0.0 });
							if (I == NU - 1 || Missing(S, I + 1, J)) B.Quad1(RP(UB, TA, 0.0), RP(UB, TB, 0.0), RP(UB, TB, Th), RP(UB, TA, Th), Scaled(L.Roof, 0.75), V3{ U.X, U.Y, 0.0 });
							if (J + 1 < NV && Missing(S, I, J + 1)) B.Quad1(RP(UA, TB, 0.0), RP(UB, TB, 0.0), RP(UB, TB, Th), RP(UA, TB, Th), Scaled(L.Roof, 0.7), Up * -1.0 + V3{ 0, 0, 0.5 });
							if (J > 0 && Missing(S, I, J - 1)) B.Quad1(RP(UA, TA, 0.0), RP(UB, TA, 0.0), RP(UB, TA, Th), RP(UA, TA, Th), Scaled(L.Roof, 0.7), V3{ V.X * S, V.Y * S, -0.3 });
						}
					// Charred rafters across what is open.
					if (H.Hurt == DmRoofHole || Broken)
						for (int I = 0; I < NU; ++I)
						{
							bool bOpen = false;
							for (int J = 0; J < NV; ++J) bOpen = bOpen || Missing(S, I, J);
							if (!bOpen) continue;
							for (double F = 0.25; F < 1.0; F += 0.5)
							{
								const double Uu = U0 + (U1 - U0) * (I + F) / NU;
								if (Broken && Broken * Uu > 0.5 * Long - 0.3) continue;
								const V3 Lo = RP(Uu, 0.05, Th + 0.06), Hi = RP(Uu, 0.97, Th + 0.06);
								const V3 Mid = (Lo + Hi) * 0.5, Half = (Hi - Lo) * 0.5;
								B.Box(Mid, Half, V3{ U.X * 0.05, U.Y * 0.05, 0.0 }, Unit3(Cross(Half, V3{ U.X, U.Y, 0.0 })) * 0.07, Rgb(38, 32, 28), true);
							}
						}
				}
				// The ridge cap.
				const double CU0 = Broken < 0 ? -0.5 * Long + 3.0 : U0, CU1 = Broken > 0 ? 0.5 * Long - 3.0 : U1;
				const Col Cap = H.Roof == 1 ? Scaled(L.Roof, 0.82) : Scaled(L.Roof, 0.8);
				for (int S = -1; S <= 1; S += 2)
					B.Quad1(W(CU0, 0.0, Ridge + 0.09), W(CU1, 0.0, Ridge + 0.09), W(CU1, S * 0.22, Ridge + 0.09 - 0.22 * Tan * 0.6), W(CU0, S * 0.22, Ridge + 0.09 - 0.22 * Tan * 0.6), Cap, V3{ V.X * S, V.Y * S, 1.5 });
				// Chimneys at the gables of a house (not a barn, a byre or a church).
				if (H.Type == HtHouse || H.Type == HtFarmhouse)
				{
					const int Ends = R.Next() < 0.6 ? 2 : 1;
					for (int E = 0; E < Ends; ++E)
					{
						const int End = Ends == 2 ? (E == 0 ? -1 : 1) : (R.Next() < 0.5 ? -1 : 1);
						if (End == Broken) continue;
						const V3 O = W(End * (0.5 * Long - 0.55), 0.0, Ridge + 0.05);
						const Col Ch = Varied(L.Stone, 0.92, 0.0);
						B.Box(O, V3{ U.X * 0.5, U.Y * 0.5, 0.0 }, V3{ V.X * 0.36, V.Y * 0.36, 0.0 }, V3{ 0, 0, 1.05 }, Ch, false);
						const Col Flue = Rgb(40, 36, 34);
						B.Box(O + V3{ 0, 0, 1.12 }, V3{ U.X * 0.56, U.Y * 0.56, 0.0 }, V3{ V.X * 0.42, V.Y * 0.42, 0.0 }, V3{ 0, 0, 0.07 }, Scaled(Ch, 0.85), true, &Flue);
					}
				}
			}
			// The tower's spire: slate, square, steep, with its iron cross.
			if (bTower)
			{
				const double S = 0.5 * Long + 0.2, Apex = Eaves + 10.0;
				const V3 Tip = W(0.0, 0.0, Apex);
				const V3 C4[4] = { W(-S, -S, Eaves - 0.1), W(S, -S, Eaves - 0.1), W(S, S, Eaves - 0.1), W(-S, S, Eaves - 0.1) };
				const Col Slate = Rgb(78, 82, 90);
				for (int K = 0; K < 4; ++K)
				{
					const V3 A = C4[K], Bq = C4[(K + 1) % 4];
					const V3 Mid = (A + Bq) * 0.5;
					B.Tri1(A, Bq, Tip, K % 2 ? Slate : Varied(Slate, 1.05, 0.0), Unit3(Mid - W(0.0, 0.0, Mid.Z)) + V3{ 0, 0, 0.2 });
				}
				const Col Iron = Rgb(40, 40, 42);
				B.Box(Tip + V3{ 0, 0, 0.6 }, V3{ 0.04, 0, 0 }, V3{ 0, 0.04, 0 }, V3{ 0, 0, 0.7 }, Iron);
				B.Box(Tip + V3{ 0, 0, 0.9 }, V3{ U.X * 0.3, U.Y * 0.3, 0 }, V3{ V.X * 0.04, V.Y * 0.04, 0 }, V3{ 0, 0, 0.04 }, Iron, true);
			}
		}
	}

	/**
	 * The field's tiles (and the band's): the ground at two levels; the lanes' floors, walls and ruts and the banks swept
	 * in the near level, the banks as ridges in the far; the walls, houses, barns and the church in Built.
	 */
	void Ground::Impl::MakeTiles()
	{
		const int TX = TilesX(), TY = TilesY();
		TileList.assign(static_cast<size_t>(TX) * TY, Tile());
		auto TileOf = [&](double X, double Y) -> Tile* {
			const int I = static_cast<int>(std::floor((X + Band) / TileSize)), J = static_cast<int>(std::floor((Y + Band) / TileSize));
			if (I < 0 || J < 0 || I >= TX || J >= TY) return nullptr;
			return &TileList[static_cast<size_t>(J) * TX + I];
		};
		auto NearZ = [this](double X, double Y) {
			double Edge = 1e6, Z = Terrain(X, Y);
			if (!InRaster(X, Y) || LaneD[Pix(X, Y)] < LaneWall + 3.0f) Z = Carve(X, Y, Z, &Edge);
			// Under a lane's mesh the ground drops a little more, so the lane is always the one seen.
			return Z - 0.35 * (1.0 - Smooth(LaneWall + 0.4, LaneWall + 1.0, Edge));
		};
		auto FarZ = [this](double X, double Y) { return Terrain(X, Y); };
		auto NearCol = [this](double X, double Y) { return GroundColour(X, Y); };
		for (int J = 0; J < TY; ++J)
			for (int I = 0; I < TX; ++I)
			{
				Tile& T = TileList[static_cast<size_t>(J) * TX + I];
				T.X0 = -Band + I * TileSize;
				T.Y0 = -Band + J * TileSize;
				T.X1 = T.X0 + TileSize;
				T.Y1 = T.Y0 + TileSize;
				const bool bInner = T.X0 >= 0.0 && T.Y0 >= 0.0 && T.X1 <= FW + 1e-6 && T.Y1 <= FH + 1e-6;
				GridSpec G;
				G.X0 = T.X0;
				G.Y0 = T.Y0;
				G.SX = G.SY = TileSize;
				G.NX = G.NY = bInner ? NearQuads : BandQuads;
				G.Skirt = 1.0;
				T.Spacing = TileSize / G.NX;
				GridMesh(T.Near, G, NearZ, NearCol);
				G.NX = G.NY = FarQuads;
				const double Sub = TileSize / FarQuads / 3.0;
				GridMesh(T.Far, G, FarZ, [this, Sub](double X, double Y) {
					// The far ground is the near ground seen from farther: each point the mean of the near colour round it.
					Col Sum;
					for (int B = -1; B <= 1; ++B)
						for (int A = -1; A <= 1; ++A)
						{
							const Col C = GroundColour(X + A * Sub, Y + B * Sub);
							Sum = { Sum.R + C.R / 9.0, Sum.G + C.G / 9.0, Sum.B + C.B / 9.0 };
						}
					return Sum;
				});
			}
		auto NearOf = [&TileOf](double X, double Y) -> Mesh* {
			Tile* T = TileOf(X, Y);
			return T ? &T->Near : nullptr;
		};
		auto FarOf = [&TileOf](double X, double Y) -> Mesh* {
			Tile* T = TileOf(X, Y);
			return T ? &T->Far : nullptr;
		};
		// The lanes: floor (crowned, rutted), walls up to the field, a strip of the field each side to meet the ground.
		for (const Lane& Ln : Lanes)
		{
			const double Half = Ln.Half;
			const double Offs[] = { 0.0, 0.45, 0.8, 1.15, Half, Half + 0.35 * LaneWall, Half + 0.7 * LaneWall, Half + LaneWall, Half + LaneWall + 0.9 };
			std::vector<double> D;
			for (int K = 8; K >= 1; --K) D.push_back(-Offs[K]);
			for (int K = 0; K <= 8; ++K) D.push_back(Offs[K]);
			std::vector<Section> Secs;
			for (size_t K = 0; K < Ln.P.size(); ++K)
			{
				const V2 Way = Unit2(Ln.P[std::min(K + 1, Ln.P.size() - 1)] - Ln.P[K > 0 ? K - 1 : 0]), N = Perp(Way);
				Section S;
				for (const double Off : D)
				{
					const V2 Q = Ln.P[K] + N * Off;
					double Edge = 1e6;
					const double Z = Carve(Q.X, Q.Y, Terrain(Q.X, Q.Y), &Edge);
					const double A = std::fabs(Off);
					const double N1 = Noise(Q.X / 3.0, Q.Y / 3.0, Seed ^ 0x21), N2 = Noise(Q.X / 0.9, Q.Y / 0.9, Seed ^ 0x22);
					Col C;
					if (A <= Half + 0.01)
					{
						C = Varied(Dust, 1.0 + 0.06 * N1 + 0.04 * N2, 0.02 * N1);
						C = Mixed(C, RutCol, std::exp(-Sq((A - 0.8) / 0.25)) * 0.8);
						if (Half < 2.5) C = Mixed(C, LaneGrass, 0.65 * std::exp(-Sq(A / 0.3)));
						C = Mixed(C, LaneGrass, 0.5 * Smooth(Half - 0.6, Half, A));
						if (Fd.bTown) C = Mixed(C, Cobbles, 0.5 * Smooth(Fd.TownR * 1.1, Fd.TownR * 0.6, std::hypot(Q.X - Fd.TownX, Q.Y - Fd.TownY)));
					}
					else if (A < Half + LaneWall + 0.01)
					{
						// The walls: earth and roots, ferns and grass where they hold.
						C = Mixed(Earth, Mixed(BankGrass, Fern, 0.5 + 0.5 * N2), 0.3 + 0.35 * (N1 + 0.5));
						C = Varied(C, 0.92 + 0.08 * N2, 0.0);
					}
					else C = GroundColour(Q.X, Q.Y);
					S.P.push_back({ Q.X, Q.Y, Z });
					S.C.push_back(C);
				}
				Secs.push_back(std::move(S));
			}
			Sweep(Secs, NearOf);
		}
		// The banks: swept at their own points, their feet sunk a little into the ground round them.
		for (const Run& Rn : Runs)
		{
			std::vector<Section> Secs, FarSecs;
			double SinceFar = 1e9;
			for (size_t K = 0; K < Rn.P.size(); ++K)
			{
				const V2 Way = Unit2(Rn.P[std::min(K + 1, Rn.P.size() - 1)] - Rn.P[K > 0 ? K - 1 : 0]), N = Perp(Way);
				const double Half = 0.5 * Rn.W[K], High = Rn.H[K];
				Section S;
				for (const double T : BankAcross)
				{
					const V2 Q = Rn.P[K] + N * (T * Half);
					double Z = Terrain(Q.X, Q.Y);
					if (!InRaster(Q.X, Q.Y) || LaneD[Pix(Q.X, Q.Y)] < LaneWall + 3.0f) Z = Carve(Q.X, Q.Y, Z, nullptr);
					const double A = std::fabs(T);
					Z += BankProfile(A, High);
					const double N1 = Noise(Q.X / 2.5, Q.Y / 2.5, Seed ^ 0x31), N2 = Noise(Q.X / 0.8, Q.Y / 0.8, Seed ^ 0x32);
					Col C;
					if (A > 1.0) C = GroundColour(Q.X, Q.Y);
					else if (A < BankTop + 0.05) C = Mixed(HedgeFoot, BrambleGreen, 0.5 + 0.5 * N1);
					else
					{
						C = Mixed(BankGrass, Fern, 0.5 + 0.6 * N2);
						// Bare earth where the bank is steep (a lane's side most of all) or worn.
						const double Steep = High / std::max(0.3, 0.35 * Half);
						C = Mixed(C, Earth, Clamp01(0.25 * Steep - 0.15 + 0.3 * N1) * (Rn.Kind == HkLaneBank ? 1.0 : 0.6));
					}
					S.P.push_back({ Q.X, Q.Y, Z });
					S.C.push_back(Varied(C, 0.94 + 0.1 * N2, 0.0));
				}
				Secs.push_back(S);
				// The far level: a ridge (its foot, its crest a little low, its foot) every 8 m or so.
				if (K > 0) SinceFar += Len(Rn.P[K] - Rn.P[K - 1]);
				if (SinceFar >= 8.0 || K + 1 == Rn.P.size())
				{
					SinceFar = 0.0;
					Section F;
					F.P = { S.P[0] + V3{ 0, 0, 0.15 }, (S.P[2] + S.P[3]) * 0.5 - V3{ 0, 0, 0.1 * High }, S.P[5] + V3{ 0, 0, 0.15 } };
					F.C = { S.C[0], Mixed(HedgeFoot, BankGrass, 0.4), S.C[5] };
					FarSecs.push_back(F);
				}
			}
			Sweep(Secs, NearOf);
			Sweep(FarSecs, FarOf);
		}
		// The stone walls: a face each side, a crowned coping, their ends squared.
		for (const WallRun& Wr : Walls)
		{
			if (Wr.P.size() < 2) continue;
			for (size_t K = 0; K + 1 < Wr.P.size(); ++K)
			{
				const V2 A = Wr.P[K], Bp = Wr.P[K + 1];
				Tile* T = TileOf(0.5 * (A.X + Bp.X), 0.5 * (A.Y + Bp.Y));
				if (!T) continue;
				Builder B(T->Built);
				const V2 N = Perp(Unit2(Bp - A)) * (0.5 * Wr.Thick);
				const double GA = Carve(A.X, A.Y, Terrain(A.X, A.Y), nullptr), GB = Carve(Bp.X, Bp.Y, Terrain(Bp.X, Bp.Y), nullptr);
				const double TA = GA + Wr.H[K], TB = GB + Wr.H[K + 1];
				const uint64_t Hk = Hash(Wr.Key, K);
				const Col Stone = Varied(Rgb(168, 160, 142), 0.9 + 0.16 * Unit(Hk), 0.03 * (Unit(Mix(Hk)) - 0.5));
				const double Ivy = Smooth(0.25, 0.6, Noise((A.X + A.Y) / 7.0, static_cast<double>(Wr.Key % 101), Seed ^ 0x41));
				const Col Face = Mixed(Stone, Rgb(58, 74, 40), 0.8 * Ivy), Foot = Mixed(Face, Rgb(96, 98, 78), 0.5);
				for (int S = -1; S <= 1; S += 2)
				{
					const V2 O = N * static_cast<double>(S);
					B.Quad(V3{ A.X + O.X, A.Y + O.Y, GA - 0.3 }, V3{ Bp.X + O.X, Bp.Y + O.Y, GB - 0.3 }, V3{ Bp.X + O.X, Bp.Y + O.Y, TB - 0.08 }, V3{ A.X + O.X, A.Y + O.Y, TA - 0.08 }, Foot, Foot, Face, Face, V3{ O.X, O.Y, 0.0 });
					B.Quad1(V3{ A.X + O.X, A.Y + O.Y, TA - 0.08 }, V3{ Bp.X + O.X, Bp.Y + O.Y, TB - 0.08 }, V3{ Bp.X, Bp.Y, TB + 0.04 }, V3{ A.X, A.Y, TA + 0.04 }, Varied(Stone, 1.08, 0.0), V3{ O.X, O.Y, 1.2 });
				}
				for (int E = 0; E < 2; ++E)
				{
					const bool bEnd = E == 0 ? K == 0 : K + 2 == Wr.P.size();
					if (!bEnd) continue;
					const V2 P = E == 0 ? A : Bp;
					const double G = E == 0 ? GA : GB, Tp = E == 0 ? TA : TB;
					const V2 Way = Unit2(Bp - A) * (E == 0 ? -1.0 : 1.0);
					B.Quad1(V3{ P.X - N.X, P.Y - N.Y, G - 0.3 }, V3{ P.X + N.X, P.Y + N.Y, G - 0.3 }, V3{ P.X + N.X, P.Y + N.Y, Tp - 0.08 }, V3{ P.X - N.X, P.Y - N.Y, Tp - 0.08 }, Stone, V3{ Way.X, Way.Y, 0.0 });
					B.Tri1(V3{ P.X - N.X, P.Y - N.Y, Tp - 0.08 }, V3{ P.X + N.X, P.Y + N.Y, Tp - 0.08 }, V3{ P.X, P.Y, Tp + 0.04 }, Stone, V3{ Way.X, Way.Y, 0.0 });
				}
			}
		}
		// The buildings.
		for (const House& H : Houses)
		{
			Tile* T = TileOf(H.C.X, H.C.Y);
			if (T) BuildHouse(T->Built, H);
		}
		for (Tile& T : TileList)
		{
			Bounds(T);
			Count.NearTris += T.Near.Tris();
			Count.FarTris += T.Far.Tris();
			Count.BuiltTris += T.Built.Tris();
		}
	}
}

namespace LandField
{
	// ====================================================================== the ring beyond

	/**
	 * A wood's top beyond the tiles, above the ground: its crowns 13 to 19 m up, rising over some ten metres at its
	 * edge, and not at all within 30 m of the tiles (where the band's woods stand as trees, so the two meet).
	 */
	double Ground::Impl::Canopy(double X, double Y) const
	{
		const double F = Fbm(X, Y, 520.0, 3, Seed ^ 0xD5);
		if (F <= 0.27) return 0.0;
		const double OutX = std::max({ 0.0, -Band - X, X - (TilesX() * TileSize - Band) });
		const double OutY = std::max({ 0.0, -Band - Y, Y - (TilesY() * TileSize - Band) });
		const double Crowns = 15.5 + 2.5 * Noise(X / 35.0, Y / 35.0, Seed ^ 0xD6) + 1.5 * Noise(X / 90.0, Y / 90.0, Seed ^ 0xDA);
		return Crowns * Smooth(0.27, 0.29, F) * Smooth(0.0, 30.0, std::hypot(OutX, OutY));
	}

	/**
	 * The ring: the country beyond the tiles out to RingOut past the field's edges, in tiles of about a kilometre whose
	 * ground is 12.5 m apart near the field and 25, 50 and 100 m farther out (the haze has it all by then); its parcels
	 * coloured by the same seeds as the band's, its woods raised to their canopy; and for the first kilometre its
	 * hedgerows as long ridges with their trees standing up from them, the layers of dark lines the bocage shows to
	 * the horizon. The edges between tiles of different spacing hide under skirts.
	 */
	void Ground::Impl::MakeRing()
	{
		const double IX0 = -Band, IY0 = -Band, IX1 = -Band + TilesX() * TileSize, IY1 = -Band + TilesY() * TileSize;
		constexpr double Step = 1000.0;
		const int Out = static_cast<int>(std::ceil(RingOut / Step - 1e-9));
		auto Edges = [Out](double Lo, double Hi) {
			std::vector<double> E;
			for (int K = Out; K >= 1; --K) E.push_back(Lo - K * Step);
			const int N = std::max(1, static_cast<int>(std::lround((Hi - Lo) / Step)));
			for (int K = 0; K <= N; ++K) E.push_back(Lo + (Hi - Lo) * K / N);
			for (int K = 1; K <= Out; ++K) E.push_back(Hi + K * Step);
			return E;
		};
		const std::vector<double> XE = Edges(IX0, IX1), YE = Edges(IY0, IY1);
		const int NXT = static_cast<int>(XE.size()) - 1, NYT = static_cast<int>(YE.size()) - 1;
		std::vector<int> Which(static_cast<size_t>(NXT) * NYT, -1);
		auto RingZ = [this](double X, double Y) { return Land(X, Y) + Canopy(X, Y); };
		auto RingCol = [this](double X, double Y) { return FarColour(X, Y); };
		for (int J = 0; J < NYT; ++J)
			for (int I = 0; I < NXT; ++I)
			{
				const double X0 = XE[static_cast<size_t>(I)], X1 = XE[static_cast<size_t>(I) + 1], Y0 = YE[static_cast<size_t>(J)], Y1 = YE[static_cast<size_t>(J) + 1];
				const double MX = 0.5 * (X0 + X1), MY = 0.5 * (Y0 + Y1);
				if (MX > IX0 && MX < IX1 && MY > IY0 && MY < IY1) continue;
				const double D = std::hypot(std::max({ 0.0, IX0 - X1, X0 - IX1 }), std::max({ 0.0, IY0 - Y1, Y0 - IY1 }));
				const double Spacing = D < 900.0 ? 12.5 : D < 1900.0 ? 25.0 : D < 3400.0 ? 50.0 : 100.0;
				Tile T;
				T.X0 = X0;
				T.Y0 = Y0;
				T.X1 = X1;
				T.Y1 = Y1;
				T.Spacing = Spacing;
				GridSpec G;
				G.X0 = X0;
				G.Y0 = Y0;
				G.SX = X1 - X0;
				G.SY = Y1 - Y0;
				G.NX = std::max(1, static_cast<int>(std::lround(G.SX / Spacing)));
				G.NY = std::max(1, static_cast<int>(std::lround(G.SY / Spacing)));
				G.Skirt = 2.0 + Spacing / 25.0;
				GridMesh(T.Near, G, RingZ, RingCol);
				Which[static_cast<size_t>(J) * NXT + I] = static_cast<int>(RingList.size());
				RingList.push_back(std::move(T));
			}
		auto RingOf = [&](double X, double Y) -> Mesh* {
			const auto IX = std::upper_bound(XE.begin(), XE.end(), X), IY = std::upper_bound(YE.begin(), YE.end(), Y);
			if (IX == XE.begin() || IX == XE.end() || IY == YE.begin() || IY == YE.end()) return nullptr;
			const int I = static_cast<int>(IX - XE.begin()) - 1, J = static_cast<int>(IY - YE.begin()) - 1;
			const int K = Which[static_cast<size_t>(J) * NXT + I];
			return K >= 0 ? &RingList[static_cast<size_t>(K)].Near : nullptr;
		};
		// The near ring's hedgerows: the parcels' edges traced on an 8 m raster a kilometre out from the tiles.
		constexpr double HR = 8.0, Reach = 1000.0;
		const double HX0 = IX0 - Reach, HY0 = IY0 - Reach;
		const int HNX = static_cast<int>(std::ceil((IX1 - IX0 + 2.0 * Reach) / HR)), HNY = static_cast<int>(std::ceil((IY1 - IY0 + 2.0 * Reach) / HR));
		std::vector<int32_t> Lab(static_cast<size_t>(HNX) * HNY);
		for (int J = 0; J < HNY; ++J)
			for (int I = 0; I < HNX; ++I)
			{
				const double X = HX0 + (I + 0.5) * HR, Y = HY0 + (J + 0.5) * HR;
				int32_t L;
				if (X > IX0 && X < IX1 && Y > IY0 && Y < IY1) L = -1;
				else if (WoodsBeyond(X, Y)) L = -2;
				else L = Plot(X, Y, nullptr, nullptr);
				Lab[static_cast<size_t>(J) * HNX + I] = L;
			}
		const Col RidgeCol = Rgb(66, 86, 42), CrownCol = Rgb(60, 82, 38);
		for (const Boundary& B : TraceEdges(Lab, HNX, HNY, HX0, HY0, HR, [](int32_t A, int32_t Bq) { return A >= 0 && Bq >= 0; }))
		{
			Line L = Chaikin(Simplify(B.P, 3.0), 2);
			if (LineLength(L) < 12.0) continue;
			L = Resample(L, 5.0);
			const uint64_t Key = Hash(Seed ^ 0xF4A2, static_cast<uint64_t>(B.A), static_cast<uint64_t>(B.B));
			Rng R(Key);
			std::vector<double> Trees;
			const double Total = LineLength(L);
			// (as near: a tree every 5 to 15 m, 10 to 15 m high, over a bank and hedge of 3 to 4 m)
			for (double S = R.Range(3.0, 12.0); S < Total - 3.0; S += R.Range(5.0, 15.0)) Trees.push_back(S);
			std::vector<double> TreeHi(Trees.size());
			for (double& T : TreeHi) T = R.Range(10.0, 15.0);
			std::vector<Section> Secs;
			double S = 0.0;
			for (size_t K = 0; K < L.size(); ++K)
			{
				if (K > 0) S += Len(L[K] - L[K - 1]);
				double High = 3.5 + 0.5 * Noise(S / 11.0, static_cast<double>(Key % 997), Seed ^ 0xF4A3);
				bool bTree = false;
				for (size_t T = 0; T < Trees.size(); ++T)
				{
					const double D = std::fabs(S - Trees[T]);
					if (D > 7.5) continue;
					const double Crown = TreeHi[T] * (1.0 - 0.45 * Sq(D / 7.5));
					if (Crown > High)
					{
						High = Crown;
						bTree = true;
					}
				}
				// The ridge sinks to nothing at the line's ends (a meeting of hedges, or where it reaches the tiles).
				High *= 0.25 + 0.75 * Smooth(0.0, 6.0, std::min(S, Total - S));
				const V2 Way = Unit2(L[std::min(K + 1, L.size() - 1)] - L[K > 0 ? K - 1 : 0]), N = Perp(Way);
				const double Half = 1.6 + 0.3 * High;
				Section Sc;
				for (int Q = -1; Q <= 1; ++Q)
				{
					const V2 P = L[K] + N * (Q * Half);
					const double Z = Land(P.X, P.Y) + (Q == 0 ? High : -0.4);
					Sc.P.push_back({ P.X, P.Y, Z });
					const double Tone = 0.9 + 0.2 * (0.5 + 0.5 * Noise(S / 6.0, static_cast<double>(Key % 991), Seed ^ 0xF4A4));
					Sc.C.push_back(Q == 0 ? Varied(bTree ? CrownCol : RidgeCol, Tone, 0.0) : Varied(FarHedge, Tone * 0.9, 0.0));
				}
				Secs.push_back(std::move(Sc));
			}
			Sweep(Secs, RingOf);
			HedgeLine H;
			H.Kind = HkStrip;
			for (const V2& P : L) H.Points.push_back({ P.X, P.Y });
			HedgeOut.push_back(std::move(H));
		}
		for (Tile& T : RingList)
		{
			Bounds(T);
			Count.RingTris += T.Near.Tris();
		}
	}
}

namespace LandField
{
	// ====================================================================== the base meshes

	namespace
	{
		V3 PointOf(const Mesh& M, uint32_t I) { return { M.Positions[I * 3], M.Positions[I * 3 + 1], M.Positions[I * 3 + 2] }; }

		/** A triangle wound to face Toward (bases built from shapes whose winding is easier to test than to work out). */
		void TriFacing(Mesh& M, uint32_t A, uint32_t B, uint32_t C, V3 Toward)
		{
			if (Dot3(Cross(PointOf(M, B) - PointOf(M, A), PointOf(M, C) - PointOf(M, A)), Toward) < 0.0) std::swap(B, C);
			M.Triangles.push_back(A);
			M.Triangles.push_back(B);
			M.Triangles.push_back(C);
		}

		/** An icosphere on the unit sphere: level 0 the icosahedron's 20 faces, each level four times as many. */
		void Icosphere(int Level, std::vector<V3>& P, std::vector<std::array<uint32_t, 3>>& F)
		{
			const double T = (1.0 + std::sqrt(5.0)) / 2.0;
			P = { { -1, T, 0 }, { 1, T, 0 }, { -1, -T, 0 }, { 1, -T, 0 }, { 0, -1, T }, { 0, 1, T }, { 0, -1, -T }, { 0, 1, -T }, { T, 0, -1 }, { T, 0, 1 }, { -T, 0, -1 }, { -T, 0, 1 } };
			for (V3& Q : P) Q = Unit3(Q);
			F = { { 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 }, { 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
				  { 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 }, { 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 } };
			for (int L = 0; L < Level; ++L)
			{
				std::unordered_map<uint64_t, uint32_t> Mid;
				auto Middle = [&P, &Mid](uint32_t A, uint32_t B) {
					const uint64_t K = (static_cast<uint64_t>(std::min(A, B)) << 32) | std::max(A, B);
					const auto Found = Mid.find(K);
					if (Found != Mid.end()) return Found->second;
					P.push_back(Unit3((P[A] + P[B]) * 0.5));
					const uint32_t I = static_cast<uint32_t>(P.size() - 1);
					Mid[K] = I;
					return I;
				};
				std::vector<std::array<uint32_t, 3>> Next;
				for (const auto& Fc : F)
				{
					const uint32_t A = Middle(Fc[0], Fc[1]), B = Middle(Fc[1], Fc[2]), C = Middle(Fc[2], Fc[0]);
					Next.push_back({ Fc[0], A, C });
					Next.push_back({ Fc[1], B, A });
					Next.push_back({ Fc[2], C, B });
					Next.push_back({ A, B, C });
				}
				F.swap(Next);
			}
		}

		/**
		 * A lump of leaves (or a stone): the sphere stretched to Radii round O, its surface pushed out and in by Noise3
		 * (Lump of its radius) so no two are alike, smooth-shaded; Paint colours each point by its place and its outward
		 * direction on the sphere. A lump of a crown bends its normals Bend of the way toward the crown's own outward
		 * direction (from Hub, through the crown's half-sizes Shape), so the crown lights as one soft mass with a leafy
		 * outline and not as a heap of separate balls.
		 */
		void Blob(Mesh& M, int Level, V3 O, V3 Radii, double Lump, double Freq, uint64_t Seed, const std::function<Col(V3, V3)>& Paint, V3 Hub = {}, V3 Shape = { 1, 1, 1 }, double Bend = 0.0)
		{
			std::vector<V3> S;
			std::vector<std::array<uint32_t, 3>> F;
			Icosphere(Level, S, F);
			std::vector<V3> P(S.size()), N(S.size());
			for (size_t K = 0; K < S.size(); ++K)
			{
				const double R = 1.0 + Lump * Noise3(S[K].X * Freq + 7.1, S[K].Y * Freq - 3.3, S[K].Z * Freq + 1.7, Seed);
				P[K] = O + V3{ S[K].X * Radii.X * R, S[K].Y * Radii.Y * R, S[K].Z * Radii.Z * R };
			}
			for (auto& Fc : F)
			{
				V3 Nf = Cross(P[Fc[1]] - P[Fc[0]], P[Fc[2]] - P[Fc[0]]);
				if (Dot3(Nf, (S[Fc[0]] + S[Fc[1]] + S[Fc[2]])) < 0.0)
				{
					std::swap(Fc[1], Fc[2]);
					Nf = Nf * -1.0;
				}
				for (const uint32_t I : Fc) N[I] = N[I] + Nf;
			}
			Builder B(M);
			const uint32_t Base0 = static_cast<uint32_t>(M.Points());
			for (size_t K = 0; K < S.size(); ++K)
			{
				const V3 Own = Unit3(N[K], S[K]);
				const V3 Out = Unit3(V3{ (P[K].X - Hub.X) / Shape.X, (P[K].Y - Hub.Y) / Shape.Y, (P[K].Z - Hub.Z) / Shape.Z }, Own);
				B.Vert(P[K], Unit3(Own * (1.0 - Bend) + Out * Bend, Own), Paint(P[K], S[K]));
			}
			for (const auto& Fc : F) B.Tri(Base0 + Fc[0], Base0 + Fc[1], Base0 + Fc[2]);
		}

		/** A trunk or a limb: a tapering tube from A to B, Sides round, open at its ends (they are in the crown or the ground). */
		void Limb(Mesh& M, V3 A, V3 B, double R0, double R1, int Sides, const Col& C0, const Col& C1)
		{
			const V3 Ax = Unit3(B - A);
			const V3 Side = Unit3(Cross(Ax, std::fabs(Ax.Z) < 0.9 ? V3{ 0, 0, 1 } : V3{ 1, 0, 0 }));
			const V3 Up = Cross(Side, Ax);
			const double Length = std::sqrt(Dot3(B - A, B - A));
			Builder Bd(M);
			const uint32_t Base0 = static_cast<uint32_t>(M.Points());
			for (int K = 0; K < Sides; ++K)
			{
				const double An = 2.0 * Pi * K / Sides;
				const V3 D = Side * std::cos(An) + Up * std::sin(An);
				const V3 Nrm = Unit3(D + Ax * ((R0 - R1) / std::max(Length, 1e-6)));
				Bd.Vert(A + D * R0, Nrm, C0);
				Bd.Vert(B + D * R1, Nrm, C1);
			}
			for (int K = 0; K < Sides; ++K)
			{
				const uint32_t A0 = Base0 + 2 * static_cast<uint32_t>(K), A1 = A0 + 1, B0 = Base0 + 2 * static_cast<uint32_t>((K + 1) % Sides), B1 = B0 + 1;
				const double An = 2.0 * Pi * (K + 0.5) / Sides;
				const V3 D = Side * std::cos(An) + Up * std::sin(An);
				TriFacing(M, A0, B0, B1, D);
				TriFacing(M, A0, B1, A1, D);
			}
		}

		/**
		 * A blade, a stalk or a petal: a thin strip through Spine (its points from foot to tip), Half wide either side at
		 * each, along Across; both faces, their normals tipped up so a meadow lights as a meadow and not as a thousand
		 * black blades edge-on to the sun.
		 */
		void Strip(Mesh& M, const std::vector<V3>& Spine, const std::vector<double>& Half, V3 Across, const Col& Foot, const Col& Tip)
		{
			if (Spine.size() < 2) return;
			const V3 Face = Unit3(Cross(Across, Spine.back() - Spine.front()), V3{ 1, 0, 0 });
			Builder B(M);
			for (int Side = 0; Side < 2; ++Side)
			{
				const V3 Out = Side == 0 ? Face : Face * -1.0;
				const V3 Nrm = Unit3(Out + V3{ 0, 0, 1.2 });
				const uint32_t Base0 = static_cast<uint32_t>(M.Points());
				for (size_t K = 0; K < Spine.size(); ++K)
				{
					const Col C = Mixed(Foot, Tip, static_cast<double>(K) / static_cast<double>(Spine.size() - 1));
					B.Vert(Spine[K] - Across * Half[K], Nrm, C);
					B.Vert(Spine[K] + Across * Half[K], Nrm, C);
				}
				for (size_t K = 0; K + 1 < Spine.size(); ++K)
				{
					const uint32_t A0 = Base0 + 2 * static_cast<uint32_t>(K), A1 = A0 + 1, B0 = A0 + 2, B1 = A0 + 3;
					TriFacing(M, A0, A1, B1, Out);
					if (Half[K + 1] > 1e-4) TriFacing(M, A0, B1, B0, Out);
				}
			}
		}

		void Measure(BaseMesh& B)
		{
			float R = 0.0f, H = 0.0f;
			for (size_t K = 0; K + 2 < B.Near.Positions.size(); K += 3)
			{
				R = std::max(R, std::hypot(B.Near.Positions[K], B.Near.Positions[K + 1]));
				H = std::max(H, B.Near.Positions[K + 2]);
			}
			B.Radius = R;
			B.Height = H;
		}

		/**
		 * A cluster of leaves (or a knot of bark): an icosahedron's twelve corners each pushed out or in by up to Jag of
		 * its radius, the whole turned at random, so no two are alike and its outline is ragged against the sky; Flat is
		 * its height over its width. Its normals are bent Bend of the way toward Soft (the spray's and the crown's own
		 * outward at a point), so a spray lights as one soft mass of leaves and not as a heap of gems. Paint colours each
		 * corner by its place and its own outward direction (turned with it). A sprig is the same made of an octahedron (eight
		 * faces, not twenty): smaller, many more of them for the triangles, its diamond outline a leafy edge.
		 */
		void Cluster(Mesh& M, V3 O, double Rad, double Flat, double Jag, Rng& R, const std::function<V3(V3)>& Soft, double Bend, const std::function<Col(V3, V3)>& Paint, bool bSprig = false)
		{
			std::vector<V3> S;
			std::vector<std::array<uint32_t, 3>> F;
			if (bSprig)
			{
				// A sprig: an octahedron, eight faces, a leafy diamond (the many small clusters of a crown's or a hedge's skin).
				S = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
				F = { { 0, 2, 4 }, { 2, 1, 4 }, { 1, 3, 4 }, { 3, 0, 4 }, { 2, 0, 5 }, { 1, 2, 5 }, { 3, 1, 5 }, { 0, 3, 5 } };
			}
			else Icosphere(0, S, F);
			const double A = R.Range(0.0, 2.0 * Pi), Tilt = R.Range(0.0, Pi);
			const double Ca = std::cos(A), Sa = std::sin(A), Ct = std::cos(Tilt), St = std::sin(Tilt);
			std::vector<V3> P(S.size()), N(S.size());
			for (size_t K = 0; K < S.size(); ++K)
			{
				const V3 Q{ S[K].X * Ca - S[K].Y * Sa, S[K].X * Sa + S[K].Y * Ca, S[K].Z };
				S[K] = { Q.X, Q.Y * Ct - Q.Z * St, Q.Y * St + Q.Z * Ct };
				const double Rr = Rad * (1.0 + Jag * R.Range(-1.0, 1.0));
				P[K] = O + V3{ S[K].X * Rr, S[K].Y * Rr, S[K].Z * Rr * Flat };
			}
			for (auto& Fc : F)
			{
				V3 Nf = Cross(P[Fc[1]] - P[Fc[0]], P[Fc[2]] - P[Fc[0]]);
				if (Dot3(Nf, S[Fc[0]] + S[Fc[1]] + S[Fc[2]]) < 0.0)
				{
					std::swap(Fc[1], Fc[2]);
					Nf = Nf * -1.0;
				}
				for (const uint32_t I : Fc) N[I] = N[I] + Nf;
			}
			Builder B(M);
			const uint32_t Base0 = static_cast<uint32_t>(M.Points());
			for (size_t K = 0; K < S.size(); ++K)
			{
				const V3 Own = Unit3(N[K], S[K]);
				B.Vert(P[K], Unit3(Own * (1.0 - Bend) + Soft(P[K]) * Bend, Own), Paint(P[K], S[K]));
			}
			for (const auto& Fc : F) B.Tri(Base0 + Fc[0], Base0 + Fc[1], Base0 + Fc[2]);
		}

		/**
		 * A hedge shrub's body: a squat upright cylinder with a rounded top and foot (|xy / A|^E + |z / C|^E = 1 round O),
		 * its skin pushed out and in by Noise3 (Lump of its size), so side by side shrubs join into a wall, not a row of
		 * balls. Normals bent Bend of the way toward its own outward.
		 */
		void Hull(Mesh& M, int Level, V3 O, double A, double C, double E, double Lump, uint64_t Seed, const std::function<Col(V3, V3)>& Paint, double Bend)
		{
			std::vector<V3> S;
			std::vector<std::array<uint32_t, 3>> F;
			Icosphere(Level, S, F);
			std::vector<V3> P(S.size()), N(S.size());
			for (size_t K = 0; K < S.size(); ++K)
			{
				const double Q = std::pow(std::hypot(S[K].X, S[K].Y) / A, E) + std::pow(std::fabs(S[K].Z) / C, E);
				const double D = std::pow(std::max(Q, 1e-9), -1.0 / E) * (1.0 + Lump * Noise3(S[K].X * 1.9 + 4.1, S[K].Y * 1.9 - 2.3, S[K].Z * 1.9 + 0.7, Seed));
				P[K] = O + S[K] * D;
			}
			for (auto& Fc : F)
			{
				V3 Nf = Cross(P[Fc[1]] - P[Fc[0]], P[Fc[2]] - P[Fc[0]]);
				if (Dot3(Nf, S[Fc[0]] + S[Fc[1]] + S[Fc[2]]) < 0.0)
				{
					std::swap(Fc[1], Fc[2]);
					Nf = Nf * -1.0;
				}
				for (const uint32_t I : Fc) N[I] = N[I] + Nf;
			}
			Builder B(M);
			const uint32_t Base0 = static_cast<uint32_t>(M.Points());
			for (size_t K = 0; K < S.size(); ++K)
			{
				const V3 Own = Unit3(N[K], S[K]);
				const V3 Out = Unit3(V3{ (P[K].X - O.X) / A, (P[K].Y - O.Y) / A, (P[K].Z - O.Z) / C }, Own);
				B.Vert(P[K], Unit3(Own * (1.0 - Bend) + Out * Bend, Own), Paint(P[K], S[K]));
			}
			for (const auto& Fc : F) B.Tri(Base0 + Fc[0], Base0 + Fc[1], Base0 + Fc[2]);
		}

		/** A random direction (the unit sphere, evenly). */
		V3 AnyWay(Rng& R)
		{
			const double Z = R.Range(-1.0, 1.0), A = R.Range(0.0, 2.0 * Pi), H = std::sqrt(std::max(0.0, 1.0 - Z * Z));
			return { H * std::cos(A), H * std::sin(A), Z };
		}

		/**
		 * A tree's shape. High: its height as built (TreeHigh); Bole: where the trunk forks; Trunk: its radius at the foot;
		 * Lean: how far the fork stands off the foot (an old apple's lean); Gnarl: how crooked trunk and limbs grow (0 to 1).
		 * The crown: its middle CZ, half-width CR and half-height CV, its sides pulled in by Waist at its middle (an elm's
		 * figure of eight). Limbs main limbs (none on a pollard: its sprouts rise straight from the knuckle); Sprays sprays
		 * of leaves (16 at most: the far tree is two big sprigs a spray), SprayR across, in each Heart clusters of LumpLo
		 * to LumpHi at its heart and Twigs sprigs about two thirds as big over its skin, Flat their height over width, Jag
		 * their raggedness; Deep how far in from the crown's skin the sprays may sit (more is denser); Light how far their
		 * sunlit tops pale toward yellow-green (most in the ash and the apple).
		 */
		struct TreeSpec
		{
			double High, Bole, Trunk, Lean, Gnarl;
			double CZ, CR, CV, Waist;
			int Limbs, Sprays, Heart, Twigs;
			double SprayR, LumpLo, LumpHi, Flat, Jag, Deep, Light;
			Col Leaf, Bark;
			bool bLeader, bPollard;
		};

		/**
		 * A broadleaf tree: a trunk (crooked as its kind grows), its main limbs arching out and a branch to each spray of
		 * leaves; each spray a dozen clusters of leaves heaped on its top and outer side, the sprays apart enough that the
		 * sky and the limbs show between them. Painted dark and cool beneath each spray and inside the crown, light and warm
		 * on its sunlit tops. The far tree: its trunk, three limbs, and two big sprigs for each spray, where the spray hangs.
		 */
		void BuildTree(BaseMesh& Bm, const TreeSpec& T, uint64_t Seed)
		{
			Rng R(Seed);
			auto Env = [&T](double V) { // the crown's half-width at V (-1 its bottom, 1 its top), over CR
				return std::sqrt(std::max(0.0, 1.0 - V * V)) * (1.0 - T.Waist * std::exp(-Sq((V - 0.1) / 0.3)));
			};
			// The trunk: four points from below the ground to the fork, kinked as the tree is gnarled.
			const double LeanA = R.Range(0.0, 2.0 * Pi);
			const V3 Fork{ T.Lean * std::cos(LeanA), T.Lean * std::sin(LeanA), T.Bole };
			std::vector<V3> Stem;
			std::vector<double> StemR;
			for (int K = 0; K <= 3; ++K)
			{
				const double F = K / 3.0;
				V3 P{ Fork.X * std::pow(F, 1.4), Fork.Y * std::pow(F, 1.4), -0.6 + (T.Bole + 0.6) * F };
				if (K == 1 || K == 2) P = P + V3{ R.Range(-1.0, 1.0), R.Range(-1.0, 1.0), 0.0 } * (T.Gnarl * (0.7 * T.Trunk + 0.08 * T.Bole));
				Stem.push_back(P);
				StemR.push_back(T.Trunk * (K == 0 ? 1.18 : 1.0 - 0.22 * F));
			}
			const Col Moss = Mixed(T.Bark, Rgb(70, 78, 50), 0.45), Dark = Scaled(T.Bark, 0.78);
			auto Segment = [](Mesh& M, V3 A, V3 B, double R0, double R1, int Sides, const Col& C0, const Col& C1) {
				// Run a little past its end, so a kinked limb shows no notch at the bend.
				const V3 Ax = Unit3(B - A);
				Limb(M, A, B + Ax * (0.6 * R1), R0, R1, Sides, C0, C1);
			};
			for (int K = 0; K < 3; ++K)
				Segment(Bm.Near, Stem[static_cast<size_t>(K)], Stem[static_cast<size_t>(K) + 1], StemR[static_cast<size_t>(K)], StemR[static_cast<size_t>(K) + 1], 8, K == 0 ? Moss : Mixed(Moss, T.Bark, K / 2.0), T.Bark);
			const V3 Hub{ Fork.X * 0.7, Fork.Y * 0.7, T.CZ }; // the crown's middle (over the fork, if the tree leans)
			auto Bark = [&T, Dark](V3 P, V3) { return Varied(Mixed(Dark, T.Bark, 0.5 + 0.5 * Noise3(P.X * 3.0, P.Y * 3.0, P.Z * 3.0, 0xBA4C)), 1.0, 0.02); };
			auto NoSoft = [](V3) { return V3{ 0, 0, 1 }; };
			if (T.bPollard)
			{
				// The knuckle the sprouts are cut back to, year on year: a swollen, knobbly head on the stub.
				Cluster(Bm.Near, Fork + V3{ 0, 0, 0.1 }, T.Trunk * 1.45, 0.75, 0.22, R, NoSoft, 0.0, Bark);
				Cluster(Bm.Near, Fork + V3{ R.Range(-0.2, 0.2), R.Range(-0.2, 0.2), -0.35 }, T.Trunk * 1.25, 0.8, 0.25, R, NoSoft, 0.0, Bark);
			}

			// The sprays: spread over the crown's skin top to well under its middle (a spiral), some set in, the crown's
			// underside left open.
			struct Spray
			{
				V3 C, Out;
				double Rad = 1.0, Tone = 1.0, Warm = 0.0;
			};
			std::vector<Spray> Sp;
			const int M = std::min(T.Sprays, 16);
			for (int K = 0; K < M; ++K)
			{
				Spray S;
				const double Zf = 0.92 - 1.75 * (K + 0.5) / M;
				const double An = K * 2.39996 + R.Range(-0.4, 0.4);
				S.Rad = T.SprayR * R.Range(0.75, 1.25);
				const double In = 1.0 - T.Deep * R.Next();
				const double H = std::max(0.0, T.CR * Env(Zf) * In - 0.5 * S.Rad);
				S.C = Hub + V3{ H * std::cos(An), H * std::sin(An), (T.CV - 0.45 * S.Rad) * Zf };
				S.C = S.C + V3{ R.Range(-0.1, 0.1) * T.CR, R.Range(-0.1, 0.1) * T.CR, R.Range(-0.08, 0.08) * T.CV };
				S.C.Z = std::min(S.C.Z, T.High - 0.45 * S.Rad - T.LumpHi * T.Flat);
				S.Out = Unit3(S.C - (Hub - V3{ 0, 0, 0.35 * T.CV }));
				S.Tone = R.Range(0.9, 1.1);
				S.Warm = R.Range(-0.04, 0.05);
				Sp.push_back(S);
			}

			// The limbs: each spray hangs from the limb nearest its bearing (the top's from the leader), and each limb runs out
			// three quarters of the way toward its sprays' middle, rising then levelling.
			const int L = T.bPollard ? 0 : std::max(1, T.Limbs);
			std::vector<double> LimbA;
			for (int K = 0; K < L; ++K) LimbA.push_back(LeanA + 2.0 * Pi * K / L + R.Range(-0.3, 0.3));
			std::vector<int> Owner(Sp.size(), -1);
			for (size_t K = 0; K < Sp.size(); ++K)
			{
				const double DX = Sp[K].C.X - Hub.X, DY = Sp[K].C.Y - Hub.Y;
				if (T.bPollard || (T.bLeader && std::hypot(DX, DY) < 0.3 * T.CR)) continue;
				const double A = std::atan2(DY, DX);
				double Best = 1e9;
				for (int Lm = 0; Lm < L; ++Lm)
				{
					const double D = std::fabs(std::remainder(A - LimbA[static_cast<size_t>(Lm)], 2.0 * Pi));
					if (D < Best)
					{
						Best = D;
						Owner[K] = Lm;
					}
				}
			}
			std::vector<V3> LimbFrom(static_cast<size_t>(L)), LimbMid(static_cast<size_t>(L)), LimbTo(static_cast<size_t>(L));
			for (int Lm = 0; Lm < L; ++Lm)
			{
				V3 Sum;
				int N = 0;
				for (size_t K = 0; K < Sp.size(); ++K)
					if (Owner[K] == Lm)
					{
						Sum = Sum + Sp[K].C;
						++N;
					}
				const double A = LimbA[static_cast<size_t>(Lm)];
				const V3 Aim = N > 0 ? Sum * (1.0 / N) : Hub + V3{ 0.6 * T.CR * std::cos(A), 0.6 * T.CR * std::sin(A), -0.2 * T.CV };
				const V3 From = Stem[2] + (Fork - Stem[2]) * R.Range(0.45, 1.0);
				const V3 To = From + (Aim - From) * 0.78;
				const double Len3 = std::sqrt(Dot3(To - From, To - From));
				const V3 Side = Unit3(Cross(To - From, V3{ 0, 0, 1 }), V3{ 1, 0, 0 });
				LimbFrom[static_cast<size_t>(Lm)] = From;
				LimbTo[static_cast<size_t>(Lm)] = To;
				LimbMid[static_cast<size_t>(Lm)] = From + (To - From) * 0.45 + V3{ 0, 0, 0.12 * Len3 } + Side * (R.Range(-1.0, 1.0) * T.Gnarl * 0.14 * Len3);
				Segment(Bm.Near, From, LimbMid[static_cast<size_t>(Lm)], T.Trunk * 0.52, T.Trunk * 0.34, 6, T.Bark, T.Bark);
				Segment(Bm.Near, LimbMid[static_cast<size_t>(Lm)], To, T.Trunk * 0.34, T.Trunk * 0.15, 6, T.Bark, Dark);
			}
			const V3 LeaderTop = Hub + V3{ 0, 0, 0.55 * T.CV };
			if (T.bLeader && !T.bPollard) Segment(Bm.Near, Fork - V3{ 0, 0, 0.2 }, LeaderTop, T.Trunk * 0.55, T.Trunk * 0.14, 6, T.Bark, Dark);
			// A branch out to each spray (a pollard's sprouts: straight poles from the knuckle).
			for (size_t K = 0; K < Sp.size(); ++K)
			{
				V3 From;
				double R0 = T.Trunk * 0.17;
				if (T.bPollard)
				{
					From = Fork + V3{ R.Range(-0.3, 0.3), R.Range(-0.3, 0.3), 0.25 };
					R0 = 0.09;
				}
				else if (Owner[K] < 0) From = Fork + (LeaderTop - Fork) * R.Range(0.4, 0.8);
				else
				{
					const size_t Lm = static_cast<size_t>(Owner[K]);
					const double DT = Dot3(Sp[K].C - LimbTo[Lm], Sp[K].C - LimbTo[Lm]), DM = Dot3(Sp[K].C - LimbMid[Lm], Sp[K].C - LimbMid[Lm]);
					From = DT < DM ? LimbTo[Lm] : LimbMid[Lm];
				}
				const V3 To = Sp[K].C - V3{ 0, 0, 0.2 * Sp[K].Rad };
				if (Dot3(To - From, To - From) > 0.04) Segment(Bm.Near, From, To, R0, R0 * 0.35, 4, T.Bark, Dark);
			}

			// The leaves.
			const double Zb = T.CZ - T.CV, Zt = T.CZ + T.CV;
			auto Paint = [&T, Hub, Zb, Zt](const Spray& S) {
				return [&T, Hub, Zb, Zt, S](V3 P, V3 Dir) {
					const double Height = Clamp01((P.Z - Zb) / std::max(1.0, Zt - Zb));
					const double Outward = Clamp01(std::hypot(P.X - Hub.X, P.Y - Hub.Y) / std::max(0.5, T.CR));
					const double Top = Dot3(Unit3(P - S.C), { 0, 0, 1 }); // the spray's top (1) to its underside (-1)
					const double Grain = Noise3(P.X * 1.7, P.Y * 1.7, P.Z * 1.7, 0x7EAF);
					const double Lit = 0.5 + 0.2 * Height + 0.12 * Outward + 0.13 * Top + 0.08 * Dir.Z;
					Col C = Varied(T.Leaf, S.Tone * Lit * (1.0 + 0.08 * Grain), S.Warm + 0.03 * Grain);
					C = Mixed(C, Rgb(30, 44, 34), 0.4 * Clamp01(-(0.6 * Top + 0.4 * Dir.Z)) + 0.15 * (1.0 - Height));
					return Mixed(C, Rgb(126, 142, 66), T.Light * Clamp01(Dir.Z) * Clamp01(Top + 0.3) * (0.4 + 0.6 * Height));
				};
			};
			auto Soft = [&T, Hub](const Spray& S) {
				return [&T, Hub, S](V3 P) {
					const V3 Crown = Unit3(V3{ (P.X - Hub.X) / T.CR, (P.Y - Hub.Y) / T.CR, (P.Z - Hub.Z) / T.CV });
					return Unit3(Unit3(P - S.C, S.Out) * 0.55 + Crown * 0.45, S.Out);
				};
			};
			for (const Spray& S : Sp)
			{
				const auto SoftS = Soft(S);
				const auto PaintS = Paint(S);
				const int N = T.Heart + T.Twigs + R.Below(5) - 2;
				for (int J = 0; J < N; ++J)
				{
					// Its heart a few big clusters; over its skin, heaped on its top and outer side and thin beneath, sprigs.
					const bool bSprig = J >= T.Heart;
					const V3 U = Unit3(AnyWay(R) + S.Out * 0.4 + V3{ 0, 0, 0.25 }, S.Out);
					const double D = J == 0 ? 0.0 : bSprig ? S.Rad * R.Range(0.6, 1.0) : S.Rad * R.Range(0.2, 0.6);
					const double Lr = R.Range(T.LumpLo, T.LumpHi) * (J == 0 ? 1.15 : bSprig ? 0.66 : 1.0);
					V3 O = S.C + V3{ U.X * D, U.Y * D, U.Z * D * 0.75 };
					O.Z = std::min(O.Z, T.High - Lr * T.Flat);
					Cluster(Bm.Near, O, Lr, T.Flat * R.Range(0.85, 1.1), T.Jag, R, SoftS, 0.75, PaintS, bSprig);
				}
			}

			// The far tree.
			Rng RF(Seed ^ 0xFA4);
			Limb(Bm.Far, Stem[0], Fork + V3{ 0, 0, 0.4 }, StemR[0], StemR[3], 5, Moss, T.Bark);
			for (int Lm = 0; Lm < std::min(L, 3); ++Lm) Limb(Bm.Far, LimbFrom[static_cast<size_t>(Lm)], LimbTo[static_cast<size_t>(Lm)], T.Trunk * 0.5, T.Trunk * 0.18, 4, T.Bark, Dark);
			if (T.bPollard) Cluster(Bm.Far, Fork + V3{ 0, 0, 0.1 }, T.Trunk * 1.45, 0.75, 0.2, RF, NoSoft, 0.0, Bark);
			auto CrownOut = [&T, Hub](V3 P) { return Unit3(V3{ (P.X - Hub.X) / T.CR, (P.Y - Hub.Y) / T.CR, (P.Z - Hub.Z) / T.CV }); };
			for (const Spray& S : Sp)
			{
				// Two big sprigs a spray, side by side across it, lit as the crown is, one soft mass, so that far off a
				// spray shows as a ragged clump and not as a ball.
				const double Rad = 0.78 * (S.Rad + 0.5 * T.LumpHi);
				const double A = RF.Range(0.0, Pi);
				for (int Side = -1; Side <= 1; Side += 2)
				{
					V3 O = S.C + V3{ Side * 0.42 * S.Rad * std::cos(A), Side * 0.42 * S.Rad * std::sin(A), (Side > 0 ? 0.3 : 0.05) * S.Rad };
					O.Z = std::min(O.Z, T.High - Rad * 0.85);
					Cluster(Bm.Far, O, Rad, 0.9, 0.3, RF, CrownOut, 0.7, Paint(S), true);
				}
			}
		}

		/**
		 * A hedge shrub. Top: its height as built (ShrubHigh; its foot sunk half a metre, as it stands on a bank's slope);
		 * CR its half-width; Square how upright its sides (the Hull's power: 2 a mound, 3 and more a wall); Lumps clusters of
		 * leaves over its body, LumpLo to LumpHi, and Leaves sprigs about half as big over its skin, Jag their
		 * raggedness; Sprigs how far loose shoots rise above its top;
		 * Leaf and, for Other of its clusters, Second (elder in the holly, field maple in the hazel).
		 */
		struct ShrubSpec
		{
			double Top, CR, Square;
			int Lumps, Leaves;
			double LumpLo, LumpHi, Jag, Sprigs;
			Col Leaf, Second;
			double Other;
		};

		/**
		 * A shrub of the hedge: a dark body thick to its foot (a laid hedge is), and over it a skin of small ragged leaf
		 * clusters, a few sprigs above, so that side by side, as the hedges plant them, they make a wall and not a row of
		 * balls. The far shrub: seven big ragged clusters.
		 */
		void BuildShrub(BaseMesh& Bm, const ShrubSpec& Sh, uint64_t Seed)
		{
			Rng R(Seed);
			constexpr double Foot = -0.5;
			const double Half = 0.5 * (Sh.Top - Foot), Mid = Foot + Half;
			const V3 O{ 0, 0, Mid };
			auto Body = [&Sh](V3 P, V3 Dir) {
				const double Height = Clamp01((P.Z - Foot) / (Sh.Top - Foot));
				const double Grain = Noise3(P.X * 2.3, P.Y * 2.3, P.Z * 2.3, 0x5A1B);
				return Varied(Sh.Leaf, (0.42 + 0.25 * Height + 0.06 * Dir.Z) * (1.0 + 0.1 * Grain), 0.03 * Grain);
			};
			Hull(Bm.Near, 1, O + V3{ 0, 0, -0.05 }, 0.72 * Sh.CR, 0.84 * Half, Sh.Square, 0.16, Seed + 1, Body, 0.5);
			auto Soft = [&Sh, Half, O](V3 P) { return Unit3(V3{ (P.X - O.X) / Sh.CR, (P.Y - O.Y) / Sh.CR, (P.Z - O.Z) / Half }); };
			auto Paint = [&Sh](const Col& Leaf, double Tone) {
				return [&Sh, Leaf, Tone](V3 P, V3 Dir) {
					const double Height = Clamp01((P.Z - Foot) / (Sh.Top - Foot));
					const double Grain = Noise3(P.X * 2.1, P.Y * 2.1, P.Z * 2.1, 0x5A1C);
					Col C = Varied(Leaf, Tone * (0.56 + 0.34 * Height + 0.1 * Dir.Z) * (1.0 + 0.1 * Grain), 0.04 * Grain);
					C = Mixed(C, Rgb(28, 40, 28), 0.25 * Clamp01(-Dir.Z));
					return Mixed(C, Rgb(122, 138, 64), 0.1 * Clamp01(Dir.Z) * Height);
				};
			};
			auto Surface = [&Sh, Half](V3 S) {
				const double Q = std::pow(std::hypot(S.X, S.Y) / Sh.CR, Sh.Square) + std::pow(std::fabs(S.Z) / Half, Sh.Square);
				return std::pow(std::max(Q, 1e-9), -1.0 / Sh.Square);
			};
			for (int Pass = 0; Pass < 2; ++Pass)
			{
				// The clusters a little in from the body's skin, then the sprigs on it and out of it: each spread over it (a
				// spiral) from its top to below the bank's face, nudged in or out.
				const bool bSprig = Pass == 1;
				const int N = bSprig ? Sh.Leaves : Sh.Lumps;
				for (int K = 0; K < N; ++K)
				{
					const double Zf = 1.0 - 1.8 * (K + 0.5) / N;
					const double An = K * 2.39996 + Pass * 1.3 + R.Range(-0.5, 0.5);
					const double Ring = std::sqrt(std::max(0.0, 1.0 - Zf * Zf));
					const V3 S{ Ring * std::cos(An), Ring * std::sin(An), Zf };
					const double Lr = R.Range(Sh.LumpLo, Sh.LumpHi) * (bSprig ? 0.55 : 1.0);
					V3 P = O + S * (Surface(S) * (bSprig ? R.Range(0.92, 1.12) : R.Range(0.8, 0.98))) - S * (0.5 * Lr);
					if (bSprig && Zf > 0.4 && R.Next() < 0.4) P.Z += R.Range(0.0, Sh.Sprigs);
					P.Z = std::min(P.Z, Sh.Top - 0.8 * Lr);
					const bool bOther = R.Next() < Sh.Other;
					Cluster(Bm.Near, P, Lr, bSprig ? R.Range(0.65, 0.9) : R.Range(0.8, 1.0), Sh.Jag, R, Soft, 0.45, Paint(bOther ? Sh.Second : Sh.Leaf, R.Range(0.86, 1.14)), bSprig);
				}
			}
			// The far shrub: seven big ragged clusters, three low, three above them between, one on top.
			Rng RF(Seed ^ 0xFA4);
			for (int K = 0; K < 7; ++K)
			{
				const double An = (K < 3 ? 2.0 * Pi * K / 3.0 : 2.0 * Pi * (K - 2.5) / 3.0) + RF.Range(-0.3, 0.3);
				const double Lr = (K == 6 ? 0.5 : 0.6) * Sh.CR;
				const double Out = K == 6 ? 0.1 * Sh.CR : 0.42 * Sh.CR;
				const double Z = K < 3 ? Foot + 0.38 * (Sh.Top - Foot) : K < 6 ? Foot + 0.66 * (Sh.Top - Foot) : Sh.Top - 0.8 * Lr;
				const V3 P{ Out * std::cos(An), Out * std::sin(An), std::min(Z, Sh.Top - 0.8 * Lr) };
				Cluster(Bm.Far, P, Lr, K < 3 ? 1.3 : 1.0, 0.25, RF, Soft, 0.45, Paint(Sh.Leaf, RF.Range(0.9, 1.05)));
			}
		}
	}

	void Ground::Impl::MakeBases()
	{
		Bases.assign(static_cast<size_t>(Base::Count), BaseMesh());
		static const char* const Names[] = { "ShrubA", "ShrubB", "ShrubC", "Bramble", "Oak", "Elm", "Ash", "Beech", "Pollard", "Apple", "Gate", "Well", "Cart", "Tuft", "TallGrass", "Wheat", "Poppies", "Flowers", "Stone", "Fern" };
		for (size_t K = 0; K < Bases.size(); ++K) Bases[K].Name = Names[K];
		auto At = [this](Base B) -> BaseMesh& { return Bases[static_cast<size_t>(B)]; };
		const uint64_t S = 0xBA5E;

		// The hedge shrubs, built ShrubHigh (2.8 m) tall: hawthorn and blackthorn dark, dense and upright-sided; hazel and
		// field maple lighter and rounder; holly and elder (dark and light clusters mixed) tall and loose, their sprigs above
		// the rest. Bramble low and sprawling, a metre high.
		BuildShrub(At(Base::ShrubA), { 2.8, 1.3, 2.5, 10, 200, 0.3, 0.46, 0.42, 0.35, Rgb(50, 68, 34), Rgb(58, 74, 36), 0.25 }, S + 1);
		BuildShrub(At(Base::ShrubB), { 2.8, 1.4, 2.2, 10, 180, 0.32, 0.5, 0.3, 0.25, Rgb(72, 94, 42), Rgb(84, 100, 46), 0.3 }, S + 2);
		BuildShrub(At(Base::ShrubC), { 2.8, 1.15, 2.4, 10, 200, 0.3, 0.46, 0.45, 0.6, Rgb(42, 60, 34), Rgb(70, 90, 44), 0.4 }, S + 3);
		BuildShrub(At(Base::Bramble), { 1.0, 0.95, 2.0, 6, 130, 0.2, 0.32, 0.45, 0.15, Rgb(58, 72, 38), Rgb(66, 76, 40), 0.2 }, S + 4);
		for (const Base B : { Base::ShrubA, Base::ShrubB, Base::ShrubC, Base::Bramble }) At(B).FarFrom = 60.0f;

		// The trees, each built at its TreeHigh: the hedgerow oak broad and ragged on spreading limbs; the elm tall, its
		// crown pulled in at the waist; the ash airy, small sprays far apart and pale; the beech dense and domed; the pollard
		// a thick stub, a knobbly knuckle and a bushy head of straight sprouts; the orchard apple low and wide on a short,
		// crooked, leaning trunk.
		BuildTree(At(Base::Oak), { 15.0, 3.4, 0.45, 0.15, 0.5, 9.0, 6.3, 4.6, 0.0, 6, 16, 4, 22, 2.2, 0.62, 1.12, 0.72, 0.38, 0.45, 0.2, Rgb(62, 84, 38), Rgb(88, 82, 70), true, false }, S + 10);
		BuildTree(At(Base::Elm), { 18.0, 4.5, 0.42, 0.1, 0.3, 11.8, 4.8, 5.8, 0.3, 4, 16, 3, 24, 1.9, 0.5, 0.95, 0.75, 0.35, 0.35, 0.16, Rgb(56, 78, 36), Rgb(78, 70, 60), true, false }, S + 11);
		BuildTree(At(Base::Ash), { 15.0, 4.0, 0.36, 0.1, 0.3, 10.0, 5.3, 4.4, 0.0, 5, 16, 2, 20, 1.7, 0.42, 0.75, 0.65, 0.4, 0.15, 0.26, Rgb(80, 100, 46), Rgb(118, 114, 102), true, false }, S + 12);
		BuildTree(At(Base::Beech), { 18.0, 3.6, 0.42, 0.05, 0.15, 11.2, 6.8, 5.8, 0.0, 5, 16, 4, 24, 2.2, 0.65, 1.1, 0.72, 0.3, 0.5, 0.18, Rgb(66, 92, 38), Rgb(128, 126, 116), true, false }, S + 13);
		BuildTree(At(Base::Pollard), { 8.0, 2.8, 0.5, 0.0, 0.6, 5.5, 3.0, 2.3, 0.0, 0, 14, 2, 16, 1.25, 0.4, 0.7, 0.8, 0.4, 0.35, 0.18, Rgb(68, 90, 40), Rgb(86, 80, 68), false, true }, S + 14);
		BuildTree(At(Base::Apple), { 5.5, 1.2, 0.17, 0.8, 1.0, 3.6, 3.2, 1.45, 0.0, 4, 12, 3, 18, 1.3, 0.4, 0.68, 0.72, 0.38, 0.3, 0.22, Rgb(72, 96, 44), Rgb(92, 82, 70), false, false }, S + 15);
		for (const Base B : { Base::Oak, Base::Elm, Base::Ash, Base::Beech, Base::Pollard, Base::Apple }) At(B).FarFrom = 140.0f;

		// A five-bar field gate on its hinge post, weathered silver-grey: the hinge at the origin, the gate along +x 3.6 m.
		{
			Builder B(At(Base::Gate).Near);
			const Col Wood = Rgb(138, 130, 112), Post = Rgb(104, 96, 82);
			B.Box({ 0, 0, 0.5 }, { 0.11, 0, 0 }, { 0, 0.11, 0 }, { 0, 0, 1.05 }, Post);
			B.Box({ 0.22, 0, 0.68 }, { 0.05, 0, 0 }, { 0, 0.045, 0 }, { 0, 0, 0.58 }, Wood, true);
			B.Box({ 3.52, 0, 0.68 }, { 0.045, 0, 0 }, { 0, 0.04, 0 }, { 0, 0, 0.56 }, Wood, true);
			const double Rails[5] = { 0.2, 0.43, 0.66, 0.89, 1.18 };
			for (int K = 0; K < 5; ++K) B.Box({ 1.87, 0, Rails[K] }, { 1.65, 0, 0 }, { 0, K == 4 ? 0.045 : 0.025, 0 }, { 0, 0, K == 4 ? 0.06 : 0.045 }, Wood, true);
			const V3 From{ 0.3, 0.0, 0.22 }, To{ 2.0, 0.0, 1.15 };
			B.Box((From + To) * 0.5, (To - From) * 0.5, { 0, 0.03, 0 }, Unit3(Cross(To - From, V3{ 0, 1, 0 })) * 0.04, Wood, true);
			At(Base::Gate).FarFrom = 150.0f;
		}
		// A farmyard well: a ring of limestone, a winch between two posts, a little slate roof.
		{
			Mesh& M = At(Base::Well).Near;
			Builder B(M);
			const Col Stone = Rgb(168, 160, 142), Dark = Rgb(40, 40, 38), Wood = Rgb(100, 88, 72), Slate = Rgb(84, 88, 96);
			constexpr int N = 14;
			const double RO = 0.8, RI = 0.58, Z0 = -0.3, Z1 = 0.85;
			for (int K = 0; K < N; ++K)
			{
				const double A0 = 2.0 * Pi * K / N, A1 = 2.0 * Pi * (K + 1) / N, AM = 0.5 * (A0 + A1);
				const V3 D0{ std::cos(A0), std::sin(A0), 0 }, D1{ std::cos(A1), std::sin(A1), 0 }, DM{ std::cos(AM), std::sin(AM), 0 };
				const Col C = Varied(Stone, 0.92 + 0.14 * Unit(Hash(S, static_cast<uint64_t>(K))), 0.0);
				B.Quad(D0 * RO + V3{ 0, 0, Z0 }, D1 * RO + V3{ 0, 0, Z0 }, D1 * RO + V3{ 0, 0, Z1 }, D0 * RO + V3{ 0, 0, Z1 }, Scaled(C, 0.8), Scaled(C, 0.8), C, C, DM);
				B.Quad1(D0 * RI + V3{ 0, 0, Z0 }, D1 * RI + V3{ 0, 0, Z0 }, D1 * RI + V3{ 0, 0, Z1 }, D0 * RI + V3{ 0, 0, Z1 }, Scaled(C, 0.55), DM * -1.0);
				B.Quad1(D0 * RI + V3{ 0, 0, Z1 }, D1 * RI + V3{ 0, 0, Z1 }, D1 * RO + V3{ 0, 0, Z1 }, D0 * RO + V3{ 0, 0, Z1 }, Varied(C, 1.06, 0.0), { 0, 0, 1 });
				B.Tri1({ 0, 0, 0.3 }, D0 * RI + V3{ 0, 0, 0.3 }, D1 * RI + V3{ 0, 0, 0.3 }, Dark, { 0, 0, 1 });
			}
			for (int Side = -1; Side <= 1; Side += 2) B.Box({ Side * 0.7, 0, 1.5 }, { 0.06, 0, 0 }, { 0, 0.06, 0 }, { 0, 0, 0.65 }, Wood);
			B.Box({ 0, 0, 1.72 }, { 0.68, 0, 0 }, { 0, 0.06, 0 }, { 0, 0, 0.06 }, Wood, true);
			for (int Side = -1; Side <= 1; Side += 2)
				B.Quad1({ -0.95, 0, 2.38 }, { 0.95, 0, 2.38 }, { 0.95, Side * 0.62, 2.02 }, { -0.95, Side * 0.62, 2.02 }, Slate, { 0, static_cast<double>(Side), 1.5 });
			At(Base::Well).FarFrom = 200.0f;
		}
		// A two-wheeled farm cart, its shafts down on the ground ahead (+x), boards weathered, iron tyres.
		{
			Mesh& M = At(Base::Cart).Near;
			Builder B(M);
			const Col Wood = Rgb(124, 108, 86), Board = Rgb(104, 116, 122), Iron = Rgb(52, 50, 48);
			B.Box({ 0, 0, 1.05 }, { 1.4, 0, 0 }, { 0, 0.7, 0 }, { 0, 0, 0.04 }, Wood, true);
			for (int Side = -1; Side <= 1; Side += 2)
			{
				B.Box({ 0, Side * 0.72, 1.28 }, { 1.4, 0, 0 }, { 0, 0.03, 0 }, { 0, 0, 0.2 }, Board, true);
				B.Box({ Side * 1.42, 0, 1.28 }, { 0.03, 0, 0 }, { 0, 0.72, 0 }, { 0, 0, 0.2 }, Board, true);
				const V3 From{ 1.3, Side * 0.45, 1.0 }, To{ 3.5, Side * 0.38, 0.12 };
				B.Box((From + To) * 0.5, (To - From) * 0.5, { 0, 0.045, 0 }, Unit3(Cross(To - From, V3{ 0, 1, 0 })) * -0.05, Wood, true);
				// A wheel: an iron-tyred rim of twelve felloes, eight spokes, a hub.
				const double WY = Side * 0.86, WR = 0.74;
				const V3 Hub{ 0, WY, WR };
				for (int K = 0; K < 12; ++K)
				{
					const double A0 = 2.0 * Pi * K / 12.0, A1 = 2.0 * Pi * (K + 1) / 12.0;
					const V3 P0 = Hub + V3{ WR * std::cos(A0), 0, WR * std::sin(A0) }, P1 = Hub + V3{ WR * std::cos(A1), 0, WR * std::sin(A1) };
					const V3 Out = Unit3(((P0 + P1) * 0.5) - Hub);
					B.Box((P0 + P1) * 0.5 - Out * 0.04, (P1 - P0) * 0.5, { 0, 0.045, 0 }, Out * 0.045, K % 2 ? Wood : Scaled(Wood, 0.92), true, &Iron);
				}
				for (int K = 0; K < 8; ++K)
				{
					const double A = 2.0 * Pi * K / 8.0;
					const V3 D{ std::cos(A), 0, std::sin(A) };
					B.Box(Hub + D * (0.5 * WR), D * (0.46 * WR), { 0, 0.025, 0 }, Unit3(Cross(D, V3{ 0, 1, 0 })) * 0.025, Wood, true);
				}
				B.Box(Hub, { 0.11, 0, 0 }, { 0, 0.1, 0 }, { 0, 0, 0.11 }, Scaled(Wood, 0.8), true);
			}
			B.Box({ 0, 0, 0.74 }, { 0.05, 0, 0 }, { 0, 0.9, 0 }, { 0, 0, 0.05 }, Scaled(Wood, 0.7), true);
			At(Base::Cart).FarFrom = 200.0f;
		}

		// The clutter, near only: what the eye sees round a soldier's boots.
		Rng R(S + 40);
		auto Around = [&R](double Spread) {
			const double A = R.Range(0.0, 2.0 * Pi), D = Spread * std::sqrt(R.Next());
			return V3{ D * std::cos(A), D * std::sin(A), 0.0 };
		};
		auto Bent = [](V3 Foot, V3 Lean, double High, int N) {
			std::vector<V3> P;
			for (int K = 0; K <= N; ++K)
			{
				const double T = static_cast<double>(K) / N;
				P.push_back(Foot + Lean * (T * T) + V3{ 0, 0, High * T * (1.0 - 0.12 * T) });
			}
			return P;
		};
		auto AcrossOf = [](V3 Lean, double A) { return Unit3(Cross({ 0, 0, 1 }, Lean), V3{ std::cos(A), std::sin(A), 0 }); };
		// A grass tuft: fourteen blades a hand's breadth round, 20 to 42 cm, leaning out, their tips paler.
		{
			Mesh& M = At(Base::Tuft).Near;
			for (int K = 0; K < 14; ++K)
			{
				const V3 Foot = Around(0.07);
				const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.2, 0.42);
				const V3 Lean{ std::cos(A) * High * R.Range(0.2, 0.6), std::sin(A) * High * R.Range(0.2, 0.6), 0 };
				Strip(M, { Foot, Foot + Lean * 0.4 + V3{ 0, 0, High * 0.55 }, Foot + Lean + V3{ 0, 0, High } }, { 0.014, 0.01, 0.0 }, AcrossOf(Lean, A), Rgb(62, 80, 42), Varied(Rgb(112, 122, 70), R.Range(0.9, 1.1), 0.0));
			}
		}
		// Tall grass and docks: longer blades bending over, and seed heads.
		{
			Mesh& M = At(Base::TallGrass).Near;
			for (int K = 0; K < 16; ++K)
			{
				const V3 Foot = Around(0.12);
				const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.45, 0.9);
				const V3 Lean{ std::cos(A) * High * R.Range(0.25, 0.6), std::sin(A) * High * R.Range(0.25, 0.6), 0 };
				const std::vector<V3> P = Bent(Foot, Lean, High, 3);
				Strip(M, P, { 0.016, 0.013, 0.008, 0.0 }, AcrossOf(Lean, A), Rgb(68, 86, 46), Rgb(124, 128, 78));
			}
			for (int K = 0; K < 5; ++K)
			{
				const V3 Foot = Around(0.08);
				const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.7, 0.95);
				const V3 Lean{ std::cos(A) * 0.1, std::sin(A) * 0.1, 0 };
				const V3 Head = Foot + Lean + V3{ 0, 0, High };
				Strip(M, { Foot, Head }, { 0.005, 0.004 }, AcrossOf(Lean, A), Rgb(96, 110, 60), Rgb(132, 130, 84));
				Strip(M, { Head, Head + Lean * 0.4 + V3{ 0, 0, 0.12 } }, { 0.018, 0.0 }, AcrossOf(Lean, A + 1.0), Rgb(150, 140, 96), Rgb(168, 156, 112));
			}
		}
		// Ripe corn: two dozen stalks, a metre high, each with its ear, leaning a little with the wind.
		{
			Mesh& M = At(Base::Wheat).Near;
			const V3 Wind{ 0.06, 0.02, 0 };
			for (int K = 0; K < 24; ++K)
			{
				const V3 Foot = Around(0.3);
				const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.82, 1.0);
				const V3 Lean = Wind + V3{ R.Range(-0.04, 0.04), R.Range(-0.04, 0.04), 0 };
				const V3 Crest = Foot + Lean + V3{ 0, 0, High };
				const V3 Across{ std::cos(A), std::sin(A), 0 };
				Strip(M, { Foot, Foot + Lean * 0.3 + V3{ 0, 0, High * 0.5 }, Crest }, { 0.007, 0.006, 0.005 }, Across, Rgb(124, 118, 64), Rgb(180, 152, 86));
				const V3 EarDir = Unit3(Lean + V3{ 0, 0, 0.6 });
				Strip(M, { Crest, Crest + EarDir * 0.05, Crest + EarDir * 0.1 }, { 0.012, 0.016, 0.004 }, Across, Rgb(186, 158, 92), Rgb(198, 170, 104));
			}
		}
		// Poppies: four stems with their red heads (two crossed cups), and a few leaves.
		{
			Mesh& M = At(Base::Poppies).Near;
			for (int K = 0; K < 4; ++K)
			{
				const V3 Foot = Around(0.12);
				const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.42, 0.68);
				const V3 Lean{ R.Range(-0.06, 0.06), R.Range(-0.06, 0.06), 0 };
				const V3 Head = Foot + Lean + V3{ 0, 0, High };
				Strip(M, { Foot, Head }, { 0.004, 0.003 }, { std::cos(A), std::sin(A), 0 }, Rgb(78, 100, 48), Rgb(92, 112, 56));
				for (int P = 0; P < 2; ++P)
				{
					const V3 Across{ std::cos(A + P * Pi / 2), std::sin(A + P * Pi / 2), 0 };
					Strip(M, { Head - V3{ 0, 0, 0.01 }, Head + V3{ 0, 0, 0.035 }, Head + V3{ 0, 0, 0.06 } }, { 0.012, 0.04, 0.035 }, Across, Rgb(150, 30, 26), Rgb(204, 44, 32));
				}
			}
			for (int K = 0; K < 3; ++K)
			{
				const V3 Foot = Around(0.1);
				const double A = R.Range(0.0, 2.0 * Pi);
				const V3 Lean{ std::cos(A) * 0.12, std::sin(A) * 0.12, 0 };
				Strip(M, Bent(Foot, Lean, 0.16, 2), { 0.02, 0.025, 0.0 }, AcrossOf(Lean, A), Rgb(70, 94, 44), Rgb(92, 112, 54));
			}
		}
		// White ox-eye daisies and yellow buttercups over their leaves.
		{
			Mesh& M = At(Base::Flowers).Near;
			for (int K = 0; K < 6; ++K)
			{
				const bool bDaisy = K < 3;
				const V3 Foot = Around(0.14);
				const double A = R.Range(0.0, 2.0 * Pi), High = bDaisy ? R.Range(0.38, 0.6) : R.Range(0.25, 0.45);
				const V3 Head = Foot + V3{ R.Range(-0.05, 0.05), R.Range(-0.05, 0.05), High };
				Strip(M, { Foot, Head }, { 0.003, 0.002 }, { std::cos(A), std::sin(A), 0 }, Rgb(76, 98, 46), Rgb(90, 110, 54));
				const double Size = bDaisy ? 0.032 : 0.014;
				const Col Petal = bDaisy ? Rgb(230, 228, 214) : Rgb(222, 192, 40);
				for (int P = 0; P < 2; ++P)
				{
					const V3 Across{ std::cos(A + P * Pi / 4), std::sin(A + P * Pi / 4), 0 };
					const V3 Flat = Unit3(Cross(Across, V3{ 0, 0, 1 })) * Size + V3{ 0, 0, 0.006 };
					Strip(M, { Head - Flat, Head + Flat }, { Size, Size }, Across, Petal, Petal);
				}
				if (bDaisy) Strip(M, { Head + V3{ -0.008, 0, 0.008 }, Head + V3{ 0.008, 0, 0.008 } }, { 0.009, 0.009 }, { 0, 1, 0 }, Rgb(214, 170, 36), Rgb(214, 170, 36));
			}
			for (int K = 0; K < 4; ++K)
			{
				const V3 Foot = Around(0.12);
				const double A = R.Range(0.0, 2.0 * Pi);
				const V3 Lean{ std::cos(A) * 0.1, std::sin(A) * 0.1, 0 };
				Strip(M, Bent(Foot, Lean, 0.12, 2), { 0.015, 0.02, 0.0 }, AcrossOf(Lean, A), Rgb(68, 92, 42), Rgb(88, 110, 52));
			}
		}
		// A stone of the field: a weathered lump of grey limestone or flint, more than half sunk, earth-stained and greened
		// at its foot, never pale (sRGB some 105 to 135, a little warm).
		Blob(At(Base::Stone).Near, 1, { 0, 0, -0.045 }, { 0.17, 0.13, 0.09 }, 0.32, 1.8, S + 50, [](V3 P, V3 Dir) {
			const double Grain = Noise3(P.X * 30.0, P.Y * 30.0, P.Z * 30.0, 0x5703);
			const Col Grey = Varied(Rgb(124, 120, 112), (0.92 + 0.1 * Dir.Z) * (1.0 + 0.08 * Grain), 0.02 + 0.02 * Grain);
			return Mixed(Grey, Rgb(84, 86, 64), 0.4 * Clamp01((0.8 - Dir.Z) / 0.35)); // the ground meets it at about half its height
		});
		// Bracken: six fronds arching out from the middle, their pinnae as a strip tapering to the tip.
		{
			Mesh& M = At(Base::Fern).Near;
			for (int K = 0; K < 6; ++K)
			{
				const double A = K * 2.0 * Pi / 6.0 + R.Range(-0.3, 0.3);
				const V3 Out{ std::cos(A), std::sin(A), 0 };
				const double Reach = R.Range(0.45, 0.7), High = R.Range(0.45, 0.7);
				std::vector<V3> P;
				for (int Q = 0; Q <= 5; ++Q)
				{
					const double T = Q / 5.0;
					P.push_back(Out * (Reach * T) + V3{ 0, 0, High * std::sin(T * Pi * 0.62) * (1.0 - 0.35 * T) });
				}
				Strip(M, P, { 0.01, 0.1, 0.13, 0.11, 0.07, 0.0 }, { -Out.Y, Out.X, 0 }, Rgb(82, 110, 44), Rgb(112, 138, 58));
			}
		}
		for (const Base B : { Base::Tuft, Base::TallGrass, Base::Wheat, Base::Poppies, Base::Flowers, Base::Stone, Base::Fern }) At(B).FarFrom = 70.0f;
		for (BaseMesh& B : Bases) Measure(B);
	}
}

namespace LandField
{
	// ====================================================================== building it, and what the game asks

	Ground::Impl::Impl(const Field& F) : Fd(F)
	{
		using Clock = std::chrono::steady_clock;
		const auto T0 = Clock::now();
		Fd.Cols = std::max(1, Fd.Cols);
		Fd.Rows = std::max(1, Fd.Rows);
		if (!(Fd.Cell > 0.0)) Fd.Cell = 25.0;
		if (Fd.Grounds.empty()) Fd.Grounds.push_back("open");
		FW = Fd.Width();
		FH = Fd.Depth();
		Seed = Mix(Fd.Seed ^ 0x1A2DF1E1Dull);
		ReadCells();
		MakeLanes();
		MakeRaster();
		MakeWays();
		MakePlots();
		MakeSites();
		MakeParcels();
		MakeDistances();
		MakeHedges();
		MakePlants();
		const auto T1 = Clock::now();
		MakeBases();
		MakeTiles();
		MakeRing();
		const auto T2 = Clock::now();
		Count.CoreSeconds = std::chrono::duration<double>(T1 - T0).count();
		Count.MeshSeconds = std::chrono::duration<double>(T2 - T1).count();
	}

	Ground::Ground(const Field& F) : In(std::make_unique<Impl>(F)) {}
	Ground::~Ground() = default;

	/**
	 * The surface drawn, its normal from the surface a hand's breadth either way, and the effects kit's word for it:
	 * the rules' cell gives it (hand-off section 7), except where what is drawn says otherwise, so a shell bursting on
	 * a bank in a field cell throws earth, in a wood's trees woods, on a cobbled village lane or in a ruined house town.
	 */
	Surface Ground::At(double X, double Y) const
	{
		const Impl& G = *In;
		Surface S;
		Drawn What = Drawn::Pasture;
		S.Z = G.Top(X, Y, &What);
		S.What = What;
		constexpr double E = 0.2;
		const double DX = G.Top(X + E, Y, nullptr) - G.Top(X - E, Y, nullptr), DY = G.Top(X, Y + E, nullptr) - G.Top(X, Y - E, nullptr);
		const V3 N = Unit3({ -DX / (2.0 * E), -DY / (2.0 * E), 1.0 });
		S.NX = N.X;
		S.NY = N.Y;
		S.NZ = N.Z;
		S.Ground = G.Fd.Ground[static_cast<size_t>(G.CellAt(X, Y))];
		const int CellFx = S.Ground >= 0 && static_cast<size_t>(S.Ground) < G.FxOfCell.size() ? G.FxOfCell[static_cast<size_t>(S.Ground)] : 0;
		const int Plain = G.bDesert ? 5 : 0;
		const bool bCobbled = G.InRaster(X, Y) && (G.OccR[G.Pix(X, Y)] & OcNear);
		int Fx = Plain;
		switch (What)
		{
		case Drawn::Floor:
		case Drawn::Wall: Fx = 1; break;
		case Drawn::Woods: Fx = 2; break;
		case Drawn::Water: Fx = 3; break;
		case Drawn::Marsh: Fx = 4; break;
		case Drawn::Village:
		case Drawn::Yard: Fx = bCobbled ? 1 : Plain; break;
		case Drawn::Lane:
			Fx = CellFx == 1 || (G.Fd.bTown && std::hypot(X - G.Fd.TownX, Y - G.Fd.TownY) < G.Fd.TownR * 0.85) ? 1 : Plain;
			break;
		default: Fx = CellFx == 1 ? 1 : Plain; break;
		}
		S.Fx = FxWords[Fx];
		return S;
	}

	const std::vector<Tile>& Ground::Tiles() const { return In->TileList; }
	const std::vector<Tile>& Ground::Ring() const { return In->RingList; }
	const BaseMesh& Ground::BaseOf(Base B) const { return In->Bases[static_cast<size_t>(B)]; }
	const std::vector<Placed>& Ground::Instances(Base B) const { return In->Inst[static_cast<size_t>(B)]; }
	const std::vector<HedgeLine>& Ground::Hedges() const { return In->HedgeOut; }
	const std::vector<std::vector<std::pair<double, double>>>& Ground::Lanes() const { return In->LaneOut; }
	const Tally& Ground::Laid() const { return In->Count; }

	int Ground::ParcelAt(double X, double Y) const
	{
		const Impl& G = *In;
		if (!G.InRaster(X, Y)) return -1;
		const int32_t P = G.Pid[G.Pix(X, Y)];
		if (P < 0) return -1;
		const int K = G.Parcels[static_cast<size_t>(P)].Kind;
		return IsFarmland(K) || K == RgVillage || K == RgYard ? P : -1;
	}

	namespace
	{
		/**
		 * What grows round a soldier's boots here, by what is drawn and what the parcel grows: a base and how many a square
		 * metre. Edge: metres in from the parcel's edge; LaneEdge: metres outside the nearest lane's floor (negative on it).
		 */
		struct Growth
		{
			double Density = 0.0;
			Base Kind = Base::Count;
		};
		Growth Grow(Drawn What, int Crop, double Edge, double LaneEdge, uint8_t Occ, double Pick)
		{
			auto Of = [Pick](double Density, std::initializer_list<std::pair<double, Base>> Odds) {
				Growth G;
				G.Density = Density;
				double Sum = 0.0;
				for (const auto& [P, B] : Odds)
				{
					Sum += P;
					G.Kind = B;
					if (Pick < Sum) break;
				}
				return G;
			};
			Growth G;
			switch (What)
			{
			case Drawn::Crop:
				if (IsCorn(Crop))
					G = Edge > 4.0 ? Of(2.0, { { 0.93, Base::Wheat }, { 0.04, Base::Poppies }, { 0.02, Base::Tuft }, { 0.01, Base::Flowers } })
								   : Of(1.4, { { 0.4, Base::Wheat }, { 0.22, Base::Poppies }, { 0.18, Base::TallGrass }, { 0.1, Base::Flowers }, { 0.1, Base::Tuft } });
				else if (Crop == CpStubble) G = Of(0.5, { { 0.7, Base::Tuft }, { 0.15, Base::Poppies }, { 0.15, Base::Flowers } });
				else G = Of(1.6, { { 1.0, Base::Tuft } });
				break;
			case Drawn::Pasture:
				if (Edge < 3.0) G = Of(1.2, { { 0.35, Base::TallGrass }, { 0.15, Base::Fern }, { 0.2, Base::Flowers }, { 0.3, Base::Tuft } });
				else if (Crop == CpLush) G = Of(1.2, { { 0.6, Base::Tuft }, { 0.3, Base::TallGrass }, { 0.1, Base::Flowers } });
				else if (Crop == CpHayCut) G = Of(0.6, { { 0.9, Base::Tuft }, { 0.1, Base::Flowers } });
				else if (Crop == CpMeadow) G = Of(1.4, { { 0.2, Base::Tuft }, { 0.55, Base::TallGrass }, { 0.15, Base::Flowers }, { 0.1, Base::Poppies } });
				else G = Of(1.0, { { 0.86, Base::Tuft }, { 0.08, Base::Flowers }, { 0.06, Base::TallGrass } });
				break;
			case Drawn::Rough: G = Of(1.2, { { 0.5, Base::TallGrass }, { 0.35, Base::Tuft }, { 0.15, Base::Fern } }); break;
			case Drawn::Works: G = Of(0.4, { { 0.25, Base::Stone }, { 0.75, Base::Tuft } }); break;
			case Drawn::Village:
			case Drawn::Yard:
				G = (Occ & OcNear) || What == Drawn::Yard ? Of(0.25, { { 0.8, Base::Tuft }, { 0.2, Base::Stone } })
														   : Of(0.8, { { 0.6, Base::Tuft }, { 0.3, Base::Flowers }, { 0.1, Base::TallGrass } });
				break;
			case Drawn::Lane:
				// The floor's edges and its middle strip grass over; between, the ruts' stones. The walls: ferns and grass.
				if (LaneEdge > 0.0) G = Of(0.8, { { 0.5, Base::Fern }, { 0.3, Base::TallGrass }, { 0.2, Base::Tuft } });
				else if (LaneEdge > -0.5) G = Of(0.3, { { 0.7, Base::Tuft }, { 0.3, Base::Fern } });
				else G = Of(0.08, { { 0.6, Base::Stone }, { 0.4, Base::Tuft } });
				break;
			case Drawn::Bank: G = Of(1.0, { { 0.45, Base::Fern }, { 0.35, Base::TallGrass }, { 0.2, Base::Tuft } }); break;
			case Drawn::Woods: G = Of(0.5, { { 0.65, Base::Fern }, { 0.35, Base::Tuft } }); break;
			case Drawn::Wall: G = Of(0.2, { { 0.5, Base::TallGrass }, { 0.5, Base::Fern } }); break;
			case Drawn::Marsh: G = Of(1.5, { { 0.8, Base::TallGrass }, { 0.2, Base::Tuft } }); break;
			case Drawn::Beyond: G = Of(0.8, { { 0.85, Base::Tuft }, { 0.1, Base::TallGrass }, { 0.05, Base::Flowers } }); break;
			default: break; // a floor or water: nothing
			}
			// Trodden: gateways, and where the recording's men and vehicles went.
			if (Occ & OcVehicle) G.Density *= 0.35;
			else if (Occ & OcGate) G.Density *= 0.3;
			else if (Occ & OcMan) G.Density *= 0.75;
			return G;
		}
	}

	void Ground::Clutter(double X0, double Y0, double X1, double Y1, std::vector<Placed>& Out) const
	{
		const Impl& G = *In;
		constexpr double Square = 8.0;
		constexpr int Tries = 128; // two things a square metre at most
		const int I0 = static_cast<int>(std::floor(X0 / Square)), I1 = static_cast<int>(std::floor(X1 / Square));
		const int J0 = static_cast<int>(std::floor(Y0 / Square)), J1 = static_cast<int>(std::floor(Y1 / Square));
		for (int J = J0; J <= J1; ++J)
			for (int I = I0; I <= I1; ++I)
			{
				const uint64_t Key = Hash(G.Seed ^ 0xC107E5ull, static_cast<uint64_t>(static_cast<int64_t>(I)), static_cast<uint64_t>(static_cast<int64_t>(J)));
				for (int K = 0; K < Tries; ++K)
				{
					// Everything about a thing comes from its square and its number, so any box that holds it lays it the same.
					const uint64_t H = Hash(Key, static_cast<uint64_t>(K));
					const double X = (I + Unit(H)) * Square, Y = (J + Unit(Mix(H))) * Square;
					if (X < X0 || X >= X1 || Y < Y0 || Y >= Y1) continue;
					const double Keep = Unit(Hash(H, 1)), Pick = Unit(Hash(H, 2));
					int Crop = CpOther;
					double Edge = 99.0, LaneEdge = 99.0;
					uint8_t Occ = 0;
					if (G.InRaster(X, Y))
					{
						const size_t P = G.Pix(X, Y);
						const ParcelInfo& Q = G.Parcels[static_cast<size_t>(G.Pid[P])];
						Crop = Q.Crop;
						Edge = G.DistE[P];
						LaneEdge = G.LaneD[P];
						Occ = G.OccR[P];
						if (Occ & OcHouse) continue;
						if (Keep >= (IsCorn(Crop) ? 1.0 : 0.75)) continue; // more than any growth here could want
					}
					Drawn What = Drawn::Beyond;
					const double Z = G.Top(X, Y, &What);
					const Growth Gr = Grow(What, Crop, Edge, LaneEdge, Occ, Pick);
					if (Gr.Kind == Base::Count || Keep >= Gr.Density / 2.0) continue;
					Placed P;
					P.Kind = Gr.Kind;
					P.X = static_cast<float>(X);
					P.Y = static_cast<float>(Y);
					P.Z = static_cast<float>(Z);
					P.Yaw = static_cast<float>(2.0 * Pi * Unit(Hash(H, 3)));
					const double U4 = Unit(Hash(H, 4)), U5 = Unit(Hash(H, 5)), U6 = Unit(Hash(H, 6));
					double Scale = 0.8 + 0.4 * U4, ScaleZ = 0.85 + 0.3 * U5;
					double TR = 1.0, TG = 1.0, TB = 1.0;
					switch (Gr.Kind)
					{
					case Base::Wheat:
						Scale = 0.9 + 0.3 * U4;
						ScaleZ = 0.92 + 0.16 * U5;
						if (Crop == CpBarley)
						{
							ScaleZ *= 0.82;
							TR = 1.06;
							TG = 1.06;
							TB = 1.1;
						}
						else if (Crop == CpOats)
						{
							ScaleZ *= 1.05;
							TR = 0.9;
							TG = 0.98;
							TB = 0.88;
						}
						else if (Crop == CpGreenWheat)
						{
							ScaleZ *= 0.95;
							TR = 0.62;
							TG = 0.84;
							TB = 0.7;
						}
						break;
					case Base::Tuft:
						Scale = 0.7 + 0.7 * U4;
						ScaleZ = 0.6 + 0.7 * U5;
						if (Crop == CpHayCut) ScaleZ *= 0.6;
						if (Crop == CpRoots && What == Drawn::Crop)
						{
							Scale *= 1.6;
							ScaleZ *= 0.8;
							TR = 0.72;
							TG = 0.86;
							TB = 0.74;
						}
						if (Crop == CpStubble && What == Drawn::Crop)
						{
							TR = 1.12;
							TG = 1.04;
							TB = 0.92;
						}
						break;
					case Base::TallGrass:
						Scale = 0.8 + 0.5 * U4;
						ScaleZ = 0.8 + 0.45 * U5;
						break;
					case Base::Stone:
						Scale = 0.6 + 1.2 * U4;
						ScaleZ = 0.7 + 0.6 * U5;
						break;
					default: break;
					}
					const double Shade = 0.95 + 0.1 * U6;
					P.Scale = static_cast<float>(Scale);
					P.ScaleZ = static_cast<float>(ScaleZ);
					P.Tint = Tint8(TR * Shade, TG * Shade, TB * Shade);
					Out.push_back(P);
				}
				// The field's stones: none strewn over the grass, only a few grey ones half sunk, in clumps where the plough
				// and the beasts left them: along the banks' feet, in the gateways and at the lanes' sides.
				constexpr int StoneTries = 6;
				for (int K = 0; K < StoneTries; ++K)
				{
					const uint64_t H = Hash(Key, 0x5703ull + static_cast<uint64_t>(K));
					const double X = (I + Unit(H)) * Square, Y = (J + Unit(Mix(H))) * Square;
					if (X < X0 || X >= X1 || Y < Y0 || Y >= Y1 || !G.InRaster(X, Y)) continue;
					const size_t Px = G.Pix(X, Y);
					const uint8_t Occ = G.OccR[Px];
					if (Occ & (OcHouse | OcVehicle | OcNear)) continue;
					Drawn What = Drawn::Beyond;
					const double Z = G.Top(X, Y, &What);
					const bool bGate = (Occ & OcGate) != 0;
					const bool bFieldFoot = (What == Drawn::Pasture || What == Drawn::Crop || What == Drawn::Rough) && G.DistE[Px] > 2.5f && G.DistE[Px] < 5.5f;
					const bool bLaneSide = What == Drawn::Lane && G.LaneD[Px] > -1.0f && G.LaneD[Px] < 0.3f;
					if (!bGate && !bFieldFoot && !bLaneSide) continue;
					// Clumps: a stretch of bank foot here and there, the rest bare.
					const double Clump = Noise(X / 6.0, Y / 6.0, G.Seed ^ 0x5703ull);
					if (Clump < (bGate ? 0.0 : 0.32) || Unit(Hash(H, 1)) > (bGate ? 0.5 : 0.45)) continue;
					Placed P;
					P.Kind = Base::Stone;
					P.X = static_cast<float>(X);
					P.Y = static_cast<float>(Y);
					P.Z = static_cast<float>(Z);
					P.Yaw = static_cast<float>(2.0 * Pi * Unit(Hash(H, 3)));
					P.Scale = static_cast<float>(0.5 + 1.2 * Unit(Hash(H, 4)) * Unit(Hash(H, 7)) + 0.3 * Clump);
					P.ScaleZ = static_cast<float>(0.65 + 0.4 * Unit(Hash(H, 5)));
					const double Shade = 0.9 + 0.16 * Unit(Hash(H, 6));
					P.Tint = Tint8(Shade * 1.02, Shade, Shade * 0.97);
					Out.push_back(P);
				}
			}
	}
}
