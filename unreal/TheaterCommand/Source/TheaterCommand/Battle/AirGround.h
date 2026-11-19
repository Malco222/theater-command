#pragma once

#include <cstdint>
#include <string>
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
	};

	/** How many edges of a mesh only one triangle uses, points at one place counted as one: its rim, and any crack. */
	size_t OpenEdges(const std::vector<double>& X, const std::vector<double>& Y, const std::vector<int>& Tris);

	/** Luma of a colour as the art director measures it (0.299, 0.587, 0.114 of the picture's bytes). */
	double Luma(const Rgb8& C);

	/** The stage's meshes painted for the scene, in the order given. Names: the palette's names, in its order. */
	std::vector<Drawn> Paint(const std::vector<Part>& Parts, const std::vector<std::string>& Names, const std::vector<Rgb8>& Palette, const Rgb8& Sea, Tally* Out = nullptr);
}
