#pragma once

#include <cstdint>
#include <vector>

/**
 * THE NIGHT SKY OVER THE SEA (look bible S7, "Night at sea"; the release 2 ocean plan, step 11(f): the moon and stars on
 * one unfogged dome). Engine-free, so the cloud test lays exactly what the game will.
 *
 * The stars are the real ones, from the Yale Bright Star Catalogue (SeaNightStars.inl, Tools/sea_waves/
 * make_star_table.py): each brought from the catalogue's epoch (J2000) back to the battle's year by precession, then
 * turned to the battle's place and hour by its local sidereal time, so a January convoy south of Iceland has Orion low
 * in the south at midnight and the Plough standing over the pole, and the Solomons have the Southern Cross. The rules
 * give the moon (light.moon: how high, from where, how much of her is lit) and the sun's depth under the horizon; the
 * battle gives its latitude, longitude, year and month. Where the hour of night is not given it is found from the sun's
 * depth (the evening's).
 *
 * What is laid, for the dome (FSeaNightDome) to draw at 400 km round the eye on M_NightSky (unlit, masked, "Is Sky", so
 * no fog or air touches it):
 *  - each star above the sea's haze bright enough to be seen through it, tonight: as a small square facing the eye, at
 *    least 1.5 of the screen's pixels across (the plan's floor, so the anti-aliasing never loses it), its light by its
 *    magnitude, dimmed through the air low down (Kasten and Young's air mass, 0.35 magnitudes an air mass at sea), its
 *    colour by its temperature, with fewer as the moon or the twilight brightens the sky;
 *  - on a dark night the Milky Way, as the faint specks of its unresolved stars along the galaxy's plane (brightest
 *    toward Sagittarius, the Great Rift dark through Cygnus and Aquila), gone under a bright moon or in twilight;
 *  - the moon as an opaque disc of her true half-degree, lit by her phase on the side toward the sun, her seas (maria)
 *    darker where they really lie on her face, her dark side by earthshine alone (so it still hides the stars behind
 *    it), reddening as she sinks into the haze.
 * Look only: the battle decides nothing by any of it.
 */
namespace SeaNightSky
{
	/** A direction in the board's frame: X east, Y north, Z up (unit length). */
	struct Dir
	{
		double X = 0.0, Y = 0.0, Z = 1.0;
	};

	/** A star of the catalogue as stored: right ascension (hours) and declination (degrees) at J2000, its visual
	 * magnitude, its colour temperature (kelvin; 0 not given). */
	struct Star
	{
		float RaHours, DecDeg, V, Kelvin;
	};
	int StarCount();
	const Star& StarAt(int Index);

	/** The battle's place and time, and the rules' night. */
	struct When
	{
		double Lat = 45.0, Lon = 0.0;            // degrees, north and east positive
		int Year = 1942, Month = 6, Day = 15;     // the date (a battle that gives no day: the 15th)
		double SolarHour = -1.0;                  // local mean solar time, 0 to 24; below 0: not given (found from SunUp)
		double SunUp = -18.0;                     // the sun's height, degrees (below 0 under the horizon)
		bool bMoon = false;                       // the rules gave the moon
		double MoonUp = -90.0, MoonFrom = 0.0;    // her height and bearing (degrees, bearing clockwise from north)
		double MoonLit = 0.0;                     // the share of her face lit, 0 (new) to 1 (full)
	};

	/** The screen the dome is laid for (at each cut): the lens's full width across and the view's width in pixels. */
	struct Lens
	{
		double FovDeg = 50.0, ViewPx = 1600.0;
	};

	/** What else dims the stars: the share of the sky under cloud (0 to 1: a star is left out where its own hash says
	 * the cloud hides it, so as the cover grows they go one by one, the same ones each time), how bright the twilight
	 * still is (StarShare from the light core, 0 to 1; below 0: worked out here from the sun's depth), and a
	 * multiplier for the stars' light ("-TheaterSeaStars=", 1 as made). */
	struct Seeing
	{
		double Cover = 0.0, StarShare = -1.0, Strength = 1.0;
	};

	/** A star or the moon laid on the dome. */
	struct Laid
	{
		Dir Way;               // from the eye
		double HalfRad = 0.0;  // half its width across the sky, radians
		float R = 1.0f, G = 1.0f, B = 1.0f; // its light (linear; the dome's Strength scales it on the screen)
		int Index = -1;        // the catalogue's row (stars only)
	};

	struct Sky
	{
		double SolarHour = 0.0;        // the hour used (given, or found)
		double SiderealDeg = 0.0;      // local sidereal time, degrees
		double LimitMag = 5.5;         // the faintest star laid tonight
		Dir SunWay, MoonWay, NorthPole;
		bool bMoonUp = false;
		double MoonHalfRad = 0.0;
		double MoonLit = 0.0;
		std::vector<Laid> Stars;       // brightest first
		int HiddenByMoon = 0, HiddenByCloud = 0, BelowHaze = 0, TooFaint = 0;
		/** The Milky Way: how much of it shows tonight (0 to 1: only a dark sky shows it; a moon or the twilight washes
		 * it out), and its unresolved stars as specks, each a star's square of its own, after the stars. */
		double MilkyShare = 0.0;
		std::vector<Laid> Glow;
	};

	/** Local mean solar hour -> the sidereal time at the place (degrees, 0 to 360), and the Julian date of that moment. */
	double JulianDate(int Year, int Month, double DayAndFraction);
	double SiderealDegrees(const When& W, double SolarHour);

	/** The sun's height and bearing (degrees) at the place at a local mean solar hour (the low-precision almanac's sun:
	 * good to about a hundredth of a degree in this century and the last). */
	void SunAt(const When& W, double SolarHour, double& OutUp, double& OutFrom);

	/** The local solar hour at which the sun stands at W.SunUp, on the evening side (from noon toward midnight); where
	 * she never sinks that far (a northern summer), local midnight, her lowest. */
	double HourOfNight(const When& W);

	/** A star's J2000 place carried to the battle's year (IAU 1976 precession), then its height and bearing at the place
	 * and sidereal time: false when it is under the horizon. */
	void Precess(double RaHours, double DecDeg, double Year, double& OutRaHours, double& OutDecDeg);
	Dir AltAz(double UpDeg, double FromDeg);
	void ToAltAz(double RaHours, double DecDeg, double LatDeg, double SiderealDeg, double& OutUp, double& OutFrom);

	/** The Milky Way's frame: J2000 right ascension (hours) and declination (degrees) to galactic longitude and
	 * latitude (degrees; l 0 toward the galaxy's centre in Sagittarius, b 90 its north pole), and back (IAU 1958, as
	 * Hipparcos fixed it in J2000). */
	void ToGalactic(double RaHours, double DecDeg, double& OutL, double& OutB);
	void FromGalactic(double L, double B, double& OutRaHours, double& OutDecDeg);
	/** The Milky Way's light at a galactic place, 0 to 1 of its brightest: a band a few degrees either side of the plane,
	 * thicker and brightest toward the centre, with the star clouds of Scutum, Cygnus and Carina, and the Great Rift's
	 * dark lane from Cygnus to Sagittarius. A painter's map from the photographs' broad shape, not a survey. */
	double MilkyAt(double L, double B);

	/** The air between: Kasten and Young's air mass at a height in degrees (1 overhead, about 38 at the horizon). */
	double AirMass(double UpDeg);

	/** The faintest magnitude seen tonight: by the sun's depth (twilight), the moon's light on the sky, and StarShare
	 * where the light core gives it. */
	double LimitOf(const When& W, const Seeing& See);

	/** A colour temperature's light, as a white with its tint (linear, the brightest channel 1). */
	void ColourOf(double Kelvin, float& R, float& G, float& B);

	/** Everything on the dome for this place, hour and lens. Deterministic: the same inputs lay the same sky. */
	Sky Lay(const When& W, const Lens& L, const Seeing& See);

	/** The moon's face as a mesh to lay at her place on the dome: a disc of Segments round its rim and Rings in, each
	 * point's light by her phase (the side toward the sun lit, a soft terminator), her maria and earthshine. Points are
	 * offsets from her centre in the sky's tangent plane, in units of her radius: Right and Up are the two directions
	 * across her (Up toward the celestial pole), so the engine lays Centre + Radius * (x Right + y Up). */
	struct Face
	{
		Dir Right, Up;
		std::vector<double> X, Y;            // on the unit disc
		std::vector<float> Light;            // 0 to 1 of her full lit face's light
		std::vector<int> Triangles;          // three indices each, wound to face the eye
	};
	Face MoonFace(const Sky& S, int Segments = 64, int Rings = 8);
	/** Her light at a point of her face (x right, y up, on the unit disc): the lit share, the maria and earthshine. */
	double MoonLightAt(const Sky& S, const Dir& Right, const Dir& Up, double X, double Y);

	/** The dome's mesh, in units round the eye (the engine adds the eye): each star a square facing the eye at Radius,
	 * then each speck of the Milky Way, then the moon's face, vertex colours linear (masked: alpha marks what is drawn). Every star's alpha is 1; the moon's
	 * is 1 where her light (of her full face's, before the air's tint) is MoonDarkCut or more, else 0: 0 lays her whole
	 * disc, her dark side by earthshine (a dark night); above it her dark side is left out, so a twilight sky shows
	 * through it as it does through the real moon's. The stars behind her are left out either way. */
	struct Mesh
	{
		std::vector<double> V;       // x, y, z each, in the board's frame (east, north, up), units
		std::vector<float> C;        // r, g, b, a each
		std::vector<int> Triangles;
		int StarQuads = 0, GlowQuads = 0, MoonTriangles = 0; // (the stars' squares, then the Milky Way's, then the moon)
	};
	Mesh BuildMesh(const Sky& S, double Radius, int MoonSegments = 64, int MoonRings = 8, double MoonDarkCut = 0.0);
}
