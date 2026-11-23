#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <vector>

/**
 * The dry plains: the North China plain and its loess hills (Peking, Tientsin, Hopei, Shantung, Honan: the Marco Polo
 * Bridge 1937, Taierhchuang 1938, the Hundred Regiments 1940, Ichi-Go's Honan 1944), Manchuria (Mukden 1931, the
 * Soviets' August storm 1945) and the plains of India and the Indus (a drive past Imphal, a what-if). Brian, 23 November
 * 2026: "do 1 and 3" (the third of the three missing battlefields; the art director's set
 * battle-models/asia/sets/dryplain.json is its brief).
 *
 *   - open fields in long strips, no hedges: gaoliang (sorghum) two and three metres tall from July, its heads red-brown
 *     in August and September; millet; soybeans; the winter wheat green over the loess from March, gold in early June,
 *     soybeans after it; Manchuria's one summer crop on the black earth and its long white winter; India's two seasons,
 *     the monsoon's jowar, bajra and pulses and the winter's wheat and the mustard's yellow, the plain parched from March
 *     to the rains;
 *   - the loess: pale yellow-brown earth, dust, the cart tracks worn down into it, poplars along them;
 *   - grave mounds in their family groups on the balks and the open ground; straw stacks and the gaoliang's shocks after
 *     the harvest;
 *   - villages of flat-roofed mud and stone houses in rows facing south (India's not in rows), walled farmyards, a pagoda;
 *     the old towns' rammed-earth walls in China and Manchuria;
 *   - loess terraces where the rules' rough ground climbs, bare hills beyond.
 *
 * Its kit is the Asia thread's battle models (Content/Models/Asia): Poplar, Willow, HillPine, NorthHouse, Farmyard,
 * TieredTower, GraveMound, StrawStack, DryBoulder, DryOutcrop. The crops, the stubble and the grass it builds itself.
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Gao = 0,      // gaoliang (sorghum); India's jowar
			Millet = 1,   // foxtail millet; India's bajra
			Wheat = 2,    // wheat alone: Manchuria's spring wheat, India's winter wheat
			Soy = 3,      // soybeans; India's pulses
			Mustard = 4,  // India's winter mustard
			Fallow = 5,   // ploughed and resting
			Grass = 6,    // the open ground: grazing, the graves' ground
			Terrace = 7,  // the loess terraces: millet on the treads
			Hill = 8,     // the hills' thin grass and rock
			WheatSoy = 9, // North China's two crops a year: the winter wheat, then soybeans after its June harvest
			Pair0 = 16    // India's two seasons: 16 + 3 x the monsoon's (jowar, bajra, pulses) + the winter's (wheat, mustard, none)
		};

		enum Slot : int
		{
			KPoplar = 0,
			KWillow = 1,
			KPine = 2,
			KHouse = 3,
			KFarm = 4,
			KTower = 5,
			KGrave = 6,
			KStack = 7,
			KBoulder = 8,
			KOutcrop = 9
		};

		enum Clut : int
		{
			CGao = 0,     // a clump of gaoliang: two rows of tall stalks, long leaves, a head on each
			CGrain = 1,   // a clump of wheat or millet
			CBush = 2,    // a leafy clump: soybeans, the mustard, the young gaoliang, the hills' scrub (by its tint)
			CStubble = 3, // stubble in its rows (the gaoliang's tall stubs by its height); the winter wheat's green rows
			CGrass = 4,   // a grass tuft
			CReed = 5     // reeds in the marsh and the ditches
		};

		LandField::Base KitOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K); }
		LandField::Base ClutOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Clut0) + K); }

		// (sRGB as painted, the art director's to check: the set's loess A7916B, loess_furrows A7936C, gaoliang_stubble
		// 988561, scree_dry A48769, hill_grass 5F6E3D; the gaoliang model's leaf 62743C, leaf_dry A49660, grain 6A3C28)
		const Tone Loess = Rgb(167, 145, 107);
		const Tone Furrows = Rgb(150, 128, 94);
		const Tone BlackEarth = Rgb(100, 88, 70);
		const Tone Alluvium = Rgb(164, 144, 110);
		const Tone GaoStubbleC = Rgb(152, 133, 97);
		const Tone Scree = Rgb(164, 135, 105);
		const Tone HillGrass = Rgb(95, 110, 61);
		const Tone OpenGrass = Rgb(116, 120, 76);
		const Tone GaoGreen = Rgb(80, 102, 52);
		const Tone GaoRipe = Rgb(118, 100, 70); // (the leaves drying olive, the heads a dull red-brown over them: so from above)
		const Tone MilletGreen = Rgb(100, 122, 60);
		const Tone MilletRipe = Rgb(176, 150, 90);
		const Tone WheatGreen = Rgb(100, 124, 62);
		const Tone WheatGold = Rgb(186, 160, 98);
		const Tone SoyGreen = Rgb(76, 100, 50);
		const Tone SoyRipe = Rgb(160, 138, 78);
		const Tone MustardFlower = Rgb(206, 184, 62);
		const Tone StrawC = Rgb(176, 156, 112);
		const Tone YellowWater = Rgb(128, 116, 92);

		/** Points from First on lit as the sky lights the ground (normals up, leaning a little out). */
		void LitUp(LandField::Mesh& M, size_t First)
		{
			for (size_t P = First; P < M.Points(); ++P)
			{
				const double X = M.Positions[3 * P], Y = M.Positions[3 * P + 1], L = std::hypot(X, Y);
				const double NX = L > 1e-6 ? 0.25 * X / L : 0.0, NY = L > 1e-6 ? 0.25 * Y / L : 0.0, N = std::sqrt(NX * NX + NY * NY + 1.0);
				M.Normals[3 * P] = static_cast<float>(NX / N);
				M.Normals[3 * P + 1] = static_cast<float>(NY / N);
				M.Normals[3 * P + 2] = static_cast<float>(1.0 / N);
			}
		}

		/** A tuft of Blades blades from a foot Spread across at (CX, CY), High to High + Extra tall, leaning out Lean. */
		void Tuft(Writer& W, double CX, double CY, int Blades, double Spread, double High, double Extra, double Lean, double Width, uint64_t S, const Tone& Foot, const Tone& Tip)
		{
			for (int B = 0; B < Blades; ++B)
			{
				const double A = 2.0 * Pi * (B + 0.6 * Unit(Hash(S, 1, B))) / Blades;
				const double Out = Lean * (0.5 + Unit(Hash(S, 2, B))), Tall = High + Extra * Unit(Hash(S, 3, B));
				const double FX = CX + Spread * std::cos(A) * Unit(Hash(S, 4, B)), FY = CY + Spread * std::sin(A) * Unit(Hash(S, 4, B));
				W.Blade(FX, FY, 0.0, FX + Out * std::cos(A), FY + Out * std::sin(A), Tall, Width, Foot, Tip);
			}
		}

		/** A leaf as a ribbon: from P[0] through P[1] and P[2] to its tip P[3], Wd[0] to Wd[2] wide at the first three, both
		 * faces lit as grass is (from above), Foot to Tip in colour. */
		void Ribbon(Writer& W, const double (&P)[4][3], const double (&Wd)[3], const Tone& Foot, const Tone& Tip)
		{
			uint32_t PA = 0, PB = 0, QA = 0, QB = 0;
			for (int K = 0; K < 3; ++K)
			{
				const double* P0 = P[K];
				const double* P1 = P[K + 1];
				double AX = -(P1[1] - P0[1]), AY = P1[0] - P0[0];
				const double L = std::hypot(AX, AY);
				if (L < 1e-9) { AX = 1.0; AY = 0.0; }
				else { AX /= L; AY /= L; }
				const double UX = P1[0] - P0[0], UY = P1[1] - P0[1], UZ = P1[2] - P0[2];
				double NX = AY * UZ, NY = -AX * UZ, NZ = AX * UY - AY * UX;
				const double NL = std::sqrt(NX * NX + NY * NY + NZ * NZ);
				if (NL > 1e-9) { NX /= NL; NY /= NL; NZ /= NL; }
				NZ = std::fabs(NZ) + 0.6;
				const double NN = std::sqrt(NX * NX + NY * NY + NZ * NZ);
				NX /= NN; NY /= NN; NZ /= NN;
				const Tone C0 = Mixed(Foot, Tip, K / 3.0), C1 = Mixed(Foot, Tip, (K + 1) / 3.0);
				const double H0 = 0.5 * Wd[K];
				if (K == 0)
				{
					PA = W.Vert(P0[0] - AX * H0, P0[1] - AY * H0, P0[2], NX, NY, NZ, C0);
					PB = W.Vert(P0[0] + AX * H0, P0[1] + AY * H0, P0[2], NX, NY, NZ, C0);
					QA = W.Vert(P0[0] - AX * H0, P0[1] - AY * H0, P0[2], -NX, -NY, NZ, C0);
					QB = W.Vert(P0[0] + AX * H0, P0[1] + AY * H0, P0[2], -NX, -NY, NZ, C0);
				}
				if (K == 2)
				{
					const uint32_t T = W.Vert(P1[0], P1[1], P1[2], NX, NY, NZ, C1), U = W.Vert(P1[0], P1[1], P1[2], -NX, -NY, NZ, C1);
					W.Tri(PA, PB, T);
					W.Tri(QB, QA, U);
					break;
				}
				const double H1 = 0.5 * Wd[K + 1];
				const uint32_t NA = W.Vert(P1[0] - AX * H1, P1[1] - AY * H1, P1[2], NX, NY, NZ, C1), NB = W.Vert(P1[0] + AX * H1, P1[1] + AY * H1, P1[2], NX, NY, NZ, C1);
				const uint32_t MA = W.Vert(P1[0] - AX * H1, P1[1] - AY * H1, P1[2], -NX, -NY, NZ, C1), MB = W.Vert(P1[0] + AX * H1, P1[1] + AY * H1, P1[2], -NX, -NY, NZ, C1);
				W.Tri(PA, PB, NB);
				W.Tri(PA, NB, NA);
				W.Tri(QB, QA, MA);
				W.Tri(QB, MA, MB);
				PA = NA; PB = NB; QA = MA; QB = MB;
			}
		}

		class Plain final : public Region
		{
		public:
			Plain(const LandField::Field& F, uint64_t S) : Region(F, S)
			{
				const int M = F.Month >= 1 && F.Month <= 12 ? F.Month : 7;
				Month = M;
				// (India west of the Burma hills; Manchuria north of the Great Wall's end at Shanhaikwan; between them the
				// North China plain)
				bIndia = F.Lon < 95.0;
				bManchuria = !bIndia && F.Lat > 41.0;
				// (the grass's year: China's green from May to September; India's from the monsoon in July into the cold
				// season, parched from March to the rains)
				if (bIndia) Green = (M >= 7 && M <= 10) ? 1.0 : (M == 11 || M == 6) ? 0.55 : (M == 12 || M <= 2) ? 0.4 : 0.12;
				else Green = (M >= 5 && M <= 9) ? 1.0 : (M == 4 || M == 10) ? 0.5 : 0.1;
				bHills = Unit(Hash(S, 0x4111)) < (bIndia ? 0.3 : 0.6);
				SnowLying = SnowOf(F, Look::Plain);
				double Sum = 0.0;
				for (const double H : F.Height) Sum += H;
				MeanZ = F.Height.empty() ? 0.0 : Sum / static_cast<double>(F.Height.size());
			}
			Look Which() const override { return Look::Plain; }

			// ---------------------------------------------------------------- the year

			/** A crop's year, in months from the first of January (12.5 the middle of December): sown, grown full, ripe, cut. */
			struct Cal
			{
				double Sow, Full, Ripe, Cut;
			};
			Cal CalOf(int C, bool bSecond) const
			{
				if (bIndia)
				{
					switch (C)
					{
					case Gao: return { 6.8, 8.8, 9.8, 10.7 };
					case Millet: return { 6.8, 8.6, 9.3, 10.0 };
					case Soy: return { 6.9, 8.5, 9.6, 10.4 };
					case Wheat: return { 11.0, 2.4, 3.4, 4.0 };
					default: return { 10.4, 12.3, 3.4, 3.6 }; // (the mustard: in flower from late December into February)
					}
				}
				if (bManchuria)
				{
					switch (C)
					{
					case Gao: return { 5.0, 7.5, 8.7, 9.7 };
					case Millet: return { 5.0, 7.4, 8.5, 9.3 };
					case Soy: return { 5.1, 7.5, 8.9, 9.7 };
					default: return { 4.2, 6.4, 7.4, 8.0 }; // (the spring wheat)
					}
				}
				switch (C)
				{
				case Gao: return { 4.8, 7.6, 8.7, 9.8 };
				case Millet: return { 5.0, 7.5, 8.6, 9.4 };
				case Soy: return bSecond ? Cal{ 6.5, 8.1, 9.3, 10.0 } : Cal{ 5.0, 7.5, 9.0, 9.8 };
				default: return { 10.0, 4.9, 5.9, 6.4 }; // (the winter wheat, sown in October, gold at the month's end, cut in early June)
				}
			}

			/** Where a crop is in its year: up (High 0 to 1 of its height, Ripe 0 green to 1 ripe), or cut Since months. */
			struct Life
			{
				int Crop = Fallow;
				bool bUp = false;
				double High = 0.0, Ripe = 0.0, Since = 99.0;
			};
			static double Wrap(double X) { return std::fmod(X + 24.0, 12.0); }
			Life LifeOf(int C, double T, bool bSecond = false) const
			{
				Life L;
				L.Crop = C;
				if (C != Gao && C != Millet && C != Wheat && C != Soy && C != Mustard) return L;
				const Cal K = CalOf(C, bSecond);
				const double D = Wrap(T - K.Sow), LF = Wrap(K.Full - K.Sow), LR = Wrap(K.Ripe - K.Sow), LC = Wrap(K.Cut - K.Sow);
				if (D >= LC)
				{
					L.Since = D - LC;
					return L;
				}
				L.bUp = true;
				// (a crop sown for the winter lies low through it: the winter wheat a hand high till March)
				const double G = Clamp01(D / std::max(0.1, LF));
				L.High = D < LF ? 0.08 + 0.92 * (K.Sow >= 9.5 ? G * G * G * G : G) : 1.0;
				L.Ripe = D < LF ? 0.0 : D < LR ? (D - LF) / std::max(0.1, LR - LF) : 1.0;
				return L;
			}
			static Life Either(const Life& A, const Life& B)
			{
				if (A.bUp) return A;
				if (B.bUp) return B;
				return A.Since <= B.Since ? A : B;
			}
			static int Kharif(int C) { return (C - Pair0) / 3 == 0 ? Gao : (C - Pair0) / 3 == 1 ? Millet : Soy; }
			static int Rabi(int C) { return (C - Pair0) % 3 == 0 ? Wheat : (C - Pair0) % 3 == 1 ? Mustard : Fallow; }
			/** A parcel's (or a strip's) crop at T: its two crops' where it has two. */
			Life ParcelLife(int C, double T) const
			{
				if (C == WheatSoy) return Either(LifeOf(Wheat, T), LifeOf(Soy, T, true));
				if (C >= Pair0 && C < Pair0 + 9) return Either(LifeOf(Kharif(C), T), LifeOf(Rabi(C), T));
				if (C == Terrace) return LifeOf(Millet, T);
				return LifeOf(C, T);
			}
			double T0() const { return Month + 0.5; }

			// ---------------------------------------------------------------- the strips

			/** A parcel's strip at a place: the families' strips some 9 to 19 m across the grain, each sown and cut in its
			 * own week (Jit months), its own shade; one in ten ploughed and resting; Balk metres to its edge. */
			struct Strip
			{
				uint64_t Key = 0;
				double Jit = 0.0, Balk = 99.0, Shade = 1.0;
				bool bFallow = false;
			};
			Strip StripOf(const Spot& S) const
			{
				Strip St;
				const double W = 9.0 + 10.0 * Unit(Hash(S.Key, 0x5791));
				const double B = S.Across / W + Unit(Hash(S.Key, 0x5792));
				const double F = B - std::floor(B);
				St.Key = Hash(S.Key, static_cast<uint64_t>(std::floor(B) + 512.0));
				St.Balk = std::min(F, 1.0 - F) * W;
				St.Jit = 0.9 * (Unit(Hash(St.Key, 0x717)) - 0.5) + 0.6 * (Unit(Hash(S.Key, 0x718)) - 0.5);
				St.bFallow = Unit(Hash(St.Key, 0xFA11)) < 0.1;
				St.Shade = 0.92 + 0.16 * Unit(Hash(St.Key, 0x5AD));
				return St;
			}
			/** A strip's crop: the parcel's, or (in China's and Manchuria's low crops) a neighbour's, one strip in three. */
			int StripCrop(const Spot& S, const Strip& St) const
			{
				const int C = S.Crop;
				if (C == Gao || C >= Pair0) return C;
				if (St.bFallow) return Fallow;
				if (C == Soy || C == WheatSoy || C == Wheat || C == Fallow)
					if (Unit(Hash(St.Key, 0x5A9)) < 0.33)
					{
						const double U = Unit(Hash(St.Key, 0x5AA));
						return U < 0.45 ? Soy : U < 0.88 ? (bManchuria ? Wheat : WheatSoy) : Fallow;
					}
				return C;
			}
			bool Tilled(int C) const { return C != Grass && C != Hill && C >= 0; }

			// ---------------------------------------------------------------- the land

			double Outside(double X, double Y) const
			{
				const double Dx = std::max({ 0.0, -X, X - Fd.Width() }), Dy = std::max({ 0.0, -Y, Y - Fd.Depth() });
				return std::sqrt(Dx * Dx + Dy * Dy);
			}
			/** The hills beyond the plain, 0 to 1 of their height, a kilometre or more out (on some fields: the plain is
			 * flat to the sky as often as not). */
			double HillAt(double X, double Y) const
			{
				if (!bHills) return 0.0;
				const double H = std::max(0.0, Fbm(X, Y, 1500.0, 3, Seed ^ 0xA51) + 0.05);
				return Smooth(300.0, 1100.0, Outside(X, Y)) * Smooth(0.0, 0.5, H) * (0.6 + 0.4 * Fbm(X, Y, 500.0, 2, Seed ^ 0xA52));
			}
			double Rolling(double X, double Y, double InMeanZ) const override
			{
				// (the North China plain and India's as level as a table; Manchuria's long swells)
				const double Swell = bManchuria ? 9.0 * Fbm(X, Y, 2600.0, 3, Seed ^ 0xB1) + 2.5 * Fbm(X, Y, 700.0, 2, Seed ^ 0xB2) : 2.5 * Fbm(X, Y, 2600.0, 2, Seed ^ 0xB1);
				return InMeanZ + Swell + (bIndia ? 30.0 : bManchuria ? 50.0 : 80.0) * HillAt(X, Y);
			}
			double LaneDepth(double X, double Y) const override
			{
				// (the loess's cart tracks worn down into it, half a metre and more (the bocage's sunk lanes are the bocage's); Manchuria's black earth rutted, India's dust)
				if (bIndia) return 0.15;
				if (bManchuria) return 0.3;
				return 0.3 + 0.5 * Clamp01(0.5 + Fbm(X, Y, 400.0, 2, Seed ^ 0x5E7));
			}
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 2.6 : 2.0) + 0.6 * U; }
			double PlotMin() const override { return 12000.0; }
			double PlotMax() const override { return 30000.0; }
			double FarTilled() const override { return 0.8; }
			double WoodsEdge() const override { return 0.3; }

			int CropOf(int Kind, double U) const override
			{
				if (Kind == KRough) return U < (bIndia ? 0.3 : 0.55) ? Terrace : Hill;
				if (Kind == KOpen)
				{
					if (U < 0.35) return Grass;
					U = (U - 0.35) / 0.65;
				}
				if (bIndia)
				{
					const int Kh = U < 0.4 ? 0 : U < 0.75 ? 1 : 2;
					const double V = std::fmod(U * 7.31, 1.0);
					const int Rb = V < 0.55 ? 0 : V < 0.75 ? 1 : 2;
					return Pair0 + 3 * Kh + Rb;
				}
				if (bManchuria) return U < 0.32 ? Soy : U < 0.60 ? Gao : U < 0.76 ? Millet : U < 0.92 ? Wheat : Fallow;
				return U < 0.36 ? WheatSoy : U < 0.58 ? Gao : U < 0.76 ? Millet : U < 0.94 ? Soy : Fallow;
			}
			const char* Word(int C) const
			{
				switch (C)
				{
				case Gao: return bIndia ? "jowar" : "gaoliang";
				case Millet: return bIndia ? "bajra" : "millet";
				case Wheat: return "wheat";
				case Soy: return bIndia ? "pulses" : "soybeans";
				case Mustard: return "mustard";
				case Fallow: return "fallow";
				case Grass: return "grass";
				case Terrace: return "loess terraces";
				case Hill: return "hill";
				default: return "";
				}
			}
			const char* CropWord(int C) const override
			{
				if (C == WheatSoy || C >= Pair0)
				{
					const Life L = ParcelLife(C, T0());
					return L.bUp ? Word(L.Crop) : L.Since < 1.5 ? "stubble" : "fallow";
				}
				return Word(C);
			}
			bool IsCorn(int C) const override
			{
				// (the gaoliang over a man's head, the millet to his chest: a tank lays them flat)
				if (C < 0) return false;
				const Life L = ParcelLife(C, T0());
				return L.bUp && ((L.Crop == Gao && L.High > 0.5) || (L.Crop == Millet && L.High > 0.75));
			}

			// ---------------------------------------------------------------- the paint

			Tone Soil() const { return bManchuria ? BlackEarth : bIndia ? Alluvium : Loess; }
			Tone GrassAt(const Spot& S, const Tone& Base) const
			{
				Tone T = Mixed(Mixed(Base, Rgb(158, 144, 104), 1.0 - Green), Rgb(140, 130, 92), 0.15 * Smooth(0.0, 0.6, S.N1));
				T = Mixed(T, Base, 0.2 * Smooth(0.0, -0.6, S.N1) * Green);
				return Varied(T, 0.96 + 0.06 * S.N2 + 0.03 * S.Broad, 0.0);
			}
			Tone CanopyOf(const Life& L) const
			{
				switch (L.Crop)
				{
				case Gao: return Mixed(GaoGreen, GaoRipe, L.Ripe);
				case Millet: return Mixed(MilletGreen, MilletRipe, L.Ripe);
				case Wheat: return Mixed(WheatGreen, WheatGold, Smooth(0.0, 0.8, L.Ripe));
				case Soy: return Mixed(SoyGreen, SoyRipe, L.Ripe);
				case Mustard: return Mixed(Mixed(Rgb(96, 122, 60), Rgb(150, 136, 84), Smooth(0.6, 1.0, L.Ripe)), MustardFlower, Flower(L));
				default: return Soil();
				}
			}
			static double Flower(const Life& L) { return L.Crop == Mustard && L.bUp ? Smooth(0.0, 0.15, L.Ripe) * Smooth(0.8, 0.55, L.Ripe) : 0.0; }
			/** A crop's state's colour over its floor: the crop over the earth by its cover; the stubble; the ploughed earth. */
			Tone StateColour(const Life& L, const Tone& Floor) const
			{
				if (L.bUp)
				{
					double Cover = (L.Crop == Soy ? 0.75 : 0.95) * Smooth(0.05, 0.6, L.High); // (the soybeans' rows show the earth between)
					if (L.Crop == Wheat) Cover = std::max(Cover, 0.22); // (the winter wheat's rows a green haze on the loess)
					return Mixed(Floor, CanopyOf(L), Cover);
				}
				if (L.Since < 1.3 && (L.Crop == Gao || L.Crop == Millet || L.Crop == Wheat)) return Mixed(Floor, L.Crop == Gao ? GaoStubbleC : StrawC, L.Crop == Gao ? 0.5 : 0.6);
				// (ploughed: the furrows' shade, drying paler)
				return Varied(Mixed(Floor, Mixed(Furrows, Soil(), bManchuria ? 0.6 : 0.3), 0.5), L.Since < 2.5 ? 0.9 : 1.0, 0.0);
			}
			Tone FloorAt(const Spot& S) const
			{
				// (India's parched months paler; the loess's own mottle)
				return Varied(Soil(), (0.95 + 0.06 * S.N2) * (bIndia && Green < 0.2 ? 1.05 : 1.0), 0.0);
			}
			Tone FieldColour(const Spot& S) const
			{
				const Strip St = StripOf(S);
				const Life L = ParcelLife(StripCrop(S, St), T0() + St.Jit);
				Tone T = Varied(StateColour(L, FloorAt(S)), St.Shade, 0.0);
				// (the balks between the strips and round the field: a line of grass, beaten earth in the dry months)
				return Mixed(T, Mixed(GrassAt(S, OpenGrass), Soil(), 0.35), 0.45 * Smooth(0.9, 0.2, St.Balk) + 0.5 * Smooth(1.2, 0.0, S.Edge));
			}

			/** The rules' heights at a place (their cells' middles, between them straight), for the terraces' contours. */
			double RulesZ(double X, double Y) const
			{
				if (Fd.Cols < 1 || Fd.Rows < 1 || Fd.Height.size() < static_cast<size_t>(Fd.Cols) * static_cast<size_t>(Fd.Rows)) return MeanZ;
				const double U = std::clamp(X / Fd.Cell - 0.5, 0.0, Fd.Cols - 1.0), V = std::clamp(Y / Fd.Cell - 0.5, 0.0, Fd.Rows - 1.0);
				const int I = std::min(static_cast<int>(U), std::max(0, Fd.Cols - 2)), J = std::min(static_cast<int>(V), std::max(0, Fd.Rows - 2));
				const int I1 = std::min(I + 1, Fd.Cols - 1), J1 = std::min(J + 1, Fd.Rows - 1);
				const double FU = U - I, FV = V - J;
				auto H = [this](int A, int B) { return Fd.Height[static_cast<size_t>(B) * static_cast<size_t>(Fd.Cols) + static_cast<size_t>(A)]; };
				const double Lo = H(I, J) + (H(I1, J) - H(I, J)) * FU, Hi = H(I, J1) + (H(I1, J1) - H(I, J1)) * FU;
				return Lo + (Hi - Lo) * FV;
			}
			/** On the loess terraces, metres to the nearest riser (the steps 2 to 4 m high along the contours) and the
			 * tread's key. */
			double Riser(const Spot& S, uint64_t& OutTread) const
			{
				const double Step = 2.0 + 2.0 * Unit(Hash(S.Key, 0x7E77));
				const double T = (RulesZ(S.X, S.Y) + 0.25 * Fbm(S.X, S.Y, 40.0, 1, Seed ^ 0x7E78)) / Step;
				const double F = T - std::floor(T);
				OutTread = Hash(S.Key, static_cast<uint64_t>(std::floor(T) + 4096.0));
				return 7.0 * std::min(F, 1.0 - F);
			}

			Tone Open(const Spot& S) const
			{
				switch (S.Crop)
				{
				case Hill:
				{
					// thin grass over the loess and the scree, the bare earth showing in its patches
					Tone T = GrassAt(S, HillGrass);
					T = Mixed(T, Varied(Scree, 0.96 + 0.08 * S.N2, 0.0), 0.35 + 0.3 * Smooth(0.0, 0.7, S.N1));
					return Mixed(T, Varied(Soil(), 1.02, 0.0), 0.2 * Smooth(0.3, 0.8, S.N2));
				}
				case Terrace:
				{
					// the treads tilled (millet), the risers a bank of bare loess with grass along its lip
					uint64_t Tread = 0;
					const double R = Riser(S, Tread);
					const Life L = LifeOf(Millet, T0() + 0.9 * (Unit(Hash(Tread, 0x717)) - 0.5));
					const Tone T = Varied(StateColour(L, FloorAt(S)), 0.92 + 0.16 * Unit(Hash(Tread, 0x5AD)), 0.0);
					return Mixed(T, Mixed(Varied(Loess, 1.06, 0.0), GrassAt(S, HillGrass), 0.35), Smooth(1.4, 0.4, R));
				}
				case Grass:
				{
					// grazed thin, worn to the earth along the paths and round the graves
					const Tone T = GrassAt(S, OpenGrass);
					return Mixed(T, Varied(Soil(), 1.02, 0.0), 0.25 + 0.2 * Smooth(0.3, 0.9, S.N2) + 0.1 * Smooth(0.5, 0.9, S.N1));
				}
				default: return FieldColour(S);
				}
			}

			Tone Colour(const Spot& S) const override
			{
				switch (S.Kind)
				{
				case KLane:
				{
					// a cart track: its floor beaten to dust, paler than the fields, the wheels' two ruts darker; its banks
					// bare earth where it runs sunk in the loess, the verge's grass above
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double Rut = std::exp(-std::pow((Off - 0.8) / 0.25, 2.0));
					Tone T = Varied(Soil(), (bManchuria ? 1.1 : 1.07) + 0.05 * S.N2, 0.0);
					T = Mixed(T, Varied(Soil(), 0.84, 0.0), 0.45 * Rut);
					return Mixed(T, GrassAt(S, OpenGrass), 0.8 * Smooth(S.LaneHalf + 0.8, S.LaneHalf + 2.4, Off));
				}
				case KVillage: return Mixed(Varied(Soil(), 1.05, 0.0), GrassAt(S, OpenGrass), 0.12 + 0.25 * Smooth(-0.2, 0.6, S.N1) * Green); // (the village's beaten earth, a little grass by the walls)
				case KYard: return Mixed(Varied(Soil(), 1.0, 0.0), StrawC, 0.12 + 0.12 * Smooth(0.0, 0.6, S.N2)); // (the threshing floor's earth and its straw)
				case KWoods: return Mixed(GrassAt(S, HillGrass), Varied(Soil(), 0.92, 0.0), 0.3 + 0.2 * S.N1);
				case KWater: return YellowWater;
				case KMarsh: return Mixed(GrassAt(S, Rgb(98, 112, 64)), YellowWater, 0.25 * Smooth(0.0, 0.7, S.N1));
				case KWorks: return Mixed(GrassAt(S, OpenGrass), Varied(Soil(), 0.85, 0.0), 0.6);
				default: break;
				}
				return Open(S);
			}

			Tone Canopy(const Spot& S) const
			{
				// (the poplars' and the willows' crowns by the season: bare grey twigs in the north's winter, yellow in
				// October; India's green but dusty in the dry months)
				Tone T;
				if (bIndia) T = Green < 0.2 ? Rgb(96, 100, 64) : Rgb(70, 88, 50);
				else T = (Month >= 11 || Month <= 3) ? Rgb(118, 108, 94) : Month == 10 ? Rgb(150, 136, 72) : Month == 4 ? Rgb(104, 122, 70) : Rgb(76, 94, 54);
				return Varied(T, 0.95 + 0.1 * S.Broad, 0.0);
			}

			Tone Far(const Spot& S, bool bWoods, double /*Gap*/) const override
			{
				// (out in the ring a parcel is its crop's state, read from its seed; the hills beyond bare loess with thin
				// grass)
				Tone T;
				if (bWoods) T = Canopy(S);
				else if (S.Kind == KField || S.Kind == KOpen || S.Kind == KRough)
				{
					Spot Q = S;
					if (Q.Crop < 0) Q.Crop = CropOf(S.Kind, Unit(Hash(S.Key, 0xFA2)));
					if (Q.Crop == Grass || Q.Crop == Hill || Q.Crop == Terrace) T = Open(Q);
					else T = Varied(StateColour(ParcelLife(Q.Crop, T0() + 0.6 * (Unit(Hash(S.Key, 0x718)) - 0.5)), FloorAt(S)), 0.94 + 0.12 * Unit(Hash(S.Key, 0x5AE)), 0.0);
				}
				else T = Colour(S);
				const double Hl = HillAt(S.X, S.Y);
				if (Hl > 0.05)
				{
					const Tone Hill_ = Varied(Mixed(Rgb(156, 134, 100), HillGrass, (0.15 + 0.3 * Green) * Smooth(0.2, 0.8, Fbm(S.X, S.Y, 300.0, 2, Seed ^ 0xA53))), 0.95 + 0.1 * S.Broad, 0.0);
					T = Mixed(T, Hill_, Smooth(0.08, 0.3, Hl));
				}
				return T;
			}

			// ---------------------------------------------------------------- round the eye

			Grown Grow(const Spot& S, double Pick) const override
			{
				if (S.Kind == KWater || S.Drawn == 9 /*floor*/ || S.Drawn == 10 /*wall*/) return Grown{};
				const bool bSnow = SnowLying > 0.25;
				if (S.Kind == KLane) return S.Lane > 0.8 ? Grown{ ClutOf(CGrass), bSnow ? 0.2 : 0.3 + 0.4 * Green } : Grown{};
				if (S.Kind == KVillage) return Pick < 0.3 ? Grown{ ClutOf(CGrass), bSnow ? 0.1 : 0.4 } : Grown{};
				if (S.Kind == KYard) return Pick < 0.25 ? Grown{ ClutOf(CStubble), 0.15 } : Grown{};
				if (S.Kind == KWorks) return Grown{ ClutOf(CGrass), 0.15 };
				if (S.Kind == KMarsh) return Pick < 0.6 ? Grown{ ClutOf(CReed), bSnow ? 0.4 : 0.7 } : Grown{ ClutOf(CGrass), bSnow ? 0.3 : 1.0 };
				if (S.Kind == KWoods) return Grown{ ClutOf(CGrass), bSnow ? 0.2 : 0.6 };
				switch (S.Crop)
				{
				case Grass: return Pick < 0.9 ? Grown{ ClutOf(CGrass), bSnow ? 0.25 : bIndia && Green < 0.2 ? 0.7 : 1.2 } : Grown{ ClutOf(CBush), bSnow ? 0.0 : 0.2 };
				case Hill: return Pick < 0.85 ? Grown{ ClutOf(CGrass), bSnow ? 0.25 : 0.9 } : Grown{ ClutOf(CBush), bSnow ? 0.1 : 0.3 };
				case Terrace:
				{
					uint64_t Tread = 0;
					if (Riser(S, Tread) < 1.2) return Grown{ ClutOf(CGrass), bSnow ? 0.2 : 0.8 };
					break;
				}
				default: break;
				}
				if (!Tilled(S.Crop)) return Pick < 0.5 ? Grown{ ClutOf(CGrass), bSnow ? 0.2 : 0.8 } : Grown{};
				const Strip St = StripOf(S);
				if (S.Crop != Terrace && (St.Balk < 0.35 || S.Edge < 0.5)) return Grown{ ClutOf(CGrass), bSnow ? 0.3 : 0.9 };
				const Life L = S.Crop == Terrace ? ParcelLife(Terrace, T0()) : ParcelLife(StripCrop(S, St), T0() + St.Jit);
				if (bSnow)
				{
					// under the snow: the gaoliang's stubs through it, the rest white
					return !L.bUp && L.Crop == Gao && L.Since < 5.0 ? Grown{ ClutOf(CStubble), 0.8 } : Grown{};
				}
				if (L.bUp)
				{
					switch (L.Crop)
					{
					case Gao: return L.High < 0.4 ? Grown{ ClutOf(CBush), 0.9 } : Grown{ ClutOf(CGao), 0.55 };
					case Millet: return L.High < 0.3 ? Grown{ ClutOf(CStubble), 0.9 } : Grown{ ClutOf(CGrain), 1.0 };
					case Wheat: return L.High < 0.35 ? Grown{ ClutOf(CStubble), 0.8 } : Grown{ ClutOf(CGrain), 1.1 };
					default: return Grown{ ClutOf(CBush), 1.1 }; // (soybeans, pulses, the mustard)
					}
				}
				if (L.Since < 1.3 && (L.Crop == Gao || L.Crop == Millet || L.Crop == Wheat)) return Grown{ ClutOf(CStubble), 0.95 };
				if (L.Crop == Gao && L.Since < 3.5) return Grown{ ClutOf(CStubble), 0.5 }; // (the gaoliang's stubs stand into the winter)
				return Pick < 0.08 ? Grown{ ClutOf(CStubble), 0.15 } : Grown{};
			}

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				const int K = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				const bool bField = (S.Kind == KField || S.Kind == KOpen || S.Kind == KRough) && Tilled(S.Crop);
				Life L;
				if (bField)
				{
					const Strip St = StripOf(S);
					L = S.Crop == Terrace ? ParcelLife(Terrace, T0()) : ParcelLife(StripCrop(S, St), T0() + St.Jit);
				}
				const double Lt = 0.94 + 0.12 * U6;
				const float Rows = static_cast<float>(S.Angle + (U7 < 0.5 ? 0.0 : Pi));
				switch (K)
				{
				case CGao:
					// (the rows along the strip; the leaves drying as the heads ripen; the tint on the linear colour)
					P.Yaw = Rows;
					P.Scale = 1.0f;
					P.ScaleZ = static_cast<float>(std::max(0.4, L.High) * (0.92 + 0.16 * U5));
					P.Tint = Tint(Lerp(1.0, 1.3, L.Ripe) * Lt, Lerp(1.0, 1.08, L.Ripe) * Lt, Lerp(1.0, 1.0, L.Ripe) * Lt);
					return true;
				case CGrain:
				{
					// (built in the ripe straw's colours, the tint greening it; the millet a head taller and tawnier)
					P.Yaw = Rows;
					P.Scale = static_cast<float>(0.85 + 0.3 * U4);
					P.ScaleZ = static_cast<float>(std::max(0.25, L.High) * (0.9 + 0.2 * U5) * (L.Crop == Millet ? 1.25 : 1.0));
					const double R = L.Ripe;
					if (L.Crop == Millet) P.Tint = Tint(Lerp(0.26, 1.0, R) * Lt, Lerp(0.6, 0.93, R) * Lt, Lerp(0.42, 0.78, R) * Lt);
					else P.Tint = Tint(Lerp(0.26, 1.0, R) * Lt, Lerp(0.6, 0.94, R) * Lt, Lerp(0.42, 0.8, R) * Lt); // (ripe a warm gold, not cream)
					return true;
				}
				case CBush:
					P.Scale = static_cast<float>(0.8 + 0.4 * U4);
					if (!bField)
					{
						// (the hills' and the open ground's scrub and weeds)
						P.ScaleZ = static_cast<float>(0.7 + 0.5 * U5);
						P.Tint = Green > 0.5 ? Tint(0.85 * Lt, 1.0 * Lt, 0.8 * Lt) : Tint(1.3 * Lt, 1.12 * Lt, 0.82 * Lt);
					}
					else if (L.Crop == Mustard)
					{
						P.ScaleZ = static_cast<float>(std::max(0.3, L.High) * (2.2 + 0.6 * U5));
						P.Tint = Flower(L) > 0.3 ? Tint(1.99, 1.6, 0.4) : Tint(Lerp(0.8, 1.5, Smooth(0.6, 1.0, L.Ripe)) * Lt, Lerp(1.05, 1.25, Smooth(0.6, 1.0, L.Ripe)) * Lt, 0.7 * Lt);
					}
					else if (L.Crop == Soy)
					{
						// (the soybeans' leaves yellowing as the pods ripen)
						P.Yaw = Rows;
						P.ScaleZ = static_cast<float>(std::max(0.3, L.High) * (1.1 + 0.3 * U5));
						P.Tint = Tint(Lerp(0.72, 1.5, L.Ripe) * Lt, Lerp(0.98, 1.05, L.Ripe) * Lt, Lerp(0.76, 0.7, L.Ripe) * Lt);
					}
					else
					{
						// (the young gaoliang and millet, leafy, knee high)
						P.Yaw = Rows;
						P.ScaleZ = static_cast<float>(0.5 + 2.2 * L.High);
						P.Tint = Tint(0.72 * Lt, 1.04 * Lt, 0.76 * Lt);
					}
					return true;
				case CStubble:
					P.Yaw = Rows;
					P.Scale = 1.0f;
					if (bField && L.bUp)
					{
						// (the winter wheat and the young millet a hand high in their rows)
						P.ScaleZ = static_cast<float>(0.9 + 0.5 * U5);
						const bool bDormant = Month == 12 || Month <= 2; // (the winter wheat dull grey-green through the north's dry winter)
						P.Tint = bDormant ? Tint(0.86, 0.98 + 0.06 * U6, 0.74) : Tint(0.6, 1.2 + 0.08 * U6, 0.6);
					}
					else if (bField && L.Crop == Gao)
					{
						// (the gaoliang's stubs a foot high and thick, grey-buff)
						P.ScaleZ = static_cast<float>(2.0 + 0.6 * U5);
						P.Tint = Tint(1.0 * Lt, 0.94 * Lt, 0.86 * Lt);
					}
					else
					{
						P.ScaleZ = static_cast<float>(0.8 + 0.4 * U5);
						P.Tint = SnowLying > 0.25 ? Tint(0.95 + 0.06 * U6, 0.92 + 0.05 * U6, 0.9) : Tint(1.0 + 0.06 * U6, 1.0, 0.95);
					}
					return true;
				case CGrass:
					P.Scale = static_cast<float>(0.6 + 0.7 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.7 * U5);
					P.Tint = Tint(0.96 + 0.08 * U7, 0.98 + 0.04 * U6, 0.94); // (the season's colour built in)
					return true;
				case CReed:
					P.Scale = static_cast<float>(0.8 + 0.5 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					P.Tint = Green > 0.5 ? Tint(0.96, 1.0, 0.94) : Tint(1.12, 1.02, 0.86);
					return true;
				default: return false;
				}
			}

			/** How ripe the gaoliang's heads are this month (most fields'): green to red-brown. */
			double HeadRipe() const
			{
				const Life L = LifeOf(Gao, T0());
				return L.bUp ? L.Ripe : 1.0;
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				switch (K)
				{
				case CGao:
				{
					// two rows 0.6 m apart along x, two plants to a row (1.2 m of row): each a stalk 2.4 to 2.9 m, five long
					// narrow leaves arching out from it in turn and drooping, a dense head on top: green in July, red-brown
					// as it ripens (the month's), the leaves green (the field's tint dries them)
					const double Rp = HeadRipe();
					// (India's jowar ripens a pale buff, the north's gaoliang a dull red-brown)
					const Tone HeadTop = Mixed(Rgb(126, 134, 72), bIndia ? Rgb(170, 148, 102) : Rgb(102, 64, 48), Rp), HeadFoot = Mixed(Rgb(98, 112, 56), bIndia ? Rgb(132, 112, 78) : Rgb(74, 48, 38), Rp);
					for (int R = 0; R < 2; ++R)
						for (int Pl = 0; Pl < 2; ++Pl)
						{
							const uint64_t Hs = Hash(0x6A0B, static_cast<uint64_t>(R * 2 + Pl));
							const double X = -0.3 + 0.6 * Pl + 0.12 * (Unit(Hash(Hs, 1)) - 0.5), Y = -0.3 + 0.6 * R + 0.05 * (Unit(Hash(Hs, 2)) - 0.5);
							const double Tall = 2.4 + 0.5 * Unit(Hash(Hs, 3));
							W.Blade(X, Y, 0.0, X + 0.01, Y, Tall, 0.04, Rgb(92, 102, 54), Rgb(118, 122, 68));
							W.Blade(X, Y, 0.0, X, Y + 0.01, Tall, 0.04, Rgb(92, 102, 54), Rgb(118, 122, 68));
							const double A0 = 2.0 * Pi * Unit(Hash(Hs, 4));
							for (int L = 0; L < 5; ++L)
							{
								const double Z = 0.35 + (Tall - 0.8) * L / 4.5;
								const double A = A0 + (L % 2 ? Pi : 0.0) + 0.6 * (Unit(Hash(Hs, 10 + L)) - 0.5);
								const double Out = (0.6 + 0.25 * Unit(Hash(Hs, 20 + L))) * (1.0 - 0.25 * L / 5.0);
								const double Ca = std::cos(A), Sa = std::sin(A), Droop = 0.3 + 0.25 * Unit(Hash(Hs, 30 + L));
								const double P[4][3] = { { X, Y, Z }, { X + 0.3 * Out * Ca, Y + 0.3 * Out * Sa, Z + 0.22 }, { X + 0.68 * Out * Ca, Y + 0.68 * Out * Sa, Z + 0.24 }, { X + Out * Ca, Y + Out * Sa, Z + 0.24 - Droop } };
								const double Wd[3] = { 0.04, 0.065, 0.045 };
								Ribbon(W, P, Wd, Rgb(56, 80, 40), Rgb(92, 114, 56));
							}
							// (the head: a spindle of grain some 0.3 m long, four crossed blades round its stem,
							// nodding a little, as the gaoliang's heavy heads do)
							const double Nod = 0.06 + 0.06 * Unit(Hash(Hs, 40)), NA = 2.0 * Pi * Unit(Hash(Hs, 41));
							const double HX = X + Nod * std::cos(NA), HY = Y + Nod * std::sin(NA);
							for (int B = 0; B < 4; ++B)
							{
								const double A = Pi * B / 4.0 + NA, Ca = 0.045 * std::cos(A), Sa = 0.045 * std::sin(A);
								W.Blade(X + Ca * 0.2, Y + Sa * 0.2, Tall - 0.04, HX + 0.002 * Ca, HY + 0.002 * Sa, Tall + 0.3, 0.1, HeadFoot, HeadTop);
							}
						}
					M.Paint = 2;
					M.FarFrom = 32.0f;
					// far off: each row's leaves as pointed fans, the heads standing over them
					M.Far = LandField::Mesh();
					Writer F(M.Far);
					for (int R = 0; R < 2; ++R)
					{
						const double Y = -0.3 + 0.6 * R;
						for (int Sg = 0; Sg < 2; ++Sg)
						{
							for (const double Sd : { -1.0, 1.0 })
							{
								const uint64_t Hs = Hash(0x6AF5, static_cast<uint64_t>(R * 4 + Sg * 2 + (Sd > 0.0 ? 1 : 0)));
								const double XC = -0.3 + 0.6 * Sg + 0.15 * (Unit(Hash(Hs, 1)) - 0.5), XT = XC + 0.4 * (Unit(Hash(Hs, 3)) - 0.5);
								const double Top = 2.2 + 0.2 * Unit(Hash(Hs, 2)), Low = 1.4 + 0.2 * Unit(Hash(Hs, 5));
								F.Blade(XC, Y, Top, XT, Y + Sd * (0.4 + 0.1 * Unit(Hash(Hs, 4))), Low, 0.9, Rgb(80, 104, 54), Rgb(56, 80, 42));
							}
							const double XH = -0.3 + 0.6 * Sg;
							F.Blade(XH, Y, 2.3, XH + 0.02, Y, 2.95, 0.14, HeadFoot, HeadTop);
						}
					}
					break;
				}
				case CGrain:
					// a clump of wheat (or millet by its tint and height): twenty-four stalks a metre tall over half a
					// metre, each its ear a slim blade on its top, a leaf at every third; built in the ripe straw's colours
					for (int B = 0; B < 24; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0x6E41, B)), D = 0.26 * std::sqrt(Unit(Hash(0x6E42, B)));
						const double X = D * std::cos(A), Y = D * std::sin(A), High = 0.85 + 0.2 * Unit(Hash(0x6E43, B));
						const double LX = 0.07 * std::cos(A), LY = 0.07 * std::sin(A);
						W.Blade(X, Y, 0.0, X + LX, Y + LY, High, 0.018, Rgb(150, 128, 80), Rgb(178, 152, 96));
						W.Blade(X + 0.9 * LX, Y + 0.9 * LY, High - 0.04, X + 1.3 * LX, Y + 1.3 * LY, High + 0.1, 0.03, Rgb(184, 158, 94), Rgb(196, 170, 104));
						if (B % 3 == 0) W.Blade(X, Y, 0.0, X + 2.5 * LX - 0.04, Y + 2.5 * LY + 0.04, 0.35, 0.025, Rgb(146, 126, 80), Rgb(170, 146, 94));
					}
					M.Paint = 2;
					M.FarFrom = 45.0f;
					break;
				case CBush:
					// a leafy clump over some 0.4 m: two dozen broad leaves (the soybeans', the mustard's, the young
					// gaoliang's by its tint and height), pods or flowers standing over them
					// (broad leaves on their stalks, arching out and up in three tiers, not grass blades)
					for (int B = 0; B < 14; ++B)
					{
						const double A = 2.0 * Pi * (B + 0.5 * Unit(Hash(0xB05B, B))) / 7.0, Tier = static_cast<double>(B % 3);
						const double Z = 0.1 + 0.11 * Tier, Out = 0.14 + 0.08 * Unit(Hash(0xB05C, B)) - 0.03 * Tier;
						const double FX = 0.04 * std::cos(A), FY = 0.04 * std::sin(A);
						W.Blade(0.0, 0.0, 0.0, FX, FY, Z, 0.012, Rgb(76, 96, 48), Rgb(92, 112, 56));
						W.Blade(FX, FY, Z, FX + Out * std::cos(A), FY + Out * std::sin(A), Z + 0.1 + 0.06 * Unit(Hash(0xB05D, B)), 0.1, Rgb(70, 98, 46), Rgb(116, 142, 70));
					}
					for (int B = 0; B < 5; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0xB05C, B + 20)), D = 0.1 * Unit(Hash(0xB05D, B + 20));
						W.Bead(D * std::cos(A), D * std::sin(A), 0.3 + 0.1 * Unit(Hash(0xB05E, B)), 0.03, 0.06, Rgb(140, 132, 70), Rgb(108, 104, 60)); // (pods, or the mustard's flowers by the tint)
					}
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 40.0f;
					break;
				case CStubble:
					// the cut rows: short straw stubs in rows 0.3 m apart along x, a little loose straw lying between (the
					// gaoliang's by its height, the winter wheat's green by its tint)
					for (int R = 0; R < 4; ++R)
						for (int H = 0; H < 4; ++H)
						{
							const uint64_t Hs = Hash(0x57BB, static_cast<uint64_t>(R * 4 + H));
							const double X = -0.45 + 0.3 * H + 0.05 * (Unit(Hash(Hs, 1)) - 0.5), Y = -0.45 + 0.3 * R + 0.03 * (Unit(Hash(Hs, 2)) - 0.5);
							Tuft(W, X, Y, 4, 0.025, 0.1, 0.06, 0.03, 0.016, Hs, Rgb(128, 112, 78), Rgb(178, 160, 112));
						}
					for (int B = 0; B < 5; ++B)
					{
						const double X = 1.0 * (Unit(Hash(0x57BC, B)) - 0.5), Y = 1.0 * (Unit(Hash(0x57BD, B)) - 0.5), A = 2.0 * Pi * Unit(Hash(0x57BE, B));
						W.Blade(X, Y, 0.01, X + 0.3 * std::cos(A), Y + 0.3 * std::sin(A), 0.03, 0.02, Rgb(170, 150, 104), Rgb(186, 168, 120));
					}
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 35.0f;
					break;
				case CGrass:
				{
					// a clump of thin sward: thirty-odd blades over some 0.6 m, a hand or two high; the season's colour (the
					// north's winter and India's parched months straw-buff)
					const Tone Foot = Mixed(Rgb(122, 108, 78), Rgb(88, 100, 56), Green), Tip = Mixed(Rgb(184, 166, 120), Rgb(146, 154, 92), Green);
					Tuft(W, 0.0, 0.0, 30, 0.3, 0.08, 0.16, 0.08, 0.016, 0x6A55, Foot, Tip);
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 45.0f;
					break;
				}
				case CReed:
					Tuft(W, 0.0, 0.0, 28, 0.22, 1.2, 0.8, 0.16, 0.02, 0x2EED, Rgb(86, 100, 58), Rgb(170, 160, 108));
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 60.0f;
					break;
				default: break;
				}
			}

			// ---------------------------------------------------------------- the kit

			const char* Folder() const override { return "Asia"; }
			const char* KitName(int K) const override
			{
				switch (K)
				{
				case KPoplar: return "Poplar";
				case KWillow: return "Willow";
				case KPine: return "HillPine";
				case KHouse: return "NorthHouse";
				case KFarm: return "Farmyard";
				case KTower: return "TieredTower";
				case KGrave: return "GraveMound";
				case KStack: return "StrawStack";
				case KBoulder: return "DryBoulder";
				case KOutcrop: return "DryOutcrop";
				default: return nullptr;
				}
			}
			void KitLook(int K, LandField::BaseMesh& M) const override
			{
				auto Sight = [&M](float Mid, float Half, float Across, float Trunk) {
					M.bSight = true;
					M.SightMid = Mid;
					M.SightHalf = Half;
					M.SightAcross = Across;
					M.SightTrunk = Trunk;
				};
				switch (K)
				{
				case KPoplar:
					// (its near mesh from 60 m, not 90: the plain's poplars stand in their hundreds along the tracks)
					M.bTree = true;
					M.Paint = 1;
					M.FarFrom = 60.0f;
					Sight(0.6f, 0.4f, 0.5f, 0.3f);
					break;
				case KWillow:
					M.bTree = true;
					M.Paint = 1;
					M.FarFrom = 60.0f;
					Sight(0.6f, 0.4f, 0.8f, 0.3f);
					break;
				case KPine:
					M.bTree = true;
					M.Paint = 1;
					M.FarFrom = 90.0f;
					Sight(0.72f, 0.28f, 0.7f, 0.35f);
					break;
				case KHouse:
				case KFarm:
					M.FarFrom = 200.0f;
					Sight(0.5f, 0.5f, 0.85f, 0.0f);
					break;
				case KTower:
					M.FarFrom = 400.0f;
					Sight(0.5f, 0.5f, 0.6f, 0.0f);
					break;
				case KGrave:
				case KStack:
					M.FarFrom = 120.0f;
					Sight(0.5f, 0.5f, 0.85f, 0.0f);
					break;
				case KOutcrop:
					M.FarFrom = 250.0f;
					Sight(0.5f, 0.5f, 0.8f, 0.0f);
					break;
				case KBoulder:
					M.FarFrom = 150.0f;
					Sight(0.5f, 0.5f, 0.85f, 0.0f);
					break;
				default: M.FarFrom = 90.0f; break;
				}
			}

			// ---------------------------------------------------------------- what stands on it

			void Sites(Host& H) override
			{
				double TX = 0.0, TY = 0.0, TR = 0.0;
				if (!H.Town(TX, TY, TR)) return;
				Rng R(H.Seed() ^ 0x9A1A);
				int Built = 0, Yards = 0;
				// The village: China's and Manchuria's houses in rows, every one facing south (the village turned a little
				// as its ground lies), a lane's width between the rows; India's every way, close-packed.
				const double Turn = R.Range(-0.15, 0.15), Ct = std::cos(Turn), St = std::sin(Turn);
				const double Reach = 0.85 * TR;
				for (double V = -Reach; V <= Reach; V += 14.5)
					for (double U = -Reach; U <= Reach; U += 12.4)
					{
						const double JU = R.Range(-1.0, 1.0), JV = R.Range(-0.8, 0.8), Gap = R.Next(), Yaw = R.Range(-0.05, 0.05), Twist = R.Range(-0.5, 0.5), Quarter = R.Next();
						const double PU = U + JU, PV = V + JV;
						// (the village's edge ragged, its houses thinning out)
						const double Out = std::hypot(PU, PV) / Reach;
						if (Out > 1.0 || Gap < 0.12 + 0.55 * Smooth(0.55, 1.0, Out)) continue;
						const double X = TX + PU * Ct - PV * St, Y = TY + PU * St + PV * Ct;
						const double HY = bIndia ? Turn + Twist + (Quarter < 0.5 ? 0.5 * Pi : 0.0) : Turn + Yaw;
						if (H.House(KitOf(KHouse), X, Y, HY, 10.4, 7.2)) ++Built;
					}
				// The walled farmyards at its edge (where the rules lay farmyards, first).
				for (int K = 0; K < 240 && Yards < 4; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(0.6 * TR, TR + 160.0), Yaw = R.Range(-0.08, 0.08);
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A);
					const bool bYard = H.At(X, Y).Kind == KYard;
					if ((bYard || (K >= 160 && D < TR + 60.0)) && H.House(KitOf(KFarm), X, Y, Turn + Yaw, 21.4, 16.0)) ++Yards;
				}
				// A pagoda on the highest free ground near the town, in China one town in two.
				const double Tower = R.Next();
				if (!bIndia && !bManchuria && Tower < 0.5)
				{
					double BestX = 0.0, BestY = 0.0, BestZ = -1e9;
					for (int K = 0; K < 60; ++K)
					{
						const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(TR, TR + 220.0);
						const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A);
						if (!H.Free(X, Y, 8.0)) continue;
						const double Z = H.Land(X, Y);
						if (Z > BestZ)
						{
							BestZ = Z;
							BestX = X;
							BestY = Y;
						}
					}
					if (BestZ > -1e8 && H.House(KitOf(KTower), BestX, BestY, Turn, 11.0, 11.0)) ++Built;
				}
				// The town's wall, China's and Manchuria's (two towns in three): a square round the houses, its sides along
				// their rows (laid in Walls, after the houses).
				const double Walled = R.Next();
				bTownWall = !bIndia && Walled < 0.67;
				WallX = TX;
				WallY = TY;
				WallHalf = Reach + 14.0;
				WallTurn = Turn;
				H.Tally(Built + Yards, 0, Yards);
			}

			void Walls(Host& H) override
			{
				if (!bTownWall) return;
				// the rammed-earth wall: walked round its square a step under 2 m at a time; open where a lane, a way, a
				// house, the water or the marsh is, and fallen in its stretches (an old wall, its gates long gone)
				const double Ca = std::cos(WallTurn), Sa = std::sin(WallTurn), Hf = WallHalf;
				const double Cor[5][2] = { { -Hf, -Hf }, { Hf, -Hf }, { Hf, Hf }, { -Hf, Hf }, { -Hf, -Hf } };
				std::vector<std::pair<double, double>> Run;
				auto Lay = [&]() {
					if (Run.size() >= 3) H.Wall(Run, 2.4, 0.9);
					Run.clear();
				};
				double Walked = 0.0;
				for (int Sd = 0; Sd < 4; ++Sd)
				{
					const double L = std::hypot(Cor[Sd + 1][0] - Cor[Sd][0], Cor[Sd + 1][1] - Cor[Sd][1]);
					const int N = std::max(1, static_cast<int>(std::ceil(L / 1.8)));
					for (int J = 0; J < N; ++J, Walked += L / N)
					{
						const double T = static_cast<double>(J) / N;
						const double U = Lerp(Cor[Sd][0], Cor[Sd + 1][0], T), V = Lerp(Cor[Sd][1], Cor[Sd + 1][1], T);
						const double X = WallX + U * Ca - V * Sa, Y = WallY + U * Sa + V * Ca;
						const Spot Q = H.At(X, Y);
						const bool bFallen = Noise(Walked / 60.0, 3.7, Seed ^ 0xFA11E) > 0.45;
						if (bFallen || !Q.bLaid || Q.Kind == KLane || Q.Lane < 2.5 || Q.Kind == KWater || Q.Kind == KMarsh || Q.Drawn == 9 || Q.Drawn == 11 || Q.Drawn == 12 ||
							Q.bMan || Q.bVehicle || Q.bNear)
						{
							Lay();
							continue;
						}
						Run.push_back({ X, Y });
					}
				}
				Lay();
			}
			Tone WallStone() const override { return Rgb(167, 145, 107); }
			double WallIvy() const override { return 0.0; }

			static void Put(Host& H, LandField::Base B, double X, double Y, double Room, double Yaw, double Scale, double ScaleZ, LandField::Rgb8 Tn)
			{
				if (H.Free(X, Y, Room)) H.Plant(B, X, Y, Yaw, Scale, ScaleZ, Tn);
			}

			/** The poplars in their lines along the cart tracks, both sides or one, a stretch lined and a stretch bare. */
			void Avenues(Host& H)
			{
				if (bIndia) return;
				Rng R(H.Seed() ^ 0xA7E1);
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const std::vector<std::pair<double, double>>& Line = H.LaneLine(L);
					const double Half = H.LaneHalf(L);
					double Walked = 0.0, Next = R.Range(0.0, 8.0);
					for (size_t K = 1; K < Line.size(); ++K)
					{
						const double DX = Line[K].first - Line[K - 1].first, DY = Line[K].second - Line[K - 1].second, DL = std::hypot(DX, DY);
						Walked += DL;
						if (DL < 1e-6 || Walked < Next) continue;
						const double Step = R.Range(9.0, 12.0);
						Next = Walked + Step;
						for (const double Side : { -1.0, 1.0 })
						{
							const double Sc = R.Range(0.85, 1.2), Sz = R.Range(0.85, 1.15), Yaw = R.Range(0.0, 2.0 * Pi), Lf = R.Range(0.92, 1.08), Miss = R.Next();
							if (Noise(Walked / 160.0, static_cast<double>(L) * 5.1 + Side * 1.7, Seed ^ 0xA7E) < 0.05 || Miss < 0.15) continue;
							const double Off = Half + 2.2;
							const double X = Line[K].first - DY / DL * Side * Off, Y = Line[K].second + DX / DL * Side * Off;
							const Spot Q = H.At(X, Y);
							if (Q.Kind == KVillage || Q.Kind == KWater || Q.Kind == KYard || Q.Lane < 0.8) continue;
							Put(H, KitOf(KPoplar), X, Y, 1.5, Yaw, Sc, Sz, Tint(Lf, Lf, Lf));
						}
					}
				}
			}

			void Plants(Host& H) override
			{
				Avenues(H);
				const double Bd = H.Band();
				constexpr double Cell = 8.0;
				const int NI = static_cast<int>(std::ceil((H.Width() + 2.0 * Bd) / Cell)), NJ = static_cast<int>(std::ceil((H.Depth() + 2.0 * Bd) / Cell));
				const uint64_t Key0 = H.Seed() ^ 0x9A5A7;
				const bool bStacks = bIndia ? (Month >= 4 && Month <= 6) || Month >= 10 : Month >= 9 || Month <= 3;
				const LandField::Rgb8 Plain_ = Tint(1.0, 1.0, 1.0);
				for (int J = 0; J < NJ; ++J)
					for (int I = 0; I < NI; ++I)
					{
						Rng R(Hash(Key0, static_cast<uint64_t>(I), static_cast<uint64_t>(J)));
						const double X0 = -Bd + I * Cell, Y0 = -Bd + J * Cell;
						const double JX = R.Next(), JY = R.Next();
						const double X = X0 + JX * Cell, Y = Y0 + JY * Cell;
						const Spot S = H.At(X, Y);
						if (S.Kind == KWater || S.Kind == KLane) continue;
						// (every draw in a statement of its own, in a set order: g++ and clang take a call's arguments in
						// different orders)
						const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Sc = R.Range(0.8, 1.2), Sz = R.Range(0.85, 1.15), Lf = R.Range(0.9, 1.1);
						const LandField::Rgb8 Leaf = Tint(Lf, Lf, Lf);
						// (India's groves of broad trees stood in by the willows; China's and Manchuria's poplars)
						const int Tall = bIndia ? KWillow : KPoplar;
						switch (S.Kind)
						{
						case KWoods:
							// (the plain's groves open, some 25 trees a hectare: a wood here is a village's grove, not a forest,
							// and the poplars along every track are many already)
							if (Keep < 0.11) Put(H, KitOf(Tall), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							else if (Keep < 0.16) Put(H, KitOf(bIndia ? KPoplar : KWillow), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							break;
						case KRough:
							if (S.Crop == Hill)
							{
								if (!bIndia && Keep < 0.02) Put(H, KitOf(KPine), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
								else if (Keep < 0.045) Put(H, KitOf(KOutcrop), X, Y, 4.0, Yaw, 0.6 + 0.6 * (Sc - 0.8), Sz, Plain_);
								else if (Keep < 0.09) Put(H, KitOf(KBoulder), X, Y, 1.5, Yaw, Sc, Sz, Plain_);
							}
							else if (Keep < 0.004) Put(H, KitOf(KGrave), X, Y, 1.8, Yaw, Sc, Sz, Plain_);
							else if (Keep < 0.012) Put(H, KitOf(KBoulder), X, Y, 1.5, Yaw, Sc, Sz, Plain_);
							break;
						case KVillage:
						case KYard:
							if (Keep < 0.035) Put(H, KitOf(Tall), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							else if (Keep < 0.05) Put(H, KitOf(KWillow), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							else if (bStacks && Keep < 0.18) Put(H, KitOf(KStack), X, Y, 1.6, Yaw, Sc, Sz, Plain_);
							break;
						case KMarsh:
							if (Keep < 0.03) Put(H, KitOf(KWillow), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							break;
						default:
						{
							// the open ground and the fields: the graves in their family groups on the balks and the open
							// ground, a lone tree, the gaoliang's shocks after its harvest, straw stacks at the fields' edges
							const bool bOpen = S.Kind == KOpen;
							const bool bBalk = Tilled(S.Crop) && (S.Edge < 4.0 || StripOf(S).Balk < 2.0);
							const Life L = Tilled(S.Crop) ? ParcelLife(S.Crop, T0()) : Life{};
							if (Keep < (bOpen && !Tilled(S.Crop) ? 0.004 : bBalk ? 0.0015 : 0.0))
							{
								const int Graves = 2 + R.Below(4);
								for (int G = 0; G < Graves; ++G)
								{
									const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(0.0, 5.0), GS = R.Range(0.8, 1.25), GY = R.Range(-0.3, 0.3);
									Put(H, KitOf(KGrave), X + D * std::cos(A), Y + D * std::sin(A), 1.6, GY, GS, GS, Plain_);
								}
							}
							else if (Keep < 0.006 + (bOpen ? 0.004 : 0.0)) Put(H, KitOf(Tall), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							else if (L.Crop == Gao && !L.bUp && L.Since < 1.5 && Keep < 0.03) Put(H, KitOf(KStack), X, Y, 1.4, Yaw, 0.7 * Sc, 1.3 * Sz, Tint(0.95, 0.86, 0.76));
							else if (bStacks && Tilled(S.Crop) && S.Edge < 3.0 && Keep < 0.02) Put(H, KitOf(KStack), X, Y, 1.6, Yaw, Sc, Sz, Plain_);
							break;
						}
						}
					}
			}

			int Fx(const std::string& Ground, int Else) const override
			{
				if (Ground == "rough" || Ground == "rock") return 7;
				// (India's fields in the monsoon throw up mud)
				if (bIndia && Month >= 7 && Month <= 9 && (Ground == "field" || Ground == "fields")) return 4;
				return Else;
			}
			const char* Earth() const override { return bManchuria ? "blackearth" : "loam"; }

		private:
			int Month = 7;
			bool bIndia = false, bManchuria = false, bHills = false, bTownWall = false;
			double Green = 1.0, MeanZ = 0.0, SnowLying = 0.0;
			double WallX = 0.0, WallY = 0.0, WallHalf = 0.0, WallTurn = 0.0;
		};
	}

	std::unique_ptr<Region> MakePlain(const LandField::Field& F, uint64_t S) { return std::make_unique<Plain>(F, S); }
}
