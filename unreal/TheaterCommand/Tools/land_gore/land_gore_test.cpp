// The gore falls' cloud test (Effects, release 2, behind -TheaterLandGore; Battle/LandMen's Crowd::AddGore and SetGore).
// Plays a small battle of its own through the crowd: 160 men, an American and a German line, killed at 3 s by a field
// gun's shell, a mortar's bomb or a rifle, each burst 0 to 13 m from the man it kills. Checks:
//   off   twins added but the switch off: every man falls exactly as with no twins (his fall, his skin, byte for byte)
//   on    only shell kills near their burst are gored, the share falling away with the distance; each gored man takes
//         one of his twin's gore falls, keeps his clips, a limb leaves its joint, his limb_off events come; no whole
//         joint of his opens on its caps (the twin's joined skin); no man's skin holds a NaN. Notes: a limb resting
//         under 1 m from its joint, or flying over 3 m high (the art director's asks: the clips' own to mend)
//   same  a second crowd gores the same men with the same falls
//   back  a replay from the start gives every man his own file again; played on, the same men are gored again
// With an out folder it also paints gore_falls.ppm: six gored men from the side, each from the hit to rest in six steps.
// Usage: land_gore_test <soldiers folder> <twins folder> <armies.json> [out folder]
#define main land_men_picture_main
#include "land_men_picture.cpp"
#undef main

#include <chrono>
#include <cstring>
#include <set>
#include <tuple>

namespace
{
	const char* const Cuts[] = { "upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r", "thigh_l", "thigh_r", "calf_l", "calf_r" };
	constexpr int MenEach = 80;
	constexpr double Hit = 3.0;

	struct Who
	{
		int B = 1, K = 1;
		std::string W;
		double Burst = 0.0;
	};

	std::string Recording(std::vector<Who>& Men)
	{
		std::ostringstream R, Rounds;
		R << "{\"seed\": 23, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"}],"
		  << "\"men\": [";
		Rounds << "\"rounds\": [";
		for (int I = 0; I < 2 * MenEach; ++I)
		{
			Who Wh;
			Wh.B = I % 2 ? 2 : 1;
			Wh.K = I / 2 + 1;
			const int Kind = (I / 2) % 8;
			Wh.W = Kind < 6 ? "gun" : Kind == 6 ? "mortar" : "rifle";
			Wh.Burst = Wh.W == "rifle" ? 0.0 : static_cast<double>((I / 16) % 14);
			const double X = 30.0 * (I / 2), Y = Wh.B == 1 ? 0.0 : 60.0, F = Wh.B == 1 ? 0.0 : 3.14159265358979;
			const char* Stance = (I / 2) % 5 == 4 ? "wait" : "fire_stand";
			if (I) { R << ","; Rounds << ","; }
			R << "{\"b\": " << Wh.B << ", \"k\": " << Wh.K << ", \"r\": \"rifle\", \"at\": [[0, " << X << ", " << Y << ", " << F << "], [10, " << X << ", " << Y << ", " << F
			  << "]], \"ch\": [[0, \"" << Stance << "\", \"none\", 0, 0, 0, 0]], \"d\": " << Hit << "}";
			// (the burst that far from him, off to his side: x, y the round's, as the drill writes a shell's)
			const double A = 0.7 * I;
			Rounds << "{\"t\": " << Hit << ", \"w\": \"" << Wh.W << "\", \"on\": " << Wh.B << ", \"ok\": " << Wh.K << ", \"x\": " << X + Wh.Burst * std::cos(A)
			       << ", \"y\": " << Y + Wh.Burst * std::sin(A) << ", \"r\": \"kill\"}";
			Men.push_back(Wh);
		}
		R << "], " << Rounds.str() << "]}";
		return R.str();
	}

	uint64_t HashOf(const std::vector<float>& V, uint64_t H)
	{
		for (const float F : V)
		{
			uint32_t U = 0;
			std::memcpy(&U, &F, sizeof(U));
			H = (H ^ U) * 1099511628211ull;
		}
		return H;
	}

	/** The widest any limb stands off its joint (metres): the cut bone's head against where its parent holds it. */
	double LimbGap(const Crowd& C, int Man)
	{
		const Soldier& S = *C.SoldierOf(Man);
		const Xform* Fr = C.FramesOf(Man);
		double Most = 0.0;
		for (const char* Name : Cuts)
		{
			const int B = S.BoneIndex(Name);
			if (B < 0) continue;
			const int P = S.Bones[static_cast<size_t>(B)].Parent;
			if (P < 0) continue;
			const Vec3 Head = S.Bones[static_cast<size_t>(B)].Head;
			const Vec3 A = Fr[B].Point(Head), Q = Fr[P].Point(Head);
			Most = std::max(Most, std::sqrt(static_cast<double>((A.X - Q.X) * (A.X - Q.X) + (A.Y - Q.Y) * (A.Y - Q.Y) + (A.Z - Q.Z) * (A.Z - Q.Z))));
		}
		return Most;
	}

	/** The highest any limb off its joint stands above the (flat) ground now: every bone below a cut bone that has left
	 * its joint by more than 0.1 m, at its head. 0 where none has. */
	double LimbHigh(const Crowd& C, int Man)
	{
		const Soldier& S = *C.SoldierOf(Man);
		const Xform* Fr = C.FramesOf(Man);
		std::vector<char> Off(S.Bones.size(), 0);
		for (const char* Name : Cuts)
		{
			const int B = S.BoneIndex(Name);
			if (B < 0 || S.Bones[static_cast<size_t>(B)].Parent < 0) continue;
			const Vec3 Head = S.Bones[static_cast<size_t>(B)].Head;
			const Vec3 A = Fr[B].Point(Head), Q = Fr[S.Bones[static_cast<size_t>(B)].Parent].Point(Head);
			if (std::sqrt(static_cast<double>((A.X - Q.X) * (A.X - Q.X) + (A.Y - Q.Y) * (A.Y - Q.Y) + (A.Z - Q.Z) * (A.Z - Q.Z))) > 0.1) Off[static_cast<size_t>(B)] = 1;
		}
		double Most = 0.0;
		for (size_t B = 0; B < S.Bones.size(); ++B)
		{
			bool bLimb = false;
			for (int P = static_cast<int>(B); P >= 0 && !bLimb; P = S.Bones[static_cast<size_t>(P)].Parent) bLimb = Off[static_cast<size_t>(P)] != 0;
			if (bLimb) Most = std::max(Most, static_cast<double>(Fr[B].Point(S.Bones[B].Head).Z));
		}
		return Most;
	}

	/**
	 * The widest a whole joint of a gored man stands open (metres; the art director, 18:34Z: no cap seen till his limb
	 * goes): his twin's seam copies (its near points after his own file's, before the caps) against the points they
	 * copy, at the same rest place, skinned; only at cuts still whole (a copy and its point skinned alike once joined).
	 * -1 where it cannot be read.
	 */
	double JointOpen(Crowd& C, int Man, const Soldier& Own)
	{
		const Soldier& Tw = *C.SoldierOf(Man);
		const int N = Own.Meshes[Near].Points, All = Tw.Meshes[Near].Points;
		if (Tw.Cuts.empty()) return -1.0;
		// (his mesh as the crowd makes it: under a helmet finish the points only his own helmet uses left out)
		const Mesh& Full = Tw.Meshes[Near];
		std::vector<int> Ix(static_cast<size_t>(All), 0);
		int Kept = 0;
		{
			std::vector<uint8_t> Use(static_cast<size_t>(All), 2);
			if (C.Info(Man).Finish >= 0 && Tw.HelmetCount[Near] > 0)
			{
				std::fill(Use.begin(), Use.end(), 0);
				const size_t F = static_cast<size_t>(Tw.HelmetFirst[Near]), Fc = static_cast<size_t>(Tw.HelmetCount[Near]);
				for (size_t T = 0; T < Full.Triangles.size() / 3; ++T)
					for (int K = 0; K < 3; ++K) Use[Full.Triangles[T * 3 + static_cast<size_t>(K)]] |= (T >= F && T < F + Fc) ? 1 : 2;
			}
			for (size_t P = 0; P < Use.size(); ++P) Ix[P] = Use[P] != 1 ? Kept++ : -1;
		}
		if (C.PointsOf(Man, Near) < Kept) return -1.0;
		int Caps = All;
		for (const Soldier::Cut& K : Tw.Cuts) Caps = std::min({ Caps, K.Stump[Near][0], K.Limb[Near][0] });
		// (several of his own points can share a rest place, each side of a hard edge or a paint's seam, and he weights them
		// a little apart; a copy is read against the one it copies, the nearest of them as skinned, so his own seams are not
		// counted as the twin's)
		std::multimap<std::tuple<float, float, float>, int> Orig;
		const std::vector<float>& R = Tw.Meshes[Near].At;
		for (int P = 0; P < N; ++P) Orig.emplace(std::make_tuple(R[static_cast<size_t>(P) * 3], R[static_cast<size_t>(P) * 3 + 1], R[static_cast<size_t>(P) * 3 + 2]), P);
		std::vector<float> At, Fa;
		Skinned(C, Man, At, Fa);
		// (the cuts whose bone has left its joint, as the crowd reads them, and the bones that went with each)
		const Xform* Fr = C.FramesOf(Man);
		uint32_t Off = 0;
		std::vector<char> Gone(Tw.Bones.size(), 0);
		for (size_t K = 0; K < Tw.Cuts.size(); ++K)
		{
			const int B = Tw.Cuts[K].BoneIx;
			const Vec3 Head = Tw.Bones[static_cast<size_t>(B)].Head;
			const Vec3 A = Fr[B].Point(Head), Q = Fr[Tw.Bones[static_cast<size_t>(B)].Parent].Point(Head);
			if ((A.X - Q.X) * (A.X - Q.X) + (A.Y - Q.Y) * (A.Y - Q.Y) + (A.Z - Q.Z) * (A.Z - Q.Z) <= 1e-4f) continue;
			Off |= 1u << K;
			for (size_t Bn = 0; Bn < Tw.Bones.size(); ++Bn)
				for (int P = static_cast<int>(Bn); P >= 0; P = Tw.Bones[static_cast<size_t>(P)].Parent) if (P == B) Gone[Bn] = 1;
		}
		std::map<int, uint16_t> Masks;
		const Soldier::Joined& Jn = Tw.Joins[Near];
		for (size_t K = 0; K < Jn.Point.size(); ++K) Masks[static_cast<int>(Jn.Point[K])] = Jn.Mask[K];
		// (a point at a cut that is off, or riding a limb that went: not a whole joint)
		auto AtOff = [&](int P) {
			const auto Mk = Masks.find(P);
			if (Mk != Masks.end()) return (Mk->second & Off) != 0u;
			return Gone[Full.Part[static_cast<size_t>(P)]] != 0 || Gone[Full.Part2[static_cast<size_t>(P)]] != 0;
		};
		double Most = 0.0;
		for (int P = N; P < Caps; ++P)
		{
			const size_t O = static_cast<size_t>(P) * 3;
			if (AtOff(P) || Ix[static_cast<size_t>(P)] < 0) continue;
			const size_t A = static_cast<size_t>(Ix[static_cast<size_t>(P)]) * 3;
			double Near_ = -1.0;
			const auto Span = Orig.equal_range(std::make_tuple(R[O], R[O + 1], R[O + 2]));
			for (auto Found = Span.first; Found != Span.second; ++Found)
			{
				if (AtOff(Found->second) || Ix[static_cast<size_t>(Found->second)] < 0) continue;
				const size_t Q = static_cast<size_t>(Ix[static_cast<size_t>(Found->second)]) * 3;
				const double G = std::sqrt(static_cast<double>((At[A] - At[Q]) * (At[A] - At[Q]) + (At[A + 1] - At[Q + 1]) * (At[A + 1] - At[Q + 1]) + (At[A + 2] - At[Q + 2]) * (At[A + 2] - At[Q + 2])));
				Near_ = Near_ < 0.0 ? G : std::min(Near_, G);
			}
			if (Near_ >= 0.0) Most = std::max(Most, Near_);
		}
		return Most;
	}

	struct Result
	{
		std::vector<std::string> Clip;
		std::vector<int> Gored;
		std::vector<double> Gap, High;
		uint64_t Skin = 1469598103934665603ull;
		int LimbOff = 0, NaN = 0;
		double Ms = 0.0;
	};

	Result Play(Crowd& C, const std::vector<Who>& Men, double From, double To)
	{
		Result Out;
		const auto T0 = std::chrono::steady_clock::now();
		double Measured = 0.0;
		Out.High.assign(Men.size(), 0.0);
		for (double T = From; T <= To; T += 1.0 / 60.0)
		{
			C.Update(T);
			for (const Event& E : C.Events()) Out.LimbOff += E.Fx == "limb_off" ? 1 : 0;
			// (the limbs' arcs, every frame: the art director's 3 m cap, 18:34Z; not in the time)
			const auto M0 = std::chrono::steady_clock::now();
			for (size_t I = 0; I < Men.size(); ++I) Out.High[I] = std::max(Out.High[I], LimbHigh(C, C.Find(Men[I].B, Men[I].K)));
			Measured += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - M0).count();
		}
		Out.Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count() - Measured;
		for (size_t I = 0; I < Men.size(); ++I)
		{
			const int Man = C.Find(Men[I].B, Men[I].K);
			Out.Clip.push_back(C.Info(Man).FallClip);
			std::vector<float> At, Fa;
			Skinned(C, Man, At, Fa);
			for (const float F : At) Out.NaN += std::isfinite(F) ? 0 : 1;
			Out.Skin = HashOf(At, Out.Skin);
			Out.Gap.push_back(LimbGap(C, Man));
		}
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: land_gore_test <soldiers folder> <twins folder> <armies.json> [out folder]\n");
		return 2;
	}
	std::string Err;
	std::map<std::string, std::shared_ptr<Soldier>> Files;
	std::map<std::string, Soldier> Twins;
	for (const char* Army : { "GER", "POL", "USA" })
	{
		auto S = std::make_shared<Soldier>();
		if (!S->Load(ReadFile(std::string(Argv[1]) + "/soldier_" + Army + ".json"), Err)) { std::printf("soldier %s: %s\n", Army, Err.c_str()); return 1; }
		Files[Army] = S;
		const std::string Text = ReadFile(std::string(Argv[2]) + "/soldier_" + Army + "_gore.json");
		if (Text.empty()) continue;
		Soldier Tw;
		if (!Tw.Load(Text, Err)) { std::printf("twin %s: %s\n", Army, Err.c_str()); return 1; }
		Twins[Army] = std::move(Tw);
	}
	if (Twins.find("USA") == Twins.end() || Twins.find("GER") == Twins.end()) { std::printf("FAIL: no USA or GER twin in %s\n", Argv[2]); return 1; }
	const std::string Armies = ReadFile(Argv[3]);
	std::vector<Who> Men;
	const std::string Rec = Recording(Men);
	const GroundFn Flat = [](double, double) { return Ground(); };
	int Failed = 0;
	auto Check = [&Failed](bool bOk, const std::string& What) { std::printf("%s: %s\n", bOk ? "ok" : "FAIL", What.c_str()); Failed += bOk ? 0 : 1; };
	auto Make = [&](Crowd& C, bool bTwins, bool bOn) {
		for (const auto& KV : Files) C.AddSoldier(KV.first, KV.second);
		if (bTwins)
		{
			for (const auto& KV : Twins)
			{
				std::string Why;
				if (!C.AddGore(KV.first, KV.second, Why)) std::printf("twin %s not used: %s\n", KV.first.c_str(), Why.c_str());
			}
		}
		C.SetGore(bOn);
		if (!C.ReadArmies(Armies, Err) || !C.Build(Rec, Flat, Err)) { std::printf("build: %s\n", Err.c_str()); std::exit(1); }
		C.SetEye(-1000, -1000, 2);
	};
	const double Until = Hit + 5.0;

	// ---- Off: the twins added, the switch off, against no twins at all.
	{
		Crowd A, B;
		Make(A, false, false);
		Make(B, true, false);
		const Result Ra = Play(A, Men, 0.0, Until), Rb = Play(B, Men, 0.0, Until);
		Check(Ra.Clip == Rb.Clip && Ra.Skin == Rb.Skin && B.Gored() == 0 && Rb.LimbOff == 0, "off: every fall and every skin as with no twins, none gored");
		std::printf("     (%.0f ms against %.0f ms for %d frames)\n", Rb.Ms, Ra.Ms, static_cast<int>(Until * 60.0));
	}

	// ---- On.
	Crowd C;
	Make(C, true, true);
	const Result Rc = Play(C, Men, 0.0, Until);
	int Gored = 0, RifleGored = 0, FarGored = 0, BadClip = 0, NoLimb = 0, LimbOnWhole = 0, LostClips = 0, JointsRead = 0;
	double MostOpen = 0.0;
	int ByD[14] = {}, ShellD[14] = {}, WrongSide = 0;
	std::vector<int> GoredMen;
	for (size_t I = 0; I < Men.size(); ++I)
	{
		const int Man = C.Find(Men[I].B, Men[I].K);
		const Soldier* Own = Files[C.Info(Man).FileArmy].get();
		const bool bGored = C.SoldierOf(Man) != Own;
		if (Men[I].W != "rifle") { ++ShellD[static_cast<int>(Men[I].Burst)]; if (bGored) ++ByD[static_cast<int>(Men[I].Burst)]; }
		if (!bGored)
		{
			if (Rc.Gap[I] > 0.05) ++LimbOnWhole;
			continue;
		}
		++Gored;
		GoredMen.push_back(static_cast<int>(I));
		const Soldier& S = *C.SoldierOf(Man);
		if (Men[I].W == "rifle") ++RifleGored;
		const double Reach = Men[I].W == "gun" ? 8.0 : 6.0;
		if (Men[I].Burst > Reach) ++FarGored;
		// (thrown away from the burst: his fall's side its, where it fell 1.5 m or more off; the test's men stand still)
		if (Men[I].Burst >= 1.5)
		{
			const double A = 0.7 * static_cast<double>(I), Face = Men[I].B == 1 ? 0.0 : 3.14159265358979;
			double Off = std::atan2(std::cos(A), std::sin(A)) - Face;
			while (Off > 3.14159265358979) Off -= 6.28318530717959;
			while (Off < -3.14159265358979) Off += 6.28318530717959;
			Off = std::fabs(Off);
			if (C.Info(Man).FallFrom != (Off < 1.22 ? "front" : Off > 1.92 ? "back" : "side")) ++WrongSide;
		}
		bool bIn = false;
		for (const auto& St : S.DeathsGore) for (const auto& Kd : St.second) for (const std::string& N : Kd.second) bIn = bIn || N == Rc.Clip[I];
		if (!bIn) ++BadClip;
		// (off its joint at all; the art director asks 1 m or more, an arm 1 to 4 m, a lower leg 1 to 3 m: a note, the
		// clips' own to mend)
		if (Rc.Gap[I] < 0.1) ++NoLimb;
		if (Rc.Gap[I] < 1.0) std::printf("     note: %s %s's limb rests %.2f m from its joint (the art director asks 1 m or more)\n", C.Info(Man).FileArmy.c_str(), Rc.Clip[I].c_str(), Rc.Gap[I]);
		if (Rc.High[I] > 3.0) std::printf("     note: %s %s's limb flies %.1f m high (the art director asks 3 m at most)\n", C.Info(Man).FileArmy.c_str(), Rc.Clip[I].c_str(), Rc.High[I]);
		const double Open = JointOpen(C, Man, *Own);
		if (Open >= 0.0) { ++JointsRead; MostOpen = std::max(MostOpen, Open); }
		for (const Clip& Cl : Own->Clips) LostClips += S.ClipIndex(Cl.Name) == Own->ClipIndex(Cl.Name) ? 0 : 1;
	}
	std::printf("     gored %d of %d men (%d twins read); by the burst's distance (shell kills):", Gored, static_cast<int>(Men.size()), static_cast<int>(Twins.size()));
	for (int D = 0; D < 14; ++D) std::printf(" %dm %d/%d", D, ByD[D], ShellD[D]);
	std::printf("\n");
	Check(Gored > 0 && Gored == C.Gored(), "on: men gored, and the crowd's count says so");
	Check(RifleGored == 0, "on: no man killed by a rifle gored");
	Check(FarGored == 0, "on: no man past the burst's reach gored");
	Check(ByD[1] + ByD[2] > ByD[4] + ByD[5] && ByD[4] + ByD[5] >= ByD[6] + ByD[7], "on: more gored near the burst than farther off");
	Check(ByD[0] * (ShellD[1] + ShellD[2]) < (ByD[1] + ByD[2]) * ShellD[0], "on: a man the burst was laid on gored less often than one within 2 m");
	Check(WrongSide == 0, "on: each gored man 1.5 m or more from the burst falls by its side (" + std::to_string(WrongSide) + " not)");
	Check(BadClip == 0, "on: each gored man in one of his twin's gore falls");
	Check(LostClips == 0, "on: each gored man keeps every clip number of his own file");
	Check(NoLimb == 0, "on: each gored man has a limb off its joint (more than 0.1 m) at rest");
	Check(LimbOnWhole == 0, "on: no other man's limb leaves its joint");
	Check(Rc.LimbOff >= Gored, "on: the limb_off events come (" + std::to_string(Rc.LimbOff) + ")");
	char OpenWords[160];
	std::snprintf(OpenWords, sizeof(OpenWords), "on: no whole joint of a gored man opens on its caps (widest %.1f mm, %d men read)", MostOpen * 1000.0, JointsRead);
	Check(JointsRead > 0 && MostOpen < 0.002, OpenWords);
	Check(Rc.NaN == 0, "on: no NaN in any skin");
	std::printf("     (%.0f ms for %d frames)\n", Rc.Ms, static_cast<int>(Until * 60.0));

	// ---- Same: another crowd, built alike.
	{
		Crowd D;
		Make(D, true, true);
		const Result Rd = Play(D, Men, 0.0, Until);
		Check(Rd.Clip == Rc.Clip && Rd.Skin == Rc.Skin, "same: a second crowd gores the same men with the same falls");
	}

	// ---- Back: a replay from the start, then on again.
	{
		std::vector<uint32_t> No;
		for (const int I : GoredMen) No.push_back(C.MeshNo(C.Find(Men[static_cast<size_t>(I)].B, Men[static_cast<size_t>(I)].K)));
		C.Update(0.5);
		bool bOwn = C.Gored() == 0;
		for (size_t K = 0; K < GoredMen.size(); ++K)
		{
			const Who& Wh = Men[static_cast<size_t>(GoredMen[K])];
			const int Man = C.Find(Wh.B, Wh.K);
			bOwn = bOwn && C.SoldierOf(Man) == Files[C.Info(Man).FileArmy].get() && C.MeshNo(Man) != No[K] && C.PointsOf(Man, Near) > 0;
		}
		Check(bOwn, "back: a replay from the start gives every gored man his own file and mesh again");
		const Result Re = Play(C, Men, 0.5, Until);
		Check(Re.Clip == Rc.Clip && Re.Skin == Rc.Skin, "back: played on, the same men are gored with the same falls");
	}

	// ---- The picture: six gored men, each in his own fall, from the side.
	if (Argc > 4 && !GoredMen.empty())
	{
		std::vector<int> Pick;
		std::set<std::string> Seen;
		for (const int I : GoredMen) if (Pick.size() < 6 && Seen.insert(Rc.Clip[static_cast<size_t>(I)]).second) Pick.push_back(I);
		// (then each army's own falls: no clip twice for the same army)
		for (const int I : GoredMen)
			if (Pick.size() < 6 && std::find(Pick.begin(), Pick.end(), I) == Pick.end()
				&& Seen.insert(C.Info(C.Find(Men[static_cast<size_t>(I)].B, Men[static_cast<size_t>(I)].K)).FileArmy + "/" + Rc.Clip[static_cast<size_t>(I)]).second)
				Pick.push_back(I);
		for (const int I : GoredMen) if (Pick.size() < 6 && std::find(Pick.begin(), Pick.end(), I) == Pick.end()) Pick.push_back(I);
		Crowd P;
		Make(P, true, true);
		const int Cols = 6, Rows = static_cast<int>(Pick.size()), SheetW = 1600, CellH = 210;
		struct Step { std::vector<float> At, Fa; };
		std::vector<std::vector<Step>> Steps(static_cast<size_t>(Rows), std::vector<Step>(Cols));
		std::vector<std::pair<double, std::pair<int, int>>> Due;
		for (int Row = 0; Row < Rows; ++Row)
		{
			const Who& Wh = Men[static_cast<size_t>(Pick[static_cast<size_t>(Row)])];
			const Soldier& Tw = *C.SoldierOf(C.Find(Wh.B, Wh.K));
			const int Ix = Tw.ClipIndex(Rc.Clip[static_cast<size_t>(Pick[static_cast<size_t>(Row)])]);
			const double Long = Ix >= 0 ? Tw.Clips[static_cast<size_t>(Ix)].Long : 2.0;
			for (int Col = 0; Col < Cols; ++Col) Due.push_back({ Hit + 0.02 + (Col < Cols - 1 ? Long * Col / (Cols - 1) : Long + 0.3), { Row, Col } });
		}
		std::stable_sort(Due.begin(), Due.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		double Now = 0.0;
		for (const auto& Dn : Due)
		{
			while (Now + 1.0 / 60.0 < Dn.first) { Now += 1.0 / 60.0; P.Update(Now); }
			Now = Dn.first;
			P.Update(Now);
			const Who& Wh = Men[static_cast<size_t>(Pick[static_cast<size_t>(Dn.second.first)])];
			Step& St = Steps[static_cast<size_t>(Dn.second.first)][static_cast<size_t>(Dn.second.second)];
			Skinned(P, P.Find(Wh.B, Wh.K), St.At, St.Fa);
		}
		std::vector<uint8_t> Sheet(static_cast<size_t>(SheetW) * static_cast<size_t>(CellH * Rows) * 3, 236);
		Light Lt = Daylight(215.0, 13.0);
		Lt.EarthA = Lin(118, 98, 76);
		Lt.EarthB = Lin(96, 80, 62);
		Lt.HazeK = 0.0;
		for (int Row = 0; Row < Rows; ++Row)
		{
			const Who& Wh = Men[static_cast<size_t>(Pick[static_cast<size_t>(Row)])];
			const int Man = P.Find(Wh.B, Wh.K);
			std::printf("     picture row %d: %s, %s %s, burst %.0f m off, %s (%s)\n", Row, P.Info(Man).FileArmy.c_str(), Wh.W.c_str(), "kill", Wh.Burst,
				P.Info(Man).FallClip.c_str(), P.Info(Man).FallKind.c_str());
			double XLo = 1e9, XHi = -1e9, YLo = 1e9, YHi = -1e9;
			for (const Step& St : Steps[static_cast<size_t>(Row)])
			{
				for (size_t K = 0; K < St.At.size(); K += 3)
				{
					XLo = std::min(XLo, static_cast<double>(St.At[K])); XHi = std::max(XHi, static_cast<double>(St.At[K]));
					YLo = std::min(YLo, static_cast<double>(St.At[K + 1])); YHi = std::max(YHi, static_cast<double>(St.At[K + 1]));
				}
			}
			const double XMid = 0.5 * (XLo + XHi), YMid = 0.5 * (YLo + YHi);
			const double Back = std::max(5.0, (YHi - YLo) * 1.4 + 3.0);
			for (int Col = 0; Col < Cols; ++Col)
			{
				const int OX = (SheetW * Col) / Cols, OX1 = (SheetW * (Col + 1)) / Cols;
				const int CW = OX1 - OX - 4, CH = CellH - 4;
				Scene Sc;
				const Step& St = Steps[static_cast<size_t>(Row)][static_cast<size_t>(Col)];
				Sc.AddMan(P.LookOf(Man, Near), St.At, St.Fa);
				Sc.AddGround(XMid - 10.0, YMid - 10.0, XMid + 10.0, YMid + 12.0, 0.5, Flat);
				const double Span = std::max(XHi - XLo + 0.8, 2.4 * CW / CH);
				const double Fov = 2.0 * std::atan(Span * 0.5 / Back) * 180.0 / 3.14159265358979;
				const Camera Cam({ XMid, YMid - Back, 1.45 }, { XMid, YMid, 0.6 }, Fov, CW * 2, CH * 2);
				ShadowMap Sh;
				Sh.Build(Sc, Lt.ToSun, { XMid - 4.0, YMid - 4.0, -0.1 }, { XMid + 4.0, YMid + 4.0, 2.2 }, 1536);
				const Picture Pic = Paint(Sc, Cam, Lt, Sh);
				Put(Pic, 1.0, Sheet, SheetW, OX + 2, Row * CellH + 2);
			}
		}
		const std::string OutPath = std::string(Argv[4]) + "/gore_falls.ppm";
		if (!WritePpm(OutPath, Sheet, SheetW, CellH * Rows)) { std::printf("cannot write %s\n", OutPath.c_str()); return 1; }
		std::printf("wrote %s (%dx%d)\n", OutPath.c_str(), SheetW, CellH * Rows);
	}
	std::printf(Failed ? "land_gore_test: %d FAILED\n" : "land_gore_test: all passed\n", Failed);
	return Failed ? 1 : 0;
}
