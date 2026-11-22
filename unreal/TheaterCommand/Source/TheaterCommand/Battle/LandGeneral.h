#pragma once

// What the general's screen draws over a live ground battle this frame (release 2): his clipboard (LandCommand), each
// block's tag put on the screen by the scene (LandBattleCommand.cpp), the routes of his orders, his drag's box or line,
// a block he carries in the setup step and his ground's edge. Plain C++, kept in the scene's state; SLandBattleScreen
// draws it. Every place is in the viewport's pixels.

#include "Battle/LandCommand.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

struct FLandGeneralShown
{
	/** The general's clipboard: the tags, his hand, the orders. */
	LandCommand::General Clipboard;
	/** His controls are up (a live battle with a side of his own), and whether the film has the camera instead (C; Esc
	 * brings the map back). */
	bool bOn = false, bFilm = false;
	double ViewW = 0.0, ViewH = 0.0;

	/** A tag on the screen: where its stalk stands on the ground and where its tag hangs over it. */
	struct Mark
	{
		int Block = -1;
		double X = 0.0, Y = 0.0;
		double FootX = 0.0, FootY = 0.0;
		/** The block's ground's four corners (chosen blocks only, for the ring round them). */
		bool bCorners = false;
		double Corners[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	};
	std::vector<Mark> Marks;

	/** An order's route on the ground: from the block along its path to where it goes or whom it attacks. */
	struct Route
	{
		int Block = -1;
		LandCommand::Chip Says = LandCommand::Chip::Staff;
		bool bAttack = false, bBack = false, bChosen = false;
		std::vector<std::pair<double, double>> Points;
	};
	std::vector<Route> Routes;

	/** His drag: a box (x0, y0, x1, y1); a line (its two ends) and where each block in hand would stand along it. */
	bool bBox = false;
	double Box[4] = { 0, 0, 0, 0 };
	bool bLine = false;
	double LineEnds[4] = { 0, 0, 0, 0 };
	std::vector<std::pair<double, double>> Ghosts;

	/** A block carried in the setup step: its ground's corners where he would set it down, and whether it may stand. */
	bool bCarry = false;
	int Carried = -1;
	double Corners[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	bool bCarryOk = false;
	/** His ground's edge in the setup step, along the ground. */
	std::vector<std::pair<double, double>> RoomEdge;
	/** The woods' edges on and near his ground (field metres, two points a stretch), made once; and in the setup step on
	 * the screen this frame (two points a stretch, those behind the eye left out). A block set down in woods starts hidden
	 * from the enemy (Brian's ambushes, 22 November 04:18Z). */
	bool bWoodsMade = false;
	std::vector<std::pair<double, double>> WoodsField, WoodsEdge;
	/** The block carried would stand in woods: it would start hidden. */
	bool bCarryWoods = false;
	/** The enemy's blocks hidden from him this frame (in woods, not yet found), sorted: their men, vehicles and tags are
	 * not drawn, the film never frames them, and he cannot point at them. */
	std::vector<int> FoeHidden;
	bool Hides(int Block) const { return std::binary_search(FoeHidden.begin(), FoeHidden.end(), Block); }

	/** The pointer, the tag or block under it (-1 none) and what a click would do there ("GO HERE", "ATTACK", ...). */
	double PointerX = -1.0, PointerY = -1.0;
	int Hover = -1;
	std::string Would;
	/** An order armed by a button: the next click goes at the double ("double") or fires on what it is on ("fire"). */
	std::string Armed;
	/** Where he last gave an order (on the field, and on the screen this frame) and when (real seconds), for the mark
	 * that answers his click at once. */
	double PingFieldX = 0.0, PingFieldY = 0.0, PingX = 0.0, PingY = 0.0, PingAt = -10.0;
	bool bPingOn = false, bPingAttack = false;
	/** How long ago that was (real seconds), for the ring opening out. */
	double PingAge = 0.0;
};
