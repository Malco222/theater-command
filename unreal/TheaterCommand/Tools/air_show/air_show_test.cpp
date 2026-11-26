// The air show's cloud test (ruling 238; air 3d-handoff.md sections 9 and 10): plays a recorded air battle
// (Content/Data/AirBattles/<key>.json, from the rules repo's tools/air-record) through the very code the game runs
// (Source/TheaterCommand/Battle/AirShow and AirFeed), at the game's frame rate, and checks that every aircraft flew as
// an aircraft does. It writes a track of the flight for the check pictures (air_show_frames.py).
//
//   Tools/air_show/run.sh Content/Data/AirBattles/oahu.json [out-dir]
//
// Plain C++20, no engine: it builds anywhere with g++ or clang++.

#include "Battle/AirDesk.h"
#include "Battle/AirFeed.h"
#include "Battle/AirGround.h"
#include "Battle/AirShow.h"
#include "Battle/AirSky.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>

namespace
{
	/**
	 * A bomber's bay from her model file, as AAirBattle::PlaneMesh takes it from the shape it draws: her racked load
	 * ("bombs" in near.part), only where both doors (bay.port and bay.starboard, turning) are there too, in metres
	 * forward, right and up from the middle of her points' box (the point the scene draws her about; a sculpted model is
	 * drawn at its own size). The file has x forward, y to the right.
	 */
	bool BayFromModel(const std::string& File, AirShow::Show::BayPlace& Out)
	{
		std::ifstream Model(File, std::ios::binary);
		if (!Model) return false;
		std::stringstream Text;
		Text << Model.rdbuf();
		AirFeed::Json M;
		std::string Why;
		if (!AirFeed::Parse(Text.str(), M, Why)) return false;
		int Doors = 0, Bombs = -1;
		const AirFeed::Json& Parts = M["parts"];
		for (size_t K = 0; K < Parts.Count(); ++K)
		{
			const std::string Name = Parts.At(K)["name"].Str("");
			if ((Name == "bay.port" || Name == "bay.starboard") && Parts.At(K)["kind"].Str("") == "turn") ++Doors;
			if (Name == "bombs") Bombs = static_cast<int>(K);
		}
		const AirFeed::Json& Points = M["near"]["points"];
		const AirFeed::Json& PartOf = M["near"]["part"];
		if (Doors != 2 || Bombs < 0 || Points.Count() < 3 || PartOf.Count() * 3 > Points.Count()) return false;
		double Lo[3] = { 1e18, 1e18, 1e18 }, Hi[3] = { -1e18, -1e18, -1e18 }, LoB[3] = { 1e18, 1e18, 1e18 }, HiB[3] = { -1e18, -1e18, -1e18 };
		for (size_t I = 0; I + 2 < Points.Count(); I += 3)
		{
			const bool bLoad = I / 3 < PartOf.Count() && static_cast<int>(PartOf.At(I / 3).Num(0.0)) == Bombs;
			for (int A = 0; A < 3; ++A)
			{
				const double V = Points.At(I + static_cast<size_t>(A)).Num(0.0);
				Lo[A] = std::min(Lo[A], V);
				Hi[A] = std::max(Hi[A], V);
				if (bLoad) { LoB[A] = std::min(LoB[A], V); HiB[A] = std::max(HiB[A], V); }
			}
		}
		if (LoB[0] > HiB[0]) return false;
		const double Mid[3] = { (Lo[0] + Hi[0]) * 0.5, (Lo[1] + Hi[1]) * 0.5, (Lo[2] + Hi[2]) * 0.5 };
		Out.Fore = HiB[0] - Mid[0];
		Out.Aft = LoB[0] - Mid[0];
		Out.Up = (LoB[2] + HiB[2]) * 0.5 - Mid[2];
		Out.Half = std::clamp((HiB[1] - LoB[1]) * 0.25, 0.1, 0.4);
		return true;
	}

	/** Land, plainly: inside any ring of three or more points, by every edge of it (the check on AirGround::BoardLand). */
	bool PlainLand(const std::vector<std::vector<std::pair<double, double>>>& Rings, double X, double Y)
	{
		for (const std::vector<std::pair<double, double>>& Ring : Rings)
		{
			if (Ring.size() < 3) continue;
			bool bIn = false;
			for (size_t K = 0; K < Ring.size(); ++K)
			{
				const std::pair<double, double>& A = Ring[K];
				const std::pair<double, double>& B = Ring[(K + 1) % Ring.size()];
				if ((A.second > Y) != (B.second > Y) && X < A.first + (Y - A.second) * (B.first - A.first) / (B.second - A.second)) bIn = !bIn;
			}
			if (bIn) return true;
		}
		return false;
	}

	const char* FxName(AirShow::FxKind K)
	{
		switch (K)
		{
		case AirShow::FxKind::Gun: return "gun";
		case AirShow::FxKind::GunStop: return "gunstop";
		case AirShow::FxKind::Hit: return "hit";
		case AirShow::FxKind::TrailStart: return "trailstart";
		case AirShow::FxKind::TrailMove: return "trailmove";
		case AirShow::FxKind::TrailStop: return "trailstop";
		case AirShow::FxKind::Flak: return "flak";
		case AirShow::FxKind::Blast: return "blast";
		case AirShow::FxKind::Fall: return "fall";
		case AirShow::FxKind::FallMove: return "fallmove";
		case AirShow::FxKind::FallStop: return "fallstop";
		case AirShow::FxKind::Splash: return "splash";
		case AirShow::FxKind::Crash: return "crash";
		case AirShow::FxKind::BombHit: return "bombhit";
		case AirShow::FxKind::Sound: return "sound";
		case AirShow::FxKind::Dispatch: return "dispatch";
		case AirShow::FxKind::ShipHit: return "shiphit";
		case AirShow::FxKind::ShipSunk: return "shipsunk";
		case AirShow::FxKind::ShipGun: return "shipgun";
		case AirShow::FxKind::MarkHit: return "markhit";
		case AirShow::FxKind::MarkGun: return "markgun";
		case AirShow::FxKind::Smoke: return "smoke";
		}
		return "?";
	}

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

	/**
	 * The gunnery, watched as the eye will see it (AirShow.cpp, "The gunnery"). Brian, 21 November: "make sure the gun
	 * fire from the planes actually hit the target and do damage when its a hit"; "i can see a gunner on a bomber and
	 * its tracers hit a fighter and then it goes down"; "when they do hit a plane or something with gun fire than it
	 * shows up on the receiving object".
	 *
	 * The battle counts every loss and every hit. What the scene owes them is that the eye can follow each: the burst
	 * that carries one is laid true, its tracers end on the aircraft, its strikes are drawn where they end, and only
	 * then does the damage begin; every other burst is seen to miss. So this watches what the effects kit will draw.
	 * Each frame it flies one round of every Gun effect by the kit's own flight (the show's Rounds rows, RoundAt, ended
	 * at the effect's Tof where it carries one) and keeps what airframes the round entered, how near it passed one it
	 * did not strike, and where it ended; with them every Hit drawn, every Fall begun and every Moment told to the
	 * cameras. Closed after the last frame, it sets the rounds beside the strikes and the strikes beside the falls.
	 *
	 * A ship's guns are not watched here (ShipGuns; her light guns' streams have keys of their own, from
	 * ShipGuns::StreamBase up, and her rounds' hits are told as "ship" or "kill"): they are laid by the ships' own
	 * dice, most of them to miss, and they have their own row, "the ships fire back". Her streams and her hits are
	 * counted (ShipStreams, ShipHits) and left out of every row on the aircraft's guns.
	 */
	struct GunWatch
	{
		using Vec = AirShow::Vec;
		struct Axes { Vec F, R, U; };
		/** An aircraft's own frame as it is drawn: forward, right, up. */
		static Axes AxesOf(const AirShow::PlaneOut& P)
		{
			const double Rad = 3.14159265358979323846 / 180.0;
			const double H = P.Heading * Rad, G = P.Pitch * Rad, Bk = P.Bank * Rad;
			Axes A;
			A.F = Vec(std::sin(H) * std::cos(G), std::cos(H) * std::cos(G), std::sin(G));
			const Vec R0(std::cos(H), -std::sin(H), 0.0), U0 = R0.Cross(A.F);
			A.R = R0 * std::cos(Bk) - U0 * std::sin(Bk);
			A.U = U0 * std::cos(Bk) + R0 * std::sin(Bk);
			return A;
		}
		static Vec Into(const Axes& A, const Vec& D) { return Vec(D.Dot(A.F), D.Dot(A.R), D.Dot(A.U)); }

		/** An airframe a round came into before its end, and one it passed near. */
		struct Entry { int Plane = 0; bool bFighter = false, bFalling = false; };
		struct Pass { int Plane = 0; double Least = 1e9; };
		struct Round
		{
			int Gun = 0;
			Vec From, Vel;
			double Born = 0.0, End = 1.2, Drag = 0.36, Drop = 9.81, LastAge = 0.0;
			bool bEnds = false, bUnder = false;
			std::vector<Entry> Entered;
			std::vector<Pass> Passed; // (to 3 m of its skin, coarsely: 0 inside, 0.75, 1.5, 3)
		};
		/** A round that ended on an aircraft, a strike drawn on one, a round ended on the ground, a strike drawn there. */
		struct Ended { int Plane = 0; Vec Local; double At = 0.0; int Gun = 0; bool bMatched = false; };
		struct Strike { int Plane = 0; Vec Local; double At = 0.0; int Id = 0, Nth = 0; bool bMatched = false; double Off = 1e9; std::set<int> From; };
		struct Low { Vec At; double From = 0.0, To = 0.0; bool bMatched = false; };

		/** Set to have each thing a row counts told on stderr as it is found (AIR_SHOW_GUNNERY=1). */
		bool bTell = false;

		// What was drawn.
		long GunFxEnding = 0, RoundsFlown = 0; // (Gun effects that told an end; rounds flown out, one a Gun effect)
		long Strikes = 0, Splinters = 0, OddHits = 0, GroundHits = 0;
		long ShipStreams = 0, ShipHits = 0; // (a ship's light guns' stream frames and her rounds' hits: not the aircraft's)
		long Falls = 0, FallsStruck = 0; // (falls begun; those with a strike on the aircraft in the 2 s before)
		// The rounds: through an aircraft that flies on (ThroughEnded of them ended later on something else, ThroughFree
		// flew out their life), through one already going down, through the shooter's own.
		long Through = 0, ThroughEnded = 0, ThroughFree = 0, ThroughFalling = 0, ThroughOwn = 0;
		// Where they ended: on an aircraft; short of their life in the air; on the ground or the sea; and those drawn on
		// under the ground.
		long EndOnPlane = 0, EndShort = 0, EndGround = 0, UnderGround = 0;
		// How near a round passed an airframe it did not end on, the nearest of each round: within 0.75 m, 1.5 m, 3 m.
		long Graze = 0, Close15 = 0, Close3 = 0;
		double MostReachOff = 0.0; // metres, the most a Gun effect's Reach lay from where its rounds are at its Tof
		long PastLife = 0;         // Gun effects whose Tof is past their round's life
		// The strikes, set beside the rounds (Close).
		long StrikesNoRound = 0, EndsNoStrike = 0, OffBody = 0, LocalOff = 0, StrikesNoBurst = 0;
		// How far a strike was drawn from where the nearest of its rounds ended, in the aircraft's own frame: the sum over
		// the strikes that had a round, and the most.
		double OffSum = 0.0, OffMost = 0.0;
		long GroundWrong = 0, GroundEndsBare = 0, GroundHitsBare = 0;
		// The bursts that struck (a run of strikes with one Id), by what their Moment came to.
		long BurstsStruck = 0, FighterBurstsStruck = 0, GunnerBurstsStruck = 0, BurstsForNothing = 0, BurstsUntold = 0;
		// The Moments that ran through to the damage: losses and hits; of them, those whose burst was seen to strike the
		// aircraft first, by rounds of the guns the Moment names; and the gunners' share of the losses.
		long Kills = 0, KillsSeen = 0, KillsNoFall = 0, GunnerKills = 0, GunnerKillsSeen = 0, HitsDone = 0, HitsSeen = 0;
		// A gunner's kill as the cameras are told it: those told; struck less than a second after he opened; with its
		// times out of order, no window named, or told late.
		long GunnerKillsTold = 0, GunnerKillsSoon = 0, GunnerKillsOdd = 0;
		double LeastLead = 1e9;
		std::map<std::string, int> Kinds, KindsDone;

		void Frame(const AirShow::Show& Show)
		{
			const double Air = Show.AirNow(), AirDt = Air - LastAir;
			const std::vector<AirShow::PlaneOut>& Planes = Show.Planes();
			std::map<int, const AirShow::PlaneOut*> ByKey;
			for (const AirShow::PlaneOut& P : Planes) ByKey[P.Key] = &P;
			for (const AirShow::Show::Moment& M : Show.Moments())
			{
				if (!Told.count(M.Id)) ToldAt[M.Id] = Air;
				Told[M.Id] = M;
			}
			auto FloorAt = [&Show](double X, double Y) { return Show.Ground ? Show.Ground(X, Y) : 0.0; };
			for (const AirShow::Fx& E : Show.Effects)
			{
				if (E.Kind == AirShow::FxKind::Hit)
				{
					if (E.Text == "ground" || E.Text == "water")
					{
						// (on the ground or the sea where it says, and told as what lies there)
						const double Floor = FloorAt(E.At.X, E.At.Y);
						if (std::fabs(E.At.Z - Floor) > 0.01 || (E.Text == "water") != (Floor <= 0.5)) ++GroundWrong;
						++GroundHits;
						Spurts.push_back(Low{ E.At, Air, Air });
						continue;
					}
					// (a strike that is not gunfire, as flak's splinters once the core tells them by their Text, is counted
					// apart: no round is owed it)
					if (E.Key <= 0) { ++OddHits; continue; }
					if (E.Text == "ship" || E.Text == "kill") { ++ShipHits; continue; }
					if (E.Text != "air") { ++Splinters; continue; }
					++Strikes;
					Strike St;
					St.Plane = E.Key;
					St.At = Air;
					St.Local = E.Aim;
					St.Id = E.Id;
					St.Nth = E.Nth;
					const auto Its = ByKey.find(E.Key);
					bool bOff = Its == ByKey.end(), bAskew = false;
					if (!bOff)
					{
						const AirShow::PlaneOut& P = *Its->second;
						bOff = !AirShow::InAirframe(ShapeFor(P), E.Aim, 0.45);
						bAskew = (Into(AxesOf(P), E.At - P.At) - E.Aim).Length() > 0.05;
					}
					OffBody += bOff ? 1 : 0;
					LocalOff += bAskew ? 1 : 0;
					if (bTell && (bOff || bAskew))
						std::fprintf(stderr, "gunnery: strike %s at %.2f on %d (burst %d, strike %d) at %.2f %.2f %.2f of its own frame\n", bOff ? "off the airframe" : "not where it is told", Air, E.Key, E.Id, E.Nth, E.Aim.X, E.Aim.Y, E.Aim.Z);
					LastStruck[E.Key] = Air;
					Struck.push_back(St);
					continue;
				}
				if (E.Kind == AirShow::FxKind::Fall)
				{
					++Falls;
					FellAt[E.Key] = Air;
					const auto Last = LastStruck.find(E.Key);
					FallsStruck += Last != LastStruck.end() && Air - Last->second < 2.0 ? 1 : 0;
					continue;
				}
				if (E.Kind != AirShow::FxKind::Gun) continue;
				if (ShipGuns::IsStream(E.Key)) { ++ShipStreams; continue; }
				Round R;
				R.Gun = E.Key;
				R.From = E.At;
				R.Vel = (E.Aim - E.At).Unit() * (E.Mv > 0.0 ? E.Mv : 870.0) + E.Vel;
				R.Born = Air;
				auto Row = Show.Rounds.find(E.What);
				if (Row == Show.Rounds.end()) Row = Show.Rounds.find("hmg");
				if (Row != Show.Rounds.end())
				{
					R.Drag = Row->second.Drag;
					R.Drop = Row->second.Drop;
					R.End = Row->second.Life;
				}
				if (E.Tof > 0.0)
				{
					// (the kit ends the tracer at the sooner of the two: the reach told must be where the rounds are then,
					// and an end told past the round's life is one the tracer never reaches)
					MostReachOff = std::max(MostReachOff, std::fabs((AirShow::RoundAt(R.From, R.Vel, E.Tof, R.Drag, R.Drop) - R.From).Length() - E.Reach));
					PastLife += E.Tof > R.End + 1e-6 ? 1 : 0;
					R.End = std::min(R.End, E.Tof);
					R.bEnds = true;
					++GunFxEnding;
				}
				Flying.push_back(R);
			}
			// The rounds fly on to this frame's time: through what, how near, and where they end. Each aircraft is taken
			// back along its own path of this frame to where it was when the round was at each step.
			std::vector<Vec> Moved(Planes.size());
			for (size_t N = 0; N < Planes.size(); ++N)
			{
				const auto Was = LastAt.find(Planes[N].Key);
				if (Was != LastAt.end()) Moved[N] = Planes[N].At - Was->second;
			}
			for (size_t K = 0; K < Flying.size();)
			{
				Round& R = Flying[K];
				const double Age = Air - R.Born, To = std::min(Age, R.End);
				const bool bOver = Age >= R.End;
				const Vec A = AirShow::RoundAt(R.From, R.Vel, R.LastAge, R.Drag, R.Drop), B = AirShow::RoundAt(R.From, R.Vel, To, R.Drag, R.Drop);
				const Vec Mid = (A + B) * 0.5;
				const double Half = (B - A).Length() * 0.5;
				int EndOn = -1;
				Vec EndLocal;
				for (size_t N = 0; N < Planes.size(); ++N)
				{
					const AirShow::PlaneOut& P = Planes[N];
					if (!P.bShown) continue;
					const double Step = Moved[N].Length();
					if ((P.At - Moved[N] * 0.5 - Mid).Length() > Half + P.Span + Step + 6.0) continue;
					const bool bOwn = P.Key == R.Gun / 64;
					const Axes Fr = AxesOf(P);
					const AirShow::Shape& Sh = ShapeFor(P);
					const Vec PlaneVel = AirDt > 1e-9 ? Moved[N] * (1.0 / AirDt) : Vec();
					const int Steps = std::max(2, static_cast<int>(Half * 2.0 / 0.4));
					bool bIn = false;
					double Least = 1e9;
					for (int S = 0; S <= Steps; ++S)
					{
						const double At = R.LastAge + (To - R.LastAge) * (static_cast<double>(S) / Steps);
						// (its own muzzle stands in or by its own skin: the first 2.5 m are its own)
						if (bOwn && At * 800.0 < 2.5) continue;
						const Vec L = Into(Fr, AirShow::RoundAt(R.From, R.Vel, At, R.Drag, R.Drop) - (P.At - PlaneVel * (Air - (R.Born + At))));
						// (a round that ends: where it ends is its strike, or the mark it stopped short of)
						const bool bLast = R.bEnds && At >= R.End - 0.0025;
						if (AirShow::InAirframe(Sh, L, 0.0))
						{
							if (bLast) { EndOn = P.Key; EndLocal = L; }
							else bIn = true;
							Least = 0.0;
						}
						else if (AirShow::InAirframe(Sh, L, 0.75))
						{
							Least = std::min(Least, 0.75);
							if (bLast && bOver && EndOn < 0) { EndOn = P.Key; EndLocal = L; }
						}
						else if (AirShow::InAirframe(Sh, L, 1.5)) Least = std::min(Least, 1.5);
						else if (AirShow::InAirframe(Sh, L, 3.0)) Least = std::min(Least, 3.0);
					}
					if (!bOwn && Least <= 3.0)
					{
						bool bHad = false;
						for (Pass& Near : R.Passed) if (Near.Plane == P.Key) { Near.Least = std::min(Near.Least, Least); bHad = true; }
						if (!bHad) R.Passed.push_back(Pass{ P.Key, Least });
					}
					if (!bIn) continue;
					if (bOwn)
					{
						// (a stream through its own aircraft is counted once a quarter second, not once a round)
						double& Last = LastThrough[std::make_pair(R.Gun, P.Key)];
						if (Last > 0.0 && Air - Last < 0.25) continue;
						Last = Air;
						++ThroughOwn;
						if (bTell) std::fprintf(stderr, "gunnery: at %.2f gun %d (craft %d slot %d) fires through its own aircraft (%s)\n", Air, R.Gun, R.Gun / 64, R.Gun % 64, P.Model.c_str());
						continue;
					}
					bool bHad = false;
					for (const Entry& In : R.Entered) bHad = bHad || In.Plane == P.Key;
					if (!bHad) R.Entered.push_back(Entry{ P.Key, P.bFighter, P.bFalling });
				}
				R.LastAge = To;
				// (drawn on under the ground or the sea: by more than its guns' standing apart can take them. The stream is
				// reckoned along the nose; a gun 4 m ahead on it ends as far on, and wing guns 3 m out that cross at 300 m
				// are 6 m out the other way at 900, which in a steep or banked dive is so far under. So 12 m.)
				if (!R.bUnder && B.Z < FloorAt(B.X, B.Y) - 12.0) R.bUnder = true;
				if (!bOver) { ++K; continue; }
				// Its flight over. An aircraft it went into and then ended in is struck, not passed through; any other it
				// went into, it was drawn through (a stream through one aircraft counted once a quarter second).
				for (const Entry& In : R.Entered)
				{
					if (In.Plane == EndOn) continue;
					double& Last = LastThrough[std::make_pair(R.Gun, In.Plane)];
					if (Last > 0.0 && Air - Last < 0.25) continue;
					Last = Air;
					if (In.bFalling) { ++ThroughFalling; continue; }
					++Through;
					++(R.bEnds ? ThroughEnded : ThroughFree);
					if (bTell) std::fprintf(stderr, "gunnery: at %.2f a round of gun %d (craft %d, %s) drawn through %s %d, which flies on (%s)\n", Air, R.Gun, R.Gun / 64, R.Gun % 64 >= 32 ? "a gunner's" : "fixed", In.bFighter ? "fighter" : "bomber", In.Plane, R.bEnds ? "it ended farther on" : "it flew out its life");
				}
				++RoundsFlown;
				if (EndOn >= 0)
				{
					++EndOnPlane;
					Ends.push_back(Ended{ EndOn, EndLocal, R.Born + R.End, R.Gun });
				}
				else if (R.bEnds && B.Z - FloorAt(B.X, B.Y) <= 3.0)
				{
					++EndGround;
					Downs.push_back(Low{ B, R.Born, R.Born + R.End });
				}
				else if (R.bEnds) ++EndShort;
				if (R.bUnder)
				{
					++UnderGround;
					if (bTell) std::fprintf(stderr, "gunnery: at %.2f a round of gun %d (craft %d slot %d) drawn on under the ground, to %.1f m under it%s\n", Air, R.Gun, R.Gun / 64, R.Gun % 64, FloorAt(B.X, B.Y) - B.Z, R.bEnds ? " where it ends" : ", flying out its life");
				}
				double Nearest = 1e9;
				for (const Pass& Near : R.Passed) if (Near.Plane != EndOn) Nearest = std::min(Nearest, Near.Least);
				if (Nearest <= 0.75) ++Graze;
				else if (Nearest <= 1.5) ++Close15;
				else if (Nearest <= 3.0) ++Close3;
				Flying[K] = Flying.back();
				Flying.pop_back();
			}
			LastAt.clear();
			for (const AirShow::PlaneOut& P : Planes) LastAt[P.Key] = P.At;
			LastAir = Air;
		}

		/** After the last frame: the strikes set beside the rounds, the bursts beside their Moments, the Moments beside
		 * the falls. */
		void Close()
		{
			// A strike drawn: was a round seen to end there (on that aircraft, within a fifth of a second)? A round that
			// ended on an aircraft: was a strike drawn on it?
			for (Strike& St : Struck)
			{
				for (Ended& En : Ends)
				{
					if (En.Plane != St.Plane || std::fabs(En.At - St.At) > 0.2) continue;
					En.bMatched = true;
					St.bMatched = true;
					St.Off = std::min(St.Off, (En.Local - St.Local).Length());
					St.From.insert(En.Gun);
				}
				if (St.Id <= 0) ++StrikesNoBurst;
				if (St.bMatched)
				{
					OffSum += St.Off;
					OffMost = std::max(OffMost, St.Off);
					continue;
				}
				++StrikesNoRound;
				if (bTell) std::fprintf(stderr, "gunnery: strike at %.2f on %d (burst %d, strike %d) with no round ended there\n", St.At, St.Plane, St.Id, St.Nth);
			}
			for (const Ended& En : Ends)
			{
				if (En.bMatched) continue;
				++EndsNoStrike;
				if (bTell) std::fprintf(stderr, "gunnery: a round of gun %d (craft %d) ended at %.2f on %d and no strike was drawn\n", En.Gun, En.Gun / 64, En.At, En.Plane);
			}
			// The ground's: each strike drawn there has a round ending by it (within the stream's scatter, 15 m), fired
			// no more than a moment after it or ended no more than a moment before; and a round ended there has a strike
			// drawn near where it ends (the strikes come at the gun's rate, some 13 a second, and the rounds flown here
			// one a frame: along a stream that walks fast over the ground the nearest strike may be 25 m on; so 40 m).
			for (Low& Down : Downs)
			{
				for (Low& Spurt : Spurts)
				{
					if (Spurt.From < Down.From - 0.3 || Spurt.From > Down.To + 0.3) continue;
					const double Apart = std::hypot(Spurt.At.X - Down.At.X, Spurt.At.Y - Down.At.Y);
					if (Apart <= 40.0) Down.bMatched = true;
					if (Apart <= 15.0) Spurt.bMatched = true;
				}
				GroundEndsBare += Down.bMatched ? 0 : 1;
				if (bTell && !Down.bMatched) std::fprintf(stderr, "gunnery: a round fired at %.2f ended on the ground at %.2f (%.0f %.0f) and no strike was drawn near\n", Down.From, Down.To, Down.At.X, Down.At.Y);
			}
			for (const Low& Spurt : Spurts)
			{
				GroundHitsBare += Spurt.bMatched ? 0 : 1;
				if (bTell && !Spurt.bMatched) std::fprintf(stderr, "gunnery: a strike on the ground at %.2f (%.0f %.0f) with no round ended near\n", Spurt.From, Spurt.At.X, Spurt.At.Y);
			}
			// The bursts that struck, each a run of strikes with its own number: what its Moment came to.
			std::map<int, int> Runs;
			for (const Strike& St : Struck) if (St.Id > 0) ++Runs[St.Id];
			for (const auto& [Id, Count] : Runs)
			{
				++BurstsStruck;
				const auto Its = Told.find(Id);
				if (Its == Told.end()) { ++BurstsUntold; continue; }
				++(Its->second.Gun >= 0 ? GunnerBurstsStruck : FighterBurstsStruck);
				if (Its->second.Stage == -1)
				{
					++BurstsForNothing;
					if (bTell) std::fprintf(stderr, "gunnery: burst %d (%s) drew %d strikes on %d and nothing came of it\n", Id, Its->second.Kind.c_str(), Count, Its->second.Victim);
				}
			}
			const std::set<std::string> Windows = { "nose", "chin", "top", "radio", "ball", "waistL", "waistR", "tail" };
			for (const auto& [Id, M] : Told)
			{
				++Kinds[M.Kind];
				// (what the eye had of it: the first strike of its burst drawn on the aircraft it names, and whether rounds
				// of the guns it names were seen to end there)
				double FirstHit = -1.0;
				bool bByThose = false;
				for (const Strike& St : Struck)
				{
					if (St.Id != Id || St.Plane != M.Victim) continue;
					if (M.FallAt >= 0.0 && St.At > M.FallAt + 1e-6) continue;
					if (FirstHit < 0.0) FirstHit = St.At;
					for (const int Gun : St.From) bByThose = bByThose || (Gun / 64 == M.Shooter && (M.Gun < 0 ? Gun % 64 < 32 : Gun >= M.Gun && Gun < M.Gun + 4));
				}
				const bool bGunnerKill = M.Kind == "gunner-kill";
				if (bGunnerKill && M.Stage >= 1)
				{
					// As the cameras are told it: when he opens, a second or more before his rounds strike, his window
					// named, his own bomber's mount; then the times in their order.
					++GunnerKillsTold;
					const double Struck0 = FirstHit >= 0.0 ? FirstHit : M.StrikeAt;
					LeastLead = std::min(LeastLead, Struck0 - M.OpenAt);
					const bool bSoon = Struck0 - M.OpenAt < 1.0 - 1e-6;
					const bool bOdd = !Windows.count(M.Station) || M.Gun < 0 || M.Gun / 64 != M.Shooter || ToldAt[Id] > M.OpenAt + 0.05 || M.StrikeAt < M.OpenAt ||
					                  (FirstHit >= 0.0 && std::fabs(M.StrikeAt - FirstHit) > 0.05) || (M.Stage == 2 && (M.FallAt < M.StrikeAt || M.Ended < M.FallAt));
					GunnerKillsSoon += bSoon ? 1 : 0;
					GunnerKillsOdd += bOdd ? 1 : 0;
					if (bTell && (bSoon || bOdd))
						std::fprintf(stderr, "gunnery: gunner's kill %d told %s: shooter %d gun %d window '%s' victim %d; told at %.2f, opens %.2f, strikes %.2f (seen %.2f), falls %.2f\n", Id, bOdd ? "wrong" : "too late for a camera",
						             M.Shooter, M.Gun, M.Station.c_str(), M.Victim, ToldAt[Id], M.OpenAt, M.StrikeAt, FirstHit, M.FallAt);
				}
				if (M.Stage != 2) continue;
				++KindsDone[M.Kind];
				const bool bSeen = FirstHit >= 0.0 && bByThose;
				if (M.Kind == "kill" || bGunnerKill)
				{
					// (and it goes down then: its fall begun as the Moment says, or within half a second after)
					const auto Fell = FellAt.find(M.Victim);
					const bool bFell = Fell != FellAt.end() && Fell->second > M.FallAt - 0.05 && Fell->second < M.FallAt + 0.5;
					++Kills;
					KillsNoFall += bFell ? 0 : 1;
					KillsSeen += bSeen && bFell ? 1 : 0;
					GunnerKills += bGunnerKill ? 1 : 0;
					GunnerKillsSeen += bGunnerKill && bSeen && bFell ? 1 : 0;
					if (bTell) std::fprintf(stderr, "gunnery: %s %d by %d on %d at %.2f: %s%s\n", M.Kind.c_str(), Id, M.Shooter, M.Victim, M.FallAt, bSeen ? "seen struck first" : FirstHit >= 0.0 ? "struck, but by no round of those guns" : "no strike drawn", bFell ? "" : ", and no fall begun then");
				}
				else
				{
					++HitsDone;
					HitsSeen += bSeen ? 1 : 0;
				}
			}
		}

	private:
		const AirShow::Shape& ShapeFor(const AirShow::PlaneOut& P)
		{
			const std::string Name = P.Model + (P.bFighter ? "/f" : "/b");
			auto It = Shapes.find(Name);
			if (It == Shapes.end()) It = Shapes.emplace(Name, AirShow::ShapeOf(P.Model, P.bFighter)).first;
			return It->second;
		}
		std::vector<Round> Flying;
		std::vector<Ended> Ends;
		std::vector<Strike> Struck;
		std::vector<Low> Downs, Spurts;
		std::map<std::pair<int, int>, double> LastThrough;
		std::map<int, Vec> LastAt;
		std::map<int, double> LastStruck, FellAt, ToldAt;
		std::map<int, AirShow::Show::Moment> Told;
		std::map<std::string, AirShow::Shape> Shapes;
		double LastAir = 0.0;
	};

	/** One play of a battle as the gunnery's watcher saw it, with the core's own checks of that play, and the frames a
	 * second it was played at. */
	struct GunPlay
	{
		GunWatch Watch;
		AirShow::Checks Check;
		double Fps = 60.0;
	};

	/**
	 * The recording played once more for the gunnery alone, with dice of its own, set as the scene sets the play it
	 * is like (its eye, its ground, its deck, the kit's gun budget), at so many frames a second. What the gunnery
	 * shows hangs on a chaos of small chances (which pilot's nose comes on when), so one play tells little: of six
	 * plays of the big Bremen raid, one showed no round through an aircraft and another seven.
	 * LowTo over 0: a made-over copy of the battle flown low, every group brought down together until its lowest
	 * bombers fly LowTo metres up, so that streams come down on the ground or the sea (in the recordings as they are,
	 * the fights are too high for any to).
	 */
	GunPlay FlyForGuns(const AirFeed::Recording& Rec, const AirShow::Show& Like, uint64_t Seed, double Fps, double LowTo, bool bTell)
	{
		GunPlay Out;
		Out.Watch.bTell = bTell;
		Out.Fps = Fps;
		AirShow::Show Show(Seed);
		AirFeed::Apply(Rec.Board, Show);
		Show.Eye = Like.Eye;
		Show.Ground = Like.Ground;
		Show.CloudTop = Like.CloudTop;
		Show.DeckAt = Like.DeckAt;
		Show.DeckReach = Like.DeckReach;
		Show.DeckBase = Like.DeckBase;
		Show.MostGuns = Like.MostGuns;
		double Lower = 0.0;
		if (LowTo > 0.0)
		{
			double Lowest = 1e9;
			for (const AirFeed::Frame& F : Rec.Views)
				for (const AirShow::Group& G : F.Groups)
					if (!G.bFighter && G.Alt > 50.0) Lowest = std::min(Lowest, G.Alt);
			Lower = Lowest < 1e8 ? std::max(0.0, Lowest - LowTo) : 0.0;
		}
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const AirFeed::Frame& F = Rec.Views[Taken++];
			if (Lower <= 0.0)
			{
				Show.Take(F.Head.T, F.Groups, F.Events, F.Ships);
				return;
			}
			// (what stands on the ground stays there; nothing is brought under 60 m)
			std::vector<AirShow::Group> Groups = F.Groups;
			std::vector<AirShow::Event> Events = F.Events;
			for (AirShow::Group& G : Groups) if (G.Alt > 50.0) G.Alt = std::max(60.0, G.Alt - Lower);
			for (AirShow::Event& E : Events) if (E.Alt > 50.0) E.Alt = std::max(60.0, E.Alt - Lower);
			Show.Take(F.Head.T, Groups, Events, F.Ships);
		};
		TakeNext();
		if (Taken < Rec.Views.size()) TakeNext();
		double T = Rec.Views.front().Head.T;
		const double EndT = Rec.Views.back().Head.T, Dt = 1.0 / Fps;
		while (T < EndT - 1e-9)
		{
			T = std::min(EndT, T + Dt);
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
			Show.Advance(T);
			Out.Watch.Frame(Show);
			Show.Effects.clear();
		}
		Out.Watch.Close();
		Out.Check = Show.Checked();
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: air_show_test <recording.json> [out-dir] [frames-per-second]\n");
		return 2;
	}
	const std::string Path = Argv[1];
	const std::string OutDir = Argc > 2 ? Argv[2] : ".";
	const double Fps = Argc > 3 ? std::atof(Argv[3]) : 60.0;
	// (AIR_SHOW_B17_FINE=1: the battle as the Battle rules card's "Finer B-17" draws it, her engines the newer file's,
	// AirShow::SetFineB17, as AAirBattle::B17FineRead sets it when the row is on)
	if (const char* Fine = std::getenv("AIR_SHOW_B17_FINE"); Fine && std::string(Fine) == "1")
	{
		AirShow::SetFineB17(true);
		std::printf("(the finer B-17: her engines as B17Fine.json's sockets put them)\n");
	}

	std::ifstream In(Path, std::ios::binary);
	if (!In)
	{
		std::fprintf(stderr, "cannot read %s\n", Path.c_str());
		return 2;
	}
	std::stringstream Buffer;
	Buffer << In.rdbuf();
	AirFeed::Recording Rec;
	std::string Error;
	const auto Read0 = std::chrono::steady_clock::now();
	if (!AirFeed::ReadRecording(Buffer.str(), Rec, Error))
	{
		std::fprintf(stderr, "%s: %s\n", Path.c_str(), Error.c_str());
		return 2;
	}
	const double ReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Read0).count();

	AirShow::Show Show(Rec.Seed ? Rec.Seed : 238);
	AirFeed::Apply(Rec.Board, Show);
	Show.Eye = AirShow::Vec(Rec.Board.TargetX, Rec.Board.TargetY - 6000.0, 5000.0);
	// Each bomber's bay, as the scene hands it to the core from the shape it draws (its model files beside the
	// recordings: Content/Data/AirBattles -> Content/Models; the scene's names for the battle's models).
	{
		const size_t Data = Path.rfind("/Data/AirBattles/");
		const std::string Models = Data == std::string::npos ? std::string("Content/Models/") : Path.substr(0, Data) + "/Models/";
		const std::map<std::string, std::string> Named = { { "B-17", "B17" }, { "B5N", "Kate" }, { "G4M", "Betty" }, { "G3M", "Nell" }, { "SM.79", "SM79" }, { "Pe-2", "Pe2" }, { "LeO 451", "LeO451" }, { "D.520", "D520" }, { "D3A", "Val" }, { "SBD", "Dauntless" }, { "TBD", "Devastator" }, { "TBF", "Avenger" } };
		std::set<std::string> Asked;
		for (const AirFeed::Frame& View : Rec.Views)
			for (const AirShow::Group& G : View.Groups)
			{
				if (G.bFighter || !Asked.insert(G.Model).second) continue;
				const auto Name = Named.find(G.Model);
				AirShow::Show::BayPlace Bay;
				if (BayFromModel(Models + (Name == Named.end() ? G.Model : Name->second) + ".json", Bay)) Show.BaysOf[G.Model] = Bay;
			}
	}
	// A battle with no stage flies over the board's own land, handed to the core and the sky as AAirBattle::Open hands
	// it (the pre-check on round 14: Bremen's bombs had burst as water columns).
	const std::shared_ptr<const AirGround::BoardLand> BoardGround =
		Rec.Board.StageKey.empty() ? std::make_shared<const AirGround::BoardLand>(Rec.Board.Land) : std::shared_ptr<const AirGround::BoardLand>();
	const bool bBoardGround = BoardGround && !BoardGround->IsEmpty();
	if (bBoardGround) Show.Ground = [BoardGround](double X, double Y) { return BoardGround->At(X, Y); };
	// The day's sky, laid as the scene lays it (the sea under it here where there is a stage: the stage's hills are the
	// engine's; with no stage, the board's land). SkyAt, the sea's, is the one the made-up days below are laid from.
	const AirSky::Placing SkyAt = AirFeed::PlacingOf(Rec.Board, Rec.Views.front().Groups, Rec.Seed);
	AirSky::Placing SkyHere = SkyAt;
	if (bBoardGround)
	{
		SkyHere.Ground = Show.Ground;
		SkyHere.GroundTop = AirGround::BoardLand::LandHigh;
	}
	const auto Sky0 = std::chrono::steady_clock::now();
	const AirSky::Field Sky = AirSky::Lay(Rec.Board.Cloud, SkyHere);
	const double SkyLayMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Sky0).count();
	Show.CloudTop = Sky.DeckTop;
	if (Sky.DeckTop > 0.0)
	{
		Show.DeckAt = [&Sky](double X, double Y)
		{
			AirShow::DeckSpan Here;
			Here.Hide = Sky.DeckAt(X, Y, &Here.Foot);
			return Here;
		};
		Show.DeckReach = AirSky::Reach * 5.0 / 6.0; // (as the scene sets it)
	}

	// The track: every half battle second, each aircraft and each small thing; every effect as it comes.
	std::ofstream Track(OutDir + "/" + Rec.Key + "-track.csv");
	Track << "t,kind,key,group,side,fighter,model,x,y,z,heading,pitch,bank,shown,falling,ground,extra\n";
	std::map<std::string, int> FxCount;
	std::map<std::string, int> Sounds;
	// (with no stage: what comes down on the board's land and on the sea, and what is told wrong or laid off the ground)
	int BoardLandBombs = 0, BoardSeaBombs = 0, BoardLandFalls = 0, BoardSeaFalls = 0, BoardWrong = 0;
	int MostPlanes = 0, MostThings = 0, LiveGuns = 0, MostLiveGuns = 0;
	// (the bomb bays, Brian 21 November 05:44Z "add working doors in release 1": each bomber whose doors were seen
	// open, and each stick heard where it lands, by its first bomb, or a bomb alone)
	std::map<int, double> BayMost, BayOpenAt, StickAt;
	int BayBombsSeen = 0, BayBombsOff = 0, GunKeysTwice = 0;
	// (the strategy camera's focus, once a second: the fight's groups, else the raider's bombers; how many aircraft
	// within 1.2 km of the middle of their groups, as it looked before, and of the thickest crowd, AirDesk::CrowdOf)
	int FocusSamples = 0, FocusOldEmpty = 0, FocusThin = 0, FocusAnchor = -1;
	double FocusNextT = 0.0;
	// (the guns fire back picture's moment, found as the scene finds it, AAirBattle's GunsReach: the first light gun
	// laid on an attacker within 1.6 km of her ship, the aircraft the one nearest where it is laid; then how far he is
	// from her 2.5 and 10 s on, the picture taken 2.5 real seconds on and the clock running 1 to 4 times)
	double GunsT = -1.0, GunsRange = 0.0, GunsUp = 0.0, GunsFar25 = -1.0, GunsFar10 = -1.0;
	int GunsShip = -1, GunsOn = -1;
	std::string GunsModel;
	std::vector<double> BayBombAhead;
	// And she lets it go wings level (the coordinator on release 1's Mac film: the B-17 banked 50 to 60 degrees as her
	// first bomb left; the art director: wings level from the doors opening): the most bank of a bay bomber from her
	// doors standing open to a second after her last bomb, and the most of any bay bomber flying at any time.
	std::map<int, double> BombsGoneAt;
	double StickBankMost = 0.0, BayBomberBankMost = 0.0;
	int StickHits = 0, OneHits = 0;
	// Drop on the lead and the rally (Brian, 22 November 04:11Z): each level box's spread, the median distance of its
	// bombers flying in sight from their middle, at its first bomb and the most over the next 120 air seconds (of the
	// bombers in its shape: not a straggler fallen out of it, nor a model made still closing up, Checks::NotInBox).
	std::map<int, double> BoxFirstBomb, BoxSpreadAt, BoxSpreadMost;
	double BoxNextT = 0.0;
	// (seen from outside the core: each level box's bombers' first bombs, by her key, and her box; and each bay bomber's
	// doors, the air time they were shut again after her last bomb, and those still open BayShutS after it as she flew)
	// The strike is on (Show::StrikeOn, the audit's 4.2): every frame with a bomb or a torpedo in the air, or a bomber
	// diving (her nose more than 30 degrees down, not falling), it must be on; and once off, within 5 air s of the last
	// of those or of the last hit on a ship shown (the bomb or torpedo having struck), or (release 2) on a piece of a
	// column under a raid on troops (MarkHit: a strafing pass's hit shown as its rounds land).
	long StrikeNeed = 0, StrikeMiss = 0, StrikeOnFrames = 0, StrikeEnds = 0;
	// The vapour trails (PlaneOut::Vapour, the audit's 4.12): none under the day's trail height; the most seen.
	// (escorts at their stations by role, the raids answer's Part 3.4: each close escort's or top cover's height over
	// its bombers and its side of them, sampled each air second while its group is not fighting; and each one's weave,
	// the air seconds between its turns from one side to the other)
	std::map<std::string, std::vector<double>> EscortOver;
	std::map<std::string, std::array<long, 2>> EscortFlank;
	std::map<int, std::array<double, 4>> EscortWas; // key: lateral, its way (+1/-1), the lateral and air time at its last turn
	std::map<std::string, std::vector<double>> EscortLegs;
	// (falls the eye can follow, the audit's 4.3: each falling aircraft's bank frame to frame, and the air time)
	std::map<int, std::pair<double, double>> FallBankWas;
	long FallBankSteps = 0, FallBankFast = 0;
	// (each fall's end, a crash or a splash: its velocity against the aircraft's last FallMove, at the same place)
	AirShow::Vec EndFrom, EndVel;
	bool bEndFrom = false;
	long EndsSeen = 0, EndsVelWrong = 0;
	double EndSpeedLeast = 1e9, EndSpeedMost = 0.0;
	double FallBankMost = 0.0;
	long VapourLow = 0, VapourOn = 0;
	double VapourMost = 0.0;
	double StrikeLast = -1e9, StrikeLateMost = 0.0;
	bool bStrikeWas = false;
	std::map<int, int> StickBox;
	std::map<int, double> BayShutAt;
	std::set<int> BayShutLate;
	double BayShutMost = 0.0;
	std::map<int, bool> GunOn;
	// The ships' anti-aircraft guns (Brian, 21 November 18:06Z: "for the ships being attacked by planes, they can fire
	// machine gun fire and flak guns at them just like real life"): every stream from a muzzle on its own ship, she
	// afloat as the scene shows her (her sinking shown, her guns silent); the rest is the gunnery's own count.
	std::map<int, AirShow::Ship> ShipsSeen;
	std::set<int> ShipsDownShown;
	long ShipStreams = 0, ShipStreamsOff = 0;
	auto ShipStreamFx = [&](const AirShow::Fx& E)
	{
		++ShipStreams;
		const auto It = ShipsSeen.find((E.Key - ShipGuns::StreamBase) / 64);
		if (It == ShipsSeen.end() || ShipsDownShown.count(It->first)) { ++ShipStreamsOff; return; }
		const AirShow::Ship& Sh = It->second;
		const AirShow::Vec Rel = E.At - Sh.At;
		const double Along = Rel.X * std::sin(Sh.Face) + Rel.Y * std::cos(Sh.Face);
		const double Across = Rel.X * std::cos(Sh.Face) - Rel.Y * std::sin(Sh.Face);
		if (std::fabs(Along) > Sh.Long * 0.5 + 1.0 || std::fabs(Across) > Sh.Beam * 0.5 + 0.5 || E.At.Z < 0.0 || E.At.Z > 30.0) ++ShipStreamsOff;
	};
	// (each gun's muzzle found on the aircraft it fires from: none on one falling or out of sight, Brian 11-20 14:20Z
	// "after the bomber crew bails out, they can't keep shooting"; and its key naming that aircraft, never the next)
	int GunsFromFalling = 0, GunsFromHidden = 0, GunsMisKeyed = 0, HitsOnFalling = 0, FiringAtFalling = 0;
	// The box's return fire is a web about the fighter, never one point (the art director's D6, 17:27Z): gunners of
	// two bombers laid on one fighter (aim points within 60 m) never lay on the same spot within 1.5 m.
	long WebPairs = 0, WebOnePoint = 0;
	// And each gunner's gun swings no faster than a man (or a power turret) lays it: how fast each gun's line turns
	// between frames, degrees a second (one in five of a .50's rounds is a tracer, about 0.37 s apart).
	std::map<int, std::pair<double, AirShow::Vec>> GunLine;
	std::vector<double> Swings;
	// A ride on the fighters that fight the bombers shows what they fight (Brian on the Bremen film, 20 November: "where
	// are the german fighters?"): every second, a ride chosen among each side's fighters as the scene chooses it
	// (AirDesk::ChooseRide); where that side has a fighter closing on a bomber within 2.5 km, the ride is on one.
	int RideSamples = 0, RideOnBomber = 0, RideHeadOn = 0, RideFar = 0;
	double RideGapSum = 0.0, RideNextT = 0.0;
	// The chase view (Brian, 21 November 05:52Z: ride behind one plane of a squadron, "move from one plane to the next"):
	// every second, each squadron's line stepped right round both ways (every aircraft once, back to the first, only
	// ones flying and shown, the step back undoing the step on); a fall steps on to one still flying; the squadrons
	// stepped round (his own before the enemy's, each once); and the count on the plate.
	long ChaseLines = 0, ChaseAircraft = 0, ChaseFalls = 0, ChaseBad = 0;
	std::string ChaseFirstBad;
	auto ChaseWrong = [&](const std::string& What) { if (ChaseBad++ == 0) ChaseFirstBad = What; };
	// The fighter pilots fly with purpose (Brian on the Bremen film, 20 November 20:42Z: "they were kinda just flying
	// around doing nothing"): among the fighters of groups in a fight, when each group's fight began and its guns first
	// fired (on the air clock); and every half battle second, that each pilot has a task the ride cameras know, and
	// each wingman a leader of his own group still flying.
	std::map<int, double> FightBegan, FirstGuns;
	long PilotSamples = 0, PilotNoTask = 0, PilotOddTask = 0, WingmenSeen = 0, WingmenAdrift = 0;
	bool bEscortFought = false;
	double PilotNextT = 0.0;
	const std::set<std::string> TaskWords = { "attacking", "breaking", "covering", "going home", "positioning", "escorting", "evading", "closing" };
	double WorstFrameMs = 0.0, TotalMs = 0.0;
	long Frames = 0;
	int EventsLost = 0, EventsChutes = 0;
	std::map<std::string, int> EventsHow; // (the ways down the battle names, as the scene plays them: collide as its pieces)
	int EventsHits = 0, EventsFiresOut = 0, EventsRegain = 0;
	double FirstBombAir = -1.0; // (the air clock when the first bombs fall)
	std::vector<AirSky::Vec> FallsAt; // (where each fall began, and on the air clock when, in Z)
	std::vector<double> FallsAir;
	std::map<int, double> FallFromX;                      // (each fall open: where it began, east and west)
	std::vector<std::pair<double, double>> FallSpans;     // (each fall ended: where it began and where it ended)
	// Falls and parachutes that end in the air (on a cumulus day, none: every one reaches the sea or runs out of time).
	std::map<int, double> FallBegan;
	int FallsEndAloft = 0, ChutesVanished = 0, ChutesDown = 0;
	std::vector<AirShow::Thing> ChutesBefore;
	// The falls as the scene hands them to the effects kit: a start, its moves, an end; and the kit's room for debris.
	AirShow::DebrisRoom Debris;
	std::map<int, std::string> FallsOpen;
	int FallStarts = 0, FallStrays = 0, MostDebris = 0;
	// The scene's room for trails, as on the newer kit (24) and an older one (10), a burning aircraft's own first.
	AirShow::TrailRoom Trails60, Trails24, Trails10;
	Trails60.Most = 60; // (the scene's with the kit's far trails on, game.air_far)
	Trails24.Most = 24;
	Trails10.Most = 10;
	int PieceTrails = 0, OwnTrails = 0;
	// A dead engine smokes for as long as it is out (Brian 15:29Z, the art director's G1): every aircraft the scene
	// counts an engine dead on has its own trail lit, and none of its oil or streak is stopped while it still flies.
	std::map<int, std::string> TrailKinds;
	std::set<int> DeadSeen, DeadDark;
	int DeadCleared = 0;
	// No group still flies the place's stand-in once a view has named its model.
	int Guessed = 0;
	for (const AirFeed::Frame& F : Rec.Views)
		for (const AirShow::Group& G : F.Groups)
			Guessed += G.bModelGuessed ? 1 : 0;
	// (a model falls only for its share of the losses, AR1: the battle's models, each loss's extra.models; a straggler
	// not shown falling out has her later news told, never shown)
	int EventsLosses = 0, EventsNoModel = 0;
	std::set<std::pair<int, int>> StragsNotShown;
	for (const AirFeed::Frame& F : Rec.Views)
		for (const AirShow::Event& E : F.Events)
		{
			if (E.What == "air:straggle" && E.Models <= 0) StragsNotShown.insert({ E.Unit, E.Strag });
			if (E.What == "air:lost")
			{
				++EventsLosses;
				EventsNoModel += E.Models <= 0 ? 1 : 0;
				EventsLost += std::max(0, E.Models);
				EventsChutes += E.Models > 0 ? E.Chutes : 0;
				EventsHow[E.How.empty() ? (E.Kind == "explode" || E.Kind == "spin" ? E.Kind : std::string("burn")) : E.How] += std::max(0, E.Models);
			}
			if (E.What == "air:hit") { ++EventsHits; EventsFiresOut += E.Mark == "fireout" ? 1 : 0; }
			if (E.What == "air:fireout" && !(E.Strag >= 0 && StragsNotShown.count({ E.Unit, E.Strag }))) ++EventsFiresOut;
			if (E.What == "air:regain") ++EventsRegain;
		}
	// The gunnery as the eye will see it (GunWatch, above): a round of every stream flown by the kit's own flight, with
	// every strike, fall and moment. And what the battle could have laid to guns, counted from the recording itself:
	// every loss, hit survived and bomber shot out of her box that it gives to neither flak, a collision nor a
	// falling wreck (the most the core may hold for a burst; an older recording names no guns for some hits).
	GunWatch Gunnery;
	Gunnery.bTell = std::getenv("AIR_SHOW_GUNNERY") != nullptr;
	int EventsGunLost = 0, EventsGunHits = 0, EventsGunnerLost = 0;
	for (const AirFeed::Frame& F : Rec.Views)
		for (const AirShow::Event& E : F.Events)
		{
			if (E.bByFlak || (E.What != "air:lost" && E.What != "air:hit" && E.What != "air:straggle")) continue;
			if (E.What == "air:lost" && (E.How == "struck" || E.How == "collide")) continue;
			// (a loss that brings no model down, a straggler not shown falling out: nothing is shown, so nothing is owed a
			// burst; AR1, Event::Models 0)
			if ((E.What == "air:lost" || E.What == "air:straggle") && E.Models <= 0) continue;
			if (E.What != "air:lost") ++EventsGunHits;
			else if (E.By >= 0 || E.GunBox >= 0) { ++EventsGunLost; EventsGunnerLost += E.GunBox >= 0 ? 1 : 0; }
		}

	const double Dt = 1.0 / Fps;
	double NextSample = 0.0;
	// The game holds one view in hand ahead of what it shows, so the picture between views has somewhere to go.
	size_t Taken = 0;
	auto TakeNext = [&]()
	{
		const AirFeed::Frame& F = Rec.Views[Taken++];
		Show.Take(F.Head.T, F.Groups, F.Events, F.Ships, &F.Marks);
		for (const AirShow::Ship& Sh : F.Ships) ShipsSeen[Sh.Id] = Sh;
	};
	TakeNext();
	if (Taken < Rec.Views.size()) TakeNext();
	double T = Rec.Views.front().Head.T;
	const double EndT = Rec.Views.back().Head.T;
	while (T < EndT - 1e-9)
	{
		T = std::min(EndT, T + Dt);
		while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
		const auto F0 = std::chrono::steady_clock::now();
		Show.Advance(T);
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - F0).count();
		WorstFrameMs = std::max(WorstFrameMs, Ms);
		TotalMs += Ms;
		++Frames;

		const std::vector<AirShow::PlaneOut>& PlanesNow = Show.Planes();
		if (GunsT >= 0.0 && (GunsFar25 < 0.0 || GunsFar10 < 0.0) && ShipsSeen.count(GunsShip))
		{
			for (const AirShow::PlaneOut& P : PlanesNow)
			{
				if (P.Key != GunsOn) continue;
				const double Far = (P.At - ShipsSeen[GunsShip].At).Length();
				if (GunsFar25 < 0.0 && T >= GunsT + 2.5) GunsFar25 = Far;
				if (GunsFar10 < 0.0 && T >= GunsT + 10.0) GunsFar10 = Far;
			}
		}
		if (T >= FocusNextT)
		{
			FocusNextT = T + 1.0;
			std::vector<int> Picked;
			AirShow::Vec Sum;
			for (const auto& [Id, G] : Show.Groups())
				if (G.Doing == AirShow::Act::Fight) { Picked.push_back(Id); Sum += AirShow::Vec(G.X, G.Y, G.Alt); }
			if (Picked.empty())
				for (const auto& [Id, G] : Show.Groups())
					if (G.Side == 0 && !G.bFighter && G.Doing != AirShow::Act::Down) { Picked.push_back(Id); Sum += AirShow::Vec(G.X, G.Y, G.Alt); }
			AirShow::Vec Crowd;
			if (!Picked.empty() && AirDesk::CrowdOf(PlanesNow, Picked, 1200.0, FocusAnchor, Crowd))
			{
				const AirShow::Vec Old = Sum * (1.0 / static_cast<double>(Picked.size()));
				int NearOld = 0, NearNew = 0;
				for (const AirShow::PlaneOut& P : PlanesNow)
				{
					if (!P.bShown || P.bFalling || P.bOnGround) continue;
					NearOld += (P.At - Old).Length() <= 1200.0 ? 1 : 0;
					NearNew += (P.At - Crowd).Length() <= 1200.0 ? 1 : 0;
				}
				++FocusSamples;
				FocusOldEmpty += NearOld == 0 ? 1 : 0;
				FocusThin += NearNew < 3 ? 1 : 0;
			}
		}
		{
			// (one muzzle to a key: two guns under one key are one stream to the kit, drawn as a line from one muzzle to
			// the other; a B-17's ninth gun row took the next aircraft's first key, the sound review's find)
			std::map<int, AirShow::Vec> KeyAt;
			for (const AirShow::Fx& E : Show.Effects)
			{
				if (E.Kind != AirShow::FxKind::Gun) continue;
				const auto Had = KeyAt.find(E.Key);
				if (Had == KeyAt.end()) KeyAt[E.Key] = E.At;
				else if ((Had->second - E.At).Length() > 0.5) ++GunKeysTwice;
			}
		}
		{
			std::vector<const AirShow::Fx*> Gunners;
			for (const AirShow::Fx& E : Show.Effects)
				if (E.Kind == AirShow::FxKind::Gun && E.Key % 64 >= 32 && !ShipGuns::IsStream(E.Key)) Gunners.push_back(&E); // (a bomber's gunners; a ship's guns have their own row)
			for (const AirShow::Fx* E : Gunners)
			{
				const AirShow::Vec Way = (E->Aim - E->At).Unit();
				const auto Was = GunLine.find(E->Key);
				const double AirNow = Show.AirNow(); // (on the air clock, which the guns are laid on)
				if (Was != GunLine.end() && AirNow - Was->second.first < 0.1 && AirNow > Was->second.first)
					Swings.push_back(std::acos(std::max(-1.0, std::min(1.0, Way.Dot(Was->second.second)))) * 57.29577951308232 / (AirNow - Was->second.first));
				GunLine[E->Key] = { AirNow, Way };
			}
			for (size_t I = 0; I < Gunners.size(); ++I)
				for (size_t J = I + 1; J < Gunners.size(); ++J)
				{
					if (Gunners[I]->Key / 64 == Gunners[J]->Key / 64) continue;
					const double Apart = (Gunners[I]->Aim - Gunners[J]->Aim).Length();
					if (Apart > 60.0) continue;
					++WebPairs;
					WebOnePoint += Apart < 1.5 ? 1 : 0;
				}
		}
		for (const AirShow::PlaneOut& P : PlanesNow)
		{
			if (P.Bay > 0.0) BayMost[P.Key] = std::max(BayMost[P.Key], P.Bay);
			if (P.Bay >= 0.99 && !BayOpenAt.count(P.Key)) BayOpenAt[P.Key] = Show.AirNow();
		}
		for (const AirShow::Thing& Th : Show.Things())
			if (Th.Kind == AirShow::ThingKind::Bomb && !Th.bTorpedo && !StickAt.count(Th.Key))
			{
				StickAt[Th.Key] = Show.AirNow();
				for (const AirShow::PlaneOut& P : PlanesNow)
				{
					if (P.Key != Th.Key || P.bFighter) continue;
					const auto Box = Show.Groups().find(P.Group);
					if (Box != Show.Groups().end() && Box->second.Strike != "dive" && Box->second.Strike != "torpedo")
					{
						if (!BoxFirstBomb.count(P.Group)) BoxFirstBomb[P.Group] = Show.AirNow();
						StickBox[P.Key] = P.Group;
					}
				}
				// (her first bomb, in her own frame: inside her bay's racked load, where her model has one)
				for (const AirShow::PlaneOut& P : PlanesNow)
				{
					if (P.Key != Th.Key) continue;
					const auto Bay = Show.BaysOf.find(P.Model);
					if (Bay == Show.BaysOf.end()) break;
					const double D = 3.14159265358979 / 180.0, H = P.Heading * D, G = P.Pitch * D, B = P.Bank * D;
					const AirShow::Vec F(std::sin(H) * std::cos(G), std::cos(H) * std::cos(G), std::sin(G)), R0(std::cos(H), -std::sin(H), 0.0);
					const AirShow::Vec U0 = R0.Cross(F), R = R0 * std::cos(B) - U0 * std::sin(B), U = U0 * std::cos(B) + R0 * std::sin(B);
					const AirShow::Vec Rel = Th.At - P.At;
					const double Along = Rel.Dot(F), Across = Rel.Dot(R), Over = Rel.Dot(U);
					const AirShow::Show::BayPlace& Was = Bay->second;
					++BayBombsSeen;
					const bool bIn = Along <= Was.Fore + 0.3 && Along >= Was.Aft - 0.3 && std::fabs(Across) <= 1.0 && std::fabs(Over - Was.Up) <= 1.0;
					if (!bIn)
					{
						++BayBombsOff;
						std::fprintf(stderr, "bomber %d's first bomb %.2f m ahead, %.2f right, %.2f up of her middle; her bay %.2f to %.2f ahead, %.2f up\n", P.Key, Along, Across, Over, Was.Aft, Was.Fore, Was.Up);
					}
					BayBombAhead.push_back(Along);
					break;
				}
			}
		for (const AirShow::PlaneOut& P : PlanesNow)
		{
			if (!BayMost.count(P.Key) || P.bFalling || !P.bShown || P.bOnGround) continue;
			BayBomberBankMost = std::max(BayBomberBankMost, std::fabs(P.Bank));
			if (P.bBombsGone && !BombsGoneAt.count(P.Key)) BombsGoneAt[P.Key] = Show.AirNow();
			const auto Gone = BombsGoneAt.find(P.Key);
			if (Gone != BombsGoneAt.end() && !BayShutAt.count(P.Key))
			{
				if (P.Bay <= 0.0)
				{
					BayShutAt[P.Key] = Show.AirNow();
					BayShutMost = std::max(BayShutMost, Show.AirNow() - Gone->second);
				}
				else if (Show.AirNow() > Gone->second + Show.BayShutS + 0.1) BayShutLate.insert(P.Key);
			}
			const auto Open = BayOpenAt.find(P.Key);
			if (Open != BayOpenAt.end() && (Gone == BombsGoneAt.end() || Show.AirNow() < Gone->second + 1.0))
				StickBankMost = std::max(StickBankMost, std::fabs(P.Bank));
		}
		if (Show.AirNow() >= BoxNextT)
		{
			BoxNextT = Show.AirNow() + 0.5;
			for (const auto& [Box, First] : BoxFirstBomb)
			{
				// (from its first bomb through its turn for home, until it has rallied: at most 300 air s)
				const auto Over = Show.Checked().RallyOver.find(Box);
				if (Show.AirNow() - First > 300.0 || (Over != Show.Checked().RallyOver.end() && Show.AirNow() > Over->second)) continue;
				std::vector<double> Xs, Ys, Far;
				for (const AirShow::PlaneOut& P : PlanesNow)
					if (P.Group == Box && !P.bFighter && !P.bFalling && P.bShown && !P.bOnGround && !Show.Checked().NotInBox.count(P.Key)) { Xs.push_back(P.At.X); Ys.push_back(P.At.Y); }
				if (Xs.size() < 3) continue;
				std::vector<double> Sx = Xs, Sy = Ys;
				std::nth_element(Sx.begin(), Sx.begin() + Sx.size() / 2, Sx.end());
				std::nth_element(Sy.begin(), Sy.begin() + Sy.size() / 2, Sy.end());
				for (size_t I = 0; I < Xs.size(); ++I) Far.push_back(std::hypot(Xs[I] - Sx[Sx.size() / 2], Ys[I] - Sy[Sy.size() / 2]));
				std::nth_element(Far.begin(), Far.begin() + Far.size() / 2, Far.end());
				const double Spread = Far[Far.size() / 2];
				if (!BoxSpreadAt.count(Box)) BoxSpreadAt[Box] = Spread;
				BoxSpreadMost[Box] = std::max(BoxSpreadMost[Box], Spread);
			}
		}
		for (const AirShow::PlaneOut& P : PlanesNow)
		{
			if (!P.bFiring || P.Target < 0) continue;
			for (const AirShow::PlaneOut& T_ : PlanesNow) if (T_.Key == P.Target && T_.bFalling) { ++FiringAtFalling; break; }
		}
		{
			const auto& GroupsNow = Show.Groups();
			const bool bSample = T >= PilotNextT;
			if (bSample) PilotNextT = T + 0.5;
			for (const AirShow::PlaneOut& P : PlanesNow)
			{
				if (!P.bFighter || P.bFalling || P.bOnGround || !P.bShown) continue;
				const auto G = GroupsNow.find(P.Group);
				if (G == GroupsNow.end() || G->second.Doing != AirShow::Act::Fight) continue;
				if (!FightBegan.count(P.Group)) FightBegan[P.Group] = Show.AirNow();
				if (P.bFiring && !FirstGuns.count(P.Group)) FirstGuns[P.Group] = Show.AirNow();
				if (!bSample) continue;
				++PilotSamples;
				bEscortFought = bEscortFought || G->second.With >= 0;
				PilotNoTask += P.Task.empty() ? 1 : 0;
				PilotOddTask += !P.Task.empty() && !TaskWords.count(P.Task) ? 1 : 0;
				if (P.Leader < 0) continue;
				++WingmenSeen;
				bool bLed = false;
				for (const AirShow::PlaneOut& L : PlanesNow) if (L.Key == P.Leader && L.Group == P.Group && !L.bFalling) bLed = true;
				WingmenAdrift += bLed ? 0 : 1;
			}
		}
		if (T >= RideNextT)
		{
			RideNextT = T + 1.0;
			{
				std::vector<AirDesk::Tag> Tags;
				for (const auto& [Id, G] : Show.Groups())
				{
					AirDesk::Tag Tg;
					Tg.Id = Id;
					Tg.Side = G.Side;
					Tg.bMine = G.Side == 0;
					Tg.bFighter = G.bFighter;
					Tags.push_back(Tg);
				}
				const std::vector<int> Squadrons = AirDesk::RideSquadrons(Tags, PlanesNow);
				bool bTheirsSeen = false;
				for (const int Id : Squadrons)
				{
					const AirDesk::Tag* Tg = nullptr;
					for (const AirDesk::Tag& Each : Tags) if (Each.Id == Id) Tg = &Each;
					if (!Tg) { ChaseWrong("a squadron with no tag"); continue; }
					if (!Tg->bMine) bTheirsSeen = true;
					else if (bTheirsSeen) ChaseWrong("his own squadron after the enemy's");
				}
				if (!Squadrons.empty())
				{
					int Sq = AirDesk::RideSquadronStep(Tags, PlanesNow, -1, 1);
					std::set<int> SquadronsSeen;
					for (size_t K = 0; K < Squadrons.size(); ++K)
					{
						if (!SquadronsSeen.insert(Sq).second) ChaseWrong("a squadron twice round the ring");
						const int Next = AirDesk::RideSquadronStep(Tags, PlanesNow, Sq, 1);
						if (AirDesk::RideSquadronStep(Tags, PlanesNow, Next, -1) != Sq) ChaseWrong("up does not undo down");
						Sq = Next;
					}
					if (Sq != Squadrons.front()) ChaseWrong("the squadrons not round to the first");
				}
				for (const auto& [Id, G] : Show.Groups())
				{
					const std::vector<int> Line = AirDesk::RideLine(PlanesNow, Id);
					std::set<int> Falling;
					for (const AirShow::PlaneOut& P : PlanesNow)
					{
						if (P.Group == Id && P.bFalling) Falling.insert(P.Key);
					}
					if (Line.empty())
					{
						if (AirDesk::RideStep(PlanesNow, Id, -1, 0) != -1 || std::find(Squadrons.begin(), Squadrons.end(), Id) != Squadrons.end())
							ChaseWrong("a squadron with none up offered");
						continue;
					}
					++ChaseLines;
					const int First = AirDesk::RideStep(PlanesNow, Id, -1, 0);
					if (First != Line.front()) ChaseWrong("a squadron's ride not from its leader");
					if (AirDesk::RideCount(PlanesNow, Id, First) != "Aircraft 1 of " + std::to_string(Line.size())) ChaseWrong("the count on the plate");
					std::set<int> Seen;
					int Key = First;
					for (size_t K = 0; K < Line.size(); ++K)
					{
						bool bGood = false;
						for (const AirShow::PlaneOut& P : PlanesNow)
						{
							if (P.Key == Key) bGood = P.Group == Id && P.bShown && !P.bFalling && !P.bOnGround;
						}
						if (!bGood) ChaseWrong("a step onto one not flying or of another squadron");
						if (!Seen.insert(Key).second) ChaseWrong("an aircraft twice along the line");
						if (AirDesk::RideStep(PlanesNow, Id, Key, 0) != Key) ChaseWrong("a ride not kept to");
						const int Next = AirDesk::RideStep(PlanesNow, Id, Key, 1);
						if (AirDesk::RideStep(PlanesNow, Id, Next, -1) != Key) ChaseWrong("back does not undo on");
						Key = Next;
						++ChaseAircraft;
					}
					if (Key != First) ChaseWrong("the line not round to its leader");
					for (const int Gone : Falling)
					{
						++ChaseFalls;
						const int On = AirDesk::RideStep(PlanesNow, Id, Gone, 1);
						const auto After = std::upper_bound(Line.begin(), Line.end(), Gone);
						if (On != (After == Line.end() ? Line.front() : *After)) ChaseWrong("a fall not stepped on to the next flying");
					}
				}
			}
			for (int Side = 0; Side < 2; ++Side)
			{
				std::vector<AirShow::PlaneOut> Mine;
				bool bCloser = false;
				for (const AirShow::PlaneOut& P : PlanesNow)
				{
					if (P.Side != Side) continue;
					Mine.push_back(P);
					if (!P.bFighter || P.Target < 0 || !P.bShown || P.bFalling) continue;
					for (const AirShow::PlaneOut& Q : PlanesNow)
						if (Q.Key == P.Target && !Q.bFighter && Q.bShown && !Q.bFalling && (Q.At - P.At).Length() <= 2500.0) bCloser = true;
				}
				if (!bCloser) continue;
				AirShow::Vec Middle;
				int Count = 0;
				for (const AirShow::PlaneOut& P : PlanesNow) if (P.bShown) { Middle += P.At; ++Count; }
				Middle = Middle * (1.0 / std::max(1, Count));
				const AirDesk::Subject Pick = AirDesk::ChooseRide(Mine, PlanesNow, Middle, -1);
				++RideSamples;
				for (const AirShow::PlaneOut& Q : PlanesNow)
				{
					if (Q.Key != Pick.Other) continue;
					for (const AirShow::PlaneOut& P : PlanesNow)
					{
						if (P.Key != Pick.Plane) continue;
						const double Gap = (Q.At - P.At).Length();
						RideGapSum += Gap;
						RideFar += Gap > 2500.0 ? 1 : 0;
						RideOnBomber += Q.bFighter ? 0 : 1;
						RideHeadOn += AirDesk::HeadOn(P, PlanesNow) > 0.5 ? 1 : 0;
					}
				}
			}
		}
		for (const AirShow::Fx& E : Show.Effects)
		{
			if (E.Kind == AirShow::FxKind::TrailStart) TrailKinds[E.Key] = E.What;
			if (E.Kind == AirShow::FxKind::TrailStop)
			{
				const auto Was = TrailKinds.find(E.Key);
				if (Was != TrailKinds.end() && (Was->second == "oil" || Was->second == "streak"))
					for (const AirShow::PlaneOut& P : PlanesNow)
						if (P.Key == E.Key / 8 && P.bShown && !P.bFalling && !P.bOnGround) ++DeadCleared;
				if (Was != TrailKinds.end()) TrailKinds.erase(Was);
			}
		}
		for (const AirShow::PlaneOut& P : PlanesNow)
		{
			if (P.DeadEngines.empty() || !P.bShown || P.bFalling || P.bOnGround) continue;
			DeadSeen.insert(P.Key);
			if (TrailKinds.find(P.Key * 8 + 1) == TrailKinds.end()) DeadDark.insert(P.Key);
		}
		for (const AirShow::Fx& E : Show.Effects)
		{
			const char* Name = FxName(E.Kind);
			if (E.Kind == AirShow::FxKind::Gun && ShipGuns::IsStream(E.Key))
			{
				ShipStreamFx(E);
				// (a battleship or a carrier where the battle has one, as the scene picks)
				auto Capital = [](const AirShow::Ship& Sh) { return Sh.Class == "bb" || Sh.Class == "bc" || Sh.Class == "cv" || Sh.Class == "cvl"; };
				bool bCapitals = false;
				for (const auto& Seen : ShipsSeen) bCapitals = bCapitals || Capital(Seen.second);
				const auto Her = ShipsSeen.find((E.Key - ShipGuns::StreamBase) / 64);
				if (GunsT < 0.0 && (!bCapitals || (Her != ShipsSeen.end() && Capital(Her->second))))
				{
					const AirShow::PlaneOut* On = nullptr;
					double Nearest = 400.0;
					for (const AirShow::PlaneOut& P : PlanesNow)
					{
						const double Off = (P.At - E.Aim).Length();
						if (P.bShown && !P.bOnGround && !P.bFalling && Off < Nearest) { Nearest = Off; On = &P; }
					}
					if (On && (On->At - E.At).Length() <= 1600.0)
					{
						GunsT = T;
						GunsShip = (E.Key - ShipGuns::StreamBase) / 64;
						GunsOn = On->Key;
						GunsRange = (On->At - E.At).Length();
						GunsUp = On->At.Z;
						GunsModel = On->Model;
					}
				}
			}
			else if (E.Kind == AirShow::FxKind::Gun)
			{
				// (the aircraft its key names, which must carry the muzzle)
				const AirShow::PlaneOut* Own = nullptr;
				for (const AirShow::PlaneOut& P : PlanesNow) if (P.Key == E.Key / 64) Own = &P;
				const double Off = Own ? (Own->At - E.At).Length() : 1e18;
				if (!Own || Off > Own->Span * 0.75)
				{
					++GunsMisKeyed;
					if (std::getenv("AIR_SHOW_GUNKEYS") && GunsMisKeyed < 40)
						std::fprintf(stderr, "gun key %d (craft %d slot %d %s) %.1f m off its craft\n", E.Key, E.Key / 64, E.Key % 64, E.What.c_str(), Off);
				}
				else
				{
					GunsFromFalling += Own->bFalling ? 1 : 0;
					GunsFromHidden += Own->bShown ? 0 : 1;
				}
			}
			if (E.Kind == AirShow::FxKind::Hit && E.Text != "kill") // (a ship's rounds bringing one down, as it begins to fall: the ships' row)
			{
				for (const AirShow::PlaneOut& P : PlanesNow)
					if (P.bFalling && (P.At - E.At).Length() < P.Span * 0.6) { ++HitsOnFalling; break; }
			}
			++FxCount[Name];
			if (E.Kind == AirShow::FxKind::Sound) ++Sounds[E.What];
			if (E.Kind == AirShow::FxKind::Gun) GunOn[E.Key] = true;
			if (E.Kind == AirShow::FxKind::BombHit && FirstBombAir < 0.0) FirstBombAir = Show.AirNow();
			if (E.Kind == AirShow::FxKind::BombHit) { StickHits += E.Text == "stick" ? 1 : 0; OneHits += E.Text == "one" ? 1 : 0; }
			if (bBoardGround && (E.Kind == AirShow::FxKind::BombHit || ((E.Kind == AirShow::FxKind::Crash || E.Kind == AirShow::FxKind::Splash) && E.What != "torpedo")))
			{
				// (judged by every edge of every ring tried plainly, not by the banded ground asked again; the pre-check)
				const double Floor = PlainLand(Rec.Board.Land, E.At.X, E.At.Y) ? AirGround::BoardLand::LandHigh : 0.0;
				const bool bOnLand = Floor > 0.5;
				const bool bToldLand = E.Kind == AirShow::FxKind::BombHit ? E.What == "land" : E.Kind == AirShow::FxKind::Crash;
				if (bToldLand != bOnLand || std::fabs(E.At.Z - Floor) > 0.01) ++BoardWrong;
				(E.Kind == AirShow::FxKind::BombHit ? (bOnLand ? BoardLandBombs : BoardSeaBombs) : (bOnLand ? BoardLandFalls : BoardSeaFalls)) += 1;
			}
			if (E.Kind == AirShow::FxKind::GunStop) GunOn.erase(E.Key);
			if (E.Kind == AirShow::FxKind::ShipSunk) ShipsDownShown.insert(E.Key);
			if (E.Kind == AirShow::FxKind::Fall)
			{
				++FallStarts;
				FallsAt.push_back(AirSky::Vec(E.At.X, E.At.Y, E.At.Z));
				FallsAir.push_back(Show.AirNow());
				FallFromX[E.Key] = E.At.X;
				FallBegan[E.Key] = Show.AirNow();
				Debris.Take(Show.AirNow(), E);
				if (E.What != "explode" && E.What != "burn" && E.What != "spin") ++FallStrays;
				if (E.What != "explode") FallsOpen[E.Key] = E.What;
			}
			if (E.Kind == AirShow::FxKind::FallMove && !FallsOpen.count(E.Key)) ++FallStrays;
			if (E.Kind == AirShow::FxKind::FallMove) { EndFrom = E.At; EndVel = E.Vel; bEndFrom = true; }
			if ((E.Kind == AirShow::FxKind::Crash || E.Kind == AirShow::FxKind::Splash) && (E.What == "plane" || E.What == "ditch" || E.What == "belly"))
			{
				++EndsSeen;
				const bool bSame = bEndFrom && std::fabs(EndFrom.X - E.At.X) < 1e-6 && std::fabs(EndFrom.Y - E.At.Y) < 1e-6;
				if (!bSame || (E.Vel - EndVel).Length() > 1e-9 || E.Vel.Length() < 1.0) ++EndsVelWrong;
				EndSpeedLeast = std::min(EndSpeedLeast, E.Vel.Length());
				EndSpeedMost = std::max(EndSpeedMost, E.Vel.Length());
			}
			if (E.Kind == AirShow::FxKind::FallStop && FallsOpen.erase(E.Key) == 0) ++FallStrays;
			if (E.Kind == AirShow::FxKind::FallStop && E.What == "boom") FallBegan.erase(E.Key); // (blown apart in the air: it ends there)
			if (E.Kind == AirShow::FxKind::FallStop)
			{
				const auto From = FallFromX.find(E.Key);
				if (From != FallFromX.end())
				{
					if (E.What != "boom") FallSpans.push_back({ From->second, E.At.X });
					FallFromX.erase(From);
				}
			}
			if (E.Kind == AirShow::FxKind::FallStop && E.What != "boom")
			{
				const auto Began = FallBegan.find(E.Key);
				const double Under = Show.Ground ? Show.Ground(E.At.X, E.At.Y) : 0.0;
				if (Began != FallBegan.end() && E.At.Z > Under + 2.5 && Show.AirNow() - Began->second < 399.0) ++FallsEndAloft;
				if (Began != FallBegan.end()) FallBegan.erase(Began);
			}
			if (E.Kind == AirShow::FxKind::TrailStart)
			{
				const bool bPiece = E.Text == "piece" || E.What == "streak";
				PieceTrails += bPiece ? 1 : 0;
				OwnTrails += bPiece ? 0 : 1;
				int Out = 0;
				Trails60.Start(E.Key, bPiece, Out);
				Trails24.Start(E.Key, bPiece, Out);
				Trails10.Start(E.Key, bPiece, Out);
			}
			if (E.Kind == AirShow::FxKind::TrailStop)
			{
				Trails60.Stop(E.Key);
				Trails24.Stop(E.Key);
				Trails10.Stop(E.Key);
			}
			if (E.Kind != AirShow::FxKind::Gun && E.Kind != AirShow::FxKind::TrailMove && E.Kind != AirShow::FxKind::FallMove && E.Kind != AirShow::FxKind::Hit)
			{
				std::string Text = E.What;
				for (char& C : Text) if (C == ',' || C == '\n') C = ';';
				Track << Num(Show.Shown(), 3) << ",fx:" << Name << "," << E.Key << ",0," << E.Side << ",0," << Text << ","
				      << Num(E.At.X) << "," << Num(E.At.Y) << "," << Num(E.At.Z) << ",0,0,0,1,0,0," << Num(E.Size) << "\n";
			}
			else if (E.Kind == AirShow::FxKind::Gun && Show.Shown() >= NextSample - 1e-9)
			{
				Track << Num(Show.Shown(), 3) << ",fx:gun," << E.Key << ",0," << E.Side << ",0," << E.What << ","
				      << Num(E.At.X) << "," << Num(E.At.Y) << "," << Num(E.At.Z) << ",0,0,0,1,0,0,"
				      << Num(E.At.X + E.Aim.X * 400.0) << ";" << Num(E.At.Y + E.Aim.Y * 400.0) << ";" << Num(E.At.Z + E.Aim.Z * 400.0) << "\n";
			}
		}
		Gunnery.Frame(Show);
		{
			bool bNeed = false;
			for (const AirShow::Thing& Th : Show.Things()) bNeed = bNeed || Th.Kind == AirShow::ThingKind::Bomb;
			for (const AirShow::PlaneOut& P : PlanesNow) bNeed = bNeed || (!P.bFighter && !P.bFalling && P.bShown && !P.bOnGround && P.Pitch < -30.0);
			for (const AirShow::Fx& E : Show.Effects)
				if (E.Kind == AirShow::FxKind::ShipHit || E.Kind == AirShow::FxKind::ShipSunk || E.Kind == AirShow::FxKind::MarkHit) StrikeLast = Show.AirNow();
			for (const AirShow::PlaneOut& P : PlanesNow)
			{
				VapourLow += P.Vapour > 0.0 && (Show.TrailM <= 0.0 || P.At.Z < Show.TrailM) ? 1 : 0;
				VapourOn += P.Vapour > 0.0 ? 1 : 0;
				VapourMost = std::max(VapourMost, P.Vapour);
				if (!P.bFalling || !P.bShown)
				{
					FallBankWas.erase(P.Key);
					continue;
				}
				const auto Was = FallBankWas.find(P.Key);
				if (Was != FallBankWas.end() && Show.AirNow() > Was->second.second + 1e-9)
				{
					const double Rate = std::fabs(std::remainder(P.Bank - Was->second.first, 360.0)) / (Show.AirNow() - Was->second.second);
					++FallBankSteps;
					FallBankMost = std::max(FallBankMost, Rate);
					FallBankFast += Rate > 600.0 ? 1 : 0;
				}
				FallBankWas[P.Key] = { P.Bank, Show.AirNow() };
			}
			const bool bOn = Show.StrikeOn();
			StrikeOnFrames += bOn ? 1 : 0;
			if (bNeed)
			{
				++StrikeNeed;
				StrikeMiss += bOn ? 0 : 1;
				StrikeLast = Show.AirNow();
			}
			if (bStrikeWas && !bOn)
			{
				++StrikeEnds;
				StrikeLateMost = std::max(StrikeLateMost, Show.AirNow() - StrikeLast);
			}
			// (still on as the battle ends: as late as it has been on since)
			if (bOn && T >= EndT - 1e-9) StrikeLateMost = std::max(StrikeLateMost, Show.AirNow() - StrikeLast);
			bStrikeWas = bOn;
		}
		Show.Effects.clear();
		if (std::getenv("AIR_SHOW_TRACE") && std::fmod(Show.Shown(), 10.0) < Dt * 0.999)
			std::fprintf(stderr, "== %.1f\n%s", Show.Shown(), Show.Describe().c_str());
		MostDebris = std::max(MostDebris, Debris.Holding(Show.AirNow()));
		LiveGuns = static_cast<int>(GunOn.size());
		MostLiveGuns = std::max(MostLiveGuns, LiveGuns);
		int Shown = 0;
		for (const AirShow::PlaneOut& P : Show.Planes()) if (P.bShown) ++Shown;
		MostPlanes = std::max(MostPlanes, Shown);
		MostThings = std::max(MostThings, static_cast<int>(Show.Things().size()));
		{
			// (each parachute last frame found again this frame, where it should have drifted; one not found has come
			// down if it was at the sea, else it vanished in the air)
			std::vector<AirShow::Thing> ChutesNow;
			for (const AirShow::Thing& Th : Show.Things())
				if (Th.Kind == AirShow::ThingKind::Chute) ChutesNow.push_back(Th);
			for (const AirShow::Thing& Was : ChutesBefore)
			{
				const AirShow::Vec Next = Was.At + Was.Vel * Dt;
				bool bFound = false;
				for (const AirShow::Thing& Now : ChutesNow)
					if ((Now.At - Next).Length() < 6.0 && std::fabs(Now.Age - Was.Age) < 3.0 * Dt + 1e-6) { bFound = true; break; }
				if (bFound) continue;
				if (Was.At.Z <= 2.5 + std::fabs(Was.Vel.Z) * Dt * 2.0) ++ChutesDown;
				else if (Was.Age < 1499.0) ++ChutesVanished;
			}
			ChutesBefore.swap(ChutesNow);
		}

		if (Show.Shown() >= NextSample - 1e-9)
		{
			NextSample += 0.5;
			for (const AirShow::PlaneOut& P : Show.Planes())
			{
				const auto Gi = Show.Groups().find(P.Group);
				if (!P.bFighter || P.bFalling || !P.bShown || P.bOnGround || Gi == Show.Groups().end()) continue;
				const AirShow::Group& Eg = Gi->second;
				if (Eg.With < 0 || (Eg.Role != "close" && Eg.Role != "top") || Eg.Doing == AirShow::Act::Fight) { EscortWas.erase(P.Key); continue; }
				const auto Bi = Show.Groups().find(Eg.With);
				if (Bi == Show.Groups().end() || Bi->second.bFighter) continue;
				const AirShow::Group& Bg = Bi->second;
				const double Lateral = (P.At.X - Bg.X) * std::cos(Bg.Face) - (P.At.Y - Bg.Y) * std::sin(Bg.Face);
				EscortOver[Eg.Role].push_back(P.At.Z - Bg.Alt);
				EscortFlank[Eg.Role][Lateral > 0.0 ? 1 : 0]++;
				auto Was = EscortWas.find(P.Key);
				if (Was == EscortWas.end()) { EscortWas[P.Key] = { Lateral, 0.0, Lateral, -1.0 }; continue; }
				std::array<double, 4>& W = Was->second;
				const double Way = Lateral > W[0] + 1.0 ? 1.0 : Lateral < W[0] - 1.0 ? -1.0 : W[1];
				// (a turn: its way across reversed, at least 150 m from where it last turned)
				if (W[1] != 0.0 && Way != W[1] && std::fabs(W[0] - W[2]) > 150.0)
				{
					if (W[3] >= 0.0) EscortLegs[Eg.Role].push_back(Show.AirNow() - W[3]);
					W[2] = W[0];
					W[3] = Show.AirNow();
				}
				W[0] = Lateral;
				W[1] = Way;
			}
			for (const AirShow::PlaneOut& P : Show.Planes())
				Track << Num(Show.Shown(), 3) << ",plane," << P.Key << "," << P.Group << "," << P.Side << "," << (P.bFighter ? 1 : 0) << ","
				      << P.Model << "," << Num(P.At.X) << "," << Num(P.At.Y) << "," << Num(P.At.Z) << "," << Num(P.Heading) << ","
				      << Num(P.Pitch) << "," << Num(P.Bank) << "," << (P.bShown ? 1 : 0) << "," << (P.bFalling ? 1 : 0) << ","
				      << (P.bOnGround ? 1 : 0) << "," << Num(P.Span) << "\n";
			for (const AirShow::Thing& S : Show.Things())
				Track << Num(Show.Shown(), 3) << ",thing:" << (S.Kind == AirShow::ThingKind::Chute ? "chute" : S.Kind == AirShow::ThingKind::Bomb ? (S.bTorpedo ? "torpedo" : "bomb") : "piece")
				      << "," << S.Key << ",0,0,0,," << Num(S.At.X) << "," << Num(S.At.Y) << "," << Num(S.At.Z) << "," << Num(S.Heading)
				      << ",0,0,1," << (S.bOpen ? 1 : 0) << ",0," << Num(S.Size) << "\n";
			for (const auto& G : Show.Groups())
				Track << Num(Show.Shown(), 3) << ",group," << G.first << "," << G.first << "," << G.second.Side << "," << (G.second.bFighter ? 1 : 0) << ","
				      << G.second.Model << "," << Num(G.second.X) << "," << Num(G.second.Y) << "," << Num(G.second.Alt) << ","
				      << Num(G.second.Face * 180.0 / 3.14159265358979) << ",0,0,1,0,0," << G.second.Figures << "\n";
		}
	}
	Track.close();

	const AirShow::Checks& C = Show.Checked();
	std::printf("%s  (%s, %s)  %zu views to %.1f battle s, seed %llu\n", Rec.Key.c_str(), Rec.Board.Title.c_str(), Rec.Board.Date.c_str(),
	            Rec.Views.size(), EndT, static_cast<unsigned long long>(Rec.Seed));
	std::printf("  read %.0f ms; %ld frames at %.0f a second; %.3f ms a frame on average, %.2f ms at worst\n",
	            ReadMs, Frames, Fps, TotalMs / std::max(1L, Frames), WorstFrameMs);
	std::printf("  most aircraft shown at once %d, most small things %d, most guns firing at once %d (budget %d)\n",
	            MostPlanes, MostThings, MostLiveGuns, Show.MostGuns);
	std::printf("  falls %d (battle lost %d), parachutes %d (battle %d), passes %d, breaks under %d / over %d\n",
	            C.Falls, EventsLost, C.Chutes, EventsChutes, C.Passes, C.UnderBreaks, C.OverBreaks);
	std::printf("  gun frames %d, hits %d, flak bursts %d, bombs %d\n", C.GunFrames, C.Hits, C.FlakBursts, C.Bombs);
	// (the pilots: how soon after its fight opened the first group's guns fired, the soonest of them)
	double SoonestGuns = -1.0;
	for (const auto& [Id, Began] : FightBegan)
	{
		const auto Fired = FirstGuns.find(Id);
		if (Fired != FirstGuns.end() && (SoonestGuns < 0.0 || Fired->second - Began < SoonestGuns)) SoonestGuns = Fired->second - Began;
	}
	std::printf("  pilots: %d fights, first guns %.1f air s after a fight opens; wingmen %.0f m from their leaders (%ld of %ld steps over 500 m); threats %d, evasions %d, covers %d, guard chases %d\n",
	            C.Fights, SoonestGuns, C.WingSamples ? C.WingSum / C.WingSamples : 0.0, C.WingFar, C.WingSamples, C.Threats, C.Evasions, C.Covers, C.GuardChases);
	std::printf("  pilots: falls to fighters %d, %d of them under those fighters' guns (%d held for the guns to bear); %ld pilot samples, %ld with no task; lowest in a fight %.0f m; hurt %d, dry %d\n",
	            C.FallsByFighters, C.FallsUnderFire, C.FallsHeld, PilotSamples, PilotNoTask, C.LowestFight > 1e8 ? 0.0 : C.LowestFight, C.Hurt, C.Dry);
	std::printf("  pilots: %d scissors (an overshoot, then each rolling back into the other), %d rolls back in them, %d won from behind, the longest %.1f air s; %d pulls up and over, %d half-rolls away\n",
	            C.Scissors, C.ScissorTurns, C.ScissorWins, C.LongestScissors, C.YoYos, C.SplitS);
	// (the gunnery: what the battle laid to guns and what the core says it showed of it; then what a round of every
	// stream, flown by the kit's flight, was seen to do, in this play)
	Gunnery.Close();
	std::printf("  gunnery: to guns %d losses (%d to gunners) and %d hits; the core says %d and %d struck first, %d and %d from a burst before, %d and %d with no strike; gunners' kills seen %d;"
	            " bursts %d fighters' (%d struck, %d asked) and %d gunners' (%d struck); stream steps %ld (%ld held, %ld laid wide, %ld ended short, %ld on the ground)\n",
	            C.ToGuns, C.GunnerKills, C.HitsToGuns, C.ToGunsStruck, C.HitsStruck, C.ToGunsLate, C.HitsLate, C.ToGunsUnseen, C.HitsUnseen, C.GunnerKillsSeen,
	            C.Bursts, C.BurstsOn, C.BurstsAsked, C.GunnerBursts, C.GunnerBurstsOn, C.StreamSteps, C.HeldSteps, C.WideSteps, C.ShortSteps, C.GroundSteps);
	std::printf("  gunnery, seen: %ld rounds flown, %ld told an end (%ld on an aircraft, %ld in the air, %ld on the ground); %ld strikes on aircraft, %.2f m from where a round ended on the mean, %.2f at most;"
	            " %ld bursts struck (%ld fighters', %ld gunners'); losses struck first %ld of %ld told, hits %ld of %ld; %ld of %ld falls with a strike on them in the 2 s before\n",
	            Gunnery.RoundsFlown, Gunnery.GunFxEnding, Gunnery.EndOnPlane, Gunnery.EndShort, Gunnery.EndGround, Gunnery.Strikes,
	            Gunnery.OffSum / static_cast<double>(std::max(1L, Gunnery.Strikes - Gunnery.StrikesNoRound)), Gunnery.OffMost,
	            Gunnery.BurstsStruck, Gunnery.FighterBurstsStruck, Gunnery.GunnerBurstsStruck, Gunnery.KillsSeen, Gunnery.Kills, Gunnery.HitsSeen, Gunnery.HitsDone, Gunnery.FallsStruck, Gunnery.Falls);
	std::printf("  gunnery, moments told:");
	for (const auto& [Kind, Count] : Gunnery.Kinds) std::printf(" %s %d (%d through to the damage)", Kind.c_str(), Count, Gunnery.KindsDone.count(Kind) ? Gunnery.KindsDone.at(Kind) : 0);
	std::printf("\n");
	// One play tells little of the gunnery (FlyForGuns, above): so the battle is flown five times more for it alone,
	// each with dice of its own (the seed and 7919 times the play's number), the last two at half the frames a second
	// (the game's frames are not always these), and the gunnery's rows below hold over all six. And twice more made over
	// to fly low, its lowest bombers 150 m up, where streams come down on the ground or the sea: only the ground's row
	// reads those.
	std::vector<GunPlay> GunPlays, LowPlays;
	GunPlays.push_back(GunPlay{ Gunnery, C, Fps });
	for (uint64_t Again = 1; Again < 6; ++Again) GunPlays.push_back(FlyForGuns(Rec, Show, (Rec.Seed ? Rec.Seed : 238) + Again * 7919, Again < 4 ? Fps : Fps * 0.5, 0.0, Gunnery.bTell));
	for (uint64_t Again = 0; Again < 2; ++Again) LowPlays.push_back(FlyForGuns(Rec, Show, (Rec.Seed ? Rec.Seed : 238) + Again * 7919, Again < 1 ? Fps : Fps * 0.5, 150.0, Gunnery.bTell));
	for (size_t K = 0; K < GunPlays.size(); ++K)
	{
		const GunWatch& W = GunPlays[K].Watch;
		const AirShow::Checks& Ck = GunPlays[K].Check;
		std::printf("  gunnery, play %zu (%.0f frames a second): falls %d; losses to guns seen struck first %ld of %d (gunners' %ld of %d), hits %ld of %d; %ld bursts struck (%ld for nothing); through %ld, its own %ld, one falling %ld;"
		            " ends with no strike %ld of %ld, strikes with no round %ld of %ld, off the airframe %ld, not where told %ld; within 0.75 m %ld, 1.5 m %ld, 3 m %ld of %ld rounds\n",
		            K + 1, GunPlays[K].Fps, Ck.Falls, W.KillsSeen, Ck.ToGuns, W.GunnerKillsSeen, Ck.GunnerKills, W.HitsSeen, Ck.HitsToGuns, W.BurstsStruck, W.BurstsForNothing, W.Through, W.ThroughOwn, W.ThroughFalling,
		            W.EndsNoStrike, W.EndOnPlane, W.StrikesNoRound, W.Strikes, W.OffBody, W.LocalOff, W.Graze, W.Close15, W.Close3, W.RoundsFlown);
	}
	for (size_t K = 0; K < LowPlays.size(); ++K)
	{
		const GunWatch& W = LowPlays[K].Watch;
		const AirShow::Checks& Ck = LowPlays[K].Check;
		std::printf("  gunnery, flown low, play %zu (%.0f frames a second): falls %d; streams on the ground %ld of %ld steps; %ld rounds ended there (%ld with no strike near), %ld strikes drawn there (%ld with no round near, %ld told or laid wrong);"
		            " %ld rounds drawn on under it\n",
		            K + 1, LowPlays[K].Fps, Ck.Falls, Ck.GroundSteps, Ck.StreamSteps, W.EndGround, W.GroundEndsBare, W.GroundHits, W.GroundHitsBare, W.GroundWrong, W.UnderGround);
	}
	double SwingMid = 0.0, Swing90 = 0.0, Swing99 = 0.0;
	if (!Swings.empty())
	{
		std::sort(Swings.begin(), Swings.end());
		auto At_ = [&](double Q) { return Swings[std::min(Swings.size() - 1, static_cast<size_t>(Q * static_cast<double>(Swings.size())))]; };
		SwingMid = At_(0.5); Swing90 = At_(0.9); Swing99 = At_(0.99);
	}
	std::printf("  gunners' lines swing %.1f deg/s (half), %.1f (9 in 10), %.1f (99 in 100); %ld pairs on one fighter, %ld on one spot\n",
	            SwingMid, Swing90, Swing99, WebPairs, WebOnePoint);
	std::printf("  chase view: %ld squadron lines stepped round (%ld aircraft), %ld falls stepped on from, %ld wrong%s%s\n",
	            ChaseLines, ChaseAircraft, ChaseFalls, ChaseBad, ChaseBad ? ", first: " : "", ChaseFirstBad.c_str());
	std::printf("  rides on the fighters at the bombers: %d seconds with one closing, the ride on a bomber in %d, head-on in %d, its quarry %.0f m off on the mean\n",
	            RideSamples, RideOnBomber, RideHeadOn, RideGapSum / std::max(1, RideSamples));
	std::printf("  debris: %d falls given it, %d left without (%d of them blown apart; room %d puffs, most held at once %d)\n",
	            Debris.Given, Debris.Refused, Debris.RefusedBlasts, Debris.Most, MostDebris);
	std::printf("  trails: %d an aircraft's own, %d pieces'; room 60: most lit %d, refused %d (%d an aircraft's own), pieces put out %d;"
	            " room 24: most lit %d, refused %d (%d an aircraft's own), pieces put out %d; room 10: refused %d (%d an aircraft's own)\n",
	            OwnTrails, PieceTrails, Trails60.Peak, Trails60.Refused, Trails60.RefusedOwn, Trails60.PutOut, Trails24.Peak, Trails24.Refused, Trails24.RefusedOwn,
	            Trails24.PutOut, Trails10.Refused, Trails10.RefusedOwn);
	std::printf("  sky: %s%s from %.0f to %.0f m (zone %s, deck chance %.2f), bands %.0f and %.0f m; %zu clouds, %zu puffs, cover %.2f; laid in %.1f ms\n",
	            Rec.Board.Cloud.bGiven ? "" : "(none given) ", Sky.Sky.bDeck ? "a deck" : "scattered cumulus", Sky.Base, Sky.Top, Rec.Board.Cloud.Zone.c_str(),
	            Rec.Board.Cloud.Chance, SkyAt.Band, SkyAt.LowBand, Sky.Clouds.size(), Sky.Puffs.size(), Sky.Cover, SkyLayMs);
	{
		// Each group's model as the scene makes it: the first view's, which the feed now fills from the next view to name it.
		std::map<std::string, int> Made;
		for (const AirShow::PlaneOut& P : Show.Planes()) ++Made[std::to_string(P.Group) + ":" + P.Model];
		std::printf("  models still flying at the end:");
		for (const auto& P : Made) std::printf(" %s x%d", P.first.c_str(), P.second);
		std::printf("\n");
	}
	{
		// The named aces the views carry (extra.ace, on a group's lead unit only): each with his group, his victories
		// and their nations as the feed reads them, for the scene's marks.
		std::map<std::string, std::string> Aces;
		for (const AirFeed::Frame& F : Rec.Views)
			for (const AirShow::Group& G : F.Groups)
			{
				if (G.AceName.empty()) continue;
				std::string Row = "group " + std::to_string(G.Id) + ", " + std::to_string(G.AceKills) + " victories:";
				for (const auto& P : G.AceBy) Row += " " + P.first + " " + std::to_string(P.second);
				Aces[G.AceName] = Row;
			}
		if (!Aces.empty())
		{
			std::printf("  aces:");
			for (const auto& P : Aces) std::printf(" %s (%s)", P.first.c_str(), P.second.c_str());
			std::printf("\n");
		}
	}
	std::printf("  effects:");
	for (const auto& P : FxCount) std::printf(" %s %d", P.first.c_str(), P.second);
	std::printf("\n  sounds:");
	for (const auto& P : Sounds) std::printf(" %s %d", P.first.c_str(), P.second);
	std::printf("\n");

	// The flyover before the battle (Brian's, approved 19 November): its shots over the battle's first groups as the
	// scene plays them, the battle at x1 under them, a frame every thirtieth of a second.
	int FlyNoLook = 0, FlyLow = 0, FlyJumps = 0, FlyIntoPlane = 0;
	// (the raid's and the guard's shots, the art director on build 1750: the guard's lead three 120 to 180 pixels wide
	// in the 2881 by 1786 picture at its picture's moment, the raid's nearest three (its trailing element) about 120,
	// 100 to 160, with three quarters of the raid in the picture ahead of them (her note on build 1760); never fewer
	// than three of the group in the picture after the shot's first second; and a harbour held in the target shot's
	// lower third. The flyover's bars cover the picture's top and bottom 195 rows.)
	const double FlyFocal = 1440.5 / std::tan(AirDesk::Flyover::Lens * 0.5 * 3.14159265358979 / 180.0);
	const double FlyTop = 195.0, FlyFoot = 1591.0, FlyThird = FlyTop + (FlyFoot - FlyTop) * 2.0 / 3.0;
	const bool bFlyHarbour = Rec.Board.PlaceKind == "harbour";
	double FlyLeadLeast = 1e9, FlyLeadMost = 0.0, FlyTrailLeast = 1e9, FlyTrailMost = 0.0, FlyHarbourRow = -1.0;
	int FlyLeastInFrame = 1 << 30, FlyLeadPictures = 0, FlyRaidShown = 0, FlyRaidInFrame = 0;
	std::set<int> FlyShots;
	bool bFlyBegun = false;
	AirDesk::Flyover Flyover;
	{
		AirShow::Show Fly(Rec.Seed ? Rec.Seed : 238);
		AirFeed::Apply(Rec.Board, Fly);
		size_t FlyTaken = 0;
		auto FlyTake = [&]()
		{
			const AirFeed::Frame& F = Rec.Views[FlyTaken++];
			Fly.Take(F.Head.T, F.Groups, F.Events, F.Ships);
		};
		FlyTake();
		if (FlyTaken < Rec.Views.size()) FlyTake();
		double FlyT = Rec.Views.front().Head.T;
		Fly.Advance(FlyT);
		bFlyBegun = Flyover.Begin(Fly.Groups(), Rec.Board.TargetX, Rec.Board.TargetY, bFlyHarbour);
		const double Step = 1.0 / 30.0;
		const double FlyPictures[3] = { 3.0, AirDesk::Flyover::RaidLong + 2.5, AirDesk::Flyover::RaidLong + AirDesk::Flyover::TargetLong + 2.0 };
		int FlyNextPicture = 0;
		AirShow::Vec LastEye;
		int LastIndex = -2;
		for (double RT = 0.0; bFlyBegun && RT <= Flyover.Length() + 0.5; RT += Step)
		{
			FlyT += Step / std::max(Rec.Board.Scale, 0.1);
			while (FlyTaken < Rec.Views.size() && Rec.Views[FlyTaken - 1].Head.T <= FlyT + 1e-9) FlyTake();
			Fly.Advance(FlyT);
			const AirDesk::Flyover::Shot Shot = Flyover.At(RT, Fly.Groups(), Fly.Planes());
			const AirShow::Vec Way = Shot.Look - Shot.Eye;
			if (Way.Length() < 50.0) ++FlyNoLook;
			if (Shot.Eye.Z < 60.0) ++FlyLow;
			if (Shot.Index >= 0) FlyShots.insert(Shot.Index);
			// (within a shot the eye glides: no more than 250 m a second, the formation's speed and the dolly's)
			if (LastIndex == Shot.Index && (Shot.Eye - LastEye).Length() > 250.0 * Step) ++FlyJumps;
			// (the picture as the scene takes it: the look's own right and up, the lens across the picture's width)
			const AirShow::Vec Fw = Way.Unit();
			const AirShow::Vec Rt = AirShow::Vec(Fw.Y, -Fw.X, 0.0).Unit();
			const AirShow::Vec Hi(Rt.Y * Fw.Z - Rt.Z * Fw.Y, Rt.Z * Fw.X - Rt.X * Fw.Z, Rt.X * Fw.Y - Rt.Y * Fw.X);
			auto Row = [&](const AirShow::Vec& P, double& OutX, double& OutY)
			{
				const AirShow::Vec D = P - Shot.Eye;
				const double Z = D.Dot(Fw);
				OutX = 1440.5 + FlyFocal * D.Dot(Rt) / std::max(Z, 1e-3);
				OutY = 893.0 - FlyFocal * D.Dot(Hi) / std::max(Z, 1e-3);
				return Z;
			};
			const bool bPicture = FlyNextPicture < 3 && RT + 1e-9 >= FlyPictures[FlyNextPicture];
			const int Picture = bPicture ? FlyNextPicture++ : -1;
			const int Close = Shot.Index == 0 ? Flyover.RaidGroup() : Shot.Index == 2 ? Flyover.GuardGroup() : -1;
			const double ShotBegan = Shot.Index == 0 ? 0.0 : AirDesk::Flyover::RaidLong + AirDesk::Flyover::TargetLong;
			int InFrame = 0, Shown = 0;
			std::vector<double> Spans;
			for (const AirShow::PlaneOut& P : Fly.Planes())
			{
				if (!P.bShown) continue;
				const AirShow::Vec To = P.At - Shot.Eye;
				if (To.Length() < 40.0) ++FlyIntoPlane;
				if (P.Group == Close && Close >= 0)
				{
					if (!P.bFalling) ++Shown;
					double X = 0.0, Y = 0.0;
					const double Z = Row(P.At, X, Y);
					if (Z > 1.0 && X >= 0.0 && X < 2881.0 && Y >= FlyTop && Y < FlyFoot)
					{
						++InFrame;
						Spans.push_back(P.Span * FlyFocal / Z);
					}
				}
			}
			if (Close >= 0 && RT > ShotBegan + 1.0) FlyLeastInFrame = std::min(FlyLeastInFrame, InFrame);
			if (Close >= 0 && (Picture == 0 || Picture == 2))
			{
				double& Least = Picture == 0 ? FlyTrailLeast : FlyLeadLeast;
				double& Most = Picture == 0 ? FlyTrailMost : FlyLeadMost;
				std::sort(Spans.rbegin(), Spans.rend());
				for (size_t K = 0; K < Spans.size() && K < 3; ++K)
				{
					Least = std::min(Least, Spans[K]);
					Most = std::max(Most, Spans[K]);
				}
				if (Spans.size() < 3) Least = 0.0;
				if (Picture == 0)
				{
					FlyRaidShown = Shown;
					FlyRaidInFrame = InFrame;
				}
				++FlyLeadPictures;
			}
			if (Picture == 1 && bFlyHarbour)
			{
				double X = 0.0;
				Row(AirShow::Vec(Rec.Board.TargetX, Rec.Board.TargetY, 0.0), X, FlyHarbourRow);
			}
			LastEye = Shot.Eye;
			LastIndex = Shot.Index;
		}
	}
	std::printf("  flyover: raid group %d, guard group %d, %.1f s of shots, %zu shots\n", Flyover.RaidGroup(), Flyover.GuardGroup(), Flyover.Length(), FlyShots.size());

	std::vector<Limit> Limits = {
		{ "no sliding sideways (degrees between nose and path)", C.MostSlip <= 1.0, Num(C.MostSlip, 2), "at most 1" },
		{ "no turn tighter than 210 m", C.LeastRadius >= 205.0, Num(C.LeastRadius), "at least 205" },
		{ "no more than 6 g", C.MostG <= 6.05, Num(C.MostG, 2), "at most 6.05" },
		{ "roll no faster than 160 degrees a second", C.MostRoll <= 161.0, Num(C.MostRoll), "at most 161" },
		{ "fighters in a fight never hover", C.LeastFightSpeed >= 80.0 || C.LeastFightSpeed > 1e8, Num(C.LeastFightSpeed > 1e8 ? 0.0 : C.LeastFightSpeed), "at least 80 m/s (about 290 km/h)" },
		{ "fighters in a fight not too fast", C.MostFightSpeed <= 201.0, Num(C.MostFightSpeed), "at most 200 m/s (a diving pass)" },
		{ "no aircraft pops from place to place", C.MostJump <= 25.0, Num(C.MostJump), "at most 25 m beyond its speed in a frame" },
		{ "never two gunners at one window", C.SharedWindows == 0, std::to_string(C.SharedWindows), "0" },
		[&]() -> Limit
		{
			// Every gunner's window that has a fighter in its arc for an air second or more fires at least once (the
			// sound director's measure: a B-17's tail twin must own the tail window, the radio gun never taking it).
			bool bOk = true;
			std::string Text;
			for (const auto& [Window, Sees] : C.GunnerSees)
			{
				const auto F = C.GunnerFires.find(Window);
				const long Fires = F == C.GunnerFires.end() ? 0 : F->second;
				if (Sees >= 30 && Fires == 0) bOk = false;
				Text += Window + " " + std::to_string(Fires) + " bursts (" + Num(Sees / 30.0, 0) + " s in its arc); ";
			}
			return { "every gunner's window with an attacker in its arc fires", bOk, Text.empty() ? std::string("no gunner had a fighter in his arc") : Text,
			         "at least one burst from each window that had a fighter in its arc for an air second" };
		}(),
		{ "a model falls for every aircraft lost", C.Falls >= EventsLost - 2 && C.Falls <= EventsLost + 2 && C.LostUnshown == EventsNoModel,
		  std::to_string(C.Falls) + " falls for " + std::to_string(EventsLosses) + " losses, the battle's models " + std::to_string(EventsLost) + "; " + std::to_string(C.LostUnshown) +
		  " told with no model falling (the battle's " + std::to_string(EventsNoModel) + "), " + std::to_string(C.StragUnshown) + " stragglers not shown",
		  "the battle's models, within 2 (a model falls only for its share of the losses: AR1)" },
		{ "the kit told of every fall, its moves only while it falls", FallStarts == C.Falls && FallStrays == 0, std::to_string(FallStarts) + " starts, " + std::to_string(FallStrays) + " stray", "a start a fall, 0 stray" },
		{ "debris within its room", MostDebris <= Debris.Most, std::to_string(MostDebris), "at most the room" },
		{ "every burning aircraft keeps its trail (room 24)", Trails24.RefusedOwn == 0 && Trails24.Peak <= 24, std::to_string(Trails24.RefusedOwn), "0 refused" },
		{ "every burning aircraft keeps its trail (room 60, the kit's far trails on)", Trails60.RefusedOwn == 0 && Trails60.Peak <= 60, std::to_string(Trails60.RefusedOwn) + " (most lit " + std::to_string(Trails60.Peak) + ")", "0 refused" },
		{ "no group left on the stand-in model", Guessed == 0, std::to_string(Guessed), "0 views" },
		{ "parachutes as the battle says", C.Chutes >= EventsChutes * 8 / 10, std::to_string(C.Chutes), "at least 8 in 10 of the battle's" },
		{ "every way down played as the battle names it", [&]() { int Off = 0; for (const auto& [How, N] : EventsHow) { const auto It = C.FallsBy.find(How); Off += std::abs(N - (It == C.FallsBy.end() ? 0 : It->second)); } return Off <= 2; }(),
		  [&]() { std::string Text; for (const auto& [How, N] : C.FallsBy) Text += How + " " + std::to_string(N) + " "; return Text; }(), "the battle's, within 2" },
		{ "falls strike when the battle says", C.FallsOffTime <= std::max(1, (C.FallsOnTime + C.FallsOffTime) / 20), std::to_string(C.FallsOnTime) + " on time, " + std::to_string(C.FallsOffTime) + " off, worst " + Num(C.MostOffTime, 1) + " s; " +
		  std::to_string(C.FallsLow) + " begun too low for the battle's time, in as slowly as an aeroplane comes down; " + std::to_string(C.FallsTrue) + " drawn from over the battle's band, as long as an aeroplane takes",
		  "within a quarter of dur, 1 in 20 may miss (a fall begun under LowFallM, 400 m, or too low to last the battle's time at 8 m/s, strikes as soon as it can: the audit's 4.5; one drawn higher takes as long as it must, never steeper: 4.12)" },
		{ "parachutes open when the battle says", C.ChutesOffTime == 0, std::to_string(C.ChutesOffTime), "0 late" },
		{ "hits survived shown", C.HitsShown >= EventsHits * 9 / 10, std::to_string(C.HitsShown) + " of " + std::to_string(EventsHits), "9 in 10" },
		{ "fires put out", C.FiresOut >= EventsFiresOut * 7 / 10, std::to_string(C.FiresOut) + " of " + std::to_string(EventsFiresOut), "7 in 10" },
		{ "the box's streams make a web, not one point", WebOnePoint * 20 <= WebPairs, std::to_string(WebOnePoint) + " of " + std::to_string(WebPairs) + " pairs on one spot", "at most 1 in 20" },
		{ "a ride on the bombers' attackers shows a bomber", RideFar == 0 && RideOnBomber * 4 >= RideSamples * 3,
		  std::to_string(RideOnBomber) + " of " + std::to_string(RideSamples) + " s on a bomber (" + std::to_string(RideHeadOn) + " head-on), " + std::to_string(RideFar) + " too far", "3 in 4, none over 2.5 km" },
		{ "the chase view steps through every aircraft and squadron, round and back", ChaseBad == 0 && ChaseLines > 0,
		  std::to_string(ChaseLines) + " lines, " + std::to_string(ChaseAircraft) + " aircraft, " + std::to_string(ChaseFalls) + " falls, " + std::to_string(ChaseBad) + " wrong" + (ChaseBad ? " (" + ChaseFirstBad + ")" : std::string()),
		  "0 wrong" },
		{ "a gunner's gun swings no faster than he lays it", Swing99 <= 55.0, std::to_string(static_cast<int>(Swing99 + 0.5)) + " deg/s (99 in 100)", "at most 55 (one gun, one stream)" },
		{ "bombers back in formation", C.Regained >= EventsRegain * 7 / 10, std::to_string(C.Regained) + " of " + std::to_string(EventsRegain), "7 in 10" },
		{ "every dead engine smokes while it flies", DeadDark.empty() && DeadCleared == 0,
		  std::to_string(DeadSeen.size()) + " with an engine out, " + std::to_string(DeadDark.size()) + " without smoke, " + std::to_string(DeadCleared) + " cleared", "0 and 0" },
		{ "guns within the effects budget", MostLiveGuns <= Show.MostGuns, std::to_string(MostLiveGuns), "at most MostGuns" },
		{ "no gun fires from a falling or hidden aircraft", GunsFromFalling == 0 && GunsFromHidden == 0, std::to_string(GunsFromFalling) + " falling, " + std::to_string(GunsFromHidden) + " hidden", "0" },
		{ "every gun's key names its own aircraft", GunsMisKeyed == 0, std::to_string(GunsMisKeyed), "0" },
		{ "no rounds strike a falling aircraft", HitsOnFalling == 0 && FiringAtFalling == 0, std::to_string(HitsOnFalling) + " hits, " + std::to_string(FiringAtFalling) + " frames firing", "0" },
		{ "flak bursts round the boxes", C.FlakBursts > 0, std::to_string(C.FlakBursts), "some" },
		// The ships' guns (ShipGuns): with ships under attack, streams from her own decks at raiders inside each gun's
		// reach, most of them missing (the historian's A2: hundreds of rounds a kill), a few rounds seen striking home;
		// the heavy guns' bursts; none held on a plane her own fighters are after. With no ships, nothing.
		[&]() -> Limit
		{
			const ShipGuns::Tally& T_ = Show.ShipTally();
			const bool bShips = !ShipsSeen.empty();
			const double OnShare = T_.StreamFrames > 0 ? static_cast<double>(T_.StreamOn) / static_cast<double>(T_.StreamFrames) : 0.0;
			const bool bOk = bShips ? ShipStreams > 0 && ShipStreamsOff == 0 && T_.OutOfReach == 0 && OnShare <= 0.2 && T_.Hits > 0 && T_.Rounds > 0 && T_.Bursts > 0
			                        : ShipStreams == 0 && T_.Rounds == 0 && T_.Hits == 0;
			std::string Got = bShips
				? std::to_string(T_.StreamFrames) + " stream frames (" + std::to_string(ShipStreamsOff) + " off their ship), on the plane " + Num(OnShare * 100.0, 1) + "%; " +
				  std::to_string(T_.Hits) + " hits seen; " + std::to_string(T_.Rounds) + " heavy rounds, " + std::to_string(T_.Bursts) + " bursts, " + std::to_string(T_.Splashes) + " splashes; flak losses under the light guns " +
				  std::to_string(T_.Kills) + ", the heavy " + std::to_string(T_.KillsHeavy) + "; " + std::to_string(T_.OutOfReach) + " out of reach; most streams " + std::to_string(T_.MostStreams)
				: std::string("no ships: ") + std::to_string(ShipStreams) + " streams, " + std::to_string(T_.Rounds) + " rounds";
			return Limit{ "the ships fire back: in reach, from their decks, mostly missing", bOk, Got, "streams on their ships, in reach, at most 1 frame in 5 on the plane, some hits, some bursts (none with no ships)" };
		}(),
		// The bomb bays (Brian, 21 November 05:44Z): the doors open before each level bomber's stick leaves, held while it
		// falls, then shut; a usual war load each (B-17 12, Lancaster 14, He 111 8...), heard where it lands.
		{ "every stick through open doors, the doors shut after", C.BombsThroughShutDoors == 0 && C.BaysLeftOpen == 0 && (StickHits == 0 || !BayMost.empty()),
		  std::to_string(C.Sticks) + " sticks, " + std::to_string(C.StickBombs) + " bombs; " + std::to_string(BayMost.size()) + " bays seen open; " +
		  std::to_string(C.BombsThroughShutDoors) + " bombs through shut doors, " + std::to_string(C.BaysLeftOpen) + " bays left open",
		  "0 and 0 (bays seen open where a stick fell)" },
		// The scene's ask (core-r2-8): a fall's crash or splash carries the aircraft's velocity as it struck, for the
		// fireball to keep a little of.
		{ "a crash or a splash carries the aircraft's velocity as it struck", EndsVelWrong == 0 && (EndsSeen > 0 || FallStarts == 0),
		  std::to_string(EndsSeen) + " ends, " + std::to_string(EndsVelWrong) + " not its last FallMove's velocity there" +
		  (EndsSeen > 0 ? "; " + Num(EndSpeedLeast) + " to " + Num(EndSpeedMost) + " m/s" : std::string()),
		  "0 (each the velocity of its aircraft's last FallMove, at the same place)" },
		{ "a stick let go wings level, a bomber's turns gentle", StickBankMost <= 3.0 && BayBomberBankMost <= 30.5,
		  BayMost.empty() ? std::string("no bays") : "most bank " + Num(StickBankMost, 1) + " deg from her doors open to her stick gone, " + Num(BayBomberBankMost, 1) + " ever",
		  "at most 3 on the run, 30 ever" },
		// Drop on the lead and the rally (Brian, 22 November 04:11Z: "the lead plane always drops the bombs first and the
		// rest follow"; "after the bombers drop their bombs they close the bomb bay doors and start turning back home").
		{ "the lead's stick first, the rest on his", C.DropsBeforeLead == 0 && (C.DropsOnLead == 0 || (C.OnLeadLeast >= 0.59 && C.OnLeadMost <= 1.81)),
		  std::to_string(C.DropsOnLead) + " sticks on their lead's, " + (C.DropsOnLead ? Num(C.OnLeadLeast, 2) + " to " + Num(C.OnLeadMost, 2) + " air s after his first bomb" : std::string("none")) +
		  "; " + std::to_string(C.DropsBeforeLead) + " before his",
		  "none before his; each 0.6 to 1.8 air s after his first bomb" },
		// (the same, seen from outside the core: in each level box one first bomb leaves before all the others, and each
		// other's 0.6 to 1.8 air s after it, to a frame)
		[&]() {
			std::map<int, std::vector<double>> Firsts;
			for (const auto& [Key, Box] : StickBox) Firsts[Box].push_back(StickAt[Key]);
			int Boxes = 0, Off = 0, Tied = 0;
			double Least = 1e9, Most = 0.0;
			for (auto& [Box, Ts] : Firsts)
			{
				if (Ts.size() < 2) continue;
				++Boxes;
				std::sort(Ts.begin(), Ts.end());
				Tied += Ts[1] - Ts[0] < 0.02 ? 1 : 0;
				for (size_t K = 1; K < Ts.size(); ++K)
				{
					const double After = Ts[K] - Ts[0];
					Least = std::min(Least, After);
					Most = std::max(Most, After);
					Off += After < 0.6 - 0.05 || After > 1.8 + 0.05 ? 1 : 0;
				}
			}
			return Limit{ "in each box one first bomb leads, the others 0.6 to 1.8 air s on", Off == 0 && Tied == 0,
				Boxes ? std::to_string(Boxes) + " boxes; the others " + Num(Least, 2) + " to " + Num(Most, 2) + " air s after their lead's; " + std::to_string(Off) + " outside, " + std::to_string(Tied) + " boxes with two first" : std::string("no level boxes"),
				"the lead's alone first; each other 0.6 to 1.8 air s after (to a frame, 0.05)" };
		}(),
		{ "each bay's doors shut within BayShutS of her last bomb", BayShutLate.empty(),
		  std::to_string(BayShutAt.size()) + " shut, the last " + Num(BayShutMost, 2) + " air s after her last bomb; " + std::to_string(BayShutLate.size()) + " still open past it",
		  "shut within BayShutS (2.5 air s) and a frame, none open past it" },
		[&]() {
			bool bTight = true;
			std::string Boxes;
			for (const auto& [Box, At] : BoxSpreadAt)
			{
				const double Most = BoxSpreadMost[Box];
				bTight = bTight && Most <= At * 1.5 + 30.0;
				Boxes += (Boxes.empty() ? "" : ", ") + std::string("box ") + std::to_string(Box) + " " + Num(At, 0) + " m at its first bomb, at most " + Num(Most, 0);
			}
			const bool bOk = (C.Rallies > 0 || BoxSpreadAt.empty()) && C.RallyAgainst == 0 && C.RallyBankSeen <= 25.0 && bTight && C.RallyMadeFar <= 1500.0;
			return Limit{ "each box turns for home on its lead, one way round, together", bOk,
				std::to_string(C.Rallies) + " rallies; " + std::to_string(C.RallyAgainst) + " of " + std::to_string(C.RallySteps) + " steps banked against the lead's way; most bank " + Num(C.RallyBankSeen, 1) +
				" deg; " + (Boxes.empty() ? std::string("no level boxes") : Boxes) + "; models made on the lead at most " + Num(C.RallyMadeFar, 0) + " m from him",
				"every wing his way round, at most 25 deg, each box's middle spread at most 1.5 times (+30 m) its spread at its first bomb until it has rallied, a model made meanwhile within 1.5 km of its lead" };
		}(),
		{ "the strategy camera on aircraft", FocusSamples > 0 && FocusThin * 20 <= FocusSamples,
		  std::to_string(FocusThin) + " of " + std::to_string(FocusSamples) + " seconds' focus with under 3 aircraft within 1.2 km (the middle of the groups, as before: " + std::to_string(FocusOldEmpty) + " with none)",
		  "under 5% (the Mac's release 2 check: approach, Oahu 02 and 04, Midway 04 and 07 framed on empty sky)" },
		[&]() -> Limit
		{
			const bool bShips = !ShipsSeen.empty();
			const double Start = Rec.Views.front().Head.T;
			const auto Name = !ShipsSeen.count(GunsShip) ? std::string("?") : !ShipsSeen[GunsShip].Name.empty() ? ShipsSeen[GunsShip].Name : "a " + ShipsSeen[GunsShip].Class;
			const bool bOk = bShips ? GunsT >= 0.0 && GunsFar25 >= 0.0 && GunsFar25 <= 2400.0 : GunsT < 0.0;
			std::string Got = GunsT < 0.0 ? std::string("no moment") :
				Name + " at " + std::to_string(static_cast<int>(GunsT - Start) / 60) + ":" + (static_cast<int>(GunsT - Start) % 60 < 10 ? "0" : "") + std::to_string(static_cast<int>(GunsT - Start) % 60) +
				" of the battle, her guns on a " + GunsModel + " " + Num(GunsRange, 0) + " m off and " + Num(GunsUp, 0) + " m up; 2.5 s on " + Num(GunsFar25, 0) + " m from her, 10 s on " + Num(GunsFar10, 0);
			return Limit{ "the guns fire back picture has its moment", bOk, Got, "with ships, an attack run found and he within 2.4 km of her 2.5 s on (none with no ships)" };
		}(),
		{ "one gun to a key", GunKeysTwice == 0, std::to_string(GunKeysTwice) + " frames' guns firing from two muzzles under one key", "0 (two muzzles under one key: one tracer line between them)" },
		{ "each stick out of her bay, between its doors", BayBombsOff == 0 && (Show.BaysOf.empty() || StickAt.empty() || BayBombsSeen > 0),
		  Show.BaysOf.empty() ? std::string("no bays in the models flown") : std::to_string(BayBombsSeen) + " first bombs seen, " + std::to_string(BayBombsOff) + " outside the racked load; " +
		  [&]() { std::string Out; for (const auto& [Model, Bay] : Show.BaysOf) Out += Model + "'s bay " + Num(Bay.Aft, 2) + " to " + Num(Bay.Fore, 2) + " m ahead of her middle, " + Num(Bay.Up, 2) + " up; "; return Out; }() +
		  (BayBombAhead.empty() ? std::string() : "first bombs " + Num(*std::min_element(BayBombAhead.begin(), BayBombAhead.end()), 2) + " to " + Num(*std::max_element(BayBombAhead.begin(), BayBombAhead.end()), 2) + " m ahead"),
		  "every first bomb inside her racked load (the Mac film of release 2: they fell from solid belly 3 m aft)" },
		{ "the doors open on the run, before the stick", [&]() { for (const auto& [Key, At] : StickAt) { const auto Open = BayOpenAt.find(Key); if (BayMost.count(Key) && (Open == BayOpenAt.end() || At - Open->second < 0.0)) return false; } return true; }(),
		  [&]() { double Least = 1e9, Most = -1e9; for (const auto& [Key, At] : StickAt) { const auto Open = BayOpenAt.find(Key); if (Open == BayOpenAt.end()) continue; Least = std::min(Least, At - Open->second); Most = std::max(Most, At - Open->second); }
		          return Least > 1e8 ? std::string("no bays") : "open " + Num(Least, 1) + " to " + Num(Most, 1) + " air s before the first bomb"; }(),
		  "open before the first bomb leaves" },
		{ "each stick heard where it lands, not when let go", Sounds.count("bomb_stick_01") == 0 && StickHits + OneHits <= C.Sticks && (C.Bombs == 0 || StickHits + OneHits > 0),
		  std::to_string(StickHits) + " sticks and " + std::to_string(OneHits) + " single bombs heard landing; " + std::to_string(C.Rushes) + " rushes near the eye; " +
		  std::to_string(Sounds.count("bomb_stick_01") ? Sounds["bomb_stick_01"] : 0) + " stick sounds at the drop",
		  "a landing heard for each stick, none at the drop" },
		{ "dive bombers and torpedo planes leave low, never climbing back", C.LeftLowMost <= 500.0, Num(C.LeftLowMost) + " m the highest one flew after her dive or her drop",
		  "at most 500 m over the ground or the sea, under her bomb's 600 (the design director's ruling: they leave low, never climb back)" },
		[&]() -> Limit
		{
			// The escorts at their stations (the raids answer's Part 3.4): a close escort 600 to 1,200 m over its bombers,
			// over both flanks, legs of 25 to 30 s; top cover 1,500 to 3,000 m over, legs of 40 to 60 s (the middles).
			auto Middle = [](std::vector<double> V) { if (V.empty()) return 0.0; std::sort(V.begin(), V.end()); return V[V.size() / 2]; };
			bool bOk = true;
			std::string Text;
			for (const auto& [Role, Overs] : EscortOver)
			{
				const double Over = Middle(Overs), Leg = Middle(EscortLegs[Role]);
				const long Left = EscortFlank[Role][0], Right = EscortFlank[Role][1];
				const bool bTop = Role == "top";
				const double Lo = bTop ? 1500.0 : 600.0, Hi = bTop ? 3000.0 : 1200.0, LegLo = bTop ? 40.0 : 25.0, LegHi = bTop ? 60.0 : 30.0;
				const bool bBoth = bTop || (Left * 4 >= Left + Right && Right * 4 >= Left + Right);
				bOk = bOk && Over >= Lo && Over <= Hi && bBoth && (EscortLegs[Role].empty() || (Leg >= LegLo && Leg <= LegHi));
				Text += Role + ": " + Num(Over, 0) + " m over its bombers, " + std::to_string(Left) + " samples on the left and " + std::to_string(Right) + " on the right, legs " + Num(Leg, 1) +
				        " air s (" + std::to_string(EscortLegs[Role].size()) + "); ";
			}
			return { "escorts fly their stations over their bombers", bOk, Text.empty() ? std::string("no close escort or top cover") : Text,
			         "the middles: close 600 to 1,200 m over, a quarter at least on each flank, legs 25 to 30 s; top 1,500 to 3,000 m, legs 40 to 60 s" };
		}(),
		{ "a fall leans and rolls as the eye can follow", FallBankFast == 0,
		  std::to_string(FallBankSteps) + " frames of falls; the bank turned at most " + Num(FallBankMost) + " degrees an air second; " + std::to_string(FallBankFast) + " frames faster than 600",
		  "never over 10 degrees in a sixtieth of an air second (600 a second; the audit's 4.3)" },
		{ "vapour trails only over the day's trail height", VapourLow == 0, "the day's height " + Num(Show.TrailM) + " m; " + std::to_string(VapourOn) + " aircraft-frames trailing, " + std::to_string(VapourLow) + " under it; most " + Num(VapourMost, 2),
		  "none under it" },
		{ "the strike is on while bombs fall and dives go down", StrikeMiss == 0 && StrikeLateMost <= 5.0 && (StrikeNeed == 0 || StrikeEnds > 0),
		  std::to_string(StrikeNeed) + " frames with a bomb or torpedo in the air or a bomber diving, " + std::to_string(StrikeMiss) + " of them off; on " + std::to_string(StrikeOnFrames) +
		  " frames, off " + std::to_string(StrikeEnds) + " times, at most " + Num(StrikeLateMost, 2) + " air s after the last had struck",
		  "on in every such frame; off within 5 air s after the last bomb, torpedo or dive (or hit on a ship or a column shown)" },
		{ "fighters pass the bombers", C.Passes > 0, std::to_string(C.Passes), "some" },
		// The fighter pilots (the pilots' section of AirShow.cpp).
		{ "the first fighter guns soon after a fight opens", C.FallsByFighters == 0 || (SoonestGuns >= 0.0 && SoonestGuns <= 40.0), Num(SoonestGuns, 1), "within 40 air s (where the battle has fighters bring anything down)" },
		{ "every fighter in a fight has a task the cameras know", PilotOddTask == 0 && PilotNoTask * 100 <= PilotSamples,
		  std::to_string(PilotNoTask) + " of " + std::to_string(PilotSamples) + " with none, " + std::to_string(PilotOddTask) + " with a word nobody knows", "at most 1 in 100 with none, 0 unknown" },
		{ "wingmen hold their leaders", C.WingSamples == 0 || (C.WingSum / C.WingSamples <= 400.0 && C.WingFar * 4 <= C.WingSamples),
		  Num(C.WingSamples ? C.WingSum / C.WingSamples : 0.0) + " m on average, " + std::to_string(C.WingFar) + " of " + std::to_string(C.WingSamples) + " steps over 500 m", "at most 400 m, 1 step in 4 over 500 m" },
		{ "a wingman's leader is one of his own, still flying", WingmenAdrift * 50 <= WingmenSeen, std::to_string(WingmenAdrift) + " of " + std::to_string(WingmenSeen), "at most 1 in 50 (a moment after his leader falls)" },
		{ "aircraft lost to fighters fall to guns that bear", C.FallsByFighters < 6 || C.FallsUnderFire * 6 >= C.FallsByFighters,
		  std::to_string(C.FallsUnderFire) + " of " + std::to_string(C.FallsByFighters), "at least 1 in 6 under those fighters' guns" },
		{ "a pilot with an enemy on his tail does something about it", C.Threats < 5 || C.Evasions > 0, std::to_string(C.Evasions) + " evasions for " + std::to_string(C.Threats) + " threats seen", "some" },
		{ "a scissors is won only after its crossings, and never drags on", C.ScissorTurns >= 2 * C.ScissorWins && C.LongestScissors <= 24.0, std::to_string(C.Scissors) + " scissors, " + std::to_string(C.ScissorTurns) + " rolls back, " + std::to_string(C.ScissorWins) + " won, longest " + Num(C.LongestScissors, 1) + " s", "two rolls back or more for each one won, none over 24 air s" },
		{ "an escort in a fight goes after the attackers", !bEscortFought || C.FallsByFighters == 0 || C.GuardChases + C.Covers > 0, std::to_string(C.GuardChases) + " chases off its box, " + std::to_string(C.Covers) + " off a partner's tail", "some" },
		{ "no fighter fights into the ground", C.LowestFight >= 60.0 && C.LowestChase >= 10.0, Num(C.LowestFight > 1e8 ? 0.0 : C.LowestFight) + " m; after torpedo planes over the sea " + (C.LowestChase > 1e8 ? std::string("none") : Num(C.LowestChase) + " m"),
		  "at least 60 m over it (a strafing run apart); after a torpedo plane at wave-top over the sea, at least 10 m" },
		{ "torpedoes go at wave-top, at the type's own speed", [&]() {
			  for (size_t K = 0; K < C.TorpedoDropAt.size(); ++K)
			  {
				  const std::string& M = C.TorpedoDropModel[K];
				  const double Lo = M == "TBD" ? 45.0 : M == "B5N" ? 51.0 : 38.0, Hi = M == "TBD" ? 57.0 : M == "B5N" ? 82.0 : 85.0;
				  if (C.TorpedoDropAt[K] < 10.0 || C.TorpedoDropAt[K] > 25.0 || C.TorpedoDropSpeed[K] < Lo || C.TorpedoDropSpeed[K] > Hi) return false;
			  }
			  return C.TorpedoLowest >= 5.0; }(),
		  [&]() {
			  if (C.TorpedoDropAt.empty()) return std::string("no torpedoes");
			  const auto [AtLo, AtHi] = std::minmax_element(C.TorpedoDropAt.begin(), C.TorpedoDropAt.end());
			  const auto [VLo, VHi] = std::minmax_element(C.TorpedoDropSpeed.begin(), C.TorpedoDropSpeed.end());
			  return std::to_string(C.TorpedoDropAt.size()) + " torpedoes (" + C.TorpedoDropModel.front() + ") let go " + Num(*AtLo, 1) + " to " + Num(*AtHi, 1) + " m over the sea at " + Num(*VLo, 1) + " to " + Num(*VHi, 1) +
			         " m/s; the lowest a torpedo plane flew " + Num(C.TorpedoLowest, 1) + " m"; }(),
		  "10 to 25 m; the Devastator 45 to 57 m/s, the Kate 51 to 82; none under 5 m (never touching the water)" },
		{ "a frame fits the budget", TotalMs / std::max(1L, Frames) < 2.0, Num(TotalMs / std::max(1L, Frames), 3), "under 2 ms on average" },
	};
	// The gunnery (AirShow.cpp, "The gunnery"; GunWatch and FlyForGuns, above). Brian, 21 November: "make sure the gun
	// fire from the planes actually hit the target and do damage when its a hit"; "i can see a gunner on a bomber and
	// its tracers hit a fighter and then it goes down". The battle's count decides every loss and every hit; these hold
	// the scene to showing them so that the eye can follow, over this play and the five flown again with other dice:
	// nothing is struck that no round was seen to strike, no round is drawn through an aircraft that flies on, every
	// round that ends on an aircraft draws its strike there, and a burst that strikes is one the battle counted.
	{
		auto Seen = [&GunPlays](long GunWatch::* Field)
		{
			long All = 0;
			for (const GunPlay& Each : GunPlays) All += Each.Watch.*Field;
			return All;
		};
		auto PlaysWith = [&GunPlays](long GunWatch::* Field)
		{
			int Count = 0;
			for (const GunPlay& Each : GunPlays) Count += Each.Watch.*Field > 0 ? 1 : 0;
			return std::to_string(Count) + " of " + std::to_string(GunPlays.size()) + " plays";
		};
		auto Of = [](long Part, long Whole) { return std::to_string(Part) + " of " + std::to_string(Whole); };
		const long PlaysFlown = static_cast<long>(GunPlays.size());
		Limits.push_back({ "every strike on an aircraft has a round ending there", Seen(&GunWatch::StrikesNoRound) == 0,
		                   Of(Seen(&GunWatch::StrikesNoRound), Seen(&GunWatch::Strikes)) + " strikes with none, in " + PlaysWith(&GunWatch::StrikesNoRound) + "; " +
		                       std::to_string(Seen(&GunWatch::Splinters)) + " not gunfire, counted apart; " + std::to_string(Seen(&GunWatch::StrikesNoBurst)) + " that name no burst",
		                   "0 (a round of a drawn stream ended on that aircraft within a fifth of a second of the strike)" });
		Limits.push_back({ "no round is drawn through an aircraft that flies on", Seen(&GunWatch::Through) == 0,
		                   std::to_string(Seen(&GunWatch::Through)) + " (" + std::to_string(Seen(&GunWatch::ThroughEnded)) + " ended farther on, " + std::to_string(Seen(&GunWatch::ThroughFree)) + " flew out their life), in " +
		                       PlaysWith(&GunWatch::Through) + "; " + std::to_string(Seen(&GunWatch::ThroughFalling)) + " more through one already going down",
		                   "0 (a stream through one aircraft counted once a quarter second)" });
		Limits.push_back({ "no round is drawn through its own aircraft", Seen(&GunWatch::ThroughOwn) == 0,
		                   std::to_string(Seen(&GunWatch::ThroughOwn)) + ", in " + PlaysWith(&GunWatch::ThroughOwn), "0 (past its first 2.5 m; a stream counted once a quarter second)" });
		Limits.push_back({ "every round that ends on an aircraft draws its strike", Seen(&GunWatch::EndsNoStrike) == 0,
		                   Of(Seen(&GunWatch::EndsNoStrike), Seen(&GunWatch::EndOnPlane)) + " rounds ended on one with no strike drawn, in " + PlaysWith(&GunWatch::EndsNoStrike),
		                   "0 (ended in its airframe or within 0.75 m of it; a strike on that aircraft within a fifth of a second)" });
		Limits.push_back({ "strikes lie on the airframe, at the place told", Seen(&GunWatch::OffBody) == 0 && Seen(&GunWatch::LocalOff) == 0 && Seen(&GunWatch::OddHits) == 0,
		                   std::to_string(Seen(&GunWatch::OffBody)) + " off the airframe, " + std::to_string(Seen(&GunWatch::LocalOff)) + " not where told, of " + std::to_string(Seen(&GunWatch::Strikes)) + " strikes; " +
		                       std::to_string(Seen(&GunWatch::OddHits)) + " on nothing (no aircraft named, nor the ground or the sea)",
		                   "0, 0 and 0 (within 0.45 m of its boxes; its place in the aircraft's own frame within 5 cm of where it is drawn)" });
		// (a Gun effect that tells an end tells its time and its reach: the two must be one place. An end told past the
		// round's life is counted beside it and not held here: the tracer is gone before it, which harms nothing where
		// the stream was only to stop short, and where it was to strike, the rows above and the ground's below catch it.)
		{
			double ReachOff = 0.0;
			for (const GunPlay& Each : GunPlays) ReachOff = std::max(ReachOff, Each.Watch.MostReachOff);
			Limits.push_back({ "a stream's reach told is where its rounds end", ReachOff <= 0.5,
			                   Num(ReachOff, 2) + " m off at most, over " + std::to_string(Seen(&GunWatch::GunFxEnding)) + " stream frames that told an end; " + std::to_string(Seen(&GunWatch::PastLife)) +
			                       " of them told one past their round's life",
			                   "Reach within 0.5 m of the rounds' place at Tof, by the kit's flight" });
		}
		// No more bursts strike than the battle counted. The core lays to guns no more than the recording gives them
		// (what it holds for a burst is the battle's, never its own); in no play do more bursts strike, or more Moments
		// run through to the damage, than it laid to guns (each such Moment is one event of the battle's); and no burst
		// draws strikes and then comes to nothing, the aircraft flying on with the strikes seen on it.
		{
			bool bWithin = true;
			long MostStruck = 0, Laid = 0;
			for (const GunPlay& Each : GunPlays)
			{
				const AirShow::Checks& Ck = Each.Check;
				const GunWatch& W = Each.Watch;
				bWithin = bWithin && Ck.ToGuns <= EventsGunLost && Ck.HitsToGuns <= EventsGunHits && W.Kills <= Ck.ToGuns && W.HitsDone <= Ck.HitsToGuns && W.BurstsStruck <= Ck.ToGuns + Ck.HitsToGuns;
				if (W.BurstsStruck >= MostStruck)
				{
					MostStruck = W.BurstsStruck;
					Laid = Ck.ToGuns + Ck.HitsToGuns;
				}
			}
			Limits.push_back({ "no more bursts strike than the battle counted", bWithin && Seen(&GunWatch::BurstsForNothing) == 0 && Seen(&GunWatch::BurstsUntold) == 0,
			                   std::to_string(MostStruck) + " bursts struck at most in a play, for " + std::to_string(Laid) + " losses and hits laid to guns (the recording gives " + std::to_string(EventsGunLost + EventsGunHits) +
			                       " at most); " + Of(Seen(&GunWatch::BurstsForNothing), Seen(&GunWatch::BurstsStruck)) + " struck and came to nothing, " + std::to_string(Seen(&GunWatch::BurstsUntold)) + " told no Moment",
			                   "never more than were laid to guns; 0 that strike and come to nothing; 0 untold" });
		}
		// Losses to guns are seen struck first: of the aircraft the battle lost to guns (the recording's own count, each
		// play), those whose Moment ran through to the fall with strikes of its burst drawn on that aircraft first,
		// rounds of the guns it names seen to end there, and the fall begun as it says.
		// THE LIMIT, SET AT THE MERGE: at least 1 in 4, over the six plays together. With the fighters' and the gunners'
		// parts in, the core shows 335 of 630 on the big Bremen raid, 142 of 174 on the small, 102 of 270 on Oahu and
		// 87 of 186 on Midway at this test's own dice. Sixteen other dice in the measuring tool (30 frames a second)
		// gave, in the worst play of each, 48 of 105, 17 of 29, 13 of 45 and 11 of 31: 1 in 4 is about the worst single
		// play of the two battles that show least, rounded down, and the six plays together stand well clear of it on
		// every recording. It is a floor under what the eye is shown, not a mark of what is wanted: a loss no gun came
		// to bear on in its time still shows with no strike, and those are the fighter pilots' to bring down.
		{
			long Lost = 0, CoreStruck = 0, CoreLate = 0, CoreUnseen = 0, Least = 1 << 30;
			for (const GunPlay& Each : GunPlays)
			{
				Lost += std::max(EventsGunLost, Each.Check.ToGuns);
				CoreStruck += Each.Check.ToGunsStruck;
				CoreLate += Each.Check.ToGunsLate;
				CoreUnseen += Each.Check.ToGunsUnseen;
				Least = std::min(Least, Each.Watch.KillsSeen);
			}
			const long SeenStruck = Seen(&GunWatch::KillsSeen);
			Limits.push_back({ "losses to guns are seen struck first", Lost == 0 || (SeenStruck * 4 >= Lost && Seen(&GunWatch::KillsNoFall) == 0),
			                   Of(SeenStruck, Lost) + " over " + std::to_string(PlaysFlown) + " plays (" + std::to_string(Least) + " in the worst); the core says " + std::to_string(CoreStruck) + " struck first, " +
			                       std::to_string(CoreLate) + " from a burst some seconds before, " + std::to_string(CoreUnseen) + " with no strike; " + std::to_string(Seen(&GunWatch::KillsNoFall)) + " told fallen with no fall begun",
			                   "at least 1 in 4 over the plays (the limit set at the merge), where the battle has any; a fall begun for each" });
			// And fighters lost to a box's gunners are seen struck by a gunner: the same, the rounds from the mount the
			// Moment names on a bomber of that box.
			// THE LIMIT, SET AT THE MERGE: at least 2 in 3, over the six plays together. With the gunners' part in, the
			// core shows 85 of the big Bremen raid's 96, and 12 of 16 in its worst play, at this test's own dice; sixteen
			// other dice in the measuring tool gave 13.8 of 16 on the mean and 12 in the worst play. Only the big Bremen
			// raid has the battle lose fighters to gunners; the others have none to show.
			long GunnersLost = 0;
			for (const GunPlay& Each : GunPlays) GunnersLost += std::max(EventsGunnerLost, Each.Check.GunnerKills);
			Limits.push_back({ "fighters lost to gunners are seen struck by a gunner", GunnersLost == 0 || Seen(&GunWatch::GunnerKillsSeen) * 3 >= GunnersLost * 2,
			                   GunnersLost == 0 ? std::string("the battle has none") : Of(Seen(&GunWatch::GunnerKillsSeen), GunnersLost) + " over " + std::to_string(PlaysFlown) + " plays, in " + PlaysWith(&GunWatch::GunnerKillsSeen),
			                   "at least 2 in 3 over the plays (the limit set at the merge), where the battle has any" });
		}
		// A gunner's kill is told to the cameras before it strikes (Show::Moment, "gunner-kill"): told as he opens, a
		// second or more before his rounds strike, so a shot can be on his window as he fires; his window named, the
		// mount his own bomber's; and its times in their order (he opens, his rounds strike as the strikes are drawn,
		// the fall begins).
		{
			double Lead = 1e9;
			for (const GunPlay& Each : GunPlays) Lead = std::min(Lead, Each.Watch.LeastLead);
			Limits.push_back({ "a gunner's kill is told to the cameras before it strikes", Seen(&GunWatch::GunnerKillsSoon) == 0 && Seen(&GunWatch::GunnerKillsOdd) == 0,
			                   std::to_string(Seen(&GunWatch::GunnerKillsTold)) + " told: " + std::to_string(Seen(&GunWatch::GunnerKillsSoon)) + " struck under a second after he opened" +
			                       (Lead < 1e8 ? " (the soonest " + Num(Lead, 2) + " s)" : std::string()) + ", " + std::to_string(Seen(&GunWatch::GunnerKillsOdd)) + " with no window, the wrong mount or its times out of order",
			                   "0 and 0 (OpenAt a second or more before the first strike drawn)" });
		}
		// A stream that comes down on the ground or the sea strikes it. In the recordings as they are the fights are too
		// high for any to, so the battle is also flown low (LowPlays): there, and in the plays as recorded, no round is
		// drawn on under the ground, every round that ends on it has a strike drawn near, each strike lies on the ground
		// or the sea and is told as which, and each has a round ending by it.
		// THE LIMIT, SET AT THE MERGE: the last of those at the design's 0, with the rest. The strafing part ends each
		// gun's rounds of a grounded stream at their own time of flight, and the core shows no strike there with no
		// round near on any recording flown low (0 of 411, 390, 1,331 and 336 strikes on the four).
		{
			long Under = 0, Wrong = 0, Downs = 0, DownsBare = 0, Spurts = 0, SpurtsBare = 0, Steps = 0;
			bool bMostNear = true;
			for (const std::vector<GunPlay>* Each : { &GunPlays, &LowPlays })
			{
				for (const GunPlay& One : *Each)
				{
					Under += One.Watch.UnderGround;
					Wrong += One.Watch.GroundWrong;
					Downs += One.Watch.EndGround;
					DownsBare += One.Watch.GroundEndsBare;
					Spurts += One.Watch.GroundHits;
					SpurtsBare += One.Watch.GroundHitsBare;
					Steps += One.Check.GroundSteps;
					bMostNear = bMostNear && One.Watch.GroundHitsBare == 0;
				}
			}
			Limits.push_back({ "a stream that comes down on the ground or the sea strikes it", Under == 0 && Wrong == 0 && DownsBare == 0 && bMostNear && (Downs == 0 || Spurts > 0),
			                   std::to_string(Steps) + " stream steps came down (" + std::to_string(Seen(&GunWatch::EndGround)) + " rounds as recorded, the rest flown low): " + Of(DownsBare, Downs) + " rounds ended there with no strike near, " +
			                       Of(SpurtsBare, Spurts) + " strikes there with no round near, " + std::to_string(Wrong) + " told or laid wrong, " + std::to_string(Under) + " rounds drawn on under it",
			                   "0 rounds with no strike, 0 strikes with no round (the limit set at the merge), 0 wrong, 0 under" });
		}
		// Misses are seen to miss: how near a round passed an airframe it did not end on (nor its own), the nearest of
		// each round. A round inside one is the row above's; this is the margin the eye reads. The core lays a miss
		// 1.2 m or more clear of the aircraft it is fired at and 0.72 m or more clear of any other, as it reckons their
		// flight; the mark here (1 round in 100 within 0.75 m) catches a lay gone wrong, and is the art director's to
		// tighten.
		{
			const long Flown = Seen(&GunWatch::RoundsFlown), Near = Seen(&GunWatch::Graze);
			Limits.push_back({ "misses are seen to miss", Near * 100 <= Flown,
			                   std::to_string(Near) + " rounds within 0.75 m of an airframe they did not strike, " + std::to_string(Seen(&GunWatch::Close15)) + " more within 1.5 m, " +
			                       std::to_string(Seen(&GunWatch::Close3)) + " within 3 m, of " + std::to_string(Flown) + " flown",
			                   "at most 1 in 100 within 0.75 m" });
		}
		// And the battle's count is kept whatever the dice: in every play a model falls for every aircraft lost, and the
		// hits survived are shown.
		{
			bool bKept = true;
			std::string Counts;
			for (const GunPlay& Each : GunPlays)
			{
				bKept = bKept && Each.Check.Falls >= EventsLost - 2 && Each.Check.Falls <= EventsLost + 2 && Each.Check.HitsShown >= EventsHits * 9 / 10;
				Counts += (Counts.empty() ? "" : ", ") + std::to_string(Each.Check.Falls) + " and " + std::to_string(Each.Check.HitsShown);
			}
			Limits.push_back({ "the battle's count kept whatever the dice", bKept, "falls and hits shown, each play: " + Counts + " (the battle's " + std::to_string(EventsLost) + " and " + std::to_string(EventsHits) + ")",
			                   "falls within 2 of the battle's, 9 hits in 10 shown, in every play" });
		}
	}
	// The sky (AirSky): the day the battle gave, laid inside its heights, scattered, within the painter's room, the same
	// every time, its clearing kept over the target as the wind carries the clouds, and turned to an eye within a
	// frame's budget.
	auto OutOfHeight = [](const AirSky::Field& Fd)
	{
		// (each cloud from the day's base up and no taller than the day; each puff's middle within its cloud's foot and
		// top)
		int Out = 0;
		for (const AirSky::Cloud& Cl : Fd.Clouds)
		{
			if (Cl.At.Z < Fd.Base - 0.5 || Cl.H > Fd.Top - Fd.Base + 0.5) ++Out;
			for (int K = Cl.First; K < Cl.First + Cl.Count; ++K)
			{
				const AirSky::Puff& Pf = Fd.Puffs[static_cast<size_t>(K)];
				if (Pf.At.Z < Cl.At.Z - 0.5 || Pf.At.Z > Cl.At.Z + Cl.H + 0.5) ++Out;
			}
		}
		return Out;
	};
	const bool bCumulus = !Sky.Sky.bDeck;
	{
		const AirSky::Field Again = AirSky::Lay(Rec.Board.Cloud, SkyHere);
		bool bSame = Again.Puffs.size() == Sky.Puffs.size();
		for (size_t K = 0; bSame && K < Sky.Puffs.size(); ++K)
			bSame = Again.Puffs[K].At.X == Sky.Puffs[K].At.X && Again.Puffs[K].At.Y == Sky.Puffs[K].At.Y && Again.Puffs[K].At.Z == Sky.Puffs[K].At.Z && Again.Puffs[K].R == Sky.Puffs[K].R;
		// The wind's carry when the first bombs fall, at the battle's end, and two hours on (the field repeats: any carry
		// leaves it whole).
		const double EndAir = Show.AirNow();
		const AirSky::Vec AtBombs = Sky.DriftAt(FirstBombAir > 0.0 ? FirstBombAir : 0.0), AtEnd = Sky.DriftAt(EndAir), AtLast = Sky.DriftAt(EndAir + 7200.0);
		AirSky::Painter Paint;
		std::vector<AirSky::Vec> Points, Shadows;
		std::vector<uint8_t> Rgba, ShadowRgba;
		double WorstPaintMs = 0.0, WorstShapedMs = 0.0;
		AirSky::Painter Shaped;
		Shaped.SetSprites(true);
		std::vector<AirSky::Vec> ShapedPoints, ShapedShadows;
		std::vector<uint8_t> ShapedRgba, ShapedShadowRgba;
		std::vector<float> ShapedUvs;
		int Nearer = 0, MostDrawn = 0, MostDropped = 0, LeastNearFall = 1 << 30, MostShadows = 0, DarkAmongPuffs = 0;
		// (as the scene paints: the shadows laid apart, for their own mesh under everything)
		Paint.Paint(Sky, AirSky::Vec(Rec.Board.TargetX, Rec.Board.TargetY, 9000.0), AirSky::Vec(), SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Points, Rgba, &Shadows, &ShadowRgba); // (warm: the room made)
		Shaped.Paint(Sky, AirSky::Vec(Rec.Board.TargetX, Rec.Board.TargetY, 9000.0), AirSky::Vec(), SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), ShapedPoints, ShapedRgba, &ShapedShadows, &ShapedShadowRgba, &ShapedUvs);
		// (eyes behind the raid, low over the target, near the sea, and behind the battle's farthest falls from the target
		// and its middle one, where the fight is)
		std::vector<AirSky::Vec> Eyes = { AirSky::Vec(Rec.Board.TargetX, Rec.Board.TargetY - 6000.0, 5000.0), AirSky::Vec(Rec.Board.TargetX + 3000.0, Rec.Board.TargetY, 1200.0),
		                                  AirSky::Vec(Rec.Board.TargetX - 9000.0, Rec.Board.TargetY + 4000.0, 300.0) };
		std::vector<size_t> ByFar(FallsAt.size());
		for (size_t K = 0; K < ByFar.size(); ++K) ByFar[K] = K;
		auto FromTarget = [&](size_t K) { return std::hypot(FallsAt[K].X - Rec.Board.TargetX, FallsAt[K].Y - Rec.Board.TargetY); };
		std::sort(ByFar.begin(), ByFar.end(), [&](size_t A, size_t B) { return FromTarget(A) > FromTarget(B); });
		const size_t FallEyes = Eyes.size();
		if (!ByFar.empty())
		{
			for (const size_t K : { ByFar.front(), ByFar[ByFar.size() / 2] })
				Eyes.push_back(AirSky::Vec(FallsAt[K].X, FallsAt[K].Y - 1500.0, FallsAt[K].Z + 300.0));
		}
		const double FarthestFall = ByFar.empty() ? 0.0 : FromTarget(ByFar.front());
		for (size_t EyeIndex = 0; EyeIndex < Eyes.size(); ++EyeIndex)
		{
			const AirSky::Vec& E = Eyes[EyeIndex];
			// (the middle of 15 paints, and the least of three such tries: one paint is a few milliseconds, and one slow
			// stretch of a shared machine inside a 50 ms window is not the painter's; the pre-check, 19 November 16:11Z)
			double LeastMiddle = 1e18;
			for (int Try = 0; Try < 3; ++Try)
			{
				std::vector<double> Took;
				for (int Rep = 0; Rep < 15; ++Rep)
				{
					const auto P0 = std::chrono::steady_clock::now();
					Paint.Paint(Sky, E, AtBombs, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Points, Rgba, &Shadows, &ShadowRgba);
					Took.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - P0).count());
				}
				std::nth_element(Took.begin(), Took.begin() + 7, Took.end());
				LeastMiddle = std::min(LeastMiddle, Took[7]);
			}
			WorstPaintMs = std::max(WorstPaintMs, LeastMiddle);
			// (and as the scene paints where M_Cloud is built: each puff one of the sheet's shapes, with its places on
			// the sheet and the sun's way on it)
			{
				double LeastShaped = 1e18;
				for (int Try = 0; Try < 3; ++Try)
				{
					std::vector<double> Took;
					for (int Rep = 0; Rep < 15; ++Rep)
					{
						const auto P0 = std::chrono::steady_clock::now();
						Shaped.Paint(Sky, E, AtBombs, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), ShapedPoints, ShapedRgba, &ShapedShadows, &ShapedShadowRgba, &ShapedUvs);
						Took.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - P0).count());
					}
					std::nth_element(Took.begin(), Took.begin() + 7, Took.end());
					LeastShaped = std::min(LeastShaped, Took[7]);
				}
				WorstShapedMs = std::max(WorstShapedMs, LeastShaped);
			}
			MostDrawn = std::max(MostDrawn, Paint.Drawn());
			MostDropped = std::max(MostDropped, Paint.Dropped());
			MostShadows = std::max(MostShadows, Paint.ShadowsDrawn());
			for (size_t K = 0; K < Points.size(); ++K)
			{
				if (Rgba[K * 4 + 3] > 0 && Rgba[K * 4] < 60 && Rgba[K * 4 + 1] < 60 && Rgba[K * 4 + 2] < 60) ++DarkAmongPuffs;
			}
			if (EyeIndex >= FallEyes) LeastNearFall = std::min(LeastNearFall, Paint.Drawn());
			// (laid far to near: each puff's middle no farther from the eye than the one before)
			double Last = 1e18;
			for (size_t K = 0; K < Points.size(); K += static_cast<size_t>(Paint.PerPuff()))
			{
				const AirSky::Vec D = Points[K] - (E - AtBombs);
				const double Far = D.Dot(D);
				// (a cloud the clearing has taken is a point at its middle: it keeps its place in the order)
				if (Far > Last + 1e-6) ++Nearer;
				Last = Far;
			}
		}
		// Under the scene's cloud ink (1.3, AirBattle.cpp), only the sunlit tops rise: painted with the ink, no point is
		// brighter than without it, the brightest are as they were, and the shaded sides and bases are the ink's share
		// darker, so that the ink brings them back where they were (the art director, 16:31Z).
		int InkBrighter = 0, InkTopsMoved = 0, InkShadeOff = 0, InkTops = 0, InkShades = 0;
		{
			AirSky::Painter Plain, Inked;
			Inked.SetInk(1.3);
			std::vector<AirSky::Vec> PlainPoints, InkedPoints;
			std::vector<uint8_t> PlainRgba, InkedRgba;
			const AirSky::Vec E = Eyes.front();
			Plain.Paint(Sky, E, AtBombs, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), PlainPoints, PlainRgba);
			Inked.Paint(Sky, E, AtBombs, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), InkedPoints, InkedRgba);
			int Top = 0; // (the brightest green laid: a sunlit top)
			for (size_t K = 1; K < PlainRgba.size(); K += 4) Top = std::max(Top, static_cast<int>(PlainRgba[K]));
			for (size_t K = 0; K + 3 < PlainRgba.size() && K < InkedRgba.size(); K += 4)
			{
				const int R0 = PlainRgba[K], G0 = PlainRgba[K + 1], R1 = InkedRgba[K], G1 = InkedRgba[K + 1];
				if (R1 > R0 + 1 || G1 > G0 + 1) ++InkBrighter;
				if (G0 >= Top - 1)
				{
					++InkTops;
					if (std::abs(G1 - G0) > 1) ++InkTopsMoved;
				}
				// (a base or a shaded side: no brighter than the shaded side's own paint, 164 of green in a picture,
				// some 94 laid)
				if (G0 > 0 && G0 <= static_cast<int>(AirSky::ShadeSide.G * 255.0f))
				{
					++InkShades;
					if (std::abs(G1 * 1.3 - G0) > 2.0) ++InkShadeOff;
				}
			}
		}
		// The sky in two rooms (AirSky::Rooms, the scene's since the Mac's Midway stutter): painted both at once from each
		// eye, the near room and the far one hold every puff the plain painter draws, no puff twice, none left out of the
		// near room for want of space, the same shadows; and the near room's clouds the nearer, but at the line.
		int RoomsWrong = 0, RoomsMostNear = 0;
		double RoomsLeastLine = 1e18;
		{
			AirSky::Painter Whole;
			AirSky::Rooms Two;
			std::vector<AirSky::Vec> WholePoints, WholeShadows;
			std::vector<uint8_t> WholeRgba, WholeShadowRgba;
			for (const AirSky::Vec& E : Eyes)
			{
				Whole.Paint(Sky, E, AtBombs, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), WholePoints, WholeRgba, &WholeShadows, &WholeShadowRgba);
				Two.PaintBoth(Sky, E, AtBombs, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f));
				RoomsMostNear = std::max(RoomsMostNear, Two.Near.Drawn());
				RoomsLeastLine = std::min(RoomsLeastLine, Two.Line());
				if (Two.Near.Drawn() + Two.Far.Drawn() != Whole.Drawn() || Two.Near.Dropped() > 0 || Two.Far.Dropped() > Whole.Dropped() || Two.Far.ShadowsDrawn() != Whole.ShadowsDrawn()) ++RoomsWrong;
				// (each puff's heart, its alpha laid, summed: the same in the two rooms as in the whole)
				auto Hearts = [](const std::vector<AirSky::Vec>& Pts, const std::vector<uint8_t>& Cols, size_t Per)
				{
					long Sum = 0;
					for (size_t K = 0; K < Pts.size(); K += Per) Sum += Cols[K * 4 + 3];
					return Sum;
				};
				const size_t Per = static_cast<size_t>(Whole.PerPuff());
				if (Hearts(Two.NearPoints, Two.NearRgba, Per) + Hearts(Two.FarPoints, Two.FarRgba, Per) != Hearts(WholePoints, WholeRgba, Per)) ++RoomsWrong;
				bool bSameShadows = Two.Shadows.size() == WholeShadows.size() && Two.ShadowRgba == WholeShadowRgba;
				for (size_t K = 0; bSameShadows && K < WholeShadows.size(); ++K)
					bSameShadows = (Two.Shadows[K] - WholeShadows[K]).Length() < 1e-6;
				if (!bSameShadows) ++RoomsWrong;
			}
		}
		// The clearing over the target (within 1.8 km of it), and the field round it, as first laid, when the first
		// bombs fall, at the battle's end and two hours on.
		double WorstNear = 0.0, LeastField = 1.0, MostField = 0.0;
		for (const AirSky::Vec& D : { AirSky::Vec(), AtBombs, AtEnd, AtLast })
		{
			WorstNear = std::max(WorstNear, Sky.CoverOf(D, 0.0, 1800.0));
			const double Field = Sky.CoverOf(D, 4500.0, 9000.0);
			LeastField = std::min(LeastField, Field);
			MostField = std::max(MostField, Field);
		}
		// The same clearing as the painter draws it (the game's own path: its eased lift, its clearing, its far and near
		// puffs): the cloud drawn within 1.8 km of an eye 2.5 km over the tops, over the target and over 16 points of the
		// field round it (4.5 to 9 km out), as alpha times a puff's area a square kilometre, at each carry.
		auto PaintedOver = [&Sky, &SkyAt](AirSky::Painter& Pt, double X, double Y, const AirSky::Vec& D, std::vector<AirSky::Vec>& Pts, std::vector<uint8_t>& Cols)
		{
			const AirSky::Vec E(X, Y, Sky.Top + 2500.0);
			Pt.Paint(Sky, E, D, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Pts, Cols);
			const size_t Per = static_cast<size_t>(Pt.PerPuff()), Sides = static_cast<size_t>(Pt.Sides());
			double Sum = 0.0;
			for (size_t K = 0; K + Per <= Pts.size(); K += Per)
			{
				const double A = Cols[K * 4 + 3] / 255.0;
				if (A <= 0.0) continue;
				const AirSky::Vec Mid = Pts[K] + D;
				if (Cols[K * 4 + 0] < 60 && Cols[K * 4 + 1] < 60 && Cols[K * 4 + 2] < 60) continue; // (a cloud's shadow: dark, on the sea or the land)
				if (std::hypot(Mid.X - X, Mid.Y - Y) > 1800.0) continue;
				double R2 = 0.0;
				for (size_t J = 0; J < Sides; ++J)
				{
					const AirSky::Vec Rim = Pts[K + 1 + Sides + J] - Pts[K];
					R2 += Rim.Dot(Rim) / static_cast<double>(Sides);
				}
				Sum += A * R2 * 1e-6;
			}
			return Sum / (3.14159265358979 * 1.8 * 1.8);
		};
		double WorstPaintedShare = 0.0, LeastPaintedField = 1e9;
		{
			AirSky::Painter Over;
			std::vector<AirSky::Vec> Pts;
			std::vector<uint8_t> Cols;
			for (const AirSky::Vec& D : { AirSky::Vec(), AtBombs, AtEnd, AtLast })
			{
				const double NearSum = PaintedOver(Over, Rec.Board.TargetX, Rec.Board.TargetY, D, Pts, Cols);
				double FieldSum = 0.0;
				for (int K = 0; K < 16; ++K)
				{
					const double A = 2.0 * 3.14159265358979 * K / 16.0, Out = K % 2 ? 5500.0 : 8000.0;
					FieldSum += PaintedOver(Over, Rec.Board.TargetX + Out * std::cos(A), Rec.Board.TargetY + Out * std::sin(A), D, Pts, Cols) / 16.0;
				}
				LeastPaintedField = std::min(LeastPaintedField, FieldSum);
				WorstPaintedShare = std::max(WorstPaintedShare, FieldSum > 0.0 ? NearSum / FieldSum : 1.0);
			}
		}
		// Cloud where the fight is: the share of the sky under cloud within 4 km of each fall more than 9 km from the
		// target, where the wind had carried the field when it fell.
		auto CoverNear = [&Sky](double CX, double CY, const AirSky::Vec& D)
		{
			int Spots = 0, Under = 0;
			for (int GX = 0; GX < 24; ++GX)
				for (int GY = 0; GY < 24; ++GY)
				{
					const double LX = CX - 4000.0 + (GX + 0.5) * 8000.0 / 24 - D.X, LY = CY - 4000.0 + (GY + 0.5) * 8000.0 / 24 - D.Y;
					++Spots;
					for (size_t K = 0; K < Sky.Clouds.size(); ++K)
					{
						const AirSky::Cloud& Cl = Sky.Clouds[K];
						const AirSky::Vec Sh = Sky.ShiftNear(static_cast<int>(K), LX, LY);
						const double Ex = Cl.At.X + Sh.X - LX, Ey = Cl.At.Y + Sh.Y - LY, R = Cl.W * 0.45;
						if (Ex * Ex + Ey * Ey <= R * R && Sky.Place(static_cast<int>(K), LX, LY, D).Show >= 0.5)
						{
							++Under;
							break;
						}
					}
				}
			return Spots ? static_cast<double>(Under) / Spots : 0.0;
		};
		double LeastFar = 1.0, SumFar = 0.0;
		int FarFalls = 0;
		for (size_t K = 0; K < FallsAt.size(); ++K)
		{
			if (std::hypot(FallsAt[K].X - Rec.Board.TargetX, FallsAt[K].Y - Rec.Board.TargetY) < 9000.0) continue;
			const double Here = CoverNear(FallsAt[K].X, FallsAt[K].Y, Sky.DriftAt(FallsAir[K]));
			LeastFar = std::min(LeastFar, Here);
			SumFar += Here;
			++FarFalls;
		}
		// A thin cumulus day (a 400 m deep layer, inside the Hawaii zone's own ranges).
		AirSky::Day Thin;
		Thin.bGiven = true;
		Thin.Base = 1100.0;
		Thin.Top = 1500.0;
		const AirSky::Field Shallow = AirSky::Lay(Thin, SkyAt);
		const int Outside = OutOfHeight(Sky) + (bCumulus ? OutOfHeight(Shallow) : 0);
		Limits.push_back({ "the day's sky read from the battle", Rec.Board.Cloud.bGiven && Rec.Board.Cloud.Top >= Rec.Board.Cloud.Base + 299.0,
		                   std::string(Rec.Board.Cloud.bDeck ? "deck " : "cumulus ") + Num(Rec.Board.Cloud.Base, 0) + " to " + Num(Rec.Board.Cloud.Top, 0) + " m", "given, tops 300 m over the base" });
		Limits.push_back({ "cloud inside the day's base and tops (a thin day too)", Outside == 0 && !Sky.Clouds.empty() && !Shallow.Clouds.empty(), std::to_string(Outside) + " outside", "0" });
		Limits.push_back({ bCumulus ? "cumulus scattered, about 3/8 (share under cloud)" : "a deck covers the sky (share under cloud)",
		                   bCumulus ? Sky.Cover >= 0.31 && Sky.Cover <= 0.45 : Sky.Cover >= 0.625, Num(Sky.Cover, 2), bCumulus ? "0.31 to 0.45" : "5/8 or more" });
		if (bCumulus)
		{
			Limits.push_back({ "the target's clearing kept as the clouds drift", WorstNear <= 0.6 * LeastField && LeastField >= 0.29 && MostField <= 0.47,
			                   "near " + Num(WorstNear, 2) + ", field " + Num(LeastField, 2) + " to " + Num(MostField, 2) + " (drift " + Num(AtBombs.Length(), 0) + " m at the bombs, " + Num(AtLast.Length(), 0) + " at most)",
			                   "near at most 6/10 of the field, the field whole at every carry" });
			Limits.push_back({ "the clearing as the painter draws it", WorstPaintedShare <= 0.6 && LeastPaintedField > 0.0,
			                   "near " + Num(WorstPaintedShare, 2) + " of the field at worst (field " + Num(LeastPaintedField, 3) + " at least)", "at most 6/10 of the field at every carry" });
		}
		Limits.push_back({ "clouds where the fight is, far from the target too", FarFalls == 0 || (LeastFar >= (bCumulus ? 0.12 : 0.45) && SumFar / FarFalls >= (bCumulus ? 0.25 : 0.6)),
		                   std::to_string(FarFalls) + " falls past 9 km (farthest " + Num(FarthestFall / 1000.0, 1) + " km): cover " + Num(LeastFar, 2) + " at least, " + Num(FarFalls ? SumFar / FarFalls : 0.0, 2) + " on average",
		                   bCumulus ? "at least 0.12, 0.25 on average" : "at least 0.45, 0.6 on average" });
		Limits.push_back({ "the sky within the painter's room", MostDropped == 0 && LeastNearFall >= 300,
		                   std::to_string(MostDrawn) + " puffs at most of " + std::to_string(Sky.MostPuffs) + ", " + std::to_string(MostDropped) + " left out; " + std::to_string(LeastNearFall == (1 << 30) ? 0 : LeastNearFall) + " drawn round the falls' eyes at least",
		                   "none left out, 300 or more round the fight" });
		Limits.push_back({ "the same sky every time", bSame, bSame ? "same" : "different", "same" });
		Limits.push_back({ "the sky laid far to near for each eye", Nearer == 0, std::to_string(Nearer) + " out of order", "0" });
		Limits.push_back({ "the sky in two rooms, every puff in one", RoomsWrong == 0,
		                   std::to_string(RoomsWrong) + " eyes wrong; the near room " + std::to_string(RoomsMostNear) + " puffs at most of " + std::to_string(AirSky::Rooms().NearRoom()) + ", the line " + Num(RoomsLeastLine, 0) + " m at least",
		                   "0 (the whole sky's puffs, hearts and shadows; none left out of the near room)" });
		// (5 ms on this machine: the art director's light, each puff's own lump and its soft edges away from the sun, and the
		// shadows laid apart made a paint about 1.8 times what it was when the mark was 4, some 3 ms here, up to 4.2 under
		// clang on a busy machine; the mark is there to catch the next such step, and the Mac's cores are faster)
		Limits.push_back({ "the sky turned to an eye within budget", WorstPaintMs < 5.0, Num(WorstPaintMs, 2) + " ms", "under 5 ms (the least of three middles of 15)" });
		Limits.push_back({ "the sky's shapes turned to an eye within budget", WorstShapedMs < 5.0, Num(WorstShapedMs, 2) + " ms", "under 5 ms (the least of three middles of 15; the sheet's shapes, as where M_Cloud is built)" });
		// (the shadows in their own room, none among the puffs: their mesh is drawn under the ground's fires and smoke)
		Limits.push_back({ "the flyover shows the raid and what it comes for", bFlyBegun && FlyShots.count(0) && FlyShots.count(1) && Flyover.Length() <= 14.0,
		                   std::to_string(FlyShots.size()) + " shots, " + Num(Flyover.Length()) + " s", "the raid and the target shown, 14 s or less" });
		// (a battle with no guard has no guard picture: its lead figures stand at their starts)
		const bool bGuardPicture = FlyLeadMost > 0.0;
		Limits.push_back({ "the flyover's close shots hold their three near",
		                   FlyLeadPictures >= 1 && FlyTrailLeast >= 100.0 && FlyTrailMost <= 160.0 && (!bGuardPicture || (FlyLeadLeast >= 120.0 && FlyLeadMost <= 180.0)) &&
		                       FlyLeastInFrame >= 3,
		                   std::to_string(FlyLeadPictures) + " pictures, the raid's nearest three " + Num(FlyTrailLeast, 0) + " to " + Num(FlyTrailMost, 0) + " px wide, the guard's lead " +
		                       (bGuardPicture ? Num(FlyLeadLeast, 0) + " to " + Num(FlyLeadMost, 0) : std::string("(no guard)")) + ", at least " +
		                       std::to_string(FlyLeastInFrame) + " of the group in the picture",
		                   "the raid's 100 to 160 px of 2881, the guard's 120 to 180 (the art director, builds 1750 and 1760), at least 3 in the picture after each shot's first second" });
		Limits.push_back({ "the flyover's raid shot shows the raid stretching ahead", FlyRaidShown > 0 && FlyRaidInFrame * 4 >= FlyRaidShown * 3,
		                   std::to_string(FlyRaidInFrame) + " of its " + std::to_string(FlyRaidShown) + " planes in the picture",
		                   "three quarters of the raid in the picture, seen from behind its trailing element (the art director, build 1760)" });
		Limits.push_back({ "the flyover holds a harbour in its lower third", !bFlyHarbour || (FlyHarbourRow >= FlyThird && FlyHarbourRow < FlyFoot),
		                   bFlyHarbour ? "the target at row " + Num(FlyHarbourRow, 0) : std::string("not a harbour"),
		                   "the lower third, rows 1126 to 1591 of 1786, at the target's picture (the art director on Oahu, build 1750)" });
		// Every two hulls apart, at their real sizes as the stage draws them (AirFeed::HullOf): Brian on the film of build
		// 1768, Battleship Row's ships drawn on top of each other. Each hull a box of her length and beam, the gap the widest
		// along any of the two boxes' sides; in every view that carries the ships.
		{
			int HullPairs = 0, HullsTouching = 0;
			double LeastGap = 1e9;
			std::string Closest;
			for (const AirFeed::Frame& F : Rec.Views)
			{
				const std::vector<AirFeed::ShipRow>& Rows = F.ShipRows;
				std::vector<std::array<std::pair<double, double>, 4>> Boxes;
				for (const AirFeed::ShipRow& Row : Rows)
				{
					const AirFeed::HullSize Size = AirFeed::HullOf(Row.Class, Row.Name, Row.Nation);
					const double Ax = std::sin(Row.Face), Ay = std::cos(Row.Face), Bx = std::cos(Row.Face), By = -std::sin(Row.Face);
					std::array<std::pair<double, double>, 4> Box;
					int K = 0;
					for (const auto& [A, B] : { std::pair<double, double>{ -0.5, -0.5 }, { 0.5, -0.5 }, { 0.5, 0.5 }, { -0.5, 0.5 } })
					{
						Box[static_cast<size_t>(K++)] = { Row.X + Ax * Size.Long * A + Bx * Size.Beam * B, Row.Y + Ay * Size.Long * A + By * Size.Beam * B };
					}
					Boxes.push_back(Box);
				}
				for (size_t I = 0; I < Boxes.size(); ++I)
				{
					for (size_t J = I + 1; J < Boxes.size(); ++J)
					{
						if (Rows[I].bSunk || Rows[J].bSunk) continue;
						double Gap = -1e9;
						for (const auto* Poly : { &Boxes[I], &Boxes[J] })
						{
							for (size_t E = 0; E < 4; ++E)
							{
								const auto& P0 = (*Poly)[E];
								const auto& P1 = (*Poly)[(E + 1) % 4];
								double Nx = P1.second - P0.second, Ny = P0.first - P1.first;
								const double Len = std::hypot(Nx, Ny);
								if (Len < 1e-9) continue;
								Nx /= Len, Ny /= Len;
								double MinI = 1e18, MaxI = -1e18, MinJ = 1e18, MaxJ = -1e18;
								for (const auto& [X, Y] : Boxes[I]) MinI = std::min(MinI, X * Nx + Y * Ny), MaxI = std::max(MaxI, X * Nx + Y * Ny);
								for (const auto& [X, Y] : Boxes[J]) MinJ = std::min(MinJ, X * Nx + Y * Ny), MaxJ = std::max(MaxJ, X * Nx + Y * Ny);
								Gap = std::max(Gap, std::max(MinJ - MaxI, MinI - MaxJ));
							}
						}
						++HullPairs;
						if (Gap < 1.0) ++HullsTouching;
						if (Gap < LeastGap)
						{
							LeastGap = Gap;
							Closest = (Rows[I].Name.empty() ? Rows[I].Class + " " + std::to_string(Rows[I].Id) : Rows[I].Name) + " and " + (Rows[J].Name.empty() ? Rows[J].Class + " " + std::to_string(Rows[J].Id) : Rows[J].Name);
						}
					}
				}
			}
			if (HullPairs > 0)
			{
				Limits.push_back({ "every two hulls apart at their real sizes", HullsTouching == 0,
				                   std::to_string(HullsTouching) + " pairs within 1 m of each other or into it; the closest " + Closest + ", " + Num(LeastGap, 1) + " m",
				                   "0 (Brian on the film of build 1768: Battleship Row's ships on top of each other)" });
			}
		}
		if (bBoardGround)
		{
			Limits.push_back({ "with no stage, the board's land takes bombs and falls as land", BoardWrong == 0 && BoardLandBombs + BoardLandFalls > 0,
			                   std::to_string(BoardLandBombs) + " bombs and " + std::to_string(BoardLandFalls) + " falls on the land, " + std::to_string(BoardSeaBombs) + " and " + std::to_string(BoardSeaFalls) + " on the sea; " + std::to_string(BoardWrong) + " told wrong or laid off the ground",
			                   "0 wrong, some on the land (the pre-check on round 14)" });
		}
		Limits.push_back({ "the flyover's eye glides clear", FlyNoLook == 0 && FlyLow == 0 && FlyJumps == 0 && FlyIntoPlane == 0,
		                   std::to_string(FlyJumps) + " jumps, " + std::to_string(FlyLow) + " low, " + std::to_string(FlyIntoPlane) + " in a plane, " + std::to_string(FlyNoLook) + " looks lost",
		                   "0 of each" });
		// The stage's ground as the scene paints it (AirGround; the art director on build 1734): the grass varied and
		// patched with red earth, the beach a band along the water, the shallows darkening into the deep (not where a
		// lagoon is), the runways clear of the land; every triangle's points there. (A raid with no stage, as Bremen's,
		// flies over the board's own land: there is no stage's ground, yard or berths to hold to.)
		if (Rec.Board.StageKey.empty())
		{
			std::printf("  no stage: the board's own land under the battle; the stage's ground, yard and berths not checked\n");
		}
		else
		{
			const std::string Base = Path.substr(0, Path.rfind("/AirBattles/") == std::string::npos ? 0 : Path.rfind("/AirBattles/"));
			const std::string GroundPath = Base + "/Stages/" + Rec.Board.StageKey + ".json";
			std::ifstream GroundIn(GroundPath);
			std::stringstream GroundText;
			GroundText << GroundIn.rdbuf();
			AirFeed::Json GroundJson;
			std::string GroundWhy;
			const bool bGround = !Rec.Board.StageKey.empty() && GroundIn && AirFeed::Parse(GroundText.str(), GroundJson, GroundWhy);
			std::vector<std::string> PaintNames;
			std::vector<AirGround::Rgb8> PaintColours;
			const AirFeed::Json& PaletteJson = GroundJson["palette"];
			for (size_t K = 0; bGround && K < PaletteJson.Count(); ++K)
			{
				PaintNames.push_back(PaletteJson.At(K)["name"].Str(""));
				const unsigned long Packed = std::strtoul(PaletteJson.At(K)["hex"].Str("#808080").c_str() + 1, nullptr, 16);
				PaintColours.push_back({ static_cast<uint8_t>((Packed >> 16) & 255u), static_cast<uint8_t>((Packed >> 8) & 255u), static_cast<uint8_t>(Packed & 255u) });
			}
			std::vector<AirGround::Part> GroundParts;
			const AirFeed::Json& MeshesJson = GroundJson["meshes"];
			for (size_t K = 0; bGround && K < MeshesJson.Count(); ++K)
			{
				const AirFeed::Json& M = MeshesJson.At(K);
				AirGround::Part Part;
				Part.Name = M["name"].Str("");
				for (size_t I = 0; I + 2 < M["v"].Count(); I += 3)
				{
					Part.X.push_back(M["v"].At(I).Num() * 0.1);
					Part.Y.push_back(M["v"].At(I + 1).Num() * 0.1);
					Part.Z.push_back(M["v"].At(I + 2).Num() * 0.1);
				}
				for (size_t I = 0; I < M["c"].Count(); ++I) Part.Paints.push_back(M["c"].At(I).Int());
				for (size_t I = 0; I < M["t"].Count(); ++I) Part.Tris.push_back(M["t"].At(I).Int());
				GroundParts.push_back(std::move(Part));
			}
			AirGround::Tally GroundTally;
			const auto GroundStart = std::chrono::steady_clock::now();
			AirGround::Focus GroundNear;
			GroundNear.X = Rec.Board.StageAimX;
			GroundNear.Y = Rec.Board.StageAimY;
			GroundNear.Radius = 4000.0; // (as AAirStage::BuildGround asks)
			const std::vector<AirGround::Drawn> GroundDrawn = AirGround::Paint(GroundParts, PaintNames, PaintColours, { 0x1F, 0x57, 0x7A }, &GroundTally, GroundNear);
			const double GroundMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - GroundStart).count();
			const int CanePaint = static_cast<int>(std::find(PaintNames.begin(), PaintNames.end(), std::string("cane")) - PaintNames.begin());
			const int ConcretePaint = static_cast<int>(std::find(PaintNames.begin(), PaintNames.end(), std::string("concrete")) - PaintNames.begin());
			int BadIndex = 0, GrewWrong = 0;
			size_t TrisBefore = 0, TrisAfter = 0, SlabTops = 0;
			double SlabRimOff = -1.0;
			double SumL = 0.0, SumLL = 0.0;
			int Grass = 0;
			for (size_t K = 0; K < GroundDrawn.size() && K < GroundParts.size(); ++K)
			{
				const AirGround::Drawn& D = GroundDrawn[K];
				for (const int I : D.Tris) BadIndex += I < 0 || static_cast<size_t>(I) >= D.X.size() || D.Colours.size() != D.X.size();
				// (the land's and the shallows' rims as long as they were: a point laid on an edge is shared by both its
				// triangles, so no crack opens; the rest as they were)
				const bool bSplit = D.Name == "land" || D.Name == "shallows";
				const double RimWas = bSplit ? AirGround::RimLength(GroundParts[K].X, GroundParts[K].Y, GroundParts[K].Tris) : 0.0;
				GrewWrong += bSplit ? std::fabs(AirGround::RimLength(D.X, D.Y, D.Tris) - RimWas) > 0.5 + 1e-6 * RimWas || D.Tris.size() < GroundParts[K].Tris.size()
				           : D.Name == "works" ? D.Tris.size() < GroundParts[K].Tris.size()
				                               : D.Tris.size() != GroundParts[K].Tris.size();
				if (D.Name == "works")
				{
					// (the works' flat concrete tops cut into slabs: the slabs' rim no longer than the tops' was, so no crack
					// opens between two slabs or two triangles' slabs; shorter where the stage's own mesh had a seam, as two
					// triangles' edges cut at the same grid's lines meet point for point)
					const AirGround::Part& P = GroundParts[K];
					std::vector<int> Tops, Slabs;
					for (size_t Tri = 0; Tri + 2 < P.Tris.size(); Tri += 3)
					{
						const int A = P.Tris[Tri], B = P.Tris[Tri + 1], Cn = P.Tris[Tri + 2];
						if (P.Paints[A] != ConcretePaint || P.Paints[B] != ConcretePaint || P.Paints[Cn] != ConcretePaint) continue;
						const double UX = P.X[B] - P.X[A], UY = P.Y[B] - P.Y[A], UZ = P.Z[B] - P.Z[A], VX = P.X[Cn] - P.X[A], VY = P.Y[Cn] - P.Y[A], VZ = P.Z[Cn] - P.Z[A];
						const double NX = UY * VZ - UZ * VY, NY = UZ * VX - UX * VZ, NZ = UX * VY - UY * VX, Sz = std::sqrt(NX * NX + NY * NY + NZ * NZ);
						if (Sz > 1e-9 && std::fabs(NZ) > 0.95 * Sz) Tops.insert(Tops.end(), { A, B, Cn });
					}
					for (size_t Tri = 0; Tri + 2 < D.Tris.size(); Tri += 3)
						if (static_cast<size_t>(D.Tris[Tri]) >= P.X.size()) Slabs.insert(Slabs.end(), { D.Tris[Tri], D.Tris[Tri + 1], D.Tris[Tri + 2] });
					const double TopsRim = AirGround::RimLength(P.X, P.Y, Tops), SlabsRim = AirGround::RimLength(D.X, D.Y, Slabs);
					SlabRimOff = std::max(SlabRimOff, (SlabsRim - TopsRim) / std::max(TopsRim, 1.0));
					SlabTops += Tops.size() / 3;
				}
				TrisBefore += GroundParts[K].Tris.size() / 3;
				TrisAfter += D.Tris.size() / 3;
				if (D.Name != "land") continue;
				for (size_t I = 0; I < GroundParts[K].Paints.size(); ++I)
				{
					// (the grass within 6 km of the stage's aim: how much its brightness varies)
					if (GroundParts[K].Paints[I] == CanePaint && std::hypot(D.X[I] - Rec.Board.StageAimX, D.Y[I] - Rec.Board.StageAimY) < 6000.0)
					{
						const double L = AirGround::Luma(D.Colours[I]);
						SumL += L;
						SumLL += L * L;
						++Grass;
					}
				}
			}
			const double GrassSpread = Grass > 1 ? std::sqrt(std::max(0.0, SumLL / Grass - (SumL / Grass) * (SumL / Grass))) : 0.0;
			// The yard (the historian's 1941 blocks, tools/stages/yard.py): each row whole; each ship in a dock inside its
			// basin and along it, Shaw inside YFD-2's walls; no ship's hull on a shop's pad, the apron, a quay or a dock
			// not hers (the historian's berths of 19 November: every hull clear); the land under each block between its
			// skirt's foot and its top (the cloud pre-check on round 6: pads lay under the land); the paved tops worn.
			int YardRows = 0, YardBad = 0, DockedIn = 0, DockedOut = 0, ShipUnder = 0, LandOver = 0, LandUnder = 0, HullAground = 0, HullOnPier = 0;
			std::string Aground;
			const AirFeed::Json& YardJson = GroundJson["yard"];
			struct YardBlock { double X, Y, Ground, Long, Wide, Heading; int Kind; };
			std::vector<YardBlock> Blocks;
			for (size_t K = 0; bGround && K < YardJson.Count(); ++K)
			{
				const AirFeed::Json& Row = YardJson.At(K);
				const int Kind = Row.Count() >= 8 ? Row.At(7).Int() : -1;
				YardBad += Row.Count() != 8 || Kind < 0 || Kind > 4 || Row.At(3).Num() <= 0.0 || Row.At(4).Num() <= 0.0;
				if (Row.Count() >= 8) Blocks.push_back({ Row.At(0).Num(), Row.At(1).Num(), Row.At(2).Num(), Row.At(3).Num(), Row.At(4).Num(), Row.At(6).Num(), Kind });
				++YardRows;
			}
			// (a point's place along a block's length and across it, metres from its middle)
			auto InBlock = [](const YardBlock& Block, double Px, double Py, double Pad, double& Along, double& Across)
			{
				const double H = Block.Heading * 3.14159265358979 / 180.0;
				Along = (Px - Block.X) * std::sin(H) + (Py - Block.Y) * std::cos(H);
				Across = (Px - Block.X) * std::cos(H) - (Py - Block.Y) * std::sin(H);
				return std::fabs(Along) <= Block.Long * 0.5 + Pad && std::fabs(Across) <= Block.Wide * 0.5 + Pad;
			};
			{
				// (the record's ships are on the board: turned back to the stage as AAirStage::BoardToStage does)
				const double Ct = std::cos(Rec.Board.StageTurn), St = std::sin(Rec.Board.StageTurn);
				for (const AirFeed::ShipRow& Row : Rec.Views.front().ShipRows)
				{
					const double Dx = Row.X - Rec.Board.TargetX, Dy = Row.Y - Rec.Board.TargetY;
					const double Sx = Rec.Board.StageAimX + Dx * Ct + Dy * St, Sy = Rec.Board.StageAimY - Dx * St + Dy * Ct;
					const double Heading = Row.Face + Rec.Board.StageTurn; // radians, the compass's way
					const AirFeed::HullSize Size = AirFeed::HullOf(Row.Class, Row.Name, Row.Nation);
					const double ShipLong = Size.Long, ShipBeam = Size.Beam;
					const bool bDocked = Row.bDock || Row.Berth == "YFD-2";
					// (her hull's outline every 5 m and its middle line)
					std::vector<std::pair<double, double>> Hull;
					for (double A = -0.5; A <= 0.5 + 1e-9; A += 5.0 / ShipLong)
					{
						for (const double B : { -0.5, 0.0, 0.5 })
						{
							Hull.push_back({ Sx + std::sin(Heading) * ShipLong * A + std::cos(Heading) * ShipBeam * B, Sy + std::cos(Heading) * ShipLong * A - std::sin(Heading) * ShipBeam * B });
						}
					}
					bool bInside = false, bUnder = false;
					for (const YardBlock& Block : Blocks)
					{
						double Along = 0.0, Across = 0.0;
						const double Skew = std::fabs(std::sin(Heading - Block.Heading * 3.14159265358979 / 180.0)); // (0 along the block, either way)
						if (bDocked && (Block.Kind == 1 || Block.Kind == 3))
						{
							InBlock(Block, Sx, Sy, 0.0, Along, Across);
							const double BasinLong = Block.Kind == 1 ? Block.Long - 20.0 : Block.Long, BasinWide = Block.Kind == 1 ? Block.Wide - 15.0 : Block.Wide - 6.0;
							bInside = bInside || (std::fabs(Along) + ShipLong * 0.5 <= BasinLong * 0.5 + 1.0 && std::fabs(Across) <= BasinWide * 0.5 && Skew < std::sin(5.0 * 3.14159265358979 / 180.0));
							continue;
						}
						const double Pad = Block.Kind == 0 ? 12.0 : 0.0;
						for (const auto& [Px, Py] : Hull) bUnder = bUnder || InBlock(Block, Px, Py, Pad, Along, Across);
					}
					if (!bDocked)
					{
						// (and none of it on the land or a pier's top, as the stage file lays them: a docked ship lies on land)
						double MinX = Hull.front().first, MaxX = MinX, MinY = Hull.front().second, MaxY = MinY;
						for (const auto& [Px, Py] : Hull)
						{
							MinX = std::min(MinX, Px), MaxX = std::max(MaxX, Px), MinY = std::min(MinY, Py), MaxY = std::max(MaxY, Py);
						}
						for (const AirGround::Part& Mesh : GroundParts)
						{
							if (Mesh.Name != "land" && Mesh.Name != "works") continue;
							bool bOn = false;
							for (size_t Corner = 0; Corner + 2 < Mesh.Tris.size() && !bOn; Corner += 3)
							{
								const size_t I0 = static_cast<size_t>(Mesh.Tris[Corner]), I1 = static_cast<size_t>(Mesh.Tris[Corner + 1]), I2 = static_cast<size_t>(Mesh.Tris[Corner + 2]);
								if (I0 >= Mesh.X.size() || I1 >= Mesh.X.size() || I2 >= Mesh.X.size()) continue;
								const double Ax = Mesh.X[I0], Ay = Mesh.Y[I0], Bx = Mesh.X[I1], By = Mesh.Y[I1], Cx = Mesh.X[I2], Cy = Mesh.Y[I2];
								if (std::max({ Ax, Bx, Cx }) < MinX || std::min({ Ax, Bx, Cx }) > MaxX || std::max({ Ay, By, Cy }) < MinY || std::min({ Ay, By, Cy }) > MaxY) continue;
								const double Den = (By - Cy) * (Ax - Cx) + (Cx - Bx) * (Ay - Cy);
								if (std::fabs(Den) < 1e-9) continue; // (a pier's side, upright)
								for (const auto& [Px, Py] : Hull)
								{
									const double L0 = ((By - Cy) * (Px - Cx) + (Cx - Bx) * (Py - Cy)) / Den, L1 = ((Cy - Ay) * (Px - Cx) + (Ax - Cx) * (Py - Cy)) / Den;
									if (L0 > 1e-6 && L1 > 1e-6 && 1.0 - L0 - L1 > 1e-6)
									{
										bOn = true;
										break;
									}
								}
							}
							if (bOn)
							{
								(Mesh.Name == "land" ? HullAground : HullOnPier) += 1;
								Aground += (Aground.empty() ? "" : ", ") + (Row.Name.empty() ? Row.Class : Row.Name) + (Row.Berth.empty() ? " " + std::to_string(Row.Id) + " at " + Num(Sx, 0) + " " + Num(Sy, 0) : " at " + Row.Berth) + (Mesh.Name == "land" ? " on land" : " on a pier");
							}
						}
					}
					ShipUnder += bUnder;
					if (bDocked)
					{
						(bInside ? DockedIn : DockedOut) += 1;
					}
				}
			}
			// (the land's points under each block's top, a shop's with its pad: none above its top, none below its skirt)
			for (const AirGround::Drawn& D : GroundDrawn)
			{
				if (D.Name != "land") continue;
				for (const YardBlock& Block : Blocks)
				{
					if (Block.Kind > 2) continue;
					const double Pad = Block.Kind == 0 ? 12.0 : 0.0;
					const double Top = Block.Kind == 1 ? 0.2 : 0.25, Foot = -4.5; // (AAirStage::BuildWorks's floor, paving and skirt)
					for (size_t I = 0; I < D.X.size(); ++I)
					{
						double Along = 0.0, Across = 0.0;
						if (!InBlock(Block, D.X[I], D.Y[I], Pad, Along, Across)) continue;
						LandOver += D.Z[I] > Block.Ground + Top;
						LandUnder += D.Z[I] < Block.Ground + Foot;
					}
				}
			}
			// (the pads' and the apron's paving as AAirStage::BuildWorks lays it, in the stage's concrete)
			AirGround::Drawn Paved;
			Paved.Name = "paving";
			int PanelCount = 0;
			double PanelSum = 0.0, PanelSquares = 0.0, PanelLow = 255.0, PanelHigh = 0.0;
			{
				const auto Found = std::find(PaintNames.begin(), PaintNames.end(), std::string("concrete"));
				const AirGround::Rgb8 Concrete = Found == PaintNames.end() ? AirGround::Rgb8{ 0x8F, 0x8B, 0x83 } : PaintColours[static_cast<size_t>(Found - PaintNames.begin())];
				for (const YardBlock& Block : Blocks)
				{
					if (Block.Kind != 0 && Block.Kind != 2) continue;
					const double Pad = Block.Kind == 0 ? 24.0 : 0.0;
					const double H = Block.Heading * 3.14159265358979 / 180.0;
					for (const AirGround::Panel& Square : AirGround::Paving(Block.X, Block.Y, Block.Long + Pad, Block.Wide + Pad, Block.Heading, Concrete))
					{
						const double L = AirGround::Luma(Square.Colour);
						PanelSum += L;
						PanelSquares += L * L;
						PanelLow = std::min(PanelLow, L);
						PanelHigh = std::max(PanelHigh, L);
						++PanelCount;
						const int First = static_cast<int>(Paved.X.size());
						for (const auto& [A, B] : { std::pair<double, double>{ -0.5, -0.5 }, { 0.5, -0.5 }, { 0.5, 0.5 }, { -0.5, 0.5 } })
						{
							Paved.X.push_back(Square.X + std::sin(H) * Square.Long * A + std::cos(H) * Square.Wide * B);
							Paved.Y.push_back(Square.Y + std::cos(H) * Square.Long * A - std::sin(H) * Square.Wide * B);
							Paved.Z.push_back(Block.Ground + 0.3);
							Paved.Colours.push_back(Square.Colour);
						}
						for (const int I : { 0, 1, 2, 0, 2, 3 }) Paved.Tris.push_back(First + I);
					}
				}
			}
			const double PanelMean = PanelCount ? PanelSum / PanelCount : 0.0;
			const double PanelSpread = PanelCount > 1 ? std::sqrt(std::max(0.0, PanelSquares / PanelCount - PanelMean * PanelMean)) : 0.0;
			const bool bOahu = Rec.Board.StageKey == "oahu";
			std::printf("  yard (%s): %d blocks, %d malformed, %d docked ships inside their docks and %d outside, %d hulls on a block, %d on land and %d on a pier%s%s%s; land points %d over a block's top, %d under its skirt; %d paving slabs, luma %.1f (spread %.1f, %.0f..%.0f)\n",
			            Rec.Board.StageKey.c_str(), YardRows, YardBad, DockedIn, DockedOut, ShipUnder, HullAground, HullOnPier, Aground.empty() ? "" : " (", Aground.c_str(), Aground.empty() ? "" : ")", LandOver, LandUnder, PanelCount, PanelMean, PanelSpread, PanelLow, PanelHigh);
			Limits.push_back({ "every hull in the water, clear of the land and the piers", bGround && HullAground == 0 && HullOnPier == 0,
			                   std::to_string(HullAground) + " on land, " + std::to_string(HullOnPier) + " on a pier" + (Aground.empty() ? std::string() : ": " + Aground),
			                   "0 of each (the historian's berths of 19 November; the docked ships aside)" });
			Limits.push_back({ "the yard's blocks whole, the docked ships in their docks", bGround && YardBad == 0 && DockedOut == 0 && ShipUnder == 0 && LandOver == 0 && LandUnder == 0 && (!bOahu || (YardRows >= 10 && DockedIn == 4)),
			                   std::to_string(YardRows) + " blocks, " + std::to_string(YardBad) + " malformed, " + std::to_string(DockedIn) + " docked in, " + std::to_string(DockedOut) + " out, " + std::to_string(ShipUnder) + " hulls on a block, " + std::to_string(LandOver) + " land points over a top, " + std::to_string(LandUnder) + " under a skirt",
			                   "at Oahu Pennsylvania, Cassin and Downes in Dry Dock No. 1 along its 211 degrees and Shaw in YFD-2; no hull on a block; the land under each block between its skirt and its top" });
			Limits.push_back({ "the yard's paving worn, about 140 luma", !bOahu || (PanelCount > 0 && PanelMean >= 134.0 && PanelMean <= 146.0 && PanelSpread >= 3.0 && PanelSpread <= 9.0),
			                   std::to_string(PanelCount) + " slabs, luma " + Num(PanelMean) + ", spread " + Num(PanelSpread),
			                   "at Oahu luma 134 to 146, spread 3 to 9 (the art director: about 140, stains and patching of 6 to 10)" });
			if (std::getenv("AIR_GROUND_DUMP") && bGround)
			{
				// (the painted ground for Tools/air_show/ground_picture.py: a top-down picture of the stage as painted)
				std::ofstream Dump(OutDir + "/ground-" + Rec.Board.StageKey + ".bin", std::ios::binary);
				auto Word = [&Dump](uint32_t W) { Dump.write(reinterpret_cast<const char*>(&W), 4); };
				Word(static_cast<uint32_t>(GroundDrawn.size() + 1));
				auto Part = [&Dump, &Word](const AirGround::Drawn& D)
				{
					Word(static_cast<uint32_t>(D.Name.size()));
					Dump.write(D.Name.data(), static_cast<std::streamsize>(D.Name.size()));
					Word(static_cast<uint32_t>(D.X.size()));
					for (size_t I = 0; I < D.X.size(); ++I)
					{
						const float Place[3] = { static_cast<float>(D.X[I]), static_cast<float>(D.Y[I]), static_cast<float>(D.Z[I]) };
						Dump.write(reinterpret_cast<const char*>(Place), sizeof(Place));
						const uint8_t Rgb[4] = { D.Colours[I].R, D.Colours[I].G, D.Colours[I].B, 255 };
						Dump.write(reinterpret_cast<const char*>(Rgb), 4);
					}
					Word(static_cast<uint32_t>(D.Tris.size()));
					Dump.write(reinterpret_cast<const char*>(D.Tris.data()), static_cast<std::streamsize>(D.Tris.size() * sizeof(int)));
				};
				for (const AirGround::Drawn& D : GroundDrawn) Part(D);
				Part(Paved);
			}
			std::printf("  ground (%s): the works' %zu flat concrete tops cut into %d slabs, their rim %+.4f%% of the tops'\n", Rec.Board.StageKey.c_str(), SlabTops, GroundTally.Slabs, SlabRimOff * 100.0);
			std::printf("  ground (%s): %d points laid near the battle (%zu triangles, %zu before), %d sand splits, shallows %d faded and %d kept, %d sheltered shore points, %d shallows points by a wall, %d earth, %d concrete worn, %d runway points lifted, grass spread %.1f luma over %d points, %.0f ms%s\n",
			            Rec.Board.StageKey.c_str(), GroundTally.Refined, TrisAfter, TrisBefore, GroundTally.SandSplit, GroundTally.ShoreFaded, GroundTally.ShoreKept, GroundTally.Sheltered, GroundTally.Walled, GroundTally.Earth, GroundTally.Worn,
			            GroundTally.RunwayRaised, GrassSpread, Grass, GroundMs, GroundTally.bLagoon ? ", a lagoon: the shallows keep their edge" : "");
			const bool bShore = GroundTally.bLagoon ? GroundTally.ShoreFaded == 0 : GroundTally.ShoreFaded > 0 && GroundTally.ShoreKept > 0 && GroundTally.ShoreFaded * 4 >= GroundTally.ShoreKept;
			// (a coast with no works keeps its shallows band all round, no point of it taken for a wall: Oahu painted again
			// without its works part; the pre-check, 19:40Z, found every shallows point there the deep sea's)
			AirGround::Tally BareTally;
			if (bOahu)
			{
				std::vector<AirGround::Part> Bare;
				for (const AirGround::Part& P : GroundParts)
					if (P.Name != "works") Bare.push_back(P);
				AirGround::Paint(Bare, PaintNames, PaintColours, { 0x1F, 0x57, 0x7A }, &BareTally, GroundNear);
				std::printf("  ground (%s) without its works: shallows %d faded and %d kept, %d by a wall\n", Rec.Board.StageKey.c_str(), BareTally.ShoreFaded, BareTally.ShoreKept, BareTally.Walled);
			}
			const bool bBareShore = BareTally.Walled == 0 && BareTally.ShoreKept > 0 && BareTally.ShoreFaded > 0;
			Limits.push_back({ "the stage's ground painted as the art director asked", bGround && BadIndex == 0 && GrewWrong == 0 && bShore && GroundTally.SandSplit > 0 && GroundTally.RunwayRaised > 0 && TrisAfter <= TrisBefore + 90000 &&
			                   (!bOahu || (GroundTally.Earth > 0 && GrassSpread >= 5.0 && GroundTally.Refined > 0 && GroundTally.Sheltered > 0 && GroundTally.Worn > 0 && GroundTally.Walled > 0 && bBareShore && GroundTally.Slabs > 0 && SlabRimOff < 1e-4)),
			                   bGround ? std::to_string(GroundTally.SandSplit) + " sand splits, " + std::to_string(GroundTally.ShoreFaded) + " shallows points faded, " + std::to_string(GroundTally.Earth) + " earth, grass spread " + Num(GrassSpread) + ", " + std::to_string(GroundTally.Refined) + " laid near, " + std::to_string(TrisAfter - TrisBefore) + " triangles more, " + std::to_string(BadIndex) + " bad points, " + std::to_string(GrewWrong) + " cracked or lost"
			                           : "no stage at " + GroundPath,
			                   "sand split, shallows faded (kept at a lagoon), runways lifted; at Oahu earth patches and the grass varied by 5 luma or more, the ground split near the harbour, sheltered shores plain, concrete worn, the quays paved in slabs with no crack between them, the band kept with no works; no cracks; 90,000 triangles more at most" });
		}
		Limits.push_back({ "the cloud ink lifts only the sunlit tops", InkBrighter == 0 && InkTopsMoved == 0 && InkShadeOff == 0 && InkTops > 0 && InkShades > 0,
		                   std::to_string(InkBrighter) + " brighter, " + std::to_string(InkTopsMoved) + " of " + std::to_string(InkTops) + " tops moved, " + std::to_string(InkShadeOff) + " of " + std::to_string(InkShades) + " shades off",
		                   "0 brighter, tops as they were, shades the ink's share darker" });
		Limits.push_back({ "the clouds' shadows laid apart from the puffs", DarkAmongPuffs == 0 && (!bCumulus || Rec.Board.bNight || MostShadows > 0),
		                   std::to_string(MostShadows) + " shadows at most, " + std::to_string(DarkAmongPuffs) + " dark points among the puffs",
		                   "none among the puffs; some on a cumulus day" });
		Limits.push_back({ "falls and parachutes end on the sea on a cumulus day", !bCumulus || (Show.CloudTop == 0.0 && FallsEndAloft == 0 && ChutesVanished == 0),
		                   std::to_string(FallsEndAloft) + " falls and " + std::to_string(ChutesVanished) + " parachutes end in the air (" + std::to_string(ChutesDown) + " parachutes down)",
		                   "0 and 0, with no deck top" });
	}

	// The sky's other days, laid round this battle's target with its own bands: a deck under the bombers (moved to 1.5
	// to 2.5 km under the lowest above 2,000 m, its thickness kept; over the lowest band where that is the band, and
	// between the two where both fly), a strike at ships with its hole, high ground, a ticket with no sky; and this
	// battle flown again under a deck, where falls begun above it end in it where a tile of it is drawn, and go on down
	// through its holes, past its reach, and from under it.
	{
		AirSky::Day Deck;
		Deck.bGiven = true;
		Deck.bDeck = true;
		Deck.Base = 2600.0;
		Deck.Top = 3600.0;
		const double Band = SkyAt.Band;
		const AirSky::Field Under = AirSky::Lay(Deck, SkyAt);
		Limits.push_back({ "a deck 1.5 to 2.5 km under the lowest bombers", Band > 2000.0 && Under.Top <= Band - 1500.0 + 0.5 && Under.Top >= Band - 2500.0 - 0.5 && std::fabs((Under.Top - Under.Base) - 1000.0) < 0.5 && Under.DeckTop == Under.Top,
		                   "band " + Num(Band, 0) + ", top " + Num(Under.Top, 0) + " m (moved " + Num(Under.Moved, 0) + "), " + Num(Under.Top - Under.Base, 0) + " m thick", "top 1,500 to 2,500 m under the band, 1,000 m thick" });
		Limits.push_back({ "a deck covers the sky (share under cloud)", Under.Cover >= 0.625, Num(Under.Cover, 2), "5/8 or more" });
		AirSky::Day Deep = Deck;
		Deep.Base = 600.0;
		Deep.Top = 1200.0;
		const AirSky::Field Raised = AirSky::Lay(Deep, SkyAt);
		Limits.push_back({ "a deck far under the bombers raised to them", Raised.Top >= Band - 2500.0 - 0.5 && Raised.Top <= Band - 1500.0 + 0.5 && std::fabs((Raised.Top - Raised.Base) - 600.0) < 0.5,
		                   "top " + Num(Raised.Top, 0) + " m (" + Raised.MovedWhy + "), " + Num(Raised.Top - Raised.Base, 0) + " m thick", "top 1,500 to 2,500 m under the band, 600 m thick" });
		AirSky::Day Low = Deck;
		Low.Base = 900.0;
		Low.Top = 1500.0;
		AirSky::Placing Lowest = SkyAt;
		Lowest.Band = 0.0;
		Lowest.LowBand = 1800.0;
		const AirSky::Field Over = AirSky::Lay(Low, Lowest);
		AirSky::Placing Mixed = SkyAt;
		Mixed.Band = 4500.0;
		Mixed.LowBand = 1800.0;
		const AirSky::Field Between = AirSky::Lay(Low, Mixed);
		Limits.push_back({ "the lowest band flies under its deck", Over.Base >= 1800.0 + 600.0 - 0.5 && Between.Base >= 2400.0 - 0.5 && Between.Top <= 3000.0 + 0.5 && Between.Top - Between.Base >= 299.5,
		                   "base " + Num(Over.Base, 0) + " m (" + Over.MovedWhy + "); with 4,500 m over it " + Num(Between.Base, 0) + " to " + Num(Between.Top, 0) + " m",
		                   "base at least 2,400 m; between, 2,400 to 3,000 m" });
		AirSky::Placing Ships = SkyAt;
		Ships.bShips = true;
		const AirSky::Field Hole = AirSky::Lay(Deck, Ships);
		// (the nearest puff drawn there with any body: a tenth of its alpha or more)
		double Nearest = 1e9;
		for (size_t K = 0; K < Hole.Clouds.size(); ++K)
		{
			const AirSky::Cloud& Cl = Hole.Clouds[K];
			const AirSky::Placed Pl = Hole.Place(static_cast<int>(K), Ships.TargetX, Ships.TargetY, AirSky::Vec());
			for (int J = Cl.First; J < Cl.First + Cl.Count; ++J)
			{
				const AirSky::Puff& Pf = Hole.Puffs[static_cast<size_t>(J)];
				if (Pl.Show * Pf.Alpha < 0.1 * Pf.Alpha) continue;
				Nearest = std::min(Nearest, std::hypot(Pf.At.X + Pl.Shift.X - Ships.TargetX, Pf.At.Y + Pl.Shift.Y - Ships.TargetY));
			}
		}
		const bool bHoleOpen = Hole.DeckAt(Ships.TargetX, Ships.TargetY) == 0.0 && Hole.DeckAt(Ships.TargetX + 1500.0, Ships.TargetY - 1500.0) == 0.0;
		Limits.push_back({ "a deck keeps a hole over the ships", Nearest >= 2200.0 && Hole.Cover >= 0.625 && bHoleOpen && Hole.DriftAt(3600.0).Length() == 0.0,
		                   Num(Nearest, 0) + " m to the nearest cloud, cover " + Num(Hole.Cover, 2) + (bHoleOpen ? ", open to falls" : ", closed to falls"), "at least 2,200 m, cover 5/8, open, never drifting" });
		const int DeckOutside = OutOfHeight(Under) + OutOfHeight(Raised) + OutOfHeight(Over) + OutOfHeight(Between) + OutOfHeight(Hole);
		Limits.push_back({ "deck tiles inside their own base and tops", DeckOutside == 0, std::to_string(DeckOutside) + " outside", "0" });

		// High ground: a broad mountain of 2,000 m 4 km off the target, a steep ridge of 1,000 m 6 km east (as steep as
		// Oahu's Koolau), a peak of 3,800 m 6 km north. Every puff's body clear of the ground under it, cloud lifted
		// over it, no deck tile within 1.5 km of the bombers, and the peak standing through the deck.
		const double TX = SkyAt.TargetX, TY = SkyAt.TargetY;
		const std::function<double(double, double)> Grounds[] = {
			[TX, TY](double X, double Y) { return 2000.0 * std::exp(-((X - TX - 4000.0) * (X - TX - 4000.0) + (Y - TY) * (Y - TY)) / (2.0 * 3000.0 * 3000.0)); },
			[TX](double X, double) { return 1000.0 * std::exp(-(X - TX - 6000.0) * (X - TX - 6000.0) / (2.0 * 700.0 * 700.0)); },
			[TX, TY](double X, double Y) { return 3800.0 * std::exp(-((X - TX) * (X - TX) + (Y - TY - 6000.0) * (Y - TY - 6000.0)) / (2.0 * 2500.0 * 2500.0)); },
		};
		const double Feature[3][2] = { { TX + 4000.0, TY }, { TX + 6000.0, TY }, { TX, TY + 6000.0 } };
		// (a field as the painter draws it, the wind carrying cumulus over the ground for 20 minutes (a deck: once),
		// painted whenever the painter asks to be: every drawn puff's heart ring 150 m over the highest ground under it,
		// its rim 30 m, each cloud's foot 300 m over the ground under its puffs' hearts, and no cloud's lift jumping more
		// than 8 m a second of air)
		struct Swept
		{
			int Buried = 0, Popped = 0, Paints = 0;
			double LeastHeart = 1e9, LeastRim = 1e9, LeastFoot = 1e9, MostRate = 0.0;
		};
		auto Sweep = [&SkyAt](const AirSky::Field& Laid, const std::function<double(double, double)>& Beneath, const AirSky::Vec& Eye, double Last, Swept& Out)
		{
			auto GroundAt = [&Beneath](double X, double Y) { return std::max(0.0, Beneath(X, Y)); };
			AirSky::Painter Pt;
			std::vector<AirSky::Vec> Pts;
			std::vector<uint8_t> Cols;
			std::vector<double> Before(Laid.Clouds.size(), -1.0), BeforeAir(Laid.Clouds.size(), 0.0);
			for (double Air = 0.0; Air <= Last + 1e-9; Air += 0.5)
			{
				const AirSky::Vec D = Laid.DriftAt(Air);
				if (!Pt.Stale(Eye, D)) continue;
				Pt.Paint(Laid, Eye, D, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Pts, Cols);
				++Out.Paints;
				const AirSky::Vec Seen = Eye - D;
				for (size_t I = 0; I < Laid.Clouds.size(); ++I)
				{
					const AirSky::Cloud& Cl = Laid.Clouds[I];
					const int Index = static_cast<int>(I);
					const double Shown = Pt.ShownLift(Index);
					const AirSky::Vec Shift = Laid.ShiftNear(Index, Seen.X, Seen.Y);
					const double WX = Cl.At.X + Shift.X + D.X, WY = Cl.At.Y + Shift.Y + D.Y;
					if (Shown < 0.0 || std::hypot(WX - Eye.X, WY - Eye.Y) > 9000.0)
					{
						Before[I] = -1.0;
						continue;
					}
					if (Before[I] >= 0.0)
					{
						const double Rate = std::fabs(Shown - Before[I]) / std::max(0.5, Air - BeforeAir[I]);
						Out.MostRate = std::max(Out.MostRate, Rate);
						if (Rate > 8.0) ++Out.Popped;
					}
					Before[I] = Shown;
					BeforeAir[I] = Air;
					const AirSky::Placed Pl = Laid.PlaceLifted(Index, Shift, D, Shown);
					if (Pl.Squash <= 0.0 || Pl.Show < 0.002) continue;
					double UnderHearts = 0.0;
					for (int J = Cl.First; J < Cl.First + Cl.Count; ++J)
					{
						const AirSky::Puff& Pf = Laid.Puffs[static_cast<size_t>(J)];
						const double PX = Pf.At.X + Shift.X + D.X, PY = Pf.At.Y + Shift.Y + D.Y, PZ = Cl.At.Z + Shown + (Pf.At.Z - Cl.At.Z) * Pl.Squash;
						const double Flat = std::max(0.08, Pf.Flat * Pl.Squash);
						double UnderHeart = GroundAt(PX, PY), UnderRim = UnderHeart;
						for (int K = 0; K < 8; ++K)
						{
							const double A = 2.0 * 3.14159265358979 * K / 8.0;
							UnderHeart = std::max(UnderHeart, GroundAt(PX + 0.55 * Pf.R * std::cos(A), PY + 0.55 * Pf.R * std::sin(A)));
							UnderRim = std::max(UnderRim, GroundAt(PX + Pf.R * std::cos(A), PY + Pf.R * std::sin(A)));
						}
						UnderRim = std::max(UnderRim, UnderHeart);
						UnderHearts = std::max(UnderHearts, UnderHeart);
						const double Heart = PZ - 0.55 * Pf.R * Flat - UnderHeart, Rim = PZ - Pf.R * Flat - UnderRim;
						Out.LeastHeart = std::min(Out.LeastHeart, Heart);
						Out.LeastRim = std::min(Out.LeastRim, Rim);
						if (Heart < 150.0 || Rim < 30.0) ++Out.Buried;
					}
					if (UnderHearts > 0.5) Out.LeastFoot = std::min(Out.LeastFoot, Cl.At.Z + Shown - UnderHearts);
				}
			}
		};
		auto SweptTold = [](const Swept& W, double Ms)
		{
			return std::to_string(W.Buried) + " puffs too low; heart ring " + Num(W.LeastHeart, 0) + " m, rim " + Num(W.LeastRim, 0) + " m, foot " + Num(W.LeastFoot, 0) + " m over the ground at least; lift " +
			       Num(W.MostRate, 1) + " m a second at most (" + std::to_string(W.Paints) + " paints, " + Num(Ms / 1000.0, 1) + " s)";
		};
		auto SweptOk = [](const Swept& W) { return W.Buried == 0 && W.LeastFoot >= 299.5 && W.Popped == 0; };
		int Lifted = 0, TooHigh = 0, Pierced = 0;
		Swept OverHills;
		bool bPeakOpen = true;
		const auto Sweep0 = std::chrono::steady_clock::now();
		for (int G = 0; G < 3; ++G)
		{
			AirSky::Placing Hills = SkyAt;
			Hills.Ground = Grounds[G];
			for (const AirSky::Day& Day : { Rec.Board.Cloud, Deck, Low })
			{
				const AirSky::Field Laid = AirSky::Lay(Day, Hills);
				// (each cloud where it is drawn by the high ground, as first laid: its copy nearest it, lifted and thinned)
				for (size_t I = 0; I < Laid.Clouds.size(); ++I)
				{
					const AirSky::Cloud& Cl = Laid.Clouds[I];
					const AirSky::Placed Pl = Laid.Place(static_cast<int>(I), Feature[G][0], Feature[G][1], AirSky::Vec());
					if (Pl.Squash <= 0.0)
					{
						++Pierced;
						continue;
					}
					if (Pl.Lift > 0.0) ++Lifted;
					const double Cap = std::max(Hills.Band - 1500.0, Laid.Top);
					if (Day.bDeck && Hills.Band > 2000.0 && Cl.At.Z + Pl.Lift + Pl.Squash * Cl.H > Cap + 0.5) ++TooHigh;
				}
				if (G == 2 && Day.bDeck && Laid.DeckAt(TX, TY + 6000.0) != 0.0) bPeakOpen = false;
				Sweep(Laid, Grounds[G], AirSky::Vec(Feature[G][0], Feature[G][1] - 4000.0, Laid.Top + 1000.0), Day.bDeck ? 0.0 : 1200.0, OverHills);
			}
		}
		const double SweepMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Sweep0).count();
		Limits.push_back({ "cloud lifted over high ground, the deck under the band", Lifted > 0 && TooHigh == 0 && Pierced > 0 && bPeakOpen,
		                   std::to_string(Lifted) + " lifted, " + std::to_string(TooHigh) + " tiles too high, " + std::to_string(Pierced) + " left out for the peak" + (bPeakOpen ? "" : ", the peak under cloud"),
		                   "some lifted, 0 too high, the peak through the deck" });
		Limits.push_back({ "drawn clear of high ground as the wind carries it", SweptOk(OverHills), SweptTold(OverHills, SweepMs), "heart 150, rim 30, foot 300 or more; at most 8 m a second" });

		// The same over Oahu's own ground, the stage's 150 m grid (Content/Data/Stages/oahu.json, as AAirStage reads it,
		// its aim on the target, unturned), with the wind carrying the day's cumulus over Kaala and over the Koolau's
		// highest; and the painter asked no more often over the island than over the sea, for an eye flying round the
		// harbour (its lift eases only as the wind carries it, whatever the eye does).
		{
			const std::string StagePath = Path.substr(0, Path.rfind("/AirBattles/") == std::string::npos ? 0 : Path.rfind("/AirBattles/")) + "/Stages/oahu.json";
			std::ifstream StageIn(StagePath);
			std::stringstream StageText;
			StageText << StageIn.rdbuf();
			AirFeed::Json StageJson;
			std::string StageWhy;
			const bool bStage = StageIn && AirFeed::Parse(StageText.str(), StageJson, StageWhy);
			const AirFeed::Json& Grid = StageJson["ground"];
			const double E0 = Grid["e0"].Num(), N0 = Grid["n0"].Num(), Cell = Grid["cell"].Num(150.0);
			const int Cols = Grid["cols"].Int(), Rows = Grid["rows"].Int();
			std::vector<double> Heights;
			for (size_t K = 0; K < Grid["z"].Count(); ++K) Heights.push_back(Grid["z"].At(K).Num());
			const bool bGrid = bStage && Cols > 0 && Rows > 0 && Heights.size() == static_cast<size_t>(Cols) * static_cast<size_t>(Rows);
			const std::function<double(double, double)> Oahu = [&Heights, E0, N0, Cell, Cols, Rows, TX, TY](double X, double Y)
			{
				const int Col = static_cast<int>(std::floor((X - TX - E0) / Cell)), Row = static_cast<int>(std::floor((Y - TY - N0) / Cell));
				if (Col < 0 || Row < 0 || Col >= Cols || Row >= Rows) return 0.0;
				return std::max(0.0, Heights[static_cast<size_t>(Row) * static_cast<size_t>(Cols) + static_cast<size_t>(Col)]);
			};
			// (Kaala, the island's highest, and the Koolau's highest east of the harbour)
			double KaalaX = TX, KaalaY = TY, KaalaZ = -1.0, KoolauX = TX, KoolauY = TY, KoolauZ = -1.0;
			for (int Row = 0; bGrid && Row < Rows; ++Row)
				for (int Col = 0; Col < Cols; ++Col)
				{
					const double Z = Heights[static_cast<size_t>(Row) * static_cast<size_t>(Cols) + static_cast<size_t>(Col)];
					const double X = TX + E0 + (Col + 0.5) * Cell, Y = TY + N0 + (Row + 0.5) * Cell;
					if (Z > KaalaZ) { KaalaZ = Z; KaalaX = X; KaalaY = Y; }
					if (X > TX + 2000.0 && Z > KoolauZ) { KoolauZ = Z; KoolauX = X; KoolauY = Y; }
				}
			AirSky::Placing OahuAt = SkyAt;
			OahuAt.Ground = Oahu;
			const AirSky::Field OverOahu = AirSky::Lay(Rec.Board.Cloud, OahuAt);
			Swept Island;
			const auto Island0 = std::chrono::steady_clock::now();
			if (bGrid)
			{
				Sweep(OverOahu, Oahu, AirSky::Vec(KaalaX, KaalaY - 4000.0, OverOahu.Top + 1000.0), 1200.0, Island);
				Sweep(OverOahu, Oahu, AirSky::Vec(KoolauX, KoolauY - 4000.0, OverOahu.Top + 1000.0), 1200.0, Island);
			}
			const double IslandMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Island0).count();
			Limits.push_back({ "drawn clear of Oahu's own ground as the wind carries it", bGrid && SweptOk(Island),
			                   bGrid ? "Kaala " + Num(KaalaZ, 0) + " m, Koolau " + Num(KoolauZ, 0) + " m: " + SweptTold(Island, IslandMs) : "no stage ground at " + StagePath,
			                   "heart 150, rim 30, foot 300 or more; at most 8 m a second" });
			// (an eye flying round the harbour 8 km out at 3,000 m and 120 m/s, a minute at 60 frames a second)
			const AirSky::Field OverSea = AirSky::Lay(Rec.Board.Cloud, SkyAt);
			auto PaintsFlying = [&SkyAt, TX, TY](const AirSky::Field& Laid)
			{
				AirSky::Painter Pt;
				std::vector<AirSky::Vec> Pts;
				std::vector<uint8_t> Colours;
				int Count = 0;
				for (int Frame = 0; Frame < 3600; ++Frame)
				{
					const double Secs = Frame / 60.0, A = Secs * 120.0 / 8000.0;
					const AirSky::Vec Eye(TX + 8000.0 * std::cos(A), TY + 8000.0 * std::sin(A), 3000.0);
					const AirSky::Vec D = Laid.DriftAt(Secs);
					if (!Pt.Stale(Eye, D)) continue;
					Pt.Paint(Laid, Eye, D, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Pts, Colours);
					++Count;
				}
				return Count;
			};
			const int PaintsIsland = bGrid ? PaintsFlying(OverOahu) : 0, PaintsSea = PaintsFlying(OverSea);
			Limits.push_back({ "the sky painted no more often over the island than the sea", bGrid && PaintsIsland <= PaintsSea + PaintsSea / 4 + 20,
			                   std::to_string(PaintsIsland) + " paints over Oahu, " + std::to_string(PaintsSea) + " over the sea, of 3,600 frames", "at most a quarter more over the island" });
			// The sky in two rooms along the same flights, and one low through the clouds (3 km out at the base and 150 m),
			// the plain painter beside it: how often the far room turns, and the points sent to the engine (a room's
			// slots, 33 points a puff, and the shadows' with the far room), against the whole sky's each paint.
			struct Flown { int Paints = 0, Near = 0, Far = 0; double Whole = 0.0, Sent = 0.0; };
			auto RoomsFlying = [&SkyAt, TX, TY](const AirSky::Field& Laid, double Out, double High)
			{
				AirSky::Painter Pt;
				AirSky::Rooms Two;
				std::vector<AirSky::Vec> Pts, Shades;
				std::vector<uint8_t> Colours, ShadeColours;
				Flown Got;
				const double Per = Pt.PerPuff(), Shadowed = AirSky::Painter::MostShadows * Per;
				for (int Frame = 0; Frame < 3600; ++Frame)
				{
					const double Secs = Frame / 60.0, A = Secs * 120.0 / Out;
					const AirSky::Vec Eye(TX + Out * std::cos(A), TY + Out * std::sin(A), High);
					const AirSky::Vec D = Laid.DriftAt(Secs);
					if (Pt.Stale(Eye, D))
					{
						Pt.Paint(Laid, Eye, D, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Pts, Colours, &Shades, &ShadeColours);
						++Got.Paints;
						Got.Whole += Pt.RoomFor(Laid) * Per + Shadowed;
					}
					const AirSky::Rooms::Sent What = Two.Turn(Laid, Eye, D, SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f));
					if (What == AirSky::Rooms::Sent::Near) { ++Got.Near; Got.Sent += Two.NearRoom() * Per; }
					if (What == AirSky::Rooms::Sent::Both) { ++Got.Far; Got.Sent += (Two.NearRoom() + Two.Far.RoomFor(Laid)) * Per + Shadowed; }
				}
				return Got;
			};
			const Flown HighSea = RoomsFlying(OverSea, 8000.0, 3000.0), LowSea = RoomsFlying(OverSea, 3000.0, OverSea.Base + 150.0);
			const Flown HighIsland = bGrid ? RoomsFlying(OverOahu, 8000.0, 3000.0) : Flown();
			auto FlownOk = [](const Flown& F) { return F.Far * 4 <= F.Paints + 8 && F.Sent * 2.0 <= F.Whole; };
			auto FlownTold = [](const Flown& F)
			{
				return std::to_string(F.Far) + " far turns and " + std::to_string(F.Near) + " near of the whole's " + std::to_string(F.Paints) + " paints, " +
				       Num(F.Sent / 60e3, 0) + "k points a second of " + Num(F.Whole / 60e3, 0) + "k";
			};
			Limits.push_back({ "the far clouds turned seldom, under half the points sent", FlownOk(HighSea) && FlownOk(LowSea) && (!bGrid || FlownOk(HighIsland)),
			                   "high " + FlownTold(HighSea) + "; low " + FlownTold(LowSea) + (bGrid ? "; over Oahu " + FlownTold(HighIsland) : std::string()),
			                   "far turns a quarter of the whole's paints or fewer, half its points or fewer, on each flight" });
		}
		AirFeed::Json Bare;
		std::string Why;
		AirFeed::Sky Old;
		const bool bRead = AirFeed::Parse("{\"place\":{\"kind\":\"city\",\"name\":\"Essen\"}}", Bare, Why) && AirFeed::ReadSky(Bare, Old);
		const AirSky::Field Plain = AirSky::Lay(Old.Cloud, AirFeed::PlacingOf(Old, {}, 0));
		Limits.push_back({ "a ticket with no sky draws cumulus", bRead && !Old.Cloud.bGiven && !Plain.Sky.bDeck && !Plain.Clouds.empty() && Plain.DeckTop == 0.0,
		                   bRead ? std::to_string(Plain.Clouds.size()) + " cumulus" : "unread", "cumulus, no deck" });
		// The after card's totals, each side's fighters and bombers lost (the rules' after.lost, afterLost: the sums of
		// the card's rows): read as the card gives them, and a card from before them summed from its rows the same way.
		{
			const char* Rows = "\"groups\":[{\"name\":\"A\",\"side\":\"att\",\"ftr\":40,\"bmr\":0,\"lostFtr\":3,\"lostBmr\":0},"
			                   "{\"name\":\"B\",\"side\":\"att\",\"ftr\":0,\"bmr\":60,\"lostFtr\":0,\"lostBmr\":9},"
			                   "{\"name\":\"C\",\"side\":\"def\",\"ftr\":50,\"bmr\":0,\"lostFtr\":7,\"lostBmr\":0}]";
			AirFeed::Json New, Older;
			AirFeed::After Card, OldCard;
			const bool bNew = AirFeed::Parse(std::string("{") + Rows + ",\"lost\":{\"att\":{\"ftr\":3,\"bmr\":9},\"def\":{\"ftr\":7,\"bmr\":0}}}", New, Why) && AirFeed::ReadAfter(New, Card);
			const bool bOld = AirFeed::Parse(std::string("{") + Rows + "}", Older, Why) && AirFeed::ReadAfter(Older, OldCard);
			auto Four = [](const AirFeed::After& A)
			{
				return std::to_string(A.LostAttFtr) + "/" + std::to_string(A.LostAttBmr) + "/" + std::to_string(A.LostDefFtr) + "/" + std::to_string(A.LostDefBmr);
			};
			const bool bOk = bNew && bOld && Four(Card) == "3/9/7/0" && Four(OldCard) == "3/9/7/0" && Card.Groups.size() == 3 && Card.Groups[1].Lost == 9;
			Limits.push_back({ "the after card's losses read, each side's fighters and bombers", bOk,
			                   (bNew ? "given " + Four(Card) : std::string("unread")) + "; " + (bOld ? "summed from the rows " + Four(OldCard) : std::string("unread")) + "; this battle's card " + Four(Rec.Card),
			                   "3/9/7/0 both ways (attacker's fighters/bombers, defender's)" });
		}

		// Every kind of day (Brian, 20 November: the weather different from battle to battle, "not always cloudy or some
		// days more cloudy"): this battle's sky laid again at each cover the rules can give, clear to closed. The share
		// of the whole laid square under the clouds' discs (nine tenths of their half-width, a 200 m grid) rises with the
		// cover; a clear day lays no cloud and paints nothing; a closed deck leaves no break wider than a tile's gap; and
		// the cloudiest of each kind fits the painter's room from the eyes over the target, high and low, none left out.
		{
			auto Whole = [](const AirSky::Field& Fd)
			{
				const int G = 220;
				const double Step = AirSky::Period / G;
				std::vector<uint8_t> Hit(static_cast<size_t>(G) * G, 0);
				for (const AirSky::Cloud& Cl : Fd.Clouds)
				{
					if (Cl.bScrap) continue;
					const double R = Cl.W * 0.45, LX = Cl.At.X - Fd.Origin.X, LY = Cl.At.Y - Fd.Origin.Y;
					for (int V = static_cast<int>(std::floor((LY - R) / Step)); V <= static_cast<int>(std::floor((LY + R) / Step)); ++V)
						for (int U = static_cast<int>(std::floor((LX - R) / Step)); U <= static_cast<int>(std::floor((LX + R) / Step)); ++U)
						{
							const double X = (U + 0.5) * Step - LX, Y = (V + 0.5) * Step - LY;
							if (X * X + Y * Y <= R * R) Hit[static_cast<size_t>(((V % G) + G) % G) * G + static_cast<size_t>(((U % G) + G) % G)] = 1;
						}
				}
				int N = 0;
				for (const uint8_t H : Hit) N += H;
				return static_cast<double>(N) / (static_cast<double>(G) * G);
			};
			struct Kind
			{
				bool bDeck;
				double Cover;
			};
			const Kind Kinds[] = { { false, 0.0 }, { false, 0.1 }, { false, 0.25 }, { false, 0.39 }, { false, 0.55 }, { true, 0.6 }, { true, 0.77 }, { true, 0.9 }, { true, 1.0 } };
			std::string Told;
			bool bRising = true, bClearEmpty = false, bClosed = false, bRoom = true;
			double Last = -1.0;
			int MostRoom = 0, Left = 0;
			for (const Kind& K : Kinds)
			{
				AirSky::Day Each = Rec.Board.Cloud;
				Each.bDeck = K.bDeck;
				Each.Cover = K.Cover;
				const AirSky::Field Fd = AirSky::Lay(Each, SkyHere);
				const double Got = Whole(Fd);
				if (K.bDeck && K.Cover == 0.6) Last = -1.0;
				bRising = bRising && Got > Last;
				Last = Got;
				if (!K.bDeck && K.Cover == 0.0)
				{
					AirSky::Painter Empty;
					std::vector<AirSky::Vec> P0, S0;
					std::vector<uint8_t> C0, SC0;
					Empty.Paint(Fd, AirSky::Vec(Rec.Board.TargetX, Rec.Board.TargetY, 4000.0), AirSky::Vec(), SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), P0, C0, &S0, &SC0);
					bClearEmpty = Fd.Clouds.empty() && Fd.Puffs.empty() && Empty.Drawn() == 0 && Empty.Dropped() == 0;
				}
				if (K.bDeck && K.Cover == 1.0) bClosed = Got >= 0.86;
				if (K.Cover == 0.55 || K.Cover == 1.0)
				{
					AirSky::Painter Room;
					std::vector<AirSky::Vec> Pr, Sr;
					std::vector<uint8_t> Cr, SCr;
					for (const double High : { 9000.0, 4000.0, Fd.Base + 150.0 })
					{
						Room.Paint(Fd, AirSky::Vec(Rec.Board.TargetX, Rec.Board.TargetY - 3000.0, High), AirSky::Vec(), SkyAt.Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Pr, Cr, &Sr, &SCr);
						MostRoom = std::max(MostRoom, Room.Drawn());
						Left = std::max(Left, Room.Dropped());
					}
					bRoom = bRoom && Left == 0;
				}
				Told += (Told.empty() ? "" : ", ") + std::string(K.bDeck ? "deck " : "") + Num(K.Cover, 2) + " " + Num(Got, 2);
			}
			Limits.push_back({ "every kind of day laid to its cover, clear to closed", bRising && bClearEmpty && bClosed && bRoom,
			                   Told + "; clear " + (bClearEmpty ? "empty" : "not empty") + "; " + std::to_string(MostRoom) + " puffs at most, " + std::to_string(Left) + " left out",
			                   "rising with the cover, a clear day empty, a closed deck 0.86 or more of the square, none left out" });
			// (the light under it: a cumulus day's sun whole and its sky the passed look's at three eighths; under a deck
			// the sun's share falling with the cover, about a third under Bremen's, at most a tenth under a closed one,
			// and whole over the deck's top)
			AirSky::Day Cu, Broken, Closed;
			Cu.Cover = AirSky::DefaultCumulus;
			Broken.bDeck = Closed.bDeck = true;
			Broken.Cover = 0.77;
			Closed.Cover = 1.0;
			const AirSky::Lighting LCu = AirSky::LightOf(Cu, 1.0), LBroken = AirSky::LightOf(Broken, 1.0), LClosed = AirSky::LightOf(Closed, 1.0), LOver = AirSky::LightOf(Closed, 0.0);
			Limits.push_back({ "the day's light under its cloud", LCu.Sun == 1.0 && std::fabs(LCu.Sky - 1.0) < 1e-9 && LBroken.Sun > 0.25 && LBroken.Sun < 0.45 && LClosed.Sun <= 0.1 && LClosed.Sun < LBroken.Sun && LClosed.Sky > LBroken.Sky && LOver.Sun == 1.0,
			                   "cumulus " + Num(LCu.Sun, 2) + "/" + Num(LCu.Sky, 2) + ", broken " + Num(LBroken.Sun, 2) + "/" + Num(LBroken.Sky, 2) + ", closed " + Num(LClosed.Sun, 2) + "/" + Num(LClosed.Sky, 2) + ", over it " + Num(LOver.Sun, 2),
			                   "cumulus 1/1, broken a third of the sun, closed a tenth or less, whole over the top" });
		}

		// This battle flown again under a deck, twice. First at 3,500 to 4,500 m, not a day's height but a test's (laid
		// for a band of 6,500 m), so that about half its losses begin above it and half under it, with its hole over the
		// middle of where they begin, over the sea. Then the lowest band's deck (laid 900 to 1,500 m, put at 2,400 to 3,000 m over a band of
		// 1,800 m) round the 3,800 m peak, whose tiles near it are lifted over the laid top. The eye is the main run's,
		// the deck drawn out to the scene's reach. Every end is judged against the drawn top puffs found tile by tile, not
		// through the deck's own lookup: an end in the deck lies in a drawn tile, between its foot and 150 m under its
		// top puff's middle (halfway, in a thin tile); and nothing that came out over a tile's foot is ever left falling,
		// shown, inside a drawn tile.
		struct Flown
		{
			int AboveFalls = 0, InsideFalls = 0, UnderFalls = 0, UnderLifted = 0, InDeck = 0, Through = 0, UnderDown = 0, OnHigh = 0, WrongEnd = 0, TimedOut = 0, WrongDeck = 0;
			int PiecesDown = 0, ShownInside = 0, FellInside = 0, Mismatch = 0, Lookups = 0, RowInClear = 0, RowsLetGo = 0;
			double Ms = 0.0;
		};
		const AirShow::Vec MainEye = Show.Eye;
		auto FlyUnder = [&](const AirSky::Field& Test, const std::function<double(double, double)>& Ground, const AirShow::Vec& Eye)
		{
			const auto Fly0 = std::chrono::steady_clock::now();
			Flown Out;
			AirShow::Show Decked(Rec.Seed ? Rec.Seed : 238);
			AirFeed::Apply(Rec.Board, Decked);
			Decked.Ground = Ground;
			Decked.CloudTop = Test.DeckTop;
			Decked.DeckAt = [&Test](double X, double Y)
			{
				AirShow::DeckSpan Here;
				Here.Hide = Test.DeckAt(X, Y, &Here.Foot);
				return Here;
			};
			Decked.DeckReach = AirSky::Reach * 5.0 / 6.0;
			Decked.DeckBase = Test.Base;
			Decked.Eye = Eye;
			auto Floor = [&Ground](double X, double Y) { return Ground ? std::max(0.0, Ground(X, Y)) : 0.0; };
			// (the deck over a point found tile by tile: the nearest tile whose top puff, at its copy nearest the point,
			// reaches over it within eight tenths of its reach and is drawn there, more than half shown)
			auto Drawn = [&](double X, double Y)
			{
				AirShow::DeckSpan Best;
				++Out.Lookups;
				if (std::hypot(X - Decked.Eye.X, Y - Decked.Eye.Y) > Decked.DeckReach) return Best;
				double BestFar = 1e18;
				for (size_t K = 0; K < Test.Clouds.size(); ++K)
				{
					const AirSky::Cloud& Cl = Test.Clouds[K];
					if (Cl.bScrap) continue; // (a scrap in the ships' hole is not the deck: falls pass through it)
					const AirSky::Puff& Top = Test.Puffs[static_cast<size_t>(Cl.First)];
					const AirSky::Vec Shift = Test.ShiftNear(static_cast<int>(K), X, Y);
					const double Ex = Top.At.X + Shift.X - X, Ey = Top.At.Y + Shift.Y - Y, Far = Ex * Ex + Ey * Ey;
					if (Far > 0.64 * Top.R * Top.R || Far >= BestFar) continue;
					const AirSky::Placed Pl = Test.Place(static_cast<int>(K), X, Y, AirSky::Vec());
					if (Pl.Squash <= 0.0 || Pl.Show < 0.5) continue;
					BestFar = Far;
					Best.Foot = Cl.At.Z + Pl.Lift;
					Best.Hide = Cl.At.Z + Pl.Lift + (Top.At.Z - Cl.At.Z) * Pl.Squash;
				}
				return Best;
			};
			auto Hides = [](const AirShow::DeckSpan& H) { return H.Hide - std::min(150.0, 0.5 * (H.Hide - H.Foot)); };
			// (a lookup is wanted only where a tile could be: from the laid base up to the highest a lift could raise one)
			const double LowestFoot = Test.Base - 1.0, HighestTop = Test.Top + (Ground ? 5000.0 : 1.0);
			struct Begun
			{
				bool bAbove = false, bRow = false;
				double Air = 0.0, RowZ = 0.0;
			};
			std::map<int, Begun> Begins;
			size_t Next = 0;
			for (double At = Rec.Views.front().Head.T; At < EndT - 1e-9;)
			{
				At = std::min(EndT, At + 1.0 / 30.0);
				while (Next < Rec.Views.size() && Rec.Views[Next].Head.T <= At + 1.0 + 1e-9)
				{
					Decked.Take(Rec.Views[Next].Head.T, Rec.Views[Next].Groups, Rec.Views[Next].Events, Rec.Views[Next].Ships);
					++Next;
				}
				Decked.Advance(At);
				for (const AirShow::Fx& E : Decked.Effects)
				{
					if (E.Kind == AirShow::FxKind::Fall)
					{
						const AirShow::DeckSpan Here = Drawn(E.At.X, E.At.Y);
						const bool bAbove = E.At.Z > Test.DeckTop;
						Out.AboveFalls += bAbove ? 1 : 0;
						Out.InsideFalls += !bAbove && E.At.Z > Test.Base ? 1 : 0;
						Out.UnderFalls += E.At.Z <= Test.Base ? 1 : 0;
						const bool bOver = Here.Hide > 0.0 && E.At.Z > Here.Foot;
						// (above the laid top, but under the foot of a tile lifted over high ground)
						Out.UnderLifted += bAbove && Here.Hide > 0.0 && !bOver ? 1 : 0;
						Begins[E.Key] = { bAbove, bOver, Decked.AirNow(), bOver ? std::min(Here.Hide, E.At.Z) : 0.0 };
						// (the kit's debris ends in the drawn tile over where the fall began, no higher than the fall, else
						// on the sea or the ground)
						const double Want = bOver ? std::min(Here.Hide, E.At.Z) : Floor(E.At.X, E.At.Y);
						if (std::fabs(E.Deck - Want) > 0.5) ++Out.WrongDeck;
					}
					if (E.Kind == AirShow::FxKind::FallStop)
					{
						const auto Found = Begins.find(E.Key);
						if (Found == Begins.end()) continue;
						if (E.What == "boom") { Begins.erase(Found); continue; } // (blown apart in the air: it ends there)
						const AirShow::DeckSpan Here = Drawn(E.At.X, E.At.Y);
						double Foot = 0.0;
						const double Hide = std::hypot(E.At.X - Decked.Eye.X, E.At.Y - Decked.Eye.Y) > Decked.DeckReach ? 0.0 : Test.DeckAt(E.At.X, E.At.Y, &Foot);
						if (std::fabs(Hide - Here.Hide) > 0.5 || (Hide > 0.0 && std::fabs(Foot - Here.Foot) > 0.5)) ++Out.Mismatch;
						const double Land = Floor(E.At.X, E.At.Y);
						const bool bDown = E.At.Z <= Land + 2.5;
						const bool bInCloud = Here.Hide > 0.0 && E.At.Z > Here.Foot - 0.5 && E.At.Z <= Hides(Here) + 0.5;
						if (bInCloud) ++Out.InDeck;
						else if (bDown && Found->second.bAbove) ++Out.Through;
						else if (bDown) ++Out.UnderDown;
						else if (Decked.AirNow() - Found->second.Air >= 399.0) ++Out.TimedOut;
						else ++Out.WrongEnd;
						if (bDown && Land > 2.5) ++Out.OnHigh;
						Begins.erase(Found);
					}
					if ((E.Kind == AirShow::FxKind::Crash || E.Kind == AirShow::FxKind::Splash) && E.What == "piece") ++Out.PiecesDown;
					// (the debris row sheds only over a drawn tile, above its foot and above the height it was told; let
					// go (Deck -1) once not; the move on which a fall ends in a tile aside)
					if (E.Kind == AirShow::FxKind::FallMove)
					{
						const auto Found = Begins.find(E.Key);
						if (Found == Begins.end() || !Found->second.bRow) continue;
						if (E.Deck < 0.0)
						{
							Found->second.bRow = false;
							++Out.RowsLetGo;
							continue;
						}
						const AirShow::DeckSpan Here = E.At.Z < LowestFoot ? AirShow::DeckSpan() : Drawn(E.At.X, E.At.Y);
						const bool bEnding = Here.Hide > 0.0 && E.At.Z > Here.Foot && E.At.Z < Hides(Here) - 0.5;
						if (!bEnding && (Here.Hide <= 0.0 || E.At.Z <= Here.Foot || E.At.Z < Found->second.RowZ - 0.5)) ++Out.RowInClear;
					}
				}
				// (no falling aircraft still falling inside a drawn tile, whatever it passed before: under one tile's foot it
				// may yet fall into a lower one beside it)
				for (const AirShow::PlaneOut& P : Decked.Planes())
				{
					if (!P.bFalling || !P.bShown || P.At.Z < LowestFoot || P.At.Z > HighestTop) continue; // (one blown apart is not drawn)
					const AirShow::DeckSpan Here = Drawn(P.At.X, P.At.Y);
					if (Here.Hide > 0.0 && P.At.Z > Here.Foot && P.At.Z < Hides(Here) - 0.5) ++Out.FellInside;
				}
				// (nor a piece or a parachute)
				for (const AirShow::Thing& Th : Decked.Things())
				{
					if (Th.Kind == AirShow::ThingKind::Bomb || Th.At.Z < LowestFoot || Th.At.Z > HighestTop) continue;
					const AirShow::DeckSpan Here = Drawn(Th.At.X, Th.At.Y);
					if (Here.Hide > 0.0 && Th.At.Z > Here.Foot && Th.At.Z < Hides(Here) - 1.0) ++Out.ShownInside;
				}
				Decked.Effects.clear();
			}
			Out.Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Fly0).count();
			return Out;
		};
		auto Told = [](const Flown& F)
		{
			return std::to_string(F.AboveFalls) + " begun above it (" + std::to_string(F.UnderLifted) + " under a lifted tile), " + std::to_string(F.InsideFalls) + " in its layer, " +
			       std::to_string(F.UnderFalls) + " under it; " + std::to_string(F.InDeck) + " end in it, " + std::to_string(F.Through) + " from above through a hole or past it, " +
			       std::to_string(F.UnderDown) + " from under it on the ground or sea, " + std::to_string(F.OnHigh) + " on high ground; " + std::to_string(F.WrongEnd) + " end in clear air, " +
			       std::to_string(F.FellInside) + " left falling in cloud, " + std::to_string(F.TimedOut) + " timed out, " + std::to_string(F.WrongDeck) + " wrong debris ends, " +
			       std::to_string(F.ShownInside) + " small things shown inside, " + std::to_string(F.Mismatch) + " lookups unlike the tiles; debris rows " + std::to_string(F.RowsLetGo) +
			       " let go, " + std::to_string(F.RowInClear) + " moves shedding in clear air; " + std::to_string(F.PiecesDown) + " pieces down (" +
			       Num(F.Ms / 1000.0, 1) + " s)";
		};
		// (where the main run's falls began: the middles of their places and heights)
		std::vector<double> FallXs, FallYs, FallZs;
		for (const AirSky::Vec& F : FallsAt)
		{
			FallXs.push_back(F.X);
			FallYs.push_back(F.Y);
		}
		for (const AirSky::Vec& F : FallsAt) FallZs.push_back(F.Z);
		std::sort(FallXs.begin(), FallXs.end());
		std::sort(FallYs.begin(), FallYs.end());
		std::sort(FallZs.begin(), FallZs.end());
		AirSky::Day TestDay = Deck;
		TestDay.Base = 3500.0;
		TestDay.Top = 4500.0;
		AirSky::Placing TestAt = SkyAt;
		TestAt.Band = 6500.0;
		TestAt.LowBand = 0.0;
		TestAt.bShips = true;
		// (its hole laid over the middle of where the falls began, not over the ships, so that some falls begun above
		// the deck go down through the hole and the rest end in its tiles round it, whatever the day's tiles happen to
		// leave open: on a deck day of eight tenths, twenty falls over the tiles could all end in them)
		if (!FallXs.empty())
		{
			TestAt.TargetX = FallXs[FallXs.size() / 2];
			TestAt.TargetY = FallYs[FallYs.size() / 2];
		}
		const AirSky::Field Test = AirSky::Lay(TestDay, TestAt);
		const Flown Sea = FlyUnder(Test, std::function<double(double, double)>(), MainEye);
		Limits.push_back({ "under a deck, falls begun above it end in it where drawn",
		                   Sea.WrongDeck == 0 && Sea.WrongEnd == 0 && Sea.FellInside == 0 && Sea.ShownInside == 0 && Sea.Mismatch == 0 && Sea.RowInClear == 0 && Sea.TimedOut <= 2 && Sea.InDeck > 0 && Sea.Through > 0 && (Sea.UnderFalls == 0 || Sea.UnderDown > 0),
		                   Told(Sea), "every end in a drawn tile or on the sea, some of each; none in clear air or left in cloud" });
		// (a high table of ground where the battle's falls are thickest, the middle of where they began, 150 m under
		// the middle of their heights, so that falls begin over it above the laid top but under the tiles lifted over
		// it; not a height of these islands but a test's; the eye on it)
		// (its middle 1.5 km east of theirs, so that its steep edge, where tiles side by side are lifted unlike, is
		// where falls are too)
		// (taken from the falls the peak can reach, those begun no higher than its 4.5 km cap and 150 m: a battle whose
		// fighters fight high, as Midway's patrols do when their staff climbs them over their own fleet, the tactics
		// panel's S1, still has falls begun under the tiles lifted over it; every fall, where none begins that low)
		// (and above the deck's laid top: only those can begin above it and under a tile lifted over the peak; not the
		// torpedo planes' falls at wave-top, nor those of the dive bombers' low run home)
		std::vector<double> PeakXs, PeakYs, PeakZs;
		for (const AirSky::Vec& F : FallsAt)
		{
			if (F.Z > 4650.0 || F.Z <= Over.DeckTop) continue;
			PeakXs.push_back(F.X);
			PeakYs.push_back(F.Y);
			PeakZs.push_back(F.Z);
		}
		if (PeakXs.empty())
		{
			PeakXs = FallXs;
			PeakYs = FallYs;
			PeakZs = FallZs;
		}
		std::sort(PeakXs.begin(), PeakXs.end());
		std::sort(PeakYs.begin(), PeakYs.end());
		std::sort(PeakZs.begin(), PeakZs.end());
		const double PX = (PeakXs.empty() ? TX : PeakXs[PeakXs.size() / 2]) + 1500.0, PY = PeakYs.empty() ? TY + 6000.0 : PeakYs[PeakYs.size() / 2];
		const double PH = PeakZs.empty() ? 3800.0 : std::min(4500.0, std::max(1000.0, PeakZs[PeakZs.size() / 2] - 150.0));
		const std::function<double(double, double)> Peak = [PX, PY, PH](double X, double Y)
		{
			const double Q = ((X - PX) * (X - PX) + (Y - PY) * (Y - PY)) / (2.0 * 3000.0 * 3000.0);
			return PH * std::exp(-Q * Q);
		};
		AirSky::Placing PeakAt = Lowest;
		PeakAt.Ground = Peak;
		const AirSky::Field Peaked = AirSky::Lay(Low, PeakAt);
		const Flown Hill = FlyUnder(Peaked, Peak, AirShow::Vec(PX, PY - 6000.0, 5000.0));
		Limits.push_back({ "the lowest band's deck lifted over high ground in the fight",
		                   Hill.WrongDeck == 0 && Hill.WrongEnd == 0 && Hill.FellInside == 0 && Hill.ShownInside == 0 && Hill.Mismatch == 0 && Hill.RowInClear == 0 && Hill.TimedOut <= 2 && Hill.UnderLifted > 0 && Hill.OnHigh > 0,
		                   Told(Hill), "every end in a drawn tile or on the ground, some under a lifted tile and on the peak; none in clear air or left in cloud" });
		// A step in the deck, sharper than any lift makes: west of the middle of where falls begin, a tile whose foot is
		// higher than any fall; east of it a thick tile low down. Every fall west of the step is under a tile's foot,
		// and one that drifts east must still end in the low tile: passing under one tile never lets a fall, a piece
		// or a parachute through a lower one beside it.
		{
			// (the step laid across the way the most of the main play's falls drifted east, halfway along one of them, so
			// that some cross it to be judged; then, if none crossed there, at the middle of where they began)
			const double Middle = FallXs.empty() ? TX : FallXs[FallXs.size() / 2];
			std::vector<double> Steps;
			int Across = 0;
			double Best = Middle;
			for (const auto& [From, To] : FallSpans)
			{
				if (To - From < 100.0) continue;
				const double At = 0.5 * (From + To);
				int N = 0;
				for (const auto& [From2, To2] : FallSpans) N += From2 < At - 25.0 && To2 > At + 25.0 ? 1 : 0;
				if (N > Across) { Across = N; Best = At; }
			}
			if (Across > 0) Steps.push_back(Best);
			Steps.push_back(Middle);
			const double LowFoot = 100.0, LowHide = 3000.0, LowEnds = LowHide - 150.0;
			int Crossed = 0, Stuck = 0, ThingsStuck = 0, Tried = 0;
			for (const double SX : Steps)
			{
				if (Crossed > 0) break;
				++Tried;
				AirShow::Show Stepped(Rec.Seed ? Rec.Seed : 238);
				AirFeed::Apply(Rec.Board, Stepped);
				Stepped.Eye = MainEye;
				Stepped.DeckAt = [SX, LowFoot, LowHide](double X, double)
				{
					AirShow::DeckSpan Here;
					Here.Foot = X < SX ? 20000.0 : LowFoot;
					Here.Hide = X < SX ? 20500.0 : LowHide;
					return Here;
				};
				Stepped.BaysOf = Show.BaysOf;
				std::map<int, bool> BegunWest;
				// (fed as the main play is fed, one view in hand ahead of what it shows, at its rate: the smooth flying
				// reads the path a little ahead of the picture, so a re-fly fed further ahead flies its own paths, and its
				// falls end away from the step laid on the main play's)
				size_t Next = 0;
				auto TakeStep = [&]()
				{
					const AirFeed::Frame& F = Rec.Views[Next++];
					Stepped.Take(F.Head.T, F.Groups, F.Events, F.Ships);
				};
				TakeStep();
				if (Next < Rec.Views.size()) TakeStep();
				for (double At = Rec.Views.front().Head.T; At < EndT - 1e-9;)
				{
					At = std::min(EndT, At + Dt);
					while (Next < Rec.Views.size() && Rec.Views[Next - 1].Head.T <= At + 1e-9) TakeStep();
					Stepped.Advance(At);
					for (const AirShow::Fx& E : Stepped.Effects)
					{
						if (E.Kind == AirShow::FxKind::Fall) BegunWest[E.Key] = E.At.X < SX;
						if (E.Kind != AirShow::FxKind::FallStop) continue;
						const auto Found = BegunWest.find(E.Key);
						if (Found == BegunWest.end()) continue;
						if (Found->second && E.At.X >= SX && E.At.Z > LowFoot && E.At.Z <= LowEnds + 0.5) ++Crossed;
						BegunWest.erase(Found);
					}
					for (const AirShow::PlaneOut& P : Stepped.Planes())
					{
						if (P.bFalling && P.bShown && P.At.X >= SX && P.At.Z > LowFoot && P.At.Z < LowEnds - 0.5) ++Stuck;
					}
					for (const AirShow::Thing& Th : Stepped.Things())
					{
						if (Th.Kind != AirShow::ThingKind::Bomb && Th.At.X >= SX && Th.At.Z > LowFoot && Th.At.Z < LowEnds - 1.0) ++ThingsStuck;
					}
					Stepped.Effects.clear();
				}
			}
			Limits.push_back({ "a fall under one tile's foot still ends in a lower one beside it",
			                   Crossed > 0 && Stuck == 0 && ThingsStuck == 0,
			                   std::to_string(Crossed) + " falls begun west of the step end in the low tile east of it (" + std::to_string(Tried) + " of " +
			                       std::to_string(Steps.size()) + " steps laid); " + std::to_string(Stuck) + " frames of a plane left falling in it, " +
			                       std::to_string(ThingsStuck) + " of a piece or parachute",
			                   "some cross and end there; none left falling inside" });
		}
	}

	int Failed = 0;
	for (const Limit& L : Limits)
	{
		std::printf("  %s %-58s %s (%s)\n", L.bOk ? "ok  " : "FAIL", L.Name, L.Got.c_str(), L.Want);
		if (!L.bOk) ++Failed;
	}
	// (each ship's guns as drawn, and every gun heard from)
	if (!Show.ShipTally().Kits.empty())
	{
		std::printf("  ships' guns (%04d-%02d):\n", Show.Year, Show.Month);
		for (const auto& [Name, Kit] : Show.ShipTally().Kits) std::printf("    %s: %s\n", Name.c_str(), Kit.c_str());
		std::string Fired;
		for (const std::string& G : Show.ShipTally().Fired) Fired += (Fired.empty() ? "" : ", ") + G;
		std::printf("    fired: %s\n", Fired.c_str());
	}

	// A live battle whose first view names no model (the bridge's opening view before the rules' mend): every group's
	// first view carries a wrong stand-in, marked as a guess. The show takes it, then the next view a battle second
	// ahead as the scene does, then draws: every aircraft, and every group as shown, must be its own model by then.
	if (Rec.Views.size() >= 2)
	{
		AirShow::Show Live(Rec.Seed ? Rec.Seed : 238);
		AirFeed::Apply(Rec.Board, Live);
		std::vector<AirShow::Group> First = Rec.Views[0].Groups;
		std::map<int, std::string> Own;
		for (const AirShow::Group& G : Rec.Views[1].Groups) Own[G.Id] = G.Model;
		for (AirShow::Group& G : First)
		{
			G.Model = G.bFighter ? "Bf109" : "B17";
			G.bModelGuessed = true;
		}
		Live.Take(Rec.Views[0].Head.T, First, Rec.Views[0].Events, Rec.Views[0].Ships);
		Live.Take(Rec.Views[1].Head.T, Rec.Views[1].Groups, Rec.Views[1].Events, Rec.Views[1].Ships);
		Live.Advance(Rec.Views[0].Head.T + 1.0 / Fps);
		int Wrong = 0, Planes = 0;
		for (const AirShow::PlaneOut& P : Live.Planes())
		{
			++Planes;
			Wrong += Own.count(P.Group) && Own[P.Group] != P.Model ? 1 : 0;
		}
		for (const auto& G : Live.Groups()) Wrong += Own.count(G.first) && Own[G.first] != G.second.Model ? 1 : 0;
		const Limit L{ "a live battle's guessed models made over by the next view", Wrong == 0 && Planes > 0, std::to_string(Wrong) + " wrong (of " + std::to_string(Planes) + " aircraft and their groups)", "0 wrong" };
		std::printf("  %s %-58s %s (%s)\n", L.bOk ? "ok  " : "FAIL", L.Name, L.Got.c_str(), L.Want);
		if (!L.bOk) ++Failed;
	}

	// The screen, as the player sees it: the battle on the screen's clock at x1 (real time) from the start to the end,
	// the words at a few moments, and the after card.
	{
		AirShow::Show Shown(Rec.Seed ? Rec.Seed : 238);
		AirFeed::Apply(Rec.Board, Shown);
		AirDesk::Desk Desk;
		Desk.Begin(Rec.Board, Rec.Views.front().Head.Me);
		// (the tug-of-war bar as the other side's player would see it: the same views read by a desk of his)
		AirDesk::Desk Other;
		const std::string OtherMe = Rec.Views.front().Head.Me == "def" ? "att" : "def";
		Other.Begin(Rec.Board, OtherMe);
		AirDesk::Clock Clock;
		size_t Next = 0;
		auto Feed = [&](double Upto)
		{
			while (Next < Rec.Views.size() && Rec.Views[Next].Head.T <= Upto + 1.0 + 1e-9)
			{
				Shown.Take(Rec.Views[Next].Head.T, Rec.Views[Next].Groups, Rec.Views[Next].Events, Rec.Views[Next].Ships);
				Desk.Take(Rec.Views[Next]);
				Other.Take(Rec.Views[Next]);
				++Next;
			}
		};
		Feed(0.0);
		double Battle = Rec.Views.front().Head.T, Real = 0.0, MetReal = -1.0;
		const double Frame = 1.0 / 60.0;
		std::string Tag;
		int Printed = 0;
		const double Marks[] = { 5.0, -1.0, 60.0, 180.0, 1e9 };
		AirDesk::Words W, WOther;
		// The tug-of-war bar: level until the first loss is read (or the battle is over); never a step (a frame moves it at most what the easing
		// allows); the other side's the mirror of his; and at the end it lies on the winner's side of the middle.
		double KnotMostStep = 0.0, KnotMirrorOff = 0.0, KnotBeforeLoss = 0.0;
		const std::map<int, AirShow::Group> NoGroups;
		auto KnotFrame = [&](double Step)
		{
			const double Before = W.Knot;
			Desk.Write(Step, Shown.Groups(), Tag, W);
			Other.Write(Step, NoGroups, Tag, WOther);
			KnotMostStep = std::max(KnotMostStep, std::fabs(W.Knot - Before));
			KnotMirrorOff = std::max(KnotMirrorOff, std::fabs(W.Knot + WOther.Knot - 1.0));
			// (level while the battle runs: once it is over the bar settles on the winner even where nothing was lost, as a
			// raid handed to the staff or turned back before contact is)
			if (!Desk.Head().bOver && Desk.Head().LostAtt + Desk.Head().LostDef == 0 && Desk.Head().Worth <= 0.0) KnotBeforeLoss = std::max(KnotBeforeLoss, std::fabs(W.Knot - 0.5));
		};
		while (Battle < EndT - 1e-9 && Real < 3600.0)
		{
			const double Run = Clock.Step(Frame, Desk.Head(), Rec.Board.Scale, Tag);
			Battle = std::min(EndT, Battle + Run);
			Real += Frame;
			Feed(Battle);
			Shown.Advance(Battle);
			Desk.Reach(Shown.Shown());
			Other.Reach(Shown.Shown());
			for (const AirShow::Fx& E : Shown.Effects)
				if (E.Kind == AirShow::FxKind::Dispatch) Desk.Say(E.What, E.Text, E.Side, E.Key);
			Shown.Effects.clear();
			KnotFrame(Frame);
			if (MetReal < 0.0 && Desk.Head().Contact >= 0.0) MetReal = Real;
			const bool bMark = (Printed == 0 && Real >= Marks[0]) || (Printed == 1 && MetReal >= 0.0 && Real >= MetReal + 1.0)
				|| (Printed == 2 && MetReal >= 0.0 && Real >= MetReal + Marks[2]) || (Printed == 3 && MetReal >= 0.0 && Real >= MetReal + Marks[3]);
			if (bMark)
			{
				++Printed;
				std::printf("\n  screen at %.0f real s (battle %s): [%s]\n    %s (%s) against %s (%s); %s; the bar's knot %.2f; bars %.2f / %.2f\n    THE AIM: %s / %s / %s\n",
				            Real, W.Clock.c_str(), W.ClockTag.c_str(), W.MySide.c_str(), W.MyNat.c_str(), W.TheirSide.c_str(), W.TheirNat.c_str(), W.Chance.c_str(), W.Knot,
				            W.MyBar, W.TheirBar, W.AimTarget.c_str(), W.AimState.c_str(), W.AimLost.c_str());
				if (!W.NearLine.empty()) std::printf("    near line: %s\n", W.NearLine.c_str());
				for (const AirDesk::Tag& Tg : W.Tags)
					std::printf("    tag %s%s: %s, %s%s%s\n", Tg.bMine ? "(yours) " : "", Tg.bFighter ? "fighters" : "bombers", Tg.Name.c_str(), Tg.Count.c_str(),
					            Tg.Doing.empty() ? "" : ", ", Tg.Doing.c_str());
				for (const AirDesk::Dispatch& D : W.Dispatches) std::printf("    dispatch (%.0f s): %s\n", D.Age, D.Text.c_str());
			}
		}
		std::printf("\n  on the screen's clock at x1: the formations meet %.0f real s in; the battle ends %.0f real s in (%.1f real minutes)\n",
		            MetReal, Real, Real / 60.0);
		{
			// (the last view read, then 6 real seconds for the knot to settle: the after card's wait. Release 1.5: the end
			// settles on the side the after card books, as the game's does once TC.battleEnd answers at the over line; the
			// other side's desk reads the same card as its mirror)
			Desk.Reach(EndT + 1.0);
			Other.Reach(EndT + 1.0);
			const std::string Stamp = Desk.Head().Winner;
			Desk.SettleFrom(Rec.Card);
			Other.Settle(Desk.Winner());
			if (Desk.Winner() != Stamp)
				std::printf("  the after card (%s) books the other side from the view's stamp (%s): the bar and the music follow the card\n",
				            Rec.Card.Level.c_str(), Stamp.empty() ? "none" : Stamp.c_str());
			for (int K = 0; K < 360; ++K) KnotFrame(Frame);
			const int Won = AirFeed::SideOf(Desk.Winner());
			const int MineIx = Desk.MySideIndex();
			const bool bEnds = !Desk.Head().bOver || Won < 0 || (Won == MineIx ? W.Knot > 0.5 + 0.9 * AirDesk::KnotSettle : W.Knot < 0.5 - 0.9 * AirDesk::KnotSettle);
			// (one frame's most: the whole bar's width eased over one frame of the lag)
			const double StepMost = 1.0 - std::exp(-Frame / AirDesk::KnotLag);
			char Got[200];
			std::snprintf(Got, sizeof Got, "level %.3f off before the first loss; most step %.4f a frame; the other side's off the mirror %.1e; ends %.2f, %s won",
			              KnotBeforeLoss, KnotMostStep, KnotMirrorOff, W.Knot, Won == MineIx ? "his side" : Won < 0 ? "no side" : "the other side");
			const Limit L{ "the tug-of-war bar: level, eased, mirrored, ends on the card's winner",
			               KnotBeforeLoss <= 1e-9 && KnotMostStep <= StepMost + 1e-9 && KnotMirrorOff <= 1e-9 && bEnds, Got, "0 before the first loss, steps within the easing, mirror within 1e-9, ends past the settle on the winner's side" };
			std::printf("  %s %-58s %s (%s)\n", L.bOk ? "ok  " : "FAIL", L.Name, L.Got.c_str(), L.Want);
			if (!L.bOk) ++Failed;
		}
		const AirDesk::Card Card = Desk.CardOf(Rec.Card);
		std::printf("  after card: %s / %s / %s\n", Card.Kicker.c_str(), Card.Title.c_str(), Card.Level.c_str());
		for (const std::string& L : Card.Lines) std::printf("    %s\n", L.c_str());
		for (const AirDesk::Card::Row& R : Card.Rows) std::printf("    %s%s: %s\n", R.bMine ? "(yours) " : "", R.Name.c_str(), R.Words.c_str());
		std::printf("  staff's question: %s\n", Desk.StaffQuestion().c_str());
		if (MetReal < 0.0 || Real >= 3600.0) { std::printf("  FAIL the screen's clock never reached the end\n"); ++Failed; }
	}

	std::printf("%s\n", Failed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failed ? 1 : 0;
}
