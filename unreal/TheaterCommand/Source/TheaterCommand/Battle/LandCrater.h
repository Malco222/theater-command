#pragma once

#include "Battle/LandField.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * The ground battle's shell holes, in plain C++ so the cloud test lays the same ones the game draws (Brian on the first
 * stills, 21 November: "the graphics for the terrain need a lot of work"; a shell hole was a flat dark disc on the
 * grass). A fresh hole reads as torn earth: a raised ragged rim of thrown soil, drier and lighter at its crest; a
 * darker, damp hollow; rays of thrown soil running out over the grass; clods and torn turf scattered round it; brown
 * earth colours, never a black disc. Sized by the round: a mortar bomb, a 75 or an 88's high explosive, a 105. Every
 * hole stays for the whole battle (the art director's ruling).
 *
 * Brian, 22 November: "explosions should create craters". A hole of a mortar bomb or bigger is dug: its bowl goes down
 * into the ground (a mortar's 0.35 m, a 105's 0.8 m, a 150's 1.2 m at its middle) and the ground drawn over it is cut
 * away inside the bowl's wall (Dug), so a man stands in it, a low camera sees into it and its far wall shades as earth
 * does. The cut runs where the hole's own wall already stands over the ground (the 0.86 ring, short of the crest), so
 * the ground's cut edge is never seen. A grenade's scuff and a small round's hole are laid over the ground as before,
 * their floor a few centimetres up. Older holes, the tracks' marks and the ground are cut alike by a new hole's cut,
 * and a new hole is laid on what is there: the floor of an older one where it was torn in it. Colours are 8-bit sRGB vertex colours for the game's M_MapSurface (made linear
 * there), as LandField's are; the meshes are LandField::Mesh, in the field's frame (metres, x east, y north, z up),
 * counter-clockwise seen from above.
 *
 * Cost: a 105's hole is some 270 points and 480 triangles, a mortar's about 200 and 340; a few hundred holes by the
 * end of a battle are some 100,000 points. They are kept by place: each 64 m square of the field (a Patch) is its own
 * mesh in the game, its holes in sections of PerChunk. The engine copies and uploads every section of a mesh again
 * when any one of them changes, and the shadows cached under that mesh's bounds are drawn again; so a new hole makes
 * the game send one section of one small mesh, and only the shadows in and round that square are redrawn.
 */
namespace LandCrater
{
	using LandField::Rgb8;

	/** How big a hole a round leaves. */
	struct Size
	{
		/** The hole across its rim's crest, metres (0: no hole). */
		double Across = 0.0;
		/** How high the rim's crest stands over the ground, metres. */
		double Rim = 0.0;
		/** How far the thrown soil reaches, as a share of the hole's radius (its rays a little further). */
		double Reach = 1.9;
		/** The clods thrown round it, and the biggest's size, metres. */
		int Clods = 0;
		double Clod = 0.1;
		/** Points round each ring (fewer for the small holes). */
		int Sides = 20;
		/** How deep its bowl goes under the ground at its middle, metres (0: laid over the ground, nothing cut). */
		double Deep = 0.0;
	};

	/**
	 * The hole a round leaves, by the effects kit's name for it (LandPlay::Effect::Name) and its calibre: "grenade" a
	 * scuff half a metre across; "light" (a 37 to 57 mm high explosive) 0.9 m; "mortar" (the 81 mm bomb) 1.3 m; "gun75"
	 * a tank's or anti-tank gun's high explosive, 1.5 m for a 75 and 1.8 m for an 88 (by CalibreMm); "howitzer" or
	 * "field" (the 105) 2.8 m; "heavy" (a 150) 4 m. Any other name: by the calibre, a 75's from nothing given.
	 */
	Size SizeOf(const std::string& Name, double CalibreMm);

	/** The soil a hole is torn in (8-bit sRGB): its damp hollow, its wall, its dry crest, the soil thrown out, the
	 * thrown soil's far thin edge over the grass, and the clods (and torn turf's green). */
	struct Soil
	{
		Rgb8 Hollow, Wall, Crest, Thrown, Edge, Clod, Turf;
	};
	/** By the effects kit's ground word: "earth" (the bocage's brown loam), "woods" (darker, with leaf mould), "mud",
	 * "town" (grey with stone and plaster), "sand", "snow" (brown-black earth thrown on the white). Any other: earth. */
	Soil SoilOf(const std::string& Ground);

	/** The drawn ground's height at a place (the field's x and y, metres). */
	using HeightFn = std::function<double(double X, double Y)>;
	/** The drawn ground's own colour at a place (8-bit sRGB): LandField::Ground::ColourAt. */
	using ColourFn = std::function<Rgb8(double X, double Y)>;
	/** The drawn ground's normal at a place, as it is lit there (unit, up): LandField::Ground::DrawnUp. */
	using UpFn = std::function<void(double X, double Y, double& OutX, double& OutY, double& OutZ)>;

	/**
	 * Lays one hole at X, Y into Out (appended), draped on the ground by Height; Seed makes it its own (the same seed,
	 * the same hole). Lift: metres more over the ground for this hole's floor, so two holes laid over each other do not
	 * flicker (Field gives each hole its own few millimetres). Nothing for a size of no width. Under, when given, is the
	 * ground's own colour: the thrown soil's thin edge takes it exactly and the ring inside it leans to it, so the hole
	 * fades into whatever it was torn in (stubble, corn, pasture) with no outline; torn turf takes the ground's colour
	 * too. The same seed lays the same shape with it or without it. A size with Deep digs its bowl under Height and,
	 * when OutCut is given, writes there the outline to cut the ground by (x, y pairs, counter-clockwise, convex: the
	 * hull of its wall's 0.86 ring, where the wall already stands over the ground). The thrown soil is lit by Lit's
	 * normal toward its edge (by the slope of Height across the drawn ground's spacing where none is given), so it is
	 * lit as the ground round it.
	 */
	void Lay(double X, double Y, const Size& S, const Soil& Paint, uint64_t Seed, const HeightFn& Height, double Lift, LandField::Mesh& Out,
		const ColourFn& Under = ColourFn(), std::vector<double>* OutCut = nullptr, const UpFn& Lit = UpFn());

	/**
	 * Cuts out of a mesh what lies inside a convex outline seen from above (x, y pairs, counter-clockwise): every
	 * triangle that reaches into it is replaced by the pieces of it left outside, on its own plane, their points'
	 * normals and colours taken across it, so the surface outside is unchanged to the last millimetre. Triangles from
	 * index KeepFrom to KeepTo (in Triangles, by threes) are left as they are (a new hole's own). Returns whether
	 * anything was cut.
	 */
	bool CutOut(LandField::Mesh& M, const std::vector<double>& Outline, size_t KeepFrom = 0, size_t KeepTo = 0);
	/** Whether X, Y lies inside a convex outline (x, y pairs, counter-clockwise). */
	bool Inside(const std::vector<double>& Outline, double X, double Y);

	/**
	 * A hedge cutter's breach (release 2; Brian, 22 November 14:19Z, "Let's do 1 and 2": bladed Shermans punch a gap
	 * through a hedgerow where the player picks). Where the bank stood, its earth is torn out down to the field over
	 * the gap; the bank's cut ends are left as ragged faces of fresh earth falling into it, a lip of torn turf at their
	 * tops; the spoil and torn turf she pushed lie heaped on the side she drove out to, a little spilled on the other;
	 * clods strewn. The ground drawn is cut over the bank's footprint (as a dug hole's bowl is), so the tank and the men
	 * go through on the gap's floor. X, Y: the gap's middle on the bank's line; Yaw: the bank's heading there (radians
	 * from east); Half: half the gap at the field's level; Foot: how far the bank's foot reaches either side of its
	 * line; Through: the side she drove out to, +1 to the left of Yaw, -1 to its right.
	 */
	struct Breach
	{
		double X = 0.0, Y = 0.0, Yaw = 0.0, Half = 2.0, Foot = 1.4;
		int Through = 1;
	};
	/** The banks' rise over the land at a place (LandField::Ground::BankRise): what a breach takes away. */
	using RiseFn = std::function<double(double X, double Y)>;

	/** All of a battle's holes, kept for the whole battle, grouped by place for the game's meshes. */
	class Field
	{
	public:
		/** Holes a section of a patch's mesh. */
		static constexpr int PerChunk = 24;
		/** A patch's side, metres: a hole belongs to the square its middle is in (its thrown soil may reach a few
		 * metres past the square's edge). */
		static constexpr double PatchSize = 64.0;
		/** Beyond this many, no more are laid (never reached by a battle of the dry run's size; a guard). */
		static constexpr int Most = 2400;

		/** One square of the field's holes: one mesh in the game, one section for each PerChunk of its holes. */
		struct Patch
		{
			/** Which square: its south-west corner is at I * PatchSize, J * PatchSize. */
			int I = 0, J = 0;
			std::vector<LandField::Mesh> Sections;
			int Count = 0;
		};
		/** A section of a patch's mesh. */
		struct Place
		{
			int Patch = 0, Section = 0;
		};

		/** InUnder: the ground's own colour (Lay's Under), or none for the earth colours alone. */
		void Begin(HeightFn InHeight, uint64_t InSeed, ColourFn InUnder = ColourFn());
		/** The soil a hole on plain ground ("earth") throws, where the ground is not the bocage's loam: "blackearth" on
		 * the steppe. Empty for the loam. */
		void SetEarth(const std::string& Word) { Earth = Word; }
		/** A round landed at X, Y on Ground (the effects kit's word): its hole laid, by its name and calibre (SizeOf).
		 * False when it leaves none (a name of no size, water, the guard reached). Dug only where Dig says it may be
		 * (open ground: a field, a pasture, a garden; not a lane, a bank, a yard), when Dig is given. */
		bool Add(double X, double Y, const std::string& Name, double CalibreMm, const std::string& Ground);
		/** A hedge cutter's breach torn (Breach), laid as a hole is: its own points in its patch's section, the ground
		 * drawn and older holes cut over the bank's footprint (TakeCuts), the grass round the eye left bare over it and
		 * its spoil. Rise: the banks' rise (the gap's floor is Begin's Height less it). Ground: the effects kit's word
		 * for the soil. False with no Rise, a gap of no width, or past the guard. */
		bool AddBreach(const Breach& B, const RiseFn& Rise, const std::string& Ground);
		/** The floor a vehicle drives on inside a breach's cut (its torn earth, without the clods): false outside every
		 * breach's cut. */
		bool BreachFloor(double X, double Y, double& OutZ) const;
		/** Where a hole may be dug (set before the first Add; none: anywhere). */
		std::function<bool(double X, double Y)> Dig;
		/** The drawn ground's normal, to light the thrown soil by (Lay's Lit; set before the first Add). */
		UpFn Lit;
		/** The game's and the checks' one reading of Dig: open ground (a field of corn or stubble, a pasture, rough
		 * grass, the country beyond), not a lane, a bank, a yard, a garden or orchard, the woods (a tree or a hedge would
		 * hang over a bowl dug under it), a floor, a wall's line, water or marsh. G must outlive the function. */
		static std::function<bool(double X, double Y)> DigOn(const LandField::Ground& G)
		{
			const LandField::Ground* Gp = &G;
			return [Gp](double X, double Y) {
				switch (Gp->At(X, Y).What)
				{
				case LandField::Drawn::Crop:
				case LandField::Drawn::Pasture:
				case LandField::Drawn::Rough:
				case LandField::Drawn::Beyond:
					return true;
				default:
					return false;
				}
			};
		}
		/** The cuts made since the last call (each an outline for CutOut): the game cuts its ground and the tracks'
		 * marks by them. Older holes are cut here, by Add. */
		std::vector<std::vector<double>> TakeCuts();
		/** Whether a point is inside a dug hole's cut (the ground is gone there: what stands on it stands on TopAt). */
		bool InCut(double X, double Y) const;
		/** The surface there now: TopAt inside a cut, else the ground (Begin's Height). */
		double SurfaceAt(double X, double Y) const;
		/** The sections a hole was laid in since the last call, in the order laid: each to be sent to its patch's mesh
		 * again, whole. A patch is numbered when its first hole is laid, so a new patch's number is always the next. */
		std::vector<Place> TakeChanged();
		int Patches() const { return static_cast<int>(PatchList.size()); }
		const Patch& PatchAt(int P) const { return PatchList[static_cast<size_t>(P)]; }
		const LandField::Mesh& Section(const Place& Where) const { return PatchAt(Where.Patch).Sections[static_cast<size_t>(Where.Section)]; }
		int Count() const { return static_cast<int>(Holes.size()); }
		/** Whether a point is in a hole or on its rim (the grass and corn laid round the eye leave it bare). */
		bool Bare(double X, double Y) const;
		/**
		 * The holes' own face at a point: the highest of their floors, walls, crests, thrown soil and clods over X, Y
		 * (OutZ, metres, the field's height), read from the triangles laid; false where no hole reaches. A hole's rim
		 * stands over the drawn ground (a 105's crest some 0.19 m, a 150's 0.27 m), so a body lying across a rim is laid
		 * on this face, not on the ground under it; inside a dug hole's cut it is the bowl's floor, under the ground. A
		 * hole is not read where a later hole's cut has taken it away.
		 */
		bool TopAt(double X, double Y, double& OutZ) const;

		struct Hole
		{
			double X = 0.0, Y = 0.0;
			/** Its radius at its rim's crest, and how high its crest stands. */
			double R = 0.0, Rim = 0.0;
			/** Which section of which patch holds its points. */
			Place In;
			/** Its first point in that section (the middle of its floor), the points round each ring, its clods. */
			uint32_t First = 0;
			int Sides = 0, Clods = 0;
			/** How far its thrown soil reaches from its middle at the farthest, metres. */
			double Out = 0.0;
			/** A dug hole's bowl depth and its cut's outline (x, y pairs; empty: laid over the ground). */
			double Deep = 0.0;
			std::vector<double> Cut;
			/** A hedge cutter's breach (AddBreach), not a shell's hole: its points a grid in the bank's frame (u along
			 * Yaw, v to its left), NU by NV from First, Step apart from U0, V0; the gap's cut CutU along and CutV across,
			 * the spoil out to SpoilOut on the Through side, BackOut on the other. */
			bool bBreach = false;
			double Yaw = 0.0, U0 = 0.0, V0 = 0.0, Step = 0.3, CutU = 0.0, CutV = 0.0, SpoilOut = 0.0, BackOut = 0.0;
			int NU = 0, NV = 0, Through = 1;
		};
		const std::vector<Hole>& All() const { return Holes; }

	private:
		HeightFn Height;
		ColourFn Under;
		uint64_t Seed = 1;
		std::string Earth;
		std::vector<Hole> Holes;
		/** The patches by number, and their numbers by square. */
		std::vector<Patch> PatchList;
		std::unordered_map<int64_t, int> PatchOf;
		std::vector<Place> Changed;
		std::vector<std::vector<double>> Cuts;
		/** The holes by 8 m squares of the field. */
		std::unordered_map<int64_t, std::vector<int>> Squares;
		static int64_t SquareOf(int I, int J);
		/** Whether a hole laid after hole After has cut X, Y away. */
		bool CutAfter(int After, double X, double Y) const;
		/** The patch and section a new hole at X, Y is laid in (its patch numbered when its first hole is). */
		Place PlaceFor(double X, double Y);
		/** A new hole laid in Into's section from FirstIndex on: older holes cut by its cut (CutLine, kept in H and
		 * TakeCuts), filed by the 8 m squares its Reach touches, its section listed as changed. */
		void Keep(Hole& H, std::vector<double>&& CutLine, size_t FirstIndex, double Reach);
		/** A breach's grid face at X, Y (false off its grid). */
		bool OnBreach(const Hole& H, double X, double Y, double& OutZ) const;
	};

	/**
	 * The ground drawn, cut by the dug holes: a field tile's near ground (LandField::Tile::Near, 250 m) is split, the
	 * first time a hole is dug in it, into Split x Split parts by each triangle's middle (nothing moves, so no seam), each
	 * its own mesh in the game from then on: a new hole then sends only the part or two it is cut in (some 600 triangles,
	 * a 62 m square of shadows), never the tile. The far level is left whole: at 700 m a hole is a few pixels, and its
	 * rim and thrown soil, laid over the ground, still show it.
	 */
	class Dug
	{
	public:
		static constexpr int Split = 4;
		struct Place
		{
			int Tile = 0, Part = 0;
		};
		/** The field's tiles (LandField::Ground::Tiles(); it must outlive this). */
		void Begin(const std::vector<LandField::Tile>* InTiles);
		/** A hole's cut (Field::TakeCuts): the parts it reaches cut. */
		void Cut(const std::vector<double>& Outline);
		/** The tiles split since the last call: their near ground is drawn by parts from now on. */
		std::vector<int> TakeSplit();
		/** The parts cut since the last call (each to be sent again, whole). */
		std::vector<Place> TakeChanged();
		bool IsSplit(int Tile) const { return Tile >= 0 && static_cast<size_t>(Tile) < Parts.size() && !Parts[static_cast<size_t>(Tile)].empty(); }
		const LandField::Mesh& PartOf(const Place& Where) const { return Parts[static_cast<size_t>(Where.Tile)][static_cast<size_t>(Where.Part)]; }
		int Cuts() const { return CutCount; }

	private:
		const std::vector<LandField::Tile>* Tiles = nullptr;
		std::vector<std::vector<LandField::Mesh>> Parts;
		std::vector<std::vector<float>> Boxes; // per tile, per part: x0, y0, x1, y1
		std::vector<int> Split_;
		std::vector<Place> Changed;
		int CutCount = 0;
		void SplitTile(int T);
	};

	/**
	 * The tanks' tracks on the ground (Brian, 21 November: "the way the units move and the landscape"; a tank drove over
	 * standing corn and pasture and left no mark). Under each track, as she drives, a strip of pressed ground: on a
	 * pasture flattened grass with the soil showing in its middle, in standing corn the straw crushed into the soil, on
	 * bare earth or a lane a dark rut; in corn the corn between her tracks laid flat as well, the whole of her width. The
	 * strips fade at their edges to the ground's own colour (Under), as the holes' thrown soil does, and stay for the
	 * whole battle. They are kept by place as the holes are (a mesh a 64 m square, in sections), laid a few millimetres
	 * over the ground and under the holes' floors (a hole torn in a rut covers it). Where a track ran, the grass and corn
	 * laid round the eye leave the ground bare (Flat).
	 */
	class Ruts
	{
	public:
		/** What the ground is at a point, for the strips' colour: none where no mark is left (water, a floor, a wall). */
		enum class Under : uint8_t { None, Grass, Corn, Earth };
		using KindFn = std::function<Under(double X, double Y)>;

		static constexpr double PatchSize = 64.0;
		/** Marks a section of a patch's mesh. */
		static constexpr int PerChunk = 48;
		/** How far she drives between one laid piece and the next, metres. */
		static constexpr double Step = 1.2;
		/** Beyond this many pieces, no more are laid (a guard: a battle's tanks lay some thousands). */
		static constexpr int Most = 40000;
		/** A strip's points across her way at each of its two ends, laid left to right an end at a time (the checks read
		 * them so): the outer edge (the ground's own colour), the feathered band, the pressed strip's two, the band, the
		 * edge. */
		static constexpr int StripAcross = 6;

		struct Patch
		{
			int I = 0, J = 0;
			std::vector<LandField::Mesh> Sections;
			int Count = 0;
		};
		using Place = Field::Place;

		void Begin(HeightFn InHeight, ColourFn InUnder, KindFn InKind);
		/** What a track leaves on this ground (the game's and the checks' one reading): standing corn laid flat, pasture,
		 * rough and gardens pressed grass, stubble, plough, woods and marsh torn earth; nothing on a lane's floor (its own
		 * ruts), a cobbled place, a bank, a wall's line, a house's floor or water. G must outlive the function. */
		static KindFn KindOn(const LandField::Ground& G)
		{
			// (here in the header: a check that never asks it needs no ground of its own)
			const LandField::Ground* Gp = &G;
			return [Gp](double X, double Y) {
				const LandField::Surface Su = Gp->At(X, Y);
				const std::string Fx = Su.Fx;
				if (Fx == "town" || Fx == "water") return Under::None;
				switch (Su.What)
				{
				case LandField::Drawn::Crop: return Gp->CornAt(X, Y) ? Under::Corn : Under::Earth;
				case LandField::Drawn::Pasture:
				case LandField::Drawn::Rough:
				case LandField::Drawn::Village:
				case LandField::Drawn::Beyond:
					return Fx == "earth" ? Under::Grass : Under::Earth;
				case LandField::Drawn::Works:
				case LandField::Drawn::Woods:
				case LandField::Drawn::Marsh:
					return Under::Earth;
				default: return Under::None; // (a lane's floor, a yard, a bank, a wall's line, a floor, water)
				}
			};
		}
		/** A vehicle's track (or tyre) width, metres, by her model's name and kind: a Tiger's 0.725 m battle tracks, a
		 * Sherman's 0.42, a Panzer IV's 0.40, a lorry's tyres and a gun's wheels narrower. */
		static double TrackWidthOf(const std::string& Name, const std::string& Kind);
		/**
		 * Vehicle V now: the middles of her two tracks on the ground (field metres; left, right), each track's width. A
		 * piece of each track is laid once she has driven Step from the last; the first call only marks where she is.
		 * Returns whether anything was laid.
		 */
		bool Drive(int V, double LX, double LY, double RX, double RY, double Width);
		/** Vehicle V's marks start afresh at her next Drive (she was seen to jump: a seek, or she left the field). */
		void Lift(int V);
		/** The sections a piece was laid in since the last call (each to be sent to its patch's mesh again, whole). */
		std::vector<Place> TakeChanged();
		int Patches() const { return static_cast<int>(PatchList.size()); }
		const Patch& PatchAt(int P) const { return PatchList[static_cast<size_t>(P)]; }
		const LandField::Mesh& Section(const Place& Where) const { return PatchAt(Where.Patch).Sections[static_cast<size_t>(Where.Section)]; }
		int Pieces() const { return Count; }
		/** Whether a point is under a track's mark, or between a tank's tracks where she laid the corn flat. */
		bool Flat(double X, double Y) const;
		/** A dug hole's cut (Field::TakeCuts): the marks inside it cut away. */
		void Cut(const std::vector<double>& Outline);
		/** Where the ground is cut (Field::InCut): no mark is laid across it. */
		std::function<bool(double X, double Y)> DugAt;
		/** The drawn ground's normal, each strip lit by it (none: the slope of the height across the ground's spacing). */
		UpFn Lit;

	private:
		struct Last
		{
			bool bOn = false;
			double LX = 0.0, LY = 0.0, RX = 0.0, RY = 0.0;
		};
		/** A laid piece, for Flat: from A to B the way she drove, her tracks Apart between middles, Width each; Corn:
		 * the corn between them laid too. */
		struct Piece
		{
			double AX = 0.0, AY = 0.0, BX = 0.0, BY = 0.0, Apart = 0.0, Width = 0.0;
			bool bCorn = false;
		};
		HeightFn Height;
		ColourFn UnderColour;
		KindFn Kind;
		std::vector<Last> Was;
		std::vector<Piece> Laid;
		std::vector<Patch> PatchList;
		std::unordered_map<int64_t, int> PatchOf;
		std::vector<Place> Changed;
		std::unordered_map<int64_t, std::vector<int>> Squares;
		int Count = 0;
		/** One strip from (AX, AY) to (BX, BY), about Half wide each side of its line, about Edge more at each side
		 * feathering to the ground (each end its own, by its place and Salt); Paint at its middle, mottled; Lift metres
		 * over the ground, lit as the ground is. Into the mesh. */
		void Strip(LandField::Mesh& Out, double AX, double AY, double BX, double BY, double Half, double Edge, const Rgb8& Paint, double Lift,
			uint64_t Salt) const;
		Patch& PatchFor(double X, double Y, Place& OutIn);
		static int64_t SquareOf(int I, int J) { return (static_cast<int64_t>(I) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(J)); }
	};
}
