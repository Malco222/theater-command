#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <vector>

/**
 * Rice country: central and southern China, Korea, Japan south of Hokkaido and the Ryukyus (Shanghai and Nanking 1937,
 * Wuhan 1938, Changsha 1939 to 1944, Ichi-Go 1944, Okinawa 1945, an invasion of Japan). Brian, 23 November 2026: "do 1
 * and 3" (1: the three missing battlefields, rice country first; the art director's monsoon set,
 * battle-models/asia/sets/monsoon.json, is its brief).
 *
 *   - paddies in plots of some 14 to 24 m behind low grassed bunds, each plot at its own stage of the rice year: flooded
 *     and pale with the sky in April and May, the seedlings in rows over the water in June, a deep yellow-green from July,
 *     gold in late September, stubble and straw stacks after the harvest, drained grey-brown mud, winter wheat and in the
 *     south the rape's yellow in the dry fields between;
 *   - terraces where the rules' rough ground climbs, the paddies stepping up the slope along its contours;
 *   - vegetable strips in the dry fields round the villages; grave mounds in their family groups on the open ground and
 *     at the paddies' corners; willows along the tracks and the water;
 *   - the hills' woods of pine, bamboo groves in their folds and round every village;
 *   - villages close-packed, their brick houses under grey tile all facing south, a walled farmyard or two at the edge,
 *     a mud house under thatch for the poor, a pagoda on a rise, straw stacks after the harvest; the tracks raised earth,
 *     red in the south.
 *
 * Its kit is the Asia thread's battle models (Content/Models/Asia, from battle-models/asia/models): HillPine, Bamboo,
 * Willow, ChinaHouse, Farmyard, ThatchHouse, TieredTower, GraveMound, StrawStack, RockOutcrop, Boulder. The rice, the
 * stubble, the bunds, the grass and the dry fields' crops round the eye it builds itself.
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Paddy = 0,   // rice behind bunds
			Garden = 1,  // the dry fields: vegetables in strips, sweet potatoes, beans; winter wheat and rape
			Grass = 2,   // the open ground: grazing, the graves' ground
			Terrace = 3, // paddies stepping up a slope along its contours
			Hill = 4     // the hills' rough grass and scrub
		};

		enum Slot : int
		{
			KPine = 0,
			KBamboo = 1,
			KWillow = 2,
			KHouse = 3,
			KYardHouse = 4,
			KThatch = 5,
			KTower = 6,
			KGrave = 7,
			KStack = 8,
			KOutcrop = 9,
			KBoulder = 10
		};

		enum Clut : int
		{
			CRice = 0,    // a patch of rice hills in their rows
			CStubble = 1, // the cut hills after the harvest
			CBund = 2,    // a length of a paddy's bund, grassed
			CGrass = 3,   // a grass tuft
			CCrop = 4,    // the dry fields' crop: a leafy clump (wheat, rape, vegetables by its tint)
			CReed = 5     // reeds in the marsh and the ditches
		};

		/** A paddy plot's stage of the rice year. */
		enum Stage : int
		{
			SWater = 0,    // flooded for the planting: the sky in it
			SSeedling = 1, // the seedlings just planted out in rows, the water between
			SYoung = 2,    // the rice a hand or two high, the water still showing
			SGreen = 3,    // the rice grown, a deep yellow-green
			SRipe = 4,     // ripe, gold (or the winter wheat's harvest in May)
			SStubble = 5,  // cut: stubble and straw
			SMud = 6,      // drained and ploughed: grey-brown mud
			SWinter = 7,   // winter wheat on the drained paddy: a thin green
			SRape = 8      // the rape in flower in the south's April: yellow
		};

		LandField::Base KitOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K); }
		LandField::Base ClutOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Clut0) + K); }

		// (sRGB as painted, the art director's to check: the set's paddy_mud 675E4A, laterite_grass 6E7849, hill_grass
		// 5F6E3D, laterite 985F41; the flooded plots' sky the Pacific thread's passed paddy, a little bluer)
		const Tone Water = Rgb(140, 152, 152);
		const Tone RiceGreen = Rgb(96, 132, 50);
		const Tone RiceYoung = Rgb(104, 136, 64);
		const Tone RiceGold = Rgb(178, 150, 74);
		const Tone StubbleC = Rgb(150, 130, 92);
		const Tone Mud = Rgb(103, 94, 74);
		const Tone WheatGreen = Rgb(100, 124, 62);
		const Tone RapeYellow = Rgb(176, 164, 74);
		const Tone BundGrass = Rgb(106, 118, 64);
		const Tone OpenGrass = Rgb(110, 120, 73);
		const Tone HillGrass = Rgb(95, 110, 61);
		const Tone RedEarth = Rgb(152, 95, 65);
		const Tone GreyEarth = Rgb(140, 126, 100);
		const Tone Needles = Rgb(104, 90, 64);
		const Tone PineCanopy = Rgb(56, 72, 42);

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

		class Asia final : public Region
		{
		public:
			Asia(const LandField::Field& F, uint64_t S) : Region(F, S)
			{
				const int M = F.Month >= 1 && F.Month <= 12 ? F.Month : 7;
				Month = M;
				bWinter = M <= 2 || M == 12;
				// (the grass's year: green from April to October, straw in the winter)
				Green = (M >= 5 && M <= 9) ? 1.0 : (M == 4 || M == 10) ? 0.6 : M == 11 || M == 3 ? 0.3 : 0.1;
				// (the red earth of the south, Hunan, Kiangsi and Kwangtung; the Yangtze's delta, Korea and Japan grey-brown)
				Red = Smooth(30.5, 27.0, F.Lat) * (F.Lon > 118.5 && F.Lat > 29.0 ? 0.0 : 1.0) * (F.Lon > 123.0 ? 0.3 : 1.0);
				// (the rape in flower in April south of the Huai; Korea's and Japan's paddies a fortnight later)
				bRape = F.Lat < 34.0 && F.Lon < 123.0;
				bLate = F.Lat > 35.0;
				// (hills round the plain, as round most of rice country; the delta about Shanghai flat to the sea)
				bDelta = F.Lon > 119.0 && F.Lon < 122.5 && F.Lat > 30.0 && F.Lat < 33.5;
				// (the snow lying, as LandField will lay it: under it no bunds, no crop and no green blade show)
				SnowLying = SnowOf(F, Look::Rice);
				double Sum = 0.0;
				for (const double H : F.Height) Sum += H;
				MeanZ = F.Height.empty() ? 0.0 : Sum / static_cast<double>(F.Height.size());
			}
			Look Which() const override { return Look::Rice; }

			// ---------------------------------------------------------------- the year

			/** A paddy plot's stage this month: each plot its own, as each family works its own. */
			int StageOf(uint64_t Parcel, uint64_t Plot) const
			{
				struct Share
				{
					int Stage;
					double Part;
				};
				static const Share Year[12][4] = {
					{ { SStubble, 0.45 }, { SMud, 0.25 }, { SWinter, 0.2 }, { SWater, 0.1 } },     // January
					{ { SStubble, 0.35 }, { SMud, 0.3 }, { SWinter, 0.25 }, { SWater, 0.1 } },     // February
					{ { SMud, 0.35 }, { SWinter, 0.3 }, { SWater, 0.25 }, { SStubble, 0.1 } },     // March
					{ { SWater, 0.45 }, { SRape, 0.2 }, { SWinter, 0.15 }, { SMud, 0.2 } },        // April
					{ { SWater, 0.35 }, { SSeedling, 0.35 }, { SRipe, 0.15 }, { SMud, 0.15 } },    // May
					{ { SSeedling, 0.35 }, { SYoung, 0.45 }, { SWater, 0.1 }, { SStubble, 0.1 } }, // June
					{ { SYoung, 0.25 }, { SGreen, 0.75 }, { SGreen, 0.0 }, { SGreen, 0.0 } },      // July
					{ { SGreen, 1.0 }, { SGreen, 0.0 }, { SGreen, 0.0 }, { SGreen, 0.0 } },        // August
					{ { SGreen, 0.45 }, { SRipe, 0.55 }, { SRipe, 0.0 }, { SRipe, 0.0 } },         // September
					{ { SRipe, 0.4 }, { SStubble, 0.45 }, { SMud, 0.15 }, { SMud, 0.0 } },         // October
					{ { SStubble, 0.55 }, { SMud, 0.3 }, { SWinter, 0.15 }, { SWinter, 0.0 } },    // November
					{ { SStubble, 0.5 }, { SMud, 0.3 }, { SWinter, 0.2 }, { SWinter, 0.0 } }       // December
				};
				// (the north's year a fortnight behind: half its plots a month back)
				int M = Month;
				if (bLate && Unit(Hash(Parcel, 0x1A7E)) < 0.5) M = M == 1 ? 12 : M - 1;
				// (the plots of a parcel mostly at one stage, a family's or a village's work together; some a step on or
				// behind: the patchwork reads parcel by parcel from afar and plot by plot near)
				const double U = 0.3 * Unit(Hash(Parcel, 0x57A6)) + 0.7 * Unit(Hash(Plot, 0x57A7));
				double Run = 0.0;
				for (const Share& S : Year[M - 1])
				{
					Run += S.Part;
					if (U < Run) return S.Stage == SRape && !bRape ? SWinter : S.Stage;
				}
				return Year[M - 1][0].Stage;
			}

			/** A stage's colour, Plot's own shade of it. */
			static Tone StageColour(int St, uint64_t Plot)
			{
				const double U = Unit(Hash(Plot, 0xC01));
				// (each plot its own light and dark, so the patchwork reads from above where the bunds are too fine to)
				const double L = 0.87 + 0.26 * Unit(Hash(Plot, 0xC03));
				return Varied(Painted(St, Plot, U), L, 0.0);
			}
			static Tone Painted(int St, uint64_t Plot, double U)
			{
				switch (St)
				{
				case SWater: return Varied(Mixed(Water, Mud, 0.12 + 0.25 * U), 0.96 + 0.1 * Unit(Hash(Plot, 0xC02)), -0.01);
				case SSeedling: return Mixed(Mixed(Water, Mud, 0.18), RiceYoung, 0.1 + 0.1 * U);
				case SYoung: return Mixed(Mixed(Water, Mud, 0.2), RiceYoung, 0.45 + 0.2 * U);
				case SGreen: return Varied(RiceGreen, 0.95 + 0.1 * U, 0.02 * U);
				case SRipe: return Varied(Mixed(RiceGold, RiceGreen, 0.25 * U), 0.95 + 0.08 * U, 0.0);
				case SStubble: return Varied(Mixed(StubbleC, Mud, 0.2 + 0.3 * U), 0.95 + 0.08 * U, 0.0);
				case SMud: return Varied(Mud, 0.92 + 0.14 * U, 0.01);
				case SWinter: return Mixed(Mud, WheatGreen, 0.45 + 0.4 * U);
				case SRape: return Mixed(RiceYoung, RapeYellow, 0.65 + 0.3 * U);
				default: return Mud;
				}
			}

			// ---------------------------------------------------------------- the plots

			struct PlotAt
			{
				uint64_t Key = 0;
				int Stage = SMud;
				double Bund = 99.0; // metres to the nearest bund's middle
				double Way = 0.0;   // the bund's line (radians from east)
			};

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

			/** The paddy plot at a place: plots some 14 to 24 m along the parcel's grain and a little over half that across
			 * (each parcel its own), bunds between them; on the terraces a plot to each step of the slope. */
			PlotAt PlotOf(const Spot& S) const
			{
				PlotAt P;
				if (S.Crop == Terrace)
				{
					const double Step = 1.3 + 1.1 * Unit(Hash(S.Key, 0x7E77));
					const double T = (RulesZ(S.X, S.Y) + 0.17 * Fbm(S.X, S.Y, 40.0, 1, Seed ^ 0x7E78)) / Step;
					const double F = T - std::floor(T);
					P.Key = Hash(S.Key, static_cast<uint64_t>(std::floor(T) + 4096.0));
					P.Bund = 6.0 * std::min(F, 1.0 - F); // (a riser's metres across, as near as the contours allow)
					P.Stage = StageOf(S.Key, P.Key);
					return P;
				}
				const double L = 14.0 + 10.0 * Unit(Hash(S.Key, 0xBADD));
				const double W = L * (0.55 + 0.25 * Unit(Hash(S.Key, 0xBADE)));
				const double A = S.Along / L + Unit(Hash(S.Key, 0xBADF)), B = S.Across / W + Unit(Hash(S.Key, 0xBAE0));
				const double FA = A - std::floor(A), FB = B - std::floor(B);
				const double DA = std::min(FA, 1.0 - FA) * L, DB = std::min(FB, 1.0 - FB) * W;
				P.Key = Hash(S.Key, static_cast<uint64_t>(std::floor(A) + 512.0), static_cast<uint64_t>(std::floor(B) + 512.0));
				P.Stage = StageOf(S.Key, P.Key);
				// (the nearer bund: one across the grain, or one along it)
				P.Bund = std::min(DA, DB);
				P.Way = DA < DB ? S.Angle + 0.5 * Pi : S.Angle;
				return P;
			}

			// ---------------------------------------------------------------- the land

			double Outside(double X, double Y) const
			{
				const double Dx = std::max({ 0.0, -X, X - Fd.Width() }), Dy = std::max({ 0.0, -Y, Y - Fd.Depth() });
				return std::sqrt(Dx * Dx + Dy * Dy);
			}
			/** The hills round the plain, 0 to 1 of their height: a kilometre or more out, wooded above the terraces. */
			double HillAt(double X, double Y) const
			{
				if (bDelta) return 0.0;
				const double H = std::max(0.0, Fbm(X, Y, 1500.0, 3, Seed ^ 0xA51) + 0.05);
				return Smooth(250.0, 900.0, Outside(X, Y)) * Smooth(0.0, 0.5, H) * (0.6 + 0.4 * Fbm(X, Y, 500.0, 2, Seed ^ 0xA52));
			}
			double Rolling(double X, double Y, double InMeanZ) const override
			{
				// (the paddies' plain as level as water; the hills standing up out of it some 60 to 120 m)
				const double Plain = InMeanZ + 3.0 * Fbm(X, Y, 2600.0, 2, Seed ^ 0xB1);
				return Plain + 110.0 * HillAt(X, Y);
			}
			double LaneDepth(double /*X*/, double /*Y*/) const override { return 0.0; }
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 2.6 : 1.9) + 0.6 * U; }
			double PlotMin() const override { return 20000.0; }
			double PlotMax() const override { return 45000.0; }
			double FarTilled() const override { return 0.75; }
			double WoodsEdge() const override { return 0.35; }

			int CropOf(int Kind, double U) const override
			{
				if (Kind == KField) return U < 0.82 ? Paddy : Garden;
				if (Kind == KRough) return U < 0.5 ? Terrace : Hill;
				// (the level open ground is paddy too, three parcels in four: rice country leaves little to graze)
				return U < 0.75 ? Paddy : Grass;
			}
			const char* CropWord(int C) const override
			{
				switch (C)
				{
				case Paddy: return "paddy";
				case Garden: return "dry field";
				case Grass: return "grass";
				case Terrace: return "terraces";
				case Hill: return "hill grass";
				default: return "";
				}
			}
			bool IsCorn(int C) const override
			{
				// (the rice grown and ripe stands knee to waist high; a tank lays it flat)
				return C == Paddy && (Month >= 7 && Month <= 10);
			}

			// ---------------------------------------------------------------- the paint

			Tone Soil() const { return Mixed(GreyEarth, RedEarth, Red); }
			Tone GrassAt(const Spot& S, const Tone& Base) const
			{
				Tone T = Mixed(Mixed(Base, Rgb(150, 138, 96), 1.0 - Green), Rgb(136, 128, 90), 0.15 * Smooth(0.0, 0.6, S.N1));
				T = Mixed(T, Base, 0.2 * Smooth(0.0, -0.6, S.N1) * Green);
				return Varied(T, 0.96 + 0.06 * S.N2 + 0.03 * S.Broad, 0.0);
			}
			Tone PaddyColour(const Spot& S) const
			{
				const PlotAt P = PlotOf(S);
				Tone T = StageColour(P.Stage, P.Key);
				// (the bunds: a hint at the ground's own spacing; the bund itself is the clutter's round the eye)
				T = Mixed(T, GrassAt(S, BundGrass), 0.35 * Smooth(1.6, 0.2, P.Bund) + 0.5 * Smooth(1.2, 0.0, S.Edge));
				return T;
			}
			Tone GardenColour(const Spot& S) const
			{
				// strips some 3 to 6 m wide across the grain, each its own crop: greens of several kinds and bare earth; the
				// winter's wheat and the south's April rape
				const double W = 2.5 + 2.0 * Unit(Hash(S.Key, 0x6A2));
				const double B = S.Across / W;
				const uint64_t Strip = Hash(S.Key, static_cast<uint64_t>(std::floor(B) + 512.0));
				const double U = Unit(Strip);
				Tone T;
				if (bWinter || Month == 3) T = U < 0.55 ? Mixed(Mud, WheatGreen, 0.55) : U < 0.8 ? Varied(Mud, 1.05, 0.02) : Rgb(88, 110, 58);
				else if (Month == 4 && bRape && U < 0.35) T = RapeYellow;
				else T = U < 0.3 ? Rgb(92, 112, 56) : U < 0.5 ? Rgb(84, 102, 54) : U < 0.7 ? Rgb(104, 118, 62) : U < 0.85 ? Mixed(Varied(Mixed(Mud, Soil(), 0.4), 1.04, 0.0), Rgb(98, 112, 60), 0.35) : Rgb(110, 120, 66);
				return Varied(Mixed(T, GrassAt(S, BundGrass), 0.5 * Smooth(1.0, 0.0, S.Edge)), 0.96 + 0.06 * S.N2, 0.0);
			}
			Tone Open(const Spot& S) const
			{
				switch (S.Crop)
				{
				case Paddy:
				case Terrace: return PaddyColour(S);
				case Garden: return GardenColour(S);
				case Hill:
				{
					Tone T = GrassAt(S, HillGrass);
					T = Mixed(T, Rgb(78, 90, 52), 0.3 * Smooth(0.1, 0.6, S.N1)); // (scrub in its patches)
					return Mixed(T, Varied(Soil(), 0.9, 0.0), 0.2 * Smooth(0.3, 0.8, S.N2));
				}
				default:
				{
					Tone T = GrassAt(S, OpenGrass);
					// (worn to the earth along the paths the villagers take, and round the graves)
					return Mixed(T, Soil(), 0.18 * Smooth(0.4, 0.9, S.N2) + 0.1 * Smooth(0.5, 0.9, S.N1));
				}
				}
			}

			Tone Colour(const Spot& S) const override
			{
				switch (S.Kind)
				{
				case KLane:
				{
					// a raised earth track between the paddies: its floor beaten bare, red in the south, the wheels' two ruts
					// darker, a grassed verge at its edges
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double Rut = std::exp(-std::pow((Off - 0.8) / 0.25, 2.0));
					Tone T = Varied(Soil(), 1.0 + 0.05 * S.N2, 0.0);
					T = Mixed(T, Varied(Soil(), 0.82, 0.0), 0.5 * Rut);
					return Mixed(T, GrassAt(S, BundGrass), Smooth(S.LaneHalf - 0.6, S.LaneHalf + 0.6, Off));
				}
				case KVillage: return Mixed(GrassAt(S, OpenGrass), Varied(Soil(), 1.04, 0.0), 0.2 + 0.35 * Smooth(0.0, 0.6, S.N1)); // (grass between the houses, beaten to earth in patches)
				case KYard: return Mixed(GrassAt(S, OpenGrass), Varied(Mixed(Soil(), Rgb(160, 150, 128), 0.35), 1.0, 0.0), 0.3 + 0.3 * Smooth(-0.3, 0.5, S.N1)); // (the farmyards' ground: grass worn to beaten earth in patches)
				case KWoods: return Mixed(GrassAt(S, HillGrass), Needles, 0.45 + 0.2 * S.N1);
				case KWater: return Rgb(92, 104, 100);
				case KMarsh: return Mixed(GrassAt(S, Rgb(98, 112, 64)), Water, 0.25 * Smooth(0.0, 0.7, S.N1));
				case KWorks: return Mixed(GrassAt(S, OpenGrass), Varied(Soil(), 0.85, 0.0), 0.6);
				default: break;
				}
				return Open(S);
			}

			Tone Far(const Spot& S, bool bWoods, double /*Gap*/) const override
			{
				// (out in the ring a parcel is one plot's stage, read from its seed; the woods a dark pine canopy; the hills
				// wooded above their terraces)
				Tone T;
				if (bWoods) T = Varied(PineCanopy, 0.95 + 0.1 * S.Broad, 0.0);
				else if (S.Crop == Paddy || S.Crop == Terrace || (S.Crop < 0 && S.Kind == KField)) T = StageColour(StageOf(S.Key, S.Key), S.Key);
				else if (S.Crop == Garden) T = Mixed(Rgb(92, 116, 56), Mud, bWinter ? 0.5 : 0.2);
				else T = Open(S);
				const double Hl = HillAt(S.X, S.Y);
				if (Hl > 0.05)
				{
					const Tone Wood = Varied(Mixed(PineCanopy, HillGrass, 0.25 + 0.3 * Smooth(0.2, 0.8, Fbm(S.X, S.Y, 300.0, 2, Seed ^ 0xA53))), 0.95 + 0.1 * S.Broad, 0.0);
					T = Mixed(T, Wood, Smooth(0.08, 0.3, Hl));
				}
				return T;
			}

			// ---------------------------------------------------------------- round the eye

			Grown Grow(const Spot& S, double Pick) const override
			{
				if (S.Kind == KWater || S.Drawn == 9 /*floor*/ || S.Drawn == 10 /*wall*/) return Grown{};
				if (SnowLying > 0.25)
				{
					// under the snow: the paddies' stubble through it, dead grass and reeds here and there, nothing green
					if (S.Kind == KMarsh) return Grown{ ClutOf(CReed), 0.4 };
					if ((S.Crop == Paddy || S.Crop == Terrace) && S.Kind != KWoods && S.Kind != KLane && S.Kind != KVillage && S.Kind != KYard)
						return PlotOf(S).Stage == SStubble ? Grown{ ClutOf(CStubble), 0.45 } : Grown{};
					return Pick < 0.4 ? Grown{ ClutOf(CGrass), 0.25 } : Grown{};
				}
				if (S.Kind == KLane) return S.Lane > -0.4 ? Grown{ ClutOf(CGrass), 0.5 } : Grown{};
				if (S.Kind == KVillage) return Pick < 0.75 ? Grown{ ClutOf(CGrass), 0.9 * Smooth(0.6, 0.0, S.N1) + 0.1 } : Grown{ ClutOf(CCrop), 0.5 * Smooth(-0.6, 0.0, -S.N1) }; // (and the kitchen gardens' greens in their patches)
				if (S.Kind == KYard) return Pick < 0.7 ? Grown{ ClutOf(CGrass), 0.4 } : Grown{ ClutOf(CStubble), 0.1 };
				if (S.Kind == KWorks) return Grown{ ClutOf(CGrass), 0.15 };
				if (S.Kind == KMarsh) return Pick < 0.6 ? Grown{ ClutOf(CReed), 0.7 } : Grown{ ClutOf(CGrass), 1.0 };
				if (S.Kind == KWoods) return Grown{ ClutOf(CGrass), 0.6 };
				switch (S.Crop)
				{
				case Paddy:
				case Terrace:
				{
					const PlotAt P = PlotOf(S);
					if (S.Crop == Paddy && P.Bund < 0.16) return Grown{ ClutOf(CBund), 2.0 };
					if (S.Edge < 0.4) return Grown{ ClutOf(CGrass), 1.0 };
					switch (P.Stage)
					{
					case SSeedling: return Grown{ ClutOf(CRice), 0.55 };
					case SYoung:
					case SGreen:
					case SRipe: return Grown{ ClutOf(CRice), 0.6 };
					case SStubble: return Grown{ ClutOf(CStubble), 0.55 };
					case SMud: return Pick < 0.15 ? Grown{ ClutOf(CStubble), 0.2 } : Grown{};
					case SWinter: return Grown{ ClutOf(CStubble), 0.7 }; // (the winter wheat's short green rows: the stubble's mesh, green)
					case SRape: return Grown{ ClutOf(CCrop), 1.4 };
					default: return Grown{};
					}
				}
				case Garden:
					if (S.Edge < 0.6) return Grown{ ClutOf(CGrass), 1.0 };
					if (bWinter || Month == 3) return Grown{ ClutOf(CStubble), 0.7 };
					return Grown{ ClutOf(CCrop), Month == 4 && bRape ? 1.2 : 0.9 };
				case Hill: return Grown{ ClutOf(CGrass), 1.5 };
				default: return Pick < 0.9 ? Grown{ ClutOf(CGrass), 1.4 } : Grown{ ClutOf(CCrop), 0.3 };
				}
			}

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				const int K = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				const bool bPaddy = S.Crop == Paddy || S.Crop == Terrace;
				switch (K)
				{
				case CRice:
				case CStubble:
				{
					// (the hills in their rows: the rows along the grain, a row every 0.3 m across it; on the terraces along
					// the slope as it falls)
					// (a thing stays where it is laid: the boxes round the eye that overlap must lay the same things)
					const PlotAt Pl = bPaddy ? PlotOf(S) : PlotAt{};
					if (S.Crop == Paddy) P.Yaw = static_cast<float>(S.Angle + (U7 < 0.5 ? 0.0 : Pi));
					P.Scale = 1.0f;
					if (K == CStubble && (Pl.Stage == SWinter || S.Crop == Garden))
					{
						// (the winter wheat a hand high in its rows)
						P.ScaleZ = static_cast<float>(0.9 + 0.5 * U5);
						P.Tint = Tint(0.6, 1.2 + 0.08 * U6, 0.6);
						return true;
					}
					if (K == CStubble)
					{
						P.ScaleZ = static_cast<float>(0.8 + 0.4 * U5);
						P.Tint = bWinter ? Tint(0.95 + 0.06 * U6, 0.92 + 0.05 * U6, 0.9) : Tint(1.0 + 0.06 * U6, 1.0, 0.95);
						return true;
					}
					const double L = 0.94 + 0.12 * U6;
					switch (Pl.Stage)
					{
					case SSeedling:
						P.ScaleZ = static_cast<float>(0.3 + 0.1 * U5);
						P.Tint = Tint(0.8 * L, 1.05 * L, 0.8 * L);
						break;
					case SYoung:
						P.ScaleZ = static_cast<float>(0.55 + 0.15 * U5);
						P.Tint = Tint(0.75 * L, 1.02 * L, 0.75 * L);
						break;
					case SRipe:
						P.ScaleZ = static_cast<float>(0.95 + 0.15 * U5);
						P.Tint = Tint(1.59 * L, 0.89 * L, 0.94 * L); // (the tips some 185, 152, 70: gold; the tint is on the linear colour)
						break;
					default:
						P.ScaleZ = static_cast<float>(0.9 + 0.2 * U5);
						P.Tint = Tint(0.7 * L, 1.0 * L, 0.72 * L); // (the tips some 128, 160, 61)
						break;
					}
					return true;
				}
				case CBund:
				{
					// (a length of bund laid on its line, turned along it)
					const PlotAt Pl = PlotOf(S);
					P.Yaw = static_cast<float>(Pl.Way + (U7 < 0.5 ? 0.0 : Pi));
					P.Scale = static_cast<float>(0.85 + 0.3 * U4);
					P.ScaleZ = static_cast<float>(0.75 + 0.4 * U5);
					P.Tint = Green > 0.5 ? Tint(0.92 + 0.08 * U6, 0.98 + 0.06 * U6, 0.9) : Tint(1.1 + 0.06 * U6, 1.02, 0.9);
					return true;
				}
				case CGrass:
					P.Scale = static_cast<float>(0.6 + 0.7 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.7 * U5);
					P.Tint = Green > 0.5 ? Tint(0.96 + 0.08 * U7, 1.0 + 0.04 * U6, 0.92) : Tint(1.12 + 0.06 * U7, 1.04, 0.88);
					return true;
				case CCrop:
				{
					P.Scale = static_cast<float>(0.8 + 0.4 * U4);
					const PlotAt Pl = bPaddy ? PlotOf(S) : PlotAt{};
					const bool bRapeStrip = !bPaddy && Month == 4 && bRape && Unit(Hash(S.Key, static_cast<uint64_t>(std::floor(S.Across / (2.5 + 2.0 * Unit(Hash(S.Key, 0x6A2)))) + 512.0))) < 0.35;
					if ((bPaddy && Pl.Stage == SRape) || bRapeStrip)
					{
						P.ScaleZ = static_cast<float>(1.1 + 0.3 * U5);
						P.Tint = Tint(1.99, 1.6, 0.4); // (its flowers some 204, 173, 36; the tint is on the linear colour)
					}
					else if (bPaddy || bWinter || Month == 3)
					{
						// (winter wheat, a hand high)
						P.ScaleZ = static_cast<float>(0.4 + 0.2 * U5);
						P.Tint = Tint(0.75, 1.0 + 0.06 * U6, 0.8);
					}
					else
					{
						// (the vegetables, each strip its own green)
						const double U = Unit(Hash(Hash(S.Key, static_cast<uint64_t>(std::floor(S.Across / (2.5 + 2.0 * Unit(Hash(S.Key, 0x6A2)))) + 512.0)), 0x7E6));
						P.ScaleZ = static_cast<float>(0.6 + 0.5 * U5);
						P.Tint = U < 0.4 ? Tint(0.75, 1.05, 0.8) : U < 0.7 ? Tint(0.62, 0.9, 0.72) : Tint(0.9, 1.06, 0.8);
					}
					return true;
				}
				case CReed:
					P.Scale = static_cast<float>(0.8 + 0.5 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					P.Tint = Green > 0.5 ? Tint(0.96, 1.0, 0.94) : Tint(1.12, 1.02, 0.86);
					return true;
				default: return false;
				}
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				switch (K)
				{
				case CRice:
					// a patch of rice 1.2 m square: four rows 0.3 m apart along its x, four hills to a row, each hill a
					// sheaf of blades fanning up and out (as grown: 0.75 m; the seedlings and the young rice by their
					// height); the rows' hills a little out of line, as hands plant them
					for (int R = 0; R < 4; ++R)
						for (int H = 0; H < 4; ++H)
						{
							const uint64_t Hs = Hash(0x51CE, static_cast<uint64_t>(R * 4 + H));
							const double X = -0.45 + 0.3 * H + 0.05 * (Unit(Hash(Hs, 1)) - 0.5), Y = -0.45 + 0.3 * R + 0.03 * (Unit(Hash(Hs, 2)) - 0.5);
							Tuft(W, X, Y, 5, 0.03, 0.55, 0.25, 0.14, 0.016, Hs, Rgb(86, 100, 44), Rgb(150, 160, 72)); // (between the green and the gold: its tint makes either)
						}
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 40.0f;
					break;
				case CStubble:
					// the cut hills: short straw stubs in the same rows, a little loose straw lying between
					for (int R = 0; R < 4; ++R)
						for (int H = 0; H < 4; ++H)
						{
							const uint64_t Hs = Hash(0x57BB, static_cast<uint64_t>(R * 4 + H));
							const double X = -0.45 + 0.3 * H + 0.05 * (Unit(Hash(Hs, 1)) - 0.5), Y = -0.45 + 0.3 * R + 0.03 * (Unit(Hash(Hs, 2)) - 0.5);
							Tuft(W, X, Y, 4, 0.025, 0.1, 0.06, 0.03, 0.014, Hs, Rgb(128, 112, 78), Rgb(178, 160, 112));
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
				case CBund:
				{
					// a bund 3 m long along its x: earth heaped some 0.18 m high, 0.5 m across its foot and 0.26 m across its
					// rounded top, its ends sunk, grassed, the grass thick along its crown
					const Tone Top = Rgb(106, 116, 66), Side = Rgb(98, 104, 64), Foot = Rgb(92, 92, 66);
					const double Hl = 1.5, Hb = 0.25, Ht = 0.13, Z = 0.18;
					uint32_t V[8];
					V[0] = W.Vert(-Hl - 0.2, -Hb, -0.06, 0.0, -0.6, 0.8, Foot);
					V[1] = W.Vert(Hl + 0.2, -Hb, -0.06, 0.0, -0.6, 0.8, Foot);
					V[2] = W.Vert(Hl, -Ht, Z, 0.0, -0.2, 0.98, Side);
					V[3] = W.Vert(-Hl, -Ht, Z, 0.0, -0.2, 0.98, Side);
					V[4] = W.Vert(-Hl, Ht, Z, 0.0, 0.2, 0.98, Top);
					V[5] = W.Vert(Hl, Ht, Z, 0.0, 0.2, 0.98, Top);
					V[6] = W.Vert(Hl + 0.2, Hb, -0.06, 0.0, 0.6, 0.8, Foot);
					V[7] = W.Vert(-Hl - 0.2, Hb, -0.06, 0.0, 0.6, 0.8, Foot);
					W.Tri(V[0], V[1], V[2]);
					W.Tri(V[0], V[2], V[3]);
					W.Tri(V[3], V[2], V[5]);
					W.Tri(V[3], V[5], V[4]);
					W.Tri(V[4], V[5], V[6]);
					W.Tri(V[4], V[6], V[7]);
					const size_t Grass = M.Near.Points();
					for (int T = 0; T < 10; ++T) Tuft(W, -1.4 + 0.31 * T, 0.0, 6, 0.12, 0.1, 0.18, 0.1, 0.015, Hash(0xB0D, T), Rgb(88, 102, 56), Rgb(140, 152, 88));
					for (size_t P = Grass; P < M.Near.Points(); ++P) M.Near.Positions[3 * P + 2] += static_cast<float>(Z);
					LitUp(M.Near, Grass);
					M.Paint = 2;
					M.FarFrom = 60.0f;
					break;
				}
				case CGrass:
					// a clump of sward: thirty-odd blades over some 0.6 m, a hand or two high
					Tuft(W, 0.0, 0.0, 34, 0.3, 0.08, 0.16, 0.08, 0.016, 0x6A55, Rgb(88, 100, 56), Rgb(146, 154, 92));
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 45.0f;
					break;
				case CCrop:
					// a crop's clump over some 0.4 m: two dozen narrow leaves (winter wheat, the vegetables' rows, the rape:
					// by its tint and height), a few seed heads standing over them
					Tuft(W, 0.0, 0.0, 24, 0.18, 0.22, 0.16, 0.1, 0.022, 0xC40B, Rgb(72, 96, 46), Rgb(124, 146, 74));
					for (int B = 0; B < 5; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0xC40C, B)), D = 0.12 * Unit(Hash(0xC40D, B));
						W.Bead(D * std::cos(A), D * std::sin(A), 0.36 + 0.08 * Unit(Hash(0xC40E, B)), 0.03, 0.05, Rgb(150, 140, 60), Rgb(112, 112, 56)); // (an ear, or the rape's flowers, by the tint)
					}
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 40.0f;
					break;
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
				case KPine: return "HillPine";
				case KBamboo: return "Bamboo";
				case KWillow: return "Willow";
				case KHouse: return "ChinaHouse";
				case KYardHouse: return "Farmyard";
				case KThatch: return "ThatchHouse";
				case KTower: return "TieredTower";
				case KGrave: return "GraveMound";
				case KStack: return "StrawStack";
				case KOutcrop: return "RockOutcrop";
				case KBoulder: return "Boulder";
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
				case KPine:
					M.bTree = true;
					M.Paint = 1;
					M.FarFrom = 90.0f;
					Sight(0.72f, 0.28f, 0.7f, 0.35f);
					break;
				case KBamboo:
					M.bTree = true;
					M.Paint = 1;
					M.FarFrom = 80.0f;
					Sight(0.55f, 0.45f, 0.8f, 0.2f);
					break;
				case KWillow:
					M.bTree = true;
					M.Paint = 1;
					M.FarFrom = 70.0f;
					Sight(0.6f, 0.4f, 0.8f, 0.3f);
					break;
				case KHouse:
				case KYardHouse:
				case KThatch:
					M.FarFrom = 200.0f;
					Sight(0.5f, 0.5f, 0.85f, 0.0f);
					break;
				case KTower:
					M.FarFrom = 400.0f;
					Sight(0.5f, 0.5f, 0.6f, 0.0f);
					break;
				case KGrave:
					M.FarFrom = 120.0f;
					Sight(0.5f, 0.5f, 0.8f, 0.0f);
					break;
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
				Rng R(H.Seed() ^ 0xA51A);
				int Built = 0, Yards = 0;
				// The village: houses close-packed in rows, every one facing south (the village turned a little as its
				// ground lies), a lane's width between the rows; one in six a mud house under thatch.
				const double Turn = R.Range(-0.18, 0.18), Ct = std::cos(Turn), St = std::sin(Turn);
				const double Reach = 0.85 * TR;
				for (double V = -Reach; V <= Reach; V += 12.5)
					for (double U = -Reach; U <= Reach; U += 13.6)
					{
						const double JU = R.Range(-1.2, 1.2), JV = R.Range(-1.0, 1.0), Gap = R.Next(), Kind = R.Next(), Yaw = R.Range(-0.06, 0.06);
						const double PU = U + JU, PV = V + JV;
						// (the village's edge ragged, its houses thinning out)
						const double Out = std::hypot(PU, PV) / Reach;
						if (Out > 1.0 || Gap < 0.12 + 0.55 * Smooth(0.55, 1.0, Out)) continue;
						const double X = TX + PU * Ct - PV * St, Y = TY + PU * St + PV * Ct;
						const bool bThatch = Kind < 0.16;
						if (H.House(KitOf(bThatch ? KThatch : KHouse), X, Y, Turn + Yaw, bThatch ? 10.2 : 12.2, bThatch ? 7.2 : 7.6)) ++Built;
					}
				// A walled farmyard or two at its edge: the house across the yard's north side, the yard to the south.
				// (where the rules lay farmyards, first: one to each yard's ground it fits)
				for (int K = 0; K < 240 && Yards < 5; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(0.6 * TR, TR + 160.0), Yaw = R.Range(-0.08, 0.08);
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A);
					const bool bYard = H.At(X, Y).Kind == KYard;
					if ((bYard || (K >= 160 && D < TR + 60.0)) && H.House(KitOf(KYardHouse), X, Y, Turn + Yaw, 21.4, 23.4)) ++Yards;
				}
				// The pagoda: on the highest free ground near the village, one village in two.
				if (R.Next() < 0.5)
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
				H.Tally(Built + Yards, 0, Yards);
			}

			static void Put(Host& H, LandField::Base B, double X, double Y, double Room, double Yaw, double Scale, double ScaleZ, LandField::Rgb8 Tn)
			{
				if (H.Free(X, Y, Room)) H.Plant(B, X, Y, Yaw, Scale, ScaleZ, Tn);
			}

			void Plants(Host& H) override
			{
				const double Bd = H.Band();
				constexpr double Cell = 8.0;
				const int NI = static_cast<int>(std::ceil((H.Width() + 2.0 * Bd) / Cell)), NJ = static_cast<int>(std::ceil((H.Depth() + 2.0 * Bd) / Cell));
				const uint64_t Key0 = H.Seed() ^ 0xA5A7;
				double TX = 0.0, TY = 0.0, TR = 0.0;
				const bool bTown = H.Town(TX, TY, TR);
				const bool bStacks = Month >= 9 || Month <= 3;
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
						// (bamboo round the villages: a grove in every lee)
						const double Village = bTown ? Smooth(TR + 260.0, TR + 40.0, std::hypot(X - TX, Y - TY)) : 0.0;
						// a bamboo grove: three to six clumps close together, never one alone
						auto Grove = [&](double GX, double GY) {
							const int Clumps = 3 + R.Below(4);
							for (int C = 0; C < Clumps; ++C)
							{
								const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(0.0, 6.0), GYaw = R.Range(0.0, 2.0 * Pi), GS = R.Range(0.45, 0.7), GZ = R.Range(0.8, 1.1);
								Put(H, KitOf(KBamboo), GX + D * std::cos(A), GY + D * std::sin(A), 2.5, GYaw, GS, GZ, Leaf);
							}
						};
						switch (S.Kind)
						{
						case KWoods:
							if (Keep < 0.34 - 0.2 * Village) Put(H, KitOf(KPine), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							else if (Keep < 0.34 - 0.2 * Village + 0.03 * Village) Grove(X, Y);
							break;
						case KRough:
							if (S.Crop == Hill)
							{
								if (Keep < 0.06) Put(H, KitOf(KPine), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
								else if (Keep < 0.075) Put(H, KitOf(KOutcrop), X, Y, 4.0, Yaw, 0.6 + 0.6 * (Sc - 0.8), Sz, Tint(1.0, 1.0, 1.0));
								else if (Keep < 0.1) Put(H, KitOf(KBoulder), X, Y, 1.5, Yaw, Sc, Sz, Tint(1.0, 1.0, 1.0));
							}
							else if (Keep < 0.006) Put(H, KitOf(KGrave), X, Y, 1.8, Yaw, Sc, Sz, Tint(1.0, 1.0, 1.0));
							break;
						case KVillage:
						case KYard:
							if (Keep < 0.012) Grove(X, Y);
							else if (Keep < 0.06) Put(H, KitOf(KWillow), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							else if (bStacks && Keep < 0.2) Put(H, KitOf(KStack), X, Y, 1.6, Yaw, Sc, Sz, Tint(1.0, 1.0, 1.0));
							break;
						case KMarsh:
							if (Keep < 0.03) Put(H, KitOf(KWillow), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							break;
						default:
						{
							// the open ground and the fields: graves in their family groups, willows along the tracks, bamboo near
							// the village, straw stacks on the paddies' edges after the harvest
							const bool bOpen = S.Kind == KOpen;
							if (Keep < (bOpen ? 0.004 : 0.0012))
							{
								const int Graves = 2 + R.Below(4);
								for (int G = 0; G < Graves; ++G)
								{
									const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(0.0, 5.0), GS = R.Range(0.8, 1.25), GY = R.Range(-0.3, 0.3);
									Put(H, KitOf(KGrave), X + D * std::cos(A), Y + D * std::sin(A), 1.6, GY, GS, GS, Tint(1.0, 1.0, 1.0));
								}
							}
							else if (Keep < 0.006 + 0.03 * Smooth(8.0, 3.0, S.Lane)) Put(H, KitOf(KWillow), X, Y, 1.5, Yaw, Sc, Sz, Leaf);
							else if (bOpen && Keep < 0.0065 + 0.006 * Village) Grove(X, Y);
							else if (bStacks && S.Crop == Paddy && S.Edge < 3.0 && Keep < 0.09) Put(H, KitOf(KStack), X, Y, 1.6, Yaw, Sc, Sz, Tint(1.0, 1.0, 1.0));
							break;
						}
						}
					}
			}

			int Fx(const std::string& Ground, int Else) const override
			{
				// (a shell in a flooded or a wet paddy throws up water and mud)
				if ((Ground == "field" || Ground == "fields" || Ground == "paddy") && Month >= 4 && Month <= 9) return 4;
				if (Ground == "rough" || Ground == "rock") return 7;
				return Else;
			}
			const char* Earth() const override { return Red > 0.5 ? "laterite" : "loam"; }

		private:
			int Month = 7;
			bool bWinter = false, bRape = false, bLate = false, bDelta = false;
			double Green = 1.0, Red = 0.0, MeanZ = 0.0, SnowLying = 0.0;
		};
	}

	std::unique_ptr<Region> MakeAsia(const LandField::Field& F, uint64_t S) { return std::make_unique<Asia>(F, S); }
}
