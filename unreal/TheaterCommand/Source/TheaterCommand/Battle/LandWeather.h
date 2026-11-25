#pragma once

// Rain and snow in the ground battle (release 2, the look bible's G10: rain and snow seen falling, from the battle's own
// weather). Plain C++: what falls over this battle, and how the day changes for it, read from what the campaign has
// already drawn and sent (the battle ticket's weather word and its sky: LandFeed's WeatherWord and Weather). No new
// draw: the same battle always has the same weather. Looks only: nothing in any fight reads it. LandBattleWeather.cpp
// draws it (Effects/WeatherFall) and lays it on the day (the haze, the light, the ground's sheen, the rain's sound);
// LandField lays the wet ground (Field::Rain). The numbers are Content/Effects/land-weather.json's ("pick" and "day").

#include "Effects/WeatherFallCore.h"

#include <functional>
#include <string>

namespace LandWeather
{
	/** What the battle knows of its weather. */
	struct Asked
	{
		/** The campaign's word over the place ("clear", "rain", "snow", "mud"), empty where it keeps none. */
		std::string Word;
		/** The day's sky as drawn (the recording's, or -TheaterLandWeather='s): its cover 0 to 1, and whether a deck. */
		double Cover = 0.0;
		bool bDeck = false;
		/** The snow lying on the field (LandField's, 0 to 1): what falls over snow is snow. */
		double SnowLying = 0.0;
		/** The look of the land (LandScape::NameOf: "jungle", "island", "rice", "desert", "drysteppe" ...). */
		std::string Look;
		/** "-TheaterLandFall=rain,0.8" (or "snow,0.5", or "none"): the Mac's stand-in, whatever the day. */
		std::string ForcedKind;
		double ForcedStrength = -1.0;
	};

	/** The battle's weather, and the day's changes for it. */
	struct Weather
	{
		WeatherFall::Kind Kind = WeatherFall::Kind::None;
		/** How hard it falls, 0 to 1 (light rain about 0.3, a downpour 1). */
		double Strength = 0.0;
		/** The open ground's wetness, 0 to 1 (rain only): darker, its sheen, the water standing in its low places. */
		double Wet = 0.0;
		/** The haze: its density times HazeTimes, its colour drawn HazeToward's share toward HazeInk (linear). */
		double HazeTimes = 1.0;
		double HazeToward = 0.0;
		double HazeInk[3] = { 0.5, 0.52, 0.55 };
		/** The day's lamps (the sun's key, the fill, the sky light) times this. */
		double LightTimes = 1.0;
		/** The sky's own brightness (the deck the battle draws) times this: a rain's sky a darker grey lid than a dry
		 * overcast's, and snow's darker than the snow lying under it (the art director's R1 and S2). */
		double SkyTimes = 1.0;
		/** The ground's and the things' surfaces when wet (M_LandSurface's and M_MapSurface's "Roughness" and "Specular");
		 * negative: as they were. */
		double GroundRough = -1.0, GroundSpecular = -1.0, ThingRough = -1.0, ThingSpecular = -1.0, SteelRough = -1.0, SteelSpecular = -1.0;
		/** The falling drops' colour, as a share of the haze's own (on the screen, against the far haze). */
		double InkOverHaze = 1.15;
		/** The rain's sound (the mix's beds.place.rain), 0 to 1. */
		double Sound = 0.0;
		/** In plain words, for the log. */
		std::string Why;
	};

	/** A number from the recipe (a path such as "pick.rain.from_cover"), false where it gives none. */
	using Book = std::function<bool(const char*, double&)>;

	/** The battle's weather (Book may be empty: the standing numbers). */
	Weather Pick(const Asked& A, const Book& Numbers);

	/** The share of the field's open ground made wet by rain, before the field is laid (no snow known yet): what
	 * LandField::Field::Rain takes. 0 where no rain falls. */
	double RainOnField(const Asked& A, const Book& Numbers);

	/** "rain,0.8" read into a kind word and a strength (-1 where none given). */
	void ReadForced(const std::string& Text, std::string& Kind, double& Strength);
}
