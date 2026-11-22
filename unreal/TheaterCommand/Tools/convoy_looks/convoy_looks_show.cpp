// The submarine scene's looks against the show itself: a recorded convoy night (the sea show's stand-in, read only)
// played through the game's own Battle/SeaShow at 30 frames a second, and the looks' core (Battle/ConvoyLooksCore) fed
// from it as the scene feeds it. Checks that what the looks draw agrees with what the show does: every boat, as the
// show steers her, never pops where she can be seen, rides surfaced where the show has her, and shows of the model the
// stage draws (the kit's code-built submarine, typed here from its code, apart from the looks' own rows) only what her
// state says: at periscope depth the head of her taller periscope while the show has it up and nothing while it is
// down, nothing deep, the swirl as the last of her bridge goes under and feathers only with all of it under; every
// pattern's charges go in where the show bursts them, and before; the ahead thrower's ring is in before its bursts; a
// starshell lights and sinks. Plain C++, no engine. Usage: Tools/convoy_looks/run.sh (it runs this after the core's).

#include "Battle/AirFeed.h"
#include "Battle/ConvoyLooksCore.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using namespace ConvoyLook;

	int ShowFailures = 0, ShowChecks = 0;

	void ShowCheck(bool bOk, const std::string& What)
	{
		++ShowChecks;
		if (!bOk)
		{
			++ShowFailures;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	std::string Said(const char* Format, double A, double B = 0.0, double C = 0.0)
	{
		char Line[256];
		std::snprintf(Line, sizeof(Line), Format, A, B, C);
		return Line;
	}

	/** A charge as the looks drew it going in: its escort, when it lands, where, whether the ahead thrower's (its ring's
	 * middle), and the pattern's sinking time. */
	struct Landing
	{
		int Key = 0;
		double Lands = 0.0;
		P3 To, Middle;
		bool bAhead = false;
		double Sinking = 0.0, Ring = 0.0;
	};

	/**
	 * The boat as the stage draws her, typed from Campaign/CampaignShips.cpp's Submarine() (built by her navy: 67 m by 6.2
	 * for "GER" or none, 95 by 8.3 for "USA", 80 by 6.2 for any other; the stem's rake 2 m forward of the hull) and
	 * stretched by AirStage's FitHull to AirFeed::HullOf's size, steered with the looks' heave and the show's trim
	 * (plus bow up, about her waterline amidships). Heights over the still sea: the bridge's top (6.25 m: deck 1.65 + 4.6)
	 * at both ends of its plan (5.4 forward, 4.6 aft) and the jumping wire's head on it (5.0 forward, 6.5); the fixed
	 * periscopes' heads (1.5 forward, 10.65; amidships, 9.15).
	 */
	struct StageBoat
	{
		double Up = 1.0, Along = 1.0;

		StageBoat(const std::string& Nation, double FitLong, double FitBeam)
		{
			const double Hull = Nation == "USA" ? 95.0 : (Nation.empty() || Nation == "GER") ? 67.0 : 80.0, Beam = Nation == "USA" ? 8.3 : 6.2;
			Up = FitBeam / Beam;
			Along = FitLong / (Hull + 2.0);
		}
		double At(double Heave, double Pitch, double Fore, double High) const
		{
			const double R = Pitch * 3.14159265358979323846 / 180.0;
			return Heave + High * Up * std::cos(R) + Fore * Along * std::sin(R);
		}
		double Bridge(double Heave, double Pitch) const { return std::max({ At(Heave, Pitch, 5.4, 6.25), At(Heave, Pitch, -4.6, 6.25), At(Heave, Pitch, 5.0, 6.5) }); }
		double Scope(double Heave, double Pitch) const { return At(Heave, Pitch, 1.5, 10.65); }
		double Low(double Heave, double Pitch) const { return At(Heave, Pitch, 0.0, 9.15); }
	};

	struct BoatSeen
	{
		BoatState State;
		BoatModel Model;
		Drawn Size;
		StageBoat Stage{ std::string(), 95.0, 8.3 };
		bool bSeen = false;
		double LastHeave = 0.0, WorstPop = 0.0, WorstOff = 0.0, PopAt = 0.0, OffAt = 0.0;
		// (what of the stage's model shows, against her state: frames at periscope depth with the show's periscope up and
		// settled, how many show its head out, the least it shows, the most of her shorter periscope and her bridge; with it
		// down and settled, the most of her highest point; deep, the same)
		int UpFrames = 0, UpShown = 0, DownFrames = 0, DeepFrames = 0;
		double UpLeast = 99.0, UpLowMost = -99.0, UpBridgeMost = -99.0, DownMost = -99.0, DeepMost = -99.0;
		double ScopeSince = -1.0, DownSince = -1.0;
		int Swirls = 0, Feathers = 0;
		double SwirlLeast = 99.0, SwirlMost = -99.0, FeatherBridgeMost = -99.0;
	};

	int PlayNight(const std::string& Path)
	{
		std::ifstream File(Path);
		if (!File)
		{
			std::printf("cannot read %s\n", Path.c_str());
			return 2;
		}
		std::stringstream Text;
		Text << File.rdbuf();
		SeaFeed::Recording Rec;
		std::string Error;
		if (!SeaFeed::ReadRecording(Text.str(), Rec, Error) || Rec.Views.empty())
		{
			std::printf("%s: %s\n", Path.c_str(), Error.c_str());
			return 2;
		}
		SeaShow::Show Night(Rec.Seed ? Rec.Seed : 239);
		Night.SetField(Rec.Board);
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& V = Rec.Views[Taken++];
			Night.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
		};
		TakeNext();
		if (Taken < Rec.Views.size()) TakeNext();

		const SeaShow::Field& Board = Night.Board();
		const double Sky = SkyLit(Board.bLight, Board.SunUp, Board.MoonUp, Board.MoonLit);
		std::map<int, BoatSeen> Boats;
		std::vector<Landing> Landings;
		int Drops = 0, PatternBursts = 0, AheadBursts = 0, Stars = 0;
		double WorstMiss = 0.0, LeastLead = 1e9, AheadOff = -1e9;
		const double Step = 1.0 / 30.0;
		double T = Rec.Views.front().Head.T;
		const double EndT = Rec.Views.back().Head.T;
		while (true)
		{
			Night.Advance(T);
			for (const SeaShow::ShipOut& Ship : Night.Ships())
			{
				if (Ship.Class != "ss" || Ship.Dive.empty()) continue;
				BoatSeen& B = Boats[Ship.Key];
				if (!B.bSeen)
				{
					B.Model = ModelOf(Ship.Nation);
					const AirFeed::HullSize Hull = AirFeed::HullOf("ss", std::string(), Ship.Nation);
					B.Size = DrawnOf(B.Model, Hull.Long, Hull.Beam);
					B.Stage = StageBoat(Ship.Nation, Hull.Long, Hull.Beam);
				}
				BoatIn In;
				In.T = T;
				In.Dive = Ship.Dive;
				In.Keel = Ship.Keel;
				In.Dived = Ship.Dived;
				In.bScope = Ship.bScope;
				In.Heave = Ship.Heave;
				In.Pitch = Ship.Pitch;
				In.Speed = Ship.Speed;
				In.At = P3(Ship.At.X, Ship.At.Y, 0.0);
				In.Heading = Ship.Heading;
				In.ScopeKeel = Board.ScopeKeel;
				In.Lit = Ship.Dive == "deep" ? Sky : 0.0;
				const BoatOut Out = StepBoat(B.State, In, B.Model, B.Size);
				// (the show's very first frame has her untrimmed, no keel: not hers yet, the looks begin her on the next)
				if (!(Ship.Keel > 0.0)) continue;
				const double Top = Out.Heave + B.Model.Top * B.Size.Up;
				// (a pop only counts where some of her can be seen: her top within 3 m of the surface)
				if (B.bSeen && Top > -3.0 && std::fabs(Out.Heave - B.LastHeave) > B.WorstPop)
				{
					B.WorstPop = std::fabs(Out.Heave - B.LastHeave);
					B.PopAt = T;
				}
				// (surfaced, she rides where the show has her, less her trim awash: the deck of the stage's model 0.5 m out)
				const double ShowHeave = Ship.Heave - Ship.Dived;
				if (Ship.Dive == "surfaced" && Ship.Dived < 0.05 && std::fabs(Out.Heave - (ShowHeave - (1.65 * B.Stage.Up - 0.5))) > B.WorstOff)
				{
					B.WorstOff = std::fabs(Out.Heave - (ShowHeave - (1.65 * B.Stage.Up - 0.5)));
					B.OffAt = T;
				}
				// What of the stage's model shows: at periscope depth, settled (the periscope up or down 5 s), the head of her
				// taller periscope out while the show has it up and nothing while it is down; deep, nothing.
				const double Bridge = B.Stage.Bridge(Out.Heave, Ship.Pitch), Scope = B.Stage.Scope(Out.Heave, Ship.Pitch), Low = B.Stage.Low(Out.Heave, Ship.Pitch);
				const bool bAtScope = Ship.Dive == "periscope" && std::fabs(Ship.Keel - Board.ScopeKeel) < 0.05;
				B.ScopeSince = bAtScope && Ship.bScope ? (B.ScopeSince < 0.0 ? T : B.ScopeSince) : -1.0;
				B.DownSince = bAtScope && !Ship.bScope ? (B.DownSince < 0.0 ? T : B.DownSince) : -1.0;
				if (B.ScopeSince >= 0.0 && T - B.ScopeSince > 5.0)
				{
					++B.UpFrames;
					B.UpShown += Scope > 0.0 ? 1 : 0;
					B.UpLeast = std::min(B.UpLeast, Scope);
					B.UpLowMost = std::max(B.UpLowMost, Low);
					B.UpBridgeMost = std::max(B.UpBridgeMost, Bridge);
				}
				if (B.DownSince >= 0.0 && T - B.DownSince > 5.0)
				{
					++B.DownFrames;
					B.DownMost = std::max(B.DownMost, Scope);
				}
				if (Ship.Dive == "deep" && Ship.Keel > Board.ScopeKeel + 6.0)
				{
					++B.DeepFrames;
					B.DeepMost = std::max(B.DeepMost, std::max(Scope, Bridge));
				}
				for (const BoatSplash& Sp : Out.Splashes)
				{
					if (Sp.Kind == SplashKind::Swirl)
					{
						++B.Swirls;
						B.SwirlLeast = std::min(B.SwirlLeast, Bridge);
						B.SwirlMost = std::max(B.SwirlMost, Bridge);
					}
					else if (Sp.Kind == SplashKind::Feather)
					{
						++B.Feathers;
						B.FeatherBridgeMost = std::max(B.FeatherBridgeMost, Bridge);
					}
				}
				B.LastHeave = Out.Heave;
				B.bSeen = true;
			}
			for (const SeaShow::Fx& E : Night.Effects)
			{
				if (E.Kind == SeaShow::FxKind::ChargeDrop)
				{
					++Drops;
					DropIn In;
					In.Kind = E.What;
					In.N = static_cast<int>(std::lround(E.Size));
					In.Spread = E.Spread;
					In.Sinking = E.Sinking;
					In.Until = E.Until;
					In.At = P3(E.At.X, E.At.Y, E.At.Z);
					In.Aim = P3(E.Aim.X, E.Aim.Y, 0.0);
					In.Vel = P3(E.Vel.X, E.Vel.Y, 0.0);
					if (const SeaShow::ShipOut* Escort = Night.Ship(E.Key))
					{
						In.Beam = Escort->Beam;
						In.Long = Escort->Long;
					}
					for (const DropShot& Shot : DropPlan(In, static_cast<uint64_t>(E.Key) * 7919ull + static_cast<uint64_t>(T * 10.0)))
					{
						Landing L;
						L.Key = E.Key;
						L.Lands = T + Shot.Dt + Shot.Flight;
						L.To = Shot.To;
						L.bAhead = Shot.Side == 2;
						L.Middle = In.Aim;
						L.Sinking = In.Sinking;
						L.Ring = In.Spread > 0.0 ? In.Spread * 0.5 : 20.0;
						Landings.push_back(L);
					}
				}
				else if (E.Kind == SeaShow::FxKind::ChargeBurst)
				{
					const bool bAhead = E.What == "ahead";
					double Nearest = 1e9;
					const Landing* Near = nullptr;
					for (const Landing& L : Landings)
					{
						if (L.Key != E.Key || L.bAhead != bAhead || L.Lands > T + 0.5) continue;
						const double D = std::hypot(L.To.X - E.At.X, L.To.Y - E.At.Y);
						if (D < Nearest)
						{
							Nearest = D;
							Near = &L;
						}
					}
					ShowCheck(Near != nullptr, Said("a burst at %.1f s (escort %.0f) has a charge drawn going in before it", T, E.Key));
					if (!Near) continue;
					if (bAhead)
					{
						// (the ahead thrower's bombs burst only on a hit, the show placing them at the boat where she is then, wherever the
						// salvo was aimed: the ring is drawn where the show aimed it, so how far off it is the show's, told, not checked)
						++AheadBursts;
						AheadOff = std::max(AheadOff, std::hypot(E.At.X - Near->Middle.X, E.At.Y - Near->Middle.Y) - Near->Ring);
						ShowCheck(T > Near->Lands, Said("an ahead bomb's burst at %.1f s comes after its ring is in (%.1f s after)", T, T - Near->Lands));
					}
					else if (E.Size < 0.5)
					{
						// (a booked burst, Size 1, the show places at the boat it kills or hurts: not on the pattern's line)
						++PatternBursts;
						WorstMiss = std::max(WorstMiss, Nearest);
						LeastLead = std::min(LeastLead, T - Near->Lands);
						ShowCheck(Nearest <= 9.0, Said("a pattern's burst at %.1f s lies on a charge drawn going in (%.1f m off)", T, Nearest));
						ShowCheck(T - Near->Lands >= 0.5 * Near->Sinking && T - Near->Lands <= Near->Sinking + 3.0,
						          Said("a pattern's burst at %.1f s comes as its charge has sunk (%.1f s after it went in, sinking %.1f)", T, T - Near->Lands, Near->Sinking));
					}
				}
				else if (E.Kind == SeaShow::FxKind::Starshell)
				{
					++Stars;
					StarIn In;
					In.Burst = P3(E.At.X, E.At.Y, E.At.Z);
					In.T0 = T;
					In.Until = E.Until > T ? E.Until : T + StarBurnUsual();
					In.Wind = P3(Night.Wind().X, Night.Wind().Y, 0.0);
					const StarOut Lit = StarAt(In, T + 1.0);
					const StarOut Late = StarAt(In, In.Until - 0.5);
					const StarOut Gone = StarAt(In, In.Until + 10.0);
					ShowCheck(Lit.bBurning && Lit.Light > 0.9, Said("a starshell at %.1f s lights (%.2f)", T, Lit.Light));
					ShowCheck(Late.Flare.Z < Lit.Flare.Z - 0.5 * (In.Until - T) && Late.Flare.Z > 40.0, Said("a starshell at %.1f s sinks as it burns, still high at the end (%.0f m to %.0f m)", T, Lit.Flare.Z, Late.Flare.Z));
					ShowCheck(!Gone.bShown && Gone.Light == 0.0, Said("a starshell at %.1f s is gone after it burns out", T));
					ShowCheck(LitBy(Lit, P3(E.At.X, E.At.Y, 0.0)) > 0.2, Said("a starshell at %.1f s lights the sea under it (%.2f)", T, LitBy(Lit, P3(E.At.X, E.At.Y, 0.0))));
				}
			}
			Night.Effects.clear();
			if (T >= EndT - 1e-9) break;
			T = std::min(EndT, T + Step);
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
		}

		ShowCheck(!Boats.empty(), "the night has boats");
		for (const auto& [Key, B] : Boats)
		{
			ShowCheck(B.WorstPop < 0.25, Said("boat %.0f never pops where she can be seen (worst %.3f m in a frame, at %.1f s)", Key, B.WorstPop, B.PopAt));
			ShowCheck(B.WorstOff < 0.3, Said("boat %.0f surfaced rides where the show has her, her casing awash (worst %.2f m off, at %.1f s)", Key, B.WorstOff, B.OffAt));
			ShowCheck(B.UpFrames > 0 && B.UpShown == B.UpFrames && B.UpLeast > 0.5,
			          Said("boat %.0f at periscope depth with the show's periscope up: the stage's model shows its taller periscope's head in every frame (%.0f), at least %.2f m", Key, B.UpFrames, B.UpLeast));
			ShowCheck(B.UpFrames == 0 || (B.UpLowMost < 0.0 && B.UpBridgeMost < -1.0),
			          Said("boat %.0f, the periscope up: her shorter periscope (at most %.2f m) and her bridge (at most %.2f m) under", Key, B.UpLowMost, B.UpBridgeMost));
			ShowCheck(B.DownFrames == 0 || B.DownMost < -0.3, Said("boat %.0f at periscope depth with the show's periscope down: nothing of the stage's model out (its highest point at most %.2f m)", Key, B.DownMost));
			ShowCheck(B.DeepFrames == 0 || B.DeepMost < -3.0, Said("boat %.0f running deep: nothing of the stage's model near the surface (at most %.2f m)", Key, B.DeepMost));
			ShowCheck(B.Swirls == 0 || (B.SwirlLeast > -0.2 && B.SwirlMost <= 0.0),
			          Said("boat %.0f: each swirl comes as the last of the stage's model's bridge goes under (%.2f to %.2f m)", Key, B.SwirlLeast, B.SwirlMost));
			ShowCheck(B.FeatherBridgeMost < 0.0, Said("boat %.0f: feathers (%.0f) only while all of the stage's model's bridge is under (at most %.2f m)", Key, B.Feathers, B.FeatherBridgeMost));
		}
		ShowCheck(PatternBursts > 0 && AheadBursts > 0 && Stars > 0, Said("the night has pattern bursts (%.0f), ahead bursts (%.0f) and a starshell (%.0f)", PatternBursts, AheadBursts, Stars));
		std::printf("%s: %zu boats, %d drops, %d pattern bursts (worst %.1f m off a charge drawn going in, the least %.1f s after it), %d ahead bursts (at most %.1f m outside the ring the show aimed), %d starshells\n",
		            Rec.Key.c_str(), Boats.size(), Drops, PatternBursts, WorstMiss, LeastLead, AheadBursts, AheadOff, Stars);
		for (const auto& [Key, B] : Boats)
		{
			std::printf("  boat %d: worst pop %.3f m (at %.1f s); surfaced, at most %.2f m off the show's trim awash (at %.1f s); the stage's model: periscope up %d frames, its head %.2f m out at"
			            " least, the shorter's at most %.2f, the bridge's %.2f; periscope down %d frames, highest point at most %.2f; deep %d frames, at most %.2f; %d swirls as the bridge went"
			            " under (%.2f to %.2f m), %d feathers (the bridge at most %.2f m)\n",
			            Key, B.WorstPop, B.PopAt, B.WorstOff, B.OffAt, B.UpFrames, B.UpLeast, B.UpLowMost, B.UpBridgeMost, B.DownFrames, B.DownMost, B.DeepFrames, B.DeepMost, B.Swirls,
			            B.SwirlLeast, B.SwirlMost, B.Feathers, B.FeatherBridgeMost);
		}
		return 0;
	}
}

int main(int ArgC, char** ArgV)
{
	int Status = 0;
	for (int I = 1; I < ArgC; ++I)
	{
		Status = std::max(Status, PlayNight(ArgV[I]));
	}
	std::printf("convoy looks against the show: %d checks, %d failed\n", ShowChecks, ShowFailures);
	return ShowFailures > 0 ? 1 : Status;
}
