#pragma once

#include "Battle/AirFeed.h"
#include "Battle/AirSky.h"
#include "Battle/SeaLookSwitches.h"

#include <cstdint>
#include <string>
#include <vector>

/**
 * THE FLEET'S SKY, in plain C++ (the release 2 ocean plan, step 13: clouds and weather, tier 3), so that the cloud test
 * lays, paints and checks exactly what the sea scene draws (Battle/SeaWeather is the engine's side).
 *
 * The clouds are the air battle's own (Battle/AirSky): the same laying, the same puffs, the same painter, the same deck
 * shade, so a storm at sea and a deck over Bremen are one sky. What is the sea's own:
 * - Which day is laid (Choose): the rules' field.sky {cover, kind, base, top, rain, word}, as air's sky.place.cloud is read
 *   (AirFeed); with none, nothing (a clear day, as today), unless the stand-in switch lays air's own stand-in cumulus
 *   (AirSky::Day as AirFeed leaves it when the bundle gives none); "-TheaterSeaWeather=deck,0.8" lays a day for review
 *   pictures, as "-TheaterWeather=" does for air.
 * - Where (PlacingFor): the battle's seed, the board's middle as the target (cumulus thin out over it, as over a raid's
 *   target, so the overview at 1,300 m sees its ships), no ships' hole in a deck (a fleet is seen from the sea, not from
 *   a bomber), no bombers' band (a deck stays at the day's own base and top), the key body as the sun, the field's own
 *   wind (AirSky's Placing::Wind), and room for 1,500 puffs.
 * - The dome (PaintDome): the laid puffs as the shot's own eye sees them, painted into a picture of the whole sky,
 *   DomeW by DomeH, the columns even in bearing and the rows even in the sine of the height above the horizon (so a low
 *   eye's sky near the horizon, where nearly all the cloud is, has the most rows: 11.4 a degree across and 9 a degree up
 *   at the horizon, against a 50 degree lens's 21.5 pixels a degree on a 1,077 pixel screen). The painter is AirSky's own
 *   (Painter::Paint, its shapes from the sheet of torn fragments where given), and each of its triangles is laid on the
 *   picture by the line from the eye through each pixel, so a cloud stands in the picture exactly where the line from
 *   the eye to it points. The scene's material (M_SkyDome, Tools/build_content.py) reads it by the camera's own way,
 *   u the bearing (east of north) over a turn, v one less the sine of the height: DomeUv.
 * - The shade (ShadeOf): a deck's is AirSky::ShadeOf's, the light function M_DeckShade reads; a cumulus day's, each cloud's
 *   shadow laid in the same picture where the sun's line through the cloud's middle crosses the picture's height, so
 *   the same light function lays each shadow on the sea and the ships down-sun of its own cloud.
 * - Rain (RainUnder): up to six far curtains under the darkest clouds, 6 to 18 km from the board's middle, laid as the air
 *   battle's rain shafts (four crossed sheets, thinning down to a mist at the sea, lagging upwind of the cloud), and the
 *   heaviest one's footprint as the squall the sea's material will darken and roughen (SquallOf).
 * Nothing here counts and nothing here is read by the show: it is only what is drawn. It draws from AirSky's own seeded
 * stream, never the show's.
 *
 * Places are board metres (X east, Y north, Z up, the sea at 0), as AirSky's.
 */
namespace SeaSky
{
	using Vec = AirSky::Vec;
	using Rgb = AirSky::Rgb;

	// ---- The switches -----------------------------------------------------------------------------------------------

	/** What is drawn of the clouds: none (today), the dome, or the puffs themselves (drawn as the dome until puffs mode
	 * lands: the plan's step 13(e)). */
	enum class Clouds
	{
		Off,
		Dome,
		Puffs
	};

	/** The five weather switches as the scene takes them (Take). */
	struct Asked
	{
		/** -TheaterSeaClouds=0|dome|puffs. */
		Clouds Draw = Clouds::Off;
		/** -TheaterSeaCloudShade=1: the clouds' shade on the sea and the ships, by the key lamp's light function. */
		bool bShade = false;
		/** -TheaterSeaRain=1: the far rain curtains. */
		bool bRain = false;
		/** -TheaterSeaSkyStandIn=1: with no sky from the rules, air's own stand-in cumulus. */
		bool bStandIn = false;
		/** -TheaterSeaWeather=kind,cover[,rain]: a day asked for review pictures ("deck,0.8", "0.3" for cumulus). */
		bool bWeather = false;
		bool bWeatherDeck = false;
		double WeatherCover = 0.0;
		/** Its rain, 0 to 1 (below 0: as the cover gives, RainOf). */
		double WeatherRain = -1.0;
		/** Anything drawn at all (with none of the three, nothing is laid and nothing changes). */
		bool Any() const { return Draw != Clouds::Off || bShade || bRain; }
	};

	/** The five switches from the look the scene read (SeaLookSwitches::Read), each one's value in force written back so
	 * the "Sea look:" line says what was applied: "-TheaterSeaClouds=dome", "puffs" drawn as "dome" until puffs mode
	 * lands, anything else "0"; the three of 0 or 1 as SeaLookSwitches::TakeOnOff takes them; the review day as read
	 * ("deck,0.80", "cumulus,0.30", ",0.60" its rain), or "none" when it cannot be read. */
	Asked Take(SeaLookSwitches::Look& InForce);

	// ---- The day ----------------------------------------------------------------------------------------------------

	/** The rules' sky for the action (field.sky), as read. */
	struct Given
	{
		bool bGiven = false;
		AirSky::Day Day;
		/** How heavy its rain is, 0 to 1 (light 0.3 about 1 mm an hour, moderate 0.6 about 4, heavy 1 about 16 [EST]);
		 * below 0, not given. */
		double Rain = -1.0;
	};
	/** field.sky {cover, kind ("deck" or "cumulus"), base, top (metres over the sea), rain, word} of a recording's field,
	 * read as AirFeed reads air's sky.place.cloud (the same fallbacks: cumulus, base 950, top 2,500, the cover not given
	 * below 0). Absent: not given. */
	Given ReadField(const AirFeed::Json& Field);
	/** The same from a recording's whole text; not given (and why, in OutError) where it cannot be read. */
	Given ReadRecording(const std::string& Text, std::string* OutError = nullptr);

	/** Air's own stand-in cumulus: the sky AirFeed leaves for a bundle that gives none (AirSky::Day as made: cumulus,
	 * its cover the art director's passed 0.39). */
	AirSky::Day StandIn();

	/** The sky the scene lays. */
	struct Sky
	{
		bool bLaid = false;
		/** Laid from air's stand-in cumulus, the rules having sent none. */
		bool bStandIn = false;
		/** Laid as asked for review (-TheaterSeaWeather=). */
		bool bAsked = false;
		/** A cumulus day of StratoFrom or more, drawn as a broken deck of stratocumulus (Day is that deck). */
		bool bStrato = false;
		AirSky::Day Day;
		/** The cover the sky reads as, 0 to 1: the day's, held to what its kind can draw (AirSky::Day::CoverLaid; cumulus
		 * to CumulusReach, the broken stratocumulus to StratoReach). A cumulus day is laid at more (Day.Cover) so that it
		 * paints this much, the stratocumulus with its gaps opened to it (DeckHoles); the night dome hides its stars by
		 * this, and the cumulus light is this cover's (ReadAs). */
		double Cover = 0.0;
		/** The broken stratocumulus's gap threshold (StratoHolesFor), which Lay hands AirSky (Placing::DeckHoles); below
		 * 0, air's own for the cover laid. */
		double DeckHoles = -1.0;
		/** How heavy the rain is, 0 to 1 (RainOf, from Cover). */
		double Rain = 0.0;
		/** Why, for the log: "the rules' sky", "stand-in sky: the rules sent none", ... */
		std::string Why;
	};
	/** The rules' sky where given; else, with the stand-in switch, air's stand-in cumulus; else none (a clear sky, as
	 * today). A review day asked overrides both: its kind and cover over the rules' base and top (or the table's middles). */
	Sky Choose(const Given& FromRules, const Asked& Switches);

	/** How heavy the rain is, 0 to 1: as given; else a heavy cumulus day's showers as the air battle has them (none at
	 * 0.45 of cover, three of the six curtains from 0.55), and a closed deck's rain (none at 0.85, three curtains at 0.97
	 * and over [EST: a sky of nine tenths or more under nimbostratus is mostly a raining one]). */
	double RainOf(const AirSky::Day& D, double GivenRain);

	/** What cumulus laid at 0, 0.05, ... AirSky::CumulusMost paints of the whole dome seen from the sea (each pixel the
	 * same piece of sky, as a sky's eighths are counted), measured by Tools/sea_waves/cover_table over nine seeds of 20
	 * eyes (25 November). Soft, torn puffs, and the farthest left out of the painter's room, paint about two thirds of the
	 * footprint a cover lays. */
	constexpr double CumulusPainted[12] = { 0.0, 0.031, 0.069, 0.091, 0.103, 0.152, 0.184, 0.239, 0.264, 0.291, 0.332, 0.346 };
	/** The most of the sky cumulus paints at sea: CumulusPainted at AirSky::CumulusMost. A cumulus day of more, under
	 * StratoFrom, reads as this much (from there the broken stratocumulus paints more). */
	constexpr double CumulusReach = CumulusPainted[11];
	/** The cumulus cover a sea battle lays for a day's cover, so that the sky seen from the sea paints what the day says
	 * (the sea scene's follow-up, 25 November: a cumulus day of 0.5 painted 0.22 of the sky overhead, nearly clear to
	 * the eye): CumulusPainted read backward, between its steps in a line; from CumulusReach, AirSky::CumulusMost. 0 at
	 * none, and never less as the cover rises. The sea's own: air's cumulus is laid as given. */
	double CumulusLaidFor(double Cover);
	/** A cumulus day of this much cover or more is drawn at sea as a broken deck of stratocumulus (the sea scene's ruling,
	 * 25 November: cumulus paints at most CumulusReach of the sky, so a half-cloudy day drawn as cumulus reads nearly
	 * clear). A field holds one kind of cloud, so the change is made at once, and nobody sees it happen: a battle keeps
	 * one cover from start to end. Its word, its rain and its share of the sky stay the day's own. */
	constexpr double StratoFrom = 0.40;
	/** Its deck: laid at AirSky::DeckLeast, its base the day's held to StratoBaseLeast to StratoBaseMost [EST: a fair
	 * weather stratocumulus sheet over the sea lies some 500 to 1,500 m up, a few hundred metres thick], its top
	 * StratoDepth over its base, its gaps opened (AirSky::Placing::DeckHoles) so that the sky reads as the day's cover. */
	constexpr double StratoBaseLeast = 600.0, StratoBaseMost = 1000.0, StratoDepth = 700.0;
	/** The gap thresholds measured (AirSky::Placing::DeckHoles, the noise under which a tile is left out; air's own deck
	 * at DeckLeast leaves out under 0.48), and what each paints of the whole dome seen from the sea, measured as
	 * CumulusPainted by Tools/sea_waves/cover_table (its strato mode, base 800 m) over nine seeds of 20 eyes (25
	 * November). */
	constexpr double StratoHoles[9] = { 0.48, 0.50, 0.52, 0.54, 0.56, 0.58, 0.60, 0.62, 0.64 };
	constexpr double StratoPainted[9] = { 0.619, 0.582, 0.540, 0.498, 0.453, 0.408, 0.355, 0.302, 0.257 };
	/** The most of the sky the broken deck paints at sea (its fewest gaps): a day of more reads as this much. */
	constexpr double StratoReach = StratoPainted[0];
	/** The gap threshold that paints a cover: StratoPainted read backward, between its steps in a line; the first
	 * threshold from StratoReach up, the last under the least it paints. */
	double StratoHolesFor(double Cover);
	/** The chosen day as it reads (its light, AirSky::LightOf): a cumulus day with the cover it paints (Sky::Cover); a
	 * deck, the broken stratocumulus among them, as laid. */
	AirSky::Day ReadAs(const Sky& Chosen);
	/** The chosen day's light under its cloud at Under (0 over the deck's top to 1 under its base), as shares of a clear
	 * day's: AirSky::LightOf(ReadAs), and under the broken stratocumulus the laid deck's taken only as far as its cover
	 * reads of the most it paints (Sky::Cover over StratoReach: a deck laid at DeckLeast whatever its gaps, its light
	 * reckoned from that cover, would read darker than its gaps let through; the air scene's note, 25 November). */
	AirSky::Lighting LightOf(const Sky& Chosen, double Under);

	/** The most puffs the painter draws at once (the plan's ledger: 1,500). */
	constexpr int MostPuffs = 1500;
	/** Where the sky is laid for the action: Seed the battle's, the target the board's middle, no ships' hole, no
	 * bombers' band, Sun the way to the key body, Wind the field's (metres a second, the way it blows toward), the sea
	 * under it. */
	AirSky::Placing PlacingFor(uint64_t Seed, double MiddleX, double MiddleY, const Vec& Sun, const Vec& Wind);
	/** AirSky::Lay of the chosen day, its gaps as chosen (Sky::DeckHoles, over the placing's); an empty field when none
	 * is laid. */
	AirSky::Field Lay(const Sky& Chosen, const AirSky::Placing& P);

	// ---- The dome ---------------------------------------------------------------------------------------------------

	constexpr int DomeW = 4096, DomeH = 512;

	/** The picture's place of a way from the eye (a unit, board axes): its column (the bearing east of north over a turn,
	 * times the width) and its row (one less the way's height sine, times the height), each real, a pixel's middle at
	 * half past its number. Rows past the height are under the horizon. */
	void PlaceOf(const Vec& Way, int W, int H, double& OutColumn, double& OutRow);
	/** The way from the eye through a place of the picture (a unit, board axes). */
	Vec WayOf(double Column, double Row, int W, int H);
	/** The material's texture coordinates for a way in the world's axes (X the board's north, Y its east, Z up), as
	 * M_SkyDome works them out: u = frac(atan2(Y, X) / 2 pi), v = 1 - Z. */
	void DomeUv(double WorldX, double WorldY, double WorldZ, double& OutU, double& OutV);

	/** The sky's picture from one eye. */
	struct Dome
	{
		int W = DomeW, H = DomeH;
		/** Four bytes a pixel, rows from the zenith down: red, green and blue half the cloud's light (128 the light painted,
		 * 255 twice it: the fragment sheet's measure, so a sunlit rim the sheet lights past white keeps it), linear; alpha
		 * how much cloud. Where there is no cloud, the colour of the nearest cloud beside it, so the chip's smoothing
		 * between pixels draws no dark rim. */
		std::vector<uint8_t> Rgba;
		/** The eye and the wind's carry it was painted for, and the battle's second (Painted). */
		Vec Eye, Drift;
		double Painted = 0.0;
		/** The puffs the painter drew, and those it had no room for. */
		int Puffs = 0, Dropped = 0;
		/** The triangles laid, and the pixels they were tested on. */
		int64_t Triangles = 0, Tested = 0;
		/** How much cloud at a way (the nearest pixel), 0 to 1. */
		double AlphaAt(const Vec& Way) const;
		/** Its share of the whole sky, each pixel the same piece of sky (rows even in the sine). */
		double Cover() const;
	};

	/** How a dome is painted. */
	struct DomeHow
	{
		int W = DomeW, H = DomeH;
		/** Rows painted, from the zenith (the rest left empty): the whole sky by default. */
		int Rows = -1;
		/** The sheet of torn fragments (AirSky::MakeFragments) to draw each puff as one of its shapes, as M_Cloud draws
		 * them; none, soft discs. */
		const std::vector<uint8_t>* Sheet = nullptr;
		/** The scene's cloud ink (the air battle's CloudBright, 1.3): the painter paints the shaded side, the base and
		 * the shadows that much darker, and M_SkyDome lifts the whole by it, so only the sunlit tops rise. */
		double Ink = 1.3;
	};

	/**
	 * Paints the laid clouds as the eye sees them: AirSky's painter turns the puffs to the eye (Painter::Paint, its
	 * shadows laid apart and not drawn: they lie on the sea), and each of its triangles is laid on the picture by the line
	 * from the eye through each pixel's middle (where that line meets the triangle, its colour and alpha taken between
	 * the triangle's points as they lie there), far to near, each over what is behind it. Painter is the caller's: it keeps
	 * its own memory from one paint to the next (a cloud's lift; none at sea), and nothing else may use it meanwhile.
	 * Light is the hour's light on cloud (white by day; the scene's day light share is the material's). The same field,
	 * eye, drift, sun and light give the same picture, byte for byte.
	 */
	void PaintDome(const AirSky::Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light, AirSky::Painter& Painter, Dome& Out,
		const DomeHow& How = DomeHow());

	/** A deck's cover under which its gaps are left open (seven eighths: broken); from it to a full overcast the dome
	 * shuts them behind the lids (DeckClosing). */
	constexpr double DeckOpenUnder = 0.875;
	/** The widest the painted cover overhead (the caps over 60 degrees) falls short of the field's over the dealt sweep,
	 * none to a full overcast in tenths (Tools/sea_waves/sea_sky_test): a ceiling the test holds, for the plan's cover
	 * line. Measured 25 November at 0.268 and, with cumulus laid to read (CumulusLaidFor), 0.263 (g++) and 0.257 (clang),
	 * both at a cumulus day of 0.5; with that day drawn as broken stratocumulus (StratoFrom), 0.186 on both, at a deck of
	 * 0.7 (air's own, overhead 0.51), the half-cloudy days now overhead 0.28 at 0.4 and 0.37 at 0.5. */
	constexpr double WorstCoverGap = 0.19;
	/** How much of a deck's gaps the dome shuts behind its lids, 0 to 1: none under DeckOpenUnder of cover laid, all of
	 * them at a full overcast; none on a cumulus day, and none from an eye at or over the deck's base. */
	double DeckClosing(const AirSky::Field& F, const Vec& Eye);

	/** The overview shot's eye (the plan's line 941, the sea scene's rule): the scene's own 1,300 m; under a deck the
	 * dome draws, 150 m under its base, never under 300 m, since a dome over the eye shows no cloud below it. Fixed for
	 * the battle: the day is laid once. */
	constexpr double OverviewMost = 1300.0, OverviewUnderBase = 150.0, OverviewLeast = 300.0;
	double OverviewHeight(bool bDeckDrawn, double BaseMetres);

	// ---- The shade --------------------------------------------------------------------------------------------------

	/** The sun's share under the heart of a cumulus cloud [EST: a fair-weather cumulus some 0.5 to 1.5 km deep lets
	 * through a few per cent of the direct beam; its thinner edges and the light scattered forward through them, about a
	 * fifth under its heart as the light function lays it, softened over its edge]. */
	constexpr double CumulusThrough = 0.2;
	/**
	 * The clouds' shade on the sea and everything on it, as the key lamp's light function reads it (M_DeckShade: each lit
	 * point read up the sun's way to where that way crosses the picture's height Z): a deck's AirSky::ShadeOf(F, Side), the
	 * same picture byte for byte; a cumulus day's, each cloud of 600 m or more (as air lays shadows) as much as it is shown
	 * with the wind's carry Drift, its shadow the breadth of its body (0.42 of its width), stretched down-sun as the sun is
	 * lower, as the air painter's shadows are, and laid in the picture at the place where the sun's line through its
	 * middle (0.45 of its height over its foot) crosses Z: so the light function lays it on the sea where that line meets
	 * the sea. The picture is in the field's own frame: the scene moves its corner with the drift.
	 */
	AirSky::DeckShade ShadeOf(const AirSky::Field& F, const Vec& Sun, const Vec& Drift = Vec(), int Side = 256);
	/** Where the sun's line through a cloud's middle crosses a height: its middle's place on a cumulus shade picture laid
	 * at Z (ShadeOf), in the field's frame. */
	Vec ShadowPlace(const AirSky::Field& F, int CloudIndex, const Vec& Sun, double Z);

	// ---- Rain -------------------------------------------------------------------------------------------------------

	/** One far rain curtain: under a cloud (Cloud) at Mid on the board (as laid, nearest the target), from inside its base
	 * (Top) down to the sea (Foot 0), Radius across, its foot lagging upwind by Lag. */
	struct Curtain
	{
		Vec Mid;
		double Top = 0.0, Foot = 0.0, Radius = 0.0;
		Vec Lag;
		int Cloud = -1;
		/** Its sheets' alpha at their thickest. */
		double Peak = 0.0;
	};
	/** The curtains and their mesh: four crossed sheets a curtain of 5 by 5 points (as the air battle's shafts), both
	 * faces of every triangle, the points in board metres as laid (the scene adds the wind's carry), each point's alpha. */
	struct Rain
	{
		double Heavy = 0.0;
		std::vector<Curtain> Curtains;
		std::vector<Vec> Points;
		std::vector<uint8_t> Alpha;
		std::vector<int> Triangles;
	};
	/** The most curtains (the plan: up to six). */
	constexpr int MostCurtains = 6;
	/** How near the board's middle and how far from it a curtain may stand (metres): far rain only (the rain at the eye is
	 * the weather fall's, FWeatherFall), and under clouds the dome draws (AirSky::Reach). */
	constexpr double CurtainsFrom = 6000.0, CurtainsTo = 18000.0;
	/**
	 * The far rain under the darkest clouds (the biggest, tallest, least lit, shown whole), at least 5 km apart: as many
	 * as six times Heavy (0 to 1, RainOf), rounded. A cumulus shower's curtain about a quarter of its cloud across
	 * (500 m to 2.2 km, as air's); a deck's rain a heavier band in the general rain, one and a half tiles across (1.5 to
	 * 3 km [EST: the bands of heavier rain in frontal rain are a few kilometres across]). Each sheet's alpha at its
	 * thickest 0.1 + 0.2 Heavy [EST: rain of 1 to 16 mm an hour dims a line through a curtain 1.5 km across by roughly a
	 * third to nine tenths; the crossed sheets add to that where they cross]. Seed turns each curtain's sheets.
	 */
	Rain RainUnder(const AirSky::Field& F, double Heavy, uint64_t Seed);

	/** The heaviest rain's footprint on the sea, for the sea's material to darken and roughen (MPC_FleetSea's Squall,
	 * step 7): its middle at the sea (the curtain's foot, lagged), its radius, how heavy. None without rain. */
	struct Squall
	{
		bool bOn = false;
		double X = 0.0, Y = 0.0, R = 0.0, Heavy = 0.0;
	};
	Squall SquallOf(const Rain& R);

	/** The deck's far sheet (the air battle's LayDeckSheet): how solid it is for the day's cover, 0.4 at a broken 0.6 to all
	 * of it closed, and its colour seen from under its base, the base's grey, darker on a closed day. */
	double FarSheetSolid(const AirSky::Field& F);
	Rgb FarSheetColour(const AirSky::Field& F, double EyeZ);
}
