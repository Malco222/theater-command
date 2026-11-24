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
	 * an empty place (tallies.kill_rows). Across: the marks to a row (the model's kills.per_row: ten, the Spitfire's
	 * nine, her number coming forward over a tenth column). */
	std::vector<std::vector<std::string>> KillRowsOf(const std::vector<std::pair<std::string, int>>& Groups, int Across = PerRow);

	/** The kill symbols slot by slot (Across to a row, the lowest row first, each from the front), "" for an empty
	 * place: Nation is the painter's (the aircraft's own), By the victims' nations (tallies.kills_in_order). */
	std::vector<std::string> KillsInOrder(const std::string& Nation, int Kills, const std::map<std::string, int>& By, int Across = PerRow);

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
	 * aircraft, so that no two of a formation share their tins; KillsAcross the model's kills.per_row (bombs are ten to
	 * a row always). */
	std::vector<Mark> Fill(const std::string& Nation, int Missions, int Kills, const std::map<std::string, int>& By,
		int MissionSlots, int KillSlots, int Seat = 0, int KillsAcross = PerRow);

	// ---- An ace's name under his cockpit (Brian's named aces, release 2: the modeller's tally.name on every fighter,
	// battle-models/air-next/ACES.md, and acepanel.py on claude/project-thread-3vdnfu, whose rules these follow) ----

	/** The letters' sheet's index (Tools/name_letters.py, Content/Data/Tally/name-letters.json): its size, the cells'
	 * size and the row of the capitals' middle in each, in pixels; the capitals' height in pixels; the word space and
	 * the gap between letters, in capitals' heights; and each character's cell and its width as painted, in capitals'
	 * heights (BAR the solid cell). */
	struct Letters
	{
		struct Cell
		{
			int Col = 0;
			int Row = 0;
			double Wide = 0.0;
		};
		int Width = 0, Height = 0, Cap = 0, CellW = 0, CellH = 0, Mid = 0;
		double Space = 0.45, Gap = 0.16;
		std::map<std::string, Cell> Cells;
	};

	/** Reads the index (one cell a line, as Tools/name_letters.py writes it). False if it is not one. */
	bool ReadLetters(const std::string& Json, Letters& Out);

	/** His title as the rules give it (aces.ts aceTitle: "Flight Lieutenant Tom Harland") in the forms that may be
	 * painted, the one wanted first: his rank short as his nation wrote it, his initials and surname, in capitals
	 * ("F/LT T. HARLAND"); then rank and surname; then the surname alone. A nation with no short ranks in ACES.md
	 * paints none (Japan's: initial and surname, "T. KAWANO"). */
	std::vector<std::string> NameTries(const std::string& Nation, const std::string& Title);

	/** One letter of his name: its character (UTF-8), its middle in metres aft of the line's front, its width in
	 * metres as painted. */
	struct NameLetter
	{
		std::string Ch;
		double Middle = 0.0;
		double Wide = 0.0;
	};

	/** His name laid on his line: the form painted, the capitals' height (metres), the letters (spaces left out), and
	 * how far it runs (metres), with the characters the sheet has no cell for. */
	struct NameLaid
	{
		std::string Text;
		double Cap = 0.0;
		double Run = 0.0;
		std::vector<NameLetter> Letters;
		std::string Missing;
	};

	/** The first form that goes in Long with capitals Cap high, or smaller down to CapMin (acepanel.fitted); none
	 * fitting, the last set to fit. Left-aligned from the line's front, the hand's gap between letters. */
	NameLaid LayName(const std::vector<std::string>& Tries, const Letters& Hand, double Long, double Cap, double CapMin);

	/** A colour's lightness L* (0 black, 100 white), from its sRGB 0xRRGGBB. */
	double Lightness(unsigned Rgb);

	/** An American olive drab: a dark brownish green (L* under 45, red and green within a tenth of each other, both 10
	 * or more over blue and at least 1.15 times it: the Warhawk's 4F5236, the modeller's Mustang 4C4B35, the game's
	 * Mustang in the art director's matte 555A47, the unit table's 565C3A). */
	bool OliveDrab(unsigned Rgb);

	/** The ink of his name for the paint under it (ACES.md, the art director's 19:47Z): yellow 0xD9B43A on an American
	 * machine's olive drab (the owner decides: the RAF's dark green is nearly the same colour); else 0x1A1A1A where its
	 * L* is 50 or more, and 0xF2F0E8 under that (the bombers' record rule). */
	unsigned NameInk(unsigned SkinRgb, const std::string& Owner);

	/** The white of a German pilot's bar a kill on his rudder (the modeller's marks.BAR_WHITE). */
	constexpr unsigned BarWhite = 0xDAD8CE;

	/** A title for the test ace of this nation, as the rules would give one (invented, from their own lists). */
	std::string SampleTitle(const std::string& Nation);
}
