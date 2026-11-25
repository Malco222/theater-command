// The men's worn look (release 2, behind tc.Look.Worn: LandMen::Crowd::SetWearMix), checked on the soldiers thread's own
// files (Content/Models/Soldiers) and armies.json, with no engine. The art director's 25 Nov numbers (section 2 of
// battle-sky-and-worn-numbers.md): a man's boots and knees take 30 to 50 in 100 of the field's own ground colour, and
// roughness by part; Brian's 22 Nov: uniforms dirtier the longer a unit is at the front, fresh replacements clean, rain
// darkens the cloth and splashes the mud higher, snow white on the shoulders and helmets.
//
//   OFF: with the mix never set, and with it set off (whatever else it says), every look the crowd makes (each man's
//        and rider's at every level, the banked far sets, the vertex-animation body sets and their helmets) is the
//        same to the byte, with the same sets numbered the same, and no roughness; a file read without its wear keeps
//        none of it. (worn.sh also builds --dump against the code before the worn look and compares every byte.)
//   ON:  no rain, sand (221, 210, 180), the art director's band as she ruled it on 25 November (for every man in the
//        line): the American's, the German's and the Soviet's boots (paint 2 below the knee: the files paint the eyes as
//        boots at mid and far) move 30 in 100 of the way to the soil (the mean over their points) by W 0.75 and no more
//        than 50 at W 1; their knee fronts run 0 to 9 under the boots at W 1 (she asks 5 to 8, as the American's do: his
//        read 8.9); a fresh man (W 0.1) under 6 in 100. Read at the default W 0.8 too (every man's with the worn trial
//        off): at least 30. (The German and Soviet files' boots were raised to the American's by dirt_band.py.)
//   Face: the face's detail face_detail.py laid in the mesh's tint (Soldier::Detail) is kept with it on: at W 0 each of
//        its points is the OFF colour times the clean shadow over the baked one and the cast, within 2 bytes; at W 0.6
//        on the files' own dust it reads as OFF within a few bytes, the darkest of it too, and the eyes; a gore twin
//        takes its file's (TakeFaceDetail, as Crowd::AddGore) and its face is then its file's.
//   Roughness: each point's byte decodes to its part's table (within 0.01) where nothing is on it (W 0), and is 1 to
//        253 whatever the weather; none without bRough.
//   Rain 1 darkens the cloth and splashes the mud higher (more points over a fifth of the way, more of them over the
//        knee); snow 1 whitens the helmets' crowns and the shoulders; the gore paints take none of it.
//   Sets: two men whose wear differs never share a set (BankKey, BodyKey) nor its look, where with it off they did;
//        a rider dressed as a man (AddRiderLike) is his look to the byte; a mix set again after the looks were made makes
//        them again (MeshNo), and set off again gives the looks of before, to the byte.
//
// Usage: land_worn_men_test <soldiers folder> <armies.json> <pose_bank.json>
//        land_worn_men_test --dump <out file> <soldiers folder> <armies.json> <pose_bank.json>
//          (every look with the mix never set, for worn.sh's comparison; built with -DLAND_WORN_OLD it uses only what
//          the crowd had before the worn look, so the same program runs against the code before it)
#include "Battle/LandMen.h"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace LandMen;

namespace
{
	int Failures = 0;

	[[maybe_unused]] void Fail(const char* Format, ...)
	{
		va_list Args;
		va_start(Args, Format);
		std::printf("FAIL: ");
		std::vprintf(Format, Args);
		std::printf("\n");
		va_end(Args);
		++Failures;
	}

	std::string ReadFile(const std::string& Path)
	{
		std::ifstream F(Path, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	using Files = std::map<std::string, std::shared_ptr<Soldier>>;

	/** The files by their names (soldier_<Name>.json), each read with or without its wear. */
	bool LoadFiles(const std::string& Folder, const std::vector<std::string>& Names, bool bWear, Files& Out)
	{
		for (const std::string& Name : Names)
		{
			auto S = std::make_shared<Soldier>();
			std::string Err;
			const std::string Text = ReadFile(Folder + "/soldier_" + Name + ".json");
#ifdef LAND_WORN_OLD
			(void)bWear;
			const bool bOk = S->Load(Text, Err);
#else
			const bool bOk = S->Load(Text, Err, bWear);
#endif
			if (!bOk)
			{
				std::printf("soldier_%s.json: %s\n", Name.c_str(), Err.c_str());
				return false;
			}
			Out[Name] = S;
		}
		return true;
	}

	/**
	 * A small battle of the crowd's own: block 1 (side att) and block 2 (side def), Men men each standing at their
	 * rifles, a few metres apart, and the sides' months in the line (AttMonths, DefMonths: the recording's sides.*.worn).
	 */
	std::string Recording(const char* ArmyA, const char* ArmyB, int Men, double AttMonths, double DefMonths)
	{
		std::ostringstream R;
		R << "{\"seed\": 23, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]}, "
		  << "\"sides\": {\"att\": {\"worn\": " << AttMonths << "}, \"def\": {\"worn\": " << DefMonths << "}}, "
		  << "\"blocks\": [{\"id\": 1, \"army\": \"" << ArmyA << "\", \"nat\": \"" << ArmyA << "\", \"side\": \"att\"}, "
		  << "{\"id\": 2, \"army\": \"" << ArmyB << "\", \"nat\": \"" << ArmyB << "\", \"side\": \"def\"}], \"men\": [";
		bool bFirst = true;
		for (int B = 1; B <= 2; ++B)
		{
			for (int K = 0; K < Men; ++K)
			{
				const double X = 6.0 * K, Y = 30.0 * B;
				R << (bFirst ? "" : ", ") << "{\"b\": " << B << ", \"k\": " << (K + 3) << ", \"r\": \"rifle\", \"at\": [[0, " << X << ", " << Y << ", 1.5708], [10, " << X << ", "
				  << Y << ", 1.5708]], \"ch\": [[0, \"fire_stand\", \"none\", 0, 0, 0, 0]]}";
				bFirst = false;
			}
		}
		R << "], \"rounds\": []}";
		return R.str();
	}

	struct Setup
	{
		const Files* F = nullptr;
		std::string Armies, Bank;
		std::string Rec;
		bool bWorn = false;         // uniforms that age (SetWorn), each side's months from the recording
		bool bBank = false;         // the pose bank read before Build, and the mid men banked (SetPoseBankMid)
#ifndef LAND_WORN_OLD
		bool bSetMix = false;
		Crowd::WearMix Mix;
#endif
	};

	std::unique_ptr<Crowd> MakeCrowd(const Setup& Su)
	{
		auto C = std::make_unique<Crowd>();
		C->SetWorn(Su.bWorn);
#ifndef LAND_WORN_OLD
		if (Su.bSetMix) C->SetWearMix(Su.Mix);
#endif
		for (const auto& KV : *Su.F) C->AddSoldier(KV.first, KV.second);
		std::string Err;
		if (!C->ReadArmies(Su.Armies, Err)) { std::printf("armies.json: %s\n", Err.c_str()); return nullptr; }
		if (Su.bBank && !C->ReadPoseBank(Su.Bank, Err)) { std::printf("pose bank: %s\n", Err.c_str()); return nullptr; }
		const GroundFn Flat = [](double, double) { return Ground(); };
		if (!C->Build(Su.Rec, Flat, Err)) { std::printf("build: %s\n", Err.c_str()); return nullptr; }
		if (Su.bBank) C->SetPoseBankMid(true);
		return C;
	}

	// ---- Every look of a crowd, in one order: the dump, and the comparison of two crowds ----------------------------

	void Put(std::vector<uint8_t>& Out, uint32_t V)
	{
		for (int K = 0; K < 4; ++K) Out.push_back(static_cast<uint8_t>((V >> (8 * K)) & 0xFF));
	}
	void PutLook(std::vector<uint8_t>& Out, const Look& Lk)
	{
		Put(Out, static_cast<uint32_t>(Lk.Points));
		Put(Out, static_cast<uint32_t>(Lk.Triangles.size()));
		for (const uint32_t T : Lk.Triangles) Put(Out, T);
		Put(Out, static_cast<uint32_t>(Lk.Paint.size()));
		Out.insert(Out.end(), Lk.Paint.begin(), Lk.Paint.end());
		Put(Out, static_cast<uint32_t>(Lk.Rgb.size()));
		Out.insert(Out.end(), Lk.Rgb.begin(), Lk.Rgb.end());
	}

	/** Every look the crowd makes, as bytes (the men's and riders' own at each level, then each banked far set's, then
	 * each body set's at mid and far and its helmets'), with each man's sets and finish; and how many looks had any
	 * roughness (OutRough; the code before the worn look has none to count). */
	std::vector<uint8_t> LooksOf(Crowd& C, int& OutRough)
	{
		std::vector<uint8_t> Out;
		OutRough = 0;
		auto Add = [&](const Look& Lk) {
			PutLook(Out, Lk);
#ifndef LAND_WORN_OLD
			OutRough += Lk.Rough.empty() ? 0 : 1;
#endif
		};
		Put(Out, static_cast<uint32_t>(C.Count()));
		std::map<int, const Soldier*> FileOfBody;
		for (int M = 0; M < C.Count(); ++M)
		{
			const ManInfo& In = C.Info(M);
			Put(Out, static_cast<uint32_t>(In.Finish + 1));
			Put(Out, static_cast<uint32_t>(In.Face));
			Put(Out, static_cast<uint32_t>(C.BankKey(M) + 1));
			Put(Out, static_cast<uint32_t>(C.BodyKey(M) + 1));
			if (C.BodyKey(M) >= 0) FileOfBody[C.BodyKey(M)] = C.SoldierOf(M);
			for (int L = 0; L < LodCount; ++L) Add(C.LookOf(M, static_cast<Lod>(L)));
		}
		Put(Out, static_cast<uint32_t>(C.BankKeys()));
		for (int K = 0; K < C.BankKeys(); ++K) Add(C.BankLook(K));
		Put(Out, static_cast<uint32_t>(C.BodyKeys()));
		for (int K = 0; K < C.BodyKeys(); ++K)
		{
			const auto Found = FileOfBody.find(K);
			const size_t Finishes = Found != FileOfBody.end() && Found->second ? Found->second->Finishes.size() : 0;
			for (const Lod L : { Mid, Far })
			{
				Add(C.BankBodyLook(K, L));
				for (size_t Fi = 0; Fi < Finishes; ++Fi) Add(C.HelmetLook(K, static_cast<int>(Fi), L));
			}
		}
		return Out;
	}

	/** The crowds the dump and the OFF check make: the Americans and the Germans (both with their finishes), the worn trial
	 * on and off, the pose bank on; then riders of the gore twin's file, the tankers' and the sergeant's, and a rider
	 * dressed as the first man. */
	std::vector<std::unique_ptr<Crowd>> OffCrowds(Setup Su)
	{
		std::vector<std::unique_ptr<Crowd>> Out;
		Su.Rec = Recording("USA", "GER", 5, 0.5, 9.0);
		Su.bBank = true;
		for (const bool bWorn : { false, true })
		{
			Su.bWorn = bWorn;
			auto C = MakeCrowd(Su);
			if (!C) return {};
			uint64_t Seed = 0xBEEF;
			for (const char* Rider : { "USA_gore", "USA_tanker", "USA_nco", "GER" })
			{
				const int M = C->AddRider(Rider, std::string(Rider).substr(0, 3), Seed++);
				if (M < 0) { std::printf("no rider of soldier_%s\n", Rider); return {}; }
			}
			C->AddRiderLike(0);
			Out.push_back(std::move(C));
		}
		return Out;
	}

#ifndef LAND_WORN_OLD
	// ---- The worn look's own checks ---------------------------------------------------------------------------------

	struct C3
	{
		double R = 0, G = 0, B = 0;
	};
	C3 operator-(C3 A, C3 B) { return { A.R - B.R, A.G - B.G, A.B - B.B }; }
	double Dot(C3 A, C3 B) { return A.R * B.R + A.G * B.G + A.B * B.B; }
	C3 RgbAt(const Look& Lk, size_t P) { return { Lk.Rgb[P * 3] / 255.0, Lk.Rgb[P * 3 + 1] / 255.0, Lk.Rgb[P * 3 + 2] / 255.0 }; }
	double Value(C3 A) { return 0.2126 * A.R + 0.7152 * A.G + 0.0722 * A.B; }

	const double Sand[3] = { 221.0 / 255.0, 210.0 / 255.0, 180.0 / 255.0 };
	const double MudOfDust[3] = { 0.45, 0.39, 0.35 };

	/** The recipe's soil at a point: the mix's soil, toward its mud by the rain, in the point's clean shadow. */
	C3 SoilAt(const Mesh& Me, size_t P, double Rain)
	{
		const double Clean = Me.ShadeClean[P] / 255.0;
		double Ch[3];
		for (int K = 0; K < 3; ++K) Ch[K] = Sand[K] * (1.0 + (MudOfDust[K] - 1.0) * Rain) * Clean;
		return { Ch[0], Ch[1], Ch[2] };
	}

	/** How far a point went from Base (its colour with nothing on it) toward Soil (0 none, 1 all the way), along the line
	 * between them; NaN where the two are too alike to tell (under 0.05 apart). */
	double Toward(C3 Base, C3 Now, C3 Soil)
	{
		const C3 D = Soil - Base;
		const double L2 = Dot(D, D);
		return L2 < 0.0025 ? std::nan("") : Dot(Now - Base, D) / L2;
	}

	Crowd::WearMix MixOf(double W, double Rain, double Snow, bool bRough = false)
	{
		Crowd::WearMix M;
		M.bOn = true;
		M.bRough = bRough;
		for (int K = 0; K < 3; ++K) M.Soil[K] = static_cast<float>(Sand[K]);
		M.Rain = static_cast<float>(Rain);
		M.Snow = static_cast<float>(Snow);
		M.W = static_cast<float>(W);
		return M;
	}

	/** A crowd of riflemen (no bank) with that mix, and a rider of each file asked (the same seed, so the same face), his
	 * points the file's own at every level (a rider has no finish and nothing cut). */
	struct Riders
	{
		std::unique_ptr<Crowd> C;
		std::map<std::string, int> Of;
	};
	Riders RidersWith(const Setup& Base, const Crowd::WearMix* Mix, const std::vector<std::string>& Names)
	{
		Setup Su = Base;
		Su.Rec = Recording("USA", "GER", 2, 0.0, 0.0);
		Su.bSetMix = Mix != nullptr;
		if (Mix) Su.Mix = *Mix;
		Riders Out;
		Out.C = MakeCrowd(Su);
		if (!Out.C) return Out;
		for (const std::string& N : Names) Out.Of[N] = Out.C->AddRider(N, N.substr(0, 3), 0x5EED);
		return Out;
	}

	/** The part's roughness the table gives a paint of that file (the test's own copy of SetWearMix's table). */
	double PartRough(const Soldier& S, int Pa)
	{
		static const double Army[15] = { 0.90, 0.90, 0.45, 0.88, 0.80, 0.65, 0.90, 0.55, 0.55, 0.42, 0.92, 0.92, 0.45, 0.95, 0.60 };
		if (Pa < 15) return Army[Pa];
		const auto Found = S.OwnPaintNames.find(Pa);
		if (Found == S.OwnPaintNames.end()) return 0.90;
		const std::string& N = Found->second;
		auto Has = [&N](const char* W) { return N.find(W) != std::string::npos; };
		if (Has("skin")) return 0.55;
		if (Has("eye") || Has("iris")) return 0.35;
		if (Has("blood") || Has("wound")) return 0.35;
		if (Has("bone")) return 0.55;
		if (Has("leather") || Has("boot") || Has("belt")) return 0.45;
		if (Has("helmet")) return 0.65;
		if (Has("steel") || Has("metal") || Has("brass") || Has("badge") || Has("buckle") || Has("rocket") || Has("tin")) return 0.42;
		if (Has("glass") || Has("lens")) return 0.30;
		return 0.90;
	}
	bool IsGore(const Soldier& S, int Pa)
	{
		const auto Found = S.OwnPaintNames.find(Pa);
		if (Found == S.OwnPaintNames.end()) return false;
		const std::string& N = Found->second;
		return N.find("wound") != std::string::npos || N.find("bone") != std::string::npos || N.find("blood") != std::string::npos;
	}

	/** The points of a rider's look (the file's mesh at that level, one for one) that are his boots (paint 2 below the
	 * knee, under 0.8 m standing: at mid and far the files paint the eyes as boots too), and his knee fronts: on a thigh
	 * or a calf, within 10 cm of a calf's head, facing forward (x over 0.3), not skin. */
	void BootsAndKnees(const Soldier& S, Lod L, std::vector<size_t>& Boots, std::vector<size_t>& Knees)
	{
		const Mesh& Me = S.Meshes[L];
		const int Calf[2] = { S.BoneIndex("calf_l"), S.BoneIndex("calf_r") };
		const std::set<int> Legs = { S.BoneIndex("thigh_l"), S.BoneIndex("thigh_r"), Calf[0], Calf[1] };
		for (size_t P = 0; P < static_cast<size_t>(Me.Points); ++P)
		{
			if (Me.Paint[P] == 2 && Me.At[P * 3 + 2] < 0.8f) Boots.push_back(P);
			if (!Legs.count(Me.Part[P]) || Me.Paint[P] == 7 || Me.Facing[P * 3] <= 0.3f) continue;
			bool bNear = false;
			for (const int Cb : Calf)
			{
				if (Cb < 0) continue;
				const Vec3& H = S.Bones[static_cast<size_t>(Cb)].Head;
				const double DX = Me.At[P * 3] - H.X, DY = Me.At[P * 3 + 1] - H.Y, DZ = Me.At[P * 3 + 2] - H.Z;
				bNear = bNear || DX * DX + DY * DY + DZ * DZ < 0.10 * 0.10;
			}
			if (bNear) Knees.push_back(P);
		}
	}

	/** The mean of Toward over Points: the look Now against Base (the same man with nothing on him), the soil SoilAt. */
	double MeanToward(const Look& Base, const Look& Now, const Mesh& Me, const std::vector<size_t>& Points, double Rain, int* OutUsed = nullptr)
	{
		double Sum = 0.0;
		int N = 0;
		for (const size_t P : Points)
		{
			const double T = Toward(RgbAt(Base, P), RgbAt(Now, P), SoilAt(Me, P, Rain));
			if (std::isnan(T)) continue;
			Sum += T;
			++N;
		}
		if (OutUsed) *OutUsed = N;
		return N > 0 ? Sum / N : std::nan("");
	}

	bool SameLook(const Look& A, const Look& B, bool bRoughToo = true)
	{
		return A.Points == B.Points && A.Triangles == B.Triangles && A.Paint == B.Paint && A.Rgb == B.Rgb && (!bRoughToo || A.Rough == B.Rough);
	}
#endif
}

int main(int Argc, char** Argv)
{
	const bool bDump = Argc >= 2 && std::string(Argv[1]) == "--dump";
	const int Arg0 = bDump ? 3 : 1;
	if (Argc < Arg0 + 3)
	{
		std::fprintf(stderr, "usage: land_worn_men_test [--dump <out file>] <soldiers folder> <armies.json> <pose_bank.json>\n");
		return 2;
	}
	const std::string Folder = Argv[Arg0];
	Setup Base;
	Base.Armies = ReadFile(Argv[Arg0 + 1]);
	Base.Bank = ReadFile(Argv[Arg0 + 2]);
	const std::vector<std::string> OffNames = { "USA", "GER", "USA_gore", "USA_tanker", "USA_nco" };
	[[maybe_unused]] const auto T0 = std::chrono::steady_clock::now();
	Files Plain;
	if (!LoadFiles(Folder, OffNames, false, Plain)) return 1;
	Base.F = &Plain;

	if (bDump)
	{
		// (every look with the mix never set: the game with tc.Look.Worn off)
		std::vector<uint8_t> All;
		int Rough = 0;
		for (auto& C : OffCrowds(Base))
		{
			const std::vector<uint8_t> One = LooksOf(*C, Rough);
			All.insert(All.end(), One.begin(), One.end());
		}
		std::ofstream Out(Argv[2], std::ios::binary);
		Out.write(reinterpret_cast<const char*>(All.data()), static_cast<std::streamsize>(All.size()));
		std::printf("dumped %zu bytes of looks to %s\n", All.size(), Argv[2]);
		return Out ? 0 : 1;
	}

#ifdef LAND_WORN_OLD
	std::printf("built against the code before the worn look: --dump only\n");
	return 2;
#else
	Files Worn;
	if (!LoadFiles(Folder, { "USA", "GER", "USA_gore", "USA_tanker", "USA_nco", "USA_bazooka", "SOV" }, true, Worn)) return 1;
	std::printf("soldier files read in %.0f ms\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count());

	// ---- What is read: nothing new without the wear; with it, every level's and every finish's whole ----------------
	{
		for (const auto& KV : Plain)
		{
			const Soldier& S = *KV.second;
			size_t Kept = S.OwnPaintNames.size();
			for (int L = 0; L < LodCount; ++L)
			{
				const Mesh& Me = S.Meshes[L];
				Kept += Me.ShadeClean.size() + Me.Dirt.size() + Me.DirtWet.size() + Me.Wet.size() + Me.Snow.size();
				Kept += S.Detail[L].At.size() + S.Detail[L].Mul.size() + S.Detail[L].EyeAt.size() + S.Detail[L].EyeWas.size();
				for (const Finish& Fi : S.Finishes)
					Kept += Fi.Lods[L].ShadeClean.size() + Fi.Lods[L].Dirt.size() + Fi.Lods[L].DirtWet.size() + Fi.Lods[L].Wet.size() + Fi.Lods[L].Snow.size();
			}
			if (Kept) Fail("soldier_%s read without its wear keeps %zu of it", KV.first.c_str(), Kept);
		}
		for (const auto& KV : Worn)
		{
			const Soldier& S = *KV.second;
			for (int L = 0; L < LodCount; ++L)
			{
				std::vector<const Mesh*> Ms = { &S.Meshes[L] };
				for (const Finish& Fi : S.Finishes) Ms.push_back(&Fi.Lods[L]);
				for (const Mesh* Me : Ms)
				{
					const size_t N = static_cast<size_t>(Me->Points);
					if (Me->ShadeClean.size() != N || Me->Dirt.size() != N || Me->DirtWet.size() != N || Me->Wet.size() != N || Me->Snow.size() != N)
						Fail("soldier_%s's level %d: a mesh's wear is not whole (%zu points)", KV.first.c_str(), L, N);
				}
			}
			if (S.OwnPaintNames.empty()) Fail("soldier_%s read with its wear has no paint names", KV.first.c_str());
		}
		// (the face's detail face_detail.py laid in the tint: near and mid in every file but a gore twin (which takes its
		// file's, Crowd::AddGore), far too in a file with no faces; the eyes' shade it set at near)
		for (const char* N : { "USA", "GER", "USA_nco", "USA_bazooka", "USA_tanker" })
		{
			const Soldier& S = *Worn.at(N);
			for (int L = 0; L < LodCount; ++L)
			{
				const Soldier::FaceDetail& Fd = S.Detail[L];
				const bool bWant = L != Far || S.Faces.size() <= 1;
				if (bWant && (Fd.At.empty() || Fd.Mul.size() != Fd.At.size() * 3)) Fail("soldier_%s's level %d: no face detail read (%zu points)", N, L, Fd.At.size());
			}
			if (S.Detail[Near].EyeAt.size() < 100) Fail("soldier_%s: the eyes' shade face_detail set not read (%zu points)", N, S.Detail[Near].EyeAt.size());
			std::printf("soldier_%s: the face's detail on %zu, %zu and %zu points (near, mid, far), the eyes' shade on %zu\n", N, S.Detail[Near].At.size(), S.Detail[Mid].At.size(),
				S.Detail[Far].At.size(), S.Detail[Near].EyeAt.size());
		}
		if (!Worn.at("USA_gore")->Detail[Near].At.empty()) Fail("the gore twin's file has a face detail of its own: the twin's check below proves nothing");
		const auto& Gn = Worn.at("USA_gore")->OwnPaintNames;
		if (!Gn.count(35) || Gn.at(35) != "wound" || !Gn.count(36) || Gn.at(36) != "bone" || !Gn.count(37) || Gn.at(37) != "blood")
			Fail("the gore twin's paint names are not wound 35, bone 36, blood 37");
		std::printf("read: no wear kept without it; with it every mesh's whole, the own paints named\n");
	}

	// ---- OFF: never set, and set off (with all else on), alike to the byte -----------------------------------------
	{
		const auto T = std::chrono::steady_clock::now();
		Setup Never = Base;
		Setup Off = Base;
		Off.F = &Worn;
		Off.bSetMix = true;
		Off.Mix = MixOf(1.0, 1.0, 1.0, true);
		Off.Mix.bOn = false;
		auto A = OffCrowds(Never);
		auto B = OffCrowds(Off);
		if (A.size() != 2 || B.size() != 2) { Fail("the OFF crowds were not made"); return 1; }
		for (size_t K = 0; K < A.size(); ++K)
		{
			int RoughA = 0, RoughB = 0;
			const std::vector<uint8_t> LA = LooksOf(*A[K], RoughA), LB = LooksOf(*B[K], RoughB);
			if (LA != LB) Fail("OFF (%s): the looks with the mix set off are not those with it never set (%zu and %zu bytes)", K ? "uniforms that age" : "plain", LA.size(), LB.size());
			if (RoughA || RoughB) Fail("OFF: %d and %d looks carry roughness", RoughA, RoughB);
			if (A[K]->BankKeys() != B[K]->BankKeys() || A[K]->BodyKeys() != B[K]->BodyKeys()) Fail("OFF: the sets are not as many");
			std::printf("OFF (%s): %d men and riders, %d banked sets, %d body sets: %zu bytes of looks alike, none with roughness\n", K ? "uniforms that age" : "plain",
				A[K]->Count(), A[K]->BankKeys(), A[K]->BodyKeys(), LA.size());
			// (the alpha a mesh builder writes: 255 everywhere with no roughness)
			const Look& Lk = A[K]->LookOf(0, Near);
			bool bWhole = true;
			for (size_t P = 0; P < static_cast<size_t>(Lk.Points); ++P) bWhole = bWhole && Lk.Alpha(P) == 255;
			if (!bWhole) Fail("OFF: a point's alpha is not 255");
		}
		std::printf("OFF checked in %.0f ms\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T).count());
	}

	// ---- ON, no rain, sand: the boots and knees toward the soil at W 1, W 0.75, the default W 0.8, and fresh ----------
	// (The art director's band, as she ruled it on 25 November, is for every man in the line: his boots 30 in 100 of the
	// way to the field's ground by W 0.75 and no more than 50 at W 1, his knee fronts 5 to 8 under the boots. With
	// tc.Look.Worn on and the worn trial off every man is W 0.8 (LandMen's DefaultWear). The reading is linear in W (each
	// point goes 0.85 x dirt x W of the way): checked, so what is read at one W holds at the others.)
	{
		const std::vector<std::string> Names = { "USA", "GER", "SOV" };
		const Crowd::WearMix Clean = MixOf(0.0, 0.0, 0.0), Veteran = MixOf(1.0, 0.0, 0.0), Floor = MixOf(0.75, 0.0, 0.0),
			Default = MixOf(static_cast<double>(Crowd::DefaultWear), 0.0, 0.0), Fresh = MixOf(0.1, 0.0, 0.0);
		Setup Su = Base;
		Su.F = &Worn;
		std::vector<std::string> Have;
		for (const std::string& N : Names)
		{
			if (Worn.count(N)) Have.push_back(N);
		}
		Riders Bs = RidersWith(Su, &Clean, Have), Vs = RidersWith(Su, &Veteran, Have), Ls = RidersWith(Su, &Floor, Have),
			Ds = RidersWith(Su, &Default, Have), Fs = RidersWith(Su, &Fresh, Have);
		if (!Bs.C || !Vs.C || !Ls.C || !Ds.C || !Fs.C) { Fail("the riders' crowds were not made"); return 1; }
		for (const std::string& N : Have)
		{
			const Soldier& S = *Worn.at(N);
			for (const Lod L : { Near, Mid, Far })
			{
				std::vector<size_t> Boots, Knees;
				BootsAndKnees(S, L, Boots, Knees);
				const Look& B = Bs.C->LookOf(Bs.Of[N], L);
				const Look& V = Vs.C->LookOf(Vs.Of[N], L);
				const Look& Lo = Ls.C->LookOf(Ls.Of[N], L);
				const Look& D = Ds.C->LookOf(Ds.Of[N], L);
				const Look& F = Fs.C->LookOf(Fs.Of[N], L);
				if (B.Points != S.Meshes[L].Points) { Fail("%s's rider is not his file's mesh one for one", N.c_str()); continue; }
				int UsedB = 0, UsedK = 0;
				const double BootsV = MeanToward(B, V, S.Meshes[L], Boots, 0.0, &UsedB), KneesV = MeanToward(B, V, S.Meshes[L], Knees, 0.0, &UsedK);
				const double BootsL = MeanToward(B, Lo, S.Meshes[L], Boots, 0.0);
				const double BootsD = MeanToward(B, D, S.Meshes[L], Boots, 0.0), KneesD = MeanToward(B, D, S.Meshes[L], Knees, 0.0);
				const double BootsF = MeanToward(B, F, S.Meshes[L], Boots, 0.0), KneesF = MeanToward(B, F, S.Meshes[L], Knees, 0.0);
				const char* Level = L == Near ? "near" : L == Mid ? "mid" : "far";
				std::printf("%s %s: boots (%d points) %.1f%% of the way to the sand at W 1, %.1f%% at W 0.75, %.1f%% at the default W %.2f, %.1f%% at W 0.1; "
							"knee fronts (%d points) %.1f%% at W 1, %.1f%% at W %.2f, %.1f%% at W 0.1\n", N.c_str(), Level, UsedB, 100.0 * BootsV, 100.0 * BootsL,
					100.0 * BootsD, static_cast<double>(Crowd::DefaultWear), 100.0 * BootsF, UsedK, 100.0 * KneesV, 100.0 * KneesD,
					static_cast<double>(Crowd::DefaultWear), 100.0 * KneesF);
				// (linear in W, but for the bytes' rounding)
				if (UsedB && std::fabs(BootsL - 0.75 * BootsV) > 0.02) Fail("%s %s: the boots at W 0.75 (%.1f%%) are not 0.75 of W 1's (%.1f%%)", N.c_str(), Level, 100.0 * BootsL, 100.0 * BootsV);
				if (UsedB && std::fabs(BootsD - Crowd::DefaultWear * BootsV) > 0.02) Fail("%s %s: the boots at the default W (%.1f%%) are not %.2f of W 1's (%.1f%%)", N.c_str(), Level, 100.0 * BootsD, static_cast<double>(Crowd::DefaultWear), 100.0 * BootsV);
				if (L == Far) continue;
				if (!(BootsF < 0.06) || !(KneesF < 0.06)) Fail("%s fresh (W 0.1): boots %.1f%%, knees %.1f%% (under 6)", N.c_str(), 100.0 * BootsF, 100.0 * KneesF);
				// (the pass line on the near man, the one the close camera sees: the mid and far levels' dirt is the near's
				// sampled on fewer points, for the reading only)
				if (L != Near) continue;
				if (UsedB < 50 || UsedK < 10) Fail("%s: too few boot (%d) or knee (%d) points to read", N.c_str(), UsedB, UsedK);
				if (!(BootsL >= 0.30)) Fail("%s's boots move %.1f%% toward the soil at W 0.75 (30 at least)", N.c_str(), 100.0 * BootsL);
				if (!(BootsD >= 0.30 && KneesD >= 0.25)) Fail("%s at the default W %.2f: boots %.1f%%, knee fronts %.1f%% (30 and 25 at least)", N.c_str(), static_cast<double>(Crowd::DefaultWear), 100.0 * BootsD, 100.0 * KneesD);
				if (!(BootsV <= 0.50 && KneesV <= 0.50)) Fail("%s at W 1: boots %.1f%%, knee fronts %.1f%% (50 at most)", N.c_str(), 100.0 * BootsV, 100.0 * KneesV);
				if (!(KneesV >= BootsV - 0.09 && KneesV <= BootsV + 0.005)) Fail("%s at W 1: knee fronts %.1f%% against boots %.1f%% (0 to 9 under)", N.c_str(), 100.0 * KneesV, 100.0 * BootsV);
			}
		}
	}

	// ---- The face: the detail face_detail.py laid in the mesh's tint, kept with the worn look on -------------------
	// (The recipe leaves the mesh's own tint out, the baked earthy cast, but the soldiers thread's face_detail.py ran after
	// shade.py and laid the face's lines and shadows in that tint too: the worn look lays them back on, Soldier::Detail.
	// At W 0, dry, each of its points is exactly the OFF colour times the clean shadow over the baked one and the cast
	// (the tint over the detail): within 2 bytes. At W 0.6, dry, on the files' own dust, the baked look's own wear, they
	// read as the OFF man within a few bytes; the points it darkened most (the lash lines, the irises, the lips, the
	// brim's shade) as dark as OFF.)
	{
		const std::vector<std::string> Names = { "USA", "GER", "USA_nco", "USA_tanker" };
		Crowd::WearMix Own = MixOf(0.6, 0.0, 0.0);
		const float Dust[3] = { 0.60f, 0.54f, 0.43f };
		for (int K = 0; K < 3; ++K) Own.Soil[K] = Dust[K];
		const Crowd::WearMix Bare = MixOf(0.0, 0.0, 0.0);
		Setup Su = Base;
		Su.F = &Worn;
		Riders Off = RidersWith(Base, nullptr, Names), On = RidersWith(Su, &Own, Names), On0 = RidersWith(Su, &Bare, Names);
		if (!Off.C || !On.C || !On0.C) { Fail("the faces' crowds were not made"); return 1; }
		const auto Lum = [](const uint8_t* C) { return 0.2126 * C[0] + 0.7152 * C[1] + 0.0722 * C[2]; };
		for (const std::string& N : Names)
		{
			const Soldier& S = *Worn.at(N);
			if (Off.C->Info(Off.Of[N]).Face != On.C->Info(On.Of[N]).Face) { Fail("%s's riders OFF and ON have not one face", N.c_str()); continue; }
			for (int L = 0; L < LodCount; ++L)
			{
				const Soldier::FaceDetail& Fd = S.Detail[L];
				const Mesh& Me = S.Meshes[L];
				if (Fd.At.empty()) continue;
				const Look& A = Off.C->LookOf(Off.Of[N], static_cast<Lod>(L));
				const Look& B = On.C->LookOf(On.Of[N], static_cast<Lod>(L));
				const Look& B0 = On0.C->LookOf(On0.Of[N], static_cast<Lod>(L));
				if (A.Points != Me.Points || B.Points != Me.Points || Me.Tint.size() != static_cast<size_t>(Me.Points) * 3 || Me.Shade.size() != static_cast<size_t>(Me.Points))
				{
					Fail("%s level %d: the riders are not the file's mesh one for one", N.c_str(), L);
					continue;
				}
				double SumOn = 0.0, DarkOff = 0.0, DarkOn = 0.0;
				int MaxOn = 0, MaxBase = 0, Dark = 0;
				for (size_t K = 0; K < Fd.At.size(); ++K)
				{
					const size_t P = static_cast<size_t>(Fd.At[K]);
					const double Clean = Me.ShadeClean[P] / 255.0;
					for (size_t C = 0; C < 3; ++C)
					{
						const int Off8 = A.Rgb[P * 3 + C], On8 = B.Rgb[P * 3 + C], Base8 = B0.Rgb[P * 3 + C];
						SumOn += std::abs(On8 - Off8);
						MaxOn = std::max(MaxOn, std::abs(On8 - Off8));
						// (the base: OFF x clean / (shade x cast), the cast the mesh's tint over the detail)
						const double Cast = Me.Tint[P * 3 + C] / std::max(1e-6, static_cast<double>(Fd.Mul[K * 3 + C]));
						const double Want = Off8 * Clean / std::max(1e-6, Me.Shade[P] * Cast);
						MaxBase = std::max(MaxBase, static_cast<int>(std::lround(std::fabs(Base8 - Want))));
					}
					if (0.2126 * Fd.Mul[K * 3] + 0.7152 * Fd.Mul[K * 3 + 1] + 0.0722 * Fd.Mul[K * 3 + 2] < 0.6)
					{
						++Dark;
						DarkOff += Lum(&A.Rgb[P * 3]);
						DarkOn += Lum(&B.Rgb[P * 3]);
					}
				}
				const double MeanOn = SumOn / (3.0 * static_cast<double>(Fd.At.size()));
				std::printf("%s %s face (%zu points of its detail): ON at W 0.6 on the files' dust against OFF, %.2f bytes apart on the mean, %d at most; "
							"the %d it darkened most %.1f OFF, %.1f ON (value, 0 to 255); W 0 the base within %d bytes\n",
					N.c_str(), L == Near ? "near" : L == Mid ? "mid" : "far", Fd.At.size(), MeanOn, MaxOn, Dark, Dark ? DarkOff / Dark : 0.0, Dark ? DarkOn / Dark : 0.0, MaxBase);
				if (MaxBase > 2) Fail("%s level %d: the face's base at W 0 is %d bytes off the OFF colour's", N.c_str(), L, MaxBase);
				if (MeanOn > 4.0) Fail("%s level %d: the face ON at W 0.6 on the files' dust is %.2f bytes from OFF on the mean (4 at most)", N.c_str(), L, MeanOn);
				if (Dark && std::fabs(DarkOn - DarkOff) / Dark > 4.0) Fail("%s level %d: the face's darkest detail reads %.1f ON, %.1f OFF", N.c_str(), L, DarkOn / Dark, DarkOff / Dark);
			}
			// (the eyes: no dirt on them, their clean shadow mended (MendEyes): at W 0.6 on the files' dust, as OFF but for
			// the baked cast)
			const Look& A = Off.C->LookOf(Off.Of[N], Near);
			const Look& B = On.C->LookOf(On.Of[N], Near);
			double EyeSum = 0.0;
			int EyeN = 0;
			for (size_t P = 0; P < static_cast<size_t>(A.Points); ++P)
			{
				const auto Found = S.OwnPaintNames.find(A.Paint[P]);
				if (Found == S.OwnPaintNames.end() || (Found->second != "eye_white" && Found->second != "iris")) continue;
				EyeSum += std::fabs(Lum(&A.Rgb[P * 3]) - Lum(&B.Rgb[P * 3]));
				++EyeN;
			}
			std::printf("%s near eyes (%d points of eye_white and iris): %.2f apart in value, ON at W 0.6 against OFF\n", N.c_str(), EyeN, EyeN ? EyeSum / EyeN : 0.0);
			if (EyeN < 100 || EyeSum / EyeN > 6.0) Fail("%s: the eyes ON are %.2f from OFF in value (%d points)", N.c_str(), EyeN ? EyeSum / EyeN : 0.0, EyeN);
		}
		// (a gore twin holds its file's points one for one, but no record of its own: it takes its file's, as Crowd::AddGore
		// has it do, and its rider's face is then its file's to a byte or two; without, it is not)
		{
			const Soldier& File = *Worn.at("USA");
			auto Twin = std::make_shared<Soldier>(*Worn.at("USA_gore"));
			const bool bTook = Twin->TakeFaceDetail(File);
			const bool bAgain = Twin->TakeFaceDetail(File);
			Files Mended = Worn;
			Mended["USA_gore"] = Twin;
			Setup Tu = Base;
			Tu.F = &Mended;
			Riders Tw = RidersWith(Tu, &Own, { "USA", "USA_gore" }), Un = RidersWith(Su, &Own, { "USA_gore" });
			if (!Tw.C || !Un.C) { Fail("the twin's crowds were not made"); return 1; }
			int Worst = 0, WorstUnmended = 0;
			for (int L = 0; L < LodCount; ++L)
			{
				const Look& Fl = Tw.C->LookOf(Tw.Of["USA"], static_cast<Lod>(L));
				const Look& Tl = Tw.C->LookOf(Tw.Of["USA_gore"], static_cast<Lod>(L));
				const Look& Ul = Un.C->LookOf(Un.Of["USA_gore"], static_cast<Lod>(L));
				for (const int P : File.Detail[L].At)
				{
					for (size_t C = 0; C < 3; ++C)
					{
						const size_t I = static_cast<size_t>(P) * 3 + C;
						Worst = std::max(Worst, std::abs(Tl.Rgb[I] - Fl.Rgb[I]));
						WorstUnmended = std::max(WorstUnmended, std::abs(Ul.Rgb[I] - Fl.Rgb[I]));
					}
				}
			}
			std::printf("the gore twin: its file's face detail taken (%s; not twice: %s), its face at most %d bytes from its file's ON (%d without it)\n", bTook ? "yes" : "no",
				bAgain ? "taken again" : "no", Worst, WorstUnmended);
			if (Tw.C->Info(Tw.Of["USA"]).Face != Tw.C->Info(Tw.Of["USA_gore"]).Face) Fail("the twin's rider has not his file's rider's face");
			if (!bTook || bAgain || Worst > 2 || WorstUnmended < 20) Fail("the gore twin does not take its file's face detail as it should");
		}
	}

	// ---- Roughness: the table where nothing is on him, 1 to 253 whatever the weather, none unasked ------------------
	{
		const std::vector<std::string> Names = { "USA", "GER", "USA_gore", "USA_tanker", "USA_nco", "USA_bazooka" };
		const Crowd::WearMix Bare = MixOf(0.0, 0.0, 0.0, true), Storm = MixOf(1.0, 1.0, 1.0, true), NoRough = MixOf(1.0, 1.0, 1.0, false);
		Setup Su = Base;
		Su.F = &Worn;
		Riders Rb = RidersWith(Su, &Bare, Names), Rs = RidersWith(Su, &Storm, Names), Rn = RidersWith(Su, &NoRough, Names);
		if (!Rb.C || !Rs.C || !Rn.C) { Fail("the roughness crowds were not made"); return 1; }
		std::map<std::string, std::pair<double, int>> Seen; // (each named own paint's byte read back, and its points)
		int Checked = 0, Off = 0, OutOfSpan = 0;
		for (const std::string& N : Names)
		{
			const Soldier& S = *Worn.at(N);
			for (int L = 0; L < LodCount; ++L)
			{
				const Look& B = Rb.C->LookOf(Rb.Of[N], static_cast<Lod>(L));
				const Look& St = Rs.C->LookOf(Rs.Of[N], static_cast<Lod>(L));
				if (!Rn.C->LookOf(Rn.Of[N], static_cast<Lod>(L)).Rough.empty()) Fail("%s: roughness written without bRough", N.c_str());
				if (B.Rough.size() != static_cast<size_t>(B.Points) || St.Rough.size() != static_cast<size_t>(St.Points)) { Fail("%s: the roughness is not whole", N.c_str()); continue; }
				for (size_t P = 0; P < static_cast<size_t>(B.Points); ++P)
				{
					const int Pa = B.Paint[P];
					const double Want = PartRough(S, Pa), Got = B.Rough[P] / 255.0;
					++Checked;
					if (std::fabs(Got - Want) > 0.01) ++Off;
					OutOfSpan += B.Rough[P] < 1 || B.Rough[P] > 253 || St.Rough[P] < 1 || St.Rough[P] > 253 ? 1 : 0;
					if (Pa >= 15 && S.OwnPaintNames.count(Pa))
					{
						auto& Sn = Seen[S.OwnPaintNames.at(Pa)];
						Sn.first = Got;
						++Sn.second;
					}
				}
			}
		}
		// (the army's paints on a man with a helmet's finish, whose finish's points come after his body's)
		{
			Setup Fu = Base;
			Fu.F = &Worn;
			Fu.Rec = Recording("USA", "GER", 3, 0.0, 0.0);
			Fu.bSetMix = true;
			Fu.Mix = Bare;
			auto C = MakeCrowd(Fu);
			if (!C) { Fail("the finishes' crowd was not made"); return 1; }
			int Finished = 0;
			for (int M = 0; M < C->Count(); ++M)
			{
				if (C->Info(M).Finish < 0) continue;
				++Finished;
				const Soldier& S = *C->SoldierOf(M);
				const Look& B = C->LookOf(M, Near);
				for (size_t P = 0; P < static_cast<size_t>(B.Points); ++P)
				{
					++Checked;
					if (std::fabs(B.Rough[P] / 255.0 - PartRough(S, B.Paint[P])) > 0.01) ++Off;
				}
			}
			if (Finished == 0) Fail("no man of the finishes' crowd wears a finish");
		}
		std::printf("roughness: %d points read back, %d off their part's table by more than 0.01, %d outside 1 to 253 (W 0 dry, and W 1 in rain and snow)\n", Checked, Off, OutOfSpan);
		for (const auto& KV : Seen) std::printf("  %s: %.3f (%d points)\n", KV.first.c_str(), KV.second.first, KV.second.second);
		if (Off) Fail("%d points' roughness is not their part's", Off);
		if (OutOfSpan) Fail("%d points' roughness byte is outside 1 to 253", OutOfSpan);
		for (const char* Want : { "eye_white", "iris", "wound", "bone", "blood", "lens", "rank", "knit", "jacket" })
			if (!Seen.count(Want)) Fail("no point of the own paint %s was read", Want);
		// (every rule of the own paints' names, on a small file of the test's own: a triangle a name, every point as dirty,
		// wet and snowed on as can be; at W 0 dry its part's roughness, and the gore ones the same in a storm)
		{
			const char* Named[] = { "skin_dark", "eye_lash", "iris", "blood", "wound", "bone", "leather_strap", "boot_lace", "belt", "helmet_net", "steel", "metal", "brass", "badge",
				"buckle", "rocket", "tin_cup", "glass", "lens", "kapok" };
			constexpr int Count = static_cast<int>(sizeof(Named) / sizeof(Named[0]));
			std::ostringstream T;
			T << "{\"name\": \"TST\", \"bones\": [{\"name\": \"root\", \"head\": [0, 0, 0]}], \"new_paints\": {";
			for (int K = 0; K < Count; ++K) T << (K ? ", " : "") << "\"" << Named[K] << "\": " << (20 + K);
			T << "}, \"near\": {";
			auto List = [&T](const char* Key, auto Each, int Per) {
				T << "\"" << Key << "\": [";
				for (int P = 0; P < Count * 3 * Per; ++P) T << (P ? ", " : "") << Each(P);
				T << "], ";
			};
			List("points", [](int P) { return 0.1 * (P % 7); }, 3);
			List("facings", [](int P) { return P % 3 == 2 ? 1 : 0; }, 3);
			List("paints", [](int P) { return 20 + P / 3; }, 1);
			List("part", [](int) { return 0; }, 1);
			List("part2", [](int) { return 0; }, 1);
			List("w2", [](int) { return 0; }, 1);
			List("shade", [](int) { return 0.8; }, 1);
			List("shade_clean", [](int) { return 0.8; }, 1);
			for (const char* Key : { "dirt", "dirt_wet", "wet", "snow" }) List(Key, [](int) { return 1; }, 1);
			List("triangles", [](int P) { return P; }, 1);
			T << "\"tint\": []}}";
			auto Tst = std::make_shared<Soldier>();
			std::string Err;
			if (!Tst->Load(T.str(), Err, true)) Fail("the test's own file: %s", Err.c_str());
			else
			{
				Files Own = Worn;
				Own["TST"] = Tst;
				Setup Tu = Base;
				Tu.F = &Own;
				Riders Bare2 = RidersWith(Tu, &Bare, { "TST" }), Storm2 = RidersWith(Tu, &Storm, { "TST" });
				const Look& A = Bare2.C->LookOf(Bare2.Of["TST"], Near);
				const Look& B = Storm2.C->LookOf(Storm2.Of["TST"], Near);
				int Wrong = 0, GoreMoved = 0, CleanStill = 0;
				for (size_t P = 0; P < static_cast<size_t>(A.Points); ++P)
				{
					const int Pa = A.Paint[P];
					if (A.Rough.size() != static_cast<size_t>(A.Points) || std::fabs(A.Rough[P] / 255.0 - PartRough(*Tst, Pa)) > 0.01) ++Wrong;
					const bool bSame = A.Rgb[P * 3] == B.Rgb[P * 3] && A.Rgb[P * 3 + 1] == B.Rgb[P * 3 + 1] && A.Rgb[P * 3 + 2] == B.Rgb[P * 3 + 2] && A.Rough[P] == B.Rough[P];
					if (IsGore(*Tst, Pa)) GoreMoved += bSame ? 0 : 1;
					else CleanStill += bSame ? 1 : 0;
				}
				std::printf("  the test's own file: %d own paints by name, %d points off their part's roughness; the gore ones changed by a storm at %d points, the others left at %d\n", Count, Wrong,
					GoreMoved, CleanStill);
				if (A.Points != Count * 3 || Wrong || GoreMoved || CleanStill) Fail("the own paints' names are not read as the table says");
			}
		}
		// (skin, read straight: 0.55 within 0.01)
		const Look& B = Rb.C->LookOf(Rb.Of["USA"], Near);
		double SkinSum = 0.0;
		int SkinN = 0;
		for (size_t P = 0; P < static_cast<size_t>(B.Points); ++P)
			if (B.Paint[P] == 7) { SkinSum += B.Rough[P] / 255.0; ++SkinN; }
		std::printf("  skin: %.3f (%d points)\n", SkinN ? SkinSum / SkinN : 0.0, SkinN);
		if (!SkinN || std::fabs(SkinSum / SkinN - 0.55) > 0.01) Fail("skin's roughness is not 0.55");
	}

	// ---- Rain darkens the cloth and splashes the mud higher; snow whitens; the gore paints take none ---------------
	{
		const std::vector<std::string> Names = { "USA", "GER", "USA_gore" };
		Setup Su = Base;
		Su.F = &Worn;
		const Crowd::WearMix DryBare = MixOf(0.0, 0.0, 0.0), WetBare = MixOf(0.0, 1.0, 0.0), Dry = MixOf(1.0, 0.0, 0.0), Wet = MixOf(1.0, 1.0, 0.0), Snowed = MixOf(0.0, 0.0, 1.0),
			Storm = MixOf(1.0, 1.0, 1.0);
		Riders Db = RidersWith(Su, &DryBare, Names), Wb = RidersWith(Su, &WetBare, Names), D = RidersWith(Su, &Dry, Names), Wt = RidersWith(Su, &Wet, Names),
			Sn = RidersWith(Su, &Snowed, Names), Sm = RidersWith(Su, &Storm, Names);
		if (!Db.C || !Wb.C || !D.C || !Wt.C || !Sn.C || !Sm.C) { Fail("the weather crowds were not made"); return 1; }
		for (const std::string& N : { std::string("USA"), std::string("GER") })
		{
			const Soldier& S = *Worn.at(N);
			const Mesh& Me = S.Meshes[Near];
			const Look& Lb = Db.C->LookOf(Db.Of[N], Near);
			const Look& Lw = Wb.C->LookOf(Wb.Of[N], Near);
			const Look& Ld = D.C->LookOf(D.Of[N], Near);
			const Look& Lt = Wt.C->LookOf(Wt.Of[N], Near);
			const Look& Ls = Sn.C->LookOf(Sn.Of[N], Near);
			// (the cloth soaked: its value, with nothing on it, dry and wet)
			double DryV = 0.0, WetV = 0.0;
			int Cloth = 0;
			for (size_t P = 0; P < static_cast<size_t>(Me.Points); ++P)
			{
				if (Me.Paint[P] != 0 && Me.Paint[P] != 1) continue;
				DryV += Value(RgbAt(Lb, P));
				WetV += Value(RgbAt(Lw, P));
				++Cloth;
			}
			// (the mud: points over a fifth of the way to their soil at W 1, dry and in rain, and those over the knee)
			int OverDry = 0, OverWet = 0, HighDry = 0, HighWet = 0;
			for (size_t P = 0; P < static_cast<size_t>(Me.Points); ++P)
			{
				const double TD = Toward(RgbAt(Lb, P), RgbAt(Ld, P), SoilAt(Me, P, 0.0));
				const double TW = Toward(RgbAt(Lw, P), RgbAt(Lt, P), SoilAt(Me, P, 1.0));
				const bool bHigh = Me.At[P * 3 + 2] > 0.62f;
				if (TD > 0.2) { ++OverDry; HighDry += bHigh ? 1 : 0; }
				if (TW > 0.2) { ++OverWet; HighWet += bHigh ? 1 : 0; }
			}
			std::printf("%s rain: the cloth's value %.3f dry, %.3f soaked (%d points); over a fifth of the way to the soil at W 1: %d points dry (%d over the knee), %d in rain (%d)\n", N.c_str(),
				DryV / std::max(1, Cloth), WetV / std::max(1, Cloth), Cloth, OverDry, HighDry, OverWet, HighWet);
			if (!(WetV < DryV * 0.85)) Fail("%s: rain does not darken the cloth (%.3f to %.3f)", N.c_str(), DryV / std::max(1, Cloth), WetV / std::max(1, Cloth));
			if (!(OverWet > OverDry) || !(HighWet > HighDry)) Fail("%s: rain does not splash the mud higher (%d to %d points, %d to %d over the knee)", N.c_str(), OverDry, OverWet, HighDry, HighWet);
			// (snow: the shoulders, tunic or straps on a clavicle or an upper arm facing up; his own helmet's crown)
			const std::set<int> Shoulder = { S.BoneIndex("clavicle_l"), S.BoneIndex("clavicle_r"), S.BoneIndex("upperarm_l"), S.BoneIndex("upperarm_r") };
			double ShB = 0.0, ShS = 0.0, CrB = 0.0, CrS = 0.0;
			int ShN = 0, CrN = 0;
			for (size_t P = 0; P < static_cast<size_t>(Me.Points); ++P)
			{
				const bool bUp = Me.Facing[P * 3 + 2] > 0.6f;
				if (bUp && Shoulder.count(Me.Part[P]) && (Me.Paint[P] == 0 || Me.Paint[P] == 4 || Me.Paint[P] == 10))
				{
					ShB += Value(RgbAt(Lb, P));
					ShS += Value(RgbAt(Ls, P));
					++ShN;
				}
				if (Me.Paint[P] == 5 && Me.Facing[P * 3 + 2] > 0.7f)
				{
					CrB += Value(RgbAt(Lb, P));
					CrS += Value(RgbAt(Ls, P));
					++CrN;
				}
			}
			std::printf("%s snow: the shoulders' value %.3f to %.3f (%d points), his helmet's crown %.3f to %.3f (%d points)\n", N.c_str(), ShB / std::max(1, ShN), ShS / std::max(1, ShN), ShN,
				CrB / std::max(1, CrN), CrS / std::max(1, CrN), CrN);
			if (ShN < 10 || CrN < 10) Fail("%s: too few shoulder (%d) or crown (%d) points", N.c_str(), ShN, CrN);
			else if (!(ShS / ShN > ShB / ShN + 0.10) || !(CrS / CrN > CrB / CrN + 0.15)) Fail("%s: snow does not whiten his shoulders and helmet's crown", N.c_str());
		}
		// (a finish's crown, on a man of a small battle: its points after his body's)
		{
			double Was = 0.0, Now = 0.0;
			int N = 0;
			for (const Crowd::WearMix* Mx : { &DryBare, &Snowed })
			{
				Setup Fu = Base;
				Fu.F = &Worn;
				Fu.Rec = Recording("USA", "GER", 3, 0.0, 0.0);
				Fu.bSetMix = true;
				Fu.Mix = *Mx;
				auto C = MakeCrowd(Fu);
				if (!C) { Fail("the finishes' crowd was not made"); return 1; }
				for (int M = 0; M < C->Count(); ++M)
				{
					if (C->Info(M).Finish < 0) continue;
					const Soldier& S = *C->SoldierOf(M);
					const Mesh& Fm = S.Finishes[static_cast<size_t>(C->Info(M).Finish)].Lods[Near];
					const Look& Lk = C->LookOf(M, Near);
					const size_t Body = static_cast<size_t>(Lk.Points - Fm.Points);
					for (size_t P = 0; P < static_cast<size_t>(Fm.Points); ++P)
					{
						if (Fm.Facing[P * 3 + 2] <= 0.7f) continue;
						(Mx == &DryBare ? Was : Now) += Value(RgbAt(Lk, Body + P));
						N += Mx == &DryBare ? 1 : 0;
					}
				}
			}
			std::printf("finishes' crowns under snow: value %.3f to %.3f (%d points)\n", Was / std::max(1, N), Now / std::max(1, N), N);
			if (N < 10 || !(Now / N > Was / N + 0.15)) Fail("snow does not whiten the finishes' crowns");
		}
		// (the gore paints: no dirt, wet or snow; the same colour and roughness bare and in a storm at W 1)
		{
			const Soldier& S = *Worn.at("USA_gore");
			int Gore = 0, Moved = 0;
			for (int L = 0; L < LodCount; ++L)
			{
				const Look& A = Db.C->LookOf(Db.Of["USA_gore"], static_cast<Lod>(L));
				const Look& B = Sm.C->LookOf(Sm.Of["USA_gore"], static_cast<Lod>(L));
				for (size_t P = 0; P < static_cast<size_t>(A.Points); ++P)
				{
					if (!IsGore(S, A.Paint[P])) continue;
					++Gore;
					Moved += A.Rgb[P * 3] != B.Rgb[P * 3] || A.Rgb[P * 3 + 1] != B.Rgb[P * 3 + 1] || A.Rgb[P * 3 + 2] != B.Rgb[P * 3 + 2] ? 1 : 0;
				}
			}
			std::printf("gore: %d points of wound, bone and blood, %d changed by W 1 in rain and snow\n", Gore, Moved);
			if (Gore < 50 || Moved) Fail("the gore paints are not left alone (%d points, %d changed)", Gore, Moved);
		}
	}

	// ---- Sets: men whose wear differs never share one; a rider dressed as a man is his look; a mix set again --------
	{
		const auto T = std::chrono::steady_clock::now();
		// (two blocks of one army, a half and nine tenths of a month in the line: one faded row, AgedRow's step 1, but
		// W 0.4 and 0.65)
		Setup Su = Base;
		Su.F = &Worn;
		Su.Rec = Recording("USA", "USA", 6, 0.5, 0.9);
		Su.bWorn = true;
		Su.bBank = true;
		auto Off = MakeCrowd(Su);
		Su.bSetMix = true;
		Su.Mix = MixOf(-1.0, 0.0, 0.0, true);
		auto On = MakeCrowd(Su);
		if (!Off || !On) { Fail("the sets' crowds were not made"); return 1; }
		int Pairs = 0, Shared = 0, WouldShare = 0;
		std::set<float> Ws;
		for (int A = 0; A < On->Count(); ++A)
		{
			Ws.insert(On->Info(A).Wear);
			for (int B = A + 1; B < On->Count(); ++B)
			{
				if (On->Info(A).Wear == On->Info(B).Wear) continue;
				++Pairs;
				if (On->BodyKey(A) == On->BodyKey(B) || (On->BankKey(A) >= 0 && On->BankKey(A) == On->BankKey(B))) ++Shared;
				if (Off->BodyKey(A) == Off->BodyKey(B)) ++WouldShare;
			}
		}
		std::printf("sets: wears");
		for (const float W : Ws) std::printf(" %.2f", static_cast<double>(W));
		std::printf("; %d pairs of men whose wear differs, %d of them in one set with it on, %d with it off; %d body sets on, %d off; %d banked sets on, %d off\n", Pairs, Shared, WouldShare,
			On->BodyKeys(), Off->BodyKeys(), On->BankKeys(), Off->BankKeys());
		if (Ws.size() != 2 || !Ws.count(0.4f) || !Ws.count(0.65f)) Fail("the wears are not 0.4 and 0.65");
		if (Shared) Fail("%d pairs of men whose wear differs share a set", Shared);
		if (!WouldShare) Fail("no pair of men whose wear differs would have shared a set with it off: the check proves nothing");
		// (and their sets' looks differ)
		{
			std::map<float, int> KeyOf;
			for (int M = 0; M < On->Count(); ++M) KeyOf[On->Info(M).Wear] = On->BodyKey(M);
			if (KeyOf.size() == 2)
			{
				const Look& A = On->BankBodyLook(KeyOf.begin()->second, Far);
				const Look& B = On->BankBodyLook(KeyOf.rbegin()->second, Far);
				if (A.Rgb == B.Rgb) Fail("two body sets of different wear have one look");
			}
		}
		// (every set's look carries its roughness whole, 1 to 253)
		{
			int Sets = 0, Bad = 0;
			auto Check = [&Sets, &Bad](const Look& Lk) {
				if (Lk.Points == 0) return;
				++Sets;
				bool bOk = Lk.Rough.size() == static_cast<size_t>(Lk.Points);
				for (const uint8_t R : Lk.Rough) bOk = bOk && R >= 1 && R <= 253;
				Bad += bOk ? 0 : 1;
			};
			for (int K = 0; K < On->BankKeys(); ++K) Check(On->BankLook(K));
			for (int K = 0; K < On->BodyKeys(); ++K)
				for (const Lod L : { Mid, Far })
				{
					Check(On->BankBodyLook(K, L));
					for (int Fi = 0; Fi < static_cast<int>(Worn.at("USA")->Finishes.size()); ++Fi) Check(On->HelmetLook(K, Fi, L));
				}
			std::printf("sets: %d looks of the banked and body sets and their helmets, %d without their roughness whole\n", Sets, Bad);
			if (Bad || !Sets) Fail("%d of the sets' %d looks lack their roughness", Bad, Sets);
		}
		// (a rider dressed as a man: his wear and his look, to the byte)
		for (int M = 0; M < 3; ++M)
		{
			const int R = On->AddRiderLike(M);
			if (R < 0 || On->Info(R).Wear != On->Info(M).Wear) { Fail("the rider dressed as man %d has not his wear", M); continue; }
			for (int L = 0; L < LodCount; ++L)
				if (!SameLook(On->LookOf(R, static_cast<Lod>(L)), On->LookOf(M, static_cast<Lod>(L)))) Fail("the rider dressed as man %d has not his look at level %d", M, L);
		}
		// (the mix set again: the same, nothing made again; off, every look as with it never set; on again, as before)
		std::vector<uint32_t> Was;
		for (int M = 0; M < On->Count(); ++M) Was.push_back(On->MeshNo(M));
		On->SetWearMix(Su.Mix);
		bool bKept = true;
		for (int M = 0; M < On->Count(); ++M) bKept = bKept && On->MeshNo(M) == Was[static_cast<size_t>(M)];
		if (!bKept) Fail("the same mix set again made the looks again");
		int RoughOn = 0, RoughOff = 0, RoughNever = 0;
		const std::vector<uint8_t> OnBefore = LooksOf(*On, RoughOn);
		Crowd::WearMix NoMix;
		On->SetWearMix(NoMix);
		bool bMoved = true;
		for (int M = 0; M < On->Count(); ++M) bMoved = bMoved && On->MeshNo(M) != Was[static_cast<size_t>(M)];
		if (!bMoved) Fail("a man's MeshNo did not move as the mix changed");
		// (the crowd never mixed, with the same riders dressed alike)
		for (int M = 0; M < 3; ++M) Off->AddRiderLike(M);
		const std::vector<uint8_t> OffAfter = LooksOf(*On, RoughOff), Never = LooksOf(*Off, RoughNever);
		if (OffAfter != Never || RoughOff) Fail("set off after the looks were made, they are not those of a crowd never mixed (%zu and %zu bytes, %d with roughness)", OffAfter.size(), Never.size(), RoughOff);
		On->SetWearMix(Su.Mix);
		int RoughAgain = 0;
		if (LooksOf(*On, RoughAgain) != OnBefore || RoughAgain != RoughOn) Fail("set on again, the looks are not those first made");
		std::printf("sets: a rider dressed as a man is his look; the mix set again makes the looks again, off as never mixed, on again as first made (%d looks with roughness) (%.0f ms)\n", RoughOn,
			std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T).count());
	}

	std::printf("land worn men %s (%d failures)\n", Failures ? "FAILED" : "ok", Failures);
	return Failures ? 1 : 0;
#endif
}
