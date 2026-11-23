#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
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
			KPoplar = 0, // the poplar, built here: a soft leafy column, or its fine grey twigs in the winter
			KWillow = 1,
			KPine = 2,
			KHouse = 3,
			KFarm = 4,
			KTower = 5,
			KGrave = 6,
			KStack = 7,
			KBoulder = 8,
			KOutcrop = 9,
			// (built here in KitPlain: no file to read or to miss)
			KRidge = 10,      // a balk's ridge 16 m long between the strips: the strips' lines from 30 m up and from afar
			KRidgeShort = 11, // the same 6 m long, at a line's ends
			KWallLong = 12,   // the town wall's rammed earth, 6 m of it: its layers, its putlog holes, its battlements
			KWallShort = 13,  // the same 2 m long
			KGate = 14,       // a gate's or a corner's tower on the wall, a pavilion on it
			KCave = 15        // the loess's cave dwellings: three arched fronts dug into a cut bank
		};
		constexpr double RidgeLong = 16.0, RidgeShort = 6.0, WallLong = 6.0, WallShort = 2.0;
		/** The strips' blocks: a quarter of the survey's half-mile block (LandField's SurveyBlock, 804.67 m) on a side, so
		 * a block's edges are the fields' edges; the strips run one way within each, north-south or east-west. */
		constexpr double StripCell = 402.335, FieldLine = 100.58375;
		/** The town wall: its rammed earth's height (the fight's wall the same), its thickness at the foot and the top. */
		constexpr double WallHigh = 3.05, WallFoot = 1.0, WallTop = 0.6;
		/** The loess terraces: a step's rise; the tread's fall outward; the riser's share of a step's run. */
		constexpr double Rise = 3.0, TreadFall = 0.1, RiserRun = 0.25;

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
		const Tone BlackEarth = Rgb(72, 64, 54); // (the art director, 23 November: dark soil, not tan)
		const Tone BlackFurrows = Rgb(56, 50, 43);
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

		/** A broad leaf from its foot (X0, Y0, Z0) to its tip: a rounded oval Width across, its tip blunt; one face, lit
		 * from above as the row crops' leaves are seen. */
		void Leaflet(Writer& W, double X0, double Y0, double Z0, double X1, double Y1, double Z1, double Width, const Tone& Foot, const Tone& Tip)
		{
			double AX = -(Y1 - Y0), AY = X1 - X0;
			const double L = std::hypot(AX, AY);
			if (L < 1e-9) { AX = 1.0; AY = 0.0; }
			else { AX /= L; AY /= L; }
			auto At = [&](double F, double Side, double Lift, const Tone& C) {
				return W.Vert(X0 + F * (X1 - X0) + Side * AX, Y0 + F * (Y1 - Y0) + Side * AY, Z0 + F * (Z1 - Z0) + Lift, 0.12 * AX, 0.12 * AY, 0.99, C);
			};
			const Tone Mid = Mixed(Foot, Tip, 0.5);
			const uint32_t B = At(0.0, 0.0, 0.0, Foot), L1 = At(0.38, 0.5 * Width, 0.01, Mid), R1 = At(0.38, -0.5 * Width, 0.01, Mid);
			const uint32_t L2 = At(0.82, 0.32 * Width, 0.0, Tip), R2 = At(0.82, -0.32 * Width, 0.0, Tip), T = At(1.0, 0.0, -0.01, Tip);
			W.Tri(B, R1, L1);
			W.Tri(L1, R1, R2);
			W.Tri(L1, R2, L2);
			W.Tri(L2, R2, T);
		}

		/** A rounded spindle along P (a seed head): six sides round, R its radius at each point, its normals out. */
		void Spindle(Writer& W, const double (&P)[4][3], const double (&R)[4], const Tone& Foot, const Tone& Tip)
		{
			constexpr int Sides = 6;
			uint32_t Ring[4][Sides];
			for (int K = 0; K < 4; ++K)
			{
				const double* A = P[K > 0 ? K - 1 : 0];
				const double* B = P[K < 3 ? K + 1 : 3];
				double T[3] = { B[0] - A[0], B[1] - A[1], B[2] - A[2] };
				const double TL = std::max(1e-9, std::sqrt(T[0] * T[0] + T[1] * T[1] + T[2] * T[2]));
				for (double& V : T) V /= TL;
				const double Ref[3] = { std::fabs(T[2]) < 0.9 ? 0.0 : 1.0, 0.0, std::fabs(T[2]) < 0.9 ? 1.0 : 0.0 };
				double N1[3] = { T[1] * Ref[2] - T[2] * Ref[1], T[2] * Ref[0] - T[0] * Ref[2], T[0] * Ref[1] - T[1] * Ref[0] };
				const double NL = std::max(1e-9, std::sqrt(N1[0] * N1[0] + N1[1] * N1[1] + N1[2] * N1[2]));
				for (double& V : N1) V /= NL;
				const double N2[3] = { T[1] * N1[2] - T[2] * N1[1], T[2] * N1[0] - T[0] * N1[2], T[0] * N1[1] - T[1] * N1[0] };
				const Tone C = Mixed(Foot, Tip, K / 3.0);
				for (int S = 0; S < Sides; ++S)
				{
					const double Th = 2.0 * Pi * (S + 0.5 * (K % 2)) / Sides, Ct = std::cos(Th), St = std::sin(Th);
					const double D[3] = { Ct * N1[0] + St * N2[0], Ct * N1[1] + St * N2[1], Ct * N1[2] + St * N2[2] };
					double NX = D[0], NY = D[1], NZ = D[2] + 0.35;
					const double NN = std::sqrt(NX * NX + NY * NY + NZ * NZ);
					NX /= NN; NY /= NN; NZ /= NN;
					Ring[K][S] = W.Vert(P[K][0] + R[K] * D[0], P[K][1] + R[K] * D[1], P[K][2] + R[K] * D[2], NX, NY, NZ, C);
				}
			}
			for (int K = 0; K < 3; ++K)
				for (int S = 0; S < Sides; ++S)
				{
					const int S1 = (S + 1) % Sides;
					W.Tri(Ring[K][S], Ring[K][S1], Ring[K + 1][S1]);
					W.Tri(Ring[K][S], Ring[K + 1][S1], Ring[K + 1][S]);
				}
		}

		/** A ridge along x, Half either way: its profile Wide across and High up (round, or plain for afar), sunk at its
		 * ends Sink so lengths laid end to end run on as one line. (The rice country's bund, LandAsia.) */
		void Ridge(Writer& W, double Half, double High, double Wide, const Tone& Crest, const Tone& Flank, const Tone& Foot, bool bRound, double Sink, int Sections = 1)
		{
			struct Pt
			{
				double Y, Z, NY, NZ;
				Tone C;
			};
			const double H = 0.5 * Wide;
			const std::vector<Pt> Round = { { -H, -0.05, -0.6, 0.8, Foot }, { -0.62 * H, 0.62 * High, -0.45, 0.89, Flank }, { -0.24 * H, High, -0.12, 0.99, Crest },
											{ 0.24 * H, High, 0.12, 0.99, Crest }, { 0.62 * H, 0.62 * High, 0.45, 0.89, Flank }, { H, -0.05, 0.6, 0.8, Foot } };
			const std::vector<Pt> Flat = { { -H, -0.05, -0.5, 0.87, Foot }, { -0.25 * H, High, -0.15, 0.99, Crest }, { 0.25 * H, High, 0.15, 0.99, Crest }, { H, -0.05, 0.5, 0.87, Foot } };
			const std::vector<Pt>& Pr = bRound ? Round : Flat;
			// (its length in Sections between the sunk ends, each ring's earth and grass its own shade a little, so a long
			// ridge is no smooth tube)
			std::vector<double> Xs;
			if (Sink > 0.0) Xs.push_back(-Half);
			for (int K = 0; K <= Sections; ++K) Xs.push_back(-Half + Sink + (2.0 * (Half - Sink)) * K / Sections);
			if (Sink > 0.0) Xs.push_back(Half);
			const int Rings = static_cast<int>(Xs.size());
			std::vector<std::vector<uint32_t>> V(static_cast<size_t>(Rings));
			for (int R = 0; R < Rings; ++R)
			{
				const double X = Xs[static_cast<size_t>(R)];
				const bool bEnd = Sink > 0.0 && (R == 0 || R == Rings - 1);
				const double Sh = Sections > 1 ? 0.9 + 0.2 * Unit(Hash(0x41D6, static_cast<uint64_t>(R))) : 1.0;
				for (const Pt& P : Pr) V[static_cast<size_t>(R)].push_back(W.Vert(X, P.Y * (bEnd ? 0.7 : 1.0), bEnd ? -0.06 : P.Z, 0.0, P.NY, P.NZ, Varied(P.C, Sh, 0.0)));
			}
			for (int R = 0; R + 1 < Rings; ++R)
				for (size_t K = 0; K + 1 < Pr.size(); ++K)
				{
					const std::vector<uint32_t>& A = V[static_cast<size_t>(R)];
					const std::vector<uint32_t>& B = V[static_cast<size_t>(R) + 1];
					W.Tri(A[K], B[K], B[K + 1]);
					W.Tri(A[K], B[K + 1], A[K + 1]);
				}
		}

		/** A quad A, B, C, D (counter-clockwise seen from its front), lit flat by its own normal. */
		void Quad(Writer& W, const double (&A)[3], const double (&B)[3], const double (&C)[3], const double (&D)[3], const Tone& T)
		{
			const double UX = B[0] - A[0], UY = B[1] - A[1], UZ = B[2] - A[2], VX = D[0] - A[0], VY = D[1] - A[1], VZ = D[2] - A[2];
			double NX = UY * VZ - UZ * VY, NY = UZ * VX - UX * VZ, NZ = UX * VY - UY * VX;
			const double L = std::sqrt(NX * NX + NY * NY + NZ * NZ);
			if (L > 1e-12) { NX /= L; NY /= L; NZ /= L; }
			const uint32_t I0 = W.Vert(A[0], A[1], A[2], NX, NY, NZ, T), I1 = W.Vert(B[0], B[1], B[2], NX, NY, NZ, T);
			const uint32_t I2 = W.Vert(C[0], C[1], C[2], NX, NY, NZ, T), I3 = W.Vert(D[0], D[1], D[2], NX, NY, NZ, T);
			W.Tri(I0, I1, I2);
			W.Tri(I0, I2, I3);
		}

		/**
		 * A face of rammed earth from its foot (A0 to B0, at Z0) up to its top (A1 to B1, at Z1), its front to the right of
		 * A to B seen from above: laid in its layers Layer high, each its own shade as each basket of earth was, every
		 * fourth a little darker where the boards' joint ran.
		 */
		void Rammed(Writer& W, double AX0, double AY0, double BX0, double BY0, double AX1, double AY1, double BX1, double BY1, double Z0, double Z1, double Layer, const Tone& C, uint64_t S)
		{
			const int N = std::max(1, static_cast<int>(std::ceil((Z1 - Z0) / Layer - 1e-6)));
			for (int K = 0; K < N; ++K)
			{
				const double T0 = static_cast<double>(K) / N, T1 = static_cast<double>(K + 1) / N;
				const double A0[3] = { Lerp(AX0, AX1, T0), Lerp(AY0, AY1, T0), Lerp(Z0, Z1, T0) }, B0[3] = { Lerp(BX0, BX1, T0), Lerp(BY0, BY1, T0), Lerp(Z0, Z1, T0) };
				const double A1[3] = { Lerp(AX0, AX1, T1), Lerp(AY0, AY1, T1), Lerp(Z0, Z1, T1) }, B1[3] = { Lerp(BX0, BX1, T1), Lerp(BY0, BY1, T1), Lerp(Z0, Z1, T1) };
				const double Shade = (0.93 + 0.12 * Unit(Hash(S, static_cast<uint64_t>(K)))) * (K % 4 == 0 ? 0.9 : 1.0);
				Quad(W, A0, B0, B1, A1, Varied(C, Shade, 0.0));
			}
		}

		/** A flat fan (a half disc) in the plane y = Y facing +y: its middle (X, Z), Rad round, from angle 0 to pi. */
		void Arch(Writer& W, double X, double Y, double Z, double Rad, int Steps, const Tone& T)
		{
			const uint32_t Mid = W.Vert(X, Y, Z, 0.0, 1.0, 0.0, T);
			uint32_t Prev = W.Vert(X - Rad, Y, Z, 0.0, 1.0, 0.0, T);
			for (int K = 1; K <= Steps; ++K)
			{
				const double A = Pi - Pi * K / Steps;
				const uint32_t Cur = W.Vert(X + Rad * std::cos(A), Y, Z + Rad * std::sin(A), 0.0, 1.0, 0.0, T);
				W.Tri(Mid, Prev, Cur);
				Prev = Cur;
			}
		}
		/** A flat rectangle in the plane y = Y facing +y, from (X0, Z0) to (X1, Z1). */
		void Panel(Writer& W, double X0, double X1, double Y, double Z0, double Z1, const Tone& T)
		{
			const double A[3] = { X1, Y, Z0 }, B[3] = { X0, Y, Z0 }, C[3] = { X0, Y, Z1 }, D[3] = { X1, Y, Z1 };
			Quad(W, A, B, C, D, T);
		}

		class Plain final : public Region
		{
			/** A road's way out of the field at its edge (its place, the way out). */
			struct Exit
			{
				double X = 0.0, Y = 0.0, DX = 0.0, DY = 0.0;
			};
			/** A cave dwellings' yard: its bank's middle, the way it faces (toward the field), the yard's height. */
			struct Cave
			{
				double X = 0.0, Y = 0.0, OX = 0.0, OY = -1.0, Yard = 0.0;
			};
			static constexpr double CaveHalf = 10.0;

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
				// (the India's crops dried off and the earth baked and cracked from March to the rains)
				bHardpan = bIndia && M >= 3 && M <= 6;
				bBare = LeaflessOf(F, Look::Plain) || SnowLying > 0.25;
				// The loess country (Shansi, Shensi, western Honan; or where the rules say hills): the tableland rising
				// round the field in its terraces, cut by its gullies, its cave dwellings in their banks (Relief).
				bLoess = !bIndia && !bManchuria && (F.Lon < 114.5 || F.Terrain == "hills" || F.Terrain == "mountain");
				if (bLoess)
				{
					bHills = false;
					// The roads out of the field (LandField runs each lane that reaches the field's edge on out through the
					// band): the terraces and the gullies leave them a way up (as the Mediterranean's, LandMed).
					for (int J = 0; J < F.Rows; ++J)
						for (int I = 0; I < F.Cols; ++I)
						{
							if (I != 0 && J != 0 && I != F.Cols - 1 && J != F.Rows - 1) continue;
							const size_t K = static_cast<size_t>(J) * F.Cols + I;
							if (K >= F.Ground.size()) continue;
							const int G = F.Ground[K];
							if (G < 0 || static_cast<size_t>(G) >= F.Grounds.size()) continue;
							const std::string& Word = F.Grounds[static_cast<size_t>(G)];
							if (Word != "road" && Word != "bridge") continue;
							Exit E;
							E.DX = I == 0 ? -1.0 : I == F.Cols - 1 ? 1.0 : 0.0;
							E.DY = E.DX != 0.0 ? 0.0 : J == 0 ? -1.0 : 1.0;
							E.X = (I + 0.5) * F.Cell + E.DX * 0.5 * F.Cell;
							E.Y = (J + 0.5) * F.Cell + E.DY * 0.5 * F.Cell;
							bool bNear = false;
							for (const Exit& Q : Exits)
								if (Q.DX == E.DX && Q.DY == E.DY && std::hypot(Q.X - E.X, Q.Y - E.Y) < 30.0) bNear = true;
							if (!bNear) Exits.push_back(E);
						}
					// The cave dwellings: three along the far side (the north, the way the attack looks), one either side,
					// each a yard cut into the slope some 170 m out, its bank facing the field; clear of the roads.
					const double FW = F.Width(), FH = F.Depth();
					const double Spots[5][4] = { { 0.2 * FW, FH + 172.0, 0.0, -1.0 }, { 0.52 * FW, FH + 166.0, 0.0, -1.0 }, { 0.8 * FW, FH + 178.0, 0.0, -1.0 },
												 { FW + 170.0, 0.55 * FH, -1.0, 0.0 }, { -170.0, 0.4 * FH, 1.0, 0.0 } };
					for (int K = 0; K < 5; ++K)
					{
						Cave C;
						const double Slide = 80.0 * (Unit(Hash(S, 0xCA5E, static_cast<uint64_t>(K))) - 0.5);
						C.X = Spots[K][0] + (Spots[K][2] == 0.0 ? Slide : 0.0);
						C.Y = Spots[K][1] + (Spots[K][2] == 0.0 ? 0.0 : Slide);
						C.OX = Spots[K][2];
						C.OY = Spots[K][3];
						for (int Try = 0; Try < 4; ++Try)
						{
							bool bRoad = false;
							for (const Exit& E : Exits)
								if (E.DX == -C.OX && E.DY == -C.OY && std::hypot(E.X - C.X - 170.0 * C.OX, E.Y - C.Y - 170.0 * C.OY) < 70.0) bRoad = true;
							if (!bRoad) break;
							C.X += C.OY != 0.0 ? 110.0 : 0.0;
							C.Y += C.OX != 0.0 ? 110.0 : 0.0;
						}
						C.Yard = LoessLift(C.X, C.Y, Outside(C.X, C.Y)) - 6.2;
						Caves.push_back(C);
					}
				}
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
				double Jit = 0.0, Balk = 99.0, Shade = 1.0, Warm = 0.0;
				/** The way its rows run (radians from east). */
				double Angle = 0.0;
				bool bFallow = false;
			};
			/** A block of strips (StripCell on a side): its key, whether its strips run north-south, their mean width. */
			struct Block
			{
				uint64_t Key = 0;
				bool bNS = true;
				double W = 14.0;
			};
			Block BlockOf(double X, double Y) const
			{
				Block B;
				const int64_t I = static_cast<int64_t>(std::floor(X / StripCell)), J = static_cast<int64_t>(std::floor(Y / StripCell));
				B.Key = Hash(Seed ^ 0x57C1, static_cast<uint64_t>(I + 4096), static_cast<uint64_t>(J + 4096));
				B.bNS = Unit(Hash(B.Key, 1)) < 0.55;
				B.W = 9.0 + 10.0 * Unit(Hash(B.Key, 2));
				return B;
			}
			/** A block's strips' K-th edge, metres across them (X where they run north-south, else Y). */
			static double EdgeOf(const Block& B, int64_t K) { return (static_cast<double>(K) + 0.36 * (Unit(Hash(B.Key, static_cast<uint64_t>(K + (int64_t(1) << 20)), 3)) - 0.5)) * B.W; }
			/** The strip a place across a block's strips lies in (its number), and its edges either side. */
			static int64_t StripAt(const Block& B, double U, double& Lo, double& Hi)
			{
				const int64_t K = static_cast<int64_t>(std::floor(U / B.W));
				for (int64_t T = K - 1; T <= K + 1; ++T)
				{
					Lo = EdgeOf(B, T);
					Hi = EdgeOf(B, T + 1);
					if (U >= Lo && U < Hi) return T;
				}
				Lo = EdgeOf(B, K);
				Hi = EdgeOf(B, K + 1);
				return K;
			}
			Strip StripOf(const Spot& S) const
			{
				Strip St;
				if (bIndia)
				{
					// (India's plots small and every way, each its own strips across its own grain)
					const double W = 9.0 + 10.0 * Unit(Hash(S.Key, 0x5791));
					const double B = S.Across / W + Unit(Hash(S.Key, 0x5792));
					const double F = B - std::floor(B);
					St.Key = Hash(S.Key, static_cast<uint64_t>(std::floor(B) + 512.0));
					St.Balk = std::min(F, 1.0 - F) * W;
					St.Angle = S.Angle;
				}
				else
				{
					// China's and Manchuria's: long and narrow, every strip of a block running the one way, square to the
					// compass as the fields and the tracks are, on from field to field across a balk
					const Block B = BlockOf(S.X, S.Y);
					const double U = B.bNS ? S.X : S.Y;
					double Lo = 0.0, Hi = 0.0;
					const int64_t K = StripAt(B, U, Lo, Hi);
					St.Key = Hash(B.Key, static_cast<uint64_t>(K + (int64_t(1) << 20)));
					St.Balk = std::min(U - Lo, Hi - U);
					St.Angle = B.bNS ? 0.5 * Pi : 0.0;
				}
				St.Jit = 0.9 * (Unit(Hash(St.Key, 0x717)) - 0.5) + 0.6 * (Unit(Hash(S.Key, 0x718)) - 0.5);
				St.bFallow = Unit(Hash(St.Key, 0xFA11)) < 0.1;
				// (each strip's own shade and its own warmth, a yellower green or a bluer: the strips read on from 30 m up, as
				// the farm's fields do; the art director, 23 November)
				St.Shade = 0.82 + 0.36 * Unit(Hash(St.Key, 0x5AD));
				St.Warm = Unit(Hash(St.Key, 0x3A3)) - 0.5;
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
			// ---- the loess's tableland

			/** The loess's rise beyond the field: its slopes facing the field, a flat top some 60 to 95 m up 450 m out,
			 * then the tableland's broken country. */
			double LoessLift(double X, double Y, double D) const
			{
				const double Side = 70.0 + 26.0 * Noise(X / 1400.0, Y / 1400.0, Seed ^ 0x10E5);
				return Side * Smooth(25.0, 430.0, D) + 35.0 * Smooth(500.0, 2500.0, D) * (0.5 + 0.5 * Fbm(X, Y, 1800.0, 2, Seed ^ 0x10E6));
			}
			/** Clear of the roads' ways up out of the field: 0 on one, 1 away. */
			double ExitClear(double X, double Y) const
			{
				double W = 1.0;
				for (const Exit& E : Exits)
				{
					const double RX = X - E.X, RY = Y - E.Y;
					const double Along = RX * E.DX + RY * E.DY;
					if (Along < -30.0) continue;
					const double Across = std::fabs(-RX * E.DY + RY * E.DX);
					const double Room = 12.0 + 0.12 * std::max(0.0, Along);
					W *= Smooth(Room, Room + 16.0, Across);
				}
				return W;
			}
			/** A gully's share of its depth here (0 none): the gullies running out from the tableland toward the plain,
			 * their banks cut steep; none by the cave dwellings. */
			double GullyAt(double X, double Y, double D) const
			{
				if (D < 50.0) return 0.0;
				const double A = std::atan2(Y - 0.5 * Fd.Depth(), X - 0.5 * Fd.Width());
				const double G = 1.0 - std::fabs(Noise(A * 1500.0 / 170.0, D / 650.0, Seed ^ 0x10E7));
				double V = Smooth(0.905, 0.972, G) * Smooth(50.0, 140.0, D) * Smooth(Pi, 2.9, std::fabs(A));
				for (const Cave& C : Caves) V *= Smooth(40.0, 90.0, std::hypot(X - C.X, Y - C.Y));
				return V;
			}
			/** How much of a terrace's step is laid here (0 none, 1 in full): on the slopes in the band (the ring's mesh
			 * cannot hold them), not all of them (some left to the grass), not in a gully nor across a road's way up. */
			double TerraceW(double X, double Y, double D) const
			{
				if (D < 35.0 || D > 345.0) return 0.0;
				double W = Smooth(35.0, 70.0, D) * Smooth(345.0, 290.0, D) * Smooth(-0.25, 0.05, Noise(X / 480.0, Y / 480.0, Seed ^ 0x7E1));
				if (W <= 0.0) return 0.0;
				W *= 1.0 - Smooth(0.1, 0.5, GullyAt(X, Y, D));
				return W <= 0.0 ? 0.0 : W * ExitClear(X, Y);
			}
			/** A terrace's step at a height: a tread falling gently outward, then the riser (its deviation from the slope). */
			static double Step(double E)
			{
				const double U = E / Rise, F = U - std::floor(U);
				const double P = F < 1.0 - RiserRun ? TreadFall * F / (1.0 - RiserRun) : TreadFall + (1.0 - TreadFall) * Smooth(1.0 - RiserRun, 1.0, F);
				return Rise * (P - F);
			}
			/** The cave dwellings' yard at a place, in the cave's frame (U along its bank, V out from it): whether in it. */
			static bool InYard(const Cave& C, double X, double Y, double Margin)
			{
				const double RX = X - C.X, RY = Y - C.Y;
				const double U = RX * C.OY - RY * C.OX, V = RX * C.OX + RY * C.OY;
				return std::fabs(U) < CaveHalf + 2.0 + Margin && V > -4.0 - Margin && V < 45.0 + Margin;
			}
			double Relief(double X, double Y, double Z) const override
			{
				if (!bLoess) return Z;
				const double D = Outside(X, Y);
				if (D <= 0.0) return Z; // (the field: the rules' own heights)
				const double E = LoessLift(X, Y, D);
				const double W = TerraceW(X, Y, D);
				const double G = GullyAt(X, Y, D) * ExitClear(X, Y);
				double Up = E + (W > 0.0 ? W * Step(E) : 0.0) - G * std::min(0.85 * E, 10.0 + 14.0 * Smooth(80.0, 600.0, D));
				for (const Cave& C : Caves)
					if (InYard(C, X, Y, 0.0)) Up = std::min(Up, C.Yard);
				return Z + Up;
			}
			bool Surveyed() const override { return !bIndia; }

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
					// (the soybeans' rows show the earth between; Manchuria's black earth between all its rows)
					double Cover = (L.Crop == Soy ? (bManchuria ? 0.55 : 0.75) : (bManchuria ? 0.85 : 0.95)) * Smooth(0.05, 0.6, L.High);
					if (L.Crop == Wheat) Cover = std::max(Cover, 0.22); // (the winter wheat's rows a green haze on the loess)
					return Mixed(Floor, CanopyOf(L), Cover);
				}
				if (L.Since < 1.3 && (L.Crop == Gao || L.Crop == Millet || L.Crop == Wheat)) return Mixed(Floor, L.Crop == Gao ? GaoStubbleC : StrawC, (L.Crop == Gao ? 0.5 : 0.6) * (bManchuria ? 0.6 : 1.0));
				// (India's fields baked before the rains: the crust's pale grey-buff, the same as its plates, so the cracked
				// patches read as one crust with the uncracked between them)
				if (bHardpan) return Mixed(Floor, Rgb(198, 180, 142), 0.65);
				// (ploughed: the furrows' shade, drying paler)
				return Varied(Mixed(Floor, bManchuria ? BlackFurrows : Mixed(Furrows, Soil(), 0.3), 0.5), L.Since < 2.5 ? 0.9 : 1.0, 0.0);
			}
			Tone FloorAt(const Spot& S) const
			{
				// (India's parched months paler, the baked hardpan's crust in its patches; the loess's own mottle)
				const Tone T = Varied(Soil(), (0.95 + 0.06 * S.N2) * (bIndia && Green < 0.2 ? 1.05 : 1.0), 0.0);
				return bHardpan ? Mixed(T, Rgb(186, 168, 132), 0.35 * Smooth(-0.2, 0.5, S.N1) + 0.15 * Smooth(0.0, 0.8, S.N2)) : T;
			}
			Tone FieldColour(const Spot& S) const
			{
				const Strip St = StripOf(S);
				const Life L = ParcelLife(StripCrop(S, St), T0() + St.Jit);
				Tone T = Varied(StateColour(L, FloorAt(S)), St.Shade, 0.0);
				T = Tone{ T.R * (1.0 + 0.24 * St.Warm), T.G * (1.0 + 0.06 * St.Warm), T.B * (1.0 - 0.28 * St.Warm) };
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

			/** The loess's band beyond the field: a gully's banks bare loess, its floor grassed; a cave dwellings' yard
			 * beaten earth; a terrace's riser bare (its walls laid in Walls). Else Base. */
			Tone Loessed(const Spot& S, const Tone& Base) const
			{
				const double D = Outside(S.X, S.Y);
				if (D <= 0.0) return Base;
				for (const Cave& C : Caves)
					if (InYard(C, S.X, S.Y, 0.0)) return Mixed(Varied(Loess, 1.04 + 0.04 * S.N2, 0.0), StrawC, 0.1 + 0.1 * Smooth(0.2, 0.8, S.N1));
				Tone T = Base;
				const double E = LoessLift(S.X, S.Y, D), W = TerraceW(S.X, S.Y, D);
				if (W > 0.3)
				{
					const double F = E / Rise - std::floor(E / Rise);
					T = Mixed(T, Varied(Loess, 1.06, 0.0), 0.7 * Smooth(1.0 - RiserRun - 0.08, 1.0 - RiserRun + 0.04, F) * W);
				}
				const double G = GullyAt(S.X, S.Y, D);
				if (G > 0.02)
				{
					// (its banks the loess's bare pale walls, streaked; its floor grass and a thread of the water's sand)
					const Tone Bank = Varied(Mixed(Loess, Rgb(188, 166, 124), 0.5 + 0.3 * S.N2), 0.97 + 0.06 * S.N1, 0.0);
					T = Mixed(T, Bank, Smooth(0.02, 0.35, G));
					T = Mixed(T, GrassAt(S, HillGrass), 0.6 * Smooth(0.85, 1.0, G));
				}
				return T;
			}

			Tone Colour(const Spot& S) const override
			{
				if (bLoess && S.Kind != KLane && S.Kind != KWater && Outside(S.X, S.Y) > 0.0) return Loessed(S, Plainly(S));
				return Plainly(S);
			}
			Tone Plainly(const Spot& S) const
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
				case KWorks: return Mixed(Varied(GrassAt(S, OpenGrass), 0.86 + 0.16 * Smooth(-0.5, 0.5, S.N2), 0.0), Varied(Soil(), 0.85 + 0.1 * S.N2, 0.0), 0.06 + 0.3 * Smooth(0.35, 0.75, -S.N1)); // (grassed over, darker in its swales, the earth bare in its patches)
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
				if (bLoess)
				{
					// (the tableland's gullies bare pale loess in their branching lines; its tops the fields' patchwork,
					// dustier with the distance)
					const double D = Outside(S.X, S.Y), G = GullyAt(S.X, S.Y, D);
					T = Mixed(T, Varied(Rgb(184, 162, 122), 0.95 + 0.1 * S.Broad, 0.0), Smooth(0.05, 0.5, G) + 0.25 * Smooth(300.0, 2500.0, D));
				}
				const double Hl = HillAt(S.X, S.Y);
				if (Hl > 0.05)
				{
					const Tone Hill_ = Varied(Mixed(Rgb(156, 134, 100), HillGrass, (0.15 + 0.3 * Green) * Smooth(0.2, 0.8, Fbm(S.X, S.Y, 300.0, 2, Seed ^ 0xA53))), 0.95 + 0.1 * S.Broad, 0.0);
					T = Mixed(T, Hill_, Smooth(0.08, 0.3, Hl));
				}
				return T;
			}

			// ---------------------------------------------------------------- round the eye

			/** Whether a place lies on a row crop's line (its lines Pitch apart across Angle, within Tol of one): its clumps
			 * of rows laid only there, so the rows run on unbroken from clump to clump as they were drilled, not each clump
			 * its own way. The share of places on a line is 2 Tol / Pitch. */
			static bool OnRow(const Spot& S, double Angle, double Pitch, double Tol)
			{
				const double V = -S.X * std::sin(Angle) + S.Y * std::cos(Angle);
				return std::fabs(V - Pitch * std::round(V / Pitch)) < Tol;
			}

			Grown Grow(const Spot& S, double Pick) const override
			{
				if (S.Kind == KWater || S.Drawn == 9 /*floor*/ || S.Drawn == 10 /*wall*/) return Grown{};
				const bool bSnow = SnowLying > 0.25;
				if (S.Kind == KLane) return S.Lane > 0.8 ? Grown{ ClutOf(CGrass), bSnow ? 0.2 : 0.3 + 0.4 * Green } : Grown{};
				if (bLoess && Outside(S.X, S.Y) > 0.0)
				{
					for (const Cave& C : Caves)
						if (InYard(C, S.X, S.Y, 0.0)) return Pick < 0.15 ? Grown{ ClutOf(CGrass), 0.3 } : Grown{};
					// (the terraces' lips: a band of long grass along each bank's top, rounding it; the art director)
					const double D = Outside(S.X, S.Y);
					if (D > 30.0 && TerraceW(S.X, S.Y, D) >= 0.45)
					{
						const double L0 = Rise * (1.0 - RiserRun), F = (LoessLift(S.X, S.Y, D) - L0) / Rise;
						if (F - std::floor(F) < 0.13) return Grown{ ClutOf(CGrass), bSnow ? 0.3 : 1.5 };
					}
				}
				// (the village's ground: grass along the walls and round the yards, thin by the doors)
				if (S.Kind == KVillage) return Pick < 0.8 ? Grown{ ClutOf(CGrass), bSnow ? 0.15 : S.bNear ? 0.35 : 0.85 } : Grown{};
				if (S.Kind == KYard) return Pick < 0.25 ? Grown{ ClutOf(CStubble), 0.15 } : Grown{};
				// (the old fieldworks' ground: grass over it again, trodden thin in its patches)
				if (S.Kind == KWorks)
				{
					// (one sward over it, the ground's own mottle through it, the taller tufts in its patches; the art
					// director, 23 November)
					if (bSnow) return Pick < 0.85 ? Grown{ ClutOf(CGrass), 0.2 } : Grown{};
					return Pick < 0.62 ? Grown{ LandField::Base::Sward, 1.25 } : Grown{ ClutOf(CGrass), 0.5 + 0.6 * Smooth(-0.3, 0.5, S.N1) };
				}
				if (S.Kind == KMarsh) return Pick < 0.6 ? Grown{ ClutOf(CReed), bSnow ? 0.4 : 0.7 } : Grown{ ClutOf(CGrass), bSnow ? 0.3 : 1.0 };
				if (S.Kind == KWoods) return Grown{ ClutOf(CGrass), bSnow ? 0.2 : 0.6 };
				switch (S.Crop)
				{
				case Grass:
					// (India's parched grazing: tufts on the baked crust between)
					if (bHardpan && Pick >= 0.6) return Grown{ ClutOf(CGao), 0.3 };
					return Grown{ ClutOf(CGrass), bSnow ? 0.25 : bHardpan ? 0.8 : 1.2 };
				case Hill: return Grown{ ClutOf(CGrass), bSnow ? 0.25 : 0.95 };
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
				if (S.Crop != Terrace && (St.Balk < 0.5 || S.Edge < 0.5)) return Grown{ ClutOf(CGrass), bSnow ? 0.3 : 1.6 }; // (the balk's grass, through its ridge)
				const Life L = S.Crop == Terrace ? ParcelLife(Terrace, T0()) : ParcelLife(StripCrop(S, St), T0() + St.Jit);
				if (bSnow)
				{
					// under the snow: every cut strip's stubble in its rows through it (Manchuria's on its ridges, laid on
					// their lines), the gaoliang's tall
					if (L.bUp || St.bFallow || L.Since >= 6.0) return Grown{};
					if (bManchuria) return OnRow(S, St.Angle, 1.2, 0.1) ? Grown{ ClutOf(CStubble), 2.0 } : Grown{};
					return Grown{ ClutOf(CStubble), 0.95 };
				}
				if (L.bUp)
				{
					switch (L.Crop)
					{
					case Gao:
						if (L.High >= 0.4) return Grown{ ClutOf(CGao), 0.55 };
						break;
					case Millet:
						if (L.High >= 0.3) return Grown{ ClutOf(CGrain), 1.0 };
						if (!bManchuria) return Grown{ ClutOf(CStubble), 0.9 };
						break;
					case Wheat:
						if (L.High >= 0.35) return Grown{ ClutOf(CGrain), 1.1 };
						if (!bManchuria) return Grown{ ClutOf(CStubble), 0.8 };
						break;
					default: break;
					}
					// (the soybeans, the pulses, the mustard, the young gaoliang: their rows full, on their lines)
					return OnRow(S, St.Angle, 1.0, 0.2) ? Grown{ ClutOf(CBush), 1.55 } : Grown{};
				}
				if (bManchuria && !St.bFallow && L.Since < 6.0) return OnRow(S, St.Angle, 1.2, 0.1) ? Grown{ ClutOf(CStubble), 2.0 } : Grown{}; // (the ridges)
				if (L.Since < 1.3 && (L.Crop == Gao || L.Crop == Millet || L.Crop == Wheat)) return Grown{ ClutOf(CStubble), 0.95 };
				if (L.Crop == Gao && L.Since < 3.5) return Grown{ ClutOf(CStubble), 0.5 }; // (the gaoliang's stubs stand into the winter)
				// (India's fields baked and cracked till the rains)
				if (bHardpan) return Pick < 0.8 ? Grown{ ClutOf(CGao), 0.45 } : Grown{ ClutOf(CStubble), 0.15 };
				return Pick < 0.08 ? Grown{ ClutOf(CStubble), 0.15 } : Grown{};
			}

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				if (!DressOwn(P, S, U4, U5, U6, U7)) return false;
				// (each strip's crop drawn in its strip's own shade and warmth, as its floor is)
				const int K = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				if ((K == CGao || K == CGrain || K == CBush) && SnowLying <= 0.25 && !bHardpan && (S.Kind == KField || S.Kind == KOpen || S.Kind == KRough) && Tilled(S.Crop) && S.Crop != Terrace)
				{
					const Strip St = StripOf(S);
					auto Ch = [](uint8_t C, double F) { return static_cast<uint8_t>(std::clamp(C * F, 0.0, 255.0)); };
					P.Tint.R = Ch(P.Tint.R, St.Shade * (1.0 + 0.24 * St.Warm));
					P.Tint.G = Ch(P.Tint.G, St.Shade * (1.0 + 0.06 * St.Warm));
					P.Tint.B = Ch(P.Tint.B, St.Shade * (1.0 - 0.28 * St.Warm));
				}
				return true;
			}
			bool DressOwn(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const
			{
				const int K = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				const bool bField = (S.Kind == KField || S.Kind == KOpen || S.Kind == KRough) && Tilled(S.Crop);
				Life L;
				double RowA = S.Angle;
				if (bField)
				{
					const Strip St = StripOf(S);
					L = S.Crop == Terrace ? ParcelLife(Terrace, T0()) : ParcelLife(StripCrop(S, St), T0() + St.Jit);
					if (S.Crop != Terrace) RowA = St.Angle;
				}
				const double Lt = 0.94 + 0.12 * U6;
				const float Rows = static_cast<float>(RowA + (U7 < 0.5 ? 0.0 : Pi));
				const bool bSnow = SnowLying > 0.25;
				switch (K)
				{
				case CGao:
					if (bHardpan)
					{
						// (the cracked crust: its plates every way)
						P.Scale = static_cast<float>(0.9 + 0.3 * U4);
						P.ScaleZ = 1.0f;
						P.Tint = Tint(0.97 + 0.06 * U5, 0.97 + 0.05 * U5, 0.96 + 0.04 * U6);
						return true;
					}
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
					// (two rows of plants, half a metre apart, 2.4 m of them: laid on the strip's rows, Grow's lines)
					P.Yaw = Rows;
					P.Scale = static_cast<float>(0.95 + 0.1 * U4);
					if (L.Crop == Mustard)
					{
						P.ScaleZ = static_cast<float>(std::max(0.3, L.High) * (2.2 + 0.6 * U5));
						P.Tint = Flower(L) > 0.3 ? Tint(1.99, 1.6, 0.4) : Tint(Lerp(0.8, 1.5, Smooth(0.6, 1.0, L.Ripe)) * Lt, Lerp(1.05, 1.25, Smooth(0.6, 1.0, L.Ripe)) * Lt, 0.7 * Lt);
					}
					else if (L.Crop == Soy)
					{
						// (the soybeans' leaves yellowing as the pods ripen)
						P.ScaleZ = static_cast<float>(std::max(0.3, L.High) * (1.1 + 0.3 * U5));
						P.Tint = Tint(Lerp(0.72, 1.5, L.Ripe) * Lt, Lerp(0.98, 1.05, L.Ripe) * Lt, Lerp(0.76, 0.7, L.Ripe) * Lt);
					}
					else
					{
						// (the young gaoliang and millet, leafy, knee high; Manchuria's young wheat)
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
						// (the gaoliang's stubs a foot high and thick, grey-buff; standing out of the snow)
						P.ScaleZ = static_cast<float>((bSnow ? 2.5 : 2.0) + 0.5 * U5);
						P.Tint = Tint(1.0 * Lt, 0.94 * Lt, 0.86 * Lt);
					}
					else
					{
						// (the cut rows; under the snow standing out of it, their ridges' crests blown clear)
						P.ScaleZ = static_cast<float>((bSnow ? 1.7 : 0.8) + 0.4 * U5);
						P.Tint = bSnow ? Tint(0.95 + 0.06 * U6, 0.92 + 0.05 * U6, 0.9) : Tint(1.0 + 0.06 * U6, 1.0, 0.95);
					}
					return true;
				case CGrass:
					P.Scale = static_cast<float>(0.85 + 0.6 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.7 * U5);
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
					if (bHardpan)
					{
						// India's baked crust, 2 m of it: its plates (a jittered lattice, some 0.5 m across, each its own shade,
						// curled a little at its edges) and the dark cracks between them
						constexpr int N = 5;
						double PX[N][N], PY[N][N];
						for (int J = 0; J < N; ++J)
							for (int I = 0; I < N; ++I)
							{
								const uint64_t Hs = Hash(0x4A2D, static_cast<uint64_t>(J * N + I));
								const bool bRim = I == 0 || J == 0 || I == N - 1 || J == N - 1;
								PX[J][I] = -1.0 + 0.5 * I + (bRim ? 0.0 : 0.3 * (Unit(Hash(Hs, 1)) - 0.5));
								PY[J][I] = -1.0 + 0.5 * J + (bRim ? 0.0 : 0.3 * (Unit(Hash(Hs, 2)) - 0.5));
							}
						const Tone Crack = Rgb(92, 78, 60);
						for (int J = 0; J + 1 < N; ++J)
							for (int I = 0; I + 1 < N; ++I)
							{
								// the plate: its corners drawn in toward its middle by the crack's half width, raised a hair
								const double MX = 0.25 * (PX[J][I] + PX[J][I + 1] + PX[J + 1][I + 1] + PX[J + 1][I]), MY = 0.25 * (PY[J][I] + PY[J][I + 1] + PY[J + 1][I + 1] + PY[J + 1][I]);
								const Tone Pl = Varied(Rgb(196, 178, 140), 0.92 + 0.12 * Unit(Hash(0x4A2E, static_cast<uint64_t>(J * N + I))), 0.0);
								// (the crack's width its own along each plate, a hairline to a finger's width)
								const double Keep = 0.93 + 0.045 * Unit(Hash(0x4A2F, static_cast<uint64_t>(J * N + I)));
								auto In = [&](double X, double Y, double Z, double (&Out)[3]) {
									Out[0] = MX + (X - MX) * Keep;
									Out[1] = MY + (Y - MY) * Keep;
									Out[2] = Z;
								};
								double A[3], B[3], C[3], D[3];
								In(PX[J][I], PY[J][I], 0.035, A);
								In(PX[J][I + 1], PY[J][I + 1], 0.035, B);
								In(PX[J + 1][I + 1], PY[J + 1][I + 1], 0.035, C);
								In(PX[J + 1][I], PY[J + 1][I], 0.035, D);
								Quad(W, A, B, C, D, Pl);
								// (the crack under it: the earth's dark showing, a plate's width wider)
								const double E0[3] = { PX[J][I], PY[J][I], 0.015 }, E1[3] = { PX[J][I + 1], PY[J][I + 1], 0.015 };
								const double E2[3] = { PX[J + 1][I + 1], PY[J + 1][I + 1], 0.015 }, E3[3] = { PX[J + 1][I], PY[J + 1][I], 0.015 };
								Quad(W, E0, E1, E2, E3, Crack);
							}
						M.Paint = 2;
						M.FarFrom = 40.0f;
						break;
					}
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
							const double Nod = 0.06 + 0.06 * Unit(Hash(Hs, 40)), NA = 2.0 * Pi * Unit(Hash(Hs, 41));
							const double HX = X + Nod * std::cos(NA), HY = Y + Nod * std::sin(NA);
							if (bIndia)
							{
								// (the jowar's head a loose panicle (the art director, 23 November): its branches arching out from
								// the stem's top and drooping, the grain along them; its tip nodding over)
								for (int B = 0; B < 5; ++B)
								{
									const double A = NA + 2.0 * Pi * (B + 0.4 * Unit(Hash(Hs, 50 + B))) / 5.0, Ca = std::cos(A), Sa = std::sin(A);
									const double Z0 = Tall + 0.04 + 0.05 * B, Out = 0.15 + 0.06 * Unit(Hash(Hs, 60 + B)), Fall = 0.14 + 0.1 * Unit(Hash(Hs, 70 + B));
									const double Pt[4][3] = { { X, Y, Z0 }, { X + 0.35 * Out * Ca, Y + 0.35 * Out * Sa, Z0 + 0.05 }, { X + 0.75 * Out * Ca, Y + 0.75 * Out * Sa, Z0 + 0.01 }, { X + Out * Ca, Y + Out * Sa, Z0 - Fall } };
									const double Wd[3] = { 0.035, 0.05, 0.04 };
									Ribbon(W, Pt, Wd, HeadFoot, HeadTop);
								}
								const double Tip[4][3] = { { X, Y, Tall }, { X + 0.3 * (HX - X), Y + 0.3 * (HY - Y), Tall + 0.3 }, { HX, HY, Tall + 0.36 }, { HX + 1.4 * (HX - X), HY + 1.4 * (HY - Y), Tall + 0.2 } };
								const double TW[3] = { 0.04, 0.06, 0.05 };
								Ribbon(W, Tip, TW, HeadFoot, HeadTop);
							}
							else
							{
								// (the head: one compact cluster of grain at the stalk's very top, some 0.3 m long and a hand thick,
								// nodding a little: a slim core and its knots of grain heaped round it, knobbly from every side; the art
								// director, 23 November)
								const double Pt[4][3] = { { X, Y, Tall - 0.06 }, { X + 0.3 * (HX - X), Y + 0.3 * (HY - Y), Tall + 0.06 },
														  { X + 0.7 * (HX - X), Y + 0.7 * (HY - Y), Tall + 0.18 }, { HX, HY, Tall + 0.3 } };
								const double Rd[4] = { 0.02, 0.034, 0.03, 0.01 };
								Spindle(W, Pt, Rd, HeadFoot, HeadTop);
								for (int B = 0; B < 8; ++B)
								{
									const double F = 0.08 + 0.11 * B, A = NA + 2.39996 * B, Bulge = 0.032 + 0.018 * std::sin(Pi * std::min(1.0, F * 1.1));
									const double BR = (0.024 + 0.008 * Unit(Hash(Hs, 50 + B))) * (1.0 - 0.35 * F);
									W.Bead(X + F * (HX - X) + Bulge * std::cos(A), Y + F * (HY - Y) + Bulge * std::sin(A), Tall - 0.03 + 0.32 * F, BR, 1.7 * BR, B % 3 == 0 ? Varied(HeadTop, 1.1, 0.0) : HeadTop, HeadFoot);
								}
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
							if (bIndia) F.Blade(XH - 0.1, Y, 2.45, XH + 0.14, Y, 2.62, 0.3, HeadFoot, HeadTop); // (the panicle's droop)
							else F.Blade(XH, Y, 2.3, XH + 0.02, Y, 2.95, 0.14, HeadFoot, HeadTop);
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
					// a row crop's two rows, 0.5 m apart along x, six plants to a row over 2.4 m (the soybeans', the
					// mustard's, the young gaoliang's by its tint and height): each five broad leaves on their stalks arching
					// out and up in tiers, a pod or a flower over them
					for (int R = 0; R < 2; ++R)
						for (int Pl = 0; Pl < 6; ++Pl)
						{
							const uint64_t Hs = Hash(0xB05A, static_cast<uint64_t>(R * 6 + Pl));
							const double X = -1.0 + 0.4 * Pl + 0.1 * (Unit(Hash(Hs, 1)) - 0.5), Y = -0.25 + 0.5 * R + 0.04 * (Unit(Hash(Hs, 2)) - 0.5);
							const double A0 = 2.0 * Pi * Unit(Hash(Hs, 3));
							for (int B = 0; B < 4; ++B)
							{
								const double A = A0 + 2.0 * Pi * (B + 0.4 * Unit(Hash(Hs, 10 + B))) / 4.0, Tier = static_cast<double>(B % 3);
								const double Z = 0.1 + 0.1 * Tier, Out = 0.17 + 0.07 * Unit(Hash(Hs, 20 + B)) - 0.03 * Tier;
								const double FX = X + 0.04 * std::cos(A), FY = Y + 0.04 * std::sin(A);
								W.Blade(X, Y, 0.0, FX, FY, Z, 0.014, Rgb(76, 96, 48), Rgb(92, 112, 56));
								// (the leaflet: a broad rounded oval, blunt at its tip, a dull green; the art director, 23 November)
								Leaflet(W, FX, FY, Z, FX + Out * std::cos(A), FY + Out * std::sin(A), Z + 0.08 + 0.06 * Unit(Hash(Hs, 30 + B)), 0.15, Rgb(66, 86, 46), Rgb(98, 114, 64));
							}
							if (Pl % 2 == R) W.Bead(X, Y, 0.3 + 0.08 * Unit(Hash(Hs, 40)), 0.035, 0.07, Rgb(140, 132, 70), Rgb(108, 104, 60)); // (pods, or the mustard's flowers by the tint)
						}
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 40.0f;
					break;
				case CStubble:
					if (bManchuria)
					{
						// Manchuria's ridges (its crops sown on them, 0.6 m apart): two low ridges of the black earth along x, 2.4 m
						// of each, the cut stubs along their crests (the gaoliang's by its height); under the snow the crests
						// blown clear, dark lines over the white
						const bool bWhite = SnowLying > 0.25;
						for (int R = 0; R < 2; ++R)
						{
							// (each ridge low and rounded, 3.6 m of it, its ends sunk so the lengths run on as one line; under
							// the snow a soft white mound along it, its ends drawn out long and low (no ledge to throw the low
							// sun's shadow; the art director, 23 November), the stubs' rows what reads)
							const double Y = -0.3 + 0.6 * R;
							const size_t First = M.Near.Points();
							// (bare, lower and broader, its ends drawn out long and low as under the snow: the art director's
							// black logs, 23 November, were these lengths' blunt rounded ends close by)
							Ridge(W, 1.8, bWhite ? 0.04 : 0.07, bWhite ? 0.52 : 0.55, bWhite ? Rgb(210, 214, 222) : Rgb(70, 62, 52), bWhite ? Rgb(220, 224, 231) : Rgb(62, 55, 46), bWhite ? Rgb(226, 230, 236) : Rgb(58, 52, 44), true, 0.7);
							for (size_t P = First; P < M.Near.Points(); ++P) M.Near.Positions[3 * P + 1] = static_cast<float>(M.Near.Positions[3 * P + 1] + Y);
							for (int H = 0; H < 12; ++H)
							{
								const uint64_t Hs = Hash(0x57BF, static_cast<uint64_t>(R * 12 + H));
								const double X = -1.65 + 0.3 * H + 0.05 * (Unit(Hash(Hs, 1)) - 0.5);
								for (int B = 0; B < 3; ++B)
								{
									// (a stub from the crest, standing a hand or more over it)
									const double A = 2.0 * Pi * (B + Unit(Hash(Hs, 2, B))) / 3.0, Tall = 0.2 + 0.08 * Unit(Hash(Hs, 3, B));
									const double Foot = bWhite ? 0.03 : 0.07;
									W.Blade(X, Y, Foot, X + 0.025 * std::cos(A), Y + 0.025 * std::sin(A), Tall + Foot - 0.07, 0.02, Rgb(118, 104, 74), Rgb(170, 152, 108));
								}
							}
						}
						LitUp(M.Near, 0);
						M.Paint = 2;
						M.FarFrom = 35.0f;
						break;
					}
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
					// a clump of sward, full: thirty-odd blades over some 0.8 m, a hand or two high, a denser heart; the
					// season's colour (the north's winter and India's parched months straw-buff)
					const Tone Foot = Mixed(Rgb(122, 108, 78), Rgb(88, 100, 56), Green), Tip = Mixed(Rgb(184, 166, 120), Rgb(146, 154, 92), Green);
					Tuft(W, 0.0, 0.0, 26, 0.4, 0.1, 0.18, 0.1, 0.02, 0x6A55, Foot, Tip);
					Tuft(W, 0.0, 0.0, 10, 0.1, 0.16, 0.12, 0.07, 0.022, 0x6A56, Foot, Tip);
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
				case KPoplar: return bBare ? (SnowLying > 0.25 ? "PoplarSnowBare" : "PoplarBare") : "PoplarLeaf";
				case KWillow: return "Willow";
				case KPine: return "HillPine";
				case KHouse: return "NorthHouse";
				case KFarm: return "Farmyard";
				case KTower: return "TieredTower";
				case KGrave: return "GraveMound";
				case KStack: return "StrawStack";
				case KBoulder: return "DryBoulder";
				case KOutcrop: return "DryOutcrop";
				case KRidge: return SnowLying > 0.25 ? "RidgeSnow" : "BalkRidge";
				case KRidgeShort: return SnowLying > 0.25 ? "RidgeSnowShort" : "BalkRidgeShort";
				case KWallLong: return "TownWall";
				case KWallShort: return "TownWallShort";
				case KGate: return "WallTower";
				case KCave: return "CaveFront";
				default: return nullptr;
				}
			}
			bool KitBuilt(int K) const override { return K == KPoplar || (K >= KRidge && K <= KCave); }
			// (a balk's ridge, 0.12 to 0.14 m: the men's ways cross it, so its line runs on unbroken where they do)
			bool KitLow(int K) const override { return K == KRidge || K == KRidgeShort; }
			void KitPlain(int K, LandField::BaseMesh& M) const override
			{
				if (K != KPoplar && (K < KRidge || K > KCave))
				{
					Region::KitPlain(K, M);
					return;
				}
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				switch (K)
				{
				case KPoplar:
					PoplarMesh(M);
					break;
				case KRidge:
				case KRidgeShort:
				{
					// the balk between two strips as a low ridge, its crest a beaten path (the art director, 23 November: the
					// strips' edges crisp and unbroken from 30 m up): low and broad, grassed in the summer (the clutter's
					// grass through it near), dust in the dry months, each stretch its own shade; its lengths' ends barely
					// sunk, so they run on as one line; under the snow a soft white mound, its crest a shade greyer
					const bool bSnow = SnowLying > 0.25;
					const Tone Dust = Mixed(Soil(), Rgb(160, 142, 106), bManchuria ? 0.3 : 0.5);
					const Tone Crest = bSnow ? Rgb(204, 208, 216) : Mixed(Dust, Rgb(112, 118, 74), 0.5 * Green);
					const Tone Flank = bSnow ? Rgb(218, 222, 229) : Mixed(Varied(Dust, 0.86, 0.0), Rgb(96, 108, 60), 0.8 * Green);
					const Tone Foot = bSnow ? Rgb(226, 230, 236) : Mixed(Varied(Soil(), 0.85, 0.0), Rgb(92, 104, 58), 0.5 * Green);
					const double Half = 0.5 * (K == KRidge ? RidgeLong : RidgeShort);
					{
						Writer W(M.Near);
						// (low and broad near by, so from a man's eye it is a bank of earth, not a pipe: the art director, 23
						// November)
						Ridge(W, Half, bSnow ? 0.14 : 0.09, bSnow ? 0.95 : 1.1, Crest, Flank, Foot, true, 0.15, K == KRidge ? 8 : 3);
					}
					{
						Writer W(M.Far);
						Ridge(W, Half, 0.14, 0.9, Crest, Flank, Foot, bSnow, 0.0);
					}
					M.FarFrom = 50.0f;
					M.Radius = static_cast<float>(Half);
					M.Height = 0.25f;
					break;
				}
				case KWallLong:
				case KWallShort:
					WallPiece(M, K == KWallLong ? WallLong : WallShort);
					break;
				case KGate:
					GateTower(M);
					break;
				default:
					CaveFront(M);
					break;
				}
			}

			/** The rammed earth's own colour: the loess's (Manchuria's walls of the same loess, carted). */
			static Tone EarthWall() { return Rgb(178, 156, 116); }

			/**
			 * The town wall's rammed earth, Long of it along x (its outside toward -y): its faces battered from WallFoot at
			 * the foot to WallTop at the top, laid in their layers, putlog holes in rows where the boards' beams went
			 * through; on top the walk, the breastwork along its outer edge and its merlons, a low lip inside.
			 */
			static void WallPiece(LandField::BaseMesh& M, double Long)
			{
				const double H = 0.5 * Long, B = WallFoot, T = WallTop, Z0 = -0.6, Z1 = WallHigh;
				const Tone C = EarthWall();
				const uint64_t S = Long > 3.0 ? 0x3A11 : 0x3A12;
				for (int Lv = 0; Lv < 2; ++Lv)
				{
					const bool bNear = Lv == 0;
					Writer W(bNear ? M.Near : M.Far);
					const double Layer = bNear ? 0.15 : 0.45;
					Rammed(W, -H, -B, H, -B, -H, -T, H, -T, Z0, Z1, Layer, C, S);              // the outside
					Rammed(W, H, B, -H, B, H, T, -H, T, Z0, Z1, Layer, Varied(C, 0.96, 0.0), S ^ 7); // the inside
					Rammed(W, H, -B, H, B, H, -T, H, T, Z0, Z1, bNear ? 0.3 : 0.9, C, S ^ 9);         // its ends (a fallen stretch's)
					Rammed(W, -H, B, -H, -B, -H, T, -H, -T, Z0, Z1, bNear ? 0.3 : 0.9, C, S ^ 11);
					{
						const double A[3] = { -H, -T, Z1 }, Bq[3] = { H, -T, Z1 }, Cq[3] = { H, T, Z1 }, D[3] = { -H, T, Z1 };
						Quad(W, A, Bq, Cq, D, Varied(C, 0.88, 0.0)); // (the walk, trodden)
					}
					// the breastwork and its merlons along the outer edge; the lip inside
					const Tone Top = Varied(C, 1.04, 0.0);
					W.Box(0.0, -T + 0.2, Z1 + 0.27, H, 0.2, 0.27, 0.0, Top);
					const int N = std::max(1, static_cast<int>(std::lround(Long / 1.6)));
					for (int K = 0; K < N; ++K) W.Box(-H + (K + 0.5) * Long / N, -T + 0.23, Z1 + 0.54 + 0.32, 0.42, 0.23, 0.32, 0.0, Top);
					W.Box(0.0, T - 0.1, Z1 + 0.12, H, 0.1, 0.12, 0.0, Varied(C, 0.94, 0.0));
					if (!bNear) continue;
					// the putlog holes along the boards' courses on the outside, each where its beam went, many filled or
					// weathered away (no even grid; the art director, 23 November)
					for (const double Z0h : { 0.75, 1.55, 2.35 })
					{
						const int Holes = std::max(1, static_cast<int>(std::lround(Long)));
						for (int K = 0; K < Holes; ++K)
						{
							const uint64_t Hh = Hash(S ^ 0x9071, static_cast<uint64_t>(K), static_cast<uint64_t>(Z0h * 100.0));
							if (Unit(Hh) < 0.45) continue;
							const double Z = Z0h + 0.1 * (Unit(Hash(Hh, 1)) - 0.5);
							const double Y = -(B + (T - B) * (Z - Z0) / (Z1 - Z0)) - 0.012;
							const double X = -H + (K + 0.5) * Long / Holes + 0.36 * (Unit(Hash(Hh, 2)) - 0.5);
							const double A[3] = { X - 0.06, Y, Z - 0.06 }, Bq[3] = { X + 0.06, Y, Z - 0.06 }, Cq[3] = { X + 0.06, Y, Z + 0.06 }, D[3] = { X - 0.06, Y, Z + 0.06 };
							Quad(W, A, Bq, Cq, D, Rgb(72, 60, 46));
						}
					}
				}
				M.FarFrom = 150.0f;
				M.Radius = static_cast<float>(std::hypot(H, B));
				M.Height = static_cast<float>(WallHigh + 1.0);
			}

			/**
			 * A tower on the town wall, at a gate's either side or a corner: 7.2 m along the wall (x), 8 m across it, 5.4 m
			 * high, battered and layered as the wall, battlemented round its top; on it a small pavilion of timber and
			 * plaster under a grey tiled hip roof.
			 */
			static void GateTower(LandField::BaseMesh& M)
			{
				const double HX0 = 3.6, HY0 = 4.0, HX1 = 3.25, HY1 = 3.65, Z0 = -0.6, Z1 = 5.4;
				const Tone C = EarthWall();
				for (int Lv = 0; Lv < 2; ++Lv)
				{
					const bool bNear = Lv == 0;
					Writer W(bNear ? M.Near : M.Far);
					const double Layer = bNear ? 0.15 : 0.45;
					Rammed(W, -HX0, -HY0, HX0, -HY0, -HX1, -HY1, HX1, -HY1, Z0, Z1, Layer, C, 0x6A71);
					Rammed(W, HX0, -HY0, HX0, HY0, HX1, -HY1, HX1, HY1, Z0, Z1, Layer, Varied(C, 0.97, 0.0), 0x6A72);
					Rammed(W, HX0, HY0, -HX0, HY0, HX1, HY1, -HX1, HY1, Z0, Z1, Layer, Varied(C, 0.95, 0.0), 0x6A73);
					Rammed(W, -HX0, HY0, -HX0, -HY0, -HX1, HY1, -HX1, -HY1, Z0, Z1, Layer, Varied(C, 0.98, 0.0), 0x6A74);
					{
						const double A[3] = { -HX1, -HY1, Z1 }, B[3] = { HX1, -HY1, Z1 }, Cq[3] = { HX1, HY1, Z1 }, D[3] = { -HX1, HY1, Z1 };
						Quad(W, A, B, Cq, D, Varied(C, 0.88, 0.0));
					}
					// the battlements round the top
					const Tone Top = Varied(C, 1.04, 0.0);
					W.Box(0.0, -HY1 + 0.2, Z1 + 0.27, HX1, 0.2, 0.27, 0.0, Top);
					W.Box(0.0, HY1 - 0.2, Z1 + 0.27, HX1, 0.2, 0.27, 0.0, Top);
					W.Box(-HX1 + 0.2, 0.0, Z1 + 0.27, 0.2, HY1 - 0.4, 0.27, 0.0, Top);
					W.Box(HX1 - 0.2, 0.0, Z1 + 0.27, 0.2, HY1 - 0.4, 0.27, 0.0, Top);
					for (int K = 0; K < 5; ++K)
					{
						const double X = -HX1 + (K + 0.5) * 2.0 * HX1 / 5.0;
						W.Box(X, -HY1 + 0.21, Z1 + 0.76, 0.32, 0.21, 0.22, 0.0, Top);
						W.Box(X, HY1 - 0.21, Z1 + 0.76, 0.32, 0.21, 0.22, 0.0, Top);
					}
					for (int K = 0; K < 5; ++K)
					{
						const double Y = -HY1 + 0.42 + (K + 0.5) * (2.0 * HY1 - 0.84) / 5.0;
						W.Box(-HX1 + 0.21, Y, Z1 + 0.76, 0.21, 0.3, 0.22, 0.0, Top);
						W.Box(HX1 - 0.21, Y, Z1 + 0.76, 0.21, 0.3, 0.22, 0.0, Top);
					}
					// the pavilion: its walls, the posts at its corners, its grey hip roof with its eaves well out
					const double PX = 2.1, PY = 1.7, P0 = Z1, P1 = Z1 + 2.3;
					W.Box(0.0, 0.0, 0.5 * (P0 + P1), PX, PY, 0.5 * (P1 - P0), 0.0, Rgb(196, 184, 160));
					if (bNear)
						for (const double SX : { -1.0, 1.0 })
							for (const double SY : { -1.0, 1.0 }) W.Box(SX * (PX + 0.06), SY * (PY + 0.06), 0.5 * (P0 + P1), 0.12, 0.12, 0.5 * (P1 - P0), 0.0, Rgb(110, 52, 40));
					const double EX = PX + 0.9, EY = PY + 0.9, RX = 1.2, RZ = P1 + 1.7, EZ = P1 - 0.15;
					const Tone Tile = Rgb(84, 84, 88), TileLit = Rgb(96, 96, 100);
					const double E0[3] = { -EX, -EY, EZ }, E1[3] = { EX, -EY, EZ }, E2[3] = { EX, EY, EZ }, E3[3] = { -EX, EY, EZ };
					const double R0[3] = { -RX, 0.0, RZ }, R1[3] = { RX, 0.0, RZ };
					Quad(W, E0, E1, R1, R0, TileLit); // (the roof's front and back slopes, its ends)
					Quad(W, E2, E3, R0, R1, Tile);
					{
						const uint32_t A = W.Vert(E1[0], E1[1], E1[2], 0.7, 0.0, 0.7, Tile), B = W.Vert(E2[0], E2[1], E2[2], 0.7, 0.0, 0.7, Tile), Cv = W.Vert(R1[0], R1[1], R1[2], 0.7, 0.0, 0.7, Tile);
						W.Tri(A, B, Cv);
						const uint32_t D = W.Vert(E3[0], E3[1], E3[2], -0.7, 0.0, 0.7, Tile), E = W.Vert(E0[0], E0[1], E0[2], -0.7, 0.0, 0.7, Tile), F = W.Vert(R0[0], R0[1], R0[2], -0.7, 0.0, 0.7, Tile);
						W.Tri(D, E, F);
					}
					{
						// (the eaves' undersides, shadowed)
						const double U0[3] = { -EX, -EY, EZ - 0.02 }, U1[3] = { -EX, EY, EZ - 0.02 }, U2[3] = { EX, EY, EZ - 0.02 }, U3[3] = { EX, -EY, EZ - 0.02 };
						Quad(W, U0, U1, U2, U3, Rgb(60, 56, 54));
					}
					// the ridge's ends turned up
					if (bNear)
						for (const double SX : { -1.0, 1.0 }) W.Blade(SX * RX, 0.0, RZ - 0.1, SX * (RX + 0.25), 0.0, RZ + 0.45, 0.3, Tile, TileLit);
				}
				M.FarFrom = 250.0f;
				M.Radius = 5.4f;
				M.Height = 9.0f;
			}

			/**
			 * The loess's cave dwellings (yaodong): a cut bank 20 m long and 6.6 m high (its face at y = 0, the hill behind
			 * toward -y, the yard before it toward +y), dug with three arched caves: each its arch's surround, its front of
			 * timber and paper lattice under the arch, its door and its windows; a grey eave along the face above them.
			 */
			static void CaveFront(LandField::BaseMesh& M)
			{
				const double H = CaveHalf, Back = 13.0, Z0 = -1.0, Z1 = 6.6;
				const Tone Face = Rgb(182, 160, 120), Top = Rgb(150, 142, 98);
				for (int Lv = 0; Lv < 2; ++Lv)
				{
					const bool bNear = Lv == 0;
					Writer W(bNear ? M.Near : M.Far);
					// the bank: its face cut upright, in the spade's layers, round its three arches (the arches cut straight
					// into it, the homes' fronts set back in them; the art director, 23 November); its top grassed; its sides
					const double R = 1.7, Spring = 2.5, Crown = Spring + R, Deep = 0.6, Xs[3] = { -6.6, 0.0, 6.6 };
					const double Lay = bNear ? 0.55 : 2.0;
					Rammed(W, H, 0.0, -H, 0.0, H, 0.0, -H, 0.0, Crown, Z1, Lay, Face, 0xCA71);
					Rammed(W, H, 0.0, -H, 0.0, H, 0.0, -H, 0.0, Z0, 0.0, Lay, Face, 0xCA74);
					for (int K = 0; K <= 3; ++K)
					{
						// (the piers between the arches and at the ends)
						const double A = K == 0 ? -H : Xs[K - 1] + R, B = K == 3 ? H : Xs[K] - R;
						Rammed(W, B, 0.0, A, 0.0, B, 0.0, A, 0.0, 0.0, Crown, Lay, Face, 0xCA80 + static_cast<uint64_t>(K));
					}
					auto Flat = [&W](const double (&P)[3], const double (&Q)[3], const double (&Rr)[3], double NX, double NY, double NZ, const Tone& C) {
						// (a triangle turned to face its normal)
						const double UX = Q[0] - P[0], UY = Q[1] - P[1], UZ = Q[2] - P[2], VX = Rr[0] - P[0], VY = Rr[1] - P[1], VZ = Rr[2] - P[2];
						const bool bFlip = (UY * VZ - UZ * VY) * NX + (UZ * VX - UX * VZ) * NY + (UX * VY - UY * VX) * NZ < 0.0;
						const uint32_t I0 = W.Vert(P[0], P[1], P[2], NX, NY, NZ, C), I1 = W.Vert(Q[0], Q[1], Q[2], NX, NY, NZ, C), I2 = W.Vert(Rr[0], Rr[1], Rr[2], NX, NY, NZ, C);
						if (bFlip) W.Tri(I0, I2, I1);
						else W.Tri(I0, I1, I2);
					};
					const Tone Reveal = Rgb(146, 126, 94);
					constexpr int Steps = 8;
					for (const double X : Xs)
					{
						// the spandrels over each arch, the face's own
						for (const double Sd : { -1.0, 1.0 })
						{
							const double C[3] = { X + Sd * R, 0.0, Crown };
							for (int K = 0; K < Steps; ++K)
							{
								const double T0 = 0.5 * Pi * K / Steps, T1 = 0.5 * Pi * (K + 1) / Steps;
								const double P0[3] = { X + Sd * R * std::cos(T0), 0.0, Spring + R * std::sin(T0) };
								const double P1[3] = { X + Sd * R * std::cos(T1), 0.0, Spring + R * std::sin(T1) };
								Flat(C, P0, P1, 0.0, 1.0, 0.0, Varied(Face, 0.98, 0.0));
							}
						}
						// its reveals: the jambs and the arch's soffit, cut back to the front
						for (const double Sd : { -1.0, 1.0 })
						{
							const double A[3] = { X + Sd * R, 0.0, 0.0 }, B[3] = { X + Sd * R, -Deep, 0.0 }, C[3] = { X + Sd * R, -Deep, Spring }, D[3] = { X + Sd * R, 0.0, Spring };
							Flat(A, B, C, -Sd, 0.0, 0.0, Reveal);
							Flat(A, C, D, -Sd, 0.0, 0.0, Reveal);
						}
						for (int K = 0; K < 2 * Steps; ++K)
						{
							const double T0 = Pi * K / (2 * Steps), T1 = Pi * (K + 1) / (2 * Steps), TM = 0.5 * (T0 + T1);
							const double A[3] = { X + R * std::cos(T0), 0.0, Spring + R * std::sin(T0) }, B[3] = { X + R * std::cos(T1), 0.0, Spring + R * std::sin(T1) };
							const double C[3] = { X + R * std::cos(T1), -Deep, Spring + R * std::sin(T1) }, D[3] = { X + R * std::cos(T0), -Deep, Spring + R * std::sin(T0) };
							Flat(A, B, C, -std::cos(TM), 0.0, -std::sin(TM), Varied(Reveal, 0.85, 0.0));
							Flat(A, C, D, -std::cos(TM), 0.0, -std::sin(TM), Varied(Reveal, 0.85, 0.0));
						}
						// a small eave of grey tiles over each arch
						W.Box(X, 0.12, Crown + 0.38, R + 0.55, 0.16, 0.06, 0.0, Rgb(88, 88, 90));
					}
					{
						const double A[3] = { -H, 0.0, Z1 }, B[3] = { H, 0.0, Z1 }, C[3] = { H, -Back, Z1 }, D[3] = { -H, -Back, Z1 };
						Quad(W, D, C, B, A, Top);
					}
					Rammed(W, -H, 0.0, -H, -Back, -H, 0.0, -H, -Back, Z0, Z1, 2.0, Varied(Face, 0.92, 0.0), 0xCA72);
					Rammed(W, H, -Back, H, 0.0, H, -Back, H, 0.0, Z0, Z1, 2.0, Varied(Face, 0.92, 0.0), 0xCA73);
					for (const double X : Xs)
					{
						const double Y0 = -Deep;
						// the front set back in the arch: timber below, the lattice's paper in the arch
						Panel(W, X - R, X + R, Y0, 0.0, Spring, Rgb(118, 86, 58));
						Arch(W, X, Y0, Spring, R, 10, Rgb(214, 204, 174));
						// the door, the windows either side of it
						Panel(W, X - 0.5, X + 0.5, Y0 + 0.03, 0.0, 2.05, Rgb(60, 44, 34));
						Panel(W, X - 1.45, X - 0.75, Y0 + 0.03, 1.0, 2.0, Rgb(206, 196, 166));
						Panel(W, X + 0.75, X + 1.45, Y0 + 0.03, 1.0, 2.0, Rgb(206, 196, 166));
						if (!bNear) continue;
						// the lattice's bars: up the arch, across it, across the windows
						const Tone Bar = Rgb(80, 58, 40);
						for (const double BX : { -1.0, -0.33, 0.33, 1.0 }) Panel(W, X + BX - 0.035, X + BX + 0.035, Y0 + 0.03, Spring, Spring + std::sqrt(std::max(0.0, R * R - BX * BX)) - 0.05, Bar);
						Panel(W, X - R + 0.1, X + R - 0.1, Y0 + 0.03, Spring + 0.7, Spring + 0.76, Bar);
						Panel(W, X - R, X + R, Y0 + 0.03, Spring - 0.05, Spring + 0.04, Bar);
						for (const double SX : { -1.1, 1.1 }) Panel(W, X + SX - 0.03, X + SX + 0.03, Y0 + 0.05, 1.0, 2.0, Bar);
						for (const double SX : { -1.1, 1.1 }) Panel(W, X + SX - 0.35, X + SX + 0.35, Y0 + 0.05, 1.47, 1.53, Bar);
					}
				}
				M.FarFrom = 400.0f;
				M.Radius = static_cast<float>(H + 2.0);
				M.Height = 7.0f;
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
					// (its near mesh from 110 m: the plain's poplars stand in their hundreds along the tracks)
					M.bTree = true;
					M.Paint = 1;
					M.FarFrom = 110.0f;
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
				case KRidge:
				case KRidgeShort:
					M.FarFrom = 50.0f;
					break;
				case KWallLong:
				case KWallShort:
					M.FarFrom = 150.0f;
					Sight(0.5f, 0.5f, 0.95f, 0.0f);
					break;
				case KGate:
					M.FarFrom = 250.0f;
					Sight(0.5f, 0.5f, 0.9f, 0.0f);
					break;
				case KCave:
					M.FarFrom = 400.0f;
					Sight(0.5f, 0.5f, 0.95f, 0.0f);
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
				if (bLoess) Terraces(H);
				if (!bTownWall) return;
				// The rammed-earth wall: walked round its square a step under 2 m at a time; open where a lane, a way, a
				// house, the water or the marsh is, and fallen in its stretches (an old wall). A tower either side of each
				// road's gate and at its corners; along the rest its rammed earth in 6 m and 2 m lengths, the fight's wall
				// under them.
				const double Ca = std::cos(WallTurn), Sa = std::sin(WallTurn), Hf = WallHalf;
				const double Cor[5][2] = { { -Hf, -Hf }, { Hf, -Hf }, { Hf, Hf }, { -Hf, Hf }, { -Hf, -Hf } };
				auto World = [&](int Sd, double T, double& X, double& Y) {
					const double U = Lerp(Cor[Sd][0], Cor[Sd + 1][0], T), V = Lerp(Cor[Sd][1], Cor[Sd + 1][1], T);
					X = WallX + U * Ca - V * Sa;
					Y = WallY + U * Sa + V * Ca;
				};
				const double L = 2.0 * Hf;
				const int N = std::max(1, static_cast<int>(std::ceil(L / 1.8)));
				const double St = L / N;
				// each side's steps, 0 to N (N its corner, the next side's first): 0 built, 1 open for a road, 2 open else
				std::vector<std::vector<int>> Why(4, std::vector<int>(static_cast<size_t>(N) + 1, 2));
				double Walked = 0.0;
				for (int Sd = 0; Sd < 4; ++Sd)
					for (int J = 0; J < N; ++J, Walked += St)
					{
						double X = 0.0, Y = 0.0;
						World(Sd, static_cast<double>(J) / N, X, Y);
						const Spot Q = H.At(X, Y);
						const bool bFallen = Noise(Walked / 60.0, 3.7, Seed ^ 0xFA11E) > 0.45;
						int W = 0;
						if (Q.Kind == KLane || Q.Lane < 2.5) W = 1;
						else if (bFallen || !Q.bLaid || Q.Kind == KWater || Q.Kind == KMarsh || Q.Drawn == 9 || Q.Drawn == 11 || Q.Drawn == 12 || Q.bMan || Q.bVehicle || Q.bNear) W = 2;
						Why[static_cast<size_t>(Sd)][static_cast<size_t>(J)] = W;
					}
				for (int Sd = 0; Sd < 4; ++Sd) Why[static_cast<size_t>(Sd)][static_cast<size_t>(N)] = Why[static_cast<size_t>((Sd + 1) % 4)][0];
				// The towers: at each road's gate either side, and at the corners the wall turns.
				std::vector<std::vector<double>> Towers(4);
				// (a gate's tower as near its road as it will stand, stepping back along the wall from the way's edge)
				auto Tower = [&](int Sd, double At, double Back) {
					for (int Try = 0; Try < (Back == 0.0 ? 1 : 4); ++Try, At += Back)
					{
						double X = 0.0, Y = 0.0;
						World(Sd, At / L, X, Y);
						if (H.House(KitOf(KGate), X, Y, WallTurn + 0.5 * Pi * Sd, 7.2, 8.0))
						{
							Towers[static_cast<size_t>(Sd)].push_back(At);
							return true;
						}
					}
					return false;
				};
				for (int Sd = 0; Sd < 4; ++Sd)
				{
					const std::vector<int>& Wy = Why[static_cast<size_t>(Sd)];
					if (Wy[0] == 0 && Why[static_cast<size_t>((Sd + 3) % 4)][static_cast<size_t>(N) - 1] == 0 && Tower(Sd, 0.0, 0.0)) Towers[static_cast<size_t>((Sd + 3) % 4)].push_back(L);
					for (int J = 0; J < N; ++J)
					{
						if (Wy[static_cast<size_t>(J)] == 0 && Wy[static_cast<size_t>(J) + 1] == 1) Tower(Sd, J * St - 2.4, -1.8);
						if (Wy[static_cast<size_t>(J)] == 1 && Wy[static_cast<size_t>(J) + 1] == 0) Tower(Sd, (J + 1) * St + 2.4, 1.8);
					}
				}
				// The wall along each side between them.
				const LandField::Rgb8 Plain_ = Tint(1.0, 1.0, 1.0);
				for (int Sd = 0; Sd < 4; ++Sd)
				{
					std::vector<int> Wy = Why[static_cast<size_t>(Sd)];
					for (const double At : Towers[static_cast<size_t>(Sd)])
						for (int J = 0; J <= N; ++J)
							if (std::fabs(J * St - At) < 3.2) Wy[static_cast<size_t>(J)] = 3;
					const double Yaw = WallTurn + 0.5 * Pi * Sd;
					for (int J = 0; J <= N;)
					{
						if (Wy[static_cast<size_t>(J)] != 0)
						{
							++J;
							continue;
						}
						int E = J;
						while (E + 1 <= N && Wy[static_cast<size_t>(E) + 1] == 0) ++E;
						// (at a corner the wall runs on to its outer corner; at a tower, into its foot)
						double S0 = J * St - (J == 0 && Why[static_cast<size_t>((Sd + 3) % 4)][static_cast<size_t>(N) - 1] == 0 ? WallFoot : 0.0);
						double S1 = E * St + (E == N && Why[static_cast<size_t>((Sd + 1) % 4)][1] == 0 ? WallFoot : 0.0);
						for (const double Tw : Towers[static_cast<size_t>(Sd)])
						{
							if (J > 0 && Wy[static_cast<size_t>(J) - 1] == 3 && std::fabs(Tw - (J - 1) * St) < 3.3) S0 = std::min(S0, Tw + 3.0);
							if (E < N && Wy[static_cast<size_t>(E) + 1] == 3 && std::fabs(Tw - (E + 1) * St) < 3.3) S1 = std::max(S1, Tw - 3.0);
						}
						J = E + 1;
						if (S1 - S0 < WallShort) continue;
						// the rammed earth's lengths: 6 m while they fit, then 2 m ones, the last flush with the run's end
						double At = S0;
						while (S1 - At >= WallLong)
						{
							double X = 0.0, Y = 0.0;
							World(Sd, (At + 0.5 * WallLong) / L, X, Y);
							H.Plant(KitOf(KWallLong), X, Y, Yaw, 1.0, 1.0, Plain_);
							At += WallLong;
						}
						while (At < S1 - 0.25)
						{
							const double Mid = std::min(At + 0.5 * WallShort, S1 - 0.5 * WallShort);
							double X = 0.0, Y = 0.0;
							World(Sd, Mid / L, X, Y);
							H.Plant(KitOf(KWallShort), X, Y, Yaw, 1.0, 1.0, Plain_);
							At += WallShort;
						}
						// the fight's wall under it, its height and thickness
						std::vector<std::pair<double, double>> Run;
						for (double T = S0; T < S1 + 0.01; T += std::min(1.8, std::max(0.01, S1 - T)))
						{
							double X = 0.0, Y = 0.0;
							World(Sd, T / L, X, Y);
							Run.push_back({ X, Y });
							if (T >= S1) break;
						}
						if (Run.size() >= 2) H.Wall(Run, WallHigh - 0.05, 2.0 * WallTop - 0.1);
					}
				}
			}
			/** Metres from the town wall's line (its square), 1e9 where there is none. */
			double FromWall(double X, double Y) const
			{
				if (!bTownWall) return 1e9;
				const double RX = X - WallX, RY = Y - WallY, Ca = std::cos(WallTurn), Sa = std::sin(WallTurn);
				const double U = RX * Ca + RY * Sa, V = -RX * Sa + RY * Ca;
				const double DU = std::fabs(U) - WallHalf, DV = std::fabs(V) - WallHalf;
				if (DU <= 0.0 && DV <= 0.0) return -std::max(DU, DV);
				return std::hypot(std::max(0.0, DU), std::max(0.0, DV));
			}

			/** The loess terraces' risers: along the foot of every riser the band's slopes are stepped in (Relief), a bank
			 * of the loess's own earth, traced on the height's contours (marching squares, as the Mediterranean's). */
			void Terraces(Host& H)
			{
				const double B = H.Band(), Step_ = 4.0, FW = H.Width(), FH = H.Depth();
				const double X0 = -B, Y0 = -B;
				const int NX = static_cast<int>(std::ceil((FW + 2.0 * B) / Step_)) + 1, NY = static_cast<int>(std::ceil((FH + 2.0 * B) / Step_)) + 1;
				std::vector<float> V(static_cast<size_t>(NX) * NY, -1.0f);
				for (int J = 0; J < NY; ++J)
					for (int I = 0; I < NX; ++I)
					{
						const double X = X0 + I * Step_, Y = Y0 + J * Step_, D = Outside(X, Y);
						if (D < 30.0) continue;
						V[static_cast<size_t>(J) * NX + I] = static_cast<float>(LoessLift(X, Y, D));
					}
				const double L0 = Rise * (1.0 - RiserRun);
				auto LevelBelow = [L0](double E) { return static_cast<int>(std::floor((E - L0) / Rise)); };
				auto KeyOf = [](uint64_t Edge, int Level) { return (Edge << 8) | static_cast<uint64_t>(Level & 0xFF); };
				std::unordered_map<uint64_t, std::pair<double, double>> Where;
				std::unordered_map<uint64_t, std::vector<uint32_t>> Links;
				std::vector<std::pair<uint64_t, uint64_t>> Segs;
				auto Cross = [&](int IA, int JA, int IB, int JB, uint64_t Edge, int Level) {
					const double VA = V[static_cast<size_t>(JA) * NX + IA], VB = V[static_cast<size_t>(JB) * NX + IB];
					const double Lv = L0 + Level * Rise;
					const double T = std::clamp((Lv - VA) / (VB - VA), 0.0, 1.0);
					const uint64_t Key = KeyOf(Edge, Level);
					if (!Where.count(Key)) Where[Key] = { X0 + Lerp(IA, IB, T) * Step_, Y0 + Lerp(JA, JB, T) * Step_ };
					return Key;
				};
				for (int J = 0; J + 1 < NY; ++J)
					for (int I = 0; I + 1 < NX; ++I)
					{
						const size_t K = static_cast<size_t>(J) * NX + I;
						const double C0 = V[K], C1 = V[K + 1], C2 = V[K + NX + 1], C3 = V[K + NX];
						if (C0 < 0.0 || C1 < 0.0 || C2 < 0.0 || C3 < 0.0) continue;
						const int Lo = LevelBelow(std::min({ C0, C1, C2, C3 })) + 1, Hi = LevelBelow(std::max({ C0, C1, C2, C3 }));
						for (int Lv = Lo; Lv <= Hi; ++Lv)
						{
							if (Lv < 0) continue;
							const double At = L0 + Lv * Rise;
							const int Case = (C0 >= At ? 1 : 0) | (C1 >= At ? 2 : 0) | (C2 >= At ? 4 : 0) | (C3 >= At ? 8 : 0);
							if (Case == 0 || Case == 15) continue;
							const uint64_t EB = 2ull * K, EL = 2ull * K + 1ull, ET = 2ull * (K + NX), ER = 2ull * (K + 1) + 1ull;
							auto Bottom = [&]() { return Cross(I, J, I + 1, J, EB, Lv); };
							auto Right = [&]() { return Cross(I + 1, J, I + 1, J + 1, ER, Lv); };
							auto Top = [&]() { return Cross(I, J + 1, I + 1, J + 1, ET, Lv); };
							auto Left = [&]() { return Cross(I, J, I, J + 1, EL, Lv); };
							auto Seg = [&](uint64_t A, uint64_t Bk) {
								Links[A].push_back(static_cast<uint32_t>(Segs.size()));
								Links[Bk].push_back(static_cast<uint32_t>(Segs.size()));
								Segs.push_back({ A, Bk });
							};
							switch (Case)
							{
							case 1: case 14: Seg(Left(), Bottom()); break;
							case 2: case 13: Seg(Bottom(), Right()); break;
							case 3: case 12: Seg(Left(), Right()); break;
							case 4: case 11: Seg(Right(), Top()); break;
							case 6: case 9: Seg(Bottom(), Top()); break;
							case 7: case 8: Seg(Left(), Top()); break;
							case 5:
								Seg(Left(), Top());
								Seg(Bottom(), Right());
								break;
							default:
								Seg(Left(), Bottom());
								Seg(Right(), Top());
								break;
							}
						}
					}
				std::vector<uint8_t> Used(Segs.size(), 0);
				auto Next = [&](uint64_t At, uint32_t From) -> int {
					const auto Found = Links.find(At);
					if (Found == Links.end()) return -1;
					for (const uint32_t S : Found->second)
						if (S != From && !Used[S]) return static_cast<int>(S);
					return -1;
				};
				for (uint32_t S0 = 0; S0 < Segs.size(); ++S0)
				{
					if (Used[S0]) continue;
					Used[S0] = 1;
					std::vector<uint64_t> Keys{ Segs[S0].first, Segs[S0].second };
					for (int Dir = 0; Dir < 2; ++Dir)
					{
						uint32_t Cur = S0;
						for (;;)
						{
							const uint64_t End = Dir == 0 ? Keys.back() : Keys.front();
							const int Nx = Next(End, Cur);
							if (Nx < 0) break;
							Used[static_cast<size_t>(Nx)] = 1;
							Cur = static_cast<uint32_t>(Nx);
							const uint64_t Other = Segs[Cur].first == End ? Segs[Cur].second : Segs[Cur].first;
							if (Dir == 0) Keys.push_back(Other);
							else Keys.insert(Keys.begin(), Other);
						}
					}
					if (Keys.size() < 4) continue;
					// the bank's line, where the terraces are laid; its height the riser's
					std::vector<std::pair<double, double>> Line;
					double WSum = 0.0;
					int WN = 0;
					auto Lay = [&]() {
						// (the contour's corners rounded off twice over, so the bank curves along the hillside as the
						// terraces do, no straight boxes; the art director, 23 November)
						for (int It = 0; It < 2 && Line.size() >= 3; ++It)
						{
							std::vector<std::pair<double, double>> Sm{ Line.front() };
							for (size_t K = 0; K + 1 < Line.size(); ++K)
							{
								const auto& A = Line[K];
								const auto& Bp = Line[K + 1];
								Sm.push_back({ 0.75 * A.first + 0.25 * Bp.first, 0.75 * A.second + 0.25 * Bp.second });
								Sm.push_back({ 0.25 * A.first + 0.75 * Bp.first, 0.25 * A.second + 0.75 * Bp.second });
							}
							Sm.push_back(Line.back());
							Line.swap(Sm);
						}
						if (Line.size() >= 4 && WN > 0) H.Wall(Line, 0.3 + (WSum / WN) * Rise * (1.0 - TreadFall), 0.8);
						Line.clear();
						WSum = 0.0;
						WN = 0;
					};
					for (const uint64_t K : Keys)
					{
						const std::pair<double, double> P = Where[K];
						const double D = Outside(P.first, P.second);
						const double Wt = TerraceW(P.first, P.second, D);
						bool bYard = false;
						for (const Cave& C : Caves) bYard = bYard || InYard(C, P.first, P.second, 3.0);
						if (Wt < 0.45 || D > B - 4.0 || bYard)
						{
							Lay();
							continue;
						}
						Line.push_back(P);
						WSum += Wt;
						++WN;
					}
					Lay();
				}
			}
			Tone WallStone() const override { return Rgb(172, 150, 112); }
			double WallIvy() const override { return 0.0; }

			static void Put(Host& H, LandField::Base B, double X, double Y, double Room, double Yaw, double Scale, double ScaleZ, LandField::Rgb8 Tn)
			{
				if (H.Free(X, Y, Room)) H.Plant(B, X, Y, Yaw, Scale, ScaleZ, Tn);
			}

			/** A poplar: LandField's elm drawn tall and narrow (the art director, 23 November: a column of fine leaves, soft
			 * at its edges; bare in the north's winter, its branches up its stem), 19 to 26 m; yellow in October. */
			/**
			 * The poplar, 20 m high (scaled to its own where it is planted; the art director, 23 November): its pale grey
			 * trunk to near its tip; in leaf a soft column of small leaf clusters from a quarter of the way up straight to
			 * the top, rounded in at its foot, widest a third of the way up, drawn to a point, its clusters lit as one
			 * rounded mass; bare, its branches held close up the trunk and a fine grey mass of twigs along them. Far off the
			 * same column whole, its edge stippled soft; bare, the twigs' grey haze.
			 */
			void PoplarMesh(LandField::BaseMesh& M) const
			{
				constexpr double Tall = 20.0, Foot = 0.24 * Tall, Wide = 1.75;
				auto Rad = [](double V) { return Wide * std::sqrt(Smooth(0.0, 0.16, V)) * std::pow(std::max(0.0, 1.0 - std::pow(V, 2.3)), 0.8); };
				auto ZOf = [](double V) { return Foot + V * (Tall - Foot); };
				const Tone Bark = Rgb(158, 156, 146), BarkFoot = Rgb(112, 108, 98), Twig = Rgb(136, 132, 128), TwigTip = Rgb(160, 156, 152);
				Writer W(M.Near);
				const double TP[4][3] = { { 0.0, 0.0, -0.3 }, { 0.0, 0.0, 0.3 * Tall }, { 0.0, 0.0, 0.65 * Tall }, { 0.0, 0.0, 0.93 * Tall } };
				const double TR[4] = { 0.3, 0.2, 0.12, 0.03 };
				Spindle(W, TP, TR, BarkFoot, Bark);
				if (!bBare)
				{
					// the leaf column: a dark heart of shaded leaves (so what shows between the clusters is leaf, not sky), and
					// clusters spiralled up it, most at its skin, some within (darker), each two small leaves of a few
					// hand-widths crossed, their normals the column's, so the whole lights as one rounded mass
					Column(W, Rad, ZOf, 0.7, [](double V, double U) { return Mixed(Rgb(34, 48, 24), Rgb(52, 70, 34), Clamp01(0.3 + 0.5 * V + 0.2 * U)); }, 0x9075);
					constexpr int N = 560;
					for (int I = 0; I < N; ++I)
					{
						const uint64_t Hs = Hash(0x9070, static_cast<uint64_t>(I));
						const double V = Clamp01((I + Unit(Hash(Hs, 1))) / N * 1.02 - 0.01);
						const double A = 2.39996 * I + 0.4 * (Unit(Hash(Hs, 2)) - 0.5);
						const bool bIn = Unit(Hash(Hs, 3)) < 0.16;
						const double R0 = Rad(V), Fr = bIn ? 0.35 + 0.35 * Unit(Hash(Hs, 4)) : 0.78 + 0.26 * Unit(Hash(Hs, 4));
						const double Ca = std::cos(A), Sa = std::sin(A);
						const double CX = Fr * R0 * Ca, CY = Fr * R0 * Sa, CZ = ZOf(V) + 0.3 * (Unit(Hash(Hs, 5)) - 0.5);
						double NX = Ca, NY = Sa, NZ = 0.3 + 0.6 * (V - 0.5);
						const double NN = std::sqrt(NX * NX + NY * NY + NZ * NZ);
						NX /= NN; NY /= NN; NZ /= NN;
						const double Light = Clamp01(0.3 + 0.45 * V + 0.3 * Unit(Hash(Hs, 6))) * (bIn ? 0.55 : 1.0);
						const Tone C = Unit(Hash(Hs, 7)) < 0.08 ? Rgb(112, 124, 84) : Mixed(Rgb(44, 62, 30), Rgb(92, 112, 54), Light);
						const double Sz = (0.3 + 0.16 * Unit(Hash(Hs, 8))) * (0.65 + 0.35 * R0 / Wide);
						// (its two leaves: across the column and up it, each turned a little)
						const double UX = -Sa, UY = Ca, UZ = 0.0;
						const double VX = NY * UZ - NZ * UY, VY = NZ * UX - NX * UZ, VZ = NX * UY - NY * UX;
						for (int L = 0; L < 2; ++L)
						{
							const double Th = (L == 0 ? 0.0 : 0.5 * Pi) + 0.7 * (Unit(Hash(Hs, 10 + L)) - 0.5), Ct = std::cos(Th), St = std::sin(Th);
							const double AX = Ct * UX + St * VX, AY = Ct * UY + St * VY, AZ = Ct * UZ + St * VZ;
							const double BX = NY * AZ - NZ * AY, BY = NZ * AX - NX * AZ, BZ = NX * AY - NY * AX;
							const double Out = 0.04 * L, PX = CX + Out * NX, PY = CY + Out * NY, PZ = CZ + Out * NZ;
							const Tone CL = Varied(C, 0.94 + 0.12 * Unit(Hash(Hs, 20 + L)), 0.0);
							const uint32_t P0 = W.Vert(PX + Sz * AX, PY + Sz * AY, PZ + Sz * AZ, NX, NY, NZ, CL);
							const uint32_t P1 = W.Vert(PX + 0.62 * Sz * BX, PY + 0.62 * Sz * BY, PZ + 0.62 * Sz * BZ, NX, NY, NZ, CL);
							const uint32_t P2 = W.Vert(PX - Sz * AX, PY - Sz * AY, PZ - Sz * AZ, NX, NY, NZ, CL);
							const uint32_t P3 = W.Vert(PX - 0.62 * Sz * BX, PY - 0.62 * Sz * BY, PZ - 0.62 * Sz * BZ, NX, NY, NZ, CL);
							W.Tri(P0, P1, P2);
							W.Tri(P0, P2, P3);
						}
					}
				}
				else
				{
					// bare: its branches from the trunk all the way up, steeply up and close in (the column's own outline),
					// and their fine twigs, grey
					constexpr int Limbs = 30;
					for (int I = 0; I < Limbs; ++I)
					{
						const uint64_t Hs = Hash(0x9072, static_cast<uint64_t>(I));
						const double V0 = Clamp01((I + Unit(Hash(Hs, 1))) / Limbs * 0.9 - 0.04);
						const double A = 2.39996 * I, Ca = std::cos(A), Sa = std::sin(A);
						const double Z0 = ZOf(V0) - 0.6, Up = 4.0 + 5.0 * (1.0 - V0) * Unit(Hash(Hs, 2));
						const double V1 = Clamp01((Z0 + Up - Foot) / (Tall - Foot)), Out = 0.85 * Rad(V1);
						const double EX = Out * Ca, EY = Out * Sa, EZ = std::min(Tall - 0.3, Z0 + Up);
						W.Blade(0.0, 0.0, Z0, EX, EY, EZ, 0.09, Twig, TwigTip);
						for (int T = 0; T < 9; ++T)
						{
							const uint64_t Ht = Hash(Hs, 30 + T);
							const double F = 0.2 + 0.8 * Unit(Hash(Ht, 1));
							const double SX = F * EX, SY = F * EY, SZ = Z0 + F * (EZ - Z0);
							const double TA = A + 1.6 * (Unit(Hash(Ht, 2)) - 0.5), Ln = 0.9 + 1.1 * Unit(Hash(Ht, 3));
							const double VT = Clamp01((SZ - Foot) / (Tall - Foot)), Lim = std::max(0.2, Rad(VT) - std::hypot(SX, SY));
							const double Rch = std::min(0.45 * Ln, Lim);
							W.Blade(SX, SY, SZ, SX + Rch * std::cos(TA), SY + Rch * std::sin(TA), SZ + 0.85 * Ln, 0.035, Twig, TwigTip);
						}
					}
				}
				// far off: the column whole, a ring of eight every few metres, each point pushed in or out a little so its
				// edge is soft, the trunk under it; bare, narrower, the twigs' grey haze
				Writer Fw(M.Far);
				const double FP[4][3] = { { 0.0, 0.0, -0.3 }, { 0.0, 0.0, 0.5 * Foot }, { 0.0, 0.0, Foot }, { 0.0, 0.0, Foot + 1.0 } };
				const double FR[4] = { 0.3, 0.24, 0.2, 0.18 };
				Spindle(Fw, FP, FR, BarkFoot, Bark);
				if (!bBare)
					Column(Fw, Rad, ZOf, 1.0, [](double V, double U) { return Mixed(Rgb(46, 64, 31), Rgb(84, 104, 50), Clamp01(0.35 + 0.5 * V) * (0.9 + 0.2 * U)); }, 0x9073);
				else
					for (int I = 0; I < 12; ++I)
					{
						// (bare: its limbs as a fan of thin grey strokes up the column's outline, the sky through them)
						const double V0 = 0.02 + 0.07 * (I % 4), A = 2.39996 * I, V1 = 0.55 + 0.4 * Unit(Hash(0x9076, static_cast<uint64_t>(I)));
						const double Out = 0.8 * Rad(0.5 * V1);
						Fw.Blade(0.0, 0.0, ZOf(V0), Out * std::cos(A), Out * std::sin(A), ZOf(V1), 0.32, Twig, Mixed(Twig, Rgb(170, 166, 168), 0.4));
					}
			}

			/** A column round the tree's axis: a ring of eight every few metres to Rad and ZOf (V 0 to 1), each point pushed
			 * in or out a little so its edge is soft, drawn to a point; Share of the radius; its colour by V and a jitter. */
			template <typename RadF, typename ZF, typename ColF>
			static void Column(Writer& W, RadF Rad, ZF ZOf, double Share, ColF Col, uint64_t Salt)
			{
				constexpr int Rings = 9, Round = 8;
				const double Vs[Rings] = { 0.0, 0.06, 0.16, 0.3, 0.45, 0.6, 0.74, 0.87, 0.96 };
				uint32_t Id[Rings][Round];
				for (int K = 0; K < Rings; ++K)
					for (int J = 0; J < Round; ++J)
					{
						const double V = Vs[K], A = 2.0 * Pi * (J + 0.5 * (K % 2)) / Round, Ca = std::cos(A), Sa = std::sin(A);
						const double Push = 0.86 + 0.24 * Unit(Hash(Salt, static_cast<uint64_t>(K * Round + J)));
						const double R0 = Share * Rad(std::max(0.03, V)) * Push;
						const double NZ = 0.25 + 0.5 * (V - 0.5), NN = std::sqrt(1.0 + NZ * NZ);
						Id[K][J] = W.Vert(R0 * Ca, R0 * Sa, ZOf(V), Ca / NN, Sa / NN, NZ / NN, Col(V, Unit(Hash(Salt ^ 0x77, static_cast<uint64_t>(K * Round + J)))));
					}
				const uint32_t Apex = W.Vert(0.0, 0.0, ZOf(1.0), 0.0, 0.0, 1.0, Col(1.0, 0.5));
				for (int K = 0; K + 1 < Rings; ++K)
					for (int J = 0; J < Round; ++J)
					{
						const int J1 = (J + 1) % Round;
						W.Tri(Id[K][J], Id[K][J1], Id[K + 1][J1]);
						W.Tri(Id[K][J], Id[K + 1][J1], Id[K + 1][J]);
					}
				for (int J = 0; J < Round; ++J) W.Tri(Id[Rings - 1][J], Id[Rings - 1][(J + 1) % Round], Apex);
			}

			void Poplar(Host& H, double X, double Y, double Room, double Yaw, double USize) const
			{
				if (!H.Free(X, Y, Room)) return;
				const double High = 19.0 + 7.0 * Clamp01(USize);
				const LandField::Rgb8 Tn = Month == 10 ? Tint(1.3, 1.16, 0.66) : Tint(0.96, 1.0, 0.94);
				const double Fat = 0.9 + 0.2 * Unit(Hash(static_cast<uint64_t>(std::llround(X * 3.0)), static_cast<uint64_t>(std::llround(Y * 3.0)), 0x9071));
				H.Plant(KitOf(KPoplar), X, Y, Yaw, High / 20.0 * Fat, High / 20.0, Tn);
			}

			/**
			 * The balks between the strips as low ridges, and the fields' edges where two fields meet, laid in their lines
			 * over the field and a little of its band so the strips stay crisp from above and afar (the clutter's grass on
			 * the balks round the eye): a 16 m length where the whole of it is tilled, 6 m ones at a line's ends.
			 */
			void Ridges(Host& H)
			{
				if (bIndia) return;
				constexpr double Reach = 60.0;
				const double XA = -Reach, XB = H.Width() + Reach, YA = -Reach, YB = H.Depth() + Reach;
				const LandField::Rgb8 Plain_ = Tint(1.0, 1.0, 1.0);
				auto Tilled_ = [this](const Spot& Q) { return Q.bLaid && (Q.Kind == KField || Q.Kind == KOpen || Q.Kind == KRough) && Tilled(Q.Crop) && Q.Crop != Terrace && Q.Lane > 1.0 && !Q.bNear; };
				// one line (bAlongY: at X = C, from Y = A to B; else at Y = C, from X = A to B), laid where Ok says
				auto Line = [&](bool bAlongY, double C, double A, double B, auto&& Ok) {
					const double Yaw = bAlongY ? 0.5 * Pi : 0.0;
					auto At = [&](double T, double& X, double& Y) {
						X = bAlongY ? C : T;
						Y = bAlongY ? T : C;
					};
					const int N = static_cast<int>(std::floor((B - A) / RidgeLong));
					if (N < 1) return;
					std::vector<char> In(static_cast<size_t>(N) + 1);
					for (int K = 0; K <= N; ++K)
					{
						double X = 0.0, Y = 0.0;
						At(A + K * RidgeLong, X, Y);
						In[static_cast<size_t>(K)] = Ok(X, Y) ? 1 : 0;
					}
					for (int K = 0; K < N; ++K)
					{
						const double T0 = A + K * RidgeLong;
						const bool bA = In[static_cast<size_t>(K)] != 0, bB = In[static_cast<size_t>(K) + 1] != 0;
						if (bA && bB)
						{
							double X = 0.0, Y = 0.0;
							At(T0 + 0.5 * RidgeLong, X, Y);
							if (H.Free(X, Y, 0.4, true)) H.Plant(KitOf(KRidge), X, Y, Yaw, 1.0, 1.0, Plain_);
							continue;
						}
						if (!bA && !bB) continue;
						// (a line's end: the 6 m lengths that lie in it)
						for (const double Off : { 3.0, 8.0, 13.0 })
						{
							double X = 0.0, Y = 0.0, XE = 0.0, YE = 0.0;
							At(T0 + Off + (bA ? -3.0 : 3.0), XE, YE);
							At(T0 + Off, X, Y);
							if (Ok(XE, YE) && Ok(X, Y) && H.Free(X, Y, 0.4, true)) H.Plant(KitOf(KRidgeShort), X, Y, Yaw, 1.0, 1.0, Plain_);
						}
					}
				};
				const int64_t I0 = static_cast<int64_t>(std::floor(XA / StripCell)), I1 = static_cast<int64_t>(std::floor(XB / StripCell));
				const int64_t J0 = static_cast<int64_t>(std::floor(YA / StripCell)), J1 = static_cast<int64_t>(std::floor(YB / StripCell));
				for (int64_t J = J0; J <= J1; ++J)
					for (int64_t I = I0; I <= I1; ++I)
					{
						const double BX0 = I * StripCell, BY0 = J * StripCell;
						const Block Bk = BlockOf(BX0 + 1.0, BY0 + 1.0);
						// the strips' edges, along the block
						const double U0 = std::max(Bk.bNS ? BX0 : BY0, Bk.bNS ? XA : YA), U1 = std::min((Bk.bNS ? BX0 : BY0) + StripCell, Bk.bNS ? XB : YB);
						const double V0 = std::max(Bk.bNS ? BY0 : BX0, Bk.bNS ? YA : XA), V1 = std::min((Bk.bNS ? BY0 : BX0) + StripCell, Bk.bNS ? YB : XB);
						if (U1 <= U0 || V1 <= V0) continue;
						for (int64_t K = static_cast<int64_t>(std::floor(U0 / Bk.W)) - 1; K <= static_cast<int64_t>(std::ceil(U1 / Bk.W)) + 1; ++K)
						{
							const double U = EdgeOf(Bk, K);
							if (U < U0 + 0.8 || U > U1 - 0.8) continue;
							Line(Bk.bNS, U, V0, V1, [&](double X, double Y) { return Tilled_(H.At(X, Y)); });
						}
						// the fields' edges, both ways, where two tilled fields meet
						for (int Ax = 0; Ax < 2; ++Ax)
						{
							const bool bAlongY = Ax == 0;
							const double C0 = bAlongY ? BX0 : BY0, A = std::max(bAlongY ? BY0 : BX0, bAlongY ? YA : XA), B = std::min((bAlongY ? BY0 : BX0) + StripCell, bAlongY ? YB : XB);
							if (B <= A) continue;
							for (int K = 0; K < 4; ++K)
							{
								const double C = C0 + K * FieldLine;
								if (C < (bAlongY ? XA : YA) || C > (bAlongY ? XB : YB)) continue;
								Line(bAlongY, C, A, B, [&](double X, double Y) {
									const double DX = bAlongY ? 2.0 : 0.0, DY = bAlongY ? 0.0 : 2.0;
									const Spot P = H.At(X - DX, Y - DY), Q = H.At(X + DX, Y + DY);
									return P.Key != Q.Key && Tilled_(P) && Tilled_(Q);
								});
							}
						}
					}
			}

			/** The cave dwellings in their cut banks (their yards cut in Relief). */
			void CaveHomes(Host& H)
			{
				for (const Cave& C : Caves) H.Plant(KitOf(KCave), C.X, C.Y, std::atan2(-C.OX, C.OY), 1.0, 1.0, Tint(1.0, 1.0, 1.0));
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
							const double Size = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Miss = R.Next();
							if (Noise(Walked / 160.0, static_cast<double>(L) * 5.1 + Side * 1.7, Seed ^ 0xA7E) < 0.05 || Miss < 0.15) continue;
							const double Off = Half + 2.2;
							const double X = Line[K].first - DY / DL * Side * Off, Y = Line[K].second + DX / DL * Side * Off;
							const Spot Q = H.At(X, Y);
							if (Q.Kind == KVillage || Q.Kind == KWater || Q.Kind == KYard || Q.Lane < 0.8 || FromWall(X, Y) < 6.0) continue;
							Poplar(H, X, Y, 1.5, Yaw, Size);
						}
					}
				}
			}

			void Plants(Host& H) override
			{
				Ridges(H);
				CaveHomes(H);
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
						if (S.Kind == KWater || S.Kind == KLane || FromWall(X, Y) < 6.0) continue;
						// (every draw in a statement of its own, in a set order: g++ and clang take a call's arguments in
						// different orders)
						const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Sc = R.Range(0.8, 1.2), Sz = R.Range(0.85, 1.15), Lf = R.Range(0.9, 1.1);
						const LandField::Rgb8 Leaf = Tint(Lf, Lf, Lf);
						// (India's groves of broad trees stood in by the willows; China's and Manchuria's poplars)
						auto Tall = [&]() {
							if (bIndia) Put(H, KitOf(KWillow), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							else Poplar(H, X, Y, 1.5, Yaw, (Sc - 0.8) / 0.4);
						};
						switch (S.Kind)
						{
						case KWoods:
							// (the plain's groves open, some 25 trees a hectare: a wood here is a village's grove, not a forest,
							// and the poplars along every track are many already)
							if (Keep < 0.11) Tall();
							else if (Keep < 0.16) Put(H, bIndia ? LandField::Base::Ash : KitOf(KWillow), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
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
							if (Keep < 0.035) Tall();
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
							else if (Keep < 0.006 + (bOpen ? 0.004 : 0.0)) Tall();
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
			std::vector<Exit> Exits;
			std::vector<Cave> Caves;
			int Month = 7;
			bool bIndia = false, bManchuria = false, bHills = false, bTownWall = false, bLoess = false, bHardpan = false, bBare = false;
			double Green = 1.0, MeanZ = 0.0, SnowLying = 0.0;
			double WallX = 0.0, WallY = 0.0, WallHalf = 0.0, WallTurn = 0.0;
		};
	}

	std::unique_ptr<Region> MakePlain(const LandField::Field& F, uint64_t S) { return std::make_unique<Plain>(F, S); }
}
