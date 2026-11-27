// The B-17 paint sheen's test (the Battle rules card's "B-17 paint sheen", row flyB17Sheen; Brian, 26 November 2026:
// "make the new ones work", the look he approved on 25 November). Her own finish a point (the model thread's gloss and
// metal) is read from Content/Data/AirSkin/B17Fine-sheen.json (Battle/AirSheen, made by Tools/make_air_skin.py
// --sheen-out in the same run as her overlay), carried in her meshes' first texture coordinate and turned into roughness,
// specular and metal by M_AirPaint (Tools/build_content.py, its "if air:" block). Usage:
//   air_b17_sheen_test <Content/Models/B17Fine.json> <AirSkin/B17Fine-sheen.json> <AirSkin/B17Fine.json>
//                      [<Source/TheaterCommand>] [<Tools/build_content.py>]
//   S1 the sheen file reads with her counts and is her overlay's (the same model thread's file, the same base); each
//      fault refused;
//   S2 the law: nothing carried (U = V = 0) is PlaneInk's numbers exactly, whatever the ink, the tune and the guard;
//      the guard whole gives the ink's; the glare guard at a low sun's mirror, none with the sun high, the grazing
//      guard off unless asked;
//   S3 her own points under the default tune: her paint no smoother than the floor (her glass alone smoother), never
//      rougher than today, no metal where her file's is 0.5 or under (SheenMetalFrom 0.5, the art director's 27
//      November 00:16Z ruling: metal on true fittings only; the points that carry it are counted and printed), the untouched points exactly today's;
//   S4 no garbage between corners: along every edge of her near and far meshes, roughness, specular and metal stay
//      between the corners' own;
//   S5 the launch word's numbers;
//   S6 (given build_content.py) M_AirPaint's nodes carry the law's names, defaults and constants;
//   S7 (given the source folder) the Unreal side, read from its source: the row off unless asked, nothing read or keyed
//      with it off;
//   S8 metal on her true fittings alone (the art director, 27 November 2026 00:16Z; the lead's ruling R1): no point of
//      a paint outside the file's metal_paints (the tool's FITTINGS) carries metal, at either level, and the points
//      carrying all of their own at the start 0.5 counted per paint.
#include "Battle/AirSheen.h"
#include "Battle/AirSkin.h"
#include "Battle/AirUnitColours.h"
#include "Battle/LandJson.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Bad = 0;

	void Check(bool bOk, const std::string& What, const std::string& Got = "")
	{
		std::printf("  %s %s%s%s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.empty() ? "" : ": ", Got.c_str());
		if (!bOk) ++Bad;
	}

	bool ReadText(const std::string& Path, std::string& Out)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In) return false;
		std::stringstream Buf;
		Buf << In.rdbuf();
		Out = Buf.str();
		return true;
	}

	int Count(const std::string& Text, const std::string& What)
	{
		int N = 0;
		for (size_t At = Text.find(What); At != std::string::npos; At = Text.find(What, At + 1)) ++N;
		return N;
	}

	std::string Swapped(const std::string& Text, const std::string& From, const std::string& To)
	{
		const size_t At = Text.find(From);
		if (At == std::string::npos) return std::string();
		return Text.substr(0, At) + To + Text.substr(At + From.size());
	}

	std::string Num(double V)
	{
		char B[32];
		std::snprintf(B, sizeof B, "%.4f", V);
		return B;
	}

	/** A fixed little generator (no clock): the same tunes every run. */
	struct Lcg
	{
		uint64_t State = 0x2545F4914F6CDD1Dull;
		double Next()
		{
			State = State * 6364136223846793005ull + 1442695040888963407ull;
			return static_cast<double>(State >> 11) / 9007199254740992.0;
		}
	};

	bool Same(const AirSheen::Finish& A, double Rough, double Spec, double Metal)
	{
		return A.Rough == Rough && A.Spec == Spec && A.Metal == Metal;
	}

	bool Near(const AirSheen::Finish& A, double Rough, double Spec, double Metal)
	{
		return std::fabs(A.Rough - Rough) <= 1e-12 && std::fabs(A.Spec - Spec) <= 1e-12 && std::fabs(A.Metal - Metal) <= 1e-12;
	}

	AirSheen::Vec3 Way(double Up, double Round)
	{
		const double D = 3.14159265358979323846 / 180.0;
		return { std::cos(Up * D) * std::cos(Round * D), std::cos(Up * D) * std::sin(Round * D), std::sin(Up * D) };
	}

	/** One point's carried pair under her overlay's shade (at strength 1). */
	AirSheen::Carried CarryAt(const AirSheen::Level& L, const AirSkin::Level& Skin, size_t K)
	{
		return AirSheen::Carry(L.Gloss[K], L.Metal[K], AirSkin::ShadeFactor(Skin.Shade[K], 1.0));
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: air_b17_sheen_test <B17Fine.json> <AirSkin/B17Fine-sheen.json> <AirSkin/B17Fine.json> [<Source/TheaterCommand>] [<build_content.py>]\n");
		return 2;
	}
	std::string BaseText, SheenText, SkinText;
	if (!ReadText(Argv[1], BaseText) || !ReadText(Argv[2], SheenText) || !ReadText(Argv[3], SkinText))
	{
		std::fprintf(stderr, "cannot read the files\n");
		return 2;
	}
	AirUnitColours::Model Base;
	std::string Why;
	if (!AirUnitColours::ReadModel(BaseText, "B17Fine", Base, Why))
	{
		std::fprintf(stderr, "B17Fine.json does not read: %s\n", Why.c_str());
		return 2;
	}
	const size_t NearN = Base.Near.Count(), FarN = Base.Far.Count();
	AirSkin::Overlay Skin;
	if (!AirSkin::Read(SkinText, NearN, FarN, Base.Colours.size(), Skin, Why))
	{
		std::fprintf(stderr, "the overlay does not read: %s\n", Why.c_str());
		return 2;
	}

	std::printf("S1 the sheen file reads, her overlay's\n");
	Check(NearN == 143899 && FarN == 15534, "her counts", std::to_string(NearN) + " / " + std::to_string(FarN));
	AirSheen::Sheen Sh;
	const bool bRead = AirSheen::Read(SheenText, NearN, FarN, Sh, Why);
	Check(bRead, "the sheen file reads", Why);
	if (!bRead) return 1;
	Check(Sh.Of == "B17Fine" && Sh.Near.Count() == NearN && Sh.Far.Count() == FarN, "laid on B17Fine, one a point");
	Check(Sh.SourceSha256 == Skin.SourceSha256 && Sh.BaseSha256 == Skin.BaseSha256 && Sh.BaseFnv64 == Skin.BaseFnv64 && Sh.BaseFnv64 == AirSkin::Fnv64(BaseText),
		"made from the overlay's very files (source and base sha256, the base's FNV-1a 64)", Sh.SourceSha256.substr(0, 16));
	Check(Sh.CoatGloss == 39 && Sh.CoatMetal == 0, "her coat's finish 39/0 (her gloss the model file's; no metal, her coat no fitting)", std::to_string(Sh.CoatGloss) + "/" + std::to_string(Sh.CoatMetal));
	Check(Count(SheenText, "mark") == 0 && Count(SheenText, "month") == 0 && Count(SheenText, "year") == 0, "no marks, months or years in it");
	{
		struct Fault
		{
			const char* What;
			std::string Text;
			size_t Near, Far;
		};
		const std::vector<Fault> Faults = {
			{ "not JSON", SheenText.substr(0, SheenText.size() / 2), NearN, FarN },
			{ "near one point short (the counts asked one more)", SheenText, NearN + 1, FarN },
			{ "far one point more (the counts asked one fewer)", SheenText, NearN, FarN - 1 },
			{ "a gloss of 101", Swapped(SheenText, "\"gloss\":[", "\"gloss\":[101,"), NearN + 1, FarN },
			{ "a fractional metal", Swapped(SheenText, "\"metal\":[", "\"metal\":[3.5,"), NearN + 1, FarN },
			{ "a metal of -1", Swapped(SheenText, "\"metal\":[", "\"metal\":[-1,"), NearN + 1, FarN },
			{ "a month key", Swapped(SheenText, "\"sheen_of\"", "\"month\":4,\"sheen_of\""), NearN, FarN },
			{ "a mark key", Swapped(SheenText, "\"sheen_of\"", "\"marks\":[],\"sheen_of\""), NearN, FarN },
			{ "a from key in a level", Swapped(SheenText, "\"gloss\":[", "\"from\":\"1944-04\",\"gloss\":["), NearN, FarN },
			{ "another model's", Swapped(SheenText, "\"sheen_of\":\"B17Fine\"", "\"sheen_of\":\"B17\""), NearN, FarN },
			{ "no far", Swapped(SheenText, "\"far\":{", "\"away\":{"), NearN, FarN },
			{ "no metal", Swapped(SheenText, "\"metal\":[", "\"metals\":["), NearN, FarN },
			{ "a coat out of range", Swapped(SheenText, "\"coat\":[", "\"coat\":[140,0],\"was\":["), NearN, FarN },
			{ "no source sha", Swapped(SheenText, "\"source_sha256\"", "\"source\""), NearN, FarN },
		};
		for (const Fault& F : Faults)
		{
			AirSheen::Sheen No;
			std::string W;
			Check(!F.Text.empty() && !AirSheen::Read(F.Text, F.Near, F.Far, No, W), std::string("refused: ") + F.What, W);
		}
	}

	std::printf("S2 the law: nothing carried is PlaneInk's numbers exactly\n");
	{
		std::vector<AirSheen::Tune> Tunes(1);
		Lcg R;
		for (int K = 0; K < 50; ++K)
		{
			AirSheen::Tune T;
			T.Gain = 1.5 * R.Next();
			T.Floor = 0.9 * R.Next();
			T.Spec = 0.5 * R.Next();
			T.Metal = 1.5 * R.Next();
			T.Glare = R.Next();
			T.Graze = R.Next();
			T.MetalFrom = R.Next();
			Tunes.push_back(T);
		}
		bool bZero = true, bWhole = true;
		const double Roughs[] = { 0.9, 0.78, 0.6, 0.45, 0.3, 0.0 };
		const double Specs[] = { 0.15, 0.5 };
		const double Guards[] = { 0.0, 0.3, 1.0 };
		for (const AirSheen::Tune& T : Tunes)
		{
			for (const double Ir : Roughs)
			{
				for (const double Is : Specs)
				{
					for (const double G : Guards)
					{
						bZero = bZero && Same(AirSheen::Law(0.0, 0.0, Ir, Is, T, G), Ir, Is, 0.0);
					}
					for (int U = 0; U <= 18; ++U)
					{
						for (int V = 0; V <= 10; ++V)
						{
							bWhole = bWhole && Near(AirSheen::Law(U * 0.05, V * 0.1, Ir, Is, T, 1.0), Ir, Is, 0.0);
						}
					}
				}
			}
		}
		Check(bZero, "Law(0, 0) == (ink roughness, ink specular, 0) exactly: 6 inks x 2 speculars x 3 guards x 51 tunes");
		Check(bWhole, "the guard whole: the ink's numbers wherever U and V (within 1e-12)");
		const AirSheen::Tune Default;
		const AirSheen::Vec3 Up{ 0.0, 0.0, 1.0 };
		const AirSheen::Vec3 Sun10 = Way(10.0, 0.0);
		const AirSheen::Vec3 Mirror10 = Way(10.0, 180.0);
		Check(AirSheen::GuardOf(Up, Mirror10, Sun10, Default) >= 1.0 - 1e-9, "glare guard whole at a 10 degree sun's exact mirror", Num(AirSheen::GuardOf(Up, Mirror10, Sun10, Default)));
		{
			bool bHigh = true;
			for (const double Up30 : { 30.5, 35.0, 45.0, 70.0 })
			{
				bHigh = bHigh && AirSheen::GuardOf(Up, Way(Up30, 180.0), Way(Up30, 0.0), Default) == 0.0;
			}
			Check(bHigh, "no guard with the sun over 30 degrees, even at its mirror");
		}
		const AirSheen::Vec3 Along = Way(5.0, 0.0); // (a look along her skin, the sun behind the eye: far from its mirror)
		AirSheen::Tune Grazed;
		Grazed.Graze = 1.0;
		Check(AirSheen::GuardOf(Up, Along, Sun10, Default) == 0.0 && AirSheen::GuardOf(Up, Along, Sun10, Grazed) == 1.0,
			"a 5 degree grazing look: no guard by default, whole with the grazing guard asked");
		Check(AirSheen::GuardOf(Up, Way(40.0, 0.0), Sun10, Default) == 0.0, "away from the mirror and not grazing: no guard");
		const AirSheen::Vec3 None{};
		const double Nothing = AirSheen::GuardOf(Up, Mirror10, None, Default);
		Check(std::isfinite(Nothing) && Nothing == 0.0, "no sun set (0,0,0): no guard, never NaN", Num(Nothing));
		AirSheen::Tune Unglared;
		Unglared.Glare = 0.0;
		Check(AirSheen::GuardOf(Up, Mirror10, Sun10, Unglared) == 0.0, "the glare guard off (launch word): none at the mirror");
	}

	std::printf("S3 her own points, the default tune, PlaneInk 0.9/0.15, no guard, her shade whole\n");
	{
		const AirSheen::Tune T;
		for (const bool bFar : { false, true })
		{
			const AirSheen::Level& L = Sh.LevelOf(bFar);
			const AirSkin::Level& SkinL = Skin.LevelOf(bFar);
			const AirUnitColours::Mesh& Me = bFar ? Base.Far : Base.Near;
			int Floored = 0, Glass = 0, Fittings = 0, Untouched = 0, Smooth = 0, Rough = 0, Spec = 0, Under = 0, Metal = 0, Kept = 0, CoatMetal = 0, RivetMetal = 0, GlassDull = 0;
			for (size_t K = 0; K < L.Count(); ++K)
			{
				const AirSheen::Carried C = CarryAt(L, SkinL, K);
				const AirSheen::Finish F = AirSheen::Law(C.U, C.V, 0.9, 0.15, T, 0.0);
				const int Paint = SkinL.PaintAt(K, Me.Paints[K]);
				const bool bGlass = L.Gloss[K] < 12;
				Glass += bGlass ? 1 : 0;
				GlassDull += bGlass && !(F.Rough < 0.2) ? 1 : 0;
				Floored += !bGlass && F.Rough == T.Floor ? 1 : 0;
				Smooth += !bGlass && F.Rough < T.Floor - 1e-12 ? 1 : 0;
				Rough += F.Rough > 0.9 + 1e-12 ? 1 : 0;
				Spec += F.Spec < 0.15 - 1e-12 || F.Spec > 0.25 + 1e-12 ? 1 : 0;
				// (metal only from SheenMetalFrom 0.5 up: a point at 0.45 carries none, one at 0.55 all of its own; what carries it
				// is counted where it falls on her coat or her rivets)
				Under += L.Metal[K] <= 50 && F.Metal != 0.0 ? 1 : 0;
				CoatMetal += (Paint == 0 || Paint == 1 || Paint == 2) && F.Metal != 0.0 ? 1 : 0;
				RivetMetal += Paint == 42 && F.Metal != 0.0 ? 1 : 0;
				if (L.Metal[K] >= 55 && C.U >= 0.05f)
				{
					++Fittings;
					Metal += std::fabs(F.Metal - static_cast<double>(C.V)) > 1e-6 ? 1 : 0;
				}
				if (L.Gloss[K] == 90)
				{
					++Untouched;
					Kept += Same(F, 0.9, 0.15, 0.0) ? 0 : 1;
				}
			}
			const std::string Name = bFar ? "far" : "near";
			Check(Smooth == 0, Name + ": no paint smoother than the floor 0.45 (glass alone)", std::to_string(Smooth));
			Check(Rough == 0, Name + ": none rougher than today's 0.9");
			Check(Glass > 0 && GlassDull == 0, Name + ": every glass point glints (under 0.2 rough), her contact shade at full strength", std::to_string(GlassDull) + " of " + std::to_string(Glass));
			Check(Spec == 0, Name + ": specular 0.15 to 0.25");
			Check(Under == 0, Name + ": no metal where her file's is 0.5 or under", std::to_string(Under));
			Check(Fittings > 0 && Metal == 0, Name + ": her fittings' metal her file's own", std::to_string(Fittings) + " fitting points");
			Check(Untouched > 0 && Kept == 0, Name + ": the untouched points (gloss 90) exactly today's", std::to_string(Untouched));
			std::printf("    %s: %d points at the floor, %d glass, %d fittings with metal, %d untouched; metal on her coat (paints 0-2) at %d points, on rivet heads (42) at %d\n",
				Name.c_str(), Floored, Glass, Fittings, Untouched, CoatMetal, RivetMetal);
		}
		// (the start, 0.5: a fitting at 0.45 in her file (her seam lips' line, engine, exhaust) draws no metal; at 0.55 all)
		const AirSheen::Finish At45 = AirSheen::Law(0.3, 0.45, 0.9, 0.15, T, 0.0), At55 = AirSheen::Law(0.3, 0.55, 0.9, 0.15, T, 0.0);
		Check(T.MetalFrom == 0.5 && At45.Metal == 0.0 && std::fabs(At55.Metal - 0.55) < 1e-9, "the start 0.5: Metal(V = 0.45) == 0, Metal(V = 0.55) == 0.55", Num(At55.Metal));
		// (a point the file does not hold, her star and bar laid after it, wears her coat's finish)
		const AirSheen::Carried Coat = AirSheen::Carry(Sh.CoatGloss, Sh.CoatMetal, 1.0);
		const AirSheen::Finish F = AirSheen::Law(Coat.U, Coat.V, 0.9, 0.15, T, 0.0);
		Check(std::fabs(F.Rough - 0.45) < 1e-6 && F.Metal == 0.0 && std::fabs(F.Spec - 0.25) < 1e-12, "her coat's finish (39/0): 0.45 rough (her own 0.39 floored), 0.25 specular, no metal", Num(F.Rough));
	}

	std::printf("S8 metal on her true fittings alone (the art director's ruling): her file's metal_paints, the base's paint names\n");
	{
		LandJson::Json Doc;
		std::string W;
		const bool bDoc = LandJson::Parse(SheenText, Doc, W);
		const std::vector<std::string> Eight = { "steel", "blade", "line", "engine", "exhaust", "gunmetal", "hub", "head" };
		std::vector<std::string> Kept;
		for (size_t K = 0; bDoc && K < Doc["metal_paints"].Count(); ++K)
		{
			Kept.push_back(Doc["metal_paints"].At(K).Str(""));
		}
		Check(bDoc && Kept == Eight, "metal_paints: steel, blade, line, engine, exhaust, gunmetal, hub, head", W);
		Check(Base.PaintNames.size() == Base.Colours.size(), "the base names every paint", std::to_string(Base.PaintNames.size()));
		const std::map<std::string, int> WantNear = { { "line", 9792 }, { "gunmetal", 2474 }, { "steel", 1620 }, { "head", 1224 }, { "hub", 832 }, { "blade", 264 } };
		const std::map<std::string, int> WantFar = { { "steel", 624 }, { "blade", 264 }, { "gunmetal", 178 }, { "hub", 160 } };
		for (const bool bFar : { false, true })
		{
			const AirSheen::Level& L = Sh.LevelOf(bFar);
			const AirSkin::Level& SkinL = Skin.LevelOf(bFar);
			const AirUnitColours::Mesh& Me = bFar ? Base.Far : Base.Near;
			int Off = 0, Total = 0;
			std::map<std::string, int> Whole;
			for (size_t K = 0; K < L.Count(); ++K)
			{
				const int Paint = SkinL.PaintAt(K, Me.Paints[K]);
				const std::string Name = Paint >= 0 && static_cast<size_t>(Paint) < Base.PaintNames.size() ? Base.PaintNames[static_cast<size_t>(Paint)] : std::string("?");
				const bool bFitting = std::find(Kept.begin(), Kept.end(), Name) != Kept.end();
				Off += !bFitting && L.Metal[K] > 0 ? 1 : 0;
				if (L.Metal[K] >= 55)
				{
					++Whole[Name];
					++Total;
				}
			}
			const std::string Lv = bFar ? "far" : "near";
			std::string Got;
			for (const auto& KV : Whole) Got += KV.first + " " + std::to_string(KV.second) + ", ";
			Check(Off == 0, Lv + ": no point of a paint outside the eight carries metal (no rivet, seam or paint point over the start)", std::to_string(Off));
			Check(Whole == (bFar ? WantFar : WantNear) && Total == (bFar ? 1226 : 16206), Lv + ": metal 0.55 or more on her fittings alone, counted", Got + std::to_string(Total));
		}
	}

	std::printf("S4 no garbage between corners\n");
	{
		const AirSheen::Tune T;
		for (const bool bFar : { false, true })
		{
			const AirSheen::Level& L = Sh.LevelOf(bFar);
			const AirSkin::Level& SkinL = Skin.LevelOf(bFar);
			const std::vector<int>& Tri = (bFar ? Base.Far : Base.Near).Triangles;
			long Edges = 0, Out = 0;
			for (size_t T3 = 0; T3 + 2 < Tri.size(); T3 += 3)
			{
				for (int E = 0; E < 3; ++E)
				{
					const size_t A = static_cast<size_t>(Tri[T3 + E]), B = static_cast<size_t>(Tri[T3 + (E + 1) % 3]);
					if (A >= L.Count() || B >= L.Count()) continue;
					const AirSheen::Carried Ca = CarryAt(L, SkinL, A), Cb = CarryAt(L, SkinL, B);
					const AirSheen::Finish Fa = AirSheen::Law(Ca.U, Ca.V, 0.9, 0.15, T, 0.0), Fb = AirSheen::Law(Cb.U, Cb.V, 0.9, 0.15, T, 0.0);
					++Edges;
					for (const double W : { 0.25, 0.5, 0.75 })
					{
						const double U = Ca.U + (Cb.U - Ca.U) * W, V = Ca.V + (Cb.V - Ca.V) * W;
						const AirSheen::Finish F = AirSheen::Law(U, V, 0.9, 0.15, T, 0.0);
						auto Between = [](double X, double P, double Q) { return X >= std::min(P, Q) - 1e-12 && X <= std::max(P, Q) + 1e-12; };
						Out += Between(F.Rough, Fa.Rough, Fb.Rough) && Between(F.Spec, Fa.Spec, Fb.Spec) && Between(F.Metal, Fa.Metal, Fb.Metal) ? 0 : 1;
					}
				}
			}
			Check(Edges > 0 && Out == 0, std::string(bFar ? "far" : "near") + ": every edge at a quarter, half, three quarters", std::to_string(Edges) + " edges, " + std::to_string(Out) + " outside");
		}
	}

	std::printf("S5 the launch word's numbers\n");
	{
		AirSheen::Tune T;
		std::string W;
		Check(AirSheen::ParseTune("0.5,0.6,0.2,0.8", T, W) && T.Gain == 0.5 && T.Floor == 0.6 && T.Spec == 0.2 && T.Metal == 0.8 && T.Glare == 1.0 && T.Graze == 0.0 && T.MetalFrom == 0.5,
			"four given, the rest kept");
		T = AirSheen::Tune();
		Check(AirSheen::ParseTune(",0.5,,", T, W) && T.Gain == 1.0 && T.Floor == 0.5 && T.Spec == 0.25, "empty words keep their numbers");
		T = AirSheen::Tune();
		Check(AirSheen::ParseTune("", T, W) && T.Gain == 1.0 && T.Floor == 0.45 && T.MetalFrom == 0.5, "nothing given, nothing changed");
		T = AirSheen::Tune();
		Check(!AirSheen::ParseTune("x", T, W) && !W.empty() && T.Gain == 1.0, "a word not a number: said, kept", W);
		T = AirSheen::Tune();
		Check(!AirSheen::ParseTune("2,2,2,2,2,2,2", T, W) && !W.empty() && T.Gain == 1.5 && T.Floor == 0.9 && T.Spec == 0.5 && T.Metal == 1.5 && T.Glare == 1.0 && T.Graze == 1.0
				&& T.MetalFrom == 1.0,
			"every number clamped, said");
		T = AirSheen::Tune();
		Check(!AirSheen::ParseTune("nan,inf,-inf,1e999", T, W) && T.Gain == 1.0 && T.Floor == 0.45 && T.Spec == 0.25 && T.Metal == 1.0, "never NaN or infinite: kept", W);
		T = AirSheen::Tune();
		Check(!AirSheen::ParseTune(",,,,,,,1", T, W) && !W.empty(), "an eighth number said");
		T = AirSheen::Tune();
		Check(AirSheen::ParseTune(",,,,,1", T, W) && T.Graze == 1.0 && T.Glare == 1.0, "the sixth is the grazing guard");
	}
	if (Argc >= 6)
	{
		std::printf("S6 M_AirPaint's nodes (Tools/build_content.py, its \"if air:\" block)\n");
		std::string Script;
		Check(ReadText(Argv[5], Script), "build_content.py read");
		const size_t From = Script.find("\n    if air:\n");
		const size_t To = From == std::string::npos ? std::string::npos : Script.find("\n    finish(m)\n", From);
		const std::string Block = From == std::string::npos || To == std::string::npos ? std::string() : Script.substr(From, To - From);
		Check(!Block.empty(), "the block found");
		// (every scalar(m, "<Name>", <default>, ...) in it, one a line)
		struct Param
		{
			std::string Name;
			double Default;
		};
		std::vector<Param> Params;
		for (size_t At = Block.find("scalar(m, \""); At != std::string::npos; At = Block.find("scalar(m, \"", At + 1))
		{
			const size_t NameAt = At + 11, NameEnd = Block.find('"', NameAt);
			if (NameEnd == std::string::npos) break;
			const size_t ValueAt = Block.find(", ", NameEnd) + 2;
			Params.push_back({ Block.substr(NameAt, NameEnd - NameAt), std::strtod(Block.c_str() + ValueAt, nullptr) });
		}
		const AirSheen::Tune T;
		const std::vector<Param> Wanted = { { "Roughness", 0.9 }, { "Specular", 0.25 }, { "SheenEdge", T.Edge }, { "SheenGain", T.Gain }, { "SheenGlassBelow", T.GlassBelow },
			{ "SheenFloor", T.Floor }, { "SheenSpec", T.Spec }, { "SheenMetalFrom", T.MetalFrom }, { "SheenMetal", T.Metal }, { "SheenLowSun", T.LowSun }, { "SheenGlare", T.Glare },
			{ "SheenGraze", T.Graze } };
		bool bAll = Params.size() == Wanted.size();
		std::string Got;
		for (const Param& W : Wanted)
		{
			int Found = 0;
			for (const Param& P : Params)
			{
				if (P.Name == W.Name)
				{
					++Found;
					bAll = bAll && std::fabs(P.Default - W.Default) < 1e-12;
				}
			}
			bAll = bAll && Found == 1;
			if (Found != 1) Got += W.Name + " ";
		}
		Check(bAll, "its scalar parameters, each once, with AirSheen::Tune's defaults (Roughness 0.9 and Specular 0.25 as M_MapSurface's)", Got + std::to_string(Params.size()) + " found");
		Check(Block.find("vector(m, \"SheenSun\", (0.0, 0.0, 0.0, 0.0),") != std::string::npos, "SheenSun (0,0,0): no guard until C++ sets the sun");
		Check(Block.find("node(m, unreal.MaterialExpressionTextureCoordinate, ") != std::string::npos && Block.find("carried, -1300, 1450, r=True, g=False, b=False, a=False)") != std::string::npos
				&& Block.find("carried, -1300, 1600, r=False, g=True, b=False, a=False)") != std::string::npos,
			"U and V read from the first texture coordinate's red and green");
		bool bConstants = true;
		for (const char* C : { "-0.9, -1000, 1950", "20.0, -700, 1950", "20.0, -700, 2500", "mirror, -0.9, ", "10.0, -400, 2700", "n_dot_v, -0.15, ", "4.0, -1000, 2850", "4.1667, -850, 3000",
				 "n_dot_s, 2.0, ", "sun, -1.0, ", "depth, -1.0, ", "0.9, -1000, 1450", "rough, 0.34, chipped", "chipped, 0.85, 350, 2500" })
		{
			bConstants = bConstants && Block.find(C) != std::string::npos;
		}
		Check(bConstants, "the law's constants: 0.9, 20, the glare's 0.9 and 10, the graze's 0.15 and 4, the low sun's 4.1667, the mirror's 2 and -1");
		Check(Count(Block, "to_property(") == 3 && Count(Block, "MP_ROUGHNESS") == 1 && Count(Block, "MP_SPECULAR") == 1 && Count(Block, "MP_METALLIC") == 1
				&& Count(Block, "rough = lerp(m, rough, rough_ink, guard,") == 1 && Count(Block, "spec = lerp(m, spec, spec_ink, guard,") == 1 && Count(Block, "metal = lerp(m, metal, 0.0, guard,") == 1,
			"roughness, specular and metal each connected once, each back to the ink's under the guard");
		Check(Count(Script, "    if not air:\n        to_property(times(m, chipped, 0.85, 750, -700), \"\", unreal.MaterialProperty.MP_METALLIC)\n") == 1
				&& Count(Script, "    if not air:\n        to_property(lerp(m, scalar(m, \"Roughness\", 0.9, -450, 200), 0.34, chipped, -250, 200), \"\", unreal.MaterialProperty.MP_ROUGHNESS)\n"
								 "        to_property(scalar(m, \"Specular\", 0.25, -450, 300), \"\", unreal.MaterialProperty.MP_SPECULAR)\n") == 1,
			"M_MapSurface's own metal, roughness and specular kept, under if not air");
		Check(Count(Script, "def build_map_surface(name=\"M_MapSurface\", soot=False, crowd=False, sway=False, air=False):") == 1
				&& Count(Script, "build_map_surface(\"M_AirPaint\", air=True)") == 1 && Count(Script, "(\"M_AirPaint\", build_air_paint_or_tell)") == 1
				&& Count(Script, "M_AirPaint was not built") == 1,
			"M_AirPaint built when missing or asked (TC_REBUILD=M_AirPaint), told when it fails");
	}
	if (Argc >= 5)
	{
		std::printf("S7 the Unreal side, from its source\n");
		const std::string Src = Argv[4];
		std::string Battle, BattleH, Rules, StateH;
		Check(ReadText(Src + "/Battle/AirBattle.cpp", Battle) && ReadText(Src + "/Battle/AirBattle.h", BattleH) && ReadText(Src + "/Battle/LandLive.cpp", Rules)
				&& ReadText(Src + "/Battle/AirBattleState.h", StateH),
			"the air battle's source read");
		// (the row: once, off unless asked, its launch words)
		Check(Count(Rules, "{ TEXT(\"flyB17Sheen\"), TEXT(\"B-17 paint sheen\"), ") == 1 && Count(Rules, "TEXT(\"TheaterAirNoB17Sheen\"), false, TEXT(\"TheaterAirB17Sheen\") }") == 1,
			"the row: off unless asked, -TheaterAirB17Sheen / -TheaterAirNoB17Sheen");
		Check(Count(StateH, "int B17SheenOn = -1;") == 1, "the state's B17SheenOn begins unread");
		// (read in one place, right after her paint's own row, both battle starts)
		Check(Count(Battle, "\tB17SheenRead();") == 2 && Count(BattleH, "void B17SheenRead();") == 1, "B17SheenRead called at both battle starts");
		bool bOrder = true;
		for (size_t At = Battle.find("\tB17SkinRead();"); At != std::string::npos; At = Battle.find("\tB17SkinRead();", At + 1))
		{
			const size_t Sheen = Battle.find("\tB17SheenRead();", At);
			bOrder = bOrder && Sheen != std::string::npos && Sheen - At < 200;
		}
		Check(bOrder, "each B17SheenRead right after its B17SkinRead");
		const size_t Body = Battle.find("void AAirBattle::B17SheenRead()");
		const size_t BodyEnd = Body == std::string::npos ? std::string::npos : Battle.find("\n}\n", Body);
		const std::string Read = Body == std::string::npos || BodyEnd == std::string::npos ? std::string() : Battle.substr(Body, BodyEnd - Body);
		const size_t Unset = Read.find("SheenNow = nullptr;"), Rule = Read.find("if (!FLandLive::RuleNow(TEXT(\"flyB17Sheen\")))"), File = Read.find("FFileHelper::LoadFileToArray("),
					 Said = Read.find("UE_LOG"), Word = Read.find("TEXT(\"TheaterAirB17SheenInk=\")"), On = Read.find("State->B17SheenOn = 1;");
		Check(!Read.empty() && Unset != std::string::npos && Rule != std::string::npos && File != std::string::npos && Said != std::string::npos && Word != std::string::npos
				&& Unset < Rule && Rule < File && Rule < Said && Rule < Word && On != std::string::npos && Word < On,
			"off: unset first, then the row asked before any file is read, any word asked or any line said");
		Check(Count(Battle, "FLandLive::RuleNow(TEXT(\"flyB17Sheen\"))") == 1 && Count(Battle, "AirSheen::Read(") == 1 && Count(Battle, "TEXT(\"TheaterAirB17SheenInk=\")") == 1,
			"the sheen file and its launch word read in B17SheenRead alone");
		Check(Read.find("SheenKept.SourceSha256 != Worn->SourceSha256") != std::string::npos, "refused unless made from her paint's own file");
		// (her shapes: a key of their own, their texture coordinate filled, only with the row on and her paint on)
		Check(Count(Battle, "const bool bSheen = bSkin && State && State->B17SheenOn == 1 && SheenNow;") == 1
				&& Count(Battle, "\tif (bSheen)\n\t{\n\t\tShapeKey += TEXT(\"|sheen\");\n\t}") == 1 && Count(Battle, "+= TEXT(\"|sheen\")") == 1,
			"'|sheen' added to her key only inside if (bSheen)");
		int Onto = 0, OntoGuarded = 0;
		for (size_t At = Battle.find("AirParts::SheenOnto("); At != std::string::npos; At = Battle.find("AirParts::SheenOnto(", At + 1))
		{
			++Onto;
			const size_t If = Battle.rfind("if (bSheen)", At);
			OntoGuarded += If != std::string::npos && At - If < 40 ? 1 : 0;
		}
		Check(Onto == 2 && OntoGuarded == 2, "her finish laid on her near and far shapes, each behind if (bSheen)", std::to_string(Onto) + " / " + std::to_string(OntoGuarded));
		Check(Count(Battle, "static const TArray<FVector2D> None;") == 1 && Count(Battle, "return Carried.Num() == Points && Points > 0 ? Carried : None;") == 1
				&& Count(Battle, "Shape.Sheen.Num() == Count ? FVector2f(") == 1 && Count(Battle, "TArray<FVector2D>(), First->Colors") == 0,
			"an empty sheen draws no texture coordinate on her own mesh and zero on the copies, as before");
		// (the ink: PlaneInk unless the shape is hers and M_AirPaint is made; the sentinel read, never set)
		const size_t Ink = Battle.find("UMaterialInterface* AAirBattle::InkFor(const FString& Shape) const");
		const std::string InkBody = Ink == std::string::npos ? std::string() : Battle.substr(Ink, Battle.find("\n}\n", Ink) - Ink);
		Check(InkBody.find("if (SheenInk && Shape.Contains(TEXT(\"|sheen\")))") != std::string::npos && InkBody.find("return PlaneInk.Get();") != std::string::npos,
			"InkFor gives PlaneInk without SheenInk or without '|sheen'");
		Check(Count(Battle, "SetScalarParameterValue(TEXT(\"SheenEdge\")") == 0 && Count(Battle, "K2_GetScalarParameterValue(TEXT(\"SheenEdge\"))") == 1,
			"the sentinel SheenEdge read and never set");
		const size_t Load = Battle.find("TEXT(\"/Game/Campaign/Materials/M_AirPaint.M_AirPaint\")"), Gate = Battle.find("if (State && State->B17SheenOn == 1 && SheenNow)");
		const size_t Sentinel = Battle.find("K2_GetScalarParameterValue(TEXT(\"SheenEdge\"))"), FirstSet = Battle.find("SheenMade->SetScalarParameterValue(");
		Check(Count(Battle, "M_AirPaint.M_AirPaint") == 1 && Gate != std::string::npos && Load != std::string::npos && Gate < Load && Load - Gate < 300 && Sentinel < FirstSet,
			"M_AirPaint loaded only with the row on, its sentinel read before anything is set");
		// (her glare guard's sun: set once the key light is aimed, after HideTheTable, a stage battle's sun with it; and
		// not before Raise, where a stage battle's TowardSun is still the default 30 degrees)
		const size_t Hide = Battle.find("\tHideTheTable();\n\tif (SheenInk && SheenNow)\n\t{");
		const size_t SunAfter = Hide == std::string::npos ? std::string::npos : Battle.find("\t\tSheenSunSet();", Hide);
		const size_t Raised = Battle.find("\tRaise();\n");
		Check(Count(Battle, "\tHideTheTable();\n") == 1 && Hide != std::string::npos && SunAfter != std::string::npos && SunAfter - Hide < 400 && Raised != std::string::npos && Raised < Hide
				&& Count(Battle, "SheenSunSet();") == 2 && Count(Battle, "Air battle: B-17 paint sheen: on (") == 1 && Battle.find("Air battle: B-17 paint sheen: on (") > Hide,
			"her glare guard's sun set (and said) after Raise and HideTheTable, where the key light is aimed; again only with the deck's shade");
		Check(Count(Battle, "SetMaterial(0, PlaneInk)") == 0 && Count(Battle, "SetMaterial(1, PlaneInk)") == 0 && Count(Battle, "InkFor(Made->Shape)") == 3 && Count(Battle, "InkFor(ShapeKey)") == 1
				&& Count(Battle, "InkFor(Lit->Shape)") == 1 && Count(Battle, "InkFor(Shape)") == 1 && Count(Battle, "if (SheenInk)\n\t{\n\t\tQuad(TEXT(\"AirWarmSheen\")") == 1,
			"every section and copy of her shapes on InkFor; the sheen's ink warmed under the black");
	}
	std::printf("air_b17_sheen_test: %s\n", Bad ? "FAILED" : "ok");
	return Bad ? 1 : 0;
}
