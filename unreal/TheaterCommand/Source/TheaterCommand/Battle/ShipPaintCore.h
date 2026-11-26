#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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
 * - bShade (-TheaterSeaShipLight= on, at any top light above 0): the point's colour multiplied by the file's own "shade"
 *   (its baked shadow, 1 open, less in a crease), as a figure's is (the soldiers' maker's worn cloth). Today's ship files
 *   carry no shade: until the models re-export with it (R1.3's models half), it changes nothing.
 * - bRustRamp (-TheaterSeaShipSkin=1): the one-colour rust replaced by a ramp, dark brown at its source, brown in its
 *   body, a thin brown-orange film at the foot of a run with body above it (never on her salt nor beside it), with a
 *   ragged edge; light wear away from a run at the paint below a wear of 5, and from 5 a little darker and greyer than
 *   the point unworn, never rust-coloured; no channel of a stained point above the paint's own but where salt pales it,
 *   and then never pink; the salt as the art director ruled it, under a run's body and over light wear; and a wooden
 *   deck's planks held under her 150,135,110 as drawn.
 * - TopLight (-TheaterSeaShipLight=: 1 or on for SeaTopLight, 0.10, or a top light of its own from 0.0 to 0.30) is not a
 *   point's paint: it is the ships' material's own lift on what faces the sky, which the sea battle sets on its own steel
 *   ink through SteelOf. It rides here so the options travel together.
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
 * HER CORRECTION AFTER THE ROUND 1 AFTER BOARDS (25 November 2026, 20:28Z, "light wear is not rust"; it supersedes the
 * 16:05Z sheet where they differ). The ramp put its film on every lightly worn point, so the U-boat's band between her
 * rails went from 106,107,111 to 108,99,99 and the battleship's side from 72,87,104 to 74,82,95: red went up, a pink
 * wash. Under the skin, now:
 *   1. no channel of a stained point (painted, worn) above the paint's own (Held, on what the point shows once rust,
 *      light wear, salt and soot are laid). Salt is a deposit and may be paler (her 16:05Z sheet), so where her salt lies
 *      a point may rise above the paint, but only in every channel at once and never past that salt over the clean
 *      paint: a point below the paint in any channel is above it in none, so red never rises while blue falls (the pink
 *      the round 1c review found where salt lay on rust: 106,114,120 to 110,101,99, now 106,101,99); rust the salt lifts
 *      above the paint is no warmer than the salt makes the clean paint, and light wear no warmer than rule 2 allows. The
 *      rust under the salt is held to the paint's own first;
 *   2. wear under 20 of 100 away from a run takes no rust colour (LightWear): against the same point unworn (the paint,
 *      with her salt and the soot where the file lays them), 3 to 6% darker, up to 15% greyer, red minus blue up by 3 at
 *      most, in sRGB levels as she measures, and so in the mesh's own bytes: the point is stored as whole levels chosen
 *      inside her bounds, since on a near-grey paint (126,131,129) one level of rounding is more than 15% of its colour;
 *   3. the film, 124,80,50, only at the foot of a run that has body above it (BodyAbove finds them; Ramp lays the film
 *      only there, and elsewhere starts the ramp at its body). The source and body are as ruled before.
 *
 * HER RULING ON THE RE-SHOT 04 AND 02 (26 November 2026, 02:22Z; it keeps the 20:28Z rules and settles three things):
 *   - the salt, in one order: a run's body (wear of BodyOverSaltWear, 40, and more) beats salt: no salt lift there, the
 *     body shows as ramped, held to rule 1; salt beats light wear: a lightly worn point (under 20, no body above) under
 *     her salt shows that salt as it lies on the same point unworn, paler in every channel together and never warmer
 *     than her salt makes the clean paint; between them (wear 20 to 39, and a run's foot under 20) the salt is laid as
 *     round 1c laid it, a pale lift in every channel together, no warmer than her salt on the clean paint. The unworn
 *     point under her salt is held the same way, so where soot lies on the salt it never reads warmer nor pink;
 *   - light wear stays at the paint below a wear of 5 (LightWearFrom): such a point paints as the same point unworn, bit
 *     for bit, and so does light wear under her salt (PaintsAsUnworn), its finish byte too (SteelFinish). From 5 it is
 *     darker by 3% rising evenly to 6% at 19 (LightDarkAt), within LightDarkBand of that, as neutral as before: up to
 *     15% greyer, red less blue up by 3 at most, no channel above the paint;
 *   - the film is the weakest coat: never on a point her salt lies on, nor on one that shares a triangle with it
 *     (BodyAbove's SaltMark). Such a point takes what it takes with no film: the ramp from its body;
 *   - and her call 3's heading, hold the rust beside salt (warm over pale reads salmon): every point her salt lies on is
 *     never warmer than her salt makes the clean paint, red less blue in the whole levels the mesh stores, lifted by the
 *     salt or not. A rust point of wear 20 to 39 (or a run's foot) that her salt cannot lift in every channel is held by
 *     rule 1 at the paint in red with green and blue below it, warmer than the salt beside it; there only red comes
 *     down, to her salt's room over the point's own blue (Submarine_GER's point 7347, wear 20 under a salt of 23:
 *     106,104,104 before, now 92,104,104 on her 106,114,120), so no corner beside her pale salt is warmer than it.
 *
 * WHERE ON THE RUN. A file's wear at a point is how strong the stain is there: a weep's ribbon carries the most at its
 * head and fades to nothing at its foot (battle-models' ships.py, the U-boat's streaks), and a chip on an edge carries a
 * little. So the wear is read as the place on the ramp: much wear, the source; a little, the film, but only at the foot of
 * a run with its body above it (below 20 of 100 anywhere else is light wear, not rust). What makes a run
 * read as a stain and not as a painted stripe is that it is never even: a body-space hash of the point (its place on the
 * hull, to the millimetre, never a draw of the battle's own) roughens the film's edge, and a slower one along the hull
 * gives each run its own strength and tone, so no two neighbours are alike. The same file is painted the same way every
 * time it is read.
 *
 * A RUN, AND WHAT LIES ABOVE (rule 3). Her own up axis is her model's +Z: the file's z, which the reader keeps (it turns
 * x and y about it, never z), with her keel her draught below z 0, her waterline at 0 and her decks facing +Z; the
 * paint is laid on the model in that frame, before the scene rolls or pitches her. A run is a patch of her stained skin:
 * painted points with wear above 0, joined by the sides of the mesh's own triangles, points at one place (to the
 * millimetre, where the mesh splits a point at a hard edge) counted as one. Its body is its points with wear of 20 and
 * more (RunBodyWear). A run goes down: a point lies at the foot of a run with body above it when a body point of its
 * own patch lies higher in Z, by 1 mm to 3 m (RunLongestMm, her longest run), more above it than beside it (45 degrees
 * or steeper), no further to the side than half a run's foot is wide, 22.5 cm (her foot at most three times the widest
 * source, 15 cm, so 45 cm across), plus the rake, tan 10 degrees of the drop, and on the same face of the plate (their
 * facings not turned more than a right angle apart, so a plate's underside never takes the film of its top). A
 * U-boat's weeps are ribbons on her casing, apart from it, so the casing's light wear between them is no run's foot; a
 * hawse stain's foot is the hull just under its body.
 */
namespace ShipPaintCore
{
	/** A colour in the engine's linear light (as FLinearColor holds it: four floats, red, green, blue, alpha). */
	struct Linear
	{
		float R = 0.0f, G = 0.0f, B = 0.0f, A = 1.0f;
	};

	/** The ships' top light at sea (the art director, 25 November 16:05Z: "one number for every ship in the sea scene"):
	 * the lift M_MapSurface gives to what faces the sky, set on the sea battle's own steel ink by -TheaterSeaShipLight=1 (or
	 * =on). Her one Mac pair is 0.26 against 0.10; if a deck no longer reads as its own plane, 0.12; if a steel deck is the
	 * brightest thing aboard, 0.08. -TheaterSeaShipLight= takes any of them as a number too (0.0 to SeaTopLightMost).
	 * The material lifts by up x TopLight x metal x Wear, and the sea's steel ink has never had Wear, so the sea has drawn no
	 * top light at all. SteelOf gives the ink the Wear the lift needs, with or without the skin: with the skin, the first
	 * look's own (SeaFirstLookWear); with the light alone, so little (SeaLightAloneWear) that its blot mottle cannot be seen,
	 * and TopLight raised to match, so the lift is the same either way, and the same as the map's sum gives for that light
	 * at a Wear of 1: 1 + the light x the metal. Unworn steel is marked alpha 128 by the reader, which the material reads as
	 * 254/255 metal, so an unworn steel deck facing the sky is lifted by 254/255 of the light (1.0996 at 0.10, 1.2590 at
	 * 0.26); a worn one (alpha 127 and below, all metal) by the light itself (1.10, 1.26). */
	inline constexpr float SeaTopLight = 0.10f;
	/** The most top light -TheaterSeaShipLight= takes as a number (anything over it is refused and taken as off). */
	inline constexpr float SeaTopLightMost = 0.30f;
	/** The material's own (M_MapSurface's TopLight as Tools/build_content.py makes it): the map's, whose pieces have Wear.
	 * Never drawn at sea: with every switch off the sea's ink has no Wear, and with either ship switch on SteelOf sets the
	 * ink's TopLight itself. */
	inline constexpr float MapTopLight = 0.26f;
	/** What the sea has drawn until now on its ships' steel: no top light (its ink's Wear is 0). The skin's first look keeps
	 * to it unless -TheaterSeaShipLight= asks for a top light, so the skin alone never brings in the map's 0.26. */
	inline constexpr float SeaTopLightToday = 0.0f;
	/** The first look at sea (R1.4's "free first look"): M_MapSurface's own Wear (its two-size blot mottle) on the sea
	 * battle's steel ink, under -TheaterSeaShipSkin=1. 0 today. */
	inline constexpr float SeaFirstLookWear = 1.0f;
	/** The Wear the light alone sets (-TheaterSeaShipLight= without -TheaterSeaShipSkin=1), only so the top light can show.
	 * In M_MapSurface the Wear does three things: it lets the lift through (1 + up x TopLight x metal x Wear), it lays the
	 * blot mottle (the colour times a mix of 1 and 0.8 + 0.42 of the blot, by Wear x (metal/2 + 1/2)), and it lets the chips
	 * through (times Wear; held off by ChipAt at SeaChipAt anyway). At 0.01, with TopLight set to the light divided by it,
	 * the lift is what a Wear of 1 gives with the light itself (1 + the light x the metal), while the mottle moves a colour
	 * by at most 0.22 in 100 (0.01 x 0.22): less than one level of 255 anywhere, so the light alone looks like the light
	 * alone, not like the skin. */
	inline constexpr float SeaLightAloneWear = 0.01f;
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

	/** Her correction of 25 November 20:28Z (above). The wear from which a point is rust, a run's body or its source;
	 * below it, light wear, which takes the film only at the foot of a run with body above it. */
	inline constexpr int32_t RunBodyWear = 20;
	/** Light wear away from a run (rule 2), her bounds in sRGB levels, against the same point unworn: darker by 3 to 6% (in
	 * brightness, the Rec. 709 sum of the levels), greyer by 0 to 15% (its colour, the most of its channels less the least,
	 * for its brightness), red less blue up by 3 levels at most. */
	inline constexpr float LightDarkLeast = 0.03f, LightDarkMost = 0.06f, LightGreyMost = 0.15f, LightWarmMost = 3.0f;
	/** Her ruling of 26 November: light wear below this wear stays at the paint (the point as it is unworn, bit for bit);
	 * from it, darker by LightDarkFrom, rising evenly to LightDarkTo at a wear of 19. */
	inline constexpr int32_t LightWearFrom = 5;
	/** Light wear's darkening, against the same point unworn: 3% at a wear of 5 to 6% at 19, a straight line (LightDarkAt),
	 * and the band about that line the stored whole levels keep to (a fifth of a percent each way; never past 3 or 6%).
	 * Its greying aims at 1% to 14% as the wear grows from 1 to 19, as round 1c's did (less where red less blue would rise
	 * further). The whole levels stored are the nearest to that aim that keep every one of her bounds and the band in the
	 * bytes themselves (LightWear). */
	inline constexpr float LightDarkFrom = 0.03f, LightDarkTo = 0.06f, LightDarkBand = 0.002f, LightGreyFrom = 0.01f, LightGreyTo = 0.14f;
	/** Her order for the salt (26 November): from this wear a run's body beats her salt, which lifts it no further. */
	inline constexpr int32_t BodyOverSaltWear = 40;
	/** BodyAbove's marks, a bit each: at the foot of a run with its body above (FootMark), and, on such a point, her salt
	 * on it or on a point that shares a triangle with it, so the film never lies there (SaltMark). */
	inline constexpr uint8_t FootMark = 1, SaltMark = 2;
	/** The darkening light wear aims at, at a wear of LightWearFrom to 19 (her straight line from 3% to 6%; 0 below it). */
	inline constexpr float LightDarkAt(int32_t Wear)
	{
		return Wear < LightWearFrom ? 0.0f
			: LightDarkFrom + (LightDarkTo - LightDarkFrom) * static_cast<float>((Wear < RunBodyWear - 1 ? Wear : RunBodyWear - 1) - LightWearFrom) /
				static_cast<float>(RunBodyWear - 1 - LightWearFrom);
	}
	/** How far LightWear looks from its aim for whole levels that keep her bounds, in levels each way of each channel. */
	inline constexpr int32_t LightSearch = 6;
	/** A run's reach (rule 3), in millimetres: its longest (3 m), how wide its foot may be (three times the widest source,
	 * 15 cm; a point may lie half of it aside), and its rake, tan 10 degrees, as 1763 in 10000 of the drop. */
	inline constexpr int64_t RunLongestMm = 3000, RunFootWideMm = 450, RunRakePer10000 = 1763;

	/** What the sea battle sets on its own steel ink (an instance of M_MapSurface; AAirStage::SetShipPaint) for the options
	 * and the first look's wear. Whatever is not marked to be set is left as the material makes it. */
	struct Steel
	{
		/** TopLight to set; below 0, left as the material makes it (0.26, which lifts nothing while Wear is 0). */
		float TopLight = -1.0f;
		/** Wear to set (the blot mottle, and the lift by TopLight); 0, left as made (0: no mottle, no lift, no chips). */
		float Wear = 0.0f;
		/** ChipAt set to SeaChipAt: no chips. */
		bool bChipsOff = false;
		/** BlotTiling set to the first look's blots, about 7 m across on the ship's own metres (the skin only). */
		bool bBlots = false;
	};

	/** What the sea battle asks of its ships' paint. As made: today's, everything off. */
	struct Options
	{
		/** Multiply by the file's "shade" (R1.3). */
		bool bShade = false;
		/** The rust ramp and the ruled salt (R1.4). */
		bool bRustRamp = false;
		/** The ships' top light at sea, the lift on what faces the sky (-TheaterSeaShipLight=, above 0 when on); 0 or below,
		 * none asked. The ink's own TopLight is set from it by SteelOf. Not a point's paint. */
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
		/** At the foot of a run with its body above it (BodyAbove's FootMark, under the skin only): it may take the rust's
		 * film. */
		bool bBodyAbove = false;
		/** Her salt on it, or on a point that shares a triangle with it (BodyAbove's SaltMark, on a point at a run's foot,
		 * under the skin only): the film never lies there, the ramp starts at its body. */
		bool bBesideSalt = false;
	};

	/** One point as BodyAbove reads it: where it lies (the reader's frame, metres, +Z her own up axis), which way it faces
	 * (the file's facing, as the reader turns it; any length), its wear (0 to 100), its salt as the file gives it (below 0,
	 * none) and whether it is painted steel that rusts and salts. */
	struct RunPoint
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		double FX = 0.0, FY = 0.0, FZ = 0.0;
		int32_t Wear = 0;
		int32_t Salt = -1;
		bool bPainted = false;
	};

	/** Which of her points lie at the foot of a run with its body above them (rule 3; see "A run, and what lies above"):
	 * one mark a point, FootMark for those, 0 for every other (an unstained point is never marked); and on a point so
	 * marked, SaltMark as well where her salt lies on it (painted, salt above 0) or on any point of a triangle it is a
	 * corner of (her ruling of 26 November: the film never beside her salt). Triangles: the mesh's, three point indices
	 * each (Indices of them); a side with an end out of range is passed over, and so is a triangle for the salt. The same
	 * mesh is marked the same way every time. */
	std::vector<uint8_t> BodyAbove(const std::vector<RunPoint>& Points, const int32_t* Triangles, size_t Indices);

	/** The point's colour (linear, as the reader turns it into the mesh's colour). */
	Linear Paint(const Point& P, const Palette& Pal, const Options& O);

	/** The sea battle's steel ink for these options and the first look's wear (FirstLookWear, 0 for none). With every switch
	 * off, nothing is set. With the skin (FirstLookWear above 0): its Wear, its blots, the chips held off, and the top light
	 * asked (none, SeaTopLightToday, if not). With the light alone: the Wear SeaLightAloneWear, the chips held off, and
	 * nothing else. Either way the ink's TopLight is the light asked divided by the Wear set, so a steel deck facing the sky
	 * is lifted by 1 + T x its metal, whether the skin is on or not: 1 + 254/255 x T on unworn steel (the reader's alpha
	 * 128; 1.0996 at 0.10), 1 + T on worn steel (alpha 127 to 78, all metal). */
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
	 * there before it is laid over the coat (linear), how much of it is laid, the brightness it may not pass (a share of
	 * the paint's own), and how much of the foot's film is in that colour (0 where it is the body's or darker). */
	struct Laid
	{
		float Along = 0.0f;
		Linear Colour;
		float Over = 0.0f;
		float Ceiling = 1.0f;
		float Foot = 0.0f;
	};
	/** The ramp at a point that rusts (P.bPainted and P.Wear above 0), and the coat it leaves: the film only where
	 * P.bBodyAbove. Paint holds what it leaves to the paint's own (rule 1), once the soot and salt are laid. */
	Linear Ramp(const Point& P, Laid* Out = nullptr);
	/** Light wear away from a run (rule 2: P.bPainted, P.Wear LightWearFrom to 19, no body above, no salt: Paint leaves the
	 * point as it is unworn below a wear of 5 and under her salt) on the point as it is unworn (Unworn: what Paint gives it
	 * with no wear, its soot laid): darker by LightDarkAt its wear and a little greyer, no rust colour, no channel above
	 * Unworn nor above the paint; whole sRGB levels, the nearest to her aim that keep all of her bounds and the band
	 * (LightDarkBand) against Unworn's own bytes. Where whole levels cannot keep the band within LightSearch levels (a
	 * near-grey paint, where a level is a large share of its colour), the levels that keep all her other bounds with the
	 * darkening nearest the line; where none keeps them, her aim's nearest whole levels held under Unworn (none in today's
	 * files; the test counts any such point as out). */
	Linear LightWear(const Point& P, const Linear& Unworn);
	/** Rule 1: each channel held to Most's (its alpha kept). */
	Linear NoneAbove(Linear C, const Linear& Most);
	/** Rule 1 on a stained point as Paint lays it, and on an unworn point her salt lies on: each channel held to the
	 * paint's own, or where her salt lies to that salt over the clean paint (Salted, at most); below the paint in any
	 * channel, above it in none; and with bNoWarmer (where her salt lies), lifted by it or not, red less blue in the whole
	 * levels the mesh stores (StoredLevel) no higher than Salted's own, only red moving (lifted in every channel, before the
	 * hold to the paint, so that what it lowers is held with it). */
	Linear Held(Linear C, const Linear& Paint, const Linear& Salted, bool bNoWarmer);
	/** Whether the skin paints the point as the same point unworn (her ruling of 26 November): light wear away from a run
	 * (P.bPainted, P.Wear 1 to 19, no body above) below a wear of LightWearFrom, or under her salt. Only under the skin
	 * (O.bRustRamp). */
	bool PaintsAsUnworn(const Point& P, const Options& O);
	/** The finish byte the reader stores on a point of her steel (the material's metal: 128 unworn, below it worn): 128 less
	 * half the point's wear, as ever; where the skin paints the point as it is unworn (PaintsAsUnworn), the unworn 128, so
	 * that such a point's vertex is the unworn point's bit for bit. With every switch off, today's, always. */
	uint8_t SteelFinish(const Point& P, const Options& O);

	/** The byte the mesh stores for a linear channel (ToFColor's sRGB: the standard's curve, rounded half up), and the
	 * linear channel the engine reads back from a byte (FLinearColor's own, from the same curve). */
	int32_t StoredLevel(float Lin);
	float LevelLinear(int32_t Level);

	/** sRGB (0-1) to linear and back, as the standard has them (the ramp's own steps; the engine's tables agree to well
	 * under one level). */
	float ToLinear(float Srgb);
	float ToSrgb(float Lin);
	/** Rec. 709 brightness of a linear colour. */
	float Brightness(const Linear& C);
}
