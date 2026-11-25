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
 * Only the sea battle sets them (ASeaBattle::OpenOn, through AAirStage::SetShipPaint), each behind its own switch, and
 * never in the month's sea scene inside a campaign, which does not take the switches:
 * - bShade (-TheaterSeaShipLight=1): the point's colour multiplied by the file's own "shade" (its baked shadow, 1 open,
 *   less in a crease), as a figure's is (the soldiers' maker's worn cloth). Today's ship files carry no shade: until the
 *   models re-export with it (R1.3's models half), it changes nothing.
 * - bRustRamp (-TheaterSeaShipSkin=1): the one-colour rust replaced by a ramp, dark brown at its source, brown in its
 *   body, a thin brown-orange film at its foot, with a ragged edge; the salt as the art director ruled it; and a wooden
 *   deck's planks held under her 150,135,110 as drawn.
 * - TopLight (-TheaterSeaShipLight=1) is not a point's paint: it is the ships' material's own lift on what faces the sky,
 *   which the sea battle sets on its own steel ink (SeaTopLight, through SteelOf). It rides here so the options travel
 *   together.
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
	 * Her Mac check: if a deck no longer reads as its own plane, 0.12; if a steel deck is the brightest thing aboard, 0.08.
	 * The material lifts only where its Wear is on (1 + up x TopLight x metal x Wear), and the sea's steel ink has never
	 * had Wear: so the sea has drawn no top light at all, and this shows only with the skin's first look
	 * (-TheaterSeaShipSkin=1). Her pair is none (the skin alone) against 0.10 (the skin and the light), and the material
	 * cannot give a top light without its Wear's blot mottle too. */
	inline constexpr float SeaTopLight = 0.10f;
	/** The material's own (M_MapSurface's TopLight as Tools/build_content.py makes it): the map's, whose pieces have Wear.
	 * Never drawn at sea: the sea's ink has no Wear, and with the skin's Wear on, SteelOf sets SeaTopLightToday. */
	inline constexpr float MapTopLight = 0.26f;
	/** What the sea has drawn until now on its ships' steel: no top light (its ink's Wear is 0). The skin's first look keeps
	 * to it unless -TheaterSeaShipLight=1 asks for SeaTopLight, so the skin alone never brings in the map's 0.26. */
	inline constexpr float SeaTopLightToday = 0.0f;
	/** The first look at sea (R1.4's "free first look"): M_MapSurface's own Wear (its two-size blot mottle) on the sea
	 * battle's steel ink, under -TheaterSeaShipSkin=1. 0 today. */
	inline constexpr float SeaFirstLookWear = 1.0f;
	/** The material's chips held off at sea (its "ChipAt", -0.78 as made): its chip is metal (0.85), smoother than the paint
	 * (0.34) and coloured "ChipColour", so a rust-coloured chip draws as a near-black, glossy fleck, never as the dark of a
	 * rust source. At -2 nothing chips whatever the noise (the most its sum reaches is 1.5 + 0.3 + ChipAt). Rust-coloured
	 * chips, matt and not metal, are the new ship material's (R1.4). */
	inline constexpr float SeaChipAt = -2.0f;
	/** The blot mottle at its most (M_MapSurface: the colour times 0.8 + 0.42 of the blot, by Wear on painted metal, and by
	 * half of it on what is not metal), and so on a wooden deck's planks at the first look's wear: 1.11. */
	inline constexpr float MottleMost = 0.8f + 0.42f;
	inline constexpr float WoodMottleMost = 1.0f + (MottleMost - 1.0f) * 0.5f * SeaFirstLookWear;
	/** The art director's wooden deck (25 November 16:05Z): under the skin, never over 150,135,110 as drawn (sRGB). */
	inline constexpr float DeckR = 150.0f, DeckG = 135.0f, DeckB = 110.0f;

	/** What the sea battle sets on its own steel ink (an instance of M_MapSurface; AAirStage::SetShipPaint) for the options
	 * and the first look's wear. */
	struct Steel
	{
		/** TopLight to set; below 0, left as the material makes it (0.26, which lifts nothing while Wear is 0). */
		float TopLight = -1.0f;
		/** Wear to set (the blot mottle, and the lift by TopLight); 0, left as made (0: no mottle, no lift, no chips). */
		float Wear = 0.0f;
		/** ChipAt set to SeaChipAt: no chips. */
		bool bChipsOff = false;
	};

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
		/** A wooden deck's planks (the reader's "planks", by the file's paint names). */
		bool bDeckWood = false;
	};

	/** The point's colour (linear, as the reader turns it into the mesh's colour). */
	Linear Paint(const Point& P, const Palette& Pal, const Options& O);

	/** The sea battle's steel ink for these options and the first look's wear (FirstLookWear, 0 for none): the top light
	 * where asked, else none (SeaTopLightToday) once the first look's Wear is on, else left; the Wear; and with it the chips
	 * held off. With every switch off, nothing is set. */
	Steel SteelOf(const Options& O, float FirstLookWear);

	/** A wooden deck's most under the skin (linear, each channel): the art director's 150,135,110 with room for the blot
	 * mottle's 1.11, rounded down to a whole sRGB level, so the deck as drawn stays under it however the bytes are read. */
	Linear DeckWoodMost();

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
