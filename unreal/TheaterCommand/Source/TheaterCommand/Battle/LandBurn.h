#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * A knocked-out vehicle's fire as a film would show it (release 2, behind -TheaterLandFilmFire; Brian, 23 November
 * 2026 14:44Z: "the tank on fire doesnt look realistic"). Plain C++, so the effects preview draws exactly the plan the
 * game plays.
 *
 * Before it, every kill lit four fires of 2.4 m over her hull (her burn clip's fire loops at the engine, the deck's
 * side and both hatches) that the effects kit joined into one body of flame 3 m across, roaring the same from the
 * kill to the battle's end, every kill alike. A burning tank in the newsreels is mostly smoke, and her flame comes out
 * of her openings: a tall roaring tongue out of a hatch or the open turret ring once her ammunition goes, lower
 * flames licking out of the engine grilles, a thick black column, the rounds inside cooking off in pops for minutes,
 * then the flames dropping, and the wreck smouldering on in grey wisps from her hatches long after.
 *
 * So each kill gets a story by how the fates table says she ended (Vehicle::FateKey) and her size, drawn from her own
 * dice so no two wrecks burn alike: which openings burn (the kit's Vent: a burning opening that never melts into the
 * deck's fire), how long her deck fire lasts (the kit's fire.late lowers it after 3 to 5 minutes), when the rounds
 * cook off, and the wisps after. Fates that do not burn (her crew bailed out, her crew killed, a track blown off)
 * only smoke a while from a hatch. Times are life's seconds after her kill; places are her model's sockets.
 */
namespace LandBurn
{
	enum class Do : uint8_t
	{
		Fire,  // the kit's Burn: a fire Across wide at Socket for Seconds of flame (then its grey column)
		Vent,  // the kit's Vent: flame and smoke forced out of an opening Across wide at Socket for Seconds
		Cook,  // the kit's CookOff at Socket: Pops pops over Over seconds; a jet of Jet seconds after the last (0: none)
		Wisp,  // the kit's Steam of Kind (smoulder, smoke) at Socket, Across wide, for Seconds
	};

	struct Call
	{
		double At = 0.0;        // life's seconds after her kill
		Do What = Do::Fire;
		std::string Socket;     // engine, fire.1, fire.2 (the commander's hatch), fire.3 (the driver's), ring
		std::string Kind;       // a Wisp's steam row
		double Across = 0.0, Seconds = 0.0;
		int Pops = 0;
		double Over = 0.0, Jet = 0.0;
		int Slot = 0;           // her own number for it (0 to 15): the caller makes it unique among the battle's
	};

	struct Wreck
	{
		std::string Fate;       // the fates table's key (tank.brew_up, tank.engine_fire ...); empty: none picked
		bool bBrewUp = false;   // her magazine went (the brew_up clip: the turret thrown, the ring open)
		bool bRing = false;     // she has a turret ring socket
		bool bHatch = true;     // she has the commander's hatch fire socket (fire.2)
		bool bDriver = true;    // and the driver's (fire.3)
		std::string Class;      // heavy, medium, light (an armoured car or a half-track too), lorry, car, gun, none
		uint64_t Seed = 1;
		double Left = 600.0;    // life's seconds from her kill to the battle's end (and a little after)
	};

	struct Plan
	{
		std::vector<Call> Calls; // by At
		bool bBurns = false;     // flames at all (her soot)
		double FlamesFrom = -1.0, FlamesUntil = -1.0; // life's seconds after her kill: her first flame and her deck fire's last
		std::string Story;       // for the log: what she does
	};

	/** Her fire's story. The same Wreck gives the same plan. */
	Plan Make(const Wreck& W);

	/** The scene's book of the battle's wrecks: each vehicle's plan, made at her kill, and the next call due. */
	struct Director
	{
		struct Her
		{
			bool bMade = false;
			Plan Story;
			size_t Next = 0;
		};
		std::vector<Her> All;
		/** -TheaterLandFilmFire (release 2, off by default until the art director and a 45 fps reading pass), read as
		 * the scene opens (its look, battle-fx-film.json, is laid over the effects recipe then). */
		bool bOn = false, bAsked = false;
		Her& Of(size_t Vehicle)
		{
			if (All.size() <= Vehicle)
			{
				All.resize(Vehicle + 1);
			}
			return All[Vehicle];
		}
		/** The kit's number for her call: above every number the scene gives its own fires (E.Key, E.Key + 200000),
		 * below the kit's own columns' (1 << 28). */
		static int IdOf(size_t Vehicle, int Slot) { return (1 << 26) + static_cast<int>(Vehicle % 400000) * 16 + (Slot & 15); }
	};

	/** A vehicle's class from her model's kind, name and size (a model file's "kind" and name, her length in metres). */
	std::string ClassOf(const std::string& Kind, const std::string& Name, double Long);
}
