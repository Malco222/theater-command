#include "Battle/FleetSpray.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace FleetSpray
{
	namespace
	{
		double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : V > Hi ? Hi : V; }

		/** An angle into (-pi, pi]. */
		double Wrap(double A)
		{
			A = std::fmod(A + Pi, 2.0 * Pi);
			return (A < 0.0 ? A + 2.0 * Pi : A) - Pi;
		}

		/** SplitMix64's step: the spray's own stream (never the show's Dice or Roll), a number from 0 to under 1. */
		double Mix(uint64_t N)
		{
			uint64_t Z = N + 0x9E3779B97F4A7C15ull;
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			Z ^= Z >> 31;
			return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
		}
	}

	Bow BowOf(const std::string& Class, double Long)
	{
		// (the plan's thresholds: 4 destroyers and boats, 5 cruisers, 6 battleships; [EST] a merchant with the cruisers, her
		// low loaded forecastle against her slow speed and bluff bow, and a carrier with the battleships)
		if (Class == "dd") return { 4, 7.0, 11.0 };
		if (Class == "ss") return { 4, 2.5, 5.0 };
		if (Class == "ca") return { 5, 9.0, 17.0 };
		if (Class == "mv") return { 5, 5.0, 12.0 };
		if (Class == "bb") return { 6, 10.0, 22.0 };
		if (Class == "cv") return { 6, 12.0, 25.0 };
		// (any other by her length: a destroyer's up to 125 m, a cruiser's up to 210 m, a battleship's past it)
		if (Long <= 125.0) return { 4, 7.0, 11.0 };
		if (Long <= 210.0) return { 5, 9.0, 17.0 };
		return { 6, 10.0, 22.0 };
	}

	double WindFor(double WindU, bool bGale)
	{
		const double U = std::isfinite(WindU) ? WindU : 0.0;
		return Clamp(U, 0.0, bGale ? GaleCap : Raised);
	}

	int BeaufortOf(double WindU, bool bGale)
	{
		// [P] WMO's scale, U = 0.836 B^(3/2) m/s: force n holds from n - 0.5 to n + 0.5 of B (force 4 from 5.5 m/s, 7 from
		// 13.9 m/s), as FleetWaves::ForceOf's unrounded number
		const double U = WindFor(WindU, bGale);
		const double B = std::pow(U / 0.836, 2.0 / 3.0);
		return static_cast<int>(std::floor(B + 0.5));
	}

	double StemZ(double Long, double Heave, double PitchDeg)
	{
		const double P = std::isfinite(PitchDeg) ? Clamp(PitchDeg, -30.0, 30.0) : 0.0;
		return (std::isfinite(Heave) ? Heave : 0.0) + 0.5 * Long * std::sin(P * Pi / 180.0);
	}

	double Encounter(double WindU, double Speed, double OffBow)
	{
		const double U = std::max(3.0, WindU);
		const double Peak = 0.877 * Gee / U;
		return Clamp(1.0 + Peak * std::max(0.0, Speed) * std::cos(OffBow) / Gee, 0.15, 2.5);
	}

	Sea BowSea(const Hull& H, double Immersion, double Rate, double Beaufort, const std::string& Class, bool bGale)
	{
		Sea Out;
		Out.Head = std::cos(H.OffBow);
		const int Force = static_cast<int>(std::floor((std::isfinite(Beaufort) ? Beaufort : 0.0) + 0.5));
		const int Felt = bGale ? Force : std::min(Force, 6);
		const Bow B = BowOf(Class, H.Long);
		Out.Threshold = SprayShare * OchiSlam * std::sqrt(Gee * std::max(10.0, H.Long));
		if (!H.bThrows || Felt < B.SheetFrom || !std::isfinite(Rate) || !std::isfinite(Immersion))
		{
			return Out;
		}
		// (her forefoot in the sea: the stem's foot within half a metre of the water, or under it)
		if (Rate < Out.Threshold || Immersion < -0.5)
		{
			return Out;
		}
		Out.bSheet = true;
		Out.Severity = Clamp((Rate - Out.Threshold) / (2.0 * Out.Threshold), 0.0, 1.0);
		Out.Jet = JetGain * Rate + SpeedGain * std::max(0.0, H.Speed) * std::max(0.0, Out.Head);
		// (a sheet stands as a jet thrown up would, v^2 / 2g, to a little over her bridge at the most)
		Out.Top = std::min(Out.Jet * Out.Jet / (2.0 * Gee), B.Bridge + 3.0);
		Out.Fan = 30.0 + 30.0 * Out.Severity;
		// (up and down again: twice its rise, v / g, held to 1 - 3 s, the plan's fall)
		Out.Life = Clamp(2.0 * std::sqrt(2.0 * Gee * Out.Top) / Gee, 1.0, 3.0);
		if (bGale && Felt >= 7)
		{
			// [P] WMO's Beaufort 7: "the sea heaps up ... foam blown in streaks along the direction of the wind"; 8: "edges of
			// crests begin to break into spindrift". Green water when the sea over her stem reaches her forecastle (deck
			// wetness: the relative motion over the freeboard, Ochi 1964), or [EST] a hard sheet in a head or bow sea.
			Out.bGreen = Immersion > B.Freeboard || (Out.Severity >= 0.4 && Out.Head > 0.5);
			Out.bSpindrift = true;
		}
		return Out;
	}

	const char* KindWord(What Is)
	{
		return Is == What::Green ? "green" : Is == What::Spindrift ? "spindrift" : "sheet";
	}

	Spray::Spray(const Settings& InSettings) : Given(InSettings)
	{
		Near.reserve(Nearest);
		Event.reserve(8);
	}

	int Spray::Live(double T) const
	{
		int N = 0;
		for (const Alive& A : Puffs)
		{
			N += A.Until > T ? A.Puffs : 0;
		}
		return N;
	}

	bool Spray::Fits(double T, const std::vector<Call>& Calls) const
	{
		int Add = 0;
		for (const Call& C : Calls)
		{
			Add += C.Puffs;
		}
		return Live(T) + Add <= MostLive;
	}

	void Spray::Step(double T, std::vector<Hull>& Ships, double EyeX, double EyeY, const Wind& W, bool bSkipping, std::vector<Call>& Out)
	{
		Out.clear();
		if (T + 5.0 < LastT)
		{
			// (the clock gone back: another battle)
			Tracks.clear();
			Puffs.clear();
			Tally = Count();
		}
		const double Dt = T - LastT;
		LastT = T;
		// (what has fallen is gone from the count)
		Puffs.erase(std::remove_if(Puffs.begin(), Puffs.end(), [T](const Alive& A) { return A.Until <= T; }), Puffs.end());
		const double U = WindFor(W.U, W.bGale);
		const int Force = BeaufortOf(W.U, W.bGale);
		// The Nearest ships to the eye that may throw (their middles' distance on the sea; the lower key first at a tie).
		Near.clear();
		double NearD[Nearest] = {};
		int NearAt[Nearest] = {};
		int Found = 0;
		for (int I = 0; I < static_cast<int>(Ships.size()); ++I)
		{
			Hull& H = Ships[static_cast<size_t>(I)];
			// (the sea off her bow: where the wind blows from, off her heading; the wind sea runs with it)
			H.OffBow = Wrap(W.From - H.Heading);
			if (!H.bThrows || !std::isfinite(H.X) || !std::isfinite(H.Y))
			{
				continue;
			}
			const double D = std::hypot(H.X - EyeX, H.Y - EyeY);
			int Slot = Found;
			while (Slot > 0 && (D < NearD[Slot - 1] || (D == NearD[Slot - 1] && H.Key < Ships[static_cast<size_t>(NearAt[Slot - 1])].Key)))
			{
				if (Slot < Nearest)
				{
					NearD[Slot] = NearD[Slot - 1];
					NearAt[Slot] = NearAt[Slot - 1];
				}
				--Slot;
			}
			if (Slot < Nearest)
			{
				NearD[Slot] = D;
				NearAt[Slot] = I;
				Found = std::min(Found + 1, Nearest);
			}
		}
		for (int K = 0; K < Found; ++K)
		{
			Near.push_back(Ships[static_cast<size_t>(NearAt[K])].Key);
		}
		// (a frame too long, or a skip: each stem's rate taken afresh; nothing thrown while a picture run skips ahead)
		const bool bFresh = bSkipping || !(Dt > 0.0) || Dt > 0.5;
		for (int K = 0; K < Found; ++K)
		{
			const Hull& H = Ships[static_cast<size_t>(NearAt[K])];
			Track& Tr = Tracks[H.Key];
			const double Ride = StemZ(H.Long, H.Heave, H.Pitch);
			const double Water = std::isfinite(H.WaterAtStem) ? H.WaterAtStem : 0.0;
			const double Immersion = Water - Ride;
			double Rate = 0.0;
			const bool bRate = Tr.bHave && !bFresh && Tr.T < T;
			if (bRate)
			{
				const double Step = T - Tr.T;
				// (the table ride's period has no heading in it: its share of the rate is scaled by the encounter; the drawn
				// water at her stem, sampled where her stem is, already meets her at it; step 5's look ride too)
				const double RideRate = -(Ride - Tr.StemRide) / Step;
				const double WaterRate = (Water - Tr.Water) / Step;
				Rate = (H.bTableRide ? Encounter(U, H.Speed, H.OffBow) : 1.0) * RideRate + WaterRate;
			}
			Tr.T = T;
			Tr.StemRide = Ride;
			Tr.Water = Water;
			Tr.Rate = Rate;
			Tr.Immersion = Immersion;
			Tr.SeenAt = T;
			Tr.bHave = true;
			if (!bRate)
			{
				continue;
			}
			const Sea S = BowSea(H, Immersion, Rate, Force, H.Class, W.bGale);
			// (one sheet a plunge: armed again once the rate has fallen back under seven tenths of the threshold)
			if (Rate < 0.7 * S.Threshold)
			{
				Tr.bArmed = true;
			}
			if (!S.bSheet || !Tr.bArmed || T - Tr.LastSheet < Rest)
			{
				continue;
			}
			Tr.bArmed = false;
			Tr.LastSheet = T;
			++Tr.Sheets;
			Throw(T, H, S, W, Out);
		}
		// (ships long out of the nearest forgotten)
		for (auto It = Tracks.begin(); It != Tracks.end();)
		{
			It = T - It->second.SeenAt > 5.0 ? Tracks.erase(It) : std::next(It);
		}
		const int Now = Live(T);
		Tally.MostLive = std::max(Tally.MostLive, Now);
	}

	void Spray::Throw(double T, const Hull& H, const Sea& S, const Wind& W, std::vector<Call>& Out)
	{
		const double U = WindFor(W.U, W.bGale);
		const double Ax = std::sin(H.Heading), Ay = std::cos(H.Heading);
		const double Sx = std::cos(H.Heading), Sy = -std::sin(H.Heading);
		const double StemX = H.X + Ax * 0.5 * H.Long, StemY = H.Y + Ay * 0.5 * H.Long;
		// (the wind across her deck: the wind, blowing toward the way opposite where it comes from, less her own way)
		const double Tx = -std::sin(W.From) * U, Ty = -std::cos(W.From) * U;
		const double DriftX = Windage * (Tx - Ax * H.Speed), DriftY = Windage * (Ty - Ay * H.Speed);
		const double DriftLen = std::hypot(DriftX, DriftY);
		// (the stream: her key and her sheet's count, so each sheet differs a little, the same every play)
		const uint64_t Seed = Given.Seed * 0x2545F4914F6CDD1Dull + static_cast<uint64_t>(static_cast<uint32_t>(H.Key)) * 977ull + Tracks[H.Key].Sheets * 131ull;
		const double Jitter = 0.9 + 0.2 * Mix(Seed);
		const Bow B = BowOf(H.Class, H.Long);
		Event.clear();
		auto Add = [&](Row Kit, What Is, double X, double Y, double Z, double Wx, double Wy, double Wz, double Top, double Fan, double Life)
		{
			Call C;
			C.Kit = Kit;
			C.Is = Is;
			C.Key = H.Key;
			C.T = T;
			C.X = X;
			C.Y = Y;
			C.Z = Z;
			const double L = std::sqrt(Wx * Wx + Wy * Wy + Wz * Wz);
			C.WayX = L > 1e-9 ? Wx / L : Ax;
			C.WayY = L > 1e-9 ? Wy / L : Ay;
			C.WayZ = L > 1e-9 ? Wz / L : 0.0;
			C.DriftX = DriftX;
			C.DriftY = DriftY;
			C.Top = Top;
			C.Fan = Fan;
			C.Life = Life;
			C.Puffs = Kit == Row::Sheet ? RowPuffs : SkipPuffs;
			C.Until = T + (Kit == Row::Sheet ? Life + RowAfter : SkipLife);
			Event.push_back(C);
		};
		// (each sheet a little unlike the last, within her bridge and the plan's fan)
		const double Top = std::min(S.Top * Jitter, B.Bridge + 3.0), Fan = std::min(60.0, S.Fan * Jitter), Life = S.Life;
		// (aft and down the wind: her deck's wind if it blows, else straight aft)
		const double AftX = DriftLen > 0.5 ? DriftX / DriftLen : -Ax, AftY = DriftLen > 0.5 ? DriftY / DriftLen : -Ay;
		const bool bSpindrift = S.bSpindrift && (BeaufortOf(W.U, W.bGale) >= 8 || Mix(Seed + 7) < 0.5);
		if (Given.bSprayRow)
		{
			// The naval row takes the sheet whole: from her stem at the water, thrown up and a little ahead, its fan and height.
			Add(Row::Sheet, What::Sheet, StemX, StemY, H.WaterAtStem, Ax * 0.44, Ay * 0.44, 0.9, Top, Fan, Life);
			if (S.bGreen)
			{
				Add(Row::Sheet, What::Green, StemX - Ax * 0.08 * H.Long, StemY - Ay * 0.08 * H.Long, B.Freeboard, -Ax, -Ay, 0.2, B.Freeboard + 1.0, 70.0, Life + 1.0);
			}
			if (bSpindrift)
			{
				Add(Row::Sheet, What::Spindrift, StemX, StemY, Top, AftX, AftY, 0.1, Top, Fan, Life + 1.0);
			}
		}
		else
		{
			// The kit's Bow row, its low spurts: one each side of her stem out on the sea, along the sheet's fan; one on her
			// forecastle, thrown aft and down the wind, when the sheet stands over it.
			const double Out1 = 0.25 * H.Beam + 1.0;
			const double Fr = Fan * Pi / 180.0;
			const double Bx = StemX - Ax * 0.03 * H.Long, By = StemY - Ay * 0.03 * H.Long;
			Add(Row::SeaSpurt, What::Sheet, Bx + Sx * Out1, By + Sy * Out1, 0.0, Ax * std::cos(Fr) + Sx * std::sin(Fr), Ay * std::cos(Fr) + Sy * std::sin(Fr), 0.0, Top, Fan, Life);
			Add(Row::SeaSpurt, What::Sheet, Bx - Sx * Out1, By - Sy * Out1, 0.0, Ax * std::cos(Fr) - Sx * std::sin(Fr), Ay * std::cos(Fr) - Sy * std::sin(Fr), 0.0, Top, Fan, Life);
			if (Top > B.Freeboard)
			{
				Add(Row::DeckSpurt, What::Sheet, StemX - Ax * 0.08 * H.Long, StemY - Ay * 0.08 * H.Long, 0.0, AftX, AftY, 0.0, Top, Fan, Life);
			}
			if (S.bGreen)
			{
				Add(Row::DeckSpurt, What::Green, StemX - Ax * 0.15 * H.Long, StemY - Ay * 0.15 * H.Long, 0.0, -Ax, -Ay, 0.0, B.Freeboard + 1.0, 70.0, Life + 1.0);
			}
			if (bSpindrift)
			{
				// (to leeward of her bow, carried down the wind)
				const double Lee = (Sx * Tx + Sy * Ty) >= 0.0 ? 1.0 : -1.0;
				const double Off = 0.5 * H.Beam + 2.0;
				Add(Row::SeaSpurt, What::Spindrift, StemX + Sx * Off * Lee, StemY + Sy * Off * Lee, 0.0, Tx, Ty, 0.0, Top, Fan, Life + 1.0);
			}
		}
		// The room: the whole sheet or none of it (the green sea and the spindrift are dropped first, then the sheet).
		while (!Event.empty() && !Fits(T, Event) && Event.back().Is != What::Sheet)
		{
			Event.pop_back();
		}
		if (Event.empty() || !Fits(T, Event))
		{
			++Tally.Dropped;
			return;
		}
		++Tally.Sheets;
		bool bGreen = false, bDrift = false;
		for (const Call& C : Event)
		{
			Puffs.push_back(Alive{ C.Until, C.Puffs });
			Out.push_back(C);
			++Tally.Calls;
			bGreen = bGreen || C.Is == What::Green;
			bDrift = bDrift || C.Is == What::Spindrift;
		}
		Tally.Green += bGreen ? 1 : 0;
		Tally.Spindrift += bDrift ? 1 : 0;
	}

	std::string Spray::Line() const
	{
		char Buf[400];
		std::snprintf(Buf, sizeof Buf,
			"Sea spray: %d sheets over the bows of the %d ships nearest the eye (%d green seas, %d with spindrift; %d left unthrown for room), "
			"%d calls on the kit's %s, at most %d puffs alive at once (%d allowed of its 3,000).",
			Tally.Sheets, Nearest, Tally.Green, Tally.Spindrift, Tally.Dropped, Tally.Calls, Given.bSprayRow ? "naval spray row" : "Bow row (its low spurts on the water)",
			Tally.MostLive, MostLive);
		return Buf;
	}
}
