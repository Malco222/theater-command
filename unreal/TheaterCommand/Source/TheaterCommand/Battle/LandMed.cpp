#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * Italy and the Mediterranean (Sicily 1943, the mainland 1943-45, Greece and Crete 1941): the art director's look targets,
 * section 2. The field itself is the valley's floor, the rules' heights kept: small holdings of olives in loose
 * irregular rows, bush vines in their rows, wheat (gold stubble after the July harvest, ploughed or green with the sown
 * corn in winter), dry fallow, maquis on the rough; white dusty roads between dry-stone walls; a stone farmhouse with a
 * low terracotta roof on each holding, its haystacks and its dark cypresses; the village round the rules' town with its
 * bell tower and its walled cemetery of cypresses. Round the field the valley's sides rise in dry-stone terraces, and
 * beyond them the hills, always on the horizon, a hill town on the crest. By the month: Sicily's July (gold stubble,
 * dead straw grass, white dust), the Winter Line's and Cassino's winter (brown mud, wet green-brown grass, bare vines,
 * snow on the high ridges), the spring's green corn, the autumn's turning vines. In the south (Sicily, below 38.6
 * degrees) the recording's woods are old olive groves and its open ground ochre fallow with macchia in clumps, as the
 * art director's dry south set (battle-models/med/sets/drysouth.json); on the mainland its woods are holm oak.
 * Kit: battle-models/med.
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Olive = 0,  // an olive grove: old trees in loose rows, ploughed earth or dry grass under them
			Vine = 1,   // bush vines in rows
			Grain = 2,  // wheat: green in winter, ripe in May and June, stubble after the harvest, ploughed in the autumn
			Fallow = 3, // grazing and fallow: grass, thistles, stones
			Maquis = 4, // the rough: macchia scrub, garrigue and limestone
			Plough = 5  // bare ploughed earth
		};

		enum Slot : int
		{
			KOlive = 0,
			KCypress = 1,
			KMacchia = 2,
			KFarm = 3,
			KTower = 4,
			KStones = 5,
			KBoulder = 6,
			KHouseStone = 7,
			KPole = 8,
			KSangar = 9
		};

		enum Clut : int
		{
			CGrass = 0, // a tuft of grass (straw in summer, green-brown in winter: its tint)
			CStalk = 1, // dry stalks: wild fennel, thistle
			CStone = 2, // pale limestone stones
			CVine = 3,  // a bush vine (in leaf, turning or bare by the month)
			CShrub = 4, // garrigue: a low dark mound of thyme and cistus
			CClod = 5   // ploughed earth's clods
		};

		LandField::Base KitOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K); }
		LandField::Base ClutOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Clut0) + K); }

		const Tone RFresh = Tone{ 92, 116, 58 };

		/** A terrace's step: its riser (metres of height a tread), the share of it the tread falls, the share of a tread's
		 * run the riser takes. */
		constexpr double Rise = 2.2, TreadFall = 0.14, RiserRun = 0.2;

		struct Farm
		{
			double X = 0.0, Y = 0.0, Yaw = 0.0;
			// its track out to the lane, and its threshing floor
			bool bTrack = false;
			double AX = 0.0, AY = 0.0, BX = 0.0, BY = 0.0;
			double FloorX = 0.0, FloorY = 0.0;
		};

		struct Exit
		{
			double X = 0.0, Y = 0.0, DX = 0.0, DY = 0.0;
		};

		/** One choice of the clutter: a share of the candidates and its kind. */
		struct Share
		{
			double Part;
			LandField::Base Kind;
		};
		Grown Of(double Pick, double Total, std::initializer_list<Share> Parts)
		{
			double Sum = 0.0;
			for (const Share& P : Parts)
			{
				Sum += P.Part;
				if (Pick < Sum) return Grown{ P.Kind, Total };
			}
			return Grown{};
		}

		double SegDist(double PX, double PY, double AX, double AY, double BX, double BY)
		{
			const double DX = BX - AX, DY = BY - AY, L2 = DX * DX + DY * DY;
			const double T = L2 > 1e-9 ? std::clamp(((PX - AX) * DX + (PY - AY) * DY) / L2, 0.0, 1.0) : 0.0;
			return std::hypot(PX - AX - T * DX, PY - AY - T * DY);
		}

		class Med final : public Region
		{
		public:
			Med(const LandField::Field& F, uint64_t S) : Region(F, S)
			{
				FW = F.Width();
				FH = F.Depth();
				const int M = std::clamp(F.Month, 1, 12) - 1;
				// (the year in the valleys: the summer drought from June to September, the rains from October, mud from
				// November to March, the green of the corn and the grass from the autumn rains to May)
				static const double DryOf[12] = { 0.0, 0.0, 0.05, 0.2, 0.5, 0.85, 1.0, 1.0, 0.85, 0.45, 0.1, 0.0 };
				static const double WetOf[12] = { 1.0, 1.0, 0.75, 0.4, 0.1, 0.0, 0.0, 0.0, 0.05, 0.3, 0.8, 1.0 };
				static const double GreenOf[12] = { 0.45, 0.5, 0.75, 0.95, 0.7, 0.25, 0.05, 0.0, 0.05, 0.3, 0.45, 0.45 };
				static const double LeafOf[12] = { 0.0, 0.0, 0.0, 0.3, 0.8, 1.0, 1.0, 1.0, 1.0, 0.9, 0.35, 0.0 };
				static const double TurnOf[12] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.05, 0.2, 0.7, 1.0, 0.0 };
				static const int StageOf[12] = { 0, 0, 0, 0, 1, 2, 2, 2, 2, 3, 3, 0 };
				Dry = DryOf[M];
				Wet = WetOf[M];
				Green = GreenOf[M];
				Leaf = LeafOf[M];
				Turn = TurnOf[M];
				Stage = StageOf[M];
				bSouth = F.Lat < 38.6;
				// (the far ridges white in the winter months: Monte Cairo over Cassino)
				bSnowCaps = (M <= 2 || M == 11) && F.Lat > 39.5;

				// The hill town's crest: beyond the field, behind the rules' town as the attack looks at it.
				const double TX = F.bTown ? F.TownX : 0.5 * FW;
				HillX = std::clamp(TX - 220.0, 150.0, FW - 150.0);
				HillY = FH + 620.0;
				HillTop = 0.0;
				{
					const double D = Out(HillX, HillY);
					HillTop = Hills(HillX, HillY, D, SideAt(HillX, HillY)) + 110.0;
				}

				// The roads out of the field (LandField runs each lane that reaches the field's edge on out through the
				// band, straight, drifting up to 30 m aside): the terraces leave them a way up.
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
			}

			Look Which() const override { return Look::Med; }

			// ------------------------------------------------------------------ the land

			/** Metres outside the field (0 inside). */
			double Out(double X, double Y) const
			{
				const double DX = std::max({ 0.0, -X, X - FW }), DY = std::max({ 0.0, -Y, Y - FH });
				return std::sqrt(DX * DX + DY * DY);
			}
			/** How high the valley's sides rise round the field (they differ side to side). */
			double SideAt(double X, double Y) const { return 58.0 + 22.0 * Noise(X / 1500.0, Y / 1500.0, Seed ^ 0x5D1); }
			/** The land's rise beyond the field: the valley's sides, then the hills. */
			double Hills(double X, double Y, double D, double Side) const
			{
				double E = Side * Smooth(0.0, 620.0, D);
				if (D > 300.0)
				{
					const double F = Fbm(X, Y, 2300.0, 2, Seed ^ 0x6B1);
					const double Ridge = 1.0 - std::fabs(Noise(X / 1600.0 + 0.37, Y / 1600.0 - 0.21, Seed ^ 0x6B2));
					const double Mount = 120.0 + 130.0 * F + 110.0 * Ridge * Ridge;
					E += std::max(40.0, Mount) * Smooth(300.0, 2700.0, D);
				}
				return E;
			}
			/** The hill town's crest's share of its height at a place (C its distance from the crest, east-west stretched;
			 * D outside the field): a flat top, steep sides falling some 500 m round, nothing at the field's edge. */
			static double HillPart(double C, double D) { return Smooth(600.0, 120.0, C) * Smooth(100.0, 300.0, D); }
			/** The distance from the hill town's crest as HillPart takes it: stretched east-west, and the face toward the
			 * field steep (its foot at the band's outer edge, so the band below it is terraced as the valley's sides are). */
			double CrestC(double X, double Y) const
			{
				const double DY = Y - HillY;
				return std::hypot((X - HillX) / 1.7, DY < 0.0 ? DY / 0.62 : DY);
			}
			/** The same with the hill town's crest. */
			double Lift(double X, double Y, double D, double Side) const
			{
				const double E = Hills(X, Y, D, Side);
				const double C = CrestC(X, Y);
				if (C >= 600.0) return E;
				const double Crest = HillTop * HillPart(C, D);
				return 0.5 * (E + Crest + std::sqrt((E - Crest) * (E - Crest) + 400.0));
			}
			/** How much of a terrace's step is laid here (0 none, 1 in full): where the valley's side is steep enough to
			 * terrace, within the band (the ring's mesh cannot hold them), not all of it (some slopes left to the maquis),
			 * and never across a road's way up. */
			double Terraced(double X, double Y, double D, double Side) const
			{
				if (D < 40.0 || D > 345.0) return 0.0;
				const double T = D / 620.0;
				const double Slope = Side * 6.0 * T * (1.0 - T) / 620.0;
				double W = Smooth(0.05, 0.085, Slope) * Smooth(345.0, 285.0, D);
				if (W <= 0.0) return 0.0;
				W *= Smooth(-0.15, 0.1, Noise(X / 520.0, Y / 520.0, Seed ^ 0x7E1));
				// (not up the hill town's steep sides: scrub and rock there)
				W *= Smooth(0.08, 0.02, HillPart(CrestC(X, Y), D));
				if (W <= 0.0) return 0.0;
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
			/** A terrace's step at a height: a tread falling gently outward, then the riser (its deviation from the slope). */
			static double Step(double E)
			{
				const double U = E / Rise, F = U - std::floor(U);
				const double P = F < 1.0 - RiserRun ? TreadFall * F / (1.0 - RiserRun) : TreadFall + (1.0 - TreadFall) * Smooth(1.0 - RiserRun, 1.0, F);
				return Rise * (P - F);
			}

			double Relief(double X, double Y, double Z) const override
			{
				const double D = Out(X, Y);
				if (D <= 0.0) return Z; // (the field: the rules' own heights)
				const double Side = SideAt(X, Y);
				const double E = Lift(X, Y, D, Side);
				const double W = Terraced(X, Y, D, Side);
				return Z + E + (W > 0.0 ? W * Step(E) : 0.0);
			}
			double Rolling(double X, double Y, double MeanZ) const override { return MeanZ + 5.0 * Fbm(X, Y, 1500.0, 2, Seed ^ 0xB1); }
			double LaneDepth(double /*X*/, double /*Y*/) const override { return 0.3; }
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 3.0 : 2.4) + 0.8 * U; }

			// ------------------------------------------------------------------ the parcels

			double PlotMin() const override { return 14000.0; }
			double PlotMax() const override { return 36000.0; }
			double FarTilled() const override { return 0.55; }
			double WoodsEdge() const override { return 0.32; }
			int CropOf(int Kind, double U) const override
			{
				if (Kind == KRough) return Maquis;
				if (Kind == KField) return U < 0.34 ? Olive : U < 0.52 ? Vine : U < 0.88 ? Grain : Plough;
				return U < 0.6 ? Fallow : U < 0.76 ? Olive : U < 0.9 ? Grain : Maquis;
			}
			const char* CropWord(int C) const override
			{
				switch (C)
				{
				case Olive: return "olives";
				case Vine: return "vines";
				// (the harvested wheat is not the bocage's "stubble": the test reads that word as Normandy's mid-July, its
				// harvest barely begun; Sicily's was in by the end of June)
				case Grain: return Stage == 2 ? "wheat stubble" : Stage == 3 ? "ploughed" : "wheat";
				case Fallow: return "fallow";
				case Maquis: return "maquis";
				case Plough: return "ploughed";
				default: return "";
				}
			}

			// ------------------------------------------------------------------ colours

			/** The earth: a pale limestone soil, or the red-brown terra rossa in broad patches; wet in the winter. */
			Tone Soil(double X, double Y, double N1) const
			{
				// (Sicily's interior mostly the pale grey-brown of its clays and marls, the red in patches)
				const double Red = Smooth(-0.2, 0.3, Noise(X / 380.0, Y / 380.0, Seed ^ 0x501) + 0.25 * N1 + (bSouth ? -0.28 : 0.05));
				const Tone Pale = Mixed(Rgb(188, 176, 148), Rgb(204, 198, 182), Smooth(0.2, 0.7, N1));
				const Tone T = Mixed(Pale, Rgb(162, 96, 64), Red);
				return Mixed(T, Rgb(100, 82, 62), 0.58 * Wet);
			}
			Tone Ploughed(const Tone& Earth, double N2) const
			{
				// (the furrows' shade and the clods' lit tops; darker and browner wet)
				return Varied(Mixed(Earth, Rgb(112, 92, 72), 0.4 + 0.1 * Wet), 0.84 + 0.2 * N2, 0.0);
			}
			/** Grass by the month: dead straw in the summer, wet green-brown in the winter, green in the spring. */
			Tone Grass(double N1, double N2) const
			{
				// (ochre, the art director's fallow 9C925C round it; its patches of thistle and dead weed green-brown, strong
				// enough to read in the noon sun)
				Tone Summer = Mixed(Rgb(150, 136, 92), Rgb(186, 168, 116), Smooth(-0.5, 0.3, N1));
				Summer = Mixed(Summer, Rgb(98, 98, 62), 0.75 * Smooth(0.05, 0.4, N2));
				// (the winter's: wet green-brown, last summer's dead grass in brown patches, never lime)
				Tone Winter = Mixed(Rgb(130, 114, 80), Rgb(104, 118, 66), Clamp01(Green + 0.25 * N1));
				Winter = Mixed(Winter, Rgb(126, 106, 74), 0.6 * Smooth(0.05, 0.4, N2));
				return Mixed(Winter, Summer, Dry);
			}
			Tone Dust() const { return Mixed(Rgb(156, 142, 120), Rgb(230, 222, 204), Dry); }

			Tone Ground(int C, double X, double Y, double N1, double N2, uint64_t Key) const
			{
				const Tone Earth = Soil(X, Y, N1);
				switch (C)
				{
				case Olive:
				{
					// (half the groves ploughed clean under the trees, the rest left to the grass)
					if (Unit(Mix(Key ^ 0x01E1)) < 0.55) return Mixed(Ploughed(Earth, N2), Grass(N1, N2), 0.15 + 0.2 * Smooth(0.2, 0.7, N1));
					return Mixed(Grass(N1, N2), Earth, 0.25 + 0.2 * Smooth(0.0, 0.6, N2));
				}
				case Vine:
				{
					const Tone Leaves = Mixed(Rgb(92, 114, 52), Mixed(Rgb(198, 162, 66), Rgb(156, 80, 46), 0.4 + 0.3 * N1), Turn);
					const Tone Stocks = Rgb(112, 96, 78);
					return Mixed(Mixed(Ploughed(Earth, N2), Stocks, 0.12), Leaves, 0.32 * Leaf);
				}
				case Grain:
					switch (Stage)
					{
					case 2:
					{
						// gold stubble: straw rows over pale earth, paler where the straw lies, the earth through it here and there
						const Tone Straw = Mixed(Rgb(188, 162, 106), Rgb(210, 186, 128), Smooth(-0.2, 0.6, N1));
						return Mixed(Straw, Earth, 0.15 + 0.35 * Smooth(-0.1, 0.6, N2));
					}
					case 3: return Ploughed(Earth, N2);
					case 1: return Mixed(Rgb(150, 156, 78), Rgb(204, 182, 106), Clamp01(0.5 + 0.6 * N1));
					default: return Mixed(Ploughed(Earth, N2), Rgb(98, 128, 56), Green * (0.38 + 0.15 * N1));
					}
				case Maquis:
				{
					// thin soil, limestone breaking through, the scrub's dark clumps
					Tone T = Mixed(Mixed(Rgb(154, 136, 98), Earth, 0.3), Rgb(204, 196, 176), 0.6 * Smooth(0.15, 0.6, N2));
					return Mixed(T, Mixed(Rgb(78, 94, 56), Rgb(88, 100, 62), Wet), 0.6 * Smooth(0.0, 0.4, N1));
				}
				case Plough: return Ploughed(Earth, N2);
				default:
				{
					// grazing: dead grass, the bare stony earth showing through in patches
					const Tone Bare = Mixed(Earth, Rgb(204, 198, 182), 0.3);
					Tone T = Mixed(Grass(N1, N2), Bare, 0.1 + 0.55 * Smooth(-0.3, -0.75, N2) * Smooth(-0.3, 0.3, N1));
					// (and the thin stony ground of the macchia in patches, its scrub in dark clumps)
					const Tone Scrubby = Mixed(Mixed(Rgb(142, 126, 89), Earth, 0.2), Rgb(84, 92, 58), 0.35 * Smooth(0.0, 0.5, N2));
					return Mixed(T, Scrubby, 0.8 * Smooth(0.3, 0.5, N1));
				}
				}
			}

			/** The farms' yards, threshing floors and tracks (Sites lays them): T where none is near. */
			Tone Farmyard(const Tone& T, double X, double Y, double N2) const
			{
				if (FarmGrid.empty()) return T;
				const int I = static_cast<int>(std::floor((X + Margin) / FarmCell)), J = static_cast<int>(std::floor((Y + Margin) / FarmCell));
				if (I < 0 || J < 0 || I >= FarmNX || J >= FarmNY) return T;
				Tone Out = T;
				for (const uint32_t K : FarmGrid[static_cast<size_t>(J) * FarmNX + I])
				{
					const Farm& F = Farms[K];
					const double D = std::hypot(X - F.X, Y - F.Y);
					// beaten earth round the house, dust in the summer, mud in the winter
					if (D < 26.0) Out = Mixed(Out, Mixed(Rgb(112, 96, 76), Rgb(204, 188, 156), Dry), 0.85 * Smooth(24.0, 12.0, D + 3.0 * N2));
					// the threshing floor: a round of laid stones, straw on it after the harvest
					const double Q = std::hypot(X - F.FloorX, Y - F.FloorY);
					if (Q < 7.0) Out = Mixed(Out, Mixed(Rgb(176, 168, 150), Rgb(218, 200, 152), 0.6 * Dry), Smooth(7.0, 5.5, Q));
					if (F.bTrack)
					{
						const double R = SegDist(X, Y, F.AX, F.AY, F.BX, F.BY);
						if (R < 2.6) Out = Mixed(Out, Varied(Dust(), 0.96, 0.0), 0.9 * Smooth(2.6, 1.1, R + 0.3 * N2));
					}
				}
				return Out;
			}

			Tone Colour(const Spot& S) const override
			{
				Tone T;
				switch (S.Kind)
				{
				case KLane:
				{
					// a white road: its crown and its wheel ruts, its dust on the verges (in the winter a grey-brown slurry,
					// the ruts wet)
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double Rut = std::exp(-((Off - 0.85) / 0.28) * ((Off - 0.85) / 0.28));
					const Tone Crown = Varied(Dust(), 1.0 + 0.03 * S.N2, 0.0);
					const Tone Pressed = Mixed(Rgb(102, 90, 74), Rgb(204, 196, 176), Dry);
					T = Mixed(Crown, Pressed, 0.55 * Rut);
					if (Wet > 0.5 && S.N2 > 0.35) T = Mixed(T, Rgb(92, 94, 96), 0.6 * Rut * Smooth(0.35, 0.6, S.N2)); // (puddles)
					// off the floor: the verge, dusty
					if (S.Lane > -0.4) T = Mixed(T, Mixed(Grass(S.N1, S.N2), Dust(), 0.45 * Dry), Smooth(-0.4, 1.6, S.Lane));
					return T;
				}
				case KVillage:
				{
					// the village's yards and gardens: beaten earth, the gardens' green
					// the village's yards and gardens: beaten earth and dust, the gardens' rows of beans and tomatoes green
					// against it even in the summer (watered from the well)
					const Tone Earth = Mixed(Rgb(118, 100, 80), Rgb(194, 176, 142), Dry);
					T = Mixed(Earth, Soil(S.X, S.Y, S.N1), 0.35);
					T = Mixed(T, Mixed(RFresh, Rgb(112, 122, 66), 0.5 * Dry), 0.7 * Smooth(-0.05, 0.3, S.N1) * Smooth(-0.6, 0.0, S.N2));
					return Varied(T, 1.0 + 0.06 * S.N2, 0.0);
				}
				case KYard: return Varied(Mixed(Rgb(116, 100, 80), Rgb(202, 186, 152), Dry), 1.0 + 0.05 * S.N2, 0.0);
				case KWater: return Mixed(Rgb(84, 98, 90), Rgb(96, 104, 96), Dry);
				case KMarsh: return Varied(Mixed(Rgb(98, 108, 66), Rgb(158, 148, 98), Dry), 1.0 + 0.06 * S.N1, 0.0);
				case KWorks:
				{
					// (the positions on ground too hard to dig: the macchia's stony ground, the spoil of what was dug in
					// pale heaps)
					const double Patch = Noise(S.X / 15.0, S.Y / 15.0, Seed ^ 0x9A1);
					T = Ground(Maquis, S.X, S.Y, S.N1, Patch, S.Key);
					return Varied(Mixed(T, Rgb(196, 188, 166), 0.5 * Smooth(0.1, 0.5, S.N2)), 1.0 + 0.04 * Patch, 0.0);
				}
				case KWoods:
					if (bSouth) break; // (the south's woods are its old olive groves: their ground as theirs)
					T = Mixed(Mixed(Rgb(110, 98, 72), Rgb(92, 86, 64), Wet), Rgb(78, 92, 56), 0.35 + 0.3 * S.N1);
					return Varied(T, 1.0 + 0.06 * S.N2, 0.0);
				default: break;
				}
				const int C = S.Crop >= 0 ? S.Crop : S.Kind == KRough ? Maquis : S.Kind == KWoods ? Olive : Fallow;
				// (the crops' own mottle some 15 m across: the ground's N2 is finer than the near ground's points, 4 m apart,
				// and reads there as one flat colour)
				const double Patch = Noise(S.X / 15.0, S.Y / 15.0, Seed ^ 0x9A1);
				T = Ground(C, S.X, S.Y, S.N1, Patch, S.Key);
				// (the roads' dust laid on the ground beside them)
				if (S.Lane < 3.5) T = Mixed(T, Dust(), 0.45 * Dry * Smooth(3.5, 0.0, S.Lane));
				T = Farmyard(T, S.X, S.Y, Patch);
				return Varied(T, 1.0 + 0.04 * Patch + 0.04 * S.Broad, 0.012 * S.Broad);
			}

			/** The land's rise beyond the field and its slope there (from the coarse grid Sites lays, else worked out). */
			void LiftAt(double X, double Y, double& OutE, double& OutSlope) const
			{
				if (LiftGrid.empty())
				{
					const double Side = SideAt(X, Y);
					OutE = Lift(X, Y, Out(X, Y), Side);
					const double EX = Lift(X + 30.0, Y, Out(X + 30.0, Y), Side), EY = Lift(X, Y + 30.0, Out(X, Y + 30.0), Side);
					OutSlope = std::hypot(EX - OutE, EY - OutE) / 30.0;
					return;
				}
				const double U = (X - GridX0) / GridStep, V = (Y - GridY0) / GridStep;
				const int I = std::clamp(static_cast<int>(std::floor(U)), 0, GridNX - 2), J = std::clamp(static_cast<int>(std::floor(V)), 0, GridNY - 2);
				const double A = Clamp01(U - I), B = Clamp01(V - J);
				const size_t K = static_cast<size_t>(J) * GridNX + I;
				const double E00 = LiftGrid[K], E10 = LiftGrid[K + 1], E01 = LiftGrid[K + GridNX], E11 = LiftGrid[K + GridNX + 1];
				OutE = Lerp(Lerp(E00, E10, A), Lerp(E01, E11, A), B);
				OutSlope = std::hypot(Lerp(E10 - E00, E11 - E01, B), Lerp(E01 - E00, E11 - E10, A)) / GridStep;
			}

			Tone Far(const Spot& S, bool bWoods, double Gap) const override
			{
				(void)Gap;
				double E = 0.0, Slope = 0.0;
				LiftAt(S.X, S.Y, E, Slope);
				const Tone Canopy = Mixed(Rgb(58, 72, 44), Rgb(70, 78, 54), 0.5 + 0.5 * S.N1);
				Tone T;
				if (bWoods && bSouth) T = Mixed(Mixed(Rgb(108, 116, 84), Soil(S.X, S.Y, S.N1), 0.35), Canopy, 0.25); // (old olive groves)
				else if (bWoods) T = Canopy;
				else
				{
					const int C = S.Crop >= 0 ? S.Crop : Fallow;
					T = Ground(C, S.X, S.Y, S.N1, 0.0, S.Key);
					// (the hills: their middle slopes terraced and grey-green with olives, the steep ones scrub and rock, the
					// high ground under oak and chestnut woods and the maquis, dark)
					const double Steep = Smooth(0.12, 0.3, Slope + 0.05 * S.N1);
					const double Mid = Smooth(0.045, 0.11, Slope) * (1.0 - Steep);
					const Tone Grove = Mixed(Mixed(Rgb(124, 132, 98), Soil(S.X, S.Y, S.N1), 0.3), Grass(S.N1, 0.0), 0.15);
					T = Mixed(T, Grove, 0.7 * Mid * Smooth(-0.5, 0.0, S.N1));
					const Tone Scrub = Mixed(Rgb(66, 82, 50), Rgb(176, 170, 150), Smooth(0.2, 0.7, S.N1));
					T = Mixed(T, Scrub, Steep);
					const double High = Smooth(30.0, 120.0, E) * Smooth(-0.15, 0.25, Noise(S.X / 650.0, S.Y / 650.0, Seed ^ 0x3D1) + 0.3 * S.N1);
					T = Mixed(T, Canopy, 0.85 * High);
				}
				if (bSnowCaps) T = Mixed(T, Rgb(228, 230, 236), Smooth(250.0, 330.0, E + 40.0 * S.N1));
				return Varied(T, 1.0 + 0.05 * S.Broad, 0.01 * S.Broad);
			}

			// ------------------------------------------------------------------ the clutter round the eye

			/** The vines' rows in a parcel: how far apart, and where the first runs. */
			static double RowGap(uint64_t Key) { return 2.0 + 0.4 * Unit(Mix(Key ^ 0x71E5)); }
			static double RowNear(const Spot& S)
			{
				const double G = RowGap(S.Key), A = S.Across - G * Unit(Mix(S.Key ^ 0x71E6));
				return A - G * std::round(A / G);
			}

			Grown Grow(const Spot& S, double Pick) const override
			{
				if (S.Drawn == 9 || S.Drawn == 11 || S.Kind == KWater) return Grown{};
				const LandField::Base Grass_ = ClutOf(CGrass), Stalk = ClutOf(CStalk), Stone = ClutOf(CStone), Shrub = ClutOf(CShrub), Clod = ClutOf(CClod);
				if (S.Kind == KLane || S.Drawn == 7)
				{
					if (S.Lane < -0.7) return Of(Pick, 0.06, { { 0.8, Stone }, { 0.2, Clod } }); // (the road: a few stones)
					return Of(Pick, 0.55, { { 0.7, Grass_ }, { 0.18, Stalk }, { 0.12, Stone } });
				}
				// (in patches: the spot comes without the ground's noises, so its own, cheap)
				const double Clump = Smooth(-0.4, 0.4, Noise(S.X / 5.3, S.Y / 5.3, Seed ^ 0xC1));
				const double Thick = 0.55 + 0.9 * Clump;
				if (S.Drawn == 10) return Of(Pick, 0.45, { { 0.6, Grass_ }, { 0.25, Stalk }, { 0.15, Stone } }); // (a wall's foot)
				switch (S.Kind)
				{
				case KVillage:
				case KYard: return Of(Pick, 0.18, { { 0.6, Grass_ }, { 0.4, Stone } });
				case KWorks: return Of(Pick, 0.15, { { 1.0, Stone } });
				case KMarsh: return Of(Pick, 1.0, { { 0.7, Grass_ }, { 0.3, Stalk } });
				case KWoods:
					if (!bSouth) return Of(Pick, 0.6 * Thick, { { 0.45, Shrub }, { 0.35, Grass_ }, { 0.2, Stone } });
					break;
				default: break;
				}
				const int C = S.Crop >= 0 ? S.Crop : S.Kind == KRough ? Maquis : Fallow;
				const LandField::Base Sward = LandField::Base::Sward;
				switch (C)
				{
				case Vine:
					if (S.Edge > 1.2 && std::fabs(RowNear(S)) < 0.22) return Grown{ ClutOf(CVine), 1.7 };
					return Of(Pick, 0.3, { { 0.6, Clod }, { 0.3, Grass_ }, { 0.1, Stone } });
				case Grain:
					if (Stage == 2) return Of(Pick, 1.0, { { 0.86, LandField::Base::Stubble }, { 0.06, Grass_ }, { 0.04, Stalk }, { 0.04, Stone } });
					if (Stage == 3) return Of(Pick, 0.5, { { 0.8, Clod }, { 0.15, Stone }, { 0.05, Grass_ } });
					if (Stage == 1) return Of(Pick, 0.9, { { 0.8, Grass_ }, { 0.15, Clod }, { 0.05, Stone } });
					return Of(Pick, 0.8, { { 0.55, Grass_ }, { 0.4, Clod }, { 0.05, Stone } });
				case Plough: return Of(Pick, 0.5, { { 0.8, Clod }, { 0.15, Stone }, { 0.05, Grass_ } });
				case Olive:
					if (Unit(Mix(S.Key ^ 0x01E1)) < 0.55) return Of(Pick, 0.45, { { 0.6, Clod }, { 0.15, Stone }, { 0.25, Grass_ } });
					return Of(Pick, 0.8 * Thick, { { 0.35, Sward }, { 0.47, Grass_ }, { 0.1, Stalk }, { 0.08, Stone } });
				case Maquis: return Of(Pick, 0.8, { { 0.3, Shrub }, { 0.3, Stone }, { 0.3, Grass_ }, { 0.1, Stalk } });
				default: return Of(Pick, 1.25 * Thick, { { 0.3, Sward }, { 0.5, Grass_ }, { 0.12, Stalk }, { 0.05, Stone }, { 0.03, Shrub } });
				}
			}

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				const int C = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				if (C < 0 || C > 5) return false; // (the stubble along its rows and the sward in the ground's colour: LandField's)
				switch (C)
				{
				case CGrass:
				{
					// most tufts small, a few big; straw in the summer, green-brown in the winter, a few dead ones always
					const double Size = 0.55 + 0.85 * std::pow(U4, 1.5);
					const bool bShoots = S.Crop == Grain && Stage == 0;
					P.Scale = static_cast<float>(bShoots ? 0.45 + 0.2 * U4 : Size);
					P.ScaleZ = static_cast<float>(bShoots ? 0.4 + 0.2 * U5 : Size * (0.7 + 0.5 * U5));
					const double Dead = U6 < 0.15 ? 1.0 : Dry;
					const double G = bShoots ? 1.0 : Green;
					const double R = Lerp(Lerp(0.74, 0.6, G), 1.02 + 0.08 * U7, Dead), Gc = Lerp(Lerp(0.78, 0.8, G), 0.97 + 0.05 * U7, Dead), B = Lerp(Lerp(0.56, 0.5, G), 0.86, Dead);
					P.Tint = Tint(R, Gc, B);
					return true;
				}
				case CStalk:
				{
					P.Scale = static_cast<float>(0.7 + 0.5 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.7 * U5);
					const double L = 0.9 + 0.2 * U6;
					P.Tint = Tint(L * Lerp(0.9, 1.04, Dry), L * Lerp(0.92, 1.0, Dry), L * Lerp(0.86, 0.94, Dry));
					return true;
				}
				case CStone:
				{
					P.Scale = static_cast<float>(0.5 + 1.3 * U4 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.5 * U5);
					const double L = 0.86 + 0.22 * U6;
					P.Tint = Tint(L * 1.02, L, L * 0.96);
					return true;
				}
				case CVine:
				{
					// onto its row's line, its stock leaning a little
					const double Off = RowNear(S);
					P.X -= static_cast<float>(-std::sin(S.Angle) * Off);
					P.Y -= static_cast<float>(std::cos(S.Angle) * Off);
					P.Scale = static_cast<float>(0.8 + 0.4 * U4);
					P.ScaleZ = static_cast<float>(0.85 + 0.35 * U5);
					const double L = 0.92 + 0.16 * U6;
					P.Tint = Tint(L * (1.0 + 0.08 * U7), L, L * (1.0 - 0.06 * U7));
					return true;
				}
				case CShrub:
				{
					P.Scale = static_cast<float>(0.6 + 0.8 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.6 * U5);
					const double L = 0.88 + 0.24 * U6;
					P.Tint = Tint(L * (0.96 + 0.1 * U7), L, L * 0.96);
					return true;
				}
				default:
				{
					// clods in the earth's own colour
					P.Scale = static_cast<float>(0.7 + 0.6 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.4 * U5);
					const Tone E = Ploughed(Soil(P.X, P.Y, 0.0), 0.0);
					const double L = (0.9 + 0.2 * U6) / 128.0;
					P.Tint = Tint(E.R * L, E.G * L, E.B * L);
					return true;
				}
				}
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				Writer W(M.Near);
				Rng R(Seed ^ (0xC107ull + static_cast<uint64_t>(K) * 977ull));
				switch (K)
				{
				case CGrass:
				{
					// a tuft: a score of blades, built straw-pale (the tint gives the month its colour)
					for (int B = 0; B < 30; ++B)
					{
						const double A = R.Range(0.0, 2.0 * Pi), Out0 = R.Range(0.0, 0.16), High = R.Range(0.12, 0.36), Lean = R.Range(0.08, 0.3);
						const double X0 = Out0 * std::cos(A), Y0 = Out0 * std::sin(A);
						const double L = R.Range(0.85, 1.12), Wide = R.Range(0.016, 0.026);
						W.Blade(X0, Y0, -0.01, X0 + Lean * std::cos(A), Y0 + Lean * std::sin(A), High, Wide, Varied(Rgb(150, 136, 100), L, 0.0), Varied(Rgb(206, 192, 146), L, 0.0));
					}
					// (a few gone to seed, their heads pale)
					for (int B = 0; B < 5; ++B)
					{
						const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.38, 0.55), Lean = R.Range(0.06, 0.16);
						const double X1 = Lean * std::cos(A), Y1 = Lean * std::sin(A);
						W.Blade(0.0, 0.0, -0.01, X1, Y1, High, 0.01, Rgb(150, 136, 98), Rgb(196, 182, 140));
						W.Blade(X1, Y1, High - 0.07, X1 * 1.15, Y1 * 1.15, High + 0.02, 0.024, Rgb(200, 186, 146), Rgb(218, 206, 168));
					}
					M.FarFrom = 45.0f;
					M.Paint = 2;
					break;
				}
				case CStalk:
				{
					// wild fennel and thistle gone to seed: a few stems, their heads
					const int Stems = 4;
					for (int B = 0; B < Stems; ++B)
					{
						const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.5, 0.95), Lean = R.Range(0.03, 0.14);
						const double X1 = Lean * std::cos(A), Y1 = Lean * std::sin(A);
						W.Blade(0.0, 0.0, -0.02, X1, Y1, High, 0.025, Rgb(118, 100, 72), Rgb(150, 128, 90));
						for (int H = 0; H < 5; ++H)
						{
							const double B2 = R.Range(0.0, 2.0 * Pi), Spray = R.Range(0.05, 0.1), Top = R.Range(0.02, 0.07);
							W.Blade(X1, Y1, High - 0.02, X1 + Spray * std::cos(B2), Y1 + Spray * std::sin(B2), High + Top, 0.018, Rgb(134, 114, 80), Rgb(168, 146, 102));
						}
					}
					M.FarFrom = 50.0f;
					M.Paint = 2;
					break;
				}
				case CStone:
				{
					// a few pieces of pale limestone, one bigger
					W.Rock(0.0, 0.0, 0.16, 0.1, 31, Rgb(214, 206, 188), Rgb(150, 140, 122));
					W.Rock(0.24, 0.1, 0.07, 0.05, 32, Rgb(206, 198, 180), Rgb(144, 134, 118));
					W.Rock(-0.18, 0.16, 0.06, 0.04, 33, Rgb(196, 188, 170), Rgb(138, 128, 112));
					M.FarFrom = 40.0f;
					break;
				}
				case CVine:
				{
					// a bush vine (alberello): its gnarled stock some 0.5 m, its arms; in leaf from May, turning yellow and
					// red-brown in October, bare from November to March
					const Tone Bark = Rgb(104, 88, 70), BarkLit = Rgb(132, 114, 92);
					W.Box(0.0, 0.0, 0.25, 0.035, 0.035, 0.26, 0.3, Bark);
					for (int A = 0; A < 5; ++A)
					{
						const double Th = A * 2.0 * Pi / 5.0 + R.Range(-0.3, 0.3), Reach = R.Range(0.14, 0.26);
						const double X1 = Reach * std::cos(Th), Y1 = Reach * std::sin(Th), Z1 = R.Range(0.62, 0.78);
						W.Blade(0.0, 0.0, 0.48, X1, Y1, Z1, 0.04, Bark, BarkLit);
						// (the canes: pruned short spurs in the winter, long and leafy in the summer)
						const int Canes = Leaf > 0.2 ? 2 : 1;
						for (int C = 0; C < Canes; ++C)
						{
							const double Up = Leaf > 0.2 ? R.Range(0.35, 0.6) : R.Range(0.08, 0.16);
							const double OutX = R.Range(1.2, 2.0), OutY = R.Range(1.2, 2.0);
							W.Blade(X1, Y1, Z1, X1 * OutX, Y1 * OutY, Z1 + Up, 0.018, BarkLit, Rgb(140, 120, 92));
						}
					}
					if (Leaf > 0.05)
					{
						const int Leaves = static_cast<int>(10.0 + 50.0 * Leaf);
						for (int L = 0; L < Leaves; ++L)
						{
							const double Th = R.Range(0.0, 2.0 * Pi), Rad = R.Range(0.05, 0.48) * (0.6 + 0.4 * Leaf), Z = R.Range(0.5, 1.15);
							const double X = Rad * std::cos(Th), Y = Rad * std::sin(Th);
							const double Dir = R.Range(0.0, 2.0 * Pi), Long = R.Range(0.1, 0.17);
							const double Pick = R.Next(), Light = R.Range(0.9, 1.1), Rise_ = R.Range(-0.05, 0.08), Wide = R.Range(0.1, 0.15);
							Tone Lf = Varied(Mixed(Rgb(84, 110, 46), Rgb(112, 132, 58), Pick), Light, 0.0);
							const Tone Turned = Pick < 0.6 ? Rgb(200, 166, 64) : Pick < 0.85 ? Rgb(168, 96, 48) : Rgb(140, 70, 42);
							Lf = Mixed(Lf, Turned, Turn);
							W.Blade(X, Y, Z, X + Long * std::cos(Dir), Y + Long * std::sin(Dir), Z + Rise_, Wide, Lf, Varied(Lf, 1.1, 0.0));
						}
						M.Paint = 1;
					}
					M.FarFrom = 70.0f;
					break;
				}
				case CShrub:
				{
					// garrigue: a low mound of small dark leaves, grey-green on top
					for (int L = 0; L < 70; ++L)
					{
						const double Th = R.Range(0.0, 2.0 * Pi), U = R.Next();
						const double Rad = 0.34 * std::sqrt(U), Z = 0.3 * std::sqrt(std::max(0.0, 1.0 - U)) + R.Range(0.0, 0.05);
						const double X = Rad * std::cos(Th), Y = Rad * std::sin(Th), Dir = R.Range(0.0, 2.0 * Pi), Lit = R.Range(0.3, 1.0);
						const Tone Lf = Mixed(Rgb(70, 84, 48), Rgb(124, 128, 94), Smooth(0.08, 0.36, Z) * Lit);
						W.Blade(X, Y, Z - 0.03, X + 0.06 * std::cos(Dir), Y + 0.06 * std::sin(Dir), Z + 0.04, 0.06, Varied(Lf, 0.8, 0.0), Lf);
					}
					M.FarFrom = 55.0f;
					M.Paint = 1;
					break;
				}
				default:
				{
					// clods: a few lumps of earth, built grey-brown (the tint gives the earth's own colour)
					W.Rock(0.0, 0.0, 0.11, 0.07, 41, Rgb(136, 130, 124), Rgb(84, 80, 76));
					W.Rock(0.2, -0.08, 0.08, 0.05, 42, Rgb(132, 126, 120), Rgb(80, 76, 72));
					W.Rock(-0.12, 0.18, 0.06, 0.04, 43, Rgb(128, 122, 116), Rgb(78, 74, 70));
					M.FarFrom = 35.0f;
					break;
				}
				}
				M.Far = LandField::Mesh();
			}

			// ------------------------------------------------------------------ the kit

			const char* Folder() const override { return "Med"; }
			const char* KitName(int K) const override
			{
				switch (K)
				{
				case KOlive: return "OliveTree";
				case KCypress: return "Cypress";
				case KMacchia: return "Macchia";
				case KFarm: return "Farmhouse";
				case KTower: return "SlimTower";
				case KStones: return "Stones";
				case KBoulder: return "Boulder";
				case KHouseStone: return "FlatRoofHouseStone";
				case KPole: return "TelegraphPole";
				case KSangar: return "Sangar";
				default: return nullptr;
				}
			}
			void KitLook(int K, LandField::BaseMesh& M) const override
			{
				switch (K)
				{
				case KOlive:
					M.bSight = true;
					M.SightMid = 0.62f;
					M.SightHalf = 0.32f;
					M.SightAcross = 0.75f;
					M.SightTrunk = 0.3f;
					M.FarFrom = 120.0f;
					M.Paint = 1;
					M.bTree = true; // (far off a crown with no shadow, as the bocage's trees)
					break;
				case KCypress:
					M.bSight = true;
					M.SightMid = 0.5f;
					M.SightHalf = 0.48f;
					M.SightAcross = 0.85f;
					M.SightTrunk = 0.3f;
					M.FarFrom = 220.0f;
					M.Paint = 1;
					M.bTree = true;
					break;
				case KMacchia:
					M.bSight = true;
					M.SightMid = 0.5f;
					M.SightHalf = 0.5f;
					M.SightAcross = 0.8f;
					M.FarFrom = 90.0f;
					M.Paint = 1;
					break;
				case KFarm:
				case KHouseStone:
				case KTower:
					M.bSight = true;
					M.SightMid = 0.5f;
					M.SightHalf = 0.5f;
					M.SightAcross = 0.9f;
					M.FarFrom = 400.0f;
					break;
				case KBoulder:
				case KSangar:
					M.bSight = true;
					M.SightMid = 0.5f;
					M.SightHalf = 0.5f;
					M.SightAcross = 0.85f;
					M.FarFrom = 200.0f;
					break;
				case KPole: M.FarFrom = 220.0f; break;
				default: M.FarFrom = 90.0f; break;
				}
			}
			void KitPlain(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				switch (K)
				{
				case KFarm:
					W.Box(0.0, 0.0, 3.0, 7.0, 4.2, 3.0, 0.0, Rgb(204, 188, 156));
					W.Box(0.0, 0.0, 6.6, 7.3, 4.5, 0.6, 0.0, Rgb(170, 100, 70));
					break;
				case KHouseStone: W.Box(0.0, 0.0, 1.8, 5.5, 5.5, 1.8, 0.0, Rgb(214, 202, 176)); break;
				case KTower: W.Box(0.0, 0.0, 10.0, 1.6, 1.6, 10.0, 0.0, Rgb(206, 192, 164)); break;
				case KOlive:
					W.Box(0.0, 0.0, 1.0, 0.18, 0.18, 1.0, 0.0, Rgb(112, 106, 96));
					W.Rock(0.0, 0.0, 2.2, 3.6, 5, Rgb(132, 140, 108), Rgb(92, 98, 74));
					break;
				case KCypress: W.Box(0.0, 0.0, 6.0, 0.9, 0.9, 6.0, 0.0, Rgb(46, 60, 42)); break;
				case KMacchia: W.Rock(0.0, 0.0, 0.8, 1.1, 7, Rgb(76, 92, 54), Rgb(52, 64, 38)); break;
				case KPole: W.Box(0.0, 0.0, 3.5, 0.1, 0.1, 3.5, 0.0, Rgb(110, 96, 80)); break;
				default: W.Rock(0.0, 0.0, 0.9, 0.7, 11, Rgb(196, 186, 164), Rgb(130, 120, 104)); break;
				}
				M.Far = M.Near;
				M.FarFrom = 400.0f;
			}

			// ------------------------------------------------------------------ what stands on it

			void Sites(Host& H) override
			{
				Rng R(H.Seed() ^ 0x51E5);
				int Built = 0, Hurt = 0, Steads = 0;
				double TX = 0.0, TY = 0.0, TR = 0.0;
				const bool bTown = H.Town(TX, TY, TR);
				if (bTown) Village(H, R, TX, TY, TR, Built, Hurt);
				Holdings(H, R, bTown, TX, TY, TR, Built, Steads);
				H.Tally(Built, Hurt, Steads);
				// (the land's rise beyond the field on a 50 m grid out to the ring's edge, for the far ground's paint)
				GridX0 = -5600.0;
				GridY0 = -5600.0;
				GridNX = static_cast<int>(std::ceil((FW + 11200.0) / GridStep)) + 1;
				GridNY = static_cast<int>(std::ceil((FH + 11200.0) / GridStep)) + 1;
				LiftGrid.assign(static_cast<size_t>(GridNX) * GridNY, 0.0f);
				for (int J = 0; J < GridNY; ++J)
					for (int I = 0; I < GridNX; ++I)
					{
						const double X = GridX0 + I * GridStep, Y = GridY0 + J * GridStep;
						LiftGrid[static_cast<size_t>(J) * GridNX + I] = static_cast<float>(Lift(X, Y, Out(X, Y), SideAt(X, Y)));
					}
				// (the farms by 100 m squares, for the ground's paint)
				FarmNX = static_cast<int>(std::ceil((FW + 2.0 * Margin) / FarmCell));
				FarmNY = static_cast<int>(std::ceil((FH + 2.0 * Margin) / FarmCell));
				FarmGrid.assign(static_cast<size_t>(FarmNX) * FarmNY, {});
				for (size_t K = 0; K < Farms.size(); ++K)
				{
					const Farm& F = Farms[K];
					double X0 = F.X - 30.0, X1 = F.X + 30.0, Y0 = F.Y - 30.0, Y1 = F.Y + 30.0;
					if (F.bTrack)
					{
						X0 = std::min({ X0, F.AX - 4.0, F.BX - 4.0 });
						X1 = std::max({ X1, F.AX + 4.0, F.BX + 4.0 });
						Y0 = std::min({ Y0, F.AY - 4.0, F.BY - 4.0 });
						Y1 = std::max({ Y1, F.AY + 4.0, F.BY + 4.0 });
					}
					const int I0 = std::max(0, static_cast<int>(std::floor((X0 + Margin) / FarmCell))), I1 = std::min(FarmNX - 1, static_cast<int>(std::floor((X1 + Margin) / FarmCell)));
					const int J0 = std::max(0, static_cast<int>(std::floor((Y0 + Margin) / FarmCell))), J1 = std::min(FarmNY - 1, static_cast<int>(std::floor((Y1 + Margin) / FarmCell)));
					for (int J = J0; J <= J1; ++J)
						for (int I = I0; I <= I1; ++I) FarmGrid[static_cast<size_t>(J) * FarmNX + I].push_back(static_cast<uint32_t>(K));
				}
			}

			/** The village round the rules' town: its bell tower over a small piazza, its houses packed along the
			 * streets and behind them, stone and plaster under terracotta (some shell-struck), its cemetery of cypresses
			 * outside. */
			void Village(Host& H, Rng& R, double TX, double TY, double TR, int& Built, int& Hurt)
			{
				// the bell tower, near the middle
				for (int K = 0; K < 80; ++K)
				{
					const double A = K * 2.39, D = 4.0 + 1.3 * K;
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A), Yaw = R.Range(0.0, 0.5 * Pi);
					if (H.House(KitOf(KTower), X, Y, Yaw, 4.2, 4.2))
					{
						TowerX = X;
						TowerY = Y;
						bTower = true;
						++Built;
						break;
					}
				}
				// the piazza before it
				if (bTower) H.Clear(TowerX + 9.0, TowerY - 4.0, 9.0);
				auto Lay = [&](double X, double Y, double Yaw) {
					const double Pick = R.Next();
					const double Odds = R.Next();
					if (Pick < (bSouth ? 0.25 : 0.12))
					{
						if (H.House(KitOf(KHouseStone), X, Y, Yaw, 11.6, 11.2)) return 11.6;
					}
					else if (Pick < 0.62)
					{
						const double Long = R.Range(8.0, 12.0), Deep = R.Range(6.5, 8.0);
						const int Hit = Odds < 0.07 ? 3 : Odds < 0.15 ? 2 : Odds < 0.27 ? 1 : 0;
						if (H.Building(X, Y, Yaw, Long, Deep, 0, 2, 1, false, Hit))
						{
							if (Hit) ++Hurt;
							return Long;
						}
					}
					else if (H.House(KitOf(KFarm), X, Y, Yaw, 14.8, 8.8)) return 14.8;
					return 0.0;
				};
				// along the streets: house after house facing the road
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const std::vector<std::pair<double, double>>& Line = H.LaneLine(L);
					const double Half = H.LaneHalf(L);
					for (int Side = -1; Side <= 1; Side += 2)
					{
						double Skip = 0.0;
						for (size_t K = 1; K < Line.size(); ++K)
						{
							const double AX = Line[K - 1].first, AY = Line[K - 1].second, BX = Line[K].first, BY = Line[K].second;
							const double Len = std::hypot(BX - AX, BY - AY);
							if (Len < 1e-6) continue;
							Skip -= Len;
							if (Skip > 0.0) continue;
							if (std::hypot(BX - TX, BY - TY) > TR * 1.05) continue;
							const double UX = (BX - AX) / Len, UY = (BY - AY) / Len;
							const double Deep = 8.0;
							const double X = BX - UY * Side * (Half + 1.6 + 0.5 * Deep), Y = BY + UX * Side * (Half + 1.6 + 0.5 * Deep);
							const double Skew = R.Range(-0.04, 0.04);
							const double Laid = Lay(X, Y, std::atan2(UY, UX) + Skew);
							if (Laid > 0.0)
							{
								++Built;
								Skip = 0.5 * Laid + R.Range(4.5, 7.0);
							}
							else Skip = 1.0;
						}
					}
				}
				// and behind them, packed
				// (on the rules' built-up cells, and a few round them)
				const int Want = std::max(16, static_cast<int>(80.0 * (TR / 150.0) * (TR / 150.0)));
				for (int K = 0; K < 1500 && Built < Want; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = TR * std::sqrt(R.Next()) * 1.15;
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A);
					const double Skew = R.Range(-0.12, 0.12), Turn_ = R.Next();
					const double Yaw = Skew + (Turn_ < 0.5 ? 0.0 : 0.5 * Pi);
					const double Odds = R.Next();
					if (!CellBuilt(X, Y) && Odds > 0.15) continue;
					if (Lay(X, Y, Yaw) > 0.0) ++Built;
				}
				// the cemetery: a walled square outside the village, cypresses in rows inside its walls
				for (int K = 0; K < 40 && !bCemetery; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = TR * R.Range(1.15, 1.6);
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A), Yaw = A + R.Range(-0.2, 0.2);
					const double CA = std::cos(Yaw), SA = std::sin(Yaw);
					bool bFree = true;
					for (int U = -2; U <= 2 && bFree; ++U)
						for (int V = -2; V <= 2 && bFree; ++V)
						{
							const double PX = X + U * 8.5 * CA - V * 6.0 * SA, PY = Y + U * 8.5 * SA + V * 6.0 * CA;
							if (!H.Free(PX, PY, 2.5)) bFree = false;
						}
					if (!bFree) continue;
					bCemetery = true;
					CemX = X;
					CemY = Y;
					CemYaw = Yaw;
					// its cypresses: two rows along its long walls, a pair at its gate
					for (int Row = -1; Row <= 1; Row += 2)
						for (int U = -3; U <= 3; ++U)
						{
							const double PX = X + U * 5.0 * CA - Row * 9.0 * SA, PY = Y + U * 5.0 * SA + Row * 9.0 * CA;
							const double Sz = R.Range(1.0, 1.4), TreeYaw = R.Range(0.0, 2.0 * Pi), Wide = R.Range(0.9, 1.1);
							H.Plant(KitOf(KCypress), PX, PY, TreeYaw, Wide, Sz, Tint(1.0, 1.0, 1.0));
						}
					for (int Side = -1; Side <= 1; Side += 2)
					{
						const double PX = X - 21.0 * CA - Side * 3.0 * SA, PY = Y - 21.0 * SA + Side * 3.0 * CA;
						const double TreeYaw = R.Range(0.0, 2.0 * Pi), Tall = R.Range(1.2, 1.5);
						H.Plant(KitOf(KCypress), PX, PY, TreeYaw, 1.0, Tall, Tint(1.0, 1.0, 1.0));
					}
					H.Clear(X, Y, 19.0);
				}
			}

			/** The holdings: a stone farmhouse on each, some ten hectares apiece, its haystacks and its cypresses, a
			 * low stone shed on some; its track out to the nearest road, lined with cypresses on a few. */
			void Holdings(Host& H, Rng& R, bool bTown, double TX, double TY, double TR, int& Built, int& Steads)
			{
				const double B = H.Band(), Spacing = 330.0;
				for (double GY = -B + 0.5 * Spacing; GY < FH + B; GY += Spacing)
					for (double GX = -B + 0.5 * Spacing; GX < FW + B; GX += Spacing)
					{
						const double JX = R.Range(-0.35, 0.35) * Spacing, JY = R.Range(-0.35, 0.35) * Spacing;
						for (int Try = 0; Try < 6; ++Try)
						{
							const double X = GX + JX + R.Range(-40.0, 40.0), Y = GY + JY + R.Range(-40.0, 40.0);
							const double Skew = R.Range(-0.25, 0.25), Turn_ = R.Next();
							const double Yaw = Skew + (Turn_ < 0.5 ? 0.0 : 0.5 * Pi);
							if (X < -B + 30.0 || Y < -B + 30.0 || X > FW + B - 30.0 || Y > FH + B - 30.0) continue;
							if (bTown && std::hypot(X - TX, Y - TY) < TR * 1.5) continue;
							if (!CellOpen(X, Y) || !H.Free(X, Y, 9.0)) continue;
							// (on one tread: not across a terrace's riser)
							double Lo = 1e9, Hi = -1e9;
							for (int C = 0; C < 4; ++C)
							{
								const double U = (C & 1) ? 8.0 : -8.0, V = (C & 2) ? 5.0 : -5.0;
								const double Z = H.Land(X + U * std::cos(Yaw) - V * std::sin(Yaw), Y + U * std::sin(Yaw) + V * std::cos(Yaw));
								Lo = std::min(Lo, Z);
								Hi = std::max(Hi, Z);
							}
							if (Hi - Lo > 1.1) continue;
							if (!H.House(KitOf(KFarm), X, Y, Yaw, 14.8, 8.8)) continue;
							++Built;
							++Steads;
							Farm F;
							F.X = X;
							F.Y = Y;
							F.Yaw = Yaw;
							const double CA = std::cos(Yaw), SA = std::sin(Yaw);
							// a low stone shed beside it, on some
							const double ShedOdds = R.Next(), ShedSide = R.Next();
							if (ShedOdds < 0.35)
							{
								const double Side = ShedSide < 0.5 ? -1.0 : 1.0;
								if (H.House(KitOf(KHouseStone), X + Side * 15.5 * CA + 3.0 * SA, Y + Side * 15.5 * SA - 3.0 * CA, Yaw, 11.6, 11.2)) ++Built;
							}
							// the threshing floor before the house; haystacks round it
							const double FloorAlong = R.Range(-5.0, 5.0), FloorAcross = R.Range(-5.0, 5.0);
							F.FloorX = X - 15.0 * SA + FloorAlong * CA;
							F.FloorY = Y + 15.0 * CA + FloorAcross * SA;
							const int Stacks = 1 + R.Below(3);
							for (int K = 0; K < Stacks; ++K)
							{
								const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(9.0, 13.0);
								const double PX = F.FloorX + D * std::cos(A), PY = F.FloorY + D * std::sin(A);
								const double Sz = R.Range(2.0, 2.5), Tall = R.Range(2.4, 2.9);
								// (the summer's new straw gold; the winter's old stack weathered grey-brown, part eaten)
								const double Old = 1.0 - Dry;
								const double StackYaw = R.Range(0.0, 2.0 * Pi), Eaten = R.Next();
								if (H.Free(PX, PY, 2.6)) H.Plant(LandField::Base::Haycock, PX, PY, StackYaw, Sz, Sz * Tall * (1.0 - 0.25 * Old * Eaten), Tint(Lerp(1.12, 0.9, Old), Lerp(1.08, 0.88, Old), Lerp(0.92, 0.82, Old)));
							}
							// cypresses by the house
							const int Cyp = 1 + R.Below(3);
							for (int K = 0; K < Cyp; ++K)
							{
								const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(11.0, 16.0);
								const double PX = X + D * std::cos(A), PY = Y + D * std::sin(A);
								const double TreeYaw = R.Range(0.0, 2.0 * Pi), Wide = R.Range(0.9, 1.15), Tall = R.Range(1.0, 1.5);
								if (H.Free(PX, PY, 1.5)) H.Plant(KitOf(KCypress), PX, PY, TreeYaw, Wide, Tall, Tint(1.0, 1.0, 1.0));
							}
							// its track to the nearest road
							double Best = 170.0, BX = 0.0, BY = 0.0;
							for (size_t L = 0; L < H.Lanes(); ++L)
							{
								const std::vector<std::pair<double, double>>& Line = H.LaneLine(L);
								for (size_t K = 0; K < Line.size(); K += 2)
								{
									const double D = std::hypot(Line[K].first - X, Line[K].second - Y);
									if (D < Best)
									{
										Best = D;
										BX = Line[K].first;
										BY = Line[K].second;
									}
								}
							}
							if (Best < 170.0 && Best > 25.0)
							{
								F.bTrack = true;
								const double UX = (BX - X) / Best, UY = (BY - Y) / Best;
								F.AX = X + UX * 9.0;
								F.AY = Y + UY * 9.0;
								F.BX = BX;
								F.BY = BY;
								// lined with cypresses, on a few (a villa's drive)
								const double Avenue = R.Next();
								if (Avenue < 0.3)
									for (double S = 18.0; S < Best - 8.0; S += 8.0)
										for (int Side = -1; Side <= 1; Side += 2)
										{
											// (some 7 to 9 m apart)
											const double Along = R.Range(-1.0, 1.0), TreeYaw = R.Range(0.0, 2.0 * Pi), Wide = R.Range(0.9, 1.1), Tall = R.Range(1.15, 1.45);
											const double PX = X + UX * (S + Along) - UY * Side * 3.6, PY = Y + UY * (S + Along) + UX * Side * 3.6;
											if (H.Free(PX, PY, 1.2)) H.Plant(KitOf(KCypress), PX, PY, TreeYaw, Wide, Tall, Tint(1.0, 1.0, 1.0));
										}
							}
							Farms.push_back(F);
							break;
						}
					}
			}

			/** Whether the rules' cell here is built up (the town's). */
			bool CellBuilt(double X, double Y) const
			{
				const int I = std::clamp(static_cast<int>(std::floor(X / Fd.Cell)), 0, Fd.Cols - 1), J = std::clamp(static_cast<int>(std::floor(Y / Fd.Cell)), 0, Fd.Rows - 1);
				const size_t K = static_cast<size_t>(J) * Fd.Cols + I;
				if (K >= Fd.Ground.size()) return false;
				const int G = Fd.Ground[K];
				if (G < 0 || static_cast<size_t>(G) >= Fd.Grounds.size()) return false;
				const std::string& W = Fd.Grounds[static_cast<size_t>(G)];
				return W == "buildings" || W == "suburb" || W == "industry";
			}

			/** Whether the rules' cell here (the nearest at the field's edge, out in the band) is open or tilled land. */
			bool CellOpen(double X, double Y) const
			{
				const int I = std::clamp(static_cast<int>(std::floor(X / Fd.Cell)), 0, Fd.Cols - 1), J = std::clamp(static_cast<int>(std::floor(Y / Fd.Cell)), 0, Fd.Rows - 1);
				const size_t K = static_cast<size_t>(J) * Fd.Cols + I;
				if (K >= Fd.Ground.size()) return true;
				const int G = Fd.Ground[K];
				if (G < 0 || static_cast<size_t>(G) >= Fd.Grounds.size()) return true;
				const std::string& W = Fd.Grounds[static_cast<size_t>(G)];
				return W == "open" || W == "field" || W == "park";
			}

			// ------------------------------------------------------------------ walls

			/** Lay a run of wall, broken where it may not stand (a road's floor, a house, the water, out of the band),
			 * with a gap now and then and a stretch fallen low. */
			void Run(Host& H, Rng& R, const std::vector<std::pair<double, double>>& Line, double High, double Thick, double GapOdds, bool bTerrace)
			{
				std::vector<std::pair<double, double>> Cur;
				double Hi = High;
				auto Flush = [&]() {
					if (Cur.size() >= 3) H.Wall(Cur, Hi, Thick);
					Cur.clear();
					// (a stretch fallen or bulged: some one in six low)
					const double Fallen = R.Next(), Low = R.Range(0.45, 0.7), Even = R.Range(0.92, 1.06);
					Hi = High * (Fallen < 0.16 ? Low : Even);
				};
				double Since = 0.0, Piece = R.Range(18.0, 60.0);
				for (size_t K = 0; K < Line.size(); ++K)
				{
					const double X = Line[K].first, Y = Line[K].second;
					if (K > 0) Since += std::hypot(X - Line[K - 1].first, Y - Line[K - 1].second);
					const Spot S = H.At(X, Y);
					bool bOk = S.bLaid && S.Lane > 0.4 && !S.bNear && S.Drawn != 9 && S.Drawn != 11 && S.Drawn != 7 && S.Kind != KWater && S.Kind != KMarsh;
					if (bOk && bTerrace && S.Kind == KVillage) bOk = false;
					if (!bOk)
					{
						Flush();
						Since = 0.0;
						continue;
					}
					if (Since > Piece)
					{
						Since = 0.0;
						Piece = R.Range(18.0, 60.0);
						const double Gap = R.Next();
						if (Gap < GapOdds)
						{
							Flush();
							continue;
						}
						Cur.push_back({ X, Y });
						Flush();
					}
					Cur.push_back({ X, Y });
				}
				Flush();
			}

			void Walls(Host& H) override
			{
				Rng R(H.Seed() ^ 0xA115);
				Terraces(H, R);
				Fields(H, R);
				// The roads between dry-stone walls: along most of their length outside the village, both sides or one.
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const std::vector<std::pair<double, double>>& Line = H.LaneLine(L);
					const double Half = H.LaneHalf(L);
					for (int Side = -1; Side <= 1; Side += 2)
					{
						std::vector<std::pair<double, double>> Run_;
						double S = 0.0;
						for (size_t K = 0; K < Line.size(); ++K)
						{
							const size_t A = K > 0 ? K - 1 : K, B = K + 1 < Line.size() ? K + 1 : K;
							const double DX = Line[B].first - Line[A].first, DY = Line[B].second - Line[A].second, DL = std::hypot(DX, DY);
							if (DL < 1e-6) continue;
							if (K > 0) S += std::hypot(Line[K].first - Line[K - 1].first, Line[K].second - Line[K - 1].second);
							const double Off = Half + 1.7;
							const double X = Line[K].first - DY / DL * Side * Off, Y = Line[K].second + DX / DL * Side * Off;
							const bool bWalled = Noise(S / 110.0, static_cast<double>(L) * 7.3 + Side * 2.1, Seed ^ 0xA1A) > -0.15;
							if (!bWalled)
							{
								const double High = R.Range(0.9, 1.2);
								if (Run_.size() >= 3) Run(H, R, Run_, High, 0.6, 0.12, false);
								Run_.clear();
								continue;
							}
							Run_.push_back({ X, Y });
						}
						const double High = R.Range(0.9, 1.2);
						if (Run_.size() >= 3) Run(H, R, Run_, High, 0.6, 0.12, false);
					}
				}
				// The cemetery's walls.
				if (bCemetery)
				{
					const double CA = std::cos(CemYaw), SA = std::sin(CemYaw);
					auto P = [&](double U, double V) { return std::pair<double, double>{ CemX + U * CA - V * SA, CemY + U * SA + V * CA }; };
					std::vector<std::pair<double, double>> Ring = { P(-19.0, -2.5), P(-19.0, -12.0), P(19.0, -12.0), P(19.0, 12.0), P(-19.0, 12.0), P(-19.0, 2.5) };
					std::vector<std::pair<double, double>> Fine;
					for (size_t K = 0; K + 1 < Ring.size(); ++K)
					{
						const double L = std::hypot(Ring[K + 1].first - Ring[K].first, Ring[K + 1].second - Ring[K].second);
						const int N = std::max(1, static_cast<int>(std::ceil(L / 1.8)));
						for (int J = 0; J < N; ++J)
						{
							const double T = static_cast<double>(J) / N;
							Fine.push_back({ Lerp(Ring[K].first, Ring[K + 1].first, T), Lerp(Ring[K].second, Ring[K + 1].second, T) });
						}
					}
					Fine.push_back(Ring.back());
					H.Wall(Fine, 2.1, 0.55);
				}
			}
			std::unordered_set<uint64_t> WoodCells;

			/** Whether the edge between two parcels is walled: the groves' and the vineyards' most often, some of the rest. */
			bool Walled(const Spot& A, const Spot& B) const
			{
				for (const Spot* S : { &A, &B })
					if (S->Key == 0 || S->Kind == KLane || S->Kind == KWater || S->Kind == KMarsh || S->Kind == KVillage || S->Kind == KYard || !S->bLaid) return false;
				const uint64_t Pair = A.Key < B.Key ? Hash(A.Key, B.Key) : Hash(B.Key, A.Key);
				const bool bTrees = A.Crop == Olive || A.Crop == Vine || B.Crop == Olive || B.Crop == Vine;
				return Unit(Mix(Pair ^ 0xF1E1D)) < (bTrees ? 0.32 : 0.1);
			}

			/** The fields' walls: along a share of the parcels' edges, found where a coarse scan sees two parcels meet and
			 * traced finer there, then laid with their gateways. */
			void Fields(Host& H, Rng& R)
			{
				const double B = H.Band(), Coarse = 20.0, Fine = 4.0;
				const int CNX = static_cast<int>(std::ceil((FW + 2.0 * B) / Coarse)) + 1, CNY = static_cast<int>(std::ceil((FH + 2.0 * B) / Coarse)) + 1;
				std::vector<uint64_t> CKey(static_cast<size_t>(CNX) * CNY, 0);
				std::vector<Spot> CSpot(CKey.size());
				for (int J = 0; J < CNY; ++J)
					for (int I = 0; I < CNX; ++I)
					{
						const Spot S = H.At(-B + I * Coarse, -B + J * Coarse);
						CKey[static_cast<size_t>(J) * CNX + I] = S.Key;
						CSpot[static_cast<size_t>(J) * CNX + I] = S;
					}
				const int Per = static_cast<int>(Coarse / Fine);
				const int FNX = (CNX - 1) * Per + 1;
				std::unordered_map<uint64_t, Spot> Cache;
				auto FineAt = [&](int I, int J) -> const Spot& {
					const uint64_t K = static_cast<uint64_t>(J) * static_cast<uint64_t>(FNX) + static_cast<uint64_t>(I);
					auto Found = Cache.find(K);
					if (Found != Cache.end()) return Found->second;
					return Cache.emplace(K, H.At(-B + I * Fine, -B + J * Fine)).first->second;
				};
				std::unordered_map<uint64_t, std::vector<std::pair<double, double>>> Groups;
				std::unordered_set<uint64_t> EdgeDone;
				// a crossing between two fine points of different parcels, found to half a metre
				auto Crossing = [&](int IA, int JA, int IB, int JB) {
					const Spot& A = FineAt(IA, JA);
					const Spot& Bs = FineAt(IB, JB);
					if (A.Key == Bs.Key || !Walled(A, Bs)) return;
					const uint64_t EdgeK = (static_cast<uint64_t>(JA) * static_cast<uint64_t>(FNX) + static_cast<uint64_t>(IA)) * 2ull + (IB != IA ? 0ull : 1ull);
					if (!EdgeDone.insert(EdgeK).second) return;
					double T0 = 0.0, T1 = 1.0;
					const double AX = -B + IA * Fine, AY = -B + JA * Fine, BX = -B + IB * Fine, BY = -B + JB * Fine;
					for (int K = 0; K < 3; ++K)
					{
						const double T = 0.5 * (T0 + T1);
						if (H.At(Lerp(AX, BX, T), Lerp(AY, BY, T)).Key == A.Key) T0 = T;
						else T1 = T;
					}
					const double T = 0.5 * (T0 + T1);
					const uint64_t Pair = A.Key < Bs.Key ? Hash(A.Key, Bs.Key) : Hash(Bs.Key, A.Key);
					Groups[Pair].push_back({ Lerp(AX, BX, T), Lerp(AY, BY, T) });
				};
				for (int J = 0; J + 1 < CNY; ++J)
					for (int I = 0; I + 1 < CNX; ++I)
					{
						const size_t K = static_cast<size_t>(J) * CNX + I;
						const Spot* Corners[4] = { &CSpot[K], &CSpot[K + 1], &CSpot[K + CNX], &CSpot[K + CNX + 1] };
						bool bAny = false;
						for (int A = 0; A < 4 && !bAny; ++A)
							for (int Bc = A + 1; Bc < 4 && !bAny; ++Bc)
								if (Corners[A]->Key != Corners[Bc]->Key && Walled(*Corners[A], *Corners[Bc])) bAny = true;
						if (!bAny) continue;
						for (int FJ = 0; FJ <= Per; ++FJ)
							for (int FI = 0; FI <= Per; ++FI)
							{
								const int GI = I * Per + FI, GJ = J * Per + FJ;
								if (FI < Per) Crossing(GI, GJ, GI + 1, GJ);
								if (FJ < Per) Crossing(GI, GJ, GI, GJ + 1);
							}
					}
				// each edge's points chained into a line (nearest next, no farther than a fine step and a half), eased, laid
				for (auto& [Pair, Pts] : Groups)
				{
					if (Pts.size() < 4) continue;
					std::vector<uint8_t> Taken(Pts.size(), 0);
					size_t Left = Pts.size();
					while (Left > 0)
					{
						size_t Start = 0;
						while (Taken[Start]) ++Start;
						std::vector<std::pair<double, double>> Line{ Pts[Start] };
						Taken[Start] = 1;
						--Left;
						for (int Dir = 0; Dir < 2; ++Dir)
							for (;;)
							{
								const std::pair<double, double> End = Dir == 0 ? Line.back() : Line.front();
								double Best = 6.2;
								size_t Pick = Pts.size();
								for (size_t Q = 0; Q < Pts.size(); ++Q)
								{
									if (Taken[Q]) continue;
									const double D = std::hypot(Pts[Q].first - End.first, Pts[Q].second - End.second);
									if (D < Best)
									{
										Best = D;
										Pick = Q;
									}
								}
								if (Pick == Pts.size()) break;
								Taken[Pick] = 1;
								--Left;
								if (Dir == 0) Line.push_back(Pts[Pick]);
								else Line.insert(Line.begin(), Pts[Pick]);
							}
						if (Line.size() < 4) continue;
						std::vector<std::pair<double, double>> Eased(Line.size());
						for (size_t K = 0; K < Line.size(); ++K)
						{
							const size_t A = K > 0 ? K - 1 : K, Bn = K + 1 < Line.size() ? K + 1 : K;
							Eased[K] = { (Line[A].first + 2.0 * Line[K].first + Line[Bn].first) / 4.0, (Line[A].second + 2.0 * Line[K].second + Line[Bn].second) / 4.0 };
						}
						const double High = R.Range(0.85, 1.25);
						Run(H, R, Eased, High, 0.55, 0.3, false);
					}
				}
			}

			/** The terraces' walls: along the foot of every riser the band's slopes are stepped in (Relief), traced on the
			 * height's contours (marching squares on the valley side's own rise). */
			void Terraces(Host& H, Rng& R)
			{
				const double B = H.Band(), Step_ = 3.0;
				const double X0 = -B, Y0 = -B;
				const int NX = static_cast<int>(std::ceil((FW + 2.0 * B) / Step_)) + 1, NY = static_cast<int>(std::ceil((FH + 2.0 * B) / Step_)) + 1;
				std::vector<float> V(static_cast<size_t>(NX) * NY, -1.0f);
				for (int J = 0; J < NY; ++J)
					for (int I = 0; I < NX; ++I)
					{
						const double X = X0 + I * Step_, Y = Y0 + J * Step_, D = Out(X, Y);
						if (D < 30.0) continue;
						V[static_cast<size_t>(J) * NX + I] = static_cast<float>(Lift(X, Y, D, SideAt(X, Y)));
					}
				const double L0 = Rise * (1.0 - RiserRun);
				auto LevelBelow = [L0](double E) { return static_cast<int>(std::floor((E - L0) / Rise)); };
				// edge ids: the bottom edge of square (I, J) is 2 (J NX + I), its left edge 2 (J NX + I) + 1; a crossing by
				// its edge and its level
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
				// chained into lines
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
							const int N = Next(End, Cur);
							if (N < 0) break;
							Used[static_cast<size_t>(N)] = 1;
							Cur = static_cast<uint32_t>(N);
							const uint64_t Other = Segs[Cur].first == End ? Segs[Cur].second : Segs[Cur].first;
							if (Dir == 0) Keys.push_back(Other);
							else Keys.insert(Keys.begin(), Other);
						}
					}
					if (Keys.size() < 4) continue;
					// the wall's line, where the terraces are laid; its height the riser's
					std::vector<std::pair<double, double>> Line;
					double WSum = 0.0;
					int WN = 0;
					auto Lay = [&]() {
						if (Line.size() >= 4 && WN > 0)
						{
							Run(H, R, Line, 0.3 + (WSum / WN) * Rise * (1.0 - TreadFall), 0.6, 0.18, true);
							// olives along the tread below the wall, on most terraces
							const double Planted = R.Next();
							if (Planted < 0.7)
							{
								double Since = R.Range(0.0, 8.0);
								for (size_t K = 1; K < Line.size(); ++K)
								{
									const double DX = Line[K].first - Line[K - 1].first, DY = Line[K].second - Line[K - 1].second;
									Since += std::hypot(DX, DY);
									if (Since < 9.5) continue;
									Since = R.Range(-1.5, 1.5);
									const double PX = Line[K].first, PY = Line[K].second;
									const double Side = SideAt(PX, PY);
									const double GX = Lift(PX + 2.0, PY, Out(PX + 2.0, PY), Side) - Lift(PX - 2.0, PY, Out(PX - 2.0, PY), Side);
									const double GY = Lift(PX, PY + 2.0, Out(PX, PY + 2.0), Side) - Lift(PX, PY - 2.0, Out(PX, PY - 2.0), Side);
									const double GL = std::hypot(GX, GY);
									if (GL < 1e-6) continue;
									const double Down = R.Range(4.5, 8.0);
									TerraceTrees.push_back({ PX - GX / GL * Down, PY - GY / GL * Down });
								}
							}
						}
						Line.clear();
						WSum = 0.0;
						WN = 0;
					};
					for (const uint64_t K : Keys)
					{
						const std::pair<double, double> P = Where[K];
						const double D = Out(P.first, P.second);
						const double Wt = Terraced(P.first, P.second, D, SideAt(P.first, P.second));
						if (Wt < 0.45 || D > H.Band() - 4.0)
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

			// ------------------------------------------------------------------ plants

			void Plants(Host& H) override
			{
				Rng R(H.Seed() ^ 0x9A47);
				Groves(H, R);
				const double B = H.Band(), W = H.Width() + 2.0 * B, D = H.Depth() + 2.0 * B;
				// the woods (the south's old olive groves; the mainland's holm oak, its dark evergreen crowns), the maquis on
				// the rough, macchia in clumps across the fallow, a lone olive or two in the grazing, the field corners' cairns
				// of cleared stones, the sangars on the works
				const int Tries = static_cast<int>(W * D / 60.0);
				for (int K = 0; K < Tries; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.75, 1.3), Shade = R.Range(0.9, 1.1), Tall = R.Next();
					if (!H.Free(X, Y, 1.0)) continue;
					const Spot S = H.At(X, Y);
					if (S.Drawn == 9 || S.Drawn == 11 || S.Drawn == 7 || S.Kind == KWater || S.Kind == KLane) continue;
					const int C = S.Crop >= 0 ? S.Crop : S.Kind == KRough ? Maquis : Fallow;
					if (S.Kind == KWorks)
					{
						// (sangars, stone breastworks, some three a hectare)
						if (Keep < 0.018 && H.Free(X, Y, 3.0)) H.Plant(KitOf(KSangar), X, Y, Yaw, 0.85 + 0.25 * Size, 1.0, Tint(1.0, 1.0, 1.0));
						continue;
					}
					if (S.Kind == KVillage)
					{
						// (the gardens' fruit trees, dark citrus and an olive or two)
						if (Keep < 0.075) H.Plant(KitOf(KOlive), X, Y, Yaw, Size * 0.8, Size * 0.75, Tint(0.7, 0.86, 0.66));
						continue;
					}
					if (S.Kind == KWoods && bSouth)
					{
						// (the south's woods: old olive groves on a loose lattice some 9 m, a tree missing here and there,
						// macchia in the gaps)
						const double Lat = 9.0, CA = std::cos(0.3), SA = std::sin(0.3);
						const double U = X * CA + Y * SA, V = -X * SA + Y * CA;
						const int64_t CI = static_cast<int64_t>(std::floor(U / Lat)), CJ = static_cast<int64_t>(std::floor(V / Lat));
						const uint64_t Cell = Hash(Seed ^ 0x0F1, static_cast<uint64_t>(CI), static_cast<uint64_t>(CJ));
						if (Keep < 0.88 && WoodCells.insert(Cell).second)
						{
							const double JU = 1.4 * (2.0 * Unit(Mix(Cell ^ 0x11)) - 1.0), JV = 1.4 * (2.0 * Unit(Mix(Cell ^ 0x12)) - 1.0);
							const double PU = (CI + 0.5) * Lat + JU, PV = (CJ + 0.5) * Lat + JV;
							const double PX = PU * CA - PV * SA, PY = PU * SA + PV * CA;
							const Spot Q = H.At(PX, PY);
							if (Q.Kind == KWoods && Q.Drawn != 7 && H.Free(PX, PY, 1.4))
							{
								const double Age = Unit(Mix(Cell ^ 0x13)), Silver = Unit(Mix(Cell ^ 0x14));
								const double Sz = 1.1 + 0.6 * Age;
								H.Plant(KitOf(KOlive), PX, PY, Yaw, Sz, Sz * (0.9 + 0.2 * Silver), Tint(0.92 + 0.1 * Silver, 0.95 + 0.08 * Silver, 0.92 + 0.12 * Silver));
							}
						}
						else if (Keep > 0.94) H.Plant(KitOf(KMacchia), X, Y, Yaw, Size, Size * Lerp(0.8, 1.4, Tall), Tint(Shade, Shade, Shade));
						continue;
					}
					if (S.Kind == KWoods)
					{
						if (Keep < 0.75)
						{
							// (holm oak, dark and evergreen; the deciduous oaks among them brown into the winter)
							const bool bHolm = R.Next() < 0.7;
							const double Sz = R.Range(0.48, 0.7);
							const LandField::Rgb8 Tn = bHolm ? Tint(0.44 * Shade, 0.52 * Shade, 0.42 * Shade) : Tint(Lerp(1.05, 0.62, Leaf) * Shade, Lerp(0.78, 0.7, Leaf) * Shade, Lerp(0.5, 0.5, Leaf) * Shade);
							H.Plant(LandField::Base::Oak, X, Y, Yaw, Sz, Sz * Lerp(0.85, 1.05, Tall), Tn);
						}
						else H.Plant(KitOf(KMacchia), X, Y, Yaw, Size * 1.4, Size * 1.6, Tint(Shade, Shade, Shade));
						continue;
					}
					// (Sicily's and the south's wheat and fallow with trees standing in it here and there, olives and
					// carobs, some fields none, some a few to the hectare)
					if (C == Grain || C == Fallow || C == Plough)
					{
						static const double PerHa[5] = { 0.0, 0.0, 2.0, 4.0, 7.0 };
						const double Ha = PerHa[static_cast<int>(Unit(Mix(S.Key ^ 0x5CA7)) * 4.999)];
						if (Keep < Ha * 60.0 / 10000.0 && S.Edge > 5.0)
						{
							const bool bCarob = R.Next() < 0.25;
							H.Plant(KitOf(KOlive), X, Y, Yaw, Size * (bCarob ? 1.5 : 1.35), Size * (bCarob ? 1.3 : 1.45), bCarob ? Tint(0.72, 0.84, 0.7) : Tint(1.04, 1.07, 1.05));
							continue;
						}
					}
					if (C == Maquis)
					{
						if (Keep < 0.63) H.Plant(KitOf(KMacchia), X, Y, Yaw, Size, Size * Lerp(0.7, 1.5, Tall), Tint(Shade, Shade, Shade));
						else if (Keep < 0.675) H.Plant(KitOf(KBoulder), X, Y, Yaw, Size, Size * 0.8, Tint(1.08, 1.06, 1.02));
						else if (Keep < 0.705) H.Plant(KitOf(KStones), X, Y, Yaw, Size, Size, Tint(1.1, 1.08, 1.04));
						continue;
					}
					if (C == Fallow)
					{
						// (macchia in clumps across it, some 14 a hectare, thicker along its edges; stones cleared into heaps)
						if (Keep < 0.084 || (S.Edge < 4.0 && Keep < 0.25)) H.Plant(KitOf(KMacchia), X, Y, Yaw, Size, Size * Lerp(0.7, 1.4, Tall), Tint(Shade, Shade, Shade));
						else if (Keep > 0.25 && Keep < 0.286) H.Plant(KitOf(KStones), X, Y, Yaw, Size * 1.1, Size, Tint(1.12, 1.1, 1.05));
						continue;
					}
					if ((C == Grain || C == Plough) && S.Edge < 3.0 && Keep < 0.015) H.Plant(KitOf(KStones), X, Y, Yaw, Size * 1.2, Size, Tint(1.12, 1.1, 1.05));
				}
				// the olives on the terraces' treads (Walls found them)
				for (const auto& [X, Y] : TerraceTrees)
				{
					const double Yaw = R.Range(0.0, 2.0 * Pi), Age = R.Next(), Silver = R.Next(), Tall = R.Range(0.9, 1.1);
					if (!H.Free(X, Y, 1.4)) continue;
					const Spot S = H.At(X, Y);
					if (S.Drawn == 7 || S.Drawn == 10 || S.Kind == KWoods || S.Kind == KWater || S.Kind == KVillage) continue;
					const double Sz = 1.05 + 0.6 * Age;
					H.Plant(KitOf(KOlive), X, Y, Yaw, Sz, Sz * Tall, Tint(0.92 + 0.1 * Silver, 0.95 + 0.08 * Silver, 0.92 + 0.12 * Silver));
				}
				Lanes(H, R);
				HillTown(H, R);
			}

			/** The olive groves: in each, the trees on its own loose rows some ten metres apart (fewer than 150 a hectare),
			 * the rows wavering, a tree missing here and there, old and young. Each parcel found from a coarse scan, then
			 * walked slot by slot from where it was found. */
			void Groves(Host& H, Rng& R)
			{
				const double B = H.Band();
				std::unordered_set<uint64_t> Seen;
				for (double SY = -B + 10.0; SY < FH + B; SY += 20.0)
					for (double SX = -B + 10.0; SX < FW + B; SX += 20.0)
					{
						const Spot S = H.At(SX, SY);
						if (S.Crop != Olive || S.Key == 0 || S.Kind == KWoods || S.Kind == KVillage) continue;
						if (!Seen.insert(S.Key).second) continue;
						const uint64_t Key = S.Key;
						const bool bOld = S.Kind == KOpen;
						const double Ca = std::cos(S.Angle), Sa = std::sin(S.Angle);
						const double OX = S.X - (S.Along * Ca - S.Across * Sa), OY = S.Y - (S.Along * Sa + S.Across * Ca);
						const double GA = 8.5 + 2.5 * Unit(Mix(Key ^ 0x0A1)), GC = 9.5 + 2.0 * Unit(Mix(Key ^ 0x0A2));
						const double A0 = GA * Unit(Mix(Key ^ 0x0A3)), C0 = GC * Unit(Mix(Key ^ 0x0A4));
						auto Slot = [&](int I, int J, double& OutX, double& OutY) {
							const double Along = A0 + I * GA, Across = C0 + J * GC + 1.4 * Noise(Along / 45.0, J * 1.7, Key);
							OutX = OX + Along * Ca - Across * Sa;
							OutY = OY + Along * Sa + Across * Ca;
						};
						std::unordered_set<int64_t> Done;
						std::vector<std::pair<int, int>> Todo{ { static_cast<int>(std::lround((S.Along - A0) / GA)), static_cast<int>(std::lround((S.Across - C0) / GC)) } };
						Done.insert(static_cast<int64_t>(Todo[0].first) * 100003 + Todo[0].second);
						while (!Todo.empty())
						{
							const auto [I, J] = Todo.back();
							Todo.pop_back();
							double PX = 0.0, PY = 0.0;
							Slot(I, J, PX, PY);
							const Spot Q = H.At(PX, PY);
							if (Q.Key != Key) continue;
							for (const auto& [DI, DJ] : { std::pair<int, int>{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } })
								if (Done.insert(static_cast<int64_t>(I + DI) * 100003 + (J + DJ)).second) Todo.push_back({ I + DI, J + DJ });
							const double Miss = R.Next(), JX = R.Range(-1.4, 1.4), JY = R.Range(-1.4, 1.4), Yaw = R.Range(0.0, 2.0 * Pi);
							const double Age = R.Next(), Silver = R.Range(0.0, 1.0), Tall = R.Range(0.9, 1.12);
							if (Miss < (bOld ? 0.3 : 0.12) || Q.Edge < 2.5) continue;
							const double X = PX + JX, Y = PY + JY;
							if (!H.Free(X, Y, 1.4)) continue;
							// (centuries old most of them, a few young ones replanted; the crowns speckled toward silver)
							const double Sz = Age < 0.15 ? 0.85 + 0.3 * Age / 0.15 : 1.2 + 0.55 * (Age - 0.15) / 0.85;
							H.Plant(KitOf(KOlive), X, Y, Yaw, Sz, Sz * Tall, Tint(0.92 + 0.1 * Silver, 0.95 + 0.08 * Silver, 0.92 + 0.12 * Silver));
						}
					}
			}

			/** Along the roads: rows of cypress here and there, the telegraph line beside the main road. */
			void Lanes(Host& H, Rng& R)
			{
				size_t Main = 0;
				double Longest = 0.0;
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const std::vector<std::pair<double, double>>& Line = H.LaneLine(L);
					double Len = 0.0;
					for (size_t K = 1; K < Line.size(); ++K) Len += std::hypot(Line[K].first - Line[K - 1].first, Line[K].second - Line[K - 1].second);
					if (Len > Longest)
					{
						Longest = Len;
						Main = L;
					}
					// a row of cypresses on one side, now and then (a villa's frontage, a chapel, a crossing)
					const double Half = H.LaneHalf(L);
					double S = 0.0, NextRow = R.Range(150.0, 400.0), RowEnd = -1.0;
					int Side = 1;
					for (size_t K = 1; K < Line.size(); ++K)
					{
						const double DX = Line[K].first - Line[K - 1].first, DY = Line[K].second - Line[K - 1].second, DL = std::hypot(DX, DY);
						if (DL < 1e-6) continue;
						S += DL;
						if (S > NextRow && RowEnd < 0.0)
						{
							RowEnd = S + R.Range(30.0, 90.0);
							const double Which_ = R.Next();
							Side = Which_ < 0.5 ? -1 : 1;
						}
						if (RowEnd > 0.0)
						{
							if (std::fmod(S, 7.0) < DL)
							{
								const double Off = Half + 3.4;
								const double X = Line[K].first - DY / DL * Side * Off, Y = Line[K].second + DX / DL * Side * Off;
								const double Yaw = R.Range(0.0, 2.0 * Pi), Wide = R.Range(0.9, 1.1), Tall = R.Range(1.1, 1.5);
								if (H.Free(X, Y, 1.2) && H.At(X, Y).Drawn != 7) H.Plant(KitOf(KCypress), X, Y, Yaw, Wide, Tall, Tint(1.0, 1.0, 1.0));
							}
							if (S > RowEnd)
							{
								RowEnd = -1.0;
								NextRow = S + R.Range(250.0, 600.0);
							}
						}
					}
				}
				if (H.Lanes() == 0) return;
				// the poles, 50 m apart
				const std::vector<std::pair<double, double>>& Line = H.LaneLine(Main);
				const double Off = H.LaneHalf(Main) + 3.2;
				double S = 0.0;
				for (size_t K = 1; K < Line.size(); ++K)
				{
					const double DX = Line[K].first - Line[K - 1].first, DY = Line[K].second - Line[K - 1].second, DL = std::hypot(DX, DY);
					if (DL < 1e-6) continue;
					S += DL;
					if (std::fmod(S, 50.0) >= DL) continue;
					const double X = Line[K].first + DY / DL * Off, Y = Line[K].second - DX / DL * Off;
					const double Tall = R.Range(0.97, 1.03);
					if (H.Free(X, Y, 0.6)) H.Plant(KitOf(KPole), X, Y, std::atan2(DY, DX) + 0.5 * Pi, 1.0, Tall, Tint(1.0, 1.0, 1.0));
				}
			}

			/** The hill town on the crest beyond the field: its houses packed on the top, its tower over them, cypresses
			 * on its slopes (seen kilometres off, under the haze). */
			void HillTown(Host& H, Rng& R)
			{
				std::vector<std::pair<double, double>> Laid;
				const double Reach = 150.0;
				for (int K = 0; K < 1400 && Laid.size() < 110; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = Reach * std::sqrt(R.Next());
					const double X = HillX + D * std::cos(A), Y = HillY + 10.0 + D * std::sin(A);
					const double Pick = R.Next(), Skew = R.Range(-0.15, 0.15), Turn_ = R.Next();
					const double Yaw = Skew + (Turn_ < 0.5 ? 0.0 : 0.5 * Pi) + 0.3;
					const double Room = Pick < 0.6 ? 9.5 : 8.0;
					// (on the crest's flat top, its front houses at the brow)
					bool bClear = std::hypot(X - HillX, Y - HillY + 20.0) > 14.0 && CrestC(X, Y) < 150.0;
					for (const auto& [QX, QY] : Laid)
						if (std::hypot(X - QX, Y - QY) < Room + 1.5) bClear = false;
					if (!bClear) continue;
					Laid.push_back({ X, Y });
					// (three and four storeys packed on the crest)
					const double Sc = R.Range(0.85, 1.0), Tall = R.Range(1.3, 1.9);
					if (Pick < 0.6) H.Plant(KitOf(KFarm), X, Y, Yaw, Sc, Tall, Tint(1.0, 1.0, 1.0));
					else H.Plant(KitOf(KHouseStone), X, Y, Yaw, Sc * 0.9, Tall * 1.2, Tint(1.0, 1.0, 1.0));
				}
				H.Plant(KitOf(KTower), HillX, HillY - 20.0, 0.3, 1.4, 1.6, Tint(1.0, 1.0, 1.0));
				for (int K = 0; K < 24; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(Reach + 10.0, Reach + 90.0);
					const double Yaw = R.Range(0.0, 2.0 * Pi), Tall = R.Range(1.1, 1.5);
					const double X = HillX + D * std::cos(A), Y = HillY + 10.0 + D * std::sin(A);
					if (CrestC(X, Y) < 300.0) H.Plant(KitOf(KCypress), X, Y, Yaw, 1.0, Tall, Tint(1.0, 1.0, 1.0));
				}
			}

			// ------------------------------------------------------------------ the effects

			int Fx(const std::string& Ground, int Else) const override
			{
				(void)Ground;
				return Wet > 0.6 && Else == 0 ? 4 : Else; // (the winter's earth is mud)
			}
			const char* Earth() const override { return "terrarossa"; }
			Tone WallStone() const override { return Rgb(176, 168, 148); }
			double WallIvy() const override { return 0.08; }

		private:
			static constexpr double Margin = 300.0, FarmCell = 100.0;
			double FW = 0.0, FH = 0.0;
			double Dry = 1.0, Wet = 0.0, Green = 0.0, Leaf = 1.0, Turn = 0.0;
			int Stage = 2;
			bool bSouth = false, bSnowCaps = false;
			double HillX = 0.0, HillY = 0.0, HillTop = 0.0;
			std::vector<Exit> Exits;
			std::vector<Farm> Farms;
			std::vector<std::vector<uint32_t>> FarmGrid;
			int FarmNX = 0, FarmNY = 0;
			bool bTower = false, bCemetery = false;
			std::vector<float> LiftGrid;
			double GridX0 = 0.0, GridY0 = 0.0, GridStep = 50.0;
			int GridNX = 0, GridNY = 0;
			std::vector<std::pair<double, double>> TerraceTrees;
			double TowerX = 0.0, TowerY = 0.0, CemX = 0.0, CemY = 0.0, CemYaw = 0.0;
		};
	}

	std::unique_ptr<Region> MakeMed(const LandField::Field& F, uint64_t S) { return std::make_unique<Med>(F, S); }
}
