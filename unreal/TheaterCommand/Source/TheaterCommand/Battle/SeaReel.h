#pragma once

// The BEFORE reel's measured shots (the ship detail plan, R1.1; the Mac's run sheet, sea-scene/ship-detail/mac/BEFORE-REEL.md).
// Two look-only switches for review pictures, each off by default, taken only in a picture run (-TheaterSeaBattleShots=)
// and never in the month's scene, so with neither asked the scene is as it was:
// - -TheaterSeaReel=<shot>[,<shot>...] keeps only the planned shots named, by their names without their numbers ("under":
//   the first window under the water, never its -burst picture), their numbers and moments unchanged; holds each after its
//   picture until 8 s after it or 3 s before the next kept shot, whichever comes first; and says a line of each one's
//   numbers: its frames by thread (read as the threads' line reads them), the ship parts on screen and on their far mesh,
//   the draw calls and primitives, and the ships laid in it; and a line for each ship laid, with the game thread's time
//   for it, and their sum at the end.
// - -TheaterSeaRideStrip=<ship>,<from>,<seconds> replaces the shots with one that follows that ship (a hit's eye), a
//   picture "<key>-ride-<ship>-NNNN.png" and a line every frame from <from> for <seconds> (1 to 30), then quits. With
//   -benchmark -fps=60 the game steps 1/60 s a frame, so the strip is a true 60 frames a second however slowly it draws.
// No line here holds "Sea battle: threads" or "frame times over", so the bench's reader (Tools/sea_waves/bench_table.py)
// never takes one for its own. ASeaBattle (Battle/SeaBattle.cpp) asks this for its rules and its lines. Engine-free:
// Tools/ship_detail/reel_test.cpp builds it in the cloud.

#include "Battle/SeaFrames.h"
#include "Battle/SeaLookSwitches.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace SeaReel
{
	inline std::string Trimmed(const std::string& Text)
	{
		size_t From = 0, To = Text.size();
		while (From < To && std::isspace(static_cast<unsigned char>(Text[From])) != 0)
		{
			++From;
		}
		while (To > From && std::isspace(static_cast<unsigned char>(Text[To - 1])) != 0)
		{
			--To;
		}
		return Text.substr(From, To - From);
	}

	/** A value's parts between its commas, each trimmed (an empty part kept, so "64,,10" has three). */
	inline std::vector<std::string> Parts(const std::string& Value)
	{
		std::vector<std::string> Out;
		std::string One;
		for (const char Ch : Value)
		{
			if (Ch == ',')
			{
				Out.push_back(Trimmed(One));
				One.clear();
			}
			else
			{
				One.push_back(Ch);
			}
		}
		Out.push_back(Trimmed(One));
		return Out;
	}

	/** A planned shot's name without its number ("05-a-hit" is "a-hit", "112-the-end" "the-end"); a name with none, as it is. */
	inline std::string Unnumbered(const std::string& Name)
	{
		size_t Digits = 0;
		while (Digits < Name.size() && std::isdigit(static_cast<unsigned char>(Name[Digits])) != 0)
		{
			++Digits;
		}
		return Digits > 0 && Digits < Name.size() && Name[Digits] == '-' ? Name.substr(Digits + 1) : Name;
	}

	/** The shots -TheaterSeaReel= asks for, in its order, each once, without any number given with it ("05-a-hit" asks for
	 * "a-hit"); none for "off", "0" or nothing. */
	inline std::vector<std::string> ReadReel(const std::string& Value)
	{
		std::vector<std::string> Out;
		const std::string Whole = Trimmed(Value);
		if (Whole.empty() || Whole == "off" || Whole == "0")
		{
			return Out;
		}
		for (const std::string& One : Parts(Whole))
		{
			const std::string Name = Unnumbered(One);
			if (!Name.empty() && std::find(Out.begin(), Out.end(), Name) == Out.end())
			{
				Out.push_back(Name);
			}
		}
		return Out;
	}

	/** Whether the reel keeps a planned shot: its name, without its number, asked for; or "under" asked and this the first
	 * window under the water that is not a -burst picture (bFirstUnder: the scene says which that is). */
	inline bool Keeps(const std::vector<std::string>& Asked, const std::string& Name, bool bFirstUnder)
	{
		const std::string Bare = Unnumbered(Name);
		for (const std::string& One : Asked)
		{
			if (One == Bare || (One == "under" && bFirstUnder))
			{
				return true;
			}
		}
		return false;
	}

	/** How long a kept shot is held after its picture (battle seconds): until 8 s after it (At, the picture's moment) or 3 s
	 * before the next kept shot's moment (NextAt), whichever comes first, and never before the picture itself. */
	inline double HoldUntil(double At, double NextAt)
	{
		return std::max(At, std::min(At + 8.0, NextAt - 3.0));
	}

	/** -TheaterSeaRideStrip=<ship>,<from>,<seconds>: the ship followed (her key in the show), from when (battle seconds) and
	 * for how long (1 to 30 s). Off unless all three read. */
	struct Ride
	{
		bool bOn = false;
		int Ship = -1;
		double From = 0.0, Seconds = 0.0;
	};

	inline Ride ReadRide(const std::string& Value)
	{
		Ride Out;
		const std::vector<std::string> Bits = Parts(Value);
		if (Bits.size() != 3 || Bits[0].empty() || Bits[1].empty() || Bits[2].empty())
		{
			return Out;
		}
		char* End = nullptr;
		const long Ship = std::strtol(Bits[0].c_str(), &End, 10);
		const bool bShip = End && *End == '\0' && Ship >= 0 && Ship < 1000000000L;
		const double From = std::strtod(Bits[1].c_str(), &End);
		const bool bFrom = End && *End == '\0' && std::isfinite(From) && From >= 0.0;
		const double Seconds = std::strtod(Bits[2].c_str(), &End);
		const bool bSeconds = End && *End == '\0' && std::isfinite(Seconds) && Seconds > 0.0;
		if (!bShip || !bFrom || !bSeconds)
		{
			return Out;
		}
		Out.bOn = true;
		Out.Ship = static_cast<int>(Ship);
		Out.From = From;
		Out.Seconds = std::clamp(Seconds, 1.0, 30.0);
		return Out;
	}

	/** The give either end of a ride strip, battle seconds (a quarter of a 60th: the engine's fixed step of 1/60 s is a
	 * float's, so 1,200 steps from a skip land a hair past or short of the second asked). */
	inline constexpr double RideGive = 0.004;

	/** Whether the battle's clock is within the ride strip, its first and last moments both taken. */
	inline bool InRide(const Ride& R, double BattleT)
	{
		return R.bOn && BattleT >= R.From - RideGive && BattleT <= R.From + R.Seconds + RideGive;
	}

	/** Whether the ride strip is over. */
	inline bool RideOver(const Ride& R, double BattleT)
	{
		return R.bOn && BattleT > R.From + R.Seconds + RideGive;
	}

	inline std::string RideName(int Ship)
	{
		return "ride-" + std::to_string(Ship);
	}

	/** A ride strip's picture: "<key>-ride-<ship>-NNNN.png" ("samar-ride-64-0000.png"). */
	inline std::string RidePicture(const std::string& Key, int Ship, int Frame)
	{
		char Number[16];
		std::snprintf(Number, sizeof(Number), "%04d", Frame);
		return Key + "-" + RideName(Ship) + "-" + Number + ".png";
	}

	/** The two switches as the scene takes them (ASeaBattle::OpenOn), from the look it read, each one's value in force
	 * written back so the "Sea look:" line says what was applied: the shots kept ("funnels,a-hit"), the ride strip as read
	 * ("64,200.000,10"), or "off" where it is not taken (no picture run, the month's scene, nothing readable). Asked for
	 * together, the ride strip is taken and the reel is not. */
	struct Taken
	{
		std::vector<std::string> Shots;
		Ride Strip;
	};

	inline SeaLookSwitches::Asked* AskedFor(SeaLookSwitches::Look& InForce, const char* Name)
	{
		for (SeaLookSwitches::Asked& One : InForce.Given)
		{
			if (One.Index >= 0 && One.Index < SeaLookSwitches::Count && std::string(SeaLookSwitches::All[One.Index].Name) == Name)
			{
				return &One;
			}
		}
		return nullptr;
	}

	inline Taken Take(SeaLookSwitches::Look& InForce, bool bPictureRun)
	{
		Taken Out;
		SeaLookSwitches::Asked* Strip = AskedFor(InForce, "TheaterSeaRideStrip=");
		SeaLookSwitches::Asked* Reel = AskedFor(InForce, "TheaterSeaReel=");
		if (Strip)
		{
			Out.Strip = bPictureRun ? ReadRide(Strip->Value) : Ride();
			char Said[96];
			std::snprintf(Said, sizeof(Said), "%d,%.3f,%g", Out.Strip.Ship, Out.Strip.From, Out.Strip.Seconds);
			Strip->Value = Out.Strip.bOn ? std::string(Said) : std::string("off");
		}
		if (Reel)
		{
			Out.Shots = bPictureRun && !Out.Strip.bOn ? ReadReel(Reel->Value) : std::vector<std::string>();
			std::string Said;
			for (const std::string& One : Out.Shots)
			{
				Said += (Said.empty() ? "" : ",") + One;
			}
			Reel->Value = Said.empty() ? std::string("off") : Said;
		}
		return Out;
	}

	/** What a shot of each kind shows, in the order of ESeaShot (Battle/SeaBattle.h; reel_test holds the two together). */
	inline constexpr const char* KindWords[] = { "the fleets", "the line", "a broadside", "the fall of shot", "a hit", "torpedo tracks", "a strike",
		"going down", "the whole battle", "a fire", "the funnels", "the water", "both lines", "under the water" };
	inline constexpr int Kinds = static_cast<int>(sizeof(KindWords) / sizeof(KindWords[0]));

	inline const char* WordsOf(int Kind)
	{
		return Kind >= 0 && Kind < Kinds ? KindWords[Kind] : "a shot";
	}

	/** Whether a frame counts toward a kept shot's numbers: the camera on the shot to be pictured next (bOnShot) and settled
	 * after its cut, not a frame apart (a skip's or a picture's), from the battle's second second of play (RealT, the scene's
	 * real clock), a real frame (Took under half a minute: longer is the game held), and not within 0.6 s of the picture:
	 * before it by the battle's clock (ShotAt, its moment, less BattleT), after it by the real clock (SincePicture, once
	 * bPictured). */
	inline bool Counts(bool bOnShot, bool bSettled, bool bApart, double RealT, double Took, double ShotAt, double BattleT, bool bPictured, double SincePicture)
	{
		return bOnShot && bSettled && !bApart && RealT >= 1.0 && Took < 30.0 && (bPictured ? SincePicture >= 0.6 : ShotAt - BattleT >= 0.6);
	}

	/** A kept shot's numbers as it is held: its frames by thread, and in each counted frame the ship parts on screen and on
	 * their far mesh, the draw calls and primitives (below 0: not read on this engine); the ships laid in it and their time. */
	struct Held
	{
		SeaFrames::Record Threads;
		std::vector<float> Parts, FarParts, Calls, Prims;
		bool bCallsRead = false;
		int Laid = 0;
		double LaidMs = 0.0;

		void Add(double GameMs, double RenderMs, double RhiMs, double ChipMs, double WholeMs, double BattleT, int InParts, int InFar, int InCalls, int InPrims)
		{
			if (Threads.Num() >= SeaFrames::Record::MostFrames)
			{
				return;
			}
			Threads.Add(GameMs, RenderMs, RhiMs, ChipMs, WholeMs, 0, BattleT);
			Parts.push_back(static_cast<float>(InParts));
			FarParts.push_back(static_cast<float>(InFar));
			if (InCalls >= 0 && InPrims >= 0)
			{
				bCallsRead = true;
				Calls.push_back(static_cast<float>(InCalls));
				Prims.push_back(static_cast<float>(InPrims));
			}
		}

		void Lay(double Ms)
		{
			++Laid;
			LaidMs += Ms;
		}
	};

	/** The middle, the 95th in the hundred and the most of some values (the threads' line's own rule, SeaFrames::ShareOf). */
	struct Three
	{
		double Middle = 0.0, P95 = 0.0, Most = 0.0;
	};

	inline Three ThreeOf(std::vector<float> Values)
	{
		Three Out;
		if (Values.empty())
		{
			return Out;
		}
		std::sort(Values.begin(), Values.end());
		Out.Middle = SeaFrames::ShareOf(Values, 0.5);
		Out.P95 = SeaFrames::ShareOf(Values, 0.95);
		Out.Most = Values.back();
		return Out;
	}

	/** A kept shot's line, said as the reel moves on from it: "Sea battle: reel 05-a-hit (a hit, ship 1024) at 235.3 s: N
	 * frames from A to B s; game m/p95/p99 ms, render ..., graphics chip ..., the most of the three ..., the whole frame ...
	 * (mean/95th/99th); ship parts on screen middle/95th/most, F on their far mesh at the middle; draw calls ... and
	 * primitives ... million (middle/95th/most); K ships laid in it, X ms." Subject below 0: no ship named. */
	inline std::string ShotLine(const std::string& Name, const std::string& What, int Subject, double At, const Held& H)
	{
		char Head[256], Laid[96];
		if (Subject >= 0)
		{
			std::snprintf(Head, sizeof(Head), "Sea battle: reel %s (%s, ship %d) at %.1f s: ", Name.c_str(), What.c_str(), Subject, At);
		}
		else
		{
			std::snprintf(Head, sizeof(Head), "Sea battle: reel %s (%s) at %.1f s: ", Name.c_str(), What.c_str(), At);
		}
		std::snprintf(Laid, sizeof(Laid), "%d ships laid in it, %.1f ms.", H.Laid, H.LaidMs);
		const SeaFrames::Record& R = H.Threads;
		if (R.Num() == 0)
		{
			return std::string(Head) + "no frames counted; " + Laid;
		}
		const SeaFrames::Spread G = SeaFrames::Of(R.Game), D = SeaFrames::Of(R.Render), C = SeaFrames::Of(R.Chip), M = SeaFrames::Of(R.Most), W = SeaFrames::Of(R.Whole);
		const Three P = ThreeOf(H.Parts), F = ThreeOf(H.FarParts);
		char Body[768], Calls[192];
		std::snprintf(Body, sizeof(Body),
			"%d frames from %.1f to %.1f s; game %.2f/%.2f/%.2f ms, render %.2f/%.2f/%.2f ms, graphics chip %.2f/%.2f/%.2f ms, the most of the three "
			"%.2f/%.2f/%.2f ms, the whole frame %.2f/%.2f/%.2f ms (mean/95th/99th); ship parts on screen %.0f/%.0f/%.0f, %.0f on their far mesh at the "
			"middle; ",
			G.N, R.FirstT, R.LastT, G.Mean, G.P95, G.P99, D.Mean, D.P95, D.P99, C.Mean, C.P95, C.P99, M.Mean, M.P95, M.P99, W.Mean, W.P95, W.P99, P.Middle, P.P95,
			P.Most, F.Middle);
		if (H.bCallsRead && !H.Calls.empty())
		{
			const Three N = ThreeOf(H.Calls), Q = ThreeOf(H.Prims);
			std::snprintf(Calls, sizeof(Calls), "draw calls %.0f/%.0f/%.0f and primitives %.2f/%.2f/%.2f million (middle/95th/most); ", N.Middle, N.P95, N.Most,
				Q.Middle / 1.0e6, Q.P95 / 1.0e6, Q.Most / 1.0e6);
		}
		else
		{
			std::snprintf(Calls, sizeof(Calls), "draw calls not read; ");
		}
		return std::string(Head) + Body + Calls + Laid;
	}

	/** A ship's laying: "Sea battle: laid ship 64 (dd, JPN, Destroyer_JAP.json) in 3.2 ms of the game thread at 180.0 s
	 * (while skipping)." File empty: the kit's own code-built model. */
	inline std::string LaidLine(int Ship, const std::string& Class, const std::string& Navy, const std::string& File, double Ms, double BattleT, bool bSkipping)
	{
		char Text[384];
		std::snprintf(Text, sizeof(Text), "Sea battle: laid ship %d (%s, %s, %s) in %.1f ms of the game thread at %.1f s%s.", Ship, Class.c_str(), Navy.c_str(),
			File.empty() ? "the kit's own model" : File.c_str(), Ms, BattleT, bSkipping ? " (while skipping)" : "");
		return Text;
	}

	/** The layings' sum at the end: "Sea battle: reel: 23 ships laid, 3.1 ms at the middle, 6.0 at the 95th, 9.4 at most." */
	inline std::string LaidSum(const std::vector<float>& Ms)
	{
		if (Ms.empty())
		{
			return "Sea battle: reel: no ships laid.";
		}
		const Three T = ThreeOf(Ms);
		char Text[192];
		std::snprintf(Text, sizeof(Text), "Sea battle: reel: %d ships laid, %.1f ms at the middle, %.1f at the 95th, %.1f at most.", static_cast<int>(Ms.size()), T.Middle,
			T.P95, T.Most);
		return Text;
	}

	/** A ride strip's frame: "Sea battle: ride frame 0000 at 200.000 s, ship 64 (dd): heel +1.23, pitch -0.40 degrees,
	 * heave +0.12 m." */
	inline std::string RideLine(int Frame, double BattleT, int Ship, const std::string& Class, double Heel, double Pitch, double Heave)
	{
		char Text[256];
		std::snprintf(Text, sizeof(Text), "Sea battle: ride frame %04d at %.3f s, ship %d (%s): heel %+.2f, pitch %+.2f degrees, heave %+.2f m.", Frame, BattleT, Ship,
			Class.c_str(), Heel, Pitch, Heave);
		return Text;
	}

	/** The reel's opening line, once its shots are kept: what was asked, what was kept of the plan, and any name the plan has
	 * no shot for. Kept: the kept shots' names (numbered) and moments. */
	inline std::string OpenLine(const std::vector<std::string>& Asked, int Planned, const std::vector<std::pair<std::string, double>>& Kept,
		const std::vector<std::string>& Missing)
	{
		std::string Text = "Sea battle: reel of";
		for (size_t K = 0; K < Asked.size(); ++K)
		{
			Text += (K == 0 ? " " : ", ") + Asked[K];
		}
		char Count[96];
		std::snprintf(Count, sizeof(Count), ": %d of the %d planned shots kept", static_cast<int>(Kept.size()), Planned);
		Text += Count;
		for (size_t K = 0; K < Kept.size(); ++K)
		{
			char One[160];
			std::snprintf(One, sizeof(One), "%s%s at %.1f s", K == 0 ? " (" : ", ", Kept[K].first.c_str(), Kept[K].second);
			Text += One;
		}
		Text += Kept.empty() ? "" : ")";
		if (!Missing.empty())
		{
			Text += "; none planned for";
			for (size_t K = 0; K < Missing.size(); ++K)
			{
				Text += (K == 0 ? " " : ", ") + Missing[K];
			}
		}
		return Text + "; each held after its picture until 8 s after it or 3 s before the next kept shot.";
	}
}
