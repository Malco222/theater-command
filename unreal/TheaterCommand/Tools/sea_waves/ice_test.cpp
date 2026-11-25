// The release 2 ocean plan, step 6, and the look bible's S6 ("ice on rails and guns in the Arctic and the winter
// Atlantic"): ship icing (Battle/ShipIce). No engine: the core itself, on the battle modeller's own American destroyer
// (Content/Models/Ships/Destroyer_USA.json, read here by a small reader of its number lists), held to the published
// figures its comments cite and to the plan:
//   - Overland's predictor PPR = Va (Tf - Ta) / (1 + 0.3 (Tw - Tf)) with Tf = -1.7 C, and NOAA's classes from it;
//   - no ice at all in the tropics, in the Mediterranean, or in the northern summer, so a battle there is drawn as made;
//   - heavy icing on a January Murmansk run, and in the Denmark Strait and the winter North Atlantic;
//   - only what faces up, forward or into the wind is whitened, nothing under the waterline's metre, and her hull's sides
//     below the main deck mostly bare;
//   - every colour stays within the glaze: between her paint and the pale grey-white, never snow-white, and her paint's
//     alpha is never lowered nor left in the band that marks paint laid on the water;
//   - a frost never set (every air battle, Pearl Harbor and Midway, and every sea battle by default) leaves her colours
//     byte for byte as they were;
//   - the same ice every time, whatever is glazed first;
//   - a 20,000-point hull costs under 2 ms, once, as she is laid.
// Usage: ice_test <Content/Models/Ships>   (built and run by Tools/sea_waves/run_storm.sh)

#include "Battle/ShipIce.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Checks = 0, Failed = 0;

	void Check(const std::string& What, bool bOk, const std::string& Got)
	{
		++Checks;
		Failed += bOk ? 0 : 1;
		std::printf("  %s  %s: %s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.c_str());
	}

	void Note(const std::string& What, const std::string& Got) { std::printf("  note  %s: %s\n", What.c_str(), Got.c_str()); }

	std::string Num(double X, int Places = 2)
	{
		char Text[80];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, X);
		return Text;
	}

	/** The engine's types, as much of them as the glaze touches. */
	struct Vec3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};
	struct Colour
	{
		unsigned char B = 0, G = 0, R = 0, A = 255; // (FColor's own order; the glaze reads them by name)
		bool operator==(const Colour& O) const { return R == O.R && G == O.G && B == O.B && A == O.A; }
	};
	/** FMiniatureMesh, as much of it as ShipIce::Glaze reads. */
	struct Mesh
	{
		std::vector<Vec3> Vertices;
		std::vector<Vec3> Normals;
		std::vector<Colour> Colors;
	};

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}

	/** The numbers of a list named Key inside the file (its first one past From): "points":[1,2,3]. */
	bool Numbers(const std::string& Text, const std::string& Key, size_t From, std::vector<double>& Out)
	{
		const size_t At = Text.find("\"" + Key + "\":[", From);
		if (At == std::string::npos)
		{
			return false;
		}
		size_t I = Text.find('[', At) + 1;
		const size_t End = Text.find(']', I);
		Out.clear();
		while (I < End)
		{
			const char* Start = Text.c_str() + I;
			char* Stop = nullptr;
			const double V = std::strtod(Start, &Stop);
			if (Stop == Start)
			{
				++I;
				continue;
			}
			Out.push_back(V);
			I += static_cast<size_t>(Stop - Start);
			while (I < End && (Text[I] == ',' || Text[I] == ' ' || Text[I] == '\n'))
			{
				++I;
			}
		}
		return !Out.empty();
	}

	/** The hex colours of a list named Key ("colours":["5F6E80", ...]). */
	bool Hexes(const std::string& Text, const std::string& Key, std::vector<Colour>& Out)
	{
		const size_t At = Text.find("\"" + Key + "\":[");
		if (At == std::string::npos)
		{
			return false;
		}
		size_t I = Text.find('[', At) + 1;
		const size_t End = Text.find(']', I);
		Out.clear();
		while (I < End)
		{
			const size_t Open = Text.find('"', I);
			if (Open == std::string::npos || Open > End)
			{
				break;
			}
			const size_t Close = Text.find('"', Open + 1);
			const std::string Hex = Text.substr(Open + 1, Close - Open - 1);
			if (Hex.size() == 6)
			{
				const long V = std::strtol(Hex.c_str(), nullptr, 16);
				Colour C;
				C.R = static_cast<unsigned char>((V >> 16) & 0xFF);
				C.G = static_cast<unsigned char>((V >> 8) & 0xFF);
				C.B = static_cast<unsigned char>(V & 0xFF);
				C.A = 128; // (painted metal: the stage's FMiniatureMesh::Finish for a ship, art fix 12)
				Out.push_back(C);
			}
			I = Close + 1;
		}
		return !Out.empty();
	}

	/**
	 * Her near model as the stage lays it: the modeller's points (x forward, y right, z up, metres) in the stage's own
	 * frame (bow toward -X, starboard -Y, as FMiniatureMesh::Sculpted turns them) and world units, each point's paint as
	 * her colour, painted metal at a half (its alpha 128).
	 */
	bool ShipMesh(const std::string& Path, double Units, Mesh& Out)
	{
		const std::string Text = ReadAll(Path);
		const size_t Near = Text.find("\"near\":{");
		std::vector<double> Points, Facings, Paints;
		std::vector<Colour> Paint;
		if (Near == std::string::npos || !Numbers(Text, "points", Near, Points) || !Numbers(Text, "facings", Near, Facings) || !Numbers(Text, "paints", Near, Paints)
			|| !Hexes(Text, "colours", Paint) || Points.size() != Facings.size() || Points.size() != Paints.size() * 3)
		{
			return false;
		}
		const size_t N = Paints.size();
		Out.Vertices.resize(N);
		Out.Normals.resize(N);
		Out.Colors.resize(N);
		for (size_t I = 0; I < N; ++I)
		{
			Out.Vertices[I] = Vec3{ -Points[3 * I] * Units, -Points[3 * I + 1] * Units, Points[3 * I + 2] * Units };
			Out.Normals[I] = Vec3{ -Facings[3 * I], -Facings[3 * I + 1], Facings[3 * I + 2] };
			const size_t K = static_cast<size_t>(std::max(0.0, Paints[I]));
			Out.Colors[I] = K < Paint.size() ? Paint[K] : Colour{ 90, 110, 95, 128 };
		}
		return true;
	}

	/** How whole a colour's glaze is: how far it has gone from her paint toward the glaze (0 to 1), by its red. */
	double Cover(const Colour& Was, const Colour& Now)
	{
		const double Span = static_cast<double>(ShipIce::GlazeR) - Was.R;
		return std::fabs(Span) < 1e-9 ? 0.0 : (static_cast<double>(Now.R) - Was.R) / Span;
	}
}

int main(int ArgC, char** ArgV)
{
	std::printf("Ship icing (Battle/ShipIce; the release 2 ocean plan, step 6; the look bible's S6)\n");

	// ----------------------------------------------------------------------------------------------------------------
	// Overland's predictor and NOAA's classes.
	{
		Check("the freezing point of sea water, Overland's Tf", ShipIce::FreezeSea == -1.7, Num(ShipIce::FreezeSea) + " C");
		// PPR = Va (Tf - Ta) / (1 + 0.3 (Tw - Tf)): worked by hand at 15 m/s, air -10 C, sea 2 C
		const double Hand = 15.0 * (-1.7 + 10.0) / (1.0 + 0.3 * (2.0 + 1.7));
		Check("Overland's PPR worked out as he writes it (15 m/s, air -10 C, sea 2 C)", std::fabs(ShipIce::Ppr(15.0, -10.0, 2.0) - Hand) < 1e-9,
			Num(ShipIce::Ppr(15.0, -10.0, 2.0)) + " (by hand " + Num(Hand) + ")");
		Check("no icing where the air is at or above the freezing point, whatever the wind", ShipIce::Ppr(25.0, -1.7, 0.0) <= 0.0 && ShipIce::Ppr(25.0, 5.0, 4.0) < 0.0,
			Num(ShipIce::Ppr(25.0, 5.0, 4.0)));
		Check("no icing in a calm, however cold", ShipIce::Ppr(0.0, -25.0, -1.0) == 0.0, "0");
		const bool bClasses = ShipIce::IcingOf(0.0) == ShipIce::Icing::None && ShipIce::IcingOf(10.0) == ShipIce::Icing::Light
			&& ShipIce::IcingOf(30.0) == ShipIce::Icing::Moderate && ShipIce::IcingOf(60.0) == ShipIce::Icing::Heavy && ShipIce::IcingOf(90.0) == ShipIce::Icing::Extreme;
		Check("NOAA's classes from it (light under 22.4, moderate under 53.3, heavy under 83, extreme over)", bClasses,
			std::string(ShipIce::Named(ShipIce::IcingOf(30.0))) + " at 30, " + ShipIce::Named(ShipIce::IcingOf(90.0)) + " at 90");
		const bool bRates = std::fabs(ShipIce::RateOf(22.4) - 0.7) < 0.02 && std::fabs(ShipIce::RateOf(53.3) - 2.0) < 0.02 && std::fabs(ShipIce::RateOf(83.0) - 4.0) < 0.02
			&& ShipIce::RateOf(0.0) == 0.0 && ShipIce::RateOf(120.0) <= 6.0;
		Check("and their rates (0.7, 2 and 4 cm an hour at the class edges, never over 6)", bRates,
			Num(ShipIce::RateOf(22.4)) + ", " + Num(ShipIce::RateOf(53.3)) + ", " + Num(ShipIce::RateOf(83.0)) + ", " + Num(ShipIce::RateOf(120.0)) + " cm an hour");
		Check("a longer hull lifts less of the sea to her decks, each square metre of them (Overland's vessels 20 to 75 m)",
			ShipIce::ShipShare(60.0) > ShipIce::ShipShare(110.0) && ShipIce::ShipShare(110.0) > ShipIce::ShipShare(250.0) && ShipIce::ShipShare(250.0) >= 0.35,
			Num(ShipIce::ShipShare(60.0)) + " at 60 m, " + Num(ShipIce::ShipShare(110.0)) + " at 110, " + Num(ShipIce::ShipShare(250.0)) + " at 250");
	}

	// ----------------------------------------------------------------------------------------------------------------
	// The climatology: nothing in the tropics or in summer, heavy on the Murmansk run in January.
	{
		struct Place
		{
			const char* What;
			double Lat, Lon;
			int Month0;
			bool bIce;
		};
		const Place Places[] = {
			{ "Samar in October (the recorded action)", 11.7, 126.2, 9, false },
			{ "the Mediterranean in June (light-ships' own sea)", 37.0, 15.5, 5, false },
			{ "the tropics in January", 5.0, -30.0, 0, false },
			{ "the Murmansk run in July", 70.0, 33.0, 6, false },
			{ "the North Atlantic in July", 55.0, -20.0, 6, false },
			{ "the Murmansk run in January", 70.0, 33.0, 0, true },
			{ "the Barents Sea in February", 73.0, 30.0, 1, true },
			{ "the Denmark Strait in February", 66.0, -27.0, 1, true },
			// (the mid-Atlantic convoy route: the Gulf Stream keeps its air well above freezing in January, so there is no spray
			// icing there; in the Atlantic the ice belongs to its cold western side and to the north, which the rows below hold)
			{ "the mid-Atlantic convoy route in January", 55.0, -20.0, 0, false },
			{ "the Grand Banks in January (the western Atlantic's cold side)", 45.0, -50.0, 0, true },
			{ "the Gulf of St Lawrence in January", 48.0, -62.0, 0, true },
			{ "the Labrador Sea in January", 56.0, -52.0, 0, true },
			{ "the Sea of Okhotsk in January", 50.0, 148.0, 0, true },
		};
		bool bRight = true;
		std::string Said;
		for (const Place& P : Places)
		{
			const ShipIce::Frost F = ShipIce::FrostOf(P.Lat, P.Lon, P.Month0, 13.0, 0.0, 0.0);
			const bool bIce = F.On() && ShipIce::ShareOf(F, 110.0, 0.0) > 0.0;
			bRight = bRight && bIce == P.bIce;
			Said += std::string(bIce == P.bIce ? "" : "WRONG ") + P.What + ": " + ShipIce::Named(ShipIce::IcingOf(F.BattlePpr)) + " (PPR " + Num(F.BattlePpr, 0) + ", "
				+ Num(ShipIce::ThickOf(F, 110.0, 0.0), 1) + " cm on a destroyer); ";
		}
		Check("ice where the winter northern seas have it and nowhere else (13 places)", bRight, Said);
		const ShipIce::Frost Winter = ShipIce::FrostOf(70.0, 33.0, 0, 13.0, 0.0, 0.0);
		Check("a January Murmansk run ices heavily: moderate or worse, and over 3 cm on a destroyer as the battle opens",
			ShipIce::IcingOf(Winter.BattlePpr) >= ShipIce::Icing::Moderate && ShipIce::ThickOf(Winter, 110.0, 0.0) > 3.0 && ShipIce::ShareOf(Winter, 110.0, 0.0) > 0.4,
			ShipIce::Named(ShipIce::IcingOf(Winter.BattlePpr)) + std::string(", ") + Num(ShipIce::ThickOf(Winter, 110.0, 0.0), 1) + " cm, glaze "
				+ Num(ShipIce::ShareOf(Winter, 110.0, 0.0)));
		Check("and it grows while the battle is fought, never past the thickest she carries",
			ShipIce::ThickOf(Winter, 110.0, 2.0) > ShipIce::ThickOf(Winter, 110.0, 0.0) && ShipIce::ThickOf(Winter, 110.0, 100.0) <= ShipIce::ThickMost,
			Num(ShipIce::ThickOf(Winter, 110.0, 0.0), 1) + " cm at the start, " + Num(ShipIce::ThickOf(Winter, 110.0, 2.0), 1) + " after two hours, "
				+ Num(ShipIce::ThickOf(Winter, 110.0, 100.0), 1) + " at the most");
		Check("a destroyer ices more than a battleship in the same weather", ShipIce::ThickOf(Winter, 110.0, 1.0) > ShipIce::ThickOf(Winter, 250.0, 1.0),
			Num(ShipIce::ThickOf(Winter, 110.0, 1.0), 1) + " cm against " + Num(ShipIce::ThickOf(Winter, 250.0, 1.0), 1));
		Check("a harder wind ices harder", ShipIce::FrostOf(70.0, 33.0, 0, 20.0, 0.0, 0.0).BattleRate > Winter.BattleRate,
			Num(ShipIce::FrostOf(70.0, 33.0, 0, 20.0, 0.0, 0.0).BattleRate) + " against " + Num(Winter.BattleRate) + " cm an hour");
		Note("the line the scene says on the Murmansk run", ShipIce::Line(Winter, 0));
		Note("the line in the tropics", ShipIce::Line(ShipIce::FrostOf(11.7, 126.2, 9, 8.0, 0.0, 0.0), 9));
	}

	// ----------------------------------------------------------------------------------------------------------------
	// One colour glazed, and the glaze's range.
	{
		unsigned char R = 95, G = 110, B = 128, A = 128;
		const unsigned char R0 = R, G0 = G, B0 = B;
		ShipIce::GlazeOne(R, G, B, A, 0.0);
		Check("a cover of nothing leaves a colour exactly as it was", R == R0 && G == G0 && B == B0 && A == 128, "unchanged");
		ShipIce::GlazeOne(R, G, B, A, 1.0);
		Check("a cover at its most never goes past the glaze, so no ice is snow-white",
			R <= ShipIce::GlazeR && G <= ShipIce::GlazeG && B <= ShipIce::GlazeB && R > R0 && A > 128 && A <= ShipIce::AlphaMost,
			std::to_string(R) + ", " + std::to_string(G) + ", " + std::to_string(B) + ", alpha " + std::to_string(A));
		// (the band 232 to 248 marks paint laid on the water, M_MapSurface's on_water: the ice never leaves a colour in it)
		bool bBand = true, bDown = true, bKept = true;
		for (int Start = 0; Start <= 255; ++Start)
		{
			for (const double C : { 0.1, 0.4, 0.85 })
			{
				unsigned char Cr = 90, Cg = 100, Cb = 110, Ca = static_cast<unsigned char>(Start);
				ShipIce::GlazeOne(Cr, Cg, Cb, Ca, C);
				const bool bWas = Start >= 232 && Start <= 248;
				bBand = bBand && (bWas || !(Ca >= 232 && Ca <= 248));
				bKept = bKept && (!bWas || Ca == static_cast<unsigned char>(Start));
				bDown = bDown && Ca >= static_cast<unsigned char>(Start);
			}
		}
		Check("the paint's alpha is never lowered by the ice, and never carried into the band that marks paint laid on the water (232 to 248); "
			  "one already in it, a wake's own, is left alone", bBand && bDown && bKept, "768 cases");
	}

	// ----------------------------------------------------------------------------------------------------------------
	// Her own hull: the battle modeller's American destroyer.
	const std::string Folder = ArgC > 1 ? ArgV[1] : "Content/Models/Ships";
	Mesh Bare;
	const bool bModel = ShipMesh(Folder + "/Destroyer_USA.json", 100.0, Bare);
	Check("the battle modeller's American destroyer read (her near model)", bModel, bModel ? std::to_string(Bare.Vertices.size()) + " points" : "not read");
	if (bModel)
	{
		const ShipIce::Frost Winter = ShipIce::FrostOf(70.0, 33.0, 0, 13.0, 0.0, 0.0);
		const ShipIce::Frost Tropic = ShipIce::FrostOf(11.7, 126.2, 9, 8.0, 0.0, 0.0);
		const int N = static_cast<int>(Bare.Vertices.size());
		// (she heads north, the wind on her starboard bow)
		const double Face = 0.0, WindFrom = 0.6;
		Mesh Iced = Bare, None = Bare, Summer = Bare;
		ShipIce::Glaze(Iced, 100.0, Winter, Face, 0.0);
		ShipIce::Glaze(None, 100.0, ShipIce::Frost(), Face, 0.0);
		ShipIce::Glaze(Summer, 100.0, Tropic, Face, 0.0);
		bool bSame = true, bTropic = true;
		for (int I = 0; I < N; ++I)
		{
			bSame = bSame && None.Colors[I] == Bare.Colors[I];
			bTropic = bTropic && Summer.Colors[I] == Bare.Colors[I];
		}
		Check("a frost never set leaves every colour byte for byte as it was (every air battle, Pearl Harbor, and the sea battle by default)", bSame, "21,502 points");
		Check("and a battle in the tropics the same, though the switch is on", bTropic, "unchanged");

		// (her waterline is her model's z = 0; her wind is on her starboard bow)
		const double Off = std::remainder(WindFrom - Face, 2.0 * ShipIce::Pi);
		Mesh Blown = Bare;
		ShipIce::Glaze(Blown, 100.0, Winter, -Off, 0.0); // (the wind off her bow: Glaze takes her heading, the frost its bearing)
		double Deck[ShipIce::Slices];
		{
			std::vector<Vec3> P(Bare.Vertices), Nm(Bare.Normals);
			double MinX = 1e18, MaxX = -1e18, High = 0.0;
			for (int I = 0; I < N; ++I)
			{
				MinX = std::min(MinX, P[I].X / 100.0);
				MaxX = std::max(MaxX, P[I].X / 100.0);
				High = std::max(High, P[I].Z / 100.0);
			}
			ShipIce::DeckLine(P.data(), Nm.data(), N, 0.01, MinX, MaxX, High, Deck);
		}
		int Changed = 0, Wet = 0, Wrong = 0, SideBelow = 0, SideIced = 0, Fore = 0, ForeIced = 0, Aft = 0, AftIced = 0;
		double SideCover = 0.0, DeckCover = 0.0, ForeCover = 0.0, AftCover = 0.0;
		int DeckPoints = 0;
		double MostCover = 0.0;
		bool bRange = true;
		double MinX = 1e18, MaxX = -1e18;
		for (int I = 0; I < N; ++I)
		{
			MinX = std::min(MinX, Bare.Vertices[I].X / 100.0);
			MaxX = std::max(MaxX, Bare.Vertices[I].X / 100.0);
		}
		const double Span = std::max(1e-6, MaxX - MinX);
		for (int I = 0; I < N; ++I)
		{
			const double X = Bare.Vertices[I].X / 100.0, Z = Bare.Vertices[I].Z / 100.0;
			const double Along = (MaxX - X) / Span;
			const int K = std::clamp(static_cast<int>(Along * ShipIce::Slices), 0, ShipIce::Slices - 1);
			const bool bIce = !(Iced.Colors[I] == Bare.Colors[I]);
			Changed += bIce ? 1 : 0;
			if (Z < 1.0)
			{
				++Wet;
				Wrong += bIce ? 1 : 0;
				continue;
			}
			const Vec3& Nm = Bare.Normals[I];
			// (what faces neither the sky, nor her bow, nor the wind is never iced)
			const bool bFacing = Nm.Z > 0.0 || -Nm.X > 0.0 || (Nm.X * -std::cos(Off) + Nm.Y * -std::sin(Off)) > 0.0;
			Wrong += bIce && !bFacing ? 1 : 0;
			const double C = Cover(Bare.Colors[I], Iced.Colors[I]);
			if (Z < Deck[K] - 0.4 && std::fabs(Nm.Z) < 0.6)
			{
				++SideBelow;
				SideIced += bIce ? 1 : 0;
				SideCover += C;
			}
			else
			{
				++DeckPoints;
				DeckCover += C;
			}
			if (Along > 0.75)
			{
				++Fore;
				ForeIced += bIce ? 1 : 0;
				ForeCover += C;
			}
			else if (Along < 0.25)
			{
				++Aft;
				AftIced += bIce ? 1 : 0;
				AftCover += C;
			}
			if (bIce)
			{
				MostCover = std::max(MostCover, C);
				bRange = bRange && C > -0.02 && C <= ShipIce::CoverMost + 0.03 && Iced.Colors[I].A >= Bare.Colors[I].A && Iced.Colors[I].A <= ShipIce::AlphaMost;
			}
		}
		Check("nothing under the waterline's metre is whitened, and nothing that faces away from the sky, her bow and the wind", Wrong == 0,
			std::to_string(Wrong) + " wrong of " + std::to_string(Changed) + " whitened (" + std::to_string(Wet) + " points at or under the waterline)");
		const double SideMean = SideCover / std::max(1, SideBelow), DeckMean = DeckCover / std::max(1, DeckPoints);
		Check("her hull's sides below the main deck stay mostly bare: a fifth or fewer of their points touched (the flare at her stem), "
			  "and their glaze a quarter of what lies over her decks and upper works",
			SideBelow > 100 && SideIced * 5 <= SideBelow && SideMean * 4.0 <= DeckMean,
			std::to_string(SideIced) + " of " + std::to_string(SideBelow) + " side points (" + Num(100.0 * SideIced / std::max(1, SideBelow), 1) + "%), glaze "
				+ Num(SideMean) + " against " + Num(DeckMean) + " over her decks");
		const double ForeMean = ForeCover / std::max(1, Fore), AftMean = AftCover / std::max(1, Aft);
		Check("her forward quarter ices far more thickly than her after quarter (the cutter's own measure: most of her spray ice forward)",
			ForeMean > 2.0 * AftMean && Fore > 100 && Aft > 100,
			"glaze " + Num(ForeMean) + " forward against " + Num(AftMean) + " aft (" + Num(100.0 * ForeIced / std::max(1, Fore), 1) + "% of points forward, "
				+ Num(100.0 * AftIced / std::max(1, Aft), 1) + "% aft)");
		Check("every whitened colour stays within the glaze (never past it, its alpha never lowered)", bRange, "most glazed " + Num(MostCover) + " of " + Num(ShipIce::CoverMost));
		Check("a good part of her is iced on a January Murmansk run (a fifth of her points or more)", Changed * 5 >= N,
			std::to_string(Changed) + " of " + std::to_string(N) + " (" + Num(100.0 * Changed / N, 1) + "%)");
		// (the wind's own side: ice to windward, where the wind is abeam)
		Mesh Beam = Bare;
		ShipIce::Glaze(Beam, 100.0, Winter, -0.5 * ShipIce::Pi, 0.0);
		long long Port = 0, Starboard = 0;
		double PortCover = 0.0, StarboardCover = 0.0;
		for (int I = 0; I < N; ++I)
		{
			// (the points that face mostly athwartships, over the waterline: starboard is -Y in the model's frame, and the wind
			// is abeam from starboard, so those are her windward side)
			if (Bare.Vertices[I].Z / 100.0 < 1.0 || std::fabs(Bare.Normals[I].Y) < 0.7)
			{
				continue;
			}
			const double C = Cover(Bare.Colors[I], Beam.Colors[I]);
			if (Bare.Normals[I].Y < 0.0)
			{
				++Starboard;
				StarboardCover += C;
			}
			else
			{
				++Port;
				PortCover += C;
			}
		}
		const double Windward = StarboardCover / std::max(1ll, Starboard), Lee = PortCover / std::max(1ll, Port);
		Check("with the wind abeam, her windward side ices and her lee side far less (half as thick or less)", Windward > 2.0 * Lee && Starboard > 100 && Port > 100,
			"glaze " + Num(Windward) + " windward (" + std::to_string(Starboard) + " points) against " + Num(Lee) + " to leeward (" + std::to_string(Port) + ")");

		// The same ice every time, whatever is glazed first.
		Mesh Again = Bare, Twice = Bare;
		ShipIce::Glaze(Again, 100.0, Winter, Face, 0.0);
		ShipIce::Glaze(Twice, 100.0, Tropic, Face, 0.0); // (a frost with no ice first: it changes nothing)
		ShipIce::Glaze(Twice, 100.0, Winter, Face, 0.0);
		bool bDeterministic = true;
		for (int I = 0; I < N; ++I)
		{
			bDeterministic = bDeterministic && Again.Colors[I] == Iced.Colors[I] && Twice.Colors[I] == Iced.Colors[I];
		}
		Check("the same ice every time, and the same after a frost that ices nothing", bDeterministic, "21,502 points");

		// What it costs, once, as the stage lays her: a hull of 20,000 points and more.
		{
			Mesh Big = Bare;
			while (Big.Vertices.size() < 20000)
			{
				const size_t Was = Big.Vertices.size();
				Big.Vertices.insert(Big.Vertices.end(), Bare.Vertices.begin(), Bare.Vertices.begin() + static_cast<long>(std::min(Was, Bare.Vertices.size())));
				Big.Normals.insert(Big.Normals.end(), Bare.Normals.begin(), Bare.Normals.begin() + static_cast<long>(std::min(Was, Bare.Normals.size())));
				Big.Colors.insert(Big.Colors.end(), Bare.Colors.begin(), Bare.Colors.begin() + static_cast<long>(std::min(Was, Bare.Colors.size())));
			}
			double Least = 1.0e9;
			for (int Try = 0; Try < 5; ++Try)
			{
				Mesh Copy = Big;
				const auto Began = std::chrono::steady_clock::now();
				ShipIce::Glaze(Copy, 100.0, Winter, Face, 0.0);
				Least = std::min(Least, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Began).count());
			}
			Check("a hull of 20,000 points and more is glazed in under 2 ms, once, as she is laid", Least < 2.0 && Big.Vertices.size() >= 20000,
				Num(Least, 3) + " ms for " + std::to_string(Big.Vertices.size()) + " points (the least of five)");
		}
	}

	std::printf("%d checks, %d failed\n%s\n", Checks, Failed, Failed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failed ? 1 : 0;
}
