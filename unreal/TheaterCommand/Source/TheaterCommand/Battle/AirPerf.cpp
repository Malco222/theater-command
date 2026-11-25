#include "Battle/AirPerf.h"

#include "Battle/AirFeed.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace AirPerf
{
	namespace
	{
		constexpr double Gee = 9.81;
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Deg = Pi / 180.0;
		constexpr double SeaRho = 1.225;
		constexpr double Watts = 745.7;      // a horsepower
		constexpr double PropEta = 0.8;      // a propeller's efficiency in the climb and the turn
		constexpr double Oswald = 0.8;       // the span's efficiency, e
		constexpr double PilotG = 6.5;       // the most g a pilot of the war pulled and kept his sight, without a g-suit
		constexpr double ClpQuarter = 0.1125; // a straight wing's roll damping, -Clp / 4 (Clp about -0.45)

		/** A figure as the file gives it: [value, source, mark (, low, high)], or a plain number; 0 if absent. */
		double Fig(const AirFeed::Json& J)
		{
			if (J.IsNumber()) return J.Num();
			if (J.IsArray() && J.Count() > 0) return J.At(0).Num();
			return 0.0;
		}

		std::string MarkOf(const AirFeed::Json& J)
		{
			return J.IsArray() && J.Count() > 2 ? J.At(2).Str("") : std::string();
		}

		double Sigma(double HeightM) { return Density(HeightM) / SeaRho; }

		/** Its weight, newtons. */
		double Weight(const Row& R) { return std::max(R.Mass, 100.0) * Gee; }

		/** Its engines' power at a height, watts: its low gear's power up to that gear's height where the historian gave
		 * one, then straight across to its rated power at its rated height, held there, and above it falling with the air. */
		double PowerAt(const Row& R, double HeightM)
		{
			const double Rated = R.Power * Watts;
			if (HeightM > R.PowerM) return Rated * Density(HeightM) / Density(R.PowerM);
			if (R.PowerLow <= 0.0 || R.PowerLowM >= R.PowerM) return Rated;
			if (HeightM <= R.PowerLowM) return R.PowerLow * Watts;
			return (R.PowerLow + (R.Power - R.PowerLow) * (HeightM - R.PowerLowM) / (R.PowerM - R.PowerLowM)) * Watts;
		}

		/** Its thrust at a height and a true speed, newtons. */
		double ThrustAt(const Row& R, double HeightM, double Speed)
		{
			if (R.Thrust > 0.0) return R.Thrust * 1000.0 * std::pow(Sigma(HeightM), 0.7);
			return PropEta * PowerAt(R, HeightM) / std::max(Speed, 40.0);
		}

		/** pi e A: the induced drag's divisor. */
		double SpanFactor(const Row& R) { return Pi * Oswald * R.Span * R.Span / std::max(R.Wing, 1.0); }

		/** Its zero-lift drag coefficient, from its own top speed (thrust equals drag there). */
		double DragZero(const Row& R)
		{
			const double V = R.Top / 3.6, Q = 0.5 * Density(R.TopM) * V * V, QS = Q * std::max(R.Wing, 1.0);
			const double W = Weight(R);
			const double Induced = W * W / (QS * SpanFactor(R));
			return std::clamp((ThrustAt(R, R.TopM, V) - Induced) / QS, 0.012, 0.06);
		}

		/** The lift it uses in a hard turn: the historian's own figure where he gave one (a slatted or flapped wing), else
		 * a plain wing's 1.35 (the rules' data/aircraft.json gives the Spitfire 1.36 and the Hurricane 1.35). */
		double TurnLift(const Row& R)
		{
			return R.ClMax > 0.0 ? std::clamp(R.ClMax, 1.0, 1.7) : 1.35;
		}

		double GMaxOf(const Row& R)
		{
			const double Limit = R.GLimit > 0.0 ? R.GLimit : R.IsFighter() ? 7.0 : 3.0;
			return std::min(Limit, PilotG);
		}
	}

	double Density(double HeightM)
	{
		const double H = std::clamp(HeightM, 0.0, 11000.0);
		return SeaRho * std::pow(1.0 - 2.25577e-5 * H, 4.25588);
	}

	std::string Plain(const std::string& Name)
	{
		std::string Out;
		for (const char Ch : Name)
		{
			const unsigned char U = static_cast<unsigned char>(Ch);
			if (std::isalnum(U)) Out.push_back(static_cast<char>(std::tolower(U)));
		}
		return Out;
	}

	double TurnRadius(const Row& R, double HeightM, double* OutTurnSeconds)
	{
		if (OutTurnSeconds) *OutTurnSeconds = 0.0;
		if (R.Mass <= 0.0 || R.Wing <= 0.0 || R.Span <= 0.0 || R.Top <= 0.0 || (R.Power <= 0.0 && R.Thrust <= 0.0)) return 0.0;
		const double Rho = Density(HeightM), S = R.Wing, W = Weight(R), Cd0 = DragZero(R), Cl = TurnLift(R), Span = SpanFactor(R), GMost = GMaxOf(R);
		// (from just over its stall, true at this height, to its top speed)
		const double Slow = (R.Stall > 0.0 ? R.Stall / 3.6 : std::sqrt(2.0 * W / (SeaRho * S * 1.35))) * std::sqrt(SeaRho / Rho) * 1.05;
		double Best = std::numeric_limits<double>::infinity(), BestTime = 0.0;
		for (double V = Slow; V <= R.Top / 3.6; V += 0.5)
		{
			const double QS = 0.5 * Rho * V * V * S;
			const double Spare = ThrustAt(R, HeightM, V) - QS * Cd0;
			if (Spare <= 0.0) continue;
			const double N = std::min({ std::sqrt(Spare * QS * Span) / W, Cl * QS / W, GMost });
			if (N <= 1.05) continue;
			const double Radius = V * V / (Gee * std::sqrt(N * N - 1.0));
			if (Radius < Best)
			{
				Best = Radius;
				BestTime = 2.0 * Pi * Radius / V;
			}
		}
		if (!std::isfinite(Best)) return 0.0;
		if (OutTurnSeconds) *OutTurnSeconds = BestTime;
		return Best;
	}

	Flight Limits(const Row& R)
	{
		Flight F;
		const bool bFighter = R.IsFighter();
		F.VTop = R.Top > 0.0 ? R.Top / 3.6 : bFighter ? 150.0 : 110.0;
		F.VMin = R.Stall > 0.0 ? 1.15 * R.Stall / 3.6 : bFighter ? 45.0 : 40.0;
		// (a fighter cruises in formation and fights at about four-fifths of its top speed, a bomber at its cruise)
		if (bFighter) F.VMax = std::max(R.Cruise / 3.6, 0.82 * F.VTop);
		else F.VMax = R.Cruise > 0.0 ? R.Cruise / 3.6 : 0.75 * F.VTop;
		F.VMax = std::clamp(F.VMax, std::min(F.VMin * 1.3, 0.95 * F.VTop), 0.95 * F.VTop);
		F.VDive = std::max(R.Dive / 3.6, F.VTop);
		F.GMax = GMaxOf(R);
		// (the turn: a measured radius where the historian laid one, else worked out; held to sane bounds, a fighter's 150
		// to 450 m as the brief of 24 November sets them, any other's 150 to 2,500 m. Only the Me 262 is held: it works out
		// at about 495 m and flies at 450, still the widest of the fighters.)
		double Radius = R.TurnRadius > 0.0 && R.TurnMark == "measured" ? R.TurnRadius : TurnRadius(R, FightM);
		if (Radius <= 0.0) Radius = bFighter ? 300.0 : 800.0;
		F.RMin = bFighter ? std::clamp(Radius, 150.0, 450.0) : std::clamp(Radius, 150.0, 2500.0);
		F.RollMax = (R.Roll > 0.0 ? R.Roll : bFighter ? 90.0 : 30.0) * Deg;
		const double K = R.Engines >= 3 || R.Span >= 28.0 ? 0.19 : R.Engines == 2 ? 0.16 : 0.13;
		const double Loading = R.Wing > 0.0 ? R.Mass / R.Wing : 150.0;
		F.RollTime = std::clamp(Loading * K * K / (ClpQuarter * Density(FightM) * std::max(F.VMax, 30.0)), 0.08, 1.2);
		F.ClimbMost = R.Climb > 0.0 ? R.Climb : bFighter ? 15.0 : 5.0;
		F.Ceiling = R.Ceiling > 0.0 ? R.Ceiling : bFighter ? 10000.0 : 7500.0;
		return F;
	}

	double RollAt(const Row& R, double IndicatedMs)
	{
		const double Peak = (R.Roll > 0.0 ? R.Roll : 90.0) * Deg;
		const double PeakAt = R.RollKmh > 0.0 ? R.RollKmh / 3.6 : 85.0;
		const double Slow = R.Stall > 0.0 ? R.Stall / 3.6 : 35.0;
		if (IndicatedMs <= PeakAt)
		{
			// (the ailerons' bite grows with speed from about half the stall up to the peak, where the stick force caps it)
			return Peak * std::clamp((IndicatedMs - 0.5 * Slow) / std::max(PeakAt - 0.5 * Slow, 1.0), 0.3, 1.0);
		}
		if (R.RollFast <= 0.0 || R.RollFastKmh <= R.RollKmh) return Peak;
		const double Fast = R.RollFast * Deg, FastAt = R.RollFastKmh / 3.6;
		const double Rate = Peak + (Fast - Peak) * (IndicatedMs - PeakAt) / (FastAt - PeakAt);
		return std::max(Rate, Fast / 3.0);
	}

	const Row* Table::Named(const std::string& Name) const
	{
		const auto It = ByName.find(Plain(Name));
		return It == ByName.end() ? nullptr : &Rows[It->second];
	}

	const Row* Table::Of(const std::string& Model, const std::string& Nation, double Year) const
	{
		const size_t Colon = Model.find(':');
		const std::string Base = Plain(Colon == std::string::npos ? Model : Model.substr(0, Colon));
		std::string Nat = !Nation.empty() ? Nation : Colon == std::string::npos ? std::string() : Model.substr(Colon + 1);
		if (Nat == "JPN") Nat = "JAP";
		// A plain model with its nation ("Fighter:SOV", or "Fighter" and "SOV"): the type it stands for, by nation and date.
		const auto Stand = StandIns.find(Base);
		if (Stand != StandIns.end() && !Nat.empty())
		{
			for (const StandIn& S : Stand->second)
			{
				if (!S.Nation.empty() && S.Nation != Nat) continue;
				if (S.Until > 0.0 && Year > 0.0 && Year >= S.Until) continue;
				if (const Row* Standing = Named(S.Of)) return Standing;
			}
		}
		const auto It = ByName.find(Base);
		if (It == ByName.end()) return nullptr;
		const Row& Found = Rows[It->second];
		// (a model's plain name flies the mark of the battle's date; a mark named outright flies as named)
		if (Year <= 0.0 || Found.Marks.empty() || Base == Plain(Found.Key)) return &Found;
		const Row* Pick = &Found;
		for (const Row::Mark& K : Found.Marks)
		{
			if (Year >= K.From)
			{
				if (const Row* Later = Named(K.Of)) Pick = Later;
			}
		}
		return Pick;
	}

	bool Read(const std::string& Text, Table& Out, std::string& OutError)
	{
		Out = Table();
		AirFeed::Json J;
		if (!AirFeed::Parse(Text, J, OutError)) return false;
		const AirFeed::Json& List = J["rows"];
		if (!List.IsArray() || List.Count() == 0)
		{
			OutError = "no rows";
			return false;
		}
		for (size_t I = 0; I < List.Count(); ++I)
		{
			const AirFeed::Json& X = List.At(I);
			Row R;
			R.Key = X["key"].Str("");
			R.Nation = X["nation"].Str("");
			R.Role = X["role"].Str("");
			R.Style = X["style"].Str("");
			R.Engines = X["engines"].Int(1);
			for (size_t N = 0; N < X["names"].Count(); ++N) R.Names.push_back(X["names"].At(N).Str(""));
			R.Span = Fig(X["span_m"]);
			R.Wing = Fig(X["wing_m2"]);
			R.Mass = Fig(X["mass_kg"]);
			R.Power = Fig(X["power_hp"]);
			R.PowerM = Fig(X["power_m"]);
			R.PowerLow = Fig(X["power_low_hp"]);
			R.PowerLowM = Fig(X["power_low_m"]);
			R.Thrust = Fig(X["thrust_kn"]);
			R.Top = Fig(X["top_kmh"]);
			R.TopM = Fig(X["top_m"]);
			R.Cruise = Fig(X["cruise_kmh"]);
			R.Stall = Fig(X["stall_kmh"]);
			R.Climb = Fig(X["climb_ms"]);
			R.Ceiling = Fig(X["ceiling_m"]);
			R.TurnRadius = Fig(X["turn_m"]);
			R.TurnMark = MarkOf(X["turn_m"]);
			R.TurnTime = Fig(X["turn_s"]);
			R.TurnM = Fig(X["turn_at_m"]);
			R.Roll = Fig(X["roll_dps"]);
			R.RollKmh = Fig(X["roll_kmh"]);
			R.RollFast = Fig(X["roll_fast_dps"]);
			R.RollFastKmh = Fig(X["roll_fast_kmh"]);
			R.GLimit = Fig(X["g_limit"]);
			R.Dive = Fig(X["dive_kmh"]);
			R.Bomb = Fig(X["bomb_kg"]);
			R.Range = Fig(X["range_km"]);
			R.ClMax = Fig(X["clmax"]);
			for (size_t K = 0; K < X["marks"].Count(); ++K)
			{
				Row::Mark M;
				M.Of = X["marks"].At(K)["row"].Str("");
				M.From = X["marks"].At(K)["from"].Num(0.0);
				R.Marks.push_back(M);
			}
			if (R.Key.empty())
			{
				OutError = "a row with no key";
				return false;
			}
			const size_t At = Out.Rows.size();
			std::vector<std::string> Calls = R.Names;
			Calls.push_back(R.Key);
			for (const std::string& Name : Calls)
			{
				const std::string P = Plain(Name);
				const auto Was = Out.ByName.find(P);
				if (Was != Out.ByName.end() && Was->second != At)
				{
					OutError = "the name " + Name + " is given to " + Out.Rows[Was->second].Key + " and " + R.Key;
					return false;
				}
				Out.ByName[P] = At;
			}
			Out.Rows.push_back(R);
		}
		const AirFeed::Json& Stands = J["stand_ins"];
		for (size_t I = 0; I < Stands.Keys.size() && I < Stands.Values.size(); ++I)
		{
			std::vector<Table::StandIn>& Into = Out.StandIns[Plain(Stands.Keys[I])];
			for (size_t K = 0; K < Stands.Values[I].Count(); ++K)
			{
				const AirFeed::Json& E = Stands.Values[I].At(K);
				Table::StandIn S;
				S.Nation = E["nation"].Str("");
				S.Of = E["row"].Str("");
				S.Until = E["until"].Num(0.0);
				if (!Out.Named(S.Of))
				{
					OutError = "the stand-in " + Stands.Keys[I] + " names no row " + S.Of;
					return false;
				}
				Into.push_back(S);
			}
		}
		for (const Row& R : Out.Rows)
		{
			for (const Row::Mark& M : R.Marks)
			{
				if (!Out.Named(M.Of))
				{
					OutError = R.Key + "'s mark " + M.Of + " is no row";
					return false;
				}
			}
		}
		return true;
	}
}
