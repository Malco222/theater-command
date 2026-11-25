// The lorried men's seats' cloud test (release 1.5 and 2; Brian, 23 November 14:10Z: "troops are stuck under the truck"):
// a battle read whole (whole.json, as Tools/land_live/run.sh or LAND_LIVE_TICKET writes it) played with the game's own
// vehicles (LandKit::Battle) and men (LandMen::Crowd), the carriers' benches handed to the men each frame as the scene
// hands them (LandKit::BenchesNow, BenchFor, Crowd::SetSeats). Every frame, every man aboard (Crowd::AboardOf, seated
// or kneeling where his file has no seated clip): his pelvis in a carrier's bed (inside her box, over her floor),
// never two men on one seat, and none left on the ground under her. The carriers' axes are checked against their
// headings. Prints how many man-frames were aboard and how they sat.
// Usage: land_live_seat_test <Content folder> <folder with whole.json> [seconds]
#include "Battle/LandKit.h"
#include "Battle/LandMen.h"
#include "Battle/Fates.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
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
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: land_live_seat_test <Content folder> <folder with whole.json> [seconds]\n");
		return 2;
	}
	const std::string Content = Argv[1];
	const std::string Folder = Argv[2];
	const double Upto = Argc > 3 ? std::atof(Argv[3]) : 1e9;
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

	// ---- The vehicles.
	Fates::Table Table;
	const bool bTable = ReadFile(Content + "/Data/Battle/fates.json", FatesText) && Table.Load(FatesText, Error);
	const LandKit::Battle::ReadFn Read = [&Content](const std::string& File, std::string& Out) { return ReadFile(Content + "/Models/Battle/" + File, Out); };
	const LandKit::GroundFn Hills = [](double X, double Y, double& OutZ, LandKit::V3& OutNormal)
	{
		OutZ = HillZ(X, Y);
		const double DX = 1.5 / 45.0 * std::cos(X / 45.0) * std::cos(Y / 60.0) + 0.6 / 23.0 * std::cos((X + Y) / 23.0);
		const double DY = -1.5 / 60.0 * std::sin(X / 45.0) * std::sin(Y / 60.0) + 0.6 / 23.0 * std::cos((X + Y) / 23.0);
		OutNormal = LandKit::V3(-DX, -DY, 1.0).Unit();
	};
	LandKit::Battle Kit;
	Kit.Life = 2.0;
	if (!Kit.Load(WholeText, Read, Hills, bTable ? &Table : nullptr, 1, Error))
	{
		std::printf("FAIL the vehicles: %s\n", Error.c_str());
		return 1;
	}

	// ---- The men: every soldier file there is (a missing army stands in as the game's do).
	LandMen::Crowd Men;
	Men.SetLife(2.0);
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
		double Z = 0.0;
		LandKit::V3 N;
		const double DX = 1.5 / 45.0 * std::cos(X / 45.0) * std::cos(Y / 60.0) + 0.6 / 23.0 * std::cos((X + Y) / 23.0);
		const double DY = -1.5 / 60.0 * std::sin(X / 45.0) * std::sin(Y / 60.0) + 0.6 / 23.0 * std::cos((X + Y) / 23.0);
		Z = HillZ(X, Y);
		N = LandKit::V3(-DX, -DY, 1.0).Unit();
		G.Height = static_cast<float>(Z);
		G.Normal = { static_cast<float>(N.X), static_cast<float>(N.Y), static_cast<float>(N.Z) };
		return G;
	};
	if (!Men.Build(WholeText, GroundAt, Error)) { std::printf("FAIL the men: %s\n", Error.c_str()); return 1; }
	std::vector<LandKit::Benches> Carriers;
	Men.SetSeats([&Carriers](double X, double Y, int Block, int SeatOn, int Seat, LandMen::SeatPlace& Out)
	{
		LandKit::BenchSeat B;
		if (Carriers.empty() || !LandKit::BenchFor(Carriers, X, Y, Block, SeatOn, Seat, B)) return false;
		Out.X = B.At.X;
		Out.Y = B.At.Y;
		Out.Z = B.At.Z;
		Out.Floor = B.Floor;
		Out.Facing = static_cast<float>(B.Heading);
		Out.bBench = B.bBench;
		return true;
	});
	Men.SetEye(0.0, 0.0, 0.0);
	int CarrierCount = 0;
	for (const LandKit::Vehicle& V : Kit.Vehicles()) CarrierCount += V.Look().Kind == "carrier" && V.Look().SocketIndex("rider.1") >= 0;
	std::printf("land_live_seat_test: %s: %zu vehicles (%d carriers with benches), %d men\n", Folder.c_str(), Kit.Vehicles().size(), CarrierCount, Men.Count());

	// ---- Play, 20 frames a battle second.
	double End = 0.0;
	{
		const size_t At = WholeText.rfind("\"end\":");
		End = At == std::string::npos ? 300.0 : std::strtod(WholeText.c_str() + At + 6, nullptr);
	}
	End = std::min(End, Upto);
	long long Aboard = 0, Benched = 0, OffBed = 0, Doubled = 0, Through = 0, Unsaid = 0, AxesOff = 0, Frames = 0;
	LandKit::Benches One;   // (the first carrier seen with her benches: the overfull lorry's check below)
	bool bHaveOne = false;
	double MostHigh = 0.0, LeastHigh = 1e9;
	for (double T = 0.0; T <= End + 1e-9; T += 0.05)
	{
		Kit.Update(T);
		Carriers.clear();
		for (const LandKit::Vehicle& V : Kit.Vehicles())
		{
			LandKit::Benches B;
			if (V.Fade < 0.5 || !LandKit::BenchesNow(V, B)) continue;
			// (her forward is her heading's way)
			const double Off = std::hypot(B.Forward.X - std::sin(V.Heading), B.Forward.Y - std::cos(V.Heading));
			if (Off > 0.15 && ++AxesOff <= 3) Fail("a carrier's forward is off her heading by " + std::to_string(Off));
			Carriers.push_back(B);
		}
		if (!bHaveOne && !Carriers.empty())
		{
			One = Carriers.front();
			bHaveOne = true;
		}
		Men.Update(T);
		++Frames;
		std::vector<std::pair<LandMen::Vec3, long long>> Seated;   // (each seated man's pelvis, and whose bed the rules have him in)
		for (int M = 0; M < Men.Count(); ++M)
		{
			int Block = -1, SeatOn = -1, Seat = -1;
			if (!Men.AboardOf(M, Block, SeatOn, Seat)) continue;
			if (SeatOn < 0 || Seat < 0) ++Unsaid;
			const long long Hers = static_cast<long long>(Block) * 100000 + SeatOn;
			const LandMen::Soldier* S = Men.SoldierOf(M);
			const int Pb = S ? S->BoneIndex("pelvis") : -1;
			if (Pb < 0) continue;
			const LandMen::Vec3 Pv = Men.FramesOf(M)[Pb].Point(S->Bones[static_cast<size_t>(Pb)].Head);
			++Aboard;
			// (in a carrier's bed: within her box, her middle line and her length, and over her floor)
			bool bIn = false;
			double Nearest = 1e9;
			for (size_t Ci = 0; Ci < Carriers.size(); ++Ci)
			{
				const LandKit::Benches& B = Carriers[Ci];
				const double Dx = Pv.X - B.At.X, Dy = Pv.Y - B.At.Y;
				const double A = Dx * B.Forward.X + Dy * B.Forward.Y, R = Dx * B.Right.X + Dy * B.Right.Y;
				const double High = Pv.Z - B.At.Z;
				if (A > -4.0 && A < 1.0 && std::fabs(R) < 1.2 && High > 0.6 && std::fabs(R) < Nearest)
				{
					if (!bIn)
					{
						MostHigh = std::max(MostHigh, High);
						LeastHigh = std::min(LeastHigh, High);
					}
					bIn = true;
					Nearest = std::fabs(R);
				}
			}
			if (!bIn)
			{
				++OffBed;
				if (OffBed <= 5)
				{
					char Buf[200];
					std::snprintf(Buf, sizeof(Buf), "t %.2f man %d seated off any carrier's bed: pelvis (%.1f, %.1f, %.2f), ground %.2f", T, M, Pv.X, Pv.Y, Pv.Z, HillZ(Pv.X, Pv.Y));
					Fail(Buf);
				}
				continue;
			}
			++Benched;
			for (const auto& [O, OHers] : Seated)
			{
				if (std::hypot(O.X - Pv.X, O.Y - Pv.Y) < 0.3 && std::fabs(O.Z - Pv.Z) < 0.4)
				{
					// (two carriers standing in one another: the vehicles' own overlap, counted apart)
					if (OHers != Hers) { ++Through; break; }
					if (++Doubled <= 3)
					{
						char Buf[200];
						std::snprintf(Buf, sizeof(Buf), "t %.2f man %d sits on another man of his own carrier (pelvises %.2f m apart)", T, M, std::hypot(O.X - Pv.X, O.Y - Pv.Y));
						Fail(Buf);
					}
					break;
				}
			}
			Seated.emplace_back(Pv, Hers);
		}
	}
	std::printf("  %lld frames to %.0f s: %lld man-frames seated aboard, %lld in a carrier's bed (pelvis %.2f to %.2f m over her root), %lld off any bed, %lld on another man in his own carrier; %lld where two carriers stand in one another (the vehicles' overlap)\n",
		Frames, End, Aboard, Benched, LeastHigh < 1e8 ? LeastHigh : 0.0, MostHigh, OffBed, Doubled, Through);
	// ---- An overfull lorry's last men (release 2, the scene's trial word `leave`: BenchFor's bFarAft; Brian, 25
	// November 17:11Z, "some soldiers freeze and then disapear": the read's item 5). By his place alone (no word of his
	// seat), the sixteen men the rules stand in a far block's lorry, a row each 1.1 m from 0.5 m forward of her root to
	// 7.2 m aft: every one has a place in her bed, between her first bench and her last, the kneeling rows in the order
	// they stand and no two rows on one spot; a man 9 m aft is on no bed. Off, as before: the last two rows (past 5 m
	// aft) have no seat, and every other man the seat he had.
	if (bHaveOne)
	{
		const std::vector<LandKit::Benches> Alone { One };
		double FrontAlong = -1e9, RearAlong = 1e9;
		for (int I = 0; I < One.Left + One.Righted; ++I)
		{
			const double A = (One.Seat[I] - One.At).Dot(One.Forward);
			FrontAlong = std::max(FrontAlong, A);
			RearAlong = std::min(RearAlong, A);
		}
		const auto PlaceOf = [&One](double Along, double Across, double& X, double& Y)
		{
			X = One.At.X + One.Forward.X * Along + One.Right.X * Across;
			Y = One.At.Y + One.Forward.Y * Along + One.Right.Y * Across;
		};
		int OnBench = 0, Kneeling = 0, NoSeat = 0, Outside = 0, Close = 0, OffWrong = 0, BenchMoved = 0;
		double LastKneel = 1e9, Tightest = 1e9;
		for (int N = 0; N < 8; ++N)
		{
			for (int Side = 0; Side < 2; ++Side)
			{
				double X = 0.0, Y = 0.0;
				PlaceOf(0.5 - 1.1 * N, Side ? 0.55 : -0.55, X, Y);
				LandKit::BenchSeat Seat, Before;
				const bool bBefore = LandKit::BenchFor(Alone, X, Y, -1, -1, -1, Before);
				if (bBefore != (N < 6)) ++OffWrong;
				if (!LandKit::BenchFor(Alone, X, Y, -1, -1, -1, Seat, true))
				{
					++NoSeat;
					continue;
				}
				// (a man on a bench sits where he sat)
				if (Seat.bBench && (!bBefore || !Before.bBench || std::hypot(Seat.At.X - Before.At.X, Seat.At.Y - Before.At.Y) > 1e-9)) ++BenchMoved;
				const double A = (Seat.At - One.At).Dot(One.Forward);
				if (A > FrontAlong + 0.05 || A < RearAlong - 0.05) ++Outside;
				if (Seat.bBench)
				{
					++OnBench;
					continue;
				}
				++Kneeling;
				if (Side == 0)
				{
					if (LastKneel < 1e8)
					{
						Tightest = std::min(Tightest, LastKneel - A);
						if (LastKneel - A < 0.15) ++Close;
					}
					LastKneel = A;
				}
			}
		}
		double X9 = 0.0, Y9 = 0.0;
		PlaceOf(-9.0, 0.0, X9, Y9);
		LandKit::BenchSeat Far;
		const bool bFar = LandKit::BenchFor(Alone, X9, Y9, -1, -1, -1, Far, true);
		std::printf("  an overfull lorry by place (sixteen men, a row each 1.1 m to 7.2 m aft of her root; her benches %.2f to %.2f m along her): %d on benches, %d kneeling down her bed, the kneeling rows at least %.2f m apart, %d with no seat, %d outside her benches' length; a man 9 m aft %s; off, as before: %s\n",
			RearAlong, FrontAlong, OnBench, Kneeling, Tightest < 1e8 ? Tightest : 0.0, NoSeat, Outside, bFar ? "IS seated" : "is on no bed",
			OffWrong == 0 && BenchMoved == 0 ? "the last four men have no seat, the benched men sit where they sat" : "NOT as before");
		if (OffWrong > 0) Fail(std::to_string(OffWrong) + " of the sixteen men are seated or not, off, otherwise than before (5 m aft)");
		if (BenchMoved > 0) Fail(std::to_string(BenchMoved) + " benched men of an overfull lorry sit elsewhere than they did");
		if (NoSeat > 0) Fail(std::to_string(NoSeat) + " of an overfull lorry's sixteen men have no seat by their place");
		if (Outside > 0) Fail(std::to_string(Outside) + " of an overfull lorry's men are put outside her benches' length");
		if (Close > 0) Fail(std::to_string(Close) + " kneeling rows of an overfull lorry within 0.15 m of the row before");
		if (bFar) Fail("a man 9 m aft of a lorry's root is seated in her");
	}
	if (Unsaid > 0) Fail(std::to_string(Unsaid) + " man-frames aboard without the rules' word for his vehicle and seat");
	if (Aboard == 0) Fail("no man was ever seated aboard (no lorried men in this battle, or none mounted)");
	if (OffBed > 0) Fail(std::to_string(OffBed) + " man-frames seated off any carrier's bed");
	if (Doubled > 0) Fail(std::to_string(Doubled) + " man-frames with two men on one seat");
	std::printf(Failures ? "FAILED (%d)\n" : "ALL CHECKS PASSED\n", Failures);
	return Failures ? 1 : 0;
}
