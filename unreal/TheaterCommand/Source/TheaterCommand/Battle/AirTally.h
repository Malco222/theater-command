#pragma once

#include <map>
#include <string>
#include <vector>

/**
 * An aircraft's record painted on her side: a yellow bomb for each mission flown and a symbol for each aircraft shot
 * down (Brian, 20 November: "yellow painted bombs for each bombing mission they did and then the symbols showing how
 * many aircraft they shot down", on every nation's bombers; and an ace's victories on his fighter, the air thread's
 * named aces of 22 November). Which mark goes in which of the model's slots, as the modeller's tallies.fill lays them
 * (battle-models/air/tally/README.md, the art director's ruling of 20 November): the kills grouped by the victim's
 * symbol, the commonest first, each group on a row of its own; the oldest bomb faded to 0.67 of the newest's contrast,
 * the oldest kill of a row only to 0.85.
 *
 * Engine-free: the cloud test checks it against the README's examples and the reference's rules
 * (Tools/air_show/air_tally_test.cpp).
 */
namespace AirTally
{
	constexpr int PerRow = 10;
	constexpr int KillRows = 3;
	constexpr double Fade = 0.67;
	constexpr double KillFade = 0.85;

	/** The symbol a victim of this nation is painted with (KILL-GER, KILL-UK for the Commonwealth, KILL-PLANE for anyone
	 * else). */
	std::string VictimSymbol(const std::string& Nation);

	/** The symbol a nation paints for every kill whatever the victim (KILL-SOV, KILL-BLOSSOM), or "" where it paints
	 * its victims'. */
	std::string OwnSymbol(const std::string& Nation);

	/** The rows of kill symbols, the lowest first, for groups (symbol, count) in order: each a list of symbols, "" for
	 * an empty place (tallies.kill_rows). */
	std::vector<std::vector<std::string>> KillRowsOf(const std::vector<std::pair<std::string, int>>& Groups);

	/** The kill symbols slot by slot (ten to a row, the lowest row first, each from the front), "" for an empty place:
	 * Nation is the painter's (the aircraft's own), By the victims' nations (tallies.kills_in_order). */
	std::vector<std::string> KillsInOrder(const std::string& Nation, int Kills, const std::map<std::string, int>& By);

	struct Mark
	{
		bool bKill = false;
		int Slot = 0;       // in the model's missions or kills slots
		int Row = 0;
		std::string Tag;    // BOMB or a KILL- symbol
		double Fade = 1.0;  // its contrast: 1 the newest
		int Tin = -1;       // a bomb's tin (TinsOf), -1 a kill
	};

	/** The bombs' yellows, sRGB, the newest row's first. */
	const std::vector<unsigned> & Tins();

	/** Every mark of a record (tallies.fill): MissionSlots and KillSlots the model's; Seat any whole number for the
	 * aircraft, so that no two of a formation share their tins. */
	std::vector<Mark> Fill(const std::string& Nation, int Missions, int Kills, const std::map<std::string, int>& By,
		int MissionSlots, int KillSlots, int Seat = 0);
}
