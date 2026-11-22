#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace Fates { class Table; class Director; }

/**
 * The ground battle's tanks, guns and lorries, in plain C++ (Brian, 21 November 2026: ground battles "as intense as the
 * air battles", and "accurate units, doctrine, odds and looks. Most shots miss, but every hit shows on what it
 * struck"). The same code runs in the game (the land battle's actor hands it the recording and the ground, and draws
 * what it says) and in the cloud, where Tools/land_kit plays the Normandy dry run through it and checks it.
 *
 * Three things live here:
 * - A battle model read and cut into rigid bodies (Model): the vehicle modeller's file (battle-models/land/vehicles,
 *   copied to Content/Models/Battle) has one mesh with a part number on every point. A game draws a rigid mesh per
 *   moving part, so the mesh is cut along its parts: one body for each part that moves on its own (hull, turret, gun,
 *   barrel, every hatch, wheel, sprocket and idler, the aerial, a gun's traverse and trail legs, a lorry's steering),
 *   with the parts that never move apart from it (kind "fixed": skirts, stowage, a barrel's sleeve) merged into it.
 *   A track's separate links (near mesh only) make one body a side, which the caller may leave still or re-pose on the
 *   CPU each frame (Model::PoseLinks). Each body's points come painted: the coat of its nation and month, the
 *   camouflage blotches, worn edges, exhaust soot and road dust, and a weight per point for how black it burns.
 * - A vehicle played from its track in the recording (Vehicle): where it is, how it sits on the ground, how its hull
 *   rocks, how its turret trains and its gun lays, when its barrel recoils, its hatches, wheels and tracks, how it is
 *   knocked out and burns. Everything is worked out ahead from the recording at 20 steps a battle second, so the pose
 *   at any time is a pure function of that time: a frame, a seek back and a picture all see the same tank.
 * - What it shows the effects kit and the sound (Event): muzzle flashes, spent cases, exhaust, brakes, sparks where a
 *   round struck, hatches, the knocked-out and brew-up beats, the fires that burn to the battle's end. And the places
 *   the lead draws on it: every socket, every scar (mk) on the surface it marks, the national marks.
 *
 * LOOKS ONLY. The battle (the rules and the squad brain) decided where every vehicle is, which round struck and what
 * it did. Nothing here changes that; what is decided here is only what the recording leaves open: the hull's rock, how
 * fast the turret comes round (the model's own traverse), the barrel's lay, how a kill looks (the Fates picker, ruled
 * by Content/Data/Battle/fates.json, so a battle never shows the same end twice running).
 *
 * Frames. The field is x east, y north, z up, metres (the recording's). A model is x forward, y right, z up (the
 * modeller's, and Unreal's own: left-handed). Places given out (Place::At, every Event and socket point) are field
 * points: hand them to the scene's World(x, y, z) as for the air battle's. Turns given out (Place::Q) are quaternions
 * that take a body's own points (model axes) into the axes x north, y east, z up: the axes World() lays the field in
 * Unreal (FVector(north, east, up)), so FQuat(Q.X, Q.Y, Q.Z, Q.W) is a proper rotation there and needs no mirroring:
 *     Mesh->SetWorldLocationAndRotation(World(P.At.X, P.At.Y, P.At.Z), FQuat(P.Q.X, P.Q.Y, P.Q.Z, P.Q.W));
 * with the body's vertices FVector(x, y, z) * UnitsPerMetre as they stand. Headings are radians, 0 north, clockwise,
 * as the recording's facing f.
 */
namespace LandKit
{
	// ---------------------------------------------------------------------------------------------------------------
	// Small maths.

	struct V3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		V3() = default;
		V3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
		V3 operator+(const V3& O) const { return V3(X + O.X, Y + O.Y, Z + O.Z); }
		V3 operator-(const V3& O) const { return V3(X - O.X, Y - O.Y, Z - O.Z); }
		V3 operator*(double S) const { return V3(X * S, Y * S, Z * S); }
		V3 operator-() const { return V3(-X, -Y, -Z); }
		V3& operator+=(const V3& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
		double Dot(const V3& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		V3 Cross(const V3& O) const { return V3(Y * O.Z - Z * O.Y, Z * O.X - X * O.Z, X * O.Y - Y * O.X); }
		double Length() const;
		V3 Unit(const V3& Otherwise = V3(0.0, 0.0, 1.0)) const;
	};

	/** A turn: w, x, y, z (Hamilton's; it carries p to q p q*, the model file's own rule for a turn about an axis). */
	struct Quat
	{
		double W = 1.0, X = 0.0, Y = 0.0, Z = 0.0;
		Quat() = default;
		Quat(double InW, double InX, double InY, double InZ) : W(InW), X(InX), Y(InY), Z(InZ) {}
		/** A turn of Radians about a unit axis (+ carries p toward axis x p, as the model's turns). */
		static Quat Axis(const V3& A, double Radians);
		Quat operator*(const Quat& O) const;
		Quat Conj() const { return Quat(W, -X, -Y, -Z); }
		V3 Rotate(const V3& P) const;
		Quat Unit() const;
		/** Part of the way from no turn to this one (0 none, 1 all of it). */
		Quat Scaled(double Share) const;
	};

	/** A rigid move: turn, then shift. Internal frame: x north, y east, z up (the field turned to Unreal's axes). */
	struct Xf
	{
		Quat Q;
		V3 T;
		V3 Apply(const V3& P) const { return Q.Rotate(P) + T; }
		Xf operator*(const Xf& Child) const;
	};

	/** Where a body is drawn: At, its pivot on the field (x east, y north, z up); Q, its turn into Unreal's axes. */
	struct Place
	{
		V3 At;
		Quat Q;
		/** A point of the body (model axes, metres from its pivot) on the field. */
		V3 Point(const V3& Local) const;
		/** A way of the body (model axes) on the field (x east, y north, z up). */
		V3 Way(const V3& Local) const;
	};

	/** The field's x east, y north, z up from the internal x north, y east, z up, and back (the same swap both ways). */
	inline V3 Swap(const V3& P) { return V3(P.Y, P.X, P.Z); }

	// ---------------------------------------------------------------------------------------------------------------
	// JSON, read lean: the battle models run to 4.6 MB and the recording to 4 MB. AirFeed's tree of small values took
	// 128 ms and a few hundred megabytes for one model (most of it lists of numbers, each a value of its own); this keeps
	// every value in one flat list and every list of plain numbers as numbers side by side, and reads a model in a
	// fifth of the time.

	class Json
	{
	public:
		enum class Type : uint8_t { Null, Bool, Number, String, Array, Object };

		/** One value of the text: a small handle (copy it freely; it lives as long as its Json). */
		class Value
		{
		public:
			Type Kind() const;
			bool IsNull() const { return Kind() == Type::Null; }
			bool IsNumber() const { return Kind() == Type::Number; }
			bool IsString() const { return Kind() == Type::String; }
			bool IsArray() const { return Kind() == Type::Array; }
			bool IsObject() const { return Kind() == Type::Object; }
			double Num(double Default = 0.0) const;
			int Int(int Default = 0) const;
			bool Truth(bool Default = false) const;
			std::string Str(const char* Default = "") const;
			bool Is(const char* Text) const;
			/** Items of a list, or fields of an object. */
			size_t Count() const;
			/** A field by name, or a null. */
			Value operator[](const char* Key) const;
			Value operator[](const std::string& Key) const { return (*this)[Key.c_str()]; }
			/** An item of a list (or the value of an object's field by its place), or a null. */
			Value At(size_t Index) const;
			/** An object's field name by its place. */
			std::string Name(size_t Index) const;
			/** A list of numbers into Out (false, and Out empty, when it is not a list of plain numbers). */
			bool Numbers(std::vector<double>& Out) const;
			bool Numbers(std::vector<float>& Out) const;
			bool Numbers(std::vector<int>& Out) const;

		private:
			friend class Json;
			const Json* Doc = nullptr;
			uint32_t Node = 0;
			int64_t Sub = -1; // a number of a list of numbers, by its place
		};

		/** Reads JSON text; false (and why) on a fault. */
		bool Parse(const std::string& Source, std::string& OutError);
		Value Root() const;

	private:
		struct Node
		{
			Type Kind = Type::Null;
			bool Flag = false;
			bool bNumbers = false; // a list of plain numbers: Numbers[A .. A + B)
			uint32_t A = 0, B = 0; // a container's Kids[A .. A + B) (an object's as name, value pairs); a string's Strings[A .. A + B)
			double Number = 0.0;
		};
		std::vector<Node> Nodes;
		std::vector<uint32_t> Kids;
		std::vector<double> NumberList;
		std::string Strings;
		friend struct Reader;
	};

	// ---------------------------------------------------------------------------------------------------------------
	// The battle model.

	enum class PartKind : uint8_t { Root, Turn, Spin, Slide, Fixed, Free };

	struct Part
	{
		std::string Name;
		int Parent = -1;
		V3 Pivot;               // model frame
		PartKind Kind = PartKind::Fixed;
		V3 Axis;                // a turn's or spin's axis (unit); + turns p toward Axis x p
		bool bLimits = false;
		double Lo = 0.0, Hi = 0.0;  // degrees
		V3 SlideWay;            // a slide's way (unit), and how far it goes (metres)
		double SlideLo = 0.0, SlideHi = 0.0;
		int Body = -1;          // the body it is drawn in (-1: none, or swapped out)
		int Side = 0;           // a track link or a running wheel: -1 the left, +1 the right
		bool bLink = false;     // a track's link (posed by the tracks.left/right clips)
		bool bShown = true;     // false where this month's swaps scale it to nothing (the Panzer IV's short gun)
		double Radius = 0.0;    // a wheel, sprocket, idler or roller (running.wheels)
	};

	struct Socket
	{
		std::string Name;
		int Part = 0;
		V3 At;      // model frame
		V3 Facing;  // unit
	};

	/** One keyed track of a clip: the part it moves and its keys (times in seconds into the clip). */
	struct ClipTrack
	{
		int Part = 0;
		std::vector<float> RotT, Rot;     // Rot: w, x, y, z a key
		std::vector<float> MoveT, Move;   // x, y, z a key (metres, the parent's frame)
		std::vector<float> TurnT, Turn;   // degrees about the part's axis
		std::vector<float> SlideT, Slide; // metres along the part's slide
	};

	struct ClipEvent
	{
		double T = 0.0;
		std::string Fx, Socket, Cue;
		bool bCueLoops = false;
	};

	struct Clip
	{
		std::string Name, About;
		double Long = 1.0;
		bool bLoop = false;
		double Stride = 0.0;  // a track clip: metres a loop
		std::vector<ClipTrack> Tracks;
		std::vector<ClipEvent> Events;
	};

	struct Mark
	{
		std::string Tag;   // a nation, or "_" (the lorry: whoever owns her)
		V3 At, Facing;     // model frame
		double Radius = 0.2;
		int Part = 0;
	};

	/** One level of detail of one body. */
	struct Mesh
	{
		std::vector<float> Points;      // x, y, z a point: model axes, metres from the body's pivot
		std::vector<float> Normals;     // x, y, z a point (unit)
		std::vector<uint8_t> Colours;   // r, g, b, a a point: 8-bit sRGB (FColor wants them as B, G, R, A)
		std::vector<float> Soot;        // 0 to 1 a point: how far toward BurntColour it goes once the wreck has burnt through
		std::vector<uint32_t> Triangles;// three point numbers a triangle
		/** A track body's links (Near): the run of points of each, in the order of Model::Links(Side). */
		std::vector<uint32_t> LinkFirst, LinkCount;
		size_t Count() const { return Soot.size(); }
	};

	struct Body
	{
		std::string Name;   // its part's name: "hull", "turret", "wheel_l3", "track_l" ...
		int Part = 0;       // the part it is drawn for
		int Parent = -1;    // the body it hangs on (-1: none, the vehicle's root body or the root itself)
		V3 Pivot;           // model frame: its points are kept from here
		Mesh Near, Far;
		bool bTrack = false;// a track: its Near is the links (PoseLinks moves them), its Far the band, drawn still
		int Side = 0;       // a track or a running wheel: -1 the left, +1 the right
		/** An earlier body of the same shape and paint (a road wheel): draw both from one mesh (-1: its own). */
		int SameAs = -1;
		/** The box round each run of 64 of Near's triangles, in their order (lo x, y, z, hi x, y, z a run, model axes
		 * from the pivot): a round cast at her (a scar's place) passes by every run whose box it misses. */
		std::vector<float> Runs;
	};

	/** What a pose asks of one part: a turn about its own axis (degrees), a slide along its way (metres), and a clip's
	 * free turn and shift on top (the parent's frame). */
	struct PartState
	{
		double Turn = 0.0, Slide = 0.0;
		Quat Rot;
		V3 Move;
	};

	/** The coat a model is painted in, by the nation that owns it and the month of the battle. */
	struct Dress
	{
		std::string Nation = "USA";
		int Year = 1944, Month = 7;
	};

	class Model
	{
	public:
		/** Reads a battle model's text and cuts it into bodies painted for D. False (and why) on a fault. */
		bool Load(const std::string& Text, const Dress& D, std::string& OutError);

		std::string Name, Title, Kind, Nation, Coat; // Kind: tank, gun, carrier; Coat: the coat chosen ("yellow" ...)
		double Long = 0.0, Wide = 0.0, High = 0.0;
		Dress Dressed;
		std::vector<Part> Parts;
		std::vector<Body> Bodies;
		std::vector<Socket> Sockets;
		std::vector<Mark> Marks;
		std::map<std::string, Clip> Clips;
		/** The sooty black-brown a burnt-out wreck goes to (8-bit sRGB): lerp each point toward it by its Soot times the
		 * vehicle's Vehicle::Soot (0 to 1 over 30 s of burning). */
		uint8_t BurntColour[3] = { 38, 33, 29 };

		// The controls (controls, running): parts by number, -1 where the model has none.
		int Turret = -1, Gun = -1, Barrel = -1, Traverse = -1, Hull = -1, Breech = -1;
		double TurretDps = 0.0, TraverseDps = 0.0; // degrees a second
		std::vector<int> Hatches;      // every hatch (the commander's first, where the model has one)
		int CommanderHatch = -1;
		std::vector<int> Steer;        // a lorry's front wheels' kingpins
		double SteerLimit = 32.0;
		std::vector<int> Trails;       // a gun's trail legs
		double TrailClosed = 0.0;      // degrees: a leg closed for the road
		std::vector<int> Wheels;       // every running wheel (Part::Radius, Part::Side)
		int TrackBody[2] = { -1, -1 }; // the left and right track bodies
		double TrackStride[2] = { 0.0, 0.0 }; // metres a loop of tracks.left / tracks.right
		double TrackHalf = 1.0;        // half the distance between the tracks (or the wheels), metres
		/** Where she stands on the ground (model frame, z 0): the four corners of her tracks' or wheels' footprint (a
		 * tank's front and rear road wheels' contact, a gun's two wheels and her spades, a lorry's four wheels). */
		V3 Foot[4];
		/** The box her faces are cast into for a scar (the hull with its running gear, and the turret alone). */
		V3 HullLo, HullHi, TurretLo, TurretHi;

		int PartIndex(const std::string& PartName) const;
		int SocketIndex(const std::string& SocketName) const;
		const Clip* FindClip(const std::string& ClipName) const;
		/** The links of a track side (-1 left, +1 right), in the order their points run in the track body's Near. */
		const std::vector<int>& Links(int Side) const { return Side < 0 ? LinksL : LinksR; }
		/**
		 * The track links of a side posed for a run of Run metres (the side's distance gone, + forward): every point of
		 * the track body's Near, x, y, z (model axes, from the body's pivot), written to Out (3 floats a point). The
		 * tracks.left/right clip run at Run / stride of a loop; a loop moves each link on to its neighbour's place.
		 */
		void PoseLinks(int Side, double Run, float* Out) const;
		/** Every part placed (internal frame) from the root's place and what the pose asks of each part (one a part;
		 * the track links are left out: PoseLinks moves them inside their track's body). */
		void Solve(const Xf& RootXf, const std::vector<PartState>& S, std::vector<Xf>& OutParts) const;
		/** Each body's place from the parts' (Solve's), into Out (one a body). */
		void Places(const std::vector<Xf>& PartXf, std::vector<Place>& Out) const;
		/** A clip's keys at Time (seconds into it), Share of it (0 to 1), added to S; Turned: the clip turned about her
		 * up by that many degrees first (fire.rock about the turret's bearing). */
		void Play(const Clip& C, double Time, double Share, std::vector<PartState>& S, double Turned = 0.0) const;

	private:
		std::vector<int> LinksL, LinksR;
		std::vector<int> LinkTrack; // a link part's track in its tracks clip, by part (-1: none)
		friend class Vehicle;
	};

	// ---------------------------------------------------------------------------------------------------------------
	// The recording's words for one vehicle (battle-bridge.ts, "The men"; Tools/land_record/record.ts, Track).

	struct Change
	{
		double T = 0.0;
		std::string Pose, Cover;
		bool bAim = false;
		double AimX = 0.0, AimY = 0.0;
		bool bHidden = false, bButtoned = false;
	};

	struct ScarWord
	{
		std::string Face, Kind; // front, left, right, rear, top; gouge, hole, scorch, pocks
		double U = 0.0, H = 0.0, From = 0.0;
	};

	struct Round
	{
		int N = 0;
		double T = 0.0, L = 0.0;
		int By = -1, K = -1;     // the shooter (block, model), -1 unseen
		int J = 0;               // the man of the shooter's section (the drill's sections; 0 the key man)
		std::string W, R, Face;  // weapon; miss, glance, worn, kill; the face struck
		int On = -1, Ok = -1;    // the block fired at, the model struck
		int Oj = 0;              // the man of the model's section struck (0 the model itself: a follower is no hull)
		double X = 0.0, Y = 0.0, U = 0.0, H = 0.0;
	};

	struct TrackWords
	{
		int Block = 0, K = 0;
		int J = 0;                           // of a section (the drill's sections: a section's lorry), 0 the key
		std::string Role;                    // tank, gun, lorry
		std::vector<double> At;              // t, x, y, f a kept point
		std::vector<Change> Changes;
		std::vector<double> Shots;
		double Died = -1.0;                  // -1: never
		double Out = -1.0;                   // -1: never left the field
		std::vector<ScarWord> Scars;
	};

	// ---------------------------------------------------------------------------------------------------------------
	// What a vehicle shows the effects and the sound.

	struct Event
	{
		double T = 0.0;        // battle time it happens
		int Vehicle = -1;      // Battle::Vehicles() number
		std::string Fx;        // the model clip's fx name (muzzle_flash_big, case_clang, exhaust_start, hit_sparks, ...)
		std::string Cue;       // the sound director's name, or empty
		bool bLoop = false;    // a loop that goes on until Until (fire_loop, smoke_loop, an engine's loop cue)
		double Until = 0.0;    // battle time a loop ends (the battle's end for a wreck's fire)
		std::string Socket;    // where on the model, or empty (a hit's point is its own)
		V3 At;                 // field point
		V3 Facing;             // field way (unit)
		int Key = 0;           // the same vehicle and socket give the same key: a later loop there takes over
		// A round striking (hit_sparks, rattle): what struck and what it did.
		int Round = 0;
		std::string Weapon, Result, Face;
		double Speed = 0.0;    // dust, brakes: the vehicle's speed, m/s
	};

	/** The ground under a field point: its height and its up (field axes, unit). The caller's terrain. */
	using GroundFn = std::function<void(double X, double Y, double& OutZ, V3& OutNormal)>;

	struct Scar
	{
		ScarWord Word;
		int Body = -1;    // the body it is on (it moves with it: a turret's turns with the turret)
		V3 Local, Normal; // model axes, from the body's pivot
		bool bOnSurface = false; // found on her mesh (false: the model's hit socket for that face stands in)
	};

	/** One national mark as placed now. */
	struct MarkPlace
	{
		std::string Tag;
		bool bOwner = false; // the owner's own mark (the tag is her nation's, or the lorry's "_")
		V3 At, Facing;       // field
		double Radius = 0.2;
	};

	// ---------------------------------------------------------------------------------------------------------------
	// One vehicle played from the recording.

	class Vehicle
	{
	public:
		/**
		 * Works out the vehicle's whole battle ahead (every 0.05 s): its place, speed and turn, the turret's and gun's
		 * lay, wheels and tracks, hatches, shots, hits and its end. Rounds: the battle's rounds (those it fired and those
		 * that struck it are picked out). End: the battle's last second. bBrewUp: a kill shown as a brew-up (the turret
		 * thrown) instead of a knock-out. Index: its number among the battle's vehicles (for events' keys).
		 */
		void Build(const Model& InModel, const TrackWords& InWords, const std::vector<Round>& Rounds, double End,
			const GroundFn& InGround, bool bInBrewUp, int InIndex, const std::string& InSide, const std::string& InNation);

		/**
		 * Works her out again once her words have grown (a live battle: Battle::Extend): new kept points, changes, shots,
		 * her death, scars, the provisional point where she stands now, rounds, a later End. Only from a little before
		 * the first step the new words can change: every pass keeps its running state each battle second, so the steps
		 * before that are kept as they were and the passes take up there (less the second or so each looks ahead), and
		 * work on to the new End. What Update has given out is not given again and nothing due is skipped: her events
		 * after the last Update's time are made anew from the new words, the earlier ones stay given, and a move-off or
		 * stop the new words date before that time is given at the first step after it. What the screen has shown
		 * stands: those steps keep her place (a tank her facing too) and her moving or standing, and she eases onto the
		 * new line over a second (a kept point that comes late, or a provisional one, may move it by a tenth of a metre
		 * or a few degrees). A lorry whose first move comes after her standing was shown keeps her standing, and turns
		 * to her way so as to face it as she rolls (read whole, she faces it from the start). Her pose is the last
		 * Update's again. Rounds: those she fired and those that struck her, in the order they came (they only grow).
		 * Build her first.
		 */
		void Grow(const TrackWords& InWords, const std::vector<Round>& Rounds, double InEnd, bool bInBrewUp);

		/** The pose at battle time T, and every event from the last Update to T into Out (none on a seek back). */
		void Update(double T, std::vector<Event>* Out);

		/** Life seconds a battle second (Battle::Life): her clips, and her crew's hands on the turret, the gun and the
		 * wheel, are life's. Set before Build. */
		double Life = 1.0;
		/** A live battle not over yet: the time a loop that lasts to the battle's end (her fire, her smoke) is given
		 * until, since that end is not known yet (Battle::Extend gives the battle's limit). 0: End (a whole recording). */
		double LoopsUntil = 0.0;

		// Who.
		const Model& Look() const { return *Shape; }
		const TrackWords& Words() const { return Track; }
		int Index = 0;
		std::string Side, Nation, FateKey;
		bool bBrewUp = false;
		double Died = -1.0;      // battle time knocked out, -1 never
		double BurnFrom = -1.0;  // battle time the fire takes hold (-1 never): it burns to BurnUntil
		double BurnUntil = 0.0;

		// The pose now (after Update).
		double Now = 0.0;
		const std::vector<Place>& Bodies() const { return BodyPlaces; } // one a Model::Bodies entry
		Place Root;              // the root (the ground under her middle)
		double Heading = 0.0;    // radians, 0 north, clockwise
		double Pitch = 0.0, Roll = 0.0; // degrees: nose up, right side up (her seat on the ground)
		V3 Velocity;             // field metres a second
		double Speed = 0.0;      // metres a second, - astern
		bool bMoving = false;
		double Load = 0.0;       // 0 to 1: how hard her engine works (exhaust smoke): idle 0.1, driving, pulling away
		std::string EngineCue;   // the loop playing now (tank_idle_loop, tank_loop ...), empty when dead
		double RunLeft = 0.0, RunRight = 0.0; // metres each track (wheel side) has run
		double TurretDeg = 0.0, GunDeg = 0.0, TraverseDeg = 0.0, SteerDeg = 0.0, HatchOpen = 0.0, Recoil = 0.0;
		bool bDead = false, bBurning = false;
		double Soot = 0.0;       // 0 to 1: how far the burning has blackened her (over 30 s)
		double Fade = 1.0;       // 1 shown; to 0 over a second once she has left the field
		/** Her dust sockets now (field), for the effects kit's VehicleDust each frame while she moves. */
		V3 DustLeft, DustRight;

		/** A socket's field point and way at the pose now; false if the model has none of that name. */
		bool SocketNow(const std::string& Name, V3& OutAt, V3& OutFacing) const;
		/** The same at any battle time (poses her then; a little dearer). */
		bool SocketAt(const std::string& Name, double T, V3& OutAt, V3& OutFacing) const;
		/** Every scar (mk) on her, its surface place found; show one from its Word.From. */
		const std::vector<Scar>& Scars() const { return ScarList; }
		/** A scar's field point and outward normal at the pose now. */
		void ScarNow(size_t I, V3& OutAt, V3& OutNormal) const;
		/** Where round N struck her (a glance, worn or kill with a face), at the pose now; false if it did not. */
		bool StruckNow(int RoundN, V3& OutAt, V3& OutNormal) const;
		/** The national marks at the pose now. */
		void MarksNow(std::vector<MarkPlace>& Out) const;
		/** The near track links of a side posed for her run now (see Model::PoseLinks). */
		void PoseLinks(int TrackSide, float* Out) const;

		/** The checks' and pictures' way in: pose her exactly as given (a picture's pose, no recording). */
		struct Pin
		{
			double X = 0.0, Y = 0.0, Heading = 0.0;
			double Turret = 0.0, Gun = 0.0, Traverse = 0.0, Steer = 0.0, Trail = 0.0;
			double Hatches = 0.0;       // 0 shut to 1 every hatch open
			double FireAge = -1.0;      // seconds since a shot (the barrel's recoil), -1 none
			double DeathAge = -1.0;     // seconds since knocked out (-1 alive)
			bool bBrewUp = false;
		};
		void PinPose(const Model& InModel, const Pin& P, const GroundFn& InGround);

	private:
		struct Tick
		{
			float X = 0, Y = 0, F = 0, V = 0, W = 0, RunL = 0, RunR = 0;
			float Turret = 0, Gun = 0, Traverse = 0, Steer = 0, Drive = 0;
		};
		struct Timed
		{
			double T = 0.0;
			const ClipEvent* E = nullptr; // the clip's event (its fx, socket, cue), or none for a strike
			int Round = 0;                // a strike: the round's number
			std::string Fx, Cue, Weapon, Result, Face;
			bool bLoop = false;
			double Until = 0.0;
		};
		struct Strike
		{
			int N = 0;
			double L = 0.0, U = 0.0, H = 0.0;
			int Body = -1;
			V3 Local, Normal;
			bool bHeavy = false;  // a shell or shot: she jolts (the hit clip); small arms only rattle
			bool bOnSurface = false;
			std::string W, R, Face;
			double TurretAt = 0.0; // her turret's training at L when it was cast (Grow casts it again if that changes)
		};
		/** The passes' running state as they enter a step, kept every Resume step (a battle second): Grow takes up there. */
		struct Resume
		{
			double Last = 0.0;                               // a lorry's last way of going
			double Carriage = 0.0;                           // a gun's carriage, and whether her crew are swinging it
			bool bShifting = false;
			bool bOn = false;                                // moving off and stopping
			double OnFor = 0.0, OffFor = 0.0;
			size_t MoveOffs = 0, Stops = 0;
			double Turret = 0.0, Lay = 0.0, Trav = 0.0, Steer = 0.0, Drive = 0.0, RunL = 0.0, RunR = 0.0;
			double Rise = 0.0, RiseX = -1e9, RiseY = -1e9, RiseAX = -1e9, RiseAY = -1e9;
		};
		const Model* Shape = nullptr;
		TrackWords Track;
		GroundFn Ground;
		double Step = 0.05, End = 0.0;
		std::vector<Tick> Ticks;
		std::vector<double> Shots, MoveOffs, Stops, HeavyHits, Hatch; // times (Hatch: the open/shut changes)
		std::vector<bool> HatchOpens;
		std::vector<Strike> Strikes;
		std::vector<Timed> Timeline;
		size_t NextEvent = 0;
		double LastT = -1.0;
		double Phase = 0.0;            // her own offset into the looping clips, so a platoon does not bob as one
		bool bTowed = false;
		std::vector<Scar> ScarList;
		std::vector<Place> BodyPlaces;
		std::vector<Xf> PartXf;        // the pose now, every part (internal frame)
		mutable std::vector<PartState> Scratch;
		bool bPinned = false;
		Pin Pinned;
		// What Grow takes up from: her place and heading every step (the heading as the passes leave it), the passes'
		// state every battle second, a lorry's first step on the move (-1: none yet) and the step she faces her way from
		// (0, the start, unless her standing had been shown before her first move came), and how many of the rounds
		// Strikes was made from.
		std::vector<double> PathX, PathY, PathF;
		std::vector<Resume> Resumes;
		size_t FirstMove = static_cast<size_t>(-1);
		size_t TurnFrom = 0;
		size_t StrikesSeen = 0;

		/** Works her out from step From on (0: all of it), the earlier steps kept; her events after Given made anew. */
		void Work(size_t From, const std::vector<Round>& Rounds, bool bInBrewUp, double Given);
		/** The pose at battle time T (Update's, without the events). */
		void Show(double T);
		void PlaceAt(double T, double& OutX, double& OutY, double& OutF) const;
		Tick TickAt(double T) const;
		void Pose(double T, std::vector<Xf>& OutParts, Xf* OutRoot, double* OutPitch = nullptr, double* OutRoll = nullptr) const;
		void PosePinned(std::vector<Xf>& OutParts, Xf* OutRoot, double* OutPitch = nullptr, double* OutRoll = nullptr) const;
		Xf Seat(double X, double Y, double F, double* OutPitch, double* OutRoll) const;
		bool Cast(const std::string& Face, double U, double H, double TurretAt, int& OutBody, V3& OutLocal, V3& OutNormal) const;
		void Finish(const Xf& RootXf);
		static double Latest(const std::vector<double>& Times, double T);
	};

	// ---------------------------------------------------------------------------------------------------------------
	// The battle: every tank, gun and lorry of a recording.

	struct Block
	{
		int Id = 0;
		std::string Side, Nation, Kind, Model, Cal;
	};

	class Battle
	{
	public:
		/** Reads a model file by its name ("Sherman.json") from Content/Models/Battle; false if it cannot. */
		using ReadFn = std::function<bool(const std::string& FileName, std::string& OutText)>;

		/**
		 * Reads the recording, loads the models it needs (each once a nation: the lorry stands in for both sides'), picks
		 * how each kill looks (FatesTable: Content/Data/Battle/fates.json; null: a fixed one in four brews up) and works
		 * out every vehicle. False (and why) on a fault.
		 */
		bool Load(const std::string& RecordingText, const ReadFn& Read, const GroundFn& InGround, const Fates::Table* FatesTable,
			uint64_t Seed, std::string& OutError);

		/**
		 * A live battle (src/battle/land/live.ts): Load it from the header (no men yet: no vehicle, the models its blocks
		 * name read ahead), then hand it each piece the rules fight, in order (the scene's LandFeed::AppendDelta reads
		 * the same text). A new vehicle track (tank, gun, lorry) becomes a new vehicle at the end of Vehicles(), her
		 * model read if it is new. A vehicle whose words grew (kept points, changes, shots, her death, scars, leaving
		 * the field, the provisional point where she stands now) or that a new round was fired by or struck is worked
		 * out again from a little before the change (Vehicle::Grow: the cost is the few seconds since, not the battle so
		 * far), and every other one at least every three quarters of a battle second, so each is worked out past what
		 * the screen can show. End becomes the piece's t, or the battle's end once it is over. A kill's look is picked as
		 * her death comes in, by the same picker Load uses and in the same order (the order they fell), so a battle
		 * grown piece by piece and the same battle read whole show the same ends. A reset (the men placed again before
		 * the clock ran) drops every vehicle and begins again. A piece is skimmed before it is read: the news of tracks
		 * known to be men on foot (most of it), and its views, events and phases, are passed over unread. Load's Fates
		 * table and ground must outlive the battle. False (and why) on a piece it cannot read, or a battle not loaded
		 * from a live header.
		 */
		bool Extend(const std::string& DeltaText, std::string& OutError);

		/** Poses every vehicle at battle time T; what happened since the last Update is added to Events. */
		void Update(double T);

		std::vector<Vehicle>& Vehicles() { return All; }
		const std::vector<Vehicle>& Vehicles() const { return All; }
		const std::vector<Round>& Rounds() const { return RoundList; }
		const std::vector<Block>& Blocks() const { return BlockList; }
		/** The models by "Name|Nation" (the lorry as "Truck|USA" and "Truck|GER"). */
		const std::map<std::string, Model>& Models() const { return Looks; }
		double End = 0.0;
		/** Life seconds a battle second (the rules' clock runs about twice as fast as life; the scene plays the fight at
		 * half of it): the vehicles' clips and their crews' hands timed in life's seconds. 1, the default, is as before.
		 * Set before Load. */
		double Life = 1.0;
		int Year = 1944, Month = 7;
		/** What has happened since it was last emptied: the caller plays it and clears it. */
		std::vector<Event> Events;
		/** Loaded from a live battle's header (Extend grows it); and over (its last piece has come). */
		bool bLive = false, bOver = false;

	private:
		std::vector<Vehicle> All;
		std::vector<Round> RoundList;
		std::vector<Block> BlockList;
		std::map<std::string, Model> Looks;
		// What Extend goes on with: Load's reader, ground and kill picker (it remembers what it has shown), the battle's
		// limit, the last Update's time; and each vehicle's model, her words as the pieces grew them (ending in her
		// provisional point when she has one), the rounds she fired or that struck her, whether her death's look is
		// picked and whether it brews up, and the End she was last worked out to. Every track by its number: its vehicle
		// or -1 (a man on foot), and the vehicles by who they are (block, model, man).
		ReadFn Reader;
		GroundFn Ground;
		std::shared_ptr<Fates::Director> Picker;
		uint64_t FateSeed = 0;
		const Fates::Table* FateTable = nullptr;
		double Limit = 0.0, Shown = -1.0;
		std::vector<std::string> KeyOf;
		std::vector<TrackWords> Grown;
		std::vector<uint8_t> Tailed, Picked, Brew;
		std::vector<std::vector<Round>> Own;
		std::vector<double> WorkedTo;
		std::vector<int> TrackVehicle;
		std::map<std::tuple<int, int, int>, int> ByWho;

		const Block* BlockOf(int Id) const;
		/** The model for a vehicle of block B in Role, read and painted if it is not yet; its key into Looks. */
		bool LoadLook(const Block& B, const std::string& Role, std::string& OutKey, std::string& OutError);
		/** How a kill looks (its fate key, and whether it brews up) from the picker; or without a table, a fixed one in
		 * four brewing up. Rounds: those that struck her, the killing one among them. */
		std::string PickLook(const TrackWords& W, const Model& M, const std::vector<Round>& Rounds, uint8_t& OutBrew);
	};

	/** Reads a recording's vehicle tracks, rounds and blocks (Battle::Load does it; the checks use it alone). */
	bool ReadRecording(const Json& Doc, std::vector<Block>& OutBlocks, std::vector<TrackWords>& OutTracks,
		std::vector<Round>& OutRounds, double& OutEnd, int& OutYear, int& OutMonth, std::string& OutError);
}
