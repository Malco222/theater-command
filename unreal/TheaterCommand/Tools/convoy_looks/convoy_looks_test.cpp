// The submarine scene's looks (Battle/ConvoyLooksCore), checked in the cloud: the boat's states and her dive and her
// coming up, never a pop, and the faint swirl over her deep where light falls; a depth charge's dome and column by depth
// and charge; a pattern going in where and when the show bursts it; the ring of ahead-thrown bombs; a starshell's fall
// and light; a crushed boat's boils; a boat blown riding high; a rescuer's scrambling nets where the sea scene's men climb
// her side. Plain C++, no engine.
// Usage: Tools/convoy_looks/run.sh   (CONVOY_LOOKS_STORY=1 prints a dive, the bursts by depth and a starshell). Given the
// game's source folder (run.sh gives it), it also checks the boat rows against the stage's model in its source, and the
// rescue against the sea scene's wreck model in its.

#include "Battle/ConvoyLooksCore.h"

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
		int Feathers = 0, Washes = 0, Swirls = 0, Bows = 0;
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
			double Long = 0.0;
			const size_t L = Model.find("\"long\":");
			Check(L != std::string::npos && std::sscanf(Model.c_str() + L + 7, "%lf", &Long) == 1 && std::fabs(Long - M.Long) < 0.05, N + F("her bounds %.1f m long", Long));
			Check(M.WireTop > M.TowerTop && M.TowerFore > M.TowerAft && M.Deck < M.TowerTop && M.Top == M.ScopeTop, N + "her standard (or shears) over her bridge's rim, her casing under it");
		}
		Check(SculptedOf("Submarine_ITA").Name.empty(), "no row for a file the game does not have");
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
		     })
		{
			Check(Show.find(Said) != std::string::npos, std::string("the sea scene's rescue still has: ") + Said);
		}
		// (and the boat's end is the scene's whole: her men, her raft, her wreckage and her oil, by how she went)
		Check(Show.find("if (How == \"scuttled\") return { 0.95, 0.02, 0.02, 0.5 };") != std::string::npos && Show.find("if (How == \"crushed\") return { 0.0, 0.0, 0.0, 0.0 };") != std::string::npos,
		      "a boat scuttled leaves most of her men in the water, one crushed none: the scene's");
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
	std::printf("convoy looks: %d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
