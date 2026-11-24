#pragma once

#include <vector>

/**
 * The enemy's month near you (release 2; Brian, 24 November 2026: "when the AI nations are doing their turn, show
 * movements around my territories or if my units are attacked so i know whats going on").
 *
 * When the month is ended, the table's eye visits each computer nation's month that came near the player's ground
 * (the rules mark those steps 'near', Rules/near-moves.ts) and draws what happened there as a staff officer would on a
 * situation map: a bold arrow for each foreign army that came up to his border or into his land (deep red for a country
 * at war with him, amber for one at peace, fainter where it went away), a brighter arrow and a battle mark (a ring with
 * two crossed strokes) where his troops were attacked (red where the place was lost, brass where it held), and a
 * dashed cream arrow where his own corps fell back. Arrows grow from their start over a little more than half a second.
 *
 * Plain C++, no engine, so that it can be built and checked alone (Tools/enemy_moves/run.sh). The table
 * (ACampaignMapActor::ShowStepArrows) hands in the arrows' ends in map units and lays what comes out on the ground
 * through its own heights; the controller asks it where to look, and for how long.
 */
namespace TheaterEnemyMoves
{
	struct FPoint
	{
		double X = 0.0;
		double Y = 0.0;
	};

	struct FInk
	{
		unsigned char R = 0, G = 0, B = 0, A = 255;
	};

	enum class EKind : unsigned char
	{
		Move, // a foreign army came up to his ground, into it, or away from it
		Attack, // his troops (or his ground) were attacked
		Fell // his own corps fell back
	};

	struct FArrow
	{
		FPoint From, To;
		bool bFrom = true; // false: where it came from is not known (a landing): only the mark at To is drawn
		EKind Kind = EKind::Move;
		bool bWar = true; // at war with him (red); false: at peace with him (amber)
		bool bAway = false; // (a move) it went away from his ground
		bool bTaken = false; // (an attack) the place was lost
		bool bSiege = false; // (an attack) the field was lost, the town holds
		int Corps = 1;
	};

	/** A ribbon along the ground: Half is its half-width and Lift its height over the ground, both in map units. */
	struct FStroke
	{
		std::vector<FPoint> Points;
		double Half = 1.0;
		double Lift = 0.4;
		FInk Ink;
		bool bClosed = false;
	};

	struct FTriangle
	{
		FPoint A, B, C;
		double Lift = 0.4;
		FInk Ink;
	};

	struct FLayer
	{
		std::vector<FStroke> Strokes;
		std::vector<FTriangle> Triangles;
	};

	/** What to lay, in this order: the dark edges (Under), then the inks over them (Over). */
	struct FDrawing
	{
		FLayer Under, Over;
		void Clear()
		{
			Under = FLayer();
			Over = FLayer();
		}
		bool IsEmpty() const { return Under.Strokes.empty() && Under.Triangles.empty() && Over.Strokes.empty() && Over.Triangles.empty(); }
	};

	/** How far an arrow has grown, Seconds after its step began: 0 to 1 over GrowSeconds, eased. */
	constexpr double GrowSeconds = 0.65;
	double Grown(double Seconds);

	/** Lays the arrows, each grown to Grow (0 nothing, 1 whole), into Out (cleared first). */
	void Lay(const std::vector<FArrow>& Arrows, double Grow, FDrawing& Out);

	/** Where the eye should look so that every point is in view: the middle of them all, and how far the farthest lies from it. */
	struct FFrame
	{
		FPoint Middle;
		double Reach = 0.0;
		bool bValid = false;
	};
	FFrame Frame(const std::vector<FPoint>& Points);

	/**
	 * How far the eye stands back (as the camera's zoom, 0 closest to 1 farthest) to see a frame whose farthest point
	 * lies Reach map units from its middle, for a camera whose distance runs from MinDistance to MaxDistance (world units)
	 * at UnitScale world units a map unit. Never closer than 0.40 nor farther than 0.66 (the watch's own range).
	 */
	double ZoomFor(double Reach, double UnitScale, double MinDistance, double MaxDistance);

	/**
	 * How long the eye stays at a step of the watch, in seconds: long enough to read its sentence. Attacks on his troops
	 * hold it longest (more where a place was lost); a step of moves alone, a little less; a step with raids or ships
	 * going down keeps the time those take to be seen (Raids, Sinkings: the watch's own pause for them, or 0).
	 */
	double Pause(int Attacks, int Lost, int Moves, double Raids, double Sinkings);
}
