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
		uint64_t Seed = 1;
		/** The open steppe, where the recording lays it (Steppe.bOn); else the bocage. */
		SteppeLay Steppe;

		/**
		 * Where men and vehicles go, if known (a recording's men tracks, each man's kept points): a hedge they cross is
		 * opened there, as wide as HalfWidth either side plus half a metre, and nothing is built or planted on them. Leave
		 * empty for a battle not yet fought.
		 */
		struct Way
		{
			std::vector<std::pair<double, double>> Points;
			double HalfWidth = 1.0;
		};
		std::vector<Way> Ways;

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
		 * "earth", "town", "woods", "water", "mud", "sand" (desert terrain) or "snow" (never from the rules: the 3D lays
		 * winter itself). A static string.
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
		Count
	};

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
	};

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
		int Damaged = 0;
		bool bChurch = false;
		int Farmsteads = 0;
		int Poles = 0;            // the steppe's telegraph poles
		double RailKm = 0.0;      // and its railway, laid
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
		/** Every instance of a base (none for the clutter bases: see Clutter). */
		const std::vector<Placed>& Instances(Base B) const;

		/**
		 * The near clutter (grass tufts, tall grass, standing corn, poppies, flowers, stones, ferns) in a box: the same
		 * things every time for the same ground, laid by 8 m squares, so boxes that overlap agree. For the squares round
		 * the eye (some 40 to 80 m): about one thing a square metre in grass, two in standing corn.
		 */
		void Clutter(double X0, double Y0, double X1, double Y1, std::vector<Placed>& Out) const;

		/** The hedgerows' and walls' lines (tests, maps). */
		const std::vector<HedgeLine>& Hedges() const;
		/** The gateways and breaches through them. */
		const std::vector<HedgeOpening>& Openings() const;
		/** The lanes' middle lines. */
		const std::vector<std::vector<std::pair<double, double>>>& Lanes() const;
		/** The parcel at a place (-1 where none: lanes, woods, beyond), for maps and tests. */
		int ParcelAt(double X, double Y) const;
		/** What a parcel grows, in a word ("wheat", "barley", "oats", "green wheat", "stubble", "roots", "grazed",
		 * "lush", "hay cut", "meadow", "rye", "steppe", "fallow"; "" for none), for tests and sheets. */
		std::string CropOfParcel(int Parcel) const;

		const Tally& Laid() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> In;
	};
}
