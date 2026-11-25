#include "Battle/LandField.h"

#include "Battle/LandHouse.h"

#include "Battle/LandScape.h"
#include "Battle/LandSteppe.h"

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
		/** The clutter's squares: everything laid round the eye comes from its own square's numbers. */
		constexpr double ClutterSquare = 8.0;
		uint64_t SquareKeyIJ(int64_t I, int64_t J) { return (static_cast<uint64_t>(I) << 32) ^ static_cast<uint64_t>(static_cast<uint32_t>(J)); }
		uint64_t SquareKeyAt(double X, double Y)
		{
			return SquareKeyIJ(static_cast<int64_t>(std::floor(X / ClutterSquare)), static_cast<int64_t>(std::floor(Y / ClutterSquare)));
		}
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
			CpOther,
			// the steppe's (LandSteppe): winter rye ripe and tall, the collective farm's pasture and hay, black fallow
			CpRye,
			CpSteppe,
			CpFallow,
			// a region's own (LandScape::Region::CropOf): CpRegion + its number, 0 to 63
			CpRegion = 64
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
			bool bKit = false;   // the steppe's cottage: the east kit's Khata stands here (planted, not built)
			Base KitB = Base::Khata; // (a region's kit house: its slot, Kit0..Kit19)
			bool bWhite = false; // the steppe's farm buildings: whitewashed, their roofs plank or straw
			bool bBoards = false; // (the trial "sheds": the eastern farm's sheds built as sheds, doored and worn at the foot)
			bool bDaub = false;   // (and on the steppe, south of the forest zone: clay on wattle under straw, not boards)
			float Lie[4][18] = {}; // (and the ground along each of its four walls, 18 places a wall: its plinth follows it)
			int Model = -1;      // (the trial "houses": the kit slot of the farmhouse built to the vehicles' detail standing here)
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
			OcGate = 32,    // a field gateway: the ground trodden
			OcCut = 64      // a hedge cutter's cut to come (Field::Cuts): the hedge whole, but no tree in its gap
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
		// (a region's survey grid: the plots square to the compass, unwarped, in half-mile blocks cut square at their halves)
		bool bSurvey = false;
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
		/** The houses filed by 16 m square (the first HGridN of them, in their order), so HouseAt answers a point from its
		 * square, not from every house (a city's thousands); those after HGridN are looked through one by one. */
		SegGrid HGrid;
		size_t HGridN = 0;
		void FileHouses();
		std::vector<Footing> Footings;      // the kit houses' (SeatKitHouses)
		std::vector<Col> FootingPaints;     // and each one's colour
		/** The farmhouses, barns, byres, church and village houses built to the vehicles' detail (the trial "houses"), one a kind: kit slot HouseSlot0 + K is HouseModels[K]
		 * (HouseSlot0 the bocage's 0; a region's the first slot after its own kit's). */
		std::vector<LandHouse::Spec> HouseModels;
		int HouseSlot0 = 0;
		/** A house slot that stands for a region's kit it set down as a thing (the east's burnt-out houses): that kit's slot (-1
		 * none) and the scale it was planted at, so the slot keeps the thing's shape in sight. */
		std::vector<int> HouseFrom;
		std::vector<double> HouseFromScale, HouseFromScaleZ;
		std::vector<V2> WayA, WayB;
		std::vector<double> WayHalf;
		std::vector<int> WayOf;       // each way piece's way (Fd.Ways' index)
		std::vector<uint32_t> WayK;   // and its first point there
		std::vector<HedgeCrossing> CrossOut;
		/** Whether a place lies within a cut's gap and Extra metres round it (Fd.Cuts). */
		bool NearCut(double X, double Y, double Extra) const
		{
			for (const Field::Cut& C : Fd.Cuts)
			{
				const double R = 0.5 * C.Width + Extra;
				if ((X - C.X) * (X - C.X) + (Y - C.Y) * (Y - C.Y) < R * R) return true;
			}
			return false;
		}
		SegGrid WGrid;
		struct Spot
		{
			V2 P;
			double Yaw = 0.0;
			int What = 0; // 0 well, 1 cart, 2 rubble stones
		};
		std::vector<Spot> Props;
		/** The steppe (Fd.Steppe on): its farm fields, railway and bank; and the kit's things to plant (burnt cottages,
		 * wattle, well sweeps, haystacks), each where MakeSites put it. */
		std::unique_ptr<LandSteppe::Plan> Sp;
		struct KitSpot
		{
			Base B = Base::Haystack;
			V2 P;
			double Yaw = 0.0, Scale = 1.0, ScaleZ = 1.0;
		};
		std::vector<KitSpot> Kits;
		/** A region's things set down while its sites are laid, planted with the rest (each with its own tint). */
		struct RegionKit
		{
			KitSpot K;
			Rgb8 Tint;
		};
		std::vector<RegionKit> RegionKits;
		/** The look laid (LandScape) and, where it is neither the bocage nor the steppe, the region that paints and
		 * plants it; the snow lying over any of them (null where none). PlainFx: the effects word of open ground. */
		LandScape::Look LookIs = LandScape::Look::Bocage;
		std::unique_ptr<LandScape::Region> Rg;
		std::unique_ptr<LandScape::Snow> SnowLay;
		double SnowIs = 0.0;
		uint8_t PlainFx = 0;
		/** The thaw's wet ground drawn (on trial, "wet", where Field::Weather lays the mud), or rain's: Wetted. */
		bool bWet = false;
		/** Rain's wetness on the open ground (Field::Rain, none over snow), 0 to 1: 0 where the wet is the thaw's. */
		double RainWet = 0.0;
		/** Each field's own tone and state over its crop's paint (on trial, "fieldtone"): Toned. */
		bool bTone = false;
		struct RegionHost;
		/** A place as a region sees it (the parcel, its edges, the lanes and ways; with LandField's noises if bNoise). */
		LandScape::Spot SpotAt(double X, double Y, bool bNoise) const;
		Col RegionColour(double X, double Y, const LandScape::Spot* Pre = nullptr) const;
		Col RegionFar(double X, double Y) const;
		/** A region's road across its floor and the slope to the ground beside (A metres from its middle, Half its half
		 * width): the region's own track, not the bocage's grassed lane and fern walls. */
		Col RegionLane(double X, double Y, double A, double Half) const;
		Col Snowed(const Col& C, double X, double Y, bool bFar) const;

		/** The cattle's paths (MakeCowPaths): their segments, each with its half width; and the pieces of trodden earth
		 * laid along them, sorted by the clutter's 8 m squares (CowSquares: a square's first piece and its end). */
		std::vector<V2> CowA, CowB;
		std::vector<double> CowHalf;
		SegGrid CGrid;
		std::vector<Placed> CowBits;
		std::unordered_map<uint64_t, std::pair<uint32_t, uint32_t>> CowSquares;
		/** On trial ("feet"): where a bush's body, a tree's trunk or its low boughs, a stack or a well stands in the grass
		 * (its place, the ground it stands on and its radius over the lowest metre), sorted by the clutter's 8 m squares
		 * it reaches into (FootSquares: a square's first and its end; one that lies across a square's edge is in each).
		 * The clutter keeps out of them what would stand up into the body: no grass blowing through a bush; the ferns on
		 * a bank's flank under a hedge, their tops below its shrubs' feet, stay. Empty with the word off. */
		// (GrassFoot, not Footing: LandField::Footing is a kit house's footing, 1.5)
		struct GrassFoot
		{
			float X = 0.0f, Y = 0.0f, Z = 0.0f, R = 0.0f;
		};
		std::vector<GrassFoot> Feet;
		std::unordered_map<uint64_t, std::pair<uint32_t, uint32_t>> FootSquares;
		/** The footings' dial, on trial with them ("feetwide"): off, a clump may stand with half its own width inside a
		 * footing's edge (as first built); on, its whole body and its blades' sweep in a full gust stand clear. */
		bool bFeetWide = false;
		/** Burnt ground (Ground::AddBurn; none unless the scorch layer gives some): rounds in which the clutter thins
		 * and dries, by the clutter's 8 m squares each reaches into. Core: within it 3 things in 10 stay; Outer: from
		 * there on all stay. BaseMean: each base's mean colour as built, to bring what stays toward straw (made at the
		 * first round). */
		struct Burn
		{
			float X = 0.0f, Y = 0.0f, Core = 0.0f, Outer = 0.0f;
		};
		std::unordered_map<uint64_t, std::vector<Burn>> BurnSquares;
		std::vector<Col> BaseMean;

		std::vector<HedgeLine> HedgeOut;
		std::vector<HedgeOpening> OpeningOut;
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
		double Terrain(double X, double Y) const { return Land(X, Y) + (Sp ? Sp->BankRise(X, Y) : Rg ? 0.0 : Ridges(X, Y)); }
		double Sunk(double X, double Y) const;
		double Carve(double X, double Y, double Z, double* OutEdge) const;
		double BankAt(double X, double Y) const;
		/** How far the nearest bank's middle line lies from here, looked for out to Reach metres (more than Reach where none is). */
		double BankNear(double X, double Y, double Reach) const;
		/** The nearest lane's half width (its floor), 2.2 where none is near. */
		double LaneHalfAt(double X, double Y) const;
		double Top(double X, double Y, Drawn* What) const;
		/** The ground under everything laid on it, as the tiles take it: the far tiles' height. */
		double UnderZ(double X, double Y) const;
		/** The near tiles' height: carved for the lanes, a little lower under a lane's own mesh. */
		double NearZ(double X, double Y) const;
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
		double WayRoom(V2 P, bool bVehicles = false) const;
		/** Whether B is a slot of the region's kit it calls low (Region::KitLow). */
		bool KitLowAt(Base B) const
		{
			const int K = static_cast<int>(B) - static_cast<int>(Base::Kit0);
			return Rg && K >= 0 && K < KitSlots && Rg->KitLow(K);
		}
		/** A region's low kit standing taller than LowKitHigh after all, once its models are built: taken up where it lies
		 * on a recorded men's way. */
		void TakeUpTallLowKit();
		void MakeFeet();
		void AddBurn(double X, double Y, double Core, double Outer);
		void MakeSites();
		struct House ChurchYard(V2 C, double Yaw) const;
		void LayChurch(V2 C, double Yaw, int Hurt);
		void MakeSteppeSites();
		bool Fits(const House& H, double Margin) const;
		/** Fits for a terrace's house (the city's): wall to wall with the houses beside it, so only its own footprint kept
		 * off what is built (less a pixel and a bit at its edges, where a neighbour's stamp reaches) and the men's and
		 * vehicles' ways. */
		bool FitsRow(const House& H) const;
		void Stamp(const House& H, double Margin);
		void FloorOf(House& H) const;
		void SeatKitHouses();
		void AddWall(const Line& L, double High, double Thick, uint64_t Key);
		void MakeParcels();
		void MakeDistances();
		void MakeHedges();
		void MakeCowPaths();
		/** Metres from a place to the nearest cow path's edge (less than nothing on it; a lot where none is near). */
		double CowPath(double X, double Y) const;
		void MakePlants();
		void Plant(Base B, double X, double Y, double Yaw, double Scale, double ScaleZ, Rgb8 Tint, bool bThrough = false);

		// meshes
		Col GroundColour(double X, double Y) const;
		/** The thaw's wet ground over a colour where bWet: a fifth darker, sodden, and mud standing in low patches. Under
		 * rain (RainWet): soaked darker the harder it has fallen, and water standing in pools in the hollows, more of
		 * them the harder it falls, the grey of the sky over it; the sheen is the ground material's (LandBattle lays it
		 * wet). */
		Col Wetted(Col C, double X, double Y) const
		{
			if (!bWet) return C;
			if (RainWet <= 0.0)
			{
				const double Puddle = Smooth(0.25, 0.6, Noise(X / 23.0, Y / 23.0, Seed ^ 0x3E7));
				return Mixed(Varied(C, 0.8, 0.0), Rgb(66, 58, 44), 0.35 * Puddle);
			}
			const double W = std::clamp(RainWet, 0.0, 1.0);
			// (the hollows: the bottoms of the ground's own 24 m dips, Land's finest relief, the same noise, so the water
			// stands where the drawn ground is low; broken into pools by a finer noise. Two noises a point, not a look round
			// it: the colour is asked at every point of the ground and every track mark)
			const double Dip = -Noise(X / 24.0, Y / 24.0, Seed ^ 0xA3);
			const double Patch = Noise(X / 6.0, Y / 6.0, Seed ^ 0x5A1);
			// Water lies on bare ground: the lanes, the trodden gateways, the ploughed earth, the holes (Room 1); none in
			// grass or standing corn, which drink it. Read from the colour, which knows them all (the art director's R6;
			// round 2's B: soft pools in a meadow read as bruises until the ground's material carries a wet mask with a
			// firm edge, so none there). The water the grey of the sky it holds, as the general's slanting eye sees it
			// (Water).
			// (bare earth and dust: browns, hue under about 45 degrees and not bright gold; growing things, grass and
			// crops, greener or richer than that)
			const double Most = std::max({ C.R, C.G, C.B }), Least = std::min({ C.R, C.G, C.B }), Span = Most - Least;
			const double Sat = Most > 0.0 ? Span / Most : 0.0;
			double Hue = 0.0;
			if (Span > 1e-6)
			{
				Hue = Most == C.R ? 60.0 * (C.G - C.B) / Span : Most == C.G ? 60.0 * ((C.B - C.R) / Span + 2.0) : 60.0 * ((C.R - C.G) / Span + 4.0);
				if (Hue < 0.0) Hue += 360.0;
			}
			const double Growing = Smooth(42.0, 52.0, Hue) * (Hue < 200.0 ? 1.0 : 0.0);
			const double Room = (1.0 - Growing) * (1.0 - Smooth(0.42, 0.52, Sat));
			const double From = 0.95 - 0.3 * W * Room;
			const double Puddle = Room <= 0.02 ? 0.0 : Smooth(From, From + 0.15, Dip + 0.35 * Patch) * (0.4 + 0.6 * Room);
			return Mixed(Soaked(C), Water, 0.8 * Puddle);
		}
		/** Standing water's colour: the overcast's grey it mirrors, lighter than the wet earth round it, as ruts and
		 * pools shine silver from a slanting eye (the art director's R6). */
		static constexpr Col Water{ 118.0, 123.0, 130.0 };
		/** Rain's soak on a colour (RainWet): darker the harder it falls, and the paler and drier it was the darker it
		 * goes (a lane's dust most, to about 55% of dry), as wet earth does; ripe corn and stubble, a bright gold, to
		 * about 80%, as the corn standing over it (the Rgb8 Soaked), and still gold. The same over the far ground
		 * (FarColour), so near and far, and the general's view and the eye's, meet. */
		Col Soaked(const Col& C) const
		{
			if (RainWet <= 0.0) return C;
			const double W = std::clamp(RainWet, 0.0, 1.0);
			const double Light = (0.3 * C.R + 0.59 * C.G + 0.11 * C.B) / 255.0;
			// (wet colour deepens, never greys: away from its own grey by more than half at the wettest, against the grey
			// day's light and the haze, which drain it; the art director's R2. A lane's dust to about 55% of dry, green
			// grass hardly darker: the grey day and the haze darken it enough)
			// (a bright gold, ripe corn's and stubble's, the art director's round 2 A: darkened by as much as the corn
			// standing in it and no richer, so it stays gold seen from above and does not turn burnt orange; its red over
			// green kept, within 0.05)
			const double Most = std::max({ C.R, C.G, C.B }), Least = std::min({ C.R, C.G, C.B });
			const double Sat = Most > 0.0 ? (Most - Least) / Most : 0.0, Gold = Smooth(0.45, 0.6, Sat) * (C.R >= C.G && C.G > C.B ? 1.0 : 0.0);
			const double Grey = (C.R + C.G + C.B) / 3.0, Rich = 1.0 + 0.6 * W * (1.0 - Sat) * (1.0 - Gold);
			// (its red a touch less, the grade's curve warming a darker gold by about as much)
			const Col Deep{ std::max(0.0, Grey + (C.R - Grey) * Rich) * (1.0 - 0.03 * W * Gold), std::max(0.0, Grey + (C.G - Grey) * Rich), std::max(0.0, Grey + (C.B - Grey) * Rich) };
			return Varied(Deep, 1.0 - W * (0.05 + 0.45 * Smooth(0.35, 0.6, Light) * (1.0 - Gold)), 0.0);
		}
		/** Rain's soak on a thing's tint (the corn standing, the stubble, the hay in its cocks and stacks): darker the
		 * harder it falls, to about three quarters of dry in a downpour, its hue kept (the art director's R3: red over
		 * green kept 1.05 to 1.15; round 2's A, red over green up by no more than 0.05: the grade's curve warms a darker
		 * gold by itself, so its red a touch less). As it was where no rain falls. */
		Rgb8 Soaked(const Rgb8& T) const
		{
			if (RainWet <= 0.0) return T;
			const double W = std::clamp(RainWet, 0.0, 1.0), Keep = 1.0 - 0.25 * W;
			const auto One = [Keep](double V) { return static_cast<uint8_t>(std::clamp(std::lround(V * Keep), 0L, 255L)); };
			Rgb8 Out;
			Out.R = One(T.R * (1.0 - 0.03 * W));
			Out.G = One(T.G);
			Out.B = One(T.B);
			return Out;
		}
		/** Whether a thing's kind is straw or corn that rain soaks darker (Soaked). */
		static bool Soaks(Base B)
		{
			return B == Base::Wheat || B == Base::Stalks || B == Base::Stubble || B == Base::Haycock || B == Base::Haystack;
		}
		Col GroundBare(double X, double Y, const LandScape::Spot* Pre = nullptr) const;
		/** A parcel's own tone and state over its plants' paint where bTone (C as it is everywhere else). */
		Col Toned(Col C, double X, double Y) const;
		Col SteppeColour(double X, double Y, const LandScape::Spot* Pre = nullptr) const;
		Col FarColour(double X, double Y) const;
		Col FarBare(double X, double Y) const;
		void MakeTiles();
		void MakeRing();
		void MakeBases();
	};

	/** What a region may ask of the ground being laid, and lay on it (LandScape::Host). While the sites are laid
	 * (bDefer) its plantings wait for the planting step, when the ground under them is final. */
	struct Ground::Impl::RegionHost final : LandScape::Host
	{
		Impl& G;
		bool bDefer = false;
		std::vector<std::vector<std::pair<double, double>>> LaneLines;

		RegionHost(Impl& In, bool bInDefer) : G(In), bDefer(bInDefer)
		{
			for (const Lane& L : G.Lanes)
			{
				std::vector<std::pair<double, double>> P;
				P.reserve(L.P.size());
				for (const V2& Q : L.P) P.push_back({ Q.X, Q.Y });
				LaneLines.push_back(std::move(P));
			}
		}
		double Width() const override { return G.FW; }
		double Depth() const override { return G.FH; }
		double Band() const override { return LandField::Band; }
		bool Town(double& OutX, double& OutY, double& OutR) const override
		{
			if (!G.Fd.bTown) return false;
			OutX = G.Fd.TownX;
			OutY = G.Fd.TownY;
			OutR = G.Fd.TownR;
			return true;
		}
		double Land(double X, double Y) const override { return G.Land(X, Y); }
		double Top(double X, double Y) const override { return G.Top(X, Y, nullptr); }
		LandScape::Spot At(double X, double Y) const override
		{
			LandScape::Spot S = G.SpotAt(X, Y, false);
			Drawn What = Drawn::Pasture;
			S.Z = G.Top(X, Y, &What);
			S.Drawn = static_cast<int>(What);
			return S;
		}
		bool Free(double X, double Y, double Room, bool bManMay) const override
		{
			if (!G.InRaster(X, Y)) return false;
			const size_t P = G.Pix(X, Y);
			if (G.OccR[P] & (OcHouse | OcNear | OcWall)) return false;
			if (G.LaneD[P] < Room + 0.3) return false;
			return G.WayRoom({ X, Y }, bManMay) >= Room;
		}
		void Plant(Base B, double X, double Y, double Yaw, double Scale, double ScaleZ, Rgb8 Tint) override
		{
			if (bDefer)
			{
				KitSpot K;
				K.B = B;
				K.P = { X, Y };
				K.Yaw = Yaw;
				K.Scale = Scale;
				K.ScaleZ = ScaleZ;
				G.RegionKits.push_back({ K, Tint });
				return;
			}
			G.Plant(B, X, Y, Yaw, Scale, ScaleZ, Tint);
		}
		bool House(Base B, double X, double Y, double Yaw, double Long, double Deep) override
		{
			struct House H;
			H.Type = HtHouse;
			H.bKit = true;
			H.KitB = B;
			H.Long = Long;
			H.Deep = Deep;
			H.C = { X, Y };
			H.Yaw = Yaw;
			H.Key = Hash(G.Seed ^ 0x40B5E, static_cast<uint64_t>(std::llround(X * 8.0)), static_cast<uint64_t>(std::llround(Y * 8.0)));
			if (!G.Fits(H, 1.2)) return false;
			G.FloorOf(H);
			G.Stamp(H, 1.0);
			G.Houses.push_back(H);
			return true;
		}
		bool Row(Base B, double X, double Y, double Yaw, double Long, double Deep) override
		{
			struct House H;
			H.Type = HtHouse;
			H.bKit = true;
			H.KitB = B;
			H.Long = Long;
			H.Deep = Deep;
			H.C = { X, Y };
			H.Yaw = Yaw;
			H.Key = Hash(G.Seed ^ 0x40B5E, static_cast<uint64_t>(std::llround(X * 8.0)), static_cast<uint64_t>(std::llround(Y * 8.0)));
			if (!G.FitsRow(H)) return false;
			G.FloorOf(H);
			G.Stamp(H, 0.3);
			G.Houses.push_back(H);
			return true;
		}
		bool Building(double X, double Y, double Yaw, double Long, double Deep, int Type, int Storeys, int Roof, bool bWhite, int Hurt) override
		{
			struct House H;
			H.Type = std::clamp(Type, static_cast<int>(HtHouse), static_cast<int>(HtByre));
			H.Storeys = std::clamp(Storeys, 1, 2);
			H.Roof = std::clamp(Roof, 0, 1);
			H.bWhite = bWhite;
			H.Hurt = std::clamp(Hurt, static_cast<int>(DmNone), static_cast<int>(DmShell));
			H.Long = Long;
			H.Deep = Deep;
			H.C = { X, Y };
			H.Yaw = Yaw;
			H.Key = Hash(G.Seed ^ 0xB01D5ull, static_cast<uint64_t>(std::llround(X * 8.0)), static_cast<uint64_t>(std::llround(Y * 8.0)));
			if (!G.Fits(H, 1.2)) return false;
			G.FloorOf(H);
			G.Stamp(H, 1.0);
			G.Houses.push_back(H);
			return true;
		}
		bool Church(double X, double Y, double Yaw, int Hurt) override
		{
			if (G.Count.bChurch || !G.Fits(G.ChurchYard({ X, Y }, Yaw), 0.5)) return false;
			G.LayChurch({ X, Y }, Yaw, Hurt > 0 ? DmRoofHole : DmNone);
			return true;
		}
		void Wall(const std::vector<std::pair<double, double>>& Points, double High, double Thick) override
		{
			Line L;
			L.reserve(Points.size());
			for (const auto& [X, Y] : Points) L.push_back({ X, Y });
			if (L.size() >= 2) G.AddWall(L, High, Thick, Hash(G.Seed ^ 0xA11Bull, static_cast<uint64_t>(G.Walls.size())));
		}
		void Clear(double X, double Y, double R) override
		{
			const int I0 = std::max(0, static_cast<int>((X - R - G.RX0) / Res)), I1 = std::min(G.NX - 1, static_cast<int>((X + R - G.RX0) / Res));
			const int J0 = std::max(0, static_cast<int>((Y - R - G.RY0) / Res)), J1 = std::min(G.NY - 1, static_cast<int>((Y + R - G.RY0) / Res));
			for (int J = J0; J <= J1; ++J)
				for (int I = I0; I <= I1; ++I)
					if (std::hypot(G.RX0 + (I + 0.5) * Res - X, G.RY0 + (J + 0.5) * Res - Y) < R) G.OccR[static_cast<size_t>(J) * G.NX + I] |= OcNear;
		}
		size_t Lanes() const override { return LaneLines.size(); }
		const std::vector<std::pair<double, double>>& LaneLine(size_t K) const override { return LaneLines[K]; }
		double LaneHalf(size_t K) const override { return G.Lanes[K].Half; }
		uint64_t Seed() const override { return G.Seed ^ 0x5CA9E5ull; }
		void Tally(int NewHouses, int NewDamaged, int NewFarmsteads) override
		{
			G.Count.Houses += NewHouses;
			G.Count.Damaged += NewDamaged;
			G.Count.Farmsteads += NewFarmsteads;
		}
	};
}

namespace LandField
{
	namespace
	{
		const char* const FxWords[] = { "earth", "town", "woods", "water", "mud", "sand", "snow", "rock" };

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
		if (Rg) return Rg->Rolling(X, Y, MeanZ);
		// (the steppe's swells are half as high and longer, its balkas cut into them)
		if (Sp) return MeanZ + 9.0 * Fbm(X, Y, 3400.0, 3, Seed ^ 0xB1) + 2.0 * Fbm(X, Y, 900.0, 2, Seed ^ 0xB2);
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
		if (Rg) Z = Rg->Relief(X, Y, Z); // (dunes, wadis, terraces, a beach's fall: the region's own)
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
		// (a steppe road is no sunken lane: a dirt track worn a hand or two into the black earth by carts and lorries)
		double D = Rg ? Rg->LaneDepth(X, Y) : Sp ? 0.15 + 0.15 * (0.5 + 0.5 * Noise(X / 150.0, Y / 150.0, Seed ^ 0xC1)) : 0.6 + 0.8 * (0.5 + 0.5 * Noise(X / 150.0, Y / 150.0, Seed ^ 0xC1));
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

	double Ground::Impl::LaneHalfAt(double X, double Y) const
	{
		const std::vector<uint32_t>* Near = LGrid.At(X, Y);
		double Best = 1e18, Half = 2.2;
		if (Near)
			for (const uint32_t Id : *Near)
			{
				const LaneSeg& S = LSegs[Id];
				double T;
				const double D = SegDist({ X, Y }, S.A, S.B, T);
				if (D < Best)
				{
					Best = D;
					Half = Lanes[static_cast<size_t>(S.Lane)].Half;
				}
			}
		return Half;
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

	double Ground::Impl::BankNear(double X, double Y, double Reach) const
	{
		double Best = Reach + 1.0;
		if (BGrid.Cells.empty()) return Best;
		const V2 P{ X, Y };
		const int I0 = std::max(0, static_cast<int>(std::floor((X - Reach - BGrid.X0) / BGrid.Size)));
		const int I1 = std::min(BGrid.NX - 1, static_cast<int>(std::floor((X + Reach - BGrid.X0) / BGrid.Size)));
		const int J0 = std::max(0, static_cast<int>(std::floor((Y - Reach - BGrid.Y0) / BGrid.Size)));
		const int J1 = std::min(BGrid.NY - 1, static_cast<int>(std::floor((Y + Reach - BGrid.Y0) / BGrid.Size)));
		for (int J = J0; J <= J1; ++J)
			for (int I = I0; I <= I1; ++I)
				for (const uint32_t Id : BGrid.Cells[static_cast<size_t>(J) * BGrid.NX + I])
				{
					const BankSeg& S = BSegs[Id];
					const Run& R = Runs[S.Run];
					double T;
					Best = std::min(Best, SegDist(P, R.P[S.K], R.P[S.K + 1], T));
				}
		return Best;
	}

	void Ground::Impl::FileHouses()
	{
		if (HGridN == Houses.size()) return;
		HGrid.Init(RX0, RY0, RX0 + NX * Res, RY0 + NY * Res, 16.0);
		for (size_t K = 0; K < Houses.size(); ++K)
		{
			const House& H = Houses[K];
			HGrid.Add(static_cast<uint32_t>(K), H.C, H.C, 0.5 * std::hypot(H.Long, H.Deep) + 0.05);
		}
		HGridN = Houses.size();
	}

	int Ground::Impl::HouseAt(double X, double Y) const
	{
		if (!InRaster(X, Y) || !(OccR[Pix(X, Y)] & OcHouse)) return -1;
		const auto Inside = [this, X, Y](size_t K)
		{
			const House& H = Houses[K];
			const V2 D{ X - H.C.X, Y - H.C.Y };
			const double U = D.X * std::cos(H.Yaw) + D.Y * std::sin(H.Yaw), V = -D.X * std::sin(H.Yaw) + D.Y * std::cos(H.Yaw);
			return std::fabs(U) <= 0.5 * H.Long && std::fabs(V) <= 0.5 * H.Deep;
		};
		// (the filed houses from their square, each square's in their order, so the first found is the first in the list,
		// as looking through them all finds it; then any built after they were filed)
		size_t From = 0;
		if (HGridN > 0)
		{
			if (const std::vector<uint32_t>* Near = HGrid.At(X, Y))
				for (const uint32_t K : *Near)
					if (Inside(K)) return static_cast<int>(K);
			From = HGridN;
		}
		for (size_t K = From; K < Houses.size(); ++K)
			if (Inside(K)) return static_cast<int>(K);
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
						*What = P < Pid.size() && Pid[P] >= 0 && Parcels[static_cast<size_t>(Pid[P])].Kind == RgField ? Drawn::Crop : Drawn::Pasture;
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
		PlainFx = bDesert ? 5 : 0;
		if (Rg) PlainFx = static_cast<uint8_t>(std::clamp(Rg->Fx("open", PlainFx), 0, 7));
		if (SnowIs >= 0.5) PlainFx = 6;
		// (rain falling, Field::Rain: none where snow lies, as LandWeather's pick.snow_lying_from)
		RainWet = SnowIs < 0.3 && std::isfinite(Fd.Rain) ? std::clamp(Fd.Rain, 0.0, 1.0) : 0.0;
		if ((Fd.Weather == "mud" || RainWet > 0.0) && PlainFx == 0) PlainFx = 4; // (the thaw's mud over open earth: Field::Weather; or rain's)
		bWet = PlainFx == 4 && Fd.Weather == "mud" && !Fd.Trial.empty() && Fd.Tried("wet"); // (its look, on trial)
		if (RainWet > 0.0) bWet = true; // (rain's wet ground, wherever it falls: Wetted)
		bTone = !Fd.Trial.empty() && Fd.Tried("fieldtone"); // (each field its own tone: on trial)
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
			if (G == "bunker" && bDesert) Fx = 0;
			if (G == "rock") Fx = 7; // (bare rock: a low wide grey spray and stone chips, a pale bruise, not a crater)
			if (Rg) Fx = static_cast<uint8_t>(std::clamp(Rg->Fx(G, Fx), 0, 7));
			// (deep snow: a shell throws snow and the earth under it, but not in a wood, a town or the water)
			if (SnowIs >= 0.5 && (Fx == 0 || Fx == 4 || Fx == 5)) Fx = 6;
			RgOf[K] = R;
			FxOfCell[K] = Fx;
		}
		// The rivers' beds (release 1.5 mend; Brian 24 November, "their is a huge ditch in the map", at Monterrey): the
		// rules lay a river's water and its bridges at sea level less 2 m (field.ts: min(height, 0) - 2) whatever the
		// ground round it, so in hills (relief to 70 m) the Rio Grande ran 30 to 55 m down a sheer-sided cut, and in
		// mountains to 180 m. The fight keeps the rules' own heights; here the drawn water sits 2 m under the lowest bank
		// within 150 m. Only water with a bank within 75 m everywhere is a river: the sea along a shore keeps its level.
		// -TheaterLandOldRiverBed (Field::bOldRiverBed): as before.
		if (!Fd.bOldRiverBed && Fd.Cols > 0 && Fd.Rows > 0)
		{
			std::vector<uint8_t> WetOf(Fd.Grounds.size(), 0);
			for (size_t K = 0; K < Fd.Grounds.size(); ++K)
			{
				const std::string& G = Fd.Grounds[K];
				WetOf[K] = G == "water" || G == "blown" || G == "bridge" ? 1 : 0;
			}
			const int C = Fd.Cols, R = Fd.Rows;
			std::vector<uint8_t> Wet(N, 0);
			for (size_t K = 0; K < N; ++K)
			{
				const int G = Fd.Ground[K];
				Wet[K] = G >= 0 && static_cast<size_t>(G) < WetOf.size() ? WetOf[static_cast<size_t>(G)] : 0;
			}
			constexpr int Near = 3, Reach = 6;
			// Each wet cell: whether a bank is within Near cells, and the lowest bank within Reach.
			std::vector<uint8_t> Banked(N, 0);
			std::vector<double> Low(N, std::numeric_limits<double>::infinity());
			for (int J = 0; J < R; ++J)
				for (int I = 0; I < C; ++I)
				{
					const size_t K = static_cast<size_t>(J) * C + I;
					if (!Wet[K]) continue;
					for (int B = std::max(0, J - Reach); B <= std::min(R - 1, J + Reach); ++B)
						for (int A = std::max(0, I - Reach); A <= std::min(C - 1, I + Reach); ++A)
						{
							const size_t Q = static_cast<size_t>(B) * C + A;
							if (Wet[Q]) continue;
							Low[K] = std::min(Low[K], Fd.Height[Q]);
							if (std::max(A - I, I - A) <= Near && std::max(B - J, J - B) <= Near) Banked[K] = 1;
						}
				}
			// Each body of water (side by side, not corner to corner) whole: a river only if every cell of it is banked.
			std::vector<uint8_t> Seen(N, 0);
			std::vector<size_t> Body, Todo;
			for (size_t K0 = 0; K0 < N; ++K0)
			{
				if (!Wet[K0] || Seen[K0]) continue;
				Body.clear();
				Todo.assign(1, K0);
				Seen[K0] = 1;
				bool bRiver = true;
				while (!Todo.empty())
				{
					const size_t K = Todo.back();
					Todo.pop_back();
					Body.push_back(K);
					if (!Banked[K] || !std::isfinite(Low[K])) bRiver = false;
					const int I = static_cast<int>(K % static_cast<size_t>(C)), J = static_cast<int>(K / static_cast<size_t>(C));
					const int Ni[4] = { I - 1, I + 1, I, I }, Nj[4] = { J, J, J - 1, J + 1 };
					for (int D = 0; D < 4; ++D)
					{
						if (Ni[D] < 0 || Nj[D] < 0 || Ni[D] >= C || Nj[D] >= R) continue;
						const size_t Q = static_cast<size_t>(Nj[D]) * C + Ni[D];
						if (!Wet[Q] || Seen[Q]) continue;
						Seen[Q] = 1;
						Todo.push_back(Q);
					}
				}
				if (!bRiver) continue;
				for (const size_t K : Body) Fd.Height[K] = std::max(Fd.Height[K], Low[K] - 2.0);
			}
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
			// (none where the land was surveyed: its roads run the section lines straight)
			const double Total = LineLength(L), Wanders = Rg && Rg->Surveyed() ? 0.0 : 1.0;
			Line W(L.size());
			double S = 0.0;
			for (size_t K = 0; K < L.size(); ++K)
			{
				if (K > 0) S += Len(L[K] - L[K - 1]);
				const V2 Way = Unit2(L[std::min(K + 1, L.size() - 1)] - L[K > 0 ? K - 1 : 0]);
				const double Amp = 2.5 * std::min({ 1.0, S / 40.0, (Total - S) / 40.0 });
				double Bend = 12.0 * Smooth(0.0, 1.0, std::min({ 1.0, S / 110.0, (Total - S) / 110.0 }));
				if (Fd.bTown) Bend *= Smooth(Fd.TownR * 0.9, Fd.TownR * 1.5, std::hypot(L[K].X - Fd.TownX, L[K].Y - Fd.TownY));
				W[K] = L[K] + Perp(Way) * Wanders * (Amp * Noise(S / 80.0, Index * 7.31, Seed ^ 0xE1) + Bend * Noise(S / 260.0, Index * 3.77 + 0.5, Seed ^ 0xE2));
			}
			Lane Ln;
			Ln.P = Resample(W, 2.0);
			bool bVillage = false;
			for (const V2& Q : Ln.P)
				if (Fd.bTown && std::hypot(Q.X - Fd.TownX, Q.Y - Fd.TownY) < Fd.TownR) bVillage = true;
			Ln.Half = bVillage ? 2.6 : 2.0 + 0.4 * Unit(Hash(Seed, 0x1A4E, static_cast<uint64_t>(Index)));
			if (Sp) Ln.Half = (bVillage ? 3.4 : 2.8) + 0.8 * Unit(Hash(Seed, 0x1A4E, static_cast<uint64_t>(Index))); // (wide, rutted, unfenced)
			if (Rg) Ln.Half = Rg->LaneHalf(bVillage, Unit(Hash(Seed, 0x1A4E, static_cast<uint64_t>(Index))));
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
		// (on the steppe the oak woods are few: some one ground in twenty-five, the groves in the balkas' heads)
		double Edge = Sp ? 0.43 : Rg ? Rg->WoodsEdge() : 0.27;
		// (on trial, "edge": the woods held back from the field's edge along a wandering line, out to some 160 m, so a
		// forest's edge bays and juts and never runs straight along the field's: the art director's winter carry, the
		// Ardennes' straight outer band, for every ground)
		if (!Sp && !Fd.Trial.empty() && Fd.Tried("edge"))
		{
			const double Out = std::max({ 0.0, -X, X - Fd.Width(), -Y, Y - Fd.Depth() });
			Edge += 0.45 * Smooth(90.0, 0.0, Out + 70.0 * Noise(X / 110.0, Y / 110.0, Seed ^ 0x5B2E));
		}
		return Fbm(X, Y, 520.0, 3, Seed ^ 0xD5) > Edge;
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
		for (size_t Wi = 0; Wi < Fd.Ways.size(); ++Wi)
		{
			const Field::Way& W = Fd.Ways[Wi];
			const bool bVehicle = W.HalfWidth >= VehicleHalf;
			// (a vehicle's: every pixel any part of which is on it, so no trunk stands in its way)
			const double Clear = W.HalfWidth + (bVehicle ? 0.8 + 0.71 * Res : 0.6);
			for (size_t K = 0; K + 1 < W.Points.size(); ++K)
			{
				const V2 A{ W.Points[K].first, W.Points[K].second }, B{ W.Points[K + 1].first, W.Points[K + 1].second };
				if (Len(B - A) < 1e-6 && K + 2 < W.Points.size()) continue;
				// (a hedge cutter's cut to come: no way through the hedge there yet, the cutter's nor the men's who use the
				// gap after her, so the hedge is laid whole and the scene tears it open when she bursts through)
				if (!Fd.Cuts.empty())
				{
					bool bAtCut = false;
					for (const Field::Cut& C : Fd.Cuts)
					{
						double T;
						if (SegDist({ C.X, C.Y }, A, B, T) < 0.5 * C.Width + 3.0)
						{
							bAtCut = true;
							break;
						}
					}
					if (bAtCut) continue;
				}
				WGrid.Add(static_cast<uint32_t>(WayA.size()), A, B, 2.5);
				WayA.push_back(A);
				WayB.push_back(B);
				WayHalf.push_back(W.HalfWidth);
				WayOf.push_back(static_cast<int>(Wi));
				WayK.push_back(static_cast<uint32_t>(K));
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
		// A cut's gap and a metre and a half round it: no tree stands there (the cutter's blade takes the bank and its
		// shrubs, never an oak).
		// (every raster cell any of that disc lies in: its middle within the disc's reach and half a cell's diagonal)
		for (const Field::Cut& C : Fd.Cuts)
		{
			const double R = 0.5 * C.Width + 1.5, Reach = R + 0.7072 * Res;
			const int I0 = std::max(0, static_cast<int>(std::floor((C.X - Reach - RX0) / Res))), I1 = std::min(NX - 1, static_cast<int>(std::floor((C.X + Reach - RX0) / Res)));
			const int J0 = std::max(0, static_cast<int>(std::floor((C.Y - Reach - RY0) / Res))), J1 = std::min(NY - 1, static_cast<int>(std::floor((C.Y + Reach - RY0) / Res)));
			for (int J = J0; J <= J1; ++J)
				for (int I = I0; I <= I1; ++I)
					if (std::hypot(RX0 + (I + 0.5) * Res - C.X, RY0 + (J + 0.5) * Res - C.Y) <= Reach) OccR[static_cast<size_t>(J) * NX + I] |= OcCut;
		}
	}

	/** How far a place lies off the recorded ways: metres past the nearest one's half width (a lot where none is near);
	 * bVehicles, off the vehicles' ways only. */
	double Ground::Impl::WayRoom(V2 P, bool bVehicles) const
	{
		double Room = 1e9;
		if (const std::vector<uint32_t>* Near = WGrid.At(P.X, P.Y))
			for (const uint32_t W : *Near)
			{
				if (bVehicles && WayHalf[W] < VehicleHalf) continue;
				double T;
				Room = std::min(Room, SegDist(P, WayA[W], WayB[W], T) - WayHalf[W]);
			}
		return Room;
	}

	void Ground::Impl::TakeUpTallLowKit()
	{
		if (!Rg) return;
		for (int K = 0; K < KitSlots; ++K)
		{
			if (!Rg->KitLow(K)) continue;
			const Base B = static_cast<Base>(static_cast<int>(Base::Kit0) + K);
			const double High = Bases[static_cast<size_t>(B)].Height;
			std::vector<Placed>& L = Inst[static_cast<size_t>(B)];
			const size_t Was = L.size();
			L.erase(std::remove_if(L.begin(), L.end(), [&](const Placed& P) { return High * P.ScaleZ > LowKitHigh && WayRoom({ P.X, P.Y }) < 0.8; }), L.end());
			Count.Instances -= Was - L.size();
		}
	}

	namespace
	{
		/** The most a foot's own reach is widened by for the thing that would stand at its edge (Ground::Clutter); and
		 * with the dial on ("feetwide"), when a clump's whole width and its blades' sweep are kept clear. A blade's
		 * sweep: a fifth of its height (the grass bends 0.14 radians from its root at a full gust and springs a quarter
		 * past it; the corn much the same at its ears). */
		constexpr double FootEdge = 0.45, FootWide = 0.9, BladeSweep = 0.2;

		/** A place a base's body stands in the grass, in the base's own frame: its middle and its radius, metres. */
		struct LowFoot
		{
			double X = 0.0, Y = 0.0, R = 0.0;
		};

		/**
		 * Where a base stands in the grass, as built: the middle of what it has between 0.1 and 1 m up (its body, its
		 * leaves' cards and sprigs, each triangle's edges cut to that band, so a trunk with no ring of points there is
		 * still measured) and the radius round that middle that holds 85 in 100 of it: a hedge shrub's whole body, a
		 * leaning apple's trunk where it stands and not round its root, a spruce's lowest boughs. Nothing where it has
		 * nothing there. A long thin thing (its spread one way under a sixth of the other, by their variances) the grass
		 * grows along and through, a fallen log, a length of fence: nothing; but one in leaf, a curtain of climbers at a
		 * wood's edge, stands in the grass all along itself: a row of rounds down its length, at most twelve.
		 */
		std::vector<LowFoot> LowFeet(const BaseMesh& M)
		{
			constexpr double Lo = 0.1, Hi = 1.0;
			std::vector<double> Xs, Ys;
			double SX = 0.0, SY = 0.0, SXX = 0.0, SYY = 0.0, SXY = 0.0;
			auto Take = [&](double X, double Y) {
				Xs.push_back(X);
				Ys.push_back(Y);
				SX += X;
				SY += Y;
				SXX += X * X;
				SYY += Y * Y;
				SXY += X * Y;
			};
			for (const Mesh* Ms : { &M.Near, &M.Cards, &M.Sprigs })
			{
				const size_t Pts = Ms->Positions.size() / 3;
				for (size_t T = 0; T + 2 < Ms->Triangles.size(); T += 3)
					for (int E = 0; E < 3; ++E)
					{
						const size_t A = Ms->Triangles[T + static_cast<size_t>(E)], B = Ms->Triangles[T + static_cast<size_t>((E + 1) % 3)];
						if (A >= Pts || B >= Pts) continue;
						const float* Pa = &Ms->Positions[A * 3];
						const float* Pb = &Ms->Positions[B * 3];
						const double Za = Pa[2], Zb = Pb[2];
						if (std::max(Za, Zb) < Lo || std::min(Za, Zb) > Hi) continue;
						// (the edge's two ends, each brought along it into the band; a level edge's ends where they are)
						for (int End = 0; End < 2; ++End)
						{
							const double Z = std::clamp(End == 0 ? Za : Zb, Lo, Hi);
							const double U = std::fabs(Zb - Za) > 1e-9 ? (Z - Za) / (Zb - Za) : static_cast<double>(End);
							Take(Pa[0] + (Pb[0] - Pa[0]) * U, Pa[1] + (Pb[1] - Pa[1]) * U);
						}
					}
			}
			std::vector<LowFoot> Out;
			if (Xs.size() < 6) return Out;
			const double N = static_cast<double>(Xs.size()), MX = SX / N, MY = SY / N;
			const double CXX = SXX / N - Sq(MX), CYY = SYY / N - Sq(MY), CXY = SXY / N - MX * MY;
			const double Half = 0.5 * (CXX + CYY), Diff = std::sqrt(Sq(0.5 * (CXX - CYY)) + CXY * CXY);
			const size_t At = std::min(Xs.size() - 1, static_cast<size_t>(0.85 * N));
			auto Centile = [](std::vector<double>& V, size_t K) {
				std::nth_element(V.begin(), V.begin() + static_cast<std::ptrdiff_t>(K), V.end());
				return V[K];
			};
			std::vector<double> D(Xs.size());
			if (Half + Diff > 0.25 && Half - Diff < (Half + Diff) / 6.0)
			{
				if (M.Cards.Triangles.empty() && M.Sprigs.Triangles.empty()) return Out;
				// (its length's way: the wider spread's own direction)
				const double Turn = 0.5 * std::atan2(2.0 * CXY, CXX - CYY), AX = std::cos(Turn), AY = std::sin(Turn);
				for (size_t K = 0; K < Xs.size(); ++K) D[K] = std::fabs(-(Xs[K] - MX) * AY + (Ys[K] - MY) * AX);
				const double Wide = std::max(0.3, Centile(D, At));
				for (size_t K = 0; K < Xs.size(); ++K) D[K] = (Xs[K] - MX) * AX + (Ys[K] - MY) * AY;
				const double T0 = Centile(D, static_cast<size_t>(0.05 * N)), T1 = Centile(D, std::min(Xs.size() - 1, static_cast<size_t>(0.95 * N)));
				const int Rounds = std::clamp(static_cast<int>(std::ceil((T1 - T0) / (1.4 * Wide))), 1, 12);
				for (int K = 0; K < Rounds; ++K)
				{
					const double Tt = Lerp(T0, T1, (K + 0.5) / Rounds);
					Out.push_back({ MX + AX * Tt, MY + AY * Tt, Wide });
				}
				return Out;
			}
			for (size_t K = 0; K < Xs.size(); ++K) D[K] = std::hypot(Xs[K] - MX, Ys[K] - MY);
			Out.push_back({ MX, MY, Centile(D, At) });
			return Out;
		}
	}

	void Ground::Impl::MakeFeet()
	{
		if (Fd.Trial.empty() || !Fd.Tried("feet")) return;
		bFeetWide = Fd.Tried("feetwide");
		struct Reached
		{
			uint64_t Key;
			GrassFoot F;
		};
		std::vector<Reached> All;
		size_t Stand = 0;
		for (size_t B = 0; B <= static_cast<size_t>(Base::Haycock); ++B) Stand += Inst[B].size();
		All.reserve(2 * Stand);
		for (size_t B = 0; B <= static_cast<size_t>(Base::Haycock); ++B)
		{
			const Base Is = static_cast<Base>(B);
			const std::vector<Placed>& L = Inst[B];
			// (not what the grass grows through or up against: a gate, a fence, a pole; nor a house, whose floor the
			// raster keeps clear; nor a region's low kit, a balk or a bund)
			if (L.empty() || Is == Base::Gate || Is == Base::Wattle || Is == Base::Pole || Is == Base::Sweep || Is == Base::Khata || Is == Base::Chimney || KitLowAt(Is)) continue;
			if (Bases[B].Class == BaseMesh::Draw::House) continue;
			// (nor a wood's undergrowth, floor growth itself: a few stems and leaves over a wide round, the ferns among them)
			if (Bases[B].Name == "Undergrowth") continue;
			const std::vector<LowFoot> Low = LowFeet(Bases[B]);
			for (const LowFoot& Lf : Low)
				for (const Placed& P : L)
				{
					// (a thin trunk is a trunk still: only a twig's width is left out)
					const double R = std::min(Lf.R * P.Scale, 3.5);
					if (R < 0.05) continue;
					const double Reach = R + (bFeetWide ? FootWide : FootEdge), Cs = std::cos(static_cast<double>(P.Yaw)), Sn = std::sin(static_cast<double>(P.Yaw));
					const float FX = static_cast<float>(P.X + (Lf.X * Cs - Lf.Y * Sn) * P.Scale), FY = static_cast<float>(P.Y + (Lf.X * Sn + Lf.Y * Cs) * P.Scale);
					const int I0 = static_cast<int>(std::floor((FX - Reach) / ClutterSquare)), I1 = static_cast<int>(std::floor((FX + Reach) / ClutterSquare));
					const int J0 = static_cast<int>(std::floor((FY - Reach) / ClutterSquare)), J1 = static_cast<int>(std::floor((FY + Reach) / ClutterSquare));
					for (int J = J0; J <= J1; ++J)
						for (int I = I0; I <= I1; ++I) All.push_back({ SquareKeyIJ(I, J), GrassFoot{ FX, FY, P.Z, static_cast<float>(R) } });
				}
		}
		std::stable_sort(All.begin(), All.end(), [](const Reached& A, const Reached& B) { return A.Key < B.Key; });
		Feet.reserve(All.size());
		for (size_t K = 0; K < All.size();)
		{
			size_t E = K;
			while (E < All.size() && All[E].Key == All[K].Key) ++E;
			FootSquares[All[K].Key] = { static_cast<uint32_t>(K), static_cast<uint32_t>(E) };
			for (size_t Q = K; Q < E; ++Q) Feet.push_back(All[Q].F);
			K = E;
		}
	}

	void Ground::Impl::AddBurn(double X, double Y, double Core, double Outer)
	{
		if (!std::isfinite(X) || !std::isfinite(Y) || !(Outer > 0.0)) return;
		Outer = std::min(Outer, 60.0);
		Core = std::clamp(Core, 0.0, Outer);
		if (BaseMean.empty())
		{
			BaseMean.assign(Bases.size(), Col{ 128.0, 128.0, 128.0 });
			for (size_t B = 0; B < Bases.size(); ++B)
			{
				const std::vector<Rgb8>& Cs = Bases[B].Near.Colours;
				if (Cs.empty()) continue;
				Col Sum;
				for (const Rgb8& C : Cs)
				{
					Sum.R += C.R;
					Sum.G += C.G;
					Sum.B += C.B;
				}
				const double N = static_cast<double>(Cs.size());
				BaseMean[B] = { Sum.R / N, Sum.G / N, Sum.B / N };
			}
		}
		const int I0 = static_cast<int>(std::floor((X - Outer) / ClutterSquare)), I1 = static_cast<int>(std::floor((X + Outer) / ClutterSquare));
		const int J0 = static_cast<int>(std::floor((Y - Outer) / ClutterSquare)), J1 = static_cast<int>(std::floor((Y + Outer) / ClutterSquare));
		for (int J = J0; J <= J1; ++J)
			for (int I = I0; I <= I1; ++I)
				BurnSquares[SquareKeyIJ(I, J)].push_back({ static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Core), static_cast<float>(Outer) });
	}
}

namespace LandField
{
	// ====================================================================== the plots

	namespace
	{
		/** The plots' blocks: a lattice this far apart (along and across the frame), each corner jittered. */
		constexpr double BlockU = 560.0, BlockV = 500.0, BlockJitter = 0.22;
		/** The survey's blocks (Region::Surveyed): half a mile, square to the compass. */
		constexpr double SurveyBlock = 804.67;
		/**
		 * A plot is cut again while it is larger than a size drawn between these (square metres): fields of some 2 to 5
		 * hectares, so that at a man's eye each closes in at 100 to 200 m across (the art director's sight lines).
		 */
		constexpr double PlotMin = 32000.0, PlotMax = 60000.0;

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
		// (North America's township survey: the fields square to the compass on the half-mile grid; Region::Surveyed)
		bSurvey = Rg && Rg->Surveyed();
		if (bSurvey) Theta0 = 0.0;
	}

	/**
	 * A place in the plots' frame: bent by a long slow warp of some 40 m over 750 m and a little one of 8 m over 170 m
	 * (so the hedges run in gentle curves, not ruled lines), then turned to the country's grain.
	 */
	V2 Ground::Impl::PlotFrame(double X, double Y) const
	{
		if (bSurvey) return { X, Y };
		const double WX = 40.0 * Noise(X / 760.0, Y / 760.0, Seed ^ 0x9A01) + 8.0 * Noise(X / 170.0, Y / 170.0, Seed ^ 0x9A02);
		const double WY = 40.0 * Noise(X / 760.0, Y / 760.0, Seed ^ 0x9A03) + 8.0 * Noise(X / 170.0, Y / 170.0, Seed ^ 0x9A04);
		const double QX = X + WX, QY = Y + WY, C = std::cos(Theta0), S = std::sin(Theta0);
		return { QX * C + QY * S, -QX * S + QY * C };
	}

	/**
	 * The plot at a place, as Norman fields were made: the country in blocks of about half a kilometre (a lattice, its
	 * corners jittered), each block cut across its length into two, and each half again, at a third to two thirds
	 * along and a little askew, until the plot is a field of about 2 to 5 hectares. So the fields are four-sided and
	 * irregular, longer one way than the other, their hedges meeting mostly in T's, and bent a little with the warp.
	 * Its number (under a million), how far the place lies inside its edges (OutGap, metres) and the kind of land the
	 * rules give round its middle (OutKind: open or field; beyond the field's reach about one plot in three is corn).
	 */
	int Ground::Impl::Plot(double X, double Y, double* OutGap, uint8_t* OutKind) const
	{
		if (Sp)
		{
			// The steppe: the collective farm's great fields as the recording laid them, half a kilometre and more across,
			// one crop to a field. The number carries the crop (its last three bits), so CropOf reads it back.
			int Crop = LandSteppe::CropPasture;
			const int Id = Sp->FieldAt(X, Y, Crop, OutGap);
			if (OutKind) *OutKind = LandSteppe::IsCorn(Crop) ? static_cast<uint8_t>(RgField) : static_cast<uint8_t>(RgOpen);
			return (Id % 120000) * 8 + Crop;
		}
		const V2 P = PlotFrame(X, Y);
		// (the survey's: half-mile blocks, square, their corners where the survey put them)
		const double BU = bSurvey ? SurveyBlock : BlockU, BV = bSurvey ? SurveyBlock : BlockV, Jit = bSurvey ? 0.0 : BlockJitter;
		auto Corner = [this, BU, BV, Jit](int64_t I, int64_t J) {
			const uint64_t H = Hash(static_cast<uint64_t>(I), static_cast<uint64_t>(J), Seed ^ 0x9A05);
			return V2{ (static_cast<double>(I) + 2.0 * Jit * (Unit(H) - 0.5)) * BU, (static_cast<double>(J) + 2.0 * Jit * (Unit(Mix(H)) - 0.5)) * BV };
		};
		const int64_t I0 = static_cast<int64_t>(std::floor(P.X / BU)), J0 = static_cast<int64_t>(std::floor(P.Y / BV));
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
			if (QuadArea(Q) < Lerp(Rg ? Rg->PlotMin() : PlotMin, Rg ? Rg->PlotMax() : PlotMax, Unit(Hash(Key, 1)))) break;
			// Across its length: between the longer pair of opposite sides, now and then the other way where it is squat.
			const double L02 = Len(Q[1] - Q[0]) + Len(Q[2] - Q[3]), L13 = Len(Q[2] - Q[1]) + Len(Q[3] - Q[0]);
			bool bTurn = L13 > L02;
			if (std::max(L02, L13) < 1.3 * std::min(L02, L13) && Unit(Hash(Key, 2)) < 0.3) bTurn = !bTurn;
			const V2 R[4] = { Q[bTurn ? 1 : 0], Q[bTurn ? 2 : 1], Q[bTurn ? 3 : 2], Q[bTurn ? 0 : 3] };
			// (the survey cuts a block square at its half: the eighty, the forty, the twenty)
			const double A = bSurvey ? 0.5 : 0.34 + 0.32 * Unit(Hash(Key, 3));
			const double B = bSurvey ? 0.5 : std::clamp(A + 0.36 * (Unit(Hash(Key, 4)) - 0.5), 0.22, 0.78);
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
			else Kind = Unit(Mix(Key ^ 0x4B1D)) < (Rg ? Rg->FarTilled() : 0.36) ? RgField : RgOpen;
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
		// (a surveyed field one crop whatever the rules' ground under it: its open, rough and field parts drawn alike)
		const int KindKey = bSurvey && (Kind == RgOpen || Kind == RgRough) ? static_cast<int>(RgField) : Kind;
		const double U = Unit(Hash(Seed ^ 0xC80B, static_cast<uint64_t>(SeedIndex), static_cast<uint64_t>(KindKey)));
		if (Rg && (Kind == RgField || Kind == RgOpen || Kind == RgRough)) return CpRegion + std::clamp(Rg->CropOf(Kind, U), 0, 63);
		if (Sp && SeedIndex >= 0 && (Kind == RgField || Kind == RgOpen))
		{
			// (the steppe's field: its crop in its number's last three bits; where the rules' cell and the field disagree
			// at an edge, the cell's kind wins and the crop follows it)
			const int Crop = SeedIndex % 8;
			if (Kind == RgField) return Crop == LandSteppe::CropRye ? CpRye : CpWheat;
			return Crop == LandSteppe::CropFallow ? CpFallow : CpSteppe;
		}
		switch (Kind)
		{
		// (mid-July 1944: the harvest barely begun, so only a little early barley is cut; a late-sown wheat still green; the
		// environment review of 22 November: stubble 3% of the arable at most, green wheat 15%)
		case RgField: return U < 0.42 ? CpWheat : U < 0.60 ? CpBarley : U < 0.72 ? CpOats : U < 0.87 ? CpGreenWheat : U < 0.90 ? CpStubble : CpRoots;
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

	bool Ground::Impl::FitsRow(const House& H) const
	{
		const double C = std::cos(H.Yaw), S = std::sin(H.Yaw);
		const double HU = 0.5 * H.Long + 0.3, HV = 0.5 * H.Deep + 0.3, R = std::hypot(HU, HV);
		const int I0 = static_cast<int>((H.C.X - R - RX0) / Res) - 1, I1 = static_cast<int>((H.C.X + R - RX0) / Res) + 1;
		const int J0 = static_cast<int>((H.C.Y - R - RY0) / Res) - 1, J1 = static_cast<int>((H.C.Y + R - RY0) / Res) + 1;
		if (I0 < 2 || J0 < 2 || I1 >= NX - 2 || J1 >= NY - 2) return false;
		for (int J = J0; J <= J1; ++J)
			for (int I = I0; I <= I1; ++I)
			{
				const double DX = RX0 + (I + 0.5) * Res - H.C.X, DY = RY0 + (J + 0.5) * Res - H.C.Y;
				const double U = std::fabs(DX * C + DY * S), V = std::fabs(-DX * S + DY * C);
				if (U > HU || V > HV) continue;
				const size_t K = static_cast<size_t>(J) * NX + I;
				if (OccR[K] & (OcVehicle | OcMan | OcWall)) return false;
				if ((OccR[K] & OcHouse) && U <= 0.5 * H.Long - 1.6 && V <= 0.5 * H.Deep - 1.6) return false;
				const uint8_t G = Rgn[K];
				if (G == RgWater || G == RgMarsh || G == RgWoods) return false;
				if (LaneD[K] < 0.8f) return false;
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

	/**
	 * The region's kit houses on their plots, once their models are read: each model's footprint (its points within 0.3 m
	 * of its foot) centred on the plot the field kept for it, where the model's middle is off its origin (the Med and
	 * desert houses by up to 3 m, over ground nobody had looked at); and under every house of a kit house slot, the plot's
	 * and a planted one's alike, a footing from the lowest ground beneath its footprint up to its floor, in its walls'
	 * own colour there, so a house on a slope stands on the ground all round (Brian, 24 November: the Algiers battle's
	 * houses stood off the ground, men walking under them).
	 */
	void Ground::Impl::SeatKitHouses()
	{
		const int Kit0 = static_cast<int>(Base::Kit0), Kit15 = static_cast<int>(Base::Kit15);
		struct Print
		{
			double X0 = 1e9, X1 = -1e9, Y0 = 1e9, Y1 = -1e9, R = 0.0, G = 0.0, B = 0.0;
			int N = 0;
			bool bHouse = false;
		};
		Print Prints[16];
		for (const House& H : Houses)
			if (H.bKit && static_cast<int>(H.KitB) >= Kit0 && static_cast<int>(H.KitB) <= Kit15) Prints[static_cast<int>(H.KitB) - Kit0].bHouse = true;
		for (int K = 0; K < 16; ++K)
		{
			Print& P = Prints[K];
			if (!P.bHouse) continue;
			const Mesh& M = Bases[static_cast<size_t>(Kit0 + K)].Near;
			for (size_t I = 0; I < M.Points(); ++I)
			{
				if (M.Positions[I * 3 + 2] > 0.3f) continue;
				const double X = M.Positions[I * 3], Y = M.Positions[I * 3 + 1];
				P.X0 = std::min(P.X0, X);
				P.X1 = std::max(P.X1, X);
				P.Y0 = std::min(P.Y0, Y);
				P.Y1 = std::max(P.Y1, Y);
				P.R += M.Colours[I].R;
				P.G += M.Colours[I].G;
				P.B += M.Colours[I].B;
				++P.N;
			}
		}
		for (int K = 0; K < 16; ++K)
		{
			const Print& P = Prints[K];
			if (!P.bHouse || P.N == 0) continue;
			const Base B = static_cast<Base>(Kit0 + K);
			const double MidX = 0.5 * (P.X0 + P.X1), MidY = 0.5 * (P.Y0 + P.Y1), HalfL = 0.5 * (P.X1 - P.X0), HalfD = 0.5 * (P.Y1 - P.Y0);
			const Col Paint = Scaled(Rgb(P.R / P.N, P.G / P.N, P.B / P.N), 0.86);
			for (Placed& Pl : Inst[static_cast<size_t>(B)])
			{
				const double C = std::cos(Pl.Yaw), S = std::sin(Pl.Yaw), Sc = Pl.Scale;
				// (a house the field laid on a floor: its model's middle on the plot's)
				for (const House& H : Houses)
					if (H.bKit && H.KitB == B && std::fabs(H.C.X - Pl.X) < 0.01 && std::fabs(H.C.Y - Pl.Y) < 0.01)
					{
						Pl.X = static_cast<float>(H.C.X - (MidX * C - MidY * S) * Sc);
						Pl.Y = static_cast<float>(H.C.Y - (MidX * S + MidY * C) * Sc);
						break;
					}
				const double CX = Pl.X + (MidX * C - MidY * S) * Sc, CY = Pl.Y + (MidX * S + MidY * C) * Sc;
				double Lo = 1e9;
				for (int A = -2; A <= 2; ++A)
					for (int D = -2; D <= 2; ++D)
					{
						const double U = 0.5 * A * HalfL * Sc, V = 0.5 * D * HalfD * Sc;
						const double X = CX + U * C - V * S, Y = CY + U * S + V * C;
						Lo = std::min(Lo, Carve(X, Y, Terrain(X, Y), nullptr));
					}
				if (Pl.Z - Lo < 0.05) continue;
				Footing F;
				F.X = CX;
				F.Y = CY;
				F.Yaw = Pl.Yaw;
				F.HalfL = std::max(0.3, HalfL * Sc - 0.12);
				F.HalfD = std::max(0.3, HalfD * Sc - 0.12);
				F.Foot = Lo - 0.4;
				F.Top = Pl.Z + 0.05;
				Footings.push_back(F);
				FootingPaints.push_back(Paint);
			}
		}
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
		if (Sp)
		{
			MakeSteppeSites();
			return;
		}
		if (Rg)
		{
			RegionHost H(*this, true);
			Rg->Sites(H);
			return;
		}
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
					if (!Fits(ChurchYard(C, Yaw), 0.5)) continue;
					LayChurch(C, Yaw, R.Next() < 0.35 ? DmRoofHole : DmNone);
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


	/**
	 * The steppe's farm (the state farm on the dry run, its whitewashed cottages strung along the lanes): each household a
	 * plot along its lane, 18 to 30 m of frontage, its cottage (the east kit's Khata, long side or gable to the lane) set
	 * back behind a wattle fence, its haystack behind, a well sweep at one plot in four; some two in five of them burnt to
	 * their stove and chimney (the kit's ruin share for a village fought over in 1943, the art director's ruling of 19
	 * November). Then the farm's own long sheds and its granary at the edge, whitewashed, under plank or straw. No church,
	 * no stone wall, no farmstead out in the fields: the collective farm worked the steppe from the village.
	 */
	/** The churchyard's ground: the church's footprint with room round it for its wall. */
	House Ground::Impl::ChurchYard(V2 C, double Yaw) const
	{
		const V2 U{ std::cos(Yaw), std::sin(Yaw) };
		House Yard;
		Yard.C = C + U * 0.7;
		Yard.Yaw = Yaw;
		Yard.Long = 31.0 + 12.0;
		Yard.Deep = 8.6 + 12.0;
		return Yard;
	}

	/** The village church (nave, chancel, the tower at its west end) in its walled churchyard, its middle at C, its nave
	 * along Yaw; the frame's village lays it, and a region's (Host::Church). Its yard must fit (ChurchYard). */
	void Ground::Impl::LayChurch(V2 C, double Yaw, int Hurt)
	{
		const V2 U{ std::cos(Yaw), std::sin(Yaw) }, V = Perp(U);
		const House Yard = ChurchYard(C, Yaw);
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
		Nave.Hurt = Hurt;
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
	}

	void Ground::Impl::MakeSteppeSites()
	{
		if (!Fd.bTown) return;
		Rng R(Seed ^ 0x5174);
		constexpr double KhataLong = 9.18, KhataDeep = 6.46, Panel = 4.11;
		int Burnt = 0;
		// Near-only marks round a ruin: no tree, shrub or shed on its ash, but men walk through it.
		auto MarkNear = [this](V2 C, double Reach) {
			const int I0 = std::max(0, static_cast<int>((C.X - Reach - RX0) / Res)), I1 = std::min(NX - 1, static_cast<int>((C.X + Reach - RX0) / Res));
			const int J0 = std::max(0, static_cast<int>((C.Y - Reach - RY0) / Res)), J1 = std::min(NY - 1, static_cast<int>((C.Y + Reach - RY0) / Res));
			for (int J = J0; J <= J1; ++J)
				for (int I = I0; I <= I1; ++I)
					if (std::hypot(RX0 + (I + 0.5) * Res - C.X, RY0 + (J + 0.5) * Res - C.Y) < Reach) OccR[static_cast<size_t>(J) * NX + I] |= OcNear;
		};
		auto Free = [this](V2 C) { return InRaster(C.X, C.Y) && !(OccR[Pix(C.X, C.Y)] & (OcHouse | OcVehicle)) && LaneD[Pix(C.X, C.Y)] > 0.5f; };
		// A run of wattle along a line, in the kit's panels (4.11 m) stretched a little to fit; a gap left at Gap (metres
		// along it, negative for none), now and then a panel down.
		auto Fence = [&](V2 A, V2 B, double Gap) {
			const double L = Len(B - A);
			if (L < 2.0) return;
			const int N = std::max(1, static_cast<int>(std::lround(L / Panel)));
			const V2 T = Unit2(B - A);
			const double Each = L / N, Yaw = std::atan2(T.Y, T.X);
			for (int K = 0; K < N; ++K)
			{
				const double Mid = (K + 0.5) * Each;
				if (Gap >= 0.0 && std::fabs(Mid - Gap) < 0.5 * Each) continue;
				if (R.Next() < 0.1) continue; // knocked down
				Kits.push_back({ Base::Wattle, A + T * Mid, Yaw + R.Range(-0.03, 0.03), Each / Panel, R.Range(0.85, 1.08) });
			}
		};
		int Placed = 0;
		for (size_t L = 0; L < Lanes.size(); ++L)
		{
			const Line& P = Lanes[L].P;
			const double Half = Lanes[L].Half;
			for (int Side = -1; Side <= 1; Side += 2)
			{
				size_t K = 0;
				while (K + 2 < P.size() && Placed < 70)
				{
					const V2 At = P[K];
					if (std::hypot(At.X - Fd.TownX, At.Y - Fd.TownY) > Fd.TownR * 1.1)
					{
						++K;
						continue;
					}
					const double Front = R.Range(18.0, 30.0);
					size_t KE = K;
					while (KE + 1 < P.size() && Len(P[KE] - At) < Front) ++KE;
					const V2 T = Unit2(P[KE] - At), N = Perp(T) * static_cast<double>(Side);
					const double Line0 = Half + LaneWall + 0.6; // the fence, just off the road's edge
					const bool bGable = R.Next() < 0.3;
					const double Along = bGable ? KhataDeep : KhataLong, Across = bGable ? KhataLong : KhataDeep;
					const double Back = R.Range(4.0, 9.0), U = R.Range(0.5 * Along + 2.0, std::max(0.5 * Along + 2.5, Front - 0.5 * Along - 2.0));
					House H;
					H.Type = HtHouse;
					H.bKit = true;
					H.Long = KhataLong;
					H.Deep = KhataDeep;
					H.C = At + T * U + N * (Line0 + Back + 0.5 * Across);
					const double Tilt = R.Range(-0.05, 0.05);
					const double Flip = R.Next() < 0.5 ? Pi : 0.0;
					H.Yaw = std::atan2(T.Y, T.X) + (bGable ? 0.5 * Pi : 0.0) + Tilt + Flip;
					H.Key = Mix(R.State);
					const bool bRuin = R.Next() < 0.4;
					if (!Fits(H, 1.5))
					{
						K += 2;
						continue;
					}
					if (bRuin)
					{
						Kits.push_back({ Base::Chimney, H.C, H.Yaw, 1.0, R.Range(0.9, 1.05) });
						MarkNear(H.C, 6.5);
						++Burnt;
					}
					else
					{
						FloorOf(H);
						Stamp(H, 1.2);
						Houses.push_back(H);
					}
					++Placed;
					// Its fence along the lane, the gate before the door; a fence down one side now and then.
					const V2 F0 = At + N * Line0, F1 = At + T * Front + N * Line0;
					if (R.Next() < 0.85) Fence(F0, F1, U + R.Range(-2.0, 2.0));
					if (R.Next() < 0.5) Fence(F0 + T * 0.3, F0 + T * 0.3 + N * (Back + Across + R.Range(6.0, 14.0)), -1.0);
					// The well sweep by the fence, the haystack behind the cottage.
					if (R.Next() < 0.25)
					{
						const double SweepAlong = R.Range(2.0, std::max(2.5, Front - 2.0));
						const double Off = R.Range(1.6, 3.2);
						const V2 W = At + T * SweepAlong + N * (Line0 + Off);
						if (Free(W) && Len(W - H.C) > 0.5 * KhataLong + 2.5) Kits.push_back({ Base::Sweep, W, R.Range(0.0, 2.0 * Pi), 1.0, R.Range(0.92, 1.08) });
					}
					if (R.Next() < 0.55)
					{
						const double Behind = R.Range(4.0, 9.0);
						const double Aside = R.Range(-4.0, 4.0);
						const V2 S = H.C + N * (0.5 * Across + Behind) + T * Aside;
						if (Free(S))
						{
							const double Sz = R.Range(0.8, 1.15);
							Kits.push_back({ Base::Haystack, S, R.Range(0.0, 2.0 * Pi), Sz, Sz * R.Range(0.85, 1.1) });
							MarkNear(S, 2.5);
						}
					}
					K = KE;
				}
			}
		}
		// The farm's long sheds (cattle, the tractors) and its granary at the village's edge, set to the fields' grain.
		int Sheds = 0;
		const double Grain = Sp ? Sp->Lay.Turn : 0.0;
		const bool bShedTrial = !Fd.Trial.empty() && Fd.Tried("sheds");
		for (int Ring = 0; Ring < 8 && Sheds < 4; ++Ring)
			for (int K = 0; K < 24 && Sheds < 4; ++K)
			{
				const double A = K * 2.0 * Pi / 24.0 + Ring * 0.41, D = Fd.TownR * (0.7 + 0.12 * Ring);
				House H;
				H.Type = Sheds == 1 ? HtBarn : HtByre;
				H.bWhite = true;
				H.bBoards = bShedTrial;
				H.bDaub = bShedTrial && Fd.Lat < 54.5;
				H.Long = H.Type == HtBarn ? R.Range(22.0, 28.0) : R.Range(36.0, 50.0);
				H.Deep = H.Type == HtBarn ? R.Range(10.0, 12.0) : R.Range(9.0, 11.0);
				H.C = V2{ Fd.TownX + D * std::cos(A), Fd.TownY + D * std::sin(A) };
				const double Square = R.Next() < 0.5 ? 0.0 : 0.5 * Pi;
				const double Tilt = R.Range(-0.04, 0.04);
				H.Yaw = Grain + Square + Tilt;
				H.Roof = R.Next() < 0.5 ? 0 : 1;
				H.Front = R.Next() < 0.5 ? 1 : -1;
				H.Key = Mix(R.State ^ 0x54ED);
				const double U = R.Next();
				H.Hurt = U < 0.15 ? DmShell : U < 0.35 ? DmGable : U < 0.6 ? DmRoofHole : DmNone;
				if (!Fits(H, 3.0)) continue;
				FloorOf(H);
				if (bShedTrial)
				{
					// (the ground along each wall, in BuildHouse's order of faces and their ways)
					const double C = std::cos(H.Yaw), S = std::sin(H.Yaw);
					for (int F = 0; F < 4; ++F)
						for (int J = 0; J < 18; ++J)
						{
							const double T = J / 17.0;
							const double Uu = F == 0 ? H.Long * (0.5 - T) : F == 1 ? H.Long * (T - 0.5) : F == 2 ? 0.5 * H.Long : -0.5 * H.Long;
							const double Vv = F == 0 ? 0.5 * H.Deep : F == 1 ? -0.5 * H.Deep : F == 2 ? H.Deep * (T - 0.5) : H.Deep * (0.5 - T);
							const double X = H.C.X + Uu * C - Vv * S, Y = H.C.Y + Uu * S + Vv * C;
							H.Lie[F][J] = static_cast<float>(Carve(X, Y, Terrain(X, Y), nullptr));
						}
				}
				Stamp(H, 2.0);
				Houses.push_back(H);
				++Sheds;
			}
		Count.Farmsteads = 0;
		for (const House& H : Houses)
		{
			++Count.Houses;
			if (H.Hurt != DmNone) ++Count.Damaged;
		}
		Count.Houses += Burnt;
		Count.Damaged += Burnt;
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
		// it is, unless the two are near even (then the rules' edge parts it, as a hedge would).
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
						if (std::min(Sh[0], Sh[1]) < 0.47 * (Sh[0] + Sh[1])) Kind = Most;
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
		// Slivers and scraps under 0.8 ha (where a kind's edge, a wood or a lane cuts a corner off a plot) join the
		// neighbour they share most edge with, so no field is a pocket a man sees across in a few strides.
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
			const bool bSmall = (IsFarmland(Kind) && A < 8000.0) || (Kind == RgVillage && A < 300.0);
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
			if (Q.Kind == RgOpen && !Sp && !Rg && Fd.bTown && std::hypot(Q.CX - Fd.TownX, Q.CY - Fd.TownY) < Fd.TownR + 300.0 && Unit(Mix(Q.Key ^ 0x0C)) < 0.12) Q.bOrchard = true;
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
		if (Rg && !Rg->Hedges())
		{
			// A region's ground has no hedgerows: its own walls and terraces, if any, along its parcels' edges.
			BGrid.Init(-Band - 20.0, -Band - 20.0, FW + Band + 20.0, FH + Band + 20.0, 8.0);
			Inst.assign(static_cast<size_t>(Base::Count), {});
			RegionHost H(*this, false);
			Rg->Walls(H);
			return;
		}
		if (Sp)
		{
			// The steppe has no hedges: the fields meet in a grassy strip, the gardens behind their wattle (MakeSteppeSites).
			BGrid.Init(-Band - 20.0, -Band - 20.0, FW + Band + 20.0, FH + Band + 20.0, 8.0);
			Inst.assign(static_cast<size_t>(Base::Count), {});
			return;
		}
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
					{
						// (each crossing kept, with how far along its way it was: the hedge cutters' test switch picks from them)
						const V2 At = Lerp2(H.P[K], H.P[K + 1], T), Seg = WayB[W] - WayA[W];
						const double L2 = Dot(Seg, Seg), Tw = L2 > 1e-12 ? Clamp01(Dot(At - WayA[W], Seg) / L2) : 0.0;
						const V2 Head = Unit2(H.P[K + 1] - H.P[K]);
						HedgeCrossing Cr;
						Cr.X = At.X;
						Cr.Y = At.Y;
						Cr.Yaw = std::atan2(Head.Y, Head.X);
						Cr.Width = 2.0 * Half;
						Cr.Kind = H.Kind;
						Cr.Way = WayOf[W];
						Cr.Track = Fd.Ways[static_cast<size_t>(WayOf[W])].Track;
						Cr.Along = static_cast<double>(WayK[W]) + Tw;
						CrossOut.push_back(Cr);
					}
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
				if (std::fabs(H.S[K] - At) < GateHalf + 0.3 && (!H.Keep[K] || NearCut(H.P[K].X, H.P[K].Y, GateHalf + 1.0))) bClear = false;
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
			{
				const V2 Middle = H.P[Hinge] + Way * (At - H.S[Hinge]);
				OpeningOut.push_back({ Middle.X, Middle.Y, Yaw, 2.0 * GateHalf, 0, 0 });
			}
			const V2 Inward = V2{ Q.CX, Q.CY } - H.P[Hinge];
			const double Into = Dot(Inward, Perp(Way)) > 0.0 ? 1.0 : -1.0;
			const bool bSwung = R.Next() < 0.55;
			const double Swing = R.Range(1.1, 1.75);
			// Where a recorded way goes through the gateway the gate stands open, and where its leaf or post would stand on
			// one, the gateway is left without its gate.
			const V2 Post = H.P[Hinge] + Way * 0.15;
			auto Free = [this, Post](double Dir, double Room) {
				const V2 Leaf{ std::cos(Dir), std::sin(Dir) };
				for (double T = 0.0; T <= 3.6; T += 0.4)
					if (WayRoom(Post + Leaf * T) < Room) return false;
				return true;
			};
			if (bSwung || !Free(Yaw, 0.3)) Yaw += Into * Swing;
			if (!Free(Yaw, 0.2)) Yaw -= 2.0 * Into * Swing;
			if (Free(Yaw, 0.2)) Gates.push_back({ Post, Yaw });
			// The gateway trodden to earth either side.
			const V2 Mid = H.P[Hinge] + Way * 2.1;
			for (double DY = -6.0; DY <= 6.0; DY += Res)
				for (double DX = -6.0; DX <= 6.0; DX += Res)
					if (DX * DX + DY * DY < 30.0 && InRaster(Mid.X + DX, Mid.Y + DY)) OccR[Pix(Mid.X + DX, Mid.Y + DY)] |= OcGate;
		}
		Count.Gates = static_cast<int>(Gates.size());
		// Where the recording's men went over a hedgerow or a lane's bank in numbers (three crossings or more within 12.5 m
		// of one of them), a breach blown through it, bank and all, as the engineers' charges did: 2.5 to 4 m across (the
		// more men, the wider), at most one in 40 m of hedge and none within 15 m of a gateway or another gap, its earth
		// thrown out either side. The rest of the men climb the bank and push through the shrubs (MakePlants nudges two
		// apart for each), so the hedge stays a wall where the fight is.
		auto PointOn = [](const Hedge& H, double S, V2& Way) {
			size_t K0 = 0;
			while (K0 + 2 < H.P.size() && H.S[K0 + 1] <= S) ++K0;
			Way = Unit2(H.P[K0 + 1] - H.P[K0]);
			return H.P[K0] + Way * (S - H.S[K0]);
		};
		for (Hedge& H : Hs)
		{
			if (H.Kind != HkHedgerow && H.Kind != HkLaneBank) continue;
			std::vector<double> Over;
			for (size_t K = 0; K + 1 < H.P.size(); ++K)
			{
				if (!H.Keep[K] || !H.Keep[K + 1]) continue;
				const V2 Mid = Lerp2(H.P[K], H.P[K + 1], 0.5);
				const std::vector<uint32_t>* Near = WGrid.At(Mid.X, Mid.Y);
				if (!Near) continue;
				for (const uint32_t W : *Near)
				{
					double T;
					if (WayHalf[W] < VehicleHalf && Crosses(H.P[K], H.P[K + 1], WayA[W], WayB[W], T)) Over.push_back(Lerp(H.S[K], H.S[K + 1], T));
				}
			}
			if (Over.size() < 3) continue;
			std::sort(Over.begin(), Over.end());
			const double Last = H.S.back();
			auto Gapped = [&H](double S, double Room) {
				for (size_t K = 0; K < H.P.size(); ++K)
					if (!H.Keep[K] && std::fabs(H.S[K] - S) < Room) return true;
				return false;
			};
			// (40 m along this hedge, and where a hedge runs on in line past a junction as another, a breach in that one
			// counts too: within 3 m of this one's line, or this one within 3 m of its, heading within 20 degrees)
			std::vector<double> Blown;
			auto InLine = [&](double S) {
				for (const double B : Blown)
					if (std::fabs(B - S) < 40.0) return true;
				V2 Way;
				const V2 P = PointOn(H, S, Way);
				for (const HedgeOpening& O : OpeningOut)
				{
					if (O.Kind != 1) continue;
					const V2 D{ O.X - P.X, O.Y - P.Y }, Theirs{ std::cos(O.Yaw), std::sin(O.Yaw) };
					const double Turn = std::fabs(std::remainder(O.Yaw - std::atan2(Way.Y, Way.X), Pi));
					const bool bOnLine = std::fabs(D.X * Way.Y - D.Y * Way.X) < 3.0 || std::fabs(D.X * Theirs.Y - D.Y * Theirs.X) < 3.0;
					if (Turn < 0.35 && bOnLine && std::fabs(Dot(D, Way)) < 40.0) return true;
				}
				return false;
			};
			for (;;)
			{
				size_t Best = Over.size();
				int Most = 0;
				for (size_t I = 0; I < Over.size(); ++I)
				{
					const double S = Over[I];
					// (nor within 15 m of a hedge cutter's cut to come: a gap too)
					if (S < 4.0 || S > Last - 4.0 || Gapped(S, 15.0) || InLine(S)) continue;
					if (!Fd.Cuts.empty())
					{
						V2 Along;
						const V2 Here = PointOn(H, S, Along);
						if (NearCut(Here.X, Here.Y, 15.0)) continue;
					}
					int Men = 0;
					for (const double O : Over) Men += std::fabs(O - S) <= 12.5 ? 1 : 0;
					if (Men > Most)
					{
						Most = Men;
						Best = I;
					}
				}
				if (Best == Over.size() || Most < 3) break;
				const double At = Over[Best], Half = std::min(2.0, 1.25 + 0.05 * Most);
				Blown.push_back(At);
				for (size_t K = 0; K < H.P.size(); ++K)
					if (std::fabs(H.S[K] - At) < Half) H.Keep[K] = 0;
				V2 Way;
				const V2 Middle = PointOn(H, At, Way);
				OpeningOut.push_back({ Middle.X, Middle.Y, std::atan2(Way.Y, Way.X), 2.0 * Half, 1, Most });
				++Count.Breaches;
				// (its earth thrown out 3 to 5 m either side, torn and trodden)
				for (double DY = -5.5; DY <= 5.5; DY += Res)
					for (double DX = -5.5; DX <= 5.5; DX += Res)
					{
						const double Across = DX * -Way.Y + DY * Way.X, Along = DX * Way.X + DY * Way.Y;
						const double Ax = Across / 5.0, Al = Along / (Half + 1.5);
						if (Ax * Ax + Al * Al < 1.0 && InRaster(Middle.X + DX, Middle.Y + DY)) OccR[Pix(Middle.X + DX, Middle.Y + DY)] |= OcGate;
					}
			}
		}
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
			case Base::Birch: return 15.0;
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

		/** A tree of the steppe's oak woods and its balkas' groves: oak most, birch, ash, a few elm; no beech (it grows no
		 * farther east than Poland). */
		Base SteppeTree(Rng& R, double& High)
		{
			const double U = R.Next();
			const Base B = U < 0.45 ? Base::Oak : U < 0.72 ? Base::Birch : U < 0.9 ? Base::Ash : Base::Elm;
			switch (B)
			{
			case Base::Oak: High = R.Range(11.0, 17.0); break;
			case Base::Birch: High = R.Range(13.0, 18.0); break;
			case Base::Ash: High = R.Range(11.0, 16.0); break;
			default: High = R.Range(13.0, 18.0); break;
			}
			return B;
		}

		/**
		 * A bocage hedge's tree and its height over the bank's top (the bank lifts it some 1.2 m more): standards of oak,
		 * elm and ash 14 to 22 m, a beech now and then, and a third of all of them pollarded (a thick stub and a bushy head,
		 * 6.5 to 10 m); a row of pollards all pollards.
		 */
		Base BocageTree(Rng& R, bool bPollards, double& High)
		{
			const double U = R.Next();
			const Base B = bPollards || U < 0.28 ? Base::Pollard : U < 0.6 ? Base::Oak : U < 0.75 ? Base::Elm : U < 0.94 ? Base::Ash : Base::Beech;
			switch (B)
			{
			case Base::Oak: High = R.Range(15.0, 21.0); break;
			case Base::Elm: High = R.Range(16.0, 22.0); break;
			case Base::Ash: High = R.Range(14.0, 19.0); break;
			case Base::Beech: High = R.Range(15.0, 20.0); break;
			default: High = R.Range(6.5, 10.0); break;
			}
			return B;
		}
	}

	void Ground::Impl::Plant(Base B, double X, double Y, double Yaw, double Scale, double ScaleZ, Rgb8 Tint, bool bThrough)
	{
		// Nothing stands on a recorded way, a man's or a vehicle's: no stem, trunk or prop within its half width and the
		// thing's own body besides (the planting's coarse marks miss a stem by a pixel's corner now and then). The gates are
		// hung or left off where they are cut. A hedgerow's shrub men push through (bThrough) is kept off vehicles' ways only,
		// and so is a region's low kit (Region::KitLow: a balk's ridge a man steps over; TakeUpTallLowKit holds it to that).
		if (B != Base::Gate)
		{
			const double Body = B <= Base::Bramble ? 0.35 : B <= Base::Apple ? 0.4 : B == Base::Cart ? 1.6 : B == Base::Haycock ? 1.0 : 0.8;
			if (WayRoom({ X, Y }, bThrough || KitLowAt(B)) < Body) return;
		}
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
	 * The cattle's paths (the art director's review of 22 November, item 3: bare lines 30 to 60 cm wide along the hedges
	 * and into each gateway). In a pasture with beasts in it they keep to its hedges, 3 to 4 m off the bank and out of
	 * the brambles, wandering a little, on some three in five metres of its hedges; and from each gateway two or three
	 * paths fan out into the field, worn by the beasts going in and out. Each path is a line of points a metre apart:
	 * its segments are kept (the clutter keeps the grass off them) and pieces of trodden earth laid along it half a
	 * metre apart (the clutter lays them round the eye with the grass). Not on the steppe.
	 */
	void Ground::Impl::MakeCowPaths()
	{
		if (Sp || Rg || Parcels.empty()) return;
		auto Grazed = [this](V2 P) {
			if (!InRaster(P.X, P.Y)) return false;
			const size_t K = Pix(P.X, P.Y);
			if (Pid[K] < 0 || (OccR[K] & (OcHouse | OcNear | OcWall)) || Rgn[K] != RgOpen) return false;
			const ParcelInfo& Q = Parcels[static_cast<size_t>(Pid[K])];
			return Q.Crop == CpGrazed || Q.Crop == CpLush;
		};
		std::vector<std::vector<V2>> Paths;
		std::vector<double> Halves;
		Rng R(Seed ^ 0xC0A7);
		auto Keep = [&Paths, &Halves](std::vector<V2>& Run, double Half) {
			if (Run.size() >= 6)
			{
				Paths.push_back(Run);
				Halves.push_back(Half);
			}
			Run.clear();
		};
		// Along the hedges: each run's line a metre apart, either side, off by 3 to 4 m.
		for (size_t L = 0; L < HedgeOut.size(); ++L)
		{
			const HedgeLine& H = HedgeOut[L];
			if ((H.Kind != HkHedgerow && H.Kind != HkLaneBank && H.Kind != HkWoodEdge) || H.Points.size() < 2) continue;
			std::vector<V2> At, Normal;
			std::vector<double> Along;
			double S = 0.0;
			for (size_t K = 0; K + 1 < H.Points.size(); ++K)
			{
				const V2 A{ H.Points[K].first, H.Points[K].second }, B{ H.Points[K + 1].first, H.Points[K + 1].second };
				const double Lg = Len(B - A);
				if (Lg < 1e-6) continue;
				const V2 U = (B - A) * (1.0 / Lg);
				for (double T = At.empty() ? 0.0 : 1.0 - std::fmod(S, 1.0); T < Lg; T += 1.0)
				{
					At.push_back(A + U * T);
					Normal.push_back(Perp(U));
					Along.push_back(S + T);
				}
				S += Lg;
			}
			for (const double Side : { -1.0, 1.0 })
			{
				std::vector<V2> Run;
				const double Half = R.Range(0.2, 0.3), Off0 = R.Range(3.0, 3.8), Key = static_cast<double>(L) * 7.31 + Side * 3.7;
				for (size_t K = 0; K < At.size(); ++K)
				{
					// (smoothed: the normal's turn at a bend taken over the points round it, so the path rounds it)
					V2 N = Normal[K];
					if (K > 0) N = N + Normal[K - 1];
					if (K + 1 < Normal.size()) N = N + Normal[K + 1];
					N = Unit2(N);
					const double On = Noise(Along[K] / 35.0, Key, Seed ^ 0xC0A8);
					const double Off = Off0 + 0.7 * Noise(Along[K] / 9.0, Key + 11.0, Seed ^ 0xC0A9);
					const V2 P = At[K] + N * (Side * Off);
					if (On > -0.1 && Grazed(P) && DistE[Pix(P.X, P.Y)] > 2.2f && WayRoom(P, true) > 0.5) Run.push_back(P);
					else Keep(Run, Half);
				}
				Keep(Run, Half);
			}
		}
		// From each gateway, into each pasture it opens on: two or three paths fanning out, 12 to 28 m, bending a little.
		for (const HedgeOpening& O : OpeningOut)
		{
			if (O.Kind != 0) continue;
			const V2 Mid{ O.X, O.Y }, Way{ std::cos(O.Yaw), std::sin(O.Yaw) };
			for (const double Side : { -1.0, 1.0 })
			{
				const V2 Into = Perp(Way) * Side;
				if (!Grazed(Mid + Into * 4.0)) continue;
				const int Spokes = R.Next() < 0.4 ? 3 : 2;
				for (int K = 0; K < Spokes; ++K)
				{
					const double Fan = R.Range(0.5, 0.8);
					const double Wander = R.Range(-0.2, 0.2);
					const double Turn = (K - 0.5 * (Spokes - 1)) * Fan + Wander;
					const double Long = R.Range(12.0, 28.0), Bend = R.Range(-0.025, 0.025), Half = R.Range(0.24, 0.32);
					V2 Dir = Into * std::cos(Turn) + Way * std::sin(Turn);
					V2 P = Mid + Into * 1.0;
					std::vector<V2> Run;
					for (double T = 0.0; T <= Long; T += 1.0)
					{
						if (T >= 3.0 && !Grazed(P)) break;
						if (WayRoom(P, true) < 0.5) break;
						Run.push_back(P);
						const double C = std::cos(Bend), Sn = std::sin(Bend);
						Dir = V2{ Dir.X * C - Dir.Y * Sn, Dir.X * Sn + Dir.Y * C };
						P = P + Dir;
					}
					if (Run.size() >= 5)
					{
						Paths.push_back(Run);
						Halves.push_back(Half);
					}
				}
			}
		}
		// Their segments, for the grass; their pieces of earth, by the clutter's squares.
		CGrid.Init(-Band - 20.0, -Band - 20.0, FW + Band + 20.0, FH + Band + 20.0, 8.0);
		double Metres = 0.0;
		for (size_t K = 0; K < Paths.size(); ++K)
		{
			const std::vector<V2>& Pt = Paths[K];
			double Along = 0.0;
			for (size_t I = 0; I + 1 < Pt.size(); ++I)
			{
				const V2 D = Pt[I + 1] - Pt[I];
				const double Lg = Len(D);
				if (Lg < 1e-6) continue;
				CGrid.Add(static_cast<uint32_t>(CowA.size()), Pt[I], Pt[I + 1], Halves[K] + 0.3);
				CowA.push_back(Pt[I]);
				CowB.push_back(Pt[I + 1]);
				CowHalf.push_back(Halves[K]);
				Metres += Lg;
				const V2 U = D * (1.0 / Lg);
				for (double T = R.Range(0.0, 0.5); T < Lg; T += 0.5)
				{
					// (worn wider in places and narrower in others, here and there grassed over)
					const double Wear = Noise((Along + T) / 6.0, static_cast<double>(K) * 3.3, Seed ^ 0xC0AA);
					const double Sz = R.Range(0.9, 1.1), Shade = R.Range(0.92, 1.08), Warm = R.Range(0.96, 1.02), Side = R.Range(-0.04, 0.04), Turn = R.Range(-0.15, 0.15);
					if (Wear < -0.4) continue;
					const V2 Here = Pt[I] + U * T + Perp(U) * Side;
					Placed B;
					B.Kind = Base::Trodden;
					B.X = static_cast<float>(Here.X);
					B.Y = static_cast<float>(Here.Y);
					B.Z = static_cast<float>(Top(Here.X, Here.Y, nullptr));
					B.Yaw = static_cast<float>(std::atan2(U.Y, U.X) + Turn);
					B.Scale = static_cast<float>(Halves[K] / 0.26 * Sz * (0.75 + 0.35 * Smooth(-0.4, 0.5, Wear)));
					B.ScaleZ = 1.0f;
					B.Tint = Tint8(Shade, Shade, Shade * Warm);
					CowBits.push_back(B);
				}
				Along += Lg;
			}
		}
		Count.CowPathKm = Metres / 1000.0;
		std::stable_sort(CowBits.begin(), CowBits.end(), [](const Placed& A, const Placed& B) { return SquareKeyAt(A.X, A.Y) < SquareKeyAt(B.X, B.Y); });
		for (size_t K = 0; K < CowBits.size();)
		{
			const uint64_t Key = SquareKeyAt(CowBits[K].X, CowBits[K].Y);
			size_t E = K;
			while (E < CowBits.size() && SquareKeyAt(CowBits[E].X, CowBits[E].Y) == Key) ++E;
			CowSquares[Key] = { static_cast<uint32_t>(K), static_cast<uint32_t>(E) };
			K = E;
		}
	}

	double Ground::Impl::CowPath(double X, double Y) const
	{
		double Off = 1e9;
		if (const std::vector<uint32_t>* Near = CGrid.At(X, Y))
			for (const uint32_t K : *Near)
			{
				double T;
				Off = std::min(Off, SegDist({ X, Y }, CowA[K], CowB[K], T) - CowHalf[K]);
			}
		return Off;
	}

	/**
	 * What grows: along every bank its shrubs (hawthorn, blackthorn, hazel, holly, elder) in clumps a couple of metres
	 * apart, sometimes thin enough to see through, brambles at its feet, and every so often a tall tree (some hedges a
	 * row of pollards); the woods' trees, undergrowth thick at their edges; the orchards' apple trees in rows; a lone oak
	 * in some pastures; fruit trees in the gardens. Nothing stands on a house, a wall or a lane's floor, or on a recorded
	 * way (no trunk near a man's, nothing near a vehicle's).
	 */
	void Ground::Impl::MakePlants()
	{
		if (Rg)
		{
			// A region plants its own: its woods, trees, scrub and rocks, and the kit's things its sites set down; then its
			// kit houses on their floors.
			// (the trial "houses", release 2: the kit houses LandHouse builds to the vehicles' detail where the region asks
			// it (its KitHouse), four looks of each kind, each look a slot of its own after the region's kit; each house
			// turned end for end where need be so its door, or its gable where it stands gable on, faces the nearest lane)
			// (the desert's houses, LandHouse's Kind 10 and over, wait on a word of their own, "deserthouses", until they
			// too have passed: "houses" alone leaves the desert's kit as it was)
			if (Fd.Tried("houses") || Fd.Tried("deserthouses"))
			{
				auto Asked = [&](const LandHouse::Spec& Of) { return Fd.Tried(Of.Kind >= 10 ? "deserthouses" : "houses"); };
				HouseSlot0 = 0;
				for (int K = 0; K < KitSlots; ++K)
					if (Rg->KitName(K) || Rg->KitBuilt(K)) HouseSlot0 = K + 1;
				for (House& H : Houses)
				{
					if (!H.bKit) continue;
					LandHouse::Spec S;
					const int K = static_cast<int>(H.KitB) - static_cast<int>(Base::Kit0);
					if (K < 0 || K >= KitSlots || !Rg->KitHouse(K, S) || !Asked(S)) continue;
					S.Long = H.Long;
					S.Deep = H.Deep;
					S.Hurt = 0;
					const uint64_t Look = static_cast<uint64_t>(std::min(3.0, 4.0 * Unit(Hash(H.Key, 0x1D8A))));
					S.Key = Hash(0xEA57ull + static_cast<uint64_t>(S.Kind * 4 + S.Roof), Look);
					S.Below = std::max(1.0, H.Floor - H.Foot);
					int Slot = -1;
					for (size_t J = 0; J < HouseModels.size(); ++J)
					{
						const LandHouse::Spec& O = HouseModels[J];
						if (O.Kind == S.Kind && O.Long == S.Long && O.Deep == S.Deep && O.Roof == S.Roof && O.Hurt == S.Hurt && O.Key == S.Key) Slot = static_cast<int>(J);
					}
					if (Slot < 0 && HouseSlot0 + static_cast<int>(HouseModels.size()) < KitSlots)
					{
						Slot = static_cast<int>(HouseModels.size());
						HouseModels.push_back(S);
					}
					if (Slot < 0) continue;
					HouseModels[static_cast<size_t>(Slot)].Below = std::max(HouseModels[static_cast<size_t>(Slot)].Below, S.Below);
					H.Model = Slot;
					H.KitB = static_cast<Base>(static_cast<int>(Base::Kit0) + HouseSlot0 + Slot);
					double Best = 1e18;
					V2 Near = H.C;
					for (const Lane& Ln : Lanes)
						for (const V2& Q : Ln.P)
						{
							const double D2 = (Q.X - H.C.X) * (Q.X - H.C.X) + (Q.Y - H.C.Y) * (Q.Y - H.C.Y);
							if (D2 < Best)
							{
								Best = D2;
								Near = Q;
							}
						}
					const double Dx = (Near.X - H.C.X) * std::cos(H.Yaw) + (Near.Y - H.C.Y) * std::sin(H.Yaw);
					const double Dy = -(Near.X - H.C.X) * std::sin(H.Yaw) + (Near.Y - H.C.Y) * std::cos(H.Yaw);
					if (std::fabs(Dx) > std::fabs(Dy) ? Dx < 0.0 : Dy < 0.0) H.Yaw += Pi;
					++Count.HouseModels;
				}
				// (and a region's kit it set down as a thing but builds as a house of its own size: the east's burnt-out
				// houses, their stove and chimney standing over the ashes, LandHouse's Kind 9; four looks of each, chosen by
				// where each stands, each planted where its thing was at its full size, turned as it was; its ash bed's skirt
				// run down past the ground's fall under its footprint. Where it stood and what stands in sight there as before.)
				for (RegionKit& RK : RegionKits)
				{
					const int K = static_cast<int>(RK.K.B) - static_cast<int>(Base::Kit0);
					LandHouse::Spec S;
					if (K < 0 || K >= KitSlots || !Rg->KitHouse(K, S) || !Asked(S)) continue;
					S.Hurt = 0;
					const uint64_t Where = Hash(static_cast<uint64_t>(std::llround(RK.K.P.X * 8.0)) ^ 0x5EEDull, static_cast<uint64_t>(std::llround(RK.K.P.Y * 8.0)));
					const uint64_t Look = static_cast<uint64_t>(std::min(3.0, 4.0 * Unit(Hash(Where, 0x1D8B))));
					S.Key = Hash(0xEA57ull + static_cast<uint64_t>(S.Kind * 4 + S.Roof), Look);
					const double Cy = std::cos(RK.K.Yaw), Sy = std::sin(RK.K.Yaw), Mid = Top(RK.K.P.X, RK.K.P.Y, nullptr);
					double Low = Mid;
					for (int A = -1; A <= 1; A += 2)
						for (int Bq = -1; Bq <= 1; Bq += 2)
						{
							const double U = (0.5 * S.Long + 0.55) * A, V = (0.5 * S.Deep + 0.55) * Bq;
							Low = std::min(Low, Top(RK.K.P.X + U * Cy - V * Sy, RK.K.P.Y + U * Sy + V * Cy, nullptr));
						}
					S.Below = std::clamp(Mid - Low + 0.3, 0.6, 5.0);
					int Slot = -1;
					for (size_t J = 0; J < HouseModels.size(); ++J)
					{
						const LandHouse::Spec& O = HouseModels[J];
						if (O.Kind == S.Kind && O.Long == S.Long && O.Deep == S.Deep && O.Roof == S.Roof && O.Hurt == S.Hurt && O.Key == S.Key) Slot = static_cast<int>(J);
					}
					if (Slot < 0 && HouseSlot0 + static_cast<int>(HouseModels.size()) < KitSlots)
					{
						Slot = static_cast<int>(HouseModels.size());
						HouseModels.push_back(S);
					}
					if (Slot < 0) continue;
					const size_t Sl = static_cast<size_t>(Slot);
					HouseModels[Sl].Below = std::max(HouseModels[Sl].Below, S.Below);
					if (HouseFrom.size() < HouseModels.size())
					{
						HouseFrom.resize(HouseModels.size(), -1);
						HouseFromScale.resize(HouseModels.size(), 1.0);
						HouseFromScaleZ.resize(HouseModels.size(), 1.0);
					}
					HouseFrom[Sl] = K;
					HouseFromScale[Sl] = RK.K.Scale;
					HouseFromScaleZ[Sl] = RK.K.ScaleZ;
					RK.K.B = static_cast<Base>(static_cast<int>(Base::Kit0) + HouseSlot0 + Slot);
					RK.K.Scale = 1.0;
					RK.K.ScaleZ = 1.0;
					++Count.HouseModels;
				}
				Count.HouseKinds = static_cast<int>(HouseModels.size());
			}
			for (const RegionKit& RK : RegionKits) Plant(RK.K.B, RK.K.P.X, RK.K.P.Y, RK.K.Yaw, RK.K.Scale, RK.K.ScaleZ, RK.Tint);
			for (const House& H : Houses)
			{
				if (!H.bKit) continue;
				std::vector<Placed>& L = Inst[static_cast<size_t>(H.KitB)];
				const size_t Was = L.size();
				Plant(H.KitB, H.C.X, H.C.Y, H.Yaw, 1.0, 1.0, Tint8(1.0, 1.0, 1.0));
				if (L.size() > Was) L.back().Z = static_cast<float>(H.Floor - 0.1); // (its plinth on the highest ground under it)
				// (the trial "deserthouses": a desert house's yard is its own floor of beaten earth, laid just over the highest ground
				// under it, its walls run down past the ground's fall)
				if (L.size() > Was && H.Model >= 0 && H.Model < static_cast<int>(HouseModels.size()) && HouseModels[static_cast<size_t>(H.Model)].Kind == 10) L.back().Z = static_cast<float>(H.Floor + 0.03);
			}
			RegionHost Host(*this, false);
			Rg->Plants(Host);
			for (const std::vector<Placed>& L : Inst) Count.Instances += L.size();
			return;
		}
		// The bocage's farmhouses built to the vehicles' detail (release 2, the trial "houses", LandHouse): each kind of
		// farmhouse (its size, roof and damage, and one of two looks) a kit slot, planted on its floor, turned so its door
		// faces as LandField laid it; its plain built box left out of the tiles. Then the village church whole (LandHouse's
		// Kind 3: its nave, chancel and tower one model, planted at the nave; all three parts leave the tiles), then its
		// farm's barn and byre (Kind 1 and 2), then the village's houses (Kind 6), each its own (no two of the same size).
		// Where the slots run out, as before.
		if (!Sp && Fd.Tried("houses"))
			for (int Pass = 0; Pass < 5; ++Pass)
			for (size_t I = 0; I < Houses.size(); ++I)
			{
				House& H = Houses[I];
				const int Want[5] = { HtFarmhouse, HtChurch, HtBarn, HtByre, HtHouse }, KindOf[5] = { 0, 3, 1, 2, 6 };
				if (H.bKit || H.bWhite || H.Type != Want[Pass]) continue;
				// (a church's chancel and tower stand on as LayChurch put them, after its nave)
				const bool bChurch = H.Type == HtChurch;
				if (bChurch && (H.Part != 0 || I + 2 >= Houses.size() || Houses[I + 1].Type != HtChurch || Houses[I + 1].Part != 1 || Houses[I + 2].Type != HtChurch || Houses[I + 2].Part != 2)) continue;
				LandHouse::Spec S;
				S.Kind = KindOf[Pass];
				S.Long = H.Long;
				S.Deep = H.Deep;
				S.Storeys = H.Storeys;
				S.Roof = H.Roof;
				S.Hurt = bChurch ? std::min(H.Hurt, static_cast<int>(DmRoofHole)) : H.Hurt;
				// (a farmhouse's two looks by its roof and damage, as before; a barn's, a byre's and a church's their own)
				S.Key = Pass == 0 ? Hash(0xFA4A0ull, static_cast<uint64_t>(H.Roof * 8 + H.Hurt), H.Key & 1)
				        : Pass == 4 ? Hash(0xFA4A6ull, H.Key, static_cast<uint64_t>(S.Roof * 8 + S.Hurt)) // (a village house's look its own)
				                    : Hash(0xFA4A0ull + static_cast<uint64_t>(S.Kind), static_cast<uint64_t>(S.Roof * 8 + S.Hurt), H.Key & 1);
				S.Below = std::max(1.0, H.Floor - H.Foot); // (its plinth down past the lowest ground under it, as BuildHouse's walls)
				if (bChurch)
					for (size_t J = I + 1; J <= I + 2; ++J) S.Below = std::max(S.Below, Houses[I].Floor - Houses[J].Foot);
				int Slot = -1;
				for (size_t K = 0; K < HouseModels.size(); ++K)
				{
					const LandHouse::Spec& O = HouseModels[K];
					if (O.Kind == S.Kind && O.Long == S.Long && O.Deep == S.Deep && O.Storeys == S.Storeys && O.Roof == S.Roof && O.Hurt == S.Hurt && O.Key == S.Key) Slot = static_cast<int>(K);
				}
				if (Slot < 0 && static_cast<int>(HouseModels.size()) < KitSlots)
				{
					Slot = static_cast<int>(HouseModels.size());
					HouseModels.push_back(S);
				}
				if (Slot < 0) continue;
				// (houses of a kind share its model: its plinth runs as deep as the deepest of them needs)
				HouseModels[static_cast<size_t>(Slot)].Below = std::max(HouseModels[static_cast<size_t>(Slot)].Below, S.Below);
				H.Model = Slot;
				if (bChurch) Houses[I + 1].Model = Houses[I + 2].Model = Slot;
				Placed P;
				P.Kind = static_cast<Base>(static_cast<int>(Base::Kit0) + Slot);
				P.X = static_cast<float>(H.C.X);
				P.Y = static_cast<float>(H.C.Y);
				P.Z = static_cast<float>(H.Floor);
				P.Yaw = static_cast<float>(H.Yaw + (H.Front < 0 ? Pi : 0.0));
				const double Tone = 0.97 + 0.06 * Unit(Hash(H.Key, 0x7147));
				P.Tint = Tint8(Tone, Tone, Tone);
				Inst[static_cast<size_t>(P.Kind)].push_back(P);
				++Count.HouseModels;
			}
		Count.HouseKinds = static_cast<int>(HouseModels.size());
		auto Clear = [this](double X, double Y, uint8_t Bits) {
			if (!InRaster(X, Y)) return false;
			const size_t P = Pix(X, Y);
			return !(OccR[P] & Bits) && LaneD[P] > 0.4f;
		};
		// A recorded man's way and a wood edge's or a garden hedge's shrub: its stem keeps off his way, and where he
		// crosses the hedge its crown too, so the gap he goes through is open (where he walks along the hedge, only his own
		// width is kept clear of stems). He crosses it where his way runs across it, or where a stretch of his way passes
		// from one side of its line to the other however slantwise.
		auto ManClear = [this](V2 P, V2 Along) {
			const std::vector<uint32_t>* Near = WGrid.At(P.X, P.Y);
			if (!Near) return true;
			for (const uint32_t W : *Near)
			{
				if (WayHalf[W] >= VehicleHalf) continue;
				double T;
				const V2 Dir = Unit2(WayB[W] - WayA[W]);
				const double SideA = Along.X * (WayA[W].Y - P.Y) - Along.Y * (WayA[W].X - P.X), SideB = Along.X * (WayB[W].Y - P.Y) - Along.Y * (WayB[W].X - P.X);
				const bool bAcross = std::fabs(Dir.X * Along.Y - Dir.Y * Along.X) > 0.5 || SideA * SideB <= 0.0;
				if (SegDist(P, WayA[W], WayB[W], T) < WayHalf[W] + (bAcross ? 1.7 : 0.35)) return false;
			}
			return true;
		};
		// A hedgerow's or a lane bank's: the hedge stays a wall. Where a man goes over it outside its gateways and breaches
		// (MakeHedges) he climbs the bank and pushes through between two shrubs moved apart along it for him (no stem within
		// his half width and half a metre: a trampled notch about a metre wide, its shrubs a little lower), and where his
		// way runs along the bank, its shrubs and trees stand a little to one side of it. A trunk is moved as far as 4 m
		// and never stands on his way; a shrub that cannot be given room (where many cross close together) stays, and he
		// pushes through it.
		auto InHisWay = [this](V2 P, V2 Along, double Room) {
			const std::vector<uint32_t>* Near = WGrid.At(P.X, P.Y);
			if (!Near) return false;
			for (const uint32_t W : *Near)
			{
				if (WayHalf[W] >= VehicleHalf) continue;
				double T;
				const V2 Dir = Unit2(WayB[W] - WayA[W]);
				const double SideA = Along.X * (WayA[W].Y - P.Y) - Along.Y * (WayA[W].X - P.X), SideB = Along.X * (WayB[W].Y - P.Y) - Along.Y * (WayB[W].X - P.X);
				const bool bAcross = std::fabs(Dir.X * Along.Y - Dir.Y * Along.X) > 0.5 || SideA * SideB <= 0.0;
				if (bAcross && SegDist(P, WayA[W], WayB[W], T) < WayHalf[W] + Room) return true;
			}
			return false;
		};
		constexpr uint8_t Built = OcHouse | OcNear | OcWall | OcVehicle;
		// (the nearest place along the bank as far as Reach and across it as far as 0.9 m that is clear of what is built,
		// of a crossing man's way by Room and of every way by Body, and no nearer a sunken lane's wall; false where there
		// is none)
		auto Nudged = [&](V2& P, V2 Along, double Room, double Body, double Reach) {
			if (!InRaster(P.X, P.Y)) return false;
			const float Lane = std::min(LaneD[Pix(P.X, P.Y)], static_cast<float>(LaneWall + 0.45));
			auto Fine = [&](V2 Q) { return Clear(Q.X, Q.Y, Built) && LaneD[Pix(Q.X, Q.Y)] >= Lane && !InHisWay(Q, Along, Room) && WayRoom(Q) >= Body; };
			if (Fine(P)) return true;
			const V2 Across = Perp(Along);
			for (double Step = 0.3; Step <= Reach + 0.9 + 1e-9; Step += 0.3)
				for (double D = std::max(0.0, Step - 0.9); D <= std::min(Step, Reach) + 1e-9; D += 0.3)
					for (const double SD : { 1.0, -1.0 })
						for (const double SE : { 1.0, -1.0 })
						{
							const V2 Q = P + Along * (D * SD) + Across * ((Step - D) * SE);
							if (Fine(Q))
							{
								P = Q;
								return true;
							}
						}
			return false;
		};
		// The hedges. A hedgerow's bank and a lane's carry a dense hedge with no gap but the gateways and the breaches, as
		// Normandy's were in 1944, untrimmed: its shrubs a metre and a half apart along the bank's top, broad enough to
		// spill over its faces, its top ragged, 2.5 to 3.2 m over the bank on the whole and rising and falling a metre or
		// more in every 6 to 10 m along it, and every 5 to 9 m a clump of hazel or elder grown up to 4 or 5 m; brambles
		// hanging over both faces. In it a tree every 5 to 15 m, oak, elm and ash standing 15 to 22 m, a third of them
		// pollards (their bushy heads at 6 to 10 m), and between them young oak, ash and pollard shoots 3.5 to 6 m, so
		// from the field the leaves run up from the hedge into the crowns with little sky between (the bocage's walls 4 to
		// 6 m high in all). A wood's edge and a garden's hedge are lower and looser.
		const auto InField = [this](V2 P) { return P.X > -60.0 && P.Y > -60.0 && P.X < FW + 60.0 && P.Y < FH + 60.0; };
		for (const Run& Rn : Runs)
		{
			Rng R(Rn.Key ^ 0x9A17);
			const double Total = LineLength(Rn.P);
			const bool bGarden = Rn.Kind == HkGarden, bBocage = Rn.Kind == HkHedgerow || Rn.Kind == HkLaneBank;
			const double Thin = bBocage ? 0.0 : R.Next() < 0.15 ? 0.3 : 0.06;
			const bool bPollards = bBocage && R.Next() < 0.12;
			const double TreeStep = bGarden ? 1e9 : !bBocage ? 45.0 : bPollards ? R.Range(6.0, 8.5) : R.Range(7.5, 11.0);
			const double Level = bBocage ? R.Range(2.0, 2.6) : R.Range(2.0, 2.5), Along = static_cast<double>(Rn.Key % 1013);
			const double Wave = R.Range(6.0, 10.0), Swell = R.Range(1.0, 1.4);
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
			double NextKind = 0.0, NextTall = R.Range(2.0, 10.0);
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
				V2 P = PointAt(S, Way);
				P = P + Perp(Way) * (bBocage ? R.Range(-0.28, 0.28) : R.Range(-0.35, 0.35));
				if (!Clear(P.X, P.Y, Built | (bBocage ? 0 : OcMan))) continue;
				bool bTrod = false, bThrough = false;
				if (bBocage)
				{
					bTrod = InHisWay(P, Way, 0.55);
					bThrough = !Nudged(P, Way, 0.55, 0.35, 1.2);
				}
				else if (!ManClear(P, Way)) continue;
				const double Shade = R.Range(0.9, 1.1);
				double Scale, High;
				Base What = Kind;
				if (bBocage)
				{
					Scale = R.Range(0.9, 1.35);
					High = std::max(1.4, Level + Swell * Noise(S / Wave, Along, Seed ^ 0x9A18) + 0.75 * Noise(S / 2.5, Along, Seed ^ 0x9A19) + R.Range(-0.4, 0.4));
					// (a clump grown up: hazel or elder, 3.9 to 5 m over the bank; where a man went over, the next one)
					if (S >= NextTall && !bTrod)
					{
						NextTall = S + R.Range(8.0, 14.0);
						What = R.Next() < 0.6 ? Base::ShrubC : Base::ShrubB;
						Scale = R.Range(1.1, 1.4);
						High = R.Range(3.9, 5.0);
					}
				}
				else
				{
					Scale = R.Range(1.0, 1.45);
					High = R.Range(Lo, Hi);
				}
				if (bTrod) High *= 0.85;
				const double Turned = R.Range(0.0, 2.0 * Pi);
				const double TintR = R.Range(0.95, 1.05);
				const double TintB = R.Range(0.92, 1.05);
				Plant(What, P.X, P.Y, Turned, Scale, High / ShrubHigh, Tint8(Shade * TintR, Shade, Shade * TintB), bThrough);
				// Brambles and nettles: on a bocage bank in the field, hanging over both its faces most of their length; else
				// spilling from the bank's foot here and there (not where he goes over).
				if (bGarden || bTrod) continue;
				const double Wide = Rn.W[std::min(Seg, Rn.W.size() - 1)];
				for (const double Side : { -1.0, 1.0 })
				{
					const bool bFaces = bBocage && InField(P);
					if (R.Next() >= (bFaces ? 0.3 : bBocage ? 0.06 : 0.11)) continue;
					const V2 F = P + Perp(Way) * (Side * Wide * (bFaces ? R.Range(0.25, 0.45) : 0.5));
					if (Clear(F.X, F.Y, Built | OcMan) && LaneD[Pix(F.X, F.Y)] > 0.8f)
					{
						const double BrambleTurned = R.Range(0.0, 2.0 * Pi);
						const double BrambleWide = R.Range(0.7, 1.2);
						const double Tall = R.Range(0.6, 1.1);
						Plant(Base::Bramble, F.X, F.Y, BrambleTurned, BrambleWide, Tall, Tint8(1, 1, 1));
					}
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
				V2 P = PointAt(S, Way);
				P = P + Perp(Way) * R.Range(-0.3, 0.3);
				if (bBocage ? !Clear(P.X, P.Y, static_cast<uint8_t>(Built | OcCut)) || !Nudged(P, Way, 0.9, 0.45, 4.0) || !Clear(P.X, P.Y, OcCut)
							: !Clear(P.X, P.Y, static_cast<uint8_t>(Built | OcMan | OcCut))) continue;
				double High = 0.0;
				const Base B = bBocage ? BocageTree(R, bPollards, High) : HedgeTree(R, false, High);
				const double Spread = R.Range(0.85, 1.15);
				const double Turned = R.Range(0.0, 2.0 * Pi);
				const double TintR = R.Range(0.92, 1.06);
				const double TintG = R.Range(0.94, 1.06);
				const double TintB = R.Range(0.9, 1.04);
				Plant(B, P.X, P.Y, Turned, High / TreeHigh(B) * Spread, High / TreeHigh(B), Tint8(TintR, TintG, TintB));
			}
			// The young shoots between them (in the field and a little round it): oak, ash and pollard growth 3.5 to 6 m,
			// every 4 to 7 m, their crowns down in the hedge's top.
			if (!bBocage) continue;
			Seg = 0;
			Before = 0.0;
			for (double S = R.Range(1.5, 6.0); S < Total - 2.0; S += R.Range(4.0, 7.0))
			{
				V2 Way;
				V2 P = PointAt(S, Way);
				P = P + Perp(Way) * R.Range(-0.35, 0.35);
				if (!InField(P) || !Clear(P.X, P.Y, static_cast<uint8_t>(Built | OcCut)) || !Nudged(P, Way, 0.9, 0.45, 1.5) || !Clear(P.X, P.Y, OcCut)) continue;
				const double U = R.Next();
				const Base B = U < 0.45 ? Base::Oak : U < 0.75 ? Base::Ash : Base::Pollard;
				const double High = R.Range(3.5, 6.0);
				const double Turned = R.Range(0.0, 2.0 * Pi);
				const double Spread = R.Range(1.0, 1.35);
				const double TintR = R.Range(0.95, 1.08);
				const double TintG = R.Range(0.97, 1.08);
				const double TintB = R.Range(0.9, 1.02);
				Plant(B, P.X, P.Y, Turned, High / TreeHigh(B) * Spread, High / TreeHigh(B), Tint8(TintR, TintG, TintB));
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
					const Base B = Sp ? SteppeTree(R, High) : HedgeTree(R, true, High);
					const double Turned = R.Range(0.0, 2.0 * Pi);
					const double Spread = R.Range(0.85, 1.1);
					const double TintR = R.Range(0.9, 1.05);
					const double TintG = R.Range(0.92, 1.06);
					const double TintB = R.Range(0.9, 1.04);
					Plant(B, PX, PY, Turned, High / TreeHigh(B) * Spread, High / TreeHigh(B), Tint8(TintR, TintG, TintB));
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
					const size_t Was = Inst[static_cast<size_t>(B)].size();
					const double Turned = R.Range(0.0, 2.0 * Pi);
					const double Spread = R.Range(0.8, 1.3);
					const double TintR = R.Range(0.92, 1.04);
					const double TintG = R.Range(0.94, 1.06);
					Plant(B, PX, PY, Turned, Spread, High, Tint8(TintR, TintG, 1.0));
					// (where a man's way goes in at the wood's edge, its undergrowth there is opened as a hedge's is: taken back
					// after its numbers are drawn, so the rest of the wood is laid as before)
					if (DistE[Pix(PX, PY)] < 3.0f && WayRoom({ PX, PY }) < 1.7) Inst[static_cast<size_t>(B)].resize(Was);
				}
		}
		// Orchards in rows along their parcel; a lone oak in some pastures; fruit trees in the gardens.
		{
			std::vector<double> X0(Parcels.size(), 1e9), Y0(Parcels.size(), 1e9), X1(Parcels.size(), -1e9), Y1(Parcels.size(), -1e9);
			for (int J = 0; J < NY; ++J)
				for (int I = 0; I < NX; ++I)
				{
					const size_t C = static_cast<size_t>(Pid[static_cast<size_t>(J) * NX + I]);
					if (!Parcels[C].bOrchard && !(Parcels[C].Kind == RgOpen && Parcels[C].Crop == CpHayCut)) continue;
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
							const double Turned = R.Range(0.0, 2.0 * Pi);
							const double Spread = R.Range(0.9, 1.15);
							const double TintR = R.Range(0.94, 1.06);
							const double TintB = R.Range(0.94, 1.04);
							Plant(Base::Apple, PX, PY, Turned, High / TreeHigh(Base::Apple) * Spread, High / TreeHigh(Base::Apple), Tint8(TintR, 1.0, TintB));
						}
				}
				else if (Sp && Q.Kind == RgOpen && Q.Crop == CpSteppe && Q.Area > 20000.0)
				{
					// The steppe's hay: ricks in twos to fours, some one group to ten hectares of pasture.
					const int Groups = static_cast<int>(std::lround(Q.Area / 100000.0 * R.Range(0.6, 1.4)));
					for (int Gr = 0; Gr < Groups; ++Gr)
					{
						const double Reach = std::sqrt(Q.Area) * 0.4;
						const double GX = Q.CX + R.Range(-Reach, Reach), GY = Q.CY + R.Range(-Reach, Reach);
						const int Stacks = 2 + R.Below(3);
						for (int K = 0; K < Stacks; ++K)
						{
							const double PX = GX + R.Range(-12.0, 12.0), PY = GY + R.Range(-12.0, 12.0);
							if (!InRaster(PX, PY) || static_cast<size_t>(Pid[Pix(PX, PY)]) != C || DistE[Pix(PX, PY)] < 12.0f || !Clear(PX, PY, OcHouse | OcNear | OcMan | OcVehicle)) continue;
							const double Sz = R.Range(0.8, 1.2);
							const double Turned = R.Range(0.0, 2.0 * Pi);
							const double Tall = R.Range(0.85, 1.1);
							const double TintR = R.Range(0.95, 1.05);
							const double TintB = R.Range(0.92, 1.04);
							Plant(Base::Haystack, PX, PY, Turned, Sz, Sz * Tall, Tint8(TintR, 1.0, TintB));
						}
					}
				}
				else if (Q.Kind == RgOpen && Q.Area > 6000.0 && Unit(Q.Key ^ 0xF1E1D) < (Sp ? 0.08 : 0.2))
				{
					const double PX = Q.CX + R.Range(-15.0, 15.0), PY = Q.CY + R.Range(-15.0, 15.0);
					if (InRaster(PX, PY) && static_cast<size_t>(Pid[Pix(PX, PY)]) == C && DistE[Pix(PX, PY)] > 18.0f && Clear(PX, PY, OcHouse | OcNear | OcWall | OcMan | OcVehicle))
					{
						const double High = R.Range(14.0, 19.0);
						const double Turned = R.Range(0.0, 2.0 * Pi);
						const double Spread = R.Range(1.0, 1.2);
						Plant(Base::Oak, PX, PY, Turned, High / 15.0 * Spread, High / 15.0, Tint8(1, 1, 1));
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
						else
						{
							const double Turned = R.Range(0.0, 2.0 * Pi);
							const double Wide = R.Range(0.8, 1.1);
							const double Tall = R.Range(1.5, 2.4);
							Plant(Base::ShrubB, PX, PY, Turned, Wide, Tall / ShrubHigh, Tint8(1, 1.03, 1));
						}
					}
				}
				// The bocage's mown meadows: in two of three the June hay stands in cocks in loose rows along the field's
				// long way (the mower's swaths), 9 to 13 m between the rows and 7 to 10 m along them, some 100 a hectare;
				// in some of those only part of the field is done. None within 5 m of the hedge or in the gateway, none on
				// a recorded way. (Their own draws, so the rest of the field is laid as before.)
				if (!Sp && Q.Kind == RgOpen && Q.Crop == CpHayCut && Q.Area > 1500.0)
				{
					Rng H(Q.Key ^ 0x4A1C0C);
					++Count.HayFields;
					if (H.Next() < 0.67 && X1[C] > X0[C])
					{
						const double Ca = std::cos(Q.Angle), Sa = std::sin(Q.Angle);
						const double Reach = std::hypot(X1[C] - X0[C], Y1[C] - Y0[C]) * 0.5 + 6.0;
						const double MX = 0.5 * (X0[C] + X1[C]), MY = 0.5 * (Y0[C] + Y1[C]);
						const double Gap = H.Range(9.0, 13.0), Step = H.Range(7.0, 10.0);
						// (part done: the rows past a line across the field still to cock)
						const double Done = H.Next() < 0.3 ? H.Range(-0.3, 0.4) * Reach : 1e9;
						const int Was = Count.Haycocks;
						for (double V = -Reach + H.Range(0.0, Gap); V <= Reach; V += Gap)
						{
							if (V > Done) break;
							const double Shift = H.Range(0.0, Step);
							for (double U = -Reach + Shift; U <= Reach; U += Step)
							{
								// (loose rows, and cocks of every size, 1.0 to 1.6 m high: the art director's cut 5, item 5, saw one size and
								// shape in a grid)
								const double Uj = U + H.Range(-2.4, 2.4), Vj = V + H.Range(-2.0, 2.0);
								const double PX = MX + Uj * Ca - Vj * Sa, PY = MY + Uj * Sa + Vj * Ca;
								const double Gone = H.Next(), Sz = H.Range(0.74, 1.04), Tall = H.Range(0.92, 1.08), Yaw = H.Range(0.0, 2.0 * Pi), Tn = H.Range(0.94, 1.06);
								if (Gone < 0.1 || !InRaster(PX, PY)) continue; // (a cock already carted, or never made)
								const size_t P = Pix(PX, PY);
								if (static_cast<size_t>(Pid[P]) != C || DistE[P] < 5.0f || (OccR[P] & OcGate) || !Clear(PX, PY, OcHouse | OcNear | OcWall | OcMan | OcVehicle)) continue;
								const size_t Before = Inst[static_cast<size_t>(Base::Haycock)].size();
								Plant(Base::Haycock, PX, PY, Yaw, Sz, Sz * Tall, Tint8(Tn, Tn, Tn * H.Range(0.96, 1.03)));
								if (Inst[static_cast<size_t>(Base::Haycock)].size() > Before) ++Count.Haycocks;
							}
						}
						if (Count.Haycocks > Was) ++Count.HayFieldsCocked;
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
		// The steppe's: the kit's cottages on their floors, the ruins, wattle, sweeps and ricks where the farm put them,
		// and the telegraph poles along the railway, every 50 m, 8 m to its west, a kilometre on past the tiles each way.
		if (Sp)
		{
			Rng R(Seed ^ 0x6417);
			for (const House& H : Houses)
			{
				if (!H.bKit) continue;
				std::vector<Placed>& L = Inst[static_cast<size_t>(Base::Khata)];
				const size_t Was = L.size();
				const double TintR = R.Range(0.96, 1.03);
				const double TintG = R.Range(0.96, 1.02);
				const double TintB = R.Range(0.95, 1.02);
				Plant(Base::Khata, H.C.X, H.C.Y, H.Yaw, 1.0, 1.0, Tint8(TintR, TintG, TintB));
				if (L.size() > Was) L.back().Z = static_cast<float>(H.Floor - 0.13); // (its plinth on the highest ground under it)
			}
			for (const KitSpot& K : Kits)
			{
				const double TintR = R.Range(0.95, 1.04);
				const double TintG = R.Range(0.95, 1.03);
				const double TintB = R.Range(0.94, 1.03);
				Plant(K.B, K.P.X, K.P.Y, K.Yaw, K.Scale, K.ScaleZ, Tint8(TintR, TintG, TintB));
			}
			if (Sp->HasRail())
			{
				std::vector<std::pair<double, double>> At;
				std::vector<double> Yaw;
				LandSteppe::PolesAlongRail(*Sp, -Band - 1000.0, -Band + TilesY() * TileSize + 1000.0, 50.0, 8.0, At, Yaw);
				for (size_t K = 0; K < At.size(); ++K)
				{
					const size_t Was = Inst[static_cast<size_t>(Base::Pole)].size();
					const double Lean = R.Range(-0.04, 0.04);
					const double Tall = R.Range(0.94, 1.05);
					const double TintR = R.Range(0.92, 1.06);
					const double TintG = R.Range(0.94, 1.04);
					Plant(Base::Pole, At[K].first, At[K].second, Yaw[K] + Lean, 1.0, Tall, Tint8(TintR, TintG, 1.0));
					if (Inst[static_cast<size_t>(Base::Pole)].size() > Was) ++Count.Poles;
				}
			}
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
		// (the pale crops a step darker: the second cut's stubble lit near white, 236 228 176, where the art director
		// asks open ground lit 120 to 160; 21 November)
		// (the mown meadow a step darker and greener again: it lit 170 to 190 behind the knocked-out Tiger on the fourth
		// cut's frames, where the art director asks open ground 120 to 160; the aftermath coming green through the
		// cut stems three weeks on, 22 November)
		const Col HayCut = Rgb(114, 120, 70);        // a meadow mown in June: the aftermath, hay-coloured stems over new green
		const Col Meadow = Rgb(126, 128, 74);        // standing grass gone to seed: buff over green
		// (the corn painted warmer, red by 1.12 and blue by 0.56 of the bytes: the grade keeps under half a middle tone's
		// colour, and July wheat at 166 140 76 came out a bleached cream, 137 137 108; the art director's cut 5, item 4,
		// asks it lit gold, red over green 1.08 to 1.15 and blue under it 0.6 to 0.65. Its stalks and bundles the same)
		const Col Wheat = Rgb(186, 140, 43);         // wheat ripening, gold with green in it (a stand of it, its stalks shading one another)
		const Col Barley = Rgb(186, 148, 53);        // barley, ripe and paler
		const Col Oats = Rgb(168, 144, 47);          // oats, greener gold
		const Col GreenWheat = Rgb(134, 128, 38);    // a late field, still green
		// Barley cut by the binder: the straw's stubble in its rows over brown earth, with the clover undersown and the
		// weeds coming green between them. The stubble lit 198 on the third cut's frames with the grade's half stop
		// down, and as flat as a sheet (its spread, darkest tenth to brightest, 40); the art director asked it a fifth
		// darker and spread 60 or more (22 November): the straw itself a little darker, and the earth and the green
		// showing through in bands along the lands and in patches.
		const Col Stubble = Rgb(160, 144, 100);       // the straw stubble, standing
		const Col StubbleEarth = Rgb(104, 86, 62);   // the earth between its rows, where the sheaves' stooks stood and the carts went
		const Col Regrowth = Rgb(98, 106, 60);       // clover undersown, and the weeds: green coming through the stubble
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
			case CpRye: return Rgb(162, 146, 104);
			case CpSteppe: return Rgb(130, 128, 80);
			case CpFallow: return Rgb(76, 70, 52);
			default: return PastureGrazed;
			}
		}
		bool IsCorn(int Crop) { return Crop == CpWheat || Crop == CpBarley || Crop == CpOats || Crop == CpGreenWheat || Crop == CpRye; }
		/** A parcel's crop as the steppe's colours know it. */
		int SteppeCrop(int Crop)
		{
			return Crop == CpWheat ? LandSteppe::CropWheat : Crop == CpRye ? LandSteppe::CropRye : Crop == CpFallow ? LandSteppe::CropFallow : LandSteppe::CropPasture;
		}
		Col OfTone(const LandSteppe::Tone& T) { return Col{ T.R, T.G, T.B }; }
		LandSteppe::Tone ToTone(const Col& C) { return LandSteppe::Tone{ C.R, C.G, C.B }; }

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
		if (!SnowLay) return Wetted(bTone ? Toned(GroundBare(X, Y), X, Y) : GroundBare(X, Y), X, Y);
		if (!InRaster(X, Y)) return Snowed(Wetted(bTone ? Toned(GroundBare(X, Y), X, Y) : GroundBare(X, Y), X, Y), X, Y, false);
		// (under snow the place read once, for the ground's colour and the snow's both: read twice, the snow cost as much
		// as the ground under it, and the track marks ask this colour at every step of every tank)
		const LandScape::Spot S = SpotAt(X, Y, true);
		const Col C = Wetted(bTone ? Toned(GroundBare(X, Y, &S), X, Y) : GroundBare(X, Y, &S), X, Y);
		const LandScape::Tone T = SnowLay->Over(LandScape::Tone{ C.R, C.G, C.B }, S);
		return Col{ T.R, T.G, T.B };
	}

	/**
	 * Each field its own (on trial, "fieldtone"): its farmer grazed it or cut it on his own day, on his own soil and his
	 * own drainage, so no two fields of one crop are quite one green. A pasture a little lighter or darker (8% either
	 * way) and a little warmer, one in four fresh bitten (paler, warmer), one in five rank and ungrazed (darker, a deeper
	 * green, never bluer); corn 6% either way and a little warmer or cooler, one in three a week behind (greener), one in
	 * four over-ripe (paler). The whole parcel the one tone, so no new mottle; on a plant's paint only (a sward's green
	 * well over its blue, or ripe corn's red over its blue), never earth, dust or water. The ground the men stand on is
	 * never under nine tenths of its value, and a sward never bluer: the men read against it as before.
	 * (Not a region's ground: its looks are its own.)
	 */
	Col Ground::Impl::Toned(Col C, double X, double Y) const
	{
		if (Rg || !InRaster(X, Y)) return C;
		const size_t P = Pix(X, Y);
		if (Pid[P] < 0) return C;
		const uint8_t Kind = Rgn[P];
		if (Kind != RgOpen && Kind != RgField && Kind != RgRough) return C;
		const ParcelInfo& Q = Parcels[static_cast<size_t>(Pid[P])];
		if (Q.Key == 0) return C;
		const double Green = Smooth(0.04, 0.16, (C.G - C.B) / std::max(1.0, C.G)) * Smooth(0.82, 0.95, C.G / std::max(1.0, C.R));
		const double Ripe = Smooth(0.25, 0.4, (C.R - C.B) / std::max(1.0, C.R)) * Smooth(1.15, 0.95, C.G / std::max(1.0, C.R)) * (1.0 - Green);
		if (Green + Ripe < 0.02) return C;
		// (the parcel's three draws, each its own: the same on every compiler)
		const double U1 = Unit(Hash(Q.Key, 0x70E1));
		const double U2 = Unit(Hash(Q.Key, 0x70E2));
		const double U3 = Unit(Hash(Q.Key, 0x70E3));
		const bool bGrass = !(Q.Kind == RgField && Q.Crop != CpRoots);
		const auto Luma = [](const Col& A) { return 0.2126 * A.R + 0.7152 * A.G + 0.0722 * A.B; };
		const double Was = Luma(C);
		Col T;
		if (bGrass)
		{
			T = Varied(C, 1.0 + 0.16 * (U1 - 0.5), 0.06 * U2);
			if (U3 < 0.25) T = Varied(T, 1.06, 0.04);                                  // fresh bitten
			else if (U3 > 0.8) T = Varied(Mixed(T, Rgb(60, 88, 44), 0.12), 0.93, 0.0); // rank, left ungrazed
			// Never bluer than the crop's own paint: a sward gone blue reads as a photograph's cast, not a field. And a
			// darker field seen across the haze reads bluer by itself (the air's blue the same, the field's own green less),
			// so the darker a sward is made, the less blue its paint: its blue/green down by its darkening (measured from
			// the general's height, 170 and 400 m up; the field close by only a deeper green).
			const double Dark = 1.0 - std::clamp(Luma(T) / std::max(1.0, Was), 0.9, 1.0);
			const double Most = C.B / std::max(1.0, C.G) * (1.0 - 1.2 * Dark) + 0.005;
			if (T.B > Most * T.G) T.B = Most * T.G;
		}
		else
		{
			T = Varied(C, 1.0 + 0.12 * (U1 - 0.5), 0.06 * (U2 - 0.5));
			if (U3 < 0.33) T = Mixed(T, Rgb(150, 146, 62), 0.18); // a week behind, still greenish
			else if (U3 > 0.75) T = Varied(T, 1.05, 0.03);       // over-ripe, bleached
		}
		// (the men read against the ground: never under nine tenths of its value; the whole colour lifted, its hue kept)
		const double Now = Luma(T);
		if (Now < 0.9 * Was && Now > 0.0) T = Scaled(T, 0.9 * Was / Now);
		return Mixed(C, T, std::max(Green, Ripe));
	}

	Col Ground::Impl::GroundBare(double X, double Y, const LandScape::Spot* Pre) const
	{
		if (Rg) return RegionColour(X, Y, Pre);
		if (Sp) return SteppeColour(X, Y, Pre);
		if (!InRaster(X, Y)) return FarBare(X, Y);
		const size_t P = Pix(X, Y);
		const ParcelInfo& Q = Parcels[static_cast<size_t>(Pid[P])];
		const double Edge = Bilinear(DistE, X, Y);
		// (the same three noises a Spot reads: taken from it when the caller has one)
		const double N1 = Pre ? Pre->N1 : Fbm(X, Y, 30.0, 2, Seed ^ 0x11), N2 = Pre ? Pre->N2 : Noise(X / 7.0, Y / 7.0, Seed ^ 0x12);
		const double Broad = Pre ? Pre->Broad : Fbm(X, Y, 320.0, 2, Seed ^ 0x13);
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
			if (Q.Crop == CpStubble)
			{
				// Bare in bands along the lands (the binder's swath and the carts' wheels) and in patches; green in others.
				const double Bare = Smooth(0.1, 0.35, 0.5 * Lands + 0.6 * N2 + 0.3 * Streak);
				C = Mixed(C, StubbleEarth, 0.85 * Bare);
				C = Mixed(C, Regrowth, 0.6 * Smooth(0.1, 0.35, N1 + 0.35 * Noise(X / 11.0, Y / 11.0, Seed ^ 0x17)));
			}
			C = Mixed(C, Margin, 0.55 * Smooth(5.0, 1.5, Edge)); // the headland: thin corn, weeds
		}
		else
		{
			// Grass: darker tufts where the cattle left it, paler where they grazed it down; thistles and docks.
			C = Varied(C, 1.0 + 0.08 * N1, 0.03 * N1);
			// (a mown meadow's patches the regrowth's green, not the standing meadow's buff: the hay lifted, the clover under it)
			C = Mixed(C, Q.Crop == CpHayCut ? PastureGrazed : PastureLush, (Q.Crop == CpHayCut ? 0.45 : 0.3) * Smooth(0.2, 0.55, N2));
			C = Mixed(C, Margin, 0.45 * Smooth(5.0, 1.0, Edge));
		}
		C = Mixed(C, HedgeFoot, 0.55 * Smooth(2.6, 0.6, Edge));
		if (OccR[P] & OcGate) C = Mixed(C, Earth, 0.45 * (0.6 + 0.4 * N2));
		return Varied(C, 1.0 + 0.06 * N2 + 0.05 * Broad, 0.02 * Broad);
	}


	/**
	 * The steppe's ground (LandSteppe's colours): each farm field its crop, wheat banded by the tractor's drill and
	 * streaked along its rows, the grassy strip where two fields meet; the balka's and the waste ground's dry grass,
	 * greener in the hollows; the dirt roads black earth worn to grey dust, their ruts darker, grass along their verges;
	 * the railway's bank its ballast and cinders, grass taking its sides; the gardens and yards as the bocage's.
	 */
	Col Ground::Impl::SteppeColour(double X, double Y, const LandScape::Spot* Pre) const
	{
		if (!InRaster(X, Y)) return FarBare(X, Y);
		const size_t P = Pix(X, Y);
		const ParcelInfo& Q = Parcels[static_cast<size_t>(Pid[P])];
		const double Edge = Bilinear(DistE, X, Y);
		const double N1 = Pre ? Pre->N1 : Fbm(X, Y, 30.0, 2, Seed ^ 0x11), N2 = Pre ? Pre->N2 : Noise(X / 7.0, Y / 7.0, Seed ^ 0x12);
		const double Broad = Pre ? Pre->Broad : Fbm(X, Y, 320.0, 2, Seed ^ 0x13);
		Col C;
		bool bStrip = true; // (the strip at its edge still to lay)
		switch (Rgn[P])
		{
		case RgWoods:
			C = Mixed(Litter, WoodGreen, 0.35 + 0.5 * N1 + 0.4 * Smooth(7.0, 1.0, Edge));
			C = Varied(C, 1.0 + 0.1 * N2, 0.03 * N1);
			break;
		case RgWater: return WaterCol;
		case RgMarsh: return Varied(MarshCol, 1.0 + 0.08 * N1, 0.0);
		case RgLane:
		{
			bStrip = false;
			const double D = Bilinear(LaneD, X, Y);
			if (D < 0.0)
			{
				// (two ruts a cart's width apart either side of the middle, the middle grassed a little)
				const double Off = std::fabs(D + LaneHalfAt(X, Y));
				const double Rut = Smooth(0.5, 0.15, std::fabs(Off - 0.75));
				C = Mixed(OfTone(LandSteppe::RoadDust()), OfTone(LandSteppe::RoadRut()), 0.7 * Rut);
				C = Mixed(C, Rgb(110, 110, 72), 0.3 * Smooth(0.35, 0.0, Off) * (0.5 + 0.5 * N1));
				C = Varied(C, 1.0 + 0.06 * N2, 0.01);
			}
			// (its verge: the floor's dust into the grassy strip the fields beside it end in, by the lane's edge)
			else C = Mixed(Varied(OfTone(LandSteppe::RoadDust()), 1.0 + 0.06 * N2, 0.01), OfTone(LandSteppe::StripTone(N2, Broad)), Smooth(0.0, LaneWall + 0.3, D));
			break;
		}
		case RgYard:
			C = Mixed(YardEarth, Muck, Smooth(0.1, 0.5, N1));
			C = Varied(C, 1.0 + 0.08 * N2, 0.02 * N1);
			break;
		case RgVillage:
			C = Mixed(Garden, Rgb(104, 112, 60), 0.5 + 0.5 * N1);
			C = Varied(C, 1.0 + 0.08 * N2 + 0.05 * Broad, 0.02 * N1);
			break;
		case RgWorks:
			C = Mixed(Rgb(82, 72, 58), Rgb(96, 84, 66), Smooth(0.0, 0.45, N1 + 0.3 * N2)); // black earth dug and thrown up
			C = Mixed(C, OfTone(LandSteppe::RoughColour(N1, N2)), 0.35 * Smooth(0.2, -0.3, N1));
			C = Varied(C, 1.0 + 0.07 * N2, 0.0);
			break;
		default:
			if (Q.Kind == RgRough) C = OfTone(LandSteppe::RoughColour(N1, N2));
			else if (Rgn[P] == RgOpen || Rgn[P] == RgField)
			{
				// (the tractor's drill bands across the field's length, some 20 to 30 m apart; its rows streak along it)
				const double Ca = std::cos(Q.Angle), Sa = std::sin(Q.Angle);
				const double Along = (X - Q.CX) * Ca + (Y - Q.CY) * Sa, Across = -(X - Q.CX) * Sa + (Y - Q.CY) * Ca;
				const double Lands = std::cos(2.0 * Pi * Across / (1.6 * Q.Lands));
				const double Streak = Noise(Along / 40.0, Across / 2.5, Seed ^ 0x14);
				C = OfTone(LandSteppe::FieldColour(SteppeCrop(Q.Crop), N1, N2, Broad, Lands, Streak, Edge));
				bStrip = false; // (its own strip at its edge)
			}
			else C = OfTone(LandSteppe::RoughColour(N1, N2));
			break;
		}
		// Every edge between two kinds of ground the same grassy strip, so the colour runs on unbroken across it.
		if (bStrip) C = Mixed(C, OfTone(LandSteppe::StripTone(N2, Broad)), LandSteppe::StripShare(Edge));
		if (Sp->HasRail())
		{
			const double D = Sp->RailDistance(X, Y);
			if (D < 10.0) C = OfTone(LandSteppe::BankColour(D, N1, N2, ToTone(C)));
		}
		return C;
	}

	/** Beyond the band: the same parcels by their seeds, their crops, woods; hedges as a darkening along the parcels' edges. */
	Col Ground::Impl::FarColour(double X, double Y) const
	{
		const Col C = FarBare(X, Y);
		return SnowLay ? Snowed(C, X, Y, true) : Soaked(C); // (rain's soak as near, without its pools: Soaked)
	}

	Col Ground::Impl::FarBare(double X, double Y) const
	{
		if (Rg) return RegionFar(X, Y);
		const double Broad = Fbm(X, Y, 500.0, 2, Seed ^ 0x15), N1 = Fbm(X, Y, 90.0, 2, Seed ^ 0x16);
		if (WoodsBeyond(X, Y)) return Varied(CanopyCol, 1.0 + 0.12 * N1, 0.02 * Broad);
		if (Sp)
		{
			// (the steppe's fields on to the haze, the strip between them a faint line; the railway's bank running on)
			int Crop = LandSteppe::CropPasture;
			double Gap = 0.0;
			Sp->FieldAt(X, Y, Crop, &Gap);
			Col C = OfTone(LandSteppe::FarFieldColour(Crop, N1, Broad, Gap));
			if (Sp->HasRail())
			{
				const double D = Sp->RailDistance(X, Y);
				if (D < 10.0) C = OfTone(LandSteppe::BankColour(D, N1, 0.0, ToTone(C)));
			}
			return C;
		}
		double Gap = 0.0;
		uint8_t Kind = RgOpen;
		const int S = Plot(X, Y, &Gap, &Kind);
		const int Crop = CropOf(S, Kind);
		Col C = CropColour(Crop);
		// (a stubble far off: its straw, earth and green together, as the near ground's paint comes out on the whole)
		if (Crop == CpStubble) C = Mixed(Mixed(C, StubbleEarth, 0.3), Regrowth, 0.2);
		C = Varied(C, 1.0 + 0.08 * N1 + 0.05 * Broad, 0.02 * Broad);
		return Mixed(C, FarHedge, 0.5 * Smooth(9.0, 1.0, Gap));
	}

	// ====================================================================== a region's ground, and snow over any

	LandScape::Spot Ground::Impl::SpotAt(double X, double Y, bool bNoise) const
	{
		LandScape::Spot S;
		S.X = X;
		S.Y = Y;
		if (bNoise)
		{
			S.N1 = Fbm(X, Y, 30.0, 2, Seed ^ 0x11);
			S.N2 = Noise(X / 7.0, Y / 7.0, Seed ^ 0x12);
			S.Broad = Fbm(X, Y, 320.0, 2, Seed ^ 0x13);
		}
		if (!InRaster(X, Y))
		{
			// Out in the ring: the plot's seed, its kind and crop as the band's, the woods by the country's noise.
			S.bLaid = false;
			double Gap = 0.0;
			uint8_t Kind = RgOpen;
			const int Index = Plot(X, Y, &Gap, &Kind);
			const int Crop = CropOf(Index, Kind);
			S.Kind = WoodsBeyond(X, Y) ? LandScape::KWoods : static_cast<int>(Kind);
			S.Crop = Crop >= CpRegion ? Crop - CpRegion : -1;
			S.Edge = Gap;
			S.Key = Hash(Seed ^ 0xBA2C, static_cast<uint64_t>(Index));
			return S;
		}
		const size_t P = Pix(X, Y);
		// (a region's sites are laid before the parcels: until MakeParcels no parcel and no edge distance, not a read
		// past the end)
		const bool bParcels = Pid.size() == Rgn.size();
		const int Id = bParcels ? Pid[P] : -1;
		int K = Rgn[P];
		if (Id >= 0)
		{
			const ParcelInfo& Q = Parcels[static_cast<size_t>(Id)];
			if (K == RgOpen || K == RgField || K == RgRough) K = Q.Kind;
			S.Crop = Q.Crop >= CpRegion ? Q.Crop - CpRegion : -1;
			S.Key = Q.Key;
			S.Area = Q.Area;
			S.Angle = Q.Angle;
			const double Ca = std::cos(Q.Angle), Sa = std::sin(Q.Angle);
			S.Along = (X - Q.CX) * Ca + (Y - Q.CY) * Sa;
			S.Across = -(X - Q.CX) * Sa + (Y - Q.CY) * Ca;
		}
		S.Kind = K < LandScape::KCount ? K : static_cast<int>(LandScape::KOpen);
		S.Edge = DistE.size() != Rgn.size() ? 1e6 : bNoise ? Bilinear(DistE, X, Y) : DistE[P];
		S.Lane = bNoise ? Bilinear(LaneD, X, Y) : LaneD[P];
		if (S.Lane < LaneWall + 3.0) S.LaneHalf = LaneHalfAt(X, Y);
		const uint8_t O = OccR[P];
		S.bNear = (O & OcNear) != 0;
		S.bGate = (O & OcGate) != 0;
		S.bMan = (O & OcMan) != 0;
		S.bVehicle = (O & OcVehicle) != 0;
		return S;
	}

	Col Ground::Impl::RegionColour(double X, double Y, const LandScape::Spot* Pre) const
	{
		if (!InRaster(X, Y)) return RegionFar(X, Y);
		const LandScape::Tone T = Rg->Colour(Pre ? *Pre : SpotAt(X, Y, true));
		return Col{ T.R, T.G, T.B };
	}

	Col Ground::Impl::RegionFar(double X, double Y) const
	{
		const LandScape::Spot S = SpotAt(X, Y, true);
		const LandScape::Tone T = Rg->Far(S, S.Kind == LandScape::KWoods && !S.bLaid, S.Edge);
		return Col{ T.R, T.G, T.B };
	}

	Col Ground::Impl::RegionLane(double X, double Y, double A, double Half) const
	{
		LandScape::Spot S = SpotAt(X, Y, true);
		S.Kind = LandScape::KLane;
		S.Lane = A - Half;
		S.LaneHalf = Half;
		const LandScape::Tone T = Rg->Colour(S);
		const Col C{ T.R, T.G, T.B };
		return SnowLay ? Snowed(C, X, Y, false) : C;
	}

	Col Ground::Impl::Snowed(const Col& C, double X, double Y, bool bFar) const
	{
		const LandScape::Tone Bare{ C.R, C.G, C.B };
		LandScape::Tone T;
		if (bFar || !InRaster(X, Y))
		{
			LandScape::Spot S;
			S.X = X;
			S.Y = Y;
			S.bLaid = false;
			S.N1 = Fbm(X, Y, 90.0, 2, Seed ^ 0x16);
			S.Broad = Fbm(X, Y, 500.0, 2, Seed ^ 0x15);
			const bool bWoods = WoodsBeyond(X, Y);
			S.Kind = bWoods ? LandScape::KWoods : LandScape::KOpen;
			T = SnowLay->OverFar(Bare, S, bWoods);
		}
		else
		{
			// (no height or drawn kind: no snow reads them, and the drawn top cost as much as the rest)
			T = SnowLay->Over(Bare, SpotAt(X, Y, true));
		}
		return Col{ T.R, T.G, T.B };
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
		/** A hole in a wall: along it from A0 to A1, up from Z0 to Z1. What: 0 glass, 1 door, 2 shutters shut, 3 belfry louvre, 4 cart door, 5 the wattle bared where a shed's daub fell. */
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
				if (H.bWhite)
				{
					// The steppe farm's sheds: whitewash over clay and brick, greyed and flaking, under weathered planks or straw.
					L.Stone = Varied(Rgb(198, 194, 180), Tone, 0.5 * Warm);
					L.Dressed = Mixed(L.Stone, Rgb(150, 140, 124), 0.25);
					L.Roof = H.Roof == 1 ? Varied(Rgb(128, 110, 78), R.Range(0.9, 1.06), 0.0) : Varied(Rgb(96, 92, 86), R.Range(0.9, 1.08), 0.0);
					L.Shutter = Rgb(84, 104, 108);
					L.Door = Rgb(88, 74, 58);
				}
			}
			// The eastern farm's sheds on trial ("sheds", the art director's blank barn, 25 Nov). On the steppe (bDaub): clay
			// on wattle, whitewashed, the wash flaked to the clay and damp at the foot, the wattle bared where the daub fell; the
			// barn's on a post frame that shows, limewashed or clay-brown; both under grey straw thatch in courses, its ridge
			// held by crossed poles. In the forest zone: weathered boards stood upright, each its own grey or brown, lapped on
			// its neighbour, under the old roof. Either: a plinth of fieldstone or old brick, the gables boarded to the rake,
			// plank doors on strap hinges, hatches, small windows.
			struct ShedLook
			{
				bool bPlank = false, bFrame = false, bThatch = false;
				double Lap = 0.0; // how far its boards stand out of the wall's line
				Col Board, BoardB, Wash, Clay, Plinth, Timber, Leaf, Iron, Fallen, Within, Thatch, Withy;
			} Sd;
			if (H.bBoards)
			{
				const uint64_t SK = Hash(H.Key, 0x5ED5ull);
				// (the historian, 25 Nov: upright boards are the forest's habit. On the black-earth steppe a kolkhoz barn and
				// byre are clay on wattle, the barn's on a post frame, limewashed or left clay-brown, under straw thatch gone
				// grey; boards only for the doors and the gables)
				Sd.bPlank = !H.bDaub;
				Sd.bFrame = H.bDaub && H.Type == HtBarn;
				Sd.bThatch = H.bDaub;
				Sd.Lap = Sd.bPlank ? 0.04 : 0.0;
				const double T = 0.92 + 0.14 * Unit(Mix(SK ^ 1));
				Sd.Board = Scaled(Rgb(132, 128, 120), T);
				Sd.BoardB = Scaled(Rgb(116, 102, 86), T);
				Sd.Wash = L.Stone;
				Sd.Clay = Rgb(150, 124, 92);
				if (Sd.bFrame && Unit(Mix(SK ^ 4)) < 0.5) Sd.Wash = Varied(Mixed(Sd.Clay, Rgb(134, 124, 106), 0.3), T, 0.0); // (its daub never limewashed)
				Sd.Thatch = Scaled(Rgb(132, 125, 108), 0.94 + 0.12 * Unit(Mix(SK ^ 5)));
				Sd.Withy = Rgb(124, 106, 80);
				Sd.Plinth = Unit(Mix(SK ^ 2)) < 0.5 ? Rgb(112, 104, 94) : Rgb(94, 79, 70); // (fieldstone, or old brick gone dull)
				Sd.Timber = Scaled(Rgb(98, 86, 72), T);
				const double UL = Unit(Mix(SK ^ 3));
				// (its doors bare, or an old paint gone chalky: blue-green, ochre red, green)
				Sd.Leaf = UL < 0.5 ? Rgb(110, 100, 86) : Mixed(UL < 0.75 ? Rgb(92, 110, 112) : UL < 0.9 ? Rgb(126, 80, 62) : Rgb(96, 108, 86), Rgb(122, 118, 110), 0.35);
				Sd.Iron = Rgb(48, 44, 42);
				Sd.Within = Rgb(38, 34, 31); // (a shed's inside seen through a breach: unlit timber, darker than a house's plaster)
				Sd.Fallen = Rgb(72, 62, 52); // (what lies where a boarded wall came down: its boards, broken and burnt)
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
				// (a shed on trial: the ground along this wall, and a door's sill on it)
				auto Gd = [&](double A) {
					const double T = std::clamp(A / Fc.Len, 0.0, 1.0) * 17.0;
					const int K = std::min(16, static_cast<int>(T));
					return Lerp(H.Lie[F][K], H.Lie[F][K + 1], T - K);
				};
				auto Sill = [&](double A0, double A1) { return std::max(Gd(A0), Gd(A1)) + 0.05; };
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
				else if (H.bBoards && H.Type == HtBarn)
				{
					// (the trial "sheds") The barn: its cart doors in the front; a door near one end of the back; hatches
					// under the eaves for its air, every five metres or so; in each gable the loft's pitching door, and in
					// one a pair of doors under it. Its doors' sills on the ground where they stand.
					if (!Fc.bGable)
					{
						if (bFront)
						{
							const double Z = Sill(0.5 * Fc.Len - 1.7, 0.5 * Fc.Len + 1.7);
							Ops.push_back({ 0.5 * Fc.Len - 1.7, 0.5 * Fc.Len + 1.7, Z, Z + 3.7, 4 });
						}
						else
						{
							const double A = Fc.Len * (Unit(Hash(H.Key, 0x0BE5ull)) < 0.5 ? 0.22 : 0.78), Z = Sill(A - 0.55, A + 0.55);
							Ops.push_back({ A - 0.55, A + 0.55, Z, Z + 2.1, 1 });
						}
						const int N = std::max(2, static_cast<int>(Fc.Len / 5.0));
						for (int K = 0; K < N; ++K)
						{
							const double A = Fc.Len * (K + 0.5) / N;
							if (bFront && std::fabs(A - 0.5 * Fc.Len) < 2.6) continue;
							Ops.push_back({ A - 0.35, A + 0.35, Eaves - 1.7, Eaves - 1.1, 2 });
						}
					}
					else
					{
						if (Fc.End == (Unit(Hash(H.Key, 0x0BE6ull)) < 0.5 ? -1 : 1))
						{
							const double Z = Sill(0.5 * Fc.Len - 1.3, 0.5 * Fc.Len + 1.3);
							Ops.push_back({ 0.5 * Fc.Len - 1.3, 0.5 * Fc.Len + 1.3, Z, Z + 2.9, 4 });
						}
						Ops.push_back({ 0.5 * Fc.Len - 0.5, 0.5 * Fc.Len + 0.5, Eaves - 1.5, Eaves - 0.5, 2 });
					}
				}
				else if (H.bBoards && H.Type == HtByre)
				{
					// The byre: its two doors in the front, small windows under the eaves along both sides, a pair of
					// doors in each end.
					if (!Fc.bGable)
					{
						double Doors[2] = { -1e9, -1e9 };
						if (bFront)
							for (int K = 0; K < 2; ++K)
							{
								Doors[K] = Fc.Len * (K == 0 ? 0.3 : 0.72);
								const double Z = Sill(Doors[K] - 0.6, Doors[K] + 0.6);
								Ops.push_back({ Doors[K] - 0.6, Doors[K] + 0.6, Z, Z + 2.05, 1 });
							}
						const int N = std::max(3, static_cast<int>(Fc.Len / 3.8));
						for (int K = 0; K < N; ++K)
						{
							const double A = Fc.Len * (K + 0.5) / N;
							if (std::fabs(A - Doors[0]) < 1.3 || std::fabs(A - Doors[1]) < 1.3) continue;
							Ops.push_back({ A - 0.32, A + 0.32, Eaves - 1.75, Eaves - 1.2, 0 });
						}
					}
					else
					{
						const double Z = Sill(0.5 * Fc.Len - 1.1, 0.5 * Fc.Len + 1.1);
						Ops.push_back({ 0.5 * Fc.Len - 1.1, 0.5 * Fc.Len + 1.1, Z, Z + 2.4, 4 });
					}
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
				if (H.bDaub && !Fc.bGable && Fc.Len > 4.0)
				{
					// (the trial "sheds") Where the daub has fallen from the wattle, low on a long wall of a clay shed: one to
					// three places, clear of its doors and of each other.
					const uint64_t WK = Hash(H.Key, 0x3A77ull, static_cast<uint64_t>(F));
					const int N = 1 + static_cast<int>(Unit(WK) * 2.99);
					for (int K = 0; K < N; ++K)
					{
						const uint64_t PK = Hash(WK, static_cast<uint64_t>(K));
						const double Wd = 0.9 + 0.5 * Unit(PK), A = 1.0 + std::max(0.0, Fc.Len - 2.0 - Wd) * Unit(Mix(PK ^ 1)), Hi = 0.55 + 0.35 * Unit(Mix(PK ^ 2));
						const double Z0 = std::max(Gd(A), Gd(A + Wd)) + 0.48 + 0.3 * Unit(Mix(PK ^ 3));
						bool bFree = A + Wd < Fc.Len - 0.8;
						for (const Opening& O : Ops) bFree = bFree && (A + Wd < O.A0 - 0.45 || A > O.A1 + 0.45 || Z0 + Hi < O.Z0 - 0.3);
						if (bFree) Ops.push_back({ A, A + Wd, Z0, Z0 + Hi, 5 });
					}
				}
				// An opening the broken wall's top runs through is gone with it.
				Ops.erase(std::remove_if(Ops.begin(), Ops.end(), [&](const Opening& O) { return std::min(Top(O.A0), Top(O.A1)) < O.Z1 + 0.25; }), Ops.end());
				const V3 Out3{ Fc.Out.X, Fc.Out.Y, 0.0 };
				if (H.bBoards)
				{
					// (the trial "sheds") A shed's face, its own way: the plinth, its top following the ground along the
					// wall; over it the boards, each a strip from the plinth to the wall's top (dark down the edge its
					// neighbour laps, damp and green-black at the foot, ragged board by board, browner under the eaves), or
					// the washed clay in cells; round its openings; its sill beam and corner boards; its doors of planks on
					// their ledges, brace and strap hinges; the gable boarded upright to the rake over a weather board.
					const double Lap = Sd.Lap;
					const uint64_t FK = Hash(H.Key, 0x5ED6ull, static_cast<uint64_t>(F));
					const V3 Along3{ U.X * Fc.DU + V.X * Fc.DV, U.Y * Fc.DU + V.Y * Fc.DV, 0.0 };
					const Col Scorch = Rgb(52, 46, 42);
					// (a piece of wall whose top comes down to its foot at one end is a triangle, at both nothing: no flat sliver
					// of a triangle is laid)
					auto Shut = [&](V3 P0, V3 P1, V3 P2, V3 P3, const Col& C0, const Col& C1, const Col& C2, const Col& C3, V3 Toward) {
						const bool b0 = P3.Z - P0.Z < 1e-3, b1 = P2.Z - P1.Z < 1e-3;
						if (b0 && b1) return;
						if (!b0 && !b1)
						{
							B.Quad(P0, P1, P2, P3, C0, C1, C2, C3, Toward);
							return;
						}
						const V3 PT = b0 ? P2 : P3;
						V3 N = Cross(P1 - P0, PT - P0);
						if (Dot3(N, N) < 1e-14) return;
						N = Unit3(N);
						const bool bTurn = Dot3(N, Toward) < 0.0;
						if (bTurn) N = N * -1.0;
						const uint32_t I0 = B.Vert(P0, N, C0), I1 = B.Vert(P1, N, C1), I2 = B.Vert(PT, N, b0 ? C2 : C3);
						if (bTurn) B.Tri(I0, I2, I1);
						else B.Tri(I0, I1, I2);
					};
					// (sooted under the roof where it was hit, as the other houses; charred down from a top that is broken or burnt
					// away, Tz the wall's top over the point)
					auto Burnt = [&](const Col& C, V3 P, double Tz) {
						const Col Out = bInside ? Mixed(C, Scorch, 0.5 * Smooth(Eaves - 2.5, Eaves + 0.5, P.Z + 1.2 * Noise(P.X + P.Y, 1.0, H.Key)) * (bShell ? 1.0 : 0.6)) : C;
						if (Tz >= Eaves - 0.05) return Out;
						// (black in its last 18 cm under the break, the soot gone by 70 cm under it: below that the wall its own tone, so
						// the ragged edge reads against the dark inside)
						const double Dn = Tz - P.Z;
						return Mixed(Out, Rgb(34, 30, 28), Dn < 0.18 ? Lerp(0.85, 0.45, Clamp01(Dn / 0.18)) : 0.45 * Smooth(0.7, 0.18, Dn));
					};
					// (each paint by the point and the ground under it, Gz)
					auto PlinthAt = [&](V3 P, double Gz, double Tz) {
						const double Mot = Noise((P.X + P.Y) / 0.45, P.Z / 0.3, H.Key ^ 0x71);
						return Burnt(Mixed(Varied(Sd.Plinth, 1.0 + 0.14 * Mot, 0.03 * Mot), Rgb(44, 44, 34), Smooth(Gz + 0.3, Gz - 0.4, P.Z)), P, Tz);
					};
					// (how high over the ground a board is rotted black at its foot, 15 to 50 cm up from the plinth, each its own,
					// so the line is ragged; and how high the damp fades out over that)
					auto RotOf = [&](uint64_t BK) { return 0.55 + 0.35 * Unit(Mix(BK ^ 0x9B)); };
					auto DampOf = [&](uint64_t BK) { return RotOf(BK) + 0.3 + 0.6 * Unit(Mix(BK ^ 0x99)) * Unit(Mix(BK ^ 0x9A)); };
					auto BoardAt = [&](uint64_t BK, int Side, V3 P, double Gz, double Tz) {
						const double U1 = Unit(BK), U2 = Unit(Mix(BK ^ 0x55));
						Col C = Mixed(Sd.Board, Sd.BoardB, U1);
						if (U2 < 0.07) C = Mixed(C, Rgb(168, 148, 110), 0.6); // (a new board among the old)
						else if (U2 > 0.92) C = Scaled(C, 0.83); // (an old dark board: with the spread 0.76 to 0.90 of its neighbours)
						C = Scaled(C, (0.92 + 0.16 * Unit(Mix(BK ^ 0x77))) * (Side == 0 ? 0.74 : 1.0));
						const double Hg = P.Z - Gz, Rot = RotOf(BK), Damp = DampOf(BK);
						C = Mixed(C, Rgb(46, 46, 36), Hg < Rot + 0.001 ? Lerp(0.9, 0.72, Clamp01((Hg - 0.4) / (Rot - 0.4))) : 0.72 * Clamp01((Damp - Hg) / (Damp - Rot)));
						C = Mixed(C, Rgb(92, 78, 62), 0.3 * Smooth(Eaves - 1.0, Eaves, P.Z));
						return Burnt(C, P, Tz);
					};
					auto WashAt = [&](double A, V3 P, double Gz, double Tz) {
						const double Z = P.Z - Gz, AA = A + 37.0 * F;
						const double Mot = Noise(AA / 0.9, P.Z / 0.55, H.Key ^ 0x51);
						Col C = Varied(Sd.Wash, 1.0 + 0.05 * Mot, 0.015 * Mot);
						// (the rain's grey streaks under the eaves; the wash come away to the clay in patches, most where the
						// damp lifts it low down; damp and splash at the foot)
						C = Mixed(C, Rgb(146, 144, 134), 0.35 * Smooth(0.1, 0.7, Noise(AA / 0.55, P.Z / 7.0, H.Key ^ 0x52)) * Smooth(Eaves - 2.4, Eaves - 0.2, P.Z));
						// (round a place where the daub has fallen the wash is gone with it: bare clay two hands out from the break)
						double Bare = 0.0;
						for (const Opening& O : Ops)
							if (O.What == 5) Bare = std::max(Bare, Smooth(0.45, 0.0, std::hypot(std::max({ O.A0 - A, A - O.A1, 0.0 }), std::max({ O.Z0 - P.Z, P.Z - O.Z1, 0.0 }))));
						const double Fl = Noise(AA / 1.2, P.Z / 0.8, H.Key ^ 0x53) + 0.85 * Smooth(1.6, 0.45, Z) + 1.4 * Bare;
						C = Mixed(C, Sd.Clay, 0.85 * Smooth(0.42, 0.8, Fl));
						C = Mixed(C, Rgb(88, 84, 66), 0.6 * Smooth(1.0, 0.4, Z));
						return Burnt(C, P, Tz);
					};
					// A strip proud of the wall (a beam, a board, a door's post): its foot and its top at each end, its lips
					// back to the wall where asked.
					auto Rail = [&](double A0, double A1, double Z00, double Z01, double Z10, double Z11, double Proud, const Col& C, bool bLips) {
						const V3 P0 = At(A0, Z00, -Proud), P1 = At(A1, Z01, -Proud), P2 = At(A1, Z11, -Proud), P3 = At(A0, Z10, -Proud);
						B.Quad1(P0, P1, P2, P3, C, Out3);
						if (!bLips) return;
						B.Quad1(P3, P2, At(A1, Z11, 0.0), At(A0, Z10, 0.0), Scaled(C, 1.1), { 0, 0, 1 });
						B.Quad1(P0, P1, At(A1, Z01, 0.0), At(A0, Z00, 0.0), Scaled(C, 0.7), { 0, 0, -1 });
						B.Quad1(P0, P3, At(A0, Z10, 0.0), At(A0, Z00, 0.0), Scaled(C, 0.85), Along3 * -1.0);
						B.Quad1(P1, P2, At(A1, Z11, 0.0), At(A1, Z01, 0.0), Scaled(C, 0.85), Along3);
					};
					auto Level = [&](double A0, double A1, double Z0, double Z1, double Proud, const Col& C, bool bLips) { Rail(A0, A1, Z0, Z0, Z1, Z1, Proud, C, bLips); };
					// Its columns: boards of 18 to 30 cm, or the clay in cells of 75 cm; the openings' edges among them.
					std::vector<double> Cuts{ 0.0, Fc.Len };
					if (Sd.bPlank)
					{
						uint64_t N = 0;
						for (double A = 0.18 + 0.12 * Unit(Hash(FK, N)); A < Fc.Len - 0.1; A += 0.18 + 0.12 * Unit(Hash(FK, ++N))) Cuts.push_back(A);
					}
					else
						for (double A = 0.75; A < Fc.Len - 0.3; A += 0.75) Cuts.push_back(A);
					// (a board's or a cell's edge within 2 cm of an opening's gives way to it, never the opening's to the board's:
					// a column begun before its opening's edge would count as opening, and leave the wall open beside the jamb)
					for (const Opening& O : Ops)
						Cuts.erase(std::remove_if(Cuts.begin() + 2, Cuts.end(), [&O](double X) { return std::fabs(X - O.A0) < 0.02 || std::fabs(X - O.A1) < 0.02; }), Cuts.end());
					for (const Opening& O : Ops)
					{
						Cuts.push_back(O.A0);
						Cuts.push_back(O.A1);
					}
					std::sort(Cuts.begin(), Cuts.end());
					Cuts.erase(std::unique(Cuts.begin(), Cuts.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.02; }), Cuts.end());
					// (where the wall's paint is told again going up it, over the ground: the foot's wear ends at the first)
					const double Rows[5] = { 1.0, 1.7, 2.6, 3.6, 4.6 };
					// (a column's top at each side. A clay wall breaks along the break's own line; a boarded one board by board, each
					// board's end square at its own height, up to 18 cm over or under that line)
					auto BoardTop = [&](size_t I) -> std::pair<double, double> {
						const double T0 = std::min(Eaves, Top(Cuts[I])), T1 = std::min(Eaves, Top(Cuts[I + 1]));
						if (!Sd.bPlank || (T0 >= Eaves - 0.05 && T1 >= Eaves - 0.05)) return { T0, T1 };
						const double T = std::min(Eaves, 0.5 * (T0 + T1) + 0.36 * (Unit(Mix(Hash(FK, 0xB0A2ull, static_cast<uint64_t>(I)) ^ 0x7B)) - 0.5));
						return { T, T };
					};
					for (size_t I = 0; I + 1 < Cuts.size(); ++I)
					{
						const double A0 = Cuts[I], A1 = Cuts[I + 1], AM = 0.5 * (A0 + A1), G0 = Gd(A0), G1 = Gd(A1);
						const std::pair<double, double> TT = BoardTop(I);
						const double T0 = TT.first, T1 = TT.second;
						const uint64_t BK = Hash(FK, 0xB0A2ull, static_cast<uint64_t>(I));
						double DoorSill = 1e9;
						std::vector<std::pair<double, double>> Gaps;
						for (const Opening& O : Ops)
							if (AM > O.A0 && AM < O.A1)
							{
								Gaps.push_back({ O.Z0, O.Z1 });
								if (O.What == 1 || O.What == 4) DoorSill = O.Z0;
							}
						std::sort(Gaps.begin(), Gaps.end());
						const bool bDoor = DoorSill < 1e8;
						// The plinth (a clay shed's stands out of its wall, its top weathered; under a door, up to its sill).
						{
							const double Q0 = std::min(bDoor ? DoorSill : G0 + 0.4, T0), Q1 = std::min(bDoor ? DoorSill : G1 + 0.4, T1), In = Sd.bPlank ? 0.0 : -0.06;
							const double E0 = !Sd.bPlank && I == 0 ? -0.06 : A0, E1 = !Sd.bPlank && I + 2 == Cuts.size() ? Fc.Len + 0.06 : A1;
							// (its foot half a metre under the ground along the wall, no deeper: its damp shows in what stands out)
							const double U0 = std::max(H.Foot, G0 - 0.5), U1 = std::max(H.Foot, G1 - 0.5);
							if (Q0 > U0 + 0.01 || Q1 > U1 + 0.01)
							{
								const V3 P0 = At(E0, U0, In), P1 = At(E1, U1, In), P2 = At(E1, std::max(U1, Q1), In), P3 = At(E0, std::max(U0, Q0), In);
								Shut(P0, P1, P2, P3, PlinthAt(P0, G0, T0), PlinthAt(P1, G1, T1), PlinthAt(P2, G1, T1), PlinthAt(P3, G0, T0), Out3);
								// (its weathered top up to the wall's foot: level under a door, and where the wall is broken down into it)
								const double W0 = bDoor || Q0 < G0 + 0.399 ? 0.0 : 0.05, W1 = bDoor || Q1 < G1 + 0.399 ? 0.0 : 0.05;
								if (!Sd.bPlank) B.Quad1(P3, P2, At(E1, P2.Z + W1, 0.0), At(E0, P3.Z + W0, 0.0), Scaled(Sd.Plinth, 1.1), { 0, 0, 1 });
							}
						}
						// The wall over it, between its openings: a piece's foot and top at each side.
						auto Piece = [&](double Z00, double Z01, double Z10, double Z11) {
							if (Z10 <= Z00 + 0.01 && Z11 <= Z01 + 0.01) return;
							const V3 P0 = At(A0, Z00, -Lap), P1 = At(A1, Z01, -Lap), P2 = At(A1, std::max(Z01, Z11), -Lap), P3 = At(A0, std::max(Z00, Z10), -Lap);
							if (Sd.bPlank)
							{
								// (a board flat across its width, its own tone; the edge its neighbour laps a hard dark line, its first 2 cm)
								const double AL = std::min(A0 + 0.02, 0.5 * (A0 + A1)), UL = (AL - A0) / (A1 - A0), GL = Lerp(G0, G1, UL), TL = Lerp(T0, T1, UL);
								const V3 PL0 = At(AL, Lerp(P0.Z, P1.Z, UL), -Lap), PL1 = At(AL, Lerp(P3.Z, P2.Z, UL), -Lap);
								Shut(P0, PL0, PL1, P3, BoardAt(BK, 0, P0, G0, T0), BoardAt(BK, 0, PL0, GL, TL), BoardAt(BK, 0, PL1, GL, TL), BoardAt(BK, 0, P3, G0, T0), Out3);
								Shut(PL0, P1, P2, PL1, BoardAt(BK, 1, PL0, GL, TL), BoardAt(BK, 1, P1, G1, T1), BoardAt(BK, 1, P2, G1, T1), BoardAt(BK, 1, PL1, GL, TL), Out3);
							}
							else Shut(P0, P1, P2, P3, WashAt(A0, P0, G0, T0), WashAt(A1, P1, G1, T1), WashAt(A1, P2, G1, T1), WashAt(A0, P3, G0, T0), Out3);
						};
						// (where its paint is told again going up it: the foot's wear and its damp over the ground, or the clay's rows;
						// under a broken top the soot's foot and the char's, 70 and 18 cm under the break)
						auto Span = [&](double Za0, double Za1, double Zb0, double Zb1, bool bTop) {
							std::pair<double, double> Lines[7];
							int NL = 0;
							for (int K = 0; K < (Sd.bPlank ? 2 : 5); ++K)
							{
								const double Off = !Sd.bPlank ? Rows[K] : K == 0 ? RotOf(BK) : DampOf(BK);
								Lines[NL++] = { G0 + Off, G1 + Off };
							}
							if (bTop && (T0 < Eaves - 0.05 || T1 < Eaves - 0.05))
							{
								Lines[NL++] = { Zb0 - 0.7, Zb1 - 0.7 };
								Lines[NL++] = { Zb0 - 0.18, Zb1 - 0.18 };
							}
							std::sort(Lines, Lines + NL, [](const std::pair<double, double>& X, const std::pair<double, double>& Y) { return X.first + X.second < Y.first + Y.second; });
							double L0 = Za0, L1 = Za1;
							for (int K = 0; K < NL; ++K)
							{
								const double S0 = Lines[K].first, S1 = Lines[K].second;
								if (S0 < L0 + 0.05 || S1 < L1 + 0.05 || S0 > Zb0 - 0.05 || S1 > Zb1 - 0.05) continue;
								Piece(L0, L1, S0, S1);
								L0 = S0;
								L1 = S1;
							}
							Piece(L0, L1, Zb0, Zb1);
						};
						// (its foot on the plinth's top, down with it where the wall is broken lower; its top under the roof: a long
						// wall's boards stand out of the wall's line, where the roof's slope is that much lower)
						double F0 = std::min(G0 + 0.4, T0), F1 = std::min(G1 + 0.4, T1);
						const double Cap = !Fc.bGable && bRoof ? Eaves - (Lap + 0.01) * Tan : Eaves, TB0 = std::min(T0, Cap), TB1 = std::min(T1, Cap);
						for (const std::pair<double, double>& G : Gaps)
						{
							if (G.first > std::max(F0, F1) + 0.01) Span(F0, F1, std::min(G.first, TB0), std::min(G.first, TB1), false);
							F0 = std::max(F0, G.second);
							F1 = std::max(F1, G.second);
						}
						Span(F0, F1, TB0, TB1, true);
						if (bInside)
						{
							// (from the wall's foot, as the other houses: a wall broken below the floor has its inside and its charred top there)
							const V3 Q2 = At(A1, std::max(H.Foot, T1), Thick), Q3 = At(A0, std::max(H.Foot, T0), Thick);
							Shut(At(A0, H.Foot, Thick), At(A1, H.Foot, Thick), Q2, Q3, Sd.Within, Sd.Within, Sd.Within, Sd.Within, Out3 * -1.0);
							if (T0 < Eaves - 0.01 || T1 < Eaves - 0.01) B.Quad1(At(A0, std::max(H.Foot, TB0), -Lap), At(A1, std::max(H.Foot, TB1), -Lap), Q2, Q3, Mixed(Sd.bPlank ? Sd.BoardB : Sd.Clay, Rgb(34, 30, 28), Sd.bPlank ? 0.85 : 0.4), { 0, 0, 1 });
							// (boards broken at their own heights: the taller one's side stands open over its neighbour's end, charred)
							if (Sd.bPlank && I + 2 < Cuts.size())
							{
								const double TN = BoardTop(I + 1).first, ZLo = std::max(H.Foot, std::min(T1, TN)), ZHi = std::max(H.Foot, std::max(T1, TN));
								if (ZHi > ZLo + 0.01) B.Quad1(At(A1, ZLo, -Lap), At(A1, ZLo, Thick), At(A1, ZHi, Thick), At(A1, ZHi, -Lap), Mixed(Sd.BoardB, Rgb(34, 30, 28), 0.8), Along3 * (T1 > TN ? 1.0 : -1.0));
							}
						}
					}
					// A boarded shed's sill beam on its plinth (broken at its doors, gone where the wall is down to it) and
					// its corner boards.
					if (Sd.bPlank)
					{
						for (double A = -0.07; A < Fc.Len + 0.07; A += 1.5)
						{
							double A0 = A, A1 = std::min(Fc.Len + 0.07, A + 1.5);
							bool bClear = std::min(Eaves, Top(std::clamp(0.5 * (A0 + A1), 0.0, Fc.Len))) > Gd(0.5 * (A0 + A1)) + 0.7;
							for (const Opening& O : Ops)
								if (O.What == 1 || O.What == 4)
								{
									// (a door: the beam stops at its post, either side)
									if (A0 > O.A0 - 0.15 && A1 < O.A1 + 0.15) bClear = false;
									else if (A0 < O.A0 - 0.15 && A1 > O.A0 - 0.15 && A1 < O.A1 + 0.15) A1 = O.A0 - 0.15;
									else if (A1 > O.A1 + 0.15 && A0 < O.A1 + 0.15 && A0 > O.A0 - 0.15) A0 = O.A1 + 0.15;
									else if (A0 <= O.A0 - 0.15 && A1 >= O.A1 + 0.15)
									{
										// (a piece over the whole door: its near part here, its far part goes on)
										const double AD = O.A0 - 0.15;
										if (bClear && AD > A0 + 0.02) Rail(A0, AD, Gd(A0) + 0.38, Gd(AD) + 0.38, Gd(A0) + 0.5, Gd(AD) + 0.5, 0.075, Mixed(Sd.Timber, Rgb(46, 46, 36), 0.35), true);
										A0 = O.A1 + 0.15;
									}
								}
							if (bClear && A1 > A0 + 0.02) Rail(A0, A1, Gd(A0) + 0.38, Gd(A1) + 0.38, Gd(A0) + 0.5, Gd(A1) + 0.5, 0.075, Mixed(Sd.Timber, Rgb(46, 46, 36), 0.35), true);
						}
						for (int E = 0; E < 2; ++E)
						{
							// (7 cm proud of the wall, its top that much lower down the roof's slope, or it stands up through the roof)
							const double AE = E == 0 ? 0.0 : Fc.Len, TopE = std::min(bRoof ? Eaves - 0.08 * Tan : Eaves, Top(AE));
							if (TopE > Gd(AE) + 0.9) Level(E == 0 ? -0.07 : Fc.Len - 0.15, E == 0 ? 0.15 : Fc.Len + 0.07, Gd(AE) + 0.5, TopE, 0.07, Scaled(Sd.Timber, 1.12), true);
						}
					}
					// A clay barn's frame showing in its walls: a post at each corner and every two metres or so, broken at what
					// opens in its way and gone at a door; along a long wall the plate under the eaves. The daub between.
					if (Sd.bFrame)
					{
						const double Pr = 0.02, CapZ = !Fc.bGable && bRoof ? Eaves - (Sd.bThatch ? 0.3 : 0.0) - (Pr + 0.01) * Tan : Eaves; // (a thatch is 30 cm thick: the plate under it)
						const int NP = std::max(1, static_cast<int>(std::lround(Fc.Len / 2.2)));
						const Col Post = Mixed(Sd.Timber, Rgb(60, 54, 46), 0.25);
						for (int K = 0; K <= NP; ++K)
						{
							const double AC = Fc.Len * K / NP, PA0 = K == 0 ? -Pr : K == NP ? Fc.Len - 0.16 : AC - 0.08, PA1 = K == 0 ? 0.16 : K == NP ? Fc.Len + Pr : AC + 0.08;
							// (the post's top no higher than the wall's drawn top under either edge of it, or at a cut between them: a
							// broken clay wall's top runs straight from cut to cut)
							auto Drawn = [&](double Ap) {
								Ap = std::clamp(Ap, 0.0, Fc.Len);
								size_t Ic = static_cast<size_t>(std::upper_bound(Cuts.begin(), Cuts.end(), Ap) - Cuts.begin());
								Ic = std::clamp<size_t>(Ic, 1, Cuts.size() - 1) - 1;
								const std::pair<double, double> Tc = BoardTop(Ic);
								return Lerp(Tc.first, Tc.second, (Ap - Cuts[Ic]) / std::max(1e-6, Cuts[Ic + 1] - Cuts[Ic]));
							};
							double TopP = std::min({ CapZ, Drawn(PA0), Drawn(PA1) });
							for (const double Ct : Cuts)
								if (Ct > PA0 && Ct < PA1) TopP = std::min(TopP, std::min(Eaves, Top(Ct)));
							bool bDoorHere = false;
							std::vector<std::pair<double, double>> Gp;
							for (const Opening& O : Ops)
								if (PA1 > O.A0 - 0.13 && PA0 < O.A1 + 0.13)
								{
									if (O.What == 1 || O.What == 4) bDoorHere = true;
									else Gp.push_back({ O.Z0 - 0.1, O.Z1 + 0.18 });
								}
							if (!bDoorHere)
							{
								std::sort(Gp.begin(), Gp.end());
								const Col PC = Scaled(Post, 0.92 + 0.16 * Unit(Hash(FK, 0xF0A7ull, static_cast<uint64_t>(K))));
								double Z = Gd(AC) + 0.4;
								for (const std::pair<double, double>& G : Gp)
								{
									if (std::min(G.first, TopP) > Z + 0.1) Level(PA0, PA1, Z, std::min(G.first, TopP), Pr, PC, false);
									Z = std::max(Z, G.second);
								}
								if (TopP > Z + 0.1) Level(PA0, PA1, Z, TopP, Pr, PC, false);
							}
							// (the plate over the bay that begins here, where the wall stands whole to its eaves)
							if (!Fc.bGable && K < NP && Top(AC) >= Eaves - 0.05 && Top(Fc.Len * (K + 1) / NP) >= Eaves - 0.05) Level(AC, Fc.Len * (K + 1) / NP, CapZ - 0.16, CapZ, Pr, Scaled(Post, 0.95), false);
						}
					}
					// Its openings: the reveal and the frame round it; a window's glass and bars; a door's or a hatch's leaves.
					for (const Opening& O : Ops)
					{
						if (O.What == 5)
						{
							// The daub fallen from the wattle: the clay broken off in a ragged line round the place, its raw edge a lip
							// back to the rods 4 cm in; the rods thin and level, woven round upright stakes, each before one stake and
							// behind the next, the row over it the other way; weathered grey-brown, clay still in the weave.
							const double D5 = 0.04, Wide5 = O.A1 - O.A0, High5 = O.Z1 - O.Z0;
							const uint64_t WKey = Hash(FK, 0x3A78ull, static_cast<uint64_t>(std::llround(O.A0 * 16.0)));
							const Col Rod = Rgb(188, 165, 130);
							const int NS = std::max(2, static_cast<int>(std::lround(Wide5 / 0.35))), NB = std::max(6, static_cast<int>(std::lround(High5 / 0.05)));
							for (int Bn = 0; Bn < NB; ++Bn)
								for (int Sg = 0; Sg < NS; ++Sg)
								{
									const double Wa0 = Lerp(O.A0, O.A1, static_cast<double>(Sg) / NS), Wa1 = Lerp(O.A0, O.A1, static_cast<double>(Sg + 1) / NS);
									const double Wz0 = Lerp(O.Z0, O.Z1, static_cast<double>(Bn) / NB), Wz1 = Lerp(O.Z0, O.Z1, static_cast<double>(Bn + 1) / NB);
									const uint64_t RK = Hash(WKey, static_cast<uint64_t>(Bn * 16 + Sg));
									Col Wy = Scaled(Rod, ((Bn + Sg) % 2 == 0 ? 1.0 : 0.88) * (0.92 + 0.16 * Unit(RK)));
									if (Unit(Mix(RK ^ 0x21)) < 0.3) Wy = Mixed(Wy, Sd.Clay, 0.6); // (clay still on it)
									const Col Wd = Scaled(Wy, 0.85);
									B.Quad(At(Wa0, Wz0, D5), At(Wa1, Wz0, D5), At(Wa1, Wz1, D5), At(Wa0, Wz1, D5), Wd, Wd, Wy, Wy, Out3);
								}
							for (int Sg = 1; Sg < NS; ++Sg)
							{
								const double Sa = Lerp(O.A0, O.A1, static_cast<double>(Sg) / NS);
								B.Quad1(At(Sa - 0.014, O.Z0, D5 - 0.012), At(Sa + 0.014, O.Z0, D5 - 0.012), At(Sa + 0.014, O.Z1, D5 - 0.012), At(Sa - 0.014, O.Z1, D5 - 0.012), Scaled(Rod, 0.78), Out3);
							}
							// (the break's edge: round the place a point each 20 cm or so, the clay standing in from it 2 to 12 cm, each
							// its own; from there the lip, 3 cm further in and back to the rods)
							struct Rim
							{
								double A, Z, DA, DZ;
							};
							std::vector<Rim> Round;
							const int NA5 = std::max(3, static_cast<int>(std::lround(Wide5 / 0.2))), NZ5 = std::max(2, static_cast<int>(std::lround(High5 / 0.2)));
							for (int K = 0; K < NA5; ++K) Round.push_back({ Lerp(O.A0, O.A1, static_cast<double>(K) / NA5), O.Z0, K == 0 ? 1.0 : 0.0, 1.0 });
							for (int K = 0; K < NZ5; ++K) Round.push_back({ O.A1, Lerp(O.Z0, O.Z1, static_cast<double>(K) / NZ5), -1.0, K == 0 ? 1.0 : 0.0 });
							for (int K = 0; K < NA5; ++K) Round.push_back({ Lerp(O.A1, O.A0, static_cast<double>(K) / NA5), O.Z1, K == 0 ? -1.0 : 0.0, -1.0 });
							for (int K = 0; K < NZ5; ++K) Round.push_back({ O.A0, Lerp(O.Z1, O.Z0, static_cast<double>(K) / NZ5), 1.0, K == 0 ? -1.0 : 0.0 });
							auto Clay5 = [&](double A, double Z) { return WashAt(A, At(A, Z, -Lap), Gd(A), std::min(Eaves, Top(A))); };
							for (size_t K = 0; K < Round.size(); ++K)
							{
								const Rim& R0 = Round[K];
								const Rim& R1 = Round[(K + 1) % Round.size()];
								auto InOf = [&](size_t Kk) { return (Round[Kk].DA != 0.0 && Round[Kk].DZ != 0.0 ? 0.07 : 0.02) + 0.11 * Unit(Hash(WKey, 0x200ull + Kk)); };
								auto DeepOf = [](const Rim& Rm) { return Rm.DZ < 0.0 ? 0.035 : 0.03; };
								const double In0 = InOf(K), In1 = InOf((K + 1) % Round.size());
								const double QA0 = R0.A + R0.DA * In0, QZ0 = R0.Z + R0.DZ * In0, QA1 = R1.A + R1.DA * In1, QZ1 = R1.Z + R1.DZ * In1;
								B.Quad(At(R0.A, R0.Z, -Lap), At(R1.A, R1.Z, -Lap), At(QA1, QZ1, -Lap), At(QA0, QZ0, -Lap), Clay5(R0.A, R0.Z), Clay5(R1.A, R1.Z), Scaled(Clay5(QA1, QZ1), 0.93), Scaled(Clay5(QA0, QZ0), 0.93), Out3);
								// (the lip: lit where it looks up, in its own shade where it looks down)
								const Col Lip = Scaled(Sd.Clay, 0.95 + 0.025 * (R0.DZ + R1.DZ));
								B.Quad1(At(QA0, QZ0, -Lap), At(QA1, QZ1, -Lap), At(QA1 + R1.DA * DeepOf(R1), QZ1 + R1.DZ * DeepOf(R1), D5), At(QA0 + R0.DA * DeepOf(R0), QZ0 + R0.DZ * DeepOf(R0), D5), Lip, Out3);
							}
							continue;
						}
						const bool bLeaf = O.What != 0, bDoor = O.What == 1 || O.What == 4;
						const double D = bLeaf ? 0.07 : 0.16;
						const V3 A0 = At(O.A0, O.Z0, -Lap), A1 = At(O.A1, O.Z0, -Lap), A2 = At(O.A1, O.Z1, -Lap), A3 = At(O.A0, O.Z1, -Lap);
						// (the reveal runs back to the dark behind the leaves, 2 cm past them: no crack round a door to see through)
						const double DB = bLeaf ? D + 0.02 : D;
						const V3 B0 = At(O.A0, O.Z0, DB), B1 = At(O.A1, O.Z0, DB), B2 = At(O.A1, O.Z1, DB), B3 = At(O.A0, O.Z1, DB);
						B.Quad1(A0, B0, B3, A3, Sd.Timber, Along3);
						B.Quad1(A1, B1, B2, A2, Sd.Timber, Along3 * -1.0);
						B.Quad1(A3, A2, B2, B3, Scaled(Sd.Timber, 0.75), { 0, 0, -1 });
						B.Quad1(A0, A1, B1, B0, Scaled(Sd.Timber, 1.1), { 0, 0, 1 });
						const double TW = bLeaf ? 0.11 : 0.07, TP = Lap + (bLeaf ? 0.035 : 0.02);
						const Col Trim = bLeaf ? Sd.Timber : Mixed(Sd.Timber, L.Frame, 0.45);
						Level(O.A0 - TW, O.A0, O.Z0, O.Z1, TP, Trim, false);
						Level(O.A1, O.A1 + TW, O.Z0, O.Z1, TP, Trim, false);
						Level(O.A0 - TW - 0.04, O.A1 + TW + 0.04, O.Z1, O.Z1 + (bLeaf ? 0.15 : 0.07), TP + 0.015, Scaled(Trim, 0.94), bLeaf);
						if (!bDoor) Level(O.A0 - TW - 0.03, O.A1 + TW + 0.03, O.Z0 - 0.06, O.Z0, TP + 0.03, Scaled(Trim, 1.06), true);
						// (a clay shed's plinth stands out of its wall: its ends closed at a door's jambs)
						if (bDoor && !Sd.bPlank)
							for (int E = 0; E < 2; ++E)
							{
								const double AJ = E == 0 ? O.A0 : O.A1, ZJ = Gd(AJ) + 0.4;
								if (ZJ > O.Z0 + 0.01) B.Quad1(At(AJ, O.Z0, -0.06), At(AJ, ZJ, -0.06), At(AJ, ZJ + 0.05, 0.0), At(AJ, O.Z0, 0.0), Scaled(Sd.Plinth, 0.85), Along3 * (E == 0 ? 1.0 : -1.0));
							}
						if (bShell)
						{
							B.Quad1(B0, B1, B2, B3, Rgb(30, 28, 26), Out3);
							continue;
						}
						if (!bLeaf)
						{
							B.Quad1(B0, B1, B2, B3, L.Glass, Out3);
							const double AM = 0.5 * (O.A0 + O.A1), ZM = 0.5 * (O.Z0 + O.Z1), Pr = D - 0.02;
							B.Quad1(At(AM - 0.025, O.Z0, Pr), At(AM + 0.025, O.Z0, Pr), At(AM + 0.025, O.Z1, Pr), At(AM - 0.025, O.Z1, Pr), Trim, Out3);
							B.Quad1(At(O.A0, ZM - 0.025, Pr), At(O.A1, ZM - 0.025, Pr), At(O.A1, ZM + 0.025, Pr), At(O.A0, ZM + 0.025, Pr), Trim, Out3);
							continue;
						}
						// (the dark behind the leaves, seen between them)
						B.Quad1(B0, B1, B2, B3, Rgb(34, 30, 28), Out3);
						const int Leaves = O.What == 4 ? 2 : 1;
						const uint64_t OKey = Hash(FK, 0xD002ull, static_cast<uint64_t>(std::llround(O.A0 * 16.0) + 4096 * std::llround((O.Z1 - Floor) * 16.0)));
						for (int Lf = 0; Lf < Leaves; ++Lf)
						{
							const double LA0 = Lerp(O.A0, O.A1, static_cast<double>(Lf) / Leaves) + 0.015, LA1 = Lerp(O.A0, O.A1, static_cast<double>(Lf + 1) / Leaves) - 0.015;
							const double LZ0 = O.Z0 + (bDoor ? 0.04 : 0.015), LZ1 = O.Z1 - 0.015;
							const bool bHingeLow = Leaves == 2 ? Lf == 0 : Unit(OKey) < 0.5; // (its hinges on the side of the lesser A)
							const int NP = std::max(2, static_cast<int>(std::lround((LA1 - LA0) / 0.19)));
							for (int Pk = 0; Pk < NP; ++Pk)
							{
								const double PA0 = Lerp(LA0, LA1, static_cast<double>(Pk) / NP), PA1 = Lerp(LA0, LA1, static_cast<double>(Pk + 1) / NP);
								const uint64_t PK = Hash(OKey, static_cast<uint64_t>(Lf * 64 + Pk));
								const Col C = Scaled(Mixed(Sd.Leaf, Sd.Board, 0.35 * Unit(PK)), 0.88 + 0.22 * Unit(Mix(PK)));
								const Col Low = Mixed(C, Rgb(66, 62, 50), bDoor ? 0.45 : 0.12);
								// (a plank flat across its width, as the wall's boards; the joint a dark line, its first 12 mm)
								const double PL = std::min(PA0 + 0.012, 0.5 * (PA0 + PA1));
								const Col LowJ = Scaled(Low, 0.72), CJ = Scaled(C, 0.72);
								B.Quad(At(PA0, LZ0, D), At(PL, LZ0, D), At(PL, LZ1, D), At(PA0, LZ1, D), LowJ, LowJ, CJ, CJ, Out3);
								B.Quad(At(PL, LZ0, D), At(PA1, LZ0, D), At(PA1, LZ1, D), At(PL, LZ1, D), Low, Low, C, C, Out3);
							}
							// Its ledges, a strap hinge on the top and bottom ones, and the brace between each pair.
							const double Pr = D - 0.025;
							const Col Ledge = Scaled(Sd.Leaf, 0.8);
							const int NL = LZ1 - LZ0 > 1.5 ? 3 : 2;
							double ZL[3] = { 0.0, 0.0, 0.0 };
							for (int K = 0; K < NL; ++K)
							{
								ZL[K] = Lerp(LZ0, LZ1, NL == 3 ? (K == 0 ? 0.12 : K == 1 ? 0.5 : 0.88) : (K == 0 ? 0.2 : 0.8));
								B.Quad1(At(LA0, ZL[K] - 0.055, Pr), At(LA1, ZL[K] - 0.055, Pr), At(LA1, ZL[K] + 0.055, Pr), At(LA0, ZL[K] + 0.055, Pr), Ledge, Out3);
								if (NL == 3 && K == 1) continue;
								const double HA0 = bHingeLow ? LA0 : LA1 - 0.45 * (LA1 - LA0), HA1 = bHingeLow ? LA0 + 0.45 * (LA1 - LA0) : LA1;
								B.Quad1(At(HA0, ZL[K] - 0.022, Pr - 0.012), At(HA1, ZL[K] - 0.022, Pr - 0.012), At(HA1, ZL[K] + 0.022, Pr - 0.012), At(HA0, ZL[K] + 0.022, Pr - 0.012), Sd.Iron, Out3);
							}
							for (int K = 0; K + 1 < NL; ++K)
							{
								const double S = bHingeLow ? 1.0 : -1.0, BA0 = bHingeLow ? LA0 + 0.03 : LA1 - 0.03, BA1 = bHingeLow ? LA1 - 0.03 : LA0 + 0.03;
								B.Quad1(At(BA0, ZL[K] + 0.055, Pr), At(BA0 + S * 0.12, ZL[K] + 0.055, Pr), At(BA1, ZL[K + 1] - 0.055, Pr), At(BA1 - S * 0.12, ZL[K + 1] - 0.055, Pr), Ledge, Out3);
							}
						}
						// The beaten earth run up to a door's sill.
						if (bDoor)
						{
							const double Run = O.What == 4 ? 2.4 : 1.5, Wd = 0.35, Low = std::min(Gd(O.A0), Gd(O.A1)) - 0.35;
							const V3 R0 = At(O.A0 - 0.15, O.Z0 + 0.01, -Lap), R1 = At(O.A1 + 0.15, O.Z0 + 0.01, -Lap), R2 = At(O.A1 + 0.15 + Wd, Low, -Lap - Run), R3 = At(O.A0 - 0.15 - Wd, Low, -Lap - Run);
							const Col E0 = Rgb(122, 108, 84), E1 = Rgb(104, 94, 72);
							B.Quad(R0, R1, R2, R3, E0, E0, E1, E1, { 0, 0, 1 });
							B.Tri1(R0, R3, At(O.A0 - 0.15 - Wd, Low, -Lap), E1, Along3 * -1.0);
							B.Tri1(R1, R2, At(O.A1 + 0.15 + Wd, Low, -Lap), E1, Along3);
						}
					}
					// The gable boarded upright to the rake, where the roof still stands over it.
					if (Fc.bGable && bRoof && Broken != Fc.End)
					{
						auto Rake = [&](double A) { return Eaves + (Ridge - Eaves) * (1.0 - std::fabs(2.0 * A / Fc.Len - 1.0)); };
						const uint64_t GK = Hash(FK, 0x6AB1ull);
						std::vector<double> GB{ 0.0, 0.5 * Fc.Len, Fc.Len };
						uint64_t N = 0;
						// (a board's edge within 5 cm of the ridge's line gives way to it: the boarding runs up to the ridge)
						for (double A = 0.2 + 0.1 * Unit(Hash(GK, N)); A < Fc.Len - 0.1; A += 0.2 + 0.1 * Unit(Hash(GK, ++N)))
							if (std::fabs(A - 0.5 * Fc.Len) >= 0.05) GB.push_back(A);
						std::sort(GB.begin(), GB.end());
						GB.erase(std::unique(GB.begin(), GB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.05; }), GB.end());
						for (size_t I = 0; I + 1 < GB.size(); ++I)
						{
							const double A0 = GB[I], A1 = GB[I + 1], Gz = Eaves - 9.0, Tz = 1e9; // (no foot's damp up here, and its top whole)
							const uint64_t BK = Hash(GK, 0xB0A3ull, static_cast<uint64_t>(I));
							// (each board flat across its width, the edge its neighbour laps a hard dark line: its first 2 cm)
							const double AL = std::min(A0 + 0.02, 0.5 * (A0 + A1));
							for (int Side = 0; Side < 2; ++Side)
							{
								const double B0 = Side == 0 ? A0 : AL, B1 = Side == 0 ? AL : A1;
								const V3 P0 = At(B0, Eaves, -0.04), P1 = At(B1, Eaves, -0.04), P2 = At(B1, Rake(B1), -0.04), P3 = At(B0, Rake(B0), -0.04);
								Shut(P0, P1, P2, P3, BoardAt(BK, Side, P0, Gz, Tz), BoardAt(BK, Side, P1, Gz, Tz), BoardAt(BK, Side, P2, Gz, Tz), BoardAt(BK, Side, P3, Gz, Tz), Out3);
							}
						}
						// (the weather board from corner to corner, its ends cut to the rake: nothing of it stands up through the roof)
						const double AW = std::min(0.45 * Fc.Len, 0.05 * Fc.Len / (Ridge - Eaves));
						Rail(0.0, AW, Eaves - 0.07, Eaves - 0.07, Eaves - 0.01, Eaves + 0.09, 0.075, Sd.Timber, true);
						Level(AW, Fc.Len - AW, Eaves - 0.07, Eaves + 0.09, 0.075, Sd.Timber, true);
						Rail(Fc.Len - AW, Fc.Len, Eaves - 0.07, Eaves - 0.07, Eaves + 0.09, Eaves - 0.01, 0.075, Sd.Timber, true);
						if (bInside) B.Tri1(At(0.0, Eaves, Thick), At(Fc.Len, Eaves, Thick), At(0.5 * Fc.Len, Ridge, Thick), Sd.Within, Out3 * -1.0);
					}
					continue;
				}
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
				Mound(B, W(R.Range(-0.2, 0.2) * Long, 0.0, Floor - 0.1), 0.3 * Long, 0.3 * Deep, bShell ? 1.1 : 0.6, H.Yaw, Sd.bPlank ? Sd.Fallen : L.Stone, H.Key ^ 0x60);
			}
			if (Broken) Mound(B, W(Broken * (0.5 * Long + 1.0), R.Range(-0.8, 0.8), Floor - 0.25), 2.4, 0.45 * Deep + 0.8, 1.0, H.Yaw, Sd.bPlank ? Sd.Fallen : L.Stone, H.Key ^ 0x61);
			// The roof: two slopes of panels with their thickness, overhanging the walls; a hole or a broken end where hit.
			if (bRoof)
			{
				// (a shed on trial under straw: the thatch 30 cm thick, hung half a metre over the walls, laid in courses of
				// rough sheaves, each course's butts standing over the one below; grey with age)
				const bool bThatch = H.bBoards && Sd.bThatch;
				const double Over = bThatch ? 0.5 : 0.35, GOver = bThatch ? 0.35 : 0.25, Th = bThatch ? 0.3 : 0.14;
				const Col RoofC = bThatch ? Sd.Thatch : L.Roof;
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
					// (a thatch's sheaves, two to a panel, the line between them a little to one side in each course; a course's
					// lower edge jogs 3 to 6 cm up or down the slope at each sheaf, the two of a panel opposite ways, so no line
					// runs straight for over two sheaves; at the eave each hangs up to 10 cm as it was cut)
					const uint64_t SS = static_cast<uint64_t>(S + 2);
					const double SlopeLen = std::hypot(0.5 * Deep + Over, Ridge - EaveZ);
					auto Edge = [&](int Pn, int Jn, int Rw) {
						const double At0 = U0 + (U1 - U0) * Pn / NU;
						if (Pn <= 0 || Pn >= NU || Missing(S, Pn - 1, Jn) || Missing(S, Pn, Jn)) return At0;
						return At0 + 0.24 * (U1 - U0) / NU * (Unit(Hash(H.Key, 0x7AE0ull + SS, static_cast<uint64_t>(Pn * 16 + Rw))) - 0.5);
					};
					auto Split = [&](int Pn, int Rw) { return 0.5 + 0.2 * (Unit(Hash(H.Key, 0x7AB0ull + SS, static_cast<uint64_t>(Pn * 16 + Rw))) - 0.5); };
					auto Jog = [&](int Sf, int Rw) {
						const uint64_t RK = Hash(H.Key, 0x7AA0ull + SS, static_cast<uint64_t>(Rw));
						const bool bUp = (((Hash(RK, static_cast<uint64_t>(Sf / 2)) >> 9) + static_cast<uint64_t>(Sf)) & 1ull) != 0;
						const double Far = 0.03 + 0.03 * Unit(Hash(RK, 0x100ull + static_cast<uint64_t>(Sf)));
						return bUp ? Far : -Far;
					};
					auto Hang = [&](int Sf) { return 0.10 * Unit(Hash(H.Key, 0x7AD0ull + SS, static_cast<uint64_t>(Sf))); };
					// (the streaks down the slope: a tone at each panel's side and one of its own between, 18 in 100 across, half
					// as much again as a course's own swing from its butts to its top)
					auto Streak = [&](int Cn) {
						auto Side = [&](int Pn) { return 0.91 + 0.18 * Unit(Hash(H.Key, 0x7A70ull + SS, static_cast<uint64_t>(Pn))); };
						return Cn % 2 == 0 ? Side(Cn / 2) : 0.5 * (Side(Cn / 2) + Side(Cn / 2 + 1)) + 0.10 * (Unit(Hash(H.Key, 0x7AC0ull + SS, static_cast<uint64_t>(Cn))) - 0.5);
					};
					// (a place mended with newer straw: one panel in twenty begins one, four sheaves wide at most; each sheaf's
					// part of it begins and ends a course off its neighbour's, so its outline is ragged along the sheaves and no
					// side of it straight for over two courses)
					auto Mended = [&](int Sf, int Rw) {
						static const int Shape[4][4][2] = {
							{ { 0, 0 }, { -1, 1 }, { 0, 2 }, { -1, 0 } },
							{ { 0, 1 }, { 1, 3 }, { 0, 2 }, { 1, 1 } },
							{ { 0, 0 }, { -1, 1 }, { 0, 0 }, { 9, -9 } },
							{ { 0, 1 }, { -1, 0 }, { 0, 2 }, { 1, 1 } },
						};
						for (int Pn = Sf / 2 - 1; Pn <= Sf / 2; ++Pn)
							for (int Jn = 0; Jn < NV && Pn >= 0; ++Jn)
							{
								const uint64_t NH = Hash(H.Key, static_cast<uint64_t>(Pn * 8 + Jn), SS);
								if (!(Unit(Mix(NH ^ 0x51)) < 0.05) || Missing(S, Pn, Jn)) continue;
								const int Cg = Sf - 2 * Pn, Rr = Rw - (4 * Jn + 1);
								const int* Span5 = Shape[(NH >> 20) & 3][((NH >> 24) & 1) != 0 ? 3 - Cg : Cg];
								if (Rr >= Span5[0] && Rr <= Span5[1]) return true;
							}
						return false;
					};
					// (a sheaf's lower edge in its course: how far up the slope it lies, how far each of its corners stands proud,
					// and whether its top is the roof's or a hole's edge)
					struct Butt
					{
						double Te = 0.0, RA = 0.0, RB = 0.0;
						bool bHead = false;
					};
					auto ButtOf = [&](int Ip, int Jp, int Csp, int Bhp) {
						const int Rwp = Jp * 4 + Csp, Sfp = 2 * Ip + Bhp;
						const double TAp = static_cast<double>(Jp) / NV, TBp = static_cast<double>(Jp + 1) / NV, Tap = Lerp(TAp, TBp, Csp / 4.0);
						const bool bEaveP = Csp == 0 && Jp == 0, bFootP = Csp == 0 && Jp > 0 && Missing(S, Ip, Jp - 1);
						const bool bSideA = Bhp == 0 && (Ip == 0 || Missing(S, Ip - 1, Jp)), bSideB = Bhp == 1 && (Ip == NU - 1 || Missing(S, Ip + 1, Jp));
						// (a corner over a hole's corner lies flat as its neighbour's does)
						const bool bCornerA = Bhp == 0 && Csp == 0 && Jp > 0 && Ip > 0 && Missing(S, Ip - 1, Jp - 1), bCornerB = Bhp == 1 && Csp == 0 && Jp > 0 && Ip + 1 < NU && Missing(S, Ip + 1, Jp - 1);
						Butt Bt;
						Bt.Te = bEaveP ? -Hang(Sfp) / SlopeLen : bFootP ? Tap : Tap + Jog(Sfp, Rwp) / SlopeLen;
						Bt.RA = bEaveP || bFootP || bSideA || bCornerA ? 0.0 : -0.04;
						Bt.RB = bEaveP || bFootP || bSideB || bCornerB ? 0.0 : -0.04;
						Bt.bHead = Csp == 3 && (Jp + 1 >= NV || Missing(S, Ip, Jp + 1));
						return Bt;
					};
					for (int I = 0; I < NU; ++I)
						for (int J = 0; J < NV; ++J)
						{
							if (Missing(S, I, J)) continue;
							const double UA = U0 + (U1 - U0) * I / NU, UB = U0 + (U1 - U0) * (I + 1) / NU, TA = static_cast<double>(J) / NV, TB = static_cast<double>(J + 1) / NV;
							const uint64_t PH = Hash(H.Key, static_cast<uint64_t>(I * 8 + J), static_cast<uint64_t>(S + 2));
							// (thatch takes its tone from the roof's corners below, so no panel reads as a tile of its own)
							Col C = bThatch ? RoofC : Varied(RoofC, 0.94 + 0.12 * Unit(PH), 0.0);
							if (!bThatch && J == 0 && Unit(Mix(PH)) < 0.5) C = Mixed(C, H.Roof == 1 ? Rgb(92, 84, 62) : Rgb(132, 132, 104), 0.3); // moss, lichen
							if (H.Hurt != DmNone && Near(S, I, J)) C = Mixed(C, Rgb(36, 32, 30), 0.55);
							const Col C2 = Varied(C, 1.04, 0.0);
							const Col Under = H.bBoards ? Rgb(50, 44, 38) : Rgb(70, 60, 52); // (a shed on trial: its roof's underside as dark as its inside)
							if (bThatch)
							{
								// (straw under the sheaves, 3 cm down: what shows under a course's butts and at any hair's width between
								// two sheaves is straw in shade, never the dark under the roof or the sky)
								B.Quad1(RP(UA, TA, 0.03), RP(UB, TA, 0.03), RP(UB, TB, 0.03), RP(UA, TB, 0.03), Scaled(C, 0.8), Up);
								for (int Cs = 0; Cs < 4; ++Cs)
									for (int Bh = 0; Bh < 2; ++Bh)
									{
										// (a sheaf in its course: its butts 4 cm proud at its lower edge, that edge 3 to 6 cm up or down the
										// slope from the course's line, each sheaf its own way; its top run in under the next course's butts
										// by a third of a course and more, so the step shows a dark line and never the sky. Flat at a verge
										// and at a hole's edge, where the roof's own edge closes it; at the eave it hangs as its straw was cut)
										const int Rw = J * 4 + Cs, Sf = 2 * I + Bh;
										const double UL = Edge(I, J, Rw), UR = Edge(I + 1, J, Rw), UM = Lerp(UL, UR, Split(I, Rw)), Ua = Bh == 0 ? UL : UM, Ub = Bh == 0 ? UM : UR;
										const double Ta = Lerp(TA, TB, Cs / 4.0), Tb = Lerp(TA, TB, (Cs + 1) / 4.0);
										const Butt Me = ButtOf(I, J, Cs, Bh);
										const bool bEave = Cs == 0 && J == 0, bHead = Me.bHead;
										const double Te = Me.Te;
										const double Tu = bHead ? Tb : std::min(1.0, Tb + 0.3 * (Tb - Ta) + 0.07 / SlopeLen);
										const bool bEdgeA = Bh == 0 && (I == 0 || Missing(S, I - 1, J)), bEdgeB = Bh == 1 && (I == NU - 1 || Missing(S, I + 1, J));
										const double Sink = bHead ? 0.0 : 0.012;
										// (where two sheaves lie side by side their sides are one line from the lower butts up to the top they
										// share: the one whose butts lie higher up the slope takes its corner from its neighbour's side, so
										// no light shows between them)
										auto Shared = [&](double Mine, double Theirs, double TeTheirs) { return Te > TeTheirs ? Lerp(Theirs, Sink, (Te - TeTheirs) / (Tu - TeTheirs)) : Mine; };
										double RA = Me.RA, RB = Me.RB, RN = 0.0;
										if (!bEdgeA)
										{
											const Butt Nb = Bh == 0 ? ButtOf(I - 1, J, Cs, 1) : ButtOf(I, J, Cs, 0);
											if (Nb.bHead == bHead) RA = Shared(RA, Nb.RB, Nb.Te);
										}
										if (!bEdgeB)
										{
											const Butt Nb = Bh == 1 ? ButtOf(I + 1, J, Cs, 0) : ButtOf(I, J, Cs, 1);
											if (Nb.bHead == bHead)
											{
												RB = Shared(RB, Nb.RA, Nb.Te);
												// (and its neighbour's corner on that side, for the eave's side below)
												RN = Nb.Te > Te ? Lerp(Me.RB, Sink, (Nb.Te - Te) / (Tu - Te)) : Nb.RA;
											}
										}
										// (its tone: the streak down the slope from each side of it, the stronger; a little of its course's own;
										// moss on the lowest third, thickest at the eave; a place mended with newer straw a step lighter and
										// yellower, by the same step on a dark roof as on a pale one, and no moss on it yet)
										const bool bNew = Mended(Sf, Rw);
										const Col Cc = bNew ? Col{ C.R * 1.126, C.G * 1.129, C.B * 1.086 } : C;
										auto Straw = [&](int Cn, double Tt, double Lit) {
											const double Tone = Streak(Cn) + 0.04 * (Unit(Hash(H.Key, 0x7A80ull + SS, static_cast<uint64_t>(Cn * 16 + Rw))) - 0.5);
											const double Mo = Unit(Hash(H.Key, 0x7A90ull + SS, static_cast<uint64_t>(Cn)));
											const double Moss = J == 0 && !bNew ? 0.5 * Mo * Mo * Clamp01(1.0 - Tt * NV) : 0.0;
											return Scaled(Mixed(Cc, Rgb(98, 102, 68), Moss), Tone * Lit);
										};
										B.Quad(RP(Ua, Te, RA), RP(Ub, Te, RB), RP(Ub, Tu, Sink), RP(Ua, Tu, Sink), Straw(Sf, Ta, 0.98), Straw(Sf + 1, Ta, 0.98), Straw(Sf + 1, Tb, 0.86), Straw(Sf, Tb, 0.86), Up);
										if (!bEave) continue;
										// (the eave: the sheaf's cut butts, the thatch's underside out to them, and its side where its
										// neighbour hangs shorter or longer, or the roof ends)
										B.Quad1(RP(Ua, Te, RA), RP(Ub, Te, RB), RP(Ub, Te, Th), RP(Ua, Te, Th), Scaled(RoofC, 0.7), V3{ V.X * S, V.Y * S, -0.3 });
										B.Quad1(RP(Ua, Te, Th), RP(Ub, Te, Th), RP(Ub, TB, Th), RP(Ua, TB, Th), Under, Up * -1.0);
										if (bEdgeA) B.Quad1(RP(Ua, Te, 0.0), RP(Ua, 0.0, 0.0), RP(Ua, 0.0, Th), RP(Ua, Te, Th), Scaled(RoofC, 0.75), V3{ -U.X, -U.Y, 0.0 });
										if (bEdgeB) B.Quad1(RP(Ub, Te, 0.0), RP(Ub, 0.0, 0.0), RP(Ub, 0.0, Th), RP(Ub, Te, Th), Scaled(RoofC, 0.75), V3{ U.X, U.Y, 0.0 });
										else
										{
											const double Tn = -Hang(Sf + 1) / SlopeLen, Way = Te < Tn ? 1.0 : -1.0;
											B.Quad1(RP(Ub, Te, RB), RP(Ub, Tn, RN), RP(Ub, Tn, Th), RP(Ub, Te, Th), Scaled(RoofC, 0.75), V3{ U.X * Way, U.Y * Way, 0.0 });
										}
									}
							}
							else B.Quad(RP(UA, TA, 0.0), RP(UB, TA, 0.0), RP(UB, TB, 0.0), RP(UA, TB, 0.0), C, C, C2, C2, Up);
							if (!bThatch || J > 0) B.Quad1(RP(UA, TA, Th), RP(UB, TA, Th), RP(UB, TB, Th), RP(UA, TB, Th), Under, Up * -1.0);
							if (!bThatch && J == 0) B.Quad1(RP(UA, 0.0, 0.0), RP(UB, 0.0, 0.0), RP(UB, 0.0, Th), RP(UA, 0.0, Th), Scaled(RoofC, 0.7), V3{ V.X * S, V.Y * S, -0.3 });
							// (a thatch's cut edge at a hole or a broken end is charred straw; a verge is straw)
							const Col Char = bThatch ? Mixed(RoofC, Rgb(36, 32, 30), 0.6) : Scaled(RoofC, 0.75), CharD = bThatch ? Char : Scaled(RoofC, 0.7);
							if (I == 0 || Missing(S, I - 1, J)) B.Quad1(RP(UA, TA, 0.0), RP(UA, TB, 0.0), RP(UA, TB, Th), RP(UA, TA, Th), I > 0 ? Char : Scaled(RoofC, 0.75), V3{ -U.X, -U.Y, 0.0 });
							if (I == NU - 1 || Missing(S, I + 1, J)) B.Quad1(RP(UB, TA, 0.0), RP(UB, TB, 0.0), RP(UB, TB, Th), RP(UB, TA, Th), I < NU - 1 ? Char : Scaled(RoofC, 0.75), V3{ U.X, U.Y, 0.0 });
							if (J + 1 < NV && Missing(S, I, J + 1)) B.Quad1(RP(UA, TB, 0.0), RP(UB, TB, 0.0), RP(UB, TB, Th), RP(UA, TB, Th), CharD, Up * -1.0 + V3{ 0, 0, 0.5 });
							if (J > 0 && Missing(S, I, J - 1)) B.Quad1(RP(UA, TA, 0.0), RP(UB, TA, 0.0), RP(UB, TA, Th), RP(UA, TA, Th), CharD, V3{ V.X * S, V.Y * S, -0.3 });
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
				const Col Cap = bThatch ? Mixed(RoofC, Sd.Clay, 0.3) : H.Roof == 1 ? Scaled(L.Roof, 0.82) : Scaled(L.Roof, 0.8);
				// (a thatch's ridge: straw bent over it and sealed with clay, a roll twice as wide; held by pairs of crossed poles)
				const double CW = bThatch ? 0.45 : 0.22, CZ = bThatch ? 0.14 : 0.09;
				const double CE = bThatch ? Ridge - CW * Tan + 0.04 : Ridge + CZ - CW * Tan * 0.6; // (a thatch's roll comes down onto the straw)
				for (int S = -1; S <= 1; S += 2)
					B.Quad1(W(CU0, 0.0, Ridge + CZ), W(CU1, 0.0, Ridge + CZ), W(CU1, S * CW, CE), W(CU0, S * CW, CE), Cap, V3{ V.X * S, V.Y * S, 1.5 });
				if (bThatch)
				{
					const Col Pole = Rgb(112, 104, 92);
					const V3 U3{ U.X, U.Y, 0.0 };
					const int NPair = std::max(2, static_cast<int>((CU1 - CU0 - 1.0) / 2.2));
					for (int K = 0; K <= NPair; ++K)
						for (int S = -1; S <= 1; S += 2)
						{
							// (a pole up one slope from 1.2 m under the ridge, on over it by 40 cm: its fellow up the other slope beside it)
							const double Uu = CU0 + 0.5 + (CU1 - CU0 - 1.0) * K / NPair + S * 0.05;
							// (no pole over open rafters: not past a broken end, not over a hole that reaches the ridge)
							const int RI = std::clamp(static_cast<int>((Uu - U0) / (U1 - U0) * NU), 0, NU - 1);
							if (Missing(-1, RI, NV - 1) || Missing(1, RI, NV - 1)) continue;
							const V3 P0 = W(Uu, S * 1.2, Ridge - 1.2 * Tan + 0.08), P1 = W(Uu, -S * 0.4, Ridge + 0.4 * Tan + 0.08);
							const V3 Nn = Unit3(Cross(P1 - P0, U3)) * 0.028, Uh = U3 * 0.028;
							B.Quad1(P0 - Uh, P0 + Uh, P1 + Uh, P1 - Uh, Pole, V3{ V.X * S, V.Y * S, 1.0 });
							B.Quad1(P0 - Nn, P0 + Nn, P1 + Nn, P1 - Nn, Scaled(Pole, 0.8), U3);
							B.Quad1(P0 - Nn, P0 + Nn, P1 + Nn, P1 - Nn, Scaled(Pole, 0.8), U3 * -1.0);
						}
				}
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
	double Ground::Impl::UnderZ(double X, double Y) const
	{
		// (the steppe's railway bank is swept at its own points: the ground's mesh leaves it out and sinks a little under it)
		return Sp ? Land(X, Y) - (Sp->HasRail() ? 0.25 * Smooth(8.4, 7.4, Sp->RailDistance(X, Y)) : 0.0) : Terrain(X, Y);
	}

	double Ground::Impl::NearZ(double X, double Y) const
	{
		double Edge = 1e6, Z = UnderZ(X, Y);
		if (!InRaster(X, Y) || LaneD[Pix(X, Y)] < LaneWall + 3.0f) Z = Carve(X, Y, Z, &Edge);
		// Under a lane's mesh the ground drops a little more, so the lane is always the one seen.
		return Z - 0.35 * (1.0 - Smooth(LaneWall + 0.4, LaneWall + 1.0, Edge));
	}

	void Ground::Impl::MakeTiles()
	{
		const int TX = TilesX(), TY = TilesY();
		TileList.assign(static_cast<size_t>(TX) * TY, Tile());
		auto TileOf = [&](double X, double Y) -> Tile* {
			const int I = static_cast<int>(std::floor((X + Band) / TileSize)), J = static_cast<int>(std::floor((Y + Band) / TileSize));
			if (I < 0 || J < 0 || I >= TX || J >= TY) return nullptr;
			return &TileList[static_cast<size_t>(J) * TX + I];
		};
		// (the heights as members: Ground::DrawnUp asks them again, to light what is laid on the ground as these tiles are)
		auto NearZ_ = [this](double X, double Y) { return NearZ(X, Y); };
		auto FarZ = [this](double X, double Y) { return UnderZ(X, Y); };
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
				GridMesh(T.Near, G, NearZ_, NearCol);
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
		// Under snow the frame's own lanes (the bocage's sunken ones, the steppe's raised road) take it as a region's do in
		// RegionLane: the road packed and rutted, its sides the plough's banks.
		// (the place read once a section, at its middle, and moved across it: read at every point, the snow cost more than
		// the lanes themselves)
		const auto LaneSnowed = [this](const Col& Bare, const LandScape::Spot& Mid, double X, double Y, double A, double Half) {
			LandScape::Spot S = Mid;
			S.X = X;
			S.Y = Y;
			S.Kind = LandScape::KLane;
			S.Lane = A - Half;
			S.LaneHalf = Half;
			const LandScape::Tone T = SnowLay->Over(LandScape::Tone{ Bare.R, Bare.G, Bare.B }, S);
			return Col{ T.R, T.G, T.B };
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
				const bool bSnowLane = SnowLay && !Rg && InRaster(Ln.P[K].X, Ln.P[K].Y);
				const LandScape::Spot Mid = bSnowLane ? SpotAt(Ln.P[K].X, Ln.P[K].Y, true) : LandScape::Spot{};
				for (const double Off : D)
				{
					const V2 Q = Ln.P[K] + N * Off;
					double Edge = 1e6;
					const double Z = Carve(Q.X, Q.Y, Terrain(Q.X, Q.Y), &Edge);
					const double A = std::fabs(Off);
					const double N1 = Noise(Q.X / 3.0, Q.Y / 3.0, Seed ^ 0x21), N2 = Noise(Q.X / 0.9, Q.Y / 0.9, Seed ^ 0x22);
					Col C;
					if (Rg && A < Half + LaneWall + 0.01) C = RegionLane(Q.X, Q.Y, A, Half); // (a region's road: its own)
					else if (A <= Half + 0.01)
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
					if (SnowLay && !Rg && A < Half + LaneWall + 0.01) C = bSnowLane ? LaneSnowed(C, Mid, Q.X, Q.Y, A, Half) : Snowed(C, Q.X, Q.Y, false);
					// (rain: the lane's floor and walls soaked and pooled as the ground is, and water lying along its ruts)
					if (RainWet > 0.0 && A < Half + LaneWall + 0.01)
					{
						C = Wetted(C, Q.X, Q.Y);
						if (A <= Half + 0.01) C = Mixed(C, Water, 0.6 * std::clamp(RainWet, 0.0, 1.0) * std::exp(-Sq((A - 0.8) / 0.25)));
					}
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
				const bool bSnowBank = SnowLay && InRaster(Rn.P[K].X, Rn.P[K].Y);
				const LandScape::Spot Mid = bSnowBank ? SpotAt(Rn.P[K].X, Rn.P[K].Y, true) : LandScape::Spot{};
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
						// (a hedge bank mostly grass and weeds over its earth: the second cut's read as bare clay)
						C = Mixed(C, Earth, Clamp01(0.25 * Steep - 0.15 + 0.3 * N1) * (Rn.Kind == HkLaneBank ? 1.0 : 0.35));
					}
					// (under snow the bank holds it on its top and gentle sides; thinner at the hedge's foot, where the hedge
					// took the fall, and on a lane bank's steep face, the earth showing through in streaks)
					if (SnowLay && A <= 1.0)
					{
						const double Steep = High / std::max(0.3, 0.35 * Half);
						const double Hold = A < BankTop + 0.05 ? 0.55 + 0.2 * N1 : Clamp01(0.95 - 0.12 * Steep * (Rn.Kind == HkLaneBank ? 1.0 : 0.5) + 0.15 * N2);
						Col White;
						if (bSnowBank)
						{
							LandScape::Spot At_ = Mid;
							At_.X = Q.X;
							At_.Y = Q.Y;
							const LandScape::Tone Tn = SnowLay->Over(LandScape::Tone{ C.R, C.G, C.B }, At_);
							White = Col{ Tn.R, Tn.G, Tn.B };
						}
						else White = Snowed(C, Q.X, Q.Y, false);
						C = Mixed(C, White, Hold);
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
					F.C = { S.C[0], SnowLay ? Mixed(S.C[2], S.C[3], 0.5) : Mixed(HedgeFoot, BankGrass, 0.4), S.C[5] };
					FarSecs.push_back(F);
				}
			}
			Sweep(Secs, NearOf);
			Sweep(FarSecs, FarOf);
		}
		// The stone walls: a face each side, a crowned coping, their ends squared.
		const bool WallsBroken = Rg && Rg->WallBroken();
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
				// (a region's walls broken along their tops, on trial: Region::WallBroken, Italy's "walltop": each point's top
				// a stone higher or lower, a stone's middle between, one length in five with its coping fallen)
				const bool bBroken = WallsBroken;
				// (broken: the top fallen lower along wandering stretches some metres long, each stone a little uneven; never
				// higher than asked, so a wall inside the kit's stone stays inside it)
				auto Fallen = [&](size_t I) {
					const double Lane = static_cast<double>(Wr.Key % 977) + 0.5;
					return -0.05 - 0.2 * Smooth(-0.3, 0.5, Noise(static_cast<double>(I) / 3.0, Lane, Seed ^ 0x70B)) - 0.06 * Unit(Hash(Wr.Key ^ 0x70A, I));
				};
				const double JagA = bBroken ? Fallen(K) : 0.0, JagB = bBroken ? Fallen(K + 1) : 0.0;
				const double TA = GA + Wr.H[K] + JagA, TB = GB + Wr.H[K + 1] + JagB;
				const uint64_t Hk = Hash(Wr.Key, K);
				const LandScape::Tone Own = Rg ? Rg->WallStone() : LandScape::Rgb(168, 160, 142); // (a region's walls: its own stone)
				const Col Stone = Varied(Rgb(Own.R, Own.G, Own.B), 0.9 + 0.16 * Unit(Hk), 0.03 * (Unit(Mix(Hk)) - 0.5));
				const double Ivy = Smooth(0.25, 0.6, Noise((A.X + A.Y) / 7.0, static_cast<double>(Wr.Key % 101), Seed ^ 0x41));
				const Col Face = Mixed(Stone, Rgb(58, 74, 40), (Rg ? Rg->WallIvy() : 0.8) * Ivy), Foot = Mixed(Face, Rg ? Stone : Rgb(96, 98, 78), 0.5);
				if (bBroken)
				{
					// (each point its own stone's tone, shared with the next length, so no length reads as a cast panel)
					auto StoneAt = [&](uint64_t I) {
						const Col C = Varied(Rgb(Own.R, Own.G, Own.B), 0.84 + 0.24 * Unit(Hash(Wr.Key ^ 0x70C, I)), 0.05 * (Unit(Hash(Wr.Key ^ 0x70D, I)) - 0.5));
						return Mixed(C, Rgb(58, 74, 40), (Rg ? Rg->WallIvy() : 0.8) * Ivy);
					};
					const V2 Mid = (A + Bp) * 0.5;
					const double GM = 0.5 * (GA + GB);
					// (and a stone gone from the top here and there: one length in four)
					double TM = 0.5 * (TA + TB) - 0.04 * Unit(Hash(Wr.Key ^ 0x70F, K));
					if (Unit(Hash(Wr.Key ^ 0x70E, K)) < 0.25) TM -= 0.12 + 0.16 * Unit(Hash(Wr.Key ^ 0x710, K));
					TM = std::max(TM, GM + 0.35);
					const V2 Pt[3] = { A, Mid, Bp };
					const double Gd[3] = { GA, GM, GB }, Tp[3] = { TA, TM, TB };
					const Col Cs[3] = { StoneAt(2 * K), StoneAt(2 * K + 1), StoneAt(2 * K + 2) };
					// (the face's foot and top each their own stone's tone, crossing along it, the foot darker: no smooth slab,
					// and no more triangles than a plain broken wall; a face in two courses read 1.73 million near, over 1.5)
					const Col Us[3] = { StoneAt(0x40000 + 2 * K), StoneAt(0x40000 + 2 * K + 1), StoneAt(0x40000 + 2 * K + 2) };
					for (int Half = 0; Half < 2; ++Half)
						for (int S = -1; S <= 1; S += 2)
						{
							const V2 O = N * static_cast<double>(S), P0 = Pt[Half], P1 = Pt[Half + 1];
							const Col F0 = Cs[Half], F1 = Cs[Half + 1], U0 = Us[Half], U1 = Us[Half + 1];
							B.Quad(V3{ P0.X + O.X, P0.Y + O.Y, Gd[Half] - 0.3 }, V3{ P1.X + O.X, P1.Y + O.Y, Gd[Half + 1] - 0.3 }, V3{ P1.X + O.X, P1.Y + O.Y, Tp[Half + 1] - 0.08 },
								V3{ P0.X + O.X, P0.Y + O.Y, Tp[Half] - 0.08 }, Varied(F0, 0.7, 0.0), Varied(F1, 0.7, 0.0), U1, U0, V3{ O.X, O.Y, 0.0 });
							B.Quad(V3{ P0.X + O.X, P0.Y + O.Y, Tp[Half] - 0.08 }, V3{ P1.X + O.X, P1.Y + O.Y, Tp[Half + 1] - 0.08 }, V3{ P1.X, P1.Y, Tp[Half + 1] + 0.04 },
								V3{ P0.X, P0.Y, Tp[Half] + 0.04 }, Varied(F0, 1.08, 0.0), Varied(F1, 1.08, 0.0), Varied(F1, 1.08, 0.0), Varied(F0, 1.08, 0.0), V3{ O.X, O.Y, 1.2 });
						}
				}
				else
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
		// The buildings (the steppe's cottages are the kit's, planted).
		for (const House& H : Houses)
		{
			if (H.bKit || H.Model >= 0) continue;
			Tile* T = TileOf(H.C.X, H.C.Y);
			if (T) BuildHouse(T->Built, H);
		}
		// The kit houses' footings (SeatKitHouses): a block of the walls' stone from the lowest ground under each to its floor.
		for (size_t K = 0; K < Footings.size(); ++K)
		{
			const Footing& Fo = Footings[K];
			Tile* T = TileOf(Fo.X, Fo.Y);
			if (!T) continue;
			Builder Bd(T->Built);
			const V3 U{ std::cos(Fo.Yaw), std::sin(Fo.Yaw), 0.0 }, V{ -std::sin(Fo.Yaw), std::cos(Fo.Yaw), 0.0 };
			Bd.Box(V3{ Fo.X, Fo.Y, 0.5 * (Fo.Foot + Fo.Top) }, U * Fo.HalfL, V * Fo.HalfD, V3{ 0.0, 0.0, 0.5 * (Fo.Top - Fo.Foot) }, FootingPaints[K]);
		}
		// The steppe's railway on its bank: ballast, sleepers and rails near, the bank and its ballast far.
		if (Sp && Sp->HasRail())
		{
			const std::function<double(double, double)> Ground = [this](double X, double Y) { return Terrain(X, Y); };
			const std::function<LandSteppe::Tone(double, double)> Paint = [this](double X, double Y) { return ToTone(GroundColour(X, Y)); };
			const std::function<Mesh*(double, double)> Near = NearOf, Far = FarOf;
			double Km = 0.0;
			LandSteppe::LayRail(*Sp, -Band, -Band + TilesY() * TileSize, false, Ground, Paint, Near, Km);
			Count.RailKm += Km;
			LandSteppe::LayRail(*Sp, -Band, -Band + TilesY() * TileSize, true, Ground, Paint, Far, Km);
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
		const double F = Fbm(X, Y, 520.0, 3, Seed ^ 0xD5), Edge = Sp ? 0.43 : Rg ? Rg->WoodsEdge() : 0.27;
		if (F <= Edge) return 0.0;
		const double OutX = std::max({ 0.0, -Band - X, X - (TilesX() * TileSize - Band) });
		const double OutY = std::max({ 0.0, -Band - Y, Y - (TilesY() * TileSize - Band) });
		const double Crowns = 15.5 + 2.5 * Noise(X / 35.0, Y / 35.0, Seed ^ 0xD6) + 1.5 * Noise(X / 90.0, Y / 90.0, Seed ^ 0xDA);
		return Crowns * Smooth(Edge, Edge + 0.02, F) * Smooth(0.0, 30.0, std::hypot(OutX, OutY));
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
		if (Sp)
		{
			// The steppe has no hedgerows to the horizon. Its railway runs on: in full a kilometre past the tiles each way,
			// then its bank and ballast alone out to the ring's edge.
			if (Sp->HasRail())
			{
				const std::function<double(double, double)> Ground = [this](double X, double Y) { return Terrain(X, Y); };
				const std::function<LandSteppe::Tone(double, double)> Paint = [this](double X, double Y) { return ToTone(GroundColour(X, Y)); };
				const std::function<Mesh*(double, double)> Target = RingOf;
				double Km = 0.0;
				LandSteppe::LayRail(*Sp, IY0 - 1000.0, IY0, false, Ground, Paint, Target, Km);
				Count.RailKm += Km;
				LandSteppe::LayRail(*Sp, IY1, IY1 + 1000.0, false, Ground, Paint, Target, Km);
				Count.RailKm += Km;
				LandSteppe::LayRail(*Sp, IY0 - RingOut, IY0 - 1000.0, true, Ground, Paint, Target, Km);
				LandSteppe::LayRail(*Sp, IY1 + 1000.0, IY1 + RingOut, true, Ground, Paint, Target, Km);
			}
			for (Tile& T : RingList)
			{
				Bounds(T);
				Count.RingTris += T.Near.Tris();
			}
			return;
		}
		if (Rg && !Rg->FarRows())
		{
			// (a region's plain has no hedgerows to the horizon unless it asks: the desert's serir, Italy's terraces)
			for (Tile& T : RingList)
			{
				Bounds(T);
				Count.RingTris += T.Near.Tris();
			}
			return;
		}
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

#include "Battle/LandScorch.h" // (the burnt cottages' look and ground, release 2)

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

		/**
		 * A leaf folded up at its midrib: through Spine (foot to tip), Half wide either side at each, its two halves lifted
		 * Lift of their width toward the leaf's upper side, each half its own flat face (a crease along the midrib). Above,
		 * Foot to Tip along it, the midrib a little paler; beneath, Under (a leaf's underside paler and greyer), its normals
		 * turned down. Across is the leaf's breadth, near level; the upper side is the one facing up.
		 */
		void Folded(Mesh& M, const std::vector<V3>& Spine, const std::vector<double>& Half, V3 Across, double Lift, const Col& Foot, const Col& Tip, const Col& Under, const Col& Rib)
		{
			if (Spine.size() < 2) return;
			Builder B(M);
			const size_t N = Spine.size();
			for (int Face = 0; Face < 2; ++Face)
			{
				for (int Halfway = 0; Halfway < 2; ++Halfway)
				{
					const double Way = Halfway == 0 ? -1.0 : 1.0;
					const uint32_t Base0 = static_cast<uint32_t>(M.Points());
					V3 Toward{ 0, 0, 1 };
					for (size_t K = 0; K < N; ++K)
					{
						const V3 Along = Unit3(Spine[std::min(K + 1, N - 1)] - Spine[K > 0 ? K - 1 : 0], V3{ 1, 0, 0 });
						V3 Up = Unit3(Cross(Across, Along));
						if (Up.Z < 0.0) Up = Up * -1.0;
						const V3 Side = Unit3(Cross(Along, Up), Across) * (Dot3(Cross(Along, Up), Across) < 0.0 ? -1.0 : 1.0);
						const V3 Edge = Spine[K] + (Side * Way + Up * Lift) * Half[K];
						// (each half's face: up, tipped away from the crease)
						const V3 Lit = Unit3(Up - Side * (Way * Lift));
						const V3 Nrm = Face == 0 ? Unit3(Lit + V3{ 0, 0, 0.35 }) : Unit3(Lit * -1.0 + V3{ 0, 0, 0.5 });
						Toward = Face == 0 ? Lit : Lit * -1.0;
						const double T = static_cast<double>(K) / static_cast<double>(N - 1);
						const Col Top = Mixed(Foot, Tip, T);
						B.Vert(Spine[K], Nrm, Face == 0 ? Mixed(Top, Rib, 0.35) : Under);
						B.Vert(Edge, Nrm, Face == 0 ? Top : Varied(Under, 0.94, 0.0));
					}
					for (size_t K = 0; K + 1 < N; ++K)
					{
						const uint32_t A0 = Base0 + 2 * static_cast<uint32_t>(K), A1 = A0 + 1, B0 = A0 + 2, B1 = A0 + 3;
						TriFacing(M, A0, A1, B1, Toward);
						if (Half[K + 1] > 1e-4) TriFacing(M, A0, B1, B0, Toward);
					}
				}
			}
		}

		void Measure(BaseMesh& B)
		{
			float R = 0.0f, H = 0.0f;
			for (const Mesh* M : { &B.Near, &B.Cards, &B.Sprigs })
				for (size_t K = 0; K + 2 < M->Positions.size(); K += 3)
				{
					R = std::max(R, std::hypot(M->Positions[K], M->Positions[K + 1]));
					H = std::max(H, M->Positions[K + 2]);
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

		/** The leaves' picture's quarters (T_LandLeaves, Tools/land_look/make_leaf_textures.py): which spray a card carries. */
		enum LeafCell : int
		{
			LeafThorn = 0, // hawthorn, blackthorn: small lobed leaves, close set
			LeafRound = 1, // hazel, field maple: round leaves
			LeafOak = 2,   // oak: lobed leaves bunched at the twigs' ends
			LeafOval = 3   // elm, beech, apple, holly, elder: oval leaves to a point
		};

		/**
		 * A leaf card: a flat W by H card at C carrying one spray of the leaves' picture (its Cell), its twig's foot at the
		 * card's foot, Up the way from foot to tip, turned to face Out (the shrub's or the spray's outward there). One face:
		 * the leaves' material draws both, lit alike. Normals bent Bend of the way toward Soft, as Cluster's are, so the
		 * cards light as one soft mass; painted by Paint at each corner, as a cluster is.
		 */
		void LeafCard(Mesh& M, V3 C, V3 Face, V3 Up, double W, double H, int Cell, V3 Out, const std::function<V3(V3)>& Soft, double Bend, const std::function<Col(V3, V3)>& Paint)
		{
			Face = Unit3(Face, Out);
			if (Dot3(Face, Out) < 0.0) Face = Face * -1.0;
			Up = Up - Face * Dot3(Up, Face);
			Up = Unit3(Up, Unit3(Cross(Face, V3{ 1, 0, 0 }), V3{ 0, 1, 0 }));
			const V3 Rt = Cross(Up, Face); // the viewer's right, seen from the front
			const double U0 = 0.5 * (Cell % 2), V0 = 0.5 * (Cell / 2), E = 0.004;
			const V3 Corner[4] = { C - Up * (0.5 * H) - Rt * (0.5 * W), C - Up * (0.5 * H) + Rt * (0.5 * W), C + Up * (0.5 * H) + Rt * (0.5 * W),
								   C + Up * (0.5 * H) - Rt * (0.5 * W) };
			const double Uv[4][2] = { { U0 + E, V0 + 0.5 - E }, { U0 + 0.5 - E, V0 + 0.5 - E }, { U0 + 0.5 - E, V0 + E }, { U0 + E, V0 + E } };
			Builder B(M);
			const uint32_t Base0 = static_cast<uint32_t>(M.Points());
			for (int K = 0; K < 4; ++K)
			{
				B.Vert(Corner[K], Unit3(Face * (1.0 - Bend) + Soft(Corner[K]) * Bend, Face), Paint(Corner[K], Out));
				M.Uvs.push_back(static_cast<float>(Uv[K][0]));
				M.Uvs.push_back(static_cast<float>(Uv[K][1]));
			}
			B.Tri(Base0, Base0 + 1, Base0 + 2);
			B.Tri(Base0, Base0 + 2, Base0 + 3);
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
			int Cell; // its leaves' spray on the cards (LeafCell)
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
				// (the heart's clusters: in the shade of the leaves on the cards round them, a little smaller and darker,
				// the crown's depth seen through its gaps, not smooth green lumps of their own)
				const auto Shaded = [&PaintS](V3 P, V3 Dir) { return Mixed(PaintS(P, Dir), Rgb(30, 44, 30), 0.3); };
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
					if (bSprig) Cluster(Bm.Sprigs, O, Lr, T.Flat * R.Range(0.85, 1.1), T.Jag, R, SoftS, 0.75, PaintS, true);
					else Cluster(Bm.Near, O, 0.85 * Lr, T.Flat * R.Range(0.85, 1.1), T.Jag, R, SoftS, 0.75, Shaded, false);
				}
				// The leaves themselves on cards over the spray's skin, more of them than the sprigs they stand in for
				// (a card is two triangles, a sprig eight), each turned out from the spray, its twig's foot toward it, a
				// sprig's breadth and a half across; a few set in, the light falling through to the heart's dark.
				Rng RC(Seed ^ (0xCA4D + 977u * static_cast<uint64_t>(&S - Sp.data())));
				const int Cards = (T.Twigs * 8) / 5 + RC.Below(5);
				for (int J = 0; J < Cards; ++J)
				{
					const V3 U = Unit3(AnyWay(RC) + S.Out * 0.5 + V3{ 0, 0, 0.3 }, S.Out);
					const double D = S.Rad * RC.Range(0.45, 1.05);
					const double Lump = RC.Range(T.LumpLo, T.LumpHi);
					const double Size = Lump * RC.Range(1.2, 1.6);
					V3 O = S.C + V3{ U.X * D, U.Y * D, U.Z * D * 0.75 };
					O.Z = std::min(O.Z, T.High - 0.4 * Size);
					const V3 Face = Unit3(U + AnyWay(RC) * 0.35, U);
					const V3 Up = Unit3(U * 0.6 + V3{ 0, 0, 0.5 } + AnyWay(RC) * 0.7, U);
					LeafCard(Bm.Cards, O, Face, Up, Size, Size, T.Cell, U, SoftS, 0.7, PaintS);
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
			int Cell, CellOther; // the leaves' sprays on its cards (its Leaf's, its Second's: LeafCell)
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
					const auto Painted = Paint(bOther ? Sh.Second : Sh.Leaf, R.Range(0.86, 1.14));
					// (the clusters under the cards a little smaller and in their shade: the hedge's depth, not lumps)
					const auto Shaded = [&Painted](V3 Q, V3 Dir) { return Mixed(Painted(Q, Dir), Rgb(28, 40, 28), 0.25); };
					if (bSprig) Cluster(Bm.Sprigs, P, Lr, R.Range(0.65, 0.9), Sh.Jag, R, Soft, 0.45, Painted, true);
					else Cluster(Bm.Near, P, 0.9 * Lr, R.Range(0.8, 1.0), Sh.Jag, R, Soft, 0.45, Shaded, false);
				}
			}
			// The leaves on cards over the body's skin, as the sprigs are spread (a spiral, its top to below the bank's
			// face), a third more of them: each turned out from the shrub, its twig's foot toward the body, a few rising
			// above the top as loose shoots; the Second's leaves (field maple, elder) on their own spray.
			Rng RC(Seed ^ 0xCA4D);
			const int Cards = (Sh.Leaves * 4) / 3;
			for (int K = 0; K < Cards; ++K)
			{
				const double Zf = 1.0 - 1.85 * (K + 0.5) / Cards;
				const double An = K * 2.39996 + 0.7 + RC.Range(-0.5, 0.5);
				const double Ring = std::sqrt(std::max(0.0, 1.0 - Zf * Zf));
				const V3 S{ Ring * std::cos(An), Ring * std::sin(An), Zf };
				const double Lump = RC.Range(Sh.LumpLo, Sh.LumpHi);
				const double Size = Lump * RC.Range(1.5, 2.0);
				V3 P = O + S * (Surface(S) * RC.Range(0.9, 1.08));
				if (Zf > 0.4 && RC.Next() < 0.35) P.Z += RC.Range(0.0, Sh.Sprigs);
				P.Z = std::min(P.Z, Sh.Top - 0.35 * Size);
				const bool bOther = RC.Next() < Sh.Other;
				const V3 Face = Unit3(S + AnyWay(RC) * 0.5, S);
				const V3 Up = Unit3(S * 0.5 + V3{ 0, 0, 0.6 } + AnyWay(RC) * 0.8, S);
				LeafCard(Bm.Cards, P, Face, Up, Size, Size, bOther ? Sh.CellOther : Sh.Cell, S, Soft, 0.5, Paint(bOther ? Sh.Second : Sh.Leaf, RC.Range(0.86, 1.14)));
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
		static const char* const Names[] = { "ShrubA", "ShrubB", "ShrubC", "Bramble", "Oak", "Elm", "Ash", "Beech", "Pollard", "Apple", "Gate", "Well", "Cart", "Khata", "Chimney", "Pole", "Wattle", "Sweep", "Haystack", "Birch", "Kit0", "Kit1", "Kit2", "Kit3", "Kit4", "Kit5", "Kit6", "Kit7", "Kit8", "Kit9", "Kit10", "Kit11", "Kit12", "Kit13", "Kit14", "Kit15", "Kit16", "Kit17", "Kit18", "Kit19", "Kit20", "Kit21", "Kit22", "Kit23", "Kit24", "Kit25", "Kit26", "Kit27", "Kit28", "Kit29", "Kit30", "Kit31", "Kit32", "Kit33", "Kit34", "Kit35", "Kit36", "Kit37", "Kit38", "Kit39", "Kit40", "Kit41", "Kit42", "Kit43", "Kit44", "Kit45", "Kit46", "Kit47", "Kit48", "Kit49", "Kit50", "Kit51", "Kit52", "Kit53", "Kit54", "Kit55", "Kit56", "Kit57", "Kit58", "Kit59", "Kit60", "Kit61", "Kit62", "Kit63", "Haycock", "Tuft", "TallGrass", "Wheat", "Poppies", "Flowers", "Stone", "Fern", "Stubble", "Sward", "Trodden", "Stalks", "Rosette", "Clut0", "Clut1", "Clut2", "Clut3", "Clut4", "Clut5" };
		static_assert(std::size(Names) == static_cast<size_t>(Base::Count), "a name for every base");
		for (size_t K = 0; K < Bases.size(); ++K) Bases[K].Name = Names[K];
		auto At = [this](Base B) -> BaseMesh& { return Bases[static_cast<size_t>(B)]; };
		const uint64_t S = 0xBA5E;

		// The hedge shrubs, built ShrubHigh (2.8 m) tall: hawthorn and blackthorn dark, dense and upright-sided; hazel and
		// field maple lighter and rounder; holly and elder (dark and light clusters mixed) tall and loose, their sprigs above
		// the rest. Bramble low and sprawling, a metre high.
		BuildShrub(At(Base::ShrubA), { 2.8, 1.3, 2.5, 10, 200, 0.3, 0.46, 0.42, 0.35, Rgb(50, 68, 34), Rgb(58, 74, 36), 0.25, LeafThorn, LeafThorn }, S + 1);
		BuildShrub(At(Base::ShrubB), { 2.8, 1.4, 2.2, 10, 180, 0.32, 0.5, 0.3, 0.25, Rgb(72, 94, 42), Rgb(84, 100, 46), 0.3, LeafRound, LeafOak }, S + 2);
		BuildShrub(At(Base::ShrubC), { 2.8, 1.15, 2.4, 10, 200, 0.3, 0.46, 0.45, 0.6, Rgb(42, 60, 34), Rgb(70, 90, 44), 0.4, LeafOval, LeafRound }, S + 3);
		BuildShrub(At(Base::Bramble), { 1.0, 0.95, 2.0, 6, 130, 0.2, 0.32, 0.45, 0.15, Rgb(58, 72, 38), Rgb(66, 76, 40), 0.2, LeafThorn, LeafOak }, S + 4);
		for (const Base B : { Base::ShrubA, Base::ShrubB, Base::ShrubC, Base::Bramble }) At(B).FarFrom = 60.0f;
		// (brambles low under the hedges and in the woods' edges, drawn near only, out to 70 m as the grass is)
		At(Base::Bramble).Far = Mesh();
		At(Base::Bramble).FarFrom = 70.0f;

		// The trees, each built at its TreeHigh: the hedgerow oak broad and ragged on spreading limbs; the elm tall, its
		// crown pulled in at the waist; the ash airy, small sprays far apart and pale; the beech dense and domed; the pollard
		// a thick stub, a knobbly knuckle and a bushy head of straight sprouts; the orchard apple low and wide on a short,
		// crooked, leaning trunk.
		BuildTree(At(Base::Oak), { 15.0, 3.4, 0.45, 0.15, 0.5, 9.0, 6.3, 4.6, 0.0, 6, 16, 4, 22, 2.2, 0.62, 1.12, 0.72, 0.38, 0.45, 0.2, Rgb(62, 84, 38), Rgb(88, 82, 70), true, false, LeafOak }, S + 10);
		BuildTree(At(Base::Elm), { 18.0, 4.5, 0.42, 0.1, 0.3, 11.8, 4.8, 5.8, 0.3, 4, 16, 3, 24, 1.9, 0.5, 0.95, 0.75, 0.35, 0.35, 0.16, Rgb(56, 78, 36), Rgb(78, 70, 60), true, false, LeafOval }, S + 11);
		BuildTree(At(Base::Ash), { 15.0, 4.0, 0.36, 0.1, 0.3, 10.0, 5.3, 4.4, 0.0, 5, 16, 2, 20, 1.7, 0.42, 0.75, 0.65, 0.4, 0.15, 0.26, Rgb(80, 100, 46), Rgb(118, 114, 102), true, false, LeafOval }, S + 12);
		BuildTree(At(Base::Beech), { 18.0, 3.6, 0.42, 0.05, 0.15, 11.2, 6.8, 5.8, 0.0, 5, 16, 4, 24, 2.2, 0.65, 1.1, 0.72, 0.3, 0.5, 0.18, Rgb(66, 92, 38), Rgb(128, 126, 116), true, false, LeafOval }, S + 13);
		BuildTree(At(Base::Pollard), { 8.0, 2.8, 0.5, 0.0, 0.6, 5.5, 3.0, 2.3, 0.0, 0, 14, 2, 16, 1.25, 0.4, 0.7, 0.8, 0.4, 0.35, 0.18, Rgb(68, 90, 40), Rgb(86, 80, 68), false, true, LeafOval }, S + 14);
		BuildTree(At(Base::Apple), { 5.5, 1.2, 0.17, 0.8, 1.0, 3.6, 3.2, 1.45, 0.0, 4, 12, 3, 18, 1.3, 0.4, 0.68, 0.72, 0.38, 0.3, 0.22, Rgb(72, 96, 44), Rgb(92, 82, 70), false, false, LeafOval }, S + 15);
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
			// (on trial, "heads": the clump built above, on the shared draws, so everything built after it is as it was; then
			// let go and built again on its own draws. Its heads were paper, a pale triangle 3.6 cm across and 12 cm high on a
			// stalk 1 cm wide, the brightest thing in a meadow. Now fourteen leaves half as wide, every second one arched
			// over at its tip, one in five yellowing; and six flowering culms, thin, each nodding its head 10 to 35 degrees,
			// a head of one of four kinds: an open panicle, a dense spike, a feathery spray (the steppe's feather grass), a
			// spike of alternating spikelets. Painted dark for the grade, as the bracken and the dock were; each culm its
			// own value. The heads as high as before: the lens over the grass and the snow's depths read that height.)
			if (!Fd.Trial.empty() && Fd.Tried("heads"))
			{
				M = Mesh();
				Rng RH(S ^ 0x4EAD5);
				const auto FootOf = [&RH](double Spread) {
					const double A = RH.Range(0.0, 2.0 * Pi);
					const double D = Spread * std::sqrt(RH.Next());
					return V3{ D * std::cos(A), D * std::sin(A), 0.0 };
				};
				// (a way Spread off a head's axis, Round turned about it; and a breadth across a way, as level as it goes)
				const auto Off = [](V3 Axis, double Round, double Spread) {
					const V3 U = Unit3(Cross({ 0, 0, 1 }, Axis), { 1, 0, 0 });
					const V3 W = Cross(Axis, U);
					return Unit3(Axis * std::cos(Spread) + (U * std::cos(Round) + W * std::sin(Round)) * std::sin(Spread), Axis);
				};
				const auto Level = [](V3 Way, double Round) { return Unit3(Cross({ 0, 0, 1 }, Way), { -std::sin(Round), std::cos(Round), 0 }); };
				for (int K = 0; K < 14; ++K)
				{
					const V3 Foot = FootOf(0.12);
					const double A = RH.Range(0.0, 2.0 * Pi);
					const double High = RH.Range(0.45, 0.9);
					const double Out = RH.Range(0.25, 0.6);
					const V3 Lean{ std::cos(A) * High * Out, std::sin(A) * High * Out, 0 };
					std::vector<V3> P = Bent(Foot, Lean, High, 3);
					if (K % 2 == 1) P.back().Z -= 0.12 * High;
					const bool bDry = K % 5 == 2;
					Strip(M, P, { 0.0075, 0.006, 0.0035, 0.0 }, AcrossOf(Lean, A), bDry ? Rgb(92, 88, 52) : Rgb(64, 84, 44), bDry ? Rgb(150, 136, 84) : Rgb(112, 124, 72));
				}
				// (buff, straw, purple-grey and silver-green: a soft panicle purple-grey, a fresh spike silver-green, the rest
				// ripening buff and straw. The purple-grey only just warm of grey: redder, the grade made it pink.)
				const Col Paints[4] = { Rgb(150, 134, 92), Rgb(162, 146, 100), Rgb(122, 112, 106), Rgb(124, 134, 100) };
				constexpr int PaintOf[6] = { 2, 3, 0, 1, 0, 1 };
				// (their heights spread over 0.7 to 0.95 m a sixth each, the spike's the tallest: the clump stands to about a
				// metre, as before, and no two heads at one height)
				constexpr int Tier[6] = { 3, 5, 1, 4, 0, 2 };
				for (int K = 0; K < 6; ++K)
				{
					const V3 Foot = FootOf(0.08);
					const double A = RH.Range(0.0, 2.0 * Pi);
					const double High = 0.7 + 0.25 * (Tier[K] + RH.Next()) / 6.0;
					const double Nod = RH.Range(10.0, 35.0) * Pi / 180.0;
					const double L = RH.Range(0.92, 1.08);
					const V3 Out{ std::cos(A), std::sin(A), 0 };
					const V3 Side{ -Out.Y, Out.X, 0 };
					// (the culm up from the clump, leaning a little, bent over above its knee toward the nod; the head on along it)
					const V3 Knee = Foot + Out * (0.04 * High) + V3{ 0, 0, 0.6 * High };
					const V3 Neck = Knee + Out * (0.4 * High * std::sin(0.5 * Nod)) + V3{ 0, 0, 0.4 * High * std::cos(0.5 * Nod) };
					const V3 Dir = Out * std::sin(Nod) + V3{ 0, 0, std::cos(Nod) };
					const Col Tip = Varied(Paints[PaintOf[K]], L, 0.0), Foot0 = Varied(Paints[PaintOf[K]], 0.88 * L, 0.0);
					Strip(M, { Foot, Knee, Neck }, { 0.0022, 0.0018, 0.0014 }, Side, Rgb(84, 102, 54), Mixed(Rgb(118, 122, 76), Tip, 0.35));
					switch (K % 4)
					{
					case 0:
						// an open panicle: four branchlets from its neck, hair-fine, the lower two splayed, the upper two close in
						for (int Br = 0; Br < 4; ++Br)
						{
							const double Long = RH.Range(0.04, 0.07);
							const double Spread = (Br < 2 ? RH.Range(28.0, 42.0) : RH.Range(10.0, 20.0)) * Pi / 180.0;
							const double Round = A + Br * 0.5 * Pi + RH.Range(-0.3, 0.3);
							const V3 Way = Off(Dir, Round, Spread);
							Strip(M, { Neck, Neck + Way * Long }, { 0.0015, 0.0 }, Level(Way, Round), Foot0, Tip);
						}
						break;
					case 1:
					{
						// a dense spike: two strips crossed along it, near even all its length (a diamond read as a paper kite)
						const double Long = RH.Range(0.06, 0.1);
						const V3 Mid = Neck + Dir * (0.5 * Long), End = Neck + Dir * Long;
						Strip(M, { Neck, Mid, End }, { 0.003, 0.0034, 0.0012 }, Side, Foot0, Tip);
						Strip(M, { Neck, Mid, End }, { 0.003, 0.0034, 0.0012 }, Unit3(Cross(Dir, Side), Out), Foot0, Tip);
						break;
					}
					case 2:
						// a feathery spray: five hairs fanned out from its neck
						for (int Hair = 0; Hair < 5; ++Hair)
						{
							const double Long = RH.Range(0.05, 0.09);
							const double Spread = RH.Range(6.0, 22.0) * Pi / 180.0;
							const double Round = A + Hair * 0.4 * Pi + RH.Range(-0.25, 0.25);
							const V3 Way = Off(Dir, Round, Spread);
							Strip(M, { Neck, Neck + Way * Long }, { 0.001, 0.0 }, Level(Way, Round), Foot0, Tip);
						}
						break;
					default:
					{
						// spikelets set alternately either side of its stem, edge on
						const double Long = RH.Range(0.07, 0.11);
						std::vector<V3> Spine;
						std::vector<double> Half;
						for (int Q = 0; Q < 5; ++Q)
						{
							const double Zig = Q == 1 ? 0.003 : Q == 3 ? -0.003 : 0.0;
							Spine.push_back(Neck + Dir * (Long * Q / 4.0) + Side * Zig);
							Half.push_back(Q % 2 == 1 ? 0.0034 : 0.0015);
						}
						Strip(M, Spine, Half, Side, Foot0, Tip);
						break;
					}
					}
				}
			}
		}
		// Ripe corn: three dozen stalks, about a metre high (the field stands them 1.2 to 1.4 m to the ears), each with its
		// ear, leaning a little with the wind; each stalk drawn is a few stalks seen together. (The environment review, 22
		// November: the floor showed between the stalks a few metres from the eye, and the ears stood about 1.1 m.)
		{
			Mesh& M = At(Base::Wheat).Near;
			const V3 Wind{ 0.06, 0.02, 0 };
			for (int K = 0; K < 36; ++K)
			{
				const V3 Foot = Around(0.36);
				const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.9, 1.04);
				const V3 Lean = Wind + V3{ R.Range(-0.04, 0.04), R.Range(-0.04, 0.04), 0 };
				const V3 Crest = Foot + Lean + V3{ 0, 0, High };
				const V3 Across{ std::cos(A), std::sin(A), 0 };
				// (each stalk's own value, 12% either way, and the crop darker toward the soil: the art director's cut 4)
				const double L = R.Range(0.88, 1.12);
				// (painted warm, as the field's corn is: the grade drains it; the art director's cut 5, item 4)
				Strip(M, { Foot, Foot + Lean * 0.3 + V3{ 0, 0, High * 0.5 }, Crest }, { 0.007, 0.006, 0.005 }, Across, Varied(Rgb(139, 118, 36), 0.5 * L, 0.0),
					  Varied(Rgb(202, 152, 48), L, 0.0));
				const V3 EarDir = Unit3(Lean + V3{ 0, 0, 0.6 });
				Strip(M, { Crest, Crest + EarDir * 0.05, Crest + EarDir * 0.1 }, { 0.012, 0.016, 0.004 }, Across, Varied(Rgb(208, 158, 52), L, 0.0),
					  Varied(Rgb(222, 170, 58), L, 0.0));
			}
		}
		// The same corn near the lens, stalk by stalk (the art director's cut 4, fix 4: at 1.2 m a bundle of stalks read
		// as one plank and its ear as a lozenge): single stalks over the bundle's ground, each 3 to 4 mm across
		// with two or three leaves 8 to 12 mm wide hanging off it, bent and some yellowed, and its ear 8 to 10 cm long and
		// 1.3 cm wide, two zigzag rows of spikelets seen two ways (no awns: wheat); straw gold at the ear, greener and darker
		// down the stem, each stalk's value its own, darkening to 0.4 at the soil. Four dozen to a clump (each bundle's ground,
		// with its own scale and tint). Drawn on the stalks' foliage ink, within
		// a few metres of the lens only.
		{
			Mesh& M = At(Base::Stalks).Near;
			const V3 Wind{ 0.06, 0.02, 0 };
			for (int K = 0; K < 48; ++K)
			{
				const V3 Foot = Around(0.36);
				const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.9, 1.04), L = R.Range(0.88, 1.12);
				const V3 Lean = Wind + V3{ R.Range(-0.04, 0.04), R.Range(-0.04, 0.04), 0 };
				const V3 Mid = Foot + Lean * 0.3 + V3{ 0, 0, High * 0.5 };
				const V3 Crest = Foot + Lean + V3{ 0, 0, High };
				const V3 Across{ std::cos(A), std::sin(A), 0 };
				const Col Soil = Varied(Rgb(146, 130, 45), 0.4 * L, 0.0), Green = Varied(Rgb(146, 130, 45), L, 0.0), Straw = Varied(Rgb(197, 150, 52), L, 0.0);
				Strip(M, { Foot, Foot + Lean * 0.08 + V3{ 0, 0, High * 0.25 }, Mid }, { 0.0019, 0.0018, 0.0017 }, Across, Soil, Green);
				Strip(M, { Mid, Foot + Lean * 0.65 + V3{ 0, 0, High * 0.78 }, Crest }, { 0.0017, 0.0016, 0.0015 }, Across, Green, Straw);
				// (its leaves from the nodes, out and drooping, the lower ones darker and more of them yellowed)
				const int Leaves = R.Next() < 0.5 ? 2 : 3;
				for (int Lf = 0; Lf < Leaves; ++Lf)
				{
					const double NodeH = (0.22 + 0.2 * Lf + R.Range(-0.04, 0.04)) * High, Long = R.Range(0.15, 0.25), Wide = R.Range(0.004, 0.006);
					const double Turn = A + R.Range(0.6, 2.6) * (Lf % 2 == 0 ? 1.0 : -1.0);
					const V3 Out{ std::cos(Turn), std::sin(Turn), 0 };
					const V3 Node = Foot + Lean * (NodeH / High) + V3{ 0, 0, NodeH };
					const double Dark = 0.4 + 0.6 * NodeH / High;
					const Col Leaf = Mixed(Rgb(121, 120, 36), Rgb(193, 152, 52), R.Range(0.0, 0.7 - 0.4 * NodeH / High));
					Strip(M, { Node, Node + Out * (0.35 * Long) + V3{ 0, 0, 0.2 * Long }, Node + Out * (0.75 * Long) + V3{ 0, 0, 0.18 * Long }, Node + Out * Long - V3{ 0, 0, 0.06 * Long } },
						  { 0.25 * Wide, Wide, 0.75 * Wide, 0.0 }, Unit3(Cross({ 0, 0, 1 }, Out), Across), Varied(Leaf, Dark * L * 0.9, 0.0), Varied(Leaf, Dark * L, 0.0));
				}
				// (its ear: two zigzag strips crossed, the spikelets' rows)
				const V3 EarDir = Unit3(Lean + V3{ 0, 0, 0.6 });
				const double EarLong = R.Range(0.08, 0.1);
				for (int Way = 0; Way < 2; ++Way)
				{
					const V3 EarAcross = Unit3(Way == 0 ? Across : Cross(EarDir, Across), Across);
					std::vector<V3> Spine;
					std::vector<double> Half;
					for (int Spk = 0; Spk <= 6; ++Spk)
					{
						const double T = Spk / 6.0;
						Spine.push_back(Crest + EarDir * (EarLong * T));
						Half.push_back(Spk == 6 ? 0.0015 : Spk % 2 == 0 ? 0.0065 : 0.0045);
					}
					Strip(M, Spine, Half, EarAcross, Varied(Rgb(208, 156, 54), L, 0.0), Varied(Rgb(220, 168, 59), L, 0.0));
				}
			}
		}
		// Poppies: four stems, one flower to a stem, a cup of four orange-red petals 5 to 8 cm across round a dark middle
		// (the art director's cut 4: three flat red hearts on one stem read as nothing like a poppy); two buds nodding on
		// their crooked stems; a few leaves.
		{
			Mesh& M = At(Base::Poppies).Near;
			for (int K = 0; K < 4; ++K)
			{
				const V3 Foot = Around(0.12);
				const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.42, 0.68), Cup = R.Range(0.025, 0.04);
				const V3 Lean{ R.Range(-0.06, 0.06), R.Range(-0.06, 0.06), 0 };
				const V3 Head = Foot + Lean + V3{ 0, 0, High };
				Strip(M, { Foot, Head }, { 0.0022, 0.0018 }, { std::cos(A), std::sin(A), 0 }, Rgb(64, 86, 40), Rgb(92, 112, 56));
				for (int P = 0; P < 4; ++P)
				{
					const double Pa = A + P * Pi / 2 + R.Range(-0.15, 0.15);
					const V3 Out{ std::cos(Pa), std::sin(Pa), 0 };
					const V3 Side = Unit3(Cross({ 0, 0, 1 }, Out), Out);
					Strip(M, { Head, Head + Out * (0.45 * Cup) + V3{ 0, 0, 0.35 * Cup }, Head + Out * (0.85 * Cup) + V3{ 0, 0, 0.9 * Cup } }, { 0.15 * Cup, 0.55 * Cup, 0.45 * Cup }, Side,
						  Rgb(150, 26, 20), Varied(Rgb(200, 40, 30), R.Range(0.92, 1.06), 0.0));
				}
				for (int P = 0; P < 2; ++P)
				{
					const V3 Across{ std::cos(A + P * Pi / 2), std::sin(A + P * Pi / 2), 0 };
					Strip(M, { Head + V3{ 0, 0, 0.004 }, Head + V3{ 0, 0, 0.016 } }, { 0.006, 0.005 }, Across, Rgb(34, 30, 28), Rgb(52, 46, 40));
				}
			}
			for (int K = 0; K < 2; ++K)
			{
				const V3 Foot = Around(0.12);
				const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.36, 0.5);
				const V3 Over{ std::cos(A) * 0.06, std::sin(A) * 0.06, 0 };
				const V3 Top = Foot + Over * 0.5 + V3{ 0, 0, High };
				const V3 Hook = Top + Over + V3{ 0, 0, -0.03 };
				Strip(M, { Foot, Top, Hook }, { 0.002, 0.0018, 0.0015 }, AcrossOf(Over, A), Rgb(64, 86, 40), Rgb(90, 110, 54));
				Strip(M, { Hook, Hook - V3{ 0, 0, 0.012 }, Hook - V3{ 0, 0, 0.024 } }, { 0.004, 0.007, 0.003 }, AcrossOf(Over, A), Rgb(88, 108, 56), Rgb(104, 118, 62));
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
			// (on trial, "heads": built above on the shared draws, as the tall grass is, then built again on its own. The
			// daisies were two crossed paper squares 6.4 cm across. Now each ox-eye daisy twelve rays 1.3 to 1.7 cm round a
			// domed yellow middle, its rays tilted 10 degrees up; each buttercup five petals set round its heart in a cup,
			// each petal from the heart out to its broad rim (a wedge: two faces of one triangle, so the flowers keep under
			// 190 triangles). Stems and leaves as they were.)
			if (!Fd.Trial.empty() && Fd.Tried("heads"))
			{
				M = Mesh();
				Rng RF(S ^ 0xF10E5);
				const auto FootOf = [&RF](double Spread) {
					const double A = RF.Range(0.0, 2.0 * Pi);
					const double D = Spread * std::sqrt(RF.Next());
					return V3{ D * std::cos(A), D * std::sin(A), 0.0 };
				};
				const double RayUp = 10.0 * Pi / 180.0, CupUp = 48.0 * Pi / 180.0;
				for (int K = 0; K < 6; ++K)
				{
					const bool bDaisy = K < 3;
					const V3 Foot = FootOf(0.14);
					const double A = RF.Range(0.0, 2.0 * Pi);
					const double High = bDaisy ? RF.Range(0.38, 0.6) : RF.Range(0.25, 0.45);
					const double DX = RF.Range(-0.05, 0.05);
					const double DY = RF.Range(-0.05, 0.05);
					const V3 Head = Foot + V3{ DX, DY, High };
					Strip(M, { Foot, Head }, { 0.003, 0.002 }, { std::cos(A), std::sin(A), 0 }, Rgb(76, 98, 46), Rgb(90, 110, 54));
					if (bDaisy)
					{
						for (int Ray = 0; Ray < 12; ++Ray)
						{
							const double Long = RF.Range(0.013, 0.017);
							const double Round = A + Ray * Pi / 6.0 + RF.Range(-0.1, 0.1);
							const V3 Out{ std::cos(Round), std::sin(Round), 0 };
							const V3 Root = Head + Out * 0.005 + V3{ 0, 0, 0.003 };
							Strip(M, { Root, Root + (Out * std::cos(RayUp) + V3{ 0, 0, std::sin(RayUp) }) * Long }, { 0.0035, 0.0 }, { -Out.Y, Out.X, 0 }, Rgb(214, 212, 198), Rgb(214, 212, 198));
						}
						// (its middle a low dome 1.6 cm across, arched over one way)
						const V3 Ax{ std::cos(A), std::sin(A), 0 };
						const V3 Mid = Head + V3{ 0, 0, 0.004 };
						Strip(M, { Mid - Ax * 0.008, Mid + V3{ 0, 0, 0.004 }, Mid + Ax * 0.008 }, { 0.0055, 0.008, 0.0055 }, { -Ax.Y, Ax.X, 0 }, Rgb(214, 170, 36), Rgb(214, 170, 36));
					}
					else
					{
						const V3 Heart = Head + V3{ 0, 0, 0.002 };
						for (int Pt = 0; Pt < 5; ++Pt)
						{
							const double Long = RF.Range(0.009, 0.011);
							const double Round = A + Pt * 0.4 * Pi + RF.Range(-0.1, 0.1);
							const V3 Out{ std::cos(Round), std::sin(Round), 0 };
							const V3 Rim = Heart + (Out * std::cos(CupUp) + V3{ 0, 0, std::sin(CupUp) }) * Long;
							Strip(M, { Rim, Heart }, { 0.0055, 0.0 }, { -Out.Y, Out.X, 0 }, Rgb(222, 192, 40), Rgb(196, 160, 32));
						}
					}
				}
				for (int K = 0; K < 4; ++K)
				{
					const V3 Foot = FootOf(0.12);
					const double A = RF.Range(0.0, 2.0 * Pi);
					const V3 Lean{ std::cos(A) * 0.1, std::sin(A) * 0.1, 0 };
					Strip(M, Bent(Foot, Lean, 0.12, 2), { 0.015, 0.02, 0.0 }, AcrossOf(Lean, A), Rgb(68, 92, 42), Rgb(88, 110, 52));
				}
			}
		}
		// A stone of the field: a weathered lump of grey limestone or flint, more than half sunk, earth-stained and greened
		// at its foot, never pale (sRGB some 105 to 135, a little warm).
		Blob(At(Base::Stone).Near, 1, { 0, 0, -0.045 }, { 0.17, 0.13, 0.09 }, 0.32, 1.8, S + 50, [](V3 P, V3 Dir) {
			const double Grain = Noise3(P.X * 30.0, P.Y * 30.0, P.Z * 30.0, 0x5703);
			const Col Grey = Varied(Rgb(124, 120, 112), (0.92 + 0.1 * Dir.Z) * (1.0 + 0.08 * Grain), 0.02 + 0.02 * Grain);
			return Mixed(Grey, Rgb(84, 86, 64), 0.4 * Clamp01((0.8 - Dir.Z) / 0.35)); // the ground meets it at about half its height
		});
		// Bracken (the art director's cut 5, item 6: the six broad strips of the old one read as flat mint cards, paper):
		// three fronds from the ground, each a stem rising and its blade held out near level, a triangle of leaflets in
		// pairs along its midrib, longest at its foot, each leaflet folded up along its own midrib (two faces meeting in a
		// crease, so it shades itself), dark green above with a paler midrib and paler, greyer beneath. Each frond its own
		// green, one in four going yellow toward its tips. Painted dark and warm for the grade (lit L 90 to 120).
		{
			Mesh& M = At(Base::Fern).Near;
			constexpr int Fronds = 3, Pairs = 6;
			const double Turn0 = R.Range(0.0, 2.0 * Pi);
			for (int K = 0; K < Fronds; ++K)
			{
				const double A = Turn0 + K * 2.0 * Pi / Fronds + R.Range(-0.45, 0.45);
				const V3 Out{ std::cos(A), std::sin(A), 0 };
				const V3 Side{ -Out.Y, Out.X, 0 };
				const double Stem = R.Range(0.26, 0.46), Blade = R.Range(0.5, 0.7), L = R.Range(0.84, 1.1);
				const bool bYellow = R.Next() < 0.25;
				const Col Dark = Varied(Rgb(72, 92, 28), L, 0.0);
				const Col Light = Varied(bYellow ? Rgb(130, 122, 34) : Rgb(90, 110, 30), L, 0.0);
				const Col Under = Varied(Rgb(122, 130, 90), L, 0.0), Vein = Rgb(128, 136, 70);
				// the stem up from the ground, leaning out; then the midrib out, rising a little and drooping to its tip
				const V3 Foot = Out * R.Range(0.0, 0.06);
				const V3 Knee = Foot + Out * (0.14 * Stem) + V3{ 0, 0, Stem };
				std::vector<V3> Rib;
				for (int Q = 0; Q <= 4; ++Q)
				{
					const double T = Q / 4.0;
					Rib.push_back(Knee + Out * (Blade * T) + V3{ 0, 0, 0.1 * Blade * std::sin(T * Pi * 0.7) - 0.24 * Blade * T * T });
				}
				Strip(M, { Foot, Foot + Out * (0.04 * Stem) + V3{ 0, 0, 0.6 * Stem }, Knee }, { 0.0045, 0.004, 0.0035 }, Side, Rgb(74, 58, 34), Rgb(92, 96, 40));
				Strip(M, Rib, { 0.0032, 0.0028, 0.0022, 0.0016, 0.001 }, Side, Rgb(92, 96, 40), Rgb(100, 108, 40));
				const auto RibAt = [&Rib](double T, V3& Along) {
					const double U = std::clamp(T, 0.0, 1.0) * 4.0;
					const size_t I = std::min<size_t>(3, static_cast<size_t>(U));
					Along = Unit3(Rib[I + 1] - Rib[I], V3{ 1, 0, 0 });
					return Rib[I] + (Rib[I + 1] - Rib[I]) * (U - static_cast<double>(I));
				};
				const auto Leaflet = [&](V3 From, V3 Dir, double Long, double Shade) {
					const V3 Flat = Unit3(Cross(V3{ 0, 0, 1 }, Dir), Side);
					const double W = Long * R.Range(0.15, 0.18);
					const std::vector<V3> Line = { From, From + Dir * (0.5 * Long) + V3{ 0, 0, 0.03 * Long }, From + Dir * Long - V3{ 0, 0, 0.14 * Long } };
					Folded(M, Line, { 0.55 * W, W, 0.0 }, Flat, R.Range(0.38, 0.55), Varied(Dark, Shade, 0.0), Varied(Light, Shade, 0.0), Under, Vein);
				};
				for (int Pr = 0; Pr < Pairs; ++Pr)
				{
					const double T = (Pr + 0.35) / (Pairs + 0.4);
					V3 Along;
					const V3 On = RibAt(T, Along);
					const double Long = Blade * 0.42 * (1.0 - 0.72 * T) * R.Range(0.9, 1.1);
					for (int Sd = -1; Sd <= 1; Sd += 2)
					{
						const V3 Dir = Unit3(Side * (0.85 * Sd) + Along * 0.55 + V3{ 0, 0, R.Range(-0.05, 0.08) });
						Leaflet(On, Dir, Long, R.Range(0.92, 1.06));
					}
				}
				// (its tip: one leaflet on along the midrib)
				V3 Along;
				const V3 Tip = RibAt(1.0, Along);
				Leaflet(Tip - Along * 0.02, Along, Blade * 0.1, 1.04);
			}
		}
		// A dock and two plantains (the art director's cut 5 note: the bracken keeps to the hedge banks; out in the open
		// grass a low flat rosette lying close to the ground). The dock: seven broad oblong leaves 16 to 30 cm long from its
		// crown, the outer five arched low and lying out flat at their tips, the inner two held up, their edges waved, a
		// red-brown midrib in some, one outer leaf going yellow. The plantains beside it: five broad leaves each, flat to the
		// ground, paler, and a slender seed spike or two. Painted dark and warm for the grade, as the bracken; its own
		// random draws, so the meshes built after it are as they were.
		{
			Mesh& M = At(Base::Rosette).Near;
			Rng RR(S ^ 0xD0C7);
			const auto Leaf = [&M, &RR](V3 Crown, double A, double Long, double Wide, double Rise, double Lift, const Col& Foot, const Col& Tip, const Col& Under, const Col& Rib) {
				const V3 Out{ std::cos(A), std::sin(A), 0 };
				const V3 Side{ -Out.Y, Out.X, 0 };
				std::vector<V3> Spine;
				std::vector<double> Half;
				constexpr int N = 5;
				for (int Q = 0; Q < N; ++Q)
				{
					const double T = Q / static_cast<double>(N - 1);
					// (up off the crown and down to the ground at its tip; narrow at its stalk, broadest past its middle)
					Spine.push_back(Crown + Out * (Long * T) + V3{ 0, 0, Long * Rise * std::sin(T * Pi) + 0.004 });
					Half.push_back(Q + 1 < N ? Wide * (0.25 + 0.75 * std::sin(T * Pi * 0.92)) * RR.Range(0.86, 1.12) : 0.0);
				}
				Folded(M, Spine, Half, Side, Lift, Foot, Tip, Under, Rib);
			};
			const double Turn0 = RR.Range(0.0, 2.0 * Pi);
			for (int K = 0; K < 7; ++K)
			{
				const bool bInner = K >= 5, bYellow = K == 1;
				const double A = bInner ? Turn0 + 0.6 + (K - 5) * Pi + RR.Range(-0.4, 0.4) : Turn0 + K * 2.0 * Pi / 5.0 + RR.Range(-0.3, 0.3);
				const double Long = bInner ? RR.Range(0.16, 0.22) : RR.Range(0.22, 0.3), Wide = Long * RR.Range(0.19, 0.24);
				const double L = RR.Range(0.9, 1.08);
				const Col Foot = Varied(Rgb(60, 82, 28), L, 0.0), Tip = Varied(bYellow ? Rgb(118, 108, 36) : Rgb(80, 102, 32), L, 0.0);
				const Col Rib = K % 3 == 0 ? Rgb(118, 72, 46) : Rgb(104, 114, 54);
				// (each draw into its own name first: a call's arguments are taken in no set order, and g++ and clang take
				// them differently, so the cloud's dock and the Mac's would differ leaf by leaf. The pre-check, 22 November)
				const double Rise = bInner ? RR.Range(0.35, 0.5) : RR.Range(0.12, 0.2);
				const double Lift = RR.Range(0.12, 0.22);
				Leaf({ 0, 0, 0 }, A, Long, Wide, Rise, Lift, Foot, Tip, Rgb(112, 122, 80), Rib);
			}
			for (int Pl = 0; Pl < 2; ++Pl)
			{
				const double At0 = Turn0 + 1.4 + Pl * 2.6 + RR.Range(-0.5, 0.5), Off = RR.Range(0.2, 0.3);
				const V3 Crown{ Off * std::cos(At0), Off * std::sin(At0), 0 };
				const double Turn = RR.Range(0.0, 2.0 * Pi), L = RR.Range(0.92, 1.08);
				for (int K = 0; K < 5; ++K)
				{
					const double Long = RR.Range(0.08, 0.13);
					const double A = Turn + K * 2.0 * Pi / 5.0 + RR.Range(-0.25, 0.25);
					const double Wide = Long * RR.Range(0.3, 0.36);
					const double Rise = RR.Range(0.05, 0.1);
					Leaf(Crown, A, Long, Wide, Rise, 0.1, Varied(Rgb(76, 98, 34), L, 0.0), Varied(Rgb(92, 112, 38), L, 0.0), Rgb(118, 128, 84), Rgb(116, 124, 70));
				}
				for (int Spike = 0; Spike < 1 + Pl; ++Spike)
				{
					const double A = RR.Range(0.0, 2.0 * Pi), High = RR.Range(0.12, 0.2);
					const V3 Head = Crown + V3{ RR.Range(-0.03, 0.03), RR.Range(-0.03, 0.03), High };
					Strip(M, { Crown, Crown + (Head - Crown) * 0.6, Head }, { 0.0018, 0.0016, 0.0028 }, { std::cos(A), std::sin(A), 0 }, Rgb(86, 100, 42), Rgb(112, 102, 62));
				}
			}
		}
		// A barley stubble: the binder cut it a hand's breadth up, so its stalks stand 8 to 15 cm in the drill rows, 17 cm
		// apart, pale and hollow, their feet in shade; and a few straws the binder dropped lie loose along the rows.
		// (1.4 m along seven rows; the field turns it to its own rows. The art director, 22 November: "straw rows over
		// brown earth", the third cut's stubble a flat pale sheet.) Each stalk drawn is a few stalks seen together.
		{
			Mesh& M = At(Base::Stubble).Near;
			constexpr int Rows = 7, Stalks = 11;
			constexpr double Apart = 0.17, Along = 1.4;
			for (int Row = 0; Row < Rows; ++Row)
			{
				const double Y = (Row - 0.5 * (Rows - 1)) * Apart + R.Range(-0.015, 0.015);
				for (int K = 0; K < Stalks; ++K)
				{
					const double X = ((K + R.Range(0.15, 0.85)) / Stalks - 0.5) * Along;
					const V3 Foot{ X, Y + R.Range(-0.012, 0.012), -0.01 };
					const double High = R.Range(0.08, 0.15), A = R.Range(0.0, Pi);
					const V3 Lean{ R.Range(-0.02, 0.02), R.Range(-0.02, 0.02), 0 };
					const double VaryHue = R.Range(-0.03, 0.03);
					const double VaryTone = R.Range(0.9, 1.08);
					Strip(M, { Foot, Foot + Lean + V3{ 0, 0, High } }, { 0.008, 0.005 }, { std::cos(A), std::sin(A), 0 },
						  Rgb(112, 98, 66), Varied(Rgb(178, 160, 110), VaryTone, VaryHue));
				}
			}
			for (int K = 0; K < 8; ++K)
			{
				// a loose straw: lying along the rows, give or take, a little off the ground at one end
				const double A = R.Range(-0.5, 0.5), Long = R.Range(0.18, 0.4);
				const V3 Mid{ R.Range(-0.6, 0.6), R.Range(-0.55, 0.55), 0.005 };
				const V3 Way{ std::cos(A), std::sin(A), 0 };
				Strip(M, { Mid - Way * (0.5 * Long), Mid + Way * (0.5 * Long) + V3{ 0, 0, 0.02 } }, { 0.006, 0.005 }, { -Way.Y, Way.X, 0 }, Rgb(150, 134, 92),
					  Rgb(184, 166, 116));
			}
		}
		// The sward: the grass itself, a hand high, three dozen blades in a patch, grazed short. Built grey, so that its
		// tint (the ground's own colour where it grows, Clutter) is its colour: the feet in shade, the tips lit, a few dead
		// and straw-coloured. Close round the eye only (25 m), where the painted ground alone read as a flat green felt
		// with tufts stuck on it (Brian, 22 November: "i want the grass and bushes and plants to look more realistic").
		{
			Mesh& M = At(Base::Sward).Near;
			for (int K = 0; K < 36; ++K)
			{
				const V3 Foot = Around(0.42);
				const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.05, 0.14);
				const V3 Lean{ std::cos(A) * High * R.Range(0.15, 0.55), std::sin(A) * High * R.Range(0.15, 0.55), 0 };
				const bool bDead = R.Next() < 0.1;
				const double L = R.Range(0.88, 1.12);
				const Col Tip = bDead ? Rgb(148, 138, 108) : Varied(Rgb(132, 138, 124), L, 0.0);
				Strip(M, { Foot - V3{ 0, 0, 0.01 }, Foot + Lean * 0.4 + V3{ 0, 0, High * 0.55 }, Foot + Lean + V3{ 0, 0, High } }, { 0.008, 0.006, 0.0 }, AcrossOf(Lean, A),
					  Varied(Rgb(84, 88, 80), L, 0.0), Tip);
			}
		}
		for (const Base B : { Base::Tuft, Base::TallGrass, Base::Wheat, Base::Poppies, Base::Flowers, Base::Stone, Base::Fern, Base::Stubble }) At(B).FarFrom = 70.0f;
		// (low and flat: the grass hides it well before the tufts)
		At(Base::Rosette).FarFrom = 40.0f;
		At(Base::Sward).FarFrom = 25.0f;
		// (the corn stalk by stalk only near the lens: its ink fades it out from 3.5 to 5 m, and past 5.5 m it is not drawn)
		At(Base::Stalks).FarFrom = 5.5f;
		// The steppe's, from the east kit (each file's text in Fd.Steppe.Kit); plain stand-ins for a model not given.
		for (const Base B : { Base::Khata, Base::Chimney, Base::Pole, Base::Wattle, Base::Sweep, Base::Haystack, Base::Birch })
		{
			BaseMesh& M = At(B);
			const auto Found = Fd.Steppe.Kit.find(LandSteppe::KitName(B));
			std::string Error;
			const double FarFrom = B == Base::Wattle ? 60.0 : B == Base::Pole ? 110.0 : 150.0;
			if (Found == Fd.Steppe.Kit.end() || !LandSteppe::ReadKit(Found->second, FarFrom, M, Error))
			{
				M.Near = Mesh();
				M.Far = Mesh();
				LandSteppe::PlainKit(B, M);
			}
		}
		// (the birch's bark made over on trial "birchbark": off-white, short marks, a dark foot)
		if (Fd.Tried("birchbark"))
		{
			BaseMesh& M = At(Base::Birch);
			LandSteppe::BirchBark(M.Near, true);
			LandSteppe::BirchBark(M.Mid, false);
			LandSteppe::BirchBark(M.Far, false);
		}
		// A haycock of the bocage's mown meadows (July 1944: the hay cut in June, forked into cocks to make before it is
		// carted): a beehive of hay 1.4 m high and 1.7 m across, settled a little wider at its foot, rounded to its top.
		// Built up in layers of forkfuls, each layer's foot hanging over the one below like thatch (a ragged lip and a
		// shaded step under it), its straw combed down the sides in locks; its outer hay bleached by the sun and the rain,
		// straw and gold on its sides, greyer on its crown where the weather took it, darker at its foot where it sits on
		// the aftermath; a twisted, ragged topknot; a few loose wisps round its foot. Painted warm for the grade, lit about
		// 150 140 95. (The art director's cut 5, item 5: smooth olive domes, like tarps or mossy rocks, with white quills
		// stuck in them.) Beyond 150 m a plain one of few faces.
		{
			BaseMesh& Bm = At(Base::Haycock);
			constexpr double High = 1.45, Wide = 0.86, Layer = 0.2;
			auto Hive = [](double T) {
				// (its profile: a little tuck at the ground, widest a fifth of the way up, then rounding over to the top)
				return Wide * std::pow(std::max(0.0, 1.0 - std::pow(T, 2.2)), 0.55) * (1.0 + 0.07 * std::sin(Pi * std::min(1.0, T * 2.2)));
			};
			// (where in its layer a height is, 0 at a layer's lip to 1 just under the next one's; the lips wander up and
			// down round it, as forkfuls do)
			auto InLayer = [](double T, double A) {
				const double Z = T * High - 0.09 * Noise3(std::cos(A) * 1.7, std::sin(A) * 1.7, T * 3.0, 0x4A1F) - 0.035 * Noise3(std::cos(A) * 5.0, std::sin(A) * 5.0, T * 7.0, 0x4A20);
				return Z / Layer - std::floor(Z / Layer);
			};
			auto Radius = [&](double T, double A, bool bLayers) {
				const double Rough = 1.0 + 0.04 * Noise3(std::cos(A) * 2.6, std::sin(A) * 2.6, T * 5.0, 0x4A1B) + 0.025 * Noise3(std::cos(A) * 9.0, std::sin(A) * 9.0, T * 12.0, 0x4A1E);
				// (a layer stands out most at its lip and tucks in under the next; less so toward the crown)
				const double Strong = 0.55 + 0.45 * Noise3(std::cos(A) * 3.0, std::sin(A) * 3.0, T * 6.0, 0x4A22);
				const double Lip = bLayers ? 0.07 * Strong * std::pow(1.0 - InLayer(T, A), 1.6) * (1.0 - 0.6 * T) : 0.0;
				return Hive(T) * Rough * (1.0 + Lip);
			};
			auto HayPaint = [&](double T, double A, double Bright) {
				const double Patch = Noise3(std::cos(A) * 1.8, std::sin(A) * 1.8, T * 2.5, 0x4A1D);
				// (combed: streaks running down it, a lock's width apart)
				const double Comb = Noise3(std::cos(A) * 5.0, std::sin(A) * 5.0, T * 1.2, 0x4A1C);
				const double Forkful = Noise3(std::cos(A) * 0.9, std::sin(A) * 0.9, std::floor(T * High / Layer) * 1.7, 0x4A21);
				Col C = Mixed(Rgb(152, 130, 62), Rgb(171, 146, 71), Clamp01(0.5 + 0.9 * Patch)); // straw and gold
				C = Mixed(C, Rgb(128, 120, 58), 0.25 * Smooth(0.2, 0.55, -Patch));                  // a little greener hay
				C = Mixed(C, Rgb(138, 126, 86), 0.45 * Smooth(0.55, 0.95, T));                      // the crown weathered grey
				C = Mixed(C, Rgb(100, 92, 50), Smooth(0.2, 0.02, T));                               // its foot, damp
				return Varied(C, Bright * (1.0 + 0.1 * Comb + 0.06 * Forkful), 0.02 * Comb);
			};
			auto Lathe = [&](Mesh& M, int Rings, int Round, bool bLayers) {
				Builder Bd(M);
				const uint32_t Base0 = static_cast<uint32_t>(M.Points());
				auto Point = [&](double T, double A) {
					const double Rad = Radius(T, A, bLayers);
					return V3{ std::cos(A) * Rad, std::sin(A) * Rad, T * High - 0.04 };
				};
				for (int J = 0; J <= Rings; ++J)
					for (int K = 0; K <= Round; ++K)
					{
						const double T = std::min(0.995, static_cast<double>(J) / Rings), A = 2.0 * Pi * K / Round;
						const double DT = 0.5 / Rings, DA = 2.0 * Pi / Round * 0.5;
						const V3 Up = Point(std::min(1.0, T + DT), A) - Point(std::max(0.0, T - DT), A);
						const V3 Round1 = Point(T, A + DA) - Point(T, A - DA);
						const V3 N = Unit3(Cross(Round1, Up), V3{ std::cos(A), std::sin(A), 0 });
						// (the step under each layer's lip in its shade, the lip itself catching the light)
						const double Under = bLayers ? 0.8 + 0.2 * Smooth(0.95, 0.55, InLayer(T, A)) : 1.0;
						Bd.Vert(Point(T, A), N, HayPaint(T, A, (0.92 + 0.08 * N.Z) * Under));
					}
				for (int J = 0; J < Rings; ++J)
					for (int K = 0; K < Round; ++K)
					{
						const uint32_t A0 = Base0 + static_cast<uint32_t>(J * (Round + 1) + K), A1 = A0 + 1;
						const uint32_t B0 = A0 + static_cast<uint32_t>(Round + 1), B1 = B0 + 1;
						Bd.Tri(A0, A1, B1);
						Bd.Tri(A0, B1, B0);
					}
				// (its top: a small cap over the last ring)
				const uint32_t Top0 = Bd.Vert({ 0, 0, High - 0.04 + 0.01 }, { 0, 0, 1 }, HayPaint(1.0, 0.0, 1.0));
				for (int K = 0; K < Round; ++K)
				{
					const uint32_t A0 = Base0 + static_cast<uint32_t>(Rings * (Round + 1) + K);
					Bd.Tri(A0, A0 + 1, Top0);
				}
			};
			Lathe(Bm.Near, 30, 28, true);
			Lathe(Bm.Far, 5, 10, false);
			Rng Rh(S + 61);
			// Locks of straw combed down over the lips, lying on it (their faces lit as its side is, not as blades: the old
			// straws standing out of it, tipped to the sky, lit white)
			auto Lock = [&](const std::vector<V3>& Spine, double Half, V3 Across, const Col& Top, const Col& Foot) {
				Builder Bd(Bm.Near);
				for (int Side = 0; Side < 2; ++Side)
				{
					const uint32_t Base0 = static_cast<uint32_t>(Bm.Near.Points());
					V3 Face{ 0, 0, 1 };
					for (size_t K = 0; K < Spine.size(); ++K)
					{
						const V3 Out = Unit3(V3{ Spine[K].X, Spine[K].Y, 0.0 } + V3{ 0, 0, 0.3 + 0.5 * Spine[K].Z / High }, V3{ 0, 0, 1 });
						Face = Side == 0 ? Out : Out * -1.0;
						const double W = Half * (K + 1 == Spine.size() ? 0.35 : 1.0);
						const Col C = Mixed(Top, Foot, static_cast<double>(K) / static_cast<double>(Spine.size() - 1));
						Bd.Vert(Spine[K] - Across * W, Face, C);
						Bd.Vert(Spine[K] + Across * W, Face, C);
					}
					for (size_t K = 0; K + 1 < Spine.size(); ++K)
					{
						const uint32_t A0 = Base0 + 2 * static_cast<uint32_t>(K), A1 = A0 + 1, B0 = A0 + 2, B1 = A0 + 3;
						TriFacing(Bm.Near, A0, A1, B1, Face);
						TriFacing(Bm.Near, A0, B1, B0, Face);
					}
				}
			};
			for (int K = 0; K < 40; ++K)
			{
				// (from high in a layer down over its lip, and a little past it)
				const double A = Rh.Range(0.0, 2.0 * Pi), Down = Rh.Range(1.0, 5.0);
				const double Lip = Down * Layer + Rh.Range(-0.03, 0.03);
				const double T0 = std::min(0.9, (Lip + Rh.Range(0.6, 0.85) * Layer) / High), T1 = (Lip - Rh.Range(0.02, 0.06)) / High;
				const double Tm = 0.5 * (T0 + T1);
				const V3 Out{ std::cos(A), std::sin(A), 0 };
				const V3 Across{ -Out.Y, Out.X, 0 };
				const double L = Rh.Range(0.88, 1.06);
				std::vector<V3> Spine = { Out * (Radius(T0, A, true) + 0.01) + V3{ 0, 0, T0 * High - 0.04 },
										  Out * (Radius(Tm, A, true) + 0.018) + V3{ 0, 0, Tm * High - 0.04 },
										  Out * (Hive(T1) * 1.07 + 0.012) + V3{ 0, 0, T1 * High - 0.04 } };
				Lock(Spine, Rh.Range(0.014, 0.024), Across, HayPaint(T0, A, L), HayPaint(T1, A, 0.86 * L));
			}
			// Its topknot: a few twists of hay standing up from the crown and flopping over, ragged against the sky.
			for (int K = 0; K < 6; ++K)
			{
				const double A = Rh.Range(0.0, 2.0 * Pi), Long = Rh.Range(0.12, 0.22);
				const V3 Way{ std::cos(A), std::sin(A), 0 };
				const V3 Foot = Way * Rh.Range(0.0, 0.08) + V3{ 0, 0, High - 0.06 };
				const V3 Mid = Foot + Way * (0.3 * Long) + V3{ 0, 0, 0.45 * Long };
				const V3 Tip = Foot + Way * (0.9 * Long) + V3{ 0, 0, 0.15 * Long };
				Strip(Bm.Near, { Foot, Mid, Tip }, { 0.02, 0.016, 0.004 }, { -Way.Y, Way.X, 0 }, HayPaint(0.98, A, 0.96), HayPaint(0.98, A + 1.0, 0.9));
			}
			for (int K = 0; K < 12; ++K)
			{
				// a wisp lying loose round its foot
				const double A = Rh.Range(0.0, 2.0 * Pi), Turn = A + Rh.Range(-0.9, 0.9);
				const V3 Foot{ std::cos(A) * Rh.Range(0.8, 0.95), std::sin(A) * Rh.Range(0.8, 0.95), 0.015 };
				const V3 Way{ std::cos(Turn), std::sin(Turn), 0 };
				const double Long = Rh.Range(0.15, 0.24);
				Strip(Bm.Near, { Foot, Foot + Way * (0.5 * Long) + V3{ 0, 0, 0.012 }, Foot + Way * Long }, { 0.014, 0.012, 0.004 }, { -Way.Y, Way.X, 0 }, Rgb(132, 116, 58),
					  Rgb(150, 130, 66));
			}
			Bm.FarFrom = 150.0f;
		}
		// A piece of a cow path (MakeCowPaths lays them end to end along it, half a metre apart): bare earth trodden hard,
		// a little dried mud in it, 1.1 m long and half a metre wide, its edge ragged and gone to trodden grass. Domed a
		// couple of centimetres over the ground at its middle and its edge sunk under it, so where two pieces overlap the
		// one nearer its middle shows (no flicker between them), and the ground itself makes the path's edge.
		{
			Mesh& M = At(Base::Trodden).Near;
			Builder Bd(M);
			Rng Rt(S + 62);
			constexpr int Round = 14;
			const V3 Up{ 0, 0, 1 };
			const uint32_t Mid = Bd.Vert({ 0, 0, 0.026 }, Up, Rgb(110, 102, 80));
			uint32_t Inner[Round], Outer[Round];
			for (int K = 0; K < Round; ++K)
			{
				const double A = 2.0 * Pi * K / Round;
				const double Rag = 1.0 + 0.16 * Noise(std::cos(A) * 2.0 + 3.0, std::sin(A) * 2.0 + 3.0, S + 63);
				const double CX = std::cos(A) * 0.55, CY = std::sin(A) * 0.26;
				const double Mud = Rt.Next();
				// (dry July earth, grey-brown, not red; a little dried mud; its edge trodden grass)
				const Col Bare = Mud < 0.3 ? Rgb(98, 92, 74) : Varied(Rgb(114, 106, 84), Rt.Range(0.94, 1.06), 0.0);
				Inner[K] = Bd.Vert({ CX * 0.6 * Rag, CY * 0.55 * Rag, 0.02 }, Up, Bare);
				Outer[K] = Bd.Vert({ CX * Rag, CY * Rag, -0.012 }, Up, Mixed(Rgb(94, 106, 62), Rgb(112, 106, 82), Rt.Range(0.0, 0.35)));
			}
			for (int K = 0; K < Round; ++K)
			{
				const int N = (K + 1) % Round;
				Bd.Tri(Mid, Inner[K], Inner[N]);
				Bd.Tri(Inner[K], Outer[K], Outer[N]);
				Bd.Tri(Inner[K], Outer[N], Inner[N]);
			}
			At(Base::Trodden).FarFrom = 60.0f;
		}
		// (the steppe's burnt cottage, its look of release 2 unless the old is asked: LandScorch)
		if (Fd.Steppe.bOn && !Fd.bOldBurntGround) LandScorch::Look(Fd.Steppe.Kit, At(Base::Chimney));
		// A region's kit (each file's text in Fd.Kit, by the name its region gives the slot; a plain stand-in for a model
		// not given) and its own clutter.
		if (Rg)
		{
			for (int K = 0; K < KitSlots; ++K)
			{
				const char* Name = Rg->KitName(K);
				if (!Name) continue;
				const Base B = static_cast<Base>(static_cast<int>(Base::Kit0) + K);
				BaseMesh& M = At(B);
				const auto Found = Fd.Kit.find(Name);
				std::string Error;
				if (Found == Fd.Kit.end() || !LandSteppe::ReadKit(Found->second, 150.0, M, Error))
				{
					M.Near = Mesh();
					M.Mid = Mesh();
					M.Far = Mesh();
					Rg->KitPlain(K, M);
				}
				M.Name = Name;
				Rg->KitLook(K, M);
				if (!Fd.bOldBurntGround && M.Name == "BurntChimney") LandScorch::Look(Fd.Kit, M); // (the east village's burnt cottage)
			}
			for (int K = 0; K < 6; ++K) Rg->ClutterMesh(K, At(static_cast<Base>(static_cast<int>(Base::Clut0) + K)));
			for (int K = static_cast<int>(Base::Tuft); K < static_cast<int>(Base::Clut0); ++K) Rg->OwnClutter(static_cast<Base>(K), At(static_cast<Base>(K)));
		}
		// The farmhouses, barns, byres, church and village houses built to the vehicles' detail (the trial "houses"): their three levels in the kit's slots, drawn as
		// buildings (near, the middle from 90 m, plain from 300 m).
		for (size_t K = 0; K < HouseModels.size(); ++K)
		{
			BaseMesh& M = At(static_cast<Base>(static_cast<int>(Base::Kit0) + HouseSlot0 + static_cast<int>(K)));
			LandHouse::Levels L;
			// (an east house built under the snow lying, its roof's blanket its own; the bocage's left to the snow's tint)
			LandHouse::Spec Hs = HouseModels[K];
			if (Hs.Kind >= 7 && SnowLay) Hs.Snow = SnowIs;
			LandHouse::Build(Hs, L);
			M.Near = std::move(L.Near);
			M.Mid = std::move(L.Mid);
			M.Far = std::move(L.Far);
			M.MidFrom = 90.0f;
			M.FarFrom = 300.0f;
			M.Class = BaseMesh::Draw::House;
			static const char* const Hurts[4] = { "Whole", "RoofHole", "Gable", "Shell" };
			static const char* const Kinds[11] = { "Farmhouse", "Barn", "Byre", "Church", "Church", "Church", "House", "Izba", "Khata", "Burnt", "Desert" };
			const int Kd = std::clamp(HouseModels[K].Kind, 0, 10);
			const char* const Roofs = Kd == 10 ? (HouseModels[K].Roof == 1 ? "Mud" : HouseModels[K].Roof == 2 ? "Two" : "Lime") : Kd == 9 ? (HouseModels[K].Roof == 1 ? "Clay" : "Logs") : Kd >= 7 ? (HouseModels[K].Roof == 1 ? "Thatch" : "Plank") : (HouseModels[K].Roof == 1 ? "Tile" : "Slate");
			M.Name = std::string(Kinds[Kd]) + Roofs + Hurts[std::clamp(HouseModels[K].Hurt, 0, 3)] + std::to_string(K);
		}
		// Snow over it all: the broadleaf trees bare, snow on what faces up (a kit's middle level, which the snow does not
		// reach, dropped: its near level is drawn to its far one).
		if (SnowLay)
			for (size_t K = 0; K < Bases.size(); ++K)
			{
				// (a house built to the vehicles' detail, the trial "houses", keeps its middle level, the snow laid on it
				// as on its near one: its near drawn out to 300 m would be some 25 thousand triangles a house)
				const int Slot = static_cast<int>(K) - static_cast<int>(Base::Kit0) - HouseSlot0;
				const bool bBuilt = Slot >= 0 && Slot < static_cast<int>(HouseModels.size());
				// (a burnt-out house's snow is its own: laid as it is built, scoured off its ashes in patches)
				if (bBuilt && HouseModels[static_cast<size_t>(Slot)].Kind == 9) continue;
				Mesh Kept = bBuilt ? std::move(Bases[K].Mid) : Mesh();
				Bases[K].Mid = Mesh();
				SnowLay->Base(static_cast<Base>(K), Bases[K]);
				if (bBuilt)
				{
					BaseMesh Lay;
					Lay.Name = Bases[K].Name;
					Lay.Near = std::move(Kept);
					SnowLay->Base(static_cast<Base>(K), Lay);
					Bases[K].Mid = std::move(Lay.Near);
				}
			}
		// (no snow, but the winter's months: the broadleaf trees and the hedges bare all the same)
		else if (LandScape::LeaflessOf(Fd, LookIs))
		{
			const std::unique_ptr<LandScape::Snow> Bare = LandScape::MakeSnowCover(0.0, LookIs, Seed ^ 0x5D0Bull);
			for (size_t K = 0; K < Bases.size(); ++K) Bare->Leafless(static_cast<Base>(K), Bases[K]);
		}
		for (BaseMesh& B : Bases) Measure(B);
		// (a burnt-out house in sight as the kit's thing it stands for was: the same shape, its stove and chimney, not its
		// whole footprint; the size it was planted at kept)
		for (size_t K = 0; K < HouseFrom.size() && K < HouseModels.size(); ++K)
		{
			if (HouseFrom[K] < 0) continue;
			const BaseMesh& Was = Bases[static_cast<size_t>(Base::Kit0) + static_cast<size_t>(HouseFrom[K])];
			BaseMesh& M = Bases[static_cast<size_t>(Base::Kit0) + static_cast<size_t>(HouseSlot0) + K];
			if (!Was.bSight || M.Height <= 0.0f || M.Radius <= 0.0f) continue;
			M.bSight = true;
			M.SightMid = static_cast<float>(Was.SightMid * Was.Height * HouseFromScaleZ[K] / M.Height);
			M.SightHalf = static_cast<float>(Was.SightHalf * Was.Height * HouseFromScaleZ[K] / M.Height);
			M.SightAcross = static_cast<float>(Was.SightAcross * Was.Radius * HouseFromScale[K] / M.Radius);
			M.SightTrunk = static_cast<float>(Was.SightTrunk * HouseFromScale[K]);
		}
		// A region's kit slots' classes for the game's far draw, where its KitLook set none: a tree as it marked it, a
		// slot it laid as a house a house, else a tall thing big and a low one a shrub (the frame budget's classes).
		if (Rg)
			for (int K = 0; K < KitSlots; ++K)
			{
				const Base B = static_cast<Base>(static_cast<int>(Base::Kit0) + K);
				BaseMesh& M = At(B);
				if (M.Near.Empty() || M.Class != BaseMesh::Draw::Thing) continue;
				const bool bHouse = std::any_of(Houses.begin(), Houses.end(), [B](const House& H) { return H.bKit && H.KitB == B; });
				M.Class = M.bTree ? BaseMesh::Draw::Tree : bHouse ? BaseMesh::Draw::House : M.Height >= 3.0f ? BaseMesh::Draw::Big : BaseMesh::Draw::Shrub;
			}
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
		if (Fd.Steppe.bOn) Sp = std::make_unique<LandSteppe::Plan>(Fd.Steppe);
		// The look (LandScape): the bocage and the steppe are LandField's own; any other ground is a region's, where its
		// file is built (until then, the bocage); and the snow lying on any of them.
		LookIs = Sp ? LandScape::Look::Steppe : LandScape::Pick(Fd);
		if (!Sp) Rg = LandScape::Make(LookIs, Fd, Seed ^ 0x5CA9E5ull);
		if (!Sp && !Rg) LookIs = LandScape::Look::Bocage;
		SnowIs = LandScape::SnowOf(Fd, LookIs);
		SnowLay = LandScape::MakeSnow(SnowIs, LookIs, Seed ^ 0x5D0Bull, Fd);
		if (!SnowLay) SnowIs = 0.0;
		ReadCells();
		MakeLanes();
		MakeRaster();
		MakeWays();
		MakePlots();
		MakeSites();
		FileHouses();
		MakeParcels();
		MakeDistances();
		MakeHedges();
		MakeCowPaths();
		MakePlants();
		// (rain: the hay in its cocks and stacks soaked darker, Soaked; the corn and the stubble as they are laid, Clutter)
		if (RainWet > 0.0)
			for (const Base B : { Base::Haycock, Base::Haystack })
				for (Placed& Pl : Inst[static_cast<size_t>(B)]) Pl.Tint = Soaked(Pl.Tint);
		FileHouses();
		const auto T1 = Clock::now();
		MakeBases();
		SeatKitHouses();
		TakeUpTallLowKit();
		MakeFeet();
		MakeTiles();
		MakeRing();
		// (snow on the roofs and the walls' tops)
		if (SnowLay)
			for (Tile& T : TileList) SnowLay->Built(T.Built);
		const auto T2 = Clock::now();
		Count.CoreSeconds = std::chrono::duration<double>(T1 - T0).count();
		Count.MeshSeconds = std::chrono::duration<double>(T2 - T1).count();
	}

	Ground::Ground(const Field& F) : In(std::make_unique<Impl>(F))
	{
		// (each burnt cottage's own burnt ground, in the near tiles, unless the old look is asked: LandScorch)
		if (!F.bOldBurntGround) LandScorch::Into(In->TileList, LandScorch::Lay(*this, F));
	}
	Ground::~Ground() = default;

	double Ground::HeightAt(double X, double Y, Drawn* OutWhat) const
	{
		Drawn What = Drawn::Pasture;
		const double Z = In->Top(X, Y, &What);
		if (OutWhat)
		{
			*OutWhat = What;
		}
		return Z;
	}

	void Ground::DrawnUp(double X, double Y, double& OutX, double& OutY, double& OutZ) const
	{
		// As GridMesh lays a near tile: its square's corners lit by the slope across their neighbours, the square cut
		// along its flatter diagonal, the normal mixed across the triangle the place is in.
		const Impl& G = *In;
		const int TI = static_cast<int>(std::floor((X + Band) / TileSize)), TJ = static_cast<int>(std::floor((Y + Band) / TileSize));
		const double X0 = -Band + TI * TileSize, Y0 = -Band + TJ * TileSize;
		const bool bInner = X0 >= 0.0 && Y0 >= 0.0 && X0 + TileSize <= G.FW + 1e-6 && Y0 + TileSize <= G.FH + 1e-6;
		const int Q = bInner ? NearQuads : BandQuads;
		const double Step = TileSize / Q;
		const int I = std::clamp(static_cast<int>(std::floor((X - X0) / Step)), 0, Q - 1), J = std::clamp(static_cast<int>(std::floor((Y - Y0) / Step)), 0, Q - 1);
		double Z[4][4]; // (corners I-1 to I+2, J-1 to J+2; the four outer corners never asked)
		for (int B = 0; B < 4; ++B)
			for (int A = 0; A < 4; ++A)
				Z[B][A] = ((A == 0 || A == 3) && (B == 0 || B == 3)) ? 0.0 : G.NearZ(X0 + (I - 1 + A) * Step, Y0 + (J - 1 + B) * Step);
		auto Corner = [&Z, Step](int A, int B) { return Unit3({ -(Z[B][A + 1] - Z[B][A - 1]) / (2.0 * Step), -(Z[B + 1][A] - Z[B - 1][A]) / (2.0 * Step), 1.0 }); };
		const V3 Na = Corner(1, 1), Nb = Corner(2, 1), Nc = Corner(2, 2), Nd = Corner(1, 2);
		const double U = std::clamp((X - X0) / Step - I, 0.0, 1.0), V = std::clamp((Y - Y0) / Step - J, 0.0, 1.0);
		V3 N;
		if (std::fabs(Z[1][1] - Z[2][2]) <= std::fabs(Z[1][2] - Z[2][1]))
		{
			// (A, B, C and A, C, D: cut from the square's first corner to its far one)
			N = U >= V ? Na * (1.0 - U) + Nb * (U - V) + Nc * V : Na * (1.0 - V) + Nc * U + Nd * (V - U);
		}
		else
		{
			// (A, B, D and B, C, D)
			N = U + V <= 1.0 ? Na * (1.0 - U - V) + Nb * U + Nd * V : Nb * (1.0 - V) + Nc * (U + V - 1.0) + Nd * (1.0 - U);
		}
		N = Unit3(N);
		OutX = N.X;
		OutY = N.Y;
		OutZ = N.Z;
	}

	double Ground::DrawnZ(double X, double Y) const
	{
		// As DrawnUp: the near tile's square the place is in, cut along its flatter diagonal, the height across the
		// triangle from its three corners.
		const Impl& G = *In;
		const int TI = static_cast<int>(std::floor((X + Band) / TileSize)), TJ = static_cast<int>(std::floor((Y + Band) / TileSize));
		const double X0 = -Band + TI * TileSize, Y0 = -Band + TJ * TileSize;
		const bool bInner = X0 >= 0.0 && Y0 >= 0.0 && X0 + TileSize <= G.FW + 1e-6 && Y0 + TileSize <= G.FH + 1e-6;
		const int Q = bInner ? NearQuads : BandQuads;
		const double Step = TileSize / Q;
		const int I = std::clamp(static_cast<int>(std::floor((X - X0) / Step)), 0, Q - 1), J = std::clamp(static_cast<int>(std::floor((Y - Y0) / Step)), 0, Q - 1);
		const double Ax = X0 + I * Step, Ay = Y0 + J * Step;
		const double Za = G.NearZ(Ax, Ay), Zb = G.NearZ(Ax + Step, Ay), Zc = G.NearZ(Ax + Step, Ay + Step), Zd = G.NearZ(Ax, Ay + Step);
		const double U = std::clamp((X - X0) / Step - I, 0.0, 1.0), V = std::clamp((Y - Y0) / Step - J, 0.0, 1.0);
		if (std::fabs(Za - Zc) <= std::fabs(Zb - Zd))
		{
			return U >= V ? Za * (1.0 - U) + Zb * (U - V) + Zc * V : Za * (1.0 - V) + Zc * U + Zd * (V - U);
		}
		return U + V <= 1.0 ? Za * (1.0 - U - V) + Zb * U + Zd * V : Zb * (1.0 - V) + Zc * (U + V - 1.0) + Zd * (1.0 - U);
	}

	bool Ground::MayBeBuilt(double X, double Y) const
	{
		// (Top says Floor only inside a house's footprint pixels, and Wall only on a wall's, both in the raster)
		const Impl& G = *In;
		return G.InRaster(X, Y) && (G.OccR[G.Pix(X, Y)] & (OcHouse | OcWall)) != 0;
	}

	/** The ground's own colour at a point, as its vertices are painted (a shell hole's edge takes it). */
	Rgb8 Ground::ColourAt(double X, double Y) const
	{
		return Byte(In->GroundColour(X, Y));
	}

	bool Ground::CornAt(double X, double Y) const
	{
		const Impl& G = *In;
		if (!G.InRaster(X, Y)) return false;
		const size_t P = G.Pix(X, Y);
		if (G.Rgn[P] != RgField) return false;
		Drawn What = Drawn::Beyond;
		G.Top(X, Y, &What);
		const int Crop = G.Parcels[static_cast<size_t>(G.Pid[P])].Crop;
		if (Crop >= CpRegion) return What == Drawn::Crop && G.Rg && G.Rg->IsCorn(Crop - CpRegion);
		return What == Drawn::Crop && IsCorn(Crop);
	}

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
		const int Plain = G.PlainFx;
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
		default: Fx = CellFx == 1 || CellFx == 7 ? CellFx : Plain; break; // (a town's cell or a rock's its own; else the plain's)
		}
		S.Fx = FxWords[Fx];
		return S;
	}

	const std::vector<Tile>& Ground::Tiles() const { return In->TileList; }
	const std::vector<Tile>& Ground::Ring() const { return In->RingList; }
	const BaseMesh& Ground::BaseOf(Base B) const { return In->Bases[static_cast<size_t>(B)]; }
	int Ground::DrawStep() const { return In->Rg ? std::clamp(In->Rg->DrawStep(), 0, 4) : -1; }
	const std::vector<Placed>& Ground::Instances(Base B) const { return In->Inst[static_cast<size_t>(B)]; }
	const std::vector<Footing>& Ground::Footings() const { return In->Footings; }
	const std::vector<HedgeLine>& Ground::Hedges() const { return In->HedgeOut; }
	const std::vector<HedgeOpening>& Ground::Openings() const { return In->OpeningOut; }
	const std::vector<HedgeCrossing>& Ground::Crossings() const { return In->CrossOut; }
	double Ground::BankRise(double X, double Y) const { return In->BankAt(X, Y); }

	bool Ground::NearestBank(double X, double Y, double Reach, BankPlace& Out) const
	{
		const Impl& G = *In;
		if (G.BGrid.Cells.empty() || !(X == X) || !(Y == Y)) return false;
		const V2 P{ X, Y };
		const int I0 = std::max(0, static_cast<int>(std::floor((X - Reach - G.BGrid.X0) / G.BGrid.Size)));
		const int I1 = std::min(G.BGrid.NX - 1, static_cast<int>(std::floor((X + Reach - G.BGrid.X0) / G.BGrid.Size)));
		const int J0 = std::max(0, static_cast<int>(std::floor((Y - Reach - G.BGrid.Y0) / G.BGrid.Size)));
		const int J1 = std::min(G.BGrid.NY - 1, static_cast<int>(std::floor((Y + Reach - G.BGrid.Y0) / G.BGrid.Size)));
		double Best = Reach;
		bool bFound = false;
		for (int J = J0; J <= J1; ++J)
			for (int I = I0; I <= I1; ++I)
				for (const uint32_t Id : G.BGrid.Cells[static_cast<size_t>(J) * G.BGrid.NX + I])
				{
					const BankSeg& S = G.BSegs[Id];
					const Run& R = G.Runs[S.Run];
					double T;
					const double D = SegDist(P, R.P[S.K], R.P[S.K + 1], T);
					if (D >= Best) continue;
					Best = D;
					bFound = true;
					const V2 On = Lerp2(R.P[S.K], R.P[S.K + 1], T), Head = Unit2(R.P[S.K + 1] - R.P[S.K]);
					Out.X = On.X;
					Out.Y = On.Y;
					Out.Yaw = std::atan2(Head.Y, Head.X);
					Out.Height = Lerp(R.H[S.K], R.H[S.K + 1], T);
					Out.Foot = BankEdge * 0.5 * Lerp(R.W[S.K], R.W[S.K + 1], T);
					Out.Kind = R.Kind;
				}
		return bFound;
	}
	const std::vector<std::vector<std::pair<double, double>>>& Ground::Lanes() const { return In->LaneOut; }
	const Tally& Ground::Laid() const { return In->Count; }
	const char* Ground::LookWord() const { return LandScape::NameOf(In->LookIs); }
	double Ground::SnowLying() const { return In->SnowIs; }
	const char* Ground::EarthWord() const
	{
		if (In->SnowIs >= 0.5) return "snow";
		return In->Rg ? In->Rg->Earth() : "";
	}

	std::string Ground::CropOfParcel(int Parcel) const
	{
		const Impl& G = *In;
		if (Parcel < 0 || static_cast<size_t>(Parcel) >= G.Parcels.size()) return "";
		if (G.Parcels[static_cast<size_t>(Parcel)].Crop >= CpRegion) return G.Rg ? G.Rg->CropWord(G.Parcels[static_cast<size_t>(Parcel)].Crop - CpRegion) : "";
		switch (G.Parcels[static_cast<size_t>(Parcel)].Crop)
		{
		case CpGrazed: return "grazed";
		case CpLush: return "lush";
		case CpHayCut: return "hay cut";
		case CpMeadow: return "meadow";
		case CpWheat: return "wheat";
		case CpBarley: return "barley";
		case CpOats: return "oats";
		case CpGreenWheat: return "green wheat";
		case CpStubble: return "stubble";
		case CpRoots: return "roots";
		case CpRye: return "rye";
		case CpSteppe: return "steppe";
		case CpFallow: return "fallow";
		default: return "";
		}
	}

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
		Growth Grow(Drawn What, int Crop, double Edge, double LaneEdge, uint8_t Occ, double Pick, bool bSteppe)
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
				else if (Crop == CpStubble) G = Of(1.0, { { 0.84, Base::Stubble }, { 0.11, Base::Tuft }, { 0.03, Base::Poppies }, { 0.02, Base::Flowers } });
				else G = Of(1.6, { { 1.0, Base::Tuft } });
				break;
			case Drawn::Pasture:
				// (the steppe's: its pasture tussocky, feather grass in it; its fallow weedy; the strip between fields tall)
				if (bSteppe && Edge < 3.0) G = Of(1.4, { { 0.5, Base::TallGrass }, { 0.3, Base::Tuft }, { 0.2, Base::Flowers } });
				else if (Crop == CpSteppe) G = Of(1.3, { { 0.45, Base::Tuft }, { 0.42, Base::TallGrass }, { 0.13, Base::Flowers } });
				else if (Crop == CpFallow) G = Of(0.9, { { 0.5, Base::Tuft }, { 0.3, Base::TallGrass }, { 0.2, Base::Flowers } });
				else if (Edge < 3.0) G = Of(1.2, { { 0.35, Base::TallGrass }, { 0.15, Base::Fern }, { 0.2, Base::Flowers }, { 0.3, Base::Tuft } });
				// (a dock or a plantain here and there in the open grass, where the beasts leave them: the art director's cut 5
				// note. Each takes its share from the tufts', next to them, so nothing else in the grass moves)
				else if (Crop == CpLush) G = Of(1.45, { { 0.35, Base::Sward }, { 0.37, Base::Tuft }, { 0.03, Base::Rosette }, { 0.18, Base::TallGrass }, { 0.07, Base::Flowers } });
				else if (Crop == CpHayCut) G = Of(1.2, { { 0.55, Base::Sward }, { 0.38, Base::Tuft }, { 0.02, Base::Rosette }, { 0.05, Base::Flowers } });
				else if (Crop == CpMeadow) G = Of(1.45, { { 0.2, Base::Sward }, { 0.15, Base::Tuft }, { 0.45, Base::TallGrass }, { 0.12, Base::Flowers }, { 0.08, Base::Poppies } });
				else G = Of(1.45, { { 0.6, Base::Sward }, { 0.29, Base::Tuft }, { 0.03, Base::Rosette }, { 0.05, Base::Flowers }, { 0.03, Base::TallGrass } });
				break;
			case Drawn::Rough: G = Of(1.2, { { 0.5, Base::TallGrass }, { 0.35, Base::Tuft }, { 0.15, Base::Fern } }); break;
			case Drawn::Works: G = Of(0.3, { { 1.0, Base::Tuft } }); break; // (its few stones: the clumped pass below)
			case Drawn::Village:
			case Drawn::Yard:
				G = (Occ & OcNear) || What == Drawn::Yard ? Of(0.25, { { 0.8, Base::Tuft }, { 0.2, Base::Stone } })
														   : Of(1.2, { { 0.4, Base::Sward }, { 0.36, Base::Tuft }, { 0.18, Base::Flowers }, { 0.06, Base::TallGrass } });
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
			case Drawn::Beyond: G = Of(1.2, { { 0.45, Base::Sward }, { 0.45, Base::Tuft }, { 0.06, Base::TallGrass }, { 0.04, Base::Flowers } }); break;
			default: break; // a floor or water: nothing
			}
			// (no bracken on the steppe: dry grass where the bocage has fern; and its pasture's tufts as they were)
			if (bSteppe && G.Kind == Base::Fern) G.Kind = Base::TallGrass;
			if (bSteppe && G.Kind == Base::Rosette) G.Kind = Base::Tuft;
			// Trodden: gateways, and where the recording's men and vehicles went.
			if (Occ & OcVehicle) G.Density *= 0.35;
			else if (Occ & OcGate) G.Density *= 0.3;
			else if (Occ & OcMan) G.Density *= 0.75;
			return G;
		}
	}

	void Ground::AddBurn(double X, double Y, double Core, double Outer) { In->AddBurn(X, Y, Core, Outer); }

	void Ground::Clutter(double X0, double Y0, double X1, double Y1, std::vector<Placed>& Out) const
	{
		const Impl& G = *In;
		const size_t Before = Out.size();
		constexpr double Square = ClutterSquare;
		constexpr int Tries = 128; // two things a square metre at most
		const int I0 = static_cast<int>(std::floor(X0 / Square)), I1 = static_cast<int>(std::floor(X1 / Square));
		const int J0 = static_cast<int>(std::floor(Y0 / Square)), J1 = static_cast<int>(std::floor(Y1 / Square));
		for (int J = J0; J <= J1; ++J)
			for (int I = I0; I <= I1; ++I)
			{
				const uint64_t Key = Hash(G.Seed ^ 0xC107E5ull, static_cast<uint64_t>(static_cast<int64_t>(I)), static_cast<uint64_t>(static_cast<int64_t>(J)));
				// (on trial, "feet": the bushes' bodies, the trunks and the stacks that reach into this square)
				const Impl::GrassFoot* Foot0 = nullptr;
				const Impl::GrassFoot* Foot1 = nullptr;
				if (!G.FootSquares.empty())
				{
					const auto Stood = G.FootSquares.find(SquareKeyIJ(I, J));
					if (Stood != G.FootSquares.end())
					{
						Foot0 = G.Feet.data() + Stood->second.first;
						Foot1 = G.Feet.data() + Stood->second.second;
					}
				}
				// (burnt ground: the rounds that reach into this square)
				const std::vector<Impl::Burn>* Burnt = nullptr;
				if (!G.BurnSquares.empty())
				{
					const auto Charred = G.BurnSquares.find(SquareKeyIJ(I, J));
					if (Charred != G.BurnSquares.end()) Burnt = &Charred->second;
				}
				for (int K = 0; K < Tries; ++K)
				{
					// Everything about a thing comes from its square and its number, so any box that holds it lays it the same.
					const uint64_t H = Hash(Key, static_cast<uint64_t>(K));
					const double X = (I + Unit(H)) * Square, Y = (J + Unit(Mix(H))) * Square;
					if (X < X0 || X >= X1 || Y < Y0 || Y >= Y1) continue;
					const double Keep = Unit(Hash(H, 1)), Pick = Unit(Hash(H, 2));
					int Crop = CpOther;
					double Edge = 99.0, LaneEdge = 99.0, RowAngle = 0.0;
					uint8_t Occ = 0;
					if (G.InRaster(X, Y))
					{
						const size_t P = G.Pix(X, Y);
						const ParcelInfo& Q = G.Parcels[static_cast<size_t>(G.Pid[P])];
						Crop = Q.Crop;
						RowAngle = Q.Angle;
						Edge = G.DistE[P];
						LaneEdge = G.LaneD[P];
						Occ = G.OccR[P];
						if (Occ & OcHouse) continue;
						// (more than any growth here could want; a pasture's clumps want more)
						if (Keep >= (G.Rg || IsCorn(Crop) ? 1.0 : Crop <= CpMeadow ? 0.95 : 0.75)) continue;
					}
					Drawn What = Drawn::Beyond;
					const double Z = G.Top(X, Y, &What);
					if (G.Sp && G.Sp->HasRail() && G.Sp->RailDistance(X, Y) < 3.0) continue; // (nothing grows on the railway's ballast)
					// The bocage's grass in clumps, not spread even (the review's item 3): in some patches tussocks and
					// flowers together, between them the short sward. On
					// a cow path, the ground trodden bare (its own pieces of earth laid below), its edge thinned.
					// (a region's ground: its own growth, read from the place as it sees it)
					LandScape::Spot Seen;
					if (G.Rg || G.SnowLay)
					{
						Seen = G.SpotAt(X, Y, false);
						Seen.Z = Z;
						Seen.Drawn = static_cast<int>(What);
					}
					Growth Gr;
					if (G.Rg)
					{
						const LandScape::Grown Own = G.Rg->Grow(Seen, Pick);
						Gr.Kind = Own.Kind;
						Gr.Density = Own.Density;
						if (Occ & OcVehicle) Gr.Density *= 0.35;
						else if (Occ & OcMan) Gr.Density *= 0.75;
					}
					else Gr = Grow(What, Crop, Edge, LaneEdge, Occ, Pick, G.Sp != nullptr);
					if (Gr.Kind == Base::Count) continue;
					// (past what the thickest clump could keep, 1.85 times its growth: let go before the clump's noise and
					// the cow paths are asked. The pre-check, 22 November: the clumps had made the bocage's clutter half as
					// dear again)
					if (Keep >= Gr.Density * 1.85 / 2.0) continue;
					double InClump = -1.0, Thick = 1.0;
					if (What == Drawn::Pasture && !G.Sp && !G.Rg)
					{
						// (the noise's squares turned off the field's axes, each its own way, and pushed about a little: its
						// lattice laid the clumps out on a 4.5 m grid, read as rows of tufts from a low lens; the art director's
						// cut 4, shot 08)
						const double Wx = 1.3 * Noise(X / 7.3 + 3.1, Y / 7.3 - 1.7, G.Seed ^ 0x7AF9), Wy = 1.3 * Noise(X / 7.3 - 5.3, Y / 7.3 + 2.9, G.Seed ^ 0x7AFA);
						const double Ax = 0.809 * X - 0.588 * Y + Wx, Ay = 0.588 * X + 0.809 * Y + Wy;   // (36 degrees)
						const double Bx = 0.276 * X + 0.961 * Y, By = -0.961 * X + 0.276 * Y;             // (74 degrees the other way)
						const double Clump = Noise(Ax / 4.5, Ay / 4.5, G.Seed ^ 0x7AF7) + 0.4 * Noise(Bx / 1.6, By / 1.6, G.Seed ^ 0x7AF8);
						InClump = Smooth(-0.3, 0.3, Clump);
						const double Path = G.CowPath(X, Y);
						if (Path < 0.0 && Keep > 0.05) continue;
						if (Path < 0.12) Thick = 0.5;
					}
					// (the short sward between the clumps; the tussocks and the flowers in them. The tall grass stays as it
					// was, spread: the camera reads it as standing corn for its low shots)
					if (InClump >= 0.0 && Gr.Kind != Base::TallGrass) Thick *= Gr.Kind == Base::Sward ? 1.2 - 0.45 * InClump : 0.15 + 1.7 * InClump;
					if (Keep >= Gr.Density * Thick / 2.0) continue;
					// The bracken keeps to the hedge banks (the art director's cut 5 note): in the open grass or a field's
					// rough ground more than 5 m from a bank's middle (a rough corner, a gateway, an edge with no hedge), a
					// dock's low rosette in its place. (Asked last, of the few kept: the banks' grid is dearer than a draw.)
					if (!G.Rg && Gr.Kind == Base::Fern && (What == Drawn::Pasture || What == Drawn::Rough) && G.BankNear(X, Y, 5.0) > 5.0) Gr.Kind = Base::Rosette;
					Placed P;
					P.Kind = Gr.Kind;
					P.X = static_cast<float>(X);
					P.Y = static_cast<float>(Y);
					P.Z = static_cast<float>(Z);
					P.Yaw = static_cast<float>(2.0 * Pi * Unit(Hash(H, 3)));
					const double U4 = Unit(Hash(H, 4)), U5 = Unit(Hash(H, 5)), U6 = Unit(Hash(H, 6));
					double Scale = 0.8 + 0.4 * U4, ScaleZ = 0.85 + 0.3 * U5;
					double TR = 1.0, TG = 1.0, TB = 1.0;
					// (a region dresses its own things, or leaves a kind to the bocage's dressing below)
					P.Scale = 1.0f;
					P.ScaleZ = 1.0f;
					P.Tint = Tint8(1.0, 1.0, 1.0);
					const bool bDressed = G.Rg && G.Rg->Dress(P, Seen, U4, U5, U6, Unit(Hash(H, 7)));
					if (!bDressed) switch (Gr.Kind)
					{
					case Base::Wheat:
						// (wheat's ears 1.2 to 1.4 m up, barley 1.0 to 1.1, rye at Prokhorovka 1.5 to 1.8: a kneeling man
						// hidden, a standing man seen from his belt up)
						Scale = 0.9 + 0.3 * U4;
						ScaleZ = 1.12 + 0.14 * U5;
						if (Crop == CpBarley)
						{
							ScaleZ *= 0.8;
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
						else if (Crop == CpRye)
						{
							// (winter rye, ripe and chest high: greyer and paler than the wheat)
							ScaleZ *= 1.3;
							TR = 0.98;
							TG = 0.97;
							TB = 1.06;
						}
						// (the steppe's corn as it was: the corn's meshes are painted warmer for the bocage's grade, cut 5;
						// Prokhorovka's July morning has its own grade and its fields their own paint)
						if (G.Sp)
						{
							TR /= 1.12;
							TB /= 0.56;
						}
						break;
					case Base::Tuft:
					{
						// (most tufts small, a few big, each its own height for its spread and its own green or straw: the
						// same big tuft over and over read as rows of one model; the art director's cut 4, shot 08)
						Scale = 0.55 + 0.95 * std::pow(U4, 1.6);
						ScaleZ = Scale * (0.6 + 0.6 * U5);
						if (Crop == CpHayCut) ScaleZ *= 0.6;
						const double Dry = Unit(Hash(H, 7));
						TR = 0.92 + 0.2 * Dry;
						TG = 0.97 + 0.07 * Dry;
						TB = 0.88 + 0.18 * Dry;
						if (Crop == CpRoots && What == Drawn::Crop)
						{
							Scale *= 1.6;
							ScaleZ *= 0.8;
							TR = 0.72;
							TG = 0.86;
							TB = 0.74;
						}
						break;
					}
					case Base::TallGrass:
						Scale = 0.8 + 0.5 * U4;
						ScaleZ = 0.8 + 0.45 * U5;
						if (G.Sp && U6 < 0.45)
						{
							// (feather grass, its awns silvering in the wind; the rest of the steppe's grass gone buff)
							TR = 1.18;
							TG = 1.12;
							TB = 1.12;
						}
						else if (G.Sp)
						{
							TR = 1.1;
							TG = 1.0;
							TB = 0.86;
						}
						break;
					case Base::Stone:
						Scale = 0.6 + 1.2 * U4;
						ScaleZ = 0.7 + 0.6 * U5;
						break;
					case Base::Sward:
					{
						// the ground's own colour where it grows (built grey, 128 a tint is as built)
						const Col Here = G.GroundColour(X, Y);
						TR = Here.R / 118.0;
						TG = Here.G / 118.0;
						TB = Here.B / 118.0;
						Scale = 0.8 + 0.4 * U4;
						ScaleZ = Crop == CpHayCut || Crop == CpGrazed ? 0.7 + 0.4 * U5 : 0.9 + 0.5 * U5;
						break;
					}
					case Base::Rosette:
					{
						// (each its own size, a little warmer or cooler: a dock in the sun, one in a tussock's shade)
						Scale = 0.75 + 0.5 * U4;
						ScaleZ = 0.8 + 0.4 * U5;
						const double Warm = Unit(Hash(H, 7));
						TR = 0.94 + 0.12 * Warm;
						TB = 1.05 - 0.1 * Warm;
						break;
					}
					case Base::Stubble:
						// along the field's own drill rows (its ploughed lands' way), either way round
						P.Yaw = static_cast<float>(RowAngle + (U6 < 0.5 ? 0.0 : Pi) + 0.06 * (U4 - 0.5));
						Scale = 0.85 + 0.3 * U4;
						ScaleZ = 0.8 + 0.4 * U5;
						break;
					default: break;
					}
					if (!bDressed)
					{
						const double Shade = 0.95 + 0.1 * U6;
						P.Scale = static_cast<float>(Scale);
						P.ScaleZ = static_cast<float>(ScaleZ);
						P.Tint = Tint8(TR * Shade, TG * Shade, TB * Shade);
					}
					// (on trial, "feet": nothing grows inside a bush's body, a trunk or a stack that would stand more than a
					// hand's height over its foot, nor so near its edge that its blades would blow through the leaves; the
					// short sward, a stone, what lies flat and what stands lower down a bank's flank stay)
					if (Foot0)
					{
						const BaseMesh& Own = G.Bases[static_cast<size_t>(P.Kind)];
						const double Tall = Own.Height * P.ScaleZ, Wide = Own.Radius * P.Scale, Tip = Z + Tall - 0.3;
						const double Half = G.bFeetWide ? std::min(FootWide, Wide + BladeSweep * Tall) : std::min(FootEdge, 0.5 * Wide);
						bool bIn = false;
						for (const Impl::GrassFoot* F = Foot0; F != Foot1 && !bIn; ++F) bIn = Tip > F->Z && Sq(X - F->X) + Sq(Y - F->Y) < Sq(F->R + Half);
						if (bIn) continue;
					}
					// (on burnt ground: 3 things in 10 stay within a round's core, the same ones every time the square is laid,
					// each by its own number; more toward the round's outer edge, where all stay; the flowers go first; what
					// stays is dry, brought toward straw. Where rounds overlap the strongest counts. A stone stays a stone.)
					if (Burnt && P.Kind != Base::Stone)
					{
						double Char = 0.0;
						for (const Impl::Burn& Bn : *Burnt)
						{
							const double Off = std::hypot(X - Bn.X, Y - Bn.Y);
							Char = std::max(Char, Off <= Bn.Core ? 1.0 : Off >= Bn.Outer ? 0.0 : (Bn.Outer - Off) / (Bn.Outer - Bn.Core));
						}
						if (Char > 0.0)
						{
							const bool bFlower = P.Kind == Base::Flowers || P.Kind == Base::Poppies;
							if (Unit(Hash(H, 0xB027ull)) < Char * (bFlower ? 1.6 : 0.7)) continue;
							const Col Mean = G.BaseMean[static_cast<size_t>(P.Kind)];
							const double Dry = 0.85 * Char;
							P.Tint = Tint8(Lerp(P.Tint.R / 128.0, 150.0 / std::max(Mean.R, 40.0), Dry), Lerp(P.Tint.G / 128.0, 125.0 / std::max(Mean.G, 40.0), Dry),
								Lerp(P.Tint.B / 128.0, 80.0 / std::max(Mean.B, 40.0), Dry));
						}
					}
					// (under snow: buried, or its tips showing through, frosted)
					if (G.SnowLay && !G.SnowLay->Clutter(P, Seen)) continue;
					Out.push_back(P);
					// (the same corn stalk by stalk, on the same ground, for the lens close by)
					if (Gr.Kind == Base::Wheat)
					{
						P.Kind = Base::Stalks;
						Out.push_back(P);
					}
				}
				// The field's stones: none strewn over the grass, only a few grey ones half sunk, in clumps where the plough
				// and the beasts left them: along the banks' feet, in the gateways and at the lanes' sides; and on the works,
				// a scatter of spoil here and there where the digging threw it, the rest bare earth.
				const int StoneTries = G.Rg ? 0 : 6; // (a region lays its own stones with its growth)
				for (int K = 0; K < StoneTries; ++K)
				{
					const uint64_t H = Hash(Key, 0x5703ull + static_cast<uint64_t>(K));
					const double X = (I + Unit(H)) * Square, Y = (J + Unit(Mix(H))) * Square;
					if (X < X0 || X >= X1 || Y < Y0 || Y >= Y1 || !G.InRaster(X, Y)) continue;
					const size_t Px = G.Pix(X, Y);
					const uint8_t Occ = G.OccR[Px];
					if (Occ & (OcHouse | OcVehicle | OcMan | OcNear)) continue;
					Drawn What = Drawn::Beyond;
					const double Z = G.Top(X, Y, &What);
					const bool bGate = (Occ & OcGate) != 0;
					const bool bFieldFoot = (What == Drawn::Pasture || What == Drawn::Crop || What == Drawn::Rough) && G.DistE[Px] > 2.5f && G.DistE[Px] < 5.5f;
					const bool bLaneSide = What == Drawn::Lane && G.LaneD[Px] > -1.0f && G.LaneD[Px] < 0.3f;
					const bool bWorks = What == Drawn::Works;
					if (!bGate && !bFieldFoot && !bLaneSide && !bWorks) continue;
					if (G.Sp && !bWorks) continue; // (the black earth has no stones)
					// Clumps: a stretch of bank foot here and there, the rest bare (on the works fewer and sparser: some 0.007
					// a square metre over them all).
					const double Clump = Noise(X / 6.0, Y / 6.0, G.Seed ^ 0x5703ull);
					if (Clump < (bGate ? 0.0 : bWorks ? 0.42 : 0.32) || Unit(Hash(H, 1)) > (bGate ? 0.5 : bWorks ? 0.4 : 0.45)) continue;
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
					if (G.SnowLay)
					{
						LandScape::Spot Seen = G.SpotAt(X, Y, false);
						Seen.Z = Z;
						Seen.Drawn = static_cast<int>(What);
						if (!G.SnowLay->Clutter(P, Seen)) continue;
					}
					Out.push_back(P);
				}
				// The cow paths' trodden earth in this square.
				const auto Found = G.CowSquares.find(SquareKeyIJ(I, J));
				if (Found != G.CowSquares.end() && !G.SnowLay) // (no beasts out in the snow: their paths under it)
					for (uint32_t K = Found->second.first; K < Found->second.second; ++K)
					{
						const Placed& B = G.CowBits[K];
						if (B.X >= X0 && B.X < X1 && B.Y >= Y0 && B.Y < Y1) Out.push_back(B);
					}
			}
		// (rain: the corn standing and the stubble soaked darker, Soaked)
		if (G.RainWet > 0.0)
			for (size_t K = Before; K < Out.size(); ++K)
				if (Impl::Soaks(Out[K].Kind)) Out[K].Tint = G.Soaked(Out[K].Tint);
	}
}

namespace LandField
{
	Mesh CrownOf(const Mesh& From)
	{
		Mesh Out;
		const size_t N = From.Points();
		if (N == 0 || From.Positions.size() < N * 3) return Out;
		double High = 0.5;
		for (size_t K = 0; K < N; ++K) High = std::max(High, static_cast<double>(From.Positions[K * 3 + 2]));
		auto Leafy = [&From](size_t K) { return static_cast<int>(From.Colours[K].G) > static_cast<int>(From.Colours[K].R) + 6; };
		size_t LeafN = 0;
		for (size_t K = 0; K < N; ++K) LeafN += Leafy(K) ? 1 : 0;
		const bool bByColour = LeafN >= 8;
		auto InCrown = [&](size_t K) { return bByColour ? Leafy(K) : From.Positions[K * 3 + 2] > High / 3.0; };
		double Lo[3] = { 1e9, 1e9, 1e9 }, Hi[3] = { -1e9, -1e9, -1e9 };
		double Leaf[3] = { 0, 0, 0 }, Bark[3] = { 0, 0, 0 }, TrunkR = 0.0;
		size_t CrownN = 0, BarkN = 0;
		for (size_t K = 0; K < N; ++K)
		{
			const float* P = &From.Positions[K * 3];
			const Rgb8& C = From.Colours[K];
			if (InCrown(K))
			{
				for (int A = 0; A < 3; ++A)
				{
					Lo[A] = std::min(Lo[A], static_cast<double>(P[A]));
					Hi[A] = std::max(Hi[A], static_cast<double>(P[A]));
				}
				Leaf[0] += C.R;
				Leaf[1] += C.G;
				Leaf[2] += C.B;
				++CrownN;
			}
			else
			{
				Bark[0] += C.R;
				Bark[1] += C.G;
				Bark[2] += C.B;
				++BarkN;
				if (P[2] < High * 0.2) TrunkR = std::max(TrunkR, std::sqrt(static_cast<double>(P[0]) * P[0] + static_cast<double>(P[1]) * P[1]));
			}
		}
		if (CrownN == 0) return Out;
		for (int A = 0; A < 3; ++A)
		{
			Leaf[A] /= static_cast<double>(CrownN);
			Bark[A] = BarkN ? Bark[A] / static_cast<double>(BarkN) : (A == 0 ? 92.0 : A == 1 ? 80.0 : 66.0);
		}
		const double CX = 0.5 * (Lo[0] + Hi[0]), CY = 0.5 * (Lo[1] + Hi[1]), CZ = 0.5 * (Lo[2] + Hi[2]);
		// (the leaves' box holds their outermost tips: the ball a little inside it, as much leaf as the eye sees in it)
		const double RX = std::max(0.3, 0.42 * (Hi[0] - Lo[0])), RY = std::max(0.3, 0.42 * (Hi[1] - Lo[1])), RZ = std::max(0.3, 0.46 * (Hi[2] - Lo[2]));
		auto Byte = [](double V) { return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(V)), 0, 255)); };
		auto Put = [&Out, &Byte](double X, double Y, double Z, double NX, double NY, double NZ, const double* C, double Shade) {
			const double L = std::max(1e-9, std::sqrt(NX * NX + NY * NY + NZ * NZ));
			Out.Positions.insert(Out.Positions.end(), { static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z) });
			Out.Normals.insert(Out.Normals.end(), { static_cast<float>(NX / L), static_cast<float>(NY / L), static_cast<float>(NZ / L) });
			Out.Colours.push_back(Rgb8{ Byte(C[0] * Shade), Byte(C[1] * Shade), Byte(C[2] * Shade) });
			return static_cast<uint32_t>(Out.Colours.size() - 1);
		};
		// (each triangle's corners so that (B - A) x (C - A) points out, as the field's Builder makes them)
		auto Tri = [&Out](uint32_t A, uint32_t B, uint32_t C) { Out.Triangles.insert(Out.Triangles.end(), { A, B, C }); };
		auto Outward = [&Out](uint32_t A, uint32_t B, uint32_t C, double OX, double OY, double OZ) {
			const float* PA = &Out.Positions[A * 3];
			const float* PB = &Out.Positions[B * 3];
			const float* PC = &Out.Positions[C * 3];
			const double UX = PB[0] - PA[0], UY = PB[1] - PA[1], UZ = PB[2] - PA[2];
			const double VX = PC[0] - PA[0], VY = PC[1] - PA[1], VZ = PC[2] - PA[2];
			const double NX = UY * VZ - UZ * VY, NY = UZ * VX - UX * VZ, NZ = UX * VY - UY * VX;
			const double MX = (PA[0] + PB[0] + PC[0]) / 3.0 - OX, MY = (PA[1] + PB[1] + PC[1]) / 3.0 - OY, MZ = (PA[2] + PB[2] + PC[2]) / 3.0 - OZ;
			return NX * MX + NY * MY + NZ * MZ >= 0.0;
		};
		auto Face = [&](uint32_t A, uint32_t B, uint32_t C, double OX, double OY, double OZ) {
			if (Outward(A, B, C, OX, OY, OZ)) Tri(A, B, C);
			else Tri(A, C, B);
		};
		// The trunk: three sides from the ground into the crown's lower third.
		const double R = std::clamp(TrunkR * 0.8, 0.12, 0.6), Top = Lo[2] + RZ * 0.6;
		const double Pi2 = 6.283185307179586;
		uint32_t Foot[3], Head[3];
		for (int K = 0; K < 3; ++K)
		{
			const double A = Pi2 * K / 3.0, X = R * std::cos(A), Y = R * std::sin(A);
			Foot[K] = Put(X, Y, 0.0, X, Y, 0.0, Bark, 0.85);
			Head[K] = Put(X, Y, Top, X, Y, 0.0, Bark, 1.0);
		}
		for (int K = 0; K < 3; ++K)
		{
			const int J = (K + 1) % 3;
			Face(Foot[K], Foot[J], Head[J], 0.0, 0.0, 0.5 * Top);
			Face(Foot[K], Head[J], Head[K], 0.0, 0.0, 0.5 * Top);
		}
		// The crown: a ball of six sides and four bands, darker toward its underside.
		const int Sides = 6, Bands = 4;
		const uint32_t North = Put(CX, CY, CZ + RZ, 0.0, 0.0, 1.0, Leaf, 1.08);
		std::vector<uint32_t> Ring[Bands - 1];
		for (int B = 1; B < Bands; ++B)
		{
			const double Polar = 3.141592653589793 * B / Bands, SZ = std::cos(Polar), SR = std::sin(Polar);
			const double Shade = 0.72 + 0.36 * (0.5 + 0.5 * SZ);
			for (int K = 0; K < Sides; ++K)
			{
				const double A = Pi2 * (K + 0.5 * (B % 2)) / Sides, DX = SR * std::cos(A), DY = SR * std::sin(A);
				Ring[B - 1].push_back(Put(CX + RX * DX, CY + RY * DY, CZ + RZ * SZ, DX / RX, DY / RY, SZ / RZ, Leaf, Shade));
			}
		}
		const uint32_t South = Put(CX, CY, CZ - RZ, 0.0, 0.0, -1.0, Leaf, 0.66);
		for (int K = 0; K < Sides; ++K)
		{
			const int J = (K + 1) % Sides;
			Face(North, Ring[0][K], Ring[0][J], CX, CY, CZ);
			for (int B = 0; B + 1 < Bands - 1; ++B)
			{
				Face(Ring[B][K], Ring[B + 1][K], Ring[B + 1][J], CX, CY, CZ);
				Face(Ring[B][K], Ring[B + 1][J], Ring[B][J], CX, CY, CZ);
			}
			Face(South, Ring[Bands - 2][J], Ring[Bands - 2][K], CX, CY, CZ);
		}
		return Out;
	}
}
