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
//      rougher than today, no metal where her file's is under SheenMetalFrom (0.4: her paint's 0.06 and 0.18, most
//      rivet heads' 0.35; the points her file gives more are counted and printed), the untouched points exactly today's;
//   S4 no garbage between corners: along every edge of her near and far meshes, roughness, specular and metal stay
//      between the corners' own;
//   S5 the launch word's numbers;
//   S6 (given build_content.py) M_AirPaint's nodes carry the law's names, defaults and constants;
//   S7 (given the source folder) the Unreal side, read from its source: the row off unless asked, nothing read or keyed
//      with it off.
#include "Battle/AirSheen.h"
#include "Battle/AirSkin.h"
#include "Battle/AirUnitColours.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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
	Check(Sh.CoatGloss == 39 && Sh.CoatMetal == 18, "her coat's finish 39/18 (the model file of 26 November)", std::to_string(Sh.CoatGloss) + "/" + std::to_string(Sh.CoatMetal));
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
			int Floored = 0, Glass = 0, Fittings = 0, Untouched = 0, Smooth = 0, Rough = 0, Spec = 0, Under = 0, Metal = 0, Kept = 0, CoatMetal = 0, RivetMetal = 0;
			for (size_t K = 0; K < L.Count(); ++K)
			{
				const AirSheen::Carried C = CarryAt(L, SkinL, K);
				const AirSheen::Finish F = AirSheen::Law(C.U, C.V, 0.9, 0.15, T, 0.0);
				const int Paint = SkinL.PaintAt(K, Me.Paints[K]);
				const bool bGlass = L.Gloss[K] < 12;
				Glass += bGlass ? 1 : 0;
				Floored += !bGlass && F.Rough == T.Floor ? 1 : 0;
				Smooth += !bGlass && F.Rough < T.Floor - 1e-12 ? 1 : 0;
				Rough += F.Rough > 0.9 + 1e-12 ? 1 : 0;
				Spec += F.Spec < 0.15 - 1e-12 || F.Spec > 0.25 + 1e-12 ? 1 : 0;
				// (metal only from SheenMetalFrom up: her file's paint metal (0.06, 0.18) and most rivet heads' (0.35) carry none;
				// what her file gives 0.4 or more is kept, and counted where it falls on her coat or her rivets)
				Under += L.Metal[K] < 40 && F.Metal != 0.0 ? 1 : 0;
				CoatMetal += (Paint == 0 || Paint == 1 || Paint == 2) && F.Metal != 0.0 ? 1 : 0;
				RivetMetal += Paint == 42 && F.Metal != 0.0 ? 1 : 0;
				if (L.Metal[K] >= 45 && C.U >= 0.05f)
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
			Check(Spec == 0, Name + ": specular 0.15 to 0.25");
			Check(Under == 0, Name + ": no metal where her file's is under 0.4 (her paint, most rivet heads)", std::to_string(Under));
			Check(Fittings > 0 && Metal == 0, Name + ": her fittings' metal her file's own", std::to_string(Fittings) + " fitting points");
			Check(Untouched > 0 && Kept == 0, Name + ": the untouched points (gloss 90) exactly today's", std::to_string(Untouched));
			std::printf("    %s: %d points at the floor, %d glass, %d fittings with metal, %d untouched; metal on her coat (paints 0-2) at %d points, on rivet heads (42) at %d\n",
				Name.c_str(), Floored, Glass, Fittings, Untouched, CoatMetal, RivetMetal);
		}
		// (a point the file does not hold, her star and bar laid after it, wears her coat's finish)
		const AirSheen::Carried Coat = AirSheen::Carry(Sh.CoatGloss, Sh.CoatMetal, 1.0);
		const AirSheen::Finish F = AirSheen::Law(Coat.U, Coat.V, 0.9, 0.15, T, 0.0);
		Check(std::fabs(F.Rough - 0.45) < 1e-6 && F.Metal == 0.0 && std::fabs(F.Spec - 0.25) < 1e-12, "her coat's finish (39/18): 0.45 rough (her own 0.39 floored), 0.25 specular, no metal", Num(F.Rough));
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
		Check(AirSheen::ParseTune("0.5,0.6,0.2,0.8", T, W) && T.Gain == 0.5 && T.Floor == 0.6 && T.Spec == 0.2 && T.Metal == 0.8 && T.Glare == 1.0 && T.Graze == 0.0 && T.MetalFrom == 0.4,
			"four given, the rest kept");
		T = AirSheen::Tune();
		Check(AirSheen::ParseTune(",0.5,,", T, W) && T.Gain == 1.0 && T.Floor == 0.5 && T.Spec == 0.25, "empty words keep their numbers");
		T = AirSheen::Tune();
		Check(AirSheen::ParseTune("", T, W) && T.Gain == 1.0 && T.Floor == 0.45 && T.MetalFrom == 0.4, "nothing given, nothing changed");
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
	std::printf("air_b17_sheen_test: %s\n", Bad ? "FAILED" : "ok");
	return Bad ? 1 : 0;
}
