#include "Battle/ShipIce.h"

#include <cstdio>

namespace ShipIce
{
	namespace
	{
		/**
		 * The northern seas the war's convoys and fleets crossed, each its coldest and warmest month's mean air and sea
		 * surface (C) over open water. [EST] read from the climate normals of the stations on their shores and islands
		 * (Murmansk -10 C in January, Vardo -5, Bear Island -8, Jan Mayen -5, Reykjavik -0.5, Tasiilaq -7, St John's -5,
		 * Halifax -6, Cartwright -14, Dutch Harbor -1, Okhotsk -24), the ship reports of the marine climatologies (COADS,
		 * now ICOADS) and the World Ocean Atlas's sea surface (Locarnini et al.), taken at sea, where the air is milder than
		 * ashore. The air is coldest in late January, the sea in early March (AirLow, SeaLow: months from mid-January).
		 * The first box holding a place is its water; longitudes may run past 180 east (the Bering Sea).
		 */
		struct Box
		{
			const char* Where;
			double South, North, West, East;
			double AirCold, AirWarm, SeaCold, SeaWarm;
		};
		constexpr Box Boxes[] = {
			{ "the Barents Sea and the Murmansk run", 68.0, 80.0, 10.0, 50.0, -11.0, 7.0, 1.5, 8.0 },
			{ "the White Sea and the Kola coast", 63.0, 68.0, 30.0, 45.0, -13.0, 13.0, -1.5, 10.0 },
			{ "the Greenland Sea, Jan Mayen and Bear Island's west", 68.0, 80.0, -25.0, 10.0, -6.0, 6.0, 1.5, 7.0 },
			{ "the Denmark Strait", 63.0, 68.0, -35.0, -20.0, -6.0, 7.0, 0.5, 6.0 },
			{ "the Norwegian Sea", 62.0, 68.0, -20.0, 15.0, -1.0, 11.0, 5.0, 11.0 },
			{ "the Labrador Sea and the Strait of Belle Isle", 52.0, 62.0, -65.0, -45.0, -12.0, 8.0, -1.0, 7.0 },
			{ "the Gulf of St Lawrence", 45.0, 52.0, -70.0, -58.0, -10.0, 18.0, -1.5, 15.0 },
			{ "the Grand Banks and the Nova Scotia approaches", 40.0, 52.0, -70.0, -45.0, -3.0, 17.0, 0.5, 16.0 },
			{ "the Baltic", 53.0, 66.0, 9.0, 31.0, -4.0, 17.0, 0.5, 17.0 },
			{ "the North Sea", 51.0, 62.0, -4.0, 9.0, 4.0, 15.0, 6.0, 15.0 },
			{ "the Bering Sea", 56.0, 66.0, 162.0, 200.0, -12.0, 8.0, -1.0, 9.0 },
			{ "the Aleutians and the Gulf of Alaska", 48.0, 61.0, 162.0, 232.0, -2.0, 11.0, 3.0, 11.0 },
			{ "the Sea of Okhotsk and the Kurils", 43.0, 60.0, 135.0, 162.0, -12.0, 14.0, -1.5, 13.0 },
			{ "the Sea of Japan's north", 38.0, 50.0, 127.0, 142.0, -4.0, 22.0, 2.0, 24.0 },
			{ "the Mediterranean", 30.0, 46.0, -6.0, 37.0, 11.0, 26.0, 14.0, 26.0 },
			{ "the northeast Atlantic", 45.0, 62.0, -45.0, -4.0, 6.0, 15.0, 9.0, 15.0 },
		};
		constexpr double AirLow = 0.6, SeaLow = 1.6;
		/** The open ocean by latitude (either hemisphere, its winter in its own): coldest and warmest months [EST, zonal
		 * means of the same climatologies over open water]. */
		struct Band
		{
			double Lat, AirCold, AirWarm, SeaCold, SeaWarm;
		};
		constexpr Band Bands[] = {
			{ 0.0, 26.5, 27.5, 27.0, 28.5 },  { 20.0, 22.0, 27.0, 24.0, 28.0 }, { 30.0, 15.0, 25.0, 18.0, 26.0 },
			{ 40.0, 9.0, 22.0, 13.0, 23.0 },  { 50.0, 3.0, 14.0, 7.0, 14.0 },   { 60.0, -2.0, 9.0, 3.5, 9.0 },
			{ 70.0, -10.0, 4.0, 0.0, 5.0 },   { 80.0, -25.0, 0.0, -1.8, 0.0 },  { 90.0, -30.0, 0.0, -1.8, -1.0 },
		};

		/** A month's mean between the coldest and warmest, the coldest at Low (months from mid-January). */
		double Season(double Cold, double Warm, double Month, double Low)
		{
			return 0.5 * (Cold + Warm) - 0.5 * (Warm - Cold) * std::cos(2.0 * Pi * (Month - Low) / 12.0);
		}

		double MixOf(uint64_t N)
		{
			uint64_t Z = N + 0x9E3779B97F4A7C15ull;
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			Z ^= Z >> 31;
			return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
		}

		const char* MonthName(int Month0)
		{
			static const char* const Names[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
			return Names[((Month0 % 12) + 12) % 12];
		}
	}

	Climate ClimateAt(double Lat, double Lon, int Month0)
	{
		Climate Out;
		const double La = std::isfinite(Lat) ? std::clamp(Lat, -90.0, 90.0) : 45.0;
		double Lo = std::isfinite(Lon) ? std::remainder(Lon, 360.0) : 0.0;
		const double M = static_cast<double>(((Month0 % 12) + 12) % 12);
		for (const Box& B : Boxes)
		{
			const bool bIn = La >= B.South && La < B.North && ((Lo >= B.West && Lo < B.East) || (Lo + 360.0 >= B.West && Lo + 360.0 < B.East));
			if (bIn)
			{
				Out.Air = Season(B.AirCold, B.AirWarm, M, AirLow);
				Out.Sea = std::max(FreezeSea - 0.1, Season(B.SeaCold, B.SeaWarm, M, SeaLow));
				Out.Where = B.Where;
				return Out;
			}
		}
		// (the open ocean by latitude; the southern hemisphere's winter in its own July)
		const double A = std::fabs(La);
		const double Mh = La < 0.0 ? M - 6.0 : M;
		int K = 0;
		while (K + 2 < static_cast<int>(sizeof(Bands) / sizeof(Bands[0])) && A > Bands[K + 1].Lat)
		{
			++K;
		}
		const Band& P = Bands[K];
		const Band& Q = Bands[K + 1];
		const double T = std::clamp((A - P.Lat) / (Q.Lat - P.Lat), 0.0, 1.0);
		auto Lerp = [T](double X, double Y) { return X + (Y - X) * T; };
		Out.Air = Season(Lerp(P.AirCold, Q.AirCold), Lerp(P.AirWarm, Q.AirWarm), Mh, AirLow);
		Out.Sea = std::max(FreezeSea - 0.1, Season(Lerp(P.SeaCold, Q.SeaCold), Lerp(P.SeaWarm, Q.SeaWarm), Mh, SeaLow));
		Out.Where = A < 23.5 ? "the tropics" : A < 45.0 ? "the open ocean, the middle latitudes" : A < 66.5 ? "the open ocean, the high latitudes" : "the polar sea";
		return Out;
	}

	double Ppr(double WindU, double Air, double Sea)
	{
		const double Va = std::isfinite(WindU) ? std::max(0.0, WindU) : 0.0;
		return Va * (FreezeSea - Air) / (1.0 + 0.3 * (std::max(Sea, FreezeSea) - FreezeSea));
	}

	Icing IcingOf(double P)
	{
		return !(P > 0.0) ? Icing::None : P < 22.4 ? Icing::Light : P < 53.3 ? Icing::Moderate : P < 83.0 ? Icing::Heavy : Icing::Extreme;
	}

	const char* Named(Icing Class)
	{
		switch (Class)
		{
		case Icing::Light: return "light";
		case Icing::Moderate: return "moderate";
		case Icing::Heavy: return "heavy";
		case Icing::Extreme: return "extreme";
		default: return "none";
		}
	}

	double RateOf(double P)
	{
		if (!(P > 0.0)) return 0.0;
		if (P < 22.4) return 0.7 * P / 22.4;
		if (P < 53.3) return 0.7 + 1.3 * (P - 22.4) / (53.3 - 22.4);
		if (P < 83.0) return 2.0 + 2.0 * (P - 53.3) / (83.0 - 53.3);
		return std::min(6.0, 4.0 + 2.0 * (P - 83.0) / (83.0 - 53.3));
	}

	double ShipShare(double Long)
	{
		return std::clamp(std::sqrt(75.0 / std::max(10.0, Long)), 0.35, 1.2);
	}

	Frost FrostOf(double Lat, double Lon, int Month0, double WindU, double WindFrom, double BeginT)
	{
		Frost F;
		const Climate C = ClimateAt(Lat, Lon, Month0);
		const double U = std::clamp(std::isfinite(WindU) ? WindU : 0.0, 0.0, 24.4);
		F.Air = C.Air;
		F.Sea = C.Sea;
		F.Wind = U;
		F.Where = C.Where;
		F.PassagePpr = Ppr(std::max(U, PassageWind), C.Air, C.Sea);
		F.BattlePpr = Ppr(U, C.Air, C.Sea);
		F.PassageRate = RateOf(F.PassagePpr);
		F.BattleRate = RateOf(F.BattlePpr);
		F.Hours = PassageHours;
		F.Since = std::isfinite(BeginT) ? BeginT : 0.0;
		F.WindFrom = std::isfinite(WindFrom) ? WindFrom : 0.0;
		return F;
	}

	double ThickOf(const Frost& F, double Long, double Hours)
	{
		if (!F.On()) return 0.0;
		const double Own = ShipShare(Long);
		return std::min(ThickMost, Own * (F.PassageRate * F.Hours + F.BattleRate * std::max(0.0, Hours)));
	}

	double ShareOf(const Frost& F, double Long, double Hours)
	{
		const double T = ThickOf(F, Long, Hours);
		return T > 0.0 ? 1.0 - std::exp(-T / ShareCm) : 0.0;
	}

	std::string Line(const Frost& F, int Month0)
	{
		char Buf[480];
		if (!F.On())
		{
			std::snprintf(Buf, sizeof Buf, "Sea ice: %s in %s, air %.1f C, sea %.1f C, wind %.1f m/s: no spray icing (PPR %.0f; Overland 1990), so no ice is drawn.",
				F.Where, MonthName(Month0), F.Air, F.Sea, F.Wind, F.BattlePpr);
			return Buf;
		}
		std::snprintf(Buf, sizeof Buf,
			"Sea ice: %s in %s, air %.1f C, sea %.1f C, wind %.1f m/s: spray icing %s in the battle (PPR %.0f, %.1f cm an hour on a 75 m hull) and %s on the "
			"passage (PPR %.0f); a 110 m destroyer opens with %.1f cm (glaze %.2f), a 250 m battleship %.1f cm (%.2f), and an hour's battle adds %.1f cm to the destroyer.",
			F.Where, MonthName(Month0), F.Air, F.Sea, F.Wind, Named(IcingOf(F.BattlePpr)), F.BattlePpr, F.BattleRate, Named(IcingOf(F.PassagePpr)), F.PassagePpr,
			ThickOf(F, 110.0, 0.0), ShareOf(F, 110.0, 0.0), ThickOf(F, 250.0, 0.0), ShareOf(F, 250.0, 0.0), ThickOf(F, 110.0, 1.0) - ThickOf(F, 110.0, 0.0));
		return Buf;
	}

	double Exposure(double Along, double Up, double Deck, double Nx, double Ny, double Nz, double Wx, double Wy)
	{
		// (the waterline's metre: every sea washes it)
		if (!(Up >= 1.0))
		{
			return 0.0;
		}
		// Which way it faces: the sky (spray falls on it and lies to freeze), her bow (-X: the sheet strikes it), the wind.
		const double Sky = std::max(0.0, Nz), Ahead = std::max(0.0, -Nx), Into = std::max(0.0, Nx * Wx + Ny * Wy);
		const double Facing = std::max({ 0.85 * Sky, 0.9 * Ahead, 0.8 * Into });
		if (!(Facing > 0.0))
		{
			return 0.0;
		}
		// Forward the most: a fifth of it at her stern.
		const double A = std::clamp(Along, 0.0, 1.0);
		const double Fore = 0.2 + 0.8 * A * A;
		double Height = 1.0;
		if (Up < Deck - 0.4 && std::fabs(Nz) < 0.6)
		{
			// (her side under the main deck: washed bare by the seas, but for the flare at her stem, which the sheet strikes)
			Height = 0.3 * Detail::SmoothStep(0.8, 1.0, A);
		}
		else
		{
			// (from the deck up 6 m the most; above it less, to a third at 20 m over the deck: masts and yards a lighter
			// glaze of the spray's fine part)
			const double Over = Up - Deck;
			Height = Over <= 6.0 ? 1.0 : std::max(0.35, 1.0 - 0.65 * (Over - 6.0) / 14.0);
		}
		return std::clamp(Fore * Height * Facing, 0.0, 1.0);
	}

	double Patch(double X, double Y, double Z)
	{
		const auto Cell = [](double V) { return static_cast<uint64_t>(static_cast<int64_t>(std::floor(V * 2.0)) + (1ll << 40)); };
		const uint64_t Key = Cell(X) * 0x9E3779B1ull ^ Cell(Y) * 0x85EBCA77ull ^ Cell(Z) * 0xC2B2AE3Dull;
		return 0.75 + 0.4 * MixOf(Key);
	}

	void GlazeOne(uint8_t& R, uint8_t& G, uint8_t& B, uint8_t& A, double Cover)
	{
		const double C = std::isfinite(Cover) ? std::clamp(Cover, 0.0, CoverMost) : 0.0;
		if (!(C > 0.0))
		{
			return;
		}
		const auto To = [C](uint8_t V, int Glaze) { return static_cast<uint8_t>(std::lround(V + (Glaze - V) * C)); };
		R = To(R, GlazeR);
		G = To(G, GlazeG);
		B = To(B, GlazeB);
		if (A < AlphaIce)
		{
			A = static_cast<uint8_t>(std::min<long>(AlphaMost, std::lround(A + (AlphaIce - A) * C)));
		}
	}
}
