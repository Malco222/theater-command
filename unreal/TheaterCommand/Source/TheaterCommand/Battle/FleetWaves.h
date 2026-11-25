#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

/**
 * THE FLEET SEA'S WAVES (the release 2 ocean plan, step 2: the wave core; the standing bar, an ocean as good as the
 * B-17). One set of waves, drawn and ridden on one sea clock: the wind's own sea, made from the battle's wind with the
 * show's own height (SeaShow's High, 0.021 U^2 up to its Raised 13.8 m/s), as five bands of two trains each at plus and
 * minus the band's spread off downwind, with a swell where the field gives one, and short trains drawn per pixel only.
 * Plain C++20, no engine: nothing is wired yet. The chip draws the same sum from FleetWavesGpu.inl, which this file also
 * compiles in float through its own stand-ins for the shader's types (its "GPU twin", FleetWaves::Gpu), so the cloud holds
 * the chip's text to the core (Tools/sea_waves/sea_waves_test.cpp).
 *
 * LOOKS ONLY. The waves draw on their own stream (Mix(Seed*31+17+i), SplitMix64's), never the show's Dice or Roll, and
 * nothing the show decides reads them. The same seed and wind give the same sea, whatever is asked first.
 *
 * Places are board metres in SeaShow's Vec frame (X east, Y north, Z up); headings radians clockwise from north (Ahead
 * is (sin, cos)); a train's phase is Kx X + Ky Y - W T + Phase0, and it travels along (Kx, Ky). The stage swaps X and Y
 * (ASeaBattle::World) and works in its own units: StageParams does both, once, and the .inl works in the stage's frame.
 * Numbers: [P] published (Pierson-Moskowitz, Mitsuyasu, JONSWAP, Cox-Munk, Monahan); [SHOW] the show's, which these
 * must agree with; [PLAN] the release 2 ocean plan's; [EST] an estimate, to be refitted on the Mac.
 */
namespace FleetWaves
{
	inline constexpr double Pi = 3.14159265358979323846;
	inline constexpr double TwoPi = 2.0 * Pi;
	inline constexpr double Gee = 9.81;
	inline constexpr int MostDrawn = 10, MostPixel = 16, ReadCount = 10, MostRings = 8, Bands = 5;
	/** [PLAN] The most rings a battle's grid is built with: 8 at 96 cells a side (65,016 points, in the ledger's 72k), 7 at
	 * 128 (98,663, in its 104k; 8 would be 112k). */
	inline constexpr int GridRings = 8, GridRings128 = 7;
	/** [SHOW] The wind the sea is raised by at most (SeaShow's Raised, Beaufort 6's top), and the gale's (-TheaterSeaGale). */
	inline constexpr double Raised = 13.8, GaleCap = 24.4;
	/** [PLAN] No train shorter than this is displaced (metres); the pixel-only trains run from 2 to 30 m. */
	inline constexpr double ShortestDrawn = 8.0, PixelShortest = 2.0, PixelLongest = 30.0;
	/** [PLAN] The wind sea's bands, as shares of the peak's frequency, and their fixed edges (the geometric middles). */
	inline constexpr double BandAt[Bands] = { 0.75, 0.9, 1.05, 1.25, 1.55 };
	inline constexpr double BandEdge[Bands + 1] = { 0.68465, 0.82158, 0.97211, 1.14564, 1.39194, 1.72601 };
	/** [PLAN] How far each heading is jittered (a share of the band's spread), and how far each train's frequency sits off
	 * its band's (a share, so a pair's crossing never stands still). */
	inline constexpr double HeadingJitter = 0.2, FrequencyJitter = 0.03;
	/** [PLAN] The swell's two trains, this many radians apart. */
	inline constexpr double SwellApart = 8.0 * Pi / 180.0;
	/** [PLAN] The ride's filters: heave and pitch through a light low-pass at 0.4 Tp, damping 0.7; roll her own
	 * oscillator at her period (SeaShow's RideOf), damping 0.12. A train she cannot average out is one at least 0.3 of
	 * her length. */
	inline constexpr double HeaveNatural = 0.4, HeaveDamping = 0.7, RollDamping = 0.12, RideShortest = 0.3;
	/** [PLAN] The subject's patch: 97 x 97 points at 4 m, 192 m from its middle to each side; or at 8 m (384 m) where a
	 * 192 m square cannot keep to the rings' lines, or her length needs it; or at 2 m (96 m) near the eye, where neither
	 * can (a hull of 120 m at most); the main mesh under it drops by its crest top and this much. Under her the patch's own
	 * cell reaches half her length from her own place (PatchLongest when the shot gives none: a battleship's), and from
	 * there its points ease onto the main mesh, their weights' cell and their place both, over PatchEaseLeast at the least,
	 * to be the main mesh's own surface by Patch.EaseEnd: the half side less 1.5 times the coarsest cell its square may touch
	 * and a patch cell. The square's middle snaps by twice that cell at most, so a snap moves its edge and its notches only
	 * where the patch is already the main mesh's surface, and the drawn sea does not move. It is wanted wherever the main
	 * mesh's cell under her (morphed, at her stem, middle and stern, any heading) is coarser than her trains of half her
	 * length need, or than PatchCell; its square keeps to the rings' lines, touching none finer than its cell and none
	 * coarser than the coarsest its ease allows. The farthest subject the shots plan (the Water shot's, 2 km and more out) is
	 * held by the rings: at a cut C0 is doubled until her patch lies inside them, and once where it is wanted but no square
	 * near the eye keeps to the lines.
	 * HELD FRAMES: the patch is shown by Presence, 0 to 1 over PatchFade seconds of the frame's own time; at 0 it is the main
	 * mesh's own surface everywhere (its ease whole), so it is put on, taken off and changes its cell only there, and its
	 * presence is taken down ahead of a square that would no longer keep to the rings (her place and the eye's PatchFade
	 * ahead, and a snap more). It stays wanted down to PatchStay of the cell that put it on. */
	inline constexpr double PatchCell = 4.0, PatchHalf = 192.0, PatchDrop = 5.0, PatchLongest = 250.0;
	inline constexpr double PatchEaseLeast = 24.0, PatchFarthest = 3000.0, PatchFinest = 2.0;
	inline constexpr double PatchFade = 1.0, PatchStay = 0.9;
	inline constexpr int PatchCells = 96;
	/** [PLAN] The clipmap's first cell (the eye's height over 14, a power of two) is held between these (metres). */
	inline constexpr double LeastC0 = 0.5, MostC0 = 8.0;
	/** [PLAN] The flat skirt beyond the rings reaches this far from the eye (metres). */
	inline constexpr double SkirtReach = 200000.0;
	/** [PLAN] The whitecaps' tails: the fold gate of the three largest trains this many seconds back. */
	inline constexpr double TailLags[4] = { 1.0, 2.0, 4.0, 8.0 };
	/** [EST] How white a tail is against a head (the foam picture's mottle, on the whole). */
	inline constexpr double TailShare = 0.5;
	/** [EST] The Ripples picture's mean square slope for each unit of a read's lean (CampaignTextures::MakeWaterBumps):
	 * the chop and ripple reads' share of the slopes, for the roughness remainder. The Mac refits it. */
	inline constexpr double PictureMss = 0.25;
	/** [PLAN] The clocked pixel reads, each with its own phase frac(rate x SeaT) (no 600 s clock anywhere). */
	enum Read { ChopA, ChopB, RippleA, RippleB, Gusts, GustFingers, FoamA, FoamB, FoamShift, CrestFrame };

	/** A number from 0 to under 1 fixed by a whole number: SplitMix64's step, the waves' own stream. */
	double Mix(uint64_t N);

	/** One train of waves. K = |(Kx, Ky)| (radians a metre), W its frequency (radians a second, W^2 = g K), A its height
	 * (metres), Q its lean (Gerstner's), L its length (metres). Band: 0-4 the wind sea's, -1 the swell's, -2 a pixel
	 * train's. Spread: the band's rms spread (radians) it was laid off downwind by. */
	struct Train
	{
		double Kx = 0.0, Ky = 0.0, K = 0.0, W = 0.0, A = 0.0, Q = 0.0, Phase0 = 0.0, L = 0.0;
		int Band = -2;
		double Spread = 0.0;
	};

	/** What a sea is made from. WindU metres a second; DownwindR the way it blows toward; a swell's height (metres),
	 * period (seconds) and the way it comes from, 0 for none; Fetch metres, 0 for a sea the wind has fully raised; Cap the
	 * most wind the sea is raised by (Raised; GaleCap only under -TheaterSeaGale); PixelTrains 0-16 (-TheaterSeaPixelTrains). */
	struct Asked
	{
		double WindU = 0.0, DownwindR = 0.0, SwellH = 0.0, SwellT = 0.0, SwellFromR = 0.0, Fetch = 0.0;
		uint64_t Seed = 0;
		double Cap = Raised;
		int PixelTrains = 12;
	};

	/** A sea: D the displaced trains, largest first; P the pixel-only trains. Hs, Tp, Lp its spectrum's; WindU the wind
	 * that raised it (capped); CoxMunk the mean square slope (both ways) the sun's road wants; Fold and FoldSoft the
	 * whitecaps' gate on J, and FarHead the head their trains' faded pixels are drawn with (Monahan's cover over the
	 * whitecaps' picture's mean, FleetWavesFold.inl); Read the clocked pixel reads' rates (lengths a second). */
	struct Sea
	{
		Train D[MostDrawn];
		int ND = 0;
		Train P[MostPixel];
		int NP = 0;
		double Hs = 0.0, Tp = 0.0, Lp = 0.0, WindU = 0.0, CoxMunk = 0.0, Fold = 0.0, FoldSoft = 1.0, FarHead = 0.0;
		double Read[ReadCount] = {};
		/** Also: the peak's frequency; the Beaufort number, unrounded ((U/0.836)^(2/3), as the show's Force); the lean's
		 * budget Qb; the spectrum's share inside the displaced bands; the detail reads' slopes; the tails' life (s). */
		double Wp = 0.0, Force = 0.0, Qb = 0.25, InBands = 0.0, DetailMss = 0.0, TailTau = 1.0, Downwind = 0.0;
		/** The whitecaps' picture's side (metres: M_StageSea's 160 m x its cap size by wind), which the head's shape is read at. */
		double CapTile = 160.0;
		bool bSwell = false;
		uint64_t Seed = 0;
	};

	/** Makes the sea (no Monte Carlo: Fold comes from the table). Run lazily: only at tier 1 or more, or with a rider. */
	Sea Make(const Asked& A);
	/** [P] The spectrum Make lays (Pierson-Moskowitz's shape at peak Wp, holding (Hs/4)^2 in all), m^2 s. */
	double Spectrum(double W, double Wp, double Hs);
	/** [P] Mitsuyasu's cos-2s spread at frequency W: its rms (radians). */
	double SpreadAt(double W, double Wp);
	/** [P] Cox-Munk's mean square slope (both ways), Monahan's whitecap cover, the Beaufort number unrounded. */
	double CoxMunkOf(double U);
	double MonahanOf(double U);
	double ForceOf(double U);
	/** Fold, FoldSoft and FarHead by wind speed (FleetWavesFold.inl, solved offline by Tools/sea_waves/fold_table.cpp). */
	void FoldOf(double U, double& Fold, double& FoldSoft, double& FarHead);
	/** [P] JONSWAP's fetch law: the height and the peak's frequency of a sea raised by U over Fetch metres. */
	void FetchLaw(double U, double Fetch, double& Hs, double& Wp);

	// ------------------------------------------------------------------------------------------------------------------
	// The sum.

	/** A displacement: sideways (X, Y) and up (Z), metres. */
	struct Disp
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};
	/** Forward Gerstner at a rest point: the height sum(w a cos th), the shift -sum(w Q a k^ sin th). W: one weight a
	 * displaced train, or null for all 1. */
	Disp Displace(const Sea& S, double X, double Y, double T, const double* W = nullptr);
	/** The surface's slopes (dz/dx, dz/dy over the rest point), its squeeze (dX/dx + dY/dy of the shift) and J (the
	 * shift's Jacobian, 1 flat, 0 folded), from the displaced trains and the pixel trains at full weight, each faded by
	 * its own Fade (null: 1; 26 numbers, as the chip's slots: the displaced trains' 10, then the pixel trains' 16). Mss:
	 * the trains' faded slopes, sum(fade (k a)^2 / 2), without the detail reads. */
	struct Slopes
	{
		double Sx = 0.0, Sy = 0.0, Squeeze = 0.0, J = 1.0, Mss = 0.0;
	};
	Slopes SlopesAt(const Sea& S, double X, double Y, double T, const double* Fade = nullptr);
	/** The whitecaps (the plan's step 7, as the chip draws them): the head saturate((Fold - J) / FoldSoft), and the
	 * tails: the fold gate with the three largest trains taken TailLags back, each faded by exp(-lag / TailTau), times
	 * TailShare. Foam = max(head, tail). Fold < 0 asks for the sea's own. */
	struct Foam
	{
		double Head = 0.0, Tail = 0.0, J = 1.0, Lag[4] = { 1.0, 1.0, 1.0, 1.0 };
	};
	Foam FoamAt(const Sea& S, double X, double Y, double T, double Fold = -1.0, double FoldSoft = -1.0);
	/** The whitecaps as the chip's pixel draws them at a depth (metres) with the lens's PxPerRad: the gate read from every train
	 * at full weight (FoamAt), and as the shortest train drawn fades under the pixel (PixelFade) the head goes to FarHead and
	 * the tail to nought, its mean, so the cover holds Monahan's at every depth and nothing under the pixel flickers. */
	Foam FoamDrawn(const Sea& S, double X, double Y, double T, double Depth, double PxPerRad);
	/** The fade the whitecaps' gate is drawn at: the shortest drawn train's PixelFade (1 with no train). */
	double FoamFade(const Sea& S, double Depth, double PxPerRad);
	/** The cover a Foam gives under a gate: max(head x Shape, tail) from its J and lagged Js. Shape: the whitecaps'
	 * picture's foam where the head is drawn (HeadShapeAt), step 7's head = the gate x the picture's shape. */
	double CoverOf(double J, const double (&Lag)[4], double Fold, double FoldSoft, double TailTau, double Shape = 1.0);
	/** [PLAN, step 7] The whitecaps' picture's foam (red: SeaFace::MakeFoam's, Size x Size bytes, four a pixel; u along the
	 * wind) under a head at a rest point: read with the picture CapTile metres to a side, u downwind, sliding downwind with
	 * the peak's crests (the CrestFrame read's phase), bilinear and repeating. 0 to 1. */
	double HeadShapeAt(const Sea& S, const std::vector<uint8_t>& Picture, int Size, double X, double Y, double T);

	// ------------------------------------------------------------------------------------------------------------------
	// What is drawn: the eye-centred clipmap, the subject's patch, the weights.

	/** The subject's patch: its middle (snapped), its half side and cell (metres), on or off; and the subject's own place
	 * (unsnapped), how far from it the patch's own sea reaches (Full) and where its ease onto the main mesh is done
	 * (EaseEnd), Chebyshev metres: the sea under her goes with her, never with the square's snaps; Snap, the most its middle
	 * moves at a snap (twice the coarsest cell its square may touch). Presence: how far it is shown (1 its own sea, as made;
	 * 0 the main mesh's own surface everywhere in it: every point's ease is 1 - Presence (1 - its own)); Whole: its cell
	 * holds her trains of half her length whole out to half her length (else a nearer Full, or a coarser cell, as it could);
	 * WantCell: the cell under her above which it was put on (the finer of PatchCell and the cell her trains need: it stays
	 * wanted while the main mesh's cell under her is over PatchStay of it); bSubject: the frame had a subject, at SubjectX/Y
	 * whether or not the patch is on (so the next frame knows how she moves); Forced: on this frame it went off or changed
	 * while still shown (its square could no longer be laid where the frame before foresaw: a pop, which the walks count). */
	struct PatchAt
	{
		double X = 0.0, Y = 0.0, Half = 0.0, Cell = 0.0;
		double SubjectX = 0.0, SubjectY = 0.0, Full = 0.0, EaseEnd = 1.0, Snap = 0.0;
		double Presence = 1.0, WantCell = 0.0;
		bool bOn = false, bWhole = false, bSubject = false, bForced = false;
	};
	/** What the scene draws this frame. Eye (the eye's ground point, unsnapped) and the eye before; C0 the first ring's
	 * cell (metres, ring k's is C0 2^k); Rings the rings built; N the cells along a ring's side (96 or 128); RingX/Y each
	 * ring's snapped middle (a whole number of twice its cell); SeaT and SeaTBefore the sea clock this frame and last;
	 * PxPerRad the lens's pixels a radian; Origin the point the stage's numbers are taken from (the last ring's middle). */
	struct Drawn
	{
		double EyeX = 0.0, EyeY = 0.0, EyeBeforeX = 0.0, EyeBeforeY = 0.0;
		double C0 = 1.0;
		int Rings = 1, N = 96;
		double RingX[MostRings] = {}, RingY[MostRings] = {};
		PatchAt Patch;
		double SeaT = 0.0, SeaTBefore = 0.0, PxPerRad = 1000.0;
		double OriginX = 0.0, OriginY = 0.0;
	};

	/** The rings a battle's grid of N cells a side is built with: as many as its longest displaced train needs at the least
	 * C0, plus one, or as hold the farthest subject the shots plan (PatchFarthest, with her widest patch) from the 14 m
	 * lookout's C0 of 1 m, whichever is more; at most GridRings at 96 (8), GridRings128 at 128 (7) (DrawnFor doubles C0
	 * where the longest train or a subject needs more). */
	int RingsFor(const Sea& S, int N = 96);
	/** The frame's drawn sea. Eye and Look board metres (Look unused but kept for the snapshot's callers); LensDeg the
	 * lens's width; Subject the shot's subject's drawn place, or null; SeaT the sea clock; Last the frame before, or null
	 * (with bCut: a cut or a skip, so C0 may change and the clock before is this one). Rings and N as the grid was built;
	 * ScreenPx the picture's width in pixels; SubjectLong her drawn length (metres: the trains of half of it are whole under
	 * her from stem to stern, where her patch can hold them); FrameSeconds the frame's own time since Last (the patch's
	 * presence ramps by it; below 0: the sea clock's step, nought at a cut). */
	Drawn DrawnFor(const Sea& S, const double (&Eye)[3], const double (&Look)[3], double LensDeg, const double* Subject, double SeaT,
		const Drawn* Last, bool bCut, int Rings, int N = 96, double ScreenPx = 1077.0, double SubjectLong = PatchLongest, double FrameSeconds = -1.0);
	/** The cell her trains of half her length need to be whole (the shortest displaced train of Long / 2 or more, over 12;
	 * huge with none), and the coarsest main-mesh cell under her (morphed: at her middle and the corners and sides of the
	 * square Long / 2 round her, so at her stem and stern at any heading). */
	double NeedCell(const Sea& S, double Long);
	double MainUnder(const Drawn& D, const double* Subject, double Long);
	/** A ring's cell (metres). */
	double CellOf(const Drawn& D, int Ring);
	/** The ring whose square holds a point (the first, from the eye out), or -1 beyond them all. */
	int RingOf(const Drawn& D, double X, double Y);
	/** How far ring K's points have moved toward the next ring's grid at Chebyshev distance Dist from the eye (CDLOD):
	 * 0 inside, rising over the ring's outer quarter to 1 by (N/2 - 2) cells, so every point the next ring could take
	 * from it by a snap is already on that ring's grid. */
	double MorphOf(const Drawn& D, int K, double Dist);
	/** The cell the weights are taken at, at a point: the main mesh's (its ring's, morphed, a little ahead, so no edge of
	 * a triangle with a train's weight on any corner is longer than an eighth of it), or inside the patch, the patch's
	 * (its own cell out to Full from the subject, eased to the main mesh's by EaseEnd; the main mesh's in its notches).
	 * Huge beyond the rings. A patch point's own sea is taken at it; over the ease its place goes to the main mesh's too. */
	double CellAt(const Drawn& D, double X, double Y);
	double MainCellAt(const Drawn& D, double X, double Y);
	/** A train's drawn weight at a cell: 1 while the cell is L/12 or less, 0 from L/8. WeightAt, WeightsAt: at a point, its
	 * weight at CellAt, and over the patch's ease its weight at the main mesh's cell as far as the point has eased onto
	 * the main mesh's surface (the drawn sea is the one and the other in those shares). */
	double WeightOf(double L, double Cell);
	double WeightAt(const Sea& S, const Drawn& D, double X, double Y, int J);
	/** Every displaced train's weight at a point (into W[MostDrawn]). */
	void WeightsAt(const Sea& S, const Drawn& D, double X, double Y, double* W);
	/** The weighted forward height at a rest point, at the drawn clock. */
	double HeightDrawn(const Sea& S, const Drawn& D, double X, double Y);
	/** The height over a fixed point (three Newton inversions of the shift), full weight: the tests' reference only. */
	double HeightAt(const Sea& S, double X, double Y, double T);
	/** The most the mesh can stand off the true weighted sum at a point: each train's sag a w (1 - cos(pi d / L)) over
	 * the longest side d of a triangle there (the cell's diagonal; over the patch's ease the main mesh's, which it eases
	 * onto), at the most weight it has about the point, and a times the most its weight changes across the triangle (where
	 * the cell grows, the mesh lays the fade straight); and the crest top sum(w a). */
	double ChordBound(const Sea& S, const Drawn& D, double X, double Y);
	double CrestTop(const Sea& S, const Drawn& D, double X, double Y);

	// ------------------------------------------------------------------------------------------------------------------
	// The meshes, and where the chip puts their points.

	/** One point of the grid or the patch, as the stage lays it (the stage's frame: X the board's Y). X, Y: cells from
	 * its ring's middle (the patch's: from its middle; the skirt's far points: ones toward it). Ring: 0-7, -1 the patch.
	 * Kind: 0 the ring's frame, 1-4 an L trim (1 + dx + 2 dy, dx and dy the next ring's step inward, in the stage's
	 * frame), 6 the skirt's far points (its near ones are the last ring's own edge); the patch: 0 inside, 7 its edge.
	 * AxisX, AxisY: 1 where the point is odd along that axis (UV1, the way it morphs). */
	struct GridPoint
	{
		float X = 0.0f, Y = 0.0f;
		int Ring = 0, Kind = 0;
		float AxisX = 0.0f, AxisY = 0.0f;
	};
	struct Mesh
	{
		std::vector<GridPoint> Points;
		std::vector<int> Triangles; // each the way round that faces up, as AAirStage::BuildSea lays them
	};
	/** The clipmap: ring 0 whole, rings 1 to Rings-1 as frames round an L trim in each of its four ways (the three not in
	 * use collapse to a point on the chip), and the flat skirt out to SkirtReach. An odd point on a ring's outer edge goes
	 * to its even neighbour toward the ring's corners at (-, +) and (+, -) (toward -x along the top row, +x along the
	 * bottom, -y down the right column, +y up the left), so the next ring's points meet it with no T-junction. */
	Mesh Grid(int N, int Rings);
	/** The subject's patch: 97 x 97 points. At its corners (-H, +H) and (+H, -H) the main cell inside its square is split
	 * (00-11) so that one of its triangles has all three points on the square's edge and is never dropped: that triangle,
	 * a notch with its long side along the patch's own diagonals, stays the main mesh's, and the patch's points in or on it
	 * lie on its long side (Placed::bNotch), their triangles of no area. Where the ring's own fold has put one of its
	 * points on the corner there is no such triangle and no notch. */
	Mesh Patch();
	/** Where a point of the grid or the patch rests and moves to (board metres), as the chip puts it: Rest after the trim's
	 * collapse and the morph, the cell its weights are taken at, and its place displaced (with the patch's hole and the
	 * patch's edge taken from the main mesh's). bBefore: at the eye and clock before, on this frame's snaps. */
	struct Placed
	{
		double RestX = 0.0, RestY = 0.0, Cell = 0.0, X = 0.0, Y = 0.0, Z = 0.0;
		double Ease = 0.0; // how far its shift and height have eased toward the next ring's grid (the morph)
		bool bCollapsed = false, bDropped = false;
		bool bNotch = false; // a patch point in or on one of its notches: laid on the notch's long side, the main mesh's

	};
	Placed PlaceOf(const Sea& S, const Drawn& D, const GridPoint& P, bool bBefore = false);
	/** The drawn mesh's height over a rest point: the triangle of the drawn triangulation (rings, trims, morph, the patch
	 * with its edge) that holds it, its corners' heights laid linearly. The probe's, the float tests' and SeaNear's
	 * reference. */
	double DrawnMeshZ(const Sea& S, const Drawn& D, double X, double Y);
	/** The same, of the main mesh alone (the rings and the skirt, as if there were no patch; under the patch, before its
	 * drop): the patch's edge is held to it. */
	double MainMeshZ(const Sea& S, const Drawn& D, double X, double Y);
	/** The drawn surface over a fixed (world) point: the height of the displaced triangulation there, the triangle of
	 * displaced corners that holds it (the drawn mesh's map from rest to place inverted, Newton on each triangle's own affine
	 * map, to a micrometre). What a float, a man or a splash at (X, Y) stands on; SeaNear's reference. RestOut: the rest
	 * point it found. */
	double DrawnSurfaceZ(const Sea& S, const Drawn& D, double X, double Y, double* RestOut = nullptr);

	// ------------------------------------------------------------------------------------------------------------------
	// The ride: a train's steady response for one hull.

	struct RideFilter
	{
		double HeaveW = 1.0, HeaveZeta = HeaveDamping, RollW = 0.5, RollZeta = RollDamping;
	};
	/** Her filters: the light low-pass at 0.4 Tp, and her roll at RollT seconds. */
	RideFilter FilterFor(const Sea& S, double RollT);
	/** Her response to one train, at its rest phase: heave metres, pitch degrees (bow up), roll degrees (starboard down,
	 * as the show's Heel). Re[C e^(i th)] with th the train's phase at her middle. */
	struct RideC
	{
		std::complex<double> Heave, Pitch, Roll;
	};
	/** The plane by least squares through 5 stations along and 3 across (ends weighted 0.5), through her filter at the
	 * encounter frequency W - K.V. */
	RideC RideCoef(const Train& T, double Long, double Beam, double Hdg, double Speed, const RideFilter& F);
	/** A hull's coefficients: the trains she cannot average out (L >= 0.3 her length). */
	struct HullRide
	{
		int N = 0;
		int Of[MostDrawn] = {};
		RideC C[MostDrawn];
	};
	void RideTrains(const Sea& S, double Long, double Beam, double Hdg, double Speed, double RollT, HullRide& Out);
	/** Her ride at her middle (X, Y) and the sea clock, each train weighted (W: the displaced trains' weights, null 1). */
	void RideAt(const Sea& S, const HullRide& H, double X, double Y, double SeaT, const double* W, double& Heave, double& Pitch, double& Roll);

	// ------------------------------------------------------------------------------------------------------------------
	// SeaNear: the drawn sea round the eye and the subject, on small grids, for everything in the water.

	/** A grid of heights: its first node, its cell (and 1 over it), the square it answers for (its middle and half side;
	 * Reach < 0: none), its nodes a side, and the heights row by row; and its shifts, at every other node of it (its even
	 * nodes: SNodes a side, twice its cell apart from its first node; X and Y of each, in turn, row by row). Made as it
	 * stands, it is the empty level BuildNear lays: two nodes a side of nought, answering for nothing, so a Near not yet
	 * built reads 0 everywhere. */
	struct NearLevel
	{
		double X0 = 0.0, Y0 = 0.0, Cell = 1.0, Inv = 0.0;
		double Mx = 0.0, My = 0.0, Reach = -1.0;
		int Nodes = 2, SNodes = 2;
		double SInv = 0.0;
		std::vector<float> H = std::vector<float>(4, 0.0f);
		std::vector<float> S = std::vector<float>(8, 0.0f);
	};
	/** SeaNear: a value (it keeps no pointer to the sea or the frame), so a snapshot may copy it or hand it to a pool
	 * thread. L[K]: ring K's level, K under NL (from NL on, and wherever built none, an empty level that reads 0 and
	 * answers for nothing: Reach < 0); L[NearBlock]: the subject's block, answering for the square EaseEnd and a patch
	 * cell round her unsnapped place; the eye's ground point and 1 / ((Half - 2) C0), which picks a point's level from its
	 * distance to the eye. Made as it stands it reads 0 everywhere. */
	inline constexpr int NearBlock = MostRings;
	struct Near
	{
		NearLevel L[MostRings + 1];
		int NL = 0;
		double Ex = 0.0, Ey = 0.0, Pick = 1.0;
	};
	/** The eye's levels: one at each ring's own grid (the mesh's own points) on every ring where any displaced train has
	 * weight, so beyond the last one every weight is nought and the drawn sea flat; the inside of the ring before left to
	 * the finer level. The subject's block on the patch's grid, over the square where the patch's cells have not yet eased
	 * whole onto the main mesh (EaseEnd and a patch cell round her), each node eased onto the eye's levels as the patch's
	 * point is: past it, its edge and its notches included, the patch is the main mesh's surface, which the eye's levels
	 * hold. Forward heights and shifts at rest points with the drawn weights baked into every node (each node's train
	 * weights from its cell, which is its column's or its row's, so the sum stays separable: cos(kx x + ky y + c) =
	 * cos(kx x) cos(ky y + c) - sin(kx x) sin(ky y + c); a shift is the same sum of a train Q a k^ long a quarter turn on);
	 * over a ring's morph band each odd node eased toward the middle of its even neighbours, as the mesh's own point is, so
	 * the level is the mesh itself. The shifts at every other node (the even ones, which never ease), for SampleNear's
	 * step from a world point to the rest point under it. MostLevels: the levels built at most (the frame guard's rung).
	 * bShifts false: the rest surface alone (its heights, the work BuildNear did before it answered world points: its own
	 * ledger line), the shifts left nought, so SampleNear reads SampleNearRest's height. */
	void BuildNear(const Sea& S, const Drawn& D, Near& Out, int MostLevels = MostRings, bool bShifts = true);
	/** A level's height at a rest point, each cell split as the mesh splits it (the 00-11 diagonal). */
	inline double SampleLevel(const NearLevel& Lv, double X, double Y)
	{
		const double Fx = (X - Lv.X0) * Lv.Inv, Fy = (Y - Lv.Y0) * Lv.Inv;
		const int Last = Lv.Nodes - 2;
		int I = static_cast<int>(Fx), J = static_cast<int>(Fy);
		I = I < 0 ? 0 : (I > Last ? Last : I);
		J = J < 0 ? 0 : (J > Last ? Last : J);
		const double U = Fx - I, V = Fy - J;
		const float* H = Lv.H.data() + static_cast<size_t>(J) * Lv.Nodes + I;
		// (below the diagonal, U >= V, it is Z00 + U (Z10 - Z00) + V (Z11 - Z10), above it Z00 + V (Z01 - Z00) + U (Z11 - Z01):
		// both are the bilinear's first three terms and min(U, V) of its twist, min(U, V) as (U + V - |U - V|) / 2, so no
		// branch, which a sample's side, all but random, would mispredict)
		const double Z00 = H[0], Z10 = H[1], Z01 = H[Lv.Nodes], Z11 = H[Lv.Nodes + 1];
		const double Least = 0.5 * (U + V - std::fabs(U - V));
		return Z00 + U * (Z10 - Z00) + V * (Z01 - Z00) + Least * ((Z11 - Z10) - (Z01 - Z00));
	}
	/** A level's shift at a rest point, from its even nodes (each cell of them split as the height's). */
	inline void ShiftLevel(const NearLevel& Lv, double X, double Y, double& Sx, double& Sy)
	{
		const double Fx = (X - Lv.X0) * Lv.SInv, Fy = (Y - Lv.Y0) * Lv.SInv;
		const int Last = Lv.SNodes - 2;
		int I = static_cast<int>(Fx), J = static_cast<int>(Fy);
		I = I < 0 ? 0 : (I > Last ? Last : I);
		J = J < 0 ? 0 : (J > Last ? Last : J);
		const double U = Fx - I, V = Fy - J, Least = 0.5 * (U + V - std::fabs(U - V));
		const float* P = Lv.S.data() + 2 * (static_cast<size_t>(J) * Lv.SNodes + I);
		const size_t Up = 2 * static_cast<size_t>(Lv.SNodes);
		const double X00 = P[0], X10 = P[2], X01 = P[Up], X11 = P[Up + 2];
		const double Y00 = P[1], Y10 = P[3], Y01 = P[Up + 1], Y11 = P[Up + 3];
		Sx = X00 + U * (X10 - X00) + V * (X01 - X00) + Least * ((X11 - X10) - (X01 - X00));
		Sy = Y00 + U * (Y10 - Y00) + V * (Y01 - Y00) + Least * ((Y11 - Y10) - (Y01 - Y00));
	}
	/** The level that answers for a rest point: the subject's block where it holds the point, else level K = ceil(log2(d / 46
	 * C0)) for a point d from the eye (at RingCells 96: Half - 2 cells, Pick = 1 / (46 C0)), an empty one beyond them all (no
	 * train has weight there). Ring K's square holds every point within 46 of its cells of the eye (its middle is within 2
	 * of the eye), and a point over 23 of them out is in ring K, or in the outer band of ring K - 1, which has eased whole
	 * onto ring K's grid; level K's hole (ring K - 1's inside) stops 22 of its cells from the eye, so each of the point's
	 * four nodes is laid. The last ring's edge is flat (its cell there is twice its own, where no train has weight), so a
	 * point beyond it reads 0 from the edge. The level is found without a search or a branch. */
	inline const NearLevel& NearLevelAt(const Near& N, double X, double Y)
	{
		const double Q = std::max(std::fabs(X - N.Ex), std::fabs(Y - N.Ey)) * N.Pick;
		// (ceil(log2(Q)) from its bits: the exponent, and one more where the mantissa is not nought; 0 for Q of 1 or less)
		uint64_t Bits = 0;
		std::memcpy(&Bits, &Q, sizeof(Bits));
		int K = static_cast<int>(Bits >> 52) - 1023 + static_cast<int>((Bits & 0xFFFFFFFFFFFFFull) != 0);
		K = K < 0 ? 0 : (K > MostRings - 1 ? MostRings - 1 : K);
		const NearLevel& B = N.L[NearBlock];
		const int In = static_cast<int>(std::fabs(X - B.Mx) <= B.Reach) & static_cast<int>(std::fabs(Y - B.My) <= B.Reach);
		return N.L[K + In * (NearBlock - K)];
	}
	/** The drawn mesh's height over a rest point (its own point there, before its shift): DrawnMeshZ's, to the float. */
	inline double SampleNearRest(const Near& N, double X, double Y) { return SampleLevel(NearLevelAt(N, X, Y), X, Y); }
	/** The drawn mesh's shift at a rest point (where its point there is drawn, less the rest point). */
	inline void NearShift(const Near& N, double X, double Y, double& Sx, double& Sy) { ShiftLevel(NearLevelAt(N, X, Y), X, Y, Sx, Sy); }
	/** The drawn sea's height over a world point (DrawnSurfaceZ's, within 5 cm at release 2's winds): the rest point under it
	 * by one step back along the shift there (X less the shift at X), and the mesh's height at that rest point, each read
	 * from the level that answers for its own point. What a float, a man, a splash or the kit's ground at (X, Y) stands on. */
	inline double SampleNear(const Near& N, double X, double Y)
	{
		double Sx = 0.0, Sy = 0.0;
		NearShift(N, X, Y, Sx, Sy);
		return SampleNearRest(N, X - Sx, Y - Sy);
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Light on the water: the slopes drawn, and the roughness that makes up Cox-Munk.

	/** The slopes the pixel draws at a depth (metres): every train faded where it is under 4 pixels, plus the detail reads
	 * (faded far as M_StageSea's FarLean: to 0.35 of their lean by 6 km). */
	double DrawnMss(const Sea& S, const Drawn& D, double Depth);
	double FarLean(double Depth);
	/** A train's fade by its pixels: 0 under 4, 1 from 8. */
	double PixelFade(double L, double Depth, double PxPerRad);
	/** [PLAN, M3] Unreal's roughness for a pixel whose own is R0 with Dmss of slopes left out: (R0^4 + Dmss)^(1/4). */
	double RoughnessFor(double R0, double Dmss);

	// ------------------------------------------------------------------------------------------------------------------
	// The stage's numbers: the MPC (MPC_FleetSea, step 7).

	struct F4
	{
		float R = 0.0f, G = 0.0f, B = 0.0f, A = 0.0f;
	};
	/** Each MPC vector, in the stage's frame and units. Train/TrainBefore/Pixel: RG wavenumber (turns a unit), B height
	 * (units), A phase (turns, in [-0.5, 0.5)) at Origin. Steep: R Q a (units), G L (units), B w (radians a second), A k a.
	 * Rings: the snapped middles of two rings each, from Origin. Eye: RG the eye's ground point, BA the eye before, from
	 * Origin. Shape: C0 (units), rings, pixels a radian, N. Patch: RG its middle from Origin, B its half side, A its cell (0
	 * off). PatchEase: RG the subject's own place from the patch's middle, B how far the patch's own cell reaches from it,
	 * A where its ease to the main mesh's cell is done (units). Fold: R Fold, G FoldSoft, B the tails' life (s), A
	 * TailShare. Lean: R each train's most Q k a (Qb over every train, displaced and pixel), G Qb, B the detail reads'
	 * slopes (DetailMss), A UnitsPerMetre. Rough: R Cox-Munk's mean square slope for the sea's wind (both ways), G that
	 * wind (m/s, capped), so the material's roughness remainder is max(R - the slopes the pixel draws, 0) (step 7).
	 * ReadPhase: the clocked reads' phases (turns), four a vector. Origin: RG Origin itself (units). Fade: R the patch's
	 * presence (0 to 1: at 0 every patch point is the main mesh's own surface), G the whitecaps' head where their trains
	 * have faded under the pixel (FarHead: Monahan's cover over the picture's mean), B their tail there (nought: the head's
	 * mean carries the cover), A spare. (The hull waves' inputs, step 9a, join it there.) */
	struct ForStage
	{
		F4 Train[MostDrawn], TrainBefore[MostDrawn], Steep[MostDrawn], Pixel[MostPixel], Rings[4];
		F4 Eye, Shape, Patch, PatchEase, Fold, Lean, Rough, ReadPhase[3], Origin, Fade;
	};
	/** The MPC's own defaults: no effect anywhere, every divisor 1. */
	ForStage EmptyStage();
	/** The frame's numbers: the swap (stage X = board Y), the units, phases in double frac'd before the cast, the clock
	 * before on this frame's snaps. */
	ForStage StageParams(const Sea& S, const Drawn& D, double UnitsPerMetre);
	/** Each vector's name as the MPC holds it, and its value, in order (for material_check and the scans). */
	std::vector<std::pair<std::string, F4>> StageList(const ForStage& F);

	/** The wraps a phase is taken through: frac into [-0.5, 0.5) (turns). */
	double Turns(double T);

	// ------------------------------------------------------------------------------------------------------------------
	// The wake lane (step 12): a 512 x 512 RG8 picture over 2.5 km round the eye.

	/** A stern point of a ship's wake: where, how old (s), her beam, which ship (a lane joins her points in order). */
	struct LanePoint
	{
		double X = 0.0, Y = 0.0, Age = 0.0, Beam = 10.0;
		int Ship = 0;
	};
	struct OilDisc
	{
		double X = 0.0, Y = 0.0, Radius = 0.0;
	};
	/** The window: its corner, snapped to whole texels, its side (metres) and texels. */
	struct LaneWindow
	{
		double X0 = 0.0, Y0 = 0.0, Size = 2500.0;
		int Texels = 512;
	};
	LaneWindow LaneWindowFor(double EyeX, double EyeY);
	/** Red the calm, turbulent lane (widening and fading with age and wind), green fresh foam; texel (i, j) at
	 * RG[2 (j Texels + i)], X along i. A pure function of the points. */
	void PaintLanes(const std::vector<LanePoint>& Wake, const std::vector<OilDisc>& Oil, const LaneWindow& W, double WindU, std::vector<uint8_t>& RG);

	// ------------------------------------------------------------------------------------------------------------------
	// The gale (the plan's step 14(c), an extra behind its own switch, off by default; Brian's and the art director's
	// decision). Under it the scene asks Make with Cap GaleCap (24.4 m/s, Beaufort 9's top) and the fetch GaleFetchOf gives;
	// at or under Raised (13.8 m/s) it asks nothing new, so every number above is as made (Tools/sea_waves/gale_pin.cpp
	// holds them to 942d9295's, byte for byte). Nothing here is read by anything the show decides. Nothing is wired yet:
	// the game does not draw the fleet sea, so the switch is not read in this tree (SeaLookSwitches.h keeps it unbuilt).

	/** [P] The World Meteorological Organization's Beaufort 7, 8 and 9 (WMO-No. 558, Manual on Marine Meteorological
	 * Services, the Beaufort scale's table; the same in Bowditch's): the wind at 10 m (m/s, from and to) and the probable
	 * height of the waves in the open sea, remote from land, and their probable most (metres). The words: 7 "sea heaps up
	 * and white foam from breaking waves begins to be blown in streaks along the direction of the wind"; 8 "edges of crests
	 * begin to break into spindrift; the foam is blown in well-marked streaks"; 9 "dense streaks of foam along the direction
	 * of the wind; crests of waves begin to topple, tumble and roll over; spray may affect visibility". */
	struct BeaufortSea
	{
		int Force = 0;
		double From = 0.0, To = 0.0, Probable = 0.0, Most = 0.0;
	};
	inline constexpr BeaufortSea WmoGale[3] = { { 7, 13.9, 17.1, 4.0, 5.5 }, { 8, 17.2, 20.7, 5.5, 7.5 }, { 9, 20.8, 24.4, 7.0, 10.0 } };
	/** [EST] How long the gale has blown when the field gives no fetch (hours; 63,000 s). It is picked so the sea a gale of
	 * it raises at the cap is exactly the cap's own sea (0.021 Raised^2, 4.00 m), so the sea grows smoothly with the wind
	 * across the cap; and then each of WMO's rows has its probable height at its foot within 5% (4.04 m at Beaufort 7's,
	 * 5.27 at 8's, 6.68 at 9's) and rises toward the row's probable most by its head, never past it (gale_test). A sea the
	 * wind has fully raised (Make with no fetch: Pierson-Moskowitz, the show's 0.021 U^2) is WMO's probable most instead,
	 * within 9% at each row's middle: that needs a gale of two days and more over 1,000 km and more of sea, which a
	 * battle's gale seldom had, so the gale's own sea is the fetch- and duration-limited one. */
	inline constexpr double GaleHours = 17.5;
	/** [P] The fetch whose sea a wind of U (m/s) raises in Hours over open water (metres): the fetch the duration limits
	 * the sea as, by CERC's Shore Protection Manual (1984), eq. 3-35, g t / U = 68.8 (g F / U^2)^(2/3) (with U for its
	 * adjusted wind U_A, as FetchLaw takes it). It grows as U^0.5, so the duration-limited sea's own height grows as
	 * U^1.25 Hours^0.75 and its peak period as U^0.5: both rise with the wind, as a gale's sea must. */
	double FetchOfHours(double U, double Hours);
	/** The fetch Make is asked with under the gale, for a wind U: at or under Raised, FieldFetch as given (nothing new);
	 * over it, the fetch a gale of Hours raises its sea over (FetchOfHours), and no more than the field's own fetch where
	 * it gives one (FieldFetch > 0: a short fetch holds the sea lower, as it would, and then the sea may fall below the
	 * cap's own, which only a fetch the rules send can do). */
	double GaleFetchOf(double U, double FieldFetch = 0.0, double Hours = GaleHours);
	/** [EST] How much of the wind's streaks (SeaFace::MakeStreaks) the sea shows at a wind U (m/s), 0 to 1: none to the
	 * foot of Beaufort 6 (10.8), faint in its upper half (the plan's "from Beaufort 6"), 0.15 at 7's foot where WMO's
	 * streaks begin, 0.45 at 8's (well marked), 0.8 at 9's (dense), 1 at GaleCap; straight between. The picture's green
	 * is each streak's rank, so a streak shows where its rank is under the share (the denser sea keeps the fainter's). */
	double StreakShareOf(double U);

	/** A hull on the wake lane, for her footprint (the gale's): her middle at the waterline (board metres), her heading
	 * (radians clockwise from north), her length and beam at the waterline (metres). */
	struct LaneHull
	{
		double X = 0.0, Y = 0.0, Hdg = 0.0, Long = 100.0, Beam = 10.0;
	};
	/** [EST] The footprint's blue: her signed distance in from her waterline (metres, inside plus), FootReach either way
	 * over the byte's 0 to 255 (0.5 at the waterline, 0.125 m a step); and how far in from it the sea is wholly flat.
	 * Written as a distance, not as cover: the lane's texel is 4.9 m, wider than half a destroyer's beam, and a distance
	 * read between texels is right along her straight sides (it is straight there), where a cover would spread the flat
	 * half a texel out onto the water beside her. */
	inline constexpr double FootReach = 16.0, FootRamp = 1.5;
	/** [EST] Her waterline's half-breadth, a share of half her beam, at S along her from her stern (-1) to her stem (+1):
	 * 1 - S^2 forward of her middle (a fine entrance), 1 - |S|^3 abaft it (a fuller run); the waterplane's coefficient
	 * comes to 0.71, a destroyer's. */
	double HalfBreadthShare(double S);
	/** Her signed distance in from her waterline at a point (metres, plus inside; the first order of it, exact along her
	 * parallel middle body, and past her ends the distance to her stem or stern). */
	double HullInside(const LaneHull& H, double X, double Y);
	/** The wake lane as the gale draws it: RGBA8, texel (i, j) at RGBA[4 (j Texels + i)]: red and green exactly PaintLanes'
	 * (the same bytes), blue each hull's footprint (her signed distance in from her waterline, the most of every hull's,
	 * FootReach either way; 0 with no hull near), alpha 0 (spare). The chip flattens the drawn sea inside her by
	 * FootFlatten(FootInside(blue)) (the step 12 material's read: owed). */
	void PaintLanesGale(const std::vector<LanePoint>& Wake, const std::vector<OilDisc>& Oil, const std::vector<LaneHull>& Hulls, const LaneWindow& W, double WindU,
		std::vector<uint8_t>& RGBA);
	/** The footprint's blue (0 to 1, as the chip reads it) as her distance in from her waterline (metres). */
	inline double FootInside(double Blue) { return (Blue - 0.5) * 2.0 * FootReach; }
	/** How far the drawn sea is flattened there: 0 at her waterline and outside it, 1 from FootRamp in. */
	inline double FootFlatten(double Inside) { return std::clamp(Inside / FootRamp, 0.0, 1.0); }

	// ------------------------------------------------------------------------------------------------------------------
	// The chip's text, compiled here in float (FleetWavesGpu.inl through these stand-ins).

	namespace Gpu
	{
		struct float2
		{
			float x = 0.0f, y = 0.0f;
			float2() = default;
			constexpr float2(float A, float B) : x(A), y(B) {}
		};
		struct float4
		{
			float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
			float4() = default;
			constexpr float4(float A, float B, float C, float D) : x(A), y(B), z(C), w(D) {}
		};
		/** Everything the Custom node is handed. Mode: 0 the point's place (rest + shift, height, the weights' cell), 1 its
		 * rest (rest, the weights' cell), 2 the pixel's light (slopes, J, the slopes drawn), 3 the pixel's foam (head, tail,
		 * squeeze, height over the crest top). Local: the point's cells (Mode 0, 1) or the pixel's rest from Origin (2, 3);
		 * Vtx: ring and kind; Axis: UV1; Cell: the pixel's weights' cell (from the interpolator); Depth: PixelDepth. */
		struct Inputs
		{
			float4 Train[MostDrawn], Steep[MostDrawn], Pixel[MostPixel], Rings[4];
			float4 Eye, Shape, Patch, PatchEase, Fold, Lean, Fade;
			float2 Local, Vtx, Axis;
			float Cell = 0.0f, Depth = 0.0f, Mode = 0.0f;
		};
		/** The .inl itself, run once. bNaNs: min, max and saturate keep a NaN (as a chip may), for the empty MPC's test. */
		float4 Run(const Inputs& In, bool bNaNs = false);
		/** The inputs from a ForStage (Train or TrainBefore; the eye now or before). */
		Inputs From(const ForStage& F, bool bBefore);
	}
}
