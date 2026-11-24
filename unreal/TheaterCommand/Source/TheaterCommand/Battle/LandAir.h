#pragma once

#include "Battle/LandFeed.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * Ground-attack fighters over the land battle, drawn (Effects, release 2, behind -TheaterLandAirSupport; Brian's ask
 * through the coordinator, 23 November: the fighters on call "fired on" from the ground, "rarely shot down", and they
 * "crash"). Plain C++, no engine: the game draws what it says (ALandBattle, its Effects (R2) hook), and the cloud plays
 * it through Tools/land_air.
 *
 * The rules decide everything (src/battle/land/sim.ts, air support; LandFeed::AirNote): which side calls a strike and
 * when, each leg's run down a strip of the field (land:air-in, then land:strafe: from, to, its width, how long in battle
 * seconds), its bombs (land:bomb-drop; each burst a shell of calibre bomb, which the scene draws as ever), and the
 * planes it cost (air:lost: explode, burn or spin), as the run ends. Nothing here changes any of it. What is decided
 * here is only what the rules leave open: how a leg's planes fly their run, which of them is the one lost, where the
 * ground's guns stand and where their streams go, and how a plane falls and where it strikes.
 *
 * A leg's planes come in one behind another down the line of its run, high, push over into a dive aimed at the middle
 * of their lane, their guns opening at the machine's range from the lane's start and walking it to its end (the art
 * director's notes of 23 November, 19:08Z: a fighter from 600 to 1000 m in a 20 to 30 degree dive, guns from about
 * 900 m to 250 m, out at the bottom 30 to 60 m up; a Stuka from high in a 75 degree dive, its two guns a short burst,
 * out at about 300 m; an Il-2 low and shallow, out at 15 m), pull out, climb away and break left and right in turn,
 * each 6 to 12 s behind the one before, so that each dive is its own beat. A lane starts 150 m past its own side's
 * front at the least. They fly at a real machine's speed along their way (105 to 140 m/s), not the rules' picture's (120
 * board metres a battle second): the rules' run is seconds of the battle, its planes' pass seconds of life. The ground's
 * guns are the blocks of the other side nearest the run, two guns a block, in ragged bursts, their streams aimed short
 * of the lead, so that the tracers fall behind. A plane lost is the last of its leg not yet lost, hit in its pass or as
 * it climbs away; it blows up (its pieces and its fuel come down burning), or goes down on fire in a long shallow dive,
 * or spins in (about 3 s from 150 m), and burns where it struck.
 *
 * Places are board metres (x east, y north, z up), headings degrees clockwise from north, as the feed's. Times: battle
 * seconds outside; a plane flies on the real seconds since its leg came over (the battle's times Life, the scene's own).
 */
namespace LandAir
{
	struct V3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};

	using GroundFn = std::function<double(double X, double Y)>;

	/** An air force's machine for the ground attack in a year, as the miniature kit names it, and how it flies it. */
	struct Machine
	{
		std::string Model;              // the kit's model ("Mustang", "Stuka"; "Fighter" a plain one in the nation's paint)
		double Span = 11.0;             // metres
		double Speed = 130.0;           // metres a second over the ground in the attack
		double High = 800.0, Low = 45.0; // its height before the dive and at the bottom of its pull-out, metres
		double Dive = 25.0;             // the dive's angle, degrees
		double Climb = 25.0;            // the climb away, degrees
		double Open = 900.0, Shut = 250.0; // the slant ranges its guns open at (to its lane's start) and shut at (to its end)
		double PushG = 1.5, PullG = 4.0; // into the dive and out of it: the push-over's and the pull-out's g
		std::string Gun = "hmg";        // its guns, as the kit's air_guns rows name them
		double Rate = 20.0;             // rounds a real second drawn from each wing
	};
	Machine MachineOf(const std::string& Nat, int Year);

	enum class Fall : uint8_t { None, Explode, Burn, Spin };

	/** One plane as it is drawn now. */
	struct Plane
	{
		int Key = 0;                    // its leg's number times 8 and its place in the leg: its guns, trail and fall go by it
		int Leg = -1, Slot = 0, Side = -1;
		std::string Nat, Model, Gun;
		double Span = 11.0;
		V3 At, Vel;                     // board metres; metres a real second
		double Heading = 0.0, Pitch = 0.0, Bank = 0.0; // degrees: clockwise from north, nose up, right wing down
		bool bFiring = false;
		V3 Muzzle[2], Aim;              // its wings' guns, and the ground they rake
		Fall How = Fall::None;          // going down, how; None while it flies
		bool bWreck = false;            // down, burning where it struck (drawn nose in, the rest of the battle)
	};

	/** A gun on the ground firing up at a strike now: where it stands and where its stream goes. */
	struct Gunner
	{
		int Key = 0;
		V3 From, To;
	};

	/** What happens to a plane between two frames: hit (its fall begins there), or into the ground. */
	struct Happen
	{
		enum class What : uint8_t { Hit, Crash } Kind = What::Hit;
		int Key = 0;
		double T = 0.0;                 // battle seconds
		Fall How = Fall::None;
		V3 At, Vel;
		double Span = 11.0;
	};

	/**
	 * A downed pilot under his parachute, then down on the field (Effects, release 2: -TheaterLandChutes with
	 * -TheaterLandAirSupport; Brian, 23 November 20:09Z). The rules say whether he got out (air:lost's chutes: never
	 * from a plane blown apart); here only how. He goes over the side a second or two after the hit (a spinning plane's
	 * pilot sooner) and falls free a second or so, never rising (only a zooming plane throws a man up), his way off her
	 * dying as a falling man's does; then he pulls, and his canopy fills over the next 45 to 50 m of his fall and takes
	 * the rest of his way, so that he drops about 50 m before the wind takes him. He comes down at 5 to 6.5 m/s (a
	 * heavy man 6 to 7) drifting with the battle's wind, swinging under it about 4 s a swing, from 14 degrees down to a
	 * gentle 5. Out under 60 m he pulls at once and his canopy is still filling when he hits: a hard landing. Down, he
	 * goes over flat under his falling canopy, and the scene swaps him there for a man of the crowd (in his army's tank
	 * crews' file where it has one, else its infantry's, bareheaded); in a wind over 4 m/s his canopy drags him a metre
	 * or two; he lies a moment (after a hard landing 3 to 5 s), gets up onto his knees and works his harness free (1 to
	 * 1.5 s), stands and runs 20 to 40 m from the nearest enemy (after a hard landing limping, slower and not so far),
	 * and goes to ground there for the rest of the battle; his canopy falls in downwind of him and lies there. (The art
	 * director's notes of 24 November, 02:26Z.)
	 */
	struct Chute
	{
		int Key = 0;                    // his plane's
		int Side = -1;
		std::string Nat;                // his air force's (his army's soldier file, on the ground)
		bool bDown = false;             // on the ground
		V3 At;                          // his feet (in the air, as he hangs in his harness 1.1 m over them)
		V3 Vel;                         // metres a real second
		double Open = 0.0;              // his canopy: 0 still packed (falling free), 1 full; down, as it was as he landed
		double Swing = 0.0;             // degrees: his swing under it, nose of his drift up
		double Heading = 0.0;           // degrees: the way he drifts (in the air), the way he faces (down)
		double Face = 0.0;              // down: the way he faced as he landed (degrees), as he lies and where his harness lies
		// Down: his army's clip, how far into it (real seconds) and how fast it goes on now (its seconds a real second;
		// less than nothing, played back): go_prone held as he lies (and is dragged), go_prone played back as he gets up
		// onto his knees, kneel_down held at its end as he works his harness free, stand_up, run (after a hard landing
		// slowed, with a hitch), go_prone where he goes to ground, held. Empty while his chute's mesh draws him. Blend:
		// how long the scene takes to blend into his clip from the one before (real seconds).
		std::string Clip;
		double Time = 0.0, Rate = 1.0, Blend = 0.25;
		// His canopy: up over him (Slump 0) to fallen in and flat on the ground (1), where it lies then (Heap, its
		// middle) and the way it lies from where its lines end (degrees); Landed, where they end: him as he lies, dragged
		// with it in a wind, else where he landed
		V3 Heap, Landed;
		double HeapHeading = 0.0, Slump = 0.0;
		// (the art director's notes, 24 November 02:26Z) Whether his chute's mesh draws him: in the air, and down until
		// he is swapped for the crowd's man as he lies under his falling canopy; there, how far he has gone over from his
		// feet to flat (0 to 1). A hard landing (out too low for his canopy to fill); how far his canopy has dragged him
		// (metres). His canopy's silk (linear red, green and blue: her off-white, each a little warmer or cooler), its
		// gores (24 to 28), and its size in the air (metres across its skirt, and how deep).
		bool bInMesh = true, bHard = false;
		double Fallen = 0.0, Dragged = 0.0;
		double Silk[3] = { 0.86, 0.85, 0.80 };
		int Gores = 26;
		double Across = 5.2, Deep = 2.5;
	};

	/** A pilot's lines: his harness hangs this far under his canopy's skirt, and his feet HangFeet under that (metres;
	 * the scene's mesh hangs on it, so that his feet are At). */
	constexpr double LinesLong = 4.1, HangFeet = 1.1;

	/**
	 * Down, the scene's chute mesh draws him going over flat before the crowd's man takes him (the art director's third
	 * note, 24 November 02:26Z: nobody is to see him pop). He pitches OverPitch degrees about his feet, his hips OverHip
	 * up him, and is moved so that, flat, his hips lie where the crowd's man's do. That man is a rider, posed by go_prone
	 * held at its end with nothing taken back (LandMen's StepRider: only the crowd's own men have go_prone's end offset
	 * taken back as it plays), so his hips are go_prone's end offset forward and to the right of his seat, ProneHips up
	 * (every soldier file's end_offset is 0.59 m forward and 0.14 m right, its pelvis lying 0.14 m up, all within a
	 * centimetre; the test reads them). ProneEndX and ProneEndY are that offset until the scene knows his file, then his
	 * file's own. (A review of 24 November: moved back 0.9 m, he had lain 0.7 m short of the crowd's man, who then
	 * jumped forward in plain view under the canopy.)
	 */
	constexpr double OverPitch = 84.0, OverHip = 0.8, ProneEndX = 0.595, ProneEndY = 0.137, ProneHips = 0.143;
	/** How far the mesh's man is moved as he goes over (metres: forward of his feet the way he faces, to his right, up),
	 * at Fallen (0 on his feet, 1 flat), for go_prone's end offset EndX forward and EndY right. */
	V3 OverShift(double Fallen, double EndX, double EndY);
	/** Where his hips are then (metres: forward, right and up of his feet); flat, EndX, EndY and ProneHips. */
	V3 OverHips(double Fallen, double EndX, double EndY);

	/**
	 * A canopy's silk as the scene draws it (Effects, R2; the art director's sixth note, 24 November 02:26Z), in the
	 * chute mesh's own space: metres, x the way he drifts, y to his right, z up. Each point has its facing and its
	 * colour (linear red, green and blue); each triangle three points and the way it faces out (the scene winds it by
	 * that). Both faces of the silk are drawn, so that it is seen from under it.
	 */
	struct SilkMesh
	{
		std::vector<V3> Points, Facings;
		std::vector<double> Colours;    // three a point
		std::vector<int> Triangles;     // three a triangle
		std::vector<V3> Outward;        // one a triangle
		std::vector<int> Seams;         // its skirt's point at each gore's seam (its outer face's): where a line ends
		void Clear();
	};
	/** His canopy in the air, its origin the middle of its skirt: Open 0 a streamer of silk over him, to 1 a full dome
	 * Across wide and Deep deep, its gores' seams faint darker lines. */
	void CanopyShape(const Chute& C, double Open, SilkMesh& Out);
	/** His canopy once he is down, its origin where its lines end (his feet), x the way it falls (the wind's): from as it
	 * was as he landed (Slump 0) it falls in downwind and settles into a low rumpled heap Off metres off (1), its 7 m of
	 * silk lying on the field. */
	void HeapShape(const Chute& C, double Slump, double Off, SilkMesh& Out);
	/** Down, whether a line runs back to him from the Seam-th of Seams seams (SilkMesh::Seams' order): the half of its
	 * skirt behind its middle the way it falls, toward where its lines end. Chosen by its place round the skirt, not by
	 * where its rumple puts it, so that the same lines are drawn all the while it falls in (the mesh keeping its make). */
	bool LineBack(size_t Seam, size_t Seams);

	/**
	 * The scene's chute mesh (the art director's sixth note, 24 November 02:26Z: cheaply, no mesh made again a frame) is
	 * made once for each make it has (falling free, him alone; under his canopy, him, his lines and its silk; down, the
	 * same going over; once the crowd's man has him, his lines and its silk) and otherwise moved in place where it can
	 * be, its points only (UpdateMeshSection: no new proxy, its storage kept). Its silk is built again only when its shape
	 * has moved by a step: Shape (his canopy's open share in the air; 2 and its slump once down) by OpenStep in the air,
	 * SlumpStep down, or to full or flat, or down from the air (SilkMoved). In the air it is moved in place: a dome
	 * filling never folds. Down it is made again, and by a coarser step: the scene winds each triangle by where its
	 * points are, and the heap's rumple folds some over as it falls in (about one in fifty within a step, its crown's
	 * and its hem's most; a sixth by the time it lies flat), which kept as first wound would show their inside's
	 * shading. His part (him going over and his lines) is built again when Lying (how far over he has gone in it, from 0
	 * on his feet to 1 flat; less than nothing once the crowd's man has him) has moved by LyingStep, or to flat, or he is
	 * handed over (ManMoved), or with the silk; it is moved in place while its make is the same. The Was values are
	 * those last built (-1 and -9: none yet).
	 */
	constexpr double OpenStep = 0.02, SlumpStep = 0.05, LyingStep = 0.05;
	bool SilkMoved(double Shape, double WasShape);
	bool ManMoved(double Lying, double WasLying);
	/**
	 * How much a canopy at Canopy glows seen from Eye, the sun the way TowardSun goes (all three in one frame, any
	 * units): its silk lets the light through, so its side turned from the sun gives back a share of its own colour, a
	 * little always (seen from under it) and most when it stands against the sun; none at night. The scene gives it to
	 * the silk's own ink as the pieces' FigureFill, by its key light against a clear day's.
	 */
	double Glow(const V3& Eye, const V3& Canopy, const V3& TowardSun);

	/** The kit's numbers for a plane's guns (Shots), a ground gun's, and a plane's trail, fall and fire (one each). */
	inline int GunIdOf(int Key, int Wing) { return 90000 + (Key % 2400) * 2 + (Wing & 1); }
	inline int GunnerIdOf(int Key) { return 95000 + Key % 4800; }
	inline int FireIdOf(int Key) { return (1 << 25) + Key % 1000000; }

	class Sky
	{
	public:
		/** Plans every leg the notes name: its planes' ways, the ground's guns, its losses' falls. Life: real seconds a
		 * battle second (the scene's). The guns stand where the other side's blocks stand at the leg's coming (the
		 * recording's views). */
		void Build(const LandFeed::Recording& Rec, const GroundFn& Ground, double Life);
		/** The notes the plan was made from (a live battle's grow: Build again when more come). */
		size_t Notes() const { return NotesN; }
		size_t Legs() const { return Plans.size(); }
		/** Every plane drawn at battle time T (none before its leg comes or after it has gone, but its wreck), and the
		 * ground's guns firing then. */
		void At(double T, std::vector<Plane>& OutPlanes, std::vector<Gunner>& OutGuns) const;
		/** What happened in (From, To], in time's order. */
		void Between(double From, double To, std::vector<Happen>& Out) const;
		/** Every downed pilot out of his plane by battle time T: under his canopy, or down (Chute). */
		void Chutes(double T, std::vector<Chute>& Out) const;
		/** The pilots who get out (the rules' chutes), planned. */
		size_t ChutesPlanned() const;
		/** Rounds a gun firing Rate a real second fires in (From, To] (by the battle's clock: the same however the
		 * frames fall). */
		int Rounds(double From, double To, double Rate) const;

	private:
		struct Sample
		{
			double T = 0.0;             // real seconds since its leg came
			V3 At;
			double Heading = 0.0, Pitch = 0.0, Bank = 0.0;
		};
		struct Down
		{
			Fall How = Fall::None;
			double Hit = 0.0, Crash = 0.0; // real seconds since its leg came
			std::vector<Sample> Path;   // hit to crash (none for an explosion: its pieces fall in the kit)
			V3 HitAt, HitVel, CrashAt, CrashVel;
			// (its pilot, where the rules say he got out: real seconds since its leg came he leaves her, his canopy is
			// full, his feet are down; his way down, each sample's Pitch his canopy's open share; then on the ground: how
			// long he lies, how long he runs, where to, and the way he faces as he lands; where his canopy comes to lie)
			bool bChute = false;
			double Out = 0.0, Opened = 0.0, Landed = 0.0;
			std::vector<Sample> Drop;
			double Lie = 2.0, RunS = 0.0, Face = 0.0, RunHeading = 0.0;
			V3 LandAt, RunTo, Heap;
			// (the art director's notes, 24 November 02:26Z: his canopy's look, as Chute's, and which way its swing starts;
			// its open share as he landed, and whether he landed hard; on the ground, real seconds from his landing: his
			// going over flat, his swap for the crowd's man, his being dragged (and where to), his getting up (when, and
			// how long onto his knees) and his harness's release; whether he limps, and his run's clip's pace)
			double Silk[3] = { 0.86, 0.85, 0.80 };
			int Gores = 26;
			double Across = 5.2, Deep = 2.5, SwingSide = 1.0;
			double OpenDown = 1.0;
			bool bHard = false, bLimp = false;
			double FallS = 0.5, SwapS = 0.9, DragS = 0.0, Rise = 0.0, UpS = 1.0, ReleaseS = 1.2, Pace = 1.0;
			V3 DragTo;
		};
		struct Burst
		{
			double On = 0.0, Off = 0.0; // real seconds since its leg came
			double Across = 0.0, Up = 0.0, Lead = 0.5;
		};
		struct Post
		{
			int Key = 0;
			V3 At;
			std::vector<Burst> Bursts;
		};
		struct Plan
		{
			int Flight = -1, Side = -1, Models = 4, Year = 1944;
			std::string Nat;
			Machine M;
			double Tin = 0.0;           // battle seconds: over the start of its run
			double Run[4] = { 0.0, 0.0, 0.0, 0.0 };
			double Len = 1.0, Fx = 0.0, Fy = 1.0, Head = 0.0; // the run's length, way (unit) and heading (degrees)
			double Width = 60.0, Base = 0.0;
			double Gap = 6.0, Pass = 4.0; // real seconds between planes, and each plane's pass (guns open to guns shut)
			// Its way in (metres along the run from its start, heights over Base): level at High from Ua, the push-over
			// from Uq into the dive, the dive down its line (aimed at its lane's middle), the pull-out to the bottom at Uc.
			double Ua = 0.0, Uq = 0.0, Uc = 0.0;
			double Us = 0.0;            // where its guns' lane starts (at least 150 m past its own side's front)
			double Gam = 0.4, Rq = 1000.0, Rp = 600.0; // the dive's angle (radians), the push-over's and the pull-out's radii
			double Seg[4] = { 0.0, 0.0, 0.0, 0.0 }; // the level, the push-over, the dive and the pull-out, metres of way
			double Sf0 = 0.0, Bottom = 0.0; // metres along its way in where its guns open; seconds from then to the bottom
			std::vector<Down> Falls;    // per slot (How None: not lost)
			std::vector<Post> Posts;
		};

		/** Its way in, S metres along it: where it is along the run (U), its height over Base (H) and its pitch (radians,
		 * nose up). Before its start it is level; past the bottom, at the bottom. */
		static void WayIn(const Plan& P, double S, double& U, double& H, double& Pitch);
		/** A plane of a leg in flight at Tau (real seconds since its leg came): false where it is not drawn. */
		bool Fly(const Plan& P, int Slot, double Tau, Sample& Out) const;
		void Flown(const Plan& P, int Slot, double Tau, Plane& Out) const;
		void PlanFall(Plan& P, int Slot, Fall How, double Tau);
		void PlanChute(Plan& P, int Slot, const V3& Wind);

		std::vector<Plan> Plans;
		GroundFn GroundAt;
		double Life = 2.0;
		size_t NotesN = 0;
	};
}
