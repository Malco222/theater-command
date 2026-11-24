// The ground-attack fighters' cloud test (Effects, release 2, behind -TheaterLandAirSupport; Battle/LandAir and the
// feed's air notes, LandFeed::AirNote). No recording made yet carries the fighters on call, so it writes one of its own
// in the rules' words (src/battle/land/sim.ts: land:air-in, land:strafe, land:bomb-drop, air:lost), three strikes over a
// flat field: American Mustangs in 1944 (four, one lost on fire), German Stukas in 1942 (four, one lost in a spin) and
// Russian Il-2s in 1943 (two legs of two, one blown up). Checks:
//   feed     every note read, with its flight, run, models, strafe, bombs and loss
//   machine  each air force's machine by year
//   flight   no plane before its leg comes; each flown at its machine's speed, never into the ground, its heading and
//            attitude smooth; a Stuka dives steeply, an Il-2 stays low
//   guns     each plane's guns walk its lane of the strip from start to end, on the ground; the leg rakes the strip for
//            about the rules' time
//   losses   as many planes hit as the rules lost, each hit once and into the ground once after; a burning one in a long
//            dive, a spinning one steep, one blown up gone at once; a wreck where each struck; no other plane falls
//   ground   only the other side's blocks fire, at a plane in reach, their streams ending in the air near it
//   same     built twice, or grown note by note as a live battle grows, the same planes; rounds the same however the
//            frames fall; no NaN; the planes' hash printed against release 1's
//   chutes   the downed pilots (-TheaterLandChutes): out of the right planes, down, up and away; and the art director's
//            notes of 24 November (02:26Z) on them, on this field's pilots and a second, windy field's (5.7 m/s; its
//            Il-2 loses her pilot under 60 m): falling free, filling, the wind, the swap, the harness and the drag, the
//            hard landing and the limp, the swing, the canopy's shape, seams, silk and glow; and a review's of the same
//            day: the scene's mesh made and moved in place at 45 frames a second, the swap's frames, his lying pose
//            against every soldier file's go_prone (LAND_AIR_SOLDIERS, which run.sh sets), the blends, his lines
// With an out folder it writes air_paths.txt for paths.py (the picture: each strike side-on and from above), and the
// pilots' frames and chute_silk.txt (his canopy's mesh, full and fallen in) for chutes.py.
// Usage: Tools/land_air/run.sh [out folder]
#include "Battle/LandAir.h"
#include "Battle/LandJson.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace LandAir;

namespace
{
	int Failed = 0;
	void Check(bool bOk, const std::string& What)
	{
		std::printf("%s: %s\n", bOk ? "ok" : "FAIL", What.c_str());
		Failed += bOk ? 0 : 1;
	}

	struct Strike
	{
		int Flight;
		const char* Side;
		const char* Nat;
		int Year, Models;
		double Run[4];
		double T;
		const char* Lost;   // "" none
	};

	const Strike Strikes[] = {
		{ 1, "att", "USA", 1944, 4, { 700.0, 760.0, 1100.0, 760.0 }, 60.0, "burn" },
		{ 2, "def", "GER", 1942, 4, { 1050.0, 320.0, 850.0, 320.0 }, 90.0, "spin" },
		{ 3, "att", "SOV", 1943, 2, { 600.0, 700.0, 600.0, 900.0 }, 120.0, "explode" },
		{ 4, "att", "SOV", 1943, 2, { 1000.0, 680.0, 1300.0, 860.0 }, 120.0, "" },
	};

	/** The chutes' second field (the art director's notes, 24 November 02:26Z): an Il-2 spinning in low, its pilot out
	 * under 60 m, and a Mustang burning, in a wind of over 4 m/s (sky.wind, 5.73 m/s east-south-east). */
	const Strike Windy[] = {
		{ 5, "att", "SOV", 1943, 2, { 600.0, 700.0, 600.0, 900.0 }, 60.0, "spin" },
		{ 6, "att", "USA", 1944, 4, { 700.0, 760.0, 1100.0, 760.0 }, 90.0, "burn" },
	};
	constexpr double WindyX = 5.2, WindyY = -2.4;

	/** The recording: a 2 km field, the attacker's blocks to the south, the defender's to the north, and the strikes
	 * (Strikes; or List's Count, with Sky's words at its root). */
	std::string Recording(bool bAll, size_t Upto = 100, bool bOwnOnStrip = false, const Strike* List = Strikes, size_t Count = 4, const char* Sky = "")
	{
		std::ostringstream R;
		R << "{\"key\": \"air-test\", \"date\": \"July 1944\", " << Sky << "\"field\": {\"w\": 2000, \"h\": 2000}, \"men\": [], \"views\": [";
		for (int V = 0; V < 2; ++V)
		{
			R << (V ? "," : "") << "{\"t\": " << (V ? 100 : 0) << ", \"units\": [";
			// (bOwnOnStrip: the attacker's first block stood on the Mustangs' strip, 60 m along it and 20 m aside)
			const double Att[][2] = { { bOwnOnStrip ? 760.0 : 700.0, bOwnOnStrip ? 740.0 : 300.0 }, { 900, 330 }, { 1100, 300 }, { 1500, 250 } };
			const double Def[][2] = { { 650, 780 }, { 900, 760 }, { 1150, 790 }, { 600, 1700 } };
			for (int K = 0; K < 4; ++K) R << (K ? "," : "") << "{\"id\": " << K + 1 << ", \"side\": \"att\", \"x\": " << Att[K][0] << ", \"y\": " << Att[K][1] << ", \"str\": 0.8}";
			for (int K = 0; K < 4; ++K) R << ",{\"id\": " << K + 11 << ", \"side\": \"def\", \"x\": " << Def[K][0] << ", \"y\": " << Def[K][1] << ", \"str\": " << (K == 3 ? 0.0 : 0.7) << "}";
			R << "]}";
		}
		R << "], \"events\": [";
		size_t N = 0;
		bool bFirst = true;
		auto Add = [&](const std::string& E) { if (N++ >= Upto) return; R << (bFirst ? "" : ",") << E; bFirst = false; };
		for (size_t Si = 0; Si < Count; ++Si)
		{
			const Strike& S = List[Si];
			const double Len = std::hypot(S.Run[2] - S.Run[0], S.Run[3] - S.Run[1]), Secs = Len / 120.0;
			std::ostringstream In, Sf, Lo;
			In << "{\"t\": " << S.T << ", \"what\": \"land:air-in\", \"side\": \"" << S.Side << "\", \"text\": \"\", \"x\": " << S.Run[0] << ", \"y\": " << S.Run[1]
			   << ", \"extra\": {\"flight\": " << S.Flight << ", \"run\": [" << S.Run[0] << ", " << S.Run[1] << ", " << S.Run[2] << ", " << S.Run[3] << "], \"models\": " << S.Models
			   << ", \"alt\": 60, \"nat\": \"" << S.Nat << "\", \"year\": " << S.Year << ", \"heading\": " << std::atan2(S.Run[2] - S.Run[0], S.Run[3] - S.Run[1]) << "}, \"cue\": \"fighter_pass\"}";
			Add(In.str());
			Sf << "{\"t\": " << S.T << ", \"what\": \"land:strafe\", \"side\": \"" << S.Side << "\", \"text\": \"\", \"x\": 0, \"y\": 0, \"extra\": {\"flight\": " << S.Flight
			   << ", \"from\": [" << S.Run[0] << ", " << S.Run[1] << "], \"to\": [" << S.Run[2] << ", " << S.Run[3] << "], \"width\": 60, \"secs\": " << Secs << "}, \"cue\": \"machine_gun\"}";
			Add(Sf.str());
			if (bAll && S.Year < 1944)
			{
				std::ostringstream Bd;
				Bd << "{\"t\": " << S.T + Secs * 0.6 << ", \"what\": \"land:bomb-drop\", \"side\": \"" << S.Side << "\", \"text\": \"\", \"x\": 0, \"y\": 0, \"extra\": {\"flight\": " << S.Flight
				   << ", \"from\": {\"x\": 0, \"y\": 0, \"alt\": 60}, \"to\": {\"x\": " << S.Run[0] + (S.Run[2] - S.Run[0]) * 0.8 << ", \"y\": " << S.Run[1] + (S.Run[3] - S.Run[1]) * 0.8 << "}, \"at\": " << S.T + Secs * 0.8 << "}}";
				Add(Bd.str());
			}
			if (*S.Lost)
			{
				Lo << "{\"t\": " << S.T + Secs + 0.1 << ", \"what\": \"air:lost\", \"side\": \"" << S.Side << "\", \"text\": \"\", \"x\": " << S.Run[2] << ", \"y\": " << S.Run[3]
				   << ", \"extra\": {\"flight\": " << S.Flight << ", \"kind\": \"" << S.Lost << "\", \"chutes\": " << (std::strcmp(S.Lost, "explode") ? 1 : 0) << ", \"over\": \"enemy\", \"air\": \"ftr\", \"alt\": 60}, \"cue\": \"plane_down\"}";
				Add(Lo.str());
			}
		}
		R << "], \"rounds\": []}";
		return R.str();
	}

	double Flat(double, double) { return 0.0; }

	uint64_t Hash(const std::vector<Plane>& Ps, const std::vector<Gunner>& Gs, uint64_t H)
	{
		auto Mix = [&H](double V) { uint64_t U = 0; std::memcpy(&U, &V, sizeof(U)); H = (H ^ U) * 1099511628211ull; };
		for (const Plane& P : Ps) { Mix(P.Key); Mix(P.At.X); Mix(P.At.Y); Mix(P.At.Z); Mix(P.Heading); Mix(P.Pitch); Mix(P.Bank); Mix(P.bFiring); }
		for (const Gunner& G : Gs) { Mix(G.Key); Mix(G.To.X); Mix(G.To.Y); Mix(G.To.Z); }
		return H;
	}

	V3 Plus(const V3& A, const V3& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
	V3 Mul(const V3& A, double K) { return { A.X * K, A.Y * K, A.Z * K }; }
	double Length(const V3& A) { return std::sqrt(A.X * A.X + A.Y * A.Y + A.Z * A.Z); }

	double Turn(double A, double B)
	{
		double D = std::fmod(B - A, 360.0);
		if (D > 180.0) D -= 360.0;
		if (D < -180.0) D += 360.0;
		return std::fabs(D);
	}
}

int main(int Argc, char** Argv)
{
	const double Life = 2.0;
	LandFeed::Recording Rec;
	std::string Err;
	if (!LandFeed::ReadRecording(Recording(true), Rec, Err)) { std::printf("FAIL: recording: %s\n", Err.c_str()); return 1; }

	// ---- feed
	int In = 0, Strafe = 0, Bomb = 0, Lost = 0;
	bool bFields = true;
	for (const LandFeed::AirNote& N : Rec.Air)
	{
		In += N.What == "land:air-in";
		Strafe += N.What == "land:strafe";
		Bomb += N.What == "land:bomb-drop";
		Lost += N.What == "air:lost";
		if (N.What == "land:air-in" && N.Flight == 1) bFields = bFields && N.Models == 4 && N.Nat == "USA" && N.Year == 1944 && N.Run[2] == 1100.0 && N.Side == 0;
		if (N.What == "land:strafe" && N.Flight == 2) bFields = bFields && std::fabs(N.Secs - 200.0 / 120.0) < 1e-4 && N.Width == 60.0 && N.Run[0] == 1050.0 && N.Run[2] == 850.0 && N.Side == 1;
		if (N.What == "land:bomb-drop" && N.Flight == 2) bFields = bFields && N.At > 90.0 && N.To[0] == 1050.0 - 200.0 * 0.8;
		if (N.What == "air:lost" && N.Flight == 3) bFields = bFields && N.Kind == "explode" && N.Chutes == 0;
	}
	std::printf("     notes: %d in, %d strafe, %d bombs, %d lost, fields %s\n", In, Strafe, Bomb, Lost, bFields ? "right" : "wrong");
	Check(In == 4 && Strafe == 4 && Bomb == 3 && Lost == 3 && bFields, "feed: every note read, with its flight, run, models, strafe, bombs and loss");

	// ---- machine
	Check(MachineOf("USA", 1944).Model == "Mustang" && MachineOf("USA", 1943).Model == "Warhawk" && MachineOf("GER", 1942).Model == "Stuka" && MachineOf("GER", 1943).Model == "Stuka"
		&& MachineOf("GER", 1944).Model == "Fw190" && MachineOf("UK", 1941).Model == "Hurricane" && MachineOf("CAN", 1944).Model == "Spitfire"
		&& MachineOf("SOV", 1941).Model == "Sturmovik" && MachineOf("JAP", 1942).Model == "Zero" && MachineOf("ITA", 1942).Model == "Fighter",
		"machine: each air force's machine by year");

	Sky S;
	S.Build(Rec, Flat, Life);
	Check(S.Legs() == 4 && S.Notes() == Rec.Air.size(), "build: four legs planned");

	// ---- play it, 60 frames a second
	const double T0 = 40.0, T1 = 170.0, Dt = 1.0 / 60.0;
	struct Seen
	{
		bool bShown = false, bFired = false, bHit = false, bCrashed = false;
		double FirstT = 1e9, MinSpeed = 1e9, MaxSpeed = 0.0, MinPitch = 0.0, MinClear = 1e9, MaxStep = 0.0, MaxHeadStep = 0.0;
		double FireFrom = 1e9, FireTo = -1e9, SlantOpen = -1.0, SlantShut = -1.0;
		double AimStartMiss = 1e9, AimEndMiss = 1e9, AimHigh = 0.0;
		double HitT = -1.0, CrashT = -1.0, MaxHigh = 0.0, LowestFly = 1e9;
		Plane Last;
		bool bLast = false;
		int Hits = 0, Crashes = 0, WreckFrames = 0, WreckOff = 0, ShownAfterExplode = 0;
		LandAir::Fall How = LandAir::Fall::None;
		V3 HitAt, CrashAt;
	};
	std::vector<Seen> Seens(64);
	int GunFrames = 0, OwnSideGuns = 0, FarStreams = 0, LowStreams = 0, NoPlaneGuns = 0, Nans = 0;
	std::vector<int> GunsByLeg(4, 0);
	std::vector<Plane> Ps;
	std::vector<Gunner> Gs;
	std::vector<Happen> Hs;
	uint64_t H1 = 1469598103934665603ull;
	std::ofstream Out;
	if (Argc > 1) Out.open(std::string(Argv[1]) + "/air_paths.txt");
	int RoundsSum = 0;
	for (double T = T0; T <= T1; T += Dt)
	{
		S.At(T, Ps, Gs);
		H1 = Hash(Ps, Gs, H1);
		S.Between(T - Dt, T, Hs);
		RoundsSum += S.Rounds(T - Dt, T, 22.0);
		for (const Happen& H : Hs)
		{
			Seen& Sn = Seens[static_cast<size_t>(H.Key)];
			if (H.Kind == Happen::What::Hit) { ++Sn.Hits; Sn.HitT = H.T; Sn.How = H.How; Sn.HitAt = H.At; }
			else { ++Sn.Crashes; Sn.CrashT = H.T; Sn.CrashAt = H.At; }
			if (Out) Out << (H.Kind == Happen::What::Hit ? "hit " : "crash ") << H.Key << " " << H.T << " " << H.At.X << " " << H.At.Y << " " << H.At.Z << " " << static_cast<int>(H.How) << "\n";
		}
		for (const Plane& P : Ps)
		{
			Seen& Sn = Seens[static_cast<size_t>(P.Key)];
			for (double V : { P.At.X, P.At.Y, P.At.Z, P.Heading, P.Pitch, P.Bank, P.Vel.X, P.Vel.Y, P.Vel.Z }) Nans += std::isfinite(V) ? 0 : 1;
			if (Out) Out << "p " << P.Key << " " << T << " " << P.At.X << " " << P.At.Y << " " << P.At.Z << " " << P.Heading << " " << P.Pitch << " " << P.Bank << " " << P.bFiring << " " << static_cast<int>(P.How) << " " << P.bWreck
				<< " " << P.Aim.X << " " << P.Aim.Y << " " << P.Model << "\n";
			if (P.bWreck)
			{
				++Sn.WreckFrames;
				if (Sn.Crashes == 0 || std::hypot(P.At.X - Sn.CrashAt.X, P.At.Y - Sn.CrashAt.Y) > 1.0) ++Sn.WreckOff;
				continue;
			}
			if (Sn.How == LandAir::Fall::Explode && Sn.Hits > 0) ++Sn.ShownAfterExplode;
			if (!Sn.bShown) { Sn.bShown = true; Sn.FirstT = T; }
			if (P.bFiring) Sn.MaxHigh = std::max(Sn.MaxHigh, P.At.Z);
			if (P.How == LandAir::Fall::None)
			{
				const double Sp = std::sqrt(P.Vel.X * P.Vel.X + P.Vel.Y * P.Vel.Y + P.Vel.Z * P.Vel.Z);
				Sn.MinSpeed = std::min(Sn.MinSpeed, Sp);
				Sn.MaxSpeed = std::max(Sn.MaxSpeed, Sp);
				Sn.MinPitch = std::min(Sn.MinPitch, P.Pitch);
				Sn.MinClear = std::min(Sn.MinClear, P.At.Z);
				Sn.LowestFly = Sn.MinClear; // (the flat field: its height is its clearance; its bottom, once past it)
				if (Sn.bLast && Sn.Last.How == LandAir::Fall::None)
				{
					Sn.MaxStep = std::max(Sn.MaxStep, std::max(std::fabs(P.Pitch - Sn.Last.Pitch), std::fabs(P.Bank - Sn.Last.Bank)));
					Sn.MaxHeadStep = std::max(Sn.MaxHeadStep, Turn(P.Heading, Sn.Last.Heading));
				}
				if (P.bFiring)
				{
					const Strike& St = Strikes[P.Leg];
					const double Fx = (St.Run[2] - St.Run[0]), Fy = (St.Run[3] - St.Run[1]), L = std::hypot(Fx, Fy);
					// (its lane's start and end: the run's ends, off to its side by its place in the leg)
					const double W0 = St.Models > 1 ? (P.Slot - (St.Models - 1) * 0.5) * std::min(18.0, 60.0 / St.Models) : 0.0;
					const double Sx = St.Run[0] + Fy / L * W0, Sy = St.Run[1] - Fx / L * W0, Ex = St.Run[2] + Fy / L * W0, Ey = St.Run[3] - Fx / L * W0;
					if (!Sn.bFired) Sn.AimStartMiss = std::hypot(P.Aim.X - Sx, P.Aim.Y - Sy);
					const double Slant = std::hypot(std::hypot(P.At.X - P.Aim.X, P.At.Y - P.Aim.Y), P.At.Z - P.Aim.Z);
					if (!Sn.bFired) Sn.SlantOpen = Slant;
					Sn.SlantShut = Slant;
					Sn.AimEndMiss = std::hypot(P.Aim.X - Ex, P.Aim.Y - Ey);
					Sn.AimHigh = std::max(Sn.AimHigh, std::fabs(P.Aim.Z));
					Sn.bFired = true;
					Sn.FireFrom = std::min(Sn.FireFrom, T);
					Sn.FireTo = std::max(Sn.FireTo, T);
				}
			}
			Sn.Last = P;
			Sn.bLast = true;
		}
		for (const Gunner& G : Gs)
		{
			++GunFrames;
			const int Leg = G.Key / 8;
			++GunsByLeg[static_cast<size_t>(Leg)];
			// (from one of the other side's blocks: the att's strikes over the def's blocks, to the north)
			const bool bAttStrike = std::strcmp(Strikes[Leg].Side, "att") == 0;
			if ((bAttStrike && G.From.Y < 600.0) || (!bAttStrike && G.From.Y > 600.0)) ++OwnSideGuns;
			double Near = 1e9;
			for (const Plane& P : Ps) if (P.Key / 8 == Leg && !P.bWreck) Near = std::min(Near, std::hypot(std::hypot(G.To.X - P.At.X, G.To.Y - P.At.Y), G.To.Z - P.At.Z));
			if (Near > 1e8) ++NoPlaneGuns;
			else if (Near > 150.0) ++FarStreams;
			if (G.To.Z < 8.0) ++LowStreams;
			if (Out) Out << "g " << G.Key << " " << T << " " << G.From.X << " " << G.From.Y << " " << G.To.X << " " << G.To.Y << " " << G.To.Z << "\n";
		}
	}
	if (Out)
	{
		for (const Strike& St : Strikes) Out << "run " << St.Flight << " " << St.Run[0] << " " << St.Run[1] << " " << St.Run[2] << " " << St.Run[3] << " " << St.Nat << " " << St.Year << " " << St.Side << "\n";
		for (const LandFeed::Unit& U : Rec.Views[0].Units) Out << "u " << U.Id << " " << U.Side << " " << U.X << " " << U.Y << " " << U.Str << "\n";
	}

	// ---- flight
	int Early = 0, Slow = 0, Ground = 0, Jerky = 0, NotShown = 0, BadDive = 0;
	double Stuka = 0.0, Il2High = 0.0;
	for (size_t L = 0; L < 4; ++L)
	{
		const Strike& St = Strikes[L];
		const Machine M = MachineOf(St.Nat, St.Year);
		for (int K = 0; K < St.Models; ++K)
		{
			const Seen& Sn = Seens[L * 8 + static_cast<size_t>(K)];
			if (!Sn.bShown) { ++NotShown; continue; }
			if (Sn.FirstT < St.T - 25.0 / Life) ++Early;
			if (Sn.MinSpeed < 0.9 * M.Speed || Sn.MaxSpeed > 1.2 * M.Speed) { ++Slow; std::printf("     note: leg %zu plane %d speed %.0f to %.0f m/s (its machine's %.0f)\n", L, K, Sn.MinSpeed, Sn.MaxSpeed, M.Speed); }
			// (the art director's shapes, 19:08Z: a fighter's dive 20 to 30 degrees, out 30 to 60 m up, guns from about
			// 900 m to 250 m; a Stuka's 70 to 80, out about 300 m up; an Il-2's out 10 to 20 m up)
			const bool bLost = *St.Lost && K == St.Models - 1;
			const double Dive = -Sn.MinPitch;
			bool bShape = true;
			if (M.Model == "Stuka") bShape = Dive >= 70.0 && Dive <= 80.0 && Sn.LowestFly >= 250.0 && Sn.LowestFly <= 350.0;
			else if (M.Model == "Sturmovik") bShape = Sn.LowestFly >= 10.0 && Sn.LowestFly <= 20.0;
			else bShape = Dive >= 20.0 && Dive <= 30.0 && Sn.LowestFly >= 30.0 && Sn.LowestFly <= 60.0 && Sn.SlantOpen >= 800.0 && Sn.SlantOpen <= 1000.0 && Sn.SlantShut >= 180.0 && Sn.SlantShut <= 320.0;
			// (the one lost: hit before its bottom or soon after, so only its dive and its guns' opening)
			if (bLost) bShape = M.Model == "Stuka" ? Dive >= 70.0 && Dive <= 80.0 : M.Model == "Sturmovik" ? true : Dive >= 20.0 && Dive <= 30.0 && Sn.SlantOpen >= 800.0 && Sn.SlantOpen <= 1000.0;
			if (!bShape) ++BadDive;
			std::printf("     leg %zu plane %d (%s): dive %.0f deg, bottom %.0f m, guns from %.0f m to %.0f m slant\n", L, K, M.Model.c_str(), Dive, Sn.LowestFly, Sn.SlantOpen, Sn.SlantShut);
			if (Sn.MinClear < 5.0) ++Ground;
			if (Sn.MaxStep > 3.0 || Sn.MaxHeadStep > 3.0) { ++Jerky; std::printf("     note: leg %zu plane %d steps %.1f deg (heading %.1f) a frame\n", L, K, Sn.MaxStep, Sn.MaxHeadStep); }
			if (M.Model == "Stuka") Stuka = std::min(Stuka, Sn.MinPitch);
			if (M.Model == "Sturmovik") Il2High = std::max(Il2High, Sn.MaxHigh);
		}
	}
	Check(NotShown == 0 && Early == 0, "flight: every plane drawn, none long before its leg comes");
	Check(Slow == 0, "flight: each at its machine's speed along its way (0.9 to 1.2 times: the climb away's on top)");
	Check(BadDive == 0, "flight: the art director's shapes (fighters 20 to 30 deg, out 30 to 60 m, guns 800-1000 to 180-320 m; Stuka 70 to 80 deg, out 250 to 350 m; Il-2 out 10 to 20 m)");
	Check(Ground == 0, "flight: none flies into the ground (5 m and over)");
	Check(Jerky == 0, "flight: heading, pitch and bank smooth (3 degrees a frame at most)");
	char Words[200];
	std::snprintf(Words, sizeof(Words), "flight: a Stuka dives steeply (%.0f deg), an Il-2 stays low (%.0f m at most)", Stuka, Il2High);
	Check(Stuka < -65.0 && Il2High < 200.0, Words);

	// ---- guns
	int NoFire = 0, OffLane = 0, AimUp = 0, BadGap = 0;
	for (size_t L = 0; L < 4; ++L)
	{
		const Strike& St = Strikes[L];
		double From = 1e9, To = -1e9;
		for (int K = 0; K < St.Models; ++K)
		{
			const Seen& Sn = Seens[L * 8 + static_cast<size_t>(K)];
			if (!Sn.bFired) { ++NoFire; continue; }
			const bool bLost = *St.Lost && K == St.Models - 1;
			if (Sn.AimStartMiss > 10.0 || (!bLost && Sn.AimEndMiss > 10.0)) { ++OffLane; std::printf("     note: leg %zu plane %d aim starts %.1f m and ends %.1f m off its lane\n", L, K, Sn.AimStartMiss, Sn.AimEndMiss); }
			if (Sn.AimHigh > 0.01) ++AimUp;
			if (K > 0)
			{
				const double Gap = (Sn.FireFrom - Seens[L * 8 + static_cast<size_t>(K) - 1].FireFrom) * Life;
				if (Gap < 5.99 || Gap > 12.01) { ++BadGap; std::printf("     note: leg %zu plane %d dives %.1f s after the one before\n", L, K, Gap); }
			}
			From = std::min(From, Sn.FireFrom);
			To = std::max(To, Sn.FireTo);
		}
		std::printf("     leg %zu (%s %d, %d planes): guns on the strip from %.1f to %.1f battle s (the rules' run %.1f s from %.1f)\n", L, St.Nat, St.Year, St.Models, From, To,
			std::hypot(St.Run[2] - St.Run[0], St.Run[3] - St.Run[1]) / 120.0, St.T);
	}
	Check(NoFire == 0 && OffLane == 0 && AimUp == 0, "guns: each plane's guns walk its lane of the strip from start to end (the lost one till it is hit), on the ground");
	Check(BadGap == 0, "guns: each plane dives 6 to 12 s after the one before (each dive its own beat)");
	{
		// (a lane never over its own men: the attacker's block stood on the Mustangs' strip, 60 m along it)
		LandFeed::Recording Own;
		LandFeed::ReadRecording(Recording(true, 100, true), Own, Err);
		Sky O;
		O.Build(Own, Flat, Life);
		double First = 1e9;
		std::vector<Plane> Op;
		std::vector<Gunner> Og;
		for (double T = T0; T <= T1; T += Dt)
		{
			O.At(T, Op, Og);
			for (const Plane& P : Op) if (P.Leg == 0 && P.bFiring) First = std::min(First, P.Aim.X - 700.0);
		}
		std::printf("     with its own block 60 m along the strip, the Mustangs' guns strike from %.0f m along it\n", First);
		Check(First >= 209.0 && First < 400.0, "guns: a lane starts 150 m past its own side's front at the least");
	}

	// ---- losses
	int Hit = 0, BadOrder = 0, BadFall = 0, OtherFell = 0, NoWreck = 0, Explodes = 0;
	for (size_t L = 0; L < 4; ++L)
	{
		const Strike& St = Strikes[L];
		for (int K = 0; K < St.Models; ++K)
		{
			const Seen& Sn = Seens[L * 8 + static_cast<size_t>(K)];
			const bool bLost = *St.Lost && K == St.Models - 1;
			if (!bLost) { OtherFell += Sn.Hits + Sn.Crashes; continue; }
			Hit += Sn.Hits;
			if (Sn.Hits != 1 || Sn.Crashes != 1 || Sn.CrashT <= Sn.HitT || std::fabs(Sn.CrashAt.Z) > 0.01) ++BadOrder;
			const double Fell = (Sn.CrashT - Sn.HitT) * Life, Across = std::hypot(Sn.CrashAt.X - Sn.HitAt.X, Sn.CrashAt.Y - Sn.HitAt.Y), Drop = Sn.HitAt.Z - Sn.CrashAt.Z;
			std::printf("     leg %zu: %s from %.0f m, %.1f s down, %.0f m on, at %.0f deg\n", L, St.Lost, Drop, Fell, Across, std::atan2(Drop, std::max(1.0, Across)) * 57.2958);
			if (std::strcmp(St.Lost, "burn") == 0 && (Fell < 2.0 || Fell > 25.0 || std::atan2(Drop, Across) * 57.2958 > 30.0)) ++BadFall;
			if (std::strcmp(St.Lost, "spin") == 0 && (std::atan2(Drop, std::max(1.0, Across)) * 57.2958 < 30.0 || Drop / std::max(0.1, Fell) < 45.0)) ++BadFall;
			if (std::strcmp(St.Lost, "explode") == 0) { ++Explodes; if (Sn.ShownAfterExplode > 0 || Sn.WreckFrames > 0) ++BadFall; }
			else if (Sn.WreckFrames == 0 || Sn.WreckOff > 0) ++NoWreck;
		}
	}
	Check(Hit == Lost && BadOrder == 0, "losses: as many planes hit as the rules lost, each hit once and into the ground once after");
	Check(BadFall == 0 && Explodes == 1, "losses: on fire a long dive, a spin steep and quick (about 3 s from 150 m), blown up gone at once");
	Check(NoWreck == 0, "losses: a wreck where each burning or spinning plane struck, the rest of the battle");
	Check(OtherFell == 0, "losses: no other plane falls");

	// ---- ground
	std::printf("     ground fire: %d gun frames (by leg %d %d %d %d)\n", GunFrames, GunsByLeg[0], GunsByLeg[1], GunsByLeg[2], GunsByLeg[3]);
	Check(GunFrames > 0 && GunsByLeg[0] > 0 && GunsByLeg[1] > 0 && GunsByLeg[2] + GunsByLeg[3] > 0, "ground: each strike fired on");
	Check(OwnSideGuns == 0, "ground: only the other side's blocks fire (a block with no strength never)");
	std::printf("     (streams with no plane %d, over 150 m from one %d, under 8 m up %d)\n", NoPlaneGuns, FarStreams, LowStreams);
	Check(NoPlaneGuns == 0 && FarStreams == 0 && LowStreams == 0, "ground: each stream ends in the air near a plane in reach (150 m: short of its lead)");

	// ---- same
	{
		Sky B;
		B.Build(Rec, Flat, Life);
		uint64_t H2 = 1469598103934665603ull;
		for (double T = T0; T <= T1; T += Dt) { B.At(T, Ps, Gs); H2 = Hash(Ps, Gs, H2); }
		std::printf("     planes hash %016llx (as before the chutes' notes: e258eaf3320235ff)\n", static_cast<unsigned long long>(H1));
		Check(H2 == H1, "same: built twice, the same planes and guns frame for frame");
		// (a live battle's notes come a few at a time: the first leg flown the same whatever came after)
		LandFeed::Recording Part;
		LandFeed::ReadRecording(Recording(true, 3), Part, Err);
		Sky C;
		C.Build(Part, Flat, Life);
		bool bSame = true;
		for (double T = T0; T <= T1; T += 0.5)
		{
			std::vector<Plane> A1, A2;
			std::vector<Gunner> G1, G2;
			S.At(T, A1, G1);
			C.At(T, A2, G2);
			std::vector<Plane> F1, F2;
			for (const Plane& P : A1) if (P.Leg == 0) F1.push_back(P);
			for (const Plane& P : A2) if (P.Leg == 0) F2.push_back(P);
			bSame = bSame && Hash(F1, {}, 1) == Hash(F2, {}, 1);
		}
		Check(bSame, "same: grown note by note as a live battle grows, its first leg flown the same");
		int Coarse = 0;
		const int Frames = static_cast<int>(std::ceil((T1 - T0) * 23.0));
		for (int F = 0; F < Frames; ++F) Coarse += S.Rounds(T0 + F / 23.0, std::min(T1, T0 + (F + 1) / 23.0), 22.0);
		std::printf("     rounds at 22 a second: %d whole, %d at 23 frames a second, %d at 60\n", S.Rounds(T0, T1, 22.0), Coarse, RoundsSum);
		Check(Coarse == S.Rounds(T0, T1, 22.0) && std::abs(RoundsSum - S.Rounds(T0, T1, 22.0)) <= 2, "same: the rounds the same however the frames fall");
		Check(Nans == 0, "same: no NaN");
	}
	// ---- chutes (-TheaterLandChutes): the burning plane's and the spinning plane's pilots get out (the rules' chutes),
	// the blown-up one's not; and the art director's notes of 24 November, 02:26Z, on them and on a second field (an
	// Il-2's pilot out low, a Mustang's, in a wind over 4 m/s)
	{
		Check(S.ChutesPlanned() == 2, "chutes: a pilot out of each plane the rules say he got out of, none out of the one blown apart");
		LandFeed::Recording WindyRec;
		const std::string WindySky = "\"sky\": {\"wind\": [" + std::to_string(WindyX) + ", " + std::to_string(WindyY) + "]}, ";
		LandFeed::ReadRecording(Recording(true, 100, false, Windy, 2, WindySky.c_str()), WindyRec, Err);
		Sky S2;
		S2.Build(WindyRec, Flat, Life);
		struct Watched
		{
			double First = -1.0, OutHigh = 0.0, OpenHigh = -1.0, LandT = -1.0, MostStep = 0.0, MinSink = 1e9, MaxSink = 0.0;
			V3 Was, FirstAt, LandAt;
			bool bWas = false;
			std::string Clips;
			double FirstFromPlane = 1e9;
			int Under = 0;
			double HeapOff = 0.0, SlumpEnd = 0.0, EnemyAtLand = 0.0, EnemyAtEnd = 0.0;
			// (the art director's notes: falling free, above where he left her or not half g t squared under it; his
			// canopy full (real seconds after the first frame) and how far he had dropped by then, and by the time he
			// was first within 1 m/s of the wind; his way through the air growing again, or off the wind's 3 s after his
			// canopy was full; as he lands, his canopy's open share;
			// the swap (his going over and his canopy's slump then); the clips' times; how far he was dragged and how
			// nearly with the wind (the cosine); his run's speed; his swing's crossings and its most early and late)
			int Rose = 0, Shallow = 0, Grew = 0, OffWind = 0, Swapped = 0, BadSwap = 0;
			double FullT = -1.0, DropToFull = 0.0, LastRel = 1e18, OpenDown = -1.0, WithAir = -1.0, LandRel = -1.0;
			bool bHard = false, bInMeshWas = false;
			double FallenWas = 0.0, SwapSlump = -1.0;
			double KneelFrom = -1.0, KneelTo = -1.0, BackFrom = -1.0, RiseT = -1.0, DragMost = 0.0, DragWay = 0.0, DragDone = -1.0;
			double RunFrom = -1.0, RunTo = -1.0, RunDist = 0.0;
			V3 RunStart, RunLast;
			std::vector<double> Crossings;
			double SwingEarly = 0.0, SwingLate = 0.0, SwingLateLeast = 1e9, LastSwing = 0.0, SwingT0 = -1.0;
			Chute Look;
			// (a review of 24 November: the way he faced as he landed, kept while he is down; his clips' blends)
			double FaceAt = 1e9;
			int FaceMoved = 0, BlendBad = 0;
		};
		auto Play = [&](const Sky& Sk, const LandFeed::Recording& R, double Wx, double Wy, std::map<int, Watched>& Sn, std::ofstream* To, int Tag)
		{
			// (for chutes.py: the field's wind and life, and each pilot's frames under his plane's key, Tag over it)
			if (To && To->is_open()) *To << "w " << Tag << " " << Wx << " " << Wy << " " << Life << "\n";
			std::vector<Happen> Hc;
			Sk.Between(0.0, 400.0, Hc);
			std::map<int, double> Hits;
			for (const Happen& H : Hc) if (H.Kind == Happen::What::Hit) Hits[H.Key] = H.T;
			const double Dc = 1.0 / 60.0;
			auto NearestEnemy = [&](const V3& At, int Side)
			{
				double Best = 1e18;
				const LandFeed::View* V = R.ViewAt(200.0);
				if (V) for (const LandFeed::Unit& U : V->Units) if (U.Side >= 0 && U.Side != Side && U.Str > 0.05) Best = std::min(Best, std::hypot(U.X - At.X, U.Y - At.Y));
				return Best;
			};
			std::vector<Chute> Cs;
			std::vector<Plane> Pl;
			std::vector<Gunner> Gn;
			for (double T = T0; T <= 400.0; T += Dc)
			{
				Sk.Chutes(T, Cs);
				Sk.At(T, Pl, Gn);
				for (const Chute& C : Cs)
				{
					if (To && To->is_open() && std::fmod(T, 0.05) < Dc)
						*To << "c " << C.Key + Tag << " " << T << " " << C.At.X << " " << C.At.Y << " " << C.At.Z << " " << C.Open << " " << C.Swing << " " << (C.bDown ? 1 : 0) << " " << (C.Clip.empty() ? "-" : C.Clip)
							<< " " << C.Heap.X << " " << C.Heap.Y << " " << C.Slump << " " << (C.bInMesh ? 1 : 0) << " " << C.Fallen << " " << C.Rate << " " << (C.bHard ? 1 : 0) << " " << C.Dragged
							<< " " << C.Time << " " << C.Vel.X << " " << C.Vel.Y << " " << C.Vel.Z << "\n";
					Watched& W = Sn[C.Key];
					const double Over = C.At.Z - Flat(C.At.X, C.At.Y);
					if (W.First < 0.0)
					{
						W.First = T;
						W.OutHigh = Over;
						W.FirstAt = C.At;
						W.Look = C;
						for (const Plane& P : Pl) if (P.Key == C.Key) W.FirstFromPlane = Length(Plus(P.At, Mul(C.At, -1.0)));
						if (Hits.count(C.Key)) W.SwingT0 = Hits[C.Key];
					}
					if (!std::isfinite(C.At.X) || !std::isfinite(C.At.Z) || !std::isfinite(C.Swing) || !std::isfinite(C.Time) || !std::isfinite(C.Rate)) ++Nans;
					W.Under += Over < -0.01;
					if (W.bWas) W.MostStep = std::max(W.MostStep, Length(Plus(C.At, Mul(W.Was, -1.0))) / (Dc * Life));
					W.Was = C.At;
					W.bWas = true;
					if (!C.bDown)
					{
						const double Since0 = (T - W.First) * Life;
						// (her first note: falling free he is never above where he left her, and at least half g t squared under)
						if (C.Open <= 0.0)
						{
							W.Rose += C.At.Z > W.FirstAt.Z + 1e-9;
							W.Shallow += C.At.Z > W.FirstAt.Z - 0.5 * 9.81 * Since0 * Since0 + 1e-6;
						}
						// (her second: his way through the air only ever dies away, and he is with the wind 3 s after his canopy
						// is full)
						const double Rel = std::hypot(C.Vel.X - Wx, C.Vel.Y - Wy);
						W.Grew += Rel > W.LastRel * (1.0 + 1e-9) + 1e-9;
						W.LastRel = Rel;
						if (W.WithAir < 0.0 && Rel < 1.0) W.WithAir = W.FirstAt.Z - C.At.Z;
						if (C.Open >= 1.0 && W.FullT < 0.0) { W.FullT = Since0; W.DropToFull = W.FirstAt.Z - C.At.Z; }
						if (W.FullT >= 0.0 && Since0 >= W.FullT + 3.0 && Over > 2.0) W.OffWind += Rel > 1.0;
						if (C.Open >= 1.0) W.OpenHigh = W.OpenHigh < 0.0 ? Over : W.OpenHigh;
						if (C.Open >= 1.0 && T > W.First + 8.0 / Life && Over > 3.0) { W.MinSink = std::min(W.MinSink, -C.Vel.Z); W.MaxSink = std::max(W.MaxSink, -C.Vel.Z); }
						// (the swing: its crossings, its most in its first 2.5 s and after 25 s, and its least over each 4 s then)
						if (W.FullT >= 0.0)
						{
							const double After = Since0 - W.FullT;
							if (W.LastSwing * C.Swing < 0.0) W.Crossings.push_back(After);
							W.LastSwing = C.Swing != 0.0 ? C.Swing : W.LastSwing;
							if (After < 2.5) W.SwingEarly = std::max(W.SwingEarly, std::fabs(C.Swing));
							if (After > 25.0) W.SwingLate = std::max(W.SwingLate, std::fabs(C.Swing));
						}
						continue;
					}
					if (W.LandT < 0.0)
					{
						W.LandT = T;
						W.LandAt = C.Landed;
						W.EnemyAtLand = NearestEnemy(C.At, C.Side);
						W.HeapOff = std::hypot(C.Heap.X - C.Landed.X, C.Heap.Y - C.Landed.Y);
						W.OpenDown = C.Open;
						W.LandRel = W.LastRel;
						W.bHard = C.bHard;
						W.bInMeshWas = true;
					}
					const double Down = (T - W.LandT) * Life;
					// (her third note: swapped for the crowd's man as he lies flat under his falling canopy)
					if (W.bInMeshWas && !C.bInMesh)
					{
						++W.Swapped;
						W.SwapSlump = C.Slump;
						W.BadSwap += W.FallenWas < 0.999 || C.Slump < 0.1 || C.Slump > 0.7;
					}
					W.bInMeshWas = C.bInMesh;
					W.FallenWas = C.Fallen;
					if (W.FaceAt > 1e8) W.FaceAt = C.Face;
					W.FaceMoved += C.Face != W.FaceAt;
					if (C.bInMesh) continue;
					W.BlendBad += C.Blend != (C.Clip == "kneel_down" ? 0.5 : 0.25);
					if (W.Clips.empty() || W.Clips.substr(W.Clips.rfind(' ') + 1) != C.Clip) W.Clips += " " + C.Clip;
					// (her fourth: dragged, then back up onto his knees, his harness worked free, up)
					if (C.Dragged > W.DragMost)
					{
						W.DragMost = C.Dragged;
						const double Dx = C.Landed.X - W.LandAt.X, Dy = C.Landed.Y - W.LandAt.Y;
						W.DragWay = (Dx * Wx + Dy * Wy) / std::max(1e-9, std::hypot(Dx, Dy) * std::hypot(Wx, Wy));
						W.DragDone = Down;
					}
					if (C.Clip == "go_prone" && C.Rate < 0.0 && W.BackFrom < 0.0) W.BackFrom = Down;
					if (C.Clip == "go_prone" && W.RiseT < 0.0 && (C.Rate < 0.0 || C.Time < 1.9 - 1e-9) && W.KneelFrom < 0.0) W.RiseT = Down;
					if (C.Clip == "kneel_down") { if (W.KneelFrom < 0.0) W.KneelFrom = Down; W.KneelTo = Down; }
					if (C.Clip == "run")
					{
						if (W.RunFrom < 0.0) { W.RunFrom = Down; W.RunStart = C.At; }
						W.RunTo = Down;
						W.RunLast = C.At;
					}
					W.SlumpEnd = C.Slump;
					W.EnemyAtEnd = NearestEnemy(C.At, C.Side);
				}
			}
		};
		std::map<int, Watched> Sn, Sw;
		Play(S, Rec, 2.6, 1.2, Sn, Out.is_open() ? &Out : nullptr, 0);
		Play(S2, WindyRec, WindyX, WindyY, Sw, Out.is_open() ? &Out : nullptr, 100);
		std::map<int, double> HitT, CrashT;
		std::vector<Happen> Hc;
		S.Between(0.0, 400.0, Hc);
		for (const Happen& H : Hc) { if (H.Kind == Happen::What::Hit) HitT[H.Key] = H.T; else CrashT[H.Key] = H.T; }
		int Bad = 0;
		for (const auto& Pair : Sn)
		{
			const Watched& W = Pair.second;
			const double HitAtT = HitT.count(Pair.first) ? HitT[Pair.first] : -1.0, CrashAtT = CrashT.count(Pair.first) ? CrashT[Pair.first] : -1.0;
			std::printf("     pilot of plane %d: out %.2f s after the hit (%.0f m up, %.1f m from her), canopy full %.0f m up, down at %.1f m/s to %.1f, landed %.0f s after he left;%s; canopy %.1f m downwind, fallen in %.2f; %.0f m from the nearest enemy, then %.0f; step %.0f m/s, under %d\n",
				Pair.first, (W.First - HitAtT) * Life, W.OutHigh, W.FirstFromPlane, W.OpenHigh, W.MinSink, W.MaxSink, (W.LandT - W.First) * Life, W.Clips.c_str(), W.HeapOff, W.SlumpEnd, W.EnemyAtLand, W.EnemyAtEnd, W.MostStep, W.Under);
			const bool bOk = HitAtT >= 0.0 && W.First > HitAtT && W.First < CrashAtT && W.FirstFromPlane < 6.0 && W.OutHigh >= 30.0 && W.OpenHigh > 15.0
				&& W.MinSink >= 4.5 && W.MaxSink <= 7.0 && W.LandT > W.First && W.Under == 0 && W.MostStep < 170.0
				&& W.Clips == " go_prone kneel_down stand_up run go_prone" && std::abs(W.HeapOff - 4.0) < 0.01 && W.SlumpEnd > 0.999 && W.EnemyAtEnd > W.EnemyAtLand + 15.0;
			Bad += !bOk;
		}
		Check(Sn.size() == 2 && Bad == 0, "chutes: out of her just after the hit and before she strikes, his canopy full well up, down at 5 to 6.5 m/s, never under the ground; down, flat, up onto his knees, his harness, up, running from the enemy, flat; his canopy fallen in 4 m downwind");
		Sky B2;
		B2.Build(Rec, Flat, Life);
		bool bSameChutes = true;
		std::vector<Chute> C1, C2;
		for (double T = T0; T <= 400.0; T += 0.25)
		{
			S.Chutes(T, C1);
			B2.Chutes(T, C2);
			bSameChutes = bSameChutes && C1.size() == C2.size();
			for (size_t K = 0; K < C1.size() && K < C2.size(); ++K) bSameChutes = bSameChutes && C1[K].At.X == C2[K].At.X && C1[K].At.Z == C2[K].At.Z && C1[K].Clip == C2[K].Clip;
		}
		Check(bSameChutes, "chutes: the same twice");
		Check(Nans == 0, "chutes: no NaN");

		// The art director's notes (24 November, 02:26Z), on both fields' pilots.
		std::vector<std::pair<int, const Watched*>> All;
		for (const auto& Pair : Sn) All.emplace_back(Pair.first, &Pair.second);
		for (const auto& Pair : Sw) All.emplace_back(100 + Pair.first, &Pair.second);
		int Rose = 0, Shallow = 0, Grew = 0, OffWind = 0, FarOff = 0, BadSwap = 0, Swapped = 0, Low = 0, HardBad = 0, KneelBad = 0, DragBad = 0, SwingBad = 0, LookBad = 0;
		int FaceBad = 0, BlendBad = 0;
		for (const auto& Each : All)
		{
			const Watched& W = *Each.second;
			const bool bWindy = Each.first >= 100, bLow = W.OutHigh < 60.0;
			const double Lie = W.RiseT, Kneeled = W.KneelTo - W.KneelFrom, RunSpeed = W.RunTo > W.RunFrom ? std::hypot(W.RunLast.X - W.RunStart.X, W.RunLast.Y - W.RunStart.Y) / (W.RunTo - W.RunFrom) : 0.0;
			double Period = 0.0;
			if (W.Crossings.size() >= 5) Period = 2.0 * (W.Crossings.back() - W.Crossings.front()) / static_cast<double>(W.Crossings.size() - 1);
			std::printf("     %s pilot of plane %d: out %.0f m up%s; free fall rose %d, under half g t^2 %d; canopy full %.1f s out, %.0f m down; with the wind %.0f m down; way grew %d, off the wind %d; landed %s (canopy %.2f open, %.1f m/s through the air); swapped %d at slump %.2f; lay %.1f s, back up from %.1f s, kneeling %.2f s; dragged %.2f m (%.2f with the wind); run %.1f m/s; swing %.2f s a swing, %.1f deg at first, %.1f late\n",
				bWindy ? "windy:" : "calm: ", Each.first % 100, W.OutHigh, bLow ? " (low)" : "", W.Rose, W.Shallow, W.FullT, W.DropToFull, W.WithAir, W.Grew, W.OffWind, W.bHard ? "hard" : "soft", W.OpenDown, W.LandRel,
				W.Swapped, W.SwapSlump, Lie, W.BackFrom, Kneeled, W.DragMost, W.DragWay, RunSpeed, Period, W.SwingEarly, W.SwingLate);
			Rose += W.Rose;
			Shallow += W.Shallow;
			Grew += W.Grew;
			OffWind += W.OffWind;
			if (!bLow) FarOff += W.FullT < 0.0 || W.DropToFull < 40.0 || W.DropToFull > 75.0 || W.WithAir < 40.0 || W.WithAir > 75.0;
			BadSwap += W.BadSwap;
			Swapped += W.Swapped == 1;
			FaceBad += W.FaceMoved;
			BlendBad += W.BlendBad;
			KneelBad += Kneeled < 1.0 - 0.03 || Kneeled > 1.5 + 0.03 || W.BackFrom < 0.0 || W.BackFrom >= W.KneelFrom || W.Clips.find(" kneel_down stand_up ") == std::string::npos;
			if (bLow)
			{
				++Low;
				HardBad += !W.bHard || W.OpenDown >= 1.0 || W.LandRel > 8.0 || Lie < 3.0 - 0.03 || Lie > 5.0 + 0.03 || RunSpeed < 1.2 || RunSpeed > 0.7 * 4.2 || W.EnemyAtEnd < W.EnemyAtLand + 6.0;
			}
			else
			{
				HardBad += W.bHard || W.OpenDown < 1.0 || RunSpeed < 4.1;
				Period = Period > 0.0 ? Period : -1.0;
				SwingBad += Period < 3.8 || Period > 4.4 || W.SwingEarly > 14.5 || W.SwingEarly < 10.0 || (W.SwingLate > 0.0 && (W.SwingLate < 3.0 || W.SwingLate > 6.5));
			}
			DragBad += bWindy ? W.DragMost < 1.0 - 1e-6 || W.DragMost > 2.0 + 1e-6 || W.DragDone < 0.0 || W.DragDone > W.BackFrom || W.DragWay < 0.99 : W.DragMost > 0.0;
			// (her sixth note: his canopy's look, full)
			SilkMesh M;
			CanopyShape(W.Look, 1.0, M);
			double MinX = 1e9, MaxX = -1e9, MinZ = 1e9, MaxZ = -1e9, Seam = 1e9, Panel = 0.0;
			for (size_t K = 0; K < M.Points.size(); ++K)
			{
				MinX = std::min(MinX, M.Points[K].X); MaxX = std::max(MaxX, M.Points[K].X);
				MinZ = std::min(MinZ, M.Points[K].Z); MaxZ = std::max(MaxZ, M.Points[K].Z);
				Seam = std::min(Seam, M.Colours[K * 3 + 1]);
				Panel = std::max(Panel, M.Colours[K * 3 + 1]);
			}
			int Up = 0, DownFacing = 0;
			for (const V3& O : M.Outward) { Up += O.Z > 0.2; DownFacing += O.Z < -0.2; }
			// (its lines' ends: one at each seam, on its skirt, its lowest)
			double SeamLow = -1e9;
			for (const int K : M.Seams) SeamLow = std::max(SeamLow, M.Points[static_cast<size_t>(K)].Z);
			const double Across = MaxX - MinX, Deep = MaxZ - MinZ, Warm = W.Look.Silk[0] / 0.86 - 1.0, Cool = 1.0 - W.Look.Silk[2] / 0.80;
			std::printf("        his canopy: %.2f m across, %.2f deep, %d gores (%zu points, %zu triangles, %d facing up and %d down), seams %.0f%% darker, silk %.3f %.3f %.3f\n",
				Across, Deep, W.Look.Gores, M.Points.size(), M.Triangles.size() / 3, Up, DownFacing, (1.0 - Seam / Panel) * 100.0, W.Look.Silk[0], W.Look.Silk[1], W.Look.Silk[2]);
			LookBad += Across < 4.9 || Across > 5.6 || Deep < 0.42 * Across || Deep > 0.55 * Across || W.Look.Gores < 24 || W.Look.Gores > 28 || W.Look.Gores % 2
				|| (1.0 - Seam / Panel) < 0.05 || (1.0 - Seam / Panel) > 0.2 || std::fabs(Warm) > 0.0401 || std::fabs(Cool) > 0.0401 || W.Look.Silk[1] != 0.85
				|| static_cast<int>(M.Triangles.size() / 3) != static_cast<int>(M.Outward.size()) || Up < 100 || DownFacing < 100
				|| static_cast<int>(M.Seams.size()) != W.Look.Gores || SeamLow > MinZ + 1e-9;
		}
		Check(Rose == 0 && Shallow == 0, "chutes: falling free he is never above where he left her, and at least half g t squared under it (the art director's first note)");
		Check(FarOff == 0 && Grew == 0 && OffWind == 0, "chutes: about 50 m down before his canopy is full and the wind takes him (within 1 m/s of it; 40 to 75 m), his way through the air only dying away (the wind no push from rest), and with the wind still 3 s after his canopy is full (her second)");
		Check(BadSwap == 0 && Swapped == static_cast<int>(All.size()), "chutes: drawn in his chute's mesh until he lies flat under his falling canopy, then the crowd's man (her third)");
		Check(KneelBad == 0, "chutes: up onto his knees (go_prone played back), his harness worked free kneeling 1 to 1.5 s (kneel_down's last pose), then stand_up (her fourth)");
		Check(BlendBad == 0 && FaceBad == 0, "chutes: each clip blended into over 0.25 s, the harness's kneel over 0.5 s (go_prone's knees far from it); he lies, and his harness, the way he faced as he landed");
		Check(DragBad == 0, "chutes: in a wind over 4 m/s his canopy drags him 1 to 2 m before he gets up; in a lighter one not at all (her fourth)");
		Check(Low >= 1 && HardBad == 0, "chutes: out under 60 m his canopy is still filling as he lands, hard (but its way spent: 8 m/s through the air at most); he lies 3 to 5 s and limps off slower; the rest land soft and run (her fifth)");
		Check(SwingBad == 0, "chutes: his swing about 4 s a swing, from 14 degrees settling to about 5 and never dead still (her answers)");
		Check(LookBad == 0, "chutes: his canopy a dome 5 to 5.5 m across and about half as deep, on 24 to 28 gores with faint darker seams, a line at each, both its faces; its silk her off-white within 4% (her sixth)");
		// (the glow: looking up into it a little, against the sun most, the sun behind the eye or under the field none)
		const double Below = Glow({ 0.0, 0.0, 0.0 }, { 0.0, 100.0, 100.0 }, { 0.0, -0.6, 0.8 }), Against = Glow({ 0.0, 0.0, 0.0 }, { 0.0, 60.0, 80.0 }, { 0.0, 0.6, 0.8 });
		const double Behind = Glow({ 0.0, 0.0, 300.0 }, { 0.0, 100.0, 100.0 }, { 0.0, -0.6, 0.8 }), Night = Glow({ 0.0, 0.0, 0.0 }, { 0.0, 60.0, 80.0 }, { 0.0, 0.6, -0.1 });
		std::printf("     glow: seen from under it %.2f, against the sun %.2f, the sun behind the eye %.2f, at night %.2f\n", Below, Against, Behind, Night);
		Check(Below > 0.05 && Below < 0.3 && Against > 0.8 && Behind < 1e-9 && Night == 0.0, "chutes: his canopy glows against the sun, a little seen from under it, not with the sun behind the eye nor at night (her sixth)");

		// The scene's chute mesh at 45 frames a second (her sixth note, and a review of 24 November: it had been made
		// again 84 to 101 times a pilot, 54 of them as his canopy fell in). LandAir's steps as the scene takes them: a
		// section made again when its make changes (him alone; him, his lines and silk; down, with him going over; once
		// the crowd's man has him), and down its silk each time it moves; else moved in place. With the crowd posed
		// before the chutes as in the scene (a ride set one frame shows the next), so that the swap shows him in one of
		// the two every frame, never both and never neither.
		int MadeMost = 0, AirMade = 0, Gaps = 0, Twice = 0;
		for (int Field = 0; Field < 2; ++Field)
		{
			struct Kept
			{
				double Shape = -1.0, Lying = -9.0;
				int Make0 = -1, Make1 = -1, Made = 0, Moved = 0, SlumpMade = 0, MadeAloft = 0;
				bool bRide = false;
			};
			std::map<int, Kept> Ks;
			std::vector<Chute> Now;
			for (double Tf = T0; Tf <= 400.0; Tf += 1.0 / (45.0 * Life))
			{
				(Field ? S2 : S).Chutes(Tf, Now);
				for (const Chute& C : Now)
				{
					Kept& K = Ks[C.Key];
					const bool bCrowd = K.bRide;
					const double Shape = C.bDown ? 2.0 + C.Slump : C.Open;
					const double Lying = C.bDown && (C.bInMesh || !K.bRide) ? (C.bInMesh ? C.Fallen : 1.0) : -1.0;
					const bool bSilk = SilkMoved(Shape, K.Shape), bMan = ManMoved(Lying, K.Lying);
					if (bSilk || bMan)
					{
						const double SilkAt = bSilk ? Shape : K.Shape;
						const int Make0 = C.bDown ? (Lying >= 0.0 ? 2 : 3) : (SilkAt > 0.0 ? 1 : 0), Make1 = C.bDown || SilkAt > 0.0 ? 1 : 0;
						const bool bMade = Make0 != K.Make0 || (bSilk && (Make1 != K.Make1 || (Make1 == 1 && C.bDown)));
						K.Made += bMade;
						K.Moved += !bMade;
						K.SlumpMade += bMade && C.bDown && C.Slump > 0.0 && C.Slump < 1.0;
						K.MadeAloft += bMade && !C.bDown && K.Make1 == 1;
						K.Make0 = Make0;
						if (bSilk) { K.Make1 = Make1; K.Shape = Shape; }
						if (bMan) K.Lying = Lying;
					}
					if (C.bDown)
					{
						const bool bMesh = K.Lying >= 0.0;
						Gaps += !bMesh && !bCrowd;
						Twice += bMesh && bCrowd;
					}
					if (!C.bInMesh) K.bRide = true;
				}
			}
			for (const auto& Each : Ks)
			{
				std::printf("     %s pilot of plane %d's mesh at 45 fps: made %d times (%d as his canopy falls in), moved in place %d\n", Field ? "windy:" : "calm: ", Each.first,
					Each.second.Made, Each.second.SlumpMade, Each.second.Moved);
				MadeMost = std::max(MadeMost, Each.second.Made);
				AirMade += Each.second.MadeAloft;
			}
		}
		Check(MadeMost <= 30 && AirMade == 0, "chutes: his mesh made again at most 30 times at 45 fps, never in the air once his canopy is out (moved in place as it fills), down each 0.05 of its falling in (her sixth)");
		Check(Gaps == 0 && Twice == 0, "chutes: at the swap he is drawn every frame, in his chute's mesh or as the crowd's man, never both (the crowd posed before the chutes)");
		// (moved in place, a triangle keeps the way round it was first wound, which the scene chooses by where its points
		// are: so a canopy filling must never fold one over; down it is made again, its rumple folding some)
		auto Windings = [](const SilkMesh& M, std::vector<int>& Into)
		{
			Into.clear();
			for (size_t K = 0; K + 2 < M.Triangles.size(); K += 3)
			{
				const V3& A = M.Points[static_cast<size_t>(M.Triangles[K])], &B = M.Points[static_cast<size_t>(M.Triangles[K + 1])], &Cn = M.Points[static_cast<size_t>(M.Triangles[K + 2])];
				const V3 Ux = Plus(B, Mul(Cn, -1.0)), Vx = Plus(A, Mul(Cn, -1.0));
				const V3 Nx{ Ux.Y * Vx.Z - Ux.Z * Vx.Y, Ux.Z * Vx.X - Ux.X * Vx.Z, Ux.X * Vx.Y - Ux.Y * Vx.X };
				const V3& O = M.Outward[K / 3];
				Into.push_back(Nx.X * O.X + Nx.Y * O.Y + Nx.Z * O.Z >= 0.0 ? 1 : -1);
			}
		};
		int Folded = 0, HeapFolded = 0, LinesBad = 0;
		for (const auto& Each : All)
		{
			const Chute& Lk = Each.second->Look;
			SilkMesh First, M;
			std::vector<int> W0, W1;
			CanopyShape(Lk, 0.001, First);
			Windings(First, W0);
			for (double Open = 0.01; Open <= 1.0 + 1e-9; Open += 0.01)
			{
				CanopyShape(Lk, Open, M);
				Windings(M, W1);
				for (size_t K = 0; K < W0.size() && K < W1.size(); ++K) Folded += W0[K] != W1[K];
			}
			// (down: the lines back to him, by their place round the skirt, behind its middle all the while it falls in)
			Chute Dn = Lk;
			Dn.Open = 1.0;
			int Back = 0;
			for (size_t K = 0; K < static_cast<size_t>(Lk.Gores); ++K) Back += LineBack(K, static_cast<size_t>(Lk.Gores));
			LinesBad += Back < Lk.Gores / 2 - 2 || Back > Lk.Gores / 2;
			for (double Sl = 0.0; Sl <= 1.0 + 1e-9; Sl += 0.05)
			{
				HeapShape(Dn, Sl, 4.0, M);
				double Mid = 0.0;
				for (const int Sm : M.Seams) Mid += M.Points[static_cast<size_t>(Sm)].X / static_cast<double>(M.Seams.size());
				for (size_t K = 0; K < M.Seams.size(); ++K) LinesBad += LineBack(K, M.Seams.size()) && M.Points[static_cast<size_t>(M.Seams[K])].X >= Mid;
				if (Sl > 0.99)
				{
					Windings(M, W1);
					for (size_t K = 0; K < W0.size() && K < W1.size(); ++K) HeapFolded += W0[K] != W1[K];
				}
			}
		}
		std::printf("     moved in place: a canopy filling folds %d triangles over (of its windings as first made); lying flat, its rumple %d\n", Folded, HeapFolded);
		Check(Folded == 0 && HeapFolded > 0 && LinesBad == 0, "chutes: a canopy filling never folds a triangle over (moved in place); down its lines run back to him from the same half of its skirt all the while it falls in");

		// The swap's lying pose (her third note; a review of 24 November): the crowd's man is a rider in go_prone held at
		// its end, nothing taken back, so his hips are its end offset forward and right of his feet, and as low as its
		// pelvis track says; the mesh's man flat must lie there. Every soldier file that has them, read as the game reads
		// them (its go_prone's end_offset, its pelvis's rest).
		{
			std::string Dir = std::getenv("LAND_AIR_SOLDIERS") ? std::getenv("LAND_AIR_SOLDIERS") : "";
			if (Dir.empty())
			{
				const std::string Here = __FILE__;
				Dir = Here.substr(0, Here.find_last_of('/')) + "/../../Content/Models/Soldiers";
			}
			std::vector<std::string> Files;
			std::error_code Why;
			for (const std::filesystem::directory_entry& E : std::filesystem::directory_iterator(Dir, Why))
			{
				const std::string Name = E.path().filename().string();
				if (Name.rfind("soldier_", 0) == 0 && Name.size() > 5 && Name.substr(Name.size() - 5) == ".json") Files.push_back(E.path().string());
			}
			std::sort(Files.begin(), Files.end());
			int Read = 0, Offsets = 0, Off = 0, OwnOff = 0;
			double Most = 0.0, OwnMost = 0.0, WasShort = 1e9;
			for (const std::string& Path : Files)
			{
				std::string Text, Why2;
				LandJson::Json J;
				if (!LandJson::ReadFile(Path, Text) || !LandJson::Parse(Text, J, Why2)) continue;
				++Read;
				const std::vector<double> End = LandJson::Numbers(J["clips"]["go_prone"]["end_offset"]);
				double PelvisX = 0.0, PelvisZ = -1.0;
				const LandJson::Json& Bones = J["bones"];
				for (size_t K = 0; K < Bones.Count(); ++K)
				{
					if (Bones.At(K)["name"].Str("") != "pelvis") continue;
					const std::vector<double> Head = LandJson::Numbers(Bones.At(K)["head"]);
					if (Head.size() >= 3) { PelvisX = Head[0]; PelvisZ = Head[2]; }
				}
				if (End.size() < 3 || PelvisZ < 0.0) continue;
				++Offsets;
				// (where his hips lie in the file: forward, right and up of his seat)
				const V3 Lies{ PelvisX + End[0], End[1], PelvisZ + End[2] };
				const V3 ByDefault = OverHips(1.0, ProneEndX, ProneEndY), ByOwn = OverHips(1.0, End[0], End[1]);
				const double Miss = Length(Plus(ByDefault, Mul(Lies, -1.0))), OwnMiss = Length(Plus(ByOwn, Mul(Lies, -1.0)));
				Most = std::max(Most, Miss);
				OwnMost = std::max(OwnMost, OwnMiss);
				Off += Miss > 0.03;
				OwnOff += OwnMiss > 0.02;
				// (as it was: pitched 84 degrees and moved back 0.9 m, his hips 0.1 m behind his feet)
				WasShort = std::min(WasShort, Lies.X - (0.8 * std::sin(84.0 * 3.14159265358979323846 / 180.0) - 0.9));
			}
			const V3 Standing = OverHips(0.0, ProneEndX, ProneEndY);
			std::printf("     the swap's lying pose: %d soldier files read, %d with go_prone's end offset; the mesh's man flat %.3f m from the crowd's man's hips at most by his own file's offset, %.3f by LandAir's (he had lain %.2f m short)\n",
				Read, Offsets, OwnMost, Most, WasShort);
			Check(Offsets >= 10 && Off == 0 && OwnOff == 0 && Length(Plus(Standing, Mul(V3{ 0.0, 0.0, OverHip }, -1.0))) < 1e-12,
				"chutes: flat in his chute's mesh his hips lie where the crowd's man's do in every soldier file (go_prone's end offset, and its pelvis as it lies), on his feet as he lands (her third: no pop)");
		}
		if (Out.is_open())
		{
			// (his canopy for chutes.py's look: the first pilot's, full, and on the field fallen half in)
			std::ofstream Silk(std::string(Argv[1]) + "/chute_silk.txt");
			const Watched& W = Sn.begin()->second;
			for (int Which = 0; Which < 2; ++Which)
			{
				SilkMesh M;
				if (Which == 0) CanopyShape(W.Look, 1.0, M);
				else HeapShape(W.Look, 1.0, 4.0, M);
				Silk << "mesh " << Which << " " << W.Look.Gores << "\n";
				for (size_t K = 0; K < M.Points.size(); ++K)
					Silk << "p " << M.Points[K].X << " " << M.Points[K].Y << " " << M.Points[K].Z << " " << M.Facings[K].X << " " << M.Facings[K].Y << " " << M.Facings[K].Z << " " << M.Colours[K * 3] << " " << M.Colours[K * 3 + 1] << " " << M.Colours[K * 3 + 2] << "\n";
				for (size_t K = 0; K + 2 < M.Triangles.size(); K += 3) Silk << "t " << M.Triangles[K] << " " << M.Triangles[K + 1] << " " << M.Triangles[K + 2] << "\n";
			}
		}
	}
	std::printf("land_air_test: %s\n", Failed ? "FAILED" : "all passed");
	return Failed ? 1 : 0;
}
