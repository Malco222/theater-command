#pragma once

#include "Battle/LandEye.h"
#include "Battle/LandFeed.h"

#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * The general's view (Brian, 22 November 02:55Z, release 2: he deploys his army and commands the ground battle himself).
 * A camera the player steers over the field, beside the film director (LandEye) that he hands the lens to with C.
 * Plain C++, so the cloud test drives it with scripted keys, slides, zooms and turns and checks that it never goes
 * under the ground, into a hedge, a house, a tree or the corn, and never jolts.
 *
 * How it moves, like a hawk on a string tied to the point in the middle of the picture: the point (the look-at, on the
 * ground) slides about the field; the string's length is the zoom, from about 400 m up looking down on the whole
 * battle to a man's shoulder looking level along the ground, the view tilting up smoothly as it comes in; turning walks
 * the hawk round the point. Every move is eased, so a key held starts and stops softly and a slide or a pinch is never
 * a jump. The player may also tilt the string himself (Brian, 24 November: "it's hard to move the angle of the camera
 * to see a good troop movement from above"): his own tilt, from the automatic one up to almost straight down, is kept
 * as he zooms; one press of the top view swings the view to look down from about 100 m up, a second press swings it
 * back. Untouched, the view tilts with the zoom exactly as before. Four views of his own may be kept under a number
 * each (Brian, 24 November 20:38Z, release 2: saved views) and called back with one press: the view flies there, its
 * turn and its zoom eased no faster or harder than the picture bears. Where the eye would stand in a hedge, a house or a tree, or among the stalks, it rises over it as an
 * operator would stand up, and comes down again gently once past. Where it cannot climb in time it slows and waits at
 * the thing while it rises, as a man stops at a hedge and climbs it; near the ground it never stands under a crown or
 * eaves (it steps out from under first), and a tall thing near a low eye is stepped round. Only where it could not be
 * kept clear smoothly at all (deep in a wood) is it put clear at once, and then the picture cuts (Pose.bCut).
 *
 * Headings are as everywhere in the cores: radians, 0 north, clockwise (LandFeed::FacingTo).
 */
namespace LandGeneralEye
{
	using LandPlay::Vec;

	/** What the player does this frame (the scene fills it from keys, mouse and trackpad). */
	struct Input
	{
		/** Keys held (W A S D, arrows): -1..1 each, right and ahead of the view. */
		double PanRight = 0.0, PanAhead = 0.0;
		/** The pointer at the screen's edge: -1..1 each; left out while bLockEdgePan (a box or a unit being dragged). */
		double EdgeRight = 0.0, EdgeAhead = 0.0;
		bool bLockEdgePan = false;
		/** This frame's two-finger slide or middle drag, in picture widths (right) and heights (ahead): the ground under
		 * the fingers moves with them, so a slide to the right takes the view left: the scene sends the slide as it
		 * wants the view to go. */
		double SlideRight = 0.0, SlideAhead = 0.0;
		/** Wheel notches or pinch this frame, + in (a pinch's scale change: about 0.1 a notch). */
		double Zoom = 0.0;
		/** Degrees this frame, + turns the view to the right (clockwise seen from above), about the look-at. */
		double Turn = 0.0;
		/** Degrees this frame, + tilts the view further down toward looking straight down on the field, - back toward
		 * the automatic tilt (Shift with W or S, Option with a two-finger slide or the middle drag): his own tilt, kept
		 * as the zoom changes; tilted all the way back, the view tilts with the zoom again. */
		double Tilt = 0.0;
		/** The top view, one press (O, or TOP VIEW): the view swings to look almost straight down from about 100 m up
		 * (or from as high as it already was); a second press swings it back to the tilt and the zoom it had. */
		bool bTopView = false;
		/** A view of his own kept under a number, 0 to 3 (Shift with F1 to F4, or Option and Shift with 1 to 4): where he
		 * looks, his heading, his zoom and his tilt. It is kept where his own hand's last move is bringing it to rest
		 * (a top view just pressed is kept as the top view, not half way up to it, following a unit or not). What is
		 * not his own hand's is kept as it stands this frame: all of it while flying to a kept view (the flight then
		 * ends there) or waiting at a hedge, and where it looks while gliding to a point or following a unit; -1
		 * none. A tilt the flight has the turning of is kept as his own only where it shows on the picture at the
		 * press, and else as the automatic one (the view kept is the picture he has to a quarter of a degree, never
		 * a tilt he cannot see); and under the zoom where his tilt is handed back toward a man's shoulder
		 * (Settings::TiltOwnFrom) a tilt kept in a flight comes to rest at what that zoom lets it be. */
		int KeepView = -1;
		/** A kept view called back (F1 to F4, Option with 1 to 4, or its chip): the view flies there. One never kept is
		 * kept now instead, so the first press of a number is never wasted and never moves the view; -1 none. A key's
		 * pan, a slide, a zoom, a turn or a tilt still moving the view as it is called (a key still held, a trackpad's
		 * slide still running on) is left out until it has stopped, and the pointer at the screen's edge for the whole
		 * flight: none of them takes the view from the call. Any move begun after it does: the flight is let go and
		 * the view comes to rest where it is, under his hand. Its tilt, where the call had the turning of it, stays
		 * his own if the picture has it as his hand takes the flight and still will where it comes to rest; and else
		 * it goes on back to the automatic one, so no tilt of his own is left on unseen and none comes on where the
		 * picture had none as he took it. (What he keeps is where the tilt's own swing brings it to rest, which may
		 * be more than the picture yet showed. A tilt his own hand was already turning back goes on back.)
		 * The flight lasts until the tilt is there too, so a move in the last of the tilt's swing takes it as well. */
		int CallView = -1;
		/** Glide to a field point (a double click on the map). */
		bool bGoTo = false;
		Vec GoTo;
		/** Glide to a block (a unit card clicked, Tab); -1 none. */
		int GoBlock = -1;
		/** While held after GoBlock, the view keeps that block in the middle as it moves; any pan, slide or GoTo ends
		 * it. */
		bool bFollow = false;
	};

	struct Settings
	{
		/** The eye's height over the look-at at the farthest zoom, and the lens there and at the nearest. */
		double Highest = 400.0;
		double FovFar = 50.0, FovNear = 44.0;
		/** At the nearest zoom: the arm's length and the look-at's height over the ground (a man's chest), metres; the
		 * eye there stands about 1.6 m up, a man's shoulder. */
		double NearArm = 3.0, NearAim = 1.35;
		/** The tilt down at the nearest and farthest zoom, degrees. */
		double TiltNear = 5.0, TiltFar = 52.0;
		/** How far the look-at may go past the rules' field's edges, metres (so picking never lands far off it). */
		double Margin = 40.0;
		/** A notch of zoom, as a share of the whole range (about a fifth nearer each notch). */
		double ZoomStep = 0.045;
		/** Key and edge pan: this share of the arm's length over the ground a second, plus PanLeast m/s. */
		double PanSpeed = 0.75, PanLeast = 4.0;
		/** How fast the look-at, the zoom and the turn catch up (a critically damped spring's rate), and a glide's. */
		double Ease = 11.0, ZoomEase = 9.0, TurnEase = 12.0, Glide = 3.2;
		/** A glide low over the field flies higher the faster it goes, this many metres for each m/s, up to LiftMost (a
		 * glide to a block in Sedan's woods ran 360 m/s at the treetops; it met them a frame before it braked). */
		double GlideLift = 0.04;
		/** How far from any thing the eye keeps (a hedge's shrubs, a house, a tree), and over the ground and the ears of
		 * corn, metres. */
		double ThingClear = 1.2, GroundClear = 1.0, OverCrop = 0.35;
		/** The nearer clearance the eased eye never comes inside (it may lag into the wanted one for a frame or two). */
		double HardClear = 0.45, HardGround = 0.5, HardOverCrop = 0.1;
		/** World::Free is asked only this near the ground (it says a house's floor or a bank's hedge at any height). */
		double FreeUnder = 12.0;
		/** And for the near clearance, only this near (a bank's hedge, a house's rooms). */
		double FreeHard = 3.5;
		/** Nothing the scene draws stands taller than this over the ground: an eye higher is clear without asking. */
		double Tallest = 30.0;
		/** How far from a man the eye keeps (the view may stand among the men, never in one), metres. */
		double ManClear = 0.5;
		/** How high the view is lifted over a thing before it steps round it instead (or half its height over the
		 * ground, if more), and the most it is ever lifted, metres. */
		double LiftLow = 6.0, LiftMost = 25.0;
		/** How fast the view is carried aside round a tall thing (a spring's rate), and the furthest, metres. */
		double PushRate = 8.0, PushMost = 8.0;
		/** The eye's lift over things: how fast it rises and settles back (spring rates). */
		double RiseRate = 20.0, SettleRate = 3.0;
		/** The lift's tent over a thing near the eye: it slopes away this many metres a metre. */
		double Slope = 0.7;
		/** Following a block, the nearest the zoom comes (about 9 m back and 4 m up). */
		double FollowNearest = 0.25;
		/** The lift's fastest climb and its hardest push, a share of the eye's distance to its look-at a second (and
		 * a second squared): about 46 and 860 degrees of the picture. */
		double LiftSpeed = 0.8, LiftPush = 15.0;
		/** How fast the lift is hurried when the eye is inside the near clearance (a spring's rate), and the most it
		 * may be hurried in a frame before it is put clear with a cut instead, metres. */
		double HurryRate = 30.0, HurryMost = 0.15;
		/** The hurried lift's hardest push, a share of the eye's distance to its look-at a second squared (about 1,030
		 * degrees of the picture, under the 1,200 the view is held to). */
		double HurryPush = 18.0;
		/** How long the lift may be shut in under leaves or eaves before the view cuts clear, seconds. */
		double StuckMost = 0.4;
		/** How fast the look-at's height follows the ground it slides over (a spring's rate). */
		double GroundEase = 5.0;
		/** How fast the eye may climb ahead of a thing it is coming to, m/s (it looks 1.6 s ahead). */
		double Climb = 2.5;
		/** The longest the view brakes at a thing while the lift climbs, seconds. */
		double BrakeLongest = 3.0;
		/** His own tilt (Input::Tilt, the top view): the steepest it goes, degrees (almost straight down; never quite, so
		 * the heading keeps its meaning); how softly it meets the automatic tilt, its floor, degrees; the zoom under
		 * which it is handed back to the automatic tilt toward a man's shoulder (0..1); how fast it eases (a spring's
		 * rate); and its fastest swing and hardest push, degrees a second and a second squared (a swing to the top view
		 * held well under the 1,200 degrees a second squared the view is held to). */
		double TiltMost = 85.0, TiltSoft = 3.0, TiltOwnFrom = 0.2, TiltEase = 6.0, TiltFastest = 70.0, TiltPush = 250.0;
		/** With his own tilt set, the zoom's fastest swing and hardest push too, shares of the whole zoom a second and a
		 * second squared: the top view's climb from a man's shoulder to 100 m, two thirds of the zoom in one press, takes
		 * about two seconds and is not a jolt. */
		double ZoomFastest = 0.5, ZoomPush = 1.0;
		/** The top view: its tilt, degrees, and the least height of the eye over the look-at there, metres. */
		double TopTilt = 80.0, TopUp = 100.0;
		/** Flying to a kept view: the turn's fastest swing and hardest push, degrees a second and a second squared (half
		 * a turn about the look-at takes some two and a half seconds; the zoom is held as with his own tilt). */
		double TurnFastest = 90.0, TurnPush = 300.0;
		/** And the zoom's and the tilt's fastest and hardest then, a share of their own: a call may ask the whole zoom
		 * and the whole tilt together, and the turn with them from high up. */
		double FlySlower = 0.8;
		/** A flight from low down, or to a view kept low down, goes as a crane would: up first where it stands, till the
		 * eye is this high over what it looks at (over everything the scene draws, Tallest), then across and round, then
		 * down. Turning or crossing among the trees would have it stop dead at one. Metres. */
		double FlyUp = 45.0;
		/** What was moving the view as a kept view was called is left out until it has been still this long, seconds
		 * (a trackpad's slide runs on for a second or so after the fingers lift, a frame or two apart at its end). */
		double LeftStill = 0.15;
	};

	/** How many views of his own the general may keep. */
	inline constexpr int ViewCount = 4;

	class Eye
	{
	public:
		/** Before the clock runs (the deploy step) or at any time: the view looks at LookAt (a field point; its height
		 * is ignored) facing Heading, the eye Up metres over the ground there. */
		bool Begin(const LandFeed::Recording& InRecording, const LandEye::World& InWorld, const Vec& LookAt, double Heading, double Up = 250.0,
			const Settings& InSettings = Settings());
		/** One frame: RealDt seconds of life, the battle clock at Battle (for following a block). */
		LandEye::Pose Update(const Input& In, double RealDt, double Battle);
		const LandEye::Pose& Current() const { return Now; }

		/** Where the view looks (on the ground), its heading, and its zoom (0 nearest, 1 farthest). */
		Vec LookAt() const { return Vec(Tx, Ty, AimZ); }
		double Heading() const { return H; }
		double Zoom() const { return S; }
		/** The block the view is following, -1 none. */
		int Following() const { return Follow; }
		double Margin() const { return Set.Margin; }
		/** The view's lift over things and its step aside now (for the cloud test). */
		double LiftNow() const { return Lift; }
		double PushNow() const { return std::hypot(PushX, PushY); }
		double NeedNow() const { return NeedWas; }
		int HurriedNow() const { return Hurried; }
		int WaitedFrames() const { return Waited; }
		int BrakedFrames() const { return Braked; }
		/** How many places the last frame asked about (for the cloud test). */
		int AskedNow() const { return Asked; }
		/** Whether his own tilt is set (else the view tilts with the zoom), whether the top view is on (TOP VIEW lit),
		 * and the tilt down now, degrees. */
		bool OwnTilt() const { return bOwnTilt; }
		bool TopView() const { return bTop; }
		double TiltDegrees() const;
		/** Whether a view is kept under a number (0 to 3); the kept view he stands at, is coming to rest at or is flying
		 * to (its chip lit), -1 once his own hand, a glide or a followed unit has taken the view from it, in where it
		 * looks, its turn, its zoom or its tilt; the kept view
		 * a call is flying to now, -1 none (the flight is over once the view is there in where it looks, its turn, its
		 * zoom and its tilt: his own tilt at the view's, or handed back to the automatic one for a view kept without
		 * his own); and whether the turn and the zoom are held to their fastest and hardest, as
		 * they are through a flight and through a draw-back from too near a followed unit, until both have come to
		 * rest. */
		bool ViewKept(int Number) const { return Number >= 0 && Number < ViewCount && Views[Number].bKept; }
		int ViewAt() const { return AtView; }
		int FlyingTo() const { return FlyTo; }
		bool Flying() const { return bFlying; }
		/** A kept view as a call will reach it (for the cloud test): where it looks, its heading (radians), its zoom and
		 * its tilt down (degrees); false where none is kept under the number. */
		bool ViewIs(int Number, Vec& OutLook, double& OutHeading, double& OutZoom, double& OutTilt) const;
		/** The eye's height over the look-at at a zoom (0..1), at the automatic tilt. */
		double UpAt(double Zoom) const;
		/** Where a block's men or vehicles stand on average at a battle time (those not fallen or gone); false if none. */
		bool BlockAt(int Block, double Battle, double& OutX, double& OutY) const;

	private:
		const LandFeed::Recording* Rec = nullptr;
		LandEye::World Here;
		Settings Set;
		LandEye::Pose Now;
		/** The look-at, the heading and the zoom now, their wanted values, and their springs' swings. */
		double Tx = 0.0, Ty = 0.0, H = 0.0, S = 1.0, AimZ = 0.0;
		double WantX = 0.0, WantY = 0.0, WantH = 0.0, WantS = 1.0;
		double SwingX = 0.0, SwingY = 0.0, SwingH = 0.0, SwingS = 0.0;
		bool bGliding = false;
		int Follow = -1;
		/** The eye's lift over things now and its swing. */
		double Lift = 0.0, LiftSwing = 0.0;
		/** The view carried aside round a tall thing now, and its swing. */
		double PushX = 0.0, PushY = 0.0, PushSwingX = 0.0, PushSwingY = 0.0;
		/** The ground under the look-at, eased, and its swing. */
		double GroundEased = 0.0, GroundSwing = 0.0;
		bool bFirst = true;
		double NeedWas = 0.0;
		int Hurried = 0;
		/** Frames the view waited at a thing while the lift climbed (for the cloud test). */
		int Waited = 0;
		/** Braking before a thing the lift cannot climb in time: the lift it must reach before the view goes on, how
		 * long it has braked, and frames braked (for the cloud test). */
		bool bBrake = false;
		double BrakeNeed = 0.0, BrakeFor = 0.0;
		int Braked = 0;
		mutable int Asked = 0;
		/** How long the lift has been shut in under a thing, seconds. */
		double StuckFor = 0.0;
		/** His own tilt, radians: now, wanted and its swing (the tilt the view takes from it is TiltFor's); whether it
		 * is set, whether it is on its way back to the automatic tilt (switched off once there), whether a flight to a
		 * view kept without his own tilt, not his own hand, is what turned it back (so a hand that takes the flight
		 * keeps his tilt his own while the picture still has it), and the top view with the zoom and the tilt a second press
		 * goes back to. */
		double Tl = 0.0, WantTl = 0.0, SwingTl = 0.0, TlGoal = 0.0;
		bool bOwnTilt = false, bToAuto = false, bCallToAuto = false, bTop = false, bTopWasOwn = false;
		double TopWasTl = 0.0, TopWasS = 1.0;
		/** His kept views: the look-at, the heading, the zoom, and his own tilt where he had one set. */
		struct View
		{
			bool bKept = false, bOwn = false;
			double X = 0.0, Y = 0.0, H = 0.0, S = 1.0, Tl = 0.0;
		};
		View Views[ViewCount];
		/** The kept view he stands at or is coming to rest at (-1 none); the kept view a call is flying to (-1 none: a
		 * view only kept is no flight, and a flight his hand has taken is over); and whether the turn and the zoom are
		 * held to their fastest and hardest, as a call may ask half a turn or the whole zoom at once: through a flight
		 * and through a draw-back from too near a followed unit, until both have come to rest. */
		int AtView = -1, FlyTo = -1;
		bool bFlying = false;
		/** His keys' pan, his slide, his zoom, his turn and his tilt: how long since each last moved the view, seconds,
		 * and which were still moving it as a kept view was called (left out until they have stopped). */
		static constexpr int HandKinds = 5;
		double Quiet[HandKinds] = { 1.0, 1.0, 1.0, 1.0, 1.0 };
		bool bLeft[HandKinds] = { false, false, false, false, false };
		/** The zoom whose eye stands FlyUp over the look-at at the automatic tilt (his own is never shallower). */
		double FlyS = 1.0;
		/** Each block's tracks (the live feed adds tracks: rebuilt when the count changes). */
		std::vector<std::pair<int, std::vector<int>>> ByBlock;
		size_t TracksSeen = 0;

		/** Steps aside found (or not) lately, by where they were looked for. */
		struct AsideMemo
		{
			Vec At, D;
			double When = -1e9;
			bool bOk = false;
		};
		AsideMemo Memo[4];
		/** The tops of things lately found, by the metre square: the height clear over it and when. */
		std::unordered_map<long long, std::pair<double, double>> Tops;
		/** The corn's height lately asked, by the half-metre square, and when. */
		mutable std::unordered_map<long long, std::pair<double, double>> Crops;
		int MemoNext = 0;
		double Clock = 0.0;
		/** What placing the eye changes, kept so a frame's move can be taken back. */
		struct Kept
		{
			double GroundEased = 0.0, GroundSwing = 0.0, AimZ = 0.0;
			double PushX = 0.0, PushY = 0.0, PushSwingX = 0.0, PushSwingY = 0.0, Lift = 0.0, LiftSwing = 0.0, NeedWas = 0.0;
			int Hurried = 0;
			bool bBrake = false;
			double BrakeNeed = 0.0, BrakeFor = 0.0, StuckFor = 0.0;
		};
		Kept Keep() const;
		void Restore(const Kept& K);
		/** Places the eye for the look-at, zoom and heading now (bHold: the step aside held as it is); returns how hard
		 * the lift was hurried (0 not, 1 eased fast, 2 put clear at once). */
		int Place(double Dt, bool bHold, Vec& OutEye, Vec& OutLook);
		double Ground(double X, double Y) const;
		double CropAt(double X, double Y) const;
		/** Whether the eye may stand at a point: no thing within Radius (no man within ManClear), and the film's
		 * World::Free near the ground (within FreeUnder with bFree, else FreeHard). */
		bool Clear(const Vec& At, double Radius, bool bFree = true) const;
		void ClampWanted();
		void Index();
		double ArmAt(double Zoom) const;
		double TiltAt(double Zoom) const;
		double AimAt(double Zoom) const;
		/** The tilt his own tilt Own gives at a zoom: never shallower than the automatic tilt there (met softly); the
		 * most his tilt is let ease to at a zoom (handed back to the automatic tilt toward the nearest zoom); the tilt the
		 * view takes now (his own, or the automatic); the eye's height over the look-at at a zoom and his own tilt; and
		 * the zoom whose eye stands TopUp over the look-at at the top view's tilt. */
		double TiltOwn(double Zoom, double Own) const;
		double TiltMostAt(double Zoom) const;
		/** Whether his own tilt Own stands clear of the automatic tilt at a zoom: more than a quarter of a degree
		 * over it, the soft meeting counted, whatever a man's shoulder then lets the picture show of it. */
		bool TiltStands(double Zoom, double Own) const;
		/** Whether his own tilt Own shows on the picture at a zoom: as far as that zoom lets it stand (TiltMostAt),
		 * more than a quarter of a degree over the automatic tilt there. */
		bool TiltShows(double Zoom, double Own) const;
		double TiltFor(double Zoom, double Own) const { return bOwnTilt ? TiltOwn(Zoom, Own) : TiltAt(Zoom); }
		double UpOwn(double Zoom, double Own) const;
		double TopZoom() const;
		/** His tilt this frame: the top view pressed, his tilt keys or slide, the way back to the automatic tilt. */
		void TiltWish(const Input& In);
		/** His own tilt begun where the automatic one is, just under its soft floor: the view does not move as it is
		 * set. Nothing where it is set already. */
		void OwnTiltBegin();
		/** A view kept, and a kept view called back (one never kept: kept now instead). */
		void KeepView(int Number);
		void CallView(int Number);
		/** A flight to a kept view taken by his own hand: each wish let go at the point its own spring comes to rest
		 * from the swing it has, so nothing rushes on to the kept view and nothing stops dead (bPanned: by a pan or
		 * a slide, which eases the look-at at his hand's rate; bPlace: the look-at too, not where he has asked for a
		 * glide of its own). His own tilt, where the flight had the turning of it, stays his own only if the picture
		 * has it as the move takes the flight (TiltStands of the view as it stands, zoom and tilt), and will have
		 * it where it comes to rest (TiltShows of the tilt's own rest at the zoom the taking move leaves him at);
		 * else it goes on back to the automatic one. ZoomIn is the zoom notch of the frame that takes the flight
		 * (In.Zoom * ZoomStep), which the frame has yet to take off the zoom's wish, and ZoomLeast the nearest
		 * zoom the frame will then hold the wish to (FollowNearest where a unit's card takes the flight and the
		 * unit is followed, else 0): both count towards where the zoom rests. So a notch out or a card's follow
		 * cannot leave him a tilt he cannot see, and nothing that lowers the automatic tilt after the call (a
		 * notch in, a zoom still coasting in) can make his own a tilt the picture never had. Toward a man's
		 * shoulder the tilt kept is what the shoulder has left of it as the take finds it. A pan begun while the
		 * look-at still coasts lets it go again at the pan's own rate (Update). */
		void FlightLetGo(bool bPanned, bool bPlace, double ZoomIn = 0.0, double ZoomLeast = 0.0);
	};
}
