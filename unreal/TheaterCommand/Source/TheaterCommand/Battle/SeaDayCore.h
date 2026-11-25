#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * THE FLEET SEA'S LIGHT (the release 2 ocean plan, step 3: the light core; the standing bar, an ocean and its sky as good
 * as the B-17). What the battle's day is: where the sun or the moon stands and which of them lights the scene, how strong
 * and how warm the lamps are, how far the camera's exposure opens, the sky's colours low and high, the haze and how far
 * it lets a ship be seen, the night's grade, how bright the unlit pieces may be, the moon's disc and the stars, and the
 * water's own colour by the ocean it is.
 *
 * Two modes. SEED (the default, today): the numbers the scene uses now, passed through as the scene holds them: the
 * stage's sun from the seed's hour kept 12 to 70 degrees up (ASeaBattle::OpenOn), and its sky, haze and sea material
 * after their switches (TodayScene: SkyColour, SeaSkyLight, MovingSea's SkyHigh, HazeTop and HazeReach, the kept fog after
 * BattleLook::AirHaze); the key 7.0 and the fills 0.75 at 4,900 K, the sky light 0.72: every share 1, so the lamps get
 * today's numbers bit for bit. With no TodayScene passed, its built-in numbers (SeaSkyLight's 229/230/231, HazeInk,
 * SkyOnScreen's 221/221/223, the haze from 15 km at twice the room's), which are the scene's only on a command line with
 * none of -TheaterSeaSky=, -TheaterSeaGrade=, -TheaterBattleGrade=, -TheaterSeaHigh=, -TheaterSeaHaze= or
 * -TheaterAirHaze=: step 11 passes the scene's own, and takes from Day in seed mode only what it passed or a share.
 * FIELD (-TheaterSeaDay=field, tier 2): the field's own light, from light{sun, moon, vis}, or its hour through SunAt with
 * no clamp, down to astronomical night: the sun keys the scene while she is above -4 degrees, then the moon if she is up
 * and lit, else a faint glow overhead; vis is a haze from the horizon up (beta = -ln(Cmin)/vis, so a ship at the rules'
 * vis keeps Cmin of her contrast), coming in over the twilight from -6 degrees, and at night it is met through light,
 * not fog (NightSight). Step 11 hangs it on the scene (ASeaBattle::OpenTheDay, TakeTheRoom, LightTheBattle, DressTheDay).
 *
 * LOOKS ONLY. Nothing the show decides reads any of this, and it draws on no luck: the same field and switches give the
 * same day. Plain C++20, no engine. Places are SeaShow's board frame (X east, Y north, Z up); bearings are degrees
 * clockwise from north, heights degrees above the horizon. Colours are linear light as the engine takes it, unless named
 * a still's pixel (0 to 1, not linearised, as SkyOnScreen). Numbers: [TODAY] the scene's own today, pinned against its
 * source by the cloud test (Tools/sea_waves/sea_day_test.cpp); [PLAN] the release 2 ocean plan's; [P] published
 * (Koschmieder, Cox-Munk, the Planckian locus); [ENGINE] the engine's own formula, transcribed in the cloud and pinned,
 * to be checked against 5.8's source on the Mac (section 7, check 1); [EST] an estimate, to be refitted on the Mac.
 */
namespace SeaDay
{
	inline constexpr double Pi = 3.14159265358979323846;

	enum class Mode { Seed, Field };
	/** What keys the scene: the sun, the moon, or the night's faint glow straight overhead. */
	enum class BodyKind { Sun, Moon, Glow };
	/** Where the sun was worked out from: the stage's (seed: the seed's hour, kept 12 to 70 up), the field's light, the
	 * field's hour, or the seed's hour (field mode with neither: the seeded day, unclamped). */
	enum class SunSource { Stage, FieldLight, FieldHour, SeedHour };
	/** The haze: today's (from 15 km at twice the room's), the air scene's over the sea (no vis: from 30 km at 1.2
	 * times, with the weather's own air where the field's sky thickens it), the rules' vis by day, or the night's own air. */
	enum class HazeKind { Today, AirSea, Vis, Night };
	enum class WaterKind { Tropic, Temperate, North, Med };

	struct Rgb { double R = 0.0, G = 0.0, B = 0.0; };
	/** A way in the board's frame (X east, Y north, Z up), a unit vector. */
	struct Way { double X = 0.0, Y = 0.0, Z = 1.0; };

	// ---------------------------------------------------------------------------------------------------------------
	// [TODAY] The scene's own numbers (Battle/SeaBattle.cpp, Campaign/CampaignBattleLook.*, Battle/AirBattle.cpp,
	// Battle/AirStage.cpp), each pinned against its source by the cloud test.

	/** LightTheBattle: the key and each fill (intensity), the key's warmth (K), the sky light. */
	inline constexpr double KeyDay = 7.0, FillDay = 0.75, KelvinDay = 4900.0, SkyDay = 0.72;
	/** OpenOn: the hour from the seed, 8 in the morning plus up to 7 hours; "-TheaterSeaHour=" taken from 5 to 19; the
	 * stage's sun kept 12 to 70 degrees up. */
	inline constexpr double SeedHourFrom = 8.0, AskedHourLeast = 5.0, AskedHourMost = 19.0, ClampLow = 12.0, ClampHigh = 70.0;
	/** TakeTheRoom: the room's fog density before BattleLook::AirHaze, its falloff; AirHaze's start (metres) and how many
	 * times as thick. The air scene over the sea (AAirBattle): from 30 km at 1.2 times. */
	inline constexpr float RoomDensity = 0.00045f, RoomFalloff = 0.006f;
	inline constexpr double HazeStart = 15000.0, HazeThicker = 2.0, AirSeaStart = 30000.0, AirSeaThicker = 1.2;
	/** The stage's sea material's own haze (AAirStage::MovingSea): HazeTop 0.6 of the sky, half way at 5 km. */
	inline constexpr double MaterialTopToday = 0.6, MaterialReachToday = 5000.0;
	/** The stills' pixels (0 to 255): the haze at the horizon as the screen shows it (SkyOnScreen), and the sky's light
	 * at the horizon for the sea to mirror (SeaSkyLight's still, under the grade of the fit, red 1.08 and blue 0.87). */
	inline constexpr double SkyOnScreenPixel[3] = { 221.0, 221.0, 223.0 }, SeaSkyPixel[3] = { 229.0, 230.0, 231.0 };
	inline constexpr double FitRed = 1.08, FitBlue = 0.87;
	/** The fog's colour (SeaParts::HazeInk), the air scene's over the sea (AirParts::SeaHazeInk: release 2's, bluer and
	 * greener on a1acb552's frames, 22 November; 0.44, 0.55, 0.74 before), and the sky higher up that near water mirrors,
	 * as shares of the horizon's (MovingSea's SkyHigh). */
	inline constexpr float HazeInkToday[3] = { 0.58f, 0.67f, 0.76f };
	inline constexpr double SeaHazeInkToday[3] = { 0.42, 0.60, 0.88 };
	inline constexpr float SkyHighShare[3] = { 0.17f, 0.20f, 0.23f };
	/** The campaign map's night rule for smoke (CampaignMapActor's SmokeHour at full moonlight). */
	inline constexpr double SmokeNight[3] = { 0.30, 0.38, 0.58 };
	/** How far a lookout sees a boat up on a night the rules give no vis for (metres; Show::Hunt's clear night). */
	inline constexpr double ClearNightSees = 4000.0;
	/** The planned eyes over the sea (metres up; ASeaBattle::AimEye and PlanShots, every height they name): the lookout,
	 * the low rides and near eyes, the water sheets (28 and 60), the high eyes, the both-lines eyes (300, 450, 600) and
	 * the overview (1,300; under a deck the weather's dome draws, SeaSky::OverviewHeight's 150 m under its base, never
	 * under 300: every height between is kept by these two, which the cloud test sweeps). Every one must see a ship at
	 * the rules' vis. */
	inline constexpr double PlannedEyes[] = { 14.0, 16.0, 18.0, 20.0, 24.0, 28.0, 30.0, 55.0, 60.0, 70.0, 260.0, 300.0, 450.0, 600.0, 1300.0 };

	// ---------------------------------------------------------------------------------------------------------------
	// [PLAN] Field mode's numbers.

	/** The sun keys the scene while she is above this; below, the moon if she is up and lit, else the glow overhead at
	 * GlowShare of the day's key. The moon's key: KeyDay x MoonFull x her lit share x the square root of the sine of her
	 * height (the lamp's own slant gives the sine on the water already): nothing from an unlit moon, the full moon's 0.06
	 * of the day's at the zenith. */
	inline constexpr double SunKeyLowest = -4.0, GlowShare = 0.008, MoonFull = 0.06;
	/** The light on the level sea (OnSea): the key's lamp times the sine of its height (nothing from under the horizon),
	 * the sky light and FillsOnSea fills (the room's fills stand for the sky's bounce). The unlit pieces are judged by it
	 * against a clear day's at the same sun, the day's at UnlitDayLowest degrees for any sun under it. */
	inline constexpr double FillsOnSea = 2.0, UnlitDayLowest = 6.0;
	/** The key's warmth: exactly KelvinDay above KelvinEaseHigh, easing to TwilightKelvin by KelvinEaseLow (the
	 * atmosphere's per-pixel transmittance then reddens a low sun), and the moon's and the glow's NightKelvin. */
	inline constexpr double KelvinEaseHigh = 12.0, KelvinEaseLow = 6.0, TwilightKelvin = 6500.0, NightKelvin = 8000.0;
	/** The sun's lamp: from the glow's at KeyRampLow to the day's at KeyRampHigh, log-linear in her height (she keys only
	 * above -4, where under the horizon she puts nothing on the water). Below -4 the night's own body, faded in from
	 * nothing at -4 to its own strength at KeyRampLow, so the switch falls where the key lights nothing and the dusk is
	 * the sky light's, which lingers from SkyRampLow to SkyRampHigh: the glow's share of the day's at SkyRampLow and
	 * below, the day's at SkyRampHigh, by equal ratios, whatever the moon. A moon adds to the sky only her own light (her
	 * key's share of the day's, faded in under -4 as her key is): she moves nothing from -4 up. The sky light and the
	 * fills add the larger of that and the glow's key she replaces (the night's floor), so the light on the level sea
	 * is never less than the moonless night's at the same sun; the floor is the lamps' only, never the sky's colours. */
	inline constexpr double KeyRampLow = -10.0, KeyRampHigh = 4.0, SkyRampLow = -16.0, SkyRampHigh = 6.0;
	/** [EST] The sky's low-sun tints (gold toward her, rose-grey away) come in from SkyGoldFrom degrees down to 1 (the
	 * photographers' golden hour: within 2 levels of today's at 6 degrees, the colours' floor), and go out by -11. */
	inline constexpr double SkyGoldFrom = 7.0;
	/** Overcast: the key at OvercastKey of itself under a whole cover, the sky lifted by OvercastSky; rain dims both. */
	inline constexpr double OvercastKey = 0.35, OvercastSky = 0.4, RainKey = 0.3, RainSky = 0.2;
	/** The exposure, in stops over BattleLook's own: 0 by day, TwilightBias as the sun goes under -4, MoonlitBias on a
	 * moonlit night and DarkBias on a dark one; NightSight may open it to MostBias to keep a silhouette on the screen.
	 * Under -4 a moon's is never less than the moonless look's at the same sun less the stops her light adds on the level
	 * sea, so she never shows the scene darker. */
	inline constexpr double TwilightBias = 1.2, MoonlitBias = 1.8, DarkBias = 2.4, MostBias = 3.0;
	/** The night grade (through SetSeaWind's grade argument): saturation and the blue gain at full night. */
	inline constexpr double NightSaturation = 0.45, NightBlueGain = 1.06;
	/** Koschmieder's least contrast a ship keeps at the rules' vis (-TheaterSeaVisContrast), the night's clear air's
	 * meteorological range (metres: the range at 2% contrast), and the sun-side haze: the sun's colour x InscatterShare,
	 * exponent InscatterExponent, from InscatterStart metres. */
	inline constexpr double VisContrast = 0.05, ClearAirRange = 25000.0, InscatterShare = 0.3, InscatterExponent = 8.0, InscatterStart = 2000.0;
	/** Vis is a haze only while the sun is above VisFogLowest (degrees): at and below it, never. It is wholly the rules'
	 * from VisFogFull, the horizon, up: the haze comes in over the twilight between, the night's air to the day's. In that
	 * band alone, and only where the field gives vis, a quarter degree of sun may move the haze by more than a tenth (the
	 * plan's ruling: the rules' vis by day comes first; LookOf runs once a battle, so no battle sees the sun cross it).
	 * With no vis nothing comes first, and the haze is held to the tenth: it goes from the night's air at AirSeaFrom
	 * (astronomical night, where the sun's last glow leaves the sky) to the day's by AirSeaFull, its density by equal
	 * ratios and its start out from the eye to 30 km as the cube of the way (so it goes out as the air thins), over the
	 * arc the tenth needs: about 7.5 degrees for each e-fold at the least contrast 0.05, 9.8 at 0.02, and the night's clear
	 * 25 km air is about six times the air scene's haze. The sea material's own haze is eased out over the same band as
	 * the fog, from MaterialReachFar (metres: ten times the sea's 200 km edge, under a hundredth of its haze anywhere on
	 * the sea). */
	inline constexpr double VisFogLowest = -6.0, VisFogFull = 0.0, AirSeaFrom = -18.0, AirSeaFull = 2.0, MaterialReachFar = 2.0e6;
	/** The material's own haze made nothing (HazeTop 0, HazeReach this many of the world's units): the fog alone greys
	 * the sea and the ships, once. */
	inline constexpr double MaterialReachNever = 1.0e12;
	/** [EST] NightSight's picture: a hull's lit side by day as a share of the horizon's haze behind her, the sea where the
	 * ships are as a share of the sky it mirrors (the 22 November fit: 0.5 to 0.7), and the screen levels a silhouette
	 * must keep. */
	inline constexpr double HullOverSkyDay = 0.35, SeaOverSky = 0.6, LeastLevels = 2.0;

	// ---------------------------------------------------------------------------------------------------------------
	// What the day is told

	/** The field's sky (the rules' sky{cover, kind, base, top, rain, word}; step 4 reads it). */
	struct Weather
	{
		bool bGiven = false;
		double Cover = 0.0, Rain = 0.0; // 0 to 1
		double Base = 0.0, Top = 0.0;   // metres
		std::string Kind, Word;         // kind: cumulus, deck or squalls
	};

	/** The battle's field, as the scene holds it (SeaFeed's Board: lat, lon, month, the seed, the wind, light{sun, moon,
	 * vis}), and what step 4's SeaLookField reads beyond it (light.sun.from, when.hour, sky). */
	struct Field
	{
		double Lat = 45.0, Lon = 0.0;
		int Month = 4; // from 0 (January)
		uint64_t Seed = 239;
		double Wind = 0.0; // metres a second (the show's)
		bool bLight = false;
		double SunUp = 90.0;
		bool bSunFrom = false;
		double SunFrom = 0.0;
		double MoonUp = -90.0, MoonFrom = 0.0, MoonLit = 0.0;
		double Seeing = 0.0; // vis, metres (0: not said)
		bool bHour = false;
		double Hour = 12.0; // local sun time
		Weather Sky;
	};

	/** The stage's sun as ASeaBattle worked it out (the pinned hour and clamp lines, in the engine's own maths): seed mode passes it
	 * through untouched. */
	struct StageSun
	{
		double Up = 30.0, From = 120.0;
	};

	/** The scene's own sky, haze and sea material today, after its switches, as step 11 reads them off the scene: the
	 * horizon on the screen (SeaParts::SkyColour(), "-TheaterSeaSky="), the sky's light at the horizon for the sea to
	 * mirror (SeaParts::SeaSkyLight(), under "-TheaterSeaGrade=" or "-TheaterBattleGrade="), MovingSea's SkyHigh
	 * ("-TheaterSeaHigh="), HazeTop ("-TheaterSeaHaze=") and HazeReach (metres), and the kept fog after
	 * BattleLook::AirHaze ("-TheaterAirHaze=": its start in metres, density, falloff, most opacity and colour). Seed mode
	 * passes them through, as it passes the stage's sun; field mode tints its sky from them. bGiven false: BuiltInScene's,
	 * the scene's own on a bare command line only. */
	struct TodayScene
	{
		bool bGiven = false;
		Rgb SkyOnScreen, SeaSkyLight, SkyHigh;
		double MaterialTop = MaterialTopToday, MaterialReach = MaterialReachToday;
		double FogStart = HazeStart, FogDensity = 0.0, FogFalloff = 0.0, FogMostOpacity = 1.0;
		Rgb FogColour;
	};

	/** The switches and the scene's own numbers the day needs. */
	struct Asked
	{
		Mode Day = Mode::Seed;           // -TheaterSeaDay=seed|field
		double VisContrast = SeaDay::VisContrast; // -TheaterSeaVisContrast=
		float AskedHour = -1.0f;         // -TheaterSeaHour= (as OpenOn reads it: taken from 5 to 19)
		int Water = -1;                  // -TheaterSeaWater= (a WaterKind), -1 by the place
		double UnitsPerMetre = 100.0;
		double FogZ = 0.0;               // metres: the kept fog's own height (its density is given there)
		std::vector<double> Eyes;        // the planned eyes' heights (metres); empty: PlannedEyes
	};

	// ---------------------------------------------------------------------------------------------------------------
	// What the day is

	struct Body
	{
		BodyKind Kind = BodyKind::Sun;
		double Up = 0.0, From = 0.0; // degrees
		Way Toward;                  // from the scene toward her (the lamp shines the other way)
	};

	/** The engine's exponential height fog, as its component takes it: FogDensity and FogHeightFalloff (the editor's
	 * numbers, a thousand times the shader's, per world unit), the component's height, the start (world units) and
	 * FogMaxOpacity. */
	struct Fog
	{
		double Density = 0.0, Falloff = 0.0, HeightZ = 0.0, Start = 0.0, MostOpacity = 1.0;
	};

	struct Haze
	{
		HazeKind Kind = HazeKind::Today;
		double Start = HazeStart; // metres from the eye
		double Density = 0.0, Falloff = 0.0, MostOpacity = 1.0; // the engine's FogDensity, FogHeightFalloff, FogMaxOpacity
		Rgb Colour;               // the fog's inscattering colour
		/** The sun-side haze (directional inscattering): set only in field mode with vis; its colour, exponent and start
		 * (metres). Otherwise the room keeps its own, as the air scene leaves it. */
		bool bSun = false;
		Rgb SunColour;
		double SunExponent = InscatterExponent, SunStart = InscatterStart;
		/** The sea material's own haze: today's (HazeTop 0.6, half way at 5 km), or made nothing (HazeTop 0, HazeReach
		 * MaterialReachNever units) wherever the engine's fog begins at the eye. */
		bool bMaterialToday = true;
		double MaterialTop = MaterialTopToday, MaterialReach = MaterialReachToday; // metres
		/** The fog's extinction at sea level (natural, a metre), the rules' vis (metres, 0 none), how much of the vis fog is
		 * in (0 at -6 degrees of sun and below, 1 from the horizon up), and the night air's extinction at sea level. */
		double Beta = 0.0, Vis = 0.0, VisShare = 0.0, NightBeta = 0.0;
		/** The night's air as the engine takes it (its density at the component and its falloff), and its own extinction
		 * from the sky and rain: thinned (bNightThinned) only where it would hide a hull at the rules' vis (or Show::Hunt's
		 * 4 km) against the sea or the sky from a planned eye, so the rules' vis bounds the night's air but never makes it
		 * (the plan's amendment at step 3's review: the rules' sightings before the weather's own night air; Brian's to
		 * overrule). Never thicker than its own. */
		double NightDensity = 0.0, NightFalloff = 0.0, NightOwnBeta = 0.0;
		bool bNightThinned = false;
		/** By day with no vis, the weather's own air: what the field's rain, low deck and spray add to clear air's
		 * extinction (natural, a metre, at sea level; WeatherOwnBeta, 0 under a clear or fair sky, where the air scene's
		 * haze is all). It is added to the air scene's haze, whose 30 km start draws in by the square of the air scene's
		 * share of the air (a few hundred metres out in a squall), and the weather's part is thinned (bWeatherThinned,
		 * WeatherBeta) only where a hull lit by day at Show::Hunt's 4 km would lose her silhouette from a planned eye, as
		 * the night's air is. So a squall is the same air by day as by night, but for the clear air under it. */
		double WeatherBeta = 0.0, WeatherOwnBeta = 0.0;
		bool bWeatherThinned = false;
		/** The vis fog's height falloff, the least that keeps every planned eye's ray to a ship at vis at Cmin (0: none
		 * needed), and the worst of those rays. */
		double BandFalloff = 0.0, WorstSeen = 1.0;
	};

	/** NightSight: the exposure a night wants, and a hull's silhouette at the rules' vis against the sky and the sea. */
	struct Sight
	{
		double Bias = 0.0;
		double Vis = 0.0, Seen = 1.0;           // the range judged (metres) and the night air's least transmittance to it, of every planned eye's
		double HullShare = 0.0;                 // her lit side by the night's lamps, a share of the horizon's light
		double ContrastSky = 1.0, ContrastSea = 1.0;
		double LevelsSky = 255.0, LevelsSea = 255.0; // on the screen, at the bias
		bool bHolds = true;
	};

	struct MoonDisc
	{
		bool bUp = false;
		double Up = -90.0, From = 0.0, Lit = 0.0;
		double Phase = 0.0;  // radians: 0 full, pi new
		double Radius = 0.26; // degrees
		Way Toward, BrightToward; // her way, and the way across her disc to her lit limb (toward the sun)
		Rgb Bright;          // her lit face's light
	};

	struct StarField
	{
		double Share = 0.0, Visible = 0.0; // faded in by the dark; and under the field's cover
		int Count = 2500;
		double LeastPixels = 1.5;
		uint64_t Seed = 0;
		Rgb Bright;
	};

	struct Water
	{
		WaterKind Kind = WaterKind::Temperate;
		const char* Name = "temperate";
		bool bAsked = false; // -TheaterSeaWater= gave it
		Rgb Body;            // the sea's paint, a tint of today's (its brightness 1)
		Rgb Crest;           // the light through a crest
	};

	struct Look
	{
		Mode Day = Mode::Seed;
		SunSource Source = SunSource::Stage;
		double Hour = 12.0;
		double SunUp = 30.0, SunFrom = 120.0;
		double MoonUp = -90.0, MoonFrom = 0.0, MoonLit = 0.0;
		Body Key;
		/** The lamps: the key's and each fill's intensity, the sky light's; and each as a share of today's (step 11
		 * multiplies today's pinned Want and SkyWant by these). */
		double KeyLight = KeyDay, FillLight = FillDay, SkyLight = SkyDay;
		double KeyShare = 1.0, FillShare = 1.0, SkyShare = 1.0;
		double Kelvin = KelvinDay;
		bool bPerPixelTransmittance = false;
		double Bias = 0.0;                     // stops over BattleLook's own
		double Saturation = 1.0, BlueGain = 1.0; // the night grade
		Rgb HourLight{ 1.0, 1.0, 1.0 };         // what the hour does to smoke (the kit's Tick)
		/** The light on the level sea (the key's lamp times the sine of its height, the sky light, FillsOnSea fills), and
		 * every unlit piece's factor: that against a clear day's at the same sun (at UnlitDayLowest for a lower one), at
		 * most 1, so exactly 1 by a clear day. The self-lit pieces (the burst flashes, the star shells' flares: light that
		 * is its own source) keep SelfLitShare, 1: they never scale with the daylight, and the night's exposure makes them
		 * shine as they would. */
		double OnSea = KeyDay + SkyDay + FillsOnSea * FillDay;
		double UnlitShare = 1.0, SelfLitShare = 1.0;
		double StarShare = 0.0;
		double Daylight = 1.0;                 // the day under the water (FSeaUnder's ReadSun, with this key)
		/** The sky: the haze at the horizon as the screen shows it (a still's pixel), the sky's light at the horizon for
		 * the sea to mirror, the sky higher up (13 degrees, which near water mirrors; the plan's zenith), and the horizon
		 * toward and away from the sun. */
		Rgb SkyOnScreen, SeaSkyLight, SkyHigh, SkyToward, SkyAway;
		Haze Air;
		Sight Night;
		MoonDisc Moon;
		StarField Stars;
		Water Sea;
	};

	// ---------------------------------------------------------------------------------------------------------------

	/** A smooth step from 0 at From to 1 at To, either way round (From above To: falling as X rises). */
	double Ease(double From, double To, double X);

	/** [TODAY] The hour of the battle's day (local sun time) from its seed, or "-TheaterSeaHour=" (below 0: not asked;
	 * taken only from 5 to 19, as OpenOn takes it). */
	double SeedHour(uint64_t Seed, float AskedHour = -1.0f);
	/** [TODAY] Where the sun stands (SeaParts::SunAt): her bearing and height (degrees) at a latitude, in a month (0
	 * January) and at an hour. */
	void SunAt(double Latitude, int Month0, double Hour, double& OutFrom, double& OutUp);
	/** [TODAY] The stage's sun as OpenOn works it out: the seed's hour, SunAt, the height kept 12 to 70. (Seed mode
	 * itself takes the stage's sun from the scene; this is the same text, for the cloud and for field mode's seeded day.) */
	void SeedSun(uint64_t Seed, double Lat, int Month0, float AskedHour, double& OutFrom, double& OutUp);
	/** The sun's bearing when she stands Up high in the evening of that day (a field that gives her height alone). */
	double EveningFrom(double Lat, int Month0, double Up);
	/** [TODAY] The day under the water by the sun's height (SeaUnder::DaylightOf). */
	double DaylightOf(double SunUp);

	/** [TODAY] The light (as the engine takes it) a battle camera shows as a still's pixel, under a grade of these red
	 * and blue gains (SeaParts::LightOfStill: the tone curve and the grade undone), each channel a float as the scene's. */
	Rgb LightOfStill(const Rgb& Still, double GradeRed, double GradeBlue);
	/** Its inverse: the still's pixel (0 to 1) of a light, through the grade and the fitted tone curve. */
	Rgb StillOf(const Rgb& Light, double GradeRed, double GradeBlue);
	/** [P] A lamp's colour at a temperature (the engine's FLinearColor::MakeFromColorTemperature: the Planckian locus,
	 * BT.709 primaries, luminance 1). */
	Rgb KelvinColour(double Kelvin);
	/** The way toward a body Up high, From (degrees). */
	Way TowardOf(double Up, double From);

	/** [ENGINE] What the engine's height fog lets through from an eye EyeZ high to a point TargetZ high Across away (world
	 * units): its line integral (HeightFogCommon.ush's CalculateLineIntegralShared, with the start's exclusion and the
	 * least opacity, the densities and falloffs a thousandth of the component's as FogRendering.cpp scales them). */
	double FogSeen(const Fog& F, double EyeZ, double TargetZ, double Across);
	/** The engine's FogDensity that gives a level ray at its own height an extinction of Beta (natural, a metre), and back. */
	double DensityOfBeta(double Beta, double UnitsPerMetre);
	double BetaOfDensity(double Density, double UnitsPerMetre);

	struct FogFit
	{
		double Density = 0.0, Falloff = 0.0; // the engine's, with the component at FogZ
		double WorstSeen = 1.0;              // the worst planned ray at the range
		bool bBand = false;                  // a falloff was needed
	};
	/** The fog for an extinction Beta at sea level (natural, a metre): uniform, unless a planned eye's ray to a ship at
	 * Range (metres) would keep less than Cmin; then the least height falloff that keeps every one at Cmin, the density
	 * still Beta's at the sea, with the component FogZ metres up. Fitted with the engine's own line integral. */
	FogFit FogFor(double Beta, const std::vector<double>& Eyes, double Range, double FogZ, double UnitsPerMetre, double Cmin);

	struct HazeAsk
	{
		double Vis = 0.0, SunUp = 30.0, Cover = 0.0, Rain = 0.0, Wind = 0.0;
		std::string Kind;
		double Cmin = SeaDay::VisContrast, UnitsPerMetre = 100.0, FogZ = 0.0;
		std::vector<double> Eyes;
		/** A hull's silhouette at night as a share of the horizon's light, and the sea's: the night air is never let hide
		 * what the rules say is seen at their vis (or what the show's lookout sees on a clear night, ClearNightSees), from
		 * any planned eye. */
		double HullOverSky = HullOverSkyDay, SeaOverSky = SeaDay::SeaOverSky;
		/** The sea material's own haze by day with no vis: the scene's (TodayScene's HazeTop, and HazeReach in metres). */
		double MaterialTop = MaterialTopToday, MaterialReach = MaterialReachToday;
	};
	/** Field mode's haze. Vis is fog only above -6 degrees: from the horizon up and with vis given, beta = -ln(Cmin)/vis,
	 * fitted by FogFor, from the eye; at and below -6 degrees the night's own air (the field's sky and rain, else a clear
	 * 25 km, never thicker than lets a hull at vis be seen from a planned eye), never the vis; the two meet over the
	 * twilight from -6 to 0. With no vis, the air scene's sea haze with the weather's own air (WeatherBeta) from +2 degrees
	 * up, and the night's own air at -18 and below, meeting over the arc between by no more than a tenth a quarter degree.
	 * Colours are LookOf's. */
	Haze HazeFor(const HazeAsk& A);

	/** The night's lamps NightSight judges by. */
	struct NightLamps
	{
		double Key = 0.0, Sky = 0.0; // the night's key lamp (its intensity) and sky light
		double OnSea = 0.0;          // the light on the level sea (Look::OnSea's measure)
		double Horizon = 0.0;        // the horizon's light, a share of the day's: the look's own SeaSkyLight at night (the
		                             // cover lifts the sky light and the fills, not the horizon's colour)
	};
	/** At night the rules' vis is met through light, not fog: the exposure by the night's light on the level sea (DarkBias
	 * on a moonless clear night's, MoonlitBias from a clear half moon 30 degrees up's, by equal ratios between), never under
	 * LeastBias, opened (to MostBias at most) until a hull's silhouette at Vis keeps LeastLevels on the screen; her
	 * contrast against the sky and the sea at Vis through the night's Air by the engine's line integral from every planned
	 * eye. HorizonLight is the day's horizon (its luminance). */
	Sight NightSight(double Vis, const NightLamps& N, const Fog& Air, const std::vector<double>& Eyes, double Cmin, double UnitsPerMetre, double HorizonLight, double LeastBias = 0.0);
	/** The moon's key lamp, Up high and Lit (her lit share): KeyDay x MoonFull x Lit x the root of her height's sine; 0
	 * when she is down or unlit. */
	double MoonKeyOf(double Up, double Lit);

	/** The water by the ocean it is: the Mediterranean and the Black Sea (med; west of 0 longitude only south of 40N, so
	 * not the Bay of Biscay), the tropics and the subtropical open ocean under 40 degrees (tropic's clear blue: the Red
	 * Sea, the Gulf, the Gulf of Mexico, Okinawa, the Sargasso), the North Sea and the Yellow Sea (temperate, green-grey),
	 * the north (the North Atlantic and the Arctic, slate; by 45 degrees in the winter half year, 50 in the summer's),
	 * else temperate. */
	Water WaterOf(double Lat, double Lon, int Month0);
	Water WaterNamed(WaterKind Kind);
	/** "-TheaterSeaWater=" read: tropic, temperate, north or med; false for anything else (by the place). */
	bool WaterKindOf(const std::string& Name, WaterKind& Out);

	/** The sea material's own haze share at a pixel's depth (both world units), in float as the material works it:
	 * (d/reach)^2 / (1 + (d/reach)^2). */
	float MaterialHazed(float Depth, float Reach);

	/** The scene's sky, haze and sea material as they are on a bare command line (bGiven true). */
	TodayScene BuiltInScene(double UnitsPerMetre);
	/** The day. Seed: today's numbers, with the stage's sun and the scene's own sky, haze and material as given (Scene
	 * unset: BuiltInScene's). Field: the field's, its sky a tint of the scene's. */
	Look LookOf(const Field& F, const StageSun& Stage, const Asked& A, const TodayScene& Scene = TodayScene{});
	/** The day said in a line, for the log. */
	std::string Say(const Look& L);
}
