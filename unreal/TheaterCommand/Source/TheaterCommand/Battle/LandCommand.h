#pragma once

// The general's command of a live ground battle (release 2; Brian, 22 November 2026: "im the general who controls their
// deployment and who to attack and move on a map with tactics"; "i also want the game play to be smooth, fun, and
// realistic"). Plain C++, no engine: the cloud test drives it as the screen does.
//
// What the screen asks and what it says back, kept apart from the drawing: each block's tag (where it stands now, whose
// it is, its order and whether that order is still on the air), which blocks are in his hand, what a click or a drag
// means (go there, attack that, form a line along this), and the order it makes, as the rules take it (TC.landOrder's
// command, JSON). The rules have the last word on every order and placing; this only asks.
//
// Like a coach's clipboard: the players' places as they stand, a ring round the ones he is talking to, and the play he
// calls written in the league's own words. The players run it.

#include "Battle/LandFeed.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace LandCommand
{
	/** What a block's tag says of whose it is: his staff's (the computer runs it), his own (taken in hand), an order of
	 * his on the air to it; the enemy's, met or not yet; gone from the field. */
	enum class Chip : uint8_t
	{
		Staff,
		Yours,
		Radio,
		Enemy,
		Unseen,
		Gone
	};

	/** A block on the general's map now (the view at or before the time shown, eased to the next). */
	struct Tag
	{
		int Block = 0;
		/** 0 the attack, 1 the defence. */
		int Side = -1;
		double X = 0.0, Y = 0.0;
		/** Radians, 0 north, clockwise. */
		double Face = 0.0;
		/** Its frontage and depth, metres. */
		double W = 0.0, D = 0.0;
		/** The header's words: its name, its short name, the formation it belongs to, its kind (inf, arm, art, ...). */
		std::string Name, Short, Parent, Kind;
		/** A support team's kind ('mg', 'mor'; empty for every other block): its card says MACHINE-GUNS or MORTARS. */
		std::string Team;
		double Str = 1.0, Mor = 1.0;
		int Figures = 0, Figures0 = 0;
		/** The real men it stands for now (-1 not said: an enemy not met) and at the start; the men drawn for it on the
		 * field (its men's tracks so far); and its side's drawing ratio, one figure for Ratio men (0 for a block of tanks
		 * or guns, or none drawn yet). The historian, 22 November 05:50Z: the parent line carries the real men and the
		 * ratio, as Brian's 04:14Z rule asks. */
		int Men = -1, Men0 = 0, Drawn = 0, Ratio = 0;
		/** steady, wavering, routing, gone, withdrawn ("unknown" for an enemy not met); what it is doing. */
		std::string State, Act;
		bool bMine = false;
		/** An enemy the player's men have met (else only where it stands and whether it moves). */
		bool bKnown = true;
		/** His, in the fight, not routing and not a fortification: it takes his orders. */
		bool bOrderable = false;
		bool bTaken = false;
		bool bChosen = false;
		/** Hidden from the other side (in woods, not yet found): his own show the forest mark; the enemy's are not shown. */
		bool bHidden = false;
		/** An enemy found by its fire, not yet located; and his own firing into cover at a point (reconnaissance by fire). */
		bool bVague = false, bSearch = false;
		double SearchX = 0.0, SearchY = 0.0;
		/** His own: a tank with hedge cutters (it can punch a gap in a hedge), and the smoke it can still lay (-1 none). */
		bool bCutter = false;
		int Smoke = -1;
		/** His own, holding until relieved: waiting (dug in where it stands while not free), the battle second help can
		 * reach it, help never came (its men break sooner), no longer waiting. */
		bool bWaits = false, bWaitLate = false, bWaitFree = false;
		double WaitDue = -1.0;
		/** Seconds till his order reaches it, -1 none on the air. */
		double OrderIn = -1.0;
		Chip Says = Chip::Staff;
		/** Its order (move, attack, withdraw; empty none), the point it goes to, the block it attacks (-1) and its path. */
		std::string OrderKind;
		double OrderX = 0.0, OrderY = 0.0;
		int OrderTarget = -1;
		std::vector<std::pair<double, double>> Path;
	};

	/**
	 * A corps coming up from next door, not yet on the field (neighbours join the fight, release 2; the campaign card
	 * priced it in): his own side's, and the enemy's his staff knows of. It walks on whole at its time, at a point on the
	 * field's edge, its blocks under his staff like the rest.
	 */
	struct Neighbour
	{
		int Corps = 0;
		/** 0 the attack, 1 the defence; his own side's or the enemy's. */
		int Side = -1;
		bool bMine = false;
		/** The formation's name (its blocks' parent), its leading kind (arm where it brings tanks, else its first block's),
		 * its blocks, and the real men it brings (his own; -1 for the enemy's, not counted by his staff). */
		std::string Name, Kind;
		int Units = 0, Men = -1;
		/** Battle seconds till it walks on (0 at its time), the card's word for when (early, midway, late; may be empty),
		 * and where: the field's edge (south, north, west, east) and the point on it, field metres. */
		double In = 0.0;
		std::string Edge, Arrive;
		double X = 0.0, Y = 0.0;
	};

	/** Where one block of a line would stand (the rules' own lining up, for the line drawn while he drags). */
	struct Ghost
	{
		int Block = 0;
		double X = 0.0, Y = 0.0, Face = 0.0;
	};

	class General
	{
	public:
		/** From the battle's header: his side (none: a film, nothing to command) and each block's words. */
		void Begin(const LandFeed::Recording& Rec);
		/** The tags at battle time T (the screen's): each block from the view at or before T, eased toward the next. */
		void Update(const LandFeed::Recording& Rec, double T);

		const std::vector<Tag>& Tags() const { return Shown; }
		const Tag* TagOf(int Block) const;
		/** The corps still coming up from next door at the screen's time, soonest first (none: empty). */
		const std::vector<Neighbour>& Neighbours() const { return Coming; }
		/** His own corps' despatch riders at the screen's time, each eased toward the next view (none: empty). */
		const std::vector<LandFeed::Rider>& Riders() const { return RidersNow; }
		/** The rider nearest the point within Reach metres, as an index into Riders(); -1 none. */
		int RiderAt(double X, double Y, double Reach) const;
		/** 0 the attack, 1 the defence, -1 a film. */
		int Side() const { return Me; }
		/** The battle time the tags were last read at (the screen's). */
		double Now() const { return ReadAt; }
		/** The setup step is open: he stands his blocks on his ground, the clock still. */
		bool Deploying() const { return bSetup; }

		// ---- Picking (ground points: the screen finds the ground under the pointer; Reach, metres, from its zoom) ----
		/** His orderable block nearest the point within Reach (or over whose ground it falls), -1 none. */
		int MineAt(double X, double Y, double Reach) const;
		/** An enemy block on the field nearest the point within Reach (met or not: he may attack what he sees), -1 none. */
		int EnemyAt(double X, double Y, double Reach) const;

		// ---- His hand: the blocks he has picked up ----
		/** A click on his block: it alone in hand, or (bAdd, Shift) added or put down again. */
		void Pick(int Block, bool bAdd);
		/** A box drawn round several: the orderable among them in hand (bAdd keeps those already in it). */
		void Choose(const std::vector<int>& Blocks, bool bAdd);
		void Clear();
		/** In hand, orderable now, in the order picked up. */
		std::vector<int> Chosen() const;
		/** Tab: his next orderable block after the first in hand (bBack the one before), alone in hand; -1 none. */
		int Next(bool bBack);

		// ---- Orders, as TC.landOrder takes them (JSON; "" when nothing in hand can take it) ----
		/** One click on the field with blocks in hand: an enemy there is attacked (guns alone fire on it and stay where
		 * they are), else they go there, keeping their shape; at the double when bRun. */
		std::string Click(double X, double Y, double Reach, bool bRun) const;
		std::string Move(double X, double Y, bool bRun) const;
		std::string Attack(int Enemy) const;
		std::string FireOn(int Enemy) const;
		/** Reconnaissance by fire: those in hand stand and fire into the cover at a point (a wood, a hedge) to flush out
		 * what hides there (the historian's check, 22 November); "" with none in hand. */
		std::string FireInto(double X, double Y) const;
		/** A line from (X0, Y0) to (X1, Y1): the blocks spread along it, facing the enemy's side. */
		std::string Line(double X0, double Y0, double X1, double Y1) const;
		std::string Hold() const;
		std::string FallBack() const;
		/** Handed back to his staff (those he took in hand). */
		std::string ToStaff() const;
		/** The rules' own lining up of the blocks in hand along a line (for the line drawn while he drags). */
		std::vector<Ghost> LineUp(double X0, double Y0, double X1, double Y1) const;

		// ---- Hedge cutters and smoke on call (Brian, 22 November 14:19Z; shown only where the rules give a block one) ----
		/** The first block in hand with hedge cutters, -1 none. */
		int Cutter() const;
		/** Those in hand with smoke still to lay. */
		std::vector<int> Smokers() const;
		/** The cutter in hand punches a gap in the hedge at (X, Y) (the screen snaps the point to a hedge first). */
		std::string Cut(double X, double Y) const;
		/** Those in hand with smoke lay a line of it from (X0, Y0) to (X1, Y1). */
		std::string SmokeLine(double X0, double Y0, double X1, double Y1) const;

		// ---- Hold until relieved (release 2; Brian, 22 November 12:48Z; the rules decide who may, and say why not) ----
		/** A corps of his is coming up from next door, or is on the field: help that a block of his may wait for. */
		bool HelpComing() const { return bHelp; }
		/** Those in hand on foot, not waiting already (the rules take one a corps and say why for the rest). */
		std::vector<int> Waiters() const;
		/** Those in hand on foot dig in and wait for help. */
		std::string Wait() const;

		/**
		 * An order he has just given, shown at once: the screen runs a couple of battle seconds behind the rules, so the
		 * views only say what they made of it once the screen reaches the moment it was given (Until: the rules' time
		 * then, and a little). Till then its blocks' tags say it is on the air, with its route (Kind: move, attack,
		 * withdraw, hold, line; staff hands them back), each block to its own point (Points, one a block, or one for all).
		 */
		void Sent(const std::vector<int>& Ids, const std::string& Kind, const std::vector<std::pair<double, double>>& Points, int Target, double Until);

		// ---- The setup step ----
		/** Whether the block may stand there as far as the screen can tell (wholly inside his ground; the rules decide
		 * the rest: water, a bend's lanes, another block). */
		bool MayStand(int Block, double X, double Y) const;
		/** His ground [x0, y0, x1, y1]. */
		const double* Room() const { return RoomAt; }

	private:
		struct Words
		{
			int Id = 0, Side = -1;
			std::string Name, Short, Parent, Kind, Team;
			double W = 0.0, D = 0.0;
			int Figures0 = 0, Men0 = 0;
			/** Drawn as tanks or guns (a model), not men. */
			bool bVehicles = false;
		};
		struct Pending
		{
			int Block = 0;
			std::string Kind;
			double X = 0.0, Y = 0.0;
			int Target = -1;
			double Until = 0.0;
		};
		std::vector<Words> Blocks;
		/** The men drawn for each block (by Blocks' order), counted from the tracks so far; each side's ratio. */
		std::vector<int> DrawnOf;
		size_t Counted = 0;
		int RatioOf[2] = { 0, 0 };
		/** The men's tracks counted up to the recording's now, and each side's ratio from them. */
		void CountDrawn(const LandFeed::Recording& Rec);
		std::vector<Tag> Shown;
		std::vector<Neighbour> Coming;
		std::vector<LandFeed::Rider> RidersNow;
		bool bHelp = false;
		double ReadAt = 0.0;
		std::vector<Pending> Given;
		std::vector<int> Hand;
		int Me = -1;
		bool bSetup = false;
		double RoomAt[4] = { 0, 0, 0, 0 };

		const Words* WordsOf(int Id) const;
		std::string Units(const std::vector<int>& Ids) const;
	};

	/** A number as the rules' JSON takes it: finite, to the centimetre. */
	std::string Number(double V);

	/** The nearest point to (X, Y) on any of the lines (each a run of corners), within Reach metres: true and the point
	 * where one is (a hedge for the cutters to snap to), false where none comes that close. */
	bool NearestOnLines(const std::vector<const std::vector<std::pair<double, double>>*>& Lines, double X, double Y, double Reach, double& OutX, double& OutY);
}
