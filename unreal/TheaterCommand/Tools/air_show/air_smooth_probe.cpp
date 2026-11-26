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

#include "Battle/AirDesk.h"
#include "Battle/AirFeed.h"
#include "Battle/AirGround.h"
#include "Battle/AirShow.h"
#include "Battle/AirSky.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace
{
	/** The Battle rules card's "Each bomber keeps her place in her box" (flyBoxKeep) as the probe flies it: on unless
	 * AIR_SMOOTH_BOX=0. (Built against Battle files from before the row, AIR_PROBE_NO_BOX_ROW, it is never set.) */
	bool BoxRowOn() { return !std::getenv("AIR_SMOOTH_BOX") || std::string(std::getenv("AIR_SMOOTH_BOX")) != "0"; }

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

// The shake check (--shake; Brian, 25 November 13:27Z: "the bomber is shaking in the video too like the fighter"). The
// tremor above, at a steady 60 frames a second, is what a 0.4 s parabola leaves: above about 3 Hz. A bomber that rocks
// once or twice a second passes it as smooth, and so does a picture that jumps as a whole. This flies the recording as
// a Mac session does and looks at it as the scene does:
//  - the frames: a steady 60 a second, or "mac" (a GPU-bound Mac: 16.7, 22, 33 and 45 ms frames and about one in 75 a
//    hitch of 80 to 120 ms, each clamped to 0.1 s as AAirBattle::Tick does), or N a second;
//  - the clock: x1, x4 or x10 (AirDesk::Clock::Step's arithmetic), "game" (AirDesk::Clock itself: x10 until the
//    formations meet and once the fight winds down) or "b1:T" (the Mac's B1 run: the game's clock, x4 pressed at battle
//    second T);
//  - the views: "replay" (every view up to a battle second ahead, as a recording plays), "live" (AAirBattle::Feed: one
//    call of the rules a frame for what they owe a battle second ahead, the rules' clock stepping whole tenths, only
//    the newest view taken and the events of the others with it), or "late" (views that arrive 0 to 60 ms late, one in
//    twenty bunched with the next few);
// and measures, by model, by what each aircraft is about and by the clock's speed:
//  - HI, what a 0.4 s parabola leaves (above about 3 Hz), and BAND, the 0.4 s parabola less the 1.5 s one (about 0.5
//    to 3 Hz: a rock), of place, heading, pitch and bank, rms and peak; and the jumps of its roll rate from one frame to
//    the next over 10 and over 30 degrees a second, a minute;
//  - the chase camera riding it (AAirBattle::AimEye's close view: a bomber's eye 1.8 spans behind, 0.55 out and 0.45
//    up, looking 1.2 spans ahead, a fighter's 22 m behind and 6 up; its way eased over 0.33 s, its facing eased at 6 a
//    second with a quarter of the bank as its roll, through the 62 degree lens): its middle and a wing tip on the 1440
//    picture, HI and BAND, pixels;
//  - each bomber type in its place in a box: its swings of bank over a degree (each way, a bomber's air minute), their middle
//    and largest, the share of its time banked over 5 degrees and over 50 m from its slot, and the most it banked there
//    (3 air seconds after taking it); each torpedo plane's farthest from her slot in the 20 air seconds before her run;
//  - the strategy camera, as AimEye has it before (the hard rings, a copy here) and flying smooth (AirDesk::Overview,
//    the game's own), through the 55 degree lens on 1440 by 878: what a 0.4 s and a 1.2 s parabola leave of each
//    aircraft's place on the picture (rms, the 99th percentile, the peak), the look's jumps of 20 pixels or more beyond
//    its glide (a real minute; while it holds its crowd, 2 s and more after a change of the groups looked at or of its
//    anchor, and in all: a move to another crowd is a pan, eased, but at x4 and x10 a crowd kilometres away is a fast
//    one), the eye's steps in distance over 2% beyond its glide, and how far the look trails what it is after (95th
//    percentile, while it holds its crowd; and the share of all frames it is within 100 m of it, flying smooth only).
// Flying smooth (the default) it judges, and fails where (the numbers measured on raid-steps.json and the set pieces,
// Hampshire before and after, are in Tools/air_show/run.sh):
//  - the strategy camera, smooth: the bombers on its picture over 1.5 px rms or 2 px at the 99th percentile in 0.4 s,
//    or over 5 px rms in 1.2 s; the fighters over 1 px rms in 0.4 s; a look jump of 20 px more than once a real minute
//    while it holds its crowd; an eye step over 2%; its look farther than 480 px on the picture from what it is after,
//    the middle two thirds, or the old camera's (95th percentile, holding its crowd). At x10 and on the game's own
//    clock (x10 to the meeting): the bombers over 3 px rms or 10 at the 99th percentile in 0.4 s, the 1.2 s bombers
//    and the 0.4 s fighters worse than the old camera's (1.2 s is 12 air seconds of the crowd's own turns), more than
//    two look jumps a real minute, and the look off the picture, 720 px (or the old camera's);
//  - a bomber in her box: bank BAND over 0.2, 0.25 and 0.6 degrees rms at x1, x4 and x10 for a type of two engines or
//    20 m of span, 0.2, 1.2 and 2 for a single-engined type; the chase camera's wing tip BAND over 0.2, 1 and 2.5 px
//    rms (1, 6 and 12 single-engined); roll-rate jumps over 10 degrees a second more than once a minute at x1 and x4,
//    over 30 more than three times a minute at x10 (eight single-engined); banked more than RallyBankMost and a
//    degree there; swings over 1 degree more than 0.3 times a bomber's air minute (where a type has an air minute and
//    more); banked over 5 degrees more than 1% of her time; a torpedo plane over 100 m from her slot in the 20 air
//    seconds before her run. Her bank and tip BAND and roll-rate jumps in her box are taken only where she is in it
//    all the 1.5 s about the moment (her roll out of it, for home or into a dive, is not her box rocking);
//  - the strategy camera at x10 and on the game's clock: its look within 100 m of what it is after in fewer than 95%
//    of the frames, or than the old camera's share.
// AIR_SMOOTH_OLD=1 flies it as release 1.5 did, with the strategy camera as it was, and reports only; its report is
// the same, to the byte, as the release 2 build's before this check (Tools/air_show/data/shake-old-30899121.txt, run.sh).
// The Battle rules card's rows, as a battle reads them when it opens (each on unless set to 0): AIR_SMOOTH_EYE=0, "The
// raid camera holds steady" off, the scene's eye the strategy camera as it was (both cameras are measured either way);
// AIR_SMOOTH_BOX=0, "Each bomber keeps her place in her box" off (AirShow::Show::SetBoxKeep). run.sh holds the report
// with both off to 30899121's own flying (data/shake-rows-off-30899121.txt), with the camera's alone on to 30899121's
// flying under 2464374bb's camera (data/shake-eye-only-30899121.txt), and with the bombers' alone on to 2464374bb's
// (data/shake-eye-off-2464374bb.txt).
//
// Usage: air_smooth_probe --shake <recording.json> <frames: 60|mac|N> <speed: 1|4|10|game|b1:T> <views: replay|live|late>
//                         [from battle s] [to battle s] [seed, 1]
namespace Shake
{
	constexpr double Pi = 3.14159265358979323846;
	double Rad(double D) { return D * Pi / 180.0; }
	double Deg(double R) { return R * 180.0 / Pi; }
	double Wrap180(double A)
	{
		while (A > 180.0) A -= 360.0;
		while (A < -180.0) A += 360.0;
		return A;
	}

	/** The frames' real seconds. */
	struct Pacer
	{
		std::string Kind;
		std::mt19937_64 Dice;
		Pacer(const std::string& InKind, uint64_t Seed) : Kind(InKind), Dice(Seed) {}
		double U() { return std::uniform_real_distribution<double>(0.0, 1.0)(Dice); }
		double Next()
		{
			if (Kind == "mac")
			{
				if (U() < 1.0 / 75.0) return 0.080 + 0.040 * U();
				const double Q = U();
				const double Base = Q < 0.25 ? 0.0167 : Q < 0.55 ? 0.022 : Q < 0.85 ? 0.033 : 0.045;
				return Base + 0.001 * (U() * 2.0 - 1.0);
			}
			const double N = std::atof(Kind.c_str());
			return 1.0 / (N > 0.0 ? N : 60.0);
		}
	};

	/** A vector in the scene's own axes (X north, Y east, Z up), metres: AAirBattle::World with a metre a unit. */
	struct V3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		V3() = default;
		V3(double A, double B, double C) : X(A), Y(B), Z(C) {}
		V3 operator+(const V3& O) const { return V3(X + O.X, Y + O.Y, Z + O.Z); }
		V3 operator-(const V3& O) const { return V3(X - O.X, Y - O.Y, Z - O.Z); }
		V3 operator*(double S) const { return V3(X * S, Y * S, Z * S); }
		double Dot(const V3& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		double Size() const { return std::sqrt(Dot(*this)); }
		V3 Unit() const { const double S = Size(); return S < 1e-12 ? V3() : *this * (1.0 / S); }
	};
	V3 World(const AirShow::Vec& P) { return V3(P.Y, P.X, P.Z); }
	AirShow::Vec Board(const V3& W) { return AirShow::Vec(W.Y, W.X, W.Z); }

	/** FRotator (pitch, yaw, roll, degrees) and its matrix's axes: forward, right, up. */
	struct Rot { double P = 0.0, Y = 0.0, R = 0.0; };
	void Axes(const Rot& Ro, V3& F, V3& Ri, V3& Up)
	{
		const double SP = std::sin(Rad(Ro.P)), CP = std::cos(Rad(Ro.P));
		const double SY = std::sin(Rad(Ro.Y)), CY = std::cos(Rad(Ro.Y));
		const double SR = std::sin(Rad(Ro.R)), CR = std::cos(Rad(Ro.R));
		F = V3(CP * CY, CP * SY, SP);
		Ri = V3(SR * SP * CY - CR * SY, SR * SP * SY + CR * CY, -SR * CP);
		Up = V3(-(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY, CR * CP);
	}
	/** FVector::Rotation. */
	Rot RotationOf(const V3& V) { Rot R; R.Y = Deg(std::atan2(V.Y, V.X)); R.P = Deg(std::atan2(V.Z, std::sqrt(V.X * V.X + V.Y * V.Y))); return R; }
	/** An aircraft's own axes from its heading (clockwise from north), pitch and bank (right wing down), scene axes. */
	void Attitude(double H, double P, double B, V3& F, V3& Ri, V3& Up) { Axes(Rot{ P, H, B }, F, Ri, Up); }

	struct Eye
	{
		V3 At;
		Rot Face;
		double Lens = 55.0;
		bool bValid = false;
		/** P on the 1440 by 878 picture, false behind the eye. */
		bool Project(const V3& P, double& Sx, double& Sy) const
		{
			V3 F, R, U;
			Axes(Face, F, R, U);
			const V3 D = P - At;
			const double X = D.Dot(F);
			if (X < 1.0) return false;
			const double Px = 720.0 / std::tan(Rad(Lens * 0.5));
			Sx = 720.0 + D.Dot(R) / X * Px;
			Sy = 439.0 - D.Dot(U) / X * Px;
			return true;
		}
	};

	/** The rms, peak and percentiles of a set of residuals. */
	struct Stat
	{
		double Sum2 = 0.0, Peak = 0.0;
		long N = 0;
		std::vector<double> All;
		bool bKeep = false;
		void Add(double R)
		{
			Sum2 += R * R;
			++N;
			Peak = std::max(Peak, std::fabs(R));
			if (bKeep) All.push_back(std::fabs(R));
		}
		double Rms() const { return N ? std::sqrt(Sum2 / static_cast<double>(N)) : 0.0; }
		double Pct(double Q)
		{
			if (All.empty()) return 0.0;
			std::sort(All.begin(), All.end());
			return All[std::min(All.size() - 1, static_cast<size_t>(Q * static_cast<double>(All.size())))];
		}
	};

	/** The weights of the least-squares parabola through the samples within +-M of T[C] whose value at T[C] they give. */
	bool Weights(const std::vector<double>& T, size_t C, double M, size_t& Lo, size_t& Hi, std::vector<double>& W)
	{
		Lo = C;
		while (Lo > 0 && T[C] - T[Lo - 1] <= M + 1e-9) --Lo;
		Hi = C;
		while (Hi + 1 < T.size() && T[Hi + 1] - T[C] <= M + 1e-9) ++Hi;
		if (Hi - Lo < 4) return false;
		double S0 = 0.0, S1 = 0.0, S2 = 0.0, S3 = 0.0, S4 = 0.0;
		for (size_t I = Lo; I <= Hi; ++I)
		{
			const double D = T[I] - T[C], D2 = D * D;
			S0 += 1.0; S1 += D; S2 += D2; S3 += D2 * D; S4 += D2 * D2;
		}
		const double Co0 = S2 * S4 - S3 * S3, Co1 = -(S1 * S4 - S3 * S2), Co2 = S1 * S3 - S2 * S2;
		const double Det = S0 * Co0 + S1 * Co1 + S2 * Co2;
		if (std::fabs(Det) < 1e-18) return false;
		W.resize(Hi - Lo + 1);
		for (size_t I = Lo; I <= Hi; ++I)
		{
			const double D = T[I] - T[C];
			W[I - Lo] = (Co0 + Co1 * D + Co2 * D * D) / Det;
		}
		return true;
	}
	double Fit(const std::vector<double>& V, size_t Lo, size_t Hi, const std::vector<double>& W)
	{
		double S = 0.0;
		for (size_t I = Lo; I <= Hi; ++I) S += W[I - Lo] * V[I];
		return S;
	}

	enum Doing : int { OnBox, OnJoining, OnRun, OnRally, OnStraggle, OnHome, OnDive, OnOther, OnFormation, OnFight };
	const char* DoingName[] = { "box", "joining", "run", "rally/after", "straggle", "home", "dive/torpedo", "other", "formation", "fight" };
	enum Pace : int { X1, X2, X4, Easing, X10, Paces };
	const char* PaceName[] = { "x1", "x2", "x4", "easing", "x10" };

	struct Sample
	{
		double T = 0.0, Air = 0.0; // real and air seconds
		double X = 0.0, Y = 0.0, Z = 0.0, H = 0.0, P = 0.0, B = 0.0, Span = 12.0, Gap = -1.0;
		int Frame = 0;
		Doing D = OnOther;
		bool bTorpedoRun = false;
	};
	struct Track
	{
		int Key = 0, Group = 0;
		bool bFighter = false, bTwin = false;
		std::string Model;
		std::vector<Sample> S;
	};
	struct Cell
	{
		Stat PlaceHi, PlaceBand, HeadBand, PitchBand, BankHi, BankBand, TipHi, TipBand, MidHi, MidBand;
		long Jumps10 = 0, Jumps30 = 0;
		double Seconds = 0.0;
		std::set<int> Keys;
		double PerMinute(long J) const { return Seconds > 0.0 ? 60.0 * static_cast<double>(J) / Seconds : 0.0; }
	};
	struct BoxRow
	{
		double Seconds = 0.0, Banked = 0.0, Far = 0.0, GapSeconds = 0.0, Most = 0.0, MostJoining = 0.0;
		std::vector<double> Swings;
		std::vector<double> TorpedoGaps;
		bool bTwin = false;
	};

	/** A strategy camera: before (the hard rings, AimEye's as it was) or flying smooth (AirDesk::Overview). */
	struct Strategy
	{
		bool bSmooth = false;
		// (before)
		int Anchor = -1;
		V3 LookAt, LookWanted;
		bool bLookSet = false;
		// (smooth)
		AirDesk::Overview Over;
		AirShow::Vec Look;
		// (both)
		Eye Now;
		std::vector<double> T, Away, Lag;
		std::vector<V3> Looks;
		std::vector<char> Settled;
		double Changed = -1e9;  // real time of the last change of the groups looked at or of the anchor
		std::vector<int> PickedWas;
		int AnchorWas = -2;
		std::map<int, std::vector<std::array<double, 3>>> Seen; // key: real s, x, y on the picture
		std::map<int, std::vector<int>> SeenFrame;
	};

	/** The groups the strategy camera looks at with none in hand (AimEye): those fighting, else his own bombers. */
	std::vector<int> PickedOf(const std::map<int, AirShow::Group>& Groups, int Me, AirShow::Vec& Sum, int& Count)
	{
		std::vector<int> Picked;
		Sum = AirShow::Vec();
		Count = 0;
		for (const auto& Pair : Groups)
		{
			const AirShow::Group& G = Pair.second;
			if (G.Doing == AirShow::Act::Down || G.Doing == AirShow::Act::Idle) continue;
			if (G.Doing == AirShow::Act::Fight) { Sum += AirShow::Vec(G.X, G.Y, G.Alt); Picked.push_back(G.Id); ++Count; }
		}
		if (Count == 0)
		{
			for (const auto& Pair : Groups)
			{
				const AirShow::Group& G = Pair.second;
				if (G.Side == Me && !G.bFighter && G.Doing != AirShow::Act::Down) { Sum += AirShow::Vec(G.X, G.Y, G.Alt); Picked.push_back(G.Id); ++Count; }
			}
		}
		return Picked;
	}

	/** The old law's eye distance for a crowd's middle span (AimEye before). */
	double OldTypeAway(double Span) { return Span > 0.0 ? std::clamp(Span * 1920.0 / ((Span >= 25.0 ? 70.0 : 45.0) * 2.0 * std::tan(Rad(27.5))), 250.0, 1500.0) : 800.0; }

	/** One frame of a strategy camera, as AimEye has it (no group in hand, the player's zoom 800 m, pitch -12). */
	void Aim(Strategy& C, const std::vector<AirShow::PlaneOut>& Planes, const std::map<int, AirShow::Group>& Groups, const AirFeed::Sky& B, int Me, double Yaw, double Real, double RealT)
	{
		AirShow::Vec Focus(B.TargetX, B.TargetY, 3000.0), Sum;
		int Count = 0;
		const std::vector<int> Picked = PickedOf(Groups, Me, Sum, Count);
		double TypeAway = 800.0;
		int AnchorNow = -1;
		if (C.bSmooth)
		{
			const AirDesk::Overview::Aim A = C.Over.Step(Planes, Picked, Real, C.Anchor, !C.bLookSet);
			if (A.bFound) Focus = A.Focus;
			else if (Count > 0) Focus = Sum * (1.0 / Count);
			TypeAway = A.TypeAway;
			C.Look = C.bLookSet ? C.Over.Look(C.Look, Focus, Real) : Focus;
			C.LookAt = World(C.Look);
		}
		else
		{
			// (AimEye as it was: the middle of those within 350 m of the crowd's middle, else the nearest; the span the
			// middle one's; the look carried by any step under 150 m and eased 1.2 s)
			AirShow::Vec Crowd;
			double Span = 0.0;
			if (AirDesk::CrowdOf(Planes, Picked, 1200.0, C.Anchor, Crowd))
			{
				Focus = Crowd;
				std::vector<double> Spans;
				const AirShow::PlaneOut* Nearest = nullptr;
				double NearestFar2 = 1e18;
				AirShow::Vec Tight;
				int TightCount = 0;
				for (const AirShow::PlaneOut& P : Planes)
				{
					if (!P.bShown || P.bFalling || P.bOnGround || std::find(Picked.begin(), Picked.end(), P.Group) == Picked.end()) continue;
					const AirShow::Vec Off = P.At - Crowd;
					const double Far2 = Off.Dot(Off);
					if (Far2 > 1200.0 * 1200.0) continue;
					Spans.push_back(P.Span > 1.0 ? P.Span : 12.0);
					if (Far2 < NearestFar2) { NearestFar2 = Far2; Nearest = &P; }
					if (Far2 < 350.0 * 350.0) { Tight += P.At; ++TightCount; }
				}
				if (!Spans.empty())
				{
					std::nth_element(Spans.begin(), Spans.begin() + static_cast<long>(Spans.size() / 2), Spans.end());
					Span = Spans[Spans.size() / 2];
				}
				if (TightCount >= 2) Focus = Tight * (1.0 / TightCount);
				else if (Nearest) Focus = Nearest->At;
			}
			else if (Count > 0) Focus = Sum * (1.0 / Count);
			TypeAway = OldTypeAway(Span);
			const V3 Wanted = World(Focus);
			if (C.bLookSet)
			{
				const V3 Moved = Wanted - C.LookWanted;
				if (Moved.Size() < 150.0) C.LookAt = C.LookAt + Moved;
				C.LookAt = C.LookAt + (Wanted - C.LookAt) * (1.0 - std::exp(-Real / 1.2));
			}
			else C.LookAt = Wanted;
			C.LookWanted = Wanted;
		}
		AnchorNow = C.Anchor;
		C.bLookSet = true;
		V3 F, R, U;
		Axes(Rot{ -12.0, Yaw, 0.0 }, F, R, U);
		V3 From = C.LookAt - F * TypeAway;
		From.Z = std::max(From.Z, 40.0);
		C.Now.At = From;
		C.Now.Face = RotationOf(C.LookAt - From);
		C.Now.Lens = 55.0;
		C.Now.bValid = true;
		if (Picked != C.PickedWas || AnchorNow != C.AnchorWas) C.Changed = RealT;
		C.PickedWas = Picked;
		C.AnchorWas = AnchorNow;
		C.T.push_back(RealT);
		C.Away.push_back(TypeAway);
		C.Looks.push_back(C.LookAt);
		C.Lag.push_back((C.LookAt - World(Focus)).Size());
		C.Settled.push_back(RealT - C.Changed >= 2.0 ? 1 : 0);
	}

	struct Verdict
	{
		bool bFailed = false;
		void Judge(bool bPass, const std::string& What)
		{
			std::printf("  %s: %s\n", bPass ? "pass" : "FAIL", What.c_str());
			bFailed = bFailed || !bPass;
		}
	};
	std::string Say(const char* Format, ...)
	{
		char Line[512];
		va_list Args;
		va_start(Args, Format);
		std::vsnprintf(Line, sizeof Line, Format, Args);
		va_end(Args);
		return Line;
	}

	int Play(int Argc, char** Argv)
	{
		if (Argc < 5)
		{
			std::fprintf(stderr, "usage: air_smooth_probe --shake <recording.json> <60|mac|N> <1|4|10|game|b1:T> <replay|live|late> [from] [to] [seed]\n");
			return 2;
		}
		const std::string Path = Argv[1], FrameKind = Argv[2], SpeedKind = Argv[3], FeedKind = Argv[4];
		const double From = Argc > 5 ? std::atof(Argv[5]) : 0.0;
		const double To = Argc > 6 ? std::atof(Argv[6]) : 1e18;
		const uint64_t Seed = Argc > 7 ? static_cast<uint64_t>(std::atoll(Argv[7])) : 1;
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
		// (the Battle rules card's rows, as a battle reads them when it opens; each on unless set to 0 here: the raid
		// camera, "flySteadyEye", AIR_SMOOTH_EYE=0 the scene's eye the strategy camera as it was (both are measured))
		const bool bEyeRow = !std::getenv("AIR_SMOOTH_EYE") || std::string(std::getenv("AIR_SMOOTH_EYE")) != "0";
		// (and the bombers' place in their box, "flyBoxKeep", AIR_SMOOTH_BOX=0 off: AirShow::Show::SetBoxKeep)
		const bool bBoxRow = BoxRowOn();
#ifndef AIR_PROBE_NO_BOX_ROW
		Show.SetBoxKeep(bBoxRow);
#endif
		Show.Eye = AirShow::Vec(Rec.Board.TargetX, Rec.Board.TargetY - 6000.0, 5000.0);
		const std::shared_ptr<const AirGround::BoardLand> BoardGround =
			Rec.Board.StageKey.empty() ? std::make_shared<const AirGround::BoardLand>(Rec.Board.Land) : std::shared_ptr<const AirGround::BoardLand>();
		if (BoardGround && !BoardGround->IsEmpty()) Show.Ground = [BoardGround](double X, double Y) { return BoardGround->At(X, Y); };
		const double Scale = std::max(0.1, Rec.Board.Scale);
		if (FeedKind != "replay" && (Rec.Views.size() < 3 || std::fabs(Rec.Views[1].Head.T - Rec.Views[0].Head.T - 0.1) > 1e-6))
		{
			std::fprintf(stderr, "%s views need a view every rules step (0.1 battle s)\n", FeedKind.c_str());
			return 2;
		}

		Pacer Frames(FrameKind, Seed);
		std::mt19937_64 FeedDice(Seed * 7919 + 13);
		auto FU = [&FeedDice]() { return std::uniform_real_distribution<double>(0.0, 1.0)(FeedDice); };
		AirDesk::Clock Clock;
		const double PressAt = SpeedKind.rfind("b1:", 0) == 0 ? std::atof(SpeedKind.c_str() + 3) : -1.0;
		auto RunFor = [&](double Real, const AirFeed::Top& Head)
		{
			if (SpeedKind == "game" || PressAt >= 0.0)
			{
				Clock.Speed = PressAt >= 0.0 && Show.Shown() >= PressAt ? 4.0 : 1.0;
				Clock.bStrike = Show.StrikeOn();
				std::string Tag;
				return Clock.Step(Real, Head, Scale, Tag);
			}
			const double Sp = std::atof(SpeedKind.c_str());
			return Sp >= 10.0 ? Real * 10.0 : Real * Sp / Scale;
		};
		size_t Taken = 0, HeadAt = 0, RulesAt = 0, LateNext = 1;
		double FedT = Rec.Views.front().Head.T, BattleT = FedT, RulesOwed = 0.0, Ideal = FedT;
		std::vector<double> Arrive(Rec.Views.size(), 1e18);
		auto TakeView = [&](const AirFeed::Frame& F, const std::vector<AirShow::Event>* More)
		{
			if (More && !More->empty())
			{
				std::vector<AirShow::Event> All = *More;
				All.insert(All.end(), F.Events.begin(), F.Events.end());
				Show.Take(F.Head.T, F.Groups, All, F.Ships, &F.Marks);
			}
			else Show.Take(F.Head.T, F.Groups, F.Events, F.Ships, &F.Marks);
			FedT = F.Head.T;
		};
		TakeView(Rec.Views[0], nullptr);
		Taken = 1;
		const double EndT = std::min(To, Rec.Views.back().Head.T);
		// (my side: the raider's; the camera starts behind his raid, looking along its way, AAirBattle's opening)
		const int Me = 0;
		double Yaw = 200.0;
		bool bYaw = false;
		Strategy Before, Smooth;
		Smooth.bSmooth = true;
		std::vector<Strategy*> Cams = { &Before };
		if (!bOld) Cams.push_back(&Smooth);
		std::map<int, Track> Tracks;
		std::vector<double> FrameReal;
		std::vector<int> FramePace;
		int Frame = 0;
		double Wall = 0.0, WindowAt = -1.0;
		const bool bLeadIn = From - Rec.Views.front().Head.T > 20.0;
		while (BattleT < EndT - 1e-9)
		{
			const bool bFastLeadIn = bLeadIn && BattleT < From - 10.0;
			const double Real = bFastLeadIn ? 1.0 / 60.0 : std::min(Frames.Next(), 0.1);
			Wall += Real;
			const double Run = bFastLeadIn ? 10.0 * Real : RunFor(Real, Rec.Views[HeadAt].Head);
			BattleT = std::min(EndT, BattleT + Run);
			if (FeedKind == "replay" || bFastLeadIn)
			{
				while (Taken < Rec.Views.size() && Rec.Views[Taken].Head.T <= BattleT + 1.0 + 1e-9) TakeView(Rec.Views[Taken++], nullptr);
				RulesAt = Taken - 1;
				LateNext = Taken;
			}
			else if (FeedKind == "live")
			{
				const double Owed = BattleT + 1.0 - FedT;
				if (Owed > 1e-6)
				{
					RulesOwed += Owed;
					int N = 0;
					std::vector<size_t> Ticked;
					while (RulesOwed >= 0.1 - 1e-9 && N < 40 && RulesAt + 1 < Rec.Views.size())
					{
						++RulesAt;
						RulesOwed -= 0.1;
						++N;
						Ticked.push_back(RulesAt);
					}
					std::vector<AirShow::Event> Skipped;
					for (size_t K = 0; K + 1 < Ticked.size(); ++K) Skipped.insert(Skipped.end(), Rec.Views[Ticked[K]].Events.begin(), Rec.Views[Ticked[K]].Events.end());
					if (N >= 40) RulesOwed = 0.0;
					if (Rec.Views[RulesAt].Head.T > FedT + 1e-9) TakeView(Rec.Views[RulesAt], &Skipped);
					Taken = RulesAt + 1;
				}
				BattleT = std::min(BattleT, FedT);
			}
			else
			{
				Ideal += Run;
				while (LateNext < Rec.Views.size() && Rec.Views[LateNext].Head.T <= Ideal + 1e-9 && Arrive[LateNext] > 1e17)
				{
					Arrive[LateNext] = Wall + 0.060 * FU();
					if (FU() < 0.05) Arrive[LateNext] += static_cast<double>(1 + static_cast<int>(FU() * 3.0)) * 0.1 * (Run > 1e-9 ? Real / Run : 1.0);
					++LateNext;
				}
				while (Taken < Rec.Views.size() && Arrive[Taken] <= Wall) TakeView(Rec.Views[Taken++], nullptr);
				BattleT = std::min(BattleT, FedT);
			}
			if (Smooth.Now.bValid || Before.Now.bValid)
			{
				const V3 E = (bOld || !bEyeRow ? Before : Smooth).Now.At;
				Show.Eye = Board(E);
			}
			Show.Advance(BattleT);
			while (HeadAt + 1 < Rec.Views.size() && Rec.Views[HeadAt + 1].Head.T <= Show.Shown() + 1e-9) ++HeadAt;
			++Frame;
			FrameReal.push_back(Real);
			{
				const double Ratio = Real > 0.0 ? Run / Real : 0.0;
				const double X = Ratio * Scale;
				FramePace.push_back(Ratio >= 9.0 ? X10 : X < 1.5 ? X1 : X < 3.0 ? X2 : X < 6.0 ? X4 : Easing);
			}
			const std::vector<AirShow::PlaneOut>& Planes = Show.Planes();
			const std::map<int, AirShow::Group>& Groups = Show.Groups();
			if (!bYaw)
			{
				for (const auto& Pair : Groups)
				{
					if (Pair.second.Side == Me && !Pair.second.bFighter) { Yaw = Deg(Pair.second.Face); bYaw = true; break; }
				}
			}
			for (Strategy* C : Cams) Aim(*C, Planes, Groups, Rec.Board, Me, Yaw, Real, Wall);
			if (BattleT < From) continue;
			if (WindowAt < 0.0) WindowAt = Wall;
			// (what each bomber is about: Show::Describe's role, her bay, the bombers out of their box)
			std::map<int, std::string> Role;
			{
				std::istringstream Lines(Show.Describe());
				std::string Line;
				while (std::getline(Lines, Line))
				{
					if (Line.empty() || Line[0] == '#') continue;
					std::istringstream W(Line);
					int Key = 0;
					std::string G, Kind, RoleStep;
					W >> Key >> G >> Kind >> RoleStep;
					Role[Key] = RoleStep;
				}
			}
			const AirShow::Checks& Check = Show.Checked();
			for (const AirShow::PlaneOut& P : Planes)
			{
				if (!P.bShown || P.bFalling || P.bOnGround) continue;
				Track& Tr = Tracks[P.Key];
				if (Tr.S.empty())
				{
					Tr.Key = P.Key; Tr.Group = P.Group; Tr.bFighter = P.bFighter; Tr.Model = P.Model;
					const AirShow::Shape Form = AirShow::ShapeOf(P.Model, P.bFighter);
					Tr.bTwin = !P.bFighter && (Form.Span >= 20.0 || Form.Engines.size() >= 2 || P.Span >= 20.0);
				}
				Sample S;
				S.T = Wall; S.Air = Show.AirNow(); S.X = P.At.X; S.Y = P.At.Y; S.Z = P.At.Z; S.H = P.Heading; S.P = P.Pitch; S.B = P.Bank; S.Span = P.Span; S.Frame = Frame;
				const std::string& RS = Role[P.Key];
				const std::string R = RS.substr(0, RS.find('/'));
				if (P.bFighter) S.D = (P.Task.empty() || P.Task == "escorting" || P.Task == "closing") && R == "slot" ? OnFormation : OnFight;
				else if (R == "slot")
				{
					if (P.Bay > 0.0 && !P.bBombsGone) S.D = OnRun;
					else if (P.bBombsGone) S.D = OnRally;
					else if (Check.NotInBox.count(P.Key)) S.D = OnJoining;
					else S.D = OnBox;
				}
				else if (R == "straggle" || R == "cover") S.D = OnStraggle;
				else if (R == "home") S.D = OnHome;
				else if (R == "dive")
				{
					S.D = OnDive;
					const auto G = Groups.find(P.Group);
					S.bTorpedoRun = G != Groups.end() && G->second.Strike == "torpedo";
				}
				else S.D = OnOther;
				const auto Gap = Check.SlotGap.find(P.Key);
				if (Gap != Check.SlotGap.end()) S.Gap = Gap->second;
				Tr.S.push_back(S);
			}
			for (Strategy* C : Cams)
			{
				for (const AirShow::PlaneOut& P : Planes)
				{
					if (!P.bShown || P.bFalling || P.bOnGround) continue;
					double Sx = 0.0, Sy = 0.0;
					if (!C->Now.Project(World(P.At), Sx, Sy) || Sx < -50.0 || Sx > 1490.0 || Sy < -50.0 || Sy > 928.0) continue;
					C->Seen[P.Key].push_back({ Wall, Sx, Sy });
					C->SeenFrame[P.Key].push_back(Frame);
				}
			}
		}

		// ---- the reckoning ----
		std::printf("%s | frames %s, clock %s, views %s%s, battle %.0f to %.0f s | %d frames over %.1f real s\n", Path.substr(Path.find_last_of('/') + 1).c_str(), FrameKind.c_str(), SpeedKind.c_str(),
			(FeedKind + (bOld ? ", flown as release 1.5 (AIR_SMOOTH_OLD)" : "") + (!bOld && !bEyeRow ? ", the raid camera's row off (AIR_SMOOTH_EYE=0)" : "") + (!bOld && !bBoxRow ? ", the bombers' row off (AIR_SMOOTH_BOX=0)" : "")).c_str(), "", From, EndT, Frame, Wall);
		std::map<std::tuple<std::string, int, int>, Cell> Cells; // (model, doing, pace)
		std::map<std::string, BoxRow> Boxes;
		std::map<std::pair<std::string, int>, Cell> InBox; // (model, pace): a bomber in her box all the 1.5 s about it
		for (auto& Pair : Tracks)
		{
			Track& Tr = Pair.second;
			BoxRow& Bx = Boxes[Tr.Model];
			if (!Tr.bFighter) Bx.bTwin = Tr.bTwin;
			size_t Begin = 0;
			while (Begin < Tr.S.size())
			{
				size_t End = Begin + 1;
				while (End < Tr.S.size() && Tr.S[End].Frame == Tr.S[End - 1].Frame + 1) ++End;
				const size_t N = End - Begin;
				// Each bomber in her box: her swings of bank (a turn of over a degree each way), the time banked over 5
				// degrees and over 50 m from her slot, the most she banked in her place 3 air seconds after taking it;
				// and a torpedo plane's farthest from her slot in the 20 air seconds before her run.
				if (!Tr.bFighter)
				{
					size_t I = Begin;
					while (I < End)
					{
						const bool bSlot = Tr.S[I].D == OnBox || Tr.S[I].D == OnJoining || Tr.S[I].D == OnRally;
						size_t J = I + 1;
						while (J < End && (Tr.S[J].D == OnBox || Tr.S[J].D == OnJoining || Tr.S[J].D == OnRally) == bSlot) ++J;
						if (bSlot)
						{
							const double Took = Tr.S[I].Air;
							for (size_t K = I; K < J; ++K)
							{
								if (Tr.S[K].Air - Took < 3.0) continue;
								double& Most = Tr.S[K].D == OnJoining ? Bx.MostJoining : Bx.Most;
								Most = std::max(Most, std::fabs(Tr.S[K].B));
							}
						}
						I = J;
					}
					I = Begin;
					while (I < End)
					{
						size_t J = I + 1;
						while (J < End && (Tr.S[J].D == OnBox) == (Tr.S[I].D == OnBox)) ++J;
						if (Tr.S[I].D == OnBox && J - I > 1)
						{
							double Ext = Tr.S[I].B, Turn = Tr.S[I].B;
							int Way = 0;
							for (size_t K = I; K < J; ++K)
							{
								const Sample& S = Tr.S[K];
								const double Dt = K > I ? S.Air - Tr.S[K - 1].Air : 0.0;
								Bx.Seconds += Dt;
								Bx.Banked += std::fabs(S.B) > 5.0 ? Dt : 0.0;
								if (S.Gap >= 0.0) { Bx.GapSeconds += Dt; Bx.Far += S.Gap > 50.0 ? Dt : 0.0; }
								const double Bk = S.B;
								if (K == I) continue;
								if (Way >= 0 && Bk > Ext) { Ext = Bk; if (Bk - Turn > 1.0) Way = 1; continue; }
								if (Way <= 0 && Bk < Ext) { Ext = Bk; if (Turn - Bk > 1.0) Way = -1; continue; }
								if (Way == 1 && Ext - Bk > 1.0) { Bx.Swings.push_back(Ext - Turn); Turn = Ext; Ext = Bk; Way = -1; }
								else if (Way == -1 && Bk - Ext > 1.0) { Bx.Swings.push_back(Turn - Ext); Turn = Ext; Ext = Bk; Way = 1; }
							}
						}
						I = J;
					}
					for (size_t K = Begin + 1; K < End; ++K)
					{
						if (!Tr.S[K].bTorpedoRun || Tr.S[K - 1].bTorpedoRun) continue;
						double Farthest = -1.0;
						for (size_t Q = K; Q-- > Begin && Tr.S[K].Air - Tr.S[Q].Air <= 20.0;) Farthest = std::max(Farthest, Tr.S[Q].Gap);
						if (Farthest >= 0.0) Bx.TorpedoGaps.push_back(Farthest);
					}
				}
				if (N < 10 || Tr.S[End - 1].T - Tr.S[Begin].T < 3.2) { Begin = End; continue; }
				std::vector<double> T(N), X(N), Y(N), Z(N), H(N), P(N), B(N), Tip(N), MidX(N), MidY(N);
				double Hu = Tr.S[Begin].H, Bu = Tr.S[Begin].B;
				// (the close view riding her, frame by frame)
				V3 CloseAhead;
				Rot Facing;
				for (size_t I = 0; I < N; ++I)
				{
					const Sample& S = Tr.S[Begin + I];
					T[I] = S.T; X[I] = S.X; Y[I] = S.Y; Z[I] = S.Z; P[I] = S.P;
					if (I > 0) { Hu += Wrap180(S.H - Tr.S[Begin + I - 1].H); Bu += Wrap180(S.B - Tr.S[Begin + I - 1].B); }
					H[I] = Hu; B[I] = Bu;
					const double Real = FrameReal[static_cast<size_t>(S.Frame - 1)];
					V3 Fw, Rt, Upv;
					Attitude(S.H, S.P, S.B, Fw, Rt, Upv);
					const V3 Body = World(AirShow::Vec(S.X, S.Y, S.Z));
					CloseAhead = I == 0 ? Fw : (CloseAhead + (Fw - CloseAhead) * (1.0 - std::exp(-Real / 0.33))).Unit();
					V3 Flat = V3(CloseAhead.X, CloseAhead.Y, 0.0).Unit();
					const V3 FlatRight(-Flat.Y, Flat.X, 0.0); // (Up x Flat in the scene's axes)
					V3 Want, Look;
					if (Tr.bFighter)
					{
						Want = Body - CloseAhead * 22.0 + V3(0.0, 0.0, 6.0);
						Look = Body + CloseAhead * 80.0;
					}
					else
					{
						const double Size = std::max(12.0, S.Span);
						Want = Body - CloseAhead * (Size * 1.8) + FlatRight * (Size * 0.55) + V3(0.0, 0.0, Size * 0.45);
						Look = Body + CloseAhead * (Size * 1.2);
					}
					Rot Wanted = RotationOf(Look - Want);
					Wanted.R = std::clamp(S.B * 0.25, -15.0, 15.0);
					if (I == 0) Facing = Wanted;
					else
					{
						const double K = std::clamp(Real * 6.0, 0.0, 1.0);
						Facing.P = Wrap180(Facing.P + Wrap180(Wanted.P - Facing.P) * K);
						Facing.Y = Wrap180(Facing.Y + Wrap180(Wanted.Y - Facing.Y) * K);
						Facing.R = Wrap180(Facing.R + Wrap180(Wanted.R - Facing.R) * K);
					}
					Eye Close;
					Close.At = Want; Close.Face = Facing; Close.Lens = 62.0;
					double Lx = 0.0, Ly = 0.0;
					Close.Project(Body + Rt * (0.5 * S.Span), Lx, Ly);
					Tip[I] = Ly;
					Close.Project(Body, MidX[I], MidY[I]);
				}
				std::vector<double> W2, W15;
				for (size_t C = 0; C < N; ++C)
				{
					if (T[C] - T[0] < 1.5 || T[N - 1] - T[C] < 1.5) continue;
					size_t L2 = 0, H2 = 0, L15 = 0, H15 = 0;
					if (!Weights(T, C, 0.2, L2, H2, W2) || !Weights(T, C, 1.5, L15, H15, W15)) continue;
					// (a window across a change of the clock's speed is not tremor: the picture speeds up or slows as a whole)
					const int Pc = FramePace[static_cast<size_t>(Tr.S[Begin + C].Frame - 1)];
					bool bSame = true;
					for (size_t I = L15; I <= H15 && bSame; ++I) bSame = FramePace[static_cast<size_t>(Tr.S[Begin + I].Frame - 1)] == Pc;
					if (!bSame) continue;
					auto Bands = [&](const std::vector<double>& V, double& Hi, double& Band)
					{
						const double S2 = Fit(V, L2, H2, W2), S15 = Fit(V, L15, H15, W15);
						Hi = V[C] - S2;
						Band = S2 - S15;
					};
					double Xh, Xb, Yh, Yb, Zh, Zb, Hh, Hb, Ph, Pb, Bh, Bb, Th, Tb, Mxh, Mxb, Myh, Myb;
					Bands(X, Xh, Xb); Bands(Y, Yh, Yb); Bands(Z, Zh, Zb); Bands(H, Hh, Hb); Bands(P, Ph, Pb); Bands(B, Bh, Bb);
					Bands(Tip, Th, Tb); Bands(MidX, Mxh, Mxb); Bands(MidY, Myh, Myb);
					Cell& Ce = Cells[{ Tr.Model, Tr.S[Begin + C].D, Pc }];
					Ce.PlaceHi.Add(std::sqrt(Xh * Xh + Yh * Yh + Zh * Zh));
					Ce.PlaceBand.Add(std::sqrt(Xb * Xb + Yb * Yb + Zb * Zb));
					Ce.HeadBand.Add(Hb); Ce.PitchBand.Add(Pb); Ce.BankHi.Add(Bh); Ce.BankBand.Add(Bb);
					Ce.TipHi.Add(Th); Ce.TipBand.Add(Tb); Ce.MidHi.Add(std::hypot(Mxh, Myh)); Ce.MidBand.Add(std::hypot(Mxb, Myb));
					if (C > 1)
					{
						const double R1 = (B[C] - B[C - 1]) / std::max(1e-6, T[C] - T[C - 1]), R0 = (B[C - 1] - B[C - 2]) / std::max(1e-6, T[C - 1] - T[C - 2]);
						Ce.Jumps10 += std::fabs(R1 - R0) > 10.0 ? 1 : 0;
						Ce.Jumps30 += std::fabs(R1 - R0) > 30.0 ? 1 : 0;
					}
					Ce.Seconds += T[C] - T[C - 1];
					Ce.Keys.insert(Tr.Key);
					// (judged in her box only where all the 1.5 s about it are in it: one either side of her leaving it,
					// rolling for home or into her dive, is her roll out of it, not her box rocking. Taken with it, the
					// He 111s of the line-hand raid read 0.25 degrees rms at x4 in their box, nearly all of it in the 2 s
					// before they turned for home, and 0.013 without; the Dauntlesses over Midway on the game's clock 0.40,
					// as they rolled into their dives, and 0.027 without)
					bool bWhole = Tr.S[Begin + C].D == OnBox;
					for (size_t I = L15; I <= H15 && bWhole; ++I) bWhole = Tr.S[Begin + I].D == OnBox;
					if (bWhole)
					{
						Cell& Ib = InBox[{ Tr.Model, Pc }];
						Ib.BankBand.Add(Bb);
						Ib.TipBand.Add(Tb);
						if (C > 1)
						{
							const double R1 = (B[C] - B[C - 1]) / std::max(1e-6, T[C] - T[C - 1]), R0 = (B[C - 1] - B[C - 2]) / std::max(1e-6, T[C - 1] - T[C - 2]);
							Ib.Jumps10 += std::fabs(R1 - R0) > 10.0 ? 1 : 0;
							Ib.Jumps30 += std::fabs(R1 - R0) > 30.0 ? 1 : 0;
						}
						Ib.Seconds += T[C] - T[C - 1];
						Ib.Keys.insert(Tr.Key);
					}
				}
				Begin = End;
			}
		}

		std::printf("  the flying, by model, what it is about and the clock's speed. HI: what a 0.4 s parabola leaves; BAND: the 0.4 s\n");
		std::printf("  parabola less the 1.5 s one (about 0.5 to 3 Hz). rms/peak. The chase camera riding it: px on the 1440 picture.\n");
		std::printf("    %-10s %-17s %3s %6s | %-15s %-15s | %-8s %-8s | %-14s %-14s | %-11s | %-15s %-15s %-8s\n", "model", "about", "n", "sec", "place HI m", "place BAND m",
			"head BND", "pitch BD", "bank HI deg", "bank BAND deg", "roll jumps", "chase tip HI", "chase tip BAND", "mid BAND");
		for (auto& Pair : Cells)
		{
			Cell& C = Pair.second;
			if (C.Seconds < 2.0) continue;
			const std::string About = std::string(DoingName[std::get<1>(Pair.first)]) + " " + PaceName[std::get<2>(Pair.first)];
			std::printf("    %-10s %-17s %3zu %6.0f | %.4f/%-8.3f %.4f/%-8.3f | %-8.3f %-8.3f | %.3f/%-8.2f %.3f/%-8.2f | %5.1f %5.1f | %.3f/%-9.2f %.3f/%-9.2f %-8.3f\n",
				std::get<0>(Pair.first).c_str(), About.c_str(), C.Keys.size(), C.Seconds, C.PlaceHi.Rms(), C.PlaceHi.Peak, C.PlaceBand.Rms(), C.PlaceBand.Peak,
				C.HeadBand.Rms(), C.PitchBand.Rms(), C.BankHi.Rms(), C.BankHi.Peak, C.BankBand.Rms(), C.BankBand.Peak, C.PerMinute(C.Jumps10), C.PerMinute(C.Jumps30),
				C.TipHi.Rms(), C.TipHi.Peak, C.TipBand.Rms(), C.TipBand.Peak, C.MidBand.Rms());
		}
		std::printf("  each bomber type in its box: swings of bank over 1 deg (a bomber's air minute; middle, largest), banked over 5 deg, over 50 m\n");
		std::printf("  from her slot, the most banked in her place (3 air s after taking it; closing up apart); torpedo planes before their run\n");
		for (auto& Pair : Boxes)
		{
			BoxRow& R = Pair.second;
			if (R.Seconds < 1.0 && R.Most <= 0.0 && R.MostJoining <= 0.0) continue;
			std::vector<double> Sw = R.Swings;
			std::sort(Sw.begin(), Sw.end());
			std::printf("    %-10s %6.0f bomber air s: swings %.2f a bomber's air minute, middle %.1f, largest %.1f deg; banked over 5 deg %.1f%%, over 50 m off %.1f%%; most bank %.1f deg in her box, %.1f closing up",
				Pair.first.c_str(), R.Seconds, R.Seconds > 0.0 ? 60.0 * static_cast<double>(Sw.size()) / R.Seconds : 0.0, Sw.empty() ? 0.0 : Sw[Sw.size() / 2], Sw.empty() ? 0.0 : Sw.back(),
				R.Seconds > 0.0 ? 100.0 * R.Banked / R.Seconds : 0.0, R.GapSeconds > 0.0 ? 100.0 * R.Far / R.GapSeconds : 0.0, R.Most, R.MostJoining);
			if (!R.TorpedoGaps.empty())
			{
				std::vector<double> G = R.TorpedoGaps;
				std::sort(G.begin(), G.end());
				std::printf("; %zu torpedo runs, farthest from her slot in the 20 s before: middle %.0f, most %.0f m", G.size(), G[G.size() / 2], G.back());
			}
			std::printf("\n");
		}

		// The strategy cameras: each aircraft's place on the picture, less a parabola over 0.4 s and over 1.2 s.
		std::map<int, bool> Fighter;
		for (const auto& Pair : Tracks) Fighter[Pair.first] = Pair.second.bFighter;
		struct Picture { Stat B4, B12, F4, F12; double JumpsPerMinute = 0.0, HeldPerMinute = 0.0, Held = 0.0, Steps = 0.0, LagP95 = 0.0, LagP50 = 0.0, LagPxP95 = 0.0, Near = 0.0; long Jumps = 0, HeldJumps = 0, Frames = 0, NearFrames = 0; };
		std::vector<Picture> Pictures(Cams.size());
		for (size_t Ci = 0; Ci < Cams.size(); ++Ci)
		{
			Strategy& C = *Cams[Ci];
			Picture& Pc = Pictures[Ci];
			Pc.B4.bKeep = Pc.B12.bKeep = Pc.F4.bKeep = Pc.F12.bKeep = true;
			for (auto& Pair : C.Seen)
			{
				const bool bF = Fighter.count(Pair.first) ? Fighter[Pair.first] : true;
				const std::vector<std::array<double, 3>>& Ss = Pair.second;
				const std::vector<int>& Fr = C.SeenFrame[Pair.first];
				size_t Begin = 0;
				while (Begin < Ss.size())
				{
					size_t End = Begin + 1;
					while (End < Ss.size() && Fr[End] == Fr[End - 1] + 1) ++End;
					std::vector<double> T, X, Y, W;
					for (size_t I = Begin; I < End; ++I) { T.push_back(Ss[I][0]); X.push_back(Ss[I][1]); Y.push_back(Ss[I][2]); }
					for (size_t I = 0; I < T.size(); ++I)
					{
						size_t Lo = 0, Hi = 0;
						if (T[I] - T.front() >= 0.6 && T.back() - T[I] >= 0.6 && Weights(T, I, 0.6, Lo, Hi, W))
							(bF ? Pc.F12 : Pc.B12).Add(std::hypot(X[I] - Fit(X, Lo, Hi, W), Y[I] - Fit(Y, Lo, Hi, W)));
						if (T[I] - T.front() >= 0.2 && T.back() - T[I] >= 0.2 && Weights(T, I, 0.2, Lo, Hi, W))
							(bF ? Pc.F4 : Pc.B4).Add(std::hypot(X[I] - Fit(X, Lo, Hi, W), Y[I] - Fit(Y, Lo, Hi, W)));
					}
					Begin = End;
				}
			}
			// (the look's jumps: its step less its glide, the middle of its steps over 0.3 s either way, across the
			// picture at the eye's distance, events a frame or two apart counted once)
			V3 F, R, U;
			Axes(Rot{ -12.0, Yaw, 0.0 }, F, R, U);
			const double Fp = 720.0 / std::tan(Rad(27.5));
			double LastJump = -1e9, LastStep = -1e9;
			double Span0 = -1.0, Span1 = 0.0;
			std::vector<double> Lags, LagPx;
			for (size_t I = 1; I < C.T.size(); ++I)
			{
				if (C.T[I] < WindowAt || C.T[I - 1] < WindowAt) continue;
				++Pc.Frames;
				Pc.NearFrames += C.Lag[I] <= 100.0 ? 1 : 0;
				if (C.Settled[I])
				{
					Lags.push_back(C.Lag[I]);
					LagPx.push_back(C.Lag[I] / std::max(1.0, C.Away[I]) * Fp);
				}
				size_t Lo = I, Hi = I;
				while (Lo > 1 && C.T[I] - C.T[Lo - 1] <= 0.3) --Lo;
				while (Hi + 1 < C.T.size() && C.T[Hi + 1] - C.T[I] <= 0.3) ++Hi;
				std::vector<double> Vx, Vy, Vz, Va;
				for (size_t K = Lo; K <= Hi; ++K)
				{
					const double Dt = std::max(1e-9, C.T[K] - C.T[K - 1]);
					const V3 D = C.Looks[K] - C.Looks[K - 1];
					Vx.push_back(D.X / Dt); Vy.push_back(D.Y / Dt); Vz.push_back(D.Z / Dt); Va.push_back((C.Away[K] - C.Away[K - 1]) / Dt);
				}
				auto Mid = [](std::vector<double>& V) { std::nth_element(V.begin(), V.begin() + static_cast<long>(V.size() / 2), V.end()); return V[V.size() / 2]; };
				const double Dt = C.T[I] - C.T[I - 1];
				// (the eye's step in distance beyond its glide, as the look's)
				if (std::fabs(C.Away[I] - C.Away[I - 1] - Mid(Va) * Dt) > 0.02 * std::max(1.0, C.Away[I - 1]))
				{
					if (C.T[I] - LastStep > 0.04) Pc.Steps += 1.0;
					LastStep = C.T[I];
				}
				V3 E = (C.Looks[I] - C.Looks[I - 1]) - V3(Mid(Vx), Mid(Vy), Mid(Vz)) * Dt;
				E = E - F * E.Dot(F);
				const double Px = E.Size() / std::max(1.0, C.Away[I]) * Fp;
				if (Px >= 20.0)
				{
					if (C.T[I] - LastJump > 0.04)
					{
						++Pc.Jumps;
						Pc.HeldJumps += C.Settled[I] ? 1 : 0;
					}
					LastJump = C.T[I];
				}
				Pc.Held += C.Settled[I] ? Dt : 0.0;
				if (Span0 < 0.0) Span0 = C.T[I];
				Span1 = C.T[I];
			}
			const double Minutes = std::max(1e-9, (Span1 - Span0) / 60.0);
			Pc.JumpsPerMinute = static_cast<double>(Pc.Jumps) / Minutes;
			Pc.HeldPerMinute = Pc.Held > 0.0 ? 60.0 * static_cast<double>(Pc.HeldJumps) / Pc.Held : 0.0;
			Pc.Near = Pc.Frames > 0 ? static_cast<double>(Pc.NearFrames) / static_cast<double>(Pc.Frames) : 1.0;
			if (!Lags.empty())
			{
				std::sort(Lags.begin(), Lags.end());
				Pc.LagP95 = Lags[std::min(Lags.size() - 1, Lags.size() * 95 / 100)];
				Pc.LagP50 = Lags[Lags.size() / 2];
				std::sort(LagPx.begin(), LagPx.end());
				Pc.LagPxP95 = LagPx[std::min(LagPx.size() - 1, LagPx.size() * 95 / 100)];
			}
			std::printf("  the strategy camera, %s: bombers on its picture %.2f px rms, %.2f at the 99th percentile, %.1f at most (0.4 s), %.2f rms, %.2f at the 99th (1.2 s);\n",
				C.bSmooth ? "flying smooth (AirDesk::Overview)" : "as it was (the hard rings)", Pc.B4.Rms(), Pc.B4.Pct(0.99), Pc.B4.Peak, Pc.B12.Rms(), Pc.B12.Pct(0.99));
			std::printf("    fighters %.2f px rms, %.2f at the 99th (0.4 s), %.2f rms (1.2 s); look jumps of 20 px %.1f a real minute while it holds its crowd\n",
				Pc.F4.Rms(), Pc.F4.Pct(0.99), Pc.F12.Rms(), Pc.HeldPerMinute);
			std::printf("    (2 s and more after a change of its groups or its anchor, %.0f%% of the time), %.1f in all; eye steps over 2%% %.0f; the look %.0f m off\n",
				100.0 * Pc.Held / std::max(1e-9, Span1 - Span0), Pc.JumpsPerMinute, Pc.Steps, Pc.LagP50);
			std::printf("    what it is after in the middle, %.0f m at the 95th percentile (holding its crowd), %.0f px on the picture\n", Pc.LagP95, Pc.LagPxP95);
		}
		std::printf("  the strategy camera's distance for a crowd of 11.2, 22.6 and 31.6 m spans: %.1f, %.1f and %.1f m (as it was %.1f, %.1f and %.1f)\n",
			AirDesk::Overview::TypeAwayOf(11.2), AirDesk::Overview::TypeAwayOf(22.6), AirDesk::Overview::TypeAwayOf(31.6), OldTypeAway(11.2), OldTypeAway(22.6), OldTypeAway(31.6));
		if (bOld)
		{
			std::printf("  (flown as release 1.5 flew it, AIR_SMOOTH_OLD: reported, not judged)\n");
			return 0;
		}
		// (the guard: how much of the time the look is within 100 m of what it is after, all frames; the report as
		// release 1.5 flew it, above, is kept to the byte, so this is given flying smooth only)
		std::printf("  the strategy camera's look within 100 m of what it is after: %.1f%% of the frames flying smooth, %.1f%% as it was\n",
			100.0 * Pictures[1].Near, 100.0 * Pictures[0].Near);

		// ---- the judgement ----
		Verdict V;
		{
			Picture& Pc = Pictures[1];
			const Picture& Was = Pictures[0];
			// (at x10, or the game's own clock that runs at x10 to the meeting, 1.2 s is 12 air seconds of the crowd's own
			// turning and a fighter's dogfight, and a look eased over a real second trails more: there, no worse than the
			// camera as it was, and the crowd's middle kept within the middle half of the picture)
			const bool bFast = !(SpeedKind == "1" || SpeedKind == "4");
			// (at x1 and x4 the bombers on its picture 1.5 px rms at most, and 2 at the 99th percentile)
			constexpr double B4PctSlow = 2.0;
			const double B12Most = bFast ? std::max(5.0, Was.B12.Rms()) : 5.0, F4Most = bFast ? std::max(1.0, Was.F4.Rms()) : 1.0;
			const double B4Most = bFast ? 3.0 : 1.5, B4PctMost = bFast ? 10.0 : B4PctSlow, JumpsMost = bFast ? 2.0 : 1.0;
			V.Judge(Pc.B4.N > 0 && Pc.B4.Rms() <= B4Most && Pc.B4.Pct(0.99) <= B4PctMost && Pc.B12.Rms() <= B12Most,
				Say("the strategy camera's bombers steady: %.2f px rms, %.2f at the 99th percentile (0.4 s; at most %.1f and %.1f), %.2f rms (1.2 s; at most %.2f)", Pc.B4.Rms(), Pc.B4.Pct(0.99), B4Most, B4PctMost, Pc.B12.Rms(), B12Most));
			V.Judge(Pc.F4.Rms() <= F4Most, Say("the strategy camera's fighters steady: %.2f px rms (0.4 s; at most %.2f)", Pc.F4.Rms(), F4Most));
			V.Judge(Pc.HeldPerMinute <= JumpsMost && Pc.Steps == 0.0, Say("the strategy camera's look never jumps: %.2f jumps of 20 px a real minute holding its crowd (at most %.0f), %.0f eye steps over 2%% (none)", Pc.HeldPerMinute, JumpsMost, Pc.Steps));
			// (the crowd's middle kept on the picture, within its middle two thirds at x1 and x4: a look eased over a real
			// second trails a crowd whose middle wanders, as a busy one's does, 400 px and more at x10)
			const double LagMost = std::max(bFast ? 720.0 : 480.0, Was.LagPxP95);
			V.Judge(Pc.LagPxP95 <= LagMost, Say("the strategy camera keeps up: its look %.0f px off what it is after on the picture at the 95th percentile, %.0f m (at most %.0f px, or the old camera's)", Pc.LagPxP95, Pc.LagP95, LagMost));
			// (and at x10 its look within 100 m of what it is after in 95% of the frames, or as often as the old camera's:
			// over the set pieces what it is after itself jumps as the crowds part, and neither camera reaches 95%)
			if (bFast)
			{
				const double NearLeast = std::min(0.95, Was.Near);
				V.Judge(Pc.Near >= NearLeast, Say("the strategy camera's look within 100 m of what it is after in %.1f%% of the frames (at least %.1f%%, or the old camera's)", 100.0 * Pc.Near, 100.0 * NearLeast));
			}
		}
		for (auto& Pair : InBox)
		{
			const Cell& C = Pair.second;
			const int Pc = Pair.first.second;
			if (C.Seconds < 30.0 || (Pc != X1 && Pc != X4 && Pc != X10)) continue;
			const std::string& M = Pair.first.first;
			const bool bTwin = Boxes[M].bTwin;
			// (a twin's wing tip on the chase camera's picture no more than 0.2 px rms at x1 and 1 at x4, her bank 0.2 and
			// 0.25 degrees; a single-engined type, a dive or torpedo bomber, rolls quicker in her place: her bank 0.2 and
			// 1.2 degrees, her tip 1 and 6 px)
			const double BankMost = Pc == X1 ? 0.2 : Pc == X4 ? (bTwin ? 0.25 : 1.2) : (bTwin ? 0.6 : 2.0);
			const double TipMost = Pc == X1 ? (bTwin ? 0.2 : 1.0) : Pc == X4 ? (bTwin ? 1.0 : 6.0) : (bTwin ? 2.5 : 12.0);
			V.Judge(C.BankBand.Rms() <= BankMost && C.TipBand.Rms() <= TipMost,
				Say("%s in her box at %s rocks no more than a chase camera can bear: bank BAND %.3f deg rms (at most %.2f), wing tip BAND %.2f px rms (at most %.1f)", M.c_str(), PaceName[Pc], C.BankBand.Rms(), BankMost, C.TipBand.Rms(), TipMost));
			if (Pc == X10) V.Judge(C.PerMinute(C.Jumps30) <= (bTwin ? 3.0 : 8.0), Say("%s in her box at x10: %.1f roll-rate jumps over 30 deg/s a minute (at most %.0f)", M.c_str(), C.PerMinute(C.Jumps30), bTwin ? 3.0 : 8.0));
			else V.Judge(C.PerMinute(C.Jumps10) <= 1.0, Say("%s in her box at %s: %.1f roll-rate jumps over 10 deg/s a minute (at most 1)", M.c_str(), PaceName[Pc], C.PerMinute(C.Jumps10)));
		}
		constexpr double SwingsMost = 0.3;
		for (auto& Pair : Boxes)
		{
			BoxRow& R = Pair.second;
			if (R.Seconds <= 0.0 && R.Most <= 0.0) continue;
			V.Judge(R.Most <= Show.RallyBankMost + 1.0 && R.MostJoining <= Show.RallyBankMost + 1.0,
				Say("%s banks no more than %.0f deg in her place (and a degree): %.1f in her box, %.1f closing up", Pair.first.c_str(), Show.RallyBankMost, R.Most, R.MostJoining));
			if (!R.TorpedoGaps.empty())
			{
				const double Far = *std::max_element(R.TorpedoGaps.begin(), R.TorpedoGaps.end());
				V.Judge(Far <= 100.0, Say("%s in her place for her torpedo run: %.0f m from her slot at the farthest in the 20 air s before it (at most 100)", Pair.first.c_str(), Far));
			}
			if (R.Seconds < 60.0) continue;
			std::vector<double> Sw = R.Swings;
			std::sort(Sw.begin(), Sw.end());
			// (every swing of a degree and more counts: the weave was of 20 to 40, once a bomber's air minute; a box closing
			// up on its losses swings each of its bombers a degree or two, the Vals over Oahu a little over once in five
			// air minutes)
			const double Rate = 60.0 * static_cast<double>(Sw.size()) / R.Seconds;
			V.Judge(Rate <= SwingsMost && R.Banked / R.Seconds <= 0.01,
				Say("%s holds her place without weaving: %.2f swings over 1 deg a bomber's air minute (at most %.1f; the middle %.1f and the largest %.1f), banked over 5 deg %.1f%% (at most 1%%)",
					Pair.first.c_str(), Rate, SwingsMost, Sw.empty() ? 0.0 : Sw[Sw.size() / 2], Sw.empty() ? 0.0 : Sw.back(), 100.0 * R.Banked / R.Seconds));
		}
		std::printf("  %s: the bombers and the strategy camera steady as a Mac plays them\n", V.bFailed ? "FAIL" : "pass");
		return V.bFailed ? 1 : 0;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc > 1 && std::string(Argv[1]) == "--shake") return Shake::Play(Argc - 1, Argv + 1);
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
#ifndef AIR_PROBE_NO_BOX_ROW
	Show.SetBoxKeep(BoxRowOn());
#endif
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
