// The effects kit's first batch of the owner's film details (Brian, 26 November: "i like the debris coming off the
// planes when hit", "lets try to add more visual dramatic things like that", "do 2-11"), each part with its launch
// word on (its overlay laid over the recipe, as the game's FBattleEffects lays it) and off, on the kit's own code and
// the game's own recipe, no engine:
//
//   A. flak by its size (-TheaterAirFlakKinds, battle-fx-flak.json): off, light and medium burst as heavy, the
//      control; on, heavy bursts exactly as before, medium a 6 to 8 m grey burst gone in about 5 s and light a 3 to
//      4 m grey puff gone in about 3 s, each lingering by its own row; a six-minute barrage of all three holds fewer
//      pieces than before, and the kit's shared dice are drawn as often as before.
//
//     Tools/fx_ten/run.sh [out folder]
//     fx_ten_test <Content/Effects folder> <out folder>
#include "fx_ten_common.h"

#include <chrono>
#include <functional>

namespace
{
	using FxTen::Battle;
	using FxTen::Bit;
	using FxTen::Vec;

	int Checks = 0, Failed = 0;

	void Check(bool bOk, const char* What, const std::string& Got)
	{
		++Checks;
		if (!bOk)
		{
			++Failed;
		}
		std::printf("  %s %-66s  %s\n", bOk ? "ok  " : "FAIL", What, Got.c_str());
	}

	std::string Say(const char* Format, double A = 0.0, double B = 0.0, double C = 0.0, double D = 0.0)
	{
		char Text[512];
		std::snprintf(Text, sizeof(Text), Format, A, B, C, D);
		return Text;
	}

	std::string Folder;

	/** The shipped recipe, and with the overlays named laid over it in order. */
	FxTen::Book Recipe(const std::vector<std::string>& Overlays = {})
	{
		FxTen::Book B;
		if (!B.Lay(Folder + "/battle-fx.json"))
		{
			std::exit(2);
		}
		for (const std::string& O : Overlays)
		{
			if (!B.Lay(Folder + "/" + O))
			{
				std::exit(2);
			}
		}
		return B;
	}

	struct Kit
	{
		Battle Fx;
		explicit Kit(const FxTen::Book& Book, unsigned Seed = 5u)
		{
			Fx.Setup(&Book, &FxTen::Flat, nullptr, Seed);
			Fx.SetWind(Vec(1.6, 0.4, 0.0));
		}
	};

	double RadiusAt(const Bit& B, double Age)
	{
		const double K = std::clamp(Age / std::max(B.Life, 1e-6), 0.0, 1.0);
		return B.R0 + (B.R1 - B.R0) * std::pow(K, B.Grow);
	}

	/** How wide the smoke laid since First stands at Now, across its widest way (its lights left out). */
	double SmokeAcross(const Battle& Fx, double Now, size_t First = 0)
	{
		double Most = 0.0;
		for (size_t I = First; I < Fx.Bits.size(); ++I)
		{
			const Bit& A = Fx.Bits[I];
			if (A.bLight || Now < A.Born || Now > A.Born + A.Life)
			{
				continue;
			}
			for (size_t J = I; J < Fx.Bits.size(); ++J)
			{
				const Bit& B = Fx.Bits[J];
				if (B.bLight || Now < B.Born || Now > B.Born + B.Life)
				{
					continue;
				}
				const double D = (Fx.Where(A, Now - A.Born) - Fx.Where(B, Now - B.Born)).Length() + RadiusAt(A, Now - A.Born) + RadiusAt(B, Now - B.Born);
				Most = std::max(Most, D);
			}
		}
		return Most;
	}

	/** When the last piece laid since First is gone. */
	double LastGone(const Battle& Fx, size_t First = 0)
	{
		double Last = 0.0;
		for (size_t I = First; I < Fx.Bits.size(); ++I)
		{
			Last = std::max(Last, Fx.Bits[I].Born + Fx.Bits[I].Life);
		}
		return Last;
	}

	double Px(double Metres, double Distance)
	{
		return Metres / Distance * FxTen::Looking(Vec(), Vec(0.0, 1.0, 0.0)).PixelsPerRadian;
	}

	// ----------------------------------------------------------------------------------------------------------------
	// A. Flak by its size

	/** One burst of Kind 600 m off at 1 s, its digest every second for 30 s. */
	std::vector<uint64_t> OneBurst(const FxTen::Book& Book, const char* Kind)
	{
		Kit K(Book);
		const Battle::View V = FxTen::Looking(Vec(0.0, 0.0, 1500.0), Vec(0.0, 600.0, 1500.0));
		std::vector<uint64_t> Out;
		for (int Frame = 0; Frame < 900; ++Frame)
		{
			const double Now = Frame / 30.0;
			if (Frame == 30)
			{
				K.Fx.AirFlak(Now, Vec(0.0, 600.0, 1500.0), Kind);
			}
			K.Fx.Draw(Now, V);
			if (Frame % 30 == 0)
			{
				Out.push_back(FxTen::Digest(K.Fx));
			}
		}
		return Out;
	}

	struct Barrage
	{
		size_t MostBits = 0, MostFlak = 0;
		int Bursts[3] = { 0, 0, 0 };
		uint64_t Seeds = 0;
		double DensestMs = 0.0, MeanMs = 0.0;
		std::vector<uint64_t> Digests;
	};

	/** Six minutes of flak round the eye: heavy every 0.9 s, medium every 1.4 s and light every 2.5 s, ragged, 300 m to
	 * 1 km off at the bombers' height (Kinds: which of the three fire). */
	Barrage SixMinutes(const FxTen::Book& Book, bool bHeavy, bool bMedium, bool bLight)
	{
		Kit K(Book);
		Barrage R;
		const Battle::View V = FxTen::Looking(Vec(0.0, 0.0, 1500.0), Vec(0.0, 600.0, 1500.0));
		TheaterFx::Dice When(77);
		double Next[3] = { 0.5, 0.8, 1.1 };
		const double Every[3] = { 0.9, 1.4, 2.5 };
		const bool bOn[3] = { bHeavy, bMedium, bLight };
		const char* Kinds[3] = { "heavy", "medium", "light" };
		for (int Frame = 0; Frame < 360 * 30; ++Frame)
		{
			const double Now = Frame / 30.0;
			for (int I = 0; I < 3; ++I)
			{
				while (Next[I] <= Now)
				{
					const double Along = When.U(-1.0, 1.0);
					const double Out = When.U(300.0, 1000.0);
					const double Up = When.U(-30.0, 30.0);
					const double Gap = When.U(0.6, 1.4);
					if (bOn[I])
					{
						K.Fx.AirFlak(Now, Vec(Along * 0.6 * Out, Out, 1500.0 + Up), Kinds[I]);
						++R.Bursts[I];
					}
					Next[I] += Every[I] * Gap;
				}
			}
			const auto Was = std::chrono::steady_clock::now();
			K.Fx.Draw(Now, V);
			const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Was).count();
			R.MeanMs += Ms / (360.0 * 30.0);
			if (K.Fx.Bits.size() > R.MostBits)
			{
				R.MostBits = K.Fx.Bits.size();
				R.DensestMs = Ms;
			}
			if (Frame % 30 == 0)
			{
				R.Digests.push_back(FxTen::Digest(K.Fx));
			}
		}
		R.MostFlak = R.MostBits;
		R.Seeds = K.Fx.Seeds;
		return R;
	}

	void FlakKinds()
	{
		std::printf("A. Flak by its size (-TheaterAirFlakKinds, battle-fx-flak.json)\n");
		const FxTen::Book Off = Recipe();
		const FxTen::Book On = Recipe({ "battle-fx-flak.json" });
		// (off: a light or a medium burst is a heavy one, piece for piece; the control that the kinds are drawn apart on)
		const std::vector<uint64_t> OffHeavy = OneBurst(Off, "heavy");
		Check(OneBurst(Off, "light") == OffHeavy && OneBurst(Off, "medium") == OffHeavy, "off: light and medium burst as heavy (as before)", "piece for piece, 30 s");
		Check(OneBurst(On, "light") != OffHeavy && OneBurst(On, "medium") != OffHeavy, "on: light and medium burst as their own rows (the control)", "differ from heavy");
		Check(OneBurst(On, "heavy") == OffHeavy, "on: heavy bursts exactly as before", "piece for piece, 30 s");
		// (each kind's size and life, a burst 600 m off at 1 s)
		const char* Kinds[3] = { "light", "medium", "heavy" };
		double Across[3][2] = {}, Gone[3] = {};
		for (int I = 0; I < 3; ++I)
		{
			Kit K(On);
			const Battle::View V = FxTen::Looking(Vec(0.0, 0.0, 1500.0), Vec(0.0, 600.0, 1500.0));
			K.Fx.Draw(0.0, V);
			K.Fx.AirFlak(1.0, Vec(0.0, 600.0, 1500.0), Kinds[I]);
			Gone[I] = LastGone(K.Fx) - 1.0;
			Across[I][0] = SmokeAcross(K.Fx, 3.0);
			Across[I][1] = SmokeAcross(K.Fx, 7.0);
		}
		// (sizes in the kit's own measure, the one heavy's "22 m" is written in: heavy's long_m 22 stands about 40 m
		// across its widest at 2 s, its lobes grown and stretched; each kind by the same measure, scaled to heavy's 22)
		const double ByHeavy = Across[2][0] > 1e-6 ? 22.0 / Across[2][0] : 0.0;
		Check(Across[0][0] * ByHeavy >= 2.5 && Across[0][0] * ByHeavy <= 5.0, "light: a 3 to 4 m puff at 2 s (as heavy's 22 m is measured)", Say("%.1f m", Across[0][0] * ByHeavy));
		Check(Across[1][0] * ByHeavy >= 5.5 && Across[1][0] * ByHeavy <= 9.5, "medium: a 6 to 8 m burst at 2 s (as heavy's 22 m is measured)", Say("%.1f m", Across[1][0] * ByHeavy));
		Check(Across[2][0] >= 30.0 && Across[2][0] <= 60.0, "heavy: as it was (its 22 m stands 30 to 60 m at its widest at 2 s)", Say("%.1f m", Across[2][0]));
		Check(Gone[0] <= 6.0, "light: gone by 6 s, lingering to linger.flak.by.light", Say("%.1f s", Gone[0]));
		Check(Gone[1] <= 9.5, "medium: gone by 9.5 s, lingering to linger.flak.by.medium", Say("%.1f s", Gone[1]));
		Check(Gone[2] >= 24.0, "heavy: lingering to 25 s as before", Say("%.1f s", Gone[2]));
		std::printf("     sizes: light %.1f m at 2 s, %.1f m at 6 s; medium %.1f m, %.1f m; heavy %.1f m, %.1f m; gone after %.1f, %.1f, %.1f s\n",
			Across[0][0], Across[0][1], Across[1][0], Across[1][1], Across[2][0], Across[2][1], Gone[0], Gone[1], Gone[2]);
		std::printf("     on the screen (1600 px, 42 degrees), light / medium / heavy at 2 s: %.0f / %.0f / %.0f px at 300 m, %.0f / %.0f / %.0f px at 1 km\n",
			Px(Across[0][0], 300.0), Px(Across[1][0], 300.0), Px(Across[2][0], 300.0), Px(Across[0][0], 1000.0), Px(Across[1][0], 1000.0), Px(Across[2][0], 1000.0));
		// (the linger rows: by name only where the row is; asked by a name with none, a burst would linger not at all,
		// which is why heavy is never asked by its name)
		{
			Kit K(On);
			Check(K.Fx.LingerOf("flak", "light").bOn && K.Fx.LingerOf("flak", "medium").bOn && K.Fx.LingerOf("flak").bOn, "on: light, medium and the kind's own linger rows are on", "3 of 3");
			Check(!K.Fx.LingerOf("flak", "heavy").bOn, "control: asked by a name with no row, no linger (heavy never is)", "off");
		}
		// (six minutes of all three, and of heavy alone)
		const Barrage BOff = SixMinutes(Off, true, true, true);
		const Barrage BOn = SixMinutes(On, true, true, true);
		Check(BOn.MostBits <= BOff.MostBits, "a six-minute barrage of all three holds no more pieces than before", Say("%.0f at most, %.0f before", static_cast<double>(BOn.MostBits), static_cast<double>(BOff.MostBits)));
		Check(BOn.Seeds == BOff.Seeds, "the kit's shared dice drawn as often as before", Say("%.0f draws", static_cast<double>(BOn.Seeds)));
		std::printf("     the barrage: %d heavy, %d medium, %d light bursts in 6 min; most pieces %zu (before %zu)\n", BOn.Bursts[0], BOn.Bursts[1], BOn.Bursts[2], BOn.MostBits, BOff.MostBits);
		std::printf("     time: the densest frame's Draw, %.3f ms on (%zu pieces), %.3f ms off (%zu pieces); a frame's mean %.3f ms on, %.3f ms off\n", BOn.DensestMs, BOn.MostBits, BOff.DensestMs, BOff.MostBits, BOn.MeanMs, BOff.MeanMs);
		const Barrage HOff = SixMinutes(Off, true, false, false);
		const Barrage HOn = SixMinutes(On, true, false, false);
		Check(HOn.Digests == HOff.Digests, "on: six minutes of heavy flak alone, every piece as before", Say("%.0f samples", static_cast<double>(HOn.Digests.size())));
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "fx_ten_test <Content/Effects folder> <out folder>\n");
		return 2;
	}
	Folder = Argv[1];
	FlakKinds();
	std::printf("fx_ten_test: %d checks, %d failed\n", Checks, Failed);
	if (Failed == 0)
	{
		std::printf("ALL CHECKS PASSED\n");
	}
	return Failed == 0 ? 0 : 1;
}
