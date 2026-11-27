// The submarine scene's looks (Battle/ConvoyLooksCore), checked in the cloud: the boat's states and her dive and her
// coming up, never a pop, and the faint swirl over her deep where light falls; a depth charge's dome and column by depth
// and charge; a pattern going in where and when the show bursts it; the ring of ahead-thrown bombs; a starshell's fall
// and light; a crushed boat's boils; a boat blown riding high; a rescuer's scrambling nets where the sea scene's men climb
// her side. Plain C++, no engine.
// Usage: Tools/convoy_looks/run.sh   (CONVOY_LOOKS_STORY=1 prints a dive, the bursts by depth and a starshell). Given the
// game's source folder (run.sh gives it), it also checks the boat rows against the stage's model in its source, and the
// rescue against the sea scene's wreck model in its.

#include "Battle/ConvoyLooksCore.h"
#include "Battle/DeckCrewCore.h"
#include "Battle/Fates.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using namespace ConvoyLook;

	int Failures = 0, Checks = 0;
	bool bStory = false;

	void Check(bool bOk, const std::string& What)
	{
		++Checks;
		if (!bOk)
		{
			++Failures;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	std::string F(const char* Format, double A, double B = 0.0, double C = 0.0)
	{
		char Line[256];
		std::snprintf(Line, sizeof(Line), Format, A, B, C);
		return Line;
	}

	constexpr double Frame = 1.0 / 30.0;
	constexpr double Pi = 3.14159265358979323846;

	/**
	 * A boat as the sea scene's show gives her (SeaShow.cpp Show::Trim and Output, 2cfd75b8): her keel eased to the depth
	 * her state asks (a smoothstep over the time a dive takes: to periscope depth in 27 s, then 1 m/s deeper; up at 1.2
	 * m/s, and from periscope depth to the surface in 20 s), Dived her keel less her surfaced keel (a twentieth less of
	 * her class's length: 0.07 of it), her heave on the swell fading by e each 8 m under, her trim the swell's (fading
	 * alike) and down by the bow as she goes down, up as she comes up (atan of her keel's rate over her speed, at most 12
	 * degrees: Show::Advance). The stand-in convoy's header: class ss 67 m, subs{scope 13, deep 80}.
	 */
	struct ShowBoat
	{
		double Up = 67.0 * 0.07, ScopeKeel = 13.0, DeepKeel = 80.0;
		double Keel = -1.0, KeelTo = 0.0, KeelFrom = 0.0, KeelSince = 0.0, KeelFor = 0.0;
		double LastKeel = -1.0, LastT = 0.0, Sinking = 0.0;

		void Step(double T, const std::string& Dive, double Want = -1.0)
		{
			if (Keel >= 0.0)
			{
				LastKeel = Keel;
			}
			const double To = Want >= 0.0 ? Want : Dive == "deep" ? DeepKeel : Dive == "periscope" ? ScopeKeel : Up;
			if (Keel < 0.0)
			{
				Keel = KeelTo = KeelFrom = To;
			}
			if (std::fabs(To - KeelTo) > 0.05)
			{
				const double Lo = std::min(To, Keel), Hi = std::max(To, Keel);
				const double Shallow = std::max(0.0, std::min(Hi, ScopeKeel) - Lo);
				const bool bDown = To > Keel;
				KeelFrom = Keel;
				KeelTo = To;
				KeelSince = T;
				KeelFor = Shallow / std::max(1.0, ScopeKeel - Up) * (bDown ? 27.0 : 20.0) + (Hi - Lo - Shallow) / (bDown ? 1.0 : 1.2);
			}
			const double K = KeelFor > 0.0 ? std::clamp((T - KeelSince) / KeelFor, 0.0, 1.0) : 1.0;
			Keel = KeelFrom + (KeelTo - KeelFrom) * K * K * (3.0 - 2.0 * K);
			Sinking = LastKeel >= 0.0 && T > LastT ? (Keel - LastKeel) / (T - LastT) : 0.0;
			LastT = T;
		}
		double Dived() const { return std::max(0.0, Keel - Up); }
		double Calm() const { return std::exp(-std::max(0.0, Keel - Up) / 8.0); }
		double Pitch(double T, double Speed) const
		{
			return 2.5 * std::sin(T * 0.45) * Calm() + std::clamp(-std::atan(Sinking / std::max(1.5, Speed)) / Pi * 180.0, -12.0, 12.0);
		}
	};

	/** A boat's run, frames at 30 a second: surfaced to 20 s, a crash dive to periscope depth, her periscope up from 60
	 * to 120 s, deep from 130 s, up to the surface from 210 s. */
	struct Run
	{
		double MostStep = 0.0;       // the most her drawn heave moved in one frame (metres)
		double MostVisibleStep = 0.0; // the same while any of her was within HiddenUnder of the sea
		int Feathers = 0, Washes = 0, Swirls = 0, Bows = 0, Vents = 0;
		std::vector<double> DiveWash, UpWash; // where along her each wash was, diving and coming up
		double DeckSurfaced = 99.0, TowerAtPeriscope = -99.0, TopAtDeep = -99.0;
		double ScopeUpLeast = 99.0, ScopeUpMost = -99.0, LowUpMost = -99.0; // her periscopes' heads, the show's periscope up
		double ScopeDownMost = -99.0; // her taller periscope's head, at periscope depth with it down
		double LeftSurface = -1.0, AtPeriscope = -1.0;
		double SwirlBridge = 99.0; // her bridge's highest point (as drawn) when the swirl came
		bool bFeatherWithoutCut = false;
		double MostTrimLift = 0.0; // the most her trim lifted the bridge's highest point over its top amidships
		int DeepSwirls = 0, ShallowSwirls = 0;
		double MostSwirl = 0.0;
	};

	/** The washes' places along her (plus: forward), in their order: the most forward comes before the most aft. */
	bool BowFirst(const std::vector<double>& Along)
	{
		const auto Fore = std::max_element(Along.begin(), Along.end());
		const auto Aft = std::min_element(Along.begin(), Along.end());
		return Fore < Aft && *Fore - *Aft > 10.0;
	}

	Run Steam(const BoatModel& M, const Drawn& D, bool bJumpKeel, double Lit)
	{
		Run R;
		BoatState S;
		ShowBoat Show;
		double Last = 0.0;
		bool bFirst = true;
		const double Heading = 30.0;
		double SurfMark = 0.0, ScopeMark = 0.0;
		for (double T = 0.0; T < 300.0; T += Frame)
		{
			std::string Dive = "surfaced";
			if (T >= 20.0 && T < 130.0) Dive = "periscope";
			else if (T >= 130.0 && T < 210.0) Dive = "deep";
			Show.Step(T, Dive, bJumpKeel && T >= 100.0 && T < 101.0 ? 40.0 : -1.0);
			BoatIn In;
			In.T = T;
			In.Dive = Dive;
			In.Keel = Show.Keel;
			In.Dived = Show.Dived();
			In.bScope = T >= 60.0 && T < 120.0;
			In.Speed = Dive == "surfaced" ? 3.5 : 2.5;
			In.Pitch = Show.Pitch(T, In.Speed);
			In.Heading = Heading;
			In.At = P3(100.0 + In.Speed * T * std::sin(Heading * Pi / 180.0), 200.0 + In.Speed * T * std::cos(Heading * Pi / 180.0), 0.0);
			In.Heave = 0.6 * std::sin(T * 0.7) * Show.Calm();
			In.Lit = Lit;
			const BoatOut O = StepBoat(S, In, M, D);
			const double Top = O.Heave + M.Top * D.Up;
			if (!bFirst)
			{
				R.MostStep = std::max(R.MostStep, std::fabs(O.Heave - Last));
				if (std::max(Top, Last + M.Top * D.Up) > -6.0)
				{
					R.MostVisibleStep = std::max(R.MostVisibleStep, std::fabs(O.Heave - Last));
				}
			}
			bFirst = false;
			Last = O.Heave;
			if (T < 20.0)
			{
				R.DeckSurfaced = O.Heave - In.Heave + M.Deck * D.Up;
				SurfMark = O.Heave - In.Heave;
			}
			R.MostTrimLift = std::max(R.MostTrimLift, O.BridgeZ - (O.Heave + M.TowerTop * D.Up));
			if (T > 100.0 && T < 115.0 && !bJumpKeel)
			{
				ScopeMark = O.Heave - In.Heave;
				R.ScopeUpLeast = std::min(R.ScopeUpLeast, O.ScopeZ);
				R.ScopeUpMost = std::max(R.ScopeUpMost, O.ScopeZ);
				R.LowUpMost = std::max(R.LowUpMost, O.LowZ);
				R.TowerAtPeriscope = std::max(R.TowerAtPeriscope, O.BridgeZ);
			}
			if ((T > 50.0 && T < 60.0) || (T > 125.0 && T < 130.0))
			{
				R.ScopeDownMost = std::max(R.ScopeDownMost, O.ScopeZ);
			}
			if (T > 200.0 && T < 210.0)
			{
				R.TopAtDeep = std::max(R.TopAtDeep, std::max(Top, O.ScopeZ));
			}
			if (T >= 20.0 && R.LeftSurface < 0.0 && O.Heave - In.Heave < SurfMark - 0.2)
			{
				R.LeftSurface = T;
			}
			for (const BoatSplash& Sp : O.Splashes)
			{
				const double Along = (Sp.At.X - In.At.X) * std::sin(Heading * Pi / 180.0) + (Sp.At.Y - In.At.Y) * std::cos(Heading * Pi / 180.0);
				switch (Sp.Kind)
				{
				case SplashKind::Feather:
					++R.Feathers;
					// (the whole bridge under as drawn, her trim counted, and her taller periscope's head out; never deep)
					if (!O.bTowerUnder || O.BridgeZ >= 0.0 || O.ScopeZ <= 0.0 || Dive == "deep")
					{
						R.bFeatherWithoutCut = true;
					}
					break;
				case SplashKind::Wash:
					(T < 100.0 ? R.DiveWash : R.UpWash).push_back(Along);
					++R.Washes;
					break;
				case SplashKind::Swirl:
					++R.Swirls;
					R.SwirlBridge = O.BridgeZ;
					break;
				case SplashKind::Bow:
					++R.Bows;
					break;
				case SplashKind::Vent:
				case SplashKind::Scuttle:
				case SplashKind::Puff:
					// (only from her vents opened once she has struck, and her scuttling charges after: never on a dive or a surfacing)
					++R.Vents;
					break;
				}
			}
			for (const Swirl& W : S.Swirls)
			{
				if (W.T0 > T - Frame * 0.5)
				{
					(Dive == "deep" ? R.DeepSwirls : R.ShallowSwirls) += 1;
					R.MostSwirl = std::max(R.MostSwirl, W.Strength);
				}
			}
			if (bStory && !bJumpKeel && Lit == 0.0 && std::fmod(T + 1e-9, 10.0) < Frame)
			{
				std::printf("  %-13s t %5.1f %-9s keel %5.1f dived %5.1f heave %6.2f (show's %6.2f) trim %5.1f bridge %6.2f scope %s head %6.2f low %6.2f feather %.2f\n",
				            M.Name.c_str(), T, Dive.c_str(), Show.Keel, Show.Dived(), O.Heave, In.Heave - Show.Dived(), In.Pitch, O.BridgeZ, O.bScopeShown ? "out" : "--", O.ScopeZ,
				            O.LowZ, O.Feather);
			}
		}
		(void)ScopeMark;
		return R;
	}

	void TestBoats()
	{
		// (the code-built boats, which the stage fits to AirFeed::HullOf("ss"), 95 by 8.3 m; and the battle modeller's, which
		// it draws as made)
		struct Boat
		{
			BoatModel M;
			Drawn D;
		};
		std::vector<Boat> All;
		for (const char* Nation : { "GER", "USA", "UK" })
		{
			All.push_back({ ModelOf(Nation), DrawnOf(ModelOf(Nation), 95.0, 8.3) });
		}
		for (const char* File : { "Submarine_GER", "Submarine_USA", "Submarine" })
		{
			All.push_back({ SculptedOf(File), Drawn() });
		}
		for (const Boat& Each : All)
		{
			const BoatModel& M = Each.M;
			const Drawn& D = Each.D;
			const std::string N = M.Name + ": ";
			const Run R = Steam(M, D, false, 0.0);
			Check(R.MostVisibleStep <= 2.5 * Frame + 1e-9, N + F("no pop: the most her heave moved in a frame while any of her could be seen, %.3f m", R.MostVisibleStep));
			Check(std::fabs(R.DeckSurfaced - 0.5) < 1e-6, N + F("surfaced: her casing awash, its deck 0.5 m over the still sea: %.2f m", R.DeckSurfaced));
			Check(R.TowerAtPeriscope < -0.5, N + F("at periscope depth (13 m keel), all of her bridge under as drawn: at most %.2f m", R.TowerAtPeriscope));
			Check(R.ScopeUpLeast > 0.7 && R.ScopeUpMost < 2.2, N + F("the show's periscope up: her taller periscope's head 0.7 to 2.2 m out (%.2f to %.2f m)", R.ScopeUpLeast, R.ScopeUpMost));
			Check(R.LowUpMost < 0.0, N + F("the show's periscope up: her shorter one's head stays under (at most %.2f m)", R.LowUpMost));
			Check(R.ScopeDownMost < -0.4, N + F("at periscope depth with the show's periscope down, nothing of her shows: her highest point at most %.2f m", R.ScopeDownMost));
			Check(R.TopAtDeep < -20.0, N + F("deep (80 m), nothing of her within 20 m of the surface: %.2f m", R.TopAtDeep));
			Check(R.Feathers > 40, N + F("a feather while her periscope cuts the sea under way: %.0f spurts", R.Feathers));
			Check(!R.bFeatherWithoutCut, N + "no feather unless her periscope alone cuts the sea (all of her bridge under, as drawn)");
			Check(R.Swirls == 1, N + F("one swirl, where her bridge went under on the dive: %.0f", R.Swirls));
			Check(R.SwirlBridge < 0.0 && R.SwirlBridge > -0.15, N + F("the swirl comes as the last of her bridge goes under, as drawn: %.3f m", R.SwirlBridge));
			Check(R.MostTrimLift > 0.5, N + F("going down by the bow, her trim lifts the bridge's after end over its top amidships (by %.2f m), and the looks count it", R.MostTrimLift));
			Check(R.Washes >= 3 && R.Washes <= 12, N + F("water off her casing going down and coming up: %.0f churns", R.Washes));
			Check(R.DiveWash.size() >= 2 && BowFirst(R.DiveWash), N + "on the dive the wash runs from the bow aft");
			Check(R.UpWash.size() >= 2 && BowFirst(R.UpWash), N + "coming up, the bow breaks out first in its foam");
			Check(R.DeepSwirls == 0 && R.ShallowSwirls == 0, N + "in the dark, no swirl over her deep");
			Check(R.Vents == 0, N + F("no air from her vents and no scuttling charge on a dive or a surfacing: %.0f", R.Vents));
			const Run Lit = Steam(M, D, false, 0.6);
			Check(Lit.DeepSwirls >= 10, N + F("deep under a starshell's light, a faint swirl now and then: %.0f", Lit.DeepSwirls));
			Check(Lit.ShallowSwirls == 0, N + "no such swirl surfaced or at periscope depth");
			Check(Lit.MostSwirl <= 0.6 && Lit.MostSwirl > 0.05, N + F("the swirl no stronger than the light on it: %.2f", Lit.MostSwirl));
			const Run J = Steam(M, D, true, 0.0);
			Check(J.MostVisibleStep <= 2.5 * Frame + 1e-9, N + F("a jump in the show's keel is eased, never a pop: %.3f m a frame", J.MostVisibleStep));
		}
		{
			// the dive takes seconds: from leaving her surfaced trim to her periscope depth's mark, as the show eases it
			const BoatModel M = ModelOf("GER");
			const Drawn D = DrawnOf(M, 95.0, 8.3);
			std::vector<double> Heaves;
			BoatState S;
			ShowBoat Show;
			for (double T = 0.0; T < 80.0; T += Frame)
			{
				const std::string Dive = T >= 10.0 ? "periscope" : "surfaced";
				Show.Step(T, Dive);
				BoatIn In;
				In.T = T;
				In.Dive = Dive;
				In.Keel = Show.Keel;
				In.Dived = Show.Dived();
				Heaves.push_back(StepBoat(S, In, M, D).Heave);
			}
			double Left = -1.0, Under = -1.0;
			for (size_t I = 0; I < Heaves.size(); ++I)
			{
				const double T = static_cast<double>(I) * Frame;
				if (Left < 0.0 && Heaves[I] < Heaves.front() - 0.2) Left = T;
				if (Under < 0.0 && Heaves[I] < Heaves.back() + 0.2) Under = T;
			}
			Check(Left >= 10.0 && Under - Left >= 15.0 && Under - Left <= 35.0, F("a crash dive over seconds, never a pop: under in %.1f s (from %.1f s)", Under - Left, Left));
		}
		{
			// surfaced on the first frame: her waterline rides the swell, low enough that her casing is awash
			const BoatModel M = ModelOf("GER");
			const Drawn D = DrawnOf(M, 95.0, 8.3);
			BoatState S;
			BoatIn In;
			In.Keel = 4.69;
			In.Dive = "surfaced";
			In.Heave = 0.5;
			const BoatOut O = StepBoat(S, In, M, D);
			Check(std::fabs(O.Heave - (0.5 - (M.Deck * D.Up - 0.5))) < 1e-9, F("surfaced: the swell's heave less her trim awash: %.3f", O.Heave));
			Check(!O.bScopeShown && O.BridgeZ > 5.0 && O.Splashes.empty(), "surfaced: her bridge up, no periscope alone, no splash on the first frame");
		}
		{
			// the show's very first frame has her untrimmed (no keel), the next at periscope depth: she is there at once
			const BoatModel M = ModelOf(std::string());
			const Drawn D = DrawnOf(M, 95.0, 8.3);
			BoatState S;
			BoatIn In;
			In.Dive = "surfaced";
			In.Keel = 0.0;
			const BoatOut First = StepBoat(S, In, M, D);
			Check(!S.bBegun && First.Splashes.empty(), "an untrimmed first frame (no keel) is not her first: nothing kept, no splash");
			In.T = Frame;
			In.Dive = "periscope";
			In.Keel = 13.0;
			In.Dived = 13.0 - 4.69;
			In.bScope = true;
			const BoatOut Next = StepBoat(S, In, M, D);
			Check(Next.bScopeShown && Next.ScopeZ > 0.7 && Next.LowZ < 0.0 && Next.BridgeZ < -1.0,
			      F("the battle opens on her at periscope depth, periscope up: there at once (bridge %.2f, periscope head %.2f, the shorter's %.2f)", Next.BridgeZ, Next.ScopeZ, Next.LowZ));
		}
		{
			// fast on the surface in a swell: spray off her bow as it drops, no more than every 1.5 s
			const BoatModel M = ModelOf("GER");
			const Drawn D = DrawnOf(M, 95.0, 8.3);
			BoatState S;
			int Bows = 0, Slow = 0;
			for (double T = 0.0; T < 30.0; T += Frame)
			{
				BoatIn In;
				In.T = T;
				In.Keel = 4.69;
				In.Dive = "surfaced";
				In.Speed = 8.5;
				In.Heave = 1.2 * std::sin(T * 0.9);
				for (const BoatSplash& Sp : StepBoat(S, In, M, D).Splashes)
				{
					Bows += Sp.Kind == SplashKind::Bow ? 1 : 0;
				}
			}
			BoatState S3;
			for (double T = 0.0; T < 30.0; T += Frame)
			{
				BoatIn In;
				In.T = T;
				In.Keel = 4.69;
				In.Dive = "surfaced";
				In.Speed = 2.0;
				In.Heave = 1.2 * std::sin(T * 0.9);
				Slow += static_cast<int>(StepBoat(S3, In, M, D).Splashes.size());
			}
			Check(Bows >= 4 && Bows <= 20, F("surfaced at 17 knots into a swell: spray off her bow, %.0f times in 30 s", Bows));
			Check(Slow == 0, "surfaced and slow: no spray");
		}
		{
			// forced up (scuttled): from 80 m to her surfaced trim, whatever the show still says, never a pop once in sight
			const BoatModel M = ModelOf("GER");
			const Drawn D = DrawnOf(M, 95.0, 8.3);
			BoatState S;
			double Heave = -99.0, Last = 0.0, Most = 0.0, UpAt = -1.0;
			bool bFirst = true;
			for (double T = 0.0; T < 40.0; T += Frame)
			{
				BoatIn In;
				In.T = T;
				In.Keel = 80.0;
				In.Dived = 80.0 - 4.69;
				In.Dive = "deep";
				if (T >= 5.0)
				{
					ForceUp(S, 5.0, In.Dived);
				}
				Heave = StepBoat(S, In, M, D).Heave;
				if (!bFirst && Heave + M.Top * D.Up > -6.0) Most = std::max(Most, std::fabs(Heave - Last));
				if (UpAt < 0.0 && std::fabs(Heave + (M.Deck * D.Up - 0.5)) < 0.05) UpAt = T;
				bFirst = false;
				Last = Heave;
			}
			Check(std::fabs(Heave + (M.Deck * D.Up - 0.5)) < 0.05, F("forced up, she lies awash on the surface: heave %.2f", Heave));
			Check(UpAt > 5.0 + ForcedUpFor() * 0.8 && UpAt < 5.0 + ForcedUpFor() + 8.0, F("forced up in about 12 s: on the surface %.1f s after", UpAt - 5.0));
			Check(Most <= 2.5 * Frame + 1e-9, F("forced up, never a pop once in sight: %.3f m a frame", Most));
		}
		{
			// forced up with her tanks blown (the show's surfacing "forced", or her scuttling: Blow), she comes up riding high,
			// her model's own waterline at the sea, her deck where the sea scene's men go over it; never a pop
			const BoatModel M = ModelOf("GER");
			const Drawn D = DrawnOf(M, 95.0, 8.3);
			BoatState S;
			double Deck = -99.0, Last = 0.0, Most = 0.0;
			bool bFirst = true;
			for (double T = 0.0; T < 40.0; T += Frame)
			{
				BoatIn In;
				In.T = T;
				In.Keel = 80.0;
				In.Dived = 80.0 - 4.69;
				In.Dive = "deep";
				if (T >= 5.0)
				{
					ForceUp(S, 5.0, In.Dived);
					Blow(S, true);
				}
				const BoatOut Out = StepBoat(S, In, M, D);
				Deck = Out.DeckZ;
				if (!bFirst && Out.Heave + M.Top * D.Up > -6.0) Most = std::max(Most, std::fabs(Out.Heave - Last));
				bFirst = false;
				Last = Out.Heave;
			}
			Check(std::fabs(Deck - M.Deck * D.Up) < 0.02, F("blown, she rides high: her deck %.2f m out (her model's own, %.2f)", Deck, M.Deck * D.Up));
			Check(Most <= 2.5 * Frame + 1e-9, F("blown and forced up, never a pop once in sight: %.3f m a frame", Most));
		}
		{
			// surfaced awash, then her tanks blown (a scuttling with no forced surfacing told), then flooded again: eased
			// over BlowFor each way, never faster than a metre a second
			const BoatModel M = ModelOf("GER");
			const Drawn D = DrawnOf(M, 95.0, 8.3);
			BoatState S;
			double Awash = 0.0, High = 0.0, Back = 0.0, Most = 0.0, Last = 0.0, HighAt = -1.0;
			bool bFirst = true;
			for (double T = 0.0; T < 30.0; T += Frame)
			{
				BoatIn In;
				In.T = T;
				In.Keel = 4.69;
				In.Dive = "surfaced";
				if (std::fabs(T - 5.0) < 0.5 * Frame) Blow(S, true);
				if (std::fabs(T - 15.0) < 0.5 * Frame) Blow(S, false);
				const BoatOut Out = StepBoat(S, In, M, D);
				if (T < 5.0) Awash = Out.DeckZ;
				if (T > 5.0 && T < 15.0) High = Out.DeckZ;
				if (HighAt < 0.0 && Out.DeckZ > M.Deck * D.Up - 0.01) HighAt = T;
				Back = Out.DeckZ;
				if (!bFirst) Most = std::max(Most, std::fabs(Out.DeckZ - Last) / Frame);
				bFirst = false;
				Last = Out.DeckZ;
			}
			Check(std::fabs(Awash - 0.5) < 0.01, F("surfaced, her deck awash, %.2f m out", Awash));
			Check(std::fabs(High - M.Deck * D.Up) < 0.01 && HighAt > 5.0 && HighAt <= 5.0 + BlowFor() + 0.1,
			      F("her tanks blown, she rides high (%.2f m) in %.1f s", High, HighAt - 5.0));
			Check(std::fabs(Back - 0.5) < 0.01, F("flooded again, back awash (%.2f m)", Back));
			Check(Most <= 1.0, F("riding up or settling, at most %.2f m a second", Most));
		}
		{
			// a swirl's life
			Swirl W;
			W.T0 = 10.0;
			W.Strength = 0.5;
			double R0 = 0.0, S0 = 0.0, R1 = 0.0, S1 = 0.0, R2 = 0.0, S2 = 0.0;
			Check(!SwirlAt(W, 9.0, R0, S0), "no swirl before it is made");
			Check(SwirlAt(W, 10.5, R0, S0) && SwirlAt(W, 13.0, R1, S1) && SwirlAt(W, 18.5, R2, S2), "a swirl lasts its life");
			Check(R0 < R1 && R1 < R2 && R2 <= 6.5 + 1e-9, F("it opens out, to 13 m across: %.1f, %.1f, %.1f m", R0, R1, R2));
			Check(S1 > S0 && S1 > S2 && S1 <= 0.5 + 1e-9, "it shows, then fades, no stronger than it was made");
			Check(!SwirlAt(W, 10.0 + SwirlLife() + 0.1, R0, S0), "gone after its life");
		}
	}

	/** A file's text, empty where it cannot be read. */
	std::string TextOf(const std::string& Path)
	{
		std::ifstream File(Path);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}

	/**
	 * The boat rows against the model the stage draws: AirStage lays "Submarine:<navy>"; with no sculpted Submarine among
	 * the game's models, the kit builds her in code (Campaign/CampaignShips.cpp, Submarine()), and FitHull stretches her
	 * to AirFeed::HullOf's 95 by 8.3 m. Given the game's source folder, its text is read: a change to that model, a
	 * sculpted one put in, or another size fails here, so the rows are mended with it.
	 */
	void TestModelRows(const std::string& Root)
	{
		struct Row
		{
			const char* Nation;
			double Hull, Beam;
		};
		for (const Row& W : { Row{ "GER", 67.0, 6.2 }, Row{ "", 67.0, 6.2 }, Row{ "USA", 95.0, 8.3 }, Row{ "UK", 80.0, 6.2 }, Row{ "JAP", 80.0, 6.2 } })
		{
			const BoatModel M = ModelOf(W.Nation);
			const Drawn D = DrawnOf(M, 95.0, 8.3);
			const std::string N = std::string("[") + W.Nation + "] ";
			Check(std::fabs(M.Long - (W.Hull + 2.0)) < 1e-9 && std::fabs(M.Wide - W.Beam) < 1e-9, N + F("the stage's model is %.0f m (with the stem's rake) by %.1f m", M.Long, M.Wide));
			Check(std::fabs(M.Deck - 1.65) < 1e-9 && std::fabs(M.TowerTop - 6.25) < 1e-9 && std::fabs(M.ScopeTop - 10.65) < 1e-9 && std::fabs(M.LowTop - 9.15) < 1e-9 && M.Top == M.ScopeTop,
			      N + "its deck 1.65 m, its bridge's top 6.25, its fixed periscopes' heads 10.65 and 9.15, the taller its highest point");
			Check(M.ScopeAlong == 1.5 && M.LowAlong == 0.0 && M.TowerFore == 5.4 && M.TowerAft == -4.6, N + "the taller periscope 1.5 m forward of the shorter; the bridge's top from 5.4 m forward to 4.6 aft");
			Check(std::fabs(D.Up - 8.3 / W.Beam) < 1e-9 && std::fabs(D.Along - 95.0 / (W.Hull + 2.0)) < 1e-9, N + F("fitted to 95 by 8.3 m: up %.3f, along %.3f", D.Up, D.Along));
			const double Clear = ScopeClearOf(M, D);
			Check(Clear >= 1.0 && Clear <= 1.8 && (M.ScopeTop - M.LowTop) * D.Up - Clear >= 0.4 - 1e-9, N + F("her taller periscope shows %.2f m, the shorter's head %.2f m under", Clear, (M.ScopeTop - M.LowTop) * D.Up - Clear));
		}
		if (Root.empty())
		{
			std::printf("(no source folder given: the rows not read against the model's code)\n");
			return;
		}
		const std::string Ships = TextOf(Root + "/Campaign/CampaignShips.cpp");
		const size_t From = Ships.find("static void Submarine(FMiniatureMesh& M)");
		const size_t To = From == std::string::npos ? std::string::npos : Ships.find("static const MiniatureParts::FModel Models[]", From);
		Check(From != std::string::npos && To != std::string::npos, "Campaign/CampaignShips.cpp has the code-built Submarine()");
		const std::string Sub = From != std::string::npos && To != std::string::npos ? Ships.substr(From, To - From) : std::string();
		for (const char* Said : {
			     "Tag == TEXT(\"USA\") ? 95.0 : (Tag == TEXT(\"GER\") ? 67.0 : 80.0)",
			     "const FHull H{ Long, Tag == TEXT(\"USA\") ? 8.3 : 6.2, 1.6, 0.9, 0.0, 2.0, 0.0 };",
			     "const double Z = Y.Deck(0.0);",
			     "{ { 5.4, 0.0 }, { 3.6, 1.3 }, { -2.8, 1.3 }, { -4.6, 0.0 }, { -2.8, -1.3 }, { 3.6, -1.3 } } },\n\t\t\t{ Z, Z + 4.6 }",
			     "M.Rod(At(1.5, 0.0, Z + 4.6), At(1.5, 0.0, Z + 9.0), 0.12, Steel, 4);",
			     "M.Rod(At(0.0, 0.0, Z + 4.6), At(0.0, 0.0, Z + 7.5), 0.1, Steel, 4);",
			     "M.Pillar(At(-7.0, 0.0, Z), 2.0, 2.0, 2.2, Y.L.Upper, 10);",
			     "Y.LightGun(-7.0, 0.0, Z + 2.2);",
			     "At(5.0, 0.0, Z + 4.8), 0.05, Steel, 3);",
		     })
		{
			Check(Sub.find(Said) != std::string::npos, std::string("the stage's code-built submarine still has: ") + Said);
		}
		Check(Ships.find("return DeckAt(H, X) + 0.05;") != std::string::npos && Ships.find("M.Rod(At(X, Side - 0.25, Z + 1.0), At(X + 1.6, Side - 0.25, Z + 2.2), 0.09, Steel, 4);") != std::string::npos,
		      "its deck laid 0.05 m over the hull, its anti-aircraft gun's barrels 1.6 m long rising 1.2 m");
		Check(TextOf(Root + "/Battle/AirStage.cpp").find("Class == TEXT(\"ss\") ? TEXT(\"Submarine\")") != std::string::npos, "the stage lays a boat as \"Submarine\"");
		Check(TextOf(Root + "/Battle/AirFeed.cpp").find("if (Class == \"ss\") return { 95.0, 8.3 };") != std::string::npos, "and fits her to 95 by 8.3 m");
		const std::string Content = Root + "/../../Content/Models/";
		Check(!std::ifstream(Content + "Submarine.json").good() && !std::ifstream(Content + "Old/Submarine.json").good(),
		      "no sculpted Submarine among the game's models (the stage would draw it instead of the code-built one)");
		Check(std::ifstream(Content + "Bf109.json").good(), "(the game's models are where this test looks for them)");
		// The battle modeller's boats the stage draws now wherever the game has them (AirStage's ShipFileOf, "Ships/" and the
		// class's model name): SculptedOf's rows against each file's own sockets and size, as Tools/battle_ships_in.py
		// writes them (the attack periscope raised).
		const std::string Stage = TextOf(Root + "/Battle/AirStage.cpp");
		Check(Stage.find("Kind = TEXT(\"Submarine\");") != std::string::npos && Stage.find("TEXT(\"Ships/\") + Asked") != std::string::npos,
		      "the stage draws a boat as the battle modeller's Ships/Submarine_<navy>, else Ships/Submarine");
		// (a socket's place or facing in a model's sockets file, as Tools/battle_ships_in.py writes it: along her forward and
		// up, in metres for a place)
		auto ReadSocket = [](const std::string& Text, const std::string& Name, const std::string& Field, double& OutAlong, double& OutUp)
		{
			const size_t Named = Text.find("\"name\":\"" + Name + "\"");
			const size_t Where = Named == std::string::npos ? std::string::npos : Text.find("\"" + Field + "\":[", Named);
			double Across = 0.0;
			return Where != std::string::npos && std::sscanf(Text.c_str() + Where + Field.size() + 4, "%lf,%lf,%lf", &OutAlong, &Across, &OutUp) == 3;
		};
		for (const char* File : { "Submarine_GER", "Submarine_USA", "Submarine" })
		{
			const BoatModel M = SculptedOf(File);
			const std::string N = std::string("[") + File + "] ";
			const std::string Sockets = TextOf(Content + "Ships/" + File + ".sockets.json");
			const std::string Model = TextOf(Content + "Ships/" + File + ".json");
			Check(!Sockets.empty() && !Model.empty(), N + "the model and its sockets are in the game");
			auto SocketAt = [&Sockets](const std::string& Name, double& X, double& Z)
			{
				const size_t At = Sockets.find("\"name\":\"" + Name + "\"");
				const size_t Where = At == std::string::npos ? std::string::npos : Sockets.find("\"at\":[", At);
				double Y = 0.0;
				return Where != std::string::npos && std::sscanf(Sockets.c_str() + Where + 6, "%lf,%lf,%lf", &X, &Y, &Z) == 3;
			};
			double X = 0.0, Z = 0.0;
			Check(SocketAt("periscope.1", X, Z) && std::fabs(X - M.ScopeAlong) < 0.05 && M.ScopeTop > Z && M.ScopeTop - Z < 0.5 && Z > M.TowerTop + 3.0,
			      N + F("her attack periscope raised: its head %.2f m up at %.2f along, the rod's top %.2f", Z, X, M.ScopeTop));
			Check(SocketAt("periscope.2", X, Z) && std::fabs(X - M.LowAlong) < 0.05 && M.LowTop > Z && M.LowTop - Z < 0.5,
			      N + F("her other periscope housed: its head %.2f m up at %.2f along, the rod's top %.2f", Z, X, M.LowTop));
			Check(SocketAt("aa.1", X, Z) && std::fabs(X - M.GunAlong) < 0.05 && std::fabs(M.GunTop - Z - 0.1) < 0.02,
			      N + F("her light gun's muzzle %.2f m up at %.2f along", Z, X));
			Check(SocketAt("wake.bow", X, Z) && std::fabs(X - M.BowAlong) < 0.05, N + F("her stem at the waterline %.1f m forward", X));
			// (her ensign staff's foot, where a boat that strikes runs up her white flag: abaft her bridge's periscope
			// standard, on it)
			Check(SocketAt("ensign", X, Z) && std::fabs(X - M.EnsignAlong) < 0.05 && std::fabs(Z - M.EnsignTop) < 0.05 && X < M.ScopeAlong && X > M.TowerAft - 0.5 && Z > M.TowerTop,
			      N + F("her ensign staff's foot %.2f m along at %.2f m, at her bridge's after end abaft her periscopes", X, Z));
			double Long = 0.0;
			const size_t L = Model.find("\"long\":");
			Check(L != std::string::npos && std::sscanf(Model.c_str() + L + 7, "%lf", &Long) == 1 && std::fabs(Long - M.Long) < 0.05, N + F("her bounds %.1f m long", Long));
			Check(M.WireTop > M.TowerTop && M.TowerFore > M.TowerAft && M.Deck < M.TowerTop && M.Top == M.ScopeTop, N + "her standard (or shears) over her bridge's rim, her casing under it");
			// Her deck gun's muzzle (muzzle.turret.A.1, where the scene lights a round of hers): on her casing clear of her
			// bridge, inside her length, a barrel's height over the casing, and at the end the show stows it at (Gunnery: an
			// American fleet boat's aft of her tower, stowed facing aft; any other navy's forward of it, facing forward).
			const bool bGunAft = std::string(File) == "Submarine_USA";
			double Up = 0.0;
			Check(SocketAt("muzzle.turret.A.1", X, Z) && (bGunAft ? X < M.TowerAft : X > M.TowerFore) && std::fabs(X) < M.BowAlong && Z - M.Deck >= 0.5 && Z - M.Deck <= 2.0,
			      N + (bGunAft ? "aft of her bridge, " : "forward of her bridge, ") + F("her deck gun's muzzle %.2f m along, %.2f m over her casing", X, Z - M.Deck));
			Check(ReadSocket(Sockets, "muzzle.turret.A.1", "facing", X, Up) && (bGunAft ? X < -0.9 : X > 0.9) && std::fabs(Up) < 0.1,
			      N + F("her deck gun stowed fore and aft toward its own end: facing %.3f along, %.3f up", X, Up));
		}
		Check(SculptedOf("Submarine_ITA").Name.empty(), "no row for a file the game does not have");
		// (the show stows a boat's deck gun by her navy as these models lay it, trains it no further than its arc (its arcs
		// against the models' own limits: TestGuns), and the scene draws the gun laid so and lights the round at its muzzle.
		// These pins were the show's "Stow = H->Nation == "USA" ? 180.0 : 0.0;" and "Arc = 150.0;" and the scene's swung
		// muzzle until the review of 23 November: changed on purpose with them.)
		const std::string ShowSource = TextOf(Root + "/Battle/SeaShow.cpp"), SceneSource = TextOf(Root + "/Battle/SeaBattle.cpp");
		Check(ShowSource.find("H.bAftGun = H.Class == \"ss\" && H.Nation == \"USA\";") != std::string::npos && ShowSource.find("H.Train = H.bAftGun ? 180.0 : 0.0;") != std::string::npos &&
		          ShowSource.find("Stow = H->bAftGun ? 180.0 : 0.0;") != std::string::npos && ShowSource.find("Arc = DeckGunArc;") != std::string::npos &&
		          ShowSource.find("Arc = MerchantGunArc;") != std::string::npos,
		      "the show stows an American boat's deck gun aft and any other's forward, each gun bearing no further than its arc");
		// (a boat's deck-gun bore until the battle names it, by navy: a Japanese boat's the 14 cm most of her fleet boats
		// carried. The show had her 120 mm until the join of the view under the water; this pin came with that change.)
		Check(ShowSource.find("if (Class == \"ss\") return Nation == \"GER\" ? 88.0 : Nation == \"UK\" ? 102.0 : Nation == \"USA\" ? (F.Year >= 1944 ? 127.0 : "
		                      "F.Year >= 1942 ? 102.0 : 76.0) : Nation == \"JAP\" ? 140.0 : 100.0;") != std::string::npos,
		      "the show's deck-gun bores, by navy, until the battle names one: a German boat's 88 mm, a British boat's 102, an American's 76 to 127 by year, "
		      "a Japanese boat's 140 (her 14 cm), any other's 100");
		// (and every such round's miss a small column, to that 14 cm: the sea show's test holds a small column to a deck gun's
		// 40 to 140 mm, as it holds the bores. The show threw a warship's column from 130 mm until the review of the join of
		// the view under the water, which a Japanese boat's 14 cm crossed: this pin came with that change.)
		Check(ShowSource.find("const std::string Splash = bOneGun && Fell.Calibre <= 140.0 ? \"small\" : ") != std::string::npos,
		      "a boat's deck gun's and a merchant's gun's misses fall as small columns at every bore the show gives them, to a Japanese boat's 140 mm");
		Check(SceneSource.find("Stage->SocketAt(E.Key, bStern ? TEXT(\"muzzle.turret.Y.1\") : TEXT(\"muzzle.turret.A.1\"), GunMuzzle, GunBore)") != std::string::npos &&
		          SceneSource.find("const bool bStern = E.What == \"mv\" && Firer->bAftGun;") != std::string::npos &&
		          SceneSource.find("Stage->TrainGun(Ship.Key, bStern ? TEXT(\"turret.Y\") : TEXT(\"turret.A\"), Ship.Train, Ship.Elevation, Back);") != std::string::npos,
		      "the scene draws the gun the show lays as it lays it, and lights the round at its muzzle (a merchant's stern gun's where the show's bAftGun has it)");
		// A merchant's guns (the stage draws one as a Freighter, a Tanker or a Troopship): her bow gun's muzzle (A.1) forward and
		// her stern gun's (Y.1) aft, each stowed facing its own end, as the show trains the one that bears.
		for (const char* File : { "Freighter", "Tanker", "Troopship" })
		{
			const std::string N = std::string("[") + File + "] ";
			const std::string Sockets = TextOf(Content + "Ships/" + File + ".sockets.json");
			double BowX = 0.0, BowZ = 0.0, BowWay = 0.0, SternX = 0.0, SternZ = 0.0, SternWay = 0.0, Up = 0.0;
			Check(ReadSocket(Sockets, "muzzle.turret.A.1", "at", BowX, BowZ) && BowX > 0.0 && BowZ > 0.0 && ReadSocket(Sockets, "muzzle.turret.A.1", "facing", BowWay, Up) && BowWay > 0.9,
			      N + F("her bow gun's muzzle %.1f m forward, %.1f m up, facing %.3f along", BowX, BowZ, BowWay));
			Check(ReadSocket(Sockets, "muzzle.turret.Y.1", "at", SternX, SternZ) && SternX < 0.0 && SternZ > 0.0 && ReadSocket(Sockets, "muzzle.turret.Y.1", "facing", SternWay, Up) && SternWay < -0.9,
			      N + F("her stern gun's muzzle %.1f m aft, %.1f m up, facing %.3f along", -SternX, SternZ, SternWay));
		}
	}

	/**
	 * An enemy boat out of the player's sight (BoatIn::bShown false: the sea show's SetMine, until she is found): stepped
	 * through the run of Steam (surfaced, a crash dive, her periscope up, deep under a starshell's light, up again) beside a
	 * twin kept in sight all along, hidden from 10 to 100 s and from 150 to 240 s. She rides exactly as her twin (so found she
	 * is where she would be, no jump), throws nothing on the sea while hidden (no feather, wash, spray or new swirl), leaves
	 * the swirls laid before to fade as their own, and once found throws as her twin does. A third, hidden from 140 to 180 s
	 * and found while still deep under the light, lays her first swirl after it when her twin does: her swirls' clock runs
	 * on while she is out of sight, so found she gives no swirl of her own the moment she is seen.
	 */
	void TestUnseen()
	{
		const BoatModel M = ModelOf("GER");
		const Drawn D = DrawnOf(M, 95.0, 8.3);
		BoatState Hid, Open, FoundDeep;
		auto OutDeep = [](double T) { return T >= 140.0 && T < 180.0; };
		double FoundDeepSwirl = -1.0, OpenDeepSwirl = -1.0;
		ShowBoat Show;
		auto Out = [](double T) { return (T >= 10.0 && T < 100.0) || (T >= 150.0 && T < 240.0); };
		double MostApart = 0.0;
		int HidThrown = 0, OpenThrownHidden = 0, HidLaid = 0, OpenLaidHidden = 0, HidAfter = 0, OpenAfter = 0, KeptAtHide = 0, OpenKeptAtHide = 0;
		bool bLooked = false;
		for (double T = 0.0; T < 300.0; T += Frame)
		{
			std::string Dive = "surfaced";
			if (T >= 20.0 && T < 130.0) Dive = "periscope";
			else if (T >= 130.0 && T < 210.0) Dive = "deep";
			Show.Step(T, Dive);
			BoatIn In;
			In.T = T;
			In.Dive = Dive;
			In.Keel = Show.Keel;
			In.Dived = Show.Dived();
			In.bScope = T >= 60.0 && T < 120.0;
			In.Speed = Dive == "surfaced" ? 3.5 : 2.5;
			In.Pitch = Show.Pitch(T, In.Speed);
			In.Heading = 30.0;
			In.At = P3(100.0 + In.Speed * T * 0.5, 200.0 + In.Speed * T * 0.866, 0.0);
			In.Heave = 0.6 * std::sin(T * 0.7) * Show.Calm();
			In.Lit = 0.6;
			const BoatOut O = StepBoat(Open, In, M, D);
			In.bShown = !Out(T);
			const BoatOut H = StepBoat(Hid, In, M, D);
			In.bShown = !OutDeep(T);
			StepBoat(FoundDeep, In, M, D);
			for (const Swirl& W : FoundDeep.Swirls)
				if (FoundDeepSwirl < 0.0 && W.T0 >= 180.0) FoundDeepSwirl = W.T0;
			for (const Swirl& W : Open.Swirls)
				if (OpenDeepSwirl < 0.0 && W.T0 >= 180.0) OpenDeepSwirl = W.T0;
			MostApart = std::max({ MostApart, std::fabs(H.Heave - O.Heave), std::fabs(H.BridgeZ - O.BridgeZ), std::fabs(H.ScopeZ - O.ScopeZ) });
			if (Out(T))
			{
				HidThrown += static_cast<int>(H.Splashes.size());
				OpenThrownHidden += static_cast<int>(O.Splashes.size());
			}
			else if (T >= 240.0)
			{
				HidAfter += static_cast<int>(H.Splashes.size());
				OpenAfter += static_cast<int>(O.Splashes.size());
			}
			for (const Swirl& W : Hid.Swirls)
				if (Out(W.T0) && W.T0 > T - Frame * 0.5) ++HidLaid;
			for (const Swirl& W : Open.Swirls)
				if (Out(W.T0) && W.T0 > T - Frame * 0.5) ++OpenLaidHidden;
			// (just after the hide while deep: the swirls laid before it still on the sea, as many as her twin's)
			if (!bLooked && T >= 150.0 + 2.0 * Frame)
			{
				bLooked = true;
				for (const Swirl& W : Hid.Swirls) KeptAtHide += W.T0 < 150.0 ? 1 : 0;
				for (const Swirl& W : Open.Swirls) OpenKeptAtHide += W.T0 < 150.0 ? 1 : 0;
			}
		}
		Check(MostApart < 1e-9, F("hidden, she rides exactly as her twin in sight: %.2g m apart at the most", MostApart));
		Check(HidThrown == 0 && OpenThrownHidden > 0, F("hidden, nothing thrown on the sea (her twin threw %.0f): %.0f", OpenThrownHidden, HidThrown));
		Check(HidLaid == 0 && OpenLaidHidden > 0, F("hidden deep under the light, no swirl laid (her twin laid %.0f): %.0f", OpenLaidHidden, HidLaid));
		Check(KeptAtHide > 0 && KeptAtHide == OpenKeptAtHide, F("the swirls laid before the hide fade as their own, none put out: %.0f of her twin's %.0f", KeptAtHide, OpenKeptAtHide));
		Check(HidAfter > 0 && HidAfter == OpenAfter, F("found, she throws as her twin does: %.0f of her twin's %.0f", HidAfter, OpenAfter));
		Check(FoundDeepSwirl >= 180.0 && FoundDeepSwirl < 210.0 && std::fabs(FoundDeepSwirl - OpenDeepSwirl) < 1e-9,
		      F("found while deep, her first swirl when her twin's (her clock ran on unseen): %.3f s, her twin's %.3f s", FoundDeepSwirl, OpenDeepSwirl));
	}

	void TestBursts()
	{
		const double Mk = ChargeKg("pattern"), Hh = ChargeKg("ahead");
		Check(Hh < Mk, "the ahead-thrown bomb is smaller than a pattern's charge");
		const BurstLook Shallow = BurstOf(15.0, Mk);
		Check(Shallow.ColumnHigh > 30.0 && Shallow.ColumnHigh < 50.0, F("a charge set to 50 ft throws a column 30 to 50 m: %.1f", Shallow.ColumnHigh));
		Check(Shallow.DomeHigh > 5.0 && Shallow.DomeHigh <= 12.0, F("its dome 5 to 12 m: %.1f", Shallow.DomeHigh));
		Check(Shallow.ColumnAt > Shallow.DomeAt, "the white dome first, then the column");
		Check(Shallow.BoilAt < 0.0, "a shallow charge vents in its column, no boil after");
		BurstLook Was = Shallow;
		for (double Depth = 20.0; Depth <= 250.0; Depth += 5.0)
		{
			const BurstLook B = BurstOf(Depth, Mk);
			Check(B.DomeAt > Was.DomeAt, F("deeper, the dome is later (%.0f m)", Depth));
			Check(B.DomeHigh <= Was.DomeHigh + 1e-9, F("deeper, the dome is lower (%.0f m)", Depth));
			Check(B.DomeWide > Was.DomeWide, F("deeper, the dome is broader (%.0f m)", Depth));
			Check(B.ColumnHigh <= Was.ColumnHigh + 1e-9, F("deeper, the column is no higher (%.0f m)", Depth));
			Check((B.ColumnHigh > 0.0) == (B.BoilAt < 0.0), F("a column or a boil, never both (%.0f m)", Depth));
			Check(B.BoilAt < 0.0 || B.BoilAt > B.DomeAt, F("the boil after the dome (%.0f m)", Depth));
			Was = B;
		}
		Check(BurstOf(100.0, Mk).ColumnHigh == 0.0, "a charge set to 100 m throws no column, its dome alone");
		Check(BurstOf(30.0, Hh).ColumnHigh < BurstOf(30.0, Mk).ColumnHigh, "an ahead bomb's burst is smaller than a charge's at the same depth");
		Check(ColumnRow(2.0).empty() && ColumnRow(10.0) == "how105" && ColumnRow(20.0) == "how150" && ColumnRow(40.0) == "bomb250" && ColumnRow(55.0) == "bomb500",
		      "a column's kit row by its height");
		for (const double Kg : { Mk, Hh })
		{
			for (double Depth = 5.0; Depth <= 300.0; Depth += 5.0)
			{
				const BurstLook B = BurstOf(Depth, Kg);
				for (const bool bHit : { false, true })
				{
					const std::vector<KitSpout> Spouts = SpoutsOf(B, bHit, 6.0, static_cast<uint64_t>(Depth * 10.0 + Kg));
					int Churns = 0, Columns = 0, Skips = 0;
					double Far = 0.0, Lastest = -1.0;
					bool bOrdered = true, bOutward = true;
					for (const KitSpout& K : Spouts)
					{
						Check(std::isfinite(K.Dt) && K.Dt >= B.DomeAt - 1e-9, F("each call at or after the dome (%.0f m)", Depth));
						bOrdered = bOrdered && K.Dt >= Lastest;
						Lastest = K.Dt;
						if (K.Row == "churn") ++Churns;
						else if (K.Row == "skip")
						{
							++Skips;
							bOutward = bOutward && K.Dx * K.WayX + K.Dy * K.WayY > 0.0;
						}
						else ++Columns;
						Far = std::max(Far, std::hypot(K.Dx, K.Dy));
					}
					const std::string W = F("(%.0f m, %.0f kg)", Depth, Kg);
					Check(bOrdered, "calls in time order " + W);
					Check(Churns >= (bHit ? 1 : 0) && Churns <= MostChurns() + (bHit ? 1 : 0), "at most three churns a burst, one more for her air " + W);
					Check((B.DomeHigh >= ChurnLeast() || B.DomeHigh < DomeLeast()) == (Skips == 0), "a low dome is a ring of low spurts, a high one churns, a flat one nothing " + W);
					Check(B.DomeHigh >= DomeLeast() || B.BoilAt > 0.0, "a burst too deep to lift the sea still shows its boil " + W);
					Check(Skips == 0 || (Skips >= 6 && Skips <= 10 && bOutward), "the low ring's spurts thrown outward " + W);
					Check(Columns == (B.ColumnHigh > 0.0 ? 1 : 0), "a column only where it throws one " + W);
					Check(Far <= std::max(0.5 * B.DomeWide, 6.0) + 1e-9, "the dome's calls inside its width " + W);
				}
			}
		}
		const std::vector<KitSpout> Hit = SpoutsOf(BurstOf(60.0, Mk), true, 6.0, 5);
		Check(Hit.size() == SpoutsOf(BurstOf(60.0, Mk), false, 6.0, 5).size() + 1, "the booked burst has one more boil, her air");
		const KitSpout& Air = *std::max_element(Hit.begin(), Hit.end(), [](const KitSpout& L, const KitSpout& R) { return L.Dt < R.Dt; });
		Check(Air.Row == "churn" && std::hypot(Air.Dx, Air.Dy) <= 6.0 + 1e-9, "her air comes up last, within the charge's lethal radius");
		if (bStory)
		{
			std::printf("  bursts (Mk VII; ahead bomb):\n");
			for (const double Depth : { 15.0, 30.0, 50.0, 70.0, 100.0, 200.0 })
			{
				const BurstLook B = BurstOf(Depth, Mk), A = BurstOf(Depth, Hh);
				std::printf("    %5.0f m: dome at %.2f s, %.1f m high, %.0f m across; column at %.2f s, %.0f m (%s); boil at %.1f s | ahead: dome %.1f m, column %.0f m\n", Depth, B.DomeAt,
				            B.DomeHigh, B.DomeWide, B.ColumnAt, B.ColumnHigh, ColumnRow(B.ColumnHigh).empty() ? "none" : ColumnRow(B.ColumnHigh).c_str(), B.BoilAt, A.DomeHigh,
				            A.ColumnHigh);
			}
		}
	}

	/** The show's burst for charge K of a pattern (SeaShow.cpp, sea:depthCharge, without its 3 m jitter). */
	P3 ShowBurst(const P3& Stern, double HeadingDeg, int K, int N, double Spread, double Beam)
	{
		const double Share = N > 1 ? static_cast<double>(K) / (N - 1) : 0.0;
		const bool bRail = K % 2 == 0;
		const double Side = bRail ? 0.0 : ((K / 2) % 2 == 0 ? 1.0 : -1.0) * (Spread * 0.5 - Beam * 0.5);
		return Stern + AheadOf(HeadingDeg) * (Share * Spread) + RightOf(HeadingDeg) * Side;
	}

	void TestPatterns()
	{
		for (const int N : { 5, 10, 14 })
		{
			for (const double Speed : { 3.0, 7.0, 9.0 })
			{
				const double Heading = 75.0, Spread = 120.0, Beam = 10.0, Sinking = 23.3;
				DropIn In;
				In.N = N;
				In.Spread = Spread;
				In.Sinking = Sinking;
				In.Until = Spread / Speed; // [the show's Run]
				In.At = P3(5000.0, 4000.0, 1.0);
				In.Vel = AheadOf(Heading) * Speed;
				In.Aim = In.At + AheadOf(Heading) * (Spread * 0.5);
				In.Beam = Beam;
				In.Long = 95.0;
				const std::vector<DropShot> P = DropPlan(In, 7);
				const std::string W = F("(%.0f charges at %.0f m/s)", N, Speed);
				Check(static_cast<int>(P.size()) == N, "a pattern of so many charges " + W);
				int Port = 0, Starboard = 0, Rails = 0;
				double Worst = 0.0, WorstStern = 0.0, Last = 0.0;
				bool bInBefore = true, bAlternate = true;
				for (int K = 0; K < static_cast<int>(P.size()); ++K)
				{
					const DropShot& S = P[K];
					Rails += S.Side == 0 ? 1 : 0;
					Port += S.Side == -1 ? 1 : 0;
					Starboard += S.Side == 1 ? 1 : 0;
					bAlternate = bAlternate && (S.Side == 0) == (K % 2 == 0);
					const P3 Burst = ShowBurst(P3(In.At.X, In.At.Y, 0.0), Heading, K, N, Spread, Beam);
					Worst = std::max(Worst, std::hypot(S.To.X - Burst.X, S.To.Y - Burst.Y));
					if (S.Side == 0)
					{
						WorstStern = std::max(WorstStern, std::hypot(S.From.X - Burst.X, S.From.Y - Burst.Y));
					}
					// (in the water before the show bursts it: its share of the run, then the sinking time)
					const double Share = N > 1 ? static_cast<double>(K) / (N - 1) : 0.0;
					bInBefore = bInBefore && S.Dt + S.Flight < Share * In.Until + Sinking;
					Check(std::fabs(S.Dt - Share * In.Until) < 1e-9, "each charge goes in at its even share of the run " + W);
					Check(S.To.Z == 0.0 && S.From.Z > 1.0, "from her deck, into the sea " + W);
					Last = std::max(Last, S.Dt);
				}
				Check(bAlternate, "alternately off the rails and from the throwers " + W);
				Check(Rails == (N + 1) / 2 && Port + Starboard == N / 2 && std::abs(Port - Starboard) <= 1, F("rails %.0f, throwers %.0f and %.0f", Rails, Port, Starboard) + " " + W);
				Check(Worst < 2.0, F("each lands where the show bursts it (within %.2f m)", Worst) + " " + W);
				Check(WorstStern < 2.5, F("rolled off her stern where she is then (within %.2f m)", WorstStern) + " " + W);
				Check(std::fabs(Last - In.Until) < 1e-9, "the last goes in as the run ends " + W);
				Check(bInBefore, "every charge in the water before its burst " + W);
			}
		}
		for (const int N : { 5, 10, 14 })
		{
			// laid all at once over the spot the battle tells (the show's Run 0, as the naval rules lay a pattern): every
			// charge goes in at the drop, lands where the show bursts it, each rail charge rolled off over its own place
			const double Heading = 124.0, Spread = 120.0, Beam = 10.0, Sinking = 36.0;
			DropIn In;
			In.N = N;
			In.Spread = Spread;
			In.Sinking = Sinking;
			In.Until = 0.0;
			In.At = P3(9864.0, 15668.0, 1.0);
			In.Vel = AheadOf(Heading) * 4.2;
			In.Aim = In.At + AheadOf(Heading) * (Spread * 0.5);
			In.Beam = Beam;
			In.Long = 95.0;
			const std::vector<DropShot> P = DropPlan(In, 7);
			const std::string W = F("(%.0f charges laid at once)", N);
			Check(static_cast<int>(P.size()) == N, "a pattern of so many charges " + W);
			double Worst = 0.0, WorstStern = 0.0, Latest = 0.0;
			for (int K = 0; K < static_cast<int>(P.size()); ++K)
			{
				const DropShot& S = P[K];
				const P3 Burst = ShowBurst(P3(In.At.X, In.At.Y, 0.0), Heading, K, N, Spread, Beam);
				Worst = std::max(Worst, std::hypot(S.To.X - Burst.X, S.To.Y - Burst.Y));
				if (S.Side == 0)
				{
					WorstStern = std::max(WorstStern, std::hypot(S.From.X - Burst.X, S.From.Y - Burst.Y));
				}
				Latest = std::max(Latest, S.Dt);
				Check(S.To.Z == 0.0 && S.From.Z > 1.0 && S.Dt + S.Flight < Sinking, "from her deck, into the sea before its burst " + W);
			}
			Check(Latest == 0.0, F("every charge goes in at the drop (the last %.2f s after it)", Latest) + " " + W);
			Check(Worst < 2.0, F("each lands where the show bursts it (within %.2f m)", Worst) + " " + W);
			Check(WorstStern < 2.5, F("each rail charge rolled off over its own place (within %.2f m)", WorstStern) + " " + W);
		}
		{
			// thrown to either side, well out on its arc
			DropIn In;
			In.N = 10;
			In.Until = 17.0;
			In.Vel = AheadOf(0.0) * 7.0;
			In.Aim = P3(0.0, 60.0, 0.0);
			const std::vector<DropShot> P = DropPlan(In, 3);
			for (const DropShot& S : P)
			{
				if (S.Side == 1 || S.Side == -1)
				{
					Check(S.To.X * S.Side > 40.0 && S.High > 5.0 && S.Flight > 1.0, F("a thrown charge lands well out to its side on an arc: %.1f m", S.To.X));
				}
			}
		}
		{
			// the ahead thrower's 24 bombs: a ring of small splashes round the pattern's middle, about 200 m ahead
			DropIn In;
			In.Kind = "ahead";
			In.N = 24;
			In.Spread = 40.0;
			In.Sinking = 80.0 / 7.0;
			In.Until = 0.0;
			In.At = P3(1000.0, 1000.0, 3.0);
			In.Vel = AheadOf(0.0) * 6.0;
			In.Aim = P3(1000.0, 1200.0, 0.0);
			const std::vector<DropShot> P = DropPlan(In, 11);
			double Least = 1e9, Most = 0.0, Last = 0.0, Ahead = 0.0;
			int Quarters[4] = { 0, 0, 0, 0 };
			for (const DropShot& S : P)
			{
				const double R = std::hypot(S.To.X - In.Aim.X, S.To.Y - In.Aim.Y);
				Least = std::min(Least, R);
				Most = std::max(Most, R);
				Last = std::max(Last, S.Dt + S.Flight);
				Ahead += (S.To.Y - In.At.Y) / static_cast<double>(P.size());
				const double A = std::atan2(S.To.X - In.Aim.X, S.To.Y - In.Aim.Y);
				Quarters[static_cast<int>((A + Pi) / (Pi / 2.0)) % 4] += 1;
				Check(S.Side == 2 && S.High > 20.0, "thrown from her forecastle high over her bow");
				Check(std::hypot(S.From.X - (In.At.X + In.Vel.X * S.Dt), S.From.Y - (In.At.Y + In.Vel.Y * S.Dt)) < 1e-6, "fired from her forecastle where she is then");
			}
			Check(P.size() == 24, F("the ahead thrower's 24 bombs: %.0f", static_cast<double>(P.size())));
			Check(Least > 16.0 && Most < 24.0, F("their splashes on a ring 40 m across: %.1f to %.1f m out", Least, Most));
			Check(Quarters[0] >= 4 && Quarters[1] >= 4 && Quarters[2] >= 4 && Quarters[3] >= 4, "all round the ring");
			Check(Ahead > 180.0 && Ahead < 220.0, F("about 200 m ahead of her: %.0f m", Ahead));
			Check(Last <= 0.85 * In.Sinking, F("all in before any burst: %.1f s", Last));
		}
		{
			const P3 From(0.0, 0.0, 3.0), To(40.0, 0.0, 0.0);
			const P3 A0 = Arc(From, To, 10.0, 2.0, 0.0), A1 = Arc(From, To, 10.0, 2.0, 2.0), Mid = Arc(From, To, 10.0, 2.0, 1.0);
			Check(std::hypot(A0.X - From.X, A0.Z - From.Z) < 1e-9 && std::hypot(A1.X - To.X, A1.Z - To.Z) < 1e-9, "an arc starts at its thrower and ends at its splash");
			Check(std::fabs(Mid.Z - (1.5 + 10.0)) < 1e-9, F("its top High over the middle: %.2f m", Mid.Z));
		}
	}

	void TestStarshell()
	{
		for (const double Burn : { 40.0, 60.0, 75.0, 90.0 })
		{
			StarIn In;
			In.Burst = P3(5000.0, 6000.0, 450.0);
			In.T0 = 100.0;
			In.Until = 100.0 + Burn;
			In.Wind = P3(4.0, -2.0, 0.0);
			In.Seed = 99;
			double LastZ = 1e9, Most = 0.0, Early = 0.0, Late = 0.0;
			int NEarly = 0, NLate = 0;
			bool bAlwaysUp = true, bSinks = true, bChute = true, bDark = true;
			for (double T = 90.0; T < 100.0 + Burn + 10.0; T += 0.05)
			{
				const StarOut O = StarAt(In, T);
				if (T < 100.0)
				{
					bDark = bDark && !O.bShown && O.Light == 0.0;
					continue;
				}
				if (!O.bShown) continue;
				bSinks = bSinks && O.Flare.Z < LastZ;
				LastZ = O.Flare.Z;
				bChute = bChute && O.Chute.Z > O.Flare.Z;
				if (O.bBurning)
				{
					bAlwaysUp = bAlwaysUp && O.Flare.Z >= 40.0 - 1e-6;
					Most = std::max(Most, O.Light);
					if (T - 100.0 > 2.0 && T - 100.0 < 0.3 * Burn) { Early += O.Light; ++NEarly; }
					if (T - 100.0 > 0.7 * Burn && T - 100.0 < Burn - 6.0) { Late += O.Light; ++NLate; }
				}
				else
				{
					Check(O.Light == 0.0, "no light once it has burnt out");
				}
			}
			const std::string W = F("(%.0f s burn)", Burn);
			Check(bDark, "nothing before it bursts " + W);
			Check(bSinks && bChute, "it sinks all the time under its parachute " + W);
			Check(Most <= 1.0 && Most > 0.9, F("its light up to full: %.2f", Most) + " " + W);
			Check(Early / std::max(NEarly, 1) > Late / std::max(NLate, 1) + 0.1, F("its light falls as it burns down and sinks: %.2f then %.2f", Early / std::max(NEarly, 1), Late / std::max(NLate, 1)) + " " + W);
			Check(bAlwaysUp, "it burns out still in the air, 40 m up or more " + W);
			const StarOut Mid = StarAt(In, 100.0 + Burn * 0.5);
			Check(Mid.Flare.X > In.Burst.X + 0.9 * 4.0 * Burn * 0.5 - 2.0 && Mid.Flare.Y < In.Burst.Y, "it drifts down the wind " + W);
			Check(!StarAt(In, 100.0 + Burn + 3.5).bShown, "gone a moment after it burns out " + W);
			Check(StarAt(In, 100.0 + Burn + 1.0).bShown && !StarAt(In, 100.0 + Burn + 1.0).bBurning, "its dead candle a moment after " + W);
			if (bStory && Burn == 75.0)
			{
				std::printf("  starshell, %0.f s burn:\n", Burn);
				for (double T = 100.0; T <= 100.0 + Burn + 3.0; T += 7.5)
				{
					const StarOut O = StarAt(In, T);
					std::printf("    t %5.1f flare %6.0f %6.0f %5.0f m light %.2f, on the sea under it %.2f\n", T - 100.0, O.Flare.X, O.Flare.Y, O.Flare.Z, O.Light, LitBy(O, P3(O.Flare.X, O.Flare.Y, 0.0)));
				}
			}
		}
		Check(StarFall(450.0, 60.0) == 3.0, "from 450 m a minute's flare sinks 3 m a second");
		Check(StarFall(200.0, 90.0) < 3.0 && 200.0 - StarFall(200.0, 90.0) * 90.0 >= 40.0 - 1e-9, "from 200 m it sinks slower, so it burns out in the air");
		Check(StarBurnUsual() >= 60.0 && StarBurnUsual() <= 90.0, "a show without a burn-out time: 60 to 90 s");
		{
			StarOut O;
			O.bBurning = true;
			O.Light = 1.0;
			O.Flare = P3(0.0, 0.0, 450.0);
			const double Under = LitBy(O, P3(0.0, 0.0, 0.0)), Near = LitBy(O, P3(500.0, 0.0, 0.0)), Far = LitBy(O, P3(3000.0, 0.0, 0.0));
			Check(std::fabs(Under - 0.5) < 1e-9 && Near < Under && Far < 0.03, F("its light on the sea: %.2f under it, %.2f 500 m off, %.3f 3 km off", Under, Near, Far));
			O.bBurning = false;
			Check(LitBy(O, P3()) == 0.0, "a burnt-out flare lights nothing");
		}
		Check(SkyLit(false, 0.0, 0.0, 0.0) == 1.0, "a recording without its light is a day's");
		Check(SkyLit(true, 30.0, -10.0, 0.0) == 1.0, "by day, the sea lit");
		const double Moon = SkyLit(true, -24.0, 22.0, 0.45);
		Check(Moon > 0.03 && Moon < 0.15, F("the stand-in convoy's night (sun -24, a 45 per cent moon 22 degrees up): %.3f", Moon));
		Check(SkyLit(true, -24.0, -5.0, 1.0) == 0.0, "moonless, dark");
	}

	void TestEnds()
	{
		const std::vector<BoilShot> Crush = CrushPlan(41);
		Check(Crush.size() >= 6 && Crush.size() <= 10, F("a crushed boat: a hump and boils after it, %.0f churns", static_cast<double>(Crush.size())));
		double First = 1e9, Last = 0.0;
		int Hump = 0;
		for (const BoilShot& B : Crush)
		{
			First = std::min(First, B.Dt);
			Last = std::max(Last, B.Dt);
			Check(std::hypot(B.Dx, B.Dy) <= 20.0 + 1e-9, "her air comes up over where she was (20 m)");
		}
		for (const BoilShot& B : Crush) Hump += B.Dt < First + 1.0 ? 1 : 0;
		Check(First >= 2.0 && Last <= 30.0, F("the hump after %.1f s, the last boil by %.1f s", First, Last));
		Check(Hump >= 3, "the hump: several boils at once");

	}

	/**
	 * A rescue: the men in the water are the sea scene's, and so is who climbs a rescuer's side, where and how high
	 * (SeaShow.cpp RunWrecks); the looks hang the nets they climb. Given the game's source folder, the scene's numbers are
	 * read from its text, so a change there fails here.
	 */
	/** A section is made afresh only when its shape changes (the pre-check's note on the cost of the looks' mesh). */
	void TestMeshShape()
	{
		MeshShape Shape;
		const std::vector<int> Net = { 0, 1, 2, 0, 2, 3, 4, 5, 6 };
		Check(!Shape.Holds(7, Net.data(), static_cast<int>(Net.size())), "mesh: nothing kept, so the first is made");
		Shape.Keep(7, Net.data(), static_cast<int>(Net.size()));
		Check(Shape.Holds(7, Net.data(), static_cast<int>(Net.size())), "mesh: the same points and triangles hold the shape: only sent on");
		std::vector<int> Turned = Net;
		std::swap(Turned[1], Turned[2]);
		Check(!Shape.Holds(7, Turned.data(), static_cast<int>(Turned.size())), "mesh: a triangle turned over is another shape");
		Check(!Shape.Holds(8, Net.data(), static_cast<int>(Net.size())), "mesh: another count of points is another shape");
		Check(!Shape.Holds(7, Net.data(), 6), "mesh: fewer triangles is another shape");
		Shape.Forget();
		Check(!Shape.Holds(7, Net.data(), static_cast<int>(Net.size())), "mesh: forgotten (the section cleared), it is made afresh");
		Shape.Keep(0, nullptr, 0);
		Check(Shape.Holds(0, nullptr, 0) && !Shape.Holds(3, Net.data(), 3), "mesh: an empty shape holds only for an empty section");
	}

	void TestRescue(const std::string& Root)
	{
		Check(StandsBy(2.9) && !StandsBy(3.0), "a rescuer stands by under 3 m/s, as the scene has it");
		// A destroyer heading east (her starboard side to the south), 100 m by 10, stopped.
		RescuerIn R;
		R.At = P3(1000.0, 2000.0, 0.0);
		R.Heading = 90.0;
		R.Long = 100.0;
		R.Beam = 10.0;
		R.Speed = 0.5;
		auto Climber = [](double Along, double Side)
		{
			ManIn M;
			M.At = P3(1000.0 + Along, 2000.0 - Side * 5.3, 2.0);
			M.Doing = ManDoing::Climb;
			return M;
		};
		auto Swimmer = [](double X, double Y, double ToX, double ToY)
		{
			ManIn M;
			M.At = P3(X, Y, 0.0);
			M.Heading = std::atan2(ToX - X, ToY - Y);
			M.Doing = ManDoing::Swim;
			return M;
		};
		double Along = 0.0, Side = 0.0;
		bool bClimbing = false;
		Check(ClimbPlace(R, Climber(20.0, 1.0), Along, Side, bClimbing) && bClimbing && std::fabs(Along - 20.0) < 1e-9 && Side == 1.0, "a man of the scene's up her starboard side, 20 m forward");
		Check(ClimbPlace(R, Climber(-30.0, -1.0), Along, Side, bClimbing) && Side == -1.0, "and one up her port side");
		Check(!ClimbPlace(R, Climber(40.0, 1.0), Along, Side, bClimbing), "none climbs beyond her waist (0.35 of her length from amidships)");
		// (the scene points a man swimming for her at his place on her side, 1 m out from it)
		Check(ClimbPlace(R, Swimmer(1000.0, 1964.0, 990.0, 1994.0), Along, Side, bClimbing) && !bClimbing && std::fabs(Along + 10.0) < 1e-6 && Side == 1.0,
		      F("one swimming for her side, 30 m off, reaches it where he is making for (%.2f m along)", Along));
		Check(!ClimbPlace(R, Swimmer(1000.0, 1964.0, 1000.0, 1900.0), Along, Side, bClimbing), "one swimming away from her: none");
		Check(!ClimbPlace(R, Swimmer(1000.0, 1880.0, 990.0, 1994.0), Along, Side, bClimbing), "one swimming for her still more than 60 m off: not yet");
		Check(!ClimbPlace(R, Swimmer(1000.0, 1964.0, 1080.0, 1994.0), Along, Side, bClimbing), "one swimming past her bow: none");

		// Her nets, at stations 5 m apart from amidships: one hung where the swimmer will climb, down in 2 s; a man
		// climbing beside it is on it; one further along gets the next station's, alongside it; one at the end of a
		// station's net takes the next too.
		std::vector<NetHung> Nets;
		std::vector<ManIn> Men = { Swimmer(1000.0, 1964.0, 990.0, 1994.0) };
		StepNets(Nets, R, Men, true, 0.1);
		Check(Nets.size() == 1 && OnNets(Nets, 1.0, -10.0, 0.0) && Nets[0].Out > 0.0 && Nets[0].Out < 0.1, "standing by, a net goes over where a man swims for her");
		for (int I = 0; I < 20; ++I) StepNets(Nets, R, Men, true, 0.1);
		Check(Nets.size() == 1 && Nets[0].Out == 1.0, F("down her side in %.0f s", NetDropFor()));
		// (one 2 m off her side, his heading through the water meeting it 14.4 m forward while she still has way on her: a
		// net there, and one at his place along her now, 12 m forward, where the scene may bring him to it)
		std::vector<NetHung> Close;
		StepNets(Close, R, { Swimmer(1012.0, 1992.0, 1014.4, 1994.0) }, true, 0.1);
		Check(Close.size() == 2 && OnNets(Close, 1.0, 14.4, 0.0) && OnNets(Close, 1.0, 12.0, 0.0),
		      F("one close to her side: a net where his heading meets it and one where he is along her now (%.0f nets)", static_cast<double>(Close.size())));
		Men = { Climber(-8.5, 1.0), Climber(20.0, 1.0), Climber(-7.0, 1.0), Climber(-7.4, 1.0) };
		StepNets(Nets, R, Men, true, 0.1);
		Check(Nets.size() == 3, F("men on it, at the next station and 20 m forward: three nets (%.0f)", static_cast<double>(Nets.size())));
		bool bBeside = false;
		for (const NetHung& N : Nets) bBeside = bBeside || (N.Side == 1.0 && std::fabs(N.Along + 5.0) < 1e-9);
		Check(bBeside, "the one just aft of it alongside it, not over it");
		for (const ManIn& M : Men)
		{
			ClimbPlace(R, M, Along, Side, bClimbing);
			Check(OnNets(Nets, Side, Along, 0.0), F("each man climbing her side is on her nets (%.1f m along)", Along));
		}
		Check(!OnNets(Nets, 1.0, 0.0, 0.0) && !OnNets(Nets, -1.0, -8.5, 0.0) && !OnNets(Nets, 1.0, 22.6, 0.0), "and a place no net is at, or just past a net's end, is not");
		// Under way (the scene's men give her up): the nets she has no one on come in, the one a man is still on stays.
		R.Speed = 5.0;
		Men = { Climber(20.0, 1.0), Swimmer(1000.0, 1964.0, 990.0, 1994.0) };
		for (int I = 0; I < 25; ++I) StepNets(Nets, R, Men, true, 0.1);
		Check(Nets.size() == 1 && OnNets(Nets, 1.0, 20.0, 1.0), F("under way: only the net a man is still on stays out (%.0f nets)", static_cast<double>(Nets.size())));
		Men.clear();
		for (int I = 0; I < 25; ++I) StepNets(Nets, R, Men, true, 0.1);
		Check(Nets.empty(), "and once he is aboard, it comes in");
		// Stopped again with her rescue's time over: none goes over for a man swimming for her.
		R.Speed = 0.0;
		Men = { Swimmer(1000.0, 1964.0, 990.0, 1994.0) };
		StepNets(Nets, R, Men, false, 0.1);
		Check(Nets.empty(), "her rescue's time over: no net");

		// A net's ropes: from her rail at the scene's deck height into the sea, close against her side where the scene's
		// men climb it (0.3 m out), 5 m wide, on the side asked; halfway down, halfway.
		NetIn In;
		In.Ship = P3(1000.0, 2000.0, 0.0);
		In.Heading = 90.0;
		In.Beam = 10.0;
		In.Free = FreeOf("dd");
		In.Side = 1.0;
		In.Along = -10.0;
		In.Out = 1.0;
		const std::vector<Rope> Net = NetOf(In);
		double Top = -99.0, Foot = 99.0, Wide = 0.0, OutMost = 0.0, OutLeast = 99.0;
		for (const Rope& L : Net)
		{
			for (const P3& P : { L.From, L.To })
			{
				Top = std::max(Top, P.Z);
				Foot = std::min(Foot, P.Z);
				Wide = std::max(Wide, std::fabs(P.X - 990.0));
				OutMost = std::max(OutMost, 2000.0 - P.Y - 5.0);
				OutLeast = std::min(OutLeast, 2000.0 - P.Y - 5.0);
			}
		}
		Check(Net.size() >= 30 && Net.size() <= 45, F("a scrambling net of rope: %.0f lengths (each a rod drawn: a few dozen)", static_cast<double>(Net.size())));
		Check(std::fabs(Top - 5.0) < 1e-9 && Foot < 0.0 && Foot > -1.0, F("from her rail (%.1f m, the scene's height of a destroyer's deck) into the sea (%.1f m)", Top, Foot));
		Check(OutLeast > 0.0 && OutMost <= 0.3 + 1e-9, F("on her starboard side, between it and the men climbing it (%.2f to %.2f m out)", OutLeast, OutMost));
		Check(Wide <= 2.5 + 1e-9, F("5 m wide: %.1f m each way", Wide));
		In.Out = 0.5;
		double HalfFoot = 99.0;
		for (const Rope& L : NetOf(In)) HalfFoot = std::min({ HalfFoot, L.From.Z, L.To.Z });
		Check(std::fabs(HalfFoot - (5.0 - 0.5 * 5.4)) < 1e-6, F("halfway down: its foot at %.2f m", HalfFoot));
		In.Out = 0.0;
		Check(NetOf(In).empty(), "in: nothing");
		In.Out = 1.0;
		In.Side = -1.0;
		double North = 0.0;
		for (const Rope& L : NetOf(In)) North = std::max(North, L.From.Y - 2000.0);
		Check(North >= 5.0, "on her port side when asked");

		// A boat's deck blown and riding high, against the scene's height of it (its men go over her side from there).
		for (const char* Nation : { "GER", "", "UK", "JAP" })
		{
			const BoatModel M = ModelOf(Nation);
			const Drawn D = DrawnOf(M, 95.0, 8.3);
			Check(std::fabs(FreeOf("ss") - M.Deck * D.Up) <= 0.35, std::string("[") + Nation + "] " + F("a boat riding high: her deck %.2f m out, the scene's men go over it from %.2f m", M.Deck * D.Up, FreeOf("ss")));
		}
		// (and the battle modeller's boats, which the stage draws now: the scene takes her casing by her navy)
		for (const char* Nation : { "GER", "", "USA", "UK", "JAP" })
		{
			const std::string Nat(Nation);
			// (as the stage's ShipFileOf: "Submarine_" + her navy where the game has it, else the class's own, no navy too)
			const BoatModel M = SculptedOf(Nat == "USA" ? "Submarine_USA" : Nat == "GER" ? "Submarine_GER" : "Submarine");
			Check(std::fabs(BoatFreeOf(Nat) - M.Deck) <= 0.1, "[" + Nat + "] " + F("the battle modeller's boat riding high: her casing %.2f m out, the scene's men go over it from %.2f m", M.Deck, BoatFreeOf(Nat)));
		}
		if (Root.empty())
		{
			std::printf("(no source folder given: the rescue not read against the sea scene's code)\n");
			return;
		}
		const std::string Show = TextOf(Root + "/Battle/SeaShow.cpp");
		Check(!Show.empty(), "Battle/SeaShow.cpp read");
		// (the scene's deck heights: CompanyOf's fifth figure, Free)
		const size_t Company = Show.find("Company CompanyOf(const std::string& Class)");
		Check(Company != std::string::npos, "the sea scene's CompanyOf, a ship's company and her deck's height by class");
		for (const char* Class : { "bb", "cv", "ca", "cl", "de", "ss", "mv" })
		{
			const std::string Key = std::string("if (Class == \"") + Class + "\") return { ";
			const size_t At = Company == std::string::npos ? std::string::npos : Show.find(Key, Company);
			double Free = -1.0;
			if (At != std::string::npos)
			{
				double A = 0.0, B = 0.0, C = 0.0, E = 0.0;
				std::sscanf(Show.c_str() + At + Key.size(), "%lf, %lf, %lf, %lf, %lf", &A, &B, &C, &E, &Free);
			}
			Check(std::fabs(Free - FreeOf(Class)) < 1e-9, std::string("the scene's deck height for ") + Class + F(": %.1f m, the looks' %.1f", Free, FreeOf(Class)));
		}
		Check(Show.find("return { 300, 6, 1, 30, 5.0, 1.0 }; // a destroyer") != std::string::npos && FreeOf("dd") == 5.0, "and any other (a destroyer's): 5.0 m");
		for (const char* Said : {
			     "double FreeOf(const std::string& Class) { return CompanyOf(Class).Free; }",
			     "double BoatFreeOf(const std::string& Nation) { return Nation == \"USA\" ? 2.6 : Nation == \"GER\" ? 1.8 : 2.0; }",
			     "W.Free = H.Class == \"ss\" ? BoatFreeOf(H.Nation) : C.Free;",
			     "if (By && (By->Doing == Fate::Gone || By->Spd >= 3.0 || Flat(By->P - M.O.At) > 900.0)) By = nullptr;",
			     "const double Along = Clamp(Rel.Dot(Ahead(By->Hdg)), -0.35 * Sz.Long, 0.35 * Sz.Long);",
			     "M.Off = Vec(Along, Side * (0.5 * Sz.Beam + 1.0), 0.0);",
			     "M.Off = Vec(M.Off.X, M.Off.Y > 0.0 ? 0.5 * SizeOf(By->Class).Beam + 0.3 : -(0.5 * SizeOf(By->Class).Beam + 0.3), Up);",
			     "M.O.At.Z = FreeOf(By->Class) * Clamp(",
			     "M.O.Drop = W.bLeft && Mix(S + 37) < 0.6 ? 0.8 : std::max(0.6, W.Free * (1.0 - 0.8 * Down));",
			     "if (Class == \"ss\") return { 55, 1, 0, 10, 2.5, 0.8 };",
			     "const double Half = 0.35 * SizeOf(By->Class).Long, Step = (M.O.Key & 1) ? 1.2 : -1.2;",
		     })
		{
			Check(Show.find(Said) != std::string::npos, std::string("the sea scene's rescue still has: ") + Said);
		}
		// (and the boat's end is the scene's whole: her men, her raft, her wreckage and her oil, by how she went)
		Check(Show.find("if (How == \"scuttled\") return { 0.95, 0.02, 0.02, 0.5 };") != std::string::npos && Show.find("if (How == \"crushed\") return { 0.0, 0.0, 0.0, 0.0 };") != std::string::npos,
		      "a boat scuttled leaves most of her men in the water, one crushed none: the scene's");
	}

	/** A flat array of numbers in a model file's text: the first "Key":[ at or after From (empty where there is none). */
	std::vector<double> NumbersOf(const std::string& Text, const std::string& Key, size_t From)
	{
		std::vector<double> Out;
		const size_t At = Text.find("\"" + Key + "\":[", From);
		if (At == std::string::npos) return Out;
		const char* P = Text.c_str() + At + Key.size() + 4;
		while (*P && *P != ']')
		{
			char* End = nullptr;
			const double V = std::strtod(P, &End);
			if (End == P) break;
			Out.push_back(V);
			P = End;
			while (*P == ',' || *P == ' ') ++P;
		}
		return Out;
	}

	/** Where a part of a model file's "parts" begins in its text (its name), and its place in the list; npos and -1 where
	 * it has none. */
	size_t PartAt(const std::string& Text, const std::string& Name, int& OutIndex)
	{
		OutIndex = -1;
		const size_t Start = Text.find("\"parts\":[");
		const size_t Stop = Start == std::string::npos ? std::string::npos : Text.find("}]", Start);
		int Index = 0;
		for (size_t At = Text.find("{\"name\":\"", Start); Start != std::string::npos && At != std::string::npos && At < Stop; At = Text.find("{\"name\":\"", At + 1), ++Index)
		{
			if (Text.compare(At + 9, Name.size() + 1, Name + "\"") == 0)
			{
				OutIndex = Index;
				return At;
			}
		}
		return std::string::npos;
	}

	/**
	 * A boat's casing where her crew stand when she strikes (BoatModel's casing rows, which SeaShow::DeckOf mirrors and
	 * Tools/convoy_looks/convoy_looks_show holds to them): against each of the battle modeller's files, the highest surface
	 * facing up (not of her periscopes or her deck gun) under every station of the stretches her men stand on, half a
	 * metre apart along her and a tenth across from 0.85 m (the least the show puts a man out) to her casing's edge, both
	 * sides, within 0.1 m of CasingAt; each stretch clear of her bridge by half a metre; her deck gun's length along her
	 * against its mount's and barrel's own points.
	 */
	void TestCasing(const std::string& Root)
	{
		if (Root.empty())
		{
			return;
		}
		const std::string Content = Root + "/../../Content/Models/Ships/";
		for (const char* File : { "Submarine_GER", "Submarine_USA", "Submarine" })
		{
			const BoatModel M = SculptedOf(File);
			const std::string N = std::string("[") + File + " casing] ";
			const std::string Model = TextOf(Content + File + ".json");
			int ScopeOne = -1, ScopeTwo = -1, Mount = -1, Barrel = -1;
			PartAt(Model, "periscope.1", ScopeOne);
			PartAt(Model, "periscope.2", ScopeTwo);
			PartAt(Model, "turret.A", Mount);
			PartAt(Model, "turret.A.gun.1", Barrel);
			const size_t Near = Model.find("\"near\":{");
			const std::vector<double> Points = NumbersOf(Model, "points", Near), Corners = NumbersOf(Model, "triangles", Near), Parts = NumbersOf(Model, "part", Near);
			const bool bRead = Near != std::string::npos && !Parts.empty() && Points.size() == Parts.size() * 3 && !Corners.empty() && Corners.size() % 3 == 0 && ScopeOne >= 0 &&
			                   ScopeTwo >= 0 && Mount >= 0 && Barrel >= 0;
			Check(bRead, N + "her near mesh, its parts, her periscopes and her deck gun read");
			if (!bRead)
			{
				continue;
			}
			auto Fitting = [&](size_t K)
			{
				const int Part = static_cast<int>(Parts[K]);
				return Part == ScopeOne || Part == ScopeTwo || Part == Mount || Part == Barrel;
			};
			// Her deck gun along her: its mount's and its barrel's points, stowed.
			double GunLo = 1e9, GunHi = -1e9;
			for (size_t K = 0; K < Parts.size(); ++K)
			{
				const int Part = static_cast<int>(Parts[K]);
				if (Part == Mount || Part == Barrel)
				{
					GunLo = std::min(GunLo, Points[K * 3]);
					GunHi = std::max(GunHi, Points[K * 3]);
				}
			}
			Check(std::fabs(GunLo - M.GunFrom) < 0.05 && std::fabs(GunHi - M.GunTo) < 0.05, N + F("her deck gun from %.2f to %.2f m along her (the row's %.2f", GunLo, GunHi, M.GunFrom) + F(" to %.2f)", M.GunTo));
			// (her surfaces facing up, as flat triangles in plan; under each station the highest under Cap, as the casing's)
			struct Facing
			{
				double X[3], Y[3], Z[3];
				double Lo, Hi, Left, Right;
			};
			std::vector<Facing> Faces;
			for (size_t T = 0; T + 2 < Corners.size(); T += 3)
			{
				const size_t A = static_cast<size_t>(Corners[T]), B = static_cast<size_t>(Corners[T + 1]), C = static_cast<size_t>(Corners[T + 2]);
				if (A >= Parts.size() || B >= Parts.size() || C >= Parts.size() || Fitting(A) || Fitting(B) || Fitting(C)) continue;
				Facing Tri;
				for (int K = 0; K < 3; ++K)
				{
					const size_t V = K == 0 ? A : K == 1 ? B : C;
					Tri.X[K] = Points[V * 3];
					Tri.Y[K] = Points[V * 3 + 1];
					Tri.Z[K] = Points[V * 3 + 2];
				}
				// (a wall, seen from above, has no area)
				if (std::fabs((Tri.X[1] - Tri.X[0]) * (Tri.Y[2] - Tri.Y[0]) - (Tri.Y[1] - Tri.Y[0]) * (Tri.X[2] - Tri.X[0])) < 1e-9) continue;
				Tri.Lo = std::min({ Tri.X[0], Tri.X[1], Tri.X[2] });
				Tri.Hi = std::max({ Tri.X[0], Tri.X[1], Tri.X[2] });
				Tri.Left = std::min({ Tri.Y[0], Tri.Y[1], Tri.Y[2] });
				Tri.Right = std::max({ Tri.Y[0], Tri.Y[1], Tri.Y[2] });
				Faces.push_back(Tri);
			}
			const double Cap = M.Casing + 1.5;
			auto Surface = [&Faces, Cap](double X, double Y)
			{
				double Best = -1e9;
				for (const Facing& Tri : Faces)
				{
					if (X < Tri.Lo - 1e-9 || X > Tri.Hi + 1e-9 || Y < Tri.Left - 1e-9 || Y > Tri.Right + 1e-9) continue;
					const double D = (Tri.Y[1] - Tri.Y[2]) * (Tri.X[0] - Tri.X[2]) + (Tri.X[2] - Tri.X[1]) * (Tri.Y[0] - Tri.Y[2]);
					const double L0 = ((Tri.Y[1] - Tri.Y[2]) * (X - Tri.X[2]) + (Tri.X[2] - Tri.X[1]) * (Y - Tri.Y[2])) / D;
					const double L1 = ((Tri.Y[2] - Tri.Y[0]) * (X - Tri.X[2]) + (Tri.X[0] - Tri.X[2]) * (Y - Tri.Y[2])) / D;
					const double L2 = 1.0 - L0 - L1;
					if (L0 < -1e-6 || L1 < -1e-6 || L2 < -1e-6) continue;
					const double Z = L0 * Tri.Z[0] + L1 * Tri.Z[1] + L2 * Tri.Z[2];
					if (Z < Cap && Z > Best) Best = Z;
				}
				return Best;
			};
			int Stations = 0, Missing = 0;
			double Worst = 0.0, WorstAlong = 0.0, WorstAcross = 0.0;
			const double Stretches[2][2] = { { M.AftFrom, M.AftTo }, { M.ForeFrom, M.ForeTo } };
			for (const auto& Stretch : Stretches)
			{
				for (double Along = Stretch[0]; Along <= Stretch[1] + 1e-9; Along += 0.5)
				{
					for (double Out = 0.85; Out <= M.Edge + 1e-9; Out += 0.1)
					{
						for (const double Hand : { -1.0, 1.0 })
						{
							const double Z = Surface(Along, Hand * Out);
							if (Z < -1e8)
							{
								++Missing;
								continue;
							}
							++Stations;
							const double Off = std::fabs(CasingAt(M, Along, Hand * Out) - Z);
							if (Off > Worst)
							{
								Worst = Off;
								WorstAlong = Along;
								WorstAcross = Hand * Out;
							}
						}
					}
				}
			}
			Check(Stations >= 100 && Missing == 0 && Worst <= 0.1,
			      N + std::to_string(Stations) + " stations where her men stand (" + std::to_string(Missing) + " over no casing): CasingAt within " +
			          F("%.3f m of her deck (the worst %.1f m along, %.1f across; 0.1 or less)", Worst, WorstAlong, WorstAcross));
			Check(M.ForeFrom >= M.TowerFore + 0.5 && M.AftTo <= M.TowerAft - 0.5 && M.ForeTo > M.ForeFrom && M.AftTo > M.AftFrom && M.Edge > 0.85,
			      N + F("her stretches clear of her bridge by half a metre (forward from %.1f, aft from %.1f), out to %.2f m", M.ForeFrom, M.AftTo, M.Edge));
		}
		const BoatModel Built = ModelOf("GER");
		Check(CasingAt(Built, 10.0, 0.9) == Built.Deck && Built.EnsignAlong == Built.TowerAft && Built.EnsignTop > Built.TowerTop,
		      "the code-built boat: her casing her flat deck, her white flag at her bridge's after end");
	}

	/**
	 * A struck boat's white flag (FlagAt) and her vents (OpenVents, StepBoat's Vent): none before she strikes, run up from
	 * the foot of its 2 m staff to its head in its first seconds; streaming to leeward of the air over her, rippling, the
	 * same moment the same flag and another boat's in its own time; in a calm hanging down beside the staff, never under its
	 * foot (nor while it goes up); hauled down and gone. Its staff's foot on her as drawn, leaning aft as her bow rises.
	 * Scuttled, her vents' spurts a second and a half apart, side about, from forward aft along her saddle tanks, for
	 * VentFor seconds once, and nothing thrown out of the player's sight.
	 */
	/**
	 * The stage's sinking (AirStage::MoveShip's way "sink", its end "stern") as the looks reproduce it (StageSinking): its
	 * own lines read from its source, and the curve at a few moments: down by the stern, her trim 14 degrees at 35 s, her
	 * waterline her top and 40 m under at 70 s.
	 */
	void TestStageSinking(const std::string& Root)
	{
		if (!Root.empty())
		{
			const std::string Stage = TextOf(Root + "/Battle/AirStage.cpp");
			for (const char* Said : {
			         "const double Under = Ship.High + 40.0;",
			         "Pitch = 14.0 * Down * StageParts::Ease(T / 35.0);",
			         "Depth = Under * StageParts::EaseIn(T / 70.0);",
			         "const double Down = Ship.End == TEXT(\"bow\") ? 1.0 : Ship.End == TEXT(\"stern\") ? -1.0",
			         "return X * X * (3.0 - 2.0 * X);",
			         "// (a positive pitch lifts the model's +X, her stern: the bow goes down)",
			         "Ship->End = End;",
			     })
			{
				Check(Stage.find(Said) != std::string::npos, std::string("the stage's sinking still has: ") + Said);
			}
			const size_t In = Stage.find("static double EaseIn(double X)");
			Check(In != std::string::npos && Stage.find("return X * X;", In) != std::string::npos && Stage.find("return X * X;", In) < In + 120, "the stage's EaseIn is still X squared");
		}
		double Depth = 0.0, Pitch = 0.0;
		StageSinking(0.0, 10.0, true, Depth, Pitch);
		const bool bStill = Depth == 0.0 && Pitch == 0.0;
		StageSinking(35.0, 10.0, true, Depth, Pitch);
		const bool bHalf = std::fabs(Depth - 50.0 * 0.25) < 1e-9 && std::fabs(Pitch - 14.0) < 1e-9;
		StageSinking(70.0, 10.0, false, Depth, Pitch);
		const bool bGone = std::fabs(Depth - 50.0) < 1e-9 && Pitch == 0.0;
		Check(bStill && bHalf && bGone, "stage sinking: from her trim, by the stern 14 degrees at 35 s, her top 40 m under at 70 s (her look's own end: no trim)");
	}

	void TestFlagAndVents()
	{
		FlagIn In;
		In.Foot = P3(100.0, 200.0, 6.0);
		In.Up = UpOf(30.0, 0.0);
		In.Air = P3(6.0, 0.0, 0.0);
		In.Seed = 77;
		Check(!FlagAt(In, 10.0).bShown, "flag: none before she strikes");
		In.Since = 0.0;
		Check(!FlagAt(In, 10.0).bShown, "flag: none the moment she strikes, run up from there");
		In.Since = 0.5 * FlagRaiseFor();
		const FlagOut Going = FlagAt(In, 10.0);
		In.Since = 10.0;
		const FlagOut Flying = FlagAt(In, 10.0);
		auto Apart = [](const P3& A, const P3& B) { return std::sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y) + (A.Z - B.Z) * (A.Z - B.Z)); };
		Check(Flying.bShown && Apart(Flying.Head, In.Foot + P3(0.0, 0.0, FlagStaff())) < 1e-9 && FlagStaff() >= 2.0, F("flag: on a staff %.1f m high over her ensign's place", FlagStaff()));
		Check(Going.bShown && Going.Point[0][0].Z < Flying.Point[0][0].Z - 0.3 && std::fabs(Flying.Point[0][0].Z - Flying.Head.Z) < 1e-9,
		      F("flag: run up the staff, its head %.2f m under the staff's head halfway up, at it once hoisted", Flying.Point[0][0].Z - Going.Point[0][0].Z));
		double Leeward = 1e9;
		for (int J = 0; J < FlagDown; ++J) Leeward = std::min(Leeward, Flying.Point[J][FlagAlong - 1].X - Flying.Point[J][0].X);
		Check(Leeward > 0.8 * FlagFly(), F("flag: in a 6 m/s breeze it streams to leeward, its fly %.2f m out from the staff at the least", Leeward));
		const FlagOut Again = FlagAt(In, 10.0), Later = FlagAt(In, 10.3);
		bool bSame = true;
		double Moved = 0.0;
		for (int J = 0; J < FlagDown; ++J)
		{
			for (int I = 0; I < FlagAlong; ++I)
			{
				bSame = bSame && Apart(Again.Point[J][I], Flying.Point[J][I]) == 0.0;
				Moved = std::max(Moved, Apart(Later.Point[J][I], Flying.Point[J][I]));
			}
		}
		Check(bSame && Moved > 0.01 && Moved < 0.3, F("flag: the same moment the same flag; it ripples (%.2f m in 0.3 s at the most)", Moved));
		In.Seed = 78;
		Check(Apart(FlagAt(In, 10.0).Point[1][FlagAlong - 1], Flying.Point[1][FlagAlong - 1]) > 1e-4, "flag: another boat's ripples in its own time");
		In.Seed = 77;
		// (a calm, and a calm while it goes up: hanging down beside the staff, none of it under the staff's foot)
		for (const double Since : { 0.3, 10.0 })
		{
			In.Air = P3();
			In.Since = Since;
			const FlagOut Calm = FlagAt(In, 10.0);
			double Lowest = 1e9;
			for (int J = 0; J < FlagDown; ++J)
				for (int I = 0; I < FlagAlong; ++I) Lowest = std::min(Lowest, Calm.Point[J][I].Z - In.Foot.Z);
			Check(Calm.bShown && Lowest >= 0.15 - 1e-9 && (Since < 1.0 || Calm.Point[0][FlagAlong - 1].Z < Calm.Point[0][0].Z - 0.5),
			      F("flag: in a calm (%.1f s up) it hangs down beside the staff, never under its foot (%.2f m over it at the lowest)", Since, Lowest));
		}
		In.Air = P3(6.0, 0.0, 0.0);
		In.Since = 60.0;
		In.Lowered = 0.5 * FlagLowerFor();
		const FlagOut Coming = FlagAt(In, 10.0);
		In.Lowered = FlagLowerFor() + 0.1;
		Check(Coming.bShown && Coming.Point[0][0].Z < Flying.Point[0][0].Z - 0.3 && !FlagAt(In, 10.0).bShown, "flag: hauled down the staff, then gone");
		// Her staff's foot on her as the stage draws her: level, her heave and its height over her waterline; bow up, it comes
		// aft and lower (it stands abaft amidships), and the staff leans aft.
		const BoatModel Ger = SculptedOf("Submarine_GER");
		BoatIn Her;
		Her.Heading = 90.0;
		BoatOut Rode;
		Rode.Heave = 0.4;
		const P3 Level = EnsignAt(Ger, Drawn(), Her, Rode);
		Her.Pitch = 5.0;
		const P3 Tipped = EnsignAt(Ger, Drawn(), Her, Rode);
		const P3 Leans = UpOf(90.0, 5.0);
		Check(std::fabs(Level.X - Ger.EnsignAlong) < 1e-9 && std::fabs(Level.Y) < 1e-9 && std::fabs(Level.Z - 0.4 - Ger.EnsignTop) < 1e-9 && Tipped.X < Level.X - 0.3 && Tipped.Z < Level.Z &&
		          Leans.X < -0.05 && Leans.Z > 0.99,
		      F("flag: her staff's foot %.2f m along at %.2f m, %.2f m aft with her bow 5 degrees up", Level.X, Level.Z, Level.X - Tipped.X));

		// Her vents, scuttled after she struck: on a boat surfaced and riding high (her tanks blown as she struck), opened at
		// 5 s, the stage taking her down by the stern from then.
		for (const bool bShown : { true, false })
		{
			const BoatModel M = SculptedOf("Submarine_GER");
			BoatState S;
			Blow(S, true);
			std::vector<double> When, Along, Across;
			// (what the kit is called with for each: a jet straight up off her saddle tank while it is out of the sea, the
			// heights it leaves her at against the stage's sinking, and white water once it is under)
			int Jets = 0, Churns = 0, JetsLeaning = 0, JetsOff = 0, ChurnsOff = 0;
			double FirstZ = -99.0;
			// (her scuttling charges: when, where along her, how far out, how high, and what the kit is called with)
			std::vector<double> ChargeWhen, ChargeAlong, ChargeOut, PuffAlong, PuffZ;
			int ChargeCalls = 0, PuffCalls = 0;
			for (double T = 0.0; T < 45.0; T += Frame)
			{
				if (std::fabs(T - 5.0) < 0.5 * Frame) OpenVents(S, T);
				if (std::fabs(T - 35.0) < 0.5 * Frame) OpenVents(S, T);
				BoatIn Up;
				Up.T = T;
				Up.Dive = "surfaced";
				Up.Keel = 67.0 * 0.07;
				Up.Heading = 90.0;
				Up.bShown = bShown;
				for (const BoatSplash& Sp : StepBoat(S, Up, M, Drawn()).Splashes)
				{
					const SplashKit Kit = KitOf(Sp);
					if (Sp.Kind == SplashKind::Scuttle)
					{
						ChargeWhen.push_back(T);
						ChargeAlong.push_back(Sp.At.X);
						ChargeOut.push_back(std::fabs(Sp.At.Y));
						ChargeCalls += Kit.Call == KitCall::Spout && Kit.Row == "churn" && Kit.Where.Z == 0.0 ? 1 : 0;
					}
					if (Sp.Kind == SplashKind::Puff)
					{
						PuffAlong.push_back(Sp.At.X);
						PuffZ.push_back(Sp.At.Z);
						PuffCalls += Kit.Call == KitCall::Puff && Kit.Way.Z > 0.95 && Kit.Across > 0.3 && Kit.Seconds > 0.0 && std::fabs(Kit.Where.Z - Sp.At.Z) < 1e-12 ? 1 : 0;
					}
					if (Sp.Kind != SplashKind::Vent) continue;
					When.push_back(T);
					Along.push_back(Sp.At.X);
					Across.push_back(Sp.At.Y);
					if (FirstZ < -90.0) FirstZ = Sp.At.Z;
					// (where it leaves her, as the stage takes her down: her saddle tank's height on her as she rides and sinks)
					double Depth = 0.0, Pitch = 0.0;
					StageSinking(T - 5.0, M.Top, true, Depth, Pitch);
					const double Expect = M.Casing - 1.0 - Depth + Sp.At.X * std::sin(Pitch * Pi / 180.0) + (M.Casing - 1.0) * (std::cos(Pitch * Pi / 180.0) - 1.0);
					if (Sp.At.Z > 0.1)
					{
						++Jets;
						JetsLeaning += Kit.Call == KitCall::Jet && Kit.Way.Z > 0.95 && Kit.Across > 0.2 && Kit.Seconds >= 1.0 ? 0 : 1;
						JetsOff += std::fabs(Sp.At.Z - Expect) < 0.02 ? 0 : 1;
					}
					else
					{
						++Churns;
						ChurnsOff += Kit.Call == KitCall::Spout && Kit.Row == "churn" && Kit.Where.Z == 0.0 ? 0 : 1;
					}
				}
			}
			if (!bShown)
			{
				Check(When.empty() && ChargeWhen.empty() && PuffZ.empty(),
				      F("vents: out of the player's sight none thrown (%.0f), nor her charges' (%.0f)", static_cast<double>(When.size()), static_cast<double>(ChargeWhen.size() + PuffZ.size())));
				continue;
			}
			Check(std::fabs(FirstZ - (M.Casing - 1.0)) < 1e-9 && Jets >= 3 && JetsLeaning == 0 && JetsOff == 0,
			      F("vents: the first leaves her saddle tank %.2f m over the sea; %.0f out of the sea, each the kit's white jet straight up (%.0f not), at her saddle tank as the stage "
			        "takes her down",
			        FirstZ, Jets, JetsLeaning) +
			          F(" (%.0f off it)", JetsOff));
			Check(Churns >= 3 && ChurnsOff == 0, F("vents: once under, white water on the sea over them (%.0f, %.0f not)", Churns, ChurnsOff));
			Check(ChargeWhen.size() == 2 && PuffZ.size() == 2 && std::fabs(ChargeWhen[0] - 5.0 - ScuttleChargeAt(0)) < 1.5 * Frame &&
			          std::fabs(ChargeWhen[1] - 5.0 - ScuttleChargeAt(1)) < 1.5 * Frame && ScuttleChargeAt(0) >= 6.0 && ScuttleChargeAt(2) < 0.0,
			      F("charges: two, at %.1f s and %.1f s (her vents opened at 5 s)", ChargeWhen.empty() ? -1.0 : ChargeWhen[0], ChargeWhen.size() < 2 ? -1.0 : ChargeWhen[1]));
			if (ChargeWhen.size() == 2 && PuffZ.size() == 2)
			{
				Check(std::fabs(ChargeAlong[0] - M.AftHatch) < 1e-9 && std::fabs(ChargeAlong[1] - M.ForeHatch) < 1e-9 && std::fabs(PuffAlong[0] - M.AftHatch) < 1e-9 &&
				          std::fabs(PuffAlong[1] - M.ForeHatch) < 1e-9,
				      F("charges: the after hatch's first (%.1f m along her), then the fore one's (%.1f m)", ChargeAlong[0], ChargeAlong[1]));
				Check(ChargeOut[0] > 0.5 * M.Wide + 0.3 && ChargeOut[1] > 0.5 * M.Wide + 0.3 && ChargeOut[0] < 0.5 * M.Wide + 2.0 && ChargeCalls == 2,
				      F("charges: each a muffled heave of white water alongside, %.2f m and %.2f m out from her middle line (the kit's churn: no flash)", ChargeOut[0], ChargeOut[1]));
				Check(PuffZ[0] > 0.3 && PuffZ[1] > 0.3 && PuffCalls == 2, F("charges: their smoke out of the hatch over each, %.2f m and %.2f m over the sea, straight up (the kit's puff)", PuffZ[0], PuffZ[1]));
			}
			// (a picture run skipping past her charges: none thrown late)
			BoatState Skipped;
			Blow(Skipped, true);
			int Late = 0;
			for (const double T : { 0.0, 0.1, 0.2, 20.0, 20.1, 20.2 })
			{
				if (T == 0.1) OpenVents(Skipped, T);
				BoatIn Up;
				Up.T = T;
				Up.Dive = "surfaced";
				Up.Keel = 67.0 * 0.07;
				Up.Heading = 90.0;
				for (const BoatSplash& Sp : StepBoat(Skipped, Up, M, Drawn()).Splashes) Late += Sp.Kind == SplashKind::Scuttle || Sp.Kind == SplashKind::Puff ? 1 : 0;
			}
			Check(Late == 0 && Skipped.Charges == ScuttleCharges, F("charges: skipped past, none thrown late (%.0f)", Late));
			bool bSteady = !When.empty() && When.front() < 5.0 + Frame, bAbout = true, bAft = true, bOnHer = true;
			for (size_t K = 0; K < When.size(); ++K)
			{
				bOnHer = bOnHer && std::fabs(std::fabs(Across[K]) - 0.45 * M.Wide) < 1e-6 && std::fabs(Along[K]) <= 0.3 * M.Long + 1e-6;
				if (K == 0) continue;
				bSteady = bSteady && std::fabs(When[K] - When[K - 1] - 1.5) < 1.5 * Frame;
				bAbout = bAbout && Across[K] * Across[K - 1] < 0.0;
				bAft = bAft && Along[K] < Along[K - 1];
			}
			Check(bSteady && bAbout && bAft && bOnHer && When.back() < 5.0 + VentFor() && When.size() >= 15 && When.size() <= 18,
			      F("vents: %.0f spurts from %.1f s to %.1f s, a second and a half apart, side about along her saddle tanks from forward aft; opened once",
			        static_cast<double>(When.size()), When.empty() ? -1.0 : When.front(), When.empty() ? -1.0 : When.back()));
		}
	}

	/**
	 * A boat's deck gun and a merchant's guns, as the stage draws them trained (AirStage's TrainGun) and the scene mans them
	 * (GunCrewOf), against the battle modeller's files (the review of 23 November): each mount's row (MountOf) against its
	 * file (where it stands along her, the deck about it, where it lies stowed and how far it turns); the show's arcs
	 * (SeaShow.h's DeckGunArc and MerchantGunArc) against the models' own limits, and where the show stows each gun against
	 * the model's rest bearing; at every bearing the mount turns to, each man of its crew on its deck, clear of its mount
	 * and barrel as they are turned, apart from the others and, on a boat, off her bridge; his crew only while the gun is
	 * laid and she is up, crouching only for a moment after a shell; the scene's crew and acts pinned in its source and the
	 * sailor files' clips for them; a merchant's hit looks (the table's "ap" family: the review found "mv" gave none).
	 */
	void TestGuns(const std::string& Root)
	{
		if (Root.empty())
		{
			return;
		}
		const std::string Content = Root + "/../../Content/";
		const std::string ShowHeader = TextOf(Root + "/Battle/SeaShow.h");
		double DeckArc = 0.0, MerchantArc = 0.0;
		{
			const size_t At = ShowHeader.find("static constexpr double DeckGunArc = ");
			Check(At != std::string::npos && std::sscanf(ShowHeader.c_str() + At, "static constexpr double DeckGunArc = %lf, MerchantGunArc = %lf;", &DeckArc, &MerchantArc) == 2,
			      F("the show's arcs read: a boat's deck gun %.0f degrees either side of where it stows, a merchant's guns %.0f", DeckArc, MerchantArc));
		}
		const struct
		{
			const char* File;
			bool bStern, bBoat;
		} Mounts[] = {
			{ "Submarine_GER", false, true }, { "Submarine_USA", false, true }, { "Submarine", false, true },
			{ "Freighter", false, false }, { "Freighter", true, false }, { "Tanker", false, false },
			{ "Tanker", true, false }, { "Troopship", false, false }, { "Troopship", true, false },
		};
		for (const auto& Asked : Mounts)
		{
			const GunMount M = MountOf(Asked.File, Asked.bStern);
			const std::string N = std::string("[") + Asked.File + (Asked.bStern ? " stern gun] " : " gun] ");
			const std::string Model = TextOf(Content + "Models/Ships/" + Asked.File + ".json");
			const std::string Sockets = TextOf(Content + "Models/Ships/" + Asked.File + ".sockets.json");
			int MountIndex = -1, BarrelIndex = -1;
			const size_t MountAt = PartAt(Model, M.Part, MountIndex), BarrelAt = PartAt(Model, M.Part + ".gun.1", BarrelIndex);
			Check(M.File == Asked.File && M.Part == (Asked.bStern ? "turret.Y" : "turret.A") && MountIndex >= 0 && BarrelIndex >= 0, N + "a row, its mount and barrel in the model");
			if (M.File.empty() || MountIndex < 0 || BarrelIndex < 0)
			{
				continue;
			}
			const std::vector<double> Pivot = NumbersOf(Model, "pivot", MountAt), Limits = NumbersOf(Model, "limits", MountAt), Breech = NumbersOf(Model, "pivot", BarrelAt);
			// (the note first says a bearing is the "rest bearing plus the turn", then names it after a semicolon)
			const size_t Said = Model.find("; rest bearing ", MountAt);
			double RestSaid = -1.0;
			Check(Said != std::string::npos && Said < Model.find('}', MountAt) && std::sscanf(Model.c_str() + Said, "; rest bearing %lf", &RestSaid) == 1 && RestSaid == M.Rest,
			      N + F("the model's rest bearing %.0f, the row's %.0f", RestSaid, M.Rest));
			double MuzzleX = 0.0, MuzzleY = 0.0, MuzzleZ = 0.0;
			const size_t Named = Sockets.find("\"name\":\"muzzle." + M.Part + ".1\"");
			const size_t AtSaid = Named == std::string::npos ? std::string::npos : Sockets.find("\"at\":[", Named);
			const bool bMuzzle = AtSaid != std::string::npos && std::sscanf(Sockets.c_str() + AtSaid + 6, "%lf,%lf,%lf", &MuzzleX, &MuzzleY, &MuzzleZ) == 3;
			Check(Pivot.size() == 3 && Breech.size() == 3 && Limits.size() == 2 && bMuzzle, N + "its pivots, limits and muzzle read");
			if (Pivot.size() != 3 || Breech.size() != 3 || Limits.size() != 2 || !bMuzzle)
			{
				continue;
			}
			// (where it lies stowed: its muzzle's bearing from its barrel's pivot, x forward and y to starboard)
			const double Stowed = std::atan2(MuzzleY - Breech[1], MuzzleX - Breech[0]) * 180.0 / Pi;
			Check(std::fabs(std::remainder(Stowed - M.Rest, 360.0)) < 1.0 && M.Rest == (Asked.bStern || std::string(Asked.File) == "Submarine_USA" ? 180.0 : 0.0),
			      N + F("stowed at %.1f degrees off her bow, the row's %.0f (an American fleet boat's gun and a merchant's stern gun facing aft: the show's)", Stowed, M.Rest));
			const double ShowArc = Asked.bBoat ? DeckArc : MerchantArc;
			Check(std::fabs(Pivot[0] - M.Along) < 0.01 && Limits[0] == -M.Arc && Limits[1] == M.Arc && M.Arc == ShowArc,
			      N + F("its mount %.2f m along her (row %.2f), turning %.0f degrees either way, the row's and the show's %.0f", Pivot[0], M.Along, Limits[1]) +
			          F(" (%.0f)", ShowArc));

			// Her near mesh: its points, their facings and parts, its triangles; the deck about the mount (flat, facing up,
			// not of the gun), and how much of it lies at the row's height.
			const size_t Near = Model.find("\"near\":{");
			const std::vector<double> Points = NumbersOf(Model, "points", Near), Facings = NumbersOf(Model, "facings", Near);
			const std::vector<double> Corners = NumbersOf(Model, "triangles", Near), Parts = NumbersOf(Model, "part", Near);
			Check(Near != std::string::npos && Points.size() == Facings.size() && Points.size() == Parts.size() * 3 && Corners.size() % 3 == 0 && !Corners.empty(),
			      N + "her near mesh read");
			if (Points.size() != Parts.size() * 3 || Facings.size() != Points.size() || Corners.empty())
			{
				continue;
			}
			auto OfGun = [&](size_t K) { const int Part = static_cast<int>(Parts[K]); return Part == MountIndex || Part == BarrelIndex; };
			struct Flat
			{
				double X[3], Y[3], Z[3];
			};
			std::vector<Flat> Decks;
			double Area = 0.0;
			for (size_t T = 0; T + 2 < Corners.size(); T += 3)
			{
				const size_t A = static_cast<size_t>(Corners[T]), B = static_cast<size_t>(Corners[T + 1]), C = static_cast<size_t>(Corners[T + 2]);
				if (A >= Parts.size() || B >= Parts.size() || C >= Parts.size() || OfGun(A) || OfGun(B) || OfGun(C)) continue;
				if (Facings[A * 3 + 2] + Facings[B * 3 + 2] + Facings[C * 3 + 2] < 2.7) continue;
				Flat Tri;
				for (int K = 0; K < 3; ++K)
				{
					const size_t V = K == 0 ? A : K == 1 ? B : C;
					Tri.X[K] = Points[V * 3];
					Tri.Y[K] = Points[V * 3 + 1];
					Tri.Z[K] = Points[V * 3 + 2];
				}
				const double Cx = (Tri.X[0] + Tri.X[1] + Tri.X[2]) / 3.0 - Pivot[0], Cy = (Tri.Y[0] + Tri.Y[1] + Tri.Y[2]) / 3.0 - Pivot[1];
				const double Cz = (Tri.Z[0] + Tri.Z[1] + Tri.Z[2]) / 3.0;
				if (std::hypot(Cx, Cy) > 4.0) continue;
				Decks.push_back(Tri);
				if (std::hypot(Cx, Cy) > 0.6 && std::hypot(Cx, Cy) < 3.0 && std::fabs(Cz - M.Deck) <= 0.15)
				{
					Area += 0.5 * std::fabs((Tri.X[1] - Tri.X[0]) * (Tri.Y[2] - Tri.Y[0]) - (Tri.X[2] - Tri.X[0]) * (Tri.Y[1] - Tri.Y[0]));
				}
			}
			Check(Area >= 4.0 && M.Deck < Pivot[2], N + F("her deck by the mount %.2f m up: %.1f square metres of it, under the mount's foot (%.2f)", M.Deck, Area, Pivot[2]));
			// (the deck's height under a point, where a flat of it lies at the row's height there; -1e9 where none)
			auto DeckAt = [&Decks, &M](double X, double Y)
			{
				for (const Flat& Tri : Decks)
				{
					const double D = (Tri.Y[1] - Tri.Y[2]) * (Tri.X[0] - Tri.X[2]) + (Tri.X[2] - Tri.X[1]) * (Tri.Y[0] - Tri.Y[2]);
					if (std::fabs(D) < 1e-12) continue;
					const double L0 = ((Tri.Y[1] - Tri.Y[2]) * (X - Tri.X[2]) + (Tri.X[2] - Tri.X[1]) * (Y - Tri.Y[2])) / D;
					const double L1 = ((Tri.Y[2] - Tri.Y[0]) * (X - Tri.X[2]) + (Tri.X[0] - Tri.X[2]) * (Y - Tri.Y[2])) / D;
					const double L2 = 1.0 - L0 - L1;
					if (L0 < -1e-9 || L1 < -1e-9 || L2 < -1e-9) continue;
					const double Z = L0 * Tri.Z[0] + L1 * Tri.Z[1] + L2 * Tri.Z[2];
					if (std::fabs(Z - M.Deck) <= 0.3) return Z;
				}
				return -1e9;
			};
			// The gun's own points (its mount's and barrel's), the height of a man over the deck, to be turned with it.
			std::vector<double> GunX, GunY;
			for (size_t K = 0; K < Parts.size(); ++K)
			{
				if (OfGun(K) && Points[K * 3 + 2] > M.Deck + 0.05 && Points[K * 3 + 2] < M.Deck + 1.9)
				{
					GunX.push_back(Points[K * 3] - Pivot[0]);
					GunY.push_back(Points[K * 3 + 1] - Pivot[1]);
				}
			}
			const BoatModel Hers = Asked.bBoat ? SculptedOf(Asked.File) : BoatModel();
			int Placed = 0, OffDeck = 0, OnGun = 0, Crowded = 0, OnBridge = 0;
			double Closest = 1e9;
			for (double Turn = -M.Arc; Turn <= M.Arc + 1e-9; Turn += 5.0)
			{
				CrewIn In;
				In.bLaid = true;
				In.Dive = Asked.bBoat ? "surfaced" : "";
				In.Train = M.Rest + Turn;
				In.Key = 7;
				const std::vector<CrewMan> Crew = GunCrewOf(M, In);
				const double C = std::cos(Turn * Pi / 180.0), S = std::sin(Turn * Pi / 180.0);
				for (size_t K = 0; K < Crew.size(); ++K)
				{
					++Placed;
					const double X = M.Along + Crew[K].Fore, Y = Crew[K].Right;
					if (DeckAt(X, Y) < -1e8) ++OffDeck;
					double Gap = 1e9;
					for (size_t G = 0; G < GunX.size(); ++G)
					{
						// (+Turn carries her forward toward her starboard side: x cos - y sin, x sin + y cos)
						Gap = std::min(Gap, std::hypot(M.Along + GunX[G] * C - GunY[G] * S - X, Pivot[1] + GunX[G] * S + GunY[G] * C - Y));
					}
					Closest = std::min(Closest, Gap);
					if (Gap < 0.25) ++OnGun;
					for (size_t J = 0; J < K; ++J)
					{
						if (std::hypot(Crew[J].Fore - Crew[K].Fore, Crew[J].Right - Crew[K].Right) < 0.5) ++Crowded;
					}
					if (Asked.bBoat && X > Hers.TowerAft - 0.3 && X < Hers.TowerFore + 0.3) ++OnBridge;
				}
			}
			Check(Placed > 0 && OffDeck == 0 && OnGun == 0 && Crowded == 0 && OnBridge == 0,
			      N + std::to_string(Placed) + " men placed at every bearing it turns to: " + std::to_string(OffDeck) + " off its deck, " + std::to_string(OnGun) +
			          " in its mount or barrel (the nearest " + F("%.2f m", Closest) + "), " + std::to_string(Crowded) + " crowded, " + std::to_string(OnBridge) + " on her bridge (all 0)");
		}
		Check(MountOf("Submarine_ITA", false).File.empty() && MountOf("Submarine_GER", true).File.empty(), "no mount for a file the game does not have, nor a boat's stern gun");

		// Who mans it, and when (the show's bGunLaid: told on a ship she can fire at, she up, her casing clear).
		{
			const GunMount M = MountOf("Submarine_GER", false);
			CrewIn In;
			In.bLaid = true;
			In.Dive = "surfaced";
			In.Train = 40.0;
			In.T = 100.0;
			In.Key = 1920;
			const std::vector<CrewMan> Up = GunCrewOf(M, In);
			bool bFacing = Up.size() == static_cast<size_t>(CrewMost), bClips = true;
			for (const CrewMan& Man : Up)
			{
				bFacing = bFacing && Man.Facing == In.Train && !Man.bDuck;
				bClips = bClips && Man.Clip >= 0.0 && Man.Clip < 4.0;
			}
			Check(bFacing && bClips, "a boat's gun laid, she up: its crew of four at it, facing as it bears, standing, each at his own place in his clip");
			CrewIn Down = In;
			Down.Dive = "periscope";
			CrewIn Awash = In;
			Awash.Dived = 1.5;
			CrewIn Dropped = In;
			Dropped.bLaid = false;
			Check(GunCrewOf(M, Down).empty() && GunCrewOf(M, Awash).empty() && GunCrewOf(M, Dropped).empty(),
			      "none at it from the diving alarm, awash, or once her gun is dropped");
			CrewIn Merchant = In;
			Merchant.Dive.clear();
			Check(GunCrewOf(MountOf("Freighter", true), Merchant).size() == static_cast<size_t>(CrewMost), "a merchant's gun laid: its crew at it");
			// (a shell close aboard at 100 s: each crouches within DuckLate of it, for DuckFor, and stands again)
			int Before = 0, Soon = 0, During = 0, After = 0;
			for (const double At : { 99.9, 100.05, 100.0 + DuckLate + 0.01, 100.0 + DuckFor - 0.01, 100.0 + DuckLate + DuckFor + 0.01 })
			{
				CrewIn Shaken = In;
				Shaken.ShakenAt = 100.0;
				Shaken.T = At;
				int Ducked = 0;
				for (const CrewMan& Man : GunCrewOf(M, Shaken)) Ducked += Man.bDuck ? 1 : 0;
				if (At < 100.0) Before = Ducked;
				else if (At < 100.1) Soon = Ducked;
				else if (At < 100.0 + DuckFor) During += Ducked;
				else After = Ducked;
			}
			Check(Before == 0 && Soon < CrewMost && During == 2 * CrewMost && After == 0,
			      F("a shell close aboard: %.0f crouched before it, %.0f at once (each as he hears it)", Before, Soon) + F(", all through it (%.0f of 8), none after (%.0f)", During, After));
			const std::vector<CrewMan> Again = GunCrewOf(M, In);
			bool bSame = Again.size() == Up.size();
			for (size_t K = 0; bSame && K < Up.size(); ++K) bSame = Again[K].Fore == Up[K].Fore && Again[K].Right == Up[K].Right && Again[K].Clip == Up[K].Clip;
			Check(bSame, "the same crew for the same frame (the look's own hash, no dice of the battle's)");
		}

		// The scene: the crew drawn from each ship's watch (Battle/DeckCrewCore, stepped every frame from what the show says
		// of her) on the deck the stage gives, at their gun clips, crouched by the files' cower, going to it and from it by
		// climb_net, run and idle; none on a boat the show hides; a boat's hits' looks put out while she is told down or
		// under (as the sea show's check has it); a merchant's hits by the table's "ap" family. These pins were the scene's
		// "ConvoyLook::GunCrewOf(Gun, Told)", "Stage->GunDeck(Ship.Key, GunPart, Gunner.Fore, Gunner.Right, Gun.Deck, Feet,
		// Her)", the idle or cower act and the key "-(Ship.Key * ConvoyLook::CrewMost + K + 1)" until 24 November (Brian: the
		// files' own gun clips): changed on purpose. The crew's own checks are TestCrew's.
		const std::string Scene = TextOf(Root + "/Battle/SeaBattle.cpp"), Sailors = TextOf(Root + "/Battle/SeaSailors.cpp");
		Check(Scene.find("DeckCrew::Step(Crewed, Said, Mounted.Shape, Mounted.Mount, Kit->second, Now, &GunCues);") != std::string::npos &&
		          Scene.find("if (!DeckCrew::Drawn(Is, Ship))") != std::string::npos &&
		          Scene.find("Stage->GunDeck(Ship.Key, Mounted.Part, Is.At.Root.Fore, Is.At.Root.Right, Is.At.Root.Up, FeetAt, HerTurn)") != std::string::npos &&
		          Scene.find("Gunner.Turn = HerTurn * FQuat(FVector::UpVector, FMath::DegreesToRadians(Is.At.Facing));") != std::string::npos &&
		          Scene.find("Gunner.Mine = -(Ship.Key * DeckCrew::Roles + Who + 1);") != std::string::npos &&
		          Scene.find("Nearest.Add(TPair<double, int32>(Gunner.Pixels, Gunner.Mine));") != std::string::npos &&
		          Scene.find("const int32 BankPose = S.Crew.PoseOf(CrewFile, Is.What, Is.Time);") != std::string::npos &&
		          Scene.find("ConvoyLook::GunCrewOf") == std::string::npos &&
		          Sailors.find("case Act::Stand: return \"idle\";") != std::string::npos && Sailors.find("case Act::Duck: return \"cower\";") != std::string::npos,
		      "the scene mans each gun from its watch on its deck (DeckCrew), drawn only where the show draws her, keyed apart from the men in the water");
		// (what they work with drawn where their clips have it, their cues heard at their hands, through the scene's own sound
		// path in the mix's "body" family: silent where the sound is not brought in)
		const std::string Mix = TextOf(Content + "Data/Sound/mix.json");
		Check(Scene.find("DrawGunWheel(NearMesh, ManAt, UnitsPerMetre, Handwheel, Turned, Fitted.ShaftAhead, Fitted.ShaftIn, bLayer);") != std::string::npos &&
		          Scene.find("DeckCrew::LoaderRound(GunKit, ShownTime, Fitted.bBoat, MouthAt, BoreAt, InHands)") != std::string::npos &&
		          Scene.find("SeaSailors::RoundAlong(GunKit.Passing, GunKit.Long[PasserRole], ShownTime, Handed)") != std::string::npos &&
		          Scene.find("Sound(FName(UTF8_TO_TCHAR(Heard.Name.c_str())), CueAt[Whose], TEXT(\"body\"), 1.0f, false);") != std::string::npos &&
		          Scene.find("UE_LOG(LogTheater, Display, TEXT(\"Sea battle: the sound %s is not in /Game/Campaign/Sounds/Real") != std::string::npos &&
		          Mix.find("\"body\": {") != std::string::npos,
		      "the scene draws the wheels, the sights and the rounds where the clips have them, and plays the crew's cues at their hands (the mix's body family)");
		// (the one hatch drawn, her own, where her crew come up and the passer stands; no opening that moves with him (the
		// review of 24 November); the loader's rounds from a rack where no passer hands them up)
		Check(Scene.find("bScuttleOpen") == std::string::npos && Scene.find("const DeckCrew::Spot HatchSpot = DeckCrew::HatchOf(Fitted, Mounted.Mount);") != std::string::npos &&
		          Scene.find("Manned.bRack = !Crewed.bHanding;") != std::string::npos && Scene.find("DrawGunRack(NearMesh, ManAt, UnitsPerMetre, GunKit);") != std::string::npos &&
		          Scene.find("if (!Manned.bRack || !DeckCrew::RackRound(GunKit, ShownTime, Racked))") != std::string::npos,
		      "the scene opens her own hatch only (none moving with the passer), and draws the loader's next round on a rack where no passer hands it up");
		// (a merchant has no hatch by her gun: none open while her gun's crew are at it, nor one drawn where its place was
		// never set, at the board's middle; the review of 25 November: a stray hatch on every frame a merchant's gun was
		// manned)
		Check(Scene.find("Manned.bHatchOpen = true;") == std::string::npos && Scene.find("Manned.bHatchOpen = Mounted.Shape.bBoat;") != std::string::npos &&
		          Scene.find("if (Fitted.bBoat && Manned.bHatchOpen)") != std::string::npos &&
		          Scene.find("if (Manned.bHatchOpen && Manned.Mounted != nullptr && Manned.Mounted->Shape.bBoat)\n\t\t\t{\n\t\t\t\tDrawGunHatch(NearMesh, Manned.Hatch, UnitsPerMetre, Manned.HatchAway);") != std::string::npos,
		      "the scene opens a hatch for a boat's gun's crew only: none drawn for a merchant's (none at the board's middle)");
		// (hands up: a struck boat's man giving up by the files' hands_up where his file has it, else standing, as before)
		Check(Sailors.find("case Act::HandsUp: return \"hands_up\";") != std::string::npos &&
		          Sailors.find("const Act Up = Has(K, Act::HandsUp) ? Act::HandsUp : Act::Stand;") != std::string::npos,
		      "a struck boat's crew put their hands up by the files' hands_up, standing (idle) where a file lacks it");
		// (the stage's ship models, as TestCrew's guns and the show's checks take them: a boat's by her navy, a merchant's by
		// her key's roll)
		const std::string Staged = TextOf(Root + "/Battle/AirStage.cpp");
		Check(Staged.find("Kind = Roll < 13u ? TEXT(\"Freighter\") : Roll < 18u ? TEXT(\"Tanker\") : TEXT(\"Troopship\");") != std::string::npos &&
		          Staged.find("const uint32 Roll = (static_cast<uint32>(Given.Id) * 2654435761u >> 16) % 20u;") != std::string::npos,
		      "a merchant's model by her key's roll (Freighter, Tanker, Troopship), as the show's crew checks take it");
		for (const char* Navy : { "GER", "ITA", "JAP", "SOV", "UK", "USA" })
		{
			const std::string Men = TextOf(Content + "Models/Soldiers/soldier_" + Navy + "_sailor.json");
			bool bBoth = !Men.empty();
			for (const char* Clip : { "idle", "cower" })
			{
				const size_t At = Men.find(std::string("\"") + Clip + "\":{\"long\":");
				const size_t Rooted = At == std::string::npos ? std::string::npos : Men.find("\"root\":\"", At);
				bBoth = bBoth && Rooted != std::string::npos && Men.compare(Rooted + 8, 5, "deck\"") == 0;
			}
			Check(bBoth, std::string("the ") + Navy + " sailors' file has idle and cower, each on a deck");
		}
		// A struck boat's crew on her casing (the sea show's Hands): walked to their places at 1.25 m a second and run to her
		// side at 3, so the files' walk and run, on a deck, must cover that ground in their stride or the men's feet slide.
		{
			const std::string ShowText = TextOf(Root + "/Battle/SeaShow.cpp");
			Check(Sailors.find("case Act::Walk: return \"walk\";") != std::string::npos && Sailors.find("case Act::Run: return \"run\";") != std::string::npos &&
			          ShowText.find("const bool bAcross = Toward(D.Across, D.PlaceAcross, 1.25);") != std::string::npos &&
			          ShowText.find("if (Toward(D.Across, D.Side * Row.Edge, 3.0))") != std::string::npos,
			      "a struck boat's men walk to their places at 1.25 m/s and run to her side at 3 m/s, by the files' walk and run");
			for (const char* Navy : { "GER", "ITA", "JAP", "SOV", "UK", "USA" })
			{
				const std::string Men = TextOf(Content + "Models/Soldiers/soldier_" + Navy + "_sailor.json");
				bool bPaced = !Men.empty();
				for (const auto& [Clip, Pace] : { std::pair<const char*, double>{ "walk", 1.25 }, std::pair<const char*, double>{ "run", 3.0 } })
				{
					const std::string Head = std::string("\"") + Clip + "\":{\"long\":";
					const size_t At = Men.find(Head);
					const size_t Strode = At == std::string::npos ? std::string::npos : Men.find("\"stride\":", At);
					const size_t Rooted = At == std::string::npos ? std::string::npos : Men.find("\"root\":\"", At);
					if (Strode == std::string::npos || Rooted == std::string::npos)
					{
						bPaced = false;
						continue;
					}
					const double Long = std::strtod(Men.c_str() + At + Head.size(), nullptr), Stride = std::strtod(Men.c_str() + Strode + 9, nullptr);
					bPaced = bPaced && Men.compare(Rooted + 8, 5, "deck\"") == 0 && Long > 0.0 && std::fabs(Stride / Long - Pace) < 0.05 * Pace;
				}
				Check(bPaced, std::string("the ") + Navy + " sailors' file has walk (1.25 m/s) and run (3 m/s), each on a deck");
			}
		}
		// A boat's hits' looks put out by one rule, ConvoyLook::PutsOutLooks, which the scene asks each frame just before it
		// puts them out on the stage (and the stage's PutOut hands to the ship looks'), and the sea show's cloud test holds the
		// show's hits on boats to (the review of 23 November: the test had its own copy of the rule). These pins were the
		// scene's "if (Ship.Class == "ss" && (Ship.Dive != "surfaced" || Ship.Dived > 1.0))" until then: changed on purpose.
		{
			const std::string Asks = "if (Ship.Class == \"ss\" && ConvoyLook::PutsOutLooks(Ship.Dive, Ship.Dived))";
			const size_t At = Scene.find(Asks);
			const size_t Out = At == std::string::npos ? std::string::npos : Scene.find("Stage->PutOut(Ship.Key, S.BattleT);", At);
			const std::string StageSource = TextOf(Root + "/Battle/AirStage.cpp");
			const size_t Puts = StageSource.find("void AAirStage::PutOut(int32 Id, double Now)");
			Check(Out != std::string::npos && Out - At < Asks.size() + 16 && Puts != std::string::npos && StageSource.find("Looks.PutOut(Id, Now);", Puts) != std::string::npos,
			      "the scene puts out a boat's hits' looks when PutsOutLooks says, each frame, the stage handing it to her looks");
			Check(!PutsOutLooks("surfaced", 0.0) && !PutsOutLooks("surfaced", 1.0) && PutsOutLooks("surfaced", 1.2) && PutsOutLooks("periscope", 0.0) &&
			          PutsOutLooks("periscope", 9.0) && PutsOutLooks("deep", 60.0),
			      "her hits' looks put out from the diving alarm (told down, her casing still awash) and while she is under, not while she is up");
		}
		// A merchant's hit looks (the review of 23 November: every hit on a merchant took the transports' and merchants' "ap"
		// looks, her torpedo hits too): the table's "ap" for a shell's hit on her alone, a boat's deck gun's; her torpedo and
		// bomb hits her own "mv", as before the deck gun (no look of the table's), by one rule, ConvoyLook::HitLookFamily, that
		// the ship looks ask for every hit and the reel. These pins were "return Class == TEXT("mv") ? std::string("ap") :
		// FamilyOf(Class);" and "Blow.Family = ShipLookParts::HitFamilyOf(Class);" until then: changed on purpose.
		const std::string Looks = TextOf(Root + "/Battle/ShipLooks.cpp");
		size_t Uses = 0;
		for (size_t At = Looks.find("Blow.Family = ShipLookParts::HitFamilyOf(Class, Strike);"); At != std::string::npos;
		     At = Looks.find("Blow.Family = ShipLookParts::HitFamilyOf(Class, Strike);", At + 1))
		{
			++Uses;
		}
		Check(Looks.find("return ConvoyLook::HitLookFamily(FamilyOf(Class), WeaponOf(Strike));") != std::string::npos && Uses == 3 &&
		          Looks.find("HitFamilyOf(Class)") == std::string::npos,
		      "a hit's look family by its strike (HitLookFamily), in the scene and its reel");
		Check(HitLookFamily("mv", "shell_light") == "ap" && HitLookFamily("mv", "shell_medium") == "ap" && HitLookFamily("mv", "shell_heavy") == "ap" &&
		          HitLookFamily("mv", "torpedo") == "mv" && HitLookFamily("mv", "bomb") == "mv" && HitLookFamily("mv", "bomb_heavy") == "mv" &&
		          HitLookFamily("dd", "shell_light") == "dd" && HitLookFamily("ss", "shell_light") == "ss" && HitLookFamily("ap", "torpedo") == "ap" &&
		          HitLookFamily("bb", "bomb") == "bb",
		      "a merchant struck by a shell takes the transports' and merchants' looks (\"ap\"); by a torpedo or a bomb her own (\"mv\"); any other ship her own");
		{
			Fates::Table Table;
			std::string Why;
			Check(Table.Load(TextOf(Content + "Data/Battle/fates.json"), Why), "the looks table loads: " + Why);
			Fates::Director Picker(Table, 241u);
			int Some = 0, None = 0, Torpedoed = 0;
			for (int K = 0; K < 20; ++K)
			{
				Fates::Blow Worn;
				Worn.Domain = "sea";
				Worn.Family = "ap";
				Worn.Weapon = "shell_light";
				Worn.Result = "worn";
				Worn.Hurt = 0.05 * K;
				Worn.Nerve = 0.65;
				Worn.Unit = 3 * 64 + K;
				Some += Picker.Choose(Worn).Key.empty() ? 0 : 1;
				Worn.Family = "mv";
				None += Picker.Choose(Worn).Key.empty() ? 0 : 1;
				// (and her torpedo and bomb hits as the ship looks ask for them: none of the table's, as before)
				for (const char* Weapon : { "torpedo", "bomb", "bomb_heavy" })
				{
					Worn.Weapon = Weapon;
					Worn.Family = HitLookFamily("mv", Worn.Weapon);
					Torpedoed += Picker.Choose(Worn).Key.empty() ? 0 : 1;
				}
			}
			Check(Some == 20 && None == 0 && Torpedoed == 0, "a worn sea blow on a merchant from a deck gun's shell (\"ap\", shell_light) has a look every time: " +
			                                                     std::to_string(Some) + " of 20 (as \"mv\", " + std::to_string(None) + "); her torpedo and bomb hits none, as before (" +
			                                                     std::to_string(Torpedoed) + " of 60)");
		}
	}

	/** The sailor files the scene reads, by navy (as SeaBattle's BeginPlay reads Content/Models/Soldiers), into one crew. */
	bool ReadSailors(const std::string& Content, SeaSailors::Crew& Out, std::string& OutWhy)
	{
		for (const char* Navy : { "GER", "ITA", "JAP", "SOV", "UK", "USA" })
		{
			std::string Why;
			if (!Out.AddFile(std::string(Navy) == "UK" ? "GBR" : Navy, TextOf(Content + "Models/Soldiers/soldier_" + Navy + "_sailor.json"), Why))
			{
				OutWhy += std::string(" ") + Navy + ": " + Why;
			}
		}
		return Out.Files() == 6;
	}

	/** A model file's near mesh about one gun (Content/Models/Ships): its gun's mount and barrel by bands (their reach
	 * across and up, a band at a time: along the bore for the barrel, up for its mount), in the gun's own frame as built
	 * (u along its bore lying stowed, v to its right, z over her waterline, from its mount's pivot), and the flats facing up
	 * within 4 m of the mount that are not of the gun: her deck there. */
	struct GunModel
	{
		bool bRead = false;
		double PivotX = 0.0, PivotY = 0.0, Rest = 0.0;
		double BarrelU = 0.0, BarrelZ = 0.0; // the barrel's pivot
		struct Band
		{
			double Lo = 1e9, Hi = -1e9, ZLo = 1e9, ZHi = -1e9; // its reach across (v) and, for the barrel's, up
			double ULo = 1e9, UHi = -1e9;                      // the mount's along (u)
		};
		std::map<int, Band> Mount, Barrel; // by z band (the mount's: 5 cm), by u band (the barrel's: 10 cm)
		struct Tri
		{
			double X[3], Y[3], Z[3];
		};
		std::vector<Tri> Decks;
		double MountBackU = 0.0, MountHalf = 0.0;
	};

	GunModel ReadGun(const std::string& Content, const GunMount& M)
	{
		GunModel G;
		const std::string Model = TextOf(Content + "Models/Ships/" + M.File + ".json");
		int MountIndex = -1, BarrelIndex = -1;
		const size_t MountAt = PartAt(Model, M.Part, MountIndex), BarrelAt = PartAt(Model, M.Part + ".gun.1", BarrelIndex);
		if (MountIndex < 0 || BarrelIndex < 0)
		{
			return G;
		}
		const std::vector<double> Pivot = NumbersOf(Model, "pivot", MountAt), Breech = NumbersOf(Model, "pivot", BarrelAt);
		const size_t Near = Model.find("\"near\":{");
		const std::vector<double> Points = NumbersOf(Model, "points", Near), Facings = NumbersOf(Model, "facings", Near);
		const std::vector<double> Corners = NumbersOf(Model, "triangles", Near), Parts = NumbersOf(Model, "part", Near);
		if (Pivot.size() != 3 || Breech.size() != 3 || Points.size() != Parts.size() * 3 || Facings.size() != Points.size() || Corners.empty())
		{
			return G;
		}
		G.bRead = true;
		G.PivotX = Pivot[0];
		G.PivotY = Pivot[1];
		G.Rest = M.Rest;
		const double Way = std::cos(M.Rest * Pi / 180.0) >= 0.0 ? 1.0 : -1.0;
		G.BarrelU = (Breech[0] - Pivot[0]) * Way;
		G.BarrelZ = Breech[2];
		G.MountBackU = 1e9;
		for (size_t K = 0; K < Parts.size(); ++K)
		{
			const int Part = static_cast<int>(Parts[K]);
			const double U = (Points[K * 3] - Pivot[0]) * Way, V = (Points[K * 3 + 1] - Pivot[1]) * Way, Z = Points[K * 3 + 2];
			if (Part == MountIndex)
			{
				GunModel::Band& B = G.Mount[static_cast<int>(std::floor(Z / 0.05))];
				B.Lo = std::min(B.Lo, V);
				B.Hi = std::max(B.Hi, V);
				B.ULo = std::min(B.ULo, U);
				B.UHi = std::max(B.UHi, U);
				G.MountBackU = std::min(G.MountBackU, U);
				G.MountHalf = std::max(G.MountHalf, std::fabs(V));
			}
			else if (Part == BarrelIndex)
			{
				GunModel::Band& B = G.Barrel[static_cast<int>(std::floor((U - G.BarrelU) / 0.1))];
				B.Lo = std::min(B.Lo, V);
				B.Hi = std::max(B.Hi, V);
				B.ZLo = std::min(B.ZLo, Z);
				B.ZHi = std::max(B.ZHi, Z);
			}
		}
		for (size_t T = 0; T + 2 < Corners.size(); T += 3)
		{
			const size_t A = static_cast<size_t>(Corners[T]), B = static_cast<size_t>(Corners[T + 1]), C = static_cast<size_t>(Corners[T + 2]);
			if (A >= Parts.size() || B >= Parts.size() || C >= Parts.size())
			{
				continue;
			}
			const int PA = static_cast<int>(Parts[A]), PB = static_cast<int>(Parts[B]), PC = static_cast<int>(Parts[C]);
			if (PA == MountIndex || PA == BarrelIndex || PB == MountIndex || PB == BarrelIndex || PC == MountIndex || PC == BarrelIndex)
			{
				continue;
			}
			if (Facings[A * 3 + 2] + Facings[B * 3 + 2] + Facings[C * 3 + 2] < 2.7)
			{
				continue;
			}
			GunModel::Tri Flat;
			for (int K = 0; K < 3; ++K)
			{
				const size_t P = K == 0 ? A : K == 1 ? B : C;
				Flat.X[K] = Points[P * 3];
				Flat.Y[K] = Points[P * 3 + 1];
				Flat.Z[K] = Points[P * 3 + 2];
			}
			if (std::hypot((Flat.X[0] + Flat.X[1] + Flat.X[2]) / 3.0 - Pivot[0], (Flat.Y[0] + Flat.Y[1] + Flat.Y[2]) / 3.0 - Pivot[1]) <= 6.0)
			{
				G.Decks.push_back(Flat);
			}
		}
		return G;
	}

	/** The model's deck under a place by its gun (Fore, Right from the mount's middle), nearest Want within Band; -1e9
	 * where none. */
	double ModelDeck(const GunModel& G, double Fore, double Right, double Want, double Band)
	{
		const double X = G.PivotX + Fore, Y = G.PivotY + Right;
		double Best = -1e9;
		for (const GunModel::Tri& T : G.Decks)
		{
			const double D = (T.Y[1] - T.Y[2]) * (T.X[0] - T.X[2]) + (T.X[2] - T.X[1]) * (T.Y[0] - T.Y[2]);
			if (std::fabs(D) < 1e-12) continue;
			const double L0 = ((T.Y[1] - T.Y[2]) * (X - T.X[2]) + (T.X[2] - T.X[1]) * (Y - T.Y[2])) / D;
			const double L1 = ((T.Y[2] - T.Y[0]) * (X - T.X[2]) + (T.X[0] - T.X[2]) * (Y - T.Y[2])) / D;
			const double L2 = 1.0 - L0 - L1;
			if (L0 < -1e-9 || L1 < -1e-9 || L2 < -1e-9) continue;
			const double Z = L0 * T.Z[0] + L1 * T.Z[1] + L2 * T.Z[2];
			if (std::fabs(Z - Want) <= Band && std::fabs(Z - Want) < std::fabs(Best - Want)) Best = Z;
		}
		return Best;
	}

	/** How deep a place by the gun (her frame from the mount's middle, the gun trained to Bears and lifted to Lie degrees
	 * over its built lie, run back Back metres) is inside its mount or barrel, a band at a time less Margin; 0 outside. */
	double InGun(const GunModel& G, double Bears, double Lift, double Back, double Fore, double Right, double Up, double Margin)
	{
		const double D = (Bears - G.Rest) * Pi / 180.0, Way = std::cos(G.Rest * Pi / 180.0) >= 0.0 ? 1.0 : -1.0;
		// (un-turned to where it lies stowed, then into the gun's own frame: u along its bore, v to its right)
		const double A = Fore * std::cos(D) + Right * std::sin(D), B = -Fore * std::sin(D) + Right * std::cos(D);
		const double U = A * Way, V = B * Way;
		double Deepest = 0.0;
		auto Inside = [&](const GunModel::Band& Bd, double At, double Lo, double Hi)
		{
			const double Depth = std::min({ V - Bd.Lo, Bd.Hi - V, At - Lo, Hi - At }) - Margin;
			if (Depth > Deepest) Deepest = Depth;
		};
		if (const auto It = G.Mount.find(static_cast<int>(std::floor(Up / 0.05))); It != G.Mount.end())
		{
			Inside(It->second, U, It->second.ULo, It->second.UHi);
		}
		// (the barrel lifted about its pivot, and run back along its bore)
		const double L = Lift * Pi / 180.0, Du = U - G.BarrelU, Dz = Up - G.BarrelZ;
		const double Bu = Du * std::cos(L) + Dz * std::sin(L) + Back, Bz = G.BarrelZ - Du * std::sin(L) + Dz * std::cos(L);
		if (const auto It = G.Barrel.find(static_cast<int>(std::floor(Bu / 0.1))); It != G.Barrel.end())
		{
			Inside(It->second, Bz, It->second.ZLo, It->second.ZHi);
		}
		return std::max(0.0, Deepest);
	}

	/**
	 * How far two men at their posts go into each other through their clips, as capsules about their bones (metres; below
	 * 0 the gap between them): his trunk 0.14 m about from his pelvis to his chest, 0.12 to his neck and 0.10 to his
	 * head, his shoulders 0.07, his arms 0.055 to his elbows, 0.045 to his wrists and 0.04 to his fingers' tips, his legs
	 * 0.075 to his knees and 0.055 to his ankles; each man's clip every Every seconds, every time of one against every
	 * time of the other (they keep no step together).
	 */
	double BodiesInto(const SeaSailors::Crew& Sailors, int K, SeaSailors::Act ActA, float LongA, const DeckCrew::Post& PostA, SeaSailors::Act ActB,
	                  float LongB, const DeckCrew::Post& PostB, float Every)
	{
		struct Limb
		{
			const char* From;
			const char* To;
			double About;
		};
		static const Limb Limbs[] = {
			{ "pelvis", "chest", 0.14 },         { "chest", "neck", 0.12 },           { "neck", "head", 0.10 },
			{ "chest", "upperarm_l", 0.07 },     { "chest", "upperarm_r", 0.07 },     { "upperarm_l", "lowerarm_l", 0.055 },
			{ "lowerarm_l", "hand_l", 0.045 },   { "hand_l", "fingers_l", 0.04 },     { "upperarm_r", "lowerarm_r", 0.055 },
			{ "lowerarm_r", "hand_r", 0.045 },   { "hand_r", "fingers_r", 0.04 },     { "thigh_l", "calf_l", 0.075 },
			{ "calf_l", "foot_l", 0.055 },       { "thigh_r", "calf_r", 0.075 },      { "calf_r", "foot_r", 0.055 },
		};
		constexpr size_t Count = sizeof(Limbs) / sizeof(Limbs[0]);
		using Ends = std::vector<std::array<DeckCrew::Spot, 2>>;
		auto Pose = [&](SeaSailors::Act A, float T, const DeckCrew::Post& At)
		{
			Ends Out(Count);
			for (size_t I = 0; I < Count; ++I)
			{
				const LandMen::Vec3 From = Sailors.BoneAt(K, A, T, Limbs[I].From), To = Sailors.BoneAt(K, A, T, Limbs[I].To);
				Out[I][0] = DeckCrew::ToHer(At, From.X, From.Y, From.Z);
				Out[I][1] = DeckCrew::ToHer(At, To.X, To.Y, To.Z);
			}
			return Out;
		};
		// (the nearest two segments come, Ericson's closest points of two segments)
		auto Apart = [](const DeckCrew::Spot& P1, const DeckCrew::Spot& Q1, const DeckCrew::Spot& P2, const DeckCrew::Spot& Q2)
		{
			const double D1[3] = { Q1.Fore - P1.Fore, Q1.Right - P1.Right, Q1.Up - P1.Up }, D2[3] = { Q2.Fore - P2.Fore, Q2.Right - P2.Right, Q2.Up - P2.Up };
			const double Rv[3] = { P1.Fore - P2.Fore, P1.Right - P2.Right, P1.Up - P2.Up };
			auto Dot = [](const double* U, const double* W) { return U[0] * W[0] + U[1] * W[1] + U[2] * W[2]; };
			const double A = Dot(D1, D1), E = Dot(D2, D2), Fv = Dot(D2, Rv);
			double Sv = 0.0, Tv = 0.0;
			if (A > 1e-12 && E > 1e-12)
			{
				const double C = Dot(D1, Rv), Bv = Dot(D1, D2), Under = A * E - Bv * Bv;
				Sv = Under > 1e-12 ? std::clamp((Bv * Fv - C * E) / Under, 0.0, 1.0) : 0.0;
				Tv = (Bv * Sv + Fv) / E;
				if (Tv < 0.0)
				{
					Tv = 0.0;
					Sv = std::clamp(-C / A, 0.0, 1.0);
				}
				else if (Tv > 1.0)
				{
					Tv = 1.0;
					Sv = std::clamp((Bv - C) / A, 0.0, 1.0);
				}
			}
			else if (A > 1e-12)
			{
				Sv = std::clamp(-Dot(D1, Rv) / A, 0.0, 1.0);
			}
			else if (E > 1e-12)
			{
				Tv = std::clamp(Fv / E, 0.0, 1.0);
			}
			const double Gap[3] = { Rv[0] + D1[0] * Sv - D2[0] * Tv, Rv[1] + D1[1] * Sv - D2[1] * Tv, Rv[2] + D1[2] * Sv - D2[2] * Tv };
			return std::sqrt(Dot(Gap, Gap));
		};
		std::vector<Ends> Others;
		for (float T = 0.f; T < LongB; T += Every)
		{
			Others.push_back(Pose(ActB, T, PostB));
		}
		double Most = -1e9;
		for (float T = 0.f; T < LongA; T += Every)
		{
			const Ends Mine = Pose(ActA, T, PostA);
			for (const Ends& Theirs : Others)
			{
				for (size_t I = 0; I < Count; ++I)
				{
					for (size_t J = 0; J < Count; ++J)
					{
						Most = std::max(Most, Limbs[I].About + Limbs[J].About - Apart(Mine[I][0], Mine[I][1], Theirs[J][0], Theirs[J][1]));
					}
				}
			}
		}
		return Most;
	}

	/** How far a man's boots go past the model's deck at a post (metres: from each corner of his boots off it, stepped in
	 * toward his root a centimetre at a time until on it; 0 all on it): his heels 0.2 m behind his root, his toes 0.33
	 * ahead, 0.21 either side. OutOff counts the corners off it. */
	double BootsOff(const GunModel& G, const DeckCrew::Post& P, int* OutOff = nullptr)
	{
		double Most = 0.0;
		for (const double X : { -0.2, 0.33 })
		{
			for (const double Y : { -0.21, 0.21 })
			{
				const DeckCrew::Spot C = DeckCrew::ToHer(P, X, Y, 0.0);
				if (ModelDeck(G, C.Fore, C.Right, P.Root.Up, 0.3) > -1e8)
				{
					continue;
				}
				if (OutOff) ++*OutOff;
				double In = 0.0;
				for (; In < 1.0; In += 0.01)
				{
					const DeckCrew::Spot Back = DeckCrew::ToHer(P, X * (1.0 - In / std::hypot(X, Y)), Y * (1.0 - In / std::hypot(X, Y)), 0.0);
					if (ModelDeck(G, Back.Fore, Back.Right, P.Root.Up, 0.3) > -1e8) break;
				}
				Most = std::max(Most, In);
			}
		}
		return Most;
	}

	/** Her deck's height over the sea at a place by her gun (metres), as SeaShow::HandAt sets the show's own men on her
	 * casing: the deck there (her waterline's frame), less how far she is down, with her pitch (bow up) along her from
	 * amidships, her heave, and her heel (starboard down) across her. */
	double FreeboardOf(const GunMount& M, double DeckUp, double Fore, double Right, double Dived, double Pitch, double Heave, double Heel)
	{
		return DeckUp - Dived + (M.Along + Fore) * std::sin(Pitch * Pi / 180.0) + Heave - Right * std::sin(Heel * Pi / 180.0);
	}

	/** The round in the passer's hands as the loader takes it and in the loader's as he has it first (the pair's moment),
	 * each in her frame from their posts: how far apart (metres). */
	double HandOffGap(const DeckCrew::Blocks& B, const DeckCrew::Post& Passing, const DeckCrew::Post& Loading)
	{
		float First = -1.f;
		for (size_t I = 0; I + 7 < B.Loading.Track.size() && First < 0.f; I += 8)
		{
			if (B.Loading.Track[I] >= B.Taken - 1e-4f && B.Loading.Track[I + 1] > 0.5f) First = B.Loading.Track[I];
		}
		SeaSailors::RoundAt Taken, Handed;
		SeaSailors::RoundAlong(B.Loading, B.Long[static_cast<int>(DeckCrew::Role::Loader)], First, Taken);
		SeaSailors::RoundAlong(B.Passing, B.Long[static_cast<int>(DeckCrew::Role::Passer)], B.Taken - 0.01f, Handed);
		const DeckCrew::Spot A = DeckCrew::ToHer(Passing, Handed.Base.X, Handed.Base.Y, Handed.Base.Z);
		const DeckCrew::Spot C = DeckCrew::ToHer(Loading, Taken.Base.X, Taken.Base.Y, Taken.Base.Z);
		return std::sqrt((A.Fore - C.Fore) * (A.Fore - C.Fore) + (A.Right - C.Right) * (A.Right - C.Right) + (A.Up - C.Up) * (A.Up - C.Up));
	}

	/** What a crew's frames showed: their boots against the model's deck (on their way and at the gun), the hatch (two
	 * men in it, their feet apart; a man waiting short of it, from its middle), and how they came and went. */
	struct CrewSeen
	{
		double BootsMost = 0.0, BootsAt = -1.0, FeetApart = 99.0, WaitFrom = 99.0;
		int BootsWho = -1, InHatchTwo = 0, Waited = 0;
		void See(const GunModel& Model, const DeckCrew::GunShape& S, const GunMount& M, const DeckCrew::Watch& W, double T)
		{
			const DeckCrew::Spot Hatch = DeckCrew::HatchOf(S, M);
			std::vector<double> Climbing;
			for (int R = 0; R < DeckCrew::Roles; ++R)
			{
				const DeckCrew::Shown& Is = W.Men[R];
				if (!Is.bShown) continue;
				if (Is.What == SeaSailors::Act::Climb)
				{
					Climbing.push_back(Is.At.Root.Up);
					continue;
				}
				if (R == static_cast<int>(DeckCrew::Role::Passer)) continue;
				const double Off = BootsOff(Model, Is.At);
				if (Off > BootsMost)
				{
					BootsMost = Off;
					BootsAt = T;
					BootsWho = R;
				}
				if (W.Hands[R].Now == DeckCrew::Phase::Wait)
				{
					++Waited;
					WaitFrom = std::min(WaitFrom, std::hypot(Is.At.Root.Fore - Hatch.Fore, Is.At.Root.Right - Hatch.Right));
				}
			}
			for (size_t I = 0; I < Climbing.size(); ++I)
			{
				for (size_t J = I + 1; J < Climbing.size(); ++J)
				{
					++InHatchTwo;
					FeetApart = std::min(FeetApart, std::fabs(Climbing[I] - Climbing[J]));
				}
			}
		}
	};

	/**
	 * A German boat's gun swung, her crew at it, from abaft her port beam round her bow to abaft her starboard beam and
	 * back at TrainRate degrees a second (15, and the show's own 8 across her beam), then the diving alarm: the loader's
	 * post moves on with it, his root no quicker than the gun's train swings it and he steps round the breech (TurnRate,
	 * SlideRate), so never a jump from one way round the breech to the other; turned both ways in turn; every frame each
	 * man's boots on the model's deck, going to the gun, at it as he steps round the breech, and going back to the hatch
	 * and waiting by it (a corner over a deck flat's edge OverhangMost at the most); at his work, the loader moved in from
	 * his clip's post 0.30 m at the most; standing clear as he steps round behind the breech (his hands would be in its
	 * ring), none of him in the gun; one man in the hatch at a time.
	 */
	void TestCrewSweep(const std::string& Content, const SeaSailors::Crew& Sailors)
	{
		const double OverhangMost = 0.15;
		const GunMount M = MountOf("Submarine_GER", false);
		const DeckCrew::GunShape S = DeckCrew::ShapeOf(M);
		const GunModel Model = ReadGun(Content, M);
		const DeckCrew::Blocks B = DeckCrew::BlocksOf(Sailors, Sailors.FileFor("GER"));
		const int Loader = static_cast<int>(DeckCrew::Role::Loader), File = Sailors.FileFor("GER");
		for (const double TrainRate : { 15.0, 8.0 })
		{
			const double Swing = 135.0, Hold = 10.0, After = 8.0;
			const double Across = 2.0 * Swing / TrainRate;
			// (his root at most 2 m from the mount's middle, his clip's reach from the breech)
			const double Fastest = (TrainRate * Pi / 180.0 * 2.0 + DeckCrew::TurnRate * Pi / 180.0 * B.Loading.Breech[0] + DeckCrew::SlideRate) * Frame * 1.05;
			DeckCrew::Watch W;
			DeckCrew::Told In;
			In.bBoat = true;
			In.bInLine = true;
			In.bLaid = true;
			In.Dive = "surfaced";
			In.Elevation = 10.0;
			In.Key = 77;
			CrewSeen Seen;
			double Jump = 0.0, JumpAt = -1.0, LastFore = 0.0, LastRight = 0.0, ShiftMost = 0.0, ClearShift = 0.0, ClearDeep = 0.0;
			int Clear = 0, ClearIn = 0;
			std::vector<float> SkinAt, SkinFacing;
			bool bWas = false;
			int Frames = 0, TurnedPort = 0, TurnedStarboard = 0, Sides = 0, LastSide = 0, ShownAfter = 0;
			const double Alarm = Hold + 2.0 * Across;
			for (int Tick = 0; Tick <= static_cast<int>((Alarm + After) * 30.0); ++Tick)
			{
				const double T = Tick / 30.0, Into = std::clamp(T - Hold, 0.0, 2.0 * Across);
				In.Train = M.Rest - Swing + TrainRate * (Into <= Across ? Into : 2.0 * Across - Into);
				In.Dive = T >= Alarm ? "periscope" : "surfaced";
				DeckCrew::Step(W, In, S, M, B, T, nullptr);
				Seen.See(Model, S, M, W, T);
				const DeckCrew::Shown& Is = W.Men[Loader];
				const bool bAt = Is.bShown && W.Hands[Loader].Now == DeckCrew::Phase::AtGun;
				if (bAt && bWas)
				{
					const double Moved = std::hypot(Is.At.Root.Fore - LastFore, Is.At.Root.Right - LastRight);
					if (Moved > Jump)
					{
						Jump = Moved;
						JumpAt = T;
					}
				}
				if (bAt)
				{
					++Frames;
					TurnedPort += Is.At.Turned < -1.0 ? 1 : 0;
					TurnedStarboard += Is.At.Turned > 1.0 ? 1 : 0;
					const int Side = Is.At.Turned > 1.0 ? 1 : Is.At.Turned < -1.0 ? -1 : 0;
					Sides += LastSide != 0 && Side != 0 && Side != LastSide ? 1 : 0;
					LastSide = Side != 0 ? Side : LastSide;
					LastFore = Is.At.Root.Fore;
					LastRight = Is.At.Root.Right;
					if (Is.bAtWork)
					{
						ShiftMost = std::max(ShiftMost, Is.At.Shift);
					}
					else
					{
						// (standing clear as he steps round behind the breech: none of him in the gun as it stands)
						++Clear;
						ClearShift = std::max(ClearShift, Is.At.Shift);
						const int Count = Sailors.Skin(File, Is.What, Is.Time, LandMen::Mid, SkinAt, SkinFacing);
						for (int I = 0; I < Count; ++I)
						{
							const DeckCrew::Spot Her = DeckCrew::ToHer(Is.At, SkinAt[static_cast<size_t>(I) * 3], SkinAt[static_cast<size_t>(I) * 3 + 1], SkinAt[static_cast<size_t>(I) * 3 + 2]);
							const double Depth = InGun(Model, DeckCrew::MountTrain(M, In.Train), std::clamp(In.Elevation, S.LiftLeast, S.LiftMost), 0.0, Her.Fore, Her.Right, Her.Up, 0.01);
							ClearIn += Depth > 0.0 ? 1 : 0;
							ClearDeep = std::max(ClearDeep, Depth);
						}
					}
				}
				bWas = bAt;
				for (const DeckCrew::Shown& Man : W.Men) ShownAfter += T > Alarm + 4.5 && Man.bShown ? 1 : 0;
			}
			const std::string N = std::string("[a German boat's gun swung beam to beam and back at ") + (TrainRate > 10.0 ? "15" : "8") + " degrees a second] ";
			Check(Frames > static_cast<int>(2.0 * Across * 30.0) && Jump <= Fastest && TurnedPort > 0 && TurnedStarboard > 0 && Sides >= 2,
			      N + F("the loader's post moves on with it: %.3f m a frame at the most (at %.1f s; %.3f)", Jump, JumpAt, Fastest) +
			          F(", turned to port %.0f frames, to starboard %.0f", TurnedPort, TurnedStarboard) + F(", his way round the breech changed %.0f times", Sides));
			Check(Model.bRead && Seen.BootsMost <= OverhangMost && ShiftMost <= 0.30 && ClearIn == 0,
			      N + F("every frame each man's boots on the model's deck, to the gun, at it and back to the hatch: over its edge by %.3f m at the most (%.2f", Seen.BootsMost, OverhangMost) +
			          F("; man %.0f at %.1f s); the loader at his work moved in from his clip's post %.3f m at the most (0.30)", Seen.BootsWho, Seen.BootsAt, ShiftMost) +
			          F("; standing clear stepping round behind the breech %.0f frames (moved in %.3f m at the most), none of him in the gun (%.0f points in", Clear, ClearShift, ClearIn) +
			          F(", %.3f m deep)", ClearDeep));
			Check(Seen.FeetApart >= DeckCrew::HatchApart - 1e-6 && Seen.Waited > 0 && Seen.WaitFrom >= 0.5 && ShownAfter == 0,
			      N + F("one at a time down her hatch at the alarm: two men in it %.0f frames, their feet %.2f m apart at the least", Seen.InHatchTwo, Seen.FeetApart) +
			          F(" (1.8); %.0f frames waiting, %.2f m from its middle at the least (0.5)", Seen.Waited, Seen.WaitFrom) + F(", none up 4.5 s after (%.0f)", ShownAfter));
		}
	}

	/** Whether a gun's loader is shown at his post with no round in his hands short of the breech shut (waiting with his
	 * hands out, reaching for the next, past breech_close, or standing clear of the breech): as the scene draws him. */
	bool ShutAtPost(const DeckCrew::Watch& W, const DeckCrew::Blocks& K)
	{
		const int Loader = static_cast<int>(DeckCrew::Role::Loader);
		const DeckCrew::Shown& Is = W.Men[Loader];
		return W.Hands[Loader].Now == DeckCrew::Phase::AtGun && W.SentAt < 0.0 && Is.bShown &&
		       (Is.What != SeaSailors::Act::Loader || Is.Time < K.Taken || Is.Time >= K.Shut);
	}

	/** Whether a gun's layer, trainer and loader are shown at their posts (none sent below). */
	bool AtPosts(const DeckCrew::Watch& W)
	{
		bool bAll = W.SentAt < 0.0;
		for (int R = 0; R < DeckCrew::Roles; ++R)
		{
			bAll = bAll && (R == static_cast<int>(DeckCrew::Role::Passer) || (W.Hands[R].Now == DeckCrew::Phase::AtGun && W.Men[R].bShown));
		}
		return bAll;
	}

	/**
	 * A gun's crew through a night as the show tells it (DeckCrew::Step, at 30 frames a second): a boat surfaced, her gun
	 * told at 10 s, its rounds fired at the show's least cadence and slower once her crew are up (on some, rounds before
	 * that: a hit the battle tells fired as her gun is told and another 2 s on, the loader's round in his hands; one as
	 * her crew are still coming up her hatch; one as the loader rams, short of the breech shut), the diving alarm at 60 s,
	 * she going down as the show takes her (ShowBoat: her keel eased down, bow down by her rate of going down over her
	 * speed, her pitch and heave on the swell); the same boat striking instead; the German and the other navies' boats,
	 * their guns forward, slow and at speed; a skip while manned; a merchant at her gun until she leaves her line, and at
	 * her other gun; a boat whose hatch is where the loader can take the rounds from the passer. Her crew up her hatch one
	 * at a time after her gun is told (until a round is fired, then each at his post at once), every round fired with her
	 * layer, trainer and loader at their posts and the breech shut (the review of 25 November), the loader's ram before
	 * each round he could have loaded in his own time, all below within DownWithin of the alarm and none on her deck where
	 * it is within FreeboardLeast of the sea, none from the moment she strikes, the cues heard (from the rack: no
	 * handoff), none on a boat the show hides.
	 */
	void TestCrewTimes(const std::string& Content, const SeaSailors::Crew& Sailors)
	{
		const double DownWithin = 4.5, FreeboardLeast = 0.1;
		const int Loader = static_cast<int>(DeckCrew::Role::Loader), Passer = static_cast<int>(DeckCrew::Role::Passer);
		const double Later[] = { 20.0, 24.5, 29.0, 33.25, 38.0, 42.3, 50.0, 55.0 };
		const struct
		{
			const char* File;
			const char* Navy;
			bool bStrikes;
			double Speed, Off; // her speed through the water (metres a second), the gun's train off where it stows
			double Early[2];   // rounds before her crew are all up and the loader has rammed one (-1 none)
		} Cases[] = {
			{ "Submarine_USA", "USA", false, 1.5, 20.0, { -1.0, -1.0 } }, { "Submarine_USA", "USA", true, 1.5, 20.0, { 10.0, 12.0 } },
			{ "Submarine_GER", "GER", false, 1.5, 20.0, { -1.0, -1.0 } }, { "Submarine_GER", "GER", false, 1.5, 90.0, { 12.0, -1.0 } },
			{ "Submarine_GER", "GER", false, 6.0, 60.0, { -1.0, -1.0 } }, { "Submarine", "JAP", false, 1.5, -40.0, { -1.0, -1.0 } },
			{ "Submarine", "JAP", false, 1.5, 20.0, { 13.0, 15.5 } },     { "Submarine_GER", "GER", false, 1.5, 20.0, { 10.0, 12.0 } },
		};
		for (const auto& Case : Cases)
		{
			// (the show's rounds: those before her crew are up, then its least cadence and slower)
			std::vector<double> Fires;
			for (const double At : Case.Early)
			{
				if (At >= 0.0) Fires.push_back(At);
			}
			Fires.insert(Fires.end(), std::begin(Later), std::end(Later));
			const double FirstEarly = Fires.front() < Later[0] ? Fires.front() : -1.0;
			const GunMount M = MountOf(Case.File, false);
			const DeckCrew::GunShape S = DeckCrew::ShapeOf(M);
			const GunModel Model = ReadGun(Content, M);
			const DeckCrew::Blocks B = DeckCrew::BlocksOf(Sailors, Sailors.FileFor(Case.Navy));
			const std::string N = std::string("[") + Case.File + ", " + Case.Navy + " crew, " + (Case.bStrikes ? "striking at 60 s" : "the diving alarm at 60 s") +
			                      F(", %.1f m/s, the gun %.0f degrees off", Case.Speed, Case.Off) + (FirstEarly < 0.0 ? std::string() : F(", a round at %.2f s and %.2f", Case.Early[0], Case.Early[1])) + "] ";
			ShowBoat Boat;
			Boat.Up = M.File == "Submarine_USA" ? 95.0 * 0.07 : M.File == "Submarine" ? 80.0 * 0.07 : 67.0 * 0.07;
			DeckCrew::Watch W;
			DeckCrew::Told In;
			In.bBoat = true;
			In.bInLine = true;
			In.Train = M.Rest + Case.Off;
			In.Elevation = 5.0;
			In.Key = 4242;
			CrewSeen Seen;
			std::map<std::string, int> Heard;
			double FirstUp[DeckCrew::Roles] = { -1.0, -1.0, -1.0, -1.0 }, AllAt = -1.0, Gone = -1.0, LastRam = -1.0, LastFire = -1.0, AtSince = -1.0;
			double DeckLeast = 99.0, DeckLeastAt = -1.0;
			int ShownBeforeTold = 0, ShownStruck = 0, Rounds = 0, Eligible = 0, RammedFirst = 0, ShownHidden = 0, Awash = 0, Shut = 0, Snapped = 0;
			double OpenAt = -1.0;
			size_t Next = 0;
			for (int Tick = 0; Tick <= 75 * 30; ++Tick)
			{
				const double T = Tick / 30.0;
				In.bLaid = T >= 10.0 && T < 60.0;
				In.Dive = !Case.bStrikes && T >= 60.0 ? "periscope" : "surfaced";
				Boat.Step(T, In.Dive);
				In.Dived = Boat.Dived();
				In.bStruck = Case.bStrikes && T >= 60.0;
				const double Pitch = Boat.Pitch(T, Case.Speed), Heave = 0.3 * std::sin(T * 0.6) * Boat.Calm(), Heel = 3.0 * std::sin(T * 0.37) * Boat.Calm();
				bool bFires = false;
				while (Next < Fires.size() && Fires[Next] <= T + 1e-9)
				{
					In.FiredAt = Fires[Next++];
					bFires = true;
				}
				// (whether the loader could have loaded this round: at the gun all the time since the last, the show's
				// rounds no quicker than his clip at its pace)
				if (bFires)
				{
					++Rounds;
					const bool bCould = LastFire >= 0.0 && AtSince >= 0.0 && AtSince <= LastFire && In.FiredAt - LastFire >= DeckCrew::FastestReload(B, true);
					if (bCould)
					{
						++Eligible;
						RammedFirst += LastRam > LastFire && LastRam <= In.FiredAt + 1e-9 ? 1 : 0;
					}
					LastFire = In.FiredAt;
				}
				std::vector<DeckCrew::Cue> Cues;
				// (as drawn the frame before the round: her crew at their posts, the breech shut)
				const bool bShutBefore = ShutAtPost(W, B), bUpBefore = AtPosts(W);
				DeckCrew::Step(W, In, S, M, B, T, &Cues);
				Seen.See(Model, S, M, W, T);
				for (const DeckCrew::Cue& Each : Cues)
				{
					++Heard[Each.Name];
					LastRam = Each.Name == B.RamCue ? T : LastRam;
				}
				if (bFires)
				{
					const bool bShut = AtPosts(W) && (bShutBefore || ShutAtPost(W, B));
					Shut += bShut ? 1 : 0;
					Snapped += bShut && !(bUpBefore && bShutBefore) ? 1 : 0;
					OpenAt = !bShut && OpenAt < 0.0 ? In.FiredAt : OpenAt;
				}
				const bool bLoaderAt = W.Hands[Loader].Now == DeckCrew::Phase::AtGun;
				AtSince = bLoaderAt ? (AtSince < 0.0 ? T : AtSince) : -1.0;
				bool bAny = false, bAll = true;
				SeaShow::ShipOut Hidden;
				Hidden.bShown = false;
				for (int R = 0; R < DeckCrew::Roles; ++R)
				{
					const DeckCrew::Shown& Is = W.Men[R];
					bAny = bAny || Is.bShown;
					bAll = bAll && (R == Passer || (Is.bShown && Is.bAtWork));
					if (Is.bShown && FirstUp[R] < 0.0) FirstUp[R] = T;
					ShownHidden += DeckCrew::Drawn(Is, Hidden) ? 1 : 0;
					if (!Is.bShown) continue;
					// (her deck over the sea where he is, the hatch's where he climbs in it)
					const double Deck = FreeboardOf(M, DeckCrew::DeckUnder(S, M, Is.At.Root.Fore, Is.At.Root.Right), Is.At.Root.Fore, Is.At.Root.Right, In.Dived, Pitch, Heave, Heel);
					if (Deck < DeckLeast)
					{
						DeckLeast = Deck;
						DeckLeastAt = T;
					}
					Awash += Deck <= FreeboardLeast ? 1 : 0;
				}
				AllAt = bAll && AllAt < 0.0 ? T : AllAt;
				ShownBeforeTold += bAny && T < 10.0 ? 1 : 0;
				ShownStruck += bAny && In.bStruck ? 1 : 0;
				Gone = T >= 60.0 && !bAny && Gone < 0.0 ? T : Gone;
			}
			if (FirstEarly < 0.0)
			{
				Check(ShownBeforeTold == 0 && FirstUp[0] >= 10.0 && FirstUp[0] < 10.1 && FirstUp[1] > FirstUp[0] + DeckCrew::Depth / DeckCrew::ClimbUpRate &&
				          FirstUp[2] > FirstUp[1] + DeckCrew::Depth / DeckCrew::ClimbUpRate && FirstUp[Passer] < 0.0 && AllAt > 10.0 && AllAt < 20.0,
				      N + F("her crew up her hatch one at a time once her gun is told (10 s): the layer at %.2f s, the trainer at %.2f, the loader at %.2f", FirstUp[0], FirstUp[1], FirstUp[2]) +
				          F("; at their work by %.2f; no passer (none can hand the loader his rounds from her hatch: %.0f)", AllAt, FirstUp[Passer]));
			}
			else
			{
				// (a round before they are all up: up her hatch one at a time until it, then each at his post as it fires)
				Check(ShownBeforeTold == 0 && FirstUp[0] >= 10.0 && FirstUp[0] < 10.1 && AllAt >= FirstEarly && AllAt < FirstEarly + 0.5 * Frame && FirstUp[2] <= FirstEarly + 1e-9 &&
				          (FirstUp[1] >= FirstEarly - 1e-9 || FirstUp[1] > FirstUp[0] + DeckCrew::Depth / DeckCrew::ClimbUpRate) && FirstUp[Passer] < 0.0,
				      N + F("her crew up her hatch one at a time until the round at %.2f s, then each at his post as it fires: the layer up at %.2f s, the trainer at %.2f", FirstEarly, FirstUp[0], FirstUp[1]) +
				          F(", the loader at %.2f; all at their work at %.2f", FirstUp[2], AllAt));
			}
			Check(Shut == Rounds && (FirstEarly < 0.0 || Snapped >= 1),
			      N + F("every round fired with her layer, trainer and loader at their posts and the breech shut: %.0f of %.0f (%.0f as they were coming up or the loader's round short of the breech", Shut, Rounds, Snapped) +
			          F(", put at their posts at once; the first not at %.2f s)", OpenAt));
			Check(Eligible >= 5 && RammedFirst == Eligible,
			      N + F("the loader's ram before each round he could have loaded in his own time: %.0f of %.0f (of the show's %.0f rounds)", RammedFirst, Eligible, Rounds));
			if (Case.bStrikes)
			{
				Check(ShownStruck == 0 && Gone >= 60.0 && Gone < 60.0 + Frame,
				      N + F("none of her gun's crew from the moment she strikes (her men on her casing are the show's): %.0f frames shown after, gone at %.2f s", ShownStruck, Gone));
			}
			else
			{
				Check(Gone >= 60.0 && Gone <= 60.0 + DownWithin && Awash == 0,
				      N + F("all below %.2f s after it (%.1f at the most), none on her deck within 0.1 m of the sea: %.0f man-frames", Gone - 60.0, DownWithin, Awash) +
				          F(" (her deck under a man %.2f m over it at the least, at %.2f s)", DeckLeast, DeckLeastAt));
			}
			Check(Heard[B.TakenCue] == 0 && Heard[B.RamCue] > 0 && Heard[B.ShutCue] > 0 && Heard[B.LiftCue] == 0 && ShownHidden == 0,
			      N + F("the cues heard: shell_ram %.0f, breech_close %.0f; from the rack, no round_handoff (%.0f)", Heard[B.RamCue], Heard[B.ShutCue], Heard[B.TakenCue]) +
			          F(" nor round_lift (%.0f); none drawn on a boat the show hides (%.0f)", Heard[B.LiftCue], ShownHidden));
			Check(Model.bRead && Seen.BootsMost <= 0.15 && Seen.FeetApart >= DeckCrew::HatchApart - 1e-6 && (Case.bStrikes || Seen.WaitFrom >= 0.5),
			      N + F("each man's boots on the model's deck to the gun, at it and back (%.3f m over its edge at the most, 0.15); one at a time in her hatch (two in it %.0f frames", Seen.BootsMost, Seen.InHatchTwo) +
			          F(", feet %.2f m apart at the least, 1.8); waiting off its middle (%.2f m from it at the least)", Seen.FeetApart, Seen.WaitFrom));
		}
		// A skip while manned (the scene's SkipTo, a whole window in a tick): no cue across it, and after it no more than one
		// of each a round (the review of 24 November: every cue again on each of eleven frames).
		for (const bool bBoat : { false, true })
		{
			const GunMount Gun = bBoat ? MountOf("Submarine_GER", false) : MountOf("Freighter", true);
			const DeckCrew::GunShape Her = DeckCrew::ShapeOf(Gun);
			const DeckCrew::Blocks Kit = DeckCrew::BlocksOf(Sailors, Sailors.FileFor(bBoat ? "GER" : "UK"));
			DeckCrew::Watch W;
			DeckCrew::Told In;
			In.bBoat = bBoat;
			In.bInLine = true;
			In.bLaid = true;
			In.bAft = !bBoat;
			In.Dive = bBoat ? "surfaced" : "";
			In.Train = Gun.Rest;
			In.Elevation = 5.0;
			double Now = 100.0;
			std::vector<DeckCrew::Cue> Cues;
			for (int I = 0; I < 30 * 12; ++I)
			{
				Now += Frame;
				if (I == 30 * 8) In.FiredAt = Now;
				Cues.clear();
				DeckCrew::Step(W, In, Her, Gun, Kit, Now, &Cues);
			}
			Now += 60.0;
			Cues.clear();
			DeckCrew::Step(W, In, Her, Gun, Kit, Now, &Cues);
			const size_t AcrossSkip = Cues.size();
			std::map<std::string, int> Heard;
			int Most = 0;
			const double Fired[] = { Now + 0.5, Now + 0.5 + DeckCrew::FastestReload(Kit, bBoat) + 0.5 };
			for (int I = 0; I < 30 * 12; ++I)
			{
				Now += Frame;
				for (const double At : Fired) In.FiredAt = At <= Now ? std::max(In.FiredAt, At) : In.FiredAt;
				Cues.clear();
				DeckCrew::Step(W, In, Her, Gun, Kit, Now, &Cues);
				for (const DeckCrew::Cue& Each : Cues) Most = std::max(Most, ++Heard[Each.Name]);
			}
			Check(AcrossSkip == 0 && Most <= 2 && Heard[Kit.RamCue] == 2 && W.bManned,
			      std::string(bBoat ? "[a German boat's gun" : "[a merchant's stern gun") + F(", a 60 s skip while manned] no cue across it (%.0f), then each cue once a round: %.0f at the most of any for 2 rounds", static_cast<double>(AcrossSkip), Most) +
			          F(" (shell_ram %.0f)", Heard[Kit.RamCue]));
		}
		// A merchant's crew: at her gun as it is first told, none once she leaves her line; at her stern gun as she changes
		// to it, each gun its own crew (the old one's stand down, the new one's close up at once: none between). Her guns
		// kept loaded: her first round 2 s after her gun is told (the review of 25 November: a troopship's loader 2 s into
		// his clip, before any ram, as it fired) from the loader waiting with it rammed, one fired as her stern gun is first
		// told (a hit the battle tells is fired at once), every round at a manned gun with the breech shut, the loader's ram
		// heard before each he could load in his own time.
		{
			const GunMount Gun = MountOf("Freighter", false);
			const DeckCrew::GunShape Her = DeckCrew::ShapeOf(Gun);
			const GunMount Aft = MountOf("Freighter", true);
			const DeckCrew::GunShape HerAft = DeckCrew::ShapeOf(Aft);
			const DeckCrew::Blocks Kit = DeckCrew::BlocksOf(Sailors, Sailors.FileFor("UK"));
			DeckCrew::Watch W;
			DeckCrew::Told In;
			In.bInLine = true;
			In.Train = 30.0;
			In.Key = 77;
			int AtOnce = 0, After = 0, Passers = 0, Changed = 0, Between = 0, Rounds = 0, Shut = 0, Waited = 0, Eligible = 0, Rammed = 0;
			const double Fires[] = { 12.0, 18.0, 20.0, 27.0 };
			double LastRam = -1.0, LastFire = -1.0, OpenAt = -1.0;
			size_t Next = 0;
			for (int Tick = 0; Tick <= 40 * 30; ++Tick)
			{
				const double T = Tick / 30.0;
				In.bLaid = T >= 10.0;
				In.bInLine = T < 30.0;
				In.bAft = T >= 20.0;
				In.Train = In.bAft ? 160.0 : 30.0;
				bool bFires = false;
				while (Next < std::size(Fires) && Fires[Next] <= T + 1e-9)
				{
					In.FiredAt = Fires[Next++];
					bFires = true;
				}
				if (bFires && LastFire >= 0.0 && In.FiredAt - LastFire >= DeckCrew::FastestReload(Kit, false) && W.bAft == In.bAft)
				{
					++Eligible;
					Rammed += LastRam > LastFire && LastRam <= In.FiredAt + 1e-9 ? 1 : 0;
				}
				LastFire = bFires ? In.FiredAt : LastFire;
				const bool bWasAft = W.bAft && W.bManned;
				const bool bShutBefore = ShutAtPost(W, Kit) && W.bAft == In.bAft, bUpBefore = AtPosts(W) && W.bAft == In.bAft;
				std::vector<DeckCrew::Cue> Cues;
				DeckCrew::Step(W, In, In.bAft ? HerAft : Her, In.bAft ? Aft : Gun, Kit, T, &Cues);
				for (const DeckCrew::Cue& Each : Cues) LastRam = Each.Name == Kit.RamCue ? T : LastRam;
				if (bFires)
				{
					++Rounds;
					const bool bShut = AtPosts(W) && W.bAft == In.bAft && (bShutBefore || ShutAtPost(W, Kit));
					Shut += bShut ? 1 : 0;
					Waited += bShut && bUpBefore && bShutBefore ? 1 : 0;
					OpenAt = !bShut && OpenAt < 0.0 ? In.FiredAt : OpenAt;
				}
				Changed += W.bManned && W.bAft && !bWasAft && T >= 20.0 && T < 20.0 + Frame ? 1 : 0;
				int Up = 0;
				for (int R = 0; R < DeckCrew::Roles; ++R)
				{
					AtOnce += T >= 10.0 && T < 10.05 && W.Men[R].bShown && W.Men[R].bAtWork ? 1 : 0;
					After += T >= 30.0 && W.Men[R].bShown ? 1 : 0;
					Passers += R == Passer && W.Men[R].bShown ? 1 : 0;
					Up += W.Men[R].bShown ? 1 : 0;
				}
				Between += T >= 10.1 && T < 30.0 && Up != 3 ? 1 : 0;
			}
			Check(AtOnce == 3 * 2 && After == 0 && Passers == 0 && Changed == 1 && Between == 0,
			      F("a merchant's gun told: its layer, trainer and loader at it at once (%.0f of 6 frame-men), none once she leaves her line (%.0f), no passer (no hatch: %.0f)", AtOnce, After, Passers) +
			          F("; at her stern gun the frame she changes to it (%.0f), never without her gun's crew between (%.0f frames)", Changed, Between));
			Check(Rounds == 4 && Shut == Rounds && Waited == 3 && Eligible == 2 && Rammed == Eligible,
			      F("a merchant's guns kept loaded: every round at a manned gun with the breech shut, %.0f of %.0f (the first not at %.2f s)", Shut, Rounds, OpenAt) +
			          F("; %.0f of 3 from the loader at his post waiting with it rammed (her first among them, 2 s after her gun is told)", Waited) +
			          F(", the loader's ram heard before each he could load in his own time (%.0f of %.0f)", Rammed, Eligible));
		}
		// A boat whose hatch is where the loader can take the rounds from the passer (the American boat's, moved along her to
		// where the round in the passer's hands meets the loader's at a bearing): the passer up it last, standing in it, his
		// round meeting the loader's hands whenever he hands up, the cues of the handing heard, down it first at the alarm.
		{
			const GunMount M = MountOf("Submarine_USA", false);
			DeckCrew::GunShape S = DeckCrew::ShapeOf(M);
			const DeckCrew::Blocks B = DeckCrew::BlocksOf(Sailors, Sailors.FileFor("USA"));
			SeaSailors::RoundAt Handed, Taken;
			float First = -1.f;
			for (size_t I = 0; I + 7 < B.Loading.Track.size() && First < 0.f; I += 8)
			{
				if (B.Loading.Track[I] >= B.Taken - 1e-4f && B.Loading.Track[I + 1] > 0.5f) First = B.Loading.Track[I];
			}
			SeaSailors::RoundAlong(B.Loading, B.Long[Loader], First, Taken);
			SeaSailors::RoundAlong(B.Passing, B.Long[Passer], B.Taken - 0.01f, Handed);
			const double Reach = std::hypot(Handed.Base.X, Handed.Base.Y);
			// (the bearing whose handing place is nearest her middle line, and the hatch put there)
			double Best = 1e9, Train = M.Rest;
			for (double Turn = -60.0; Turn <= 60.0; Turn += 2.5)
			{
				const DeckCrew::Post L = DeckCrew::PostOf(S, B, M, DeckCrew::Role::Loader, M.Rest + Turn, 5.0);
				const DeckCrew::Spot X = DeckCrew::ToHer(L, Taken.Base.X, Taken.Base.Y, Taken.Base.Z);
				if (std::fabs(X.Right) < Best)
				{
					Best = std::fabs(X.Right);
					Train = M.Rest + Turn;
				}
			}
			const DeckCrew::Post L = DeckCrew::PostOf(S, B, M, DeckCrew::Role::Loader, Train, 5.0);
			const DeckCrew::Spot X = DeckCrew::ToHer(L, Taken.Base.X, Taken.Base.Y, Taken.Base.Z);
			S.HatchAlong = M.Along + X.Fore + std::sqrt(std::max(0.0, Reach * Reach - X.Right * X.Right));
			S.bScuttle = true;
			DeckCrew::Watch W;
			DeckCrew::Told In;
			In.bBoat = true;
			In.bInLine = true;
			In.Train = Train;
			In.Elevation = 5.0;
			In.Key = 99;
			const GunModel Model = ReadGun(Content, M);
			CrewSeen Seen;
			std::map<std::string, int> Heard;
			int Handing = 0, Rooted = 0, Met = 0;
			double RootMost = 0.0, GapMost = 0.0, PasserFirst = -1.0, LoaderFirst = -1.0, Gone = -1.0;
			for (int Tick = 0; Tick <= 50 * 30; ++Tick)
			{
				const double T = Tick / 30.0;
				In.bLaid = T >= 1.0 && T < 40.0;
				In.Dive = T >= 40.0 ? "periscope" : "surfaced";
				if (T >= 15.0 && std::fmod(T - 15.0, 6.0) < Frame * 0.5) In.FiredAt = T;
				std::vector<DeckCrew::Cue> Cues;
				DeckCrew::Step(W, In, S, M, B, T, &Cues);
				Seen.See(Model, S, M, W, T);
				for (const DeckCrew::Cue& Each : Cues) ++Heard[Each.Name];
				const DeckCrew::Shown& Is = W.Men[Passer];
				PasserFirst = Is.bShown && PasserFirst < 0.0 ? T : PasserFirst;
				LoaderFirst = W.Men[Loader].bShown && LoaderFirst < 0.0 ? T : LoaderFirst;
				if (Is.bShown)
				{
					const DeckCrew::Spot Hatch = DeckCrew::HatchOf(S, M);
					RootMost = std::max(RootMost, std::hypot(Is.At.Root.Fore - Hatch.Fore, Is.At.Root.Right - Hatch.Right));
					++Rooted;
				}
				if (Is.bShown && Is.bAtWork)
				{
					++Handing;
					const double Gap = HandOffGap(B, Is.At, W.Men[Loader].At);
					GapMost = std::max(GapMost, Gap);
					Met += Gap <= DeckCrew::HandOffNear + 1e-9 ? 1 : 0;
				}
				bool bAny = false;
				for (const DeckCrew::Shown& Man : W.Men) bAny = bAny || Man.bShown;
				Gone = T >= 40.0 && !bAny && Gone < 0.0 ? T : Gone;
			}
			Check(DeckCrew::PasserEver(S, B, M) && Handing > 300 && Met == Handing && Rooted > 0 && RootMost <= 0.1 && PasserFirst > LoaderFirst,
			      F("[a boat whose hatch is where the passer can hand up] the passer up it after the loader (%.2f s, %.2f), standing in its middle (%.3f m from it at the most, 0.1)", PasserFirst, LoaderFirst, RootMost) +
			          F(", handing up %.0f frames, his round meeting the loader's hands on each (%.3f m apart at the most, 0.04)", Handing, GapMost));
			Check(Heard[B.TakenCue] > 0 && Heard[B.LiftCue] > 0 && Seen.FeetApart >= DeckCrew::HatchApart - 1e-6 && Gone > 40.0 && Gone <= 40.0 + DownWithin,
			      F("[a boat whose hatch is where the passer can hand up] round_handoff heard %.0f times, round_lift %.0f; one at a time in the hatch (feet %.2f m apart at the least)", Heard[B.TakenCue], Heard[B.LiftCue], Seen.FeetApart) +
			          F(", all below %.2f s after the alarm", Gone - 40.0));
		}
	}

	/**
	 * A deck gun's crew (Battle/DeckCrewCore; Brian, 24 November 2026: the sailor files' gun_layer, gun_trainer, gun_loader
	 * and ammo_passer, and hands_up): every navy's file has its 27 clips (at the least the 14 the men in the water need),
	 * the gun clips' blocks and the loader's and the passer's events; the bank's key holds every act and pose; each gun's
	 * shape against the battle modeller's file; the layer's and the trainer's hands on their wheels' rims through their
	 * clips, the loader's fist on the round's base as he rams it and the round from the passer's hands to his; at every
	 * bearing the gun turns to and lift, each man's feet on the deck there, none of him in its mount or barrel (nor the
	 * loader in its barrel run back as it fires), the round laid home along the model's own bore; a boat's crew up the
	 * hatch when she is up and her gun told, the loader's ram before each round the show fires, all below before her
	 * casing is awash, none on a boat the show hides; a merchant's at hers until she leaves her line; the files' hands up
	 * for a struck boat's crew, idle without it; the scene's gather and draw pinned in its source.
	 */
	void TestCrew(const std::string& Root)
	{
		if (Root.empty())
		{
			return;
		}
		using SeaSailors::Act;
		const std::string Content = Root + "/../../Content/";
		SeaSailors::Crew Sailors;
		std::string Why;
		Check(ReadSailors(Content, Sailors, Why), "the six navies' sailor files read with the gun crew's clips:" + Why);
		for (int K = 0; K < Sailors.Files(); ++K)
		{
			const std::string N = "[" + Sailors.Army(K) + " sailors] ";
			int Has = 0;
			for (int A = 0; A < SeaSailors::Acts; ++A) Has += Sailors.Has(K, static_cast<Act>(A)) ? 1 : 0;
			Check(Sailors.File(K).Clips.size() >= 14 && Has == SeaSailors::Acts,
			      N + F("%.0f clips (14 at the least), %.0f of the scene's %.0f acts", static_cast<double>(Sailors.File(K).Clips.size()), Has, SeaSailors::Acts));
			const DeckCrew::Blocks B = DeckCrew::BlocksOf(Sailors, K);
			Check(B.bGun && B.TakenCue == "round_handoff" && B.RamCue == "shell_ram" && B.ShutCue == "breech_close" && B.LiftCue == "round_lift" && B.Wheels[0].Radius > 0.f &&
			          B.Wheels[1].Radius > 0.f && !B.Wheels[0].Turn.empty() && !B.Wheels[1].Turn.empty() && B.Loading.bBreech && !B.Passing.Track.empty() &&
			          B.Pair.X > 0.f && B.ClimbLong > 0.f && B.RunLong > 0.f && B.StandLong > 0.f && B.DuckLong > 0.f,
			      N + F("the gun clips' blocks: the wheels, the rounds, the pair; the loader's round taken at %.2f s, rammed at %.2f, the breech shut at %.2f", B.Taken, B.Ram, B.Shut) +
			          F(", the passer's lifted at %.2f (round_handoff, shell_ram, breech_close, round_lift)", B.Lift));
			// (the layer's and the trainer's hands on their wheels' rims all through their clips: the fingers about it, the
			// wrists by it)
			for (int R = 0; R < 2; ++R)
			{
				const Act A = R == 0 ? Act::Layer : Act::Trainer;
				const SeaSailors::Wheel& W = B.Wheels[R];
				double Fingers = 0.0, Wrists = 0.0;
				for (float T = 0.f; T < B.Long[R]; T += 1.f / 30.f)
				{
					for (const char* Bone : { "hand_l", "hand_r", "fingers_l", "fingers_r" })
					{
						const LandMen::Vec3 P = Sailors.BoneAt(K, A, T, Bone);
						const double Off = std::hypot(P.X - W.Hub[0], std::hypot(P.Y - W.Hub[1], P.Z - W.Hub[2]) - W.Radius);
						(Bone[0] == 'f' ? Fingers : Wrists) = std::max(Bone[0] == 'f' ? Fingers : Wrists, Off);
					}
				}
				Check(Fingers <= 0.03 && Wrists <= 0.08, N + (R == 0 ? "the layer's" : "the trainer's") +
				                                             F(" hands on his wheel's rim all through his clip: his fingers within %.3f m of it (0.03), his wrists %.3f (0.08)", Fingers, Wrists));
			}
			// (the loader's fist on the round's base as he rams it; the round from the passer's hands to his as he takes it)
			SeaSailors::RoundAt Rammed, Taken, Handed;
			const bool bRammed = SeaSailors::RoundAlong(B.Loading, B.Long[2], B.Ram, Rammed) && Rammed.bShown;
			const LandMen::Vec3 Fist = Sailors.BoneAt(K, Act::Loader, B.Ram, "fingers_r");
			const double FistOff = std::sqrt((Fist.X - Rammed.Base.X) * (Fist.X - Rammed.Base.X) + (Fist.Y - Rammed.Base.Y) * (Fist.Y - Rammed.Base.Y) + (Fist.Z - Rammed.Base.Z) * (Fist.Z - Rammed.Base.Z));
			Check(bRammed && FistOff <= 0.05, N + F("the loader's fist on the round's base as he rams it (%.2f s): %.3f m off (0.05)", B.Ram, FistOff));
			float First = -1.f;
			for (size_t I = 0; I + 7 < B.Loading.Track.size() && First < 0.f; I += 8)
			{
				if (B.Loading.Track[I] >= B.Taken - 1e-4f && B.Loading.Track[I + 1] > 0.5f) First = B.Loading.Track[I];
			}
			SeaSailors::RoundAlong(B.Loading, B.Long[2], First, Taken);
			SeaSailors::RoundAlong(B.Passing, B.Long[3], B.Taken - 0.01f, Handed);
			const double HandOff = std::sqrt(std::pow(Handed.Base.X - B.Pair.X - Taken.Base.X, 2.0) + std::pow(Handed.Base.Y - B.Pair.Y - Taken.Base.Y, 2.0) +
			                                 std::pow(Handed.Base.Z - B.Pair.Z - Taken.Base.Z, 2.0));
			Check(Handed.bShown && Taken.bShown && First >= B.Taken && First <= B.Taken + 0.1f && HandOff <= 0.02,
			      N + F("the round from the passer's hands to the loader's as he takes it (%.2f s, first his at %.2f): %.3f m apart (0.02)", B.Taken, First, HandOff));
			// (hands up: a struck boat's man standing at his place, his hands up, by the file's own clip)
			SeaShow::ManOut Giving;
			Giving.Doing = SeaShow::Swim::HandsUp;
			Giving.Nation = Sailors.Army(K);
			Check(Sailors.Place(K, Giving, SeaSailors::Aboard(), 100.0, -1.0).What == Act::HandsUp, N + "a struck boat's man giving up: hands_up");
		}
		// (the art director's British sea jersey, oatmeal (6bab72b3, 22 November): made in Content only, and lost once to a
		// byte copy of the soldiers thread's file; kept over every import since)
		{
			const std::string Uk = TextOf(Content + "Models/Soldiers/soldier_UK_sailor.json");
			const int K = Sailors.FileFor("UK");
			SeaSailors::Dress Jersey;
			if (K >= 0) Sailors.DressOf(K, 1, false, Jersey);
			bool bOatmeal = false, bCream = false;
			for (int I = 0; I < 64; ++I)
			{
				bOatmeal = bOatmeal || (Jersey.bSet[I] && Jersey.Paint[I] == 0xAEA285u);
				bCream = bCream || (Jersey.bSet[I] && Jersey.Paint[I] == 0xD3CAB2u);
			}
			Check(Uk.find("\"sweater\":{\"shirt_own\":\"AEA285\",\"about\":\"the oatmeal (undyed wool) roll-neck sea jersey") != std::string::npos &&
			          Uk.find("D3CAB2") == std::string::npos && bOatmeal && !bCream,
			      "the British sailors' sea jersey oatmeal (AEA285, the art director's of 22 November), not the cream: the one in seven outside warm water wears it");
		}
		// (a file without the gun crew's clips or hands up reads still: its men stand for hands up, its guns unmanned)
		{
			std::string Old = TextOf(Content + "Models/Soldiers/soldier_GER_sailor.json");
			for (const char* Clip : { "\"hands_up\":{", "\"gun_layer\":{", "\"gun_trainer\":{", "\"gun_loader\":{", "\"ammo_passer\":{" })
			{
				const size_t At = Old.find(Clip);
				if (At != std::string::npos) Old.replace(At + 1, 1, "x");
			}
			SeaSailors::Crew Before;
			std::string BeforeWhy;
			const bool bRead = Before.AddFile("GER", Old, BeforeWhy);
			SeaShow::ManOut Giving;
			Giving.Doing = SeaShow::Swim::HandsUp;
			Check(bRead && !Before.Has(0, Act::HandsUp) && !Before.Has(0, Act::Loader) && !DeckCrew::BlocksOf(Before, 0).bGun &&
			          Before.Place(0, Giving, SeaSailors::Aboard(), 100.0, -1.0).What == Act::Stand,
			      "a file of before 24 November (no gun crew's clips, no hands_up) reads: its struck men stand (idle), its guns unmanned" + BeforeWhy);
		}
		// The bank's key: every act and every act's poses packed apart (six bits each), as the scene's SailorKey packs them.
		{
			int Most = 0;
			for (int A = 0; A < SeaSailors::Acts; ++A) Most = std::max(Most, SeaSailors::PosesOf(static_cast<Act>(A)));
			const std::string Scene = TextOf(Root + "/Battle/SeaBattle.cpp");
			// (the gun crew's poses banked as few as the layer's and the trainer's: a far man is small, and each pose a copy,
			// its draw; a copy whose men are all put away hidden)
			int GunPoses = 0;
			for (const Act A : { Act::HandsUp, Act::Layer, Act::Trainer, Act::Loader, Act::Passer }) GunPoses = std::max(GunPoses, SeaSailors::PosesOf(A));
			Check(GunPoses <= 12 && Scene.find("const bool bAnyIn = SailorFree[Copy].Num() < SailorCopies[Copy]->GetInstanceCount();") != std::string::npos &&
			          Scene.find("SailorCopies[Copy]->SetVisibility(bAnyIn);") != std::string::npos,
			      F("the gun crew's acts banked at %.0f poses at the most (12) and a copy hidden while all its men are put away", GunPoses));
			Check(SeaSailors::Acts <= 64 && Most <= 64 && Scene.find("static_assert(SeaSailors::Acts <= 64, \"SailorKey packs an act in six bits\");") != std::string::npos &&
			          Scene.find("return ((((((static_cast<int64>(File) * 64 + Dress) * 64 + What) * 64 + Pose) * 4 + Lod) * 2 + (bMirror ? 1 : 0)) * 2 + (bOiled ? 1 : 0));") != std::string::npos,
			      F("the pose bank's key holds %.0f acts and up to %.0f poses of each (64 of each at the most)", SeaSailors::Acts, Most));
		}
		if (Sailors.Files() == 0)
		{
			return;
		}

		// Each gun's shape against its file, and its crew at it at every bearing it turns to.
		const struct
		{
			const char* File;
			bool bStern;
			const char* Navy; // the navy whose men man it here
		} Guns[] = {
			{ "Submarine_GER", false, "GER" }, { "Submarine_USA", false, "USA" }, { "Submarine", false, "JAP" }, { "Submarine", false, "UK" },
			{ "Freighter", false, "UK" }, { "Freighter", true, "USA" }, { "Tanker", false, "UK" }, { "Tanker", true, "SOV" }, { "Troopship", false, "UK" }, { "Troopship", true, "ITA" },
		};
		for (const auto& Asked : Guns)
		{
			const GunMount M = MountOf(Asked.File, Asked.bStern);
			const DeckCrew::GunShape S = DeckCrew::ShapeOf(M);
			const GunModel G = ReadGun(Content, M);
			const int K = Sailors.FileFor(Asked.Navy);
			const DeckCrew::Blocks B = DeckCrew::BlocksOf(Sailors, K);
			const std::string N = std::string("[") + Asked.File + (Asked.bStern ? " stern gun, " : " gun, ") + Asked.Navy + " crew] ";
			Check(!S.File.empty() && G.bRead && B.bGun, N + "its shape, its model and its crew's blocks");
			if (S.File.empty() || !G.bRead || !B.bGun)
			{
				continue;
			}
			const auto MountLow = G.Mount.empty() ? 0.0 : G.Mount.begin()->first * 0.05;
			const double BarrelBack = G.Barrel.empty() ? 0.0 : -G.Barrel.begin()->first * 0.1;
			if (S.bBoat)
			{
				Check(std::fabs(S.PivotUp - G.BarrelZ) < 0.01 && std::fabs(S.PivotAhead - G.BarrelU) < 0.01 && std::fabs(S.Foot - MountLow) < 0.05 &&
				          std::fabs(S.BreechBack - BarrelBack) < 0.1 && S.LiftMost == 35.0 && S.Built == 2.0,
				      N + F("its barrel's pivot %.3f m up (the file's %.3f), its foot %.3f", S.PivotUp, G.BarrelZ, S.Foot) + F(" (the mount's lowest %.2f), its breech %.3f behind", MountLow, S.BreechBack));
				// (her deck flats' edge each quarter metre along her by the gun, DeckEdges: the narrowest the flats run unbroken
				// out from her middle line on both sides within that quarter, a centimetre's give; the breech's ring, the
				// barrel's hindmost band, from RingBack behind the chamber's mouth and RingHalf about its bore)
				double EdgesOver = 0.0, EdgesUnder = 0.0;
				for (int Slice = 0; Slice < DeckCrew::DeckEdgeCount; ++Slice)
				{
					const double Mid = DeckCrew::DeckEdgeFrom + DeckCrew::DeckEdgeStep * Slice;
					double Least = 1e9;
					for (double Fore = Mid - 0.5 * DeckCrew::DeckEdgeStep; Fore <= Mid + 0.5 * DeckCrew::DeckEdgeStep + 1e-9; Fore += 0.0125)
					{
						const double Floor = DeckCrew::DeckUnder(S, M, Fore, 1.0);
						double R = 0.3;
						while (R <= 3.0 && ModelDeck(G, Fore, R, Floor, 0.3) > -1e8 && ModelDeck(G, Fore, -R, Floor, 0.3) > -1e8) R += 0.005;
						Least = std::min(Least, R - 0.005);
					}
					EdgesOver = std::max(EdgesOver, S.DeckEdges[static_cast<size_t>(Slice)] - Least);
					EdgesUnder = std::max(EdgesUnder, Least - S.DeckEdges[static_cast<size_t>(Slice)]);
				}
				const auto Ring = G.Barrel.find(static_cast<int>(std::floor(-S.BreechBack * std::cos(S.Built * Pi / 180.0) / 0.1)));
				const double RingAxis = G.BarrelZ - S.BreechBack * std::sin(S.Built * Pi / 180.0);
				Check(S.bDeckEdges && EdgesOver <= 1e-9 && EdgesUnder <= 0.01 && Ring != G.Barrel.end() &&
				          std::fabs(Ring->first * 0.1 + S.BreechBack + S.RingBack) < 0.005 && std::fabs(std::max(-Ring->second.Lo, Ring->second.Hi) - S.RingHalf) < 0.005 &&
				          std::fabs(std::max(RingAxis - Ring->second.ZLo, Ring->second.ZHi - RingAxis) - S.RingHalf) < 0.01,
				      N + F("her deck flats' edge each quarter metre by the gun as the model's (%.3f m wider at the most, %.3f narrower)", EdgesOver, EdgesUnder) +
				          F("; the breech's ring %.3f m behind its chamber's mouth and %.3f about its bore", S.RingBack, S.RingHalf));
				Check(S.HatchAlong == (std::fabs(S.Boat.ForeHatch - M.Along) <= std::fabs(S.Boat.AftHatch - M.Along) ? S.Boat.ForeHatch : S.Boat.AftHatch) &&
				          S.bScuttle == (std::string(Asked.File) == "Submarine_USA"),
				      N + F("her crew's hatch %.1f m along her, the gun %.1f: a man handing the rounds up there ", S.HatchAlong, M.Along) + (S.bScuttle ? "(near enough)" : "(none: too far)"));
			}
			else
			{
				Check(std::fabs(S.PivotUp - G.BarrelZ) < 0.01 && std::fabs(S.PivotAhead - G.BarrelU) < 0.01 && std::fabs(S.Back + G.MountBackU) < 0.01 && S.Foot == M.Deck,
				      N + F("its barrel's pivot %.3f m up and %.2f ahead (the file's), its gunhouse's open back %.2f m behind its middle", S.PivotUp, S.PivotAhead, S.Back));
			}
			// At every bearing it turns to, lying level and laid up: each man's feet on her deck, none of him in the gun (each 2.5
			// degrees, her beams among them, the review of 24 November: 15 degrees missed the German loader over her side
			// abeam; how near the men come to each other and to the gun, and the round home, each 15 and as laid up most).
			int Posts = 0, RootsOff = 0, FeetOff = 0, InIt = 0, InRun = 0, OffBore = 0, Corners = 0;
			double Overhang = 0.0, Deepest = 0.0, RunDeepest = 0.0, BoreWorst = 0.0, MouthWorst = 0.0, RootWorst = 0.0;
			// (the passer in her hatch, at the trains he hands up at, his round meeting the loader's hands; the loader's hands
			// over the gun's own chamber's mouth, laid up to the show's usual 10 degrees; how far he is moved in)
			int PasserTrains = 0, PasserMet = 0;
			double PasserFrom = 0.0, PasserGap = 0.0, HighLeast = 1e9, HighMost = -1e9, ShiftMost = 0.0, EasedMost = 0.0;
			// (where the worst of each was: the man, the bearing off where it stows and the lift)
			double RootAt[3] = { -1.0, 0.0, 0.0 }, OverAt[3] = { -1.0, 0.0, 0.0 }, DeepAt[3] = { -1.0, 0.0, 0.0 }, BoreAt3[3] = { -1.0, 0.0, 0.0 };
			// (the loader into the layer or the trainer, and where; his turns about the breech)
			double CrewInto = -1e9, CrewAt[3] = { -1.0, 0.0, 0.0 }, TurnedMost = 0.0;
			int TurnedPosts = 0, TurnedSlid = 0;
			auto Mark = [](double* Where, double Who, double Bearing, double Laid)
			{
				Where[0] = Who;
				Where[1] = Bearing;
				Where[2] = Laid;
			};
			const double Lifts[] = { 0.0, 5.0, 10.0, 15.0, S.bBoat ? 30.0 : 45.0 };
			std::vector<double> Turns;
			for (double Turn = -M.Arc; Turn <= M.Arc + 1e-9; Turn += 2.5) Turns.push_back(Turn);
			for (const double Beam : { -90.0, 90.0 })
			{
				if (std::fabs(Beam) <= M.Arc && std::find_if(Turns.begin(), Turns.end(), [Beam](double Each) { return std::fabs(Each - Beam) < 1e-9; }) == Turns.end()) Turns.push_back(Beam);
			}
			for (const double Turn : Turns)
			{
				const bool bCoarse = std::fabs(std::remainder(Turn, 15.0)) < 1e-9;
				for (const double Lift : Lifts)
				{
					const bool bDeep = bCoarse && (Lift == 0.0 || Lift == 15.0 || Lift >= 30.0);
					const double Train = M.Rest + Turn, Bears = DeckCrew::MountTrain(M, Train);
					for (int R = 0; R < DeckCrew::Roles; ++R)
					{
						const DeckCrew::Post P = DeckCrew::PostOf(S, B, M, static_cast<DeckCrew::Role>(R), Train, Lift);
						if (R == static_cast<int>(DeckCrew::Role::Passer))
						{
							// (in her hatch's middle; handing up only where his round meets the loader's hands)
							if (!S.bBoat) continue;
							const DeckCrew::Post Loading = DeckCrew::PostOf(S, B, M, DeckCrew::Role::Loader, Train, Lift);
							PasserFrom = std::max(PasserFrom, std::hypot(P.Root.Fore - (S.HatchAlong - M.Along), P.Root.Right));
							if (DeckCrew::PasserHolds(S, B, M, Loading))
							{
								++PasserTrains;
								const double Gap = HandOffGap(B, P, Loading);
								PasserGap = std::max(PasserGap, Gap);
								PasserMet += Gap <= DeckCrew::HandOffNear + 1e-9 ? 1 : 0;
							}
							continue;
						}
						++Posts;
						if (R == static_cast<int>(DeckCrew::Role::Loader))
						{
							ShiftMost = std::max(ShiftMost, P.Shift);
							if (S.bBoat && Lift <= 10.0)
							{
								const double High = DeckCrew::BreechHigh(S, B, M, P, Train, Lift);
								HighLeast = std::min(HighLeast, High);
								HighMost = std::max(HighMost, High);
							}
						}
						if (R == static_cast<int>(DeckCrew::Role::Loader) && bDeep)
						{
							TurnedPosts += P.Turned != 0.0 ? 1 : 0;
							TurnedSlid += P.Slid > DeckCrew::SlideMost * std::clamp((DeckCrew::SlideTo - std::fabs(P.Turned)) / (DeckCrew::SlideTo - DeckCrew::TurnNear), 0.0, 1.0) + 1e-9 ? 1 : 0;
							TurnedMost = std::max(TurnedMost, std::fabs(P.Turned));
							for (const DeckCrew::Role Wheel : { DeckCrew::Role::Layer, DeckCrew::Role::Trainer })
							{
								const DeckCrew::Post Beside = DeckCrew::PostOf(S, B, M, Wheel, Train, Lift);
								const double Into = BodiesInto(Sailors, K, Act::Loader, B.Long[R], P, DeckCrew::ActOf(Wheel), B.Long[static_cast<int>(Wheel)], Beside, 0.25f);
								if (Into > CrewInto) Mark(CrewAt, static_cast<double>(Wheel), Turn, Lift);
								CrewInto = std::max(CrewInto, Into);
							}
						}
						const double Floor = P.Root.Up;
						const double Under = ModelDeck(G, P.Root.Fore, P.Root.Right, Floor, 0.3);
						if (std::fabs(Under - Floor) > RootWorst) Mark(RootAt, R, Turn, Lift);
						RootWorst = std::max(RootWorst, std::fabs(Under - Floor));
						if (std::fabs(Under - Floor) > 0.08) ++RootsOff;
						{
							// (his feet: heels 0.2 m behind his root, toes 0.33 ahead, 0.21 either side)
							Corners += 4;
							const double In = BootsOff(G, P, &FeetOff);
							if (In > Overhang) Mark(OverAt, R, Turn, Lift);
							Overhang = std::max(Overhang, In);
						}
						// (none of him in the gun as it stands, through his clip; the loader nor in the barrel run back, as it
						// fires while he waits for the next round)
						if (!S.bBoat || !bDeep)
						{
							continue;
						}
						const Act A = DeckCrew::ActOf(static_cast<DeckCrew::Role>(R));
						std::vector<float> At, Facing;
						for (float T = 0.f; T < B.Long[R]; T += 0.1f)
						{
							const int Count = Sailors.Skin(K, A, T, LandMen::Mid, At, Facing);
							const bool bHold = R == static_cast<int>(DeckCrew::Role::Loader) && T <= DeckCrew::HoldTo + 1e-4f;
							for (int I = 0; I < Count; ++I)
							{
								const DeckCrew::Spot Her = DeckCrew::ToHer(P, At[static_cast<size_t>(I) * 3], At[static_cast<size_t>(I) * 3 + 1], At[static_cast<size_t>(I) * 3 + 2]);
								const double Depth = InGun(G, Bears, std::clamp(Lift, S.LiftLeast, S.LiftMost), 0.0, Her.Fore, Her.Right, Her.Up, 0.01);
								if (Depth > 0.0) ++InIt;
								if (Depth > Deepest) Mark(DeepAt, R, Turn, Lift);
								Deepest = std::max(Deepest, Depth);
								if (bHold)
								{
									const double Run = InGun(G, Bears, std::clamp(Lift, S.LiftLeast, S.LiftMost), 0.45, Her.Fore, Her.Right, Her.Up, 0.01);
									if (Run > 0.0) ++InRun;
									RunDeepest = std::max(RunDeepest, Run);
								}
							}
						}
						if (R == static_cast<int>(DeckCrew::Role::Loader))
						{
							// (the round laid home along the model's own bore, at the chamber's mouth or behind it)
							double Way[3];
							const DeckCrew::Spot Mouth = DeckCrew::BreechOf(S, B, M, Train, Lift, Way);
							const DeckCrew::Spot MouthHis = DeckCrew::ToHim(P, Mouth);
							DeckCrew::Post Turned = P;
							Turned.Root = DeckCrew::Spot();
							const DeckCrew::Spot BoreHis = DeckCrew::ToHim(Turned, DeckCrew::Spot{ Way[0], Way[1], Way[2] });
							SeaSailors::RoundAt Home;
							const LandMen::Vec3 MouthAt = { static_cast<float>(MouthHis.Fore), static_cast<float>(MouthHis.Right), static_cast<float>(MouthHis.Up) };
							const LandMen::Vec3 BoreAt = { static_cast<float>(BoreHis.Fore), static_cast<float>(BoreHis.Right), static_cast<float>(BoreHis.Up) };
							if (!DeckCrew::LoaderRound(B, DeckCrew::LastShown(B), true, MouthAt, BoreAt, Home))
							{
								++OffBore;
								continue;
							}
							const double Along = (Home.Base.X - MouthAt.X) * BoreAt.X + (Home.Base.Y - MouthAt.Y) * BoreAt.Y + (Home.Base.Z - MouthAt.Z) * BoreAt.Z;
							const double Ox = Home.Base.X - MouthAt.X - BoreAt.X * Along, Oy = Home.Base.Y - MouthAt.Y - BoreAt.Y * Along, Oz = Home.Base.Z - MouthAt.Z - BoreAt.Z * Along;
							const double Off = std::sqrt(Ox * Ox + Oy * Oy + Oz * Oz);
							const double Lies = Home.Way.X * BoreAt.X + Home.Way.Y * BoreAt.Y + Home.Way.Z * BoreAt.Z;
							BoreWorst = std::max(BoreWorst, Off);
							MouthWorst = std::max(MouthWorst, -Along);
							if (Off > 0.01 || Along > 1e-3 || Along < -0.06 || Lies < 0.999)
							{
								++OffBore;
								Mark(BoreAt3, Along, Turn, Lift);
							}
							// (eased onto the gun's bore from his hands, never dropping out of them as he holds it until he
							// rams it: its base off where his clip has it in his hands, a frame apart at a boat's pace, no
							// more than DropMost more up or down; rammed, it runs on up the gun's bore from his fist)
							double OffWas = 0.0;
							bool bWas = false;
							for (float T = 0.f; T <= B.Ram; T += static_cast<float>(Frame * DeckCrew::BoatRate))
							{
								SeaSailors::RoundAt Now, InHands;
								const bool bNow = DeckCrew::LoaderRound(B, T, true, MouthAt, BoreAt, Now) && Now.bShown &&
								                  SeaSailors::RoundAlong(B.Loading, B.Long[R], T, InHands) && InHands.bShown;
								const double OffNow = bNow ? static_cast<double>(Now.Base.Z - InHands.Base.Z) : 0.0;
								if (bNow && bWas) EasedMost = std::max(EasedMost, std::fabs(OffNow - OffWas));
								OffWas = OffNow;
								bWas = bNow;
							}
						}
					}
				}
			}
			Check(Posts > 0 && RootsOff == 0, N + F("%.0f posts at every bearing and lift: each man's root on her deck there (the worst %.3f m off it, %.0f more than 0.08", Posts, RootWorst, RootsOff) +
			                                      F("; man %.0f at %.0f degrees, laid %.0f)", RootAt[0], RootAt[1], RootAt[2]));
			// (a boat's casing narrows abeam her gun: the loader stood as near the breech as the barrel's run back lets him,
			// and turned about it as far as the layer and the trainer leave him room, a German's heels over its edge at the
			// most OverhangMost there)
			const double OverhangMost = 0.15;
			Check(Overhang <= OverhangMost, N + F("each man's feet on her deck: %.0f of %.0f corners of his boots over its edge, by %.3f m at the most", FeetOff, Corners, Overhang) +
			                                     F(" (%.2f: a boat's loader abeam, on her narrow casing; man %.0f at %.0f degrees", OverhangMost, OverAt[0], OverAt[1]) + F(", laid %.0f)", OverAt[2]));
			// (turned only where her casing is too narrow behind the breech, and the further turned the less nearer it; clear
			// of the men at the wheels, a merchant's gunhouse's crew close about its breech by its clips)
			const double CrewIntoMost = 0.02;
			Check(TurnedSlid == 0 && TurnedMost <= DeckCrew::TurnMost + 1e-9 && (TurnedPosts == 0 || (S.bBoat && S.DeckHalf < 2.0)),
			      N + F("the loader turned about the breech at %.0f posts, %.0f degrees at the most (%.0f nearer it than SlideMost, less beyond TurnNear and none by SlideTo), only on a boat's narrow casing", TurnedPosts, TurnedMost, TurnedSlid));
			Check(CrewInto <= CrewIntoMost, N + F("the loader clear of the layer and the trainer through their clips: into them by %.3f m at the most (%.2f; below 0 the gap", CrewInto, CrewIntoMost) +
			                                    F(": man %.0f at %.0f degrees, laid %.0f)", CrewAt[0], CrewAt[1], CrewAt[2]));
			if (S.bBoat)
			{
				Check(InIt == 0 && InRun == 0, N + F("none of the crew in the gun's mount or barrel through their clips (%.0f points in, %.3f m deep", InIt, Deepest) +
				                                   F(": man %.0f at %.0f degrees, laid %.0f)", DeepAt[0], DeepAt[1], DeepAt[2]) +
				                                   F(", nor the waiting loader in its barrel run back as it fires (%.0f, %.3f)", InRun, RunDeepest));
				Check(OffBore == 0, N + F("the round eased home along the model's own bore at every bearing and lift: %.0f off, %.3f m from its line at the most, %.3f behind the chamber's mouth (0.06", OffBore, BoreWorst, MouthWorst) +
				                        F("; %.3f along it at %.0f degrees, laid %.0f)", BoreAt3[0], BoreAt3[1], BoreAt3[2]));
				// (his hands as his clip has them over the gun's own chamber's mouth, as measured: the files' gun_loader puts the
				// breech 1.12 m over his feet, these guns' mouths lie lower over her casing, and lower the more the gun is laid
				// up; no place on her deck lowers his hands, so the round is eased down across the gap from when he lays it in
				// the tray (Laid) until it is home, off his hands a frame at a time no more than DropMost, until the soldiers
				// thread's gun_loader has the boats' breech height: BreechGapMost bounds the gap as it is, not a claim it is
				// closed)
				const double BreechGapMost = 0.30, DropMost = 0.04;
				Check(HighMost <= BreechGapMost && HighLeast >= -0.05 && EasedMost <= DropMost,
				      N + F("the loader's hands over the gun's own chamber's mouth, laid 0 to 10 degrees: %.3f to %.3f m (known, %.2f at the most", HighLeast, HighMost, BreechGapMost) +
				          F("; the round eased across it off his hands, %.3f m a frame up or down at the most, %.2f)", EasedMost, DropMost));
				Check(PasserFrom <= 0.1 && PasserMet == PasserTrains && PasserTrains == 0 && !DeckCrew::PasserEver(S, B, M) && ShiftMost <= 0.30,
				      N + F("the passer's root in her hatch's middle at every train (%.3f m from it at the most, 0.1); handing up at %.0f posts", PasserFrom, PasserTrains) +
				          F(" (his round meeting the loader's hands at %.0f, %.3f m apart at the most): none, her hatch too far from the loader's hands for their pair", PasserMet, PasserGap) +
				          F(", the loader's rounds from a rack; the loader moved in from his clip's post %.3f m at the most (0.30)", ShiftMost));
			}
		}
		TestCrewTimes(Content, Sailors);
		TestCrewSweep(Content, Sailors);
	}
}

int main(int ArgC, char** ArgV)
{
	bStory = std::getenv("CONVOY_LOOKS_STORY") != nullptr;
	TestModelRows(ArgC > 1 ? std::string(ArgV[1]) : std::string());
	TestBoats();
	TestUnseen();
	TestBursts();
	TestPatterns();
	TestStarshell();
	TestEnds();
	TestRescue(ArgC > 1 ? std::string(ArgV[1]) : std::string());
	TestMeshShape();
	TestGuns(ArgC > 1 ? std::string(ArgV[1]) : std::string());
	TestCrew(ArgC > 1 ? std::string(ArgV[1]) : std::string());
	TestCasing(ArgC > 1 ? std::string(ArgV[1]) : std::string());
	TestStageSinking(ArgC > 1 ? std::string(ArgV[1]) : std::string());
	TestFlagAndVents();
	std::printf("convoy looks: %d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
