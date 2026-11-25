#pragma once

// THE GALE'S RIDE (the release 2 ocean plan, step 14(c), the gale: Brian's and the art director's decision; risk 14, "at
// Beaufort 7 and up, crests can rise through a destroyer's low decks"). FleetRide's limits at Beaufort 7, 8 and 9, as data
// the ride will read when step 5 builds it (FleetRide.{h,cpp}), so nothing is built twice: the roll and pitch it is held
// to (its soft limits, as step 5's RollMax = 1.1 x AtForce at her Beaufort), the decks it is held to, and how often the
// sea the scene draws stands over each of them. Heave is not limited (step 5: "water on her side is exactly what the tests
// catch"). SeaShow's own table (the art director's, RideOf and AtForce in SeaShow.cpp, Beaufort 3 to 6) and its Raised cap
// of 13.8 m/s are not changed and not read here: they are mirrored, and gale_test fails if the mirror and SeaShow.cpp's
// text part. Nothing the show decides reads anything in this file. Engine-free; Tools/sea_waves/gale_test.cpp holds it to
// the art director's table (read from SeaShow.cpp's own text), to the ship models (gale_decks.py, Content/Models/Ships), to
// the trim the convoy looks draw a surfaced boat at (ConvoyLooksCore's StepBoat) and to the sea (the freeboard tests).
// Numbers: [AD] the art director's; [SHOW] the game's own, mirrored; [RULE] made from the art director's by the rule given;
// [MODEL] read from the model file she is drawn with; [P] published; [PLAN] the plan's; [EST] an estimate; [TEST] measured
// by gale_test and pinned as measured.

namespace FleetRideGale
{
	/** The Beaufort numbers a gale row holds, in order: 7, 8 and 9 (WMO: 13.9-17.1, 17.2-20.7 and 20.8-24.4 m/s). */
	inline constexpr int Forces = 3, FirstForce = 7;

	/** [AD] The art director's table, as SeaShow.cpp's RideOf holds it (21 November 23:44Z): roll and pitch (degrees
	 * either way) at Beaufort 3, 4, 5 and 6, and the roll period (seconds), for a destroyer's or destroyer escort's row
	 * (110 m and under), a cruiser's (180 m) and a battleship's (250 m and over). gale_test reads them again from
	 * SeaShow.cpp and fails if one has moved. */
	inline constexpr double TableRoll[3][4] = { { 1.5, 3.0, 5.5, 9.5 }, { 0.75, 1.5, 3.0, 5.5 }, { 0.35, 0.75, 1.5, 3.0 } };
	inline constexpr double TablePitch[3][4] = { { 0.5, 1.25, 2.5, 4.0 }, { 0.3, 0.75, 1.25, 2.0 }, { 0.2, 0.4, 0.75, 1.25 } };
	inline constexpr double TableRollT[3] = { 9.0, 12.5, 16.0 };

	/** [SHOW] The Beaufort number of the show's cap, (13.8 / 0.836)^(2/3): the art director's limits hold to it, flat from
	 * 6 (AtForce stops at Beaufort 6), and the gale's climb only above it. gale_test checks it against the show's formula. */
	inline constexpr double ForceCap = 6.483087342897949;

	/** [RULE, EST] The gale's rows at Beaufort 7, 8 and 9 (degrees either way, as the art director's table): from 6 to 7
	 * the art director's own step from 5 to 6, and each step after it three quarters of the one before. [EST] The motions
	 * grow more slowly than the sea: a sea the wind has fully raised keeps one steepness at every wind (Pierson-Moskowitz:
	 * 2 pi Hs / (g Tp^2) = 0.025), and as its waves outgrow her length she rides them more nearly as they come. For Brian's
	 * and the art director's sign-off. (Every hull under 110 m takes the destroyer's row, as RideOf has it: a corvette or a
	 * surfaced boat is held to a destroyer's roll, though Flowers and VIICs rolled 30-40 deg in Atlantic gales. A row of
	 * their own is the art director's to give.) */
	struct Row
	{
		double Roll[Forces], Pitch[Forces];
	};
	inline constexpr Row Gale[3] = {
		{ { 13.5, 16.5, 18.75 }, { 5.5, 6.625, 7.46875 } },        // a destroyer's (110 m and under)
		{ { 8.0, 9.875, 11.28125 }, { 2.75, 3.3125, 3.734375 } },  // a cruiser's (180 m)
		{ { 4.5, 5.625, 6.46875 }, { 1.75, 2.125, 2.40625 } },     // a battleship's (250 m and over)
	};

	/** [SHOW] Which rows a hull of Long metres lies between, and how far, as RideOf takes it: K = clamp((Long - 110) / 140,
	 * 0, 1) x 2; rows A = 0 and 1 under K = 1, else 1 and 2; T = K or K - 1. */
	inline void RowsOf(double Long, int& A, double& T)
	{
		const double Share = (Long - 110.0) / 140.0;
		const double K = (Share < 0.0 ? 0.0 : (Share > 1.0 ? 1.0 : Share)) * 2.0;
		A = K < 1.0 ? 0 : 1;
		T = K < 1.0 ? K : K - 1.0;
	}
	/** [SHOW] AtForce, as SeaShow.cpp's: a row of the table at a Beaufort number that need not be whole: between 3 and 6
	 * between the columns, under 3 less to nothing in a calm, over 6 as at 6. */
	inline double AtForceOf(const double (&Of)[4], double Force)
	{
		if (Force <= 3.0) return Of[0] * (Force / 3.0 < 0.0 ? 0.0 : (Force / 3.0 > 1.0 ? 1.0 : Force / 3.0));
		const double K = (Force < 6.0 ? Force : 6.0) - 3.0;
		const int I = static_cast<int>(K) < 2 ? static_cast<int>(K) : 2;
		return Of[I] + (Of[I + 1] - Of[I]) * (K - I);
	}
	/** The art director's limit for a hull of Long metres at a Beaufort number (degrees either way; bRoll true for roll,
	 * else pitch): RideOf's row for her length, then AtForce, in the show's own order. */
	inline double TableAt(double Long, double Force, bool bRoll)
	{
		int A = 0;
		double T = 0.0;
		RowsOf(Long, A, T);
		const double(&Lo)[4] = bRoll ? TableRoll[A] : TablePitch[A];
		const double(&Hi)[4] = bRoll ? TableRoll[A + 1] : TablePitch[A + 1];
		double Of[4] = {};
		for (int I = 0; I < 4; ++I) Of[I] = Lo[I] + (Hi[I] - Lo[I]) * T;
		return AtForceOf(Of, Force);
	}
	/** Her roll period (seconds), between the rows as RideOf takes it. */
	inline double RollTAt(double Long)
	{
		int A = 0;
		double T = 0.0;
		RowsOf(Long, A, T);
		return TableRollT[A] + (TableRollT[A + 1] - TableRollT[A]) * T;
	}
	/**
	 * The limit for a hull of Long metres at a Beaufort number (degrees either way; bRoll true for roll, else pitch). Up to
	 * the show's cap (ForceCap, 13.8 m/s) it is the art director's own, TableAt, exactly; above it, it climbs straight from
	 * the art director's at the cap to the gale's row at 7, between the gale's rows at 7, 8 and 9, and holds 9's over 9. No
	 * jump at the cap: TableAt there is already Beaufort 6's column, which is where the climb starts.
	 */
	inline double LimitAt(double Long, double Force, bool bRoll)
	{
		if (Force <= ForceCap) return TableAt(Long, Force, bRoll);
		int A = 0;
		double T = 0.0;
		RowsOf(Long, A, T);
		double Of[Forces] = {};
		for (int F = 0; F < Forces; ++F)
		{
			const double Lo = bRoll ? Gale[A].Roll[F] : Gale[A].Pitch[F], Hi = bRoll ? Gale[A + 1].Roll[F] : Gale[A + 1].Pitch[F];
			Of[F] = Lo + (Hi - Lo) * T;
		}
		const double Six = TableAt(Long, ForceCap, bRoll);
		if (Force <= 7.0) return Six + (Of[0] - Six) * (Force - ForceCap) / (7.0 - ForceCap);
		const double K = (Force >= 9.0 ? 9.0 : Force) - 7.0;
		const int I = K >= 1.0 ? 1 : 0;
		return Of[I] + (Of[I + 1] - Of[I]) * (K - I);
	}
	/** [PLAN] The ride's soft limit is this many times LimitAt (step 5's 1.1), for roll and pitch. Heave has none. */
	inline constexpr double SoftOver = 1.1;

	/**
	 * The decks the sea is held against: where green water first comes aboard. Bow: her forecastle deck at 0.45 of her
	 * length forward of her middle, on her middle line; Waist: her weather deck at her side 0.1 of her length abaft her
	 * middle (clear of a model's break); Stern: her quarterdeck at her side 0.4 of her length abaft it; heights over her
	 * model's waterline (metres), WaistOut and SternOut how far out from her middle line those edges are (metres; a boat's
	 * casing where her men stand, BoatDeck::Edge). Long and Beam: the model's (or her class's); Speed: her class's in the
	 * show (a fleet destroyer's 14 m/s, a surfaced boat's 7) or the month's sub scene's (escorts 7.7 to 8.2 m/s).
	 * Trim: [SHOW] how far under her model's waterline the game draws her at rest: a surfaced boat rides at trim awash,
	 * ConvoyLooksCore's StepBoat lowering her model by its casing's height over DeckAwash (0.5 m) unless her tanks are blown
	 * (forced up, or striking), when she rides at her model's waterline; a ship 0. gale_test runs StepBoat and fails if
	 * these have moved.
	 * [MODEL] the lowest deck there in the model file (gale_decks.py measures it again: within 5 cm). The corvette and the
	 * frigate have no model of their own (the stage draws every "dd" with Destroyer*.json, so their drawn decks are a
	 * destroyer's today): their rows are [EST], from their published size [P: Rules/sub-scene-looks.json] and a destroyer's
	 * proportions, for when the modeller makes them.
	 */
	struct Deck
	{
		const char* Name;
		const char* Model; // (Content/Models/Ships/<Model>.json; empty: none of her own)
		double Long, Beam, Speed;
		double Bow, Waist, Stern, WaistOut, SternOut;
		bool bBoat;
		double Trim;
	};
	inline constexpr Deck Decks[] = {
		{ "destroyer", "Destroyer", 114.0, 11.0, 14.0, 6.947, 3.42, 3.42, 5.5, 4.07, false, 0.0 },
		{ "destroyer (USA)", "Destroyer_USA", 114.8, 12.0, 14.0, 7.034, 4.02, 4.02, 6.0, 4.44, false, 0.0 },
		{ "destroyer (JAP)", "Destroyer_JAP", 118.5, 10.8, 14.0, 8.005, 3.326, 3.509, 5.4, 4.0, false, 0.0 },
		{ "destroyer (GER)", "Destroyer_GER", 123.0, 11.8, 14.0, 7.751, 3.62, 3.62, 5.9, 4.37, false, 0.0 },
		{ "frigate (River class) [EST decks]", "", 91.9, 11.1, 8.2, 6.0, 3.5, 2.4, 5.55, 4.1, false, 0.0 },
		{ "corvette (Flower class) [EST decks]", "", 62.5, 10.1, 7.7, 5.0, 2.0, 2.0, 5.05, 3.7, false, 0.0 },
		{ "U-boat surfaced (Type VIIC)", "Submarine_GER", 67.0, 6.2, 7.0, 2.194, 1.52, 1.52, 1.2, 0.85, true, 1.27 },
		{ "submarine surfaced (USA)", "Submarine_USA", 95.0, 8.3, 7.0, 3.595, 2.42, 2.42, 1.9, 1.35, true, 2.1 },
		{ "submarine surfaced (any other navy)", "Submarine", 80.0, 7.0, 7.0, 2.575, 1.82, 1.82, 1.5, 1.05, true, 1.5 },
	};
	inline constexpr int DeckCount = static_cast<int>(sizeof(Decks) / sizeof(Decks[0]));
	/** [EST] The speed a ship is slowed to in a gale when she heaves to or keeps steerage way (m/s): the freeboard tests run
	 * at her class's speed and at this. */
	inline constexpr double HoveTo = 3.0;

	/**
	 * [TEST] How often the sea the scene draws stands over each deck in the look, at Beaufort 7, 8 and 9 (the share of the
	 * time, 0 to 1; at each row's middle wind, 15.5, 18.9 and 22.6 m/s, in the gale's own sea, GaleFetchOf's fetch, never a
	 * fully raised sea the scene does not ask for): the most over four headings to the sea (head, bow, beam, quartering) and
	 * both speeds, each instant counted once (a deck is wet when the water stands over it on either side), the ride as step 5
	 * lays it (its light low-pass at 0.4 Tp for heave and pitch, her roll oscillator at RollTAt; full weights; roll and pitch
	 * soft-limited at 1.1 x LimitAt, heave not), a boat at the trim the game draws her at (Trim). Pinned as measured: gale_test
	 * fails if a share moves from its pin by more than 0.003 and a twentieth of it, either way.
	 * These are the look's, not the ships': the ride's low-pass has no hull period and no bow swell-up, so her bow follows
	 * the water head-on and her forecastle stays nearly dry, where real destroyers shipped green water over it in heavy head
	 * seas (why breakwaters were fitted). gale_test's hull reference (a heave and pitch period of 5.75 s, damping 0.25 [EST])
	 * shows the bow wet; step 6(b)'s green water should not take the look's dry forecastle for the sea's.
	 */
	struct Green
	{
		double Bow[Forces], Waist[Forces], Stern[Forces];
	};
	inline constexpr Green GreenMost[DeckCount] = {
		{ { 0.0000, 0.0000, 0.0000 }, { 0.0000, 0.0075, 0.0144 }, { 0.0000, 0.0000, 0.0006 } }, // destroyer
		{ { 0.0000, 0.0000, 0.0000 }, { 0.0000, 0.0025, 0.0044 }, { 0.0000, 0.0000, 0.0000 } }, // destroyer (USA)
		{ { 0.0000, 0.0000, 0.0000 }, { 0.0000, 0.0075, 0.0163 }, { 0.0000, 0.0000, 0.0000 } }, // destroyer (JAP)
		{ { 0.0000, 0.0000, 0.0000 }, { 0.0000, 0.0044, 0.0094 }, { 0.0000, 0.0000, 0.0000 } }, // destroyer (GER)
		{ { 0.0000, 0.0000, 0.0000 }, { 0.0000, 0.0019, 0.0119 }, { 0.0056, 0.0300, 0.0475 } }, // frigate (River class) [EST decks]
		{ { 0.0000, 0.0000, 0.0000 }, { 0.0413, 0.0994, 0.1487 }, { 0.0119, 0.0575, 0.1081 } }, // corvette (Flower class) [EST decks]
		{ { 0.2425, 0.2744, 0.3119 }, { 0.4106, 0.4506, 0.4700 }, { 0.4250, 0.4650, 0.4494 } }, // U-boat surfaced (Type VIIC)
		{ { 0.1369, 0.2050, 0.2537 }, { 0.4113, 0.4431, 0.4669 }, { 0.4037, 0.4531, 0.4412 } }, // submarine surfaced (USA)
		{ { 0.2181, 0.2575, 0.3013 }, { 0.3912, 0.4325, 0.4569 }, { 0.3956, 0.4519, 0.4275 } }, // submarine surfaced (any other navy)
	};
	/**
	 * [TEST] The same at the show's cap, 13.8 m/s (Beaufort 6.48, the most wind below Beaufort 7; its sea the show's own,
	 * fully raised, 4.00 m, which is the sea the scene draws there): bow, waist, stern. Step 5's own check below Beaufort 7
	 * (amended 25 November) pins boats and corvettes as measured: a boat at trim awash is meant to wash, and a corvette's
	 * waist is wet a little. Destroyers' forecastles, waists and quarterdecks stay dry here (gale_test checks it, and at
	 * 12.3 m/s). Pinned as GreenMost is.
	 */
	struct GreenCap
	{
		double Bow, Waist, Stern;
	};
	inline constexpr GreenCap GreenBelow[DeckCount] = {
		{ 0.0000, 0.0000, 0.0000 }, // destroyer
		{ 0.0000, 0.0000, 0.0000 }, // destroyer (USA)
		{ 0.0000, 0.0000, 0.0000 }, // destroyer (JAP)
		{ 0.0000, 0.0000, 0.0000 }, // destroyer (GER)
		{ 0.0000, 0.0000, 0.0000 }, // frigate (River class) [EST decks]
		{ 0.0000, 0.0169, 0.0031 }, // corvette (Flower class) [EST decks]
		{ 0.1919, 0.3937, 0.3794 }, // U-boat surfaced (Type VIIC)
		{ 0.1006, 0.3856, 0.3506 }, // submarine surfaced (USA)
		{ 0.1744, 0.3625, 0.3513 }, // submarine surfaced (any other navy)
	};
}
