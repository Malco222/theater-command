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
			Streak, // drawn out along its own flight: clods, sparks, tracer; never thinner than LeastPx on the screen
			Plate // a torn piece of an aircraft: a flat piece lying along Along and Face, tumbling about Along, lit and glinting
		};
		Shape Form = Shape::Puff;
		Vec From, Vel, Along;
		double Born = 0.0, Life = 1.0;
		double Drag = 0.0, Gravity = 0.0, Rise = 0.0, Windage = 0.0;
		double R0 = 0.1, R1 = 0.2, Grow = 0.5; // radius when born, at the end of its life, and how soon it gets there
		double Stretch = 1.0, Flatten = 1.0, Point = 0.0, Tail = 0.0, Spin = 0.0;
		double HalfLength = 0.0; // (a tracer's streak: half its length in metres)
		Vec Origin; // (a tracer: where it was fired from, or a ricochet's strike; nothing of it is drawn behind this)
		bool bFired = false; // (made by Tracer(): only then is Origin set, and only then is the streak cut at it; a hit's spark is never cut)
		double Until = 0.0; // (a close shot's whole path: how far along from Origin it strikes; nothing is drawn past it. 0: no end)
		double Alpha = 1.0, FadeIn = 0.02, Hold = 0.2, Cool = 1.0;
		double HazeBy = 0.0, HazeAlpha = 0.0; // (a skirt: settles to HazeAlpha by HazeBy seconds)
		double Heart = 0.5, Rim = 0.0; // how thick it is at its heart ring, and at its edge, as shares of its middle
		double HeartEnd = -1.0; // (when 0 or more: the heart thins or thickens to this by the end of its life, so old smoke loses its dark core)
		double Inner = 0.5; // (where the heart ring sits, as a share of the radius: further out, a denser body with a thinner soft edge)
		double FeatherPx = 0.0; // (over 0: its edge fades over at least this many pixels on the screen however far the eye is, its heart ring brought in to suit)
		// (lumps on a lump, an oil column's puff: at least LumpsPx pixels across on the screen, it carries Lumps smaller
		// lumps of its own on its face, their sizes dealt between LumpsR0 and LumpsR1 of its radius, one step of that
		// range to each by its own seed, so no two puffs carry the same pattern; each is set LumpsOut0 to LumpsOut1 of the way
		// from its middle to where the lump would touch its edge, turning with it and as thick as LumpsAlpha of it;
		// each takes LumpsLight of its light from its own sunward side and keeps LumpsRim of it at its rim, so its edge
		// is dark on its shadow side only; LumpsInner over 0 is where its own heart ring sits, so its edge is firmer
		// than the puff's. A big puff near the eye is then a billowing mass and never one soft disc. 0: none)
		int Lumps = 0;
		double LumpsPx = 0.0, LumpsAlpha = 1.0, LumpsLight = 0.6, LumpsRim = 0.8, LumpsInner = 0.0;
		double LumpsR0 = 0.3, LumpsR1 = 0.45, LumpsOut0 = 0.3, LumpsOut1 = 1.0;
		// (a tongue of flame's root, other than 0: its fan is drawn from a point this share of its half length back from
		// its middle along Along (ahead of it when under 0), so its own colour sits there, at its root, and runs out
		// evenly to its edge's colour along it and across it. Held to 0.9 either way)
		double Hub = 0.0;
		double Smear = 0.0; // (an aircraft trail's puff: drawn this many metres longer behind it, against Along, so puffs metres apart join in one line)
		double SmearFor = 0.0; // (and that length shrinks to under a third over this many seconds, so an old trail breaks into lumps; 0 keeps it)
		// (a ribbon trail's piece, Ribbon over 0: a length of ribbon from its own point back to the piece laid before it,
		// wherever that one has drifted to and as wide and as dark there as that one is now. Its far edge is that
		// piece's own near edge, so the two share it exactly, as the links of one strip: no thin tips, no step, no
		// overlap to darken and no gap, however fast the aircraft flew. Back: the piece behind, as it was laid (Way:
		// the way the aircraft flew there, which the strip runs along at that edge, so it bends through its pieces as
		// the path did and never turns a corner at a join; Alpha: its own opacity; LumpsFrom: its own SmearHold). Ribbon: the share of its half
		// width that is its full dense core; from there it thins to nothing at its two sides. ShowFrom, ShowBy:
		// seconds between which it comes into sight (a length that hangs on, as the short ones it stands in for go).
		// SmearHold: the share of its life it stays a link; after that it draws in to a round lump of its own)
		struct Reach
		{
			Vec From, Vel, Way;
			double Rise = 0.0, Born = 0.0, R0 = 0.0, RMid = 0.0, R1 = 0.0, Grow = 1.0, Life = 1.0, Alpha = 1.0, LumpsFrom = -1.0;
			double GrowBy = 0.0, GrowEarly = 1.0; // (that piece's own widening clock: a trail may change kind at this join)
			bool bOn = false;
		};
		double Ribbon = 0.0, ShowFrom = 0.0, ShowBy = 0.0, SmearHold = -1.0;
		Reach Back;
		// (a ribbon's piece widens in two stages, GrowBy over 0: from R0 to RMid over its first GrowBy seconds, by the
		// power GrowEarly (under 1: fast at first, as smoke spreads in an aircraft's wake), then on to R1 by the end
		// of its life, by Grow. And it pales as it widens, PaleBy over 0: as dark as it was laid until PaleFrom
		// seconds, then paler, PaleTo as dark from PaleBy seconds on. The short pieces and the lengths that hang on
		// under them pale by the one clock, so the young trail stays as black as it was. Art director 15:22Z)
		double GrowBy = 0.0, RMid = 0.0, GrowEarly = 1.0;
		double PaleTo = 0.0, PaleFrom = 0.0, PaleBy = 0.0;
		double EdgeSoft = 0.0; // (over 0: its lumpy outline is gentler the wider the piece is for its length, so a wide old length has soft swells, never a comb of spikes: all of it up to a width of its length over EdgeSoft)
		double LeastPx = 0.0; // (a streak: never thinner than this on the screen; a puff: never smaller across its long way, and thinner to match)
		double LeastKeep = 0.0; // (a puff held up to LeastPx keeps at least this share of its opacity: a light's twinkle, a far trail's dark line)
		double MostPx = 0.0; // (a tracer's streak: never wider than this on the screen, so one passing close stays a thin line; 0: no limit)
		double ShadeK = 1.0; // (how strongly it is lit on its sunward side and shadowed on the other: a skirt's lit top and dark foot)
		// (over 0, and not a light: its side away from the sun is ShadowCol, this share of the way at its rim and three
		// fifths of that at its heart ring, so white steam has a cool shadow side of its own, darker than ShadeK's)
		Rgb ShadowCol;
		double ShadowK = 0.0;
		double Rough = 0.0, Turn = 0.0, TurnRate = 0.0; // (a clod: how lumpy its outline is, and how it tumbles)
		uint32_t LumpSeed = 0;
		// (smoke: the middle of its cloud, so a puff on the cloud's sunward side is lit as a whole and one on the far
		// side is in its shadow; the middle drifts with the wind and climbs at CentreRise)
		Vec Centre;
		double CloudShade = 0.0, CloudR = 1.0, CentreRise = 0.0;
		// (smoke seen with the sun behind the eye shows its sunlit face: FaceLit is how far it then goes toward LitCol, most
		// on its cloud's sunward side and least on the far side; none seen side-on or against the sun, nor at night)
		double FaceLit = 0.0;
		Rgb LitCol;
		double TwinPx = 0.0; // (at least this many pixels across, drawn as two fans, the second turned half a step: twice the sides, so no facet shows; 0 never)
		Rgb Col, Col2;
		bool bLight = false; // flame, flash, sparks, tracer: keeps its own colour by night and takes no shading
		mutable bool bShown = false; // (laid at least once: a light shorter than a frame is kept until it has been)
		bool bWhole = false; // (a light: however late its first frame comes, that frame shows it whole, as one shorter than a frame is)
		uint8_t Glow = 0; // (a tracer's streak: 1 its body, 2 its pale core; by day both widen and the core whitens, to hold on a pale sky)
		bool bHug = false; // rides the ground as it moves (skirts)
		bool bKeep = false; // (an oil fire's column: never left out when the screen is full, the near ones going first)
		double SunSide = 0.0; // (an oil column: its sunward side lifted toward LitCol, its far side kept dark, wherever the eye is)
		double AxisWindage = 0.0; // (and the column's own axis: from Centre, carried by the wind at this share, so a puff knows which side of it it is)
		int OilId = -1; // (a puff of an oil fire's column: the fire's id, so the column's width at its height is found each frame)
		double StretchBy = 0.0; // (over 0: Stretch is reached only by this age, from round at birth: a pall drawn out as it runs)
		double SunYoung = 1.0, SunFrom = 0.0, SunBy = 0.0; // (SunBy over 0: its sunward side is lit only SunYoung as much until SunFrom seconds old and in full by SunBy, so smoke is darkest where it leaves the fire)
		// (a thrown clod: true flight under gravity, landing at Land seconds and once more at Land2 after one bounce)
		bool bThrown = false;
		double Land = 1e9, Land2 = 1e9;
		Vec Vel2, At1, At2;
		// (a falling aircraft's piece: its other flat way at birth, square to Along; how fast it tumbles about Along,
		// radians a second; how strongly it catches the sun)
		Vec Face;
		double Tumble = 0.0, Glint = 0.0;
		// (a light's clump of fire: its edge goes EdgeK of the way to Edge, so a hot middle darkens out to a smoky rim
		// and the clump reads as a lump of fire, never a flat cut-out; 0 keeps one colour across it)
		Rgb Edge;
		double EdgeK = 0.0;
		double EdgeIn = 0.25; // (and how much of that its heart ring takes: more, and the colour turns sooner from its middle)
		double DayAlpha = 1.0; // (by day its opacity is this share of what it is at dusk: a flash's glow, thin in daylight)
		double Over = 1.0; // (a light: how far past white it burns, up to picture.lit_gain; see Battle::PictureGain)
		double Nearer = 0.0; // (sorted as if this many metres nearer the eye: a fire's white heart drawn over its own clumps)
		// (dust that may hide a unit: from a high steep eye it thins near the units, so a player always finds his own.
		// 1: a vehicle's trail, near any vehicle laying dust; 2: a gun's blast veil, near Origin, the gun)
		uint8_t Thin = 0;
		// (a lump of dust or smoke that rests on the ground, over 0: its middle is never lower than this share of its own
		// radius above the ground under it, wherever it drifts, so the ground never cuts it along a straight line)
		double Rest = 0.0;
		// (dust born low, LowBy over 0: until LowFrom seconds old it lies along the ground as a mound LowFlat times as high
		// as it is wide, as wide again as keeps its bulk, with LowLit of its light and shade (0: its own plain tone, no
		// lit top and no shadow side), its heart ring LowSoft of the way out it will sit (under 1: a young skirt with a
		// soft edge); by LowBy seconds it has risen round, firmed, and takes its light and shade in full. The shape: a
		// plain round puff only)
		double LowFlat = 1.0, LowFrom = 0.0, LowBy = 0.0, LowSoft = 1.0, LowLit = 0.0;
		double FootClear = 0.0, FootK = 0.0; // (a resting puff takes FootCol, FootK of it, while it lies on the surface, and its own colour by FootClear of its radius clear of where it rests; 0 never)
		Rgb FootCol;
	};

	class Battle
	{
	public:
		/** The ground's height (metres) at X, Y (metres). */
		using GroundFn = double (*)(void* Context, double X, double Y);

		void Setup(const Recipe* InRecipe, GroundFn InGround, void* InContext, uint32_t Seed);
		void SetWind(const Vec& MetresPerSecond) { Wind = MetresPerSecond; }
		/** The ground at X, Y (metres) as the game's SCREEN shows it there, each channel 0 to 255; false where the game
		 * cannot say. */
		using ColourFn = bool (*)(void* Context, double X, double Y, double& R, double& G, double& B);
		/** The ground's colour on the screen, where the game can say: on dry ground a gun's dust takes it (muzzle.dry) in
		 * place of the recipe's paint, written to land on it (picture.to_land), so the dust is the colour of the ground
		 * it rose from. Asked once a shot, never a frame. */
		void SetGroundColour(ColourFn Fn, void* Context)
		{
			ColourAt = Fn;
			ColourContext = Context;
		}
		/** How far off a puff is still drawn (game.draw_beyond_m, 1500, by default): an air scene, whose strategy camera
		 * stands 800 to 2,000 m off the fight, draws farther (air.draw_beyond_m). Kept oil columns keep their own. */
		void SetDrawBeyond(double Metres) { Beyond = Metres > 1.0 ? Metres : 1.0; }
		double AirBeyond() const { return Num("air.draw_beyond_m", Beyond); }
		void Clear();

		/** The size row (sizes.<kind>) a calibre from the field battle draws (from_shell_cal): "howitzer" -> "how105". */
		std::string SizeOfCalibre(const char* Calibre) const;

		/** A shell, mortar bomb, grenade or bomb landing at At at Now. Size is a sizes row ("how105"), Ground a grounds
		 * row ("earth", "sand", "snow", "mud", "town", "woods", "water"). On armour: sparks and a grey puff, no earth. */
		void Shell(double Now, const Vec& At, const char* Size, const char* Ground, bool bOnArmour = false);
		/** An armour-piercing round that misses: a low spurt of earth and a skip, no crown and no crater. */
		void Skip(double Now, const Vec& At, const Vec& Heading, const char* Ground);
		/** A gun firing, its muzzle at At, pointing along Facing. Gun is a muzzle row ("tank_gun", "mg", "howitzer").
		 * Ground (a grounds row) is what its blast throws up: on sand a tank or field gun's shot raises a veil of dust
		 * in front of it that hides the gun and its line of sight for a second or two, then lingers. */
		void Muzzle(double Now, const Vec& At, const Vec& Facing, const char* Gun, const char* Ground = "earth");
		/** Dust thrown up behind a vehicle on dry ground: every frame, for each of its dust sockets (dust.left,
		 * dust.right), where the socket is and the vehicle's velocity (metres a second). Id: any number that stays the
		 * same for that socket. Ground is a grounds row; snow, mud and water throw none (dust.vehicles.by_ground).
		 * Low and long, drifting on the wind (SetWind), lingering; taller and longer at speed; tracks throw more than
		 * wheels. Standing still, nothing; a socket not called for a while is forgotten. */
		void VehicleDust(double Now, int Id, const Vec& At, const Vec& Velocity, const char* Ground, bool bTracked = true);
		/** Rounds fired by one gun (Gun: any number that is the same for the same gun) from From toward To, PerSecond:
		 * every fifth is a tracer (tracer.one_in), out of step between guns; machine guns flicker at the muzzle. */
		void Shots(double Now, int Gun, const Vec& From, const Vec& To, const char* Weapon, int Rounds, double PerSecond, const char* Ground = "earth");
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
		/** Rounds striking an aircraft at At: Normal the skin's outward way there, Carry that aircraft's velocity (m/s),
		 * Kind the air_guns row of the gun that hit. A bright spit with sparks, a dark scrap or two torn off and left
		 * behind in the air, a puff of smoke; all bigger for cannon (the row's strike_m). */
		void AirHit(double Now, const Vec& At, const Vec& Normal, const Vec& Carry, const char* Kind);
		/** A trail from a moving point on an aircraft (an engine): TrailStart once, TrailMove every frame with where the
		 * point is and its velocity, TrailStop when it ends; what is already in the air fades on its own. Id: any
		 * number that stays the same for that trail. Kind is an air_trails row: "smoke" (thin and grey, thickening and
		 * darkening), "fire" (flame at the root, black smoke), "glycol" (a thin white vapour). TrailStart on a running
		 * trail changes its kind (smoke catching fire). */
		void TrailStart(double Now, int Id, const Vec& At, const Vec& Carry, const char* Kind, double SpanM = 0.0);
		void TrailMove(double Now, int Id, const Vec& At, const Vec& Carry);
		void TrailStop(int Id);
		/** A falling aircraft's debris row (art director 04:40Z). Kind: "explode" sheds every piece at once with a short
		 * torn fireball; "burn" sheds 5 to 10 over its fall; "spin" a few, sometimes a wing. Mostly light torn skin plates
		 * that flutter and glint, with a few heavy dark pieces that drop straight. AirFall once as the fall starts, with
		 * the aircraft's span (20 m or more sheds a bomber's row) and DeckZ, the height where pieces end: the cloud
		 * deck's top on a day with one, else the ground's; they fade into it, never popping out. Then AirFallMove every
		 * frame with where the model is, and AirFallStop when it is gone. Id: any number that stays the same for that fall. */
		void AirFall(double Now, int Id, const Vec& At, const Vec& Velocity, const char* Kind, double SpanM, double DeckZ);
		void AirFallMove(double Now, int Id, const Vec& At, const Vec& Velocity);
		void AirFallStop(int Id);
		/** A flak shell bursting at At, at the bombers' height: a soft flash of about 0.2 s off-centre in five to eight
		 * see-through lobes of smoke in a crooked chain, longer along the wind, with thin wisps trailing downwind; it
		 * hangs where it burst, drifts with the wind, greys and stretches, and is gone by about 9 s. Kind is an
		 * air_flak row ("heavy"). */
		void AirFlak(double Now, const Vec& At, const char* Kind = "heavy");
		/** Something burning at At, about Across metres wide, flames for Seconds, then a thinning grey column. Id, 0 or
		 * more: the caller's own number for this fire, so BurnOut finds it (under 0: none). A fire lit later under the
		 * same number takes the number; the earlier one burns on as it was. */
		void Burn(double Now, const Vec& At, double Across, double Seconds, int Id = -1);
		/** That fire's crew puts it out (the recipe's fire.put_out; no such row, or no fire burning under that number:
		 * nothing). Over Over seconds (0 or less: the row's own) its flames are eaten back from the edges to its seat,
		 * lower and sputtering, never thinner. How "water" (a ship's hoses): a surge of white steam at the seat for the
		 * first seconds; "extinguisher" (a tank's crew): a brief pale puff. Its smoke turns from black through grey to
		 * grey-white, thins to a wisp and is gone in about a minute; red embers glow where it burned for some seconds
		 * after the last flame. A big fire's own column burns on only while another of its fires does. */
		void BurnOut(double Now, int Id, double Over = 0.0, const char* How = "water");
		/** Steam let go by a ship's boiler or engine-room hit (or a vehicle's radiator), from a vent Across metres wide
		 * at At, blowing along Way (up, if that is no direction) for Seconds: a narrow white jet, clear for a moment at
		 * its mouth, that breaks into rags which evaporate within seconds; never a bank, never a flash. It roars at
		 * first, then its length and speed sink together as the pressure falls, to a wavering wisp. Kind is a steam
		 * row: "vent" (a deck vent, a skylight or a hole in her side: metres tall) or "funnel" (the escape pipes on the
		 * funnel's face as the safety valves lift: tens of metres, straight up). Id: any number that stays the same for
		 * that vent; a second call under it starts it afresh. No steam row in the recipe: nothing. */
		void Steam(double Now, int Id, const Vec& At, const Vec& Way, double Across, double Seconds, const char* Kind = "vent");
		/** That vent is shut, or blown dry: no new steam; what is in the air is gone in seconds. */
		void SteamStop(double Now, int Id);
		/** Ammunition cooking off inside a burning tank (or a ship's gun mount, or aircraft parked on a deck), seen at
		 * its hatch, At, Across metres wide, opening along Up: Pops sharp pops (held to 0..40) at ragged, quickening
		 * moments over Over seconds, each a small flash by the flash rule, a grey-black puff, orange embers in short
		 * arcs and now and then a dark fragment tumbling off; never a round fired skyward. Jet over 0: the propellant
		 * burning as a roaring ragged flame from the hatch for that long, in one or two pulses, black smoke rolling off
		 * its top; it starts JetFrom seconds after the call (under 0: after the last pop). The burning wreck after is
		 * the caller's own Burn. No cook_off row in the recipe: nothing. */
		void CookOff(double Now, const Vec& At, const Vec& Up, double Across, int Pops, double Over, double Jet, double JetFrom = -1.0);
		/** An armour-piercing shot glancing off armour at At: Incoming the way it flew, Normal the plate's outward way
		 * there (either may be rough: no direction counts as level and up). A small two-frame flash, sparks thrown in a
		 * narrow cone the way it glances, cooling as they fall, a wisp of paint and metal dust, and the shot itself
		 * going off glowing and tumbling at a shallow angle; where it comes down in sight it kicks up a little of the
		 * Ground. No burst, no smoke. CalibreMm sizes it (held to 20..460). No glance row in the recipe: nothing. */
		void Glance(double Now, const Vec& At, const Vec& Incoming, const Vec& Normal, double CalibreMm = 75.0, const char* Ground = "earth");
		/** Oil burning on the sea where a ship went down (Kind "sunk"), or a ship on fire where she left the line
		 * ("burning"): low flames across the slick and a tall black column leaning down the wind, kept even when the
		 * screen is full, so a far column on the horizon never blinks out. Reach is the oil's reach in metres (the sea
		 * battle's zone r); Until, when it stops (the fight's end; 0: until OilStop); Since, how long it has already
		 * burned, so a scene that opens later shows the column grown. Id: any number that stays the same for that
		 * fire. Kinds live in the recipe's oil_fires. */
		void OilFire(double Now, int Id, const Vec& At, double Reach, const char* Kind = "sunk", double Until = 0.0, double Since = 0.0);
		/** That oil fire goes out: no new flame or smoke; what is in the air thins away on its own. */
		void OilStop(double Now, int Id);

		/** What the eye is: where it is, which way its screen's right, up and forward point, and how many pixels a
		 * radian across the screen is (the screen's width / (2 tan(field of view / 2))). Smoke takes SmokeLight (the
		 * hour: white by day); Sun is the way toward the sun. */
		struct View
		{
			Vec Eye, Right, Up, Forward;
			double PixelsPerRadian = 1000.0;
			Rgb SmokeLight = Rgb(1.0f, 1.0f, 1.0f);
			Vec Sun = Vec(-0.45, -0.35, 0.82);
			Rgb SunLight = Rgb(1.0f, 1.0f, 1.0f); // (the sun's own light, its colour and strength: none at night)
			// (how wide the view is: the tangent of half its angle across, and up (0: a quarter more than across). Given,
			// what is wholly off the screen is neither laid nor takes a place, so smoke behind the eye never crowds out
			// what is in front of it. 0 across: not known, and everything in range is laid, as before)
			double HalfAcross = 0.0, HalfUp = 0.0;
		};
		/** Lays everything alive at Now for this eye into Points and Colours (PerPuff points a puff, far to near).
		 * Returns how many puffs; the rest of the room up to the last frame's count is folded away. */
		int Draw(double Now, const View& Eye);
		/** Each puff is a middle and two rings of Ring() points (game.puff_sides, 16 unless the recipe says less), so
		 * PerPuff() points; it is fixed at Setup, so the mesh's triangles never change. */
		int Ring() const { return Sides; }
		/** What the picture must multiply the colours by (the material's brightness): picture.lit_gain, 1 by default. */
		double PictureGain() const { return LitGain; }
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
		/** The strongest lamps lit at Now, at most Room of them. Day (DayOf the hour's light, 0 at night, 1 by day)
		 * turns them down to a share and toward a warm white, and puts out small guns' altogether. A flash's lamp is full
		 * on the first frame drawn after it, half on the next, then out: call it after Draw. */
		int Lamps(double Now, Lamp* Into, int Room, double Day = 0.0) const;
		/** How much the hour's light (View::SmokeLight) is day: 0 at night, 1 in full day (game.day_light_from, _full). */
		double DayOf(const Rgb& HourLight) const;

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
			double SmokeS = 0.8, SmokeAlpha = 0.1, Lamp = 0.0, FlashLeastPx = 0.0; // (FlashLeastPx: the flash never under this on the screen, a twinkle far off)
			int OneIn = 5, SmokeEvery = 3, Licks = 0;
			// (the close view, art director 04:40Z: a heavy machine gun's flicker gains CloseLicks side licks and a lamp of
			// CloseLamp when the eye is within LicksWithin metres of it; every round drops its spent case and belt link
			// from the chute under the gun, CaseM long and CaseD across, a link LinkM, when the eye is within CasesWithin
			// metres. 0 for each: none, as before)
			Span CloseLicks{ 0.0, 0.0 };
			double CloseLamp = 0.0, LicksWithin = 0.0, CasesWithin = 0.0, CaseM = 0.0, CaseD = 0.0, LinkM = 0.0;
			double ChuteBack = 1.0, ChuteDown = 0.25, CaseDrag = 5.0, CaseGlint = 4.0, CaseLeastPx = 0.8;
			Span Throw{ 3.0, 8.0 }, CaseSink{ 18.0, 28.0 }, LinkSink{ 12.0, 20.0 }, CaseLife{ 1.2, 2.0 }, CaseTumble{ 15.0, 40.0 };
			Rgb CaseCol, CaseDark, LinkCol;
			// (art director 06:19Z: a hot flash, lit from within: a white-yellow root at the muzzle, a saturated orange body,
			// a fading tip, never peach. bHotFlash: the cannon flash in these colours; the close view's licks always)
			bool bHotFlash = false;
			Rgb FlashHot, FlashOrange, FlashRim; // (FlashRim: the deeper orange its edges go to, so a rim over the sky stays orange)
		};
		/** A ground gun's spent case (ground_cases.<gun>): its size and how it is thrown and lies. */
		struct CaseRow
		{
			double M = 0.08, D = 0.012, After = 0.0, Back = 0.0, Below = 0.0, Side = 1.0, Scatter = 0.25, Glint = 4.0, LeastPx = 0.8;
			Span Throw{ 2.0, 4.0 }, Up{ 1.0, 2.5 }, Lie{ 2.0, 4.0 }, Tumble{ 15.0, 30.0 };
			Rgb Col, Dark;
		};
		bool CaseRowOf(const std::string& Gun, const Vec& At, CaseRow& Out_) const; // (false: none for this gun, too far off, or no room)
		void GroundCase(double T, const Vec& Breech, const Vec& Away, const CaseRow& C, Dice& D);
		int LayCase(int Slot, const Bit& B, double Age, const Vec& P, double A, const View& Eye, int Room); // (a spent case's shadow, tube and mouth: the places it took)
		/** A ground gun's flash close to the eye (muzzle.hot, art director 07:08Z): lit from within, a yellow-white root
		 *  and an orange body, as the air guns' close flash. */
		struct HotFlash
		{
			Rgb Root, Root2, Hot, Orange, Rim;
			// (art director 13:54Z, build 1720: the flame graded from its root out and ragged, new every round. Only when
			// the recipe has muzzle.hot.tongues: see HotFlame)
			bool bRagged = false;
			Rgb Yellow, Red;
			Span Tongues, Licks, Long;
			double Wide = 0.2, Glow = 0.0, GlowDay = 0.5, GlowM = 0.035, Over = 1.0;
			// (art director 16:30Z, build 1732: the root alone burns near white, so the yellow has its own number
			// (yellow_over; 0: a share of the root's, as before), and the root's glow is the kit's own soft halo, HaloM
			// past the root all round and Halo opaque at the root's edge, never the game's bloom)
			double YellowOver = 0.0, Halo = 0.0, HaloM = 0.0;
		};
		/** A close gun flash as the art director ruled it from the game's own frames: a small white-yellow root right at
		 *  the muzzle, melting into a yellow then orange body widest a third of the way out, which breaks into ragged
		 *  tongues, red-orange only at their ends, with short licks thrown sideways and a thin glow round it. Its length
		 *  and every piece are rolled anew each call, so no two flashes match. Len: its usual length in metres. */
		void HotFlame(double Born, const Vec& At, const Vec& Fw, double Len, double Life, const HotFlash& H, Dice& D);
		/** A burst's close flames to come: each is laid when its round fires (Run), so a long burst never holds hundreds
		 *  of pieces waiting and the room is asked only for the one flame burning. */
		struct FlameDue
		{
			double Born = 0.0;
			Vec At, Fw;
			double Len = 0.5, Life = 0.025;
			HotFlash H;
			uint64_t Seed = 1;
		};
		std::vector<FlameDue> FlamesDue;
		bool HotOf(const Vec& At, HotFlash& Out_) const; // (false: no muzzle.hot in the recipe, or the gun past within_m of the eye)
		/** A dust.vehicles row for one ground, read once a ground. */
		struct DustRow
		{
			std::string Ground;
			double Share = 0.0, LeastMps = 1.0, FullMps = 12.0, Spacing = 2.5, MostPerS = 4.0, Kick = 0.1, Drag = 1.0;
			double Windage = 1.0, FlatSlow = 0.5, FlatFast = 0.75, HazeBy = 2.0, HazeAlpha = 0.2, Tracked = 1.3, Spread = 0.6;
			double LowShare = 0.5, LowFlatten = 0.35, LowWide = 1.25, LowThick = 1.2; // (the low dense skirts among the billows)
			Span Life{ 4.0, 6.0 }, LifeFast{ 7.0, 10.0 }, R0{ 0.5, 0.9 }, R1{ 1.5, 2.5 }, R1Fast{ 3.5, 5.0 }, RiseM{ 1.0, 3.0 }, Alpha{ 0.35, 0.5 };
			Rgb Low, High; // (its colour by the ground, where it is thrown up; and higher in the air, paler)
		};
		/** An air_trails row, read once a kind. */
		struct TrailRow
		{
			std::string Name;
			double Life = 6.0, Spacing = 8.0, R0 = 0.5, R1 = 3.5, Alpha = 0.5, DarkenS = 4.0, RampS = 1.5, Smear = 1.5, Carried = 0.05, Flame = 0.0;
			double Grow = 0.5, Lumpy = 0.0, SmearS = 0.0, Rise = 0.0;
			double ThinAfter = 0.0, ThinGrow = 1.0, ThinAlpha = 1.0, ThinHold = -1.0, ThinRagged = 0.0; int ThinKeep = 1; // (an old trail thinned: past ThinAfter s only one puff in ThinKeep hangs on to Life, ThinGrow times wider, ThinAlpha times denser, full until ThinHold of its life)
			double LeastPx = 0.0, LeastKeep = 0.0; // (a far trail held at least LeastPx across, keeping LeastKeep of its opacity)
			// (a ribbon, Core over 0: every piece a length of one strip back to the piece before it, sharing its edge,
			// dense over Core of its half width and thinning to its sides; 0: the trail as it was, each puff a lens. The
			// lengths that hang on, one in ThinKeep, reach back to the last of their own and come into sight between
			// JoinFrom and JoinBy of thin_after_s as the short ones go; one is laid wherever the path has turned TurnCos
			// since the last, so a spin's trail follows its curve. LumpsFrom: the share of its life after which a length
			// draws in to a lump. Drift: the share of the lumpy scatter each piece is thrown with)
			double Core = 0.0, JoinFrom = 0.15, JoinBy = 0.45, TurnCos = 0.9, LumpsFrom = 0.75, Drift = 0.35;
			// (PaleTo, PaleFrom, PaleBy: a ribbon pales from PaleFrom seconds old to PaleTo as dark by PaleBy seconds, and
			// stays so; 0: as it was. Wander: how far its width wanders along its length, 1 about a sixth either way.
			// WidenBy: seconds by which it has widened from r0_m to r1_m, by the power grow; 0: by the short pieces' life)
			double PaleTo = 0.0, PaleFrom = 0.0, PaleBy = 2.5, Wander = 0.0, WidenBy = 0.0;
			double SmokeStart = 1.0; // (fire: where along the flame its smoke starts, a share of the flame's length)
			Span FlameLiftOfLen{ 0.0, 0.0 }; // (fire: how far each tongue stands off the skin as a share of its length; 0: flame_lift_m)
			double LicksPerFrame = 0.4; // (fire: tongues torn free of the tip and streaming away aft, a frame)
			double FlameAlphaRoot = 0.92, FlameAlphaFall = 0.2, FlameHold = 0.45, FlameWide = 1.0, LickAlpha = 0.9; // (fire: a tongue's opacity at the cowl and how much it loses to its tip; how long it holds full, a share of its life; how wide; a torn-free lick's opacity)
			double FlameLiftCurve = 0.0; // (fire: a tongue's stand-off grows along it as this power of the way to its tip, so it is rooted on the skin; 0: off the skin from near its root)
			bool bFlameTip = false; Rgb FlameTip; // (fire: the colour a tongue's tip goes to, into its smoke)
			double TipSmoke = 0.0, TipSmokeR = 0.3; // (fire: black smoke laid at each tongue's tip at this opacity, widening to TipSmokeR of the flame's length; 0: none)
			Span TipSmokeLife{ 0.3, 0.6 };
			double FlameOfSpan = 0.0, FlameMost = 0.0, FlameLeastPx = 0.0; // (a fire's flame: this share of the aircraft's span, at most FlameMost m; never under FlameLeastPx on the screen)
			Span FlameLift; // (metres a fire's tongue stands off the skin)
			Rgb Col, ColFull, ColOld;
		};
		/** A trail being laid: where its point was at the last call, and how far it has gone since its last puff. */
		struct Trailing
		{
			int Id = 0;
			bool bOn = false;
			std::string Kind;
			double Born = 0.0, KindBorn = 0.0, Last = 0.0, Owed = 0.0, LastPuff = 0.0, NextFlame = 0.0;
			double Tip = 0.0; // (fire: how far aft of the point its smoke starts, at the flame's tips)
			double Span = 0.0; // (the aircraft's span, metres, as TrailStart was told: a fire's flame is sized by it)
			unsigned Puffs = 0, NextKept = 0, KeptGap = 0; // (puffs laid, counted only when the row thins its old puffs; the next to hang on, and how many back the last one kept was)
			Vec At, Carry, LastPuffAt;
			Vec KeptAt; bool bKeptAt = false; // (where the last puff kept to hang was laid)
			Vec PuffAt, KeptWay; bool bPuffAt = false; // (a ribbon: where the last puff was laid, and the way the path ran at the last one kept)
			Bit::Reach Prev, Kept; // (a ribbon: the last piece laid and the last length that hangs on, for the next of each to reach back to)
			Vec Wander; double RiseK = 1.0; bool bWander = false; // (a ribbon: the drift and rise its last piece took, which the next takes most of, so the strip meanders and never zig-zags)
			double Wide = 0.0; // (and how much wider or narrower than the row its last piece was)
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
			bool bNightOnly = false; // (a small gun's flash: no lamp on the ground by day)
			int FirstFrame = -1; // (the first frame drawn after it was born: its lamp is full then, half the next, then out)
		};
		struct Knock
		{
			Vec At;
			double Born = 0.0, Strength = 0.0, Within = 0.0, Push = 0.0;
		};
		/** An oil_fires row, read when a fire starts. */
		struct OilRow
		{
			Span Heads, HeadR, FlameR, FlameTall, FlameLife, ColumnLife, Rise, Burst, Windage, R0, R1, Alpha;
			Span BaseLife, BaseRise, BaseR, BaseAlpha;
			double BaseEvery = 0.0; // (the low black smoke rolling off the whole slick before it joins the column; 0: none)
			double BaseFlat = 1.0; // (under 1: that roll hugs the water, this high for its width)
			Span PallLife, PallR1, PallWindage, PallAlpha;
			double PallEvery = 0.0, PallStretch = 1.0, PallStretchBy = 0.0, PallHold = 0.15, PallDrift = 0.0, PallRough = 0.0, PallRest = 0.0; Span PallLevel{ 1.0, 1.0 }; // (the long flat pall run out down the wind from the column's top; 0: none)
			double SlickLong = 0.0, SlickWide = 0.0; // (over 0: the burning slick an oval, these shares of Reach long and wide, its heads all over it)
			double FlameOdds = 1.0, FlamePoint = 0.9; // (flame_point: how sharp each tongue's tip, 1 a spike; each head's chance of a tongue each flame_every_s: a broken line of low flames)
			double SunSide = 0.0; // (its sunward side lit toward Lit, so the column reads round)
			double SunYoung = 1.0, SunFrom = 0.0, SunBy = 0.0; // (SunBy over 0: a puff has only SunYoung of that light until SunFrom seconds old and all of it by SunBy, so the column is black at its root)
			double BurstDrag = 0.1, HeadSpread = 0.6, FlameUp = 0.0, FlameEvery = 0.1, ColumnEvery = 1.0, Slow = 0.06, Hold = 0.5, Heart = 0.6, Rough = 0.3;
			double Cool = 1.0, Lamp = 0.25, LampReach = 2.0, TwinPx = 0.0, Grow = 0.6;
			int Lumps = 0; // (lumps, lumps_px, lumps_alpha, lumps_light, lumps_rim, lumps_inner, lumps_r, lumps_out: see Bit::Lumps)
			double LumpsPx = 0.0, LumpsAlpha = 1.0, LumpsLight = 0.6, LumpsRim = 0.8, LumpsInner = 0.0;
			Span LumpsR{ 0.3, 0.45 }, LumpsOut{ 0.3, 1.0 };
			bool bKeep = true;
			Rgb Col, Col2, Flame, Lit;
			// (a small fire's column held in one piece, fire.smoke_column only: tight_m, overlap, least_px, least_keep.
			// See the column's Lay in Run. 0: as before)
			double TightM = 0.0, Overlap = 0.0, ColumnLeastPx = 0.0, ColumnLeastKeep = 0.0;
		};
		/** A big fire's smoke as a bank of lumps run out down the wind (fire.smoke_bank), read when the fire starts. */
		struct BankRow
		{
			bool bOn = false; // (the recipe asks for it and this fire is from_m across or more)
			double Wide = 1.6, Long = 2.5; // (the bank's width as a share of the fire's, and this fire's own length as a share of that width)
			double Life = 40.0, Short = 0.3, Thin = 2.0; // (this fire's lumps' longest life, the shortest as a share of it, and how few live long)
			double EveryA = 0.35, EveryB = 0.8; // (seconds between lumps while it flames, and after)
			double Heads = 0.33, Rough = 0.35, Heart = 0.85, Hold = 0.5, SunSide = 0.0, TwinPx = 0.0, Rest = 1.0, Spread = 0.35;
			double SunYoung = 1.0, SunFrom = 0.0, SunBy = 0.0; // (how much of its lit side a lump has when young, and the shares of the longest life until which it has no more and by which it has it all)
			Span LumpR{ 0.3, 0.5 }, Alpha{ 0.75, 0.95 }, Shade{ 0.8, 1.25 }, Rise{ 0.0, 0.4 }, HeadRise{ 0.6, 1.4 };
			Rgb Lit;
		};
		/** A steam row (steam.vent, steam.funnel), read when the vent opens. */
		struct SteamRow
		{
			double Reach = 10.0, Cone = 12.0, Gap = 1.0, Every = 0.06, Roar = 0.3, Least = 0.12, Mouth = 0.5, Rag = 0.2, Rise = 1.5, Windage = 0.5, Alpha = 0.9, Waver = 0.25, Rough = 0.35, Lance = 0.0, LanceLife = 0.3, LanceLong = 3.5;
			Span Life{ 1.6, 2.8 };
			Rgb Col, Old;
			// (a rag's shadow side, Bit::ShadowCol, and how far it goes to it; the plume's head: Cap of its rags lay
			// another, CapWide times as big, in its top from CapFrom of its reach up, living CapLife and going off down the wind)
			Rgb Shadow;
			double ShadowK = 0.0, SideShade = 0.0, Cap = 0.0, CapWide = 2.0, CapFrom = 0.62; // (SideShade: how far a rag thrown to the plume's side away from the sun goes to the shadow colour all over)
			Span CapLife{ 3.0, 6.0 };
		};
		/** The cook_off row, read when it starts. */
		struct CookRow
		{
			double JetTall = 4.5, JetWide = 0.5, JetEvery = 0.04, JetSmokeEvery = 0.12, Dip = 0.35, Two = 0.5, PuffAlpha = 0.6, FlashK = 0.75, Lamp = 0.3, FragmentOdds = 0.35;
			// (the jet's surge in width and height, as a share; how far it leans for each metre a second of wind, and at
			// the most; its sparks a step; its lamp on the hull and ground, and the share of that it keeps by day; the
			// tongues torn off its sides a step; how uneven a pop's embers are)
			double JetPulse = 0.0, JetLean = 0.0, JetLeanMost = 0.4, JetSparks = 0.0, JetLamp = 0.0, JetLampDay = 0.6, JetTorn = 0.0, EmberUneven = 1.0;
			Span Embers{ 6.0, 14.0 }, EmberUp{ 5.0, 20.0 }, EmberLife{ 0.8, 2.2 }, Puffs{ 2.0, 3.0 };
			Rgb Ember, EmberOld, Puff, Smoke, Fragment, Root;
		};
		struct Source
		{
			Vec At;
			double Start = 0.0, Flames = 0.0, End = 0.0, Across = 1.0, Next = 0.0, NextSmoke = 0.0, NextBase = 0.0, NextPall = 0.0;
			// (a burning thing put out on cue, BurnOut: OutAt 0 or more is when its crew began and OutOver how long they
			// take to beat its flames back to its seat; its smoke has paled by OutPale seconds after that; OutSurge over
			// 0, the seconds of white steam at its seat as the water hits, laid by the clock NextSteam)
			double OutAt = -1.0, OutOver = 0.0, OutPale = 20.0, OutSurge = 0.0, NextSteam = 0.0;
			int Column = -1; // (a burning thing: the big fire's own column it founded or joined, by that column's Id)
			// (a steam vent, Kind 4: the way it blows and its row. A cook-off, Kind 5: Way is its hatch's way out, Pops
			// the moments of its pops, PopNext the next of them, and its jet burns from JetAt to JetEnd, JetTall metres
			// at its fullest, in two pulses when JetDip is over 0 (the share of its time at which it sinks between them))
			Vec Way;
			SteamRow Vent;
			CookRow Cook;
			std::vector<double> Pops;
			size_t PopNext = 0;
			double JetAt = 0.0, JetEnd = 0.0, JetTall = 0.0, JetDip = 0.0;
			Vec SlickA, SlickB; // (an oil fire's oval slick: its half-axes, long and wide)
			int Kind = 0; // 0 a burning thing, 1 a smouldering crater, 2 burning oil on the sea, 3 a big fire's own column, 4 a steam vent, 5 ammunition cooking off
			double UnderOil = 0.0; // (a big fire's own column: none of it is laid while oil burns within this share of that oil's reach of a fire it stands on)
			bool bStoodDown = false; // (a big fire's own column: burning oil has stood over her once, so this column never stands again; see fire.smoke_column's most)
			int Id = -1; // (an oil fire, a steam vent, or a burning thing that may be put out: the caller's number for it)
			uint64_t Seed = 1;
			OilRow Oil; // (an oil fire: its row)
			BankRow Bank; // (a burning thing: its smoke as a bank of lumps, when the recipe asks and it is big enough)
			std::vector<Vec> Heads; // (an oil fire: where on the slick it burns hardest, fixed when it starts)
		};
		struct Paint
		{
			Rgb Soil, Heavy, Dust, Clod, Smoke, First, Splinter;
			Rgb Ring, Foot, Stone; // (the dust ring's colour, the dark foot's, the shower's lumps': by default the dust, heavy and clod)
			double FootDust = -1.0; // (how much of the ground's dust shows in the dark foot's veil and fill; under 0, the recipe's burst.foot shares)
			Rgb Scar; // (the earth thrown round the crater's mark, where the ground gives it: snow's brown-black star)
			bool bScar = false;
			double ScarRays = 1.0; // (and how much longer those rays are drawn out)
			bool bFirst = false, bSplinter = false;
			double Grain = 1.0; // (the crown's puffs as a share of their size: under 1, broken and granular, as snow)
			double FirstAtFoot = 1.0, FirstAtTip = 1.0; // (how much of a jet is the ground's first colour, foot to tip)
		};

		const Recipe* Book = nullptr;
		GroundFn GroundAt = nullptr;
		ColourFn ColourAt = nullptr;
		void* ColourContext = nullptr;
		void* GroundContext = nullptr;
		Vec Wind = Vec(1.6, 0.4, 0.0);
		uint64_t Seeds = 1;
		int OwnColumns = 0; // (big fires given a column of their own so far: each takes an id above every caller's, for the column's width measure)
		uint64_t Glances = 0; // (ricochets so far: with the place, the moment and the kit's seed, each one's own dice, so no two fly alike)
		std::vector<double> ShotEnds; // (when each ricochet's shot now in flight is done: no more than glance.most_shots are followed at once)
		static constexpr int OwnColumnId = 1 << 28;
		int MaxPuffs = 3000;
		int Sides = 16, PointsPerPuff = 33;
		std::vector<double> RingCos, RingSin, Ring2Cos, Ring2Sin, Ring3Cos, Ring3Sin, Ring5Cos, Ring5Sin; // (made once at Setup, so no puff works out a cosine a side each frame)
		int LastDrawn = 0;
		double ShadeLit = 0.18, ShadeDark = 0.28, ClodLeast = 1.2, Beyond = 1500.0, KeepBeyond = 1500.0;
		struct RoundLight
		{
			bool bOn = false;
			Vec Right, Back, Sun; // (level: the screen's right, toward the eye, toward the sun)
			double Mid = 0.0; // (the column's middle at the puff's height, along Right)
			double R = 1.0, Gain = 0.0, Level = 1.0, Up = 0.0; // (the column's radius there; how strong; the sun's level and upward shares)
			double Lobe = 0.25; // (the share of its light a fan takes from its own sunward side, the rest from the column's)
			double Rim = 0.0; // (over 0, a lump on a lump: its rim keeps this share of its light, however see-through it is there)
		};
		double RoundUp = 0.35;
		RoundLight Round; // (while laying one oil column puff: each point lit as a point on a round column, by where it sits across it)
		Rgb EdgeTo; // (while laying one bit: its rim's colour, EdgeK of the way; see Bit::Edge)
		double EdgeIn = 0.25; // (and the share of it the heart ring takes; see Bit::EdgeIn)
		// (while laying one bit: over 0, the heart ring sits this share of the way out along the bit's long way, and
		// Inner only across it. A day tracer: solid to within a pixel of its edge all round, the last pixel fading out,
		// so a thin line a frame long is one straight line and never a stair of steps. Art director 14:26Z)
		double LayInnerAlong = 0.0;
		// (while laying one bit: a ribbon's piece. LayLong: from its middle to its near edge's middle; AxisB is half its
		// near edge and LayFar half its far edge; LayBendFar, LayBendNear: how the strip's line bows off the straight
		// run to leave each edge along the path there; LayTailAlpha: its far edge's opacity as a share of its near
		// edge's; LayLump, LayLumpFar: how far its near and far ends have drawn in to a round lump, 0 a link of the
		// strip and 1 a round puff)
		Vec LayLong, LayFar, LayBendFar, LayBendNear; bool bLayLong = false; double LayTailAlpha = 1.0, LayLump = 0.0, LayLumpFar = 0.0, LayWob = 1.0;
		// (picture.lit_gain: the game's picture multiplies every colour by this (the wrapper sets the material's
		// brightness to it), and every colour is written that much darker, so all looks as before; but a light's hottest
		// parts (Bit::Over) are written up to that much brighter than white, so they land white-hot after the game's
		// tone curve, which holds plain white down to about 224 of 255. 1: nothing changes)
		double LitGain = 1.0, PutK = 1.0;
		double LaySquare = 0.0; // (while laying one bit: 0 an ellipse; toward 1 its outline squared off, a gun case's cylinder seen side on)
		Vec LayHub; // (while laying one bit: where its fan's middle point sits off its middle; see Bit::Hub)
		Rgb LayShadow; // (while laying one bit: its shadow side's colour, Bit::ShadowCol in the hour's light, and how far it goes to it)
		double LayShadowK = 0.0;
		double LumpsRoom = 0.0; // (set by Draw: 1 while every lump a big puff may carry, Bit::Lumps, has a place with a tenth of the room to spare, less as that goes, 0 with the room full: the lumps give way to everything else)
		double LumpsMost = 0.0, LumpsHeld = 0.0, LumpsShown = 0.0, LumpsAt = -1.0e9, LumpsZoom = 0.0; // (the most places asked of late, lumps and all; how long since the count last came near it; LumpsRoom at the last Draw; when that was; and the view's pixels a radian then: see LumpsRoom in Draw)
		// (the lumps' cost held, read at Setup: game.lumps_most, no more puffs than this carry lumps at once, the biggest
		// on the screen first (0: any number); game.lumps_px_most, a puff this wide on the screen or wider carries none,
		// thinning from .0 to .1 pixels (a column the eye is in or under fills the screen with pieces whose lumps are
		// murk and cost the most of all; .1 of 0: any size); game.lumps_least_ink, a lump thinner than this is not
		// laid (0: every one). LumpsTight, set by Draw: how many times its own lumps_px a puff must be across to carry
		// lumps now, 1 with places for all, more with over lumps_most puffs wide enough; it moves by half its size
		// a second at most, so a puff's lumps come and go as slowly as the count changes. LumpSizes: Draw's own list)
		int LumpsMostPuffs = 0;
		double LumpsBigA = 0.0, LumpsBigB = 0.0, LumpsLeastInk = 0.0, LumpsTight = 1.0;
		// (game.lumps_soft_s: LumpsTight closes most of the way to what is asked in this many seconds (0: at once, held
		// only to half its size a second); game.lumps_band: a puff's lumps come in over this share of the width asked
		// (0.25 without the key). With many puffs alike in size most of the kept ones lie in that band and show faint:
		// a narrower band shows them nearer whole)
		double LumpsSoft = 0.0, LumpsBand = 0.25;
		std::vector<double> LumpSizes;
		// (while laying a spent case's body: each point lit as a point on a metal tube, by where it sits across it. Art
		// director 07:08Z: a bright band along the top, a shade side and underside, never a pale plank)
		struct TubeLight
		{
			bool bOn = false;
			Vec Mid, Across, Back, Sun, Half; // (the tube's middle; across it on the screen; toward the eye; the sun; half way between)
			double R = 1.0, Shine = 8.0;
			Rgb Shade, Lit, Spark; // (in shade; full sun; the glint's own light at its peak)
		};
		TubeLight Tube;
		Rgb CaseMouth, CaseGlint; // (ground_cases: the dark inside of an open case's mouth; the colour of its glint)
		double CaseShadow = 0.5, CaseShine = 8.0;
		std::vector<double> CaseUntil; // (when each long-lying case goes: past ground_cases.most_lying, new ones lie only briefly)
		size_t MostLying = 200;
		double CaseFar = 150.0; // (ground_cases.draw_within_m: past it a case is neither counted nor laid)
		double CaseDetailPx = 3.0; // (ground_cases.detail_px: a case narrower on the screen is laid as its tube alone)
		bool CaseDetailed(double Rad, double Deep, const View& Eye) const { return 2.0 * Rad * std::max(Eye.PixelsPerRadian, 1.0) >= CaseDetailPx * std::max(Deep, 1e-3); }
		// (art director 14:03Z, build 1720: from the game's own camera, 19 m off, a 6 cm case is under a pixel across and
		// its lit and shade sides average out to the ground's own tone. ground_cases.hold_px: a case is never narrower on
		// the screen than this, and held so its glint is brighter (far_glint) and wider (far_shine) and its shade side darker (far_shade),
		// a speck of brass with a dark underside; it keeps its whole opacity while its true width is over full_px and is
		// gone under gone_px. nearer_m: laid this many metres nearer the eye, in the same place on the screen, so the
		// game's flat-faced ground never buries it. 0: none of it, as before)
		double CaseHoldPx = 0.0, CaseFullPx = 0.6, CaseGonePx = 0.2, CaseFarGlint = 1.0, CaseFarShade = 1.0, CaseFarShine = 0.0, CaseNearer = 0.0;
		double EdgeK = 0.0;
		double SkyFill = 0.25, SunThrough = 1.5, SunThroughPow = 3.0;
		// (see-through smoke and dust thins out as it comes up to the eye, game.near_eye_fade_m: nothing of a piece whose
		// near edge is within NearGone metres of the eye, all of it from NearWhole on. 0, 0: no such fade)
		double NearGone = 0.0, NearWhole = 0.0;
		Rgb SunThroughCol = Rgb(1.0f, 0.61f, 0.35f);
		Span ClodRough, ClodTurns; // (read at Setup: every clod asks)
		double ClodShade = 2.5;
		Vec LastEye;
		Vec LastSun; // (the way to the sun in the last frame drawn; none before the first: a steam plume's rags on its far side from the sun are born in its shade)
		bool bEyeKnown = false; // (the eye of the last frame drawn: a hit close to it throws clods past the lens)
		std::vector<Bit> Bits; // alive or still to be born
		std::vector<Bit> Marks; // craters: for the whole battle
		std::vector<Flash> Flashes;
		std::vector<Knock> Knocks;
		std::vector<Source> Sources;
		std::vector<int> GunRounds; // rounds fired by each gun so far
		std::vector<AirRow> AirRows;
		std::vector<Firing> Firings; // the air guns
		std::vector<TrailRow> TrailRows;
		std::vector<DustRow> DustRows;
		struct Dusting
		{
			int Id = 0;
			double Last = -1e9, Owed = 0.0;
			Vec At;
		};
		std::vector<Dusting> Dusts; // (the vehicles' dust sockets being laid)
		std::vector<Trailing> Trails; // the aircraft trails being laid
		/** A falling aircraft shedding its debris row over its fall. */
		struct Falling
		{
			int Id = 0, Left = 0;
			bool bOn = false, bWing = false;
			double Span = 11.0, DeckZ = 0.0, Next = 0.0, Last = 0.0;
			Vec At, Vel;
			uint64_t Seed = 0;
		};
		std::vector<Falling> Falls;
		// (the tracer's look, read at Setup: one streak for every gun, on the ground and in the air)
		double TracerHalf = 8.0, TracerLeast = 3.0, TracerMost = 5.0, TracerDayWide = 1.5, WholePathBelow = 100.0, WholeFor = 0.015;
		// (art director 13:54Z, build 1720: by day a tracer is a solid pale core, tracer.day_core_px across, inside a warm
		// edge (tracer.day_edge), tracer.day_px across in all, at its whole opacity along its whole length, so it stands
		// 60 luma over pale ground and is never a faint hairline. 0: by day it widens by day_width, as before)
		double TracerDayPx = 0.0, TracerDayCorePx = 0.0, TracerDayOver = 1.0; // (day_over: its core that far past white by day; see LitGain)
		Rgb TracerDayEdge;
		double GlintPow = 30.0; // (a falling piece's glint: how tight the sun's mirror spot on it is, air_debris.glint_power)
		double DayFrom = 0.38, DayFull = 0.9, DayLampShare = 0.25;
		Rgb DayLampCol;
		int FrameNo = 0; // (frames drawn: flashes' lamps go by frames, not seconds)
		Rgb TracerCol, TracerPale, TracerDayCore, FlameCol, FlashCoreCol, GunSmokeCol, CannonHeartCol;
		std::vector<Vec> Out;
		std::vector<uint8_t> OutRgba;
		struct Order
		{
			const Bit* B;
			double Deep;
			bool bKeep; // (counted first when the room is short: a kept oil column, or a light)
			double Far = 0.0; // (a puff that may carry lumps, Bit::Lumps: how far it is from the eye; else 0)
		};
		std::vector<Order> Sorted;
		struct ColumnSpan
		{
			int Id = 0, Bin = 0;
			double Lo = 0.0, Hi = 0.0; // (along the screen's right, level)
		};
		std::vector<ColumnSpan> Spans; // (this frame: how far across the screen each oil column reaches, by fire and by ColumnBin metres of height)
		static constexpr double ColumnBin = 20.0;
		bool SpanAt(int Id, double Z, double& Mid, double& Half) const;
		size_t TwinBits = 0; // (twins in sight at the last Draw: each takes a second slot)
		// (the room a gate asks about: what was in sight at the last Draw and whatever was made since. A piece 5 km off,
		// never drawn, holds no place: counted, far debris shut off the cases, gun smoke and showers beside the eye. The
		// pre-check, 06:42Z)
		size_t InSight = 0, HeldAtDraw = 0;
		size_t SeenBits = 0, SeenTwin = 0; // (of InSight and TwinBits, what was on the screen: see RoomSeen)
		size_t CaseOff = 0; // (the places spent cases off the screen hold in the room asked: see RoomForCases)
		// (the eye given may be the frame before's: the campaign map reads the camera before it has moved for the frame.
		// So what the eye moved and turned between the last two frames is allowed for again, twice over, before a bit
		// is called off the screen. A still eye changes nothing. The pre-check, 14:55Z)
		Vec EyeWas, AheadWas, UpWas;
		bool bEyeWas = false;
		double EyeMoved = 0.0, EyeTurned = 0.0;
		// (the vehicles' dust's own count: in sight at the last Draw and laid since; its gate takes the fuller of the room
		// and dust.vehicles.most_of_room of it. The pre-check, 07:10Z)
		size_t DustInSight = 0, DustSince = 0;
		double DustRoom = 0.4, DustFullWithin = 120.0;
		// (the steep high eye's thinning of dust near the units, worked out once a Draw: how much, and where the dust
		// sockets are now; read at Setup from dust.vehicles.from_above)
		double SteepThin = 0.0;
		std::vector<Vec> NearSockets;
		Span ThinAbove{ 15.0, 25.0 }, ThinSteep{ 0.3, 0.5 }, ThinNear{ 6.0, 12.0 };
		double ThinKeep = 0.25;
		size_t RoomAsked() const { return InSight + (Bits.size() > HeldAtDraw ? Bits.size() - HeldAtDraw : 0) + TwinBits; }
		/** The room asked for on the screen alone: what the last frame laid, and what has been made since. The close
		 *  grit (spent cases) goes by this: build 1720's test barrage, off the screen behind the eye, held the room over
		 *  three fifths full, and the machine gun in front of the eye threw no brass at all. The same as RoomAsked when
		 *  the view's width is not known. */
		size_t RoomSeen() const { return SeenBits + (Bits.size() > HeldAtDraw ? Bits.size() - HeldAtDraw : 0) + SeenTwin; }
		/** What a spent case is thrown by: the screen's room, and the brass lying off it. A barrage off the screen does
		 *  not stop the brass; brass off the screen does, or guns firing beside the eye fill the room asked with cases
		 *  nobody sees (three places each) and every shell throws half its shower for them. */
		size_t RoomForCases() const { return RoomSeen() + CaseOff; }
		bool OffScreen(const Bit& B, const Vec& At, const View& Eye) const; // (wholly outside the view, with room to spare for its size and the eye's shake)
		std::vector<Bit> Roots; // (each frame: the young end of each trail, from its point back to its newest hanging puff)

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
		bool GroundColour(const Vec& At, double& R255, double& G255, double& B255) const; // (the ground there as the game's screen shows it, if the game can say)
		// (what to write for the game's screen to show a value: picture.to_land, read once and then asked)
		struct LandTable
		{
			static constexpr int Most = 64;
			double Write[Most] = {};
			int Count = 0;
			double ToLand(double Screen255) const;
		};
		LandTable LandTableOf() const;
		void RaggedFlash(double T0, const Vec& At, double K, double Small); // (a shell's flash by the gun flashes' rule: burst.flash.tongues)
		void Skirt(double T0, const Vec& At, double K, const Paint& P, double DustK, Dice& D);
		void Crown(double T0, const Vec& At, double CrownH, double K, double Slow, const Paint& P, double ClodK, double ShowerK, const std::string& Size, Dice& D);
		void Smoke(double T0, const Vec& At, double CrownH, double K, double Scale, const std::string& Size, const std::string& Ground, const Paint& P, double DustK, Dice& D);
		void Scar(double T0, const Vec& At, double Across, const Paint& P, bool bSnow, Dice& D);
		void Armour(double T0, const Vec& At, double Scale, Dice& D);
		/** A tracer's streak (two layers, paler along its middle), its tail at Start when born, flying at Vel along Way;
		 * Drag and Drop (gravity) slow and bend it, for the air guns' long flights. */
		void Tracer(double Born, const Vec& Start, const Vec& Way, const Vec& Vel, double For, double Drag, double Drop, double Half = -1.0, double AlphaK = 1.0, double Behind = 0.0, double Until = 0.0);
		const AirRow& AirRowOf(const std::string& Kind);
		void AirRound(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, double Mps, bool bTracer, bool bSmoke, Dice& D);
		void CannonFlash(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, Dice& D);
		void Spent(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, Dice& D);
		const TrailRow& TrailRowOf(const std::string& Kind);
		const DustRow& DustRowOf(const std::string& Ground);
		void TrailPuff(double T, const Vec& At, const Vec& Carry, Trailing& Tr, const TrailRow& Row, Dice& D);
		void Shed(double Now, const Vec& At, const Vec& Vel, const Vec& Throw, double SpanM, double DeckZ, int Sort, Dice& D); // (one piece: Sort 0 a skin plate, 1 a heavy piece, 2 a wing)
		void TornFireball(double Now, const Vec& At, const Vec& Vel, double SpanM, Dice& D);
		double FlameOf(const Trailing& Tr, const TrailRow& Row) const; // (a fire trail's flame length: the row's, or by the aircraft's span)
		void Run(double Now); // the steady sources: fire, smouldering
		void OilColumn(Source& S, double T, bool bPall = false); // one puff of an oil fire's column (or its long pall), born at T
		Vec OnSlick(const Source& S, Dice& D) const; // a point anywhere on an oil fire's slick
		void OilBase(Source& S, double T); // one puff of the low smoke rolling off the slick, born at T
		OilRow OilRowOf(const std::string& Kind) const;
		OilRow OilRowAt(const std::string& Prefix) const; // (the row under any prefix, every dial held to a sane span)
		int OwnColumn(double Now, const Source& Fire); // (a big fire's own column: started, or this fire joined to one standing near; that column's Id, or -1 for none)
		SteamRow SteamRowOf(const std::string& Kind) const; // (the row, every dial held to a sane span)
		CookRow CookRowOf() const;
		void SteamRag(double T, const Vec& At, const Vec& Way, double Reach, double Across, double Push, const SteamRow& R, Dice& D); // (one rag of steam, born at T: Push 1 at full pressure, down to the row's least)
		void CookPop(const Source& S, double T); // (one pop of a cook-off)
		void CookJet(const Source& S, double T, double Full); // (one step of its hatch jet, Full 0 to 1 of its height)
		bool UnderOil(const Source& Column, double T) const; // (burning oil stands over one of its fires at T: the oil's column is hers)
		BankRow BankRowOf(double Across, uint64_t Seed) const;
		void SmokeBank(const Source& S, double T, bool bFlames, double After, const Rgb& Foot, double FootClear, double FootK, double Pale = 0.0, const Rgb& PaleCol = Rgb());
		void Prune(double Now);

		Vec Where(const Bit& B, double Age) const;
		Vec Going(const Bit& B, double Age) const;
		void Lay(int Slot, const Vec& Middle, const Vec& AxisA, const Vec& AxisB, const Rgb& Col, double Alpha, double HeartAlpha, double RimAlpha, double Inner, double Point, double Tail, double Shade, const View& Eye, double Rough = 0.0, uint32_t LumpSeed = 0, double Turn = 0.0, const Rgb& Floor = Rgb(), const Rgb& Through = Rgb(), const Vec& Outward = Vec(), const Rgb& Lift = Rgb(), const Vec& TaperB = Vec());
		int LayBit(int Slot, const Bit& B, double Now, const View& Eye, int Room); // (how many slots it took: 0, 1, or 2 for a twin)
	};
}
