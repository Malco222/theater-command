// The live ground battle's vehicles (release 2): the game's own Battle/LandKit grown piece by piece as the rules fight
// (LandKit::Battle::Extend, from header.json and deltas.jsonl, as Tools/land_live/run.sh writes them) against the same
// battle read whole (whole.json, Battle::Load as before). Both are played at 60 frames a second (the battle's clock at
// half life's), the screen two battle seconds behind the latest piece (to the end and 8 s on once it is over), and
// compared every frame: the vehicles there, each one's place (within 0.25 m: a provisional point is 0.1 m from the
// rules' path), heading, turret and gun (from her bodies, within 2 degrees, or the provisional points' own slack of
// facing where that is more: the rules keep a point for 0.1 m of place but let her facing drift a few degrees between
// kept points, and the whole battle's line between them is not where the rules had her), whether she is knocked out,
// how (brew-up, the Fates look) and when, each frame's step of her place and heading (the grown battle's against the
// whole's: a piece that works her out again must not make her jump), and every event the effects and the sound are
// given (the same effect and cue, on the same vehicle, within a frame; one a step of the passes apart is counted, and
// so are a lorry's first move-off, a move-off or stop that a provisional point dated otherwise, and one of a turn on
// the spot of less than the rules' facing slack that only a provisional point showed, each named; one missing fails).
// A lorry is left out of the heading check until 3 s after she first moves: read whole, she faces her first way of
// going from the start, which a live battle cannot know (her worst is printed apart). Extend is timed: what a piece
// costs the game thread, at the battle's end too.
// Usage: land_live_kit_test <Content folder> <folder with header.json, deltas.jsonl, whole.json>

#include "Battle/Fates.h"
#include "Battle/LandKit.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace LandKit;

namespace
{
	using Clock = std::chrono::steady_clock;

	int Failures = 0;

	void Check(bool bOk, const std::string& What)
	{
		if (bOk)
		{
			return;
		}
		++Failures;
		if (Failures <= 30)
		{
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	bool ReadFile(const std::string& Path, std::string& Out)
	{
		std::ifstream F(Path, std::ios::binary);
		if (!F)
		{
			return false;
		}
		std::stringstream S;
		S << F.rdbuf();
		Out = S.str();
		return true;
	}

	double WrapDeg(double A)
	{
		A = std::fmod(A + 180.0, 360.0);
		if (A < 0.0)
		{
			A += 360.0;
		}
		return A - 180.0;
	}

	/** The angle between two turns, degrees. */
	double Apart(const Quat& A, const Quat& B)
	{
		const double Dot = std::fabs(A.W * B.W + A.X * B.X + A.Y * B.Y + A.Z * B.Z);
		return 2.0 * std::acos(std::min(1.0, Dot)) * 180.0 / 3.14159265358979323846;
	}

	/** The body a part is drawn in, or -1. */
	int BodyOf(const Model& M, int Part)
	{
		return Part >= 0 ? M.Parts[static_cast<size_t>(Part)].Body : -1;
	}

	/** Where a track's kept points put her at T (straight between them, as Vehicle::PlaceAt does). */
	void Along(const std::vector<double>& A, double T, double& X, double& Y, double& F)
	{
		const size_t N = A.size() / 4;
		size_t K = 0;
		while (K + 1 < N && A[(K + 1) * 4] <= T)
		{
			++K;
		}
		X = A[K * 4 + 1];
		Y = A[K * 4 + 2];
		F = A[K * 4 + 3];
		if (K + 1 < N && T > A[K * 4])
		{
			const double U = (T - A[K * 4]) / (A[(K + 1) * 4] - A[K * 4]);
			X += (A[(K + 1) * 4 + 1] - X) * U;
			Y += (A[(K + 1) * 4 + 2] - Y) * U;
			F += WrapDeg((A[(K + 1) * 4 + 3] - F) * 180.0 / 3.14159265358979323846) * 3.14159265358979323846 / 180.0 * U;
		}
	}

	std::string Who(const Vehicle& V)
	{
		char Buf[96];
		std::snprintf(Buf, sizeof(Buf), "%s %d/%d (%s)", V.Look().Name.c_str(), V.Words().Block, V.Words().K, V.Words().Role.c_str());
		return Buf;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: land_live_kit_test <Content folder> <folder with header.json, deltas.jsonl, whole.json>\n");
		return 2;
	}
	const std::string Content = Argv[1];
	const std::string Folder = Argv[2];
	std::string HeaderText, WholeText, FatesText, Error;
	if (!ReadFile(Folder + "/header.json", HeaderText) || !ReadFile(Folder + "/whole.json", WholeText))
	{
		std::printf("FAIL cannot read %s/header.json or whole.json\n", Folder.c_str());
		return 2;
	}
	Fates::Table Table;
	const bool bTable = ReadFile(Content + "/Data/Battle/fates.json", FatesText) && Table.Load(FatesText, Error);
	Check(bTable, "the Fates table read (" + Error + ")");
	const Battle::ReadFn Read = [&Content](const std::string& File, std::string& Out) { return ReadFile(Content + "/Models/Battle/" + File, Out); };
	// (rolling ground: the gun's lay reads the ground under her and her aim)
	const GroundFn Hills = [](double X, double Y, double& OutZ, V3& OutNormal)
	{
		OutZ = 1.5 * std::sin(X / 45.0) * std::cos(Y / 60.0) + 0.6 * std::sin((X + Y) / 23.0);
		const double DX = 1.5 / 45.0 * std::cos(X / 45.0) * std::cos(Y / 60.0) + 0.6 / 23.0 * std::cos((X + Y) / 23.0);
		const double DY = -1.5 / 60.0 * std::sin(X / 45.0) * std::sin(Y / 60.0) + 0.6 / 23.0 * std::cos((X + Y) / 23.0);
		OutNormal = V3(-DX, -DY, 1.0).Unit();
	};
	// The battle's own seed, as the scene gives it (the same to both).
	uint64_t Seed = 1;
	{
		Json Doc;
		if (Doc.Parse(HeaderText, Error) && Doc.Root()["seed"].IsNumber())
		{
			Seed = static_cast<uint64_t>(Doc.Root()["seed"].Num());
		}
	}

	Battle Whole, Live;
	Whole.Life = 2.0;
	Live.Life = 2.0;
	const auto W0 = Clock::now();
	if (!Whole.Load(WholeText, Read, Hills, bTable ? &Table : nullptr, Seed, Error))
	{
		std::printf("FAIL the whole battle: %s\n", Error.c_str());
		return 1;
	}
	const auto W1 = Clock::now();
	if (!Live.Load(HeaderText, Read, Hills, bTable ? &Table : nullptr, Seed, Error))
	{
		std::printf("FAIL the header: %s\n", Error.c_str());
		return 1;
	}
	const auto W2 = Clock::now();
	Check(Live.bLive && !Whole.bLive, "the header reads as live, the whole as a recording");
	Check(Live.Vehicles().empty(), "the header has no vehicle");
	Check(!Live.Models().empty(), "the header's models are read ahead");
	{
		std::string Why;
		Battle NotLive;
		Check(!NotLive.Extend("{\"t\":0.5,\"men\":[]}", Why), "a battle not loaded from a header refuses a piece");
	}

	// When each lorry first moves, read whole (a metre from where she stood).
	std::vector<double> FirstMoves(Whole.Vehicles().size(), -1.0);
	for (size_t I = 0; I < Whole.Vehicles().size(); ++I)
	{
		const TrackWords& W = Whole.Vehicles()[I].Words();
		for (size_t K = 0; W.Role == "lorry" && K + 3 < W.At.size(); K += 4)
		{
			if (std::hypot(W.At[K + 1] - W.At[1], W.At[K + 2] - W.At[2]) >= 1.0)
			{
				FirstMoves[I] = W.At[K];
				break;
			}
		}
	}

	// The worst seen, frame by frame.
	const double Pi = 3.14159265358979323846;
	const double Frame = 1.0 / (60.0 * Live.Life);
	double Shown = 0.0;
	long Frames = 0, Compared = 0, Matched = 0, FarEvents = 0, OverHeading = 0, OverTurret = 0;
	double PlaceWorst = 0.0, HeadingWorst = 0.0, LorryWorst = 0.0, TurretWorst = 0.0, GunWorst = 0.0, EventTWorst = 0.0, EventAtWorst = 0.0;
	std::string PlaceWho, HeadingWho, TurretWho, GunWho, EventAtWho;
	long CountMisses = 0, DeathMisses = 0;
	// (seamless: each frame's step of a vehicle on the screen, grown against whole, so a piece that works her out again
	// shows as a jump)
	std::vector<V3> WasWhole, WasLive;
	std::vector<double> WasWholeHeading, WasLiveHeading;
	double JumpPlace = 0.0, JumpHeading = 0.0;
	std::string JumpPlaceWho, JumpHeadingWho;
	struct Pending
	{
		Event E;
		long Frame = 0;
	};
	std::vector<Pending> FromWhole, FromLive;
	std::vector<Event> OnlyWhole, OnlyLive;
	auto SameEvent = [](const Event& A, const Event& B) { return A.Vehicle == B.Vehicle && A.Fx == B.Fx && A.Cue == B.Cue && A.Round == B.Round; };
	auto PlayTo = [&](double Until)
	{
		while (Shown + 1e-9 < Until)
		{
			const double T = std::min(Until, Shown + Frame);
			Whole.Update(T);
			Live.Update(T);
			++Frames;
			const std::vector<Vehicle>& A = Whole.Vehicles();
			const std::vector<Vehicle>& B = Live.Vehicles();
			if (A.size() != B.size())
			{
				++CountMisses;
				Check(CountMisses > 1, "the vehicles at " + std::to_string(T) + ": " + std::to_string(B.size()) + " grown, " + std::to_string(A.size()) + " whole");
			}
			for (size_t I = 0; I < A.size() && I < B.size(); ++I)
			{
				const Vehicle& P = A[I];
				const Vehicle& Q = B[I];
				++Compared;
				const double D = (P.Root.At - Q.Root.At).Length();
				if (D > PlaceWorst)
				{
					PlaceWorst = D;
					PlaceWho = Who(P) + " at " + std::to_string(T);
				}
				const bool bStanding = P.Words().Role == "lorry" && (FirstMoves[I] < 0.0 || T < FirstMoves[I] + 3.0);
				if (I < WasWhole.size())
				{
					const double J = ((Q.Root.At - WasLive[I]) - (P.Root.At - WasWhole[I])).Length();
					if (J > JumpPlace)
					{
						JumpPlace = J;
						JumpPlaceWho = Who(P) + " at " + std::to_string(T);
					}
					const double JH = std::fabs(WrapDeg((Q.Heading - WasLiveHeading[I] - (P.Heading - WasWholeHeading[I])) * 180.0 / Pi));
					if (!bStanding && JH > JumpHeading)
					{
						JumpHeading = JH;
						JumpHeadingWho = Who(P) + " at " + std::to_string(T);
					}
				}
				WasWhole.resize(std::max(WasWhole.size(), I + 1));
				WasLive.resize(WasWhole.size());
				WasWholeHeading.resize(WasWhole.size());
				WasLiveHeading.resize(WasWhole.size());
				WasWhole[I] = P.Root.At;
				WasLive[I] = Q.Root.At;
				WasWholeHeading[I] = P.Heading;
				WasLiveHeading[I] = Q.Heading;
				const double H = std::fabs(WrapDeg((P.Heading - Q.Heading) * 180.0 / Pi));
				if (bStanding)
				{
					LorryWorst = std::max(LorryWorst, H);
					continue;
				}
				OverHeading += H > 2.0 ? 1 : 0;
				if (H > HeadingWorst)
				{
					HeadingWorst = H;
					HeadingWho = Who(P) + " at " + std::to_string(T);
				}
				const Model& M = P.Look();
				const int TurretBody = BodyOf(M, M.Turret >= 0 ? M.Turret : M.Traverse), GunBody = BodyOf(M, M.Gun);
				double Over = 0.0;
				if (TurretBody >= 0)
				{
					const double G = Apart(P.Bodies()[static_cast<size_t>(TurretBody)].Q, Q.Bodies()[static_cast<size_t>(TurretBody)].Q);
					Over = std::max(Over, G);
					if (G > TurretWorst)
					{
						TurretWorst = G;
						TurretWho = Who(P) + " at " + std::to_string(T);
					}
				}
				if (GunBody >= 0)
				{
					const double G = Apart(P.Bodies()[static_cast<size_t>(GunBody)].Q, Q.Bodies()[static_cast<size_t>(GunBody)].Q);
					Over = std::max(Over, G);
					if (G > GunWorst)
					{
						GunWorst = G;
						GunWho = Who(P) + " at " + std::to_string(T);
					}
				}
				OverTurret += Over > 2.0 ? 1 : 0;
				// (knocked out on the screen: the same moment, the same look)
				const bool bSameEnd = P.bDead == Q.bDead && (!P.bDead || (P.Died == Q.Died && P.bBrewUp == Q.bBrewUp && P.FateKey == Q.FateKey && P.BurnFrom == Q.BurnFrom));
				if (!bSameEnd)
				{
					++DeathMisses;
					Check(false, Who(P) + "'s end at " + std::to_string(T) + ": died " + std::to_string(Q.Died) + " " + Q.FateKey + " v " + std::to_string(P.Died) + " " + P.FateKey);
				}
			}
			// Every event once, on the same vehicle, the same effect and cue, within a frame.
			for (const Event& E : Whole.Events)
			{
				FromWhole.push_back({ E, Frames });
			}
			for (const Event& E : Live.Events)
			{
				FromLive.push_back({ E, Frames });
			}
			Whole.Events.clear();
			Live.Events.clear();
			for (size_t I = 0; I < FromWhole.size();)
			{
				size_t J = 0;
				while (J < FromLive.size() && !(SameEvent(FromWhole[I].E, FromLive[J].E) && std::fabs(FromWhole[I].E.T - FromLive[J].E.T) <= Frame + 1e-9))
				{
					++J;
				}
				if (J < FromLive.size())
				{
					EventTWorst = std::max(EventTWorst, std::fabs(FromWhole[I].E.T - FromLive[J].E.T));
					const double D = (FromWhole[I].E.At - FromLive[J].E.At).Length();
					if (D > EventAtWorst)
					{
						EventAtWorst = D;
						EventAtWho = FromWhole[I].E.Fx + " of " + Who(A[static_cast<size_t>(FromWhole[I].E.Vehicle)]) + " at " + std::to_string(FromWhole[I].E.T);
					}
					FarEvents += D > 0.5 ? 1 : 0;
					++Matched;
					FromWhole.erase(FromWhole.begin() + static_cast<std::ptrdiff_t>(I));
					FromLive.erase(FromLive.begin() + static_cast<std::ptrdiff_t>(J));
					continue;
				}
				++I;
			}
			// (one not matched within two frames is one the other did not give then)
			for (std::vector<Pending>* List : { &FromWhole, &FromLive })
			{
				for (size_t I = 0; I < List->size();)
				{
					if (Frames - (*List)[I].Frame >= 2)
					{
						(List == &FromWhole ? OnlyWhole : OnlyLive).push_back((*List)[I].E);
						List->erase(List->begin() + static_cast<std::ptrdiff_t>(I));
						continue;
					}
					++I;
				}
			}
			Shown = T;
		}
	};

	// The provisional points' own slack: where each one puts a vehicle against the whole battle's line at its time.
	std::vector<int> TrackOf; // a piece's track number: its vehicle in the whole battle, or -1
	double TailPlace = 0.0, TailFacing = 0.0;
	auto Slack = [&](const std::string& Text)
	{
		Json Doc;
		std::string Why;
		if (!Doc.Parse(Text, Why))
		{
			return;
		}
		const Json::Value R = Doc.Root();
		if (R["reset"].Truth(false))
		{
			TrackOf.clear();
		}
		const Json::Value Men = R["men"];
		for (size_t I = 0; I < Men.Count(); ++I)
		{
			const Json::Value Man = Men.At(I);
			if (Man["n"].Int(-1) != static_cast<int>(TrackOf.size()))
			{
				continue;
			}
			int Found = -1;
			for (size_t V = 0; V < Whole.Vehicles().size(); ++V)
			{
				const TrackWords& W = Whole.Vehicles()[V].Words();
				if (W.Block == Man["b"].Int() && W.K == Man["k"].Int() && W.J == std::max(0, Man["j"].Int(0)) && W.Role == Man["r"].Str())
				{
					Found = static_cast<int>(V);
				}
			}
			TrackOf.push_back(Found);
		}
		std::vector<double> Row;
		if (!R["tails"].Numbers(Row))
		{
			return;
		}
		const double T = R["t"].Num();
		for (size_t K = 0; K + 3 < Row.size(); K += 4)
		{
			const int N = static_cast<int>(Row[K]);
			if (N < 0 || N >= static_cast<int>(TrackOf.size()) || TrackOf[static_cast<size_t>(N)] < 0)
			{
				continue;
			}
			const Vehicle& V = Whole.Vehicles()[static_cast<size_t>(TrackOf[static_cast<size_t>(N)])];
			if (V.Died >= 0.0 && T >= V.Died - 0.2)
			{
				continue; // (a wreck's last points are laid where she was hit)
			}
			double X = 0.0, Y = 0.0, F = 0.0;
			Along(V.Words().At, T, X, Y, F);
			TailPlace = std::max(TailPlace, std::hypot(Row[K + 1] - X, Row[K + 2] - Y));
			if (V.Words().Role != "lorry")
			{
				TailFacing = std::max(TailFacing, std::fabs(WrapDeg((Row[K + 3] - F) * 180.0 / Pi)));
			}
		}
	};

	std::ifstream In(Folder + "/deltas.jsonl");
	std::string Line;
	int Deltas = 0;
	double PrevT = -1.0, ExtendMs = 0.0, MostMs = 0.0, MostAt = 0.0;
	std::vector<std::pair<double, double>> Costs; // (t, ms)
	while (std::getline(In, Line))
	{
		if (Line.empty())
		{
			continue;
		}
		const auto A = Clock::now();
		const bool bOk = Live.Extend(Line, Error);
		const double Ms = std::chrono::duration<double, std::milli>(Clock::now() - A).count();
		if (!bOk)
		{
			std::printf("FAIL piece %d: %s\n", Deltas, Error.c_str());
			return 1;
		}
		++Deltas;
		Slack(Line);
		Costs.emplace_back(Live.End, Ms);
		if (Deltas > 1)
		{
			// (the first piece names every man: it is timed apart)
			ExtendMs += Ms;
			if (Ms > MostMs)
			{
				MostMs = Ms;
				MostAt = Live.End;
			}
		}
		Check(Live.End >= PrevT, "the pieces go forward");
		PrevT = Live.End;
		PlayTo(Live.bOver ? Live.End + 8.0 : std::max(0.0, Live.End - 2.0));
	}
	Check(Deltas > 0, "pieces were read");
	Check(Live.bOver, "the last piece ends the battle");
	PlayTo(Shown + 2.0 * Frame);
	for (const Pending& P : FromWhole)
	{
		OnlyWhole.push_back(P.E);
	}
	for (const Pending& P : FromLive)
	{
		OnlyLive.push_back(P.E);
	}

	// The events one gave and the other did not then, paired: the same but for a step or two of the passes apart (her
	// moving off or stopping read a step later from a provisional point, or the first step after the screen); a lorry's
	// first move (she turns to her way on the screen, where read whole she faced it from the start); and a move-off or
	// stop that a provisional point dated otherwise than the whole battle's line does (the rules keep a point for a
	// tenth of a metre, not for a few degrees of facing: by the time the next kept point came the screen had shown it,
	// so it stands, or the grown battle shows it at the first step after). The rest are missing.
	long Shifted = 0, Lorries = 0, Provisional = 0;
	double ShiftWorst = 0.0, LorryShift = 0.0, ProvisionalWorst = 0.0;
	std::map<std::string, long> ShiftedFx;
	std::string ProvisionalWho;
	for (int Pass = 0; Pass < 3; ++Pass)
	{
		for (size_t I = 0; I < OnlyWhole.size();)
		{
			const Event& E = OnlyWhole[I];
			const size_t V = static_cast<size_t>(E.Vehicle);
			const bool bFirstMove = Whole.Vehicles()[V].Words().Role == "lorry" && (FirstMoves[V] < 0.0 || E.T < FirstMoves[V] + 3.0);
			const double Within = Pass == 0 ? 0.1 : Pass == 1 ? (bFirstMove ? 3.0 : -1.0) : 2.5;
			size_t J = 0;
			while (J < OnlyLive.size() && !(SameEvent(E, OnlyLive[J]) && std::fabs(E.T - OnlyLive[J].T) <= Within + 1e-9))
			{
				++J;
			}
			if (J < OnlyLive.size())
			{
				const double Apart = std::fabs(E.T - OnlyLive[J].T);
				if (Pass == 0)
				{
					++Shifted;
					++ShiftedFx[E.Fx];
					ShiftWorst = std::max(ShiftWorst, Apart);
				}
				else if (Pass == 1)
				{
					++Lorries;
					LorryShift = std::max(LorryShift, Apart);
				}
				else
				{
					++Provisional;
					ProvisionalWorst = std::max(ProvisionalWorst, Apart);
					// (named once for each moment: a move-off or stop is several events)
					char Buf[160];
					std::snprintf(Buf, sizeof(Buf), "%s at %.2f, grown %.2f", Who(Whole.Vehicles()[V]).c_str(), E.T, OnlyLive[J].T);
					if (ProvisionalWho.find(Buf) == std::string::npos)
					{
						ProvisionalWho += (ProvisionalWho.empty() ? "" : "; ") + std::string(Buf);
					}
				}
				OnlyWhole.erase(OnlyWhole.begin() + static_cast<std::ptrdiff_t>(I));
				OnlyLive.erase(OnlyLive.begin() + static_cast<std::ptrdiff_t>(J));
				continue;
			}
			++I;
		}
	}
	// And a turn on the spot that only a provisional point showed: she stood where the rules last kept her and turned less
	// than a kept line may miss her facing by (0.05 rad, under 3 degrees), so the whole battle's line holds her still and
	// gives no move-off or stop, while the grown battle, shown that point a tenth of a second after the kept one, saw her
	// pivot that much at once (her brakes and idle heard after it). Counted and named only where the event is her going's
	// or standing's (from her move_off, drive, stop or idle clip) and the whole battle holds her within 0.1 m and that
	// slack for a second either side of it; any other the grown battle alone gives is missing.
	long Twitches = 0;
	std::string TwitchWho;
	const double FacingSlack = 0.05 * 180.0 / Pi;
	auto GoingFx = [](const Vehicle& V, const Event& E)
	{
		if (E.Round != 0)
		{
			return false;
		}
		for (const char* Name : { "move_off", "drive", "stop", "idle" })
		{
			const Clip* C = V.Look().FindClip(Name);
			for (size_t K = 0; C && K < C->Events.size(); ++K)
			{
				if (C->Events[K].Fx == E.Fx)
				{
					return true;
				}
			}
		}
		return false;
	};
	auto HeldStill = [&](const Vehicle& V, double T)
	{
		double X0 = 0.0, Y0 = 0.0, F0 = 0.0;
		Along(V.Words().At, T - 1.0, X0, Y0, F0);
		for (int S = -9; S <= 10; ++S)
		{
			double X = 0.0, Y = 0.0, F = 0.0;
			Along(V.Words().At, T + S * 0.1, X, Y, F);
			if (std::hypot(X - X0, Y - Y0) > 0.1 + 1e-6 || std::fabs(WrapDeg((F - F0) * 180.0 / Pi)) > FacingSlack + 1e-6)
			{
				return false;
			}
		}
		return true;
	};
	for (size_t I = 0; I < OnlyLive.size();)
	{
		const Event& E = OnlyLive[I];
		const size_t V = static_cast<size_t>(E.Vehicle);
		if (V < Whole.Vehicles().size() && V < Live.Vehicles().size() && GoingFx(Live.Vehicles()[V], E)
			&& (Whole.Vehicles()[V].Died < 0.0 || E.T < Whole.Vehicles()[V].Died - 1.0) && HeldStill(Whole.Vehicles()[V], E.T))
		{
			++Twitches;
			char Buf[160];
			std::snprintf(Buf, sizeof(Buf), "%s at %.2f", Who(Whole.Vehicles()[V]).c_str(), E.T);
			if (TwitchWho.find(Buf) == std::string::npos)
			{
				TwitchWho += (TwitchWho.empty() ? "" : "; ") + std::string(Buf);
			}
			OnlyLive.erase(OnlyLive.begin() + static_cast<std::ptrdiff_t>(I));
			continue;
		}
		++I;
	}
	for (const Event& E : OnlyWhole)
	{
		Check(false, "only the whole battle gives " + E.Fx + " / " + E.Cue + " of " + Who(Whole.Vehicles()[static_cast<size_t>(E.Vehicle)]) + " at " + std::to_string(E.T));
	}
	for (const Event& E : OnlyLive)
	{
		Check(false, "only the grown battle gives " + E.Fx + " / " + E.Cue + " of " + Who(Live.Vehicles()[static_cast<size_t>(E.Vehicle)]) + " at " + std::to_string(E.T));
	}

	// Once over: the same vehicles, the same ends, the same End.
	Check(Live.End == Whole.End, "end " + std::to_string(Live.End) + " v " + std::to_string(Whole.End));
	Check(Live.Vehicles().size() == Whole.Vehicles().size(), "vehicles " + std::to_string(Live.Vehicles().size()) + " v " + std::to_string(Whole.Vehicles().size()));
	int Kills = 0, Brews = 0;
	for (size_t I = 0; I < Live.Vehicles().size() && I < Whole.Vehicles().size(); ++I)
	{
		const Vehicle& P = Whole.Vehicles()[I];
		const Vehicle& Q = Live.Vehicles()[I];
		Check(P.Words().Block == Q.Words().Block && P.Words().K == Q.Words().K && P.Words().J == Q.Words().J && P.Words().Role == Q.Words().Role, Who(P) + ": who");
		Check(P.Died == Q.Died && P.bBrewUp == Q.bBrewUp && P.FateKey == Q.FateKey && P.BurnFrom == Q.BurnFrom && P.BurnUntil == Q.BurnUntil,
			Who(P) + ": her end " + std::to_string(Q.Died) + " " + Q.FateKey + " v " + std::to_string(P.Died) + " " + P.FateKey);
		Check(P.Scars().size() == Q.Scars().size(), Who(P) + ": scars");
		Kills += P.Died >= 0.0 ? 1 : 0;
		Brews += P.bBrewUp ? 1 : 0;
	}

	// What a piece costs: on average, at worst, and over the battle's last 30 s.
	double LateMs = 0.0, LateMost = 0.0;
	int Late = 0;
	for (const auto& C : Costs)
	{
		if (C.first >= Live.End - 30.0)
		{
			LateMs += C.second;
			LateMost = std::max(LateMost, C.second);
			++Late;
		}
	}
	const double FirstMs = Costs.empty() ? 0.0 : Costs.front().second;
	const double Facing = std::max(2.0, TailFacing + 0.5);
	std::string Shifts;
	for (const auto& Named : ShiftedFx)
	{
		Shifts += " " + Named.first + " " + std::to_string(Named.second);
	}
	std::printf("%s: %d pieces to t=%.1f; %zu vehicles (%d killed, %d brewed up); whole Load %.0f ms, header Load %.0f ms\n",
		Folder.c_str(), Deltas, Live.End, Live.Vehicles().size(), Kills, Brews, std::chrono::duration<double, std::milli>(W1 - W0).count(),
		std::chrono::duration<double, std::milli>(W2 - W1).count());
	std::printf("  Extend: %.3f ms a piece (worst %.2f ms at t=%.1f; the first %.1f ms); the last 30 s: %.3f ms a piece, worst %.2f ms\n",
		ExtendMs / std::max(1, Deltas - 1), MostMs, MostAt, FirstMs, LateMs / std::max(1, Late), LateMost);
	std::printf("  provisional points from the whole battle's line: %.3f m, facing %.2f deg\n", TailPlace, TailFacing);
	std::printf("  %ld frames, %ld vehicle-frames: place within %.3f m (%s)\n", Frames, Compared, PlaceWorst, PlaceWho.c_str());
	std::printf("  heading within %.3f deg (%s; %ld vehicle-frames over 2); a lorry before her first move %.1f deg\n", HeadingWorst, HeadingWho.c_str(),
		OverHeading, LorryWorst);
	std::printf("  turret within %.3f deg (%s), gun %.3f deg (%s); %ld vehicle-frames over 2\n", TurretWorst, TurretWho.c_str(), GunWorst, GunWho.c_str(), OverTurret);
	std::printf("  a frame's step, grown v whole: place within %.4f m (%s), heading %.3f deg (%s)\n", JumpPlace, JumpPlaceWho.c_str(), JumpHeading, JumpHeadingWho.c_str());
	std::printf("  %ld events the same within a frame (worst %.4f s), places within %.3f m (%s; %ld over 0.5 m); %ld a step or two apart (worst %.3f s:%s)\n",
		Matched, EventTWorst, EventAtWorst, EventAtWho.c_str(), FarEvents, Shifted, ShiftWorst, Shifts.c_str());
	std::printf("  %ld of a lorry's first move (her turn, up to %.2f s apart); %ld moved by a provisional point (up to %.2f s: %s); "
		"%ld of a turn on the spot only a provisional point showed (%s); %zu only whole, %zu only grown; ends differing in %ld vehicle-frames\n",
		Lorries, LorryShift, Provisional, ProvisionalWorst, ProvisionalWho.empty() ? "none" : ProvisionalWho.c_str(), Twitches,
		TwitchWho.empty() ? "none" : TwitchWho.c_str(), OnlyWhole.size(), OnlyLive.size(), DeathMisses);
	Check(PlaceWorst <= 0.25, "every place within 0.25 m");
	Check(HeadingWorst <= Facing, "every heading within " + std::to_string(Facing) + " deg");
	Check(TurretWorst <= Facing && GunWorst <= Facing, "every turret and gun within " + std::to_string(Facing) + " deg");
	Check(Matched > 1000, "enough events");
	Check(Shifted * 50 < Matched, "events a step apart are few");
	Check(Provisional * 200 < Matched, "events moved by a provisional point are few");
	Check(Twitches * 200 < Matched, "events of a turn on the spot only a provisional point showed are few");
	Check(JumpPlace <= 0.02 && JumpHeading <= 1.0, "no jump on the screen as a piece works her out again");

	// Effects (R2), small arms off armour (-TheaterLandRicochet, Battle::bKeepPlates): the same battle grown again with
	// the plates of armour's small-arms strikes kept. Growing her (Extend, on the game thread) must cost what it costs
	// with the switch off: the plates are searched after the pieces, a few a frame (Battle::KeepPlates, as PumpLive
	// calls it: the earliest still to be drawn first, about 1 ms), never inside Extend. Each frame's searching is timed,
	// and every plate is kept before the screen reaches its strike. (Armour here: every tank and gun; the game's
	// LandPlay::ArmouredVehicle also makes armour of the half-tracks the rules carry men in.)
	{
		Battle Keep;
		Keep.Life = Live.Life;
		Keep.bKeepPlates = true;
		Keep.Armoured = [](const TrackWords& W, const std::string&) { return W.Role != "lorry"; };
		std::string KeepError;
		if (!Keep.Load(HeaderText, Read, Hills, bTable ? &Table : nullptr, Seed, KeepError))
		{
			std::printf("FAIL the header, plates kept: %s\n", KeepError.c_str());
			return 1;
		}
		std::ifstream KeepIn(Folder + "/deltas.jsonl");
		std::string KeepLine;
		int KeepPieces = 0;
		long KeepFrames = 0, BusyFrames = 0;
		size_t Searched = 0, DrawnUnsearched = 0, LateStrikes = 0, BehindWas = 0;
		double KeepExtendMs = 0.0, KeepExtendMost = 0.0, SearchMs = 0.0, SearchMost = 0.0, KeepShown = 0.0, LastEnd = 0.0;
		while (std::getline(KeepIn, KeepLine))
		{
			if (KeepLine.empty())
			{
				continue;
			}
			const auto A = Clock::now();
			if (!Keep.Extend(KeepLine, KeepError))
			{
				std::printf("FAIL piece %d, plates kept: %s\n", KeepPieces, KeepError.c_str());
				return 1;
			}
			const double Ms = std::chrono::duration<double, std::milli>(Clock::now() - A).count();
			++KeepPieces;
			if (KeepPieces > 1)
			{
				KeepExtendMs += Ms;
				KeepExtendMost = std::max(KeepExtendMost, Ms);
			}
			// (the frames until the next piece, one search call a frame; the screen one battle second behind the latest, as
			// close as the game lets it come before it waits on the rules (LandBattleLive.cpp, LiveMargin), not the two
			// seconds of the comparison above)
			const double ShowTo = Keep.bOver ? Keep.End + 8.0 : std::max(0.0, Keep.End - 1.0);
			const long FramesNow = std::max(1L, static_cast<long>(std::ceil((Keep.End - LastEnd) / Frame)));
			LastEnd = Keep.End;
			const double From = KeepShown;
			// (a piece's strikes already behind the screen as it comes: drawn at her skin's point, never searched)
			const size_t BehindNow = Keep.PlatesWaiting(-1e300) - Keep.PlatesWaiting(KeepShown);
			LateStrikes += BehindNow > BehindWas ? BehindNow - BehindWas : 0;
			for (long F = 0; F < FramesNow; ++F)
			{
				const double ShownWas = KeepShown;
				KeepShown = std::max(KeepShown, From + (ShowTo - From) * static_cast<double>(F + 1) / static_cast<double>(FramesNow));
				// (the strikes the screen reaches on this frame: each must have been searched by now)
				DrawnUnsearched += Keep.PlatesWaiting(ShownWas) - Keep.PlatesWaiting(KeepShown);
				const auto S0 = Clock::now();
				const size_t Did = Keep.KeepPlates(KeepShown, 4, 0.001);
				const double SMs = std::chrono::duration<double, std::milli>(Clock::now() - S0).count();
				++KeepFrames;
				if (Did > 0)
				{
					++BusyFrames;
					Searched += Did;
					SearchMs += SMs;
					SearchMost = std::max(SearchMost, SMs);
				}
			}
			BehindWas = Keep.PlatesWaiting(-1e300) - Keep.PlatesWaiting(KeepShown);
		}
		// (a strike the screen passed unsearched was drawn at her skin's point: KeepPlates never searches behind the screen;
		// one whose cast a later piece moved to another body after it was drawn waits behind it, harmless)
		const size_t LeftBehind = Keep.PlatesWaiting(-1e300);
		size_t KeptLive = 0, KeptWhole = 0, Struck = 0;
		Battle WholeKeep;
		WholeKeep.Life = Live.Life;
		WholeKeep.bKeepPlates = true;
		WholeKeep.Armoured = Keep.Armoured;
		const bool bWholeKeep = WholeKeep.Load(WholeText, Read, Hills, bTable ? &Table : nullptr, Seed, KeepError);
		Check(bWholeKeep, "the whole battle with its plates kept (" + KeepError + ")");
		for (const Round& R : WholeKeep.Rounds())
		{
			if ((R.W != "rifle" && R.W != "mg") || R.R.empty() || R.R == "miss")
			{
				continue;
			}
			for (size_t I = 0; I < WholeKeep.Vehicles().size() && I < Keep.Vehicles().size(); ++I)
			{
				const TrackWords& W = WholeKeep.Vehicles()[I].Words();
				if (R.On == W.Block && R.Ok == W.K && R.Oj == W.J && W.Role != "lorry")
				{
					++Struck;
					KeptWhole += WholeKeep.Vehicles()[I].PlateKept(R.N) ? 1 : 0;
					KeptLive += Keep.Vehicles()[I].PlateKept(R.N) ? 1 : 0;
				}
			}
		}
		const double OffMean = ExtendMs / std::max(1, Deltas - 1), KeepMean = KeepExtendMs / std::max(1, KeepPieces - 1);
		std::printf("  small arms off armour, plates kept: %zu small-arms strikes on armour, %zu plates kept grown piece by piece (%zu read whole); %zu reached by the screen before searched; %zu behind the screen as their piece came "
			"(new, or cast again on another body), %zu left waiting behind it at the end; "
			"Extend %.3f ms a piece (worst %.2f ms) against %.3f ms (worst %.2f ms) with the switch off; searching on %ld of %ld frames, %zu plates, %.2f ms on such a frame, the most %.2f ms\n",
			Struck, KeptLive, KeptWhole, DrawnUnsearched, LateStrikes, LeftBehind, KeepMean, KeepExtendMost, OffMean, MostMs, BusyFrames, KeepFrames, Searched,
			BusyFrames ? SearchMs / static_cast<double>(BusyFrames) : 0.0, SearchMost);
		Check(KeepMean <= 1.5 * OffMean + 2.0, "plates kept: Extend costs what it costs with the switch off (" + std::to_string(KeepMean) + " ms a piece against " + std::to_string(OffMean) + ")");
		Check(DrawnUnsearched == 0, "plates kept: every plate searched before the screen reaches its strike");
		Check(KeptLive <= KeptWhole && KeptLive + LeftBehind >= KeptWhole, "plates kept: grown piece by piece, as many plates kept as read whole (but those cast again after the screen passed them)");
		Check(Struck == 0 || KeptLive > 0, "plates kept: some plate kept");
	}
	std::printf("%s\n", Failures ? "FAIL" : "PASS");
	return Failures ? 1 : 0;
}
