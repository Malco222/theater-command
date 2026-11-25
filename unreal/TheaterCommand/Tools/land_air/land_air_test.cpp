// The ground-attack fighters' cloud test (Effects, release 2, behind -TheaterLandAirSupport; Battle/LandAir and the
// feed's air notes, LandFeed::AirNote). No recording made yet carries the fighters on call, so it writes one of its own
// in the rules' words (src/battle/land/sim.ts: land:air-in, land:strafe, land:bomb-drop, air:lost), three strikes over a
// flat field: American Mustangs in 1944 (four, one lost on fire), German Stukas in 1942 (four, one lost in a spin) and
// Russian Il-2s in 1943 (two legs of two, one blown up). Checks:
//   feed     every note read, with its flight, run, models, strafe, bombs and loss
//   machine  each air force's machine by year
//   flight   no plane before its leg comes; each flown at its machine's speed, never into the ground, its heading and
//            attitude smooth; a Stuka dives steeply, an Il-2 stays low
//   guns     each plane's guns walk its lane of the strip from start to end, on the ground; the leg rakes the strip for
//            about the rules' time
//   losses   as many planes hit as the rules lost, each hit once and into the ground once after; a burning one in a long
//            dive, a spinning one steep, one blown up gone at once; a wreck where each struck; no other plane falls
//   ground   only the other side's blocks fire, at a plane in reach, their streams ending in the air near it
//   same     built twice, or grown note by note as a live battle grows, the same planes; rounds the same however the
//            frames fall; no NaN; the planes' hash printed against release 1's
//   chutes   the downed pilots (-TheaterLandChutes): out of the right planes, down, up and away; and the art director's
//            notes of 24 November (02:26Z) on them, on this field's pilots and a second, windy field's (5.7 m/s; its
//            Il-2 loses her pilot under 60 m): falling free, filling, the wind, the swap, the harness and the drag, the
//            hard landing and the limp, the swing, the canopy's shape, seams, silk and glow; and a review's of the same
//            day: the scene's mesh made and moved in place at 45 frames a second, the swap's frames, his lying pose
//            against every soldier file's go_prone (LAND_AIR_SOLDIERS, which run.sh sets), the blends, his lines; her
//            verdict of 24 November (14:34Z): his canopy's hem pulled in toward its lines side-on, and down it lies
//            stretched along the wind in uneven folds of its own, its far end lifted, its near end by his harness,
//            falling in downwind and never under the ground on a slope; and her note of 17:40Z, from above (FromAbove):
//            its outline uneven, one side wider, its hem wavy, three or four folds across it, its far end's lift slumped
//            to one side, a few of its lines across its silk, its luma across its middle about her 126 to 221; and on a
//            second look, a few of its lines lying on top across its body, and its far end broken up (its vent off its
//            middle, a crease across its dome, folded back under at an edge, its seams kinked); none of his lines under
//            the ground on eleven fields, drawn from his harness where the scene draws it, over the verdict's four and
//            300 pilots' numbers (a second review); and the crowd's man he
//            becomes (LandMen's Crowd::PilotDress, AddRider; Effects, R2): a USA or GER pilot in the tankers' file as it
//            is, a SOV or UK pilot (an army without one) in its infantry file bareheaded and without his pack (the art
//            director: "no steel helmet, no rifle, no pack"), at every level his file's triangles less the helmet's and
//            pack's runs (pack_tris; a file without them keeps his pack, as before)
// With an out folder it writes air_paths.txt for paths.py (the picture: each strike side-on and from above), and the
// pilots' frames and chute_silk.txt (his canopy's mesh: full, lying, two pilots', and falling in) for chutes.py.
// Usage: Tools/land_air/run.sh [out folder]
#include "Battle/LandAir.h"
#include "Battle/LandJson.h"
#include "Battle/LandMen.h"
#include "pack_cut.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using namespace LandAir;

namespace
{
	int Failed = 0;
	void Check(bool bOk, const std::string& What)
	{
		std::printf("%s: %s\n", bOk ? "ok" : "FAIL", What.c_str());
		Failed += bOk ? 0 : 1;
	}

	struct Strike
	{
		int Flight;
		const char* Side;
		const char* Nat;
		int Year, Models;
		double Run[4];
		double T;
		const char* Lost;   // "" none
	};

	const Strike Strikes[] = {
		{ 1, "att", "USA", 1944, 4, { 700.0, 760.0, 1100.0, 760.0 }, 60.0, "burn" },
		{ 2, "def", "GER", 1942, 4, { 1050.0, 320.0, 850.0, 320.0 }, 90.0, "spin" },
		{ 3, "att", "SOV", 1943, 2, { 600.0, 700.0, 600.0, 900.0 }, 120.0, "explode" },
		{ 4, "att", "SOV", 1943, 2, { 1000.0, 680.0, 1300.0, 860.0 }, 120.0, "" },
	};

	/** The chutes' second field (the art director's notes, 24 November 02:26Z): an Il-2 spinning in low, its pilot out
	 * under 60 m, and a Mustang burning, in a wind of over 4 m/s (sky.wind, 5.73 m/s east-south-east). */
	const Strike Windy[] = {
		{ 5, "att", "SOV", 1943, 2, { 600.0, 700.0, 600.0, 900.0 }, 60.0, "spin" },
		{ 6, "att", "USA", 1944, 4, { 700.0, 760.0, 1100.0, 760.0 }, 90.0, "burn" },
	};
	constexpr double WindyX = 5.2, WindyY = -2.4;

	/** The recording: a 2 km field, the attacker's blocks to the south, the defender's to the north, and the strikes
	 * (Strikes; or List's Count, with Sky's words at its root). */
	std::string Recording(bool bAll, size_t Upto = 100, bool bOwnOnStrip = false, const Strike* List = Strikes, size_t Count = 4, const char* Sky = "")
	{
		std::ostringstream R;
		R << "{\"key\": \"air-test\", \"date\": \"July 1944\", " << Sky << "\"field\": {\"w\": 2000, \"h\": 2000}, \"men\": [], \"views\": [";
		for (int V = 0; V < 2; ++V)
		{
			R << (V ? "," : "") << "{\"t\": " << (V ? 100 : 0) << ", \"units\": [";
			// (bOwnOnStrip: the attacker's first block stood on the Mustangs' strip, 60 m along it and 20 m aside)
			const double Att[][2] = { { bOwnOnStrip ? 760.0 : 700.0, bOwnOnStrip ? 740.0 : 300.0 }, { 900, 330 }, { 1100, 300 }, { 1500, 250 } };
			const double Def[][2] = { { 650, 780 }, { 900, 760 }, { 1150, 790 }, { 600, 1700 } };
			for (int K = 0; K < 4; ++K) R << (K ? "," : "") << "{\"id\": " << K + 1 << ", \"side\": \"att\", \"x\": " << Att[K][0] << ", \"y\": " << Att[K][1] << ", \"str\": 0.8}";
			for (int K = 0; K < 4; ++K) R << ",{\"id\": " << K + 11 << ", \"side\": \"def\", \"x\": " << Def[K][0] << ", \"y\": " << Def[K][1] << ", \"str\": " << (K == 3 ? 0.0 : 0.7) << "}";
			R << "]}";
		}
		R << "], \"events\": [";
		size_t N = 0;
		bool bFirst = true;
		auto Add = [&](const std::string& E) { if (N++ >= Upto) return; R << (bFirst ? "" : ",") << E; bFirst = false; };
		for (size_t Si = 0; Si < Count; ++Si)
		{
			const Strike& S = List[Si];
			const double Len = std::hypot(S.Run[2] - S.Run[0], S.Run[3] - S.Run[1]), Secs = Len / 120.0;
			std::ostringstream In, Sf, Lo;
			In << "{\"t\": " << S.T << ", \"what\": \"land:air-in\", \"side\": \"" << S.Side << "\", \"text\": \"\", \"x\": " << S.Run[0] << ", \"y\": " << S.Run[1]
			   << ", \"extra\": {\"flight\": " << S.Flight << ", \"run\": [" << S.Run[0] << ", " << S.Run[1] << ", " << S.Run[2] << ", " << S.Run[3] << "], \"models\": " << S.Models
			   << ", \"alt\": 60, \"nat\": \"" << S.Nat << "\", \"year\": " << S.Year << ", \"heading\": " << std::atan2(S.Run[2] - S.Run[0], S.Run[3] - S.Run[1]) << "}, \"cue\": \"fighter_pass\"}";
			Add(In.str());
			Sf << "{\"t\": " << S.T << ", \"what\": \"land:strafe\", \"side\": \"" << S.Side << "\", \"text\": \"\", \"x\": 0, \"y\": 0, \"extra\": {\"flight\": " << S.Flight
			   << ", \"from\": [" << S.Run[0] << ", " << S.Run[1] << "], \"to\": [" << S.Run[2] << ", " << S.Run[3] << "], \"width\": 60, \"secs\": " << Secs << "}, \"cue\": \"machine_gun\"}";
			Add(Sf.str());
			if (bAll && S.Year < 1944)
			{
				std::ostringstream Bd;
				Bd << "{\"t\": " << S.T + Secs * 0.6 << ", \"what\": \"land:bomb-drop\", \"side\": \"" << S.Side << "\", \"text\": \"\", \"x\": 0, \"y\": 0, \"extra\": {\"flight\": " << S.Flight
				   << ", \"from\": {\"x\": 0, \"y\": 0, \"alt\": 60}, \"to\": {\"x\": " << S.Run[0] + (S.Run[2] - S.Run[0]) * 0.8 << ", \"y\": " << S.Run[1] + (S.Run[3] - S.Run[1]) * 0.8 << "}, \"at\": " << S.T + Secs * 0.8 << "}}";
				Add(Bd.str());
			}
			if (*S.Lost)
			{
				Lo << "{\"t\": " << S.T + Secs + 0.1 << ", \"what\": \"air:lost\", \"side\": \"" << S.Side << "\", \"text\": \"\", \"x\": " << S.Run[2] << ", \"y\": " << S.Run[3]
				   << ", \"extra\": {\"flight\": " << S.Flight << ", \"kind\": \"" << S.Lost << "\", \"chutes\": " << (std::strcmp(S.Lost, "explode") ? 1 : 0) << ", \"over\": \"enemy\", \"air\": \"ftr\", \"alt\": 60}, \"cue\": \"plane_down\"}";
				Add(Lo.str());
			}
		}
		R << "], \"rounds\": []}";
		return R.str();
	}

	double Flat(double, double) { return 0.0; }

	uint64_t Hash(const std::vector<Plane>& Ps, const std::vector<Gunner>& Gs, uint64_t H)
	{
		auto Mix = [&H](double V) { uint64_t U = 0; std::memcpy(&U, &V, sizeof(U)); H = (H ^ U) * 1099511628211ull; };
		for (const Plane& P : Ps) { Mix(P.Key); Mix(P.At.X); Mix(P.At.Y); Mix(P.At.Z); Mix(P.Heading); Mix(P.Pitch); Mix(P.Bank); Mix(P.bFiring); }
		for (const Gunner& G : Gs) { Mix(G.Key); Mix(G.To.X); Mix(G.To.Y); Mix(G.To.Z); }
		return H;
	}

	V3 Plus(const V3& A, const V3& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
	V3 Mul(const V3& A, double K) { return { A.X * K, A.Y * K, A.Z * K }; }
	double Length(const V3& A) { return std::sqrt(A.X * A.X + A.Y * A.Y + A.Z * A.Z); }
	V3 Unit3(const V3& A) { const double L = Length(A); return L > 1e-12 ? Mul(A, 1.0 / L) : A; }

	double Turn(double A, double B)
	{
		double D = std::fmod(B - A, 360.0);
		if (D > 180.0) D -= 360.0;
		if (D < -180.0) D += 360.0;
		return std::fabs(D);
	}

	/** How many times a run of values turns back by By or more: its peaks and its dips (bPeaks: its peaks alone, each a
	 * rise of By or more and then a fall of By or more). */
	int Swings(const std::vector<double>& V, double By, bool bPeaks = false)
	{
		int Dir = 0, Count = 0;
		double Ext = V.empty() ? 0.0 : V[0], Lo = Ext, Hi = Ext;
		for (const double X : V)
		{
			if (Dir == 0)
			{
				Lo = std::min(Lo, X);
				Hi = std::max(Hi, X);
				if (X - Lo >= By) { Dir = 1; Ext = X; }
				else if (Hi - X >= By) { Dir = -1; Ext = X; }
			}
			else if (Dir > 0)
			{
				Ext = std::max(Ext, X);
				if (Ext - X >= By) { ++Count; Dir = -1; Ext = X; }
			}
			else
			{
				Ext = std::min(Ext, X);
				if (X - Ext >= By) { Count += bPeaks ? 0 : 1; Dir = 1; Ext = X; }
			}
		}
		return Count;
	}

	/** A smooth line through a run of values: at each, a straight line's least squares over Win values either way
	 * (tricube weighted). */
	std::vector<double> SmoothFit(const std::vector<double>& V, int Win)
	{
		const int N = static_cast<int>(V.size());
		std::vector<double> Fit(V.size(), 0.0);
		for (int K = 0; K < N; ++K)
		{
			double Sw = 0.0, Sx = 0.0, Sy = 0.0, Sxx = 0.0, Sxy = 0.0;
			for (int J = std::max(0, K - Win); J <= std::min(N - 1, K + Win); ++J)
			{
				const double X = static_cast<double>(J - K) / (Win + 1);
				const double W = std::pow(1.0 - std::pow(std::fabs(X), 3.0), 3.0), Y = V[static_cast<size_t>(J)];
				Sw += W;
				Sx += W * X;
				Sy += W * Y;
				Sxx += W * X * X;
				Sxy += W * X * Y;
			}
			const double Det = Sw * Sxx - Sx * Sx;
			Fit[static_cast<size_t>(K)] = Det > 1e-12 ? (Sy * Sxx - Sx * Sxy) / Det : Sy / std::max(1e-12, Sw);
		}
		return Fit;
	}

	/** A run of values each the mean of itself and Half on either side. */
	std::vector<double> Softened(const std::vector<double>& V, size_t Half)
	{
		std::vector<double> Out(V.size(), 0.0);
		for (size_t K = 0; K < V.size(); ++K)
		{
			double S = 0.0;
			int C = 0;
			for (size_t J = K >= Half ? K - Half : 0; J <= std::min(V.size() - 1, K + Half); ++J) { S += V[J]; ++C; }
			Out[K] = S / C;
		}
		return Out;
	}

	/**
	 * A lying canopy from above (the art director's note of 24 November 17:40Z: from above both pilots' silk read as "a
	 * smooth cocoon or a gourd": an even outline, only lengthwise ridges). Its silk's faces turned up, drawn straight
	 * down onto a grid of 2 cm cells, each cell keeping its topmost face's height and its sRGB luma as chutes.py shades
	 * it from above (its sun high on the near side, no glow; each cell lit by its facing there, its face's corners'
	 * between them, as the scene lights each pixel). Lengths are shares of its length from its near end (his). Its wind
	 * line: straight along the wind (as it lies, along its length) through where its lines gather (its hem's seams'
	 * middle).
	 * (a) Its outline's asymmetry, from a tenth of its length to nine tenths: how much more of its silk lies on one side
	 *     of its wind line than on the other, a share of all of it there ((right - left) over right + left, its sides'
	 *     widths from that line summed along it); and the same about the straight line from where its lines gather to
	 *     its tip (its far end's crown), so that a canopy no wider one side than the other but lying aslant is not taken
	 *     for one (a review of 24 November: measured about its own outline's middle, which takes a wider side in as
	 *     much as it widens it, a canopy wider one side had read as even, and about its under layer's middle seam, which
	 *     no one sees).
	 * (b) Its edges' waviness: each side's outline from a tenth of its length to nine tenths, how far off a smooth line
	 *     through it (a local straight line's least squares over a metre either way, so that its gores folded at its
	 *     edges count as well as its jags: the root mean square), and how many times it turns back by 5 cm or more; and
	 *     its near edge (its hem), across the span of its line's seams and 10 cm on (as far as 0.3 m past where its
	 *     lines gather), how far off a smooth curve through it (the least squares parabola: its round) from its most
	 *     forward to its most back (metres: a gore flipped back toward him or tucked under shows as a bite or a step)
	 *     and how many times it turns back by 4 cm or more.
	 * (c) Its folds across it: its top's mean height over the middle 80% of its width, from 0.08 of its length to 0.8
	 *     (short of the air its far end holds), off a smooth line through it (over 1 m either way), its rises of 2 cm
	 *     or more over the dips on either side; and its luma over the middle half of its width (over 10 cm along), its
	 *     light bands 8 or more over the dark on either side. And its luma across its middle (chutes.py's reading:
	 *     its middle column, over 14 cm; within 2 of chutes.py's 3 cm pixels for the verdict's two from above), the
	 *     darkest and lightest.
	 * (d) Its far end's slump sideways: over its lifted far end (0.72 to 0.95 of its length) the middle of its lift
	 *     across it (each cell's height over the lowest there its weight: not its highest cell, which a long fold's
	 *     ridge takes as often as not), how far to one side of its outline's middle on the mean (metres).
	 * (e) Its lines as the scene draws them lying (LandBattleAir's DownShape: LineLies' points, slack, from his harness as
	 *     he lies): how many run 0.2 m or more over its silk and on it (their middle no more than 4 mm under the silk's top
	 *     there: seen, the 12 mm cord's top over it), the most that one does, and how far under the silk's top any line goes
	 *     at the deepest where it runs over it (a review of 24 November: a line pressed under a flipped gore had been counted
	 *     across it), short of the last 10 cm of each running to its seam's point on its hem (the hem is under its silk's
	 *     top there), all of each laid across it. And (her note of 17:40Z, (3), after a second look: they had crossed only
	 *     the flap at its near end) across its body: each line's length on its silk more than 0.15 of its length past where
	 *     its lines gather; how many run 0.3 m or more so, and the most that one does. And of those it lies across (LineLies'
	 *     longer; a review: they had run from where the rest gather, along it more than across, 3.6 to 4.1 m of each on it,
	 *     and floated over its folds): each one's length on its silk; how slanted it lies, from where it comes onto its silk
	 *     to where it leaves it, off its wind line (degrees); how far along it it reaches (a share of its length past where
	 *     its lines gather); how far it comes onto its silk from his harness, straight, and how near where its lines
	 *     gather it passes on its way there (the plain distance); how high its middle stands over its silk's top where it
	 *     runs over it at the most and on the mean, and the share of its length on it where that is over 4 cm (lying on
	 *     it: the 12 mm cord within about 2 cm of it); and the longest drawn, from his harness, as a share of a real line's
	 *     length (some 1.33 times its spread across: LandAir's lines across it are no longer).
	 * (f) Its far end's breakup (her note of 17:40Z, (2), after a second look: its far end had stayed a neat dome, its
	 *     seams meeting at its middle): its vent (its tip, where its seams meet) off the middle of its far fifth's silk,
	 *     a share of its half-width there at the most; a crease across its dome, the deepest its top dips along it under
	 *     the straight between the highest it stands either side (its upper hull: a review found a crease measured under
	 *     the lower of those had to cut 0.35 m into the dome's fall to its tip to count, a notch at eye level), row by row
	 *     across it 16 cm or more in from its edges from 0.72 of its length on (metres); its last two rings folded back
	 *     under at an edge, how much farther out to one side of its middle its top layer reaches than its edge's seam does
	 *     (metres, at the most); and its seams over its top's middle across, how far each bends aside ring to ring over its
	 *     last four rings (the most second difference across it, metres, on the mean over them).
	 */
	struct Above
	{
		double Side = 0.0, SideTip = 0.0, Off[2] = {}, Hem = 0.0, Slump = 0.0, LumaLo = 0.0, LumaHi = 0.0, CrossMost = 0.0, Buried = 0.0;
		int Turns[2] = {}, NearTurns = 0, Folds = 0, Bands = 0, Lines = 0, Crossing = 0;
		double BodyMost = 0.0, FloatMost = 0.0, FloatMean = 0.0, FloatHigh = 0.0, LongShare = 0.0, OnLeast = 1e9, OnMost = 0.0, SlantLeast = 90.0, ReachMost = 0.0, GrassMost = 0.0;
		double GatherNear = 1e9;
		int BodyLines = 0, Strewn = 0, Slanted = 0;    // (Slanted: how many of those laid across it SlantLeast was measured on)
		double Vent = 0.0, Crease = 0.0, Under = 0.0, Kinks = 0.0;
		/** Its lines across its body off her note (3) and its reviews: none, or a tangle (over four); the longest across
		 * its body under 0.3 m or over 2.5; one laid across it with under 1 m or over 3 m of it on its silk (her 1 to
		 * 2.5 m: LandAir lays each to 2.45 m at most, as its silk lies before the ground and measured as these are, but
		 * drawn over its folds and its edges as it lies one runs on up to half a metre more), under 18 degrees off its wind
		 * line (a gore's seam, not a line across; 35 on the mean over many), reaching past half its length, passing within
		 * 0.1 m of where its lines gather on its way to its silk (where all the rest run: a review, of lines run on from
		 * there over it) or coming onto it farther than his lines' length in the air from his harness (LinesLong); floating
		 * over its silk (its middle more than 4 cm over its top for more than an eighth of its length on it, 25 cm
		 * anywhere, or 2.6 cm on the mean: the 12 mm cord within about 2 cm of it); or drawn longer than a real line. */
		bool BodyOff() const
		{
			return BodyLines < 1 || BodyLines > 4 || BodyMost < 0.3 || BodyMost > 2.5 || Strewn < 1 || OnLeast < 1.0 || OnMost > 3.0 || SlantLeast < 18.0 || ReachMost > 0.5
				|| GatherNear < 0.1 || GrassMost > LinesLong || FloatHigh > 0.12 || FloatMost > 0.25 || FloatMean > 0.026 || LongShare > 1.0;
		}
		/** Its far end a neat dome, off her note (2) on a second look: not folded back under 0.1 m at an edge, no crease
		 * 5 cm deep across it, and its vent within 0.15 of its half-width of its middle with its seams bending under 0.1 m
		 * ring to ring. */
		bool FarOff() const { return Under < 0.1 || Crease < 0.05 || (Vent < 0.15 && Kinks < 0.1); }
		/** Any canopy off her note: its outline next to even (under 0.05 more on one side of its wind line, or of the line
		 * to its tip); its edges next to smooth (both within 2 cm of a smooth line); its hem next to smooth (within 0.2 m
		 * of a smooth curve) or not turning back; its far end's lift under 0.15 m to one side, or toppled (0.45 m); none
		 * of its lines across its silk, or a tangle, or any line's middle under its silk's top by 8 mm (its 12 mm cord's
		 * top no longer over it); its luma across its middle more than about 10 off her 126 to 221. (Its folds across it
		 * and its light bands are counted over the many: a fold across part of its width, or running into its far end's
		 * rise, may go uncounted.) */
		bool Bad() const
		{
			return Side < 0.05 || SideTip < 0.05 || std::max(Off[0], Off[1]) < 0.02 || NearTurns < 1 || Hem < 0.2 || Slump < 0.15 || Slump > 0.45 || Crossing < 1 || Crossing > 6
				|| Buried > 0.008 || LumaLo < 115.0 || LumaHi > 232.0;
		}
	};
	Above FromAbove(const Chute& Dn, const SilkMesh& M)
	{
		Above A;
		constexpr double Cell = 0.02;
		const size_t Half = M.Points.size() / 2;
		double MinX = 1e9, MaxX = -1e9, MinY = 1e9, MaxY = -1e9;
		for (size_t K = 0; K < Half; ++K)
		{
			MinX = std::min(MinX, M.Points[K].X);
			MaxX = std::max(MaxX, M.Points[K].X);
			MinY = std::min(MinY, M.Points[K].Y);
			MaxY = std::max(MaxY, M.Points[K].Y);
		}
		const double X0 = MinX - 0.1, Y0 = MinY - 0.1;
		const int Nx = static_cast<int>((MaxX - X0 + 0.1) / Cell) + 2, Ny = static_cast<int>((MaxY - Y0 + 0.1) / Cell) + 2;
		const auto At = [Nx](int I, int J) { return static_cast<size_t>(J) * static_cast<size_t>(Nx) + static_cast<size_t>(I); };
		const auto Xof = [X0](int I) { return X0 + (I + 0.5) * Cell; };
		const auto Yof = [Y0](int J) { return Y0 + (J + 0.5) * Cell; };
		std::vector<double> Top(static_cast<size_t>(Nx) * static_cast<size_t>(Ny), -1e9), Luma(Top.size(), 0.0);
		// (and each cell's triangles facing up, for the top of its silk at a point itself)
		std::vector<size_t> UpTris, UpStarts(Top.size() + 1, 0), UpItems;
		std::vector<int> UpBox;
		const double SunLong = std::sqrt(0.5 * 0.5 + 0.6 * 0.6 + 0.62 * 0.62);
		const V3 Sun{ 0.5 / SunLong, -0.6 / SunLong, 0.62 / SunLong };
		for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
		{
			const size_t Ia = static_cast<size_t>(M.Triangles[T]), Ib = static_cast<size_t>(M.Triangles[T + 1]), Ic = static_cast<size_t>(M.Triangles[T + 2]);
			V3 N = Plus(Plus(M.Facings[Ia], M.Facings[Ib]), M.Facings[Ic]);
			const double Nl = Length(N);
			if (Nl < 1e-12) continue;
			N = Mul(N, 1.0 / Nl);
			if (N.Z <= 0.0) continue;
			const V3& Pa = M.Points[Ia];
			const V3& Pb = M.Points[Ib];
			const V3& Pc = M.Points[Ic];
			const double D = (Pb.X - Pa.X) * (Pc.Y - Pa.Y) - (Pc.X - Pa.X) * (Pb.Y - Pa.Y);
			if (std::fabs(D) < 1e-12) continue;
			const int I0 = std::max(0, static_cast<int>(std::floor((std::min({ Pa.X, Pb.X, Pc.X }) - X0) / Cell)));
			const int I1 = std::min(Nx - 1, static_cast<int>(std::ceil((std::max({ Pa.X, Pb.X, Pc.X }) - X0) / Cell)));
			const int J0 = std::max(0, static_cast<int>(std::floor((std::min({ Pa.Y, Pb.Y, Pc.Y }) - Y0) / Cell)));
			const int J1 = std::min(Ny - 1, static_cast<int>(std::ceil((std::max({ Pa.Y, Pb.Y, Pc.Y }) - Y0) / Cell)));
			UpTris.push_back(T);
			UpBox.insert(UpBox.end(), { I0, I1, J0, J1 });
			for (int J = J0; J <= J1; ++J)
				for (int I = I0; I <= I1; ++I) ++UpStarts[At(I, J) + 1];
			for (int J = J0; J <= J1; ++J)
			{
				const double Y = Yof(J);
				for (int I = I0; I <= I1; ++I)
				{
					const double X = Xof(I);
					const double Wb = ((X - Pa.X) * (Pc.Y - Pa.Y) - (Pc.X - Pa.X) * (Y - Pa.Y)) / D;
					const double Wc = ((Pb.X - Pa.X) * (Y - Pa.Y) - (X - Pa.X) * (Pb.Y - Pa.Y)) / D;
					if (Wb < -1e-9 || Wc < -1e-9 || Wb + Wc > 1.0 + 1e-9) continue;
					const double Z = Pa.Z + (Pb.Z - Pa.Z) * Wb + (Pc.Z - Pa.Z) * Wc;
					if (Z <= Top[At(I, J)]) continue;
					// (its shade as chutes.py's and the scene's: its facing there, its corners' between them; 0.22 and 0.78
					// of its light, its colour there, into sRGB, and its luma)
					const double Wa = 1.0 - Wb - Wc;
					const V3 Here = Plus(Plus(Mul(Unit3(M.Facings[Ia]), Wa), Mul(Unit3(M.Facings[Ib]), Wb)), Mul(Unit3(M.Facings[Ic]), Wc));
					const double Lit = (Here.X * Sun.X + Here.Y * Sun.Y + Here.Z * Sun.Z) / std::max(1e-12, Length(Here));
					const double Shade = 0.22 + 0.78 * std::max(0.0, Lit);
					const double Weights[3] = { 0.2126, 0.7152, 0.0722 };
					double Lm = 0.0;
					for (size_t Ch = 0; Ch < 3; ++Ch)
					{
						const double Lin = (M.Colours[Ia * 3 + Ch] * Wa + M.Colours[Ib * 3 + Ch] * Wb + M.Colours[Ic * 3 + Ch] * Wc) * Shade;
						Lm += Weights[Ch] * std::floor(255.0 * std::pow(std::clamp(Lin, 0.0, 1.0), 1.0 / 2.2));
					}
					Top[At(I, J)] = Z;
					Luma[At(I, J)] = Lm;
				}
			}
		}
		for (size_t K = 1; K < UpStarts.size(); ++K) UpStarts[K] += UpStarts[K - 1];
		UpItems.resize(UpStarts.back());
		{
			std::vector<size_t> Next(UpStarts.begin(), UpStarts.end() - 1);
			for (size_t K = 0; K < UpTris.size(); ++K)
				for (int J = UpBox[4 * K + 2]; J <= UpBox[4 * K + 3]; ++J)
					for (int I = UpBox[4 * K]; I <= UpBox[4 * K + 1]; ++I) UpItems[Next[At(I, J)]++] = UpTris[K];
		}
		// (the top of its silk at a point itself, not the middle of its cell (by a fold's flank or its edge that stands
		// up to its slope times 1.4 cm off); none, -1e9)
		const auto TopAt = [&](double X, double Y)
		{
			const int I = static_cast<int>(std::floor((X - X0) / Cell)), J = static_cast<int>(std::floor((Y - Y0) / Cell));
			if (I < 0 || J < 0 || I >= Nx || J >= Ny) return -1e9;
			double Best = -1e9;
			for (size_t Q = UpStarts[At(I, J)]; Q < UpStarts[At(I, J) + 1]; ++Q)
			{
				const size_t T = UpItems[Q];
				const V3& Pa = M.Points[static_cast<size_t>(M.Triangles[T])];
				const V3& Pb = M.Points[static_cast<size_t>(M.Triangles[T + 1])];
				const V3& Pc = M.Points[static_cast<size_t>(M.Triangles[T + 2])];
				const double D = (Pb.X - Pa.X) * (Pc.Y - Pa.Y) - (Pc.X - Pa.X) * (Pb.Y - Pa.Y);
				const double Wb = ((X - Pa.X) * (Pc.Y - Pa.Y) - (Pc.X - Pa.X) * (Y - Pa.Y)) / D;
				const double Wc = ((Pb.X - Pa.X) * (Y - Pa.Y) - (X - Pa.X) * (Pb.Y - Pa.Y)) / D;
				if (Wb < -1e-9 || Wc < -1e-9 || Wb + Wc > 1.0 + 1e-9) continue;
				Best = std::max(Best, Pa.Z + (Pb.Z - Pa.Z) * Wb + (Pc.Z - Pa.Z) * Wc);
			}
			return Best;
		};
		// (each column along it: where its silk is across it, its first cell and its last)
		std::vector<int> Lo(static_cast<size_t>(Nx), -1), Hi(static_cast<size_t>(Nx), -1);
		int First = -1, Last = -1;
		for (int I = 0; I < Nx; ++I)
		{
			for (int J = 0; J < Ny; ++J)
			{
				if (Top[At(I, J)] < -1e8) continue;
				if (Lo[static_cast<size_t>(I)] < 0) Lo[static_cast<size_t>(I)] = J;
				Hi[static_cast<size_t>(I)] = J;
			}
			if (Lo[static_cast<size_t>(I)] >= 0) { First = First < 0 ? I : First; Last = I; }
		}
		if (First < 0) return A;
		const auto Col = [First, Last](double Share) { return First + static_cast<int>(Share * (Last - First)); };
		const auto Right = [&](int I) { return Yof(Hi[static_cast<size_t>(I)]) + 0.5 * Cell; };
		const auto Left = [&](int I) { return Yof(Lo[static_cast<size_t>(I)]) - 0.5 * Cell; };
		// (where its lines gather: its hem's seams' middle; and its tip)
		V3 Gather;
		for (const int Sm : M.Seams) Gather = Plus(Gather, Mul(M.Points[static_cast<size_t>(Sm)], 1.0 / static_cast<double>(M.Seams.size())));
		const V3& Tip = M.Points[Half - 1];
		// (a)
		{
			double Wind = 0.0, ToTip = 0.0, Wide = 0.0;
			for (int I = Col(0.1); I <= Col(0.9); ++I)
			{
				if (Lo[static_cast<size_t>(I)] < 0) continue;
				const double Aslant = Gather.Y + (Tip.Y - Gather.Y) * (Xof(I) - Gather.X) / std::max(1e-9, Tip.X - Gather.X);
				Wind += Right(I) + Left(I) - 2.0 * Gather.Y;
				ToTip += Right(I) + Left(I) - 2.0 * Aslant;
				Wide += Right(I) - Left(I);
			}
			A.Side = std::fabs(Wind) / std::max(1e-9, Wide);
			A.SideTip = std::fabs(ToTip) / std::max(1e-9, Wide);
		}
		// (b)
		{
			std::vector<double> Edge[2];
			for (int I = Col(0.1); I <= Col(0.9); ++I)
			{
				if (Lo[static_cast<size_t>(I)] < 0) continue;
				Edge[0].push_back(Right(I));
				Edge[1].push_back(-Left(I));
			}
			for (int E = 0; E < 2; ++E)
			{
				const std::vector<double> Fit = SmoothFit(Edge[E], static_cast<int>(1.0 / Cell + 0.5));
				double Sq = 0.0;
				for (size_t K = 0; K < Fit.size(); ++K) Sq += (Edge[E][K] - Fit[K]) * (Edge[E][K] - Fit[K]);
				A.Off[E] = std::sqrt(Sq / static_cast<double>(std::max<size_t>(1, Fit.size())));
				A.Turns[E] = Swings(Edge[E], 0.05);
			}
			double SeamLo = 1e9, SeamHi = -1e9;
			for (size_t K = 0; K < M.Seams.size(); ++K)
			{
				if (!LineBack(K, M.Seams.size())) continue;
				SeamLo = std::min(SeamLo, M.Points[static_cast<size_t>(M.Seams[K])].Y);
				SeamHi = std::max(SeamHi, M.Points[static_cast<size_t>(M.Seams[K])].Y);
			}
			std::vector<double> Near, Across;
			for (int J = 0; J < Ny; ++J)
			{
				if (Yof(J) < SeamLo - 0.1 || Yof(J) > SeamHi + 0.1) continue;
				for (int I = First; I <= Last && Xof(I) <= Gather.X + 0.3; ++I)
				{
					if (Top[At(I, J)] < -1e8) continue;
					Near.push_back(Xof(I));
					Across.push_back(Yof(J));
					break;
				}
			}
			A.NearTurns = Swings(Near, 0.04);
			// (off a smooth curve through it: the least squares parabola across it, its round; and how far its most
			// forward and most back lie either side of that)
			double S[5] = {}, T[3] = {};
			const double Mid = 0.5 * (SeamLo + SeamHi);
			for (size_t K = 0; K < Near.size(); ++K)
			{
				const double Y = Across[K] - Mid;
				double Pw = 1.0;
				for (int Pk = 0; Pk < 5; ++Pk) { S[Pk] += Pw; if (Pk < 3) T[Pk] += Pw * Near[K]; Pw *= Y; }
			}
			// (Cramer's rule on the normal equations)
			const auto Det3 = [](double M11, double M12, double M13, double M21, double M22, double M23, double M31, double M32, double M33) { return M11 * (M22 * M33 - M23 * M32) - M12 * (M21 * M33 - M23 * M31) + M13 * (M21 * M32 - M22 * M31); };
			const double Dt = Det3(S[0], S[1], S[2], S[1], S[2], S[3], S[2], S[3], S[4]);
			double C0 = 0.0, C1 = 0.0, C2 = 0.0;
			if (std::fabs(Dt) > 1e-12)
			{
				C0 = Det3(T[0], S[1], S[2], T[1], S[2], S[3], T[2], S[3], S[4]) / Dt;
				C1 = Det3(S[0], T[0], S[2], S[1], T[1], S[3], S[2], T[2], S[4]) / Dt;
				C2 = Det3(S[0], S[1], T[0], S[1], S[2], T[1], S[2], S[3], T[2]) / Dt;
			}
			double Fore = 0.0, Aft = 0.0;
			for (size_t K = 0; K < Near.size(); ++K)
			{
				const double Y = Across[K] - Mid, Off = Near[K] - C0 - C1 * Y - C2 * Y * Y;
				Fore = std::min(Fore, Off);
				Aft = std::max(Aft, Off);
			}
			A.Hem = Aft - Fore;
		}
		// (c)
		{
			std::vector<double> Height, Light;
			for (int I = Col(0.08); I <= Col(0.8); ++I)
			{
				if (Lo[static_cast<size_t>(I)] < 0) continue;
				const int L0 = Lo[static_cast<size_t>(I)], H0 = Hi[static_cast<size_t>(I)], W = H0 - L0;
				double Sh = 0.0, Sl = 0.0;
				int Nh = 0, Nl = 0;
				for (int J = L0 + W / 10; J <= H0 - W / 10; ++J)
				{
					if (Top[At(I, J)] < -1e8) continue;
					Sh += Top[At(I, J)];
					++Nh;
					if (J >= L0 + W / 4 && J <= H0 - W / 4) { Sl += Luma[At(I, J)]; ++Nl; }
				}
				if (Nh == 0 || Nl == 0) continue;
				Height.push_back(Sh / Nh);
				Light.push_back(Sl / Nl);
			}
			const std::vector<double> Trend = SmoothFit(Height, static_cast<int>(1.0 / Cell + 0.5));
			for (size_t K = 0; K < Height.size(); ++K) Height[K] -= Trend[K];
			A.Folds = Swings(Height, 0.02, true);
			A.Bands = Swings(Softened(Light, 2), 8.0, true);
			const int Mx = (First + Last) / 2;
			std::vector<double> Across;
			for (int J = Lo[static_cast<size_t>(Mx)] + 2; J <= Hi[static_cast<size_t>(Mx)] - 1; ++J) Across.push_back(Luma[At(Mx, J)]);
			const std::vector<double> Soft = Softened(Across, 3);
			A.LumaLo = Soft.empty() ? 0.0 : *std::min_element(Soft.begin(), Soft.end());
			A.LumaHi = Soft.empty() ? 0.0 : *std::max_element(Soft.begin(), Soft.end());
		}
		// (d)
		{
			double Sum = 0.0;
			int N = 0;
			for (int I = Col(0.72); I <= Col(0.95); ++I)
			{
				if (Lo[static_cast<size_t>(I)] < 0) continue;
				double Low = 1e9, Lift = 0.0, LiftY = 0.0;
				for (int J = Lo[static_cast<size_t>(I)]; J <= Hi[static_cast<size_t>(I)]; ++J)
				{
					if (Top[At(I, J)] > -1e8) Low = std::min(Low, Top[At(I, J)]);
				}
				for (int J = Lo[static_cast<size_t>(I)]; J <= Hi[static_cast<size_t>(I)]; ++J)
				{
					if (Top[At(I, J)] < -1e8) continue;
					Lift += Top[At(I, J)] - Low;
					LiftY += (Top[At(I, J)] - Low) * Yof(J);
				}
				if (Lift < 1e-9) continue;
				Sum += LiftY / Lift - 0.5 * (Right(I) + Left(I));
				++N;
			}
			A.Slump = std::fabs(Sum / std::max(1, N));
		}
		// (e)
		{
			const V3 H0 = OverHarness(1.0, ProneEndX, ProneEndY);
			const double Turn2 = (Dn.Face - Dn.HeapHeading) * 3.14159265358979323846 / 180.0;
			const V3 H{ H0.X * std::cos(Turn2) - H0.Y * std::sin(Turn2), H0.X * std::sin(Turn2) + H0.Y * std::cos(Turn2), H0.Z };
			const double Body = Gather.X + 0.15 * (MaxX - MinX), RealLong = RealLines * Dn.Across;
			double FloatSum = 0.0, FloatN = 0.0, HighOn = 0.0, AllOn = 0.0;
			for (size_t K = 0; K < M.Seams.size(); ++K)
			{
				if (!LineBack(K, M.Seams.size())) continue;
				++A.Lines;
				V3 Way[LinePoints];
				const int Points = LineLies(M, K, H, 1.0, Way);
				const V3& E = Way[Points - 1];
				const bool bStrewn = Points > 3;
				double On = 0.0, OnBody = 0.0, Drawn = 0.0, Reach = -1e9;
				V3 OnFirst, OnLast;
				bool bOn = false;
				for (int Pt = 0; Pt + 1 < Points; ++Pt)
				{
					const V3 D = Plus(Way[Pt + 1], Mul(Way[Pt], -1.0));
					Drawn += Length(D);
					const int Steps = std::max(1, static_cast<int>(std::hypot(D.X, D.Y) / 0.01));
					for (int S = 0; S < Steps; ++S)
					{
						const V3 P = Plus(Way[Pt], Mul(D, (S + 0.5) / Steps));
						// (and how near where its lines gather one laid across it comes on its way to its silk)
						if (bStrewn && !bOn) A.GatherNear = std::min(A.GatherNear, std::hypot(P.X - Gather.X, P.Y - Gather.Y));
						const double Under = TopAt(P.X, P.Y);
						if (Under < -1e8 || (!bStrewn && std::hypot(P.X - E.X, P.Y - E.Y) <= 0.1)) continue;
						const double Over = Under - P.Z <= 0.004 ? std::hypot(D.X, D.Y) / Steps : 0.0;
						On += Over;
						OnBody += P.X > Body ? Over : 0.0;
						A.Buried = std::max(A.Buried, Under - P.Z);
						if (bStrewn && Over > 0.0)
						{
							if (!bOn) OnFirst = P;
							bOn = true;
							OnLast = P;
							Reach = std::max(Reach, P.X);
							A.FloatMost = std::max(A.FloatMost, P.Z - Under);
							FloatSum += P.Z - Under;
							FloatN += 1.0;
							HighOn += P.Z - Under > 0.04 ? Over : 0.0;
							AllOn += Over;
						}
					}
				}
				A.Crossing += On >= 0.2;
				A.CrossMost = std::max(A.CrossMost, On);
				A.BodyLines += OnBody >= 0.3;
				A.BodyMost = std::max(A.BodyMost, OnBody);
				if (!bStrewn) continue;
				++A.Strewn;
				A.LongShare = std::max(A.LongShare, Drawn / RealLong);
				A.OnLeast = std::min(A.OnLeast, On);
				A.OnMost = std::max(A.OnMost, On);
				if (!bOn) continue;
				A.SlantLeast = std::min(A.SlantLeast, std::atan2(std::fabs(OnLast.Y - OnFirst.Y), OnLast.X - OnFirst.X) * 57.29577951308232);
				++A.Slanted;
				A.ReachMost = std::max(A.ReachMost, (Reach - Gather.X) / (MaxX - MinX));
				A.GrassMost = std::max(A.GrassMost, std::hypot(OnFirst.X - H.X, OnFirst.Y - H.Y));
			}
			A.FloatMean = FloatN > 0.0 ? FloatSum / FloatN : 0.0;
			A.FloatHigh = AllOn > 0.0 ? HighOn / AllOn : 0.0;
		}
		// (f)
		{
			// (its vent, where its seams meet: its tip, off the middle of its far fifth's silk, a share of its half-width
			// there at the most)
			double SumY = 0.0, Cells = 0.0, Widest = 0.0;
			for (int I = Col(0.8); I <= Last; ++I)
			{
				if (Lo[static_cast<size_t>(I)] < 0) continue;
				Widest = std::max(Widest, 0.5 * (Right(I) - Left(I)));
				for (int J = Lo[static_cast<size_t>(I)]; J <= Hi[static_cast<size_t>(I)]; ++J)
				{
					if (Top[At(I, J)] < -1e8) continue;
					SumY += Yof(J);
					Cells += 1.0;
				}
			}
			A.Vent = Cells > 0.0 && Widest > 0.0 ? std::fabs(Tip.Y - SumY / Cells) / Widest : 0.0;
			// (a crease across its far end's dome: along it, row by row across it, away from its edges, the deepest its top
			// dips under the lower of the highest it stands either side, from 0.72 of its length on)
			for (int J = 0; J < Ny; ++J)
			{
				std::vector<double> Run;
				const auto Dips = [&Run, &A]()
				{
					// (its upper hull along the row, and how far under it its top dips)
					std::vector<size_t> Hull;
					for (size_t K = 0; K < Run.size(); ++K)
					{
						while (Hull.size() >= 2)
						{
							const size_t Ha = Hull[Hull.size() - 2], Hb = Hull.back();
							if ((Run[Hb] - Run[Ha]) * static_cast<double>(K - Ha) > (Run[K] - Run[Ha]) * static_cast<double>(Hb - Ha)) break;
							Hull.pop_back();
						}
						Hull.push_back(K);
					}
					for (size_t Hk = 0; Hk + 1 < Hull.size(); ++Hk)
					{
						const size_t Ha = Hull[Hk], Hb = Hull[Hk + 1];
						for (size_t K = Ha + 1; K < Hb; ++K) A.Crease = std::max(A.Crease, Run[Ha] + (Run[Hb] - Run[Ha]) * static_cast<double>(K - Ha) / static_cast<double>(Hb - Ha) - Run[K]);
					}
					Run.clear();
				};
				for (int I = Col(0.72); I <= Last; ++I)
				{
					const bool bIn = Lo[static_cast<size_t>(I)] >= 0 && J >= Lo[static_cast<size_t>(I)] + 8 && J <= Hi[static_cast<size_t>(I)] - 8 && Top[At(I, J)] > -1e8;
					if (bIn) Run.push_back(Top[At(I, J)]);
					else Dips();
				}
				Dips();
			}
			// (its last two rings folded back under at an edge: across its top layer from its middle, how much farther out
			// its silk reaches than its edge does, metres, at the most; and its seams over its top's middle across, ring to
			// ring from its ninth on, how far each bends aside, on the mean)
			const int Cols = Dn.Gores * 4, Ringed = static_cast<int>((Half - 1) / static_cast<size_t>(Cols));
			const auto ThetaOf = [Cols](int Col2)
			{
				const int Q = Col2 % 4;
				return 2.0 * 3.14159265358979323846 * (Col2 / 4 + (Q == 0 ? 0.0 : Q == 1 ? 0.2 : Q == 2 ? 0.5 : 0.8)) / (Cols / 4);
			};
			for (int R = Ringed - 2; R < Ringed; ++R)
			{
				const double Mid = M.Points[static_cast<size_t>(R * Cols + Cols / 2)].Y;
				for (int Way = -1; Way <= 1; Way += 2)
				{
					const double Edge = Way * (M.Points[static_cast<size_t>(R * Cols + (Way > 0 ? Cols / 4 : 3 * Cols / 4))].Y - Mid);
					double Out = Edge;
					for (int Col2 = 0; Col2 < Cols; ++Col2)
					{
						const double Theta = ThetaOf(Col2);
						if (std::cos(Theta) <= 0.0 && Way * std::sin(Theta) > 0.0) Out = std::max(Out, Way * (M.Points[static_cast<size_t>(R * Cols + Col2)].Y - Mid));
					}
					A.Under = std::max(A.Under, Out - Edge);
				}
			}
			int SeamsN = 0;
			for (int Col2 = 0; Col2 < Cols; Col2 += 4)
			{
				const double Theta = ThetaOf(Col2);
				if (std::cos(Theta) > -0.3 || std::fabs(std::sin(Theta)) > 0.45) continue;
				double Ys[4];
				for (int R = 0; R < 4; ++R) Ys[R] = M.Points[static_cast<size_t>((Ringed - 4 + R) * Cols + Col2)].Y;
				A.Kinks += std::max(std::fabs(Ys[0] - 2.0 * Ys[1] + Ys[2]), std::fabs(Ys[1] - 2.0 * Ys[2] + Ys[3]));
				++SeamsN;
			}
			A.Kinks /= std::max(1, SeamsN);
		}
		return A;
	}

	/** The 300 pilots' numbers the sweeps measure: canopies 5 to 5.5 m across on 24 to 28 gores, in winds of 0 to 7 m/s,
	 * a seventh landing before it has filled, each facing the way it falls or up to a right angle off it. */
	Chute SweepPilot(int K)
	{
		Chute Dn;
		Dn.Folds = static_cast<uint32_t>(2654435761u * static_cast<uint32_t>(K + 1));
		Dn.Gores = 24 + 2 * (K % 3);
		Dn.Across = 5.0 + 0.5 * ((K * 37) % 100) / 100.0;
		Dn.Deep = 0.48 * Dn.Across;
		Dn.Wind = 7.0 * ((K * 53) % 100) / 100.0;
		Dn.Open = K % 7 == 0 ? 0.75 : 1.0;
		Dn.HeapHeading = 360.0 * ((K * 71) % 100) / 100.0;
		Dn.Face = Dn.HeapHeading + 90.0 * (((K * 29) % 100) / 50.0 - 1.0);
		return Dn;
	}

	/** The fields a canopy is laid on to see none of it goes under the ground, in the canopy's own space (x the way it
	 * falls from his feet, y to its right; metres over his feet): a slope of 1 in 4 rising and falling along the wind and
	 * across it, a field of hummocks, a hedgerow bank 1 m high, a step 0.4 m up, a ditch 0.6 m deep, a sharp crest, a
	 * crater's bowl and rim, and a bank along it under one edge. */
	constexpr int GroundFields = 11;
	double FieldUnder(int Field, double X, double Y)
	{
		switch (Field)
		{
		case 0: return 0.25 * X;
		case 1: return -0.25 * X;
		case 2: return 0.25 * Y;
		case 3: return -0.25 * Y;
		case 4: return 0.6 * std::sin(X * 0.45) * std::cos(Y * 0.37) + 0.3 * std::sin(X * 1.1 + Y * 0.8);
		case 5: return std::fabs(X - 5.0) < 0.6 ? 0.5 + 0.5 * std::cos(3.14159265358979323846 * (X - 5.0) / 0.6) : 0.0;
		case 6: return 0.4 * std::clamp((X - 6.0) / 0.1 + 0.5, 0.0, 1.0);
		case 7: return std::fabs(X - 3.5) < 0.6 ? -0.6 * (1.0 - (X - 3.5) * (X - 3.5) / 0.36) : 0.0;
		case 8: return std::max(0.0, 0.6 - 0.6 * std::fabs(X - 5.0));
		case 9:
		{
			const double R = std::hypot(X - 6.0, Y - 0.5);
			return R < 1.5 ? -0.8 * (1.0 - R * R / 2.25) + 0.25 * R * R / 2.25 : R < 2.5 ? 0.25 * (2.5 - R) : 0.0;
		}
		default: return std::fabs(Y - 1.0) < 0.5 ? 0.4 + 0.4 * std::cos(3.14159265358979323846 * (Y - 1.0) / 0.5) : 0.0;
		}
	}
	/** The field as the board's ground (GroundFn, board metres) under a canopy that lies from Dn's Landed the way it
	 * falls. */
	GroundFn FieldBoard(int Field, const Chute& Dn)
	{
		const double Hd = Dn.HeapHeading * 3.14159265358979323846 / 180.0;
		const V3 At0 = Dn.Landed;
		return [Field, At0, Hd](double X, double Y)
		{
			const double Ex = X - At0.X, Ey = Y - At0.Y;
			return At0.Z + FieldUnder(Field, Ex * std::sin(Hd) + Ey * std::cos(Hd), Ex * std::cos(Hd) - Ey * std::sin(Hd));
		};
	}
	/** His harness as the scene draws his lines from it once he lies (LandBattleAir's DownShape: in his chute mesh's own
	 * space, his feet at its origin, his man flat and rigid, the way he faced as he landed), not lifted by the ground
	 * under it. */
	V3 DrawnHarness(const Chute& Dn)
	{
		const V3 H0 = OverHarness(1.0, ProneEndX, ProneEndY);
		const double Turn2 = (Dn.Face - Dn.HeapHeading) * 3.14159265358979323846 / 180.0;
		return { H0.X * std::cos(Turn2) - H0.Y * std::sin(Turn2), H0.X * std::sin(Turn2) + H0.Y * std::cos(Turn2), H0.Z };
	}
	/** How far a line as the scene draws it (Way, Points of them, LineLies') stands over Field at its lowest: each of its
	 * points but his harness, and its straights at seven places each from the first on (the cord's middle). */
	double LineOver(int Field, const V3* Way, int Points)
	{
		double Least = 1e9;
		for (int Pt = 1; Pt < Points; ++Pt)
		{
			Least = std::min(Least, Way[Pt].Z - FieldUnder(Field, Way[Pt].X, Way[Pt].Y));
			for (int S7 = 1; S7 < 8 && Pt + 1 < Points; ++S7)
			{
				const V3 Q = Plus(Way[Pt], Mul(Plus(Way[Pt + 1], Mul(Way[Pt], -1.0)), S7 / 8.0));
				Least = std::min(Least, Q.Z - FieldUnder(Field, Q.X, Q.Y));
			}
		}
		return Least;
	}

	/** The 300 pilots' numbers on every field (FieldUnder), falling in and lying, made as the scene makes them (SilkStep:
	 * made each 0.05 of its fall and moved in place between) and looked at each 0.0125 of it. At each make: how many of its
	 * points are 5 mm or more under the ground (and the deepest), how many of his lines as the scene draws them (LineLies,
	 * as slack as they are then, from DrawnHarness) have their cord's middle under it anywhere (LineOver), and the least
	 * those across its silk and the rest stand over it. Between makes: how many have it more than 3 mm under (inside the
	 * 12 mm cord: a third review found one 0.7 mm under there on hummocks), and the least. And at makes and between, on
	 * each field, how far the points his lines across its silk go through across the grass stand over both the taut
	 * straight from his harness to where it comes onto its silk and the ground there (Flung: a third review found them
	 * flung 5 to 7 m up off a bank), the most and how many looks had one over 1.5 m. Pilots First, First + Every and on. */
	struct GroundTally
	{
		int Under = 0, LinesUnder = 0, LinesUnderBetween = 0;
		double Deepest = -1e9, AcrossLeast = 1e9, RestLeast = 1e9, BetweenLeast = 1e9;
		int AcrossAt[3] = { -1, -1, -1 };    // (the pilot, field and step of its least, across)
		double Flung[GroundFields], FlungSlump[GroundFields];
		int FlungPilot[GroundFields], FlungHigh[GroundFields];
		GroundTally()
		{
			std::fill(Flung, Flung + GroundFields, -1e9);
			std::fill(FlungSlump, FlungSlump + GroundFields, -1.0);
			std::fill(FlungPilot, FlungPilot + GroundFields, -1);
			std::fill(FlungHigh, FlungHigh + GroundFields, 0);
		}
	};
	void GroundSweep(int First, int Every, GroundTally& T)
	{
		SilkMesh M;
		for (int K = First; K < 300; K += Every)
		{
			const Chute Dn = SweepPilot(K);
			const double Off = HeapOff(Dn.Folds, Dn.Across, Dn.Wind, Dn.Face, Dn.HeapHeading);
			const V3 H = DrawnHarness(Dn);
			for (int Field = 0; Field < GroundFields; ++Field)
			{
				const GroundFn Board = FieldBoard(Field, Dn);
				SilkStep Step;
				for (int Q = 0; Q <= 80; ++Q)
				{
					const int Sk = Q / 4;
					const bool bMake = Q % 4 == 0;
					const double Slump = Q / 80.0;
					if (bMake) Step.Make(Dn, Slump, Off, Board);
					Step.At(Slump, M);
					if (bMake)
					{
						for (const V3& Pt : M.Points)
						{
							const double Short = FieldUnder(Field, Pt.X, Pt.Y) - Pt.Z;
							T.Under += Short + 0.005 > 0.0;
							T.Deepest = std::max(T.Deepest, Short);
						}
					}
					double Highest = -1e9;
					for (size_t Sm = 0; Sm < M.Seams.size(); ++Sm)
					{
						if (!LineBack(Sm, M.Seams.size())) continue;
						V3 Way[LinePoints];
						const int Points = LineLies(M, Sm, H, LineSlack(Slump), Way);
						const double Least = LineOver(Field, Way, Points);
						if (Points > 3)
						{
							for (int Pt = 1; Pt <= LineGrass; ++Pt)
							{
								const double TautZ = H.Z + (Way[LineGrass + 1].Z - H.Z) * Pt / (LineGrass + 1.0);
								Highest = std::max(Highest, Way[Pt].Z - std::max(TautZ, FieldUnder(Field, Way[Pt].X, Way[Pt].Y)));
							}
						}
						if (!bMake)
						{
							T.LinesUnderBetween += Least < -0.003;
							if (Points > 3) T.BetweenLeast = std::min(T.BetweenLeast, Least);
							continue;
						}
						T.LinesUnder += Least < 0.0;
						if (Points > 3 && Least < T.AcrossLeast)
						{
							T.AcrossLeast = Least;
							T.AcrossAt[0] = K;
							T.AcrossAt[1] = Field;
							T.AcrossAt[2] = Sk;
						}
						if (Points <= 3) T.RestLeast = std::min(T.RestLeast, Least);
					}
					T.FlungHigh[Field] += Highest > 1.5;
					if (Highest > T.Flung[Field])
					{
						T.Flung[Field] = Highest;
						T.FlungPilot[Field] = K;
						T.FlungSlump[Field] = Slump;
					}
				}
			}
		}
	}
}

int main(int Argc, char** Argv)
{
	const double Life = 2.0;
	LandFeed::Recording Rec;
	std::string Err;
	if (!LandFeed::ReadRecording(Recording(true), Rec, Err)) { std::printf("FAIL: recording: %s\n", Err.c_str()); return 1; }

	// ---- feed
	int In = 0, Strafe = 0, Bomb = 0, Lost = 0;
	bool bFields = true;
	for (const LandFeed::AirNote& N : Rec.Air)
	{
		In += N.What == "land:air-in";
		Strafe += N.What == "land:strafe";
		Bomb += N.What == "land:bomb-drop";
		Lost += N.What == "air:lost";
		if (N.What == "land:air-in" && N.Flight == 1) bFields = bFields && N.Models == 4 && N.Nat == "USA" && N.Year == 1944 && N.Run[2] == 1100.0 && N.Side == 0;
		if (N.What == "land:strafe" && N.Flight == 2) bFields = bFields && std::fabs(N.Secs - 200.0 / 120.0) < 1e-4 && N.Width == 60.0 && N.Run[0] == 1050.0 && N.Run[2] == 850.0 && N.Side == 1;
		if (N.What == "land:bomb-drop" && N.Flight == 2) bFields = bFields && N.At > 90.0 && N.To[0] == 1050.0 - 200.0 * 0.8;
		if (N.What == "air:lost" && N.Flight == 3) bFields = bFields && N.Kind == "explode" && N.Chutes == 0;
	}
	std::printf("     notes: %d in, %d strafe, %d bombs, %d lost, fields %s\n", In, Strafe, Bomb, Lost, bFields ? "right" : "wrong");
	Check(In == 4 && Strafe == 4 && Bomb == 3 && Lost == 3 && bFields, "feed: every note read, with its flight, run, models, strafe, bombs and loss");

	// ---- machine
	Check(MachineOf("USA", 1944).Model == "Mustang" && MachineOf("USA", 1943).Model == "Warhawk" && MachineOf("GER", 1942).Model == "Stuka" && MachineOf("GER", 1943).Model == "Stuka"
		&& MachineOf("GER", 1944).Model == "Fw190" && MachineOf("UK", 1941).Model == "Hurricane" && MachineOf("CAN", 1944).Model == "Spitfire"
		&& MachineOf("SOV", 1941).Model == "Sturmovik" && MachineOf("JAP", 1942).Model == "Zero" && MachineOf("ITA", 1942).Model == "Fighter",
		"machine: each air force's machine by year");

	Sky S;
	S.Build(Rec, Flat, Life);
	Check(S.Legs() == 4 && S.Notes() == Rec.Air.size(), "build: four legs planned");

	// ---- play it, 60 frames a second
	const double T0 = 40.0, T1 = 170.0, Dt = 1.0 / 60.0;
	struct Seen
	{
		bool bShown = false, bFired = false, bHit = false, bCrashed = false;
		double FirstT = 1e9, MinSpeed = 1e9, MaxSpeed = 0.0, MinPitch = 0.0, MinClear = 1e9, MaxStep = 0.0, MaxHeadStep = 0.0;
		double FireFrom = 1e9, FireTo = -1e9, SlantOpen = -1.0, SlantShut = -1.0;
		double AimStartMiss = 1e9, AimEndMiss = 1e9, AimHigh = 0.0;
		double HitT = -1.0, CrashT = -1.0, MaxHigh = 0.0, LowestFly = 1e9;
		Plane Last;
		bool bLast = false;
		int Hits = 0, Crashes = 0, WreckFrames = 0, WreckOff = 0, ShownAfterExplode = 0;
		LandAir::Fall How = LandAir::Fall::None;
		V3 HitAt, CrashAt;
	};
	std::vector<Seen> Seens(64);
	int GunFrames = 0, OwnSideGuns = 0, FarStreams = 0, LowStreams = 0, NoPlaneGuns = 0, Nans = 0;
	std::vector<int> GunsByLeg(4, 0);
	std::vector<Plane> Ps;
	std::vector<Gunner> Gs;
	std::vector<Happen> Hs;
	uint64_t H1 = 1469598103934665603ull;
	std::ofstream Out;
	if (Argc > 1) Out.open(std::string(Argv[1]) + "/air_paths.txt");
	int RoundsSum = 0;
	for (double T = T0; T <= T1; T += Dt)
	{
		S.At(T, Ps, Gs);
		H1 = Hash(Ps, Gs, H1);
		S.Between(T - Dt, T, Hs);
		RoundsSum += S.Rounds(T - Dt, T, 22.0);
		for (const Happen& H : Hs)
		{
			Seen& Sn = Seens[static_cast<size_t>(H.Key)];
			if (H.Kind == Happen::What::Hit) { ++Sn.Hits; Sn.HitT = H.T; Sn.How = H.How; Sn.HitAt = H.At; }
			else { ++Sn.Crashes; Sn.CrashT = H.T; Sn.CrashAt = H.At; }
			if (Out) Out << (H.Kind == Happen::What::Hit ? "hit " : "crash ") << H.Key << " " << H.T << " " << H.At.X << " " << H.At.Y << " " << H.At.Z << " " << static_cast<int>(H.How) << "\n";
		}
		for (const Plane& P : Ps)
		{
			Seen& Sn = Seens[static_cast<size_t>(P.Key)];
			for (double V : { P.At.X, P.At.Y, P.At.Z, P.Heading, P.Pitch, P.Bank, P.Vel.X, P.Vel.Y, P.Vel.Z }) Nans += std::isfinite(V) ? 0 : 1;
			if (Out) Out << "p " << P.Key << " " << T << " " << P.At.X << " " << P.At.Y << " " << P.At.Z << " " << P.Heading << " " << P.Pitch << " " << P.Bank << " " << P.bFiring << " " << static_cast<int>(P.How) << " " << P.bWreck
				<< " " << P.Aim.X << " " << P.Aim.Y << " " << P.Model << "\n";
			if (P.bWreck)
			{
				++Sn.WreckFrames;
				if (Sn.Crashes == 0 || std::hypot(P.At.X - Sn.CrashAt.X, P.At.Y - Sn.CrashAt.Y) > 1.0) ++Sn.WreckOff;
				continue;
			}
			if (Sn.How == LandAir::Fall::Explode && Sn.Hits > 0) ++Sn.ShownAfterExplode;
			if (!Sn.bShown) { Sn.bShown = true; Sn.FirstT = T; }
			if (P.bFiring) Sn.MaxHigh = std::max(Sn.MaxHigh, P.At.Z);
			if (P.How == LandAir::Fall::None)
			{
				const double Sp = std::sqrt(P.Vel.X * P.Vel.X + P.Vel.Y * P.Vel.Y + P.Vel.Z * P.Vel.Z);
				Sn.MinSpeed = std::min(Sn.MinSpeed, Sp);
				Sn.MaxSpeed = std::max(Sn.MaxSpeed, Sp);
				Sn.MinPitch = std::min(Sn.MinPitch, P.Pitch);
				Sn.MinClear = std::min(Sn.MinClear, P.At.Z);
				Sn.LowestFly = Sn.MinClear; // (the flat field: its height is its clearance; its bottom, once past it)
				if (Sn.bLast && Sn.Last.How == LandAir::Fall::None)
				{
					Sn.MaxStep = std::max(Sn.MaxStep, std::max(std::fabs(P.Pitch - Sn.Last.Pitch), std::fabs(P.Bank - Sn.Last.Bank)));
					Sn.MaxHeadStep = std::max(Sn.MaxHeadStep, Turn(P.Heading, Sn.Last.Heading));
				}
				if (P.bFiring)
				{
					const Strike& St = Strikes[P.Leg];
					const double Fx = (St.Run[2] - St.Run[0]), Fy = (St.Run[3] - St.Run[1]), L = std::hypot(Fx, Fy);
					// (its lane's start and end: the run's ends, off to its side by its place in the leg)
					const double W0 = St.Models > 1 ? (P.Slot - (St.Models - 1) * 0.5) * std::min(18.0, 60.0 / St.Models) : 0.0;
					const double Sx = St.Run[0] + Fy / L * W0, Sy = St.Run[1] - Fx / L * W0, Ex = St.Run[2] + Fy / L * W0, Ey = St.Run[3] - Fx / L * W0;
					if (!Sn.bFired) Sn.AimStartMiss = std::hypot(P.Aim.X - Sx, P.Aim.Y - Sy);
					const double Slant = std::hypot(std::hypot(P.At.X - P.Aim.X, P.At.Y - P.Aim.Y), P.At.Z - P.Aim.Z);
					if (!Sn.bFired) Sn.SlantOpen = Slant;
					Sn.SlantShut = Slant;
					Sn.AimEndMiss = std::hypot(P.Aim.X - Ex, P.Aim.Y - Ey);
					Sn.AimHigh = std::max(Sn.AimHigh, std::fabs(P.Aim.Z));
					Sn.bFired = true;
					Sn.FireFrom = std::min(Sn.FireFrom, T);
					Sn.FireTo = std::max(Sn.FireTo, T);
				}
			}
			Sn.Last = P;
			Sn.bLast = true;
		}
		for (const Gunner& G : Gs)
		{
			++GunFrames;
			const int Leg = G.Key / 8;
			++GunsByLeg[static_cast<size_t>(Leg)];
			// (from one of the other side's blocks: the att's strikes over the def's blocks, to the north)
			const bool bAttStrike = std::strcmp(Strikes[Leg].Side, "att") == 0;
			if ((bAttStrike && G.From.Y < 600.0) || (!bAttStrike && G.From.Y > 600.0)) ++OwnSideGuns;
			double Near = 1e9;
			for (const Plane& P : Ps) if (P.Key / 8 == Leg && !P.bWreck) Near = std::min(Near, std::hypot(std::hypot(G.To.X - P.At.X, G.To.Y - P.At.Y), G.To.Z - P.At.Z));
			if (Near > 1e8) ++NoPlaneGuns;
			else if (Near > 150.0) ++FarStreams;
			if (G.To.Z < 8.0) ++LowStreams;
			if (Out) Out << "g " << G.Key << " " << T << " " << G.From.X << " " << G.From.Y << " " << G.To.X << " " << G.To.Y << " " << G.To.Z << "\n";
		}
	}
	if (Out)
	{
		for (const Strike& St : Strikes) Out << "run " << St.Flight << " " << St.Run[0] << " " << St.Run[1] << " " << St.Run[2] << " " << St.Run[3] << " " << St.Nat << " " << St.Year << " " << St.Side << "\n";
		for (const LandFeed::Unit& U : Rec.Views[0].Units) Out << "u " << U.Id << " " << U.Side << " " << U.X << " " << U.Y << " " << U.Str << "\n";
	}

	// ---- flight
	int Early = 0, Slow = 0, Ground = 0, Jerky = 0, NotShown = 0, BadDive = 0;
	double Stuka = 0.0, Il2High = 0.0;
	for (size_t L = 0; L < 4; ++L)
	{
		const Strike& St = Strikes[L];
		const Machine M = MachineOf(St.Nat, St.Year);
		for (int K = 0; K < St.Models; ++K)
		{
			const Seen& Sn = Seens[L * 8 + static_cast<size_t>(K)];
			if (!Sn.bShown) { ++NotShown; continue; }
			if (Sn.FirstT < St.T - 25.0 / Life) ++Early;
			if (Sn.MinSpeed < 0.9 * M.Speed || Sn.MaxSpeed > 1.2 * M.Speed) { ++Slow; std::printf("     note: leg %zu plane %d speed %.0f to %.0f m/s (its machine's %.0f)\n", L, K, Sn.MinSpeed, Sn.MaxSpeed, M.Speed); }
			// (the art director's shapes, 19:08Z: a fighter's dive 20 to 30 degrees, out 30 to 60 m up, guns from about
			// 900 m to 250 m; a Stuka's 70 to 80, out about 300 m up; an Il-2's out 10 to 20 m up)
			const bool bLost = *St.Lost && K == St.Models - 1;
			const double Dive = -Sn.MinPitch;
			bool bShape = true;
			if (M.Model == "Stuka") bShape = Dive >= 70.0 && Dive <= 80.0 && Sn.LowestFly >= 250.0 && Sn.LowestFly <= 350.0;
			else if (M.Model == "Sturmovik") bShape = Sn.LowestFly >= 10.0 && Sn.LowestFly <= 20.0;
			else bShape = Dive >= 20.0 && Dive <= 30.0 && Sn.LowestFly >= 30.0 && Sn.LowestFly <= 60.0 && Sn.SlantOpen >= 800.0 && Sn.SlantOpen <= 1000.0 && Sn.SlantShut >= 180.0 && Sn.SlantShut <= 320.0;
			// (the one lost: hit before its bottom or soon after, so only its dive and its guns' opening)
			if (bLost) bShape = M.Model == "Stuka" ? Dive >= 70.0 && Dive <= 80.0 : M.Model == "Sturmovik" ? true : Dive >= 20.0 && Dive <= 30.0 && Sn.SlantOpen >= 800.0 && Sn.SlantOpen <= 1000.0;
			if (!bShape) ++BadDive;
			std::printf("     leg %zu plane %d (%s): dive %.0f deg, bottom %.0f m, guns from %.0f m to %.0f m slant\n", L, K, M.Model.c_str(), Dive, Sn.LowestFly, Sn.SlantOpen, Sn.SlantShut);
			if (Sn.MinClear < 5.0) ++Ground;
			if (Sn.MaxStep > 3.0 || Sn.MaxHeadStep > 3.0) { ++Jerky; std::printf("     note: leg %zu plane %d steps %.1f deg (heading %.1f) a frame\n", L, K, Sn.MaxStep, Sn.MaxHeadStep); }
			if (M.Model == "Stuka") Stuka = std::min(Stuka, Sn.MinPitch);
			if (M.Model == "Sturmovik") Il2High = std::max(Il2High, Sn.MaxHigh);
		}
	}
	Check(NotShown == 0 && Early == 0, "flight: every plane drawn, none long before its leg comes");
	Check(Slow == 0, "flight: each at its machine's speed along its way (0.9 to 1.2 times: the climb away's on top)");
	Check(BadDive == 0, "flight: the art director's shapes (fighters 20 to 30 deg, out 30 to 60 m, guns 800-1000 to 180-320 m; Stuka 70 to 80 deg, out 250 to 350 m; Il-2 out 10 to 20 m)");
	Check(Ground == 0, "flight: none flies into the ground (5 m and over)");
	Check(Jerky == 0, "flight: heading, pitch and bank smooth (3 degrees a frame at most)");
	char Words[200];
	std::snprintf(Words, sizeof(Words), "flight: a Stuka dives steeply (%.0f deg), an Il-2 stays low (%.0f m at most)", Stuka, Il2High);
	Check(Stuka < -65.0 && Il2High < 200.0, Words);

	// ---- guns
	int NoFire = 0, OffLane = 0, AimUp = 0, BadGap = 0;
	for (size_t L = 0; L < 4; ++L)
	{
		const Strike& St = Strikes[L];
		double From = 1e9, To = -1e9;
		for (int K = 0; K < St.Models; ++K)
		{
			const Seen& Sn = Seens[L * 8 + static_cast<size_t>(K)];
			if (!Sn.bFired) { ++NoFire; continue; }
			const bool bLost = *St.Lost && K == St.Models - 1;
			if (Sn.AimStartMiss > 10.0 || (!bLost && Sn.AimEndMiss > 10.0)) { ++OffLane; std::printf("     note: leg %zu plane %d aim starts %.1f m and ends %.1f m off its lane\n", L, K, Sn.AimStartMiss, Sn.AimEndMiss); }
			if (Sn.AimHigh > 0.01) ++AimUp;
			if (K > 0)
			{
				const double Gap = (Sn.FireFrom - Seens[L * 8 + static_cast<size_t>(K) - 1].FireFrom) * Life;
				if (Gap < 5.99 || Gap > 12.01) { ++BadGap; std::printf("     note: leg %zu plane %d dives %.1f s after the one before\n", L, K, Gap); }
			}
			From = std::min(From, Sn.FireFrom);
			To = std::max(To, Sn.FireTo);
		}
		std::printf("     leg %zu (%s %d, %d planes): guns on the strip from %.1f to %.1f battle s (the rules' run %.1f s from %.1f)\n", L, St.Nat, St.Year, St.Models, From, To,
			std::hypot(St.Run[2] - St.Run[0], St.Run[3] - St.Run[1]) / 120.0, St.T);
	}
	Check(NoFire == 0 && OffLane == 0 && AimUp == 0, "guns: each plane's guns walk its lane of the strip from start to end (the lost one till it is hit), on the ground");
	Check(BadGap == 0, "guns: each plane dives 6 to 12 s after the one before (each dive its own beat)");
	{
		// (a lane never over its own men: the attacker's block stood on the Mustangs' strip, 60 m along it)
		LandFeed::Recording Own;
		LandFeed::ReadRecording(Recording(true, 100, true), Own, Err);
		Sky O;
		O.Build(Own, Flat, Life);
		double First = 1e9;
		std::vector<Plane> Op;
		std::vector<Gunner> Og;
		for (double T = T0; T <= T1; T += Dt)
		{
			O.At(T, Op, Og);
			for (const Plane& P : Op) if (P.Leg == 0 && P.bFiring) First = std::min(First, P.Aim.X - 700.0);
		}
		std::printf("     with its own block 60 m along the strip, the Mustangs' guns strike from %.0f m along it\n", First);
		Check(First >= 209.0 && First < 400.0, "guns: a lane starts 150 m past its own side's front at the least");
	}

	// ---- losses
	int Hit = 0, BadOrder = 0, BadFall = 0, OtherFell = 0, NoWreck = 0, Explodes = 0;
	for (size_t L = 0; L < 4; ++L)
	{
		const Strike& St = Strikes[L];
		for (int K = 0; K < St.Models; ++K)
		{
			const Seen& Sn = Seens[L * 8 + static_cast<size_t>(K)];
			const bool bLost = *St.Lost && K == St.Models - 1;
			if (!bLost) { OtherFell += Sn.Hits + Sn.Crashes; continue; }
			Hit += Sn.Hits;
			if (Sn.Hits != 1 || Sn.Crashes != 1 || Sn.CrashT <= Sn.HitT || std::fabs(Sn.CrashAt.Z) > 0.01) ++BadOrder;
			const double Fell = (Sn.CrashT - Sn.HitT) * Life, Across = std::hypot(Sn.CrashAt.X - Sn.HitAt.X, Sn.CrashAt.Y - Sn.HitAt.Y), Drop = Sn.HitAt.Z - Sn.CrashAt.Z;
			std::printf("     leg %zu: %s from %.0f m, %.1f s down, %.0f m on, at %.0f deg\n", L, St.Lost, Drop, Fell, Across, std::atan2(Drop, std::max(1.0, Across)) * 57.2958);
			if (std::strcmp(St.Lost, "burn") == 0 && (Fell < 2.0 || Fell > 25.0 || std::atan2(Drop, Across) * 57.2958 > 30.0)) ++BadFall;
			if (std::strcmp(St.Lost, "spin") == 0 && (std::atan2(Drop, std::max(1.0, Across)) * 57.2958 < 30.0 || Drop / std::max(0.1, Fell) < 45.0)) ++BadFall;
			if (std::strcmp(St.Lost, "explode") == 0) { ++Explodes; if (Sn.ShownAfterExplode > 0 || Sn.WreckFrames > 0) ++BadFall; }
			else if (Sn.WreckFrames == 0 || Sn.WreckOff > 0) ++NoWreck;
		}
	}
	Check(Hit == Lost && BadOrder == 0, "losses: as many planes hit as the rules lost, each hit once and into the ground once after");
	Check(BadFall == 0 && Explodes == 1, "losses: on fire a long dive, a spin steep and quick (about 3 s from 150 m), blown up gone at once");
	Check(NoWreck == 0, "losses: a wreck where each burning or spinning plane struck, the rest of the battle");
	Check(OtherFell == 0, "losses: no other plane falls");

	// ---- ground
	std::printf("     ground fire: %d gun frames (by leg %d %d %d %d)\n", GunFrames, GunsByLeg[0], GunsByLeg[1], GunsByLeg[2], GunsByLeg[3]);
	Check(GunFrames > 0 && GunsByLeg[0] > 0 && GunsByLeg[1] > 0 && GunsByLeg[2] + GunsByLeg[3] > 0, "ground: each strike fired on");
	Check(OwnSideGuns == 0, "ground: only the other side's blocks fire (a block with no strength never)");
	std::printf("     (streams with no plane %d, over 150 m from one %d, under 8 m up %d)\n", NoPlaneGuns, FarStreams, LowStreams);
	Check(NoPlaneGuns == 0 && FarStreams == 0 && LowStreams == 0, "ground: each stream ends in the air near a plane in reach (150 m: short of its lead)");

	// ---- same
	{
		Sky B;
		B.Build(Rec, Flat, Life);
		uint64_t H2 = 1469598103934665603ull;
		for (double T = T0; T <= T1; T += Dt) { B.At(T, Ps, Gs); H2 = Hash(Ps, Gs, H2); }
		std::printf("     planes hash %016llx (as before the chutes' notes: e258eaf3320235ff)\n", static_cast<unsigned long long>(H1));
		Check(H2 == H1, "same: built twice, the same planes and guns frame for frame");
		// (a live battle's notes come a few at a time: the first leg flown the same whatever came after)
		LandFeed::Recording Part;
		LandFeed::ReadRecording(Recording(true, 3), Part, Err);
		Sky C;
		C.Build(Part, Flat, Life);
		bool bSame = true;
		for (double T = T0; T <= T1; T += 0.5)
		{
			std::vector<Plane> A1, A2;
			std::vector<Gunner> G1, G2;
			S.At(T, A1, G1);
			C.At(T, A2, G2);
			std::vector<Plane> F1, F2;
			for (const Plane& P : A1) if (P.Leg == 0) F1.push_back(P);
			for (const Plane& P : A2) if (P.Leg == 0) F2.push_back(P);
			bSame = bSame && Hash(F1, {}, 1) == Hash(F2, {}, 1);
		}
		Check(bSame, "same: grown note by note as a live battle grows, its first leg flown the same");
		int Coarse = 0;
		const int Frames = static_cast<int>(std::ceil((T1 - T0) * 23.0));
		for (int F = 0; F < Frames; ++F) Coarse += S.Rounds(T0 + F / 23.0, std::min(T1, T0 + (F + 1) / 23.0), 22.0);
		std::printf("     rounds at 22 a second: %d whole, %d at 23 frames a second, %d at 60\n", S.Rounds(T0, T1, 22.0), Coarse, RoundsSum);
		Check(Coarse == S.Rounds(T0, T1, 22.0) && std::abs(RoundsSum - S.Rounds(T0, T1, 22.0)) <= 2, "same: the rounds the same however the frames fall");
		Check(Nans == 0, "same: no NaN");
	}
	// ---- chutes (-TheaterLandChutes): the burning plane's and the spinning plane's pilots get out (the rules' chutes),
	// the blown-up one's not; and the art director's notes of 24 November, 02:26Z, on them and on a second field (an
	// Il-2's pilot out low, a Mustang's, in a wind over 4 m/s)
	{
		Check(S.ChutesPlanned() == 2, "chutes: a pilot out of each plane the rules say he got out of, none out of the one blown apart");
		LandFeed::Recording WindyRec;
		const std::string WindySky = "\"sky\": {\"wind\": [" + std::to_string(WindyX) + ", " + std::to_string(WindyY) + "]}, ";
		LandFeed::ReadRecording(Recording(true, 100, false, Windy, 2, WindySky.c_str()), WindyRec, Err);
		Sky S2;
		S2.Build(WindyRec, Flat, Life);
		struct Watched
		{
			double First = -1.0, OutHigh = 0.0, OpenHigh = -1.0, LandT = -1.0, MostStep = 0.0, MinSink = 1e9, MaxSink = 0.0;
			V3 Was, FirstAt, LandAt;
			bool bWas = false;
			std::string Clips;
			double FirstFromPlane = 1e9;
			int Under = 0;
			double HeapOff = 0.0, SlumpEnd = 0.0, EnemyAtLand = 0.0, EnemyAtEnd = 0.0;
			// (the art director's notes: falling free, above where he left her or not half g t squared under it; his
			// canopy full (real seconds after the first frame) and how far he had dropped by then, and by the time he
			// was first within 1 m/s of the wind; his way through the air growing again, or off the wind's 3 s after his
			// canopy was full; as he lands, his canopy's open share;
			// the swap (his going over and his canopy's slump then); the clips' times; how far he was dragged and how
			// nearly with the wind (the cosine); his run's speed; his swing's crossings and its most early and late)
			int Rose = 0, Shallow = 0, Grew = 0, OffWind = 0, Swapped = 0, BadSwap = 0;
			double FullT = -1.0, DropToFull = 0.0, LastRel = 1e18, OpenDown = -1.0, WithAir = -1.0, LandRel = -1.0;
			// (and when he pulled and when he was first within 1 m/s of the wind, real seconds after the first frame)
			double PullT = -1.0, WithAirT = -1.0;
			bool bHard = false, bInMeshWas = false;
			double FallenWas = 0.0, SwapSlump = -1.0;
			double KneelFrom = -1.0, KneelTo = -1.0, BackFrom = -1.0, RiseT = -1.0, DragMost = 0.0, DragWay = 0.0, DragDone = -1.0;
			double RunFrom = -1.0, RunTo = -1.0, RunDist = 0.0;
			V3 RunStart, RunLast;
			std::vector<double> Crossings;
			double SwingEarly = 0.0, SwingLate = 0.0, SwingLateLeast = 1e9, LastSwing = 0.0, SwingT0 = -1.0;
			Chute Look;
			// (his chute as he lands: his canopy as it falls in and lies, his facing, where its lines meet him)
			Chute Lies;
			// (a review of 24 November: the way he faced as he landed, kept while he is down; his clips' blends)
			double FaceAt = 1e9;
			int FaceMoved = 0, BlendBad = 0;
		};
		auto Play = [&](const Sky& Sk, const LandFeed::Recording& R, double Wx, double Wy, std::map<int, Watched>& Sn, std::ofstream* To, int Tag)
		{
			// (for chutes.py: the field's wind and life, and each pilot's frames under his plane's key, Tag over it)
			if (To && To->is_open()) *To << "w " << Tag << " " << Wx << " " << Wy << " " << Life << "\n";
			std::vector<Happen> Hc;
			Sk.Between(0.0, 400.0, Hc);
			std::map<int, double> Hits;
			for (const Happen& H : Hc) if (H.Kind == Happen::What::Hit) Hits[H.Key] = H.T;
			const double Dc = 1.0 / 60.0;
			auto NearestEnemy = [&](const V3& At, int Side)
			{
				double Best = 1e18;
				const LandFeed::View* V = R.ViewAt(200.0);
				if (V) for (const LandFeed::Unit& U : V->Units) if (U.Side >= 0 && U.Side != Side && U.Str > 0.05) Best = std::min(Best, std::hypot(U.X - At.X, U.Y - At.Y));
				return Best;
			};
			std::vector<Chute> Cs;
			std::vector<Plane> Pl;
			std::vector<Gunner> Gn;
			for (double T = T0; T <= 400.0; T += Dc)
			{
				Sk.Chutes(T, Cs);
				Sk.At(T, Pl, Gn);
				for (const Chute& C : Cs)
				{
					if (To && To->is_open() && std::fmod(T, 0.05) < Dc)
						*To << "c " << C.Key + Tag << " " << T << " " << C.At.X << " " << C.At.Y << " " << C.At.Z << " " << C.Open << " " << C.Swing << " " << (C.bDown ? 1 : 0) << " " << (C.Clip.empty() ? "-" : C.Clip)
							<< " " << C.Heap.X << " " << C.Heap.Y << " " << C.Slump << " " << (C.bInMesh ? 1 : 0) << " " << C.Fallen << " " << C.Rate << " " << (C.bHard ? 1 : 0) << " " << C.Dragged
							<< " " << C.Time << " " << C.Vel.X << " " << C.Vel.Y << " " << C.Vel.Z << "\n";
					Watched& W = Sn[C.Key];
					const double Over = C.At.Z - Flat(C.At.X, C.At.Y);
					if (W.First < 0.0)
					{
						W.First = T;
						W.OutHigh = Over;
						W.FirstAt = C.At;
						W.Look = C;
						for (const Plane& P : Pl) if (P.Key == C.Key) W.FirstFromPlane = Length(Plus(P.At, Mul(C.At, -1.0)));
						if (Hits.count(C.Key)) W.SwingT0 = Hits[C.Key];
					}
					if (!std::isfinite(C.At.X) || !std::isfinite(C.At.Z) || !std::isfinite(C.Swing) || !std::isfinite(C.Time) || !std::isfinite(C.Rate)) ++Nans;
					W.Under += Over < -0.01;
					if (W.bWas) W.MostStep = std::max(W.MostStep, Length(Plus(C.At, Mul(W.Was, -1.0))) / (Dc * Life));
					W.Was = C.At;
					W.bWas = true;
					if (!C.bDown)
					{
						const double Since0 = (T - W.First) * Life;
						// (her first note: falling free he is never above where he left her, and at least half g t squared under)
						if (C.Open <= 0.0)
						{
							W.Rose += C.At.Z > W.FirstAt.Z + 1e-9;
							W.Shallow += C.At.Z > W.FirstAt.Z - 0.5 * 9.81 * Since0 * Since0 + 1e-6;
						}
						// (her second, in her words: "Once his own way is spent, he drifts with the air within 2-3 s. The wind
						// isn't a push that builds from rest." His way through the air only ever dies away, he is with the
						// wind within 3 s of his pull, and still with it 3 s after his canopy is full. Her first sentence,
						// "drops about 50 m straight down before the wind takes him", was release 1's fault, as her first
						// note's "rises about 7 m" was)
						const double Rel = std::hypot(C.Vel.X - Wx, C.Vel.Y - Wy);
						W.Grew += Rel > W.LastRel * (1.0 + 1e-9) + 1e-9;
						W.LastRel = Rel;
						if (W.WithAir < 0.0 && Rel < 1.0) { W.WithAir = W.FirstAt.Z - C.At.Z; W.WithAirT = Since0; }
						if (W.PullT < 0.0 && C.Open > 0.0) W.PullT = Since0;
						if (C.Open >= 1.0 && W.FullT < 0.0) { W.FullT = Since0; W.DropToFull = W.FirstAt.Z - C.At.Z; }
						if (W.FullT >= 0.0 && Since0 >= W.FullT + 3.0 && Over > 2.0) W.OffWind += Rel > 1.0;
						if (C.Open >= 1.0) W.OpenHigh = W.OpenHigh < 0.0 ? Over : W.OpenHigh;
						if (C.Open >= 1.0 && T > W.First + 8.0 / Life && Over > 3.0) { W.MinSink = std::min(W.MinSink, -C.Vel.Z); W.MaxSink = std::max(W.MaxSink, -C.Vel.Z); }
						// (the swing: its crossings, its most in its first 2.5 s and after 25 s, and its least over each 4 s then)
						if (W.FullT >= 0.0)
						{
							const double After = Since0 - W.FullT;
							if (W.LastSwing * C.Swing < 0.0) W.Crossings.push_back(After);
							W.LastSwing = C.Swing != 0.0 ? C.Swing : W.LastSwing;
							if (After < 2.5) W.SwingEarly = std::max(W.SwingEarly, std::fabs(C.Swing));
							if (After > 25.0) W.SwingLate = std::max(W.SwingLate, std::fabs(C.Swing));
						}
						continue;
					}
					if (W.LandT < 0.0)
					{
						W.LandT = T;
						W.LandAt = C.Landed;
						W.EnemyAtLand = NearestEnemy(C.At, C.Side);
						W.HeapOff = std::hypot(C.Heap.X - C.Landed.X, C.Heap.Y - C.Landed.Y);
						W.Lies = C;
						W.OpenDown = C.Open;
						W.LandRel = W.LastRel;
						W.bHard = C.bHard;
						W.bInMeshWas = true;
					}
					const double Down = (T - W.LandT) * Life;
					// (her third note: swapped for the crowd's man as he lies flat under his falling canopy)
					if (W.bInMeshWas && !C.bInMesh)
					{
						++W.Swapped;
						W.SwapSlump = C.Slump;
						W.BadSwap += W.FallenWas < 0.999 || C.Slump < 0.1 || C.Slump > 0.7;
					}
					W.bInMeshWas = C.bInMesh;
					W.FallenWas = C.Fallen;
					if (W.FaceAt > 1e8) W.FaceAt = C.Face;
					W.FaceMoved += C.Face != W.FaceAt;
					if (C.bInMesh) continue;
					W.BlendBad += C.Blend != (C.Clip == "kneel_down" ? 0.5 : 0.25);
					if (W.Clips.empty() || W.Clips.substr(W.Clips.rfind(' ') + 1) != C.Clip) W.Clips += " " + C.Clip;
					// (her fourth: dragged, then back up onto his knees, his harness worked free, up)
					if (C.Dragged > W.DragMost)
					{
						W.DragMost = C.Dragged;
						const double Dx = C.Landed.X - W.LandAt.X, Dy = C.Landed.Y - W.LandAt.Y;
						W.DragWay = (Dx * Wx + Dy * Wy) / std::max(1e-9, std::hypot(Dx, Dy) * std::hypot(Wx, Wy));
						W.DragDone = Down;
					}
					if (C.Clip == "go_prone" && C.Rate < 0.0 && W.BackFrom < 0.0) W.BackFrom = Down;
					if (C.Clip == "go_prone" && W.RiseT < 0.0 && (C.Rate < 0.0 || C.Time < 1.9 - 1e-9) && W.KneelFrom < 0.0) W.RiseT = Down;
					if (C.Clip == "kneel_down") { if (W.KneelFrom < 0.0) W.KneelFrom = Down; W.KneelTo = Down; }
					if (C.Clip == "run")
					{
						if (W.RunFrom < 0.0) { W.RunFrom = Down; W.RunStart = C.At; }
						W.RunTo = Down;
						W.RunLast = C.At;
					}
					W.SlumpEnd = C.Slump;
					W.EnemyAtEnd = NearestEnemy(C.At, C.Side);
				}
			}
		};
		std::map<int, Watched> Sn, Sw;
		Play(S, Rec, 2.6, 1.2, Sn, Out.is_open() ? &Out : nullptr, 0);
		Play(S2, WindyRec, WindyX, WindyY, Sw, Out.is_open() ? &Out : nullptr, 100);
		std::map<int, double> HitT, CrashT;
		std::vector<Happen> Hc;
		S.Between(0.0, 400.0, Hc);
		for (const Happen& H : Hc) { if (H.Kind == Happen::What::Hit) HitT[H.Key] = H.T; else CrashT[H.Key] = H.T; }
		int Bad = 0;
		for (const auto& Pair : Sn)
		{
			const Watched& W = Pair.second;
			const double HitAtT = HitT.count(Pair.first) ? HitT[Pair.first] : -1.0, CrashAtT = CrashT.count(Pair.first) ? CrashT[Pair.first] : -1.0;
			std::printf("     pilot of plane %d: out %.2f s after the hit (%.0f m up, %.1f m from her), canopy full %.0f m up, down at %.1f m/s to %.1f, landed %.0f s after he left;%s; canopy %.1f m downwind, fallen in %.2f; %.0f m from the nearest enemy, then %.0f; step %.0f m/s, under %d\n",
				Pair.first, (W.First - HitAtT) * Life, W.OutHigh, W.FirstFromPlane, W.OpenHigh, W.MinSink, W.MaxSink, (W.LandT - W.First) * Life, W.Clips.c_str(), W.HeapOff, W.SlumpEnd, W.EnemyAtLand, W.EnemyAtEnd, W.MostStep, W.Under);
			const bool bOk = HitAtT >= 0.0 && W.First > HitAtT && W.First < CrashAtT && W.FirstFromPlane < 6.0 && W.OutHigh >= 30.0 && W.OpenHigh > 15.0
				&& W.MinSink >= 4.5 && W.MaxSink <= 7.0 && W.LandT > W.First && W.Under == 0 && W.MostStep < 170.0
				&& W.Clips == " go_prone kneel_down stand_up run go_prone" && W.HeapOff > 4.5 && W.HeapOff < 9.5 && W.SlumpEnd > 0.999 && W.EnemyAtEnd > W.EnemyAtLand + 15.0;
			Bad += !bOk;
		}
		Check(Sn.size() == 2 && Bad == 0, "chutes: out of her just after the hit and before she strikes, his canopy full well up, down at 5 to 6.5 m/s, never under the ground; down, flat, up onto his knees, his harness, up, running from the enemy, flat; his canopy fallen in downwind, its middle 4.5 to 9.5 m off");
		Sky B2;
		B2.Build(Rec, Flat, Life);
		bool bSameChutes = true;
		std::vector<Chute> C1, C2;
		for (double T = T0; T <= 400.0; T += 0.25)
		{
			S.Chutes(T, C1);
			B2.Chutes(T, C2);
			bSameChutes = bSameChutes && C1.size() == C2.size();
			for (size_t K = 0; K < C1.size() && K < C2.size(); ++K) bSameChutes = bSameChutes && C1[K].At.X == C2[K].At.X && C1[K].At.Z == C2[K].At.Z && C1[K].Clip == C2[K].Clip;
		}
		Check(bSameChutes, "chutes: the same twice");
		Check(Nans == 0, "chutes: no NaN");

		// The art director's notes (24 November, 02:26Z), on both fields' pilots.
		std::vector<std::pair<int, const Watched*>> All;
		for (const auto& Pair : Sn) All.emplace_back(Pair.first, &Pair.second);
		for (const auto& Pair : Sw) All.emplace_back(100 + Pair.first, &Pair.second);
		int Rose = 0, Shallow = 0, Grew = 0, OffWind = 0, FarOff = 0, BadSwap = 0, Swapped = 0, Low = 0, HardBad = 0, KneelBad = 0, DragBad = 0, SwingBad = 0, LookBad = 0;
		int FaceBad = 0, BlendBad = 0;
		for (const auto& Each : All)
		{
			const Watched& W = *Each.second;
			const bool bWindy = Each.first >= 100, bLow = W.OutHigh < 60.0;
			const double Lie = W.RiseT, Kneeled = W.KneelTo - W.KneelFrom, RunSpeed = W.RunTo > W.RunFrom ? std::hypot(W.RunLast.X - W.RunStart.X, W.RunLast.Y - W.RunStart.Y) / (W.RunTo - W.RunFrom) : 0.0;
			double Period = 0.0;
			if (W.Crossings.size() >= 5) Period = 2.0 * (W.Crossings.back() - W.Crossings.front()) / static_cast<double>(W.Crossings.size() - 1);
			std::printf("     %s pilot of plane %d: out %.0f m up%s; free fall rose %d, under half g t^2 %d; canopy full %.1f s out, %.0f m down; pulled %.1f s out, with the wind %.1f s out (%.1f s after his pull), %.0f m down; way grew %d, off the wind %d; landed %s (canopy %.2f open, %.1f m/s through the air); swapped %d at slump %.2f; lay %.1f s, back up from %.1f s, kneeling %.2f s; dragged %.2f m (%.2f with the wind); run %.1f m/s; swing %.2f s a swing, %.1f deg at first, %.1f late\n",
				bWindy ? "windy:" : "calm: ", Each.first % 100, W.OutHigh, bLow ? " (low)" : "", W.Rose, W.Shallow, W.FullT, W.DropToFull, W.PullT, W.WithAirT, W.WithAirT - W.PullT, W.WithAir, W.Grew, W.OffWind, W.bHard ? "hard" : "soft", W.OpenDown, W.LandRel,
				W.Swapped, W.SwapSlump, Lie, W.BackFrom, Kneeled, W.DragMost, W.DragWay, RunSpeed, Period, W.SwingEarly, W.SwingLate);
			Rose += W.Rose;
			Shallow += W.Shallow;
			Grew += W.Grew;
			OffWind += W.OffWind;
			// (his canopy full 40 to 75 m under where he left her, its opening's loss; with the air within 3 s of his pull)
			if (!bLow) FarOff += W.FullT < 0.0 || W.DropToFull < 40.0 || W.DropToFull > 75.0 || W.PullT < 0.0 || W.WithAirT < 0.0 || W.WithAirT - W.PullT > 3.0 + 0.05;
			BadSwap += W.BadSwap;
			Swapped += W.Swapped == 1;
			FaceBad += W.FaceMoved;
			BlendBad += W.BlendBad;
			KneelBad += Kneeled < 1.0 - 0.03 || Kneeled > 1.5 + 0.03 || W.BackFrom < 0.0 || W.BackFrom >= W.KneelFrom || W.Clips.find(" kneel_down stand_up ") == std::string::npos;
			if (bLow)
			{
				++Low;
				HardBad += !W.bHard || W.OpenDown >= 1.0 || W.LandRel > 8.0 || Lie < 3.0 - 0.03 || Lie > 5.0 + 0.03 || RunSpeed < 1.2 || RunSpeed > 0.7 * 4.2 || W.EnemyAtEnd < W.EnemyAtLand + 6.0;
			}
			else
			{
				HardBad += W.bHard || W.OpenDown < 1.0 || RunSpeed < 4.1;
				Period = Period > 0.0 ? Period : -1.0;
				SwingBad += Period < 3.8 || Period > 4.4 || W.SwingEarly > 14.5 || W.SwingEarly < 10.0 || (W.SwingLate > 0.0 && (W.SwingLate < 3.0 || W.SwingLate > 6.5));
			}
			DragBad += bWindy ? W.DragMost < 1.0 - 1e-6 || W.DragMost > 2.0 + 1e-6 || W.DragDone < 0.0 || W.DragDone > W.BackFrom || W.DragWay < 0.99 : W.DragMost > 0.0;
			// (her sixth note: his canopy's look, full)
			SilkMesh M;
			CanopyShape(W.Look, 1.0, M);
			double MinX = 1e9, MaxX = -1e9, MinZ = 1e9, MaxZ = -1e9, Seam = 1e9, Panel = 0.0;
			for (size_t K = 0; K < M.Points.size(); ++K)
			{
				MinX = std::min(MinX, M.Points[K].X); MaxX = std::max(MaxX, M.Points[K].X);
				MinZ = std::min(MinZ, M.Points[K].Z); MaxZ = std::max(MaxZ, M.Points[K].Z);
				Seam = std::min(Seam, M.Colours[K * 3 + 1]);
				Panel = std::max(Panel, M.Colours[K * 3 + 1]);
			}
			int Up = 0, DownFacing = 0;
			for (const V3& O : M.Outward) { Up += O.Z > 0.2; DownFacing += O.Z < -0.2; }
			// (its lines' ends: one at each seam, on its skirt, its lowest)
			double SeamLow = -1e9;
			for (const int K : M.Seams) SeamLow = std::max(SeamLow, M.Points[static_cast<size_t>(K)].Z);
			const double Across = MaxX - MinX, Deep = MaxZ - MinZ, Warm = W.Look.Silk[0] / 0.86 - 1.0, Cool = 1.0 - W.Look.Silk[2] / 0.80;
			std::printf("        his canopy: %.2f m across, %.2f deep, %d gores (%zu points, %zu triangles, %d facing up and %d down), seams %.0f%% darker, silk %.3f %.3f %.3f\n",
				Across, Deep, W.Look.Gores, M.Points.size(), M.Triangles.size() / 3, Up, DownFacing, (1.0 - Seam / Panel) * 100.0, W.Look.Silk[0], W.Look.Silk[1], W.Look.Silk[2]);
			LookBad += Across < 4.9 || Across > 5.6 || Deep < 0.42 * Across || Deep > 0.55 * Across || W.Look.Gores < 24 || W.Look.Gores > 28 || W.Look.Gores % 2
				|| (1.0 - Seam / Panel) < 0.05 || (1.0 - Seam / Panel) > 0.2 || std::fabs(Warm) > 0.0401 || std::fabs(Cool) > 0.0401 || W.Look.Silk[1] != 0.85
				|| static_cast<int>(M.Triangles.size() / 3) != static_cast<int>(M.Outward.size()) || Up < 100 || DownFacing < 100
				|| static_cast<int>(M.Seams.size()) != W.Look.Gores || SeamLow > MinZ + 1e-9;
		}
		Check(Rose == 0 && Shallow == 0, "chutes: falling free he is never above where he left her, and at least half g t squared under it (the art director's first note)");
		Check(FarOff == 0 && Grew == 0 && OffWind == 0, "chutes: with the air (within 1 m/s of the wind) within 3 s of his pull, his way through the air only dying away (the wind no push from rest), and with the wind still 3 s after his canopy is full (her second); his canopy full 40 to 75 m under where he left her");
		Check(BadSwap == 0 && Swapped == static_cast<int>(All.size()), "chutes: drawn in his chute's mesh until he lies flat under his falling canopy, then the crowd's man (her third)");
		Check(KneelBad == 0, "chutes: up onto his knees (go_prone played back), his harness worked free kneeling 1 to 1.5 s (kneel_down's last pose), then stand_up (her fourth)");
		Check(BlendBad == 0 && FaceBad == 0, "chutes: each clip blended into over 0.25 s, the harness's kneel over 0.5 s (go_prone's knees far from it); he lies, and his harness, the way he faced as he landed");
		Check(DragBad == 0, "chutes: in a wind over 4 m/s his canopy drags him 1 to 2 m before he gets up; in a lighter one not at all (her fourth)");
		Check(Low >= 1 && HardBad == 0, "chutes: out under 60 m his canopy is still filling as he lands, hard (but its way spent: 8 m/s through the air at most); he lies 3 to 5 s and limps off slower; the rest land soft and run (her fifth)");
		Check(SwingBad == 0, "chutes: his swing about 4 s a swing, from 14 degrees settling to about 5 and never dead still (her answers)");
		Check(LookBad == 0, "chutes: his canopy a dome 5 to 5.5 m across and about half as deep, on 24 to 28 gores with faint darker seams, a line at each, both its faces; its silk her off-white within 4% (her sixth)");
		// (the glow: looking up into it a little, against the sun most, the sun behind the eye or under the field none)
		const double Below = Glow({ 0.0, 0.0, 0.0 }, { 0.0, 100.0, 100.0 }, { 0.0, -0.6, 0.8 }), Against = Glow({ 0.0, 0.0, 0.0 }, { 0.0, 60.0, 80.0 }, { 0.0, 0.6, 0.8 });
		const double Behind = Glow({ 0.0, 0.0, 300.0 }, { 0.0, 100.0, 100.0 }, { 0.0, -0.6, 0.8 }), Night = Glow({ 0.0, 0.0, 0.0 }, { 0.0, 60.0, 80.0 }, { 0.0, 0.6, -0.1 });
		std::printf("     glow: seen from under it %.2f, against the sun %.2f, the sun behind the eye %.2f, at night %.2f\n", Below, Against, Behind, Night);
		Check(Below > 0.05 && Below < 0.3 && Against > 0.8 && Behind < 1e-9 && Night == 0.0, "chutes: his canopy glows against the sun, a little seen from under it, not with the sun behind the eye nor at night (her sixth)");

		// (moved in place, a triangle keeps the way round it was first wound, which the scene chooses by where its points
		// are: so a canopy filling must never fold one over; down it is made again, its rumple folding some)
		auto Windings = [](const SilkMesh& M, std::vector<int>& Into)
		{
			Into.clear();
			for (size_t K = 0; K + 2 < M.Triangles.size(); K += 3)
			{
				const V3& A = M.Points[static_cast<size_t>(M.Triangles[K])], &B = M.Points[static_cast<size_t>(M.Triangles[K + 1])], &Cn = M.Points[static_cast<size_t>(M.Triangles[K + 2])];
				const V3 Ux = Plus(B, Mul(Cn, -1.0)), Vx = Plus(A, Mul(Cn, -1.0));
				const V3 Nx{ Ux.Y * Vx.Z - Ux.Z * Vx.Y, Ux.Z * Vx.X - Ux.X * Vx.Z, Ux.X * Vx.Y - Ux.Y * Vx.X };
				const V3& O = M.Outward[K / 3];
				Into.push_back(Nx.X * O.X + Nx.Y * O.Y + Nx.Z * O.Z >= 0.0 ? 1 : -1);
			}
		};
		// The scene's chute mesh at 45 frames a second (her sixth note, and a review of 24 November: it had been made
		// again 84 to 101 times a pilot, 54 of them as his canopy fell in). LandAir's steps as the scene takes them: a
		// section made again when its make changes (him alone; him, his lines and silk; down, with him going over; once
		// the crowd's man has him), and down its silk each time it moves a step; else moved in place. With the crowd posed
		// before the chutes and drawn after them as in the scene (a ride set one frame shows the next, and only once that
		// frame's rebuilds let the crowd make his mesh: here 0, 3 and 25 frames late), so that the swap shows him every
		// frame, never neither, and twice in place for one frame at most (the frame the crowd first draws him, before
		// the chutes have seen it: a second review of 24 November). And (a second review of 24 November: made again each 0.05 of
		// its falling in and held still between, it had jumped up to 1.3 m at a make, some eleven times a second) down,
		// between two makes, his silk moved in place toward the next step's shape (LandAir::SilkStep, as the scene keeps
		// it): how far its points move in a frame at most (and at a make, from where they were drawn), how far the drawn
		// shape is from the true one between makes, and how many of its triangles show their inside for a frame (wound
		// half-way through their step). And each make's cost: its silk's points and triangles lying, and its time here.
		// And his lines as the scene draws them down (DownShape: LineLies from his harness as he goes over, as slack as
		// they are), each built again when his part is: how far their points move in a frame at most, between makes and at
		// one, and how often a line's make changes (its points' count) once he is down.
		int MadeMost = 0, AirMade = 0, Gaps = 0, TwiceMost = 0, LinesRemade = 0;
		const int Lates[] = { 0, 3, 25 };
		double FrameMost = 0.0, MakeJumpMost = 0.0, AwayMost = 0.0, MakeMs = 0.0, MakeMsMost = 0.0, LineFrameMost = 0.0, LineMakeMost = 0.0;
		long InsideFrames = 0, TriangleFrames = 0;
		int Makes = 0;
		size_t LyingPoints = 0, LyingTriangles = 0, AirPoints = 0, AirTriangles = 0;
		int LyingGores = 0;
		for (int Pass = 0; Pass < 6; ++Pass)
		{
			const int Field = Pass % 2, Late = Lates[Pass / 2];
			struct Kept
			{
				double Shape = -1.0, Lying = -9.0;
				int Make0 = -1, Make1 = -1, Made = 0, Moved = 0, SlumpMade = 0, MadeAloft = 0;
				bool bRide = false, bSeen = false;
				int RideFrame = -1, Held = 0, Twice = 0;
				SilkStep Step;
				std::vector<V3> Drawn, Lines;
				std::vector<int> Wound, LineCounts;
				double SlumpDrawn = -1.0, Most = 0.0;
			};
			std::map<int, Kept> Ks;
			std::vector<Chute> Now;
			SilkMesh Here, True, Mid, LinesOn;
			int Frame = 0;
			for (double Tf = T0; Tf <= 400.0; Tf += 1.0 / (45.0 * Life), ++Frame)
			{
				(Field ? S2 : S).Chutes(Tf, Now);
				for (const Chute& C : Now)
				{
					Kept& K = Ks[C.Key];
					// (the crowd draws him this frame, after the chutes: from the frame after his ride was set, Late frames on;
					// the chutes see last frame's drawing)
					const auto CrowdDraws = [&](int F) { return K.RideFrame >= 0 && F >= K.RideFrame + 1 + Late; };
					const bool bCrowd = CrowdDraws(Frame);
					if (K.bRide && !K.bSeen) K.bSeen = CrowdDraws(Frame - 1) || ++K.Held > LandAir::RiderHoldFrames;
					const double Shape = C.bDown ? 2.0 + C.Slump : C.Open;
					const double Lying = C.bDown && (C.bInMesh || !K.bRide || !K.bSeen) ? (C.bInMesh ? C.Fallen : 1.0) : -1.0;
					const bool bSilk = SilkMoved(Shape, K.Shape) || (C.bDown && K.Step.Due(C.Slump)), bMan = ManMoved(Lying, K.Lying);
					const bool bStep = !bSilk && C.bDown && K.Step.Moving() && std::fabs(C.Slump - K.SlumpDrawn) > 1e-9;
					if (bSilk || bMan || bStep)
					{
						const double SilkAt = bSilk ? Shape : K.Shape;
						const int Make0 = C.bDown ? (Lying >= 0.0 ? 2 : 3) : (SilkAt > 0.0 ? 1 : 0), Make1 = C.bDown || SilkAt > 0.0 ? 1 : 0;
						const bool bMade = Make0 != K.Make0 || (bSilk && (Make1 != K.Make1 || (Make1 == 1 && C.bDown)));
						K.Made += bMade;
						K.Moved += !bMade;
						K.SlumpMade += bMade && C.bDown && C.Slump > 0.0 && C.Slump < 1.0;
						K.MadeAloft += bMade && !C.bDown && K.Make1 == 1;
						K.Make0 = Make0;
						if (bSilk) { K.Make1 = Make1; K.Shape = Shape; }
						if (bMan) K.Lying = Lying;
						if (C.bDown && (bSilk || bStep))
						{
							const double Off = std::hypot(C.Heap.X - C.Landed.X, C.Heap.Y - C.Landed.Y);
							if (bSilk)
							{
								const auto Began = std::chrono::steady_clock::now();
								K.Step.Make(C, C.Slump, Off, GroundFn());
								const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Began).count();
								MakeMs += Ms;
								MakeMsMost = std::max(MakeMsMost, Ms);
								++Makes;
								K.Step.Wound(Mid);
								Windings(Mid, K.Wound);
								LyingPoints = K.Step.From.Points.size();
								LyingTriangles = K.Step.From.Triangles.size() / 3;
								LyingGores = C.Gores;
								CanopyShape(C, 1.0, True);
								AirPoints = True.Points.size();
								AirTriangles = True.Triangles.size() / 3;
							}
							K.Step.At(C.Slump, Here);
							double Moved = 0.0;
							for (size_t P = 0; P < Here.Points.size() && P < K.Drawn.size(); ++P) Moved = std::max(Moved, Length(Plus(Here.Points[P], Mul(K.Drawn[P], -1.0))));
							K.Most = std::max(K.Most, Moved);
							(bSilk ? MakeJumpMost : FrameMost) = std::max(bSilk ? MakeJumpMost : FrameMost, Moved);
							K.Drawn = Here.Points;
							K.SlumpDrawn = C.Slump;
							if (bStep)
							{
								// (the true shape there, against the drawn one; and each triangle drawn as wound at its make, against
								// the way round it is now)
								HeapShape(C, C.Slump, Off, True);
								for (size_t P = 0; P < True.Points.size() && P < Here.Points.size(); ++P) AwayMost = std::max(AwayMost, Length(Plus(True.Points[P], Mul(Here.Points[P], -1.0))));
								True.Points = Here.Points;
								std::vector<int> Way;
								Windings(True, Way);
								for (size_t T = 0; T < Way.size() && T < K.Wound.size(); ++T) InsideFrames += Way[T] != K.Wound[T];
								TriangleFrames += static_cast<long>(Way.size());
							}
						}
						if (C.bDown && K.Step.FromSlump >= 0.0)
						{
							// (his lines as DownShape draws them this frame, against those it drew last)
							K.Step.At(C.Slump, LinesOn);
							const V3 H0 = OverHarness(Lying >= 0.0 ? Lying : 1.0, ProneEndX, ProneEndY);
							const double Turn2 = (C.Face - C.HeapHeading) * 3.14159265358979323846 / 180.0;
							const V3 H{ H0.X * std::cos(Turn2) - H0.Y * std::sin(Turn2), H0.X * std::sin(Turn2) + H0.Y * std::cos(Turn2), H0.Z };
							std::vector<V3> LinesNow;
							std::vector<int> CountsNow;
							for (size_t Sm = 0; Sm < LinesOn.Seams.size(); ++Sm)
							{
								if (!LineBack(Sm, LinesOn.Seams.size())) continue;
								V3 Way[LinePoints];
								const int Points = LineLies(LinesOn, Sm, H, LineSlack(C.Slump), Way);
								LinesNow.insert(LinesNow.end(), Way, Way + Points);
								CountsNow.push_back(Points);
							}
							if (!K.LineCounts.empty() && CountsNow != K.LineCounts) ++LinesRemade;
							else if (!K.LineCounts.empty())
							{
								double Moved = 0.0;
								for (size_t P = 0; P < LinesNow.size(); ++P) Moved = std::max(Moved, Length(Plus(LinesNow[P], Mul(K.Lines[P], -1.0))));
								(bSilk ? LineMakeMost : LineFrameMost) = std::max(bSilk ? LineMakeMost : LineFrameMost, Moved);
							}
							K.Lines.swap(LinesNow);
							K.LineCounts.swap(CountsNow);
						}
					}
					if (C.bDown)
					{
						const bool bMesh = K.Lying >= 0.0;
						Gaps += !bMesh && !bCrowd;
						K.Twice += bMesh && bCrowd;
					}
					if (!C.bInMesh && !K.bRide) { K.bRide = true; K.RideFrame = Frame; }
				}
			}
			for (const auto& Each : Ks)
			{
				if (Late == 0)
				{
					std::printf("     %s pilot of plane %d's mesh at 45 fps: made %d times (%d as his canopy falls in), moved in place %d; his silk moved %.2f m in a frame at most\n", Field ? "windy:" : "calm: ", Each.first,
						Each.second.Made, Each.second.SlumpMade, Each.second.Moved, Each.second.Most);
				}
				MadeMost = std::max(MadeMost, Each.second.Made);
				AirMade += Each.second.MadeAloft;
				TwiceMost = std::max(TwiceMost, Each.second.Twice);
			}
		}
		std::printf("     the swap with the crowd's mesh 0, 3 and 25 frames late: %d frames with him drawn nowhere, twice in place %d at most a pilot (held at most %d)\n", Gaps, TwiceMost,
			LandAir::RiderHoldFrames);
		std::printf("     falling in at 45 fps: its silk moved in place between makes, %.2f m in a frame at most (%.1f m/s), %.2f m at a make; drawn within %.3f m of its true shape; %.2f%% of its triangles' frames showing their inside (wound half-way through their step)\n",
			FrameMost, FrameMost * 45.0, MakeJumpMost, AwayMost, 100.0 * static_cast<double>(InsideFrames) / static_cast<double>(std::max(1L, TriangleFrames)));
		std::printf("     falling in at 45 fps, his lines as drawn (from his harness as he goes over, as slack as they are): %.2f m in a frame at most between makes, %.2f m at a make; %d times a line's make changed\n",
			LineFrameMost, LineMakeMost, LinesRemade);
		std::printf("     lying his silk is %zu points and %zu triangles on %d gores (in the air %zu and %zu); a make (its shape and the next step's, a flat field) %.2f ms on average and %.2f at most here\n",
			LyingPoints, LyingTriangles, LyingGores, AirPoints, AirTriangles, MakeMs / std::max(1, Makes), MakeMsMost);
		Check(MadeMost <= 30 && AirMade == 0, "chutes: his mesh made again at most 30 times at 45 fps, never in the air once his canopy is out (moved in place as it fills), down each 0.05 of its falling in (her sixth)");
		Check(FrameMost < 0.3 && MakeJumpMost < 0.3 && AwayMost < 0.1 && InsideFrames * 200 < TriangleFrames,
			"chutes: falling in, his silk moves every frame (moved in place between makes), under 0.3 m a frame at 45 fps and at a make, within 0.1 m of its true shape, under one in two hundred of its triangles' frames showing their inside (a review of 24 November: it had jumped 1.3 m at a make)");
		Check(Gaps == 0 && TwiceMost <= 1, "chutes: at the swap he is drawn every frame, in his chute's mesh or as the crowd's man, both (in place) one frame at most, however late the crowd's rebuilds let it draw him");
		int Folded = 0, HeapFolded = 0, HeapStep = 0, LinesBad = 0;
		size_t HeapTris = 0;
		for (const auto& Each : All)
		{
			const Chute& Lk = Each.second->Look;
			SilkMesh First, M;
			std::vector<int> W0, W1, Wh0, Wd;
			CanopyShape(Lk, 0.001, First);
			Windings(First, W0);
			for (double Open = 0.01; Open <= 1.0 + 1e-9; Open += 0.01)
			{
				CanopyShape(Lk, Open, M);
				Windings(M, W1);
				for (size_t K = 0; K < W0.size() && K < W1.size(); ++K) Folded += W0[K] != W1[K];
			}
			// (down: the lines back to him, by their place round the skirt, behind its middle all the while it falls in;
			// made again each step, how many of its triangles fold over within one, and by the time it lies)
			const Chute& Dn = Each.second->Lies;
			const double Off = std::hypot(Dn.Heap.X - Dn.Landed.X, Dn.Heap.Y - Dn.Landed.Y);
			int Back = 0;
			for (size_t K = 0; K < static_cast<size_t>(Lk.Gores); ++K) Back += LineBack(K, static_cast<size_t>(Lk.Gores));
			LinesBad += Back < Lk.Gores / 2 - 2 || Back > Lk.Gores / 2;
			for (double Sl = 0.0; Sl <= 1.0 + 1e-9; Sl += 0.05)
			{
				HeapShape(Dn, Sl, Off, M);
				double Mid = 0.0;
				for (const int Sm : M.Seams) Mid += M.Points[static_cast<size_t>(Sm)].X / static_cast<double>(M.Seams.size());
				for (size_t K = 0; K < M.Seams.size(); ++K) LinesBad += LineBack(K, M.Seams.size()) && M.Points[static_cast<size_t>(M.Seams[K])].X >= Mid;
				Windings(M, W1);
				if (Wh0.empty()) Wh0 = W1;
				int Step = 0;
				for (size_t K = 0; K < Wd.size() && K < W1.size(); ++K) Step += Wd[K] != W1[K];
				HeapStep = std::max(HeapStep, Step);
				Wd = W1;
				if (Sl > 0.99)
				{
					int Folds = 0;
					for (size_t K = 0; K < Wh0.size() && K < W1.size(); ++K) Folds += Wh0[K] != W1[K];
					HeapFolded = std::max(HeapFolded, Folds);
					HeapTris = W1.size();
				}
			}
		}
		std::printf("     moved in place: a canopy filling folds %d triangles over (of its windings as first made); falling in (made again each 0.05), %d at most within a step and %d by the time it lies (of %zu)\n",
			Folded, HeapStep, HeapFolded, HeapTris);
		Check(Folded == 0 && HeapStep > 0 && LinesBad == 0, "chutes: a canopy filling never folds a triangle over (moved in place); falling in some fold over within a step (so it is made again); down its lines run back to him from the same half of its skirt all the while it falls in");

		// The art director's verdict of 24 November (14:34Z), carried to the Mac: (c) "Side-on, the skirt drops straight
		// like a jelly mould. Tuck it in toward the lines." (b) "The fallen canopy is a flat oval with even scallops and
		// reads as a doily. Stretch it 2-3x downwind in loose folds, the far end lifting a little, lines running back to
		// him." Each pilot's canopy full (side-on: its rings' radii and heights), and as it lies (the half of its points
		// that is its outer face: its rings' points round from the way it falls, its back half lying on top).
		{
			int SkirtBad = 0, LieBad = 0, FoldBad = 0, SameBad = 0, UnderBad = 0, FallBad = 0, AboveBad = 0, BodyBad = 0, FarBad = 0, SettleBad = 0;
			double Deepest = 0.0, Floats = 0.0, HummockMs = 0.0, HummockMsMost = 0.0, LinesDeepest = -1e9;
			int LinesUnder = 0;
			int HummockMakes = 0;
			const Watched* Other = nullptr;
			// (b) lying, on a flat field: how long along the wind and how wide; how high its far quarter and its near
			// quarter stand; its far end's round (its last ring's top, against its far quarter's, and its crown, its tip,
			// which comes down to the ground: a review of 24 November found a beak hanging 0.23 m off it); its near end, its
			// hem's seams' middle (where its lines end), how gathered (the farthest seam from it) and how far from his
			// harness, where its lines meet him (2 to 3.6 m: its 4.1 m lines a little slack across the grass; a review had
			// found 0.6 to 1.2 m stubs), and how far past its nearest silk; its lines back to him: how many, how many run
			// back toward him, and how long (none longer than his lines). And its folds: in its top layer, across it at
			// each ring from an eighth to a little over a half along it (short of the air its far end holds), the ridges
			// standing 3 cm or more over the troughs on either side: how many a ring, how unevenly they stand (each ridge's
			// rise over its troughs: their spread over their mean, where a ring has two or more) and how unevenly they are
			// spaced (where it has three or more), each on its own bar (a review of 24 November: evenly spaced folds had
			// passed on uneven heights), and how many of a ring's ridges go on along it (a ridge on the next ring within
			// 0.3 m across).
			struct Lain
			{
				double Long = 0.0, Across = 0.0, Far = 0.0, Near = 0.0, LastTop = 0.0, TipHigh = 0.0, Spread = 0.0, NearEnd = 0.0, PastNearest = 0.0, LineMost = 0.0, LineLeast = 1e9;
				int Lines = 0, LinesBack = 0, RidgesMost = 0, RidgesLeast = 99;
				double HeightSpread = 0.0, GapSpread = 0.0, Along = 0.0;
				bool Bad(int Gores) const
				{
					return Long / Across < 2.0 || Long / Across > 3.0 || Far < 0.3 || Far > 0.8 || Near > Far || Spread > 0.8 || NearEnd < 2.0 || NearEnd > 3.6 || PastNearest > 0.5
						|| Lines < Gores / 2 - 1 || LinesBack != Lines || LineMost > LinesLong || TipHigh > 0.08 || LastTop < 0.25 * Far || LastTop > 0.75 * Far;
				}
				bool Even() const { return RidgesLeast < 2 || RidgesMost > 7 || HeightSpread < 0.2 || GapSpread < 0.2 || Along < 0.6; }
			};
			auto Measure = [](const Chute& Dn, const SilkMesh& M)
			{
				Lain Ln;
				const size_t Half = M.Points.size() / 2;
				const int Cols = Dn.Gores * 4;
				double MinX = 1e9, MaxX = -1e9, MinY = 1e9, MaxY = -1e9;
				for (size_t K = 0; K < Half; ++K)
				{
					MinX = std::min(MinX, M.Points[K].X); MaxX = std::max(MaxX, M.Points[K].X);
					MinY = std::min(MinY, M.Points[K].Y); MaxY = std::max(MaxY, M.Points[K].Y);
				}
				Ln.Long = MaxX - MinX;
				Ln.Across = MaxY - MinY;
				for (size_t K = 0; K < Half; ++K)
				{
					if (M.Points[K].X >= MinX + 0.75 * Ln.Long) Ln.Far = std::max(Ln.Far, M.Points[K].Z);
					if (M.Points[K].X <= MinX + 0.25 * Ln.Long) Ln.Near = std::max(Ln.Near, M.Points[K].Z);
				}
				const int Ringed = static_cast<int>((Half - 1) / static_cast<size_t>(Cols));
				for (int Col = 0; Col < Cols; ++Col) Ln.LastTop = std::max(Ln.LastTop, M.Points[static_cast<size_t>((Ringed - 1) * Cols + Col)].Z);
				Ln.TipHigh = M.Points[Half - 1].Z;
				V3 Gather;
				for (const int Sm : M.Seams) Gather = Plus(Gather, Mul(M.Points[static_cast<size_t>(Sm)], 1.0 / static_cast<double>(M.Seams.size())));
				const V3 H = OverHarness(1.0, ProneEndX, ProneEndY);
				const double Turn = (Dn.Face - Dn.HeapHeading) * 3.14159265358979323846 / 180.0;
				const V3 Harness{ H.X * std::cos(Turn) - H.Y * std::sin(Turn), H.X * std::sin(Turn) + H.Y * std::cos(Turn), H.Z };
				for (size_t K = 0; K < M.Seams.size(); ++K)
				{
					const V3& Sm = M.Points[static_cast<size_t>(M.Seams[K])];
					Ln.Spread = std::max(Ln.Spread, std::hypot(Sm.X - Gather.X, Sm.Y - Gather.Y));
					if (!LineBack(K, M.Seams.size())) continue;
					++Ln.Lines;
					const double Line = Length(Plus(Sm, Mul(Harness, -1.0)));
					Ln.LineMost = std::max(Ln.LineMost, Line);
					Ln.LineLeast = std::min(Ln.LineLeast, Line);
					Ln.LinesBack += Sm.X > Harness.X;
				}
				Ln.NearEnd = std::hypot(Gather.X - Harness.X, Gather.Y - Harness.Y);
				Ln.PastNearest = Gather.X - MinX;
				std::vector<std::vector<std::pair<double, double>>> RidgesOf;
				std::vector<double> RingAlong;
				for (int R = 0; R < Ringed; ++R)
				{
					std::vector<std::pair<double, double>> Row, Found;
					double Mean = 0.0;
					for (int Col = 0; Col < Cols; ++Col)
					{
						const int Q = Col % 4;
						const double Theta = 2.0 * 3.14159265358979323846 * (Col / 4 + (Q == 0 ? 0.0 : Q == 1 ? 0.2 : Q == 2 ? 0.5 : 0.8)) / (Cols / 4);
						const V3& Pt = M.Points[static_cast<size_t>(R * Cols + Col)];
						Mean += (Pt.X - MinX) / Ln.Long / Cols;
						if (std::cos(Theta) < -0.3) Row.emplace_back(Pt.Y, Pt.Z);
					}
					RingAlong.push_back(Mean);
					std::sort(Row.begin(), Row.end());
					for (size_t K = 1; K + 1 < Row.size(); ++K)
					{
						if (Row[K].second < Row[K - 1].second || Row[K].second < Row[K + 1].second) continue;
						double LeftLow = Row[K].second, RightLow = Row[K].second;
						for (size_t J = K; J-- > 0 && Row[J].second <= Row[J + 1].second + 1e-12;) LeftLow = Row[J].second;
						for (size_t J = K + 1; J < Row.size() && Row[J].second <= Row[J - 1].second + 1e-12; ++J) RightLow = Row[J].second;
						const double Rise = Row[K].second - std::max(LeftLow, RightLow);
						if (Rise >= 0.03) Found.emplace_back(Row[K].first, Rise);
					}
					RidgesOf.push_back(Found);
				}
				int Rows = 0, GapRows = 0, GoOn = 0, Ridges = 0;
				for (int R = 0; R + 1 < Ringed; ++R)
				{
					if (RingAlong[static_cast<size_t>(R)] < 0.12 || RingAlong[static_cast<size_t>(R)] > 0.58) continue;
					const std::vector<std::pair<double, double>>& Found = RidgesOf[static_cast<size_t>(R)];
					Ln.RidgesMost = std::max(Ln.RidgesMost, static_cast<int>(Found.size()));
					Ln.RidgesLeast = std::min(Ln.RidgesLeast, static_cast<int>(Found.size()));
					for (const auto& Rg : Found)
					{
						++Ridges;
						for (const auto& Next : RidgesOf[static_cast<size_t>(R + 1)]) if (std::fabs(Next.first - Rg.first) < 0.3) { ++GoOn; break; }
					}
					if (Found.size() < 2) continue;
					double Hm = 0.0, Hv = 0.0, Gm = 0.0, Gv = 0.0;
					const double N = static_cast<double>(Found.size());
					for (const auto& Rg : Found) Hm += Rg.second / N;
					for (const auto& Rg : Found) Hv += (Rg.second - Hm) * (Rg.second - Hm) / N;
					Ln.HeightSpread += std::sqrt(Hv) / Hm;
					++Rows;
					if (Found.size() < 3) continue;
					for (size_t K = 1; K < Found.size(); ++K) Gm += (Found[K].first - Found[K - 1].first) / (N - 1.0);
					for (size_t K = 1; K < Found.size(); ++K) Gv += (Found[K].first - Found[K - 1].first - Gm) * (Found[K].first - Found[K - 1].first - Gm) / (N - 1.0);
					Ln.GapSpread += std::sqrt(Gv) / Gm;
					++GapRows;
				}
				Ln.HeightSpread /= std::max(1, Rows);
				Ln.GapSpread /= std::max(1, GapRows);
				Ln.Along = Ridges > 0 ? static_cast<double>(GoOn) / Ridges : 0.0;
				return Ln;
			};
			// (and over 300 pilots' numbers, canopies 5 to 5.5 m across, in winds of 0 to 7 m/s, facing the way it falls
			// or up to a right angle off it: how many lie badly or in even folds, and the least and most of each)
			{
				int SweepBad = 0, SweepEven = 0, Few = 0, EvenHigh = 0, EvenSpaced = 0, Broken = 0, Ratio = 0, Lift = 0, Gathered = 0, Lines = 0, Tip = 0;
				double RatioLeast = 1e9, RatioMost = 0.0, FarLeast = 1e9, FarMost = 0.0, NearLeast = 1e9, NearMost = 0.0, HighLeast = 1e9, SpacedLeast = 1e9, LineMost = 0.0, TipMost = 0.0;
				// (and from above, her note of 24 November 17:40Z: the least and most of each of FromAbove's readings)
				double SideLeast = 1e9, SideMost = 0.0, SideSum = 0.0, TipLeast = 1e9, TipSum = 0.0, WavyLeast = 1e9, WavyMost = 0.0, WavySum = 0.0, HemLeast = 1e9, HemSum = 0.0;
				double SlumpLeast = 1e9, SlumpMost = 0.0, SlumpSum = 0.0, LumaLeast = 1e9, LumaMost = 0.0, BuriedMost = 0.0, OnLeast = 1e9, OnMost = 0.0;
				int TurnsLeast = 99, TurnsMost = 0, FoldsLeast = 99, FoldsMost = 0, BandsLeast = 99, BandsMost = 0, CrossLeast = 99, CrossMost = 0, HemTurnsLeast = 99;
				int AboveOff = 0, SideLow = 0, FoldsOff = 0, FoldsFew = 0, BandsOne = 0, BandsNone = 0, SlumpLow = 0, CrossSum = 0;
				int BodyLeast = 99, BodyLinesMost = 0, BodySum = 0, BodyNone = 0, StrewnLeast = 99, StrewnMost = 0, StrewnSum = 0;
				double BodyLongLeast = 1e9, BodyLongMost = 0.0, BodyLongSum = 0.0, FloatMost = 0.0, FloatMeanMost = 0.0, LongShareMost = 0.0, FloatMeanSum = 0.0;
				double OnSilkLeast = 1e9, OnSilkMost = 0.0, OnSilkSum = 0.0, SlantLeast = 90.0, SlantSum = 0.0, ReachMost = 0.0, GrassMost = 0.0, GatherLeast = 1e9, FloatHighMost = 0.0;
				int OnSilkOver = 0, SlantN = 0;
				double SettleBuried = 0.0;
				int BodyOffs = 0, FarOffs = 0, VentLow = 0;
				double VentLeast = 1e9, VentSum = 0.0, CreaseLeast = 1e9, CreaseSum = 0.0, UnderLeast = 1e9, UnderSum = 0.0, KinksLeast = 1e9, KinksSum = 0.0;
				SilkMesh M;
				for (int K = 0; K < 300; ++K)
				{
					const Chute Dn = SweepPilot(K);
					const double Off = HeapOff(Dn.Folds, Dn.Across, Dn.Wind, Dn.Face, Dn.HeapHeading);
					HeapShape(Dn, 1.0, Off, M);
					const Lain Ln = Measure(Dn, M);
					const Above Ab = FromAbove(Dn, M);
					const double Wavy = std::max(Ab.Off[0], Ab.Off[1]);
					SideLeast = std::min(SideLeast, Ab.Side);
					SideMost = std::max(SideMost, Ab.Side);
					SideSum += Ab.Side;
					TipLeast = std::min(TipLeast, Ab.SideTip);
					TipSum += Ab.SideTip;
					HemLeast = std::min(HemLeast, Ab.Hem);
					HemSum += Ab.Hem;
					WavyLeast = std::min(WavyLeast, Wavy);
					WavyMost = std::max(WavyMost, Wavy);
					WavySum += Wavy;
					TurnsLeast = std::min(TurnsLeast, Ab.Turns[0] + Ab.Turns[1]);
					TurnsMost = std::max(TurnsMost, Ab.Turns[0] + Ab.Turns[1]);
					FoldsLeast = std::min(FoldsLeast, Ab.Folds);
					FoldsMost = std::max(FoldsMost, Ab.Folds);
					BandsLeast = std::min(BandsLeast, Ab.Bands);
					BandsMost = std::max(BandsMost, Ab.Bands);
					SlumpLeast = std::min(SlumpLeast, Ab.Slump);
					SlumpMost = std::max(SlumpMost, Ab.Slump);
					SlumpSum += Ab.Slump;
					CrossLeast = std::min(CrossLeast, Ab.Crossing);
					CrossMost = std::max(CrossMost, Ab.Crossing);
					OnLeast = std::min(OnLeast, Ab.CrossMost);
					OnMost = std::max(OnMost, Ab.CrossMost);
					BuriedMost = std::max(BuriedMost, Ab.Buried);
					LumaLeast = std::min(LumaLeast, Ab.LumaLo);
					LumaMost = std::max(LumaMost, Ab.LumaHi);
					HemTurnsLeast = std::min(HemTurnsLeast, Ab.NearTurns);
					AboveOff += Ab.Bad();
					SideLow += Ab.Side < 0.1;
					FoldsOff += Ab.Folds < 3 || Ab.Folds > 4;
					FoldsFew += Ab.Folds < 2 || Ab.Folds > 5;
					BandsOne += Ab.Bands < 2;
					BandsNone += Ab.Bands < 1;
					SlumpLow += Ab.Slump < 0.2;
					CrossSum += Ab.Crossing;
					BodyLeast = std::min(BodyLeast, Ab.BodyLines);
					BodyLinesMost = std::max(BodyLinesMost, Ab.BodyLines);
					BodySum += Ab.BodyLines;
					BodyNone += Ab.BodyLines < 1;
					BodyLongLeast = std::min(BodyLongLeast, Ab.BodyMost);
					BodyLongMost = std::max(BodyLongMost, Ab.BodyMost);
					BodyLongSum += Ab.BodyMost;
					StrewnLeast = std::min(StrewnLeast, Ab.Strewn);
					StrewnMost = std::max(StrewnMost, Ab.Strewn);
					StrewnSum += Ab.Strewn;
					FloatMost = std::max(FloatMost, Ab.FloatMost);
					FloatMeanMost = std::max(FloatMeanMost, Ab.FloatMean);
					LongShareMost = std::max(LongShareMost, Ab.LongShare);
					FloatMeanSum += Ab.FloatMean;
					OnSilkLeast = std::min(OnSilkLeast, Ab.OnLeast);
					OnSilkMost = std::max(OnSilkMost, Ab.OnMost);
					OnSilkSum += Ab.OnMost;
					// (its least slant on the mean over those it was measured on: a pilot with none laid across it on its silk
					// had counted 90 degrees)
					if (Ab.Slanted > 0)
					{
						SlantLeast = std::min(SlantLeast, Ab.SlantLeast);
						SlantSum += Ab.SlantLeast;
						++SlantN;
					}
					ReachMost = std::max(ReachMost, Ab.ReachMost);
					GrassMost = std::max(GrassMost, Ab.GrassMost);
					// (and as it settles, falling in, on the first 60: from 0.8 of its fall, its lines slack and lying over it,
					// none of them under its top (a review: they had sunk into its folds as it settled, up to 0.19 m, and risen
					// through them at its last make))
					for (int Sl = 0; Sl < 4 && K < 60; ++Sl)
					{
						SilkMesh Settling;
						HeapShape(Dn, 0.8 + 0.05 * Sl, Off, Settling);
						SettleBuried = std::max(SettleBuried, FromAbove(Dn, Settling).Buried);
					}
					GatherLeast = std::min(GatherLeast, Ab.GatherNear);
					FloatHighMost = std::max(FloatHighMost, Ab.FloatHigh);
					OnSilkOver += Ab.OnMost > 2.6;
					BodyOffs += Ab.BodyOff();
					FarOffs += Ab.FarOff();
					VentLow += Ab.Vent < 0.1;
					VentLeast = std::min(VentLeast, Ab.Vent);
					VentSum += Ab.Vent;
					CreaseLeast = std::min(CreaseLeast, Ab.Crease);
					CreaseSum += Ab.Crease;
					UnderLeast = std::min(UnderLeast, Ab.Under);
					UnderSum += Ab.Under;
					KinksLeast = std::min(KinksLeast, Ab.Kinks);
					KinksSum += Ab.Kinks;
					SweepBad += Ln.Bad(Dn.Gores);
					SweepEven += Ln.Even();
					Few += Ln.RidgesLeast < 2 || Ln.RidgesMost > 7;
					EvenHigh += Ln.HeightSpread < 0.2;
					EvenSpaced += Ln.GapSpread < 0.2;
					Broken += Ln.Along < 0.6;
					Ratio += Ln.Long / Ln.Across < 2.0 || Ln.Long / Ln.Across > 3.0;
					Lift += Ln.Far < 0.3 || Ln.Far > 0.8 || Ln.Near > Ln.Far;
					Gathered += Ln.Spread > 0.8 || Ln.NearEnd < 2.0 || Ln.NearEnd > 3.6 || Ln.PastNearest > 0.5;
					Lines += Ln.Lines < Dn.Gores / 2 - 1 || Ln.LinesBack != Ln.Lines || Ln.LineMost > LinesLong;
					Tip += Ln.TipHigh > 0.08 || Ln.LastTop < 0.25 * Ln.Far || Ln.LastTop > 0.75 * Ln.Far;
					RatioLeast = std::min(RatioLeast, Ln.Long / Ln.Across);
					RatioMost = std::max(RatioMost, Ln.Long / Ln.Across);
					FarLeast = std::min(FarLeast, Ln.Far);
					FarMost = std::max(FarMost, Ln.Far);
					NearLeast = std::min(NearLeast, Ln.NearEnd);
					NearMost = std::max(NearMost, Ln.NearEnd);
					HighLeast = std::min(HighLeast, Ln.HeightSpread);
					SpacedLeast = std::min(SpacedLeast, Ln.GapSpread);
					LineMost = std::max(LineMost, Ln.LineMost);
					TipMost = std::max(TipMost, Ln.TipHigh);
				}
				std::printf("     over 300 pilots' numbers lying: %d off (%d its length, %d its ends' heights, %d its near end, %d its lines, %d its far end's round), %d in even folds (%d too few or many a ring, %d even in height, %d evenly spaced, %d not along it); %.2f to %.2f as long as wide, its far end up %.2f to %.2f m and its tip %.2f at most, its near end %.2f to %.2f m past his harness, its lines %.2f m at most; its folds' rises %.2f uneven at the least and their spacing %.2f (spread over mean)\n",
					SweepBad, Ratio, Lift, Gathered, Lines, Tip, SweepEven, Few, EvenHigh, EvenSpaced, Broken, RatioLeast, RatioMost, FarLeast, FarMost, TipMost, NearLeast, NearMost, LineMost, HighLeast, SpacedLeast);
				std::printf("     over 300 pilots' numbers from above: (a) its silk %.3f to %.3f more on one side of its wind line (%.3f on the mean), %.3f at the least about the line to its tip (%.3f on the mean); (b) its more wavy edge %.1f to %.1f cm off a smooth line (%.1f on the mean), its edges turning back %d to %d times; its hem %.2f m off a smooth curve at the least (%.2f on the mean), turning back %d times at the least; (c) %d to %d folds across it, %d to %d light bands along it; (d) its far end's lift %.2f to %.2f m to one side (%.2f on the mean); (e) %d to %d lines across its silk, the longest across it %.2f to %.2f m, under its top %.3f m at the deepest; across its middle luma %.0f to %.0f at the most; %d off; %d under 0.1 to one side, %d not three or four folds across (%d under two or over five), %d one light band or none (%d none), %d slumped under 0.2 m, %.2f lines across its silk on the mean\n",
					SideLeast, SideMost, SideSum / 300.0, TipLeast, TipSum / 300.0, WavyLeast * 100.0, WavyMost * 100.0, WavySum / 3.0, TurnsLeast, TurnsMost, HemLeast, HemSum / 300.0, HemTurnsLeast, FoldsLeast, FoldsMost, BandsLeast, BandsMost,
					SlumpLeast, SlumpMost, SlumpSum / 300.0, CrossLeast, CrossMost, OnLeast, OnMost, BuriedMost, LumaLeast, LumaMost, AboveOff, SideLow, FoldsOff, FoldsFew, BandsOne, BandsNone, SlumpLow, CrossSum / 300.0);
				std::printf("     over 300 pilots' numbers from above, across its body (her note of 17:40Z, (3), after a second look): (e) %d to %d of its lines 0.3 m or more across it past 0.15 of its length (%.2f on the mean, %d none), the longest so %.2f to %.2f m (%.2f on the mean); %d to %d laid across it (%.2f on the mean), %.2f to %.2f m of each on its silk (its longest %.2f on the mean, %d over 2.6 m), %.0f degrees off its wind line at the least (its least %.0f on the mean, over the %d with one measured), reaching %.2f of its length at most, passing %.2f m from where its lines gather at the nearest on its way to it and coming onto it %.2f m from his harness at most; their middle %.3f m over its silk at most (%.3f on the mean at most, %.3f on the mean), more than 4 cm over it for %.3f of its length on it at most, drawn %.2f of a real line's length at most\n",
					BodyLeast, BodyLinesMost, BodySum / 300.0, BodyNone, BodyLongLeast, BodyLongMost, BodyLongSum / 300.0, StrewnLeast, StrewnMost, StrewnSum / 300.0, OnSilkLeast, OnSilkMost, OnSilkSum / 300.0, OnSilkOver,
					SlantLeast, SlantSum / std::max(1, SlantN), SlantN, ReachMost, GatherLeast, GrassMost, FloatMost, FloatMeanMost, FloatMeanSum / 300.0, FloatHighMost, LongShareMost);
				std::printf("     settling (0.8 to 0.95 of its fall), on 60 pilots: his lines under its top %.3f m at the deepest\n", SettleBuried);
				SettleBad += SettleBuried > 0.008;
				std::printf("     over 300 pilots' numbers from above, its far end (her note of 17:40Z, (2), after a second look): (f) its vent %.2f of its half-width off its middle at the least (%.2f on the mean, %d under 0.1), a crease across its dome %.3f m deep at the least (%.3f on the mean), folded back under at an edge %.2f m at the least (%.2f on the mean), its seams bending %.3f m ring to ring at the least (%.3f on the mean); %d with a neat far end, %d with their lines across its body off\n",
					VentLeast, VentSum / 300.0, VentLow, CreaseLeast, CreaseSum / 300.0, UnderLeast, UnderSum / 300.0, KinksLeast, KinksSum / 300.0, FarOffs, BodyOffs);
				// (across its body: none off, none without one; 1.8 of its lines across it on the mean (two on most: a line
				// across its body no longer than her 2.5 m, slanted, from where its lines gather does not fit across some
				// canopies' near half), the longest across its body 0.8 to 2 m on the mean, each one's longest on its silk 1.3
				// to 2.4 m on the mean and over 2.6 m on a tenth at most, its least slant 35 degrees or more on the mean, their
				// middle within 2 cm of its silk's top on the mean (its cord lying on it within 1.4 cm). Its far end: none neat;
				// on the mean its vent 0.22 of its half-width off its middle (one in fifteen under 0.1), a crease 9 cm deep
				// across it (a review of 24 November 17:40Z, after a second look: the neat dome before it had measured 4.8
				// cm on the mean, under the 5 this had asked), 0.2 m of it folded back under at an edge and its seams bending
				// 0.16 m ring to ring)
				BodyBad += BodyOffs + (BodySum < 540) + (BodyLongSum / 300.0 < 0.8) + (BodyLongSum / 300.0 > 2.0) + (OnSilkSum / 300.0 < 1.3) + (OnSilkSum / 300.0 > 2.4)
					+ (OnSilkOver > 30) + (SlantSum / std::max(1, SlantN) < 35.0) + (FloatMeanSum / 300.0 > 0.02) + (BodyNone > 0);
				FarBad += FarOffs + (VentSum / 300.0 < 0.22) + (VentLow > 20) + (CreaseSum / 300.0 < 0.09) + (UnderSum / 300.0 < 0.2) + (KinksSum / 300.0 < 0.16);
				// (none off; three in four with three or four folds across it, one in a hundred under two or over five; two
				// in three with two light bands or more, one in a hundred none; a twentieth under 0.1 to one side or slumped
				// under 0.2 m; on the mean 0.2 to one side of either line, its edge 3 cm off a smooth line, its hem 0.28 m off
				// a smooth curve, its far end slumped 0.22 m and one and a half of its lines across its silk)
				AboveBad += AboveOff + (FoldsOff > 75) + (FoldsFew > 3) + (BandsOne > 100) + (BandsNone > 3) + (SideLow > 15) + (SlumpLow > 15) + (SideSum / 300.0 < 0.2) + (TipSum / 300.0 < 0.2)
					+ (WavySum / 300.0 < 0.03) + (HemSum / 300.0 < 0.28) + (SlumpSum / 300.0 < 0.22) + (CrossSum < 450);
				LieBad += SweepBad;
				FoldBad += SweepEven;
			}
			// (and on the same fields as the verdict's four below, over the 300 pilots' numbers, falling in and lying: its
			// points, and his lines as the scene draws them (GroundSweep), a share of the pilots on each thread there is. A
			// second review: drawn from his harness where the scene draws it, his lines across its silk had cut up to 0.23 m
			// into hummocks, and while barely slack (a slump of 0.30 to 0.36) up to 0.07 m still, on pilots the four did not
			// show. A third: looked at between makes too, as the scene moves them, one had gone 0.7 mm under on hummocks; and
			// from his harness in a bank or a hummock (his man drawn flat at his feet's height) the grass of his lines across
			// its silk had been flung 5 to 7 m up off the bank, 1.9 m off hummocks)
			GroundTally Swept;
			double FlungMost = -1e9;
			std::string FlungEach;
			{
				const int Threads = static_cast<int>(std::clamp(std::thread::hardware_concurrency(), 1u, 16u));
				std::vector<GroundTally> Tallies(static_cast<size_t>(Threads));
				std::vector<std::thread> Workers;
				for (int Th = 0; Th < Threads; ++Th) Workers.emplace_back(GroundSweep, Th, Threads, std::ref(Tallies[static_cast<size_t>(Th)]));
				for (std::thread& Worker : Workers) Worker.join();
				for (const GroundTally& Tl : Tallies)
				{
					Swept.Under += Tl.Under;
					Swept.LinesUnder += Tl.LinesUnder;
					Swept.LinesUnderBetween += Tl.LinesUnderBetween;
					Swept.Deepest = std::max(Swept.Deepest, Tl.Deepest);
					Swept.RestLeast = std::min(Swept.RestLeast, Tl.RestLeast);
					Swept.BetweenLeast = std::min(Swept.BetweenLeast, Tl.BetweenLeast);
					for (int Field = 0; Field < GroundFields; ++Field)
					{
						Swept.FlungHigh[Field] += Tl.FlungHigh[Field];
						if (Tl.Flung[Field] > Swept.Flung[Field] || (Tl.Flung[Field] == Swept.Flung[Field] && Tl.FlungPilot[Field] < Swept.FlungPilot[Field]))
						{
							Swept.Flung[Field] = Tl.Flung[Field];
							Swept.FlungPilot[Field] = Tl.FlungPilot[Field];
							Swept.FlungSlump[Field] = Tl.FlungSlump[Field];
						}
					}
					// (the first pilot of those as low, whatever the threads)
					if (Tl.AcrossLeast < Swept.AcrossLeast || (Tl.AcrossLeast == Swept.AcrossLeast && Tl.AcrossAt[0] < Swept.AcrossAt[0]))
					{
						Swept.AcrossLeast = Tl.AcrossLeast;
						std::copy(Tl.AcrossAt, Tl.AcrossAt + 3, Swept.AcrossAt);
					}
				}
				std::printf("     over 300 pilots' numbers on the same %d fields, falling in and lying (each 0.05 of its fall): %d of its points 5 mm under the ground or more (%.3f m at the deepest); %d of his lines under it, those across its silk %.3f m over it at the lowest (pilot %d, field %d, %.2f of its fall) and the rest %.3f\n",
					GroundFields, Swept.Under, Swept.Deepest, Swept.LinesUnder, Swept.AcrossLeast, Swept.AcrossAt[0], Swept.AcrossAt[1], Swept.AcrossAt[2] / 20.0, Swept.RestLeast);
				for (int Field = 0; Field < GroundFields; ++Field)
				{
					FlungMost = std::max(FlungMost, Swept.Flung[Field]);
					char Each2[96];
					std::snprintf(Each2, sizeof(Each2), "%s%d: %.3f (pilot %d, %.4f; %d over 1.5 m)", Field ? ", " : "", Field, Swept.Flung[Field], Swept.FlungPilot[Field], Swept.FlungSlump[Field], Swept.FlungHigh[Field]);
					FlungEach += Each2;
				}
				std::printf("     and between makes (each 0.0125 of its fall, moved in place as the scene moves it): %d of his lines' middles more than 3 mm under the ground, those across its silk %.4f m over it at the lowest; the points those go through across the grass over both the taut straight from his harness and the ground, at makes and between, on each field at most %s\n",
					Swept.LinesUnderBetween, Swept.BetweenLeast, FlungEach.c_str());
			}
			int Nth = 0;
			for (const auto& Each : All)
			{
				const Watched& W = *Each.second;
				++Nth;
				// (c) side-on: widest a quarter to a half of its depth under its crown, its hem 0.8 to 0.9 as wide, its silk
				// at its hem heading in toward its lines (leaning in more than not at all, and no more than they do); and (a
				// review of 24 November: it had dropped straight 0.8 m under its widest, a flat-topped drum bevelled at the
				// bottom) its silhouette as drawn (each ring's outermost point, straight between rings) curving in all the way
				// down from its widest, leaning in more at each ring down it, and its crown rounder than a drum's: its width
				// at 0.5 and 0.6 of its depth down, and at 3% and 11% under its top
				SilkMesh M;
				CanopyShape(W.Look, 1.0, M);
				const int Cols = W.Look.Gores * 4, Ringed = static_cast<int>((M.Points.size() / 2 - 1) / static_cast<size_t>(Cols));
				std::vector<double> RingR(static_cast<size_t>(Ringed), 0.0), RingZ(static_cast<size_t>(Ringed), 0.0), RingOut(static_cast<size_t>(Ringed), 0.0);
				double Top = -1e9, Bottom = 1e9;
				for (int R = 0; R < Ringed; ++R)
				{
					for (int Col = 0; Col < Cols; ++Col)
					{
						const V3& Pt = M.Points[static_cast<size_t>(R * Cols + Col)];
						RingR[static_cast<size_t>(R)] += std::hypot(Pt.X, Pt.Y) / Cols;
						RingZ[static_cast<size_t>(R)] += Pt.Z / Cols;
						RingOut[static_cast<size_t>(R)] = std::max(RingOut[static_cast<size_t>(R)], std::hypot(Pt.X, Pt.Y));
					}
				}
				for (const V3& Pt : M.Points) { Top = std::max(Top, Pt.Z); Bottom = std::min(Bottom, Pt.Z); }
				const size_t Wide = static_cast<size_t>(std::max_element(RingR.begin(), RingR.end()) - RingR.begin());
				const double HemShare = RingR[0] / RingR[Wide], Down = (Top - RingZ[Wide]) / (Top - Bottom);
				const double Lean = std::atan2(RingR[1] - RingR[0], RingZ[1] - RingZ[0]) * 180.0 / 3.14159265358979323846;
				const double LinesLean = std::atan2(RingR[0] - 0.25, LinesLong - 0.8) * 180.0 / 3.14159265358979323846;
				// (the silhouette's half-width at a share of its depth down from its top: the rings' outermost points, and its
				// crown's point, straight between)
				const double Widest2 = *std::max_element(RingOut.begin(), RingOut.end());
				auto WidthAt = [&](double Share)
				{
					const double Z = Top - Share * (Top - Bottom);
					double LoZ = Top, LoR = 0.0;
					for (int R = Ringed - 1; R >= 0; --R)
					{
						const double Rz = RingZ[static_cast<size_t>(R)], Rr = RingOut[static_cast<size_t>(R)];
						if (Rz <= Z) return (Rr + (LoR - Rr) * (Z - Rz) / std::max(1e-9, LoZ - Rz)) / Widest2;
						LoZ = Rz;
						LoR = Rr;
					}
					return RingOut[0] / Widest2;
				};
				int Straight = 0;
				double LeanWas = 0.0;
				std::string Leans;
				for (size_t R = Wide; R-- > 0;)
				{
					const double Way = std::atan2(RingOut[R + 1] - RingOut[R], RingZ[R + 1] - RingZ[R]) * 180.0 / 3.14159265358979323846;
					Straight += Way < 3.0 || Way < LeanWas;
					LeanWas = Way;
					char Buf[16];
					std::snprintf(Buf, sizeof Buf, "%s%.0f", Leans.empty() ? "" : ", ", Way);
					Leans += Buf;
				}
				const double At50 = WidthAt(0.5), At60 = WidthAt(0.6), Crown3 = WidthAt(0.03), Crown11 = WidthAt(0.11);
				std::printf("     %s pilot of plane %d's canopy side-on: %.2f m across at its widest, %.2f of its %.2f m depth under its crown; its hem %.2f m across, %.2f of its widest; its silk at its hem leaning in %.0f degrees, its lines %.0f\n",
					Each.first >= 100 ? "windy:" : "calm: ", Each.first % 100, 2.0 * RingR[Wide], Down, Top - Bottom, 2.0 * RingR[0], HemShare, Lean, LinesLean);
				std::printf("        its silhouette %.3f of its widest at 0.5 of its depth down and %.3f at 0.6; leaning in %s degrees from its widest down, ring by ring; its crown %.2f of its widest 3%% under its top and %.2f 11%% under it\n",
					At50, At60, Leans.c_str(), Crown3, Crown11);
				SkirtBad += HemShare < 0.8 || HemShare > 0.9 || Down < 0.25 || Down > 0.5 || Wide == 0 || RingR[1] <= RingR[0] || Lean < 5.0 || Lean > LinesLean
					|| Straight > 0 || At60 > 0.985 || Crown11 > 0.65;

				// (b) lying, on a flat field (Measure): two to three times as long along the wind as it is wide; its far end
				// up 0.3 to 0.8 m on the air it holds, its near end lower; its near end gathered (its hem's seams within
				// 0.8 m of their middle, its nearest silk) and within 1.5 m of his harness, where its lines meet him, the
				// lines running back to him from it; its folds uneven (their rises or their spacing spread a fifth of
				// their mean or more: even scallops spread none), two to seven a ring, most going on along it
				const Chute& Dn = W.Lies;
				const double Off = std::hypot(Dn.Heap.X - Dn.Landed.X, Dn.Heap.Y - Dn.Landed.Y);
				HeapShape(Dn, 1.0, Off, M);
				const Lain Ln = Measure(Dn, M);
				std::printf("        lying: %.1f m along the wind by %.1f across (%.2f to 1), its middle %.1f m off; its far end up %.2f m, its last ring %.2f and its tip %.2f, its near end %.2f; its hem gathered within %.2f m, %.2f m past his harness, %d lines back to him %.1f to %.1f m long; folds %d to %d a ring across it, their rises %.2f uneven and their spacing %.2f (spread over mean), %.0f%% going on along it\n",
						Ln.Long, Ln.Across, Ln.Long / Ln.Across, Off, Ln.Far, Ln.LastTop, Ln.TipHigh, Ln.Near, Ln.Spread, Ln.NearEnd, Ln.LinesBack, Ln.LineLeast, Ln.LineMost, Ln.RidgesLeast, Ln.RidgesMost, Ln.HeightSpread, Ln.GapSpread, Ln.Along * 100.0);
				LieBad += Ln.Bad(Dn.Gores);
				FoldBad += Ln.Even();
				// (from above: her note of 24 November 17:40Z, FromAbove)
				const Above Ab = FromAbove(Dn, M);
				std::printf("        from above: (a) its silk %.3f more on one side of its wind line, %.3f about the line to its tip; (b) its edges %.1f and %.1f cm off a smooth line (right, left), turning back %d and %d times; its hem %.2f m off a smooth curve, turning back %d times; (c) %d folds across it, %d light bands along it, across its middle luma %.0f to %.0f; (d) its far end's lift %.2f m to one side; (e) %d of its %d lines across its silk (%.2f m at most), under its top %.3f m at the deepest; across its body %d 0.3 m or more (%.2f m at most), %d laid across it, %.2f to %.2f m of each on its silk, %.0f degrees off its wind line at the least, reaching %.2f of its length, passing %.2f m from where its lines gather at the nearest and coming onto it %.2f m from his harness at most, their middle %.3f m over its silk at most (%.3f on the mean, more than 4 cm over it for %.3f of its length on it), drawn %.2f of a real line's length at most\n",
					Ab.Side, Ab.SideTip, Ab.Off[0] * 100.0, Ab.Off[1] * 100.0, Ab.Turns[0], Ab.Turns[1], Ab.Hem, Ab.NearTurns, Ab.Folds, Ab.Bands, Ab.LumaLo, Ab.LumaHi, Ab.Slump, Ab.Crossing, Ab.Lines, Ab.CrossMost, Ab.Buried,
					Ab.BodyLines, Ab.BodyMost, Ab.Strewn, Ab.OnLeast, Ab.OnMost, Ab.SlantLeast, Ab.ReachMost, Ab.GatherNear, Ab.GrassMost, Ab.FloatMost, Ab.FloatMean, Ab.FloatHigh, Ab.LongShare);
				// (each of the verdict's four: 0.12 or more to one side of either line, its hem 0.25 m off a smooth curve,
				// three or four folds across, a light band along it, its far end slumped 0.2 m or more, two of its lines
				// across its silk, its luma across its middle within 6 of her 126 to 221)
				AboveBad += Ab.Bad() || Ab.Side < 0.12 || Ab.SideTip < 0.12 || Ab.Hem < 0.25 || Ab.Folds < 3 || Ab.Folds > 4 || Ab.Bands < 1 || Ab.Slump < 0.2 || Ab.Crossing < 2 || Ab.LumaLo < 120.0
					|| Ab.LumaHi > 227.0;
				// (and two of its lines or more across its body on his and the second pilot's, the two in the pictures, one or
				// more on the others (the last's near half too long for a second within her 2.5 m, slanted); its far end
				// broken up)
				std::printf("        its far end from above: (f) its vent %.2f of its half-width off its middle, a crease across its dome %.3f m deep, folded back under at an edge %.2f m, its seams bending %.3f m ring to ring\n",
					Ab.Vent, Ab.Crease, Ab.Under, Ab.Kinks);
				BodyBad += Ab.BodyOff() || Ab.BodyLines < (Nth <= 2 ? 2 : 1);
				FarBad += Ab.FarOff();

				// (two pilots' canopies lie differently: this one's with another's number, his own size and all)
				if (Other)
				{
					Chute Swap = Dn;
					Swap.Folds = Other->Lies.Folds;
					SilkMesh Mine, Theirs;
					HeapShape(Dn, 1.0, Off, Mine);
					HeapShape(Swap, 1.0, Off, Theirs);
					double Sum = 0.0;
					for (size_t K = 0; K < Mine.Points.size(); ++K) Sum += std::pow(Mine.Points[K].Z - Theirs.Points[K].Z, 2.0) + std::pow(Mine.Points[K].Y - Theirs.Points[K].Y, 2.0);
					const double Apart = std::sqrt(Sum / static_cast<double>(Mine.Points.size()));
					std::printf("        with another pilot's number it lies %.2f m apart (across and up, the root of the mean square)\n", Apart);
					SameBad += Dn.Folds == Other->Lies.Folds || Apart < 0.08;
				}
				Other = &W;

				// (falling in: moving downwind and down, its middle never back toward him, its top never rising, longer
				// along the wind half fallen in than as it was; how far a point moves from one make to the next, which the
				// scene moves in place between, above)
				{
					SilkMesh Was, Now;
					double MidWas = -1e9, TopWas = 1e9, Step = 0.0, Long0 = 0.0, LongHalf = 0.0, Mid0 = 0.0, MidHalf = 0.0;
					int Back = 0, Rise = 0;
					for (int Sk = 0; Sk <= 20; ++Sk)
					{
						HeapShape(Dn, Sk / 20.0, Off, Now);
						double Mid = 0.0, TopNow = -1e9, Lo = 1e9, Hi = -1e9;
						for (const V3& Pt : Now.Points) { Mid += Pt.X / static_cast<double>(Now.Points.size()); TopNow = std::max(TopNow, Pt.Z); Lo = std::min(Lo, Pt.X); Hi = std::max(Hi, Pt.X); }
						Back += Mid < MidWas - 1e-9;
						Rise += TopNow > TopWas + 1e-9;
						if (Sk == 0) { Long0 = Hi - Lo; Mid0 = Mid; }
						if (Sk == 10) { LongHalf = Hi - Lo; MidHalf = Mid; }
						if (Sk > 0) for (size_t K = 0; K < Now.Points.size(); ++K) Step = std::max(Step, Length(Plus(Now.Points[K], Mul(Was.Points[K], -1.0))));
						MidWas = Mid;
						TopWas = TopNow;
						std::swap(Was, Now);
					}
					std::printf("        falling in: its middle back toward him %d times, its top rising %d; half fallen in %.1f m along the wind (as he landed %.1f) and %.1f m on; %.2f m from one make to the next at most (moved in place between)\n",
						Back, Rise, LongHalf, Long0, MidHalf - Mid0, Step);
					FallBad += Back > 0 || Rise > 0 || LongHalf < Long0 + 0.5 || MidHalf - Mid0 < 1.5;
				}

				// (none of it under the ground, falling in or lying: on a slope of 1 in 4 rising and falling along the wind
				// and across it, on a field of hummocks, and (a review of 24 November: laid point by point, its triangles had
				// cut 0.2 m into a bank, a ditch's sides, a sharp crest and a crater's rim) across a hedgerow bank 1 m high, a
				// step 0.4 m up, a ditch 0.6 m deep, a sharp crest, a crater's bowl and rim, and a bank along it under one
				// edge: its points, and its triangles' edges at seven places each against the ground itself. And on the
				// slopes, its underside lying on the ground, not floating: each ring's lowest point over the ground there)
				for (int Field = 0; Field < GroundFields; ++Field)
				{
					// (the field in the canopy's own space: x the way it falls from his feet, y to its right)
					const auto Local = [Field](double X, double Y) { return FieldUnder(Field, X, Y); };
					const GroundFn Board = FieldBoard(Field, Dn);
					for (int Sk = 0; Sk <= 20; ++Sk)
					{
						const auto Began = std::chrono::steady_clock::now();
						HeapShape(Dn, Sk / 20.0, Off, M, Board);
						if (Field == 4)
						{
							const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Began).count();
							HummockMs += Ms;
							HummockMsMost = std::max(HummockMsMost, Ms);
							++HummockMakes;
						}
						for (const V3& Pt : M.Points)
						{
							const double Short = Local(Pt.X, Pt.Y) + 0.005 - Pt.Z;
							UnderBad += Short > 0.0;
							Deepest = std::max(Deepest, Short - 0.005);
						}
						for (size_t K = 0; K + 2 < M.Triangles.size(); K += 3)
						{
							bool bCut = false;
							for (int E = 0; E < 3; ++E)
							{
								const V3& Pa = M.Points[static_cast<size_t>(M.Triangles[K + static_cast<size_t>(E)])];
								const V3& Pb = M.Points[static_cast<size_t>(M.Triangles[K + static_cast<size_t>((E + 1) % 3)])];
								for (int S7 = 1; S7 < 8; ++S7)
								{
									const double T = S7 / 8.0;
									const double Short = Local(Pa.X + (Pb.X - Pa.X) * T, Pa.Y + (Pb.Y - Pa.Y) * T) - (Pa.Z + (Pb.Z - Pa.Z) * T);
									bCut = bCut || Short > 0.0;
									Deepest = std::max(Deepest, Short);
								}
							}
							UnderBad += bCut;
						}
						// (and his lines, as the scene draws them (LineLies, as slack as they are now, from his harness where the
						// scene draws it: DrawnHarness, in the mesh's own space, not lifted by the ground under it): each of their
						// points but his harness, and their straights at seven places each from the first on, the cord's middle
						// never under the ground (a review: a line across its silk had gone 0.3 m into a slope falling away
						// downwind, and 0.6 m into a bank, between his harness and its edge; a second review: drawn from his
						// harness as it is drawn, not as this had lifted it onto the ground, they cut 0.23 m into hummocks))
						{
							const V3 H = DrawnHarness(Dn);
							for (size_t K = 0; K < M.Seams.size(); ++K)
							{
								if (!LineBack(K, M.Seams.size())) continue;
								V3 Way[LinePoints];
								const int Points = LineLies(M, K, H, LineSlack(Sk / 20.0), Way);
								const double Least = LineOver(Field, Way, Points);
								LinesUnder += Least < 0.0;
								LinesDeepest = std::max(LinesDeepest, -Least);
							}
						}
						if (Sk == 20 && Field < 4)
						{
							const int Cols2 = Dn.Gores * 4, Rings2 = static_cast<int>((M.Points.size() / 2 - 1) / static_cast<size_t>(Cols2));
							for (int R = 0; R < Rings2; ++R)
							{
								double Lowest = 1e9;
								for (int Col = 0; Col < Cols2; ++Col)
								{
									const V3& Pt = M.Points[static_cast<size_t>(R * Cols2 + Col)];
									Lowest = std::min(Lowest, Pt.Z - Local(Pt.X, Pt.Y));
								}
								Floats = std::max(Floats, Lowest);
							}
						}
					}
				}
			}
			std::printf("     the verdict's canopies: side-on %d off, lying %d off, folds %d even, %d alike, %d falling in badly, %d points or triangles under the ground (%.3f m at the deepest), on a slope its underside %.3f m over the ground at most; %d off from above\n", SkirtBad, LieBad, FoldBad, SameBad, FallBad, UnderBad, Deepest, Floats, AboveBad);
			std::printf("     on the same fields falling in and lying, %d of his lines under the ground anywhere (their middle %.3f m over it at the lowest)\n", LinesUnder, -LinesDeepest);
			std::printf("     on a field of hummocks a make's shape (its ground read on a quarter-metre grid, its edges kept over it) %.2f ms on average and %.2f at most here\n",
				HummockMs / std::max(1, HummockMakes), HummockMsMost);
			Check(SkirtBad == 0, "chutes: side-on his canopy is widest a quarter to a half of its depth under its crown and curves in to a hem 0.8 to 0.9 as wide, its silk heading in toward its lines (the art director's verdict of 24 November 14:34Z, (c)); curving in all the way down from its widest, leaning in more ring by ring (under 0.985 of its widest 0.6 of its depth down), its crown round (under 0.65 of its widest 11% under its top)");
			Check(LieBad == 0, "chutes: down his canopy lies 2 to 3 times as long along the wind as it is wide, its far end up 0.3 to 0.8 m and coming round and down to the ground (its last ring a quarter to three quarters as high, its tip on it), its near end gathered 2 to 3.6 m past his harness, its lines running back to him from it, none longer than they are (her verdict, (b); a review of 24 November)");
			Check(FoldBad == 0 && SameBad == 0, "chutes: it lies in a few long folds running along it, their heights uneven and their spacing uneven (each on its own bar), and no two pilots' alike (her verdict, (b); a review of 24 November)");
			Check(AboveBad == 0, "chutes: from above it lies uneven (her note of 24 November 17:40Z): more of its silk on one side of its wind line, its edges and its hem wavy; three or four folds across it, light bands along it; its far end's lift slumped to one side; one to six of its lines across its silk and none under it; its luma across its middle about her 126 to 221 (over 300 pilots' numbers and the verdict's four)");
			Check(BodyBad == 0, "chutes: from above a few of his lines lie across its body, not only its near end (her note of 24 November 17:40Z, (3), after a second look): one to four 0.3 m or more past 0.15 of its length on each (1.8 on the mean, and two on his and the second pilot's), each 1 to 3 m on its silk (2.4 on the mean at most), slanted over its silk and down into its folds, aside of where the rest gather, lying on it (its middle within 2.6 cm of its top on the mean, 4 cm for an eighth of it at most, never under it by 8 mm), drawn no longer than a real line (over 300 pilots' numbers and the verdict's four)");
			Check(FarBad == 0, "chutes: from above its far end is broken up, not a neat dome (her note of 24 November 17:40Z, (2), after a second look): its vent off its middle or its seams kinked over it, a crease across its dome, an edge folded back under (over 300 pilots' numbers and the verdict's four)");
			Check(FallBad == 0, "chutes: falling in it blows downwind and down, longer along the wind, never back toward him nor rising (her verdict, (b))");
			Check(SettleBad == 0, "chutes: as it settles falling in (0.8 to 0.95 of its fall) none of his lines is under its silk's top by 8 mm, those across it nor the rest (a review of 24 November 17:40Z, after a second look: they had sunk into its folds, then risen through them)");
			Check(LinesUnder == 0 && Swept.LinesUnder == 0, "chutes: none of his lines is under the ground falling in or lying, those across his canopy nor the rest, drawn from his harness where the scene draws it, on the same fields (a review of 24 November 17:40Z, after a second look; the verdict's four and 300 pilots' numbers)");
			Check(Swept.LinesUnderBetween == 0 && FlungMost <= 1.75, "chutes: between makes too, as the scene moves them, none of his lines' middles more than 3 mm under the ground; and his lines across its silk run from his harness along or over the grass to it, never flung up off it like a pole (a third review: from his harness in a bank they had stood 5 to 7 m up): the points they go through across the grass no more than 1.75 m over both the taut straight from his harness and the ground, at makes and between, on the same fields (300 pilots' numbers, each 0.0125 of the fall)");
			Check(UnderBad == 0 && Swept.Under == 0 && Floats < 0.08, "chutes: none of his canopy is under the ground falling in or lying, its points nor its triangles' edges, on a slope of 1 in 4 either way along the wind or across it, on hummocks, across a hedgerow bank, a step, a ditch, a sharp crest, a crater's rim or along a bank (the verdict's four; its points on 300 pilots' numbers too); and lying on a slope its underside on the ground, not floating (her verdict, (b))");
		}

		// The swap's lying pose (her third note; a review of 24 November): the crowd's man is a rider in go_prone held at
		// its end, nothing taken back, so his hips are its end offset forward and right of his feet, and as low as its
		// pelvis track says; the mesh's man flat must lie there. Every soldier file that has them, read as the game reads
		// them (its go_prone's end_offset, its pelvis's rest).
		{
			std::string Dir = std::getenv("LAND_AIR_SOLDIERS") ? std::getenv("LAND_AIR_SOLDIERS") : "";
			if (Dir.empty())
			{
				const std::string Here = __FILE__;
				Dir = Here.substr(0, Here.find_last_of('/')) + "/../../Content/Models/Soldiers";
			}
			std::vector<std::string> Files;
			std::error_code Why;
			for (const std::filesystem::directory_entry& E : std::filesystem::directory_iterator(Dir, Why))
			{
				const std::string Name = E.path().filename().string();
				if (Name.rfind("soldier_", 0) == 0 && Name.size() > 5 && Name.substr(Name.size() - 5) == ".json") Files.push_back(E.path().string());
			}
			std::sort(Files.begin(), Files.end());
			int Read = 0, Offsets = 0, Off = 0, OwnOff = 0;
			double Most = 0.0, OwnMost = 0.0, WasShort = 1e9;
			for (const std::string& Path : Files)
			{
				std::string Text, Why2;
				LandJson::Json J;
				if (!LandJson::ReadFile(Path, Text) || !LandJson::Parse(Text, J, Why2)) continue;
				++Read;
				const std::vector<double> End = LandJson::Numbers(J["clips"]["go_prone"]["end_offset"]);
				double PelvisX = 0.0, PelvisZ = -1.0;
				const LandJson::Json& Bones = J["bones"];
				for (size_t K = 0; K < Bones.Count(); ++K)
				{
					if (Bones.At(K)["name"].Str("") != "pelvis") continue;
					const std::vector<double> Head = LandJson::Numbers(Bones.At(K)["head"]);
					if (Head.size() >= 3) { PelvisX = Head[0]; PelvisZ = Head[2]; }
				}
				if (End.size() < 3 || PelvisZ < 0.0) continue;
				++Offsets;
				// (where his hips lie in the file: forward, right and up of his seat)
				const V3 Lies{ PelvisX + End[0], End[1], PelvisZ + End[2] };
				const V3 ByDefault = OverHips(1.0, ProneEndX, ProneEndY), ByOwn = OverHips(1.0, End[0], End[1]);
				const double Miss = Length(Plus(ByDefault, Mul(Lies, -1.0))), OwnMiss = Length(Plus(ByOwn, Mul(Lies, -1.0)));
				Most = std::max(Most, Miss);
				OwnMost = std::max(OwnMost, OwnMiss);
				Off += Miss > 0.03;
				OwnOff += OwnMiss > 0.02;
				// (as it was: pitched 84 degrees and moved back 0.9 m, his hips 0.1 m behind his feet)
				WasShort = std::min(WasShort, Lies.X - (0.8 * std::sin(84.0 * 3.14159265358979323846 / 180.0) - 0.9));
			}
			const V3 Standing = OverHips(0.0, ProneEndX, ProneEndY);
			std::printf("     the swap's lying pose: %d soldier files read, %d with go_prone's end offset; the mesh's man flat %.3f m from the crowd's man's hips at most by his own file's offset, %.3f by LandAir's (he had lain %.2f m short)\n",
				Read, Offsets, OwnMost, Most, WasShort);
			Check(Offsets >= 10 && Off == 0 && OwnOff == 0 && Length(Plus(Standing, Mul(V3{ 0.0, 0.0, OverHip }, -1.0))) < 1e-12,
				"chutes: flat in his chute's mesh his hips lie where the crowd's man's do in every soldier file (go_prone's end offset, and its pelvis as it lies), on his feet as he lands (her third: no pop)");
		}
		// The crowd's man a downed pilot becomes (Effects, R2; the art director, 02:26Z 24 November: "On the ground: no
		// steel helmet, no rifle, no pack. Use the tanker files where they exist (USA, GER) ... Other armies get infantry
		// with the helmet hidden"), dressed as LandBattleAir dresses him (Crowd::PilotDress) and made as it makes him
		// (AddRider): in the soldier files of LAND_AIR_SOLDIERS, each level of his look his file's triangles without the
		// runs his dress leaves off, nothing in their place, every kept point as the whole man's.
		{
			std::string Dir = std::getenv("LAND_AIR_SOLDIERS") ? std::getenv("LAND_AIR_SOLDIERS") : "";
			std::string ArmiesPath = std::getenv("LAND_AIR_ARMIES") ? std::getenv("LAND_AIR_ARMIES") : "";
			const std::string Here = __FILE__;
			if (Dir.empty()) Dir = Here.substr(0, Here.find_last_of('/')) + "/../../Content/Models/Soldiers";
			if (ArmiesPath.empty()) ArmiesPath = Here.substr(0, Here.find_last_of('/')) + "/../../Content/Data/LandBattles/armies.json";
			LandMen::Crowd Men;
			std::string ArmiesText, Why;
			LandJson::ReadFile(ArmiesPath, ArmiesText);
			int Files = 0;
			for (const char* Name : { "USA", "USA_tanker", "GER", "GER_panzer", "SOV", "UK" })
			{
				std::string Text;
				auto Sol = std::make_shared<LandMen::Soldier>();
				if (!LandJson::ReadFile(Dir + "/soldier_" + Name + ".json", Text) || !Sol->Load(Text, Why)) continue;
				Men.AddSoldier(Name, Sol);
				++Files;
			}
			const std::string Day = "{\"seed\": 23, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]}, \"blocks\": [], \"men\": [], \"rounds\": []}";
			const LandMen::GroundFn Flat = [](double, double) { return LandMen::Ground(); };
			const bool bBuilt = Men.ReadArmies(ArmiesText, Why) && Men.Build(Day, Flat, Why);
			int Dressed = 0, Undressed = 0;
			bool bPackCut = false;
			for (const char* Nat : { "USA", "GER", "SOV", "UK" })
			{
				const LandMen::Crowd::Dress D = Men.PilotDress(Nat);
				const LandMen::Soldier* Sol = bBuilt ? Men.SoldierFile(D.File) : nullptr;
				if (!Sol) continue;
				const std::string Crew = std::string(Nat) == "USA" ? "USA_tanker" : std::string(Nat) == "GER" ? "GER_panzer" : std::string();
				const bool bCrewHere = !Crew.empty() && Men.SoldierFile(Crew) != nullptr;
				// (his dress: the tankers' own as it is; else infantry, bareheaded, no pack)
				const bool bDress = bCrewHere ? D.bTankers && !D.bBareHead && !D.bNoPack : !D.bTankers && D.bBareHead && D.bNoPack;
				const uint64_t Seed = 0xC47E2ull ^ 7ull * 0x9E3779B97F4A7C15ull;
				const int Man = Men.AddRider(D.File, Nat, Seed, D.Row, D.bBareHead, D.bNoPack), Whole = Men.AddRider(D.File, Nat, Seed, D.Row);
				if (Man < 0 || Whole < 0) { ++Undressed; continue; }
				const int Clip = Sol->ClipIndex("go_prone") >= 0 ? Sol->ClipIndex("go_prone") : 0;
				Men.SetRide(Man, PackCut::Standing(Clip, 0.1f));
				Men.SetRide(Whole, PackCut::Standing(Clip, 0.1f));
				Men.Update(1.0 + 0.1 * Dressed);
				std::string Line, Fault;
				for (int L = 0; L < LandMen::LodCount; ++L)
				{
					const LandMen::Lod Lv = static_cast<LandMen::Lod>(L);
					std::vector<PackCut::Run> Runs;
					if (D.bBareHead) Runs.push_back(PackCut::HelmetRun(*Sol, Lv));
					if (D.bNoPack) Runs.push_back(PackCut::PackRun(*Sol, Lv));
					const PackCut::Cut Want = PackCut::CutOf(Sol->Meshes[L], Runs);
					const std::string Off = PackCut::Compare(Men, Man, Whole, Lv, Want);
					if (!Off.empty() && Fault.empty()) Fault = Off;
					bPackCut = bPackCut || (D.bNoPack && Sol->PackCount[L] > 0);
					char Buf[96];
					std::snprintf(Buf, sizeof(Buf), "%s%d/%zu", L ? ", " : "", static_cast<int>(Want.Triangles.size() / 3), Sol->Meshes[L].Triangles.size() / 3);
					Line += Buf;
				}
				if (!bDress || !Fault.empty()) ++Undressed;
				++Dressed;
				std::printf("     a downed %s pilot: soldier_%s%s%s%s, near/mid/far %s triangles drawn%s%s\n", Nat, D.File.c_str(), D.bTankers ? " (the tankers')" : "",
					D.bBareHead ? ", bareheaded" : "", D.bNoPack ? (Sol->PackCount[LandMen::Near] > 0 ? ", no pack" : ", no pack asked (his file marks none: his pack on)") : "",
					Line.c_str(), Fault.empty() ? "" : ": ", Fault.c_str());
			}
			Check(bBuilt && Files >= 2 && Dressed >= 2 && Undressed == 0,
				std::string("chutes: a downed pilot's man of the crowd (PilotDress, AddRider): USA and GER in the tankers' files as they are, SOV and UK in their infantry file bareheaded and without his pack") +
				(bPackCut ? " (the pack's run left out, nothing in its place)" : " (no file here marks its pack: drawn with it, as before)"));
		}
		if (Out.is_open())
		{
			// (his canopy for chutes.py's look: the first pilot's full; as it lies once down, his and the second pilot's;
			// and his falling in, a quarter at a time. Down, where his harness lies and the way he faces from the way it
			// falls, and the seams his lines run back from)
			std::ofstream Silk(std::string(Argv[1]) + "/chute_silk.txt");
			const Watched& W = Sn.begin()->second;
			const Watched& W2 = std::next(Sn.begin())->second;
			for (int Which = 0; Which < 7; ++Which)
			{
				SilkMesh M;
				const Chute& Dn = Which == 2 ? W2.Lies : W.Lies;
				const double Off = std::hypot(Dn.Heap.X - Dn.Landed.X, Dn.Heap.Y - Dn.Landed.Y);
				if (Which == 0) CanopyShape(W.Look, 1.0, M);
				else if (Which <= 2) HeapShape(Dn, 1.0, Off, M);
				else HeapShape(Dn, (Which - 3) * 0.25, Off, M);
				Silk << "mesh " << Which << " " << (Which == 0 ? W.Look.Gores : Dn.Gores) << "\n";
				if (Which > 0)
				{
					const double Turn = Dn.Face - Dn.HeapHeading;
					const V3 H = OverHarness(1.0, ProneEndX, ProneEndY);
					const double Tr = Turn * 3.14159265358979323846 / 180.0;
					Silk << "h " << H.X * std::cos(Tr) - H.Y * std::sin(Tr) << " " << H.X * std::sin(Tr) + H.Y * std::cos(Tr) << " " << H.Z << " " << Turn << "\n";
					for (size_t K = 0; K < M.Seams.size(); ++K) if (LineBack(K, M.Seams.size())) Silk << "s " << M.Seams[K] << " " << K << "\n";
					// (each line's points as the scene draws them lying, LineLies')
					const V3 Hl{ H.X * std::cos(Tr) - H.Y * std::sin(Tr), H.X * std::sin(Tr) + H.Y * std::cos(Tr), H.Z };
					for (size_t K = 0; K < M.Seams.size(); ++K)
					{
						if (!LineBack(K, M.Seams.size())) continue;
						V3 Way[LinePoints];
						const double Slack = Which <= 2 ? 1.0 : LineSlack((Which - 3) * 0.25);
						const int Points = LineLies(M, K, Hl, Slack, Way);
						Silk << "l";
						for (int Pt = 0; Pt < Points; ++Pt) Silk << " " << Way[Pt].X << " " << Way[Pt].Y << " " << Way[Pt].Z;
						Silk << "\n";
					}
				}
				for (size_t K = 0; K < M.Points.size(); ++K)
					Silk << "p " << M.Points[K].X << " " << M.Points[K].Y << " " << M.Points[K].Z << " " << M.Facings[K].X << " " << M.Facings[K].Y << " " << M.Facings[K].Z << " " << M.Colours[K * 3] << " " << M.Colours[K * 3 + 1] << " " << M.Colours[K * 3 + 2] << "\n";
				for (size_t K = 0; K + 2 < M.Triangles.size(); K += 3) Silk << "t " << M.Triangles[K] << " " << M.Triangles[K + 1] << " " << M.Triangles[K + 2] << "\n";
			}
		}
	}
	std::printf("land_air_test: %s\n", Failed ? "FAILED" : "all passed");
	return Failed ? 1 : 0;
}
