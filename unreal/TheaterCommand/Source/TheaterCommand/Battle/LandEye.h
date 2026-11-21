#pragma once

#include "Battle/LandFeed.h"
#include "Battle/LandPlay.h"

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
 *  - Wide: 40 to 70 m up behind a side's line, across the field: where everyone is. Only to close the battle.
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
	 * or a picture asked for, its subject gone from the field. */
	enum class Why : unsigned char { Start, Opening, Length, Fall, Action, Dead, Rescue, Asked, Gone };
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
		/** The hand-held wobble's strength (0 off, 1 as made). */
		double Hand = 1.0;
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
		 * asks where the eye will be AheadLook battle seconds on, so a shot is left before it drives into a hedge. */
		double ScreenFar = 30.0, ScreenMost = 0.3, ScreenLeave = 0.45, BankNear = 6.0, AheadLook = 0.8;
		/** The shortest a shot holds, real seconds (only a rescue cuts sooner). */
		double ShotLeast = 3.0;
		/** Once the armies are near, a shot in which nothing has happened for this long (real seconds) is left. */
		double DeadAfter = 4.0;
		/** How long a fall is held after the man is hit: his fall to the ground and a moment where he lies (battle s). */
		double FallHold = 4.0;
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

		// For the test.
		int Cuts() const { return CutCount; }
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
		/** The subject fell during this shot: hold on him a while, then cut. */
		double FellAt = -1.0;
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
		/** Whether the shot just placed is a fit picture now: the lens clear, the picture open, the subject in sight. */
		bool Fits(double Battle) const;
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
		double HeatNear(double X, double Y, double From, double To, double Radius) const;
		/** The side a shot's first try stood on (+1 right, -1 left): the next try stands on the other. */
		double VariantSide = 1.0;
		void Choose(double Battle, double Real);
		/** Places a shot of a kind on a track; Variant 0, 1, 2 ... asks another side, distance or height. */
		bool Place(Kind K, int Track, double Battle, int Variant = 0);
		Vec KeepAbove(Vec At, double Least) const;
		/** How high the corn stands round a place (World::Crop), 0 where none or none known. */
		double CropAt(double X, double Y) const;
		/** How far over the ground the corn holds a lens near the ground now (metres; 0 none): up at once as the eye
		 * comes over taller corn, down slowly as it leaves it, so it does not bob over every clump. */
		double CropHold = 0.0;
	};
}
