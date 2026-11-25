#pragma once

#include "Battle/AirFeed.h"
#include "Battle/SeaNightSky.h"

#include <string>

/**
 * THE SEA'S LOOK FROM THE RULES' FIELD (the release 2 ocean plan, step 4). Everything a recording's field says about how
 * the battle looks, read in one place: its hour, its light (the sun's height and bearing, the moon's, the night's
 * vis), its sky (cover, kind, cloud base and top, rain), its sea (swell and fetch), its date to the day, and the convoy
 * nights' own night{hours, day} and window{from, to, night}. Read with AirFeed's JSON reader from the text
 * ASeaBattle::OpenOn already holds, so SeaFeed (the show's reader) is not touched and the show reads none of it: only
 * light.vis, as now, which SeaFeed reads for the battle. Engine-free, so the cloud test reads exactly what the game
 * reads.
 *
 * Nothing here decides anything. An absent field, or an absent part of one, leaves that part at "not given", and every
 * reader then does today's thing, bit for bit.
 *
 * The dated ask to the rules (Tools/sea_waves/asks.txt, 24 November 2026): for every sea battle, fleet actions too,
 * emit when.hour, light{sun{up, from}, moon{lit, up, from}, vis}, sky{cover, kind, base, top, rain, word} and
 * sea{swell{height, period, from}, fetch}. Until it lands the three real fleet recordings give none of them, and the
 * convoy nights give light and night only.
 */
namespace SeaLookField
{
	/** light{sun, moon, vis}: each part a number or an object (form.md section 5, ruling 241). */
	struct Light
	{
		bool bGiven = false;
		double SunUp = 90.0;        // degrees over the horizon (below 0 under it)
		bool bSunFrom = false;      // light.sun.from given (SeaFeed reads only up)
		double SunFrom = 180.0;     // degrees clockwise from north
		bool bMoon = false;         // light.moon given
		double MoonLit = 0.0;       // 0 new, 1 full
		bool bMoonPlace = false;    // light.moon.up and .from given (a bare number gives only her lit share)
		double MoonUp = -90.0, MoonFrom = 180.0;
		double Vis = 0.0;           // metres a ship is seen at (0 not said)
	};

	/** sky{cover, kind, base, top, rain, word}: as air's sky.place.cloud is read (AirFeed), so a storm at sea and a deck
	 * over Bremen are one sky. */
	struct Sky
	{
		bool bGiven = false;
		double Cover = -1.0;        // 0 to 1 (below 0: not given)
		std::string Kind;           // "cumulus", "deck" or "squalls" (empty: not given)
		double Base = -1.0, Top = -1.0; // metres over the sea (below 0: not given)
		double Rain = -1.0;         // 0 to 1 (below 0: not given)
		std::string Word;           // the rules' own word for the sky, as the card says it
	};

	/** sea{swell{height, period, from}, fetch}. */
	struct Sea
	{
		bool bSwell = false;
		double SwellHeight = 0.0;   // metres, significant
		double SwellPeriod = 0.0;   // seconds
		double SwellFrom = 0.0;     // degrees clockwise from north, where it runs from
		double Fetch = -1.0;        // kilometres of open water upwind (below 0: not given)
	};

	/** A convoy night's own: night{hours, day} (the night's length and the month's day it fell on) and the recorded
	 * window into it, window{from, to, night} (seconds after nightfall, and the night's length in seconds). */
	struct Night
	{
		bool bGiven = false;
		double Hours = 0.0;
		int Day = 0;                // the day of the month, 1 to 31 (0: not given)
		bool bWindow = false;
		double From = 0.0, To = 0.0, Length = 0.0; // seconds
	};

	struct Field
	{
		/** Whether the field said anything for the look at all (when.hour, light, sky, sea or night): when not, every
		 * reader does today's thing. */
		bool bAny = false;
		double Hour = -1.0;         // when.hour, the local solar hour (below 0: not given)
		bool bWhen = false;
		int Year = 1941, Month = 4; // Month from 0 (January), as the field and SeaShow::Field keep it
		double Lat = 45.0, Lon = 0.0;
		double WindFrom = 0.0;      // radians clockwise from north, where it blows from
		double WindSpeed = -1.0;    // metres a second (below 0: not given)
		Light L;
		Sky S;
		Sea W;
		Night N;
		std::string Title, ZoneName;

		/** The day of the month the battle fell on: night.day where the rules give it, else the 15th. */
		int DayOfMonth() const { return N.Day >= 1 && N.Day <= 31 ? N.Day : 15; }
		/** The recorded window's middle, in hours after nightfall (below 0: no window given). */
		double WindowMiddleHours() const;
	};

	/** The field object of a recording (its "field"). */
	Field Read(const AirFeed::Json& FieldJson);
	/** A whole recording's text: its field read; false (and why) where the text is not JSON or holds no field. */
	bool ReadRecording(const std::string& Text, Field& Out, std::string* OutError = nullptr);

	/** The night sky's own "when" from the field (Battle/SeaNightSky): the place, the date to the day (its month from 1,
	 * as the sky counts), when.hour where given (else the sky finds the hour from the sun's depth, the evening's), the
	 * sun's height, and the moon where the rules placed her (a bare lit share places her nowhere, so no disc). A field
	 * that gives no light gives the sun high: the dome stays off. */
	SeaNightSky::When NightWhen(const Field& F);

	/** One line for the log, saying what the field gave for the look and what it did not ("not given" for each part
	 * missing), so a Mac run says which battles carry the dated ask's data. */
	std::string Said(const Field& F);
}
