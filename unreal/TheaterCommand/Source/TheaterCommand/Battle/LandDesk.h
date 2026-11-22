#pragma once

#include "Battle/LandFeed.h"

#include <string>
#include <vector>

/**
 * The ground battle's words and bar, plain C++ (the screen draws them): the title, the place and date, the clock, the
 * tug-of-war bar (the one rule for air, land and sea: /mnt/project-files/air-battles/tug-of-war-bar/README.md, the land
 * form; the look the UI director's ruling of 21 November 14:47Z), how near each army is to breaking, the battle's own
 * dispatches as it speaks them, and the after card.
 *
 * The recording is the computer fighting both sides, so the "player" here is the attack (the Americans): the bar's
 * left flag is his.
 */
namespace LandDesk
{
	struct Line
	{
		double T = 0.0;
		std::string Text;
		int Side = -1;
		/** 0 to 1: how far it has faded (the screen keeps the last few, the newest bright). */
		double Age = 0.0;
	};

	struct Words
	{
		std::string Title, Place, Date;
		/** The battle clock as m:ss. */
		std::string Clock;
		/** The bar's knot from the player's flag (left): 0.5 level, over it he is winning. Eased. */
		double Knot = 0.5;
		/** HOW NEAR TO BREAKING OFF, each army, 0 to 1. */
		double BreakOwn = 0.0, BreakEnemy = 0.0;
		std::string OwnNat, EnemyNat;
		std::vector<Line> Lines;
		bool bOver = false;
		std::string Level;
		std::vector<std::string> After;
	};

	class Desk
	{
	public:
		void Begin(const LandFeed::Recording& InRecording, int InOwn = 0);
		/** The words at battle time Battle; RealDt the real seconds since the last update (the knot's easing). */
		const Words& Update(double Battle, double RealDt);
		/** The knot as the rule gives it at a battle time, before easing (for the test). */
		double KnotAt(double Battle) const;

	private:
		const LandFeed::Recording* Rec = nullptr;
		int Own = 0;
		Words Now;
		bool bEased = false;
		double Balance0[2] = { 0.5, 0.5 };
		bool bBalance0 = false;
	};
}
