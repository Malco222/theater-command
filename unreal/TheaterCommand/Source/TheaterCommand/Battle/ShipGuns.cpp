#include "Battle/ShipGuns.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ShipGuns
{
	namespace
	{
		// Each gun as its navy had it: name, heavy, kit row, barrels on a mount, rounds a minute a barrel, muzzle
		// velocity, the range it was fired at, a heavy gun's ceiling, its highest elevation, rounds before a change and
		// the change's seconds, calibre. Rates are those a crew kept up, not the gun's best.

		// The United States.
		const Piece Us5x25{ "5-inch/25", true, "", 1, 15.0, 657.0, 9000.0, 8300.0, 85.0, 0, 0.0, 127.0 };
		const Piece Us5x38{ "5-inch/38", true, "", 1, 17.0, 792.0, 10500.0, 11000.0, 85.0, 0, 0.0, 127.0 };
		const Piece Us5x38Twin{ "5-inch/38 twin", true, "", 2, 17.0, 792.0, 10500.0, 11000.0, 85.0, 0, 0.0, 127.0 };
		const Piece UsFifty{ ".50 cal M2", false, "hmg", 1, 550.0, 887.0, 1300.0, 0.0, 90.0, 0, 0.0, 12.7 };
		const Piece UsPiano{ "1.1-inch quad", false, "aa40", 4, 140.0, 823.0, 2800.0, 0.0, 90.0, 8, 2.0, 28.0 };
		const Piece UsOerlikon{ "20 mm Oerlikon", false, "aa25", 1, 450.0, 830.0, 1500.0, 0.0, 90.0, 60, 8.0, 20.0 };
		const Piece UsBoforsQuad{ "40 mm Bofors quad", false, "aa40", 4, 120.0, 881.0, 3000.0, 0.0, 90.0, 0, 0.0, 40.0 };
		const Piece UsBoforsTwin{ "40 mm Bofors twin", false, "aa40", 2, 120.0, 881.0, 3000.0, 0.0, 90.0, 0, 0.0, 40.0 };
		// Japan.
		const Piece Jp127{ "12.7 cm/40 Type 89 twin", true, "", 2, 11.0, 720.0, 9500.0, 9400.0, 90.0, 0, 0.0, 127.0 };
		const Piece Jp120{ "12 cm/45 10th Year Type twin", true, "", 2, 10.0, 825.0, 8500.0, 8400.0, 75.0, 0, 0.0, 120.0 };
		const Piece Jp127dd{ "12.7 cm/50 Type 3 twin", true, "", 2, 8.0, 910.0, 5000.0, 0.0, 55.0, 0, 0.0, 127.0, true };
		const Piece Jp356{ "35.6 cm/45 main battery", true, "", 2, 2.0, 775.0, 5000.0, 0.0, 43.0, 0, 0.0, 356.0, true };
		const Piece Jp25Single{ "25 mm Type 96 single", false, "aa25", 1, 220.0, 900.0, 2000.0, 0.0, 85.0, 15, 3.5, 25.0 };
		const Piece Jp25Twin{ "25 mm Type 96 twin", false, "aa25", 2, 220.0, 900.0, 2000.0, 0.0, 85.0, 15, 3.5, 25.0 };
		const Piece Jp25Triple{ "25 mm Type 96 triple", false, "aa25", 3, 220.0, 900.0, 2000.0, 0.0, 85.0, 15, 3.5, 25.0 };
		const Piece Jp13{ "13.2 mm Type 93 twin", false, "hmg", 2, 450.0, 800.0, 1300.0, 0.0, 85.0, 30, 3.0, 13.2 };
		// Britain.
		const Piece Gb4{ "4-inch Mk XVI twin", true, "", 2, 15.0, 811.0, 9500.0, 11900.0, 80.0, 0, 0.0, 102.0 };
		const Piece Gb45{ "4.5-inch twin", true, "", 2, 12.0, 755.0, 10000.0, 12500.0, 80.0, 0, 0.0, 113.0 };
		const Piece GbPomPom8{ "2-pdr pom-pom octuple", false, "aa40", 8, 100.0, 701.0, 2500.0, 0.0, 80.0, 0, 0.0, 40.0 };
		const Piece GbPomPom4{ "2-pdr pom-pom quad", false, "aa40", 4, 100.0, 701.0, 2500.0, 0.0, 80.0, 0, 0.0, 40.0 };
		const Piece GbVickers{ "0.5-inch Vickers quad", false, "hmg", 4, 700.0, 770.0, 900.0, 0.0, 80.0, 200, 4.0, 12.7 };
		const Piece GbOerlikon{ "20 mm Oerlikon", false, "aa25", 1, 450.0, 830.0, 1500.0, 0.0, 90.0, 60, 8.0, 20.0 };
		const Piece GbBofors{ "40 mm Bofors twin", false, "aa40", 2, 120.0, 881.0, 3000.0, 0.0, 90.0, 0, 0.0, 40.0 };
		const Piece Gb47{ "4.7-inch twin", true, "", 2, 10.0, 747.0, 5000.0, 0.0, 40.0, 0, 0.0, 120.0, true };
		// Germany.
		const Piece De105{ "10.5 cm SK C/33 twin", true, "", 2, 16.0, 900.0, 10000.0, 12500.0, 80.0, 0, 0.0, 105.0 };
		const Piece De37{ "3.7 cm SK C/30 twin", false, "aa40", 2, 30.0, 1000.0, 2500.0, 0.0, 85.0, 0, 0.0, 37.0 };
		const Piece De20{ "2 cm C/38", false, "aa25", 1, 450.0, 835.0, 1500.0, 0.0, 90.0, 20, 4.0, 20.0 };
		const Piece De20Quad{ "2 cm Flakvierling 38", false, "aa25", 4, 450.0, 835.0, 1500.0, 0.0, 90.0, 20, 4.0, 20.0 };
		const Piece De127{ "12.7 cm SK C/34", true, "", 1, 10.0, 830.0, 5000.0, 0.0, 30.0, 0, 0.0, 127.0, true };
		// Italy.
		const Piece It90{ "90 mm/50", true, "", 1, 12.0, 860.0, 9500.0, 10800.0, 75.0, 0, 0.0, 90.0 };
		const Piece It100{ "100 mm/47 twin", true, "", 2, 8.0, 850.0, 8000.0, 10000.0, 85.0, 0, 0.0, 100.0 };
		const Piece It37{ "37 mm/54 Breda twin", false, "aa40", 2, 120.0, 800.0, 2500.0, 0.0, 80.0, 6, 1.5, 37.0 };
		const Piece It20{ "20 mm/65 Breda twin", false, "aa25", 2, 220.0, 840.0, 1500.0, 0.0, 90.0, 12, 3.0, 20.0 };
		const Piece It120{ "120 mm/50 twin", true, "", 2, 6.0, 950.0, 5000.0, 0.0, 45.0, 0, 0.0, 120.0, true };
		// The Soviet Union.
		const Piece Su100{ "100 mm/56 B-34", true, "", 1, 12.0, 900.0, 9500.0, 11000.0, 85.0, 0, 0.0, 100.0 };
		const Piece Su76{ "76 mm 34-K", true, "", 1, 15.0, 813.0, 7000.0, 9000.0, 85.0, 0, 0.0, 76.2 };
		const Piece Su45{ "45 mm 21-K", false, "aa40", 1, 25.0, 760.0, 2000.0, 0.0, 85.0, 0, 0.0, 45.0 };
		const Piece Su37{ "37 mm 70-K", false, "aa40", 1, 160.0, 880.0, 2500.0, 0.0, 85.0, 5, 1.5, 37.0 };
		const Piece SuDshk{ "12.7 mm DShK", false, "hmg", 1, 600.0, 850.0, 1000.0, 0.0, 85.0, 50, 4.0, 12.7 };
		const Piece Su130{ "130 mm/50 B-13", true, "", 1, 7.0, 870.0, 5000.0, 0.0, 45.0, 0, 0.0, 130.0, true };
		// France.
		const Piece Fr100{ "100 mm/45 twin", true, "", 2, 10.0, 765.0, 8000.0, 10000.0, 80.0, 0, 0.0, 100.0 };
		const Piece Fr90{ "90 mm/50 twin", true, "", 2, 12.0, 850.0, 8000.0, 10000.0, 80.0, 0, 0.0, 90.0 };
		const Piece Fr37{ "37 mm/50 twin", false, "aa40", 2, 30.0, 850.0, 2000.0, 0.0, 80.0, 0, 0.0, 37.0 };
		const Piece Fr13Quad{ "13.2 mm Hotchkiss quad", false, "hmg", 4, 450.0, 800.0, 1100.0, 0.0, 90.0, 30, 3.0, 13.2 };
		const Piece Fr13Twin{ "13.2 mm Hotchkiss twin", false, "hmg", 2, 450.0, 800.0, 1100.0, 0.0, 90.0, 30, 3.0, 13.2 };
		const Piece Fr130{ "130 mm/40 twin", true, "", 2, 8.0, 725.0, 5000.0, 0.0, 36.0, 0, 0.0, 130.0, true };
		// Any other navy: a small navy's usual fit, bought abroad.
		const Piece Xx76{ "76 mm high-angle", true, "", 1, 15.0, 750.0, 6500.0, 8000.0, 85.0, 0, 0.0, 76.0 };
		const Piece Xx13{ "13 mm machine gun", false, "hmg", 1, 450.0, 800.0, 1100.0, 0.0, 85.0, 30, 3.0, 13.0 };
		const Piece Xx20{ "20 mm cannon", false, "aa25", 1, 450.0, 830.0, 1400.0, 0.0, 90.0, 60, 8.0, 20.0 };
		const Piece Xx40{ "40 mm Bofors twin", false, "aa40", 2, 120.0, 881.0, 3000.0, 0.0, 90.0, 0, 0.0, 40.0 };

		std::string Upper(const std::string& In)
		{
			std::string Out;
			for (const char C : In) Out += static_cast<char>(C >= 'a' && C <= 'z' ? C - 'a' + 'A' : C);
			return Out;
		}

		std::string Lower(const std::string& In)
		{
			std::string Out;
			for (const char C : In) Out += static_cast<char>(C >= 'A' && C <= 'Z' ? C - 'A' + 'a' : C);
			return Out;
		}
	}

	int Battery::LightMounts() const
	{
		int N = 0;
		for (const Mount& M : Mounts) if (M.Gun && !M.Gun->bHeavy) N += M.Count;
		return N;
	}

	int Battery::HeavyBarrels() const
	{
		int N = 0;
		for (const Mount& M : Mounts) if (M.Gun && M.Gun->bHeavy) N += M.Count * M.Gun->Barrels;
		return N;
	}

	double Battery::LightReach() const
	{
		double R = 0.0;
		for (const Mount& M : Mounts) if (M.Gun && !M.Gun->bHeavy && M.Count > 0) R = std::max(R, M.Gun->Reach);
		return R;
	}

	double Battery::HeavyReach() const
	{
		double R = 0.0;
		for (const Mount& M : Mounts) if (M.Gun && M.Gun->bHeavy && M.Count > 0) R = std::max(R, M.Gun->Reach);
		return R;
	}

	Battery BatteryOf(const std::string& NationIn, const std::string& ClassIn, int Year, int Month, const std::string& Name)
	{
		const std::string Nation = Upper(NationIn);
		std::string Class = Lower(ClassIn);
		// (the classes the battle may yet name, as the nearest it has)
		if (Class == "bc") Class = "bb";
		if (Class == "cl") Class = "ca";
		if (Class == "cve" || Class == "cvl") Class = "cv";
		if (Class == "de" || Class == "pt" || Class == "tb") Class = "dd";
		const int When = Year * 12 + std::clamp(Month, 1, 12) - 1;
		auto From = [When](int Y, int M) { return When >= Y * 12 + M - 1; };
		Battery B;
		auto Add = [&B](const Piece& P, int Count) { if (Count > 0) B.Mounts.push_back(Mount{ &P, Count }); };

		if (Nation == "USA" || Nation == "US")
		{
			// The .50 calibre and the 1.1-inch through the first winter; the 20 mm from the spring of 1942; the 40 mm from
			// the autumn of 1942; the fleet's last fit from mid 1944.
			const bool b20 = From(1942, 4), b40 = From(1942, 11), bLate = From(1944, 6);
			if (Class == "bb")
			{
				if (!b20) { Add(Us5x25, 8); Add(UsFifty, 8); }
				else if (!b40) { Add(Us5x25, 8); Add(UsPiano, 4); Add(UsOerlikon, 16); }
				else if (!bLate) { Add(Us5x25, 8); Add(UsBoforsQuad, 4); Add(UsOerlikon, 30); }
				else { Add(Us5x38Twin, 8); Add(UsBoforsQuad, 10); Add(UsOerlikon, 40); }
			}
			else if (Class == "cv")
			{
				if (!b20) { Add(Us5x38, 8); Add(UsPiano, 4); Add(UsFifty, 24); }
				else if (!b40) { Add(Us5x38, 8); Add(UsPiano, 4); Add(UsOerlikon, 24); }
				else if (!bLate) { Add(Us5x38, 12); Add(UsBoforsQuad, 8); Add(UsOerlikon, 40); }
				else { Add(Us5x38, 12); Add(UsBoforsQuad, 17); Add(UsOerlikon, 50); }
			}
			else if (Class == "dd")
			{
				if (!b20) { Add(Us5x38, 5); Add(UsFifty, 4); }
				else if (!b40) { Add(Us5x38, 5); Add(UsOerlikon, 6); }
				else { Add(Us5x38, 5); Add(UsBoforsTwin, bLate ? 5 : 2); Add(UsOerlikon, 7); }
			}
			else if (Class == "ss")
			{
				Add(b20 ? UsOerlikon : UsFifty, 2);
			}
			else if (Class == "ag")
			{
				// (Utah, the fleet's target ship, by 1941 its anti-aircraft gunnery school)
				Add(Us5x25, 4); Add(UsFifty, 6);
			}
			else
			{
				if (!b20) { Add(Us5x25, 8); Add(UsPiano, 2); Add(UsFifty, 8); }
				else if (!b40) { Add(Us5x25, 8); Add(UsPiano, 4); Add(UsOerlikon, 12); }
				else if (!bLate) { Add(Us5x25, 8); Add(UsBoforsQuad, 4); Add(UsBoforsTwin, 4); Add(UsOerlikon, 20); }
				else { Add(Us5x38Twin, 6); Add(UsBoforsQuad, 6); Add(UsBoforsTwin, 6); Add(UsOerlikon, 24); }
			}
		}
		else if (Nation == "JAP" || Nation == "JPN" || Nation == "JP")
		{
			// The twin 25 mm of 1941 and 1942; triples and singles added from mid 1943, and again from mid 1944.
			const bool bMore = From(1943, 7), bMost = From(1944, 6);
			if (Class == "bb")
			{
				// (her main battery thrown low into the sea ahead of a torpedo plane, as at Leyte and before)
				Add(Jp356, 4);
				if (!bMore) { Add(Jp127, 4); Add(Jp25Twin, 10); Add(Jp13, 2); }
				else if (!bMost) { Add(Jp127, 6); Add(Jp25Triple, 10); Add(Jp25Twin, 2); }
				else { Add(Jp127, 6); Add(Jp25Triple, 20); Add(Jp25Single, 30); }
			}
			else if (Class == "cv")
			{
				if (!bMore)
				{
					// (the Midway four as they were fitted that June)
					if (Name == "Akagi") { Add(Jp120, 6); Add(Jp25Twin, 14); }
					else if (Name == "Kaga") { Add(Jp127, 8); Add(Jp25Twin, 11); }
					else if (Name == "Hiryu") { Add(Jp127, 6); Add(Jp25Triple, 7); Add(Jp25Twin, 5); }
					else if (Name == "Soryu") { Add(Jp127, 6); Add(Jp25Twin, 14); }
					else { Add(Jp127, 6); Add(Jp25Triple, 6); Add(Jp25Twin, 6); }
				}
				else if (!bMost) { Add(Jp127, 8); Add(Jp25Triple, 16); Add(Jp25Single, 10); }
				else { Add(Jp127, 8); Add(Jp25Triple, 20); Add(Jp25Single, 30); }
			}
			else if (Class == "dd")
			{
				// (a fleet destroyer's 12.7 cm rose to 55 degrees: no answer to a dive bomber overhead)
				if (!bMore) { Add(Jp127dd, 3); Add(Jp25Twin, 2); Add(Jp13, 2); }
				else if (!bMost) { Add(Jp127dd, 2); Add(Jp25Triple, 2); Add(Jp25Twin, 2); }
				else { Add(Jp127dd, 2); Add(Jp25Triple, 4); Add(Jp25Single, 10); }
			}
			else if (Class == "ss")
			{
				Add(bMore ? Jp25Twin : Jp13, 1);
			}
			else
			{
				if (!bMore) { Add(Jp127, 4); Add(Jp25Twin, 6); }
				else if (!bMost) { Add(Jp127, 4); Add(Jp25Triple, 4); Add(Jp25Twin, 6); }
				else { Add(Jp127, 4); Add(Jp25Triple, 8); Add(Jp25Twin, 4); Add(Jp25Single, 20); }
			}
		}
		else if (Nation == "UK" || Nation == "GBR" || Nation == "GB" || Nation == "ENG")
		{
			// The pom-pom and the 0.5-inch quads; the 20 mm from mid 1941; the 40 mm from mid 1943.
			const bool b20 = From(1941, 6), b40 = From(1943, 6);
			if (Class == "bb" || Class == "cv")
			{
				const bool bCarrier = Class == "cv";
				Add(Gb45, 8);
				Add(GbPomPom8, bCarrier ? 6 : 4);
				if (!b20) Add(GbVickers, 4);
				else Add(GbOerlikon, b40 ? 30 : (bCarrier ? 20 : 16));
				if (b40) Add(GbBofors, 4);
			}
			else if (Class == "dd")
			{
				// (her 4.7-inch rose to 40 degrees: low over the sea only; a 4-inch high-angle gun in place of a set of tubes)
				Add(Gb47, 3); Add(Xx76, 1); Add(GbPomPom4, 1);
				if (!b20) Add(GbVickers, 2);
				else Add(GbOerlikon, 4);
				if (b40) Add(GbBofors, 1);
			}
			else if (Class == "ss")
			{
				Add(b20 ? GbOerlikon : Xx13, 1);
			}
			else
			{
				Add(Gb4, 4); Add(GbPomPom4, 2);
				if (!b20) Add(GbVickers, 2);
				else Add(GbOerlikon, 8);
				if (b40) Add(GbBofors, 2);
			}
		}
		else if (Nation == "GER" || Nation == "DE")
		{
			// The 2 cm quad from 1943.
			const bool bQuad = From(1943, 1);
			if (Class == "bb") { Add(De105, 8); Add(De37, 8); Add(De20, 12); if (bQuad) Add(De20Quad, 6); }
			else if (Class == "cv") { Add(De105, 6); Add(De37, 11); Add(De20, 7); }
			else if (Class == "dd") { Add(De127, 5); Add(De37, 2); Add(De20, 6); if (bQuad) Add(De20Quad, 1); }
			else if (Class == "ss") { Add(bQuad ? De20Quad : De20, 1); }
			else { Add(De105, 6); Add(De37, 6); Add(De20, 8); if (bQuad) Add(De20Quad, 4); }
		}
		else if (Nation == "ITA" || Nation == "IT")
		{
			const bool bMore = From(1942, 6);
			if (Class == "bb") { Add(It90, 12); Add(It37, 10); Add(It20, 14); }
			else if (Class == "cv") { Add(It90, 12); Add(It37, 6); Add(It20, 10); }
			else if (Class == "dd") { Add(It120, 2); Add(It37, 1); Add(It20, bMore ? 4 : 2); }
			else if (Class == "ss") { Add(Fr13Twin, 1); }
			else { Add(It100, 6); Add(It37, 4); Add(It20, bMore ? 8 : 4); }
		}
		else if (Nation == "SOV" || Nation == "USSR" || Nation == "SU")
		{
			const bool bMore = From(1942, 6);
			if (Class == "bb") { Add(Su76, 6); Add(Su37, bMore ? 12 : 6); Add(SuDshk, 12); }
			else if (Class == "dd") { Add(Su130, 4); Add(Su76, 2); Add(Su45, 2); Add(SuDshk, 2); if (bMore) Add(Su37, 4); }
			else if (Class == "ss") { Add(Su45, 1); }
			else { Add(Su100, 6); Add(Su45, 6); Add(SuDshk, 4); if (bMore) Add(Su37, 6); }
		}
		else if (Nation == "FRA" || Nation == "FR")
		{
			const bool bRefit = From(1943, 9); // (the Free French ships refitted in American yards)
			if (Class == "bb" || Class == "cv") { Add(Class == "bb" ? Fr100 : Fr90, Class == "bb" ? 6 : 4); Add(Fr37, 4); Add(Fr13Quad, 6); }
			else if (Class == "dd") { Add(Fr130, 2); Add(Fr37, 1); Add(Fr13Twin, 2); }
			else if (Class == "ss") { Add(Fr13Twin, 1); }
			else { Add(Fr90, 4); Add(Fr37, 4); Add(Fr13Twin, 4); }
			if (bRefit && Class != "ss")
			{
				Add(UsBoforsQuad, Class == "dd" ? 1 : 4);
				Add(UsOerlikon, Class == "dd" ? 6 : 20);
			}
		}
		else
		{
			// A small navy: a few high-angle guns and machine guns, the 20 mm and the 40 mm as the war went on.
			const int Big = Class == "bb" || Class == "cv" ? 2 : Class == "dd" ? 0 : Class == "ss" ? -1 : 1;
			if (Big >= 0) Add(Xx76, Big == 2 ? 8 : Big == 1 ? 4 : 1);
			Add(Xx13, Big == 2 ? 8 : Big == 1 ? 4 : Big == 0 ? 2 : 1);
			if (From(1942, 1) && Big >= 0) Add(Xx20, Big == 2 ? 8 : Big == 1 ? 4 : 2);
			if (From(1943, 1) && Big >= 0) Add(Xx40, Big == 2 ? 4 : Big == 1 ? 2 : 1);
		}
		return B;
	}

	double DeckOf(const std::string& ClassIn)
	{
		const std::string Class = Lower(ClassIn);
		if (Class == "bb" || Class == "bc") return 12.0;
		if (Class == "cv" || Class == "cve" || Class == "cvl") return 16.0;
		if (Class == "dd" || Class == "de") return 6.0;
		if (Class == "ss") return 4.0;
		return 9.0;
	}

	std::string Describe(const Battery& B)
	{
		std::string Out;
		for (const Mount& M : B.Mounts)
		{
			if (!M.Gun || M.Count <= 0) continue;
			char Line[96];
			std::snprintf(Line, sizeof(Line), "%s%d x %s", Out.empty() ? "" : ", ", M.Count, M.Gun->Name);
			Out += Line;
		}
		return Out.empty() ? std::string("none") : Out;
	}
}

namespace ShipGuns
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Deg = Pi / 180.0;
		constexpr double Gee = 9.81;
		const Vec Up(0.0, 0.0, 1.0);

		double Len(const Vec& V) { return std::sqrt(V.Dot(V)); }
		Vec Norm(const Vec& V, const Vec& Otherwise = Vec(0.0, 1.0, 0.0))
		{
			const double L = Len(V);
			return L > 1e-9 ? V * (1.0 / L) : Otherwise;
		}
		/** Along a heading (clockwise from north) and a climb; to its right, level. */
		Vec Ahead(double Hdg, double Gam = 0.0) { return Vec(std::sin(Hdg) * std::cos(Gam), std::cos(Hdg) * std::cos(Gam), std::sin(Gam)); }
		Vec RightOf(double Hdg) { return Vec(std::cos(Hdg), -std::sin(Hdg), 0.0); }

		/** A point in a plane's own frame (X forward, Y right, Z up) in the world, banked as it is. */
		Vec LocalOf(const Plane& P, const Vec& L)
		{
			const Vec F = Ahead(P.Hdg, P.Gam);
			const Vec R0 = RightOf(P.Hdg);
			const Vec U0 = R0.Cross(F);
			const double Cb = std::cos(P.Bank), Sb = std::sin(P.Bank);
			const Vec R = R0 * Cb - U0 * Sb;
			const Vec U = U0 * Cb + R0 * Sb;
			return P.P + F * L.X + R * L.Y + U * L.Z;
		}

		/** A ship's mount's own steady share, 0 to 1: it is manned while her share of guns manned is over it, so her guns
		 * join one by one as her crews reach them. */
		double Share(int Ship, int Mount)
		{
			uint32_t H = static_cast<uint32_t>(Ship) * 2654435761u ^ static_cast<uint32_t>(Mount + 1) * 2246822519u;
			H ^= H >> 15;
			H *= 2654435761u;
			H ^= H >> 13;
			return static_cast<double>(H % 10000u) / 10000.0;
		}

		/** A light mount as drawn: its gun, and how many of hers it stands for (a ship with more than 30 has each drawn
		 * one stand for a few, its streams the denser). */
		struct Slot
		{
			const Piece* Gun = nullptr;
			double Stands = 1.0;
		};

		std::vector<Slot> LightSlots(const Battery& B)
		{
			std::vector<const Piece*> All;
			for (const Mount& M : B.Mounts)
			{
				if (!M.Gun || M.Gun->bHeavy) continue;
				for (int K = 0; K < M.Count; ++K) All.push_back(M.Gun);
			}
			// (mixed along her length: the kinds interleaved, as they stood about her decks)
			std::vector<const Piece*> Mixed;
			std::vector<size_t> Next;
			std::vector<const Piece*> Kinds;
			for (const Piece* G : All) if (std::find(Kinds.begin(), Kinds.end(), G) == Kinds.end()) Kinds.push_back(G);
			std::vector<int> Left;
			for (const Piece* G : Kinds) Left.push_back(static_cast<int>(std::count(All.begin(), All.end(), G)));
			for (bool bAny = true; bAny;)
			{
				bAny = false;
				for (size_t K = 0; K < Kinds.size(); ++K)
				{
					if (Left[K] <= 0) continue;
					Mixed.push_back(Kinds[K]);
					--Left[K];
					bAny = true;
				}
			}
			const int Most = 30;
			std::vector<Slot> Out;
			const int N = static_cast<int>(Mixed.size());
			if (N <= Most)
			{
				for (const Piece* G : Mixed) Out.push_back(Slot{ G, 1.0 });
				return Out;
			}
			for (int K = 0; K < Most; ++K) Out.push_back(Slot{ Mixed[static_cast<size_t>(K * N / Most)], static_cast<double>(N) / Most });
			return Out;
		}

		/** A light crew's bursts by its gun: a machine gun's short ones, the 20 and 25 mm's longer, the 40 mm and the
		 * pom-pom fed as they fire. */
		void NextBurst(Dice& Roll, const Piece& G, double& Burst, double& Rest)
		{
			const std::string Kind = G.Kind;
			if (Kind == "aa40") { Burst = Roll.U(5.0, 12.0); Rest = Roll.U(0.5, 1.5); }
			else if (Kind == "aa25") { Burst = Roll.U(2.5, 6.0); Rest = Roll.U(0.4, 1.5); }
			else { Burst = Roll.U(1.5, 4.0); Rest = Roll.U(0.6, 2.0); }
		}
	}

	int KindSlot(const std::string& Kind)
	{
		return Kind == "hmg" ? 0 : Kind == "aa25" ? 1 : Kind == "aa40" ? 2 : 3;
	}

	Gunnery::Gunnery(uint64_t Seed)
		: Roll(Seed)
	{
	}

	const Battery& Gunnery::BatteryFor(const Hull& H)
	{
		auto It = Batteries.find(H.Id);
		if (It != Batteries.end()) return It->second;
		Battery B = BatteryOf(H.Nation, H.Class, Year, Month, H.Name);
		Count.Kits[H.Name.empty() ? H.Class + " " + std::to_string(H.Id) : H.Name] = Describe(B);
		return Batteries.emplace(H.Id, std::move(B)).first->second;
	}

	void Gunnery::StopAll(std::vector<Shot>& Out)
	{
		for (const int Key : Firing)
		{
			Shot S;
			S.Kind = ShotKind::StreamStop;
			S.Key = Key;
			Out.push_back(S);
		}
		Firing.clear();
	}

	void Gunnery::Fire(double Now, double Dt, const std::vector<Hull>& Ships, const std::vector<Plane>& Planes, std::vector<Shot>& Out)
	{
		if (!(Dt > 0.0))
		{
			StopAll(Out);
			return;
		}
		std::map<int, size_t> ByKey;
		for (size_t K = 0; K < Planes.size(); ++K) ByKey[Planes[K].Key] = K;

		// Shells bursting and rounds striking home, as they get there.
		for (size_t K = 0; K < Shells.size();)
		{
			if (Shells[K].At > Now) { ++K; continue; }
			Shot S = Shells[K].S;
			Shells.erase(Shells.begin() + static_cast<long>(K));
			if (S.Kind == ShotKind::Hit)
			{
				// (on the plane where it is now: the spot was kept in its own frame)
				// (none on a plane already going down: its own fall is the battle's)
				auto It = ByKey.find(S.Key);
				if (It == ByKey.end() || Planes[It->second].bFalling) continue;
				const Plane& P = Planes[It->second];
				S.At = LocalOf(P, S.Aim);
				S.Normal = Norm(S.At - P.P, Up);
				S.Vel = P.V;
				S.Aim = S.At;
				Count.Hits++;
			}
			else if (S.Kind == ShotKind::Burst) Count.Bursts++;
			else if (S.Kind == ShotKind::Splash) Count.Splashes++;
			Out.push_back(S);
		}
		Budget = std::min(RoundsMost, Budget + Dt * RoundsMost);

		// The flak hold (the historian's B3): a ship's guns hold on a plane its own side's fighters are going in on.
		std::vector<int> Guarded(Planes.size(), -1);
		for (size_t K = 0; K < Planes.size(); ++K)
		{
			const Plane& F = Planes[K];
			if (!F.bFighter || !F.bFighting || F.bFalling) continue;
			for (size_t J = 0; J < Planes.size(); ++J)
			{
				const Plane& T = Planes[J];
				if (T.Side != F.Side && Len(T.P - F.P) < 900.0) Guarded[J] = F.Side;
			}
		}

		struct Want
		{
			double Far = 0.0;
			Shot S;
		};
		std::vector<Want> Streams;
		struct RoundDue
		{
			double Far = 0.0;
			size_t Ship = 0, Target = 0;
			const Piece* Gun = nullptr;
			int Mount = 0, Of = 1;
		};
		std::vector<RoundDue> Rounds;

		for (size_t ShipIndex = 0; ShipIndex < Ships.size(); ++ShipIndex)
		{
			const Hull& H = Ships[ShipIndex];
			if (H.bSunk || H.bHold || !(H.Manned > 0.0)) continue;
			const Battery& B = BatteryFor(H);
			const double Deck = DeckOf(H.Class);
			const Vec Mid = H.At + Up * Deck;
			const double Reach = std::max(B.LightReach(), B.HeavyReach());
			// Her enemies in her reach, and a plane her guns are seen bringing down.
			std::vector<size_t> Near;
			for (size_t K = 0; K < Planes.size(); ++K)
			{
				if (Planes[K].Side != H.Side && Len(Planes[K].P - Mid) <= Reach) Near.push_back(K);
			}
			long Locked = -1;
			auto Lk = Locks.find(H.Id);
			if (Lk != Locks.end())
			{
				auto It = ByKey.find(Lk->second.Plane);
				if (Lk->second.Until > Now && It != ByKey.end()) Locked = static_cast<long>(It->second);
				else Locks.erase(Lk);
			}
			if (Near.empty() && Locked < 0) continue;
			// (a plane fair to fire on: not going down, not held, its own fighters not on it)
			auto Fair = [&](size_t K)
			{
				const Plane& P = Planes[K];
				if (P.bFalling || P.bHold) return false;
				if (Guarded[K] == H.Side) { Count.Held++; return false; }
				return true;
			};

			// The light guns: each crew on its plane, in its bursts, laid with its own error.
			const std::vector<Slot> Slots = LightSlots(B);
			const int M = static_cast<int>(Slots.size());
			for (int Mo = 0; Mo < M; ++Mo)
			{
				const Piece& G = *Slots[static_cast<size_t>(Mo)].Gun;
				if (Share(H.Id, Mo) >= H.Manned) continue; // (not yet manned)
				const int First = StreamKey(H.Id, Mo * 2);
				Crew& C = Crews[First];
				if (!C.bMade)
				{
					C.bMade = true;
					C.Phase = Roll.U(0.0, 2.0 * Pi);
					C.W1 = Roll.U(0.25, 0.6);
					C.W2 = Roll.U(0.35, 0.9);
					C.Lead = Roll.U(0.5, 0.85);
					C.Left = G.Load;
					C.Burst = Roll.U(1.0, 4.0);
					C.Rest = Roll.U(0.0, 1.5);
				}
				// Where it stands: along her length, port and starboard by turns, on her decks and galleries.
				const double Side = Mo % 2 == 0 ? 1.0 : -1.0;
				const Vec Muzzle = H.At + Ahead(H.Face) * (H.Long * (-0.42 + 0.84 * (Mo + 0.5) / std::max(1, M))) + RightOf(H.Face) * (Side * H.Beam * 0.42) + Up * (Deck + 1.5);
				const Vec Outboard = RightOf(H.Face) * Side;
				auto Bears = [&](const Plane& P)
				{
					const Vec To = P.P - Muzzle;
					const double D = Len(To);
					if (D > G.Reach || D < 25.0) return false;
					const double Elev = std::asin(std::clamp(To.Z / D, -1.0, 1.0)) / Deg;
					if (Elev < -6.0 || Elev > G.MostUp) return false;
					// (not across her own decks, low down)
					return To.Dot(Outboard) >= -0.3 * D || Elev > 45.0;
				};
				long Index = -1;
				if (Locked >= 0 && Bears(Planes[static_cast<size_t>(Locked)])) Index = Locked;
				if (Index < 0)
				{
					auto It = ByKey.find(C.Target);
					if (It != ByKey.end() && Now < C.Pick && Fair(It->second) && Bears(Planes[It->second])) Index = static_cast<long>(It->second);
				}
				if (Index < 0)
				{
					// (the nearest, the one coming at her first; a fighter a little after a bomber)
					double Best = 1e18;
					for (const size_t K : Near)
					{
						const Plane& P = Planes[K];
						if (!Bears(P) || !Fair(K)) continue;
						const double D = Len(P.P - Muzzle);
						const bool bClosing = P.V.Dot(Muzzle - P.P) > 0.0;
						const double Score = D * (bClosing ? 0.6 : 1.0) * (P.bFighter ? 1.15 : 1.0);
						if (Score < Best) { Best = Score; Index = static_cast<long>(K); }
					}
					C.Target = Index >= 0 ? Planes[static_cast<size_t>(Index)].Key : -1;
					C.Pick = Now + Roll.U(4.0, 8.0);
				}
				if (Index < 0) continue;
				// Its bursts, and its drum, box or clip changed when it runs dry.
				bool bFiring = false;
				if (C.Change > 0.0) C.Change -= Dt;
				else if (C.Rest > 0.0) C.Rest -= Dt;
				else
				{
					bFiring = true;
					C.Burst -= Dt;
					if (G.Load > 0)
					{
						C.Left -= G.Rate / 60.0 * Dt;
						if (C.Left <= 0.0)
						{
							C.Change = G.Change * Roll.U(0.8, 1.3);
							C.Left = G.Load;
							C.Burst = 0.0;
						}
					}
					if (C.Burst <= 0.0) NextBurst(Roll, G, C.Burst, C.Rest);
				}
				if (!bFiring) continue;
				// Laid short of the full lead and wandering about the plane: most rounds pass behind it or beside it,
				// now and then the stream walks across it.
				const Plane& T = Planes[static_cast<size_t>(Index)];
				const bool bLocked = Index == Locked;
				const double D = Len(T.P - Muzzle);
				const double Tof = D / (G.Mv * 0.8);
				const double Lead = bLocked ? 1.0 : std::clamp(C.Lead + 0.22 * std::sin(Now * C.W1 + C.Phase), 0.3, 1.1);
				const Vec Los = Norm(T.P - Muzzle);
				const Vec A1 = Norm(Los.Cross(Up), RightOf(H.Face));
				const Vec A2 = A1.Cross(Los);
				const double Amp = (bLocked ? 0.2 : 1.0) * std::max(T.Span * 1.3, D * 0.011);
				const Vec Off = A1 * (Amp * std::sin(Now * C.W2 + C.Phase * 1.7)) + A2 * (Amp * 0.8 * std::sin(Now * C.W2 * 1.37 + C.Phase * 0.6));
				const Vec There = T.P + T.V * Tof;
				const Vec Point = T.P + T.V * (Tof * Lead) + Off;
				// (how far the plane will be from the stream's line as the rounds reach it: a plane flying straight at the
				// gun is on the line whatever the lead, and one crossing it is off by the lead and the wander alike)
				const Vec Ray = Norm(Point - Muzzle, Los);
				const Vec Gap = There - Muzzle;
				const double Miss = Len(Gap - Ray * Gap.Dot(Ray));
				const int Of = std::min(G.Barrels, 2);
				for (int K = 0; K < Of; ++K)
				{
					Want W;
					W.S.Kind = ShotKind::Stream;
					W.S.Key = First + K;
					W.S.Ship = H.Id;
					W.S.At = Muzzle + Ahead(H.Face) * ((K - (Of - 1) * 0.5) * 0.6);
					W.S.Aim = Point + Up * (0.5 * Gee * Tof * Tof * 0.85); // (laid up for the drop)
					W.S.Vel = H.Vel;
					W.S.Kit = G.Kind;
					W.S.What = G.Name;
					W.S.Rate = G.Rate * G.Barrels * Slots[static_cast<size_t>(Mo)].Stands / Of;
					W.S.Mv = G.Mv;
					W.Far = Len(W.S.At - Eye);
					Streams.push_back(W);
				}
				Count.StreamFrames++;
				const bool bOn = Miss < T.Span * 0.5;
				if (bOn) Count.StreamOn++;
				if (T.Side == H.Side || D > G.Reach || (!bLocked && (T.bHold || T.bFalling))) Count.OutOfReach++;
				Count.Fired.insert(G.Name);
				// A few rounds striking home while the stream is across it.
				if (bOn && D < G.Reach * 0.85 && Roll.U() < Dt * (bLocked ? 4.5 : 1.8))
				{
					Shell Sh;
					Sh.At = Now + Tof;
					Sh.S.Kind = ShotKind::Hit;
					Sh.S.Key = T.Key;
					Sh.S.Ship = H.Id;
					Sh.S.Kit = G.Kind;
					Sh.S.Aim = Vec(Roll.U(-0.4, 0.4) * T.Long, Roll.U(-0.45, 0.45) * T.Span, Roll.U(-0.2, 0.5));
					Shells.push_back(Sh);
				}
			}

			// The heavy guns: one director laying them on one plane, its rounds owed at their rate, the nearest ships'
			// shown first.
			std::vector<const Piece*> Heavy;
			for (const Mount& Mt : B.Mounts) if (Mt.Gun && Mt.Gun->bHeavy && Mt.Count > 0) Heavy.push_back(Mt.Gun);
			if (Heavy.empty()) continue;
			Director& Dr = Directors[H.Id];
			// (a low-angle gun fires only into the sea ahead of a torpedo plane; a high-angle one at anything from 1.5 km
			// out to its reach, and into the sea ahead of a low one inside 5 km)
			auto Bears = [&](const Piece& G, const Plane& P)
			{
				const Vec To = P.P - Mid;
				const double D = Len(To);
				const bool bLow = P.P.Z < 150.0;
				if (bLow) return D >= 800.0 && D <= std::min(G.Reach, 5000.0);
				if (G.bLow) return false;
				const double Elev = std::asin(std::clamp(To.Z / std::max(D, 1.0), -1.0, 1.0)) / Deg;
				return D >= 1500.0 && D <= G.Reach && P.P.Z <= G.Ceiling && Elev <= G.MostUp;
			};
			auto Any = [&](size_t K)
			{
				for (const Piece* G : Heavy) if (Bears(*G, Planes[K])) return true;
				return false;
			};
			long Target = -1;
			{
				auto It = ByKey.find(Dr.Target);
				if (It != ByKey.end() && Now < Dr.Pick && Fair(It->second) && Any(It->second)) Target = static_cast<long>(It->second);
			}
			if (Target < 0)
			{
				double Best = 1e18;
				for (const size_t K : Near)
				{
					if (!Fair(K) || !Any(K)) continue;
					const Plane& P = Planes[K];
					const double D = Len(P.P - Mid);
					const double Score = D * (P.V.Dot(Mid - P.P) > 0.0 ? 0.7 : 1.0);
					if (Score < Best) { Best = Score; Target = static_cast<long>(K); }
				}
				Dr.Target = Target >= 0 ? Planes[static_cast<size_t>(Target)].Key : -1;
				Dr.Pick = Now + Roll.U(6.0, 12.0);
				Dr.Lead = Roll.U(0.65, 0.95);
			}
			if (Target < 0) { Dr.Owed = 0.0; continue; }
			// (her guns on the side the plane is on, about half of them; her crews as manned)
			double PerSecond = 0.0;
			std::vector<const Piece*> On;
			for (const Mount& Mt : B.Mounts)
			{
				if (!Mt.Gun || !Mt.Gun->bHeavy || !Bears(*Mt.Gun, Planes[static_cast<size_t>(Target)])) continue;
				PerSecond += Mt.Count * Mt.Gun->Barrels * Mt.Gun->Rate / 60.0 * 0.55 * H.Manned;
				On.push_back(Mt.Gun);
			}
			Dr.Owed = std::min(Dr.Owed + PerSecond * Dt, 3.0);
			while (Dr.Owed >= 1.0 && !On.empty())
			{
				Dr.Owed -= 1.0;
				RoundDue R;
				R.Ship = ShipIndex;
				R.Target = static_cast<size_t>(Target);
				R.Gun = On[static_cast<size_t>(Dr.Next) % On.size()];
				R.Mount = Dr.Next++;
				R.Of = std::max(1, B.HeavyBarrels() / 2);
				R.Far = Len(Mid - Eye);
				Rounds.push_back(R);
			}
		}

		// The heavy rounds shown, the nearest ships' first, within the budget.
		std::sort(Rounds.begin(), Rounds.end(), [](const RoundDue& A, const RoundDue& B) { return A.Far < B.Far; });
		for (const RoundDue& R : Rounds)
		{
			if (Budget < 1.0) break;
			Budget -= 1.0;
			const Hull& H = Ships[R.Ship];
			const Plane& T = Planes[R.Target];
			const Piece& G = *R.Gun;
			const double Deck = DeckOf(H.Class);
			// (amidships, on the side toward the plane)
			const double Side = (T.P - H.At).Dot(RightOf(H.Face)) >= 0.0 ? 1.0 : -1.0;
			const Vec Muzzle = H.At + Ahead(H.Face) * (H.Long * (-0.18 + 0.36 * ((R.Mount % R.Of) + 0.5) / R.Of)) + RightOf(H.Face) * (Side * H.Beam * 0.36) + Up * (Deck + 2.5);
			const double D = Len(T.P - Muzzle);
			const double Tof = std::clamp(D / (G.Mv * 0.62), 1.0, 20.0);
			Shell Sh;
			Sh.At = Now + Tof;
			Sh.S.Ship = H.Id;
			Sh.S.Size = G.Mm;
			Sh.S.What = G.Name;
			if (T.P.Z < 150.0)
			{
				// Into the sea ahead of a torpedo plane: the splashes it must fly through.
				Vec Where = T.P + T.V * (Tof * Roll.U(0.5, 1.3)) + RightOf(T.Hdg) * (Roll.Bell() * 30.0);
				Where.Z = 0.0;
				Sh.S.Kind = ShotKind::Splash;
				Sh.S.At = Where;
			}
			else
			{
				// A time-fuzed burst where the director thought it would be: short of the lead, the fuze long or short
				// along the line of fire, a little off to either side. A proximity fuze bursts closer about it. Never on
				// a plane the battle has not lost: at least a span from every one.
				const double K = H.bProxFuze ? 0.45 : 1.0;
				const Vec Los = Norm(T.P - Muzzle);
				const Vec A1 = Norm(Los.Cross(Up), RightOf(H.Face));
				const Vec A2 = A1.Cross(Los);
				const double Lead = std::clamp(Directors[H.Id].Lead + Roll.Bell() * 0.12, 0.4, 1.2);
				Vec Where = T.P + T.V * (Tof * Lead) + Los * (Roll.Bell() * (35.0 + 0.012 * D) * K) + A1 * (Roll.Bell() * (18.0 + 0.006 * D) * K) + A2 * (Roll.Bell() * (10.0 + 0.004 * D) * K);
				Where.Z = std::min(Where.Z, G.Ceiling);
				for (const Plane& P : Planes)
				{
					const Vec Then = P.P + P.V * Tof;
					const Vec Off = Where - Then;
					if (Len(Off) < P.Span) Where = Then + Norm(Off, Up) * (P.Span * 1.1);
				}
				Sh.S.Kind = ShotKind::Burst;
				Sh.S.At = Where;
			}
			Shells.push_back(Sh);
			Shot Fired;
			Fired.Kind = ShotKind::Round;
			Fired.Ship = H.Id;
			Fired.At = Muzzle;
			Fired.Aim = Sh.S.At;
			Fired.What = G.Name;
			Fired.Size = G.Mm;
			Out.push_back(Fired);
			Count.Rounds++;
			Count.Fired.insert(G.Name);
		}

		// The streams drawn, the nearest the eye first, as many as the kit can draw.
		std::sort(Streams.begin(), Streams.end(), [](const Want& A, const Want& B) { return A.Far < B.Far; });
		if (static_cast<int>(Streams.size()) > MostStreams) Streams.resize(static_cast<size_t>(std::max(0, MostStreams)));
		std::set<int> Kept;
		for (const Want& W : Streams)
		{
			if (!Kept.insert(W.S.Key).second) continue;
			Out.push_back(W.S);
		}
		for (const int Key : Firing)
		{
			if (Kept.count(Key)) continue;
			Shot S;
			S.Kind = ShotKind::StreamStop;
			S.Key = Key;
			Out.push_back(S);
		}
		Firing = Kept;
		Count.MostStreams = std::max(Count.MostStreams, static_cast<int>(Kept.size()));
	}

	bool Gunnery::Kill(double Now, const Plane& P, const std::vector<Hull>& Ships, std::vector<Shot>& Out)
	{
		// The nearest ship of the other side with her guns manned, and which of her guns reach it (the historian's B5:
		// the heavy guns' kills fall far out, before the drop; the light and medium guns' close in, about it).
		const Hull* By = nullptr;
		double Best = 1e18;
		for (const Hull& H : Ships)
		{
			if (H.bSunk || !(H.Manned > 0.0) || H.Side == P.Side) continue;
			const double D = Len(P.P - (H.At + Up * DeckOf(H.Class)));
			if (D < Best) { Best = D; By = &H; }
		}
		if (!By) return false;
		const Battery& B = BatteryFor(*By);
		if (B.LightMounts() > 0 && Best <= B.LightReach() * 1.1)
		{
			// Her streams on it for the moment it is brought down, and the rounds that did it seen striking home as it
			// goes (What "kill": the one time rounds are seen on a plane that is falling, its fall's first frame).
			Locks[By->Id] = Lock{ P.Key, Now + 1.2 };
			const Piece* Most = nullptr;
			int MostCount = 0;
			for (const Mount& M : B.Mounts) if (M.Gun && !M.Gun->bHeavy && M.Count > MostCount) { Most = M.Gun; MostCount = M.Count; }
			const int N = static_cast<int>(Roll.U(3.0, 6.0));
			for (int K = 0; K < N; ++K)
			{
				Shot S;
				S.Kind = ShotKind::Hit;
				S.Key = P.Key;
				S.Ship = By->Id;
				S.Kit = Most ? Most->Kind : "hmg";
				S.What = "kill";
				S.At = LocalOf(P, Vec(Roll.U(-0.4, 0.4) * P.Long, Roll.U(-0.45, 0.45) * P.Span, Roll.U(-0.2, 0.5)));
				S.Normal = Norm(S.At - P.P, Up);
				S.Vel = P.V;
				Out.push_back(S);
				Count.Hits++;
			}
			Count.Kills++;
			return true;
		}
		// (her high-angle guns only: a low-angle main battery never bursts a shell up there)
		const Piece* High = nullptr;
		double HighReach = 0.0;
		for (const Mount& M : B.Mounts)
		{
			if (M.Gun && M.Gun->bHeavy && !M.Gun->bLow && M.Count > 0 && M.Gun->Reach > HighReach)
			{
				High = M.Gun;
				HighReach = M.Gun->Reach;
			}
		}
		if (High && Best <= HighReach && P.P.Z >= 150.0)
		{
			// (the one burst laid on it: this one falls to it)
			Shot S;
			S.Kind = ShotKind::Burst;
			S.Ship = By->Id;
			S.At = P.P + Vec(Roll.U(-4.0, 4.0), Roll.U(-4.0, 4.0), Roll.U(-2.0, 3.0));
			S.Size = High->Mm;
			S.What = High->Name;
			Out.push_back(S);
			Count.Bursts++;
			Count.KillsHeavy++;
			return true;
		}
		return false;
	}
}
