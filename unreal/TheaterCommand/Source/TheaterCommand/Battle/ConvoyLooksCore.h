#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * The month's submarine scene, its looks (ruling 241, Part D1: look only; the sea scene thread's d1-hooks.md, section 3,
 * the list of what is ours): a boat surfaced, at periscope depth or deep, and her going down and coming up; a depth
 * charge pattern going in and each charge's burst; a starshell drifting down under its parachute; a boat's end crushed
 * (her air coming up) or scuttled (brought up); the scrambling net down a rescuer's side. Plain C++, no engine:
 * ASeaBattle's marked hooks (Battle/ConvoyLooks) turn what is worked out here into the effects kit's calls, one small
 * mesh and a lamp or two, and the cloud test (Tools/convoy_looks/run.sh) checks it.
 *
 * LOOKS ONLY. The show (SeaShow, the sea scene thread's) says where every boat is, how deep (Keel, eased so that a dive
 * takes 25 to 30 s), when a pattern goes in and when each charge bursts, where a starshell bursts and when it burns out,
 * how a boat ends and who picks up survivors. Nothing here changes any of that or draws from the battle's luck: what is
 * decided here is only how it looks, with the look's own hash dice from the keys the show gives. The men in the water,
 * the oil and the wreckage are the sea scene's.
 *
 * Places are in board metres (X east, Y north, Z up from the sea), the show's own; headings are degrees clockwise from
 * north (the show's ShipOut.Heading). Time is the battle's clock, in seconds.
 *
 * Numbers: [OEG] US Navy OEG Report 51 as naval-battles/wolfpack-r2/history.md gives them; [P] a published figure there;
 * [FSR] the field note (field-side-read.md section 3); [SHOW] the sea scene's show (SeaShow.cpp), which these looks
 * must agree with; [EST] this lane's own estimate, where the history has none. Each is marked where it is set, in
 * ConvoyLooksCore.cpp.
 */
namespace ConvoyLook
{
	/** A point or a way, board metres. */
	struct P3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		P3() = default;
		P3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
		P3 operator+(const P3& O) const { return P3(X + O.X, Y + O.Y, Z + O.Z); }
		P3 operator-(const P3& O) const { return P3(X - O.X, Y - O.Y, Z - O.Z); }
		P3 operator*(double S) const { return P3(X * S, Y * S, Z * S); }
	};

	/** Level, along a heading (degrees clockwise from north), and to its right. */
	P3 AheadOf(double HeadingDeg);
	P3 RightOf(double HeadingDeg);
	/** The heading (degrees clockwise from north) of a level way; Else where it has no length. */
	double HeadingOf(const P3& Way, double Else);
	/** A number from 0 to under 1 fixed by a whole number: the look's own hash, never the battle's dice. */
	double Hash01(uint64_t N);
	/** 0 below A, 1 above B, smooth between. */
	double Smooth(double A, double B, double X);

	// -------------------------------------------------------------------------------------------------------------
	// The boat

	/**
	 * A boat's model as the battle models give it (battle-models/sea/Submarine*.json: metres in its own frame, x forward,
	 * z up from the waterline): her length and the width of her bounds, her draught, the casing's deck amidships, the
	 * bridge's top, her highest point (the periscopes housed), the first periscope's place along her and its head housed,
	 * how far it slides up, and her stem.
	 */
	struct BoatModel
	{
		std::string Name;
		double Long = 80.0, Wide = 7.08, Draught = 4.6;
		double Deck = 2.0, TowerTop = 4.95, Top = 6.79;
		double ScopeAlong = 0.4, ScopeHead = 6.42, ScopeSlide = 4.2;
		double BowAlong = 37.5;
	};
	/** "GER" the German layout (Submarine_GER), "USA" the American (Submarine_USA), any other the generic (Submarine). */
	BoatModel ModelOf(const std::string& Nation);

	/** How the stage draws her model: stretched along to the length it fits her to, and across and up by the beam's
	 * share (AirStage's FitHull, to AirFeed::HullOf's size). */
	struct Drawn
	{
		double Along = 1.0, Up = 1.0;
	};
	Drawn DrawnOf(const BoatModel& M, double HullLong, double HullBeam);

	/** What the show says of her this frame (ShipOut: Dive, Keel, Dived, bScope, her heave on the swell, already
	 * fading as she goes down, her speed, place and heading), the show's periscope depth (its header's subs.scope), and
	 * how much light falls on the sea over her (0 dark to 1 day: LitBy, SkyLit). */
	struct BoatIn
	{
		double T = 0.0;
		std::string Dive;       // "surfaced", "periscope", "deep"
		double Keel = 4.7;      // metres from the surface down to her keel
		double Dived = 0.0;     // metres her hull lies below her surfaced trim
		bool bScope = false;    // her periscope is up
		double Heave = 0.0;     // the swell's lift (metres)
		double Speed = 0.0;     // metres a second through the water
		P3 At;                  // her waterline amidships (Z is the show's, not read)
		double Heading = 0.0;   // degrees
		double ScopeKeel = 13.0;
		double Lit = 0.0;
	};

	/** A faint swirl on the sea over a boat running deep, where light falls on it: where, when, how strong (0 to 1). */
	struct Swirl
	{
		P3 At;
		double T0 = 0.0, Strength = 0.0;
	};

	/** What she keeps from frame to frame. */
	struct BoatState
	{
		bool bBegun = false;
		double T = 0.0;
		double Heave = 0.0;      // as drawn (metres): eased toward its mark, never faster than HeaveRate
		double Sub = 0.0;        // how far under her surfaced trim she is (the show's Dived), last frame
		double SubRate = 0.0;    // its rate (metres a second, plus: going down), smoothed
		double Raise = 0.0;      // the periscope: 0 housed, 1 right up (before easing)
		double Feather = 0.0;    // 0 to 1
		double SwellLast = 0.0;  // the show's heave last frame (for the bow's plunge)
		double NextFeather = 0.0, NextWash = 0.0, NextBow = 0.0, NextSwirl = 0.0;
		bool bTowerUnder = false;
		/** Forced up (a scuttling): from when, and from how far under; below 0 never. */
		double ForcedAt = -1.0, ForcedFrom = 0.0;
		std::vector<Swirl> Swirls;
	};

	enum class SplashKind
	{
		Feather, // the periscope's feather: a low spurt of spray along her course (the kit's Skip on water)
		Wash,    // water running off her casing as she goes down or comes up (the kit's churn)
		Swirl,   // where her bridge went under: the last of the wash, its foam left as the swirl (the kit's churn)
		Bow,     // surfaced and fast: her bow plunging into the swell throws spray ahead (the kit's Skip on water)
	};
	struct BoatSplash
	{
		SplashKind Kind = SplashKind::Wash;
		P3 At;            // on the sea (Z 0)
		P3 Way;           // level, which way it is thrown
		double T = 0.0;   // battle time it is due
	};

	struct BoatOut
	{
		double Heave = 0.0;      // what the stage is steered with: her waterline over the still sea (metres)
		bool bScopeShown = false;
		P3 ScopeFoot, ScopeHead; // the raised periscope's stick, when shown
		double Feather = 0.0;
		bool bTowerUnder = false;
		std::vector<BoatSplash> Splashes;
	};

	/** One frame of a boat (from her state and what the show says), in the stage's drawing of her model. */
	BoatOut StepBoat(BoatState& S, const BoatIn& In, const BoatModel& M, const Drawn& D);
	/** A scuttling: blow her tanks now, so that she is up in ForcedUpFor seconds wherever the show still has her. */
	void ForceUp(BoatState& S, double T, double Sub);
	/** Seconds a boat forced up takes to come up. */
	double ForcedUpFor();
	/** A swirl Age seconds old: its radius (metres) and how much of it shows (0 to 1, times its strength); false once
	 * it is gone. */
	bool SwirlAt(const Swirl& W, double T, double& OutRadius, double& OutShown);
	/** Seconds a swirl lasts. */
	double SwirlLife();

	// -------------------------------------------------------------------------------------------------------------
	// Depth charges

	/** A charge's weight, kg of TNT or its like, by the event's kind: "pattern" the stern and throwers' charge (the
	 * Mk VII's 290 lb), "ahead" the ahead-thrown bomb (the Hedgehog's, about 30 lb of a stronger filling). */
	double ChargeKg(const std::string& Kind);

	/** What a burst shows on the sea, by how deep it was set and how big it was: the white dome over it (when after the
	 * burst, how high, how wide: deeper, it is later, lower and broader), the column the gas throws up after it (when,
	 * and how high: none when deep), and the boil when the gas of a deep one reaches the sea (when; below 0 none). */
	struct BurstLook
	{
		double DomeAt = 0.0, DomeHigh = 0.0, DomeWide = 0.0;
		double ColumnAt = 0.0, ColumnHigh = 0.0;
		double BoilAt = -1.0;
	};
	BurstLook BurstOf(double DepthM, double Kg);

	/** The effects kit's size row for a column of water that high (its rows' crowns: how105 8-12 m, how150 12-20, bomb250
	 * 30-45, bomb500 45-60), or empty for a column too low to throw (under ColumnLeast metres). */
	std::string ColumnRow(double High);
	double ColumnLeast();
	/** The lowest dome drawn as the kit's churn (metres); a lower one is a ring of low spurts thrown outward, and one
	 * under DomeLeast is not seen at all. */
	double ChurnLeast();
	double DomeLeast();

	/** One call to the kit: Dt seconds after the burst, Dx and Dy metres from it (east, north), of row Row: "churn" (the
	 * kit's quiet white water: a dome, a boil), a column's own size row, or "skip" (the kit's low spurt on water, thrown
	 * along WayX, WayY). */
	struct KitSpout
	{
		double Dt = 0.0, Dx = 0.0, Dy = 0.0;
		std::string Row;
		double WayX = 0.0, WayY = 0.0;
	};
	/** A burst's calls, in time order: its dome (one to three churns, or a ring of low spurts when it is low, nothing
	 * when it is lower still), its column and its boil. bHit: the burst the battle booked on the boat (its kill or its damage): a boil of her air follows,
	 * inside Lethal metres (the charge's lethal radius). */
	std::vector<KitSpout> SpoutsOf(const BurstLook& B, bool bHit, double Lethal, uint64_t Seed);
	/** The most churns one burst asks of the kit (its foam lasts most of a minute). */
	int MostChurns();

	/** A pattern as it goes in (the ChargeDrop event): its kind, how many, how wide (metres), seconds from the drop to the
	 * burst, seconds it takes to go in (Until), where they go in (At, the show's: her stern for a pattern, her forecastle
	 * for "ahead"), the pattern's middle (Aim), the escort's velocity (metres a second), and her length and beam. */
	struct DropIn
	{
		std::string Kind = "pattern";
		int N = 10;
		double Spread = 120.0, Sinking = 22.0, Until = 0.0;
		P3 At, Aim, Vel;
		double Long = 95.0, Beam = 10.0;
	};
	/** One charge of it, absolute: fired Dt seconds after the drop, from From (rolled off her stern's rails, thrown from
	 * a thrower on her side, or the ahead thrower on her forecastle), into the sea at To after Flight seconds, High metres
	 * over the higher end at the top of its arc. Side: 0 the rails, -1 or +1 the port or starboard thrower, 2 ahead. */
	struct DropShot
	{
		double Dt = 0.0, Flight = 0.5, High = 0.0;
		int Side = 0;
		P3 From, To;
	};
	std::vector<DropShot> DropPlan(const DropIn& In, uint64_t Seed);
	/** Where a thrown charge is, Age seconds into its flight from From to To, High metres over the higher end at its top. */
	P3 Arc(const P3& From, const P3& To, double High, double Flight, double Age);

	// -------------------------------------------------------------------------------------------------------------
	// Starshell

	struct StarIn
	{
		P3 Burst;           // where it burst (Z its height)
		double T0 = 0.0;    // when
		double Until = 0.0; // when it burns out
		P3 Wind;            // metres a second, the way it blows toward
		uint64_t Seed = 1;
	};
	struct StarOut
	{
		bool bShown = false;   // the flare (or, a moment after it burns out, its dead candle) in the sky
		bool bBurning = false;
		P3 Flare, Chute;       // the candle, and the parachute's crown above it
		double Light = 0.0;    // 0 to 1 of its full light: ignites, falls as it burns down and sinks, flickers, dies
	};
	StarOut StarAt(const StarIn& In, double T);
	/** How fast it sinks (metres a second): its parachute's rate, slowed if that would bring it down to the sea before it
	 * burns out. */
	double StarFall(double Height, double Burn);
	/** Seconds a starshell burns where the show gives no burn-out time. */
	double StarBurnUsual();
	/** The share of a burning flare's light (0 to 1) that falls on the sea at At. */
	double LitBy(const StarOut& O, const P3& At);
	/** The sky's own light on the sea (0 to 1): bLight, whether the recording gives its light; the sun's and the moon's
	 * height (degrees) and the moon's lit share. A recording without light is a day's. */
	double SkyLit(bool bLight, double SunUp, double MoonUp, double MoonLit);

	// -------------------------------------------------------------------------------------------------------------
	// Ends and rescue

	/** A boat crushed deep: her air coming up as a hump of white water over where she was, then boils dwindling. Dt after
	 * the end, Dx, Dy from her last place. */
	struct BoilShot
	{
		double Dt = 0.0, Dx = 0.0, Dy = 0.0;
	};
	std::vector<BoilShot> CrushPlan(uint64_t Seed);

	/** A scrambling net down a rescuer's side, toward the survivors: rope lengths (From, To), board metres. Ship: her
	 * waterline amidships; Aim: where the survivors are. */
	struct NetIn
	{
		P3 Ship, Aim;
		double Heading = 0.0, Long = 95.0, Beam = 10.0;
	};
	struct Rope
	{
		P3 From, To;
	};
	std::vector<Rope> NetOf(const NetIn& In);
}
