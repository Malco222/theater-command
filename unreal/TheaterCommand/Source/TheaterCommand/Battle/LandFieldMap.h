#pragma once

/**
 * The map of the field in the corner of the general's screen (release 2; Brian, 26 November 2026: "its would be
 * helpful to see a small map of the battle on the bottom right to know where everyone is"). Plain C++, no engine, so
 * the cloud tests can hold it (Tools/land_live/map.sh): where the map stands on the screen, how a place on the field
 * falls on it and back, the field's woods, water and buildings as a few runs of small squares, a unit's mark at its
 * frontage and facing, and the outline of what the view sees, cut to the map's edge.
 *
 * The UI director's marks (26 November 2026, 03:51Z, section b): 208 wide at the design size, the field's shape, never
 * taller than 208; its foot 10 above the screen's, its right edge 18 in; everything at the right of the foot stands 10
 * clear of its left edge. The design director's ruling (03:50Z): it shows the player's sight only, what the field
 * itself shows him, and nothing more (LandCommand::General::Tags says which blocks those are; this file draws what it
 * is handed).
 *
 * The field's axes are the rules' own: x east, y north, metres, from its south-west corner. The map stands north up for
 * the attack, whose ground is the field's south; for the defence it is turned half round, so that a player's own ground
 * is always at the foot of his map and his left on the field is the map's left.
 */

#include "Battle/LandFeed.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace LandFieldMap
{
	/** The map's longer side on the screen, its foot's and its right edge's margins, and the room kept clear at its left
	 * (the screen's own units, as its buttons'). */
	constexpr double Side = 208.0, Foot = 10.0, RightIn = 18.0, Clear = 10.0;
	/** A unit's mark is never smaller than this: along its frontage, and through its depth. */
	constexpr double MarkLong = 5.0, MarkDeep = 3.0;

	/** Where the map stands on the screen, and how the field lies in it. */
	struct Frame
	{
		/** Its box on the screen: the top left corner, the width and the height. */
		double X = 0.0, Y = 0.0, W = 0.0, H = 0.0;
		double FieldW = 0.0, FieldH = 0.0;
		/** Turned half round (the defence's map: its own ground, the field's north, at the foot). */
		bool bTurned = false;

		bool Valid() const { return W > 0.0 && H > 0.0 && FieldW > 0.0 && FieldH > 0.0; }
		/** Screen units to a metre (the same across and up: the map keeps the field's shape). */
		double Scale() const { return FieldW > 0.0 ? W / FieldW : 0.0; }
		/** The screen's point of a place on the field. */
		void ToMap(double FieldX, double FieldY, double& OutX, double& OutY) const;
		/** The field's place of a point on the screen, held to the field; false where the point is off the map's box. */
		bool ToField(double MapX, double MapY, double& OutX, double& OutY) const;
		/** Whether a point on the screen is on the map's box. */
		bool Holds(double MapX, double MapY) const { return MapX >= X && MapX <= X + W && MapY >= Y && MapY <= Y + H; }
		/** The screen's left end for everything that stood at the right of the foot: the map's left edge less its clear room. */
		double LeftOf() const { return X - Clear; }
	};

	/** The map's frame on a screen of this size for a field of this size. Not Valid() where either is nothing. */
	Frame Lay(double ScreenW, double ScreenH, double FieldW, double FieldH, bool bTurned);

	/** What a small square of the map is painted: the ground is the map's own plate and is not listed. */
	enum class Paint : std::uint8_t { Ground = 0, Woods = 1, Water = 2, Building = 3 };

	/** A run of small squares along one row of the map, all painted alike. */
	struct Run
	{
		int Row = 0, Col = 0, Count = 0;
		Paint What = Paint::Ground;
	};

	/** The field's woods, water and buildings on the map: rows and columns of small squares from the map's top left
	 * corner (the last row and column may be cut short by the map's edge), and the runs that are not plain ground. */
	struct Painted
	{
		int Cols = 0, Rows = 0;
		double Square = 0.0;
		std::vector<Run> Runs;
	};

	/**
	 * The field painted small. A square over a bridge is plain ground, so each crossing shows as a gap in its river
	 * (the UI director, 26 November 2026, 05:35Z); else it is water where any of the field's cells under it is water or a
	 * blown bridge (a river one cell wide stays whole); else buildings where any is built on; else woods where at least half are woods.
	 * Roads are not drawn. Made once a battle: the ground does not change while it is fought.
	 */
	Painted Paints(const LandFeed::Field& Of, const Frame& In, double Square = 3.0);

	/** A run's box on the screen (cut to the map's box). */
	void RunBox(const Painted& Of, const Frame& In, const Run& One, double& OutX, double& OutY, double& OutW, double& OutH);

	/**
	 * A unit's mark: a stroke along its frontage, as thick as it is deep, at its facing (radians, 0 north, clockwise, as
	 * the rules'), never smaller than MarkLong by MarkDeep. The stroke's two ends and its thickness, on the screen.
	 */
	void Mark(const Frame& In, double FieldX, double FieldY, double Wide, double Deep, double Face,
		double& OutAx, double& OutAy, double& OutBx, double& OutBy, double& OutThick);

	/**
	 * The outline of what the view sees: the ground's corners under the screen's corners (field metres, in order
	 * round it), brought to the map and cut to its box. Closed: the first point is given again at the end. Empty where
	 * fewer than three corners are given or none of it falls on the map.
	 */
	std::vector<std::pair<double, double>> Outline(const Frame& In, const std::vector<std::pair<double, double>>& FieldCorners);
}
