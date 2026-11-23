#include "LandBurn.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace LandBurn
{
	namespace
	{
		/** Her own dice (splitmix64): the same wreck, the same story. */
		struct Dice
		{
			uint64_t S;
			explicit Dice(uint64_t Seed) : S(Seed ^ 0xB0B5F17E5EEDull) {}
			uint64_t Next()
			{
				uint64_t Z = (S += 0x9E3779B97F4A7C15ull);
				Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
				Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
				return Z ^ (Z >> 31);
			}
			double U() { return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0); }
			double U(double A, double B) { return A + (B - A) * U(); }
			int Whole(int A, int B) { return A + static_cast<int>(std::floor(U() * (B - A + 1) * 0.999999)); }
			bool P(double Chance) { return U() < Chance; }
		};

		/** A grey wisp from Socket from At to the battle's end, as steam calls of at most ten minutes each. */
		void Wisps(Plan& Out, const std::string& Socket, const std::string& Kind, double Across, double At, double Until, int Slot)
		{
			int K = 0;
			for (double T = At; T < Until - 1.0 && K < 12; T += 590.0, ++K)
			{
				Call C;
				C.At = T;
				C.What = Do::Wisp;
				C.Socket = Socket;
				C.Kind = Kind;
				C.Across = Across;
				// (each call dies down over its own time, so a long smoulder is laid as fresh wisps that start a little
				// stronger; the last one runs to the battle's end)
				C.Seconds = std::min(600.0, Until - T);
				C.Slot = Slot;
				Out.Calls.push_back(C);
			}
		}

		void Add(Plan& Out, double At, Do What, const std::string& Socket, double Across, double Seconds, int Slot)
		{
			Call C;
			C.At = At;
			C.What = What;
			C.Socket = Socket;
			C.Across = Across;
			C.Seconds = Seconds;
			C.Slot = Slot;
			Out.Calls.push_back(C);
		}

		void Cook(Plan& Out, double At, const std::string& Socket, double Across, int Pops, double Over, double Jet, int Slot)
		{
			Call C;
			C.At = At;
			C.What = Do::Cook;
			C.Socket = Socket;
			C.Across = Across;
			C.Pops = Pops;
			C.Over = Over;
			C.Jet = Jet;
			C.Slot = Slot;
			Out.Calls.push_back(C);
		}
	}

	std::string ClassOf(const std::string& Kind, const std::string& Name, double Long)
	{
		std::string N = Name;
		std::transform(N.begin(), N.end(), N.begin(), [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
		auto Has = [&](const char* Word) { return N.find(Word) != std::string::npos; };
		if (Kind == "tank")
		{
			// (by her model's name, as her length misleads: a KV-1 or a Char B1 is short for a heavy, a Panther's or a
			// T-34-85's length is mostly gun)
			if (Has("heavy") || Has("tiger") || Has("kv-1") || Has("char b1") || Has("breakthrough"))
			{
				return "heavy";
			}
			return Has("light") || Has("tankette") || Long < 5.3 ? "light" : "medium";
		}
		if (Kind == "carrier" || Kind == "aa")
		{
			return Has("lorry") || Has("truck") ? "lorry" : "light";
		}
		if (Kind == "car")
		{
			// (an armoured car burns like a light tank; a staff car like a car)
			return Has("armoured") || Has("armored") ? "light" : "car";
		}
		return Kind == "gun" ? "gun" : "none";
	}

	Plan Make(const Wreck& W)
	{
		Plan Out;
		Dice D(W.Seed);
		const double Left = std::clamp(W.Left == W.Left ? W.Left : 600.0, 5.0, 7200.0);
		const bool bTank = W.Class == "heavy" || W.Class == "medium" || W.Class == "light";
		// (her size: a heavy tank's deck fire and column a fifth bigger and longer than a medium's, a light one's less)
		const double K = W.Class == "heavy" ? 1.2 : W.Class == "light" || W.Class == "car" ? 0.8 : 1.0;
		const std::string Hatch = W.bHatch ? "fire.2" : "engine";
		const std::string F = W.Fate;
		const bool bBlown = W.bBrewUp || F == "tank.brew_up" || F == "tank.turret_off";
		// (once the turret is thrown, the commander's hatch went with it: the open ring is the hole that roars)
		const std::string Top = bBlown && W.bRing ? "ring" : Hatch;
		if (W.Class == "gun" || W.Class == "none")
		{
			// (a towed gun or a motorcycle: nothing burns; a whiff of smoke where it was struck at most, and a gun whose
			// ready rounds were hit pops them off for a few seconds)
			if (F == "gun.ammo_hit")
			{
				Cook(Out, D.U(0.5, 1.5), "engine", 0.6, D.Whole(3, 6), D.U(4.0, 12.0), 0.0, 2);
			}
			if (F == "gun.ammo_hit" || D.P(0.3))
			{
				Wisps(Out, "engine", "smoke", 0.4, 0.5, std::min(Left, D.U(20.0, 60.0)), 1);
			}
			Out.Story = F == "gun.ammo_hit" ? "ready rounds pop" : "no fire";
			std::stable_sort(Out.Calls.begin(), Out.Calls.end(), [](const Call& A, const Call& B) { return A.At < B.At; });
			Out.Calls.erase(std::remove_if(Out.Calls.begin(), Out.Calls.end(), [&](const Call& C) { return !(C.At < Left); }), Out.Calls.end());
			return Out;
		}
		if (!bTank && !(bBlown || F.empty() || F == "soft.burns" || F == "tank.engine_fire" || F == "tank.knocked_out_later_burns" || (F == "soft.flipped" && D.P(0.5))))
		{
			// (a lorry or a car shot through, riddled, stalled or left: she does not burn; steam or smoke from her engine
			// a while, at most)
			if (D.P(F == "soft.abandoned" ? 0.2 : 0.6))
			{
				Wisps(Out, "engine", "smoke", 0.6, D.U(0.8, 3.0), std::min(Left, D.U(40.0, 160.0)), 1);
			}
			Out.Story = "knocked out, " + std::string(Out.Calls.empty() ? "cold" : "smoking");
		}
		else if (!bTank)
		{
			// (a lorry or a car: her fuel catches at once (the fuel ball is the scene's), her cab and load burn for a few
			// minutes, then the wreck smoulders)
			const double Deck = std::min(Left, D.U(150.0, 360.0) * K);
			Add(Out, 1.2, Do::Fire, "engine", (W.Class == "lorry" ? 1.8 : 1.4) * K, Deck, 1);
			if (W.Class == "lorry")
			{
				Add(Out, D.U(4.0, 15.0), Do::Fire, "fire.2", 1.6, std::min(Left, Deck * D.U(0.7, 1.1)), 2);
			}
			Wisps(Out, "engine", "smoulder", 1.0, 1.2 + Deck, Left, 3);
			Out.bBurns = true;
			Out.FlamesFrom = 1.2;
			Out.FlamesUntil = 1.2 + Deck;
			Out.Story = "vehicle fire, " + std::to_string(static_cast<int>(Deck)) + " s";
		}
		else if (bBlown)
		{
			// Her ammunition went: the clip's jets and the fuel ball are the scene's at once. Then the open ring (or the
			// commander's hatch) roars like a blowtorch for a minute or two, the deck burns under a black column, the
			// rest of her rounds cook off in pops for minutes, and she burns down and smoulders.
			const double Deck = std::min(Left, D.U(360.0, 720.0) * K);
			const double Roar = std::min(Left, D.U(90.0, 220.0) * K);
			Add(Out, 0.7, Do::Fire, "engine", 1.7 * K, Deck, 1);
			Add(Out, 1.0, Do::Vent, Top, (Top == "ring" ? 0.9 : 0.6) * K, Roar, 2);
			if (W.bDriver && D.P(0.7))
			{
				Add(Out, D.U(2.0, 8.0), Do::Vent, "fire.3", 0.5, std::min(Left, D.U(50.0, 160.0)), 3);
			}
			Cook(Out, D.U(2.0, 5.0), Top, 0.9, D.Whole(9, 16), D.U(90.0, 200.0), 0.0, 4);
			if (D.P(0.5))
			{
				Cook(Out, D.U(150.0, 260.0), Top, 0.7, D.Whole(3, 6), D.U(15.0, 40.0), D.P(0.4) ? D.U(2.0, 4.0) : 0.0, 5);
			}
			Wisps(Out, Top, "smoulder", 0.8, 1.0 + Roar, Left, 6);
			Wisps(Out, "engine", "smoulder", 1.1, 0.7 + Deck, Left, 7);
			Out.bBurns = true;
			Out.FlamesFrom = 0.7;
			Out.FlamesUntil = 0.7 + Deck;
			Out.Story = "brew-up: " + Top + " roars " + std::to_string(static_cast<int>(Roar)) + " s, deck " + std::to_string(static_cast<int>(Deck)) + " s";
		}
		else if (F == "tank.small_fire_out")
		{
			// (a small fire in her engine that her crew or the extinguisher put out: low flames a minute or two, then
			// grey smoke thinning away)
			const double Catch = D.U(0.8, 2.5);
			const double Small = std::min(Left, D.U(40.0, 130.0));
			Add(Out, Catch, Do::Fire, "engine", 0.9 * K, Small, 1);
			Wisps(Out, "engine", "smoke", 0.8, Catch + Small, std::min(Left, Catch + Small + D.U(60.0, 180.0)), 2);
			Out.bBurns = true;
			Out.FlamesFrom = Catch;
			Out.FlamesUntil = Catch + Small;
			Out.Story = "small engine fire put out after " + std::to_string(static_cast<int>(Small)) + " s";
		}
		else if (F == "tank.engine_fire" || F == "tank.knocked_out_later_burns" || F == "soft.burns" || F.empty() || (F == "tank.overturned" && D.P(0.5)))
		{
			// The fire starts in her engine (at once, or after a minute of thickening smoke while her crew get clear),
			// burns on the deck under its column, then spreads forward and comes out of her hatches; some of her rounds
			// cook off later. A table that picked nothing burns as an engine fire.
			const double Catch = F == "tank.knocked_out_later_burns" ? D.U(30.0, 120.0) : D.U(0.8, 2.5);
			if (Catch > 5.0)
			{
				Wisps(Out, "engine", "smoke", 0.9, 1.5, std::min(Left, Catch + 3.0), 1);
				Wisps(Out, Hatch, "smoke", 0.5, D.U(2.0, 6.0), std::min(Left, Catch + 3.0), 2);
			}
			const double Deck = std::min(Left, D.U(420.0, 900.0) * K);
			Add(Out, Catch, Do::Fire, "engine", 1.6 * K, Deck, 3);
			const double Spread = Catch + D.U(40.0, 120.0);
			double Through = Catch + Deck;
			if (Spread < Left)
			{
				const double Roar = std::min(Left - Spread, D.U(120.0, 300.0) * K);
				Add(Out, Spread, Do::Vent, Hatch, 0.55 * K, Roar, 4);
				Wisps(Out, Hatch, "smoulder", 0.6, Spread + Roar, Left, 5);
				Through = std::max(Through, Spread + Roar);
				if (W.bDriver && D.P(0.5))
				{
					const double Drv = Spread + D.U(5.0, 30.0);
					if (Drv < Left)
					{
						Add(Out, Drv, Do::Vent, "fire.3", 0.5, std::min(Left - Drv, D.U(60.0, 180.0)), 6);
					}
				}
				if (D.P(0.45))
				{
					Cook(Out, Spread + D.U(20.0, 140.0), Hatch, 0.6, D.Whole(3, 8), D.U(8.0, 30.0), 0.0, 7);
				}
			}
			Wisps(Out, "engine", "smoulder", 1.1, Catch + Deck, Left, 8);
			Out.bBurns = true;
			Out.FlamesFrom = Catch;
			Out.FlamesUntil = Through;
			Out.Story = "engine fire from " + std::to_string(static_cast<int>(Catch)) + " s, out of the hatch from " + std::to_string(static_cast<int>(Spread)) + " s";
		}
		else
		{
			// Knocked out and not burning (her crew bailed out, her crew killed, a track off, shaken): grey smoke from a
			// hatch or the engine for a minute or three, thinning away.
			const bool bSmokes = F == "tank.crew_killed" ? D.P(0.45) : F == "tank.glance" || F == "tank.shaken" || F == "tank.rattle" || F == "tank.track_lurch" ? false : D.P(0.75);
			if (bSmokes)
			{
				const bool bEngine = D.P(0.5);
				Wisps(Out, bEngine ? "engine" : Hatch, "smoke", bEngine ? 0.8 : 0.5, D.U(1.0, 4.0), std::min(Left, D.U(60.0, 220.0)), 1);
			}
			Out.Story = bSmokes ? "knocked out, smoking" : "knocked out, cold";
		}
		std::stable_sort(Out.Calls.begin(), Out.Calls.end(), [](const Call& A, const Call& B) { return A.At < B.At; });
		// (nothing after the battle's end)
		Out.Calls.erase(std::remove_if(Out.Calls.begin(), Out.Calls.end(), [&](const Call& C) { return !(C.At < Left); }), Out.Calls.end());
		return Out;
	}
}
