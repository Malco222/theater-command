#pragma once

#include <cstdint>
#include <string>

/**
 * A ship's paint, point by point (the ship detail plan, round 1: R1.3's scene half, R1.4's rust). The ship branch of
 * FMiniatureMesh::Sculpted (Campaign/CampaignMiniatures.cpp) reads a battle model's file (tools/battle-models, kind
 * "ship") and asks Paint here for the colour of each of its points: the file's paint, rusted where the file says it is
 * worn, whitened low down where it says salt has dried, darkened where it says soot lies. Plain C++, no engine, so the
 * cloud test (Tools/ship_detail/ship_paint_test.cpp) holds it to every ship file the game has.
 *
 * TODAY'S PAINT, BIT FOR BIT. With Options left as they are made (every switch off) Paint is the ship branch's own sum as
 * it stood at 55419eb6, operation for operation, in the engine's own linear floats: one colour of rust laid over the
 * coat by 0.55 of the wear, the reader's pale salt by 0.45 of the salt, the file's black by 0.6 of the soot. The campaign
 * map, Pearl Harbor and Midway (the air stage) and every air battle never set an option, so what they draw is today's.
 * Only the sea battle sets them (ASeaBattle::OpenOn, through AAirStage::SetShipPaint), each behind its own switch:
 * - bShade (-TheaterSeaShipLight=1): the point's colour multiplied by the file's own "shade" (its baked shadow, 1 open,
 *   less in a crease), as a figure's is (the soldiers' maker's worn cloth). Today's ship files carry no shade: until the
 *   models re-export with it (R1.3's models half), it changes nothing.
 * - bRustRamp (-TheaterSeaShipSkin=1): the one-colour rust replaced by a ramp, dark brown at its source, brown in its
 *   body, a thin brown-orange film at its foot, with a ragged edge; and the salt as the art director ruled it.
 * - TopLight (-TheaterSeaShipLight=1) is not a point's paint: it is the ships' material's own lift on what faces the sky,
 *   which the sea battle sets on its own steel ink (SeaTopLight). It rides here so the options travel together.
 *
 * THE ART DIRECTOR'S NUMBERS (25 November 2026, 16:05Z, art-director/ship-detail/ship-weathering-numbers.md, which
 * supersede the plan's starting figures): rust is told by being dark and warm. In sRGB, laid over the coat:
 *   the source, at the fitting: 58,36,28, at 80 to 100%;
 *   the body of the run:        96,58,38, at 45 to 70%;
 *   the film at its foot:       124,80,50, at no more than 35%.
 * Never red above 135, never red more than 80 over blue, never lighter than the paint under it: at the source no more
 * than half the paint's brightness, at the foot still a tenth darker (on a dark paint, deck blue, boot-topping, the
 * darkest greys, the rust shows by its warmth at the paint's own brightness). Edge wear goes darker, never whiter.
 * Salt: the coat taken toward 196,200,198 by no more than 30%, and never over a rust source.
 *
 * WHERE ON THE RUN. A file's wear at a point is how strong the stain is there: a weep's ribbon carries the most at its
 * head and fades to nothing at its foot (battle-models' ships.py, the U-boat's streaks), and a chip on an edge carries a
 * little. So the wear is read as the place on the ramp: much wear, the source; a little, the film. What makes a run
 * read as a stain and not as a painted stripe is that it is never even: a body-space hash of the point (its place on the
 * hull, to the millimetre, never a draw of the battle's own) roughens the film's edge, and a slower one along the hull
 * gives each run its own strength and tone, so no two neighbours are alike. The same file is painted the same way every
 * time it is read.
 */
namespace ShipPaintCore
{
	/** A colour in the engine's linear light (as FLinearColor holds it: four floats, red, green, blue, alpha). */
	struct Linear
	{
		float R = 0.0f, G = 0.0f, B = 0.0f, A = 1.0f;
	};

	/** The ships' top light at sea (the art director, 25 November 16:05Z: "one number for every ship in the sea scene"):
	 * the lift M_MapSurface gives to what faces the sky, set on the sea battle's own steel ink under -TheaterSeaShipLight=1.
	 * The map's own is 0.26. Her Mac check: if a deck no longer reads as its own plane, 0.12; if a steel deck is the
	 * brightest thing aboard, 0.08. The material lifts only where its Wear is on (metal times wear), so at sea it shows
	 * with the skin's first look (-TheaterSeaShipSkin=1) and not without it. */
	inline constexpr float SeaTopLight = 0.10f;
	/** The material's own (M_MapSurface's TopLight as Tools/build_content.py makes it): the map's, and the sea's today. */
	inline constexpr float MapTopLight = 0.26f;
	/** The first look at sea (R1.4's "free first look"): M_MapSurface's own Wear (body-space chips and a two-size blot
	 * mottle) on the sea battle's steel ink, under -TheaterSeaShipSkin=1. 0 today. */
	inline constexpr float SeaFirstLookWear = 1.0f;

	/** What the sea battle asks of its ships' paint. As made: today's, everything off. */
	struct Options
	{
		/** Multiply by the file's "shade" (R1.3). */
		bool bShade = false;
		/** The rust ramp and the ruled salt (R1.4). */
		bool bRustRamp = false;
		/** The ships' top light on the sea battle's steel ink; below 0, the material's own (not set). Not a point's paint. */
		float TopLight = -1.0f;
	};

	/** Whether options paint a point as today (every switch off as far as the points go: the top light is the ink's). */
	inline bool PaintsAsToday(const Options& O)
	{
		return !O.bShade && !O.bRustRamp;
	}

	/** The options' mark on the painted model's cache key: nothing for today's paint (so the map, Pearl and Midway keep
	 * their keys exactly), else what the points were painted with. */
	inline std::string KeyOf(const Options& O)
	{
		if (PaintsAsToday(O))
		{
			return std::string();
		}
		return std::string("|shippaint") + (O.bShade ? "-shade" : "") + (O.bRustRamp ? "-rust" : "");
	}

	/** A ship file's colours that are not a point's own paint (the reader's, in the engine's linear light). */
	struct Palette
	{
		/** The file's "rust", or the reader's 8A4E28. */
		Linear Rust;
		/** The reader's dried salt (214,210,198). */
		Linear Salt;
		/** The file's eighth paint, its black, for soot; bSoot: the file has more than eight paints. */
		Linear Soot;
		bool bSoot = false;
	};

	/** One point of a ship file, as the reader has it. */
	struct Point
	{
		/** Its paint's own colour (the file's, linear). */
		Linear Paint;
		/** Painted steel that rusts and salts (the reader's coat paints: hull, upper, deck, roof, door, seam, and what lies on
		 * them, upper.on and hull.on). */
		bool bPainted = false;
		/** The file's wear there, 0 to 100 (the reader clamps it). */
		int32_t Wear = 0;
		/** The file's salt and soot there as the file gives them; below 0, the file gives none. */
		int32_t Salt = -1;
		int32_t Soot = -1;
		/** The file's shade there, 0 to 1 (the reader clamps it); below 0, the file gives none. */
		float Shade = -1.0f;
		/** Where the point lies on her, in the reader's frame (metres: her bow toward -X, her right hand at -Y, up +Z). */
		double X = 0.0, Y = 0.0, Z = 0.0;
	};

	/** The point's colour (linear, as the reader turns it into the mesh's colour). */
	Linear Paint(const Point& P, const Palette& Pal, const Options& O);

	// ---- What the ramp is made of, for the cloud test to hold it to the art director's numbers.

	/** The ramp's three steps in sRGB (0-255) and how much of each is laid over the coat. */
	struct Step
	{
		float R, G, B;
		float LeastOver, MostOver;
	};
	inline constexpr Step Source{ 58.0f, 36.0f, 28.0f, 0.80f, 1.00f };
	inline constexpr Step Body{ 96.0f, 58.0f, 38.0f, 0.45f, 0.70f };
	inline constexpr Step Foot{ 124.0f, 80.0f, 50.0f, 0.00f, 0.35f };
	/** The source's own range (each run's source toned within it, so that no two neighbours are alike). */
	inline constexpr Step SourceDark{ 52.0f, 34.0f, 28.0f, 0.80f, 1.00f };
	inline constexpr Step SourceLight{ 66.0f, 42.0f, 32.0f, 0.80f, 1.00f };
	/** The ruled salt: toward this (sRGB), by no more than this much. */
	inline constexpr float SaltR = 196.0f, SaltG = 200.0f, SaltB = 198.0f, SaltMost = 0.30f;

	/** How a rusted point was laid (for the test): where on the ramp (0 the foot's end, 1 the source), the ramp's colour
	 * there before it is laid over the coat (linear), how much of it is laid, and the brightness it may not pass (a
	 * share of the paint's own). */
	struct Laid
	{
		float Along = 0.0f;
		Linear Colour;
		float Over = 0.0f;
		float Ceiling = 1.0f;
	};
	/** The ramp at a point that rusts (P.bPainted and P.Wear above 0), and the coat it leaves. */
	Linear Ramp(const Point& P, Laid* Out = nullptr);

	/** sRGB (0-1) to linear and back, as the standard has them (the ramp's own steps; the engine's tables agree to well
	 * under one level). */
	float ToLinear(float Srgb);
	float ToSrgb(float Lin);
	/** Rec. 709 brightness of a linear colour. */
	float Brightness(const Linear& C);
}
