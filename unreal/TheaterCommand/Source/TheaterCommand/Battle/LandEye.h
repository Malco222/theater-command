#pragma once

#include "Battle/LandFeed.h"
#include "Battle/LandPlay.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
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
	const char* NameOf(Why W);

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
	};

	/** Whether a point is in a pose's picture (its vertical lens, the picture Aspect wide for 1 high), with a margin. */
	bool InPicture(const Pose& P, const Vec& At, double Margin = 1.0, double Aspect = 16.0 / 9.0);

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
		/** Once the armies are near, a shot in which nothing has happened for this long (real seconds) is left. */
		double DeadAfter = 4.0;
		/** How long a fall is held after the man is hit: his fall to the ground and a moment where he lies (real s). */
		double FallHold = 4.0;
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
		 * running the light is to blame, not the place: the rule rests until a shot holds. */
		double DarkLeave = 0.30, DarkHold = 0.5, DarkClear = 6.0;
		int DarkRunMost = 2;
		/** The whole mass (the production lead's plan for battles of thousands, 22 November: CD1): once a battle, between
		 * MassFrom and MassTo battle seconds (in a battle too short for them, between 55% and 72% of it), one wide shot
		 * high over the own side's rear holding MassShare of the men drawn in its picture (every man on the field the
		 * player can see: the batteries far back and the fallen too) from its first frame to its last, MassLength real
		 * seconds. Its eye no more than MassHighest metres over the men. The field is its subject: it is not left for a
		 * tank's end or a man's fall, and it waits for a moment with no tank about to be knocked out in its time unless the
		 * window is closing. MassFrom 0 or less: none. */
		double MassFrom = 150.0, MassTo = 200.0, MassShare = 0.85, MassLength = 7.0, MassHighest = 500.0;
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
		/** A shot of a kind on a subject asked for (the picture run). Subject -1: the director's choice. */
		void Ask(Kind K, int Subject = -1);
		/** Ride with this unit (Brian's yes, 22 November 03:36Z): the film stays with one block's men or tanks (its id as
		 * the recording numbers it), cutting among them by the same rules as among the field's: their falls and a tank's
		 * end are gone to, no one else's. It cuts to them at once unless the shot on the screen is already on one of them.
		 * Let go with -1; it lets go of itself when none of them is left standing or they are hidden from the player
		 * (Riding() then -1). No wide of the whole mass while riding. */
		void Ride(int Block);
		int Riding() const { return RideBlock; }

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
		/** The ground asked under the camera's rays so far (the measure of its work a frame, the same on any machine). */
		long Steps() const { return RaySteps; }
		/** Whether the shot on the screen is the whole mass (Settings::MassFrom), and the battle time it was cut to (-1:
		 * not yet); the window it is due in, for this recording. */
		bool MassShot() const { return bMassNow && Shot == Kind::Wide; }
		double MassAt() const { return MassCutAt; }
		void MassWindow(double& OutFrom, double& OutTo) const;
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
		/** Where the eye stands relative to its subject for this shot (metres: behind, beside, up) and its own place
		 * for a still shot (Watch, Wide). */
		double Back = 1.0, Side = 0.5, Up = 1.5;
		Vec Stand, Target;
		Vec EyeEased, LookEased;
		double FacingEased = 0.0;
		/** The subject fell during this shot (the real time he went down, -1 not): hold on him a while, then cut. */
		double FellAt = -1.0;
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
		 * dark (-1 not), the shots left for it in all and running, and whether the last asked shot was placed unfit. */
		std::vector<Vec> DarkSpots;
		double DarkSince = -1.0;
		int DarkLeft = 0, DarkRunNow = 0;
		bool bAskedUnfit = false;
		/** A cut's second, less choosy pass (no fit found in the middle of the battle). */
		bool bLoose = false;
		/** The whole mass (Settings::MassFrom): the shot placed is it, it has been taken, when. MassDue: whether a cut
		 * begun now goes to it (Coming: a tank's end or a fall the cut was going to, -1 none). PlaceMass: its place, the
		 * men drawn now and at its end weighed from the own side's rear and round it, nearest first. */
		bool bMassNow = false, bMassDone = false;
		double MassCutAt = -1.0;
		/** The block ridden with (Ride), -1 none; whether a track is one of its men; whether any of them still stands for
		 * the film (up, on the field, not hidden) at a battle time. */
		int RideBlock = -1;
		bool OffRide(int Track) const { return RideBlock >= 0 && Rec->Tracks[static_cast<size_t>(Track)].Block != RideBlock; }
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
		/** The choice begun in the middle of the battle (not asked, past the first seconds, before the end): the looser
		 * tries before the high view. */
		bool bChoiceMiddle = false;
		double ChoiceBattle = 0.0, ChoiceReal = 0.0;
		bool ScanChoice(long Start, long Budget);
		void ChoiceOptions(double Battle);
		/** The shot on the screen, kept while a choice's tries place others in its fields, and put back. */
		struct ShotKept
		{
			Kind Shot = Kind::Open;
			int Subject = -1;
			double FellAt = -1.0, Fov = 50.0, FacingEased = 0.0, VariantSide = 1.0, Back = 1.0, Side = 0.5, Up = 1.5, UpJitter = 0.0, TrackUp = 2.0;
			bool bFallEye = false, bNearWatch = false, bLoose = false, bMass = false;
			Vec Stand, Target, TrackOff;
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
		bool Crossing(const Vec* Eyes, const double* Times, int N, int Own) const;
		/** The last real time something moved in the picture before the armies met, and when it is next looked for. */
		double LastMoved = -1.0, NextMoveLook = 0.0;
		/** The share of a picture filled by a thing within EdgeNear of the lens (leaves, a trunk; not men, not ground). */
		double EdgeShare(const Vec& Eye, const Vec& Look, double Fov, int Except) const;
		/** Whether its own man is out of the picture or down in corn taller than he lies, seen from the eye. */
		bool OwnLost(double Battle) const;
		bool OwnLostFrom(const Pose& From, double Battle) const;
		/** Whether anything moves in the picture now (before the armies meet: the advance is the action). */
		bool MovingIn(double Battle) const;

		// Indexes, so the film's questions cost by what is near, not by every man on the field (the battles of some 8,000
		// men: the production lead's plan, 22 November). Each gives the same answers as a walk over every man or round.
		/** Each track's block (an index into the recording's blocks, -1 none), kept while the counts stand. */
		mutable std::vector<int> BlockIx;
		mutable size_t BlockIxTracks = 0, BlockIxBlocks = 0;
		const LandFeed::Block* BlockOfTrack(int Track) const;
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
