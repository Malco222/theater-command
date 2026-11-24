#pragma once

#include <vector>

/**
 * The front you can read (release 2; item 4 of the campaign map plan Brian approved): the front line drawn as a
 * map-maker's pen draws it on a staff map. Release 1 laid a short crayon ribbon along each stretch of frontier between
 * two countries at war, every stretch for itself; here those stretches are first found to be one line (chained end to
 * end, through the meetings of three borders, and on round an enclave), thinned of the border's small jags, rounded at
 * its corners within a unit of the border, and then drawn once as dark ink: a slight swell and thinning as a nib
 * gives, a soft shadow under it, and on the player's own fronts small teeth on the side of the enemy.
 *
 * Plain C++, no engine. The table (ACampaignMapActor::LayPenFront) hands in the stretches of border it found once and
 * who holds what, and lays what comes out on the ground through its own heights. It is laid again only when ground
 * changes hands or a war begins or ends, never in a frame for its own sake. Tools/front/run.sh builds this file alone
 * with the compiler's strictest warnings and checks it against small made-up maps.
 *
 * Also here, for the same reason (so that it can be checked without the engine): the clock of ink that spreads, the
 * share of a province the new colour covers at each moment of its run.
 */
namespace TheaterFront
{
	struct FPoint
	{
		double X = 0.0;
		double Y = 0.0;
	};

	/** A stretch of line two provinces share, as the table found it: from A to B, Here on one side and There on the other; Side points out of Here into There. */
	struct FBorderPiece
	{
		FPoint A, B, Side;
		int Here = -1, There = -1;
	};

	/** Who holds what, and who is at war with whom, all as numbers: a holder is a number from 0 to Holders - 1. */
	struct FSides
	{
		std::vector<int> HolderOf; // by province: its holder, or -1 for nobody
		std::vector<unsigned char> Foe; // by holder: 1 where the player is at war with it
		std::vector<unsigned char> War; // holder by holder: 1 where the two are at war
		int Holders = 0;

		/** Clears it for so many provinces and so many holders: nobody holds anything and nobody is at war. */
		void Reset(int ProvinceCount, int HolderCount);
		void SetWar(int One, int Other);
		bool AtWar(int One, int Other) const;
	};

	/** Whose war a stretch of the front is: two others' (a quieter line), or the player's own (teeth toward his enemy). */
	enum class EKind : unsigned char
	{
		Theirs,
		Mine
	};

	/** One stretch of the front: a border piece between two holders at war. */
	struct FFrontPiece
	{
		FPoint A, B;
		int Low = -1, High = -1; // the provinces either side, the lower number first: one pair's border is one run of the line
		EKind Kind = EKind::Theirs;
		int Enemy = 0; // (the player's war) +1 where his enemy lies to the left going from A to B, -1 to the right; 0 in others' wars
	};

	/** A line of the front, end to end (or round and back to its start). */
	struct FChain
	{
		std::vector<FPoint> Points;
		std::vector<double> Along; // where each point stands on the runs it was drawn from: the run's number, and how far along it (0 to 1)
		std::vector<unsigned char> Keep; // 0 free; 1 kept when the line is thinned (where one pair's border meets the next); 2 kept through the rounding too (an end)
		std::vector<double> RunLength; // each run's length, as the border has it
		double Span = 0.0; // (a closed line) the Along its first point has once the line has gone all the way round
		bool bClosed = false;
		EKind Kind = EKind::Theirs; // (the player's lines are turned so that his enemy is always to the left)
		int StartNode = -1, EndNode = -1; // the welded points the two ends stand on (-1 for a closed line)
		int StartOnto = -1, EndOnto = -1; // an end that stops where another line runs on through: that line, which the end is brought onto once both are rounded
	};

	/** How the borders are made into lines. Map units throughout. */
	struct FRules
	{
		double Weld = 0.01; // two ends closer than this are one point
		double Gap = 1.0; // two loose ends closer than this are joined (the table leaves out stretches under 0.05 long, and borders of the wider chart overlap by up to about a unit where their corners do not match)
		double Straight = -0.2; // where three or more lines meet, two go on as one where they turn by less than about a right angle and a little (the cosine of the turn above this)
		double Thin = 0.2; // how far a point of the border may stand off the thinned line
		double Cut = 0.8; // the most a corner is cut back along each side in one pass of the rounding (a right-angled corner comes in by about two thirds of a unit, about two points of the screen at working height)
		int Passes = 3; // passes of the rounding
		double Step = 1.0; // the longest stretch between two points of the finished line, so that it follows the ground's heights
	};

	/** Work arrays kept from one laying to the next, so that a laying allocates nothing once they have grown. */
	struct FScratch
	{
		std::vector<FPoint> Nodes;
		std::vector<int> NodeA, NodeB, Degree, Offsets, Incident, Partner, Remap, Loose, Through, RunOf;
		std::vector<unsigned char> Used;
		std::vector<int> Walk;
		std::vector<FPoint> Points;
		std::vector<double> Along;
		std::vector<unsigned char> Keep;
	};

	/** The stretches of border that are front: between two holders at war. */
	void PickFront(const std::vector<FBorderPiece>& Pieces, const FSides& Sides, std::vector<FFrontPiece>& Out);

	/** The stretches of front chained into lines, with their runs measured and their ends noted; nothing thinned or rounded yet. */
	void ChainFront(const std::vector<FFrontPiece>& Front, const FRules& Rules, FScratch& Work, std::vector<FChain>& Chains);

	/** (The art director's round 34, as the frontier lines have it.) A point where two short stretches turn back on each other by more than about 115 degrees is left out. */
	void Despike(FChain& Chain);

	/** Thins a line: within each run, the fewest points that keep it within Tolerance of the border (Douglas and Peucker). */
	void Thin(FChain& Chain, double Tolerance, FScratch& Work);

	/** Rounds a line's corners (Chaikin's cutting, each cut no longer than Cut): its ends stay where they are. */
	void Round(FChain& Chain, int Passes, double Cut, FScratch& Work);

	/** Puts points into every stretch longer than Step, evenly. */
	void Divide(FChain& Chain, double Step, FScratch& Work);

	/** Brings each end that stops where another line runs on through onto that line, as it is now rounded. */
	void MeetLines(std::vector<FChain>& Chains);

	/** All of it, from the borders and the sides to finished lines. Front and Chains are filled, their arrays kept. */
	void DrawFront(const std::vector<FBorderPiece>& Pieces, const FSides& Sides, const FRules& Rules, FScratch& Work, std::vector<FFrontPiece>& Front, std::vector<FChain>& Chains);

	/** A slow, even waver in the nib, -1 to 1, by where on the map: the same at the same place whenever the line is drawn again. */
	double Waver(double X, double Y);

	/** A point of the drawn line: where on the map, how high over the ground, and its colour as a picture file keeps it (sRGB) with its opacity. */
	struct FInkVertex
	{
		double X = 0.0, Y = 0.0, Lift = 0.0;
		unsigned char R = 0, G = 0, B = 0, A = 255;
	};

	struct FInkMesh
	{
		std::vector<FInkVertex> Vertices;
		std::vector<int> Triangles; // three to a triangle, wound as they come: the table turns each to face up

		void Clear();
		int Add(double X, double Y, double Lift, const unsigned char* Rgb, unsigned char Alpha);
		void Triangle(int One, int Two, int Three);
	};

	/** How the pen draws: map units at the working weight. [0] others' fronts, [1] the player's. */
	struct FPenLook
	{
		double Half[2] = { 0.26, 0.38 }; // half the pen's width (the frontier's own line is 0.2: at working height a unit is about 3.6 points of the screen)
		unsigned char Ink[2][3] = { { 0x2A, 0x21, 0x18 }, { 0x2A, 0x21, 0x18 } }; // the kit's ink-900: dark brown ink, never black
		double WaverShare = 0.12; // the nib swells and thins by this share of its width
		double Heavier = 2.7; // the staff map's weight, seen from high up: everything this much larger
		double CapReach = 0.8, CapNarrow = 0.45; // a loose end runs on this many half widths, narrowing to this share
		double TickEvery = 2.6; // the player's fronts: about this far between teeth, each run of border sharing them out evenly
		double TickBase = 0.32, TickReach = 0.65; // half a tooth's foot along the line, and how far it stands out past the ink
		double TickClear = 0.8; // no tooth nearer a loose end than this
		double TickEveryFar = 2.0; // (the staff map's weight: the teeth this many times further apart)
		double HaloInner = 1.0, HaloOuter = 3.2; // the shadow: whole under the ink, gone this many half widths out
		unsigned char HaloAlpha[2] = { 46, 77 };
		unsigned char HaloInk[3] = { 0x12, 0x0C, 0x08 };
		double ShadowX = 0.0, ShadowY = 0.0; // where the shadow falls from the line, at the working weight
		double InkLift = 0.2, HaloLift = 0.15; // over the frontier's own line (0.13), under every piece and name
	};

	/** Draws the lines in one weight (0 working, 1 the staff map's): the ink, its teeth, and the shadow under it. Both meshes are cleared first. */
	void LayPen(const std::vector<FChain>& Chains, const FPenLook& Look, int Weight, FInkMesh& Ink, FInkMesh& Halo);

	/** Where ink that spreads stands, Second seconds into a run of Length seconds. */
	struct FInkPhase
	{
		float Share = 0.0f; // the share of the province's depth the new colour has crossed: quick at first, slowing (1 - (1 - t) squared)
		float Late = 0.0f; // how far the ground beside, the frontier band and the frontier's line have come to their new place (the last three tenths)
		float Dry = 0.0f; // how far the wet rim behind the front has dried (in three tenths of a second once the run is over)
		bool bBorders = false; // the frontier's line is to be drawn in its new place now
		bool bOver = false; // the run is over: the finished paint is laid
	};
	FInkPhase InkPhase(double Second, double Length);
	/** The month's replay shows each step for Shown seconds, and the next step that moves ground ends any ink still
	 *  running. A run of Length seconds begun in a step is fitted into it: no longer than the step is shown, less one
	 *  step of the ink's clock (it is laid eight times a second), so that the colour has crossed and the frontier's band
	 *  and line have come over before it is ended. Never under 0.4 s, and never longer than Length. */
	double InkFitted(double Length, double Shown);
}

/** What the table keeps of its drawn front between one laying and the next (ACampaignMapActor::LayPenFront). */
struct FPenFront
{
	std::vector<TheaterFront::FBorderPiece> Pieces; // the table's shared edges, once
	bool bPiecesKnown = false;
	TheaterFront::FSides Sides;
	TheaterFront::FRules Rules;
	TheaterFront::FScratch Work;
	std::vector<TheaterFront::FFrontPiece> Front;
	std::vector<TheaterFront::FChain> Chains;
	TheaterFront::FInkMesh Ink, Halo;
	int Mine = 0, Theirs = 0, Closed = 0; // (for the log) the player's lines, others', and those closed on themselves
};
