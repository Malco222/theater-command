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
//            frames fall; no NaN
// With an out folder it writes air_paths.txt for paths.py (the picture: each strike side-on and from above).
// Usage: Tools/land_air/run.sh [out folder]
#include "Battle/LandAir.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
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

	/** The recording: a 2 km field, the attacker's blocks to the south, the defender's to the north, and the strikes. */
	std::string Recording(bool bAll, size_t Upto = 100, bool bOwnOnStrip = false)
	{
		std::ostringstream R;
		R << "{\"key\": \"air-test\", \"date\": \"July 1944\", \"field\": {\"w\": 2000, \"h\": 2000}, \"men\": [], \"views\": [";
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
		for (const Strike& S : Strikes)
		{
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
				   << ", \"extra\": {\"flight\": " << S.Flight << ", \"kind\": \"" << S.Lost << "\", \"chutes\": 0, \"over\": \"enemy\", \"air\": \"ftr\", \"alt\": 60}, \"cue\": \"plane_down\"}";
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
	std::printf("land_air_test: %s\n", Failed ? "FAILED" : "all passed");
	return Failed ? 1 : 0;
}
