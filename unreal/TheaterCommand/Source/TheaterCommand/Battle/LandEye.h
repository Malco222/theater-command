#pragma once

#include "Battle/LandFeed.h"
#include "Battle/LandPlay.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * The ground battle's camera director (Brian, 21 November 16:22Z: "the same intense feeling as the air battle as if
 * i'm watching" a war film; a feeling to aim for, nothing copied). Plain C++, so the cloud test can run the whole
 * battle through it and check that the eye never goes under the ground, never cuts too often, and is where the
 * fighting is when men fall.
 *
 * How a war film is shot, which this follows: mostly from among the men, at their height, hand-held, with something
 * near in the frame; the lens is often long and close on a few men; the wide shots are few and short, there to say
 * where everyone is; the camera is there before the shell lands, not after. The recording is known from the start, so
 * the director looks a few seconds ahead (as a film's director knows the script) and frames the men who are about to
 * be under fire, never inventing anything.
 *
 * The shots:
 *  - Shoulder: hand-held at a man's shoulder (1.5 m up, a metre behind him), looking where he looks.
 *  - Low: lying in the grass beside a man who is down in cover (0.4 m up), rounds striking ahead; over standing corn or
 *    tall grass, just over the ears (World::Crop), never among the stalks.
 *  - Tank: riding with a tank, 3 to 5 m behind her turret and over her hatch, looking down her gun.
 *  - Track: a tracking shot from 20 to 40 m off a tank's side as she drives and fires, a long lens on her.
 *  - Watch: from a bank or a rise 3 to 7 m up, 28 to 55 m off (a fall about to come: 10 to 18 m off, at a man's
 *    height), a long lens on a few men under fire.
 *  - Wide: 40 to 70 m up behind a side's line, across the field: where everyone is. Only to close the battle, and
 *    once in the middle of it, high over the own side's rear, the whole mass of the men in one picture
 *    (Settings::MassFrom: the scale of a battle of thousands).
 *  - Open: the opening's slow look over the field from high behind the attack, coming down.
 *
 * The art director's notes on the first film (21 November, 19:27Z), which it now keeps: no lens within 1.5 m of a
 * thing (0.9 m of a man) or under the ground, and no picture more than 30 per cent filled by something a few metres
 * from the lens (World::Solid; a shot that becomes so is left at once); cut on action, to what fires, bursts or falls
 * in the next one to two seconds, and leave a shot four seconds after the last thing happened in it; a man falling is
 * kept in the picture to the ground and held a second or two where he lies; one aerial to open, one to close; tanks
 * ridden and tracked from the side as they drive and fire; men framed near, not as small figures.
 */
namespace LandEye
{
	using LandPlay::Vec;

	enum class Kind : unsigned char { Open, Wide, Watch, Shoulder, Low, Tank, Track };
	const char* NameOf(Kind K);

	/** Why a shot was left: the opening done, its length run, the fall it held for, an event coming it went to, nothing
	 * happening in it, something in front of the lens (a rescue: the only cut sooner than Settings::ShotLeast), a key
	 * or a picture asked for, its subject gone from the field; a shell bursting at the lens (cut on its flash, the
	 * other cut allowed sooner than Settings::ShotLeast); its own man out of the picture or hidden in the corn. */
	enum class Why : unsigned char { Start, Opening, Length, Fall, Action, Dead, Rescue, Asked, Gone, Burst, Lost, Dark };
	/** A shot of its own kind of moment (release 2): the first bail-out, the whistle that starts the attack, an earned
	 * nickname's first kill, a last stand. */
	enum class Special : unsigned char { None, Bail, Whistle, Nick, Stand };
	const char* NameOf(Why W);
	const char* NameOf(Special S);

	/** Where the eye is, what it looks at, the lens (vertical field of view, degrees) and the roll (degrees). */
	struct Pose
	{
		Vec Eye, Look;
		double Fov = 55.0;
		double Roll = 0.0;
		Kind Shot = Kind::Open;
		int Subject = -1;
		/** True on the frame of a cut (the scene snaps, the sky and effects may re-sort). */
		bool bCut = false;
		/** The film's shallow focus (the graphics audit's E9, Brian's D3): on a close follow shot (Watch, Track, Low on
		 * its man, Shoulder past its man) the distance in metres held sharp, pulled after the subject as a focus puller
		 * would; 0 on the wide ones, where all of it is sharp and the scene draws no depth of field at all. Decided at the
		 * cut, so the scene's depth of field is never switched on or off in the middle of a shot. */
		double Focus = 0.0;
		/** The shot's special moment (Special), for the scene: the whistle start, a nickname's first kill, a last stand,
		 * the first bail-out's watch. */
		Special Sp = Special::None;
		/** A nickname's first kill (Settings and World::Nickname): the block, as the recording numbers it, whose earned
		 * nickname the scene shows on the picture while the shot holds (-1: none). */
		int Card = -1;
	};

	/** Whether a point is in a pose's picture (its vertical lens, the picture Aspect wide for 1 high), with a margin. */
	bool InPicture(const Pose& P, const Vec& At, double Margin = 1.0, double Aspect = 16.0 / 9.0);

	/**
	 * A vehicle's wireless aerial near the lens (Brian's picture of 25 November 2026, 16:35Z: the film riding behind a
	 * Sherman's turret, her own aerial 2 to 4 m before the lens, a thin dark line through the whole picture from the
	 * bottom to the top; the art director's stills of the desert had the same line). An aerial is a rod a thumb thick
	 * and over 2 m long standing as high as the ride's own eye: the lens's 1.5 m from a thing does not keep it out of
	 * the picture, and to keep the eye off it by enough would take the ride from behind the turret. So the scene puts
	 * it away, as a film crew stows the aerial of the tank its camera rides: how much of a vehicle's aerial is drawn,
	 * 1 the whole of it down to 0 not at all, by how far the picture's eye stands from the nearest point of it. Within
	 * AerialAway none; from AerialWhole all of it; between them it is drawn that share of its length from its foot
	 * (it runs out of its mounting as the eye draws off, eased at both ends, so by the eye's own going nothing
	 * appears or goes at a stroke). The longest in the kit, 2.5 m, is then never more than about 24 degrees long to
	 * the eye: looked straight at, a little over half the picture's height through the general's nearest lens (44
	 * degrees) and under half through the ride's; through a narrower lens, or standing at the picture's side, where
	 * a flat picture draws everything taller, it takes more. bRidden: the vehicle the film's ride is on (RiddenBy):
	 * none of hers for the whole shot, however far back the ride stands. Foot: where it is mounted, a field point;
	 * Long: its length, taken as standing upright (it leans and whips a hand or two). The picture's eye alone: the
	 * film in its corner window (LandBattle.h, CornerLens) is drawn from the field laid for his own eye, and there a
	 * ridden tank's aerial stands before the film's lens as it did.
	 */
	constexpr double AerialAway = 3.0, AerialWhole = 6.0;
	double AerialShown(const Vec& Eye, const Vec& Foot, double Long, bool bRidden);
	/** The track the film's ride is on in a pose (its Tank shot's subject), -1 in any other shot: AerialShown's bRidden. */
	int RiddenBy(const Pose& P);

	struct World
	{
		std::function<double(double X, double Y)> Height;
		/** Whether the eye can stand at a point (not inside a bank, a hedge's shrubs, a house or a tree); unset: yes. */
		std::function<bool(const Vec& At)> Free;
		/** Whether the line between two points is open (no bank, house or hedge across it); unset: yes. */
		std::function<bool(const Vec& From, const Vec& To)> Open;
		/** Where a track is now and which way it faces (the cores' own: a man's head, a tank's turret). Unset: the
		 * recording's track. */
		std::function<bool(int Track, double T, Vec& OutAt, double& OutFacing)> Where;
		/** How high standing corn or tall grass stands over the ground round a place (the tallest within about 0.7 m),
		 * metres; 0 where there is none, or only short grass. Unset: none anywhere. A lens near the ground is never put
		 * among the stalks (the first stills: a low shot 0.25 m up inside standing wheat, a few stalks filling the
		 * picture like planks): over corn the low shot is lifted just over the ears, and the director takes a shoulder
		 * shot over a man in tall corn rather than a low one. */
		std::function<double(double X, double Y)> Crop;
		/** What a point is inside besides the ground: 0 nothing, 1 a man, 2 any other thing the scene draws (a tank, a
		 * gun, a tree's trunk or crown, a hedge's shrubs, a house, a wall; LandSight::Room). Except: a track left out
		 * (the man or the tank the shot is on). Unset: nothing anywhere, and only the ground, Free and Open are asked. */
		std::function<int(const Vec& At, int Except)> Solid;
		/** Where a man's body is now (his chest, falling or fallen; for a vehicle her hull's middle), so a fall is
		 * followed to the ground. Unset: where he stands, a metre up (or a fifth of one once he has fallen). */
		std::function<bool(int Track, double T, Vec& OutAt)> Body;
		/** Whether a block is hidden from the player now: an enemy unit in a wood he has not found (Brian, 22 November:
		 * the forest ambush). The film never takes such a block's men or vehicles as a shot's subject, never goes to its
		 * fall or knock-out, and leaves a shot whose subject becomes hidden; his own hidden blocks are not his enemy's and
		 * are filmed as ever. Unset: nothing hidden. */
		std::function<bool(int Block)> Hidden;
		/** The share of the picture last drawn that is darker than L 25 of 255 (0 to 1; below 0 not known yet), read by
		 * the scene from its own frame a few times a second. The art director's dark rule (cut 4 fix 6: a hedge's shaded
		 * face filled 75% of a still): a shot whose picture stays over Settings::DarkLeave is left, and the next stands
		 * clear of where it stood. Unset: never asked (the rays cannot see light). */
		std::function<double()> DarkNow;
		/** Whether a vehicle's crew may get out at her end (the effects thread's bail-out, release 2, -TheaterLandBailOut:
		 * a tank or an armoured car with a fates row and a soldier file for her crew). Asked before her kill, as the film
		 * goes to a tank's end 1 to 3 s ahead; Director::BailOut then says at the kill whether anyone did. Unset: none,
		 * and the film is as it was. */
		std::function<bool(int Track)> Bails;
		/** A block's earned nickname (Brian's nicknames, release 2: the campaign gives a unit that has earned one its name,
		 * which the recording does not carry), empty for none; the block as the recording numbers it. Asked once a block.
		 * Every block of a corps may carry its corps' name: each name is carded once, on the first of its blocks' kills
		 * the film can go to. Unset: no nickname shot at all, and the film is as it was. */
		std::function<std::string(int Block)> Nickname;
	};

	struct Settings
	{
		/** The side whose men the camera stays with most (0 the attack, the player's in this dry run). */
		int Own = 0;
		double OwnShare = 0.7;
		/** How long a shot holds, real seconds (a war film cuts every 3 to 8 s in a fight). */
		double ShotMin = 4.5, ShotMax = 9.0;
		/** The opening's length. */
		double Opening = 9.0;
		/** The camera's share of a frame for a cut's choice, counted by the ground asked under its rays (World::Height's
		 * calls, about a third of a microsecond each with what is asked at the same place), the frame's other work first:
		 * past it the weighing of the men and the tries go on next frame and the shot on the screen holds meanwhile, so no
		 * frame of the camera's passes about 2 ms (the production lead's plan for battles of 8,000 men, 22 November). An
		 * asked shot, and a shot whose man has gone into hiding, are chosen at once. 0: a cut's choice all in one frame. */
		int ChooseSteps = 3000;
		/** And by the clock: past ChooseMs milliseconds of the camera's frame, the choice goes on next frame too, whatever
		 * the steps (0: steps only). Brian's Mac read a step at some 5 to 8 microseconds against the cloud's 1, so 3,000
		 * steps a frame were 14 to 27 ms there: LandCamera among a long frame's three heaviest jobs some 50 times a battle
		 * (the lit Normandy trace of 22 November). The test keeps it 0, so its runs are the same on any machine. Whatever
		 * the clock, a frame goes ChooseLeast steps into the choice, so it always goes on. */
		double ChooseMs = 3.0;
		int ChooseLeast = 250;
		/** The hand-held wobble's strength (0 off, 1 as made). */
		double Hand = 1.0;
		/** Brian's feel for the ground battle (22 November: a war film's fury with a squad's-eye realism, the feel only,
		 * every shot our own):
		 *  - The film's own section: one squad of the own side, the one that will see the most of the fight, is gone back
		 *    to through the battle (one shot in SquadEvery is on its men), so they become faces the film knows.
		 *  - In the fight a man is watched from beside him at his own height (NearWatch of the watch shots), not from a
		 *    bank 30-50 m off; the bank keeps the rest, for where the men are.
		 *  - Under shelling the holds are short and nervous: a shot with heavy rounds landing round its man in its time
		 *    holds ShakenHold of its length, and the hand shakes harder after a burst near the lens (NervesNear metres:
		 *    up to NervesMost more wobble, settling over NervesFade real seconds). */
		int SquadEvery = 3;
		/** The art director's notes on the third cut (22 November): a place on the ground among the men is used once in
		 * the film (no shot among them within SpotOnce metres of where one stood before); bushes and banks short of
		 * the subject screen no more than ScreenMost of a new shot (25%); men pass no nearer the lens than CrossClear
		 * (2 m); a ride on a tank's hull with nothing happening is left after DeadAfter, before contact too. */
		double SpotOnce = 5.0;
		double NearWatch = 0.6, ShakenHold = 0.65;
		double NervesNear = 30.0, NervesMost = 0.9, NervesFade = 2.0;
		uint64_t Seed = 1;
		/** The art director's rules for the lens: no nearer a thing than ThingClear metres, a man than ManClear, and
		 * no picture more than BlockedMost filled by a thing within NearBlock metres of the lens (a man within
		 * ManBlock), or by the ground within GroundBlock. The shot's own man is not counted. (Men pushed back on the
		 * second cut's stills, 21 November: "nearest man 3 m or more, wholly in frame or out"; was 0.9 and 1.5.) */
		double ThingClear = 1.5, ManClear = 2.0;
		double NearBlock = 3.5, ManBlock = 3.0, GroundBlock = 0.8, BlockedMost = 0.3;
		/** What screens the picture farther out (the art director on the second cut's film, 21 November 22:46Z: a watch
		 * shot behind a hedge under a tree, the battle through a window; a tracking shot drifted into a hedge and black;
		 * six seconds on one orchard tree): leaves, trunks or banks covering more than ScreenMost of the picture short of
		 * what it looks at (out to ScreenFar metres) turn a shot away, and a shot running past ScreenLeave is left. A ray
		 * in the picture's upper half meeting the ground within BankNear metres is a bank's face. The running look also
		 * asks where the eye will be AheadLook real seconds on, so a shot is left before it drives into a hedge. */
		double ScreenFar = 30.0, ScreenMost = 0.25, ScreenLeave = 0.45, BankNear = 6.0, AheadLook = 0.8;
		/** The shortest a shot holds, real seconds (only a rescue cuts sooner). */
		double ShotLeast = 3.0;
		/** A shot screened over ScreenMost this long on the screen, seconds, once it has held ShotLeast, is left: placed
		 * clear, Sedan's shots walked and drove on into the trees, a fifth to two fifths screened for four seconds. */
		double ScreenStrain = 1.0;
		/** Once the armies are near, a shot in which nothing has happened for this long (real seconds) is left. */
		double DeadAfter = 4.0;
		/** How long a fall is held after the man is hit: his fall to the ground and a moment where he lies (real s). */
		double FallHold = 4.0;
		/** How long into a fall nothing else cuts it (real s): the man seen on his way to the ground. After it a tank's end
		 * is gone to, and a man walking into the lens ends the hold (a fall at Kursk was held 2 s with a man at the lens). */
		double FallSeen = 0.8;
		/** The camera director's rules from the second cut's frame check (21 November): what the operator of a war film
		 * would never let happen.
		 *  - Its own man: the lens never nearer his head or chest than OwnClear (the lens was at a man's face as he rose
		 *    out of a shell hole), and he never fills more than OwnMost of the picture (1.4 s of a man's back and seat).
		 *  - A shell at the lens: a shell, mortar bomb, tank or anti-tank round landing in the picture within BurstClear
		 *    metres of the lens (a 105 mm one BurstBig times that), or within BurstAround anywhere round it, blacked out
		 *    the second cut for 0.8 s. No shot is taken that has one in its first ShotLeast seconds; one that comes later
		 *    is cut on its flash, BurstFlash real seconds after it lands.
		 *  - Men walking into the lens: a man of the recording passing within CrossClear metres of where the lens will be
		 *    in the next CrossAhead real seconds (a soldier stepped over the lens; a shot opened pressed to two backs).
		 *  - Its own man lost: out of the picture, or down in corn taller than he lies, for LostAfter real seconds (a hold
		 *    on a bank's blank face after he went prone; six seconds of empty wheat).
		 *  - Before the armies meet, a shot in which nothing moves or fires is left after QuietAfter real seconds (8.3 s
		 *    on a parked tank looking into an orchard).
		 *  - Leaves or a trunk within EdgeNear metres of the lens, in more than EdgeMost of the picture: black leaf cards
		 *    poking into the corners, a trunk cut off in mid air beside the lens.
		 *  - The look swings on a spring (no kick as it starts): LookRate on a man's fall (a 62-pixel lurch at a hit). */
		double OwnClear = 1.0, OwnMost = 0.4;
		/** The step back from its own man on a spring of this rate (1/s), never nearer than OwnNearest of OwnClear. */
		double PushRate = 14.0, OwnNearest = 0.82;
		/** Its own man come in a frame within OwnCutInside of OwnClear of the lens (a man lying down turned about where he
		 * lay): the shot is cut away from, not shoved a metre. */
		double OwnCutInside = 0.55;
		/** Nor shoved farther than this in a frame to keep OwnNearest (metres): a crawling man whose track hopped 2.4 m at
		 * Prokhorovka shoved the lens 0.47 m in one frame, a kick of 1,869 deg/s/s. Cut away instead. */
		double OwnShoveMost = 0.15;
		/** The lens lifted over banks and corn on a spring of this rate (1/s): an operator standing up, not a leap. */
		double LiftRate = 12.0;
		double BurstClear = 6.0, BurstBig = 1.4, BurstAround = 3.5, BurstFlash = 0.12;
		/** A burst's cloud (the art director, 22 November, from the effects kit's near-lens cap: a lens some 15 m inside a
		 * 500 lb bomb's cloud still went dark): no shot holds its lens within a cloud's reach of a burst in its first
		 * CloudFor seconds on the screen. CloudShell metres for a 75 mm (a tank's or an anti-tank gun's round),
		 * CloudMortar for a mortar bomb, CloudGun for the battery's 105 mm, CloudBomb for a 500 lb bomb. A burst landing
		 * that near a held shot cuts it on its flash; no shot is placed in a cloud still hanging, and a following shot
		 * that runs into one is left. */
		double CloudFor = 4.0, CloudShell = 10.0, CloudMortar = 9.0, CloudGun = 12.0, CloudBomb = 30.0;
		/** A fuel ball (the art director, 22 November 08:08Z, from the effects kit's fuel_ball: a vehicle's 6 to 10 m
		 * across): no shot places or holds its lens within FuelVehicle metres of a tank or a lorry knocked out, in the
		 * ball's first FuelFor seconds on the screen; one going up that near a held shot cuts it on its flash. Held as
		 * a burst's cloud is (InCloud, LensBurst). Farther off, the ball may fill the picture: that drama is wanted. */
		double FuelFor = 2.0, FuelVehicle = 16.0;
		/** A live battle (the general's, fought as it is shown): the film's own section is chosen HeroAfter battle seconds
		 * into the fight, from what has been fought, and again when its last man is down. Lean: the order the general
		 * gave a block draws the film's next cut to its men, if one comes within LeanFor real seconds. */
		double HeroAfter = 20.0, LeanFor = 8.0;
		/** Radio call, then the barrage (Brian's release 2 camera ask): a forward observer's fresh call for fire (his cl
		 * marks with their spot; none of his guns' rounds landing from RadioQuiet battle seconds before it to RadioSoonest
		 * after), whose first salvo lands RadioSoonest to RadioLatest battle seconds after it, RadioRounds or more of his
		 * rounds within RadioNear metres of the spot in the 15 s from then. The film cuts to him as he calls, then to the
		 * men under the fire RadioLead real seconds before the first salvo lands, and holds for it; one call in RadioEvery
		 * real seconds at the most (0: none). A recorded battle only: a live one's rounds are not known ahead. */
		double RadioEvery = 40.0, RadioLead = 1.2, RadioQuiet = 8.0, RadioSoonest = 4.0, RadioLatest = 14.0, RadioNear = 150.0;
		int RadioRounds = 3;
		/** The shallow focus (Pose::Focus): a close follow shot whose subject is within FocusFar metres at the cut is shot
		 * in shallow focus; the distance follows the subject on a critically damped spring of FocusPull a second (a focus
		 * puller, not a snap), and never past FocusMost metres. FocusFar 0: never, and nothing asked for it a frame. (The
		 * film is mostly close shots: at 40 m or 80 m alike some 47 of 50 shots are in it, 94-95% of the fight's frames, so
		 * the Mac pays for the pass nearly all fight long. Held at 0 from 21:22Z, on again at 23:10Z (Brian's "full speed
		 * ahead on graphics"): it ships on only if the Mac's A/B, -TheaterLandFocusLive=0 against 1, holds 45 fps.) */
		double FocusFar = 40.0, FocusPull = 5.0, FocusMost = 300.0;
		/** The first bail-out (the effects thread's ask, 23 November, release 2, with World::Bails): the first tank's end
		 * of a battle whose crew may get out, gone to as ever, is watched from BailNear to BailFar metres off her side
		 * toward her rear, 1.2 to 2 m up, a 24 to 32 degree lens, the look BailLookBack metres behind her middle (her
		 * hatches, her men dropping off her side and running back in her lee, all in the picture; nobody running at the
		 * lens). Once Director::BailOut says her crew is getting out, the shot holds from her hit for the longer of the
		 * first man up and BailRun seconds, and her ammunition going and BailBlast seconds, at least BailLeast, at most
		 * BailMost (real seconds), with no cut to another tank's end, a fall or a radio call meanwhile (a shell or a fuel
		 * ball at the lens, a man walking into it, still cut). Once a battle; later bail-outs are held as any tank's end.
		 * Not while a radio call's barrage is owed (the film went to the man calling it): the next crew is watched for. */
		double BailNear = 22.0, BailFar = 38.0, BailLookBack = 3.0;
		double BailRun = 8.0, BailBlast = 2.5, BailLeast = 10.0, BailMost = 15.0;
		/** The whistle start (Brian's yes, 23 November 19:52Z, release 2, -TheaterLandWhistle; off till the art director
		 * passes it and the Mac holds 45 fps): the attack's step-off (the first move of an attacking infantry block, its
		 * phase out of its hold, as the recording marks it) is shot on the block's officer from off his side, a little
		 * ahead of him, 9 to 14 m, at a man's height: he blows his whistle, he and the men about him climb out and go,
		 * held WhistleHold real seconds after it as firmly as the first bail-out. A step-off within WhistleOpen battle
		 * seconds of the start opens the film, and the opening aerial follows it; a later one is cut to from WhistleLead
		 * before it (missed if no cut can come by WhistleLate after it), and the film goes on as ever. */
		bool bWhistle = false;
		double WhistleLead = 1.5, WhistleHold = 3.5, WhistleLate = 0.8, WhistleOpen = 8.0;
		/** A nickname's first kill (Brian's yes, 23 November 19:52Z, release 2, with World::Nickname): a block with an
		 * earned nickname, about to make its first kill, is cut to: the man or the tank who fires it, close, from off his
		 * front, the kill landing NickLead real seconds in, held NickAfter after it, the nickname on the picture
		 * (Pose::Card) all the while. Its first kill the film can go to within NickLate battle seconds of its first; at
		 * most NickMost in a battle, NickEvery real seconds apart. */
		double NickLead = 1.0, NickAfter = 2.0, NickLate = 45.0, NickEvery = 25.0;
		int NickMost = 3;
		/** The last stand (Brian's yes, 23 November 19:52Z, release 2, -TheaterLandLastStand): once a battle, a section of
		 * infantry (StandFrom men at least; a platoon of more than 14 split by its squad leaders) down to StandMen
		 * standing or fewer and StandShare of itself, with no more than StandFriends of its own side within StandAlone
		 * metres of them, the enemy within StandEnemy and still firing (in the StandFire seconds after), is watched from
		 * behind their shoulders, the enemy before them (the later tries from over them, nearer), and held StandLeast to
		 * StandHold real seconds, to its end if it comes sooner (they fall, or go), as firmly as the first bail-out. Only
		 * from a place two of them are seen from, up and not behind the stalks, as it begins and halfway through: a group
		 * no place shows is let be, and the third such ends the look. */
		bool bLastStand = false;
		int StandMen = 4, StandFrom = 6, StandFriends = 3;
		double StandShare = 0.4, StandAlone = 30.0, StandEnemy = 250.0, StandFire = 8.0, StandHold = 9.0, StandLeast = 4.0;
		double CrossClear = 2.0, CrossAhead = 2.0;
		double LostAfter = 1.0;
		double QuietAfter = 5.0;
		double EdgeNear = 2.5, EdgeMost = 0.05;
		double LookRate = 6.0;
		/** The ground or corn rising ahead of a moving lens: it starts up the slope this many metres a metre before it gets
		 * there, as an operator climbs a bank, not lifted a metre in four frames (the second cut: pushed up against a
		 * bank's face, then popped over its top). */
		double Climb = 0.6;
		/** The most a following shot's operator turns with his man, degrees a second. */
		double TurnMost = 45.0;
		/** The dark rule (World::DarkNow; the art director, cut 4 fix 6: reject a shot over 30% under L 25): a shot whose
		 * drawn picture stays over DarkLeave for DarkHold seconds, past its first quarter second (the frame drawn lags the
		 * cut), is left, and no later shot stands within DarkClear metres of where it stood. After DarkRunMost such cuts
		 * running the light is to blame, not the place: the rule rests until a picture has read no darker than DarkLight
		 * for DarkHold seconds on end (past its shot's first quarter second), which shows that the light is not. Half
		 * the leaving line, so that a dim battle whose pictures hover about the line does not start the run again and
		 * again; and held as long as the dark must be, so that one reading does not: the scene's reading is of a frame
		 * asked for up to a quarter of a second before and come back later still, so a shot's first may be the last
		 * shot's picture (an opening's or a wide's too, which the rule never judges), and one frame may be a shell's
		 * flash. (Until 25 November the run started again at every cut made for another reason: under a light dark
		 * everywhere, a dusk or a storm, two shots of three quarters of a second followed nearly every ordinary cut all
		 * battle long. It never showed because the scene's reading was blind: LandBattle.cpp ReadDark.) */
		double DarkLeave = 0.30, DarkHold = 0.5, DarkClear = 6.0, DarkLight = 0.15;
		int DarkRunMost = 2;
		/** The whole mass (the production lead's plan for battles of thousands, 22 November: CD1): once a battle, between
		 * MassFrom and MassTo battle seconds (in a battle too short for them, between 55% and 72% of it), one wide shot
		 * high over the own side's rear holding MassShare of the men drawn in its picture (every man on the field the
		 * player can see: the batteries far back and the fallen too) from its first frame to its last, MassLength real
		 * seconds. Its eye no more than MassHighest metres over the men. The field is its subject: it is not left for a
		 * tank's end or a man's fall, and it waits for a moment with no tank about to be knocked out in its time unless the
		 * window is closing. MassFrom 0 or less: none. */
		double MassFrom = 150.0, MassTo = 200.0, MassShare = 0.85, MassLength = 7.0, MassHighest = 500.0;
		/** A front wider than this (the middle nine tenths of the men across the own side's rear way, metres): the wide
		 * holds MassShare of the men in its densest MassFront instead of the whole (MassStretch). */
		double MassFront = 1200.0;
		/** No place fit on the best four men in the middle of the battle (men packed shoulder to shoulder, every place in
		 * a burst's cloud): the field weighed again for men at least this far (metres) from them, both sides, before the
		 * high view, up to ElsewhereTimes times a cut. Sedan's waiting army in the woods went up to the aerial six times
		 * running. 0: none. */
		double Elsewhere = 60.0;
		int ElsewhereTimes = 2;
		/** Riding with a unit (or going to the men the general sent), its other men this far from the four none fit on
		 * are weighed once before the field: Kursk's ride left its 120 men for 7 s in 80. */
		double ElsewhereRide = 20.0;
		/** The picture run's asked shots (the ground's cut 5: "among the men" and "in the grass" asked of the attack's
		 * first man, an officer waiting among his headquarters' men, every place by him crossed by one of them; "the
		 * fallen" asked of a man lost in the grass): an asked shoulder, low, tank or village shot that will not fit on its man is
		 * tried on up to AskOthers other men, his own unit's first, then his side's within AskWithin metres (standing, or
		 * fallen as he is), each AskApart from the others tried, two places each, before it is placed unfit. */
		int AskOthers = 12;
		double AskWithin = 400.0, AskApart = 30.0;
		/** An asked still keeps other men CrossClear and ManClear off (2 m) like any shot. A man may come nearer, to
		 * AskCross metres, only running past: within CrossClear of the lens for no more than AskPass seconds on the
		 * screen, a blur through the picture, never held (the art director on 83749bb4's 04: "a man that close is mostly
		 * out of frame, and our near model shows its blocks"; cut 5's 04 read well with its nearest man at about 2 m).
		 * And an asked low shot on a man up keeps him beside the lens, out of the picture, as the low shot is made (the
		 * grass and the men ahead are its picture). */
		double AskCross = 0.9, AskPass = 0.5;
		/** An asked still left within AskHold real seconds (before the picture run can have taken it: it asks two battle
		 * seconds ahead) is asked again, at most AskAgainMost times, not where it was left. */
		double AskHold = 3.0;
		int AskAgainMost = 2;
		/** The village still (Ask's Building): a man within HouseNear metres of a house first, else within HouseFar; the
		 * look turned HouseLook of the way from him toward the house; the house at least HouseLeast of the picture's height
		 * across (half its width over the distance) and seen at HouseSeen of its points in the picture at the least. */
		double HouseNear = 120.0, HouseFar = 320.0;
		double HouseLook = 0.35, HouseLeast = 0.08;
		int HouseSeen = 2;
	};

	class Director
	{
	public:
		bool Begin(const LandFeed::Recording& InRecording, const World& InWorld, const Settings& InSettings);
		/** The pose for battle time Battle, real seconds Real since the scene opened (the clock may run faster than
		 * life, or be held: the hand-held wobble and the cut timing go by the real clock). */
		Pose Update(double Battle, double Real, double RealDt);
		/** Forces a cut on the next update (a key), or holds the shot it has (Hold true) until let go. */
		void CutNow() { bCutAsked = true; }
		void Hold(bool bHold) { bHeld = bHold; }
		/** A shot of a kind on a subject asked for (the picture run). Subject -1: the director's choice. Building true: a
		 * house is kept in the picture with its man (the art director's village still, cut 5: a watch shot of a German
		 * in a hedge and wheat, no house anywhere in it): the asked man if he stands near one, else the man of his side
		 * nearest a house, else of either side, and the eye placed so the house stands behind or beside him. */
		void Ask(Kind K, int Subject = -1, bool bBuilding = false);
		/** Ride with this unit (Brian's yes, 22 November 03:36Z): the film stays with one block's men or tanks (its id as
		 * the recording numbers it), cutting among them by the same rules as among the field's: their falls and a tank's
		 * end are gone to, no one else's. It cuts to them at once unless the shot on the screen is already on one of them.
		 * Let go with -1; it lets go of itself when none of them is left standing or they are hidden from the player
		 * (Riding() then -1). No wide of the whole mass while riding. */
		void Ride(int Block);
		int Riding() const { return RideBlock; }
		/** The platoon cam (Brian's yes, 22 November 02:40Z): the film rides with the platoon leading a side's assault
		 * (Side as the recording's blocks number it; -1 lets go), as the recording marks it (each man's Lead spans, the
		 * tactics thread's ld), going on to the next platoon as the lead passes, and among its men most of all to the
		 * leading squad's and, in a rush, to the men moving. Before anyone leads (the approach) the film is the field's;
		 * once a platoon has led, the film stays with it until another takes the lead or none of it stands. Ride() lets it
		 * go: the player's own pick comes first. LeadBlock() is the platoon ridden for the lead, -1 none. */
		void FollowLead(int Army);
		int FollowingLead() const { return LeadSide; }
		int LeadBlock() const { return LeadBlockNow; }
		/** The general has just given a block an order (a live battle): the film's next cut goes to its men, as a war film
		 * cuts from the radio call to the men it sends; the shot on the screen is left as soon as it has run its least
		 * (Settings::ShotLeast), unless it holds for a fall or a tank's end. Not over a tank's end or a fall coming, nor
		 * while riding with a unit; let go if no cut comes within Settings::LeanFor. Once: a new order leans anew. */
		void Lean(int Block);
		int Leaning() const { return LeanBlock; }
		/** Radio call, then the barrage (Settings::RadioEvery), for the test: each call the film went to, its stamp, the
		 * first salvo's landing and its middle, the caller; whether the cut to the men under the fire came (or the shot on
		 * the screen already showed the spot and held for it). */
		struct RadioBeat
		{
			double T = 0.0, Land = 0.0, X = 0.0, Y = 0.0;
			int Caller = -1;
			bool bBarrage = false;
		};
		const std::vector<RadioBeat>& RadioBeats() const { return Beats; }
		/** The fresh calls for fire the film could go to (found once, at the first look). */
		int RadioCalls() const { return static_cast<int>(Radios.size()); }
		/** A vehicle's crew getting out at her kill (the scene calls it as it makes her crew, World::Bails): FirstOut the
		 * battle seconds after her kill the first man starts up out of a hatch, -1 nobody got out; Blast the battle
		 * seconds after it her ammunition goes, -1 never. On the first bail-out of the battle the film is watching for
		 * (Settings::BailNear), the shot holds for it; any other call is let be. */
		void BailOut(int Track, double FirstOut, double Blast);
		/** The first bail-out's hold, for the test: her track, the hold asked (real s from her hit), the real time she was
		 * hit and the shot left (-1 not yet), and why it was left. */
		struct BailBeat
		{
			int Track = -1;
			double Hold = 0.0, Hit = -1.0, Left = -1.0;
			Why LeftWhy = Why::Asked;
		};
		const std::vector<BailBeat>& BailBeats() const { return BailLog; }
		/** The bail watches placed (the shot gone to for a crew that may get out), and whether the shot on the screen is
		 * one, for the test. */
		int BailWatches() const { return BailWatchCount; }
		bool BailWatchNow() const { return ShotSp == Special::Bail; }
		/** The whistle start (Settings::bWhistle): the battle time of the attack's step-off and its officer's track (-1:
		 * none found yet), so the scene can blow his whistle as he steps off. */
		double WhistleAt() const { return WhistleT; }
		int WhistleMan() const { return WhistleTrack; }
		/** The special shots gone for (whistle, nickname, last stand), for the test: which; its man (the officer, the
		 * shooter, the survivor framed) and block; the battle time of its moment (the step-off, the kill, the stand) and
		 * of its end (-1 none); the battle time it was cut to (-1: missed, nothing fit or no cut could come); the firm
		 * hold (real s); the real time it was cut to and left, and why; its shot's kind. */
		struct SpecialBeat
		{
			Special Sp = Special::None;
			int Who = -1, Block = -1;
			double At = -1.0, To = -1.0, Cut = -1.0, Hold = 0.0, CutReal = -1.0, Left = -1.0;
			Why LeftWhy = Why::Asked;
			Kind Shot = Kind::Open;
			std::vector<int> Men;
		};
		const std::vector<SpecialBeat>& SpecialBeats() const { return SpLog; }

		// For the test.
		int Cuts() const { return CutCount; }
		/** The most frames a cut's choice has taken (Settings::ChooseSteps). */
		int LongestChoice() const { return ChoiceFramesMost; }
		/** Shots left for a dark picture (World::DarkNow), and those running now (the rule rests at DarkRunMost). */
		int DarkCuts() const { return DarkLeft; }
		int DarkRun() const { return DarkRunNow; }
		/** Whether the last asked shot found no fit place and was placed anyway (its kind kept for the picture run): a
		 * still the film's own rules would have turned away. */
		bool AskedUnfit() const { return bAskedUnfit; }
		/** Whether the shot on the screen keeps a house in its picture (a village shot asked with Building, placed): false
		 * where no house fit, or none stands on the field (the steppe's cottages are things, not houses), so the picture run
		 * can name the still for what it shows. */
		bool HouseInShot() const { return ShotHouse >= 0 && Shot == Kind::Watch; }
		/** The houses found on the field for the village shot (once, at its first ask), for the test. */
		int HousesFound() const { return static_cast<int>(Houses.size()); }
		/** Finds the field's houses for the village still now (else at its ask, in that frame: some 20 to 40 ms over a
		 * 3 by 2 km field): the picture run calls it as the battle opens. */
		void FindHouses();
		/** The ground asked under the camera's rays so far (the measure of its work a frame, the same on any machine). */
		long Steps() const { return RaySteps; }
		/** Whether the shot on the screen is the whole mass (Settings::MassFrom), and the battle time it was cut to (-1:
		 * not yet); the window it is due in, for this recording. */
		bool MassShot() const { return bMassNow && Shot == Kind::Wide; }
		double MassAt() const { return MassCutAt; }
		void MassWindow(double& OutFrom, double& OutTo) const;
		/** The stretch of a front too wide for one picture the last whole-mass wide held (Settings::MassFront): a man is
		 * in it whose place, less At, runs From to To along Along. False: the whole front was held. */
		bool MassStretch(Vec& OutAt, Vec& OutAlong, double& OutFrom, double& OutTo) const;
		const std::vector<std::pair<double, Kind>>& Log() const { return CutLog; }
		/** Why the last shot was left (on the frame of a cut: why this cut). */
		Why LastWhy() const { return LeftWhy; }
		/** How much of a pose's picture is filled by something near the lens (World::Solid and the ground; 0 with no
		 * Solid), and whether its lens is clear of things and men, by the settings' rules. */
		double Blocked(const Pose& P) const { return BlockedShare(P.Eye, P.Look, P.Fov, P.Subject, 1.0); }
		/** How much of a pose's picture is screened short of what it looks at (ScreenShare, the settings' ScreenFar). */
		double Screened(const Pose& P) const { return ScreenShare(P.Eye, P.Look, P.Fov, P.Subject, 1.0); }
		bool Roomy(const Pose& P) const { return RoomAt(P.Eye, P.Subject); }
		/** Whether something happens in a pose's picture between two battle times, as the dead-shot rule counts it. */
		bool ActionSeen(const Pose& P, double From, double To) const { return ActionIn(P, From, To); }
		/** Whether a track at battle time T, Over metres over the ground at its middle (as ActionIn asks: 1.0 for a shot,
		 * 0.8 for a fall), is seen from Eye past everything but itself (Over: that height). */
		bool TrackSeen(const Vec& Eye, int Track, double T, double Over) const;
		/** The pose the director holds (after Begin: the opening's first; the scene places its eye there before the first
		 * frame is drawn). */
		const Pose& Current() const { return Now; }
		/** A heavy round's cloud's reach (Settings::CloudShell and the rest), and whether a point is inside a burst's cloud
		 * in its first CloudFor seconds at battle time T. */
		double CloudOf(const LandFeed::Round& R) const;
		bool InCloud(const Vec& At, double T) const;
		/** Whether a point is within Settings::FuelVehicle of a vehicle knocked out between two battle times (its fuel
		 * ball), and when the first went up. InCloud and LensBurst count these too. */
		bool InFuelBall(const Vec& At, double From, double To, double* OutWhen = nullptr) const;
		/** Whether a heavy round lands at a pose's lens between two battle times (Settings::BurstClear, CloudShell). */
		bool BurstAt(const Pose& P, double From, double To) const { return LensBurst(P.Eye, P.Look, P.Fov, From, To, P.Subject, nullptr); }
		/** How much of a pose's picture its own man fills (0 with no Solid or no man). */
		double OwnFill(const Pose& P) const { return OwnShare(P.Eye, P.Look, P.Fov, P.Subject); }
		/** Whether a track is a man of the film's own section (Settings::SquadEvery). */
		bool InSquad(int Track) const;
		/** Whether a track's block is hidden from the player now (World::Hidden). */
		bool HiddenNow(int Track) const;
		/** The film's own section: its block and section number (-1: none). */
		int SquadBlock() const { return HeroBlock; }
		int SquadK() const { return HeroK; }
		/** How near the lens its own man's head or chest is (metres; 1e9 with none). */
		double OwnGap(const Pose& P, double Battle) const { return OwnDistance(P.Eye, P.Subject, Battle); }
		/** Whether a man of the recording walks within CrossClear of a pose's lens between two battle times. */
		bool MenCross(const Pose& P, double From, double To) const;
		/** How much of a pose's picture leaves or a trunk fill within EdgeNear of the lens. */
		double EdgeFill(const Pose& P) const { return EdgeShare(P.Eye, P.Look, P.Fov, P.Subject); }
		/** Whether the shot's own man is out of its picture now, or down in the corn out of its sight. */
		bool Lost(double Battle) const { return OwnLost(Battle); }

	private:
		const LandFeed::Recording* Rec = nullptr;
		World Here;
		Settings Set;
		Pose Now;
		Kind Shot = Kind::Open;
		int Subject = -1;
		double ShotStarted = 0.0, ShotLength = 6.0;
		double BattleAtCut = 0.0;
		bool bCutAsked = false, bHeld = false;
		bool bAsked = false;
		Kind AskedKind = Kind::Wide;
		int AskedSubject = -1;
		bool bAskedBuilding = false;
		/** Where the eye stands relative to its subject for this shot (metres: behind, beside, up) and its own place
		 * for a still shot (Watch, Wide). */
		double Back = 1.0, Side = 0.5, Up = 1.5;
		Vec Stand, Target;
		Vec EyeEased, LookEased;
		double FacingEased = 0.0;
		/** The shallow focus as pulled (Pose::Focus): whether this shot has it, the distance held and its swing. */
		bool bFocusShot = false;
		double FocusEased = 0.0, FocusSwing = 0.0;
		double FocusWanted(double Battle) const;
		/** The subject fell during this shot (the real time he went down, -1 not): hold on him a while, then cut. */
		double FellAt = -1.0;
		/** How long this shot holds on its subject's fall or end (real s; Settings::FallHold, or a bail-out's hold). */
		double ShotHold = 4.0;
		/** The special shots (the first bail-out, the whistle, a nickname's first kill, a last stand): TrySp while a choice
		 * tries one, ShotSp the shot's (ShotShift: its look's step from its subject's body, behind a tank's middle or onto
		 * a last stand's men); HoldUntil, the real time a whistle's, a nickname's or a last stand's shot holds to, no cut
		 * but a rescue before it (-1 none). The first bail-out: bBailHold, it holds for her crew (BailOut said so);
		 * bBailDone, the battle's one is held. */
		Special TrySp = Special::None, ShotSp = Special::None;
		double HoldUntil = -1.0;
		bool bBailHold = false, bBailDone = false;
		Vec ShotShift;
		int BailWatchCount = 0;
		std::vector<BailBeat> BailLog;
		bool BailWanted(int Track) const;
		/** The whistle, a nickname's first kill, the last stand (Settings::bWhistle, World::Nickname, bLastStand): the
		 * special due now (SpecialDue: its man SpWho, block SpBlock, moment SpAt, end SpTo, its men SpMen, where they
		 * look from and at, SpAim and SpFoe); the choice's (ChoiceSp) and the shot's, held (HeldSp; CardBlock its card).
		 * bOpenNext: the opening aerial next, the film having opened on the whistle. */
		Special ChoiceSp = Special::None, HeldSp = Special::None;
		int SpWho = -1, SpBlock = -1, SpTarget = -1, CardBlock = -1;
		size_t SpRound = 0;
		double SpAt = -1.0, SpTo = -1.0;
		std::vector<int> SpMen;
		Vec SpAim, SpFoe;
		bool bOpenNext = false;
		std::vector<SpecialBeat> SpLog;
		bool AnySpecial() const { return Set.bWhistle || Set.bLastStand || static_cast<bool>(Here.Nickname); }
		Special SpecialDue(double Battle, double Real);
		/** The whistle: its step-off and officer (looked for until found), and whether it is done (taken or missed). */
		double WhistleT = -1.0, WhistleNextLook = -1.0;
		int WhistleTrack = -1;
		bool bWhistleDone = false;
		void FindWhistle(double Battle);
		/** The nicknames: each block's (by its place in the recording's blocks: -1 not asked, 0 none, 1 one), the kills of
		 * blocks with one (landing, round), read up to NickRead of the rounds; each such block's first kill's landing; the
		 * blocks whose card was shown or missed; how many shown, and when the last. */
		std::vector<signed char> NickHas;
		std::vector<std::pair<double, size_t>> NickKills;
		size_t NickRead = 0, NickFrom = 0;
		std::vector<double> NickFirst;
		std::vector<int> NickDone;
		/** Each block's nickname as World::Nickname gave it, and the names already carded (a name once a battle). */
		std::vector<std::string> NickName, NickShown;
		std::vector<size_t> NickTried;
		int NickCount = 0;
		double LastNickReal = -1e9;
		std::unordered_map<uint64_t, int> ManIndex;
		size_t ManIndexed = 0;
		int ManOf(int Block, int K, int J);
		bool FindNick(double Battle, double Real);
		/** The last stand: the sections weighed (each its men; built afresh as a live battle's tracks grow), the next look
		 * (battle time), the stand found (its men, look time, end) and whether the battle's one is done. */
		struct StandGroup
		{
			int Block = -1, Side = -1;
			std::vector<int> Men;
		};
		std::vector<StandGroup> StandGroups;
		size_t StandTracks = 0;
		double StandNextLook = -1.0, StandFoundAt = -1.0, StandTo = -1.0;
		int StandWho = -1, StandBlock = -1, StandFirst = -1;
		std::vector<int> StandMen;
		/** The groups (their first man) whose stand no place could show: not tried again; the third miss ends the look. */
		std::vector<int> StandTried;
		Vec StandAim, StandFoe;
		bool bStandDone = false;
		void BuildStandGroups();
		bool FindStand(double Battle);
		/** Battle seconds a real second (the scene plays the approach fast and the fight at life's pace, half the rules'
		 * clock): every rule about what the film shows in the next seconds asks Soon(real seconds) of the battle. */
		double Pace = 1.0;
		bool bPaced = false;
		/** How far the look is lifted with a lens held over corn (eased down when its man falls). */
		double LookLift = 0.0;
		/** The eye's step back from its own man (metres, level) and its swing: a stiff spring, not a shove. */
		Vec OwnPush, PushSwing;
		/** The battle time of a fall or a tank's end this shot holds for (-1 none): not left as dead or lost before it. */
		double WaitFor = -1.0;
		/** How far the eye is lifted over banks and corn (metres) and how fast: up on a spring, down over half a second. */
		double Lift = 0.0, LiftSwing = 0.0;
		/** The height the corn under the lens holds it at (world metres), let down on a spring as it leaves the corn;
		 * the look's lift with the corn on its own spring. */
		double CornHeld = -1e9, CornSwing = 0.0, LookLiftSwing = 0.0;
		/** The highest corn foreseen on the lens's way lately (world metres), let down at a metre a second. */
		double ForeHeld = -1e9;
		/** The film's own section (Settings::SquadEvery), -1 none, and the shots since one was on its men. */
		int HeroBlock = -1, HeroK = -1, SinceSquad = 0;
		/** Where the shots among the men stood (Settings::SpotOnce). */
		std::vector<Vec> UsedSpots;
		/** Where shots left for a dark picture stood (Settings::DarkClear), the latest 32; since when the picture has been
		 * dark, and since when plainly light (Settings::DarkLight; -1 not, either), the shots left for it in all and
		 * running, and whether the last asked shot was placed unfit. */
		std::vector<Vec> DarkSpots;
		double DarkSince = -1.0, LightSince = -1.0;
		int DarkLeft = 0, DarkRunNow = 0;
		bool bAskedUnfit = false;
		/** A cut's second, less choosy pass (no fit found in the middle of the battle). */
		bool bLoose = false;
		/** The whole mass (Settings::MassFrom): the shot placed is it, it has been taken, when. MassDue: whether a cut
		 * begun now goes to it (Coming: a tank's end or a fall the cut was going to, -1 none). PlaceMass: its place, the
		 * men drawn now and at its end weighed from the own side's rear and round it, nearest first. */
		bool bMassNow = false, bMassDone = false;
		double MassCutAt = -1.0;
		bool bStretch = false;
		Vec StretchAt, StretchAlong;
		double StretchFrom = 0.0, StretchTo = 0.0;
		/** The block ridden with (Ride), -1 none; whether a track is one of its men; whether any of them still stands for
		 * the film (up, on the field, not hidden) at a battle time. */
		int RideBlock = -1;
		bool OffRide(int Track) const { return RideBlock >= 0 && Rec->Tracks[static_cast<size_t>(Track)].Block != RideBlock; }
		/** Ride's own work, for the player's pick and the platoon cam's alike. */
		void RideWith(int Block);
		/** The platoon cam: the side followed (-1 none), the platoon ridden for the lead, when the marks are next read
		 * (every half second on the screen); the men with any lead marks, by track, kept up as a live battle grows. */
		int LeadSide = -1, LeadBlockNow = -1;
		double LeadLookAt = -1e9;
		std::vector<int> LeadMen;
		size_t LeadMenTracks = 0;
		/** Whether a man leads his side's assault at a battle time (his Lead spans); whether he is in the moving group. */
		bool InLead(size_t Track, double Battle) const;
		bool InRush(size_t Track, double Battle) const;
		void LookForLead(double Battle, double Real);
		/** The block the general last ordered (Lean), -1 none, and the real time the lean lapses (-1: not yet seen). */
		int LeanBlock = -1;
		double LeanUntil = -1.0;
		/** Radio call, then the barrage: the fresh calls for fire (IndexRadios, by their stamps), the next not yet passed,
		 * the one gone to (RadioStage 1: the cut to the caller asked; 2: on him, the barrage to come), the real time of
		 * the last; and whether the choice in hand is the radio's. */
		struct Radio
		{
			double T = 0.0, Land = 0.0, X = 0.0, Y = 0.0;
			int Caller = -1;
		};
		std::vector<Radio> Radios;
		std::vector<RadioBeat> Beats;
		size_t RadioNext = 0;
		int RadioStage = 0, RadioNow = -1;
		double LastRadioReal = -1e9;
		bool bRadioChoice = false, bRadiosFound = false;
		void IndexRadios();
		/** The men the barrage's cut may go to (Out, how many): the two of the side under fire nearest the salvo's middle
		 * (within 1.5 RadioNear), then the two of the caller's side nearest it, 40 to 600 m off. */
		int RadioMen(const Radio& Rd, double Battle, int Out[4]) const;
		/** The block a cut's weighing keeps to (riding or leaning), -1 none. */
		int ScanBlock = -1;
		/** The lists made from the recording (Index), kept up with a live one as it grows (KeepUp): the tracks and
		 * rounds put in, the vehicles not yet knocked out, each track's points weighed for MenFastest (Faster); and the
		 * film's own section chosen from the fight up to Until (ChooseHero), last looked at (battle time). */
		size_t KeptTracks = 0, KeptRounds = 0;
		std::vector<int> Vehicles;
		std::vector<size_t> FastSeen;
		double HeroLooked = -1.0;
		void Index();
		void Faster(size_t Track);
		void KeepUp(double Battle);
		void ChooseHero(double Until, double StandingAt);
		bool RideStands(double Battle) const;
		bool MassDue(double Battle, int Coming) const;
		bool PlaceMass(double Battle);
		/** The operator's nerves after a burst near the lens (0 to 1), and where they are going. */
		double Nerves = 0.0, NervesWant = 0.0;
		/** Heavy rounds landing within Radius of a point between two battle times. */
		int HeavyNear(double X, double Y, double From, double To, double Radius) const;
		double Soon(double RealSeconds) const { return RealSeconds * std::clamp(Pace, 0.1, 4.0); }
		double OnScreen(double BattleSeconds) const { return BattleSeconds / std::clamp(Pace, 0.1, 4.0); }
		uint64_t Dice = 0;
		double Phase[6] = { 0, 0, 0, 0, 0, 0 };
		int CutCount = 0;
		std::vector<std::pair<double, Kind>> CutLog;
		std::vector<Kind> Recent;
		/** The falls the director has already decided on (shown or passed over), so each is weighed once. */
		std::vector<int> Appointed;

		/** Why the coming cut is made (set before Choose). */
		Why LeftWhy = Why::Start;
		/** An event a moment ahead the next shot goes to (Update's cut on action): its track, -1 none. */
		int Pending = -1;
		/** The real time something last happened in the picture, and the battle time Update last saw. */
		double LastAction = 0.0, LastBattle = -1.0;
		/** The first round that may still be in the air at LastBattle (the rounds are in firing order). */
		size_t RoundFrom = 0;
		/** The last time the lens was looked round, when it was first found blocked (-1: it is not). */
		double NextLook = 0.0, BadSince = -1.0;
		/** The look's second half due this frame: where the picture will be and the men coming (the look in two frames). */
		bool bLookAhead = false;
		/** The shot on the screen was asked (the picture run's still): men run past it as near as Settings::AskCross. */
		bool bShotAsked = false;
		/** Whether the shot being chosen, or else the one on the screen, was asked. */
		bool AskedNow() const { return bChoosing ? bChoiceAsked : bShotAsked; }
		/** Cutting away from its own man come into the lens at once: the next shot is being chosen, the eye not shoved
		 * clear of him meanwhile (shoved, it jolted 0.43 m in the frame before the cut). */
		bool bShoveCut = false;
		/** When the picture was first found screened over ScreenMost, running (-1: it is not). */
		double ScreenedSince = -1.0;
		/** The closing aerial taken. */
		bool bClosed = false;
		/** The real time of the last cut to a man's fall (so they do not come one on another). */
		double LastFallCut = -100.0;
		/** A tracking shot's place off its tank's side (field metres, kept as she drives) and its height. */
		Vec TrackOff;
		double TrackUp = 2.0;
		/** A Watch on a fall about to come: near and at a man's height. */
		bool bNearWatch = false;
		/** Where the eye was when the subject fell, held there while he falls. */
		Vec FallEye;
		bool bFallEye = false;
		/** Each shooter's track by its block, model and man of his section ((block * 65536 + model) * 256 + j). */
		std::vector<std::pair<int64_t, int>> ShooterOf;
		/** The real time each track was last a shot's subject (the film goes round the tanks, and the men). */
		std::vector<double> ShownAt;
		/** The side of the last tank a shot was on. */
		int LastTankSide = -1;

		double Roll();
		double HeightAt(double X, double Y) const;
		int TrackOfShooter(int Block, int K, int J = 0) const;
		/** A tank or tracking shot whose tank is on the move now. */
		bool TankMoving(double Battle) const;
		bool BodyOf(int Track, double T, Vec& OutAt) const;
		/** The share of a picture (eye, look, lens) filled near the lens; Enough: stop counting once past it. */
		double BlockedShare(const Vec& Eye, const Vec& Look, double Fov, int Except, double Enough) const;
		/** Whether the lens is clear of things and men by the settings. */
		bool RoomAt(const Vec& Eye, int Except) const;
		/** The share of a picture screened short of what it looks at: a thing (a tree's crown or trunk, a hedge's shrubs,
		 * a house, a tank) anywhere out to ScreenFar (and short of the look), a man or the ground as BlockedShare counts
		 * them near the lens, and a bank's face (the ground met by a ray of the picture's upper half within BankNear). */
		double ScreenShare(const Vec& Eye, const Vec& Look, double Fov, int Except, double Enough) const;
		/** Whether the line from the eye to a point is clear of things (World::Solid; the last few metres round the point
		 * and the first round the lens left out). Except: the track the point is on (a tank's hull runs 3.2 m from her
		 * middle, so her own hull would hide her own shot), -1 none. */
		bool SeenFrom(const Vec& Eye, const Vec& At, int Except) const;
		/** Whether a pose (eye, look) would be a bad picture: the lens in or too near something, the picture filled near
		 * the lens, or screened past Limit. */
		bool BadPicture(const Vec& Eye, const Vec& Look, double Fov, double Limit) const;
		/** Where this shot's eye and look would be at battle time Battle (no easing, no hand), for a shot just placed. */
		void Intended(double Battle, Vec& OutEye, Vec& OutLook) const;
		/** Whether the shot just placed is a fit picture now (the lens clear, the picture open, the subject in sight), in
		 * parts so a try can be carried over frames: the cheap asks and its own man (part 0), the picture now (1), and for
		 * a shot that follows its subject the picture in 1.5 s (2) and 3 s (3). FitsParts: how many the shot has (none for
		 * the high views: always fit); it fits when every part does. */
		int FitsParts() const;
		bool FitsPart(double Battle, int Part) const;
		/** Something happening in a pose's picture between two battle times: a round fired or landing, a man falling. */
		bool ActionIn(const Pose& In, double From, double To) const;
		bool WhereIs(int Track, double T, Vec& OutAt, double& OutFacing) const;
		/** What is about to happen (rounds landing, men falling, tanks knocked out), from the recording: Gather picks
		 * out a stretch of the battle once a cut, HeatNear weighs it round a point. */
		struct Heat
		{
			double X = 0.0, Y = 0.0, T = 0.0, W = 0.0;
		};
		std::vector<Heat> Ahead;
		void Gather(double From, double To);
		/** The heat round a point between From and To within Radius, and in the same walk the heat between NearFrom and
		 * NearTo within NearRadius (OutNear; a window and a ring inside the first). OutSeen: the heats looked at. */
		double HeatNear(double X, double Y, double From, double To, double Radius, double NearFrom, double NearTo, double NearRadius, double& OutNear, long& OutSeen) const;
		/** The side a shot's first try stood on (+1 right, -1 left): the next try stands on the other. */
		double VariantSide = 1.0;
		/** A cut is chosen over a frame or a few (Settings::ChooseSteps a frame), the shot on the screen held meanwhile:
		 * BeginChoice weighs what to go to (the options), TryChoice tries them, as many as a frame's share allows (true:
		 * one placed, or the high view), FinishChoice makes the cut. */
		struct Option
		{
			Kind K = Kind::Wide;
			int Who = -1;
			bool bNear = false;
			Special Sp = Special::None;
		};
		std::vector<Option> Options;
		bool bChoosing = false, bChoiceAsked = false;
		int ChoiceComing = -1, ChoiceStage = 0, ChoiceV = 0, ChoiceTries = 0, ChoiceFrames = 0, ChoiceFramesMost = 0;
		size_t ChoiceO = 0;
		/** The side the choice's first try stood on (VariantSide), kept between its frames. */
		double ChoiceSide = 1.0;
		/** The ground asked under the camera's rays (HeightAt's calls, about a third of a microsecond each with what is
		 * asked at the same place): a cut's tries are counted by it. */
		mutable long RaySteps = 0;
		void BeginChoice(double Battle, double Real);
		bool TryChoice(double Battle, long Budget);
		void FinishChoice(double Battle, double Real);
		/** The weighing of the men and tanks for a cut, a frame's share at a time: ChoiceScan the next track, ChoiceTop the
		 * best four so far, and what the weighing was begun with (its battle time, whose turn it is). ScanChoice goes on
		 * with it (true: done, the options made). */
		struct Pick
		{
			double Score = -1.0;
			int Who = -1;
		};
		Pick ChoiceTop[4];
		size_t ChoiceScan = 0;
		bool bScanning = false, bScanOwnTurn = false, bScanTanksOnly = false, bScanSquadOnly = false, bScanFallsDue = false, bScanFighting = false, bScanRide = false;
		/** Weighing again away from the men no place fit on (Settings::Elsewhere): how many times this cut, where they were. */
		int ScanElsewhere = 0;
		std::vector<Vec> ScanAvoid;
		/** And men alone, when every one no place fit on was a tank (Sedan's tanks in the woods at 38 s, while a
		 * battalion ran up the road on foot). */
		bool bScanMenOnly = false;
		/** The houses on the field (a house's floor or a wall's line is no place for the eye at any height: World::Free),
		 * found by a 6 m search of the whole field at the first village ask, each by its points 2 m apart inside it. */
		struct House
		{
			Vec C;
			double R = 0.0;
			std::vector<Vec> Inside;
		};
		std::vector<House> Houses;
		bool bHousesFound = false;
		/** Whether a house is in a picture: HouseSeen of its points in it and seen from the eye, and big enough. */
		bool HouseSeenIn(const Vec& Eye, const Vec& Look, double Fov, int H) const;
		/** An asked shot's men to try (AskOthers): each with the house (and the point of it nearest him) it is framed
		 * against for the village shot, -1 otherwise. The first is the one the asked option is on. */
		struct AskTry
		{
			int Track = -1;
			int House = -1;
			Vec Aim;
		};
		std::vector<AskTry> AskTries;
		size_t AskNext = 1;
		int AskNextV = 0;
		void MakeAskTries(double Battle);
		/** The house of the place being tried and of the shot placed (-1: none), and the point of it the look turns to. */
		int TryHouse = -1, ShotHouse = -1;
		Vec TryAim, ShotAim;
		/** The barrage's cut (radio call, then the barrage): framed as the village still is, the salvo's middle (TryAim)
		 * beyond the man, so it lands in the picture round him. */
		bool bTrySalvo = false, bShotSalvo = false;
		/** The salvo's landing (battle time) the barrage's cut is framed for; the share of the way the look turns from
		 * the man to the aim (the house: Settings::HouseLook; the salvo: half). */
		double SalvoLand = -1.0;
		double AimLook() const { return bShotSalvo ? 0.5 : Set.HouseLook; }
		int AskAgain = 0;
		std::vector<Vec> AskLeft;
		/** The choice begun in the middle of the battle (not asked, past the first seconds, before the end): the looser
		 * tries before the high view. */
		bool bChoiceMiddle = false;
		double ChoiceBattle = 0.0, ChoiceReal = 0.0;
		bool ScanChoice(long Start, long Budget);
		/** The frame's share of a choice spent: its steps (Budget past Start), or the clock (Settings::ChooseMs since the
		 * frame's Update began), never on a choice made at once. */
		std::chrono::steady_clock::time_point FrameClock;
		bool ShareSpent(long Start, long Budget) const;
		void ChoiceOptions(double Battle);
		/** The shot on the screen, kept while a choice's tries place others in its fields, and put back. */
		struct ShotKept
		{
			Kind Shot = Kind::Open;
			int Subject = -1;
			double FellAt = -1.0, Fov = 50.0, FacingEased = 0.0, VariantSide = 1.0, Back = 1.0, Side = 0.5, Up = 1.5, UpJitter = 0.0, TrackUp = 2.0;
			bool bFallEye = false, bNearWatch = false, bLoose = false, bMass = false;
			Vec Stand, Target, TrackOff;
			int House = -1;
			Vec Aim;
			bool bSalvo = false;
			Special Sp = Special::None;
			bool bBailHold = false;
			double Hold = 4.0, HoldUntil = -1.0;
			Vec Shift;
		};
		ShotKept KeepShot() const;
		void PutShot(const ShotKept& K);
		/** A try of a place, its parts (FitsPart) carried over frames: bTrying while one is under way, TryPart its next
		 * part, TryBattle when it was placed, TryShot the place. TryPlace: 1 it fits (placed), 0 it does not, -1 the
		 * frame's share spent before its parts were done. */
		bool bTrying = false;
		int TryPart = 0;
		double TryBattle = 0.0;
		ShotKept TryShot;
		int TryPlace(Kind K, int Who, double Battle, int Variant, long Start, long Budget);
		/** Places a shot of a kind on a track; Variant 0, 1, 2 ... asks another side, distance or height. */
		bool Place(Kind K, int Track, double Battle, int Variant = 0);
		Vec KeepAbove(Vec At, double Least) const;
		/** KeepAbove, and the ground rising on the way toward Toward met early, up a slope of Settings::Climb. */
		Vec KeepAboveAhead(Vec At, const Vec& Toward, double Least) const;
		/** How high the corn stands round a place (World::Crop), 0 where none or none known. */
		double CropAt(double X, double Y) const;

		// The camera director's rules (Settings: OwnClear and on).
		/** A shoulder shot's height over its man's stance (the operator stands, kneels or lies down with him). */
		double UpJitter = 0.0;
		double ShoulderUp(double Battle) const;
		/** The rounds that burst (shells, mortar bombs, tank and anti-tank rounds) by the time they land. */
		std::vector<int> Heavy;
		/** The tanks and lorries knocked out (their fuel goes up: Settings::FuelVehicle), by the time they are. */
		std::vector<int> Burnt;
		/** The real time to cut on the flash of a shell landing at the lens in this shot, -1 none. */
		double BurstCutAt = -1.0;
		/** The look's swing (metres a second at the look point): a spring, so a pan starts and stops without a kick. */
		Vec LookSwing;
		/** The real time its own man was first found out of the picture or hidden, -1 not. */
		double LostSince = -1.0;
		/** A round that bursts landing at a lens (eye, look, lens) between two battle times, not one striking its own man
		 * or tank (Own): whether one does, and when the first lands. */
		bool LensBurst(const Vec& Eye, const Vec& Look, double Fov, double From, double To, int Own, double* OutWhen) const;
		/** The share of a picture its own man fills. */
		double OwnShare(const Vec& Eye, const Vec& Look, double Fov, int Own) const;
		/** Where a man's head and chest are at a battle time (the head from his chest and his stance). */
		bool HeadAndChest(int Own, double Battle, Vec& OutHead, Vec& OutChest) const;
		/** How near the lens its own man's head or chest is at a battle time. */
		double OwnDistance(const Vec& Eye, int Own, double Battle) const;
		/** An eye put back to OwnClear from its own man's head and chest (a man, not a tank). */
		Vec ClearOfOwn(Vec Eye, double Battle, double Clear = -1.0) const;
		/** How high over the ground ahead a shot near the men looks (a shoulder shot from its height, low, on a tank). */
		double LookOver(double UpNow) const;
		/** Whether a man (not Own) walks within CrossClear of an eye at N times (the eye where it will be then), read from
		 * the recording. */
		bool Crossing(const Vec* Eyes, const double* Times, int N, int Own, double Clear = -1.0) const;
		/** Whether a man within Near of a held lens at T is only running past it: within Near no more than
		 * Settings::AskPass on the screen, and never within Settings::AskCross. */
		bool Passes(const LandFeed::Track& Tr, const Vec& Eye, double T, double Near) const;
		/** How many men up (Enough at most) stand in a picture within Within metres and are seen from its eye. */
		int MenInPicture(const Pose& P, double Battle, double Within, int Enough, int Except) const;
		/** The last real time something moved in the picture before the armies met, and when it is next looked for. */
		double LastMoved = -1.0, NextMoveLook = 0.0;
		/** The share of a picture filled by a thing within EdgeNear of the lens (leaves, a trunk; not men, not ground). */
		double EdgeShare(const Vec& Eye, const Vec& Look, double Fov, int Except) const;
		/** Whether its own man is out of the picture or down in corn taller than he lies, seen from the eye. */
		bool OwnLost(double Battle) const;
		bool OwnLostFrom(const Pose& From, double Battle) const;
		/** The same of another man of the recording (a last stand's men). */
		bool ManLostFrom(const Pose& From, double Battle, int Who) const;
		/** Whether anything moves in the picture now (before the armies meet: the advance is the action). */
		bool MovingIn(double Battle) const;

		// Indexes, so the film's questions cost by what is near, not by every man on the field (the battles of some 8,000
		// men: the production lead's plan, 22 November). Each gives the same answers as a walk over every man or round.
		/** Each track's block (an index into the recording's blocks, -1 none), kept while the counts stand. */
		mutable std::vector<int> BlockIx;
		mutable size_t BlockIxTracks = 0, BlockIxBlocks = 0;
		const LandFeed::Block* BlockOfTrack(int Track) const;
		/** Each track's kind by bits (KindTank, KindGun, KindLorry), kept while the counts stand: the scans over every
		 * track compare no names (CD0: at Kursk the first frame of a cut's choice went mostly on 5,800 tracks' names,
		 * blocks and moves asked before the side was). */
		static constexpr uint8_t KindTank = 1, KindGun = 2, KindLorry = 4;
		mutable std::vector<uint8_t> TrackKinds;
		uint8_t KindOf(size_t Track) const;
		bool IsVehicleAt(size_t Track) const { return KindOf(Track) != 0; }
		/** Whether each block is hidden from the player, asked once a frame (FrameStamp) of the scene's Hidden. */
		mutable std::vector<uint8_t> HiddenVal;
		mutable std::vector<uint32_t> HiddenStamp;
		uint32_t FrameStamp = 1;
		/** The rounds Gather takes, by their place in the recording. */
		std::vector<int> GatherIx;
		/** The men where they stood at a battle time (MenT), by 16 m squares in a hashed table of chains; Crossing asks
		 * the squares round its eye, with room for how far the fastest man of the recording goes from MenT. */
		struct ManAt
		{
			int Track = -1;
			double X = 0.0, Y = 0.0;
		};
		mutable std::vector<ManAt> MenAt;
		mutable std::vector<int32_t> MenHead, MenNext;
		mutable std::vector<uint32_t> MenUsed;
		mutable double MenT = -1e18;
		mutable size_t MenTracks = 0;
		/** The fastest any man of the recording goes, metres a battle second (from his track's points). */
		double MenFastest = 0.0;
		void PlaceMen(double T) const;
		/** The rounds by when they land (indices into the recording's rounds), kept as the rounds grow. */
		std::vector<int> ByLanding;
		size_t ByLandingRounds = 0;
		void IndexLandings();
		/** Ahead's heat by 35 m squares (a hashed table of chains, each heat in its own square once), for HeatNear. */
		std::vector<int32_t> HeatHead, HeatNext;
		std::vector<uint32_t> HeatUsed;
		/** HeatNear's marks: a bit for each heat of Ahead, set as the squares are walked, cleared as they are summed. */
		mutable std::vector<uint64_t> HeatBits;
	};
}
