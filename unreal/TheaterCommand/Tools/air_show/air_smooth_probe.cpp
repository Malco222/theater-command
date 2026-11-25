// The smoothness probe (Brian, 24 November 22:35Z: "why is the plane shaking in the test video?"; "i want the flying
// to be smooth and very realistic"). It plays a recorded air battle through the game's own flight core (Battle/AirShow)
// at the game's frame pace and measures, for every aircraft drawn, how much of its motion is tremor: what is left of
// its place and of its heading, pitch and bank once a smooth curve (a parabola fitted over 0.4 s of flight, the
// Savitzky-Golay filter) is taken away. A machine flying a steady line, turn, climb or roll leaves nothing; a machine
// whose place or attitude snaps from frame to frame leaves the snap. Seen from a chase camera 25 m behind, 5 cm of
// tremor is two pixels of shake, and half a degree of roll tremor moves a bomber's wing tip 13 cm.
//
// Usage: air_smooth_probe <recording.json> [frames a second, 60] [from battle s] [to battle s] [x speed, 1] [track.csv key]
// A recording made by tools/air-record/live-steps.ts carries a view at every step of the rules (ten a battle second),
// as AAirBattle::Feed takes them in a live raid; the set pieces and r16-live carry one a battle second.
//
// It is a check as well (Tools/air_show/run.sh runs it on Tools/air_show/data/raid-steps.json, a minute of a campaign
// raid about its first contact, a view every rules step): it fails where any aircraft's tremor is what a chase camera
// would show (3 cm of place or a quarter of a degree of an angle, rms), or where a fighter flying in formation kicks
// its wings half a degree. AIR_SMOOTH_OLD=1 flies it as release 1.5 flew it (AirShow::Show::SetSmooth off) and only
// reports: 78 of the 143 aircraft of that minute trembled then, the formations' wings kicking 2.5 degrees.

#include "Battle/AirFeed.h"
#include "Battle/AirGround.h"
#include "Battle/AirShow.h"
#include "Battle/AirSky.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	struct Sample
	{
		double T = 0.0; // real seconds
		double X = 0.0, Y = 0.0, Z = 0.0, H = 0.0, P = 0.0, B = 0.0;
		int Frame = 0;
		std::string Doing;
	};

	struct Track
	{
		int Key = 0, Group = 0, Side = 0;
		bool bFighter = false;
		std::string Model;
		std::vector<Sample> S;
	};

	/** What is left at the middle of 2K+1 even samples once their least-squares parabola is taken away. */
	double Residual(const std::vector<double>& V, size_t Mid, int K)
	{
		const double Kd = K;
		const double Den = (2.0 * Kd + 3.0) * (2.0 * Kd + 1.0) * (2.0 * Kd - 1.0);
		double Fit = 0.0;
		for (int I = -K; I <= K; ++I)
		{
			const double C = (3.0 * (3.0 * Kd * Kd + 3.0 * Kd - 1.0) - 15.0 * I * I) / Den;
			Fit += C * V[Mid + static_cast<size_t>(I + K) - static_cast<size_t>(K)];
		}
		return V[Mid] - Fit;
	}

	struct Tremor
	{
		double Sum2 = 0.0, Peak = 0.0, PeakT = 0.0;
		long N = 0;
		void Add(double R, double T)
		{
			Sum2 += R * R;
			++N;
			if (std::fabs(R) > Peak) { Peak = std::fabs(R); PeakT = T; }
		}
		double Rms() const { return N ? std::sqrt(Sum2 / static_cast<double>(N)) : 0.0; }
	};
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: air_smooth_probe <recording.json> [fps] [from] [to] [speed] [key]\n");
		return 2;
	}
	const std::string Path = Argv[1];
	const double Fps = Argc > 2 ? std::atof(Argv[2]) : 60.0;
	const double From = Argc > 3 ? std::atof(Argv[3]) : 0.0;
	const double To = Argc > 4 ? std::atof(Argv[4]) : 1e18;
	const double Speed = Argc > 5 ? std::atof(Argv[5]) : 1.0;
	const int TrackKey = Argc > 6 ? std::atoi(Argv[6]) : -1;

	std::ifstream In(Path, std::ios::binary);
	if (!In) { std::fprintf(stderr, "cannot read %s\n", Path.c_str()); return 2; }
	std::stringstream Buffer;
	Buffer << In.rdbuf();
	AirFeed::Recording Rec;
	std::string Error;
	if (!AirFeed::ReadRecording(Buffer.str(), Rec, Error)) { std::fprintf(stderr, "%s: %s\n", Path.c_str(), Error.c_str()); return 2; }

	AirShow::Show Show(Rec.Seed ? Rec.Seed : 238);
	AirFeed::Apply(Rec.Board, Show);
	const bool bOld = std::getenv("AIR_SMOOTH_OLD") != nullptr;
	Show.SetSmooth(!bOld);
	Show.Eye = AirShow::Vec(Rec.Board.TargetX, Rec.Board.TargetY - 6000.0, 5000.0);
	const std::shared_ptr<const AirGround::BoardLand> BoardGround =
		Rec.Board.StageKey.empty() ? std::make_shared<const AirGround::BoardLand>(Rec.Board.Land) : std::shared_ptr<const AirGround::BoardLand>();
	if (BoardGround && !BoardGround->IsEmpty()) Show.Ground = [BoardGround](double X, double Y) { return BoardGround->At(X, Y); };

	const double Scale = std::max(0.1, Rec.Board.Scale);
	const double Real = 1.0 / Fps;          // a frame, real seconds
	const double Run = Real * Speed / Scale; // and the battle seconds it runs (AirDesk::Clock::Step at the speed set)
	size_t Taken = 0;
	auto TakeNext = [&]()
	{
		const AirFeed::Frame& F = Rec.Views[Taken++];
		Show.Take(F.Head.T, F.Groups, F.Events, F.Ships);
	};
	TakeNext();
	double T = Rec.Views.front().Head.T;
	const double EndT = std::min(To, Rec.Views.back().Head.T);
	// (the run-up to the window is flown at the same frame pace, so the machines arrive in it as the game has them)
	std::map<int, Track> Tracks;
	int Frame = 0;
	double RealT = 0.0;
	while (T < EndT - 1e-9)
	{
		T = std::min(EndT, T + (T < From - 5.0 ? 10.0 * Real : Run));
		RealT += Real;
		// (as AAirBattle::Feed: the rules, or the recording, a battle second ahead of what is shown)
		while (Taken < Rec.Views.size() && Rec.Views[Taken].Head.T <= T + 1.0 + 1e-9) TakeNext();
		Show.Advance(T);
		++Frame;
		if (T < From) continue;
		for (const AirShow::PlaneOut& P : Show.Planes())
		{
			if (!P.bShown || P.bFalling || P.bOnGround) continue;
			Track& Tr = Tracks[P.Key];
			if (Tr.S.empty()) { Tr.Key = P.Key; Tr.Group = P.Group; Tr.Side = P.Side; Tr.bFighter = P.bFighter; Tr.Model = P.Model; }
			Sample S;
			S.T = RealT; S.X = P.At.X; S.Y = P.At.Y; S.Z = P.At.Z; S.H = P.Heading; S.P = P.Pitch; S.B = P.Bank; S.Frame = Frame;
			S.Doing = P.bFighter ? P.Task : std::string("bomber");
			Tr.S.push_back(S);
		}
	}

	const int K = std::max(3, static_cast<int>(std::lround(0.2 * Fps))); // (0.2 s either side)
	struct Row
	{
		int Key = 0, Group = 0;
		bool bFighter = false;
		std::string Model;
		Tremor Place, Head, Pitch, Bank;
		double Seconds = 0.0;
	};
	std::vector<Row> Rows;
	std::map<std::string, Tremor> ByDoingPlace, ByDoingBank, ByDoingPitch, ByDoingHead;
	Tremor AllPlace, AllHead, AllPitch, AllBank;
	std::ofstream Csv;
	if (TrackKey >= 0) { Csv.open("smooth-track-" + std::to_string(TrackKey) + ".csv"); Csv << "t,frame,doing,x,y,z,heading,pitch,bank,rx,ry,rz,rh,rp,rb\n"; }
	for (auto& Pair : Tracks)
	{
		Track& Tr = Pair.second;
		// (runs of consecutive frames only: a machine hidden a while is not joined across the gap)
		size_t Begin = 0;
		Row R;
		R.Key = Tr.Key; R.Group = Tr.Group; R.bFighter = Tr.bFighter; R.Model = Tr.Model;
		while (Begin < Tr.S.size())
		{
			size_t End = Begin + 1;
			while (End < Tr.S.size() && Tr.S[End].Frame == Tr.S[End - 1].Frame + 1) ++End;
			const size_t N = End - Begin;
			if (N >= static_cast<size_t>(2 * K + 1))
			{
				std::vector<double> X(N), Y(N), Z(N), H(N), P(N), B(N);
				double Hu = Tr.S[Begin].H, Bu = Tr.S[Begin].B;
				for (size_t I = 0; I < N; ++I)
				{
					const Sample& S = Tr.S[Begin + I];
					X[I] = S.X; Y[I] = S.Y; Z[I] = S.Z; P[I] = S.P;
					if (I > 0)
					{
						// (unwrapped: a heading through north, a roll through inverted)
						double Dh = S.H - Tr.S[Begin + I - 1].H;
						while (Dh > 180.0) Dh -= 360.0;
						while (Dh < -180.0) Dh += 360.0;
						Hu += Dh;
						double Db = S.B - Tr.S[Begin + I - 1].B;
						while (Db > 180.0) Db -= 360.0;
						while (Db < -180.0) Db += 360.0;
						Bu += Db;
					}
					H[I] = Hu; B[I] = Bu;
				}
				for (size_t M = static_cast<size_t>(K); M + static_cast<size_t>(K) < N; ++M)
				{
					const Sample& S = Tr.S[Begin + M];
					const double Rx = Residual(X, M, K), Ry = Residual(Y, M, K), Rz = Residual(Z, M, K);
					const double Rp = std::sqrt(Rx * Rx + Ry * Ry + Rz * Rz);
					const double Rh = Residual(H, M, K), Rpi = Residual(P, M, K), Rb = Residual(B, M, K);
					R.Place.Add(Rp, S.T); R.Head.Add(Rh, S.T); R.Pitch.Add(Rpi, S.T); R.Bank.Add(Rb, S.T);
					AllPlace.Add(Rp, S.T); AllHead.Add(Rh, S.T); AllPitch.Add(Rpi, S.T); AllBank.Add(Rb, S.T);
					ByDoingPlace[S.Doing].Add(Rp, S.T); ByDoingBank[S.Doing].Add(Rb, S.T); ByDoingPitch[S.Doing].Add(Rpi, S.T); ByDoingHead[S.Doing].Add(Rh, S.T);
					if (Tr.Key == TrackKey)
						Csv << S.T << ',' << S.Frame << ',' << S.Doing << ',' << S.X << ',' << S.Y << ',' << S.Z << ',' << S.H << ',' << S.P << ',' << S.B << ','
						    << Rx << ',' << Ry << ',' << Rz << ',' << Rh << ',' << Rpi << ',' << Rb << '\n';
				}
				R.Seconds += static_cast<double>(N) * Real;
			}
			Begin = End;
		}
		if (R.Place.N > 0) Rows.push_back(R);
	}

	std::printf("%s at %.0f fps, x%.1f, battle %.0f to %.0f s: %zu aircraft, %d frames\n", Path.c_str(), Fps, Speed, From, EndT, Rows.size(), Frame);
	std::printf("  tremor (what a 0.4 s parabola leaves), rms / peak:\n");
	std::printf("    place   %.4f / %.3f m (peak at real %.2f s)\n", AllPlace.Rms(), AllPlace.Peak, AllPlace.PeakT);
	std::printf("    heading %.4f / %.3f deg\n", AllHead.Rms(), AllHead.Peak);
	std::printf("    pitch   %.4f / %.3f deg\n", AllPitch.Rms(), AllPitch.Peak);
	std::printf("    bank    %.4f / %.3f deg\n", AllBank.Rms(), AllBank.Peak);
	std::printf("  by what the pilot is about (place m, heading, pitch, bank deg; rms then peak; frames):\n");
	for (const auto& Pair : ByDoingPlace)
	{
		const std::string& D = Pair.first;
		std::printf("    %-12s %.4f %.3f | %.4f %.3f | %.4f %.3f | %.4f %.3f | %ld\n", D.empty() ? "(formation)" : D.c_str(),
			Pair.second.Rms(), Pair.second.Peak, ByDoingHead[D].Rms(), ByDoingHead[D].Peak, ByDoingPitch[D].Rms(), ByDoingPitch[D].Peak, ByDoingBank[D].Rms(), ByDoingBank[D].Peak, Pair.second.N);
	}
	std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Place.Rms() > B.Place.Rms(); });
	std::printf("  the eight that tremble most in place (key group model: place rms/peak m, bank rms/peak deg, pitch rms/peak deg, s flown):\n");
	for (size_t I = 0; I < Rows.size() && I < 8; ++I)
		std::printf("    %d g%d %s%s: %.4f/%.3f m at %.2f s, bank %.3f/%.2f, pitch %.3f/%.2f, heading %.3f/%.2f, %.0f s\n", Rows[I].Key, Rows[I].Group, Rows[I].Model.c_str(), Rows[I].bFighter ? "" : " (bomber)",
			Rows[I].Place.Rms(), Rows[I].Place.Peak, Rows[I].Place.PeakT, Rows[I].Bank.Rms(), Rows[I].Bank.Peak, Rows[I].Pitch.Rms(), Rows[I].Pitch.Peak, Rows[I].Head.Rms(), Rows[I].Head.Peak, Rows[I].Seconds);
	std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Bank.Rms() > B.Bank.Rms(); });
	std::printf("  the eight that tremble most in bank:\n");
	for (size_t I = 0; I < Rows.size() && I < 8; ++I)
		std::printf("    %d g%d %s%s: bank %.3f/%.2f deg at %.2f s, place %.4f/%.3f m, %.0f s\n", Rows[I].Key, Rows[I].Group, Rows[I].Model.c_str(), Rows[I].bFighter ? "" : " (bomber)",
			Rows[I].Bank.Rms(), Rows[I].Bank.Peak, Rows[I].Bank.PeakT, Rows[I].Place.Rms(), Rows[I].Place.Peak, Rows[I].Seconds);
	// (how many aircraft tremble enough to see from a chase camera: 3 cm rms of place, or a quarter degree rms of bank or pitch)
	int Seen = 0;
	for (const Row& R : Rows) Seen += (R.Place.Rms() > 0.03 || R.Bank.Rms() > 0.25 || R.Pitch.Rms() > 0.25 || R.Head.Rms() > 0.25) ? 1 : 0;
	std::printf("  aircraft whose tremor a chase camera would show (place over 3 cm rms, or an angle over 0.25 deg rms): %d of %zu\n", Seen, Rows.size());
	const double FormationKick = ByDoingBank.count("") ? ByDoingBank[""].Peak : 0.0;
	if (bOld)
	{
		std::printf("  (flown as release 1.5 flew it, AIR_SMOOTH_OLD: reported, not judged)\n");
		return 0;
	}
	const bool bPass = !Rows.empty() && Seen == 0 && FormationKick <= 0.5;
	std::printf("  %s: smooth flying (no aircraft's tremor seen from a chase camera, and no fighter in formation kicking its wings over half a degree: %d seen, %.3f deg)\n",
		bPass ? "pass" : "FAIL", Seen, FormationKick);
	return bPass ? 0 : 1;
}
