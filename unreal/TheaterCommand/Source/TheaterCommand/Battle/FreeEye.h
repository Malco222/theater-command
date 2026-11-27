#pragma once

#include <cmath>

/**
 * The free camera ("god mode") of the air and sea scenes (Brian, 26 November 2026, release 2): "i want the camera angle
 * to be like god mode where i can see everything playout like i was on a free camera mode where i can move where every
 * i want", "meaning being able to float anywhere and things are going on around you", and "if i click on a squadron
 * then it locks on them and it will follow them" / "or i can skip to individual planes". The ground battle keeps its
 * own view (LandGeneralEye); this one is for the sky and the sea, where nothing stands in the way but the ground or
 * the drawn waves under the eye. Plain C++ and the standard library only, so the cloud test flies it with scripted keys
 * and anchors and checks every rule below; the scenes wire it themselves, behind a switch.
 *
 * How it moves: a boom about a pivot. The pivot is the point the camera looks at, anywhere in space; the eye stands at
 * the end of a boom of heading H, tilt T and length Arm behind it:
 *   Eye = Pivot - F * (Arm * cos T) + (0, 0, Arm * sin T), F = (sin H, cos H, 0).
 * The zoom S (0..1) is the boom's length on a log scale, from ArmNear (about a metre: a drone, whose pan flies it level
 * and whose turn and tilt look about) to ArmFar (the whole battle under it).
 *
 * The pivot is Ref + Rel. Ref, the reference, is what the scene feeds every frame: the thing locked on (a squadron's
 * middle, one aircraft, a ship) while locked, and else the frame (the raid's moving middle, so that a camera let go in
 * an air battle is not left behind at a hundred metres a second; a still origin at sea). Rel is his own offset from
 * it, moved by his pan and rise. The camera rides the reference rigidly: Ref's movement passes straight into the
 * pivot, every frame, so nothing lags at any battle pace, and paused (Ref still) he flies the camera all the same.
 * Update runs on real seconds and never needs the battle's clock.
 *
 * Why it is eased the way it is: every move by his hand goes through a critically damped spring stepped exactly (no
 * overshoot, the same curve at any frame rate), and the turn, the tilt, the zoom, his offset and the floor's lift are
 * also held to a fastest speed and a hardest push, so a key held starts and stops softly and a slide or a pinch is
 * never a jump. The picture is held to a kick of 1,200 degrees a second squared: the look's own turning, and the eye's
 * acceleration seen from max(Arm, FeelArm) away (|accel| / max(Arm, FeelArm)), measured against the reference's own
 * movement. When he locks on, skips to another aircraft or lets go, the view is carried: where the pivot is and how
 * fast it goes in the world are kept across the change (its speed the pivot's own over the last frame), and it then
 * glides onto the new reference; let go, it comes to rest where that speed braked at the glide's push takes it, and
 * never turns back. Only a change too far to glide (CutBeyond, GlideLongest, or an eye whose glide would pass inside
 * the anchor's clearance) or a thing that jumped (TeleportBeyond) is a cut, and Pose.bCut says so on that one frame.
 *
 * The caps (tilt, boom, the ceiling EyeMost, the pivot's box, the anchor's clearance) are put on what he WANTS, so the
 * springs come to rest at a cap softly; a hard stop on the frame itself is only the last guard, for a cap that moved
 * (a ship heaving, a box sliding, a ceiling the reference's steadied heave carries the eye against: centimetres) or a
 * cut placed against one. One more, small: his offset's spring coming in on a ceiling-held height while it also moves
 * sideways (a pan with a rise) can run a centimetre or two past it, which the ceiling then stops (in twenty minutes
 * of random hands, none to a dozen frames, at most some 3 cm). Where two caps cannot both hold, the higher wins: the
 * floor, then the ceiling, then the anchor's clearance, then the box. The anchor's clearance is kept by the boom's
 * length: where the eye's way from where it is to where it comes to rest would pass nearer the anchor than
 * AnchorClear (a short boom gliding onto a new ship), the boom's goal is made just long enough, and comes back in to
 * his own zoom as the pivot arrives (pushed off her every frame instead, the eye would slide round her with a jolt).
 * Two places the hard rules act most, by the geometry: a long boom swung under the floor (its end flung at the ground
 * faster than the lift climbs: put clear on a few frames), and looking straight up at a ship from the water (under
 * her the eye is either in the sea or inside her clearance, and her heave moves that cap: some hundreds of frames in
 * twenty seconds); the picture is held to the kick there all the same.
 *
 * Headings as everywhere in the cores: radians, 0 north, clockwise seen from above. Axes: X east, Y north, Z up,
 * metres. Tilt + looks down on the pivot, - looks up at it from below. The same inputs give the same outputs bit for
 * bit: no clocks, no random numbers, no globals.
 */
namespace FreeEye
{
	struct Vec
	{
		double X = 0.0, Y = 0.0, Z = 0.0;

		Vec operator+(const Vec& B) const { return Vec{ X + B.X, Y + B.Y, Z + B.Z }; }
		Vec operator-(const Vec& B) const { return Vec{ X - B.X, Y - B.Y, Z - B.Z }; }
		Vec operator*(double K) const { return Vec{ X * K, Y * K, Z * K }; }
		Vec& operator+=(const Vec& B) { X += B.X; Y += B.Y; Z += B.Z; return *this; }
		Vec& operator-=(const Vec& B) { X -= B.X; Y -= B.Y; Z -= B.Z; return *this; }
		double Length() const { return std::sqrt(X * X + Y * Y + Z * Z); }
		Vec Unit() const
		{
			const double L = Length();
			return L > 0.0 ? Vec{ X / L, Y / L, Z / L } : Vec{};
		}
		bool Finite() const { return std::isfinite(X) && std::isfinite(Y) && std::isfinite(Z); }
	};

	/** What the scene knows of the world that the eye must respect. */
	struct World
	{
		virtual ~World() = default;
		/** The lowest the EYE may stand at (X, Y): the ground or the drawn sea there, WITHOUT the margin
		 * (Settings::FloorClear is added by the core). A wave that moves is asked again every frame. */
		virtual double Floor(double X, double Y) const = 0;
	};

	/** Every number the view keeps to. The scenes set their own per battle kind (FREE-EYE.md); these are the defaults.
	 * Angles in degrees here (the core turns them into radians), lengths in metres, times in real seconds. */
	struct Settings
	{
		/** The boom's shortest and longest: the drone's metre (closer would look through a wing or a funnel) and the
		 * whole battle in the picture. */
		double ArmNear = 1.0, ArmFar = 4000.0;
		/** A wheel notch or a pinch's step, as a share of the whole zoom: some fifty notches across the range. */
		double ZoomStep = 0.06;
		/** The tilt's range: from 80 degrees up at the pivot (an aircraft overhead, seen from under) to one short of
		 * straight down (never quite, so the heading keeps its meaning). */
		double TiltLeast = -80.0, TiltMost = 89.0;
		/** The eye never higher than this over the world's zero (the sky's drawn ceiling, the sea's haze). */
		double EyeMost = 12000.0;
		/** The eye wants to stand FloorClear over the floor (World::Floor) and never stands under FloorHard over it, on
		 * any frame, a wave included; FloorFlat is the floor when the scene gives no World. */
		double FloorClear = 3.0, FloorHard = 0.5, FloorFlat = 0.0;
		/** How fast the springs catch up (a critically damped spring's rate, 1/s): his offset (pan, rise, slide, and
		 * the glide onto a new reference), the zoom, the turn and the tilt. */
		double Ease = 8.0, ZoomEase = 8.0, TurnEase = 10.0, TiltEase = 8.0;
		/** The turn's and the tilt's fastest swing and hardest push, degrees a second and a second squared. */
		double TurnFastest = 140.0, TurnPush = 500.0, TiltFastest = 70.0, TiltPush = 260.0;
		/** The zoom's fastest swing and hardest push, shares of the whole zoom a second and a second squared. The boom's
		 * length is on a log scale of ln(4000) = 8.3 across the zoom, so 0.5 a second (the first asked) is a boom
		 * growing 63 times a second, the eye flung back at 1,750 deg/s/s of its own distance; 0.25 and 0.8 hold the
		 * zoom alone to about 620 (the whole range in some five seconds). */
		double ZoomFastest = 0.25, ZoomPush = 0.8;
		/** A key's pan and rise: this share of the boom's length a second (at least PanArmLeast metres of boom), so a far
		 * view crosses the battle and a near one creeps. */
		double PanRate = 0.9, RiseRate = 0.6, PanArmLeast = 8.0;
		/** The eye's acceleration is felt from max(Arm, FeelArm): the drone's metre-long boom is not taken as a picture
		 * swung about a metre away (it shows things tens of metres off). */
		double FeelArm = 10.0;
		/** His offset's fastest and hardest (and so the glide onto a new reference's), shares of max(Arm, FeelArm) a
		 * second and a second squared: 172 and 344 degrees of the picture. */
		double GlideFastest = 3.0, GlidePush = 6.0;
		/** A lock whose pivot would come to rest further than this from where it is now is a cut, not a glide (a glide
		 * across the whole battle tells him nothing); the same anchor moved further than TeleportBeyond (plus three
		 * frames of its own speed) in one frame jumped, and the view cuts with it; the frame stepped as far beyond its own
		 * movement (or, on the first frame after Begin, that far from Begin's) is a new frame, carried, as a new FrameId
		 * is. Metres. */
		double CutBeyond = 3000.0, TeleportBeyond = 200.0;
		/** A lock whose glide would take longer than this, seconds, is a cut too (added to the specification's numbers):
		 * the glide is held to max(Arm, FeelArm), so on the drone's short boom a lock 3 km off, or on an aircraft some
		 * hundreds of metres a second faster than the view, would crawl onto it for a minute or more. */
		double GlideLongest = 15.0;
		/** An anchor whose velocity is not fed, and the frame, are measured from their own movement (the last seen,
		 * kept through a pause), and the battle's pace may change on any frame (paused, or 0.5, to 10 on a hitch): a step
		 * along that way of up to this many times that speed (plus TeleportBeyond) is taken as its own movement, not a
		 * jump (a step across its way, or back, is judged by rule 6 as it stands). Added to the specification's numbers:
		 * 10 over 0.5. */
		double PaceJump = 20.0;
		/** Air and sea: his pan and rise move him about inside the anchor's moving frame and the lock holds (he drifts
		 * through the bomber stream; he sits off a ship's bow as she steams). False (a ground-style scene): a pan lets go
		 * first. Zoom, turn and tilt never let go. */
		bool bPanKeepsLock = true;
		/** While locked, the anchor's height followed through a low-pass of this time constant, seconds, so a ship's
		 * heave does not bob the picture; 0 off. X and Y are always followed exactly. A first-order low-pass: of an 8 s
		 * heave, 3 s passes 39% (6 s: 21%, 10 s: 13%). */
		double SteadyZ = 0.0;
		/** The VERTICAL lens, degrees, at the nearest zoom and the farthest. */
		double FovNear = 44.0, FovFar = 50.0;
		/** Pose.Look stands at least this far from the eye, so a scene that builds its rotation from Look - Eye has a
		 * long vector even on the drone's short boom. Metres. */
		double LookLeast = 10.0;
		/** The floor's lift (the whole view, pivot and eye together, carried up as a crane would, so the picture is
		 * lifted and never tipped): its spring's rate going up and settling back down (fast up, slow down), and its
		 * hardest push, a share of max(Arm, FeelArm) a second squared (344 degrees of the picture). LiftPush is added to
		 * the specification's numbers: an unheld lift met a 12 m swell with a jolt of some 28,000 deg/s/s. A goal that
		 * climbs is led by the spring's own lag at that rate (no more than FloorClear), so a crest rising under him at
		 * 4 m/s is met at FloorClear and not a metre under it. His own descent (Rise) is held where the eye he wants
		 * stands FloorClear over the floor under it, so it comes to rest there softly instead of the lift chasing it. */
		double LiftRise = 12.0, LiftSettle = 3.0, LiftPush = 6.0;
		/** Where the lift looks for the floor: round the eye this far, metres, and ahead along the pivot's movement this
		 * long, seconds, so a crest coming on or a rise the view flies at is met before it is under the eye. Added to the
		 * specification's numbers, for the same reason. */
		double FloorReach = 25.0, FloorAhead = 1.0;
	};

	/** What the player and the scene give this frame. Numbers not finite are taken as 0 (Aspect as 1.6); a vector not
	 * finite is left out (the last good one is kept; a locked anchor's and the box's carried on at their last speeds). */
	struct Input
	{
		/** Keys held, -1..1: the pivot slides right and ahead of the view's heading, level, at PanRate times the boom
		 * a second; and goes up or down at RiseRate times the boom a second. */
		double PanRight = 0.0, PanAhead = 0.0, Rise = 0.0;
		/** This frame's two-finger slide, in picture widths and heights, the way the scene wants the VIEW to go: the
		 * pivot moves by that share of what the picture shows at the pivot's distance. */
		double SlideRight = 0.0, SlideAhead = 0.0;
		/** The picture's width over its height, for the slide (not finite or under 0.1 taken as 1.6, over 10 as 10). */
		double Aspect = 1.6;
		/** Wheel notches or pinch steps this frame, + in. */
		double Zoom = 0.0;
		/** Degrees this frame: + turns the view clockwise seen from above; + tilts it further down. */
		double Turn = 0.0, Tilt = 0.0;
		/** The thing to lock on, fed EVERY frame while locked. A rising bLock, or a new AnchorId while it is held, locks
		 * on (a new id is a skip to another aircraft); the same id far from where it was is a teleport. AnchorVel is in
		 * metres a REAL second as the scene moves it this frame (battle pace already in it, zero when paused); with
		 * bAnchorVel false the core measures it from the anchor's own movement. AnchorHeading (radians) is used with
		 * bRide: the view's heading turns with the anchor's, so a ship's bow stays where it was in the picture, and where
		 * he wants to sit off her turns with her (his offset follows that on its spring, so a glide onto her mid-turn is
		 * not flung sideways). */
		bool bLock = false;
		int AnchorId = -1;
		Vec Anchor;
		bool bAnchorVel = false;
		Vec AnchorVel;
		double AnchorHeading = 0.0;
		bool bRide = false;
		/** The reference while nothing is locked, fed every frame; a new FrameId is a new frame (carried, not cut). The
		 * scene gives a new FrameId whenever the frame shifts rather than moves (a group joining the raid moves its
		 * middle): a step with the same id within TeleportBeyond of the frame's own movement (see PaceJump) is ridden
		 * as that movement. */
		Vec Frame;
		int FrameId = 0;
		/** Let go of the lock (the scene may also just stop sending bLock). After a let go the lock takes again only on
		 * a new rise of bLock or a new AnchorId. */
		bool bLetGo = false;
		/** Where the PIVOT may be, world metres; it may move each frame (a corner not finite: the last good box, moved on
		 * at its walls' last speeds). */
		bool bBox = false;
		Vec BoxMin, BoxMax;
		/** While locked, the eye never nearer than this to the anchor point (not inside the aircraft or the ship), metres;
		 * 0 no rule. */
		double AnchorClear = 0.0;
	};

	struct Pose
	{
		Vec Eye, Look;
		/** The vertical lens, degrees. */
		double Fov = 50.0;
		/** The picture jumped this frame (a lock too far to glide, a teleport): the scene tells its camera it is a cut,
		 * so motion blur and temporal effects do not smear across it. */
		bool bCut = false;
	};

	class Eye
	{
	public:
		Settings Set;

		/** From a camera as it stands (the director's shot when he takes the free camera): heading and tilt from the
		 * direction, the boom the distance (held to its range), the pivot on the line of sight; the first frame is the
		 * shot exactly (a tilt past its range, straight down or up, eases into the range from the next), whatever this
		 * eye did before. Only a shot under FloorHard over the floor or over EyeMost is moved (lifted clear, or let down
		 * under the ceiling) on that first frame, which then says it is a cut. Nothing is locked; the reference is Frame,
		 * which the first Update's Frame is to continue. */
		void Begin(const Vec& EyeAt, const Vec& LookAt, const Vec& Frame);
		/** One frame. RealDt in real seconds (held to 0..0.25: a longer hitch moves the view no further than a quarter
		 * second would, so a stall never flings it). InWorld may be null (then Settings::FloorFlat). */
		Pose Update(const Input& In, double RealDt, const World* InWorld);
		/** A look the scene asks for (a preset: top view, level): heading and tilt in radians, zoom 0..1, eased to as if
		 * by his hand. NaN keeps the current want; out of range is held to the range; pass the current value to leave
		 * one alone. */
		void Want(double InHeading, double InTilt, double InZoom);

		const Pose& Last() const { return Now; }
		/** The pivot (Ref + Rel + the floor's lift), the heading, the tilt (radians), the zoom 0..1, the boom's length. */
		Vec Pivot() const { return RefAt + RelV + Vec{ 0.0, 0.0, LiftZ }; }
		double Heading() const { return H; }
		double Tilt() const { return T; }
		double Zoom() const { return S; }
		double Arm() const { return ArmOf(S); }
		/** The anchor's id while locked, -1 none. */
		int Locked() const { return bLocked ? LockId : -1; }
		/** The reference (the anchor with its steadied height, or the frame), his offset from it, where that offset is
		 * going, and its swing (metres a second, relative to the reference). */
		Vec Ref() const { return RefAt; }
		Vec Rel() const { return RelV; }
		Vec WantRel() const { return WantRelV; }
		Vec RelSwing() const { return SwingRel; }
		double WantHeading() const { return WantH; }
		double WantTilt() const { return WantT; }
		double WantZoom() const { return WantS; }
		/** The floor's lift now, metres. */
		double Lift() const { return LiftZ; }
		/** How many cuts, how many frames the floor had to put the eye clear at once (the lift could not climb in time),
		 * and how many hard stops the caps made (each cap that stopped the view on a frame counts one: it moved; or the
		 * centimetre or two above). */
		int Cuts() const { return CutCount; }
		int HardLifts() const { return HardLiftCount; }
		int HardStops() const { return HardStopCount; }
		/** The boom's length at a zoom, and the zoom of a length (held to the range). */
		double ArmOf(double AtZoom) const;
		double ZoomOf(double Length) const;

	private:
		double FloorAt(const World* InWorld, double X, double Y) const;
		Vec BoomOf(double AtHeading, double AtTilt, double Length) const;
		void Compose(bool bCutNow);

		Vec RefAt, RefVel, RelV, WantRelV, SwingRel;
		double H = 0.0, WantH = 0.0, SwingH = 0.0;
		double T = 0.0, WantT = 0.0, SwingT = 0.0;
		double S = 0.5, WantS = 0.5, SwingS = 0.0;
		double LiftZ = 0.0, SwingLift = 0.0;
		/** The lift's goal last frame and how fast it climbed then, to lead a goal that climbs. */
		double LiftGoalLast = 0.0, LiftRateLast = 0.0;
		bool bLiftGoalKnown = false;
		/** The lock: its id, the anchor's last good place, its last speed seen moving (kept through a pause) and heading,
		 * its steadied height. */
		bool bLocked = false;
		int LockId = -1;
		Vec AnchorLast, AnchorMoving;
		bool bAnchorMoving = false;
		double AnchorHeadingLast = 0.0;
		bool bAnchorHeadingGood = false;
		double SteadyRefZ = 0.0;
		/** A new reference whose speed was not known on its first frame: it is measured on the next and the carry made
		 * good then (bCoast: the change was a let go, whose rest place moves with it). */
		bool bMeasureNext = false, bCoast = false;
		/** The frame's last good place, id and last speed seen moving (kept through a pause), and the last frame's lock
		 * input (a lock takes on its rise). */
		Vec FrameLast, FrameMoving;
		int FrameIdLast = 0;
		bool bFrameIdKnown = false, bFrameMoving = false;
		/** The pivot (without the lift) at the end of the last frame and its speed over that frame, the speed a change of
		 * reference carries; not good across a cut or a hard stop. */
		Vec PivotEnd, ChordVel;
		/** The pivot's box as last held (given good, or moved on at its walls' last speeds), those speeds, and whether a box
		 * was held last frame (its walls' speeds are measured). */
		Vec BoxLoLast, BoxHiLast, BoxLoVel, BoxHiVel;
		bool bBoxKnown = false, bBoxWas = false;
		bool bPivotEndGood = false, bChordGood = false;
		/** The first frame after Begin (the director's shot exactly), and whether that shot stood over the ceiling (let
		 * down under it, a cut). */
		bool bFirst = false, bBeginCut = false;
		bool bInLockLast = false;
		int InIdLast = -1;
		Pose Now;
		int CutCount = 0, HardLiftCount = 0, HardStopCount = 0;
	};
}
