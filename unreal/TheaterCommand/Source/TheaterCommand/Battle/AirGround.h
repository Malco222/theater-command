#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/**
 * The set-piece stage's ground as it is painted, in plain C++ so that the cloud test paints exactly what the game
 * draws (the art director on build 1734, 19 November: Pearl Harbor's land read as flat blocks in three plain colours
 * with hard polygon bands, and the shallows as a mint band; the game reuses this ground for every strike on the
 * harbour, so it is worth one cheap pass).
 *
 * The stage file (Content/Data/Stages/<key>.json, from the rules repo's tools/stages/mesh.py) gives each mesh's points
 * with one palette paint each. This pass turns them into colours and:
 * - varies the grass, patches some per cent lighter or darker at two sizes, and turns a little of it toward the red
 *   earth of the pineapple fields' paint in patches a few hundred metres across;
 * - keeps the sand to the water's edge: a sand point back from it is mostly the grass behind, and a large triangle
 *   sand at all three corners takes a greener middle by how far it lies from the coast, so that the beach is a band
 *   along the water and no longer a block with straight edges;
 * - paints the shallows' open-sea edge the sea's own paint, so the band darkens from the coast into the deep water
 *   (not where the stage has a lagoon, as Midway has: there the band meets the bright lagoon, and keeps its edge);
 * - lifts the runways 0.8 m, clear of the land under them (Ford Island's strip lay at the island's own height).
 * The mottle finer than the points are apart is the ground material's own grain (AAirStage sets it).
 *
 * The art director on the first picture (19 November, 17:45Z): the mottle and the shallows' tint spread across big
 * triangles showed as stars and wedges, the thin sand edge read as a drawn outline round the harbour's natural shores,
 * and the paving was a blank card. So near the battle (Focus) the land and the shallows are first split until no edge
 * there is longer than some tens of metres, each new point painted for its own place; the shallows fade by how far a
 * point lies between the coast and the band's open-sea edge; a shore in sheltered water (the band there narrow) is the
 * grass to the water and not sand; and the piers' concrete is worn (Worn, which the yard's paving uses too).
 *
 * On the second picture (18:47Z): by the quays, piers and seawalls the deep water runs to the wall (a berth was
 * dredged), and along the natural shores the band's width wanders, so it no longer reads as an outline glow.
 */
namespace AirGround
{
	struct Rgb8
	{
		uint8_t R = 128, G = 128, B = 128;
	};

	/** One mesh of the stage file: points in metres (x east, y north, z up from the sea), a paint each, triangles. */
	struct Part
	{
		std::string Name;
		std::vector<double> X, Y, Z;
		std::vector<int> Paints;
		std::vector<int> Tris;
	};

	/** The same as drawn: a colour each point, with the points and triangles the pass added. */
	struct Drawn
	{
		std::string Name;
		std::vector<double> X, Y, Z;
		std::vector<Rgb8> Colours;
		std::vector<int> Tris;
	};

	struct Tally
	{
		int SandSplit = 0;    // points laid to keep the sand to the water's edge (on shore edges, in sand triangles)
		int ShoreFaded = 0;   // shallows points on the open-sea side, painted the sea
		int ShoreKept = 0;    // shallows points on the coast or a reef's edge
		int Earth = 0;        // grass points turned some way toward red earth
		int RunwayRaised = 0; // runway points lifted
		bool bLagoon = false; // the stage has a lagoon: the shallows keep their edge
		int Refined = 0;      // points laid by splitting the triangles near the focus
		int Sheltered = 0;    // shore points in sheltered water, painted the grass and not sand
		int Worn = 0;         // concrete points worn
		int Walled = 0;       // shallows points by a quay, pier or seawall, painted the deep sea's
		int Slabs = 0;        // slabs the works' flat concrete tops were cut into
	};

	/** Where the battle is, and how fine the ground is split round it (no splitting with no radius). */
	struct Focus
	{
		double X = 0.0, Y = 0.0, Radius = 0.0;
		double LandEdge = 80.0, WaterEdge = 30.0; // metres: the longest edge left within the radius
	};

	/** How many edges of a mesh only one triangle uses, points at one place counted as one: its rim, and any crack. */
	size_t OpenEdges(const std::vector<double>& X, const std::vector<double>& Y, const std::vector<int>& Tris);

	/** The length of those edges, metres: a mesh split without a crack keeps its rim's length. */
	double RimLength(const std::vector<double>& X, const std::vector<double>& Y, const std::vector<int>& Tris);

	/** Concrete as it wears at a place (stage metres): patches a few per cent lighter or darker, and darker stains. */
	Rgb8 Worn(const Rgb8& Base, double X, double Y);

	/** One panel of a paved block's top: its middle (stage metres), its size along and across the block, its colour. */
	struct Panel
	{
		double X = 0.0, Y = 0.0, Long = 0.0, Wide = 0.0;
		Rgb8 Colour;
	};

	/**
	 * A paved block's top (the yard's pads and the seaplane apron, tools/stages/yard.py) as panels about Cell metres
	 * square, each its own shade of the concrete: Worn at its middle, and each a little lighter or darker than the next,
	 * as poured slabs and their patching read from the air (the art director, 17:45Z: the paving about 140 luma, not a
	 * flat card, with stains and patching of 6 to 10). Heading: the compass heading of the block's length, degrees.
	 */
	std::vector<Panel> Paving(double X, double Y, double Long, double Wide, double Heading, const Rgb8& Base, double Cell = 10.0);

	/** Luma of a colour as the art director measures it (0.299, 0.587, 0.114 of the picture's bytes). */
	double Luma(const Rgb8& C);

	/** The stage's meshes painted for the scene, in the order given. Names: the palette's names, in its order. */
	std::vector<Drawn> Paint(const std::vector<Part>& Parts, const std::vector<std::string>& Names, const std::vector<Rgb8>& Palette, const Rgb8& Sea, Tally* Out = nullptr,
		const Focus& Near = Focus());

	/**
	 * A battle with no stage (Bremen's raid): the board's own land, the rings of place.ground.land, as the flight core's
	 * ground. LandHigh metres inside any of the rings (LayGround lays every ring as land, at this same height: one number,
	 * so that the bursts are never under the sheet) and the sea outside, so that
	 * bombs and falls on the land burst and crash as on land, and aircraft wait on their fields on it (the pre-check on
	 * round 14: with no stage the core had been told the sea everywhere, and Bremen took water columns). The rings'
	 * edges are kept in bands of Y, so an ask looks at a few edges, not thousands; it reads nothing it changes, so the
	 * sky's painter may ask it from the pool's threads.
	 */
	class BoardLand
	{
	public:
		static constexpr double LandHigh = 2.0;
		BoardLand() = default;
		explicit BoardLand(const std::vector<std::vector<std::pair<double, double>>>& Rings);
		bool IsEmpty() const { return Bands.empty(); }
		/** LandHigh on the land, 0 on the sea. */
		double At(double X, double Y) const;

	private:
		struct Edge
		{
			double X0 = 0.0, Y0 = 0.0, X1 = 0.0, Y1 = 0.0;
			size_t Ring = 0;
		};
		std::vector<std::vector<Edge>> Bands;
		double Low = 0.0, BandTall = 1.0;
	};
}
