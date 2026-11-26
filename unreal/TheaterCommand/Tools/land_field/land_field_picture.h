#pragma once

#include "Battle/LandField.h"

#include <string>

/**
 * A small software painter for the land battle's ground (the cloud has no engine): the ground's meshes and instances
 * rasterised with a depth buffer, lit by a low July morning sun with soft shadows (two shadow maps), a sky and the
 * distance haze, then written as PNG pictures 1600 pixels wide. It paints what the game is given, nothing more.
 */
namespace LandFieldPicture
{
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
	};

	/** Paints <Key>-field-eye.png, <Key>-field-oblique.png and <Key>-field-map.png into Dir. False if one could not be written. */
	bool Paint(const LandField::Ground& G, const LandField::Field& F, const Sky& S, const std::string& Key, const std::string& Dir);
}
