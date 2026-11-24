#include "Battle/AirLand.h"

#include "Battle/AirFeed.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace AirLand
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Far = 1.0e18;

		// ---- Hashes and noise: the same numbers on every machine and thread, from the place's seed ----------------

		inline uint64_t Mix(uint64_t X)
		{
			X ^= X >> 30;
			X *= 0xbf58476d1ce4e5b9ull;
			X ^= X >> 27;
			X *= 0x94d049bb133111ebull;
			X ^= X >> 31;
			return X;
		}
		inline uint64_t Hash(int64_t A, int64_t B, uint64_t S)
		{
			return Mix(S ^ Mix(static_cast<uint64_t>(A) * 0x9E3779B97F4A7C15ull ^ Mix(static_cast<uint64_t>(B) + 0x632BE59BD9B4E019ull)));
		}
		inline uint64_t Hash3(int64_t A, int64_t B, int64_t C, uint64_t S) { return Hash(A, B, Mix(S + static_cast<uint64_t>(C) * 0xD6E8FEB86659FD93ull)); }
		/** 0 to 1 from a hash (its top 53 bits); Shift picks other bits of the same hash. */
		inline double Unit(uint64_t H, int Shift = 0) { return static_cast<double>(Mix(H + static_cast<uint64_t>(Shift) * 0x9E3779B97F4A7C15ull) >> 11) * (1.0 / 9007199254740992.0); }

		inline double Clamp01(double V) { return V < 0.0 ? 0.0 : (V > 1.0 ? 1.0 : V); }
		inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }
		inline double SmoothStep(double A, double B, double V)
		{
			const double T = Clamp01((V - A) / (B - A));
			return T * T * (3.0 - 2.0 * T);
		}
		inline double Fade(double T) { return T * T * T * (T * (T * 6.0 - 15.0) + 10.0); }

		/** Smooth noise, -1 to 1, one wave a unit. */
		double Noise(double X, double Y, uint64_t S)
		{
			const double FX = std::floor(X), FY = std::floor(Y);
			const int64_t IX = static_cast<int64_t>(FX), IY = static_cast<int64_t>(FY);
			const double TX = Fade(X - FX), TY = Fade(Y - FY);
			const double A = Unit(Hash(IX, IY, S)), B = Unit(Hash(IX + 1, IY, S));
			const double C = Unit(Hash(IX, IY + 1, S)), D = Unit(Hash(IX + 1, IY + 1, S));
			return 2.0 * Lerp(Lerp(A, B, TX), Lerp(C, D, TX), TY) - 1.0;
		}
		/** Noise at several sizes, each half the last and turned a little so no grid shows; about -1 to 1. */
		double Fbm(double X, double Y, uint64_t S, int Octaves)
		{
			double Sum = 0.0, Amp = 0.5, Norm = 0.0;
			for (int K = 0; K < Octaves; ++K)
			{
				Sum += Amp * Noise(X, Y, S + static_cast<uint64_t>(K) * 7919u);
				Norm += Amp;
				const double NX = X * 1.6 - Y * 1.2, NY = X * 1.2 + Y * 1.6; // (x2, turned 37 degrees)
				X = NX + 17.3;
				Y = NY - 9.1;
				Amp *= 0.5;
			}
			return Sum / Norm;
		}

		/** How much of a texel T wide a line Half each side of its middle covers, D from its middle. */
		inline double Cover(double D, double Half, double T) { return Clamp01((Half + 0.5 * T - D) / T); }
		/** How much of a texel T wide a thin line Half wide each side holds, its middle D away: never more than the line's
		 * own share of the texel, however near it runs (Cover is a wide band's edge, and makes a thin line too strong
		 * far off). */
		inline double LineCover(double D, double Half, double T)
		{
			T = std::max(1.0, T);
			return Clamp01((std::min(D + Half, 0.5 * T) - std::max(D - Half, -0.5 * T)) / T);
		}
		/** How much a pattern of cells Size wide still shows in a texel T wide: 1 when the texel is small against it, 0 when it holds many. */
		inline double Shows(double Size, double T) { return Clamp01((Size / T - 0.7) / 1.3); }

		// ---- Paint ------------------------------------------------------------------------------------------------

		/** A colour as the picture holds it (sRGB, 0 to 255) and how high what is painted stands, metres. */
		struct Ink
		{
			float R = 0.0f, G = 0.0f, B = 0.0f, H = 0.0f;
		};
		inline Ink Rgb(int R, int G, int B, double H = 0.0) { return Ink{ static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(H) }; }
		inline Ink Over(const Ink& A, const Ink& B, double T)
		{
			const float F = static_cast<float>(Clamp01(T));
			return Ink{ A.R + (B.R - A.R) * F, A.G + (B.G - A.G) * F, A.B + (B.B - A.B) * F, A.H + (B.H - A.H) * F };
		}
		inline Ink Bright(const Ink& A, double F)
		{
			const float K = static_cast<float>(F);
			return Ink{ A.R * K, A.G * K, A.B * K, A.H };
		}
		inline Ink Tall(const Ink& A, double H) { return Ink{ A.R, A.G, A.B, static_cast<float>(H) }; }
		/** Shade toward the shadow's cool grey: what the sky still lights when the sun does not. */
		inline Ink InShade(const Ink& A, double Amount)
		{
			const float F = static_cast<float>(Clamp01(Amount));
			return Ink{ A.R * (1.0f - 0.46f * F), A.G * (1.0f - 0.43f * F), A.B * (1.0f - 0.36f * F), A.H };
		}
		/** A colour a little moved by a hash: lighter or darker, warmer or cooler. */
		inline Ink Vary(const Ink& A, uint64_t H, double Light, double Warm)
		{
			const double L = 1.0 + Light * (2.0 * Unit(H, 1) - 1.0);
			const double W = Warm * (2.0 * Unit(H, 2) - 1.0);
			return Ink{ static_cast<float>(A.R * L * (1.0 + W)), static_cast<float>(A.G * L), static_cast<float>(A.B * L * (1.0 - W)), A.H };
		}

		/** The month's farmland and woods (northern Europe), by season: winter, spring, summer, autumn. */
		struct Season
		{
			Ink Pasture, WetPasture, Cereal, Plough, Stubble, Fallow, Bright; // Bright: rape in flower or ripe corn
			Ink Conifer, Broadleaf, Hedge, Garden, Grass;
			double Share[7]; // pasture, cereal, plough, stubble, fallow, bright, root (the rest)
			double Bare;     // how bare the broadleaf trees are (1 in winter): bare crowns cast thin shadows
		};
		Season SeasonOf(int Month)
		{
			// (each season's paint at its middle month; a month between two seasons takes some of both)
			static const Season Four[4] = {
				// winter (December to February; the art director, 20 November 19:31Z: the north German plain in December is
				// olive, grey-green and brown): pasture dull olive, winter corn a faint green, plough dark brown, stubble pale
				// buff, wet meadow dark, conifers green-black, bare broadleaf grey-brown with a purple cast
				// (bare broadleaf lifted to a grey-brown with a purple cast, the art director's second look, 20:08Z)
				{ Rgb(100, 100, 82), Rgb(68, 74, 63), Rgb(98, 104, 82), Rgb(84, 70, 58), Rgb(138, 128, 104), Rgb(116, 108, 88), Rgb(104, 110, 80),
					Rgb(38, 45, 39), Rgb(90, 82, 82), Rgb(64, 59, 57), Rgb(86, 86, 72), Rgb(98, 100, 80),
					{ 0.30, 0.25, 0.29, 0.05, 0.08, 0.0, 0.03 }, 1.0 },
				// spring (April): fresh grass and corn, brown seedbeds, rape in flower here and there
				{ Rgb(94, 120, 66), Rgb(86, 112, 70), Rgb(96, 126, 62), Rgb(110, 90, 70), Rgb(140, 128, 96), Rgb(122, 116, 82), Rgb(190, 176, 64),
					Rgb(42, 56, 42), Rgb(96, 104, 70), Rgb(64, 78, 48), Rgb(84, 104, 60), Rgb(92, 118, 64),
					{ 0.30, 0.32, 0.18, 0.0, 0.08, 0.06, 0.06 }, 0.3 },
				// summer (July): deep green grass, ripening corn, the first stubble
				{ Rgb(86, 114, 58), Rgb(80, 106, 62), Rgb(170, 152, 92), Rgb(118, 98, 76), Rgb(176, 162, 116), Rgb(128, 122, 82), Rgb(186, 166, 100),
					Rgb(40, 54, 38), Rgb(58, 78, 42), Rgb(52, 70, 40), Rgb(72, 96, 52), Rgb(88, 112, 58),
					{ 0.30, 0.30, 0.04, 0.12, 0.06, 0.10, 0.08 }, 0.0 },
				// autumn (October): stubble and plough, grass still green, broadleaf turning
				{ Rgb(94, 112, 68), Rgb(86, 104, 72), Rgb(104, 116, 70), Rgb(100, 82, 64), Rgb(152, 138, 102), Rgb(124, 112, 84), Rgb(132, 104, 62),
					Rgb(42, 54, 44), Rgb(112, 92, 62), Rgb(66, 66, 48), Rgb(84, 94, 62), Rgb(94, 108, 70),
					{ 0.30, 0.16, 0.32, 0.12, 0.06, 0.0, 0.04 }, 0.2 },
			};
			// (months 1..12 by the weather's seasons: winter December to February, spring March to May and so on, each
			// held most of its three months and turning to the next in its last)
			const double At = std::fmod(static_cast<double>(std::clamp(Month, 1, 12) % 12) / 3.0, 4.0);
			const int A = static_cast<int>(std::floor(At)) % 4, B = (A + 1) % 4;
			const double T = SmoothStep(0.45, 1.0, At - std::floor(At)); // hold each season most of its three months
			Season S = Four[A];
			const Season& N = Four[B];
			auto Mx = [T](Ink& X, const Ink& Y) { X = Over(X, Y, T); };
			Mx(S.Pasture, N.Pasture);
			Mx(S.WetPasture, N.WetPasture);
			Mx(S.Cereal, N.Cereal);
			Mx(S.Plough, N.Plough);
			Mx(S.Stubble, N.Stubble);
			Mx(S.Fallow, N.Fallow);
			Mx(S.Bright, N.Bright);
			Mx(S.Conifer, N.Conifer);
			Mx(S.Broadleaf, N.Broadleaf);
			Mx(S.Hedge, N.Hedge);
			Mx(S.Garden, N.Garden);
			Mx(S.Grass, N.Grass);
			for (int K = 0; K < 7; ++K)
			{
				S.Share[K] = Lerp(S.Share[K], N.Share[K], T);
			}
			S.Bare = Lerp(S.Bare, N.Bare, T);
			return S;
		}

		// ---- The coast: the board's land rings as a signed distance, metres (+ on land) -------------------------

		struct Coast
		{
			bool bAllLand = true;
			double X0 = 0.0, Y0 = 0.0, Cell = 150.0;
			int NX = 0, NY = 0;
			std::vector<float> D;

			/** Squared distances to the nearest marked cell along one row (Felzenszwalb and Huttenlocher's lower envelope). */
			static void Line(const std::vector<double>& F, std::vector<double>& Out, std::vector<int>& V, std::vector<double>& Z)
			{
				const int N = static_cast<int>(F.size());
				int K = 0;
				V[0] = 0;
				Z[0] = -Far;
				Z[1] = Far;
				for (int Q = 1; Q < N; ++Q)
				{
					double S = 0.0;
					for (;;)
					{
						const int R = V[K];
						S = ((F[Q] + static_cast<double>(Q) * Q) - (F[R] + static_cast<double>(R) * R)) / (2.0 * Q - 2.0 * R);
						if (S <= Z[K] && K > 0)
						{
							--K;
							continue;
						}
						break;
					}
					++K;
					V[K] = Q;
					Z[K] = S;
					Z[K + 1] = Far;
				}
				K = 0;
				for (int Q = 0; Q < N; ++Q)
				{
					while (Z[K + 1] < Q)
					{
						++K;
					}
					const double DQ = Q - V[K];
					Out[Q] = DQ * DQ + F[V[K]];
				}
			}

			/** Cells' squared distance (in cells) to the nearest cell where Mark is set. */
			static std::vector<double> Distance(const std::vector<uint8_t>& Mark, int W, int H)
			{
				const double Inf = 1.0e12;
				std::vector<double> G(static_cast<size_t>(W) * H);
				const int N = std::max(W, H);
				std::vector<double> F(N), Out(N), Z(N + 1);
				std::vector<int> V(N);
				for (int X = 0; X < W; ++X)
				{
					F.assign(H, 0.0);
					Out.assign(H, 0.0);
					for (int Y = 0; Y < H; ++Y)
					{
						F[Y] = Mark[static_cast<size_t>(Y) * W + X] ? 0.0 : Inf;
					}
					Line(F, Out, V, Z);
					for (int Y = 0; Y < H; ++Y)
					{
						G[static_cast<size_t>(Y) * W + X] = Out[Y];
					}
				}
				F.assign(W, 0.0);
				Out.assign(W, 0.0);
				for (int Y = 0; Y < H; ++Y)
				{
					for (int X = 0; X < W; ++X)
					{
						F[X] = G[static_cast<size_t>(Y) * W + X];
					}
					Line(F, Out, V, Z);
					for (int X = 0; X < W; ++X)
					{
						G[static_cast<size_t>(Y) * W + X] = Out[X];
					}
				}
				return G;
			}

			void Build(const std::vector<std::vector<Pt>>& Rings, double BX0, double BY0, double BX1, double BY1)
			{
				bAllLand = Rings.empty();
				if (bAllLand)
				{
					return;
				}
				X0 = BX0;
				Y0 = BY0;
				NX = static_cast<int>(std::ceil((BX1 - BX0) / Cell)) + 1;
				NY = static_cast<int>(std::ceil((BY1 - BY0) / Cell)) + 1;
				std::vector<uint8_t> Land(static_cast<size_t>(NX) * NY, 0);
				// (each row of cells filled between the rings' crossings of its middle, odd crossings in)
				std::vector<double> Cross;
				for (int J = 0; J < NY; ++J)
				{
					const double Y = Y0 + J * Cell;
					Cross.clear();
					for (const std::vector<Pt>& Ring : Rings)
					{
						const size_t N = Ring.size();
						for (size_t K = 0; K < N; ++K)
						{
							const Pt& A = Ring[K];
							const Pt& B = Ring[(K + 1) % N];
							if ((A.second <= Y) != (B.second <= Y))
							{
								Cross.push_back(A.first + (Y - A.second) / (B.second - A.second) * (B.first - A.first));
							}
						}
					}
					std::sort(Cross.begin(), Cross.end());
					for (size_t K = 0; K + 1 < Cross.size(); K += 2)
					{
						const int I0 = std::max(0, static_cast<int>(std::ceil((Cross[K] - X0) / Cell)));
						const int I1 = std::min(NX - 1, static_cast<int>(std::floor((Cross[K + 1] - X0) / Cell)));
						for (int I = I0; I <= I1; ++I)
						{
							Land[static_cast<size_t>(J) * NX + I] = 1;
						}
					}
				}
				std::vector<uint8_t> Sea(Land.size());
				bool bAnySea = false, bAnyLand = false;
				for (size_t K = 0; K < Land.size(); ++K)
				{
					Sea[K] = Land[K] ? 0 : 1;
					bAnySea = bAnySea || Sea[K];
					bAnyLand = bAnyLand || Land[K];
				}
				D.assign(Land.size(), 0.0f);
				if (!bAnySea || !bAnyLand)
				{
					// (all one or the other over the whole extent: far from any coast; all land, as an inland board's one
					// ring round it, is a board with no sea, and says so: no ", the coast" in the log)
					bAllLand = !bAnySea;
					for (size_t K = 0; K < Land.size(); ++K)
					{
						D[K] = Land[K] ? 1.0e6f : -1.0e6f;
					}
					return;
				}
				const std::vector<double> ToSea = Distance(Sea, NX, NY);
				const std::vector<double> ToLand = Distance(Land, NX, NY);
				for (size_t K = 0; K < Land.size(); ++K)
				{
					// (the coast lies half a cell from the cells on either side of it)
					D[K] = static_cast<float>(Land[K] ? (std::sqrt(ToSea[K]) - 0.5) * Cell : -(std::sqrt(ToLand[K]) - 0.5) * Cell);
				}
			}

			double At(double X, double Y) const
			{
				if (bAllLand)
				{
					return 1.0e6;
				}
				const double FX = std::clamp((X - X0) / Cell, 0.0, NX - 1.001), FY = std::clamp((Y - Y0) / Cell, 0.0, NY - 1.001);
				const int I = static_cast<int>(FX), J = static_cast<int>(FY);
				const double TX = FX - I, TY = FY - J;
				const size_t K = static_cast<size_t>(J) * NX + I;
				return Lerp(Lerp(D[K], D[K + 1], TX), Lerp(D[K + NX], D[K + NX + 1], TX), TY);
			}
		};

		// ---- Lines: rivers, roads, railways ------------------------------------------------------------------------

		enum class LineKind : uint8_t
		{
			River,
			Road,     // a country road
			MainRoad, // out of a town
			Autobahn,
			Rail,
		};
		enum : uint8_t
		{
			Avenue = 1,  // trees along both sides
			Bridged = 2, // may cross a river on a bridge
			Embanked = 4 // (a river) walled quays: in a town
		};

		struct Seg
		{
			double AX = 0.0, AY = 0.0, BX = 0.0, BY = 0.0;
			double Len = 1.0, Along = 0.0; // the line's length up to A
			float HalfA = 1.0f, HalfB = 1.0f;
			LineKind Kind = LineKind::Road;
			uint8_t Flags = 0;
			int Line = 0;
		};

		/** The nearest of each kind of line to a point. */
		struct Near
		{
			// 0 water, 1 road, 2 rail
			double D[3] = { Far, Far, Far };
			double Half[3] = { 0.0, 0.0, 0.0 };
			double Along[3] = { 0.0, 0.0, 0.0 };
			double DirX[3] = { 1.0, 1.0, 1.0 }, DirY[3] = { 0.0, 0.0, 0.0 };
			double Side[3] = { 0.0, 0.0, 0.0 }; // + on the left of the line's way
			const Seg* S[3] = { nullptr, nullptr, nullptr };
		};

		inline int ClassOf(LineKind K) { return K == LineKind::River ? 0 : K == LineKind::Rail ? 2 : 1; }

		struct Lines
		{
			std::vector<Seg> Segs;
			double X0 = 0.0, Y0 = 0.0, Cell = 400.0;
			int NX = 0, NY = 0;
			std::vector<std::vector<int>> Grid;
			std::vector<int> RiverSegs;
			int NextLine = 0;

			/** A line through points with half widths at its ends (a river widens downstream). */
			void Add(const std::vector<Pt>& Pts, LineKind Kind, double Half0, double Half1, uint8_t Flags)
			{
				if (Pts.size() < 2)
				{
					return;
				}
				double Total = 0.0;
				for (size_t K = 1; K < Pts.size(); ++K)
				{
					Total += std::hypot(Pts[K].first - Pts[K - 1].first, Pts[K].second - Pts[K - 1].second);
				}
				double Along = 0.0;
				const int Id = NextLine++;
				for (size_t K = 1; K < Pts.size(); ++K)
				{
					Seg S;
					S.AX = Pts[K - 1].first;
					S.AY = Pts[K - 1].second;
					S.BX = Pts[K].first;
					S.BY = Pts[K].second;
					S.Len = std::max(0.01, std::hypot(S.BX - S.AX, S.BY - S.AY));
					S.Along = Along;
					S.HalfA = static_cast<float>(Lerp(Half0, Half1, Total > 0 ? Along / Total : 0.0));
					Along += S.Len;
					S.HalfB = static_cast<float>(Lerp(Half0, Half1, Total > 0 ? Along / Total : 1.0));
					S.Kind = Kind;
					S.Flags = Flags;
					S.Line = Id;
					if (Kind == LineKind::River)
					{
						RiverSegs.push_back(static_cast<int>(Segs.size()));
					}
					Segs.push_back(S);
				}
			}

			void Index(double BX0, double BY0, double BX1, double BY1)
			{
				X0 = BX0;
				Y0 = BY0;
				NX = static_cast<int>(std::ceil((BX1 - BX0) / Cell)) + 1;
				NY = static_cast<int>(std::ceil((BY1 - BY0) / Cell)) + 1;
				Grid.assign(static_cast<size_t>(NX) * NY, {});
				for (int K = 0; K < static_cast<int>(Segs.size()); ++K)
				{
					const Seg& S = Segs[K];
					// (room for the banks, the trees along an avenue, a river's meadows are asked separately)
					const double Pad = std::max(S.HalfA, S.HalfB) + 40.0;
					const int I0 = std::max(0, static_cast<int>(std::floor((std::min(S.AX, S.BX) - Pad - X0) / Cell)));
					const int I1 = std::min(NX - 1, static_cast<int>(std::floor((std::max(S.AX, S.BX) + Pad - X0) / Cell)));
					const int J0 = std::max(0, static_cast<int>(std::floor((std::min(S.AY, S.BY) - Pad - Y0) / Cell)));
					const int J1 = std::min(NY - 1, static_cast<int>(std::floor((std::max(S.AY, S.BY) + Pad - Y0) / Cell)));
					for (int J = J0; J <= J1; ++J)
					{
						for (int I = I0; I <= I1; ++I)
						{
							Grid[static_cast<size_t>(J) * NX + I].push_back(K);
						}
					}
				}
			}

			void Query(double X, double Y, Near& Out) const { QueryAround(X, Y, 0, Out); }

			/** As Query, over the cells within Rings of the point's (so lines up to Rings cells off are found). */
			void QueryAround(double X, double Y, int Rings, Near& Out) const
			{
				const int CI = static_cast<int>(std::floor((X - X0) / Cell)), CJ = static_cast<int>(std::floor((Y - Y0) / Cell));
				for (int J = CJ - Rings; J <= CJ + Rings; ++J)
				{
					for (int I = CI - Rings; I <= CI + Rings; ++I)
					{
						if (I >= 0 && J >= 0 && I < NX && J < NY)
						{
							Consider(Grid[static_cast<size_t>(J) * NX + I], X, Y, Out);
						}
					}
				}
			}

			void Consider(const std::vector<int>& Look, double X, double Y, Near& Out) const
			{
				for (const int K : Look)
				{
					const Seg& S = Segs[K];
					const double DX = (S.BX - S.AX) / S.Len, DY = (S.BY - S.AY) / S.Len;
					const double T = std::clamp((X - S.AX) * DX + (Y - S.AY) * DY, 0.0, S.Len);
					const double PX = S.AX + DX * T, PY = S.AY + DY * T;
					const double Half = Lerp(S.HalfA, S.HalfB, T / S.Len);
					const double D = std::hypot(X - PX, Y - PY);
					const int C = ClassOf(S.Kind);
					// (nearest by its edge, so a wide river is found before a narrow one whose middle is nearer)
					if (D - Half < Out.D[C] - Out.Half[C])
					{
						Out.D[C] = D;
						Out.Half[C] = Half;
						Out.Along[C] = S.Along + T;
						Out.DirX[C] = DX;
						Out.DirY[C] = DY;
						Out.Side[C] = DX * (Y - S.AY) - DY * (X - S.AX);
						Out.S[C] = &S;
					}
				}
			}

			/** Whether a straight piece crosses a river wider than MinHalf each side (and where). */
			bool CrossesRiver(double AX, double AY, double BX, double BY, double MinHalf) const
			{
				// (a piece shorter than a cell: the river pieces of the cells round its middle; else every river piece)
				const std::vector<int>* Look = &RiverSegs;
				if (NX > 0 && std::hypot(BX - AX, BY - AY) < Cell * 0.5)
				{
					const int I = static_cast<int>(std::floor(((AX + BX) * 0.5 - X0) / Cell)), J = static_cast<int>(std::floor(((AY + BY) * 0.5 - Y0) / Cell));
					if (I < 0 || J < 0 || I >= NX || J >= NY)
					{
						return false;
					}
					Look = &Grid[static_cast<size_t>(J) * NX + I];
				}
				for (const int K : *Look)
				{
					const Seg& S = Segs[K];
					if (S.Kind != LineKind::River || std::max(S.HalfA, S.HalfB) < MinHalf)
					{
						continue;
					}
					const double D1 = (BX - AX) * (S.AY - AY) - (BY - AY) * (S.AX - AX);
					const double D2 = (BX - AX) * (S.BY - AY) - (BY - AY) * (S.BX - AX);
					const double D3 = (S.BX - S.AX) * (AY - S.AY) - (S.BY - S.AY) * (AX - S.AX);
					const double D4 = (S.BX - S.AX) * (BY - S.AY) - (S.BY - S.AY) * (BX - S.AX);
					if ((D1 > 0) != (D2 > 0) && (D3 > 0) != (D4 > 0))
					{
						return true;
					}
				}
				return false;
			}
		};

		/** The corners of a line rounded off (Chaikin's cuts at a quarter and three quarters, Times over), its ends kept. */
		std::vector<Pt> Rounded(std::vector<Pt> Pts, int Times)
		{
			for (int R = 0; R < Times && Pts.size() > 2; ++R)
			{
				std::vector<Pt> Out;
				Out.push_back(Pts.front());
				for (size_t K = 1; K < Pts.size(); ++K)
				{
					const Pt& A = Pts[K - 1];
					const Pt& B = Pts[K];
					if (K > 1)
					{
						Out.push_back({ A.first * 0.75 + B.first * 0.25, A.second * 0.75 + B.second * 0.25 });
					}
					if (K + 1 < Pts.size())
					{
						Out.push_back({ A.first * 0.25 + B.first * 0.75, A.second * 0.25 + B.second * 0.75 });
					}
				}
				Out.push_back(Pts.back());
				Pts = std::move(Out);
			}
			return Pts;
		}

		/** Where a line is held still (a river by the quays a plan lays along it): the swing is nothing within R of the
		 * point and full beyond 2 R. */
		struct Calm
		{
			struct Disc
			{
				double X = 0.0, Y = 0.0, R = 0.0;
			};
			std::vector<Disc> Discs;
			void Add(double X, double Y, double R)
			{
				if (R > 0.0)
				{
					Discs.push_back({ X, Y, R });
				}
			}
			double Of(double PX, double PY) const
			{
				double F = 1.0;
				for (const Disc& D : Discs)
				{
					F = std::min(F, SmoothStep(D.R, 2.0 * D.R, std::hypot(PX - D.X, PY - D.Y)));
				}
				return F;
			}
		};

		/** A line's points with meanders: its corners rounded, cut to Step metres and moved aside by slow noise, Amp at
		 * most, the swing growing from nothing over half a wave at either end so that it still meets what it is drawn to. */
		std::vector<Pt> Meander(const std::vector<Pt>& Given, double Step, double Amp, double Wave, uint64_t S, const Calm& Still = Calm(), int Octaves = 3)
		{
			const std::vector<Pt> Pts = Rounded(Given, 3);
			std::vector<Pt> Out;
			if (Pts.size() < 2)
			{
				return Pts;
			}
			double Total = 0.0;
			for (size_t K = 1; K < Pts.size(); ++K)
			{
				Total += std::hypot(Pts[K].first - Pts[K - 1].first, Pts[K].second - Pts[K - 1].second);
			}
			const double Ease = std::min(Wave * 0.5, Total * 0.3);
			double Along = 0.0;
			for (size_t K = 1; K < Pts.size(); ++K)
			{
				const double AX = Pts[K - 1].first, AY = Pts[K - 1].second, BX = Pts[K].first, BY = Pts[K].second;
				const double L = std::hypot(BX - AX, BY - AY);
				const int N = std::max(1, static_cast<int>(std::ceil(L / Step)));
				const double NX = L > 0 ? -(BY - AY) / L : 0.0, NY = L > 0 ? (BX - AX) / L : 0.0;
				for (int I = (K == 1 ? 0 : 1); I <= N; ++I)
				{
					const double T = static_cast<double>(I) / N;
					const double At = Along + T * L;
					const double PX = AX + (BX - AX) * T, PY = AY + (BY - AY) * T;
					const double Held = SmoothStep(0.0, Ease, At) * SmoothStep(0.0, Ease, Total - At) * Still.Of(PX, PY);
					const double Off = Amp * Held * Fbm(At / Wave, 0.37, S, Octaves);
					Out.push_back({ PX + NX * Off, PY + NY * Off });
				}
				Along += L;
			}
			return Out;
		}

		// ---- The town --------------------------------------------------------------------------------------------

		enum class Ground : uint8_t
		{
			None,
			Old,      // the old town's crooked blocks
			Block,    // courtyard blocks of the newer town
			Terrace,  // rows of houses with long back gardens
			Villa,    // houses in gardens
			Allot,    // allotments
			Works,    // halls, yards, chimneys
			Docks,    // quays and sheds round the basins
			Yard,     // a railway yard's many tracks
			Park,
			Cemetery,
			Airfield,
			Station,  // the train shed
			Basin,    // harbour water
			Shipyard, // slips and halls on the river
			Meadow,   // open marsh meadow inside the town's spread (kept open)
		};

		Ground GroundNamed(const std::string& N)
		{
			static const std::pair<const char*, Ground> Names[] = {
				{ "old", Ground::Old }, { "block", Ground::Block }, { "terrace", Ground::Terrace }, { "villa", Ground::Villa },
				{ "allotments", Ground::Allot }, { "works", Ground::Works }, { "docks", Ground::Docks }, { "yard", Ground::Yard },
				{ "park", Ground::Park }, { "cemetery", Ground::Cemetery }, { "airfield", Ground::Airfield }, { "station", Ground::Station },
				{ "basin", Ground::Basin }, { "shipyard", Ground::Shipyard }, { "meadow", Ground::Meadow },
			};
			for (const auto& P : Names)
			{
				if (N == P.first)
				{
					return P.second;
				}
			}
			return Ground::None;
		}

		/** A piece of a town plan: a polygon, or a strip (a yard, a basin) from A to B so wide; compass metres. */
		struct Zone
		{
			Ground Kind = Ground::None;
			std::vector<Pt> Poly;
			double MinE = 0, MinN = 0, MaxE = 0, MaxN = 0;
			// (a strip's axis, for the tracks of a yard and the lie of a basin's sheds)
			double AxisE = 1.0, AxisN = 0.0;
			bool bStrip = false;
			double AE = 0, AN = 0, BE = 0, BN = 0, Half = 0;

			void Bound()
			{
				MinE = MinN = Far;
				MaxE = MaxN = -Far;
				for (const Pt& P : Poly)
				{
					MinE = std::min(MinE, P.first);
					MaxE = std::max(MaxE, P.first);
					MinN = std::min(MinN, P.second);
					MaxN = std::max(MaxN, P.second);
				}
			}
			bool Holds(double E, double N) const
			{
				if (E < MinE || E > MaxE || N < MinN || N > MaxN)
				{
					return false;
				}
				bool In = false;
				const size_t C = Poly.size();
				for (size_t I = 0, J = C - 1; I < C; J = I++)
				{
					const Pt& A = Poly[I];
					const Pt& B = Poly[J];
					if ((A.second > N) != (B.second > N) && E < (B.first - A.first) * (N - A.second) / (B.second - A.second) + A.first)
					{
						In = !In;
					}
				}
				return In;
			}
			/** How far inside the zone's edge (metres; negative outside), for a strip only (others: a large number when in). */
			double Inside(double E, double N) const
			{
				if (!bStrip)
				{
					return Holds(E, N) ? 1.0e4 : -1.0;
				}
				const double L = std::hypot(BE - AE, BN - AN);
				const double U = (E - AE) * AxisE + (N - AN) * AxisN;
				const double V = -(E - AE) * AxisN + (N - AN) * AxisE;
				return std::min({ U, L - U, Half - std::fabs(V) });
			}
		};

		struct Spire
		{
			double X = 0.0, Y = 0.0, High = 70.0;
			bool bChurch = true; // with its nave (east to west); else a chimney
		};

		struct Town
		{
			std::string Name;
			double E = 0.0, N = 0.0;             // its middle, compass metres from the aim
			double Along = 3000.0, Across = 3000.0, Axis = 0.0; // the built-up spread (half-axes) and the bearing of the long one (radians, from north toward east)
			double CoreE = 0.0, CoreN = 0.0, CoreR = 500.0;
			double CoreLong = 1.0, CoreAxis = 0.0; // the old town's long half-axis over CoreR (an ellipse of the same area), its bearing
			/** How far out from the old town's middle a point is, in metres of a round town of the same area. */
			double CoreReach(double PE, double PN) const
			{
				const double DE = PE - CoreE, DN = PN - CoreN;
				const double U = DE * std::sin(CoreAxis) + DN * std::cos(CoreAxis), V = DE * std::cos(CoreAxis) - DN * std::sin(CoreAxis);
				return std::hypot(U / CoreLong, V * CoreLong);
			}
			bool bRampart = false;
			int CoreSide = 0; // which side of its river the old town lies (Near::Side's sign), 0 none near
			double RampartR = 650.0;
			int Bastions = 9;
			std::vector<Zone> Zones;
			std::vector<Pt> Gasometers;          // compass metres
			// Each super-block's street grid turned to the river, main road or railway nearest its seed (radians from
			// east toward north; NaN where none is near): by the super-blocks' cells, I0 and J0 the first's.
			int SuperI0 = 0, SuperJ0 = 0, SuperNI = 0, SuperNJ = 0;
			std::vector<float> SuperAngle;
		};

		struct Village
		{
			double X = 0.0, Y = 0.0, R = 250.0;
			uint64_t H = 0;
			// (its long way, a unit vector on the board, and how long it is over a round village of the same area: a village
			// on a road is strung out along it)
			double AxisX = 1.0, AxisY = 0.0, Long = 1.0;
			double Reach() const { return R * std::sqrt(Long) * 1.9; }
		};

		/** Where a point is against the nearest seed of a jittered grid (cells one unit wide): the seed, its cell, and the distance to the cells' border. */
		struct Cell
		{
			int64_t IX = 0, IY = 0;
			double SX = 0.0, SY = 0.0;   // the seed, in units
			double Border = 0.0;         // distance to the nearest border with another cell, units
			int64_t OX = 0, OY = 0;      // the cell across that border
		};
		Cell Voronoi(double X, double Y, uint64_t S, double Jitter = 0.8)
		{
			const int64_t BX = static_cast<int64_t>(std::floor(X)), BY = static_cast<int64_t>(std::floor(Y));
			Cell Best;
			double Md = Far;
			for (int64_t J = -1; J <= 1; ++J)
			{
				for (int64_t I = -1; I <= 1; ++I)
				{
					const uint64_t H = Hash(BX + I, BY + J, S);
					const double SX = BX + I + 0.5 + Jitter * (Unit(H, 1) - 0.5), SY = BY + J + 0.5 + Jitter * (Unit(H, 2) - 0.5);
					const double D = (SX - X) * (SX - X) + (SY - Y) * (SY - Y);
					if (D < Md)
					{
						Md = D;
						Best.IX = BX + I;
						Best.IY = BY + J;
						Best.SX = SX;
						Best.SY = SY;
					}
				}
			}
			double Mb = Far;
			for (int64_t J = -2; J <= 2; ++J)
			{
				for (int64_t I = -2; I <= 2; ++I)
				{
					const int64_t CX = Best.IX + I, CY = Best.IY + J;
					if (CX == Best.IX && CY == Best.IY)
					{
						continue;
					}
					const uint64_t H = Hash(CX, CY, S);
					const double SX = CX + 0.5 + Jitter * (Unit(H, 1) - 0.5), SY = CY + 0.5 + Jitter * (Unit(H, 2) - 0.5);
					const double DX = SX - Best.SX, DY = SY - Best.SY;
					const double L = std::sqrt(DX * DX + DY * DY);
					const double B = ((X - (Best.SX + SX) * 0.5) * DX + (Y - (Best.SY + SY) * 0.5) * DY) / L;
					if (-B < Mb)
					{
						Mb = -B;
						Best.OX = CX;
						Best.OY = CY;
					}
				}
			}
			Best.Border = Mb;
			return Best;
		}

		std::vector<Pt> PointsOf(const AirFeed::Json& J, double Scale)
		{
			std::vector<Pt> Out;
			for (const AirFeed::Json& P : J.Items)
			{
				Out.push_back({ P.At(0).Num() * Scale, P.At(1).Num() * Scale });
			}
			return Out;
		}
	}

	// ---- The painter ----------------------------------------------------------------------------------------------

	struct Painter::Impl
	{
		Place P;
		Tally Count;
		uint64_t Seed = 1;
		double NX = 0.0, NY = 1.0, EX = 1.0, EY = 0.0; // true north and east on the board
		double SunDX = 0.5, SunDY = -0.86, TanSun = 0.58; // toward the sun along the ground (unit), and its rise over the ground
		Season Sea4;
		Coast Shore;
		Lines L;
		std::vector<Village> Villages;
		double VillageCell = 2600.0;
		int VillageI0 = 0, VillageJ0 = 0, VillageNI = 0, VillageNJ = 0;
		std::vector<std::vector<int>> VillageGrid;
		std::vector<Town> Towns;
		std::vector<Spire> Spires;
		double SpireCell = 500.0, SpireX0 = 0.0, SpireY0 = 0.0;
		int SpireNX = 0, SpireNY = 0;
		std::vector<std::vector<int>> SpireGrid;
		double TownMinX = Far, TownMinY = Far, TownMaxX = -Far, TownMaxY = -Far; // board box round every town's spread

		// The country's character (Terrain): woods, plantation blocks, how many blocks are bounded by hedges (else
		// ditches and tracks), the blocks' size, the strips' widths, villages a cell.
		double ForestShare = 0.12, PlantShare = 0.07, HedgeShare = 0.55, BlockSize = 850.0, StripMin = 40.0, StripMax = 140.0, VillageChance = 0.7;
		bool bMarsh = false;

		// ---- Frames of reference

		void ToCompass(double X, double Y, double& E, double& N) const
		{
			const double DX = X - P.AimX, DY = Y - P.AimY;
			E = DX * EX + DY * EY;
			N = DX * NX + DY * NY;
		}
		Pt ToBoard(double E, double N) const { return { P.AimX + E * EX + N * NX, P.AimY + E * EY + N * NY }; }
		std::vector<Pt> ToBoard(const std::vector<Pt>& Pts) const
		{
			std::vector<Pt> Out;
			Out.reserve(Pts.size());
			for (const Pt& Q : Pts)
			{
				Out.push_back(ToBoard(Q.first, Q.second));
			}
			return Out;
		}

		// ---- Building what the texels ask of

		void Character()
		{
			const std::string& T = P.Terrain;
			if (T == "forest")
			{
				ForestShare = 0.42;
				PlantShare = 0.12;
				VillageChance = 0.5;
			}
			else if (T == "hills")
			{
				ForestShare = 0.28;
				PlantShare = 0.1;
				HedgeShare = 0.65;
				BlockSize = 650.0;
				StripMin = 50.0;
				StripMax = 160.0;
			}
			else if (T == "mountain")
			{
				ForestShare = 0.5;
				PlantShare = 0.12;
				BlockSize = 550.0;
				StripMin = 45.0;
				StripMax = 120.0;
				VillageChance = 0.45;
			}
			else if (T == "marsh")
			{
				ForestShare = 0.05;
				PlantShare = 0.03;
				HedgeShare = 0.15;
				StripMin = 25.0;
				StripMax = 70.0;
				bMarsh = true;
			}
			else if (T == "urban")
			{
				ForestShare = 0.08;
				VillageChance = 0.85;
			}
		}

		bool LoadPlan(const std::string& Json)
		{
			if (Json.empty())
			{
				return false;
			}
			AirFeed::Json Book;
			std::string Why;
			if (!AirFeed::Parse(Json, Book, Why))
			{
				return false;
			}
			const AirFeed::Json& Plan = Book["towns"][P.Name];
			if (!Plan.IsObject())
			{
				return false;
			}
			Count.Plan = P.Name;
			const double K = 1000.0; // (the book is in kilometres)
			Town T;
			T.Name = P.Name;
			T.E = Plan["middle"].At(0).Num() * K;
			T.N = Plan["middle"].At(1).Num() * K;
			T.Along = Plan["spread"].At(0).Num(3.0) * K;
			T.Across = Plan["spread"].At(1).Num(3.0) * K;
			T.Axis = Plan["axis"].Num(0.0) * Pi / 180.0;
			const AirFeed::Json& Core = Plan["core"];
			T.CoreE = Core["at"].At(0).Num() * K;
			T.CoreN = Core["at"].At(1).Num() * K;
			T.CoreR = Core["radius"].Num(0.5) * K;
			T.CoreLong = std::sqrt(std::max(0.2, Core["long"].Num(1.0)));
			T.CoreAxis = Core["axis"].Num(0.0) * Pi / 180.0;
			T.bRampart = Core["rampart"].Truth(false);
			T.RampartR = Core["rampartRadius"].Num(0.65) * K;
			T.Bastions = Core["bastions"].Int(9);
			for (const AirFeed::Json& Z : Plan["zones"].Items)
			{
				Zone Out;
				Out.Kind = GroundNamed(Z["kind"].Str(""));
				if (Out.Kind == Ground::None)
				{
					continue;
				}
				if (Z["poly"].IsArray())
				{
					Out.Poly = PointsOf(Z["poly"], K);
				}
				else if (Z["from"].IsArray())
				{
					// a strip: from, to, wide
					Out.bStrip = true;
					Out.AE = Z["from"].At(0).Num() * K;
					Out.AN = Z["from"].At(1).Num() * K;
					Out.BE = Z["to"].At(0).Num() * K;
					Out.BN = Z["to"].At(1).Num() * K;
					Out.Half = Z["wide"].Num(0.2) * K * 0.5;
					const double Len = std::max(1.0, std::hypot(Out.BE - Out.AE, Out.BN - Out.AN));
					Out.AxisE = (Out.BE - Out.AE) / Len;
					Out.AxisN = (Out.BN - Out.AN) / Len;
					const double SE = -Out.AxisN * Out.Half, SN = Out.AxisE * Out.Half;
					Out.Poly = { { Out.AE + SE, Out.AN + SN }, { Out.BE + SE, Out.BN + SN }, { Out.BE - SE, Out.BN - SN }, { Out.AE - SE, Out.AN - SN } };
				}
				else if (Z["at"].IsArray())
				{
					// a disc, as a polygon
					const double CE = Z["at"].At(0).Num() * K, CN = Z["at"].At(1).Num() * K, R = Z["radius"].Num(0.5) * K;
					for (int A = 0; A < 24; ++A)
					{
						Out.Poly.push_back({ CE + R * std::cos(A * Pi / 12.0), CN + R * std::sin(A * Pi / 12.0) });
					}
				}
				if (Out.Poly.size() < 3)
				{
					continue;
				}
				Out.Bound();
				T.Zones.push_back(Out);
			}
			T.Gasometers = PointsOf(Plan["gasometers"], K);
			Towns.push_back(T);

			// The lines of the plan: rivers (with their half widths at the ends, metres), railways, main roads.
			for (const AirFeed::Json& R : Plan["rivers"].Items)
			{
				std::vector<Pt> Course = PointsOf(R["course"], K);
				const double W0 = R["wide"].At(0).Num(120.0), W1 = R["wide"].At(1).Num(W0);
				std::vector<Pt> Board = ToBoard(Course);
				ToSea(Board);
				const uint64_t S = Mix(Seed ^ Hash(static_cast<int64_t>(W0), static_cast<int64_t>(Course.size()), 41));
				Calm Still;
				{
					const Pt Middle = ToBoard(T.CoreE, T.CoreN);
					Still.Add(Middle.first, Middle.second, R["calm"].Num(0.0) * K);
				}
				Board = Meander(Board, 60.0, R["meander"].Num(160.0), R["wave"].Num(2600.0), S, Still);
				L.Add(Board, LineKind::River, W0 * 0.5, W1 * 0.5, 0);
			}
			// (the plan's lines are held where the town's streets and yards were laid to them; out in the country a railway
			// runs in long gentle curves and a main road wanders: the art director, 20 November 19:31Z, no ruler lines)
			// (the roads held in the middle of the town; the railways there and at the station and every yard they run into)
			Calm InTown, AtYards;
			{
				const Pt Middle = ToBoard(T.E, T.N);
				InTown.Add(Middle.first, Middle.second, 0.35 * std::max(T.Along, T.Across));
				AtYards.Add(Middle.first, Middle.second, 2000.0);
				for (const Zone& Z : T.Zones)
				{
					if (Z.bStrip && (Z.Kind == Ground::Yard || Z.Kind == Ground::Station))
					{
						const Pt A = ToBoard(Z.AE, Z.AN), B = ToBoard(Z.BE, Z.BN);
						AtYards.Add(A.first, A.second, 700.0);
						AtYards.Add(B.first, B.second, 700.0);
					}
				}
			}
			int LineNo = 0;
			for (const AirFeed::Json& R : Plan["rails"].Items)
			{
				std::vector<Pt> Board = Meander(ToBoard(PointsOf(R, K)), 80.0, 650.0, 14000.0, Mix(Seed ^ (77 + 131 * LineNo++)), AtYards, 2);
				L.Add(Board, LineKind::Rail, 7.0, 7.0, Bridged);
			}
			for (const AirFeed::Json& R : Plan["roads"].Items)
			{
				std::vector<Pt> Board = Meander(ToBoard(PointsOf(R, K)), 50.0, 260.0, 3200.0, Mix(Seed ^ (78 + 131 * LineNo++)), InTown, 3);
				L.Add(Board, LineKind::MainRoad, 8.0, 6.0, Bridged | Avenue);
			}
			for (const AirFeed::Json& R : Plan["autobahn"].Items)
			{
				std::vector<Pt> Board = Meander(ToBoard(PointsOf(R, K)), 100.0, 500.0, 16000.0, Mix(Seed ^ (79 + 131 * LineNo++)), InTown, 1);
				L.Add(Board, LineKind::Autobahn, 13.0, 13.0, Bridged);
			}
			for (const AirFeed::Json& Q : Plan["spires"].Items)
			{
				const Pt B = ToBoard(Q.At(0).Num() * K, Q.At(1).Num() * K);
				Spires.push_back({ B.first, B.second, Q.At(2).Num(75.0), true });
			}
			return true;
		}

		/** A town made up for a target the book does not have: its size from the country, two railways, main roads. */
		void MakeTown()
		{
			Count.Plan = "made";
			Town T;
			T.Name = P.Name;
			const double Size = P.Terrain == "urban" ? 4500.0 : 3200.0;
			T.Along = Size * (1.0 + 0.3 * Unit(Seed, 3));
			T.Across = Size * (0.75 + 0.2 * Unit(Seed, 4));
			T.Axis = 2.0 * Pi * Unit(Seed, 5);
			T.CoreR = Size * 0.16;
			// works by the railway and a yard, two gasometers
			const double A = 2.0 * Pi * Unit(Seed, 6);
			const double CA = std::cos(A), SA = std::sin(A);
			auto Strip = [&T](Ground Kind, double AE, double AN, double BE, double BN, double Wide)
			{
				Zone Z;
				Z.Kind = Kind;
				Z.bStrip = true;
				Z.AE = AE;
				Z.AN = AN;
				Z.BE = BE;
				Z.BN = BN;
				Z.Half = Wide * 0.5;
				const double Len = std::max(1.0, std::hypot(BE - AE, BN - AN));
				Z.AxisE = (BE - AE) / Len;
				Z.AxisN = (BN - AN) / Len;
				const double SE = -Z.AxisN * Z.Half, SN = Z.AxisE * Z.Half;
				Z.Poly = { { AE + SE, AN + SN }, { BE + SE, BN + SN }, { BE - SE, BN - SN }, { AE - SE, AN - SN } };
				Z.Bound();
				T.Zones.push_back(Z);
			};
			const double R1 = Size * 0.45, R2 = Size * 0.95;
			Strip(Ground::Yard, CA * R1, SA * R1, CA * R2, SA * R2, 220.0);
			Strip(Ground::Works, CA * R1 - SA * 500.0, SA * R1 + CA * 500.0, CA * R2 - SA * 650.0, SA * R2 + CA * 650.0, 600.0);
			T.Gasometers = { { CA * R1 + SA * 380.0, SA * R1 - CA * 380.0 } };
			Towns.push_back(T);
			// two railways through the middle, out to well past the board
			for (int K = 0; K < 2; ++K)
			{
				const double B = A + K * (Pi * (0.55 + 0.3 * Unit(Seed, 7 + K)));
				std::vector<Pt> Compass = { { -std::cos(B) * 120000.0, -std::sin(B) * 120000.0 }, { -std::cos(B) * 1500.0, -std::sin(B) * 1500.0 }, { 0.0, 0.0 },
					{ std::cos(B) * 1500.0, std::sin(B) * 1500.0 }, { std::cos(B) * 120000.0, std::sin(B) * 120000.0 } };
				Calm InTown;
				InTown.Add(P.AimX, P.AimY, Size * 0.6);
				L.Add(Meander(ToBoard(Compass), 80.0, 650.0, 14000.0, Mix(Seed ^ (90 + K)), InTown, 2), LineKind::Rail, 7.0, 7.0, Bridged);
			}
			// the old town's churches
			const int Churches = 3 + static_cast<int>(4 * Unit(Seed, 9));
			for (int K = 0; K < Churches; ++K)
			{
				const double R = T.CoreR * 0.8 * std::sqrt(Unit(Seed, 20 + K)), B = 2.0 * Pi * Unit(Seed, 40 + K);
				const Pt Q = ToBoard(R * std::cos(B), R * std::sin(B));
				Spires.push_back({ Q.first, Q.second, 60.0 + 30.0 * Unit(Seed, 60 + K), true });
			}
		}

		/** A river's downstream end carried on to the sea, if the board's coast is near enough. */
		void ToSea(std::vector<Pt>& Course) const
		{
			if (Shore.bAllLand || Course.size() < 2)
			{
				return;
			}
			const Pt End = Course.back();
			if (Shore.At(End.first, End.second) < 0.0)
			{
				return;
			}
			// (the nearest sea within 45 km, by a ring of looks)
			double Best = Far;
			Pt To = End;
			for (double R = 1000.0; R <= 45000.0; R += 1000.0)
			{
				for (int A = 0; A < 72; ++A)
				{
					const double X = End.first + R * std::cos(A * Pi / 36.0), Y = End.second + R * std::sin(A * Pi / 36.0);
					if (Shore.At(X, Y) < -600.0 && R < Best)
					{
						Best = R;
						To = { X, Y };
					}
				}
				if (Best < Far)
				{
					break;
				}
			}
			if (Best < Far)
			{
				const Pt Mid = { (End.first + To.first) * 0.5, (End.second + To.second) * 0.5 };
				Course.push_back(Mid);
				Course.push_back(To);
			}
		}

		bool OnWater(double X, double Y, double Margin) const
		{
			if (Shore.At(X, Y) < Margin)
			{
				return true;
			}
			Near Nr;
			L.Query(X, Y, Nr);
			return Nr.D[0] < Nr.Half[0] + Margin;
		}

		/** How far a board point lies inside any town's spread, as a share of its half-axes (0 in the middle, 1 at its edge). */
		double TownReach(double X, double Y) const
		{
			double Best = Far;
			for (const Town& T : Towns)
			{
				double E = 0.0, N = 0.0;
				ToCompass(X, Y, E, N);
				E -= T.E;
				N -= T.N;
				const double U = E * std::sin(T.Axis) + N * std::cos(T.Axis), V = E * std::cos(T.Axis) - N * std::sin(T.Axis);
				Best = std::min(Best, std::hypot(U / T.Along, V / T.Across));
			}
			return Best;
		}

		void LayVillages(double BX0, double BY0, double BX1, double BY1)
		{
			// Villages strung on the main roads out of the town, every 3 to 4.5 km (the art director, 20 November 19:31Z:
			// small dark knots of roofs and bare trees every 2 to 4 km, strung on the roads).
			{
				int Line = -1;
				double Next = 0.0;
				for (const Seg& S : L.Segs)
				{
					if (S.Kind != LineKind::MainRoad)
					{
						continue;
					}
					if (S.Line != Line)
					{
						Line = S.Line;
						Next = S.Along + 1500.0 + 2000.0 * Unit(Hash(S.Line, 0, Seed ^ 0x53ull), 1);
					}
					while (Next <= S.Along + S.Len)
					{
						const double At = (Next - S.Along) / S.Len;
						const uint64_t H = Hash(S.Line, static_cast<int64_t>(Next), Seed ^ 0x54ull);
						const double DX = (S.BX - S.AX) / S.Len, DY = (S.BY - S.AY) / S.Len;
						const double Off = (Unit(H, 5) - 0.5) * 160.0;
						Village V;
						V.X = S.AX + (S.BX - S.AX) * At - DY * Off;
						V.Y = S.AY + (S.BY - S.AY) * At + DX * Off;
						V.R = 230.0 + 280.0 * std::pow(Unit(H, 4), 1.5);
						V.H = H;
						V.AxisX = DX;
						V.AxisY = DY;
						V.Long = 1.6 + 1.0 * Unit(H, 7);
						Next += 3000.0 + 1500.0 * Unit(H, 6);
						if (V.X < BX0 || V.X > BX1 || V.Y < BY0 || V.Y > BY1 || OnWater(V.X, V.Y, V.R + 150.0) || TownReach(V.X, V.Y) < 1.2)
						{
							continue;
						}
						Villages.push_back(V);
					}
				}
			}
			const size_t OnRoads = Villages.size();
			const int I0 = static_cast<int>(std::floor(BX0 / VillageCell)), I1 = static_cast<int>(std::floor(BX1 / VillageCell));
			const int J0 = static_cast<int>(std::floor(BY0 / VillageCell)), J1 = static_cast<int>(std::floor(BY1 / VillageCell));
			for (int J = J0; J <= J1; ++J)
			{
				for (int I = I0; I <= I1; ++I)
				{
					const uint64_t H = Hash(I, J, Seed ^ 0x51ull);
					if (Unit(H, 1) > VillageChance)
					{
						continue;
					}
					Village V;
					V.X = (I + 0.2 + 0.6 * Unit(H, 2)) * VillageCell;
					V.Y = (J + 0.2 + 0.6 * Unit(H, 3)) * VillageCell;
					V.R = 170.0 + 300.0 * std::pow(Unit(H, 4), 2.0);
					V.H = H;
					V.AxisX = std::cos(Pi * Unit(H, 5));
					V.AxisY = std::sin(Pi * Unit(H, 5));
					V.Long = 1.3 + 1.0 * Unit(H, 6);
					if (OnWater(V.X, V.Y, V.R + 250.0) || TownReach(V.X, V.Y) < 1.15)
					{
						continue;
					}
					bool bCrowded = false;
					for (size_t K = 0; K < OnRoads && !bCrowded; ++K)
					{
						bCrowded = std::hypot(Villages[K].X - V.X, Villages[K].Y - V.Y) < 1700.0;
					}
					if (!bCrowded)
					{
						Villages.push_back(V);
					}
				}
			}
			Count.Villages = static_cast<int>(Villages.size());
			VillageI0 = I0;
			VillageJ0 = J0;
			VillageNI = I1 - I0 + 1;
			VillageNJ = J1 - J0 + 1;
			VillageGrid.assign(static_cast<size_t>(VillageNI) * VillageNJ, {});
			for (int K = 0; K < static_cast<int>(Villages.size()); ++K)
			{
				const int I = static_cast<int>(std::floor(Villages[K].X / VillageCell)) - I0, J = static_cast<int>(std::floor(Villages[K].Y / VillageCell)) - J0;
				if (I >= 0 && J >= 0 && I < VillageNI && J < VillageNJ)
				{
					VillageGrid[static_cast<size_t>(J) * VillageNI + I].push_back(K);
				}
			}
			// Country roads: each village to its two nearest within 5 km, wandering; over a big river only now and then.
			std::vector<std::pair<int, int>> Links;
			for (int A = 0; A < static_cast<int>(Villages.size()); ++A)
			{
				std::pair<double, int> Best[3] = { { Far, -1 }, { Far, -1 }, { Far, -1 } };
				for (int B = 0; B < static_cast<int>(Villages.size()); ++B)
				{
					if (A == B)
					{
						continue;
					}
					const double D = std::hypot(Villages[A].X - Villages[B].X, Villages[A].Y - Villages[B].Y);
					if (D > 5200.0)
					{
						continue;
					}
					for (int K = 0; K < 3; ++K)
					{
						if (D < Best[K].first)
						{
							for (int M = 2; M > K; --M)
							{
								Best[M] = Best[M - 1];
							}
							Best[K] = { D, B };
							break;
						}
					}
				}
				const int Many = Unit(Villages[A].H, 7) < 0.4 ? 3 : 2;
				for (int K = 0; K < Many; ++K)
				{
					if (Best[K].second >= 0)
					{
						Links.push_back({ std::min(A, Best[K].second), std::max(A, Best[K].second) });
					}
				}
			}
			std::sort(Links.begin(), Links.end());
			Links.erase(std::unique(Links.begin(), Links.end()), Links.end());
			for (const auto& Link : Links)
			{
				const Village& A = Villages[Link.first];
				const Village& B = Villages[Link.second];
				const uint64_t H = Hash(Link.first, Link.second, Seed ^ 0x52ull);
				if (L.CrossesRiver(A.X, A.Y, B.X, B.Y, 20.0) && Unit(H, 1) > 0.25)
				{
					continue;
				}
				const std::vector<Pt> Way = Meander({ { A.X, A.Y }, { B.X, B.Y } }, 50.0, 140.0, 1400.0, H);
				L.Add(Way, LineKind::Road, 3.2, 3.2, static_cast<uint8_t>(Bridged | (Unit(H, 2) < 0.45 ? Avenue : 0)));
				++Count.Links;
			}
			// A church in every village.
			for (const Village& V : Villages)
			{
				Spires.push_back({ V.X + (Unit(V.H, 8) - 0.5) * 50.0, V.Y + (Unit(V.H, 9) - 0.5) * 50.0, 38.0 + 22.0 * Unit(V.H, 10), true });
			}
		}

		void IndexSpires(double BX0, double BY0, double BX1, double BY1)
		{
			SpireX0 = BX0;
			SpireY0 = BY0;
			SpireNX = static_cast<int>(std::ceil((BX1 - BX0) / SpireCell)) + 1;
			SpireNY = static_cast<int>(std::ceil((BY1 - BY0) / SpireCell)) + 1;
			SpireGrid.assign(static_cast<size_t>(SpireNX) * SpireNY, {});
			for (int K = 0; K < static_cast<int>(Spires.size()); ++K)
			{
				const Spire& S = Spires[K];
				// (a church's nave reaches 30 m from its tower)
				for (int DJ = -1; DJ <= 1; ++DJ)
				{
					for (int DI = -1; DI <= 1; ++DI)
					{
						const int I = static_cast<int>(std::floor((S.X + DI * 40.0 - SpireX0) / SpireCell));
						const int J = static_cast<int>(std::floor((S.Y + DJ * 40.0 - SpireY0) / SpireCell));
						if (I < 0 || J < 0 || I >= SpireNX || J >= SpireNY)
						{
							continue;
						}
						std::vector<int>& G = SpireGrid[static_cast<size_t>(J) * SpireNX + I];
						if (std::find(G.begin(), G.end(), K) == G.end())
						{
							G.push_back(K);
						}
					}
				}
			}
			Count.Spires = static_cast<int>(Spires.size());
		}

		Impl(const Place& InP, const std::string& PlansJson)
			: P(InP)
		{
			Seed = Mix(P.Seed ^ 0xA1A11Dull);
			const double NL = std::hypot(P.NorthX, P.NorthY);
			NX = NL > 0 ? P.NorthX / NL : 0.0;
			NY = NL > 0 ? P.NorthY / NL : 1.0;
			EX = NY;
			EY = -NX;
			const double SL = std::hypot(P.SunX, P.SunY);
			SunDX = SL > 0 ? P.SunX / SL : 0.0;
			SunDY = SL > 0 ? P.SunY / SL : -1.0;
			TanSun = std::max(0.08, P.SunZ / std::max(1e-6, SL));
			Sea4 = SeasonOf(P.Month);
			Character();

			// Everything is built over the farthest picture's ground, and a little past it.
			const std::vector<Frame> All = Frames(P);
			double BX0 = Far, BY0 = Far, BX1 = -Far, BY1 = -Far;
			for (const Frame& F : All)
			{
				BX0 = std::min(BX0, F.X0);
				BY0 = std::min(BY0, F.Y0);
				BX1 = std::max(BX1, F.X1());
				BY1 = std::max(BY1, F.Y1());
			}
			BX0 -= 2000.0;
			BY0 -= 2000.0;
			BX1 += 2000.0;
			BY1 += 2000.0;
			Shore.Build(P.Land, BX0, BY0, BX1, BY1);
			Count.bCoast = !Shore.bAllLand;

			if (!LoadPlan(PlansJson))
			{
				MakeTown();
			}
			// The board's own great rivers, where the plan has not laid one of its own along them.
			for (const std::vector<Pt>& R : P.Rivers)
			{
				std::vector<Pt> C = R;
				L.Add(Meander(C, 80.0, 450.0, 5200.0, Mix(Seed ^ 0x77ull ^ R.size())), LineKind::River, 120.0, 220.0, 0);
			}
			for (const Town& T : Towns)
			{
				const double Reach = std::max(T.Along, T.Across) * 1.25;
				for (int A = 0; A < 8; ++A)
				{
					const Pt Q = ToBoard(T.E + Reach * std::cos(A * Pi / 4.0), T.N + Reach * std::sin(A * Pi / 4.0));
					TownMinX = std::min(TownMinX, Q.first);
					TownMaxX = std::max(TownMaxX, Q.first);
					TownMinY = std::min(TownMinY, Q.second);
					TownMaxY = std::max(TownMaxY, Q.second);
				}
			}
			// (the lines so far are indexed once for the villages' look at the rivers, then again with the roads)
			L.Index(BX0, BY0, BX1, BY1);
			LayVillages(BX0, BY0, BX1, BY1);
			// The town's own churches are in the plan; its works' chimneys are laid as their super-blocks are met.
			L.Index(BX0, BY0, BX1, BY1);
			Count.Segments = static_cast<int>(L.Segs.size());
			for (Town& Tn : Towns)
			{
				AngleSupers(Tn);
				const Pt Core = ToBoard(Tn.CoreE, Tn.CoreN);
				Near AtCore;
				L.QueryAround(Core.first, Core.second, 3, AtCore);
				if (AtCore.S[0] && AtCore.D[0] < 2500.0)
				{
					Tn.CoreSide = AtCore.Side[0] > 0.0 ? 1 : -1;
				}
			}
			// Bridges: the roads and railways that cross a river.
			{
				std::vector<char> Seen(static_cast<size_t>(L.NextLine), 0);
				for (const Seg& S : L.Segs)
				{
					if (S.Kind == LineKind::River || Seen[static_cast<size_t>(S.Line)])
					{
						continue;
					}
					if (L.CrossesRiver(S.AX, S.AY, S.BX, S.BY, 10.0))
					{
						++Count.Bridges;
						Seen[static_cast<size_t>(S.Line)] = 1;
					}
				}
			}
			IndexSpires(BX0, BY0, BX1, BY1);
		}

		// ---- Each texel ---------------------------------------------------------------------------------------------

		/** The general lie of the fields round a place (radians): it wanders slowly, so neighbours share it. */
		double Lie(double X, double Y) const { return Pi * Unit(Seed, 0x1E) + 0.6 * Fbm(X / 16000.0, Y / 16000.0, Seed ^ 0x1E, 2); }

		/** How far over the forest's mark the ground is at a point (> 0: wood), the same in every picture: a field is
		 * wood or not by its middle (Farmland), so a wood's edges keep to the fields' lines. */
		double WoodLean(double X, double Y) const
		{
			const double F = 0.62 * Fbm(X / 3400.0, Y / 3400.0, Seed ^ 0xF0, 4) + 0.28 * Fbm(X / 900.0, Y / 900.0, Seed ^ 0xF1, 3)
				+ 0.1 * Noise(X / 110.0, Y / 110.0, Seed ^ 0xF4);
			return F - (0.2 * (1.0 - 2.4 * ForestShare) + 0.02);
		}

		/** A tree's height for the shadows it casts: bare crowns in winter cast only a thin one (the art director's
		 * second look, 20:08Z: no dark rims round the woods). */
		double TreeHigh(double Leafy) const { return Lerp(Leafy, 4.0, Sea4.Bare); }

		/** Where a point is in the forest's grid of rides: the ride's nearness (metres) and the stand it is in. */
		struct Stand
		{
			double Ride = 1.0e9;
			uint64_t H = 0;
		};
		Stand StandAt(double X, double Y) const
		{
			// (each 4 km of country its own grid, on the lie there: rides every 400 to 500 m, both ways)
			const Cell R = Voronoi(X / 4000.0, Y / 4000.0, Seed ^ 0xC4, 0.6);
			const double OX = R.SX * 4000.0, OY = R.SY * 4000.0;
			const double RA = Lie(OX, OY) + 0.25 * (Unit(Hash(R.IX, R.IY, Seed ^ 0xC5), 1) - 0.5);
			const double RU = (X - OX) * std::cos(RA) + (Y - OY) * std::sin(RA) + 5000.0, RV = -(X - OX) * std::sin(RA) + (Y - OY) * std::cos(RA) + 5000.0;
			const double A = 410.0, B = 470.0;
			Stand S;
			// (from the nearest ride: the grid's lines and the edge of its 4 km)
			S.Ride = std::min({ A * 0.5 - std::fabs(RU - (std::floor(RU / A) + 0.5) * A), B * 0.5 - std::fabs(RV - (std::floor(RV / B) + 0.5) * B), R.Border * 4000.0 });
			S.H = Hash3(R.IX * 7919 + R.IY, static_cast<int64_t>(std::floor(RU / A)), static_cast<int64_t>(std::floor(RV / B)), Seed ^ 0xC6);
			return S;
		}

		Ink Woods(double X, double Y, double T, bool bPlanted, uint64_t BlockHash) const
		{
			// A forest is one dark mass of bare crowns with darker clumps of pine and spruce in it, some stands more than
			// others (the art director, 20 November 19:31Z: not a two-colour camouflage); a plantation is most often all
			// conifer.
			uint64_t HS = BlockHash;
			double Conifer = 0.0;
			if (bPlanted)
			{
				Conifer = Unit(HS, 31) < 0.75 ? 1.0 : 0.0;
			}
			else
			{
				HS = StandAt(X, Y).H;
				// (some stands mostly pine and spruce, most mostly bare oak and beech: the clumps a few hundred metres across)
				const double Lean = 0.36 * (Unit(HS, 31) - 0.5);
				const double Clump = Fbm(X / 420.0, Y / 420.0, Seed ^ 0xC7, 3) + 0.2 * Shows(30.0, T) * Noise(X / 60.0, Y / 60.0, Seed ^ 0xC8) + Lean;
				Conifer = SmoothStep(0.17, 0.24, Clump);
			}
			const double Mixed = 1.0 - Conifer;
			Ink W = Over(Sea4.Conifer, Bright(Sea4.Broadleaf, 0.9), Mixed);
			W = Vary(W, HS, 0.04, 0.015);
			// crowns: light and dark at 8 and 20 m (a texel much larger sees their average)
			const double Crowns = 0.16 * Shows(8.0, T) * Noise(X / 7.0, Y / 7.0, Seed ^ 0xC1) + 0.1 * Shows(20.0, T) * Noise(X / 19.0, Y / 19.0, Seed ^ 0xC2)
				+ 0.08 * Noise(X / 140.0, Y / 140.0, Seed ^ 0xC3);
			W = Bright(W, 1.0 + Crowns);
			return Tall(W, TreeHigh(18.0 + 4.0 * Mixed));
		}

		/** A farmstead in its grove: Ux, Uy the farm's frame (its long way), D its distance from the farm's middle. */
		Ink Farmstead(const Ink& Under, double DU, double DV, double T, uint64_t H) const
		{
			const double R = std::hypot(DU, DV);
			const double Grove = 34.0 + 22.0 * Unit(H, 41);
			Ink Out = Under;
			// the grove: a broken ring of old trees round the yard
			const double Ring = Grove + 9.0 * Noise(std::atan2(DV, DU) * 2.0, Unit(H, 42) * 10.0, Seed ^ 0xFA);
			if (R < Ring + 6.0)
			{
				const double Gap = Noise(std::atan2(DV, DU) * 1.7 + 3.0, Unit(H, 43) * 9.0, Seed ^ 0xFB);
				const double Trees = Cover(std::fabs(R - (Ring - 6.0)), 9.0, T) * SmoothStep(-0.35, -0.1, Gap);
				Out = Over(Out, Tall(Over(Bright(Sea4.Broadleaf, 0.95), Sea4.Conifer, 0.2), TreeHigh(15.0)), Trees);
			}
			// the yard
			Out = Over(Out, Rgb(112, 104, 92), Cover(R, Grove - 14.0, T) * 0.85);
			// the house and the barn: long roofs, red tile or thatch-brown
			const Ink Roof = Unit(H, 44) < 0.7 ? Rgb(126, 70, 56) : Rgb(104, 86, 70);
			const double House = std::max(std::fabs(DU) - 16.0, std::fabs(DV + 6.0) - 7.0);
			const double Barn = std::max(std::fabs(DU + 4.0) - 11.0, std::fabs(DV - 13.0) - 6.0);
			const double Side = DV + 6.0 > 0 ? 1.0 : -1.0;
			Out = Over(Out, Tall(Bright(Roof, 1.0 + 0.12 * Side), 9.0), Cover(House, 0.0, T) * 0.5 + 0.5 * Clamp01(-House / std::max(1.0, T * 0.5)));
			Out = Over(Out, Tall(Bright(Rgb(98, 92, 88), DV - 13.0 > 0 ? 1.1 : 0.9), 8.0), Clamp01(0.5 - Barn / std::max(1.0, T)));
			return Out;
		}

		Ink Farmland(double X, double Y, double T, double RiverNear, const Near& Nr) const
		{
			// (the ground pulled about by slow noise, so the blocks' borders bend)
			const double WX = X + 150.0 * Fbm(X / 1100.0, Y / 1100.0, Seed ^ 0xA0, 3);
			const double WY = Y + 150.0 * Fbm(X / 1100.0, Y / 1100.0, Seed ^ 0xA1, 3);
			// (one size everywhere: a size that wandered would move each block's seed, in metres, from texel to texel)
			const double Size = BlockSize;
			const Cell B = Voronoi(WX / Size, WY / Size, Seed ^ 0xA3, 0.85);
			const uint64_t HB = Hash(B.IX, B.IY, Seed ^ 0xA4);
			const double Border = B.Border * Size;
			const double SX = B.SX * Size, SY = B.SY * Size;
			// Meadows: by a river the land is wet grass, its long narrow strips square to the river and parted by ditches
			// (the Weser marsh); none in a block out in the dry country, so no field block reads as a lone polygon.
			const double Wet = Clamp01(1.0 - RiverNear);
			const bool bMeadow = bMarsh ? Unit(HB, 3) < 0.7 : Unit(HB, 3) < 0.9 * Wet;

			// Woods: a forest (none in a meadow), or plantations of a few strips' width in some blocks.
			const bool bPlantBlock = !bMeadow && Unit(HB, 4) < PlantShare * 1.5 * (0.6 + 0.8 * (0.5 + 0.5 * Fbm(X / 7000.0, Y / 7000.0, Seed ^ 0xA5, 2)));

			// The block's strips: straight, on the region's lie or square to it (a little off either), their widths and
			// lengths; a meadow's strips square to the river by its seed. (The block's edges bend with the warped
			// ground; the strips inside do not.)
			double Angle = Lie(SX, SY) + (Unit(HB, 5) < 0.4 ? Pi * 0.5 : 0.0) + (Unit(HB, 11) - 0.5) * 0.3;
			if (bMeadow && Nr.S[0])
			{
				Near AtSeed;
				L.QueryAround(SX, SY, 2, AtSeed);
				if (AtSeed.S[0])
				{
					Angle = std::atan2(AtSeed.DirY[0], AtSeed.DirX[0]) + (Unit(HB, 11) - 0.5) * 0.12;
				}
			}
			const double CA = std::cos(Angle), SA = std::sin(Angle);
			const double U = (X - SX) * CA + (Y - SY) * SA, V = -(X - SX) * SA + (Y - SY) * CA;
			const double Wd = bMeadow ? Lerp(22.0, 45.0, Unit(HB, 6)) : Lerp(StripMin, StripMax, std::pow(Unit(HB, 6), 1.3));
			const double K0 = std::floor(U / Wd);
			// (each strip's edge moved a little, so no two are quite alike)
			auto EdgeAt = [&](double K) { return (K + 0.35 * (Unit(Hash3(B.IX, B.IY, static_cast<int64_t>(K), Seed ^ 0xA6)) - 0.5)) * Wd; };
			double K = K0;
			double Lo = EdgeAt(K), Hi = EdgeAt(K + 1.0);
			if (U < Lo)
			{
				K -= 1.0;
				Hi = Lo;
				Lo = EdgeAt(K);
			}
			else if (U >= Hi)
			{
				K += 1.0;
				Lo = Hi;
				Hi = EdgeAt(K + 1.0);
			}
			// (one edge in four is not there: two strips are one field)
			const bool bLoGone = !bMeadow && Unit(Hash3(B.IX, B.IY, static_cast<int64_t>(K), Seed ^ 0xA7)) < 0.25;
			const bool bHiGone = !bMeadow && Unit(Hash3(B.IX, B.IY, static_cast<int64_t>(K + 1.0), Seed ^ 0xA7)) < 0.25;
			const int64_t Strip = static_cast<int64_t>(bLoGone ? K - 1.0 : K);
			const double Len = bMeadow ? Lerp(400.0, 1000.0, Unit(Hash3(B.IX, B.IY, Strip, Seed ^ 0xA8))) : Lerp(160.0, 650.0, Unit(Hash3(B.IX, B.IY, Strip, Seed ^ 0xA8)));
			const double VOff = Unit(HB, 7) < 0.5 ? 0.0 : Unit(Hash3(B.IX, B.IY, Strip, Seed ^ 0xA9)) * Len;
			const double C = std::floor((V + VOff) / Len);
			const double FV = (V + VOff) / Len - C;
			const uint64_t HF = Hash3(Hash(B.IX, B.IY, 3), Strip, static_cast<int64_t>(C), Seed ^ 0xAA);
			// (a plantation: a run of some five strips, the whole of their length)
			const double Run = std::floor((K + 40.0) / 5.0);
			// (and some 250 to 700 m of their length, so a plantation is a rectangle in the block and not the block's polygon)
			const uint64_t HRun = Hash3(B.IX, B.IY, static_cast<int64_t>(Run), Seed ^ 0xB0);
			const double RunLong = 125.0 + 225.0 * Unit(HRun, 2), RunMid = (Unit(HRun, 3) - 0.5) * Size * 0.5;
			const bool bPlanted = bPlantBlock && Unit(HRun, 1) < 0.45 && std::fabs(V - RunMid) < RunLong + T;
			const double RunLo = EdgeAt(Run * 5.0 - 40.0), RunHi = EdgeAt(Run * 5.0 - 35.0);

			// The forest keeps to the fields' lines: a field is wood where the forest's noise at its middle says so, so
			// a wood's edge runs straight along the strips and square across their ends, and stops short of the block's
			// border (a track or ditch along it). An edge is softened a texel wide only where the field across it differs.
			auto FieldWooded = [&](double Kq, double Vq) -> bool
			{
				if (bMeadow)
				{
					return false;
				}
				const bool bGone = Unit(Hash3(B.IX, B.IY, static_cast<int64_t>(Kq), Seed ^ 0xA7)) < 0.25;
				const int64_t Sq = static_cast<int64_t>(bGone ? Kq - 1.0 : Kq);
				const double LenQ = Lerp(160.0, 650.0, Unit(Hash3(B.IX, B.IY, Sq, Seed ^ 0xA8)));
				const double VOffQ = Unit(HB, 7) < 0.5 ? 0.0 : Unit(Hash3(B.IX, B.IY, Sq, Seed ^ 0xA9)) * LenQ;
				const double MU = 0.5 * (EdgeAt(Kq) + EdgeAt(Kq + 1.0)), MV = (std::floor((Vq + VOffQ) / LenQ) + 0.5) * LenQ - VOffQ;
				return WoodLean(SX + MU * CA - MV * SA, SY + MU * SA + MV * CA) > 0.0;
			};
			const bool bWooded = FieldWooded(K, V);
			double Forest = bWooded ? 1.0 : 0.0;
			{
				const double TT = std::max(1.0, T);
				auto EdgeWith = [&](double Dist, bool bOther)
				{
					if (Dist < TT && bOther != bWooded)
					{
						Forest = bWooded ? std::min(Forest, Clamp01(Dist / TT + 0.5)) : std::max(Forest, Clamp01(0.5 - Dist / TT));
					}
				};
				if (!bMeadow)
				{
					const double DVLo = FV * Len, DVHi = (1.0 - FV) * Len;
					if (U - Lo < TT) EdgeWith(U - Lo, FieldWooded(K - 1.0, V));
					if (Hi - U < TT) EdgeWith(Hi - U, FieldWooded(K + 1.0, V));
					if (DVLo < TT) EdgeWith(DVLo, FieldWooded(K, V - DVLo - 1.0));
					if (DVHi < TT) EdgeWith(DVHi, FieldWooded(K, V + DVHi + 1.0));
				}
				if (bWooded)
				{
					Forest = std::min(Forest, 1.0 - LineCover(Border, 3.0, T));
				}
			}

			// The field's crop.
			Ink F;
			if (bMeadow)
			{
				F = Vary(Over(Sea4.WetPasture, Sea4.Pasture, 0.4 * Unit(HF, 1)), HF, 0.08, 0.03);
			}
			else
			{
				const double Pick = Unit(HF, 2);
				double Acc = 0.0;
				int Which = 6;
				for (int Q = 0; Q < 7; ++Q)
				{
					Acc += Sea4.Share[Q];
					if (Pick < Acc)
					{
						Which = Q;
						break;
					}
				}
				const Ink Crops[7] = { Sea4.Pasture, Sea4.Cereal, Sea4.Plough, Sea4.Stubble, Sea4.Fallow, Sea4.Bright, Bright(Sea4.Cereal, 0.82) };
				F = Vary(Crops[Which], HF, 0.07, 0.04);
				// ploughed earth: the furrows' long lines, faint, along the strip
				if (Which == 2)
				{
					F = Bright(F, 1.0 + 0.05 * Shows(5.0, T) * std::sin(U * 2.0 * Pi / 4.5));
				}
			}
			// within a field: slow patches of wetter and drier ground
			F = Bright(F, 1.0 + 0.06 * Fbm(X / 120.0, Y / 120.0, Seed ^ 0xAB, 2) + 0.04 * Shows(30.0, T) * Noise(X / 26.0, Y / 26.0, Seed ^ 0xAC));

			// The strips' edges: a balk of rough grass, or a ditch in a meadow.
			const double DU = std::min(bLoGone ? Far : U - Lo, bHiGone ? Far : Hi - U);
			const double DV = std::min(FV, 1.0 - FV) * Len;
			// (a meadow's ditches hold water that catches the sky's light)
			// (a meadow stays dark, wet grass out to the haze, only the thin lines of its ditches catching the light: the
			// art director's second look, 20:08Z)
			const Ink Balk = bMeadow ? Rgb(116, 126, 132) : Bright(Sea4.Grass, 0.95);
			F = Over(F, Balk, bMeadow ? 0.6 * std::max(LineCover(DU, 1.2, T), LineCover(DV, 1.0, T)) : 0.75 * std::max(Cover(DU, 1.2, T), Cover(DV, 1.0, T)));

			// Woods over the fields.
			if (bPlanted || Forest > 0.0)
			{
				Ink W = Woods(X, Y, T, bPlanted, HB);
				double Amount = bPlanted ? Clamp01(std::min({ Border - 5.0, U - RunLo, RunHi - U, RunLong - std::fabs(V - RunMid) }) / std::max(1.0, T) + 0.5) : Forest;
				if (bPlanted && Forest > Amount)
				{
					Amount = Forest;
				}
				if (!bPlanted)
				{
					// rides cut straight through a forest, both ways, every 400 m or so: a pale track in a cut of grass
					const double Ride = Cover(StandAt(X, Y).Ride, 3.5, T);
					W = Over(W, Rgb(104, 100, 86), Ride * 0.85);
					W.H = static_cast<float>(Lerp(W.H, 0.0, Ride));
				}
				F = Over(F, W, Amount);
			}

			// A farmstead in some blocks, at its seed, on its own lie.
			if (!bMeadow && !bPlanted && Forest < 0.5 && Unit(HB, 8) < 0.42)
			{
				const double DFU = U - (Unit(HB, 9) - 0.5) * 60.0, DFV = V - (Unit(HB, 10) - 0.5) * 120.0;
				if (DFU * DFU + DFV * DFV < 75.0 * 75.0)
				{
					F = Farmstead(F, DFU, DFV, T, HB);
				}
			}

			// The block's border: a hedgerow on its bank, or a ditch, or a farm track.
			const uint64_t HE = Hash(std::min(Hash(B.IX, B.IY, 1), Hash(B.OX, B.OY, 1)) & 0xFFFFFFFF, std::max(Hash(B.IX, B.IY, 1), Hash(B.OX, B.OY, 1)) & 0xFFFFFFFF, Seed ^ 0xAD);
			const double Kind = Unit(HE, 1);
			const double Hedges = bMeadow ? 0.1 : HedgeShare * (0.6 + 0.8 * (0.5 + 0.5 * Fbm(X / 6000.0, Y / 6000.0, Seed ^ 0xAE, 2)));
			if (!bMeadow && Unit(HE, 3) < 0.4)
			{
				// (no hedge, ditch or track here: the two blocks' strips just meet, so the blocks' polygons don't make a net)
			}
			else if (Kind < Hedges)
			{
				// (a hedgerow's trees in clumps along it, gaps here and there)
				const double Along = (X * -SA + Y * CA) / 9.0;
				const double Clumps = 0.75 + 0.25 * Noise(Along, Unit(HE, 2) * 50.0, Seed ^ 0xAF);
				F = Over(F, Tall(Vary(Sea4.Hedge, HE, 0.08, 0.02), Lerp(7.0 + 4.0 * Clumps, 5.0, 0.5 * Sea4.Bare)), Cover(Border, 3.2 * Clumps, T) * 0.92);
			}
			else if (Kind < Hedges + 0.35)
			{
				F = Over(F, bMeadow ? Rgb(58, 72, 70) : Rgb(70, 76, 64), Cover(Border, bMeadow ? 2.0 : 1.4, T));
			}
			else
			{
				F = Over(F, Rgb(140, 130, 108), Cover(Border, 2.2, T) * 0.8);
			}
			return F;
		}

		/** A village: houses along its roads, orchards and yards behind them. */
		Ink VillageAt(const Ink& Under, const Village& Vi, double X, double Y, double T, const Near& Nr) const
		{
			// A street village (the art director's second look, 20:08Z: no round knots): its farms in a line along the
			// road on both sides, one plot deep, each farm's great hall-house gable to the street with its yard round it
			// and a grove of bare trees behind; thinning out toward its ends, the church in the middle.
			const double VU = (X - Vi.X) * Vi.AxisX + (Y - Vi.Y) * Vi.AxisY, VV = -(X - Vi.X) * Vi.AxisY + (Y - Vi.Y) * Vi.AxisX;
			const double HalfLong = Vi.R * std::sqrt(Vi.Long) * 1.6;
			const double Ends = std::fabs(VU) / HalfLong + 0.2 * Fbm(X / 300.0, Y / 300.0, Vi.H, 2);
			if (Ends > 1.05)
			{
				return Under;
			}
			// the road it lies along: the nearest road, else its own long way (a lane)
			double Off = std::fabs(VV), Along = VU, DirX = Vi.AxisX, DirY = Vi.AxisY;
			bool bLeft = VV > 0.0;
			uint64_t Line = Vi.H;
			if (Nr.S[1] && Nr.D[1] < 170.0)
			{
				Off = std::max(0.0, Nr.D[1] - Nr.Half[1]);
				Along = Nr.Along[1];
				DirX = Nr.DirX[1];
				DirY = Nr.DirY[1];
				bLeft = Nr.Side[1] > 0.0;
				Line = static_cast<uint64_t>(Nr.S[1]->Line) * 7919u + 13u;
			}
			const double Depth = 70.0 + 40.0 * Unit(Vi.H, 21);
			if (Off > Depth + 12.0)
			{
				return Under;
			}
			const double Dense = 1.0 - SmoothStep(0.72, 1.02, Ends);
			const double TT = std::max(1.0, T);
			// the plots along the road, 40 to 75 m of frontage each; a gap here and there, more toward the ends
			const double Lot = 40.0 + 35.0 * Unit(Vi.H, 22);
			const double AtL = Along / Lot;
			const int64_t Index = static_cast<int64_t>(std::floor(AtL));
			const uint64_t HL = Hash3(static_cast<int64_t>(Line & 0xFFFFFFF), Index, bLeft ? 1 : 0, Seed ^ 0xB3);
			const bool bFarm = Unit(HL, 1) < 0.88 * Dense;
			const double In = (AtL - static_cast<double>(Index) - 0.5) * Lot; // along the road from the plot's middle
			Ink Out = Under;
			if (bFarm)
			{
				// the garden and paddock behind, the whole plot: winter grass, its back edge ragged against the fields
				const double Back = Depth + 14.0 * Noise(X / 40.0, Y / 40.0, HL ^ 2);
				const double Plot = std::max(std::fabs(In) - Lot * 0.5 + 2.0, Off - Back);
				const double InPlot = Clamp01(0.5 - Plot / TT);
				Out = Over(Out, Vary(Over(Sea4.Pasture, Sea4.Garden, 0.35), HL, 0.07, 0.03), InPlot * 0.5);
				// the hall-house: gable to the street, 13 to 16 m wide and 26 to 36 m deep, set back from the road
				const double HW = 6.5 + 1.5 * Unit(HL, 2), HD = 13.0 + 5.0 * Unit(HL, 3);
				const double HS = 5.0 + 9.0 * Unit(HL, 4) + HD;
				const double Shift = (Unit(HL, 5) - 0.5) * std::max(0.0, Lot - 2.0 * HW - 10.0);
				const double HIn = In - Shift;
				// the yard round the house and barn: trodden earth
				const double FromHouse = std::hypot(std::max(0.0, std::fabs(HIn) - HW), std::max(0.0, std::fabs(Off - HS) - HD));
				Out = Over(Out, Vary(Rgb(104, 96, 84), HL, 0.06, 0.03), Cover(FromHouse, 9.0, T) * InPlot * 0.85);
				// the farm's grove: old bare oaks in clumps round the house and its yard, and fruit trees in the garden
				const double Clumps = Noise(X / 16.0, Y / 16.0, HL ^ 3) + 0.3 * Noise(X / 45.0, Y / 45.0, HL ^ 8);
				const double Ring = SmoothStep(8.0, 14.0, FromHouse) * (1.0 - SmoothStep(26.0, 40.0, FromHouse));
				const double Orchard = (Off > HS + HD + 8.0 ? 0.55 : 0.0) * SmoothStep(0.1, 0.35, Noise(X / 9.0, Y / 9.0, HL ^ 10));
				const double Grove = std::max(Ring * SmoothStep(-0.25, 0.1, Clumps), Orchard * Shows(9.0, T) + Orchard * 0.5 * (1.0 - Shows(9.0, T))) * InPlot;
				const Ink Crowns = Bright(Over(Sea4.Broadleaf, Sea4.Conifer, 0.1), 0.95 + 0.12 * Shows(8.0, T) * Noise(X / 7.0, Y / 7.0, HL ^ 4));
				Out = Over(Out, Tall(Crowns, TreeHigh(14.0)), Grove * 0.85);
				const double House = std::max(std::fabs(HIn) - HW, std::fabs(Off - HS) - HD);
				const double Pick = Unit(HL, 7);
				const Ink Tile = Pick < 0.55 ? Rgb(102, 76, 64) : Pick < 0.85 ? Rgb(94, 84, 72) : Rgb(86, 84, 86);
				// (the ridge runs back from the street: the slopes face either way along it, the one toward the sun lighter)
				const double Toward = (HIn > 0.0 ? 1.0 : -1.0) * (DirX * SunDX + DirY * SunDY);
				Out = Over(Out, Tall(Bright(Vary(Tile, HL, 0.1, 0.03), 1.0 + 0.25 * Toward * Shows(6.0, T)), 10.0), Clamp01(0.5 - House / TT));
				// a barn or byre in some yards, along the street behind the house
				if (Unit(HL, 8) < 0.6)
				{
					const double BIn = HIn + (HIn > 0.0 ? -1.0 : 1.0) * (HW + 10.0), BOff = HS + HD - 4.0;
					const double Barn = std::max(std::fabs(BIn) - 9.0, std::fabs(Off - BOff) - 5.5);
					const double Lit = 1.0 + 0.2 * Shows(6.0, T) * ((Off > BOff) == bLeft ? 1.0 : -1.0) * (-DirY * SunDX + DirX * SunDY);
					Out = Over(Out, Tall(Bright(Vary(Rgb(92, 82, 74), HL ^ 9, 0.1, 0.03), Lit), 7.0), Clamp01(0.5 - Barn / TT));
				}
			}
			// (a texel larger than a farm sees the street's mean: roofs, yards and groves in a band along the road)
			const double Many = 1.0 - Shows(30.0, T);
			if (Many > 0.0)
			{
				const double Band = Clamp01((Depth - Off) / TT + 0.5) * Dense * 0.85;
				Out = Over(Out, Tall(Rgb(86, 78, 72), 6.0), Many * Band);
			}
			return Out;
		}

		// ---- The town's grounds

		/** Super-blocks of the town: cells some 400 m across, each with its own street grid turned its own way. */
		struct Super
		{
			Cell C;
			double SE = 0.0, SN = 0.0; // the seed, compass metres
			double Angle = 0.0;
			uint64_t H = 0;
		};
		Super SuperAt(const Town& Tn, double E, double N) const
		{
			const double Size = 640.0;
			const double WE = E + 45.0 * Noise(E / 420.0, N / 420.0, Seed ^ 0xD0), WN = N + 45.0 * Noise(E / 420.0, N / 420.0, Seed ^ 0xD1);
			Super S;
			S.C = Voronoi(WE / Size, WN / Size, Seed ^ 0xD2, 0.7);
			S.SE = S.C.SX * Size;
			S.SN = S.C.SY * Size;
			S.C.Border *= Size;
			S.H = Hash(S.C.IX, S.C.IY, Seed ^ 0xD3);
			// (the streets of a district, some 1.7 km of the town laid out at one time, run out from the middle of the
			// town or round it; each super-block of it a little off that)
			// town or round it; each super-block of it a little off that). Near the river, a main road or a railway the
			// streets run along it or square to it, so the grids bend with the river and the old roads out of the middle.
			const int I = static_cast<int>(S.C.IX) - Tn.SuperI0, J = static_cast<int>(S.C.IY) - Tn.SuperJ0;
			const float Along = I >= 0 && J >= 0 && I < Tn.SuperNI && J < Tn.SuperNJ ? Tn.SuperAngle[static_cast<size_t>(J) * Tn.SuperNI + I] : std::nanf("");
			if (!std::isnan(Along))
			{
				S.Angle = Along + (Unit(S.H, 12) < 0.3 ? Pi * 0.5 : 0.0) + (Unit(S.H, 2) - 0.5) * 0.06;
				return S;
			}
			const Cell D = Voronoi(E / 1700.0, N / 1700.0, Seed ^ 0xD4, 0.8);
			const uint64_t HD = Hash(D.IX, D.IY, Seed ^ 0xD8);
			const double Out = std::atan2(D.SY * 1700.0 - Tn.CoreN, D.SX * 1700.0 - Tn.CoreE);
			S.Angle = Out + (Unit(HD, 1) < 0.5 ? 0.0 : Pi * 0.5) + (Unit(HD, 2) - 0.5) * 0.3 + (Unit(S.H, 2) - 0.5) * 0.1;
			return S;
		}

		/** The super-blocks' turn to the lines near them (Town::SuperAngle), once the lines are laid and indexed. */
		void AngleSupers(Town& Tn) const
		{
			const double Size = 640.0;
			const double Reach = 1.3 * std::max(Tn.Along, Tn.Across);
			Tn.SuperI0 = static_cast<int>(std::floor((Tn.E - Reach) / Size)) - 1;
			Tn.SuperJ0 = static_cast<int>(std::floor((Tn.N - Reach) / Size)) - 1;
			Tn.SuperNI = static_cast<int>(std::ceil(2.0 * Reach / Size)) + 3;
			Tn.SuperNJ = Tn.SuperNI;
			Tn.SuperAngle.assign(static_cast<size_t>(Tn.SuperNI) * Tn.SuperNJ, std::nanf(""));
			for (int J = 0; J < Tn.SuperNJ; ++J)
			{
				for (int I = 0; I < Tn.SuperNI; ++I)
				{
					// (the seed as Voronoi places it)
					const int64_t CX = Tn.SuperI0 + I, CY = Tn.SuperJ0 + J;
					const uint64_t H = Hash(CX, CY, Seed ^ 0xD2);
					const double SE = (CX + 0.5 + 0.7 * (Unit(H, 1) - 0.5)) * Size, SN = (CY + 0.5 + 0.7 * (Unit(H, 2) - 0.5)) * Size;
					const Pt B = ToBoard(SE, SN);
					Near Nr;
					L.QueryAround(B.first, B.second, 2, Nr);
					double Best = 750.0;
					int Which = -1;
					for (int C = 0; C < 3; ++C)
					{
						if (!Nr.S[C] || (C == 1 && Nr.S[C]->Kind == LineKind::Road))
						{
							continue;
						}
						const double D = (Nr.D[C] - Nr.Half[C]) * (C == 2 ? 1.25 : 1.0);
						if (D < Best)
						{
							Best = D;
							Which = C;
						}
					}
					if (Which >= 0)
					{
						const double DE = Nr.DirX[Which] * EX + Nr.DirY[Which] * EY, DN = Nr.DirX[Which] * NX + Nr.DirY[Which] * NY;
						Tn.SuperAngle[static_cast<size_t>(J) * Tn.SuperNI + I] = static_cast<float>(std::atan2(DN, DE));
					}
				}
			}
		}

		/** A super-block's own tone: its age, its roofs' stuff, its soot. */
		double DistrictTone(const Super& S) const { return 0.84 + 0.24 * Unit(S.H, 13); }

		/** The share of a district's ground its buildings shade (High tall, Gap apart) at this sun: in the average a texel
		 * larger than the blocks sees, so the far town is as dark as the near one with its shadows cast. */
		double ShadeShare(double High, double Gap) const { return std::min(0.7, High / TanSun / std::max(1.0, Gap)); }

		/** A roof Box (signed distance, metres: negative inside) with its ridge along U: lit on the side toward the sun. */
		Ink Roof(const Ink& Under, double Box, double AcrossRidge, double NormE, double NormN, const Ink& Tile, double High, double T) const
		{
			// (the roof's slope facing NormE/N on AcrossRidge > 0, the other way below; lighter toward the sun)
			const double BX = NormE * EX + NormN * NX, BY = NormE * EY + NormN * NY; // compass to board
			const double Toward = (AcrossRidge > 0 ? 1.0 : -1.0) * (BX * SunDX + BY * SunDY);
			const double Lit = Shows(6.0, T) > 0 ? 1.0 + 0.26 * Toward * Shows(6.0, T) : 1.0;
			return Over(Under, Tall(Bright(Tile, Lit), High), Clamp01(0.5 - Box / std::max(1.0, T)));
		}

		Ink TileOf(uint64_t H, double Grey) const
		{
			// brick-red and dark-red tile most, some brown, grey slate (more of it in the old town and the works): seen
			// through the haze from bomber height brick goes grey-brown (the art director, 20 November 19:31Z: no mauve)
			const double Pick = Unit(H, 1);
			Ink Tile = Pick < Grey + 0.06 ? Rgb(82, 84, 88) : Pick < Grey + 0.4 ? Rgb(104, 82, 70) : Pick < Grey + 0.68 ? Rgb(90, 72, 64) : Pick < Grey + 0.88 ? Rgb(96, 86, 76) : Rgb(108, 96, 84);
			return Vary(Tile, H, 0.14, 0.03);
		}

		Ink Street(double T) const
		{
			// (a side street between tall houses: setts, mostly in their shade)
			(void)T;
			return Rgb(88, 86, 83);
		}

		/** Courtyard blocks: a grid of streets, buildings round each block's edge, yards and gardens inside. */
		Ink Blocks(const Super& S, double E, double N, double T, double Deep, double High, double SpanA, double SpanB, double StreetHalf) const
		{
			const double CA = std::cos(S.Angle), SA = std::sin(S.Angle);
			const double U = (E - S.SE) * CA + (N - S.SN) * SA, V = -(E - S.SE) * SA + (N - S.SN) * CA;
			const double A = SpanA * (0.85 + 0.3 * Unit(S.H, 3)), Bb = SpanB * (0.85 + 0.3 * Unit(S.H, 4));
			const double OU = U + A * Unit(S.H, 5), OV = V + Bb * Unit(S.H, 6);
			const double IU = std::floor(OU / A), IV = std::floor(OV / Bb);
			const double FU = OU - IU * A, FV = OV - IV * Bb; // inside the block, from its corner
			const double ToStreetU = std::min(FU, A - FU), ToStreetV = std::min(FV, Bb - FV);
			const double Edge = std::min(ToStreetU, ToStreetV) - StreetHalf; // metres in from the street's edge
			const uint64_t HBk = Hash3(S.C.IX * 977 + S.C.IY, static_cast<int64_t>(IU), static_cast<int64_t>(IV), Seed ^ 0xD5);
			// (each block its own tone and each super-block its own: the town's grain coarser than the fields')
			const double Tone = (0.76 + 0.42 * Unit(HBk, 7)) * DistrictTone(S);
			// the yard inside: gardens, sheds, paving
			const double YardPick = Unit(HBk, 1);
			Ink Yard = YardPick < 0.45 ? Bright(Sea4.Garden, 0.85) : YardPick < 0.8 ? Rgb(80, 78, 72) : Rgb(72, 72, 70);
			Yard = Over(Yard, Tall(Sea4.Hedge, 10.0), 0.4 * SmoothStep(0.1, 0.4, Noise(E / 18.0, N / 18.0, HBk)) * (YardPick < 0.45 ? 1.0 : 0.3));
			Yard = Bright(Yard, Tone);
			Ink Out = Yard;
			// the buildings round the edge: each its own stretch of the frontage and its own roof
			const bool bAlongU = ToStreetV < ToStreetU; // (fronting a street that runs along U)
			const double Front = bAlongU ? FU : FV;
			const double Piece = 14.0 + 10.0 * Unit(HBk, 2);
			const int64_t Which = static_cast<int64_t>(std::floor(Front / Piece));
			const uint64_t HBd = Hash3(static_cast<int64_t>(HBk & 0xFFFFFF), Which, bAlongU ? 1 : 2, Seed ^ 0xD6);
			const double Depth = Deep * (0.85 + 0.3 * Unit(HBd, 3));
			const double Box = std::max(-Edge, Edge - Depth); // inside the ring of buildings when negative
			const double Across = Edge - Depth * 0.5;
			// (the roof slopes toward the street and toward the yard)
			const double SideSign = bAlongU ? (FV < Bb * 0.5 ? -1.0 : 1.0) : (FU < A * 0.5 ? -1.0 : 1.0);
			const double NormE = bAlongU ? -SA * SideSign : CA * SideSign, NormN = bAlongU ? CA * SideSign : SA * SideSign;
			if (Unit(HBd, 4) < 0.94)
			{
				Out = Roof(Out, Box, -Across, NormE, NormN, Bright(TileOf(HBd, 0.18), Tone), High * (0.85 + 0.3 * Unit(HBd, 5)), T);
			}
			// the streets
			Out = Over(Out, Street(T), Cover(std::min(ToStreetU, ToStreetV), StreetHalf, T));
			// a texel that holds many blocks sees their average: roofs, yards, streets and the roofs' shade
			const double Many = 1.0 - Shows(std::min(A, Bb) * 0.5, T);
			if (Many > 0.0)
			{
				const double Built = Clamp01(2.0 * Deep / std::min(A, Bb));
				Ink Mean = Over(Over(Yard, Rgb(98, 80, 70), Built), Street(T), 2.0 * StreetHalf / std::min(A, Bb));
				Mean = InShade(Bright(Mean, Tone), ShadeShare(High, std::min(A, Bb) * 0.5));
				Out = Over(Out, Tall(Mean, High * 0.5), Many);
			}
			return Out;
		}

		/** Rows of terraced houses or villas along parallel streets: Lot the plot along the street, House its size. */
		Ink Rows(const Super& S, double E, double N, double T, double RowGap, double CrossGap, double StreetHalf, double Lot, double HouseW, double HouseD, double Set, double High, bool bGardensTreed) const
		{
			const double CA = std::cos(S.Angle), SA = std::sin(S.Angle);
			const double U = (E - S.SE) * CA + (N - S.SN) * SA, V = -(E - S.SE) * SA + (N - S.SN) * CA;
			const double Gap = RowGap * (0.85 + 0.3 * Unit(S.H, 3));
			const double Cross = CrossGap * (0.8 + 0.4 * Unit(S.H, 4));
			const double OV = V + Gap * Unit(S.H, 5), OU = U + Cross * Unit(S.H, 6);
			const double IV = std::floor(OV / Gap);
			const double FV = OV - IV * Gap; // across the rows: the street at 0 and at Gap
			const double ToStreet = std::min(FV, Gap - FV);
			const double FU = OU - std::floor(OU / Cross) * Cross;
			const double ToCross = std::min(FU, Cross - FU);
			const int Side = FV < Gap * 0.5 ? 0 : 1;
			const double Back = ToStreet - StreetHalf; // metres from the street's edge
			const int64_t LotIndex = static_cast<int64_t>(std::floor(OU / Lot));
			const uint64_t HL = Hash3(S.C.IX * 977 + S.C.IY + static_cast<int64_t>(IV) * 31 + Side, LotIndex, 5, Seed ^ 0xD7);
			// (each row its own tone, built at its own time, and each super-block its own)
			const double Tone = (0.78 + 0.4 * Unit(Hash3(S.C.IX * 977 + S.C.IY, static_cast<int64_t>(IV), Side, Seed ^ 0xDA), 1)) * DistrictTone(S);
			// the back gardens: long strips, each a little different, a shed at the end, trees in the villas' gardens
			Ink Garden = Vary(Over(Sea4.Garden, Rgb(86, 80, 68), 0.5), HL, 0.08, 0.03);
			Garden = Over(Garden, Rgb(90, 84, 74), Unit(HL, 2) < 0.3 ? 0.5 : 0.0);
			if (bGardensTreed)
			{
				// (a villa's garden: big old trees, bare in winter, dark from above)
				Garden = Over(Garden, Tall(Bright(Over(Sea4.Broadleaf, Sea4.Conifer, 0.25), 0.95), TreeHigh(13.0)), 0.75 * SmoothStep(-0.05, 0.3, Noise(E / 16.0, N / 16.0, HL)));
			}
			// (the lots' edges: fences and hedges, faint)
			const double InLot = (OU / Lot - std::floor(OU / Lot) - 0.5) * Lot;
			Garden = Over(Garden, Bright(Sea4.Hedge, 1.2), 0.5 * Cover(Lot * 0.5 - std::fabs(InLot), 0.8, T));
			Garden = Bright(Garden, Tone);
			Ink Out = Garden;
			// the house: front Set m back from the street
			const double Fr = Back - Set;
			const double Box = std::max(std::fabs(InLot) - HouseW * 0.5, std::max(-Fr, Fr - HouseD));
			const double NormE = -SA * (Side == 0 ? -1.0 : 1.0), NormN = CA * (Side == 0 ? -1.0 : 1.0);
			Out = Roof(Out, Box, HouseD * 0.5 - Fr, NormE, NormN, Bright(TileOf(HL, 0.12), Tone), High, T);
			// streets and the cross streets
			Out = Over(Out, Street(T), std::max(Cover(ToStreet, StreetHalf, T), Cover(ToCross, StreetHalf, T)));
			const double Many = 1.0 - Shows(Gap * 0.5, T);
			if (Many > 0.0)
			{
				const double Built = HouseW / Lot * HouseD / (Gap * 0.5);
				Ink Mean = Over(Over(Over(Sea4.Garden, Rgb(86, 80, 68), 0.5), Rgb(98, 80, 70), Built), Street(T), 2.0 * StreetHalf / Gap);
				if (bGardensTreed)
				{
					Mean = Over(Mean, Bright(Over(Sea4.Broadleaf, Sea4.Conifer, 0.25), 0.95), 0.35);
				}
				Mean = InShade(Bright(Mean, Tone), ShadeShare(High, Gap * 0.5));
				Out = Over(Out, Tall(Mean, High * 0.4), Many);
			}
			return Out;
		}

		/** The old town (the art director's second look, 20:08Z: not cracked mud): narrow plots of mixed widths side by
		 * side along crooked lanes, each its own steep roof with no outline round it, small dark courts behind; a few
		 * darker lanes running in from the walls to one clear, light market square. */
		Ink OldTown(const Town& Tn, double E, double N, double T) const
		{
			const double SA = std::sin(Tn.CoreAxis), CA = std::cos(Tn.CoreAxis);
			const double U = E * SA + N * CA - 15.0, V = E * CA - N * SA + 8.0; // from the square's middle
			const double TT = std::max(1.0, T);
			// The market square and the cathedral close beside it: setts, open to the sky.
			const double Square = std::max(std::fabs(U) - 64.0, std::fabs(V) - 36.0);
			if (Square < -0.5 * TT)
			{
				return Rgb(140, 136, 126);
			}
			Ink Out = OldTownHouses(Tn, E, N, T);
			// The lanes in from the gates: five to seven, crooked, in the shade of the houses either side.
			const double R = std::hypot(U, V), Phi = std::atan2(V, U);
			const uint64_t HT = Hash(static_cast<int64_t>(Tn.CoreE), static_cast<int64_t>(Tn.CoreN), Seed ^ 0xE8);
			const int Lanes = 5 + static_cast<int>(3.0 * Unit(HT, 1));
			double Lane = Far;
			for (int K = 0; K < Lanes; ++K)
			{
				const double Theta = 2.0 * Pi * (K + 0.6 * (Unit(HT, 2 + K) - 0.5)) / Lanes + 0.14 * Noise(R / 420.0, K * 3.7, Seed ^ 0xE9);
				const double Delta = std::remainder(Phi - Theta, 2.0 * Pi);
				if (std::fabs(Delta) < 0.5 * Pi)
				{
					Lane = std::min(Lane, R * std::fabs(std::sin(Delta)));
				}
			}
			Out = Over(Out, Tall(Rgb(60, 58, 56), 0.0), LineCover(Lane, 4.5, T) * 0.9 * SmoothStep(40.0, 70.0, R));
			// the square's edge
			if (Square < 0.5 * TT)
			{
				Out = Over(Out, Rgb(140, 136, 126), Clamp01(0.5 - Square / TT));
			}
			return Out;
		}

		Ink OldTownHouses(const Town& Tn, double E, double N, double T) const
		{
			// (the old town's own frame: U along its long way, by the river, V across; pulled about so no lane is straight)
			const double SA = std::sin(Tn.CoreAxis), CA = std::cos(Tn.CoreAxis);
			const double WU = E * SA + N * CA + 16.0 * Noise(E / 140.0, N / 140.0, Seed ^ 0xE0);
			const double WV = E * CA - N * SA + 11.0 * Noise(E / 140.0, N / 140.0, Seed ^ 0xE1);
			// blocks between back lanes, longer along the town than across it, of mixed sizes
			const double BU = 115.0, BV = 58.0;
			const Cell Bk = Voronoi(WU / BU, WV / BV, Seed ^ 0xE2, 0.9);
			const uint64_t HK = Hash(Bk.IX, Bk.IY, Seed ^ 0xE5);
			const double Lane = Bk.Border * BV; // (near enough, in metres)
			const double Tone = 0.84 + 0.24 * Unit(HK, 1);
			// the plots: narrow, side by side along the block, each its own width (6 to 14 m) and roof
			const double PW = 6.5 + 4.0 * Unit(HK, 2);
			auto PlotEdge = [&](double K) { return (K + 0.42 * (Unit(Hash3(Bk.IX * 977 + Bk.IY, static_cast<int64_t>(K), 5, Seed ^ 0xE6)) - 0.5)) * PW; };
			double K = std::floor(WU / PW);
			double Lo = PlotEdge(K), Hi = PlotEdge(K + 1.0);
			if (WU < Lo)
			{
				K -= 1.0;
				Hi = Lo;
				Lo = PlotEdge(K);
			}
			else if (WU >= Hi)
			{
				K += 1.0;
				Lo = Hi;
				Hi = PlotEdge(K + 1.0);
			}
			const uint64_t HP = Hash3(Bk.IX * 977 + Bk.IY, static_cast<int64_t>(K), 9, Seed ^ 0xE4);
			// (a house fronts the lane some 15 to 24 m deep; behind it a small court, in the shade of the houses round it)
			const double Deep = 15.0 + 9.0 * Unit(HP, 3);
			Ink Out;
			if (Lane > Deep && Unit(HK, 4) < 0.7)
			{
				Out = Tall(Vary(Rgb(78, 74, 68), HP, 0.08, 0.03), 0.0);
				// (a back-house in some courts)
				Out = Over(Out, Tall(Bright(TileOf(HP ^ 7, 0.3), Tone), 9.0), Unit(HP, 6) < 0.3 ? 1.0 : 0.0);
			}
			else
			{
				// the roof: its ridge runs back from the lane, so its two slopes face either way along the block
				const double BX = SA * EX + CA * NX, BY = SA * EY + CA * NY; // (the town's long way on the board)
				const double Toward = (WU > 0.5 * (Lo + Hi) ? 1.0 : -1.0) * (BX * SunDX + BY * SunDY);
				const Ink Tile = Bright(TileOf(HP, 0.3), Tone * (0.9 + 0.2 * Unit(HP, 4)));
				Out = Tall(Bright(Tile, 1.0 + 0.24 * Shows(5.0, T) * Toward), 14.0 + 7.0 * Unit(HP, 5));
			}
			// the back lanes between the blocks: thin, in the houses' shade
			Out = Over(Out, Tall(Rgb(66, 64, 61), 0.0), LineCover(Lane, 1.8, T) * 0.85);
			// (a texel larger than the plots sees the block's mean: roofs, lanes and courts, half in shade)
			const double Many = 1.0 - Shows(9.0, T);
			if (Many > 0.0)
			{
				const Ink Mean = InShade(Bright(Vary(Rgb(96, 80, 72), HK, 0.06, 0.03), Tone), ShadeShare(18.0, 26.0));
				Out = Over(Out, Tall(Mean, 9.0), Many * 0.85);
			}
			return Out;
		}

		Ink Works(const Super& S, double E, double N, double T) const
		{
			const double CA = std::cos(S.Angle), SA = std::sin(S.Angle);
			const double U = (E - S.SE) * CA + (N - S.SN) * SA, V = -(E - S.SE) * SA + (N - S.SN) * CA;
			const double A = 70.0 + 90.0 * Unit(S.H, 3), Bb = 45.0 + 60.0 * Unit(S.H, 4);
			const double IU = std::floor(U / A), IV = std::floor(V / Bb);
			const double FU = U - IU * A, FV = V - IV * Bb;
			const uint64_t HC = Hash3(S.C.IX * 977 + S.C.IY, static_cast<int64_t>(IU), static_cast<int64_t>(IV), Seed ^ 0xE5);
			const double Edge = std::min({ FU, A - FU, FV, Bb - FV });
			const double Pick = Unit(HC, 1);
			Ink Yard = Rgb(104, 98, 90);
			Ink Out = Yard;
			if (Pick < 0.68)
			{
				// a hall: flat dark felt, or saw-tooth roof lights in stripes, or old corrugated rust
				const double RoofPick = Unit(HC, 2);
				Ink Hall = RoofPick < 0.4 ? Rgb(72, 72, 74) : RoofPick < 0.7 ? Rgb(84, 84, 86) : RoofPick < 0.85 ? Rgb(110, 86, 72) : Rgb(132, 130, 124);
				if (RoofPick >= 0.4 && RoofPick < 0.7)
				{
					// (saw-tooth: the glass faces one way, so stripes of light and dark across the hall)
					Hall = Bright(Hall, 1.0 + 0.22 * Shows(9.0, T) * (std::fmod(FU, 9.0) < 3.0 ? 1.0 : -0.4));
				}
				Out = Over(Out, Tall(Vary(Hall, HC, 0.06, 0.02), 12.0 + 8.0 * Unit(HC, 3)), Clamp01((Edge - 6.0) / std::max(1.0, T) + 0.5));
			}
			else if (Pick < 0.78)
			{
				// coal and coke heaps
				Out = Over(Out, Tall(Rgb(46, 44, 44), 5.0), SmoothStep(0.0, 0.3, Noise(E / 22.0, N / 22.0, HC)) * Clamp01((Edge - 4.0) / 4.0));
			}
			// the works' lanes and sidings
			Out = Over(Out, Rgb(92, 86, 80), Cover(Edge, 3.0, T) * 0.6);
			return Out;
		}

		/** A railway yard: tracks side by side along the strip, wagons standing on them in runs. */
		Ink Yard(const Zone& Z, double E, double N, double T) const
		{
			const double U = (E - Z.AE) * Z.AxisE + (N - Z.AN) * Z.AxisN, V = -(E - Z.AE) * Z.AxisN + (N - Z.AN) * Z.AxisE;
			const double Spacing = 4.8;
			// (the tracks in groups of eight, a lane of cinders between the groups)
			const double Group = Spacing * 8.0 + 9.0;
			const double G = std::floor((V + Z.Half) / Group), InG = (V + Z.Half) - G * Group;
			const double Track = std::floor(std::min(InG, Spacing * 8.0 - 0.01) / Spacing);
			const double FV = InG - Track * Spacing;
			Ink Ballast = Rgb(104, 98, 90);
			// the rails, dark, on a pale bed (a texel wider than a track sees their mean)
			const double Rails = Shows(Spacing, T) * (Cover(std::fabs(FV - 2.4), 0.9, T)) + (1.0 - Shows(Spacing, T)) * 0.3;
			Ink Out = Over(Ballast, Rgb(62, 58, 56), Rails);
			// wagons in runs of a few hundred metres, some tracks full, some empty (a track's own, so the strip is
			// streaked along its length even where a texel holds two tracks)
			const uint64_t HT = Hash(static_cast<int64_t>(G * 16.0 + Track), static_cast<int64_t>(std::floor(U / 260.0)), Seed ^ 0xE6);
			if (Unit(HT, 1) < 0.6)
			{
				const double InWagon = std::fmod(U + 1000.0 * Unit(HT, 2), 14.5);
				const Ink Wagon = Unit(HT, 3) < 0.6 ? Rgb(76, 60, 52) : Rgb(66, 66, 66);
				Out = Over(Out, Tall(Wagon, 4.0), (InWagon < 13.5 ? 1.0 : 0.0) * Shows(Spacing, T) * Cover(std::fabs(FV - 2.4), 1.5, T)
					+ (1.0 - Shows(Spacing, T)) * (0.35 + 0.5 * Unit(HT, 4)));
			}
			Out = Over(Out, Rgb(112, 104, 94), Cover(std::fabs(InG - Spacing * 8.0 - 4.5), 4.5, T));
			return Out;
		}

		Ink Docks(const Town& Tn, double E, double N, double T) const
		{
			// quays of pale stone and setts, the sheds in rows along them
			const Super S = SuperAt(Tn, E, N);
			const double CA = std::cos(S.Angle), SA = std::sin(S.Angle);
			const double U = (E - S.SE) * CA + (N - S.SN) * SA, V = -(E - S.SE) * SA + (N - S.SN) * CA;
			const double FV = V - std::floor(V / 70.0) * 70.0, FU = U - std::floor(U / 160.0) * 160.0;
			const uint64_t HS = Hash3(S.C.IX * 31 + S.C.IY, static_cast<int64_t>(std::floor(U / 160.0)), static_cast<int64_t>(std::floor(V / 70.0)), Seed ^ 0xE7);
			Ink Out = Vary(Rgb(112, 108, 102), HS, 0.06, 0.02);
			const double Box = std::max(std::fabs(FV - 35.0) - 16.0, std::fabs(FU - 80.0) - 70.0);
			if (Unit(HS, 1) < 0.75)
			{
				Out = Roof(Out, Box, FV - 35.0, -SA, CA, Vary(Unit(HS, 2) < 0.7 ? Rgb(86, 86, 90) : Rgb(112, 80, 66), HS, 0.1, 0.02), 11.0, T);
			}
			// the quay's tracks
			Out = Over(Out, Rgb(92, 86, 80), Cover(std::fabs(FV - 4.0), 2.0, T) * 0.7);
			return Out;
		}

		/** A harbour basin's water, with the ships along its quays. */
		Ink Basin(const Zone& Z, double E, double N, double T) const
		{
			const double U = (E - Z.AE) * Z.AxisE + (N - Z.AN) * Z.AxisN, V = -(E - Z.AE) * Z.AxisN + (N - Z.AN) * Z.AxisE;
			Ink Out = Rgb(48, 58, 64);
			// ships moored along both quays, bows any way
			const double FromQuay = Z.Half - std::fabs(V);
			const double Berth = 135.0;
			const int64_t Index = static_cast<int64_t>(std::floor(U / Berth));
			const uint64_t HS = Hash3(static_cast<int64_t>(Z.AE), Index, V > 0 ? 1 : 0, Seed ^ 0xE8);
			if (Unit(HS, 1) < 0.65)
			{
				const double Long = 70.0 + 50.0 * Unit(HS, 2), Beam = 11.0 + 6.0 * Unit(HS, 3);
				const double InBerth = U - (Index + 0.5) * Berth;
				const double Hull = std::max(std::fabs(InBerth) - Long * 0.5 + std::max(0.0, std::fabs(FromQuay - 4.0 - Beam * 0.5) - Beam * 0.3) * 1.5, std::fabs(FromQuay - 4.0 - Beam * 0.5) - Beam * 0.5);
				const Ink HullInk = Unit(HS, 4) < 0.6 ? Rgb(52, 50, 50) : Rgb(70, 62, 58);
				Out = Over(Out, Tall(HullInk, 9.0), Clamp01(0.5 - Hull / std::max(1.0, T)));
				// (the bridge and holds amidships, lighter)
				const double Mid = std::max(std::fabs(InBerth + 10.0) - 9.0, std::fabs(FromQuay - 4.0 - Beam * 0.5) - Beam * 0.32);
				Out = Over(Out, Tall(Rgb(134, 128, 118), 16.0), Clamp01(0.5 - Mid / std::max(1.0, T)) * 0.8);
			}
			return Out;
		}

		Ink ParkAt(double E, double N, double T, bool bCemetery) const
		{
			// Big old trees over lawns: in winter their bare crowns a dark grey-brown mass from above with a few conifers,
			// the lawns in broad open glades (not a two-colour blotch: the art director, 20 November 19:31Z); a
			// cemetery's paths in a grid between its rows of graves.
			Ink Out = bCemetery ? Rgb(84, 88, 72) : Sea4.Grass;
			const double Open = Fbm(E / 320.0, N / 320.0, Seed ^ 0xE9, 3) + 0.3 * Fbm(E / 70.0, N / 70.0, Seed ^ 0xED, 2);
			const double Trees = 1.0 - SmoothStep(bCemetery ? -0.02 : 0.1, bCemetery ? 0.06 : 0.18, Open);
			const double Pines = SmoothStep(0.2, 0.3, Fbm(E / 90.0, N / 90.0, Seed ^ 0xEE, 2));
			const Ink Crowns = Bright(Over(Bright(Sea4.Broadleaf, 0.9), Sea4.Conifer, Pines), 1.0 + 0.14 * Shows(9.0, T) * Noise(E / 8.0, N / 8.0, Seed ^ 0xEA)
				+ 0.1 * Shows(20.0, T) * Noise(E / 20.0, N / 20.0, Seed ^ 0xEC) + 0.07 * Noise(E / 60.0, N / 60.0, Seed ^ 0xEF));
			Out = Over(Out, Tall(Crowns, TreeHigh(17.0)), Trees);
			// paths: where slow noise crosses its middle
			const double Path = std::fabs(Fbm(E / 260.0, N / 260.0, Seed ^ 0xEB, 2));
			Out = Over(Out, Rgb(126, 120, 106), Cover(Path * 260.0, 2.2, T) * 0.7);
			if (bCemetery)
			{
				const double G = std::min(std::fabs(std::fmod(std::fabs(E), 45.0) - 22.5), std::fabs(std::fmod(std::fabs(N), 45.0) - 22.5));
				Out = Over(Out, Rgb(118, 114, 102), Cover(22.5 - G, 1.5, T) * 0.6);
			}
			return Out;
		}

		Ink Allotments(double E, double N, double T) const
		{
			// Small gardens in rows: in winter beds of dug dark earth, grass, a shed on each and bare fruit trees; from
			// above a fine weave of dark and light, not one flat field.
			const double A = 15.0, B = 30.0;
			const double IU = std::floor(E / A), IV = std::floor(N / B);
			const uint64_t H = Hash(static_cast<int64_t>(IU), static_cast<int64_t>(IV), Seed ^ 0xEC);
			const double Pick = Unit(H, 1);
			Ink Out = Pick < 0.5 ? Bright(Sea4.Plough, 0.95) : Pick < 0.75 ? Sea4.Garden : Pick < 0.9 ? Bright(Sea4.Grass, 0.9) : Sea4.Hedge;
			Out = Vary(Out, H, 0.1, 0.04);
			const double FU = E - IU * A, FV = N - IV * B;
			// the shed in a corner, the trees
			const double Shed = std::max(std::fabs(FU - 3.0) - 2.0, std::fabs(FV - 4.0) - 2.5);
			Out = Over(Out, Tall(Rgb(70, 66, 62), 3.0), Clamp01(0.5 - Shed / std::max(1.0, T)));
			Out = Over(Out, Tall(Sea4.Hedge, 5.0), 0.6 * SmoothStep(0.15, 0.4, Noise(E / 6.0, N / 6.0, H)));
			const double Many = 1.0 - Shows(A, T);
			Out = Over(Out, Tall(Over(Over(Sea4.Garden, Sea4.Plough, 0.5), Sea4.Hedge, 0.3), 2.0), Many);
			// the paths between the rows of plots
			Out = Over(Out, Rgb(118, 112, 100), Cover(std::min(FV, B - FV), 0.8, T) * 0.5);
			return Out;
		}

		Ink Airfield(double E, double N, double T) const
		{
			// close-mown grass in broad stripes, the landing ground's edge track
			const double Stripe = std::fmod(std::fabs(E * 0.7 + N * 0.7), 60.0) < 30.0 ? 1.04 : 0.96;
			Ink Out = Bright(Over(Sea4.Grass, Sea4.WetPasture, 0.25), Lerp(1.0, Stripe, Shows(30.0, T)));
			return Out;
		}

		/** The town at a point: which ground, then that ground's pattern. Returns false where the town is not. */
		bool TownAt(double X, double Y, double T, const Near& Nr, Ink& Out) const
		{
			if (X < TownMinX || X > TownMaxX || Y < TownMinY || Y > TownMaxY)
			{
				return false;
			}
			if (!Towns.empty())
			{
				const Town& Tn = Towns.front();
				double E = 0.0, N = 0.0;
				ToCompass(X, Y, E, N);
				// The zones of the plan come first: each its own ground.
				for (const Zone& Z : Tn.Zones)
				{
					const double In = Z.Inside(E, N);
					if (In <= 0.0)
					{
						continue;
					}
					switch (Z.Kind)
					{
					case Ground::Yard:
						Out = Yard(Z, E, N, T);
						break;
					case Ground::Basin:
						Out = Basin(Z, E, N, T);
						break;
					case Ground::Docks:
						Out = Docks(Tn, E, N, T);
						break;
					case Ground::Works:
					case Ground::Shipyard:
						Out = Works(SuperAt(Tn, E, N), E, N, T);
						break;
					case Ground::Park:
						Out = ParkAt(E, N, T, false);
						break;
					case Ground::Cemetery:
						Out = ParkAt(E, N, T, true);
						break;
					case Ground::Allot:
						Out = Allotments(E, N, T);
						break;
					case Ground::Airfield:
						Out = Airfield(E, N, T);
						break;
					case Ground::Station:
					{
						const double U = (E - Z.AE) * Z.AxisE + (N - Z.AN) * Z.AxisN;
						Out = Tall(Bright(Rgb(110, 112, 114), 1.0 + 0.12 * Shows(8.0, T) * (std::fmod(std::fabs(U), 8.0) < 4.0 ? 1.0 : -1.0)), 22.0);
						break;
					}
					case Ground::Meadow:
						return false;
					default:
					{
						const Super S = SuperAt(Tn, E, N);
						Out = Z.Kind == Ground::Old ? OldTown(Tn, E, N, T)
							: Z.Kind == Ground::Block ? Blocks(S, E, N, T, 13.0, 17.0, 110.0, 85.0, 7.5)
							: Z.Kind == Ground::Terrace ? Rows(S, E, N, T, 58.0, 280.0, 5.5, 7.5, 6.8, 12.0, 2.0, 10.0, false)
							: Rows(S, E, N, T, 80.0, 220.0, 5.0, 26.0, 12.0, 12.0, 8.0, 9.0, true);
						break;
					}
					}
					return true;
				}
				// Else the town's spread: dense in the middle, ragged at its edge, lifted along the main roads.
				const double DE = E - Tn.E, DN = N - Tn.N;
				const double UA = DE * std::sin(Tn.Axis) + DN * std::cos(Tn.Axis), VA = DE * std::cos(Tn.Axis) - DN * std::sin(Tn.Axis);
				double R = std::hypot(UA / Tn.Along, VA / Tn.Across);
				R += 0.22 * Fbm(E / 1400.0, N / 1400.0, Seed ^ 0xF2, 3) + 0.06 * Noise(E / 250.0, N / 250.0, Seed ^ 0xF3);
				if (Nr.S[1] && Nr.S[1]->Kind == LineKind::MainRoad)
				{
					R -= 0.18 * Clamp01(1.0 - Nr.D[1] / 450.0);
				}
				const double Core = Tn.CoreReach(E, N) * (1.0 + 0.1 * Fbm(E / 260.0, N / 260.0, Seed ^ 0xF5, 2));
				if (Core < Tn.CoreR)
				{
					Out = OldTown(Tn, E, N, T);
					return true;
				}
				if (R > 1.0)
				{
					return false;
				}
				const Super S = SuperAt(Tn, E, N);
				// (a super-block's own pick, so a street of houses is not half villas and half rows)
				const double Kind = R + 0.12 * (Unit(S.H, 9) - 0.5);
				if (Kind < 0.42)
				{
					Out = Blocks(S, E, N, T, 13.0, 17.0, 110.0, 85.0, 7.5);
				}
				else if (Kind < 0.66)
				{
					Out = Rows(S, E, N, T, 58.0, 280.0, 5.5, 7.5, 6.8, 12.0, 2.0, 10.0, false);
				}
				else if (Kind < 0.86)
				{
					Out = Unit(S.H, 10) < 0.12 ? Allotments(E, N, T) : Rows(S, E, N, T, 80.0, 220.0, 5.0, 27.0, 12.0, 12.0, 8.0, 9.0, true);
				}
				else
				{
					// the edge: houses thinning into the fields, allotments, a works now and then
					const double Pick = Unit(S.H, 11);
					if (Pick < 0.18)
					{
						Out = Allotments(E, N, T);
					}
					else if (Pick < 0.26)
					{
						Out = Works(S, E, N, T);
					}
					else if (Pick < 0.75)
					{
						return false;
					}
					else
					{
						Out = Rows(S, E, N, T, 90.0, 240.0, 4.5, 30.0, 11.0, 11.0, 8.0, 9.0, true);
					}
				}
				// the super-blocks' borders: the town's through streets, on some of them
				const uint64_t HSt = Hash(std::min(S.C.IX * 7919 + S.C.IY, S.C.OX * 7919 + S.C.OY), std::max(S.C.IX * 7919 + S.C.IY, S.C.OX * 7919 + S.C.OY), Seed ^ 0xF6);
				if (Unit(HSt, 1) < 0.35)
				{
					Out = Over(Out, Rgb(96, 93, 89), Cover(S.C.Border, 5.5, T) * 0.85);
				}
				return true;
			}
			return false;
		}

		/** The old town's ramparts: a ring of park on the old walls and the zig-zag star of the bastions' moat round it,
		 * dark water, on the side away from the river. */
		bool Rampart(double X, double Y, double T, const Near& Nr, const Ink& Under, Ink& Out) const
		{
			for (const Town& Tn : Towns)
			{
				if (!Tn.bRampart)
				{
					continue;
				}
				double E = 0.0, N = 0.0;
				ToCompass(X, Y, E, N);
				const double DE = E - Tn.CoreE, DN = N - Tn.CoreN;
				const double R = Tn.CoreReach(E, N);
				// (the walls stood round the old town on its own bank: none across the river)
				if (R < Tn.RampartR - 170.0 || R > Tn.RampartR + 190.0 || Nr.D[0] < Nr.Half[0] + 70.0
					|| (Tn.CoreSide != 0 && Nr.S[0] && Nr.D[0] < 2500.0 && (Nr.Side[0] > 0.0 ? 1 : -1) != Tn.CoreSide))
				{
					continue;
				}
				// (the bastions' points: the moat's line a zig-zag round the town, each point some 120 m out)
				const double U = DE * std::sin(Tn.CoreAxis) + DN * std::cos(Tn.CoreAxis), V = DE * std::cos(Tn.CoreAxis) - DN * std::sin(Tn.CoreAxis);
				const double Ang = std::atan2(V * Tn.CoreLong, U / Tn.CoreLong);
				const double Saw = std::fabs(std::fmod(Ang / (2.0 * Pi) * Tn.Bastions + 100.25, 1.0) - 0.5) * 2.0; // 0 at a point, 1 between
				const double Moat = Tn.RampartR + 35.0 + 120.0 * (1.0 - Saw);
				const double Half = 26.0;
				// the park: from the old town's edge out to the moat, the bastions' points within it
				const double WallIn = Tn.RampartR - 120.0 + 25.0 * Fbm(E / 200.0, N / 200.0, Seed ^ 0xF7, 2);
				const double InPark = Clamp01((R - WallIn) / std::max(1.0, T) + 0.5) * Clamp01((Moat - Half - R) / std::max(1.0, T) + 0.5);
				// (the walls' park: old trees close together over its paths, bare and dark)
				const double Old = SmoothStep(-0.25, 0.15, Noise(E / 35.0, N / 35.0, Seed ^ 0xF6));
				const Ink Walls = Over(ParkAt(E, N, T, false), Tall(Bright(Sea4.Broadleaf, 0.95 + 0.12 * Shows(9.0, T) * Noise(E / 8.0, N / 8.0, Seed ^ 0xEA)
					+ 0.1 * Shows(20.0, T) * Noise(E / 20.0, N / 20.0, Seed ^ 0xEC)), TreeHigh(17.0)), 0.75 * Old);
				Out = Over(Under, Walls, InPark);
				const double Water = Cover(std::fabs(R - Moat), Half, T);
				if (Water > 0.0)
				{
					Out = Over(Out, Tall(Rgb(44, 54, 60), 0.0), Water);
					Out.H = static_cast<float>(Lerp(Out.H, 0.0, Water));
				}
				return true;
			}
			return false;
		}

		int What(double X, double Y) const
		{
			if (Shore.At(X, Y) < 0.0)
			{
				return 0;
			}
			Near Nr;
			L.Query(X, Y, Nr);
			if (Nr.S[0] && Nr.D[0] < Nr.Half[0])
			{
				return 3;
			}
			for (const Town& Tn : Towns)
			{
				double E = 0.0, N = 0.0;
				ToCompass(X, Y, E, N);
				if (Tn.CoreReach(E, N) < Tn.CoreR)
				{
					return 5;
				}
			}
			Ink Here;
			if (TownAt(X, Y, 8.0, Nr, Here))
			{
				return 4;
			}
			const int CI = static_cast<int>(std::floor(X / VillageCell)) - VillageI0, CJ = static_cast<int>(std::floor(Y / VillageCell)) - VillageJ0;
			for (int J = CJ - 1; J <= CJ + 1; ++J)
			{
				for (int I = CI - 1; I <= CI + 1; ++I)
				{
					if (I < 0 || J < 0 || I >= VillageNI || J >= VillageNJ)
					{
						continue;
					}
					for (const int K : VillageGrid[static_cast<size_t>(J) * VillageNI + I])
					{
						const Village& Vi = Villages[K];
						const double VU = (X - Vi.X) * Vi.AxisX + (Y - Vi.Y) * Vi.AxisY, VV = -(X - Vi.X) * Vi.AxisY + (Y - Vi.Y) * Vi.AxisX;
						if (std::fabs(VU) < Vi.R * std::sqrt(Vi.Long) * 1.2 && std::fabs(VV) < 80.0)
						{
							return 6;
						}
					}
				}
			}
			return WoodLean(X, Y) > 0.0 ? 2 : 1;
		}

		Ink Sample(double X, double Y, double T) const
		{
			Near Nr;
			L.Query(X, Y, Nr);
			const double Coast = Shore.At(X, Y);
			Ink Out;
			if (Coast < 0.0)
			{
				// The sea: the scene's own paint offshore, lighter and greener over the shallows by the coast.
				const double Shallow = Clamp01(1.0 + Coast / 2500.0);
				Out = Over(Rgb(40, 62, 76), Rgb(70, 88, 88), Shallow * Shallow);
				Out = Bright(Out, 1.0 + 0.05 * Fbm(X / 900.0, Y / 900.0, Seed ^ 0x5E, 3));
				// mud flats and sand at the water's edge
				Out = Over(Out, Rgb(126, 120, 104), SmoothStep(-260.0, -40.0, Coast) * (0.5 + 0.5 * SmoothStep(-0.2, 0.3, Fbm(X / 700.0, Y / 700.0, Seed ^ 0x5F, 3))));
				return Out;
			}
			// The land: the town, a village, or the farmland and woods.
			const double RiverReach = Nr.S[0] ? (Nr.D[0] - Nr.Half[0]) / std::max(250.0, Nr.Half[0] * 9.0) : 9.0;
			if (!TownAt(X, Y, T, Nr, Out))
			{
				Out = Farmland(X, Y, T, RiverReach, Nr);
			}
			{
				Ink Ramparts;
				if (Rampart(X, Y, T, Nr, Out, Ramparts))
				{
					Out = Ramparts;
				}
			}
			// Villages near the point.
			{
				{
					const int CI = static_cast<int>(std::floor(X / VillageCell)) - VillageI0, CJ = static_cast<int>(std::floor(Y / VillageCell)) - VillageJ0;
					for (int DJ = -1; DJ <= 1; ++DJ)
					{
						for (int DI = -1; DI <= 1; ++DI)
						{
							const int I = CI + DI, J = CJ + DJ;
							if (I < 0 || J < 0 || I >= VillageNI || J >= VillageNJ)
							{
								continue;
							}
							for (const int K : VillageGrid[static_cast<size_t>(J) * VillageNI + I])
							{
								const Village& V = Villages[K];
								if (std::fabs(X - V.X) < V.Reach() && std::fabs(Y - V.Y) < V.Reach())
								{
									Out = VillageAt(Out, V, X, Y, T, Nr);
								}
							}
						}
					}
				}
			}
			// The coast's edge: a pale strip of sand or salt marsh.
			if (Coast < 80.0)
			{
				Out = Over(Out, Rgb(132, 126, 104), SmoothStep(80.0, 10.0, Coast) * 0.7);
			}
			// The river: its water, its banks, quays in the town, groynes outside it.
			if (Nr.S[0])
			{
				const double D = Nr.D[0], Half = Nr.Half[0];
				const bool bInTown = TownReach(X, Y) < 0.75;
				// (banks: a light stone quay in the town, a strip of rough grass and reeds outside)
				// (the art director, 20 November 19:31Z: the darkest line on the map, slate, with a thin pale edge of mud, quays
				// and groynes so it reads as water and not a painted road)
				const Ink Bank = bInTown ? Rgb(142, 138, 130) : Rgb(130, 126, 108);
				Out = Over(Out, Bank, Cover(std::fabs(D - Half - 5.0), 6.0, T) * (D > Half ? 0.85 : 0.0));
				Ink Water = Rgb(50, 60, 66);
				Water = Bright(Water, 1.0 + 0.05 * Fbm(X / 400.0, Y / 400.0, Seed ^ 0x6A, 2));
				// groynes: short stone spurs out from both banks every ~110 m, outside the town on a wide river
				if (!bInTown && Half > 50.0)
				{
					const double Spur = std::fmod(Nr.Along[0] + 1000.0, 110.0);
					const double In = Half - D;
					Water = Over(Water, Rgb(120, 116, 104), Cover(std::fabs(Spur - 55.0), 2.5, T) * (In > 0 && In < Half * 0.28 ? 0.9 : 0.0));
				}
				Out = Over(Out, Tall(Water, 0.0), Cover(D, Half, T));
			}
			// Railways and roads over everything; bridges over the water.
			if (Nr.S[2])
			{
				const double D = Nr.D[2];
				Out = Over(Out, Rgb(102, 94, 86), Cover(D, Nr.Half[2] + 2.0, T) * 0.7);
				Out = Over(Out, Tall(Rgb(64, 60, 58), Nr.D[0] < Nr.Half[0] ? 8.0 : 0.0), Cover(D, 4.5, T) * 0.75);
			}
			if (Nr.S[1])
			{
				const Seg& S = *Nr.S[1];
				const double D = Nr.D[1], Half = Nr.Half[1];
				const bool bOverWater = Nr.D[0] < Nr.Half[0];
				const bool bInTown = S.Kind != LineKind::Road && TownReach(X, Y) < 0.85;
				if (S.Flags & Avenue && !bOverWater && !bInTown)
				{
					// (trees along both sides, a crown every 10 to 14 m)
					const double At = std::fmod(Nr.Along[1], 12.0) - 6.0;
					const double Tree = std::hypot(At, D - Half - 4.5);
					Out = Over(Out, Tall(Bright(Sea4.Hedge, 0.95), TreeHigh(12.0)), Shows(12.0, T) * Clamp01(0.5 - (Tree - 4.0) / std::max(1.0, T)) + (1.0 - Shows(12.0, T)) * Cover(std::fabs(D - Half - 4.5), 3.5, T) * 0.7);
				}
				// (in the town the main roads are its light spokes into the middle: the art director, 20 November 19:31Z)
				Ink Road = S.Kind == LineKind::Autobahn ? Rgb(150, 148, 140) : S.Kind == LineKind::MainRoad ? (bInTown ? Rgb(138, 134, 126) : Rgb(124, 120, 112)) : Rgb(136, 128, 114);
				if (S.Kind == LineKind::Autobahn)
				{
					// two carriageways of pale concrete, a strip of grass between
					Road = Over(Road, Sea4.Grass, Cover(D, 2.5, T));
				}
				Out = Over(Out, Tall(Road, bOverWater ? 8.0 : Out.H * 0.0), Cover(D, Half, T) * (bOverWater && !(S.Flags & Bridged) ? 0.0 : 0.95));
			}
			// Spires and chimneys.
			{
				const int I = static_cast<int>(std::floor((X - SpireX0) / SpireCell)), J = static_cast<int>(std::floor((Y - SpireY0) / SpireCell));
				if (I >= 0 && J >= 0 && I < SpireNX && J < SpireNY)
				{
					for (const int K : SpireGrid[static_cast<size_t>(J) * SpireNX + I])
					{
						const Spire& Sp = Spires[K];
						double DE = 0.0, DN = 0.0;
						// (in compass: a church's nave runs east from its tower)
						const double DX = X - Sp.X, DY = Y - Sp.Y;
						DE = DX * EX + DY * EY;
						DN = DX * NX + DY * NY;
						if (Sp.bChurch)
						{
							const double Nave = std::max(std::fabs(DE - 24.0) - 18.0, std::fabs(DN) - 8.5);
							const double Toward = (DN > 0 ? 1.0 : -1.0) * (NX * SunDX + NY * SunDY);
							Out = Over(Out, Tall(Bright(Rgb(80, 82, 88), 1.0 + 0.15 * Toward * Shows(6.0, T)), 22.0), Clamp01(0.5 - Nave / std::max(1.0, T)));
						}
						// (a tower thinner than a texel still stands: it fills the texel's height, so its shadow is cast)
						const double Tower = std::max(std::fabs(DE), std::fabs(DN)) - 4.5;
						if (Tower < 0.5 * T)
						{
							Out = Over(Out, Tall(Rgb(84, 84, 88), Sp.High), Clamp01(0.5 - Tower / std::max(1.0, T)));
							Out.H = static_cast<float>(Sp.High);
						}
					}
				}
			}
			// Gasometers.
			for (const Town& Tn : Towns)
			{
				for (const Pt& G : Tn.Gasometers)
				{
					const Pt B = ToBoard(G.first, G.second);
					const double D = std::hypot(X - B.first, Y - B.second);
					if (D < 50.0)
					{
						Out = Over(Out, Tall(Over(Rgb(80, 82, 84), Rgb(110, 110, 108), Cover(std::fabs(D - 22.0), 2.0, T) * 0.6), 34.0), Cover(D, 32.0, T));
					}
				}
			}
			return Out;
		}

	};

	std::vector<Frame> Frames(const Place& P)
	{
		// (the raid comes in from the board's south edge: the near picture reaches 24.6 km back along its way in and
		// 8.2 km past the aim, 8.2 km either side; the middle one 49 km square; the far one 197 km, to the haze. Each
		// finer one's edges fall on whole cells of the coarser one's sheet, 2,048 m and 8,192 m, so the scene lays each
		// coarser sheet with a hole where the finer one lies.)
		std::vector<Frame> Out(3);
		Out[0] = { P.AimX - 8192.0, P.AimY - 24576.0, 8.0, 2048, 4096 };
		Out[1] = { P.AimX - 24576.0, P.AimY - 34816.0, 24.0, 2048, 2048 };
		Out[2] = { P.AimX - 98304.0, P.AimY - 116736.0, 96.0, 2048, 2048 };
		return Out;
	}

	void SunAt(double Latitude, int Month, double Hour, double NorthX, double NorthY, double& X, double& Y, double& Z)
	{
		const double Rad = Pi / 180.0;
		const double Day = 30.44 * (std::clamp(Month, 1, 12) - 1) + 15.0;
		const double Dec = -23.44 * Rad * std::cos(2.0 * Pi * (Day + 10.0) / 365.0);
		const double Lat = Latitude * Rad, H = (Hour - 12.0) * 15.0 * Rad;
		const double Elev = std::asin(std::clamp(std::sin(Lat) * std::sin(Dec) + std::cos(Lat) * std::cos(Dec) * std::cos(H), -1.0, 1.0));
		// (its bearing from north toward east: south at noon, west of south after)
		const double Bearing = std::atan2(-std::cos(Dec) * std::sin(H), std::sin(Dec) * std::cos(Lat) - std::cos(Dec) * std::sin(Lat) * std::cos(H));
		const double L = std::hypot(NorthX, NorthY);
		const double NX = L > 0.0 ? NorthX / L : 0.0, NY = L > 0.0 ? NorthY / L : 1.0;
		const double E = std::sin(Bearing) * std::cos(Elev), N = std::cos(Bearing) * std::cos(Elev);
		// (east on the board is north turned a quarter clockwise)
		X = E * NY + N * NX;
		Y = -E * NX + N * NY;
		Z = std::sin(Elev);
	}

	Painter::Painter(const Place& P, const std::string& PlansJson)
		: Inner(new Impl(P, PlansJson))
	{
	}

	Painter::~Painter() = default;

	const Tally& Painter::Laid() const { return Inner->Count; }

	int Painter::What(double X, double Y) const { return Inner->What(X, Y); }

	void Painter::PaintRows(const Frame& F, int Row0, int Row1, uint8_t* Rgba, float* Heights) const
	{
		for (int J = std::max(0, Row0); J < std::min(F.H, Row1); ++J)
		{
			const double Y = F.Y0 + (F.H - J - 0.5) * F.Step;
			for (int K = 0; K < F.W; ++K)
			{
				const double X = F.X0 + (K + 0.5) * F.Step;
				const Ink C = Inner->Sample(X, Y, F.Step);
				uint8_t* Out = Rgba + (static_cast<size_t>(J) * F.W + K) * 4;
				Out[0] = static_cast<uint8_t>(std::clamp(C.R + 0.5f, 0.0f, 255.0f));
				Out[1] = static_cast<uint8_t>(std::clamp(C.G + 0.5f, 0.0f, 255.0f));
				Out[2] = static_cast<uint8_t>(std::clamp(C.B + 0.5f, 0.0f, 255.0f));
				Out[3] = 255;
				if (Heights)
				{
					Heights[static_cast<size_t>(J) * F.W + K] = C.H;
				}
			}
		}
	}

	void Painter::ShadeRows(const Frame& F, int Row0, int Row1, uint8_t* Rgba, const float* Heights) const
	{
		// Each texel looks toward the sun along the ground, a texel at a time: anything there taller than the sun's
		// line over this texel shades it, by how far it reaches over that line (a soft edge a texel wide).
		const double Rise = Inner->TanSun * F.Step; // (the sun's line climbs this much a texel)
		const double DX = Inner->SunDX, DY = -Inner->SunDY; // (rows run south: the picture's y is the board's -y)
		const int Most = std::min(40, static_cast<int>(std::ceil(100.0 / Rise)));
		for (int J = std::max(0, Row0); J < std::min(F.H, Row1); ++J)
		{
			for (int K = 0; K < F.W; ++K)
			{
				const size_t At = static_cast<size_t>(J) * F.W + K;
				const double H0 = Heights[At];
				double Shade = 0.0;
				for (int S = 1; S <= Most; ++S)
				{
					const int KX = K + static_cast<int>(std::lround(DX * S)), JY = J + static_cast<int>(std::lround(DY * S));
					if (KX < 0 || JY < 0 || KX >= F.W || JY >= F.H)
					{
						break;
					}
					const double Over = Heights[static_cast<size_t>(JY) * F.W + KX] - (H0 + Rise * S);
					if (Over > 0.0)
					{
						Shade = std::max(Shade, Clamp01(Over / Rise + 0.35));
						if (Shade >= 1.0)
						{
							break;
						}
					}
				}
				if (Shade > 0.0)
				{
					uint8_t* Px = Rgba + At * 4;
					const Ink C = InShade(Ink{ static_cast<float>(Px[0]), static_cast<float>(Px[1]), static_cast<float>(Px[2]), 0.0f }, Shade);
					Px[0] = static_cast<uint8_t>(std::clamp(C.R + 0.5f, 0.0f, 255.0f));
					Px[1] = static_cast<uint8_t>(std::clamp(C.G + 0.5f, 0.0f, 255.0f));
					Px[2] = static_cast<uint8_t>(std::clamp(C.B + 0.5f, 0.0f, 255.0f));
				}
			}
		}
	}

	void Painter::Paint(const Frame& F, std::vector<uint8_t>& Rgba) const
	{
		Rgba.assign(static_cast<size_t>(F.W) * F.H * 4, 0);
		std::vector<float> Heights(static_cast<size_t>(F.W) * F.H, 0.0f);
		PaintRows(F, 0, F.H, Rgba.data(), Heights.data());
		if (Shades(F))
		{
			ShadeRows(F, 0, F.H, Rgba.data(), Heights.data());
		}
	}

	void Painter::Scar(const Frame& F, double X, double Y, double Kilograms, uint8_t* Pixels, bool bBgra, int Box[4]) const
	{
		// A crater (a 250 kg bomb's some 10 m across on soft ground) and the spoil thrown round it, darker for being
		// fresh earth; on a roof, the roof broken in to dark rubble some 25 m round.
		const double R = 5.0 * std::cbrt(std::max(50.0, Kilograms) / 250.0);
		const double Spoil = R * 2.6;
		const int K0 = static_cast<int>(std::floor((X - Spoil - F.X0) / F.Step)), K1 = static_cast<int>(std::floor((X + Spoil - F.X0) / F.Step));
		const int J0 = static_cast<int>(std::floor((F.Y1() - (Y + Spoil)) / F.Step)), J1 = static_cast<int>(std::floor((F.Y1() - (Y - Spoil)) / F.Step));
		Box[0] = std::max(0, K0);
		Box[1] = std::max(0, J0);
		Box[2] = std::min(F.W - 1, K1);
		Box[3] = std::min(F.H - 1, J1);
		for (int J = Box[1]; J <= Box[3]; ++J)
		{
			for (int K = Box[0]; K <= Box[2]; ++K)
			{
				const double PX = F.X0 + (K + 0.5) * F.Step, PY = F.Y1() - (J + 0.5) * F.Step;
				const double D = std::hypot(PX - X, PY - Y);
				uint8_t* Px = Pixels + (static_cast<size_t>(J) * F.W + K) * 4;
				const double Pit = Cover(D, R, F.Step), Thrown = Cover(D, Spoil, F.Step) * (1.0 - Pit) * 0.6;
				const double Dark = 1.0 - 0.55 * Pit - 0.25 * Thrown;
				const double Earth[3] = { 92.0, 80.0, 66.0 };
				for (int C = 0; C < 3; ++C)
				{
					const double V = Px[C] * (1.0 - Thrown * 0.5) + Earth[bBgra ? 2 - C : C] * Thrown * 0.5;
					Px[C] = static_cast<uint8_t>(std::clamp(V * Dark + 0.5, 0.0, 255.0));
				}
			}
		}
	}
}
