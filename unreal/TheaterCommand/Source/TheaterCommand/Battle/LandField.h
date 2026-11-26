#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * The ground of a land battle in 3D, built from the rules' field, in plain C++ so that the cloud test draws exactly
 * what the game lays (Brian, 21 November 2026: ground battles as intense as the air battles, "as if I'm watching
 * Saving Private Ryan", a feeling only; the first is the Normandy dry run, July 1944, Americans with Shermans against
 * a German line with Tigers in the hedgerows before a stone village).
 *
 * The camera often sits at a soldier's eye, 1.6 to 2 m up, with a hedgerow bank, a wall corner or a wreck near in
 * frame, so the ground must hold up from a few metres; and it must run on into haze to a natural horizon with no edge
 * (the art director's field look and grit direction: 2 to 4 km sight on land). What is laid, from the rules' cells:
 * - the height: the 25 m cells' heights smoothed (bicubic), a gentle undulation of a few tenths of a metre, the
 *   ploughed lands of the corn fields, and sunken lanes 0.6 to 1.4 m below the fields along the rules' roads, with
 *   wheel ruts;
 * - the bocage: the open and field land cut into irregular parcels of about 80 to 200 m whose edges follow the rules'
 *   cells where field meets pasture, woods or road, each lined with a hedgerow: an earth bank 1 to 1.5 m high and 2 to
 *   3 m wide at the foot, dense shrubs on it and every so often a tall oak, elm, ash or pollard, a field gate in most
 *   parcels, and the lanes between banks;
 * - woods of mixed broadleaf with undergrowth at their edges;
 * - the village round the rules' town: houses and farmsteads of grey-cream limestone with steep slate or tile roofs,
 *   a church with a tower and spire, yard walls, orchards, a few houses already shell-damaged;
 * - beyond the field, the same bocage for 250 m in full, then farmland, hedges as low ridges with tree crowns on them, and woods as canopy out
 *   to 5 km past the field's edges, under the haze.
 *
 * Everything is in the field's frame: metres, x east, y north, z up, the origin at the field's south-west corner
 * (the rules' row 0 is the south edge; the attack comes from the south). Colours are 8-bit sRGB vertex colours for the
 * game's M_MapSurface (sRGB FColor bytes, made linear in the material); the battle camera's grade desaturates them,
 * so they are painted as the eye sees July, not pre-greyed. Every colour is the art director's to check.
 *
 * How the game uses it: build one Ground from the field (well under two seconds, on any thread); draw the Tiles and
 * the Ring (each tile its Near or Far level by distance, and its Built mesh); draw each base mesh instanced at its
 * Instances (Near close, Far beyond FarFrom); each frame ask Clutter for the squares round the eye; and stand men,
 * tanks, guns and shell bursts on At(), which is the surface drawn, banks and lanes included.
 */
namespace LandField
{
	/** A vertex colour: 8-bit sRGB. */
	struct Rgb8
	{
		uint8_t R = 128, G = 128, B = 128;
	};

	/**
	 * The open steppe (Brian, 22 November 2026: "a battle in an open field so we can see them battle more clearly";
	 * Prokhorovka, July 1943), as the recording's field.steppe lays it over the rules' plains: the collective farms' great
	 * fields of wheat and rye, pasture and fallow, a balka (a dry gully) in from a flank, a railway on its bank, the
	 * state farm's whitewashed cottages. Where it is given (On), the ground is laid as the steppe (Battle/LandSteppe)
	 * and not as the bocage: no hedgerows, no sunken lanes, the east kit's cottages, fences, wells and telegraph poles.
	 */
	struct SteppeLay
	{
		bool bOn = false;
		/** The farms' fields: a point turned by -Turn about (CX, CY) into (U, V) falls in a column of Xs and a row of Ys
		 * moved north by the column's Shift (so the fields of one column do not line up with the next's, as the farms' do
		 * not; none given, none); Patch is each field's crop, row by row (Ys), Xs.size() - 1 to a row: 1 wheat, 2 rye,
		 * 3 pasture, 4 fallow. */
		double Turn = 0.0, CX = 0.0, CY = 0.0;
		std::vector<double> Xs, Ys, Shift;
		std::vector<uint8_t> Patch;
		/** Each rules cell's crop (row by row, as Field::Ground): 0 where it is not a crop (a road, the farm, the gully). */
		std::vector<uint8_t> Crop;
		/** The balka's middle line (metres, from its mouth at the field's edge to its head) and how deep its mouth is. */
		std::vector<std::pair<double, double>> Balka;
		double BalkaDeep = 0.0;
		/** The railway's line (south to north, on past the field's ends) and the height of its bank. */
		std::vector<std::pair<double, double>> Rail;
		double RailBank = 0.0;
		/** The rise the defence holds: its top, how far its slopes run, how high (the rules' heights carry it already). */
		double HillX = 0.0, HillY = 0.0, HillR = 0.0, HillRise = 0.0;
		/** The east kit's models (battle-models/east, Content/Models/East: Khata, BurntChimney, TelegraphPole, WattleFence,
		 * WellSweep, Haystack, Birch), by name: each file's text, read by the caller. A model not given is drawn plainly. */
		std::map<std::string, std::string> Kit;
	};

	/** The rules' field, as the recording or the bridge gives it (field.*). */
	struct Field
	{
		int Cols = 0, Rows = 0;
		double Cell = 25.0;
		/** field.grounds: the names the cells index (open, field, woods, rough, marsh, water, road, bridge, buildings ...). */
		std::vector<std::string> Grounds;
		/** field.ground and field.height, row 0 the south edge: index Row * Cols + Col. */
		std::vector<int> Ground;
		std::vector<double> Height;
		/** field.storeys where given (0 where none), else empty. */
		std::vector<int> Storeys;
		/** field.town, where there is one. */
		bool bTown = false;
		double TownX = 0.0, TownY = 0.0, TownR = 0.0;
		/** field.place.terrain (plains, forest, hills, mountain, marsh, urban, desert ...). */
		std::string Terrain = "plains";
		int Month = 7;
		/** The campaign's weather word over the place (LandFeed::Recording::WeatherWord: "clear", "rain", "snow", "mud"),
		 * empty where none. "mud" (the thaw, the autumn rains): open ground's effects word is "mud", not "earth" (the
		 * holes' soil, the tracks' torn ruts, the rounds' thrown mud); snow lying, sand and rock keep their own. */
		std::string Weather;
		uint64_t Seed = 1;
		/** The open steppe, where the recording lays it (Steppe.bOn); else the bocage. */
		SteppeLay Steppe;
		/**
		 * The look of the land where it is neither the bocage nor the steppe (LandScape: the desert, the Mediterranean, the
		 * jungle and the islands, the open farmland, and snow over any of them). Look: the recording's word (field.look:
		 * "bocage", "desert", "med", "jungle", "island", "farmland"), empty to pick it from the place (Lat, Lon: sky.latLon)
		 * and the terrain; Snow: the snow lying, 0 to 1 (field.snow), less than 0 to pick it from the place and the month.
		 * Kit: the region's battle models (Content/Models/<its folder>), each file's text by its name, read by the caller.
		 */
		std::string Look;
		double Lat = 49.0, Lon = 0.0;
		double Snow = -1.0;
		std::map<std::string, std::string> Kit;
		/**
		 * The looks on trial (release 2), each behind its switch until the art director passes it and the Mac holds its
		 * frame rate: words between commas ("houses": the bocage's farmhouses, barns and byres built to the vehicles' detail, LandHouse, kit
		 * models drawn by level in place of their plain built boxes; "church": a church in the farmland's villages; "walltop": Italy's walls
		 * broken along their tops; "spruce": the winter spruce dark from above; "edge": the woods beyond the field held
		 * back from its edge along a wandering line; "kunai": a plume on one kunai clump in six; "wet": the thaw's wet ground where Weather lays the mud; "worn" and "captured": LandMen's uniforms that age and LandKit's captured kit, set by LandBattle; "runs": LandMen's run styles, each man his own runner), or "all". The game's "-TheaterLandTrial=", the cloud test's LAND_FIELD_TRIAL. Empty:
		 * every ground as passed.
		 */
		std::string Trial;
		/** The rivers' water drawn at the rules' own level, sea level less 2 m, as before release 1.5's mend (a deep cut
		 * through hills); else 2 m under its lowest bank near by ("-TheaterLandOldRiverBed"). */
		bool bOldRiverBed = false;
		bool Tried(const std::string& Word) const
		{
			if (Trial == "all") return true;
			for (size_t P = 0; P <= Trial.size();)
			{
				size_t E = Trial.find(',', P);
				if (E == std::string::npos) E = Trial.size();
				if (Trial.compare(P, E - P, Word) == 0) return true;
				P = E + 1;
			}
			return false;
		}

		/**
		 * Where men and vehicles go, if known (a recording's men tracks, each man's kept points): a hedge they cross is
		 * opened there, as wide as HalfWidth either side plus half a metre, and nothing is built or planted on them. Leave
		 * empty for a battle not yet fought.
		 */
		struct Way
		{
			std::vector<std::pair<double, double>> Points;
			double HalfWidth = 1.0;
			/** The recording's track it is (LandFeed::Recording::Tracks' index; -1 where none): for Crossings. */
			int Track = -1;
		};
		std::vector<Way> Ways;

		/**
		 * The hedge cutters' cuts (release 2; Brian, 22 November 14:19Z: bladed Shermans punch a gap through a hedgerow
		 * where the player picks): the gap's middle on the bank's line and its width, bank end to bank end. The hedge is
		 * laid whole there (no way through it opened, no tree in the gap, its shrubs planted as anywhere), so the scene
		 * tears it open when she bursts through (LandCrater::Field::AddBreach). None: the bocage as before.
		 */
		struct Cut
		{
			double X = 0.0, Y = 0.0, Width = 4.0;
		};
		std::vector<Cut> Cuts;

		double Width() const { return Cols * Cell; }
		double Depth() const { return Rows * Cell; }
	};

	/** What is drawn at a place (finer than the rules' cell). */
	enum class Drawn : uint8_t
	{
		Pasture,
		Crop,
		Woods,
		Rough,
		Works,
		Village, // gardens, yards and orchards round the houses
		Yard,    // a farmstead's yard
		Lane,    // a sunken lane's floor and walls
		Bank,    // a hedgerow's bank
		Floor,   // inside a house (its ground floor)
		Wall,    // on a stone wall's line (the ground at its foot)
		Water,
		Marsh,
		Beyond   // past the field's laid bocage: the far farmland
	};

	/** The ground at a place, as drawn. */
	struct Surface
	{
		double Z = 0.0;
		/** The surface's normal (unit, up-facing). */
		double NX = 0.0, NY = 0.0, NZ = 1.0;
		/** The rules' cell under the place: an index into Field::Grounds (the nearest cell beyond the field). */
		int Ground = 0;
		/**
		 * The effects kit's ground word for this place, from the rules' cell as section 7 of the land hand-off asks:
		 * "earth", "town", "woods", "water", "mud", "sand" (desert terrain), "rock" (the rules' rock, a region's rough
		 * where it is stone) or "snow" (never from the rules: the 3D lays winter itself). A static string.
		 */
		const char* Fx = "earth";
		Drawn What = Drawn::Pasture;
	};

	/** A mesh as the game's procedural mesh takes it. Triangles wind counter-clockwise seen from their front. */
	struct Mesh
	{
		std::vector<float> Positions; // x, y, z a point, metres
		std::vector<float> Normals;   // x, y, z a point, unit
		std::vector<Rgb8> Colours;    // a point
		/** u, v a point, on the leaves' picture (T_LandLeaves): a leaf card mesh's only; empty on every other. */
		std::vector<float> Uvs;
		std::vector<uint32_t> Triangles;

		size_t Points() const { return Colours.size(); }
		size_t Tris() const { return Triangles.size() / 3; }
		bool Empty() const { return Triangles.empty(); }
	};

	/**
	 * A square of ground for culling. The field's tiles are 250 m and also cover a 250 m band round the field (the
	 * bocage laid in full beyond its edges, so a camera at the edge sees no change of ground); the ring's are oblongs
	 * of about 1 km, their ground 12.5 m apart near the field and 25, 50 and 100 m farther out.
	 */
	struct Tile
	{
		double X0 = 0.0, Y0 = 0.0, X1 = 0.0, Y1 = 0.0;
		/** Lowest and highest point of all three meshes (for the bounds). */
		float ZMin = 0.0f, ZMax = 0.0f;
		/**
		 * Near: the ground (about 4.2 m apart in the field, 8.3 m in the band round it), the sunken lanes and
		 * the hedge banks swept at their own points. The ring's tiles have only this level.
		 */
		Mesh Near;
		/** Far: the ground about 16.7 m apart, its colours averaged from the near ground's, the banks as simple ridges. */
		Mesh Far;
		/** The houses, church, barns and stone walls standing on this tile: one level, drawn at any distance. */
		Mesh Built;
		/** The metres between the near level's ground points (for choosing a level). */
		double Spacing = 0.0;
	};

	/** The repeated things, drawn instanced. Clutter bases are placed only by Clutter(). */
	enum class Base : uint8_t
	{
		ShrubA,  // hedge shrubs: hawthorn and blackthorn, dark
		ShrubB,  // hazel and field maple, lighter and rounder
		ShrubC,  // holly and elder, tall and loose
		Bramble, // low undergrowth at wood edges and under the hedges
		Oak,
		Elm,
		Ash,
		Beech,
		Pollard, // a pollarded oak or ash of the hedgerows: a thick stub and a bushy head
		Apple,
		Gate,    // a five-bar field gate on its post
		Well,
		Cart,
		// the steppe's, from the east kit (SteppeLay::Kit)
		Khata,    // a whitewashed cottage under thatch
		Chimney,  // a burnt cottage: its stove and chimney standing in the ash
		Pole,     // a telegraph pole
		Wattle,   // a length of woven wattle fence
		Sweep,    // a well sweep
		Haystack,
		Birch,
		// a region's kit (LandScape: the Mediterranean's olives and farmhouses, the desert's flat-roofed houses and scrub,
		// the jungle's trees and stilt houses, ...): each slot the model its region names (Region::KitName), read from
		// Field::Kit; none where the ground has no region
		Kit0,
		Kit1,
		Kit2,
		Kit3,
		Kit4,
		Kit5,
		Kit6,
		Kit7,
		Kit8,
		Kit9,
		Kit10,
		Kit11,
		Kit12,
		Kit13,
		Kit14,
		Kit15,
		Kit16,
		Kit17,
		Kit18,
		Kit19,
		Kit20,
		Kit21,
		Kit22,
		Kit23,
		Kit24,
		Kit25,
		Kit26,
		Kit27,
		Kit28,
		Kit29,
		Kit30,
		Kit31,
		Kit32,
		Kit33,
		Kit34,
		Kit35,
		Kit36,
		Kit37,
		Kit38,
		Kit39,
		Kit40,
		Kit41,
		Kit42,
		Kit43,
		Kit44,
		Kit45,
		Kit46,
		Kit47,
		Kit48,
		Kit49,
		Kit50,
		Kit51,
		Kit52,
		Kit53,
		Kit54,
		Kit55,
		Kit56,
		Kit57,
		Kit58,
		Kit59,
		Kit60,
		Kit61,
		Kit62,
		Kit63,   // (the last kit slot: KitSlots in all, one after another from Kit0, before Haycock and the clutter; a region
		         // names Kit0 to Kit19, the bocage's houses built to the vehicles' detail (the trial "houses") take any of them)
		Haycock, // a mown meadow's hay forked into cocks to make, 1.2 to 1.6 m, in loose rows (the bocage's July)
		Tuft,    // clutter: grass tuft (every base from here on is clutter, laid round the eye)
		TallGrass,
		Wheat,   // standing ripe corn, a clump of stalks
		Poppies,
		Flowers, // white and yellow (ox-eye, buttercup)
		Stone,
		Fern,
		Stubble, // clutter: a cut field's straw stubble in its drill rows, a few straws lying loose
		Sward,   // clutter: the short grass itself, close round the eye, in the ground's own colour (its tint)
		Trodden, // clutter: a piece of a cow path's bare trodden earth, laid end to end along the path
		Stalks,  // clutter: standing corn stalk by stalk, with its leaves and its ears, laid where Wheat is and drawn
		         // only within a few metres of the lens (the art director's cut 4, fix 4), the Wheat there cleared
		Rosette, // clutter: a broad-leaved dock and a plantain or two lying close to the ground, in the open grass (the
		         // bracken keeps to the banks: the art director's cut 5 note)
		// clutter of a region's own (LandScape: the desert's pebbles and camel thorn, the jungle's leaf litter ...), its
		// mesh the region's (Region::ClutterMesh); none where the ground has no region
		Clut0,
		Clut1,
		Clut2,
		Clut3,
		Clut4,
		Clut5,
		Count
	};
	/** The kit's slots, Kit0 to Kit63 one after another (a region names its slot K as Kit0 + K, from its first twenty; the
	 * bocage's farmhouses, barns, byres, church and village houses built to the vehicles' detail, the trial "houses", one a
	 * slot, from the first). */
	constexpr int KitSlots = 64;
	static_assert(static_cast<int>(Base::Kit0) + KitSlots == static_cast<int>(Base::Haycock), "the kit's slots run on from Kit0 to the haycock");
	/** The tallest a region's low kit stands, metres (Region::KitLow: a balk's ridge, a bund): a man steps over it and a
	 * tank rolls over it, so it may lie across a recorded men's way. */
	constexpr double LowKitHigh = 0.25;

	struct BaseMesh
	{
		std::string Name;
		/** Near the eye, and beyond FarFrom metres (where Far is empty, draw nothing beyond it). */
		Mesh Near, Far;
		/**
		 * A hedge shrub's or a tree's leaves near the eye, as the game draws them over Near: Cards, flat cards that carry
		 * the leaves' picture (cut out by its alpha, both sides lit; M_LandLeaves), or, where the game has no such
		 * material, Sprigs, small solid clusters of leaves in their places. Empty for every other base.
		 */
		Mesh Cards, Sprigs;
		float FarFrom = 100.0f;
		/** The base's size as built (before an instance's scale): radius round its origin and height. */
		float Radius = 1.0f, Height = 1.0f;
		/** A region's kit slot: how it stands for the men's and the camera's sight (LandSight), as shares of its built
		 * size (Mid and Half of its height, Across of its radius) and its trunk in metres; bSight false: seen through. */
		bool bSight = false;
		float SightMid = 0.5f, SightHalf = 0.5f, SightAcross = 0.8f, SightTrunk = 0.0f;
		/** A region's kit or clutter slot: how it is painted (0 solid, as a house or a rock; 1 leaves, the light through
		 * them; 2 blades, grass and thin stems). The bocage's own bases are painted by their kind. */
		uint8_t Paint = 0;
		/** A region's kit slot: a tree. The game draws it far off as a crown of some 40 triangles with no shadow and not
		 * past its trees' reach (GS9), as the bocage's own trees; the region sets it in KitLook. */
		bool bTree = false;
		/** A region's kit slot: its middle level where its file has one, drawn from MidFrom metres (GS9; empty: none). */
		Mesh Mid;
		float MidFrom = 60.0f;
		/**
		 * A region's kit slot: its class for the game's far draw (GS9 by class, the frame budget of 22 November,
		 * production/theatre-budgets-1122): Tree as bTree (plain, then a crown, none past the trees' reach); Shrub, a
		 * bush or a small rock (plain from the shrubs' distance, its shadow to 250 m, none past the shrubs' reach); Big,
		 * an outcrop, a pole, a stack or a tower (plain and culled at the trees' distances, no shadow past the crowns');
		 * House, a building (plain from 300 m, never culled, its shadow to 1,200 m). Thing: as before. LandField sets one
		 * from the slot's use and size where the region's KitLook leaves it Thing.
		 */
		enum class Draw : uint8_t
		{
			Thing,
			Tree,
			Shrub,
			Big,
			House
		};
		Draw Class = Draw::Thing;
	};

	/**
	 * A tree far off in some 40 triangles (the battle-scale plan's GS9): its trunk a three-sided post and its crown a
	 * six-sided ball, sized and coloured from From's points (the crown its leafy points, green over red; if none, those
	 * above a third of its height), the ball's underside darker since it is drawn with no shadow of its own. Empty if
	 * From is. The same axes, origin and winding as the field's own meshes.
	 */
	Mesh CrownOf(const Mesh& From);

	/**
	 * A tree far off on trial "farcrowns" (the game's crown in CrownOf's place), sorted by what From shows: a broadleaf
	 * (its leaves at least 0.65 as wide as they are tall) as four or five lobes of 12 triangles where its leaf masses
	 * are, lit caps and dark, cooler undersides (each lobe's foot half its cap, never under 0.40 of the lightest cap), on
	 * a trunk stub to the lobes' foot (48 to 66 triangles); a column (a cypress, a poplar, a birch: narrower than that,
	 * not a conifer) as two to four lobes stacked up its whole height on its stem (30 to 54); a conifer (tall and narrow,
	 * its boughs widest at its foot) as two stacked eight-sided skirts as wide as its lowest boughs, tapering to its tip,
	 * its own dark green with snow on its tip and a few rim corners, some half the far mesh's own share (38 triangles);
	 * a palm (its leaves only high on a long bare stem) as CrownOf makes it. Empty if From is. The same axes, origin and
	 * winding as CrownOf's.
	 */
	Mesh CrownLobed(const Mesh& From);
	/** Which way CrownLobed reads From: 0 a broadleaf, 1 a conifer, 2 a palm, 3 a column; -1 nothing to read. Narrow:
	 * its leaves' width over their height; Taper: how much wider its leaves reach in their lowest third than their top. */
	int CrownClass(const Mesh& From, double* Narrow = nullptr, double* Taper = nullptr);

	/**
	 * One instance: the base mesh scaled (X and Y by Scale, Z by ScaleZ), turned Yaw radians counter-clockwise about
	 * z (seen from above, from +x toward +y), its origin at X, Y, Z. Tint multiplies the base's colours (Tint / 128:
	 * 128 is as built) where the material can take a per-instance colour; the bases already vary among themselves.
	 */
	struct Placed
	{
		Base Kind = Base::ShrubA;
		float X = 0.0f, Y = 0.0f, Z = 0.0f;
		float Yaw = 0.0f, Scale = 1.0f, ScaleZ = 1.0f;
		Rgb8 Tint;
	};

	/** A hedgerow's line as laid (for tests, pictures and maps). Kind: 0 hedgerow, 1 lane-side bank, 2 wood edge, 3 garden hedge, 4 stone wall, 5 far strip. */
	struct HedgeLine
	{
		std::vector<std::pair<double, double>> Points;
		int Kind = 0;
	};

	/** A way through a hedgerow or a lane's bank: a field's gateway (its gate open or shut) or a breach blown through it
	 * where the recording's men went over in numbers, bank and all. Metres from the field's south-west corner; Yaw the
	 * hedge's own heading there (radians from east); Width the gap's, from bank end to bank end. */
	struct HedgeOpening
	{
		double X = 0.0, Y = 0.0, Yaw = 0.0, Width = 0.0;
		int Kind = 0;   // 0 a gateway, 1 a breach
		int Men = 0;    // a breach: the recording's men's crossings it gathers (those within 12.5 m of it)
	};

	/** A recorded vehicle's way through a hedge's bank, opened where the field was laid: the crossing's place, the
	 * hedge's heading there (radians from east), the gap opened (bank end to bank end), the hedge's kind (HedgeLine's),
	 * and the way (Field::Ways' index, and its Track) and how far along its points it crossed (K + T: between point K
	 * and K + 1, T of the way). For the hedge cutters' test switch: which of them a cutter might have made. */
	struct HedgeCrossing
	{
		double X = 0.0, Y = 0.0, Yaw = 0.0, Width = 0.0;
		int Kind = 0;
		int Way = -1, Track = -1;
		double Along = 0.0;
	};

	/** The nearest bank's middle line to a place: the point on it, its heading (radians from east), the bank's height
	 * there, and how far its foot reaches either side of the line (where it meets the field), metres; Kind as
	 * HedgeLine's. */
	struct BankPlace
	{
		double X = 0.0, Y = 0.0, Yaw = 0.0, Height = 0.0, Foot = 0.0;
		int Kind = 0;
	};

	/** What was laid and how long it took, for the log and the test. */
	struct Tally
	{
		int Parcels = 0;          // fields and pastures of the bocage (in the field and its band)
		double ParcelMean = 0.0;  // their mean size across, metres (square root of the mean area)
		int Lanes = 0;
		double LaneKm = 0.0;
		double HedgeKm = 0.0;     // banks and walls, after the gaps
		int Gates = 0;
		int Haycocks = 0;         // the mown meadows' cocks of hay
		int HayFields = 0, HayFieldsCocked = 0; // the mown meadows, and those with their hay in cocks
		double CowPathKm = 0.0;   // the cattle's paths along the pastures' hedges and out of their gateways
		int WayGaps = 0;          // hedges opened where the recording's vehicles cross them
		int Breaches = 0;         // hedgerows blown open where the recording's men went over them in numbers
		int Houses = 0;           // houses, barns and byres (the church counted apart)
		int HouseModels = 0;      // the farmhouses, barns, byres and church built to the vehicles' detail (the trial "houses"), and their kinds
		int HouseKinds = 0;
		int Damaged = 0;
		bool bChurch = false;
		int Farmsteads = 0;
		int Poles = 0;            // the steppe's telegraph poles
		double RailKm = 0.0;      // and its railway, laid
		/** The bocage's hedge trees (hedgerows and lane banks, in the field and its band; not their young shoots): the
		 * hedges' length, the trees laid on them and those the trial "hedgewall" leaves out to clump the rest; the most trees
		 * in one run (neighbours 12 m apart at most) and its length, the widest gap between two neighbours on one hedge, and
		 * the longest stretch of a hedge with no tree (its ends counted). And their shrubs' tops: the step in height from
		 * one to the next along a hedge (its tall clumps apart), median and 95 in 100, metres, and the share over 0.5 m. */
		double BocageHedgeKm = 0.0;
		int BocageStandards = 0, BocageStandardsDropped = 0;
		int StandardRunMost = 0;
		double StandardRunMetres = 0.0, StandardGapMost = 0.0, StandardBareMost = 0.0;
		double ShrubStepMedian = 0.0, ShrubStep95 = 0.0, ShrubStepOver = 0.0;
		/** And how the bocage's hedge trees stand: the spacing from each to the next along its hedge, under 6 m, 6 to 12,
		 * 12 to 25, 25 to 60 and over 60 m; the runs (neighbours 12 m apart at most) of one tree, two, ... five, six or more. */
		int StandardSpacingBins[5] = {};
		int StandardRunsOf[6] = {};
		size_t NearTris = 0, FarTris = 0, BuiltTris = 0, RingTris = 0;
		size_t Instances = 0;
		double CoreSeconds = 0.0; // heights, lanes, parcels, hedges, village, instances
		double MeshSeconds = 0.0; // the tiles and the ring
	};

	class Ground
	{
	public:
		explicit Ground(const Field& F);
		~Ground();
		Ground(const Ground&) = delete;
		Ground& operator=(const Ground&) = delete;

		/** The surface drawn at a place: banks, lane floors and house floors included. Any thread: nothing is changed. */
		Surface At(double X, double Y) const;
		/** The drawn surface's height alone (At's Z), and what is drawn there when asked (At's What), without At's
		 * normal and effects word: a fifth of its cost, for the camera's many questions a frame. Any thread. */
		double HeightAt(double X, double Y, Drawn* OutWhat = nullptr) const;
		/** The near ground's normal at a place as its mesh is lit there (its points' normals mixed across the triangle
		 * the place is in): for what is laid flat on it, a shell hole's thrown soil, a track's strip, to be lit alike.
		 * Under a bank or a house the ground's own, not theirs. Any thread. */
		void DrawnUp(double X, double Y, double& OutX, double& OutY, double& OutZ) const;
		/** The near ground's height at a place as its mesh is drawn: the flat triangle the place is in, its corners some
		 * 4 m apart, not HeightAt's smooth height between them (they differ by a centimetre or two on a slope's bend).
		 * For what lies thin on the ground, a shell hole's thrown soil's edge, not to sink under it. The ground's own
		 * (no bank, no floor). Any thread. */
		double DrawnZ(double X, double Y) const;
		/** Whether a house's floor or a stone wall's line may be drawn at a place (the field's 2 m raster): where false,
		 * HeightAt never says Floor or Wall there. A few nanoseconds, so the camera's sight (LandSight) asks HeightAt
		 * only where it may. Any thread. */
		bool MayBeBuilt(double X, double Y) const;
		/** The ground's own colour at a place (8-bit sRGB), as the near ground's points are painted: for what is laid
		 * over it, a shell hole's thrown soil, to fade into it (a hole's edge was a hard olive outline on a pale field
		 * of stubble: the second cut's stills, 21 November). Any thread. */
		Rgb8 ColourAt(double X, double Y) const;
		/** Whether ripe or green corn stands here (wheat, barley, oats in a field's crop, not its stubble or a meadow's
		 * grass): where a tank's tracks lay it flat between them. Any thread. */
		bool CornAt(double X, double Y) const;

		/** The field's tiles (and its 250 m band), row by row from the south-west. */
		const std::vector<Tile>& Tiles() const;
		/** The far ring's tiles, out to 5 km beyond the field's edges. */
		const std::vector<Tile>& Ring() const;

		const BaseMesh& BaseOf(Base B) const;
		/** The region's step down the frame budget's ladder of nearer far-draw distances (0, GS9's own, to 4, the
		 * nearest: LandScape::Region::DrawStep); -1 where the ground has no region (the bocage and the steppe draw as
		 * they always have). */
		int DrawStep() const;
		/** Every instance of a base (none for the clutter bases: see Clutter). */
		const std::vector<Placed>& Instances(Base B) const;

		/**
		 * The near clutter (grass tufts, tall grass, standing corn, poppies, flowers, stones, ferns) in a box: the same
		 * things every time for the same ground, laid by 8 m squares, so boxes that overlap agree. For the squares round
		 * the eye (some 40 to 80 m): about one thing a square metre in grass, two in standing corn.
		 */
		void Clutter(double X0, double Y0, double X1, double Y1, std::vector<Placed>& Out) const;
		/**
		 * Burnt ground for the clutter (the scorch layer's, LandScorch): a round at X, Y in which the near clutter thins
		 * and dries. Within Core metres 3 things in 10 stay (the same ones each time, by each thing's own number); from
		 * there out to Outer metres more and more, all of them at Outer; the flowers go first; what stays is brought
		 * toward straw (about 150, 125, 80). Where rounds overlap the strongest counts, they do not multiply. With no
		 * round given the clutter is as it always was. Call it after the ground is built and before the clutter is first
		 * asked for, from one thread; a square already laid by its caller is not laid again by this.
		 */
		void AddBurn(double X, double Y, double Core, double Outer);

		/** The hedgerows' and walls' lines (tests, maps). */
		const std::vector<HedgeLine>& Hedges() const;
		/** The gateways and breaches through them. */
		const std::vector<HedgeOpening>& Openings() const;
		/** The recorded vehicles' ways through the hedges, as opened (none where a cut stands: Field::Cuts). */
		const std::vector<HedgeCrossing>& Crossings() const;
		/** The nearest bank's middle line within Reach metres of a place (a hedgerow's, a lane's, a wood's edge or a
		 * garden's; not a wall): false where none is. Any thread. */
		bool NearestBank(double X, double Y, double Reach, BankPlace& Out) const;
		/** The banks' rise over the land at a place (0 off them): what a breach through a bank takes away. Any thread. */
		double BankRise(double X, double Y) const;
		/** The lanes' middle lines. */
		const std::vector<std::vector<std::pair<double, double>>>& Lanes() const;
		/** The parcel at a place (-1 where none: lanes, woods, beyond), for maps and tests. */
		int ParcelAt(double X, double Y) const;
		/** What a parcel grows, in a word ("wheat", "barley", "oats", "green wheat", "stubble", "roots", "grazed",
		 * "lush", "hay cut", "meadow", "rye", "steppe", "fallow"; "" for none), for tests and sheets. */
		std::string CropOfParcel(int Parcel) const;

		const Tally& Laid() const;
		/** The look laid (LandScape::NameOf: "bocage", "steppe", "desert", ...) and the snow lying on it (0 to 1). */
		const char* LookWord() const;
		double SnowLying() const;
		/** The shell holes' earth for this ground (LandCrater::SetEarth), empty for the bocage's own. */
		const char* EarthWord() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> In;
	};
}
