// The flashes' lamps at sea (the sea's night fight, 25 November; look bible S7, "gun flashes light the hulls"): the
// effects kit's lamp rule (TheaterFx::Battle::SetLampRule) and its naval rows (muzzle.naval_heavy, naval_medium,
// naval_light), on scripted fights with the game's own recipe and the kit's own code, no engine.
//
//   - the default rule hands every lamp out as before (and run.sh holds fx_lamps_dump to the kit before the rule);
//   - with the sea's rule a battleship's broadside 2 km off beats a slick burning 6 km off; ranked by the eye, a near
//     destroyer beats a far battleship (and by strength alone, the control, the battleship wins); a broadside behind
//     the eye lights nothing it can see; a salvo of turrets within 0.25 s and 60 m (and a battleship's 150 m) is one
//     lamp, its strength their sum held to twice its strongest and its reach grown to cover them; two ships 500 m
//     apart are never one, even when their nearest turrets fire last (and the control, a rule of 360 m, joins them);
//     the lamps the same to the bit whatever the order of the Muzzle calls; 3 lamps when asked, never more than 4;
//     every lamp a number;
//   - each row's candelas at night and by day, its reach against its band's hulls (the table), and the light on her
//     side plating along her length at night and by day (and the control: the lamp as first built, by the fireball,
//     burnt a spot by the gun and left her ends 7 stops down);
//   - what the rule costs a frame with 40 ships firing (the fastest of 9 calls a frame, a time line: never in the
//     compilers' comparison);
//   - a picture of a night fight, from above and along each lit hull's side: today's lamps, the sea rule's as first
//     built, and as now (fx-lamps-night.png, or .ppm).
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
	bool bTimeFailed = false; // (the cost: printed as a time line and kept out of the count, so a busy machine never makes the compilers' texts differ)

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
		double Beam = 33.0, Freeboard = 8.0; // (for the light on her side: her side plating from the waterline up)
		const char* Name = "battleship";
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
		S.Beam = 20.0;
		S.Freeboard = 6.5;
		S.Name = "cruiser";
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
		S.Beam = 11.0;
		S.Freeboard = 4.5;
		S.Name = "destroyer";
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

	/** A ship's salvo toward Toward from T: each turret Gap seconds after the one before, fore to aft (bAftFirst: aft to
	 *  fore, her forward turret last). Row: the ship's own (nullptr) or another (today's: howitzer at 180 mm and up,
	 *  tank_gun below). */
	void Salvo(std::vector<Shot>& Into, const Ship& S, int ShipNo, double T, double Gap, const Vec& Toward, const char* Row = nullptr, bool bAftFirst = false)
	{
		const Vec Side = (Toward - S.Mid);
		const Vec Level = Vec(Side.X, Side.Y, 0.0).Unit(Vec(0.0, 1.0, 0.0));
		const Vec Facing = (Level + Vec(0.0, 0.0, 0.12)).Unit();
		const double Spread = S.Spread > 0.0 ? S.Spread : 0.64 * S.Long;
		for (int K = 0; K < S.Turrets; ++K)
		{
			const double Along = S.Turrets > 1 ? 0.5 * Spread - Spread * K / (S.Turrets - 1) : 0.0;
			Shot One;
			One.T = T + Gap * (bAftFirst ? S.Turrets - 1 - K : K);
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
		double LampsUs = 0.0; // (the last frame's Lamps, in microseconds: the fastest of Reps calls)
		int Reps = 1; // (the cost: Lamps called this many times a frame at the same moment, the first call's lamps kept)

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
			double Best = 1.0e18;
			for (int Rep = 0; Rep < std::max(1, Reps); ++Rep)
			{
				// (again at the same moment the kit hands out the same lamps and moves no steady lamp's ramp: its clock,
				// LampsAt, only goes on with time. The first call's lamps are the frame's)
				Battle::Lamp Again[16];
				const auto Start = std::chrono::steady_clock::now();
				const int Got = Fx.Lamps(T, Rep == 0 ? Lit : Again, Room, Day);
				const auto End = std::chrono::steady_clock::now();
				if (Rep == 0)
				{
					NLit = Got;
				}
				Best = std::min(Best, std::chrono::duration<double, std::micro>(End - Start).count());
			}
			LampsUs = Best;
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

	/** Where a lamp lies along a ship's line (her Way) from her middle, in metres. */
	double AlongOf(const Battle::Lamp& L, const Ship& S)
	{
		return (L.At - S.Mid).Dot(S.Way);
	}

	/** Whether a lamp lies by her: within half her length (and a twentieth more) of her middle along her line. */
	bool LampByHer(const Battle::Lamp& L, const Ship& S)
	{
		return std::fabs(AlongOf(L, S)) <= 0.55 * S.Long;
	}

	/** The light (lux) the lamps put on a surface at P facing N, as Unreal's lamp with its inverse-square falloff and its
	 *  attenuation window gives it (the strengths at CdAtFull candelas a unit). */
	double LuxAt(const Battle::Lamp* Lamps, int N, double CdAtFull, const Vec& P, const Vec& Facing)
	{
		double Lux = 0.0;
		for (int I = 0; I < N; ++I)
		{
			const Battle::Lamp& L = Lamps[I];
			const Vec To = L.At - P;
			const double D2 = To.Dot(To);
			const double D = std::sqrt(D2);
			if (!(D > 1e-6))
			{
				continue;
			}
			const double Cos = To.Dot(Facing) / D;
			if (Cos <= 0.0)
			{
				continue;
			}
			const double Q = D2 / std::max(1.0, L.Radius * L.Radius);
			const double Window = Q < 1.0 ? (1.0 - Q * Q) * (1.0 - Q * Q) : 0.0;
			Lux += L.Strength * CdAtFull * Cos / std::max(1.0, D2) * Window;
		}
		return Lux;
	}

	/** The light along a ship's side plating, half her freeboard up, on the side facing Side (level), bow to stern. */
	struct SideSeen
	{
		std::vector<double> Lux;
		double Peak = 0.0, Ends = 0.0, Stops = 0.0, WithinOne = 0.0; // (WithinOne: the metres of her within a stop of the peak)
	};

	SideSeen SideOf(const Battle::Lamp* Lamps, int N, double CdAtFull, const Ship& S, const Vec& Side, int Points = 97)
	{
		SideSeen Out;
		const Vec Normal = Vec(Side.X, Side.Y, 0.0).Unit(Vec(0.0, 1.0, 0.0));
		for (int K = 0; K < Points; ++K)
		{
			const double Along = S.Long * (0.5 - static_cast<double>(K) / (Points - 1));
			const Vec P = S.Mid + S.Way * Along + Normal * (0.5 * S.Beam) + Vec(0.0, 0.0, 0.5 * S.Freeboard);
			Out.Lux.push_back(LuxAt(Lamps, N, CdAtFull, P, Normal));
		}
		int Within = 0;
		for (double E : Out.Lux)
		{
			Out.Peak = std::max(Out.Peak, E);
		}
		for (double E : Out.Lux)
		{
			Within += E >= 0.5 * Out.Peak && Out.Peak > 0.0 ? 1 : 0;
		}
		Out.Ends = std::min(Out.Lux.front(), Out.Lux.back());
		Out.Stops = Out.Peak > 0.0 ? std::log2(Out.Peak / std::max(Out.Ends, 1e-12)) : 0.0;
		Out.WithinOne = S.Long * Within / static_cast<double>(Points);
		return Out;
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
	// 2b. By the eye, not by strength: a near destroyer against a far battleship; a broadside behind the eye

	void EyeOverStrength(const JsonRecipe& Book)
	{
		std::printf("\nRanked by the eye: a destroyer 1.5 km off against a battleship 8.5 km off, the slick 6 km off (night, 2 lamps)\n");
		// (the eye 20 m up at the origin looking north; a destroyer 1.5 km north and a battleship 8.5 km off fire together
		// at 2 s, all their guns in one frame; the slick burns 6 km off since long before and keeps its own lamp. One lamp
		// is left for a flash: by strength the battleship's (72 a turret against 16.5 a mount), by the eye the destroyer's)
		const Vec Eye(0.0, 0.0, 20.0);
		const Vec OilAt(2000.0, 5657.0, 0.0);
		const Ship Near = Destroyer(Vec(0.0, 1500.0, 0.0)), FarOff = Battleship(Vec(-500.0, 8500.0, 0.0));
		// (the destroyer's lamp is out after 0.1 s, the battleship's after 0.2: what counts is who holds the lamp while
		// both burn, so the frames from the first lit to the last of whichever goes out first)
		int NearLit[2] = { 0, 0 }, FarLit[2] = { 0, 0 }, FirstLit[2] = { -1, -1 }, NearFirst[2] = { -1, -1 }, NearLast[2] = { -1, -1 }, FarFirst[2] = { -1, -1 };
		for (int ByEye = 1; ByEye >= 0; --ByEye)
		{
			Fight F;
			F.Open(Book, 43u, Eye, Vec(0.0, 4000.0, 0.0));
			F.Fx.OilFire(0.0, 41, OilAt, 150.0, "sunk", 0.0, 60.0);
			Salvo(F.Shots, Near, 0, 2.0, 0.0, Vec(0.0, 12000.0, 0.0));
			Salvo(F.Shots, FarOff, 1, 2.0, 0.0, Vec(-500.0, 0.0, 0.0));
			F.Order();
			Rule R = Rule::Sea();
			R.Most = 2;
			R.bByEye = ByEye == 1; // (the control: the same rule ranking by strength alone)
			F.Fx.SetLampRule(R);
			F.Room = 2;
			for (int Frame = 0; Frame < static_cast<int>(2.4 * Fps); ++Frame)
			{
				F.Step();
				if (F.Now() - 1.0 / Fps < 2.0)
				{
					continue;
				}
				int ByNear = 0, ByFar = 0;
				for (int I = 0; I < F.NLit; ++I)
				{
					ByNear += LampByHer(F.Lit[I], Near) && Far(F.Lit[I].At, Near.Mid) < 400.0 ? 1 : 0;
					ByFar += LampByHer(F.Lit[I], FarOff) && Far(F.Lit[I].At, FarOff.Mid) < 400.0 ? 1 : 0;
				}
				FirstLit[ByEye] = FirstLit[ByEye] < 0 && ByNear + ByFar > 0 ? Frame : FirstLit[ByEye];
				NearFirst[ByEye] = NearFirst[ByEye] < 0 && ByNear > 0 ? Frame : NearFirst[ByEye];
				NearLast[ByEye] = ByNear > 0 ? Frame : NearLast[ByEye];
				FarFirst[ByEye] = FarFirst[ByEye] < 0 && ByFar > 0 ? Frame : FarFirst[ByEye];
				NearLit[ByEye] += ByNear > 0 ? 1 : 0;
				FarLit[ByEye] += ByFar > 0 ? 1 : 0;
			}
		}
		Check(FirstLit[1] >= 0 && NearFirst[1] == FirstLit[1] && NearLit[1] == NearLast[1] - NearFirst[1] + 1 && FarFirst[1] > NearLast[1], "by the eye: the near destroyer's lamp while hers burns", Say("the destroyer lit %.0f frames from the first; the battleship %.0f frames, only once hers was out", NearLit[1], FarLit[1]));
		Check(FirstLit[0] >= 0 && FarFirst[0] == FirstLit[0] && NearLit[0] == 0, "  control, the same rule by strength alone: the battleship's", Say("the battleship lit %.0f frames from the first, the destroyer %.0f", FarLit[0], NearLit[0]));
	}

	void BehindTheEye(const JsonRecipe& Book)
	{
		std::printf("\nA broadside behind the eye (1 km) against one in view (2 km), 1 lamp\n");
		// (the eye 20 m up looking north through a 55 degree lens 16:9; a battleship 1 km behind it fires south, another
		// 2 km ahead fires north, at once. The one behind is nearer, so it sends four times the light, but its lamp's
		// sphere lies wholly out of the view: it can light nothing on the screen)
		const Vec Eye(0.0, 0.0, 20.0);
		const Ship Behind = Battleship(Vec(0.0, -1000.0, 0.0)), Ahead = Battleship(Vec(0.0, 2000.0, 0.0));
		int AheadLit[2] = { 0, 0 }, BehindLit[2] = { 0, 0 }, Frames[2] = { 0, 0 };
		for (int Known = 1; Known >= 0; --Known)
		{
			Fight F;
			F.Open(Book, 47u, Eye, Vec(0.0, 4000.0, 0.0));
			if (Known == 1)
			{
				F.View.HalfAcross = std::tan(0.5 * 55.0 * 3.14159265358979323846 / 180.0);
				F.View.HalfUp = F.View.HalfAcross * 9.0 / 16.0;
			}
			Salvo(F.Shots, Behind, 0, 2.0, 0.0, Vec(0.0, -12000.0, 0.0));
			Salvo(F.Shots, Ahead, 1, 2.0, 0.0, Vec(0.0, 12000.0, 0.0));
			F.Order();
			Rule R = Rule::Sea();
			R.Most = 1;
			F.Fx.SetLampRule(R);
			F.Room = 1;
			for (int Frame = 0; Frame < static_cast<int>(2.4 * Fps); ++Frame)
			{
				F.Step();
				if (F.NLit == 0)
				{
					continue;
				}
				++Frames[Known];
				AheadLit[Known] += Far(F.Lit[0].At, Ahead.Mid) < 400.0 ? 1 : 0;
				BehindLit[Known] += Far(F.Lit[0].At, Behind.Mid) < 400.0 ? 1 : 0;
			}
		}
		Check(Frames[1] > 0 && AheadLit[1] == Frames[1], "the view known: the broadside in view takes the lamp", Say("the one ahead lit %.0f of %.0f frames, the one behind %.0f", AheadLit[1], Frames[1], BehindLit[1]));
		Check(Frames[0] > 0 && BehindLit[0] == Frames[0], "  control, the view's width not given: the nearer, behind", Say("the one behind lit %.0f of %.0f frames, the one ahead %.0f", BehindLit[0], Frames[0], AheadLit[0]));
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
		// (two battleships in line ahead 500 m apart, centre to centre, all their turrets inside one salvo's time: fore to
		// aft both; and then the leader aft to fore and the next fore to aft, so the two turrets nearest each other, 346 m
		// apart, fire last and are the strongest flashes alive, the case a merge too wide would join. The control: the
		// same shots with a rule of 360 m must be seen joined, so the check can see a join)
		{
			const Ship P = Battleship(Vec(-250.0, 2500.0, 0.0)), Q = Battleship(Vec(250.0, 2500.0, 0.0));
			const double RowReach = Book.Get("muzzle.naval_heavy.lamp_reach_m", 240.0);
			for (int Case = 0; Case < 3; ++Case)
			{
				Fight F;
				F.Open(Book, 17u, Eye, Vec(0.0, 2500.0, 0.0));
				const bool bFacing = Case >= 1;
				Salvo(F.Shots, P, 0, 2.0, 0.05, Vec(0.0, 15000.0, 0.0), nullptr, bFacing);
				Salvo(F.Shots, Q, 1, 2.02, 0.05, Vec(0.0, 15000.0, 0.0));
				F.Order();
				Rule R = Rule::Sea();
				if (Case == 2)
				{
					R.MergeM = 360.0;
				}
				F.Fx.SetLampRule(R);
				F.Room = 3;
				int Both = 0, Between = 0, Lamps = 0, Wide = 0;
				double Widest = 0.0;
				for (int Frame = 0; Frame < static_cast<int>(3.0 * Fps); ++Frame)
				{
					F.Step();
					Both += LampsBy(F, P, 40.0) >= 1 && LampsBy(F, Q, 40.0) >= 1 ? 1 : 0;
					for (int I = 0; I < F.NLit; ++I)
					{
						++Lamps;
						Between += !LampByHer(F.Lit[I], P) && !LampByHer(F.Lit[I], Q) ? 1 : 0;
						Widest = std::max(Widest, F.Lit[I].Radius);
						Wide += F.Lit[I].Radius > R.MergeM + RowReach + 1e-6 ? 1 : 0;
					}
				}
				if (Case == 0)
				{
					Check(Both > 0 && Between == 0 && Wide == 0, "two battleships 500 m apart firing fore to aft: a lamp by each", Say("a lamp by each in %.0f frames, %.0f of %.0f lamps between them; reach at most %.0f m", Both, Between, Lamps, Widest));
				}
				else if (Case == 1)
				{
					Check(Both > 0 && Between == 0 && Wide == 0, "  ...their nearest turrets (346 m) firing last: still two", Say("a lamp by each in %.0f frames, %.0f lamps between them; reach at most %.0f m (200 + %.0f)", Both, Between, Widest, RowReach));
				}
				else
				{
					Check(Between > 0, "  control, the same shots under a rule of 360 m: seen joined", Say("%.0f of %.0f lamps lie between the two ships", Between, Lamps));
				}
			}
		}
		// (and so on many pairs: two ships of any band 500 m apart, headings, moments and firing orders at random, never
		// one lamp; the control, a rule of 360 m on the same pairs, joins some)
		{
			int Pairs = 0, Joined[2] = { 0, 0 };
			for (int Wide = 0; Wide < 2; ++Wide)
			{
				Mix M;
				M.S = 2024;
				for (int Pair = 0; Pair < 60; ++Pair)
				{
					const double Kind = M.U();
					const double Heading = M.U() * 6.283185307179586;
					const double Lag = M.U() * 0.3;
					const Vec Way(std::cos(Heading), std::sin(Heading), 0.0);
					const double MidX = M.U() * 4000.0 - 2000.0;
					const double MidY = 2000.0 + M.U() * 3000.0;
					const double OrderP = M.U();
					const double OrderQ = M.U();
					const Vec Mid(MidX, MidY, 0.0);
					Ship P = Kind < 0.34 ? Battleship(Mid) : Kind < 0.67 ? Cruiser(Mid) : Destroyer(Mid);
					P.Way = Way;
					Ship Q = P;
					Q.Mid = Mid + Way * 500.0;
					Fight F;
					F.Open(Book, 100u + static_cast<uint32_t>(Pair), Eye, Mid);
					Salvo(F.Shots, P, 0, 1.0, 0.06, Mid + Vec(-Way.Y, Way.X, 0.0) * 9000.0, nullptr, OrderP < 0.5);
					Salvo(F.Shots, Q, 1, 1.0 + Lag, 0.06, Mid + Vec(-Way.Y, Way.X, 0.0) * 9000.0, nullptr, OrderQ < 0.5);
					F.Order();
					Rule R = Rule::Sea();
					if (Wide == 1)
					{
						R.MergeM = 360.0;
					}
					F.Fx.SetLampRule(R);
					F.Room = 3;
					for (int Frame = 0; Frame < static_cast<int>(2.0 * Fps); ++Frame)
					{
						F.Step();
						for (int I = 0; I < F.NLit; ++I)
						{
							Joined[Wide] += !LampByHer(F.Lit[I], P) && !LampByHer(F.Lit[I], Q) ? 1 : 0;
						}
					}
					Pairs += Wide == 0 ? 1 : 0;
				}
			}
			Check(Joined[0] == 0, "60 pairs 500 m apart, any band, heading, lag and order: never joined", Say("%.0f pairs, %.0f lamps lying between two ships", Pairs, Joined[0]));
			Check(Joined[1] > 0, "  control, the same pairs under a rule of 360 m: some joined", Say("%.0f lamps lying between two ships", Joined[1]));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// 3b. A salvo's lamp: the sum held, the reach grown, where it hangs; the same whatever the order of the calls

	void SalvoLamp(const JsonRecipe& Book)
	{
		std::printf("\nA salvo's lamp: strength, reach and place (a battleship's 3 turrets in one frame, 154 m)\n");
		const Vec Eye(0.0, 0.0, 20.0);
		const Ship S = Battleship(Vec(0.0, 3000.0, 0.0));
		const Vec Toward(0.0, 15000.0, 0.0);
		const double CdAtFull = Book.Get("game.lamp_candela_at_full", 2000.0);
		const std::string Row = std::string("muzzle.") + S.Row + ".";
		const double One = Book.Get(Row + "lamp_cd", 0.0) / CdAtFull;
		const double Reach = Book.Get(Row + "lamp_reach_m", 0.0);
		std::vector<Shot> Shots;
		Salvo(Shots, S, 0, 2.0, 0.0, Toward);
		// (where each turret's lamp hangs: half the fireball on from the muzzle along the bore, lamp_out_m on along its
		// level way, lamp_up_m up; the turrets' lamps alike, so the merged one hangs as far from the turrets' middle)
		const Vec Fw = Shots[0].Facing.Unit();
		const double Ball = std::max(Book.Get(Row + "fireball_m", 0.0), std::min(20.0, Book.Get(Row + "flame_m", 0.3)));
		const Vec Hang = Fw * (0.5 * Ball) + Vec(Fw.X, Fw.Y, 0.0).Unit() * Book.Get(Row + "lamp_out_m", 0.0) + Vec(0.0, 0.0, Book.Get(Row + "lamp_up_m", 0.0));
		Vec Middle;
		for (const Shot& One_ : Shots)
		{
			Middle = Middle + One_.At * (1.0 / static_cast<double>(Shots.size()));
		}
		double Widest = 0.0;
		for (const Shot& One_ : Shots)
		{
			Widest = std::max(Widest, Far(One_.At, Middle));
		}
		Battle::Lamp Got[2];
		for (int Case = 0; Case < 2; ++Case)
		{
			Fight F;
			F.Open(Book, 13u, Eye, S.Mid);
			F.Shots = Shots;
			F.Order();
			Rule R = Rule::Sea();
			R.MergeMost = Case == 0 ? 2.0 : 16.0;
			F.Fx.SetLampRule(R);
			F.Room = 3;
			for (int Frame = 0; Frame < static_cast<int>(3.0 * Fps) && F.NLit == 0; ++Frame)
			{
				F.Step();
			}
			Got[Case] = F.NLit == 1 ? F.Lit[0] : Battle::Lamp();
		}
		Check(std::fabs(Got[0].Strength - 2.0 * One) <= 1e-12 * One && Got[0].Strength <= 3.0 * One, "three turrets: their sum held to MergeMost (2) times one turret's", Say("%.4f times one turret's (%.0f cd, the sum would be %.0f)", Got[0].Strength / One, Got[0].Strength * CdAtFull, 3.0 * One * CdAtFull));
		Check(std::fabs(Got[1].Strength - 3.0 * One) <= 1e-12 * One, "  ...and under MergeMost 16 the sum itself", Say("%.4f times one turret's", Got[1].Strength / One));
		Check(std::fabs(Got[0].Radius - (Widest + Reach)) <= 1e-6, "its reach: the farthest turret's lamp from the middle plus its own", Say("%.3f m (%.3f + %.0f)", Got[0].Radius, Widest, Reach));
		const Vec Want = Middle + Hang;
		Check(Far(Got[0].At, Want) <= 1e-6, "hung lamp_out_m out along the bore, lamp_up_m up, from the fireball's middle", Say("%.6f m from (%.1f, %.1f, %.1f)", Far(Got[0].At, Want), Want.X, Want.Y, Want.Z));
	}

	void OrderOfCalls(const JsonRecipe& Book)
	{
		std::printf("\nThe same flashes in any order of calls: the same lamps to the bit\n");
		// (A: three heavy turrets in one frame at x = 0, 150 and 300 m, so which two share a lamp is a tie the order of the
		// calls once broke; B: a battleship's three turrets in one frame, the merged lamp's middle summed in some order;
		// C: two ships alike, 1 km apart, 1 lamp: which ship is lit was the first called. Each in all its orders)
		const Vec Eye(0.0, 0.0, 20.0);
		const Vec Up(0.0, 1.0, 0.12);
		for (int Case = 0; Case < 3; ++Case)
		{
			std::vector<Shot> Base;
			if (Case == 0)
			{
				for (int K = 0; K < 3; ++K)
				{
					Base.push_back(Shot{ 2.0, Vec(150.0 * K, 3000.0, 12.0), Up, "naval_heavy", K });
				}
			}
			else if (Case == 1)
			{
				Salvo(Base, Battleship(Vec(30.0, 3000.0, 0.0)), 0, 2.0, 0.0, Vec(30.0, 15000.0, 0.0));
			}
			else
			{
				Base.push_back(Shot{ 2.0, Vec(-500.0, 3000.0, 12.0), Up, "naval_heavy", 0 });
				Base.push_back(Shot{ 2.0, Vec(500.0, 3000.0, 12.0), Up, "naval_heavy", 1 });
				Base.push_back(Shot{ 2.0, Vec(0.0, 6000.0, 12.0), Up, "naval_light", 2 });
			}
			std::vector<int> Order(Base.size());
			for (size_t I = 0; I < Order.size(); ++I)
			{
				Order[I] = static_cast<int>(I);
			}
			std::vector<std::vector<Battle::Lamp>> First;
			int Orders = 0, Same = 0;
			do
			{
				Fight F;
				F.Open(Book, 53u, Eye, Vec(0.0, 3000.0, 0.0));
				for (int I : Order)
				{
					F.Shots.push_back(Base[static_cast<size_t>(I)]);
				}
				F.Order();
				Rule R = Rule::Sea();
				R.Most = Case == 2 ? 1 : 3;
				F.Fx.SetLampRule(R);
				F.Room = R.Most;
				std::vector<std::vector<Battle::Lamp>> Seen;
				for (int Frame = 0; Frame < static_cast<int>(2.5 * Fps); ++Frame)
				{
					F.Step();
					Seen.push_back(std::vector<Battle::Lamp>(F.Lit, F.Lit + F.NLit));
				}
				bool bSame = true;
				if (Orders == 0)
				{
					First = Seen;
				}
				for (size_t K = 0; K < Seen.size() && bSame; ++K)
				{
					bSame = Seen[K].size() == First[K].size();
					for (size_t I = 0; I < Seen[K].size() && bSame; ++I)
					{
						const Battle::Lamp& X = Seen[K][I];
						const Battle::Lamp& Y = First[K][I];
						bSame = X.At.X == Y.At.X && X.At.Y == Y.At.Y && X.At.Z == Y.At.Z && X.Strength == Y.Strength && X.Radius == Y.Radius && X.Col.R == Y.Col.R && X.Col.G == Y.Col.G && X.Col.B == Y.Col.B;
					}
				}
				++Orders;
				Same += bSame ? 1 : 0;
			} while (std::next_permutation(Order.begin(), Order.end()));
			const char* Name = Case == 0 ? "three turrets 150 m apart, a tie: every order" : Case == 1 ? "a battleship's salvo merged: every order, its middle to the bit" : "two ships alike 1 km apart, 1 lamp: every order";
			Check(Same == Orders && Orders == 6, Name, Say("%.0f of %.0f orders the same lamps, every frame, to the bit", Same, Orders));
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
			{ "naval_light", 100.0, 120.0, true, "destroyers 110-119 m" },
		};
		std::printf("  %-13s %10s %8s %10s %10s %7s %13s  %s\n", "row", "cd night", "reach m", "cd dusk", "cd by day", "lit s", "hung out/up m", "its band's hulls");
		const double Share = Book.Get("game.day_lamp_share", 0.25);
		for (const Band& B : Bands)
		{
			const RowSeen S = SeeRow(Book, B.Row);
			const double Asked = Book.Get(std::string("muzzle.") + B.Row + ".lamp_cd", -1.0);
			const std::string Row = std::string("muzzle.") + B.Row + ".";
			std::printf("  %-13s %10.0f %8.0f %10.0f %10.0f %7.3f %7.0f / %3.0f  %s\n", B.Row, S.NightCd, S.Reach, S.DuskCd, S.DayCd, S.Held, Book.Get(Row + "lamp_out_m", 0.0), Book.Get(Row + "lamp_up_m", 0.0), B.Hulls);
			Check(std::fabs(S.NightCd - Asked) <= 1e-6 * Asked, (std::string(B.Row) + ": at night its own lamp_cd").c_str(), Say("%.1f cd (lamp_cd %.1f)", S.NightCd, Asked));
			if (B.bDay)
			{
				Check(std::fabs(S.DayCd - Share * Asked) <= 1e-6 * Asked, (std::string(B.Row) + ": by day game.day_lamp_share of it").c_str(), Say("%.1f cd (%.2f of %.0f)", S.DayCd, Share, Asked));
			}
			else
			{
				Check(S.DayCd == 0.0, (std::string(B.Row) + ": by day no lamp").c_str(), Say("%.1f cd", S.DayCd));
			}
			Check(S.Reach >= B.HullLo && S.Reach <= B.HullHi, (std::string(B.Row) + ": reach about its band's hull").c_str(), Say("%.0f m (%.0f to %.0f)", S.Reach, B.HullLo, B.HullHi));
			Check(S.Held > 0.03 && S.Held <= Book.Get(std::string("muzzle.") + B.Row + ".lamp_s", 0.0) + 1e-9, (std::string(B.Row) + ": lit while its fireball burns, lamp_s at most").c_str(), Say("%.3f s (lamp_s %.2f)", S.Held, Book.Get(std::string("muzzle.") + B.Row + ".lamp_s", 0.0)));
		}
		// (today's rows, as the sea scene fires them now, for the table: by strength, 2 frames)
		for (const char* Row : { "howitzer", "tank_gun" })
		{
			const RowSeen S = SeeRow(Book, Row);
			std::printf("  %-13s %10.1f %8.1f %10.1f %10.1f %7.3f %13s  (today's, for a naval gun)\n", Row, S.NightCd, S.Reach, S.DuskCd, S.DayCd, S.Held, "");
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// 5b. The light on her side: the hull lit along her length, at night and by day

	/** The recipe as first built (6bd95621): each naval lamp by its fireball, lamp_up_m over its middle and no lamp_out_m,
	 *  the cruiser's fireball 20 m. The control the side checks are held against. */
	JsonRecipe AsFirstBuilt(const JsonRecipe& Book)
	{
		JsonRecipe Was = Book;
		const char* Rows[3] = { "naval_heavy", "naval_medium", "naval_light" };
		const double Up[3] = { 6.0, 4.0, 3.0 };
		for (int I = 0; I < 3; ++I)
		{
			const std::string Row = std::string("muzzle.") + Rows[I] + ".";
			Was.Numbers.erase(Row + "lamp_out_m");
			Was.Numbers[Row + "lamp_up_m"] = Up[I];
		}
		Was.Numbers["muzzle.naval_medium.fireball_m"] = 20.0;
		return Was;
	}

	/** Her broadside alone at night or by day, all her turrets in one frame (as the sea scene fires a salvo), seen 2 km
	 *  off: the light on the side she fires from, the first frame drawn after it (the lamp at its fullest). */
	SideSeen HullAfterBroadside(const JsonRecipe& Book, const Ship& S, double Day, double* WaterUnder = nullptr)
	{
		Fight F;
		F.Open(Book, 59u, Vec(0.0, 0.0, 20.0), S.Mid);
		const Vec Toward = S.Mid + Vec(0.0, 12000.0, 0.0);
		Salvo(F.Shots, S, 0, 2.0, 0.0, Toward);
		F.Order();
		F.Fx.SetLampRule(Rule::Sea());
		F.Room = 3;
		F.Day = Day;
		for (int Frame = 0; Frame < static_cast<int>(3.0 * Fps) && F.NLit == 0; ++Frame)
		{
			F.Step();
		}
		const double CdAtFull = Book.Get("game.lamp_candela_at_full", 2000.0);
		if (WaterUnder)
		{
			*WaterUnder = F.NLit > 0 ? LuxAt(F.Lit, F.NLit, CdAtFull, Vec(F.Lit[0].At.X, F.Lit[0].At.Y, 0.0), Vec(0.0, 0.0, 1.0)) : 0.0;
		}
		return SideOf(F.Lit, F.NLit, CdAtFull, S, Toward - S.Mid);
	}

	void HullLit(const JsonRecipe& Book)
	{
		std::printf("\nThe light on her side plating, half her freeboard up, bow to stern, after her broadside (sea rule)\n");
		// (the land's calibration: 400 cd hung 16 m up, 1.5 lux at its foot, passed; 1,200 cd, 4.7 lux, was overlit. The
		// sea's sun is 7 lux (SeaBattle.cpp, LightTheBattle). So at night her side should stay under 4.7 lux by the gun and
		// within 3 stops of that at her ends; by day her lamp should lift her side by no more than a seventh of the sun)
		const JsonRecipe Was = AsFirstBuilt(Book);
		const Ship Ships[3] = { Battleship(Vec(0.0, 2000.0, 0.0)), Cruiser(Vec(0.0, 2000.0, 0.0)), Destroyer(Vec(0.0, 2000.0, 0.0)) };
		std::printf("  %-11s %-14s %9s %9s %7s %13s %12s\n", "ship", "lamp", "peak lux", "ends lux", "stops", "within 1 stop", "water under");
		for (const Ship& S : Ships)
		{
			double WaterNow = 0.0, WaterWas = 0.0;
			const SideSeen Night = HullAfterBroadside(Book, S, 0.0, &WaterNow);
			const SideSeen ByDay = HullAfterBroadside(Book, S, 1.0);
			const SideSeen First = HullAfterBroadside(Was, S, 0.0, &WaterWas);
			std::printf("  %-11s %-14s %9.2f %9.2f %7.1f %11.0f m %10.1f lux\n", S.Name, "hung out, now", Night.Peak, Night.Ends, Night.Stops, Night.WithinOne, WaterNow);
			std::printf("  %-11s %-14s %9.2f %9.2f %7.1f %11.0f m\n", "", "  by day", ByDay.Peak, ByDay.Ends, ByDay.Stops, ByDay.WithinOne);
			std::printf("  %-11s %-14s %9.2f %9.2f %7.1f %11.0f m %10.1f lux\n", "", "first built", First.Peak, First.Ends, First.Stops, First.WithinOne, WaterWas);
			const std::string Name = S.Name;
			Check(Night.Peak > 0.5 && Night.Peak <= 4.7 && Night.Stops <= 3.0, (Name + " at night: her side lit end to end, never overlit").c_str(), Say("%.2f lux by the gun, %.2f at her ends (%.1f stops); 4.7 overlit", Night.Peak, Night.Ends, Night.Stops));
			Check(Night.WithinOne >= 0.5 * S.Long, (Name + "  ...half her length or more within a stop of the peak").c_str(), Say("%.0f m of %.0f", Night.WithinOne, S.Long));
			Check(ByDay.Peak > 0.0 && ByDay.Peak <= 1.0, (Name + " by day: a lift of a seventh of the 7-lux sun at most").c_str(), Say("%.2f lux by the gun", ByDay.Peak));
			Check(WaterNow <= 4.7, (Name + "  ...and the water under her lamp never overlit").c_str(), Say("%.2f lux (as first built %.0f)", WaterNow, WaterWas));
			Check(First.Stops >= 5.0, (Name + "  control, the lamp as first built: a spot by the gun").c_str(), Say("%.1f lux by the gun, %.2f at her ends: %.1f stops", First.Peak, First.Ends, First.Stops));
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
			F.Reps = 9; // (each frame the fastest of 9 calls: what the work costs, not what else the machine was doing)
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
			std::printf("  time: %-22s %6.2f us a frame on the mean, %6.2f at the 99th hundredth, %6.2f the worst; all forty at once %6.2f us (the fastest of 9 calls a frame)\n", Ruled == 1 ? "the sea rule, 3 lamps" : "the default, 2 lamps", Mean, P99, Worst, AllAt);
			if (Ruled == 1)
			{
				// (a time line, out of the count: the compilers' texts are compared without it)
				const bool bOk = Mean < 50.0 && P99 < 50.0;
				bTimeFailed = bTimeFailed || !bOk;
				std::printf("  time: %s the sea rule's lamps a frame, 40 ships firing, well under 50 us: %.2f us on the mean, %.2f at the 99th hundredth\n", bOk ? "ok  " : "FAIL", Mean, P99);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// 7. The picture: a night fight from above, today's lamps beside the sea rule's

	struct Picture
	{
		int W = 0, H = 0;
		int ClipX0 = 0, ClipX1 = 1 << 30; // (a panel's own columns: nothing drawn for one spills into the next)
		std::vector<double> Rgb;
		void Make(int InW, int InH)
		{
			W = InW;
			H = InH;
			Rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3u, 0.0);
		}
		void Put(int X, int Y, double R, double G, double B, double A = 1.0)
		{
			if (X < 0 || Y < 0 || X >= W || Y >= H || X < ClipX0 || X >= ClipX1)
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
		std::printf("\nA night fight, from above and along each lit hull (fx-lamps-night)\n");
		// (our line: four battleships 2 km north of the eye, heading east; theirs: three heavy cruisers 7 km north and a
		// screen of four destroyers at 5.5 km; a sunk ship's oil 6.1 km off; at 3 s our two nearest fire their
		// broadsides, the cruisers theirs, the destroyers theirs; the picture is the frame drawn at 3.05 s. Three panels:
		// today's lamps (howitzer and tank_gun rows, 2 lamps by strength), the sea rule with the naval rows' lamps as
		// first built (by the fireball), and as now (hung out and up))
		const Vec Eye(0.0, 0.0, 25.0);
		const JsonRecipe Was = AsFirstBuilt(Book);
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
		struct Firing
		{
			int ShipNo;
			double T;
		};
		const Firing Fire[] = { { 2, 3.0 }, { 3, 3.03 }, { 4, 2.96 }, { 5, 3.0 }, { 6, 3.05 }, { 7, 3.02 }, { 10, 2.98 } };
		auto TowardOf = [](const Ship& S) { return S.Mid.Y < 3000.0 ? Vec(S.Mid.X, 9000.0, 0.0) : Vec(S.Mid.X, 0.0, 0.0); };
		const int PictureFrame = static_cast<int>(std::lround(3.05 * Fps));
		const int Strips[4] = { 2, 3, 5, 7 }; // (the hulls whose side is told: the two firing battleships (drawn too), a cruiser, a destroyer)
		struct Panel
		{
			std::vector<Battle::Lamp> Lamps;
			std::vector<Shot> Fired;
			std::string Name;
		};
		Panel Panels[3];
		const double CdAtFull = Book.Get("game.lamp_candela_at_full", 2000.0);
		for (int Kind = 0; Kind < 3; ++Kind)
		{
			Fight F;
			F.Open(Kind == 1 ? Was : Book, 41u, Eye, Vec(0.0, 4000.0, 0.0));
			F.Fx.OilFire(0.0, 61, OilAt, 150.0, "sunk", 0.0, 60.0);
			for (const Firing& Fi : Fire)
			{
				const Ship& S = Ships[static_cast<size_t>(Fi.ShipNo)];
				// (today the sea scene fires a naval gun as howitzer at 180 mm and up, tank_gun below)
				const char* Today = std::string(S.Row) == "naval_light" ? "tank_gun" : "howitzer";
				Salvo(F.Shots, S, Fi.ShipNo, Fi.T, 0.0, TowardOf(S), Kind == 0 ? Today : nullptr); // (all her turrets in one frame, as the sea scene's Salvo fires them)
			}
			F.Order();
			if (Kind > 0)
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
				Panels[Kind].Lamps.push_back(F.Lit[I]);
			}
			for (const Shot& S : F.Shots)
			{
				if (S.T <= T && S.T > T - 0.3)
				{
					Panels[Kind].Fired.push_back(S);
				}
			}
			Panels[Kind].Name = Kind == 0 ? "today: 2 lamps by strength, howitzer/tank_gun" : Kind == 1 ? "sea rule, naval lamps as first built" : "sea rule, naval lamps hung out and up (now)";
			std::printf("  %s, %.2f s:\n", Panels[Kind].Name.c_str(), T);
			for (size_t I = 0; I < Panels[Kind].Lamps.size(); ++I)
			{
				const Battle::Lamp& L = Panels[Kind].Lamps[I];
				std::printf("    lamp %zu at (%.0f, %.0f, %.0f), %.0f cd, reach %.0f m, %.2f km from the eye\n", I + 1, L.At.X, L.At.Y, L.At.Z, L.Strength * CdAtFull, L.Radius, Far(L.At, Eye) / 1000.0);
			}
			for (int Strip : Strips)
			{
				const Ship& S = Ships[static_cast<size_t>(Strip)];
				const SideSeen Side = SideOf(Panels[Kind].Lamps.data(), static_cast<int>(Panels[Kind].Lamps.size()), CdAtFull, S, TowardOf(S) - S.Mid);
				if (Side.Peak < 1e-3)
				{
					std::printf("    %-10s %.1f km: no lamp on her\n", S.Name, Far(S.Mid, Eye) / 1000.0);
				}
				else if (Side.Ends < 1e-3)
				{
					std::printf("    %-10s %.1f km: her side %6.2f lux at most, an end dark (out of her lamp's reach), %3.0f m within a stop\n", S.Name, Far(S.Mid, Eye) / 1000.0, Side.Peak, Side.WithinOne);
				}
				else
				{
					std::printf("    %-10s %.1f km: her side %6.2f lux at most, %6.3f at her ends (%4.1f stops), %3.0f m within a stop\n", S.Name, Far(S.Mid, Eye) / 1000.0, Side.Peak, Side.Ends, Side.Stops, Side.WithinOne);
				}
			}
		}
		// (the picture: three panels side by side. Each: the water from above (x from -3 km to 5 km, y from -0.3 km,
		// 14.3 m a pixel); under it the battle line close (x from -800 m to 800, 2.9 m a pixel), each hull grey with the
		// plating of the side she fires from drawn along her edge; under that two strips, each a firing battleship's
		// side plating bow to stern: the lux along her as a curve on a log scale (1/16 to 64 lux), lines at 1.5 lux
		// (passed on land) and 4.7 (overlit), and the plating itself shaded. Water and plating are shaded by the lift a
		// lamp gives over a night lit at 2.6 lux (the land's night, as its calibration implies: 1.5 lux was +23% on the
		// screen): dark to amber from 0 to 4.7 lux, white over 4.7. A dotted ring is a lamp's cut-off (its attenuation
		// radius), not its lit area)
		const double X0 = -3000.0, Y0 = -300.0, Px = 8000.0 / 560.0;
		const double ZX0 = -800.0, ZY0 = 1790.0, Pz = 1600.0 / 560.0;
		const int PW = 560, MH = 540, ZH = 200, Gap = 16, Top = 44, SH = 100, Graph = 70, Legend = 64;
		const int ZTop = Top + MH + 6, STop = ZTop + ZH + 6;
		const int H = STop + 2 * SH + Legend;
		Picture Pic;
		Pic.Make(PW * 3 + Gap * 2, H);
		const double Ambient = 2.6, Over = 4.7;
		auto Shade = [&](double E, double& R, double& G, double& B)
		{
			if (E > Over)
			{
				R = G = B = 1.0;
				return;
			}
			const double T = std::clamp(std::log2(1.0 + std::max(0.0, E) / Ambient) / std::log2(1.0 + Over / Ambient), 0.0, 1.0);
			R = 0.010 + (0.62 - 0.010) * T;
			G = 0.016 + (0.46 - 0.016) * T;
			B = 0.040 + (0.22 - 0.040) * T;
		};
		std::string Labels;
		char Line[240];
		for (int Side_ = 0; Side_ < 3; ++Side_)
		{
			const int Ox = Side_ * (PW + Gap);
			const Panel& P = Panels[Side_];
			const Battle::Lamp* Lit = P.Lamps.data();
			const int NLit = static_cast<int>(P.Lamps.size());
			Pic.ClipX0 = Ox;
			Pic.ClipX1 = Ox + PW;
			// (the water, from above and close)
			for (int Y = 0; Y < MH + ZH; ++Y)
			{
				const bool bClose = Y >= MH;
				if (bClose && Y < MH + 6)
				{
					continue;
				}
				for (int X = 0; X < PW; ++X)
				{
					const int Yz = Y - MH - 6;
					const Vec W = bClose ? Vec(ZX0 + (X + 0.5) * Pz, ZY0 + (ZH - Yz - 0.5) * Pz, 0.0) : Vec(X0 + (X + 0.5) * Px, Y0 + (MH - Y - 0.5) * Px, 0.0);
					double R = 0.0, G = 0.0, B = 0.0;
					Shade(LuxAt(Lit, NLit, CdAtFull, W, Vec(0.0, 0.0, 1.0)), R, G, B);
					Pic.Put(Ox + X, Top + Y, R, G, B);
				}
			}
			auto ToPx = [&](const Vec& W, double& Sx, double& Sy)
			{
				Sx = Ox + (W.X - X0) / Px;
				Sy = Top + MH - (W.Y - Y0) / Px;
			};
			auto ToZoom = [&](const Vec& W, double& Sx, double& Sy)
			{
				Sx = Ox + (W.X - ZX0) / Pz;
				Sy = ZTop + ZH - (W.Y - ZY0) / Pz;
			};
			double Sx = 0.0, Sy = 0.0;
			ToPx(OilAt, Sx, Sy);
			Pic.Disc(Sx, Sy, 150.0 / Px, 0.05, 0.03, 0.02, 0.9);
			Pic.Disc(Sx, Sy, 3.0, 0.9, 0.35, 0.05);
			for (const Ship& S : Ships)
			{
				ToPx(S.Mid, Sx, Sy);
				Pic.Box(Sx, Sy, 0.5 * S.Long / Px, std::max(1.0, 0.5 * S.Beam / Px), 0.55, 0.57, 0.6);
				// (close: her hull, and the plating of the side she fires from shaded by its light, along her edge)
				ToZoom(S.Mid, Sx, Sy);
				if (Sx + 0.5 * S.Long / Pz < Ox || Sx - 0.5 * S.Long / Pz > Ox + PW || Sy < ZTop || Sy > ZTop + ZH)
				{
					continue;
				}
				Pic.Box(Sx, Sy, 0.5 * S.Long / Pz, 0.5 * S.Beam / Pz, 0.06, 0.065, 0.075);
				const Vec Way = TowardOf(S) - S.Mid;
				const SideSeen Plating = SideOf(Lit, NLit, CdAtFull, S, Way, 2 * static_cast<int>(0.5 * S.Long / Pz) + 1);
				const double EdgeY = Sy - (Way.Y > 0.0 ? 1.0 : -1.0) * (0.5 * S.Beam / Pz + 1.0);
				for (size_t K = 0; K < Plating.Lux.size(); ++K)
				{
					double R = 0.0, G = 0.0, B = 0.0;
					Shade(Plating.Lux[K], R, G, B);
					const int Xp = static_cast<int>(Sx + 0.5 * S.Long / Pz) - static_cast<int>(K);
					Pic.Put(Xp, static_cast<int>(EdgeY), R, G, B);
					Pic.Put(Xp, static_cast<int>(EdgeY) + (Way.Y > 0.0 ? -1 : 1), R, G, B);
				}
			}
			for (const Shot& S : P.Fired)
			{
				ToPx(S.At, Sx, Sy);
				Pic.Disc(Sx, Sy, 2.0, 1.0, 0.55, 0.15);
				ToZoom(S.At, Sx, Sy);
				if (Sy > ZTop && Sy < ZTop + ZH && Sx > Ox && Sx < Ox + PW)
				{
					Pic.Disc(Sx, Sy, 2.5, 1.0, 0.55, 0.15);
				}
			}
			for (int I = 0; I < NLit; ++I)
			{
				const Battle::Lamp& L = Lit[I];
				ToPx(L.At, Sx, Sy);
				const double Rad = std::max(2.0, L.Radius / Px);
				const int N = std::max(24, static_cast<int>(Rad * 7.0));
				for (int K = 0; K < N; K += 3)
				{
					const double A = 6.283185307179586 * K / N;
					Pic.Put(static_cast<int>(Sx + Rad * std::cos(A)), static_cast<int>(Sy + Rad * std::sin(A)), 0.8, 0.75, 0.3, 0.6);
				}
				Pic.Disc(Sx, Sy, 2.5, 0.3, 0.9, 1.0);
				std::snprintf(Line, sizeof(Line), "%.0f %.0f %d\n", Sx + 4.0, Sy - 14.0, I + 1);
				Labels += Line;
				std::snprintf(Line, sizeof(Line), "%d %d lamp %d: %.0f cd, reach %.0f m, %.0f m up, %.1f km\n", Ox + 300, Top + MH - 48 + 13 * I, I + 1, L.Strength * CdAtFull, L.Radius, L.At.Z, Far(L.At, Eye) / 1000.0);
				Labels += Line;
				ToZoom(L.At, Sx, Sy);
				if (Sy > ZTop && Sy < ZTop + ZH && Sx > Ox && Sx < Ox + PW)
				{
					Pic.Disc(Sx, Sy, 3.0, 0.3, 0.9, 1.0);
					std::snprintf(Line, sizeof(Line), "%.0f %.0f lamp %d, %.0f m up\n", Sx + 6.0, Sy - 6.0, I + 1, L.At.Z);
					Labels += Line;
				}
			}
			ToPx(Eye, Sx, Sy);
			for (int K = 0; K < 9; ++K)
			{
				Pic.Disc(Sx, Sy - K * 0.9, 4.5 - 0.5 * K, 1.0, 1.0, 1.0);
			}
			std::snprintf(Line, sizeof(Line), "%.0f %.0f eye\n", Sx + 8.0, Sy - 8.0);
			Labels += Line;
			ToPx(OilAt, Sx, Sy);
			std::snprintf(Line, sizeof(Line), "%.0f %.0f oil burning, %.1f km\n", Sx + 10.0, Sy + 6.0, Far(OilAt, Eye) / 1000.0);
			Labels += Line;
			std::snprintf(Line, sizeof(Line), "%d %d %s\n", Ox + 8, 8, P.Name.c_str());
			Labels += Line;
			std::snprintf(Line, sizeof(Line), "%d %d the water from above, 14 m a pixel; below, the battle line close, 2.9 m a pixel\n", Ox + 8, 24);
			Labels += Line;
			// (the strips: the two firing battleships, her side facing her line of fire, bow on the left)
			for (int K = 0; K < 2; ++K)
			{
				const Ship& S = Ships[static_cast<size_t>(Strips[K])];
				const int Y0s = STop + K * SH;
				const int Gx0 = Ox + 8, Gw = PW - 16;
				const int GTop = Y0s + 16, GBottom = GTop + Graph;
				const SideSeen Side = SideOf(Lit, NLit, CdAtFull, S, TowardOf(S) - S.Mid, Gw);
				for (int Y = GTop; Y < GBottom; ++Y)
				{
					for (int X = Gx0; X < Gx0 + Gw; ++X)
					{
						Pic.Put(X, Y, 0.012, 0.012, 0.016);
					}
				}
				auto YOf = [&](double Lux) { return GBottom - 1.0 - (std::log2(std::max(Lux, 1.0e-6)) + 4.0) * (Graph - 1) / 10.0; };
				for (int Stop = -4; Stop <= 6; ++Stop)
				{
					const int Yg = static_cast<int>(std::lround(YOf(std::exp2(Stop))));
					for (int X = Gx0; X < Gx0 + Gw; X += 2)
					{
						Pic.Put(X, Yg, 0.05, 0.05, 0.06);
					}
				}
				const int Y15 = static_cast<int>(std::lround(YOf(1.5))), Y47 = static_cast<int>(std::lround(YOf(Over)));
				for (int X = Gx0; X < Gx0 + Gw; ++X)
				{
					Pic.Put(X, Y15, 0.1, 0.45, 0.12);
					Pic.Put(X, Y47, 0.6, 0.08, 0.05);
				}
				for (int X = 0; X < Gw; ++X)
				{
					const double E = Side.Lux[static_cast<size_t>(X)];
					const double Yc = std::clamp(YOf(E), static_cast<double>(GTop), static_cast<double>(GBottom - 1));
					Pic.Put(Gx0 + X, static_cast<int>(Yc), 1.0, 0.95, 0.6);
					Pic.Put(Gx0 + X, static_cast<int>(Yc) + 1, 1.0, 0.95, 0.6);
					double R = 0.0, G = 0.0, B = 0.0;
					Shade(E, R, G, B);
					for (int Y = GBottom + 2; Y < GBottom + 12; ++Y)
					{
						Pic.Put(Gx0 + X, Y, R, G, B);
					}
				}
				if (Side.Peak >= 1e-3 && Side.Ends >= 1e-3)
				{
					std::snprintf(Line, sizeof(Line), "%d %d %s at x %.0f, %.1f km: her side %.2f lux at most, %.2f at her ends (%.1f stops)\n", Gx0, Y0s + 1, S.Name, S.Mid.X, Far(S.Mid, Eye) / 1000.0, Side.Peak, Side.Ends, Side.Stops);
				}
				else if (Side.Peak >= 1e-3)
				{
					std::snprintf(Line, sizeof(Line), "%d %d %s at x %.0f, %.1f km: her side %.2f lux at most, an end dark\n", Gx0, Y0s + 1, S.Name, S.Mid.X, Far(S.Mid, Eye) / 1000.0, Side.Peak);
				}
				else
				{
					std::snprintf(Line, sizeof(Line), "%d %d %s at x %.0f, %.1f km: no lamp on her\n", Gx0, Y0s + 1, S.Name, S.Mid.X, Far(S.Mid, Eye) / 1000.0);
				}
				Labels += Line;
			}
		}
		Pic.ClipX0 = 0;
		Pic.ClipX1 = 1 << 30;
		std::snprintf(Line, sizeof(Line), "%d %d %s\n", 8, H - Legend + 8, "Water and plating: the lift a lamp gives over a night lit at 2.6 lux, dark to amber from 0 to 4.7 lux, white over 4.7 (overlit). Dotted ring: a lamp's cut-off, not its lit area.");
		Labels += Line;
		std::snprintf(Line, sizeof(Line), "%d %d %s\n", 8, H - Legend + 24, "Close: each hull grey, the plating of the side she fires from shaded along her edge. Strips: that plating half her freeboard up, bow left to stern right, lux on a log scale, 1/16 to 64.");
		Labels += Line;
		std::snprintf(Line, sizeof(Line), "%d %d %s\n", 8, H - Legend + 40, "Green line 1.5 lux (the land's lamp, passed); red line 4.7 lux (overlit). Under each curve, the plating shaded as the water.");
		Labels += Line;
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
	EyeOverStrength(Book);
	BehindTheEye(Book);
	SalvoIsOneLamp(Book);
	SalvoLamp(Book);
	OrderOfCalls(Book);
	RoomAndCap(Book);
	Rows(Book);
	HullLit(Book);
	Cost(Book);
	NightPicture(Book, Argv[2]);
	std::printf("\n%d checks, %d failed\n%s\n", Checks, Failed, Failed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
	std::printf("time: the cost check %s\n", bTimeFailed ? "FAILED" : "passed");
	return Failed == 0 && !bTimeFailed ? 0 : 1;
}
