#pragma once

#include <string>
#include <vector>

/**
 * An aircraft's airscrews, found in its sculpted model so that the air battle can turn them (Brian, 20 November 14:20Z:
 * "the japanese zero's propeller didnt spin"). Tools/build_plane.py bakes each blade into the model as its own small
 * skin painted "blade" (paint 9): rings at a tenth, three tenths, six tenths, 0.88 and the whole of the screw's radius
 * out from the hub, broad and thick at the root, fined to the tip. The model does not say where the hubs are, so they
 * are found again here from the blades themselves:
 * - the blade triangles are parted into blades, each the points its triangles join;
 * - each blade's long axis runs from its fat root to its thin tip; the hub lies on that axis, the screw's radius back
 *   from the tip, the blade's length being nine tenths of the radius;
 * - blades whose hubs meet are one airscrew.
 * Plain C++, so that the cloud test finds the same hubs in the same files the game reads.
 */
namespace AirScrew
{
	struct Vec3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};

	struct Screw
	{
		Vec3 Hub;          // where the blades meet, in the points' own frame
		Vec3 Axis;         // the way the screw's shaft runs (a unit vector; either way along it)
		double Radius = 0.0;
		int Blades = 0;
	};

	struct Found
	{
		std::vector<Screw> Screws;
		/** Each triangle (by its number in the list given) true if it is a blade's, which the turning screw replaces. */
		std::vector<bool> BladeTriangle;
		/** And for each, the screw (its number in Screws) whose blade it is; -1 for every other triangle. */
		std::vector<int> ScrewOfTriangle;
	};

	/**
	 * The airscrews among Points, given which of them are painted blade (Blade, one a point; a point past its end is not)
	 * and the triangles over them (three point numbers each). A blade too small or too odd to read is left where it is.
	 */
	Found Find(const std::vector<Vec3>& Points, const std::vector<bool>& Blade, const std::vector<int>& Triangles);

	/**
	 * The paint of every point of a model file's near mesh (Content/Models/<name>.json, "near": {"paints": [...]}), read
	 * from the file's text without reading the rest of it. Empty if the file says none.
	 */
	std::vector<int> NearPaints(const std::string& FileText);

	/** The paint build_plane.py gives a blade. */
	constexpr int BladePaint = 9;

	/**
	 * Which of a model file's paints are an airscrew's blades, and which its own painted blur disc, by the file's
	 * paint_names (the 3D pre-check, 20 November 15:51Z: the newer Kate and Val paint each blade in three, blade,
	 * blade_back and blade_band, and carry a see-through blur disc of their own, blur and blur_tip): every paint whose
	 * name begins "blade", and every one beginning "blur". A file without names: paint 9 only, and no blur.
	 */
	struct PaintsAre
	{
		std::vector<bool> Blade, Blur; // by paint number
		bool IsBlade(int Paint) const { return Paint >= 0 && static_cast<size_t>(Paint) < Blade.size() && Blade[static_cast<size_t>(Paint)]; }
		bool IsBlur(int Paint) const { return Paint >= 0 && static_cast<size_t>(Paint) < Blur.size() && Blur[static_cast<size_t>(Paint)]; }
	};
	PaintsAre PaintKinds(const std::string& FileText);

	/**
	 * The paint of every point of a model file's far mesh ("far": {"paints": [...]}), read as NearPaints reads the near
	 * one's. Empty if the file has no far mesh. (The row "flyPropSpin", Brian 26 November 04:00Z: "the b-17 propellers
	 * didnt look like they were spinning". Past 300 m every aircraft is drawn from its far mesh, which kept every blade
	 * standing still and the newer models' own blur disc drawn solid.)
	 */
	std::vector<int> FarPaints(const std::string& FileText);

	/**
	 * A far mesh's airscrews taken out (the row "flyPropSpin"): its screws found as Find finds them in the near mesh
	 * (the same hubs, measured on every aircraft in Content/Models), every blade triangle of a found screw, and every
	 * triangle whose three corners are painted blur (the modeller's see-through disc, drawn solid in a far mesh), to be
	 * left out. Each screw's own blade triangles are kept apart, for an engine stopped with its blades feathered.
	 */
	struct Stripped
	{
		std::vector<Screw> Screws;
		/** By triangle: true where it goes. */
		std::vector<bool> Drop;
		/** By screw (as Screws): the numbers of its blade triangles. */
		std::vector<std::vector<int>> Blades;
		int BladeTriangles = 0, BlurTriangles = 0;
	};
	Stripped StripScrews(const std::vector<Vec3>& Points, const std::vector<int>& Paints, const PaintsAre& Are, const std::vector<int>& Triangles);

	/**
	 * Which of A's screws each of B's is (-1 none): the one whose hub lies within Share of its radius and whose radius is
	 * within Share of its own. Every one of B's matched to its own one of A's, and as many of each, or the answer is
	 * empty.
	 */
	std::vector<int> SameScrews(const std::vector<Screw>& A, const std::vector<Screw>& B, double Share = 0.05);

	/**
	 * A turning airscrew's disc (the row "flyPropSpin"), in its own Y-Z plane, the shaft along X, of radius Radius:
	 * rings at 0.12, 0.5, 0.9 and the whole of the radius, Round points about each, both faces; each point's colour and
	 * how much it hides of what is behind (Alpha, 0 to 255).
	 * - Near (bHaze false; the near shape's turning disc, 48 about): each blade trails a soft smear, darkest just behind
	 *   it and fading round to the next, with a sharp edge ahead of it and a faint haze between, so that its turn shows
	 *   from one frame to the next (the old disc changed the picture by 10 to 13 levels of 255 a frame at 60 fps).
	 * - Far (bHaze true; on the copies past 300 m, 24 about): an even, faint haze, as a film shows a fast airscrew far off,
	 *   where nothing turning can be seen (a B-17's disc is 4 to 8 pixels across at the Overview's 434 to 832 m).
	 */
	struct DiscMade
	{
		std::vector<Vec3> Points;
		std::vector<int> Triangles;
		std::vector<unsigned char> Red, Green, Blue, Alpha;
		/** The triangles' numbers before this are its front faces' (facing +X), the rest its back ones'. */
		size_t Front = 0;
	};
	DiscMade MakeDisc(double Radius, int Blades, bool bHaze);
	/** How much of what is behind the near disc hides at a point (0 to 1): Ahead its share of the way from the blade it
	 * is ahead of to the next (0 at a blade's edge); Ring 1 at half the radius, 2 at nine tenths. */
	double SmearAlpha(double Ahead, int Ring);
	/** The far haze's colour and how much it hides at each ring (0 at the hub's ring and the tips'); the near smear's
	 * colour. Both inks (M_Highlight, M_ScrewHaze) light a point with its colour's bytes as they are, not made linear
	 * as M_MapSurface makes them (build_content.py): 34 shows as a mid grey (0.13), so the smear is near black (8, 0.03)
	 * for its turn to show against the sky (the old disc's 30 changed the picture by 11 to 13 levels a frame). */
	constexpr unsigned char HazeRgb[3] = { 34, 34, 36 };
	constexpr unsigned char HazeAlpha[4] = { 0, 70, 60, 0 };
	constexpr unsigned char SmearRgb[3] = { 8, 8, 8 };
	constexpr double DiscRings[4] = { 0.12, 0.5, 0.9, 1.0 };
	constexpr int NearRound = 48, HazeRound = 24;
}
