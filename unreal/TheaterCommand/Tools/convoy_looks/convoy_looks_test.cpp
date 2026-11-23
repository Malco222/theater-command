// The submarine scene's looks (Battle/ConvoyLooksCore), checked in the cloud: the boat's states and her dive and her
// coming up, never a pop, and the faint swirl over her deep where light falls; a depth charge's dome and column by depth
// and charge; a pattern going in where and when the show bursts it; the ring of ahead-thrown bombs; a starshell's fall
// and light; a crushed boat's boils; a boat blown riding high; a rescuer's scrambling nets where the sea scene's men climb
// her side. Plain C++, no engine.
// Usage: Tools/convoy_looks/run.sh   (CONVOY_LOOKS_STORY=1 prints a dive, the bursts by depth and a starshell). Given the
// game's source folder (run.sh gives it), it also checks the boat rows against the stage's model in its source, and the
// rescue against the sea scene's wreck model in its.

#include "Battle/ConvoyLooksCore.h"
#include "Battle/Fates.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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

		// The scene: the crew drawn from GunCrewOf on the deck the stage gives, standing or crouched by the sailor files'
		// idle and cower; a boat's hits' looks put out while she is told down or under (as the sea show's check has it); a
		// merchant's hits by the table's "ap" family.
		const std::string Scene = TextOf(Root + "/Battle/SeaBattle.cpp"), Sailors = TextOf(Root + "/Battle/SeaSailors.cpp");
		Check(Scene.find("const std::vector<ConvoyLook::CrewMan> Gunners = ConvoyLook::GunCrewOf(Gun, Told);") != std::string::npos &&
		          Scene.find("Stage->GunDeck(Ship.Key, GunPart, Gunner.Fore, Gunner.Right, Gun.Deck, Feet, Her)") != std::string::npos &&
		          Scene.find("const SeaSailors::Act What = Gunner.bDuck ? SeaSailors::Act::Duck : SeaSailors::Act::Stand;") != std::string::npos &&
		          Scene.find("const int32 Mine = -(Ship.Key * ConvoyLook::CrewMost + K + 1);") != std::string::npos &&
		          Sailors.find("case Act::Stand: return \"idle\";") != std::string::npos && Sailors.find("case Act::Duck: return \"cower\";") != std::string::npos,
		      "the scene mans each gun from GunCrewOf on its deck, standing or crouched (idle, cower), keyed apart from the men in the water");
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
	TestCasing(ArgC > 1 ? std::string(ArgV[1]) : std::string());
	TestStageSinking(ArgC > 1 ? std::string(ArgV[1]) : std::string());
	TestFlagAndVents();
	std::printf("convoy looks: %d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
