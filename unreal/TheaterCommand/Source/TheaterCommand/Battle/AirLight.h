#pragma once

#include <cstdint>
#include <vector>

/**
 * An aircraft's light shape (AS1 of the production lead's battle-scale plan, 22 November: "B-17 middle shape (~1,500
 * triangles); instanced middle and far tiers"). Past AAirBattle's LightFromM an aircraft is drawn as a copy of this
 * shape, not of its far one: a B-17's far part is 7,122 triangles, a Kate's and a Val's 11,000, and at a kilometre and a
 * half the whole machine is some twenty pixels across.
 *
 * The far shape is taken down to at most the triangles asked by joining the two ends of an edge, the cheapest first. An
 * edge costs the sum of the squared distances from the joined point to the planes of the triangles that met at either
 * end (the quadric error of Garland and Heckbert, 1997), each plane weighed by its triangle's area. An edge on an open
 * rim of the shape, or between triangles of two paints, adds a plane standing upright on it, so the outline and the
 * paint's lines stay where they were. A join is refused where it would turn a triangle over or tear the surface (the two
 * ends sharing more neighbours than triangles).
 *
 * Engine-free: the cloud test runs it on every aircraft in Content/Models (Tools/air_show/air_light_test.cpp).
 */
namespace AirLight
{
	/** Past this distance from the eye (through the battle's own 55 degree lens; a narrower lens moves it out as it
	 * enlarges) an aircraft is drawn as its light shape. A pixel there is 1.45 m, a B-17 some 22 pixels across. */
	constexpr double LightFromM = 1500.0;

	/** The triangles an aircraft's light shape may have, from its far part's: a quarter of them, between 500 and 1,500 (2,000 for a far part over 16,000);
	 * none (0: its far part is drawn all the way out) for a far part of 1,200 or fewer, as a fighter's mostly are. */
	constexpr int LightMost(int FarTriangles)
	{
		// (a far part of more than 16,000 triangles, 2,000: the finer B-17's of 20,514 (Content/Models/B17Fine.json, the
		// Battle rules card's "Finer B-17") taken down to 1,500 stood 0.89 m, 0.61 of a pixel, off her far part at 1.5 km,
		// over the half pixel every aircraft is held to (air_light_test); at 2,000, 0.55 m, 0.38 of a pixel. No other
		// model's far part is near it: the largest, the Nell's, is 11,264, so every other light shape is as it was.)
		return FarTriangles > 16000 ? 2000 : FarTriangles <= 1200 ? 0 : (FarTriangles / 4 < 500 ? 500 : (FarTriangles / 4 > 1500 ? 1500 : FarTriangles / 4));
	}

	struct Shape
	{
		/** x, y, z a point. Points at the same place are joined first (a model's hard edges and paint lines are drawn
		 * as two points at one place). */
		std::vector<double> Points;
		/** Three points a triangle. */
		std::vector<int> Triangles;
		/** A colour a point (any number standing for its paint), or empty: all one paint. A triangle takes the colour
		 * two or three of its corners have, else its first corner's. */
		std::vector<uint32_t> Tones;
	};

	/** The light shape: its points, each with its facing (smoothed across edges that bend less than the crease asked,
	 * hard across the rest) and its triangle's colour; three points a triangle. */
	struct Made
	{
		std::vector<double> Points;
		std::vector<double> Normals;
		std::vector<uint32_t> Tones;
		std::vector<int> Triangles;
		/** The triangles given (once their points were joined and any with two corners at one place left out), and the
		 * triangles kept. */
		int From = 0;
		int Kept = 0;
	};

	/** The shape taken down to at most MostTriangles (it is given back whole, with its facings, if it has no more). */
	Made Lighten(const Shape& In, int MostTriangles, double CreaseDegrees = 50.0);

	/** How far apart two shapes are: the farthest any of A's points lies from B's surface, and the mean of those. */
	struct Apart
	{
		double Most = 0.0;
		double Mean = 0.0;
	};
	Apart Distance(const std::vector<double>& APoints, const std::vector<double>& BPoints, const std::vector<int>& BTriangles);
}
