#pragma once

#include "Battle/FreeEye.h"
#include "Battle/SeaLookSwitches.h"
#include "Battle/SeaShow.h"

#include <cstdint>
#include <string>
#include <vector>

/**
 * The sea battle's free camera (Brian, 26 November 2026, release 2: a camera he flies anywhere; a click on a ship locks
 * on and follows her; Tab steps through single ships; the same on land, at sea and in the air), the sea's side of it.
 * The camera itself is Battle/FreeEye (the camera thread's core, never edited here); this is everything the sea battle
 * adds to it, in plain C++ so the cloud test (Tools/sea_free_eye) runs the very code the game runs: the sea's settings,
 * the keys, which ships may be picked, locked or stepped to (only ships the player has found: an unfound boat is never
 * read for anything), the clicks along a ray, the divisions, the lock fed each frame on real seconds, the floor (the
 * drawn sea, never under its still face), the lens turned for Unreal's horizontal field of view and eased, the pivot's
 * box, the switches, the Mac's scripted drive, the rules of who owns the camera and when it cuts, and the eye-driven
 * refreshes. ASeaBattle only moves values between Unreal and these functions (Battle/SEA-FREE-EYE.md says how).
 *
 * Axes as the sea board's and the core's: X east, Y north, Z up, metres; headings clockwise from north (ShipOut's in
 * degrees, the core's in radians); Ahead(H) = (sin H, cos H).
 */
namespace SeaFreeEye
{
	using FreeEye::Vec;

	inline constexpr double Pi = 3.14159265358979323846;

	// ---- 1a. Settings ----------------------------------------------------------------------------------------------

	/** The sea's agreed numbers (04:24-04:27Z): 3 m over the drawn wave, a 1,500 m ceiling, a lock further than 800 m a
	 * cut, a ship's heave steadied over 3 s, a pan keeping the lock. */
	inline constexpr double FloorClear = 3.0, EyeMost = 1500.0, CutBeyond = 800.0, SteadyZ = 3.0;
	/** The director's lens, degrees ACROSS the picture (Unreal's FieldOfView), near and far. The free eye holds the same
	 * angle across on every window shape (the lead's ruling on Q8), so a take and a give back never change how wide the
	 * picture is: the core's vertical FovNear and FovFar are fitted to the window's aspect (FitLens) at the take and
	 * whenever the shape changes (29.4 and 32.6 high at 16:9). */
	inline constexpr double LensNear = 50.0, LensFar = 55.0;
	/** Under a drawn cloud deck the eye stays this far under its base, and never under the director's own 300 m overview
	 * (the lead's ruling on Q7, the overview's rule in SeaSky.h): min(1,500, max(300, base - 150)). */
	inline constexpr double UnderDeck = 150.0, CeilingLeast = 300.0;
	/** The held keys' turn and tilt, degrees a second: the land battle's rates (LandBattleCommand.cpp) until the core
	 * holds them (core ask 1). */
	inline constexpr double TurnRate = 75.0, TiltRate = 50.0;
	/** The pivot's box: the board and this much round it, and from BoxLow (the deepest drawn trough and heave, with room)
	 * to the ceiling. */
	inline constexpr double BoxMargin = 3000.0, BoxLow = -20.0;
	/** The lens's ease from the director's lens to the free eye's: no faster than this, degrees a REAL second, and eased
	 * in and out at this push, degrees a second squared (mark C12). */
	inline constexpr double LensFastest = 40.0, LensPush = 80.0;
	/** A click's forgiveness round a hull grows with her distance: at least this angle, degrees. */
	inline constexpr double PickSlackDeg = 0.6;
	/** A click on the water picks a division within this of its line, metres (or half its mean gap, if less). */
	inline constexpr double DivisionReach = 150.0;
	/** Division anchors' ids: from here up (ship keys are piece * 64 + slot, far below it). */
	inline constexpr int DivisionIds = 1 << 30;
	/** A lock taken with the eye nearer her than her clearance (half her drawn length, 1g): the clearance fed to the core
	 * grows from where the eye stood at this, metres a real second, to the whole of it, so the core's hard stop never
	 * throws the eye out in one frame (a take by a lock stays the director's shot). */
	inline constexpr double ClearGrows = 20.0;
	/** The core's soft clearance keeps the eye this many clearances off her (FreeEye.cpp, the boom's goal "Keeps": Clear
	 * times 1.05); a grown clearance starts this far under the eye's distance so the soft hold has nothing to catch up. */
	inline constexpr double ClearSoftMargin = 1.05;
	/** The drive's clock against its step starts and its whole seconds: a frame within this of a boundary (seconds) is
	 * counted after it, so a summed clock a hair under a whole second never moves a frame into the wrong second. */
	inline constexpr double DriveSlack = 1e-6;

	/** The core's defaults with the sea's numbers (FREE-EYE.md "Settings per scene"), its lenses fitted to Aspect and its
	 * ceiling CeilingAt; the scene sets Free.Set to it once, before any Begin (mark C20). */
	FreeEye::Settings SeaSettings(double Aspect, double CeilingAt = EyeMost);
	/** The core's FovNear and FovFar (vertical) that show LensNear and LensFar across at Aspect (Q8). */
	void FitLens(FreeEye::Settings& Set, double Aspect);
	/** The ceiling, the core's EyeMost and the box's top: EyeMost, or under a drawn deck (bDeck, its base DeckBase metres)
	 * min(EyeMost, max(CeilingLeast, DeckBase - UnderDeck)) (mark C14, the lead's ruling on Q7). */
	double Ceiling(bool bDeck, double DeckBase);

	// ---- 1b. Hands to input ----------------------------------------------------------------------------------------

	/** The keys the sea's free camera knows, by plain name (the screen turns an FKey into one). J locks and lets go (LOCK
	 * ON is J in all three scenes, the UI director 27 Nov 06:30Z; L stays the campaign's, Forces, as with the switch off). */
	enum class KeyName : uint8_t { W, A, S, D, Up, Down, Left, Right, Q, E, J, C, Tab, U, Other };
	/** What a key event is to the screen: not the camera's (Unhandled: it goes on to the campaign), the camera's
	 * (Handled), or U pressed (Handled; the screen asks UnderPressed, then hands it to ASeaBattle::UnderKey). */
	enum class KeyTaken : uint8_t { NotMine, Mine, Under };

	/** What each held key does, as a bit (latched at its press: Shift let go while it is held does not change it). */
	enum Held : uint32_t
	{
		PanLeft = 1u, PanRight = 2u, PanAhead = 4u, PanBack = 8u, TurnLeft = 16u, TurnRight = 32u, TiltDown = 64u, TiltUp = 128u,
		GoDown = 256u, GoUp = 512u
	};
	/** The held keys: W A S D, the four arrows, Q, E. */
	inline constexpr int HeldKeys = 10;

	/** The board's ray under a click: from the eye, along a way (made unit length when taken). */
	struct Ray
	{
		Vec From, Way;
	};

	/** His hands since the last frame: each held key's latched bit and their sum, the wheel's notches (+ in), the pinch's
	 * steps, the twist (degrees, + clockwise), the two-finger slide (picture widths and heights), and this frame's presses
	 * (J, C, Tab's steps, a click and a double click with its ray). */
	struct Hands
	{
		uint32_t KeyBits[HeldKeys] = {};
		uint32_t Bits = 0u;
		double Wheel = 0.0, Pinch = 0.0, Twist = 0.0, SlideRight = 0.0, SlideAhead = 0.0;
		bool bJ = false, bC = false;
		int Tab = 0;
		bool bClick = false, bDouble = false;
		Ray Click;
	};

	/** One key event, the whole key rule (mark C11): held keys latch their bit at the press, Shift with it (Shift+W,
	 * Shift+S and Shift with the up and down arrows tilt, Shift+Q and Shift+E go down and up, Q and E turn, the arrows as
	 * W A S D), and clear it at the release; a repeat changes nothing held. J and C ignore repeats; Tab steps on every
	 * press, repeats included (Shift+Tab back); U never repeats. Any other key is NotMine. */
	KeyTaken KeyEvent(Hands& H, KeyName Key, bool bDown, bool bShift, bool bRepeat);
	/** Every held key let go (the screen lost the keys). */
	void LetGoKeys(Hands& H);
	/** A click (or a double click) along a board ray. */
	void ClickAt(Hands& H, const Ray& Along, bool bDouble);
	/** The core's input from his hands over RealDt real seconds (the turn and tilt at the land's rates, held to the core's
	 * own quarter second), the slide's picture's Aspect. Picks, locks and the frame are not here. */
	FreeEye::Input InputOf(const Hands& H, double RealDt, double Aspect);
	/** Anything of his hands that moves the camera (a held key, the wheel, a pinch, a twist, a slide). */
	bool HandsMove(const Hands& H);

	// ---- 1c. Which ships may be picked, locked or stepped to --------------------------------------------------------

	/** Found (bShown) and in the fight (in line or limping): never a ship sinking or gone, never one not found. */
	bool MayPick(const SeaShow::ShipOut& Ship);
	/** The found ships in Tab's order: his side first, then the enemy; inside a side by LineAt (-1 last), piece, slot. */
	std::vector<int> TabOrder(const std::vector<SeaShow::ShipOut>& Ships, int MySide);
	/** The key Step places on from From in Order (wrapping); From not in it: the first (forward) or the last (back); Step
	 * 0: From. -1 for an empty order. */
	int TabStep(const std::vector<int>& Order, int From, int Step);

	// ---- Drawn lengths ----------------------------------------------------------------------------------------------

	/** A drawn hull's length by key, as the scene laid her (Of null, or its answer not over 0: ShipOut::Long). */
	struct Lengths
	{
		double (*Of)(const void* Context, int Key) = nullptr;
		const void* Context = nullptr;
		double DrawnLong(const SeaShow::ShipOut& Ship) const;
	};

	// ---- 1e. Clicks ------------------------------------------------------------------------------------------------

	/** The ship whose hull the ray meets first (her box Long by Beam turned to her heading, 2 m under her waterline to
	 * 6 m and 0.12 of her length over it, riding her heave; at least max(Beam / 2, distance x tan 0.6 degrees) of slack
	 * round it), of the ships MayPick allows only: an unfound boat is passed through as if she were not there. -1 none. */
	int PickAlongRay(const std::vector<SeaShow::ShipOut>& Ships, const Ray& Along);
	/** Where the ray meets the sea's still face, a division (a piece's found ships in line, two or more) whose line in
	 * slot order passes within min(150 m, half its mean gap): its piece, the nearest; -1 none. */
	int DivisionAlongRay(const std::vector<SeaShow::ShipOut>& Ships, const Ray& Along);

	// ---- 1f. Divisions ---------------------------------------------------------------------------------------------

	/** A piece's found ships in line, by slot: their keys, their middle (each at her waterline and heave), half the
	 * longest drawn, the leader (the lowest slot) and her heading in radians. No members: none found in line. */
	struct Division
	{
		std::vector<int> Members;
		Vec Middle;
		double Clear = 0.0;
		int Leader = -1;
		double Heading = 0.0;
	};
	Division DivisionOf(const std::vector<SeaShow::ShipOut>& Ships, int Piece, const Lengths& Long);

	// ---- 1d. The floor ---------------------------------------------------------------------------------------------

	/** The floor the free eye stands over: the drawn sea under (X, Y), as the scene draws it this frame (Drawn, with its
	 * Context: FleetWaves::SampleNear on the scene's own Near once the swell is drawn), never under the still face's 0
	 * (mark C3: every "under the water" test in the scene reads Z < 0). Unset: today's still face, 0. */
	struct SeaFloor : FreeEye::World
	{
		double (*Drawn)(const void* Context, double X, double Y) = nullptr;
		const void* Context = nullptr;
		double Floor(double X, double Y) const override;
	};

	// ---- 1h. Lens --------------------------------------------------------------------------------------------------

	/** A vertical lens (the core's Pose.Fov) as Unreal's horizontal FieldOfView at Aspect, degrees; and back. */
	double HorizontalLens(double Vertical, double Aspect);
	double VerticalLens(double Horizontal, double Aspect);
	/** The shown lens X (and its Swing) eased toward Want over Dt real seconds: no faster than LensFastest a second, sped
	 * up and slowed no harder than LensPush (mark C12). */
	void EaseLens(double& X, double& Swing, double Want, double Dt);

	// ---- 1g. The lock ----------------------------------------------------------------------------------------------

	/** What is locked: one ship (Key) or a division (Piece and its found members), the AnchorId fed, and where the anchor
	 * stood last frame (for its velocity on real seconds; none on the first locked frame, mark C8). */
	struct Lock
	{
		int Key = -1;
		int Piece = -1;
		int Id = -1;
		std::vector<int> Members;
		Vec Last;
		bool bLast = false;
		/** The clearance fed so far: grown from ClearFrom (the eye's distance from her when this lock was first fed, if
		 * under her clearance) for ClearFor real seconds, at ClearGrows (for the anchor ClearId). */
		double ClearFrom = 0.0, ClearFor = 0.0;
		int ClearId = -1;
		bool On() const { return Key >= 0 || Piece >= 0; }
	};

	// ---- 1j. Switch take -------------------------------------------------------------------------------------------

	/** The switches as OpenOn takes them: -TheaterSeaFreeEye=1 only in a full sea battle watched live (bFullBattle: never
	 * the month's scene, never a picture run); -TheaterSeaFreeEyeRide=1 and -TheaterSeaFreeEyeDrive= only with it. Their
	 * values in force written back for the "Sea look:" line. */
	struct Taken
	{
		bool bOn = false, bRide = false;
		/** "off", "tour", or the drive file's path as typed. */
		std::string Drive = "off";
	};
	Taken Take(SeaLookSwitches::Look& InForce, bool bFullBattle);

	// ---- 1k. The scripted drive (the Mac's check) --------------------------------------------------------------------

	/** One timed line: held keys (Hold) from From to To, a press (J, Tab, Shift+Tab or C) at From, or wheel notches spread
	 * over From to To; its words for the log. */
	struct DriveLine
	{
		double From = 0.0, To = 0.0;
		uint32_t Hold = 0u;
		KeyName Press = KeyName::Other;
		bool bShift = false;
		double Wheel = 0.0;
		std::string Words;
	};
	/** A drive: its lines, its steps (each distinct start time, with the first line's words), how far it has played (real
	 * seconds from its start: the bench's first counted frame), and each step's frames, seconds and frames in each whole
	 * second, for the per-step lines; the highest the eye rose. */
	struct Drive
	{
		bool bOn = false, bStarted = false, bDone = false;
		std::vector<DriveLine> Lines;
		std::vector<double> Starts;
		std::vector<std::string> StepWords;
		std::vector<int> StepFrames;
		std::vector<double> StepSeconds;
		std::vector<std::vector<int>> StepSecondFrames;
		int StepNow = -1;
		double T = 0.0;
		double Highest = -1e9;
	};
	/** The tour's lines (plan 1k): take, down low, a low pan turning, Tab to the first found own ship, Tab, Tab, let go,
	 * up to the ceiling, a fast pan at the top, the camera back. */
	const char* TourText();
	/** A drive's text: one line each, "<from> [<to>] hold <keys> | press <key> | wheel <notches>", '#' and after it the
	 * line's words. False, with Error, where a line does not read. */
	bool ParseDrive(const std::string& Text, Drive& Out, std::string& Error);

	// ---- One frame ---------------------------------------------------------------------------------------------------

	/** When an eye-driven refresh was last made (mark C9), on real seconds, where the eye stood and for which hero. */
	struct Refresh
	{
		bool bKnown = false;
		double At = 0.0;
		Vec Eye;
		int Hero = -1;
	};

	/** Everything of the free camera kept between frames (ASeaBattle's FreeState). */
	struct State
	{
		/** He has the camera (taken by a camera key or a lock, until C). */
		bool bOn = false;
		bool bRide = false;
		/** The free eye owned the camera last frame; C or U owes the director's first frame back a cut. */
		bool bWasFree = false, bOwedCut = false;
		Hands H;
		/** The camera's keys the screen took at their press (a bit a KeyName), so their release is the screen's too, and
		 * only theirs (ScreenKey); none once the screen loses the keys (ScreenLostKeys). */
		uint32_t ScreenKeys = 0u;
		/** The pick: a ship's key, or a division's piece; -1 none. Every lock, however made (a click, J, Tab), moves the
		 * pick onto what it locks, so J while locked always lets go. */
		int PickKey = -1, PickPiece = -1;
		Lock L;
		int NextDivisionId = DivisionIds;
		/** The lens shown (horizontal degrees) and its swing. */
		double Lens = LensNear, LensSwing = 0.0;
		/** The window shape the core's lenses were last fitted to (Q8; 0 none yet). */
		double AspectFitted = 0.0;
		Drive D;
		/** Lines for the log (the drive's), taken and cleared by the scene. */
		std::vector<std::string> Said;
		/** The picture shown last frame (free or the director's). */
		bool bLastKnown = false;
		Vec LastEye, LastLook;
		double LastLens = LensNear;
		/** What was fed to the core this frame (bFed: the free eye ran its frame), for the cloud test and the log. */
		FreeEye::Input Fed;
		bool bFed = false;
		/** Counts, for the test and the log: takes, locks, let goes, give backs. */
		int Takes = 0, Locks = 0, LetGoes = 0, GiveBacks = 0;
	};

	/** What the scene gives the free camera each frame. Board metres throughout. */
	struct Scene
	{
		const std::vector<SeaShow::ShipOut>* Ships = nullptr;
		const Lengths* Long = nullptr;
		/** The player's side (the views' "me", SeaFeed::SideOf; not Show::MineSide, -1 under ShowAll). */
		int MySide = 0;
		/** This frame's real seconds, unclamped (the core holds them to a quarter second itself). */
		double RealDt = 0.0;
		double Aspect = 16.0 / 9.0;
		/** The director's shot this frame (eye, look point, horizontal lens): where a take begins. */
		Vec DirEye, DirLook;
		double DirLens = LensNear;
		/** His own dive (U) stands: the director's under-water view owns the camera. */
		bool bHandDive = false;
		const FreeEye::World* Floor = nullptr;
		double BoardSize = 0.0;
		/** Ceiling(...) for this battle's sky. */
		double CeilingNow = EyeMost;
		/** The drive may start (the bench's first counted frame, or play's first frame without a bench). */
		bool bDriveMayStart = true;
	};

	/** What the free camera gives back. */
	struct Shot
	{
		/** The free eye owns the camera this frame (EyeOwner): Eye, Look and Lens are the picture. */
		bool bFree = false;
		Vec Eye, Look;
		/** Horizontal degrees, eased. */
		double Lens = LensNear;
		/** A cut this frame (the core's: a lock too far, a teleport, a take or a return lifted out of the water; the
		 * glue's: a take let down from over the ceiling). */
		bool bCut = false;
		/** C or U gave the camera back (the director's first frame back is owed a cut: this one). */
		bool bGiveBack = false;
		/** The camera was taken this frame (Begin). */
		bool bTaken = false;
		/** The lock's id changed this frame (a lock, a let go, a division's members). */
		bool bLockChanged = false;
		/** The ship the view follows: the locked one, a division's leader (its lowest slot), -1 none. */
		int Hero = -1;
		int Locked = -1;
	};

	/** One frame of the free camera, called every frame while the switch is on, whoever owns the camera (picks and locks
	 * are held to MayPick even while the director has it): the drive's hands, the pick and the lock dropped the frame
	 * their ship fails MayPick (marks C1, C2), C (its own frame the director's), the clicks, J, Tab, the take (Begin
	 * from the director's shot this frame, his lens, the core's lenses fitted to the window's shape; a lock that took the
	 * camera sent from the next frame), a let go holding the boom where it stands (her clearance held it off her, 1g),
	 * Rise held to the room left under the ceiling (core ask 13), the core's frame on real seconds over the sea's floor,
	 * the lens eased. */
	Shot Step(State& St, FreeEye::Eye& Free, const Scene& In);

	/** C: the camera back to the director; the lock, the pick and the Tab place cleared (mark C13); a drive running
	 * stopped. */
	void GiveBack(State& St);
	/** U pressed (the lead's ruling on Q9): while a drive plays, nothing (bOutDive false: every key but C is the
	 * drive's); else, if he has the free camera, it is given back first exactly as C gives it, and bOutDive is true: U
	 * then goes to the director's dive (ASeaBattle::UnderKey) exactly as today, from the director's eye of the frame
	 * shown (the scene's FreeDirAt, never where the free eye was flown), so the free eye never aims a dive. True when it
	 * had the camera. */
	bool UnderPressed(State& St, bool& bOutDive);
	/** The screen's answer to one key event (mark C10), true for Handled: the key rule's (KeyEvent, its result in Out).
	 * A press is the camera's only for a camera key with no Control, Command or Option held (bChord: Command+Q still
	 * quits, the campaign's chords still reach it), or for one the screen already took at its press (its repeats).
	 * A release is always told to KeyEvent, so nothing stays held in the free eye, but it is Handled only for a key the
	 * screen took at its press: a key the campaign saw go down, it sees come up. Any other key (Other: L among them)
	 * is never the camera's, either way. */
	bool ScreenKey(State& St, KeyName Key, bool bDown, bool bShift, bool bRepeat, bool bChord, KeyTaken& Out);
	/** The screen lost the keys (its focus went elsewhere): every held key let go, and none of them the screen's. */
	void ScreenLostKeys(State& St);
	/** The log lines said since last asked (the drive's), moved out. */
	std::vector<std::string> TakeSaid(State& St);

	// ---- The rules of who owns the camera, and the cut (2c, mark C7) ----------------------------------------------

	/** The free eye owns the camera this frame: the switch on, the camera taken, and no hand dive of his standing (U
	 * stays the director's view). */
	bool EyeOwner(bool bSwitch, bool bTaken, bool bHandDive);
	/** AimEye's cut. Free: the scene's owed cut or the core's own (Pose.bCut), nothing of the director's. Not free:
	 * today's expression, term for term: bEyeCut || (Want != ShotNow && bMoved) || (through the face) || (UnderWindow
	 * changed && bMoved). */
	bool CutNow(bool bFreeNow, bool bEyeCut, bool bPoseCut, bool bShotChanged, bool bMoved, bool bFaceCrossed, bool bWindowChanged);

	// ---- 2g. The eye-driven refreshes, free (mark C9) -----------------------------------------------------------

	/** Free only: the funnels chosen again on the free eye's own cut, a hero change, or once the eye has moved 1 km or 4
	 * real seconds have passed; never on a director's shot change. Due: R marked made now. */
	bool FunnelsDue(Refresh& R, double RealNow, const Vec& Eye, bool bCut, int Hero);
	/** Free only: the far wreck mesh laid again at a cut, or once the eye has moved Moved metres, or half a real second. */
	bool FarWreckDue(Refresh& R, double RealNow, const Vec& Eye, bool bCut, double Moved);
}
