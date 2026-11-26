// The air's dramatic moments (effects-r2; the owner, 26 November: "i like the debris coming off the planes when hit",
// "lets try to add more visual dramatic things like that", then "do 2-11"): the effects kit's own code, no engine, with
// the game's recipe (Content/Effects/battle-fx.json) and the overlays the air scene's words lay on it
// (battle-fx-air-spurt.json, battle-fx-air-vapour.json, battle-fx-air-flak.json).
//
//   A. the spray at the hit (AirHit's Part, air_hit.spurt): with no Part, or the row off, every hit draws byte for byte
//      as before; with each Part its count, its life and its colour; at most 40 sprays a frame; no throw of the kit's
//      shared dice (an unrelated effect laid after the hit is the same to the byte with the Part and without).
//
//     Tools/fx_air_ten/run.sh [out folder]
//     air_ten_test <battle-fx.json> <folder of the overlays> <out folder>
#define private public // (the test reads the kit's pieces, as tools/battle-fx/preview/barrage.cpp does)
#include "../fx_lamps/fx_lamps_recipe.h"
#undef private

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
	using FxLamps::JsonRecipe;
	using TheaterFx::Battle;
	using TheaterFx::Bit;
	using TheaterFx::Rgb;
	using TheaterFx::Vec;

	int Checks = 0, Failed = 0;
	std::string Overlays; // (the folder the overlays are in)

	void Check(bool bOk, const char* What, const std::string& Got)
	{
		++Checks;
		if (!bOk)
		{
			++Failed;
		}
		std::printf("  %s %-66s %s\n", bOk ? "ok  " : "FAIL", What, Got.c_str());
	}

	std::string Fmt(const char* F, double A = 0.0, double B = 0.0, double C = 0.0, double D = 0.0)
	{
		char Buf[256];
		std::snprintf(Buf, sizeof(Buf), F, A, B, C, D);
		return Buf;
	}

	/** The recipe, with these overlays (file names in Overlays) laid over it, as FBattleEffects::LayOver does. */
	JsonRecipe With(const char* Base, const std::vector<std::string>& Over)
	{
		JsonRecipe R;
		FxLamps::LoadRecipe(Base, R);
		for (const std::string& Name : Over)
		{
			const std::string Path = Overlays + "/" + Name;
			if (!FxLamps::LoadRecipe(Path.c_str(), R))
			{
				std::printf("  FAIL no overlay %s\n", Path.c_str());
				++Failed;
			}
		}
		return R;
	}

	/** FNV-1a over what the last Draw laid: its points and its colours. */
	uint64_t Hash(const Battle& Fx, int N)
	{
		uint64_t H = 1469598103934665603ull;
		auto Eat = [&H](const void* P, size_t Bytes)
		{
			const unsigned char* C = static_cast<const unsigned char*>(P);
			for (size_t I = 0; I < Bytes; ++I)
			{
				H = (H ^ C[I]) * 1099511628211ull;
			}
		};
		const size_t Points = static_cast<size_t>(N) * static_cast<size_t>(Fx.PerPuff());
		Eat(Fx.Points().data(), Points * sizeof(Vec));
		Eat(Fx.Colours().data(), Points * 4);
		return H;
	}

	std::string Hex(uint64_t H)
	{
		char Buf[24];
		std::snprintf(Buf, sizeof(Buf), "%016llx", static_cast<unsigned long long>(H));
		return Buf;
	}

	/** An eye 1,600 px across, 55 degrees, at Eye looking at At, by day with the sun high to one side. */
	Battle::View Eye(const Vec& At, const Vec& Look)
	{
		Battle::View V = FxLamps::Looking(At, Look);
		V.HalfAcross = std::tan(0.5 * 55.0 * 3.14159265358979323846 / 180.0);
		V.Sun = Vec(-0.4, 0.3, 0.87).Unit();
		return V;
	}

	void Begin(Battle& Fx, const JsonRecipe& R)
	{
		Fx.Setup(&R, &FxLamps::Flat, nullptr, 7);
		Fx.SetWind(Vec(6.0, 2.0, 0.0));
	}

	// -----------------------------------------------------------------------------------------------------------------
	// A. The spray at the hit

	/** A fighter at 4,000 m flying +X at 140 m/s takes a strike of Kind with Part at 1 s; frames every 1/30 s to 4 s,
	 * seen from 60 m abeam. Each frame's hash folded into one, the pieces made, and the frame hash at 1.5 s. */
	struct HitRun
	{
		uint64_t All = 0, AtHalf = 0;
		size_t Made = 0;
		std::vector<Bit> After; // (the pieces an unrelated burst laid after the hit, as laid)
		uint64_t SeedsAfter = 0;
	};
	HitRun HitPlay(const JsonRecipe& R, const char* Kind, const char* Part, bool bPassPart, bool bAfter = false)
	{
		Battle Fx;
		Begin(Fx, R);
		HitRun Out;
		const Vec Carry(140.0, 0.0, 0.0);
		for (int F = 0; F <= 120; ++F)
		{
			const double T = F / 30.0;
			const Vec Plane = Vec(0.0, 0.0, 4000.0) + Carry * T;
			if (F == 30)
			{
				const size_t Before = Fx.Bits.size();
				if (bPassPart)
				{
					Fx.AirHit(T, Plane + Vec(1.0, 2.0, 0.3), Vec(0.0, 1.0, 0.2), Carry, Kind, Part);
				}
				else
				{
					Fx.AirHit(T, Plane + Vec(1.0, 2.0, 0.3), Vec(0.0, 1.0, 0.2), Carry, Kind);
				}
				Out.Made = Fx.Bits.size() - Before;
				if (bAfter)
				{
					// (an unrelated hit on another aircraft, which throws the kit's shared dice: its pieces as laid)
					const size_t From = Fx.Bits.size();
					Fx.AirHit(T, Vec(300.0, 80.0, 4100.0), Vec(0.0, -1.0, 0.0), Carry, "cannon20");
					Out.After.assign(Fx.Bits.begin() + static_cast<long>(From), Fx.Bits.end());
					Out.SeedsAfter = Fx.Seeds;
				}
			}
			const Battle::View V = Eye(Plane + Vec(-10.0, -60.0, 4.0), Plane);
			const int N = Fx.Draw(T, V);
			const uint64_t H = Hash(Fx, N);
			Out.All = (Out.All ^ H) * 1099511628211ull;
			if (F == 45)
			{
				Out.AtHalf = H;
			}
		}
		return Out;
	}

	bool SamePieces(const std::vector<Bit>& A, const std::vector<Bit>& B)
	{
		if (A.size() != B.size())
		{
			return false;
		}
		for (size_t I = 0; I < A.size(); ++I)
		{
			const Bit& X = A[I];
			const Bit& Y = B[I];
			if (X.From.X != Y.From.X || X.From.Y != Y.From.Y || X.From.Z != Y.From.Z || X.Vel.X != Y.Vel.X || X.Vel.Y != Y.Vel.Y || X.Vel.Z != Y.Vel.Z
				|| X.Born != Y.Born || X.Life != Y.Life || X.R0 != Y.R0 || X.R1 != Y.R1 || X.LumpSeed != Y.LumpSeed || X.Col.R != Y.Col.R || X.Alpha != Y.Alpha)
			{
				return false;
			}
		}
		return true;
	}

	void PartA(const char* Base)
	{
		std::printf("A. The spray at the hit (AirHit's Part, air_hit.spurt)\n");
		const JsonRecipe Off = With(Base, {});
		const JsonRecipe On = With(Base, { "battle-fx-air-spurt.json" });
		Check(Off.Get("air_hit.spurt.on", -1.0) == 0.0, "the shipped recipe has the row, off", Fmt("on %.0f", Off.Get("air_hit.spurt.on", -1.0)));
		Check(On.Get("air_hit.spurt.on", -1.0) == 1.0, "the overlay turns it on", Fmt("on %.0f", On.Get("air_hit.spurt.on", -1.0)));
		// (byte for byte with no Part or the row off: every frame of the hit, for each gun)
		for (const char* Kind : { "hmg", "cannon20", "rifle", "flak" })
		{
			const HitRun Was = HitPlay(Off, Kind, "", false);
			bool bSame = true;
			for (const char* Part : { "", "skin", "fuel", "oil", "coolant" })
			{
				const HitRun Off1 = HitPlay(Off, Kind, Part, true);
				bSame = bSame && Off1.All == Was.All && Off1.Made == Was.Made;
			}
			for (const char* Part : { "", "skin", "wing" })
			{
				const HitRun On1 = HitPlay(On, Kind, Part, true);
				bSame = bSame && On1.All == Was.All && On1.Made == Was.Made;
			}
			const std::string What = std::string("a ") + Kind + " hit: row off, or no spray part, frames as before";
			Check(bSame, What.c_str(), Hex(Was.All));
		}
		// (each part: its count, its life and its colour)
		struct Want
		{
			const char* Part;
			int Least, Most;
			double LastFrom, LastTo; // (the spray's span: the last puff's death after the strike, seconds)
		};
		for (const Want& W : { Want{ "fuel", 6, 12, 1.5, 4.2 }, Want{ "oil", 4, 6, 1.1, 1.9 }, Want{ "coolant", 2, 3, 1.1, 1.9 } })
		{
			int Least = 1 << 30, Most = 0;
			double SpanLeast = 1.0e9, SpanMost = 0.0, Luma = 0.0, Warm = 0.0;
			int Runs = 0;
			for (int K = 0; K < 40; ++K)
			{
				// (the same hit in two kits alike, without the part and with it: the second's extra pieces are the spray)
				Battle Fx, Bare;
				Begin(Fx, On);
				Begin(Bare, On);
				const double T = 10.0 + K * 0.731;
				const Vec At(37.0 * K, 11.0 * K, 4000.0);
				Bare.AirHit(T, At, Vec(0.0, 1.0, 0.0), Vec(140.0, 0.0, 0.0), "hmg", "");
				Fx.AirHit(T, At, Vec(0.0, 1.0, 0.0), Vec(140.0, 0.0, 0.0), "hmg", W.Part);
				const size_t Own = Bare.Bits.size();
				const int Spray = static_cast<int>(Fx.Bits.size()) - static_cast<int>(Own);
				Least = std::min(Least, Spray);
				Most = std::max(Most, Spray);
				double Last = 0.0;
				for (size_t I = Own; I < Fx.Bits.size(); ++I)
				{
					const Bit& B = Fx.Bits[I];
					Last = std::max(Last, B.Born + B.Life - T);
					Luma += 0.2126 * B.Col.R + 0.7152 * B.Col.G + 0.0722 * B.Col.B;
					Warm += B.Col.R - B.Col.B;
					++Runs;
				}
				SpanLeast = std::min(SpanLeast, Last);
				SpanMost = std::max(SpanMost, Last);
			}
			Luma /= std::max(1, Runs);
			Warm /= std::max(1, Runs);
			const std::string Count = std::string(W.Part) + ": puffs a spray within the row's";
			Check(Least >= W.Least && Most <= W.Most && Least < Most, Count.c_str(), Fmt("%.0f to %.0f", Least, Most));
			const std::string Life = std::string(W.Part) + ": the spray is gone this long after the strike";
			Check(SpanLeast >= W.LastFrom && SpanMost <= W.LastTo, Life.c_str(), Fmt("%.2f to %.2f s", SpanLeast, SpanMost));
			const std::string Colour = std::string(W.Part) + (std::string(W.Part) == "oil" ? ": dark and brown (linear luma, red over blue)" : ": white (linear luma)");
			const bool bColour = std::string(W.Part) == "oil" ? Luma < 0.15 && Warm > 0.02 : Luma > 0.75;
			Check(bColour, Colour.c_str(), Fmt("luma %.3f, red less blue %.3f", Luma, Warm));
		}
		// (at most most_a_frame sprays at one moment; the next moment starts afresh)
		{
			// (two kits alike, one hit without the part and one with: where the second lays more, a spray was laid)
			Battle Fx, Bare;
			Begin(Fx, On);
			Begin(Bare, On);
			int Sprays = 0;
			auto Both = [&](double T, const Vec& At)
			{
				const size_t From = Fx.Bits.size(), BareFrom = Bare.Bits.size();
				Fx.AirHit(T, At, Vec(0.0, 1.0, 0.0), Vec(140.0, 0.0, 0.0), "hmg", "coolant");
				Bare.AirHit(T, At, Vec(0.0, 1.0, 0.0), Vec(140.0, 0.0, 0.0), "hmg", "");
				return Fx.Bits.size() - From > Bare.Bits.size() - BareFrom ? 1 : 0;
			};
			for (int K = 0; K < 60; ++K)
			{
				Sprays += Both(5.0, Vec(11.0 * K, 3.0 * K, 4000.0));
			}
			const int Next = Both(5.0 + 1.0 / 30.0, Vec(1.0, 3.0, 4000.0));
			Check(Sprays == 40 && Next == 1, "60 sprays at one moment: 40 laid; the next frame's laid", Fmt("%.0f, then %.0f", Sprays, Next));
		}
		// (none of the kit's shared dice: an unrelated hit laid after it is the same to the byte with the Part and without)
		{
			bool bSame = true;
			uint64_t SeedsA = 0, SeedsB = 0;
			for (const char* Part : { "fuel", "oil", "coolant" })
			{
				const HitRun A = HitPlay(On, "hmg", "", true, true);
				const HitRun B = HitPlay(On, "hmg", Part, true, true);
				bSame = bSame && SamePieces(A.After, B.After) && A.SeedsAfter == B.SeedsAfter && !A.After.empty();
				SeedsA = A.SeedsAfter;
				SeedsB = B.SeedsAfter;
			}
			Check(bSame, "an unrelated hit after the spray: the same pieces, the same Seeds", Fmt("Seeds %.0f and %.0f", static_cast<double>(SeedsA % 100000), static_cast<double>(SeedsB % 100000)));
		}
		// (from 300 m a fuel spray still shows: its puffs are held to least_px)
		{
			Battle Fx;
			Begin(Fx, On);
			Fx.AirHit(1.0, Vec(0.0, 0.0, 4000.0), Vec(0.0, 1.0, 0.0), Vec(140.0, 0.0, 0.0), "hmg", "fuel");
			const Battle::View V = Eye(Vec(160.0, -300.0, 4000.0), Vec(160.0, 0.0, 4000.0));
			Fx.Draw(1.0, V);
			const int N = Fx.Draw(1.8, V);
			Check(N >= 5, "from 300 m, 0.8 s after a fuel hit, the spray is drawn", Fmt("%.0f puffs laid", N));
		}
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "air_ten_test <battle-fx.json> <folder of the overlays> <out folder>\n");
		return 2;
	}
	Overlays = Argv[2];
	std::printf("The air's dramatic moments: the effects kit's parts A to E, with the game's recipe\n");
	PartA(Argv[1]);
	std::printf("%d checks, %d failed\n", Checks, Failed);
	return Failed == 0 ? 0 : 1;
}
