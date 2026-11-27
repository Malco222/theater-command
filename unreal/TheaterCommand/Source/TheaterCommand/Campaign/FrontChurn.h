#pragma once

#include <cstdint>
#include <vector>

/**
 * (Release 2, the campaign map look builder: "fronts that feel alive", piece 1, by the art director's sheet
 * art-director/fronts-alive/fronts-alive-look.md of 27 November 2026.)
 *
 * Churned ground between dug-in lines. Where the armies on both sides of a front have stood dug in and facing each
 * other for months, the ground between them is beaten into a band of broken, cratered earth along the frontier: it
 * shows in the second month, widens and darkens to the sixth, and heals over a year once the front moves on or a
 * side leaves (the craters green over first, the band narrows from its edges inward, and the last of it is a paler,
 * rougher strip). Strongest where the armies stand; nothing along the rest of the frontier. Looks only: no die is
 * thrown and nothing the rules read.
 *
 * It is drawn twice over, each as the map can best carry it:
 *  - into the ground picture (a point every 1.5 map units, under the owners' stain, the fields and the woods), the
 *    band's own earth: its hue, its darkness (a quarter of the ground's lightness at most), its mottling, the woods
 *    thinned in its core and the snow broken there; this is what reads from high up;
 *  - and over the ground, finely, as a veil that only darkens (so that it is right by day and by night): the band's
 *    ragged edge with its tongues, its mottling at close height, and its craters, small and in clusters.
 *
 * Plain arithmetic with no engine in it, so that the cloud can check it (Tools/front_churn/run.sh). The map
 * (CampaignFrontChurn.cpp) finds the fronts, their months and where the armies stand, lays the lines, and paints and
 * models what these give. Measures are in map units (a house on the table is about 2.1, one unit about 2.8 km);
 * colours are 0 to 255 as the eye sees them.
 */
namespace TheaterChurn
{
	// ---- the months ----------------------------------------------------------------------------------------------

	/** How far the churned ground has grown after Months of both sides dug in and facing: nothing in the first month, then 0.3, 0.5, 0.7, 0.85, whole at six (the art director's table), evenly between. */
	double Grown(double Months);
	/** The share of it left MonthsSince the front moved on or a side left: half at four months, a quarter at eight, nothing at twelve. */
	double Left(double MonthsSince);
	/** The share of its craters left by then: they green over first, nearly gone by six months. */
	double CratersLeft(double MonthsSince);

	/** One stretch of churned ground as it stands: how wide (Size), how dark (Dark), how pitted (Pits), and how far it has paled in healing (Pale), each 0 to 1. */
	struct FState
	{
		float Size = 0.0f;
		float Dark = 0.0f;
		float Pits = 0.0f;
		float Pale = 0.0f;
	};
	/** Growing, after Months dug in and facing. */
	FState Growing(double Months);
	/** Healing, MonthsSince it stood as AtLeaving. */
	FState Healing(const FState& AtLeaving, double MonthsSince);
	/** Between two states (T 0 to 1): the cross-fade of a month's step. */
	FState Mix(const FState& From, const FState& To, double T);
	/** True when nothing of it shows. */
	inline bool IsNothing(const FState& State) { return State.Size < 0.01f && State.Dark < 0.01f; }

	// ---- the band ---------------------------------------------------------------------------------------------------

	/** The core's width when it first shows (a strength of 0.3) and when it is whole, and its soft edge each side. */
	inline constexpr double CoreFirst = 1.0, CoreWhole = 2.4, EdgeFirst = 0.4, EdgeWhole = 0.6;
	/** Never wider than this in all. */
	inline constexpr double Widest = 4.0;
	/** Whole within about this of where the armies face each other (measured from the point of their shared border nearest the middle between them)... */
	inline constexpr double Reach = 10.0;
	/** ...and nothing at all beyond this. */
	inline constexpr double Gone = 18.0;
	/** How far from the frontier a point can be touched (half the widest band, and the tongues). */
	inline constexpr double Touch = Widest * 0.5 + 0.4;

	/** A point near a front: where it is, how far from the frontier (D, map units, either side), how far from where the armies face (Facing), and the stretch's state there. */
	struct FBandIn
	{
		double X = 0.0, Y = 0.0;
		double D = 1.0e9;
		double Facing = 1.0e9;
		FState State;
	};
	/** What lies on it: the band's soft body (0 to 1, for the ground picture), its core (0 to 1: craters, woods and snow), its ragged body (0 to 1, for the fine veil, with tongues), and the stretch's strength there after the taper away from the armies (0 to 1). */
	struct FBand
	{
		float Body = 0.0f;
		float Core = 0.0f;
		float Ragged = 0.0f;
		float Strength = 0.0f;
		float HalfCore = 0.0f; // (map units: where the core ends, after its wander)
	};
	FBand Band(const FBandIn& In);

	// ---- the ground picture -----------------------------------------------------------------------------------------

	/** The ground the band lies on: most of Europe, the black earth of the steppe, or the desert. */
	enum class EGround : uint8_t
	{
		Common,
		BlackEarth,
		Desert
	};
	/** The season on it: dry, the mud months, or snow lying. */
	enum class ESeason : uint8_t
	{
		Dry,
		Mud,
		Snow
	};

	/** The art director's colours (the scars' own earth). */
	inline constexpr double Churned[3] = { 106.0, 85.0, 61.0 }; // 6A553D, the band's body
	inline constexpr double Spoil[3] = { 140.0, 118.0, 80.0 }; // 8C7650, dry spoil flecks along the edges
	inline constexpr double Pit[3] = { 43.0, 35.0, 27.0 }; // 2B231B, the craters' pits: never black
	inline constexpr double BlackEarthChurned[3] = { 70.0, 58.0, 46.0 };
	inline constexpr double DesertChurned[3] = { 160.0, 134.0, 98.0 };
	inline constexpr double DesertPit[3] = { 96.0, 78.0, 58.0 };
	inline constexpr double MudChurned[3] = { 78.0, 62.0, 46.0 };
	inline constexpr double MudWettest[3] = { 62.0, 50.0, 38.0 };
	/** The most the band takes off the ground's lightness (CIE L*), the picture and the veil together: a quarter... */
	inline constexpr double MostDarker = 0.25;
	/** ...of which the ground picture takes this much, and the fine veil the rest (VeilMost of the light at most). */
	inline constexpr double PictureDarker = 0.15;
	inline constexpr double VeilMost = 0.22;
	/** A dull pool of sky in a pit, in the mud months. */
	inline constexpr double SkyPool[3] = { 128.0, 138.0, 148.0 };

	/** A point of the ground picture with the band on it: its colour (In, Out), the woods' share (Wooded, 0 to 255, the picture's alpha) and the snow's (Snow, 0 to 255). */
	void PaintCell(const uint8_t In[3], const FBand& Band, const FState& State, EGround Ground, ESeason Season, double X, double Y, uint8_t Out[3]);
	/** The woods thin out in the core and go splintered; the soft edge keeps them. */
	uint8_t Wooded(uint8_t In, const FBand& Band, const FState& State);
	/** The band breaks the snow: dark broken earth and dirty snow in the core, clean snow closing in at the edge. */
	uint8_t Snow(uint8_t In, const FBand& Band, const FState& State);

	// ---- the fine veil ----------------------------------------------------------------------------------------------

	/** How much the fine veil darkens a point (0 to VeilMost of its light): the band's ragged edge with its tongues, and its mottling close up. */
	float Veil(const FBand& Band, const FState& State, double X, double Y);

	/** A crater in the band: where, how wide across (Radius, map units), and how strongly it shows (0 to 1: it fades as the ground heals). */
	struct FPit
	{
		double X = 0.0, Y = 0.0, Radius = 0.1;
		float Dark = 1.0f;
	};
	/** A crater's colours: its rim (the lit side's, the band's earth lifted toward the spoil) and its pit (2B231B, never black; the desert's own; in the mud months a few dull sky pools). */
	void PitColours(EGround Kind, ESeason Season, const FPit& Hole, uint8_t Rim[3], uint8_t Bottom[3]);

	// ---- the lines ----------------------------------------------------------------------------------------------------

	/** A line of the front where churned ground lies: its points in order, and at each how far it is from where the armies face, and the state of its stretch. */
	struct FLine
	{
		std::vector<double> X, Y;
		std::vector<float> Facing;
		std::vector<FState> State;
		bool bClosed = false;
	};

	/**
	 * The craters along a line: in clusters with bare stretches between (never an even pepper), sizes about three to
	 * one, none in the soft edge, about a third of the core pitted where the band is whole, the same each time for the
	 * same line. (Their colours: PitColours.)
	 */
	void Pits(const FLine& Line, std::vector<FPit>& Out);

	/** Brings an open line's band to nothing over its last Over map units at each end, so that it never stops in a round cap. */
	void EaseEnds(FLine& Line, double Over = 4.0);

	/**
	 * Distances to the lines from a grid's points near them. Dist and Near start the size of the grid (NX by NY, row by
	 * row, the first point at OriginX, OriginY, Cell apart) holding Far and -1; each point within Touch of a line's
	 * stretch comes back holding its distance to the nearest, and that stretch's place (its line's number times a
	 * million and its first point's number, plus the share along it in At). The engine and the drawings use this alike.
	 */
	struct FNearest
	{
		std::vector<float> Dist;
		std::vector<int> Line;
		std::vector<int> Point;
		std::vector<float> At;
	};
	void Measure(const std::vector<FLine>& Lines, double OriginX, double OriginY, double Cell, int NX, int NY, FNearest& Out);
	/** The band's input at a grid point once measured: its place, distance, how far from where the armies face, and its stretch's state (between the stretch's two ends). False where no line is near. */
	bool At(const std::vector<FLine>& Lines, const FNearest& Nearest, int Index, double X, double Y, FBandIn& In);
}
