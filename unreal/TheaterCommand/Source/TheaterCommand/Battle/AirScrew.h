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
}
