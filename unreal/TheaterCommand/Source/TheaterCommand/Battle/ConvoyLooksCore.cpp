#include "Battle/ConvoyLooksCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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
		/** [EST] Her tanks blown, she comes up from awash to riding high (her model's own waterline at the sea) in 3 s, and
		 * flooded again settles back in as long. */
		constexpr double BlowSeconds = 3.0;
		/** [EST] Deep, a faint swirl over her where light falls: one every SwirlEvery s while she runs, lasting SwirlLast
		 * s, from SwirlFrom to SwirlTo metres across; fainter the deeper (by e each SwirlFade metres under periscope
		 * depth), none under SwirlLeast of light. */
		constexpr double SwirlEvery = 2.5, SwirlLast = 9.0, SwirlFrom = 3.0, SwirlTo = 13.0, SwirlFade = 50.0, SwirlLeast = 0.03;
		/** [EST] Scuttled after she struck: her vents' air and spray along her sides, a spurt each VentEvery s, side about,
		 * for VentSeconds as she floods (a boat flooded through her vents in well under a minute). */
		constexpr double VentSeconds = 25.0, VentEvery = 1.5;
		/** [EST] Her main vents on top of her saddle tanks, this far under her casing amidships and out at 0.45 of her beam;
		 * each spurt a white jet VentMouth across, blowing VentBlows s (the next comes VentEvery s on, from the other side). */
		constexpr double VentUnder = 1.0, VentMouth = 0.4, VentBlows = 2.0;
		/** [EST] Her scuttling charges: the after one 7 s after her vents were opened, the fore one 2.5 s later (her crew
		 * all over her side by then, SeaShow's HurryFor and a second), small charges set below to hole her: a muffled heave
		 * of white water ChargeOut m out from her side, and smoke from the hatch over it, ChargeMouth across. */
		constexpr double ChargeAfter[2] = { 7.0, 9.5 }, ChargeOut = 0.8, ChargeMouth = 0.6, ChargePop = 0.4;
		/** [EST] Her white flag: a 2 m staff (a boat-hook stepped at her ensign's place: long enough that the sheet hanging in
		 * a calm stays clear of her bridge), a sheet 1.1 m in the fly and 0.7 in the hoist (a bedsheet or a tablecloth, what a
		 * boat had), run up in 3 s, hauled down in 4; it streams out fully from a breeze of FlagStreams m/s, from FlagStill up. */
		constexpr double StaffLong = 2.0, SheetFly = 1.1, SheetHoist = 0.7, RaiseSeconds = 3.0, LowerSeconds = 4.0, FlagStill = 0.5, FlagStreams = 4.5;
		/** [EST] Never lower than this over the staff's foot, the sheet hanging in a calm. */
		constexpr double FlagClear = 0.15;

		double Dot(const P3& A, const P3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		P3 Cross(const P3& A, const P3& B) { return P3(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X); }
		/** A way of length one; Else where it has none. */
		P3 Unit(const P3& Way, const P3& Else)
		{
			const double L = std::sqrt(Dot(Way, Way));
			return L > 1e-9 ? Way * (1.0 / L) : Else;
		}
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
		// (no ensign staff on it: her white flag is stepped at her bridge's after end; her casing its flat deck)
		M.EnsignAlong = M.TowerAft;
		M.EnsignTop = M.TowerTop + 0.2;
		M.Casing = Deck;
		// (no hatches on it: where the modeller's boats have theirs, by her length)
		M.ForeHatch = 0.12 * Hull;
		M.AftHatch = -0.18 * Hull;
		return M;
	}

	BoatModel SculptedOf(const std::string& File)
	{
		// (Metres in the file's frame: along her forward from her origin amidships, up from her waterline. Tools/convoy_looks
		// reads the files' sockets and fails if these no longer agree with them.)
		BoatModel M;
		if (File == "Submarine_GER")
		{
			// A Type VIIC: casing 1.77 m amidships; the bridge's rim 5.3 m, from 3.5 forward to the after gun platform's 6.5
			// aft; the periscope standard's top (and the jumping wire's head) 5.94 m at amidships; the 20 mm gun's muzzle
			// (aa.1) 5.95 aft at 4.64 m; the attack periscope raised to 10.49 m, 0.2 aft, the sky periscope housed at 5.79 m,
			// 1.2 aft; the stem's waterline (wake.bow) 31.3 m forward; bounds 67.0 by 6.79 (her bow planes).
			M.Long = 67.0; M.Wide = 6.79; M.Deck = 1.77;
			M.TowerTop = 5.3; M.TowerFore = 3.5; M.TowerAft = -6.5;
			M.WireAlong = 0.0; M.WireTop = 5.94; M.GunAlong = -5.95; M.GunTop = 4.74;
			M.ScopeAlong = -0.2; M.ScopeTop = 10.49; M.LowAlong = -1.2; M.LowTop = 5.79;
			M.BowAlong = 31.3;
			// (her ensign's staff on her bridge's after rim, 3.4 aft at 5.62 m; her casing fore and aft of her tower, its
			// height fitted to the file where her men stand; her 88's mount and barrel stowed forward, 10.85 to 15.55)
			M.EnsignAlong = -3.4; M.EnsignTop = 5.62;
			M.Casing = 1.58; M.RiseFrom = 3.0; M.Rise = 0.0087; M.Fall = 0.051; M.Edge = 1.2;
			M.ForeFrom = 4.5; M.ForeTo = 10.0; M.AftFrom = -20.0; M.AftTo = -7.5; M.GunFrom = 10.85; M.GunTo = 15.55;
			M.ForeHatch = 8.0; M.AftHatch = -12.2;
		}
		else if (File == "Submarine_USA")
		{
			// A fleet boat: casing 2.6 m; the bridge's rim 6.5 m, from 6.5 forward to 5.0 aft; her periscope shears' top
			// (the jumping wire's head on it) 9.04 m, 2.4 forward; the forward 20 mm (aa.1) 8.65 forward at 5.71 m; the
			// attack periscope raised to 11.79 m, 2.4 forward, the other housed in the shears at 7.09 m, 1.4 forward; the
			// stem 44.5 m forward; bounds 95.0 by 7.97.
			M.Long = 95.0; M.Wide = 7.97; M.Deck = 2.6;
			M.TowerTop = 6.5; M.TowerFore = 6.5; M.TowerAft = -5.0;
			M.WireAlong = 2.4; M.WireTop = 9.04; M.GunAlong = 8.65; M.GunTop = 5.81;
			M.ScopeAlong = 2.4; M.ScopeTop = 11.79; M.LowAlong = 1.4; M.LowTop = 7.09;
			M.BowAlong = 44.5;
			// (her ensign's staff 5.2 aft at 6.92 m; her deck gun aft of her tower, its barrel stowed aft, -17.05 to -12.35)
			M.EnsignAlong = -5.2; M.EnsignTop = 6.92;
			M.Casing = 2.54; M.RiseFrom = 9.0; M.Rise = 0.0202; M.Fall = 0.031; M.Edge = 1.9;
			M.ForeFrom = 10.0; M.ForeTo = 21.0; M.AftFrom = -24.0; M.AftTo = -6.0; M.GunFrom = -17.05; M.GunTo = -12.35;
			M.ForeHatch = 13.5; M.AftHatch = -12.0;
		}
		else if (File == "Submarine")
		{
			// Any other navy's: casing 2.0 m; the bridge's rim 5.6 m, from 4.5 forward to 7.5 aft; the standard's top 6.44 m,
			// 0.4 forward; aa.1 7.25 aft at 5.02 m; the attack periscope raised to 10.99 m, 0.4 forward, the other housed at
			// 6.29 m, 0.6 aft; the stem 37.5 m forward; bounds 80.0 by 7.08.
			M.Long = 80.0; M.Wide = 7.08; M.Deck = 2.0;
			M.TowerTop = 5.6; M.TowerFore = 4.5; M.TowerAft = -7.5;
			M.WireAlong = 0.4; M.WireTop = 6.44; M.GunAlong = -7.25; M.GunTop = 5.12;
			M.ScopeAlong = 0.4; M.ScopeTop = 10.99; M.LowAlong = -0.6; M.LowTop = 6.29;
			M.BowAlong = 37.5;
			// (her ensign's staff 4.7 aft at 6.12 m; her deck gun forward, 13.35 to 18.05)
			M.EnsignAlong = -4.7; M.EnsignTop = 6.12;
			M.Casing = 1.90; M.RiseFrom = 5.0; M.Rise = 0.0122; M.Fall = 0.042; M.Edge = 1.5;
			M.ForeFrom = 5.5; M.ForeTo = 12.5; M.AftFrom = -24.0; M.AftTo = -8.5; M.GunFrom = 13.35; M.GunTo = 18.05;
			M.ForeHatch = 10.0; M.AftHatch = -13.5;
		}
		else
		{
			return M;
		}
		M.Name = "Ships/" + File;
		M.Top = M.ScopeTop;
		return M;
	}

	double CasingAt(const BoatModel& M, double Along, double Across)
	{
		return M.Casing + M.Rise * std::max(0.0, Along - M.RiseFrom) - M.Fall * Across * Across;
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

	void Blow(BoatState& S, bool bOn) { S.bBlown = bOn; }

	double BlowFor() { return BlowSeconds; }
	double SwirlLife() { return SwirlLast; }

	double VentFor() { return VentSeconds; }

	void OpenVents(BoatState& S, double T)
	{
		if (S.VentsAt >= 0.0)
		{
			return;
		}
		S.VentsAt = T;
		S.NextVent = T;
	}

	double ScuttleChargeAt(int K) { return K >= 0 && K < ScuttleCharges ? ChargeAfter[K] : -1.0; }

	void StageSinking(double T, double High, bool bByStern, double& OutDepth, double& OutPitch)
	{
		// (the stage's own: Under = High + 40, Depth = Under * EaseIn(T / 70), Pitch = 14 * Down * Ease(T / 35), its Down -1
		// for "stern", where a plus pitch lifts her stern; here plus is her bow up, so by the stern is plus)
		const double In = Clamp(T / 70.0, 0.0, 1.0), Over = Clamp(T / 35.0, 0.0, 1.0);
		OutDepth = (High + 40.0) * In * In;
		OutPitch = bByStern ? 14.0 * Over * Over * (3.0 - 2.0 * Over) : 0.0;
	}

	SplashKit KitOf(const BoatSplash& Sp)
	{
		SplashKit K;
		K.Where = Sp.At;
		K.Way = Unit(Sp.Way, P3(0.0, 0.0, 1.0));
		switch (Sp.Kind)
		{
		case SplashKind::Feather:
		case SplashKind::Bow:
			K.Call = KitCall::Spurt;
			K.Way = Unit(P3(Sp.Way.X, Sp.Way.Y, 0.0), P3(1.0, 0.0, 0.0));
			break;
		case SplashKind::Vent:
		case SplashKind::Puff:
			// (out of the sea: her vent's white jet straight up off her saddle tank, or the charge's smoke out of the hatch; under
			// it already, the air or the smoke comes up as white water on the sea over it)
			if (Sp.At.Z > 0.1)
			{
				K.Call = Sp.Kind == SplashKind::Vent ? KitCall::Jet : KitCall::Puff;
				K.Across = Sp.Kind == SplashKind::Vent ? VentMouth : ChargeMouth;
				K.Seconds = Sp.Kind == SplashKind::Vent ? VentBlows : ChargePop;
				break;
			}
			K.Call = KitCall::Spout;
			K.Where = P3(Sp.At.X, Sp.At.Y, 0.0);
			K.Row = "churn";
			break;
		case SplashKind::Wash:
		case SplashKind::Swirl:
		case SplashKind::Scuttle:
			// (the water off her casing, the swirl where her bridge went under, a charge's muffled heave: the kit's churn, its foam
			// lasting, no flash)
			K.Call = KitCall::Spout;
			K.Where = P3(Sp.At.X, Sp.At.Y, 0.0);
			K.Row = "churn";
			break;
		}
		return K;
	}

	double FlagStaff() { return StaffLong; }
	double FlagFly() { return SheetFly; }
	double FlagHoist() { return SheetHoist; }
	double FlagRaiseFor() { return RaiseSeconds; }
	double FlagLowerFor() { return LowerSeconds; }

	P3 UpOf(double HeadingDeg, double PitchDeg)
	{
		const double Trim = (std::isfinite(PitchDeg) ? Clamp(PitchDeg, -30.0, 30.0) : 0.0) * LookPi / 180.0;
		return AheadOf(HeadingDeg) * (-std::sin(Trim)) + P3(0.0, 0.0, std::cos(Trim));
	}

	P3 EnsignAt(const BoatModel& M, const Drawn& D, const BoatIn& In, const BoatOut& Out)
	{
		// (her model as the stage draws it: stretched, lifted by her heave and turned by her trim about her waterline
		// amidships, bow up: what is forward rises, what is up leans aft)
		const double Trim = (std::isfinite(In.Pitch) ? Clamp(In.Pitch, -30.0, 30.0) : 0.0) * LookPi / 180.0;
		const double Along = M.EnsignAlong * D.Along, Up = M.EnsignTop * D.Up;
		const P3 Level = P3(In.At.X, In.At.Y, 0.0) + AheadOf(In.Heading) * (Along * std::cos(Trim) - Up * std::sin(Trim));
		return P3(Level.X, Level.Y, Out.Heave + Up * std::cos(Trim) + Along * std::sin(Trim));
	}

	FlagOut FlagAt(const FlagIn& In, double Now)
	{
		FlagOut F;
		if (In.Since < 0.0)
		{
			return F;
		}
		const double Raised = Clamp(In.Since / RaiseSeconds, 0.0, 1.0);
		const double Lowered = In.Lowered >= 0.0 ? Clamp(In.Lowered / LowerSeconds, 0.0, 1.0) : 0.0;
		const double Hoisted = Raised * (1.0 - Lowered);
		if (Hoisted <= 0.0)
		{
			return F;
		}
		F.bShown = true;
		const P3 Up = Unit(In.Up, P3(0.0, 0.0, 1.0));
		F.Head = In.Foot + Up * StaffLong;
		// (the air over her, level: the way the sheet streams, and how much it streams, by the breeze)
		const P3 Level(In.Air.X, In.Air.Y, 0.0);
		const double Breeze = std::sqrt(Dot(Level, Level));
		const double Stream = Clamp((Breeze - FlagStill) / (FlagStreams - FlagStill), 0.0, 1.0);
		const P3 Lee = Unit(Level, P3(1.0, 0.0, 0.0));
		// (out to leeward, drooping as it falls light; in a calm, down beside the staff, a touch to leeward)
		const P3 Fly = Unit(Lee * (0.1 + Stream) - Up * (1.0 - Stream), Lee);
		// (its ripple across the way it streams, level)
		const P3 Flap = Unit(Cross(Up, Lee), P3(0.0, 1.0, 0.0));
		const double Hz = std::min(3.0, 0.6 + 0.15 * Breeze), Phase = Hash01(In.Seed) * 2.0 * LookPi;
		// (its head runs up the staff as it is hoisted, and down it as it is hauled down)
		const P3 Luff = F.Head - Up * ((1.0 - Hoisted) * (StaffLong - SheetHoist));
		for (int J = 0; J < FlagDown; ++J)
		{
			for (int I = 0; I < FlagAlong; ++I)
			{
				const double U = static_cast<double>(I) / (FlagAlong - 1), V = static_cast<double>(J) / (FlagDown - 1);
				const double Wave = 0.12 * U * (0.3 + 0.7 * Stream) * std::sin(2.0 * LookPi * (Hz * Now - 1.2 * U) + Phase);
				P3 P = Luff - Up * (V * SheetHoist) + Fly * (U * SheetFly) + Flap * Wave;
				// (never under the staff's foot: hanging in a calm, or as it comes down, it lies on her bridge's rail)
				const double Over = Dot(P - In.Foot, Up);
				if (Over < FlagClear)
				{
					P = P + Up * (FlagClear - Over);
				}
				F.Point[J][I] = P;
			}
		}
		return F;
	}

	bool FlagInputsOf(const BoatModel& M, const Drawn& D, const BoatIn& In, const BoatOut& Out, const P3& Wind, double Struck, double Lowered, int Key, FlagIn& OutFlag)
	{
		if (Struck < 0.0 || !In.bShown)
		{
			return false;
		}
		OutFlag.Foot = EnsignAt(M, D, In, Out);
		OutFlag.Up = UpOf(In.Heading, In.Pitch);
		// (the air over her: the wind, less her own way through it)
		const P3 Way = AheadOf(In.Heading) * In.Speed;
		OutFlag.Air = P3(Wind.X - Way.X, Wind.Y - Way.Y, 0.0);
		OutFlag.Since = Struck;
		OutFlag.Lowered = Lowered;
		OutFlag.Seed = static_cast<uint64_t>(static_cast<uint32_t>(Key)) * 2654435761ull + 29ull;
		return true;
	}

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
		// (her tanks blown, she rides high: none of her trim awash)
		const double Blown = bJump ? (S.bBlown ? 1.0 : 0.0) : Toward(S.Blown, S.bBlown ? 1.0 : 0.0, Dt / BlowSeconds);
		const double Awash = std::max(0.0, DeckDrawn - DeckAwash) * (1.0 - Going) * (1.0 - Smooth(0.0, 1.0, Blown));
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
		S.Blown = Blown;
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
		Out.DeckZ = DeckZ;
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
		// Scuttled after she struck (OpenVents): the stage takes her down by the stern from then (its sinking on top of how the
		// looks ride her), and what comes out of her comes from her as she goes.
		double StageDown = 0.0, StageTrim = 0.0;
		if (S.VentsAt >= 0.0) StageSinking(In.T - S.VentsAt, TopDrawn, true, StageDown, StageTrim);
		const double SunkTrim = Trim + StageTrim * LookPi / 180.0;
		auto Sunk = [&](double Along, double Up) { return S.Heave - StageDown + Up * D.Up * std::cos(SunkTrim) + Along * D.Along * std::sin(SunkTrim); };
		// Her main vents open, the air roaring out of her tanks with spray, a spurt each VentEvery s, side about, along her
		// saddle tanks from forward aft as she floods, for VentSeconds: straight up, a little outboard.
		if (!bJump && S.VentsAt >= 0.0 && In.T - S.VentsAt < VentSeconds && In.T >= S.NextVent)
		{
			const double Hand = S.Vents % 2 ? -1.0 : 1.0;
			const double VentAlong = (0.3 - 0.6 * Clamp((In.T - S.VentsAt) / VentSeconds, 0.0, 1.0)) * M.Long;
			P3 Vent = Mid + Fore * (VentAlong * D.Along) + Side * (Hand * 0.45 * M.Wide * D.Up);
			Vent.Z = Sunk(VentAlong, M.Casing - VentUnder);
			Out.Splashes.push_back({ SplashKind::Vent, Vent, Unit(Side * (0.2 * Hand) + P3(0.0, 0.0, 1.0), P3(0.0, 0.0, 1.0)), In.T });
			S.NextVent = In.T + VentEvery;
			++S.Vents;
		}
		// Her scuttling charges, the after one first, as she goes: each a muffled heave of white water alongside abreast its
		// hatch and its smoke out of the hatch, her up tipped by her trim (one a picture run skipped past is gone with it).
		while (S.VentsAt >= 0.0 && S.Charges < ScuttleCharges && In.T - S.VentsAt >= ChargeAfter[S.Charges])
		{
			if (!bJump && In.T - S.VentsAt - ChargeAfter[S.Charges] < 1.0)
			{
				const double HatchAlong = S.Charges == 0 ? M.AftHatch : M.ForeHatch;
				const double Hand = Hash01(Stir(static_cast<uint64_t>(S.VentsAt * 10.0), 41u + static_cast<uint64_t>(S.Charges))) < 0.5 ? -1.0 : 1.0;
				const P3 Abreast = Mid + Fore * (HatchAlong * D.Along);
				Out.Splashes.push_back({ SplashKind::Scuttle, Abreast + Side * (Hand * (0.5 * M.Wide * D.Up + ChargeOut)), Side * Hand, In.T });
				P3 Hatch = Abreast;
				Hatch.Z = Sunk(HatchAlong, CasingAt(M, HatchAlong, 0.0));
				Out.Splashes.push_back({ SplashKind::Puff, Hatch, Fore * (-std::sin(SunkTrim)) + P3(0.0, 0.0, std::cos(SunkTrim)), In.T });
			}
			++S.Charges;
		}
		// Deep: nothing of her but, where light falls on the sea over her, a faint swirl now and then as she runs.
		S.Swirls.erase(std::remove_if(S.Swirls.begin(), S.Swirls.end(), [&In](const Swirl& W) { return In.T - W.T0 > SwirlLast; }), S.Swirls.end());
		const double SwirlK = In.Dive == "deep" && S.ForcedAt < 0.0 ? Clamp(In.Lit, 0.0, 1.0) * std::exp(-std::max(0.0, In.Keel - In.ScopeKeel) / SwirlFade) : 0.0;
		if (!bJump && SwirlK >= SwirlLeast && In.Speed > FeatherFrom && In.T >= S.NextSwirl)
		{
			// (out of the player's sight none is laid; those already on the sea fade as their own, a sudden going a tell too)
			if (In.bShown) S.Swirls.push_back({ Mid, In.T, SwirlK });
			S.NextSwirl = In.T + SwirlEvery;
		}
		// Out of the player's sight (an enemy boat not yet found): nothing of her thrown on the sea, her periscope's feather,
		// the water off her casing, her bow's spray, though her clocks run on as in sight.
		if (!In.bShown) Out.Splashes.clear();
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
			// (her stern when it goes: where the pattern went in, and on along her way; a pattern laid all at once, as the
			// battle lays one on the spot it tells, from over each charge's own place along it, so no rail charge is drawn
			// flung the pattern's length from one stern)
			const P3 Track = P3(In.At.X, In.At.Y, 0.0) + Fore * (Share * Spread);
			const P3 Stern = Until > 0.0 ? In.At + In.Vel * S.Dt : P3(Track.X, Track.Y, In.At.Z);
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
		/** [EST] A scrambling net: 5 m wide, from her rail down into the sea (0.4 m under), hung 0.1 m off her side and its
		 * foot 0.2 m further out; ropes every 0.6 m across, rungs every 0.5 m. Nets hang at stations 5 m apart from
		 * amidships, side by side; a man climbing takes 0.3 m of it each side of his middle. It goes down her side, or
		 * comes in, in 2 s. */
		constexpr double NetWide = 5.0, NetMeshAcross = 0.6, NetMeshDown = 0.5, NetOff = 0.1, NetFoot = 0.2, NetUnder = 0.4, NetMargin = 0.3, NetDrop = 2.0;
		/** [SHOW] The sea scene's rescue (SeaShow.cpp RunWrecks): a rescuer stands by under 3 m/s (and its men give her up
		 * otherwise); a man swims for her side 1 m out from it, at the nearest place along it within 0.35 of her length of
		 * amidships, and climbs it 0.3 m out from it. */
		constexpr double StandByBelow = 3.0, SwimOut = 1.0, ClimbAlong = 0.35, ClimbOut = 0.3;
		/** [SHOW] One man up a net at a time: come to his place with another climbing within a metre of it, the scene moves
		 * him 1.2 m along her side to the next (a struck boat's crew, picked up in a crowd, do). */
		constexpr double ClimbShift = 1.2;
		/** [EST] A man swimming for her side gets his net once he is within 60 m of it (a minute or two before he is there);
		 * within 3 m of it (two seconds' swim or so, about a net's time going down), at his place along her now too. */
		constexpr double NetSee = 60.0, NetNear = 3.0;
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

	double BoatFreeOf(const std::string& Nation)
	{
		return Nation == "USA" ? 2.6 : Nation == "GER" ? 1.8 : 2.0;
	}

	double FreeOf(const std::string& Class)
	{
		// [SHOW] SeaShow.cpp CompanyOf's deck heights (Free): the scene's men climb a rescuer's side to these
		if (Class == "bb") return 8.0;
		if (Class == "cv") return 12.0;
		if (Class == "ca") return 7.0;
		if (Class == "cl") return 6.5;
		if (Class == "de") return 4.5;
		if (Class == "ss") return 2.5;
		if (Class == "mv") return 6.0;
		return 5.0; // a destroyer
	}

	bool StandsBy(double Speed) { return Speed < StandByBelow; }

	double NetDropFor() { return NetDrop; }

	bool ClimbPlace(const RescuerIn& R, const ManIn& M, double& OutAlong, double& OutSide, bool& bOutClimbing)
	{
		bOutClimbing = false;
		const P3 Fore = AheadOf(R.Heading), Right = RightOf(R.Heading);
		const double RX = M.At.X - R.At.X, RY = M.At.Y - R.At.Y;
		const double U = RX * Fore.X + RY * Fore.Y, V = RX * Right.X + RY * Right.Y;
		const double Side = V >= 0.0 ? 1.0 : -1.0;
		const double Half = 0.5 * R.Beam, Reach = ClimbAlong * R.Long + 0.5;
		if (M.Doing == ManDoing::Climb)
		{
			// (up her side: 0.3 m out from it, within her waist; on his first moment still where he swam to, 1 m out)
			const double Out = std::fabs(V) - Half;
			if (std::fabs(U) > Reach || Out < ClimbOut - 0.4 || Out > SwimOut + 0.3) return false;
			OutAlong = U;
			OutSide = Side;
			bOutClimbing = true;
			return true;
		}
		if (M.Doing != ManDoing::Swim) return false;
		// (swimming for her side: the scene points him at his place on it, 1 m out from her side, so where his way meets
		// that line is where he will climb)
		const double Off = std::fabs(V) - (Half + SwimOut);
		double Along = U;
		if (Off > 0.05)
		{
			const double WX = std::sin(M.Heading), WY = std::cos(M.Heading);
			const double In = -Side * (WX * Right.X + WY * Right.Y);
			if (In < 0.05) return false;
			const double Run = Off / In;
			if (Run > NetSee) return false;
			Along = U + Run * (WX * Fore.X + WY * Fore.Y);
		}
		if (std::fabs(Along) > Reach) return false;
		OutAlong = Along;
		OutSide = Side;
		return true;
	}

	bool OnNets(const std::vector<NetHung>& Nets, double Side, double Along, double Least)
	{
		// (each end of his hold on her side within a net down so far: the nets are side by side, so all of it is)
		for (const double At : { Along - NetMargin, Along, Along + NetMargin })
		{
			bool bIn = false;
			for (const NetHung& N : Nets) bIn = bIn || (N.Side == Side && N.Out >= Least && std::fabs(At - N.Along) <= 0.5 * NetWide + 1e-9);
			if (!bIn) return false;
		}
		return true;
	}

	void StepNets(std::vector<NetHung>& Nets, const RescuerIn& R, const std::vector<ManIn>& Men, bool bRescue, double Dt)
	{
		const bool bStanding = bRescue && StandsBy(R.Speed);
		std::vector<char> Wanted(Nets.size(), bStanding ? 1 : 0);
		// (the station a place on her side is at, and the next one too where it is at its end: within his hold of it, or for one
		// still swimming to it, within the scene's move along her side to a free place, so it is down before he gets there)
		auto Want = [&Nets, &Wanted](double Place, double OnSide, double Ends)
		{
			const double Station = std::round(Place / NetWide), Off = Place - Station * NetWide;
			for (const double K : { Station, std::fabs(Off) > 0.5 * NetWide - Ends ? Station + (Off > 0.0 ? 1.0 : -1.0) : Station })
			{
				bool bHung = false;
				for (size_t I = 0; I < Nets.size(); ++I)
				{
					if (Nets[I].Side == OnSide && std::fabs(Nets[I].Along - K * NetWide) < 1e-6)
					{
						Wanted[I] = 1;
						bHung = true;
					}
				}
				if (!bHung)
				{
					NetHung N;
					N.Side = OnSide;
					N.Along = K * NetWide;
					Nets.push_back(N);
					Wanted.push_back(1);
				}
			}
		};
		const P3 Fore = AheadOf(R.Heading), Right = RightOf(R.Heading);
		for (const ManIn& M : Men)
		{
			double Along = 0.0, Side = 1.0;
			bool bClimbing = false;
			if (!ClimbPlace(R, M, Along, Side, bClimbing) || (!bStanding && !bClimbing)) continue;
			Want(Along, Side, bClimbing ? NetMargin : NetMargin + ClimbShift);
			if (bClimbing) continue;
			// (one close to her side: she may still have way on her, standing by under 3 m/s, and the scene has him make for a
			// place on her side as she goes, so he comes to it about where he is along her now, not where his heading through
			// the water meets her side; a man brought to her side holding on to a float swims only those last metres. His net
			// there too, so that it is down before he is up it)
			const double RX = M.At.X - R.At.X, RY = M.At.Y - R.At.Y;
			if (std::fabs(RX * Right.X + RY * Right.Y) - 0.5 * R.Beam - SwimOut <= NetNear)
			{
				Want(std::clamp(RX * Fore.X + RY * Fore.Y, -ClimbAlong * R.Long, ClimbAlong * R.Long), Side, NetMargin + ClimbShift);
			}
		}
		size_t Kept = 0;
		for (size_t K = 0; K < Nets.size(); ++K)
		{
			NetHung N = Nets[K];
			N.Out = Toward(N.Out, Wanted[K] ? 1.0 : 0.0, std::max(0.0, Dt) / NetDrop);
			if (N.Out > 0.0 || Wanted[K]) Nets[Kept++] = N;
		}
		Nets.resize(Kept);
	}

	std::vector<Rope> NetOf(const NetIn& In)
	{
		std::vector<Rope> Out;
		const double Free = std::max(1.0, In.Free), Full = Free + NetUnder;
		const double Hung = Clamp(In.Out, 0.0, 1.0) * Full;
		if (Hung < 0.05) return Out;
		const P3 Fore = AheadOf(In.Heading), Side = RightOf(In.Heading) * (In.Side >= 0.0 ? 1.0 : -1.0);
		// (from her rail at her deck's height, close against her side where the scene's men climb it)
		const P3 Mid = P3(In.Ship.X, In.Ship.Y, 0.0) + Fore * In.Along + Side * (0.5 * In.Beam + NetOff);
		const int Across = static_cast<int>(std::round(NetWide / NetMeshAcross));
		const int Down = std::max(1, static_cast<int>(std::round(Hung / NetMeshDown)));
		auto At = [&](double U, double V)
		{
			// U across (-0.5 to 0.5), V down what is hung (0 at the rail, 1 at its foot), standing out a little at its foot
			const double Low = V * Hung / Full;
			return Mid + Fore * (U * NetWide) + Side * (NetFoot * Low * Low) + P3(0.0, 0.0, Free - V * Hung);
		};
		// (its ropes down in three lengths, enough for the little it stands out at its foot; its rungs across)
		constexpr int Lengths = 3;
		for (int I = 0; I <= Across; ++I)
		{
			const double U = static_cast<double>(I) / Across - 0.5;
			for (int J = 0; J < Lengths; ++J)
			{
				Out.push_back({ At(U, static_cast<double>(J) / Lengths), At(U, static_cast<double>(J + 1) / Lengths) });
			}
		}
		for (int J = 0; J <= Down; ++J)
		{
			const double V = static_cast<double>(J) / Down;
			Out.push_back({ At(-0.5, V), At(0.5, V) });
		}
		return Out;
	}

	// -------------------------------------------------------------------------------------------------------------
	// A gun's crew

	GunMount MountOf(const std::string& File, bool bStern)
	{
		// Measured off the battle modeller's files (Content/Models/Ships; x forward, z up from her waterline): the turret
		// part's pivot along her; the deck where most of the walking surface about the mount lies (within 3 m of it, below
		// its pivot); how far that deck reaches from her middle line within 2 m along of the mount (a boat's casing) or from
		// the mount (a merchant's round gun platform), less a man's half width and a hand; a merchant's shield's back from
		// the mount's middle (its turret part's), and a hand; its muzzle's bearing from its barrel's pivot (stowed); and its
		// limits. Tools/convoy_looks checks each against its file.
		struct Row
		{
			const char* Name;
			bool bAft;
			GunMount M;
		};
		static const Row Rows[] = {
			{ "Submarine_GER", false, { "Submarine_GER", "turret.A", 11.5, 1.64, 2.6, 0.95, 0.0, 0.0, 145.0 } },
			{ "Submarine_USA", false, { "Submarine_USA", "turret.A", -13.0, 2.49, 2.6, 1.95, 0.0, 180.0, 145.0 } },
			{ "Submarine", false, { "Submarine", "turret.A", 14.0, 1.96, 2.6, 1.2, 0.0, 0.0, 145.0 } },
			{ "Freighter", false, { "Freighter", "turret.A", 53.6, 7.98, 1.8, 1.8, 1.0, 0.0, 150.0 } },
			{ "Freighter", true, { "Freighter", "turret.Y", -61.6, 8.14, 2.15, 2.15, 1.25, 180.0, 150.0 } },
			{ "Tanker", false, { "Tanker", "turret.A", 65.6, 7.53, 1.8, 1.8, 1.0, 0.0, 150.0 } },
			{ "Tanker", true, { "Tanker", "turret.Y", -69.4, 7.53, 2.25, 2.25, 1.25, 180.0, 150.0 } },
			{ "Troopship", false, { "Troopship", "turret.A", 81.6, 12.75, 1.8, 1.8, 1.0, 0.0, 150.0 } },
			{ "Troopship", true, { "Troopship", "turret.Y", -90.6, 11.88, 2.25, 2.25, 1.25, 180.0, 150.0 } },
		};
		for (const Row& R : Rows)
		{
			if (File == R.Name && bStern == R.bAft) return R.M;
		}
		return GunMount();
	}

	std::vector<CrewMan> GunCrewOf(const GunMount& M, const CrewIn& In)
	{
		std::vector<CrewMan> Out;
		// (none at a gun not laid, nor on a boat told down or under: at the diving alarm her gun's crew go below first)
		if (M.File.empty() || !In.bLaid || !(In.Dive.empty() || In.Dive == "surfaced") || In.Dived > 1.0) return Out;
		// Each man's place by the gun (metres behind its mount's middle along its bore, and to its right), as it lies laid
		// now: a boat's gun's layer and trainer beside it, its loader behind its breech (0.65 m behind the mount), the
		// next round's man behind him; a merchant's crew in its platform behind its shield, two by two.
		const bool bShield = M.Shield > 0.0;
		const double Back[CrewMost] = { bShield ? M.Shield + 0.25 : 0.1, bShield ? M.Shield + 0.25 : 0.1, bShield ? M.Shield + 0.75 : 1.2, bShield ? M.Shield + 0.75 : 2.1 };
		const double Aside[CrewMost] = { bShield ? -0.65 : -0.85, bShield ? 0.65 : 0.85, 0.35, -0.35 };
		const double Bore = In.Train * LookPi / 180.0;
		const double Ahead = std::cos(Bore), Starboard = std::sin(Bore);
		for (int K = 0; K < CrewMost; ++K)
		{
			CrewMan Man;
			// (turned with the gun: behind it is against its bore, its right a quarter round clockwise of it)
			Man.Fore = -Back[K] * Ahead - Aside[K] * Starboard;
			Man.Right = -Back[K] * Starboard + Aside[K] * Ahead;
			// (on its deck: no further than Room from its middle, nor HalfWide from her middle line; one past her side kept
			// as far from the mount, along her instead)
			const double Far = std::hypot(Man.Fore, Man.Right);
			if (Far > M.Room)
			{
				Man.Fore *= M.Room / Far;
				Man.Right *= M.Room / Far;
			}
			if (std::fabs(Man.Right) > M.HalfWide)
			{
				const double Spare = std::sqrt(std::max(0.0, Man.Fore * Man.Fore + Man.Right * Man.Right - M.HalfWide * M.HalfWide));
				Man.Right = Man.Right > 0.0 ? M.HalfWide : -M.HalfWide;
				Man.Fore = Man.Fore >= 0.0 ? Spare : -Spare;
			}
			Man.Facing = In.Train;
			const uint64_t Own = static_cast<uint64_t>(static_cast<int64_t>(In.Key)) * 8u + static_cast<uint64_t>(K);
			Man.Clip = Hash01(Own * 3u + 1u) * 4.0;
			const double Late = Hash01(Own * 3u + 2u) * DuckLate;
			Man.bDuck = In.ShakenAt >= 0.0 && In.T >= In.ShakenAt + Late && In.T < In.ShakenAt + Late + DuckFor;
			Out.push_back(Man);
		}
		return Out;
	}

	// -------------------------------------------------------------------------------------------------------------
	// A hit's looks on a boat and a merchant

	bool PutsOutLooks(const std::string& Dive, double Dived)
	{
		// (told down, her casing awash at the diving alarm, or under: the show's own line between up and under, a metre)
		return Dive != "surfaced" || Dived > 1.0;
	}

	std::string HitLookFamily(const std::string& Family, const std::string& Weapon)
	{
		return Family == "mv" && Weapon.rfind("shell_", 0) == 0 ? std::string("ap") : Family;
	}

	bool MeshShape::Holds(int Points, const int* Corners, int Count) const
	{
		if (Kept.empty() || Count < 0 || static_cast<size_t>(Count) + 1 != Kept.size() || Kept[0] != Points) return false;
		return Count == 0 || std::memcmp(Corners, Kept.data() + 1, sizeof(int) * static_cast<size_t>(Count)) == 0;
	}

	void MeshShape::Keep(int Points, const int* Corners, int Count)
	{
		Kept.clear();
		Kept.reserve(static_cast<size_t>(std::max(0, Count)) + 1);
		Kept.push_back(Points);
		if (Count > 0) Kept.insert(Kept.end(), Corners, Corners + Count);
	}
}
