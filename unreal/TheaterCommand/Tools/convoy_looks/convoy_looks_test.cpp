// The submarine scene's looks (Battle/ConvoyLooksCore), checked in the cloud: the boat's states and her dive and her
// coming up, never a pop, and the faint swirl over her deep where light falls; a depth charge's dome and column by depth
// and charge; a pattern going in where and when the show bursts it; the ring of ahead-thrown bombs; a starshell's fall
// and light; a crushed boat's boils; a rescuer's scrambling net. Plain C++, no engine.
// Usage: Tools/convoy_looks/run.sh   (CONVOY_LOOKS_STORY=1 prints a dive, the bursts by depth and a starshell)

#include "Battle/ConvoyLooksCore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
	 * her class's length: 0.07 of it), her heave on the swell fading by e each 8 m under. The stand-in convoy's header:
	 * class ss 67 m, subs{scope 13, deep 80}.
	 */
	struct ShowBoat
	{
		double Up = 67.0 * 0.07, ScopeKeel = 13.0, DeepKeel = 80.0;
		double Keel = -1.0, KeelTo = 0.0, KeelFrom = 0.0, KeelSince = 0.0, KeelFor = 0.0;

		void Step(double T, const std::string& Dive, double Want = -1.0)
		{
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
		}
		double Dived() const { return std::max(0.0, Keel - Up); }
		double Calm() const { return std::exp(-std::max(0.0, Keel - Up) / 8.0); }
	};

	/** A boat's run, frames at 30 a second: surfaced to 20 s, a crash dive to periscope depth, her periscope up from 60
	 * to 120 s, deep from 130 s, up to the surface from 210 s. */
	struct Run
	{
		double MostStep = 0.0;       // the most her drawn heave moved in one frame (metres)
		double MostVisibleStep = 0.0; // the same while any of her was within HiddenUnder of the sea
		int Feathers = 0, Washes = 0, Swirls = 0, Bows = 0;
		std::vector<double> DiveWash, UpWash; // where along her each wash was, diving and coming up
		double DeckSurfaced = 99.0, ScopeHeadAtPeriscope = -99.0, TowerAtPeriscope = 99.0, TopHoused = 99.0, TopAtDeep = 99.0;
		double LeftSurface = -1.0, AtPeriscope = -1.0;
		bool bFeatherWithoutCut = false;
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
			if (T > 100.0 && T < 115.0 && !bJumpKeel)
			{
				ScopeMark = O.Heave - In.Heave;
				R.ScopeHeadAtPeriscope = O.ScopeHead.Z;
				R.TowerAtPeriscope = O.Heave + M.TowerTop * D.Up;
			}
			if (T > 125.0 && T < 130.0)
			{
				R.TopHoused = std::min(R.TopHoused, -Top);
			}
			if (T > 200.0 && T < 210.0)
			{
				R.TopAtDeep = Top;
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
					// (the bridge under, and her periscope, raised or housed in its standard, through the sea; never deep)
					if (!O.bTowerUnder || O.Heave + (M.ScopeHead + (O.bScopeShown ? M.ScopeSlide : 0.0)) * D.Up < 0.0 || Dive == "deep")
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
				std::printf("  %-13s t %5.1f %-9s keel %5.1f dived %5.1f heave %6.2f (show's %6.2f) scope %s head %5.2f feather %.2f\n", M.Name.c_str(), T, Dive.c_str(),
				            Show.Keel, Show.Dived(), O.Heave, In.Heave - Show.Dived(), O.bScopeShown ? "up" : "--", O.bScopeShown ? O.ScopeHead.Z : 0.0, O.Feather);
			}
		}
		(void)ScopeMark;
		return R;
	}

	void TestBoats()
	{
		for (const char* Nation : { "GER", "USA", "UK" })
		{
			const BoatModel M = ModelOf(Nation);
			// (the stage fits every boat to AirFeed::HullOf("ss"): 95 by 8.3 m)
			const Drawn D = DrawnOf(M, 95.0, 8.3);
			const std::string N = M.Name + ": ";
			const Run R = Steam(M, D, false, 0.0);
			Check(R.MostVisibleStep <= 2.5 * Frame + 1e-9, N + F("no pop: the most her heave moved in a frame while any of her could be seen, %.3f m", R.MostVisibleStep));
			Check(std::fabs(R.DeckSurfaced - 0.5) < 1e-6, N + F("surfaced: her casing awash, its deck 0.5 m over the still sea: %.2f m", R.DeckSurfaced));
			Check(R.TowerAtPeriscope < -0.5, N + F("at periscope depth (13 m keel) her bridge is under: %.2f m", R.TowerAtPeriscope));
			Check(R.ScopeHeadAtPeriscope > 1.0 && R.ScopeHeadAtPeriscope < 3.5, N + F("her raised periscope stands 1 to 3.5 m clear: %.2f m", R.ScopeHeadAtPeriscope));
			Check(R.TopHoused > 0.5, N + F("at periscope depth with her periscope housed, nothing of her shows: her top %.2f m under", R.TopHoused));
			Check(R.TopAtDeep < -20.0, N + F("deep (80 m), nothing of her within 20 m of the surface: %.2f m", R.TopAtDeep));
			Check(R.Feathers > 40, N + F("a feather while her periscope cuts the sea under way: %.0f spurts", R.Feathers));
			Check(!R.bFeatherWithoutCut, N + "no feather unless her periscope alone cuts the sea (her bridge under)");
			Check(R.Swirls == 1, N + F("one swirl, where her bridge went under on the dive: %.0f", R.Swirls));
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
			Check(!O.bScopeShown && O.Splashes.empty(), "surfaced with her scope housed: no stick, no splash on the first frame");
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
			Check(Next.Heave < -(M.Top * D.Up) && Next.bScopeShown && Next.ScopeHead.Z > 1.0,
			      F("the battle opens on her at periscope depth: there at once (heave %.2f, scope head %.2f)", Next.Heave, Next.ScopeHead.Z));
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

		NetIn In;
		In.Ship = P3(1000.0, 2000.0, 0.0);
		In.Heading = 90.0; // east: her starboard side to the south
		In.Long = 95.0;
		In.Beam = 10.0;
		In.Aim = P3(1100.0, 1700.0, 0.0);
		const std::vector<Rope> Net = NetOf(In);
		double Top = -99.0, Foot = 99.0, Wide = 0.0, Inboard = 99.0;
		for (const Rope& R : Net)
		{
			for (const P3& P : { R.From, R.To })
			{
				Top = std::max(Top, P.Z);
				Foot = std::min(Foot, P.Z);
				Wide = std::max(Wide, std::fabs(P.X - (1000.0 - 0.05 * 95.0)));
				Inboard = std::min(Inboard, 2000.0 - P.Y);
			}
		}
		Check(Net.size() > 40, F("a scrambling net of rope: %.0f lengths", static_cast<double>(Net.size())));
		Check(Inboard >= 5.0 - 1e-9, F("down her side toward the survivors, not through her: %.2f m out", Inboard));
		Check(Top > 2.0 && Top < 5.0 && Foot < 0.0 && Foot > -1.0, F("from her rail (%.1f m) into the sea (%.1f m)", Top, Foot));
		Check(Wide <= 2.5 + 1e-9, F("5 m wide: %.1f m each way", Wide));
		In.Aim = P3(1100.0, 2300.0, 0.0);
		double North = 0.0;
		for (const Rope& R : NetOf(In)) North = std::max(North, R.From.Y - 2000.0);
		Check(North >= 5.0, "the survivors to port: the net on her port side");
	}
}

int main()
{
	bStory = std::getenv("CONVOY_LOOKS_STORY") != nullptr;
	TestBoats();
	TestBursts();
	TestPatterns();
	TestStarshell();
	TestEnds();
	std::printf("convoy looks: %d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
