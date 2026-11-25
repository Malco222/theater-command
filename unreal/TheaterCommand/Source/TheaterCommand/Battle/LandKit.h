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
 *   camouflage blotches, worn edges, exhaust soot and road dust, and a weight per point for how black it burns (a
 *   file that lays its own dirt, dust, haze, blend2 and shade, is painted as it says and nothing of the game's own).
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
 * with the body's vertices FVector(x, y, z) * UnitsPerMetre as they stand, and each triangle's last two corners
 * swapped: the files wind a face counter-clockwise seen from its front, the engine draws the side it sees clockwise
 * (LandBattle.cpp's LandParts::KitFront; handed over unturned, every vehicle was drawn inside out, 23 November).
 * Headings are radians, 0 north, clockwise, as the recording's facing f.
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
		double Dust = 0.0; // 0 to 100: how much of her dust and grime lies over the paint (the file's "dust")
	};

	/** One level of detail of one body. */
	struct Mesh
	{
		std::vector<float> Points;      // x, y, z a point: model axes, metres from the body's pivot
		std::vector<float> Normals;     // x, y, z a point (unit)
		std::vector<uint8_t> Colours;   // r, g, b, a a point: 8-bit sRGB (FColor wants them as B, G, R, A)
		std::vector<float> Soot;        // 0 to 1 a point: how far toward BurntColour it goes once the wreck has burnt through
		/** Each point's paint as the file names it (Model::PaintNames' number: canvas, glass, wood, rubber ...), for an old
		 * wreck (LandHulk, release 2) to burn each stuff its own way. A point added after the file's (a captor's mark)
		 * has none: Paints may be shorter than Count(). */
		std::vector<uint8_t> Paints;
		/** r, g, b a point (8-bit sRGB): the colour THIS point burns to, where the burnt coat is on (release 2,
		 * Dress::bBurntCoat): soot, rust or ash by where the point is and which way it looks. Empty: every point burns to
		 * the model's one BurntColour, as before. BurntOf reads either. */
		std::vector<uint8_t> Burnt;
		std::vector<uint32_t> Triangles;// three point numbers a triangle
		/** A track body's links (Near): the run of points of each, in the order of Model::Links(Side). */
		std::vector<uint32_t> LinkFirst, LinkCount;
		size_t Count() const { return Soot.size(); }
		/** The colour point K burns to: its own (Burnt) where the burnt coat is on, else the model's one (Whole). */
		const uint8_t* BurntOf(size_t K, const uint8_t* Whole) const { return Burnt.size() >= (K + 1) * 3 ? &Burnt[K * 3] : Whole; }
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

	/** The coat a model is painted in, by the nation that owns it, the month of the battle and the field's ground. */
	struct Dress
	{
		std::string Nation = "USA";
		int Year = 1944, Month = 7;
		/** The field's ground (the recording's field.place.terrain: plains, desert ...): a model's swaps.paint.by_ground
		 * rows for it are read before by_month (the German desert coats of 1941 and 1942, the British Light Stone).
		 * Empty: none. */
		std::string Terrain;
		/** Captured kit (release 2, behind its switch): the nation she was taken from, where the recording's block says
		 * so (blocks[].captured) and the look is on; she wears her captor's (Nation's) coat over her maker's, the old
		 * showing at the chipped edges and where the brush missed. Empty: her own. */
		std::string Captured;
		/** Swaps chosen over the month's and the nation's (swap group -> choice), where the model has that choice: Men in
		 * lorries (the Battle rules card's row) carries her men with the Truck's tilt stowed ("tilt" -> "off"). */
		std::map<std::string, std::string> Force;
		/** A coat the rules pick for her whole army (release 2, behind its switch; the recording's blocks[].coat): the
		 * name of one the game keeps for a model several armies share (NationCoat), put on in place of what her owner
		 * would otherwise get there. "japan": Japan's khaki broken with earth brown, on the field gun, the lorry and
		 * the light tank that stand in for her, so they are one army with her own Type 97. A model with coats of its
		 * own in its file is let be. Empty, or a name the game does not keep: as before. */
		std::string Coat;
		/** Swaps the rules pick for her (release 2, the track's "sw": a motorcycle's sidecar, a staff jeep's gun put
		 * away): {swap, choice}, taken before her month's row, by_nation and default where her model has that swap and
		 * that choice; any other is let be. */
		std::map<std::string, std::string> Choose;
		/** National marks (release 2, "-TheaterLandMarks"): her owner's plain period marking painted on her skin at each
		 * of her file's mark places (Model::Marks). Off: none, as before. */
		bool bMarks = false;
		/** The field's own ground (release 2, the whole picture's worn paint, tc.Look.Worn; linear light, the field's mean
		 * paint with its hue held inside an earth box, WholeLook::FieldDust): a third layer of dirt over her file's dust and haze or
		 * the game's own road dust, on her lower third and her running gear (the art director: 30 to 50 in 100 of the
		 * field's colour there). A negative red: none, as before. */
		double FieldDust[3] = { -1.0, -1.0, -1.0 };
		/** The burnt coat (release 2, on its own switch, the trial word "burntcoat"; the art director's brief of 25
		 * November, for every army's wrecks): a burnt-out wreck goes to three colours in place of one. Soot (the model's
		 * BurntColour) above and round what the smoke poured from and under overhangs, ash grey on faces that look up and
		 * on a burnt tyre's rim, rust on upright plates and the running gear, in patches some 0.3 to 0.8 m across; never
		 * ash on an edge. Each point's colour is kept in Mesh::Burnt. Off: one colour, as before. */
		bool bBurntCoat = false;
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
		/** Where her national marks were painted (release 2, Dress::bMarks): her file's places, less those her owner's
		 * army left bare (the Tiger's and Panther's turrets carried a number, not the cross), plus those it painted that
		 * her file lacks (the Panther's hull sides; the Japanese star on her front plate). Empty with the marks off. */
		std::vector<Mark> Painted;
		std::map<std::string, Clip> Clips;
		/** The sooty black-brown a burnt-out wreck goes to (8-bit sRGB): lerp each point toward it by its Soot times the
		 * vehicle's Vehicle::Soot (0 to 1 over 30 s of burning). */
		uint8_t BurntColour[3] = { 38, 33, 29 };
		/** The file's paint names, in its order (Mesh::Paints numbers them). */
		std::vector<std::string> PaintNames;
		/** The burnt coat (release 2, Dress::bBurntCoat) was on when she was loaded: every point of every mesh of hers
		 * has a burnt colour of its own (Mesh::Burnt: soot, rust or ash), and BurntColour is her soot alone. */
		bool bBurntCoat = false;

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
		/** She carries a roof machine gun as drawn (a roof_mg part not scaled to nothing by her swaps this month, or for
		 * her nation): only then may her commander fire one. */
		bool RoofGun() const;
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
		std::string Fate;        // a kill: how the rules say she ended (out, burn, blow), by her measured odds; empty before they did
	};

	struct TrackWords
	{
		int Block = 0, K = 0;
		int J = 0;                           // of a section (the drill's sections: a section's lorry), 0 the key
		std::string Role;                    // tank, gun, lorry
		std::string Model;                   // her own model where not her block's (ml): a carrier (empty, the lorry), an SP gun, a TD
		std::map<std::string, std::string> Swaps; // swaps the rules picked for her (sw, release 2): {swap, choice}
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
		/** Effects (R2): battle seconds her brew-up waits after her kill, her knocked_out clip playing first while her
		 * crew get out (LandBail, -TheaterLandBailOut; Brian, 23 November: "if a tank gets hit people try and get out
		 * before it explosdes"). 0: at once, as before. Battle::BrewWaitLeast and Most give it, before Build. */
		double BrewWait = 0.0;
		/** Tanks rolling smoothly (R2, Brian 23 November 15:24Z: "have the tanks move more smooth and not rigid and zig
		 * zagging"; -TheaterLandSmoothTanks): a tank's place and facing each step the recording's blended over this many
		 * battle seconds either way (a bell's spread, at most about 0.45: the steps worked out again behind a live battle's
		 * newest piece). 0: the recording's places, joined straight, as before. Set before Build (Battle::SmoothTanks). */
		double SmoothTanks = 0.0;

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
		/**
		 * The front of each track's (each side's wheels') ground now (field: Model::Foot's front corners), where her marks
		 * are laid from (the ground's Ruts): under her, so the newest piece, laid a step on, is hidden by her hull. (Laid
		 * from the dust sockets at her tail, the marks began a stride and more behind her, bare snow between: Brian, 23
		 * November 15:25Z.) Her dust sockets where her model has no footprint.
		 */
		V3 TreadLeft, TreadRight;

		/** A socket's field point and way at the pose now; false if the model has none of that name. */
		bool SocketNow(const std::string& Name, V3& OutAt, V3& OutFacing) const;
		/** Men in lorries (the Battle rules card's row): her way down thrown open at battle time T (her `dismount` clip,
		 * the tailboard dropped, from T), as her first man gets down; it closes again (the clip backward) as she next moves
		 * off. Times before the last one given are passed over. Nothing where her model has no `dismount`. */
		void OpenBoard(double T);
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
			bool bOpenUp = false;       // her open_up clip played to its end (her commander's lid as she stands unbuttoned)
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
			double OnFor = 0.0, OffFor = 0.0, RollFor = 0.0;
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
		std::vector<double> Boards;    // the times her way down was thrown open (OpenBoard)
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
		std::string Captured;   // the nation her vehicles were taken from (the recording's "captured"); empty: her own
		std::string Coat;       // a coat the rules picked for her army (the recording's "coat", release 2: "japan"); empty: as before
		std::vector<std::string> Extra; // models her vehicles are drawn with besides her own (the header's "mls", release 2): read before her men come; "Jeep|mg=off": with those swaps
	};

	/**
	 * How a vehicle pierced on one face ends, as measured (Content/Data/Battle/performance-land.json, the modeller's:
	 * vehicles[name].fire_when_pierced and explodes_of_fires). Brian, 22 November: a rear hit on a Tiger or a Panther
	 * raises the chance she blows up; the scene's kills read it.
	 */
	struct FireOdds
	{
		double Front = 0.0, Side = 0.0, Rear = 0.0; // the chance she burns, pierced on that face
		double Explodes = 0.0;                       // of those fires, the share that set off her ammunition at once
	};

	/** Every vehicle's fire odds from performance-land.json's text, by its model name; false (and why) on a fault. */
	bool ReadFireOdds(const std::string& Text, std::map<std::string, FireOdds>& Out, std::string& OutError);

	/**
	 * Her end by the face struck ("front", "side", "rear", "top"; empty: the three faces' mean): "blow" (her ammunition
	 * goes up: the fates' turret off), "burn" (she brews up or burns) or "still" (knocked out, no fire), for draws U1
	 * (fire) and U2 (blast), each 0..1.
	 */
	std::string FireEnd(const FireOdds& O, const std::string& Aspect, double U1, double U2);

	/**
	 * A carrier's bench seats as she stands now: a lorry's or a half-track's rider.1 to rider.10, where the lorried men
	 * aboard sit (they were drawn kneeling on the ground where the rules seat them, so under the lorry: Brian, 23
	 * November 14:10Z, "troops are stuck under the truck"). Her root and axes on the field, and her seats each side front
	 * to back, each its top and its inboard way (field metres; x east, y north, z up).
	 */
	struct Benches
	{
		int Block = -1, K = -1;      // whose: her block and her number in it (the rules' vehicle)
		V3 At, Forward, Right;
		int Left = 0, Righted = 0;   // Seat[0, Left) down her left side, Seat[Left, Left + Righted) down her right
		V3 Seat[10], Way[10];
	};
	/** Her benches now (after her Update): false where her model is no carrier with bench seats (a lorry or a
	 * half-track, "carrier", with rider.1 and on). */
	bool BenchesNow(const Vehicle& V, Benches& Out);
	/** A man aboard's seat: its top, the bed's floor beside it (0.5 m under it), his heading on it (radians clockwise
	 * from north: inboard), and bBench false where the benches are full (he kneels in the bed's middle at her front,
	 * facing aft; At the floor there). */
	struct BenchSeat
	{
		V3 At;
		double Floor = 0.0, Heading = 0.0;
		bool bBench = true;
	};
	/**
	 * The seat of a man aboard. Where the rules say his vehicle and seat (Block, SeatOn: her k; Seat: two abreast, the odd
	 * ones on her right, front to back): that seat of hers, or the bed's middle past her benches. Else by his field place
	 * (the rules' place for him): the nearest carrier whose bed he is on (within 1.6 m of her middle line, 5 m aft to 3 m
	 * forward of her root), and his bench by where the drill seats him (a row each 0.6 m aft from 1.4 m forward of her
	 * root, his side his bench's). False where he is on none.
	 * bFarAft (release 2, the scene's trial word `leave`): by his place he is taken to 8 m aft of her root (an overfull
	 * lorry's last men stand that far back in the rules and had no seat), and the men past her benches kneel down the
	 * bed's middle in the order they stand, fitted between her first bench and her last (not 0.55 m further aft a row,
	 * which is behind her tail from about the third).
	 */
	bool BenchFor(const std::vector<Benches>& All, double X, double Y, int Block, int SeatOn, int Seat, BenchSeat& Out, bool bFarAft = false);

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
		/** Effects (R2): life's seconds a brew-up waits after her kill, the least and the most (each vehicle her own
		 * between, by her number), her knocked_out clip first so her crew can get out (Vehicle::BrewWait; LandBail,
		 * -TheaterLandBailOut). 0: at once, as before. Set before Load. */
		double BrewWaitLeast = 0.0, BrewWaitMost = 0.0;
		/** Tanks rolling smoothly (R2; Vehicle::SmoothTanks, battle seconds): 0 off, as before. Set before Load. */
		double SmoothTanks = 0.0;
		int Year = 1944, Month = 7;
		/** The field's ground, from the recording (field.place.terrain; "plains" where it names none): each model is
		 * dressed for it (Dress::Terrain). Read by Load. */
		std::string Terrain = "plains";
		/** Men in lorries (the Battle rules card's row): every lorry's model dressed with her tilt stowed (Dress::Force), so
		 * the men on her benches are seen. Set before Load. */
		bool bOpenLorries = false;
		/** The vehicles' fire odds by model name (ReadFireOdds): a kill's end is drawn by them and the face struck, and
		 * the fates table shows that end. Empty: the table's own weights alone, as before. Set before Load. */
		std::map<std::string, FireOdds> Fires;
		/** How each kill ended, after Load (and as Extend picks them): the vehicle's index, the face struck and the end
		 * drawn ("" where no odds). */
		struct KillEnd { int Track = -1; std::string Model, Face, End, Fate; };
		std::vector<KillEnd> Ends;
		/** What has happened since it was last emptied: the caller plays it and clears it. */
		std::vector<Event> Events;
		/** Loaded from a live battle's header (Extend grows it); and over (its last piece has come). */
		bool bLive = false, bOver = false;
		/** One line for each model drawn as another for want of its own file (the nearest generic in Content/Models/Battle
		 * by kind and nation: never a side's vehicles left undrawn): the caller logs them. */
		const std::vector<std::string>& StandIns() const { return StandInLines; }
		/** Captured kit (release 2, Brian's 23 Nov "Do all of these"): on, a block the recording marks captured is drawn
		 * in her captor's coat over her maker's (Dress::Captured). Off unless set; set before Load. */
		bool bCapturedLook = false;
		/** Any block marked captured, whatever the switch: stands in for every tank block of the attacker taken from the
		 * defender's model (the Mac's off/on line, "-TheaterLandCapturedTest"). Set before Load. */
		bool bCapturedTest = false;
		/** National marks (release 2, Brian's 24 Nov ask for the tanks' country markings): on, every vehicle wears her
		 * owner's plain period marking at her file's mark places (Dress::bMarks). Off unless set; set before Load. */
		bool bMarks = false;
		/** The field's own ground on every vehicle's lower third (release 2, tc.Look.Worn; Dress::FieldDust). A negative
		 * red: none, as before. Set before Load. */
		double FieldDust[3] = { -1.0, -1.0, -1.0 };
		/** The burnt coat (release 2, the trial word "burntcoat"; Dress::bBurntCoat): on, every vehicle's wreck burns to
		 * soot, rust and ash, each point to its own. Off unless set; set before Load. */
		bool bBurntCoat = false;

	private:
		std::vector<Vehicle> All;
		std::vector<Round> RoundList;
		std::vector<Block> BlockList;
		std::map<std::string, Model> Looks;
		std::vector<std::string> StandInLines;
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
		bool LoadLook(const Block& B, const std::string& Role, std::string& OutKey, std::string& OutError, const std::string& Carrier = std::string(),
			const std::map<std::string, std::string>& Choose = {});
		/** How a kill looks (its fate key, and whether it brews up) from the picker, her end first drawn by her fire odds
		 * and the face struck (recorded in Ends under Index); or without a table, a fixed one in four brewing up.
		 * Rounds: those that struck her, the killing one among them. */
		std::string PickLook(const TrackWords& W, const Model& M, const std::vector<Round>& Rounds, int Index, uint8_t& OutBrew);
		/** Effects (R2): vehicle V's brew-up wait, battle seconds (BrewWaitLeast, BrewWaitMost). */
		double BrewWaitOf(size_t V) const;
	};

	/** Reads a recording's vehicle tracks, rounds and blocks (Battle::Load does it; the checks use it alone). */
	bool ReadRecording(const Json& Doc, std::vector<Block>& OutBlocks, std::vector<TrackWords>& OutTracks,
		std::vector<Round>& OutRounds, double& OutEnd, int& OutYear, int& OutMonth, std::string& OutError);

	// ---------------------------------------------------------------------------------------------------------------
	// TK1: the far level merged, for drawing far vehicles as instances (the scene's Battle/LandCrowd).

	/**
	 * One piece of a model's far level: bodies merged into one mesh as they stand at rest (every part's turn, slide
	 * and clip at nothing: the gun at its rest lay, every hatch shut, the wheels unturned), in the frame of its anchor
	 * body (model axes, metres from the anchor's pivot, as a body's own points). Drawn at the anchor's place
	 * (Vehicle::Bodies()[Anchor]) exactly as the anchor body's own mesh would be.
	 */
	struct FarPiece
	{
		int Anchor = -1;                     // the body whose place it is drawn at (-1: no piece)
		Mesh Merged;                         // points, normals, colours, soot weights and triangles (no links)
		std::vector<int> Bodies;             // the bodies merged in, in body order
		std::vector<uint32_t> FirstPoint;    // where each of Bodies' points begin in Merged, and the end (one more than Bodies)
		std::vector<uint32_t> FirstTriangle; // where each one's triangles begin (in triangles), and the end
		bool Empty() const { return Merged.Triangles.empty(); }
	};

	/** A model's far level cut for instancing: a hull piece and, where she has a turret, a turret piece. */
	struct FarSplit
	{
		enum : int { HullPiece = 0, TurretPiece = 1 };
		/** The hull (every body not under the turret, at her root body) and the turret (the turret's body and every body
		 * under it, at the turret body). A model with no turret (a gun, a lorry) has the first only. */
		FarPiece Pieces[2];
		/**
		 * By body: the piece it is drawn in past the hand-over, or -1. Each body goes in at its far level; a body with
		 * no far level, which the scene draws at its near level at every distance, stands in at its near level
		 * (StandIn), so a piece looks as the bodies looked from afar. Left out (-1, with Kept set where it has a far
		 * level, to be drawn by its own mesh as before): a body with no near level (drawn far at every distance), and a
		 * body the scene may hide while she stands (MayHide, or the caller's KeepOut: a manned roof gun's cradle) with
		 * every body hanging on it, which must never be baked into a piece.
		 */
		std::vector<int> PieceOf;
		std::vector<uint8_t> Kept, StandIn;
		/** By body: the farthest of its merged points from its pivot (metres; FarOff weighs a turn by it). */
		std::vector<double> Reach;
		/** By body: what FarOff passes by, near-only motions shown at rest from afar: 1 its turn (a wheel's spin), 2 all
		 * of it (a hatch and what hangs on it, the barrel's recoil and the breech on it). */
		std::vector<uint8_t> Loose;
		/**
		 * TK1 (the commander's lid): the piece that holds her commander's hatch (OpenOf: the turret, or the hull where she
		 * has none, as a StuG) once more, with her open_up clip played to its end (the lid up, lifted up its post first
		 * where it lifts, as the Tiger's and the Panther's): drawn in that piece's stead while her lid stands open
		 * OpenFrom or more (Vehicle::HatchOpen), so an unbuttoned tank shows her lid up past the hand-over and her
		 * commander standing clear of it, not through it. The same bodies as that piece, in the same order. Empty
		 * (OpenOf -1) where she has no open_up clip, her commander's lid is in no piece, or the clip moves a body of
		 * another piece.
		 */
		FarPiece Open;
		int OpenOf = -1;
		static constexpr double OpenFrom = 0.5;
		bool Empty() const { return Pieces[0].Empty() && Pieces[1].Empty(); }
		/** The pieces drawn: 1, or 2 with a turret piece. */
		int Count() const { return Pieces[1].Empty() ? (Pieces[0].Empty() ? 0 : 1) : 2; }
		/** The level body B is merged at (its far mesh, or its near one where it stands in). */
		const Mesh& Level(const Model& M, int B) const
		{
			const Body& Of = M.Bodies[static_cast<size_t>(B)];
			return StandIn[static_cast<size_t>(B)] ? Of.Near : Of.Far;
		}
	};

	/** Whether the scene may hide this body while her vehicle stands (a body flagged bWhileManned, where the model has
	 * that flag): such a body is never merged into a far piece. False for every body of a model without the flag. */
	bool MayHide(const Body& B);

	/** A body the ground scene draws only while her commander mans it: a German roof gun, the MG 34 in its cradle on
	 * the cupola's rail (the crews' hatch and death clips are made without it; the Sherman's .50 stays on its post).
	 * The one rule for the scene's bodies and the far pieces' KeepOut. */
	inline bool ShownWhileManned(const Model& M, const Body& B) { return B.Name == "roof_mg_cradle" && M.Nation == "GER"; }

	/**
	 * Cuts M's far level into its pieces (once a model, at load). KeepOut (by body, may be null or short): bodies the
	 * caller keeps out of the pieces as well as those MayHide names. Every triangle of a merged body's level (FarSplit::
	 * Level) is in its piece exactly once, its points carried from the body's pivot to the anchor's as they stand at
	 * rest, with its normals, colours and soot weights. Also cuts the lid-up copy (FarSplit::Open) where she has one.
	 */
	void SplitFar(const Model& M, FarSplit& Out, const std::vector<uint8_t>* KeepOut = nullptr);

	/**
	 * How far (metres) the merged bodies of a vehicle posed now (Vehicle::Bodies()) stand off where their piece draws
	 * them (their rest place on the anchor's place), the worst of them: each body's shift plus its turn times its
	 * reach. A wheel's spin, a hatch (and what hangs on it) and the barrel's recoil are passed by: near-only motions,
	 * shown shut and still from afar. PerBody (optional): each body's own (0 for a body in no piece or passed by).
	 */
	double FarOff(const Model& M, const FarSplit& Split, const std::vector<Place>& Now, std::vector<double>* PerBody = nullptr);
}
