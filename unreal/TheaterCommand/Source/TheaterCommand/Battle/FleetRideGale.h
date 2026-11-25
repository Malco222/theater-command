#pragma once

// THE GALE'S RIDE (the release 2 ocean plan, step 14(c), the gale: Brian's and the art director's decision; risk 14, "at
// Beaufort 7 and up, crests can rise through a destroyer's low decks"). FleetRide's limits at Beaufort 7, 8 and 9, as data
// the ride will read when step 5 builds it (FleetRide.{h,cpp}), so nothing is built twice: the roll and pitch it is held
// to (its soft limits, as step 5's RollMax = 1.1 x the table at her Beaufort), the most it heaves, the decks it is held to,
// and how often the gale's sea may stand over each of them. SeaShow's own table (the art director's, RideOf in SeaShow.cpp,
// Beaufort 3 to 6) and its Raised cap of 13.8 m/s are not changed and not read here: the show and its tests do not move,
// and nothing the show decides reads anything in this file. Engine-free; Tools/sea_waves/gale_test.cpp holds it to the
// art director's table (read from SeaShow.cpp's own text), to the ship models (gale_decks.py, Content/Models/Ships) and to
// the gale's sea (the freeboard tests), and FleetWaves' gale functions (FleetWaves.h) raise that sea.
// Numbers: [AD] the art director's; [RULE] made from the art director's by the rule given; [MODEL] read from the model file
// she is drawn with; [P] published; [PLAN] the plan's; [EST] an estimate; [TEST] measured by gale_test, with its margin.

namespace FleetRideGale
{
	/** The Beaufort numbers a row holds, in order: 7, 8 and 9 (WMO: 13.9-17.1, 17.2-20.7 and 20.8-24.4 m/s). */
	inline constexpr int Forces = 3, FirstForce = 7;

	/** [AD] The art director's Beaufort 6 column (SeaShow.cpp, RideOf: a destroyer's or destroyer escort's row, 110 m and
	 * under; a cruiser's, 180 m; a battleship's, 250 m and over), and her Beaufort 5 column, which the gale's steps are made
	 * from; and her roll periods (seconds). gale_test reads them again from SeaShow.cpp and fails if they have moved. */
	inline constexpr double TableRoll5[3] = { 5.5, 3.0, 1.5 }, TableRoll6[3] = { 9.5, 5.5, 3.0 };
	inline constexpr double TablePitch5[3] = { 2.5, 1.25, 0.75 }, TablePitch6[3] = { 4.0, 2.0, 1.25 };
	inline constexpr double TableRollT[3] = { 9.0, 12.5, 16.0 };

	/** [RULE, EST] A row of the ride's limits at Beaufort 7, 8 and 9 (degrees either way, as the art director's table; the
	 * ride's soft limits are 1.1 times these, RollMax tanh(R / RollMax) as step 5 lays them). The rule: from Beaufort 6 to
	 * 7 the art director's own step from 5 to 6, and each step after it three quarters of the one before. [EST] The motions
	 * grow more slowly than the sea: a sea the wind has fully raised keeps one steepness at every wind (Pierson-Moskowitz:
	 * 2 pi Hs / (g Tp^2) = 0.025), and as its waves outgrow her length she rides them more nearly as they come. For Brian's
	 * and the art director's sign-off; gale_test reports the ride's own motions in the gale's sea against them. */
	struct Row
	{
		double Roll[Forces], Pitch[Forces];
	};
	inline constexpr Row Small = { { 13.5, 16.5, 18.75 }, { 5.5, 6.625, 7.46875 } };
	inline constexpr Row Middle = { { 8.0, 9.875, 11.28125 }, { 2.75, 3.3125, 3.734375 } };
	inline constexpr Row Big = { { 4.5, 5.625, 6.46875 }, { 1.75, 2.125, 2.40625 } };
	inline constexpr const Row* Rows[3] = { &Small, &Middle, &Big };

	/** [AD] How far along the rows a hull of Long metres lies (0 Small, 1 Middle, 2 Big; between them, between), as the art
	 * director's RideOf takes it: (Long - 110) / 140 x 2, from 0 to 2. */
	inline double RowOf(double Long)
	{
		const double K = (Long - 110.0) / 140.0;
		return 2.0 * (K < 0.0 ? 0.0 : (K > 1.0 ? 1.0 : K));
	}
	/** A row's number at a Beaufort number that need not be whole: B6 (the art director's at Beaufort 6) at 6 and under,
	 * straight from it to the row's 7, between the row's 7, 8 and 9, and as at 9 over 9. */
	inline double AtGale(const double (&Of)[Forces], double B6, double Force)
	{
		if (Force <= 6.0) return B6;
		if (Force <= 7.0) return B6 + (Of[0] - B6) * (Force - 6.0);
		const double K = (Force >= 9.0 ? 9.0 : Force) - 7.0;
		const int I = K >= 1.0 ? 1 : 0;
		return Of[I] + (Of[I + 1] - Of[I]) * (K - I);
	}
	/** The limit for a hull of Long metres at a Beaufort number (degrees either way; Roll true for roll, else pitch). */
	inline double LimitAt(double Long, double Force, bool bRoll)
	{
		const double K = RowOf(Long);
		const int I = K >= 1.0 ? 1 : 0;
		const double T = K - I;
		const double A = AtGale(bRoll ? Rows[I]->Roll : Rows[I]->Pitch, bRoll ? TableRoll6[I] : TablePitch6[I], Force);
		const double B = AtGale(bRoll ? Rows[I + 1]->Roll : Rows[I + 1]->Pitch, bRoll ? TableRoll6[I + 1] : TablePitch6[I + 1], Force);
		return A + (B - A) * T;
	}
	/** [PLAN] The ride's soft limit is this many times the row (step 5's 1.1). */
	inline constexpr double SoftOver = 1.1;
	/** [PLAN] Look heave is at most this share of the sea's Hs (step 5's "look heave is at most 0.5 Hs"), in the gale too.
	 * Under the gale heave therefore takes the same soft limit as roll and pitch, HeaveMax tanh(Heave / HeaveMax) with
	 * HeaveMax = HeaveMost x Hs: the trains' crests can sum to about 0.8 Hs, and gale_test measured 8.67 m of raw heave on a
	 * destroyer in Beaufort 9's fully raised sea (Hs 10.7 m), over the 5.37 m the limit allows. Below Beaufort 7 the table's
	 * own heave (the art director's Heave x 0.021 U^2, at most 0.2 x 4.0 m for a destroyer) never reaches it, so the limit
	 * changes nothing under the cap and the show does not move. */
	inline constexpr double HeaveMost = 0.5;

	/**
	 * The decks the gale's sea is held against: where green water first comes aboard. Bow: her forecastle deck at 0.45 of
	 * her length forward of her middle, on her middle line; Waist: her weather deck at her side 0.1 of her length abaft her
	 * middle (clear of a model's break); Stern: her quarterdeck at her side 0.4 of her length abaft it; heights over her
	 * waterline at rest (metres), WaistOut and SternOut how far out from her middle line those edges are (metres; a boat's
	 * casing where her men stand, BoatDeck::Edge). Long and Beam: the model's (or her class's); Speed: her class's in the
	 * show (a fleet destroyer's 14 m/s, a surfaced boat's 7) or the month's sub scene's (escorts 7.7 to 8.2 m/s).
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
	};
	inline constexpr Deck Decks[] = {
		{ "destroyer", "Destroyer", 114.0, 11.0, 14.0, 6.947, 3.42, 3.42, 5.5, 4.07, false },
		{ "destroyer (USA)", "Destroyer_USA", 114.8, 12.0, 14.0, 7.034, 4.02, 4.02, 6.0, 4.44, false },
		{ "destroyer (JAP)", "Destroyer_JAP", 118.5, 10.8, 14.0, 8.005, 3.326, 3.509, 5.4, 4.0, false },
		{ "destroyer (GER)", "Destroyer_GER", 123.0, 11.8, 14.0, 7.751, 3.62, 3.62, 5.9, 4.37, false },
		{ "frigate (River class) [EST decks]", "", 91.9, 11.1, 8.2, 6.0, 3.5, 2.4, 5.55, 4.1, false },
		{ "corvette (Flower class) [EST decks]", "", 62.5, 10.1, 7.7, 5.0, 2.0, 2.0, 5.05, 3.7, false },
		{ "U-boat surfaced (Type VIIC)", "Submarine_GER", 67.0, 6.2, 7.0, 2.194, 1.52, 1.52, 1.2, 0.85, true },
		{ "submarine surfaced (USA)", "Submarine_USA", 95.0, 8.3, 7.0, 3.595, 2.42, 2.42, 1.9, 1.35, true },
		{ "submarine surfaced (any other navy)", "Submarine", 80.0, 7.0, 7.0, 2.575, 1.82, 1.82, 1.5, 1.05, true },
	};
	inline constexpr int DeckCount = static_cast<int>(sizeof(Decks) / sizeof(Decks[0]));
	/** [EST] The speed a ship is slowed to in a gale when she heaves to or keeps steerage way (m/s): the freeboard tests run
	 * at her class's speed and at this. */
	inline constexpr double HoveTo = 3.0;

	/**
	 * [TEST] How often the gale's sea may stand over each deck in the look, at Beaufort 7, 8 and 9 (the share of the time,
	 * 0 to 1), the most over every heading to the sea (head, bow, beam, quartering), both speeds and both seas (the gale's
	 * own, GaleFetchOf's, and the sea the wind has fully raised), the ride at full weight with these limits: what the ride's
	 * own checks (step 5) hold it to from Beaufort 7, where its "the deck edge never goes under" ends, and what the gale's
	 * spray (step 6(b): green water from Beaufort 7) may fire by. Measured by gale_test's freeboard tests, each with a
	 * margin (the measured share times 1.25, and 0.005 more); gale_test fails if one is exceeded.
	 */
	struct Green
	{
		double Bow[Forces], Waist[Forces], Stern[Forces];
	};
	inline constexpr Green GreenMost[DeckCount] = {
		{ { 0.0050, 0.0050, 0.0050 }, { 0.0097, 0.0706, 0.1675 }, { 0.0050, 0.0441, 0.1683 } }, // destroyer
		{ { 0.0050, 0.0050, 0.0050 }, { 0.0050, 0.0300, 0.0816 }, { 0.0050, 0.0050, 0.0706 } }, // destroyer (USA)
		{ { 0.0050, 0.0050, 0.0050 }, { 0.0105, 0.0738, 0.1878 }, { 0.0050, 0.0323, 0.1409 } }, // destroyer (JAP)
		{ { 0.0050, 0.0050, 0.0050 }, { 0.0089, 0.0589, 0.1339 }, { 0.0050, 0.0152, 0.1191 } }, // destroyer (GER)
		{ { 0.0050, 0.0050, 0.0050 }, { 0.0050, 0.0191, 0.1253 }, { 0.0081, 0.0917, 0.3464 } }, // frigate (River class) [EST decks]
		{ { 0.0050, 0.0050, 0.0112 }, { 0.0902, 0.2573, 0.4433 }, { 0.0542, 0.2573, 0.4753 } }, // corvette (Flower class) [EST decks]
		{ { 0.0573, 0.1652, 0.1831 }, { 0.1425, 0.4886, 0.5355 }, { 0.1050, 0.4988, 0.5581 } }, // U-boat surfaced (Type VIIC)
		{ { 0.0152, 0.0386, 0.0488 }, { 0.0269, 0.1331, 0.2331 }, { 0.0120, 0.0886, 0.2347 } }, // submarine surfaced (USA)
		{ { 0.0472, 0.1198, 0.1417 }, { 0.0870, 0.3448, 0.4120 }, { 0.0331, 0.3019, 0.4284 } }, // submarine surfaced (any other navy)
	};
}
