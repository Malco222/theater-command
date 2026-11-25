#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <unordered_set>
#include <vector>

/**
 * The open farmland of central and western Europe and of the east west of the steppe: France's and Belgium's champaign
 * and the Ardennes' plateau (Sedan 1940, the Bulge 1944), the Rhineland and Germany, Poland and western Russia. Big open
 * fields with no bocage (the banked hedgerows are Normandy's own), each its crop of the month: ploughed land, the
 * autumn-sown wheat a green haze over its furrows in winter and blue-green in May, the spring corn a thin green over the
 * brown in May and ripe in July, beet and potatoes in rows, grazing and hay meadows (buttercups in May); the west's
 * pastures wired or hedged, its villages' orchards; in the east the peasants' strip fields, a field cut into long strips
 * 12 to 30 m wide, each its own crop. Each field's own soft breakup of lighter and darker, warmer and cooler (15 to 80 m,
 * read off a coarse grid), so it is not flat paint where the clutter has gone. Woods in stands with soft, ragged edges:
 * the Ardennes' dark spruce in most, bare beech and oak between; birch, spruce and oak in the east. Villages along the
 * roads: in the west the ground's own stone houses under slate (the Ardennes, Belgium) or tile (Lorraine, Germany), the
 * Ardennes' long-houses of house and barn under one roof, farmsteads round their yards out along the lanes; in the east
 * the kit's log izbas and whitewashed khatas behind their wattle fences, a well sweep, a haystack, some burnt to their
 * chimneys. Trees in lines along the roads (tall narrow elms for the poplars of the west, birches in the east),
 * telegraph poles along the main road.
 * Kit from the east thread's battle models (battle-models/east: Spruce, SpruceSnow, Birch, BirchBare, Izba, IzbaThatch,
 * Khata, BurntChimney, WattleFence, WellSweep, Haystack, TelegraphPole).
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Plough = 0,  // ploughed land: the furrows turned and left for the frost
			Winter = 1,  // autumn-sown wheat: a green haze over the furrows in winter, green in spring, ripe in July
			Corn = 2,    // spring corn (barley, oats): ploughed in winter, green in May and June, ripe in July, stubble after
			Roots = 3,   // beet and potatoes in rows; lifted and bare from October
			Pasture = 4, // grazing
			Meadow = 5,  // a hay meadow
			Fallow = 6,  // fallow and rough grass, weeds standing
			Rye = 7,     // the east's rye: as the wheat, taller and greyer
			Strips = 8   // the east's strip fields: a parcel cut into long strips, each its own crop
		};

		enum Slot : int
		{
			KSpruce = 0,
			KBirch = 1,
			KIzba = 2,
			KIzbaThatch = 3,
			KKhata = 4,
			KChimney = 5,
			KWattle = 6,
			KSweep = 7,
			KHaystack = 8,
			KPole = 9,
			// (no model of its own: drawn by KitPlain below)
			KPost = 10,   // a pasture fence's post, wired to the next 3.6 m on
			KMarker = 11, // a road's snow pole, a straw wisp at its top
			KDrift = 12   // a snow fence's slatted panel, 4 m
		};
		constexpr double PostStep = 3.6;

		// The clutter's own (Clut0..Clut4), their heights as built in metres (LandSnow's table keeps the same).
		constexpr LandField::Base CGrass = LandField::Base::Clut0;   // a grass tuft, 0.38 m
		constexpr LandField::Base CStubble = LandField::Base::Clut1; // a cut field's stubble in its drill row, 0.14 m
		constexpr LandField::Base CStalks = LandField::Base::Clut2;  // dry weed stalks: thistle, mugwort, umbellifer, 0.85 m
		constexpr LandField::Base CClods = LandField::Base::Clut3;   // the plough's clods, 0.12 m
		constexpr LandField::Base CShoots = LandField::Base::Clut4;  // the young corn in its drill row, 0.12 m

		/** A clod of turned earth: a low lump RX long (along x, the furrow) and RY across, its top Height over the ground and
		 * its foot sunk a third of that into it, its points pushed by a noise of its seed, few of them (it is small). Its
		 * normals turned up toward the sky, so its sides take the light as the soil round it does and it reads as earth
		 * lifted, not as a dark stone set on the field (the art director, Voronezh, 24 Nov). */
		void Clod(Writer& W, double CX, double CY, double RX, double RY, double Height, uint64_t S, const Tone& Lit, const Tone& Under)
		{
			constexpr int Rings = 3, Round = 7;
			std::vector<uint32_t> Id;
			const uint32_t Top = W.Vert(CX, CY, Height, 0.0, 0.0, 1.0, Lit);
			for (int R = 1; R <= Rings; ++R)
			{
				const double Phi = 0.5 * Pi * R / Rings; // 0 at the top to pi/2 at the foot
				for (int K = 0; K < Round; ++K)
				{
					const double Th = 2.0 * Pi * (K + 0.5 * (R & 1)) / Round;
					const double Push = 0.8 + 0.4 * Unit(Hash(S, static_cast<uint64_t>(R), static_cast<uint64_t>(K)));
					const double X = std::sin(Phi) * std::cos(Th), Y = std::sin(Phi) * std::sin(Th), Z = std::cos(Phi);
					const double NX = 0.5 * X, NY = 0.5 * Y, NZ = Z + 0.45, NL = std::sqrt(NX * NX + NY * NY + NZ * NZ);
					Id.push_back(W.Vert(CX + RX * Push * X, CY + RY * Push * Y, Height * (1.35 * Z * Push - 0.35), NX / NL, NY / NL, NZ / NL, Mixed(Under, Lit, Z)));
				}
			}
			for (int K = 0; K < Round; ++K) W.Tri(Top, Id[static_cast<size_t>(K)], Id[static_cast<size_t>((K + 1) % Round)]);
			for (int R = 0; R + 1 < Rings; ++R)
				for (int K = 0; K < Round; ++K)
				{
					const uint32_t A = Id[static_cast<size_t>(R * Round + K)], B = Id[static_cast<size_t>(R * Round + (K + 1) % Round)];
					const uint32_t C = Id[static_cast<size_t>((R + 1) * Round + (K + 1) % Round)], D = Id[static_cast<size_t>((R + 1) * Round + K)];
					W.Tri(A, D, C);
					W.Tri(A, C, B);
				}
		}

		/** A house's stove chimney: its brick dulled (it drew bright orange in the low winter sun: the art director, 20:08Z)
		 * and sooted at its top, its own shading kept (every level of the kit model). */
		void DullChimney(LandField::BaseMesh& M)
		{
			const Tone Brick = Rgb(0x8E, 0x4E, 0x3C), Dull = Rgb(116, 78, 66), Soot = Rgb(46, 41, 38);
			for (LandField::Mesh* Each : { &M.Near, &M.Mid, &M.Far })
			{
				std::vector<size_t> Bricks;
				float Top = -1e9f;
				for (size_t K = 0; K < Each->Colours.size(); ++K)
				{
					const LandField::Rgb8& C = Each->Colours[K];
					if (std::abs(C.R - static_cast<int>(Brick.R)) > 6 || std::abs(C.G - static_cast<int>(Brick.G)) > 6 || std::abs(C.B - static_cast<int>(Brick.B)) > 6) continue;
					Bricks.push_back(K);
					Top = std::max(Top, Each->Positions[3 * K + 2]);
				}
				for (const size_t K : Bricks)
				{
					LandField::Rgb8& C = Each->Colours[K];
					const Tone To = Mixed(Dull, Soot, Smooth(Top - 0.5, Top - 0.05, Each->Positions[3 * K + 2]));
					C = Byte(Tone{ To.R * C.R / Brick.R, To.G * C.G / Brick.G, To.B * C.B / Brick.B });
				}
			}
		}

		class Farm final : public Region
		{
		public:
			Farm(const LandField::Field& F, uint64_t S) : Region(F, S)
			{
				Month = std::clamp(F.Month, 1, 12);
				bEast = F.Lon >= 15.5;
				bRussia = F.Lon >= 23.5;
				bArdennes = F.Lat >= 49.85 && F.Lat <= 51.0 && F.Lon >= 4.6 && F.Lon <= 7.6; // (Sedan, at 49.7, is the Meuse's farmland)
				bSlate = (F.Lat >= 49.3 && F.Lon < 7.6) || F.Terrain == "hills" || F.Terrain == "mountain";
				SnowLying = SnowOf(F, Look::Farmland);
				// (the trial "sprucedepth" takes in "spruce": its far crowns as that word's, the wood itself deep and ragged)
				bDepthTrial = F.Tried("sprucedepth");
				bSpruceTrial = F.Tried("spruce") || bDepthTrial;
				bGrassTrial = F.Tried("farmgrass");
				bGold = EastGold(Look::Farmland, SnowLying, F.Lat, F.Lon, F.Month, F.Trial);
				bBare = Month >= 11 || Month <= 3;
				// the share of the woods' blocks that are conifer: the Ardennes' spruce plantations most
				Conifers = bArdennes ? 0.72 : bEast ? 0.45 : 0.28;
				// (the compartments' grid, turned its own way)
				const double A = 0.35 + 0.5 * Unit(Hash(Seed, 0xC0F));
				Ca = std::cos(A);
				Sa = std::sin(A);
				// the soil: the west's brown loam, Poland's paler sandy loam, Russia's darker
				Soil = bRussia ? Rgb(98, 82, 66) : bEast ? Rgb(128, 106, 82) : Rgb(118, 94, 70);
				if (bArdennes) Soil = Rgb(110, 92, 76); // (the shale's grey-brown)
				// the grass by the season: winter's dull olive, spring's and summer's green, late summer drier
				switch (Month)
				{
				case 12: case 1: case 2: GrassT = Rgb(116, 116, 76); break;
				case 3: GrassT = Rgb(104, 114, 64); break;
				case 4: case 5: case 6: GrassT = bEast ? Rgb(88, 114, 52) : Rgb(84, 108, 54); break;
				case 7: case 8: GrassT = Rgb(110, 118, 62); break;
				default: GrassT = Rgb(96, 112, 58); break;
				}
				// The ground's own breakup read once onto a 10 m grid over the field and 600 m round it (the paint and
				// the track marks ask it at every step); none under a full cover of snow, which has drifts of its own.
				if (SnowLying > 0.6) return;
				GX0 = -600.0;
				GY0 = -600.0;
				GNX = static_cast<int>((F.Width() + 1200.0) / GStep) + 2;
				GNY = static_cast<int>((F.Depth() + 1200.0) / GStep) + 2;
				Grid.resize(2 * static_cast<size_t>(GNX) * static_cast<size_t>(GNY));
				for (int J = 0; J < GNY; ++J)
					for (int I = 0; I < GNX; ++I)
					{
						double Lt = 0.0, Wm = 0.0;
						Breakup(GX0 + I * GStep, GY0 + J * GStep, Lt, Wm);
						const size_t K = 2 * (static_cast<size_t>(J) * static_cast<size_t>(GNX) + static_cast<size_t>(I));
						Grid[K] = static_cast<float>(Lt);
						Grid[K + 1] = static_cast<float>(Wm);
					}
			}
			Look Which() const override { return Look::Farmland; }

			// ------------------------------------------------------------------ the land

			// (no relief of its own: the frame's undulation of 0.2 to 0.6 m is the field's, and every Land call pays for one)
			double Rolling(double X, double Y, double MeanZ) const override
			{
				// (the Ardennes' plateau cut by deep valleys: hills on every horizon; the champaign's long low swells; the
				// east's plain)
				if (bArdennes) return MeanZ + 46.0 * Fbm(X, Y, 3000.0, 3, Seed ^ 0xB1) + 14.0 * Fbm(X, Y, 800.0, 2, Seed ^ 0xB2);
				if (bEast) return MeanZ + 8.0 * Fbm(X, Y, 3400.0, 3, Seed ^ 0xB1) + 2.5 * Fbm(X, Y, 900.0, 2, Seed ^ 0xB2);
				return MeanZ + 16.0 * Fbm(X, Y, 2800.0, 3, Seed ^ 0xB1) + 4.0 * Fbm(X, Y, 750.0, 2, Seed ^ 0xB2);
			}
			double LaneDepth(double /*X*/, double /*Y*/) const override { return 0.1; }
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 3.3 : 2.5) + 0.7 * U; }

			// ------------------------------------------------------------------ the parcels

			double PlotMin() const override { return bArdennes ? 36000.0 : 60000.0; }
			double PlotMax() const override { return bArdennes ? 80000.0 : 140000.0; }
			double FarTilled() const override { return bArdennes ? 0.4 : 0.66; }
			double WoodsEdge() const override { return bArdennes ? 0.08 : bEast ? 0.18 : 0.24; }

			int CropOf(int Kind, double U) const override
			{
				// (the rules' open land is the champaign's fields as much as its grazing: most of it tilled; the Ardennes'
				// more grazing, the east's strips)
				if (Kind == KRough) return Fallow;
				if (Kind == KOpen)
				{
					const double Grazing = bArdennes ? 0.55 : bEast ? 0.32 : 0.3;
					if (U < Grazing) return U < 0.7 * Grazing ? Pasture : Meadow;
					U = (U - Grazing) / (1.0 - Grazing);
					if (bEast) return U < 0.8 ? Strips : Fallow;
				}
				if (bEast) return U < 0.72 ? Strips : U < 0.84 ? Rye : U < 0.92 ? Roots : Plough;
				return U < 0.34 ? Winter : U < 0.58 ? Corn : U < 0.76 ? Roots : U < 0.92 ? Plough : Fallow;
			}
			const char* CropWord(int C) const override
			{
				switch (C)
				{
				case Plough: return "plough";
				case Winter: return "winter wheat";
				case Corn: return "spring corn";
				case Roots: return "roots";
				case Pasture: return "pasture";
				case Meadow: return "meadow";
				case Fallow: return "fallow";
				case Rye: return "rye";
				case Strips: return "strips";
				default: return "";
				}
			}
			bool IsCorn(int C) const override
			{
				if (C == Winter || C == Rye) return Month == 6 || Month == 7;
				if (C == Corn) return Month == 7;
				return false;
			}

			/** A strip field's strip here: its crop (a strip's own, by its number across the parcel). */
			int StripCrop(const Spot& S) const
			{
				const double W = 12.0 + 16.0 * Unit(Hash(S.Key, 0x57));
				const int64_t I = static_cast<int64_t>(std::floor(S.Across / W + 0.37));
				const double U = Unit(Hash(S.Key, static_cast<uint64_t>(I + 4096)));
				return U < 0.3 ? Winter : U < 0.45 ? Rye : U < 0.62 ? Corn : U < 0.76 ? Roots : U < 0.9 ? Plough : Meadow;
			}
			int CropAt(const Spot& S) const
			{
				const int C = S.Crop >= 0 ? S.Crop : S.Kind == KField ? Plough : Pasture;
				return C == Strips && S.bLaid ? StripCrop(S) : C == Strips ? Winter : C;
			}

			// ------------------------------------------------------------------ what stands on it

			/** Whether the woods here are conifer: stands of spruce and of broadleaf in organic shapes, some 100 to 400 m
			 * across (a slow noise and a quicker one, the place warped first, so no edge runs straight or square: the art
			 * director's read found the old grid of compartments and its rides reading as tiles on the map). */
			bool Conifer(double X, double Y) const
			{
				const double WX = X + 80.0 * Noise(X / 420.0, Y / 420.0, Seed ^ 0xC0A1), WY = Y + 80.0 * Noise(X / 420.0 + 7.3, Y / 420.0 - 3.1, Seed ^ 0xC0A2);
				const double N = 0.72 * Noise(WX / 300.0, WY / 300.0, Seed ^ 0xC0A3) + 0.28 * Noise(WX / 95.0, WY / 95.0, Seed ^ 0xC0A4);
				return N < (Conifers - 0.5) * 0.65;
			}

			// ------------------------------------------------------------------ colours

			/**
			 * The ground's own breakup, 15 to 80 m, so a field seen from the general's height (where the clutter has gone)
			 * is a textured sward or tilth and not flat paint: Lt lighter and darker (-1..1) at two sizes, Wm warmer and
			 * cooler (-1..1) broader; each bent by a broader noise and turned, so no lattice shows.
			 */
			void Breakup(double X, double Y, double& Lt, double& Wm) const
			{
				const double WX = 40.0 * Noise(X / 150.0, Y / 150.0, Seed ^ 0xB61), WY = 40.0 * Noise(X / 150.0 - 4.1, Y / 150.0 + 2.3, Seed ^ 0xB62);
				const double PX = X + WX, PY = Y + WY;
				const double U = 0.8 * PX - 0.6 * PY, V = 0.6 * PX + 0.8 * PY;
				Lt = 0.6 * Noise(U / 45.0, V / 45.0, Seed ^ 0xB63) + 0.4 * Noise(V / 19.0, -U / 19.0, Seed ^ 0xB64);
				Wm = Noise(PX / 80.0 + 0.5, PY / 80.0, Seed ^ 0xB65);
			}
			/** The breakup here, read between the grid's points (none far out in the ring, where the haze has it). */
			void ReadBreakup(double X, double Y, double& Lt, double& Wm) const
			{
				const double U = (X - GX0) / GStep, V = (Y - GY0) / GStep;
				if (!(U >= 0.0 && V >= 0.0 && U < GNX - 1.0 && V < GNY - 1.0))
				{
					Lt = 0.0;
					Wm = 0.0;
					return;
				}
				const int I = static_cast<int>(U), J = static_cast<int>(V);
				const double FU = U - I, FV = V - J;
				const size_t K = 2 * (static_cast<size_t>(J) * static_cast<size_t>(GNX) + static_cast<size_t>(I)), Row = 2 * static_cast<size_t>(GNX);
				auto Lerp2 = [&](size_t C) {
					const double A = Grid[K + C], B = Grid[K + 2 + C], Cc = Grid[K + Row + C], D = Grid[K + Row + 2 + C];
					return (A + (B - A) * FU) + ((Cc + (D - Cc) * FU) - (A + (B - A) * FU)) * FV;
				};
				Lt = Lerp2(0);
				Wm = Lerp2(1);
			}

			/** The grass: patchy at two sizes, and drier, yellower where the slow noise lifts (a sward is never one green). */
			Tone Grass(double N1, double N2) const
			{
				const Tone Dry = Mixed(GrassT, Rgb(150, 140, 92), 0.45);
				return Varied(Mixed(GrassT, Dry, 0.35 * Smooth(0.0, 0.8, N1 + 0.15 * N2)), 1.0 + 0.1 * N1 + 0.03 * N2, 0.03 * N1);
			}

			/** A field's crop as the month shows it; Bands the plough's lands across it (-1..1), Streak its drill rows. */
			Tone CropTone(int C, double N1, double N2, double Bands, double Streak) const
			{
				const Tone Earth = Varied(Soil, 1.0 + 0.07 * Bands + 0.05 * Streak + 0.04 * N2, 0.0);
				const Tone Young = Rgb(96, 122, 50), Green = Rgb(82, 112, 44);
				// (the east's summer on trial "eastgold": the ripe corn and the stubble a step darker for its own grade, as
				// the steppe's (LandSteppe: its wheat 170,148,82), so open ground lands at 120 to 160 lit and the far band
				// gold, not the cream near-blown that Normandy's grade made of them; the far ring's too, through Far)
				const Tone Ripe = bGold ? (C == Rye ? Rgb(162, 146, 98) : Rgb(176, 144, 64)) : C == Rye ? Rgb(178, 160, 104) : Rgb(194, 154, 66);
				const Tone Stubble = bGold ? Rgb(152, 134, 92) : Rgb(170, 150, 104), Leaves = Rgb(62, 92, 44);
				// (the west's wheat in May and June: the glaucous blue-green of the leaf before the ear turns)
				const Tone Blue = Rgb(82, 108, 74);
				auto Haze = [&](double G) { return Mixed(Earth, Varied(Young, 1.0 + 0.06 * Streak, 0.0), G); };
				switch (C)
				{
				case Plough: return Month >= 3 && Month <= 5 ? Varied(Earth, 1.06, 0.01) : Earth; // (harrowed paler in spring)
				case Winter:
				case Rye:
					switch (Month)
					{
					case 9: return Mixed(Earth, Stubble, 0.2);
					case 10: return Haze(0.12 + 0.05 * N1);
					case 11: case 12: case 1: case 2: return Haze(0.3 + 0.1 * N1);
					case 3: return Haze(0.48 + 0.1 * N1);
					case 4: return Haze(0.8 + 0.1 * N1);
					case 5:
						if (!bEast && C == Winter) return Varied(Mixed(Blue, Green, 0.2 + 0.25 * N1), 1.0 + 0.06 * Streak, 0.0);
						return Varied(Mixed(Young, Green, 0.5 + 0.3 * N1), 1.0 + 0.06 * Streak, 0.0);
					case 6:
						if (!bEast && C == Winter) return Varied(Mixed(Blue, Ripe, 0.2 + 0.15 * N1), 1.0 + 0.05 * Streak, 0.0);
						return Varied(Mixed(Green, Ripe, 0.25 + 0.15 * N1), 1.0 + 0.05 * Streak, 0.0);
					case 7: return Varied(Mixed(Ripe, Green, 0.1 * Smooth(0.2, 0.7, N2)), 1.0 + 0.06 * Streak, 0.0);
					default: return Mixed(Stubble, Earth, 0.25 + 0.2 * Smooth(-0.2, 0.5, N2 + 0.5 * Bands));
					}
				case Corn:
					switch (Month)
					{
					case 3: return Varied(Earth, 1.05, 0.0);
					case 4: return Haze(0.22 + 0.08 * N1);
					case 5: return Haze(bEast ? 0.68 + 0.1 * N1 : 0.48 + 0.12 * N1);
					case 6: return Varied(Mixed(Young, Green, 0.6), 1.0 + 0.06 * Streak, 0.0);
					case 7: return Varied(Mixed(Ripe, Green, 0.25 + 0.15 * N1), 1.0 + 0.06 * Streak, 0.0);
					case 8: case 9: case 10: return Mixed(Stubble, Earth, 0.2 + 0.25 * Smooth(-0.2, 0.5, N2 + 0.5 * Bands));
					default: return Earth;
					}
				case Roots:
					if (Month >= 6 && Month <= 9) return Mixed(Earth, Varied(Leaves, 1.0 + 0.1 * N1, 0.0), 0.55 + 0.3 * Smooth(-0.6, 0.6, Streak) + (Month >= 7 ? 0.1 : 0.0));
					if (Month == 5) return Haze(0.15);
					if (Month == 10) return Mixed(Earth, Leaves, 0.12 * (0.5 + 0.5 * N2)); // (lifted: the tops strewn)
					return Earth;
				case Meadow:
				{
					Tone G = Grass(N1, N2);
					if (Month == 5 || Month == 6)
					{
						// (the hay meadow uncut: taller and yellower than the grazing, buttercups in drifts)
						G = Mixed(Varied(G, 1.06, 0.03), Rgb(120, 128, 62), bEast ? 0.0 : 0.3);
						G = Mixed(G, Rgb(168, 160, 64), (bEast ? 0.15 * Smooth(0.1, 0.7, N2) : 0.24 * Smooth(0.0, 0.6, N1 + 0.25 * N2)));
					}
					else if (Month == 7) G = Mixed(G, Rgb(170, 160, 104), 0.5); // (the hay cut)
					else if (bBare) G = Mixed(G, Rgb(160, 148, 104), 0.3);
					return G;
				}
				case Fallow: return Mixed(Grass(N1, N2), Mixed(Rgb(158, 140, 98), Earth, 0.3), 0.35 + 0.25 * Smooth(-0.3, 0.5, N2) + (bBare ? 0.2 : 0.0));
				default:
					// (pasture: grazed short and darker, the rank tufts and the thistles in patches)
					if (!bEast && Month >= 4 && Month <= 6) return Mixed(Varied(GrassT, 0.86, 0.03), Grass(N1, N2), 0.3 + 0.3 * Smooth(0.0, 0.6, N1 + 0.3 * N2));
					return Mixed(Grass(N1, N2), Varied(GrassT, 0.88, 0.04), 0.35 * Smooth(0.1, 0.6, N2));
				}
			}

			Tone Colour(const Spot& S) const override
			{
				switch (S.Kind)
				{
				case KLane:
				{
					const bool bWet = bBare || Month == 10 || Month == 4;
					if (S.Lane < 0.0)
					{
						// The road's floor: the west's gravelled macadam, grey; the east's dirt road, black earth worn to
						// dust in summer, mud in the wet months; ruts either side of the middle.
						const double Off = std::fabs(S.Lane + S.LaneHalf);
						const double Rut = std::exp(-std::pow((Off - 0.85) / 0.3, 2.0));
						const Tone Road = bEast ? (bWet ? Rgb(100, 86, 70) : Rgb(152, 136, 108)) : (bWet ? Rgb(124, 118, 108) : Rgb(156, 148, 132));
						const Tone RutT = bEast ? Rgb(76, 64, 52) : Varied(Road, 0.78, 0.02);
						Tone C = Mixed(Road, RutT, 0.65 * Rut);
						if (bEast || S.LaneHalf < 2.7) C = Mixed(C, Grass(S.N1, S.N2), 0.4 * Smooth(0.35, 0.0, Off) * (0.5 + 0.5 * S.N1)); // (grass down a track's middle)
						return Varied(Mixed(C, Grass(S.N1, S.N2), 0.5 * Smooth(-0.5, 0.0, S.Lane)), 1.0 + 0.05 * S.N2, 0.01);
					}
					// its verge: grass, the road's dust thrown on it
					return Mixed(Varied(Mixed(Soil, GrassT, 0.4), 1.0 + 0.05 * S.N2, 0.0), Grass(S.N1, S.N2), Smooth(0.0, 1.6, S.Lane));
				}
				case KVillage:
				{
					// gardens: dug vegetable plots and grass, the earth round the houses trodden
					Tone C = Mixed(Grass(S.N1, S.N2), Varied(Soil, 0.95, 0.0), 0.3 + 0.3 * Smooth(-0.2, 0.5, S.N1) + (bBare ? 0.15 : 0.0));
					if (S.bNear) C = Mixed(C, Varied(Soil, 0.85, 0.0), 0.5);
					return Varied(C, 1.0 + 0.05 * S.N2 + 0.04 * S.Broad, 0.01);
				}
				case KYard: return Varied(Mixed(Varied(Soil, 0.82, 0.0), Rgb(128, 122, 112), 0.35 + 0.25 * S.N1), 1.0 + 0.06 * S.N2, 0.0);
				case KWoods:
				{
					// the floor: the spruce's needles dark; the beech's and the oak's leaves rust-brown
					const bool bCon = Conifer(S.X, S.Y);
					const Tone Floor = bCon ? Rgb(70, 60, 48) : bBare ? Rgb(128, 96, 66) : Rgb(96, 88, 56);
					return Varied(Mixed(Floor, GrassT, 0.25 * Smooth(5.0, 1.0, S.Edge)), 1.0 + 0.08 * S.N2 + 0.05 * S.N1, 0.02 * S.N1);
				}
				case KWater: return Rgb(62, 76, 78);
				case KMarsh: return Varied(Mixed(Rgb(112, 114, 76), Rgb(152, 140, 98), bBare ? 0.6 : 0.2), 1.0 + 0.06 * S.N1, 0.0);
				case KWorks: return Varied(Mixed(Varied(Soil, 0.85, 0.0), Grass(S.N1, S.N2), 0.25 * Smooth(0.2, -0.3, S.N1)), 1.0 + 0.07 * S.N2, 0.0);
				default: break;
				}
				const int C = CropAt(S);
				// the plough's lands across the field (some 9 to 14 m) and its drill rows along it, strong enough to read from
				// a turret
				// (a triangle wave, not a cosine, and the drill rows' streaks from the Spot's own mottle: the track marks ask this
				// colour at every step of every tank)
				const double Q = S.Across / (9.0 + 5.0 * Unit(S.Key));
				const double Lands = 4.0 * std::fabs(Q - std::floor(Q) - 0.5) - 1.0;
				Tone T = CropTone(C, S.N1, S.N2, Lands, 0.6 * S.N2);
				// the headland: the plough's turn, weedy grass round the field's edge; a baulk of grass between strips
				const bool bTilled = C != Pasture && C != Meadow && C != Fallow;
				if (bTilled) T = Mixed(T, Mixed(Grass(S.N1, S.N2), Soil, 0.3), 0.5 * Smooth(4.0, 1.0, S.Edge));
				// (the breakup: the sward's and the tilth's own lighter and darker drifts, warmer and cooler)
				double Lt = 0.0, Wm = 0.0;
				ReadBreakup(S.X, S.Y, Lt, Wm);
				return Varied(T, 1.0 + 0.03 * S.N2 + 0.05 * S.Broad + 0.085 * Lt, 0.02 * S.Broad + 0.04 * Wm);
			}

			Tone Far(const Spot& S, bool bWoods, double Gap) const override
			{
				if (bWoods)
				{
					// the woods far off: spruce near black-green; the broadleaf green in summer, a grey-brown haze of bare
					// twigs from November to April
					const Tone Broad = bBare ? Rgb(116, 102, 92) : Month == 4 ? Rgb(108, 112, 80) : Rgb(68, 94, 48);
					const Tone T = Conifer(S.X, S.Y) ? Rgb(44, 62, 46) : Broad;
					return Varied(T, 1.0 + 0.1 * S.N1, 0.02 * S.Broad);
				}
				const int C = CropAt(S);
				Tone T = CropTone(C, S.N1, 0.0, 0.0, 0.0);
				// (a grass baulk or a farm track between the fields: a faint line)
				T = Mixed(T, Mixed(GrassT, Soil, 0.4), 0.3 * Smooth(6.0, 1.0, Gap));
				double Lt = 0.0, Wm = 0.0;
				ReadBreakup(S.X, S.Y, Lt, Wm);
				return Varied(T, 1.0 + 0.04 * S.Broad + 0.07 * Lt, 0.02 * S.Broad + 0.035 * Wm);
			}

			// ------------------------------------------------------------------ the clutter

			Grown Grow(const Spot& S, double Pick) const override
			{
				Grown G;
				if (S.Kind == KWater || S.Drawn == 9 /*floor*/) return G;
				switch (S.Kind)
				{
				case KLane: return S.Lane > 0.4 ? (Pick < 0.85 ? Grown{ CGrass, 0.5 } : Grown{ CStalks, 0.12 }) : G;
				case KWoods: return Pick < 0.5 ? Grown{ CStalks, 0.05 } : Grown{ LandField::Base::Stone, 0.04 };
				case KVillage: return Pick < 0.8 ? Grown{ CGrass, 0.35 } : Grown{ CStalks, 0.1 };
				case KYard: return Pick < 0.5 ? Grown{ LandField::Base::Stone, 0.1 } : G;
				case KMarsh: return Pick < 0.5 ? Grown{ CStalks, 0.45 } : Grown{ CGrass, 0.5 };
				case KWorks: return Pick < 0.5 ? Grown{ CClods, 0.4 } : Grown{ CGrass, 0.15 };
				default: break;
				}
				// the headland's weeds standing round a field
				if (S.Edge < 3.0 && Pick < 0.3) return Grown{ CStalks, 0.25 };
				const int C = CropAt(S);
				switch (C)
				{
				case Plough: return Grown{ CClods, 0.55 };
				case Winter:
				case Rye:
				case Corn:
				{
					const bool bStanding = (C != Corn && (Month == 6 || Month == 7)) || (C == Corn && Month == 7);
					if (bStanding) return Grown{ LandField::Base::Wheat, 0.42 };
					if ((C == Corn && Month >= 8 && Month <= 10) || (C != Corn && (Month == 8 || Month == 9))) return Grown{ CStubble, 0.75 };
					const bool bShoots = C != Corn ? (Month >= 10 || Month <= 5) : (Month >= 4 && Month <= 6);
					if (bShoots) return Pick < 0.62 ? Grown{ CShoots, 0.7 } : Grown{ CClods, 0.3 };
					return Grown{ CClods, 0.5 };
				}
				case Roots:
					if (Month >= 6 && Month <= 9) return Grown{ CGrass, 0.55 }; // (the beet's leaves, dressed broad and dark)
					return Grown{ CClods, 0.45 };
				case Meadow:
					if (bGrassTrial && !bBare && SnowLying <= 0.15) return FieldGrass(S, Pick, C);
					if ((Month == 5 || Month == 6) && Pick < 0.12) return Grown{ LandField::Base::Flowers, 0.3 };
					return Pick < 0.94 ? Grown{ CGrass, 0.62 } : Grown{ CStalks, 0.2 };
				case Fallow:
					if (bGrassTrial && !bBare && SnowLying <= 0.15) return FieldGrass(S, Pick, C);
					return Pick < 0.6 ? Grown{ CGrass, 0.45 } : Grown{ CStalks, 0.35 };
				case Pasture:
					if (bGrassTrial && !bBare && SnowLying <= 0.15) return FieldGrass(S, Pick, C);
					return Pick < 0.95 ? Grown{ CGrass, 0.5 } : Grown{ CStalks, 0.25 };
				default: return Pick < 0.95 ? Grown{ CGrass, 0.5 } : Grown{ CStalks, 0.25 };
				}
			}

			/**
			 * The grassland in the growing months (on trial, "farmgrass"): the bocage's own short sward between clumps of
			 * tussocks, tall grass and flowers, so an eastern pasture is a pasture at a man's eye and not a bare floor with
			 * a few grass cards on it. The clumps from two noises at odd angles, some 4.6 m and 1.7 m across (no lattice);
			 * grazing short with a tussock here and there, the hay meadow standing tall and flowered, the fallow rank with
			 * weeds. Flowers only May to August. No new mesh: LandField's own dressing tints the sward with the ground.
			 */
			Grown FieldGrass(const Spot& S, double Pick, int C) const
			{
				using B = LandField::Base;
				const double Ax = 0.809 * S.X - 0.588 * S.Y, Ay = 0.588 * S.X + 0.809 * S.Y;
				const double Wide = Noise(Ax / 4.6, Ay / 4.6, Seed ^ 0xF6A5);
				const double Fine = Noise(S.X / 1.7, S.Y / 1.7, Seed ^ 0xF6A6);
				const double In = Smooth(-0.3, 0.3, Wide + 0.4 * Fine);
				double Sw = 0.58, Tu = 0.3, Ta = 0.05, Fl = 0.04, Total = 1.3; // (grazing: short, a tussock here and there)
				B Weed = B::Rosette;
				if (C == Meadow) { Sw = 0.22; Tu = 0.18; Ta = 0.45; Fl = 0.12; Total = 1.45; Weed = CStalks; }
				else if (C == Fallow) { Sw = 0.25; Tu = 0.3; Ta = 0.2; Fl = 0.07; Total = 1.2; Weed = CStalks; }
				const double Clumped = 0.15 + 1.7 * In, Between = 1.2 - 0.45 * In;
				if (Pick < Sw) return Grown{ B::Sward, Total * Between };
				if (Pick < Sw + Tu) return Grown{ B::Tuft, Total * Clumped };
				if (Pick < Sw + Tu + Ta) return Grown{ B::TallGrass, Total };
				if (Pick < Sw + Tu + Ta + Fl) return Grown{ Month >= 5 && Month <= 8 ? B::Flowers : B::Tuft, Total * Clumped };
				return Grown{ Weed, Total * (Weed == B::Rosette ? Clumped : 1.0) };
			}

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				const int C = S.Kind == KField || S.Kind == KOpen || S.Kind == KRough ? CropAt(S) : -1;
				// (a patch noise of its own: the clutter's Spot carries none)
				const double Patch = Noise(P.X / 9.0, P.Y / 9.0, Seed ^ 0xD5E5);
				if (P.Kind == CGrass)
				{
					P.Scale = static_cast<float>(0.6 + 0.8 * U4 * U4);
					P.ScaleZ = static_cast<float>(P.Scale * (0.6 + 0.6 * U5) * (C == Pasture ? 0.75 + 0.3 * Patch : 1.0));
					// green in the growing months; winter's dry straw and olive; the beet's broad dark leaves
					// (in the bare months built dry already: only varied)
					const double Dry = bBare ? 0.12 * (U7 - 0.5) : Month >= 7 && Month <= 9 ? 0.2 + 0.3 * U7 : 0.1 * U7;
					double R = Lerp(1.0, 1.55, Dry), G = Lerp(1.0, 1.25, Dry), B = Lerp(1.0, 1.25, Dry);
					if (C == Roots && Month >= 6 && Month <= 9)
					{
						P.Scale *= 1.5f;
						P.ScaleZ *= 0.75f;
						R = 0.72;
						G = 0.86;
						B = 0.78;
					}
					const double Shade = 0.92 + 0.14 * U6;
					P.Tint = Tint(R * Shade, G * Shade, B * Shade);
					return true;
				}
				if (P.Kind == CStubble)
				{
					// along the field's own drill rows, either way round; greyer as it weathers
					P.Yaw = static_cast<float>(S.Angle + (U6 < 0.5 ? 0.0 : Pi) + 0.06 * (U4 - 0.5));
					P.Scale = static_cast<float>(0.85 + 0.3 * U4);
					P.ScaleZ = static_cast<float>(0.8 + 0.4 * U5);
					const double Old = Month >= 10 || Month <= 3 ? 0.85 : 1.0;
					P.Tint = Tint(Old * (0.95 + 0.1 * U7), Old * (0.95 + 0.08 * U7), Old * (1.02 - 0.04 * U7));
					return true;
				}
				if (P.Kind == CStalks)
				{
					P.Scale = static_cast<float>(0.7 + 0.6 * U4);
					P.ScaleZ = static_cast<float>(0.55 + 0.6 * U5);
					// green weeds in the growing months, dry brown stalks the rest of the year
					const bool bGreen = Month >= 5 && Month <= 8;
					P.Tint = bGreen ? Tint(0.66, 0.98, 0.62) : Tint(0.95 + 0.2 * U7, 0.95 + 0.12 * U7, 0.92);
					return true;
				}
				if (P.Kind == CClods)
				{
					// the plough's clods, set along its furrows
					P.Yaw = static_cast<float>(S.Angle + (U6 < 0.5 ? 0.0 : Pi) + 0.2 * (U4 - 0.5));
					P.Scale = static_cast<float>(0.75 + 0.6 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					const double L = 0.9 + 0.2 * U7;
					P.Tint = Tint(L * Soil.R / 128.0, L * Soil.G / 112.0, L * Soil.B / 94.0);
					return true;
				}
				if (P.Kind == CShoots)
				{
					P.Yaw = static_cast<float>(S.Angle + (U6 < 0.5 ? 0.0 : Pi) + 0.05 * (U4 - 0.5));
					P.Scale = static_cast<float>(0.9 + 0.25 * U4);
					// (the autumn-sown a hand high through the winter, knee high in May; the spring corn growing)
					const double Tall = Month == 5 ? 3.2 : Month == 4 ? 1.9 : Month == 6 ? 4.5 : Month == 3 ? 1.3 : 1.0;
					P.ScaleZ = static_cast<float>(Tall * (0.8 + 0.4 * U5));
					const double Dull = bBare ? 0.85 : 1.0;
					P.Tint = Tint(Dull * (0.95 + 0.1 * U7), Dull * (1.0 + 0.06 * U7), Dull * 0.95);
					return true;
				}
				if (P.Kind == LandField::Base::Stone)
				{
					P.Scale = static_cast<float>(0.5 + 0.9 * U4 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.4 * U5);
					P.Tint = Tint(1.0, 0.98, 0.95);
					return true;
				}
				return false; // (the ripe corn and the meadow's flowers dressed as the bocage's)
			}

			// ------------------------------------------------------------------ the kit

			const char* Folder() const override { return "East"; }
			const char* KitName(int K) const override
			{
				switch (K)
				{
				case KSpruce: return SnowLying > 0.2 ? "SpruceSnow" : "Spruce";
				case KBirch: return bBare ? "BirchBare" : "Birch";
				case KPole: return "TelegraphPole";
				case KPost: return bEast ? nullptr : "FencePost";
				case KMarker: return SnowLying > 0.15 ? "MarkerPole" : nullptr;
				case KDrift: return SnowLying > 0.15 ? "DriftFence" : nullptr;
				default: break;
				}
				if (!bEast) return nullptr;
				switch (K)
				{
				case KIzba: return "Izba";
				case KIzbaThatch: return "IzbaThatch";
				case KKhata: return "Khata";
				case KChimney: return "BurntChimney";
				case KWattle: return "WattleFence";
				case KSweep: return "WellSweep";
				case KHaystack: return "Haystack";
				default: return nullptr;
				}
			}
			// (the post, the snow pole and the snow fence are built here in KitPlain: no file to read or to miss)
			bool KitBuilt(int K) const override { return K == KPost || K == KMarker || K == KDrift; }
			// (the trial "houses": the east's log izba, its plank roof or its thatch, LandHouse's Kind 7; the south's whitewashed
			// khata under its hipped thatch, Kind 8; and the burnt-out house its chimney stands for, Kind 9, at its own size: an
			// izba's footprint, its logs burnt down, in Russia; a khata's, its clay walls standing roofless, in the south)
			bool KitHouse(int K, LandHouse::Spec& Out) const override
			{
				if (!bEast || (K != KIzba && K != KIzbaThatch && K != KKhata && K != KChimney)) return false;
				if (K == KChimney)
				{
					Out.Kind = 9;
					Out.Roof = bRussia ? 0 : 1;
					Out.Long = bRussia ? 7.5 : 9.18;
					Out.Deep = bRussia ? 7.2 : 6.46;
					return true;
				}
				Out.Kind = K == KKhata ? 8 : 7;
				Out.Roof = K == KIzba ? 0 : 1;
				return true;
			}
			void KitLook(int K, LandField::BaseMesh& M) const override
			{
				switch (K)
				{
				case KSpruce:
					M.bTree = true; // (drawn far off as a crown, as the bocage's trees)
					M.bSight = true;
					M.SightMid = 0.45f;
					M.SightHalf = 0.45f;
					M.SightAcross = 0.75f;
					M.SightTrunk = 0.35f;
					M.FarFrom = 220.0f;
					// (on trial: the snow-laden spruce far off, as the map and the high eyes see a wood of it, a dark mass
					// some 40,55,45 flecked with snow, not a grey static: its boughs darkened, one snow point in three kept
					// white; the art director's winter carry, 20:08Z. Near, as built, but under "sprucedepth" below.)
					if (bSpruceTrial && SnowLying > 0.2)
					{
						// (a level's boughs darkened, its snow kept white where Kept says and else a bough's)
						auto Darken = [this](LandField::Mesh& Me, auto&& Kept) {
							for (size_t P = 0; P < Me.Colours.size(); ++P)
							{
								const Tone C = Tone{ static_cast<double>(Me.Colours[P].R), static_cast<double>(Me.Colours[P].G), static_cast<double>(Me.Colours[P].B) };
								const double L = 0.299 * C.R + 0.587 * C.G + 0.114 * C.B;
								// (the art director, 23:42Z: green-black, about 40,55,45 as seen with green at or over blue, one mass
								// with snow flecks: the boughs written greener and with less blue than that, as the map read them a
								// quarter darker and the sky's fill added blue (38,56,34 read 31,42,44), half the
								// speckle and half the flecks of the first try, which read blue-black and static)
								const Tone Bough = Varied(Rgb(50, 72, 36), 0.93 + 0.14 * Unit(Hash(Seed ^ 0x5E5, P)), 0.0);
								const Tone T = L > 170.0 && Kept(P) ? C : L > 170.0 ? Bough : Mixed(C, Bough, 0.9);
								Me.Colours[P] = LandField::Rgb8{ static_cast<uint8_t>(std::clamp(T.R, 0.0, 255.0)), static_cast<uint8_t>(std::clamp(T.G, 0.0, 255.0)), static_cast<uint8_t>(std::clamp(T.B, 0.0, 255.0)) };
							}
						};
						Darken(M.Far, [this](size_t P) { return Unit(Hash(Seed ^ 0x5E6, P)) < 0.17; });
						// (on trial, "sprucedepth": near too, so the wall a soldier sees is the mass the map sees, not a
						// half-white grey one, 46% of it snow on the Mac: the boughs as far off; the snow only on the pads that
						// face up (more than 0.6 of their normal up), one pad in two of those, each pad white or green whole
						// (its points joined by its triangles), not a speckle of points. One in two leaves the cloud painter's
						// wall some 23% snow where it was 48%, the 20 to 30% asked; one in three left it 16%.)
						if (bDepthTrial)
							for (LandField::Mesh* Me : { &M.Near, &M.Mid })
							{
								std::vector<uint32_t> Pad(Me->Points());
								for (size_t P = 0; P < Pad.size(); ++P) Pad[P] = static_cast<uint32_t>(P);
								auto Root = [&Pad](uint32_t P) {
									while (Pad[P] != P)
									{
										Pad[P] = Pad[Pad[P]];
										P = Pad[P];
									}
									return P;
								};
								for (size_t T = 0; T + 2 < Me->Triangles.size(); T += 3)
									for (size_t C = 1; C < 3; ++C) Pad[Root(Me->Triangles[T + C])] = Root(Me->Triangles[T]);
								const bool bFacings = Me->Normals.size() >= 3 * Pad.size();
								Darken(*Me, [&](size_t P) {
									const uint32_t R0 = Root(static_cast<uint32_t>(P));
									return bFacings && Me->Normals[3 * static_cast<size_t>(R0) + 2] > 0.6f && Unit(Hash(Seed ^ 0x5E7, R0)) < 0.5;
								});
							}
					}
					break;
				case KBirch:
					M.bTree = true;
					M.bSight = !bBare;
					M.SightMid = 0.65f;
					M.SightHalf = 0.3f;
					M.SightAcross = 0.6f;
					M.SightTrunk = 0.25f;
					M.FarFrom = 200.0f;
					break;
				case KIzba:
				case KIzbaThatch:
				case KKhata:
					DullChimney(M);
					[[fallthrough]];
				case KChimney:
				case KHaystack:
					M.bSight = true;
					M.SightMid = 0.5f;
					M.SightHalf = 0.5f;
					M.SightAcross = 0.9f;
					M.FarFrom = 300.0f;
					break;
				case KWattle: M.FarFrom = 80.0f; break;
				case KPole: M.FarFrom = 160.0f; break;
				case KPost: M.FarFrom = 45.0f; break;
				case KMarker: M.FarFrom = 120.0f; break;
				case KDrift: M.FarFrom = 90.0f; break;
				default: M.FarFrom = 150.0f; break;
				}
			}
			void KitPlain(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				switch (K)
				{
				case KSpruce:
					W.Box(0.0, 0.0, 2.5, 0.25, 0.25, 2.5, 0.0, Rgb(84, 70, 56));
					W.Box(0.0, 0.0, 10.0, 2.4, 2.4, 7.5, 0.0, Rgb(46, 64, 46));
					break;
				case KBirch:
					W.Box(0.0, 0.0, 5.0, 0.15, 0.15, 5.0, 0.0, Rgb(214, 210, 200));
					W.Box(0.0, 0.0, 10.5, 2.0, 2.0, 4.0, 0.0, bBare ? Rgb(118, 100, 94) : Rgb(96, 130, 60));
					break;
				case KIzba:
				case KIzbaThatch:
				case KKhata: W.Box(0.0, 0.0, 1.6, 3.8, 3.4, 1.6, 0.0, K == KKhata ? Rgb(226, 222, 210) : Rgb(112, 100, 88)); break;
				case KChimney: W.Box(0.0, 0.0, 2.2, 0.6, 0.6, 2.2, 0.0, Rgb(150, 92, 74)); break;
				case KWattle: W.Box(0.0, 0.0, 0.7, 2.0, 0.05, 0.7, 0.0, Rgb(122, 102, 72)); break;
				case KSweep: W.Box(0.0, 0.0, 3.0, 0.1, 0.1, 3.0, 0.0, Rgb(110, 96, 80)); break;
				case KHaystack: W.Rock(0.0, 0.0, 1.6, 3.6, 5, Rgb(170, 150, 100), Rgb(120, 104, 70)); break;
				case KPost:
				{
					// a split oak post, weathered grey-brown, 1.3 m out of the ground; two strands of wire to the next post
					W.Box(0.0, 0.0, 0.6, 0.055, 0.05, 0.7, 0.0, Rgb(92, 80, 66));
					M.Far = M.Near;
					W.Box(0.5 * PostStep, 0.0, 0.72, 0.5 * PostStep, 0.007, 0.007, 0.0, Rgb(78, 76, 74));
					W.Box(0.5 * PostStep, 0.0, 1.1, 0.5 * PostStep, 0.007, 0.007, 0.0, Rgb(78, 76, 74));
					M.FarFrom = 45.0f;
					return;
				}
				case KMarker:
					// a hazel stake two metres and more, a twist of straw bound at its top
					W.Box(0.0, 0.0, 1.05, 0.028, 0.028, 1.15, 0.0, Rgb(96, 80, 64));
					W.Box(0.0, 0.0, 2.12, 0.1, 0.07, 0.16, 0.5, Rgb(176, 154, 104));
					W.Box(0.0, 0.0, 2.0, 0.07, 0.1, 0.1, -0.4, Rgb(160, 140, 94));
					break;
				case KDrift:
				{
					// a snow fence's panel: grey lath on two rails, 1.3 m high, 4 m long, staked at its ends
					W.Box(0.0, 0.0, 0.32, 2.0, 0.018, 0.04, 0.0, Rgb(104, 94, 82));
					W.Box(0.0, 0.0, 1.06, 2.0, 0.018, 0.04, 0.0, Rgb(104, 94, 82));
					for (int L = 0; L < 16; ++L)
					{
						const double Shade = 0.88 + 0.24 * Unit(Hash(Seed ^ 0xD71F, static_cast<uint64_t>(L)));
						const double Sink = 0.06 * Unit(Hash(Seed ^ 0xD720, static_cast<uint64_t>(L)));
						W.Box(-1.875 + 0.25 * L, 0.03, 0.66 - Sink, 0.045, 0.01, 0.66, 0.0, Rgb(118 * Shade, 108 * Shade, 96 * Shade));
					}
					for (const double End : { -1.96, 1.96 }) W.Box(End, -0.04, 0.72, 0.035, 0.035, 0.82, 0.0, Rgb(92, 80, 66));
					break;
				}
				default: W.Box(0.0, 0.0, 3.6, 0.1, 0.1, 3.6, 0.0, Rgb(104, 92, 78)); break;
				}
				M.Far = M.Near;
				M.FarFrom = 400.0f;
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				Rng R(Seed ^ (0xC1u + static_cast<uint64_t>(K) * 977u));
				switch (K)
				{
				case 0:
				{
					// a grass tuft: blades from a tight foot, leaning out, built a mid green; in the bare months dead and straw
					// pale (the tint only shades it); out of the snow the dead grass's grey-buff, every blade bent the wind's
					// way (its +X: the snow turns each tuft to the wind)
					const bool bSnow = SnowLying > 0.15;
					const Tone Foot = bSnow ? Rgb(120, 110, 94) : bBare ? Rgb(112, 96, 70) : Rgb(66, 84, 42);
					const Tone Tip = bSnow ? Rgb(166, 156, 130) : bBare ? Rgb(184, 164, 116) : Rgb(122, 144, 72);
					for (int B = 0; B < 9; ++B)
					{
						const double A = bSnow ? R.Range(-0.55, 0.55) : R.Range(0.0, 2.0 * Pi), Lean = R.Range(0.05, 0.2) * (bSnow ? 1.5 : 1.0), High = R.Range(0.18, 0.38);
						const double FX = R.Range(-0.05, 0.05), FY = R.Range(-0.05, 0.05);
						const double Wide = R.Range(0.025, 0.045);
						W.Blade(FX, FY, 0.0, FX + Lean * std::cos(A), FY + Lean * std::sin(A), High, Wide, Foot, Tip);
					}
					M.Paint = 2;
					M.FarFrom = 55.0f;
					break;
				}
				case 1:
				{
					// stubble: a drill row of cut straws along x, a few lying
					for (int B = 0; B < 12; ++B)
					{
						const double X = -0.55 + 1.1 * (B + R.Next()) / 12.0, Y = R.Range(-0.03, 0.03), High = R.Range(0.08, 0.14);
						const double LeanX = R.Range(-0.02, 0.02), LeanY = R.Range(-0.02, 0.02);
						W.Blade(X, Y, 0.0, X + LeanX, Y + LeanY, High, 0.022, Rgb(150, 132, 92), Rgb(202, 184, 132));
					}
					for (int B = 0; B < 2; ++B)
					{
						const double X = R.Range(-0.4, 0.4), A = R.Range(0.0, Pi);
						W.Blade(X, 0.0, 0.01, X + 0.25 * std::cos(A), 0.25 * std::sin(A), 0.02, 0.02, Rgb(176, 158, 112), Rgb(196, 178, 128));
					}
					M.Paint = 2;
					M.FarFrom = 40.0f;
					break;
				}
				case 2:
				{
					// dry weed stalks: three or four stems from one foot, each with its seed head: a thistle's or a knapweed's
					// round, a wild carrot's flat umbel on its spreading rays (the art director, 23:42Z: never a cube on a stick)
					for (int B = 0; B < 4; ++B)
					{
						const double A = R.Range(0.0, 2.0 * Pi), Lean = R.Range(0.02, 0.12), High = R.Range(0.5, 0.85);
						const double TX = Lean * std::cos(A), TY = Lean * std::sin(A);
						W.Blade(0.0, 0.0, 0.0, TX, TY, High, 0.03, Rgb(112, 92, 64), Rgb(146, 124, 88));
						W.Blade(0.0, 0.0, 0.0, -TY * 0.5, TX * 0.5, High * 0.92, 0.025, Rgb(112, 92, 64), Rgb(140, 118, 84));
						if (B % 2 == 0)
						{
							const double Deep = R.Range(0.028, 0.04), Bulk = R.Range(0.016, 0.024);
							W.Bead(TX, TY, High + 0.005, Bulk, Deep, Rgb(150, 132, 104), Rgb(104, 88, 64));
						}
						else
						{
							const double Half = R.Range(0.035, 0.05);
							for (int H = 0; H < 6; ++H)
							{
								const double Ray = A + H * Pi / 3.0;
								W.Blade(TX, TY, High - 0.04, TX + 0.9 * Half * std::cos(Ray), TY + 0.9 * Half * std::sin(Ray), High + 0.01, 0.008, Rgb(118, 98, 70), Rgb(140, 120, 88));
							}
							W.Bead(TX, TY, High + 0.012, Half, 0.012, Rgb(156, 138, 108), Rgb(112, 96, 72));
						}
					}
					M.Paint = 2;
					M.FarFrom = 60.0f;
					break;
				}
				case 3:
				{
					// clods: a loose spill of turned earth over a metre of furrow (x), seven lumps of mixed size, 6 to 26 cm
					// long, each longer along the furrow than across it, low and half sunk, built the soil's own colour (the
					// tint gives the soil), their undersides hardly darker. Not three same-size lumps in a file (the art
					// director, 24 Nov, Voronezh re-shoot 2: dark stepping stones in single-file lines; as boxes before that,
					// hard brown bricks round the men, a boot through one)
					static constexpr double Spill[7][3] = { // (x, y, its half length)
						{ -0.44, 0.06, 0.05 }, { -0.2, -0.17, 0.09 }, { 0.03, 0.13, 0.13 }, { 0.27, -0.06, 0.07 },
						{ -0.07, -0.33, 0.035 }, { 0.2, 0.3, 0.045 }, { 0.47, 0.16, 0.03 } };
					for (int B = 0; B < 7; ++B)
					{
						const double RX = Spill[B][2] * R.Range(0.85, 1.15), RY = RX * R.Range(0.55, 0.8);
						Clod(W, Spill[B][0] + R.Range(-0.05, 0.05), Spill[B][1] + R.Range(-0.05, 0.05), RX, RY, RY * R.Range(0.45, 0.7), 51 + static_cast<uint64_t>(B),
							Rgb(128, 112, 94), Rgb(116, 101, 85));
					}
					M.Paint = 0;
					M.FarFrom = 40.0f;
					break;
				}
				case 4:
				{
					// the young corn: a drill row of shoots along x
					for (int B = 0; B < 12; ++B)
					{
						const double X = -0.6 + 1.2 * (B + R.Next()) / 12.0, Y = R.Range(-0.03, 0.03);
						const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.07, 0.12);
						W.Blade(X, Y, 0.0, X + 0.03 * std::cos(A), Y + 0.03 * std::sin(A), High, 0.02, Rgb(70, 96, 42), Rgb(112, 150, 62));
					}
					M.Paint = 2;
					M.FarFrom = 35.0f;
					break;
				}
				default: break;
				}
			}

			// ------------------------------------------------------------------ the villages

			void Sites(Host& H) override
			{
				double TX = 0.0, TY = 0.0, TR = 0.0;
				if (!H.Town(TX, TY, TR)) return;
				if (bEast && F().Lon >= 19.0) EastVillage(H, TX, TY, TR);
				else WestVillage(H, TX, TY, TR);
			}

			/** The west's village along its roads, and its farmsteads out along the lanes: the ground's own stone houses. */
			void WestVillage(Host& H, double TX, double TY, double TR)
			{
				Rng R(H.Seed() ^ 0x7E57);
				int Houses = 0, Hurt = 0, Farms = 0;
				auto HurtOf = [&R](double Odds) {
					const double U = R.Next() / Odds;
					return U < 0.07 ? 3 : U < 0.17 ? 2 : U < 0.34 ? 1 : 0;
				};
				auto RoofOf = [this, &R]() { return bSlate ? (R.Next() < 0.88 ? 0 : 1) : (R.Next() < 0.25 ? 0 : 1); };
				const double WhiteShare = bArdennes ? 0.4 : bEast ? 0.6 : 0.18;
				// The church first, at the village's heart, its nave east and west (its own draws: the houses' as before; on
				// trial, Fd.Tried("church"))
				if (Fd.Tried("church"))
				{
					Rng RC(H.Seed() ^ 0xC4C4);
					bool bChurch = false;
					for (int Ring = 0; Ring < 10 && !bChurch; ++Ring)
						for (int K = 0; K < 12 && !bChurch; ++K)
						{
							const double A = K * 2.0 * Pi / 12.0 + Ring * 0.37, D = Ring * 8.0;
							const int Kind = RC.Next() < 0.3 ? 1 : 0;
							const double Turn = RC.Range(-0.1, 0.1);
							bChurch = H.Church(TX + D * std::cos(A), TY + D * std::sin(A), Turn, Kind);
						}
				}
				// Houses fronting the roads through the village, both sides, a few metres back; now and then a long-house
				// (house and barn under one roof) or a barn behind.
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const auto& P = H.LaneLine(L);
					const double Half = H.LaneHalf(L);
					for (int Side = -1; Side <= 1; Side += 2)
					{
						size_t K = 0;
						while (K + 2 < P.size() && Houses < 60)
						{
							const double AX = P[K].first, AY = P[K].second;
							if (std::hypot(AX - TX, AY - TY) > TR * 1.1)
							{
								++K;
								continue;
							}
							const size_t KE = std::min(P.size() - 1, K + 3);
							double DX = P[KE].first - AX, DY = P[KE].second - AY;
							const double DL = std::max(1e-6, std::hypot(DX, DY));
							DX /= DL;
							DY /= DL;
							const double NX = -DY * Side, NY = DX * Side;
							const double ULong = R.Next(), UDeep = R.Next(), UBack = R.Next(), UType = R.Next(), UStorey = R.Next(), UWhite = R.Next();
							const bool bLong = (bArdennes ? 0.35 : 0.18) > UType;
							const double Long = bLong ? 18.0 + 10.0 * ULong : 8.5 + 6.0 * ULong;
							const double Deep = bLong ? 8.0 + 1.2 * UDeep : 6.4 + 1.4 * UDeep;
							const double Back = UBack < 0.6 ? 0.6 + 1.4 * UBack : 2.0 + 5.0 * UBack;
							const double Off = Half + 1.3 + Back + 0.5 * Deep;
							const double CX = AX + DX * (0.5 * Long) + NX * Off, CY = AY + DY * (0.5 * Long) + NY * Off;
							const double Yaw = std::atan2(DY, DX) + R.Range(-0.04, 0.04);
							const int Storeys = bLong ? (UStorey < 0.5 ? 2 : 1) : (UStorey < 0.6 ? 2 : 1);
							const int Damage = HurtOf(1.0);
							const int Roof = RoofOf();
							if (H.Building(CX, CY, Yaw, Long, Deep, bLong ? 1 : 0, Storeys, Roof, UWhite < WhiteShare, Damage))
							{
								++Houses;
								Hurt += Damage ? 1 : 0;
								// a barn or a shed behind, across the yard
								if (R.Next() < 0.35)
								{
									const double BLong = R.Range(10.0, 16.0), BDeep = R.Range(7.0, 9.0), BOff = Off + 0.5 * Deep + R.Range(5.0, 9.0) + 0.5 * BDeep;
									const int BDamage = HurtOf(1.2);
									const double BTilt = R.Range(-0.05, 0.05);
									if (H.Building(AX + DX * (0.5 * Long) + NX * BOff, AY + DY * (0.5 * Long) + NY * BOff, Yaw + BTilt, BLong, BDeep, 2, 1, Roof, false, BDamage))
									{
										++Houses;
										Hurt += BDamage ? 1 : 0;
									}
								}
								const double Gap = R.Range(1.5, 9.0);
								K += static_cast<size_t>((Long + Gap) / 2.0) + 1;
							}
							else K += 2;
						}
					}
				}
				// Farmsteads and hamlets out along the lanes: a farmhouse (or the Ardennes' long-house), its barn and its byre
				// round a yard on the lane's side, some 400 m apart.
				std::vector<std::pair<double, double>> Done;
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const auto& P = H.LaneLine(L);
					const double Half = H.LaneHalf(L);
					for (size_t K = 8; K + 8 < P.size(); K += 13)
					{
						if (Farms >= 9) break;
						const double AX = P[K].first, AY = P[K].second;
						if (AX < 60.0 || AY < 60.0 || AX > H.Width() - 60.0 || AY > H.Depth() - 60.0) continue;
						if (std::hypot(AX - TX, AY - TY) < TR + 160.0) continue;
						bool bNear = false;
						for (const auto& [FX, FY] : Done)
							if (std::hypot(FX - AX, FY - AY) < 420.0) bNear = true;
						const double UKeep = R.Next();
						if (bNear || UKeep < 0.55) continue;
						double DX = P[K + 2].first - P[K - 2].first, DY = P[K + 2].second - P[K - 2].second;
						const double DL = std::max(1e-6, std::hypot(DX, DY));
						DX /= DL;
						DY /= DL;
						const double USide = R.Next();
						const double Side = USide < 0.5 ? -1.0 : 1.0;
						const double NX = -DY * Side, NY = DX * Side;
						const double Vs = Half + 3.5;
						// (Building keeps them off the woods and the water: At is not to be asked while the sites are laid, the
						// parcels not yet cut)
						const double Yaw = std::atan2(DY, DX), YawV = std::atan2(NY, NX);
						const int Roof = RoofOf();
						const bool bWhite = R.Next() < WhiteShare;
						const bool bLongHouse = bArdennes && R.Next() < 0.6;
						int Laid = 0;
						const int D1 = HurtOf(1.3), D2 = HurtOf(1.3), D3 = HurtOf(1.3);
						if (H.Building(AX + NX * (Vs + 24.0), AY + NY * (Vs + 24.0), Yaw, bLongHouse ? 26.0 : 14.0, bLongHouse ? 9.0 : 7.4, 1, 2, Roof, bWhite, D1))
						{
							++Laid;
							Hurt += D1 ? 1 : 0;
							if (!bLongHouse && H.Building(AX - DX * 10.5 + NX * (Vs + 10.0), AY - DY * 10.5 + NY * (Vs + 10.0), YawV, 15.0, 8.4, 2, 1, Roof, false, D2))
							{
								++Laid;
								Hurt += D2 ? 1 : 0;
							}
							if (H.Building(AX + DX * 11.5 + NX * (Vs + 11.0), AY + DY * 11.5 + NY * (Vs + 11.0), YawV, 13.0, 6.8, 3, 1, Roof, false, D3))
							{
								++Laid;
								Hurt += D3 ? 1 : 0;
							}
							Houses += Laid;
							++Farms;
							Done.push_back({ AX, AY });
						}
					}
				}
				H.Tally(Houses, Hurt, Farms);
			}

			/** The east's village: each household a plot along its road, its cottage (an izba of logs in Russia, a
			 * whitewashed khata in Poland and the Ukraine) behind a wattle fence, a haystack behind, a well sweep at one plot
			 * in four; some burnt to their stove and chimney. */
			void EastVillage(Host& H, double TX, double TY, double TR)
			{
				Rng R(H.Seed() ^ 0xE457);
				int Houses = 0, Burnt = 0;
				constexpr double Panel = 4.11;
				auto Fence = [&](double AX, double AY, double BX, double BY, double Gap) {
					const double L = std::hypot(BX - AX, BY - AY);
					if (L < 2.0) return;
					const int N = std::max(1, static_cast<int>(std::lround(L / Panel)));
					const double TXd = (BX - AX) / L, TYd = (BY - AY) / L, Each = L / N, Yaw = std::atan2(TYd, TXd);
					for (int K = 0; K < N; ++K)
					{
						const double Mid = (K + 0.5) * Each;
						const double UDown = R.Next(), UYaw = R.Range(-0.03, 0.03), UHigh = R.Range(0.85, 1.08);
						if (Gap >= 0.0 && std::fabs(Mid - Gap) < 0.5 * Each) continue;
						if (UDown < 0.1) continue;
						H.Plant(LandField::Base::Kit6, AX + TXd * Mid, AY + TYd * Mid, Yaw + UYaw, Each / Panel, UHigh, Tint(1.0, 1.0, 1.0));
					}
				};
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const auto& P = H.LaneLine(L);
					const double Half = H.LaneHalf(L);
					for (int Side = -1; Side <= 1; Side += 2)
					{
						size_t K = 0;
						while (K + 2 < P.size() && Houses + Burnt < 70)
						{
							const double AX = P[K].first, AY = P[K].second;
							if (std::hypot(AX - TX, AY - TY) > TR * 1.15)
							{
								++K;
								continue;
							}
							const double Front = R.Range(18.0, 30.0);
							size_t KE = K;
							while (KE + 1 < P.size() && std::hypot(P[KE].first - AX, P[KE].second - AY) < Front) ++KE;
							double DX = P[KE].first - AX, DY = P[KE].second - AY;
							const double DL = std::max(1e-6, std::hypot(DX, DY));
							DX /= DL;
							DY /= DL;
							const double NX = -DY * Side, NY = DX * Side;
							const double Line0 = Half + 1.2 + 0.6;
							const double UKind = R.Next(), UGable = R.Next(), UBack = R.Next(), URuin = R.Next(), UFlip = R.Next();
							const int Kit = bRussia ? (UKind < 0.55 ? KIzba : KIzbaThatch) : (UKind < 0.75 ? KKhata : KIzbaThatch);
							const double KLong = Kit == KKhata ? 9.18 : 7.5, KDeep = Kit == KKhata ? 6.46 : 7.2;
							const bool bGable = UGable < 0.4;
							const double Along = bGable ? KDeep : KLong, Across = bGable ? KLong : KDeep;
							const double Back = 3.0 + 6.0 * UBack;
							const double U = std::clamp(0.5 * Front, 0.5 * Along + 2.0, std::max(0.5 * Along + 2.0, Front - 0.5 * Along - 2.0));
							const double CX = AX + DX * U + NX * (Line0 + Back + 0.5 * Across), CY = AY + DY * U + NY * (Line0 + Back + 0.5 * Across);
							const double Yaw = std::atan2(DY, DX) + (bGable ? 0.5 * Pi : 0.0) + (UFlip < 0.5 ? Pi : 0.0);
							const LandField::Base B = static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + Kit);
							bool bLaid = false;
							if (URuin < 0.28)
							{
								if (H.Free(CX, CY, 5.0))
								{
									H.Plant(LandField::Base::Kit5, CX, CY, Yaw, 0.7, 1.0, Tint(1.0, 1.0, 1.0));
									H.Clear(CX, CY, 6.0);
									++Burnt;
									bLaid = true;
								}
							}
							else if (H.House(B, CX, CY, Yaw, KLong, KDeep))
							{
								++Houses;
								bLaid = true;
							}
							if (!bLaid)
							{
								K += 2;
								continue;
							}
							const double F0X = AX + NX * Line0, F0Y = AY + NY * Line0, F1X = F0X + DX * Front, F1Y = F0Y + DY * Front;
							const double UFence = R.Next(), USideFence = R.Next(), UGate = R.Range(-2.0, 2.0);
							if (UFence < 0.85) Fence(F0X, F0Y, F1X, F1Y, U + UGate);
							if (USideFence < 0.45) Fence(F0X + DX * 0.3, F0Y + DY * 0.3, F0X + DX * 0.3 + NX * (Back + Across + 8.0), F0Y + DY * 0.3 + NY * (Back + Across + 8.0), -1.0);
							const double UWell = R.Next(), UWellAt = R.Range(2.0, std::max(2.5, Front - 2.0)), UWellIn = R.Range(1.8, 3.2), UWellYaw = R.Range(0.0, 2.0 * Pi);
							if (UWell < 0.25)
							{
								const double WX = AX + DX * UWellAt + NX * (Line0 + UWellIn), WY = AY + DY * UWellAt + NY * (Line0 + UWellIn);
								if (H.Free(WX, WY, 1.2) && std::hypot(WX - CX, WY - CY) > 0.5 * KLong + 2.5) H.Plant(LandField::Base::Kit7, WX, WY, UWellYaw, 1.0, 1.0, Tint(1.0, 1.0, 1.0));
							}
							const double UStack = R.Next(), UStackOut = R.Range(4.0, 9.0), UStackAlong = R.Range(-4.0, 4.0), UStackYaw = R.Range(0.0, 2.0 * Pi), USize = R.Range(0.8, 1.15);
							if (UStack < 0.5)
							{
								const double SX = CX + NX * (0.5 * Across + UStackOut) + DX * UStackAlong, SY = CY + NY * (0.5 * Across + UStackOut) + DY * UStackAlong;
								if (H.Free(SX, SY, 2.0))
								{
									H.Plant(LandField::Base::Kit8, SX, SY, UStackYaw, USize, USize, Tint(1.0, 1.0, 1.0));
									H.Clear(SX, SY, 2.5);
								}
							}
							K = KE;
						}
					}
				}
				H.Tally(Houses + Burnt, Burnt, 0);
			}

			// ------------------------------------------------------------------ the walls: the farmyards' in the west

			Tone WallStone() const override { return bArdennes ? Rgb(150, 146, 136) : Rgb(172, 162, 140); }
			double WallIvy() const override { return 0.2; }

			// ------------------------------------------------------------------ the woods, the roads' trees, the poles

			void Plants(Host& H) override
			{
				Rng R(H.Seed() ^ 0x9A48);
				const double B = H.Band(), W = H.Width() + 2.0 * B, D = H.Depth() + 2.0 * B;
				auto Bocage = [](LandField::Base K) {
					switch (K)
					{
					case LandField::Base::Oak: return 15.0;
					case LandField::Base::Elm: return 18.0;
					case LandField::Base::Ash: return 15.0;
					case LandField::Base::Beech: return 18.0;
					case LandField::Base::Apple: return 5.5;
					default: return 15.0;
					}
				};
				const LandField::Base Spruce = LandField::Base::Kit0, Birch = LandField::Base::Kit1, Pole = LandField::Base::Kit9;
				// A first coarse look at the ground (one At a 10 m square), so the fine grids below ask only near a wood or a
				// village: a square whose middle is further in from its parcel's edge than its own reach is that parcel's
				// kind throughout, and at least that far in less its reach.
				const double CS = 10.0, CReach = 0.5 * CS * 1.4143 + 3.0;
				const int CNX = static_cast<int>(std::ceil(W / CS)), CNY = static_cast<int>(std::ceil(D / CS));
				std::vector<uint8_t> CKind(static_cast<size_t>(CNX) * CNY);
				std::vector<float> CEdge(CKind.size());
				std::vector<uint64_t> CKey(CKind.size());
				std::vector<int8_t> CCrop(CKind.size());
				for (int J = 0; J < CNY; ++J)
					for (int I = 0; I < CNX; ++I)
					{
						const Spot S = H.At(-B + (I + 0.5) * CS, -B + (J + 0.5) * CS);
						const size_t C = static_cast<size_t>(J) * CNX + I;
						CKind[C] = static_cast<uint8_t>(S.Kind);
						CEdge[C] = static_cast<float>(S.Edge);
						CKey[C] = S.Key;
						CCrop[C] = static_cast<int8_t>(S.Crop);
					}
				auto Coarse = [&](double X, double Y, int& Kind) {
					const int I = std::clamp(static_cast<int>((X + B) / CS), 0, CNX - 1), J = std::clamp(static_cast<int>((Y + B) / CS), 0, CNY - 1);
					Kind = CKind[static_cast<size_t>(J) * CNX + I];
					return static_cast<double>(CEdge[static_cast<size_t>(J) * CNX + I]);
				};
				// The woods: each compartment spruce in rows (some 4.5 m apart) or broadleaf (beech, oak, ash; birch in the
				// east) at some 7 m; the rides between them open.
				{
					const double Step = 4.6;
					// (on trial, "sprucedepth": the rows shaken loose, 1.2 m, so the 4.6 m rhythm does not read from the field
					// (at 0.9 a stand's far edge still read as a comb from 30 m up); a tree near the edge asked at its own
					// place, so its metres in are known, not its square's)
					const double ConJitter = bDepthTrial ? 1.2 : 0.35, AskIn = bDepthTrial ? 5.0 : 1.0;
					// (on trial, "sprucedepth": metres in from a wood's edge, read between the coarse squares' middles, so the
					// wood darkens smoothly inward and not by 10 m squares: a wood's square counts its metres in, any other's
					// its metres out, never more than it is from the wood)
					auto InWood = [&](double X, double Y) {
						const double U = std::clamp((X + B) / CS - 0.5, 0.0, CNX - 1.0), V = std::clamp((Y + B) / CS - 0.5, 0.0, CNY - 1.0);
						const int I = std::min(static_cast<int>(U), CNX - 2), J = std::min(static_cast<int>(V), CNY - 2);
						auto In = [&](int A, int Bj) {
							const size_t C = static_cast<size_t>(Bj) * CNX + A;
							return CKind[C] == KWoods ? static_cast<double>(CEdge[C]) : -static_cast<double>(CEdge[C]);
						};
						return Lerp(Lerp(In(I, J), In(I + 1, J), U - I), Lerp(In(I, J + 1), In(I + 1, J + 1), U - I), V - J);
					};
					// (a grid in the compartments' own frame, so the spruce stand in their rows: its reach the field and its
					// band turned into that frame)
					double U0 = 1e9, U1 = -1e9, V0 = 1e9, V1 = -1e9;
					for (const double CX : { -B, W - B })
						for (const double CY : { -B, D - B })
						{
							const double CU = CX * Ca + CY * Sa, CV = -CX * Sa + CY * Ca;
							U0 = std::min(U0, CU);
							U1 = std::max(U1, CU);
							V0 = std::min(V0, CV);
							V1 = std::max(V1, CV);
						}
					const int64_t I0 = static_cast<int64_t>(std::floor(U0 / Step)), I1 = static_cast<int64_t>(std::ceil(U1 / Step));
					const int64_t J0 = static_cast<int64_t>(std::floor(V0 / Step)), J1 = static_cast<int64_t>(std::ceil(V1 / Step));
					for (int64_t J = J0; J <= J1; ++J)
						for (int64_t I = I0; I <= I1; ++I)
						{
							const double U = I * Step, V = J * Step;
							const double X = U * Ca - V * Sa, Y = U * Sa + V * Ca;
							if (X < -B || Y < -B || X > W - B || Y > D - B) continue;
							{
								// (no wood near: nothing more to ask; 1.7 m for the jitter below)
								int CK0 = KOpen;
								const double CE0 = Coarse(X, Y, CK0);
								if (CE0 > CReach + 1.7 && CK0 != KWoods) continue;
							}
							// (each place's own numbers, not a stream's: most places are no wood at all)
							const uint64_t Hp = Hash(Seed ^ 0x3005, static_cast<uint64_t>(I + 100000), static_cast<uint64_t>(J + 100000));
							const double Keep = Unit(Hash(Hp, 1));
							const bool bCon = Conifer(X, Y);
							if (!bCon && Keep > 0.45) continue; // (the broadleaf sparser: some 7 m apart)
							if (bCon && Keep < 0.04) continue; // (a gap where one fell)
							const double J1u = Unit(Hash(Hp, 2)), J2u = Unit(Hash(Hp, 3)), Yaw = 2.0 * Pi * Unit(Hash(Hp, 4)), Size = Unit(Hash(Hp, 5)), Kind = Unit(Hash(Hp, 6));
							const double PX = X + (bCon ? ConJitter : 2.4) * (J1u - 0.5), PY = Y + (bCon ? ConJitter : 2.4) * (J2u - 0.5);
							int CK = KOpen;
							const double CE = Coarse(PX, PY, CK);
							if (CE > CReach && CK != KWoods) continue;
							double Edge = -1.0; // (metres in, where asked)
							if (CE < CReach + AskIn)
							{
								const Spot S = H.At(PX, PY);
								if (S.Kind != KWoods || S.Edge < 1.0) continue;
								Edge = S.Edge;
							}
							if (!H.Free(PX, PY, 1.6)) continue; // (clear of a lane's floor and its walls)
							if (bCon && bDepthTrial)
							{
								// On trial, "sprucedepth": a deep, ragged wood. A stand's age over some 60 m, not 160, and each
								// tree its own height besides (0.76 to 1.06 of its stand's); one in twelve gone, a gap; within 8 m
								// of the edge one in seven a young spruce, a third to three fifths grown, under the old ones'
								// boughs; of the others one in twelve a leader standing proud of its stand, 1.2 to 1.3 of its
								// height (34 m at most), so that no stand, however even, reads as a comb (the art director's
								// mend, 25 Nov). (The rest kept under 1.06 so that the leaders show: a skyline is the tallest of
								// the many trees each sight line crosses, and with the rest up to 1.28 and a leader in eight, as
								// first asked, an even young stand stayed a comb in the cloud painter.)
								const double Gone = Unit(Hash(Hp, 7)), Own = Unit(Hash(Hp, 8)), Young = Unit(Hash(Hp, 9)), Grown = Unit(Hash(Hp, 10)), Lead = Unit(Hash(Hp, 11));
								if (Gone < 1.0 / 12.0) continue;
								if (Edge < 0.0) Edge = InWood(PX, PY);
								const double Age = Clamp01(0.5 + 0.9 * Noise(X / 60.0, Y / 60.0, Seed ^ 0xA6E));
								const bool bYoung = Edge < 8.0 && Young < 1.0 / 7.0;
								const double Mine = 0.76 + 0.3 * Own;
								const double Of = bYoung ? Mine * (0.35 + 0.25 * Grown) : Lead < 1.0 / 12.0 ? 1.2 + 0.1 * Own : Mine;
								const double Stand = 14.0 + 12.0 * Age, High = Stand * Of;
								// its crown as wide as its stand's, or as its own where it stands taller: in rows 4.6 m apart a
								// spruce's boughs fill the row whatever its height (crowns narrowed with the shorter trees let the
								// snow floor through, and the wood read lighter and bluer from above)
								const double Wide = bYoung ? High : Stand * std::max(Of, 1.0);
								// deeper in, darker, the snow on its boughs dimmer, as the light under the crowns (full by 30 m
								// in): its blue down faster than its red and green, so the deep wood keeps its green over blue.
								// (The sky's fill adds blue to whatever is darker: with blue spared the canopy read bluer from
								// above than under "spruce" alone, and even with blue down as much as red and green it did,
								// 8 less blue than green where "spruce" has 12; the art director's mend, 25 Nov.)
								const double Dk = Smooth(3.0, 30.0, Edge), Lt = 1.0 - 0.3 * Dk, Bl = 1.0 - 0.55 * Dk;
								H.Plant(Spruce, PX, PY, Yaw, Wide / 18.6 * (0.85 + 0.2 * Size), High / 18.6, Tint((0.94 + 0.1 * Size) * Lt, (0.96 + 0.08 * Size) * Lt, 0.96 * Bl));
								continue;
							}
							if (bCon)
							{
								// a stand's age: its spruce of a height, 14 to 26 m, the stands blending (no square blocks)
								const double Age = Clamp01(0.5 + 0.9 * Noise(X / 160.0, Y / 160.0, Seed ^ 0xA6E));
								const double High = (14.0 + 12.0 * Age) * (0.9 + 0.2 * Size);
								H.Plant(Spruce, PX, PY, Yaw, High / 18.6 * (0.85 + 0.2 * Size), High / 18.6, Tint(0.94 + 0.1 * Size, 0.96 + 0.08 * Size, 0.96));
								continue;
							}
							LandField::Base T;
							if (bEast) T = Kind < 0.45 ? Birch : Kind < 0.75 ? LandField::Base::Oak : Kind < 0.9 ? LandField::Base::Ash : Spruce;
							else T = Kind < 0.42 ? LandField::Base::Beech : Kind < 0.78 ? LandField::Base::Oak : Kind < 0.92 ? LandField::Base::Ash : LandField::Base::Elm;
							const double High = 14.0 + 7.0 * Size;
							const double Built = T == Spruce ? 18.6 : T == Birch ? 15.0 : Bocage(T);
							H.Plant(T, PX, PY, Yaw, High / Built * (0.85 + 0.25 * Kind), High / Built, Tint(0.94 + 0.1 * Size, 0.95 + 0.08 * Size, 0.95));
						}
				}
				// The woods' edges: hazel, thorn and bramble in a ragged fringe (bare and dark in winter); and the village's
				// gardens hedged (the Ardennes' beech and hawthorn round its yards and orchards).
				{
					const double Under = 3.2;
					for (double Y = -B + 0.5 * Under; Y < D - B; Y += Under)
						for (double X = -B + 0.5 * Under; X < W - B; X += Under)
						{
							{
								// (no wood's edge and no garden near: nothing more to ask; 1 m for the jitter below)
								int CK0 = KOpen;
								const double CE0 = Coarse(X, Y, CK0);
								if (CE0 > CReach + 6.0 || (CE0 > CReach + 1.0 && CK0 != KWoods && CK0 != KVillage)) continue;
							}
							const uint64_t Hp = Hash(Seed ^ 0xED6E, static_cast<uint64_t>(std::llround(X * 4.0) + 100000), static_cast<uint64_t>(std::llround(Y * 4.0) + 100000));
							const double Keep = Unit(Hash(Hp, 1));
							if (Keep > 0.5) continue;
							const double PX = X + 1.4 * (Unit(Hash(Hp, 2)) - 0.5), PY = Y + 1.4 * (Unit(Hash(Hp, 3)) - 0.5);
							int CK = KOpen;
							const double CE = Coarse(PX, PY, CK);
							if (CE > CReach + 5.0 || (CE > CReach && CK != KWoods && CK != KVillage)) continue;
							const Spot S = H.At(PX, PY);
							const bool bWoodEdge = S.Kind == KWoods && S.Edge < 5.0 && Keep < 0.32;
							const bool bGarden = S.Kind == KVillage && S.Edge < 2.2 && !S.bNear;
							if (!bWoodEdge && !bGarden) continue;
							if (!H.Free(PX, PY, 1.0)) continue;
							const double U = Unit(Hash(Hp, 4)), Size = Unit(Hash(Hp, 5)), Yaw = 2.0 * Pi * Unit(Hash(Hp, 6));
							const LandField::Base K = bGarden ? (U < 0.6 ? LandField::Base::ShrubB : LandField::Base::ShrubA) : U < 0.4 ? LandField::Base::Bramble : U < 0.75 ? LandField::Base::ShrubB : LandField::Base::ShrubA;
							const double High = K == LandField::Base::Bramble ? 0.8 + 0.6 * Size : (bGarden ? 1.6 + 1.2 * Size : 1.8 + 1.8 * Size) / 2.8;
							H.Plant(K, PX, PY, Yaw, 0.8 + 0.45 * Size, High, Tint(0.94 + 0.08 * Size, 0.96 + 0.08 * Size, 0.96));
						}
				}
				// The roads' trees: lines of tall narrow elms (the west's poplars) or birches (the east's), both sides, along
				// some of the roads outside the villages.
				double TX = 0.0, TY = 0.0, TR = 0.0;
				const bool bTown = H.Town(TX, TY, TR);
				size_t Longest = 0;
				double LongestLen = 0.0;
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const auto& P = H.LaneLine(L);
					double Len = 0.0;
					for (size_t K = 1; K < P.size(); ++K) Len += std::hypot(P[K].first - P[K - 1].first, P[K].second - P[K - 1].second);
					if (Len > LongestLen)
					{
						LongestLen = Len;
						Longest = L;
					}
					const double ULine = Unit(Hash(Seed ^ 0x7EE5, static_cast<uint64_t>(L)));
					if (ULine > (bEast ? 0.6 : 0.5)) continue;
					const double Half = H.LaneHalf(L), Apart = bEast ? R.Range(7.0, 10.0) : R.Range(9.0, 13.0);
					double Since = 0.0;
					for (size_t K = 1; K + 1 < P.size(); ++K)
					{
						Since += std::hypot(P[K].first - P[K - 1].first, P[K].second - P[K - 1].second);
						if (Since < Apart) continue;
						Since = 0.0;
						if (bTown && std::hypot(P[K].first - TX, P[K].second - TY) < TR * 1.2) continue;
						double DX = P[K + 1].first - P[K - 1].first, DY = P[K + 1].second - P[K - 1].second;
						const double DL = std::max(1e-6, std::hypot(DX, DY));
						DX /= DL;
						DY /= DL;
						for (int Side = -1; Side <= 1; Side += 2)
						{
							const double Off = Half + 1.2 + 1.6, UMiss = R.Next(), USize = R.Next(), UYaw = R.Range(0.0, 2.0 * Pi);
							const double X = P[K].first - DY * Side * Off, Y = P[K].second + DX * Side * Off;
							if (UMiss < 0.08 || !H.Free(X, Y, 1.6)) continue;
							const Spot S = H.At(X, Y);
							if (S.Kind == KWoods || S.Kind == KVillage || S.Kind == KWater) continue;
							if (bEast || bBare)
							{
								// (birches; in the west's winter too, where the elm's bare crown would not read as a poplar's)
								const double High = 13.0 + 5.0 * USize;
								H.Plant(Birch, X, Y, UYaw, High / 15.0 * 0.9, High / 15.0, Tint(1.0, 1.0, 1.0));
							}
							else
							{
								// (an elm drawn tall and narrow: a Lombardy poplar's spire)
								const double High = 21.0 + 6.0 * USize;
								H.Plant(LandField::Base::Elm, X, Y, UYaw, High / 18.0 * 0.42, High / 18.0, Tint(0.96, 1.0, 0.94));
							}
						}
					}
				}
				// The pastures' wire fences (the Ardennes' and the west's cattle country), each pasture fenced on its own
				// (one parcel's sides found and fitted at a time; a side shared with a fenced neighbour fenced once); west of
				// the Ardennes some pastures hedged instead (hawthorn and blackthorn, an oak or an ash standing out of it).
				if (!bEast)
				{
					const double Share = bArdennes ? 0.85 : 0.3;
					const double Hedged = bArdennes ? 0.0 : 0.45;
					// (the Ardennes' small ploughed fields fenced too, some of them: the cattle turned onto the stubble)
					const double Tilled = bArdennes ? 0.35 : 0.0;
					// (0 open, 1 wired, 2 hedged)
					auto Fenced = [Share, Hedged, Tilled](uint64_t Key, int Kind, int Crop) {
						const double U = Unit(Hash(Key, 0xFE7C));
						if (Kind == KOpen) return (Crop == Pasture || Crop == Meadow) ? (U < Hedged ? 2 : U < Share + Hedged ? 1 : 0) : 0;
						return Kind == KField && U < Tilled ? 1 : 0;
					};
					std::unordered_set<uint64_t> Done;
					for (size_t C = 0; C < CKey.size(); ++C)
					{
						if (CEdge[C] < 6.0f || Fenced(CKey[C], CKind[C], CCrop[C]) == 0 || !Done.insert(CKey[C]).second) continue;
						const double OX = -B + (static_cast<double>(C % CNX) + 0.5) * CS, OY = -B + (static_cast<double>(C / CNX) + 0.5) * CS;
						Fence(H, CKey[C], CKind[C], OX, OY, Fenced(CKey[C], CKind[C], CCrop[C]) == 2, Fenced);
					}
				}
				// West of the Ardennes, the orchards round the villages: a pasture by a village planted with apple trees in
				// rows some 9 m apart (the grass grazed under them).
				if (!bEast && !bArdennes)
				{
					std::unordered_set<uint64_t> Done;
					for (size_t C = 0; C < CKey.size(); ++C)
					{
						if (CKind[C] != KOpen || CCrop[C] != Pasture || CEdge[C] < 5.0f || Unit(Hash(CKey[C], 0x0C4A)) > 0.45 || !Done.insert(CKey[C]).second) continue;
						// (a village within some 120 m: the coarse squares round it)
						const int CXi = static_cast<int>(C % CNX), CYi = static_cast<int>(C / CNX);
						bool bNear = false;
						for (int J = std::max(0, CYi - 12); J <= std::min(CNY - 1, CYi + 12) && !bNear; ++J)
							for (int I = std::max(0, CXi - 12); I <= std::min(CNX - 1, CXi + 12) && !bNear; ++I)
								bNear = CKind[static_cast<size_t>(J) * CNX + I] == KVillage;
						if (!bNear) continue;
						const double OX = -B + (static_cast<double>(CXi) + 0.5) * CS, OY = -B + (static_cast<double>(CYi) + 0.5) * CS;
						Orchard(H, CKey[C], OX, OY);
					}
				}
				// In the snow, poles along the roads to mark their edges (a twist of straw at the top), and snow fences of
				// grey lath some 18 m back on the main road's windward side, against the drifts.
				if (SnowLying > 0.15)
				{
					const LandField::Base Marker = LandField::Base::Kit11, Drift = LandField::Base::Kit12;
					for (size_t L = 0; L < H.Lanes(); ++L)
					{
						const auto& P = H.LaneLine(L);
						const double Half = H.LaneHalf(L);
						const bool bMain = L == Longest;
						const double Windward = Unit(Hash(Seed ^ 0x5F3C, static_cast<uint64_t>(L))) < 0.5 ? -1.0 : 1.0;
						double Run = 0.0, NextPole = 12.0, NextPanel = 0.0;
						for (size_t K = 1; K + 1 < P.size(); ++K)
						{
							Run += std::hypot(P[K].first - P[K - 1].first, P[K].second - P[K - 1].second);
							double DX = P[K + 1].first - P[K - 1].first, DY = P[K + 1].second - P[K - 1].second;
							const double DL = std::max(1e-6, std::hypot(DX, DY));
							DX /= DL;
							DY /= DL;
							if (bTown && std::hypot(P[K].first - TX, P[K].second - TY) < TR * 1.1) continue;
							if (Run >= NextPole)
							{
								NextPole = Run + 24.0;
								for (int Side = -1; Side <= 1; Side += 2)
								{
									const double Off = Half + 1.6, UTurn = R.Range(0.0, 2.0 * Pi), UHigh = R.Range(0.9, 1.1);
									// (the far side's pole half a step on)
									const double Ahead = Side > 0 ? 0.0 : 12.0;
									const double X = P[K].first - DY * Side * Off + DX * Ahead, Y = P[K].second + DX * Side * Off + DY * Ahead;
									if (!H.Free(X, Y, 0.3, true)) continue;
									H.Plant(Marker, X, Y, UTurn, 1.0, UHigh, Tint(1.0, 1.0, 1.0));
								}
							}
							if (bMain && Run >= NextPanel)
							{
								NextPanel = Run + 4.05;
								// (in stretches of 100 to 300 m, where open land lies to windward)
								if (Noise(Run / 260.0, static_cast<double>(L), Seed ^ 0x5F3D) < -0.15) continue;
								const double Off = Half + 18.0;
								const double X = P[K].first - DY * Windward * Off, Y = P[K].second + DX * Windward * Off;
								const Spot S = H.At(X, Y);
								if (S.Kind != KOpen && S.Kind != KField) continue;
								if (!H.Free(X, Y, 1.0)) continue;
								const double UTilt = R.Range(-0.03, 0.03);
								H.Plant(Drift, X, Y, std::atan2(DY, DX) + UTilt, 1.0, 1.0, Tint(1.0, 1.0, 1.0));
							}
						}
					}
				}
				// Telegraph poles along the main road, some 45 m apart, on one side.
				if (H.Lanes() > 0)
				{
					const auto& P = H.LaneLine(Longest);
					const double Half = H.LaneHalf(Longest);
					double Since = 30.0;
					for (size_t K = 1; K + 1 < P.size(); ++K)
					{
						Since += std::hypot(P[K].first - P[K - 1].first, P[K].second - P[K - 1].second);
						if (Since < 45.0) continue;
						double DX = P[K + 1].first - P[K - 1].first, DY = P[K + 1].second - P[K - 1].second;
						const double DL = std::max(1e-6, std::hypot(DX, DY));
						DX /= DL;
						DY /= DL;
						const double Off = Half + 1.2 + 3.2, ULean = R.Range(-0.03, 0.03);
						const double X = P[K].first + DY * Off, Y = P[K].second - DX * Off;
						if (!H.Free(X, Y, 0.5)) continue;
						H.Plant(Pole, X, Y, std::atan2(DY, DX) + 0.5 * Pi + ULean, 1.0, 1.0, Tint(1.0, 1.0, 1.0));
						Since = 0.0;
					}
				}
				// Orchards round the western villages (apples and plums in the gardens), a lone oak or ash in some pastures.
				const int Tries = static_cast<int>(W * D / 10000.0 * 6.0);
				for (int K = 0; K < Tries; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B), Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Next();
					const Spot S = H.At(X, Y);
					if (S.Kind == KVillage && !bEast && Keep < 0.7)
					{
						if (!H.Free(X, Y, 2.5)) continue;
						const double High = 4.0 + 2.5 * Size;
						H.Plant(LandField::Base::Apple, X, Y, Yaw, High / 5.5 * 1.05, High / 5.5, Tint(1.0, 1.0, 1.0));
					}
					else if (S.Kind == KOpen && S.Edge > 12.0 && Keep < 0.02)
					{
						if (!H.Free(X, Y, 2.0)) continue;
						const LandField::Base T = bEast ? Birch : Size < 0.6 ? LandField::Base::Oak : LandField::Base::Ash;
						const double High = 13.0 + 6.0 * Size;
						const double Built = T == Birch ? 15.0 : 15.0;
						H.Plant(T, X, Y, Yaw, High / Built * 1.1, High / Built, Tint(1.0, 1.0, 1.0));
					}
				}
			}

			int Fx(const std::string& Ground, int Else) const override
			{
				// (the wet months' ploughland and tracks: mud)
				if ((bBare || Month == 10 || Month == 4) && (Ground == "field" || Ground == "road")) return 4;
				return Else;
			}
			const char* Earth() const override { return "loam"; }

		private:
			/**
			 * One pasture's wire fence: from a place inside it (OX, OY), its middle found from its own frame; then each of
			 * its four sides (out along its grain and across it) found where its key ends, every 7 m along, and a line
			 * fitted through them (the places off it dropped: a corner, a lane's bite); posts every 3.6 m along that line,
			 * half a metre inside, each wired to the next. None across a recorded way or a lane; a side whose neighbour is
			 * a fenced pasture with the lower key left to that one.
			 */
			/** An orchard over one pasture: apple trees in rows 9 m apart along its grain, 5 m in from its edge, some gaps. */
			void Orchard(Host& H, uint64_t Key, double OX, double OY) const
			{
				const Spot S0 = H.At(OX, OY);
				const double Ux = std::cos(S0.Angle), Uy = std::sin(S0.Angle);
				const double Step = 8.5 + 1.5 * Unit(Hash(Key, 0x0C4B));
				// (the rows' origin on the parcel's own frame, so its rows run true to its sides)
				const double Ra = S0.Along - Step * std::floor(S0.Along / Step), Rc = S0.Across - Step * std::floor(S0.Across / Step);
				const double CX = OX - (Ra * Ux - Rc * Uy), CY = OY - (Ra * Uy + Rc * Ux);
				for (int J = -45; J <= 45; ++J)
				{
					int Miss = 0;
					for (int I = -45; I <= 45; ++I)
					{
						const double A = I * Step, Cr = J * Step;
						const double X = CX + A * Ux - Cr * Uy, Y = CY + A * Uy + Cr * Ux;
						const Spot S = H.At(X, Y);
						if (S.Key != Key || S.Kind != KOpen)
						{
							// (well outside it along this row: on to the next)
							if (I > 0 && ++Miss > 6) break;
							continue;
						}
						if (S.Edge < 5.0) continue;
						const uint64_t Ht = Hash(Key ^ 0xA991E, static_cast<uint64_t>((J + 64) * 128 + I + 64));
						const double UGap = Unit(Hash(Ht, 1)), USize = Unit(Hash(Ht, 2)), UYaw = 2.0 * Pi * Unit(Hash(Ht, 3));
						const double UJx = Unit(Hash(Ht, 4)) - 0.5, UJy = Unit(Hash(Ht, 5)) - 0.5;
						if (UGap < 0.1) continue; // (a tree gone)
						const double PX = X + 0.6 * UJx, PY = Y + 0.6 * UJy;
						if (!H.Free(PX, PY, 2.5)) continue;
						const double High = 4.2 + 2.3 * USize;
						H.Plant(LandField::Base::Apple, PX, PY, UYaw, High / 5.5 * 1.05, High / 5.5, Tint(0.98 + 0.04 * USize, 1.0, 0.98));
					}
				}
			}

			template <typename FencedFn>
			void Fence(Host& H, uint64_t Key, int Kind, double OX, double OY, bool bHedge, const FencedFn& Fenced) const
			{
				const LandField::Base Post = LandField::Base::Kit10;
				auto In = [&H, Key, Kind](double X, double Y) {
					const Spot S = H.At(X, Y);
					return S.Key == Key && S.Kind == Kind;
				};
				const Spot S0 = H.At(OX, OY);
				const double Ux = std::cos(S0.Angle), Uy = std::sin(S0.Angle);
				double CX = OX - (S0.Along * Ux - S0.Across * Uy), CY = OY - (S0.Along * Uy + S0.Across * Ux);
				if (!In(CX, CY))
				{
					CX = OX;
					CY = OY;
				}
				for (int Side = 0; Side < 4; ++Side)
				{
					// (N out through the side, T along it)
					const double Nx = Side == 0 ? Ux : Side == 1 ? -Ux : Side == 2 ? -Uy : Uy;
					const double Ny = Side == 0 ? Uy : Side == 1 ? -Uy : Side == 2 ? Ux : -Ux;
					const double Tx = -Ny, Ty = Nx;
					std::vector<std::pair<double, double>> Pts; // (along T, out along N)
					double First = 0.0;
					for (int Way = -1; Way <= 1; Way += 2)
					{
						double Guess = First;
						for (int K = Way < 0 ? 0 : 1; K < 50; ++K)
						{
							const double T = Way * K * 7.0, PX = CX + Tx * T, PY = CY + Ty * T;
							if (!In(PX, PY)) break;
							double Lo = std::max(0.0, Guess - 5.0);
							if (Lo > 0.0 && !In(PX + Nx * Lo, PY + Ny * Lo)) Lo = 0.0;
							double Hi = Lo + 6.0;
							int Steps = 0;
							while (Steps < 70 && In(PX + Nx * Hi, PY + Ny * Hi))
							{
								Lo = Hi;
								Hi += 6.0;
								++Steps;
							}
							if (Steps >= 70) break;
							for (int Q = 0; Q < 4; ++Q)
							{
								const double Mid = 0.5 * (Lo + Hi);
								if (In(PX + Nx * Mid, PY + Ny * Mid)) Lo = Mid;
								else Hi = Mid;
							}
							Guess = 0.5 * (Lo + Hi);
							if (K == 0) First = Guess;
							Pts.push_back({ T, Guess });
						}
					}
					// the line through them, the places more than a metre off it dropped and fitted again
					double A = 0.0, Bs = 0.0;
					std::vector<char> Keep(Pts.size(), 1);
					size_t Kept = 0;
					for (int Pass = 0; Pass < 3; ++Pass)
					{
						double N = 0.0, St = 0.0, Sd = 0.0, Stt = 0.0, Std = 0.0;
						for (size_t K = 0; K < Pts.size(); ++K)
							if (Keep[K])
							{
								N += 1.0;
								St += Pts[K].first;
								Sd += Pts[K].second;
								Stt += Pts[K].first * Pts[K].first;
								Std += Pts[K].first * Pts[K].second;
							}
						if (N < 4.0) break;
						const double Den = N * Stt - St * St;
						Bs = std::fabs(Den) > 1e-9 ? (N * Std - St * Sd) / Den : 0.0;
						A = (Sd - Bs * St) / N;
						Kept = 0;
						for (size_t K = 0; K < Pts.size(); ++K)
						{
							Keep[K] = std::fabs(Pts[K].second - (A + Bs * Pts[K].first)) < (Pass == 0 ? 2.5 : 1.0);
							Kept += Keep[K] ? 1 : 0;
						}
					}
					if (Kept < 4) continue;
					double T0 = 1e9, T1 = -1e9;
					for (size_t K = 0; K < Pts.size(); ++K)
						if (Keep[K])
						{
							T0 = std::min(T0, Pts[K].first);
							T1 = std::max(T1, Pts[K].first);
						}
					T0 -= 4.0;
					T1 += 4.0;
					// (the neighbour over the side's middle: a fenced pasture with the lower key fences it)
					{
						const double TM = 0.5 * (T0 + T1), DM = A + Bs * TM + 1.6;
						const Spot NS = H.At(CX + Tx * TM + Nx * DM, CY + Ty * TM + Ny * DM);
						if (NS.Key != Key && NS.Key < Key && Fenced(NS.Key, NS.Kind, NS.Crop) != 0) continue;
					}
					if (bHedge)
					{
						// a hedge: hawthorn, blackthorn and hazel some 1.9 m apart a metre in, a gap here and there; an oak or an
						// ash standing out of it every 25 to 40 m
						const double Slope = std::sqrt(1.0 + Bs * Bs), DH = 1.9 / Slope;
						double NextStd = T0 + (6.0 + 20.0 * Unit(Hash(Key, static_cast<uint64_t>(Side) + 0x57D0))) / Slope;
						int I = 0;
						for (double T = T0 + 0.5 * DH; T <= T1; T += DH, ++I)
						{
							const uint64_t Hh = Hash(Key ^ 0x4ED6E, static_cast<uint64_t>(Side * 4096 + I));
							const double UJit = Unit(Hash(Hh, 1)) - 0.5, UKind = Unit(Hash(Hh, 2)), USize = Unit(Hash(Hh, 3));
							const double UYaw = 2.0 * Pi * Unit(Hash(Hh, 4)), UGap = Unit(Hash(Hh, 5));
							const double D = A + Bs * T - 0.9 + 0.35 * UJit, X = CX + Tx * T + Nx * D, Y = CY + Ty * T + Ny * D;
							if (!In(X, Y)) continue;
							if (T >= NextStd)
							{
								NextStd = T + (25.0 + 15.0 * UGap) / Slope;
								if (H.Free(X, Y, 1.6))
								{
									const LandField::Base Std = UKind < 0.65 ? LandField::Base::Oak : LandField::Base::Ash;
									const double High = 12.0 + 6.0 * USize;
									H.Plant(Std, X, Y, UYaw, High / 15.0 * 1.05, High / 15.0, Tint(0.96 + 0.06 * USize, 0.98, 0.96));
									continue;
								}
							}
							if (UGap < 0.07 || !H.Free(X, Y, 0.7)) continue;
							const LandField::Base K = UKind < 0.55 ? LandField::Base::ShrubB : LandField::Base::ShrubA;
							const double High = (2.0 + 1.3 * USize) / 2.8;
							H.Plant(K, X, Y, UYaw, 0.9 + 0.35 * USize, High, Tint(0.92 + 0.08 * USize, 0.95 + 0.06 * USize, 0.94));
						}
						continue;
					}
					const double Slope = std::sqrt(1.0 + Bs * Bs), DT = PostStep / Slope;
					const double DirX = (Tx + Nx * Bs) / Slope, DirY = (Ty + Ny * Bs) / Slope;
					const double Heading = std::atan2(DirY, DirX);
					std::vector<std::pair<double, double>> Posts;
					std::vector<char> Ok;
					for (double T = T0; T <= T1; T += DT)
					{
						const double D = A + Bs * T - 0.5, X = CX + Tx * T + Nx * D, Y = CY + Ty * T + Ny * D;
						Posts.push_back({ X, Y });
						Ok.push_back(In(X, Y) && H.Free(X, Y, 0.3, true) ? 1 : 0); // (a man steps through a wire fence; a tank's way through it is open)
					}
					for (size_t K = 0; K < Posts.size(); ++K)
					{
						if (!Ok[K]) continue;
						const bool bNext = K + 1 < Posts.size() && Ok[K + 1], bBack = K > 0 && Ok[K - 1];
						if (!bNext && !bBack) continue;
						// (wired on to the next; the last in a run wired back to the one before)
						const double Lean = 0.04 * (Unit(Hash(Key, static_cast<uint64_t>(Side * 1000) + K)) - 0.5);
						H.Plant(Post, Posts[K].first, Posts[K].second, (bNext ? Heading : Heading + Pi) + Lean, 1.0, 1.0, Tint(1.0, 1.0, 1.0));
					}
				}
			}

			const LandField::Field& F() const { return Fd; }
			int Month = 7;
			bool bEast = false, bRussia = false, bArdennes = false, bSlate = true, bBare = false;
			/** The winter spruce on trial (Field::Tried("spruce")): from above a dark mass flecked with snow (KitLook). */
			bool bSpruceTrial = false;
			/** The growing months' grassland on trial "farmgrass" (FieldGrass): the bocage's sward and tussocks, in clumps. */
			bool bGrassTrial = false;
			/** The east's summer on trial "eastgold" (LandScape::EastGold): its ripe corn and stubble a step darker
			 * (CropTone), for its own grade. */
			bool bGold = false;
			/**
			 * The spruce wood with depth on trial (Field::Tried("sprucedepth"), which takes in "spruce"): a deep, ragged wood,
			 * not a comb of cut-outs of one height at one rhythm. Its trees of their stand's age and each its own height
			 * besides, a leader here and there standing proud of its stand, a young one here and there along the edge, a gap
			 * where one fell, the rows shaken loose; the trees darker deeper in (their blue down most, so no bluer), their
			 * snow dimmer, so the eye reads metres of wood behind the first row (Plants);
			 * and near too the boughs dark green, the snow on one pad in two of those that face up, as the far crowns
			 * already are under "spruce" (KitLook).
			 */
			bool bDepthTrial = false;
			double SnowLying = 0.0, Conifers = 0.3, Ca = 1.0, Sa = 0.0;
			Tone Soil = Rgb(126, 102, 78), GrassT = Rgb(92, 134, 54);
			/** The breakup's grid (2 a point: Lt, Wm), 10 m. */
			static constexpr double GStep = 10.0;
			double GX0 = 0.0, GY0 = 0.0;
			int GNX = 0, GNY = 0;
			std::vector<float> Grid;
		};
	}

	std::unique_ptr<Region> MakeFarm(const LandField::Field& F, uint64_t S) { return std::make_unique<Farm>(F, S); }
}
