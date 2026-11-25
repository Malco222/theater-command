#pragma once

#include "Battle/ConvoyLooksCore.h"
#include "Battle/SeaSailors.h"
#include "Battle/SeaShow.h"

#include <array>
#include <string>
#include <vector>

/**
 * A deck gun's crew (Brian, 24 November 2026: the sailor files' gun_layer, gun_trainer, gun_loader and ammo_passer, and
 * hands_up): on a surfaced boat with her gun told, and on an armed merchant at her gun, its layer and trainer at its
 * handwheels, its loader behind its breech, and on a boat with a hatch by her gun a man handing the rounds up through it.
 * Each stands where his clip's own blocks put the gun's parts (the wheel's hub, the chamber's mouth) at the gun the stage
 * draws trained (AirStage's TrainGun), so his hands meet them; turned with the mount as it trains. The passer stands in
 * her hatch itself, and hands up only where the loader at his post is where their pair has him (else he waits below, and
 * the loader takes his rounds from a ready-use rack at his left, RackRound).
 *
 * A boat's crew come up her hatch nearest the gun when she has surfaced and her gun is first told, one at a time (the
 * next once the man before is out of it), and go below again at the diving alarm, before her casing is awash (never left
 * standing as she goes under), one at a time down it, those after waiting in a line short of it; none the moment she
 * strikes (her crew on her casing are the show's own from then, SeaShow::Hands, out of her bridge and hatches in that
 * same second: hers are not drawn twice). A merchant's are at her gun from when it is first told until she leaves her
 * line (her loader waiting with a round rammed: a gun kept loaded). None once a ship leaves her line otherwise (sinking:
 * her men are the show's). The loader's work keeps time with the show's rounds: he loads one, waits with his hands out
 * for the next, and takes it as the gun fires; a round fired before her crew are at the gun (a hit the battle tells is
 * fired at once) has them at their posts at once with the breech shut, and one fired as the loader holds his round short
 * of the breech shut has it home and shut as it fires (Unrammed). A shell close aboard, the layer and the trainer crouch
 * a moment, the loader and the passer work on. Nothing here changes what the
 * show decides (whether her gun is laid, when it fires, whether she is hidden): it is drawn from it only. The scene draws
 * none on a boat it hides (SeaShow's bShown).
 *
 * Plain C++, no Unreal types (Tools/convoy_looks checks it in the cloud): metres in her own frame (toward her bow and to
 * her starboard from the gun's mount's middle, and up from her waterline, as AirStage's GunDeck takes them), bearings in
 * degrees clockwise off her bow. A man's own axes are the files': x forward, y to his right, z up.
 */
namespace DeckCrew
{
	enum class Role : int
	{
		Layer,   // at the gun's left, the elevating handwheel and the sight
		Trainer, // at its right, the training handwheel
		Loader,  // behind its breech
		Passer   // in the hatch by it (a boat with one): hands the rounds up
	};
	constexpr int Roles = 4;
	/** The sailor files' clip for each man at his work. */
	SeaSailors::Act ActOf(Role R);

	/**
	 * A gun as its crew work it, measured off the battle modeller's files [MODEL] (Tools/convoy_looks checks each row
	 * against its file): a boat's open gun on its raised foot (its top over her waterline and its radius), the barrel's
	 * pivot (ahead of the mount's middle along the bore, and over her waterline), the chamber's mouth behind that pivot
	 * along the bore, the barrel's own lie as built (degrees: laid at an elevation it lies at that more) and its limits;
	 * a merchant's gun in its gunhouse, roofed and open at its back (how far behind the mount's middle), on its platform
	 * (how far from the mount's middle its crew may stand); the layer's and trainer's wheels' hubs (behind the mount's
	 * middle along the bore and out to either side of it, and where each wheel's shaft goes into the gun from its hub, in
	 * the man's own axes); a boat's casing's half width by her gun (at the mount's middle, and how much it narrows a
	 * metre forward along her; and its deck flats' at their narrowest each DeckEdgeStep along her: DeckHalfAt), and the
	 * hatch her crew come up by (metres along her
	 * from amidships: SculptedOf's ForeHatch or AftHatch, the nearer the gun), and whether it is near enough the gun
	 * (within ScuttleNear) that the rounds may come up through it: a man handing them up there (bScuttle), where the loader
	 * at his post can take them from him (PasserHolds).
	 */
	constexpr int DeckEdgeCount = 25;
	constexpr double DeckEdgeFrom = -3.0, DeckEdgeStep = 0.25, DeckEdgeSlope = 1.0;
	struct GunShape
	{
		std::string File, Part;
		bool bBoat = false;
		double Foot = 0.0, FootR = 0.0;
		double PivotAhead = 0.0, PivotUp = 0.0, BreechBack = 0.0, RingBack = 0.0, RingHalf = 0.0;
		double Built = 0.0, LiftLeast = -5.0, LiftMost = 35.0;
		double Back = 0.0, Platform = 0.0;
		double HubBack = 0.0, HubOut = 0.0;
		double ShaftAhead = 0.0, ShaftIn = 0.0;
		double DeckHalf = 0.0, DeckTaper = 0.0;
		std::array<double, DeckEdgeCount> DeckEdges{};
		bool bDeckEdges = false;
		double HatchAlong = 0.0;
		bool bScuttle = false;
		ConvoyLook::BoatModel Boat; // a boat's casing (SculptedOf)
	};
	constexpr double ScuttleNear = 1.5;
	/** A boat's casing's half width at a place by her gun (Fore: metres toward her bow from the mount's middle, within
	 * 3 m of it either way): her deck flats' at their narrowest in the DeckEdgeStep about it (DeckEdges, from DeckEdgeFrom),
	 * eased into a narrower slice's by its own widening DeckEdgeSlope a metre beyond it (never wider than a slice's own,
	 * and no step from one slice to the next: a man moving along her is moved in smoothly); else the line under them. */
	double DeckHalfAt(const GunShape& G, double Fore);
	/** The shape of a mount's gun (MountOf's), the boat's casing by it (SculptedOf's, for her hatch); empty File: none. */
	GunShape ShapeOf(const ConvoyLook::GunMount& M);

	/** What the files give for the gun's crew (one navy's file, SeaSailors): the four clips' lengths, the layer's and the
	 * trainer's wheels and sights, the loader's and the passer's rounds, the loader's root from the passer's (the pair),
	 * and the times of the loader's round taken, laid in the tray (the first key it lies along his clip's bore), rammed
	 * and the breech shut, and of the passer's round lifted, with their cues. bGun false: the file lacks a clip or a block
	 * (no gun crew drawn from it). */
	struct Blocks
	{
		bool bGun = false;
		float Long[Roles] = { 0.f, 0.f, 0.f, 0.f };
		SeaSailors::Wheel Wheels[2];
		SeaSailors::Round Loading, Passing;
		LandMen::Vec3 Pair;
		float Taken = 0.f, Laid = 0.f, Ram = 0.f, Shut = 0.f, Lift = 0.f;
		std::string TakenCue, RamCue, ShutCue, LiftCue;
		// (the clips of going to the gun and from it: climb_net up and down the hatch, run, idle while he waits at the
		// hatch, cower for a shell close aboard)
		float ClimbLong = 0.f, RunLong = 0.f, StandLong = 0.f, DuckLong = 0.f;
		/** The loader's skin ahead of him (HandsFrom metres and more) through his clip, every HandsEvery seconds, one point
		 * to a HandsCell cube (his own axes): his hands, arms and head as they reach for the breech. */
		std::vector<LandMen::Vec3> Hands;
	};
	constexpr float HandsEvery = 0.1f, HandsFrom = 0.4f, HandsCell = 0.02f;
	Blocks BlocksOf(const SeaSailors::Crew& C, int File);

	/** A place by the gun: toward her bow and to her starboard from the mount's middle, and over her waterline (metres). */
	struct Spot
	{
		double Fore = 0.0, Right = 0.0, Up = 0.0;
	};
	/** A man's post at the gun: his root (on the deck under him) and the way he faces (degrees off her bow); for the
	 * loader how far he stands nearer the breech than his clip has him (Slid, metres), how far he is turned about it off
	 * the bore's way (Turned, degrees: where her casing is too narrow behind it for him at a bearing), and how far his root
	 * is moved in from where those put him (Shift, metres: across her beam toward her middle line, a merchant's toward the
	 * mount's middle) where even so his boots would go past her casing's edge or her platform's rim. */
	struct Post
	{
		Spot Root;
		double Facing = 0.0, Slid = 0.0, Turned = 0.0, Shift = 0.0;
	};
	/** At most this much nearer the breech (metres: the recoiling barrel, 0.45 m, still clear of him), and then turned
	 * about it at most TurnMost (degrees, sought by TurnStep either way round it; beyond TurnNear less near it, by SlideTo
	 * not at all, his hand that shuts it into its side else), his root no nearer the layer's than Room nor the trainer's
	 * than RoomRight (metres: nearer, through their clips, his body goes into theirs; the trainer's clip keeps nearer his
	 * wheel). Of those, the least turned that has him on her deck with his hands clear of the breech's ring; else the
	 * least costly (over her edge by EdgeMost at the most, else moved in: ShiftIn; his hands in the ring HandsInto more).
	 * At the gun he steps round to a new post as it trains at TurnRate degrees and SlideRate metres a second [EST] (across
	 * her beam, from one way round the breech to the other), keeping the way round he is on unless the other costs
	 * SideMargin less (so he does not step to and fro). */
	constexpr double SlideMost = 0.15, TurnMost = 70.0, TurnNear = 30.0, SlideTo = 45.0, TurnStep = 2.5, Room = 0.72, RoomRight = 0.50;
	/** His skin ahead of him (Blocks' Hands) kept HandsPad clear of a boat's gun's breech's ring (GunShape's RingBack,
	 * RingHalf: metres, HandsCell's spacing); a post that has it in it costs HandsInto more than any that does not. */
	constexpr double HandsPad = 0.02, HandsInto = 1.0;
	constexpr double TurnRate = 45.0, SlideRate = 0.3, SideMargin = 0.05;
	/** At the gun the loader's root moves no quicker in a step than a place SwingReach metres out from the mount's middle
	 * swings with its train, and his own steps round the breech (TurnRate about the chamber's mouth at his clip's reach,
	 * SlideRate): where being moved in from her edge (Shift) would carry him quicker, he steps round slower. While his
	 * hands would be in the breech's ring at his post (stepping round behind it across her beam, where her casing has no
	 * room behind it), he stands clear (the stand clip, not at his work; his clip's time and cues run on). */
	constexpr double SwingReach = 2.0;

	/** The mount's train as the stage turns it (TrainGun): from where it lies stowed no further than its arc either way. */
	double MountTrain(const ConvoyLook::GunMount& M, double Train);
	/** The barrel's lie as the stage lifts it (degrees: as built and the elevation within its limits). */
	double BarrelLie(const GunShape& G, double Elevation);
	/** Where the loader puts the round in (a boat's chamber's mouth; a merchant's gunhouse's open back, at the height his
	 * clip has it), with the train MountTrain's; and the bore's way (fore, right, up). */
	Spot BreechOf(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, double Train, double Elevation, double Way[3]);
	/** The deck under a place by the gun: a boat's raised foot or her casing (CasingAt), a merchant's platform. */
	double DeckUnder(const GunShape& G, const ConvoyLook::GunMount& M, double Fore, double Right);
	/** Each man's post with the gun at a train (MountTrain's) and elevation, from his clip's blocks: the layer's and the
	 * trainer's wheel's hub where the gun's are, the loader's chamber's mouth at its breech (Prefer: the way round it he
	 * is turned now, +1 or -1, kept unless the other is better by SideMargin; 0 the better), the passer's at her hatch
	 * (PasserAt). */
	Post PostOf(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, Role R, double Train, double Elevation, int Prefer = 0);
	/** The loader's post turned about the breech Turned degrees and Slid metres nearer it, and moved in (Shift) by as much
	 * as his boots would still go past her platform's rim, or more than EdgeMost past her casing's edge (DeckHalfAt runs
	 * under the model's deck flats at their narrowest: a boot's corner that far past it, toward his root, is over a flat's
	 * edge by no more). */
	constexpr double EdgeMost = 0.12;
	Post LoaderAt(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, double Train, double Elevation, double Turned, double Slid);
	/** How far a man's boots' corners go past her casing's edge (DeckHalfAt, at its narrowest between his root and the
	 * corner) or her platform's rim at a post (metres, each toward his root: at or under 0, on it). */
	double OverEdge(const GunShape& G, const Post& P);
	/** The passer's post with the loader at his (Loading): his root in her hatch's middle on her deck (HatchOf), turned so
	 * the round in his hands as the loader takes it is where the loader's hands take it (their pair's moment, Taken); and
	 * how far apart those two are then (OutGap, metres). */
	Post PasserAt(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, const Post& Loading, double& OutGap);
	/** Whether the passer hands the round up from her hatch to the loader at his post: a boat whose hatch is by her gun
	 * (bScuttle), the round as the loader takes it within HandOffNear of his hands (else he waits below, the loader's
	 * rounds from a ready-use rack). PasserEver: at any train in the gun's arc and elevation up to 10 degrees, so he comes
	 * up her hatch at all. */
	constexpr double HandOffNear = 0.04;
	bool PasserHolds(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, const Post& Loading);
	bool PasserEver(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M);
	/** Her hatch her crew use (a boat's): its middle on her deck, toward her bow and to her starboard from the mount's
	 * middle. */
	Spot HatchOf(const GunShape& G, const ConvoyLook::GunMount& M);
	/** A man's own axes at his post, from her frame (turned his facing), and back. */
	Spot ToHer(const Post& P, double X, double Y, double Z);
	Spot ToHim(const Post& P, const Spot& Her);

	/** What the show says of her this frame (ShipOut): whether a boat, her gun laid, she surfaced (Dive) and how far down
	 * (Dived), in her line, struck, its train and elevation, when it last fired and when a shell last shook her. */
	struct Told
	{
		bool bBoat = false, bLaid = false, bInLine = false, bStruck = false, bAft = false;
		std::string Dive;
		double Dived = 0.0, Train = 0.0, Elevation = 0.0, FiredAt = -1.0, ShakenAt = -1.0;
		int Key = 0;
	};
	Told TellOf(const SeaShow::ShipOut& Ship);

	/** Where a man is on his way: below, climbing up the hatch, running to his post, at it, running back, waiting at the
	 * hatch, climbing down, or gone below. */
	enum class Phase : int
	{
		Below,
		ClimbUp,
		RunUp,
		AtGun,
		RunBack,
		Wait,
		ClimbDown
	};
	/** How fast and in what order they go [EST]: each up the hatch once the one before is out of it and OutOf metres on
	 * his way (the layer, the trainer, the loader, and the passer last, to his rung), climbing at ClimbUp metres a second
	 * from Depth under her deck, running at Run round the mount at AroundR (on a boat's narrow casing nearer it, Inboard
	 * inside her edge); and down at the diving alarm, all at once, the passer first, each into the hatch once the one
	 * before is gone below it (no two in it: Depth is under HatchApart, two men's feet apart), dropping at ClimbDown, those
	 * after him waiting in a line on their way short of it (WaitShort straight from its middle, and WaitApart more each:
	 * beside it, never over it). The loader's work at Rate times its clip's pace (a boat's gun is quicker than its clip, a
	 * merchant's slower than its cadence), and at Hold times that while he waits for the gun to fire, his hands out for the
	 * next round (to and fro in his clip up to HoldTo). */
	constexpr double Depth = 1.6, ClimbUpRate = 0.9, ClimbDownRate = 3.0, RunRate = 3.0, OutOf = 0.4, HatchApart = 1.8;
	constexpr double PasserDepth = 2.4, AroundR = 1.35, Inboard = 0.25, WaitShort = 0.75, WaitApart = 0.6;
	constexpr double BoatRate = 1.2, MerchantRate = 1.0, Hold = 0.3, HoldTo = 0.6;
	/** His rung in the passer's hatch: his root (feet) under the deck when he climbs onto it (the clip's feet's). */
	constexpr double PasserRung = -0.81;

	/** One man of the crew. */
	struct Hand
	{
		Phase Now = Phase::Below;
		double Since = 0.0;            // when he began it (battle seconds)
		double Height = 0.0;           // climbing: his root under the deck (metres, 0 at it; the passer's, under his rung)
		double Ran = 0.0;              // running: how far along his way (metres)
		std::vector<double> Way;       // running back: his way to the hatch from where he was (fore, right pairs), kept
		double DuckFrom = -1.0;        // crouched from (a shell close aboard), -1 not
	};

	/** How a man is shown now: whether he is (none below or gone), his clip and time in it, his root and facing, and
	 * whether at his work at the gun (its wheel, its round; the loader's from the rack where no passer hands it up; not
	 * the loader standing clear as he steps round behind the breech, SwingReach's). */
	struct Shown
	{
		bool bShown = false, bAtWork = false;
		SeaSailors::Act What = SeaSailors::Act::Stand;
		float Time = 0.f;
		Post At;
	};

	/** Whether the scene draws a man as he is shown: he is (none below or gone) and the show draws her (ShipOut's bShown:
	 * none on a boat it hides). Tools/convoy_looks checks the show's nights through this. */
	inline bool Drawn(const Shown& Is, const SeaShow::ShipOut& Her)
	{
		return Is.bShown && Her.bShown;
	}

	/** A cue his work makes this frame (round_handoff, shell_ram, breech_close, round_lift): who, and its name. */
	struct Cue
	{
		Role Who = Role::Loader;
		std::string Name;
	};

	/** A gun's crew through the battle: one a ship, stepped each frame from what the show says (Told). */
	struct Watch
	{
		bool bManned = false;   // called up (a merchant's: at her gun) and not all below again
		double CalledAt = -1.0, SentAt = -1.0;
		bool bAft = false;      // a merchant's gun manned: her stern gun
		double TrainSent = 0.0; // the gun's train when sent below: their ways back kept as they were
		Hand Hands[Roles];
		double Clip = 0.0;      // the loader's (and the passer's) time in his clip
		bool bWaiting = false;  // the loader's: a round loaded, his hands out for the next
		double HoldWay = 1.0;
		int Pending = 0;        // a round fired after he rammed the one he has loaded (at most one)
		int HatchFor = -1;      // going below: the man the hatch is his (stepping into it, or in it), -1 none
		bool bPasser = false;   // the passer comes up (PasserEver at the call)
		bool bHanding = false;  // the passer hands the rounds up now (PasserHolds), else the loader's from the rack
		double SeenFired = -1.0;
		double LastT = -1.0;
		double Turned = 0.0, Slid = 0.0; // the loader's post now (Post's), stepping round to PostOf's
		bool bLoaderWas = false;         // the loader shown at his post after the last Step: his root then, and the mount's train
		Spot LoaderWas;
		double BearsWas = 0.0;
		/** Each man as shown after the last Step. */
		Shown Men[Roles];
	};

	/**
	 * Steps her gun's crew to Now (battle seconds) from what the show says (In), at the gun G (M's, on her model; for a
	 * boat her casing B), with the files' blocks K. The cues their work makes this step go to OutCues where given (none
	 * across a skip, a step of more than MaxStep seconds, or a step back: a step back starts the watch afresh; across a
	 * skip the loader starts his clip afresh, waiting for the next round; the round_handoff cue only where the passer hands
	 * it up). A round the show fires (In's FiredAt, this step's own as her gun is first told among them) before the layer,
	 * the trainer and the loader are at their posts has them there at once, as across a skip, the loader waiting with the
	 * next as it fires; one fired as he holds his round short of the breech shut (Unrammed) has it home and shut as it
	 * fires: no cue for either (the gun's own report).
	 */
	constexpr double MaxStep = 0.25;
	void Step(Watch& W, const Told& In, const GunShape& G, const ConvoyLook::GunMount& M, const Blocks& K, double Now, std::vector<Cue>* OutCues);

	/** The quickest the show's gun may fire again after a round and find another rammed by then (seconds): a whole turn
	 * of his clip at its rate (whatever he was doing as it fired, he has rammed the next within it). */
	double FastestReload(const Blocks& K, bool bBoat);
	/** Whether the loader at his post has a round in his hands short of the breech shut (his clip from the round taken to
	 * breech_close, not waiting): a round the show fires then, Step has him ram it and shut the breech as it fires, so no
	 * round leaves a gun open with its round in his hands. (Every round the show fires leaves a gun with its layer, its
	 * trainer and its loader at their posts and a shut breech: Step has them at their posts at once where they are still
	 * coming up. Tools/convoy_looks checks each round by what the scene draws.) */
	bool Unrammed(const Watch& W, const Blocks& K);

	/** Where the round is drawn in the loader's hands at a time in his clip (his own axes): his clip's track, never past
	 * the gun's chamber's mouth (Mouth, in his axes: a boat's open gun's; with the bore's way Bore), and from the ram slid
	 * home along the gun's own bore by the last key it shows at (as far behind the mouth along it as his clip has it
	 * behind along his own way: turned about the breech, he rams across it); false while it is not shown (in the
	 * passer's hands, or home). A merchant's gun (bOpenGun false): his clip's track as it is, into her gunhouse's back. */
	bool LoaderRound(const Blocks& K, float Time, bool bOpenGun, const LandMen::Vec3& Mouth, const LandMen::Vec3& Bore, SeaSailors::RoundAt& Out);
	/** The last key of the loader's track the round shows at (seconds into his clip). */
	float LastShown(const Blocks& K);
	/** The round on a ready-use rack at the loader's left, where no passer hands it up: where the passer's clip holds it
	 * up for him (its first key, moved into his axes by their pair), shown from the start of his loop until he takes it
	 * (Taken); false after. */
	bool RackRound(const Blocks& K, float Time, SeaSailors::RoundAt& Out);
	/** The loader's hands over the gun's own chamber's mouth at his post (metres: his clip's breech, set on her deck at
	 * his root, less the model's mouth's height; the files' gun_loader stands taller at the breech than the boats' guns lie
	 * low, a gap his round is eased across from when it is laid in the tray to when it is home). */
	double BreechHigh(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, const Post& Loading, double Train, double Elevation);
}
