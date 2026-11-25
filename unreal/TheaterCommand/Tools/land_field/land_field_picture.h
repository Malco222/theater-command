#pragma once

#include "Battle/LandField.h"

#include <functional>
#include <string>
#include <vector>

/**
 * A small software painter for the land battle's ground (the cloud has no engine): the ground's meshes and instances
 * rasterised with a depth buffer, lit by a low July morning sun with soft shadows (two shadow maps), a sky and the
 * distance haze, then written as PNG pictures 1600 pixels wide. It paints what the game is given, nothing more.
 */
namespace LandFieldPicture
{
	/** A picture's lens, for what is drawn over it: the eye, its axes (forward, right, up), the half-width's tangent
	 * and the samples across and down (a sample at SX, SY of a point X right, Y up, Z ahead of the eye is
	 * 0.5 W (1 + X / (Z Tan)), 0.5 H - 0.5 W Y / (Z Tan)); the depths are Z, infinite where the sky is.
	 * Over adds, sample by sample: Cover, the share of what lies behind hidden (0 to 1), and Light, the colour laid
	 * over it as a share of the haze's own colour along that ray (the drops are the haze's colour, a little brighter). */
	struct Lens
	{
		double Eye[3] = { 0, 0, 0 }, Fwd[3] = { 1, 0, 0 }, Right[3] = { 0, -1, 0 }, Up[3] = { 0, 0, 1 };
		double Tan = 0.6;
		int W = 0, H = 0;
		/** The picture's pixels across as written (the samples are averaged down two to one). */
		int PixelsAcross = 0;
	};

	/** The recording's sky: the place, the day of the year, the hour on the battle's clock, the cumulus's share and base. */
	struct Sky
	{
		double Lat = 49.12, Lon = -1.09;
		int Day = 196;
		double Hour = 9.5;
		/** The clock's hours ahead of Greenwich: Normandy in July 1944 kept double summer time (both armies' clocks). */
		double Zone = 2.0;
		double Cover = 0.45, Base = 900.0;
		/** The look's own air (LandScape::AirOf) against the bocage's: the haze's colour by channel and its thickness. */
		double HazeTint[3] = { 1.0, 1.0, 1.0 };
		double HazeThick = 1.0;
		/**
		 * Rain or snow over the battle (release 2, the look bible's G10; the cloud test's LAND_FIELD_FALL): Overcast, 0 to
		 * 1, dulls the sun to a grey day's light under a deck (its shadows soft, the cumulus gone into it); HazeTimes,
		 * HazeToward and FallHaze the haze as LandWeather lays it in the game (thicker, drawn toward the rain's grey);
		 * LightTimes the day's lamps. Over: what falls, drawn over the eye's two pictures (see Lens).
		 */
		double Overcast = 0.0;
		double HazeTimes = 1.0, HazeToward = 0.0, FallHaze[3] = { 0.44, 0.46, 0.49 };
		double LightTimes = 1.0;
		/** The deck's own brightness under the weather (LandWeather's SkyTimes, as the battle's SkyShine). */
		double SkyTimes = 1.0;
		std::function<void(const Lens& V, const std::vector<float>& Depth, std::vector<float>& Light, std::vector<float>& Cover)> Over;
	};

	/** Paints <Key>-field-eye.png, <Key>-field-oblique.png and <Key>-field-map.png into Dir. False if one could not be written. */
	bool Paint(const LandField::Ground& G, const LandField::Field& F, const Sky& S, const std::string& Key, const std::string& Dir);

	/** One view of its own (the burnt cottages' sheets): the eye, the place it looks at, the field of view across (degrees), the
	 * picture's size (pixels, painted at twice and averaged down), hazed or not; the clutter laid within Clutter metres of
	 * the eye. False if it could not be written. */
	bool PaintView(const LandField::Ground& G, const LandField::Field& F, const Sky& S, const double Eye[3], const double At[3], double FovDeg, int W, int H, bool bHaze, double Clutter, const std::string& Path);
}
