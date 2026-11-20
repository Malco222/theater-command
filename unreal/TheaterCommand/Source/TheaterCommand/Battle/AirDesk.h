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
		/** Battle seconds to run for Real seconds; Tag is the field's top-left tag (empty for none). */
		double Step(double Real, const AirFeed::Top& Head, double Scale, std::string& Tag);

	private:
		double SinceMet = -1.0; // real seconds since the formations met (-1 before)
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
		double Balance = 0.5, BalanceStart = 0.5; // his, from the left
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
		std::string HandOver() const;
		/** The question HAND IT TO THE STAFF asks, with the staff's chance now. */
		std::string StaffQuestion() const;
		static std::string BreakOffAllQuestion() { return "Break off every air group? The battle ends here."; }

		int MySideIndex() const { return MyIndex; }
		const std::map<int, AirFeed::Label>& Labels() const { return LabelsNow; }
		const AirFeed::Top& Head() const { return HeadNow; }
		const AirFeed::Sky& Board() const { return Sky; }

	private:
		AirFeed::Sky Sky;
		AirFeed::Top HeadNow;
		std::string Me = "att";
		int MyIndex = 0;
		bool bStarted = false;
		double BalanceAtStart = 0.5;
		std::map<int, std::string> Called;     // sim.called: kept, as the view sends only what changed
		std::map<int, AirFeed::Label> LabelsNow;
		std::vector<Dispatch> Said;
		std::map<int, bool> EngagedOnce;       // air:engage shown once for each escort group (UI director, 21:19Z)
		std::vector<AirFeed::Frame> Ahead;     // views taken, not yet reached
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
		const AirShow::Group* Find(const std::map<int, AirShow::Group>& Groups, int Id) const;
	};
}
