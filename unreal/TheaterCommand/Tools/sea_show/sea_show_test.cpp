// The sea show's cloud test (the ship behaviour plan's B0, its look items L1 to L7): plays a recorded fleet action
// (Content/Data/SeaBattles/<key>.json, from the rules repo's tools/sea-record) through the very code the game runs
// (Source/TheaterCommand/Battle/SeaShow and SeaFeed), at the game's frame rate, and checks that every ship steamed as a
// ship does and that everything the battle said happened was shown where it happened. It writes a track of the action
// for the check pictures.
//
//   Tools/sea_show/run.sh Content/Data/SeaBattles/samar.json [out-dir]
//
// Plain C++20, no engine: it builds anywhere with g++ or clang++.

#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using SeaShow::Vec;
	constexpr double Pi = 3.14159265358979323846;

	struct Limit
	{
		const char* Name;
		bool bOk;
		std::string Got;
		const char* Want;
	};

	std::string Num(double V, int Places = 1)
	{
		char Buf[64];
		std::snprintf(Buf, sizeof Buf, "%.*f", Places, V);
		return Buf;
	}

	double Flat(const Vec& V) { return std::hypot(V.X, V.Y); }

	/** A hull as two ends of its middle line and the half beam round it (a capsule, seen from above). */
	struct Capsule
	{
		Vec A, B;
		double R = 0.0;
	};

	Capsule HullOf(const SeaShow::ShipOut& S)
	{
		const double H = S.Heading * Pi / 180.0;
		const Vec Along(std::sin(H), std::cos(H), 0.0);
		const double Half = std::max(0.0, S.Long * 0.5 - S.Beam * 0.5);
		Capsule C;
		C.A = Vec(S.At.X, S.At.Y, 0.0) + Along * Half;
		C.B = Vec(S.At.X, S.At.Y, 0.0) - Along * Half;
		C.R = S.Beam * 0.5;
		return C;
	}

	double PointToSegment(const Vec& P, const Vec& A, const Vec& B)
	{
		const Vec AB(B.X - A.X, B.Y - A.Y, 0.0);
		const double L2 = AB.X * AB.X + AB.Y * AB.Y;
		double U = L2 > 0.0 ? ((P.X - A.X) * AB.X + (P.Y - A.Y) * AB.Y) / L2 : 0.0;
		U = std::clamp(U, 0.0, 1.0);
		return std::hypot(P.X - (A.X + AB.X * U), P.Y - (A.Y + AB.Y * U));
	}

	bool Crosses(const Vec& A, const Vec& B, const Vec& C, const Vec& D)
	{
		auto Side = [](const Vec& P, const Vec& Q, const Vec& R) { return (Q.X - P.X) * (R.Y - P.Y) - (Q.Y - P.Y) * (R.X - P.X); };
		const double D1 = Side(A, B, C), D2 = Side(A, B, D), D3 = Side(C, D, A), D4 = Side(C, D, B);
		return ((D1 > 0) != (D2 > 0)) && ((D3 > 0) != (D4 > 0));
	}

	double SegmentToSegment(const Vec& A, const Vec& B, const Vec& C, const Vec& D)
	{
		if (Crosses(A, B, C, D)) return 0.0;
		return std::min(std::min(PointToSegment(A, C, D), PointToSegment(B, C, D)), std::min(PointToSegment(C, A, B), PointToSegment(D, A, B)));
	}

	/** How far into a hull's outline a point lies (below 0: outside it, by that much). */
	double Inside(const Vec& P, const SeaShow::ShipOut& S)
	{
		const Capsule C = HullOf(S);
		return C.R - PointToSegment(P, C.A, C.B);
	}

	const char* FxName(SeaShow::FxKind K)
	{
		switch (K)
		{
		case SeaShow::FxKind::Salvo: return "salvo";
		case SeaShow::FxKind::Splash: return "splash";
		case SeaShow::FxKind::ShellHit: return "shellhit";
		case SeaShow::FxKind::TorpedoHit: return "torpedohit";
		case SeaShow::FxKind::BombHit: return "bombhit";
		case SeaShow::FxKind::NearMiss: return "nearmiss";
		case SeaShow::FxKind::Sink: return "sink";
		case SeaShow::FxKind::LeaveLine: return "leaveline";
		case SeaShow::FxKind::Fire: return "fire";
		case SeaShow::FxKind::OilFire: return "oilfire";
		case SeaShow::FxKind::ScreenStart: return "screenstart";
		case SeaShow::FxKind::ScreenMove: return "screenmove";
		case SeaShow::FxKind::ScreenStop: return "screenstop";
		case SeaShow::FxKind::TrackStart: return "trackstart";
		case SeaShow::FxKind::TrackMove: return "trackmove";
		case SeaShow::FxKind::TrackStop: return "trackstop";
		case SeaShow::FxKind::Flak: return "flak";
		case SeaShow::FxKind::Launch: return "launch";
		case SeaShow::FxKind::LandOn: return "landon";
		case SeaShow::FxKind::PlaneDown: return "planedown";
		case SeaShow::FxKind::Ditch: return "ditch";
		case SeaShow::FxKind::Line: return "line";
		case SeaShow::FxKind::Sound: return "sound";
		case SeaShow::FxKind::Dispatch: return "dispatch";
		}
		return "?";
	}

	/** What one play of a recording saw. */
	struct Played
	{
		std::map<std::string, int> Fx;
		std::map<std::string, int> Sounds;
		double WorstFrameMs = 0.0, TotalMs = 0.0;
		std::vector<double> FrameMs;
		long Frames = 0;
		// the test's own eye on what the show drew
		int SameLineTouching = 0, OtherTouching = 0, TouchingFrames = 0;
		std::string SameLineFirst;
		std::map<std::pair<int, int>, std::pair<double, int>> TouchPairs; // first time, frames
		int SplashOnHull = 0, HitsOff = 0, HitsOnGone = 0, SalvosFromOut = 0;
		int ScreenStray = 0, TrackStray = 0, FireStray = 0;
		double MostPop = 0.0, MostPlanePop = 0.0;
		std::string PlanePopAt;
		int PlanesUnder = 0;
		int MostTorps = 0;
		int LineOff = 0, LineOffEnd = 0;
		std::string LineOffFirst;
		double HeelOut = 0.0;
		int SinkingAhead = 0;
		std::map<int, Vec> EndAt;  // each ship afloat at the end, by key
		std::string EndWords;
		SeaShow::Checks Check;
		int ColumnsPeak = 0, ColumnsMost = 0, ColumnsGiven = 0, ColumnsRefused = 0;
		Vec Wind;
		double SeaState = 0.0;
		double ContactAt = -1.0;
	};

	Played Play(const SeaFeed::Recording& Rec, double Fps, std::ofstream* Track)
	{
		Played Out;
		SeaShow::Show Show(Rec.Seed ? Rec.Seed : 239);
		Show.SetField(Rec.Board);
		const double Mid = Rec.Board.Size * 0.5;
		Show.Eye = Vec(Mid, Mid - 3000.0, 900.0);

		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& F = Rec.Views[Taken++];
			Show.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
		};
		// The game holds one view in hand ahead of what it shows, so the picture between views has somewhere to go.
		TakeNext();
		if (Taken < Rec.Views.size()) TakeNext();

		std::map<int, Vec> Last;
		std::map<int, double> LastSpeed;
		std::map<int, Vec> LastPlane;
		std::set<int> ScreensOpen, TracksOpen;
		const double Dt = 1.0 / Fps;
		double T = Rec.Views.front().Head.T;
		const double EndT = Rec.Views.back().Head.T;
		double NextSample = T;
		size_t Seen = 0; // the last view at or before the shown time
		while (true)
		{
			const auto F0 = std::chrono::steady_clock::now();
			Show.Advance(T);
			const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - F0).count();
			Out.WorstFrameMs = std::max(Out.WorstFrameMs, Ms);
			Out.TotalMs += Ms;
			Out.FrameMs.push_back(Ms);
			++Out.Frames;

			// The eye follows the fight, as the camera rig will: south of the middle of every piece, above the sea.
			{
				double X = 0.0, Y = 0.0;
				int N = 0;
				for (const auto& Pair : Show.Pieces()) { X += Pair.second.X; Y += Pair.second.Y; ++N; }
				if (N) Show.Eye = Vec(X / N, Y / N - 3000.0, 900.0);
			}

			const std::vector<SeaShow::ShipOut>& Ships = Show.Ships();
			std::map<int, const SeaShow::ShipOut*> ByKey;
			for (const SeaShow::ShipOut& S : Ships) ByKey[S.Key] = &S;

			for (const SeaShow::Fx& E : Show.Effects)
			{
				++Out.Fx[FxName(E.Kind)];
				switch (E.Kind)
				{
				case SeaShow::FxKind::Sound: ++Out.Sounds[E.What]; break;
				case SeaShow::FxKind::Splash:
					for (const SeaShow::ShipOut& S : Ships)
						if (S.Doing != SeaShow::Fate::Gone && Inside(E.At, S) > -2.0) { ++Out.SplashOnHull; break; }
					break;
				case SeaShow::FxKind::ShellHit:
				case SeaShow::FxKind::BombHit:
				case SeaShow::FxKind::TorpedoHit:
				{
					auto It = ByKey.find(E.Key);
					if (It == ByKey.end()) { ++Out.HitsOnGone; break; }
					// on her (a torpedo at her side): within her outline and a few metres
					if (Inside(E.At, *It->second) < -(E.Kind == SeaShow::FxKind::TorpedoHit ? 8.0 : 12.0)) ++Out.HitsOff;
					break;
				}
				case SeaShow::FxKind::Salvo:
				{
					auto It = ByKey.find(E.Key);
					if (It == ByKey.end() || It->second->Doing != SeaShow::Fate::Line) ++Out.SalvosFromOut;
					break;
				}
				case SeaShow::FxKind::ScreenStart:
					if (!ScreensOpen.insert(E.Key).second) ++Out.ScreenStray;
					break;
				case SeaShow::FxKind::ScreenMove:
					if (!ScreensOpen.count(E.Key)) ++Out.ScreenStray;
					break;
				case SeaShow::FxKind::ScreenStop:
					if (!ScreensOpen.erase(E.Key)) ++Out.ScreenStray;
					break;
				case SeaShow::FxKind::TrackStart:
					if (!TracksOpen.insert(E.Key).second) ++Out.TrackStray;
					break;
				case SeaShow::FxKind::TrackMove:
					if (!TracksOpen.count(E.Key)) ++Out.TrackStray;
					break;
				case SeaShow::FxKind::TrackStop:
					if (!TracksOpen.erase(E.Key)) ++Out.TrackStray;
					break;
				case SeaShow::FxKind::Fire:
					if (!ByKey.count(E.Key)) ++Out.FireStray;
					break;
				default: break;
				}
				if (Track && (E.Kind != SeaShow::FxKind::ScreenMove && E.Kind != SeaShow::FxKind::TrackMove && E.Kind != SeaShow::FxKind::Dispatch))
				{
					*Track << Num(Show.Shown(), 2) << ",fx," << E.Key << ",," << E.Side << ",," << FxName(E.Kind) << "," << Num(E.At.X, 0) << "," << Num(E.At.Y, 0) << ","
					       << Num(E.At.Z, 0) << ",,,,," << E.What << "\n";
				}
			}
			Show.Effects.clear();

			// Every ship: no pop beyond her speed, no heel past reason, two hulls never one through the other.
			bool bTouching = false;
			for (size_t I = 0; I < Ships.size(); ++I)
			{
				const SeaShow::ShipOut& S = Ships[I];
				auto Was = Last.find(S.Key);
				if (Was != Last.end() && Out.Frames > 1)
				{
					const double Allowed = (std::max(S.Speed, LastSpeed[S.Key]) * 1.2 + 1.0) * Dt + 0.05;
					Out.MostPop = std::max(Out.MostPop, Flat(S.At - Was->second) - Allowed);
				}
				Last[S.Key] = S.At;
				LastSpeed[S.Key] = S.Speed;
				if (S.Doing != SeaShow::Fate::Sinking) Out.HeelOut = std::max(Out.HeelOut, std::fabs(S.Heel));
				if (S.Doing == SeaShow::Fate::Sinking && (S.Down < 0.0 || S.Down > 1.0)) ++Out.SinkingAhead;
				const Capsule CI = HullOf(S);
				for (size_t J = I + 1; J < Ships.size(); ++J)
				{
					const SeaShow::ShipOut& O = Ships[J];
					if (std::fabs(O.At.X - S.At.X) > 300.0 || std::fabs(O.At.Y - S.At.Y) > 300.0) continue;
					const Capsule CJ = HullOf(O);
					if (SegmentToSegment(CI.A, CI.B, CJ.A, CJ.B) < CI.R + CJ.R)
					{
						bTouching = true;
						if (O.Piece == S.Piece)
						{
							auto Doing = [](SeaShow::Fate F) { return F == SeaShow::Fate::Line ? "line" : F == SeaShow::Fate::Limping ? "limping" : "sinking"; };
							if (!Out.SameLineTouching)
								Out.SameLineFirst = std::to_string(S.Key) + " (" + Doing(S.Doing) + ") and " + std::to_string(O.Key) + " (" + Doing(O.Doing) + ") at " + Num(Show.Shown()) + " s";
							++Out.SameLineTouching;
						}
						else ++Out.OtherTouching;
						auto& Pair = Out.TouchPairs[{ std::min(S.Key, O.Key), std::max(S.Key, O.Key) }];
						if (!Pair.second) Pair.first = Show.Shown();
						++Pair.second;
					}
				}
			}
			if (bTouching) ++Out.TouchingFrames;
			for (const SeaShow::PlaneOut& P : Show.Planes())
			{
				auto Was = LastPlane.find(P.Key);
				if (Was != LastPlane.end())
				{
					const double Pop = (P.At - Was->second).Length() - (P.Speed * 1.5 + 30.0) * Dt;
					if (Pop > Out.MostPlanePop)
					{
						Out.MostPlanePop = Pop;
						Out.PlanePopAt = "aircraft " + std::to_string(P.Key) + " (" + P.Model + ", " + P.Role + (P.bFalling ? ", falling" : "") + ") at " + Num(Show.Shown(), 2) + " s, " +
						                 Num(P.Speed, 0) + " m/s, " + Num(P.At.Z, 0) + " m up, moved " + Num((P.At - Was->second).Length(), 1) + " m";
					}
				}
				LastPlane[P.Key] = P.At;
				if (!P.bFalling && P.At.Z < -1.0) ++Out.PlanesUnder;
			}
			Out.MostTorps = std::max(Out.MostTorps, static_cast<int>(Show.Torpedoes().size()));

			// The ships in each piece's line: never more than the view before says, nor fewer than the view after.
			while (Seen + 1 < Rec.Views.size() && Rec.Views[Seen + 1].Head.T <= Show.Shown() + 1e-9) ++Seen;
			if (Seen + 1 < Rec.Views.size() || Show.Shown() >= EndT - 1e-9)
			{
				std::map<int, int> InLine;
				for (const SeaShow::ShipOut& S : Ships)
					if (S.Doing == SeaShow::Fate::Line) ++InLine[S.Piece];
				const SeaFeed::Frame& A = Rec.Views[Seen];
				const SeaFeed::Frame& B = Rec.Views[std::min(Seen + 1, Rec.Views.size() - 1)];
				std::map<int, int> After;
				for (const SeaShow::Piece& P : B.Pieces) After[P.Id] = P.Ships;
				for (const SeaShow::Piece& P : A.Pieces)
				{
					const int Hi = P.Ships, Lo = After.count(P.Id) ? std::min(P.Ships, After[P.Id]) : P.Ships;
					const int Has = InLine.count(P.Id) ? InLine[P.Id] : 0;
					if (Has < Lo || Has > Hi)
					{
						if (!Out.LineOff) Out.LineOffFirst = "piece " + std::to_string(P.Id) + " at " + Num(Show.Shown()) + " s: " + std::to_string(Has) + " in line, the battle " + std::to_string(Lo) + " to " + std::to_string(Hi);
						++Out.LineOff;
						if (Show.Shown() >= EndT - 1e-9) ++Out.LineOffEnd;
					}
				}
			}

			if (Track && Show.Shown() >= NextSample - 1e-9)
			{
				NextSample += 1.0;
				for (const SeaShow::ShipOut& S : Ships)
				{
					const char* Doing = S.Doing == SeaShow::Fate::Line ? "line" : S.Doing == SeaShow::Fate::Limping ? "limping" : S.Doing == SeaShow::Fate::Sinking ? "sinking" : "gone";
					*Track << Num(Show.Shown(), 2) << ",ship," << S.Key << "," << S.Piece << "," << S.Side << "," << S.Class << "," << S.Nation << "," << Num(S.At.X, 1) << ","
					       << Num(S.At.Y, 1) << "," << Num(S.At.Z, 1) << "," << Num(S.Heading, 1) << "," << Num(S.Heel, 2) << "," << Num(S.Speed, 1) << "," << Doing << ","
					       << (S.bFiring ? "fire" : "") << (S.bScreen ? " smoke" : "") << (S.Doing == SeaShow::Fate::Sinking ? " " + S.How : std::string()) << "\n";
				}
				for (const SeaShow::PlaneOut& P : Show.Planes())
				{
					*Track << Num(Show.Shown(), 2) << ",plane," << P.Key << ",," << P.Side << "," << P.Role << "," << P.Model << "," << Num(P.At.X, 0) << "," << Num(P.At.Y, 0) << ","
					       << Num(P.At.Z, 0) << "," << Num(P.Heading, 0) << "," << Num(P.Bank, 0) << "," << Num(P.Speed, 0) << "," << (P.bFalling ? "falling" : "") << ",\n";
				}
				for (const SeaShow::TorpOut& O : Show.Torpedoes())
				{
					*Track << Num(Show.Shown(), 2) << ",torp," << O.Key << ",," << O.Side << ",,," << Num(O.At.X, 0) << "," << Num(O.At.Y, 0) << ",0," << Num(O.Heading * 180.0 / Pi, 0)
					       << ",," << Num(O.Speed, 0) << ",,\n";
				}
			}

			if (T >= EndT - 1e-9) break;
			T = std::min(EndT, T + Dt);
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
		}

		for (const SeaShow::ShipOut& S : Show.Ships())
			if (S.Doing != SeaShow::Fate::Sinking) Out.EndAt[S.Key] = S.At;
		Out.EndWords = Show.Describe();
		Out.Check = Show.Checked();
		Out.ColumnsPeak = Show.Columns.Peak;
		Out.ColumnsMost = Show.Columns.Most;
		Out.ColumnsGiven = Show.Columns.Given;
		Out.ColumnsRefused = Show.Columns.Refused;
		Out.Wind = Show.Wind();
		Out.SeaState = Show.SeaState();
		Out.ContactAt = Show.ContactAt();
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: sea_show_test <recording.json> [out-dir] [frames-per-second]\n");
		return 2;
	}
	const std::string Path = Argv[1];
	const std::string OutDir = Argc > 2 ? Argv[2] : ".";
	const double Fps = Argc > 3 ? std::atof(Argv[3]) : 60.0;

	std::ifstream In(Path, std::ios::binary);
	if (!In)
	{
		std::fprintf(stderr, "cannot read %s\n", Path.c_str());
		return 2;
	}
	std::stringstream Buffer;
	Buffer << In.rdbuf();
	SeaFeed::Recording Rec;
	std::string Error;
	const auto Read0 = std::chrono::steady_clock::now();
	if (!SeaFeed::ReadRecording(Buffer.str(), Rec, Error))
	{
		std::fprintf(stderr, "%s: %s\n", Path.c_str(), Error.c_str());
		return 2;
	}
	const double ReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Read0).count();

	// What the battle said, for the show to be held to.
	int Events = 0, Told = 0, LineTurns = 0, Sunk = 0, Burning = 0, TorpSalvos = 0, TorpFull = 0, TorpHits = 0, Strikes = 0, StrikesLanded = 0, Raids = 0;
	bool bGunfire = false;
	std::set<int> Pieces;
	for (const SeaFeed::Frame& F : Rec.Views)
	{
		for (const SeaShow::Piece& P : F.Pieces)
		{
			Pieces.insert(P.Id);
			if (P.Engaged >= 0) bGunfire = true;
		}
		for (const SeaShow::Event& E : F.Events)
		{
			++Events;
			if (!E.Text.empty()) ++Told;
			if (E.What == "sea:sunk") ++Sunk;
			else if (E.What == "sea:burning") ++Burning;
			else if (E.What == "sea:torpedo") { ++TorpSalvos; if (!E.bShare || E.Share > 0.0) ++TorpFull; }
			else if (E.What == "sea:torpedoHit") ++TorpHits;
			else if (E.What == "ability" && E.Key == "strike") ++Strikes;
			else if (E.What == "sea:strike" && E.Landed > 0.0) ++StrikesLanded;
			else if (E.What == "sea:air") ++Raids;
			else if (E.What == "sea:line") ++LineTurns;
		}
	}
	const SeaFeed::Frame& Last = Rec.Views.back();
	std::map<int, int> EndShips;
	for (const SeaShow::Piece& P : Last.Pieces) EndShips[P.Id] = P.Ships;
	int ShipsAtStart = 0;
	for (const SeaShow::Piece& P : Rec.Views.front().Pieces) ShipsAtStart += P.Ships;

	std::printf("%s: %s (%s), %zu views, %.0f battle s, %zu pieces, %d ships; %d events: %d sunk, %d burning, %d torpedo salvos (%d with tracks, %d hits), %d strikes (%d landed), %d raids\n",
	            Rec.Key.c_str(), Rec.Board.Title.c_str(), Rec.Board.Date.c_str(), Rec.Views.size(), Last.Head.T - Rec.Views.front().Head.T, Pieces.size(), ShipsAtStart, Events, Sunk, Burning,
	            TorpSalvos, TorpFull, TorpHits, Strikes, StrikesLanded, Raids);
	std::printf("  read in %.0f ms; the winner %s (%s)\n", ReadMs, Rec.Card.Winner.c_str(), Rec.Card.Level.c_str());

	std::ofstream Track(OutDir + "/" + Rec.Key + "-sea-track.csv");
	Track << "t,kind,key,piece,side,class,nation,x,y,z,heading,heel,speed,doing,extra\n";
	const Played P = Play(Rec, Fps, &Track);
	const Played Again = Play(Rec, Fps, nullptr);
	const Played Half = Play(Rec, Fps * 0.5, nullptr);
	const SeaShow::Checks& C = P.Check;

	std::printf("  effects:");
	for (const auto& Pair : P.Fx) std::printf(" %s %d", Pair.first.c_str(), Pair.second);
	std::printf("\n  sounds:");
	for (const auto& Pair : P.Sounds) std::printf(" %s %d", Pair.first.c_str(), Pair.second);
	std::printf("\n  ways down:");
	for (const auto& Pair : C.SinksBy) std::printf(" %s %d", Pair.first.c_str(), Pair.second);
	const double WindFrom = std::fmod(std::atan2(-P.Wind.X, -P.Wind.Y) + 2.0 * Pi, 2.0 * Pi);
	std::printf("\n  wind from %.0f deg at %.1f m/s, sea state %.0f; contact at %.0f s; columns given %d, refused %d, %d at once at the most\n", WindFrom * 180.0 / Pi,
	            Flat(P.Wind), P.SeaState, P.ContactAt, P.ColumnsGiven, P.ColumnsRefused, P.ColumnsPeak);
	std::printf("  hits owed by gunfire %.1f, left unshown %.1f; screens %d (%d at once), tracks %d (%d at once), aircraft up %d at the most, cover up %d\n", C.OwedAll, C.OwedLeft,
	            C.Screens, C.MostScreens, C.TracksRun, C.MostTracks, C.MostPlanes, C.CoverUp);
	std::printf("  followers giving way to their own line crossing ahead: %.1f s\n", C.GaveWay);
	std::printf("  worst turn: %s; rudder: %s; heel: %s; lead: %s; off the track: %s; stations kept up to %.0f m off the battle's place\n", C.TurnWorst.c_str(),
	            C.RudderWorst.c_str(), C.HeelWorst.c_str(), C.LeadWorst.c_str(), C.GapWorst.c_str(), C.MostKeep);
	if (P.MostPlanePop > 0.0) std::printf("  worst aircraft step: %s\n", P.PlanePopAt.c_str());
	if (!C.OwedBy.empty())
	{
		std::printf("  hits owed at the end:");
		for (const auto& Pair : C.OwedBy) std::printf(" piece %d %.1f", Pair.first, Pair.second);
		std::printf("\n");
	}
	if (!P.TouchPairs.empty())
	{
		std::printf("  hulls touching (%zu pairs, %d frames):", P.TouchPairs.size(), P.TouchingFrames);
		int Shown = 0;
		for (const auto& Pair : P.TouchPairs)
		{
			if (++Shown > 8) { std::printf(" ..."); break; }
			std::printf(" %d/%d (%.0f s, %d)", Pair.first.first, Pair.first.second, Pair.second.first, Pair.second.second);
		}
		std::printf("\n");
	}
	if (std::getenv("SEA_SHOW_TRACE")) std::printf("%s", P.EndWords.c_str());

	// The same battle at half the frame rate: every ship afloat at the end in the same place, near enough.
	double MostDrift = 0.0;
	int Missing = 0;
	for (const auto& Pair : P.EndAt)
	{
		auto It = Half.EndAt.find(Pair.first);
		if (It == Half.EndAt.end()) { ++Missing; continue; }
		MostDrift = std::max(MostDrift, Flat(It->second - Pair.second));
	}

	std::vector<Limit> Limits;
	// L1: the line ahead, steamed as ships steam.
	Limits.push_back({ "every ship's bow on her way (slip)", C.MostSlip <= 2.0, Num(C.MostSlip, 2) + " deg", "2 deg or less" });
	Limits.push_back({ "no ship turns tighter than her class", C.MostTurn <= 1.01, Num(C.MostTurn, 3) + " of the limit", "1 or less" });
	Limits.push_back({ "no rudder thrown over faster than a ship answers", C.MostRudder <= 1.05, Num(C.MostRudder, 3) + " of the limit", "1.05 or less" });
	Limits.push_back({ "each leader on his station (the battle's place, the zig-zag, keeping clear)", C.MostLead <= 500.0, Num(C.MostLead, 0) + " m", "500 m or less" });
	// (turning in succession, a ship astern cut inside or swung outside her leader's turn by a third of her length or so)
	Limits.push_back({ "followers in their leader's wake (once closed up)", C.MostGap <= 40.0, Num(C.MostGap, 1) + " m off his track", "40 m or less" });
	Limits.push_back({ "no ship pops (the show's own count)", C.MostJump <= 0.5, Num(C.MostJump, 2) + " m", "0.5 m or less" });
	Limits.push_back({ "no ship pops (the test's own eye, every ship)", P.MostPop <= 0.5, Num(P.MostPop, 2) + " m", "0.5 m or less" });
	// (a destroyer hard over at her best speed heeled 10 to 12 degrees, and rolled 5 or so more on a fresh sea)
	Limits.push_back({ "heel in a turn and on the swell, afloat", P.HeelOut <= 18.0 && C.MostHeel <= 18.0, Num(std::max(P.HeelOut, C.MostHeel), 1) + " deg", "18 deg or less" });
	Limits.push_back({ "no two ships of a line one through the other", P.SameLineTouching == 0,
	                   P.SameLineTouching ? std::to_string(P.SameLineTouching) + " pair-frames (first " + P.SameLineFirst + ")" : "0", "0" });
	Limits.push_back({ "the line's ships as many as the battle's", P.LineOff == 0, P.LineOff ? std::to_string(P.LineOff) + " frames off (" + P.LineOffFirst + ")" : "always",
	                   "always between the view before and after" });
	Limits.push_back({ "salvos only from ships in the line", (!bGunfire || C.Salvos > 0) && P.SalvosFromOut == 0,
	                   std::to_string(C.Salvos) + " salvos, " + std::to_string(P.SalvosFromOut) + " from out", "some where the guns met, none from out" });
	Limits.push_back({ "every splash in the sea, clear of the hulls", P.SplashOnHull == 0, std::to_string(P.SplashOnHull) + " on a hull", "0" });
	Limits.push_back({ "every hit on the ship it strikes", P.HitsOff == 0 && P.HitsOnGone == 0, std::to_string(P.HitsOff) + " off, " + std::to_string(P.HitsOnGone) + " on none", "0" });
	double MostOwed = 0.0;
	for (const auto& Pair : C.OwedBy) MostOwed = std::max(MostOwed, Pair.second);
	Limits.push_back({ "the hull the battle took shown as hits", MostOwed < 1.5,
	                   Num(C.OwedLeft, 1) + " of " + Num(C.OwedAll, 1) + " left, " + Num(MostOwed, 1) + " on one piece", "under 1.5 on any piece at the end" });
	Limits.push_back({ "the columns of water within the kit's room", P.ColumnsPeak <= P.ColumnsMost, std::to_string(P.ColumnsPeak), "the room's most or fewer" });
	// L3, L4: the losses, where the battle has them.
	Limits.push_back({ "every ship sunk goes down", C.Sinks == Sunk && C.SunkEvents == Sunk && C.SunkUnmatched == 0,
	                   std::to_string(C.Sinks) + " of " + std::to_string(Sunk) + " (" + std::to_string(C.SunkUnmatched) + " unmatched)", "all" });
	Limits.push_back({ "every ship burning leaves her line and limps", C.Leaves == Burning && C.BurningUnmatched == 0,
	                   std::to_string(C.Leaves) + " of " + std::to_string(Burning) + " (" + std::to_string(C.BurningUnmatched) + " unmatched)", "all" });
	Limits.push_back({ "every torpedo salvo with tracks runs them", C.TorpedoEvents == TorpSalvos && (TorpFull == 0 || C.TracksRun >= TorpFull),
	                   std::to_string(C.TracksRun) + " tracks for " + std::to_string(TorpFull) + " salvos", "one or more each" });
	Limits.push_back({ "every torpedo hit ends a track at the ship's side", C.TorpedoHits == TorpHits && C.TorpedoHitUnmatched == 0,
	                   std::to_string(C.TorpedoHits) + " of " + std::to_string(TorpHits), "all" });
	Limits.push_back({ "torpedo tracks within the room", P.MostTorps <= SeaShow::Show::TrackRoom && P.TrackStray == 0, std::to_string(P.MostTorps) + " at once, " + std::to_string(P.TrackStray) + " stray", "the room or fewer, 0 stray" });
	// L2: the carriers' strikes.
	Limits.push_back({ "every strike flies off its carrier", Strikes == 0 || (C.StrikeEvents == Strikes && C.Launches >= 4 * Strikes),
	                   std::to_string(C.Launches) + " launched for " + std::to_string(Strikes), "4 or more a strike" });
	Limits.push_back({ "every strike that lands hits", StrikesLanded == 0 || C.BombHits >= StrikesLanded, std::to_string(C.BombHits) + " hits for " + std::to_string(StrikesLanded) + " landed",
	                   "one or more each" });
	Limits.push_back({ "the strikes fly home and land on", Strikes == 0 || C.Landings > 0, std::to_string(C.Landings) + " landed on", "some" });
	Limits.push_back({ "no aircraft under the sea or popping", P.PlanesUnder == 0 && P.MostPlanePop <= 1.0,
	                   std::to_string(P.PlanesUnder) + " under, " + Num(P.MostPlanePop, 1) + " m pop", "0 under, 1 m or less" });
	// L5, L6, L7: smoke, wind and the zig-zag.
	Limits.push_back({ "smoke screens start, move and stop in order", P.ScreenStray == 0 && P.FireStray == 0, std::to_string(P.ScreenStray + P.FireStray) + " stray", "0" });
	if (Rec.Board.WindSpeed >= 0.0)
	{
		const double Off = std::fabs(std::remainder(WindFrom - Rec.Board.WindFrom, 2.0 * Pi)) * 180.0 / Pi;
		Limits.push_back({ "the battle's own wind", Off < 0.5 && std::fabs(Flat(P.Wind) - Rec.Board.WindSpeed) < 0.05,
		                   Num(WindFrom * 180.0 / Pi, 0) + " deg at " + Num(Flat(P.Wind), 1) + " (the field: " + Num(Rec.Board.WindFrom * 180.0 / Pi, 0) + " at " + Num(Rec.Board.WindSpeed, 1) + ")",
		                   "the field's" });
	}
	Limits.push_back({ "the zig-zag steadied a minute after contact", C.MostZigAfter <= 1.0, Num(C.MostZigAfter, 2) + " m", "1 m or less" });
	// The whole show.
	{
		// Brian, 21 November: the odds are the rules' (not every shell hits), and a hit always shows on what it hit (the
		// hits-on-their-ship check above).
		const int Salvos = P.Fx.count("salvo") ? P.Fx.at("salvo") : 0, Hits = P.Fx.count("shellhit") ? P.Fx.at("shellhit") : 0;
		Limits.push_back({ "most salvos miss (hits only as the battle books them)", Salvos == 0 || Hits * 5 <= Salvos,
		                   std::to_string(Hits) + " hits for " + std::to_string(Salvos) + " salvos", "a fifth or fewer" });
	}
	Limits.push_back({ "every battle line formed or turned told to the camera", (P.Fx.count("line") ? P.Fx.at("line") : 0) == LineTurns,
	                   std::to_string(P.Fx.count("line") ? P.Fx.at("line") : 0) + " of " + std::to_string(LineTurns), "all" });
	Limits.push_back({ "every event told (its words)", P.Fx.count("dispatch") && P.Fx.at("dispatch") == Told,
	                   std::to_string(P.Fx.count("dispatch") ? P.Fx.at("dispatch") : 0) + " of " + std::to_string(Told), "all with words" });
	Limits.push_back({ "the same show every time", P.EndWords == Again.EndWords, P.EndWords == Again.EndWords ? "same" : "different", "same" });
	Limits.push_back({ "the same show at half the frame rate", Missing == 0 && MostDrift <= 25.0, Num(MostDrift, 1) + " m apart, " + std::to_string(Missing) + " missing", "25 m or less" });
	// (the slowest frame in a thousand, as the machine's own hiccups make the very worst one meaningless)
	std::vector<double> Times = P.FrameMs;
	std::sort(Times.begin(), Times.end());
	const double Slow = Times.empty() ? 0.0 : Times[std::min(Times.size() - 1, static_cast<size_t>(Times.size() * 0.999))];
	Limits.push_back({ "a frame within budget", Slow < 1.0 && P.TotalMs / std::max(1L, P.Frames) < 0.25,
	                   Num(Slow, 3) + " ms (one in a thousand), " + Num(P.TotalMs / std::max(1L, P.Frames), 3) + " ms mean, " + Num(P.WorstFrameMs, 2) + " worst",
	                   "under 1 ms one in a thousand, 0.25 mean" });

	int Failed = 0;
	for (const Limit& L : Limits)
	{
		std::printf("  %s %-58s %s (%s)\n", L.bOk ? "ok  " : "FAIL", L.Name, L.Got.c_str(), L.Want);
		if (!L.bOk) ++Failed;
	}
	std::printf("  hulls of two pieces touching: %d pair-frames, %zu pairs (shown, not held: the battle sets the pieces' places)\n", P.OtherTouching, P.TouchPairs.size());
	std::printf("%s\n", Failed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failed ? 1 : 0;
}
