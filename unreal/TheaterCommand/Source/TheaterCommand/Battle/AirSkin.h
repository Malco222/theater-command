#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace AirUnitColours
{
	struct Model;
}

/**
 * The finer B-17's worn paint (the Battle rules card's "Worn B-17 paint", row "flyB17Skin"; Brian, 26 November 2026:
 * "i want the new ones in the game"; the look he approved on 25 November). Her shape stays Content/Models/B17Fine.json;
 * the paint the model thread made for that same shape is laid over it from Content/Data/AirSkin/B17Fine.json, which
 * Tools/make_air_skin.py extracts from the model thread's own file (it computes nothing of its own):
 * - colours: her 46 paints' colours (the one army olive 5B5A44 over 9A9EA1, dark glass, ...);
 * - near and far, one number a point, each 0 to 100: blend (paint 0 toward paint 1) and soot (any paint toward paint
 *   11), read as the game reads the file's own (CampaignMiniatures: blend/100, 0.72 x soot/100, mixed linear);
 * - near and far, repaint: [point, paint, ...], points her skin paints otherwise (far: the airscrew domes, engine to hub);
 * - near and far, shade: her contact shade x 100, multiplied in (linear) under the row with -TheaterAirB17SkinShade;
 * - panel_blend: the median blend of her near paint-0 points (the nose-art panel's under-colour, PanelUnder);
 * - the sha256 of the model thread's file and of the base, and the base's size and FNV-1a 64 (the cloud test's pin).
 * Never marks (her own star and bar stays), never anything by month or year: Read refuses a key that names one.
 *
 * The overlay is read only when the row is on, as an air battle begins (AAirBattle::B17SkinRead), and Set there; with
 * the row off Set(nullptr), and nothing here is read or asked by any shape (Active() is false). Plain C++, so that the
 * cloud test (Tools/air_show/air_b17_skin_test.cpp) reads the same file by the same law.
 */
namespace AirSkin
{
	/** One mesh's (near or far) numbers, one a point. */
	struct Level
	{
		std::vector<uint8_t> Blend, Soot, Shade;
		/** (point, paint) where the skin paints a point otherwise, by point, rising */
		std::vector<std::pair<uint32_t, uint8_t>> Repaint;
		size_t Count() const { return Blend.size(); }
		/** The point's paint under the skin: the repaint's where there is one, else the base's. */
		int PaintAt(size_t Point, int BasePaint) const;
	};

	/** A point the tool wrote down with the colour the game's law gives it (sRGB 0xRRGGBB), to tie the two together. */
	struct Check
	{
		bool bFar = false;
		size_t Point = 0;
		int Paint = 0;
		uint32_t Rgb = 0, Shaded = 0;
	};

	struct Overlay
	{
		std::string Of;           // the model it is laid on ("B17Fine")
		std::vector<uint32_t> Colours; // 0xRRGGBB a paint
		Level Near, Far;
		std::string BaseSha256, SourceSha256;
		uint64_t BaseBytes = 0, BaseFnv64 = 0;
		/** the median blend of the near mesh's paint-0 points (the tool's panel_blend): PanelUnder's */
		int PanelBlend = 0;
		/** How strongly the contact shade is laid, 0 (none) to 1 (the model thread's whole shade): not in the file, set
		 * by the game as the battle begins (-TheaterAirB17SkinShade=0..1, 1 unless asked), for stills with and without. */
		double ShadeStrength = 1.0;
		std::vector<Check> Checks;
		const Level& LevelOf(bool bFar) const { return bFar ? Far : Near; }
	};

	/**
	 * Reads an overlay: its near and far as long as NearCount and FarCount, PaintCount colours, every number an integer
	 * 0 to 100, each repaint inside the points and paints; false (and why) otherwise, or where any key anywhere names a
	 * month, a year, a scheme, a mark or a "from".
	 */
	bool Read(const std::string& Text, size_t NearCount, size_t FarCount, size_t PaintCount, Overlay& Out, std::string& Why);

	/** The overlay the battle wears (nullptr: none, the row off). Kept in an atomic: the reader serves every model. */
	void Set(const Overlay* Now);
	bool Active();
	/** The overlay for a model by its file's name, or nullptr (none set, or another model's). */
	const Overlay* For(const std::string& ModelName);

	/** The game's law for an aircraft's point (CampaignMiniatures.cpp, its aircraft branch), in doubles: the paint's
	 * colour, paint 0 toward paint 1 by Blend/100, then toward paint 11 by 0.72 x Soot/100, mixed linear; times Shade
	 * (linear, 1 for none); written as sRGB bytes. */
	uint32_t Law(const std::vector<uint32_t>& Colours, int Paint, int Blend, int Soot, double Shade = 1.0);

	/** What a point's colour is multiplied by (linear) for its contact shade (0 to 100) at a strength (0 to 1, clamped):
	 * 1 - Strength x (1 - Shade/100); 1 where the shade is 100 or the strength 0. */
	double ShadeFactor(int Shade, double Strength);

	/** The earned colours' model (Battle/AirUnitColours) under the skin: its colours, its near and far blend and soot,
	 * and its repainted points. False and the model unchanged where the sizes do not match. */
	bool OntoModel(const Overlay& O, AirUnitColours::Model& M);

	/** The nose-art panel's under-colour under the skin (what a painting fades into when small): paint 0 toward paint 1
	 * at the median blend of the near mesh's paint-0 points (panel_blend), mixed linear, 0xRRGGBB. */
	uint32_t PanelUnder(const Overlay& O);

	/** FNV-1a, 64 bits. */
	uint64_t Fnv64(const std::string& Bytes);
}
