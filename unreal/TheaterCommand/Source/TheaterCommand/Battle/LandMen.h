#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * The men of a ground battle, in plain C++: every soldier the squad brain sends (TC.battleMen, or a recorded battle's
 * `men` tracks) played as a man of the soldier files, so the close camera sees men who move, kneel, take cover, fire
 * and fall as men do (Brian, 21 November 2026: "as if I'm watching Saving Private Ryan", a feeling to aim for and
 * nothing more; "when a soldier gets hit have their body be on the ground dead", "i want it to be realistic").
 *
 * The battle decides everything that counts: where each man is, what he is doing, when he fires and when he falls
 * (the rules' bridge, "The men"; the land handoff, section 9). Nothing here changes that. What is decided here is only
 * what the rules leave open: which clip shows a pose, how one pose turns into the next (a man kneels down, goes prone,
 * gets up), which fall a dead man takes, his face and his helmet's finish.
 *
 * The soldier files are the soldiers thread's (battle-models/land/soldier/README.md; Content/Models/Soldiers): one
 * skeleton of 32 bones, three levels of detail, 19 clips sampled 15 times a second, sockets, faces and helmet finishes.
 * Their axes are the man's own: x forward, y to his RIGHT, z up, metres, feet on z = 0. Everything this file hands out
 * is in the field's frame: x east, y north, z up, metres (the recording's and the bridge's), headings in radians
 * clockwise from north. In that frame the triangles wind counter-clockwise seen from outside (the right-hand rule
 * gives the outward facing), as the files' own normals say.
 *
 * Costs, for the lead's frame: Update poses every living man (a few microseconds each); Skin is the real cost and is
 * called only for the men drawn, at the level of detail LodOf gives. A dead man is skinned once when he comes to rest
 * and never again (his Stamp stops changing): bodies stay to the end of the battle and cost nothing a frame.
 *
 * No Unreal types: the scene (the lead's) copies what Skin writes into its own buffers. Tools/land_show/run.sh plays
 * the Normandy recording through this file in the cloud and checks it.
 */
namespace LandMen
{
	struct Vec3
	{
		float X = 0.f, Y = 0.f, Z = 0.f;
	};

	/** A turn, [w, x, y, z] as the files and Unreal's FQuat write it. */
	struct Quat
	{
		float W = 1.f, X = 0.f, Y = 0.f, Z = 0.f;
	};

	/** A place and a turn: p goes to R * p + T (R row by row). */
	struct Xform
	{
		float R[9] = { 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f };
		float T[3] = { 0.f, 0.f, 0.f };

		Vec3 Point(const Vec3& P) const;
		Vec3 Turn(const Vec3& V) const;
	};

	/** a then b: (a * b)(p) = a(b(p)). */
	Xform Compose(const Xform& A, const Xform& B);

	constexpr int MaxBones = 48;
	constexpr int LodCount = 3;
	enum Lod : int { Near = 0, Mid = 1, Far = 2 };

	/**
	 * The paints, by number, as every soldier file numbers them (build_tank.py's FIGURE_PAINTS: "Paint numbers and the
	 * unit table are unchanged"). armies.json names its colours by these words.
	 */
	const std::vector<std::string>& PaintNames();

	/** One level of detail of a mesh, as the file has it: one entry per point (paints, part, part2, w2 have as many
	 * entries as the points have xyz triples). */
	struct Mesh
	{
		int Points = 0;
		std::vector<float> At;          // xyz, the rest pose
		std::vector<float> Facing;      // xyz, unit normals
		std::vector<uint8_t> Paint;
		std::vector<uint8_t> Part, Part2;
		std::vector<float> W2;          // part2's weight, 0 to 0.5; part takes the rest
		std::vector<float> Shade;       // 0 to 1, wear and seams; empty: every point 1
		std::vector<float> Tint;        // rgb a point, about 1 (the soldier files' "tint": mud browner, dust paler; shade only darkens); empty: every point 1
		std::vector<int16_t> Ink;       // a helmet finish's: -1 the paint, else an index into the file's inks; empty: all -1
		std::vector<uint32_t> Triangles;
	};

	struct Bone
	{
		std::string Name;
		int Parent = -1;
		Vec3 Head;
	};

	struct Socket
	{
		std::string Name;
		int Bone = -1;
		Vec3 At, Facing;
	};

	/** A clip's event: the effect, the sound director's cue (empty: none), where (a socket or a bone, or the man). */
	struct ClipEvent
	{
		float T = 0.f, Until = -1.f;
		std::string Fx, Cue, SocketName, BoneName;
		int Socket = -1, Bone = -1;
		bool bFirstLoopOnly = false;
	};

	/** One bone's keys in a clip: times, and turns or moves. */
	struct Keys
	{
		std::vector<float> T;
		std::vector<Quat> Rot;
		std::vector<Vec3> Move;
	};

	struct Clip
	{
		std::string Name, Weapon;
		float Long = 0.f, Stride = 0.f;
		bool bLoop = false;
		bool bEndOffset = false;
		Vec3 EndOffset;                 // where the pelvis ends (go_prone and the falls): x forward, y right
		std::vector<int> RotOf, MoveOf; // per bone: its keys in Rots / Moves, or -1 (the bone keeps its rest turn)
		std::vector<Keys> Rots, Moves;
		std::vector<ClipEvent> Events;
		float FlashT = -1.f;            // the first muzzle_flash's time (the shot), -1 none
		float ReleaseT = -1.f;          // grenade_release's, -1 none
		float LeftFoot = -1.f;          // the left foot's (or elbow's) step as a share of the loop, for keeping step
		/** Release 2's run styles (the soldiers thread's run_styles.py): the clip this one is another runner's take of
		 * ("run" for run_b, run_c, run_d; "advance" for advance_b...), empty for any other clip. */
		std::string StyleOf;
		/** A tank clip's places (its "places"): the vehicle model (Piece, its file's name: "Sherman") and its socket his
		 * root goes to, moved by Offset (forward, right, up of the socket's facing). Empty for a man on his feet. */
		struct Seat
		{
			std::string Piece, Socket;
			Vec3 Offset;
		};
		std::vector<Seat> Seats;
		/** The seat on that model, or none. */
		const Seat* SeatOn(const std::string& Piece) const;
		/**
		 * A prop's keys the clip gives (release 2's field clips, the soldiers thread's: "reel", the cable reel's root and
		 * his two fists on its handles; "hands", the spliced wire's two ends in his fists; "grips", the stretcher's): its
		 * fields after the time ("t" left out), in his root's frame (his own axes: x forward, y to his right, z up,
		 * metres), at each key's time T, Fields.size() values a key.
		 */
		struct PropKeys
		{
			std::string Name;
			std::vector<std::string> Fields;
			std::vector<float> T, Values;
			/** Its fields at clip time At (a loop's wrapped round its Length, else held at the ends), into Out. */
			void Sample(float At, bool bLooped, float Length, float* Out) const;
		};
		std::vector<PropKeys> Props;
		/** The clip's prop keys by name, or none. */
		const PropKeys* PropOf(const std::string& Track) const;
		float Speed() const { return Long > 0.f && Stride > 0.f ? Stride / Long : 0.f; }
	};

	/** A face (about_variety): moved points and new facings at each level, the skin's tone, the hair's tint. */
	struct Face
	{
		std::string Name;
		Vec3 Tone { 1.f, 1.f, 1.f };
		struct Level
		{
			std::vector<int> At;
			std::vector<float> Move, Facing;
			std::vector<int> TintAt;
			std::vector<float> Tint;
		} Lods[LodCount];
	};

	/** A helmet finish: its own rigid mesh at each level, and when, where and how often it was worn. */
	struct Finish
	{
		std::string Name;
		int From = 0, To = 0;           // months as year * 12 + month - 1
		std::string Season, Front;
		double Share = 0.0;
		Mesh Lods[LodCount];
	};

	/** One soldier file (soldier_<ARMY>.json), read once. */
	class Soldier
	{
	public:
		bool Load(const std::string& Text, std::string& OutError);

		std::string Name;
		/** The gun his mesh carries: "rifle" unless the file says (a gunner's file would say "lmg"). */
		std::string Weapon = "rifle";
		std::vector<Bone> Bones;
		std::vector<Socket> Sockets;
		std::vector<Clip> Clips;
		Mesh Meshes[LodCount];
		std::vector<Face> Faces;
		std::vector<Finish> Finishes;
		std::vector<uint32_t> Inks;     // 0xRRGGBB
		/** Paints past the army's fifteen that the file names (its new_paints), each with the file's own colour. */
		std::map<int, uint32_t> OwnPaints;
		int HelmetFirst[LodCount] = { 0, 0, 0 }, HelmetCount[LodCount] = { 0, 0, 0 };
		/** deaths[stance][kind] = clip names, when the file has the map (the physics-baked falls). Empty: the README's
		 * three falls by the table. */
		std::map<std::string, std::map<std::string, std::vector<std::string>>> Deaths;
		/** deaths_gore[stance][kind] = clip names: a gore twin's falls (soldier_<ARMY>_gore.json: the same bones, its skin
		 * split at the eight cuts, shoulder, elbow, hip and knee), in which a limb is taken off at its joint and flies on
		 * its own bone's move track. Empty but in a twin (Crowd::AddGore). */
		std::map<std::string, std::map<std::string, std::vector<std::string>>> DeathsGore;
		/** A gore twin's cuts (its gore.cuts, in order: bit k of a joined point's mask), each by its bone, and its
		 * caps' points per level ([first, count], the stump's and the limb's). Empty but in a twin. A cut is off where
		 * its bone has left the joint its parent holds (more than a centimetre). */
		struct Cut
		{
			std::string Bone;
			int BoneIx = -1;
			int Stump[LodCount][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 } }, Limb[LodCount][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
		};
		std::vector<Cut> Cuts;
		/** A gore twin's joined skin (its gore.joined, per level): the points its hard weights at the cuts would crack
		 * open at a bent joint, with the man's own weights, skinned so while none of the cuts in its mask is off
		 * (the art director, 18:34Z: the caps hidden in him till his limb goes). */
		struct Joined
		{
			std::vector<uint32_t> Point;
			std::vector<uint8_t> Part, Part2;
			std::vector<float> W2;
			std::vector<uint16_t> Mask;
		};
		Joined Joins[LodCount];
		/** Per clip, the clip and its styles (its own first, then each clip whose StyleOf names it, in the file's order):
		 * run, run_b, run_c, run_d. Empty for a clip with no styles. */
		std::vector<std::vector<int>> Styles;

		int ClipIndex(const std::string& Name) const;
		int BoneIndex(const std::string& Name) const;
		int SocketIndex(const std::string& Name) const;
		/** Bones whose parent is the root (the pelvis and the weapon): moving them moves the whole man. */
		const std::vector<int>& RootChildren() const { return OnRoot; }
		/** The weapon bone and the bones on it (the bolt, the bipod): the rifle, rigid. */
		bool OnWeapon(int Bone) const { return Bone >= 0 && Bone < static_cast<int>(GunBones.size()) && GunBones[Bone]; }
		int WeaponBone() const { return Gun; }
	private:
		std::vector<int> OnRoot;
		std::vector<bool> GunBones;
		int Gun = -1;
	};

	/** A pose: each bone's turn and move from its rest, as a clip keys them. */
	struct Pose
	{
		Quat Rot[MaxBones];
		Vec3 Move[MaxBones];
	};

	void RestPose(Pose& Out);
	/** The clip at a time: a loop wraps, a one-shot holds its ends. Keys between are lerped, turns slerped. */
	void Sample(const Clip& C, float Time, Pose& Out);
	/** A to B by W (0 all A, 1 all B): turns slerped the short way. Blends of a fifth of a second use this. */
	void Blend(const Pose& A, const Pose& B, float W, Pose& Out);
	/** Each bone's frame in the man's own axes: a rest-pose point p on bone b goes to Out[b].Point(p) (the files' rule:
	 * G = G_parent * q, X = X_parent + G_parent * (head - head_parent + move), p -> X + G (p - head)). */
	void BoneFrames(const Soldier& S, const Pose& P, Xform* Out);

	/** The ground under a point: its height and its unit normal (the lead's terrain, or the test's). */
	struct Ground
	{
		float Height = 0.f;
		Vec3 Normal { 0.f, 0.f, 1.f };
	};
	using GroundFn = std::function<Ground(double X, double Y)>;

	/**
	 * A seat in a vehicle's bed: a lorried man aboard (the rules' pose "mounted") was drawn kneeling on the ground where
	 * the rules seat him, so under his lorry (Brian, 23 November 14:10Z: "troops are stuck under the truck"). The scene
	 * says where his bench is: its top (field metres, its height the field's), the bed's floor beside it, and the way he
	 * faces on it (radians clockwise from north, as a man's heading). Inboard: the bench faces the bed's middle; a man
	 * past the benches kneels in the bed's middle at its front, facing aft.
	 */
	struct SeatPlace
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		double Floor = 0.0;
		float Facing = 0.f;
		bool bBench = true;   // false: no bench left for him (the bed's middle)
	};
	/** The seat for a man aboard: his place (the rules', on his vehicle's bed), his block, and the rules' vehicle (her k in
	 * that block) and seat in her bed (two abreast, the odd ones on her right, front to back), each -1 where the rules
	 * did not say; false where no vehicle carries him. Asked from several threads at once, as GroundFn is. */
	using SeatFn = std::function<bool(double X, double Y, int Block, int SeatOn, int Seat, SeatPlace& Out)>;
	/** The stone a man is never drawn inside (the field's walls, Battle/LandStone.h): for a man at rest at the rules'
	 * place X, Y, facing Facing (radians clockwise from north) and lying as far as Lie (0 on his feet or his knee, 1
	 * prone), the step from that place to where he is drawn, his body and his weapon clear of the stone. bStood: he has
	 * his answer already and stands, or is stepping to, StoodX, StoodY (the last answer's place): he keeps the side of a
	 * wall he chose. False where he is clear as he stands. Asked from several threads at once, as GroundFn is. */
	using StoneFn = std::function<bool(double X, double Y, float Facing, float Lie, bool bStood, double StoodX, double StoodY, double& OutDx, double& OutDy)>;
	/** One of several that stand as one thing by a wall (Crowd::AddStoneTeam): a man, his reach by how far he lies
	 * (Lie), or his weapon's own mount (bPiece: Fore, Aft and Aside her reach from her place); where the rules have
	 * them and the way they face (radians clockwise from north). */
	struct StoneMember
	{
		double X = 0.0, Y = 0.0;
		float Facing = 0.f, Lie = 0.f;
		bool bPiece = false;
		double Fore = 0.0, Aft = 0.0, Aside = 0.0;
	};
	/** The one step for several that stand as one thing (Them[0] leads; Battle/LandStone.h's ClearAll): as StoneFn,
	 * the step they stand at already given as a step (bStood, StoodDx, StoodDy). OutHow: 0 they are clear as the
	 * rules have them, 1 one step clears them all, 2 the men are clear (by a step, or as they are) and her mount is
	 * left in the stone, 3 no step clears the men and none is taken. Asked from several threads at once. */
	using StoneTeamFn = std::function<bool(const StoneMember* Them, int Count, bool bStood, double StoodDx, double StoodDy, double& OutDx, double& OutDy, int& OutHow)>;

	/**
	 * Where a man stands: his root on the field (x, y), his heading, and how far his body lies along the ground's slope
	 * (0 upright, as a man stands on a hillside; 1 lying, his body laid on the slope). The pivot (his own axes, on z = 0)
	 * is where the turn onto the slope is made: his root, or where a dead man's pelvis came to rest.
	 */
	struct Placing
	{
		double X = 0.0, Y = 0.0;
		float Facing = 0.f;
		float Lie = 0.f;
		Vec3 Pivot;
		/** Metres over the ground (release 2: a wounded man carried on a stretcher, its canvas's height). */
		float Lift = 0.f;
	};
	/** A wounded man carried on a stretcher lies this high over the ground (metres): the soldiers thread's stretcher
	 * clips' four fists at 0.83 m on the mean, the Stretcher prop's root 0.14 m under them and its `lie` 0.11 m over it. */
	constexpr float CarriedLift = 0.80f;
	/** The field from the man's own axes for that placing (it reads the ground at the pivot; OutGround, when given,
	 * takes the ground's height read there). */
	Xform WorldOf(const Placing& P, const GroundFn& GroundAt, float* OutGround = nullptr);

	/**
	 * Skins one mesh by bone frames that already include the man's world (Compose(World, Frame) per bone): positions
	 * and unit facings, three floats a point, into the caller's buffers (M.Points * 3 floats each; Facings may be null).
	 * Rest points may be swapped for a face's (RestAt / RestFacing, null for the mesh's own).
	 */
	void SkinMesh(const Mesh& M, const Xform* Frames, float* OutAt, float* OutFacing, const float* RestAt = nullptr,
		const float* RestFacing = nullptr);

	/** What a round was fired with, as the rules name it (rifle, mg, at, grenade, mortar). */
	enum class Weapon : uint8_t { Rifle, Mg, At, Grenade, Mortar, Other };
	Weapon WeaponOf(const std::string& W);
	const char* WeaponName(Weapon W);

	/**
	 * Something the lead's sound and effects play: a clip's event (footstep, elbow_down, knee_down, body_fall,
	 * kit_rattle, rifle_drop, bolt, grenade_throw), a shot (muzzle_flash, one a round and one a bullet of an mg burst,
	 * the cue on the first; grenade_release; mortar for a mortar crew's round, cue mortar_fire), or slump (a man killed
	 * lying goes limp where he lies: no fall clip, so no body_fall; no cue, the lead may give it a soft one).
	 * At is the world point (the socket's, the bone's, or the man's), Dir the socket's facing (the muzzle's for a shot).
	 * Army is the man's own (USA, GER), for the nation's rifle and bolt. A shot's T is its round's own time (it may
	 * fall a little before the frame that sends it); the others are the frame's.
	 */
	struct Event
	{
		double T = 0.0;
		int Man = -1;
		std::string Fx, Cue;
		std::string Army;
		Weapon Gun = Weapon::Other;
		int Round = 0;                  // a burst's bullet, 0 the first (the one with the cue)
		Vec3 At, Dir;
	};

	/** A man of the crowd, as the recording or the bridge gives him, and what was settled for his looks. */
	struct ManInfo
	{
		int Block = 0, K = 0;
		int J = 0;                      // a follower of key man K in his section (the drill's sections), 0 the key man
		std::string Role, Army, FileArmy;
		bool bFo = false;
		double Dead = -1.0;             // battle time he fell (-1 never)
		double Out = -1.0;              // battle time he left with his block (-1 never)
		int Face = 0;
		int Finish = -1;                // -1: his own helmet
		double Delay = 0.0;             // his own small lag behind the recording, seconds
		float Jitter = 1.f;             // his own play rate on still loops and turns of pose
		std::string FallClip;           // the fall he took ("settle": a prone man going limp)
		std::string FallStance, FallKind, FallFrom; // stand|run|kneel|prone|cover; the kind; front|back|side
		double FallStarted = -1.0;      // when his fall began, as played
		double Rested = -1.0;           // when he came to rest and froze
	};

	/** His drawn look at one level of detail: the triangles, each point's paint and its colour (sRGB, 0 to 255). */
	struct Look
	{
		int Points = 0;
		std::vector<uint32_t> Triangles;
		std::vector<uint8_t> Paint;
		std::vector<uint8_t> Rgb;
	};

	/** A clip and how it plays: what a man shows now (for tests, pictures and the lead's debug lines). */
	struct Playing
	{
		std::string Clip;
		float Time = 0.f, Rate = 0.f;
		bool bGait = false, bCapped = false, bBlending = false, bShooting = false;
		float Speed = 0.f;              // metres a battle second over the ground
		float Lie = 0.f;
		bool bRifleDown = false;        // his rifle laid down by him (a spotter, a radio man, a served weapon's gunner)
	};

	/** The front a place on the earth is dressed for when the battle's file does not say ("desert", "china", "pacific",
	 * "south", "east" or "west"): the armies' colours and finishes are picked by it. Longitude is east positive, and may
	 * be given past the date line either way. */
	const char* FrontAt(double Lat, double Lon);

	class Crowd
	{
	public:
		Crowd();
		~Crowd();
		Crowd(const Crowd&) = delete;
		Crowd& operator=(const Crowd&) = delete;

		/** The soldier file drawn for an army's men (by the file's army: "GER", "POL" for the Americans until theirs). */
		void AddSoldier(const std::string& FileArmy, std::shared_ptr<const Soldier> S);
		/**
		 * Effects (R2): the gore twin (soldier_<ARMY>_gore.json) of the file added under that name, after it (AddSoldier).
		 * The crowd keeps the twin's mesh, faces, helmets and gore falls with the file's own clips and sockets, so a man
		 * swapped to it mid-battle keeps every clip he had. False, and why: no such file, its bones not the file's, or no
		 * gore falls in it (deaths_gore).
		 */
		bool AddGore(const std::string& FileArmy, const Soldier& Twin, std::string& OutWhy);
		/**
		 * Shell and tank hits throw men and take limbs off (release 2, behind -TheaterLandGore; Brian, 22 November: "make
		 * more graphic animations if a guy gets shot or blown up", "with limbs coming off"). On: a man killed by a shell,
		 * a tank's gun or a grenade near its burst may fall as his file's gore twin (AddGore), in one of its gore falls:
		 * blown off his feet with a limb gone, an arm or a leg taken off. The odds are by the burst (a field gun's 0.55 at
		 * the burst, a tank's 0.5, a mortar's 0.45, a grenade's 0.3) and fall away to none at its reach (12, 10, 8 and 4
		 * m); a man lying or in cover half that. The same battle gores the same men every time (his own dice). Who dies
		 * and when are the battle's; only how he falls changes. Off unless set: every man falls as before.
		 */
		void SetGore(bool bOn);
		/**
		 * Men who leave with their block walk or run off (release 2, the scene's trial word "leave"; Brian, 25 November
		 * 17:11Z: "some soldiers freeze and then disapear"). A living man on foot whose block leaves the battle was read
		 * as standing still from his last kept point, so he stopped dead, mid-stride if he was moving, and was cut half
		 * a second later. On: he goes on from his last kept point along his last stretch at its speed, or, if he was
		 * still, gets up, turns and walks for his own side's edge; in a gait all the way; whole for 3 s after he is out
		 * and gone half a second later. The scene hides him sooner, the first frame he is out of the picture (Leaving,
		 * LeftUnseen). Dead men, men aboard, riders and vehicles as before. Off unless set: every man as before.
		 */
		void SetLeaveWalk(bool bOn);
		/** A living man on his way off with his block (SetLeaveWalk), still drawn. */
		bool Leaving(int Man) const;
		/** A leaving man gone out of the picture: not drawn again, unless his block comes back on the list. */
		void LeftUnseen(int Man);
		/** Men who fell as their file's gore twin so far in this play. */
		int Gored() const;
		/** Changes when his mesh becomes another (a gore fall, or back at a replay from the start): his triangles and
		 * colours are made again (LookOf), so a drawn man's must be too. */
		uint32_t MeshNo(int Man) const;
		/** Content/Data/LandBattles/armies.json: each army's colours and its helmet rule. */
		bool ReadArmies(const std::string& Text, std::string& OutError);

		struct Options
		{
			/** The front for the helmet finishes (east, west, south, desert, pacific, china); empty: from the
			 * recording's place (sky.latLon). */
			std::string Front;
			/** Book the recording's rounds as the men's shots (Fire), and read its kills for the falls. */
			bool bReadRounds = true;
		};
		/** A recorded battle (Content/Data/LandBattles/<key>.json): its men's tracks, its blocks' armies, its date. */
		bool Build(const std::string& RecordingText, GroundFn GroundAt, std::string& OutError);
		bool Build(const std::string& RecordingText, GroundFn GroundAt, const Options& With, std::string& OutError);
		/**
		 * A live battle's next delta (src/battle/land/live.ts LiveDelta), after Build from its header (no men yet): new
		 * tracks become new men as Build makes them (appended: a man's number never changes, and Find finds him), new
		 * kept points and changes go on their tracks (a provisional last point, `tails`, gives way to them), new rounds
		 * are booked as the men's shots and read for the kills, d and out set his fall and his leaving; `reset` drops
		 * every man and begins again. A battle grown so plays as the same battle read whole while Update stays 2 battle
		 * seconds behind the latest delta, as the scene runs. At 1 second behind the places, shots and falls are still
		 * the same, but a turn of pose is fitted into 0.6 of the time to his next change, which must then be known: going
		 * prone (1.9 s of life, 1 battle second at Life 2) wants it 1.7 s ahead, else he takes the turn at its own pace
		 * where the battle read whole hurries it. A bazooka's reload after his rocket (9 s of life) is squeezed before
		 * his next round in the same way and wants that round up to 4.5 s ahead. Tools/land_live/men.sh checks it.
		 * Costs, on the game thread: some 2 ms for a delta of 1.7 battle seconds of Normandy's two thousand men (5 at
		 * most), well under 1 for one of half a second; the first, where every man is new (some 9 KB of state each),
		 * about 13 ms: a loading moment.
		 */
		bool Extend(const std::string& DeltaText, std::string& OutError);

		/**
		 * Riders: a tank's commander up in his hatch (the soldiers thread's soldier_USA_tanker and soldier_GER_panzer and
		 * their tank clips). He is no track of the recording's: the scene says each frame, before Update, whether he is
		 * shown, where his root goes (the floor at his feet: the vehicle's commander socket moved by the clip's
		 * root_offset) and which clip he plays how far in (SetRide); he is drawn, skinned and given his level of detail
		 * as every man is, never banked, never fades, fires no shot and sends no event. While a clip whose weapon is
		 * "none" plays (every tank clip: his gun is put away 2 m under his feet), his gun's bones are not drawn. FileArmy: his soldier file's
		 * ("USA_tanker"); Army: his army's colours ("USA"); Seed: his own (his face). Row: armies.json's row he is painted
		 * by where not his army's own ("UK@desert", "ITA_desert": RiderFileOf's). Appended after the men there are (call
		 * after Build); -1 where the file was not added.
		 */
		int AddRider(const std::string& FileArmy, const std::string& Army, uint64_t Seed, const std::string& Row = std::string());
		/**
		 * The soldier file a rider of that army wears and the colours row he is painted by (OutRow), as a rifleman of
		 * the army is dressed (MakeMan): the army's own file, else armies.json's stand-in, else the Pole's; in its desert
		 * or winter dress where made; the front's row ("UK@desert") or the dress's own ("ITA_desert"). Empty where no file
		 * draws him. Release 2's light cars and motorcycles: their drivers and riders, from the army's infantry file.
		 */
		std::string RiderFileOf(const std::string& Army, std::string& OutRow) const;
		struct Ride
		{
			bool bShown = false;
			/** His own axes on the field (x forward, y right, z up, as WorldOf makes a man's), at his root. */
			Xform Seat;
			int Clip = -1;          // his soldier file's clip (Soldier::ClipIndex)
			float Time = 0.f;       // into it: a loop wraps, a one-shot holds its ends
			/** The clip he is leaving, blended out as he takes up Clip: W 0 all FromClip, 1 all Clip (no blend). */
			int FromClip = -1;
			float FromTime = 0.f;
			float W = 1.f;
			/** Effects (R2): his gun never drawn, whatever his clip holds (a tank's crew bailing out in the tankers'
			 * infantry clips, which carry a rifle; the art director, 23 November). False: as before. */
			bool bUnarmed = false;
			/** Effects (R2): his root (bone 0) Clip's alone while FromClip blends out: the soldiers' bail clips move
			 * it, and his seat moves on with them where each ends (a crewman bailing out). False: as before. */
			bool bOwnRoot = false;
		};
		void SetRide(int Man, const Ride& R);
		bool IsRider(int Man) const;
		/** The soldier file added under that name (AddSoldier), or none. */
		const Soldier* SoldierFile(const std::string& FileArmy) const;

		/**
		 * A round a man fires (the rounds' t, w, and by/k mapped to the man by Find). His shot is played so the flash
		 * (or a grenade's release) falls exactly on T: while he holds a firing pose he keeps its aim between shots and
		 * plays the clip's shot (recoil, bolt) around each round; an mg round is one burst of 6 to 10. Book a round at
		 * least a second ahead where you can (a grenade's throw starts 0.95 s before its release); a late one plays
		 * from where it would be. Build books the recording's own.
		 */
		void Fire(int Man, double T, Weapon W);
		/** The clip's own muzzle flashes back on (every firing pose loops its clip and shows its shots), for a test. */
		bool bClipShots = false;

		/** Where the eye is this frame (field metres), for the levels of detail. Zoom: how much bigger the lens draws a
		 * man than the usual one does (tan of half the usual 50 deg over tan of half this lens's, both vertical): the
		 * levels go by his distance over Zoom, so a long lens's men 120 m off are drawn at the middle level. */
		void SetEye(double X, double Y, double Z, double Zoom = 1.0);
		/** Where the men aboard vehicles sit (SeatFn): set once; without it a man aboard kneels where the rules have him. A
		 * file with a seated clip (car_ride_back, release 2's riders) sits him on the bench; one without kneels him on the
		 * bed's floor beside it. */
		void SetSeats(SeatFn Fn);
		/**
		 * The stone no man is drawn inside (StoneFn; a mend of 27 November 2026: a man whose place the rules set in a
		 * wall's thickness was drawn in the stone, all of him hidden but what stood out of its faces). Set once. A man
		 * at rest on the field (not aboard, not carried) is drawn at the step it gives from the rules' place. He takes
		 * it as he comes in to his rest, where his track shows the rest ahead (StoneAhead, StoneStay): over the time a
		 * walk takes to make it, and for the way he will face and the stance he will have as he settles there
		 * (StoneSettle), so he arrives at the clear place and does not slide to it as he holds his rest pose (the art
		 * director, 27 November 2026); where it does not (a stop the rules had not yet told of) he takes it as he
		 * settles. Never faster than a walk (StoneSpeed), or, while his legs carry him faster, than he is going
		 * himself (the art director, 27 November 2026, 23:35Z: a man who dashes to his rest veers to his place as he
		 * runs). Where he stops he stays while that place is clear: a step
		 * moves at rest only as he turns, goes down, or must still come out of the stone (a bound of a few tenths of
		 * a second from one place by a wall to another leaves a walk too little time). A man told to go down is asked
		 * for as a lying man from that moment, one who pivots where he stands as he will face (StoneTurn). The step
		 * is given back at a walk as he moves off; one who starts the battle in a wall starts clear of it. A man who
		 * falls lies where he was drawn. PlaceOf, PlaceAsPosed and every frame of his are at the drawn place;
		 * StoneStep gives the step itself. Unset (the default): where the rules have him.
		 */
		void SetStone(StoneFn Fn);
		/** How far a man is drawn from the rules' place now to stand clear of stone (SetStone): zeros where he is not. */
		void StoneStep(int Man, double& OutDx, double& OutDy) const;
		/** A man slower than this is at rest (metres a battle second), and is stepped clear of stone once he has been
		 * for StoneDwell (battle seconds: a walking man between two kept points of a live battle is not at rest). */
		static constexpr float StoneStill = 0.3f;
		static constexpr double StoneDwell = 0.1;
		/** How fast he takes the step (metres a battle second, as the field sees him go): a walk; faster only while
		 * he himself is moving faster on his way, and then no faster than he goes. */
		static constexpr double StoneSpeed = 1.4;
		/** How far ahead on his track a man on his way is looked for a rest (battle seconds), and how long he must
		 * stay at a place for it to be one (a halt of a moment between two bounds is not). */
		static constexpr double StoneAhead = 2.2;
		static constexpr double StoneStay = 0.4;
		/** How far into his stay his facing is read for the step he comes in to (battle seconds): he turns to his front
		 * where he stands in his first moments there, and the step is the one for the way he then faces. */
		static constexpr double StoneSettle = 0.6;
		/** A man who bounds from one place by a wall to another keeps the side of it his step has him on, where that
		 * is no more than this much further than the nearest clear place at his new rest (metres). */
		static constexpr double StoneKeepFar = 1.0;
		/** A man slower than StoneStill who has crept this far from where he took his step no longer stands at it: he
		 * is asked afresh (metres). */
		static constexpr double StoneCrept = 0.5;
		/** A man at rest who turns this fast or faster where he stands (radians a battle second) pivots: the walls are
		 * asked for him as he will face when the pivot ends, or StoneSettle on if it lasts longer, so that he is not
		 * stepped off for every facing he sweeps on the way round. A slower turn is asked for as he faces. */
		static constexpr double StoneTurn = 0.2;
		/**
		 * A gun and the men whose hands are on her are one thing (the art director, 27 November 2026, 20:54Z and
		 * 21:10Z). SetStoneTeam: the one step for several, set once with SetStone. AddStoneTeam: a gun served by
		 * Gunner (her key man). As he comes in to a rest, stops there or starts the battle there, the walls are
		 * asked for him, for her and for every man of his team (his block and k) whose own track has him stay
		 * within StoneTeamReach of that place then, or brings him to stay there within StoneTeamAhead, together:
		 * one step. Each of those men comes in to it as he comes in to his rest there, and stands at it with his
		 * gunner from then on (when the gunner turns or goes down and steps on, they step with him). No man slides
		 * for it as he holds a rest pose: one who stood there on a step of his own before the gun came keeps it
		 * (and the gun has hers), and one her gunner's step does not clear (he came
		 * later than StoneTeamAhead, or he faces another way than his gun: the team's step is for men who face as
		 * she does) steps on from it by himself, as a rifleman does. His other crew step each on his own, but never
		 * on to her ground (within StoneTeamReach of where her gunner or her mount stand) nor before her muzzle
		 * (within StoneMuzzleFar of her mount and StoneMuzzleCos of her facing: her traverse and a little): one
		 * whose step would take him there stays where the rules have him. A team is kept a little clear of the
		 * stone (LandStone's AsOneClear), so that a man at her whose facing is a hair off his gunner's is clear at
		 * her step by his own asking; and a team that stands with her mount left in the stone (bodies before metal)
		 * stays so while its men are clear, and is asked for with her again only when the men must step. Ahead,
		 * Right: where her mount's origin stands from her gunner (metres along his facing and to his right); Fore,
		 * Aft, Aside: her reach from it. Once for a gun; again changes her numbers.
		 */
		void SetStoneTeam(StoneTeamFn Fn);
		void AddStoneTeam(int Gunner, double Ahead, double Right, double Fore, double Aft, double Aside);
		static constexpr double StoneTeamReach = 1.5;
		static constexpr double StoneTeamAhead = 20.0;
		static constexpr double StoneMuzzleFar = 10.0;
		static constexpr double StoneMuzzleCos = 0.866;
		/** For the tests and the log: the guns told of (in AddStoneTeam's order) and each one's gunner; how the walls
		 * answered for a gun at the last Update (StoneTeamFn's OutHow, or -1 where her gunner was on his way with
		 * no rest ahead); whether a man stood at, or was coming in to, his gun's step then (a man of her crew at
		 * her); and whether he was held to the rules' place off her ground. */
		int StoneTeamCount() const;
		int StoneTeamGunner(int Team) const;
		int StoneTeamHow(int Team) const;
		bool StoneAsOne(int Man) const;
		bool StoneKeptOff(int Man) const;
		/** Whether a man is aboard a vehicle now (the stance he shows), and the rules' word for it: his block, her k in it
		 * and his seat in her bed (each -1 where the rules did not say). */
		bool AboardOf(int Man, int& OutBlock, int& OutSeatOn, int& OutSeat) const;
		/** Plays every man to the battle's time. Forward only: going back starts over from the beginning, silently. */
		void Update(double BattleTime);
		/**
		 * Thrifty Updates, for a crowd of hundreds (the drill's sections): a man away from the eye is posed (his bones,
		 * and so his Stamp and his skin) only every few frames, PoseEvery: every frame near the eye (the near level) and
		 * while he falls, every 2nd frame to 50 m, every 3rd to 120 m, every 4th beyond. All else he does every frame:
		 * his place, his turns of pose, his shots on their rounds' times and his clips' events. Off by default (every
		 * man posed every frame, as before); the scene turns it on.
		 */
		void SetThrift(bool bOn);
		/** Runs Count jobs at once where it can, Job(K) for each K from 0 to Count - 1, and returns when all are done
		 * (the scene's ParallelFor). */
		using RunFn = std::function<void(int Count, const std::function<void(int)>& Job)>;
		/**
		 * Update steps the men over the cores through Run, a run of men each, where there are some hundreds: a man's step
		 * reads only his own state and what the crowd holds still meanwhile (the tracks, the soldiers, the bank and the
		 * GroundFn, which must bear being asked from several threads at once), so every man ends as he would one after
		 * another, to the bit, and the events come in the same order. Unset (the default): one after another, as before.
		 */
		void SetRun(RunFn Run);
		/**
		 * Life seconds a battle second: the rules' clock runs faster than life (about twice: a man rushing at 6 m/s, a
		 * tank across a field at 10), so the scene plays the fight at half the battle's clock. The clips are life's: a
		 * man goes prone, reloads, loops his idle and falls in his clip's own seconds, 1 / Life of them on the battle's
		 * clock, and a burst's bullets come ten a second of life. His gait stays on the ground (the clip at his speed);
		 * 1, the default, is as before. Set before Build (the shots are planned there).
		 */
		void SetLife(double LifePerBattleSecond);
		/**
		 * Uniforms that age (release 2, Brian's 23 Nov "Do all of these"; before Build): on, each side's cloth faded by its
		 * months in the line (the recording's sides.att.worn and sides.def.worn, the campaign's count; a year and a half
		 * the most), and some of its near men in a newer tunic or trousers or with a patch at an elbow or a knee.
		 * ForcedMonths at 0 or more stands in for both sides' (the Mac's off/on line). Off unless set.
		 */
		void SetWorn(bool bOn, double ForcedMonths = -1.0);
		/**
		 * Run styles (release 2, Brian's 23 Nov: "not all players are running exactly the same at the same time"): on,
		 * each man runs and advances as his own one of the file's runners (its run and the run's styles, run_b, run_c,
		 * run_d: other men's motion capture, each with his own stride, so at one speed each steps at his own beat),
		 * chosen by his seed, the same runner whether he runs or advances. His feet stay on the ground as before (the
		 * clip at his speed over its own). Far men banked in poses show the base clip's. Any time: a man takes his runner
		 * at his next change of gait. Off unless set (each man the file's own run, as before).
		 */
		void SetRunStyles(bool bOn);
		/** The detail levels' bounds, in metres as the picture sees him (SeenDistance, a 50 degree lens's): Near under
		 * NearTo, Mid under MidTo, Far beyond, each crossed back with a twentieth's give. 25 and 120 unless set; the scene
		 * sets them by his height on the screen (the battle-scale plan's GS3). */
		void SetLevels(double NearTo, double MidTo);
		/** Every how many frames he is posed now (1 when not thrifty). */
		int PoseEvery(int Man) const;
		/** This Update's events, in time order. */
		const std::vector<Event>& Events() const { return Outbox; }

		int Count() const;
		/** The man of block b with model k (a spotter's k is 200 or 201), follower j of his section (0 the key man; the
		 * drill's sections), or -1. */
		int Find(int Block, int K, int J = 0) const;
		const ManInfo& Info(int Man) const;
		/** Drawn at all (false only once a man who left with his block has faded out). */
		bool Visible(int Man) const;
		/** 1 drawn whole; less while he fades out after leaving with his block. Never less for a dead man. */
		float Fade(int Man) const;
		/** At rest for good (a body): his mesh never changes again. */
		bool Frozen(int Man) const;
		/** Falling now (his fall's clip, or a man killed lying going limp): every frame of it is drawn. */
		bool IsFalling(int Man) const;
		/** He serves a support team's weapon that the scene draws at his place (Battle/LandTeamGun; release 2, on trial
		 * "teamguns"): 1 lying behind his machine gun, 2 kneeling or sitting at it or at his mortar. Either way his rifle
		 * is laid down and his hands are on her while his pose is `fire` or `deployed` (the soldiers' own clips where his
		 * file has them: hmg_fire_prone, hmg_fire_sit, mortar_sight). 0, as he is made: his role's stance as before.
		 * Said when his weapon is first drawn; his stances are taken again from then. */
		void Serve(int Man, int How);
		/** Changes whenever his mesh would: skin him again only when it moved. */
		uint32_t Stamp(int Man) const;
		Lod LodOf(int Man) const;
		/** His distance from the eye at the last Update as the picture sees it (over SetEye's Zoom), metres. */
		float SeenDistance(int Man) const;
		/** Points a skin of him writes at that level (the body, then his helmet's finish). */
		int PointsOf(int Man, Lod L) const;
		/** His triangles and colours at that level (the same all battle, read once, unless his MeshNo changes). */
		const Look& LookOf(int Man, Lod L);
		/**
		 * Skins him at that level: PointsOf * 3 floats each into OutAt and OutFacing (Facing may be null). Different men
		 * may be skinned at once from different threads (between Updates, nothing else of the crowd's called meanwhile):
		 * a skin reads only tables made in Build and writes only the man's own. The one exception is a body's first skin
		 * at a level, which lays him on the ground through the GroundFn: skin those (SkinReadsGround) on one thread
		 * unless the GroundFn is safe to call at once.
		 */
		int Skin(int Man, Lod L, float* OutAt, float* OutFacing);
		/** His next Skin at that level reads the ground (a body laid on it, once a level). */
		bool SkinReadsGround(int Man, Lod L) const;
		/**
		 * The ground changed round X, Y out to Reach metres (a shell hole torn there): the bodies lying on it are laid
		 * again on the ground as the GroundFn now gives it, their frames and each level's skin once more (their next
		 * Skin reads the ground), so a hole torn under a man who already lay there leaves him on its rim, not in it.
		 * Returns how many; their Stamp moves on. Call it between Updates, as Skin.
		 */
		int Unsettle(double X, double Y, double Reach);
		/** His muzzle (socket_muzzle) on the field now, and where it points. */
		bool Muzzle(int Man, Vec3& OutAt, Vec3& OutDir) const;
		/** Any socket of his on the field now. */
		bool SocketAt(int Man, const std::string& Socket, Vec3& OutAt, Vec3& OutDir) const;
		/** Where he stands now (his root on the ground) and his heading. */
		bool PlaceOf(int Man, double& OutX, double& OutY, double& OutZ, float& OutFacing) const;
		/** As PlaceOf, but his height the ground's under him as he was last posed (a thrifty far man's some frames ago,
		 * a falling one's under his pivot), so no ground is asked: for a frame's loops over every man. A few centimetres
		 * from PlaceOf's at most on a slope; the same as it where he was posed this frame standing. */
		bool PlaceAsPosed(int Man, double& OutX, double& OutY, double& OutZ, float& OutFacing) const;
		/** The clip he shows now, for tests and debug lines. */
		Playing PlayingOf(int Man) const;
		/** How far he lies now (PlayingOf's Lie, without the clip's name). */
		float LieOf(int Man) const;
		/** Prop keys Track of the clip he shows now (Clip::Props: "reel", "hands", "grips") at its time now: each field
		 * after the time into Out, in his own axes (WorldFrame takes them to the field). False: he is not on his feet in
		 * a clip that has them. */
		bool PropAt(int Man, const std::string& Track, std::vector<float>& Out) const;
		/** A prop he holds, on the field now: prop keys Track of the clip he shows (a rider's the clip his seat gives
		 * him, SetRide) with a place (x, y, z) and a turn (qw, qx, qy, qz) in his own axes, taken to the field as his
		 * WorldFrame is (place its mesh as a banked man's, x forward). False: his clip has no such keys, or its `shown`
		 * is 0 now (put away), or he is not shown. The tank commander's field glasses: "binoculars". */
		bool PropFrame(int Man, const std::string& Track, Xform& Out) const;
		/** His bone frames on the field now (the soldier's bones in order), for tests and pictures. */
		const Xform* FramesOf(int Man) const;
		const Soldier* SoldierOf(int Man) const;

		/**
		 * The pose bank for far men (the true-scale plan's SO1; Content/Models/Soldiers/pose_bank.json, from the soldiers
		 * thread's battle-models/land/soldier/pose_bank.json): 112 poses (70, then the gaits' in-betweens), each a clip
		 * at a phase of its length (0 to 1), the same names for every army, with each clip's cycle of them and the clip to
		 * borrow from where a file lacks one (`instead`). Read it before Build or after: each soldier file's far mesh (with or without his own
		 * helmet, as his men need) is baked once, at Build, and the bank is on.
		 *
		 * While it is on, a living man at the far level (Far: seen 120 m off, SeenDistance) shows the bank's pose nearest
		 * to where his clip has got to (a loop's phase the circle round; a one-shot's held at its end once past it; of two
		 * clips blending, the one more than half in; the firing clip's while a shot is more than half shown): his bones
		 * are that pose's as baked (its rifle-down copy where he holds his rifle laid down), nothing is sampled, and he
		 * is placed every frame (thrifty frames too). Nothing of his play changes: his place, his turns of pose, his
		 * shots and his clips' events run on the same clocks; only what he looks like between the bank's poses, and no
		 * blend out there. A clip with no
		 * cycle (its `instead` neither) and going prone (it slides him) are posed and skinned as before. Falls are not
		 * banked: a far man's fall plays as before, and he freezes at its end.
		 *
		 * To draw far men as instances: men with the same BankKey look alike out there (the same far variant, helmet
		 * finish and army colours; the file's own face): one mesh for each (key, pose) shown, BankMesh's points with
		 * BankLook's triangles and colours, placed by WorldFrame. While he is banked his Stamp moves only when his pose
		 * changes (his place is WorldFrame's, every frame), so skin him (Skin, as before) only when BankPose is -1.
		 * Returns false (and the bank stays as it was) if the text is not a bank.
		 */
		bool ReadPoseBank(const std::string& Text, std::string& OutError);
		/** On (as soon as a bank is read) or off: off, every man is posed and skinned as before. */
		void SetPoseBank(bool bOn);
		bool PoseBankOn() const;
		/** The bake's cost, milliseconds, all files; and each file's (by the file's name). */
		double PoseBankMs() const;
		const std::map<std::string, double>& PoseBankMsByFile() const;
		/** The bank's poses: how many, and pose k's clip and phase. The second half are the first half's poses with his
		 * rifle laid down (as a gun's crew and the spotter hold their aim): pose k + BankPoses() / 2 is pose k so. */
		int BankPoses() const;
		const std::string& BankClip(int K) const;
		float BankPhase(int K) const;
		bool BankRifleDown(int K) const;
		/** The bank's pose he shows this frame, or -1 while he is skinned (near, mid unless SetPoseBankMid, falling, at
		 * rest, no bank, or a clip the bank lacks). */
		int BankPose(int Man) const;
		/** His baked set (men alike out there), or -1 with no bank; and how many sets the crowd has. */
		int BankKey(int Man) const;
		int BankKeys() const;
		/**
		 * Set Key baked in pose Pose: the far points and facings in his own axes (metres; as Skin writes them at Far
		 * before his place: x forward, y to his RIGHT, z up), the body then his helmet's finish, BankLook(Key).Points
		 * each. False where the set's file lacks that pose's clip. His own axes are left-handed and the field's right-
		 * handed, so WorldFrame holds a mirror (its determinant is -1); BankLook's triangles face outward once it is
		 * applied, as Skin's do. To instance without a mirror, flip y in the mesh (and the triangles' winding) and turn
		 * him by WorldFrame's matrix times that flip, a plain turn.
		 */
		bool BankMesh(int Key, int Pose, std::vector<float>& OutPoints, std::vector<float>& OutFacings) const;
		/** Set Key's triangles and colours at the far level (his look with the file's own face). */
		const Look& BankLook(int Key) const;
		/** His own axes to the field's as last placed (field = R * own + T, R row-major): his heading and, lying, the
		 * ground's slope under him. */
		const Xform& WorldFrame(int Man) const;

		/**
		 * The bank in vertex-animation form (SO1 as the Mac study wants it): one instanced component for each body set and
		 * level, its mesh the set's body at rest (any pose of BankBody will do, the material moves every point), and each
		 * instance's pose read by its material from one buffer of every bank pose (BankBody) by the instance's own
		 * numbers (BankShow: two poses and a share, so he goes smoothly between them; BankShowBefore for TSR). Helmets are
		 * instances of their own, one finish each (HelmetLook, HelmetMesh), placed by HelmetFrame. Bodies at rest stay
		 * skinned (Frozen: one skin, then never again).
		 *
		 * Mid men too, when asked (SetPoseBankMid; off by default, so a mid man is skinned as before until the draw side
		 * takes them): a man at the mid level then shows the bank's poses as a far one does, his mid mesh baked as the far.
		 */
		void SetPoseBankMid(bool bOn);
		bool PoseBankMid() const;
		/** The level he shows the bank's poses at (his LodOf: Mid or Far), or -1 when BankPose is -1. */
		int BankLevel(int Man) const;
		/** His body set: men drawn alike out to 25 m but for helmet and face (one soldier file, with his own helmet or with
		 * none where a finish is drawn, one army's colours); -1 with no bank. And how many sets the crowd has. */
		int BodyKey(int Man) const;
		int BodyKeys() const;
		/** Points of set Key's body at level L (Mid or Far); 0 where that level is not baked. */
		int BankBodyPoints(int Key, Lod L) const;
		/**
		 * Every pose of the bank, set Key's body at level L, in one buffer, pose-major: pose k's point p at
		 * (k * BankBodyPoints + p) * 3, metres in his own axes as BankMesh gives them (left-handed: see BankMesh), for
		 * BankPoses() poses; a pose the file lacks (BankHas false) is all zero. False where the level is not baked.
		 */
		bool BankBody(int Key, Lod L, std::vector<float>& OutPoints, std::vector<float>& OutFacings) const;
		bool BankHas(int Key, int Pose) const;
		/** Set Key's body at level L: triangles, paints and colours (the army's colours times shade and tint), the file's
		 * own face, so the skin (paint 7) is at tone 1: FaceTone gives each man's. */
		const Look& BankBodyLook(int Key, Lod L) const;
		/** The mesh he shows now: bake A to bake B by W (B's share; A == B and W 0 for a single pose); A -1 when he is
		 * not banked (BankPose -1). BankPose is the nearer of the two. */
		struct Show
		{
			int A = -1, B = -1;
			float W = 0.f;
		};
		Show BankShow(int Man) const;
		/** The last Update's (for TSR's previous position): A -1 if he was not banked then. */
		Show BankShowBefore(int Man) const;
		/** His face's skin tone: multiplies the skin's colour (paint 7), the faces being folded out of the body sets. */
		Vec3 FaceTone(int Man) const;
		/** The bone every helmet finish of set Key's file rides (the head), and its frame in pose k in his own axes (a
		 * finish's points p go to R p + T). */
		int BankHeadBone(int Key) const;
		bool BankHead(int Key, int Pose, Xform& Out) const;
		/** His helmet's frame on the field now: field = Out * a finish's points (HelmetMesh). Banked, between BankShow's two
		 * poses as the body is; a mirror as WorldFrame is (instance it as WorldFrame: flip y in the mesh). */
		bool HelmetFrame(int Man, Xform& Out) const;
		/** His helmet's finish (an index into his file's finishes; -1: none, or his own helmet, part of the body). */
		int HelmetOf(int Man) const;
		/** A finish alone at level L in set Key's colours: its look (empty where that level is not baked), and its points
		 * and facings (its rest, metres). */
		const Look& HelmetLook(int Key, int Finish, Lod L) const;
		bool HelmetMesh(int Key, int Finish, Lod L, std::vector<float>& OutPoints, std::vector<float>& OutFacings) const;

		/**
		 * The fall a man will take, named by the lead (a Fates look's kind: forward, back, side, knees, spin, thrown,
		 * crawl) before he falls; empty lets the crowd choose. Only where the soldier file has the deaths map.
		 */
		void SetFallKind(int Man, const std::string& Kind);

		/** The colours of an army's paint (0xRRGGBB), for pictures. */
		uint32_t PaintColour(const std::string& Army, int Paint) const;
		/** The battle's month (year * 12 + month - 1), season and front, as the finishes were picked. */
		int Month() const;
		const std::string& Season() const;
		/** One line for each army drawn with another's soldier file for want of its own (a stand-in, never a blank side). */
		const std::vector<std::string>& StandIns() const;
		/** armies.json's stand-in soldier file for an army ("POL"), empty where it names none. */
		std::string StandInOf(const std::string& Army) const;
		const std::string& FrontName() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> Inner;
		std::vector<Event> Outbox;
	};
}
