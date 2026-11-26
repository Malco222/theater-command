#pragma once

#include "Battle/AirFeed.h"
#include "Battle/AirShow.h"

#include <map>
#include <string>
#include <vector>

/**
 * The air battle's screen, as words and numbers, in plain C++ so the cloud test reads exactly what the player will
 * (air 3d-handoff.md sections 1, 4, 6 and 8; the UI director's words). It keeps the screen's clock, writes the top bar,
 * the aim slip, the near line and the dispatches, names each group's tag, writes each order as the bridge takes it,
 * and picks what the close camera looks at. AAirBattle and SAirBattleScreen only draw what it says.
 */
namespace AirDesk
{
	/**
	 * The screen's clock. In the 3D scene x1 is real time: the aircraft fly at their own speeds (Brian, 19 November:
	 * "not moving normal"), so a battle second, SKY.scale air seconds, takes that many real seconds. The run-up and the
	 * run home go at x10 of the battle's own pace, as the play version (mark 1); the clock stops while a question stands.
	 */
	class Clock
	{
	public:
		/** 0 (held), 0.5, 1, 2 or 4: the player's. */
		double Speed = 1.0;
		/** The staff's question or BREAK OFF ALL is standing. */
		bool bQuestion = false;
		/** The flyover before the battle is on: the battle runs at x1 under it, whatever the run-up would be. */
		bool bFlyover = false;
		/** The strike is on (AirShow::Show::StrikeOn, set each frame before Step; the realism audit's 4.2): while bombs
		 * or torpedoes are in the air, a stick is still to go or a dive is going down, the run-up or the run home is
		 * held at the player's own pace, and for StrikeHoldS real seconds after; it then eases into x10 over StrikeEaseS. */
		bool bStrike = false;
		double StrikeHoldS = 5.0, StrikeEaseS = 3.0;
		/** Battle seconds to run for Real seconds; Tag is the field's top-left tag (empty for none). */
		double Step(double Real, const AirFeed::Top& Head, double Scale, std::string& Tag);
		/** The last Step ran fast (past half way from the player's pace to x10): a bomb's rush near the eye, timed for
		 * the battle's own pace, is not played. */
		bool Fast() const { return bFast; }

	private:
		double SinceMet = -1.0; // real seconds since the formations met (-1 before)
		double SinceStrike = 1e9; // real seconds since the strike was last on
		bool bFast = false;
	};

	/** One group's tag over its formation. */
	struct Tag
	{
		int Id = 0;
		int Side = 0;
		bool bMine = false;
		bool bFighter = true;
		std::string Name;   // the player's own by name; the enemy's with its country word (sim.called)
		std::string Count;  // "12 aircraft"
		std::string Doing;  // "wavering", "heading home", "landed" or empty
		bool bGone = false;
	};

	/** A line in the dispatches. */
	struct Dispatch
	{
		std::string Text;
		int Side = -1;
		double Age = 0.0;   // real seconds shown
		bool bMine = false;
	};

	/** Everything the screen writes this frame. */
	struct Words
	{
		std::string Kicker;                  // "RAID RESULT · December 1941" (the after card); "THE RAID ON ..." over the field
		std::string Title;                   // "The raid on Pearl Harbor"
		std::string MySide, TheirSide;       // "Japanese", "American"
		std::string Chance;                  // "Card about 37% · now about 40%"
		/** The tug-of-war bar (Brian, 21 November 05:55Z; the UI director's ruling, 14:47Z): where his colour meets the
		 * enemy's, his share from the left (0.5 level), eased on the screen's clock (KnotOf, KnotEase). */
		double Knot = 0.5;
		std::string MyNat, TheirNat;         // the sides' nations for the flags at the bar's ends ("USA", "GER")
		double MyBar = 0.0, TheirBar = 0.0;  // HOW NEAR TO BREAKING OFF
		std::string ClockTag, Clock;         // "×10 UNTIL THE FORMATIONS MEET"; "2:31"
		std::string AimTarget, AimState, AimLost; // the aim slip under THE AIM
		std::string NearLine;                // "Your raid is near breaking off" (amber, stays)
		std::vector<Tag> Tags;
		std::vector<Dispatch> Dispatches;    // newest last
		bool bOver = false;
	};

	/** The after card, from TC.battleEnd().after (or the recording's), with his side first. */
	struct Card
	{
		std::string Kicker, Title, Level, Reckoning;
		std::vector<std::string> Lines;
		struct Row { std::string Name, Words; bool bMine = false; bool bFighter = true; };
		std::vector<Row> Rows;
	};

	class Desk
	{
	public:
		void Begin(const AirFeed::Sky& Board, const std::string& Me);
		/** A new view (the bars, the names it calls, the labels), kept until the battle shown reaches its time. */
		void Take(const AirFeed::Frame& View);
		/** The battle shown has reached ShownT: the views up to it are read (the clock and the words follow what is
		 * shown, not the view held in hand ahead of it). */
		void Reach(double ShownT);
		/**
		 * (Release 2) A raid on troops: the column as the show has it now (AirShow::Show::Marks: each piece as the hits
		 * shown on it and the views' words after them leave it) and the organisation cut so far as the show lets it out
		 * (Show::MarksCut), for the aim slip and the top bar; given each frame after Reach. So the words never tell of a
		 * hit before its bomb or rounds land on its piece (the reviewer, 25 November). Nothing where the raid is not on
		 * troops.
		 */
		void Struck(const AirMarks::Column& Shown, double Cut);
		/** A dispatch the show releases (FxKind::Dispatch): kept, at most a few, oldest out. */
		void Say(const std::string& What, const std::string& Text, int Side, int Unit);
		/** Ages the dispatches (real seconds) and writes the words for the groups as shown now. */
		void Write(double Real, const std::map<int, AirShow::Group>& Shown, const std::string& ClockTag, Words& Out);
		Card CardOf(const AirFeed::After& After) const;

		/** The orders, as TC.battleOrder takes them (handoff section 4). Empty where the order cannot be given. */
		std::string Attack(const std::vector<int>& Units, int Target) const;
		std::string Move(const std::vector<int>& Units, double X, double Y) const;
		std::string Hold(const std::vector<int>& Units) const;
		std::string BreakOff(const std::vector<int>& Units) const;
		std::string BreakOffAll() const;
		std::string Escort(const std::vector<int>& Units, int Box, bool bClose) const;
		std::string Hunt(const std::vector<int>& Units) const;
		std::string Band(const std::vector<int>& Units, int Band) const;
		std::string Jettison(const std::vector<int>& Units) const;
		/**
		 * (Release 2) A raid on troops (Brian, 24 November 2026: "if a bomber group is bombing troops instead of a
		 * factory or town and they get intercepted than i want that playable"): his fighter groups among Units (by the
		 * tags) sent down the column's road on a strafing pass, { do: 'air:strafe', units: [...] }. Empty where none of
		 * them is a fighter group of his, or he may not strafe it (CanStrafe).
		 */
		std::string Strafe(const std::vector<int>& Units, const std::vector<Tag>& Tags) const;
		std::string HandOver() const;
		/** The question HAND IT TO THE STAFF asks, with the staff's chance now. */
		std::string StaffQuestion() const;
		static std::string BreakOffAllQuestion() { return "Break off every air group? The battle ends here."; }

		int MySideIndex() const { return MyIndex; }
		/**
		 * (Release 2, the scramble offer; Brian, 24 November 2026 22:21Z: "i want every air battle playable".) The desk
		 * read for the defender. The view's chance (sky.chance, AirSim.staffChance) is the raider's whoever he is, and the
		 * aim slip and the staff's question were written for his own raid. Set, and his side the defence: the top bar's
		 * "now about" and the staff's question give his own chance, the aim slip speaks of the raid as the enemy's, and
		 * HAND IT TO THE STAFF asks of the fight. The scene sets it where the Battle rules card's "Scramble your fighters"
		 * is on (AAirBattle::DefendOn); unset, every word is as it was.
		 */
		void SetDefendRead(bool bOn) { bDefendRead = bOn; }
		bool DefendRead() const { return bDefendRead && MyIndex == 1; }
		const std::map<int, AirFeed::Label>& Labels() const { return LabelsNow; }
		const AirFeed::Top& Head() const { return HeadNow; }
		/**
		 * (Release 1.5; the playtester's finding 2 on FLY IT YOURSELF, 23 November) The battle's end as its after card
		 * books it. The view's stamp (over.winner) is the battle's own judgement, but the card is the campaign's: a raid
		 * handed to the staff is settled by the staff's dice, and an unopposed fight for the sky is booked a victory the
		 * stamp may not give. So once the card is read, the side its verdict names (his own on a victory, the other on a
		 * defeat) is the winner the bar settles on and the music plays for. A card with no verdict (OVER, NOT BOOKED)
		 * leaves the stamp standing.
		 */
		void SettleFrom(const AirFeed::After& After);
		/** The same, by side ("att" or "def"): the other side's desk of a test reads the one card as its mirror. */
		void Settle(const std::string& Winner) { Settled = Winner == "att" || Winner == "def" ? Winner : std::string(); }
		/** The winner the end shows: the after card's once it is settled, else the view's stamp ("att", "def" or ""). */
		const std::string& Winner() const { return Settled.empty() ? HeadNow.Winner : Settled; }
		const AirFeed::Sky& Board() const { return Sky; }
		/** (Release 2) The raid is on an army's column (place.kind "ground", its pieces given in place.marks): the words
		 * speak of the column, not "the target", and STRAFE THE COLUMN is offered his fighter groups. */
		bool OnTroops() const { return Sky.PlaceKind == "ground" && Sky.Marks.Has(); }
		/** (Release 2) STRAFE THE COLUMN is his to give: a raid on troops, and the column the enemy's (the reviewer, 25
		 * November: a player defending his own column had been offered it). */
		bool CanStrafe() const { return OnTroops() && ColumnTheirs(); }

	private:
		AirFeed::Sky Sky;
		AirFeed::Top HeadNow;
		std::string Me = "att";
		int MyIndex = 0;
		std::string Settled;                   // the after card's winner, once read (SettleFrom)
		bool bDefendRead = false;              // (release 2, the scramble offer) read for the defender (SetDefendRead)
		bool bStarted = false;
		double KnotShown = 0.5;                // the bar's knot as drawn, eased toward KnotOf
		std::map<int, std::string> Called;     // sim.called: kept, as the view sends only what changed
		std::map<int, AirFeed::Label> LabelsNow;
		std::vector<Dispatch> Said;
		std::map<int, bool> EngagedOnce;       // air:engage shown once for each escort group (UI director, 21:19Z)
		std::vector<AirFeed::Frame> Ahead;     // views taken, not yet reached
		// (release 2, a raid on troops) the column as the show has drawn it struck (Struck): its vehicles and guns
		// wrecked, burning and damaged, the men lost from its blocks, and the organisation cut so far (view.marks.cut)
		int MarksWrecked = 0, MarksBurning = 0, MarksDamaged = 0, MarksMenLost = 0;
		double MarksCutNow = 0.0;
		/** The column is the enemy's (its nation theirs; one of neither side's, or none said, the defender's). */
		bool ColumnTheirs() const;
		/** The column as the words name it: "the British I Corps" (its corps, place.marks.corps), else "the British column". */
		std::string ColumnName() const;
		/** A raid on troops' title, for the screen and the after card: "The raid on the British I Corps near Hampshire". */
		std::string TroopsTitle() const;
		std::string Elapsed(double Seconds) const;
		void Read(const AirFeed::Frame& View);
	};

	/** What the close camera looks at: a fighter on a pass, a bomber under attack, a fall. */
	struct Subject
	{
		int Plane = -1;     // the plane's key to ride with
		int Other = -1;     // the plane it fights (kept in frame), or -1
		double Since = 0.0; // air seconds on this subject
	};
	/** Picks the next subject (every 8 to 12 air seconds, or when the one followed falls): firing fighters first, near Near. */
	Subject Choose(const std::vector<AirShow::PlaneOut>& Planes, const std::vector<int>& Firing, const AirShow::Vec& Near, int Avoid);
	/**
	 * Picks a fighter to ride that is about the fight's business, so what it fights is in the picture with it (Brian,
	 * 20 November 2026, on the Bremen film: "where are the german fighters?"; both rides had followed a Mustang and
	 * the German fighters showed as specks): a fighter within 2.5 km of its quarry, the nearer and the more nearly
	 * pointed at it the better, one firing before one closing, any after a bomber before any after a fighter, and one
	 * coming at a bomber head-on most of all (the German fighters' head-on pass at the B-17 boxes), so the bombers fill
	 * the picture beyond it.
	 * Planes: the side's aircraft to choose among; All: every aircraft up (the quarry is looked for there). With none
	 * such, Choose's pick.
	 */
	Subject ChooseRide(const std::vector<AirShow::PlaneOut>& Planes, const std::vector<AirShow::PlaneOut>& All, const AirShow::Vec& Near, int Avoid);
	/** How nearly a fighter flies straight at the bomber it is after, 0 to 1 (0 when its quarry is a fighter or not up). */
	double HeadOn(const AirShow::PlaneOut& Fighter, const std::vector<AirShow::PlaneOut>& All);

	/**
	 * The chase view (Brian, 21 November 2026 05:52Z: "click on the squadron and then it goes to a view of one of the
	 * planes from behind so you can watch it. you can then move from one plane to the next if you want to see other
	 * planes"). A squadron's line: the aircraft of the group the close view can ride (shown, flying, not falling, not
	 * on the ground), in the order they were put up, which is its leader first and then down the formation.
	 */
	std::vector<int> RideLine(const std::vector<AirShow::PlaneOut>& Planes, int Group);
	/**
	 * The aircraft of a squadron to ride: Step 1 the next along its line after Current, -1 the one before, each going
	 * round from the end to the start; 0 Current while it can still be ridden, else the first. Current need not be in
	 * the line (one gone down: 1 is the next after where it flew). -1 when the squadron has none left to ride.
	 */
	int RideStep(const std::vector<AirShow::PlaneOut>& Planes, int Group, int Current, int Step);
	/**
	 * The squadrons the chase view steps through: each with an aircraft to ride, his own first and then the enemy's;
	 * and the one Step (1 or -1) along them from Current, going round the end (Current not among them: the first, or
	 * the last for -1). -1 when there is none.
	 */
	std::vector<int> RideSquadrons(const std::vector<Tag>& Tags, const std::vector<AirShow::PlaneOut>& Planes);
	int RideSquadronStep(const std::vector<Tag>& Tags, const std::vector<AirShow::PlaneOut>& Planes, int Current, int Step);
	/** The chase view's count: "Aircraft 3 of 12", Current's place along its squadron's line; "" when not on it. */
	std::string RideCount(const std::vector<AirShow::PlaneOut>& Planes, int Group, int Current);

	/**
	 * Where the strategy camera looks (the Mac's release 2 check: the middle of groups kilometres apart was empty sky,
	 * in all three approach pictures and in Oahu 02 and 04 and Midway 04 and 07): the middle of the thickest crowd of
	 * these groups' aircraft that fly and are drawn, the one with the most of them within Reach metres and those
	 * about it. Held on Anchor (an aircraft's key; -1 none yet) while its crowd keeps at least three quarters of the
	 * best one's, so the eye does not hop between two crowds near the same size. False when none of them flies.
	 */
	bool CrowdOf(const std::vector<AirShow::PlaneOut>& Planes, const std::vector<int>& Groups, double Reach, int& Anchor, AirShow::Vec& Middle);

	/**
	 * The strategy camera's look and framing, flying smooth (AirShow::Show::SetSmooth; Brian, 25 November 13:27Z: "the
	 * bomber is shaking in the video too like the fighter"). Measured (Tools/air_show/air_smooth_probe --shake, the
	 * Hampshire raid's minute about its first contact): the look took the plain middle of the aircraft within 350 m of
	 * the crowd's middle, that middle the plain middle of those within 1200 m of CrowdOf's anchor, and any step of it
	 * under 150 m was the eye's step in that frame; so each time an aircraft crossed one of the rings the whole picture
	 * jumped: by 20 pixels and more beyond its glide 14 to 18 times a real minute at x1 and 52 to 64 at x4 (in the
	 * raid's busiest forty seconds 100 to 120 and 320 to 410), the He 111s shaking 5 to 6 pixels rms on the 1440
	 * picture at x1 and 12 at x4 (14 to 37 in the busy seconds); and the framing took the crowd's middle span, so a
	 * crowd of He 111s and Spitfires flipped the eye between 926 and 459 m. Here:
	 * - every ring is soft, an aircraft's weight in it falling from 1 to 0 over Soft metres either side of it, and an
	 *   aircraft that falls, lands, is hidden or leaves the groups looked at fades out of it over FadeS real seconds
	 *   (one coming in fades in);
	 * - the crowd's middle is held on the crowd it is on while any of it is still about it, each aircraft's weight in
	 *   its ring eased in and out over RingS real seconds: a change of CrowdOf's anchor within the crowd moves it no more
	 *   than the aircraft move, and it goes to another crowd only when the anchor does;
	 * - the look is on a knot of the crowd: the middle of its aircraft about a point, each by how near it is (Near: a
	 *   half at the knot's width, as far as they fly in KnotS real seconds, within Tight and Reach), that point carried
	 *   by their travel and drawn on to that middle over CentreEase real seconds, so that it stays on the knot it is on,
	 *   a few aircraft or one, and goes over to a bigger one near it without a step;
	 * - the look goes with that knot, its aircraft's travel and its middle's own drift up to DriftLeast metres a real
	 *   second and DriftShare of their speed, that speed eased over CarryEase real seconds; the rest of a step (a move to
	 *   another crowd, or the knot's aircraft all gone) is eased, critically damped over about LookEase real seconds, so
	 *   that neither the look nor its speed steps;
	 * - the framing is by the crowd's mean span by weight (TypeAwayOf), eased the same way over AwayEase.
	 * Off (release 1.5's flying), the scene keeps its own camera as it was.
	 */
	class Overview
	{
	public:
		static constexpr double Reach = 1200.0, Tight = 350.0, Soft = 100.0, FadeS = 1.0, RingS = 1.0, LookEase = 1.0, AwayEase = 1.0, CentreEase = 1.5;
		static constexpr double DriftLeast = 50.0, DriftShare = 0.5, KnotS = 2.5, PaceEase = 1.0, CarryEase = 0.13, ZoomEaseS = 0.3, ZoomMost = 1.1;
		struct Aim
		{
			bool bFound = false;     // the groups have aircraft up to look at
			AirShow::Vec Focus;      // where the look would be
			double Span = 0.0;       // the crowd's mean span, metres (0: none)
			double TypeAway = 800.0; // the eye's distance at the player's own 800 m, eased
		};
		/** This frame's aim on the aircraft of the groups Picked, Real seconds after the last; Anchor as CrowdOf's. bCut:
		 * a cut, and the look and the framing start afresh. */
		Aim Step(const std::vector<AirShow::PlaneOut>& Planes, const std::vector<int>& Picked, double Real, int& Anchor, bool bCut);
		/** Where the look is drawn this frame, from where it was (Was): going with the knot Wanted (Step's Focus) is the
		 * middle of, and eased on to it. A look set otherwise since its last (a picture's, a cut's) starts from where it
		 * is. */
		AirShow::Vec Look(const AirShow::Vec& Was, const AirShow::Vec& Wanted, double Real);
		/**
		 * The eye's distance at the player's own 800 m for a crowd of this span (metres): its aircraft about 45 pixels
		 * across at 1920, 70 from 25 m, through the 55 degree lens, as the scene framed them; the 45 eased into the 70
		 * between 24 and 26 m. Within 250 and 1500 m.
		 */
		static double TypeAwayOf(double Span);
		/** An aircraft's weight about the look's knot, Far metres from its point in a knot Radius metres wide: nearly 1
		 * within half of it, a half at it, falling off as the fourth power beyond, never quite 0. */
		static double Near(double Far, double Radius);
		/** The player's zoom (Shown, the scene's distance at his own 800 m, and Vel the speed of its logarithm a second)
		 * eased on to where his wheel or trackpad has asked for it (Want) over Real seconds, critically damped over
		 * ZoomEaseS in the distance's logarithm and never faster than ZoomMost of it a second (under 2% of the distance a
		 * sixtieth of a second): a scroll never steps the picture in one frame, and a notch is about 95% there in under
		 * three quarters of a second (the raid camera's row, AAirBattle::Zoom). True once there: Shown is then Want and
		 * Vel 0. */
		static bool ZoomEase(double& Shown, double& Vel, double Want, double Real);

	private:
		std::map<int, double> Fade;      // each aircraft's share of its weight, 0 to 1
		std::map<int, double> InMid, InClose; // and its weight in the crowd's ring and about the look's knot, eased (RingS)
		std::map<int, double> Weight;    // and its whole weight in the knot, last frame
		std::map<int, AirShow::Vec> Prev; // and where it was last frame
		AirShow::Vec Carry, CarryVel, Centre, LastFocus, LastMid, Off, OffVel, Given;
		bool bStarted = false, bGiven = false, bMid = false, bFocus = false;
		int LastAnchor = -1; // the anchor last frame
		double Away = -1.0, AwayVel = 0.0, Pace = 0.0, Radius = Tight;
	};

	/**
	 * The tug-of-war bar's knot (the air thread's rule, passed by the UI director 21 November 14:47Z): the running form
	 * of the result the battle is judged by. x is the view's sky.score.ratio, (aircraft the defender lost + what the
	 * bombs were worth + 10) / (aircraft the raider lost + 10), and the knot is x / (1 + x) from the raider's flag; a
	 * defending player reads 1 / x, so his knot is the mirror. 0.5 before the first loss. Once the battle is over it
	 * lies at least KnotSettle past the middle on the booked winner's side. Kept within 0.02 and 0.98, as the card's.
	 */
	constexpr double KnotSettle = 0.05;
	constexpr double KnotLag = 1.5;
	double KnotOf(const AirFeed::Top& Head, int MyIndex);
	/** The same, with the winner the end shows given (Desk::Winner: the after card's where it is settled). */
	double KnotOf(const AirFeed::Top& Head, int MyIndex, const std::string& Winner);
	/** The knot as drawn follows KnotOf with about KnotLag real seconds of lag, never a step (a long frame counts as 0.1 s). */
	double KnotEase(double Shown, double Want, double Real);

	/**
	 * The flyover before an air battle (Brian, 19 November 2026, approved: before an air battle starts, a short cinematic
	 * flyover of both formations, like generals surveying the field). Up to three shots on the real clock, with cuts
	 * between them, while the battle runs at x1 under them:
	 *  1. the raid: its biggest bomber formation, from above and just behind its lead three, looking down on them so the
	 *     sea or the cloud deck is behind them, pushing in as the shot runs;
	 *  2. what it comes for: from beyond the target, the far side from the raid, pushing in over it while the look lifts
	 *     from the target toward where the raid comes from (over a harbour the target held in the picture's lower third);
	 *  3. the guard, when the defender has fighters up: the same as the raid's, on its lead three.
	 * Then the scene glides into its strategy view over Glide seconds. A click or a key ends it at once. Eyes and looks
	 * are in the show's metres (X east, Y north, Z up from the sea); the scene keeps the eye over the ground.
	 */
	class Flyover
	{
	public:
		struct Shot
		{
			AirShow::Vec Eye, Look;
			int Index = -1; // 0 the raid, 1 what it comes for, 2 the guard; -1 when the shots are over
		};
		static constexpr double RaidLong = 5.0, TargetLong = 4.5, GuardLong = 3.5, Glide = 2.5;
		/** The lens of the shots, degrees across. */
		static constexpr double Lens = 50.0;
		/**
		 * The raid's and the guard's shots (the art director on build 1750, 19 November 2026: the planes were 10 to 15
		 * pixel specks in a white void): each of the guard's lead three about LeadShare of the picture's width (150
		 * pixels of the 2881 pixel picture, her 120 to 180), each of the raid's trailing three about TrailShare (her note
		 * on build 1760: the nearest about 120 pixels, the rest of the raid receding ahead of them), seen from this many
		 * degrees above: the guard's from 13, the raid's from 21 (her note on build 1756: tilted 8 to 10 degrees further
		 * down, so the cloud field fills the lower half).
		 */
		static constexpr double LeadShare = 150.0 / 2881.0, TrailShare = 125.0 / 2881.0, LookDown = 13.0, RaidLookDown = 21.0;
		/** How far the raid's shot looks on from its trailing three toward the raid's middle (0 at them, 1 at it). */
		static constexpr double RaidLookOn = 0.5;
		/**
		 * And how far under the picture's middle, and how far to its side, any of the trailing three may stand in that
		 * shot (degrees): the look is drawn back toward them before one of them would leave the picture. The flyover's
		 * lower bar lies 12.7 degrees under the middle and the picture's sides 25 either way (the pre-check on round 13:
		 * with the trailing element on a flank of the raid the look turned 30 to 49 degrees off the raid's heading and
		 * the three stood outside the picture; over Midway one of them slid under the bar from 3.2 s).
		 */
		static constexpr double RaidLow = 11.0, RaidAside = 20.0;
		/** Over a harbour, how far under the picture's middle the target is held while the look lifts (degrees). */
		static constexpr double HarbourLow = 9.5;
		/**
		 * Picks the raid's formation and the guard from the battle's first groups. False if there is no raid to show.
		 * bHarbour: the target is a harbour (its ships under the land's edge), held in the target shot's lower third.
		 */
		bool Begin(const std::map<int, AirShow::Group>& Groups, double InTargetX, double InTargetY, bool bHarbour = false);
		/** The shot T real seconds after Begin, on the groups and their planes as they are now. */
		Shot At(double T, const std::map<int, AirShow::Group>& Groups, const std::vector<AirShow::PlaneOut>& Planes) const;
		/** When the shots end (the glide to the strategy view follows). */
		double Length() const { return RaidLong + TargetLong + (Guard >= 0 ? GuardLong : 0.0); }
		int RaidGroup() const { return Raid; }
		int GuardGroup() const { return Guard; }

	private:
		int Raid = -1, Guard = -1;
		double TargetX = 0.0, TargetY = 0.0;
		bool bOverHarbour = false;
		AirShow::Vec RaidAt;   // where the raid was at Begin
		AirShow::Vec Toward;   // level, from the target toward the raid at Begin
		// (the last place each group was seen: a shot whose group has gone keeps its last framing)
		mutable std::map<int, AirShow::Group> Last;
		// (each close shot's three, by plane, chosen as its shot begins: the guard's lead, the raid's trailing element;
		// the planes fly a little behind the slots their group's place gives, and the shot frames the planes)
		mutable std::map<int, std::vector<int>> Leads;
		// (a group's lead planes already seen going down, and how far the aim stands off its flying leads' middle since)
		mutable std::map<int, std::vector<int>> Fallen;
		mutable std::map<int, AirShow::Vec> Shifts;
		// (the raid's middle as its shot begins, ahead, right and up of its group's place: where the raid's shot looks)
		mutable std::map<int, AirShow::Vec> Middles;
		// (and each of the raid's trailing three as its shot begins, ahead, right and up of their own middle)
		mutable std::map<int, std::vector<AirShow::Vec>> Trios;
		/** The side the raid's shot is taken from, kept from its first frame (0 until then). */
		mutable double RaidSide = 0.0;
		const AirShow::Group* Find(const std::map<int, AirShow::Group>& Groups, int Id) const;
	};
}
