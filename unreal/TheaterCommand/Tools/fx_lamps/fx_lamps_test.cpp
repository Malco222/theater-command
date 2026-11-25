// The flashes' lamps at sea (the sea's night fight, 25 November; look bible S7, "gun flashes light the hulls"): the
// effects kit's lamp rule (TheaterFx::Battle::SetLampRule) and its naval rows (muzzle.naval_heavy, naval_medium,
// naval_light), on scripted fights with the game's own recipe and the kit's own code, no engine.
//
//   - the default rule hands every lamp out as before (and run.sh holds fx_lamps_dump to the kit before the rule);
//   - with the sea's rule a battleship's broadside 2 km off beats a slick burning 6 km off; a salvo of turrets within
//     0.25 s and 60 m (and a battleship's 150 m) is one lamp; two ships 500 m apart are never one; 3 lamps when asked,
//     never more than 4; every lamp a number;
//   - each row's candelas at night and by day, and its reach against its band's hulls (the table);
//   - what the rule costs a frame with 40 ships firing;
//   - a picture from above of a night fight, today's lamps beside the sea rule's (fx-lamps-night.png, or .ppm).
//
//     Tools/fx_lamps/run.sh [out folder]
//     fx_lamps_test <battle-fx.json> <out folder>
#include "fx_lamps_recipe.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace
{
	using FxLamps::JsonRecipe;
	using TheaterFx::Battle;
	using TheaterFx::Vec;
	using Rule = TheaterFx::Battle::LampRule;

	constexpr double Fps = 60.0;
	int Checks = 0, Failed = 0;

	void Check(bool bOk, const char* What, const std::string& Got)
	{
		++Checks;
		if (!bOk)
		{
			++Failed;
		}
		std::printf("  %s %-62s %s\n", bOk ? "ok  " : "FAIL", What, Got.c_str());
	}

	std::string Say(const char* Format, double A = 0.0, double B = 0.0, double C = 0.0, double D = 0.0)
	{
		char Text[512];
		std::snprintf(Text, sizeof(Text), Format, A, B, C, D);
		return Text;
	}

	bool RealLamp(const Battle::Lamp& L)
	{
		return std::isfinite(L.At.X) && std::isfinite(L.At.Y) && std::isfinite(L.At.Z) && std::isfinite(L.Strength) && std::isfinite(L.Radius) && std::isfinite(L.Col.R) && std::isfinite(L.Col.G) && std::isfinite(L.Col.B) && L.Strength >= 0.0 && L.Radius >= 0.0;
	}

	/** A ship: amidships on the water, heading along Way (level), her length, her guns' row and turrets (or mounts). */
	struct Ship
	{
		Vec Mid;
		Vec Way = Vec(1.0, 0.0, 0.0);
		double Long = 240.0;
		const char* Row = "naval_heavy";
		int Turrets = 3;
		double Spread = 0.0; // (metres from her first turret to her last; 0: 0.64 of her length)
		double Barrel = 18.0, High = 12.0;
	};

	/** The ships of the naval bands, as the rows' notes name them: a battleship, a cruiser, a destroyer. */
	Ship Battleship(const Vec& Mid)
	{
		Ship S;
		S.Mid = Mid;
		return S;
	}
	Ship Cruiser(const Vec& Mid)
	{
		Ship S;
		S.Mid = Mid;
		S.Long = 190.0;
		S.Row = "naval_medium";
		S.Turrets = 4;
		S.Barrel = 10.0;
		S.High = 9.0;
		return S;
	}
	Ship Destroyer(const Vec& Mid)
	{
		Ship S;
		S.Mid = Mid;
		S.Long = 115.0;
		S.Row = "naval_light";
		S.Turrets = 4;
		S.Barrel = 5.0;
		S.High = 6.0;
		return S;
	}

	/** One gun firing: when, where its muzzle is, which way, and its row. */
	struct Shot
	{
		double T = 0.0;
		Vec At, Facing;
		std::string Row;
		int ShipNo = -1;
	};

	/** A ship's salvo toward Toward from T: each turret Gap seconds after the one before, fore to aft. Row: the ship's
	 *  own (nullptr) or another (today's: howitzer at 180 mm and up, tank_gun below). */
	void Salvo(std::vector<Shot>& Into, const Ship& S, int ShipNo, double T, double Gap, const Vec& Toward, const char* Row = nullptr)
	{
		const Vec Side = (Toward - S.Mid);
		const Vec Level = Vec(Side.X, Side.Y, 0.0).Unit(Vec(0.0, 1.0, 0.0));
		const Vec Facing = (Level + Vec(0.0, 0.0, 0.12)).Unit();
		const double Spread = S.Spread > 0.0 ? S.Spread : 0.64 * S.Long;
		for (int K = 0; K < S.Turrets; ++K)
		{
			const double Along = S.Turrets > 1 ? 0.5 * Spread - Spread * K / (S.Turrets - 1) : 0.0;
			Shot One;
			One.T = T + Gap * K;
			One.At = S.Mid + S.Way * Along + Level * S.Barrel + Vec(0.0, 0.0, S.High);
			One.Facing = Facing;
			One.Row = Row ? Row : S.Row;
			One.ShipNo = ShipNo;
			Into.push_back(One);
		}
	}

	/** A fight played frame by frame: its kit, its shots in time order, the eye, the lamps asked and the day. */
	struct Fight
	{
		Battle Fx;
		std::vector<Shot> Shots;
		size_t Next = 0;
		Battle::View View;
		int Room = 2;
		double Day = 0.0;
		int FrameNo = 0;
		Battle::Lamp Lit[16];
		int NLit = 0;
		double LampsUs = 0.0; // (the last frame's Lamps, in microseconds)

		void Open(const JsonRecipe& Book, uint32_t Seed, const Vec& Eye, const Vec& Look)
		{
			Fx.Setup(&Book, &FxLamps::Flat, nullptr, Seed);
			Fx.SetWind(Vec(3.0, 0.8, 0.0));
			View = FxLamps::Looking(Eye, Look);
		}
		void Order()
		{
			std::stable_sort(Shots.begin(), Shots.end(), [](const Shot& A, const Shot& B) { return A.T < B.T; });
			Next = 0;
		}
		double Now() const
		{
			return FrameNo / Fps;
		}
		/** One frame: what fires by now (stamped now, as the wrapper stamps a call with its last Tick), Draw, the lamps. */
		void Step()
		{
			const double T = Now();
			while (Next < Shots.size() && Shots[Next].T <= T)
			{
				const Shot& S = Shots[Next++];
				Fx.Muzzle(T, S.At, S.Facing, S.Row.c_str(), "water");
			}
			Fx.Draw(T, View);
			const auto Start = std::chrono::steady_clock::now();
			NLit = Fx.Lamps(T, Lit, Room, Day);
			const auto End = std::chrono::steady_clock::now();
			LampsUs = std::chrono::duration<double, std::micro>(End - Start).count();
			++FrameNo;
		}
	};

	/** The lamps of this frame that lie by a ship (within half her length and Over metres more of amidships). */
	int LampsBy(const Fight& F, const Ship& S, double Over = 80.0)
	{
		int N = 0;
		for (int I = 0; I < F.NLit; ++I)
		{
			const Vec D = F.Lit[I].At - S.Mid;
			if (std::sqrt(D.X * D.X + D.Y * D.Y) <= 0.5 * S.Long + Over)
			{
				++N;
			}
		}
		return N;
	}

	/** The strongest lamp of this frame by a ship, or -1. */
	int LampOf(const Fight& F, const Ship& S, double Over = 80.0)
	{
		int Best = -1;
		for (int I = 0; I < F.NLit; ++I)
		{
			const Vec D = F.Lit[I].At - S.Mid;
			if (std::sqrt(D.X * D.X + D.Y * D.Y) <= 0.5 * S.Long + Over && (Best < 0 || F.Lit[I].Strength > F.Lit[Best].Strength))
			{
				Best = I;
			}
		}
		return Best;
	}

	bool SameLamps(const Fight& A, const Fight& B)
	{
		if (A.NLit != B.NLit)
		{
			return false;
		}
		for (int I = 0; I < A.NLit; ++I)
		{
			const Battle::Lamp& X = A.Lit[I];
			const Battle::Lamp& Y = B.Lit[I];
			if (X.At.X != Y.At.X || X.At.Y != Y.At.Y || X.At.Z != Y.At.Z || X.Strength != Y.Strength || X.Radius != Y.Radius || X.Col.R != Y.Col.R || X.Col.G != Y.Col.G || X.Col.B != Y.Col.B)
			{
				return false;
			}
		}
		return true;
	}

	/** A small steady random source for the test's own scenes (SplitMix64): one throw a statement. */
	struct Mix
	{
		uint64_t S = 1;
		double U()
		{
			S += 0x9E3779B97F4A7C15ull;
			uint64_t Z = S;
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			Z ^= Z >> 31;
			return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
		}
	};

	double Far(const Vec& A, const Vec& B)
	{
		return (A - B).Length();
	}

	// ---------------------------------------------------------------------------------------------------------------
	// 1. The default rule: as before

	void DefaultAsBefore(const JsonRecipe& Book)
	{
		std::printf("\nThe default rule hands the lamps out as before\n");
		// (three kits on one fight with every kind of lamp, naval rows too: one never given a rule, one given the default
		// rule, one given the sea's and then the default again; each frame's lamps must match to the bit)
		Fight Never, Given, Back;
		Fight* All[3] = { &Never, &Given, &Back };
		const Vec Eye(0.0, -300.0, 30.0), Look(0.0, 1500.0, 0.0);
		for (Fight* F : All)
		{
			F->Open(Book, 5u, Eye, Look);
			F->Fx.OilFire(0.0, 77, Vec(3000.0, 5000.0, 0.0), 150.0, "sunk", 0.0, 30.0);
			F->Fx.Burn(0.5, Vec(-40.0, 80.0, 2.0), 3.0, 20.0, 7);
			F->Room = 2;
			for (int Ship_ = 0; Ship_ < 3; ++Ship_)
			{
				const Ship S = Ship_ == 0 ? Battleship(Vec(-600.0, 1500.0, 0.0)) : Ship_ == 1 ? Cruiser(Vec(0.0, 1500.0, 0.0)) : Destroyer(Vec(600.0, 1500.0, 0.0));
				for (int K = 0; K < 6; ++K)
				{
					Salvo(F->Shots, S, Ship_, 0.4 + 2.1 * K + 0.3 * Ship_, 0.08, Vec(0.0, 9000.0, 0.0));
					Salvo(F->Shots, S, Ship_, 1.1 + 2.1 * K, 0.0, Vec(0.0, 9000.0, 0.0), Ship_ == 1 ? "tank_gun" : "howitzer");
				}
			}
			F->Order();
		}
		Given.Fx.SetLampRule(Rule());
		Back.Fx.SetLampRule(Rule::Sea());
		Back.Fx.SetLampRule(Rule());
		int Same = 0, Frames = 0, Lit = 0;
		for (int Frame = 0; Frame < 900; ++Frame)
		{
			for (Fight* F : All)
			{
				F->Step();
			}
			++Frames;
			Lit += Never.NLit > 0 ? 1 : 0;
			Same += SameLamps(Never, Given) && SameLamps(Never, Back) ? 1 : 0;
		}
		Check(Same == Frames && Lit > 600, "a rule of all defaults: every lamp as never ruled", Say("%.0f of %.0f frames the same to the bit (%.0f with lamps lit)", Same, Frames, Lit));
		Check(Never.Fx.LampsWanted() == 2 && Back.Fx.LampsWanted() == 2, "no rule: game.max_lamps lamps wanted", Say("%.0f and %.0f", Never.Fx.LampsWanted(), Back.Fx.LampsWanted()));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// 2. A broadside 2 km off beats a slick burning 6 km off

	struct Duel
	{
		int BothShips = 0, OilFrames = 0, Frames = 0, OneEach = 0, OilBackBy = -1;
		double ShipRank = 0.0, OilRank = 0.0;
	};

	Duel PlayDuel(const JsonRecipe& Book, bool bSeaRows, const Rule* R, int Room)
	{
		// (the eye 20 m up at the origin looking north; two battleships 2 km north, 600 m apart, fire their broadsides
		// north at 3 s, turret after turret 0.08 s apart; a sunk ship's oil burns 6 km off since long before)
		Fight F;
		const Vec Eye(0.0, 0.0, 20.0);
		F.Open(Book, 9u, Eye, Vec(0.0, 4000.0, 0.0));
		const Vec OilAt(2000.0, 5657.0, 0.0);
		F.Fx.OilFire(0.0, 41, OilAt, 150.0, "sunk", 0.0, 60.0);
		const Ship P = Battleship(Vec(0.0, 2000.0, 0.0)), Q = Battleship(Vec(-600.0, 2000.0, 0.0));
		Salvo(F.Shots, P, 0, 3.0, 0.08, Vec(0.0, 12000.0, 0.0), bSeaRows ? nullptr : "howitzer");
		Salvo(F.Shots, Q, 1, 3.04, 0.08, Vec(0.0, 12000.0, 0.0), bSeaRows ? nullptr : "howitzer");
		F.Order();
		if (R)
		{
			F.Fx.SetLampRule(*R);
		}
		F.Room = Room;
		Duel D;
		int LastShipFrame = -1;
		for (int Frame = 0; Frame < static_cast<int>(5.5 * Fps); ++Frame)
		{
			F.Step();
			const double T = F.Now() - 1.0 / Fps;
			bool bOil = false;
			for (int I = 0; I < F.NLit; ++I)
			{
				bOil = bOil || Far(F.Lit[I].At, OilAt) < 300.0;
			}
			const int ByP = LampsBy(F, P), ByQ = LampsBy(F, Q);
			if (T >= 3.0 && T < 3.6)
			{
				++D.Frames;
				D.OilFrames += bOil ? 1 : 0;
				D.BothShips += ByP > 0 && ByQ > 0 ? 1 : 0;
				D.OneEach += ByP == 1 && ByQ == 1 ? 1 : 0;
				const int Lp = LampOf(F, P);
				if (Lp >= 0)
				{
					const double Dp = Far(F.Lit[Lp].At, Eye);
					D.ShipRank = std::max(D.ShipRank, F.Lit[Lp].Strength / (Dp * Dp));
				}
			}
			if (ByP + ByQ > 0)
			{
				LastShipFrame = Frame;
			}
			if (T > 3.0 && bOil && D.OilBackBy < 0 && LastShipFrame >= 0 && Frame > LastShipFrame && ByP + ByQ == 0)
			{
				D.OilBackBy = Frame - LastShipFrame;
			}
		}
		const double Do = Far(OilAt, Eye);
		D.OilRank = 0.25 * std::sqrt(150.0) / (Do * Do); // (the oil's own lamp, oil_fires.sunk.lamp times the root of its reach)
		return D;
	}

	void BroadsideBeatsOil(const JsonRecipe& Book)
	{
		std::printf("\nA battleship's broadside 2 km off against a slick burning 6 km off (night)\n");
		const Duel Today = PlayDuel(Book, false, nullptr, 2);
		Check(Today.OilFrames == Today.Frames && Today.BothShips == 0, "today (howitzer rows, by strength, 2 lamps): the slick keeps one", Say("the slick lit %.0f of %.0f frames of the broadsides; both ships lit in %.0f", Today.OilFrames, Today.Frames, Today.BothShips));
		const Rule Sea = Rule::Sea();
		const Duel Three = PlayDuel(Book, true, &Sea, 3);
		Check(Three.OneEach > 0 && Three.OilFrames == Three.Frames, "the sea rule, 3 lamps: one lamp each broadside, the slick keeps its own", Say("one lamp each ship in %.0f frames, the slick lit %.0f of %.0f", Three.OneEach, Three.OilFrames, Three.Frames));
		Rule Two = Rule::Sea();
		Two.Most = 2;
		const Duel Duo = PlayDuel(Book, true, &Two, 2);
		Check(Duo.OneEach > 0 && Duo.OilFrames < Duo.Frames, "the sea rule, 2 lamps: both broadsides beat the slick", Say("one lamp each ship in %.0f frames; the slick lit %.0f of %.0f", Duo.OneEach, Duo.OilFrames, Duo.Frames));
		Check(Duo.OilBackBy > 0 && Duo.OilBackBy <= 3, "  ...and the slick's lamp is back the frame the broadsides' go out", Say("%.0f frames after", Duo.OilBackBy));
		Check(Three.ShipRank > 100.0 * Three.OilRank, "the broadside sends the eye over 100 times the slick's light", Say("%.3g against %.3g (strength over distance squared)", Three.ShipRank, Three.OilRank));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// 3. A salvo is one lamp; two ships 500 m apart are two

	void SalvoIsOneLamp(const JsonRecipe& Book)
	{
		std::printf("\nA salvo's turrets light one lamp; two ships 500 m apart, two\n");
		const Vec Eye(0.0, 0.0, 20.0);
		for (int Case = 0; Case < 3; ++Case)
		{
			// (0: a cruiser's four turrets over 60 m, 0.08 s apart (0.24 s in all); 1: a battleship's three over 154 m
			// at once; 2: a destroyer's four mounts over 60 m, 0.06 s apart)
			Ship S = Case == 0 ? Cruiser(Vec(0.0, 3000.0, 0.0)) : Case == 1 ? Battleship(Vec(0.0, 3000.0, 0.0)) : Destroyer(Vec(0.0, 3000.0, 0.0));
			if (Case != 1)
			{
				S.Spread = 60.0;
			}
			const double Gap = Case == 0 ? 0.08 : Case == 1 ? 0.0 : 0.06;
			int Most[2] = { 0, 0 }, LitFrames[2] = { 0, 0 };
			double Merged = 0.0, Single = 0.0;
			for (int Ruled = 0; Ruled < 2; ++Ruled)
			{
				Fight F;
				F.Open(Book, 13u, Eye, S.Mid);
				Salvo(F.Shots, S, 0, 2.0, Gap, Vec(0.0, 15000.0, 0.0));
				F.Order();
				if (Ruled == 1)
				{
					F.Fx.SetLampRule(Rule::Sea());
				}
				F.Room = 3;
				for (int Frame = 0; Frame < static_cast<int>(3.0 * Fps); ++Frame)
				{
					F.Step();
					const int By = LampsBy(F, S);
					Most[Ruled] = std::max(Most[Ruled], By);
					LitFrames[Ruled] += By > 0 ? 1 : 0;
					const int L = LampOf(F, S);
					if (L >= 0)
					{
						(Ruled == 1 ? Merged : Single) = std::max(Ruled == 1 ? Merged : Single, F.Lit[L].Strength);
					}
				}
			}
			const char* Name = Case == 0 ? "a cruiser's 4 turrets, 60 m, 0.24 s: one lamp" : Case == 1 ? "a battleship's 3 turrets, 154 m, at once: one lamp" : "a destroyer's 4 mounts, 60 m, 0.18 s: one lamp";
			Check(Most[1] == 1 && LitFrames[1] > 0 && Most[0] >= 2, Name, Say("sea rule at most %.0f lamp a frame (%.0f frames lit), by strength alone %.0f; the lamp %.2f times its gun's", Most[1], LitFrames[1], Most[0], Single > 0.0 ? Merged / Single : 0.0));
		}
		// (two battleships in line ahead 500 m apart, centre to centre, all their turrets inside one salvo's time)
		{
			Fight F;
			F.Open(Book, 17u, Eye, Vec(0.0, 2500.0, 0.0));
			const Ship P = Battleship(Vec(-250.0, 2500.0, 0.0)), Q = Battleship(Vec(250.0, 2500.0, 0.0));
			Salvo(F.Shots, P, 0, 2.0, 0.05, Vec(0.0, 15000.0, 0.0));
			Salvo(F.Shots, Q, 1, 2.02, 0.05, Vec(0.0, 15000.0, 0.0));
			F.Order();
			F.Fx.SetLampRule(Rule::Sea());
			F.Room = 3;
			int Both = 0, Between = 0;
			for (int Frame = 0; Frame < static_cast<int>(3.0 * Fps); ++Frame)
			{
				F.Step();
				Both += LampsBy(F, P, 40.0) == 1 && LampsBy(F, Q, 40.0) == 1 ? 1 : 0;
				for (int I = 0; I < F.NLit; ++I)
				{
					Between += Far(F.Lit[I].At, P.Mid + Vec(0.0, 18.0, 0.0)) > 0.5 * P.Long + 40.0 && Far(F.Lit[I].At, Q.Mid + Vec(0.0, 18.0, 0.0)) > 0.5 * Q.Long + 40.0 ? 1 : 0;
				}
			}
			Check(Both > 0 && Between == 0, "two battleships 500 m apart firing together: two lamps", Say("a lamp by each in %.0f frames, %.0f lamps between them", Both, Between));
		}
		// (and so on many pairs: two ships of any band 500 m apart, headings and moments at random, never one lamp)
		{
			Mix M;
			M.S = 2024;
			int Pairs = 0, Joined = 0;
			for (int Pair = 0; Pair < 60; ++Pair)
			{
				const double Kind = M.U();
				const double Heading = M.U() * 6.283185307179586;
				const double Lag = M.U() * 0.3;
				const Vec Way(std::cos(Heading), std::sin(Heading), 0.0);
				const double MidX = M.U() * 4000.0 - 2000.0;
				const double MidY = 2000.0 + M.U() * 3000.0;
				const Vec Mid(MidX, MidY, 0.0);
				Ship P = Kind < 0.34 ? Battleship(Mid) : Kind < 0.67 ? Cruiser(Mid) : Destroyer(Mid);
				P.Way = Way;
				Ship Q = P;
				Q.Mid = Mid + Way * 500.0;
				Fight F;
				F.Open(Book, 100u + static_cast<uint32_t>(Pair), Eye, Mid);
				Salvo(F.Shots, P, 0, 1.0, 0.06, Mid + Vec(-Way.Y, Way.X, 0.0) * 9000.0);
				Salvo(F.Shots, Q, 1, 1.0 + Lag, 0.06, Mid + Vec(-Way.Y, Way.X, 0.0) * 9000.0);
				F.Order();
				F.Fx.SetLampRule(Rule::Sea());
				F.Room = 3;
				for (int Frame = 0; Frame < static_cast<int>(2.0 * Fps); ++Frame)
				{
					F.Step();
					for (int I = 0; I < F.NLit; ++I)
					{
						const Vec L = F.Lit[I].At;
						const Vec Dp = L - P.Mid, Dq = L - Q.Mid;
						const double Fp = std::sqrt(Dp.X * Dp.X + Dp.Y * Dp.Y), Fq = std::sqrt(Dq.X * Dq.X + Dq.Y * Dq.Y);
						Joined += std::min(Fp, Fq) > 0.5 * P.Long + 60.0 ? 1 : 0;
					}
				}
				++Pairs;
			}
			Check(Joined == 0, "60 pairs 500 m apart, any band, heading and lag: never joined", Say("%.0f pairs, %.0f lamps lying between two ships", Pairs, Joined));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// 4. Lamps asked, the cap, numbers

	void RoomAndCap(const JsonRecipe& Book)
	{
		std::printf("\nThe lamps asked for, never more than 4, every one a number\n");
		Rule Nine = Rule::Sea();
		Nine.Most = 9;
		Battle Probe;
		Probe.Setup(&Book, &FxLamps::Flat, nullptr, 3u);
		Probe.SetLampRule(Rule::Sea());
		const int Sea = Probe.LampsWanted();
		Probe.SetLampRule(Nine);
		const int Capped = Probe.LampsWanted();
		Check(Sea == 3 && Capped == Battle::MostLamps && Battle::MostLamps == 4, "the sea rule wants 3; asked for 9, 4", Say("%.0f and %.0f (the cap %.0f)", Sea, Capped, Battle::MostLamps));
		// (a busy fight: twelve ships firing all through, fires and oil; the sea rule's 3, and a rule of 9 asked with room
		// for 16)
		int MostSea = 0, MostNine = 0, Bad = 0, FramesWithThree = 0;
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			Fight F;
			F.Open(Book, 23u, Vec(0.0, 0.0, 25.0), Vec(0.0, 3000.0, 0.0));
			F.Fx.OilFire(0.0, 1, Vec(-1500.0, 2500.0, 0.0), 120.0, "sunk", 0.0, 20.0);
			F.Fx.OilFire(0.0, 2, Vec(1500.0, 3500.0, 0.0), 80.0, "burning", 0.0, 20.0);
			for (int I = 0; I < 12; ++I)
			{
				const Ship S = I % 3 == 0 ? Battleship(Vec(-2400.0 + 450.0 * I, 1800.0 + 400.0 * (I % 2), 0.0)) : I % 3 == 1 ? Cruiser(Vec(-2400.0 + 450.0 * I, 1800.0 + 400.0 * (I % 2), 0.0)) : Destroyer(Vec(-2400.0 + 450.0 * I, 1800.0 + 400.0 * (I % 2), 0.0));
				for (int K = 0; K < 8; ++K)
				{
					Salvo(F.Shots, S, I, 0.3 + 0.17 * I + 1.3 * K, 0.07, Vec(0.0, 12000.0, 0.0));
				}
			}
			F.Order();
			F.Fx.SetLampRule(Pass == 0 ? Rule::Sea() : Nine);
			F.Room = Pass == 0 ? F.Fx.LampsWanted() : 16;
			for (int Frame = 0; Frame < static_cast<int>(12.0 * Fps); ++Frame)
			{
				F.Step();
				(Pass == 0 ? MostSea : MostNine) = std::max(Pass == 0 ? MostSea : MostNine, F.NLit);
				FramesWithThree += Pass == 0 && F.NLit == 3 ? 1 : 0;
				for (int I = 0; I < F.NLit; ++I)
				{
					Bad += RealLamp(F.Lit[I]) ? 0 : 1;
				}
			}
		}
		Check(MostSea == 3 && FramesWithThree > 0, "the sea rule lights 3 lamps", Say("at most %.0f a frame, 3 in %.0f frames", MostSea, FramesWithThree));
		Check(MostNine == 4, "asked for 9 with room for 16: never more than 4", Say("at most %.0f a frame", MostNine));
		// (wild calls: a muzzle not a number, without end, a way not a number, a calibre row unknown; the lamps stay numbers)
		{
			Fight F;
			F.Open(Book, 29u, Vec(0.0, 0.0, 25.0), Vec(0.0, 3000.0, 0.0));
			F.Fx.SetLampRule(Rule::Sea());
			F.Room = 4;
			const double Nan = std::nan(""), Inf = std::numeric_limits<double>::infinity();
			for (int Frame = 0; Frame < static_cast<int>(3.0 * Fps); ++Frame)
			{
				const double T = F.Now();
				if (Frame % 7 == 0)
				{
					F.Fx.Muzzle(T, Vec(Nan, 2000.0, 12.0), Vec(0.0, 1.0, 0.1), "naval_heavy", "water");
					F.Fx.Muzzle(T, Vec(100.0, Inf, 12.0), Vec(0.0, 1.0, 0.1), "naval_medium", "water");
					F.Fx.Muzzle(T, Vec(200.0, 2000.0, 12.0), Vec(Nan, Nan, Nan), "naval_light", "water");
					F.Fx.Muzzle(T, Vec(300.0, 2000.0, 12.0), Vec(0.0, 1.0, 0.1), "naval_heavy", "water");
					F.Fx.Muzzle(T, Vec(320.0, 2000.0, 12.0), Vec(0.0, 1.0, 0.1), "naval_heavy", "water");
				}
				F.Step();
				for (int I = 0; I < F.NLit; ++I)
				{
					Bad += RealLamp(F.Lit[I]) ? 0 : 1;
				}
			}
			Check(Bad == 0, "every lamp a number, wild calls too", Say("%.0f lamps not a number", Bad));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// 5. The rows: candelas at night and by day, reach, how long

	struct RowSeen
	{
		double NightCd = 0.0, DayCd = 0.0, DuskCd = 0.0, Reach = 0.0, Frames = 0.0, Held = 0.0;
	};

	RowSeen SeeRow(const JsonRecipe& Book, const char* Row)
	{
		RowSeen Out;
		const double CdAtFull = Book.Get("game.lamp_candela_at_full", 2000.0);
		for (int Light = 0; Light < 3; ++Light)
		{
			Fight F;
			F.Open(Book, 31u, Vec(0.0, 0.0, 20.0), Vec(0.0, 2000.0, 0.0));
			F.Shots.push_back(Shot{ 1.0, Vec(0.0, 2000.0, 14.0), Vec(0.0, 1.0, 0.12), Row, 0 });
			F.Order();
			F.Fx.SetLampRule(Rule::Sea());
			F.Room = 3;
			F.Day = Light == 0 ? 0.0 : Light == 1 ? 1.0 : 0.5;
			double First = 0.0;
			int Lit = 0;
			double Last = 0.0;
			for (int Frame = 0; Frame < static_cast<int>(2.0 * Fps); ++Frame)
			{
				F.Step();
				if (F.NLit > 0)
				{
					if (Lit == 0)
					{
						First = F.Lit[0].Strength * CdAtFull;
						Out.Reach = F.Lit[0].Radius;
					}
					++Lit;
					Last = F.Now() - 1.0 / Fps - 1.0;
				}
			}
			(Light == 0 ? Out.NightCd : Light == 1 ? Out.DayCd : Out.DuskCd) = First;
			if (Light == 0)
			{
				Out.Frames = Lit;
				Out.Held = Last;
			}
		}
		return Out;
	}

	void Rows(const JsonRecipe& Book)
	{
		std::printf("\nThe naval rows at night and by day (one gun alone, the frame after it fires)\n");
		struct Band
		{
			const char* Row;
			double HullLo, HullHi;
			bool bDay;
			const char* Hulls;
		};
		const Band Bands[3] = {
			{ "naval_heavy", 200.0, 270.0, true, "battleships 216-270 m" },
			{ "naval_medium", 160.0, 210.0, true, "cruisers 169-205 m" },
			{ "naval_light", 100.0, 120.0, false, "destroyers 110-119 m" },
		};
		std::printf("  %-13s %10s %8s %10s %10s %7s  %s\n", "row", "cd night", "reach m", "cd dusk", "cd by day", "lit s", "its band's hulls");
		const double Share = Book.Get("game.day_lamp_share", 0.25);
		for (const Band& B : Bands)
		{
			const RowSeen S = SeeRow(Book, B.Row);
			const double Asked = Book.Get(std::string("muzzle.") + B.Row + ".lamp_cd", -1.0);
			std::printf("  %-13s %10.0f %8.0f %10.0f %10.0f %7.3f  %s\n", B.Row, S.NightCd, S.Reach, S.DuskCd, S.DayCd, S.Held, B.Hulls);
			Check(std::fabs(S.NightCd - Asked) <= 1e-6 * Asked, (std::string(B.Row) + ": at night its own lamp_cd").c_str(), Say("%.1f cd (lamp_cd %.1f)", S.NightCd, Asked));
			if (B.bDay)
			{
				Check(std::fabs(S.DayCd - Share * Asked) <= 1e-6 * Asked, (std::string(B.Row) + ": by day game.day_lamp_share of it").c_str(), Say("%.1f cd (%.2f of %.0f)", S.DayCd, Share, Asked));
			}
			else
			{
				Check(S.DayCd == 0.0, (std::string(B.Row) + ": by day no lamp (a small gun's)").c_str(), Say("%.1f cd", S.DayCd));
			}
			Check(S.Reach >= B.HullLo && S.Reach <= B.HullHi, (std::string(B.Row) + ": reach about its band's hull").c_str(), Say("%.0f m (%.0f to %.0f)", S.Reach, B.HullLo, B.HullHi));
			Check(S.Held > 0.03 && S.Held <= Book.Get(std::string("muzzle.") + B.Row + ".lamp_s", 0.0) + 1e-9, (std::string(B.Row) + ": lit while its fireball burns, lamp_s at most").c_str(), Say("%.3f s (lamp_s %.2f)", S.Held, Book.Get(std::string("muzzle.") + B.Row + ".lamp_s", 0.0)));
		}
		// (today's rows, as the sea scene fires them now, for the table: by strength, 2 frames)
		for (const char* Row : { "howitzer", "tank_gun" })
		{
			const RowSeen S = SeeRow(Book, Row);
			std::printf("  %-13s %10.1f %8.1f %10.1f %10.1f %7.3f  (today's, for a naval gun)\n", Row, S.NightCd, S.Reach, S.DuskCd, S.DayCd, S.Held);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// 6. The cost: 40 ships firing

	void Cost(const JsonRecipe& Book)
	{
		std::printf("\nThe cost of the lamps a frame, 40 ships firing\n");
		for (int Ruled = 0; Ruled < 2; ++Ruled)
		{
			Fight F;
			F.Open(Book, 37u, Vec(0.0, -1500.0, 25.0), Vec(0.0, 5000.0, 0.0));
			for (int I = 0; I < 4; ++I)
			{
				F.Fx.OilFire(0.0, 300 + I, Vec(-3000.0 + 2000.0 * I, 4500.0 + 700.0 * (I % 2), 0.0), 100.0 + 30.0 * I, I % 2 == 0 ? "sunk" : "burning", 0.0, 20.0);
			}
			std::vector<Ship> Fleet;
			for (int Line = 0; Line < 2; ++Line)
			{
				for (int I = 0; I < 20; ++I)
				{
					const Vec Mid(-6000.0 + 600.0 * I, Line == 0 ? 0.0 : 9000.0, 0.0);
					Fleet.push_back(I < 6 ? Battleship(Mid) : I < 14 ? Cruiser(Mid) : Destroyer(Mid));
				}
			}
			for (size_t I = 0; I < Fleet.size(); ++I)
			{
				// (every ship a salvo every 2 s, out of step with the rest: far faster than real big guns, for the cost)
				const Vec Toward(Fleet[I].Mid.X, Fleet[I].Mid.Y < 1.0 ? 9000.0 : 0.0, 0.0);
				for (int K = 0; K < 8; ++K)
				{
					Salvo(F.Shots, Fleet[I], static_cast<int>(I), 0.2 + 0.049 * static_cast<double>(I) + 2.0 * K, 0.08, Toward);
				}
			}
			// (and one frame where all forty fire at once)
			for (size_t I = 0; I < Fleet.size(); ++I)
			{
				Salvo(F.Shots, Fleet[I], static_cast<int>(I), 17.0, 0.0, Vec(Fleet[I].Mid.X, Fleet[I].Mid.Y < 1.0 ? 9000.0 : 0.0, 0.0));
			}
			F.Order();
			if (Ruled == 1)
			{
				F.Fx.SetLampRule(Rule::Sea());
			}
			F.Room = F.Fx.LampsWanted();
			std::vector<double> Us;
			double AllAt = 0.0;
			for (int Frame = 0; Frame < static_cast<int>(18.0 * Fps); ++Frame)
			{
				F.Step();
				if (F.Now() > 1.0)
				{
					Us.push_back(F.LampsUs);
				}
				if (std::fabs(F.Now() - 1.0 / Fps - 17.0) < 0.5 / Fps)
				{
					AllAt = F.LampsUs;
				}
			}
			std::vector<double> Sorted = Us;
			std::sort(Sorted.begin(), Sorted.end());
			double Sum = 0.0;
			for (double U : Us)
			{
				Sum += U;
			}
			const double Mean = Us.empty() ? 0.0 : Sum / static_cast<double>(Us.size());
			const double P99 = Sorted.empty() ? 0.0 : Sorted[static_cast<size_t>(0.99 * static_cast<double>(Sorted.size() - 1))];
			const double Worst = Sorted.empty() ? 0.0 : Sorted.back();
			std::printf("  time: %-22s %6.2f us a frame on the mean, %6.2f at the 99th hundredth, %6.2f the worst; all forty at once %6.2f us\n", Ruled == 1 ? "the sea rule, 3 lamps" : "the default, 2 lamps", Mean, P99, Worst, AllAt);
			if (Ruled == 1)
			{
				Check(Mean < 50.0, "the sea rule's lamps a frame, 40 ships firing: well under 50 us", Say("%.2f us on the mean (99th hundredth %.2f; all forty at once %.2f)", Mean, P99, AllAt));
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// 7. The picture: a night fight from above, today's lamps beside the sea rule's

	struct Picture
	{
		int W = 0, H = 0;
		std::vector<double> Rgb;
		void Make(int InW, int InH)
		{
			W = InW;
			H = InH;
			Rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3u, 0.0);
		}
		void Put(int X, int Y, double R, double G, double B, double A = 1.0)
		{
			if (X < 0 || Y < 0 || X >= W || Y >= H)
			{
				return;
			}
			const size_t I = (static_cast<size_t>(Y) * static_cast<size_t>(W) + static_cast<size_t>(X)) * 3u;
			Rgb[I] += (R - Rgb[I]) * A;
			Rgb[I + 1] += (G - Rgb[I + 1]) * A;
			Rgb[I + 2] += (B - Rgb[I + 2]) * A;
		}
		void Disc(double Cx, double Cy, double Rad, double R, double G, double B, double A = 1.0)
		{
			for (int Y = static_cast<int>(Cy - Rad) - 1; Y <= static_cast<int>(Cy + Rad) + 1; ++Y)
			{
				for (int X = static_cast<int>(Cx - Rad) - 1; X <= static_cast<int>(Cx + Rad) + 1; ++X)
				{
					const double D = std::hypot(X + 0.5 - Cx, Y + 0.5 - Cy);
					if (D <= Rad)
					{
						Put(X, Y, R, G, B, A);
					}
				}
			}
		}
		void Ring(double Cx, double Cy, double Rad, double R, double G, double B)
		{
			const int N = std::max(24, static_cast<int>(Rad * 7.0));
			for (int I = 0; I < N; ++I)
			{
				const double A = 6.283185307179586 * I / N;
				Put(static_cast<int>(Cx + Rad * std::cos(A)), static_cast<int>(Cy + Rad * std::sin(A)), R, G, B);
			}
		}
		void Box(double Cx, double Cy, double HalfLong, double HalfWide, double R, double G, double B)
		{
			for (int Y = static_cast<int>(Cy - HalfWide); Y <= static_cast<int>(Cy + HalfWide); ++Y)
			{
				for (int X = static_cast<int>(Cx - HalfLong); X <= static_cast<int>(Cx + HalfLong); ++X)
				{
					Put(X, Y, R, G, B);
				}
			}
		}
		bool WritePpm(const std::string& Path) const
		{
			FILE* F = std::fopen(Path.c_str(), "wb");
			if (!F)
			{
				return false;
			}
			std::fprintf(F, "P6\n%d %d\n255\n", W, H);
			std::vector<unsigned char> Bytes(Rgb.size());
			for (size_t I = 0; I < Rgb.size(); ++I)
			{
				const double V = std::clamp(Rgb[I], 0.0, 1.0);
				Bytes[I] = static_cast<unsigned char>(std::lround(255.0 * std::pow(V, 1.0 / 2.2)));
			}
			std::fwrite(Bytes.data(), 1, Bytes.size(), F);
			std::fclose(F);
			return true;
		}
	};

	void NightPicture(const JsonRecipe& Book, const std::string& OutDir)
	{
		std::printf("\nA night fight from above (fx-lamps-night)\n");
		// (our line: four battleships 2 km north of the eye, heading east; theirs: three heavy cruisers 7 km north and a
		// screen of four destroyers at 5.5 km; a sunk ship's oil 6.1 km off; at 3 s our two nearest fire their
		// broadsides, the cruisers theirs, the destroyers theirs; the picture is the frame 0.1 s after)
		const Vec Eye(0.0, 0.0, 25.0);
		std::vector<Ship> Ships;
		for (int I = 0; I < 4; ++I)
		{
			Ships.push_back(Battleship(Vec(-1500.0 + 600.0 * I, 2000.0, 0.0)));
		}
		for (int I = 0; I < 3; ++I)
		{
			Ships.push_back(Cruiser(Vec(-300.0 + 600.0 * I, 7000.0, 0.0)));
		}
		const double DdX[4] = { -1400.0, -900.0, 1900.0, 2400.0 };
		for (double X : DdX)
		{
			Ships.push_back(Destroyer(Vec(X, 5500.0, 0.0)));
		}
		const Vec OilAt(3000.0, 5300.0, 0.0);
		// (who fires at 3 s, and when: the two battleships nearest the eye, the cruisers, two destroyers)
		struct Firing
		{
			int ShipNo;
			double T;
		};
		const Firing Fire[] = { { 2, 3.0 }, { 3, 3.03 }, { 4, 2.96 }, { 5, 3.0 }, { 6, 3.05 }, { 7, 3.02 }, { 10, 2.98 } };
		const int PictureFrame = static_cast<int>(std::lround(3.1 * Fps));
		struct Panel
		{
			std::vector<Battle::Lamp> Lamps;
			std::vector<Shot> Fired;
			std::string Name;
		};
		Panel Panels[2];
		for (int Ruled = 0; Ruled < 2; ++Ruled)
		{
			Fight F;
			F.Open(Book, 41u, Eye, Vec(0.0, 4000.0, 0.0));
			F.Fx.OilFire(0.0, 61, OilAt, 150.0, "sunk", 0.0, 60.0);
			for (const Firing& Fi : Fire)
			{
				const Ship& S = Ships[static_cast<size_t>(Fi.ShipNo)];
				const Vec Toward = S.Mid.Y < 3000.0 ? Vec(S.Mid.X, 9000.0, 0.0) : Vec(S.Mid.X, 0.0, 0.0);
				// (today the sea scene fires a naval gun as howitzer at 180 mm and up, tank_gun below)
				const char* Today = std::string(S.Row) == "naval_light" ? "tank_gun" : "howitzer";
				Salvo(F.Shots, S, Fi.ShipNo, Fi.T, 0.07, Toward, Ruled == 1 ? nullptr : Today);
			}
			F.Order();
			if (Ruled == 1)
			{
				F.Fx.SetLampRule(Rule::Sea());
			}
			F.Room = F.Fx.LampsWanted();
			for (int Frame = 0; Frame <= PictureFrame; ++Frame)
			{
				F.Step();
			}
			const double T = F.Now() - 1.0 / Fps;
			for (int I = 0; I < F.NLit; ++I)
			{
				Panels[Ruled].Lamps.push_back(F.Lit[I]);
			}
			for (const Shot& S : F.Shots)
			{
				if (S.T <= T && S.T > T - 0.3)
				{
					Panels[Ruled].Fired.push_back(S);
				}
			}
			Panels[Ruled].Name = Ruled == 0 ? "today: 2 lamps, strongest first, howitzer and tank_gun rows" : "sea rule: 3 lamps by the eye, a salvo one lamp, naval rows";
			std::printf("  %s, %.2f s:\n", Panels[Ruled].Name.c_str(), T);
			for (size_t I = 0; I < Panels[Ruled].Lamps.size(); ++I)
			{
				const Battle::Lamp& L = Panels[Ruled].Lamps[I];
				std::printf("    lamp %zu at (%.0f, %.0f, %.0f), %.0f cd, reach %.0f m, %.2f km from the eye\n", I, L.At.X, L.At.Y, L.At.Z, L.Strength * Book.Get("game.lamp_candela_at_full", 2000.0), L.Radius, Far(L.At, Eye) / 1000.0);
			}
		}
		// (the picture: x from -3 km to 5 km, y from -0.5 km to 8 km, 12.5 m a pixel, two panels side by side; the water
		// lit by the chosen lamps as Unreal's inverse-square lamp with its window lights it, on a log scale from 0.02 lux
		// to 20; each lamp's reach a yellow ring; every gun fired in the last 0.3 s an orange dot; hulls grey; the oil
		// brown with its fire; the eye a white mark)
		const double X0 = -3000.0, Y0 = -500.0, Px = 12.5;
		const int PW = 640, PH = 680, Gap = 16, Top = 40;
		Picture Pic;
		Pic.Make(PW * 2 + Gap, PH + Top);
		const double CdAtFull = Book.Get("game.lamp_candela_at_full", 2000.0);
		std::string Labels;
		for (int Side = 0; Side < 2; ++Side)
		{
			const int Ox = Side * (PW + Gap);
			const Panel& P = Panels[Side];
			for (int Y = 0; Y < PH; ++Y)
			{
				for (int X = 0; X < PW; ++X)
				{
					const double Wx = X0 + (X + 0.5) * Px, Wy = Y0 + (PH - Y - 0.5) * Px;
					double Lux = 0.0;
					for (const Battle::Lamp& L : P.Lamps)
					{
						const double Dx = Wx - L.At.X, Dy = Wy - L.At.Y, Dz = L.At.Z;
						const double D2 = Dx * Dx + Dy * Dy + Dz * Dz;
						const double Q = D2 / std::max(1.0, L.Radius * L.Radius);
						const double Window = Q < 1.0 ? (1.0 - Q * Q) * (1.0 - Q * Q) : 0.0;
						Lux += L.Strength * CdAtFull / std::max(1.0, D2) * Window;
					}
					const double Lit = std::clamp(std::log10(std::max(Lux, 1e-9) / 0.02) / 3.0, 0.0, 1.0);
					Pic.Put(Ox + X, Top + Y, 0.006 + 0.75 * Lit, 0.012 + 0.6 * Lit, 0.035 + 0.3 * Lit);
				}
			}
			auto ToPx = [&](const Vec& W, double& Sx, double& Sy)
			{
				Sx = Ox + (W.X - X0) / Px;
				Sy = Top + PH - (W.Y - Y0) / Px;
			};
			double Sx = 0.0, Sy = 0.0;
			ToPx(OilAt, Sx, Sy);
			Pic.Disc(Sx, Sy, 150.0 / Px, 0.05, 0.03, 0.02, 0.9);
			Pic.Disc(Sx, Sy, 3.0, 0.9, 0.35, 0.05);
			for (const Ship& S : Ships)
			{
				ToPx(S.Mid, Sx, Sy);
				const double Wide = S.Long > 200.0 ? 33.0 : S.Long > 150.0 ? 20.0 : 11.0;
				Pic.Box(Sx, Sy, 0.5 * S.Long / Px, std::max(1.0, 0.5 * Wide / Px), 0.55, 0.57, 0.6);
			}
			for (const Shot& S : P.Fired)
			{
				ToPx(S.At, Sx, Sy);
				Pic.Disc(Sx, Sy, 2.2, 1.0, 0.55, 0.15);
			}
			for (size_t I = 0; I < P.Lamps.size(); ++I)
			{
				const Battle::Lamp& L = P.Lamps[I];
				ToPx(L.At, Sx, Sy);
				Pic.Ring(Sx, Sy, std::max(2.0, L.Radius / Px), 1.0, 0.9, 0.3);
				Pic.Disc(Sx, Sy, 2.5, 1.0, 1.0, 0.7);
				// (its number by it, and what it is in the panel's list up top, so lamps side by side stay readable)
				char Line[160];
				std::snprintf(Line, sizeof(Line), "%.0f %.0f %zu\n", Sx + 4.0, Sy - 16.0, I + 1);
				Labels += Line;
				std::snprintf(Line, sizeof(Line), "%d %zu lamp %zu: %.0f cd, reach %.0f m, %.1f km from the eye\n", Ox + 8, 50 + 14 * I, I + 1, L.Strength * CdAtFull, L.Radius, Far(L.At, Eye) / 1000.0);
				Labels += Line;
			}
			ToPx(Eye, Sx, Sy);
			for (int K = 0; K < 9; ++K)
			{
				Pic.Disc(Sx, Sy - K * 0.9, 4.5 - 0.5 * K, 1.0, 1.0, 1.0);
			}
			char Line[200];
			std::snprintf(Line, sizeof(Line), "%.0f %.0f eye\n", Sx + 8.0, Sy - 6.0);
			Labels += Line;
			ToPx(OilAt, Sx, Sy);
			std::snprintf(Line, sizeof(Line), "%.0f %.0f oil burning, %.1f km\n", Sx + 10.0, Sy + 6.0, Far(OilAt, Eye) / 1000.0);
			Labels += Line;
			std::snprintf(Line, sizeof(Line), "%d %d %s\n", Ox + 8, 12, P.Name.c_str());
			Labels += Line;
		}
		const std::string Ppm = OutDir + "/fx-lamps-night.ppm";
		const bool bWrote = Pic.WritePpm(Ppm);
		FILE* Out = std::fopen((OutDir + "/fx-lamps-night-labels.txt").c_str(), "w");
		if (Out)
		{
			std::fputs(Labels.c_str(), Out);
			std::fclose(Out);
		}
		Check(bWrote && Out != nullptr, "the picture written", Ppm);
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "fx_lamps_test <battle-fx.json> <out folder>\n");
		return 2;
	}
	JsonRecipe Book;
	if (!FxLamps::LoadRecipe(Argv[1], Book))
	{
		return 2;
	}
	std::printf("The flashes' lamps at sea: the lamp rule and the naval rows (%s)\n", Argv[1]);
	DefaultAsBefore(Book);
	BroadsideBeatsOil(Book);
	SalvoIsOneLamp(Book);
	RoomAndCap(Book);
	Rows(Book);
	Cost(Book);
	NightPicture(Book, Argv[2]);
	std::printf("\n%d checks, %d failed\n%s\n", Checks, Failed, Failed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
	return Failed == 0 ? 0 : 1;
}
