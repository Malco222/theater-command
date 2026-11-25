#pragma once

#include "Battle/LandField.h"
#include "Battle/LandHouse.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * The look of the land by place and season, for every ground battle that is not Normandy's bocage or Prokhorovka's
 * steppe (Brian, 22 November 2026, 14:19-14:20Z: "there will be ground battles in deserts and other areas outside of
 * Normandy so let's not only focus on that", "I want the ground battle pass but be great for all battles", "makes it very
 * well rounded graphics for all environments and have it look as good as Normandy").
 *
 * One battlefield, many backlots: LandField lays every ground the same way (the rules' cells, the lanes, the parcels, the
 * tiles, the far ring, the clutter round the eye), and asks a Region at each step where a place looks different. The
 * bocage (Normandy, LandField's own) and the steppe (Prokhorovka, LandSteppe, laid by the recording) ask none. The rest:
 *
 *   Desert   North Africa and the Middle East: stony plain and sand, wadis, escarpments, camel thorn, flat-roofed villages
 *   Med      Italy, Sicily, Greece, the Mediterranean shore: terraced hills, stone walls, olives, vines, cypress, stone farms
 *   Jungle   Burma, Malaya, New Guinea, the Solomons: canopy, undergrowth, kunai grass, red earth tracks, stilt villages
 *   Island   the Pacific's beaches and atolls: coral sand, palm groves, the reef's edge (the Jungle's file, its shore)
 *   Farmland the open fields of central and eastern Europe (the steppe's country where no recording lays it)
 *   Dry      the dry steppe east of the Volga: Mongolia and Khalkhin Gol, Kazakhstan, Iran, the Horn of Africa: short dry
 *            grass, sand, no farms, yurts
 *   Rice     central and southern China, Korea, Japan, the Ryukyus: paddies by the season behind their bunds, terraces,
 *            tile-roofed villages facing south, bamboo, pine hills, grave mounds (release 2, on trial "rice")
 *   America  the farm country of North America (Iowa, Kansas, Ohio, Pennsylvania): fields on the survey's square mile,
 *            corn, wheat, oats and hay, red barns and silos, wire and rail fences, a main street and its grain elevator
 *            (release 2, on trial "america")
 *   Plain    the dry plains of north China, Manchuria and India: the loess, gaoliang, millet, wheat and soybeans in their
 *            strips, poplar-lined cart tracks sunk in the earth, flat-roofed villages in rows, grave mounds, the towns'
 *            rammed-earth walls; India's monsoon crops and its winter wheat and mustard (release 2, on trial "plain")
 *   City     a town or city the rules lay with streets (any theatre the climate leaves temperate or dry): its blocks of
 *            terraces holed, gutted, burnt to a storey or two, rubble heaps in the gaps and the streets, a church or a
 *            hall standing broken over them, the suburbs' villas and the works' sheds and chimneys (release 2, on
 *            trial "city")
 *
 * and Snow over any of them (the Ardennes, Moscow, Stalingrad, Karelia, Cassino's mountains).
 *
 * The look is picked from the place on the earth with the art director's climate table (the campaign map's own, so a
 * battle is fought on the ground the map shows: Brian, 18 November, "make sure the battle terrian matches the map"), the
 * rules' terrain and the month; a recording may name it outright (field.look, field.snow). Plain C++, no engine, so the
 * cloud test lays exactly what the game draws. Every colour is the art director's to check (sRGB as the eye sees it; the
 * battle camera's grade greys the middle tones, so they are painted through it: memory ground-grade-colours).
 */
namespace LandScape
{
	enum class Look : uint8_t
	{
		Bocage,   // LandField's own (Normandy): no region
		Steppe,   // LandSteppe, where the recording lays it: no region
		Desert,
		Med,
		Jungle,
		Island,
		Farmland,
		Dry,      // LandDry: the dry steppe (Khalkhin Gol, 1939)
		Rice,     // LandAsia: rice country (China, Korea, Japan; release 2, on trial "rice")
		America,  // LandAmerica: North America's farm country (release 2, on trial "america")
		Plain,    // LandPlain: the dry plains of north China, Manchuria and India (release 2, on trial "plain")
		City,     // LandCity: a bombed and shelled town or city, its streets of burnt-out terraces (release 2, on trial "city")
		Count
	};
	const char* NameOf(Look L);
	/** A look from its word ("desert", "med", "jungle", ...); Count where the word is none of them. */
	Look LookOf(const std::string& Word);

	/** The art director's climate look at a place (world-climate-boxes.json, later boxes over earlier): "temperate",
	 * "desert", "drysouth", "jungle", "monsoon", "steppe", "prairie", "savanna", "dryplain", "taiga", "tundra", "alpine",
	 * "ice". */
	const char* ClimateAt(double Lat, double Lon);

	/** The look of a field: its own word where the recording names one, the steppe where it lays one, else the place's. */
	Look Pick(const LandField::Field& F);
	/** Snow lying, 0 (none) to 1 (deep and everywhere): the field's own where it gives one (0 or more), else by the place,
	 * the look and the month (the Ardennes and the Eifel in December and January, Russia from November to March, ...). */
	double SnowOf(const LandField::Field& F, Look L);
	/** The broadleaf trees and the hedges' shrubs bare with no snow lying, by the field's month and latitude (December to
	 * February wherever they grow, November and March from 44 degrees, April from 52; the south half a year on). Never
	 * for a region that plants its own trees by the season (the Mediterranean's holm oaks keep their leaves) or grows none. */
	bool LeaflessOf(const LandField::Field& F, Look L);

	/** The day's air over a look: the haze's colour and thickness, the sky's dust, the camera's grade (LandBattle's four
	 * numbers: darks' and middles' colour kept, red, blue), used where the recording gives none. */
	struct Air
	{
		bool bSet = false;
		double HazeInk[3] = { 0.62, 0.66, 0.70 };   // linear, on a fair day (the bocage's)
		double HazeOvercast[3] = { 0.50, 0.52, 0.54 };
		double HazeDensity = 0.0069;                 // at the field's ground (LandBattle's HazeDensity)
		double SkyDust = 0.6;                        // LandBattle's SkyDust
		double Grade[4] = { 0.45, 0.48, 1.0, 1.02 }; // the colour kept in the darks and the middles; red and blue gain
		double Stops = -0.5;                         // exposure (LandBattle's half stop down; the desert's half a stop more on trial "desertstop", an island's a third on "islandstop")
		double Contrast = 1.42;                      // BattleLook's
		double Toe = 0.55;                           // the film's toe (Unreal's own)
		double KeyKelvin = 0.0;                      // 0: the battle's own
		double Cover = -1.0;                         // the cloud's usual share (the desert's clear sky), for the cloud painter where
		                                             // the recording gives its own; the game keeps the battle's weather draw
		double SkyLift = 1.0;                        // the sky's own pixels under a high sun (LandBattle's SkyLift; 1: as drawn)
	};
	Air AirOf(Look L, double Snow);
	/**
	 * The east's summer graded as its own theatre (release 2, on trial "eastgold" until the art director passes it on the
	 * Mac): the farmland of the Eastern Front east of 23.5 degrees (LandFarm's Russia: the Ukraine, Belarus, western
	 * Russia to the Urals, 44 to 62 north; Voronezh, Kursk, Kharkov, Orel; not the northern forest belt, the climate's
	 * taiga: Leningrad, the Svir, Karelia) from May to September with no snow lying (0.3 or less). Its air is the art
	 * director's T2, warm gold where Normandy's grade washed it to cream, and LandFarm lays its ripe corn and stubble a
	 * step darker for it. Trial: the field's words (Field::Tried's reading of them).
	 */
	bool EastGold(Look L, double Snow, double Lat, double Lon, int Month, const std::string& Trial);
	/** The day's air as above, for the field's place, month and trials (the recording's sky.latLon, its date's month and
	 * "-TheaterLandTrial=", as the field was laid from them): the east's summer its own where EastGold holds. */
	Air AirOf(Look L, double Snow, double Lat, double Lon, int Month, const std::string& Trial);

	// ------------------------------------------------------------------ numbers (a region's own noise; LandField keeps its own)

	inline uint64_t Mix(uint64_t H)
	{
		H ^= H >> 33;
		H *= 0xFF51AFD7ED558CCDull;
		H ^= H >> 33;
		H *= 0xC4CEB9FE1A85EC53ull;
		H ^= H >> 33;
		return H;
	}
	inline uint64_t Hash(uint64_t A, uint64_t B) { return Mix(A * 0x9E3779B97F4A7C15ull ^ Mix(B + 0x632BE59BD9B4E019ull)); }
	inline uint64_t Hash(uint64_t A, uint64_t B, uint64_t C) { return Hash(Hash(A, B), C); }
	inline double Unit(uint64_t H) { return static_cast<double>(H >> 11) / 9007199254740992.0; }
	inline double Clamp01(double X) { return X < 0.0 ? 0.0 : X > 1.0 ? 1.0 : X; }
	inline double Smooth(double E0, double E1, double X)
	{
		const double T = Clamp01((X - E0) / (E1 - E0));
		return T * T * (3.0 - 2.0 * T);
	}
	inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	/** Smooth value noise, -1 to 1, its lattice one unit apart. */
	double Noise(double X, double Y, uint64_t Seed);
	/** Fractal noise, about -1 to 1: octaves of Noise, each half the size and weight, the first Scale metres. */
	double Fbm(double X, double Y, double Scale, int Octaves, uint64_t Seed);
	/** A stream of numbers from a seed. Keep each draw in a named local before passing it on: a call's arguments are
	 * taken in no set order, so g++ (the cloud) and clang (the Mac) would draw them differently. */
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
		int Below(int N)
		{
			const int K = static_cast<int>(Next() * N);
			return K < N - 1 ? K : N - 1;
		}
	};

	/** A colour while it is mixed: sRGB 0 to 255, as the art director measures. */
	struct Tone
	{
		double R = 0.0, G = 0.0, B = 0.0;
	};
	inline Tone Rgb(double R, double G, double B) { return Tone{ R, G, B }; }
	inline Tone Mixed(const Tone& A, const Tone& B, double T)
	{
		T = Clamp01(T);
		return Tone{ A.R + (B.R - A.R) * T, A.G + (B.G - A.G) * T, A.B + (B.B - A.B) * T };
	}
	/** Lighter or darker by L, warmer (+) or cooler by W. */
	inline Tone Varied(const Tone& A, double L, double W) { return Tone{ A.R * L * (1.0 + W), A.G * L, A.B * L * (1.0 - W) }; }
	inline LandField::Rgb8 Byte(const Tone& C)
	{
		auto B = [](double V) { return static_cast<uint8_t>(V < 0.0 ? 0.0 : V > 255.0 ? 255.0 : std::lround(V)); };
		LandField::Rgb8 Out;
		Out.R = B(C.R);
		Out.G = B(C.G);
		Out.B = B(C.B);
		return Out;
	}
	/** A per-instance tint: 128 a channel is as built (1.99 at most). */
	inline LandField::Rgb8 Tint(double R, double G, double B) { return Byte(Tone{ 128.0 * R, 128.0 * G, 128.0 * B }); }

	// ------------------------------------------------------------------ what a region is asked about

	/** The kinds of ground LandField's region raster holds (its own order: the rules' cells smoothed into parcels). */
	enum Kind : int
	{
		KOpen,    // open land: grazing, waste, the desert's plain
		KField,   // tilled land: crops, groves, vines, paddies
		KWoods,
		KRough,   // rough, rock, scrub
		KWorks,   // fieldworks
		KVillage, // the village's gardens and yards round its houses
		KMarsh,
		KWater,
		KLane,    // a road's floor and its verge
		KYard,
		KCount
	};

	/** A place as LandField sees it, for the region's paint and its clutter. */
	struct Spot
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		int Kind = KOpen;
		/** What is drawn here (LandField::Drawn as a number: Pasture 0, Crop 1, Woods 2, Rough 3, Works 4, Village 5,
		 * Yard 6, Lane 7, Bank 8, Floor 9, Wall 10, Water 11, Marsh 12, Beyond 13); -1 where not asked. */
		int Drawn = -1;
		/** The region's crop of the parcel here (its own number, from CropOf); -1 none. */
		int Crop = -1;
		/** The parcel: a stable key, its size (square metres), its grain (radians from east), and the place in its frame
		 * (Along its grain, Across it, metres from its middle). */
		uint64_t Key = 0;
		double Area = 0.0, Angle = 0.0, Along = 0.0, Across = 0.0;
		/** Metres in from the parcel's edge; metres outside the nearest lane's floor (negative on it) and its half width. */
		double Edge = 99.0, Lane = 99.0, LaneHalf = 2.2;
		/** LandField's noises at the place: N1 patches (-1..1, some 30 m), N2 mottle (some 7 m), Broad (some 320 m). */
		double N1 = 0.0, N2 = 0.0, Broad = 0.0;
		/** Within a metre or two of a building; a field's gateway; a man's or a vehicle's way recorded here. */
		bool bNear = false, bGate = false, bMan = false, bVehicle = false;
		/** Inside the laid field and its band (else beyond, in the ring). */
		bool bLaid = true;
	};

	/** One clutter thing's choice: its kind (a LandField::Base from Tuft on; Count for nothing) and the things a square
	 * metre where it grows (two at most). */
	struct Grown
	{
		LandField::Base Kind = LandField::Base::Count;
		double Density = 0.0;
	};

	/** What a region may ask of the ground being laid, and what it may lay on it. */
	class Host
	{
	public:
		virtual ~Host() = default;
		/** The field's size (metres) and the band laid in full round it. */
		virtual double Width() const = 0;
		virtual double Depth() const = 0;
		virtual double Band() const = 0;
		/** The rules' town: its middle and reach; false where there is none. */
		virtual bool Town(double& OutX, double& OutY, double& OutR) const = 0;
		/** The land's height (no lanes or banks) and the surface drawn. */
		virtual double Land(double X, double Y) const = 0;
		virtual double Top(double X, double Y) const = 0;
		/** What is here (kind, crop, parcel, edges, ways; Z the surface; no noise). */
		virtual Spot At(double X, double Y) const = 0;
		/** Whether a thing of Room metres' radius may stand here: in the laid field, off the houses, the lanes and the
		 * recorded ways (a vehicle's always; a man's too unless bManMay). */
		virtual bool Free(double X, double Y, double Room, bool bManMay = false) const = 0;
		/** Plant one of a base (a tree, a kit model, a rock): its foot on the surface drawn. Off a recorded way, else not. */
		virtual void Plant(LandField::Base B, double X, double Y, double Yaw, double Scale, double ScaleZ, LandField::Rgb8 Tint) = 0;
		/** A building from the region's kit (B one of Kit0..Kit19): its floor levelled, its footprint (Long along Yaw,
		 * Deep across) kept clear and stood on, planted. False where it does not fit. */
		virtual bool House(LandField::Base B, double X, double Y, double Yaw, double Long, double Deep) = 0;
		/** A building of a street's terrace (the city's: B one of the kit's slots), as House but wall to wall with its
		 * neighbours: no gap kept round it, only its own footprint stood on and kept clear. False where it does not fit.
		 * (A host without terraces lays it as a House.) */
		virtual bool Row(LandField::Base B, double X, double Y, double Yaw, double Long, double Deep) { return House(B, X, Y, Yaw, Long, Deep); }
		/** A building of the ground's own builder (the bocage's stone houses: walls, roof, door and windows; the
		 * steppe's whitewashed ones where bWhite), for a region with no kit house of its own (the farmland of France,
		 * Belgium, Germany, the Ardennes): Type 0 house, 1 farmhouse, 2 barn, 3 byre; Storeys 1 or 2; Roof 0 slate, 1 tile;
		 * Hurt 0 whole, 1 a hole in its roof, 2 a gable down, 3 shelled. Its floor levelled, kept clear. False where it
		 * does not fit. */
		virtual bool Building(double X, double Y, double Yaw, double Long, double Deep, int Type, int Storeys, int Roof, bool bWhite, int Hurt) = 0;
		/** The village church of the ground's own builder (the bocage's: nave, chancel, the tower at its west end, a walled
		 * churchyard some 43 by 21 m), its middle at X, Y, its nave along Yaw (the tower on the -Yaw side); Hurt 0 whole,
		 * 1 a hole in its roof. One a field; false where it does not fit or one stands already. */
		virtual bool Church(double X, double Y, double Yaw, int Hurt) = 0;
		/** A dry-stone wall along a line (points about 2 m apart or fewer), High and Thick metres, built into the tiles'
		 * built mesh (as the bocage's yard walls are): men stand behind it; vehicles' ways cut it. */
		virtual void Wall(const std::vector<std::pair<double, double>>& Line, double High, double Thick) = 0;
		/** Keep a disc clear of trees and shrubs (a ruin's ash, a well's yard). */
		virtual void Clear(double X, double Y, double R) = 0;
		/** The lanes' middle lines (points about 2 m apart) and each one's half width. */
		virtual size_t Lanes() const = 0;
		virtual const std::vector<std::pair<double, double>>& LaneLine(size_t K) const = 0;
		virtual double LaneHalf(size_t K) const = 0;
		/** The field's own seed for the region (the same ground every time for the same field). */
		virtual uint64_t Seed() const = 0;
		/** Count a house (with Damaged when it is hurt) and a farmstead for the tally. */
		virtual void Tally(int Houses, int Damaged, int Farmsteads) = 0;
	};

	/**
	 * A ground's look: what LandField asks where a place is drawn differently from the bocage. Every step has a plain
	 * answer here; a region overrides what it changes.
	 */
	class Region
	{
	public:
		Region(const LandField::Field& F, uint64_t InSeed) : Fd(F), Seed(InSeed) {}
		virtual ~Region() = default;
		virtual Look Which() const = 0;

		// ---- the land
		/** The land inside the field and round it, after the rules' heights and the small undulation (Z): dunes, wadis,
		 * terraces, a beach's fall to the water. Keep what the rules lay within a metre or so inside the field (the fight
		 * reads its own heights). */
		virtual double Relief(double /*X*/, double /*Y*/, double Z) const { return Z; }
		/** The land beyond the field (kilometres out under the haze), about MeanZ. */
		virtual double Rolling(double X, double Y, double MeanZ) const;
		/** How deep a road's floor runs below the land, and its half width (a village's wider). */
		virtual double LaneDepth(double /*X*/, double /*Y*/) const { return 0.12; }
		virtual double LaneHalf(bool bVillage, double U) const { return (bVillage ? 3.2 : 2.6) + 0.8 * U; }

		// ---- the parcels
		/** Whether the bocage's hedgerows are laid (no region yet wants them). */
		virtual bool Hedges() const { return false; }
		/** A plot is cut while larger than a size between these (square metres). */
		virtual double PlotMin() const { return 32000.0; }
		virtual double PlotMax() const { return 60000.0; }
		/** Beyond the field's reach, the share of plots tilled (the rest open). */
		virtual double FarTilled() const { return 0.36; }
		/** Woods beyond the band where the country's slow noise passes this (-0.5 everywhere .. 0.6 nowhere). */
		virtual double WoodsEdge() const { return 0.27; }
		/** Whether the far ring carries the bocage's tree-lined field edges for its first kilometre (dark ridges with
		 * crowns standing up from them, the layers to the horizon): France 1940's poplar rows might; the desert not. */
		virtual bool FarRows() const { return false; }
		/** Whether the land is laid out by a survey's grid (North America's townships): the plots' blocks square to the
		 * compass, half a mile on a side, unwarped, cut square at their halves into fields of 80, 40 and 20 acres (LandField
		 * Plot); PlotMin and PlotMax still say where the cutting stops. */
		virtual bool Surveyed() const { return false; }
		/** A parcel's crop (the region's own number, 0 to 63) for its kind (KOpen, KField, KRough) and a number U. */
		virtual int CropOf(int Kind, double U) const = 0;
		/** Its word for sheets and tests. */
		virtual const char* CropWord(int Crop) const = 0;
		/** Whether a crop is standing corn (a tank lays it flat between its tracks). */
		virtual bool IsCorn(int Crop) const { return Crop < 0; }

		// ---- what stands on it
		/** The village or the farms round the rules' town (houses from the kit, walls, yards, wells). */
		virtual void Sites(Host& H) { (void)H; }
		/** After the parcels: walls along their edges, terraces' walls. */
		virtual void Walls(Host& H) { (void)H; }
		/** The stone of the walls it lays (Host::Wall) and how much ivy and moss greens their faces (0 none, the bocage's
		 * 0.8). */
		virtual Tone WallStone() const { return Rgb(168, 160, 142); }
		virtual double WallIvy() const { return 0.3; }
		/** Its walls broken along their tops (each stone's top its own, one length in five with its coping fallen) and each
		 * stone its own tone, not a cast panel: Italy's on trial ("walltop"). */
		virtual bool WallBroken() const { return false; }
		/** Woods, trees, scrub, rocks and the kit's things in the open. */
		virtual void Plants(Host& H) { (void)H; }

		// ---- colours (sRGB as the eye sees them)
		virtual Tone Colour(const Spot& S) const = 0;
		/** Out in the ring: the parcel's crop as seen from far off; woods as canopy; Gap the metres to its edge. */
		virtual Tone Far(const Spot& S, bool bWoods, double Gap) const = 0;

		// ---- the clutter round the eye
		virtual Grown Grow(const Spot& S, double Pick) const = 0;
		/** One clutter thing's size and tint (as built: Scale 0.8 to 1.2, Tint 128); U4..U7 its own numbers 0..1. Return
		 * false to keep LandField's own dressing for the kind (the bocage's grasses and stones). */
		virtual bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const
		{
			(void)P; (void)S; (void)U4; (void)U5; (void)U6; (void)U7;
			return false;
		}

		// ---- the kit: Kit0..Kit19 (LandField::KitSlots) from a region's battle models, Clut0..Clut5 its clutter
		/** The kit's folder: Content/Models/<Folder> in the game, battle-models/<folder> in the shared folder. */
		virtual const char* Folder() const { return ""; }
		/** The model file (without .json) drawn for a kit slot (0..19), or null where the slot is unused. */
		virtual const char* KitName(int Slot) const { (void)Slot; return nullptr; }
		/** A kit slot after it is read (or plainly drawn where its file is missing): its far level's distance, its shape for
		 * the men's and the camera's sight (BaseMesh::Sight*). */
		virtual void KitLook(int Slot, LandField::BaseMesh& M) const { (void)Slot; (void)M; }
		/** A plain stand-in for a kit slot whose file is not given. */
		virtual void KitPlain(int Slot, LandField::BaseMesh& M) const;
		/** A kit slot the region builds itself in KitPlain (a yurt, a cairn), with no file to read or to miss. */
		virtual bool KitBuilt(int Slot) const { (void)Slot; return false; }
		/** A kit slot whose model lies low, LandField::LowKitHigh or less as planted (a balk's ridge, a bund: a man steps
		 * over it, a tank rolls over it): it may lie across a recorded men's way, never a vehicles'. One that stands
		 * taller after all (its model read or built higher) is taken up off the men's ways once the models are built. */
		virtual bool KitLow(int Slot) const { (void)Slot; return false; }
		/** The trial "houses" (release 2): a kit slot whose houses LandHouse builds to the vehicles' detail instead of its
		 * model (Out its kind and roof; LandField gives each house's footprint and look): false where the slot keeps its
		 * model. */
		virtual bool KitHouse(int Slot, LandHouse::Spec& Out) const { (void)Slot; (void)Out; return false; }
		/** How many steps down the frame budget's ladder of nearer far-draw distances the game takes for this ground (0,
		 * GS9's own, to 4; production/theatre-budgets-1122/budgets.md section B): its dense woods' trees and shrubs turn
		 * plain, to crowns and out sooner. By its look unless the region says. */
		virtual int DrawStep() const;
		/** A clutter slot's mesh (Clut0..Clut5), built here. */
		virtual void ClutterMesh(int Slot, LandField::BaseMesh& M) const { (void)Slot; (void)M; }
		/** A clutter base of the frame's own (Tuft to Rosette) that this region never lays, made one of the region's here
		 * (its mesh as ClutterMesh makes them; its things laid by Dress setting Placed::Kind to it): true where it is. The
		 * jungle's plumed kunai on trial in Poppies' slot. */
		virtual bool OwnClutter(LandField::Base B, LandField::BaseMesh& M) const { (void)B; (void)M; return false; }

		// ---- the effects
		/** The effects kit's ground word for a rules ground name (its number: 0 earth, 1 town, 2 woods, 3 water, 4 mud,
		 * 5 sand, 6 snow, 7 rock), Else LandField's. */
		virtual int Fx(const std::string& Ground, int Else) const { (void)Ground; return Else; }
		/** The shell holes' earth (LandCrater::SetEarth): "loam", "sand", "blackearth", "terrarossa", "laterite", ... */
		virtual const char* Earth() const { return "loam"; }

	protected:
		const LandField::Field& Fd;
		uint64_t Seed = 1;
	};

	/**
	 * Snow lying over any ground (the bocage's included: the Ardennes in December). LandField asks it after the ground's
	 * own colour, of each clutter thing, of each base as built and of the houses' built mesh.
	 */
	class Snow
	{
	public:
		Snow(double InCover, Look L, uint64_t InSeed) : Cover(InCover), Under(L), Seed(InSeed) {}
		virtual ~Snow() = default;
		/** The ground's colour with the snow on it (C the colour without). */
		virtual Tone Over(const Tone& C, const Spot& S) const;
		/** The same far off, out in the ring. */
		virtual Tone OverFar(const Tone& C, const Spot& S, bool bWoods) const;
		/** A clutter thing under the snow: false where it is buried; else its tint and size as the snow leaves it. */
		virtual bool Clutter(LandField::Placed& P, const Spot& S) const;
		/** A base as built (a tree, a shrub, a kit model): bare the broadleaf trees' crowns, snow on what faces up. */
		virtual void Base(LandField::Base B, LandField::BaseMesh& M) const;
		/** The houses', churches' and walls' built mesh: snow on the roofs and the walls' tops. */
		virtual void Built(LandField::Mesh& M) const;
		/** A base bare for the winter with no snow on it (LeaflessOf): the broadleaf trees' and the hedges' leaves gone, as
		 * Base bares them, nothing white. */
		virtual void Leafless(LandField::Base B, LandField::BaseMesh& M) const;

		const double Cover;
		const Look Under;

	protected:
		uint64_t Seed = 1;
	};

	/** The battle models a field's region draws (its kit folder: Content/Models/<Folder> in the game, battle-models/
	 * <folder lower case>/models in the shared folder; each name the file without .json), for the caller to read into
	 * Field::Kit before the ground is laid. False (nothing) for the bocage and the steppe. */
	bool KitWanted(const LandField::Field& F, std::string& OutFolder, std::vector<std::string>& OutNames);

	/** The region for a look (null for the bocage and the steppe), and the snow over it (null where none lies). */
	std::unique_ptr<Region> Make(Look L, const LandField::Field& F, uint64_t Seed);
	std::unique_ptr<Snow> MakeSnow(double Cover, Look L, uint64_t Seed);
	/** The same, the field's trial words read ("wintergrass": the farmland's dry stalks dead and bent as its grass is). */
	std::unique_ptr<Snow> MakeSnow(double Cover, Look L, uint64_t Seed, const LandField::Field& F);

	// Each region's own file makes it (LandDesert.cpp, LandMed.cpp, LandTropic.cpp, LandFarm.cpp, LandDry.cpp, LandAsia.cpp,
	// LandAmerica.cpp, LandPlain.cpp, LandCity.cpp, LandSnow.cpp).
	std::unique_ptr<Region> MakeDesert(const LandField::Field& F, uint64_t Seed);
	std::unique_ptr<Region> MakeMed(const LandField::Field& F, uint64_t Seed);
	std::unique_ptr<Region> MakeTropic(const LandField::Field& F, uint64_t Seed, bool bIsland);
	std::unique_ptr<Region> MakeFarm(const LandField::Field& F, uint64_t Seed);
	std::unique_ptr<Region> MakeDry(const LandField::Field& F, uint64_t Seed);
	std::unique_ptr<Region> MakeAsia(const LandField::Field& F, uint64_t Seed);
	std::unique_ptr<Region> MakeAmerica(const LandField::Field& F, uint64_t Seed);
	std::unique_ptr<Region> MakePlain(const LandField::Field& F, uint64_t Seed);
	std::unique_ptr<Region> MakeCity(const LandField::Field& F, uint64_t Seed);
	/** Whether the rules laid a town or a city here big enough to fight in its streets: any suburb, industry or park
	 * cell, or enough built cells (the city look's test; release 2, on trial "city"). */
	bool IsCityField(const LandField::Field& F);
	std::unique_ptr<Snow> MakeSnowCover(double Cover, Look L, uint64_t Seed, bool bGrassTrial = false);

	/** A small mesh writer for regions' own meshes (clutter, plain kit): points with normals and colours. */
	struct Writer
	{
		LandField::Mesh& M;
		explicit Writer(LandField::Mesh& InM) : M(InM) {}
		uint32_t Vert(double X, double Y, double Z, double NX, double NY, double NZ, const Tone& C);
		void Tri(uint32_t A, uint32_t B, uint32_t C);
		/** A box: its middle, half extents along x, y, z (turned Yaw about z), its colour; FaceDown adds its underside. */
		void Box(double CX, double CY, double CZ, double HX, double HY, double HZ, double Yaw, const Tone& C, bool bUnder = false);
		/** A blade or a leaf: a thin tapered quad from (X0,Y0,Z0) up to (X1,Y1,Z1), Width at its foot, both faces. */
		void Blade(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double Width, const Tone& Foot, const Tone& Tip);
		/** A small round thing (a seed head, a bud): its middle, Rad round and Tall high, Top its colour above and Foot
		 * below; its normals out. */
		void Bead(double X, double Y, double Z, double Rad, double Tall, const Tone& Top, const Tone& Foot);
		/** A lumpy rock: a squashed icosphere-like ball of Radius, Height high, its points pushed by a noise of Seed. */
		void Rock(double CX, double CY, double Radius, double Height, uint64_t RockSeed, const Tone& Lit, const Tone& Under);
	};
}
