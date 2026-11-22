#include "Battle/ConvoyLooksCore.h"

#include <algorithm>
#include <cmath>

namespace ConvoyLook
{
	namespace
	{
		constexpr double LookPi = 3.14159265358979323846;

		double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : V > Hi ? Hi : V; }
		double Lerp(double A, double B, double S) { return A + (B - A) * S; }
		/** Toward Mark from V, no more than Most. */
		double Toward(double V, double Mark, double Most) { return V + Clamp(Mark - V, -Most, Most); }
		/** A first-order ease toward Mark over Tau seconds. */
		double Settle(double V, double Mark, double Dt, double Tau) { return V + (Mark - V) * (1.0 - std::exp(-Dt / std::max(Tau, 1e-3))); }
		uint64_t Stir(uint64_t A, uint64_t B) { return A * 0x9E3779B97F4A7C15ull ^ (B + 0x632BE59BD9B4E019ull + (A << 6) + (A >> 2)); }

		// ---------------------------------------------------------------------------------------------------------
		// The boat. Each number marked: [FSR] the field note's, [SHOW] the show's, [EST] this lane's own.

		/** [EST] Surfaced, her casing's deck this far over the still sea: awash, so the swell runs over it. */
		constexpr double DeckAwash = 0.5;
		/** [EST] At periscope depth with her periscope down, and below, her highest point (her taller periscope's head) at
		 * least this far under. */
		constexpr double ClearUnder = 0.8;
		/** [EST] At periscope depth with it up, her taller periscope's head at most ScopeShows over the still sea (about 6
		 * feet: a few feet of periscope was what a boat showed), and her shorter one's at least LowUnder under. */
		constexpr double ScopeShows = 1.8, LowUnder = 0.4;
		/** The most her drawn heave moves in a second (a jump in the show's keel is eased, never a pop): well over a dive's
		 * own rate (the show's: 8 m in about 27 s, then 1 m/s [SHOW, after FSR's 25 to 30 s]), so she keeps with the show.
		 * While all of her is this far under, before and after, nothing of her can be seen and she goes at once. */
		constexpr double HeaveRate = 2.5, HiddenUnder = 6.0;
		/** [EST] A periscope run up in about 4 s, down in 3 (the model's periscopes are fixed: she rises and settles by the
		 * difference, all of her but its head under). */
		constexpr double RaiseUp = 0.25, RaiseDown = 1.0 / 3.0;
		/** [EST] The feather: none under 0.4 m/s, full from 3 m/s (6 knots: a feather was why a boat attacked slowly). */
		constexpr double FeatherFrom = 0.4, FeatherFull = 3.0;
		/** [EST] The wash: while her deck is within this of the sea (drawn metres) and she is going down or coming up, a
		 * churn this often (the kit's churn leaves foam most of a minute: a few are enough). */
		constexpr double WashBand = 1.2, WashEvery = 2.0;
		/** [EST] Surfaced and fast: spray off her bow when it drops into the swell this fast (metres a second), at most
		 * every BowEvery s, from BowFast metres a second. */
		constexpr double BowPlunge = 0.35, BowEvery = 1.5, BowFast = 4.0;
		/** [EST] A boat forced up (blowing all tanks) is on the surface in 12 s. */
		constexpr double UpFor = 12.0;
		/** [EST] Deep, a faint swirl over her where light falls: one every SwirlEvery s while she runs, lasting SwirlLast
		 * s, from SwirlFrom to SwirlTo metres across; fainter the deeper (by e each SwirlFade metres under periscope
		 * depth), none under SwirlLeast of light. */
		constexpr double SwirlEvery = 2.5, SwirlLast = 9.0, SwirlFrom = 3.0, SwirlTo = 13.0, SwirlFade = 50.0, SwirlLeast = 0.03;
	}

	P3 AheadOf(double HeadingDeg)
	{
		const double R = HeadingDeg * LookPi / 180.0;
		return P3(std::sin(R), std::cos(R), 0.0);
	}

	P3 RightOf(double HeadingDeg)
	{
		const double R = HeadingDeg * LookPi / 180.0;
		return P3(std::cos(R), -std::sin(R), 0.0);
	}

	double HeadingOf(const P3& Way, double Else)
	{
		if (std::hypot(Way.X, Way.Y) < 1e-6)
		{
			return Else;
		}
		return std::atan2(Way.X, Way.Y) * 180.0 / LookPi;
	}

	double Hash01(uint64_t N)
	{
		uint64_t Z = N + 0x9E3779B97F4A7C15ull;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		Z ^= Z >> 31;
		return static_cast<double>(Z >> 11) / 9007199254740992.0;
	}

	double Smooth(double A, double B, double X)
	{
		const double S = B > A ? Clamp((X - A) / (B - A), 0.0, 1.0) : (X >= B ? 1.0 : 0.0);
		return S * S * (3.0 - 2.0 * S);
	}

	BoatModel ModelOf(const std::string& Nation)
	{
		// The stage's model, built in code (Campaign/CampaignShips.cpp, Submarine(), for the nation tag the stage asks it
		// with; none is the kit's German): FHull{ Long, beam, deck 1.6, sheer 0.9, aft 0, rake 2.0, stern 0 }, the deck
		// laid 0.05 over it (FYard::Deck), so 1.65 amidships; the conning tower from the deck to deck + 4.6 (its top's plan
		// from 5.4 forward to 4.6 aft); the jumping wire from the bow to 5.0 forward, deck + 4.8 (radius 0.05); the
		// anti-aircraft gun at 7.0 aft, its barrels to 5.4 aft, deck + 4.4 (radius 0.09); the periscopes, fixed rods from the
		// tower's top, 1.5 forward to deck + 9.0 and amidships to deck + 7.5. Her bounds: the hull's length and the stem's
		// rake forward of it, her beam across. Tools/convoy_looks/run.sh reads CampaignShips.cpp and fails if these change.
		const bool bUsa = Nation == "USA", bGer = Nation.empty() || Nation == "GER";
		const double Hull = bUsa ? 95.0 : bGer ? 67.0 : 80.0, Rake = 2.0, Deck = 1.6 + 0.05;
		BoatModel M;
		M.Name = bUsa ? "Submarine:USA" : bGer ? "Submarine:GER" : "Submarine:" + Nation;
		M.Long = Hull + Rake;
		M.Wide = bUsa ? 8.3 : 6.2;
		M.Deck = Deck;
		M.TowerTop = Deck + 4.6;
		M.TowerFore = 5.4;
		M.TowerAft = -4.6;
		M.WireAlong = 5.0;
		M.WireTop = Deck + 4.8 + 0.05;
		M.GunAlong = -5.4;
		M.GunTop = Deck + 2.2 + 2.2 + 0.09;
		M.ScopeAlong = 1.5;
		M.ScopeTop = Deck + 9.0;
		M.LowAlong = 0.0;
		M.LowTop = Deck + 7.5;
		M.Top = M.ScopeTop;
		// (the stem rakes forward from the waterline at half her length to the deck 2 m on: its spray from between)
		M.BowAlong = Hull * 0.5 + Rake * 0.5;
		return M;
	}

	Drawn DrawnOf(const BoatModel& M, double HullLong, double HullBeam)
	{
		Drawn D;
		D.Along = HullLong > 1.0 && M.Long > 1.0 ? HullLong / M.Long : 1.0;
		D.Up = HullBeam > 0.5 && M.Wide > 0.5 ? HullBeam / M.Wide : 1.0;
		return D;
	}

	double ScopeClearOf(const BoatModel& M, const Drawn& D)
	{
		return std::max(0.3, std::min(ScopeShows, (M.ScopeTop - M.LowTop) * D.Up - LowUnder));
	}

	double ForcedUpFor() { return UpFor; }
	double SwirlLife() { return SwirlLast; }

	void ForceUp(BoatState& S, double T, double Sub)
	{
		if (S.ForcedAt < 0.0)
		{
			S.ForcedAt = T;
			S.ForcedFrom = std::max(0.0, Sub);
		}
	}

	bool SwirlAt(const Swirl& W, double T, double& OutRadius, double& OutShown)
	{
		const double Age = T - W.T0;
		OutRadius = 0.0;
		OutShown = 0.0;
		if (Age < 0.0 || Age > SwirlLast)
		{
			return false;
		}
		const double S = Age / SwirlLast;
		// (it opens out as it goes, quickly at first; it shows in a second and fades over the rest)
		OutRadius = 0.5 * Lerp(SwirlFrom, SwirlTo, 1.0 - (1.0 - S) * (1.0 - S));
		OutShown = W.Strength * Smooth(0.0, 1.0, Age) * (1.0 - Smooth(0.25, 1.0, S));
		return true;
	}

	BoatOut StepBoat(BoatState& S, const BoatIn& In, const BoatModel& M, const Drawn& D)
	{
		BoatOut Out;
		const double Dt = S.bBegun ? Clamp(In.T - S.T, 0.0, 10.0) : 0.0;
		// (a long step, a picture run's skip or the first frame: she is where she should be, at once)
		const bool bJump = !S.bBegun || Dt > 1.0;
		double Sub = std::isfinite(In.Dived) ? std::max(0.0, In.Dived) : 0.0;
		if (S.ForcedAt >= 0.0)
		{
			Sub = std::min(Sub, Lerp(S.ForcedFrom, 0.0, Smooth(0.0, UpFor, In.T - S.ForcedAt)));
		}
		// Her periscope: up when the show says it is up (never deep), and down. The model's periscopes are fixed rods, so
		// "up" is her riding so high that the taller one's head is out, and "down" so low that it is under.
		const bool bWantScope = In.bScope && In.Dive != "deep";
		const double Raise = bJump ? (bWantScope ? 1.0 : 0.0) : Toward(S.Raise, bWantScope ? 1.0 : 0.0, (bWantScope ? RaiseUp : RaiseDown) * Dt);
		const double Raised = Smooth(0.0, 1.0, Raise);
		// (her keel surfaced, as the show has it, and so how far under her surfaced trim she is at periscope depth)
		const double SurfacedKeel = std::max(0.0, In.Keel - In.Dived);
		const double ScopeSub = std::max(1.0, In.ScopeKeel - SurfacedKeel);
		const double TopDrawn = M.Top * D.Up;
		const double DeckDrawn = M.Deck * D.Up;
		// Her mark: the swell's lift (the show's, already fading as she goes down), less her trim awash (surfaced only),
		// less how far under she is. Down to periscope depth that is the looks' own, whatever her model: in step with the
		// show's depth (whose easing it keeps), from nothing to where her taller periscope's head is ScopeClearOf out (up)
		// or ClearUnder under (down); deeper, the show's depth below periscope depth as drawn (the stage stretches her up by
		// D.Up, so her depth is stretched alike).
		const double Going = Clamp(Sub / ScopeSub, 0.0, 1.0);
		const double Awash = std::max(0.0, DeckDrawn - DeckAwash) * (1.0 - Going);
		const double AtScope = TopDrawn + ClearUnder - Raised * (ClearUnder + ScopeClearOf(M, D));
		const double Under = Going * AtScope + std::max(0.0, Sub - ScopeSub) * D.Up;
		const double Mark = In.Heave - Awash - Under;
		if (!S.bBegun && !(In.Keel > 0.0))
		{
			// (the show's very first frame, before it has trimmed her, gives her no keel: drawn where that frame has her, and
			// her first frame is the next, so a boat the battle opens on dived is there at once, not seen going down)
			Out.Heave = Mark;
			return Out;
		}
		S.Raise = Raise;
		const double Rate = !bJump && Dt > 1e-6 ? (Sub - S.Sub) / Dt : 0.0;
		if (bJump)
		{
			S.Heave = Mark;
			S.SubRate = 0.0;
		}
		else
		{
			const bool bHidden = S.Heave + TopDrawn < -HiddenUnder && Mark + TopDrawn < -HiddenUnder;
			S.Heave = bHidden ? Mark : Toward(S.Heave, Mark, HeaveRate * Dt);
			S.SubRate = Settle(S.SubRate, Rate, Dt, 1.0);
		}
		const double SwellRate = !bJump && Dt > 1e-6 ? (In.Heave - S.SwellLast) / Dt : 0.0;
		S.Sub = Sub;
		S.SwellLast = In.Heave;
		Out.Heave = S.Heave;

		const P3 Fore = AheadOf(In.Heading), Side = RightOf(In.Heading);
		const P3 Mid(In.At.X, In.At.Y, 0.0);
		// Her model as the stage draws it: stretched, lifted by her heave and turned by her trim about her waterline
		// amidships (bow up, what is forward rises).
		const double Trim = (std::isfinite(In.Pitch) ? Clamp(In.Pitch, -30.0, 30.0) : 0.0) * LookPi / 180.0;
		const double TrimCos = std::cos(Trim), TrimSin = std::sin(Trim);
		auto Height = [&](double Along, double Up) { return S.Heave + Up * D.Up * TrimCos + Along * D.Along * TrimSin; };
		Out.BridgeZ = std::max({ Height(M.TowerFore, M.TowerTop), Height(M.TowerAft, M.TowerTop), Height(M.WireAlong, M.WireTop), Height(M.GunAlong, M.GunTop) });
		Out.ScopeZ = Height(M.ScopeAlong, M.ScopeTop);
		Out.LowZ = Height(M.LowAlong, M.LowTop);
		const double DeckZ = S.Heave + DeckDrawn;
		const P3 ScopeAt = Mid + Fore * (M.ScopeAlong * D.Along * TrimCos);
		Out.bScopeShown = Out.ScopeZ > 0.0 && Out.BridgeZ < 0.0;
		if (Out.bScopeShown)
		{
			Out.ScopeFoot = P3(ScopeAt.X, ScopeAt.Y, Height(M.ScopeAlong, M.TowerTop));
			Out.ScopeHead = P3(ScopeAt.X, ScopeAt.Y, Out.ScopeZ);
		}
		// Its feather: only while a periscope alone cuts the sea (the bridge under), by her speed.
		const bool bCuts = Out.ScopeZ > 0.15 && Out.BridgeZ < -0.2;
		const double FeatherMark = bCuts ? Clamp((In.Speed - FeatherFrom) / (FeatherFull - FeatherFrom), 0.0, 1.0) : 0.0;
		S.Feather = bJump ? FeatherMark : Toward(S.Feather, FeatherMark, 1.2 * Dt);
		Out.Feather = S.Feather;
		if (bCuts && S.Feather > 0.15 && In.T >= S.NextFeather)
		{
			Out.Splashes.push_back({ SplashKind::Feather, ScopeAt, Fore, In.T });
			S.NextFeather = In.T + (0.5 - 0.3 * S.Feather);
		}
		// The water off her casing: while her deck is in the band and she is moving through it, from the bow aft as she
		// goes down by the bow (from her trim awash to the band's foot), and bow first as she comes up (back to it).
		if (!bJump && std::fabs(DeckZ) < WashBand && std::fabs(S.SubRate) > 0.04 && In.T >= S.NextWash)
		{
			const double Phase = Clamp((S.SubRate > 0.0 ? DeckAwash - DeckZ : DeckZ + WashBand) / (DeckAwash + WashBand), 0.0, 1.0);
			const double Along = (0.42 - 0.84 * Phase) * M.Long * D.Along;
			const double Hand = Hash01(Stir(static_cast<uint64_t>(In.T * 10.0), 17)) < 0.5 ? -1.0 : 1.0;
			Out.Splashes.push_back({ SplashKind::Wash, Mid + Fore * Along, Side * Hand, In.T });
			S.NextWash = In.T + WashEvery;
		}
		// Her bridge going under (the swirl it leaves), or coming out (the last of the wash): the whole of it as drawn.
		const bool bTowerUnder = Out.BridgeZ < 0.0;
		if (S.bBegun && !bJump && bTowerUnder != S.bTowerUnder)
		{
			Out.Splashes.push_back({ bTowerUnder ? SplashKind::Swirl : SplashKind::Wash, Mid + Fore * (0.5 * (M.TowerFore + M.TowerAft) * D.Along), Fore, In.T });
		}
		S.bTowerUnder = bTowerUnder;
		Out.bTowerUnder = bTowerUnder;
		// Surfaced and fast, her bow dropping into the swell throws spray ahead.
		if (!bJump && Sub < 0.3 && In.Speed > BowFast && SwellRate < -BowPlunge && In.T >= S.NextBow)
		{
			Out.Splashes.push_back({ SplashKind::Bow, Mid + Fore * (M.BowAlong * D.Along), Fore, In.T });
			S.NextBow = In.T + BowEvery;
		}
		// Deep: nothing of her but, where light falls on the sea over her, a faint swirl now and then as she runs.
		S.Swirls.erase(std::remove_if(S.Swirls.begin(), S.Swirls.end(), [&In](const Swirl& W) { return In.T - W.T0 > SwirlLast; }), S.Swirls.end());
		const double SwirlK = In.Dive == "deep" && S.ForcedAt < 0.0 ? Clamp(In.Lit, 0.0, 1.0) * std::exp(-std::max(0.0, In.Keel - In.ScopeKeel) / SwirlFade) : 0.0;
		if (!bJump && SwirlK >= SwirlLeast && In.Speed > FeatherFrom && In.T >= S.NextSwirl)
		{
			S.Swirls.push_back({ Mid, In.T, SwirlK });
			S.NextSwirl = In.T + SwirlEvery;
		}
		S.T = In.T;
		S.bBegun = true;
		return Out;
	}

	// -------------------------------------------------------------------------------------------------------------
	// Depth charges

	namespace
	{
		/** [P] The Mk VII depth charge: 290 lb (132 kg) of amatol, taken as so much TNT. */
		constexpr double PatternKg = 132.0;
		/** [EST] The Hedgehog's bomb: about 30 lb (14 kg) of Torpex, a stronger filling: some 20 kg of TNT's worth. */
		constexpr double AheadKg = 20.0;
		/** [EST] The look's scale, worked from a Mk VII set to 50 ft (15 m): a dome about 8 m high and 15 m across the
		 * middle, its column about 40 m, the column halving for each 15 m deeper (none to speak of past about 50 m); in
		 * the charge's own scaled depth (metres over the cube root of its kg), so a smaller charge is smaller alike. */
		constexpr double ScaledRef = 2.95, DomeK = 1.6, DomeHalving = 5.0, ColumnK = 7.86, DomeHighMost = 12.0, ColumnMost = 60.0;
		/** The least column the kit is asked to throw (metres): under it, the dome alone. */
		constexpr double ColumnFloor = 4.0;
		/** The kit's churn is 3.5 to 6 m high (its recipe's crown_m): a dome under 2.5 m is drawn as a low ring instead;
		 * [EST] one under 0.3 m does not show at all (a very deep burst: its late boil alone). */
		constexpr double ChurnFloor = 2.5, DomeFloor = 0.3;
		/** [EST] At most three churns a burst: each leaves 60 puffs of foam for 40 to 70 s (the recipe), and a pattern is 10
		 * to 14 bursts in the kit's 3,000 puffs. */
		constexpr int ChurnMost = 3;
		/** [EST] A deep charge's gas reaching the sea as a boil: rising about 25 m a second's worth of delay a metre. */
		constexpr double BoilLagPerM = 1.0 / 25.0;
		/** [P] Patterns of 5, then 10, then 14 charges; [SHOW] 10 where the event gives none, 120 m across. */
		constexpr int PatternUsual = 10;
		constexpr double PatternSpread = 120.0;
		/** [OEG] A charge's sinking time from the drop, 20 to 25 s: the middle, where the event gives none. */
		constexpr double SinkingUsual = 22.0;
		/** [P] The Hedgehog's 24 bombs; [FSR] about 200 m ahead; [SHOW] its ring 40 m across where the event gives none. */
		constexpr int AheadUsual = 24;
		constexpr double AheadRing = 40.0;
	}

	double ChargeKg(const std::string& Kind) { return Kind == "ahead" ? AheadKg : PatternKg; }
	double ColumnLeast() { return ColumnFloor; }
	double ChurnLeast() { return ChurnFloor; }
	double DomeLeast() { return DomeFloor; }
	int MostChurns() { return ChurnMost; }

	BurstLook BurstOf(double DepthM, double Kg)
	{
		BurstLook B;
		const double D = Clamp(std::isfinite(DepthM) ? DepthM : 15.0, 3.0, 400.0);
		const double C = std::cbrt(Clamp(std::isfinite(Kg) ? Kg : PatternKg, 1.0, 1000.0));
		const double Scaled = D / C;
		// [EST] The dome shows a little after the burst, later the deeper (the shock lifts the sea at once; the mound of
		// the gas comes up after it); lower and broader the deeper.
		B.DomeAt = 0.25 + D / 60.0;
		B.DomeHigh = std::min(DomeHighMost, DomeK * C * std::pow(2.0, -(Scaled - ScaledRef) / DomeHalving));
		B.DomeWide = 0.5 * D + 1.5 * C;
		// The column, where the gas vents: after the dome, high when shallow, none when deep.
		B.ColumnAt = B.DomeAt + 0.6 + 0.02 * D;
		const double Column = std::min(ColumnMost, ColumnK * C * std::pow(2.0, 1.0 - Scaled / ScaledRef));
		B.ColumnHigh = Column >= ColumnFloor ? Column : 0.0;
		// A deep one throws no column: its gas comes up later as a boil over it.
		B.BoilAt = B.ColumnHigh > 0.0 ? -1.0 : B.DomeAt + 1.0 + D * BoilLagPerM;
		return B;
	}

	std::string ColumnRow(double High)
	{
		if (!(High >= ColumnFloor)) return std::string();
		if (High < 13.0) return "how105";
		if (High < 26.0) return "how150";
		if (High < 46.0) return "bomb250";
		return "bomb500";
	}

	std::vector<KitSpout> SpoutsOf(const BurstLook& B, bool bHit, double Lethal, uint64_t Seed)
	{
		std::vector<KitSpout> Out;
		const double Turn = Hash01(Stir(Seed, 1)) * 2.0 * LookPi;
		if (B.DomeHigh >= ChurnFloor)
		{
			// The dome: the kit's churn (white water that heaves up and sinks back, quiet: no flash, no jolt). A broad one is
			// two or three of them spread over it.
			const int N = static_cast<int>(Clamp(std::round(B.DomeWide / 15.0), 1.0, static_cast<double>(ChurnMost)));
			Out.push_back({ B.DomeAt, 0.0, 0.0, "churn" });
			for (int I = 1; I < N; ++I)
			{
				const double A = Turn + 2.0 * LookPi * (I - 1) / std::max(N - 1, 1);
				const double R = 0.3 * B.DomeWide * (0.85 + 0.3 * Hash01(Stir(Seed, 10 + I)));
				Out.push_back({ B.DomeAt + 0.2 + 0.4 * Hash01(Stir(Seed, 20 + I)), R * std::sin(A), R * std::cos(A), "churn" });
			}
		}
		else if (B.DomeHigh >= DomeFloor)
		{
			// A low dome: the sea lifted in a broad flat ring, its white water thrown outward (the kit's low spurts).
			const int N = static_cast<int>(Clamp(std::round(B.DomeWide / 6.0), 6.0, 10.0));
			for (int I = 0; I < N; ++I)
			{
				const double A = Turn + 2.0 * LookPi * (I + 0.3 * (Hash01(Stir(Seed, 30 + I)) - 0.5)) / N;
				const double R = 0.35 * B.DomeWide * (0.8 + 0.4 * Hash01(Stir(Seed, 50 + I)));
				Out.push_back({ B.DomeAt + 0.3 * Hash01(Stir(Seed, 70 + I)), R * std::sin(A), R * std::cos(A), "skip", std::sin(A), std::cos(A) });
			}
		}
		const std::string Row = ColumnRow(B.ColumnHigh);
		if (!Row.empty())
		{
			Out.push_back({ B.ColumnAt, 0.0, 0.0, Row });
		}
		if (B.BoilAt >= 0.0)
		{
			Out.push_back({ B.BoilAt, 0.0, 0.0, "churn" });
		}
		if (bHit)
		{
			// [EST] Her air: a boil inside the charge's lethal radius, a few seconds on.
			const double L = Lethal > 0.0 ? Lethal : 6.4;
			const double A = Hash01(Stir(Seed, 40)) * 2.0 * LookPi, R = L * Hash01(Stir(Seed, 41));
			const double After = std::max({ B.DomeAt, B.ColumnAt, B.BoilAt });
			Out.push_back({ After + 2.5 + 1.5 * Hash01(Stir(Seed, 42)), R * std::sin(A), R * std::cos(A), "churn" });
		}
		std::stable_sort(Out.begin(), Out.end(), [](const KitSpout& L, const KitSpout& R) { return L.Dt < R.Dt; });
		return Out;
	}

	std::vector<DropShot> DropPlan(const DropIn& In, uint64_t Seed)
	{
		std::vector<DropShot> Out;
		const double Sinking = In.Sinking > 0.5 ? In.Sinking : SinkingUsual;
		const P3 Way = In.Aim - In.At;
		// (her heading from her velocity; stopped, from where the pattern goes in toward its middle)
		const double Heading = HeadingOf(In.Vel, HeadingOf(Way, 0.0));
		const P3 Fore = AheadOf(Heading), Side = RightOf(Heading);
		if (In.Kind == "ahead")
		{
			// The ahead thrower's bombs from her forecastle in a ripple [EST: under 2 s], each landing on the ring round the
			// pattern's middle [SHOW: Aim] about 200 m ahead [FSR]. Their flight [EST] about 5 s, and all in before the
			// sinking time is a third gone, so no burst comes before its splash.
			const int N = In.N > 0 ? std::min(In.N, 48) : AheadUsual;
			const double Ring = (In.Spread > 1.0 ? In.Spread : AheadRing) * 0.5;
			const double Ripple = std::min(1.8, 0.15 * Sinking);
			const double Flight = std::min(5.0, 0.35 * Sinking);
			for (int I = 0; I < N; ++I)
			{
				DropShot S;
				S.Dt = N > 1 ? Ripple * I / (N - 1) : 0.0;
				S.Side = 2;
				S.Flight = Flight;
				S.High = 45.0; // [EST]
				const double A = 2.0 * LookPi * (I + 0.3 * (Hash01(Stir(Seed, 100 + I)) - 0.5)) / N;
				const double R = Ring * (0.92 + 0.16 * Hash01(Stir(Seed, 200 + I)));
				// (fired from where her forecastle is then, as she runs on)
				S.From = In.At + In.Vel * S.Dt;
				S.To = In.Aim + Fore * (R * std::cos(A)) + Side * (R * std::sin(A));
				S.To.Z = 0.0;
				Out.push_back(S);
			}
			return Out;
		}
		// A pattern [SHOW]: its charges go in evenly over Until, alternately off the stern's rails and from the throwers to
		// either side (the even ones off the rails; the odd ones thrown, starboard and port in turn), each landing where
		// the show bursts it: along her track, the rails' on it, the throwers' Spread/2 less half her beam out.
		const int N = In.N > 0 ? std::min(In.N, 40) : PatternUsual;
		const double Spread = In.Spread > 1.0 ? In.Spread : PatternSpread;
		const double Until = std::max(0.0, In.Until);
		const double Reach = std::max(0.0, Spread * 0.5 - In.Beam * 0.5);
		const double ThrowFlight = std::min(2.6, 0.3 * Sinking); // [EST] a thrower's charge some 50 m out in under 3 s
		for (int K = 0; K < N; ++K)
		{
			DropShot S;
			const double Share = N > 1 ? static_cast<double>(K) / (N - 1) : 0.0;
			const bool bRail = K % 2 == 0;
			S.Side = bRail ? 0 : ((K / 2) % 2 == 0 ? 1 : -1);
			S.Dt = Share * Until;
			// (her stern when it goes: where the pattern went in, and on along her way)
			const P3 Stern = In.At + In.Vel * S.Dt;
			const P3 Track = P3(In.At.X, In.At.Y, 0.0) + Fore * (Share * Spread);
			if (bRail)
			{
				// [EST] off the rails at her stern, 2 m over the sea, into her wake just astern
				S.Flight = 0.6;
				S.From = P3(Stern.X, Stern.Y, 2.0);
				S.To = Track - Fore * 1.5;
				S.High = 0.0;
			}
			else
			{
				// [EST] her throwers a fifth of her length forward of the stern, at her side, 3 m up
				S.Flight = ThrowFlight;
				S.From = P3(Stern.X, Stern.Y, 3.0) + Fore * (0.2 * In.Long) + Side * (S.Side * 0.45 * In.Beam);
				S.To = Track + Side * (S.Side * Reach);
				S.High = 10.0;
			}
			S.To.Z = 0.0;
			Out.push_back(S);
		}
		return Out;
	}

	P3 Arc(const P3& From, const P3& To, double High, double Flight, double Age)
	{
		const double S = Flight > 1e-6 ? Clamp(Age / Flight, 0.0, 1.0) : 1.0;
		P3 At = From + (To - From) * S;
		At.Z += 4.0 * std::max(0.0, High) * S * (1.0 - S);
		return At;
	}

	// -------------------------------------------------------------------------------------------------------------
	// Starshell

	namespace
	{
		/** [SHOW] A starshell's flare sinks under its small parachute about 3 m a second; [EST] it burns out at least this
		 * high. */
		constexpr double StarSink = 3.0, StarLeast = 40.0;
		/** [EST] Where the show gives no burn-out time: 75 s, the middle of the 60 to 90 s this lane's brief gives (the
		 * history has no figure). */
		constexpr double StarBurn = 75.0;
		/** [EST] Its parachute's crown this far above the candle; the candle swings under it, this much, this slowly. */
		constexpr double ChuteAbove = 3.0, SwayM = 0.8, SwayS = 3.4;
		/** [EST] It drifts with the wind, a little slower. */
		constexpr double WindShare = 0.9;
		/** [EST] Its light: up in under a second, 40 per cent less by the end as it burns down and sinks, dying in the last
		 * 4 s with a sputter; the dead candle seen 3 s more. */
		constexpr double IgniteS = 0.8, BurnDown = 0.4, DieS = 4.0, DeadS = 3.0;
		/** [EST] Half its light on the sea this far from the flare (metres, straight line); a quarter at twice as far. */
		constexpr double LitHalf = 450.0;
		/** [EST] The sky's light on the sea, as a share of a flare's right under it: day 1; the moon, full and high, 0.25;
		 * none with the sun more than 6 degrees under. */
		constexpr double MoonShare = 0.25, TwilightDeg = 6.0;
	}

	double StarBurnUsual() { return StarBurn; }

	double StarFall(double Height, double Burn)
	{
		const double B = std::max(Burn, 1.0);
		if (Height - StarSink * B >= StarLeast)
		{
			return StarSink;
		}
		return std::max(0.5, (Height - StarLeast) / B);
	}

	StarOut StarAt(const StarIn& In, double T)
	{
		StarOut Out;
		const double Age = T - In.T0;
		const double Burn = std::max(In.Until - In.T0, 1.0);
		if (Age < 0.0 || Age > Burn + DeadS)
		{
			return Out;
		}
		const double Fall = StarFall(In.Burst.Z, Burn);
		const double P1 = Hash01(Stir(In.Seed, 1)) * 2.0 * LookPi, P2 = Hash01(Stir(In.Seed, 2)) * 2.0 * LookPi;
		const double Swing = 2.0 * LookPi * Age / SwayS;
		Out.Flare = P3(In.Burst.X + In.Wind.X * WindShare * Age + SwayM * std::sin(Swing + P1),
		               In.Burst.Y + In.Wind.Y * WindShare * Age + SwayM * std::sin(Swing * 0.83 + P2),
		               In.Burst.Z - Fall * Age);
		if (Out.Flare.Z <= 0.0)
		{
			return StarOut();
		}
		Out.Chute = P3(In.Burst.X + In.Wind.X * WindShare * Age, In.Burst.Y + In.Wind.Y * WindShare * Age, Out.Flare.Z + ChuteAbove);
		Out.bShown = true;
		Out.bBurning = Age <= Burn;
		if (!Out.bBurning)
		{
			return Out;
		}
		const double Ignite = Smooth(0.0, IgniteS, Age);
		const double Down = 1.0 - BurnDown * std::pow(Age / Burn, 1.4);
		const double Die = Smooth(0.0, DieS, Burn - Age);
		// (it flickers, and sputters as it dies: a few slow waves and a quick hash each twelfth of a second)
		const double Sputter = 1.0 + 2.0 * (1.0 - Smooth(0.0, 6.0, Burn - Age));
		const double Jitter = Hash01(Stir(In.Seed, static_cast<uint64_t>(Age * 12.0) + 7)) - 0.5;
		const double Flicker = 1.0 + Sputter * (0.05 * std::sin(2.0 * LookPi * 7.3 * Age + P1) + 0.035 * std::sin(2.0 * LookPi * 11.9 * Age + P2) + 0.05 * Jitter);
		Out.Light = Clamp(Ignite * Down * Die * Flicker, 0.0, 1.0);
		return Out;
	}

	double LitBy(const StarOut& O, const P3& At)
	{
		if (!O.bBurning || O.Light <= 0.0)
		{
			return 0.0;
		}
		const double Far = std::sqrt((O.Flare.X - At.X) * (O.Flare.X - At.X) + (O.Flare.Y - At.Y) * (O.Flare.Y - At.Y) + O.Flare.Z * O.Flare.Z);
		const double Q = Far / LitHalf;
		return O.Light / (1.0 + Q * Q);
	}

	double SkyLit(bool bLight, double SunUp, double MoonUp, double MoonLit)
	{
		if (!bLight)
		{
			return 1.0;
		}
		const double Sun = Smooth(-TwilightDeg, 2.0, SunUp);
		const double Moon = MoonShare * Clamp(MoonLit, 0.0, 1.0) * Smooth(0.0, 30.0, MoonUp);
		return Clamp(std::max(Sun, Moon), 0.0, 1.0);
	}

	// -------------------------------------------------------------------------------------------------------------
	// Ends and rescue

	namespace
	{
		/** [EST] The scene's own seconds for a crushed boat's air to come up (in the battle, a minute or more from 250 m:
		 * the scene's short end cannot wait for it). */
		constexpr double CrushDelay = 3.0;
		/** [EST] A scrambling net: 5 m wide, from 0.7 of her freeboard (a twentieth of her length, 2.5 to 7 m) down into
		 * the sea, its foot 0.6 m out from her side; ropes every 0.6 m across, rungs every 0.5 m. */
		constexpr double NetWide = 5.0, NetMeshAcross = 0.6, NetMeshDown = 0.5, NetFoot = 0.6, NetUnder = 0.4;
	}

	std::vector<BoilShot> CrushPlan(uint64_t Seed)
	{
		std::vector<BoilShot> Out;
		// The hump: three boils at once over a few metres; then her air in boils further and further apart, spreading
		// (churns, each leaving its foam: eight in all, so the hump is a heave and not a column).
		for (int I = 0; I < 3; ++I)
		{
			const double A = Hash01(Stir(Seed, 500 + I)) * 2.0 * LookPi, R = (I == 0 ? 0.0 : 8.0) * Hash01(Stir(Seed, 520 + I));
			Out.push_back({ CrushDelay + 0.6 * Hash01(Stir(Seed, 540 + I)), R * std::sin(A), R * std::cos(A) });
		}
		for (int K = 1; K <= 5; ++K)
		{
			const double A = Hash01(Stir(Seed, 560 + K)) * 2.0 * LookPi, R = 20.0 * std::sqrt(Hash01(Stir(Seed, 580 + K)));
			Out.push_back({ CrushDelay + 2.0 + 24.0 * std::pow(K / 5.0, 1.6), R * std::sin(A), R * std::cos(A) });
		}
		return Out;
	}

	std::vector<Rope> NetOf(const NetIn& In)
	{
		std::vector<Rope> Out;
		const P3 Fore = AheadOf(In.Heading), Right = RightOf(In.Heading);
		const P3 ToMen = In.Aim - In.Ship;
		const double Hand = ToMen.X * Right.X + ToMen.Y * Right.Y >= 0.0 ? 1.0 : -1.0;
		const P3 Side = Right * Hand;
		const double Freeboard = Clamp(In.Long / 20.0, 2.5, 7.0) * 0.7;
		// (amidships, a little abaft: where a ship's waist is lowest)
		const P3 Mid = P3(In.Ship.X, In.Ship.Y, 0.0) - Fore * (0.05 * In.Long) + Side * (0.5 * In.Beam);
		const int Across = static_cast<int>(std::round(NetWide / NetMeshAcross));
		const int Down = static_cast<int>(std::round((Freeboard + NetUnder) / NetMeshDown));
		auto At = [&](double U, double V)
		{
			// U across (-0.5 to 0.5), V down (0 at the rail to 1 at its foot in the sea), hanging out a little at its foot
			return Mid + Fore * (U * NetWide) + Side * (NetFoot * V * V) + P3(0.0, 0.0, Freeboard - V * (Freeboard + NetUnder));
		};
		for (int I = 0; I <= Across; ++I)
		{
			const double U = static_cast<double>(I) / Across - 0.5;
			for (int J = 0; J < Down; ++J)
			{
				Out.push_back({ At(U, static_cast<double>(J) / Down), At(U, static_cast<double>(J + 1) / Down) });
			}
		}
		for (int J = 0; J <= Down; ++J)
		{
			const double V = static_cast<double>(J) / Down;
			Out.push_back({ At(-0.5, V), At(0.5, V) });
		}
		return Out;
	}
}
