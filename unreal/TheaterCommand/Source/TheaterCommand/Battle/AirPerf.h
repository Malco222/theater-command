#pragma once

#include <map>
#include <string>
#include <vector>

/**
 * Every aircraft flies as its type did (Brian, 24 November 2026: "i want the flying to be smooth and very realistic" and
 * "i want the planes flying with realistic capabilities and tactics"). Until now the flight core (Battle/AirShow,
 * Show::Fit) gave every fighter the same limits and every bomber the same: a Zero turned as a Wildcat, a Fw 190 rolled
 * as a Spitfire and a Me 262 was no faster than a P-51. The historian's table, Content/Data/Air/performance.json, holds
 * each type's real figures (size, weight, power, speeds and heights, climb, turn, roll, g, dive, load and range), every
 * figure with its source and a mark (measured, manufacturer, derived or uncertain); this reads it, finds a type by the
 * names the scene and the rules give it, and works out the limits the flight core uses, in its own units.
 *
 * Nothing here decides anything: the rules decide who falls; this only says how each type flew (the scene draws).
 * Plain C++, no engine: the cloud test (Tools/air_show/air_perf_test.cpp) reads exactly what the game reads, with the
 * air feed's own JSON reader (AirFeed::Parse, in AirPerf.cpp only, so that this header can be included anywhere).
 *
 * How the limits are worked out (Limits), each formula stated so a builder can check it:
 * - The air: the standard atmosphere's density, rho(h) = 1.225 (1 - 2.25577e-5 h)^4.25588 kg/m3 (to 11 km).
 * - Power at a height: the row's power (horsepower, all engines) held from the ground up to its rated height, and above
 *   it falling with the density, P(h) = P x rho(h) / rho(rated); where the historian gave a low gear's power (a two-speed
 *   supercharger's, the Fw 190's), that up to the low gear's height and straight across from there to the rated power.
 *   A propeller's thrust T = 0.8 P / V (no more than at 40 m/s); a jet's T = thrust x (rho(h) / 1.225)^0.7, the same at
 *   every speed.
 * - Drag: its zero-lift drag coefficient from its own top speed (Vt at the height it is reached, where thrust equals
 *   drag): CD0 = (T - W^2 / (q S pi e A)) / (q S), q = rho Vt^2 / 2, W its weight (newtons), S its wing area, A its
 *   aspect ratio span^2 / S, e = 0.8; held between 0.012 and 0.06.
 * - Its lift in a hard turn: CL = 1.35, a plain wing's (the rules' data/aircraft.json: the Spitfire 1.36, the Hurricane
 *   1.35); or the row's own clmax where the historian gave one (the slats of the Bf 109 and the Me 262, the Ki-43's combat
 *   flaps: 1.45). Not from the stall speeds, which are the table's least sure figures.
 * - The tightest sustained turn, at FightM (3,000 m): at each speed V from just over its stall to its top speed, the
 *   load it can hold with its thrust, n = sqrt((T - q S CD0) q S pi e A) / W, no more than its lift allows (CL q S / W)
 *   or its pilot can stand (GMax); its radius V^2 / (g sqrt(n^2 - 1)); RMin the least of them. Where the historian laid
 *   a measured radius, that instead. For the Spitfire I this gives about 210 m at 250 km/h, the RAE's 696 ft; for the
 *   Bf 109E about 260 m, the RAE's 885 ft (270 m). Held to 150 to 450 m for a fighter, 150 to 2,500 m for any other
 *   (only the Me 262 is held: about 495 m worked out, 450 flown).
 * - The roll's inertia: its roll settles in a time tau = (m / S) k^2 / (0.1125 rho V), from its roll inertia m (k b)^2 and
 *   its wing's roll damping (Clp = -0.45, a straight wing of aspect ratio about 6); k = 0.13 for one engine in the nose,
 *   0.16 for twins, 0.19 for three or four engines or a span of 28 m or more; V its fighting speed, at FightM. About 0.15 s
 *   for a Spitfire, 0.4 for a He 111, 0.7 for a B-17.
 *
 * Speeds in the file are as published: top speed and cruise true at their heights; stall, roll and dive speeds
 * indicated. The flight core has no air density, so VMin and VDive are the indicated figures as metres a second (true at
 * the ground; about a sixth faster at 3,000 m).
 */
namespace AirPerf
{
	/** The height the turn and the roll's inertia are worked at (the historian: "turning ... at about 3,000 m"). */
	constexpr double FightM = 3000.0;

	/** One type's figures, in the file's own units (km/h, m, m/s, kg, hp, kN, degrees a second); 0 where not given. */
	struct Row
	{
		std::string Key, Nation, Role, Style;
		std::vector<std::string> Names;
		int Engines = 1;
		double Span = 0.0, Wing = 0.0, Mass = 0.0;
		double Power = 0.0, PowerM = 0.0;    // all engines' horsepower at its rated height, metres
		double PowerLow = 0.0, PowerLowM = 0.0; // a two-speed supercharger's low gear: its power and its height (0: none)
		double Thrust = 0.0;                 // a jet's static thrust at sea level, all engines, kN (0: propellers)
		double Top = 0.0, TopM = 0.0;        // top level speed, true, km/h, at TopM metres
		double Cruise = 0.0, Stall = 0.0;    // its cruise on operations (true) and its clean stall (indicated), km/h
		double Climb = 0.0, Ceiling = 0.0;   // initial climb m/s; service ceiling m
		double TurnRadius = 0.0, TurnTime = 0.0, TurnM = 0.0; // a documented sustained turn (m, s) at TurnM metres
		std::string TurnMark;                // how sure the documented radius is (measured, manufacturer, derived, uncertain)
		double Roll = 0.0, RollKmh = 0.0;    // peak rate of roll, degrees a second, at RollKmh indicated
		double RollFast = 0.0, RollFastKmh = 0.0; // its rate of roll at a high speed, and that speed
		double GLimit = 0.0, Dive = 0.0;     // the airframe's limit load factor; the placard dive limit, km/h indicated
		double Bomb = 0.0, Range = 0.0;
		double ClMax = 0.0;                  // the lift it uses in a hard turn, where the historian gave it (0: a plain wing's)
		/** A model's marks by date (the 3D model's own first): the row to fly from each year on. */
		struct Mark
		{
			std::string Of;
			double From = 0.0;
		};
		std::vector<Mark> Marks;
		/** A fighter of any kind (fighter, heavy fighter, jet fighter, carrier fighter). */
		bool IsFighter() const { return Role.find("fighter") != std::string::npos; }
	};

	/** What the flight core flies by (AirShow::Show::Craft's limits), in its units: metres, seconds, radians. */
	struct Flight
	{
		double VMin = 0.0;       // 1.15 times its clean stall, m/s (its slowest in a fight)
		double VMax = 0.0;       // the speed it cruises and fights at in formation, m/s
		double VTop = 0.0;       // top level speed, m/s
		double VDive = 0.0;      // its placard dive limit, m/s (never under VTop)
		double RMin = 0.0;       // tightest sustained turn at its fighting speed, metres
		double GMax = 0.0;       // the most g it pulls: its airframe's limit, no more than its pilot stands (6)
		double RollMax = 0.0;    // peak rate of roll, radians a second
		double RollTime = 0.0;   // seconds for its roll rate to pick up most of the way (the roll's inertia)
		double ClimbMost = 0.0;  // initial climb, m/s
		double Ceiling = 0.0;    // service ceiling, m
	};

	/** The whole table: the rows, the names they are found by, and the plain models' stand-ins. */
	struct Table
	{
		std::vector<Row> Rows;
		/** Every row's key and names, plain (Plain), to its place in Rows. */
		std::map<std::string, size_t> ByName;
		/** A plain model the scene draws for a nation ("Fighter:SOV", "Torpedo:ITA", "Bomber:FRA") and the type it stands
		 * for, by nation and date: the first entry whose nation is the one asked (or none given) and whose Until is past. */
		struct StandIn
		{
			std::string Nation, Of;
			double Until = 0.0;  // the year (with its fraction) from which it no longer stands; 0: always
		};
		std::map<std::string, std::vector<StandIn>> StandIns;

		/**
		 * The row for a model as the scene or the rules name it: "Bf109", "Bf 109", "Me 109", "B-17", "B17", "Kate:JAP",
		 * "Fighter:SOV". Nation: the nation's tag where the name does not carry it ("Fighter" with "SOV"). Year: the battle's
		 * date as a year with its fraction (June 1943 is 1943.42; 0: not known); a model's plain name then flies the mark
		 * of that date (as guns.ts arms it: a "Spitfire" of 1943 is a IX), its own key never changes. Null if none.
		 */
		const Row* Of(const std::string& Model, const std::string& Nation = std::string(), double Year = 0.0) const;
		/** A row by its key or one of its names, no marks or stand-ins. */
		const Row* Named(const std::string& Name) const;
	};

	/** Reads the table's JSON text; false (and why) on a fault. */
	bool Read(const std::string& Text, Table& Out, std::string& OutError);

	/** The flight core's limits for a type (the formulas above). */
	Flight Limits(const Row& R);

	/** Its rate of roll at an indicated speed (m/s), radians a second: rising to its peak at RollKmh, falling to RollFast
	 * at RollFastKmh and on down beyond it (never under a third of RollFast), as the wartime roll charts run. */
	double RollAt(const Row& R, double IndicatedMs);

	/** Its tightest sustained turn's radius at a height and its turn at that radius (metres; seconds for the full
	 * turn), the formulas above; 0 if it cannot hold a turn there. */
	double TurnRadius(const Row& R, double HeightM, double* OutTurnSeconds = nullptr);

	/** The standard atmosphere's density at a height, kg/m3. */
	double Density(double HeightM);

	/** A name made plain for finding: lower case, letters and digits only ("Bf 109" and "Bf109" are both "bf109"). */
	std::string Plain(const std::string& Name);
}
