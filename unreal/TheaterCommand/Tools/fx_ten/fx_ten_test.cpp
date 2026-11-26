// The effects kit's first batch of the owner's film details (Brian, 26 November: "i like the debris coming off the
// planes when hit", "lets try to add more visual dramatic things like that", "do 2-11"), each part with its launch
// word on (its overlay laid over the recipe, as the game's FBattleEffects lays it) and off, on the kit's own code and
// the game's own recipe, no engine:
//
//   A. flak by its size (-TheaterAirFlakKinds, battle-fx-flak.json): off, light and medium burst as heavy, the
//      control; on, heavy bursts exactly as before, medium a 6 to 8 m grey burst gone in about 5 s and light a 3 to
//      4 m grey puff gone in about 3 s, each lingering by its own row; a six-minute barrage of all three holds fewer
//      pieces than before, and the kit's shared dice are drawn as often as before.
//   B. no false brass at sea (-TheaterSeaNoFalseCases, battle-fx-sea-cases.json): off, a turret on the water throws a
//      field gun's case, the fault; on, none on the water, and the same guns on earth lay every piece as before.
//   C. the barrel's wisp (-TheaterLandBarrelSmoke, battle-fx-barrel.json): a thin, faint wisp from 3 to 4 s after the
//      shot, held to 2 px; none past 300 m; one a gun at once; its own dice and its own room: twelve guns and forty
//      lay every piece as before with it, forty hold 16 wisps at most.
//   D. the breech's puff (the same word and overlay, steam.breech): who puffs (AfterShotOf), and a small grey-brown
//      puff gone in under 3 s, held to 3 px.
//   E. hatch fumes and the odd case (-TheaterLandHatchFumes, battle-fx-hatch.json): fumes every second or third shot
//      with the hatch open, a case one shot in eight when stopped; none shut, nor without the word.
//   F. cases over the side (-TheaterLandCasesOut, battle-fx-cases-out.json): a battery of six open guns for five
//      minutes throws about 7 cases in 10, each lying 180 to 300 s, within most_lying.
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

	// ----------------------------------------------------------------------------------------------------------------
	// Shared by B to F: a fight played at 30 frames a second, sampled every second

	struct Fight
	{
		std::vector<uint64_t> Digests;
		std::vector<std::vector<uint64_t>> Heaps;
		size_t MostBits = 0, MostLying = 0;
		uint64_t Seeds = 0;
		double DensestMs = 0.0, MeanMs = 0.0;
	};

	/** Frames of a fight seen from Eye toward Look, Script(Fx, Now, Frame) each frame before the Draw. */
	Fight Play(const FxTen::Book& Book, const Vec& Eye, const Vec& Look, int Frames, const std::function<void(Battle&, double, int)>& Script, bool bHeaps = true)
	{
		Kit K(Book);
		Fight R;
		const Battle::View V = FxTen::Looking(Eye, Look);
		K.Fx.Draw(0.0, V);
		for (int Frame = 1; Frame <= Frames; ++Frame)
		{
			const double Now = Frame / 30.0;
			Script(K.Fx, Now, Frame);
			const auto Was = std::chrono::steady_clock::now();
			K.Fx.Draw(Now, V);
			const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Was).count();
			R.MeanMs += Ms / Frames;
			if (K.Fx.Bits.size() > R.MostBits)
			{
				R.MostBits = K.Fx.Bits.size();
				R.DensestMs = Ms;
			}
			R.MostLying = std::max(R.MostLying, K.Fx.CaseUntil.size());
			if (Frame % 30 == 0)
			{
				R.Digests.push_back(FxTen::Digest(K.Fx));
				if (bHeaps)
				{
					R.Heaps.push_back(FxTen::Heap(K.Fx));
				}
			}
		}
		R.Seeds = K.Fx.Seeds;
		return R;
	}

	/** Of every sample, how many of Small's pieces Big has not (Small's pieces all in Big: none). */
	size_t MissingAll(const Fight& Small, const Fight& Big)
	{
		size_t Miss = 0;
		for (size_t I = 0; I < Small.Heaps.size() && I < Big.Heaps.size(); ++I)
		{
			Miss += FxTen::Missing(Small.Heaps[I], Big.Heaps[I]);
		}
		return Miss;
	}

	/** A spent case on the ground or in the air (GroundCase's piece). */
	bool IsCase(const Bit& B)
	{
		return B.Form == Bit::Shape::Plate && B.bThrown && B.Gravity == 9.8;
	}

	// ----------------------------------------------------------------------------------------------------------------
	// B. No false brass at sea

	void SeaCases()
	{
		std::printf("B. No false brass at sea (-TheaterSeaNoFalseCases, battle-fx-sea-cases.json)\n");
		const FxTen::Book Off = Recipe();
		const FxTen::Book On = Recipe({ "battle-fx-sea-cases.json" });
		// (a battleship's broadside close to the eye: three turrets firing every 8 s for a minute, the sea scene's rows,
		// the howitzer's at 180 mm and over and the tank gun's below, on "water"; the same on "earth", the control)
		auto Broadside = [](const char* Ground)
		{
			return [Ground](Battle& Fx, double Now, int Frame)
			{
				if (Frame % 240 == 15)
				{
					for (int Turret = 0; Turret < 3; ++Turret)
					{
						Fx.Muzzle(Now, Vec(-12.0 + 12.0 * Turret, 0.0, 6.0), Vec(0.1, 1.0, 0.3), Turret == 1 ? "tank_gun" : "howitzer", Ground);
					}
				}
			};
		};
		const Vec Eye(0.0, -22.0, 8.0), Look(0.0, 10.0, 6.0);
		const Fight WOff = Play(Off, Eye, Look, 1800, Broadside("water"));
		const Fight WOn = Play(On, Eye, Look, 1800, Broadside("water"));
		const Fight EOff = Play(Off, Eye, Look, 1800, Broadside("earth"));
		const Fight EOn = Play(On, Eye, Look, 1800, Broadside("earth"));
		// (the cases off lays on the water: the pieces off has that on has not)
		size_t OffOnly = 0, OffOnlyCases = 0;
		{
			Kit KOff(Off), KOn(On);
			const Battle::View V = FxTen::Looking(Eye, Look);
			KOff.Fx.Draw(0.0, V);
			KOn.Fx.Draw(0.0, V);
			for (Kit* K : { &KOff, &KOn })
			{
				for (int Turret = 0; Turret < 3; ++Turret)
				{
					K->Fx.Muzzle(0.5, Vec(-12.0 + 12.0 * Turret, 0.0, 6.0), Vec(0.1, 1.0, 0.3), Turret == 1 ? "tank_gun" : "howitzer", "water");
				}
			}
			const std::vector<uint64_t> HOn = FxTen::Heap(KOn.Fx);
			for (const Bit& B : KOff.Fx.Bits)
			{
				if (!std::binary_search(HOn.begin(), HOn.end(), FxTen::PieceHash(B)))
				{
					++OffOnly;
					OffOnlyCases += IsCase(B) ? 1u : 0u;
				}
			}
		}
		Check(OffOnlyCases > 0 && OffOnly == OffOnlyCases, "off: a turret on the water throws a field gun's case (the fault)", Say("%.0f cases", static_cast<double>(OffOnlyCases)));
		Check(MissingAll(WOn, WOff) == 0, "on: on the water, every piece on lays was laid off", Say("%.0f missing", static_cast<double>(MissingAll(WOn, WOff))));
		size_t CasesOn = 0;
		{
			Kit K(On);
			K.Fx.Draw(0.0, FxTen::Looking(Eye, Look));
			for (int Turret = 0; Turret < 3; ++Turret)
			{
				K.Fx.Muzzle(0.5, Vec(-12.0 + 12.0 * Turret, 0.0, 6.0), Vec(0.1, 1.0, 0.3), Turret == 1 ? "tank_gun" : "howitzer", "water");
			}
			for (const Bit& B : K.Fx.Bits)
			{
				CasesOn += IsCase(B) ? 1u : 0u;
			}
		}
		Check(CasesOn == 0 && WOn.MostLying == 0, "on: no case thrown on the water", Say("%.0f cases", static_cast<double>(CasesOn)));
		Check(EOn.Digests == EOff.Digests, "on: the same guns on earth, every piece as before", Say("%.0f samples", static_cast<double>(EOn.Digests.size())));
		Check(WOn.Seeds == WOff.Seeds && EOn.Seeds == EOff.Seeds, "the kit's shared dice drawn as often as before", Say("%.0f draws", static_cast<double>(WOn.Seeds)));
		std::printf("     a minute's broadside, 3 turrets every 8 s: most pieces %zu off, %zu on; lying cases most %zu off, %zu on\n", WOff.MostBits, WOn.MostBits, WOff.MostLying, WOn.MostLying);
	}

	// ----------------------------------------------------------------------------------------------------------------
	// C. The barrel's wisp

	/** The pieces one shot lays, off and on: how many more on, and those (the last laid). */
	std::vector<Bit> WispOf(const FxTen::Book& On, const FxTen::Book& Off, const char* Gun, const Vec& Eye, const Vec& At, double Now = 1.0)
	{
		Kit KOff(Off), KOn(On);
		const Battle::View V = FxTen::Looking(Eye, At);
		KOff.Fx.Draw(0.0, V);
		KOn.Fx.Draw(0.0, V);
		KOff.Fx.Muzzle(Now, At, Vec(0.0, 1.0, 0.03), Gun, "earth");
		KOn.Fx.Muzzle(Now, At, Vec(0.0, 1.0, 0.03), Gun, "earth");
		std::vector<Bit> Out;
		for (size_t I = KOff.Fx.Bits.size(); I < KOn.Fx.Bits.size(); ++I)
		{
			Out.push_back(KOn.Fx.Bits[I]);
		}
		return Out;
	}

	void BarrelWisp()
	{
		std::printf("C. The barrel's wisp (-TheaterLandBarrelSmoke, battle-fx-barrel.json: muzzle.<gun>.after_wisp)\n");
		const FxTen::Book Off = Recipe();
		const FxTen::Book On = Recipe({ "battle-fx-barrel.json" });
		const Vec Eye(-30.0, -25.0, 2.0);
		const char* Guns[3] = { "tank_gun", "at_gun", "howitzer" };
		bool bSizes = true, bWhen = true, bFloor = true, bFew = true;
		double FirstAt[3] = {}, LastAt[3] = {}, Widest = 0.0, Thickest = 0.0;
		int Count[3] = {};
		for (int G = 0; G < 3; ++G)
		{
			const std::vector<Bit> W = WispOf(On, Off, Guns[G], Eye, Vec(0.0, 0.0, 2.0));
			Count[G] = static_cast<int>(W.size());
			bFew = bFew && W.size() >= 10 && W.size() <= 15;
			FirstAt[G] = 1e9;
			for (const Bit& B : W)
			{
				FirstAt[G] = std::min(FirstAt[G], B.Born - 1.0);
				LastAt[G] = std::max(LastAt[G], B.Born + B.Life - 1.0);
				Widest = std::max(Widest, B.R1);
				Thickest = std::max(Thickest, B.Alpha);
				bSizes = bSizes && B.R0 >= 0.03 && B.R1 <= 0.5;
				bFloor = bFloor && B.LeastPx >= 2.0;
			}
			bWhen = bWhen && FirstAt[G] >= (G == 2 ? 5.0 : 3.0) && LastAt[G] <= (G == 2 ? 6.5 + 7.0 + 3.0 + 0.5 : 4.0 + 6.0 + 2.6 + 0.5);
		}
		Check(bFew, "on: one shot of each gun, 10 to 15 puffs of wisp", Say("%.0f, %.0f, %.0f", Count[0], Count[1], Count[2]));
		Check(bWhen, "on: from 3 to 4 s after the shot (the howitzer 5 to 6.5 s), gone by 13 s", Say("from %.1f, %.1f, %.1f s; gone %.1f s", FirstAt[0], FirstAt[1], FirstAt[2], std::max(LastAt[0], std::max(LastAt[1], LastAt[2]))));
		Check(bSizes && Thickest <= 0.501, "on: thin, 0.03 m at the bore to 0.5 m, see-through (alpha 0.5 at most)", Say("widest %.2f m, alpha at most %.2f", Widest, Thickest));
		Check(bFloor, "on: each puff held to 2 px across, thinner to match (no twinkle)", "least_px 2");
		Check(WispOf(Off, Off, "tank_gun", Eye, Vec(0.0, 0.0, 2.0)).empty(), "off: no wisp (the recipe has no after_wisp)", "none");
		const Vec FarEye(-280.0, -150.0, 3.0);
		Check(WispOf(On, Off, "tank_gun", FarEye, Vec(0.0, 0.0, 2.0)).empty(), "on: a gun 318 m from the eye, past within_m 300: no wisp", "none");
		Check(!WispOf(On, Off, "tank_gun", Vec(-200.0, -150.0, 3.0), Vec(0.0, 0.0, 2.0)).empty(), "on: the same gun at 250 m: a wisp (the control)", "laid");
		// (one gun firing again while its wisp still curls: no second; another gun beside it: its own)
		{
			Kit K(On);
			K.Fx.Draw(0.0, FxTen::Looking(Eye, Vec(0.0, 0.0, 2.0)));
			K.Fx.Muzzle(1.0, Vec(0.0, 0.0, 2.0), Vec(0.0, 1.0, 0.03), "tank_gun", "earth");
			const size_t After1 = K.Fx.BarrelWisps.size();
			K.Fx.Muzzle(6.0, Vec(0.5, 0.2, 2.0), Vec(0.0, 1.0, 0.03), "tank_gun", "earth");
			const size_t After2 = K.Fx.BarrelWisps.size();
			K.Fx.Muzzle(6.5, Vec(12.0, 0.0, 2.0), Vec(0.0, 1.0, 0.03), "tank_gun", "earth");
			const size_t After3 = K.Fx.BarrelWisps.size();
			Check(After1 == 1 && After2 == 1 && After3 == 2, "on: one wisp a gun at once (per_gun 1); the gun beside her her own", Say("%.0f, %.0f, %.0f", static_cast<double>(After1), static_cast<double>(After2), static_cast<double>(After3)));
		}
		// (its own dice: a shot's wisp from the place and the moment, the same whatever came before)
		{
			Kit A(On), B(On);
			A.Fx.Draw(0.0, FxTen::Looking(Eye, Vec(0.0, 0.0, 2.0)));
			B.Fx.Draw(0.0, FxTen::Looking(Eye, Vec(0.0, 0.0, 2.0)));
			B.Fx.Muzzle(0.5, Vec(40.0, 30.0, 1.0), Vec(0.0, 1.0, 0.4), "howitzer", "earth");
			const size_t AFirst = A.Fx.Bits.size();
			A.Fx.Muzzle(1.0, Vec(0.0, 0.0, 2.0), Vec(0.0, 1.0, 0.03), "tank_gun", "earth");
			const size_t BFirst = B.Fx.Bits.size();
			B.Fx.Muzzle(1.0, Vec(0.0, 0.0, 2.0), Vec(0.0, 1.0, 0.03), "tank_gun", "earth");
			const size_t AN = A.Fx.Bits.size() - AFirst, BN = B.Fx.Bits.size() - BFirst;
			bool bSame = AN == BN;
			for (size_t I = 1; bSame && I <= 12 && I <= AN; ++I)
			{
				bSame = FxTen::PieceHash(A.Fx.Bits[A.Fx.Bits.size() - I]) == FxTen::PieceHash(B.Fx.Bits[B.Fx.Bits.size() - I]);
			}
			Check(bSame, "on: a wisp's dice its own (the same after another gun's shot)", "piece for piece");
		}
		// (an exchange: Guns guns, tanks, anti-tank guns and howitzers, round the eye 20 to 450 m off, each firing every 6
		// to 10 s for five minutes; off and on)
		auto Exchange = [](int Many)
		{
			return [Many](Battle& Fx, double Now, int Frame)
			{
				static thread_local std::vector<double> Next;
				if (Frame == 1)
				{
					Next.assign(static_cast<size_t>(Many), 0.0);
					for (int I = 0; I < Many; ++I)
					{
						Next[static_cast<size_t>(I)] = 0.2 + 7.6 * I / Many;
					}
				}
				for (int I = 0; I < Many; ++I)
				{
					double& At = Next[static_cast<size_t>(I)];
					if (At <= Now)
					{
						TheaterFx::Dice D(static_cast<uint64_t>(I) * 977ull + static_cast<uint64_t>(Frame));
						const double Ang = 6.283 * I / Many;
						const double R = 20.0 + 430.0 * I / Many;
						Fx.Muzzle(Now, Vec(R * std::cos(Ang), R * std::sin(Ang), 1.5), Vec(-std::cos(Ang), -std::sin(Ang), I % 3 == 2 ? 0.4 : 0.03), I % 3 == 0 ? "tank_gun" : I % 3 == 1 ? "at_gun" : "howitzer", "earth");
						At += D.U(6.0, 10.0);
					}
				}
			};
		};
		const Vec FEye(0.0, 0.0, 3.0), FLook(100.0, 60.0, 2.0);
		size_t MostWisps = 0;
		auto Counting = [&MostWisps](const std::function<void(Battle&, double, int)>& Script)
		{
			return [&MostWisps, Script](Battle& Fx, double Now, int Frame)
			{
				Script(Fx, Now, Frame);
				MostWisps = std::max(MostWisps, Fx.BarrelWisps.size());
			};
		};
		// (twelve guns: the screen has room, and the wisps only add)
		const Fight TOff = Play(Off, FEye, FLook, 9000, Exchange(12));
		const Fight TOn = Play(On, FEye, FLook, 9000, Counting(Exchange(12)));
		const size_t TWisps = MostWisps;
		Check(TWisps > 0 && MissingAll(TOff, TOn) == 0, "on: twelve guns for five minutes, every piece off laid on (the wisp adds only)", Say("%.0f missing, %.0f wisps at most", static_cast<double>(MissingAll(TOff, TOn)), static_cast<double>(TWisps)));
		Check(TOn.Seeds == TOff.Seeds, "the kit's shared dice drawn as often as before", Say("%.0f draws", static_cast<double>(TOn.Seeds)));
		// (forty: the densest case; the screen past its room off already, so the wisps give way)
		MostWisps = 0;
		const Fight FOff = Play(Off, FEye, FLook, 9000, Exchange(40));
		const Fight FOn = Play(On, FEye, FLook, 9000, Counting(Exchange(40)));
		Check(MostWisps <= 16, "on: forty guns for five minutes: at most 16 wisps at once (most)", Say("%.0f at most", static_cast<double>(MostWisps)));
		Check(FOn.MostBits <= FOff.MostBits + 16 * 15, "on: the densest frame no fuller than off and every wisp's puffs", Say("%.0f pieces, %.0f off", static_cast<double>(FOn.MostBits), static_cast<double>(FOff.MostBits)));
		// (the wisps in a room of their own: the gates that read how full the screen is read it without them, so even
		// here every piece off is laid on)
		Check(MissingAll(FOff, FOn) == 0, "on: forty guns, every piece off laid on (the wisps' own room)", Say("%.0f missing", static_cast<double>(MissingAll(FOff, FOn))));
		Check(FOn.Seeds == FOff.Seeds, "the kit's shared dice drawn as often as before (forty)", Say("%.0f draws", static_cast<double>(FOn.Seeds)));
		size_t Pieces = 0;
		for (const std::vector<uint64_t>& H : FOff.Heaps)
		{
			Pieces += H.size();
		}
		std::printf("     twelve guns: most pieces %zu off, %zu on; forty guns: most pieces %zu off, %zu on, most wisps at once %zu; off's pieces not laid on %.2f%%\n", TOff.MostBits, TOn.MostBits, FOff.MostBits, FOn.MostBits, MostWisps, 100.0 * static_cast<double>(MissingAll(FOff, FOn)) / static_cast<double>(std::max<size_t>(Pieces, 1)));
		std::printf("     time: the densest frame's Draw, %.3f ms on (%zu pieces), %.3f ms off (%zu pieces); a frame's mean %.3f ms on, %.3f ms off\n", FOn.DensestMs, FOn.MostBits, FOff.DensestMs, FOff.MostBits, FOn.MeanMs, FOff.MeanMs);
		std::printf("     on the screen (1600 px, 42 degrees): a wisp puff 0.03 to %.2f m is %.1f to %.1f px at 40 m, %.1f to %.1f px at 300 m (held to 2)\n", Widest, Px(0.06, 40.0), Px(2.0 * Widest, 40.0), Px(0.06, 300.0), Px(2.0 * Widest, 300.0));
	}

	// ----------------------------------------------------------------------------------------------------------------
	// D. The breech's puff

	TheaterFx::AfterShotAsk Ask(const char* Kind, bool bBreech, bool bLoader, bool bHatch, double HatchOpen, bool bStopped, int Shot, bool bBarrel, bool bFumes, bool bOut)
	{
		TheaterFx::AfterShotAsk A;
		A.Kind = Kind;
		A.bBreech = bBreech;
		A.bLoader = bLoader;
		A.bHatch = bHatch;
		A.HatchOpen = HatchOpen;
		A.bStopped = bStopped;
		A.Shot = Shot;
		A.bBarrelSmoke = bBarrel;
		A.bHatchFumes = bFumes;
		A.bCasesOut = bOut;
		return A;
	}

	/** A Steam of Kind at the eye's 25 m: its pieces over Seconds + 4 s, how many, the widest, the thickest, when the last goes. */
	struct Puffed
	{
		size_t Pieces = 0;
		double Widest = 0.0, Thickest = 0.0, Gone = 0.0, LeastPx = 1e9;
		uint64_t Seeds = 0, SeedsWithout = 0;
	};

	Puffed PuffOf(const FxTen::Book& Book, const char* Kind, double Seconds, double Across)
	{
		Puffed P;
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			Kit K(Book);
			const Battle::View V = FxTen::Looking(Vec(-20.0, -15.0, 2.0), Vec(0.0, 0.0, 1.5));
			K.Fx.Draw(0.0, V);
			std::vector<uint64_t> Seen;
			for (int Frame = 1; Frame <= 180; ++Frame)
			{
				const double Now = Frame / 30.0;
				if (Frame == 15 && Pass == 0)
				{
					K.Fx.Steam(Now, 7, Vec(0.0, 0.0, 1.5), Vec(-0.7, 0.0, 0.7), Across, Seconds, Kind);
				}
				K.Fx.Draw(Now, V);
				for (const Bit& B : K.Fx.Bits)
				{
					const uint64_t H = FxTen::PieceHash(B);
					if (std::find(Seen.begin(), Seen.end(), H) == Seen.end())
					{
						Seen.push_back(H);
						P.Widest = std::max(P.Widest, B.R1);
						P.Thickest = std::max(P.Thickest, B.Alpha);
						P.Gone = std::max(P.Gone, B.Born + B.Life - 0.5);
						P.LeastPx = std::min(P.LeastPx, B.LeastPx);
					}
				}
			}
			if (Pass == 0)
			{
				P.Pieces = Seen.size();
				P.Seeds = K.Fx.Seeds;
			}
			else
			{
				P.SeedsWithout = K.Fx.Seeds;
			}
		}
		return P;
	}

	void BreechPuff()
	{
		std::printf("D. The breech's puff (-TheaterLandBarrelSmoke, battle-fx-barrel.json: steam.breech)\n");
		using TheaterFx::AfterShotOf;
		Check(AfterShotOf(Ask("gun", true, true, false, 0.0, true, 1, true, false, false)).bBreechPuff, "a towed gun with a breech, the word given: a puff", "puff");
		Check(!AfterShotOf(Ask("gun", true, true, false, 0.0, true, 1, false, true, true)).bBreechPuff, "the same without the word: none", "none");
		Check(AfterShotOf(Ask("tank", true, true, false, 0.0, true, 1, true, false, false)).bBreechPuff, "an open self-propelled gun with a breech: a puff", "puff");
		Check(!AfterShotOf(Ask("tank", false, true, false, 0.0, true, 1, true, false, false)).bBreechPuff, "an open gun with no breech in her model: none", "none");
		Check(!AfterShotOf(Ask("tank", true, false, true, 1.0, true, 1, true, false, false)).bBreechPuff, "a closed tank: none (her breech is inside)", "none");
		Check(!AfterShotOf(Ask("car", true, false, true, 1.0, true, 1, true, true, true)).bBreechPuff, "an armoured car: nothing at all", "none");
		const FxTen::Book Off = Recipe();
		const FxTen::Book On = Recipe({ "battle-fx-barrel.json" });
		Check(On.Numbers.count("steam.breech.reach_m.0") == 1 && Off.Numbers.count("steam.breech.reach_m.0") == 0, "the row steam.breech on with the word, none without", "on / none");
		const Puffed P = PuffOf(On, "breech", 0.6, 0.25);
		Check(P.Pieces >= 5 && P.Pieces <= 30, "on: a small puff, 5 to 30 rags", Say("%.0f rags", static_cast<double>(P.Pieces)));
		Check(P.Widest <= 1.5 && P.Thickest <= 0.46, "on: no wider than 1.5 m, see-through (alpha 0.45 at most)", Say("widest %.2f m, alpha %.2f", P.Widest, P.Thickest));
		Check(P.Gone <= 3.0, "on: gone within 3 s of the case coming out", Say("%.1f s", P.Gone));
		Check(P.LeastPx >= 3.0, "on: each rag held to 3 px across, thinner to match", Say("least_px %.0f", P.LeastPx));
		Check(P.Seeds == P.SeedsWithout, "the kit's shared dice drawn as often as without it", Say("%.0f draws", static_cast<double>(P.Seeds)));
		std::printf("     on the screen (1600 px, 42 degrees): the puff %.2f m across its widest rag is %.0f px at 25 m, %.0f px at 150 m (the scene's gate)\n", 2.0 * P.Widest, Px(2.0 * P.Widest, 25.0), Px(2.0 * P.Widest, 150.0));
	}

	// ----------------------------------------------------------------------------------------------------------------
	// E. A closed tank's hatch fumes and the odd case

	void HatchFumes()
	{
		std::printf("E. Hatch fumes and the odd case (-TheaterLandHatchFumes, battle-fx-hatch.json)\n");
		using TheaterFx::AfterShotOf;
		int Fumes = 0, CasesMoving = 0, CasesStopped = 0, Shut = 0, NoWord = 0, Spg = 0;
		std::string Which;
		for (int Shot = 1; Shot <= 40; ++Shot)
		{
			const TheaterFx::AfterShot M = AfterShotOf(Ask("tank", true, false, true, 1.0, false, Shot, false, true, false));
			const TheaterFx::AfterShot S = AfterShotOf(Ask("tank", true, false, true, 1.0, true, Shot, false, true, false));
			const TheaterFx::AfterShot C = AfterShotOf(Ask("tank", true, false, true, 0.3, true, Shot, true, true, true));
			const TheaterFx::AfterShot N = AfterShotOf(Ask("tank", true, false, true, 1.0, true, Shot, true, false, true));
			const TheaterFx::AfterShot O = AfterShotOf(Ask("tank", true, true, true, 1.0, true, Shot, false, true, false));
			Fumes += M.bHatchFumes ? 1 : 0;
			if (M.bHatchFumes && Shot <= 12)
			{
				Which += (Which.empty() ? "" : ",") + std::to_string(Shot);
			}
			CasesMoving += M.bHatchCase ? 1 : 0;
			CasesStopped += S.bHatchCase ? 1 : 0;
			Shut += C.bHatchFumes || C.bHatchCase ? 1 : 0;
			NoWord += N.bHatchFumes || N.bHatchCase ? 1 : 0;
			Spg += O.bHatchFumes || O.bHatchCase ? 1 : 0;
		}
		Check(Fumes == 16, "hatch open: fumes every second or third shot (16 of 40)", Say("%.0f of 40", Fumes) + " (" + Which + ", ...)");
		Check(CasesStopped == 5 && CasesMoving == 0, "a case pitched out one shot in eight, only when stopped", Say("%.0f of 40 stopped, %.0f moving", CasesStopped, CasesMoving));
		Check(Shut == 0 && NoWord == 0 && Spg == 0, "hatch shut, the word not given, or an open gun: none", Say("%.0f, %.0f, %.0f", Shut, NoWord, Spg));
		const FxTen::Book Off = Recipe();
		const FxTen::Book On = Recipe({ "battle-fx-hatch.json" });
		const Puffed P = PuffOf(On, "hatch_fumes", 2.0, 0.3);
		Check(P.Pieces >= 5 && P.Widest <= 1.6 && P.Thickest <= 0.35, "on: a faint wisp out of the hatch, no wider than 1.6 m, alpha 0.34", Say("%.0f rags, widest %.2f m, alpha %.2f", static_cast<double>(P.Pieces), P.Widest, P.Thickest));
		Check(P.Gone <= 5.0 && P.LeastPx >= 3.0, "on: gone within 5 s, each rag held to 3 px", Say("%.1f s", P.Gone));
		Check(P.Seeds == P.SeedsWithout, "the kit's shared dice drawn as often as without it", Say("%.0f draws", static_cast<double>(P.Seeds)));
		// (the case pitched out: by the row ground_cases.tank; none without it)
		auto Pitch = [](const FxTen::Book& Book, const Vec& Eye, std::vector<Bit>& Out)
		{
			Kit K(Book);
			K.Fx.Draw(0.0, FxTen::Looking(Eye, Vec(0.0, 0.0, 2.3)));
			const uint64_t Was = K.Fx.Seeds;
			K.Fx.ThrowCase(1.0, Vec(0.0, 0.0, 2.6), Vec(1.0, 0.2, 0.0), "tank", 4242);
			for (const Bit& B : K.Fx.Bits)
			{
				Out.push_back(B);
			}
			return K.Fx.Seeds == Was;
		};
		std::vector<Bit> COff, COn, CFar;
		Pitch(Off, Vec(-20.0, -15.0, 2.0), COff);
		const bool bOwn = Pitch(On, Vec(-20.0, -15.0, 2.0), COn);
		Pitch(On, Vec(-40.0, -30.0, 2.0), CFar);
		Check(COff.empty(), "off: no row, no case", "none");
		const double Long = COn.empty() ? 0.0 : 2.0 * COn[0].R0 * COn[0].Stretch;
		Check(COn.size() == 1 && IsCase(COn[0]) && Long >= 0.35 && Long <= 0.5, "on: one case, 0.35 to 0.5 m long", Say("%.0f, %.2f m", static_cast<double>(COn.size()), Long));
		Check(!COn.empty() && COn[0].Life - COn[0].Land2 >= 180.0 && COn[0].Life - COn[0].Land2 <= 240.5, "on: it lies 180 to 240 s", Say("%.0f s", COn.empty() ? 0.0 : COn[0].Life - COn[0].Land2));
		Check(CFar.empty(), "on: 50 m from the eye (past ground_cases.within_m 40): none", "none");
		Check(bOwn, "a case's dice its own (the kit's shared dice not drawn)", "own");
	}

	// ----------------------------------------------------------------------------------------------------------------
	// F. Cases over an open gun's side

	void CasesOut()
	{
		std::printf("F. Cases over the side (-TheaterLandCasesOut, battle-fx-cases-out.json: ground_cases.spg)\n");
		using TheaterFx::AfterShotOf;
		Check(AfterShotOf(Ask("tank", false, true, false, 0.0, false, 3, false, false, true)).bOverSide, "an open gun (a loader's place), the word given: over the side", "thrown");
		Check(!AfterShotOf(Ask("tank", true, false, true, 1.0, true, 8, false, false, true)).bOverSide && !AfterShotOf(Ask("gun", true, true, false, 0.0, true, 3, false, false, true)).bOverSide, "a closed tank, a towed gun: never over the side", "none");
		Check(!AfterShotOf(Ask("tank", false, true, false, 0.0, false, 3, true, true, false)).bOverSide, "an open gun without the word: none", "none");
		const FxTen::Book Off = Recipe();
		const FxTen::Book On = Recipe({ "battle-fx-cases-out.json" });
		// (a battery of six open guns in a line 15 to 40 m from the eye, each firing every 7 to 10 s for five minutes,
		// each case over her right side)
		int Shots = 0;
		auto Battery = [&Shots](Battle& Fx, double Now, int Frame)
		{
			static thread_local std::vector<double> Next;
			if (Frame == 1)
			{
				Shots = 0;
				Next.assign(6, 0.0);
				for (int I = 0; I < 6; ++I)
				{
					Next[static_cast<size_t>(I)] = 0.5 + 1.3 * I;
				}
			}
			for (int I = 0; I < 6; ++I)
			{
				double& At = Next[static_cast<size_t>(I)];
				if (At <= Now)
				{
					TheaterFx::Dice D(static_cast<uint64_t>(I) * 131ull + static_cast<uint64_t>(Frame));
					const Vec Gun(-15.0 + 9.0 * I, 18.0, 0.0);
					Fx.Muzzle(Now, Gun + Vec(0.0, 3.0, 2.0), Vec(0.0, 1.0, 0.4), "tank_gun", "earth"); // (a vehicle's shot, as LandPlay names it)
					Fx.ThrowCase(Now + 1.4, Gun + Vec(1.5, -0.5, 1.9), Vec(1.0, -0.2, 0.0), "spg", 9000 + I * 77 + Frame);
					++Shots;
					At += D.U(7.0, 10.0);
				}
			}
		};
		const Vec Eye(0.0, -6.0, 2.0), Look(0.0, 18.0, 1.0);
		const Fight BOff = Play(Off, Eye, Look, 9000, Battery);
		const int ShotsOff = Shots;
		size_t Thrown = 0;
		bool bLong = true, bLie = true;
		std::vector<uint64_t> SeenCases;
		const Fight BOn = Play(On, Eye, Look, 9000, [&](Battle& Fx, double Now, int Frame)
		{
			Battery(Fx, Now, Frame);
			for (const Bit& B : Fx.Bits)
			{
				if (IsCase(B) && B.Stretch * 2.0 * B.R0 >= 0.36)
				{
					const uint64_t H = FxTen::PieceHash(B);
					if (std::find(SeenCases.begin(), SeenCases.end(), H) == SeenCases.end())
					{
						SeenCases.push_back(H);
						const double L = B.Stretch * 2.0 * B.R0;
						bLong = bLong && L >= 0.369 && L <= 0.451;
						const double Lie = B.Life - B.Land2;
						bLie = bLie && ((Lie >= 179.5 && Lie <= 300.5) || Lie <= 3.0);
					}
				}
			}
		});
		Thrown = SeenCases.size();
		Check(ShotsOff == Shots && Shots >= 180, "six guns, five minutes: about 200 shots", Say("%.0f shots", Shots));
		Check(Thrown >= static_cast<size_t>(0.55 * Shots) && Thrown <= static_cast<size_t>(0.85 * Shots), "on: about 7 in 10 cases over the side (odds 0.7)", Say("%.0f of %.0f", static_cast<double>(Thrown), Shots));
		Check(bLong, "on: each 0.37 to 0.45 m long", "0.37 to 0.45 m");
		Check(bLie, "on: each lies 180 to 300 s (or, past most_lying, a moment)", "180 to 300 s");
		Check(BOn.MostLying <= 200, "on: lying cases within ground_cases.most_lying 200", Say("%.0f at most (off %.0f)", static_cast<double>(BOn.MostLying), static_cast<double>(BOff.MostLying)));
		Check(MissingAll(BOff, BOn) == 0, "on: every piece the battery laid off is laid on (the cases add only)", Say("%.0f missing", static_cast<double>(MissingAll(BOff, BOn))));
		Check(BOn.Seeds == BOff.Seeds, "the kit's shared dice drawn as often as before", Say("%.0f draws", static_cast<double>(BOn.Seeds)));
		std::printf("     the battery: %d shots, %zu cases over the side; most pieces %zu off, %zu on; lying most %zu off, %zu on\n", Shots, Thrown, BOff.MostBits, BOn.MostBits, BOff.MostLying, BOn.MostLying);
		std::printf("     time: the densest frame's Draw, %.3f ms on (%zu pieces), %.3f ms off (%zu pieces); a frame's mean %.3f ms on, %.3f ms off\n", BOn.DensestMs, BOn.MostBits, BOff.DensestMs, BOff.MostBits, BOn.MeanMs, BOff.MeanMs);
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
	SeaCases();
	BarrelWisp();
	BreechPuff();
	HatchFumes();
	CasesOut();
	std::printf("fx_ten_test: %d checks, %d failed\n", Checks, Failed);
	if (Failed == 0)
	{
		std::printf("ALL CHECKS PASSED\n");
	}
	return Failed == 0 ? 0 : 1;
}
