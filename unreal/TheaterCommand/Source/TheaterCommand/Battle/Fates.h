#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * How a blow looks: the many ways a ship or a ground unit takes a hit, picked for the situation and never the same
 * twice running (Brian, 20 November 14:26Z, after asking the same of the air battles: "i want this same thing for
 * naval and ground battles"; his air ask: many different ways down, not every hit a kill, "variables and different
 * situations" so the battles never look the same and never get boring).
 *
 * LOOKS ONLY. The battle's numbers decide everything that counts (ruling 217): which ship is hurt and how badly, which
 * tank of a block is lost, when men waver, run or rally. Nothing here changes any of that, and nothing here draws from
 * the battle's own luck. What is decided here is only what the numbers leave open: how a result they gave looks. A
 * ship hurt but afloat may have a fire on her deck that her crew put out, or a torpedo's flooding that lists her until
 * they counter-flood; a tank lost may brew up, or be knocked out with its crew getting clear, or lose its turret. Where
 * the numbers have already named a ship's end in words (the air battle's "rolls over"), only that end is shown, and
 * only its details vary.
 *
 * What a blow shows turns on what struck it (a torpedo floods, a bomb burns, an armour-piercing round on a thick
 * front glances off), where it struck (side and rear hits on a tank find the ammunition and the engine), what it
 * struck (a destroyer breaks in two where a battleship's armour holds), the crew's nerve (a steady crew puts its fire
 * out; a shaken one bails out early) and the situation (in harbour a ship settles on the bottom; a moving tank hit in
 * the tracks slews round; a petrol engine burns more readily than a diesel; an American tank's ammunition burns less
 * once it is stowed in water jackets, from 1944).
 *
 * Never the same twice running: each battle keeps, for each group of looks (a ship, a tank, a man...), the last four
 * it showed. The last one is never shown next unless it is the only one allowed; the second to fourth last are
 * weighted 0.35, 0.6 and 0.85 (the air battles keep the same rule). And every look has dials (which side, how big, how
 * long, how many, where on the hull) that also shy away from the value they took last time, so even the same look
 * plays differently. A ship keeps her list to the side she was struck on (a sticky dial), and a once-only look (a
 * crew that bails out and climbs back in) is never shown twice on one unit.
 *
 * The looks are data: Content/Data/Battle/fates.json, its _about says what each field and each step means. The same
 * code runs in the game and in the cloud (Tools/fates/run.sh plays thousands of blows through it and checks it).
 * Plain C++, no engine: a scene turns the steps into the effects kit's calls and its models' moves.
 */
namespace Fates
{
	/** A JSON value, small and plain: the table is read once. */
	struct Json
	{
		enum class Type { Null, Bool, Number, String, Array, Object };
		Type Kind = Type::Null;
		bool Flag = false;
		double Value = 0.0;
		std::string Text;
		std::vector<Json> Items;
		std::vector<std::string> Keys; // an object's names, beside its values, in the order written
		std::vector<Json> Values;

		const Json& operator[](const char* Key) const;
		const Json& operator[](const std::string& Key) const { return (*this)[Key.c_str()]; }
		const Json& At(size_t Index) const;
		size_t Count() const { return Kind == Type::Array ? Items.size() : Kind == Type::Object ? Values.size() : 0; }
		bool IsNull() const { return Kind == Type::Null; }
		bool IsNumber() const { return Kind == Type::Number; }
		bool IsString() const { return Kind == Type::String; }
		bool IsArray() const { return Kind == Type::Array; }
		bool IsObject() const { return Kind == Type::Object; }
		double Num(double Default = 0.0) const { return Kind == Type::Number ? Value : Kind == Type::Bool ? (Flag ? 1.0 : 0.0) : Default; }
		bool Truth(bool Default = false) const { return Kind == Type::Bool ? Flag : Kind == Type::Number ? Value != 0.0 : Default; }
		std::string Str(const char* Default = "") const { return Kind == Type::String ? Text : std::string(Default); }
	};

	/** Reads JSON text; false (and why) on a fault. */
	bool ReadJson(const std::string& Source, Json& OutValue, std::string& OutError);

	/** The picker's own dice (never the battle's): a small, fast, fixed sequence from its seed. */
	class Dice
	{
	public:
		explicit Dice(uint64_t Seed = 1);
		double U(); // 0 to under 1
		double U(double A, double B) { return A + (B - A) * U(); }
		int Whole(int A, int B); // A to B, both included
	private:
		uint64_t State = 1;
	};

	/** A whole number from a name, the same on every machine (the dice's seeds). */
	uint64_t NameKey(const std::string& Name);

	/** One blow: what the numbers say happened to one unit (or one model of it) at one moment, and its situation. */
	struct Blow
	{
		std::string Domain = "sea";  // "sea" or "land"
		/**
		 * The table's family of what was struck. Sea: "bb", "ca", "cv", "dd", "ss", "ap" (a transport or merchant).
		 * Land: "tank_light", "tank_medium", "tank_heavy", "spgun", "halftrack", "truck", "armoured_car", "gun",
		 * "infantry".
		 */
		std::string Family;
		/**
		 * What struck it. Sea: "torpedo", "bomb" (a dive bomber's), "bomb_heavy" (a level bomber's armour-piercing),
		 * "shell_heavy" (a battleship's), "shell_medium" (a cruiser's), "shell_light" (a destroyer's). Land: the
		 * battle's calibre ("light", "mortar", "field", "howitzer", "heavy", "bomb") or "smallarms".
		 */
		std::string Weapon;
		/**
		 * What the numbers gave. "worn": hurt but still in the fight; "lost": a model (a tank, a man, a ship) gone;
		 * "out": a ship out of the fight, burning, making for port; and for a block of men: "waver", "rout", "rally".
		 */
		std::string Result = "worn";
		/** Where the numbers have already named the end in words ("capsize"): only that end's looks. Empty: any. */
		std::string How;
		/** Where it struck from: "front", "side", "rear" or "top". Empty: the picker draws it (the table's aspects). */
		std::string Aspect;
		double Hurt = 0.0;  // how hurt it was before this blow: 0 whole, 1 gone (1 - the numbers' strength)
		double Nerve = 1.0; // its crew's nerve: 0 broken, 1 steady (the numbers' morale)
		bool bMoving = false, bCover = false, bHarbour = false, bNight = false;
		std::string Ground; // land: "earth", "sand", "snow", "mud", "town", "woods", "water"
		int Year = 1941;
		std::string Fuel;     // land: "petrol" or "diesel"; empty: the family's own
		std::string Stowage;  // land: "wet" where the ammunition rides in water jackets (American tanks from 1944); empty: dry
		int64_t Unit = 0;     // the unit struck (a ship, a block): sticky dials and once-only looks are kept by it
		int Model = 0;        // which of its models
	};

	/** One step of a look, for the scene to play T seconds after the blow. The table's _about says what each Do means. */
	struct Beat
	{
		double T = 0.0;
		std::string Do;   // "blast", "column", "fire", "smoke", "steam", "spark", "list", "right", "trim", "settle", "clip", ...
		std::string At;   // where on the model: "fore", "mid", "aft", "bridge", "funnel", "turret", "engine", "hatch", ...
		std::string Kind; // a size row of the effects kit ("bomb500"), a clip's name, a colour ("black", "white")
		double A = 0.0, B = 0.0, C = 0.0;
		int N = 0;
	};

	/** What a blow shows. */
	struct Fate
	{
		std::string Key;    // the table's key: "ship.fire_fought"
		std::string Group;  // the memory's group: "ship", "tank", "soft", "gun", "man", "men"
		std::string Words;  // a few plain words, for logs and the art director's sheets
		bool bFinal = false; // it ends the model (the numbers' loss)
		std::string Aspect;  // where it struck from, as given or drawn
		std::vector<Beat> Beats; // in time order
		std::map<std::string, std::string> Dials; // every dial as drawn (the scene reads "side", "pace", "end" ...)

		/** A dial as a number (Default if it has none, or it is not a number). */
		double Dial(const char* Name, double Default = 0.0) const;
		std::string DialText(const char* Name, const char* Default = "") const;
	};

	/** One way a blow can look: a row of the table. */
	struct Way
	{
		std::string Key, Group, Words, Domain, How;
		std::vector<std::string> Families, On;
		double Weight = 1.0;
		bool bFinal = false, bOnce = false;
		std::map<std::string, std::map<std::string, double>> By; // "weapon": { "torpedo": 3, "*": 1 }
		std::map<std::string, double> If, Unless;                // a flag's weight when it is set / not set
		std::map<std::string, std::pair<double, double>> Lean;   // "nerve": weight at 0 and at 1, straight between
		double MinHurt = 0.0, MaxHurt = 1.0, MinNerve = 0.0, MaxNerve = 1.0;
		int FromYear = 0, ToYear = 9999;
		Json Dials;
		Json Beats;
	};

	/** The tables: Content/Data/Battle/fates.json. */
	class Table
	{
	public:
		bool Load(const std::string& Text, std::string& OutError);
		const std::vector<Way>& Ways() const { return All; }
		/** The weights of the last, second, third and fourth last look shown (the no-repeat rule). */
		const std::vector<double>& Memory() const { return Recent; }
		double DialRepeat() const { return SameDial; }
		const std::map<std::string, std::vector<std::pair<std::string, double>>>& Aspects() const { return AspectOdds; }
		/** The effects kit's size row for a weapon (the table's sizes), and its glance size (glances); empty if none. */
		std::string SizeOf(const std::string& Domain, const std::string& Weapon) const;
		std::string GlanceOf(const std::string& Domain, const std::string& Weapon) const;
		/** A way's family is one of these; the table's own sanity check at load. */
		const std::vector<std::string>& Families(const std::string& Domain) const;
	private:
		std::vector<Way> All;
		std::vector<double> Recent = { 0.0, 0.35, 0.6, 0.85 };
		double SameDial = 0.3;
		std::map<std::string, std::vector<std::pair<std::string, double>>> AspectOdds; // by domain
		std::map<std::string, std::vector<std::string>> Known;
		std::map<std::string, std::string> Sizes, Glances; // "domain|weapon" -> the kit's row
	};

	/** One battle's picker: its own dice, and its memory of what it has just shown. */
	class Director
	{
	public:
		Director(const Table& InTable, uint64_t BattleSeed);
		/** The look for a blow. A blow no way of the table fits gets an empty Key and no beats. */
		Fate Choose(const Blow& B);
		/** The named look for a blow, as Choose would draw it (its dials and steps; the memory takes it as shown). Empty
		 * Key if that look does not fit the blow. For a test reel that films each look in turn. */
		Fate Show(const Blow& B, const std::string& Key);
		/** Every look the blow could show and its weight now, the no-repeat rule applied (tests and sheets). */
		std::vector<std::pair<std::string, double>> Odds(const Blow& B) const;
		/** How many blows it has picked for. */
		int Picked() const { return Count; }
	private:
		const Table& Tables;
		uint64_t Seed = 1;
		int Count = 0;
		std::map<std::string, std::vector<std::string>> Shown;              // by group, newest first
		std::map<std::string, std::string> LastDial;                        // "way|dial" -> the value last drawn
		std::map<std::string, std::string> Sticky;                          // "unit|dial" -> the unit's own value
		std::map<std::string, int> Once;                                    // "unit|way" -> shown
		double WeightOf(const Way& W, const Blow& B, const std::string& Aspect) const;
		std::string DrawAspect(const Blow& B, Dice& D) const;
		std::string DrawDial(const Way& W, const std::string& Name, const Json& Spec, const Blow& B, const Fate& F, Dice& D);
		void Expand(const Way& W, const Json& Spec, const Fate& F, Dice& D, std::vector<Beat>& Out) const;
		Dice DiceFor(const Blow& B);
		void Build(const Way& W, const Blow& B, Fate& F, Dice& D);
	};

	/**
	 * A ship's lasting pose from the looks she has taken, eased in over each step's time: her list (degrees, plus to
	 * starboard), her trim (degrees, plus down by the bow) and how far she has settled (metres). "list" adds to the
	 * list toward a side, "right" (counter-flooding) takes most of it back, "trim" and "settle" add. Kept within what a
	 * ship afloat shows: her end is the numbers' and the scene's. The limits hold at every moment, not only on each
	 * step's aim (a new list eases in while an earlier counter-flooding is still easing out).
	 */
	class Hull
	{
	public:
		void Take(const Fate& F, double Now);
		void Pose(double Now, double& OutList, double& OutTrim, double& OutSettle) const;
		double MostList = 18.0, MostTrim = 6.0, MostSettle = 4.0;
	private:
		struct Move
		{
			double At = 0.0, For = 1.0, By = 0.0;
		};
		std::vector<Move> Lists, Trims, Settles;
		double ListAim = 0.0, TrimAim = 0.0, SettleAim = 0.0;
		static double Sum(const std::vector<Move>& Moves, double Now);
	};

	/** Eased from 0 to 1 over 0..1, gently at both ends. */
	double Ease(double X);
}
