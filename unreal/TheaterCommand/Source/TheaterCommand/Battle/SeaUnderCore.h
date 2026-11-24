#pragma once

#include "Battle/ConvoyLooksCore.h"
#include "Battle/SeaShow.h"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

/**
 * THE UNDERWATER WAR-FILM VIEW (U1; Brian's release 2 ask, the wolf packs and the escorts' depth charges): for a few of
 * a convoy night's moments the camera goes under the waves, into murky green water with silt hanging in it and the light
 * coming down through the sea's face, beside a boat at her depth or under her, while a pattern of depth charges comes
 * sinking past her, the asdic pings on her hull and the hull creaks; then it comes back up, on the escort's quarter, as
 * the sea over the pattern heaves white. Plain C++, no engine: ASeaBattle's marked hooks ("U1 hook (under water)") and
 * FSeaUnder (Battle/SeaUnder) draw and play what is worked out here, and the cloud test (Tools/sea_show/sea_under_test.cpp,
 * run by Tools/sea_show/run.sh) checks it against the very show the scene plays.
 *
 * LOOKS ONLY. The battle has decided who holds which boat, where every charge bursts and what is lost; the show
 * (SeaShow) says when each is seen. Nothing here changes any of it or draws on the battle's luck: the look's own hash
 * (ConvoyLook::Hash01) and the battle's clock are all it uses, so the same recording gives the same look. The camera
 * goes under because a pattern was dropped, never because it kills: whether a charge is the battle's blow is not even
 * handed over (SeaShow::Show::ChargeDown leaves it out), a charge is drawn from where it was first seen going in, and a
 * boat's end only lengthens a window already chosen. WHO IS FOUND IS THE SHOW'S ALONE: the surface and the water draw by
 * the one rule, SeaShow's SetMine (ShipOut::bShown, the show's own per-step truth, which reads her hunters' heard, the
 * sonar events, the blows and the guns), so a boat is gone under beside only while the show draws her (the player's own
 * always; an enemy boat only once found, for the whole window), and only where no boat the show hides is near her; the
 * stage hides what the show hides (AAirStage::SetShown), above the sea and under it. Nor is the eye under beside a boat
 * up with her crew on deck: struck and up (SeaShow's sea:surrender), or at her deck gun on the surface (CrewUp).
 *
 * Places are board metres (the show's), Z up from the sea; headings degrees clockwise from north; times the battle's
 * seconds. Numbers: [P] published (naval-battles/wolfpack-r2/field-side-read.md section 3, and the ship models); [SHOW]
 * the show's, which these must agree with; [EST] this lane's estimate, with its knob where it has one.
 */
namespace SeaUnder
{
	using ConvoyLook::P3;

	/** [P] An ahead-thrown bomb's sinking speed (the Hedgehog's, about 7 m a second). */
	inline constexpr double AheadSink = 7.0;
	/** Seconds the eye stays on the surface after a window before the shots go on. */
	inline constexpr double SurfaceHold = 12.0;
	/** [EST] How far the water lets the eye see (the fog's 5 in the hundred left), by day and by night ("-TheaterUnderSee="). */
	inline constexpr double SeeDay = 85.0, SeeNight = 60.0;
	/** [P] A Type VIIC's draught, keel to waterline (Content/Models/Ships Submarine_GER spans z -4.7 to 10.49), and [EST]
	 * how far her casing stands over her pressure hull: her hull's middle lies half of both above her keel. */
	inline constexpr double BoatDraught = 4.7, BoatCasing = 1.8;

	enum class Kind { Pattern, Ahead, Dive, Rise, ByHand };

	/** One stretch of the battle seen from under the water. */
	struct Window
	{
		Kind What = Kind::Pattern;
		std::string Name;                 // its picture's: "under-pattern", "under-ahead", "under-dive", "under-rise"
		int Boat = -1, BoatPiece = -1;    // the boat's ship key (the show's) and piece
		int Side = -1;                    // hers: 0 the attacker, 1 the defender
		int Escort = -1;                  // the escort's ship key (Pattern, Ahead), else -1
		double From = 0.0, Until = 0.0;   // the eye goes under at From and comes up at Until
		double HoldUntil = 0.0;           // and stays on the surface over it to here (SurfaceHold, or the staged beat's end)
		double Pace = 1.0;                // battle seconds a screen second where a staged scene plays it (1 watched)
		double Drop = -1.0;               // the charges went in
		double FirstBurst = -1.0, LastBurst = -1.0, Set = -1.0; // that escort's bursts of them; their mean depth (metres)
		double Sinking = 0.0, Spread = 0.0, Run = 0.0;          // the drop's: seconds to the burst, metres across, seconds going in
		int N = 0;                        // charges, or bombs
		P3 Aim, Track;                    // the pattern's middle; the way the escort ran over it (level, length 1)
		P3 Near;                          // a pattern's: where its charges that burst near her (within PatternSeen m, the booked
		                                  //   blow aside) burst from her, on the average (level, from her place at each burst)
		P3 HerAt;                         // her middle as it opened
		double Heading = 0.0, Keel = 0.0, Long = 67.0, Beam = 6.2; // hers as it opened
		int Hand = 1;                     // the eye stands to her starboard (+1) or port (-1)
		double End = -1.0;                // she went down in it (battle seconds), else -1
	};

	/**
	 * The asdic's pings on a boat's hull: the one thing of who hunts her that the show does not say. Whether she is found is
	 * never asked here: that is the show's alone (ShipOut::bShown, SeaShow's SetMine), for the director's windows, the
	 * player's own dive and what may be in the picture. An escort's asdic is on her from a sea:sonar contact it gains by
	 * asdic (or with no kind, as the show takes one) until that contact is lost, it gains her by other means (radar, sight:
	 * she is up), or she or he is sunk (sea:sunk); or from the view whose escort's echo says asdic on her (Piece::EchoOn and
	 * EchoKind, as each view says) to the view that no longer does. A radio bearing pings nothing.
	 */
	class Sight
	{
	public:
		void Begin();
		void Told(const std::vector<SeaShow::Event>& Events);
		/** A view's pieces at its time T: each escort's echo on a boat, where the view carries it. */
		void Echoed(double T, const std::vector<SeaShow::Piece>& Pieces);
		/** Whether an escort's asdic is on her at T (its piece in OutEscort): her hull rings with its pings. */
		bool Pinged(int BoatPiece, double T, int& OutEscort) const;

	private:
		/** An escort's asdic on her from From to Until. */
		struct Span { double From = 0.0, Until = 0.0; int By = -1; };
		using Hunt = std::pair<int, int>; // (escort piece, boat piece)
		/** An asdic contact over at T, its span kept by her piece. */
		static void Close(std::map<Hunt, double>& Holding, std::map<int, std::vector<Span>>& Into, const Hunt& Which, double T);
		std::map<int, std::vector<Span>> Pings; // by boat piece: each asdic contact's span, the events' and the echoes'
		std::map<Hunt, double> Open, Echoes;    // on her since: the events', the views' echoes
		std::set<int> Sunk;                     // pieces sunk
	};

	/** Her hull, or another ship's, as the eye must keep out of it: her middle (a boat's at her hull's middle depth, a
	 * ship's at the waterline), heading, length, beam, and how far down her keel and how far up her top go (Z). */
	struct Hull
	{
		int Key = -1;
		P3 At;
		double Heading = 0.0, Long = 67.0, Beam = 6.2, Bottom = -4.7, Top = 10.0;
		bool bBoat = false;
	};
	/** [P] A surface ship's draught from the models (Destroyer to z -3.6, Freighter to -7.4); others by length. */
	double DraughtOf(const std::string& Class, double Long);
	Hull HullOf(const SeaShow::ShipOut& Ship);

	/** [SHOW] Seconds after her last round a boat's gun still counts in action (she fires one about every 5 s). */
	inline constexpr double GunQuiet = 6.0;
	/** A boat up with her crew on deck, as the show has her at T: struck and up (ShipOut::Struck, from her crew's first
	 * coming out on her casing), or her deck gun in action on the surface (laid, its crew at it: ShipOut::bGunLaid; or a
	 * round fired within GunQuiet s). The eye never goes under beside such a boat: the war on her deck is the surface's. */
	bool CrewUp(const SeaShow::ShipOut& Ship, double T);

	/** A boat where the show has her at a moment, as the rules on going under beside her ask: her ship key, piece and side,
	 * her place, whether the show draws her (ShipOut::bShown: the player's own always, an enemy boat only while found),
	 * afloat (not gone), under way (in her line or limping: neither sinking nor gone), struck and up, and at her gun on the
	 * surface (the two of CrewUp). */
	struct BoatAt
	{
		int Key = -1, Piece = -1, Side = -1;
		double X = 0.0, Y = 0.0;
		bool bShown = true, bAfloat = true, bUnderWay = true, bStruck = false, bGun = false;
	};
	/** Every boat of the show now, in its own order (Out cleared first, its room kept). */
	void BoatsOf(const SeaShow::Show& Show, std::vector<BoatAt>& Out);

	/** [EST] How far from her (metres) a boat the show hides (not found) must be for the eye to go under beside her, by the
	 * water's sight in use (the "-TheaterUnderSee=" knob's, else SeeDay and SeeNight): two and a half times the farther of
	 * the two (LightLeft: under a thousandth of her light left over that) and 20 m for the eye's own stand-off from her,
	 * never under 200 m. So clearer water keeps the eye up for a boat farther off, and the murk always hides one past it. */
	double UnheldClearBy(double DaySee = SeeDay, double NightSee = SeeNight);

	/**
	 * THE DIRECTOR: which moments go under, fed the same show the scene plays (ASeaBattle::PlanShots' trial, a quarter
	 * second a step; the cloud test's, any step). Candidates: each pattern or ahead salvo on a boat, from the drop to 5 s
	 * after its last burst; a boat crash-diving deep (3 to 16 s after); a boat forced up from deep (1 to 15 s after). One
	 * boat's candidates within RunsOn s of a pattern's end run on as one; her end in it lengthens it to 8 s past (never
	 * chooses it); at most Longest s (LongestWithEnd with her end); taken by kind, then time (never by what follows), Apart s
	 * apart, at most one and one more every EveryMore s; clear of the other shots' cuts by ClearCut s (trimmed, or started
	 * after one, where enough is left); in a staged scene, inside the one beat it plays in, ScreenLeast s of screen at the
	 * least; a pattern only where one of its bursts comes within PatternSeen m of her (none farther is seen from beside her; the
	 * booked blow, which the show bursts at her keel wherever the pattern fell, is not one of its charges seen sinking);
	 * the boat drawn by the show the whole time (ShipOut::bShown: the trial is the scene's own show, SetMine and all), and
	 * no boat the show hides, nor one at her gun on the surface, within the clearance (UnheldClearBy) of her (ClearOfUnheld);
	 * her crew up on deck (CrewUp), or such a boat coming near, ends it there, the eye coming up as they do.
	 */
	class Director
	{
	public:
		/** A staged scene's beat (SeaFeed::Beat): battle seconds From to To, played over Screen seconds. */
		struct Beat { double From = 0.0, To = 0.0, Screen = 0.0; };
		static constexpr double Longest = 45.0, LongestWithEnd = 60.0, Apart = 20.0, RunsOn = 12.0, EveryMore = 150.0, ClearCut = 3.0;
		/** The least a window may be: battle seconds under, and screen seconds under in a staged scene. */
		static constexpr double ShortestUnder = 6.0, ScreenLeast = 3.0;
		/** The staged scene's pace of a beat, as ASeaBattle::RunMonthScene plays it (SeaParts::FastestWindow). */
		static constexpr double SlowestBeat = 0.25, FastestBeat = 6.0;
		/** [EST] A pattern's burst nearest her (level, metres) for the eye beside her to see its charges come down. */
		static constexpr double PatternSeen = 60.0;
		static double PaceOf(const Beat& B);

		/** InClearBy: how far a boat the show hides must be from her (UnheldClearBy, by the water's sight in use). */
		void Begin(const SeaShow::Field& InBoard, double InClearBy);
		/** Each view as the trial takes it: its time, its pieces (the escorts' echoes, Sight::Echoed) and its events. */
		void Told(double T, const std::vector<SeaShow::Piece>& Pieces, const std::vector<SeaShow::Event>& Events);
		/** One step of the trial: reads its Effects (before the caller clears them) and its ships. */
		void Step(double T, const SeaShow::Show& Trial);
		/** The windows, in battle-time order. Busy: the other shots' cut moments (battle seconds); Beats: the staged scene's
		 * (empty when watched or for pictures). */
		std::vector<Window> Finish(const std::vector<double>& Busy, const std::vector<Beat>& Beats, double BeginT, double EndT) const;
		/** The asdic's pings, from the trial's views (FSeaUnder's: the hull ringing under the water). */
		const Sight& Sighted() const { return Pings; }

	private:
		struct Attack { int Escort = -1, Boat = -1; double T = 0.0; };
		struct Burst { int Escort = -1; double T = 0.0, Depth = 0.0, X = 0.0, Y = 0.0; bool bBooked = false; };
		struct Moment { double T = 0.0; std::vector<BoatAt> Boats; };
		void Opened(Window& W, const SeaShow::ShipOut& Boat, double T) const;
		/** Whether a window, trimmed to the beats and the busy cuts, is still worth going under for. */
		bool Fits(Window& W, const std::vector<double>& Busy, const std::vector<Beat>& Beats) const;
		/** Whether what is left of it is long enough: ShortestUnder s, ScreenLeast s of screen, a pattern seen sinking. */
		static bool LongEnough(const Window& W);
		/** Whether the show lets the eye be under beside her the whole window: all of it clear at its opening, and cut short
		 * where it stops being so, the rest still long enough. */
		bool Clear(Window& W) const;
		/** The level distance from her (her ship key) to a point at a step of the trial (-1: she is not there then); OutOff, if
		 * given: the point less her place then (level). */
		double FromHer(int BoatKey, double T, double X, double Y, P3* OutOff = nullptr) const;
		SeaShow::Field Board;
		double ClearBy = 200.0;        // UnheldClearBy's, as Begin is told it
		Sight Pings;
		std::vector<Attack> Attacks;   // sea:depthCharge: escort piece on boat piece
		std::vector<Window> Candidates;
		std::vector<Burst> Bursts;     // ChargeBurst: escort ship key, when, how deep, where, the battle's booked blow (on her)
		std::map<int, double> Ends;    // a boat's ship key: she went down
		std::vector<Moment> Moments;   // each step: where every boat was
	};

	/**
	 * The one rule on what may be near a boat the eye goes under beside, for the director's windows and the player's own
	 * dive alike: no boat afloat that the show hides (not found: BoatAt::bShown, SeaShow's SetMine) nor one at her gun on
	 * the surface (BoatAt::bGun) within ClearBy m of her (UnheldClearBy, by the water's sight in use), so going under never
	 * shows a boat the escorts have not found, nor the deck gun's war. True where she is not among Boats (nothing posed,
	 * nothing near her).
	 */
	bool ClearOfUnheld(const std::vector<BoatAt>& Boats, int BoatKey, double ClearBy);
	/** The player's own dive (U): the boat to go under beside, the nearest to the eye within HandReach m that the show draws
	 * (his own, or one found), under way, her crew below (not struck and up, nor at her gun), and clear of unheld
	 * (ClearOfUnheld); -1 none, and the eye stays up. */
	inline constexpr double HandReach = 6000.0;
	int HandBoatFor(const std::vector<BoatAt>& Boats, const P3& Eye, double ClearBy);
	/** Whether his dive beside her goes on: she is not gone, still drawn by the show, her crew still below, and still clear
	 * of unheld. */
	bool HandHolds(const std::vector<BoatAt>& Boats, int BoatKey, double ClearBy);
	/** The side of her his dive takes (+1 starboard, -1 port), from the look's hash of her key. */
	int HandOf(int BoatKey);

	/** A boat as the eye needs her: her ship key, her hull's middle (Z below the sea), heading, length, beam, keel. */
	struct BoatPose { int Key = -1; P3 At; double Heading = 0.0, Long = 67.0, Beam = 6.2, Keel = 0.0; bool bThere = false; };
	struct EyePose { P3 Eye, Look; double Lens = 55.0; bool bUnder = true; };

	/** Her hull's middle below the sea (metres, positive): her keel less half her draught and casing. */
	double MiddleOf(double Keel);
	/** Her pose; there (bThere) only while she is afloat and the show draws her (bShown): the eye is never posed beside a
	 * boat the show hides, a window's eye going up over its pattern instead (EyeOf). */
	BoatPose PoseOf(const SeaShow::ShipOut& Ship);
	/** Where the eye is for a window at T, in her frame as she is now: under the water From to Until, then on the surface
	 * to HoldUntil; false outside that. Others: every hull near (hers among them is skipped), kept out of. */
	bool EyeOf(const Window& W, const BoatPose& Boat, const std::vector<Hull>& Others, double T, EyePose& Out);
	/** The player's own dive: off her side at her depth, drifting slowly round her. */
	EyePose EyeByHand(const BoatPose& Boat, const std::vector<Hull>& Others, double Since, int Hand);
	/** An eye kept under the sea (1.5 m at the least), off her hull's axis by her half beam and 6 m, and out of every
	 * other hull (pushed under its keel, smoothly, as it nears it). */
	P3 ClearOf(const P3& Eye, const BoatPose& Boat, const std::vector<Hull>& Others);
	/** Whether a point is inside a hull (with Margin metres about it): a boat's as a rod, a ship's as a box. */
	bool InsideHull(const P3& At, const Hull& H, double Margin);

	/** The light the stage's sun gives (0 below the horizon, 1 a little above it): the stage always lights the day. */
	double DaylightOf(double SunUpDeg);
	/** The water at a depth: how far it lets the eye see, its colour (linear), the sun's share left, whether shafts show. */
	struct Murk { double See = SeeDay; double R = 0.0, G = 0.0, B = 0.0; double SunShare = 0.3; bool bShafts = false; };
	Murk MurkAt(double Depth, double Daylight, double SunUp, double DaySee = SeeDay, double NightSee = SeeNight);
	/** The engine's height-fog density for it: exp2(-Density/1000 x units) left; 5 in the hundred at See. */
	double FogDensity(double See, double UnitsPerMetre);
	/** The share of light left over Metres of this water. */
	double LightLeft(double Metres, double See);

	/** The sea's face from under it: Snell's window, a disc about the eye (point 0 straight up, then CeilingRings rings of
	 * CeilingSpokes: even in angle from straight up to CeilingReach degrees, the last a skirt out at CeilingSkirt in the
	 * murk's colour). Nothing else of the face is drawn: the murk hides the sky. */
	inline constexpr int CeilingRings = 16, CeilingSpokes = 36;
	inline constexpr double CeilingReach = 62.0, CeilingSkirt = 84.0;
	struct CeilingPoint { P3 At; double R = 0.0, G = 0.0, B = 0.0; };
	void CeilingAt(const P3& Eye, double T, double Daylight, double SunUp, double SunFrom, const Murk& M, std::vector<CeilingPoint>& Out);
	/** Its ripples' shortest wavelength, and its widest cell inside the window, at a depth (metres): the one 2.5 times the
	 * other at the least, so the ripple never crawls on the mesh. */
	double RippleWave(double Depth);
	double CeilingCell(double Depth);

	/** The gas globe of a burst (Cole): its radius Age s after, below 0 once it has broken up into the rising cloud. */
	double GlobeAt(double Kg, double Depth, double Age);

	struct Drum { P3 At; double Yaw = 0.0, Tilt = 0.0; bool bBomb = false; uint64_t Key = 0; int Escort = -1; };
	struct Glob { P3 At; double R = 0.0, Alpha = 0.0; };
	struct Shaft { P3 Top, Way; double Long = 0.0, Wide = 0.0, Strength = 0.0; };
	struct Creak { double T = 0.0; int Which = 1; double Gain = 1.0; bool bRattle = false; };
	/** What is drawn in one frame (laid afresh each frame into the same lists, their room kept). Air: her air going up once
	 * she is lost, apart from the bursts' bubbles (it shares their budget). */
	struct Frame
	{
		std::vector<Drum> Drums;
		std::vector<P3> Motes;
		std::vector<Glob> Globes, Bubbles, Flashes, Air;
		std::vector<Shaft> Shafts;
	};
	void MotesAt(const P3& Eye, double T, std::vector<P3>& Out);
	void ShaftsAt(const P3& Eye, double T, double SunUp, double SunFrom, const Murk& M, std::vector<Shaft>& Out);

	/** A boat as a charge told on her needs her: her keel's place now and her way (metres a second). */
	struct BoatNow { P3 At, Vel; };

	/** What is in the water about the eye: the charges and bombs sinking, each burst's flash, globe and bubbles, her air
	 * once she is lost, the silt, the shafts; the shock of each burst in the eye and in her hull; the hull's creaks. */
	class Water
	{
	public:
		static constexpr int MostMotes = 160, MostGlobes = 12, MostDrums = 40, MostShafts = 6, MostBubbles = 240;
		static constexpr double MoteBox = 24.0;
		/** The water's sight by day and by night (the "-TheaterUnderSee=" knob's, else SeeDay, SeeNight): its murk is laid by them. */
		void Begin(const SeaShow::Field& InBoard, double InDaySee = SeeDay, double InNightSee = SeeNight);
		/**
		 * Every frame: the charges the show lists going down. Each is drawn from where it was first seen going in, sinking
		 * at the show's rate to where it bursts; where the battle later moves its burst (a blow told on her: it bursts by her,
		 * Off from her, SeaShow.cpp's sea:depthChargeHit), it is eased there over the rest of its fall from where it is
		 * drawn then, never in a lunge. Boats: by piece, for the charges told on them.
		 */
		void Remember(double T, const std::vector<SeaShow::Show::ChargeDown>& Down, const std::map<int, BoatNow>& Boats);
		/** The charges as drawn at T (every one listed, in key order). */
		void Charges(double T, std::vector<Drum>& Out) const;
		/** A burst at the battle's place. An ahead salvo's (one bursts only on her hull) takes the nearest of its bombs still
		 * sinking, of those down to within AheadAbove m of its depth or past it; where none is yet (a hit told sooner than any
		 * could sink there: a timing the rules decide), it bursts at the battle's place alone and no bomb with it. */
		void Burst(double T, const P3& At, const std::string& What);
		static constexpr double AheadAbove = 5.0;
		void AheadDropped(double T, const ConvoyLook::DropIn& In, uint64_t Seed, double Deepest);
		/** The ahead salvos' bombs still sinking at T, as drawn (wherever the eye is), in the order thrown. */
		void AheadSinking(double T, std::vector<Drum>& Out) const;
		/** A boat's end (her ship key): her air goes up from her, and only from her, once it has come (the first told kept). */
		void Ended(double T, int BoatKey) { Ends.emplace(BoatKey, T); }
		/** The ahead bursts told before any bomb could have sunk to within AheadAbove m of them (a timing the rules decide),
		 * each burst at the battle's place alone (Burst). */
		int AheadEarly() const { return AheadTooSoon; }
		/** The frame about the eye (Out's lists cleared, their room kept); Boat: the one posed (her air once she is lost). */
		void Lay(double T, const P3& Eye, const BoatPose* Boat, double Daylight, double SunUp, double SunFrom, Frame& Out) const;
		P3 ShakeAt(double T, const P3& Eye) const;
		double ShudderAt(double T, const P3& HerAt) const;
		void CreaksBetween(double T0, double T1, double Since, uint64_t Seed, double Keel0, double Keel1, const P3& HerAt, double& LastAt, std::vector<Creak>& Out) const;

	private:
		struct Sinking
		{
			P3 From;               // where it is drawn from (first where it went in, Z 0), at FromT
			double FromT = 0.0, In = 0.0, Bursts = 0.0;
			P3 To;                 // where it bursts (a charge told on her: Off from her)
			bool bOnBoat = false;
			int Boat = -1;
			P3 Off;
			int Escort = -1;
			bool bLive = true;     // still listed this frame
		};
		static uint64_t KeyOf(const SeaShow::Show::ChargeDown& C, int Again);
		P3 Target(const Sinking& S, double T) const;
		P3 DrawnAt(const Sinking& S, double T) const;
		Drum DrumOf(uint64_t Key, const Sinking& S, double T) const;
		struct Blow { double T = 0.0; P3 At; double Kg = 0.0; uint64_t Key = 0; };
		struct Bomb { double In = 0.0; P3 Entry; double Gone = 0.0, BurstT = -1.0; uint64_t Key = 0; };
		SeaShow::Field Board;
		double DaySee = SeeDay, NightSee = SeeNight;
		std::map<uint64_t, Sinking> Seen;
		std::map<int, BoatNow> BoatsNow;
		std::vector<std::pair<uint64_t, int>> Alike; // (Remember's: charges told alike so far this frame, kept for its room)
		std::vector<Blow> Blown;
		std::vector<Bomb> Bombs;
		std::map<int, double> Ends; // a boat's ship key: she went down
		int AheadTooSoon = 0;
	};
}
