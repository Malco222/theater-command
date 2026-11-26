#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * The battle effects, worked out in plain C++ so that the same code runs in the game and in the cloud preview
 * (tools/battle-fx/preview): a shell landing, a gun firing, tracer, a burning wreck, the crater left behind and the
 * camera's jolt. Every number comes from the recipe, Content/Effects/battle-fx.json (tools/battle-fx/fx.json), so the
 * look is tuned there and not here.
 *
 * Nothing here knows Unreal. Places are in metres, Z up, time in seconds. What it gives back each frame is a list of
 * soft puffs, each a middle point and two rings of points (the shape the campaign map's smoke uses, with
 * game.puff_sides points a ring, so one material serves both), already turned to the eye, coloured, and laid far to
 * near; a few lamps for the flashes; and the camera's jolt, which the caller adds to its own camera if it wants it.
 *
 * Lessons the shapes follow (the art director's sheet, art-director/explosions):
 * - the earth is the star of a land hit: a wide torn fan of dark jets, uneven, thinnest and torn at the tip, clods
 *   flying on beyond; never a dome, a ball, a hand of fingers or a broom of even strokes;
 * - many small overlapping puffs of varied size, never a few big round ones (they read as eggs, grapes, cotton);
 * - a flash lights the ground for one frame and is gone by the third; no orange fireball on a ground hit;
 * - smoke climbs, leans downwind and lingers; a crater stays for the whole battle.
 */
namespace TheaterFx
{
	struct Vec
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		Vec() = default;
		Vec(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
		Vec operator+(const Vec& O) const { return Vec(X + O.X, Y + O.Y, Z + O.Z); }
		Vec operator-(const Vec& O) const { return Vec(X - O.X, Y - O.Y, Z - O.Z); }
		Vec operator*(double S) const { return Vec(X * S, Y * S, Z * S); }
		Vec operator-() const { return Vec(-X, -Y, -Z); }
		Vec& operator+=(const Vec& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
		double Dot(const Vec& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		Vec Cross(const Vec& O) const { return Vec(Y * O.Z - Z * O.Y, Z * O.X - X * O.Z, X * O.Y - Y * O.X); }
		double Length() const;
		/** This one made a metre long, or Otherwise when it has no length. */
		Vec Unit(const Vec& Otherwise = Vec(0.0, 0.0, 1.0)) const;
	};

	/** A colour as light (linear), 0 to 1. Picture colours from the recipe (sRGB 0 to 255) are turned to these. */
	struct Rgb
	{
		float R = 0.0f, G = 0.0f, B = 0.0f;
		Rgb() = default;
		Rgb(float InR, float InG, float InB) : R(InR), G(InG), B(InB) {}
		static Rgb FromPicture(double R255, double G255, double B255);
		Rgb operator*(double S) const;
		Rgb Mix(const Rgb& O, double T) const;
	};

	/** Where the numbers come from. The path is dotted, with a list's places as numbers: "sizes.how105.crown_m.0". */
	class Recipe
	{
	public:
		virtual ~Recipe() = default;
		virtual bool Find(const char* Path, double& Out) const = 0;
		/** A word at Path (the calibre map's sizes); none by default. */
		virtual bool FindWord(const char* Path, std::string& Out) const
		{
			(void)Path;
			(void)Out;
			return false;
		}
	};

	/** A small steady random source, the same on every machine, so a battle's effects can be played again alike. */
	class Dice
	{
	public:
		explicit Dice(uint64_t Seed = 1);
		double U(); // 0 to 1
		double U(double A, double B);
		int Whole(int A, int B); // A to B, both included
		double Bell(); // a normal spread, middle 0, width 1
	private:
		uint64_t State = 1;
		uint64_t Next();
	};

	/** One thing drawn: a puff of smoke, dust or earth, a tongue of flame, a clod, a spark, a tracer, a crater's mark. */
	struct Bit
	{
		enum class Shape : uint8_t
		{
			Puff, // turned to the eye; drawn out along Along (on the screen) when Stretch is over 1
			Upright, // stands on the ground and turns about the upright only: low skirts of dust, which never cut into the ground
			Flat, // lies on the ground (a crater's mark)
			Streak, // drawn out along its own flight: clods, sparks, tracer; never thinner than LeastPx on the screen
			Plate // a torn piece of an aircraft: a flat piece lying along Along and Face, tumbling about Along, lit and glinting
		};
		Shape Form = Shape::Puff;
		Vec From, Vel, Along;
		double Born = 0.0, Life = 1.0;
		double Drag = 0.0, Gravity = 0.0, Rise = 0.0, Windage = 0.0;
		double R0 = 0.1, R1 = 0.2, Grow = 0.5; // radius when born, at the end of its life, and how soon it gets there
		double Stretch = 1.0, Flatten = 1.0, Point = 0.0, Tail = 0.0, Spin = 0.0;
		double HalfLength = 0.0; // (a tracer's streak: half its length in metres)
		Vec Origin; // (a tracer: where it was fired from, or a ricochet's strike; nothing of it is drawn behind this)
		bool bFired = false; // (made by Tracer(): only then is Origin set, and only then is the streak cut at it; a hit's spark is never cut)
		double Until = 0.0; // (a close shot's whole path: how far along from Origin it strikes; nothing is drawn past it. 0: no end)
		// (an air gun's tracer by the art director's 15:02Z ruling. BySpeed over 0: its streak is as long as this many
		// seconds of its flight at the speed it has now, never longer than it was born, so it shortens as the round
		// slows. DimTo under 1: from DimFrom of its life it dims, to DimTo of its strength by Hold, and then goes out.
		// DayPx over 0: its own width by day, in the pixels the scene tells the kit, where the ruled streak takes
		// tracer.day_px or day_core_px. bOwnDay: by day it keeps its own colours and burns its own Over past white,
		// where the ruled streak takes tracer.day_edge and day_core)
		double BySpeed = 0.0, DimFrom = 1.0, DimTo = 1.0, DayPx = 0.0;
		bool bOwnDay = false;
		double Alpha = 1.0, FadeIn = 0.02, Hold = 0.2, Cool = 1.0;
		// (CoolFrom over 0: its colour holds as laid until this share of its life and only then cools, Cool of the way to
		// Col2 by its end, so a fuel ball's middle stays yellow-orange through its first half second. 0: it cools from
		// birth, as ever)
		double CoolFrom = 0.0;
		double HazeBy = 0.0, HazeAlpha = 0.0; // (a skirt: settles to HazeAlpha by HazeBy seconds)
		// (a funnel's puff: AgeFade over 0, its opacity falls with its age, however long it lives: by e at AgeFade
		// seconds, as the power AgeFadePow of its age over that, so over 1 it holds while young and then goes sooner;
		// AgeFadeFrom over 0: its age counted for this only from that many seconds, so it holds whole until then)
		double AgeFade = 0.0, AgeFadePow = 1.0, AgeFadeFrom = 0.0;
		// (smoke that goes slowly, Battle::Lingering. bTail: past Hold it is in its cloud's tail, and goes as smoke does
		// into the air. Of the smoke the cloud had when thick, the depth of it the eye looks through, TailThird is
		// left a third of the way through the tail and TailTwoThirds at two thirds, falling evenly between (as a
		// share of what is left each moment), on at that rate to four fifths, and from there to nothing by the end
		// with no step. A puff that stays (TailGoBy 0) takes over the smoke of those that go, which are TailKeep's
		// rest of the cloud and retire over the first TailGoes of the tail; one that goes (TailGoBy over 0: the
		// share of the tail it lives) gives its smoke up as it retires. It widens on, by the tail's end TailWide
		// wider again than its own widening makes it, the same smoke over the wider face as much thinner; and the
		// cloud spreads with it, each puff by the tail's end TailSpread further again from the cloud's middle
		// (TailMid, carried by the wind at TailMidWindage) than it would be, level with the ground. It climbs
		// TailLift metres a second more. Between TailPaleFrom and TailPaleBy of the way its colour goes TailPale of
		// the way to the colour of the air behind it (the sky the scene gave, SetSky, or TailCol), so old thin smoke
		// is never a tinted ghost on the sky)
		// (its looks, all but its thinning, go on by the life and the hold it was laid with, LookLife and LookHold: its
		// own widening, its colours, its heart and its lumps are what they were, over the same seconds, and stay as they
		// ended once that life is over; and it carries no lumps once its tail has begun)
		bool bTail = false;
		double LookLife = 0.0, LookHold = 0.0;
		double LifeOfLooks() const { return bTail ? LookLife : Life; }
		double HoldOfLooks() const { return bTail ? LookHold : Hold; }
		bool InTail(double Age_) const { return bTail && Age_ > Hold * Life; }
		double TailThird = 0.1, TailTwoThirds = 0.02, TailKeep = 1.0, TailGoes = 0.4, TailGoBy = 0.0;
		double TailWide = 0.0, TailSpread = 0.0, TailMidWindage = 0.75, TailLift = 0.0, TailPale = 0.0, TailPaleFrom = 0.0, TailPaleBy = 1.0;
		Rgb TailCol;
		Vec TailMid;
		// (a funnel's plume laid evenly, Battle::PlumeMove: Even is how many halvings of the plume's puffs this one lives through, -1 for
		// any other piece; EvenK its widening, its width as a share of its width at the lip being 1 + EvenK x age^(2/3); EvenWin the last
		// share of each doubling of that width over which every other puff goes and the ones between take its smoke up)
		int Even = -1;
		double EvenK = 0.0, EvenWin = 0.0;
		// (when the halvings come: the first EvenNear each time the plume has doubled its width, the next EvenFar sooner,
		// each when it is 2 to the EvenStep times wider, and the rest at each doubling again)
		int EvenNear = 30, EvenFar = 0;
		double EvenStep = 1.0;
		// (its heart ring a true round under a lumpy rim: 0 the ring is as lumpy as the rim, as ever; 1 quite round. A funnel's
		// puff: its outline billows and its middle stays as thick whichever way it is turned)
		double HeartRound = 0.0;
		// (a funnel's plume that rolls, Battle::PlumeMove: a slow wave along the plume by the puff's number EvenN, thrown from the
		// trail's own EvenSeed: one value every EvenRollRun puffs at the lip, and twice as far apart each time the plume has
		// doubled its width, so a roll is always about as long as the plume is wide. Where the wave is high the puffs ride
		// EvenRollLift of their radius higher and are EvenRollShade darker; where it is low, as much the other way. Its depth
		// in puffs is untouched: the art director's bead measure was no worse for it. The wave is never over 1: no dice, no
		// tails. Both 0: no roll)
		uint32_t EvenN = 0, EvenSeed = 0;
		double EvenRollLift = 0.0, EvenRollShade = 0.0, EvenRollRun = 4.0;
		double Heart = 0.5, Rim = 0.0; // how thick it is at its heart ring, and at its edge, as shares of its middle
		double HeartEnd = -1.0; // (when 0 or more: the heart thins or thickens to this by the end of its life, so old smoke loses its dark core)
		double Inner = 0.5; // (where the heart ring sits, as a share of the radius: further out, a denser body with a thinner soft edge)
		double FeatherPx = 0.0; // (over 0: its edge fades over at least this many pixels on the screen however far the eye is, its heart ring brought in to suit)
		// (lumps on a lump, an oil column's puff: at least LumpsPx pixels across on the screen, it carries Lumps smaller
		// lumps of its own on its face, their sizes dealt between LumpsR0 and LumpsR1 of its radius, one step of that
		// range to each by its own seed, so no two puffs carry the same pattern; each is set LumpsOut0 to LumpsOut1 of the way
		// from its middle to where the lump would touch its edge, turning with it and as thick as LumpsAlpha of it;
		// each takes LumpsLight of its light from its own sunward side and keeps LumpsRim of it at its rim, so its edge
		// is dark on its shadow side only; LumpsInner over 0 is where its own heart ring sits, so its edge is firmer
		// than the puff's. A big puff near the eye is then a billowing mass and never one soft disc. 0: none)
		int Lumps = 0;
		double LumpsPx = 0.0, LumpsAlpha = 1.0, LumpsLight = 0.6, LumpsRim = 0.8, LumpsInner = 0.0;
		double LumpsR0 = 0.3, LumpsR1 = 0.45, LumpsOut0 = 0.3, LumpsOut1 = 1.0;
		// (a tongue of flame's root, other than 0: its fan is drawn from a point this share of its half length back from
		// its middle along Along (ahead of it when under 0), so its own colour sits there, at its root, and runs out
		// evenly to its edge's colour along it and across it. Held to 0.9 either way)
		double Hub = 0.0;
		double Smear = 0.0; // (an aircraft trail's puff: drawn this many metres longer behind it, against Along, so puffs metres apart join in one line)
		double SmearFor = 0.0; // (and that length shrinks to under a third over this many seconds, so an old trail breaks into lumps; 0 keeps it)
		// (a ribbon trail's piece, Ribbon over 0: a length of ribbon from its own point back to the piece laid before it,
		// wherever that one has drifted to and as wide and as dark there as that one is now. Its far edge is that
		// piece's own near edge, so the two share it exactly, as the links of one strip: no thin tips, no step, no
		// overlap to darken and no gap, however fast the aircraft flew. Back: the piece behind, as it was laid (Way:
		// the way the aircraft flew there, which the strip runs along at that edge, so it bends through its pieces as
		// the path did and never turns a corner at a join; Alpha: its own opacity; LumpsFrom: its own SmearHold). Ribbon: the share of its half
		// width that is its full dense core; from there it thins to nothing at its two sides. ShowFrom, ShowBy:
		// seconds between which it comes into sight (a length that hangs on, as the short ones it stands in for go).
		// SmearHold: the share of its life it stays a link; after that it draws in to a round lump of its own)
		struct Reach
		{
			Vec From, Vel, Way;
			double Rise = 0.0, Born = 0.0, R0 = 0.0, RMid = 0.0, R1 = 0.0, Grow = 1.0, Life = 1.0, Alpha = 1.0, LumpsFrom = -1.0;
			double GrowBy = 0.0, GrowEarly = 1.0; // (that piece's own widening clock: a trail may change kind at this join)
			double WakeBy = 0.0, WakeR = 0.0, WakeOn = 0.0; // (and its own spread in the aircraft's wake: see Bit::WakeBy)
			Rgb Col, Col2; double Cool = 0.0; // (and its own colour, young and old: a piece with bSoft runs to it along its length)
			double PaleTo = -1.0; // (0 and over: how far that piece pales with age, its own PaleTo, 1 not at all; under 0: as the piece reaching back to it does)
			double Lean = 0.0; // (and how much thinner its one side is: see Bit::Lean)
			double CutAt = 1.0e18; // (and that piece's own cut: see Bit::CutAt)
			bool bOn = false;
		};
		double Ribbon = 0.0, ShowFrom = 0.0, ShowBy = 0.0, SmearHold = -1.0;
		Reach Back;
		bool bSoft = false; // (a ribbon's piece: its colour runs along it to the colour of the piece behind, so no piece shows as a block)
		double Torn = 0.0; // (a ribbon's piece: its two sides wobble in and out by this much more, a torn outline; edge_torn)
		double Lean = 0.0; // (a soft ribbon's piece, -1 to 1: thinner by this much on the side it leans from, running to the piece behind's along its length, so the column has lighter folds along it and is never one even tube)
		// (a ribbon's piece widens in two stages, GrowBy over 0: from R0 to RMid over its first GrowBy seconds, by the
		// power GrowEarly (under 1: fast at first, as smoke spreads in an aircraft's wake), then on to R1 by the end
		// of its life, by Grow. And it pales as it widens, PaleBy over 0: as dark as it was laid until PaleFrom
		// seconds, then paler, PaleTo as dark from PaleBy seconds on. The short pieces and the lengths that hang on
		// under them pale by the one clock, so the young trail stays as black as it was. Art director 15:22Z)
		double GrowBy = 0.0, RMid = 0.0, GrowEarly = 1.0;
		double PaleTo = 0.0, PaleFrom = 0.0, PaleBy = 0.0;
		// (an engine's smoke spreads in the aircraft's wake, WakeBy over 0: by WakeBy seconds, about when the tail passes,
		// it is at least WakeR across its half width, fast at first, however slowly the row widens it after. Art
		// director 15:38Z: 1.5 m at the nacelle, 5 to 6 m past the tail)
		double WakeBy = 0.0, WakeR = 0.0;
		double WakeOn = 0.0; // (over 0: past WakeBy it keeps widening, as WakeR times this power of its age over WakeBy: a trail flares on behind the tail and is never a tube)
		// (a far trail's short piece, game.air_far: until the bar over it is laid (its trail has laid more than StandBars bars),
		// it stands in for that bar too, as dark as the two would be together, by StandA, coming in between StandFrom and
		// StandBy seconds as the bar would. A far bar is laid many seconds after the short pieces at its old end, and the
		// trail there went pale waiting for it; the art director's mend of 18:44Z)
		double StandA = 0.0, StandFrom = 0.0, StandBy = 0.0; unsigned StandBars = 0;
		bool bFarLong = false; // (a far trail's piece, game.air_far: many times longer than it is wide, so its line bows between its two ends no more than its own width, or its outline folds over itself and a sliver is drawn twice)
		bool bShadeTwin = false; // (game.from_above: not a puff but its shadow on the ground, laid by LayShadows from the puff itself, where the sun's line through it meets the ground)
		bool bShowEach = false; // (a far trail's bar, game.air_far: it comes into sight point by point along it, each point by the age of the trail there, not by the ages at its two ends alone; a bar many seconds long comes in as the short ones it stands in for go, all along it)
		double Billow = 0.0; // (a far trail's long piece, game.air_far.trails_billow: its two sides swell and pinch together by up to this share of its width at each point along them, its own, so one long piece is as lumpy along its length as the several short ones it stands in for; its ends as laid)
		double EdgeSoft = 0.0; // (over 0: its lumpy outline is gentler the wider the piece is for its length, so a wide old length has soft swells, never a comb of spikes: all of it up to a width of its length over EdgeSoft)
		double LeastPx = 0.0; // (a streak: never thinner than this on the screen; a puff: never smaller across its long way, and thinner to match)
		double LeastKeep = 0.0; // (a puff held up to LeastPx keeps at least this share of its opacity: a light's twinkle, a far trail's dark line)
		double MostPx = 0.0; // (a tracer's streak: never wider than this on the screen, so one passing close stays a thin line; 0: no limit)
		double ShadeK = 1.0; // (how strongly it is lit on its sunward side and shadowed on the other: a skirt's lit top and dark foot)
		// (over 0, and not a light: its side away from the sun is ShadowCol, this share of the way at its rim and three
		// fifths of that at its heart ring, so white steam has a cool shadow side of its own, darker than ShadeK's)
		Rgb ShadowCol;
		double ShadowK = 0.0;
		double Rough = 0.0, Turn = 0.0, TurnRate = 0.0; // (a clod: how lumpy its outline is, and how it tumbles)
		uint32_t LumpSeed = 0;
		// (smoke: the middle of its cloud, so a puff on the cloud's sunward side is lit as a whole and one on the far
		// side is in its shadow; the middle drifts with the wind and climbs at CentreRise)
		Vec Centre;
		double CloudShade = 0.0, CloudR = 1.0, CentreRise = 0.0;
		// (smoke seen with the sun behind the eye shows its sunlit face: FaceLit is how far it then goes toward LitCol, most
		// on its cloud's sunward side and least on the far side; none seen side-on or against the sun, nor at night)
		double FaceLit = 0.0;
		Rgb LitCol;
		// (a burst's thrown earth in the light of its own flash, Battle::InFlash. GlareK over 0: strongest GlareFor
		// seconds before GlareEnd and nothing by it, the piece goes toward GlareCol on its side toward GlareAt, the
		// flash: the most on the pieces nearest the flash (half as much GlareReach metres from it) and on those the eye
		// sees from the flash's side; a face turned from the flash keeps its own dark. bNeverUnder: no point of the
		// piece is drawn darker than NeverUnder in the hour's light, so thrown earth is dark brown, never a black cut-out)
		double GlareK = 0.0, GlareFor = 0.0, GlareEnd = 0.0, GlareReach = 1.0;
		Vec GlareAt;
		Rgb GlareCol, NeverUnder;
		bool bNeverUnder = false;
		double TwinPx = 0.0; // (at least this many pixels across, drawn as two fans, the second turned half a step: twice the sides, so no facet shows; 0 never)
		Rgb Col, Col2;
		bool bLight = false; // flame, flash, sparks, tracer: keeps its own colour by night and takes no shading
		mutable bool bShown = false; // (laid at least once: a light shorter than a frame is kept until it has been)
		bool bWhole = false; // (a light: however late its first frame comes, that frame shows it whole, as one shorter than a frame is)
		uint8_t Glow = 0; // (a tracer's streak: 1 its body, 2 its pale core; by day both widen and the core whitens, to hold on a pale sky)
		bool bHug = false; // rides the ground as it moves (skirts)
		bool bKeep = false; // (an oil fire's column: never left out when the screen is full, the near ones going first)
		bool bWisp = false; // (Effects (R2), -TheaterLandBarrelSmoke: a barrel's wisp, in its own room (after_wisp.most), not counted in the room the gates read)
		double SunSide = 0.0; // (an oil column: its sunward side lifted toward LitCol, its far side kept dark, wherever the eye is)
		double AxisWindage = 0.0; // (and the column's own axis: from Centre, carried by the wind at this share, so a puff knows which side of it it is)
		int OilId = -1; // (a puff of an oil fire's column: the fire's id, so the column's width at its height is found each frame)
		double StretchBy = 0.0; // (over 0: Stretch is reached only by this age, from round at birth: a pall drawn out as it runs)
		// (with StretchBy, a spout's water: as long as StretchFrom until it is StretchAfter seconds old, and only then drawn
		// out to Stretch, over StretchBy seconds: thrown up as a jet, it comes down as a curtain)
		double StretchFrom = 1.0, StretchAfter = 0.0;
		double SunYoung = 1.0, SunFrom = 0.0, SunBy = 0.0; // (SunBy over 0: its sunward side is lit only SunYoung as much until SunFrom seconds old and in full by SunBy, so smoke is darkest where it leaves the fire)
		// (a thrown clod: true flight under gravity, landing at Land seconds and once more at Land2 after one bounce)
		bool bThrown = false;
		double MostLong = 0.0; // (a thrown lump: drawn no longer than this times its width, in place of burst.debris.clods.most_long; 0: that)
		bool bChip = false; // (grounds.<ground>.chips: a stone splinter flung low and fast, its face catching the sun as it turns, Glint)
		bool bFarCull = false; // (a land hit's piece, burst.far: past gone_beyond_m and under gone_px across at its biggest, it takes no place and is not laid)
		double LensNear = 0.0; // (game.lens_cap: its burst's own near_m, sizes.<size>.lens_near_m, where that is more than the cap's)
		double Land = 1e9, Land2 = 1e9;
		Vec Vel2, At1, At2;
		// (a piece laid by a fire, a steam vent or a cook-off that rides a moving ship, BurnMove. bCarried: it was born
		// with Carry of the ship's velocity, the share the air has not got of it, and keeps it for CarryFor seconds
		// (a flame is short lived; a thrown piece keeps all of it until it lands). Ride other than 0: it lies on the
		// ship itself, an ember on her deck, and is moved with that fire's seat)
		Vec Carry;
		double CarryFor = 0.0;
		bool bCarried = false;
		uint32_t Ride = 0;
		// (a falling aircraft's piece: its other flat way at birth, square to Along; how fast it tumbles about Along,
		// radians a second; how strongly it catches the sun)
		Vec Face;
		double Tumble = 0.0, Glint = 0.0;
		// (a light's clump of fire: its edge goes EdgeK of the way to Edge, so a hot middle darkens out to a smoky rim
		// and the clump reads as a lump of fire, never a flat cut-out; 0 keeps one colour across it)
		Rgb Edge;
		double EdgeK = 0.0;
		double EdgeIn = 0.25; // (and how much of that its heart ring takes: more, and the colour turns sooner from its middle)
		double DayAlpha = 1.0; // (by day its opacity is this share of what it is at dusk: a flash's glow, thin in daylight)
		double Over = 1.0; // (a light: how far past white it burns, up to picture.lit_gain; see Battle::PictureGain)
		double LitOver = 1.0; // (not a light: how far past white its lit colour may be written, up to picture.lit_gain: a far sea splash's white water held over a pale sky, SeaSplash. 1: never)
		double Nearer = 0.0; // (sorted as if this many metres nearer the eye: a fire's white heart drawn over its own clumps)
		// (dust that may hide a unit: from a high steep eye it thins near the units, so a player always finds his own.
		// 1: a vehicle's trail, near any vehicle laying dust; 2: a gun's blast veil, near Origin, the gun)
		uint8_t Thin = 0;
		// (a lump of dust or smoke that rests on the ground, over 0: its middle is never lower than this share of its own
		// radius above the ground under it, wherever it drifts, so the ground never cuts it along a straight line)
		double Rest = 0.0;
		// (dust born low, LowBy over 0: until LowFrom seconds old it lies along the ground as a mound LowFlat times as high
		// as it is wide, as wide again as keeps its bulk, with LowLit of its light and shade (0: its own plain tone, no
		// lit top and no shadow side), its heart ring LowSoft of the way out it will sit (under 1: a young skirt with a
		// soft edge); by LowBy seconds it has risen round, firmed, and takes its light and shade in full. The shape: a
		// plain round puff only)
		double LowFlat = 1.0, LowFrom = 0.0, LowBy = 0.0, LowSoft = 1.0, LowLit = 0.0;
		double FootClear = 0.0, FootK = 0.0; // (a resting puff takes FootCol, FootK of it, while it lies on the surface, and its own colour by FootClear of its radius clear of where it rests; 0 never)
		Rgb FootCol;
		// (a tongue of flame that stays on its root, Anchor over 0: drawn this share of its own long half-way ahead of
		// where it is, along Along, so that as it grows in and narrows its root stays where it rises from and never
		// floats off it as a loose card. 0: drawn about where it is, as ever)
		double Anchor = 0.0;
		// (a flame's flicker, FlickerSeed other than 0: by the clock, never by its age, three waves between FlickerLo and
		// FlickerHi a second, their phases from the seed, so every piece of one seat of a fire flickers together. Its
		// opacity goes Flicker more or less, its length FlickerLong and its width FlickerWide, as shares: the fire's
		// shape and light change smoothly every frame and never step. 0: none)
		uint32_t FlickerSeed = 0;
		double Flicker = 0.0, FlickerLong = 0.0, FlickerWide = 0.0, FlickerLo = 3.0, FlickerHi = 6.0;
		// (fx-air r2: a ship's light guns and a burning ship's column. Every one 0, as before, unless its recipe key asks.
		// Hose over 0: a ship gun's hose, a thin line from this round (From, Vel, Born) back to the round fired before
		// it (Back's From, Vel, Born), laid where both are now, Hose its opacity. BurnOut over 0: a ship tracer's own
		// reach, air_guns.<kind>.burn_out_m: from DwindleFrom metres flown it thins to DwindleTo of its strength and
		// narrows to DwindleWide of its width by BurnOut, where it is out. TailFade: its tail end that much fainter than
		// its head, evenly along it. AlongK over 0: a hose's line as laid, its opacity carried along with those)
		double Hose = 0.0, BurnOut = 0.0, DwindleFrom = 0.0, DwindleTo = 1.0, DwindleWide = 1.0, TailFade = 0.0, AlongK = 0.0;
		// (PaleUpM over 0: an oil column's puff, oil_fires.<kind>.pale_up_m. By its height over PaleUpZ, as a share of
		// PaleUpM to the power PaleUpPow, it goes PaleUpMost of that share of the way to PaleUpCol, its sunlit side to
		// PaleUpLit, and thins by ThinUp of that share: black and dense low, browner grey and thinner high)
		double PaleUpM = 0.0, PaleUpMost = 0.0, PaleUpPow = 1.0, ThinUp = 0.0, PaleUpZ = 0.0;
		// (the production lead's FX2: its number in the order the battle made its pieces, given at the first Draw after
		// it was made. Prune fills each gap with the last piece, so Bits keeps no order; two pieces equally deep are
		// drawn in this order, the older first, as they were when Bits kept the order they were made in)
		uint64_t Serial = 0;
		// (an aircraft trail's piece: the trail's run, Trailing::Run, and the moment by which it is gone however long its
		// life, CutAt. The pre-check on 13388a4f: an aircraft that speeds up lays pieces of a shorter life, longest_m, and
		// those went before the slow pieces laid before them: a hole in the trail with an old length hanging alone
		// behind it. So no piece of a run outlives a piece laid after it: it is cut to that one's end, fading over the
		// last game.trail_cut_fade_s before it, and the trail goes from its old end)
		uint32_t TrailRun = 0;
		double CutAt = 1.0e18;
		Rgb PaleUpCol, PaleUpLit;
		// (the far form, Battle::FarLay: 0, anything else, drawn as ever. 1, a piece of a near burst that has a far form
		// too (a bomb of a stick), drawn while FarAt, where it struck, is within the draw range; 2, a piece of that far
		// form, drawn beyond it; the two cross over far.hand_over of the range. 3, a piece of a far form with no near
		// burst behind it, drawn wherever it is within far.draw_beyond_m)
		uint8_t FarRep = 0;
		Vec FarAt;
	};

	class Battle
	{
	public:
		/** The ground's height (metres) at X, Y (metres). */
		using GroundFn = double (*)(void* Context, double X, double Y);

		void Setup(const Recipe* InRecipe, GroundFn InGround, void* InContext, uint32_t Seed);
		void SetWind(const Vec& MetresPerSecond)
		{
			// (a part of the wind that is not a number, or is past any gale, counts as still air)
			const auto Kept = [](double V) { return V == V && V >= -200.0 && V <= 200.0 ? V : 0.0; };
			Wind = Vec(Kept(MetresPerSecond.X), Kept(MetresPerSecond.Y), Kept(MetresPerSecond.Z));
		}
		/** The ground at X, Y (metres) as the game's SCREEN shows it there, each channel 0 to 255; false where the game
		 * cannot say. */
		using ColourFn = bool (*)(void* Context, double X, double Y, double& R, double& G, double& B);
		/** The ground's colour on the screen, where the game can say: on dry ground a gun's dust takes it (muzzle.dry) in
		 * place of the recipe's paint, written to land on it (picture.to_land), so the dust is the colour of the ground
		 * it rose from. Asked once a shot, never a frame. */
		void SetGroundColour(ColourFn Fn, void* Context)
		{
			ColourAt = Fn;
			ColourContext = Context;
		}
		/** How far off a puff is still drawn (game.draw_beyond_m, 1500, by default): an air scene, whose strategy camera
		 * stands 800 to 2,000 m off the fight, draws farther (air.draw_beyond_m). Kept oil columns keep their own. */
		void SetDrawBeyond(double Metres) { Beyond = Metres > 1.0 ? Metres : 1.0; }
		/** The sky or haze that stands behind the battle's smoke as the game's SCREEN shows it at this hour, each channel
		 * 0 to 255 (as SetGroundColour's ground). Smoke that lingers pales toward a shade under it (linger.sky_share of
		 * it, 0.9), written to land on that (picture.to_land), as it thins: so it goes into the air and never stands on it
		 * as a tinted ghost. Call it again when the sky or the hour changes. Not called, or given no colour: the recipe's
		 * own linger.to by the hour's light. */
		void SetSky(double Red, double Green, double Blue);
		double AirBeyond() const { return Num("air.draw_beyond_m", Beyond); }
		/** A sea battle's distance (sea.draw_beyond_m, 16 km; without that row the air's): its gun actions are fought
		 * 5 to 15 km from the eye. */
		double SeaBeyond() const { return Num("sea.draw_beyond_m", AirBeyond()); }
		/** For a sea battle, in place of SetDrawBeyond(AirBeyond()): draw out to SeaBeyond(), and from now on hold what
		 * is far off to the sea row's floors (sea.flash, sea.column), so the far line is seen from 5 to 15 km: a gun's
		 * flash or a hit's a bright point with a little glow, a shell's column a small white pillar. Near, where each
		 * is bigger than its floor, nothing changes. And beyond the air's distance, where nothing used to be drawn, a
		 * piece of smoke or dirt that would stand under sea.gone_px across takes no place, so the room goes to what can
		 * be seen. A recipe without the sea row: the air's distance, no floors, nothing left out. */
		void UseSea();
		/**
		 * For a scene whose camera stands among the bursts (the land battle; the air and sea scenes never call it, and
		 * keep what they had). A puff is laid flat to the lens, so one whose middle is level with the lens or a little
		 * in front of it, a few metres to the side, is laid across the whole lens: a 105's smoke 12 m to the side of a
		 * man's shoulder was a huge dark soft disc hanging in the sky. Here smoke and dust (never a light, a thrown
		 * clod, a trail or a kept column) thin out by how far in front of the lens their near side is (the depth of
		 * the middle less half the size, the long way counted): nothing of one within Gone metres of the lens, all of
		 * it from Whole on; and no puff of any kind is laid whose middle is behind the lens or within Plane metres in
		 * front of it. The fade by distance (game.near_eye_fade_m) holds as before. Gone and Whole 0: off, as made.
		 */
		void SetLensFade(double Gone, double Whole, double Plane = 0.3);
		/** Whether a shell leaves its flat painted crater (Scar) in the kit's own mesh: on as made. A scene that lays
		 * craters of its own in the ground (the land battle's torn earth) turns it off; nothing else of a shell changes. */
		void SetScars(bool bOn) { bScars = bOn; }
		/** The scene grades its picture as the land battle does (most of the colour out of the darks and the middles,
		 * contrast 1.42: tools/battle-fx/land_grade.py), not by the game's own curve the recipe's colours are written
		 * for. With it the recipe's land_grade row stands in for what it names: colours (the clods' paint, the tracer's)
		 * and the clods' shade and tint, written to land in that grade as the art director ruled them (cut 5: a clod
		 * earth brown about 70,58,45 with its form, never a black silhouette; a yellow tracer yellow, not khaki). Off as
		 * made and at every Setup (a kit set up again for another scene is off until that scene asks); without the row
		 * nothing changes. */
		void SetLandGrade(bool bOn);
		/** The scene's material fades every piece where it meets the ground or a hull, over Metres (the land battle's,
		 * FBattleEffects::SetSoftContact: the art director's cut 5, still 07, a burning wreck's flame cut along a
		 * straight line where it met the hull). A piece lying on the ground (a thrown clod or fragment come to rest, a
		 * spent case, a crater's mark) would fade with it, so it is drawn game.soft_contact.lie_nearer times Metres
		 * nearer the eye along its own line of sight and smaller to match: the same picture, every point of it that far
		 * in front of what it lies on, so it stays whole. 0: off, as made and at every Setup. */
		void SetSoftContact(double Metres);
		/** For a scene seen from high above the ground: the land battle's general, 300 m up (the art director's read of
		 * the general's stills, item 5, 22 November: from up there smoke was a flat dark smudge). game.from_above: as the
		 * eye climbs past eye_m and looks down more steeply than steep, smoke's face toward the sun is lit toward
		 * lit_colour, most on each puff's sunward side and on its cloud's, and its big puffs lay their shadows on the
		 * ground away from the sun, each following its puff as it drifts, grows and thins. A low eye sees nothing
		 * changed. Off as made and at every Setup; without game.from_above.on nothing changes either. */
		void SetFromAbove(bool bOn);
		void Clear();

		/** The size row (sizes.<kind>) a calibre from the field battle draws (from_shell_cal): "howitzer" -> "how105". */
		std::string SizeOfCalibre(const char* Calibre) const;

		/** A shell, mortar bomb, grenade or bomb landing at At at Now. Size is a sizes row ("how105"), Ground a grounds
		 * row ("earth", "sand", "snow", "mud", "town", "woods", "water"). On armour: sparks and a grey puff, no earth. */
		void Shell(double Now, const Vec& At, const char* Size, const char* Ground, bool bOnArmour = false);
		/** An armour-piercing round that misses: a low spurt of earth and a skip, no crown and no crater. */
		void Skip(double Now, const Vec& At, const Vec& Heading, const char* Ground);
		/** A gun firing, its muzzle at At, pointing along Facing. Gun is a muzzle row ("tank_gun", "mg", "howitzer").
		 * Ground (a grounds row) is what its blast throws up: on sand a tank or field gun's shot raises a veil of dust
		 * in front of it that hides the gun and its line of sight for a second or two, then lingers. */
		void Muzzle(double Now, const Vec& At, const Vec& Facing, const char* Gun, const char* Ground = "earth");
		/** Dust thrown up behind a vehicle on dry ground: every frame, for each of its dust sockets (dust.left,
		 * dust.right), where the socket is and the vehicle's velocity (metres a second). Id: any number that stays the
		 * same for that socket. Ground is a grounds row; snow, mud and water throw none (dust.vehicles.by_ground).
		 * Low and long, drifting on the wind (SetWind), lingering; taller and longer at speed; tracks throw more than
		 * wheels. Standing still, nothing; a socket not called for a while is forgotten. */
		void VehicleDust(double Now, int Id, const Vec& At, const Vec& Velocity, const char* Ground, bool bTracked = true);
		/** Rounds fired by one gun (Gun: any number that is the same for the same gun) from From toward To, PerSecond:
		 * every fifth is a tracer (tracer.one_in), out of step between guns; machine guns flicker at the muzzle. */
		void Shots(double Now, int Gun, const Vec& From, const Vec& To, const char* Weapon, int Rounds, double PerSecond, const char* Ground = "earth");
		/** A gun on an aircraft firing, called every frame while its trigger is down. Muzzle is where the gun is now;
		 * Aim the point this gun is laid on (the battle decides lead and convergence, so wing guns converge where it
		 * says); Carry the aircraft's own velocity (metres a second). Kind is an air_guns row ("rifle", "hmg",
		 * "cannon20"); RatePerMin and MuzzleMps of 0 take the row's. The kit keeps each gun's next round and lays the
		 * rounds fired since the last call along the muzzle's path, so the stream stays on the gun however the
		 * aircraft turns. Every round flickers at the muzzle; one in five (the row's tracer_every) is a tracer, the
		 * same streak as on the ground, flying at the gun's speed plus the aircraft's, slowing and falling.
		 * Nation (air_guns.tracers.nations: "us", "gb", "de", "it", "jp", "su"; empty or unknown, the row's default)
		 * is whose gun it is: with air_guns.tracers in the recipe the tracer burns that nation's colours, a hot core
		 * in a narrow glow, a streak one thirtieth of a second of its flight long that shortens as it slows, dimming
		 * over its last third and out in two or three frames; a cannon's is half as wide again. */
		void AirGun(double Now, int Gun, const Vec& Muzzle, const Vec& Aim, const Vec& Carry, const char* Kind, double RatePerMin = 0.0, double MuzzleMps = 0.0, const char* Nation = "");
		/** The trigger let go: the gun's next AirGun starts a new burst (its belt goes on where it was). */
		void AirGunStop(int Gun);
		/** Rounds striking an aircraft at At: Normal the skin's outward way there, Carry that aircraft's velocity (m/s),
		 * Kind the air_guns row of the gun that hit. A bright spit with sparks, a dark scrap or two torn off and left
		 * behind in the air, a puff of smoke; all bigger for cannon (the row's strike_m). */
		void AirHit(double Now, const Vec& At, const Vec& Normal, const Vec& Carry, const char* Kind);
		/** A trail from a moving point on an aircraft (an engine): TrailStart once, TrailMove every frame with where the
		 * point is and its velocity, TrailStop when it ends; what is already in the air fades on its own. Id: any
		 * number that stays the same for that trail. Kind is an air_trails row: "smoke" (thin and grey, thickening and
		 * darkening), "fire" (flame at the root, black smoke), "glycol" (a thin white vapour). TrailStart on a running
		 * trail changes its kind (smoke catching fire). "fuel" is a tank alight, the flamer: fire streaming back from
		 * the point past the tail, as long as the row's stream_lengths says for Look (0 to 3, the event's look; under 0
		 * the second), into dense black smoke. "mist" is fuel spraying white from a torn wing root.
		 * "funnel" is a ship's funnel smoke (a row with plume keys): SpanM is the funnel's width at its top, Carry the
		 * ship's velocity, and TrailMove's Power the share of her top speed she is making, 0 to 1 (under 0: not given,
		 * and her speed over the row's top_speed_mps stands in). Its puffs are laid by time into the air she steams
		 * through, so a stopped ship smokes on, straight up; they keep none of her speed. */
		void TrailStart(double Now, int Id, const Vec& At, const Vec& Carry, const char* Kind, double SpanM = 0.0, int Look = -1);
		void TrailMove(double Now, int Id, const Vec& At, const Vec& Carry, double Power = -1.0);
		void TrailStop(int Id);
		/** A falling aircraft's debris row (art director 04:40Z). Kind: "explode" sheds every piece at once with a short
		 * torn fireball; "burn" sheds 5 to 10 over its fall; "spin" a few, sometimes a wing. Mostly light torn skin plates
		 * that flutter and glint, with a few heavy dark pieces that drop straight. AirFall once as the fall starts, with
		 * the aircraft's span (20 m or more sheds a bomber's row) and DeckZ, the height where pieces end: the cloud
		 * deck's top on a day with one, else the ground's; they fade into it, never popping out. Then AirFallMove every
		 * frame with where the model is, and AirFallStop when it is gone. Id: any number that stays the same for that fall.
		 * With air_debris.explode in the recipe an explosion is the art director's of 15:02Z 20 November: a ragged fire
		 * cloud as wide as its Look says (0 to 3, the event's look; under 0 the second), black smoke out of it that
		 * hangs, burning fuel streaks curving down, a sparkle of metal; bBombs: bombs aboard, bigger and whiter. Far off
		 * it keeps its flash and its cloud and throws nothing. With air_debris.ways the other ways of that ruling throw
		 * their own burst as they start: "wing", "tail", "breakup", "struck", "collide"; "pilot" and "glide" throw nothing. */
		void AirFall(double Now, int Id, const Vec& At, const Vec& Velocity, const char* Kind, double SpanM, double DeckZ, int Look = -1, bool bBombs = false);
		void AirFallMove(double Now, int Id, const Vec& At, const Vec& Velocity);
		void AirFallStop(int Id);
		/** A flak shell bursting at At, at the bombers' height: a soft flash of about 0.2 s off-centre in five to eight
		 * see-through lobes of smoke in a crooked chain, longer along the wind, with thin wisps trailing downwind; it
		 * hangs where it burst, drifts with the wind, greys and stretches, and is gone by about 9 s. Kind is an
		 * air_flak row ("heavy"). */
		void AirFlak(double Now, const Vec& At, const char* Kind = "heavy");
		/** A stick of bombs: Count bombs (none under 1, at most 48) of Size (a sizes row: "bomb250", "bomb500";
		 * unknown, how105) landing evenly along the line from First to Last on Ground (a grounds row; "water" for the
		 * sea; unknown, earth), the first at Now and the last Over seconds later (at most 30; under 0, or no number:
		 * the line's length at far.bombs.walk_mps, the bombers' speed over the ground, held to 4 s). The line is taken
		 * along the ground, at most 5 km of it. Each bomb lands at its own moment, laid by the next Draw on or after
		 * it. Within the draw range (SetDrawBeyond) it is the ordinary burst, as Shell; beyond it, with the recipe's
		 * far row, it is the far form (FarLay): a ripple of flashes running along the line and a few large soft pieces
		 * of the ground's own dust and smoke (white water on the sea), each held to a pixel floor, standing and
		 * spreading into one pall, drawn out to far.draw_beyond_m and fading as the near cloud does. A bomb laid near
		 * carries its far form too, drawn only once the eye is beyond the range, so a carpet never goes when the eye
		 * draws back; one laid far keeps its far form wherever the eye goes. Id: the stick's own number; a second
		 * call under an Id whose bombs are still to land is dropped. Without the far row every bomb is a Shell at its
		 * moment, as before. */
		void BombStick(double Now, int Id, const Vec& First, const Vec& Last, int Count, const char* Size, const char* Ground, double Over = -1.0);
		/** Something burning at At, about Across metres wide, flames for Seconds, then a thinning grey column. Id, 0 or
		 * more: the caller's own number for this fire, so BurnOut finds it (under 0: none). A fire lit later under the
		 * same number takes the number; the earlier one burns on as it was. bOnShip: a fire on a ship's deck (or
		 * anything afloat). It never takes a burning tank's look (fire.land_tongues, fire.land_column, the land lamp) and
		 * never joins another fire (fire.join), however small, low or still it is: the kit cannot tell a deck from the
		 * ground, and a scene's ground under a ship is her deck. The pre-check of 22 November. */
		void Burn(double Now, const Vec& At, double Across, double Seconds, int Id = -1, bool bOnShip = false);
		/** A burning opening (release 2, Brian 23 November: "the tank on fire doesnt look realistic"): flame and smoke
		 * forced out of a hatch or a grille Across metres wide at At, for Seconds, then a thinning grey column as Burn's.
		 * It is a fire as Burn's (put out by BurnOut, moved by BurnMove, burning down by fire.late) with three
		 * differences: it never joins another fire nor takes one in (fire.join), so a wreck's hatches burn as hatches
		 * and never melt into one bonfire on her deck; it never founds a column of its own (the deck's fire carries the
		 * wreck's column); and its flames are fire.vent_tongues where the recipe has that row (narrow, tall, roaring
		 * out of the hole), else fire.land_tongues as a small fire on land. Without fire.vent_tongues.every_s it is a
		 * Burn that does not join. */
		void Vent(double Now, const Vec& At, double Across, double Seconds, int Id = -1);
		/** Fuel going up at At (the recipe's fuel_ball; no such row: nothing): a ragged ball of burning fuel about
		 * Across metres wide that rolls and rises as it burns and turns to black smoke boiling off its top, the art
		 * director's ruling of 21 November: a vehicle's fuel 6 to 10 m for 1 to 1.5 s, an aircraft's crash or a fuel
		 * dump 15 to 25 m for 1.5 to 2 s, its time by its width. Velocity: what it was moving at (a crashing
		 * aeroplane's, a truck's), which the fire carries a little of. It lights the ground round it while it burns. A
		 * fire that burns on after it is the caller's Burn. */
		void FuelBall(double Now, const Vec& At, double Across, const Vec& Velocity = Vec());
		/** That fire's crew puts it out (the recipe's fire.put_out; no such row, or no fire burning under that number:
		 * nothing). Over Over seconds (0 or less: the row's own) its flames are eaten back from the edges to its seat,
		 * lower and sputtering, never thinner. How "water" (a ship's hoses): a surge of white steam at the seat for the
		 * first seconds; "extinguisher" (a tank's crew): a brief pale puff. Its smoke turns from black through grey to
		 * grey-white, thins to a wisp and is gone in about a minute; red embers glow where it burned for some seconds
		 * after the last flame. A big fire's own column burns on only while another of its fires does. */
		void BurnOut(double Now, int Id, double Over = 0.0, const char* How = "water");
		/** That fire is on a moving ship: once a frame, where its seat is now and the ship's velocity there (metres a
		 * second; none once she has stopped). New flame and smoke are born at the seat as it moves: what the air
		 * holds little of (a tongue's foot, its white-hot root) keeps the ship's way and stays on her, the rest is
		 * taken by the air the ship drives through, so the flames lean aft and the smoke streams astern; what is
		 * already in the air is left to the wind. Embers lying where it burned move with the seat. A big fire's own
		 * column moves with its fires. A number the kit does not know: nothing. A fire never moved stays where it
		 * was lit. */
		void BurnMove(double Now, int Id, const Vec& At, const Vec& Velocity);
		/** Steam let go by a ship's boiler or engine-room hit (or a vehicle's radiator), from a vent Across metres wide
		 * at At, blowing along Way (up, if that is no direction) for Seconds: a narrow white jet, clear for a moment at
		 * its mouth, that breaks into rags which evaporate within seconds; never a bank, never a flash. It roars at
		 * first, then its length and speed sink together as the pressure falls, to a wavering wisp. Kind is a steam
		 * row: "vent" (a deck vent, a skylight or a hole in her side: metres tall) or "funnel" (the escape pipes on the
		 * funnel's face as the safety valves lift: tens of metres, straight up). Id: any number that stays the same for
		 * that vent; a second call under it starts it afresh. No steam row in the recipe: nothing. */
		void Steam(double Now, int Id, const Vec& At, const Vec& Way, double Across, double Seconds, const char* Kind = "vent");
		/** That vent is shut, or blown dry: no new steam; what is in the air is gone in seconds. */
		void SteamStop(double Now, int Id);
		/** That vent is on a moving ship: once a frame, where it is now, the way it blows and the ship's velocity
		 * there (metres a second). Its steam is born at the vent as it moves and torn astern. */
		void SteamMove(double Now, int Id, const Vec& At, const Vec& Way, const Vec& Velocity);
		/** Ammunition cooking off inside a burning tank (or a ship's gun mount, or aircraft parked on a deck), seen at
		 * its hatch, At, Across metres wide, opening along Up: Pops sharp pops (held to 0..40) at ragged, quickening
		 * moments over Over seconds, each a small flash by the flash rule, a grey-black puff, orange embers in short
		 * arcs and now and then a dark fragment tumbling off; never a round fired skyward. Jet over 0: the propellant
		 * burning as a roaring ragged flame from the hatch for that long, in one or two pulses, black smoke rolling off
		 * its top; it starts JetFrom seconds after the call (under 0: after the last pop). The burning wreck after is
		 * the caller's own Burn. No cook_off row in the recipe: nothing.
		 * Id, 0 or more: the caller's own number for it, so CookOffMove finds it (under 0: none). */
		void CookOff(double Now, const Vec& At, const Vec& Up, double Across, int Pops, double Over, double Jet, double JetFrom = -1.0, int Id = -1);
		/** That cook-off is on a moving ship: once a frame, where its hatch is now, the way it opens and the ship's
		 * velocity there (metres a second). Its pops and its jet come from the hatch as it moves; embers and fragments
		 * are thrown with the ship's way on them. */
		void CookOffMove(double Now, int Id, const Vec& At, const Vec& Up, const Vec& Velocity);
		/** An armour-piercing shot glancing off armour at At: Incoming the way it flew, Normal the plate's outward way
		 * there (either may be rough: no direction counts as level and up). A small two-frame flash, sparks thrown in a
		 * narrow cone the way it glances, cooling as they fall, a wisp of paint and metal dust, and the shot itself
		 * going off glowing and tumbling at a shallow angle; where it comes down in sight it kicks up a little of the
		 * Ground. No burst, no smoke. CalibreMm sizes it (held to 20..460). No glance row in the recipe: nothing. */
		void Glance(double Now, const Vec& At, const Vec& Incoming, const Vec& Normal, double CalibreMm = 75.0, const char* Ground = "earth");
		/** Oil burning on the sea where a ship went down (Kind "sunk"), or a ship on fire where she left the line
		 * ("burning"): low flames across the slick and a tall black column leaning down the wind, kept even when the
		 * screen is full, so a far column on the horizon never blinks out. Reach is the oil's reach in metres (the sea
		 * battle's zone r); Until, when it stops (the fight's end; 0: until OilStop); Since, how long it has already
		 * burned, so a scene that opens later shows the column grown. Id: any number that stays the same for that
		 * fire. Kinds live in the recipe's oil_fires. */
		void OilFire(double Now, int Id, const Vec& At, double Reach, const char* Kind = "sunk", double Until = 0.0, double Since = 0.0);
		/** That oil fire goes out: no new flame or smoke; what is in the air thins away on its own. */
		void OilStop(double Now, int Id);
		/** A smoke screen where a smoke round came down (the drill book's screens, 22 November): at At, its burst, and
		 * then a low white wall laid from where it landed for Seconds, Across metres wide at its fullest, drifting down
		 * the wind (SetWind) with its lumps swelling as they go, so it stands as one screen through its middle with rags
		 * at its edges; after Seconds it is laid no more and thins away over its lumps' lives. Kind "wp" (white
		 * phosphorus, the American mortars' round): a hard white burst throwing burning streamers in a dome, each
		 * trailing its own white smoke and curling over as it falls, and the wall built from where they came down;
		 * "canister" (a burning smoke bomb's or a base-ejection shell's, the British and German): no burst, a gush of
		 * white from the ground that thickens into the wall over its first seconds. Never a crater, never a black or
		 * grey cloud. Its numbers are the recipe's smoke_screen row. Id 0 or more: the caller's own number for it.
		 * Kind "grenade" (release 2: a tank commander's smoke grenade covering his crew's bail-out; the art director, 24
		 * November): one canister gushing grey-white, 3 to 5 m across within 2 s, swelling to Across at the canister,
		 * densest there, and drawn down the wind into a plume 30 to 60 m long that widens and thins with distance; laid
		 * until the last 15 s of Seconds, then thinning out (smoke_screen.grenade). */
		void Screen(double Now, const Vec& At, double Across, double Seconds, const char* Kind = "wp", int Id = -1);
		/** A smoke screen laid at sea by a ship under way (the sea battle's escorts; nothing to do with Screen above, a
		 * smoke round on land). Every frame while she lays it: Id, her own number for this screen; At, where the smoke
		 * leaves her (her stern); Velocity, hers (metres a second). bFunnel: oily black smoke from her funnels (her
		 * boilers starved of air), born the row's from_up_m higher; else white chemical smoke from her stern generators.
		 * Laid by time into the air she steams through, it keeps none of her speed: dense and lumpy at her stern, it
		 * hangs low on the water, drifts and spreads down the wind (SetWind) and thins over minutes, with gaps opening
		 * in its old parts. Far off it is fewer, bigger puffs. A call that is no place or no moment is dropped; a
		 * velocity no number or past 150 m/s is counted as none; a clock that goes back or stalls past the row's
		 * restart_after_s starts it afresh from here. Its numbers are the recipe's sea_screen row; without it: nothing. */
		void ScreenLay(double Now, int Id, const Vec& At, const Vec& Velocity, bool bFunnel = false);
		/** She stops laying that screen: no new smoke; what is laid hangs, drifts and thins away on its own. Calling
		 * ScreenLay again under the Id starts a new screen. */
		void ScreenLayStop(double Now, int Id);
		/** A shell falling in the sea, by its calibre (CalibreMm, held to 20..600) and the dye its ship loaded (Dye: a
		 * name of the recipe's sea_splash.dyes, "pink", "red", "yellow", "green", "orange", "blue"; empty or unknown:
		 * white water). A column of white water as tall and as slow as the calibre throws it (sea_splash's table,
		 * after the art director's ruling: a 16-inch shell's 60 m, full height in about 2 s, standing 3 to 4 s and
		 * falling over 4 to 6 s), coloured through by the dye and paling toward its white crown as it falls, leaving a
		 * foam patch stained with it. Far off its white is held over the sky. No flash. Without the sea_splash row:
		 * Shell on "water" with the size row its calibre is nearest (bomb500, bomb250, how150), as before. */
		void SeaSplash(double Now, const Vec& At, double CalibreMm, const char* Dye = "");

		/** What the eye is: where it is, which way its screen's right, up and forward point, and how many pixels a
		 * radian across the screen is (the screen's width / (2 tan(field of view / 2))). Smoke takes SmokeLight (the
		 * hour: white by day); Sun is the way toward the sun. */
		struct View
		{
			Vec Eye, Right, Up, Forward;
			double PixelsPerRadian = 1000.0;
			Rgb SmokeLight = Rgb(1.0f, 1.0f, 1.0f);
			Vec Sun = Vec(-0.45, -0.35, 0.82);
			Rgb SunLight = Rgb(1.0f, 1.0f, 1.0f); // (the sun's own light, its colour and strength: none at night)
			// (how wide the view is: the tangent of half its angle across, and up (0: a quarter more than across). Given,
			// what is wholly off the screen is neither laid nor takes a place, so smoke behind the eye never crowds out
			// what is in front of it. 0 across: not known, and everything in range is laid, as before)
			double HalfAcross = 0.0, HalfUp = 0.0;
		};
		/** Lays everything alive at Now for this eye into Points and Colours (PerPuff points a puff, far to near).
		 * Returns how many puffs; the rest of the room up to the last frame's count is folded away. */
		int Draw(double Now, const View& Eye);
		/** Each puff is a middle and two rings of Ring() points (game.puff_sides, 16 unless the recipe says less), so
		 * PerPuff() points; it is fixed at Setup, so the mesh's triangles never change. */
		int Ring() const { return Sides; }
		/** What the picture must multiply the colours by (the material's brightness): picture.lit_gain, 1 by default. */
		double PictureGain() const { return LitGain; }
		/** How the colours go into the bytes (Colours()). false: the light itself, a byte 1/255 of it, as M_Highlight reads
		 * them. true: raised to 1/2.2 first, as M_BattlePuffs reads them (its Power of 2.2), so a dark smoke keeps its hue:
		 * in plain bytes a charcoal, written lit_gain times darker, is one to four steps a channel, and its red, green and
		 * blue round apart (a big far piece of dark earth read olive over grass, 55,59,37; the dusk specks). Set by the
		 * scene for the material it draws with, once, before the first Draw; Setup leaves it as it is. false by default. */
		void SetSrgbBytes(bool bOn) { bSrgbBytes = bOn; }
		bool SrgbBytes() const { return bSrgbBytes; }
		int PerPuff() const { return PointsPerPuff; }
		int Room() const { return MaxPuffs; }
		/** What the last Draw laid (puffs), and the room asked for as it stands: on the screen at that Draw, and what has
		 *  been made since (RoomAsked; over Room() when the room is short). For a scene's log, the production lead's ask
		 *  of 22 November. */
		int PuffsDrawn() const { return LastDrawn; }
		int PuffsAsked() const { return static_cast<int>(std::min<size_t>(RoomAsked(), 1000000000u)); }
		/** burst.far (FX3): land hits since Setup, those laid with fewer pieces, and the mean share of their pieces they kept. */
		int LandHitsMade() const { return static_cast<int>(std::min<size_t>(LandHits, 1000000000u)); }
		int LandHitsThinned() const { return static_cast<int>(std::min<size_t>(LandHitsFew, 1000000000u)); }
		double LandKeptMean() const { return LandHits > 0 ? LandFewSum / static_cast<double>(LandHits) : 1.0; }
		/** game.smoke_budget (FX9): the smoke the last Draw laid, in screens of fill (its pieces' areas on the screen,
		 *  each clipped to one screen, over the screen's), and the share of the big smoke near the lens left out then.
		 *  0 and 0 with the row off, or the view's width not given. For a scene's log, beside PuffsDrawn. */
		double SmokeScreens() const { return SmokeLaidScreens; }
		double SmokeThinned() const { return ThinShare; }
		const std::vector<Vec>& Points() const { return Out; }
		const std::vector<uint8_t>& Colours() const { return OutRgba; } // four a point: red, green, blue, alpha (linear)
		bool Busy(double Now) const;

		struct Lamp
		{
			Vec At;
			Rgb Col;
			double Strength = 0.0; // 1 is a 105 mm shell's flash
			double Radius = 30.0;
		};
		/** The strongest lamps lit at Now, at most Room of them. Day (DayOf the hour's light, 0 at night, 1 by day)
		 * turns them down to a share and toward a warm white, and puts out small guns' altogether. A flash's lamp is full
		 * on the first frame drawn after it, half on the next, then out: call it after Draw. */
		int Lamps(double Now, Lamp* Into, int Room, double Day = 0.0) const;
		/** The lamps by fire.lamp's rules (Lamps, when the recipe has fire.lamp.ramp_s): see the .cpp. */
		int SteadyLamps(double Now, Lamp* Into, int Room, double Day) const;

		/** The most lamps any scene may ask for (LampRule::Most); FBattleEffects makes no more point lights than this. */
		static constexpr int MostLamps = 4;
		/** How a scene's lamps are handed out (the sea's night fight, 25 November: look bible S7, "gun flashes light the
		 * hulls"). Set once by the scene at open with SetLampRule; as made, at every Setup, and with every knob at its
		 * default, the lamps are handed out exactly as before, to the byte.
		 *
		 * - Most: how many lamps the scene asks for in place of game.max_lamps (0: the recipe's), never over MostLamps.
		 *   The caller passes it on as Lamps' Room (FBattleEffects does); a scene that never asks makes none extra.
		 * - bByEye: each lamp is ranked by the light it sends the eye: its strength over the square of its distance from
		 *   the eye of the last Draw (5 m at the least: the floor fire.lamp's steady lights already use). A lamp's lit
		 *   patch, seen from afar, is as much dimmer per square metre as it is wider on the screen, so its reach cancels
		 *   and what is left is the light it lands over the distance squared: a burning slick 6 km off ranks under a
		 *   battleship's broadside 2 km off. Off: by strength alone, as before. The steady lights (fire.lamp,
		 *   SteadyLamps) keep their own share, game.steady_lamps of the lamps, and their ramp and keep; with bByEye the
		 *   rest go to flash or steady light by rank alike, and a steady light's own lamp goes, for the frames it is
		 *   outshone, to a flash left unlit that sends the eye Yield times its light (it comes back up over ramp_s).
		 * - MergeM, MergeS: flashes born within MergeS seconds of each other whose guns lie within MergeM metres of each
		 *   other light one lamp (a broadside: turrets fire within a fraction of a second, a hull's length apart), at
		 *   their lamps' strength-weighted middle, coloured as their strength-weighted mix, its strength their sum held
		 *   to MergeMost times its strongest flash's, its reach grown to cover each flash's own reach from there.
		 *   Strongest first; a flash joins a lamp only when its gun lies within MergeM of every gun already in it, so no
		 *   two guns of one lamp are more than MergeM apart and a lamp never chains from ship to ship. The distance is
		 *   the guns' (the muzzles the scene passed), never the lamps': a naval lamp hangs some 60 to 135 m out along its
		 *   bore (lamp_out_m), so two ships firing toward each other would bring their lamps together while their guns
		 *   stay as far apart as ever. A flash with no gun (any other row's) is measured from its lamp. MergeM 0: none
		 *   merged. The groups are made afresh each frame from the strongest flash alive, so in a ripple salvo (turrets
		 *   0.05 to 0.1 s apart) the lamp steps along her hull toward each turret as it fires, 25 to 85 m a turret, and
		 *   the earlier ones fade: it reads as the ripple. A salvo fired in one frame (the sea scene's) is one still
		 *   lamp.
		 * - Yield: see bByEye (only with it).
		 *
		 * With bByEye a lamp whose sphere (its place and its reach) lies wholly outside the last Draw's view
		 * (View::Forward, HalfAcross and HalfUp; not known while HalfAcross is 0) ranks nothing, as it can light nothing on
		 * the screen. Ties are broken by the lamps themselves (strength, birth, place, reach, colour), never by the order
		 * the calls came in: the same flashes give the same lamps to the bit, however their Muzzle calls were ordered.
		 *
		 * The sea battle's night (the sea scene, once at open, before Begin or after it):
		 *     Effects->SetLampRule(TheaterFx::Battle::LampRule::Sea());
		 * which is { Most 3, bByEye, MergeM 200, MergeS 0.3, MergeMost 2, Yield 100 }. */
		struct LampRule
		{
			int Most = 0;
			bool bByEye = false;
			double MergeM = 0.0, MergeS = 0.0, MergeMost = 2.0, Yield = 100.0;
			/** The sea battle's: 3 lamps by the eye, a broadside within 200 m and 0.3 s one lamp at most twice its
			 *  strongest turret's (the sea's effects-lamps-for-sea.md says why each). */
			static LampRule Sea()
			{
				LampRule R;
				R.Most = 3;
				R.bByEye = true;
				R.MergeM = 200.0;
				R.MergeS = 0.3;
				return R;
			}
			/** Whether it asks anything of the kit at all (one that does not is the default: the lamps as ever). */
			bool Asks() const { return Most > 0 || bByEye || MergeM > 0.0; }
		};
		/** Held: Most 0..MostLamps, MergeM 0..2,000 m, MergeS 0..2 s, MergeMost 1..16, Yield 1..10^6; a knob that is not a
		 *  number takes its default. Survives Clear; Setup puts it back to the default. */
		void SetLampRule(const LampRule& Rule);
		const LampRule& LampRuleNow() const { return Rules; }
		/** The lamps a scene should make: the rule's Most, else game.max_lamps (2), never over MostLamps. */
		int LampsWanted() const;
		/** How much the hour's light (View::SmokeLight) is day: 0 at night, 1 in full day (game.day_light_from, _full). */
		double DayOf(const Rgb& HourLight) const;

		struct Jolt
		{
			Vec Offset; // metres
			double Roll = 0.0; // degrees
		};
		/** The camera's jolt at Now for an eye at Eye, from every hit close enough; nothing from far ones. */
		Jolt Shake(double Now, const Vec& Eye) const;

	private:
		/** A range from the recipe, read once before a loop, so a loop of puffs does not ask the recipe each time. */
		struct Span
		{
			double A = 0.0, B = 0.0;
			double Pick(Dice& D) const { return D.U(A, B); }
		};
		/** An air_guns row, read from the recipe once a kind. */
		struct AirRow
		{
			std::string Name;
			double Rate = 800.0, Mps = 870.0, Flash = 0.45, Strike = 0.8, Drag = 0.36, Drop = 9.81, TracerS = 2.4, Spread = 0.004;
			double SmokeS = 0.8, SmokeAlpha = 0.1, Lamp = 0.0, FlashLeastPx = 0.0; // (FlashLeastPx: the flash never under this on the screen, a twinkle far off)
			int OneIn = 5, SmokeEvery = 3, Licks = 0;
			// (the close view, art director 04:40Z: a heavy machine gun's flicker gains CloseLicks side licks and a lamp of
			// CloseLamp when the eye is within LicksWithin metres of it; every round drops its spent case and belt link
			// from the chute under the gun, CaseM long and CaseD across, a link LinkM, when the eye is within CasesWithin
			// metres. 0 for each: none, as before)
			Span CloseLicks{ 0.0, 0.0 };
			double CloseLamp = 0.0, LicksWithin = 0.0, CasesWithin = 0.0, CaseM = 0.0, CaseD = 0.0, LinkM = 0.0;
			double ChuteBack = 1.0, ChuteDown = 0.25, CaseDrag = 5.0, CaseGlint = 4.0, CaseLeastPx = 0.8;
			Span Throw{ 3.0, 8.0 }, CaseSink{ 18.0, 28.0 }, LinkSink{ 12.0, 20.0 }, CaseLife{ 1.2, 2.0 }, CaseTumble{ 15.0, 40.0 };
			Rgb CaseCol, CaseDark, LinkCol;
			// (art director 06:19Z: a hot flash, lit from within: a white-yellow root at the muzzle, a saturated orange body,
			// a fading tip, never peach. bHotFlash: the cannon flash in these colours; the close view's licks always)
			bool bHotFlash = false;
			Rgb FlashHot, FlashOrange, FlashRim; // (FlashRim: the deeper orange its edges go to, so a rim over the sky stays orange)
			// (the air look, with air_guns.tracers in the recipe: bCannon, a cannon (its tracer takes a nation's cannon
			// colours where it has them, and its hit is a flash, a puff and a fragment where a machine gun's is a
			// sparkle); TracerWide, how many times as wide its tracer is; TracerLifeK, how long it burns as a share of
			// air_guns.tracers.life_s; StreakS, its streak's length in seconds of flight, 0 the look's own)
			bool bCannon = false;
			double TracerWide = 1.0, TracerLifeK = 1.0, StreakS = 0.0;
			// (fx-air r2, a ship's light guns: Hose, HosePx, HoseMix, the hose from each round to the one before (see
			// Bit::Hose), its opacity, width in pixels and how far its colour goes from the glow's to the core's, HoseS
			// the least seconds between its points;
			// bShipHue, the nation's ship colours where it has them; BurnOut, DwindleFrom, DwindleTo, DwindleWide,
			// TailFade and CoreTailFade (the core's), see Bit::BurnOut. 0 or off: as before)
			double Hose = 0.0, HosePx = 0.0, HoseMix = 0.0, HoseS = 0.0, BurnOut = 0.0, DwindleFrom = 0.0, DwindleTo = 1.0, DwindleWide = 1.0, TailFade = 0.0, CoreTailFade = 0.0;
			bool bShipHue = false;
		};
		/** A ground gun's spent case (ground_cases.<gun>): its size and how it is thrown and lies. */
		struct CaseRow
		{
			double M = 0.08, D = 0.012, After = 0.0, Back = 0.0, Below = 0.0, Side = 1.0, Scatter = 0.25, Glint = 4.0, LeastPx = 0.8;
			Span Throw{ 2.0, 4.0 }, Up{ 1.0, 2.5 }, Lie{ 2.0, 4.0 }, Tumble{ 15.0, 30.0 };
			Rgb Col, Dark;
		};
		// (false: none for this gun, too far off, or no room. Ground, when given, is what the gun stands on: none on a
		// ground named in ground_cases.not_on, as the sea scene's turrets on "water" (Effects (R2), -TheaterSeaNoFalseCases))
		bool CaseRowOf(const std::string& Gun, const Vec& At, CaseRow& Out_, const char* Ground = nullptr) const;
		void GroundCase(double T, const Vec& Breech, const Vec& Away, const CaseRow& C, Dice& D);
		int LayCase(int Slot, const Bit& B, double Age, const Vec& P, double A, const View& Eye, int Room); // (a spent case's shadow, tube and mouth: the places it took)
		/** Effects (R2), -TheaterLandGunFlash (Brian, 23 November: "how does the gun fire look when they shoot? can we make
		 *  the mussel flash realistic?"): a gun's own look, muzzle.look.<look> (laid over from battle-fx-guns.json), named
		 *  after its row as "mg:star" in Muzzle and Shots. Every number has its row's or the old look's meaning when left
		 *  out; a name with no colon is drawn as ever. Its pieces have their own dice, laid after the row's. */
		struct GunLook
		{
			bool bOn = false;
			double FlameM = 0.0;            // flame_m: its flame's length (0: the row's)
			double Narrow = 1.0;            // narrow: its flame's width as a share of the row's (a flash hider's cone: under 1)
			double LicksK = 1.0;            // licks: its licks thrown sideways, as a share of the row's
			bool bFlicker = false;          // flicker: each round of a burst flickers at the muzzle (Shots), as a machine gun's
			double HotM = 0.0;              // hot_m: a round's close flame in a burst (0: muzzle.hot.mg_flame_m)
			double FarK = 1.0;              // far_k: a round's far flicker's size, a share of the machine gun's
			bool bTracer = true;            // tracer: 0, its rounds lay no tracer (a sub-machine gun's pistol rounds)
			int Star = 0;                   // star: spikes thrown out round the bore (the MG 42's star), 0 none
			double StarM = 0.0, StarWide = 0.12, StarLean = 75.0, StarFarM = 0.0; // star_m, star_wide, star_lean (degrees off the bore), star_far_m
			int Jets = 0;                   // jets: a muzzle brake's side jets, left and right (2), 0 none
			double JetM = 0.0, JetBackM = 0.15, JetWide = 0.2, JetDust = 0.0; // jet_m, jet_back_m, jet_wide, jet_dust (its dust thrown aside)
			double Haze = 0.0, HazeR = 0.6, HazeWithinM = 200.0; // haze (most opaque), haze_r, haze_within_m: a pale puff that hangs
			Span HazeS{ 2.5, 4.0 };         // haze_s
			double ProneUnderM = 0.0, ProneDust = 0.0, DustWithinM = 150.0; // prone_under_m, prone_dust, dust_within_m
			double LampW = 0.0, LampBackM = 0.4, LampR = 3.0, LampWithinM = 150.0; // lamp_w, lamp_back_m, lamp_r, lamp_within_m: its glow on the firer, dusk and night only
			// (the art director's round 1, 17:06Z: by day a flash barely shows: day_k its size in full day as a share of
			// dusk's, day_pale how far its colours go toward a pale yellow-white; streak 1, its tracer a short travelling
			// streak even close in, never the whole path at once)
			double DayK = 1.0, DayPale = 0.0;
			bool bStreak = false;
		};
		bool GunLookOf(const std::string& Look, GunLook& Out_) const; // (false: no muzzle.look.<look>.on in the recipe)
		double LookDay = 0.0; // (how much the last Draw's hour was day, DayOf its SmokeLight: a look's flash by day)
		double LookDayK(const GunLook& L) const { return 1.0 - (1.0 - L.DayK) * LookDay; }
		/** A look's own after-pieces: the haze that hangs, the dust a low muzzle or a brake kicks up, its lamp. */
		void LookLaid(double Now, const Vec& At, const Vec& Fw, const GunLook& L, const char* Ground);
		void LookLamp(double Now, const Vec& At, const Vec& Fw, const GunLook& L);
		/** A ground gun's flash close to the eye (muzzle.hot, art director 07:08Z): lit from within, a yellow-white root
		 *  and an orange body, as the air guns' close flash. */
		struct HotFlash
		{
			Rgb Root, Root2, Hot, Orange, Rim;
			// (art director 13:54Z, build 1720: the flame graded from its root out and ragged, new every round. Only when
			// the recipe has muzzle.hot.tongues: see HotFlame)
			bool bRagged = false;
			Rgb Yellow, Red;
			Span Tongues, Licks, Long;
			double Wide = 0.2, Glow = 0.0, GlowDay = 0.5, GlowM = 0.035, Over = 1.0;
			// (art director 16:30Z, build 1732: the root alone burns near white, so the yellow has its own number
			// (yellow_over; 0: a share of the root's, as before), and the root's glow is the kit's own soft halo, HaloM
			// past the root all round and Halo opaque at the root's edge, never the game's bloom)
			double YellowOver = 0.0, Halo = 0.0, HaloM = 0.0;
			GunLook Look; // (Effects (R2): a gun's own look's spikes and jets, laid after the rest; off, none)
		};
		/** Effects (R2): a look's own flame pieces, its star's spikes and its brake's side jets, lit as the close flash (H)
		 *  or, H null, as the far one. */
		void LookFlame(double Born, const Vec& At, const Vec& Fw, double Len, double Life, const GunLook& L, const HotFlash* H, Dice& D);
		/** Effects (R2): a look's close flash by day (the art director's round 1): its colours Pale of the way toward a
		 *  pale yellow-white, the root kept. */
		static void PaleHot(HotFlash& H, double Pale);
		/** A close gun flash as the art director ruled it from the game's own frames: a small white-yellow root right at
		 *  the muzzle, melting into a yellow then orange body widest a third of the way out, which breaks into ragged
		 *  tongues, red-orange only at their ends, with short licks thrown sideways and a thin glow round it. Its length
		 *  and every piece are rolled anew each call, so no two flashes match. Len: its usual length in metres. */
		void HotFlame(double Born, const Vec& At, const Vec& Fw, double Len, double Life, const HotFlash& H, Dice& D);
		/** A burst's close flames to come: each is laid when its round fires (Run), so a long burst never holds hundreds
		 *  of pieces waiting and the room is asked only for the one flame burning. */
		struct FlameDue
		{
			double Born = 0.0;
			Vec At, Fw;
			double Len = 0.5, Life = 0.025;
			HotFlash H;
			uint64_t Seed = 1;
		};
		std::vector<FlameDue> FlamesDue;
		bool HotOf(const Vec& At, HotFlash& Out_) const; // (false: no muzzle.hot in the recipe, or the gun past within_m of the eye)
		/** A dust.vehicles row for one ground, read once a ground. */
		struct DustRow
		{
			std::string Ground;
			double Share = 0.0, LeastMps = 1.0, FullMps = 12.0, Spacing = 2.5, MostPerS = 4.0, Kick = 0.1, Drag = 1.0;
			double Windage = 1.0, FlatSlow = 0.5, FlatFast = 0.75, HazeBy = 2.0, HazeAlpha = 0.2, Tracked = 1.3, Spread = 0.6;
			double LowShare = 0.5, LowFlatten = 0.35, LowWide = 1.25, LowThick = 1.2; // (the low dense skirts among the billows)
			Span Life{ 4.0, 6.0 }, LifeFast{ 7.0, 10.0 }, R0{ 0.5, 0.9 }, R1{ 1.5, 2.5 }, R1Fast{ 3.5, 5.0 }, RiseM{ 1.0, 3.0 }, Alpha{ 0.35, 0.5 };
			Rgb Low, High; // (its colour by the ground, where it is thrown up; and higher in the air, paler)
		};
		/** An air_trails row, read once a kind. */
		struct TrailRow
		{
			std::string Name;
			double Life = 6.0, Spacing = 8.0, R0 = 0.5, R1 = 3.5, Alpha = 0.5, DarkenS = 4.0, RampS = 1.5, Smear = 1.5, Carried = 0.05, Flame = 0.0;
			double Longest = 0.0; // (longest_m over 0: the most sky a trail is laid over in its life; a faster aircraft's is squeezed, see TrailMove)
			double Grow = 0.5, Lumpy = 0.0, SmearS = 0.0, Rise = 0.0;
			double ThinAfter = 0.0, ThinGrow = 1.0, ThinAlpha = 1.0, ThinHold = -1.0, ThinRagged = 0.0; int ThinKeep = 1, KeepFar = 0; // (KeepFar over 0: the far form's, game.air_far.trails_keep_one_in, its bars one in that many of its puffs; ThinKeep still sets how far one reaches back)
			// (an old trail thinned: past ThinAfter s only one puff in ThinKeep hangs on to Life, ThinGrow times wider, ThinAlpha times denser, full until ThinHold of its life)
			double LeastPx = 0.0, LeastKeep = 0.0; // (a far trail held at least LeastPx across, keeping LeastKeep of its opacity)
			// (a ribbon, Core over 0: every piece a length of one strip back to the piece before it, sharing its edge,
			// dense over Core of its half width and thinning to its sides; 0: the trail as it was, each puff a lens. The
			// lengths that hang on, one in ThinKeep, reach back to the last of their own and come into sight between
			// JoinFrom and JoinBy of thin_after_s as the short ones go; one is laid wherever the path has turned TurnCos
			// since the last, so a spin's trail follows its curve. LumpsFrom: the share of its life after which a length
			// draws in to a lump. Drift: the share of the lumpy scatter each piece is thrown with)
			double Core = 0.0, JoinFrom = 0.15, JoinBy = 0.45, TurnCos = 0.9, LumpsFrom = 0.75, Drift = 0.35;
			// (PaleTo, PaleFrom, PaleBy: a ribbon pales from PaleFrom seconds old to PaleTo as dark by PaleBy seconds, and
			// stays so; 0: as it was. Wander: how far its width wanders along its length, 1 about a sixth either way.
			// WidenBy: seconds by which it has widened from r0_m to r1_m, by the power grow; 0: by the short pieces' life)
			double PaleTo = 0.0, PaleFrom = 0.0, PaleBy = 2.5, Wander = 0.0, WidenBy = 0.0;
			double SmokeStart = 1.0; // (fire: where along the flame its smoke starts, a share of the flame's length)
			Span FlameLiftOfLen{ 0.0, 0.0 }; // (fire: how far each tongue stands off the skin as a share of its length; 0: flame_lift_m)
			double LicksPerFrame = 0.4; // (fire: tongues torn free of the tip and streaming away aft, a frame)
			double FlameAlphaRoot = 0.92, FlameAlphaFall = 0.2, FlameHold = 0.45, FlameWide = 1.0, LickAlpha = 0.9; int FlameTongues = 0; double FlameOver = 1.0; // (fire: a tongue's opacity at the cowl and how much it loses to its tip; how long it holds full, a share of its life; how wide; a torn-free lick's opacity)
			double FlameLiftCurve = 0.0; // (fire: a tongue's stand-off grows along it as this power of the way to its tip, so it is rooted on the skin; 0: off the skin from near its root)
			bool bFlameTip = false; Rgb FlameTip; // (fire: the colour a tongue's tip goes to, into its smoke)
			double TipSmoke = 0.0, TipSmokeR = 0.3; // (fire: black smoke laid at each tongue's tip at this opacity, widening to TipSmokeR of the flame's length; 0: none)
			Span TipSmokeLife{ 0.3, 0.6 };
			double FlameOfSpan = 0.0, FlameMost = 0.0, FlameLeastPx = 0.0; // (a fire's flame: this share of the aircraft's span, at most FlameMost m; never under FlameLeastPx on the screen)
			// (an engine that is out, art director 15:38Z. TailWide: how wide the smoke is as it passes the tail, TailM
			// metres behind its source (0: no spread in the wake). GoutEvery: a thicker gout that often, lasting GoutFor
			// seconds, GoutWide times as wide and GoutAlpha times as dense (0: none). FadeOver: it thins to FadeTo of
			// its opacity over that long (glycol running out). ForS: it lasts that long (the extinguisher's puff).
			// bThenStreak: once thinned or done, the trail goes on as the "streak" row. FlameShrink: started on a
			// burning trail, the flames shrink back to the source over this long, never cut off)
			Span TailWide{ 0.0, 0.0 }, GoutEvery{ 0.0, 0.0 }, GoutFor{ 0.5, 1.0 }, FadeOver{ 0.0, 0.0 }, ForS{ 0.0, 0.0 };
			double TailM = 16.0, GoutWide = 1.5, GoutAlpha = 1.3, FadeTo = 1.0, FlameShrink = 0.0;
			// (the flamer, a tank alight: with stream_lengths the row streams fire back from its point. The fire is laid in
			// the air and left behind, as smoke is, so it tears into separate tongues. StreamLen: the flame's length in
			// aircraft lengths by the trail's look, 0 to 3; an aircraft is StreamLenOfSpan of its span long. StreamSoot:
			// how far its far end goes to StreamSootCol, by the look. StreamWide: a tongue's half width in metres on an
			// 11 m span, wider as the root of the span. StreamRagged: how many of those it has wandered off by its end)
			bool bStream = false;
			double StreamLen[4] = { 1.0, 1.5, 2.0, 3.0 }, StreamSoot[4] = { 0.0, 0.3, 0.6, 1.0 };
			double StreamLenOfSpan = 0.8, StreamMost = 90.0, StreamCarried = 0.25, StreamWide = 0.77, StreamSmokeFrom = 0.45, StreamOver = 1.6, StreamRagged = 1.7;
			int StreamStrands = 2;
			Rgb StreamHot, StreamBody, StreamDeep, StreamSootCol;
			double Wisps = 0.0; // (how many of its pieces throw a wisp out of the column, 0 to 1: its young edge is torn, never ruled)
			// (art director 19:52Z, the trails read as even tubes. TailWiden: Bit::WakeOn. Knots: how many of its pieces
			// lay a denser lump in the column or on its edge, KnotR of its half width across, KnotOff of it out from the
			// middle, KnotAlpha as dense as the piece, KnotDark as dark, hanging KnotS. Fold: how much denser its one
			// side is than its other, wandering along its length (Bit::Lean): lighter folds between its knots. SpurtM: a denser, wider
			// spurt that long at its source, SpurtWide times its width and SpurtAlpha its density)
			double TailWiden = 0.0, Knots = 0.0, KnotAlpha = 0.7, KnotDark = 0.8, Fold = 0.0, EdgeTorn = 0.0, SpurtWide = 2.0, SpurtAlpha = 1.6;
			Span KnotR{ 0.3, 0.55 }, KnotOff{ 0.15, 0.9 }, KnotS{ 2.5, 5.0 }, SpurtM{ 0.0, 0.0 };
			// (bSoft: the strip's colour runs from each piece to the next and its density swells and thins with its width over
			// several pieces, so no piece shows as a block. Blend: a trail changing to this kind goes over to it in that
			// many seconds, colour, density and width, never at a cut. Shadow, ShadowK: pale smoke's side away from the
			// sun, see Bit::ShadowCol)
			bool bSoft = false; double Blend = 0.0, ShadowK = 0.0; Rgb Shadow;
			bool bThenStreak = false;
			// (a funnel's plume, the row's "plume" keys: see Battle::PlumeMove. A look for full power, one for cruising and
			// one for the black surge as she rings up: a puff's opacity at the lip, the seconds by which it has fallen by e
			// and the power it falls by (Bit::AgeFade), how long the oldest hang, and its colour at the lip and at OldBy seconds)
			struct PlumeLook { double Alpha = 0.64, FadeS = 10.6, FadePow = 0.92, AgeS = 40.0, OldBy = 25.0; Rgb Col, Old; };
			struct PlumeRow
			{
				bool bOn = false;
				double Lip = 1.1, Overlap = 3.0, WideK = 0.498, WideByFunnel = 0.45, RiseK = 4.3, FeltLeast = 2.0, TopSpeed = 15.4, Funnel = 3.0;
				double FullFrom = 0.75, CruiseTo = 0.4, Size = 0.3, Shade = 0.15, Rough = 0.35, LipLeastPx = 0.0, LipMost = 4.0, ShadowOfLit = 0.55, ShadowK = 0.0;
				double SurgeRise = 0.08, SurgeWithin = 5.0; Span SurgeFor{ 15.0, 25.0 };
				double OverlapFar = 3.0; Span FarPx{ 3.0, 6.0 }; // (a far ship's puffs overlap OverlapFar deep where their lip is FarPx.A pixels or under, in full from FarPx.B)
				// (Even over 0: laid evenly, see PlumeMove, and when its halvings come; how round a puff's heart ring stays, Bit's own)
				double Even = 0.0, EvenStep = 2.0, HeartRound = 0.0;
				int EvenNear = 30, EvenFar = 0;
				// (its roll, Bit::EvenRollLift; and a wide funnel's puffs laid closer: OverlapWide deep at WideFunnel metres and wider)
				double RollLift = 0.0, RollShade = 0.0, RollRun = 4.0, OverlapWide = 3.0, WideFunnel = 8.0;
				PlumeLook Full, Cruise, Surge;
			};
			PlumeRow Plume;
			Span FlameLift; // (metres a fire's tongue stands off the skin)
			Rgb Col, ColFull, ColOld;
		};
		/** A trail being laid: where its point was at the last call, and how far it has gone since its last puff. */
		struct Trailing
		{
			int Id = 0;
			bool bOn = false;
			std::string Kind;
			double Born = 0.0, KindBorn = 0.0, Last = 0.0, Owed = 0.0, LastPuff = 0.0, NextFlame = 0.0;
			double Tip = 0.0; // (fire: how far aft of the point its smoke starts, at the flame's tips)
			double Span = 0.0; // (the aircraft's span, metres, as TrailStart was told: a fire's flame is sized by it)
			double FarK = 1.0; // (game.air_far: how many times farther apart its puffs are laid now; 1, as ever)
			unsigned BarsLaid = 0; // (how many bars that hang on it has laid: Bit::StandBars)
			unsigned Puffs = 0, NextKept = 0, KeptGap = 0; // (puffs laid, counted only when the row thins its old puffs; the next to hang on, and how many back the last one kept was)
			Vec At, Carry, LastPuffAt;
			Vec KeptAt; bool bKeptAt = false; // (where the last puff kept to hang was laid)
			Vec PuffAt, KeptWay; bool bPuffAt = false; // (a ribbon: where the last puff was laid, and the way the path ran at the last one kept)
			Bit::Reach Prev, Kept; // (a ribbon: the last piece laid and the last length that hangs on, for the next of each to reach back to)
			Vec Wander; double RiseK = 1.0; bool bWander = false; // (a ribbon: the drift and rise its last piece took, which the next takes most of, so the strip meanders and never zig-zags)
			double Wide = 0.0; // (and how much wider or narrower than the row its last piece was)
			double Lean = 0.0; // (and how much denser its one side was than its other, where the row folds)
			// (an engine that is out: its next gout and the one running; how long this kind thins over, and how long it
			// lasts before the streak takes over; the flame a put-out fire had, shrinking away by FlameOutBy)
			double NextGout = 0.0, GoutFrom = 0.0, GoutUntil = 0.0, FadeFor = 0.0, KindFor = 0.0, FlameWas = 0.0, FlameOutBy = 0.0, FlameShrinkS = 0.0;
			double FlameStartWas = 1.0; // (and where along that flame its smoke started)
			double NextStream = 0.0; int LookNo = -1; // (a flamer: its stream's next tongue, and the event's look, 0 to 3)
			// (the last piece laid, as it was laid; and, while the trail goes over to a new kind in BlendFor seconds, the
			// piece the old kind laid last, which the new kind's pieces start from)
			struct Look { Rgb Col, Col2, Shadow; double Alpha = 0.0, R0 = 0.0, R1 = 0.0, WakeR = 0.0, Life = 0.0, ShadowK = 0.0, Core = 0.0; bool bOn = false; };
			Look Laid, Was; double BlendFor = 0.0;
			// (bClose: the old kind left lengths that hang on, and the new kind lays none: the next piece closes their
			// line, by the old kind's clocks in Close, so the old trail thins away to its end and is never cut square)
			struct Bars { double Life = 0.0, Hold = 0.0, ShowFrom = 0.0, ShowBy = 0.0, Smear = 0.0, PaleTo = 0.0, PaleFrom = 0.0, PaleBy = 0.0; };
			Bars Close; bool bClose = false;
			// (a funnel's plume: the seconds since its last puff and how many it has laid; the power the scene gave last,
			// and that power a few seconds late, for a sharp rise is her ringing up; the black surge that follows; and the
			// slow wander of its puffs' climb and drift, each taking most of the last one's)
			double PlumeOwed = 1.0e9, PowerLate = -1.0, SurgeFrom = 0.0, SurgeUntil = -1.0e18, PlumeRise = 1.0, PlumeSide = 0.0;
			double Squeeze = 1.0; // (longest_m: the share of its row's time the trail was last laid by, 1 when not squeezed; TrailKind's closing bar takes it)
			// (Bit::CutAt: this kind's run of the trail, a new one at each TrailKind, and when the row's life ends for the
			// last piece TrailMove laid; a later call whose first piece would end sooner cuts the pieces before it)
			uint32_t Run = 0;
			double RunEnd = -1.0e18;
			unsigned PlumeN = 0;
		};
		/** One air gun: its belt, its next round, and where it was at the last call (to lay rounds between calls). */
		struct Firing
		{
			int Gun = 0, Count = 0;
			bool bOn = false;
			double Last = 0.0, Next = 0.0, RateK = 1.0;
			Vec Muzzle, Dir, Carry;
			Vec HoseAt, HoseVel; double HoseBorn = 0.0; bool bHose = false; // (fx-air r2: the last round of this burst, for its hose)
		};
		struct Flash
		{
			Vec At;
			Rgb Col;
			double Born = 0.0, Life = 0.0, Strength = 0.0, Radius = 30.0, Flicker = 0.0;
			bool bNightOnly = false; // (a small gun's flash: no lamp on the ground by day)
			bool bOneFrame = false; // (Effects (R2): lit on the flash's own frame alone, not half the next)
			int FirstFrame = -1; // (the first frame drawn after it was born: its lamp is full then, half the next, then out)
			double LampS = 0.0; // (a naval gun's, muzzle.<row>.lamp_s: full on its first frame, then down to nothing by this many seconds after its birth, never under half on the next frame. 0: as above)
			// (a naval gun's: its muzzle, where the lamp rule's merge measures from, the lamp itself hung far out from it
			// (lamp_out_m). bGun false: the merge measures from the lamp, At)
			Vec Gun;
			bool bGun = false;
		};
		struct Knock
		{
			Vec At;
			double Born = 0.0, Strength = 0.0, Within = 0.0, Push = 0.0;
		};
		/** An oil_fires row, read when a fire starts. */
		struct OilRow
		{
			Span Heads, HeadR, FlameR, FlameTall, FlameLife, ColumnLife, Rise, Burst, Windage, R0, R1, Alpha;
			Span BaseLife, BaseRise, BaseR, BaseAlpha;
			double BaseEvery = 0.0; // (the low black smoke rolling off the whole slick before it joins the column; 0: none)
			double BaseFlat = 1.0; // (under 1: that roll hugs the water, this high for its width)
			Span PallLife, PallR1, PallWindage, PallAlpha;
			double PallEvery = 0.0, PallStretch = 1.0, PallStretchBy = 0.0, PallHold = 0.15, PallDrift = 0.0, PallRough = 0.0, PallRest = 0.0; Span PallLevel{ 1.0, 1.0 }; // (the long flat pall run out down the wind from the column's top; 0: none)
			double SlickLong = 0.0, SlickWide = 0.0; // (over 0: the burning slick an oval, these shares of Reach long and wide, its heads all over it)
			double FlameOdds = 1.0, FlamePoint = 0.9; // (flame_point: how sharp each tongue's tip, 1 a spike; each head's chance of a tongue each flame_every_s: a broken line of low flames)
			double SunSide = 0.0; // (its sunward side lit toward Lit, so the column reads round)
			double SunYoung = 1.0, SunFrom = 0.0, SunBy = 0.0; // (SunBy over 0: a puff has only SunYoung of that light until SunFrom seconds old and all of it by SunBy, so the column is black at its root)
			double BurstDrag = 0.1, HeadSpread = 0.6, FlameUp = 0.0, FlameEvery = 0.1, ColumnEvery = 1.0, Slow = 0.06, Hold = 0.5, Heart = 0.6, Rough = 0.3;
			double Cool = 1.0, Lamp = 0.25, LampReach = 2.0, TwinPx = 0.0, Grow = 0.6;
			int Lumps = 0; // (lumps, lumps_px, lumps_alpha, lumps_light, lumps_rim, lumps_inner, lumps_r, lumps_out: see Bit::Lumps)
			double LumpsPx = 0.0, LumpsAlpha = 1.0, LumpsLight = 0.6, LumpsRim = 0.8, LumpsInner = 0.0;
			Span LumpsR{ 0.3, 0.45 }, LumpsOut{ 0.3, 1.0 };
			bool bKeep = true;
			Rgb Col, Col2, Flame, Lit;
			// (a small fire's column held in one piece, fire.smoke_column only: tight_m, overlap, least_px, least_keep.
			// See the column's Lay in Run. 0: as before)
			double TightM = 0.0, Overlap = 0.0, ColumnLeastPx = 0.0, ColumnLeastKeep = 0.0;
			// (fx-air r2: the column pales and thins as it climbs, pale_up_m and the rest; see Bit::PaleUpM. 0: as before)
			double PaleUpM = 0.0, PaleUpMost = 0.0, PaleUpPow = 1.0, ThinUp = 0.0;
			Rgb PaleUpCol, PaleUpLit;
		};
		/** A big fire's smoke as a bank of lumps run out down the wind (fire.smoke_bank), read when the fire starts. */
		struct BankRow
		{
			bool bOn = false; // (the recipe asks for it and this fire is from_m across or more)
			double Wide = 1.6, Long = 2.5; // (the bank's width as a share of the fire's, and this fire's own length as a share of that width)
			double Life = 40.0, Short = 0.3, Thin = 2.0; // (this fire's lumps' longest life, the shortest as a share of it, and how few live long)
			double EveryA = 0.35, EveryB = 0.8; // (seconds between lumps while it flames, and after)
			double Heads = 0.33, Rough = 0.35, Heart = 0.85, Hold = 0.5, SunSide = 0.0, TwinPx = 0.0, Rest = 1.0, Spread = 0.35;
			double SunYoung = 1.0, SunFrom = 0.0, SunBy = 0.0; // (how much of its lit side a lump has when young, and the shares of the longest life until which it has no more and by which it has it all)
			Span LumpR{ 0.3, 0.5 }, Alpha{ 0.75, 0.95 }, Shade{ 0.8, 1.25 }, Rise{ 0.0, 0.4 }, HeadRise{ 0.6, 1.4 };
			Rgb Lit;
		};
		/** A steam row (steam.vent, steam.funnel), read when the vent opens. */
		struct SteamRow
		{
			double Reach = 10.0, Cone = 12.0, Gap = 1.0, Every = 0.06, Roar = 0.3, Least = 0.12, Mouth = 0.5, Rag = 0.2, Rise = 1.5, Windage = 0.5, Alpha = 0.9, Waver = 0.25, Rough = 0.35, Lance = 0.0, LanceLife = 0.3, LanceLong = 3.5;
			Span Life{ 1.6, 2.8 };
			Rgb Col, Old;
			// (a rag's shadow side, Bit::ShadowCol, and how far it goes to it; the plume's head: Cap of its rags lay
			// another, CapWide times as big, in its top from CapFrom of its reach up, living CapLife and going off down the wind)
			Rgb Shadow;
			double ShadowK = 0.0, SideShade = 0.0, Cap = 0.0, CapWide = 2.0, CapFrom = 0.62; // (SideShade: how far a rag thrown to the plume's side away from the sun goes to the shadow colour all over)
			Span CapLife{ 3.0, 6.0 };
		};
		/** The fire.tongues row: a burning thing's flames as ragged tongues at seats of its own (see FireTongues). */
		struct TongueRow
		{
			bool bOn = false;
			// (a step; the surge in a tongue's height, as a share; its lean for each metre a second of wind, and at the
			// most; the chance a step of a tip torn off, and of a tongue's thin tip and of its fork; the width a fire's tongues are no
			// taller for; the width in the picture under which it burns at fewer seats, and the fewest; Over: the share of
			// its width its flames are sorted nearer the eye, to show through its own smoke)
			double Every = 0.05, Pulse = 0.45, Lean = 0.06, LeanMost = 0.6, Torn = 0.5, Tips = 0.5, Forks = 0.6, TallWide = 12.0, FullPx = 80.0, Over = 0.0;
			int SeatsLeast = 3;
			Span Seats{ 3.0, 5.0 }, Tall{ 0.6, 1.5 }, Wide{ 0.14, 0.24 }, Life{ 0.22, 0.4 };
			Rgb Root, Body, Tip; // (the white-yellow of its foot; a tongue's own colour, at its root; the dull red of its tip)
			double RootOver = 1.0; // (how far past white the root burns under the tallest seat; see picture.lit_gain)
			double FarTall = 0.0, FarPx = 400.0; // (a fire under FarPx wide in the picture stands up to FarTall taller)
			double FootSoft = 0.4; // (the share of a foot lick, from its root out to its rim, over which it fades away)
			// (smooth flames, the frame study of 22 November: a piece born BornK of its size grows to it by FullBy of its
			// life and fades in over FadeShare of it; each is born up to Jitter of a step late, so births never beat with
			// the frames; Anchor keeps its root on the seat (Bit::Anchor) and Climb is how much of its old climb it keeps;
			// the bed lives BedOverlap of its old life, so a seat's foot pieces cross-fade; Flicker, FlickerLong and
			// FlickerWide its seat's flicker (Bit::Flicker) between FlickerHz. All at their old values: as before)
			double BornK = 1.0, FullBy = 0.3, FadeShare = 0.0, Jitter = 0.0, Anchor = 0.0, Climb = 1.0, BedOverlap = 1.0;
			double Flicker = 0.0, FlickerLong = 0.0, FlickerWide = 0.0;
			Span FlickerHz{ 3.0, 6.0 };
			// (Point: how pointed a tongue's body, fork and tip are drawn, 0.9 as before; LongMost: how drawn out a body may
			// be for its width at the most, 5 as before, its forks and tips 3.5 or LongMost if less. Lower, the flames read
			// as a heap of overlapping lobes and not a row of blades: the art director's "overlapping puffs")
			double Point = 0.9, LongMost = 5.0;
			bool bSmooth = false;
			// (the fire's body, Mass over 0: so many soft round flame puffs a step, rising through it from the seats and
			// reddening and darkening to the smoke at its top, so it reads as one body and never a row of candles: MassR
			// of the fire's width each, MassUp of a seat's height their climb, MassLife their seconds; Soot: so many
			// black puffs a step off its top, SootR of its width, where the flames go into the smoke)
			double Mass = 0.0, MassUp = 0.7, Soot = 0.0;
			Span MassR{ 0.22, 0.34 }, MassLife{ 0.5, 0.8 }, SootR{ 0.25, 0.4 }, SootLife{ 1.2, 2.0 };
			Rgb MassCol, MassTop, SootCol;
			Rgb Bed; // (bBed: the foot's own colour in place of the fire's yellow heart)
			bool bBed = false;
			// (the art director's mends of 06:43Z 22 November on head C. BedOver: the foot burns that far past white, so its
			// base metre lands yellow-orange, 0 as before; MassOver: the body puffs' own, 1 as before; MassInner: how far out
			// from its middle a body puff stays dense, 0.5 as before, higher a crisper edge; Flare, bFlare: about one lick
			// every FlareEvery seconds out of the fire's tallest seat, up to FlareTall times its height, rising and falling
			// over FlareS, its top torn off as it falls;
			// BedEdge, bBedEdge, BedInner: the foot's colour at its rim (the fire's flame colour as before) and how far out it
			// stays dense (0.4 as before))
			double BedOver = 0.0, MassOver = 1.0, MassInner = 0.5, FlareTall = 1.5, FlareS = 0.5;
			double Inner = 0.0; // (Inner: how far out a tongue's body and fork stay dense, 0.8 and 0.78 as before; higher, a crisper edge)
			double BedInner = 0.4, BedTall = 1.0; // (BedTall: the foot and its heart licks that many times taller, 1 as before)
			Rgb BedEdge;
			bool bBedEdge = false;
			Span FlareEvery{ 2.0, 3.0 };
			bool bFlare = false;
		};
		/** fire.glow: a burning thing's warm light on the ground round it, read once a frame. */
		struct GlowRow
		{
			bool bOn = false;
			// (a piece every Every seconds, each living Life and fading in over a third of it, so three overlap and the
			// glow never steps; R of the fire's width across its half, held between RLeast and RMost metres; Up metres
			// over the ground under the fire; only for a fire within Above metres of the ground; its opacity at night
			// Alpha, by day DayAlpha of that; its flicker as a flame's (Bit::Flicker))
			double Every = 1.0, Life = 3.0, R = 2.2, RLeast = 3.0, RMost = 9.0, Up = 0.2, Above = 6.0, Alpha = 0.35, DayAlpha = 0.4, Over = 1.0, Flicker = 0.2;
			Span FlickerHz{ 1.5, 4.0 };
			Rgb Col;
		};
		/** fire.late: a long fire burning down, read once a frame. */
		struct LateRow
		{
			bool bOn = false;
			// (the art director's ruling, section 3: "in a long battle, after 3 to 5 minutes the flames drop to 1 to 2 m
			// and the smoke goes grey-brown and thinner". From From seconds after it was lit (its own, by its seed), over
			// By seconds: its flames to Flames of their size, its lamp and glow to Light, its land column's puffs to Col at
			// their foot and Top above, at Alpha of their ink)
			Span From{ 180.0, 300.0 };
			double By = 60.0, Flames = 0.4, Light = 0.4, Alpha = 0.6;
			Rgb Col, Top;
		};
		/** The cook_off row, read when it starts. */
		struct CookRow
		{
			double JetTall = 4.5, JetWide = 0.5, JetEvery = 0.04, JetSmokeEvery = 0.12, Dip = 0.35, Two = 0.5, PuffAlpha = 0.6, FlashK = 0.75, Lamp = 0.3, FragmentOdds = 0.35;
			// (the jet's surge in width and height, as a share; how far it leans for each metre a second of wind, and at
			// the most; its sparks a step; its lamp on the hull and ground, and the share of that it keeps by day; the
			// tongues torn off its sides a step; how uneven a pop's embers are)
			double JetPulse = 0.0, JetLean = 0.0, JetLeanMost = 0.4, JetSparks = 0.0, JetLamp = 0.0, JetLampDay = 0.6, JetTorn = 0.0, EmberUneven = 1.0;
			Span Embers{ 6.0, 14.0 }, EmberUp{ 5.0, 20.0 }, EmberLife{ 0.8, 2.2 }, Puffs{ 2.0, 3.0 };
			Rgb Ember, EmberOld, Puff, Smoke, Fragment, Root;
		};
		struct Source
		{
			Vec At;
			double Start = 0.0, Flames = 0.0, End = 0.0, Across = 1.0, Next = 0.0, NextSmoke = 0.0, NextBase = 0.0, NextPall = 0.0;
			// (a burning thing put out on cue, BurnOut: OutAt 0 or more is when its crew began and OutOver how long they
			// take to beat its flames back to its seat; its smoke has paled by OutPale seconds after that; OutSurge over
			// 0, the seconds of white steam at its seat as the water hits, laid by the clock NextSteam)
			double OutAt = -1.0, OutOver = 0.0, OutPale = 20.0, OutSurge = 0.0, NextSteam = 0.0;
			int Column = -1; // (a burning thing: the big fire's own column it founded or joined, by that column's Id)
			// (a steam vent, Kind 4: the way it blows and its row. A cook-off, Kind 5: Way is its hatch's way out, Pops
			// the moments of its pops, PopNext the next of them, and its jet burns from JetAt to JetEnd, JetTall metres
			// at its fullest, in two pulses when JetDip is over 0 (the share of its time at which it sinks between them))
			Vec Way;
			SteamRow Vent;
			CookRow Cook;
			std::vector<double> Pops;
			size_t PopNext = 0;
			double JetAt = 0.0, JetEnd = 0.0, JetTall = 0.0, JetDip = 0.0;
			Vec SlickA, SlickB; // (an oil fire's oval slick: its half-axes, long and wide)
			int Kind = 0; // 0 a burning thing, 1 a smouldering crater, 2 burning oil on the sea, 3 a big fire's own column, 4 a steam vent, 5 ammunition cooking off, 6 a smoke screen
			bool bPhos = false; // (a smoke screen: a phosphorus round's, its wall laid from where its streamers fell, Heads; Flames is when it is laid no more)
			// (a smoke screen, Kind "grenade": a tank commander's smoke grenade, smoke_screen.grenade; its width at 2 s, metres,
			// and the length of its plume down the wind, metres)
			bool bGrenade = false;
			double GrenadeStart = 0.0, GrenadePlume = 0.0;
			double NextJet = 0.0, NextSeat = 0.0, NextShade = 0.0; // (a grenade's own clocks: its jet, its billow, its shadow; GrenadeLumps)
			// (on a moving ship, BurnMove, SteamMove, CookOffMove: bMoves once it has been moved; Vel the ship's velocity
			// there, metres a second, as last told at MovedAt; Ride, its own number for the pieces that lie on the ship)
			Vec Vel;
			double MovedAt = 0.0;
			bool bMoves = false;
			bool bOnShip = false; // (a burning thing: lit on a ship's deck, Burn's bOnShip; never the land look, never joined)
			uint32_t Ride = 0;
			double UnderOil = 0.0; // (a big fire's own column: none of it is laid while oil burns within this share of that oil's reach of a fire it stands on)
			bool bStoodDown = false; // (a big fire's own column: burning oil has stood over her once, so this column never stands again; see fire.smoke_column's most)
			int Id = -1; // (an oil fire, a steam vent, or a burning thing that may be put out: the caller's number for it)
			uint64_t Seed = 1;
			OilRow Oil; // (an oil fire: its row)
			BankRow Bank; // (a burning thing: its smoke as a bank of lumps, when the recipe asks and it is big enough)
			std::vector<Vec> Heads; // (an oil fire: where on the slick it burns hardest, fixed when it starts; a smoke screen: the seats its wall is laid from)
			// (a burning thing others have joined, fire.join: its width grows from AcrossFrom to AcrossTo over AcrossBy
			// seconds from AcrossAt; AlsoIds are the joined fires' numbers, each AlsoOffsets from this one's seat, so
			// BurnMove and BurnOut by any of them find it; the first number that moves it is the one it moves by, Mover)
			double AcrossFrom = 0.0, AcrossTo = 0.0, AcrossAt = 0.0, AcrossBy = 0.0;
			std::vector<int> AlsoIds;
			std::vector<Vec> AlsoOffsets;
			int Mover = -1;
			bool bMoverSet = false;
			double NextGlow = 0.0; // (a burning thing's warm glow on the ground, fire.glow, by the clock)
			bool bLandColumn = false; // (a big fire's own column, Kind 3, that stands over a burning vehicle on land: fire.land_column)
			bool bVent = false; // (a burning thing: a burning opening, Vent: never joined, no column of its own, fire.vent_tongues)
		};
		struct Paint
		{
			Rgb Soil, Heavy, Dust, Clod, Smoke, First, Splinter;
			Rgb Ring, Foot, Stone; // (the dust ring's colour, the dark foot's, the shower's lumps': by default the dust, heavy and clod)
			double FootDust = -1.0; // (how much of the ground's dust shows in the dark foot's veil and fill; under 0, the recipe's burst.foot shares)
			Rgb Scar; // (the earth thrown round the crater's mark, where the ground gives it: snow's brown-black star)
			bool bScar = false;
			double ScarRays = 1.0; // (and how much longer those rays are drawn out)
			bool bFirst = false, bSplinter = false;
			double Grain = 1.0; // (the crown's puffs as a share of their size: under 1, broken and granular, as snow)
			double FirstAtFoot = 1.0, FirstAtTip = 1.0; // (how much of a jet is the ground's first colour, foot to tip)
			bool bSmokeTop = false; // (grounds.<ground>.smoke_of_ground: the cloud's top from the ground the game shows there)
			Rgb SmokeTop;
			bool bBruise = false; // (paint.bruise: the scar a pale powder bruise with a small dark middle, as on rock, never a dark star)
			Rgb Bruise;
			double BruiseCore = 0.4, BruiseLobesLo = 4.0, BruiseLobesHi = 6.0, BruiseChipsLo = 3.0, BruiseChipsHi = 5.0;
		};
		// (grounds.<ground>.crown_low and crown_wide, while one burst's crown is laid: a hard ground's fan lower and wider,
		// each jet's way and reach squashed so, its body and lumps as much)
		double CrownLow = 1.0, CrownWide = 1.0;
		std::string CrownGround; // (and its ground's word, for its chips: grounds.<ground>.chips)
		void Squash(Vec& Dir, double& Reach) const;

		const Recipe* Book = nullptr;
		GroundFn GroundAt = nullptr;
		ColourFn ColourAt = nullptr;
		void* ColourContext = nullptr;
		void* GroundContext = nullptr;
		Vec Wind = Vec(1.6, 0.4, 0.0);
		uint64_t Seeds = 1;
		uint32_t RideNo = 0; // (the last number given to a moving fire for the pieces that lie on its ship)
		int OwnColumns = 0; // (big fires given a column of their own so far: each takes an id above every caller's, for the column's width measure)
		uint64_t Glances = 0; // (ricochets so far: with the place, the moment and the kit's seed, each one's own dice, so no two fly alike)
		std::vector<double> ShotEnds; // (when each ricochet's shot now in flight is done: no more than glance.most_shots are followed at once)
		double GlanceAt = -1.0e18; int GlanceSame = 0; // (the moment of the last ricochet, and how many shots began to be followed at that moment: glance.most_a_frame)
		static constexpr int OwnColumnId = 1 << 28;
		int MaxPuffs = 3000;
		int Sides = 16, PointsPerPuff = 33;
		std::vector<double> RingCos, RingSin, Ring2Cos, Ring2Sin, Ring3Cos, Ring3Sin, Ring5Cos, Ring5Sin; // (made once at Setup, so no puff works out a cosine a side each frame)
		int LastDrawn = 0;
		double ShadeLit = 0.18, ShadeDark = 0.28, ClodLeast = 1.2, Beyond = 1500.0, KeepBeyond = 1500.0;
		struct RoundLight
		{
			bool bOn = false;
			Vec Right, Back, Sun; // (level: the screen's right, toward the eye, toward the sun)
			double Mid = 0.0; // (the column's middle at the puff's height, along Right)
			double R = 1.0, Gain = 0.0, Level = 1.0, Up = 0.0; // (the column's radius there; how strong; the sun's level and upward shares)
			double Lobe = 0.25; // (the share of its light a fan takes from its own sunward side, the rest from the column's)
			double Top = 0.0; // (game.from_above: seen from a high steep eye, this share of the way to TopLift on its top's sunward side, two fifths of it on the far side)
			Rgb TopLift; // (and the lift it takes toward: smoke's own lit colour from above, dark_lit_colour for black smoke)
			double Rim = 0.0; // (over 0, a lump on a lump: its rim keeps this share of its light, however see-through it is there)
		};
		double RoundUp = 0.35;
		RoundLight Round; // (while laying one oil column puff: each point lit as a point on a round column, by where it sits across it)
		Rgb EdgeTo; // (while laying one bit: its rim's colour, EdgeK of the way; see Bit::Edge)
		double EdgeIn = 0.25; // (and the share of it the heart ring takes; see Bit::EdgeIn)
		// (while laying one bit: over 0, the heart ring sits this share of the way out along the bit's long way, and
		// Inner only across it. A day tracer: solid to within a pixel of its edge all round, the last pixel fading out,
		// so a thin line a frame long is one straight line and never a stair of steps. Art director 14:26Z)
		double LayInnerAlong = 0.0;
		// (while laying one bit: a ribbon's piece. LayLong: from its middle to its near edge's middle; AxisB is half its
		// near edge and LayFar half its far edge; LayBendFar, LayBendNear: how the strip's line bows off the straight
		// run to leave each edge along the path there; LayTailAlpha: its far edge's opacity as a share of its near
		// edge's; LayLump, LayLumpFar: how far its near and far ends have drawn in to a round lump, 0 a link of the
		// strip and 1 a round puff)
		std::vector<std::pair<uint32_t, unsigned>> RunBars; // (set by Draw: each trail run's Trailing::BarsLaid, by run, for Bit::StandBars)
		Vec LayLong, LayFar, LayBendFar, LayBendNear; bool bLayLong = false; double LayTailAlpha = 1.0, LayLump = 0.0, LayLumpFar = 0.0, LayWob = 1.0, LayBillow = 0.0; uint32_t LayBillowSeed = 0; bool bLayShowEach = false; double LayShowFrom = 0.0, LayShowBy = 0.0, LayAgeNear = 0.0, LayAgeFar = 0.0;
		bool bLayFarCol = false; Rgb LayFarBy, LayFarFloor; bool bLayLean = false; double LayLean = 0.0, LayLeanFar = 0.0; // (Bit::bSoft: how its far edge's colour differs from its near edge's, in the hour's light)
		// (while laying one bit: bLayGlare, its Lift is its burst's flash and not the sun, Bit::GlareK: each point lit by how
		// far it is round toward LayGlareWay, the way to the flash; LayGlareAll: 1 the eye sees the face the flash lights and
		// all of it is lit, a half the piece stands beside the flash, 0 it stands before it and only its edge catches)
		bool bLayGlare = false; Vec LayGlareWay; double LayGlareAll = 0.0;
		// (picture.lit_gain: the game's picture multiplies every colour by this (the wrapper sets the material's
		// brightness to it), and every colour is written that much darker, so all looks as before; but a light's hottest
		// parts (Bit::Over) are written up to that much brighter than white, so they land white-hot after the game's
		// tone curve, which holds plain white down to about 224 of 255. 1: nothing changes)
		double LitGain = 1.0, PutK = 1.0;
		double LaySquare = 0.0; // (while laying one bit: 0 an ellipse; toward 1 its outline squared off, a gun case's cylinder seen side on)
		Vec LayHub; // (while laying one bit: where its fan's middle point sits off its middle; see Bit::Hub)
		Rgb LayShadow; // (while laying one bit: its shadow side's colour, Bit::ShadowCol in the hour's light, and how far it goes to it)
		double LayShadowK = 0.0;
		bool bSrgbBytes = false; // (SetSrgbBytes)
		double LayFadeHead = 1.0, LayFadeTail = 1.0; bool bLayFade = false; // (fx-air r2, while laying one bit: its opacity runs evenly from its tail end to its head, a ship's tracer or hose)
		double LayHeartRound = 0.0; // (while laying one bit: Bit::HeartRound)
		double LumpsRoom = 0.0; // (set by Draw: 1 while every lump a big puff may carry, Bit::Lumps, has a place with a tenth of the room to spare, less as that goes, 0 with the room full: the lumps give way to everything else)
		double LumpsMost = 0.0, LumpsHeld = 0.0, LumpsShown = 0.0, LumpsAt = -1.0e9, LumpsZoom = 0.0; // (the most places asked of late, lumps and all; how long since the count last came near it; LumpsRoom at the last Draw; when that was; and the view's pixels a radian then: see LumpsRoom in Draw)
		// (the lumps' cost held, read at Setup: game.lumps_most, no more puffs than this carry lumps at once, the biggest
		// on the screen first (0: any number); game.lumps_px_most, a puff this wide on the screen or wider carries none,
		// thinning from .0 to .1 pixels (a column the eye is in or under fills the screen with pieces whose lumps are
		// murk and cost the most of all; .1 of 0: any size); game.lumps_least_ink, a lump thinner than this is not
		// laid (0: every one). LumpsTight, set by Draw: how many times its own lumps_px a puff must be across to carry
		// lumps now, 1 with places for all, more with over lumps_most puffs wide enough; it moves by half its size
		// a second at most, so a puff's lumps come and go as slowly as the count changes. LumpSizes: Draw's own list)
		int LumpsMostPuffs = 0;
		double LumpsBigA = 0.0, LumpsBigB = 0.0, LumpsLeastInk = 0.0, LumpsTight = 1.0;
		// (game.lumps_soft_s: LumpsTight closes most of the way to what is asked in this many seconds (0: at once, held
		// only to half its size a second); game.lumps_band: a puff's lumps come in over this share of the width asked
		// (0.25 without the key). With many puffs alike in size most of the kept ones lie in that band and show faint:
		// a narrower band shows them nearer whole)
		double LumpsSoft = 0.0, LumpsBand = 0.25;
		std::vector<double> LumpSizes;
		// (while laying a spent case's body: each point lit as a point on a metal tube, by where it sits across it. Art
		// director 07:08Z: a bright band along the top, a shade side and underside, never a pale plank)
		struct TubeLight
		{
			bool bOn = false;
			Vec Mid, Across, Back, Sun, Half; // (the tube's middle; across it on the screen; toward the eye; the sun; half way between)
			double R = 1.0, Shine = 8.0;
			Rgb Shade, Lit, Spark; // (in shade; full sun; the glint's own light at its peak)
		};
		TubeLight Tube;
		Rgb CaseMouth, CaseGlint; // (ground_cases: the dark inside of an open case's mouth; the colour of its glint)
		double CaseShadow = 0.5, CaseShine = 8.0;
		std::vector<double> CaseUntil; // (when each long-lying case goes: past ground_cases.most_lying, new ones lie only briefly)
		size_t MostLying = 200;
		// (Effects (R2), -TheaterLandBarrelSmoke: each barrel's wisp still curling, where its gun stood and when its last puff goes; at most
		// after_wisp.per_gun from one gun's place and after_wisp.most in all)
		struct Wisping
		{
			Vec At;
			double Until = 0.0;
		};
		std::vector<Wisping> BarrelWisps;
		double CaseFar = 150.0; // (ground_cases.draw_within_m: past it a case is neither counted nor laid)
		double CaseDetailPx = 3.0; // (ground_cases.detail_px: a case narrower on the screen is laid as its tube alone)
		bool CaseDetailed(double Rad, double Deep, const View& Eye) const { return 2.0 * Rad * std::max(Eye.PixelsPerRadian, 1.0) >= CaseDetailPx * std::max(Deep, 1e-3); }
		// (art director 14:03Z, build 1720: from the game's own camera, 19 m off, a 6 cm case is under a pixel across and
		// its lit and shade sides average out to the ground's own tone. ground_cases.hold_px: a case is never narrower on
		// the screen than this, and held so its glint is brighter (far_glint) and wider (far_shine) and its shade side darker (far_shade),
		// a speck of brass with a dark underside; it keeps its whole opacity while its true width is over full_px and is
		// gone under gone_px. nearer_m: laid this many metres nearer the eye, in the same place on the screen, so the
		// game's flat-faced ground never buries it. 0: none of it, as before)
		double CaseHoldPx = 0.0, CaseFullPx = 0.6, CaseGonePx = 0.2, CaseFarGlint = 1.0, CaseFarShade = 1.0, CaseFarShine = 0.0, CaseNearer = 0.0;
		double EdgeK = 0.0;
		double SkyFill = 0.25, SunThrough = 1.5, SunThroughPow = 3.0;
		// (see-through smoke and dust thins out as it comes up to the eye, game.near_eye_fade_m: nothing of a piece whose
		// near edge is within NearGone metres of the eye, all of it from NearWhole on. 0, 0: no such fade)
		double NearGone = 0.0, NearWhole = 0.0;
		// (an aircraft's trail thins out as it comes up to the eye, game.trail_near_eye_fade_m: nothing of it within
		// TrailNearGone metres of the eye, all of it from TrailNearWhole on. By where its line is, not its edge, each end
		// of a strip by its own, so two pieces of a strip agree at the edge they share. 0, 0: no such fade.
		// game.trail_near_eye_looms: nearer than that it still shows while it is small in the picture, all of it while
		// its half width is under TrailLoomsWhole of its distance and none from TrailLoomsGone, so the aircraft the eye
		// rides with still trails its smoke from the engine to the tail. 0, 0: by the distance alone.
		// game.trail_near_eye_least_m: however small, none of it within TrailLeastGone metres and no more than the
		// distance leaves until TrailLeastWhole, so no strip is ever drawn through the eye's own plane)
		double TrailNearGone = 0.0, TrailNearWhole = 0.0, TrailLoomsWhole = 0.0, TrailLoomsGone = 0.0, TrailLeastGone = 0.0, TrailLeastWhole = 0.0;
		double TrailNearK(double OffEye, double HalfWide) const;
		// (SetLensFade: the land battle's fade by depth in front of the lens, and its near plane. 0, 0: none)
		double LensGone = 0.0, LensWhole = 0.0, LensPlane = 0.0;
		// (game.lens_cap, read at Setup: thrown clods fade out between CapThrownGone and CapThrownWhole metres in front of the
		// lens; a smoke or dust card wider than CapCard of the picture, or a light wider than CapLight, fades away over the
		// last CapSoft of that, when it is nearer the lens than CapNear metres (by degrees over the last three tenths of that,
		// so a column of smoke filling the picture from further off is never touched). 0: none, as before)
		double CapThrownGone = 0.0, CapThrownWhole = 0.0, CapCard = 0.0, CapLight = 0.0, CapSoft = 0.3, CapNear = 10.0;
		bool bScars = true; // (SetScars)
		Rgb SunThroughCol = Rgb(1.0f, 0.61f, 0.35f);
		Span ClodRough, ClodTurns; // (read at Setup: every clod asks)
		double ClodShade = 2.5;
		Span ClodTint; // (burst.debris.clods.tint: each clod's own shade of its paint, 0.8 to 1.2 as made)
		double ThrownLong = 0.0; // (burst.debris.clods.most_long: a thrown lump drawn no longer than that times its width; 0, as before)
		bool bLandGrade = false; // (SetLandGrade)
		double LieNearer = 0.0, LieNearerK = 1.5; // (SetSoftContact: how much nearer the eye a lying piece is drawn; game.soft_contact.lie_nearer)
		double LayNear = 1.0; // (while laying one bit: every point of it this share of the way from the eye to where it is, LayEye; see SetSoftContact)
		Vec LayEye;
		// (FX9, game.smoke_budget, the production lead's battle-scale plan of 22 November: a budget for the screen's area
		// of smoke. A piece of smoke under skip_px.0 across at its biggest takes no place and is not laid, whole by
		// skip_px.1; past screens of fill, the big smoke near the lens, over thin_from_px across, is thinned: a share of
		// its pieces left out, by each one's own lot, and the rest that much darker, so a cloud of many keeps its weight.
		// The share follows what is asked over thin_s seconds, never more than thin_most; only with thin 1 as well (the art
		// director refused the thinning at 17:43Z, so thin is 0). on 0 or no row: as before.
		// Smoke here is a puff or a low skirt of a burst or a fire, not a light, a column kept, a trail, a thrown piece,
		// a far form or a funnel's even plume: Smoky)
		double SkipPxLo = 0.0, SkipPxHi = 0.0, ThinScreens = 0.0, ThinFromPx = 150.0, ThinMost = 0.5, ThinS = 1.0, ThinBand = 0.15;
		double ThinShare = 0.0, ThinAt = -1.0e9, ScreenPx = 0.0, ScreenHalfX = 0.0, ScreenHalfY = 0.0, FillSmall = 0.0, FillBig = 0.0, FillLaid = 0.0, SmokeLaidScreens = 0.0;
		bool Smoky(const Bit& B) const;
		// (FX7, game.six_sided, the production lead's battle-scale plan of 22 November: a piece under under_px across on the
		// screen, its long way, is laid as six sides, not Sides. The ring is worked out at six of its points (SixSlot:
		// the first, the one half way round, where a piece's point and tail are, and four between) and every other point
		// of it is a copy of the nearest of those (SixOf), so the mesh is the same and the triangles between two copies
		// cover nothing. At under 8 px the six sides lie within half a pixel of the round. on 0 or no row: as before)
		double SixUnderPx = 0.0;
		bool LaySix = false; // (while laying one bit's own fans: it is small enough, see LayBit)
		int SixSlot[6] = { 0, 0, 0, 0, 0, 0 };
		std::vector<int> SixOf;
		double MostRad(const Bit& B, double Now) const; // (a piece's radius at its biggest by Now, its long way: never under what LayBit lays)
		/** What the scene's grade changes (SetLandGrade), read at Setup and again when it changes: the clods' shade and
		 * tint and every colour Setup keeps (Picture), each from land_grade.<its path> where the scene is under that grade. */
		void ReadGradeLooks();
		double GradeNum(const std::string& Path, double Fallback) const;
		Span GradeRange(const std::string& Path, double Low, double High) const;
		// (fire.lamp: the steady lamps lit at the last frame, by their source's seed, and how far each has come up, so a
		// fire's lamp comes up smoothly and keeps its place against near equals instead of hopping between them)
		struct LampKept
		{
			uint64_t Seed = 0;
			double Level = 0.0;
		};
		mutable std::vector<LampKept> LampsKept;
		mutable double LampsAt = -1.0e18;
		// (a lamp on its way to the scene, SteadyLamps' and the lamp rule's: its source's seed (0 for a flash), its rank,
		// when it was fired (a flash's birth), whether it is a steady light's, and where the merge measures a flash from:
		// its gun's muzzle (Flash::Gun), or the lamp's own place for a flash without one)
		struct LampLit
		{
			Lamp L;
			uint64_t Seed = 0;
			double Rank = 0.0, Born = 0.0;
			bool bSteady = false;
			Vec Gun;
		};
		// (SetLampRule: the scene's rule, and whether it differs from the default at all)
		LampRule Rules;
		bool bLampRuled = false;
		/** A flash's lamp share at Now: 1 on its first frame drawn, half the next (nothing for a one-frame flash), then
		 *  out; a naval gun's (LampS) down from its first frame to nothing by LampS. Under 0: not lit. */
		double FlashShare(const Flash& F, double Now) const;
		/** The lamp rule's rank of a lamp of Strength at At reaching Reach: over the square of its distance from the last
		 *  eye, 5 m at the least, and nothing when its sphere is wholly out of the last view (bByEye); else its strength. */
		double EyeRank(const Vec& At, double Strength, double Reach) const;
		/** Whether a sphere at At of radius Reach can light anything in the last Draw's view (true while the view's width
		 *  is not known): within the view's half diagonal angle of its forward, widened by the sphere's own angle. */
		bool LampInSight(const Vec& At, double Reach) const;
		/** The lamp rule's order: Rank first, then the lamp itself (strength, birth, place, reach, colour, seed), so a tie
		 *  never falls to the order the flashes were laid in. */
		static bool LampFirst(const LampLit& A, const LampLit& B);
		Vec SightAhead; // (the last Draw's forward, and its half diagonal angle in radians: 0 while not known)
		double SightHalf = 0.0;
		/** The lamp rule's merge of flashes into one lamp a salvo (LampRule's MergeM, MergeS and MergeMost). */
		void MergeLamps(std::vector<LampLit>& Lits) const;
		/** A naval gun's fireball and smoke ring (muzzle.<row>.fireball_m, ring_m): its own dice. */
		void GunFireball(double Now, const Vec& At, const Vec& Fw, const std::string& Row);
		Vec LastEye;
		double LastPpr = 1000.0; // (the last frame's pixels a radian: a water column far off in the picture is fewer pieces)
		Vec LastSun; // (the way to the sun in the last frame drawn; none before the first: a steam plume's rags on its far side from the sun are born in its shade)
		bool bEyeKnown = false; // (the eye of the last frame drawn: a hit close to it throws clods past the lens)
		std::vector<Bit> Bits; // alive or still to be born (in no order: Prune puts the last into each gap)
		// (Bit::Serial: the last number given, and how many of Bits have theirs. Pieces made since the last Draw are at
		// the end, in the order made, and are numbered in that order before Prune moves any)
		uint64_t Serials = 0;
		size_t Numbered = 0;
		// (Bit::CutAt: the trail runs given out, and the cuts asked since the last Draw, each for the pieces of its run
		// born by LaidBy, done at the next Draw in one pass; and the fade before a cut)
		struct TrailCut
		{
			uint32_t Run = 0;
			double LaidBy = 0.0, End = 0.0;
		};
		uint32_t TrailRuns = 0;
		std::vector<TrailCut> TrailCuts;
		double TrailCutFade = 3.0;
		void CutTrails();
		double CutOf(uint32_t Run_, double Born, double Was) const;
		std::vector<Bit> Marks; // craters: for the whole battle
		std::vector<Flash> Flashes;
		// (a fuel ball's lamp, FuelBall: lit while it burns, up as it swells and down as it goes to smoke, rising with it)
		struct BallLamp
		{
			Vec At, Vel;
			double Born = 0.0, Life = 1.0, Strength = 0.0, Radius = 10.0, Lift = 0.0;
			uint64_t Seed = 0;
			Rgb Col;
		};
		std::vector<BallLamp> BallLamps;
		bool BallLampAt(const BallLamp& L, double Now, double Day, Lamp& Into) const; // (false when it is not lit at Now)
		std::vector<Knock> Knocks;
		std::vector<Source> Sources;
		std::vector<int> GunRounds; // rounds fired by each gun so far
		std::vector<AirRow> AirRows;
		/** A nation's tracer colours (air_guns.tracers.nations.<nation>): its glowing body and its hot core, each as
		 *  written for the game's picture, and how far past white each burns. */
		struct TracerHue
		{
			std::string Name;
			Rgb Body, Core;
			double BodyOver = 1.0, CoreOver = 1.0;
		};
		std::vector<TracerHue> TracerHues;
		const TracerHue* HueOf(const char* Nation, bool bCannon, bool bShip = false); // (null: no air look in the recipe. bShip: a ship gun's, the nation's ship colours where it has them)
		// (the air look: AirTracerLife seconds alight, a streak AirStreakS seconds of flight long, dimming from
		// AirDimFrom of its life to AirDimTo and out over AirOutS; no more than AirTracerMost alight at once, the
		// guns farthest from the eye going without first)
		bool bAirTracers = false;
		double AirTracerLife = 1.2, AirStreakS = 1.0 / 30.0, AirDimFrom = 0.667, AirDimTo = 0.5, AirOutS = 0.045, AirTracerNear = 300.0;
		double AirBodyPx = 0.0, AirCorePx = 0.0; // (its glow's and its core's width in the pixels the scene tells the kit; 0: the ruled streak's)
		double AirWhiteWithin = 0.0, AirWhiteBeyond = 0.0; // (its core burns white-hot within the first of the eye, its own colour from the second on; 0: its own everywhere)
		double AirCoreInner = 0.0; // (over 0: its core is whole only this share of the way out from its middle and melts into the glow from there; 0: whole to its edge)
		int AirTracerMost = 1500; double AirOneFanBeyond = 0.0;
		double AirGlowSoft = 0.0; // (over 0: by day its glow is that strong beside the core and fades smoothly to nothing at its edge, never a solid bar; 0: solid across, as it was)
		double AirFarPx = 0.0; // (over 0: a one-fan tracer is a solid dash that wide in the core's colour; 0: the glow alone, as it was)
		// (game.air_far, the air scene's far forms; 0: off. See Setup)
		double FarTrailFromM = 0.0, FarTrailBand = 0.15, FarTrailPuffs = 40.0, FarTrailBillow = 0.25; int FarTrailKeep = 3; double FarGunFromM = 0.0, FarGunDashS = 0.2, FarGunFlightS = 0.12;
		void FarGunDash(double T, const Vec& At, const Vec& Way, const Vec& Carry, const AirRow& Row, double Mps, const TracerHue& Hue);
		std::vector<double> AirTracerEnds; // (when each air tracer now alight goes out)
		int HoseMost = 0; std::vector<double> HoseEnds; // (fx-air r2: no more than HoseMost ship guns' hose lines at once, and when each goes)
		std::vector<Firing> Firings; // the air guns
		std::vector<TrailRow> TrailRows;
		std::vector<DustRow> DustRows;
		struct Dusting
		{
			int Id = 0;
			double Last = -1e9, Owed = 0.0;
			double DropUntil = -1e18; // (FX5: a far trail let go in the heaviest barrage lays nothing until then)
			Vec At;
		};
		std::vector<Dusting> Dusts; // (the vehicles' dust sockets being laid)
		std::vector<Trailing> Trails; // the aircraft trails being laid
		/** A falling aircraft shedding its debris row over its fall. */
		struct Falling
		{
			int Id = 0, Left = 0;
			bool bOn = false, bWing = false;
			double Span = 11.0, DeckZ = 0.0, Next = 0.0, Last = 0.0;
			Vec At, Vel;
			uint64_t Seed = 0;
		};
		std::vector<Falling> Falls;
		// (the tracer's look, read at Setup: one streak for every gun, on the ground and in the air)
		double TracerHalf = 8.0, TracerLeast = 3.0, TracerMost = 5.0, TracerDayWide = 1.5, WholePathBelow = 100.0, WholeFor = 0.015;
		// (art director 13:54Z, build 1720: by day a tracer is a solid pale core, tracer.day_core_px across, inside a warm
		// edge (tracer.day_edge), tracer.day_px across in all, at its whole opacity along its whole length, so it stands
		// 60 luma over pale ground and is never a faint hairline. 0: by day it widens by day_width, as before)
		double TracerDayPx = 0.0, TracerDayCorePx = 0.0, TracerDayOver = 1.0; // (day_over: its core that far past white by day; see LitGain)
		Rgb TracerDayEdge;
		double GlintPow = 30.0; // (a falling piece's glint: how tight the sun's mirror spot on it is, air_debris.glint_power)
		double DayFrom = 0.38, DayFull = 0.9, DayLampShare = 0.25;
		Rgb DayLampCol;
		int FrameNo = 0; // (frames drawn: flashes' lamps go by frames, not seconds)
		Rgb TracerCol, TracerPale, TracerDayCore, FlameCol, FlashCoreCol, GunSmokeCol, CannonHeartCol;
		std::vector<Vec> Out;
		std::vector<uint8_t> OutRgba;
		struct Order
		{
			const Bit* B;
			double Deep;
			bool bKeep; // (counted first when the room is short: a kept oil column, or a light)
			double Far = 0.0; // (a puff that may carry lumps, Bit::Lumps: how far it is from the eye; else 0)
			uint64_t Serial = 0; // (Bit::Serial: two as deep are laid in the order they were made, wherever they lie in Bits)
		};
		std::vector<Order> Sorted;
		struct ColumnSpan
		{
			int Id = 0, Bin = 0;
			double Lo = 0.0, Hi = 0.0; // (along the screen's right, level)
		};
		std::vector<ColumnSpan> Spans; // (this frame: how far across the screen each oil column reaches, by fire and by ColumnBin metres of height)
		static constexpr double ColumnBin = 20.0;
		bool SpanAt(int Id, double Z, double& Mid, double& Half) const;
		size_t TwinBits = 0; // (twins in sight at the last Draw: each takes a second slot)
		// (the room a gate asks about: what was in sight at the last Draw and whatever was made since. A piece 5 km off,
		// never drawn, holds no place: counted, far debris shut off the cases, gun smoke and showers beside the eye. The
		// pre-check, 06:42Z)
		size_t InSight = 0, HeldAtDraw = 0;
		size_t SeenBits = 0, SeenTwin = 0; // (of InSight and TwinBits, what was on the screen: see RoomSeen)
		size_t CaseOff = 0; // (the places spent cases off the screen hold in the room asked: see RoomForCases)
		size_t WispIn = 0, WispSeen = 0, WispSince = 0; // (Effects (R2): the barrels' wisps counted at the last Draw, of them on the screen, and laid since: left out of the room the gates read)
		// (the eye given may be the frame before's: the campaign map reads the camera before it has moved for the frame.
		// So what the eye moved and turned between the last two frames is allowed for again, twice over, before a bit
		// is called off the screen. A still eye changes nothing. The pre-check, 14:55Z)
		Vec EyeWas, AheadWas, UpWas;
		bool bEyeWas = false;
		double EyeMoved = 0.0, EyeTurned = 0.0;
		// (the vehicles' dust's own count: in sight at the last Draw and laid since; its gate takes the fuller of the room
		// and dust.vehicles.most_of_room of it. The pre-check, 07:10Z)
		size_t DustInSight = 0, DustSince = 0;
		double DustRoom = 0.4, DustFullWithin = 120.0;
		// (the steep high eye's thinning of dust near the units, worked out once a Draw: how much, and where the dust
		// sockets are now; read at Setup from dust.vehicles.from_above)
		double SteepThin = 0.0;
		std::vector<Vec> NearSockets;
		Span ThinAbove{ 15.0, 25.0 }, ThinSteep{ 0.3, 0.5 }, ThinNear{ 6.0, 12.0 };
		double ThinKeep = 0.25;
		// (game.from_above, SetFromAbove: how much the eye is a high steep one this Draw, the shadows' strength, and the
		// recipe's numbers, read at Setup)
		bool bFromAboveAsked = false, bAboveOn = false;
		double FromAbove = 0.0, ShadeTwinA = 0.0;
		Span AboveEye{ 100.0, 220.0 }, AboveSteep{ 0.4, 0.7 };
		double AboveLit = 0.0, AboveShadow = 0.0, AboveWide = 1.5, AboveLeastM = 3.0, AboveLeastR = 1.5, AboveStretch = 2.5;
		double AboveDarkLit = 0.45, AboveDarkest = 0.55, AboveSoftM = 60.0;
		Span AboveDarkLum{ 0.015, 0.04 };
		int AboveEvery = 2, AboveMost = 300;
		Rgb AboveLitCol, AboveDarkCol, AboveShadowCol;
		std::unordered_map<uint64_t, float> ShadowCells; // (this Draw's shadows: how much light each patch of ground has left, by the shadows' sizes)
		int LayShadows(int Slot, double Now, const View& Eye); // (the shadows of the big smoke, laid with the craters: how many slots they took)
		// (and the room they may take: what the rest left last Draw, eased; how many were laid then; the puffs that may cast
		// one, by Bit::Serial, so the oldest smoke keeps its shadow when the room is short)
		int ShadowRoom = 0, ShadowsWere = 0;
		std::vector<std::pair<uint64_t, uint32_t>> ShadowCands;
		size_t RoomAsked() const { const size_t All = InSight + (Bits.size() > HeldAtDraw ? Bits.size() - HeldAtDraw : 0) + TwinBits; return All - std::min(All, WispIn + WispSince); }
		/** FX5, room shares by kind (game.room_shares, off by default): the room every gate but the vehicles' dust reads.
		 *  Off, RoomAsked. On, RoomAsked less the dust trails' own pieces, up to their share (dust.vehicles.most_of_room),
		 *  so a column of tanks never thins the bursts, their smoke's slow fade or a fire: the cloud rig (11-22) had 120
		 *  tanks take a third of the lingering smoke at 3.5 shells a second. The dust thins its far trails to fit what the
		 *  rest leaves (ShareThin): fewer, longer, softer pieces, never gaps, and never stopped, so a heavy barrage no
		 *  longer leaves every tank without a trail; a vehicle within dust_full_m of the eye keeps its whole trail. */
		size_t RoomGate() const
		{
			if (!(ShareDustFloor >= 0.0))
			{
				return RoomAsked();
			}
			const size_t Asked = RoomAsked();
			const size_t Dust = std::min(DustInSight + DustSince, static_cast<size_t>(DustRoom * std::max(1, MaxPuffs)));
			return Asked - std::min(Asked, Dust);
		}
		double ShareDustFloor = -1.0, ShareDustTop = 0.92, ShareDustMemory = 4.0, SharePeak = 0.0;
		double ShareDustFullM = 300.0, ShareDustMostThin = 8.0, ShareDustThinS = 2.0, ShareDustLeastAlive = 2.0, ShareThin = 1.0, ShareThinAt = -1.0e9;
		double ShareDrop = 0.0, ShareDropHoldS = 2.0, ShareDropFrom = 1.0, ShareDropBack = 0.9; // (FX5: how many far trails are let go when thinning is not enough; and for how long at least)
		double SharePeaks[8] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		int64_t SharePeakSlot = std::numeric_limits<int64_t>::min();
		/** The room asked for on the screen alone: what the last frame laid, and what has been made since. The close
		 *  grit (spent cases) goes by this: build 1720's test barrage, off the screen behind the eye, held the room over
		 *  three fifths full, and the machine gun in front of the eye threw no brass at all. The same as RoomAsked when
		 *  the view's width is not known. */
		size_t RoomSeen() const { const size_t All = SeenBits + (Bits.size() > HeldAtDraw ? Bits.size() - HeldAtDraw : 0) + SeenTwin; return All - std::min(All, WispSeen + WispSince); }
		/** What a spent case is thrown by: the screen's room, and the brass lying off it. A barrage off the screen does
		 *  not stop the brass; brass off the screen does, or guns firing beside the eye fill the room asked with cases
		 *  nobody sees (three places each) and every shell throws half its shower for them. */
		size_t RoomForCases() const { return RoomSeen() + CaseOff; }
		bool OffScreen(const Bit& B, const Vec& At, const View& Eye) const; // (wholly outside the view, with room to spare for its size and the eye's shake)
		std::vector<Bit> Roots; // (each frame: the young end of each trail, from its point back to its newest hanging puff)

		double Num(const std::string& Path, double Fallback) const;
		double Pick(Dice& D, const std::string& Path, double Low, double High) const; // a number in the recipe's range
		Span Range(const std::string& Path, double Low, double High) const;
		Rgb Picture(const std::string& Path, const Rgb& Fallback) const;
		bool Has(const std::string& Path) const;
		double Height(double X, double Y) const;
		Paint PaintOf(const std::string& Ground) const;
		const char* GroundOn(const char* Ground) const; // (grounds.<ground>.on off: "earth"; else Ground as given)
		void Throw(Bit& B) const; // a clod's flight worked out against the ground
		void Chunk(Bit& B, Dice& D) const; // a clod's lumpy, tumbling, lit-and-dark shape

		void Wink(double T0, const Vec& At, double Scale, Dice& D, const std::string& Size = std::string()); // (Size: the flash by the size's own flash_k and flash_s)
		bool GroundColour(const Vec& At, double& R255, double& G255, double& B255) const; // (the ground there as the game's screen shows it, if the game can say)
		// (what to write for the game's screen to show a value: picture.to_land, read once and then asked)
		struct LandTable
		{
			static constexpr int Most = 64;
			double Write[Most] = {};
			int Count = 0;
			double ToLand(double Screen255) const;
		};
		LandTable LandTableOf() const;
		void RaggedFlash(double T0, const Vec& At, double K, double Small, double GrowK = 1.0, double LastsS = 0.0); // (a shell's flash by the gun flashes' rule: burst.flash.tongues; GrowK times as big, and LastsS seconds long where that is over 0)
		void Skirt(double T0, const Vec& At, double K, const Paint& P, double DustK, Dice& D, const std::string& Size = std::string(), double Few = 1.0); // (Size: a skirt that rolls out to its cloud's width and hangs with it: burst.shock_ring.of_cloud. Few: burst.far, that share of its pieces, each as much wider)
		void Bulk(double T0, const Vec& At, double CrownH, double K, const std::string& Size, const std::string& Ground, const Paint& P, double CloudM, double LooksS, Dice& D, double Many); // (a burst's cloud as a dense column of earth standing on the ground in lighter smoke: burst.bulk)
		void Crown(double T0, const Vec& At, double CrownH, double K, double Slow, const Paint& P, double ClodK, double ShowerK, const std::string& Size, Dice& D, double Few = 1.0); // (Few: burst.far, see Shell)
		void Smoke(double T0, const Vec& At, double CrownH, double K, double Scale, const std::string& Size, const std::string& Ground, const Paint& P, double DustK, Dice& D, double Many = 1.0);
		// (a shell falling in the sea by its calibre, SeaSplash: how its column stands and falls, its foam, its dye and its
		// floor far off. Given to Spout; none, and the spout is as it always was)
		struct SplashLook
		{
			double RiseS = 1.0, HangS = 1.0, FallS = 2.0, FoamM = 10.0, FoamS = 10.0, CrownWide = 0.0;
			Rgb Tint = Rgb(1.0f, 1.0f, 1.0f), ShadeTint = Rgb(1.0f, 1.0f, 1.0f), Stain;
			double DyeK = 0.0, DyeTop = 0.3, DyeFoot = 0.5, DyePale = 0.5, StainK = 0.0, MistDye = 0.4;
			double FarOver = 1.0, FarFoot = 0.0, MistS = 0.0, FootWhite = 0.0;
		};
		void Spout(double T0, const Vec& At, double CrownH, const std::string& Size, Dice& D, const SplashLook* Look = nullptr); // a hit in the sea: white water thrown up and falling back
		void Scar(double T0, const Vec& At, double Across, const Paint& P, bool bSnow, Dice& D);
		void Armour(double T0, const Vec& At, double Scale, Dice& D);
		/** A tracer's streak (two layers, paler along its middle), its tail at Start when born, flying at Vel along Way;
		 * Drag and Drop (gravity) slow and bend it, for the air guns' long flights. */
		void Tracer(double Born, const Vec& Start, const Vec& Way, const Vec& Vel, double For, double Drag, double Drop, double Half = -1.0, double AlphaK = 1.0, double Behind = 0.0, double Until = 0.0);
		/** An air gun's tracer in a nation's colours (the air look): see Bit::BySpeed. */
		void AirTracer(double Born, const Vec& Start, const Vec& Way, const Vec& Vel, const AirRow& Row, const TracerHue& Hue, bool bOneFan = false);
		const AirRow& AirRowOf(const std::string& Kind);
		void AirRound(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, double Mps, bool bTracer, bool bSmoke, Dice& D, const TracerHue* Hue = nullptr);
		// (fx-air r2: a ship gun's hose, the line from this round back to the last; where a hose's two ends are now, as one
		// streak to lay (false: nothing to lay); how long a tracer burns, to its row's burn_out_m; a column puff's paling)
		void HoseLink(double T, Firing& F, const Vec& At, const Vec& Vel, const AirRow& Row, const TracerHue* Hue, double Gap);
		bool HoseNow(const Bit& B, double Now, Bit& Out_) const;
		double BurnLife(const AirRow& Row, double Mps) const;
		void PaleUp(const Bit& B, const Vec& P, const View& Eye, Rgb& Col, Rgb& Floor, Rgb& Lit, double& A) const;
		void CannonFlash(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, Dice& D);
		void Spent(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, Dice& D);
		const TrailRow& TrailRowOf(const std::string& Kind);
		void TrailKind(Trailing& Tr, const std::string& Name, double Now, bool bChange); // (a trail takes a kind: its row's clocks start)
		const DustRow& DustRowOf(const std::string& Ground);
		void TrailPuff(double T, const Vec& At, const Vec& Carry, Trailing& Tr, const TrailRow& Row, Dice& D);
		void PlumeMove(double Now, Trailing& Tr, const TrailRow& Row, const Vec& At, const Vec& Carry, double PowerIn); // (a funnel's plume, laid by time)
		void Shed(double Now, const Vec& At, const Vec& Vel, const Vec& Throw, double SpanM, double DeckZ, int Sort, Dice& D); // (one piece: Sort 0 a skin plate, 1 a heavy piece, 2 a wing)
		void TornFireball(double Now, const Vec& At, const Vec& Vel, double SpanM, Dice& D, double Across = 0.0, double Hang = 0.0, double Whiter = 1.0); // (Across over 0: the ruled fire cloud, that many spans across, its smoke hanging Hang s)
		void FuelStreaks(double Now, const Vec& At, const Vec& Vel, double SpanM, int Count, uint64_t Seed, double CloudR = 0.0); // (burning fuel curving down out of an explosion: orange heads, black tails)
		void MetalSparkle(double Now, const Vec& At, const Vec& Vel, double SpanM, int Count, double Over, uint64_t Seed); // (small metal catching the sun as it falls away)
		double FlameOf(const Trailing& Tr, const TrailRow& Row) const; // (a fire trail's flame length: the row's, or by the aircraft's span)
		void Run(double Now); // the steady sources: fire, smouldering
		void OilColumn(Source& S, double T, bool bPall = false); // one puff of an oil fire's column (or its long pall), born at T
		Vec OnSlick(const Source& S, Dice& D) const; // a point anywhere on an oil fire's slick
		void OilBase(Source& S, double T); // one puff of the low smoke rolling off the slick, born at T
		OilRow OilRowOf(const std::string& Kind) const;
		OilRow OilRowAt(const std::string& Prefix) const; // (the row under any prefix, every dial held to a sane span)
		bool JoinBurn(double Now, const Vec& At, double Across, double Seconds, int Id); // (fire.join: a fire lit by one already burning joins it; true when it did)
		void Burning(double Now, const Vec& At, double Across, double Seconds, int Id, bool bOnShip, bool bVent); // (Burn and Vent: a burning thing, joined or not)
		int OwnColumn(double Now, const Source& Fire, bool bLand = false); // (a big fire's own column: started, or this fire joined to one standing near; that column's Id, or -1 for none)
		SteamRow SteamRowOf(const std::string& Kind) const; // (the row, every dial held to a sane span)
		CookRow CookRowOf() const;
		void SteamRag(double T, const Vec& At, const Vec& Way, double Reach, double Across, double Push, const SteamRow& R, Dice& D); // (one rag of steam, born at T: Push 1 at full pressure, down to the row's least)
		void CookPop(const Source& S, double T); // (one pop of a cook-off)
		void CookJet(const Source& S, double T, double Full); // (one step of its hatch jet, Full 0 to 1 of its height)
		TongueRow TongueRowOf(bool bLand = false, bool bVent = false) const; // (bLand: fire.land_tongues where the recipe has that row, else fire.tongues; bVent: fire.vent_tongues before either)
		/** A burning thing on land under a ship's size (a tank's, not a ship's): no wider than fire.smoke_column.from_m,
		 * no higher over the ground than fire.land_column.above_m, and not under way. The test OwnColumn's land column
		 * makes, without its from_m and from_s; fire.land_tongues and fire.lamp.land_strength are for these. */
		bool LandFire(const Source& Fire) const;
		GlowRow GlowRowOf() const;
		LateRow LateRowOf() const;
		/** How far a fire lit at Start with seed Seed has burnt down by T (fire.late): 0 not yet, 1 all the way. */
		static double LateShare(const LateRow& R, uint64_t Seed, double Start, double T);
		void FireGlow(Source& S, double Now, const GlowRow& R, double Light = 1.0); // (fire.glow: a burning thing's warm light on the ground, by the clock)
		bool LandColumnStands(int Column, double T) const; // (fire.land_column: that column still stands over its fire at T)
		void FireTongues(const Source& S, double T, const TongueRow& R, const Rgb& Flame, const Rgb& Heart, const Rgb& Ember, const LateRow* Late = nullptr, double LateS = 0.0); // (one step of a burning thing's flames, by fire.tongues; Late, LateS: burning down, fire.late)
		bool UnderOil(const Source& Column, double T) const; // (burning oil stands over one of its fires at T: the oil's column is hers)
		BankRow BankRowOf(double Across, uint64_t Seed) const;
		void SmokeBank(const Source& S, double T, bool bFlames, double After, const Rgb& Foot, double FootClear, double FootK, double Pale = 0.0, const Rgb& PaleCol = Rgb());
		/** A smoke screen's wall (Kind 6): its lumps laid up to Now. */
		void ScreenLumps(Source& S, double Now);
		void GrenadeLumps(Source& S, double Now);
		// (a smoke screen laid at sea, ScreenLay: one a ship's screen. Last, At, Vel: her last call; Owed, the seconds since
		// the last puff; N, the next puff's number; Wander, the screen's slow drift across the wind)
		struct SeaScreen
		{
			int Id = 0;
			bool bOn = false, bFunnel = false;
			double Last = 0.0, Owed = 0.0, Wander = 0.0, Lift = 1.0;
			Vec At, Vel;
			uint32_t N = 0;
			uint64_t Seed = 1;
		};
		std::vector<SeaScreen> SeaScreens;
		void SeaScreenPuffs(SeaScreen& S, double Now, const Vec& At, const Vec& Vel);
		void Prune(double Now);
		/** Smoke that fades gradually into the air (Brian, 21 November 18:00Z: "have the smoke effects gradually fade
		 * away into the air not disappear right away"; the art director's ruling of 19:58Z). A row of the recipe's
		 * linger says how one kind's cloud goes, counted from the moment of its burst. Until thick_s every puff is as
		 * it was laid. Then the cloud is in its tail, to gone_s: of the smoke it had, left_third is left a third of
		 * the way and left_two_thirds at two thirds (tuned so the cloud's opacity is half its thick opacity at the
		 * first and a tenth at the second), and it ends with no step. The share keep of its puffs stay to the end
		 * and take over the smoke of the others, which retire over the first go_by of the tail. A puff that stays
		 * widens on by its own law and wide as much again, its smoke thinner over the wider face, and stands spread
		 * as much further again from the cloud's middle, so the whole cloud grows as it thins; the cloud's top climbs
		 * lift_mps more while its foot stays (in the air, all of it climbs); and between pale_from and pale_by of
		 * the way it pales pale of the way to the colour of the air behind it. Which puffs stay goes by their own
		 * count: no dice are thrown. No row, or no times in it: nothing changes. */
		struct LingerRow
		{
			bool bOn = false;
			double ThickS = 0.0, GoneS = 0.0, Keep = 0.5, GoBy = 0.4;
			double Third = 0.1, TwoThirds = 0.02, Wide = 0.0, Spread = 0.0, Lift = 0.0, Pale = 0.0, PaleFrom = 0.33, PaleBy = 0.8;
			Rgb To;
			double StayLow = 0.0; // (a burst with bulk: how much the likelier a puff at the cloud's foot is to stay than one at its top, so the base thins last)
			double BulkThird = 0.1, BulkTwoThirds = 0.02; // (a burst with bulk: its own Third and TwoThirds, the row's unless the recipe gives them)
		};
		/** The row linger.<Kind>, its times from linger.<Kind>.by.<Name> when Name is given (a shell's size, a gun). */
		const LingerRow& LingerOf(const std::string& Kind, const std::string& Name = std::string());
		std::map<std::string, LingerRow> LingerKept;
		uint64_t LingerNo = 0;
		/** Puts B, a puff of smoke of a burst that began at T0, under that row. Mid is the cloud's middle then, carried
		 * by the wind at MidWindage; Tall, over 0, the height of the cloud above Mid's own, by which a puff at its
		 * top climbs in full and one at its foot not at all (0 or less: every puff climbs alike). */
		void Lingering(Bit& B, const LingerRow& R, double T0, const Vec& Mid, double MidWindage, double Tall, double Spare, double KeepK = 1.0, double StayBias = 1.0); // (KeepK: this part of the cloud keeps that many times the row's share of its puffs; StayBias: this puff that much the likelier to be one that stays)
		// (short of room, a new cloud keeps fewer of its puffs to its end and the rest retire sooner: the share of its
		// row's keep and go_by it gets, by how much of the room, game.max_puffs, is asked for (RoomAsked, as the dust
		// goes by; Own: this burst's own pieces, left out of the count); all of them under linger.spare_from of the
		// room, down to linger.spare_least at linger.spare_by. The puffs that stay take the others' smoke as ever, so
		// the cloud goes by the same law with fewer pieces, and a long barrage's old smoke never crowds out its new
		// bursts)
		double LingerSpare(size_t Own = 0) const;
		double LingerSpareFrom = 0.5, LingerSpareBy = 0.9, LingerSpareLeast = 0.3;
		double SpareAtBurst = 1.0; // (a shell's: taken as Shell begins, before its own pieces are in the count)
		// (a shell's thrown earth, burst.throw, read once as Shell begins: how much lighter than the ground's heavy earth
		// it is, piece by piece (Mottle), its darkest colour, and the light its own flash throws on it. InFlash gives a
		// thrown piece all of them: Share of the light, and OfLighter of the way to Lighter times as light)
		struct ThrowRow
		{
			bool bFloor = false, bGlare = false;
			Rgb Floor, GlareCol;
			double GlareK = 0.0, GlareS = 0.0, GlareReach = 1.0, T0 = 0.0, Lighter = 1.0, Mottle = 0.0;
			Vec At;
		};
		ThrowRow ThrowIs;
		void InFlash(Bit& B, double Share, double OfLighter = 1.0) const;
		// ---------------------------------------------------------------------------------------------------------------
		// The far form (the recipe's far row). Beyond the draw range (SetDrawBeyond: the air scene's 4 km) a near look is
		// hundreds of pieces under a pixel each; the far form draws the same thing as a few large soft pieces, each held
		// to a pixel floor, in the near look's own colours, sizes and times, and its smoke goes by the near cloud's own
		// slow fade (linger), so it reads from 6 to 20 km as the near look does and costs a handful of places. A
		// general piece for anything seen from far off:
		// - a FarRow, far.<kind>, says how that kind looks far off: its flash (a light held to flash.least_px, with a
		//   soft glow) and its parts (FarPart), each a soft piece with its own share of the thing's width, its growth,
		//   its height at birth, its colour between the thing's dark heart and its light smoke, its floor, and its life
		//   (its own seconds, or as long as the thing's cloud lingers). A part that merges is laid once for several
		//   things in a row, drawn out along it, when they stand too close on the screen to be told apart;
		// - a FarSpot says what happened: where and when, how wide its near look is, its flash, its colours, its
		//   near cloud's linger row and life, and the row it carries the merging parts for (from At along Along);
		// - FarLay(Spot, Row, Rep) lays its pieces into the battle, marked Bit::FarRep (2: it stands in for a near look
		//   that was laid, and shows only beyond the range; 3: it is the only look). Draw does the rest as for any piece:
		//   FarShare says how much of a marked piece shows for an eye, crossing over far.hand_over of the range.
		// Today's kind is a bomb's strike (far.bombs, by BombStick). A stretch of vapour trail or a flak salvo is a FarSpot
		// a moment or a length of trail with a row of its own (far.<kind>): its own parts, the same laying and drawing.
		struct FarPart
		{
			bool bOn = false, bUpright = false, bMerges = false, bLingers = false;
			double R0 = 0.2, R1 = 0.5, Late = 1.0, GrowS = 2.0, Grow = 0.5; // (radius at birth and by grow_s, shares of the width; late: on to that many times R1 by its life's end)
			double Tall = 1.0, TallEnd = 1.0, TallS = 0.0, SettleS = 1.0; // (drawn out upward Tall times at birth, held tall_s, TallEnd times by tall_s + settle_s)
			double Flat = 0.4, Up = 0.5, Rise = 0.0, Windage = 0.75, Lift = 1.0; // (an upright's height for its width; middle's least height in its radii; climb; drift; how much of the row's lift in its tail)
			double Alpha = 0.5, Heart = 0.7, Rim = 0.1, Inner = 0.5, Rough = 0.2, Shade = 1.0, Light = 1.0, FadeIn = 0.05, Hold = 0.4;
			double PaintFrom = 0.5, PaintTo = 0.5; // (its colour, 0 the thing's dark heart to 1 its light smoke, at birth and by settle_s)
			double LeastPx = 0.0, Keep = 0.5; // (never under LeastPx across on the screen, keeping at least Keep of its opacity there)
			double LifeS = 0.0, LifeThick = 0.0; // (its own life in seconds, or that share of its linger row's thick time; neither, it lingers as its cloud does)
			double Third = 0.0, TwoThirds = 0.0; // (lingering, how thick it looks a third and two thirds through its tail, shares of how thick it looked: one far piece is one layer, not the near cloud's many; 0: the row's smoke left)
		};
		struct FarRow
		{
			bool bOn = false;
			double WalkMps = 85.0, MergePx = 0.0;
			double FlashPx = 0.0, FlashKeep = 0.9, FlashOver = 1.0, FlashM = 6.0, FlashLong = 1.0; // (flash.least_px, keep, over; width_m at flash_k 1, its size's flash_s times long)
			double GlowPx = 0.0, GlowAlpha = 0.0, GlowS = 0.1, GlowWide = 2.0;
			Rgb FlashCol;
			FarPart Crown, Body, Veil, Skirt; // (on land: the earth thrown up, the dense cloud of earth on the ground, the lighter smoke that lingers and spreads, the low dust)
			FarPart Column, Foam, Mist; // (on the sea: the white water standing up, the foam that stays, the thin mist)
		};
		struct FarSpot
		{
			Vec At, Along; // (where, on the ground; the way of the row it stands for)
			double Born = 0.0, Across = 30.0, Long = 0.0, LongS = 0.0; // (when; its near cloud's width; the length of the row it stands for from At, 0 itself alone, and the seconds that row takes to land)
			double FlashM = 0.0, FlashS = 0.0, LingerS = 0.0; // (its flash's width and time, 0 none; its near cloud's own life, never gone sooner)
			Rgb Dark, Light;
			std::string Linger; // (its linger.shell row's name: the size)
			bool bCarrier = true, bWater = false;
			uint64_t Seed = 1;
		};
		FarPart FarPartOf(const std::string& Path) const;
		const FarRow& FarRowOf(const std::string& Kind);
		std::map<std::string, FarRow> FarRows;
		void FarLay(const FarSpot& S, const FarRow& R, uint8_t Rep);
		double FarShare(const Bit& B, const Vec& EyeAt) const; // (how much of a piece marked Bit::FarRep shows to that eye)
		double FarBeyond = 20000.0, FarBand = 0.0;
		bool bFarKept = false;
		uint64_t FarSeed = 1;
		/** A stick of bombs not yet all landed (BombStick): its bombs are laid by Draw, each at its own moment. */
		struct Stick
		{
			int Id = 0, Count = 0, Laid = 0, Step = 0; // (Step: every that many bombs carries the merging parts for the bombs after it; 0 not yet known)
			Vec First, Way, Mid;
			double T0 = 0.0, Dt = 0.0, Gap = 0.0; // (the first one's moment, the seconds and the metres between bombs)
			std::string Size, Ground;
			uint64_t Seed = 1;
		};
		std::vector<Stick> Sticks;
		void LayStrikes(double Now); // (every bomb due by Now, in their order)
		void LayStrike(Stick& S, double T);
		/** And every puff of smoke laid since the pieces numbered First (what a call has just laid): not its lights,
		 * nor what it threw that falls back, nor a piece already under a row. */
		void LingerFrom(size_t First, const LingerRow& RowIn, double T0, const Vec& Mid, double MidWindage, double Tall);
		void MuzzleLaid(double Now, const Vec& At, const Vec& Facing, const char* GunIn, const char* GroundIn, const GunLook* Look = nullptr);
		/** Effects (R2), -TheaterLandBarrelSmoke: the barrel's wisp after the shot, muzzle.<gun>.after_wisp (Muzzle lays it last). */
		void AfterWisp(double Now, const Vec& At, const Vec& Facing, const char* GunIn);
		void AirHitLaid(double Now, const Vec& At, const Vec& Normal, const Vec& Carry, const char* Kind);
		void AirFlakLaid(double Now, const Vec& At, const char* Kind);
		Rgb SkyTail; // (what old smoke pales to once the scene has said what is behind it: see SetSky)
		bool bSkyKnown = false;

		Vec Where(const Bit& B, double Age) const;
		// (a sea battle's far floors, UseSea: 0 until then, and what is laid is as it always was)
		bool bSeaFar = false;
		// (FX3, the production lead's battle-scale plan of 22 November, burst.far: a hit on land outside a sea battle that
		// stands small in the picture, or lands with the room busy, is fewer and bigger pieces; and far off, a piece of
		// one too small to see takes no place. No row: as before)
		bool bLandFar = false;
		double LandFullPx = 0.0, LandLeast = 0.1, LandHalvePast = 1800.0, LandNearBusy = 0.5, LandOverBusy = 0.125, LandGonePx = 0.0, LandGoneBeyond = 300.0;
		double LandSmokePow = 0.5, LandSkirtPow = 0.5;
		// (burst.far.alpha_pow, the art director 12:33Z: a see-through piece laid Bigger is as much thicker, up to alpha_most,
		// so where fewer layers lie the ground does not show through and tint the smoke. FewAlphaPow is alpha_pow only while
		// Shell lays a land hit that keeps Few under 1; 0 otherwise, so a sea hit's far pieces are as they were)
		double LandAlphaPow = 0.0, LandAlphaMost = 0.9, FewAlphaPow = 0.0;
		double Thicker(double Alpha, double Bigger) const;
		bool bLandGoneDirt = false;
		size_t LandHits = 0, LandHitsFew = 0; // (since Setup: land hits, and those laid with fewer pieces)
		double LandFewSum = 0.0; // (and the shares they kept, summed)
		double SeaFlashPx = 0.0, SeaFlashKeep = 0.0, SeaGlowPx = 0.0, SeaGlowAlpha = 0.0, SeaGlowS = 0.0, SeaColumnPx = 0.0, SeaColumnKeep = 0.0;
		double SeaFlashHot = 0.0, SeaFlashOver = 1.0, SeaGonePx = 0.0, SeaGoneBeyond = 0.0, SeaHitFullPx = 0.0, SeaHitLeast = 1.0, SeaHitOver = 2000.0;
		Rgb SeaFlashCol;
		void SeaFlash(double Now, size_t FromBit); // (the flash a call has just laid, Bits[FromBit] on, held far off, and its glow)
		Vec WhereOwn(const Bit& B, double Age) const; // (where its own flight has it, before what a moving ship gave it)
		void Rides(const Source& S, size_t FromBit); // (the pieces laid since FromBit are a moving fire's, vent's or cook-off's: born at its seat as it moves, with the ship's way on them)
		bool MoveSource(Source& S, double Now, const Vec& At, const Vec& Velocity); // (that source is there now, going so: its lying pieces go with it. False: no place)
		Vec Going(const Bit& B, double Age) const;
		Vec GoingOwn(const Bit& B, double Age) const;
		void Lay(int Slot, const Vec& Middle, const Vec& AxisA, const Vec& AxisB, const Rgb& Col, double Alpha, double HeartAlpha, double RimAlpha, double Inner, double Point, double Tail, double Shade, const View& Eye, double Rough = 0.0, uint32_t LumpSeed = 0, double Turn = 0.0, const Rgb& Floor = Rgb(), const Rgb& Through = Rgb(), const Vec& Outward = Vec(), const Rgb& Lift = Rgb(), const Vec& TaperB = Vec());
		int LayBit(int Slot, const Bit& B, double Now, const View& Eye, int Room); // (how many slots it took: 0, 1, or 2 for a twin)
	};
}
