#pragma once

#include <cstdint>
#include <vector>

/**
 * THE SEA'S FACE (Brian, 21 November 2026 05:53Z: "make the water more detailed in the pearl harbor air battle
 * scene"; the art director's notes on the stand-in frames, 07:20Z). Two pictures the air battle's moving sea
 * (M_StageSea) reads, worked out in plain C++ so the cloud test makes exactly what the game will:
 *  - the shore picture: where the water lies shut in by the land and reef (a harbour, a loch, a lagoon: calm), how
 *    far the trade wind has blown over open water to reach it (the whitecaps grow with it, so a lee shore lies quiet
 *    for a few kilometres), how open it lies to the ocean's swell (the surf on an open reef edge), and how far it is
 *    from the shore's edge (the surf line, the harbour's colour);
 *  - the whitecaps' picture: each whitecap a short bright streak across the wind with a see-through tail of foam
 *    behind it, lying in rows along the crests, each breaking once in its cycle.
 */
namespace SeaFace
{
	/** A part of the stage the sea breaks on (its land, its reef, its works): points in stage metres, x east and y
	 * north, and its triangles, three indices each. */
	struct Part
	{
		std::vector<double> X, Y;
		std::vector<int> Triangles;
	};

	/** Metres either side of the shore's edge the shore picture's red holds (0.5 at the edge). */
	constexpr double EdgeReach = 500.0;

	/**
	 * The shore picture: Size pixels a side over the square from West to West + Span east and South to South + Span
	 * north (stage metres; row 0 at the south, column 0 at the west), four numbers a pixel, 0 to 255:
	 *  red   how far the water lies from the edge of the land or reef: 0.5 at the edge, out to sea toward 1 at
	 *        EdgeReach, inland or onto the reef toward 0;
	 *  green how much of the open sea's whitecaps the trade wind raises there: by its fetch, the open water it has
	 *        blown over (a few kilometres under a lee shore: few; past about 6 km: all), 0 where the water is shut in;
	 *  blue  how open the water lies to the ocean (the surf on a reef or shore's edge): 0 in a harbour, about 1 off an
	 *        open coast or reef;
	 *  alpha how closely land and reef shut the water in: 1 in a harbour, a loch or on the reef and its lagoon, 0 at sea.
	 * The square is the parts' bounds and 10 km of sea round them. With no parts, one pixel of open sea.
	 */
	struct Shore
	{
		int Size = 1;
		double West = -1.0, South = -1.0, Span = 2.0;
		std::vector<uint8_t> Pixels{ 255, 255, 255, 0 };
	};
	/** WindFrom: the trade wind's heading, degrees clockwise from north, that it blows from (070 at Oahu). */
	Shore MakeShore(const std::vector<Part>& Parts, double WindFrom, int Size = 1024);

	/**
	 * The whitecaps' picture, Size pixels a side, repeating, laid with u along the wind (downwind) and v across it,
	 * TileMetres to the picture. Four numbers a pixel, 0 to 255:
	 *  red   a whitecap's foam: its head, a streak across the wind 2 to 4 times as long as it is deep, and its tail
	 *        behind it upwind, where the crest that broke has run on and left it, 2 to 4 times the head's length;
	 *        feathered over half a metre, ragged at the tail; 0 between whitecaps;
	 *  green when in its cycle that whitecap breaks, 0 to 1, one value over each;
	 *  blue  how far along it the breaking crest has run when it reaches a point: 0 at the tail's upwind end, 1 at the
	 *        head's front;
	 *  alpha a fine mottle, for the tail's see-through foam.
	 * Most heads are under 2 m across the wind, the largest 3 to 5 m; they lie in rows across the wind about 20 m apart
	 * (the chop's crests), gathered in stretches along each row; no two touch (with two pixels between), and each one's
	 * green and blue are carried about a metre out round it, so a read between pixels never mixes two whitecaps'
	 * times. Count: up to 300 to 160 m (fewer where the picture has no room), of which the sea breaks some.
	 */
	std::vector<uint8_t> MakeFoam(int Size, double TileMetres, int Count = 300);
}
