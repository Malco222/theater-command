#pragma once

#include "Battle/LandCrater.h"
#include "Battle/LandFeed.h"
#include "Battle/LandField.h"
#include "Battle/LandKit.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * Old wrecks (release 2; Brian, 23 November 20:09Z: the burnt-out wrecks of a battle stay on the field for the next
 * battle there): the armour burnt out in the land battles fought over a region before, laid on its field as the battle
 * opens, rusting where they fell. LOOKS ONLY, as the scars and the old ruts: nothing the rules fight reads them, no
 * man takes cover behind one, and they are kept off every way the recording's men and machines go and every place its
 * rounds land, so no round strikes one and nothing it shows is changed by them. A live battle's rounds land where its
 * rules send them, on a wreck too (the burst is drawn there; no hole is dug under her: Yard::Over).
 *
 * The campaign sends the recording's root "oldWrecks": {"count": n, "months": m, "nats": {"GER": n1, ...}}: the
 * armoured vehicles burnt out in battles fought there before (60 at most), the months since the last of them, and
 * whose they were. LandFeed reads it (any part of it may be missing); the scene's -TheaterLandOldWrecks=N and
 * -TheaterLandOldWreckMonths=M stand in for it on the Mac, the nations then the battle's two sides'.
 *
 * Two halves, both plain C++ (the cloud test, Tools/land_hulk, runs them on every recording):
 * - Where they lie (Yard::Lay): how many the scrap men left (all within three months, about half by a year, a few by
 *   two), each nation's machines as that year had them (live.ts's standInModel's mix, with the models at hand), in
 *   small groups where a fight went on: along the field's old lines (hedges, walls and lanes, as the scars lie), at
 *   the lanes' junctions and the village's edge, and along the battle's own axis. Each is pushed off the lanes (the
 *   engineers cleared those), never in a hedge, a house, a yard, a wood, water, marsh or standing corn, never on an old
 *   shell hole, a trench, an old rut or a cattle's path, clear of every recorded vehicle's way by 8 m and every man's by
 *   1.5 m, and clear of the others. A live battle opens with no ways yet: FieldOf reads its header instead (Field::Likely:
 *   where the rules draw its blocks up and the player's setup room, the ground between its fronts, its joiners' edges;
 *   and round that, the rest of its start areas and the ground its blocks fall back over), and they are laid off it,
 *   but for the groups along the old lines and at the junctions and the village's edge, which may lie on the outer
 *   part; Yard::Reached then reads each live piece as it comes and marks a wreck a live man or machine is to come near
 *   (or is heading for), and Yard::Clear clears her away the moment the eye cannot see her go, or half a second before
 *   he comes near her where it is on her all along (Yard::Cleared), as the scene's GuardOldWrecks and ShowOldWrecks do.
 * - How each looks (Yard::Dress, Yard::Patch): the game's own model of her (the miniature the battle draws, posed by
 *   LandKit), knocked out and burnt through: her turret turned, or blown off and lying near, upturned or on its side;
 *   her gun drooped to its stop; her hatches open; a track run off beside her, snaking a little and folded back on
 *   itself once; a lorry's canvas burnt off its hoops, her bed's planks charred and some burnt through, her glass gone
 *   and her tyres burnt off to the rims; her aerial and roof gun gone. A burn laid by where the fire was (the art
 *   director's last round, 24 November: round 2's patchwork read as camouflage), each coat blending into the next:
 *   a week on soot black over about half of what faces the sky, densest round where the flames came out and
 *   streaking downwind, feathered grey and grained along the wind; round it by her heat bare grey steel in the
 *   hottest centres, her paint scorched to a chalky grey-buff round that, a brown fringe, her own paint low on her
 *   cooler sides, a fresh orange only along the edges; a thrown turret burnt from its own hatches only; a month on
 *   the bare steel flash-rusted, a dull orange-brown mottled orange and ochre, no brighter than some 150, 85, 45 in
 *   the sun on her tops or her sides, soft-edged, staining the paint round it, in runs and a softer wash down her
 *   sides; by half a year the soot washed off what faces the sky (a dark stain of it fading), the rust old and brown,
 *   mottled deep and lighter, the scorched paint flaking and weathered to faded paint on the rust, her own paint
 *   flaking, a little of her paint as the fire left it kept where the weather reaches least (low on her sides, under
 *   what stands out over them, on what turns down; none where she burnt bare), a few darker runs weeping from her
 *   seams and hatches on her upright faces, in some stretches of her only (the art director's carries of 24 November
 *   21:01Z); settled into the ground; a ragged scorched patch under her, ash black at her engine's end with a brown
 *   rim of burnt grass and her fuel's stain, that greens again, lusher than the field round it. No fire and no smoke:
 *   they burnt out long ago.
 *
 * Frames. The field is x east, y north, z up, metres (the recording's). Headings are radians, 0 north, clockwise, as
 * LandKit's. A dressed mesh is a LandField::Mesh in the field's axes, metres from its hulk's anchor (Hulk::X, Y, Z),
 * wound as the field's own meshes are (counter-clockwise seen from the front in the field's axes): the scene hands it
 * to the ground's static mesh maker (LandParts::GroundMesh) and stands the mesh at World(X, Y, Z).
 */
namespace LandHulk
{
	/** The campaign's cap on its count, and the most laid on a field at once (each a static mesh of some 90,000 points
	 * at its near level: twenty are about as many points as the dry run's own armour; the Mac's 45 fps gate judges it). */
	constexpr int MostCount = 60;
	constexpr int Most = 20;
	/** The near level is drawn within this many metres of the eye, the far beyond (as a vehicle's, LandBattle's 140). */
	constexpr double NearTo = 140.0;
	/** How far each is kept: from every recorded vehicle's way (her middle, wherever she drives the whole battle) to
	 * the wreck's outline; from every man's; from the middle line of a lane and of a hedge, wall or wood's edge; from
	 * where a shell, bomb or grenade of this battle lands (its hole would open under her); from an old rut; from another. */
	constexpr double VehicleClear = 8.0, ManClear = 1.5, LaneClear = 6.0, HedgeClear = 3.0, BlastClear = 4.0, WayClear = 2.5, Apart = 2.0;
	/** And from every piece of a cattle's path's trodden earth (a path is not laid across a wreck), and from every field
	 * telephone's wire (a lineman lays his wire round a wreck, not under her). */
	constexpr double PathClear = 1.0, WireClear = 1.0;

	/**
	 * A live battle (release 2: every battle the campaign plays, fought by the rules as it is shown) opens with nothing
	 * fought, so there are no ways to keep the wrecks off: they are laid instead off the ground its men and machines are
	 * likely to cross (Field::Likely, filled by FieldOf from the header), in two parts. Its core (FromCore): each block
	 * where the rules draw it up (units.ts deploy and drawUp: rows, GAP between blocks, the tanks on the wings, a
	 * defence's reserve, the guns at the back; a field with a river or a city's streets, whose deploy goes otherwise:
	 * each whole zone) and the setup step's room, grown by StartClear (the room by its widest block's half more); the
	 * advance between them (the ground between the two sides' fronts as drawn up, their wings and all, AdvanceMargin
	 * round it); with a corps coming up from next door, JoinClear in from every edge of the field (it walks on by one,
	 * any but the enemy's back edge); and EdgeClear in from the field's own edges. And round the core (FromLikely): the
	 * rest of each side's zone and the ground behind its blocks and its advance to its own back edge (a block falls back,
	 * and runs, straight back: sim.ts withdraw and rout), each grown by StartClear. Every wreck stays off both but the
	 * groups along the old lines and at the junctions and the village's edge, which stay off the core only, so the
	 * earlier battle's lines and village still read; the lanes LiveLaneClear from a lane's middle (the columns drive
	 * them). The rules' drill has no way round a still machine (it drives this battle's own wrecks through), so nothing
	 * keeps a live man or machine off one: the scene's guard (Yard::Reached) marks a wreck before a vehicle's middle
	 * comes within VehicleReach of her outline or a man within ManReach, the clearances a recorded battle's wrecks are
	 * laid at (Lay's, with its half metre and 40 cm over her rings; a smoothly rolling tank's BlendLead more, and a
	 * knocked-out tank's crews' BailReach where they bail out), and Yard::Clear clears her away before he gets there (the
	 * live pieces come at least a second of battle ahead of what is drawn), so every wreck left standing is as clear of
	 * the live battle's ways as a recorded battle's are of its own.
	 */
	constexpr double StartClear = 60.0, AdvanceMargin = 100.0, LiveLaneClear = 15.0, JoinClear = 300.0, EdgeClear = 60.0;
	constexpr double VehicleReach = VehicleClear + 0.5, ManReach = ManClear + 0.4;
	/** A live battle's groups lie this far apart at least (the ground it leaves is strips along its flanks: without it the
	 * groups crowded a strip's inner edge, Prokhorovka's twenty in a patch 100 by 260 m; a recorded battle's lie some 140
	 * to 190 m apart on the average). */
	constexpr double LiveGroupApart = 100.0;
	/**
	 * When a marked wreck goes (Yard::Clear). The moment the eye cannot see her go: out of its picture, too small in it
	 * (her reach under SightPx of its lines), or on a cut (the picture starts afresh). Where it is on her all along, at
	 * the latest ClearLead before the live man or machine comes near her (his middle at her reach: a tank's gun reaches
	 * some 5.5 m of the 8.5 from his middle, a man's body 0.3 m of the 1.9, so she is gone before any of him reaches her).
	 * And a wreck a moving man or machine is heading for (his last way straight on, SoonAhead of battle or SoonFar at
	 * the most, his reach and SoonWiden more round it) goes the moment the eye cannot see her go, before he is near
	 * enough to mark her (none goes in sight for that alone), so she seldom goes in the picture.
	 */
	constexpr double ClearLead = 0.5, SightPx = 2.0, SoonAhead = 8.0, SoonFar = 120.0, SoonWiden = 2.0;
	/**
	 * What else the scene draws that walks: a knocked-out tank's crew bailing out (-TheaterLandBailOut, LandBail: off her
	 * deck, 8 to 12 m back in her lee, then 18 to 38 m to her rear, cover or a crater, her rear 4 m back from her middle),
	 * read as reaching BailReach from where she is drawn from her kill on, where the scene asks it (Reached's Crews): her
	 * place at her fall, as LandKit's PlaceAt holds her (a hair before it, on her kept points joined straight; a live
	 * battle's machine is never pinned, the live drill stopping her where she is hit, "she burns where she stood"), and
	 * her pin where the reading pinned her (a recorded battle's jump to her formation place); a man who leaves the field
	 * with his block, drawn on at his last speed and facing for the second he fades (LandMen), read as walking LeaveDrift
	 * on.
	 */
	constexpr double BailReach = 60.0, LeaveDrift = 7.0;
	/**
	 * Tanks rolling smoothly (-TheaterLandSmoothTanks, release 2; LandKit::Vehicle::SmoothTanks at the merge with the
	 * joined build): LandKit draws a tank's place each step as the recording's places blended by a bell over Sigma battle
	 * seconds either way (0.4 asked plain, 0.45 at the most; cut at 2.5 Sigma), so she is drawn off her kept points'
	 * straight lines at a corner, or ahead or behind as her speed changes, by at most her fastest over the bell's reach
	 * times the bell's mean lead, under BlendLead Sigma (0.73 to 0.78 Sigma at its 0.05 s steps). Reached's Blend widens a
	 * tank's reach by that much along each piece of her way (her fastest over the kept points within 2.5 Sigma and a step
	 * of it, a step faster than 25 m/s not counted: the screen puts her there, unblended).
	 */
	constexpr double BlendLead = 0.8;

	/**
	 * The share of a field's burnt-out armour still lying there Months after the battle (the recovery companies took
	 * what could be mended within weeks, the scrap men the rest over the next year or two): all of it within three
	 * months, half at a year, 15% at two years, 6% from four on, in straight lines between.
	 */
	double Left(double Months);
	/**
	 * How many wrecks are drawn for Count burnt out (capped at MostCount) at Per real machines to a drawn one: the
	 * battle's own scale (the recording's oldWrecks "per"), so a wreck stands for as many as a tank in the battle does
	 * (Brian's battle-scale rule). Count / Per rounded, at least one where any burnt; Per not a number or under 1 is 1,
	 * over 1000 is 1000.
	 */
	int Models(int Count, double Per);
	/** How many of those (Models of Count at Per) are left after Months, rounded, at most Most. */
	int Kept(int Count, double Months, double Per = 1.0);

	/**
	 * A nation's burnt-out machine in Year as the game draws that nation's armour (src/battle/land/live.ts's
	 * standInModel: its tank mix by year, with the Panther from 1944 as shells.ts's DRAWN has it), the Nth of them; or,
	 * bSoft, its half-track where the model is at hand (the German Sd.Kfz. 251, the American M3), else the lorry.
	 */
	std::string ModelFor(const std::string& Nation, int Year, int N, bool bSoft);

	/** A circle of a wreck's outline on the field (its hull in a few, a thrown turret, a run-off track). */
	struct Ring
	{
		double X = 0.0, Y = 0.0, R = 0.0;
	};

	/** How a thrown turret lies. */
	enum class Lie : uint8_t { Tilted, OnSide, Upturned };

	/** Where the fight that left a group lay. */
	enum class Where : uint8_t { OldLine, Spot, Axis, Strewn };

	/** One wreck as laid. */
	struct Hulk
	{
		/** Her anchor: the hull's root (the middle of her footprint) on the field, sunk as she has settled. */
		double X = 0.0, Y = 0.0, Z = 0.0;
		double Heading = 0.0;
		/** Her hull's turn on the ground (radians: nose up, right side up), as she rests on it. */
		double Pitch = 0.0, Roll = 0.0;
		std::string Nation, Model;
		bool bSoft = false;
		/** Months since she burnt (the last battle's, or an earlier one's: a few months more). */
		double Months = 0.0;
		/** Her group (the fight she burnt in) and where it lay. */
		int Group = 0;
		Where At = Where::Strewn;
		/** Her turret's turn from ahead and her gun's lay, degrees (the model's own: + the turret clockwise seen from
		 * above, the gun up). */
		double Turret = 0.0, Gun = 0.0;
		/** Her turret blown off: where it lies (its ring's middle on the field), turned how, settled how deep. */
		bool bTurretOff = false;
		double TurretX = 0.0, TurretY = 0.0, TurretYaw = 0.0, TurretTilt = 0.0, TurretTiltWay = 0.0, TurretZ = 0.0;
		Lie TurretLie = Lie::Tilted;
		/** A track run off (-1 her left, +1 her right, 0 none), and where its links lie, one point a link. It snakes a
		 * little as it lies and folds back on itself once (the art director's carry, 24 November: a thrown track lay dead
		 * straight like a ladder): its first Fold links lie flat on the ground from her bow, the next Curl turn over in
		 * the fold, and the rest lie doubled back on top of the first, upside down (Strip: each one's place on the field,
		 * the doubled ones over those they lie on). Laid along the strip the placement read (Yard::Lay), beside her moved out
		 * off her road wheels. */
		int TrackOff = 0;
		std::vector<std::pair<double, double>> Strip;
		int Fold = 0, Curl = 0;
		/** Her hatches and doors open (a bit each, in the model's order of Hatches, then doors and tailboard). */
		uint32_t Open = 0;
		/** How fiercely she burnt (0.8 to 1: the share of each point's own soot weight), and how deep she has settled. */
		double Fire = 1.0, Sink = 0.0;
		/** Her own dice for her look. */
		uint64_t Seed = 0;
		/** Her outline: the hull, her gun where it reaches past it, a thrown turret, a run-off track. */
		std::vector<Ring> Rings;
		/** The scorched patch's half length along her and half width across (metres). */
		double PatchLong = 0.0, PatchWide = 0.0;
		/** Her hull's footprint, metres along her and to her right from her anchor (her model's HullLo to HullHi): Covers
		 * keeps the grass out of her corners, which her rings leave. */
		double FootLoX = 0.0, FootHiX = 0.0, FootLoY = 0.0, FootHiY = 0.0;
		/** Her thrown turret's reach, for the camera (the scene's LandSight, as a still machine of its own): a box in its
		 * own axes about (TurretX, TurretY, TurretZ), x along TurretYaw, y to its right, z up, as it lies (on its side it
		 * stands as tall as it is wide); and its gun's muzzle on the field where it has one (bTurretGun). */
		LandKit::V3 TurretBoxLo, TurretBoxHi, TurretMuzzle;
		bool bTurretGun = false;
	};

	/** What the field asks of where they lie (the scene's, or the cloud test's laid the same way). */
	struct Field
	{
		/** The fought-over ground (as LayScars reads it: every 4th point of every track). */
		std::vector<std::pair<double, double>> Fought;
		/** The battle's axis: the way its machines went (radians from east, a line: either way; LayOldRuts's Heading;
		 * not a number: none). */
		double Axis = 0.0;
		bool bAxis = false;
		/** The old lines a fight lay along (LandCrater::OldLinesOf: hedges, walls, lanes; the lanes' junctions, the
		 * village's edge). */
		LandCrater::OldLines Lines;
		/** The lanes' middle lines (kept LaneClear off) and the hedges', walls' and woods' edges' (HedgeClear). */
		std::vector<std::vector<std::pair<double, double>>> Lanes, Hedges;
		/** The recording's ways: each vehicle's kept points, each man's (the battle's own and anything that walks it). */
		std::vector<std::vector<std::pair<double, double>>> Vehicles, Men;
		/** Where this battle's shells, bombs and grenades land (each tears a hole). */
		std::vector<std::pair<double, double>> Blasts;
		/** What stands or lies in the open: trees, shrubs, carts, wells, a region's kit, the field's kept craters and
		 * the old shell holes laid before (each its middle and reach). */
		std::vector<Ring> Standing;
		/** Mud memory's old ways (their ruts are drawn: a scorched patch is not laid over one). */
		std::vector<std::vector<std::pair<double, double>>> Ways;
		/** The field telephones' wires (release 2: the recording's "wires", or the scene's -TheaterLandPhones made from
		 * the battle), each laid on the ground along its points: kept WireClear off her outline. */
		std::vector<std::vector<std::pair<double, double>>> Wires;
		/** The snow lying on the field (LandField::Ground::SnowLying, 0 to 1): her patch and the earth on her low parts
		 * under it once it has fallen since she burnt, and on what faces up on her. */
		double Snow = 0.0;
		/** The two sides' deployment zones' middles (bSides): a side's wrecks lie nearer its own, facing the other. */
		bool bSides = false;
		double AttX = 0.0, AttY = 0.0, DefX = 0.0, DefY = 0.0;
		std::string AttNat, DefNat;
		/**
		 * A live battle's (bOn: FieldOf of a live header; off for a recording read whole, which lays as before): the
		 * ground its men and machines are likely to cross, kept clear of each wreck's outline (FromLikely; the groups
		 * along the old lines and at the junctions and the village's edge, of its core only: FromCore). Fought is then
		 * the ground between the two zones' back edges off the core, Vehicles and Men empty, and the axis the line
		 * between the zones' middles.
		 */
		struct Likely
		{
			bool bOn = false;
			/** The core's boxes, [x0, y0, x1, y1] each: each block where the rules draw it up grown StartClear (each whole
			 * zone where its deploy is not modelled: !bDrawnUp), the setup step's room grown the same and its widest
			 * block's half more, and the edges' strips where a corps from next door walks on. */
			std::vector<std::vector<double>> Core;
			/** Round the core: each side's zone and the ground behind its blocks and its advance to its own back edge, grown
			 * StartClear: [x0, y0, x1, y1] each. */
			std::vector<std::vector<double>> Areas;
			/** Each block where the rules draw it up (its box as drawUp stands it, ungrown: [x0, y0, x1, y1, side]), and
			 * whether each side's are modelled (false: a river or a city's streets, its whole zone in the core). */
			std::vector<std::vector<double>> Stands;
			bool bDrawnUp[2] = { false, false };
			/** The advance: the four corners of the ground between the two fronts as the rules draw them up (each side's
			 * front line's depth, as wide as its blocks reach, wings and all), AdvanceMargin round it. */
			std::vector<std::pair<double, double>> Advance;
			/** The field less EdgeClear: [x0, y0, x1, y1]. */
			double Inside[4] = { 0.0, 0.0, 0.0, 0.0 };
			/** Each side's front as drawn up (from its blocks' westmost to their eastmost, wings and all; a corps from next
			 * door left out), and whether one comes up from next door. */
			double FrontAtt = 0.0, FrontDef = 0.0;
			bool bJoins = false;
		};
		Likely Live;
		/** The cattle's paths' trodden earth in a box (LandField's near clutter, its Trodden pieces; none: no paths). */
		std::function<void(double X0, double Y0, double X1, double Y1, std::vector<std::pair<double, double>>& Out)> Trodden;
		/** Where a wreck and her scorched patch may lie (a field, a pasture, rough ground: not a lane, a bank, a house, a
		 * wall, a yard, a garden, a wood, water, marsh, a trench, standing corn or an old shell hole). */
		std::function<bool(double X, double Y)> Open;
		/** The ground (height and its normal), as the battle's vehicles stand on it. */
		LandKit::GroundFn Ground;
		/** For the scorched patch: the ground as drawn (its flat triangles' height), its normal as lit, its own colour. */
		std::function<double(double X, double Y)> DrawnZ;
		std::function<void(double X, double Y, double& OutX, double& OutY, double& OutZ)> DrawnUp;
		std::function<LandField::Rgb8(double X, double Y)> Colour;
	};

	/**
	 * The field as the scene reads it for the wrecks (LandBattle's LayOldWrecks, and the cloud test the same way): the
	 * recording's tracks (every 4th point the fought-over ground, as LayScars reads it; each vehicle's and each man's
	 * points their ways), the way its machines went (LayOldRuts's heading), the shells, bombs and grenades of this
	 * battle (each round of a mortar, a gun, a tank or an anti-tank gun and each grenade where it lands, each 'shell'
	 * event, each crater the field keeps), the field's old lines, lanes, banks, walls and woods' edges, what stands in
	 * the open (every tree, shrub, cart, well and haycock, and a region's kit), the field telephones' wires, the two
	 * sides' zones and nations, the snow lying, and the ground. Open ground only: a pasture, rough ground, the country
	 * beyond, a field of stubble or roots (not standing corn), never on an old shell hole or in a dug one's cut (Holes:
	 * the field's holes as laid so far, the scars'; null: none); OldRuts: mud memory's ways (empty: none). G and Holes
	 * must outlive the field's functions.
	 */
	Field FieldOf(const LandFeed::Recording& Rec, const LandField::Ground& G, const LandCrater::Field* Holes,
		const std::vector<std::vector<std::pair<double, double>>>& OldRuts);

	/**
	 * How far a place lies from the ground a live battle's men and machines are likely to cross (F.Live: its core, and
	 * round it the rest of its start areas and the ground behind its blocks; the lanes are kept by their own squares):
	 * under 0 inside it, 1e9 for a recording read whole (F.Live off).
	 */
	double FromLikely(const Field& F, double X, double Y);
	/** The same from its core alone (where its blocks are drawn up and the setup room, the advance and AdvanceMargin
	 * round it, the edges a corps from next door walks on by, the field's edges). */
	double FromCore(const Field& F, double X, double Y);

	/** The eye as the scene draws this frame (field metres: the camera as posed, LandEye::Pose's shot with the player's
	 * look aside and the hand's jolt, SightAlong), for the guard to choose when a wreck goes (Yard::Clear): out of its
	 * picture, too small in it to see, or on a cut. */
	struct Sight
	{
		double EyeX = 0.0, EyeY = 0.0, EyeZ = 0.0, LookX = 0.0, LookY = 1.0, LookZ = 0.0;
		/** Its lens (degrees from the picture's top to its bottom), the picture's width over its height, and its height
		 * in pixels. */
		double Fov = 55.0, Aspect = 16.0 / 9.0, Lines = 1080.0;
		/** A cut this frame: the picture starts afresh, so nothing is seen to go. */
		bool bCut = false;
	};
	/**
	 * The eye of a camera posed at Eye looking along Ahead (field metres; any length), its lens WideFov degrees across
	 * the picture (Unreal's, the horizontal one) on a picture Aspect wide over tall, Lines high: the scene's own camera as
	 * it is drawn this frame (AimEye's pose turned by the player's drag aside, up to 50 degrees round and 30 up or down,
	 * and the hand-held jolt), not the shot's pose alone. Its roll, under a degree, is left to InSight's tenth wider.
	 */
	Sight SightAlong(double EyeX, double EyeY, double EyeZ, double AheadX, double AheadY, double AheadZ, double WideFov, double Aspect, double Lines, bool bCut);

	/**
	 * What a point of a dressed wreck shows (the coat on its side of each band's middle where her coats blend): soot,
	 * scorched paint gone chalky, bare steel, flash rust, old rust, her own paint left, charred rubber, wood or glass, a
	 * track's links or a rim black-grey through the fire (not yet rusted), the snow fallen on her since, the open hole of
	 * her turret ring (her turret thrown: the dark of her inside, not soot), or a run of rust down her side from a rusted
	 * patch above (flash or old as the patch is), or, from half a year, a darker one weeping from her seams and hatches.
	 * The cloud test measures each in its pictures.
	 */
	enum class Coat : uint8_t { Soot, Scorched, Bare, Flash, OldRust, Paint, Charred, Links, Snow, Hole, Run };
	constexpr int Coats = 11;
	const char* CoatName(Coat C);
	/** What a point of her scorched patch shows: the field's own, ash black at her engine's end, burnt grass, its brown
	 * rim, her fuel's stain, the brown of the months after, the weeds coming up lusher. */
	enum class Scorch : uint8_t { Field, Ash, Burnt, Rim, Fuel, Brown, Lush };
	constexpr int Scorches = 7;
	const char* ScorchName(Scorch S);

	/** What Dress and Patch may be asked and tell (all of it the cloud test's: the scene asks nothing). */
	struct Dressing
	{
		/** Her age to dress her at, months (the look sheet's columns); under 0 her own. */
		double Months = -1.0;
		/** Each point's coat (Dress: a Coat; Patch: a Scorch), one a point of the mesh made. */
		std::vector<uint8_t> Coats;
		/** Points burnt away (canvas, leather, rope, cloth), of glass gone or blackened, and left standing just under where
		 * the burnt cloth's top was (a lorry's tilt: its hoops). */
		int Burnt = 0, Glass = 0, Hoops = 0;
		/** A lorry's wooden bed: its planks, and how many burnt through and gone; the old floor's top and the planks' (model
		 * metres up, at rest), the floor's footprint (square metres) and how much of it, and of the rest of her wood facing
		 * up (her benches, her sides' tops), was taken off and kept. */
		int Planks = 0, PlanksGone = 0;
		double BedTop = 0.0, PlankTop = 0.0, BedArea = 0.0, BedDropped = 0.0, WoodKept = 0.0;
		/** Points dressed as wood (charred), and as her turret ring's open hole (her turret thrown), and its radius. */
		int Wood = 0, Hole = 0;
		double RingR = 0.0;
		/** Tyres burnt off to their rims. */
		int Rims = 0;
	};

	/** What the campaign (or the Mac's stand-in) asks for. */
	struct Ask
	{
		int Count = 0;
		double Months = 0.0;
		/** Whose they were, as the recording's nats give them, in its order (empty: the battle's two sides, the
		 * attacker's the odd one). */
		std::vector<std::pair<std::string, int>> Nats;
		/** The real machines each drawn wreck stands for (the recording's "per", the battle's scale; 1: one to one).
		 * Count is burnt-out machines, so Models(Count, Per) are drawn before the scrap men's share. Not read by the
		 * stand-in (bAsLaid). */
		double Per = 1.0;
		/** This battle's month: the wrecks' own is Months before it, and their machines and coats that month's (the
		 * battle's own models, Have, taken only where that is this month: else each read and dressed for hers). */
		int Year = 1944, Month = 7;
		/** The field's ground word for the coats (LandKit::Dress::Terrain). */
		std::string Terrain = "plains";
		uint64_t Seed = 1;
		/** Count is how many to lay (the Mac's stand-in, -TheaterLandOldWrecks=N: N laid, at most Most), not how many
		 * burnt out: no salvage taken off, and Months ages their look only. */
		bool bAsLaid = false;
	};

	/** The laid wrecks, their models and how many of each came to lie where (for the scene's log line). */
	class Yard
	{
	public:
		using ReadFn = LandKit::Battle::ReadFn;
		/** A model the battle has already read ("Name|Nation"), or null. */
		using HaveFn = std::function<const LandKit::Model*(const std::string& Name, const std::string& Nation)>;

		/**
		 * Lays them (and forgets any laid before): A.Count's survivors of A.Months' salvage (A.bAsLaid: A.Count), each
		 * nation's share by its nats, each machine read by Read (or taken from Have where that is this month's), placed
		 * on F. Seeded: the same field, ask and seed lay the same wrecks. Returns how many. F's ground functions are kept
		 * for Dress and Patch, and must outlive the yard.
		 */
		int Lay(const Field& F, const Ask& A, const ReadFn& Read, const HaveFn& Have = nullptr);

		const std::vector<Hulk>& Hulks() const { return List; }
		/** The model she is drawn from (never null for a laid one). */
		const LandKit::Model* ModelOf(size_t K) const;

		/** Hulk K posed, broken and burnt (bNear: the model's near level, else its far), into Out (emptied first); Note
		 * (the cloud test's): her age to dress her at, and each point's coat and what burnt away. */
		void Dress(size_t K, bool bNear, LandField::Mesh& Out, Dressing* Note = nullptr) const;
		/** The ground under and round hulk K: burnt black and ragged while fresh (ash at her engine's end, a brown rim of
		 * burnt grass, her fuel's stain), browning, then greener than the field. Note as Dress's. */
		void Patch(size_t K, LandField::Mesh& Out, Dressing* Note = nullptr) const;
		/** Whether a wreck covers a place, so the grass and corn round the eye do not stand there (under a wreck, her
		 * hull's corners and all, or a thrown turret, on a run-off track, and on the scorched patch while it is fresh; a
		 * wreck cleared away, Clear, none). */
		bool Covers(double X, double Y) const;

		/**
		 * A live battle's guard (release 2), each piece: marks the wrecks a live vehicle's middle comes within
		 * VehicleReach of, or a man within ManReach of (her outline: her rings), along the recording's tracks from Seen on
		 * (each track's kept points from Seen[T], the straight line between them as they are drawn; a knocked-out
		 * machine where she is drawn from her fall on, her place at her fall and her pin where she has one, and a tank's
		 * crew's way out to Crews from them where the scene's crews bail out, 0: they do not; a man who left with his
		 * block LeaveDrift on; a tank drawn rolling smoothly over a bell of Blend battle seconds, 0: on her kept points'
		 * lines, her reach BlendLead Blend her fastest more), each wreck once, with the battle time he first comes near
		 * her (Due); returns those newly marked. And afresh each piece, the wrecks a moving man or machine is heading for
		 * (Heading: his last way straight on SoonAhead, SoonFar at most, his reach and SoonWiden round it). Seen is grown
		 * to the tracks and moved on (a track's provisional last point, Track::bTail, read again with the next piece; a
		 * smoothly rolling tank's way back over the bell's reach of her new points too); the caller empties it when the
		 * men are placed again (LandFeed::Grown::bReset). Nothing for a yard with none laid.
		 */
		std::vector<size_t> Reached(const LandFeed::Recording& Rec, std::vector<size_t>& Seen, double Crews = 0.0, double Blend = 0.0);
		/**
		 * Each frame: the marked wrecks cleared away now, at battle time Now (the screen's) with the eye as it is: a
		 * marked one or one a man or machine is heading for the moment Eye cannot see her go (out of its picture, too
		 * small in it, or on a cut: InSight), a marked one Eye is on at the latest ClearLead before her Due. How (where
		 * given): for each, 0 out of the eye's sight, 1 on a cut, 2 in sight at her time.
		 */
		std::vector<size_t> Clear(double Now, const Sight& Eye, std::vector<int>* How = nullptr);
		/** Whether wreck K (her reach: her rings and her patch) is in the eye's picture and big enough in it to be seen
		 * go (her reach SightPx of its lines or more); hills and houses in the way not read. */
		bool InSight(size_t K, const Sight& Eye) const;
		/** Wreck K cleared away (Clear). */
		bool Cleared(size_t K) const { return K < Swept.size() && Swept[K] != 0; }
		/** Wreck K marked (Reached) and the battle time a live man or machine comes near her (1e18: not marked). */
		bool Marked(size_t K) const { return DueOf(K) < 1e17; }
		double DueOf(size_t K) const { return K < DueAt.size() ? DueAt[K] : 1e18; }
		/** A moving man or machine heading for wreck K as the last piece left them (Reached), and when he would come near. */
		bool Heading(size_t K) const { return SoonOf(K) < 1e17; }
		double SoonOf(size_t K) const { return K < SoonAt.size() ? SoonAt[K] : 1e18; }
		/** Whether a place lies under a wreck not cleared away, within Margin of her outline, her footprint or her scorched
		 * patch at its ragged edge's widest (a live battle's shell hole is not dug there: it would open under her). */
		bool Over(double X, double Y, double Margin) const;

		/** What was asked and what came of it, for the scene's log line ("12 laid of 18 left of 30 ..."). */
		std::string Said() const;
		/** How many the salvage left, how many found no room, the groups by where they lay. */
		int Wanted = 0, NoRoom = 0;
		int ByWhere[4] = { 0, 0, 0, 0 };
		/** Models read for the wrecks (none where the battle had them already), and one line for each drawn as another. */
		int Read = 0;
		std::vector<std::string> StandIns;
		/** Laid for a live battle (Field::Likely on), for Said; and how many of its groups lie on the likely ground round
		 * its core (along the old lines, at a junction or the village's edge: FromLikely under their reach). */
		bool bLive = false;
		int OnLikely = 0;

	private:
		std::vector<Hulk> List;
		/** Each wreck cleared away (Clear); when a live man or machine comes near her (Reached; 1e18 none), and when one
		 * heading for her would (afresh each piece). */
		std::vector<uint8_t> Swept;
		std::vector<double> DueAt, SoonAt;
		std::map<std::string, std::shared_ptr<LandKit::Model>> Loaded;
		std::map<std::string, const LandKit::Model*> Models;
		std::vector<std::string> KeyOf;
		Ask Asked;
		LandKit::GroundFn Ground;
		std::function<double(double, double)> DrawnZ;
		std::function<void(double, double, double&, double&, double&)> DrawnUp;
		std::function<LandField::Rgb8(double, double)> Colour;
		double Snow = 0.0;
		/** Covers' squares (4 m): each wreck's rings, her hull's footprint, and her patch while it is fresh. */
		std::map<int64_t, std::vector<std::pair<size_t, int>>> Cover;
	};
}
