#pragma once

#include "Battle/ConvoyLooksCore.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SeaShow
{
	class Show;
}

/**
 * Night fighting at sea, its looks (the look bible's S7: "Star shells hang and light the water; gun flashes light the
 * hulls; dark water still shows its waves"; the release 2 sea's -TheaterSeaNightFight=, off by default): which ship lights
 * which target with her searchlight and when, and the star shells a ship fires in a fleet action at night. Plain C++20,
 * no engine: ASeaBattle's FSeaNight (Battle/SeaNight) draws what is worked out here, and the cloud test
 * (Tools/sea_waves/run_night.sh, sea_night_test.cpp) checks it.
 *
 * LOOKS ONLY. The show (SeaShow) decides where every ship is, whom she fires on and when, and what each side has in
 * sight; nothing here changes any of that, and nothing the show decides reads anything from here. What is decided here
 * (whether a ship unmasks her light, when, for how long; a star shell's burst point) comes from this file's own
 * SplitMix64 stream keyed by the battle's seed and the ships' keys, never from the show's dice.
 *
 *  - Only at night: the recording's light (Field bLight) with the sun more than 6 degrees under the horizon, and the
 *    night share (0 night to 1 day) under a half. A recording with no light is a day's (ConvoyLook::SkyLit), so the
 *    three fleet actions as recorded draw nothing new at all.
 *  - Never on a target not in sight: a beam is laid only on a ship the show draws (ShipOut::bShown) and, for an enemy
 *    boat of a side that is not the player's, only on one a ship of that side holds (its echo on her, by sight, radar or
 *    asdic; its gun on her; her view saying it is running in on her; or her own gun on that side). A beam that loses her
 *    holds the place she was last lit at, never where she has gone. A beam that sweeps is laid by its own ship's place
 *    and her side's ships alone, so a hidden boat changes nothing of it.
 *  - Convoy nights (any boat or merchant in the battle): an escort holds a beam on a surfaced boat in sight within her
 *    light's reach (two beams at most on one boat), and when a ship of her side is struck, an escort near her may sweep
 *    her outer bow a while. The star shells are the rules' (sea:starshell, ConvoyLooks), never doubled here.
 *  - Fleet actions: a ship firing at close range may unmask her light on her target, briefly (period practice: lights
 *    were unmasked after contact and drew fire), coming on searching onto her and held, then doused; a ship opening fire
 *    beyond a few kilometres fires a spread of star shells bursting beyond her target along the line of fire, so the
 *    target shows black against their light (US Navy practice), at most MostStarsPerSide burning a side.
 *  - At most MostBeams beams at once in all, a lit beam keeping its place until it is doused; a hold takes a sweeping
 *    beam's place when none is free.
 *
 * Places are board metres (X east, Y north, Z up from the sea), the show's own; headings degrees clockwise from north.
 * Numbers: [P] a published figure, named where it is set; [SHOW] the show's own (SeaShow.cpp), which these must agree
 * with; [CL] the convoy looks' (ConvoyLooksCore.cpp), reused read-only; [EST] this lane's own estimate.
 */
namespace SeaNight
{
	using P3 = ConvoyLook::P3;

	/** At most this many searchlight beams at once, in all (the frame budget: a shadowless spot light and a few hundred
	 * triangles each). */
	constexpr int MostBeams = 4;
	/** At most this many fleet star shells burning a side at once. */
	constexpr int MostStarsPerSide = 6;

	// -------------------------------------------------------------------------------------------------------------
	// The night

	/** The recording's light (SeaShow::Field's bLight, SunUp, MoonUp, MoonLit, MoonFrom, Seeing). */
	struct Light
	{
		bool bGiven = false;
		double SunUp = 90.0, MoonUp = -90.0, MoonLit = 0.0, MoonFrom = 0.0, Seeing = 0.0;
	};
	/** The night share (0 night to 1 day) from the recording's light: ConvoyLook::SkyLit ([CL]); in field mode step 11
	 * hands the battle day's own UnlitShare to Night::SetShare in its place (ASeaBattle::OpenOn). */
	double ShareOf(const Light& L);
	/** Night fighting's night: the light given, the sun more than 6 degrees under (civil twilight's end), and the share
	 * under a half. */
	bool IsNight(const Light& L, double Share);

	// -------------------------------------------------------------------------------------------------------------
	// Searchlights by navy and size

	struct Lamp
	{
		const char* Navy = "";   // "USA", "JAP", "UK", "GER", "ITA", or "" for any other
		const char* For = "dd";  // the class that carries it: "dd", "ca", "bb", or "big" (a cruiser and a battleship)
		const char* Size = "";   // as the navy named it: "24-inch", "110 cm"
		double MirrorM = 0.6;    // the mirror's diameter (metres): the beam's width at the lamp
		double Candela = 1.0e8;  // its beam's peak intensity, candelas (the real lamp's)
		double Reach = 3000.0;   // metres: how far it lights a ship well enough to lay guns on her
		double Spread = 0.012;   // radians: the pencil beam's half angle
		double High = 12.0;      // metres over her waterline where it stands
		double Along = 0.0;      // and where along her, a share of her length forward of amidships
	};
	/** The searchlight of a ship of a class ("bb", "ca", "dd"; any other warship as "ca") and navy. */
	const Lamp& LampOf(const std::string& Class, const std::string& Nation);
	/** Whether a ship of the class carries a light she fights with (a warship: not a boat, a merchant or a carrier). */
	bool Lights(const std::string& Class);
	/** The share of ships of a navy that unmask their light when firing at close range at night [EST], and the most a
	 * fleet action's light reaches for it (metres). */
	double UnmaskOdds(const std::string& Nation);
	constexpr double CloseRange = 6000.0;

	// -------------------------------------------------------------------------------------------------------------
	// A frame of the show, as the night reads it

	struct ShipIn
	{
		int Key = 0, Piece = 0, Side = 0;
		std::string Class, Nation;
		P3 At;                 // her waterline amidships
		double Heading = 0.0;  // degrees clockwise from north
		double Long = 180.0, Beam = 20.0;
		bool bShown = true;    // the show draws her (SetMine: an enemy boat not found is not)
		bool bAfloat = true;   // in her line or limping away, or not yet a third of the way under
		bool bSurfaced = true; // a boat up, her casing clear (a ship: always)
		bool bStruck = false;  // hit and going down, or burning (a convoy's sweep after an attack)
		int Aim = -1;          // the ship her guns are laid on (her key), -1 none
		double FiredAt = -1.0; // battle time her guns last fired, below 0 never
	};
	struct PieceIn
	{
		int Id = 0, Side = 0;
		int EchoOn = -1;          // an escort's echo: the piece it is on, and how ("asdic", "radar", "sight", "bearing")
		std::string EchoKind;
		int GunOn = -1;           // the piece its gun is told on
		std::vector<int> Heard;   // a boat: the escorts running in on her now (bHeard: as her view says)
		bool bHeard = false;
	};
	struct Frame
	{
		double T = 0.0;
		int Mine = -1;            // the player's side (Show::MineSide), -1 every ship drawn
		std::vector<ShipIn> Ships;
		std::vector<PieceIn> Pieces;
		P3 Wind;                  // metres a second, the way it blows toward
		int RulesStars = 0;       // star shells the rules sent (sea:starshell) so far
	};
	/** The show's frame as the night reads it: its ships, its pieces' contacts, its wind. Read only. */
	void FrameOf(const SeaShow::Show& Show, Frame& Out);
	/** Her side has her in sight, as the show has it (above): Side's ships may light her. */
	bool InSightFor(const Frame& F, int Side, const ShipIn& Her);

	// -------------------------------------------------------------------------------------------------------------
	// What is drawn

	enum class Doing : uint8_t
	{
		Searching, // coming on, swinging onto her (or sweeping, with no target)
		Holding,   // held on her
		Lost,      // on the place she was last lit at, then swinging about it
		Dousing,   // going out
	};
	struct Beam
	{
		int Lighter = -1, Side = 0;
		int Target = -1;          // the ship it is on now (-1: still swinging onto her, a sweep, or she is lost)
		int Laying = -1;          // the ship it is being laid on or held on (-1: a sweep, or she is lost)
		Doing What = Doing::Searching;
		bool bSweep = false;      // a convoy's sweep: no target at any time
		P3 From;                  // the lamp
		P3 To;                    // where its axis ends: her hull, or the sea
		double Long = 0.0;        // metres from the lamp to To
		double MirrorM = 0.6, Spread = 0.012;
		double Candela = 1.0e8;   // the real lamp's
		double Light = 0.0;       // 0 to 1: its shutter, coming up and going out
		double Shaft = 0.0;       // 0 to 1: how much of its shaft shows in the night's haze and smoke
		std::string Size;         // the lamp's own name ("36-inch")
	};
	struct Star
	{
		int Key = 0;              // its own
		int Side = 0, Firer = -1, Target = -1;
		P3 FiredFrom, AimAt;      // the firer and her target when it was fired
		ConvoyLook::StarIn In;    // [CL] its burst, burn-out and wind: ConvoyLook::StarAt flies it
	};

	/** The night's own counts (the cloud test reads them). */
	struct Tally
	{
		int Unmasked = 0, Held = 0, Sweeps = 0, Refused = 0, Preempted = 0, Spreads = 0, StarsFired = 0, StarsRefused = 0;
		int MostBeams = 0, MostStarsSide = 0;
	};

	/**
	 * The night fighting of one battle. Begin once (the battle's seed and light; the share below 0 takes it from the
	 * light), then Step once a frame with the show's frame; read Beams() and Stars() after. Same seed, same frames: the
	 * same beams and stars, on any machine.
	 */
	class Night
	{
	public:
		void Begin(uint64_t Seed, const Light& InLight, double Share = -1.0);
		/** The night share handed in (the battle day's UnlitShare, in field mode): 0 night to 1 day. */
		void SetShare(double Share);
		double Share() const { return DayShare; }
		/** Night fighting's night now (IsNight): nothing is lit or fired otherwise. */
		bool IsOn() const { return bNight; }
		/** How strong the unlit shafts and glows are drawn: 1 at full night, less under a moon, 0 by day. */
		double Glow() const { return bNight ? 1.0 - DayShare : 0.0; }
		void Step(const Frame& F);
		/** A picture run's skip, or the clock gone back: every beam out at once, nothing carried over. */
		void Reset();
		const std::vector<Beam>& Beams() const { return OutBeams; }
		/** The fleet star shells alive (fired, flying, burning, or just burnt out): fly each with ConvoyLook::StarAt. */
		const std::vector<Star>& Stars() const { return OutStars; }
		const Tally& Counted() const { return Count; }
		/** A number from 0 to under 1 from the night's own SplitMix64 stream at Key (never the show's dice). */
		double Draw(uint64_t Key) const;

	private:
		struct Lighter
		{
			Doing What = Doing::Searching;
			bool bLit = false, bSweep = false;
			int Target = -1, Episode = 0;
			double Since = 0.0, Until = 0.0, OnAt = 0.0, OutAt = -1.0, NextTry = -1e9;
			double LastFired = -1.0;  // (her guns' last firing seen)
			double SwingFrom = 0.0;   // (radians off her target's bearing as it came on; a sweep's middle bearing)
			double SweepRate = 0.0;   // (radians a second; a sweep's half arc in SwingFrom's place below)
			double SweepHalf = 0.0;
			P3 LostAt;                // (the place she was last lit at)
			double LostBearing = 0.0;
			double StruckSeen = -1.0; // (a convoy's sweep: the attack it answers)
			double StarsUntil = -1.0; // (her last spread's burn-out)
			int StarTarget = -1;
			int Spreads = 0;
			Beam Drawn;
		};

		uint64_t SeedKey = 0;
		Light Sky;
		double DayShare = 1.0;
		bool bShareGiven = false, bNight = false;
		double LastT = -1e9;
		std::map<int, Lighter> Lighters;
		std::map<int, double> StruckAt;  // (ships of each side seen struck, and when: a convoy's sweeps answer them)
		std::vector<Star> AllStars;
		std::vector<Beam> OutBeams;
		std::vector<Star> OutStars;
		int NextStar = 1;
		Tally Count;

		void StepBeams(const Frame& F, bool bConvoy);
		void StepStars(const Frame& F, bool bConvoy);
	};
}
