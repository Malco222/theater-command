#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>

/**
 * The desert of the North African battles (Gazala, Tobruk, Sollum, Alamein, southern Tunisia; 1941-43) and the Middle
 * East's: stone, not dunes. The look-targets research (production/environments-1122/look-targets.md, section 1) sets it:
 *
 *   - a pale stony plain (serir) of fawn gravel, its pebbles half pale limestone and half varnished near black, with thin
 *     sand sheets in the hollows and bare rocky hamada on the rises and the rules' rough ground;
 *   - wadis 15 to 40 m wide with flat sandy floors, where the scrub grows thicker (under a metre deep inside the field,
 *     where the fight reads its own heights, deeper beyond it); low swells over hundreds of metres; a low escarpment on
 *     the far horizon;
 *   - camel thorn on its hummock metres apart, never continuous; dry grass tufts; pebbles and flat stones round the eye;
 *   - braided bands of vehicle ruts beside the tracks; telegraph poles 50 m apart along the main one; sangars of piled
 *     stone where the rules dig fieldworks; flat-roofed houses packed in streets round the town, a domed tomb on a rise;
 *     date palms only in southern Tunisia's oases.
 *
 * Its kit is the Mediterranean thread's battle models (Content/Models/Med, from battle-models/med).
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Serir = 0, // the stony plain: gravel and flints over hard sand
			Sand = 1,  // a sand sheet (the parcels' word only: the paint follows the land, not the parcels)
			Scrub = 2, // a flat of camel thorn and saltbush
			Rock = 3   // hamada: bare rock and rubble
		};

		enum Slot : int
		{
			KThorn = 0,
			KBoulder = 1,
			KHouse = 2,
			KHouseStone = 3,
			KSangar = 4,
			KPole = 5,
			KPalm = 6,
			KStones = 7,
			KHouseTwo = 8,
			KDome = 9
		};

		enum Clut : int
		{
			CGrass = 0,   // a dry grass tuft
			CPebbles = 1, // a few pebbles, fist-sized and smaller
			CSlab = 2,    // a flat stone a forearm across
			CTwigs = 3,   // a small dead shrub on its sand hummock, its sand tail behind it
			CGravel = 4   // a scatter of gravel, thumbnail-sized: the serir's speckle round the eye
		};

		LandField::Base KitOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K); }
		LandField::Base ClutOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Clut0) + K); }

		// (written so the ground lands, lit at noon, on the art director's screen target for the desert: sand about
		// 190,172,140, red over green 1.08-1.12, blue over green 0.78-0.84; the sky's blue and the haze pale it, so the
		// paint is darker and warmer than what is seen)
		const Tone SerirMid = Rgb(176, 150, 110);
		const Tone SerirPale = Rgb(194, 174, 134);
		const Tone Varnish = Rgb(100, 76, 54);
		const Tone SandSheet = Rgb(198, 174, 130);
		const Tone Limestone = Rgb(192, 180, 152);
		const Tone Dust = Rgb(196, 178, 142);
		const Tone Rut = Rgb(148, 128, 96);
		const Tone LimeUnder = Rgb(150, 136, 110);
		const Tone VarnishUnder = Rgb(112, 96, 76);
		/** Where the prevailing north-west wind lays a sand tail: south-east of each stone and bush (radians from east). */
		constexpr double Lee = -0.25 * Pi;

		/** A small stone of 15 triangles: a squat cone of five sides, its foot sunk. */
		void Chip(Writer& W, double CX, double CY, double R, double H, uint64_t S, const Tone& Lit, const Tone& Under)
		{
			const double TX = CX + 0.2 * R * (Unit(Hash(S, 1)) - 0.5), TY = CY + 0.2 * R * (Unit(Hash(S, 2)) - 0.5);
			const uint32_t Top = W.Vert(TX, TY, H, 0.0, 0.0, 1.0, Lit);
			uint32_t Up[5], Down[5];
			for (int K = 0; K < 5; ++K)
			{
				const double A = 2.0 * Pi * (K + 0.3 * Unit(Hash(S, 10 + K))) / 5.0;
				const double Push = 0.75 + 0.5 * Unit(Hash(S, 20 + K));
				const double X = std::cos(A), Y = std::sin(A);
				Up[K] = W.Vert(CX + R * Push * X, CY + R * Push * Y, 0.45 * H, X * 0.7, Y * 0.7, 0.7, Mixed(Under, Lit, 0.7));
				Down[K] = W.Vert(CX + 1.1 * R * Push * X, CY + 1.1 * R * Push * Y, -0.3 * H, X, Y, 0.0, Under);
			}
			for (int K = 0; K < 5; ++K)
			{
				const int N = (K + 1) % 5;
				W.Tri(Top, Up[K], Up[N]);
				W.Tri(Up[K], Down[K], Down[N]);
				W.Tri(Up[K], Down[N], Up[N]);
			}
		}

		/** A grain of gravel: a three-sided pyramid of 3 triangles, its foot in the ground. */
		void Grain(Writer& W, double CX, double CY, double R, double H, uint64_t S, const Tone& Top, const Tone& Side)
		{
			const uint32_t Apex = W.Vert(CX, CY, H, 0.0, 0.0, 1.0, Top);
			uint32_t Foot[3];
			const double A0 = 2.0 * Pi * Unit(Hash(S, 1));
			for (int K = 0; K < 3; ++K)
			{
				const double A = A0 + 2.0 * Pi * K / 3.0, X = std::cos(A), Y = std::sin(A);
				Foot[K] = W.Vert(CX + R * X, CY + R * Y, -0.2 * H, X * 0.8, Y * 0.8, 0.6, Side);
			}
			for (int K = 0; K < 3; ++K) W.Tri(Apex, Foot[K], Foot[(K + 1) % 3]);
		}

		/** A sand tail in the lee of a stone or a bush: a low ridge along +X from Start to Start + Long, Wide across at its
		 * root, tapering out; Low high. 6 triangles. */
		void Tail(Writer& W, double Start, double Long, double Wide, double Low, const Tone& Ridge, const Tone& Flank)
		{
			const uint32_t A = W.Vert(Start, 0.0, Low, 0.0, 0.0, 1.0, Ridge);
			const uint32_t L = W.Vert(Start - 0.1 * Long, 0.5 * Wide, -0.02, 0.0, 0.5, 0.85, Flank);
			const uint32_t R = W.Vert(Start - 0.1 * Long, -0.5 * Wide, -0.02, 0.0, -0.5, 0.85, Flank);
			const uint32_t M = W.Vert(Start + 0.45 * Long, 0.0, 0.55 * Low, 0.1, 0.0, 1.0, Ridge);
			const uint32_t ML = W.Vert(Start + 0.4 * Long, 0.3 * Wide, -0.02, 0.0, 0.4, 0.9, Flank);
			const uint32_t MR = W.Vert(Start + 0.4 * Long, -0.3 * Wide, -0.02, 0.0, -0.4, 0.9, Flank);
			const uint32_t E = W.Vert(Start + Long, 0.0, -0.02, 0.2, 0.0, 0.98, Flank);
			W.Tri(A, L, ML);
			W.Tri(A, ML, M);
			W.Tri(A, M, MR);
			W.Tri(A, MR, R);
			W.Tri(M, ML, E);
			W.Tri(M, E, MR);
		}

		/** A kit house's model drawn round its own footprint's middle (the kit's houses stand off their origins). */
		void Centre(LandField::BaseMesh& M)
		{
			double X0 = 1e9, X1 = -1e9, Y0 = 1e9, Y1 = -1e9;
			const std::vector<float>& P = M.Near.Positions;
			for (size_t K = 0; K + 2 < P.size(); K += 3)
			{
				X0 = std::min(X0, static_cast<double>(P[K]));
				X1 = std::max(X1, static_cast<double>(P[K]));
				Y0 = std::min(Y0, static_cast<double>(P[K + 1]));
				Y1 = std::max(Y1, static_cast<double>(P[K + 1]));
			}
			if (X0 > X1) return;
			const float CX = static_cast<float>(0.5 * (X0 + X1)), CY = static_cast<float>(0.5 * (Y0 + Y1));
			for (LandField::Mesh* Each : { &M.Near, &M.Far, &M.Cards, &M.Sprigs })
				for (size_t K = 0; K + 2 < Each->Positions.size(); K += 3)
				{
					Each->Positions[K] -= CX;
					Each->Positions[K + 1] -= CY;
				}
		}

		class Desert final : public Region
		{
		public:
			Desert(const LandField::Field& F, uint64_t S) : Region(F, S)
			{
				// (spring after the winter rains: a thin green flush in the hollows, February to April; else straw and grey)
				bSpring = F.Month >= 2 && F.Month <= 4;
				// (southern Tunisia's oases have their date palms: Gabès, Mareth, El Hamma)
				bPalms = F.Lat < 34.6 && F.Lon < 12.0 && F.Lon > 7.0;
				// The wadis read once onto a 4 m grid over the field and 320 m round it (the paint and the track marks ask
				// them many times a frame: two noises each is too dear), straight from the noise beyond it.
				GX0 = -320.0;
				GY0 = -320.0;
				GNX = static_cast<int>((F.Width() + 640.0) / GridStep) + 2;
				GNY = static_cast<int>((F.Depth() + 640.0) / GridStep) + 2;
				Grid.resize(static_cast<size_t>(GNX) * static_cast<size_t>(GNY));
				for (int J = 0; J < GNY; ++J)
					for (int I = 0; I < GNX; ++I) Grid[static_cast<size_t>(J) * static_cast<size_t>(GNX) + static_cast<size_t>(I)] = static_cast<float>(WadiNoise(GX0 + I * GridStep, GY0 + J * GridStep));
			}
			Look Which() const override { return Look::Desert; }

			// ---------------------------------------------------------------- the land

			/** How far a place lies outside the laid field (0 inside). */
			double Outside(double X, double Y) const
			{
				const double Dx = std::max({ 0.0, -X, X - Fd.Width() }), Dy = std::max({ 0.0, -Y, Y - Fd.Depth() });
				return std::sqrt(Dx * Dx + Dy * Dy);
			}
			/** A wadi here: 1 on its floor's middle, 0 out on the plain. Its lines wander some 650 m apart; 15 to 40 m across. */
			double WadiNoise(double X, double Y) const
			{
				const double Warp = 0.35 * Noise(X / 230.0, Y / 230.0, Seed ^ 0xA2);
				const double N = Noise(X / 640.0 + Warp, Y / 640.0, Seed ^ 0xA1);
				return Smooth(0.032, 0.011, std::fabs(N));
			}
			double Wadi(double X, double Y) const
			{
				const double U = (X - GX0) / GridStep, V = (Y - GY0) / GridStep;
				if (!(U >= 0.0 && V >= 0.0 && U < GNX - 1.0 && V < GNY - 1.0)) return WadiNoise(X, Y);
				const int I = static_cast<int>(U), J = static_cast<int>(V);
				const double FU = U - I, FV = V - J;
				const size_t K = static_cast<size_t>(J) * static_cast<size_t>(GNX) + static_cast<size_t>(I);
				const double A = Grid[K], B = Grid[K + 1], C = Grid[K + static_cast<size_t>(GNX)], D = Grid[K + static_cast<size_t>(GNX) + 1];
				return (A + (B - A) * FU) + ((C + (D - C) * FU) - (A + (B - A) * FU)) * FV;
			}
			double Relief(double X, double Y, double Z) const override
			{
				// (low swells a few hundred metres across, enough to hide a hull at a kilometre; the wadis cut under a
				// metre inside the field, where the fight reads its own heights, and two or three beyond it)
				const double Deep = 0.9 + 1.1 * Smooth(0.0, 250.0, Outside(X, Y));
				return Z + 0.4 * Fbm(X, Y, 300.0, 2, Seed ^ 0xD1) - Deep * Wadi(X, Y);
			}
			/** The escarpment north of the field: metres north of its foot (negative south of it). */
			double Scarp(double X, double Y) const
			{
				const double Line = Fd.Depth() + 950.0 + 300.0 * Noise(X / 2600.0, 0.5, Seed ^ 0xE1) + 50.0 * Noise(X / 300.0, 1.5, Seed ^ 0xE2);
				return Y - Line;
			}
			double Rolling(double X, double Y, double MeanZ) const override
			{
				// (the plain undulating a metre or two, the deirs' shallow bowls, and the escarpment stepping up some 70 m
				// on the northern horizon, its top ragged: a limestone band over its scree)
				const double S = Scarp(X, Y);
				const double Up = (70.0 + 10.0 * Noise(X / 700.0, 2.5, Seed ^ 0xE3)) * (0.65 * Smooth(-110.0, 10.0, S) + 0.35 * Smooth(10.0, 45.0, S));
				return MeanZ + 6.0 * Fbm(X, Y, 3800.0, 3, Seed ^ 0xB1) + 1.5 * Fbm(X, Y, 900.0, 2, Seed ^ 0xB2) + Up;
			}
			double LaneDepth(double /*X*/, double /*Y*/) const override { return 0.06; }
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 3.5 : 2.6) + 1.6 * U; }
			double PlotMin() const override { return 90000.0; }
			double PlotMax() const override { return 220000.0; }
			double FarTilled() const override { return 0.0; }
			double WoodsEdge() const override { return 0.9; }

			int CropOf(int Kind, double U) const override
			{
				if (Kind == KRough) return Rock;
				if (Kind == KField) return Scrub;
				return U < 0.7 ? Serir : U < 0.9 ? Sand : Scrub;
			}
			const char* CropWord(int C) const override
			{
				switch (C)
				{
				case Serir: return "serir";
				case Sand: return "sand";
				case Scrub: return "scrub";
				case Rock: return "rock";
				default: return "";
				}
			}

			// ---------------------------------------------------------------- the paint

			/** The plain by its land, not its parcels: sand in the hollows and the wadis, rock on the rises and the rules'
			 * rough, the stony serir between, its varnished pebbles darkening it in patches. */
			static Tone Plain(int Kind, double N1, double N2, double Broad, double W)
			{
				// (a quarter of the plain in sand sheets, a third darkened by varnished gravel in patches tens of metres
				// across, the rest the fawn serir; the rises bare limestone)
				const double Sandy = Clamp01(Smooth(0.16, 0.48, Broad + 0.3 * N1) + 0.9 * W);
				const double Rocky = Kind == KRough ? 0.85 : Smooth(-0.38, -0.62, Broad + 0.2 * N1) * (1.0 - W);
				const double Varn = Smooth(-0.25, 0.55, 0.75 * N1 + 0.55 * Broad);
				Tone T = Mixed(SerirMid, SerirPale, 0.42 + 0.25 * N2 - 0.2 * N1);
				T = Mixed(T, Varnish, 0.36 * Varn + 0.04 * N2);
				T = Mixed(T, Mixed(Limestone, Varnish, 0.2 + 0.3 * N2), Rocky);
				T = Mixed(T, Varied(SandSheet, 1.0 + 0.05 * N2, 0.0), Sandy * (1.0 - 0.6 * Rocky));
				return Varied(T, 1.0 + 0.05 * Broad, 0.015 * Broad);
			}

			Tone Colour(const Spot& S) const override
			{
				switch (S.Kind)
				{
				case KLane:
				{
					// a track: churned powdery dust, the lorries' ruts pressed darker either side of a paler crown
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double RutK = Smooth(0.5, 0.12, std::fabs(Off - 0.9));
					Tone T = Mixed(Dust, Rut, 0.65 * RutK);
					if (S.Lane > 0.0) T = Mixed(T, Plain(KOpen, S.N1, S.N2, S.Broad, 0.0), Smooth(0.0, 1.4, S.Lane));
					return Varied(T, 1.0 + 0.04 * S.N2, 0.0);
				}
				case KVillage:
				case KYard: return Varied(Rgb(200, 180, 144), 1.0 + 0.05 * S.N2, 0.01);
				case KWater: return Rgb(84, 104, 104);
				case KMarsh: return Varied(Rgb(178, 172, 150), 1.0 + 0.06 * S.N1, 0.0); // (a sabkha: grey-white salt crust)
				case KWorks: return Varied(Mixed(Limestone, SerirMid, 0.5), 1.0 + 0.06 * S.N2, 0.0); // (stone thrown up from the rock)
				default: break;
				}
				const double W = Wadi(S.X, S.Y);
				Tone T = Plain(S.Kind, S.N1, S.N2, S.Broad, W);
				if (bSpring) T = Mixed(T, Rgb(150, 150, 104), 0.22 * W + 0.08 * Smooth(0.3, 0.8, S.N1) * (1.0 - W));
				// (beside a track: the braided band of older ruts, the broken crust paler than the plain)
				if (S.Lane < 45.0)
				{
					const double Band = Smooth(45.0, 8.0, S.Lane);
					const double Phase = 0.27 * S.Lane + 0.64 * S.N1;
					const double Line = 2.0 * std::fabs(Phase - std::floor(Phase) - 0.5); // (a rut every 3.7 m, wandering)
					T = Mixed(T, Dust, Band * (0.16 + 0.42 * Smooth(0.8, 0.96, Line)));
				}
				return T;
			}

			Tone Far(const Spot& S, bool /*bWoods*/, double /*Gap*/) const override
			{
				Tone T = Plain(S.Kind == KWoods ? static_cast<int>(KOpen) : S.Kind, S.N1, 0.0, S.Broad, 0.0);
				// (the escarpment's face: the pale limestone band, darker scree under it)
				const double Sc = Scarp(S.X, S.Y);
				const double Face = Smooth(-120.0, -70.0, Sc) * Smooth(20.0, -5.0, Sc);
				// (scree darkening down the face, the cliff band at its top pale and sharp)
				const Tone Scree = Mixed(Rgb(104, 86, 66), Rgb(150, 128, 98), Smooth(-110.0, -40.0, Sc));
				return Mixed(T, Mixed(Scree, Rgb(222, 210, 182), Smooth(-22.0, -8.0, Sc)), 0.9 * Face);
			}

			// ---------------------------------------------------------------- round the eye

			Grown Grow(const Spot& S, double Pick) const override
			{
				if (S.Kind == KWater || S.Drawn == 9 /*floor*/ || S.Drawn == 10 /*wall*/) return Grown{};
				if (S.Kind == KLane) return S.Lane > -0.4 ? Grown{ ClutOf(Pick < 0.5 ? CGravel : CPebbles), 0.3 } : Grown{};
				if (S.Kind == KVillage || S.Kind == KYard) return Pick < 0.7 ? Grown{ ClutOf(CPebbles), 0.15 } : Grown{ ClutOf(CTwigs), 0.05 };
				if (S.Kind == KRough)
				{
					if (Pick < 0.35) return Grown{ ClutOf(CGravel), 0.7 };
					return Pick < 0.7 ? Grown{ ClutOf(CPebbles), 0.6 } : Grown{ ClutOf(CSlab), 0.45 };
				}
				const double W = Wadi(S.X, S.Y);
				if (W > 0.25 || S.Kind == KWoods || S.Kind == KMarsh)
				{
					// (a wadi's sandy floor: dry grass and dead shrubs two to five times thicker than on the plain)
					if (Pick < 0.4) return Grown{ ClutOf(CGrass), 0.45 };
					if (Pick < 0.55) return Grown{ ClutOf(CTwigs), 0.35 };
					if (Pick < 0.8) return Grown{ ClutOf(CGravel), 0.4 };
					return Grown{ ClutOf(CPebbles), 0.2 };
				}
				// the serir: a gravel pavement speckled pale and dark, cobbles a few to the square metre, a flat stone every
				// twenty square metres, a dry tuft now and then
				if (Pick < 0.5) return Grown{ ClutOf(CGravel), 1.0 };
				if (Pick < 0.8) return Grown{ ClutOf(CPebbles), 0.45 };
				if (Pick < 0.88) return Grown{ ClutOf(CSlab), 0.25 };
				if (Pick < 0.95) return Grown{ ClutOf(CGrass), 0.25 };
				return Grown{ ClutOf(CTwigs), 0.2 };
			}

			bool Dress(LandField::Placed& P, const Spot& /*S*/, double U4, double U5, double U6, double U7) const override
			{
				const int K = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				switch (K)
				{
				case CPebbles:
				case CGravel:
				{
					// (their stones pale and varnished as built; a scatter a little lighter or darker as a whole)
					P.Scale = static_cast<float>(K == CGravel ? 0.75 + 0.5 * U4 : 0.7 + 0.7 * U4 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					const double Pale = 0.9 + 0.18 * U7;
					P.Tint = Tint(Pale * 1.01, Pale, Pale * 0.97);
					return true;
				}
				case CSlab:
				{
					// (six in ten pale limestone, four in ten varnished brown-black on top; its sand tail to the lee)
					P.Scale = static_cast<float>(0.6 + 0.9 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					P.Yaw = static_cast<float>(Lee + 0.5 * (U6 - 0.5));
					const double Pale = 0.94 + 0.12 * U7;
					P.Tint = U6 < 0.4 ? Tint(0.62, 0.54, 0.46) : Tint(Pale * 1.02, Pale, Pale * 0.95);
					return true;
				}
				case CGrass:
				{
					P.Scale = static_cast<float>(0.6 + 0.7 * U4);
					P.ScaleZ = static_cast<float>(0.55 + 0.6 * U5);
					P.Tint = bSpring && U6 < 0.5 ? Tint(0.82, 0.95, 0.72) : Tint(0.95 + 0.1 * U7, 0.95 + 0.06 * U7, 0.92);
					return true;
				}
				case CTwigs:
				{
					P.Scale = static_cast<float>(0.6 + 0.8 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.6 * U5);
					P.Yaw = static_cast<float>(Lee + 0.6 * (U7 - 0.5));
					P.Tint = Tint(0.92 + 0.12 * U6, 0.94 + 0.08 * U6, 0.94);
					return true;
				}
				default: return false;
				}
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				switch (K)
				{
				case CGrass:
				{
					// some fourteen blades from a tight foot, straw-grey, their tips paler, leaning out
					for (int B = 0; B < 14; ++B)
					{
						const double A = 2.0 * Pi * (B + 0.5 * Unit(Hash(0x6A55, B))) / 14.0;
						const double Lean = 0.08 + 0.12 * Unit(Hash(0x6A56, B)), High = 0.16 + 0.18 * Unit(Hash(0x6A57, B));
						const double FX = 0.03 * std::cos(A), FY = 0.03 * std::sin(A);
						W.Blade(FX, FY, 0.0, FX + Lean * std::cos(A), FY + Lean * std::sin(A), High, 0.012, Rgb(120, 104, 76), Rgb(204, 188, 142));
					}
					M.Paint = 2;
					M.FarFrom = 40.0f;
					break;
				}
				case CPebbles:
				{
					// four cobbles, fist-sized and smaller: two pale limestone, two varnished dark on top
					for (int B = 0; B < 4; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0x9EB, B)), D = 0.22 * Unit(Hash(0x9EC, B));
						const double R = 0.035 + 0.05 * Unit(Hash(0x9ED, B));
						const bool bDark = B % 2 == 1;
						Chip(W, D * std::cos(A), D * std::sin(A), R, 0.6 * R, Hash(0x9EE, B), bDark ? Varnish : Limestone, bDark ? VarnishUnder : LimeUnder);
					}
					M.FarFrom = 30.0f;
					break;
				}
				case CGravel:
				{
					// a dozen grains a thumbnail to a walnut across over a half-metre: six in ten pale, four varnished
					for (int B = 0; B < 12; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0x6A7, B)), D = 0.32 * std::sqrt(Unit(Hash(0x6A8, B)));
						const double R = 0.012 + 0.022 * Unit(Hash(0x6A9, B));
						const bool bDark = Unit(Hash(0x6AA, B)) < 0.4;
						Grain(W, D * std::cos(A), D * std::sin(A), R, 0.7 * R, Hash(0x6AB, B), bDark ? Mixed(Varnish, Rgb(70, 56, 44), 0.4) : Limestone, bDark ? VarnishUnder : LimeUnder);
					}
					M.FarFrom = 22.0f;
					break;
				}
				case CSlab:
				{
					Chip(W, 0.0, 0.0, 0.16, 0.06, 0x51AB, Limestone, LimeUnder);
					Chip(W, -0.15, 0.08, 0.05, 0.035, 0x51AC, Limestone, LimeUnder);
					Tail(W, 0.1, 0.7, 0.26, 0.05, SandSheet, Mixed(SandSheet, SerirMid, 0.5));
					M.FarFrom = 45.0f;
					break;
				}
				case CTwigs:
				{
					// a dead shrub: a dozen grey twigs splayed from a sand hummock's crown, its sand tail a metre or two
					// to the lee
					Chip(W, 0.0, 0.0, 0.22, 0.08, 0x7A16, SandSheet, Rgb(176, 156, 118));
					Tail(W, 0.12, 1.4, 0.4, 0.07, SandSheet, Mixed(SandSheet, SerirMid, 0.5));
					for (int B = 0; B < 12; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0x7A17, B)), Out = 0.12 + 0.18 * Unit(Hash(0x7A18, B));
						const double High = 0.18 + 0.22 * Unit(Hash(0x7A19, B));
						W.Blade(0.0, 0.0, 0.05, Out * std::cos(A), Out * std::sin(A), High, 0.01, Rgb(92, 84, 72), Rgb(150, 142, 126));
					}
					M.Paint = 2;
					M.FarFrom = 55.0f;
					break;
				}
				default: break;
				}
			}

			// ---------------------------------------------------------------- the kit

			const char* Folder() const override { return "Med"; }
			const char* KitName(int K) const override
			{
				switch (K)
				{
				case KThorn: return "CamelThorn";
				case KBoulder: return "Boulder";
				case KHouse: return "FlatRoofHouse";
				case KHouseStone: return "FlatRoofHouseStone";
				case KSangar: return "Sangar";
				case KPole: return "TelegraphPole";
				case KPalm: return "DatePalm";
				case KStones: return "Stones";
				case KHouseTwo: return "FlatRoofHouseTwo";
				case KDome: return "DomeHouse";
				default: return nullptr;
				}
			}
			void KitLook(int K, LandField::BaseMesh& M) const override
			{
				auto Stands = [&M](float Mid, float Half, float Across, float Trunk, float Far) {
					M.bSight = true;
					M.SightMid = Mid;
					M.SightHalf = Half;
					M.SightAcross = Across;
					M.SightTrunk = Trunk;
					M.FarFrom = Far;
				};
				switch (K)
				{
				case KHouse:
				case KHouseStone:
				case KHouseTwo:
				case KDome:
					Centre(M);
					Stands(0.5f, 0.5f, 0.8f, 0.0f, 320.0f);
					break;
				case KBoulder: Stands(0.5f, 0.5f, 0.85f, 0.0f, 200.0f); break;
				case KSangar: Stands(0.45f, 0.45f, 0.6f, 0.0f, 220.0f); break; // (a man kneels behind it)
				case KPalm:
					// (its near level is five times an oak's: plain from 90 m, a crown from 400 m, as the bocage's trees)
					Stands(0.88f, 0.12f, 0.75f, 0.25f, 90.0f);
					M.bTree = true;
					break;
				case KThorn: M.FarFrom = 110.0f; break;
				case KPole: M.FarFrom = 200.0f; break;
				default: M.FarFrom = 90.0f; break;
				}
			}
			void KitPlain(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				switch (K)
				{
				case KHouse:
				case KHouseStone:
				case KHouseTwo:
				case KDome: W.Box(0.0, 0.0, 1.6, 4.0, 3.0, 1.6, 0.0, Rgb(214, 198, 166)); break;
				case KThorn: W.Rock(0.0, 0.0, 0.5, 0.45, 7, Rgb(132, 120, 80), Rgb(96, 88, 62)); break;
				case KBoulder:
				case KStones: W.Rock(0.0, 0.0, 0.9, 0.7, 11, Rgb(176, 150, 118), Rgb(120, 100, 80)); break;
				case KSangar: W.Box(0.0, 0.0, 0.4, 1.8, 0.4, 0.4, 0.0, Rgb(160, 140, 112)); break;
				default: W.Box(0.0, 0.0, 3.0, 0.12, 0.12, 3.0, 0.0, Rgb(110, 96, 80)); break;
				}
				M.Far = M.Near;
				M.FarFrom = 400.0f;
			}

			Tone WallStone() const override { return Rgb(206, 194, 166); }
			double WallIvy() const override { return 0.0; }

			// ---------------------------------------------------------------- what stands on it

			void Sites(Host& H) override
			{
				double TX = 0.0, TY = 0.0, TR = 0.0;
				if (!H.Town(TX, TY, TR)) return;
				Rng R(H.Seed() ^ 0x51E5);
				int Built = 0;
				// The town: flat-roofed houses with their courtyards packed in blocks along a loose grid turned to the
				// town's own axis, a street every third house and every fourth row, the plots thinning toward its edge.
				const double Axis = R.Range(0.0, 0.5 * Pi);
				const double Ca = std::cos(Axis), Sa = std::sin(Axis);
				const double Step = 13.0, Reach = 0.95 * TR;
				const int N = static_cast<int>(Reach / Step) + 1;
				for (int J = -N; J <= N; ++J)
					for (int I = -N; I <= N; ++I)
					{
						if (((I % 3) + 3) % 3 == 2 || ((J % 4) + 4) % 4 == 3) continue; // (the streets)
						const double U = I * Step, V = J * Step;
						const double D = std::hypot(U, V) / Reach;
						const double Skip = R.Next(), Jx = R.Range(-1.2, 1.2), Jy = R.Range(-1.2, 1.2), Turn = R.Range(-0.06, 0.06), Kind = R.Next();
						if (D > 1.0 || Skip < 0.12 + 0.55 * D * D) continue;
						const double X = TX + U * Ca - V * Sa + Jx, Y = TY + U * Sa + V * Ca + Jy;
						const int Slot = Kind < 0.2 ? KHouseTwo : Kind < 0.55 ? KHouseStone : KHouse;
						if (H.House(KitOf(Slot), X, Y, Axis + Turn + (Kind > 0.8 ? Pi : 0.0), 11.0, 11.0)) ++Built;
					}
				// A domed tomb (a marabout) on a rise outside the town: the plain's landmark.
				double BestX = 0.0, BestY = 0.0, BestZ = -1e9;
				for (int K = 0; K < 60; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(TR + 250.0, TR + 900.0);
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A);
					if (X < 50.0 || Y < 50.0 || X > H.Width() - 50.0 || Y > H.Depth() - 50.0 || !H.Free(X, Y, 6.0)) continue;
					const double Z = H.Land(X, Y);
					if (Z > BestZ)
					{
						BestZ = Z;
						BestX = X;
						BestY = Y;
					}
				}
				const double DomeYaw = R.Range(0.0, 2.0 * Pi);
				if (BestZ > -1e8 && H.House(KitOf(KDome), BestX, BestY, DomeYaw, 8.0, 8.0)) ++Built;
				H.Tally(Built, 0, 0);
			}

			void Plants(Host& H) override
			{
				Rng R(H.Seed() ^ 0x9A47);
				const double B = H.Band(), W = H.Width() + 2.0 * B, D = H.Depth() + 2.0 * B;
				// (the kit's thorn is olive, as after the winter rains; from May it is straw-grey and grey-brown)
				const double ThornR = bSpring ? 0.98 : 1.13, ThornB = bSpring ? 1.0 : 1.07;
				// Camel thorn on its hummock: some 40 a hectare on the serir, a few hundred on the scrub flats and in the
				// wadis' floors; clumped by a slow noise, never continuous.
				const int Tries = static_cast<int>(W * D / 10000.0 * 320.0);
				for (int K = 0; K < Tries; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.65, 1.35), Tall = R.Range(0.75, 1.15), Grey = R.Range(0.92, 1.06);
					const Spot S = H.At(X, Y);
					if (S.Kind == KLane || S.Kind == KVillage || S.Kind == KYard || S.Kind == KWater) continue;
					const double Wd = Wadi(X, Y);
					const double Clump = 0.5 + 0.5 * Noise(X / 60.0, Y / 60.0, Seed ^ 0x7C);
					// (the art director's desert set: some 5 a hectare on the open plain, 6 on the rough, 8 under palms; the
					// scrub flats and the wadis' floors thicker, as the research has them)
					double Share = S.Kind == KRough ? 0.02 : S.Kind == KWoods ? 0.025 : S.Kind == KMarsh ? 0.008 : S.Crop == Scrub ? 0.2 : 0.016;
					Share = std::max(Share, 0.6 * Wd) * (0.35 + 1.3 * Clump * Clump);
					if (Keep >= Share || !H.Free(X, Y, 0.8)) continue;
					H.Plant(KitOf(KThorn), X, Y, Yaw, Size, Size * Tall, Tint(Grey * ThornR, Grey, Grey * ThornB));
				}
				// Loose stones and boulders, as the art director's desert set lays them: on the open plain some 22 patches of
				// stones a hectare and a boulder in two or three hectares; on the rough 60 and 14.
				const int Rocks = static_cast<int>(W * D / 10000.0 * 100.0);
				for (int K = 0; K < Rocks; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.7, 1.4), Flat = R.Range(0.7, 1.1), Big = R.Range(0.7, 2.2);
					const Spot S = H.At(X, Y);
					const bool bRock = S.Kind == KRough;
					const bool bOpen = S.Kind == KOpen || S.Kind == KField;
					const double Stones = bRock ? 0.6 : bOpen ? 0.22 : 0.0, Boulders = Stones + (bRock ? 0.14 : bOpen ? 0.004 : 0.0);
					if (Keep >= Boulders) continue;
					const bool bBoulder = Keep >= Stones;
					if (!H.Free(X, Y, bBoulder ? 2.0 : 1.2)) continue;
					H.Plant(KitOf(bBoulder ? KBoulder : KStones), X, Y, Yaw, bBoulder ? Big : Size, (bBoulder ? Big : Size) * Flat, Tint(1.0, 1.0, 1.0));
				}
				// Sangars of piled stone where the rules dig: some 20 m apart, their open backs to the field's middle.
				{
					const double Cx = 0.5 * H.Width(), Cy = 0.5 * H.Depth();
					for (double Y = 10.0; Y < H.Depth(); Y += 24.0)
						for (double X = 10.0; X < H.Width(); X += 24.0)
						{
							const double Jx = R.Range(-6.0, 6.0), Jy = R.Range(-6.0, 6.0), Turn = R.Range(-0.5, 0.5);
							const double PX = X + Jx, PY = Y + Jy;
							if (H.At(PX, PY).Kind != KWorks || !H.Free(PX, PY, 2.5)) continue;
							H.Plant(KitOf(KSangar), PX, PY, std::atan2(Cy - PY, Cx - PX) + Pi + Turn, 1.0, 1.0, Tint(1.0, 1.0, 1.0));
						}
				}
				// Telegraph poles 55 m apart, 14 m off the longest track (the art director's desert set).
				size_t Longest = 0, Most = 0;
				for (size_t K = 0; K < H.Lanes(); ++K)
					if (H.LaneLine(K).size() > Most)
					{
						Most = H.LaneLine(K).size();
						Longest = K;
					}
				if (Most >= 2)
				{
					const std::vector<std::pair<double, double>>& L = H.LaneLine(Longest);
					const double Off = 14.0;
					double Since = 55.0;
					for (size_t K = 1; K < L.size(); ++K)
					{
						const double Dx = L[K].first - L[K - 1].first, Dy = L[K].second - L[K - 1].second, Len = std::hypot(Dx, Dy);
						Since += Len;
						if (Since < 55.0 || Len < 1e-6) continue;
						Since = 0.0;
						const double X = L[K].first - Dy / Len * Off, Y = L[K].second + Dx / Len * Off;
						const double Lean = R.Range(-0.04, 0.04), High = R.Range(0.95, 1.05);
						if (H.Free(X, Y, 0.4)) H.Plant(KitOf(KPole), X, Y, std::atan2(Dy, Dx) + Lean, 1.0, High, Tint(1.0, 1.0, 1.0));
					}
				}
				// Date palms: round the town's edge (some 14 a hectare in a ring 60 m deep), in the recording's woods as a palm
				// grove (95 a hectare), and in southern Tunisia an oasis garden by the town.
				double TX = 0.0, TY = 0.0, TR = 0.0;
				const bool bTown = H.Town(TX, TY, TR);
				if (bTown)
				{
					const int Edge = static_cast<int>(Pi * ((TR + 60.0) * (TR + 60.0) - TR * TR) / 10000.0 * 14.0);
					for (int K = 0; K < Edge; ++K)
					{
						const double A = R.Range(0.0, 2.0 * Pi), Dd = R.Range(TR, TR + 60.0), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.8, 1.2), Tall = R.Range(0.85, 1.15);
						const double X = TX + Dd * std::cos(A), Y = TY + Dd * std::sin(A);
						if (H.Free(X, Y, 3.0)) H.Plant(KitOf(KPalm), X, Y, Yaw, Size, Size * Tall, Tint(1.0, 1.0, 1.0));
					}
				}
				{
					const int Grove = static_cast<int>(W * D / 10000.0 * 95.0);
					for (int K = 0; K < Grove; ++K)
					{
						const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.8, 1.25), Tall = R.Range(0.85, 1.15);
						if (H.At(X, Y).Kind != KWoods || !H.Free(X, Y, 2.75)) continue;
						H.Plant(KitOf(KPalm), X, Y, Yaw, Size, Size * Tall, Tint(1.0, 1.0, 1.0));
					}
				}
				if (bPalms && bTown)
				{
					const double A = R.Range(0.0, 2.0 * Pi);
					const double GX = TX + (TR + 80.0) * std::cos(A), GY = TY + (TR + 80.0) * std::sin(A);
					for (int K = 0; K < 140; ++K)
					{
						const double X = GX + R.Range(-90.0, 90.0), Y = GY + R.Range(-90.0, 90.0), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.75, 1.15), Tall = R.Range(0.85, 1.15);
						if (H.Free(X, Y, 2.0)) H.Plant(KitOf(KPalm), X, Y, Yaw, Size, Size * Tall, Tint(1.0, 1.0, 1.0));
					}
				}
			}

			int Fx(const std::string& Ground, int Else) const override
			{
				if (Ground == "woods" || Ground == "forest") return 5;
				return Else == 0 || Else == 4 ? 5 : Else;
			}
			const char* Earth() const override { return "sand"; }

		private:
			bool bSpring = false, bPalms = false;
			static constexpr double GridStep = 4.0;
			double GX0 = 0.0, GY0 = 0.0;
			int GNX = 0, GNY = 0;
			std::vector<float> Grid;
		};
	}

	std::unique_ptr<Region> MakeDesert(const LandField::Field& F, uint64_t S) { return std::make_unique<Desert>(F, S); }
}
