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
		case SeaShow::FxKind::Gun: return "gun";
		case SeaShow::FxKind::GunStop: return "gunstop";
		case SeaShow::FxKind::ShipGun: return "shipgun";
		case SeaShow::FxKind::PlaneHit: return "planehit";
		case SeaShow::FxKind::ChargeDrop: return "chargedrop";
		case SeaShow::FxKind::ChargeBurst: return "chargeburst";
		case SeaShow::FxKind::Dive: return "dive";
		case SeaShow::FxKind::Starshell: return "starshell";
		case SeaShow::FxKind::Rescue: return "rescue";
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
		double MostPitch[3] = { 0.0, 0.0, 0.0 }, MostHeave[3] = { 0.0, 0.0, 0.0 }; // the ride on the sea: under 145 m, to 215 m, over
		double MostBoatAngle = 0.0, MostBoatKeel = 0.0; // a convoy's boats (ruling 241): their angle and the deepest keel drawn
		int BoatOdd = 0;                                // boat-frames with no state or a keel above the sea
		int BoatSwings = 0;                             // a boat's angle swinging 4 degrees one way or the other (a dive is two: down, level)
		std::map<int, std::pair<double, int>> BoatSwing; // by boat: the last extreme of her angle, which way it is going (+1, -1, 0)
		int ChargesOffBoat = 0;                         // depth charges booked on a boat that burst out of her reach
		// Enemy boats the player has not found (SetMine, the views' "me"): what the show hid, and anything of hers out while
		// it did; against the recording's own finds (FindsOf): shown before anything could find her, hidden while held.
		int EnemyBoats = 0;
		long UnseenFrames = 0;
		int Reveals = 0, Hides = 0;
		int WrongHidden = 0, HiddenOut = 0, HiddenMarks = 0; // hidden and not an enemy boat; hidden out of the fight; with a wake or smoke
		int HitsUnseen = 0, GunsUnseen = 0, HeardUnseen = 0; // a blow on her, a gun of hers, a cue of hers, while hidden
		int HeldHidden = 0, ShownUnfound = 0;                // hidden while an escort held her; shown before anything could find her
		int ShownStale = 0;                                  // shown under, with nothing finding her for FoundFor and more
		std::string HeldHiddenFirst, ShownUnfoundFirst, ShownStaleFirst;
		std::map<int, double> UnderSince, BlowAt;            // by key: under since (the show's BoatUnder), and the last blow on her
		std::map<int, bool> WasShown;                        // by key: shown in the frame before
		struct Sighting
		{
			double T = 0.0;
			int Key = 0;
			bool bShown = false;
		};
		std::vector<Sighting> Sightings;                     // each change of a ship's bShown (and a ship first drawn hidden)
		// where ships went down (Brian, 22 November 05:32Z): men in the water and wreckage never inside a ship, no man popping
		int MenInHull = 0, FloatsInHull = 0;
		std::string MenInHullFirst;
		double MostManPop = 0.0;
		std::string ManPopAt;
		int MostMenSeen = 0, MostFloatsSeen = 0;
		int SinkingAhead = 0;
		std::map<int, Vec> EndAt;  // each ship afloat at the end, by key
		std::string EndWords;
		SeaShow::Checks Check;
		ShipGuns::Tally Guns;                 // L9: the ships' anti-aircraft guns' own count
		std::set<int> OpenStreams;            // streams fired and not yet stopped
		int StreamsOpenAtEnd = 0, PlanesUpAtEnd = 0;
		int ColumnsPeak = 0, ColumnsMost = 0, ColumnsGiven = 0, ColumnsRefused = 0;
		Vec Wind;
		double SeaState = 0.0;
		double ContactAt = -1.0;
	};

	/** What the battle told that finds each enemy boat of the player (Mine), read from the recording alone, as the rule has
	 * it (SeaShow::Show::SetMine) and not as the show keeps it: when anything could first have found her (a contact, a blow,
	 * aircraft on her, guns laid on her or hers, forced up, given up or gone); the spans an escort of the other side held
	 * her (the views' heard where her view, as the show has it when each event plays, gives it; else each escort's contact
	 * from its gain to its loss, its end or hers, or her view beginning to say heard, however long it goes without an
	 * attack: the battle closes every contact so; only one that has made an ahead-thrown run on her, which sends no loss,
	 * ends FoundFor after its last attack);
	 * every moment the rule may count FoundFor on from (each find, each loss and each span's end, as late as each can come:
	 * aircraft over their run, a booked charge still sinking to her); and when she was given up (in sight to her end). */
	struct Finds
	{
		std::map<int, double> First;                                // by boat piece
		std::map<int, std::vector<std::pair<double, double>>> Held; // by boat piece
		std::map<int, std::vector<double>> Found;                   // by boat piece, in order
		std::map<int, double> Given;                                // by boat piece
	};

	Finds FindsOf(const SeaFeed::Recording& Rec, int Mine)
	{
		Finds Out;
		if (Mine < 0 || Rec.Views.empty()) return Out;
		const double For = SeaShow::Show::FoundFor;
		const SeaShow::Field& Board = Rec.Board;
		std::map<int, int> Sides;
		std::map<int, std::string> Classes;
		for (const SeaFeed::Frame& F : Rec.Views)
		{
			for (const SeaShow::Piece& P : F.Pieces)
			{
				Sides[P.Id] = P.Side;
				Classes[P.Id] = P.Class;
			}
		}
		auto Enemy = [&](int Id) { return Id >= 0 && Classes.count(Id) && Classes[Id] == "ss" && Sides[Id] != Mine; };
		auto Note = [&Out, &Enemy](int Id, double T)
		{
			if (!Enemy(Id)) return;
			auto It = Out.First.find(Id);
			if (It == Out.First.end() || T < It->second) Out.First[Id] = T;
			Out.Found[Id].push_back(T);
		};
		// (a moment the rule may count FoundFor on from that finds nothing first: a loss, the end of a run over her)
		auto Later = [&Out, &Enemy](int Id, double T)
		{
			if (Enemy(Id)) Out.Found[Id].push_back(T);
		};
		struct HeldSpan
		{
			double Since = 0.0, Word = 0.0; // held since, and its last word on her (a gain, or its charges on her)
			bool bAhead = false;            // an ahead-thrown run of its on her since its gain (no loss told after one)
		};
		using Spans = std::map<std::pair<int, int>, HeldSpan>; // (escort, boat)
		Spans Open;
		auto Close = [&Out, &Open](Spans::iterator It, double T)
		{
			Out.Held[It->first.second].push_back({ It->second.Since, T });
			Out.Found[It->first.second].push_back(T);
			return Open.erase(It);
		};
		// (one that has made an ahead-thrown run on her, with no word since its last attack for FoundFor, has held her its
		// while; any other holds her until its loss, its end or hers)
		auto Expire = [&Open, &Close, For](double T)
		{
			for (auto It = Open.begin(); It != Open.end();)
				It = It->second.bAhead && It->second.Word + For < T ? Close(It, It->second.Word + For) : std::next(It);
		};
		std::map<int, std::vector<std::pair<double, double>>> Drops; // escort: its patterns, dropped when and burst by when at the latest
		std::map<int, double> HeardSince;                            // boat: held by her heard since
		std::map<int, bool> HeardWas;                                // by piece: whether its view said heard, as of the view before
		for (const SeaFeed::Frame& F : Rec.Views)
		{
			const double T = F.Head.T;
			std::map<int, const SeaShow::Piece*> Here;
			std::map<int, bool> HeardNow;
			for (const SeaShow::Piece& P : F.Pieces)
			{
				if (P.Ships > 0) Here[P.Id] = &P;
				HeardNow[P.Id] = P.bHeard;
			}
			// (her view as the show has it when an event of this view plays: this view at its own moment, else the one before)
			auto ByHeard = [&HeardNow, &HeardWas, T](const SeaShow::Event& E, int Boat)
			{
				const std::map<int, bool>& As = E.T >= T - 1e-6 ? HeardNow : HeardWas;
				const auto It = As.find(Boat);
				return It != As.end() && It->second;
			};
			for (const SeaShow::Event& E : F.Events)
			{
				Expire(E.T);
				const bool bFoe = Sides.count(E.Unit) && Enemy(E.On) && Sides[E.Unit] != Sides[E.On];
				if (E.What == "sea:depthCharge" && E.Kind != "ahead")
				{
					// (its charges set where the battle says, else as deep as they go; at least 3 m/s over the spread)
					const double Set = E.Set >= 0.0 ? E.Set : Board.CrushKeel;
					const double Sinks = E.Sinking >= 0.0 ? E.Sinking : Set / std::max(0.5, Board.ChargeSink);
					Drops[E.Unit].push_back({ E.T, E.T + (E.Spread > 0.0 ? E.Spread : 120.0) / 3.0 + Sinks });
				}
				if (E.What == "sea:sonar" && E.Kind != "bearing" && bFoe)
				{
					auto It = Open.find(std::make_pair(E.Unit, E.On));
					// (a loss lets go of what this escort's events held, whichever way she is held as it is told)
					if (!E.Lost.empty())
					{
						Later(E.On, E.T);
						if (It != Open.end()) Close(It, E.T);
						continue;
					}
					Note(E.On, E.T);
					if (ByHeard(E, E.On)) continue;
					if (It == Open.end()) Open[std::make_pair(E.Unit, E.On)] = { E.T, E.T, false };
					else It->second = { It->second.Since, E.T, false };
				}
				else if (E.What == "sea:sunk")
				{
					Note(E.Unit, E.T);
					for (auto It = Open.begin(); It != Open.end();)
						It = It->first.first == E.Unit || It->first.second == E.Unit ? Close(It, E.T) : std::next(It);
				}
				else if (E.What == "sea:surrender")
				{
					Note(E.Unit, E.T);
					if (Enemy(E.Unit) && !Out.Given.count(E.Unit)) Out.Given[E.Unit] = E.T;
				}
				else if (E.What == "sea:abandon" || E.What == "sea:torpedoHit")
					Note(E.Unit, E.T);
				else if (E.What == "sea:depthChargeHit")
				{
					Note(E.Unit, E.T);
					// (its blow is the one of that escort's charges bursting nearest: one of a pattern dropped up to two
					// minutes before bursts at its own time, as late as it can sink)
					double Due = E.T;
					const auto Of = Drops.find(E.From);
					if (Of != Drops.end())
					{
						for (const auto& Drop : Of->second)
							if (E.T - Drop.first <= 120.0 + 1e-6) Due = std::max(Due, Drop.second);
					}
					Later(E.Unit, Due);
				}
				else if (E.What == "sea:air")
				{
					// (the aircraft have her over their run as the show flies it, and their blow comes at its end)
					Note(E.Unit, E.T);
					Later(E.Unit, E.T + std::clamp(E.Running > 0.0 ? E.Running : 8.0, 4.0, 60.0));
				}
				else if (E.What == "ability" && E.Key == "strike")
				{
					// (a carrier's strike on her: its blow comes as it lands, its windup on and its run in)
					Note(E.Target, E.T);
					Later(E.Target, E.T + Board.StrikeWindup + 60.0);
				}
				else if (E.What == "sea:gunHit")
				{
					Note(E.Unit, E.T);
					Note(E.From, E.T);
				}
				else if (E.What == "sea:dive" && E.Why == "forced")
					Note(E.Unit, E.T);
				else if (E.What == "sea:depthCharge" && bFoe)
				{
					Note(E.On, E.T);
					// (an escort holding her by its events has word of her in its attack; after an ahead-thrown run its
					// hold ends FoundFor after its last attack, unless it tells more)
					auto It = Open.find(std::make_pair(E.Unit, E.On));
					if (It != Open.end()) It->second = { It->second.Since, E.T, It->second.bAhead || E.Kind == "ahead" };
				}
			}
			Expire(T);
			// (an escort, or the boat, gone from the fight lets the contact go; and her view beginning to say heard lets go
			// of what the escorts' events held, as the show's Hunt does)
			for (auto It = Open.begin(); It != Open.end();)
			{
				const auto Says = HeardNow.find(It->first.second);
				const bool bSays = Says != HeardNow.end() && Says->second;
				It = !Here.count(It->first.first) || !Here.count(It->first.second) || bSays ? Close(It, T) : std::next(It);
			}
			for (const SeaShow::Piece& P : F.Pieces)
			{
				if (P.Engaged >= 0)
				{
					Note(P.Id, T);
					Note(P.Engaged, T);
				}
				// (a deck gun laid: hers on a ship of his, or a ship of his, an escort's or an armed merchant's, on her)
				if (P.GunOn >= 0)
				{
					Note(P.Id, T);
					Note(P.GunOn, T);
				}
				if (!Enemy(P.Id) || !P.bHeard) continue;
				bool bHeld = false;
				for (const int By : P.Heard)
				{
					auto Es = Here.find(By);
					if (Es == Here.end() || Es->second->Side == P.Side) continue;
					if (Es->second->EchoOn == P.Id && Es->second->EchoKind == "bearing") continue;
					bHeld = P.Ships > 0;
					break;
				}
				auto Since = HeardSince.find(P.Id);
				if (bHeld && Since == HeardSince.end())
				{
					HeardSince[P.Id] = T;
					Note(P.Id, T);
				}
				else if (!bHeld && Since != HeardSince.end())
				{
					Out.Held[P.Id].push_back({ Since->second, T });
					Later(P.Id, T);
					HeardSince.erase(Since);
				}
			}
			HeardWas = HeardNow;
		}
		const double EndT = Rec.Views.back().Head.T;
		Expire(EndT);
		for (auto It = Open.begin(); It != Open.end();) It = Close(It, EndT);
		for (const auto& Pair : HeardSince) Out.Held[Pair.first].push_back({ Pair.second, EndT });
		for (auto& Pair : Out.Found) std::sort(Pair.second.begin(), Pair.second.end());
		return Out;
	}

	Played Play(const SeaFeed::Recording& Rec, double Fps, std::ofstream* Track, int Mine)
	{
		Played Out;
		SeaShow::Show Show(Rec.Seed ? Rec.Seed : 239);
		Show.SetField(Rec.Board);
		// (the player's side as the game takes it, ASeaBattle::OpenOn: an enemy boat drawn only once found)
		Show.SetMine(Mine);
		const Finds Oracle = FindsOf(Rec, Mine);
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
		std::vector<Vec> LastMan;   // by key (the show's men are numbered from 1)
		std::vector<long> LastManFrame;
		std::vector<SeaShow::Swim> LastManDoing;
		double NextWreckLook = 0.0;
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
			// (the ships hidden this frame, and their pieces: nothing of theirs may come out while they are)
			auto Hidden = [&ByKey](int Key) { auto It = ByKey.find(Key); return It != ByKey.end() && !It->second->bShown; };
			std::set<int> HiddenPieces;
			for (const SeaShow::ShipOut& S : Ships)
				if (!S.bShown) HiddenPieces.insert(S.Piece);

			for (const SeaShow::Fx& E : Show.Effects)
			{
				++Out.Fx[FxName(E.Kind)];
				switch (E.Kind)
				{
				case SeaShow::FxKind::ShellHit:
				case SeaShow::FxKind::BombHit:
				case SeaShow::FxKind::TorpedoHit:
					if (Hidden(E.Key)) ++Out.HitsUnseen;
					Out.BlowAt[E.Key] = Show.Shown(); // (a blow finds her: the show's own shell, bomb or torpedo on her)
					break;
				case SeaShow::FxKind::ChargeBurst:
					// (a charge the battle booked on a boat, bursting on her: a blow that finds her where it bursts)
					if (E.Size > 0.5)
					{
						for (const SeaShow::ShipOut& S : Ships)
						{
							if (S.Class != "ss" || std::hypot(S.At.X - E.At.X, S.At.Y - E.At.Y) > Rec.Board.ChargeLethal + 2.0) continue;
							Out.BlowAt[S.Key] = Show.Shown();
							// (the blow finds her where it bursts: in sight in the frame it bursts in)
							if (!S.bShown) ++Out.HitsUnseen;
						}
					}
					break;
				case SeaShow::FxKind::Salvo:
				case SeaShow::FxKind::ShipGun:
					if (Hidden(E.Key)) ++Out.GunsUnseen;
					break;
				case SeaShow::FxKind::Gun:
					if (ShipGuns::IsStream(E.Key) && Hidden((E.Key - ShipGuns::StreamBase) / 64)) ++Out.GunsUnseen;
					break;
				case SeaShow::FxKind::Sound:
					// (her own cue keyed to her piece; an escort's asdic finding her is the escort's, and finds her)
					if (E.What != "sea_asdic_ping" && HiddenPieces.count(E.Key)) ++Out.HeardUnseen;
					break;
				default: break;
				}
				switch (E.Kind)
				{
				case SeaShow::FxKind::Sound: ++Out.Sounds[E.What]; break;
				case SeaShow::FxKind::Gun: Out.OpenStreams.insert(E.Key); break;
				case SeaShow::FxKind::GunStop: Out.OpenStreams.erase(E.Key); break;
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
				case SeaShow::FxKind::ChargeBurst:
					// (the blow the battle booked bursts within a charge's lethal reach of a boat, at her depth or near it)
					if (E.Size > 0.5)
					{
						bool bNear = false;
						for (const SeaShow::ShipOut& S : Ships)
							if (S.Class == "ss" && std::hypot(S.At.X - E.At.X, S.At.Y - E.At.Y) <= Rec.Board.ChargeLethal + 2.0 && std::fabs(-E.At.Z - S.Keel) <= 15.0) bNear = true;
						if (!bNear) ++Out.ChargesOffBoat;
					}
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
				// Whom the player sees (SetMine): only an enemy boat in the fight hidden, with nothing of her on the sea; never
				// shown before the battle told anything that could find her, never hidden while an escort held her.
				{
					const bool bEnemyBoat = Mine >= 0 && S.Class == "ss" && S.Side != Mine;
					if (bEnemyBoat && !Out.WasShown.count(S.Key)) ++Out.EnemyBoats;
					auto Shown = Out.WasShown.find(S.Key);
					if (Shown == Out.WasShown.end() ? !S.bShown : Shown->second != S.bShown)
					{
						Out.Sightings.push_back({ Show.Shown(), S.Key, S.bShown });
						if (Shown != Out.WasShown.end()) ++(S.bShown ? Out.Reveals : Out.Hides);
					}
					Out.WasShown[S.Key] = S.bShown;
					if (!S.bShown)
					{
						++Out.UnseenFrames;
						if (!bEnemyBoat) ++Out.WrongHidden;
						if (S.Doing != SeaShow::Fate::Line) ++Out.HiddenOut;
						if (S.Wake > 0.0 || S.Funnel > 0.0) ++Out.HiddenMarks;
					}
					if (bEnemyBoat && S.Doing == SeaShow::Fate::Line)
					{
						const double Now = Show.Shown();
						const auto First = Oracle.First.find(S.Piece);
						if (S.bShown && (First == Oracle.First.end() || Now < First->second - 0.1))
						{
							if (!Out.ShownUnfound) Out.ShownUnfoundFirst = "ship " + std::to_string(S.Key) + " at " + Num(Now, 2) + " s";
							++Out.ShownUnfound;
						}
						const auto Spans = Oracle.Held.find(S.Piece);
						bool bHeld = false;
						if (Spans != Oracle.Held.end())
							for (const auto& Span : Spans->second)
								if (Now >= Span.first + 0.1 && Now <= Span.second - 0.1) bHeld = true;
						if (bHeld && !S.bShown)
						{
							if (!Out.HeldHidden) Out.HeldHiddenFirst = "ship " + std::to_string(S.Key) + " at " + Num(Now, 2) + " s";
							++Out.HeldHidden;
						}
						// Hidden again once nothing has found her for FoundFor, the rule's other half: under longer than that
						// (every hold of hers on the surface, struck, forced up, a lookout's eye on her, a gun laid, lets her go
						// as she gets under, FoundFor on), in no span held, with no find nor blow on her for as long, and not
						// given up.
						const bool bUnder = S.Keel > S.Long * 0.07 + 1.0 && S.Dive != "surfaced";
						if (!bUnder) Out.UnderSince.erase(S.Key);
						else Out.UnderSince.emplace(S.Key, Now);
						const double Stale = SeaShow::Show::FoundFor + 1.0;
						if (bUnder && S.bShown && Now - Out.UnderSince[S.Key] > Stale)
						{
							double LastFind = -1e18;
							const auto Found = Oracle.Found.find(S.Piece);
							if (Found != Oracle.Found.end())
							{
								const auto Past = std::upper_bound(Found->second.begin(), Found->second.end(), Now + 1e-9);
								if (Past != Found->second.begin()) LastFind = *std::prev(Past);
							}
							const auto Blow = Out.BlowAt.find(S.Key);
							if (Blow != Out.BlowAt.end()) LastFind = std::max(LastFind, Blow->second);
							bool bInSpan = false;
							if (Spans != Oracle.Held.end())
								for (const auto& Span : Spans->second)
									if (Now >= Span.first - 0.1 && Now <= Span.second + 0.1) bInSpan = true;
							const auto Given = Oracle.Given.find(S.Piece);
							const bool bGiven = Given != Oracle.Given.end() && Given->second <= Now + 1e-9;
							if (!bInSpan && !bGiven && Now - LastFind > Stale)
							{
								if (!Out.ShownStale)
									Out.ShownStaleFirst = "ship " + std::to_string(S.Key) + " at " + Num(Now, 2) + " s, under since " + Num(Out.UnderSince[S.Key], 2) + " s, last found " + Num(LastFind, 2) + " s";
								++Out.ShownStale;
							}
						}
					}
				}
				auto Was = Last.find(S.Key);
				if (Was != Last.end() && Out.Frames > 1)
				{
					const double Allowed = (std::max(S.Speed, LastSpeed[S.Key]) * 1.2 + 1.0) * Dt + 0.05;
					Out.MostPop = std::max(Out.MostPop, Flat(S.At - Was->second) - Allowed);
				}
				Last[S.Key] = S.At;
				LastSpeed[S.Key] = S.Speed;
				if (S.Doing != SeaShow::Fate::Sinking) Out.HeelOut = std::max(Out.HeelOut, std::fabs(S.Heel));
				// (a boat's pitch is mostly her angle diving or surfacing, and her keel goes far under: her own check, below)
				if (S.Class == "ss")
				{
					Out.MostBoatAngle = std::max(Out.MostBoatAngle, std::fabs(S.Pitch));
					{
						// (her angle followed: a swing counted each time it turns back by 4 degrees from its last extreme)
						auto Swing = Out.BoatSwing.find(S.Key);
						if (Swing == Out.BoatSwing.end()) Out.BoatSwing[S.Key] = { S.Pitch, 0 };
						else
						{
							double& Ext = Swing->second.first;
							int& Way = Swing->second.second;
							if ((Way >= 0 && S.Pitch > Ext) || (Way < 0 && S.Pitch < Ext)) { if (Way != 0) Ext = S.Pitch; }
							if (Way == 0 && std::fabs(S.Pitch - Ext) > 4.0) { Way = S.Pitch > Ext ? 1 : -1; Ext = S.Pitch; ++Out.BoatSwings; }
							else if (Way > 0 && Ext - S.Pitch > 4.0) { Way = -1; Ext = S.Pitch; ++Out.BoatSwings; }
							else if (Way < 0 && S.Pitch - Ext > 4.0) { Way = 1; Ext = S.Pitch; ++Out.BoatSwings; }
						}
					}
					Out.MostBoatKeel = std::max(Out.MostBoatKeel, S.Keel);
					if (S.Keel < 0.0 || (S.Dive != "surfaced" && S.Dive != "periscope" && S.Dive != "deep")) ++Out.BoatOdd;
				}
				else if (S.Doing != SeaShow::Fate::Sinking)
				{
					const int Band = S.Long < 145.0 ? 0 : S.Long < 215.0 ? 1 : 2;
					Out.MostPitch[Band] = std::max(Out.MostPitch[Band], std::fabs(S.Pitch));
					Out.MostHeave[Band] = std::max(Out.MostHeave[Band], std::fabs(S.Heave));
				}
				if (S.Doing == SeaShow::Fate::Sinking && (S.Down < 0.0 || S.Down > 1.0)) ++Out.SinkingAhead;
				const Capsule CI = HullOf(S);
				for (size_t J = I + 1; J < Ships.size(); ++J)
				{
					const SeaShow::ShipOut& O = Ships[J];
					if (std::fabs(O.At.X - S.At.X) > 300.0 || std::fabs(O.At.Y - S.At.Y) > 300.0) continue;
					// (a boat dived passes under, and is passed over: ruling 241's hunters run over her to drop)
					if (S.Dived > 1.0 || O.Dived > 1.0) continue;
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
			// Men in the water: none popping from frame to frame (falling from a deck, or climbing a moving rescuer's side, the
			// most), and twice a second, none inside a ship afloat but a man jumping from her or climbing her side, nor any
			// raft, boat or piece of wreckage.
			{
				const std::vector<SeaShow::ManOut>& Men = Show.Men();
				Out.MostMenSeen = std::max(Out.MostMenSeen, static_cast<int>(Men.size()));
				Out.MostFloatsSeen = std::max(Out.MostFloatsSeen, static_cast<int>(Show.Floats().size()));
				for (const SeaShow::ManOut& M : Men)
				{
					if (M.Key >= static_cast<int>(LastMan.size())) { LastMan.resize(M.Key + 1024); LastManFrame.resize(M.Key + 1024, -9); LastManDoing.resize(M.Key + 1024); }
					if (LastManFrame[M.Key] == Out.Frames - 1)
					{
						const double Pop = (M.At - LastMan[M.Key]).Length() - (17.0 * Dt + 0.8);
						if (Pop > Out.MostManPop)
						{
							Out.MostManPop = Pop;
							Out.ManPopAt = "man " + std::to_string(M.Key) + " of ship " + std::to_string(M.Ship) + " at " + Num(Show.Shown(), 2) + " s, moved " + Num((M.At - LastMan[M.Key]).Length(), 2) + " m, " +
							               std::to_string(static_cast<int>(LastManDoing[M.Key])) + " to " + std::to_string(static_cast<int>(M.Doing)) + (M.On >= 0 ? " on " + std::to_string(M.On) : std::string());
						}
					}
					LastMan[M.Key] = M.At;
					LastManFrame[M.Key] = Out.Frames;
					LastManDoing[M.Key] = M.Doing;
				}
				if (Show.Shown() >= NextWreckLook && (!Men.empty() || !Show.Floats().empty()))
				{
					NextWreckLook = Show.Shown() + 0.5;
					for (const SeaShow::ShipOut& S : Ships)
					{
						if (S.Doing == SeaShow::Fate::Gone || S.Dived > 1.0) continue;
						const double Reach = S.Long * 0.5 + 10.0;
						for (const SeaShow::ManOut& M : Men)
						{
							if (M.Doing == SeaShow::Swim::Jump || M.Doing == SeaShow::Swim::Climb) continue;
							if (std::fabs(M.At.X - S.At.X) > Reach || std::fabs(M.At.Y - S.At.Y) > Reach) continue;
							if (Inside(Vec(M.At.X, M.At.Y, 0.0), S) > 0.3)
							{
								if (!Out.MenInHull) Out.MenInHullFirst = "man " + std::to_string(M.Key) + " of ship " + std::to_string(M.Ship) + " in ship " + std::to_string(S.Key) + " at " + Num(Show.Shown(), 1) + " s";
								++Out.MenInHull;
							}
						}
						for (const SeaShow::FloatOut& Fl : Show.Floats())
						{
							if (std::fabs(Fl.At.X - S.At.X) > Reach || std::fabs(Fl.At.Y - S.At.Y) > Reach) continue;
							if (Inside(Vec(Fl.At.X, Fl.At.Y, 0.0), S) > 0.3) ++Out.FloatsInHull;
						}
					}
				}
			}
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
					       << (S.bFiring ? "fire" : "") << (S.bScreen ? " smoke" : "") << (S.Doing == SeaShow::Fate::Sinking ? " " + S.How : std::string()) << (S.bShown ? "" : " unseen") << "\n";
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
		Out.Guns = Show.Guns();
		Out.Check.DodgesOpen = Show.OpenDodges();
		Out.StreamsOpenAtEnd = static_cast<int>(Out.OpenStreams.size());
		Out.PlanesUpAtEnd = static_cast<int>(Show.Planes().size());
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
	int DiveEvents = 0, ChargeEvents = 0, ChargeHits = 0, Starshells = 0, Rescues = 0, Boats = 0; // a convoy's night (ruling 241)
	int MenTold = 0, OilTold = 0, AbandonTold = 0; // the naval rules' sailor events (22 November)
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
			else if (E.What == "sea:dive") ++DiveEvents;
			else if (E.What == "sea:depthCharge") ++ChargeEvents;
			else if (E.What == "sea:depthChargeHit") ++ChargeHits;
			else if (E.What == "sea:starshell") ++Starshells;
			else if (E.What == "sea:rescue") ++Rescues;
			else if (E.What == "sea:men") ++MenTold;
			else if (E.What == "sea:oil") ++OilTold;
			else if (E.What == "sea:abandon") ++AbandonTold;
		}
	}
	const SeaFeed::Frame& Last = Rec.Views.back();
	std::map<int, int> EndShips;
	for (const SeaShow::Piece& P : Last.Pieces) EndShips[P.Id] = P.Ships;
	int ShipsAtStart = 0;
	for (const SeaShow::Piece& P : Rec.Views.front().Pieces)
	{
		ShipsAtStart += P.Ships;
		if (P.Class == "ss") Boats += P.Ships;
	}

	std::printf("%s: %s (%s), %zu views, %.0f battle s, %zu pieces, %d ships; %d events: %d sunk, %d burning, %d torpedo salvos (%d with tracks, %d hits), %d strikes (%d landed), %d raids\n",
	            Rec.Key.c_str(), Rec.Board.Title.c_str(), Rec.Board.Date.c_str(), Rec.Views.size(), Last.Head.T - Rec.Views.front().Head.T, Pieces.size(), ShipsAtStart, Events, Sunk, Burning,
	            TorpSalvos, TorpFull, TorpHits, Strikes, StrikesLanded, Raids);
	std::printf("  read in %.0f ms; the winner %s (%s)\n", ReadMs, Rec.Card.Winner.c_str(), Rec.Card.Level.c_str());

	std::ofstream Track(OutDir + "/" + Rec.Key + "-sea-track.csv");
	Track << "t,kind,key,piece,side,class,nation,x,y,z,heading,heel,speed,doing,extra\n";
	// (the player's side as the game takes it, the first view's "me": an enemy boat drawn only once found, SetMine)
	const int Player = SeaFeed::SideOf(Rec.Views.front().Head.Me);
	const Played P = Play(Rec, Fps, &Track, Player);
	const Played Again = Play(Rec, Fps, nullptr, Player);
	const Played Half = Play(Rec, Fps * 0.5, nullptr, Player);
	const SeaShow::Checks& C = P.Check;
	// B4's come-back, forced (the pre-check, 21 November 19:24Z): no strike flown after the battle's first 150 s, so each
	// ship that dodged has quiet water to come back to her place in, and every dodge is judged before the end.
	SeaShow::Checks Quiet;
	if (StrikesLanded > 0)
	{
		SeaFeed::Recording Cut = Rec;
		const double From = Rec.Views.front().Head.T + 150.0;
		for (SeaFeed::Frame& F : Cut.Views)
		{
			F.Events.erase(std::remove_if(F.Events.begin(), F.Events.end(), [From](const SeaShow::Event& E) { return E.T > From && E.What == "ability" && E.Key == "strike"; }),
			               F.Events.end());
		}
		Quiet = Play(Cut, Fps, nullptr, Player).Check;
	}

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
	std::printf("  followers giving way to their own line crossing ahead: %.1f s; ships keeping clear of others: %d, %.1f ship-seconds altered\n", C.GaveWay, C.ClearShips, C.KeptClear);
	std::printf("  where ships went down: %d (%d with men in the water of %d due); men in the water %d in truth, %d shown (%.0f%%), %d at once at the most (%d drawn), %d dead, %d picked up, "
	            "%d pushed aside by a ship, %d dead gone under for room; %d floats (%d rafts, %d boats), %d slicks (%d alight)\n",
	            C.Wrecks, C.WrecksManned, C.WrecksDue, C.MenReal, C.MenKept, C.MenReal ? 100.0 * C.MenKept / C.MenReal : 100.0, C.MostMen, P.MostMenSeen, C.MenDead, C.MenPicked,
	            C.MenSwept, C.MenUnder, C.Floats, C.Rafts, C.Boats, C.Slicks, C.SlicksAlight);
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
	Limits.push_back({ "each leader on his station (the battle's place, the zig-zag, keeping clear)", C.MostLead <= 500.0,
	                   Num(C.MostLead, 0) + " m two minutes after keeping clear of a ship, " + Num(C.MostLeadClearing, 0) + " m at any time", "500 m or less" });
	// (turning in succession, a ship astern cut inside or swung outside her leader's turn by a third of her length or so)
	Limits.push_back({ "followers in their leader's wake (once closed up)", C.MostGap <= 40.0, Num(C.MostGap, 1) + " m off his track", "40 m or less" });
	Limits.push_back({ "no ship pops (the show's own count)", C.MostJump <= 0.5, Num(C.MostJump, 2) + " m", "0.5 m or less" });
	Limits.push_back({ "no ship pops (the test's own eye, every ship)", P.MostPop <= 0.5, Num(P.MostPop, 2) + " m", "0.5 m or less" });
	// (a destroyer hard over at her best speed heeled 10 to 12 degrees, and rolled 5 or so more on a fresh sea)
	Limits.push_back({ "heel in a turn and on the swell, afloat", P.HeelOut <= 18.0 && C.MostHeel <= 18.0, Num(std::max(P.HeelOut, C.MostHeel), 1) + " deg", "18 deg or less" });
	// (the art director's table, 21 November 23:44Z: a destroyer pitched 3 to 5 degrees and rose and fell some 1.3 m at
	// Beaufort 6; the show's three swells sum to about 1.05 of the table's figure at their most)
	{
		const double U = Flat(P.Wind), Force = std::pow(U / 0.836, 2.0 / 3.0);
		char Ride[200];
		std::snprintf(Ride, sizeof Ride, "B%.1f: small %.1f deg %.2f m, middle %.1f deg %.2f m, big %.1f deg %.2f m", Force, P.MostPitch[0], P.MostHeave[0],
		              P.MostPitch[1], P.MostHeave[1], P.MostPitch[2], P.MostHeave[2]);
		const bool bRides = P.MostPitch[0] <= 4.4 && P.MostPitch[1] <= 2.2 && P.MostPitch[2] <= 1.4 && P.MostHeave[0] <= 0.23 * 0.021 * U * U + 0.01
		                    && P.MostPitch[0] >= P.MostPitch[2];
		Limits.push_back({ "ships ride the sea by their size and the wind (pitch, heave)", bRides, Ride, "the table's at the most, small ships the most" });
	}
	Limits.push_back({ "no two ships of a line one through the other", P.SameLineTouching == 0,
	                   P.SameLineTouching ? std::to_string(P.SameLineTouching) + " pair-frames (first " + P.SameLineFirst + ")" : "0", "0" });
	// (Brian, 22 November 05:31Z: "the ships are turning into each other and going over each other": every ship keeps
	// clear of every other, of any piece; a collision is the battle's to tell, and none is told here)
	Limits.push_back({ "no two ships of different pieces one through the other", P.OtherTouching == 0,
	                   P.OtherTouching ? std::to_string(P.OtherTouching) + " pair-frames, " + std::to_string(P.TouchPairs.size()) + " pairs" : "0", "0" });
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
	// L9: the ships' anti-aircraft guns (Battle/ShipGuns, shared with the air scene).
	{
		const ShipGuns::Tally& G = P.Guns;
		const double OnShare = G.StreamFrames > 0 ? static_cast<double>(G.StreamOn) / static_cast<double>(G.StreamFrames) : 0.0;
		Limits.push_back({ "the ships' guns fire at the strikes (streams, heavy rounds, their bursts)", StrikesLanded == 0 || (G.StreamFrames > 0 && G.Rounds > 0 && G.Bursts > 0),
		                   std::to_string(G.StreamFrames) + " stream-frames, " + std::to_string(G.Rounds) + " rounds, " + std::to_string(G.Bursts) + " bursts, " + std::to_string(G.Splashes) + " splashes",
		                   "all three, where strikes land" });
		Limits.push_back({ "no gun laid on a friend, a held aircraft or one past its reach", G.OutOfReach == 0, std::to_string(G.OutOfReach) + " (" + std::to_string(G.Held) + " frames held for a fighter)", "0" });
		Limits.push_back({ "the guns' streams within the kit's room", G.MostStreams <= 64, std::to_string(G.MostStreams) + " at once", "64 or fewer" });
		Limits.push_back({ "most of a stream passes its aircraft (on it now and then)", G.StreamFrames == 0 || (OnShare > 0.0 && OnShare < 0.4),
		                   Num(OnShare * 100.0, 1) + "% of stream-frames on it, " + std::to_string(G.Hits) + " rounds seen striking", "some, under 40%" });
		Limits.push_back({ "every stream stopped once no aircraft is up", P.StreamsOpenAtEnd == 0 || P.PlanesUpAtEnd > 0,
		                   std::to_string(P.StreamsOpenAtEnd) + " open at the end, " + std::to_string(P.PlanesUpAtEnd) + " aircraft up", "0 with none up" });
		std::string Fits;
		for (const auto& Pair : G.Kits) { if (Fits.size() < 160) Fits += (Fits.empty() ? "" : "; ") + Pair.first + ": " + Pair.second; }
		std::printf("  ship guns: %d of %d aircraft down shown under them (%d light, %d heavy); fits: %s\n", G.Kills + G.KillsHeavy, C.PlanesDown, G.Kills, G.KillsHeavy, Fits.c_str());
	}
	// B4 and B5.4 (naval-battles/ship-air-defence.md): the ships under the strikes dodge, comb and come back to their
	// places; the dive bombers' hits come with near misses close aboard.
	Limits.push_back({ "ships under a strike dodge it (big ships hard over, every ship combs a drop)", StrikesLanded == 0 || C.Dodges + C.Combs > 0,
	                   std::to_string(C.Dodges) + " dodged, " + std::to_string(C.Combs) + " combed", "some, where strikes land" });
	Limits.push_back({ "every ship that dodged back in her place three minutes on", C.MostDodgeOff <= 60.0,
	                   std::to_string(C.DodgesBack) + " back, the worst still " + Num(C.MostDodgeOff, 0) + " m off, " + std::to_string(C.DodgesOpen) + " still under attack at the end, " +
	                       std::to_string(C.DodgesClosing) + " judged later as they closed at their best speed, " + std::to_string(C.DodgesClearing) + " later as they kept clear of another ship, " + std::to_string(C.FellAstern) + " fell in astern of the next, " +
	                       std::to_string(C.HandedOver) + " leaders handed over the lead, " + std::to_string(C.DodgesUnturned) + " ended before the helm bit (not judged)",
	                   "60 m or less" });
	Limits.push_back({ "no ship strays far from her place dodging", C.MostStray <= 900.0, Num(C.MostStray, 0) + " m at the most", "900 m or less" });
	Limits.push_back({ "with no strike after 150 s, every ship that dodged back in her place, each leader on his station",
	                   StrikesLanded == 0 || (Quiet.Dodges + Quiet.Combs > 0 && Quiet.DodgesOpen == 0 && Quiet.MostDodgeOff <= 60.0 && Quiet.MostLead <= 500.0),
	                   std::to_string(Quiet.Dodges) + " dodged, " + std::to_string(Quiet.Combs) + " combed, " + std::to_string(Quiet.DodgesBack) + " back, " + std::to_string(Quiet.DodgesOpen) +
	                       " unjudged, the worst " + Num(Quiet.MostDodgeOff, 0) + " m off, the leaders " + Num(Quiet.MostLead, 0) + " m, " + Num(Quiet.MostStray, 0) + " m strayed",
	                   "some, all judged and back (60 m), 500 m" });
	Limits.push_back({ "near misses close aboard beside the bomb hits", C.BombHits == 0 || C.WingBombs > 0,
	                   std::to_string(C.NearMisses) + " near misses (" + std::to_string(C.WingBombs) + " wing bombs) for " + std::to_string(C.BombHits) + " hits", "some beside the hits" });
	Limits.push_back({ "no aircraft under the sea or popping", P.PlanesUnder == 0 && P.MostPlanePop <= 1.0,
	                   std::to_string(P.PlanesUnder) + " under, " + Num(P.MostPlanePop, 1) + " m pop", "0 under, 1 m or less" });
	// L5, L6, L7: smoke, wind and the zig-zag.
	Limits.push_back({ "smoke screens start, move and stop in order", P.ScreenStray == 0 && P.FireStray == 0, std::to_string(P.ScreenStray + P.FireStray) + " stray", "0" });
	if (Rec.Board.WindSpeed >= 0.0)
	{
		const double Off = std::fabs(std::remainder(WindFrom - Rec.Board.WindFrom, 2.0 * Pi)) * 180.0 / Pi;
		// (a calm has no bearing)
		Limits.push_back({ "the battle's own wind", (Off < 0.5 || Rec.Board.WindSpeed < 0.05) && std::fabs(Flat(P.Wind) - Rec.Board.WindSpeed) < 0.05,
		                   Num(WindFrom * 180.0 / Pi, 0) + " deg at " + Num(Flat(P.Wind), 1) + " (the field: " + Num(Rec.Board.WindFrom * 180.0 / Pi, 0) + " at " + Num(Rec.Board.WindSpeed, 1) + ")",
		                   "the field's" });
	}
	Limits.push_back({ "the zig-zag steadied a minute after contact", C.MostZigAfter <= 1.0, Num(C.MostZigAfter, 2) + " m", "1 m or less" });
	// Where ships went down (Brian, 22 November 05:32Z to 05:33Z): her men in the water, her wreckage and her oil.
	Limits.push_back({ "every ship gone down leaves her oil, and her men in the water where her way down leaves any", C.Slicks == C.Sinks && C.WrecksManned == C.WrecksDue,
	                   std::to_string(C.Slicks) + " slicks for " + std::to_string(C.Sinks) + " sunk, " + std::to_string(C.WrecksManned) + " of " + std::to_string(C.WrecksDue) + " with men",
	                   "all" });
	Limits.push_back({ "no man in the water nor any wreckage inside a ship", P.MenInHull == 0 && P.FloatsInHull == 0,
	                   std::to_string(P.MenInHull) + " man-looks, " + std::to_string(P.FloatsInHull) + " float-looks" + (P.MenInHull ? " (first " + P.MenInHullFirst + ")" : std::string()), "0" });
	Limits.push_back({ "no man in the water pops", P.MostManPop <= 0.0, P.MostManPop > 0.0 ? Num(P.MostManPop, 2) + " m over (" + P.ManPopAt + ")" : "none", "none" });
	Limits.push_back({ "the men in the water within the show's room", C.MostMen <= Rec.Board.MenMost, std::to_string(C.MostMen) + " (room " + std::to_string(Rec.Board.MenMost) + ")",
	                   "the room or fewer" });
	// The men in the water as the naval rules count them (22 November). Where the recording has none of it, each holds.
	if (MenTold + OilTold + AbandonTold > 0)
	{
		Limits.push_back({ "every ship abandoned leaves her line, and every count of her men and her oil finds her wreck",
		                   C.Abandons == AbandonTold && C.AbandonEvents == AbandonTold && C.MenMatched == MenTold && C.MenEvents == MenTold && C.OilMatched == OilTold && C.OilEvents == OilTold,
		                   std::to_string(C.Abandons) + " of " + std::to_string(AbandonTold) + " abandoned, " + std::to_string(C.MenMatched) + " of " + std::to_string(MenTold) + " men, " +
		                       std::to_string(C.OilMatched) + " of " + std::to_string(OilTold) + " oil",
		                   "all" });
		Limits.push_back({ "no more men drawn picked up than the battle's count of them", C.PickedOver == 0, std::to_string(C.PickedOver) + " over", "0" });
	}
	// A convoy's night (ruling 241: the submarine scene). Where the recording has none of it, each holds at nothing.
	if (Boats > 0 || DiveEvents + ChargeEvents + ChargeHits + Starshells + Rescues > 0)
	{
		Limits.push_back({ "every boat drawn with her state, her keel under the sea and above her crush depth", P.BoatOdd == 0 && P.MostBoatKeel <= Rec.Board.CrushKeel,
		                   std::to_string(P.BoatOdd) + " odd boat-frames, the deepest " + Num(P.MostBoatKeel, 0) + " m (crush " + Num(Rec.Board.CrushKeel, 0) + ")", "0, the crush depth or less" });
		Limits.push_back({ "a boat's angle diving or surfacing within reason", P.MostBoatAngle <= 15.0, Num(P.MostBoatAngle, 1) + " deg", "15 deg or less" });
		// (a depth told as it changes, a view at a time, followed without a nod at each: the pre-check, 22 November)
		const int Swings = 2 * (DiveEvents + 2 * Boats);
		Limits.push_back({ "a boat's angle goes over and comes back once a dive, no nodding", P.BoatSwings <= Swings,
		                   std::to_string(P.BoatSwings) + " swings of 4 degrees (" + std::to_string(Swings) + " allowed)", "two a dive told and four a boat, or fewer" });
		Limits.push_back({ "every dive told shown going", C.Dives == DiveEvents && C.DiveEvents == DiveEvents, std::to_string(C.Dives) + " of " + std::to_string(DiveEvents), "all" });
		Limits.push_back({ "every depth-charge attack goes in", C.ChargeDrops == ChargeEvents && C.ChargeEvents == ChargeEvents,
		                   std::to_string(C.ChargeDrops) + " of " + std::to_string(ChargeEvents) + ", " + std::to_string(C.ChargeBursts) + " charges burst", "all" });
		Limits.push_back({ "every depth-charge hit the battle booked bursts on the boat", C.ChargeBurstsBooked == ChargeHits && C.ChargeHitUnmatched == 0 && P.ChargesOffBoat == 0,
		                   std::to_string(C.ChargeBurstsBooked) + " of " + std::to_string(ChargeHits) + ", " + std::to_string(C.ChargeHitUnmatched) + " with no attack before it, " +
		                       std::to_string(P.ChargesOffBoat) + " out of her reach",
		                   "all, on her" });
		Limits.push_back({ "every aircraft told on a boat seen, letting go on her before she goes", C.AirOnBoats == 0 || (C.AirBlowsOnBoats == C.AirOnBoats && C.MostPlanes > 0),
		                   std::to_string(C.AirBlowsOnBoats) + " of " + std::to_string(C.AirOnBoats) + " let go on her, aircraft up " + std::to_string(C.MostPlanes) + " at the most", "all, seen" });
		Limits.push_back({ "every star shell and rescue shown", C.Starshells == Starshells && C.Rescues == Rescues,
		                   std::to_string(C.Starshells) + " of " + std::to_string(Starshells) + " star shells, " + std::to_string(C.Rescues) + " of " + std::to_string(Rescues) + " rescues", "all" });
	}
	// Enemy boats unseen until found (SetMine, the views' "me"): drawn only once the battle has told what finds her, and
	// nothing of her out while she is not. Where the player's side has no enemy boat but there are boats (a hunt of his own
	// boats, as D1's), the recording is played again as the other side's, so the rule is held there too.
	{
		auto SameSightings = [](const Played& A, const Played& B, double Slack)
		{
			if (A.Sightings.size() != B.Sightings.size()) return false;
			for (size_t K = 0; K < A.Sightings.size(); ++K)
			{
				const Played::Sighting& X = A.Sightings[K];
				const Played::Sighting& Y = B.Sightings[K];
				if (X.Key != Y.Key || X.bShown != Y.bShown || std::fabs(X.T - Y.T) > Slack) return false;
			}
			return true;
		};
		auto HideLimits = [&Limits, &SameSightings, Fps](const Played& Q, const Played& QAgain, const Played& QHalf, bool bFlip)
		{
			const SeaShow::Checks& QC = Q.Check;
			std::string Seen;
			for (size_t K = 0; K < Q.Sightings.size() && K < 16; ++K)
				Seen += (K ? ", " : "") + Num(Q.Sightings[K].T, 1) + " s " + std::to_string(Q.Sightings[K].Key) + (Q.Sightings[K].bShown ? " found" : " hidden");
			std::printf("  enemy boats unseen%s: %d boats, %ld ship-frames hidden, %d found, %d lost again (%s%s); contacts %d gained, %d lost (%d odd), %d radio bearings\n",
			            bFlip ? " (played as the other side)" : "", Q.EnemyBoats, Q.UnseenFrames, Q.Reveals, Q.Hides, Seen.c_str(), Q.Sightings.size() > 16 ? ", ..." : "", QC.Contacts,
			            QC.ContactsLost, QC.ContactsOdd, QC.Bearings);
			Limits.push_back({ bFlip ? "as the other side: only an enemy boat in the fight hidden" : "only an enemy boat in the fight is ever hidden", Q.WrongHidden == 0 && Q.HiddenOut == 0,
			                   std::to_string(Q.WrongHidden) + " not an enemy boat, " + std::to_string(Q.HiddenOut) + " out of the fight", "0, 0" });
			Limits.push_back({ bFlip ? "as the other side: nothing of a hidden boat on the sea" : "nothing of a hidden boat on the sea: no wake, no smoke", Q.HiddenMarks == 0,
			                   std::to_string(Q.HiddenMarks) + " ship-frames with a wake or smoke", "0" });
			Limits.push_back({ bFlip ? "as the other side: no blow, gun or cue of hers while hidden" : "no blow on her, gun of hers or cue of hers while she is hidden",
			                   Q.HitsUnseen == 0 && Q.GunsUnseen == 0 && Q.HeardUnseen == 0,
			                   std::to_string(Q.HitsUnseen) + " blows, " + std::to_string(Q.GunsUnseen) + " guns, " + std::to_string(Q.HeardUnseen) + " cues", "0" });
			Limits.push_back({ bFlip ? "as the other side: hidden until found, shown while held" : "an enemy boat hidden until found, and shown while an escort holds her",
			                   Q.ShownUnfound == 0 && Q.HeldHidden == 0,
			                   std::to_string(Q.ShownUnfound) + " frames shown unfound" + (Q.ShownUnfound ? " (first " + Q.ShownUnfoundFirst + ")" : std::string()) + ", " +
			                       std::to_string(Q.HeldHidden) + " hidden while held" + (Q.HeldHidden ? " (first " + Q.HeldHiddenFirst + ")" : std::string()),
			                   "0, 0" });
			Limits.push_back({ bFlip ? "as the other side: hidden again once nothing has found her for FoundFor" : "an enemy boat hidden again once nothing has found her for FoundFor",
			                   Q.ShownStale == 0, std::to_string(Q.ShownStale) + " frames shown so" + (Q.ShownStale ? " (first " + Q.ShownStaleFirst + ")" : std::string()), "0" });
			Limits.push_back({ bFlip ? "as the other side: every contact lost was one held" : "every contact lost was one held", QC.ContactsOdd == 0,
			                   std::to_string(QC.ContactsLost) + " lost, " + std::to_string(QC.ContactsOdd) + " never held", "0 never held" });
			const bool bSame = SameSightings(Q, QAgain, 1e-9), bHalf = SameSightings(Q, QHalf, 1.0 / (Fps * 0.5) + 1e-6);
			Limits.push_back({ bFlip ? "as the other side: the same sightings every time, at half the rate" : "the same sightings every time and at half the frame rate",
			                   bSame && bHalf, std::string(bSame ? "same" : "different") + ", " + (bHalf ? "same" : "different") + " at half the rate (" + std::to_string(Q.Sightings.size()) + ")",
			                   "same, same within a frame" });
		};
		if (P.EnemyBoats > 0 || P.UnseenFrames > 0)
		{
			HideLimits(P, Again, Half, false);
			// The stand-in's own, each sighting pinned: Boat A (1920) found by radar at 58 s and lost diving at 70, out of sight
			// from 160, found again by asdic at 300, to her end (forced up, given up, scuttled). Boat B (1984) found at 130 by an
			// asdic contact held a hundred seconds with no attack on her and lost at 230, found again by asdic at 280 to its loss
			// close at 335, kept in sight by the blow the battle booked on her at 358 to FoundFor after it, found again at 456 for
			// the ahead-thrown run, to her end. Boat C (2048, her views' heard and no sea:sonar) found at 63.8 by her deck gun's
			// hit on Merchant 3, before the view that says her gun is on, held by her heard from 100 to 130 and kept by his
			// lookouts while she stays up within the night's seeing, out of sight FoundFor after she is under (dived at 240);
			// held by her heard again from 472 deep, out of sight FoundFor after it lets her go at 500. The night with an
			// aircraft over it (convoy-night-air) the same, and Boat D (2112) found at 20 by her deck gun laid on Merchant 3
			// (before the aircraft run in on her at 30, over Boat C not yet found: Boat C's guns quiet, the check above), out of
			// sight FoundFor after she is under (dived at 76).
			auto Pin = [](const std::vector<Played::Sighting>& Got, const std::vector<Played::Sighting>& Want, std::string& Words)
			{
				bool bSame = Got.size() == Want.size();
				std::string Seen, Wanted;
				for (size_t K = 0; K < std::max(Got.size(), Want.size()); ++K)
				{
					if (K < Got.size()) Seen += (K ? ", " : "") + Num(Got[K].T, 2) + " " + std::to_string(Got[K].Key) + (Got[K].bShown ? " found" : " hidden");
					if (K < Want.size()) Wanted += (K ? ", " : "") + Num(Want[K].T, 2) + " " + std::to_string(Want[K].Key) + (Want[K].bShown ? " found" : " hidden");
					if (K < Got.size() && K < Want.size())
						bSame = bSame && Got[K].Key == Want[K].Key && Got[K].bShown == Want[K].bShown && std::fabs(Got[K].T - Want[K].T) <= 0.1;
				}
				Words = Seen + (bSame ? std::string() : " against " + Wanted);
				return bSame;
			};
			if (Rec.Key == "convoy-night" || Rec.Key == "convoy-night-air")
			{
				const bool bAir = Rec.Key == "convoy-night-air";
				const int Reveals = bAir ? 7 : 6, Hides = bAir ? 5 : 4;
				Limits.push_back({ "the stand-in's boats found when its events say", P.Reveals == Reveals && P.Hides == Hides && P.UnseenFrames > 0,
				                   std::to_string(P.Reveals) + " found, " + std::to_string(P.Hides) + " lost again",
				                   bAir ? "7 found, 5 lost again" : "6 found, 4 lost again" });
				std::vector<Played::Sighting> Pinned = {
					{ 0.0, 1920, false }, { 0.0, 1984, false }, { 0.0, 2048, false }, { 58.0, 1920, true }, { 63.8, 2048, true }, { 130.0, 1984, true },
					{ 160.05, 1920, false }, { 300.0, 1920, true }, { 334.95, 2048, false }, { 448.05, 1984, false }, { 456.0, 1984, true }, { 472.0, 2048, true },
					{ 590.05, 2048, false } };
				if (bAir)
				{
					Pinned.insert(Pinned.begin() + 3, { { 0.0, 2112, false }, { 20.0, 2112, true } });
					Pinned.insert(Pinned.begin() + 9, Played::Sighting{ 171.0, 2112, false });
				}
				std::string Said;
				const bool bPinned = Pin(P.Sightings, Pinned, Said);
				Limits.push_back({ "the stand-in's boats found and lost again each when its events say", bPinned, Said, "each as pinned here, within 0.1 s" });
			}
			if (Rec.Key == "convoy-night")
			{
				// The rules the night does not show by themselves, each on the night changed for it: Escort 1's contact on
				// Boat B closed by an ahead-thrown run of its at 150 (no loss told after one: she is let go FoundFor after the
				// run) and its loss told only at 250, after that (in sight FoundFor from the loss, as from any); and Boat C given
				// up at 200 on the surface (in sight to her end, whatever she does after: here she dives at 240 as before). The
				// recording's own reading of each (FindsOf) holds there too.
				SeaFeed::Recording Changed = Rec;
				for (SeaFeed::Frame& F : Changed.Views)
				{
					F.Events.erase(std::remove_if(F.Events.begin(), F.Events.end(),
					                              [](const SeaShow::Event& E) { return E.What == "sea:sonar" && E.Unit == 20 && E.On == 31 && !E.Lost.empty(); }),
					               F.Events.end());
				}
				auto Tell = [&Changed](double T, const char* What, int Unit, int Side, int On, const char* Kind, const char* Lost)
				{
					SeaShow::Event E;
					E.T = T;
					E.What = What;
					E.Unit = Unit;
					E.Side = Side;
					E.On = On;
					E.Kind = Kind;
					E.Lost = Lost;
					for (SeaFeed::Frame& F : Changed.Views)
					{
						if (F.Head.T < T - 1e-9) continue;
						F.Events.push_back(E);
						break;
					}
				};
				Tell(150.0, "sea:depthCharge", 20, 1, 31, "ahead", "");
				Tell(200.0, "sea:surrender", 32, 0, -1, "", "");
				Tell(250.0, "sea:sonar", 20, 1, 31, "asdic", "faded");
				const Played Q = Play(Changed, Fps, nullptr, Player);
				std::vector<Played::Sighting> Of;
				for (const Played::Sighting& Seen : Q.Sightings)
					if (Seen.Key == 1984 || Seen.Key == 2048) Of.push_back(Seen);
				const std::vector<Played::Sighting> Ruled = {
					{ 0.0, 1984, false }, { 0.0, 2048, false }, { 63.8, 2048, true }, { 130.0, 1984, true }, { 240.05, 1984, false }, { 250.0, 1984, true },
					{ 448.05, 1984, false }, { 456.0, 1984, true } };
				std::string RuledSaid;
				const bool bRuled = Pin(Of, Ruled, RuledSaid);
				const bool bRead = Q.ShownUnfound == 0 && Q.HeldHidden == 0 && Q.ShownStale == 0 && Q.Check.ContactsOdd == 0;
				Limits.push_back({ "an ahead-thrown run's hold, a late loss and a boat given up, each as its rule says", bRuled && bRead,
				                   RuledSaid + "; " + std::to_string(Q.ShownUnfound) + " shown unfound, " + std::to_string(Q.HeldHidden) + " hidden while held, " +
				                       std::to_string(Q.ShownStale) + " shown stale, " + std::to_string(Q.Check.ContactsOdd) + " odd losses",
				                   "each as pinned here, within 0.1 s; 0, 0, 0, 0" });
			}
		}
		else if (Boats > 0 && Player >= 0)
		{
			const Played Flip = Play(Rec, Fps, nullptr, 1 - Player);
			const Played FlipAgain = Play(Rec, Fps, nullptr, 1 - Player);
			const Played FlipHalf = Play(Rec, Fps * 0.5, nullptr, 1 - Player);
			Limits.push_back({ "the player's own boats never hidden", P.UnseenFrames == 0, std::to_string(P.UnseenFrames) + " ship-frames hidden", "0" });
			Limits.push_back({ "as the other side: his enemy's boats hidden until found", Flip.EnemyBoats > 0 && Flip.UnseenFrames > 0,
			                   std::to_string(Flip.EnemyBoats) + " boats, " + std::to_string(Flip.UnseenFrames) + " ship-frames hidden", "some" });
			HideLimits(Flip, FlipAgain, FlipHalf, true);
		}
	}
	// The tug-of-war bar (release 2; the UI director's ruling, 21 November): the knot each view gives, from the
	// player's flag. First the rule itself on views made up for it, then the recording's own.
	{
		auto Made = [](double La, double Ld, const char* Me, const char* Broken, const char* Winner)
		{
			SeaFeed::Top H;
			H.LostAtt = La;
			H.LostDef = Ld;
			H.Me = Me;
			H.Broken = Broken;
			H.bOver = Winner[0] != 0;
			H.Winner = Winner;
			return SeaFeed::KnotOf(H);
		};
		const double Level = Made(0.0, 0.0, "att", "", ""), Ahead = Made(0.2, 0.4, "att", "", ""), Mirror = Made(0.2, 0.4, "def", "", "");
		const double Broke = Made(0.3, 0.32, "att", "def", ""), Lost = Made(0.3, 0.6, "att", "", "def"), None = Made(-1.0, -1.0, "att", "", "");
		// (the side that broke off, named the winner at the end when the last of its chasers is lost: the end's word stands)
		const double Turned = Made(0.3, 0.2, "att", "att", "att");
		const bool bRule = std::fabs(Level - 0.5) < 1e-9 && std::fabs(Ahead - 0.45 / 0.7) < 1e-9 && std::fabs(Mirror - (1.0 - Ahead)) < 1e-9
			&& std::fabs(Broke - (0.5 + SeaFeed::SettleBy)) < 1e-9 && std::fabs(Lost - (0.5 - SeaFeed::SettleBy)) < 1e-9 && None < 0.0
			&& std::fabs(Turned - (0.5 + SeaFeed::SettleBy)) < 1e-9;
		Limits.push_back({ "the bar's rule (level, exchange, mirror, break, end, broken-off winner)", bRule,
		                   Num(Level, 3) + ", " + Num(Ahead, 3) + ", " + Num(Mirror, 3) + ", " + Num(Broke, 3) + ", " + Num(Lost, 3) + ", " + Num(None, 0) + ", " + Num(Turned, 3),
		                   "0.5, 0.643, 0.357, 0.6, 0.4, -1, 0.6" });
		int Recorded = 0, WrongSide = 0;
		double FirstKnot = -1.0, LastKnot = -1.0;
		for (const SeaFeed::Frame& F : Rec.Views)
		{
			const double Knot = SeaFeed::KnotOf(F.Head);
			if (Knot < 0.0)
			{
				continue;
			}
			if (Recorded++ == 0)
			{
				FirstKnot = Knot;
			}
			LastKnot = Knot;
			const int BrokeSide = SeaFeed::SideOf(F.Head.Broken), NamedSide = F.Head.bOver ? SeaFeed::SideOf(F.Head.Winner) : -1;
			const int WonSide = NamedSide >= 0 ? NamedSide : BrokeSide >= 0 ? 1 - BrokeSide : -1;
			if (WonSide >= 0)
			{
				const bool bMineWon = WonSide == SeaFeed::SideOf(F.Head.Me);
				WrongSide += (bMineWon ? Knot < 0.5 + SeaFeed::SettleBy - 1e-9 : Knot > 0.5 - SeaFeed::SettleBy + 1e-9) ? 1 : 0;
			}
		}
		if (Recorded == 0)
		{
			Limits.push_back({ "the bar's knot (the views' lost shares)", true, "none recorded yet: the scene keeps the bar hidden", "shown once recorded" });
		}
		else
		{
			Limits.push_back({ "the bar's knot starts level", std::fabs(FirstKnot - 0.5) < 1e-9, Num(FirstKnot, 3), "0.5" });
			Limits.push_back({ "the bar's knot on the winner's side from the break or the end", WrongSide == 0 && Rec.Views.back().Head.bOver,
			                   std::to_string(WrongSide) + " views wrong, the last at " + Num(LastKnot, 3), "0 wrong" });
		}
	}
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
	// (the slowest frame in a thousand, as the machine's own hiccups make the very worst one meaningless; and the least of
	// three plays' such frames and means, as one busy stretch of a shared machine is not the show's: the air test's clock
	// lines do the same, and Samar's sat on this line, 0.59 to 1.18 ms over twelve runs of one SeaShow.cpp, the pre-check,
	// 22 November 11:31Z)
	const Played Third = Play(Rec, Fps, nullptr, Player);
	double Slow = 1e18, Mean = 1e18;
	for (const Played* Q : { &P, &Again, &Third })
	{
		std::vector<double> Times = Q->FrameMs;
		std::sort(Times.begin(), Times.end());
		Slow = std::min(Slow, Times.empty() ? 0.0 : Times[std::min(Times.size() - 1, static_cast<size_t>(Times.size() * 0.999))]);
		Mean = std::min(Mean, Q->TotalMs / std::max(1L, Q->Frames));
	}
	Limits.push_back({ "a frame within budget", Slow < 1.0 && Mean < 0.25,
	                   Num(Slow, 3) + " ms (one in a thousand), " + Num(Mean, 3) + " ms mean, " + Num(P.WorstFrameMs, 2) + " worst",
	                   "under 1 ms one in a thousand, 0.25 mean (the least of three plays)" });

	int Failed = 0;
	for (const Limit& L : Limits)
	{
		std::printf("  %s %-58s %s (%s)\n", L.bOk ? "ok  " : "FAIL", L.Name, L.Got.c_str(), L.Want);
		if (!L.bOk) ++Failed;
	}
	std::printf("%s\n", Failed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failed ? 1 : 0;
}
