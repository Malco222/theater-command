#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * Earned colours for air units (release 2; Brian, 25 November 2026: "earned nicknames as they complete missions. i also
 * want them to get unique colorings or markings like the tuskegee airmen"). A group that has flown hard is known in the
 * air by colours nobody else wears: the art director's palette (Content/Data/Air/unit-colours.json, from
 * art-director/air-colours, third issue of 25 November, 04:05Z) gives each nation sixteen schemes of a spinner colour, a nose band colour
 * and a tail in one of five patterns; the rules decide which scheme a group holds and how far it has earned it (its
 * tier, 1 to 3, the ticket's place.corpsMarks). This only draws it.
 *
 * Where each step shows (the art director's brief, section 2):
 * - a fighter: step 1 its spinner (the "boss" paint; a jet's nose cap, no longer than a spinner); step 2 a narrow band
 *   right behind the spinner, no wider than the spinner is long, found from the shape (on a radial the cowling's front
 *   lip), never the whole nose paint; step 3 its tail in the pattern, both sides;
 * - a bomber: step 1 every spinner or propeller boss and the top sixth of its tail; step 2 the front ring of every
 *   engine cowling; step 3 the tail in the pattern, twin fins on both faces;
 * - the tail by nation: Germany, the United States, Japan, the Soviet Union and Italy fin and rudder; Britain the
 *   rudder only (the fin keeps its flash); France the fin only (the rudder keeps its stripes);
 * - a night finish (a battle by night, or a machine with black undersides): every earned colour times 0.55, the cream
 *   #7E7C72, and the tail's pattern on the top half of the tail only; a winter month: the colours at full strength.
 * Never recoloured: glass, crew, tyres, blades, rivet and seam lines, lamps, guns, exhausts, the recognition bands and
 * anything else that is not the machine's own coat (but the finer B-17's rivet and seam lines, 44% of her close-up points,
 * which take the colour the coat beside them takes, a shade darker as her file has them: LinesTakeCoat); nor its marks, nose art or tally, which are drawn apart and lie on
 * top. Where the pattern is checks or stripes, each marking on the part of the tail its nation colours sits on a plain
 * patch of the first colour (shaped as the marking, a little larger, laid in the tail's own skin on the marking's face;
 * by night only where the patch reaches the pattern's half, and then whole), so that it is never cut up.
 *
 * The regions are found from the model's own file (Content/Models/<Kind>.json, read as CampaignMiniatures reads it:
 * its points in order, their facings, paints and parts): the airscrews from their blades (Battle/AirScrew), the fins
 * and rudders from the shape (a newer file's rudder parts, an older file's hinge line). A pattern's edges are cut into
 * the skin (each triangle a line crosses is parted along it), so that a check is square at any distance.
 *
 * Plain C++, so that the cloud test (Tools/air_show/air_unit_colours_test.cpp) and the pictures read exactly what the
 * game reads. Nothing here decides anything: the rules say who has earned what, and nothing drawn reads back.
 */
namespace AirUnitColours
{
	struct Rgb
	{
		uint8_t R = 0, G = 0, B = 0;
		bool operator==(const Rgb& O) const { return R == O.R && G == O.G && B == O.B; }
		bool operator!=(const Rgb& O) const { return !(*this == O); }
	};
	/** "#A53A2C" or "A53A2C"; false where it is not one. */
	bool RgbOf(const std::string& Hex, Rgb& Out);

	/** One of a nation's sixteen schemes (the palette's nations.<NAT>[k]): its colours by the palette's names. */
	struct Scheme
	{
		int Number = 0;
		std::string Spinner, Ring, Pattern, First, Second, Words;
	};

	/** The art director's palette (Content/Data/Air/unit-colours.json). */
	struct Palette
	{
		std::map<std::string, Rgb> Colours;              // by name: "ink", "bone", "brick", ...
		std::map<std::string, std::vector<Scheme>> Nations; // "GER", "UK", "USA", "SOV", "JAP", "ITA", "FRA": 16 each
		std::map<std::string, std::string> Patterns;     // the five patterns' words
		double NightMultiply = 0.55;
		std::string Cream = "bone";
		Rgb CreamAtNight{ 0x7E, 0x7C, 0x72 };
		/** A nation's scheme (the palette's own nation codes; "JPN" read as "JAP"), or null. */
		const Scheme* Of(const std::string& Nation, int Number) const;
		/** A colour by name, as it is painted by day or on a night finish (dulled; the cream its own grey). */
		Rgb Paint(const std::string& Name, bool bNight) const;
	};
	bool ReadPalette(const std::string& Text, Palette& Out, std::string& Why);
	/** The palette's nation code for a nation as the battle names it ("JPN" is "JAP"); as given otherwise. */
	inline std::string NationOf(const std::string& Nation)
	{
		return Nation == "JPN" ? std::string("JAP") : Nation;
	}

	/** A row of the ticket's place.corpsMarks: a group that has colours (the rules' corps id), its nation, which of that
	 * nation's sixteen schemes (0 to 15) and how far earned (1 the spinner, with its nickname; 2 and the nose band; 3 and
	 * the tail). Its name is the nickname, shown on the cards and never on the aircraft (the brief, section 5). */
	struct CorpsMark
	{
		int Corps = -1;
		std::string Nation;
		int Scheme = -1;
		int Tier = 0;
		std::string Name;
	};
	/** place.corpsMarks read (the one place its shape is read: the rules may yet rename it), by corps: [{ corps, nat,
	 * scheme, tier, name }]. Rows that do not read (no corps, no nation, a scheme outside 0 to 15, a tier outside 1 to 3)
	 * are left out. Written for the feed's own reader (AirFeed::Json; Battle/AirFeed's ReadSky calls it), so that the
	 * feed needs nothing more to be built with. */
	template <typename TJson>
	void ReadMarks(const TJson& List, std::map<int, CorpsMark>& Out)
	{
		Out.clear();
		for (const TJson& Row : List.Items)
		{
			CorpsMark M;
			M.Corps = Row["corps"].Int(-1);
			M.Nation = NationOf(Row["nat"].Str(""));
			M.Scheme = Row["scheme"].Int(-1);
			M.Tier = Row["tier"].Int(0);
			M.Name = Row["name"].Str("");
			if (M.Corps < 0 || M.Nation.empty() || M.Scheme < 0 || M.Scheme > 15 || M.Tier < 1 || M.Tier > 3) continue;
			Out[M.Corps] = M;
		}
	}

	/** What a group's aircraft wear. */
	struct Look
	{
		std::string Nation; // the palette's code
		int Scheme = -1;
		int Tier = 0;
		bool bNightBattle = false; // the battle is fought by night (place.night)
		bool bWinter = false;      // the painter's month is a winter one (December to February): full strength, as by day
		/** Whether the nation wears stripes on a plain machine's rudder (a "*FIN" place; the game's UnitTable::Fin): a
		 * patch is laid under them only where they are drawn. */
		bool bFinStripes = true;
		bool Valid() const { return Tier >= 1 && Tier <= 3 && Scheme >= 0 && Scheme < 16 && !Nation.empty(); }
	};
	/**
	 * A group's look: false (and nothing drawn) with the rules card's row off, for a group whose corps the battle does not
	 * name or that has no row, or a row whose nation or scheme the palette does not have. Month counts from 0 (the feed's).
	 */
	bool LookOf(bool bOn, const std::map<int, CorpsMark>& Marks, int Corps, const Palette& P, bool bNightBattle, int Month, Look& Out);

	/** A part of a model (its file's parts[k]). */
	struct Part
	{
		std::string Name;
		double Pivot[3] = { 0.0, 0.0, 0.0 };
		double Axis[3] = { 0.0, 0.0, 1.0 };
	};
	/** A marking's place (its file's marks[k]): a disc of Radius at At facing Facing, mirrored to the other side where
	 * Both; or a four-cornered place on the fin (Quad: x along the machine and z up, the lower leading corner first). */
	struct MarkPlace
	{
		std::string Tag, Place;
		double At[3] = { 0.0, 0.0, 0.0 };
		double Facing[3] = { 0.0, 1.0, 0.0 };
		double Radius = 0.3;
		bool bBoth = false;
		std::vector<std::pair<double, double>> Quad;
	};
	/** One of a model's two meshes ("near" or "far"), as the file gives it: three numbers a point, one paint a point. */
	struct Mesh
	{
		std::vector<double> Points, Facings;
		std::vector<int> Paints, Parts, Triangles, Soot, Blend;
		size_t Count() const { return Paints.size(); }
		double X(size_t K) const { return Points[K * 3]; }
		double Y(size_t K) const { return Points[K * 3 + 1]; }
		double Z(size_t K) const { return Points[K * 3 + 2]; }
	};
	/** An aircraft model's file (Content/Models/<Kind>.json), the parts of it this needs. Metres; x forward, y right, z up. */
	struct Model
	{
		std::string Kind, Role;
		int Engines = 0, Jets = 0;
		std::vector<std::string> PaintNames; // the file's, or the plain build's eighteen where it names none
		std::vector<Rgb> Colours;            // each paint's own colour
		std::vector<Part> Parts;
		std::vector<MarkPlace> Marks;
		Mesh Near, Far;
		/** Fighter or bomber: the file's role, or its engines (a file with neither is one of the old plain build's bombers). */
		bool bFighter = false;
		/** Its undersides black (its "under" paint near black, neutral): a night bomber's finish. */
		bool bBlackUnder = false;
		int Paint(const char* Name) const;
	};
	bool ReadModel(const std::string& Text, const std::string& Kind, Model& Out, std::string& Why);

	/** What a paint is to the earned colours: never touched, the machine's coat (upper, upper2, under, nose), a spinner
	 * (boss or hub), or what stands for a spinner on a model that paints none (the plain build's "leg" grey at a hub). */
	enum class PaintIs : uint8_t { Never, Coat, Boss, Hub, Line };
	/** Whether a model's rivet and seam lines (its paints rivet, seam, rivet_under, seam_under) take the earned colours as
	 * the coat beside them does (PaintIs::Line), each a shade of it as the file has the line against its coat: the finer
	 * B-17 alone (Content/Models/B17Fine.json, the Battle rules card's "Finer B-17"), whose lines are 44% of her close-up
	 * points; every other model's lines are never recoloured, as before. */
	bool LinesTakeCoat(const std::string& Kind);

	/** An engine's places: its airscrew's axis (along x through Y, Z), its spinner's front and back, its cowling's front
	 * and the band behind it, and how far out from the axis the band reaches. A jet's nose cap is one too (bCap). */
	struct Engine
	{
		double Hub[3] = { 0.0, 0.0, 0.0 };
		double SpinnerFront = 0.0, SpinnerBack = 0.0;
		bool bSpinner = false; // a boss or hub was found at it
		bool bHubStandIn = false; // its boss is the hub's grey (the model paints no boss)
		double CowlFront = 0.0;
		double Band = 0.0;     // the band's width (no more than the spinner's length)
		double Reach = 1.0;    // how far out from the axis the band and the spinner reach
		bool bCap = false;     // a jet's nose cap stands for the spinner (from CowlFront back by Cap)
		double Cap = 0.0;
		double SpinnerLong() const { return bCap ? Cap : SpinnerFront - SpinnerBack; }
	};

	/** Which part of a tail a place is, for the nation's rule. */
	enum class TailPart : uint8_t { Both, Fin, Rudder };

	/** A tail's frame for a pattern, measured on the earned part's own height and chord: its lowest and highest points,
	 * the top front corner's x and the bottom rear corner's x, and how far forward its points reach along the normal to
	 * the hinge (SFore, the foremost; SAft the hindmost). */
	struct Frame
	{
		bool bAny = false;
		double ZLow = 0.0, ZHigh = 0.0, XLeadTop = 0.0, XTrailLow = 0.0, SFore = 0.0, SAft = 0.0;
		double XMin = 0.0, XMax = 0.0;
		double High() const { return ZHigh - ZLow; }
	};

	/** One fin and its rudder (two for a machine with twin fins). */
	struct Tail
	{
		double Y = 0.0;          // where it stands across the span
		double Slab = 0.1;       // how far either side of Y its skin lies
		double RootA = 0.0, RootB = 0.0; // the fin's root line, z = RootA + RootB x (below it is the fuselage or tailplane)
		/** Its rudder: a newer file's rudder part (its index in Parts), else the hinge line an older file paints on the fin
		 * (its points from the root up, x and z), aft of which is the rudder. */
		int RudderPart = -1;
		std::vector<std::pair<double, double>> Hinge;
		double Normal[2] = { 1.0, 0.0 }; // across the hinge in x and z, forward (stripes run along the hinge)
		/** Its box (x and z), round every point of it. */
		double XMin = 0.0, XMax = 0.0, ZMin = 0.0, ZMax = 0.0;
		Frame Frames[3]; // by TailPart: fin and rudder, the fin alone, the rudder alone (the near mesh's points)
	};

	/** A model's regions, found once from its near mesh and laid on either mesh. */
	struct Found
	{
		bool bFighter = false;
		std::vector<PaintIs> Paints; // by paint number
		std::vector<double> Shade;   // by paint number: a Line's shade against its coat (its colour's light over the coat's); 1 for the rest
		std::vector<Engine> Engines;
		std::vector<Tail> Tails;
		std::string Note; // what could not be found, for the log and the test
		/** For a far shape made without spinners (no boss or hub point on it: the finer B-17's), by paint number, what each
		 * paint is on that shape: its "engine" grey within a spinner's place stands for the spinner; and each engine's band
		 * laid behind that shape's own cowling lip. Both empty for every other model: its far shape laid as before. */
		std::vector<PaintIs> FarPaints;
		std::vector<double> FarCowlFront;
	};
	Found Find(const Model& M);
	/** The regions as laid on one mesh (bFar: the far one; Paint and Places take them so): F itself, or on a far shape made
	 * without spinners F with its FarPaints and FarCowlFront. */
	Found OnShape(const Found& F, bool bFar);

	/** Where a point of a mesh lies (the regions, before any scheme): for the test's counts and bounds. */
	enum class Where : uint8_t { None, Spinner, Ring, Fin, Rudder };
	/** Each point of a mesh (bFar: the far one), and for a tail point which tail (else -1). */
	void Places(const Model& M, const Found& F, bool bFar, std::vector<Where>& Out, std::vector<int>* TailOf = nullptr);
	/** Whether a point of the tail is on the part of it a nation colours. */
	TailPart PartFor(const std::string& Nation);

	/** What becomes of a point: kept, or which earned colour it takes. */
	enum class Takes : uint8_t { Keep, Spinner, Ring, Tip, First, Second, Patch };

	/** A new point of a parted triangle: where it lies among the triangle's corners (From, by weight), and its colour
	 * (earned: Colour; kept: the corners' own, mixed by weight). */
	struct Corner
	{
		int From[3] = { 0, 0, 0 };
		double Weight[3] = { 1.0, 0.0, 0.0 }; // (double: a few millimetres' sliver far aft keeps its place)
		bool bEarned = false;
		Rgb Colour;
		Takes What = Takes::Keep; // what its piece takes (for the test and the pictures; Colour is what is drawn)
	};
	/** A triangle of the file's (its corners in the file's order) parted along a pattern's edges: its pieces, three
	 * corners each, each running the way the triangle's own corners run. */
	struct Cut
	{
		int Triangle[3] = { 0, 0, 0 };
		std::vector<Corner> Corners;
		std::vector<int> Pieces;
	};
	/** A patch laid under a tail marking (for the log and the test): which mark, on which tail. A marking gets one only
	 * where its middle lies on a part of the tail its nation colours (never Britain's fin flash, nor France's rudder
	 * stripes); shaped as the marking is (a disc, a box, its own four-cornered place, Savoy's cross under its arms and
	 * shield), a little larger, on the face the marking is on. */
	struct PatchLaid
	{
		int Mark = -1;
		bool bMirrored = false;
		int Tail = -1;
	};
	/** A mesh's earned coat: its points recoloured (by index; the rest keep theirs) and its triangles parted. */
	struct Coat
	{
		bool bNight = false;
		std::vector<std::pair<int, Rgb>> Points;
		std::vector<Cut> Cuts;
		std::vector<PatchLaid> Patches;
		int Count(Takes What) const;
		std::vector<Takes> Took; // each point's (Keep for the rest)
	};
	/** The coat a look gives a model's mesh (bFar: the far one). Nothing for a look that is not valid. */
	Coat Paint(const Model& M, const Found& F, bool bFar, const Palette& P, const Look& L);
	/**
	 * A shape's triangles with a coat's cuts laid in: the shape's own triangles (corners numbered as the file's points,
	 * PointCount of them, in whatever order and turn the shape keeps them; any corner past PointCount, a marking's, is
	 * never parted), each parted one's pieces in its place, running as the shape's triangle runs. The pieces' new corners
	 * are numbered from FirstNew on, one for each of New's (cut, corner) pairs: its place and facing are its triangle's
	 * corners' mixed by Corner::Weight, its colour Corner::Colour where earned, else the corners' own colours (as they were
	 * before the coat) mixed by weight.
	 */
	void LayTriangles(const Coat& C, const std::vector<int>& Triangles, int PointCount, int FirstNew, std::vector<int>& Out, std::vector<std::pair<int, int>>& New);
	/** A mesh's colours as the game paints them from its file (each paint's colour, the upper surfaces' blend into the
	 * second colour and the exhausts' soot mixed in, as CampaignMiniatures does): for the test and the pictures. */
	std::vector<Rgb> BaseColours(const Model& M, bool bFar);
	/** Whether the look is drawn on a night finish on this model: by night, or on black undersides. */
	bool NightFinish(const Model& M, const Look& L);
	/** A short key for a look on a model's coat ("GER 3 t2 night"): two aircraft with the same key wear the same. */
	std::string KeyOf(const Model& M, const Look& L);

	/** The pattern's choice at a place on a tail's frame (x and z): true for the first colour, false for the second. */
	bool FirstAt(const std::string& Pattern, const Frame& F, const double Normal[2], double X, double Z);
}
