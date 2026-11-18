#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * The battle effects, worked out in plain C++ so that the same code runs in the game and in the cloud preview
 * (tools/battle-fx/preview): a shell landing, a gun firing, tracer, a burning wreck, the crater left behind and the
 * camera's jolt. Every number comes from the recipe, Content/Effects/battle-fx.json (tools/battle-fx/fx.json), so the
 * look is tuned there and not here.
 *
 * Nothing here knows Unreal. Places are in metres, Z up, time in seconds. What it gives back each frame is a list of
 * soft puffs, each a middle point and two rings of points (the shape the campaign map's smoke uses, with
 * game.puff_sides points a ring, so one material serves both), already turned to the eye, coloured, and laid far to
 * near; a few lamps for the flashes; and the camera's jolt, which the caller adds to its own camera if it wants it.
 *
 * Lessons the shapes follow (the art director's sheet, art-director/explosions):
 * - the earth is the star of a land hit: a wide torn fan of dark jets, uneven, thinnest and torn at the tip, clods
 *   flying on beyond; never a dome, a ball, a hand of fingers or a broom of even strokes;
 * - many small overlapping puffs of varied size, never a few big round ones (they read as eggs, grapes, cotton);
 * - a flash lights the ground for one frame and is gone by the third; no orange fireball on a ground hit;
 * - smoke climbs, leans downwind and lingers; a crater stays for the whole battle.
 */
namespace TheaterFx
{
	struct Vec
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		Vec() = default;
		Vec(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
		Vec operator+(const Vec& O) const { return Vec(X + O.X, Y + O.Y, Z + O.Z); }
		Vec operator-(const Vec& O) const { return Vec(X - O.X, Y - O.Y, Z - O.Z); }
		Vec operator*(double S) const { return Vec(X * S, Y * S, Z * S); }
		Vec operator-() const { return Vec(-X, -Y, -Z); }
		Vec& operator+=(const Vec& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
		double Dot(const Vec& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		Vec Cross(const Vec& O) const { return Vec(Y * O.Z - Z * O.Y, Z * O.X - X * O.Z, X * O.Y - Y * O.X); }
		double Length() const;
		/** This one made a metre long, or Otherwise when it has no length. */
		Vec Unit(const Vec& Otherwise = Vec(0.0, 0.0, 1.0)) const;
	};

	/** A colour as light (linear), 0 to 1. Picture colours from the recipe (sRGB 0 to 255) are turned to these. */
	struct Rgb
	{
		float R = 0.0f, G = 0.0f, B = 0.0f;
		Rgb() = default;
		Rgb(float InR, float InG, float InB) : R(InR), G(InG), B(InB) {}
		static Rgb FromPicture(double R255, double G255, double B255);
		Rgb operator*(double S) const;
		Rgb Mix(const Rgb& O, double T) const;
	};

	/** Where the numbers come from. The path is dotted, with a list's places as numbers: "sizes.how105.crown_m.0". */
	class Recipe
	{
	public:
		virtual ~Recipe() = default;
		virtual bool Find(const char* Path, double& Out) const = 0;
		/** A word at Path (the calibre map's sizes); none by default. */
		virtual bool FindWord(const char* Path, std::string& Out) const
		{
			(void)Path;
			(void)Out;
			return false;
		}
	};

	/** A small steady random source, the same on every machine, so a battle's effects can be played again alike. */
	class Dice
	{
	public:
		explicit Dice(uint64_t Seed = 1);
		double U(); // 0 to 1
		double U(double A, double B);
		int Whole(int A, int B); // A to B, both included
		double Bell(); // a normal spread, middle 0, width 1
	private:
		uint64_t State = 1;
		uint64_t Next();
	};

	/** One thing drawn: a puff of smoke, dust or earth, a tongue of flame, a clod, a spark, a tracer, a crater's mark. */
	struct Bit
	{
		enum class Shape : uint8_t
		{
			Puff, // turned to the eye; drawn out along Along (on the screen) when Stretch is over 1
			Upright, // stands on the ground and turns about the upright only: low skirts of dust, which never cut into the ground
			Flat, // lies on the ground (a crater's mark)
			Streak // drawn out along its own flight: clods, sparks, tracer; never thinner than LeastPx on the screen
		};
		Shape Form = Shape::Puff;
		Vec From, Vel, Along;
		double Born = 0.0, Life = 1.0;
		double Drag = 0.0, Gravity = 0.0, Rise = 0.0, Windage = 0.0;
		double R0 = 0.1, R1 = 0.2, Grow = 0.5; // radius when born, at the end of its life, and how soon it gets there
		double Stretch = 1.0, Flatten = 1.0, Point = 0.0, Tail = 0.0, Spin = 0.0;
		double HalfLength = 0.0; // (a tracer's streak: half its length in metres)
		double Alpha = 1.0, FadeIn = 0.02, Hold = 0.2, Cool = 1.0;
		double HazeBy = 0.0, HazeAlpha = 0.0; // (a skirt: settles to HazeAlpha by HazeBy seconds)
		double Heart = 0.5, Rim = 0.0; // how thick it is at its heart ring, and at its edge, as shares of its middle
		double Inner = 0.5; // (where the heart ring sits, as a share of the radius: further out, a denser body with a thinner soft edge)
		double LeastPx = 0.0;
		double ShadeK = 1.0; // (how strongly it is lit on its sunward side and shadowed on the other: a skirt's lit top and dark foot)
		double Rough = 0.0, Turn = 0.0, TurnRate = 0.0; // (a clod: how lumpy its outline is, and how it tumbles)
		uint32_t LumpSeed = 0;
		// (smoke: the middle of its cloud, so a puff on the cloud's sunward side is lit as a whole and one on the far
		// side is in its shadow; the middle drifts with the wind and climbs at CentreRise)
		Vec Centre;
		double CloudShade = 0.0, CloudR = 1.0, CentreRise = 0.0;
		Rgb Col, Col2;
		bool bLight = false; // flame, flash, sparks, tracer: keeps its own colour by night and takes no shading
		bool bHug = false; // rides the ground as it moves (skirts)
		// (a thrown clod: true flight under gravity, landing at Land seconds and once more at Land2 after one bounce)
		bool bThrown = false;
		double Land = 1e9, Land2 = 1e9;
		Vec Vel2, At1, At2;
	};

	class Battle
	{
	public:
		/** The ground's height (metres) at X, Y (metres). */
		using GroundFn = double (*)(void* Context, double X, double Y);

		void Setup(const Recipe* InRecipe, GroundFn InGround, void* InContext, uint32_t Seed);
		void SetWind(const Vec& MetresPerSecond) { Wind = MetresPerSecond; }
		void Clear();

		/** The size row (sizes.<kind>) a calibre from the field battle draws (from_shell_cal): "howitzer" -> "how105". */
		std::string SizeOfCalibre(const char* Calibre) const;

		/** A shell, mortar bomb, grenade or bomb landing at At at Now. Size is a sizes row ("how105"), Ground a grounds
		 * row ("earth", "sand", "snow", "mud", "town", "woods", "water"). On armour: sparks and a grey puff, no earth. */
		void Shell(double Now, const Vec& At, const char* Size, const char* Ground, bool bOnArmour = false);
		/** An armour-piercing round that misses: a low spurt of earth and a skip, no crown and no crater. */
		void Skip(double Now, const Vec& At, const Vec& Heading, const char* Ground);
		/** A gun firing, its muzzle at At, pointing along Facing. Gun is a muzzle row ("tank_gun", "mg", "howitzer"). */
		void Muzzle(double Now, const Vec& At, const Vec& Facing, const char* Gun);
		/** Rounds fired by one gun (Gun: any number that is the same for the same gun) from From toward To, PerSecond:
		 * every fifth is a tracer (tracer.one_in), out of step between guns; machine guns flicker at the muzzle. */
		void Shots(double Now, int Gun, const Vec& From, const Vec& To, const char* Weapon, int Rounds, double PerSecond);
		/** A gun on an aircraft firing, called every frame while its trigger is down. Muzzle is where the gun is now;
		 * Aim the point this gun is laid on (the battle decides lead and convergence, so wing guns converge where it
		 * says); Carry the aircraft's own velocity (metres a second). Kind is an air_guns row ("rifle", "hmg",
		 * "cannon20"); RatePerMin and MuzzleMps of 0 take the row's. The kit keeps each gun's next round and lays the
		 * rounds fired since the last call along the muzzle's path, so the stream stays on the gun however the
		 * aircraft turns. Every round flickers at the muzzle; one in five (the row's tracer_every) is a tracer, the
		 * same streak as on the ground, flying at the gun's speed plus the aircraft's, slowing and falling. */
		void AirGun(double Now, int Gun, const Vec& Muzzle, const Vec& Aim, const Vec& Carry, const char* Kind, double RatePerMin = 0.0, double MuzzleMps = 0.0);
		/** The trigger let go: the gun's next AirGun starts a new burst (its belt goes on where it was). */
		void AirGunStop(int Gun);
		/** Something burning at At, about Across metres wide, flames for Seconds, then a thinning grey column. */
		void Burn(double Now, const Vec& At, double Across, double Seconds);

		/** What the eye is: where it is, which way its screen's right, up and forward point, and how many pixels a
		 * radian across the screen is (the screen's width / (2 tan(field of view / 2))). Smoke takes SmokeLight (the
		 * hour: white by day); Sun is the way toward the sun. */
		struct View
		{
			Vec Eye, Right, Up, Forward;
			double PixelsPerRadian = 1000.0;
			Rgb SmokeLight = Rgb(1.0f, 1.0f, 1.0f);
			Vec Sun = Vec(-0.45, -0.35, 0.82);
		};
		/** Lays everything alive at Now for this eye into Points and Colours (PerPuff points a puff, far to near).
		 * Returns how many puffs; the rest of the room up to the last frame's count is folded away. */
		int Draw(double Now, const View& Eye);
		/** Each puff is a middle and two rings of Ring() points (game.puff_sides, 16 unless the recipe says less), so
		 * PerPuff() points; it is fixed at Setup, so the mesh's triangles never change. */
		int Ring() const { return Sides; }
		int PerPuff() const { return PointsPerPuff; }
		int Room() const { return MaxPuffs; }
		const std::vector<Vec>& Points() const { return Out; }
		const std::vector<uint8_t>& Colours() const { return OutRgba; } // four a point: red, green, blue, alpha (linear)
		bool Busy(double Now) const;

		struct Lamp
		{
			Vec At;
			Rgb Col;
			double Strength = 0.0; // 1 is a 105 mm shell's flash
			double Radius = 30.0;
		};
		/** The strongest lamps lit at Now, at most Room of them. */
		int Lamps(double Now, Lamp* Into, int Room) const;

		struct Jolt
		{
			Vec Offset; // metres
			double Roll = 0.0; // degrees
		};
		/** The camera's jolt at Now for an eye at Eye, from every hit close enough; nothing from far ones. */
		Jolt Shake(double Now, const Vec& Eye) const;

	private:
		/** A range from the recipe, read once before a loop, so a loop of puffs does not ask the recipe each time. */
		struct Span
		{
			double A = 0.0, B = 0.0;
			double Pick(Dice& D) const { return D.U(A, B); }
		};
		/** An air_guns row, read from the recipe once a kind. */
		struct AirRow
		{
			std::string Name;
			double Rate = 800.0, Mps = 870.0, Flash = 0.45, Strike = 0.8, Drag = 0.36, Drop = 9.81, TracerS = 2.4, Spread = 0.004;
			double SmokeS = 0.8, SmokeAlpha = 0.1, Lamp = 0.0;
			int OneIn = 5, SmokeEvery = 3, Licks = 0;
		};
		/** One air gun: its belt, its next round, and where it was at the last call (to lay rounds between calls). */
		struct Firing
		{
			int Gun = 0, Count = 0;
			bool bOn = false;
			double Last = 0.0, Next = 0.0, RateK = 1.0;
			Vec Muzzle, Dir, Carry;
		};
		struct Flash
		{
			Vec At;
			Rgb Col;
			double Born = 0.0, Life = 0.0, Strength = 0.0, Radius = 30.0, Flicker = 0.0;
		};
		struct Knock
		{
			Vec At;
			double Born = 0.0, Strength = 0.0, Within = 0.0, Push = 0.0;
		};
		struct Source
		{
			Vec At;
			double Start = 0.0, Flames = 0.0, End = 0.0, Across = 1.0, Next = 0.0, NextSmoke = 0.0;
			int Kind = 0; // 0 a burning thing, 1 a smouldering crater
			uint64_t Seed = 1;
		};
		struct Paint
		{
			Rgb Soil, Heavy, Dust, Clod, Smoke, First, Splinter;
			bool bFirst = false, bSplinter = false;
			double Grain = 1.0; // (the crown's puffs as a share of their size: under 1, broken and granular, as snow)
			double FirstAtFoot = 1.0, FirstAtTip = 1.0; // (how much of a jet is the ground's first colour, foot to tip)
		};

		const Recipe* Book = nullptr;
		GroundFn GroundAt = nullptr;
		void* GroundContext = nullptr;
		Vec Wind = Vec(1.6, 0.4, 0.0);
		uint64_t Seeds = 1;
		int MaxPuffs = 3000;
		int Sides = 16, PointsPerPuff = 33;
		std::vector<double> RingCos, RingSin, Ring2Cos, Ring2Sin, Ring3Cos, Ring3Sin, Ring5Cos, Ring5Sin; // (made once at Setup, so no puff works out a cosine a side each frame)
		int LastDrawn = 0;
		double ShadeLit = 0.18, ShadeDark = 0.28, ClodLeast = 1.2, Beyond = 1500.0;
		Span ClodRough, ClodTurns; // (read at Setup: every clod asks)
		double ClodShade = 2.5;
		Vec LastEye;
		bool bEyeKnown = false; // (the eye of the last frame drawn: a hit close to it throws clods past the lens)
		std::vector<Bit> Bits; // alive or still to be born
		std::vector<Bit> Marks; // craters: for the whole battle
		std::vector<Flash> Flashes;
		std::vector<Knock> Knocks;
		std::vector<Source> Sources;
		std::vector<int> GunRounds; // rounds fired by each gun so far
		std::vector<AirRow> AirRows;
		std::vector<Firing> Firings; // the air guns
		// (the tracer's look, read at Setup: one streak for every gun, on the ground and in the air)
		double TracerHalf = 8.0, TracerLeast = 3.0;
		Rgb TracerCol, TracerPale, FlameCol, FlashCoreCol, GunSmokeCol;
		std::vector<Vec> Out;
		std::vector<uint8_t> OutRgba;
		struct Order
		{
			const Bit* B;
			double Deep;
		};
		std::vector<Order> Sorted;

		double Num(const std::string& Path, double Fallback) const;
		double Pick(Dice& D, const std::string& Path, double Low, double High) const; // a number in the recipe's range
		Span Range(const std::string& Path, double Low, double High) const;
		Rgb Picture(const std::string& Path, const Rgb& Fallback) const;
		bool Has(const std::string& Path) const;
		double Height(double X, double Y) const;
		Paint PaintOf(const std::string& Ground) const;
		void Throw(Bit& B) const; // a clod's flight worked out against the ground
		void Chunk(Bit& B, Dice& D) const; // a clod's lumpy, tumbling, lit-and-dark shape

		void Wink(double T0, const Vec& At, double Scale, Dice& D);
		void Skirt(double T0, const Vec& At, double K, const Paint& P, double DustK, Dice& D);
		void Crown(double T0, const Vec& At, double CrownH, double K, double Slow, const Paint& P, double ClodK, const std::string& Size, Dice& D);
		void Smoke(double T0, const Vec& At, double CrownH, double K, double Scale, const std::string& Size, const std::string& Ground, const Paint& P, double DustK, Dice& D);
		void Scar(double T0, const Vec& At, double Across, const Paint& P, bool bSnow, Dice& D);
		void Armour(double T0, const Vec& At, double Scale, Dice& D);
		/** A tracer's streak (two layers, paler along its middle), its tail at Start when born, flying at Vel along Way;
		 * Drag and Drop (gravity) slow and bend it, for the air guns' long flights. */
		void Tracer(double Born, const Vec& Start, const Vec& Way, const Vec& Vel, double For, double Drag, double Drop);
		const AirRow& AirRowOf(const std::string& Kind);
		void AirRound(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, double Mps, bool bTracer, bool bSmoke, Dice& D);
		void Run(double Now); // the steady sources: fire, smouldering
		void Prune(double Now);

		Vec Where(const Bit& B, double Age) const;
		Vec Going(const Bit& B, double Age) const;
		void Lay(int Slot, const Vec& Middle, const Vec& AxisA, const Vec& AxisB, const Rgb& Col, double Alpha, double HeartAlpha, double RimAlpha, double Inner, double Point, double Tail, double Shade, const View& Eye, double Rough = 0.0, uint32_t LumpSeed = 0, double Turn = 0.0);
		bool LayBit(int Slot, const Bit& B, double Now, const View& Eye);
	};
}
