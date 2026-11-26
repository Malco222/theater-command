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
		/** Its real frontage and depth's four corners on the screen, for its ground washed in its side's colour from high
		 * up (every unit he can see, while PrintInk is above nothing). */
		bool bPrint = false;
		double Print[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	};
	/** How strongly each unit's ground is washed (0 to 0.25): full from 300 m up, gone by 150 m (the art director, 22
	 * November 18:29Z: "the pin's stem ends over empty ground"). */
	double PrintInk = 0.0;
	std::vector<Mark> Marks;

	/**
	 * A corps coming up from next door (neighbours join the fight, release 2), its chip on the screen this frame: over the
	 * place on the field's edge where it will walk on when that is in sight (bThere), else pinned just inside the screen's
	 * edge on the way to it, with that way (a unit vector on the screen) for its arrow.
	 */
	struct Comer
	{
		LandCommand::Neighbour What;
		double X = 0.0, Y = 0.0;
		bool bThere = false;
		double WayX = 0.0, WayY = 1.0;
	};
	std::vector<Comer> Comers;
	/** His despatch riders on the screen this frame (the rider ahead, release 2): the corps each rides for, where he is (on
	 * the screen), and the despatch read out by a click on him (his corps, -1 none) and when (real seconds). */
	struct RiderMark
	{
		int Corps = 0;
		double X = 0.0, Y = 0.0;
		std::string Says;
	};
	std::vector<RiderMark> RiderMarks;
	int RiderRead = -1;
	double RiderReadAt = -10.0;

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
	/** Several carried as one in the setup step (release 2, drawing up): the others' grounds' corners where they would
	 * be set down, eight numbers a block as Corners (the one he holds is Corners'); empty: one carried alone. */
	std::vector<double> CarryMore;
	/** The map of the field (release 2): the ground under the screen's four corners this frame, field metres, x then y,
	 * round the screen from its bottom left (a corner that looks over the horizon is set far off along its way); empty
	 * with the map's row off. */
	std::vector<double> MapView;
	/** The formation his units were last drawn up in (release 2: "line", "two", "wedge", "column"; empty: none yet, or
	 * he has dragged a unit by hand since): its button is lit. */
	std::string DrawnUp;
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

	/**
	 * The spotter's mark (release 2, -TheaterLandSpotter; Brian, 24 November 20:38Z): while he looks down on the field
	 * (the top view, or his own tilt as steep), once the battle is under way (not while he sets his army out), a ring
	 * on the ground round each of his fighting blocks: a hidden enemy inside it is seen (LandSight::Finds, the rules'
	 * own distances). Every block's ring for hidden men on foot; a block in hand's for guns and teams and for tanks
	 * and motorised blocks too, each with its word. A stroke is a run of the ring on the screen (a ring part off the
	 * screen or behind the eye is several, a dashed one many).
	 */
	struct SightStroke
	{
		int Block = -1;
		/** 0 hidden men on foot, 1 guns and teams (machine guns and mortars), 2 tanks and motorised blocks. */
		int Kind = 0;
		bool bChosen = false;
		std::vector<std::pair<double, double>> Points;
	};
	struct SightWord
	{
		int Kind = 0;
		/** The rules' distance, metres, and where the word hangs on the screen (by the ring's right side). */
		double Metres = 0.0, X = 0.0, Y = 0.0;
	};
	bool bSights = false;
	std::vector<SightStroke> Sights;
	std::vector<SightWord> SightWords;

	/** The pointer, the tag or block under it (-1 none) and what a click would do there ("GO HERE", "ATTACK", ...). */
	double PointerX = -1.0, PointerY = -1.0;
	int Hover = -1;
	std::string Would;
	/** An order armed by a button: the next click goes at the double ("double"), fires on what it is on ("fire"), cuts a
	 * gap in the hedge there with the cutters in hand ("gap") or lays smoke there, or along a line dragged ("smoke"). */
	std::string Armed;
	/** Hedge cutters armed, the pointer near a hedge: where on it the gap would be cut (on the screen). */
	bool bGapAt = false;
	double GapAtX = 0.0, GapAtY = 0.0;
	/** Smoke armed and a line dragged: its two ends on the screen. */
	bool bSmoke = false;
	double SmokeEnds[4] = { 0, 0, 0, 0 };
	/** Where he last gave an order (on the field, and on the screen this frame) and when (real seconds), for the mark
	 * that answers his click at once. */
	double PingFieldX = 0.0, PingFieldY = 0.0, PingX = 0.0, PingY = 0.0, PingAt = -10.0;
	bool bPingOn = false, bPingAttack = false;
	/** How long ago that was (real seconds), for the ring opening out. */
	double PingAge = 0.0;
	/** A block whose plate is held up with no pointer over it (-1 none): the walk pressed in code (-TheaterLandScript)
	 * has no hand on the mouse to hover with. */
	int ShowPlate = -1;
};
