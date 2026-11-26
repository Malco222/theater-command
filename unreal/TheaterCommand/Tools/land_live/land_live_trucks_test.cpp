// Men in lorries' cloud test (release 1.5, the Battle rules card's row "trucks"; Brian, 23 November 14:10Z: "troops are
// stuck under the truck"): a battle read whole (whole.json, as Tools/land_live/trucks-deltas.ts writes it with
// LAND_LIVE_TRUCKS=1) played with the game's own vehicles (LandKit::Battle, her tilt stowed: bOpenLorries) and men
// (LandMen::Crowd), each lorried man the rules seat shown as the scene shows him (Crowd::AboardOf, AddRiderLike,
// LandBench::RideFor, her tailboard by Vehicle::OpenBoard), 20 frames a battle second. Every frame: the man himself never
// drawn while aboard; every rider seated, rising or walking with his pelvis in her bed (inside her box, over her floor);
// never two riders on one place; and as each man gets down, his rider's last place against where the man is first drawn
// (the hand-over). Her tilt is hidden and her tailboard down while her men get down. Prints the counts.
// With a battle fought with the row off (no seats said), checks no rider is ever made.
// Usage: land_live_trucks_test <Content folder> <folder with whole.json> [seconds] [off]
#include "Battle/LandBench.h"
#include "Battle/LandKit.h"
#include "Battle/LandMen.h"
#include "Battle/Fates.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace
{
	bool ReadFile(const std::string& Path, std::string& Out)
	{
		std::ifstream F(Path, std::ios::binary);
		if (!F) return false;
		std::stringstream S;
		S << F.rdbuf();
		Out = S.str();
		return true;
	}

	/** The rolling ground both the vehicles and the men stand on (the kit test's hills). */
	double HillZ(double X, double Y) { return 1.5 * std::sin(X / 45.0) * std::cos(Y / 60.0) + 0.6 * std::sin((X + Y) / 23.0); }
	LandKit::V3 HillNormal(double X, double Y)
	{
		const double DX = 1.5 / 45.0 * std::cos(X / 45.0) * std::cos(Y / 60.0) + 0.6 / 23.0 * std::cos((X + Y) / 23.0);
		const double DY = -1.5 / 60.0 * std::sin(X / 45.0) * std::sin(Y / 60.0) + 0.6 / 23.0 * std::cos((X + Y) / 23.0);
		return LandKit::V3(-DX, -DY, 1.0).Unit();
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: land_live_trucks_test <Content folder> <folder with whole.json> [seconds] [off]\n");
		return 2;
	}
	const std::string Content = Argv[1];
	const std::string Folder = Argv[2];
	const double Upto = Argc > 3 ? std::atof(Argv[3]) : 1e9;
	const bool bOffRun = Argc > 4 && std::strcmp(Argv[4], "off") == 0;
	std::string WholeText, FatesText, ArmiesText, Error;
	if (!ReadFile(Folder + "/whole.json", WholeText) || !ReadFile(Content + "/Data/LandBattles/armies.json", ArmiesText))
	{
		std::printf("FAIL cannot read %s/whole.json or armies.json\n", Folder.c_str());
		return 2;
	}
	int Failures = 0;
	auto Fail = [&](const std::string& What) {
		if (++Failures <= 25) std::printf("  FAIL: %s\n", What.c_str());
	};
	const double Life = 2.0;

	// ---- The vehicles, the lorries' tilts stowed.
	Fates::Table Table;
	const bool bTable = ReadFile(Content + "/Data/Battle/fates.json", FatesText) && Table.Load(FatesText, Error);
	const LandKit::Battle::ReadFn Read = [&Content](const std::string& File, std::string& Out) { return ReadFile(Content + "/Models/Battle/" + File, Out); };
	const LandKit::GroundFn Hills = [](double X, double Y, double& OutZ, LandKit::V3& OutNormal)
	{
		OutZ = HillZ(X, Y);
		OutNormal = HillNormal(X, Y);
	};
	LandKit::Battle Kit;
	Kit.Life = Life;
	Kit.bOpenLorries = true;
	if (!Kit.Load(WholeText, Read, Hills, bTable ? &Table : nullptr, 1, Error))
	{
		std::printf("FAIL the vehicles: %s\n", Error.c_str());
		return 1;
	}
	int Tilts = 0, TiltsShown = 0;
	for (const auto& [Key, M] : Kit.Models())
	{
		for (const LandKit::Part& P : M.Parts)
		{
			if (P.Name != "tilt") continue;
			++Tilts;
			TiltsShown += P.bShown ? 1 : 0;
		}
	}
	if (Tilts == 0 && !bOffRun) Fail("no lorry model with a tilt in this battle");
	if (TiltsShown > 0) Fail(std::to_string(TiltsShown) + " lorry models with their tilt still up (Men in lorries stows it)");

	// ---- The men: every soldier file there is (a missing army stands in as the game's do).
	LandMen::Crowd Men;
	Men.SetLife(Life);
	for (const char* Army : { "CHN", "FRA", "GER", "ITA", "ITA_desert", "JAP", "POL", "SOV", "SOV_snow", "SOV_winter", "UK", "USA", "USA_bar", "USA_bazooka", "USA_loader", "USA_nco", "USA_radio" })
	{
		std::string Text;
		if (!ReadFile(Content + "/Models/Soldiers/soldier_" + std::string(Army) + ".json", Text)) continue;
		auto S = std::make_shared<LandMen::Soldier>();
		if (!S->Load(Text, Error)) { std::printf("FAIL soldier_%s.json: %s\n", Army, Error.c_str()); return 1; }
		Men.AddSoldier(Army, S);
	}
	if (!Men.ReadArmies(ArmiesText, Error)) { std::printf("FAIL armies.json: %s\n", Error.c_str()); return 1; }
	Men.SetThrift(false);
	const LandMen::GroundFn GroundAt = [](double X, double Y)
	{
		LandMen::Ground G;
		const LandKit::V3 N = HillNormal(X, Y);
		G.Height = static_cast<float>(HillZ(X, Y));
		G.Normal = { static_cast<float>(N.X), static_cast<float>(N.Y), static_cast<float>(N.Z) };
		return G;
	};
	if (!Men.Build(WholeText, GroundAt, Error)) { std::printf("FAIL the men: %s\n", Error.c_str()); return 1; }
	Men.SetEye(0.0, 0.0, 0.0);
	const int Recorded = Men.Count();
	int Lorries = 0;
	for (const LandKit::Vehicle& V : Kit.Vehicles()) Lorries += V.Words().Role == "lorry" && V.Look().SocketIndex("rider.1") >= 0;
	std::printf("land_live_trucks_test: %s: %zu vehicles (%d lorries with benches), %d men%s\n", Folder.c_str(), Kit.Vehicles().size(), Lorries, Recorded, bOffRun ? " (the row off)" : "");

	// ---- Play, 20 frames a battle second, as the scene's RideLorries does each frame.
	double End = 0.0;
	{
		const size_t At = WholeText.rfind("\"end\":");
		End = At == std::string::npos ? 300.0 : std::strtod(WholeText.c_str() + At + 6, nullptr);
	}
	End = std::min(End, Upto);
	std::vector<int> RiderOf(static_cast<size_t>(Recorded), -1);
	std::vector<char> WasDown(static_cast<size_t>(Recorded), 0);
	std::vector<LandMen::Vec3> LastRiderPelvis(static_cast<size_t>(Recorded));
	std::vector<double> LastDown(static_cast<size_t>(Recorded), -1.0);
	std::vector<int> LastLorry(static_cast<size_t>(Recorded), -1);
	long long Aboard = 0, Shown = 0, Seated = 0, Rising = 0, Walking = 0, Out = 0, OffBed = 0, Doubled = 0, SelfDrawn = 0, Frames = 0;
	long long Handed = 0, HandedFar = 0, Boards = 0, BoardDown = 0, WrongBench = 0;
	double HandSum = 0.0, HandMost = 0.0;
	for (double T = 0.0; T <= End + 1e-9; T += 0.05)
	{
		Kit.Update(T);
		std::vector<LandKit::Vehicle>& All = Kit.Vehicles();
		std::map<std::pair<int, int>, int> LorryOf;
		for (size_t V = 0; V < All.size(); ++V)
		{
			if (All[V].Words().Role == "lorry" && All[V].Look().SocketIndex("rider.1") >= 0 && All[V].Look().SocketIndex("step") >= 0)
				LorryOf[{ All[V].Words().Block, All[V].Words().K }] = static_cast<int>(V);
		}
		std::vector<int> Used;
		std::vector<std::pair<int, int>> Riding;   // (rider, her vehicle) shown this frame, and how
		std::vector<LandBench::Step> Steps;
		std::vector<std::tuple<int, double, int>> Whose;   // (the man, his Down, his seat) of each rider shown
		std::map<int, int> DownOf;                 // her vehicle -> men getting down now
		for (int Man = 0; Man < Recorded; ++Man)
		{
			int Block = -1, SeatOn = -1, Seat = -1;
			double Down = -1.0;
			int& Rider = RiderOf[static_cast<size_t>(Man)];
			const bool bAboard = Men.AboardOf(Man, Block, SeatOn, Seat, Down);
			const auto Her = bAboard ? LorryOf.find({ Block, SeatOn }) : LorryOf.end();
			if (Her == LorryOf.end() || LandBench::SocketOf(Seat) <= 0)
			{
				if (Rider >= 0) Men.SetRide(Rider, LandMen::Crowd::Ride());
				continue;
			}
			++Aboard;
			LandKit::Vehicle& V = All[static_cast<size_t>(Her->second)];
			if (Down >= 0.0)
			{
				V.OpenBoard(T - Down);
				++DownOf[Her->second];
			}
			if (Rider < 0) Rider = Men.AddRiderLike(Man);
			if (Rider < 0) { Fail("a rider could not be made like man " + std::to_string(Man)); continue; }
			LandMen::Crowd::Ride R;
			if (!LandBench::RideFor(V, *Men.SoldierOf(Rider), Seat, Down, T, Life, Man, R))
			{
				Fail("no ride for man " + std::to_string(Man) + " on seat " + std::to_string(Seat));
				Men.SetRide(Rider, LandMen::Crowd::Ride());
				continue;
			}
			Men.SetRide(Rider, R);
			Riding.emplace_back(Rider, Her->second);
			const LandMen::Soldier* Sol = Men.SoldierOf(Rider);
			const double OutLong = Sol->Clips[static_cast<size_t>(R.Clip)].Long;
			(void)OutLong;
			// (which step he is on, as RideFor played it)
			LandBench::Step St = LandBench::Step::Seated;
			const std::string& ClipName = Sol->Clips[static_cast<size_t>(R.Clip)].Name;
			if (ClipName.rfind("bench_rise", 0) == 0) St = LandBench::Step::Rise;
			else if (ClipName == "walk") St = LandBench::Step::Walk;
			else if (ClipName.rfind("out_", 0) == 0) St = LandBench::Step::Out;
			Steps.push_back(St);
			Whose.emplace_back(Man, Down, Seat);
			LastDown[static_cast<size_t>(Man)] = Down;
			LastLorry[static_cast<size_t>(Man)] = Her->second;
			WasDown[static_cast<size_t>(Man)] = Down >= 0.0;
		}
		Men.Update(T);
		++Frames;
		// (her tailboard: down while her men get down)
		for (const auto& [V, N] : DownOf)
		{
			(void)N;
			++Boards;
			// (her tailboard's body turned down off her up: its up against hers)
			const LandKit::Vehicle& Ve = All[static_cast<size_t>(V)];
			const std::vector<LandKit::Body>& Bs = Ve.Look().Bodies;
			for (size_t I = 0; I < Bs.size() && I < Ve.Bodies().size(); ++I)
			{
				if (Bs[I].Name != "tailboard") continue;
				const double Dot = Ve.Bodies()[I].Way(LandKit::V3(0.0, 0.0, 1.0)).Dot(Ve.Root.Way(LandKit::V3(0.0, 0.0, 1.0)));
				BoardDown += Dot < 0.8;
				break;
			}
		}
		// Every rider shown: in her bed while seated, rising or walking; none on another's place.
		std::vector<std::tuple<LandMen::Vec3, int, int, size_t>> Places;   // (pelvis, her vehicle, the step, his Riding entry)
		for (size_t I = 0; I < Riding.size(); ++I)
		{
			const int Rider = Riding[I].first;
			const LandKit::Vehicle& V = All[static_cast<size_t>(Riding[I].second)];
			if (!Men.Visible(Rider)) continue;
			++Shown;
			const LandMen::Soldier* S = Men.SoldierOf(Rider);
			const int Pb = S->BoneIndex("pelvis");
			const LandMen::Vec3 Pv = Men.FramesOf(Rider)[Pb].Point(S->Bones[static_cast<size_t>(Pb)].Head);
			const LandBench::Step St = Steps[I];
			Seated += St == LandBench::Step::Seated;
			Rising += St == LandBench::Step::Rise;
			Walking += St == LandBench::Step::Walk;
			Out += St == LandBench::Step::Out;
			const double Fx = std::sin(V.Heading), Fy = std::cos(V.Heading);
			const double Dx = Pv.X - V.Root.At.X, Dy = Pv.Y - V.Root.At.Y;
			const double A = Dx * Fx + Dy * Fy, Rr = Dx * Fy - Dy * Fx;
			const double High = Pv.Z - V.Root.At.Z;
			// (the drill's even seats on her left bench, its odd on her right: as it sets each man down, to his bench's side)
			if (St == LandBench::Step::Seated && (std::get<2>(Whose[I]) % 2 == 0) != (Rr < 0.0) && ++WrongBench <= 3)
			{
				Fail("t " + std::to_string(T) + " man " + std::to_string(std::get<0>(Whose[I])) + " on seat " + std::to_string(std::get<2>(Whose[I])) + " seated on her other bench (" + std::to_string(Rr) + " across)");
			}
			if (St != LandBench::Step::Out && !(A > -3.7 && A < 0.3 && std::fabs(Rr) < 1.15 && High > 1.3))
			{
				if (++OffBed <= 5)
				{
					char Buf[220];
					std::snprintf(Buf, sizeof(Buf), "t %.2f rider %d (step %d) off her bed: %.2f along, %.2f across, pelvis %.2f m over her root", T, Rider, static_cast<int>(St), A, Rr, High);
					Fail(Buf);
				}
			}
			for (const auto& [O, Ov, Os, Oi] : Places)
			{
				if (Ov == Riding[I].second && std::hypot(O.X - Pv.X, O.Y - Pv.Y) < 0.3 && std::fabs(O.Z - Pv.Z) < 0.4)
				{
					if (++Doubled <= 3)
					{
						char Buf[200];
						std::snprintf(Buf, sizeof(Buf), "t %.2f rider %d (man %d, down %.2f s, step %d, %.2f along her, %.2f across) on another rider's place (man %d, down %.2f s, step %d) in his own lorry", T, Rider, std::get<0>(Whose[I]), std::get<1>(Whose[I]), static_cast<int>(St), A, Rr, std::get<0>(Whose[Oi]), std::get<1>(Whose[Oi]), Os);
						Fail(Buf);
					}
					break;
				}
			}
			Places.emplace_back(Pv, Riding[I].second, static_cast<int>(St), I);
		}
		// The men themselves: never drawn aboard; as one gets down, where he is first drawn against his rider's last place.
		for (int Man = 0; Man < Recorded; ++Man)
		{
			int B = -1, K = -1, Seat = -1;
			double Down = -1.0;
			const bool bAboard = Men.AboardOf(Man, B, K, Seat, Down);
			const int Rider = RiderOf[static_cast<size_t>(Man)];
			if (bAboard && LandBench::SocketOf(Seat) > 0)
			{
				SelfDrawn += Men.Visible(Man);
				if (Rider >= 0 && Men.Visible(Rider))
				{
					const LandMen::Soldier* S = Men.SoldierOf(Rider);
					const int Pb = S->BoneIndex("pelvis");
					LastRiderPelvis[static_cast<size_t>(Man)] = Men.FramesOf(Rider)[Pb].Point(S->Bones[static_cast<size_t>(Pb)].Head);
				}
				continue;
			}
			if (WasDown[static_cast<size_t>(Man)] && Rider >= 0 && Men.Visible(Man))
			{
				WasDown[static_cast<size_t>(Man)] = 0;
				const LandMen::Soldier* S = Men.SoldierOf(Man);
				const int Pb = S->BoneIndex("pelvis");
				const LandMen::Vec3 Pv = Men.FramesOf(Man)[Pb].Point(S->Bones[static_cast<size_t>(Pb)].Head);
				const LandMen::Vec3& Q = LastRiderPelvis[static_cast<size_t>(Man)];
				const double D = std::hypot(Pv.X - Q.X, Pv.Y - Q.Y);
				++Handed;
				HandSum += D;
				HandMost = std::max(HandMost, D);
				if (D > 1.5)
				{
					++HandedFar;
					if (HandedFar <= 3)
					{
						char Buf[300];
						const LandKit::Vehicle& Lv = Kit.Vehicles()[static_cast<size_t>(LastLorry[static_cast<size_t>(Man)])];
						const double Fx = std::sin(Lv.Heading), Fy = std::cos(Lv.Heading);
						auto Along = [&](const LandMen::Vec3& P) { return (P.X - Lv.Root.At.X) * Fx + (P.Y - Lv.Root.At.Y) * Fy; };
						auto Across = [&](const LandMen::Vec3& P) { return (P.X - Lv.Root.At.X) * Fy - (P.Y - Lv.Root.At.Y) * Fx; };
						std::snprintf(Buf, sizeof(Buf), "t %.2f man %d drawn %.2f m from where his rider left him (down for %.2f s): rider %.2f along %.2f across, man %.2f along %.2f across her", T, Man, D, LastDown[static_cast<size_t>(Man)], Along(Q), Across(Q), Along(Pv), Across(Pv));
						std::printf("  note: %s\n", Buf);
						std::printf("        her root %.2f %.2f heading %.3f; man pelvis %.2f %.2f; rider pelvis %.2f %.2f\n", Lv.Root.At.X, Lv.Root.At.Y, Lv.Heading, Pv.X, Pv.Y, Q.X, Q.Y);
					}
				}
			}
			else if (!Men.Visible(Man) || !bAboard)
			{
				// (a man got down out of the eye's reach, or fell aboard: no hand-over to measure)
				if (!Men.Visible(Man)) continue;
				WasDown[static_cast<size_t>(Man)] = 0;
			}
		}
	}
	std::printf("  %lld frames to %.0f s: %lld man-frames aboard with a seat, %lld riders shown (%lld seated, %lld rising, %lld walking her floor, %lld going down); %lld off her bed, %lld on another's place, %lld men drawn themselves aboard, %lld on the other bench\n",
		Frames, End, Aboard, Shown, Seated, Rising, Walking, Out, OffBed, Doubled, SelfDrawn, WrongBench);
	std::printf("  %lld hand-overs: the man first drawn %.2f m from his rider's last place on average, %.2f the most, %lld past 1.5 m; tailboard down in %lld of %lld lorry-frames with men getting down; %d riders made\n",
		Handed, Handed ? HandSum / static_cast<double>(Handed) : 0.0, HandMost, HandedFar, BoardDown, Boards, Men.Count() - Recorded);
	if (bOffRun)
	{
		if (Men.Count() != Recorded) Fail("riders made with the row off");
	}
	else
	{
		if (Aboard == 0) Fail("no man was ever aboard with a seat (no lorried men in this battle, or the row off when fought)");
		if (Out == 0 || Rising == 0) Fail("no rider was ever seen getting down");
		if (OffBed > 0) Fail(std::to_string(OffBed) + " rider-frames off her bed");
		if (Doubled > 0) Fail(std::to_string(Doubled) + " rider-frames on another rider's place");
		if (WrongBench > 0) Fail(std::to_string(WrongBench) + " rider-frames seated on the other bench from the drill's");
		if (SelfDrawn > 0) Fail(std::to_string(SelfDrawn) + " man-frames drawn themselves while aboard");
		if (Handed > 0 && HandedFar * 20 > Handed) Fail(std::to_string(HandedFar) + " of " + std::to_string(Handed) + " hand-overs past 1.5 m");
		if (Boards > 0 && BoardDown * 10 < Boards * 8) Fail("the tailboard down in only " + std::to_string(BoardDown) + " of " + std::to_string(Boards) + " lorry-frames with men getting down");
	}
	std::printf(Failures ? "FAILED (%d)\n" : "ALL CHECKS PASSED\n", Failures);
	return Failures ? 1 : 0;
}
