#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/**
 * BOW SPRAY (the look bible's S6, heavy weather: "Spray sheets over the bow from force 6; ice on rails and guns in the
 * Arctic and the winter Atlantic"; the release 2 ocean plan, steps 6(b) and 12(b): -TheaterSeaSpray=, tier 2). LOOKS ONLY
 * and off by default: the scene makes it only under -TheaterSeaSpray=1 (Battle/HeavyWeather, its kit calls), and nothing
 * the show decides reads it. Plain C++20, no engine: Tools/sea_waves/spray_test.cpp holds it to the plan.
 *
 * A ship throws a sheet of spray when her bow slams into the sea: when the water climbs her stem fast (the stem's
 * immersion rate) with her forefoot in it. The sheet fans out 30 to 60 degrees either side of the stem, rises by how
 * hard she struck (to her bridge on a destroyer in a Beaufort 6 head sea), is blown aft and down the wind, and falls
 * back in 1 to 3 s. Head and bow seas throw the most, following seas little: the sea meets her bow at the encounter
 * frequency, high steaming into it and low running before it. A sheet is drawn from Beaufort 4 for destroyers and boats,
 * 5 for cruisers (and merchants), 6 for battleships and carriers; green water over her forecastle and spindrift torn off
 * the sheet only from Beaufort 7, so only under the gale (-TheaterSeaGale, step 14(c), another thread's; winds to its
 * 24.4 m/s are taken, and without it the wind is held to SeaShow's 13.8 m/s, Beaufort 6's top). Each ship rests 1.5 s
 * after a sheet; only the 4 ships nearest the eye throw; at most 80 puffs of the effects kit's 3,000 are alive at once.
 *
 * Its input is shaped for the release 2 ride (step 5, not built yet): today the stem is driven from the show's table ride
 * (ShipOut's Heel, Pitch and Heave, whose period has no heading in it, so its rate is scaled by the encounter here) plus
 * the drawn water at the stem where the swell is drawn (FleetWaves through SeaNear, or 0 for today's still face); step 5's
 * LookHeave and LookPitch drop into Hull's Heave and Pitch with bTableRide false, and the encounter is then theirs.
 *
 * Places are board metres in SeaShow's Vec frame (X east, Y north, Z up); headings radians clockwise from north (ahead is
 * (sin, cos)); time the battle's seconds. Its own SplitMix64 stream (never the show's Dice or Roll), so the same battle
 * throws the same spray every time. Numbers: [P] published, [SHOW] the show's, [PLAN] the ocean plan's, [KIT] the effects
 * kit's recipe (Content/Effects/battle-fx.json), [EST] an estimate, to be judged on the Mac.
 */
namespace FleetSpray
{
	inline constexpr double Gee = 9.81;
	inline constexpr double Pi = 3.14159265358979323846;
	/** [PLAN] At most this many puffs alive at once (of the kit's game.max_puffs 3,000); the ships nearest the eye that
	 * throw; each ship's rest after a sheet (seconds). */
	inline constexpr int MostLive = 80, Nearest = 4;
	inline constexpr double Rest = 1.5;
	/** [SHOW] SeaShow's Raised (the most wind its sea is raised by, Beaufort 6's top) and [PLAN] the gale's cap (step 14(c)). */
	inline constexpr double Raised = 13.8, GaleCap = 24.4;
	/** [P] Ochi's threshold for slamming: the bow's velocity relative to the water 0.093 sqrt(g L) (M. K. Ochi, "Prediction
	 * of occurrence and severity of ship slamming at sea", 5th ONR Symposium on Naval Hydrodynamics, 1964: 12 ft/s for a
	 * 520 ft ship). [EST] Spray leaves her flare well before she slams: a sheet from SprayShare of it. */
	inline constexpr double OchiSlam = 0.093, SprayShare = 0.55;
	/** [EST] The sheet's speed off her flare: JetGain times her stem's immersion rate, plus SpeedGain of her own speed into
	 * a head sea (the spray of a bow meeting a wave is driven by their speeds together: Zakrzewski, "Splashing a ship with
	 * collision-generated spray", Cold Regions Science and Technology 14, 1987). A destroyer at 15 knots into a Beaufort 6
	 * head sea strikes at 7 to 8 m/s, so her sheet leaves at about 14 m/s and stands 10 m: her bridge. */
	inline constexpr double JetGain = 1.6, SpeedGain = 0.25;
	/** [EST] The spray's drops take this share of the wind across her deck (the wind less her own way) as they fly. */
	inline constexpr double Windage = 0.5;
	/** [KIT] The kit's Skip on "water" (its ap_miss row: 5 to 8 spurt puffs living 1.0 to 1.6 s, and 5 streaks living until
	 * they land and 2 s more): counted at its most, 13 puffs for 3.5 s. [EST] A naval spray row (asked of the effects
	 * thread) counted at 24 puffs for its life and 1 s more. */
	inline constexpr int SkipPuffs = 13, RowPuffs = 24;
	inline constexpr double SkipLife = 3.5, RowAfter = 1.0;

	/** A ship's bow as spray sees it: the Beaufort (rounded, WMO's) her bow first throws a sheet at, by the plan; her
	 * forecastle's height over the waterline at the stem; and her bridge's (the tallest a sheet stands is a little over
	 * it). Heights [EST], from the period's ships: a Fletcher's forecastle about 7 m (the battle modeller's American
	 * destroyer stands its forecastle deck at 7.1 m) and her open bridge about 11 m; a Type VII's bow casing about 2.5 m
	 * and her tower's bridge 5 m; a heavy cruiser's 9 m and 17 m; a loaded tramp's forecastle head 5 m and her bridge 12 m;
	 * a fast battleship's 10 m and 22 m; a fleet carrier's 12 m and her island's bridge 25 m. */
	struct Bow
	{
		int SheetFrom = 4;
		double Freeboard = 7.0, Bridge = 11.0;
	};
	/** Her bow by her class ("dd", "ss", "ca", "mv", "bb", "cv"); any other by her length. */
	Bow BowOf(const std::string& Class, double Long);

	/** The Beaufort number of a wind (WMO's, rounded: force 4 from 5.5 m/s, 7 from 13.9), the wind held to 13.8 m/s
	 * (Beaufort 6) unless the gale is on, and to the gale's 24.4 m/s. */
	int BeaufortOf(double WindU, bool bGale);
	/** The wind spray is made with: the wind held as BeaufortOf holds it (metres a second). */
	double WindFor(double WindU, bool bGale);

	/** One ship as the spray sees her this frame. Today her ride is the show's table (ShipOut's Heave and Pitch, bTableRide
	 * true); step 5's LookHeave and LookPitch drop in with bTableRide false. WaterAtStem: the drawn water's height at her
	 * stem (SeaNear, where the swell is drawn), 0 for today's still face. OffBow: the sea's bearing off her bow (radians,
	 * 0 head seas, plus from starboard, pi following), set by Step from the wind. bThrows: shown, afloat and in the line or
	 * limping (not going down), a boat only surfaced. */
	struct Hull
	{
		int Key = 0;
		std::string Class;
		double Long = 110.0, Beam = 11.0;
		double X = 0.0, Y = 0.0, Heading = 0.0, Speed = 0.0;
		double Heave = 0.0, Pitch = 0.0;
		double WaterAtStem = 0.0;
		double OffBow = 0.0;
		bool bTableRide = true;
		bool bThrows = true;
	};

	/** Her stem's height over the still waterline as her ride carries it (metres: her heave and her pitch, bow up, over
	 * half her length). Her stem's immersion is the water there less it. */
	double StemZ(double Long, double Heave, double PitchDeg);
	/** [P] How much faster than abeam the sea meets her bow: the encounter frequency at the wind sea's peak over its own,
	 * 1 + w V cos(off bow) / g, with w = 0.877 g / U (Pierson-Moskowitz's peak; E. V. Lewis, ed., Principles of Naval
	 * Architecture vol. III, 1989), held to 0.15 - 2.5 [EST] so a ship running before the sea still lifts a little. */
	double Encounter(double WindU, double Speed, double OffBow);

	/** What BowSea says of her bow this moment. Severity 0 to 1 by how far the rate is over the sheet's threshold; Jet the
	 * sheet's speed off her flare (m/s); Top its height over the water (m); Fan how far it spreads either side of her bow
	 * (degrees, 30 to 60); Life its rise and fall (s, 1 to 3); Head the cosine of the sea off her bow (1 head seas, -1
	 * following). bGreen: the sea comes aboard over her forecastle; bSpindrift: the gale tears the sheet's top away. */
	struct Sea
	{
		bool bSheet = false, bGreen = false, bSpindrift = false;
		double Severity = 0.0, Jet = 0.0, Top = 0.0, Fan = 0.0, Life = 0.0, Head = 0.0, Threshold = 0.0;
	};
	/**
	 * Her bow in the sea: Immersion the water over her stem's still waterline (metres, plus when she is deeper there),
	 * Rate how fast it climbs her stem (m/s, plus going in), Beaufort the rounded force (held to 6 without the gale), Class
	 * hers. A sheet when the force reaches her class's, the rate is over SprayShare of Ochi's slam and her forefoot is in
	 * the sea; green water and spindrift only from Beaufort 7 and only with bGale.
	 */
	Sea BowSea(const Hull& H, double Immersion, double Rate, double Beaufort, const std::string& Class, bool bGale = false);

	/** The wind: its speed (m/s) and where it blows from (radians clockwise from north), and whether the gale is on. */
	struct Wind
	{
		double U = 0.0, From = 0.0;
		bool bGale = false;
	};

	/** The effects kit's rows a sheet is sent through: the naval spray row (Sheet; one call, when the kit has it: the
	 * requires-probe HasSprayRow), or else the kit's existing Bow row, its low spurt on the water (Skip, "water"), laid on
	 * the sea (SeaSpurt) or on her deck (DeckSpurt: the kit's ground there is her deck). */
	enum class Row
	{
		Sheet,
		SeaSpurt,
		DeckSpurt,
	};
	enum class What
	{
		Sheet,
		Green,
		Spindrift,
	};
	/** One call on the kit: board metres; Way the way it is thrown (a spurt's level); Drift the wind across her deck the
	 * drops take (m/s); Top, Fan and Life the sheet's; Puffs counted against MostLive until Until. */
	struct Call
	{
		Row Kit = Row::SeaSpurt;
		What Is = What::Sheet;
		int Key = 0;
		double T = 0.0;
		double X = 0.0, Y = 0.0, Z = 0.0;
		double WayX = 0.0, WayY = 0.0, WayZ = 0.0;
		double DriftX = 0.0, DriftY = 0.0;
		double Top = 0.0, Fan = 0.0, Life = 0.0;
		int Puffs = 0;
		double Until = 0.0;
	};

	/** The spray's settings: the gale on (Beaufort 7 and over), the kit's naval spray row there (else the Bow row's
	 * spurts), the battle's seed. */
	struct Settings
	{
		bool bGale = false;
		bool bSprayRow = false;
		uint64_t Seed = 239;
	};

	/** What the spray did over a battle: sheets, green seas, spindrift, sheets dropped for want of room under MostLive,
	 * the most puffs alive at once, kit calls made. */
	struct Count
	{
		int Sheets = 0, Green = 0, Spindrift = 0, Dropped = 0, MostLive = 0, Calls = 0;
	};

	/** The spray of a battle, frame by frame. */
	class Spray
	{
	public:
		explicit Spray(const Settings& InSettings = Settings());
		/**
		 * One frame at battle time T: Ships every ship of the show (bThrows set by the caller; OffBow set here), the eye's
		 * ground point, the wind, and whether a picture run skips ahead (nothing thrown then, and the stems' rates are
		 * taken afresh after). Out gets this frame's kit calls, nearest ship first. A clock gone back starts afresh.
		 */
		void Step(double T, std::vector<Hull>& Ships, double EyeX, double EyeY, const Wind& W, bool bSkipping, std::vector<Call>& Out);
		/** The puffs alive at T, as counted. */
		int Live(double T) const;
		const Count& Counted() const { return Tally; }
		/** The keys of the ships that may throw this frame (the Nearest to the eye, nearest first). */
		const std::vector<int>& Throwing() const { return Near; }
		const Settings& Set() const { return Given; }
		/** The line said once a battle (the plan's step 12(b): live puffs counted and logged per battle). */
		std::string Line() const;

	private:
		struct Track
		{
			double T = -1.0e9, StemRide = 0.0, Water = 0.0, Rate = 0.0, Immersion = 0.0;
			double LastSheet = -1.0e9, SeenAt = -1.0e9;
			bool bArmed = true, bHave = false;
			uint64_t Sheets = 0;
		};
		struct Alive
		{
			double Until = 0.0;
			int Puffs = 0;
		};
		Settings Given;
		std::map<int, Track> Tracks;
		std::vector<Alive> Puffs;
		std::vector<int> Near;
		std::vector<Call> Event;
		Count Tally;
		double LastT = -1.0e9;
		bool Fits(double T, const std::vector<Call>& Calls) const;
		void Throw(double T, const Hull& H, const Sea& S, const Wind& W, std::vector<Call>& Out);
	};

	/**
	 * The requires-probe for a naval spray row (the release 2 ocean plan's ask 3 to the effects thread): a kit with
	 * BowSpray(At, Way, Drift, TopMetres, FanDegrees, Seconds, Kind) takes a sheet whole, its fan, height and fall, in place
	 * of the Bow row's low spurts; Kind "sheet", "green" or "spindrift"; Drift the wind across her deck (world units a
	 * second). Instantiated by the scene with the engine's types and by the cloud test with its own.
	 */
	template <typename TKit, typename TVec, typename TText>
	inline constexpr bool HasSprayRow = requires(TKit* K, const TVec& A, float F, const TText& W) { K->BowSpray(A, A, A, F, F, F, W); };

	/** The kit row's word for a call's kind. */
	const char* KindWord(What Is);
}
