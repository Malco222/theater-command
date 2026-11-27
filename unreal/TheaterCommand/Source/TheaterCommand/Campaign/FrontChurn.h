#pragma once

#include <cstdint>
#include <vector>

/**
 * (Release 2, the campaign map look builder: "fronts that feel alive", piece 1, by the art director's sheet
 * art-director/fronts-alive/fronts-alive-look.md of 27 November 2026, and her read of the first cloud drawings,
 * churn-prototype-read-1127.md, which rules where the two differ.)
 *
 * Churned ground between dug-in lines. Where the armies on both sides of a front have stood dug in and facing each
 * other for months, the ground between them is beaten into a band of broken, cratered earth along the frontier: it
 * shows in the second month, darkens and closes up to the sixth, and heals over a year once the front moves on or a
 * side leaves (the craters green over first; from the fourth month the strip breaks into islands where the craters
 * were, paling, and the last of them is gone at twelve). Weak is broken, never thin: a band never draws narrower than
 * about 1.4 units, and where it is weaker (at first, far from the armies, toward its ends, healing) it is broken into
 * patches two to five units long with gaps between. Strongest where the armies stand; it runs the whole shared border
 * of a dug-in pair and nowhere else. Looks only: no die is thrown and nothing the rules read.
 *
 * It is drawn twice over, each as the map can best carry it:
 *  - into the ground picture (a point every 1.5 map units, under the owners' stain, the fields and the woods), the
 *    band's own earth: its hue, its darkness (a quarter of the ground's lightness at most), its mottling, the woods
 *    thinned in its core and the snow broken there;
 *  - and over the ground, finely, as a veil that only darkens (so that it is right by day and by night): the band's
 *    crisp ragged edge with its tongues, its grain close up, the clusters of craters as darker mottle; and its craters
 *    and flecks of spoil, modelled and lit, small and in clusters.
 * Where the whole band would be under about four pixels wide on the screen it fades out (Shown): the pen's ink
 * carries the front there.
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
	/** The share of it left MonthsSince the front moved on or a side left, over Year months: half at a third of it, a
	 * quarter at two thirds, nothing at its end (the art director's year of twelve: half at four months, a quarter at
	 * eight). Year comes from the rules with their record (churned.ts CHURN.year, the one home of her figure); this table
	 * keeps none of its own, and with none sent nothing heals: it is gone when the front moves on. */
	double Left(double MonthsSince, double Year);
	/** The share of its craters left by then: they green over first, nearly gone by six months. */
	double CratersLeft(double MonthsSince);

	/** One stretch of churned ground as it stands: how strong (Size: how much of it is laid), how dark (Dark), how pitted (Pits), how far it has paled in healing (Pale), and how wide its patches are (Wide: as Size while it grows; healing, the width it had when the front moved on, for its islands are as wide as the band was), each 0 to 1. */
	struct FState
	{
		float Size = 0.0f;
		float Dark = 0.0f;
		float Pits = 0.0f;
		float Pale = 0.0f;
		float Wide = 0.0f;
	};
	/** Growing, after Months dug in and facing. */
	FState Growing(double Months);
	/** Healing, MonthsSince it stood as AtLeaving, over Year months (see Left). */
	FState Healing(const FState& AtLeaving, double MonthsSince, double Year);
	/** Between two states (T 0 to 1): the cross-fade of a month's step. */
	FState Mix(const FState& From, const FState& To, double T);
	/** True when nothing of it shows. */
	inline bool IsNothing(const FState& State) { return State.Size < 0.01f && State.Dark < 0.01f; }

	// ---- the band ---------------------------------------------------------------------------------------------------

	/** The core's width when it first shows (a strength of 0.3) and when it is whole, and its soft edge each side. */
	inline constexpr double CoreFirst = 1.0, CoreWhole = 2.4, EdgeFirst = 0.4, EdgeWhole = 0.6;
	/** Never wider than this in all... */
	inline constexpr double Widest = 4.0;
	/** ...and never narrower than this where it shows (the art director's read of 27 November: weak is broken, never thin). */
	inline constexpr double Narrowest = 1.4;
	/** Its middle strays off the frontier by up to this, over stretches of four to eight units. */
	inline constexpr double Stray = 0.5;
	/** The fine veil's tongues reach out from its edge by up to this... */
	inline constexpr double TongueLongest = 0.8;
	/** ...one at most in each stretch of this along the edge, wholly inside it. */
	inline constexpr double LobeEvery = 1.6;
	/**
	 * How far a point is outside the veil's ragged edge with its tongues (map units, less than nought within): the point
	 * Along its line and Up from the edge as it lies without them, on one side (SideSeed), the tongues at Scale. The
	 * tongues are lobes: leaning, narrowing to a round tip, joined to the edge with no corner, none over some stretches.
	 */
	double LobeEdge(double Along, double Up, uint32_t SideSeed, double Scale);
	/** Whole within about this of where the armies face each other (measured from the point of their shared border nearest the middle between them)... */
	inline constexpr double Reach = 10.0;
	/** ...thinning to half at this and beyond, along a dug-in pair's own border. */
	inline constexpr double Gone = 18.0;
	/** How far from the frontier a point can be touched (half the widest band, its stray and its tongues). */
	inline constexpr double Touch = Widest * 0.5 + Stray + 0.5;

	/**
	 * A point near a front: where it is, and the nearest point of the frontier; how far from the frontier (D, map units, either side) and on which side
	 * (Side: the same, signed, plus to the left of the line's way, when bSided); how far along its line (Along, map
	 * units); how far from where the armies face (Facing); the stretch's strength there once its pairs are joined
	 * (Strength, 0 to 1; below nought, worked out from Facing and the state); and the stretch's state.
	 */
	struct FBandIn
	{
		double X = 0.0, Y = 0.0;
		double FootX = 0.0, FootY = 0.0; // (the nearest point of the frontier, when bFoot: the band's wander is read there, the same right across it)
		bool bFoot = false;
		double D = 1.0e9;
		double Side = 0.0;
		bool bSided = false;
		double Along = 0.0;
		double Facing = 1.0e9;
		double Strength = -1.0;
		FState State;
	};
	/**
	 * What lies on it: the band's soft body (0 to 1, for the ground picture), its core (0 to 1: craters, woods and
	 * snow), its ragged body (0 to 1, for the fine veil: a crisp edge with tongues), the stretch's strength there (0 to
	 * 1), whether this is a patch of it or a gap between (Whole, 0 to 1: weak ground is broken into patches, never drawn
	 * thin), how much of a cluster of craters lies here (Cluster, 0 to 1), where its core ends and where its middle
	 * lies (map units from the frontier, plus to the left).
	 */
	struct FBand
	{
		float Body = 0.0f;
		float Core = 0.0f;
		float Ragged = 0.0f;
		float Strength = 0.0f;
		float Whole = 0.0f;
		float Cluster = 0.0f;
		float HalfCore = 0.0f;
		float Middle = 0.0f;
	};
	FBand Band(const FBandIn& In);

	/** How much of it shows where the whole band would be BandPixels wide on the screen: it fades out under about four, where the pen's ink carries the front. */
	double Shown(double BandPixels);
	/** How much of a crater shows where it is Pixels across on the screen: from three to six it fades in, and smaller it is never drawn (the cluster shows as mottle). */
	double CraterShown(double Pixels);

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
	inline constexpr double Pit[3] = { 43.0, 35.0, 27.0 }; // 2B231B, the shaded third of a large crater's pit: never black
	inline constexpr double PitSmall[3] = { 74.0, 59.0, 43.0 }; // 4A3B2B, a crater's pit
	inline constexpr double RimLit[3] = { 125.0, 103.0, 72.0 }; // 7D6748, a crater's rim on its sunny side
	inline constexpr double SnowDirty[3] = { 150.0, 150.0, 148.0 }; // dirty snow
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

	/** A crater in the band: where, how wide across (Radius, map units), and how strongly it shows (0 to 1: it fades as the ground heals); or, bSpoil, a fleck of dry spoil toward its edge. */
	struct FPit
	{
		double X = 0.0, Y = 0.0, Radius = 0.1;
		float Dark = 1.0f;
		bool bSpoil = false;
	};
	/**
	 * A crater's colours: its rim on the side toward the sun (7D6748, the art director's read: shown on the lit side
	 * only), its pit (4A3B2B: the scars' 2B231B reads as black pepper at this size), and the shaded third of a pit 0.3
	 * across or more (2B231B). The desert, the black earth, the mud and the snow take their own; in the mud months one
	 * pit in six of 0.3 across or more holds a dull sky pool. A fleck of spoil is 8C7650 all over.
	 */
	void PitColours(EGround Kind, ESeason Season, const FPit& Hole, uint8_t Rim[3], uint8_t Bottom[3], uint8_t Shade[3]);

	// ---- the lines ----------------------------------------------------------------------------------------------------

	/** A line of the front where churned ground lies: its points in order, and at each how far it is from where the armies face, and the state of its stretch; once Strengthen has been through it, the strength at each point and how far along the line each lies. */
	struct FLine
	{
		std::vector<double> X, Y;
		std::vector<float> Facing;
		std::vector<FState> State;
		std::vector<float> Strength;
		std::vector<double> Run;
		bool bClosed = false;
	};

	/**
	 * The strength at each point of a line: its state's size, tapered away from where its armies face; where two
	 * stretches meet (two pairs of provinces, or a pair and its neighbour), they join at the stronger, blended over
	 * about Over map units, with no notch; the state too is blended over the same. Call it before EaseEnds.
	 */
	void Strengthen(FLine& Line, double Over = 3.0);

	/**
	 * The craters along a line: in clusters with bare stretches between (never an even pepper), sizes about three to
	 * one, none in the soft edge, about a third of the core pitted where the band is whole, the same each time for the
	 * same line. (Their colours: PitColours.)
	 */
	void Pits(const FLine& Line, std::vector<FPit>& Out);

	/** Brings an open line's band to nothing over its last Over map units at each end: weaker toward its ends, it breaks into patches and dies, never a square stump nor a round cap. */
	void EaseEnds(FLine& Line, double Over = 3.5);

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
	/** The band's input at a grid point once measured: its place, distance and side, how far along, how far from where the armies face, its strength and its stretch's state (between the stretch's two ends). False where no line is near. */
	bool At(const std::vector<FLine>& Lines, const FNearest& Nearest, int Index, double X, double Y, FBandIn& In);
}
