#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * The finer B-17's paint sheen (the Battle rules card's "B-17 paint sheen", row "flyB17Sheen", off unless asked; it
 * needs "Worn B-17 paint"). Brian, 26 November 2026: "make the new ones work"; the look he approved on 25 November
 * (battle-models/air/b17-next-look/jump/skin-3-in-flight.jpg: worn olive with a cool satin sheen on her wing tops and
 * back, glinting glass). Her paint today is every aircraft's flat ink (PlaneInk: roughness 0.9, specular 0.15); the
 * model thread's file gives each of her points its own finish, `gloss` (roughness x 100) and `metal` (x 100), which
 * Tools/make_air_skin.py --sheen-out copies into Content/Data/AirSkin/B17Fine-sheen.json, in the same run as her
 * overlay (Battle/AirSkin) and from the same bytes.
 *
 * The game carries a point's finish to the pixel in the mesh's first texture coordinate (Carry): U, how far her
 * roughness is below the untouched 0.9 (times the square root of her contact shade), and V, her metal. The material
 * M_AirPaint (Tools/build_content.py, build_map_surface(air=True)) turns them into roughness, specular and metal by
 * Law, the same sums as its nodes; at U = V = 0 it gives PlaneInk's numbers exactly, so a point with nothing carried
 * (an earned-colours copy, a marking) is drawn as today. Where a low sun would mirror in her skin, the GLARE GUARD
 * (GuardOf) puts her finish back to PlaneInk's, the one the art director passed (no sand); the grazing guard is off
 * unless asked. Every number is a launch word (-TheaterAirB17SheenInk=gain,floor,spec,metal,glare,graze,metalfrom).
 *
 * Plain C++, so that the cloud test (Tools/air_show/air_b17_sheen_test.cpp) reads the same file by the same law.
 * Nothing here depends on a date, a random draw or a counted result.
 */
namespace AirSheen
{
	/** Roughness of a point with nothing of its own: PlaneInk's. */
	constexpr double Untouched = 0.9;

	/** The material's numbers (M_AirPaint's scalar parameters "Sheen..."). Edge is never set by the game: it is the
	 * stale-asset sentinel, read from the asset as it is built. */
	struct Tune
	{
		double Gain = 1.0, Floor = 0.45, Spec = 0.25, Metal = 1.0, Glare = 1.0, Graze = 0.0, MetalFrom = 0.5;
		double LowSun = 0.5, GlassBelow = 0.12, Edge = 20.0;
	};

	struct Vec3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};

	/** How much of her finish goes back to PlaneInk's at a point facing N, seen from E (toward the eye), the sun toward S
	 * (none of them normalised here, as the material's nodes do not): sat(max(glare, graze) x low), where low is 1 with
	 * the sun under some 15 degrees and 0 over 30, glare is 1 within some 25 degrees of the sun's mirror (x Glare), and
	 * graze is 1 for a look along her skin (x Graze, 0 unless asked). 0 for S = (0,0,0). Never NaN for finite input. */
	double GuardOf(const Vec3& N, const Vec3& E, const Vec3& S, const Tune& T);

	/** What one point carries in the first texture coordinate. */
	struct Carried
	{
		float U = 0.0f, V = 0.0f;
	};

	/** U = clamp(0.9 - Gloss/100, 0, 0.9) x sqrt(clamp(ShadeFactor, 0, 1)), her glass (Gloss under 12) not shaded;
	 * V = clamp(Metal/100, 0, 1). */
	Carried Carry(int Gloss, int Metal, double ShadeFactor);

	struct Finish
	{
		double Rough = Untouched, Spec = 0.15, Metal = 0.0;
	};

	/** M_AirPaint's sums (its "if air:" block, in order): her roughness, specular and metal from what she carries (U, V),
	 * the flat ink's (InkRough, InkSpec), the tune, and the guard (GuardOf). Law(0, 0, ...) is (InkRough, InkSpec, 0)
	 * exactly, whatever the tune and the guard; Guard 1 gives the same, whatever U and V. */
	Finish Law(double U, double V, double InkRough, double InkSpec, const Tune& T, double Guard);

	/** The launch word's numbers, "gain,floor,spec,metal,glare,graze,metalfrom", each optional (empty keeps it): each
	 * clamped (gain 0..1.5, floor 0..0.9, spec 0..0.5, metal 0..1.5, glare 0..1, graze 0..1, metalfrom 0..1). Why is
	 * set for a word that is not a number (T keeps its number there), for a number out of range (T takes it clamped)
	 * and for one too many; true when nothing needed saying. */
	bool ParseTune(const std::string& Words, Tune& T, std::string& Why);

	/** One mesh's (near or far) finish, one a point, each 0 to 100. */
	struct Level
	{
		std::vector<uint8_t> Gloss, Metal;
		size_t Count() const { return Gloss.size(); }
	};

	struct Sheen
	{
		std::string Of, BaseSha256, SourceSha256;
		uint64_t BaseFnv64 = 0;
		/** Her coat's own finish (the medians over her near paint-0 points): what a point the file does not hold wears
		 * (her star and bar, laid after her file's points). */
		int CoatGloss = 90, CoatMetal = 0;
		Level Near, Far;
		/** The material's numbers for this run (the launch word's, else the defaults): not in the file. */
		Tune Look;
		const Level& LevelOf(bool bFar) const { return bFar ? Far : Near; }
	};

	/**
	 * An aircraft whose own model file carries her finish point by point: the Mustang's bare metal (battle-models
	 * air-bar-1126 issue-13, row "flyMustangMetal"; Brian, 27 November: "it should be shiny silver", "metalic"). Near
	 * and far "gloss" and "metal" (whole numbers 0 to 100) and "shade" (her contact shade with her panels' tone, 0 to 1)
	 * one a point, as the file lists them beside its paints. A point the file does not hold (her star and bar and her
	 * number, laid after her file's points) wears paint: satin, no metal (the art director, 27 Nov 03:21Z: "paint stays
	 * paint: metal 0, satin at most").
	 */
	struct OwnFinish
	{
		Level Near, Far;
		std::vector<float> NearShade, FarShade;
		int MarkGloss = 50, MarkMetal = 0;
		/** The material's numbers for this run (BareTune, or the launch word's): not in the file. */
		Tune Look;
		const Level& LevelOf(bool bFar) const { return bFar ? Far : Near; }
		const std::vector<float>& ShadeOf(bool bFar) const { return bFar ? FarShade : NearShade; }
	};

	/** Bare metal's numbers on M_AirPaint (the art director, 27 Nov 04:58Z): a floor of its own at 0.25, not the B-17's
	 * 0.45 (her pick is roughness 0.28; 0.32 is the fall-back if the Mac shows sparkle); no glare guard on bare skin;
	 * the rest as the B-17's. */
	Tune BareTune();

	/** Reads OwnFinish from a model file's "near" and "far": gloss, metal and shade, each one a point of its "paints";
	 * false (and why) for a list missing, of another length, a gloss or metal not a whole number 0 to 100, or a shade
	 * not a number 0 to 1. Nothing else in the file is read. */
	bool ReadOwn(const std::string& Text, OwnFinish& Out, std::string& Why);

	/** Reads the sheen file: sheen_of "B17Fine", the two sha256s and the base's FNV-1a 64, coat, near and far gloss and
	 * metal of NearCount and FarCount whole numbers 0 to 100; false (and why) otherwise, or where any key anywhere names a
	 * month, a year, a scheme, a mark or a "from". */
	bool Read(const std::string& Text, size_t NearCount, size_t FarCount, Sheen& Out, std::string& Why);
}
